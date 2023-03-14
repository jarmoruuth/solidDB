/*************************************************************************\
**  source       * su0inifi.c
**  directory    * su
**  description  * Windows .INI format configuration file utility
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

The inifile is read into memory to achieve fast access.
The searches use binary tree search, which further increases
performance. The inifile format is basically:

[section name #1]
keyname1=value
keyname2=another value

; Comment line is indicated using semicolon

[section name #2]
keyname1=value
...

The key names need not be globally unique, but must be unique
under certain section name
the key name may consist of all printable characters
except '[', ']', '=', \n all leading and trailing spaces from key names
and in section names lines outside [] are ignored.
The key name and section name comparison is case insensitive, but it only
knows the English letters (a-z and A-Z).

Limitations:
-----------

The entire file must fit into memory

Error handling:
--------------

Illegal lines are treated as comment, ie. ignored.

Objects used:
------------

lists           "su0list.h"
rb-trees        "su0rbtr.h"
virtual files   "su0vfil.h"

Preconditions:
-------------

Before calling the su_inifile_save* the su_vfh_globalinit(n) must be done.

Multithread considerations:
--------------------------

All interface functions are auto-mutexed.


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <sswindow.h>
#include <ssctype.h>
#include <ssstring.h>
#include <ssstdio.h>
#include <sschcvt.h>

#include <sssprint.h>
#include <ssdtoa.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <sslimits.h>
#include <ssfnsplt.h>
#include <ssfnsrch.h>
#include <ssgetenv.h>

#include "su1check.h"
#include "su0cfgst.h"
#include <ssscan.h>
#include "su0list.h"
#include "su0rbtr.h"
#include "su0vfil.h"
#include "su0inifi.h"
#include "su1reg.h"
#include "su0regis.h"
    
#define SAVE_MAXLINELEN     79      /* Max line length written as one line. */
#define READ_MAXLINELEN     1024    /* Max line length read as one line. */

struct su_inifile_st {
        char*       if_fname;
        su_list_t*  if_linelist;
        su_rbt_t*   if_sections;
        SsSemT*     if_mutex;
        bool        if_changed;
        bool        if_ordersections;
        int         if_nlink;
        bool        if_registry;    /* TRUE, if filled from registry */
        bool        if_filefound;   /* TRUE, if file was found */
        su_inifile_regkey_t if_regrootkey;
        ss_debug(su_check_t if_check;)
};

#define IF_CHECK(inifile) \
        ss_dassert(SS_CHKPTR(inifile) && inifile->if_check == SUCHK_INIFILE)

typedef struct {
        su_list_node_t* ifs_lnode;
        su_rbt_t*       ifs_keylines;
} su_ifsection_t;

typedef struct {
        su_list_node_t* ifkl_lnode;
        uchar*          ifkl_keystart;
        uchar*          ifkl_keyend;
} su_ifkeyline_t;

typedef enum {
        SU_IFLINE_UNKNOWN,
        SU_IFLINE_SECTION,
        SU_IFLINE_KEYLINE,
        SU_IFLINE_COMMENT,
        SU_IFLINE_ILLEGAL
} su_iflinetype_t;

enum su_inifile_sreg_depr_enum {
        SU_INIFILE_SREG_DEPR_NOT,
        SU_INIFILE_SREG_DEPR_REPLACED,
        SU_INIFILE_SREG_DEPR_DISCONTINUED,
        SU_INIFILE_SREG_DEPR_DISCONTINUED_REPLACED
};
typedef enum su_inifile_sreg_depr_enum su_inifile_sreg_depr_t;
        
static char default_separators[] = "\t ,";

static bool su_inifile_saveas_nomutex(
        su_inifile_t* inifile,
        char* fname);
static bool su_inifile_getvalue_nomutex(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_ref);
static char* su_inifile_getkeyline(
        su_inifile_t* inifile,
        const char* section_name,
        const char* keyname);
static char *su_inifile_keylinevalue(char* keyline);
static char* su_inifile_skipseparators(char *p, const char* separators);
static su_ifsection_t* su_ifsection_init(su_list_node_t* lnode);
static void su_ifsection_done(su_ifsection_t* ifsection);
static void su_ifline_done(void* lineptr);
static int su_ifsection_cmp(void* key1, void* key2);
static int su_ifsection_cmp2(void* key, void* datum);
static su_ifkeyline_t* su_ifkeyline_init(su_list_node_t* lnode);
static void su_ifkeyline_done(su_ifkeyline_t* keyline);
static int su_ifkeyline_cmp(void* key1, void* key2);
static int su_ifkeyline_cmp2(void* key, void* datum);
static su_iflinetype_t su_ifline_type(char *line);
static su_iflinetype_t su_ifline_type_strict(char *line);

static char* su_inifile_keyname(char* keyline);

static bool inifile_fillfromfile(
            su_inifile_t* inifi,
            char* filename,
            bool searchp);

static bool inifile_fillfrommemory(
            su_inifile_t* inifi,
            char* filename,
            char* inibuffer);

#ifndef SS_MYSQL
static bool inifile_fillfromregistry(
            su_inifile_t*       inifile,
            char*               regname,
            su_inifile_regkey_t regrootkey);
#endif /* !SS_MYSQL */

static su_inifile_t* inifile_init(void);

static char* su_inifile_parse_section(char* line);
static bool su_inifile_simplereg_isregistered(su_inifile_t* inifile,
                                              char* section, char* key);
static bool su_inifile_simplereg_isdeprecated(char* section, char* key,
                                              su_inifile_sreg_depr_t* dtype);
static char* su_inifile_simplereg_getofficial(char* section, char* key);

char su_inifile_filename[255] = "solid.ini";

char *diskless_inifile = NULL; /* for diskless, in memory solid.ini */

bool su_inifile_serverside = FALSE;
static bool su_inifile_simplereg_compatibilitymode = FALSE;
static bool su_inifile_simplereg_disablewarnings   = FALSE;

/* print function for log output */
static void (*su_inifile_simplereg_printbuf_fp)(char* buf) = NULL;

void su_inifile_simplereg_setcompatibilitymode(bool mode)
{
        su_inifile_simplereg_compatibilitymode = mode;
}

/* we might want to set warnings on only on server side */
bool su_inifile_simplereg_warningsdisabled(bool val)
{
        bool old;
        
        old = su_inifile_simplereg_disablewarnings;
        su_inifile_simplereg_disablewarnings = val;

        return old;
}

/*#***********************************************************************\
 *
 *      su_inifile_parse_section
 *
 * Parses the section name from a (inifile) line, removing []'s.
 *
 * Parameters:
 *      line - in, use
 *          Line to be parsed.
 *
 * Return value - give
 *      Parsed output.
 *
 * Limitations:
 *
 * Globals used:
 */
static char* su_inifile_parse_section(char* line)
{
        char* str, *t;
        char* s;
        char* e;

        for (s = line; ; s++) { /* skip preceding whitespace */
            if (*s == ' ' || *s == '\t') {
            } else if (*s == '[') {
                s++;
                break;
            } else {
                return NULL;
            }
        }
        for (e = s; ; e++) { /* find end of section name */
            if (*e == ']') {
                break;
            } else if (*e == ';' || *e == '\0' || *e == '\n') {
                return NULL;
            }
        }

        str = t = SsMemAlloc((e-s)+1);        
        for ( ; s < e; s++, t++) { /* copy */
            *t = *s;
        }
        *t = '\0';

        return str;
}

/*##**********************************************************************\
 *
 *      su_inifile_simplereg_init_and_check
 *
 * Sets print function to be used for printing warnings and checks inifile.
 *
 * Parameters:
 *      sse_printbuf_fp - in, hold
 *          printing function.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void su_inifile_simplereg_init_and_check(void (*sse_printbuf_fp)(char* buf))
{
        su_inifile_simplereg_setprintbuf(sse_printbuf_fp);
        su_inifile_simplereg_checkforunregistered(su_inifile_filename, TRUE);

        su_inifile_serverside = TRUE; /* makes code assert on unreg param use
                                       * on serverside, not in clients */
}

/*#***********************************************************************\
 *
 *      su_inifile_simplereg_printbuf
 *
 * Print using an imported function.
 *
 * Parameters:
 *      buf - in, use
 *         string to print.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void su_inifile_simplereg_printbuf(char* buf)
{
#if 0 /* tests won't run otherwise */
        ss_debug(if (su_inifile_simplereg_printbuf_fp == NULL) {
            SsPrintf("Trying to print: %s\n", buf);
            ss_info_assert(0, buf);
        });
#endif
        /* If you end up asserting here, just register the parameter
         * you were accessing. */
        if (su_inifile_simplereg_printbuf_fp != NULL) {
            (*su_inifile_simplereg_printbuf_fp)(buf);
        } else {
            SsPrintf(buf);
        }        
}

/*##**********************************************************************\
 *
 *      su_inifile_simplereg_setprintbuf
 *
 * Set print function to be used by simplereg warnings.
 *
 * Parameters:
 *      sse_printbuf_fp - in, hold
 *          print function.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void su_inifile_simplereg_setprintbuf(void (*sse_printbuf_fp)(char* buf))
{
        su_inifile_simplereg_printbuf_fp = sse_printbuf_fp;
}

/*##**********************************************************************\
 *
 *      su_inifile_simplereg_checkforunregistered
 *
 * Checks inifile for unregistered entries (reg list in su0regis.h).
 * Also warns about deprecated parameters.
 *
 * Parameters:
 *      inifilename - in, use
 *          filename of file to search
 *
 *      print - in
 *          if unregistered entries are to be printed out.
 *
 * Return value:
 *      TRUE, if all is ok.
 *      FALSE, if one or more unregistered entries were found.
 *
 * Limitations:
 *
 * Globals used:
 */
