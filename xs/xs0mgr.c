/*************************************************************************\
**  source       * xs0mgr.c
**  directory    * xs
**  description  * eXternal Sort manager
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

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssfile.h>

#include <su0error.h>
#include <su0list.h>
#include <su0cfgst.h>

#include <rs0sqli.h>

#include "xs1sort.h"
#include "xs1cfg.h"
#include "xs0acnd.h"
#include "xs0error.h"
#include "xs0mgr.h"

#define XS_POOLPERCENT_MAX 50
#define XS_POOLPERCENT_MIN 10


struct xs_mgr_st {
        xs_mem_t* m_memmgr;
        xs_tfmgr_t* m_tfmgr;
        dbe_bufpool_t* m_bufpool;
        dbe_db_t* m_db;
        xs_cfg_t* m_cfg;
        ulong m_sortarraysize;
};

/*##**********************************************************************\
 *
 *		xs_mgr_init
 *
 * Creates an external sort manager
 *
 * Parameters :
 *
 *	db - in, hold
 *		database object
 *
 *	inifile - in, hold
 *		inifile object
 *
 *      sortarraysize - in
 *          SQL internal sort array size
 *
 * Return value - give :
 *      created sort manager or
 *      NULL when no temporary directory is configured in the inifile
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
xs_mgr_t* xs_mgr_init(
        dbe_db_t* db,
        su_inifile_t* inifile,
        long sortarraysize)
{
        xs_mgr_t* xsmgr;
        size_t memblocks_max;
        size_t memblock_size;
        size_t memblock_size_tmp;
        uint poolusepercent;
        bool found;
        bool succp;
        char* tfdirname;
        ulong tfdirsize;
        ulong maxfiles;
        xs_cfg_t* cfg;
        uint i;
        int openflags;
        int writeflushmode;
        bool filebuffering;

        ss_dassert(sortarraysize >= 100);
        cfg = xs_cfg_init(inifile);
        xs_cfg_register_su_params();

        found = xs_cfg_tmpdir(cfg, 1, &tfdirname, &tfdirsize);
        if (!found) {
            xs_cfg_done(cfg);
            return (NULL);
        }

        xs_error_init();
        xsmgr = SSMEM_NEW(xs_mgr_t);
        xsmgr->m_db = db;
        xsmgr->m_cfg = cfg;
        xsmgr->m_bufpool = dbe_db_getbufpool(xsmgr->m_db);
        memblock_size = dbe_db_blocksize(xsmgr->m_db);
        found = xs_cfg_blocksize(xsmgr->m_cfg, &memblock_size_tmp);
        if (!found || memblock_size_tmp == 0
        ||  memblock_size_tmp <= memblock_size)
        {
            memblock_size_tmp = 0;
        } else {
            memblock_size = memblock_size_tmp;
        }
        memblocks_max = dbe_db_poolsize(xsmgr->m_db);
        found = xs_cfg_poolpercenttotal(xsmgr->m_cfg, &poolusepercent);
        if (poolusepercent < XS_POOLPERCENT_MIN
        ||  (memblock_size_tmp == 0 && poolusepercent > XS_POOLPERCENT_MAX))
        {
            ss_dassert(found);
            su_rc_fatal_error(
                XS_FATAL_PARAM_SSUUU,
                SU_XS_SECTION,
                SU_XS_POOLPERCENTTOTAL,
                poolusepercent,
                XS_POOLPERCENT_MIN,
                XS_POOLPERCENT_MAX);
        }
        xs_cfg_getwriteflushmode(cfg, &writeflushmode);
        xs_cfg_getfilebuffering(cfg, &filebuffering);
        openflags = SS_BF_SEQUENTIAL | SS_BF_EXCLUSIVE;
        if (!filebuffering) {
            openflags |= SS_BF_NOBUFFERING;
        }
        switch (writeflushmode) {
            case SS_BFLUSH_NORMAL:
                break;
            case SS_BFLUSH_BEFOREREAD:
                openflags |= SS_BF_FLUSH_BEFOREREAD;
                break;
            case SS_BFLUSH_AFTERWRITE:
                openflags |= SS_BF_FLUSH_AFTERWRITE;
                break;
            default:
                ss_rc_derror(writeflushmode);
                break;
        }
        memblocks_max /= memblock_size;
        memblocks_max *= poolusepercent;
        memblocks_max /= 100;
        if (memblocks_max < 3) {
            memblocks_max = 3;
        }

        if (memblock_size_tmp != 0) {
            xsmgr->m_memmgr =
                xs_mem_init(
                        (ulong)memblocks_max,
                        memblock_size,
                        NULL,
                        (xs_mem_allocfun_t*)NULL,
                        (xs_mem_freefun_t*)NULL,
                        (xs_hmem_getbuffun_t*)NULL);
        } else {
            xsmgr->m_memmgr =
                xs_mem_init(
                        (ulong)memblocks_max,
                        memblock_size,
                        xsmgr->m_bufpool,
                        (xs_mem_allocfun_t*)dbe_bufpool_alloc,
                        (xs_mem_freefun_t*)dbe_bufpool_free,
                        (xs_hmem_getbuffun_t*)dbe_hbuf_getbuf);
        }
        found = xs_cfg_maxfilestotal(xsmgr->m_cfg, &maxfiles);
        xsmgr->m_tfmgr =
            xs_tfmgr_init(
                maxfiles,
                xsmgr->m_memmgr,
                (ulong)dbe_db_getcreatime(xsmgr->m_db),
                openflags);
        i = 1;
        do {
            succp = xs_tfmgr_adddir(xsmgr->m_tfmgr, tfdirname, tfdirsize);
            if (!succp) {
                su_rc_fatal_error(XS_FATAL_DIR_S, tfdirname);
            }
            i++;
            SsMemFree(tfdirname);
            found = xs_cfg_tmpdir(cfg, i, &tfdirname, &tfdirsize);
        } while (found);
        xsmgr->m_sortarraysize = (ulong)sortarraysize;
        return (xsmgr);
}

/*##**********************************************************************\
 *
 *		xs_mgr_done
 *
 * Deletes a external sort manager object
 *
 * Parameters :
 *
 *	xsmgr - in, take
 *		sort manager
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void xs_mgr_done(xs_mgr_t* xsmgr)
{
        if (xsmgr == NULL) {
            return;
        }
        ss_dassert(xsmgr->m_memmgr != NULL);
        ss_dassert(xsmgr->m_tfmgr != NULL);
        ss_dassert(xsmgr->m_cfg != NULL);
        ss_dassert(xsmgr->m_db != NULL);
        ss_dassert(xsmgr->m_bufpool != NULL);

        xs_tfmgr_done(xsmgr->m_tfmgr);
        xs_mem_done(xsmgr->m_memmgr);
        xs_cfg_done(xsmgr->m_cfg);
        dbe_db_releasebufpool(xsmgr->m_db, xsmgr->m_bufpool);
        SsMemFree(xsmgr);
}

/*##**********************************************************************\
 *
 *		xs_mgr_sortinit
 *
 * Creates a sorter object
 *
 * Parameters :
 *
 *	xsmgr - in out, use
 *		external sorter manager
 *
 *	ttype - in, hold
 *		tuple type
 *
 *	lines - in
 *		estimated # of lines to be sorted
 *
 *	exact - in
 *		TRUE if lines is exact, FALSE otherwise
 *
 *	order_c - in
 *		# of order by conditions
 *
 *	order_cols - in, use
 *		array of column #'s in order by
 *
 *	descarr - in, use
 *		array of boolean flags - TRUE = desc, FALSE = asc
 *
 *	cd - in, hold
 *		client data
 *
 *	comp_fp - in, hold
 *		Compare function
 *
 *      sql - in
 *          TRUE if sorter is initiated by SQL interpreter (uses SQL attribute
 *          numbering) or
 *          FALSE if used directly from DBE (physical attribute numbers)
 *
 *  testonly - in
 *      if 1, a sorter is not created but a non-NULL
 *      dummy pointer is returned if external sorter
 *      would be used
 *
 * Return value - give:
 *      created sorter object or NULL when not enough
 *      resources were available
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
xs_sorter_t* xs_mgr_sortinit(
        xs_mgr_t* xsmgr,
        rs_ttype_t* ttype,
        ulong lines,
        bool exact,
        uint order_c,
        uint order_cols[/*order_c*/],
        bool descarr[/*order_c*/],
        void* cd,
        xs_qcomparefp_t comp_fp,
        bool sql,
        bool testonly)
{
        xs_sorter_t* sorter;
        uint i;
        su_list_t* orderby_list;
        xs_acond_t* acond;
        bool found;
        ulong memper1sort;
        size_t bufsize;
        uint maxfilesper1sort;
        ulong stepsizebytes;
        uint stepsizerows;

        ss_bprintf_1((
            "xs_mgr_sortinit: Estimated lines %lu (exact = %d) sortarraysize = %lu\n",
                lines, exact, (ulong)xsmgr->m_sortarraysize));
        ss_bprintf_2(("xs_mgr_sortinit: creating sorter\n"));
        if (testonly) {
            ss_bprintf_2(("xs_mgr_sortinit: testonly, return dummy pointer\n"));
            ss_dassert(sql);
            return((xs_sorter_t*)1);
        }

        found = xs_cfg_poolsizeper1sort(xsmgr->m_cfg, &memper1sort);
        bufsize = xs_mem_getblocksize(xsmgr->m_memmgr);
        found = xs_cfg_maxfilesper1sort(xsmgr->m_cfg, &maxfilesper1sort);
        found = xs_cfg_maxbytesperstep(xsmgr->m_cfg, &stepsizebytes);
        found = xs_cfg_maxrowsperstep(xsmgr->m_cfg, &stepsizerows);

        if (order_c != 0) {
            orderby_list = su_list_init((void(*)(void*))xs_acond_done);
            for (i = 0; i < order_c; i++) {
                uint ano;

                if (sql) {
                    ano = rs_ttype_sqlanotophys(cd, ttype, order_cols[i]);
                } else {
                    ano = order_cols[i];
                }
                acond = xs_acond_init(!descarr[i], ano);
                su_list_insertlast(orderby_list, acond);
            }
        } else {
            orderby_list = NULL;
        }

        if (memper1sort < bufsize * 3L) {
            memper1sort = bufsize * 3L;
        }
        sorter = xs_sorter_init(
                    cd,
                    xsmgr->m_memmgr,
                    xsmgr->m_tfmgr,
                    ttype,
                    orderby_list,
                    (uint)(memper1sort/bufsize),
                    maxfilesper1sort,
                    stepsizebytes,
                    stepsizerows,
                    comp_fp);
        return (sorter);
}

void xs_mgr_addcfgtocfgl(
        xs_mgr_t* xsmgr,
        su_cfgl_t* cfgl)
{
        xs_cfg_addtocfgl(xsmgr->m_cfg, cfgl);
}
