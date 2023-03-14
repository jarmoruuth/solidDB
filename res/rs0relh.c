/*************************************************************************\
**  source       * rs0relh.c
**  directory    * res
**  description  * Relation handle functions
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
This module contains the implementation of relation handle object.
A relation handle consists of a relation name, tuple type, check string,
defaultvalue and an array of keys. The operations include methods for
setting and returning the components of a relation handle.

Limitations:
-----------
The methods for returning the number of tuples in a relation and a number
of bytes in a relation always return a default value.

Canreverse returns always POSSIBLY.

Error handling:
--------------
Methods returns FALSE or dassert. No real error handling.

Objects used:
------------
Tuple type object   <rs0ttype.h>
Tuple value object  <rs0tval.h>
Key object          <rs0key.h>

Preconditions:
-------------
None

Multithread considerations:
--------------------------
Code is fully re-entrant.
The same relh object can not be used simultaneously from many threads.


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define RS0RELH_C

#include <ssc.h>
#include <ssstring.h>
#include <sssprint.h>
#include <sslimits.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>

#include <uti0vcmp.h>

#include <su0parr.h>
#include <su0bsrch.h>
#include <su0gate.h>
#include <su0slike.h>
#include <su0wlike.h>

#define RS_INTERNAL

#include "rs0types.h"
#include "rs0sdefs.h"
#include "rs0atype.h"
#include "rs0ttype.h"
#include "rs0tval.h"
#include "rs0key.h"
#include "rs0rbuf.h"
#include "rs0auth.h"
#include "rs0sysi.h"
#include "rs0cons.h"
#include "rs0cardi.h"
#include "rs0entna.h"

#ifndef SS_MYSQL
#include "rs0hcol.h"
#endif /* !SS_MYSQL */

#include "rs0relh.h"


/* #include <sqlint.h> */    /* SQL_REVERSE */

#include <sqlest.h>

#define RELH_SAMPLE_CLEARPERC   20
#define RELH_SAMPLE_CHECKCTR    2000

#define RELH_REPLAN_CLEARPERC   20
#define RELH_REPLAN_CHECKCTR    2000

#ifndef SS_NOESTSAMPLES

typedef struct {
        dynva_t as_dva;
        int     as_eqrowestimate;
} relh_attrsamples_t;

/*#***********************************************************************\
 * 
 *              relh_mustclearsamples
 * 
 * Checks is sample clear is necessary.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
static bool relh_mustclearsamples(
        void*      cd __attribute__ ((unused)),
        rs_relh_t* relh)
{
        ss_int8_t dummy1, dummy2;

        if (relh->rh_samplechangectr >= RELH_SAMPLE_CHECKCTR) {
            /* Can't reset samplechangectr here, because this function is
               called again after going to the sample mutex. */
            SsInt8Set0(&dummy1);
            if (SsInt8Cmp(relh->rh_samplentuples, dummy1) > 0) {
                SsInt8SetUint4(&dummy2, relh->rh_samplechange);
                SsInt8MultiplyByInt2(&dummy2, dummy2, 100);
                SsInt8DivideByInt8(&dummy1, dummy2, relh->rh_samplentuples);
                SsInt8SetUint4(&dummy2, RELH_SAMPLE_CLEARPERC);
                if (SsInt8Cmp(dummy1, dummy2) > 0
                    || (ulong)relh->rh_samplechange >= 3000000000UL) {
                    /* More than limit percent of tuples or at least
                       three billion tuples have changed since
                       last sample update. */
                    return(TRUE);
                }
            }
        }
        return(FALSE);
}

/*##**********************************************************************\
 * 
 *              rs_relh_clearsamples_nomutex
 * 
 * Clears all attribute sample information. Does not enter sample mutex.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
void rs_relh_clearsamples_nomutex(
        void* cd,
        rs_relh_t* relh)
{
        uint index;
        uint samplesize;
        relh_attrsamples_t* samples;

        ss_dprintf_3(("rs_relh_clearsamples_nomutex:%s, relh=%ld\n", rs_entname_getname(relh->rh_name), (long)relh));
        CHECK_RELH(relh);
        SS_NOTUSED(cd);

        su_pa_do_get(relh->rh_attrsamples, index, samples) {
            uint i;
            samplesize = (uint)su_pa_getdata(relh->rh_samplesizes, index);
            for (i = 0; i < samplesize; i++) {
                dynva_free(&samples[i].as_dva);
            }
            SsMemFree(samples);
        }
        su_pa_removeall(relh->rh_attrsamples);
        su_pa_removeall(relh->rh_samplesizes);
        su_pa_removeall(relh->rh_samplediffs);
        su_pa_removeall(relh->rh_eqrowestimate);
        su_pa_removeall(relh->rh_samplenulls);
        su_pa_removeall(relh->rh_maxdiffrows);

        relh->rh_samplechange = 0;
        SsInt8Set0(&(relh->rh_samplentuples));
        relh->rh_samplefailed = FALSE;
        relh->rh_issamples = FALSE;
        relh->rh_mustrefreshsamples = FALSE;
}

/*#***********************************************************************\
 * 
 *              relh_clearsamples
 * 
 * Clears all attribute sample information. Enters to the sample mutex.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
static void relh_clearsamples(
        void* cd,
        rs_relh_t* relh,
        bool force)
{
        SS_NOTUSED(cd);

        SsSemEnter(relh->rh_samplesem);

        /* Do duplicate check inside mutex for the need of sample clear.
         * Just in case if the sample update was active at the same time.
         */
        if (force || relh_mustclearsamples(cd, relh)) {
            rs_relh_clearsamples_nomutex(cd, relh);
        }

        SsSemExit(relh->rh_samplesem);
}

/*#***********************************************************************\
 * 
 *              relh_clearsamplesif
 * 
 * Checks if attribute sample information should be cleared so that
 * the information will be updated. If there has been certain percentage
 * of changes in the table, the old information will removed.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
static bool relh_clearsamplesif(
        rs_sysi_t* cd,
        rs_relh_t* relh)
{
        if (relh_mustclearsamples(cd, relh)) {
            /* More than limit percent of tuples has changed since
             * last sample update.
             */
            rs_sqlinfo_t* sqli;
            sqli = rs_sysi_sqlinfo(cd);
            if (rs_sqli_getestsamplemaxeqrowest(sqli) == 0) {
                /* Use old style sample clear. */
                relh_clearsamples(cd, relh, FALSE);
            } else {
                relh->rh_mustrefreshsamples = TRUE;
                relh->rh_samplechange = 0;
                relh->rh_samplechangectr = 0;
            }
            return TRUE;
        }
        if (relh->rh_samplechangectr >= RELH_SAMPLE_CHECKCTR) {
            relh->rh_samplechangectr = 0;
        }

        return FALSE;
}

bool rs_relh_mustrefreshsamples(
        void*      cd,
        rs_relh_t* relh)
{
        bool mustrefreshsamples;

        mustrefreshsamples = relh->rh_mustrefreshsamples;
        relh->rh_mustrefreshsamples = FALSE;

        return(mustrefreshsamples);
}

/*#***********************************************************************\
 *
 *      relh_mustreplan
 *
 * Tells if the searches to this table should be replanned, because the
 * contents of the table has changed enough since the last planning.
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      relh - <usage>
 *          <description>
 *
 * Return value:
 *       <description>
 *
 * Limitations:
 *
 * Globals used:
 */
static bool relh_mustreplan(
        void*      cd,
        rs_relh_t* relh)
{
        ss_int8_t dummy1, dummy2;

        if (relh->rh_replanchangectr >= RELH_REPLAN_CHECKCTR) {
            relh->rh_replanchangectr = 0;
            
            SsInt8SetUint4(&dummy2, relh->rh_replanchange);
            SsInt8MultiplyByInt2(&dummy2, dummy2, 100);
            if (SsInt8Is0(relh->rh_replanntuples)) {
                relh->rh_replanntuples = SsInt8InitFrom2Uint4s(0, 10);
            }
            SsInt8DivideByInt8(&dummy1, dummy2, relh->rh_replanntuples);
            SsInt8SetUint4(&dummy2, RELH_REPLAN_CLEARPERC);
            if (SsInt8Cmp(dummy1, dummy2) > 0
                || relh->rh_replanchange >= 3000000000UL) {
                relh->rh_replanntuples
                    = rs_cardin_ntuples(cd, relh->rh_cardin);
                relh->rh_replanchange = 0;
                
                return TRUE;
            }
        }
        return FALSE;
}
#endif /* SS_NOESTSAMPLES */

/*##**********************************************************************\
 * 
 *              rs_relh_init
 * 
 * Creates a virtual handle for a non-existent (new) relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relname - in, use
 *              name of the relation
 *
 *      relid - in
 *              relation id
 *
 *      ttype - in, use
 *              tuple type of the relation
 *
 * Return value - give : 
 * 
 *      Pointer to a virtual relation handle 
 * 
 * Limitations  : 
 * 
 *      Should be investigated if could be combined with rs_relh_create.
 * 
 * Globals used : 
 */
rs_relh_t* rs_relh_init(cd, relname, relid, ttype)
        void*           cd;
        rs_entname_t*   relname;
        ulong           relid;
        rs_ttype_t*     ttype;
{
        rs_relh_t* relh;
        uint       n_attrs, i;

        ss_dprintf_1(("%s: rs_relh_init\n", __FILE__));
        ss_dassert(relname != NULL);
        ss_dassert(rs_entname_getname(relname) != NULL);
        ss_dassert(rs_entname_getschema(relname) != NULL);
        ss_dassert(ttype != NULL);

        relh = SSMEM_NEW(rs_relh_t);

        relh->rh_check = RSCHK_RELHTYPE;
        relh->rh_nlink = 1;
        relh->rh_name = rs_entname_copy(relname);
        relh->rh_relid = relid;
        relh->rh_ttype = rs_ttype_copy(cd, ttype);
        relh->rh_aborted = FALSE;
        relh->rh_ddopactive = 0;
        relh->rh_readonly = 0;
        relh->rh_key_pa = su_pa_init();
        relh->rh_refkey_pa = su_pa_init();
        relh->rh_clusterkey = NULL;
        relh->rh_sqlcheckstrs = NULL;
        relh->rh_sqlchecknames = NULL;
        relh->rh_defarr = NULL;
        relh->rh_defvalue = NULL;
        relh->rh_cardin = rs_cardin_init(cd, relid);
        relh->rh_reltype = RS_RELTYPE_OPTIMISTIC;
        relh->rh_istransient = FALSE;
        relh->rh_isglobaltemporary = FALSE;
        relh->rh_pessgate = NULL;
        relh->rh_pesschangecount = 0;
        relh->rh_nocheck = FALSE;
        relh->rh_triggers = su_pa_init();
        relh->rh_basetablep = TRUE;
        relh->rh_historytablep = FALSE;
#ifndef SS_HSBG2
        relh->rh_rowlevelhsbp = FALSE;
#endif /* !SS_HSBG2 */

        relh->rh_syncp = FALSE;
#ifdef SS_SYNC
        relh->rh_syncrelh = NULL;
#ifndef SS_MYSQL
        relh->rh_hcol = rs_hcol_init();
#endif /* !SS_MYSQL */

#endif /* SS_SYNC */
#ifndef SS_NOESTSAMPLES
        relh->rh_attrsamples = su_pa_init();
        relh->rh_samplesizes = su_pa_init();
        relh->rh_samplediffs = su_pa_init();
        relh->rh_eqrowestimate = su_pa_init();
        relh->rh_samplenulls = su_pa_init();
        relh->rh_maxdiffrows = su_pa_init();
        relh->rh_samplesem = SsSemCreateLocal(SS_SEMNUM_RES_RELHSAMPLE);
        relh->rh_samplechange = 0;
        relh->rh_samplechangectr = 0;
        SsInt8Set0(&(relh->rh_samplentuples));
        relh->rh_samplefailed = FALSE;
        relh->rh_issamples = FALSE;
        relh->rh_mustrefreshsamples = FALSE;
        relh->rh_replanchange = 0;
        relh->rh_replanchangectr = 0;
        SsInt8Set0(&(relh->rh_replanntuples));
#endif /* SS_NOESTSAMPLES */

        relh->rh_rowcheckcolname = NULL;
        if (strcmp(rs_entname_getschema(relname), RS_AVAL_SYSNAME) == 0) {
            char* name;
            name = rs_entname_getname(relname);
            if (strcmp(name, RS_RELNAME_TABLES) == 0) {
                relh->rh_rowcheckcolname = (char *)RS_ANAME_TABLES_ID;
            } else if (strcmp(name, RS_RELNAME_COLUMNS) == 0) {
                relh->rh_rowcheckcolname = (char *)RS_ANAME_COLUMNS_REL_ID;
            } else if (strcmp(name, RS_RELNAME_PROCEDURES) == 0) {
                relh->rh_rowcheckcolname = (char *)RS_ANAME_PROCEDURES_ID;
            } else if (strcmp(name, RS_RELNAME_TRIGGERS) == 0) {
                relh->rh_rowcheckcolname = (char *)RS_ANAME_TRIGGERS_ID;
            }
        }
        ss_debug(rs_cardin_setcheck(relh->rh_cardin, relname));

#ifdef SS_MME
        relh->rh_differentiatingano = RS_ANO_NULL;
#endif
        relh->rh_sem = rs_sysi_getrslinksem(cd);

#ifndef NOT_NOTNULL_ANOARRAY
        relh->rh_notnull_anoarray = NULL;
#endif /* NOT_NOTNULL_ANOARRAY */

        relh->rh_isautoincinited = FALSE;
        relh->rh_autoincseqid = 0;

        n_attrs = rs_ttype_nattrs(cd, ttype);

        for(i = 0; i < n_attrs; i++) {
            rs_atype_t* atype;

            atype = rs_ttype_atype(cd, ttype, i);

            if (rs_atype_autoinc(cd, atype)) {
                relh->rh_isautoincinited = TRUE;

                relh->rh_autoincseqid = rs_atype_getautoincseqid(cd, atype);

                ss_pprintf_1(("Auto increment sequence id = %ld", relh->rh_autoincseqid));
                
                break;
            }
        }
        
        ss_debug(if (ss_mainmem && (relid == 0 || !rs_relh_issysrel(cd, relh))) relh->rh_reltype = RS_RELTYPE_MAINMEMORY);

        return(relh);
}