bool su_inifile_simplereg_checkforunregistered(char* inifilename,
                                               bool print)
{
        su_inifile_t* inifile;
        su_list_node_t* list_node;
        char* line, *section, *key;
        bool reg, depr;
        su_inifile_sreg_depr_t depr_type;
        int count = 0;
        char* lp, *filename;
        char buf[1024];
        bool found_p;
        
        SS_PUSHNAME("su_inifile_simplereg_checkforunregistered");
        
        /* checks only contents of solid.ini. FIXME: should check others too.
         * reason for limit was that ODBC stuff is read with inifiles and
         * those should not be checked. */

        /* should compare to su_inifile_filename */
        
        lp = filename = inifilename;
        while (*lp) { /* skip path */
            if (*lp == '\\' || *lp == '/') {
                filename = lp + 1;
            }
            lp++;
        }
        ss_dassert(*filename != '\0');
        if (SsStricmp("solid.ini", filename) != 0) {
            ss_dprintf(("checkforunregistered: ended up checking %s\n",
                        inifilename));
            SS_POPNAME;
            return TRUE;
        }

        inifile = su_inifile_init(inifilename, &found_p);
        if (!found_p) {
            su_inifile_done(inifile);            
            SS_POPNAME;
            return TRUE;
        }

#ifdef SS_DEBUG
        {
            /* here we do a sanity check for the simple registering lists */
            
            su_regis_deprecated_t* dp = su_regis_simple_replaced_register;
            bool reg;
            char* buf;
            char* secp, *keyp;
            bool compatibility;
            
            buf  = SsMemAlloc(1024);
            secp = buf;
            
            ss_dprintf_2(("su0inifi: sanitycheck of simplereg lists\n"));
            
            /* replaced parameters must be in reg list too */
                        
            for (; dp->dep_name != NULL; dp++) {
                strcpy(buf, dp->dep_name);
                
                keyp = strchr(secp, '.');
                ss_info_dassert(keyp != NULL, ("Entry %s has no '.'.", secp));
                *(keyp++) = '\0';
                
                reg = su_inifile_simplereg_isregistered(inifile,
                                                        secp, keyp);
                
                ss_info_dassert(reg == TRUE,
                                ("ReplacED parameter %s not registered. Register it.\n", dp->dep_name));
                ss_info_dassert(dp->dep_official != NULL,
                                ("ReplacED parameter %s has no official name. Fix it.\n", dp->dep_name));
                
                /* the replacing param must be there too... */
                strcpy(buf, dp->dep_official);                    
                keyp = strchr(secp, '.');
                ss_info_dassert(keyp != NULL,
                                ("Official entry %s has no '.'.", secp));
                *(keyp++) = '\0';
                reg = su_inifile_simplereg_isregistered(inifile,
                                                        secp, keyp);
                ss_info_dassert(reg == TRUE,
                                ("ReplacING parameter %s not registered. Register it.\n", dp->dep_official));
                
            }

            /* discontinued must NOT be in reg list */

            compatibility = su_inifile_simplereg_compatibilitymode;
            su_inifile_simplereg_compatibilitymode = FALSE;
            
            dp = su_regis_simple_discontinued_register;
            for (; dp->dep_name != NULL; dp++) {
                strcpy(buf, dp->dep_name);
                
                keyp = strchr(secp, '.');
                ss_info_dassert(keyp != NULL, ("Entry %s has no '.'.", secp));
                *(keyp++) = '\0';
                
                reg = su_inifile_simplereg_isregistered(inifile,
                                                        secp, keyp);
                
                ss_info_dassert(reg == FALSE, ("Discontinued parameter %s is registered. Remove it.\n", dp->dep_name));

                if (dp->dep_official != NULL) {
                    /* a replacing param must be there too... */
                    strcpy(buf, dp->dep_official);                    
                    keyp = strchr(secp, '.');
                    ss_info_dassert(keyp != NULL,
                                    ("Official entry %s has no '.'.", secp));
                    *(keyp++) = '\0';
                    reg = su_inifile_simplereg_isregistered(inifile,
                                                            secp, keyp);
                    ss_info_dassert(reg == TRUE,
                                    ("Discontinued's replacing parameter %s not registered. Register it.\n", dp->dep_official));

                }
            }
            su_inifile_simplereg_compatibilitymode = compatibility;
            
            SsMemFree(buf);
        }
#endif /* SS_DEBUG */
                       
        section = SsMemStrdup((char *)"<no section>");
        
        su_list_do_get(inifile->if_linelist, list_node, line) {
            switch (su_ifline_type_strict(line)) {
                case SU_IFLINE_SECTION:
                    SsMemFree(section);
                    section = su_inifile_parse_section(line);
                    break;
                case SU_IFLINE_KEYLINE:
                    key = su_inifile_keyname(SsMemStrdup(SsStrTrimLeft(line)));
                    reg = su_inifile_simplereg_isregistered(inifile,
                                                            section, key);
                    depr = su_inifile_simplereg_isdeprecated(section, key,
                                                             &depr_type);
                    if (depr) { /* deprecated */
                        if (depr_type
                            == SU_INIFILE_SREG_DEPR_REPLACED) {

                            char* official = su_inifile_simplereg_getofficial(
                                    section, key);
                            SsSprintf(buf, "Warning: Parameter %.256s.%.256s is deprecated. Use %.256s instead.\n", section, key, official);
                            su_inifile_simplereg_printbuf(buf);
                            
                            
                        } else if (depr_type ==
                                   SU_INIFILE_SREG_DEPR_DISCONTINUED) {

                            SsSprintf(buf, "Warning: Parameter %.256s.%.256s has been discontinued and has no effect.\n", section, key);
                            su_inifile_simplereg_printbuf(buf);
                            
                        } else if (depr_type
                            == SU_INIFILE_SREG_DEPR_DISCONTINUED_REPLACED) {

                            char* official = su_inifile_simplereg_getofficial(
                                    section, key);
                            SsSprintf(buf, "Warning: Parameter %.256s.%.256s has been discontinued and has no effect. Use %.256s instead.\n", section, key, official);
                            su_inifile_simplereg_printbuf(buf);
                            
                        } else {
                            ss_info_assert(0, ("depr_type = %d\n", depr_type));
                        }                    
                        count++;
                    } else if (!reg) { /* not registered */
                        if (print) {
                            SsSprintf(buf, "Warning: Unrecognized entry in inifile: %.256s.%.256s\n", section, key);
                            su_inifile_simplereg_printbuf(buf);
                        }
                        count++;
                    }                    
                    
                    SsMemFree(key);
                    break;
                case SU_IFLINE_ILLEGAL:
                    if (print) {
                        char* s = SsMemStrdup(line);
                        char* p;
                        /* remove CR/LF/CRLF */
                        for (p = s; *p; p++) {
                            if ( *p == '\n' || *p == '\r' ) {
                                *p = '\0';
                                break;
                            }
                        }
                        sprintf(buf, "Warning: Illegal entry in inifile: %.256s\n", s);
                        su_inifile_simplereg_printbuf(buf);
                        SsMemFree(s);
                    }
                    count++;
                    break;
                default:
                    break;
            }            
        }        
        SsMemFree(section);

        su_inifile_done(inifile);
        
        if (count > 0) {
            sprintf(buf, "Warning: %d unrecognized, illegal or deprecated entr%s in '%.256s'.\n",
                    count, (count == 1) ? "y": "ies", inifilename);
            su_inifile_simplereg_printbuf(buf);

            SS_POPNAME;
            
            return FALSE;
        }

        SS_POPNAME;
        
        return TRUE;
}

/*#***********************************************************************\
 *
 *      su_inifile_simplereg_getofficial
 *
 * Return the official parameter's name for a REPLACED deprecated parameter.
 *
 * Parameters:
 *      section - in
 *          section the parameter is in.
 *
 *      key - in
 *          key of the parameter.
 *
 * Return value - ref:
 *      pointer to the official parameter name as a "<section>.<key>" string.
 *
 * Limitations:
 *
 * Globals used:
 */
static char* su_inifile_simplereg_getofficial(char* section, char* key)
{
        su_regis_deprecated_t* p = su_regis_simple_replaced_register;
        char* s   = NULL;
        char* off = NULL;
        
        s = SsMemAlloc(strlen(section) + strlen(key) + 2);
        ss_dassert(s != NULL);
        SsSprintf(s, "%s.%s", section, key);

        while (p->dep_name != NULL) {
            if (SsStricmp(s, p->dep_name) == 0) {
                off = (char *)p->dep_official;
                ss_dassert(off != NULL);
                break;
            }
            p++;
        }

        if (off == NULL) { /* not found yet */
            p = su_regis_simple_discontinued_register;
            
            while (p->dep_name != NULL) {
                if (SsStricmp(s, p->dep_name) == 0) {
                    off = (char *)p->dep_official;
                    break;
                }
                p++;
            }
        }
        
        SsMemFree(s);

        ss_dassert(off != NULL); /* this function should not be called without
                                  * first checking if the parameter is
                                  * deprecated. */
        return off;
}

/*#***********************************************************************\
 *
 *      su_inifile_simplereg_isregistered
 *
 * Check if a parameter is registered in su0regis.c
 *
 * Parameters:
 *      inifile - in, use
 *          the inifile the parameter is in.
 *
 *      section - in, use
 *          section of the parameter.
 *
 *      key - in, use
 *          key of the parameter.
 *
 * Return value:
 *      TRUE, if it is registered.
 *      FALSE; if not.
 *
 * Limitations:
 *
 * Globals used:
 */
static bool su_inifile_simplereg_isregistered(su_inifile_t* inifile,
                                              char* section, char* key)
{
        char* s, **p;
        bool found = FALSE;
        char* fname;
        size_t fnlen;
        int i;
        static const char* igsectionlist[] = {
            "Data Sources",
            "Watchdog",
            "Test",
            NULL
        };

        if (su_inifile_simplereg_disablewarnings) {
            return TRUE;
        }
        
        /* === NOTE ===
         *
         * I have NO idea why this needs the inifile as the list of registered
         * parameters is not inifile specific in any way.
         *
         * The only reason seems to be so that some other files may be skipped
         * but is that really necessary?
         *
         */
        
        /* a few exceptions because they have a changing name or something:
         * - anything in anything else than solid.ini file.
         * - anything with [Data sources] (ODBCstuff)
         * - anything that is not registered and key is NetworkName (ODBCstuff)
         * - IndexFile.FileSpec_%u
         * - BLOBFile_%u
         * - Sorter.TmpDir_%u
         * - RCon.ServerDef_%u
         * - Test section (for tests...)
         *
         * - Watchdog section is ignored too.
         *
         * License stuff is really not a parameter but it doesn't change so
         * we can register (and check) those too.
         */

        fname = su_inifile_getname(inifile);
        fnlen = strlen(fname);

#if 0 /* mr 20040521: removed */
        /* FIXME: we may use inifiles with other names too */
        if (fnlen < 9 || SsStricmp(&(fname[fnlen-9]), "solid.ini") != 0) {
            /* if not in solid.ini -> don't check */
            ss_dprintf(("su_inifile_simplereg_isregistered: using %s instead of solid.ini\n", fname));
            return TRUE;
        }
#endif
        
        /* check exempted sections */
        for (i = 0; igsectionlist[i] != NULL; i++) {
            if (SsStricmp(igsectionlist[i], section) == 0) {
                return TRUE;
            }            
        }
        
        s = SsMemAlloc(strlen(section) + strlen(key) + 2);
        SsSprintf(s, "%s.%s", section, key);

        if ( (   SsStrnicmp("BLOBFile_",           s, 9)  == 0)
             || (SsStrnicmp("IndexFile.FileSpec_", s, 19) == 0) 
             || (SsStrnicmp("Sorter.TmpDir_",      s, 14) == 0) 
             || (SsStrnicmp("RCon.ServerDef_",     s, 15) == 0) ) {
            
            /* check that the rest is numbers */
            char *p = s, *q;
            bool ok = TRUE;

            q = p;
            while (*p) { 
                if (*p == '_') {
                    q = p + 1;
                }
                p++;                
            }
            ss_dassert(*q != '\0');
            p = q;
            while (*p) {
                if (!ss_isdigit(*p)) {
                    ok = FALSE;
                    break;
                }
                p++;
            }
            SsMemFree(s);

            return ok;
        }        
        
        p = (char **)su_regis_simple_register;
        while (*p) {
            if (SsStricmp(s, *p) == 0) {
                found = TRUE;
                break;
            }
            p++;
        }

        if (!found && su_inifile_simplereg_compatibilitymode) {
            su_regis_deprecated_t* dp = su_regis_simple_discontinued_register;
            while (dp->dep_name) {
                if (SsStricmp(s, dp->dep_name) == 0) {
                    found = TRUE;
                    break;
                }
                dp++;
            }
        }
        SsMemFree(s);

        /* here so that there can be legal parameters with this name */
        if (!found && SsStricmp(SU_ODBC_NETWORKNAME, key) == 0) {
            return TRUE;
        }
        
        return found;
}

