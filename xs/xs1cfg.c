/*************************************************************************\
**  source       * xs1cfg.c
**  directory    * xs
**  description  * Configuration object for eXternal Sorter
**               * 
**               * Copyright (C) 2006 Solid Information Technology Ltd
\*************************************************************************/
/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; only under version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA
*/


#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstddef.h>
#include <ssstring.h>
#include <sslimits.h>
#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssprint.h>
#include <ssfile.h>

#include <su0param.h>
#include <su0cfgst.h>
#include "xs1cfg.h"

#define XS_DEFPOOLPERCENTTOTAL  25
#define XS_DEFMAXFILESPER1SORT  7
#define XS_DEFMAXFILESTOTAL     200
#define XS_DEFPOOLSIZEPER1SORT  (XS_DEFMAXFILESPER1SORT * 8192UL)
#define XS_DEFMAXBYTESPERSTEP   8192L
#define XS_DEFMAXROWSPERSTEP    30
#define XS_DEFBLOCKSIZE         0 /* 0 means same block size as in db file */
#define XS_DEFFILEBUFFERING     TRUE
#define XS_DEFSORTERENABLED     TRUE
#define XS_DEFTMPDIR            "."

#define XS_DEFWRITEFLUSHMODE SS_BFLUSH_NORMAL

struct xs_cfg_st {
        su_inifile_t* xc_inifile;
        uint          xc_maxdnum;
};

static su_ret_t conf_param_rwstartup_set_cb(
        char* default_value,
        char** default_value_loc,
        char* current_value,
        char** current_value_loc,
        char** factory_value_loc
);

static su_initparam_t xs_parameters[] =
{
/* Sorter Section */
{
SU_XS_SECTION, SU_XS_TMPDIR_1, XS_DEFTMPDIR, 0, 0.0, 0,
conf_param_rwstartup_set_cb, NULL, SU_PARAM_TYPE_STR, SU_PARAM_AM_RWSTARTUP,
"Sorter temporary directory"
},
{
SU_XS_SECTION, SU_XS_POOLPERCENTTOTAL, NULL, XS_DEFPOOLPERCENTTOTAL, 0.0, 0,
conf_param_rwstartup_set_cb, NULL, SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
"Maximum percentage of cache pages used for sorting (10% - 50%)"
},
{
SU_XS_SECTION, SU_XS_POOLSIZEPER1SORT, NULL, XS_DEFPOOLSIZEPER1SORT, 0.0, 0,
conf_param_rwstartup_set_cb, NULL, SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
"Maximum memory available in bytes for one sort"
},
{
SU_XS_SECTION, SU_XS_MAXFILESTOTAL, NULL, XS_DEFMAXFILESTOTAL, 0.0, 0,
conf_param_rwstartup_set_cb, NULL, SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
"Maximum number of files used for sorting"
},
{
SU_XS_SECTION, SU_XS_BLOCKSIZE, NULL, XS_DEFBLOCKSIZE, 0.0, 0,
conf_param_rwstartup_set_cb, NULL, SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
"Block size for external sorter (0 = same as [Indexfile]Blocksize"
},
{
SU_XS_SECTION, SU_XS_SORTERENABLED, NULL, 0, 0.0, XS_DEFSORTERENABLED,
conf_param_rwstartup_set_cb, NULL, SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
"Enables external sorter"
},
{
        NULL, NULL, NULL, 0, 0.0, 0, NULL, NULL, 0, 0, NULL
}
};