/*##**********************************************************************\
 * 
 *              rs_relh_done
 * 
 * Frees a relation handle allocated by rs_relh_init.
 * Should be investigated how to combine with rs_relh_free.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, take
 *              relation handle
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_done(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        ss_dprintf_1(("%s: rs_relh_done\n", __FILE__));
        CHECK_RELH(relh);
        ss_dassert(relh->rh_name != NULL);
        ss_dassert(relh->rh_ttype != NULL);

        SsSemEnter(relh->rh_sem);

        ss_dassert(relh->rh_nlink > 0);

        relh->rh_nlink--;

        if (relh->rh_nlink == 0) {
            /* Do the actual free.
             */
            uint index;
            rs_key_t* key;
            relh_trig_t* trig;
            
            SsSemExit(relh->rh_sem);

            if (relh->rh_pessgate != NULL) {
                su_gate_done(relh->rh_pessgate);
            }
            rs_cardin_done(cd, relh->rh_cardin);

            ss_dprintf_2(("%s: rs_relh_done:Do the actual free\n", __FILE__));

#ifndef SS_NOESTSAMPLES
            rs_relh_clearsamples_nomutex(cd, relh);

            su_pa_done(relh->rh_attrsamples);
            su_pa_done(relh->rh_samplesizes);
            su_pa_done(relh->rh_samplediffs);
            su_pa_done(relh->rh_eqrowestimate);
            su_pa_done(relh->rh_samplenulls);
            su_pa_done(relh->rh_maxdiffrows);

            SsSemFree(relh->rh_samplesem);
#endif /* SS_NOESTSAMPLES */

            rs_entname_done(relh->rh_name);

            if (relh->rh_defarr != NULL) {
                /* rh_def things has to be freed before rh_ttype,
                   see rs_tval_free() below.
                */
                SsMemFree(relh->rh_defarr);
                relh->rh_defarr = NULL;
                ss_dassert(relh->rh_defvalue != NULL);
                rs_tval_free(cd, relh->rh_ttype, relh->rh_defvalue);
            }

            rs_ttype_setreadonly(cd, relh->rh_ttype, FALSE);
            rs_ttype_free(cd, relh->rh_ttype);
            if (relh->rh_sqlcheckstrs != NULL) {
                int i=0;
                ss_dassert(relh->rh_sqlchecknames != NULL);
                while (relh->rh_sqlcheckstrs[i] != NULL) {
                    SsMemFree(relh->rh_sqlcheckstrs[i]);
                    if (relh->rh_sqlchecknames[i] != NULL) {
                        SsMemFree(relh->rh_sqlchecknames[i]);
                    }
                    i++;
                }
                SsMemFree(relh->rh_sqlcheckstrs);
                SsMemFree(relh->rh_sqlchecknames);
            }

            su_pa_do_get(relh->rh_key_pa, index, key) {
                rs_key_done(cd, key);
            }
            su_pa_done(relh->rh_key_pa);

            su_pa_do_get(relh->rh_refkey_pa, index, key) {
                rs_key_done(cd, key);
            }
            su_pa_done(relh->rh_refkey_pa);

            su_pa_do_get(relh->rh_triggers, index, trig) {
                rs_entname_done(trig->t_name);
                SsMemFree(trig->t_str);
                SsMemFree(trig);
            }
            su_pa_done(relh->rh_triggers);

#ifdef SS_SYNC
            if (relh->rh_syncrelh != NULL) {
                SS_MEM_SETUNLINK(relh->rh_syncrelh);
                rs_relh_done(cd, relh->rh_syncrelh);
            }

#ifndef SS_MYSQL
            ss_dassert(relh->rh_hcol != NULL);
            rs_hcol_done(relh->rh_hcol);
#endif /* !SS_MYSQL */

#endif /* SS_SYNC */

#ifndef NOT_NOTNULL_ANOARRAY
            if (relh->rh_notnull_anoarray != NULL) {
                SsMemFree(relh->rh_notnull_anoarray);
            }
#endif /* NOT_NOTNULL_ANOARRAY */

            SsMemFree(relh);

        } else {

            SsSemExit(relh->rh_sem);
        }
}

/*##**********************************************************************\
 * 
 *              rs_relh_canreverse
 * 
 * Checks if reversing is possible with a specified relation handle.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value : 
 * 
 *      SQL_REVERSE_YES
 *      SQL_REVERSE_NO
 *      SQL_REVERSE_POSSIBLY
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint rs_relh_canreverse(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        ss_dprintf_1(("%s: Inside rs_relh_canreverse.\n", __FILE__));
        CHECK_RELH(relh);
        SS_NOTUSED(cd);
        SS_NOTUSED(relh);
        return(SQL_REVERSE_YES);
}

#ifndef rs_relh_ttype
/*##**********************************************************************\
 * 
 *              rs_relh_ttype
 * 
 * Describes a relation handle by returning a tuple type that corresponds to
 * the relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      rel - in, use
 *              relation handle
 * 
 * Return value - ref : 
 * 
 *      Pointer to tuple type that corresponds to the relation
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ttype_t* rs_relh_ttype(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        ss_dprintf_1(("%s: Inside rs_relh_ttype.\n", __FILE__));
        CHECK_RELH(relh);
        SS_NOTUSED(cd);
        ss_dassert(relh->rh_ttype != NULL);

        return _RS_RELH_TTYPE_(cd, relh);
}
#endif /* !defined(rs_relh_ttype) */



/*##**********************************************************************\
 * 
 *              rs_relh_checkstrings
 * 
 * Returns an SQL condition string that represents the CHECK constraints
 * on the relation
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value - ref : 
 * 
 *      !NULL, pointer into the SQL condition string.
 *      NULL, if no constraint is specified
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char** rs_relh_checkstrings(cd, rel, pnamearray)
        void*      cd;
        rs_relh_t* rel;
        char***    pnamearray;
{
        ss_dprintf_1(("%s: rs_relh_checkstring\n", __FILE__));
        CHECK_RELH(rel);
        SS_NOTUSED(cd);
        *pnamearray = rel->rh_sqlchecknames;
        return(rel->rh_sqlcheckstrs);
}

void rs_relh_setcatalog(
        void*      cd __attribute__ ((unused)),
        rs_relh_t* relh,
        char* newcatalog)
{
        rs_entname_t* new_en;
        
        CHECK_RELH(relh);
        new_en = rs_entname_init(newcatalog,
                                 rs_entname_getschema(relh->rh_name),
                                 rs_entname_getname(relh->rh_name));
        rs_entname_done(relh->rh_name);
        relh->rh_name = new_en;
}

bool rs_relh_isdlsysrel(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_issysrel\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        if (!rs_relh_issysrel(cd, relh)) {
            return FALSE;
        }

        if ((strcmp(rs_relh_name(cd, relh), "SYS_DL_REPLICA_CONFIG") == 0) ||
            (strcmp(rs_relh_name(cd, relh), "SYS_DL_REPLICA_DEFAULT") == 0)) {
            return(TRUE);
        }
        return (FALSE);
}

void rs_relh_setbasetable(
        void*      cd,
        rs_relh_t* relh,
        bool       basetablep)
{
        ss_dprintf_1(("%s: rs_relh_setbasetable\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        relh->rh_basetablep = basetablep;
}

void rs_relh_sethistorytable(
        void*      cd,
        rs_relh_t* relh,
        bool       historytablep)
{
        ss_dprintf_1(("%s: rs_relh_sethistorytable\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        relh->rh_historytablep = historytablep;
}

#ifndef SS_HSBG2
bool rs_relh_isrowlevelhsb(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_isrowlevelhsb\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_rowlevelhsbp);
}
#endif /* !SS_HSBG2 */

#ifndef SS_HSBG2
void rs_relh_setrowlevelhsb(
        void*      cd,
        rs_relh_t* relh,
        bool       rowlevelhsbp)
{
        ss_dprintf_1(("%s: rs_relh_setrowlevelhsb\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        relh->rh_rowlevelhsbp = rowlevelhsbp;
}
#endif /* !SS_HSBG2 */

#ifdef SS_SYNC

void rs_relh_setsync(
        void*      cd,
        rs_relh_t* relh,
        bool       syncp)
{
        ss_dprintf_1(("%s: rs_relh_issync\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        relh->rh_syncp = syncp;
}

bool rs_relh_insertsyncrelh(
        void*      cd,
        rs_relh_t* relh,
        rs_relh_t* sync_relh)
{
        bool succp;

        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_syncp);
        ss_dassert(relh->rh_syncrelh == NULL || relh->rh_syncrelh == sync_relh);

        SsSemEnter(relh->rh_sem);

        succp = (relh->rh_syncrelh == NULL);
        if (succp) {
            relh->rh_syncrelh = sync_relh;
        }

        SsSemExit(relh->rh_sem);

        return(succp);
}

rs_relh_t* rs_relh_getsyncrelh(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_syncp);

        return(relh->rh_syncrelh);
}

#ifndef SS_MYSQL
rs_hcol_t* rs_relh_gethcol(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_hcol);
}

void rs_relh_inserthcol(
        void*      cd,
        rs_relh_t* relh,
        rs_hcol_t* hcol)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_syncp);

        if (relh->rh_hcol != NULL) {
            rs_hcol_done(relh->rh_hcol);
        }

        relh->rh_hcol = hcol;
}
#endif /* !SS_MYSQL */

int rs_relh_sync_getsubscribe_count(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        /* ss_dassert(relh->rh_syncp); */

        return(rs_cardin_nsubscribers(cd, relh->rh_cardin));
}

void rs_relh_sync_addsubscribe_count(
        void*      cd,
        rs_relh_t* relh __attribute__ ((unused)),
        bool       addp __attribute__ ((unused)))
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        /* ss_dassert(relh->rh_syncp); */

#ifdef ENABLE_HISTORY_OPTIMIZATION

        rs_cardin_addsubscriber(cd, relh->rh_cardin, addp);

#else
#endif
}

#endif /* SS_SYNC */

#ifndef rs_relh_keys
/*##**********************************************************************\
 * 
 *              rs_relh_keys
 * 
 * Returns the keys of a relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value - ref : 
 * 
 *      Pointer to a su_pa_t array containing the keys
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_pa_t* rs_relh_keys(cd, relh)
        void*       cd;
        rs_relh_t*  relh;
{
        ss_dprintf_1(("%s: rs_relh_keys\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_key_pa);
        return(relh->rh_key_pa);
}

#endif /* rs_relh_keys */

#ifndef rs_relh_clusterkey
/*##**********************************************************************\
 * 
 *              rs_relh_clusterkey
 * 
 * Gets clustering key for a relation handle
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value - ref :
 *
 *      clustering key or
 *      NULL if no clustering key defined
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_key_t *rs_relh_clusterkey(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        CHECK_RELH(relh);

        return _RS_RELH_CLUSTERKEY_(cd, relh);
}
#endif /* !defined(rs_relh_clusterkey) */


#ifndef NOT_NOTNULL_ANOARRAY
/*##**********************************************************************\
 * 
 *              rs_relh_notnull_anoarray
 * 
 * Returns array of attribute numbers (=rs_ano_t) in relh 
 * which has NOT NULL property.
 * Array is terminated with RS_ANO_NULL. 
 * Array is never NULL but first element can be RS_ANO_NULL.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value - rs_ano_t*
 * 
 *      Array of attribute # which has NOT NULL property (pseudo attributes are not counted).
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_notnull_anoarray_create(
        rs_sysi_t*  cd,
        rs_relh_t*  relh)
{
        rs_ttype_t* ttype;
        uint        sql_attr_n;
        rs_ano_t i;
        rs_ano_t *anoarray;
        int pos;
        
        CHECK_RELH(relh);

        SS_PUSHNAME("rs_relh_notnull_anoarray");

        /* do not try to optimize alloc size: e.g. alloc space for all attributes. */
        ttype = rs_relh_ttype(cd, relh);
        sql_attr_n = rs_ttype_sql_nattrs(cd, ttype);
        ss_dassert(sql_attr_n > 0); /* This should be always true */
        ss_dprintf_1(("rs_relh_notnull_anoarray:alloc new array for %d attributes\n", (int)sql_attr_n));
        anoarray = SsMemAlloc((sql_attr_n+1)*sizeof(rs_ano_t));

        pos = 0; /* Current position in array */

        for (i = 0; i < (rs_ano_t)sql_attr_n; i++) {
            bool ispseudo;
            rs_atype_t* atype;

            atype = rs_ttype_sql_atype(cd, ttype, i);
            ispseudo = rs_atype_pseudo(cd, atype);

            if (!ispseudo && !rs_atype_nullallowed(cd, atype)) {
                /* This attr must be checked for not NULL.
                 * Add ano to array.
                 */
                anoarray[pos] = rs_ttype_sqlanotophys(cd, ttype, i);
                pos++;
            }
        }

        /* Terminate the array:
         * We always have an array: it is easier for caller to deal with this 
         */
        anoarray[pos] = RS_ANO_NULL; /* arrayhandling should check this endmark also! */

        relh->rh_notnull_anoarray = anoarray;

        SS_POPNAME;
}
#endif /* NOT_NOTNULL_ANOARRAY */