/*#***********************************************************************\
 *
 *      su_inifile_simplereg_isdeprecated
 *
 * Check if a parameter is deprecated.
 *
 * Parameters:
 *      section - in
 *          section of the parameter
 *
 *      key - in
 *          key of the parameter
 *
 *      depr_type - out
 *          type of deprecation, may be NULL.
 *
 * Return value:
 *      TRUE, if deprecated.
 *      FALSE, if not.
 *
 * Limitations:
 *
 * Globals used:
 */
static bool su_inifile_simplereg_isdeprecated(char* section, char* key,
                                              su_inifile_sreg_depr_t* depr_type)
{
        su_regis_deprecated_t* p = su_regis_simple_replaced_register;
        char* s = NULL;
        su_inifile_sreg_depr_t null_temp;

        if (depr_type == NULL) {
            depr_type = &null_temp;
        }
        
        *depr_type = SU_INIFILE_SREG_DEPR_NOT;
        
        s = SsMemAlloc(strlen(section) + strlen(key) + 2);
        ss_dassert(s != NULL);
        SsSprintf(s, "%s.%s", section, key);

        while (p->dep_name != NULL) { /* if replaced */
            if (SsStricmp(s, p->dep_name) == 0) {
                *depr_type = SU_INIFILE_SREG_DEPR_REPLACED;
                break;                
            }
            p++;
        }

        if (*depr_type == SU_INIFILE_SREG_DEPR_NOT) {
            p = su_regis_simple_discontinued_register;

            while (p->dep_name != NULL) { /* if discontinued */
                if (SsStricmp(s, p->dep_name) == 0) {
                    if (p->dep_official == NULL) {
                        *depr_type
                            = SU_INIFILE_SREG_DEPR_DISCONTINUED;
                    } else {
                        *depr_type
                            = SU_INIFILE_SREG_DEPR_DISCONTINUED_REPLACED;
                    }
                    break;                
                }
                p++;
            }                
        }
        
        SsMemFree(s);

        return (*depr_type == SU_INIFILE_SREG_DEPR_NOT) ? FALSE: TRUE;
}

/*##**********************************************************************\
 * 
 *              su_inifile_search
 * 
 * Searches for the inifile
 * 
 * Parameters : 
 * 
 *      fname - in, use
 *          file name of inifile
 *              
 * Return value - give :  
 *      pointer to created & found full filename or
 *      NULL if file not found.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* su_inifile_search(char* fname)
{
        char* soliddir;
        char* fpath;
        char* curdir;
        char buf[512];

#if 1
        soliddir = SsGetEnv(SU_SOLIDDIR_ENVVAR);
        if (soliddir != NULL) {
            fpath = SsFnSearch(fname, soliddir);
            if (fpath != NULL) {
                return (fpath);
            }
        }
#endif
        if (strcmp(SsFileGetPathPrefix(),"") == 0){
            curdir = SsGetcwd(buf, sizeof(buf)-1);
            if (curdir != NULL) {
                fpath = SsFnSearch(fname, curdir);
                if (fpath != NULL) {
                    return (fpath);
                }
            }
        }
        fpath = SsFnSearch(fname, (char *)"");
        if (fpath != NULL) {
            return (fpath);
        }
        fpath = SsDataFileSearch(fname);
        return (fpath);
}

/*##**********************************************************************\
 * 
 *              inifile_gennewinifilename
 * 
 * Generates a file name for inifile that did not exist before
 * 
 * Parameters : 
 * 
 *      fname - in, use
 *              filename
 *              
 * Return value - give :
 *      new generated file name
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* inifile_gennewinifilename(char* fname)
{
        char* soliddir;
        char* fpath;
        size_t fpath_size;
        bool succp;
#if 1
        soliddir = SsGetEnv(SU_SOLIDDIR_ENVVAR);
        if (soliddir != NULL) {
            fpath_size = strlen(soliddir) + strlen(fname) + 2;
            fpath = SsMemAlloc(fpath_size);
            succp = SsFnMakePath(soliddir, fname, fpath, (int)fpath_size);
            ss_dassert(succp);
            return (fpath);
        }
#endif
        fpath = SsMemStrdup(fname);
        return (fpath);
}

/*#***********************************************************************\
 * 
 *              inifile_init
 * 
 * 
 * 
 * Parameters :          - none
 * 
 * Return value - give : 
 * 
 *      New (but empty) inifile object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static su_inifile_t* inifile_init(void)
{
        su_inifile_t* inifile;

        inifile = SSMEM_NEW(su_inifile_t);
        inifile->if_nlink = 1;
        ss_debug(inifile->if_check = SUCHK_INIFILE;)
        IF_CHECK(inifile);
        inifile->if_linelist = su_list_init(su_ifline_done);
        inifile->if_sections =
            su_rbt_inittwocmp(
                su_ifsection_cmp,
                su_ifsection_cmp2,
                (void (*)(void*))su_ifsection_done);
        inifile->if_mutex = SsSemCreateLocal(SS_SEMNUM_SU_INIFILE);
        inifile->if_changed = FALSE;
        inifile->if_ordersections = FALSE;
        inifile->if_ordersections = FALSE;
        inifile->if_registry = FALSE;
        inifile->if_filefound = FALSE;
        return(inifile);
}

/*##**********************************************************************\
 * 
 *              su_inifile_init
 * 
 * Creates a su_inifile_t object and reads the contents of the inifile
 * 
 * Parameters : 
 * 
 *      fname - in, use
 *          file name
 *
 *      found - out
 *          pointer to boolean variable where the information whether
 *          the specified file was found is stored. If the pointer is
 *          NULL, no output is written.
 *
 * Return value - give :
 *      pointer to created object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_inifile_t* su_inifile_init(char *fname, bool* found)
{
        su_inifile_t*   inifile;
        bool            b;

        SS_PUSHNAME("su_inifile_init");
        inifile = inifile_init();
        if (diskless_inifile) {
            /* for diskless, get the parameter from in memory ini file */
            b = inifile_fillfrommemory(inifile, fname, diskless_inifile);
        } else {
            b = inifile_fillfromfile(inifile, fname, TRUE);
        }
        inifile->if_filefound = b;
        if (found != NULL) {
            *found = b;
        }
        SS_POPNAME;

        return (inifile);
}

/*##**********************************************************************\
 * 
 *              su_inifile_initreg
 * 
 * Creates a new su_inifile_t object.
 * The contents is read from Win32 registry.
 * 
 * Parameters : 
 * 
 *      regname - 
 *              
 *      rootkey - 
 *              
 *      p_found - out, give
 *          pointer to boolean variable where the information whether
 *          the specified registry was found is stored. If the pointer is
 *          NULL, no output is written.
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#ifndef SS_MYSQL
su_inifile_t* su_inifile_initreg(
        const char*         regname,
        su_inifile_regkey_t rootkey,
        bool*               p_found)
{
        su_inifile_t*   inifile;
        bool            b;

        SS_PUSHNAME("su_inifile_initreg");
        inifile = inifile_init();
        b = inifile_fillfromregistry(inifile, regname, rootkey);
        if (p_found != NULL) {
            *p_found = b;
        }
        SS_POPNAME;
        return (inifile);
}
#endif /* !SS_MYSQL */

/*##**********************************************************************\
 * 
 *              su_inifile_done
 * 
 * Deletes a su_inifile_t object, but does not save the file
 * 
 * Parameters : 
 * 
 *      inifile - in, take
 *              pointer to inifile object
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_inifile_done(su_inifile_t* inifile)
{
        int nlink;

        ss_dassert(inifile != NULL);
        IF_CHECK(inifile);
        ss_dassert(inifile->if_nlink > 0);

        SsSemEnter(inifile->if_mutex);
        inifile->if_nlink--;
        nlink = inifile->if_nlink;
        SsSemExit(inifile->if_mutex);

        if (nlink == 0) {
            /* Last link to the object, do physical free.
             */
            su_rbt_done(inifile->if_sections);
            su_list_done(inifile->if_linelist);
            SsMemFree(inifile->if_fname);
            SsSemFree(inifile->if_mutex);
            SsMemFree(inifile);
        }
}