/*##**********************************************************************\
 * 
 *		xs_cfg_init
 * 
 * Creates a cfg object for external sorter
 * 
 * Parameters : 
 * 
 *	inifile - in, hold
 *		inifile object
 *		
 * Return value - give :
 *      created cfg object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_cfg_t* xs_cfg_init(
        su_inifile_t* inifile)
{
        xs_cfg_t* cfg;

        ss_dassert(inifile != NULL);
        cfg = SSMEM_NEW(xs_cfg_t);
        cfg->xc_maxdnum = 1;

        cfg->xc_inifile = inifile;
        su_inifile_link(cfg->xc_inifile);
        return (cfg);
}

/*##**********************************************************************\
 * 
 *		xs_cfg_done
 * 
 * Deletes a cfg object
 * 
 * Parameters : 
 * 
 *	cfg - in, take
 *		cfg object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_cfg_done(
        xs_cfg_t* cfg)
{
        ss_dassert(cfg != NULL);
        su_inifile_done(cfg->xc_inifile);
        SsMemFree(cfg);
}

/*##**********************************************************************\
 * 
 *		xs_cfg_poolpercenttotal
 * 
 * Gets total percentage of buffer pool memory that can be used
 * by the sorts
 * 
 * Parameters : 
 * 
 *	cfg - in, use
 *		cfg object
 *		
 *	p_percent - out
 *		pointer to unsigned integer where the percentage will
 *          be stored
 *		
 * Return value :
 *      TRUE when configuration was found
 *      FALSE when default was used
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_poolpercenttotal(
        xs_cfg_t* cfg,
        uint* p_percent)
{
        long l;
        bool found;

        ss_dassert(p_percent != NULL);
        ss_dassert(cfg != NULL);

        found = su_param_getlong(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_POOLPERCENTTOTAL,
                    &l);

        if (!found) {
            l = XS_DEFPOOLPERCENTTOTAL;
        }
        *p_percent = (uint)l;
        return (found);       
}

/*##**********************************************************************\
 * 
 *		xs_cfg_poolsizeper1sort
 * 
 * Gets memory pool size for 1 sort
 * 
 * Parameters : 
 * 
 *	cfg - in, use
 *		config object
 *		
 *	p_size - out
 *		pointer to variable where the max. size will be stored
 *		
 * Return value :
 *      TRUE when configuration was found
 *      FALSE when default was used
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_poolsizeper1sort(
        xs_cfg_t* cfg,
        ulong* p_size)
{
        long l;
        bool found;
        uint maxfiles;
        size_t blocksize;
        ulong minimum;

        ss_dassert(p_size != NULL);
        ss_dassert(cfg != NULL);

        found = su_param_getlong(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_POOLSIZEPER1SORT,
                    &l);
        xs_cfg_maxfilesper1sort(cfg, &maxfiles);
        xs_cfg_blocksize(cfg, &blocksize);
        minimum = (ulong)blocksize * maxfiles;
        if (!found || l < (long)minimum) {
            l = (long)minimum;
        }
        *p_size = (ulong)l;
        return (found);       
}

/*##**********************************************************************\
 * 
 *		xs_cfg_maxfilesper1sort
 * 
 * Gets max
 * 
 * Parameters : 
 * 
 *	cfg - 
 *		
 *		
 *	p_maxfiles - 
 *		
 *		
 * Return value : 
 *      TRUE when configuration was found
 *      FALSE when default was used
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_maxfilesper1sort(
        xs_cfg_t* cfg,
        uint* p_maxfiles)
{
        long l;
        bool found;

        ss_dassert(p_maxfiles != NULL);
        ss_dassert(cfg != NULL);

        found = su_inifile_getlong(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_MAXFILESPER1SORT,
                    &l);
        if (!found) {
            l = XS_DEFMAXFILESPER1SORT;
        }
        *p_maxfiles = (uint)l;
        return (found);       
}

/*##**********************************************************************\
 * 
 *		xs_cfg_maxfilestotal
 * 
 * Gets max # of files to be used by sorts
 * 
 * Parameters : 
 * 
 *	cfg - in, use
 *		cfg object
 *		
 *	p_maxfiles -
 *          pointer to variable where max files will be stored
 *		
 *		
 * Return value : 
 *      TRUE when configuration was found
 *      FALSE when default was used
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_maxfilestotal(
        xs_cfg_t* cfg,
        ulong* p_maxfiles)
{
        long l;
        bool found;

        ss_dassert(p_maxfiles != NULL);
        ss_dassert(cfg != NULL);

        found = su_param_getlong(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_MAXFILESTOTAL,
                    &l);

        if (!found) {
            l = XS_DEFMAXFILESTOTAL;
        }
        *p_maxfiles = (ulong)l;
        return (found);       
}

/*##**********************************************************************\
 * 
 *		xs_cfg_blocksize
 * 
 * Gets blocksize for external sorter
 * 
 * Parameters : 
 * 
 *	cfg - in, use
 *		cfg object
 *		
 *	p_blocksize -
 *          pointer to variable where blocksize will be stored
 *		
 *		
 * Return value : 
 *      TRUE when configuration was found
 *      FALSE when default was used
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_blocksize(
        xs_cfg_t* cfg,
        size_t* p_blocksize)
{
        long l;
        bool found;

        ss_dassert(p_blocksize != NULL);
        ss_dassert(cfg != NULL);

        found = su_param_getlong(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_BLOCKSIZE,
                    &l);

        if (!found) {
            l = XS_DEFMAXFILESTOTAL;
        }
        *p_blocksize = (size_t)l;
        return (found);       
}

/*##**********************************************************************\
 * 
 *		xs_cfg_tmpdir
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfg - 
 *		
 *		
 *	dnum - 
 *		
 *		
 *	p_dname_give - 
 *		
 *		
 *	p_maxblocks - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_tmpdir(
        xs_cfg_t* cfg,
        uint dnum,
        char** p_dname_give,
        ulong* p_maxblocks)
{
        char* dirname;
        char keybuf[24];
        bool found;
        bool enabled;
        uint scanindex;
        ulong l;

        ss_dassert(p_dname_give != NULL);
        ss_dassert(p_maxblocks != NULL);
        ss_dassert(dnum > 0);
        ss_dassert(cfg != NULL);

        ss_dassert(sizeof(keybuf) >= strlen(SU_XS_TMPDIR) + 10);

        found = su_inifile_getbool(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_SORTERENABLED,
                    &enabled);
        if (found && !enabled) {
            *p_dname_give = NULL;
            *p_maxblocks = 0L;
            return(FALSE);
        }

        SsSprintf(keybuf, SU_XS_TMPDIR, dnum);
        scanindex = 0;
        found = su_inifile_scanstring(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    keybuf,
                    " \t,",
                    &scanindex,
                    &dirname);
        if (found) {
            found = su_inifile_scanlong(
                        cfg->xc_inifile,
                        SU_XS_SECTION,
                        keybuf,
                        " \t,",
                        &scanindex,
                        (long *)&l);
            cfg->xc_maxdnum = SS_MAX(cfg->xc_maxdnum, dnum);
        } else if (dnum == 1 && !ssfile_diskless) {
            dirname = SsMemStrdup((char *)XS_DEFTMPDIR);
            cfg->xc_maxdnum = SS_MAX(cfg->xc_maxdnum, dnum);
        } else {
            dirname = NULL;
        }
        if (dirname != NULL && !found) {
            l = ULONG_MAX;
            found = TRUE;
        }
        if (!found) {
            *p_dname_give = NULL;
            *p_maxblocks = 0L;
            return (FALSE);
        }
        *p_dname_give = dirname;
        *p_maxblocks = l;
        return (TRUE);       
}

/*##**********************************************************************\
 * 
 *		xs_cfg_maxbytesperstep
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfg - 
 *		
 *		
 *	p_maxbytes - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_maxbytesperstep(
        xs_cfg_t* cfg,
        ulong* p_maxbytes)
{
        long l;
        bool found;

        ss_dassert(p_maxbytes != NULL);
        ss_dassert(cfg != NULL);

        found = su_inifile_getlong(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_MAXBYTESPERSTEP,
                    &l);
        if (!found) {
            l = XS_DEFMAXBYTESPERSTEP;
        }
        *p_maxbytes = (ulong)l;
        return (found);       
}

/*##**********************************************************************\
 * 
 *		xs_cfg_maxrowsperstep
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfg - 
 *		
 *		
 *	p_maxrows - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_maxrowsperstep(
        xs_cfg_t* cfg,
        uint* p_maxrows)
{
        long l;
        bool found;

        ss_dassert(p_maxrows != NULL);
        ss_dassert(cfg != NULL);

        found = su_inifile_getlong(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_MAXROWSPERSTEP,
                    &l);
        if (!found) {
            l = XS_DEFMAXROWSPERSTEP;
        }
        *p_maxrows = (uint)l;
        return (found);       
}

bool xs_cfg_getwriteflushmode(
        xs_cfg_t* cfg,
        int* p_writeflushmode)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    cfg->xc_inifile,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_WRITEFLUSHMODE,
                    &l);
        if (found) {
            switch ((int)l) {
                case SS_BFLUSH_NORMAL:
                case SS_BFLUSH_BEFOREREAD:
                case SS_BFLUSH_AFTERWRITE:
                    break;
                default:
                    found = FALSE;
                    break;
            }
        }
        if (!found) {
            *p_writeflushmode = XS_DEFWRITEFLUSHMODE;
        } else {
            *p_writeflushmode = (int)l;
        }
        return (found);
}

/*##**********************************************************************\
 * 
 *		xs_cfg_getfilebuffering
 * 
 * Gets the parameter value for filebuffering flag. If filebuffering is set
 * to FALSE (No) sorter files are opened in no buffering mode. File buffering
 * is good for performance in general case but might not be useful sometimes
 * if operating system starts swapping because of file I/O.
 * 
 * Parameters : 
 * 
 *	cfg - 
 *		
 *		
 *	p_filebuffering - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_cfg_getfilebuffering(
        xs_cfg_t* cfg,
        bool* p_filebuffering)
{
        bool found;

        found = su_inifile_getbool(
                    cfg->xc_inifile,
                    SU_XS_SECTION,
                    SU_XS_FILEBUFFERING,
                    p_filebuffering);
        if (!found) {
            *p_filebuffering = XS_DEFFILEBUFFERING;
        }
        return (found);
}

void xs_cfg_addtocfgl(
        xs_cfg_t* xs_cfg,
        su_cfgl_t* cfgl)
{
        uint i;

        char tmpdir_buf[12];
        su_cfgl_addlong(
            cfgl,
            xs_cfg->xc_inifile,
            SU_XS_SECTION,
            SU_XS_POOLPERCENTTOTAL,
            (long)XS_DEFPOOLPERCENTTOTAL,
            SU_CFGL_ISADVANCED);
        su_cfgl_addlong(
            cfgl,
            xs_cfg->xc_inifile,
            SU_XS_SECTION,
            SU_XS_POOLSIZEPER1SORT,
            (long)XS_DEFPOOLSIZEPER1SORT,
            SU_CFGL_ISADVANCED);
        su_cfgl_addlong(
            cfgl,
            xs_cfg->xc_inifile,
            SU_XS_SECTION,
            SU_XS_MAXFILESPER1SORT,
            (long)XS_DEFMAXFILESPER1SORT,
            SU_CFGL_ISADVANCED);
        su_cfgl_addlong(
            cfgl,
            xs_cfg->xc_inifile,
            SU_XS_SECTION,
            SU_XS_MAXFILESTOTAL,
            (long)XS_DEFMAXFILESTOTAL,
            SU_CFGL_ISADVANCED);
        for (i = 1; i <= xs_cfg->xc_maxdnum; i++) {
            SsSprintf(tmpdir_buf, SU_XS_TMPDIR, i);
            su_cfgl_addstr(
                cfgl,
                xs_cfg->xc_inifile,
                SU_XS_SECTION,
                tmpdir_buf,
                "",
                (i > 1) ? SU_CFGL_ISADVANCED : 0);
        }
}

static su_ret_t conf_param_rwstartup_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value __attribute__ ((unused)),
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}
void xs_cfg_register_su_params(void)
{
        bool b;

        b = su_param_register_array(xs_parameters);
        ss_dassert(b);
}