rs_key_t *rs_relh_search_clusterkey(cd, relh)
        rs_sysi_t*  cd __attribute__ ((unused));
        rs_relh_t*  relh;
{
        rs_ano_t i;
        rs_key_t *key;
        
        CHECK_RELH(relh);

        su_pa_do(relh->rh_key_pa, i) {
            key = su_pa_getdata(relh->rh_key_pa, i);
            if (rs_key_isclustering(cd, key)) {
                return (key);
            }
        }
        return (NULL);
}

/*##**********************************************************************\
 * 
 *              rs_relh_primkey
 * 
 * Gets primary key for a relation handle
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value - ref :
 *
 *      primary key or
 *      NULL if no primary key defined
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_key_t *rs_relh_primkey(cd, relh)
        void*      cd __attribute__ ((unused));
        rs_relh_t* relh;
{
        rs_ano_t i;
        rs_key_t *key;
        
        CHECK_RELH(relh);

        su_pa_do(relh->rh_key_pa, i) {
            key = su_pa_getdata(relh->rh_key_pa, i);
            if (rs_key_isprimary(cd, key)) {
                return (key);
            }
        }
        return (NULL);
}

/*##**********************************************************************\
 * 
 *              rs_relh_keybyname
 * 
 * Gets key for a relation handle by a key name.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 *      keyname - in, use
 *          key name
 * 
 * Return value - ref :
 * 
 *      key with name keyname or
 *      NULL if keyname not found
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_key_t *rs_relh_keybyname(cd, relh, keyname)
        void*           cd;
        rs_relh_t*      relh;
        rs_entname_t*   keyname;
{
        rs_ano_t i;
        rs_key_t *key;
        
        CHECK_RELH(relh);

        su_pa_do_get(relh->rh_key_pa, i, key) {
            if (strcmp(rs_key_name(cd, key), rs_entname_getname(keyname)) == 0) {
                return(key);
            }
        }
        return (NULL);
}

/*##**********************************************************************\
 * 
 *              rs_relh_takekeybyname
 * 
 * Gets key for a relation handle by a key name and removes it from relh.
 * 
 * NOTE! This is only for system table bug fixes to disable certain key
 * for system table updates.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      keyname - 
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
rs_key_t *rs_relh_takekeybyname(
        void*           cd,
        rs_relh_t*      relh,
        rs_entname_t*   keyname)
{
        rs_ano_t i;
        rs_key_t *key;
        
        CHECK_RELH(relh);

        su_pa_do_get(relh->rh_key_pa, i, key) {
            if (strcmp(rs_key_name(cd, key), rs_entname_getname(keyname)) == 0) {
                su_pa_remove(relh->rh_key_pa, i);
                return(key);
            }
        }
        return (NULL);
}

/*##**********************************************************************\
 * 
 *              rs_relh_keybyid
 * 
 * Gets key for a relation handle by a key id.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 *      keyid - in, use
 *          key id
 * 
 * Return value - ref :
 * 
 *      key with id keyid or
 *      NULL if keyid not found
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_key_t *rs_relh_keybyid(cd, relh, keyid)
        void*      cd __attribute__ ((unused));
        rs_relh_t* relh;
        ulong      keyid;
{
        rs_ano_t i;
        rs_key_t *key;
        
        CHECK_RELH(relh);

        su_pa_do_get(relh->rh_key_pa, i, key) {
            if (rs_key_id(cd, key) == keyid) {
                return(key);
            }
        }
        return (NULL);
}


/*##**********************************************************************\
 * 
 *              rs_relh_keyno
 * 
 * Gets key number for a key by a key id from the relation.
 * 
 * Parameters : 
 * 
 *      cd - in, unused
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 *      keyid - in, use
 *          key id
 * 
 * Return value - ref :
 * 
 *      key number
 *      -1 if key not found
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ano_t rs_relh_keyno(void* cd __attribute__ ((unused)),
                       rs_relh_t* relh,
                       ulong keyid)
{
        rs_ano_t i;
        rs_key_t *key;
        
        CHECK_RELH(relh);

        su_pa_do_get(relh->rh_key_pa, i, key) {
            if (rs_key_id(cd, key) == keyid) {
                return i;
            }
        }
        return -1;
}

/*##**********************************************************************\
 * 
 *              rs_relh_nattrs
 * 
 * Returns the total number of attributes in a relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      rel - in, use
 *              relation handle
 * 
 * Return value : 
 * 
 *      Number of attributes 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint rs_relh_nattrs(cd, relh)
        void*       cd __attribute__ ((unused));
        rs_relh_t*  relh;
{
        ss_dprintf_1(("%s: rs_relh_nattrs\n", __FILE__));
        CHECK_RELH(relh);
        ss_dassert(relh->rh_ttype != NULL);
        return(rs_ttype_nattrs(cd, relh->rh_ttype));
}

/*##**********************************************************************\
 * 
 *              rs_relh_link
 * 
 * Links the current user to the relation handle. The handle is not
 * released until the link count is zero, i.e. everyone has called
 * rs_relh_done.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_link(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        SS_NOTUSED(cd);

        ss_dprintf_1(("%s: rs_relh_link\n", __FILE__));
        CHECK_RELH(relh);

        SsSemEnter(relh->rh_sem);

        ss_dassert(relh->rh_nlink > 0);

        relh->rh_nlink++;

        SsSemExit(relh->rh_sem);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setaborted
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_setaborted(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        SsSemEnter(relh->rh_sem);

        relh->rh_aborted = TRUE;
        relh->rh_ddopactive++;

        SsSemExit(relh->rh_sem);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setddopactive
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_setddopactive(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        SsSemEnter(relh->rh_sem);
        
        relh->rh_ddopactive++;

        SsSemExit(relh->rh_sem);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setnoddopactive
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_setnoddopactive(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        SsSemEnter(relh->rh_sem);

        ss_dassert(relh->rh_ddopactive > 0);

        relh->rh_ddopactive--;

        SsSemExit(relh->rh_sem);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setreadonlyttype
 * 
 * Sets ttype in relh as read only. Helps avoid mutexing overhead.
 * 
 * Parameters : 
 * 
 *              relh - 
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
void rs_relh_setreadonlyttype(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        rs_ttype_setreadonly(cd, relh->rh_ttype, TRUE);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setreadonly
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_setreadonly(cd, relh)
        void*      cd;
        rs_relh_t* relh;
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        SsSemEnter(relh->rh_sem);
        
        relh->rh_readonly++;

        SsSemExit(relh->rh_sem);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setreltype
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      reltype - 
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
void rs_relh_setreltype(
        void*      cd,
        rs_relh_t* relh,
        rs_reltype_t reltype)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        if (reltype == RS_RELTYPE_PESSIMISTIC && relh->rh_pessgate == NULL) {
            relh->rh_pessgate = su_gate_init(SS_SEMNUM_DBE_PESSGATE, FALSE);
            /* Allow unlimited exclusive access. */
            su_gate_setmaxexclusive(relh->rh_pessgate, 0);
        }

        relh->rh_reltype = reltype;
}

#ifndef rs_relh_istransient
bool rs_relh_istransient(
        void*       cd __attribute__ ((unused)),
        rs_relh_t*  relh)
{
        CHECK_RELH(relh);

        return _RS_RELH_ISTRANSIENT_(cd, relh);
}
#endif

void rs_relh_settransient(
        void*       cd __attribute__ ((unused)),
        rs_relh_t*  relh,
        bool        istransient)
{
        CHECK_RELH(relh);

        relh->rh_istransient = istransient;
}

#ifndef rs_relh_isglobaltemporary
bool rs_relh_isglobaltemporary(
        void*       cd __attribute__ ((unused)),
        rs_relh_t*  relh)
{
        CHECK_RELH(relh);

        return _RS_RELH_ISGLOBALTEMPORARY_(cd, relh);
}
#endif

void rs_relh_setglobaltemporary(
        void*       cd __attribute__ ((unused)),
        rs_relh_t*  relh,
        bool        isglobaltemporary)
{
        CHECK_RELH(relh);

        relh->rh_isglobaltemporary = isglobaltemporary;
}

/*##**********************************************************************\
 * 
 *              rs_relh_pessgate_enter_shared
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
long rs_relh_pessgate_enter_shared(
        void*      cd,
        rs_relh_t* relh)
{
        su_profile_timer;

        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        su_profile_start;

        su_gate_enter_shared(relh->rh_pessgate);

        su_profile_stop("rs_relh_pessgate_enter_shared");

        return(relh->rh_pesschangecount);
}

/*##**********************************************************************\
 * 
 *              rs_relh_pessgate_enter_exclusive
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
void rs_relh_pessgate_enter_exclusive(
        void*      cd,
        rs_relh_t* relh)
{
        su_profile_timer;

        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        su_profile_start;

        su_gate_enter_exclusive(relh->rh_pessgate);
        if (++relh->rh_pesschangecount < 0) {
            /* Never return negative values. */
            relh->rh_pesschangecount = 0;
        }
        su_profile_stop("rs_relh_pessgate_enter_exclusive");
}