/*##**********************************************************************\
 * 
 *              su_inifile_link
 * 
 * Increments usage link count of inifile. The caller of this method
 * must call su_inifile_done to decrement the link count.
 * 
 * Parameters : 
 * 
 *      inifile - use
 *              pointer to inifile
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_inifile_link(su_inifile_t* inifile)
{
        ss_dassert(inifile != NULL);
        IF_CHECK(inifile);
        ss_dassert(inifile->if_nlink > 0);

        SsSemEnter(inifile->if_mutex);
        inifile->if_nlink++;
        SsSemExit(inifile->if_mutex);
}

/*##**********************************************************************\
 * 
 *              su_inifile_isfilefound
 * 
 * Returns TRUE if file was read from a file.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile
 *              
 * Return value:
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_isfilefound(
        su_inifile_t* inifile)
{
        IF_CHECK(inifile);
        return (inifile->if_filefound);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getname
 * 
 * Gives reference to inifile name
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile
 *              
 * Return value - ref :
 *      pointer to buffer containing the file name
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* su_inifile_getname(
        su_inifile_t* inifile)
{
        IF_CHECK(inifile);
        return (inifile->if_fname);
}

/*##**********************************************************************\
 * 
 *              su_inifile_entermutex
 * 
 * Locks the inifile object from other usage. This is needed when backing
 * the logfile up;
 * 
 * Parameters : 
 * 
 *      inifile - in out, use
 *              pointer to inifile object
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_inifile_entermutex(su_inifile_t* inifile)
{
        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
}

/*##**********************************************************************\
 * 
 *              su_inifile_exitmutex
 * 
 * Unlocks the inifile after backing it up
 * 
 * Parameters : 
 * 
 *      inifile - in out, use
 *              pointer to inifile object
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_inifile_exitmutex(su_inifile_t* inifile)
{
        IF_CHECK(inifile);
        SsSemExit(inifile->if_mutex);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getvalue
 * 
 * Gets a pointer to value part from a keyname=value string.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value_give - out, give
 *              pointer to start of value
 *              
 * Return value : 
 *      TRUE if the keyname was found under the specified section or
 *      FALSE when not found
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_getvalue(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_give)
{
        bool succp;
        char *value_ref;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        succp =
            su_inifile_getvalue_nomutex(
                inifile,
                (char *)section,
                (char *)keyname,
                &value_ref);
        if (succp) {
            *value_give = SsMemStrdup(value_ref);
        } else {
            *value_give = NULL;
        }
        SsSemExit(inifile->if_mutex);
        return (succp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getmillisec
 * 
 * Gets a keyname=value where the value is a string representing a valid
 * long integer value in milliseconds. User may (and should) state the
 * multiplier and the unit explicitly (s/sec for second and ms/msec 
 * for milliseconds).
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value - out
 *              pointer to long int value where the value is stored
 *              
 * Return value :
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_getmillisec(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        long* value)
{
        bool foundp;
        uint scanindex = 0;

        IF_CHECK(inifile);
        foundp = su_inifile_scanmillisec(
                    inifile,
                    section,
                    keyname,
                    default_separators,
                    &scanindex,
                    value);
        return (foundp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getlong
 * 
 * Gets a keyname=value where the value is a string representing a valid
 * long integer value.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value - out
 *              pointer to long int value where the value is stored
 *              
 * Return value :
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_getlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        long* value)
{
        bool foundp;
        uint scanindex = 0;

        IF_CHECK(inifile);
        foundp = su_inifile_scanlong(
                    inifile,
                    section,
                    keyname,
                    default_separators,
                    &scanindex,
                    value);
        return (foundp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getint8
 * 
 * Gets a keyname=value where the value is a string representing a valid
 * 8-byte integer value.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value - out
 *              pointer to int8 value where the value is stored
 *              
 * Return value :
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_getint8(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        ss_int8_t* value)
{
        bool foundp;
        uint scanindex = 0;

        IF_CHECK(inifile);
        foundp = su_inifile_scanint8(
                    inifile,
                    section,
                    keyname,
                    default_separators,
                    &scanindex,
                    value);
        return (foundp);
}


/*##**********************************************************************\
 * 
 *              su_inifile_getint
 * 
 * Gets a keyname=value where the value is a string representing a valid
 * long integer value.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value - out
 *              pointer to int value where the value is stored
 *              
 * Return value :
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_getint(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        int* value)
{
        bool foundp;
        uint scanindex = 0;
        long value_long;

        IF_CHECK(inifile);
        foundp = su_inifile_scanlong(
                    inifile,
                    section,
                    keyname,
                    default_separators,
                    &scanindex,
                    &value_long);
        if (foundp) {
            *value = value_long;
        }
        return (foundp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getdouble
 *
 * Gets a keyname=value where the value is a string representing a valid
 * double value.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value - out
 *              pointer to variable where the value is stored
 *              
 * Return value : 
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal double or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_getdouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        double* value)
{
        bool foundp;
        uint scanindex = 0;

        IF_CHECK(inifile);
        foundp = su_inifile_scandouble(
                    inifile,
                    section,
                    keyname,
                    default_separators,
                    &scanindex,
                    value);
        return (foundp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getbool
 * 
 * 
 * 
 * Parameters : 
 * 
 *      inifile - 
 *              
 *              
 *      section - 
 *              
 *              
 *      keyname - 
 *              
 *              
 *      p_value - 
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
bool su_inifile_getbool(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        bool* p_value)
{
        bool foundp;
        char* s;

        IF_CHECK(inifile);

        foundp = su_inifile_getstring(
                    inifile,
                    section,
                    keyname,
                    &s);
        if (foundp) {
            ss_dassert(s != NULL);
            *p_value = (*s == 'Y' || *s == 'y');
            SsMemFree(s);
        }
        return (foundp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getstring
 * 
 * Gets a string parameter assuming the valid separators characters are
 * space, tab, and comma
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *          pointer to inifile          
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value_give - out, give
 *              pointer to variable where the found value is stored
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_getstring(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_give)
{
        bool foundp;
        uint scanindex = 0;

        IF_CHECK(inifile);
        foundp = su_inifile_scanstring(
                    inifile,
                    section,
                    keyname,
                    default_separators,
                    &scanindex,
                    value_give);
        return (foundp);
}
/*##**********************************************************************\
 * 
 *              su_inifile_scanmillisec
 * 
 * Scans time value from keyline=value1, value2, ... line, which is
 * returned in milliseconds. 
 * 
 * User may (and should) state the multiplier and the unit explicitly 
 * (s/sec for second and ms/msec for milliseconds).
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *              
 *      scanindex - in out, use
 *              pointer to index in the value string. For the first value
 *          it should be initialized to 0.
 *              
 *      value - out
 *              pointer to variable where the scanned long int value is stored
 *              
 * Return value : 
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_scanmillisec(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        long* value)
{
        char* value_str;
        char* p_mismatch;
        bool foundp;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        if (separators == NULL) {
            separators = default_separators;
        }
        foundp =
            su_inifile_getvalue_nomutex(
                inifile,
                section,
                keyname,
                &value_str);
        if (!foundp) {
            SsSemExit(inifile->if_mutex);
            return (FALSE);
        }
        ss_dassert(strlen(value_str) >= *scanindex);
        foundp = SsStrScanLong(value_str + *scanindex, value, &p_mismatch);
        if (foundp) {
            char* p;
            int i;
            static struct {
                char *name;
                int mult;
            } units[] = {
                /* 
                 * must be sorted in descending order! 
                 *
                 */
                { (char *)"msec",   1,    },
                { (char *)"ms",     1,    },
                { (char *)"sec",    1000, },
                { (char *)"s",      1000, },
            };

            p = SsStrTrimLeft(p_mismatch);

            for (i = 0; (size_t)i < sizeof(units) / sizeof(units[0]); i++) {
                size_t len;
                len = strlen(units[i].name);

                if(SsStrncmp(p, units[i].name, len) == 0) {
                    *value *= units[i].mult;
                    p_mismatch = p + len;
                    break;
                }
            }

            /* Skip separators.
            */
            p_mismatch = su_inifile_skipseparators(p_mismatch, separators);
        }
        *scanindex = (uint)(p_mismatch - value_str);
        SsSemExit(inifile->if_mutex);
        return (foundp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_scanlong
 * 
 * Scans a long int value from keyline=value1, value2, ... line
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *              
 *      scanindex - in out, use
 *              pointer to index in the value string. For the first value
 *          it should be initialized to 0.
 *              
 *      value - out
 *              pointer to variable where the scanned long int value is stored
 *              
 * Return value : 
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_scanlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        long* value)
{
        char* value_str;
        char* p_mismatch;
        bool foundp;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        if (separators == NULL) {
            separators = default_separators;
        }
        foundp =
            su_inifile_getvalue_nomutex(
                inifile,
                section,
                keyname,
                &value_str);
        if (!foundp) {
            SsSemExit(inifile->if_mutex);
            return (FALSE);
        }
        ss_dassert(strlen(value_str) >= *scanindex);
        foundp = SsStrScanLong(value_str + *scanindex, value, &p_mismatch);
        if (foundp) {
            /* Check if there is kilo or mega specification.
             */
            char* p;
            long mult = 0;
            p = SsStrTrimLeft(p_mismatch);
            switch (*p) {
                case 'k':
                case 'K':
                    /* Value is kilobytes. */
                    mult = 1024L;
                    break;
                case 'm':
                case 'M':
                    /* Value is megabytes. */
                    mult = 1024L * 1024L;
                    break;
                default:
                    break;
            }
            if (mult != 0 && strchr(separators, *p) == NULL) {
                /* Not a separator, multiply value. */
                *value *= mult;
                p_mismatch = p + 1;
            }
            /* Skip separators.
             */
            p_mismatch = su_inifile_skipseparators(p_mismatch, separators);
        }
        *scanindex = (uint)(p_mismatch - value_str);
        SsSemExit(inifile->if_mutex);
        return (foundp);
}


/*##**********************************************************************\
 * 
 *              su_inifile_scanint8
 * 
 * Scans a 8-byte int value from keyline=value1, value2, ... line
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *              
 *      scanindex - in out, use
 *              pointer to index in the value string. For the first value
 *          it should be initialized to 0.
 *              
 *      value - out
 *              pointer to variable where the scanned int8 value is stored
 *              
 * Return value : 
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_scanint8(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        ss_int8_t* value)
{
        char* value_str;
        char* p_mismatch;
        bool foundp;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        if (separators == NULL) {
            separators = default_separators;
        }
        foundp =
            su_inifile_getvalue_nomutex(
                inifile,
                section,
                keyname,
                &value_str);
        if (!foundp) {
            SsSemExit(inifile->if_mutex);
            return (FALSE);
        }
        ss_dassert(strlen(value_str) >= *scanindex);
        foundp = SsStrScanInt8(value_str + *scanindex, value, &p_mismatch);
        if (foundp) {
            /* Check if there is kilo or mega specification.
             */
            char* p;
            ss_int8_t mult;

            p = SsStrTrimLeft(p_mismatch);
            switch (*p) {
                case 'k':
                case 'K':
                    /* Value is kilobytes. */
                    SsInt8SetUint4(&mult, 1024U);
                    break;
                case 'm':
                case 'M':
                    /* Value is megabytes. */
                    SsInt8SetUint4(&mult, (ss_uint4_t)(1024L * 1024L));
                    break;
                case 'g':
                case 'G':
                    /* Value is gigabytes. */
                    SsInt8SetUint4(&mult, (ss_uint4_t)(1024L * 1024L * 1024L));
                    break;
                default:
                    goto void_multiply;
            }
            if (strchr(separators, *p) == NULL) {
                /* Not a separator, multiply value. */
                SsInt8MultiplyByInt8(value, *value, mult);
                p_mismatch = p + 1;
            }
    void_multiply:;
            /* Skip separators.
             */
            p_mismatch = su_inifile_skipseparators(p_mismatch, separators);
        }
        *scanindex = (uint)(p_mismatch - value_str);
        SsSemExit(inifile->if_mutex);
        return (foundp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_scandouble
 * 
 * Scans a double value from a keyname=value1, value2, ... line
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use 
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *              
 *      scanindex - in out, use
 *              pointer to index to value string. For the first value it 
 *          should be initialized to 0
 *              
 *      value - out
 *              pointer to variable where the value is stored
 *              
 * Return value : 
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal double or
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_scandouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        double* value)
{
        char* value_str;
        char* p_mismatch;
        bool foundp;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        if (separators == NULL) {
            separators = default_separators;
        }
        foundp =
            su_inifile_getvalue_nomutex(
                inifile,
                section,
                keyname,
                &value_str);
        if (!foundp) {
            SsSemExit(inifile->if_mutex);
            return (FALSE);
        }
        ss_dassert(strlen(value_str) >= *scanindex);
        foundp = SsStrScanDouble(value_str + *scanindex, value, &p_mismatch);
        if (foundp) {
            p_mismatch = su_inifile_skipseparators(p_mismatch, separators);
            *scanindex = (uint)(p_mismatch - value_str);
        }
        SsSemExit(inifile->if_mutex);
        return (foundp);
}