void rs_relh_pessgate_exit(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        su_gate_exit(relh->rh_pessgate);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setnocheck
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
void rs_relh_setnocheck(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        relh->rh_nocheck = TRUE;
}

/*##**********************************************************************\
 * 
 *              rs_relh_setcheck
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
void rs_relh_setcheck(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        relh->rh_nocheck = FALSE;
}

/*##**********************************************************************\
 * 
 *              rs_relh_insertkey
 * 
 * Puts the given key to the relation handle.
 * The key structure is NOT copied, the pointer is put there as is.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      key - in, take
 *              key
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 *      Should we take "key id" (index in su_pa) as an input parameter ?
 * 
 * Globals used : 
 */
bool rs_relh_insertkey(cd, relh, key)
        void*      cd;
        rs_relh_t* relh;
        rs_key_t*  key;
{
        int i;
        int nparts;
        long prevmaxlen = 0;
        long maxlen = VA_LENGTHMAXLEN;
        long atype_maxlen;

        ss_dprintf_1(("%s: rs_relh_insertkey\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        ss_dassert(relh->rh_key_pa);
        ss_debug(rs_key_setreadonly(cd, key));

        /* Calculate max storage length for this key.
         */
        nparts = rs_key_nparts(cd, key);
        for (i = 0; i < nparts; i++) {
            rs_ano_t ano;
            rs_atype_t* atype;
            ano = rs_keyp_ano(cd, key, i);
            if (ano == RS_ANO_NULL) {
                /* Not a real column. */
                atype_maxlen = VA_LONGMAXLEN;
            } else {
                atype = rs_ttype_atype(cd, relh->rh_ttype, ano);
                atype_maxlen = rs_atype_maxstoragelength(cd, atype);
            }
            if (atype_maxlen >= SS_INT4_MAX - VA_LENGTHMAXLEN) {
                maxlen = SS_INT4_MAX;
                break;
            }
            maxlen += VA_GROSSLENBYNETLEN(atype_maxlen);
            if (maxlen <= 0 || maxlen >= SS_INT4_MAX - VA_LENGTHMAXLEN || maxlen < prevmaxlen) {
                ss_dassert(maxlen >= prevmaxlen);
                maxlen = SS_INT4_MAX;
                break;
            }
            prevmaxlen = maxlen;
        }
        rs_key_setmaxstoragelen(cd, key, maxlen);

        su_pa_insert(relh->rh_key_pa, key);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *              rs_relh_deletekey
 *
 * Deletes the given key from the relation handle.
 * The key structure is NOT freed.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 * 
 *      key - in,
 *              key
 *
 * Return value :
 * 
 * Limitations  : 
 *
 *      Should we take "key id" (index in su_pa) as an input parameter ?
 * 
 * Globals used :   
 */
bool rs_relh_deletekey(cd, relh, dkey)
        void*      cd;
        rs_relh_t* relh;
        rs_key_t*  dkey;
{
        rs_key_t*  key;
        size_t     i;

        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_assert(dkey);
 
        ss_dassert(relh->rh_key_pa);
        su_pa_do_get(relh->rh_key_pa, i, key) {
            if (key == dkey) {
                su_pa_remove(relh->rh_key_pa, i);
                return TRUE;
            }
        }
        return FALSE;
}

/*##**********************************************************************\
 * 
 *              rs_relh_insertrefkey
 * 
 * Puts the given reference key to the relation handle. Reference keys
 * are used for referential integrity.
 * The key structure is NOT copied, the pointer is put there as is.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      key - in, take
 *              key
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_insertrefkey(cd, relh, key)
        void*      cd;
        rs_relh_t* relh;
        rs_key_t*  key;
{
        ss_dprintf_1(("%s: rs_relh_insertrefkey\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        ss_dassert(relh->rh_refkey_pa);
        su_pa_insert(relh->rh_refkey_pa, key);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              rs_relh_deleterefkey
 * 
 * Deletes reference key given by name to the relation handle. 
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      name - in, use
 *              key name
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_deleterefkey (cd, relh, name)
        void*      cd;
        rs_relh_t* relh;
        char*      name;
{
        size_t     i;
        rs_key_t*  key;
        bool       ret = FALSE;

        ss_dprintf_1(("%s: rs_relh_deleterefkey\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_assert(name && *name != 0);

        ss_dassert(relh->rh_refkey_pa);
        su_pa_do_get(relh->rh_refkey_pa, i, key) {
            if (strcmp(rs_key_name(cd, key), name) == 0) {
                su_pa_remove(relh->rh_refkey_pa, i);
                rs_key_done(cd, key);
                ret = TRUE;
            }
        }
        return ret;
}

/*##**********************************************************************\
 *
 *              rs_relh_hasrefkey
 *
 * Check if given relation has foreign key with given name.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      name - in, use
 *              key name
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_relh_hasrefkey (cd, relh, name)
        void*      cd;
        rs_relh_t* relh;
        char*      name;
{
        size_t     i;
        rs_key_t*  key;

        ss_dprintf_1(("%s: rs_relh_hasrefkey\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_assert(name && *name != 0);

        ss_dassert(relh->rh_refkey_pa);
        su_pa_do_get(relh->rh_refkey_pa, i, key) {
            if (rs_key_type(cd, key) == RS_KEY_FORKEYCHK &&
                strcmp(rs_key_name(cd, key), name) == 0)
            {
                return TRUE;
            }
        }
        return FALSE;
}

/*##**********************************************************************\
 * 
 *              rs_relh_refkeybyname
 * 
 * Find foreign key by its name
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      name - in, use
 *              foreign key name
 *
 * Return value : 
 *      rs_key_t* if found, NULL if not found
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_key_t* rs_relh_refkeybyname(cd, relh, name)
        void*      cd;
        rs_relh_t* relh;
        char*      name;
{
        size_t     i;
        rs_key_t*  key;

        ss_dprintf_1(("%s: rs_relh_refkeybyname\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_assert(name && *name != 0);

        ss_dassert(relh->rh_refkey_pa);
        su_pa_do_get(relh->rh_refkey_pa, i, key) {
            if (rs_key_type(cd, key) == RS_KEY_FORKEYCHK &&
                strcmp(rs_key_name(cd, key), name) == 0)
            {
                return key;
            }
        }

        return NULL;
}

/*##**********************************************************************\
 * 
 *              rs_relh_refkeybyid
 * 
 * Find foreign key by its key id
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      keyid - in, use
 *              foreign key id
 *
 * Return value : 
 *      rs_key_t* if found, NULL if not found
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_key_t* rs_relh_refkeybyid(
        void*      cd,
        rs_relh_t* relh,
        ulong      keyid)
{
        size_t     i;
        rs_key_t*  key;

        ss_dprintf_1(("%s: rs_relh_refkeybyid\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        ss_dassert(relh->rh_refkey_pa);

        su_pa_do_get(relh->rh_refkey_pa, i, key) {

            if (rs_key_id(cd, key) == keyid)
            {
                return (key);
            }
        }

        return (NULL);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setcheckstring
 * 
 * Sets the SQL condition string that represents the CHECK constraints
 * on the relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      checkstr - in, use
 *              pointer into the SQL condition string
 *
 * Return value : 
 *      TRUE, if succeeded
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_setcheckstring(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        char*      checkstr)
{
        CHECK_RELH(relh);
        SS_NOTUSED(cd);

        if (relh->rh_sqlcheckstrs) {
            int i=0;
            ss_dassert(relh->rh_sqlchecknames);
            while (relh->rh_sqlcheckstrs[i] != NULL) {
                SsMemFree(relh->rh_sqlcheckstrs[i]);
                if (relh->rh_sqlchecknames[i]) {
                    SsMemFree(relh->rh_sqlchecknames[i]);
                }
                i++;
            }
        } else {
            relh->rh_sqlcheckstrs = SsMemAlloc(2*sizeof(char*));
            relh->rh_sqlchecknames = SsMemAlloc(2*sizeof(char*));
        }

        relh->rh_sqlcheckstrs[0] = SsMemStrdup(checkstr);
        relh->rh_sqlchecknames[0] = NULL;
        relh->rh_sqlcheckstrs[1] = NULL;
        relh->rh_sqlchecknames[1] = NULL;

        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              rs_relh_addcheckstring
 *
 * Adds the SQL condition string that represents the CHECK constraints
 * on the relation.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      checkstr - in, use
 *              pointer into the SQL condition string
 *
 * Return value :
 *      TRUE, if succeeded
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_relh_addcheckstring(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        char*      checkstr,
        char*      constrname)
{
        int k;

        CHECK_RELH(relh);
        SS_NOTUSED(cd);

        if (relh->rh_sqlcheckstrs == NULL) {
            k = 0;
        } else {
            k=0;
            while (relh->rh_sqlcheckstrs[k]) {
                k++;
            }
        }

        relh->rh_sqlcheckstrs = SsMemRealloc(relh->rh_sqlcheckstrs,
                                             (k+2)*sizeof(char*));
        relh->rh_sqlchecknames = SsMemRealloc(relh->rh_sqlchecknames,
                                              (k+2)*sizeof(char*));
        relh->rh_sqlcheckstrs[k] = SsMemStrdup(checkstr);
        relh->rh_sqlcheckstrs[k+1] = NULL;
        if (constrname != NULL && constrname[0] != '$') {
            relh->rh_sqlchecknames[k] = SsMemStrdup(constrname);
        } else {
            relh->rh_sqlchecknames[k] = NULL;
        }
        relh->rh_sqlchecknames[k+1] = NULL;

        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setdefault
 * 
 * Sets the default values for the attributes of the relation
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      defarr - in, use
 *              See tb_createrelation
 *
 *      defvalue - in, use
 *              ------ "" ------
 *
 * Return value : 
 *      TRUE, if succeeded
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_setdefault(cd, relh, defarr, defvalue)
        void*       cd;
        rs_relh_t*  relh;
        uint*       defarr;
        rs_tval_t*  defvalue;
{
        ss_dprintf_1(("%s: rs_relh_setdefault\n", __FILE__));
        CHECK_RELH(relh);

        if (relh->rh_defarr != NULL) {
            SsMemFree(relh->rh_defarr);
            relh->rh_defarr = NULL;
            ss_dassert(relh->rh_defvalue != NULL);
            rs_tval_free(cd, relh->rh_ttype, relh->rh_defvalue);
        }

        if (defarr != NULL) {
            uint nattrs = rs_ttype_nattrs(cd, relh->rh_ttype);
            relh->rh_defarr = (uint *)SsMemAlloc(nattrs*sizeof(uint));
            memcpy(relh->rh_defarr, defarr, nattrs*sizeof(uint));
            ss_dassert(defvalue);
            relh->rh_defvalue = rs_tval_copy(cd, relh->rh_ttype, defvalue);
        }
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setrelid
 * 
 * Sets the relation id of relation
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in out, use
 *              relation handle
 *
 *      relid - in, use
 *              relation id
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_setrelid(
        void*      cd,
        rs_relh_t* relh,
        ulong      relid
) {
        CHECK_RELH(relh);
        SS_NOTUSED(cd);
        relh->rh_relid = relid;
}

#ifndef rs_relh_relid
/*##**********************************************************************\
 * 
 *              rs_relh_relid
 * 
 * Returns the relation id of a given relation
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
ulong rs_relh_relid(
        void*      cd,
        rs_relh_t* relh)
{
        CHECK_RELH(relh);
        SS_NOTUSED(cd);
        return _RS_RELH_RELID_(cd, relh);
}
#endif /* !defined(rs_relh_relid) */

/*##**********************************************************************\
 * 
 *              rs_relh_default
 * 
 * Returns the default values for the attributes of the relation
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 *      p_defarr - out, ref
 *              See tb_createrelation
 *
 *      p_defvalue - out, ref
 *              See tb_createrelation
 * 
 * Return value : 
 *      TRUE, if succeeded
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_default(cd, relh, p_defarr, p_defvalue)
        void*       cd;
        rs_relh_t*  relh;
        uint**      p_defarr;
        rs_tval_t** p_defvalue;
{
        ss_dprintf_1(("%s: rs_relh_default\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(p_defarr != NULL);
        ss_dassert(p_defvalue != NULL);
        *p_defarr = relh->rh_defarr;
        *p_defvalue = relh->rh_defvalue;
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              rs_relh_cardin
 * 
 * Returns relations cardinality object.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value - ref : 
 *
 *      Cardin object.
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_cardin_t* rs_relh_cardin(
        void*      cd,
        rs_relh_t* relh
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_cardin);
}

/*##**********************************************************************\
 * 
 *              rs_relh_replacecardin
 * 
 * Replaces relh cardinal information with the information in
 * parameter cr.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in
 *              
 *              
 *      cr - in
 *              
 *              
 * Return value - ref : 
 * 
 *      Relh cardin object.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
rs_cardin_t* rs_relh_replacecardin(
        void*      cd,
        rs_relh_t* relh,
        rs_cardin_t* cr
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_cardin != NULL);
        ss_dprintf_1(("rs_relh_replacecardin:%s\n", rs_entname_getname(relh->rh_name)));

        if (cr != NULL && relh->rh_cardin != cr) {
            rs_cardin_replace(cd, relh->rh_cardin, cr);
        }
        return(relh->rh_cardin);
}

/*##**********************************************************************\
 * 
 *		rs_relh_setcardin
 * 
 * Sets a new cardin object into relh. Used when loading relh info from
 * system tables and we already have old cardin info in the rbuf.
 * 
 * Parameters : 
 * 
 *		cd - 
 *			
 *			
 *		relh - 
 *			
 *			
 *		cr - 
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
void rs_relh_setcardin(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        rs_cardin_t* cr
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_cardin != NULL);
        ss_dprintf_1(("rs_relh_setcardin:%s\n", rs_entname_getname(relh->rh_name)));

        if (cr != NULL && relh->rh_cardin != cr) {
            ss_dassert(!rs_cardin_ischanged(cd, relh->rh_cardin));
            rs_cardin_link(cd, cr);
            rs_cardin_done(cd, relh->rh_cardin);
            relh->rh_cardin = cr;
        }
}

/*##**********************************************************************\
 * 
 *              rs_relh_cardinalchanged
 * 
 * Checks if nbytes and ntuples are changed in this relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value : 
 *
 *      TRUE    - changed
 *      FALSE   - not changed
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_cardinalchanged(
        void*      cd,
        rs_relh_t* relh
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dprintf_1(("rs_relh_cardinalchanged:%s:%d\n",
            rs_entname_getname(relh->rh_name), rs_cardin_ischanged(cd, relh->rh_cardin)));

        return(rs_cardin_ischanged(cd, relh->rh_cardin));
}

/*##**********************************************************************\
 * 
 *              rs_relh_clearcardinalchanged
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
void rs_relh_clearcardinalchanged(
        void*      cd,
        rs_relh_t* relh
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dprintf_1(("rs_relh_clearcardinalchanged:%s\n", rs_entname_getname(relh->rh_name)));

        rs_cardin_clearchanged(cd, relh->rh_cardin);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setcardinal
 * 
 * Sets ntuples and nbytes of a relation.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      ntuples - 
 *              
 *              
 *      nbytes - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_setcardinal(
        void*      cd,
        rs_relh_t* relh,
        ss_int8_t  ntuples,
        ss_int8_t  nbytes
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dprintf_1(("rs_relh_setcardinal:%s:ntuples=%ld, nbytes=%ld\n",
            rs_entname_getname(relh->rh_name), ntuples, nbytes));

        rs_cardin_setdata(cd, relh->rh_cardin, ntuples, nbytes);
}

/*##**********************************************************************\
 * 
 *              rs_relh_updatecardinal
 * 
 * Updates ntuples and nbytes of a relation.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      ntuples - 
 *              
 *              
 *      nbytes - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_updatecardinal(
        void*       cd,
        rs_relh_t*  relh,
        long        ntuples,
        long        nbytes,
        ulong       nchanges)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dprintf_1(("rs_relh_updatecardinal:%s:ntuples=%ld, nbytes=%ld\n, nchanges=%ld",
            rs_entname_getname(relh->rh_name), ntuples, nbytes, nchanges));

#if defined(SS_MME) && defined(MME_ALTERNATE_CARDINALITY)
        if (relh->rh_reltype == RS_RELTYPE_MAINMEMORY) {
            return FALSE;
        }
#endif

        relh->rh_samplechange += nchanges;
        relh->rh_samplechangectr += nchanges;
        relh->rh_replanchange += nchanges;
        relh->rh_replanchangectr += nchanges;

        rs_cardin_updatedata(cd, relh->rh_cardin, ntuples, nbytes);

        return relh_clearsamplesif(cd, relh) || relh_mustreplan(cd, relh);
}

/*##**********************************************************************\
 * 
 *              rs_relh_cardininfo
 * 
 * Returns cardinality information of the relation.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in
 *              
 *              
 *      p_ntuples - out
 *              
 *              
 *      p_nbytes - out
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
void rs_relh_cardininfo(
        void*      cd,
        rs_relh_t* relh,
        ss_int8_t* p_ntuples,
        ss_int8_t* p_nbytes
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(!relh->rh_isglobaltemporary);

        rs_cardin_getdata(cd, relh->rh_cardin, p_ntuples, p_nbytes);
}

/*##**********************************************************************\
 * 
 *              rs_relh_nbytes
 * 
 * Returns the number of bytes in a relation. The value is the total
 * number of bytes is a clustering key including all possible system
 * key parts. No compression is expected.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value : 
 *
 *      Number of bytes in a relation
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
ss_int8_t rs_relh_nbytes(
        void*      cd,
        rs_relh_t* relh
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(rs_cardin_nbytes(cd, relh->rh_cardin));
}

/*##**********************************************************************\
 * 
 *              rs_relh_insertbytes
 * 
 * Inserts bytes to a relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 *      nbytes - in
 *          number of bytes inserted
 *
 * Return value : 
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_insertbytes(
        void*       cd,
        rs_relh_t*  relh,
        ulong       nbytes,
        ulong       nchanges)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

#if defined(SS_MME) && defined(MME_ALTERNATE_CARDINALITY)
        ss_dassert(relh->rh_reltype != RS_RELTYPE_MAINMEMORY);
#endif
        ss_dprintf_1(("rs_relh_insertbytes:%s:nbytes=%ld\n",
            rs_entname_getname(relh->rh_name), nbytes));

        rs_cardin_insertbytes(cd, relh->rh_cardin, nbytes);

        relh->rh_samplechange += nchanges;
        relh->rh_samplechangectr += nchanges;
        relh->rh_replanchange += nchanges;
        relh->rh_replanchangectr += nchanges;

        return relh_clearsamplesif(cd, relh) || relh_mustreplan(cd, relh);
}

/*##**********************************************************************\
 * 
 *              rs_relh_deletebytes
 * 
 * Deletes bytes to a relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 *      nbytes - in
 *          number of bytes deleted
 *
 * Return value : 
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_relh_deletebytes(
        void*       cd,
        rs_relh_t*  relh,
        ulong       nbytes,
        ulong       nchanges)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

#if defined(SS_MME) && defined(MME_ALTERNATE_CARDINALITY)
        ss_dassert(relh->rh_reltype != RS_RELTYPE_MAINMEMORY);
#endif
        ss_dprintf_1(("rs_relh_deletebytes:%s:nbytes=%ld\n",
            rs_entname_getname(relh->rh_name), nbytes));

        rs_cardin_deletebytes(cd, relh->rh_cardin, nbytes);

        relh->rh_samplechange += nchanges;
        relh->rh_samplechangectr += nchanges;
        relh->rh_replanchange += nchanges;
        relh->rh_replanchangectr += nchanges;

        return relh_clearsamplesif(cd, relh) || relh_mustreplan(cd, relh);
}

#ifndef SS_NOESTSAMPLES

/*##**********************************************************************\
 *
 *      rs_relh_applydelta
 *
 * Applies the given delta to the relh's cardinality.  The deltas may be
 * positive or negative.
 *
 * Parameters:
 *      cd - in, use
 *          The client data.
 *
 *      relh - in out, use
 *          The relh.
 *
 *      delta_rows - use
 *          The number of rows to add to the cardinality.
 *
 *      delta_bytes - use
 *          The number of bytes to add to the cardinality.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
bool rs_relh_applydelta(
        void*       cd,
        rs_relh_t*  relh,
        long        delta_rows,
        long        delta_bytes)
{
        CHECK_RELH(relh);
        rs_cardin_applydelta(cd, relh->rh_cardin, delta_rows, delta_bytes);

        relh->rh_samplechange++;
        relh->rh_samplechangectr++;
        relh->rh_replanchange++;
        relh->rh_replanchangectr++;

        return relh_clearsamplesif(cd, relh) || relh_mustreplan(cd, relh);
}

/* Use this ONLY FROM INSIDE THE MME SEMAPHORE! */
bool rs_relh_applydelta_nomutex(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        long        delta_rows,
        long        delta_bytes)
{
        CHECK_RELH(relh);
        rs_cardin_applydelta_nomutex(cd, relh->rh_cardin,
                                     delta_rows, delta_bytes);

        relh->rh_samplechange++;
        relh->rh_samplechangectr++;
        relh->rh_replanchange++;
        relh->rh_replanchangectr++;

        return relh_clearsamplesif(cd, relh) || relh_mustreplan(cd, relh);
}

/*#***********************************************************************\
 * 
 *              rs_relh_printsamples
 * 
 * Prints sample information.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
void rs_relh_printsamples(
        void*      cd,
        rs_relh_t* relh,
        int        level)
{
        int i;
        int j;
        uint samplesize;
        relh_attrsamples_t* samples;
        char buf[255];

        SsSprintf(buf, "Table %.80s\n", rs_entname_getname(relh->rh_name));
        if (level > 0) {
            rs_sysi_printsqlinfo(cd, level, buf);
        } else {
            SsDbgPrintf("%s", buf);
        }

        su_pa_do_get(relh->rh_attrsamples, i, samples) {
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = rs_ttype_atype(cd, relh->rh_ttype, i);
            aval = rs_aval_create(cd, atype);

            SsSprintf(buf, "  %.80s (%.80s)\n",
                rs_ttype_aname(cd, relh->rh_ttype, i),
                rs_atype_name(cd, atype));
            if (level > 0) {
                rs_sysi_printsqlinfo(cd, level, buf);
            } else {
                SsDbgPrintf("%s", buf);
            }

            samplesize = (uint)su_pa_getdata(relh->rh_samplesizes, i);

            SsSprintf(buf, "    %d samples, %d different samples, %d equal estimate, %d null samples, %ld max diff rows\n",
                samplesize,
                (uint)su_pa_getdata(relh->rh_samplediffs, i),
                su_pa_indexinuse(relh->rh_eqrowestimate, i)
                    ? (uint)su_pa_getdata(relh->rh_eqrowestimate, i)
                    : -1,
                su_pa_indexinuse(relh->rh_samplenulls, i)
                    ? (int)su_pa_getdata(relh->rh_samplenulls, i)
                    : 0,
                su_pa_indexinuse(relh->rh_maxdiffrows, i)
                    ? (long)su_pa_getdata(relh->rh_maxdiffrows, i)
                    : -1L);
            if (level > 0) {
                rs_sysi_printsqlinfo(cd, level, buf);
            } else {
                SsDbgPrintf("%s", buf);
            }

            for (j = 0; j < (int)samplesize; j++) {
                char* s;
                rs_aval_setva(cd, atype, aval, samples[j].as_dva);
                s = rs_aval_print(cd, atype, aval);
                SsSprintf(buf, "    %.80s\n", s);
                if (level > 0) {
                    rs_sysi_printsqlinfo(cd, level, buf);
                } else {
                    SsDbgPrintf("%s", buf);
                }
                SsMemFree(s);
            }
            rs_aval_free(cd, atype, aval);
        }
}

/*#***********************************************************************\
 * 
 *              sample_sort_cmp
 * 
 * 
 * 
 * Parameters : 
 * 
 *      p_va1 - 
 *              
 *              
 *      p_va2 - 
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
static int SS_CLIBCALLBACK sample_sort_cmp(
        const void* p_as1,
        const void* p_as2)
{
        relh_attrsamples_t* as1 = p_as1;
        relh_attrsamples_t* as2 = p_as2;

        return(va_compare(as1->as_dva, as2->as_dva));
}

/*#***********************************************************************\
 * 
 *              sample_search_cmp
 * 
 * 
 * 
 * Parameters : 
 * 
 *      keyva - 
 *              
 *              
 *      p_elemva - 
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
static int sample_search_cmp(
        const void* p_keyas,
        const void* p_elemas)
{
        va_t* keyas = p_keyas;
        relh_attrsamples_t* elemas = p_elemas;

        return(va_compare(keyas, elemas->as_dva));
}

void rs_relh_samplemutex_enter(
        void*      cd,
        rs_relh_t* relh)
{
        CHECK_RELH(relh);
        SS_NOTUSED(cd);
#if defined(SS_SEMSTK_DBG) && 0 /* Does not work in localserver, JarmoR Jan 7, 2000 */
        ss_dassert(!SsSemStkFind(SS_SEMNUM_RES_RELHSAMPLE));
#endif /* SS_SEMSTK_DBG */

        SsSemEnter(relh->rh_samplesem);
}

void rs_relh_samplemutex_exit(
        void*      cd,
        rs_relh_t* relh)
{
        CHECK_RELH(relh);
        SS_NOTUSED(cd);
        ss_dassert(SsSemStkFind(SS_SEMNUM_RES_RELHSAMPLE));

        SsSemExit(relh->rh_samplesem);
}

static void relh_initattrsamples_nomutex(
        void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        vtpl_t**   keysamples,
        rs_tval_t** tval_arr,
        int*       sample_estimates,
        int        sample_size)
{
        int i;
        rs_key_t* clustkey;
        vtpl_t* keyvtpl;
        relh_attrsamples_t* attrsamples;
        rs_ano_t kpno = 0;
        uint diffsamples;
        uint nullsamples;
        rs_ttype_t* ttype;
        rs_tval_t* tval;
        rs_atype_t* atype;
        rs_datatype_t datatype;
        int eqrowestimate_sum;
        int eqrowestimate_count;

        CHECK_RELH(relh);
        ss_dprintf_1(("relh_initattrsamples_nomutex:%s, ano=%d\n", rs_relh_name(cd, relh), ano));
        ss_dassert(SsSemStkFind(SS_SEMNUM_RES_RELHSAMPLE));
        SS_PUSHNAME("relh_initattrsamples_nomutex");

        relh->rh_issamples = TRUE;

        if (SsInt8Is0(relh->rh_samplentuples)) {
            relh->rh_samplechange = 0;
            relh->rh_samplentuples = rs_cardin_ntuples(cd, relh->rh_cardin);
        }

        if (su_pa_indexinuse(relh->rh_attrsamples, ano)) {
            /* There are old samples, remove them.
             */
            relh_attrsamples_t* old_samples;
            uint old_samplesize;
            old_samples = su_pa_remove(relh->rh_attrsamples, ano);
            old_samplesize = (uint)su_pa_remove(relh->rh_samplesizes, ano);
            for (i = 0; i < (int)old_samplesize; i++) {
                dynva_free(&old_samples[i].as_dva);
            }
            SsMemFree(old_samples);
        }

        su_pa_insertat(relh->rh_samplesizes, ano, (void*)sample_size);

        if (keysamples != NULL) {
            clustkey = rs_relh_clusterkey(cd, relh);

            kpno = rs_key_searchkpno_data(cd, clustkey, ano);
            ss_dassert(kpno != RS_ANO_NULL);
        }

        ttype = rs_relh_ttype(cd, relh);

        /*  The following assert is not true in ROWID constraints.
            Jarmo Mar 18, 1995
            ss_dassert(rs_keyp_parttype(cd, clustkey, kpno) == RSAT_USER_DEFINED);
        */

        attrsamples = SsMemCalloc(sizeof(attrsamples[0]), sample_size);
        su_pa_insertat(relh->rh_attrsamples, ano, attrsamples);

        /* Loop through all samples.
         */
        for (i = 0; i < sample_size; i++) {
            va_t* va;
            size_t maxcmplen;

            if (keysamples != NULL) {
                keyvtpl = keysamples[i];
                ss_dassert(keyvtpl != NULL);

                va = vtpl_getva_at(keyvtpl, kpno);
            } else {
                tval = tval_arr[i];
                ss_dassert(tval != NULL);

                va = rs_tval_va(cd, ttype, tval, ano);
            }

            /* make sure the truncation is after even number of bytes
               in order to not truncate UNICODE values from half-char
               position! (null-termination makes the resulting
               va net len be odd, don't be confused)
            */
            maxcmplen = (RS_KEY_MAXCMPLEN + 1) / 2;
            maxcmplen *= 2;
            
            if (va_netlen(va) > maxcmplen) {
                
                dynva_setdataandnull(
                    &attrsamples[i].as_dva,
                    va_getasciiz(va),
                    maxcmplen);
            } else {
                dynva_setva(&attrsamples[i].as_dva, va);
            }
            if (sample_estimates != NULL) {
                attrsamples[i].as_eqrowestimate = sample_estimates[i];
            }
        }

        /* Sort the attribute sample values.
         */
        qsort(
            attrsamples,
            sample_size,
            sizeof(attrsamples[0]),
            sample_sort_cmp);

        for (i = 0, diffsamples = 1, nullsamples = 0; i < sample_size; i++) {
            if (i > 0) {
                if (va_compare(attrsamples[i-1].as_dva, attrsamples[i].as_dva) != 0) {
                    diffsamples++;
                } else {
                    attrsamples[i-1].as_eqrowestimate = 0;
                    attrsamples[i].as_eqrowestimate = 0;
                }
            }
            if (va_testnull(attrsamples[i].as_dva)) {
                nullsamples++;
            }
        }

        su_pa_insertat(relh->rh_samplediffs, ano, (void*)diffsamples);
        if (nullsamples > 0) {
            su_pa_insertat(relh->rh_samplenulls, ano, (void*)nullsamples);
        }

        eqrowestimate_sum = 0;
        eqrowestimate_count = 0;

        for (i = 0; i < sample_size; i++) {
            if (attrsamples[i].as_eqrowestimate > 0) {
                eqrowestimate_sum += attrsamples[i].as_eqrowestimate;
                eqrowestimate_count++;
            }
        }

        if (eqrowestimate_count > 0) {
            ss_dassert(eqrowestimate_sum / eqrowestimate_count > 0);
            su_pa_insertat(relh->rh_eqrowestimate, ano, (void*)(eqrowestimate_sum / eqrowestimate_count));
        }

        /* Find maximum possible different values for integer columns.
         */
        ttype = rs_relh_ttype(cd, relh);
        atype = rs_ttype_atype(cd, ttype, ano);
        datatype = rs_atype_datatype(cd, atype);

        if (datatype == RSDT_INTEGER
            || datatype == RSDT_BIGINT)
        {
            long maxdiffrows;
            /* Find first non-null sample.
             */
            for (i = 0; i < sample_size; i++) {
                if (!va_testnull(attrsamples[i].as_dva)) {
                    break;
                }
            }
            if (i == sample_size) {
                /* All values are NULL */
                maxdiffrows = 1;
            } else {
                /* Max different values is difference from the first and
                   last integer values. 
                 */
                ss_int8_t maxdiffrows_i8;

                /* maxdiffrows = attrsamples[sample_size-1].as_dva) - attrsamples[i].as_dva + 1; */
                SsInt8SubtractInt8(
                    &maxdiffrows_i8,
                    va_getint8(attrsamples[sample_size-1].as_dva),
                    va_getint8(attrsamples[i].as_dva));
                SsInt8AddUint2(&maxdiffrows_i8, maxdiffrows_i8, (ss_uint2_t)1);

                if (SsInt8IsNegative(maxdiffrows_i8)
                    || SsInt8GetMostSignificantUint4(maxdiffrows_i8) != 0) 
                {
                    /* Overflow */
                    maxdiffrows = LONG_MAX;
                } else {
                    ss_int8_t long_max_i8;
                    SsInt8SetInt4(&long_max_i8, LONG_MAX);
                    if (SsInt8Cmp(maxdiffrows_i8, long_max_i8) >= 0) {
                        /* Overflow */
                        maxdiffrows = LONG_MAX;
                    } else {
                        maxdiffrows = SsInt8GetLeastSignificantUint4(maxdiffrows_i8);
                    }
                }
            }
            su_pa_insertat(relh->rh_maxdiffrows, ano, (void*)maxdiffrows);
        }

        SS_POPNAME;
        ss_output_4(rs_relh_printsamples(cd, relh, 0));
        ss_trigger("rs_relh_initattrsamples");
}

/*##**********************************************************************\
 * 
 *              rs_relh_initattrsamples_nomutex
 * 
 * Inits attribute sample information. Sample information is used in
 * estimator to calculate attribute selectivities.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      ano - 
 *              
 *              
 *      keysamples - 
 *              
 *              
 *      sample_size - 
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
void rs_relh_initattrsamples_nomutex(
        void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        vtpl_t**   keysamples,
        int*       sample_estimates,
        int        sample_size)
{
        ss_dprintf_1(("rs_relh_initattrsamples_nomutex\n"));
        relh_initattrsamples_nomutex(cd, relh, ano, keysamples, NULL, sample_estimates, sample_size);
}

void rs_relh_initattrsamplesbytval_nomutex(
        void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        rs_tval_t** tval_arr,
        int        sample_size)
{
        ss_dprintf_1(("rs_relh_initattrsamplesbytval_nomutex\n"));
        relh_initattrsamples_nomutex(cd, relh, ano, NULL, tval_arr, NULL, sample_size);
}

/*##**********************************************************************\
 * 
 *              rs_relh_issamples_nomutex
 * 
 * Checks if attribute sample information is available for the relation.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      ano - 
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
bool rs_relh_issamples_nomutex(
        void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano)
{
        bool succp;

        CHECK_RELH(relh);
        SS_NOTUSED(cd);
        ss_dassert(SsSemStkFind(SS_SEMNUM_RES_RELHSAMPLE));

        if (ano == -1) {
            succp = relh->rh_issamples;
        } else {
            succp = su_pa_indexinuse(relh->rh_attrsamples, ano);
        }

        return(succp);
}

/*##**********************************************************************\
 * 
 *              rs_relh_issamplefailed_nomutex
 * 
 * Checks if attribute selectivity sampling has failed. If previous sampling
 * has failed, there is use of trying again very soon.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
bool rs_relh_issamplefailed_nomutex(
        void*      cd,
        rs_relh_t* relh)
{
        CHECK_RELH(relh);
        SS_NOTUSED(cd);
        ss_dassert(SsSemStkFind(SS_SEMNUM_RES_RELHSAMPLE));

        return(relh->rh_samplefailed);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setsamplefailed_nomutex
 * 
 * Sets attribute sampling as failed. The system has tried to get samples
 * from the database, but the sampling has failed for some reason.
 * If sampling has failed, there is use of trying again very soon.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
void rs_relh_setsamplefailed_nomutex(
        void*      cd,
        rs_relh_t* relh)
{
        CHECK_RELH(relh);
        SS_NOTUSED(cd);

        relh->rh_samplefailed = TRUE;
}

/*#***********************************************************************\
 * 
 *              relh_values_in_range
 * 
 * Calculates number of values in given number of matching sample ranges.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      samplesize - 
 *              
 *              
 *      nrange - 
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
static ss_int8_t relh_values_in_range(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        int         samplesize,
        int         nrange)
{
        ss_int8_t values_per_range;
        ss_int8_t dummy1, dummy2, dummy3;

        ss_dprintf_3(("relh_values_in_range\n"));

        if (nrange >= samplesize) {
#if 0
            nrange = samplesize-1;
#else
            return(rs_cardin_ntuples(cd, relh->rh_cardin));
#endif
        }
        SsInt8SetInt4(&dummy1, samplesize);
        SsInt8DivideByInt8(&values_per_range,
            rs_cardin_ntuples(cd, relh->rh_cardin), dummy1);
        
        /* Return values in nrange ranges, add one half range for
         * incomplete range.
         */
        SsInt8SetUint4(&dummy1, nrange);
        SsInt8MultiplyByInt8(&dummy2, values_per_range, dummy1);

        SsInt8SetUint4(&dummy1, 2);
        SsInt8DivideByInt8(&dummy3, values_per_range, dummy1);

        /* values_per_range * nrange + values_per_range/2 */
        SsInt8AddInt8(&dummy1, dummy2, dummy3);

        return(dummy1);
}

/*##**********************************************************************\
 * 
 *              rs_relh_getequalvalues
 * 
 * Returns selectivity for equal constraint if the selectivity does not
 * depend on the actual value.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      samplesize - 
 *              
 *              
 *      nrange - 
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
bool rs_relh_getequalvalues(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        rs_ano_t    ano,
        long*       p_nvalues)
{
        int samplediff;
        int samplesize;
        bool b = FALSE;

        SsSemEnter(relh->rh_samplesem);

        if (relh->rh_issamples
            && su_pa_indexinuse(relh->rh_samplesizes, ano)
            && su_pa_indexinuse(relh->rh_samplediffs, ano)) 
        {
            samplesize = (int)su_pa_getdata(relh->rh_samplesizes, ano);
            samplediff = (int)su_pa_getdata(relh->rh_samplediffs, ano);
            if (samplesize == samplediff
                && su_pa_indexinuse(relh->rh_eqrowestimate, ano)) 
            {
                b = TRUE;
                *p_nvalues = (int)su_pa_getdata(relh->rh_eqrowestimate, ano);
                ss_dprintf_1(("rs_relh_getequalvalues:eqrowestimate, nvalues=%ld\n", *p_nvalues));
            }
        }

        SsSemExit(relh->rh_samplesem);

        return(b);
}
    

/*#***********************************************************************\
 * 
 *              relh_selectivity_equalonerange
 * 
 * Gets selectivity in number of tuples for equal type of relop
 * (RS_RELOP_EQUAL or RS_RELOP_NOTEQUAL).
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      ano - 
 *              
 *              
 *      relop - 
 *              
 *              
 *      p_nvalues - 
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
static void relh_selectivity_equalonerange(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        rs_ano_t    ano,
        int         relop,
        int         samplesize,
        bool        value_known,
        bool        null_value,
        ss_int8_t*  p_nvalues)
{
        int samplediff;
        int samplediffperc;
        int samplenull;
        int samplenullperc;
        ss_int8_t ntuples_in_table_i8;
        ss_int8_t samplesize_i8;
        ss_int8_t dummy_i8;
        ss_int8_t dummy2_i8;
        ss_int8_t values_per_range_i8;
        ss_int4_t nvalues;
        ss_int8_t nvalues_i8;
        ss_int4_t ntuples_in_table_int4;

        ss_dassert(relop == RS_RELOP_EQUAL || relop == RS_RELOP_NOTEQUAL ||
                   relop == RS_RELOP_LIKE);
        ss_dassert(null_value ? value_known : TRUE);

        ss_dassert(su_pa_indexinuse(relh->rh_samplediffs, ano));
        samplediff = (int)su_pa_getdata(relh->rh_samplediffs, ano);
        if (su_pa_indexinuse(relh->rh_samplenulls, ano)) {
            samplenull = (int)su_pa_getdata(relh->rh_samplenulls, ano);
            /* Remove one for NULL value. */
            samplediff = samplediff - 1;
            samplenullperc = (int)((double)samplenull / (double)samplesize * 100.0);
        } else {
            samplenull = 0;
            samplenullperc = 0;
        }
        samplediffperc = (int)((double)samplediff / (double)samplesize * 100.0);

        ntuples_in_table_i8 = rs_cardin_ntuples(cd, relh->rh_cardin);

        if (!null_value && samplenullperc > 0) {
            /* Remove NULL values from rows in table because
             * equal constraint can never match to NULL value.
             */
            double ntuples_in_table_dbl;

            SsInt8ConvertToDouble(&ntuples_in_table_dbl, ntuples_in_table_i8);
            ntuples_in_table_dbl = ((100 - samplenullperc) / 100.0) * ntuples_in_table_dbl;
            SsInt8SetDouble(&ntuples_in_table_i8, ntuples_in_table_dbl);

            ss_dprintf_4(("relh_selectivity_equalonerange:samplenullperc=%d, ntuples_in_table=%lu->%lu\n",
                samplenullperc,
                (ulong)SsInt8GetLeastSignificantUint4(rs_cardin_ntuples(cd, relh->rh_cardin)),
                (ulong)SsInt8GetLeastSignificantUint4(ntuples_in_table_i8)));
        }

        if (samplediffperc < 1) {
            samplediffperc = 1;
        }
        SsInt8SetUint4(&dummy_i8, 100);
        if (SsInt8Cmp(ntuples_in_table_i8, dummy_i8) < 0) {
            /* Less than 100 rows in a table, set table size to 100 rows. */
            SsInt8SetUint4(&ntuples_in_table_i8, 100);
        } else {
            /* Seppo must fix! ntuples_in_table must be double! */
            SsInt8SetUint4(&dummy_i8, SS_INT4_MAX);
            if (SsInt8Cmp(ntuples_in_table_i8, dummy_i8) > 0) {
                /* More than SS_INT4_MAX rows in a table, set table size to SS_INT4_MAX. */
                SsInt8SetUint4(&ntuples_in_table_i8, SS_INT4_MAX);
            }
        }
        SsInt8ConvertToInt4(&ntuples_in_table_int4, ntuples_in_table_i8);
        if (su_pa_indexinuse(relh->rh_eqrowestimate, ano)) {
            nvalues = (int)su_pa_getdata(relh->rh_eqrowestimate, ano);
            ss_dprintf_4(("relh_selectivity_equalonerange:onerangeestimate, nvalues=%d\n", (unsigned long)nvalues));
        } else {
            nvalues = sql_tupleest(ntuples_in_table_int4, samplediffperc);
            ss_dprintf_4(("relh_selectivity_equalonerange:sql_tupleest, nvalues=%d\n", (unsigned long)nvalues));
        }

        if (value_known) {
            /* We have a known value, that is, what is the constraint value.
             * In many cases e.g. in joins the value is unknown at estimate
             * phase.
             */

            /* values_per_range_i8 = ntuples_in_table_i8 / samplesize_i8 
             */
            SsInt8SetInt4(&samplesize_i8, samplesize);
            SsInt8DivideByInt8(&values_per_range_i8, ntuples_in_table_i8, samplesize_i8);

            /* dummy_i8 = values_per_range_i8 / 2
             */
            SsInt8SetInt4(&dummy2_i8, 2);
            SsInt8DivideByInt8(&dummy_i8, values_per_range_i8, dummy2_i8);

            SsInt8SetInt4(&nvalues_i8, nvalues);
            if (SsInt8Cmp(nvalues_i8, dummy_i8) > 0) {
                /* nvalues > values_per_range_i8 / 2,
                 * set nvalues = values_per_range_i8 / 2
                 */
                SsInt8ConvertToInt4(&nvalues, dummy_i8);
            }
#if 0 /* JarmoR removed Jan 6, 1998 after sql_tuplest was changed */
            else if (nvalues == 1 && ntuples_in_table > 10000) {
                if (values_per_range >= 1000) {
                    nvalues = 2 * (values_per_range / 1000) + 1;
                } else {
                    nvalues = 2;
                }
            }
#endif /* 0 */

            if (relop == RS_RELOP_NOTEQUAL) {
                /* *p_nvalues = ntuples_in_table_i8 - nvalues_i8
                 */
                SsInt8SetUint4(&nvalues_i8, nvalues);
                SsInt8SubtractInt8(p_nvalues, ntuples_in_table_i8, nvalues_i8);
                ss_dprintf_4(("relh_selectivity_equalonerange:relop == RS_RELOP_NOTEQUAL:nvalues=%lu, samplediffperc=%d, ntuples_in_table=%lu, value_known=%d\n",
                    (unsigned long)SsInt8GetLeastSignificantUint4(*p_nvalues), 
                    samplediffperc,
                    (unsigned long)ntuples_in_table_int4, 
                    value_known));
                return;
            }
        } else {
            int avg_nrange;

            if (samplediff > 0) {
                avg_nrange = (int)(((double)samplesize / (double)samplediff) + 0.5);
            } else {
                avg_nrange = samplesize;
            }

            if (avg_nrange < 1) {
                avg_nrange = 1;
            }

            /* values_per_range_i8 = ntuples_in_table_i8 / samplesize_i8 
             */
            SsInt8SetInt4(&samplesize_i8, samplesize);
            SsInt8DivideByInt8(&values_per_range_i8, ntuples_in_table_i8, samplesize_i8);

            /* dummy_i8 = values_per_range_i8 * avg_nrange
             */
            SsInt8SetInt4(&dummy2_i8, avg_nrange);
            SsInt8MultiplyByInt8(&dummy_i8, values_per_range_i8, dummy2_i8);

            SsInt8SetInt4(&nvalues_i8, nvalues);
            if (SsInt8Cmp(nvalues_i8, dummy_i8) > 0) {
                /* nvalues > values_per_range_i8 * avg_nrange,
                 * set nvalues = values_per_range_i8 * avg_nrange
                 */
                SsInt8ConvertToInt4(&nvalues, dummy_i8);
            }
        }

        ss_dprintf_4(("relh_selectivity_equalonerange:nvalues=%lu, samplediffperc=%d, ntuples_in_table=%lu, value_known=%d\n",
            (unsigned long)nvalues, samplediffperc, (unsigned long)ntuples_in_table_int4, value_known));

        SsInt8SetUint4(p_nvalues, nvalues);
}

/*#***********************************************************************\
 * 
 *              relh_selectivity_relop
 * 
 * Gets selectivity in number of tuples for relop.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      ano - 
 *              
 *              
 *      relop - 
 *              
 *              
 *      attrsamples - 
 *              
 *              
 *      samplesize - 
 *              
 *              
 *      i - 
 *              
 *              
 *      value - 
 *              
 *              
 *      p_nvalues - 
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
static void relh_selectivity_relop(
        void*       cd,
        rs_relh_t*  relh,
        rs_ano_t    ano,
        int         relop,
        relh_attrsamples_t* attrsamples,
        int         samplesize,
        int         i,
        va_t*       value,
        int         escchar,
        ss_int8_t*  p_nvalues)
{
        int nrange = 0;
        bool nvalues_found = FALSE;
        void* likepat;
        va_index_t likepatlen;

        switch (relop) {
            case RS_RELOP_EQUAL:
            case RS_RELOP_NOTEQUAL:
                ss_dprintf_4(("relh_getselectivity_relop:relop = %s\n",
                    relop == RS_RELOP_EQUAL ? "RS_RELOP_EQUAL" : "RS_RELOP_NOTEQUAL"));
                if (value == NULL) {
                    nrange = 0;
                } else {
                    for (nrange = 0; i < samplesize; i++, nrange++) {
                        if (va_compare(attrsamples[i].as_dva, value) != 0) {
                            break;
                        }
                    }
                }
                if (nrange <= 1) {
                    relh_selectivity_equalonerange(
                        cd,
                        relh,
                        ano,
                        relop,
                        samplesize,
                        value != NULL,
                        value != NULL
                            ? va_testnull(value)
                            : FALSE,
                        p_nvalues);
                    nvalues_found = TRUE;
                } else if (relop == RS_RELOP_NOTEQUAL) {
                    nrange = samplesize - nrange;
                }
                break;
            case RS_RELOP_LT:
            case RS_RELOP_LT_VECTOR:
                ss_dprintf_4(("relh_getselectivity_relop:relop = RS_RELOP_LT\n"));
                ss_dassert(value != NULL);
                nrange = i;
                break;
            case RS_RELOP_LE:
            case RS_RELOP_LE_VECTOR:
                ss_dprintf_4(("relh_getselectivity_relop:relop = RS_RELOP_LE\n"));
                ss_dassert(value != NULL);
                /* Count also all equal values. */
                for (; i < samplesize; i++) {
                    if (va_compare(attrsamples[i].as_dva, value) != 0) {
                        break;
                    }
                }
                nrange = i;
                break;
            case RS_RELOP_GT:
            case RS_RELOP_GT_VECTOR:
                ss_dprintf_4(("relh_getselectivity_relop:relop = RS_RELOP_GT\n"));
                ss_dassert(value != NULL);
                /* Skip all equal values. */
                for (; i < samplesize; i++) {
                    if (va_compare(attrsamples[i].as_dva, value) != 0) {
                        break;
                    }
                }
                nrange = samplesize - i;
                break;
            case RS_RELOP_GE:
            case RS_RELOP_GE_VECTOR:
                ss_dprintf_4(("relh_getselectivity_relop:relop = RS_RELOP_GE\n"));
                ss_dassert(value != NULL);
                nrange = samplesize - i;
                break;
            case RS_RELOP_LIKE:
                likepat = va_getdata(value, &likepatlen);
                nrange = 0;
                ss_dassert(likepatlen != 0);
                if (likepatlen != 0) {
                    for (i = 0; i < samplesize; i++) {
                        bool accept;
                        void* sampledata;
                        va_index_t sampledatalen;
                        sampledata = va_getdata(attrsamples[i].as_dva, &sampledatalen);
                        if (sampledatalen == 0) {
                            /* sampledatalen == 0 means NULL value */
                            accept = FALSE;
                        } else {
                            rs_ttype_t* relhttype;
                            rs_atype_t* relhatype;
                            rs_datatype_t dt;

                            relhttype = rs_relh_ttype(cd, relh);
                            relhatype = rs_ttype_atype(cd, relhttype, ano);
                            dt = rs_atype_datatype(cd, relhatype);

                            ss_rc_dassert(dt == RSDT_CHAR || dt == RSDT_UNICODE, dt);
                            if (dt == RSDT_UNICODE) {
                                ss_dassert(sampledatalen & 1);
                                ss_dassert(likepatlen & 1);
                                accept = su_wlike(
                                            sampledata,
                                            sampledatalen / sizeof(ss_char2_t),
                                            likepat,
                                            likepatlen / sizeof(ss_char2_t),
                                            escchar,
                                            TRUE);
                            } else

                            {
                                accept = su_slike(
                                            sampledata,
                                            sampledatalen-1,
                                            likepat,
                                            likepatlen-1,
                                            escchar);
                            }
                        }
                        if (accept) {
                            nrange++;
                        }
                    }
                }
                if (nrange <= 1) {
                    relh_selectivity_equalonerange(
                        cd,
                        relh,
                        ano,
                        relop,
                        samplesize,
                        value != NULL,
                        FALSE,
                        p_nvalues);
                    nvalues_found = TRUE;
                }
                break;
            default:
                ss_rc_error(relop);
        }

        ss_dprintf_4(("relh_getselectivity_relop:nrange=%d\n", nrange));

        if (!nvalues_found) {
            *p_nvalues = relh_values_in_range(cd, relh, samplesize, nrange);
        }
}