/*##**********************************************************************\
 * 
 *              su_inifile_scanstring
 * 
 * Scans a string value from a keyname=value1, value2, ... line
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *              
 *              
 *      scanindex - in out, use
 *              pointer to index to value string. For the first value it 
 *          should be initialized to 0
 *              
 *      value_give - out, give
 *              pointer to char* where a copy of the scanned string is stored
 *              
 * Return value :
 *      TRUE when a valid value was found or
 *      FALSE otherwise    
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_scanstring(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        char** value_give)
{
        char* value_str;
        bool foundp;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);

        foundp = su_inifile_getvalue_nomutex(inifile,
                                             section,
                                             keyname,
                                             &value_str);
        if (!foundp) {
            *value_give = NULL;
            SsSemExit(inifile->if_mutex);

            return (FALSE);
        }

        foundp = SsStrScanStringWQuoting(value_str, (char *)separators, scanindex,
                                         ';', value_give);
        
        SsSemExit(inifile->if_mutex);
        return (foundp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_putstring
 * 
 * puts a keyname=value string under the specified section name.
 * 
 * Parameters : 
 * 
 *      inifile - in out, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value - in, use
 *              the value string
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_inifile_putstring(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* value)
{
        su_rbt_node_t* rbt_node;
        su_ifsection_t* section_ptr;
        su_list_node_t* list_node;
        su_ifkeyline_t* keyline;
        char* line;

        IF_CHECK(inifile);
        SS_PUSHNAME("su_inifile_putstring");
        SsSemEnter(inifile->if_mutex);
        line = SsMemAlloc(strlen(keyname)+1+strlen(value)+1+1); /* = and \n */
        strcpy(line, keyname);
        strcat(line, "=");
        strcat(line, value);
        strcat(line, "\n");
        rbt_node = su_rbt_search(inifile->if_sections, (char *)section);
        if (rbt_node == NULL) {
            size_t len = strlen(section);
            char* sect_line = SsMemAlloc(len+(3+1));

            *sect_line = '[';
            memcpy(sect_line + 1, section, len);
            memcpy(sect_line + len + 1, "]\n", 3);
            list_node = su_list_insertlast(inifile->if_linelist, sect_line);
            section_ptr = su_ifsection_init(list_node);
            su_rbt_insert(inifile->if_sections, section_ptr);
        } else {
            section_ptr = su_rbtnode_getkey(rbt_node);
            list_node = section_ptr->ifs_lnode;
        }
        rbt_node = su_rbt_search(section_ptr->ifs_keylines, (char *)keyname);
        if (rbt_node != NULL) { /* exists in inifile */
            char* cp, *np;
            char* oldline;
            keyline = su_rbtnode_getkey(rbt_node);
            list_node = keyline->ifkl_lnode;

            /* insert possible old comment after new value */
            oldline = su_listnode_getdata(list_node);
            cp = strchr(oldline, INIFILE_COMMENTCHAR);
            if (cp != NULL) {
                np = strstr(line, "\n");
                ss_dassert(np != NULL);
                *np = '\0';
                oldline = line; /* new line is now old line as there is a
                                 * newer line after next call */
                line = SsMemAlloc(strlen(oldline)+strlen(cp)+2);
                strcpy(line, oldline);
                strcat(line, " "); /* just for the look, clarity */
                strcat(line, cp);
                SsMemFree(oldline);
            }
            (void)su_list_setdata(inifile->if_linelist, list_node, line);
            (void)su_rbt_delete(section_ptr->ifs_keylines, rbt_node);
        } else {
            if (inifile->if_ordersections) {
                char* tmp_line;
                su_list_node_t* tmp_next;
                while ((tmp_next = su_list_next(inifile->if_linelist, list_node)) != NULL) {
                    tmp_line = su_listnode_getdata(tmp_next);
                    if (su_ifline_type(tmp_line) == SU_IFLINE_SECTION) {
                        break;
                    }
                    list_node = tmp_next;
                }
            }
            list_node = su_list_insertafter(inifile->if_linelist, list_node, line);
        }
        keyline = su_ifkeyline_init(list_node);
        su_rbt_insert(section_ptr->ifs_keylines, keyline);
        inifile->if_changed = TRUE;
        SsSemExit(inifile->if_mutex);
        SS_POPNAME;
}

/*##**********************************************************************\
 * 
 *              su_inifile_putlong
 * 
 * Puts a keyname=integervalue string under the specified section
 * 
 * Parameters : 
 * 
 *      inifile - in out, use
 *              pointer to inifile object
 *              
 *      section - in, use 
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value - in
 *              long integer value
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_inifile_putlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        long value)
{
        char buf[12];

        IF_CHECK(inifile);
        SsSprintf(buf, "%ld", value);
        su_inifile_putstring(inifile, section, keyname, buf);
}

/*##**********************************************************************\
 *
 *      su_inifile_putbool
 *
 * Puts a keyname=booleanvalue string under the specified section
 *
 * Parameters :
 *
 *  inifile - in out, use
 *      pointer to inifile object
 *
 *  section - in, use
 *      section name
 *
 *  keyname - in, use
 *      key name
 *
 *  value - in
 *      long integer value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void su_inifile_putbool(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        bool value)
{
        IF_CHECK(inifile);
        su_inifile_putstring(inifile, section, keyname, value ? "Y":"N");
}


/*##**********************************************************************\
 * 
 *              su_inifile_putdouble
 * 
 * Puts a keyname=floatingvalue string under the specified section
 * 
 * Parameters : 
 * 
 *      inifile - in out, use
 *          pointer to inifile object
 *              
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              section name
 *              
 *      value - in
 *              double value
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_inifile_putdouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        double value)
{
        char buf[25];

        IF_CHECK(inifile);
        SsDoubleToAscii(value, buf, 15);
        su_inifile_putstring(inifile, section, keyname, buf);
}

/*##**********************************************************************\
 * 
 *              su_inifile_deletekeyline
 * 
 * Deletes a keyname=value line from inifile
 * 
 * Parameters : 
 * 
 *      inifile - in out, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 * Return value : TRUE if keyline existed or
 *                FALSE when not
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_deletekeyline(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname)
{
        su_rbt_node_t* rbt_node;
        su_ifsection_t* section_ptr;
        su_ifkeyline_t* keyline;
        su_list_node_t* list_node;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        rbt_node = su_rbt_search(inifile->if_sections, (char *)section);
        if (rbt_node == NULL) {
            SsSemExit(inifile->if_mutex);
            return (FALSE);
        }
        section_ptr = su_rbtnode_getkey(rbt_node);
        rbt_node = su_rbt_search(section_ptr->ifs_keylines, (char *)keyname);
        if (rbt_node == NULL) {
            SsSemExit(inifile->if_mutex);
            return (FALSE);
        }
        keyline = su_rbtnode_getkey(rbt_node);
        list_node = keyline->ifkl_lnode;
        (void)su_rbt_delete(section_ptr->ifs_keylines, rbt_node);
        su_list_remove(inifile->if_linelist, list_node);
        inifile->if_changed = TRUE;
        SsSemExit(inifile->if_mutex);
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *              su_inifile_deletesection
 * 
 * Deletes a [Section Name] line and all the keyname=value lines below
 * that section header. Does not remove comment lines.
 * 
 * Parameters : 
 * 
 *      inifile - in out, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 * Return value : TRUE when found or
 *                FALSE when not
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_deletesection(
        su_inifile_t* inifile,
        const char* section)
{
        su_rbt_node_t* rbt_node;
        su_ifsection_t* section_ptr;
        su_ifkeyline_t* keyline;
        su_list_node_t* list_node;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        rbt_node = su_rbt_search(inifile->if_sections, (char *)section);
        if (rbt_node == NULL) {
            SsSemExit(inifile->if_mutex);
            return (FALSE);
        }
        section_ptr = su_rbtnode_getkey(rbt_node);
        for (;;) {
            su_rbt_node_t* rbt_node;
            rbt_node = su_rbt_min(section_ptr->ifs_keylines, NULL);
            if (rbt_node == NULL) {
                break;
            }
            keyline = su_rbtnode_getkey(rbt_node);
            list_node = keyline->ifkl_lnode;
            (void)su_rbt_delete(section_ptr->ifs_keylines, rbt_node);
            su_list_remove(inifile->if_linelist, list_node);
        }
        list_node = section_ptr->ifs_lnode;
        (void)su_list_remove(inifile->if_linelist, list_node);
        (void)su_rbt_delete(inifile->if_sections, rbt_node);
        SsSemExit(inifile->if_mutex);
        return (TRUE);
}


/*##**********************************************************************\
 * 
 *              su_inifile_save
 * 
 * Saves the inifile to disk
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile
 *              
 * Return value :
 *      TRUE when OK
 *      FALSE when error
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_save(su_inifile_t* inifile)
{
        bool succp = TRUE;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
        if (inifile->if_changed) {
            succp = su_inifile_saveas_nomutex(inifile, inifile->if_fname);
            if (succp) {
                inifile->if_changed = FALSE;
            }
        }
        SsSemExit(inifile->if_mutex);
        return (succp);
}

/*##**********************************************************************\
 * 
 *              su_inifile_saveas
 * 
 * Saves the inifile with a new name (maybe)
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile
 *              
 *      fname - in, use
 *              file name
 *              
 * Return value : 
 *      TRUE when OK
 *      FALSE when error
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_inifile_saveas(su_inifile_t* inifile, char* fname)
{
        bool succp;

        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        succp = su_inifile_saveas_nomutex(inifile, fname);
        SsSemExit(inifile->if_mutex);
        return (succp);
}

/*##**********************************************************************\
 * 
 *              inifile_savecallback_nomutex
 * 
 * Saves the inifile with given file callback function
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile
 *              
 *      callback - in, use
 *              
 *      ctx - in, use
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void inifile_savecallback_nomutex(su_inifile_t* inifile, void(*callback)(void* ctx, char* line), void* ctx)
{
        char* line;
        su_list_node_t* list_node;

        su_list_do_get(inifile->if_linelist, list_node, line) {
            while (strlen(line) > SAVE_MAXLINELEN) {
                char buf[SAVE_MAXLINELEN + 10];
                if (strlen(line) == SAVE_MAXLINELEN + 1
                    && line[SAVE_MAXLINELEN] == '\n') {
                    break;
                }
                strncpy(buf, line, SAVE_MAXLINELEN);
                buf[SAVE_MAXLINELEN] = '\0';
                strcat(buf, "\\\n");
                (*callback)(ctx, buf);
                line += SAVE_MAXLINELEN;
            }
            (*callback)(ctx, line);
        }
}

static void inifile_savecallbackfun(void* ctx, char* line)
{
        SS_FILE* fp = ctx;

        SsFPuts(line, fp);
}

/*##**********************************************************************\
 * 
 *              inifile_savefp_nomutex
 * 
 * Saves the inifile with given file pointer
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile
 *              
 *      fname - in, use
 *              file name
 *              
 * Return value : 
 *      TRUE when OK
 *      FALSE when error
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void inifile_savefp_nomutex(su_inifile_t* inifile, SS_FILE* fp)
{
        inifile_savecallback_nomutex(inifile, inifile_savecallbackfun, fp);
}

/*#***********************************************************************\
 * 
 *              su_inifile_saveas_nomutex
 * 
 * Saves the inifile with given name, for local use
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile
 *              
 *      fname - in, use
 *              file name
 *              
 * Return value : 
 *      TRUE when OK
 *      FALSE when error
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool su_inifile_saveas_nomutex(su_inifile_t* inifile, char* fname)
{
#ifndef SS_MYSQL
        su_vfile_t *vfp;
        SS_FILE* fp;

        if (inifile->if_registry){
            /* Currently we do not support save-method for registry
             * based inifiles
             */
            ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
            return(FALSE);
        }
        if (!su_vfh_isinitialized()) {
            ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
            return (FALSE);
        }
        vfp = su_vfp_init_txt(fname, (char *)"w");
        if (vfp == NULL) {
            ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
            return (FALSE);
        }
        fp = su_vfp_access(vfp);
        inifile_savefp_nomutex(inifile, fp);
        su_vfp_done(vfp);
#endif
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *              su_inifile_savefp
 * 
 * Saves inifile into givel file pointer.
 * 
 * Parameters : 
 * 
 *              inifile - 
 *                      
 *                      
 *              fp - 
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
void su_inifile_savefp(su_inifile_t* inifile, SS_FILE* fp)
{
        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        inifile_savefp_nomutex(inifile, fp);
        SsSemExit(inifile->if_mutex);
}

void su_inifile_savecallback(su_inifile_t* inifile, void (*callback)(void* ctx, char* line), void* ctx)
{
        IF_CHECK(inifile);
        SsSemEnter(inifile->if_mutex);
        inifile_savecallback_nomutex(inifile, callback, ctx);
        SsSemExit(inifile->if_mutex);
}