/*#***********************************************************************\
 * 
 *              relh_selectivity_range
 * 
 * Gets selectivity in number of tuples for a value range.
 * 
 * Parameters : 
 * 
 *      i - 
 *              
 *              
 *      value1 - 
 *              
 *              
 *      value1_closed - 
 *              
 *              
 *      value2 - 
 *              
 *              
 *      value2_closed - 
 *              
 *              
 *      pointlike - 
 *              
 *              
 *      p_nvalues - 
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
static void relh_selectivity_range(
        void*       cd,
        rs_relh_t*  relh,
        rs_ano_t    ano,
        relh_attrsamples_t* attrsamples,
        int         samplesize,
        int         i,
        va_t*       value1,
        bool        value1_closed,
        va_t*       value2,
        bool        value2_closed,
        bool        pointlike,
        ss_int8_t*  p_nvalues)
{
        int j;
        int nrange;

        ss_dprintf_3(("relh_selectivity_range:i=%d, value1_closed=%d, value2_closed=%d, pointlike=%d\n",
            i, value1_closed, value2_closed, pointlike));
        ss_dassert(value2 == NULL || va_compare(value1, value2) <= 0);

        if (!value1_closed) {
            /* Skip all equal values. */
            for (; i < samplesize; i++) {
                if (va_compare(attrsamples[i].as_dva, value1) != 0) {
                    break;
                }
            }
        }

        /* Find range end. */
        if (value2 == NULL) {
            /* Infinite range end. */
            j = samplesize;
        } else {
            for (j = i; j < samplesize; j++) {
                if (va_compare(attrsamples[j].as_dva, value2) >= 0) {
                    break;
                }
            }

            if (value2_closed) {
                /* Include all equal values. */
                for (; j < samplesize; j++) {
                    if (va_compare(attrsamples[j].as_dva, value2) != 0) {
                        break;
                    }
                }
            }
        }

        nrange = j - i;

        ss_dprintf_4(("relh_selectivity_range:range [%d,%d], nrange=%d, totalrange=%d\n",
            i, j, nrange, samplesize));
        ss_dassert(i <= j);

        if (pointlike && nrange <= 1) {
            relh_selectivity_equalonerange(
                cd,
                relh,
                ano,
                RS_RELOP_EQUAL,
                samplesize,
                TRUE,
                FALSE,
                p_nvalues);
        } else {
            *p_nvalues = relh_values_in_range(cd, relh, samplesize, nrange);
        }
}

/*#************************************************************************\
 * 
 *              relh_getselectivity
 * 
 * Returns approximate attribute selectivity in number of tuples
 * for a given relational operator or attribute value range.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in
 *              
 *              
 *      ano - in
 *              Attribute number.
 *              
 *      relop - in
 *              Comparison operator. Only operators RS_RELOP_EQUAL,
 *          RS_RELOP_NOTEQUAL, RS_RELOP_LT, RS_RELOP_LE, RS_RELOP_GT,
 *              RS_RELOP_GE, RS_RELOP_GE_VECTOR and RS_RELOP_GT_VECTOR are
 *          supported. If relop is -1, then value range selectivity is
 *              measured and value2 is range end (NULL for infinite range end).
 *              
 *      value1 - in
 *              Attribute value the selectivity of which is measured, or
 *              range begin value if relop is -1 amd value2 is not NULL.
 *              
 *      value1_closed - in
 *              In range, if TRUE value1 is closed range begin
 *              
 *      value2 - in
 *              If not NULL, attribute range end value the selectivity
 *          of which is measured.
 *              
 *      value2_closed - in
 *              In range, if TRUE value2 is closed range end
 *              
 *      pointlike - in
 *              In range, if TRUE range is pointlike range
 *              
 *      p_nvalues - out
 *              Selectivity in number of tuples is returned in *p_nvalues.
 *              
 * Return value : 
 * 
 *      TRUE    - selectivity calculated
 *      FALSE   - selectivity not available
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool relh_getselectivity(
        void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        int        relop,
        va_t*      value1,
        bool       value1_closed,
        va_t*      value2,
        bool       value2_closed,
        bool       pointlike,
        int        escchar,
        ss_int8_t* p_nvalues)
{
        int i;
        relh_attrsamples_t* attrsamples;
        void* elemptr;
        int samplesize;

        CHECK_RELH(relh);
        ss_dprintf_1(("relh_getselectivity:ano = %d\n", ano));
        ss_output_4(rs_relh_print(cd, relh));
        ss_dassert(p_nvalues != NULL);

#if 0 /* Pete changed from #ifdef SS_MME */
        if (relh->rh_reltype == RS_RELTYPE_MAINMEMORY) {
            SsInt8SetUint4(p_nvalues, 1000);
            return FALSE;
        }
#endif

        SsSemEnter(relh->rh_samplesem);

        if (!su_pa_indexinuse(relh->rh_attrsamples, ano)) {
            SsSemExit(relh->rh_samplesem);
            ss_dprintf_2(("relh_getselectivity:no info available\n"));
            SsInt8SetUint4(p_nvalues, 1000);
            return(FALSE);
        }

        attrsamples = su_pa_getdata(relh->rh_attrsamples, ano);
        ss_dassert(su_pa_indexinuse(relh->rh_samplesizes, ano));
        samplesize = (int)su_pa_getdata(relh->rh_samplesizes, ano);

        if (relop == RS_RELOP_LIKE) {
            ss_dassert(value1 != NULL);
            ss_dassert(value2 == NULL);
            i = 0;
        } else if (value1 == NULL) {
            ss_dassert(relop != -1);
            i = 0;
        } else {
            su_bsearch(
                value1,
                attrsamples,
                samplesize,
                sizeof(attrsamples[0]),
                sample_search_cmp,
                &elemptr);

            ss_dassert((char*)elemptr >= (char*)attrsamples);

            i = ((char*)elemptr - (char*)attrsamples) / sizeof(attrsamples[0]);
            ss_dassert(i <= samplesize);
            ss_dassert(attrsamples + i == (relh_attrsamples_t*)elemptr);

            if (i < samplesize) {
                /* Move to the first equal value.
                */
                while (i > 0 && va_compare(value1, attrsamples[i-1].as_dva) == 0) {
                    i--;
                }
            }
        }

        if (relop == -1) {
            /* Range selectivity case.
             */
            ss_dassert(value1 != NULL);
            relh_selectivity_range(
                cd,
                relh,
                ano,
                attrsamples,
                samplesize,
                i,
                value1,
                value1_closed,
                value2,
                value2_closed,
                pointlike,
                p_nvalues);

        } else {
            /* Relop selectivity case.
             */
            relh_selectivity_relop(
                cd,
                relh,
                ano,
                relop,
                attrsamples,
                samplesize,
                i,
                value1,
                escchar,
                p_nvalues);
        }
/*
        ss_dprintf_2(("relh_getselectivity:relop nvalues = %ld, relh nvalues = %ld\n",
            *p_nvalues, rs_cardin_ntuples(cd, relh->rh_cardin)));
*/

        SsSemExit(relh->rh_samplesem);

        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              rs_relh_getrelopselectivity
 * 
 * Returns approximate attribute selectivity in number of tuples
 * for a given relational operator.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in
 *              
 *              
 *      ano - in
 *              Attribute number.
 *              
 *      relop - in
 *              Comparison operator. Only operators RS_RELOP_EQUAL,
 *          RS_RELOP_NOTEQUAL, RS_RELOP_LT, RS_RELOP_LE, RS_RELOP_GT,
 *              RS_RELOP_GE, RS_RELOP_GE_VECTOR and RS_RELOP_GT_VECTOR are
 *          supported.
 *              
 *      value - in
 *              Attribute value the selectivity of which is measured.
 *              
 *      p_nvalues - out
 *              Selectivity in number of tuples is returned in *p_nvalues.
 *              
 * Return value : 
 * 
 *      TRUE    - selectivity calculated
 *      FALSE   - selectivity not available
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool rs_relh_getrelopselectivity(
        void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        int        relop,
        va_t*      value,
        int        escchar,
        ss_int8_t* p_nvalues)
{
        ss_dprintf_1(("rs_relh_getrelopselectivity\n"));

        return(relh_getselectivity(
                cd,
                relh,
                ano,
                relop,
                value,
                FALSE,
                NULL,
                FALSE,
                FALSE,
                escchar,
                p_nvalues));
}