/*#**********************************************************************\
 * 
 *              su_inifile_getvalue_nomutex
 * 
 * Gets a pointer to value part from a keyname=value string.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 *      value_ref - out, ref
 *              pointer to start of value
 *              
 * Return value : 
 *      TRUE if the keyname was found under the specified section or
 *      FALSE when not found
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool su_inifile_getvalue_nomutex(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_ref)
{
        char* keyline;
        bool b __attribute__ ((unused));
        char buf[1024] __attribute__ ((unused));

        b = FALSE;
        
#ifdef SS_DEBUG
        /* we check only in DEBUG build as only then we printed anything */
        b = su_inifile_simplereg_isregistered(inifile, (char *)section, (char *)keyname);

        if (!b) {
            sprintf(buf, "Warning: Unregistered parameter %.256s.%.256s is used.\n",
                    section, keyname);
            su_inifile_simplereg_printbuf(buf);
            if (su_inifile_serverside) {
                b = su_inifile_simplereg_isdeprecated((char *)section, (char *)keyname, NULL);
                /* only discontinued deprecated param are unregistered */
                ss_info_assert(!b, ("Discontinued parameter %s.%s used. Bad dog.\n", section, keyname));
                ss_info_assert(0, ("Unregistered parameter %s.%s used. Add it to su/su0regis.c\n", section, keyname));
            }
        }
#endif /* SS_DEBUG */
        
        keyline = su_inifile_getkeyline(inifile, section, keyname);

        if (keyline == NULL) {
            *value_ref = NULL;
            return (FALSE);
        }
        *value_ref = su_inifile_keylinevalue(keyline);

        return (TRUE);
}

/*#***********************************************************************\
 * 
 *              su_inifile_getkeyline
 * 
 * Gets pointer to keyname=value line
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section_name - in, use
 *              section name
 *              
 *      keyname - in, use
 *              key name
 *              
 * Return value - ref : 
 *      pointer to keyname=value line or
 *      NULL if keyname was not found
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static char* su_inifile_getkeyline(
        su_inifile_t* inifile,
        const char* section_name,
        const char* keyname)
{
        su_ifsection_t* section_ptr;
        su_ifkeyline_t* keyline;
        su_list_node_t* lnode;
        su_rbt_node_t* rbt_node;
        char* line;

        rbt_node = su_rbt_search(inifile->if_sections, (char *)section_name);
        if (rbt_node == NULL) {
            return (NULL);
        }
        section_ptr = su_rbtnode_getkey(rbt_node);
        ss_dassert(section_ptr != NULL);
        rbt_node = su_rbt_search(section_ptr->ifs_keylines, (char *)keyname);
        if (rbt_node == NULL) {
            return (NULL);
        }
        keyline = su_rbtnode_getkey(rbt_node);
        lnode = keyline->ifkl_lnode;
        line = su_listnode_getdata(lnode);
        return (line);
}


/*#***********************************************************************\
 * 
 *              su_inifile_getnthkeyline
 * 
 * Gets pointer to nth keyname=value line under given section
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              pointer to inifile object
 *              
 *      section_name - in, use
 *              section name
 *              
 * Return value - give : 
 *      pointer to a created keyname=value line or
 *      NULL if keyname was not found
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
char* su_inifile_getnthkeyline(
        su_inifile_t* inifile,
        const char* section_name,
        uint n)
{
        su_ifsection_t* section_ptr;
        su_list_node_t* lnode;
        su_rbt_node_t* rbt_node;
        char* line,*keyname;
        uint line_num;

        IF_CHECK(inifile);
        rbt_node = su_rbt_search(inifile->if_sections, (char *)section_name);
        if (rbt_node == NULL) {
            return (NULL);
        }
        section_ptr = su_rbtnode_getkey(rbt_node);
        ss_dassert(section_ptr != NULL);
        
        lnode = section_ptr->ifs_lnode;
        line_num = 0;
        
        for (;;) {
            lnode = su_list_next(inifile->if_linelist,lnode);
        
            if (lnode == NULL) {
                /* No more items in list */
                return NULL;
            }
            line = su_listnode_getdata(lnode);

            switch (su_ifline_type(line)) {
                case SU_IFLINE_SECTION:
                    /* Next section found */
                    return NULL;
                case SU_IFLINE_KEYLINE:
                    line_num++;
                    if (line_num == n) {
                        line = su_inifile_skipseparators(line,"\t ");
                        keyname = SsMemStrdup(line);
                        return (su_inifile_keyname(keyname));
                    }
                    break;
                case SU_IFLINE_COMMENT:
                    break;
                default:
                    ss_error;
            }
        }
}

/*##**********************************************************************\
 * 
 *              su_inifile_putline
 * 
 * Add a new "raw line" to the inifile. 
 * This can be used for instance adding comments or non-standard
 * lines to the inifile, as well as ordinary keylines.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *              
 *              
 *      sectname - in, use
 *              section name
 *              If the section does not already exist, it is created
 *              before adding the line
 *              
 *      line - in, use
 *              The buffer containing the line to be added into section
 *              
 * Return value : 
 *              
 *          TRUE, if successful
 *              
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_inifile_putline(
                su_inifile_t* inifile,
                const char* sectname,
                const char* line)
{
        su_list_node_t* listnode;
        su_ifkeyline_t* keyline;
        su_ifsection_t* section = NULL;
        su_rbt_node_t*  rbt_node;
        su_iflinetype_t linetype;
        char*           line_new;
        su_list_node_t* lnode;
        bool            insertp;

        linetype = su_ifline_type((char *)line);
        ss_dassert(linetype != SU_IFLINE_SECTION);
        ss_dassert(linetype == SU_IFLINE_KEYLINE ||
                   linetype == SU_IFLINE_COMMENT);

        rbt_node = su_rbt_search(inifile->if_sections, (char *)sectname);
        if (rbt_node == NULL) {
            /* There is not yet such section, create one */
            char* sectline = SsMemAlloc(strlen(sectname) + 1 + 3);
            SsSprintf(sectline, "[%s]\n", sectname);
            listnode = su_list_insertlast(
                            inifile->if_linelist,
                            sectline);
            section = su_ifsection_init(listnode);
            su_rbt_insert(inifile->if_sections, section);
        } else {
            section = su_rbtnode_getkey(rbt_node);
            listnode = section->ifs_lnode;
        }
        if (strchr(line, '\n') == NULL) {
            line_new = SsMemAlloc(strlen(line) + 1 + 1);
            SsSprintf(line_new, "%s\n", line);
        } else {
            line_new = SsMemStrdup((char *)line);
        }

        /* put this line as last in this section,
         * begin iteration from sectionname and continue until next section
         * begins or the end of inifile is reached.
         */
        lnode = listnode;
        insertp = FALSE;

        while (!insertp) {

            lnode = su_list_next(inifile->if_linelist,lnode);
        
            if (lnode == NULL) {
                /* last line of inifile reached */
                listnode = su_list_insertlast(inifile->if_linelist, line_new);
                insertp = TRUE;
                break;
            }
            line = su_listnode_getdata(lnode);

            switch (su_ifline_type((char *)line)) {
                case SU_IFLINE_SECTION:
                    /* Next section found */
                    su_list_insertbefore(inifile->if_linelist, lnode, line_new);
                    insertp = TRUE;    
                    break;
                case SU_IFLINE_KEYLINE:
                case SU_IFLINE_COMMENT:
                    break;
                default:
                    ss_error;
            }
        }

        if (linetype == SU_IFLINE_KEYLINE) {
            keyline = su_ifkeyline_init(listnode);
            su_rbt_insert(section->ifs_keylines, keyline);
        }
        inifile->if_changed = TRUE;
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              su_inifile_getnthline
 * 
 * Get nth "raw line" from requested inifile section. 
 * 
 * 
 * Parameters : 
 * 
 *      inifile - 
 *              
 *              
 *      section_name - 
 *              
 *              
 *      n - 
 *              
 *              
 * Return value - give: 
 * 
 *      nth line in section
 *      NULL, if not found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* su_inifile_getnthline(
        su_inifile_t* inifile,
        const char* section_name,
        uint n)
{
        su_ifsection_t* section_ptr;
        su_list_node_t* lnode;
        su_rbt_node_t* rbt_node;
        char* line;
        uint line_num;

        IF_CHECK(inifile);
        rbt_node = su_rbt_search(inifile->if_sections, (char *)section_name);
        if (rbt_node == NULL) {
            return (NULL);
        }
        section_ptr = su_rbtnode_getkey(rbt_node);
        ss_dassert(section_ptr != NULL);
        
        lnode = section_ptr->ifs_lnode;
        line_num = 0;
        
        for (;;) {
            lnode = su_list_next(inifile->if_linelist,lnode);
        
            if (lnode == NULL) {
                /* last line of inifile reached */
                return NULL;
            }
            line = su_listnode_getdata(lnode);

            switch (su_ifline_type(line)) {
                case SU_IFLINE_SECTION:
                    /* Next section found */
                    return NULL;
                case SU_IFLINE_KEYLINE:
                case SU_IFLINE_COMMENT:
                    line_num++;
                    if (line_num == n) {
                        return(SsMemStrdup(line));
                    }
                    break;
                default:
                    ss_error;
            }
        }
}

/*##**********************************************************************\
 * 
 *              su_inifile_deletenthline
 * 
 * Deletes nth "raw line" in inifile section
 * 
 * Parameters : 
 * 
 *      inifile - 
 *              
 *              
 *      section_name - 
 *              
 *              
 *      n - 
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
bool su_inifile_deletenthline(
        su_inifile_t* inifile,
        const char* section_name,
        uint n)
{
        su_ifsection_t* section_ptr;
        su_list_node_t* lnode;
        su_rbt_node_t* rbt_node;
        char* line;
        uint line_num;

        IF_CHECK(inifile);
        rbt_node = su_rbt_search(inifile->if_sections, (char *)section_name);
        if (rbt_node == NULL) {
            return (FALSE);
        }
        section_ptr = su_rbtnode_getkey(rbt_node);
        ss_dassert(section_ptr != NULL);
        
        lnode = section_ptr->ifs_lnode;
        line_num = 0;
        
        for (;;) {
            lnode = su_list_next(inifile->if_linelist,lnode);
        
            if (lnode == NULL) {
                /* last line of inifile reached */
                return(FALSE);
            }
            line = su_listnode_getdata(lnode);

            switch (su_ifline_type(line)) {
                case SU_IFLINE_SECTION:
                    /* Next section found */
                    return(FALSE);

                case SU_IFLINE_KEYLINE:
                    line_num++;
                    if (line_num == n) {
                        char* keyname;
                        line = su_inifile_skipseparators(line,"\t ");
                        keyname = SsMemStrdup(line);
                        su_inifile_deletekeyline(
                            inifile,
                            section_name,
                            su_inifile_keyname(keyname));
                        SsMemFree(keyname);
                        return(TRUE);
                    }
                    break;

                case SU_IFLINE_COMMENT:
                    line_num++;
                    if (line_num == n) {
                        su_list_remove(inifile->if_linelist,lnode);
                        inifile->if_changed = TRUE;
                        return(TRUE);
                    }
                    break;
                default:
                    ss_error;
            }
        }
}

/*##**********************************************************************\
 * 
 *              su_inifile_ordersections
 * 
 * After setting ordersections to TRUE new [section]keyname=value is
 * added last in its section. By default new [section]keyname=value
 * is always added first in its section.
 * 
 * Parameters : 
 * 
 *      inifile - 
 *              
 *      onoff - in, use 
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
void su_inifile_ordersections(su_inifile_t* inifile, bool onoff)
{
        IF_CHECK(inifile);
        inifile->if_ordersections = onoff;
}


/*#***********************************************************************\
 * 
 *              su_inifile_keyname
 * 
 * Scans the keyname part from a keyname=value line.
 * Note: Before calling this function all leading spaces should be removed
 *       from the beginning of the line.
 *
 *
 * Parameters : 
 * 
 *      keyline - in, use
 *              the line
 *              
 * Return value - ref : 
 *      pointer to value
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static char *su_inifile_keyname(char* keyline)
{
        int i;

        for (i = 0 ;; i++) {
            switch (keyline[i]) {
                case '=':
                case ' ':
                    keyline[i] = '\0';
                    return keyline;
                case '\n':
                case '\0':
                    ss_error;
                default:
                    break;
            }
        }
}



/*#***********************************************************************\
 * 
 *              su_inifile_keylinevalue
 * 
 * Scans the value part from a keyname=value line.
 * 
 * Parameters : 
 * 
 *      keyline - in, use
 *              the line
 *              
 * Return value - ref : 
 *      pointer to value
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static char *su_inifile_keylinevalue(char* keyline)
{
        for (;; keyline++) {
            switch (*keyline) {
                case '=':
                    keyline++;
                    return keyline;
                case '\n':
                case '\0':
                    ss_error;
                default:
                    break;
            }
        }
}

/*#***********************************************************************\
 * 
 *              su_inifile_skipseparators
 * 
 * Scans over the next comma in the string and returns pointer to that
 * position or end of string
 * 
 * Parameters : 
 * 
 *      p - in, use
 *          pointer to line pos
 *              
 * Return value - ref : 
 *          pointer to scanned position
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static char* su_inifile_skipseparators(char *p, const char* separators)
{
        char map[1 << CHAR_BIT];

        memset(map, 0, sizeof(map));
        for (; *separators != '\0'; separators++) {
            map[(uchar)*separators] = (char)~0;
        }
        while (map[(uchar)*p]) {
            p++;
        }
        return (p);
}

/*#***********************************************************************\
 * 
 *              su_ifsection_init
 * 
 * Creates a section record
 * 
 * Parameters : 
 * 
 *      lnode - list node that contains the inifile line
 *              
 * Return value - give : 
 *      pointer to created object
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static su_ifsection_t* su_ifsection_init(su_list_node_t* lnode)
{
        su_ifsection_t* ifsection;

        ifsection = SSMEM_NEW(su_ifsection_t);
        ifsection->ifs_keylines =
            su_rbt_inittwocmp(
                su_ifkeyline_cmp,
                su_ifkeyline_cmp2,
                (void (*)(void*))su_ifkeyline_done);
        ifsection->ifs_lnode = lnode;
        return (ifsection);
}

/*#***********************************************************************\
 * 
 *              su_ifsection_done
 * 
 * Deletes a section racord
 * 
 * Parameters : 
 * 
 *      ifsection - in, take
 *              pointer to the section object
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void su_ifsection_done(su_ifsection_t* ifsection)
{
        ss_dassert(ifsection != NULL);
        ss_dassert(ifsection->ifs_keylines != NULL); 
        su_rbt_done(ifsection->ifs_keylines);
        SsMemFree(ifsection);
}


/*#***********************************************************************\
 * 
 *              su_ifline_done
 * 
 * Deletes a line data from the line list
 * 
 * Parameters : 
 * 
 *      lineptr - in, take
 *              line pointer
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void su_ifline_done(void* lineptr)
{
        ss_dassert(lineptr != NULL);
        SsMemFree(lineptr);
}

/*#***********************************************************************\
 * 
 *              ifsection_cmp
 * 
 * Common routine for ifsection insert/search cmp
 * 
 * Parameters : 
 * 
 *      p1 - in, use
 *              first section name string
 *              
 *      p2 - in, use
 *              second section name string
 *              
 * Return value :
 *      lexical string difference as in strcmp()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static int ifsection_cmp(uchar* p1, uchar* p2)
{
        int cmp = 0;

        for (;; p1++) {
            switch (*p1) {
                case '[':
                    p1++;
                    break;
                case ' ':
                case '\t':
                    continue;
                default:
                    break;
            }
            break;
        }
        for (;; p2++) {
            switch (*p2) {
                case '[':
                    p2++;
                    break;
                case ' ':
                case '\t':
                    continue;
                default:
                    break;
            }
            break;
        }
        for (;; p1++, p2++) {
            switch (*p2) {
                case ']':
                case '\n':
                case '\0':
                    if (*p1 == ']' || *p1 == '\n' || *p1 == '\0') {
                        return (0);
                    }
                    return (1);
                default:
                    switch (*p1) {
                        case ']':
                        case '\n':
                        case '\0':
                            return (-1);
                    }
                    cmp = ss_toupper(*p1) - ss_toupper(*p2);
                    break;
            }
            if (cmp != 0) {
                break;
            }
        }
        return (cmp);
}

/*#***********************************************************************\
 * 
 *              su_ifsection_cmp
 * 
 * Compares two section records in su_rbt_insert() for a section
 * 
 * Parameters : 
 * 
 *      key1 - in, use
 *              pointer to 1st section record
 *              
 *      key2 - in, use
 *              pointer to 2nd section record
 *              
 * Return value :
 *      == 0 if section names equal
 *      >  0 if key1 > key2 or
 *      <  0 otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static int su_ifsection_cmp(void* key1, void* key2)
{
        uchar* p1 = su_listnode_getdata(((su_ifsection_t*)key1)->ifs_lnode);
        uchar* p2 = su_listnode_getdata(((su_ifsection_t*)key2)->ifs_lnode);
        int cmp = ifsection_cmp(p1, p2);
        return (cmp);
}

/*#***********************************************************************\
 * 
 *              su_ifsection_cmp2
 * 
 * Compares a section name from record and a section name in
 * su_rbt_search() for section name
 * 
 * Parameters : 
 * 
 *      key - in, use
 *              section name
 *              
 *      datum - in, use
 *              section record
 *              
 * Return value : 
 *      == 0 if section names equal
 *      >  0 if key > datum or
 *      <  0 otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static int su_ifsection_cmp2(void* key, void* datum)
{
        uchar* p1 = key;
        uchar* p2 = su_listnode_getdata(((su_ifsection_t*)datum)->ifs_lnode);
        int cmp = ifsection_cmp(p1, p2);
        return (cmp);
}


static su_ifkeyline_t* su_ifkeyline_init(su_list_node_t* lnode)
{
        su_ifkeyline_t* keyline;
        uchar* line;
        uchar* lp;

        keyline = SSMEM_NEW(su_ifkeyline_t);
        keyline->ifkl_lnode = lnode;
        for (lp = line = su_listnode_getdata(lnode); ; lp++) {
            switch (*lp) {
                case ' ':
                case '\t':
                    continue;
                default:
                    break;
            }
            break;
        }
        keyline->ifkl_keystart = lp;
        for (; ; lp++) {
            switch (*lp) {
                case '\0':
                    ss_error;
                case '=':
                    break;
                default:
                    continue;
            }
            break;
        }
        for (; --lp >= line; ) {
            switch (*lp) {
                case ' ':
                case '\t':
                    continue;
                default:
                    break;
            }
            break;
        }
        ss_dassert(lp >= line);
        keyline->ifkl_keyend = lp;
        return (keyline);
}

static void su_ifkeyline_done(su_ifkeyline_t* keyline)
{
        ss_dassert(keyline != NULL);
        SsMemFree(keyline);
}
/*#***********************************************************************\
 * 
 *              su_ifkeyline_cmp
 * 
 * Compares keynames of two keyname=value lines. Used in su_rbt_insert()
 * for key lines
 * 
 * Parameters : 
 * 
 *      key1 - in, use
 *              pointer to 1st keyline node
 *              
 *      key2 - in, use
 *              pointer to 2nd leyline node
 *              
 * Return value :
 *      == 0 if key1 == key2
 *       > 0 if key1 > key2
 *       < 0 otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static int su_ifkeyline_cmp(void* key1, void* key2)
{
        su_ifkeyline_t* kl1 = key1;
        su_ifkeyline_t* kl2 = key2; 
        uchar* p1_end = kl1->ifkl_keyend;
        uchar* p2_end = kl2->ifkl_keyend;
        uchar* p1 = kl1->ifkl_keystart;
        uchar* p2 = kl2->ifkl_keystart;
        int cmp = 0;

        for (;; p1++, p2++) {
            if (p1 > p1_end) {
                if (p2 > p2_end) {
                    return (cmp);
                }
                return (-1);
            }
            if (p2 > p2_end) {
                return (1);
            }
            cmp = ss_toupper(*p1) - ss_toupper(*p2);
            if (cmp != 0) {
                break;
            }
        }
        return (cmp);
}

/*#***********************************************************************\
 * 
 *              su_ifkeyline_cmp2
 * 
 * Compares a linelist node and a keyname
 * 
 * Parameters : 
 * 
 *      key - in, use
 *              key name
 *              
 *      datum - in, use
 *              linelist node containing keyname=value
 *              
 * Return value :
 *      == 0 if equal
 *       > 0 if key > datum
 *       < 0 otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static int su_ifkeyline_cmp2(void* key, void* datum)
{
        su_ifkeyline_t* kl = datum;
        uchar* p1 = key;
        uchar* p2 = kl->ifkl_keystart;
        uchar* p2_end = kl->ifkl_keyend;
        int cmp = 0;

        for (;; p1++, p2++) {
            if (p2 > p2_end) {
                if (*p1 == '\0') {
                    return (cmp);
                } else {
                    return (1);
                }
            }
            cmp = ss_toupper(*p1) - ss_toupper(*p2);
            if (cmp != 0) {
                break;
            }
        }
        return (cmp);
}


/*#***********************************************************************\
 * 
 *              su_ifline_type
 * 
 * 
 * 
 * Parameters : 
 * 
 *      line - in, use
 *              
 *              
 * Return value : 
 *      SU_IFLINE_SECTION if the line is: [section_name]
 *      SU_IFLINE_KEYLINE if the line is: keyname=value
 *      SU_IFLINE_COMMENT if the line is: ; Comment or illegal
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static su_iflinetype_t su_ifline_type(char *line)
{
#if 1
        su_iflinetype_t t = su_ifline_type_strict(line);
        if (t == SU_IFLINE_ILLEGAL) {
            return SU_IFLINE_COMMENT;
        }
        return t;
#else /* old code retained if new one fails */
        su_iflinetype_t t = SU_IFLINE_UNKNOWN;

        for (;; line++) {
            switch (*line) {
                case '[':
                    if (t != SU_IFLINE_UNKNOWN) {
                        return (SU_IFLINE_COMMENT);
                    }
                    t = SU_IFLINE_SECTION;
                    break;
                case ']':
                    if (t != SU_IFLINE_SECTION) {
                        return (SU_IFLINE_COMMENT);
                    }
                    return (t);
                case '=':
                    if (t == SU_IFLINE_KEYLINE) {
                        return (t);
                    }
                    /* fall to next case */
                case INIFILE_COMMENTCHAR:
                case '\n':
                case '\0':
                    return (SU_IFLINE_COMMENT);
                case ' ':
                case '\t':
                    break;
                default:
                    if (t == SU_IFLINE_UNKNOWN) {
                        t = SU_IFLINE_KEYLINE;
                    }
                    break;
            }
        }