/*##**********************************************************************\
 * 
 *              rs_relh_getrangeselectivity
 * 
 * Returns approximate attribute selectivity in number of tuples
 * for a va range.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in
 *              
 *              
 *      ano - in
 *              Attribute number.
 *              
 *      range_start - in
 *              Range begin attribute value the selectivity of which is measured.
 *              
 *      range_start_closed - in
 *              TRUE if range start is closed 
 *              
 *      range_end - in
 *              Range end attribute value the selectivity of which is measured.
 *              
 *      range_end_closed - in
 *              TRUE if range end is closed 
 *              
 *      pointlike - in
 *              TRUE if range is pointlike
 *              
 *      p_nvalues - out
 *              Selectivity in number of tuples is returned in *p_nvalues.
 *              
 * Return value : 
 * 
 *      TRUE    - selectivity calculated
 *      FALSE   - selectivity not available
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool rs_relh_getrangeselectivity(
        void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        va_t*      range_start,
        bool       range_start_closed,
        va_t*      range_end,
        bool       range_end_closed,
        bool       pointlike,
        ss_int8_t* p_nvalues)
{
        ss_dprintf_1(("rs_relh_getrangeselectivity\n"));

        return(relh_getselectivity(
                cd,
                relh,
                ano,
                -1,
                range_start,
                range_start_closed,
                range_end,
                range_end_closed,
                pointlike,
                SU_SLIKE_NOESCCHAR,
                p_nvalues));
}

/*##**********************************************************************\
 * 
 *              rs_relh_getdiffrowcount
 * 
 * Return estimate of how many different rows are returned for given ano.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      ano - 
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
long rs_relh_getdiffrowcount(
        void*      cd __attribute__ ((unused)),
        rs_relh_t* relh,
        rs_ano_t   ano,
        long       ano_rowcount)
{
        long samplediff;
        long samplesize;
        double perc_nrows;
        double factor;
        long maxdiffrows;
        long rowcount;

        CHECK_RELH(relh);

        SsSemEnter(relh->rh_samplesem);

        if (!su_pa_indexinuse(relh->rh_samplediffs, ano)) {
            rowcount = ano_rowcount / 2;

        } else {
            samplediff = (long)su_pa_getdata(relh->rh_samplediffs, ano);
            samplesize = (long)su_pa_getdata(relh->rh_samplesizes, ano);
        
            factor = (double)samplediff / (double)samplesize;
            perc_nrows = factor * (double)ano_rowcount;

            rowcount = samplediff +
                       (long)(factor*factor*factor*factor*perc_nrows);

            if (rowcount > ano_rowcount) {
                rowcount = ano_rowcount;
            }
            if (su_pa_indexinuse(relh->rh_maxdiffrows, ano)) {
                maxdiffrows = (long)su_pa_getdata(relh->rh_maxdiffrows, ano);
                if (maxdiffrows < rowcount) {
                    rowcount = maxdiffrows;
                }
            }
        }

        SsSemExit(relh->rh_samplesem);

        ss_dprintf_1(("rs_relh_getdiffrowcount:ano=%d, rowcount=%ld\n", ano, rowcount));

        return(rowcount);
}

#endif /* SS_NOESTSAMPLES */

/*##**********************************************************************\
 * 
 *              rs_relh_settrigger
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      index - 
 *              
 *              
 *      str - 
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
void rs_relh_settrigger(
        void* cd __attribute__ ((unused)),
        rs_relh_t* relh,
        int index,
        rs_entname_t* name,
        char* str)
{
        relh_trig_t* trig;

        ss_dprintf_1(("%s:rs_relh_settrigger:%d %.40s\n", __FILE__, index, str));
        CHECK_RELH(relh);

        trig = SSMEM_NEW(relh_trig_t);

        trig->t_name = rs_entname_copy(name);
        trig->t_str = SsMemStrdup(str);

        ss_dassert(!su_pa_indexinuse(relh->rh_triggers, index));

        su_pa_insertat(relh->rh_triggers, index, trig);
}

/*##**********************************************************************\
 * 
 *              rs_relh_triggercount
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
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
int rs_relh_triggercount(
        rs_relh_t* relh)
{
        CHECK_RELH(relh);
        return(su_pa_nelems(relh->rh_triggers));
}

void rs_relh_modifyttypeifsystable(
        void* cd,
        rs_relh_t* relh)
{
        rs_ttype_t* ttype;
        uint nattrs;
        uint ano;

        CHECK_RELH(relh);
        if (!rs_relh_issysrel(cd, relh)) {
            return;
        }
        ttype = relh->rh_ttype;
        nattrs = rs_ttype_nattrs(cd, ttype);
        for (ano = 0; ano < nattrs; ano++) {
            rs_atype_t* atype;
            rs_atype_t* new_atype;
            long atype_len;
            rs_sqldatatype_t sqldt;

            atype = rs_ttype_atype(cd, ttype, ano);
            if (!rs_atype_isuserdefined(cd, atype)
            ||  rs_atype_pseudo(cd, atype))
            {
                continue;
            }
            sqldt = rs_atype_sqldatatype(cd, atype);
            switch (sqldt) {
                case RSSQLDT_CHAR:
                case RSSQLDT_VARCHAR:
                case RSSQLDT_LONGVARCHAR:
                    atype_len = rs_atype_length(cd, atype);
                    if (atype_len != RS_LENGTH_NULL) {
                        new_atype = rs_atype_initbysqldt(cd, sqldt, atype_len * 2, -1L);
                        ss_debug(rs_ttype_setreadonly(cd, ttype, FALSE));
                        rs_ttype_setatype(cd, ttype, ano, new_atype);
                        ss_debug(rs_ttype_setreadonly(cd, ttype, TRUE));
                        rs_atype_free(cd, new_atype);
                    }
                    break;
                default:
                    break;
            }
        }
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *              rs_relh_print
 * 
 * Prints relation handle using SsDbgPrintf.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_relh_print(cd, relh)
        void*       cd;
        rs_relh_t*  relh;
{
        uint i;

        SsDbgPrintf("RELATION:\n");
        SsDbgPrintf("---------\n");
        SsDbgPrintf("%-20s %-20s %-5s %s\n", "SCHEMA", "NAME", "ID", "CHECKSTR");
        SsDbgPrintf("%-20s %-20s %5ld\n",
            rs_entname_getschema(relh->rh_name),
            rs_entname_getname(relh->rh_name),
            relh->rh_relid);

        SsDbgPrintf("\nTUPLE TYPE:\n");
        SsDbgPrintf("-----------\n");
        rs_ttype_print(cd, relh->rh_ttype);

        su_pa_do(relh->rh_key_pa, i) {
            SsDbgPrintf("\nKEY %2d:\n", i);
            SsDbgPrintf("-------\n");
            rs_key_print(cd, su_pa_getdata(relh->rh_key_pa, i));
        }
        su_pa_do(relh->rh_refkey_pa, i) {
            SsDbgPrintf("\nREFKEY %2d:\n", i);
            SsDbgPrintf("-------\n");
            rs_key_print(cd, su_pa_getdata(relh->rh_refkey_pa, i));
        }
}

/*##**********************************************************************\
 * 
 *              rs_relh_check
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
void rs_relh_check(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
}

#endif /* SS_DEBUG */

#if 0

bool rs_relh_setlock(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        rs_relh_lock_t locktype,
        su_err_t** p_errh)
{
        bool succp = TRUE;
        int waitcount = 0;

        CHECK_RELH(relh);

        do {
            SsSemEnter(relh->rh_sem);

            switch (locktype) {
                case RS_RELH_LOCK_SHARED_READ:
                    switch (relh->rh_locktype) {
                        case RS_RELH_LOCK_SHARED_READ:
                        case RS_RELH_LOCK_SHARED_WRITE:
                        case RS_RELH_LOCK_UPDATE:
                            relh->rh_sharecount++;
                            break;
                        case RS_RELH_LOCK_EXCLUSIVE:
                            su_err_init(p_errh, E_DDOP);
                            succp = FALSE;
                            break;
                        default:
                            ss_rc_error(locktype);
                    }
                    break;
                case RS_RELH_LOCK_SHARED_WRITE:
                    switch (relh->rh_locktype) {
                        case RS_RELH_LOCK_SHARED_READ:
                        case RS_RELH_LOCK_SHARED_WRITE:
                            relh->rh_sharecount++;
                            break;
                        case RS_RELH_LOCK_UPDATE:
                        case RS_RELH_LOCK_EXCLUSIVE:
                            su_err_init(p_errh, E_DDOP);
                            succp = FALSE;
                            break;
                        default:
                            ss_rc_error(locktype);
                    }
                    break;
                case RS_RELH_LOCK_UPDATE:
                case RS_RELH_LOCK_EXCLUSIVE:
                    switch (relh->rh_locktype) {
                        case RS_RELH_LOCK_SHARED_READ:
                        case RS_RELH_LOCK_SHARED_WRITE:
                            relh->rh_locktype = locktype;
                            relh->rh_lockuserid = rs_sysi_userid(cd);
                            /* FALLTHROUGH */
                        case RS_RELH_LOCK_UPDATE:
                        case RS_RELH_LOCK_EXCLUSIVE:
                            if (relh->rh_lockuserid != rs_sysi_userid(cd)) {
                                su_err_init(p_errh, E_DDOP);
                                succp = FALSE;
                            } else if (relh->rh_sharecount > 0) {
                                waitcount++;
                            } else {
                                waitcount = 0;
                                relh->rh_locktype = SS_MAX(locktype,
                                                           relh->rh_locktype);
                            }
                            break;
                        default:
                            ss_rc_error(locktype);
                    }
                    break;
                default:
                    ss_rc_error(locktype);
            }

            SsSemExit(relh->rh_sem);

#ifdef SS_MT
            if (waitcount > 0) {
                if (waitcount > 600) {
                    /* 600 * 100ms = 1 minute */
                    succp = FALSE;
                    su_err_init(p_errh, SRV_ERR_OPERTIMEOUT);
                } else {
                    SsThrSleep(100L);
                }
            }
#else /* SS_MT */
            ss_assert(!waitp);
#endif /* SS_MT */

        } while (waitcount > 0 && succp);

        return(TRUE);
}

void rs_relh_freelock(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        rs_relh_lock_t locktype)
{
        CHECK_RELH(relh);

        SsSemEnter(relh->rh_sem);

        switch (locktype) {
            case RS_RELH_LOCK_SHARED_READ:
            case RS_RELH_LOCK_SHARED_WRITE:
                ss_bassert(relh->rh_sharecount > 0);
                relh->rh_sharecount--;
                break;
            case RS_RELH_LOCK_UPDATE:
                ss_bassert(relh->rh_lockuserid == rs_sysi_userid(cd));
                relh->rh_locktype = RS_RELH_LOCK_SHARED_READ:
                break;
            case RS_RELH_LOCK_EXCLUSIVE:
                ss_rc_error(locktype);
            default:
                ss_rc_error(locktype);
        }

        SsSemExit(relh->rh_sem);
}

#endif /* 0 */

#ifdef SS_MME

rs_ano_t rs_relh_getdifferentiatingano(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_relh_t*      relh)
{
        rs_ano_t        ano;
        rs_ano_t        nattrs;
        rs_atype_t*     atype;

        if (relh->rh_differentiatingano != RS_ANO_NULL) {
            return relh->rh_differentiatingano;
        }

        nattrs = rs_ttype_nattrs(cd, relh->rh_ttype);
        /* Let's try to differentiate by tuple id if it exists.  This is
           more efficient since the tuple id doesn't change in updates. */
        for (ano = 0; ano < nattrs; ano++) {
            atype = rs_ttype_atype(cd, relh->rh_ttype, ano);
            if (rs_atype_attrtype(cd, atype) == RSAT_TUPLE_ID) {
                break;
            }
        }
        if (ano == nattrs) {
#ifdef SS_ALWAYS_HAVE_TUPLE_ID
            ss_derror;
#endif
            for (ano = 0; ano < nattrs; ano++) {
                atype = rs_ttype_atype(cd, relh->rh_ttype, ano);
                if (rs_atype_attrtype(cd, atype) == RSAT_TUPLE_VERSION) {
                    break;
                }
            }
        }
        ss_dassert(ano < nattrs);

        relh->rh_differentiatingano = ano;

        return ano;
}

#endif /* SS_MME */

/*##**********************************************************************\
 * 
 *              rs_relh_readautoincrement_value
 * 
 * Read the value of the autoincrement field. Note that this method
 * does not reserve mutex.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in, use
 *              
 *              
 * Return value : 
 *
 * ss_int8 value of the autoincrement field
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */

long rs_relh_readautoincrement_seqid(
        void*      cd,
        rs_relh_t* relh)
{
        long       value;
        
        CHECK_RELH(relh);
        SS_NOTUSED(cd);

        if (_RS_RELH_ISAUTOINCINITED_(cd, relh)) {
            value = relh->rh_autoincseqid;
        } else {
            value = 0;
        }

        return (value);
}

/*##**********************************************************************\
 * 
 *              rs_relh_setautoincrement_seqid
 * 
 * Set the value of the autoincrement sequence identifier.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in, use
 *
 *      seq_id - in, use
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

void rs_relh_setautoincrement_seqid(
        void*      cd,
        rs_relh_t* relh,
        long       seq_id)
{
        CHECK_RELH(relh);
        SS_NOTUSED(cd);

        if (_RS_RELH_ISAUTOINCINITED_(cd, relh)) {
            relh->rh_autoincseqid = seq_id;
        }
}

/*##**********************************************************************\
 * 
 *              rs_relh_initautoincrement
 * 
 * Initialize the value of the autoincrement field if the field has not
 * been initialized. Note that this method does not reserve mutex.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in, use
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

void rs_relh_initautoincrement(
        void*      cd,
        rs_relh_t* relh)
{
        CHECK_RELH(relh);
        SS_NOTUSED(cd);

        if (!(_RS_RELH_ISAUTOINCINITED_(cd, relh))) {
            relh->rh_autoincseqid = 0;
            relh->rh_isautoincinited = TRUE;
        }
}

#ifndef rs_relh_isautoincinited
bool rs_relh_isautoincinited(
        void*       cd __attribute__ ((unused)),
        rs_relh_t*  relh)
{
        CHECK_RELH(relh);

        return _RS_RELH_ISAUTOINCINITED_(cd, relh);
}
#endif