#endif /* out commented */
}

/*#***********************************************************************\
 * 
 *              su_ifline_type_strict
 * 
 * 
 * 
 * Parameters : 
 * 
 *      line - in, use
 *              
 *              
 * Return value : 
 *      SU_IFLINE_SECTION if the line is: [section_name]
 *      SU_IFLINE_KEYLINE if the line is: keyname=value
 *      SU_IFLINE_COMMENT if the line is: ; Comment
 *      SU_IFLINE_ILLEGAL if the line is: illegal
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static su_iflinetype_t su_ifline_type_strict(char *line)
{
        su_iflinetype_t t = SU_IFLINE_UNKNOWN;

        for (;; line++) {
            switch (*line) {
                case '[':
                    if (t != SU_IFLINE_UNKNOWN) {
                        return (SU_IFLINE_ILLEGAL);
                    }
                    t = SU_IFLINE_SECTION;
                    break;
                case ']':
                    if (t != SU_IFLINE_SECTION) {
                        return (SU_IFLINE_ILLEGAL);
                    }
                    return (t);
                case '=':
                    if (t == SU_IFLINE_KEYLINE) {
                        return (t);
                    }
                    return SU_IFLINE_ILLEGAL;
                case INIFILE_COMMENTCHAR:
                case '\n':
                case '\0':
                    if (t != SU_IFLINE_UNKNOWN) {
                        return SU_IFLINE_ILLEGAL;
                    }
                    return (SU_IFLINE_COMMENT);
                case ' ':
                case '\t':
                    break;
                default:
                    if (t == SU_IFLINE_UNKNOWN) {
                        t = SU_IFLINE_KEYLINE;
                    }
                    break;
            }
        }
}

static void inifile_fix_cr_lf(char* line, size_t* p_linelen)
{
        if (*p_linelen > 1 &&
            line[*p_linelen-2] == '\r' &&
            line[*p_linelen-1] == '\n')
        {
            line[*p_linelen-2] = '\n';
            line[*p_linelen-1] = '\0';
            (*p_linelen)--;
        }
}
/*#***********************************************************************\
 * 
 *              inifile_fillfromfile
 * 
 * Reads the file and fills the internal data structures with found 
 * information.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *          valid inifile       
 *              
 * Return value : 
 *      TRUE, file was found and successfully read
 *      FALSE, file was not found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool inifile_fillfromfile(
                su_inifile_t* inifile,
                char* fname,
                bool searchp)
{
        SS_FILE*           fp;
        su_ifsection_t* section = NULL;
        char*           linebuf;
        char*           line;
        su_list_node_t* listnode;
        su_ifkeyline_t* keyline;
        su_rbt_node_t*  rbt_node;

        IF_CHECK(inifile);
        inifile->if_registry = FALSE;
        if (searchp) {
            inifile->if_fname = su_inifile_search(fname);
            if (inifile->if_fname == NULL) {
                inifile->if_fname = inifile_gennewinifilename(fname);
            }
        } else {
            inifile->if_fname = SsMemStrdup(fname);
        }
        fp = SsFOpenT(inifile->if_fname, (char *)"r");
        if (fp == NULL) {
            ss_dprintf_1((
            "inifile_fillfromfile: cannot open file %s for reading",
            inifile->if_fname));                     
            return (FALSE);
        }
        linebuf = SsMemAlloc(2048);
        while (SsFGets(linebuf, READ_MAXLINELEN, fp) != NULL) {
            size_t linebuflen;
            size_t linelen;
            linebuflen = strlen(linebuf);
            inifile_fix_cr_lf(linebuf, &linebuflen);
            line = SsMemStrdup(linebuf);
            /* If last character before newline is backslash,
               join to the next line. */
            while (linebuflen > 1 && linebuf[linebuflen - 2] == '\\' &&
                   SsFGets(linebuf, READ_MAXLINELEN, fp) != NULL)
            {
                linebuflen = strlen(linebuf);
                linelen = strlen(line);
                inifile_fix_cr_lf(linebuf, &linebuflen); 
                /* Overwrite backslash at the end. */
                line[linelen - 2] = '\0';
                /* Reallocate space for linebuf. */
                line = SsMemRealloc(line, linelen + linebuflen + 1);
                /* Append linebuf at the end. */
                strcat(line, linebuf);
            }
            ss_dprintf_1(("su_inifile_fillfromfile: line=%s\n",
                          line));
            listnode = su_list_insertlast(inifile->if_linelist, line);
            switch (su_ifline_type(line)) {
                case SU_IFLINE_SECTION: /* [SECTION NAME] */
                    rbt_node = su_rbt_search(inifile->if_sections, line);
                    if (rbt_node == NULL) {
                        section = su_ifsection_init(listnode);
                        su_rbt_insert(inifile->if_sections, section);
                    } else {
                        section = su_rbtnode_getkey(rbt_node);
                    }
                    break;
                case SU_IFLINE_KEYLINE: /* keyname=value */
                    if (section == NULL) {
                        /* Should warn */
                        break;
                    }
                    /* To make last declaration be the one that's used:

                      rbt_node = su_rbt_search(section->ifs_keylines, line);
                      if (rbt_node != NULL) {
                          su_rbt_delete(section->ifs_keylines, rbt_node);
                      }
                    */
                    keyline = su_ifkeyline_init(listnode);
                    su_rbt_insert(section->ifs_keylines, keyline);
                    break;
                case SU_IFLINE_COMMENT: /* ; Comment (or illegal line) */
                    break;
                default:
                    ss_error;
            }
        }
        SsFClose(fp);
        SsMemFree(linebuf);
        return (TRUE);
}

/*#***********************************************************************\
 * 
 *              inifile_fillfrommemory
 * 
 * Reads the string (in-memory inifile) and fills the internal data 
 * structures with found 
 * information.
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *          valid inifile       
 *              
 * Return value : 
 *      TRUE, file was found and successfully read
 *      FALSE, file was not found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool inifile_fillfrommemory(
                su_inifile_t* inifile,
                char* fname,
                char* inibuffer)
{
        su_ifsection_t* section = NULL;
        char*           linebuf;
        char*           line;
        su_list_node_t* listnode;
        su_ifkeyline_t* keyline;
        su_rbt_node_t*  rbt_node;
        int             i;

        IF_CHECK(inifile);
        inifile->if_registry = FALSE;
        inifile->if_fname = SsMemStrdup(fname);

        linebuf = SsMemAlloc(2048);
        while (*inibuffer != '\0') {
            size_t linebuflen;
            size_t linelen;

            for (i = 0; i < READ_MAXLINELEN; i++) {
                if (*inibuffer == '\0') {
                    break;
                }
                linebuf[i] = *inibuffer++;
                if (linebuf[i] == '\n') {
                    break;
                }
            }
            linebuf[i] = '\0';
            if (i>1 && linebuf[i-1]=='\r'){
                linebuf[i-1] = '\0';
            }

            line = SsMemStrdup(linebuf);
            linebuflen = strlen(linebuf);
            /* If last character before newline is backslash,
               join to the next line. */
            while (linebuflen >= 1 && linebuf[linebuflen - 1] == '\\') {

                for (i = 0; i < READ_MAXLINELEN; i++) {
                    if (*inibuffer == '\0') {
                        break;
                    }
                    linebuf[i] = *inibuffer++;
                    if (linebuf[i] == '\n') {
                        break;
                    }
                }
                linebuf[i] = '\0';
                if (i>1 && linebuf[i-1]=='\r'){
                    linebuf[i-1] = '\0';
                }
                
                linebuflen = strlen(linebuf);
                linelen = strlen(line);
                /* Overwrite backslash at the end. */
                line[linelen - 1] = '\0';
                linelen--;
                /* Reallocate space for linebuf. */
                line = SsMemRealloc(line, linelen + linebuflen + 1);
                /* Append linebuf at the end. */
                strcat(line, linebuf);
            }
            listnode = su_list_insertlast(inifile->if_linelist, line);
            switch (su_ifline_type(line)) {
                case SU_IFLINE_SECTION: /* [SECTION NAME] */
                    rbt_node = su_rbt_search(inifile->if_sections, line);
                    if (rbt_node == NULL) {
                        section = su_ifsection_init(listnode);
                        su_rbt_insert(inifile->if_sections, section);
                    } else {
                        section = su_rbtnode_getkey(rbt_node);
                    }
                    break;
                case SU_IFLINE_KEYLINE: /* keyname=value */
                    if (section == NULL) {
                        break;
                    }
                    keyline = su_ifkeyline_init(listnode);
                    su_rbt_insert(section->ifs_keylines, keyline);
                    break;
                case SU_IFLINE_COMMENT: /* ; Comment (or illegal line) */
                    break;
                default:
                    ss_error;
            }
        }
        SsMemFree(linebuf);
        return (TRUE);
}

/*#***********************************************************************\
 * 
 *              inifile_fillfromregistry
 * 
 * Reads the information from given registry and fills the inifile
 * object with that data.
 * 
 * For more information, see the comment in su_reg_fillinifile
 * 
 * Parameters : 
 * 
 *      inifile - in, use
 *          the inifile to be filled with data found in registry                
 *              
 *      regname - in, use
 *              W32: Full path to registry, for instance "software\odbc\odbc.ini" etc           
 *              W16: "odbc.ini" (the windir is added here)
 *              
 *      regrootkey - in, use
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
#ifndef SS_MYSQL
static bool inifile_fillfromregistry(
            su_inifile_t*       inifile,
            char*               regname,
            su_inifile_regkey_t regrootkey)
{
#ifdef SS_WIN
        uint len;
        char windir[144 + 1];
        char tmpname[255];
        bool found;

        len = GetWindowsDirectory(windir, sizeof(windir));
        ss_assert(len < sizeof(windir));
        IF_CHECK(inifile);
        SsSprintf(tmpname, "%s\\%s", windir, regname);
        ss_assert(strlen(windir) + strlen(regname) + 1 < sizeof(tmpname));
        found = inifile_fillfromfile(inifile, tmpname, FALSE);
        inifile->if_registry = TRUE;
        inifile->if_regrootkey = regrootkey;
        return(found);
#else
        IF_CHECK(inifile);
        inifile->if_registry = TRUE;
        inifile->if_fname = SsMemStrdup(regname);
        inifile->if_regrootkey = regrootkey;
        return(su_reg_fillinifile(regname, regrootkey, inifile));
#endif
}
#endif /* !SS_MYSQL */


#ifdef SS_DEBUG
void su_inifile_print(
        su_inifile_t* inifile)
{
        char* line;
        su_list_node_t* list_node;

        SsSemEnter(inifile->if_mutex);
        su_list_do_get(inifile->if_linelist, list_node, line) {
            while (strlen(line) > SAVE_MAXLINELEN) {
                char buf[SAVE_MAXLINELEN + 10];
                if (strlen(line) == SAVE_MAXLINELEN + 1
                    && line[SAVE_MAXLINELEN] == '\n') {
                    break;
                }
                strncpy(buf, line, SAVE_MAXLINELEN);
                buf[SAVE_MAXLINELEN] = '\0';
                strcat(buf, "\\\n");
                SsDbgPrintf("%s", buf);
                line += SAVE_MAXLINELEN;
            }
            SsDbgPrintf("%s", line);
        }
        SsSemExit(inifile->if_mutex);
}
#endif /* SS_DEBUG */
