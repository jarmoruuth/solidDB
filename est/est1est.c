/*************************************************************************\
**  source       * est1est.c
**  directory    * est
**  description  * The index search cost estimation and key selection
**               * module. Produces estimates on the number of rows
**               * returned by a query, on the time used and gives also
**               * information on the ordering of the rows.
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

Future improvements:
-------------------
1. The functions db_info_blocksize, db_info_poolsize,
rs_relh_nbytes, rs_relh_nrows should be implemented to get the estimator
as cost-based.

2. The constants used in the estimation process should be tuned.

3. The range selectivity should be measured at least for
   large queries. This could be done in the following way:
   the function range_selectivity_for_key could be extended
   so that in relevant cases it would call form_range_constraint
   from est1pla.c and then a measuring procedure in the engine.

4. The SQL interpreter should accept also float values
   as estimates.

5. In joins there are many similar searches. Then we could
   approximate hit rate better.


Implementation:
--------------
The module can be run in a test mode by calling the function
tb_est_initialize_test(nrows).

There is a document in file tabkeyse.doc in the documentation directory.

Limitations:
-----------
Currently the module does not use any kind of information of the size of
the tables and ranges in the database.

Error handling:
--------------
No legal errors can be noticed here. All errors cause an assert.

Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------
In test mode the previous estimate object is stored
to a global variable and left as garbage.

Example:
-------
The main function is tb_est_create_estimate.

**************************************************************************
#endif /* DOCUMENTATION */

/* The mystic cd parameter in each function is a value which
can be passed to all deeper levels in the database.
*/

#define TAB1EST_C
    
#include <ssstdio.h>
#include <math.h>

#include <ssdebug.h>
#include <ssmem.h>
#include <sssprint.h>
#include <ssdtoa.h>
#include <sslimits.h>

#include <uti0vtpl.h>

#include <su0list.h>
#include <su0parr.h>
#include <su0prof.h>
#include <su0task.h>

#include <rs0cons.h>
#include <rs0types.h>
#include <rs0sysi.h>
#include <rs0key.h>
#include <rs0aval.h>
#include <rs0order.h>
#include <rs0relh.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0sysi.h>
#include <rs0sqli.h>

#include <dbe0db.h>
#include <dbe0trx.h>

#ifdef SS_MYSQL
#include <tab1defs.h>
#include <tab0tran.h>
#include <tab0info.h>
#include <tab0conn.h>
#else
#include "../tab/tab1defs.h"
#include "../tab/tab0tran.h"
#include "../tab/tab0info.h"
#include "../tab/tab0conn.h"
#endif

#include "est1est.h"
#include "est1pla.h"

typedef enum {
        EST_RANGE_NONE  = 0,
        EST_RANGE_BEGIN = SU_BFLAG_BIT(0),
        EST_RANGE_END   = SU_BFLAG_BIT(1)
} est_rangetype_t;

typedef struct {
        su_list_t*  cons_list;      /* Constraint list. */
        su_pa_t*    cons_byano;     /* Lists of constraints by each ano. */
        bool        cons_fixedest;  /* If TRUE, there are too few rows in
                                       table, fixed selectivity estimates
                                       are used. */
        bool        cons_rangeest;  /* If TRUE, use range estimates. */
        double*     cons_selectivarr;
        int         cons_selectivarrsize;
        int         cons_selectivarrcount;
        rs_estcost_t* cons_rowcounts;
        double      cons_relntuples;
        uint        cons_vectoroptn;
        uint        cons_nvectorconstraint;
        uint        cons_infolevel;
} cons_info_t;

typedef struct {
        int             usi_relid;
        tb_database_t*  usi_tdb;
        rs_sysi_t*      usi_cd;
} update_selectivity_info_t;

/***********************************************************************
 HERE START THE CONSTANTS WHICH SHOULD BE TUNED FOR REAL-WORLD
 APPLICATIONS
************************************************************************/

/* CONSTANTS ARE MOVED TO RS0SQLI.C. */

#if 0

/* The estimated selectivity for different types of search constraints */
static double est_equal_selectivity     = (1.0/100.0);
static double est_notequal_selectivity  = (9.0/10.0);
static double est_compare_selectivity   = (1.0/5.0); /* selectivity
                                                        for <, <= etc. */
static double est_like_selectivity      = (1.0/30.0);
static double est_isnull_selectivity    = (1.0/3.0);
static double est_isnotnull_selectivity = (2.0/3.0);
static double est_no_selectivity        = (1.0);


/* estimated default hit rate for an index block
 */
static double est_hit_rate_for_index    = (80.0/100.0);

/* estimated default hit rate for a data block
 */
static double est_hit_rate_for_data     = (80.0/100.0);

/* estimated minimum data size per a single index size
 */
static double est_data_size_per_index   = 4.0;

/* estimated maximum key entry size in bytes
 */
static double est_max_key_entry_size    = 30.0;

/* if a secondary index is prejoined, then the following parameter
 * tells how densely are the relevant entries there
 */
static double est_prejoin_density       = (1.0/3.0);

/* time for a B+-tree search in microseconds
 */
static double est_time_for_index_search = 1000.0;

/* time for processing a single index entry in a block in microseconds
 */
static double est_time_per_index_entry  = 300.0;

/* time for random access of a disk block in microseconds
 */
static double est_block_access_time     = 25000.0;

#endif /* 0 */

#define MAINMEM_BLOCK_ACCESS_TIME   1

/***********************************************************************
 HERE END THE CONSTANTS WHICH SHOULD BE TUNED FOR REAL-WORLD
 APPLICATIONS
************************************************************************/

/***********************************************************************
 MACROS WHICH SHOULD NOT BE CHANGED IN TUNING PROCESS
************************************************************************/

/* The lowest possible selectivity. This is used to prevent underflow
 * and division by zero.
 */
#define EST_MIN_SELECTIVITY     (1E-20)

/* The longest possible delay time per row in microseconds. This
 * is used to prevent overflow when giving the delay parameters
 * to the SQL interpreter.
 */
#define EST_MAX_DELAY_TIME      (3.6E9)

/* The default sizes used for an empty table. This is to prevent
 * division by zero.
 */
#define EST_N_ROWS_IN_EMPTY_TABLE  1L
#define EST_N_BYTES_IN_EMPTY_TABLE 100L

/***********************************************************************
 HERE END THE MACROS WHICH SHOULD NOT BE CHANGED IN TUNING PROCESS
************************************************************************/


/* Local function prototypes */
static bool data_sample_selectivity_range(
        rs_sysi_t*      cd,
        rs_relh_t*      relh,
        cons_info_t*    cons_info,
        rs_ano_t        ano,
        dynvtpl_t       range_start,
        bool            range_start_closed,
        dynvtpl_t       range_end,
        bool            range_end_closed,
        bool            pointlike,
        double*         p_selectivity);
static double range_selectivity_for_key(
        rs_sysi_t*   cd,
        rs_relh_t*   relh,
        rs_key_t*    key,
        cons_info_t* cons_info,
        bool*        unique_value,
        bool*        full_scan);
static double total_selectivity_for_key(
        rs_sysi_t*   cd,
        rs_relh_t*   relh,
        rs_key_t*    key,
        cons_info_t* cons_info,
        long*        n_solved_constraints);
static double global_selectivity(
        rs_sysi_t*   cd,
        rs_relh_t*   relh,
        cons_info_t* cons_info);
static double range_selectivity_for_constr(
        rs_sysi_t*      cd,
        rs_relh_t*      relh,
        cons_info_t*    cons_info,
        rs_cons_t*      constraint,
        bool            ascp,
        est_rangetype_t rangetype,
        bool*           p_isrange);
static double selectivity_for_constraint(
        rs_sysi_t*   cd,
        rs_relh_t*   relh,
        cons_info_t* cons_info,
        rs_cons_t*   constraint);
static bool contains_select_list(
        rs_sysi_t*  cd,
        rs_key_t*   key,
        int*        select_list);
static void time_estimate_for_key(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   table,
        rs_key_t*    key,
        cons_info_t* cons_info,
        long         n_matching_order_bys,
        int*         select_list,
        double       global_sel,
        double*      total_search_time,
        double*      delay_at_start,
        double*      average_delay_per_row,
        double*      n_rows_in_result_set,
        bool*        unique_value,
        bool*        full_scan,
        bool*        must_retrieve);
static bool is_equal_constraint(
        rs_sysi_t*   cd,
        cons_info_t* cons_info,
        uint         column_no,
        bool         non_null);
static long db_info_poolsize(
        rs_sysi_t*   cd);
static long db_info_blocksize(
        rs_sysi_t*   cd);
static bool update_selectivity_info(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        double relntuples,
        int sample_ano,
        bool force_update);

/***********************************************************************
 VARIABLES AND FUNCTIONS FOR THE TEST VERSION MODE
************************************************************************/

/* This flag is set TRUE in the test program tstest. */
static bool         est_test_version_on = FALSE;
static bool         est_selectivity_test_on = FALSE;

static long         est_test_poolsize;
static long         est_test_blocksize;

static long         est_test_n_rows;
static long         est_test_n_bytes;
static tb_est_t*    est_test_est;

static void selectivity_add(
        cons_info_t* ci,
        double selectivity,
        uint ano)
{
        rs_estcost_t nvalues;

        ss_dprintf_4(("selectivity_add:ano=%d, selectivity=%lf\n", ano, selectivity));

        nvalues = selectivity * ci->cons_relntuples;
        if (ci->cons_rowcounts[ano] == 0 ||
            ci->cons_rowcounts[ano] > nvalues) {
            if (nvalues < 1) {
                nvalues = 1;
            }
            ci->cons_rowcounts[ano] = nvalues;
        }

        if (ci->cons_selectivarrsize == ci->cons_selectivarrcount) {
            /* Allocate more space for selectivities.
             */
            ci->cons_selectivarrsize += 10;
            ci->cons_selectivarr = SsMemRealloc(
                                        ci->cons_selectivarr,
                                        ci->cons_selectivarrsize
                                        * sizeof(ci->cons_selectivarr[0]));
        }
        ci->cons_selectivarr[ci->cons_selectivarrcount] = selectivity;
        ci->cons_selectivarrcount++;
}

static int SS_CLIBCALLBACK selectivity_qsortcmp(const void* s1, const void* s2)
{
        double d1 = *(double*)s1;
        double d2 = *(double*)s2;

        /* Sort in reverse order. */
        if (d1 < d2) {
            return (1);
        } else if (d1 > d2) {
            return (-1);
        } else {
            return (0);
        }
}

static double selectivity_get(
        rs_sysi_t* cd,
        cons_info_t* ci)
{
        int i;
        double selectivity;
        double selectivity_drop_limit;

        if (ci->cons_selectivarrcount == 0) {
            return(1.0);
        }

        qsort(
            ci->cons_selectivarr,
            ci->cons_selectivarrcount,
            sizeof(ci->cons_selectivarr[0]),
            selectivity_qsortcmp);

        selectivity = 1.0;
        selectivity_drop_limit = rs_sqli_selectivity_drop_limit(rs_sysi_sqlinfo(cd));

        for (i = 0; i < ci->cons_selectivarrcount; i++) {
            double min_selectivity;
            min_selectivity = SS_MIN(selectivity, ci->cons_selectivarr[i]);
            selectivity *= ci->cons_selectivarr[i];
            if (selectivity < selectivity_drop_limit * min_selectivity) {
                /* Do not let combined selectivity drop too fast.
                 */
                selectivity = selectivity_drop_limit * min_selectivity;
            }
            if (selectivity < EST_MIN_SELECTIVITY) {
                selectivity = EST_MIN_SELECTIVITY;
            }
        }
        ci->cons_selectivarrcount = 0;
        ss_dprintf_4(("selectivity_get:selectivity=%lf\n", selectivity));
        return(selectivity);
}

/*##**********************************************************************\
 *
 *		tb_est_initialize_test
 *
 * Initializes the constants for use in the test mode.
 *
 * Parameters :
 *
 *	n_rows - in, use
 *               number of rows in the test tables
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_est_initialize_test(cd, n_rows)
        rs_sysi_t* cd;
        long n_rows;
{

/******** WARNING!!!!!!!!!!!!! Do not change the test version's values
for repeatability of tests. ******************************************/

        /* PART OF SETTINGS MOVED TO RS0SQLI.C. */

        est_test_version_on       = TRUE;
        est_selectivity_test_on   = FALSE;

        est_test_poolsize         = 1000000L;
        est_test_blocksize        = 8192L;

        est_test_n_rows           = n_rows;
        est_test_n_bytes          = 100L * n_rows;
#if 0
        est_equal_selectivity     = (1.0/100.0);
        est_notequal_selectivity  = (9.0/10.0);
        est_compare_selectivity   = (1.0/5.0); /* selectivity
                                                       for <, <= etc. */
        est_like_selectivity      = (1.0/30.0);
        est_isnull_selectivity    = (1.0/3.0);
        est_isnotnull_selectivity = (2.0/3.0);
        est_no_selectivity        = (1.0);

        est_hit_rate_for_index    = (80.0/100.0);
        est_hit_rate_for_data     = (80.0/100.0);
        est_data_size_per_index   = 4.0;
        est_max_key_entry_size    = 30.0;
        est_prejoin_density       = (1.0/3.0);
        est_time_for_index_search = 1000.0;
        est_time_per_index_entry  = 300.0;
        est_block_access_time     = 25000.0;

#else /* 0 */

        rs_sqli_settestmode(rs_sysi_sqlinfo(cd));

#endif /* 0 */
}

/*##**********************************************************************\
 *
 *		tb_est_init_selectivity_test
 *
 * Initializes a selectivity test mode.
 *
 * Parameters :
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_est_init_selectivity_test(void)
{
        est_test_version_on       = FALSE;
        est_selectivity_test_on   = TRUE;
}

/*##**********************************************************************\
 *
 *		tb_est_get_est
 *
 * Returns the last estimate for testing purposes.
 *
 * Parameters : 	 - none
 *
 * Return value : out, ref: the previous estimate
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_est_t* tb_est_get_est(void)
{
        return(est_test_est);
}

/***********************************************************************
 VARIABLES AND FUNCTIONS FOR THE TEST VERSION MODE END HERE
************************************************************************/

/*#***********************************************************************\
 *
 *		db_info_poolsize
 *
 * The buffer pool size in bytes for a single query.
 *
 * Parameters :
 *
 *	cd - in, use
 *
 *
 * Return value :
 *      the pool size
 *
 * Limitations  :
 *
 * Globals used :
 */
static long db_info_poolsize(cd)
        rs_sysi_t*  cd;
{
        long poolsize;

        if (est_test_version_on) {
            poolsize = est_test_poolsize;
        } else {
            poolsize = dbe_db_poolsizeforquery(rs_sysi_db(cd));
        }
        ss_dprintf_4(("db_info_poolsize:poolsize=%ld\n", poolsize));
        return(poolsize);
}

/*#***********************************************************************\
 *
 *		db_info_blocksize
 *
 * The block size in bytes
 *
 * Parameters :
 *
 *	cd - in, use
 *
 *
 * Return value :
 *      block size
 *
 * Limitations  :
 *
 * Globals used :
 */
static long db_info_blocksize(cd)
        rs_sysi_t* cd;
{
        if (est_test_version_on) {
            return(est_test_blocksize);
        } else {
            return(dbe_db_blocksize(rs_sysi_db(cd)));
        }
}

#ifndef SS_NOESTSAMPLES

static void est_ensureselectivityinfo(rs_sysi_t* cd, rs_relh_t* relh, bool force)
{
        ss_int8_t ntuples_i8;
        double ntuples;

        ss_dprintf_3(("est_ensureselectivityinfo\n"));

        ntuples_i8 = rs_relh_ntuples(cd, relh);
        SsInt8ConvertToDouble(&ntuples, ntuples_i8);

        if (!rs_sysi_simpleoptimizerrules(cd, ntuples)) {
            update_selectivity_info(cd, relh, ntuples, -1, force);
        }
}

static void get_attribute_selectivity(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        rs_ano_t ano,
        dynvtpl_t* sample_vtpl,
        int* sample_estimate,
        uint sample_size)
{
        int i;
        int kpno;
        int clustkpno;
        su_pa_t* keys;
        rs_key_t* key;
        bool found = FALSE;
        va_t va;
        dbe_db_t* db;
        rs_key_t* clustkey;

        db = rs_sysi_db(cd);
        keys = rs_relh_keys(cd, relh);

        su_pa_do_get(keys, i, key) {
            for (kpno = 0; ; kpno++) {
                if (!rs_keyp_isconstvalue(cd, key, kpno)) {
                    break;
                }
            }
            if (rs_keyp_ano(cd, key, kpno) == ano) {
                found = TRUE;
                break;
            }
        }
        if (!found) {
            memset(sample_estimate, '\0', sample_size * sizeof(sample_estimate[0]));
            return;
        }

        /* We have found a key where column is the first key part.
         */

        clustkey = rs_relh_clusterkey(cd, relh);
        clustkpno = rs_key_searchkpno_data(cd, clustkey, ano);
        ss_dassert(clustkpno != RS_ANO_NULL);

        va_setlong(&va, rs_key_id(cd, key));

        for (i = 0; i < sample_size; i++) {
            dynvtpl_t range_min = NULL;
            dynvtpl_t range_max = NULL;
            vtpl_t* clustkeyvtpl;

            /* Generate sample range minimum.
             */
            dynvtpl_setvtpl(&range_min, VTPL_EMPTY);
            dynvtpl_appva(&range_min, &va);
            clustkeyvtpl = sample_vtpl[i];
            dynvtpl_appva(&range_min, vtpl_getva_at(clustkeyvtpl, clustkpno));

            /* Generate sample range maximum.
             */
            dynvtpl_setvtplwithincrement(&range_max, range_min);

            sample_estimate[i] = dbe_db_getequalrowestimate(cd, db, range_min, range_max);

            dynvtpl_free(&range_min);
            dynvtpl_free(&range_max);
        }
}

static int update_selectivity_info_task(su_task_t* t, void* td)
{
        update_selectivity_info_t* usi = td;
        tb_connect_t* tbc;
        rs_rbuf_t* rbuf;
        rs_relh_t* relh;
        rs_entname_t* relname;
        rs_rbuf_present_t rp;
        ulong relid;
        bool b;

        ss_pprintf_3(("update_selectivity_info_task\n"));

        tbc = tb_sysconnect_initbycd(usi->usi_tdb, usi->usi_cd);
        rbuf = rs_sysi_rbuf(usi->usi_cd);

        b = rs_rbuf_relnamebyid(usi->usi_cd, rbuf, usi->usi_relid, &relname);
        if (b) {
            ss_dprintf_4(("update_selectivity_info_task:relname found\n"));
            relh = NULL;
            rp = rs_rbuf_relpresent(usi->usi_cd, rbuf, relname, &relh, &relid);
            if (rp == RSRBUF_BUFFERED && relh != NULL && relid == usi->usi_relid) {
                ss_dprintf_4(("update_selectivity_info_task:buffered with the same id\n"));
                est_ensureselectivityinfo(usi->usi_cd, relh, TRUE);
            }
            if (relh != NULL) {
                rs_relh_done(usi->usi_cd, relh);
            }
            rs_entname_done(relname);
        }

        tb_sysconnect_done(tbc);
        rs_sysi_done(usi->usi_cd);
        SsMemFree(usi);

        ss_pprintf_4(("update_selectivity_info_task:stop\n"));

        return(SU_TASK_STOP);
}

/*#***********************************************************************\
 *
 *		update_selectivity_info
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	relh -
 *
 *
 *	relntuples -
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
static bool update_selectivity_info(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        double relntuples,
        int sample_ano,
        bool force_update)
{
        dynvtpl_t* sample_vtpl;
        uint i;
        va_t va;
        uint alloc_size;
        uint sample_size;
        rs_sqlinfo_t* sqli;
        bool succp;
        rs_key_t* clustkey;
        uint minsamplesize;
        uint maxsamplesize;
        ulong sampleinc;
        bool mainmem;
        bool persistent_mainmem = FALSE;
        dbe_db_t* db;
        bool issamples;
        bool mustrefresh;

        ss_trigger("update_selectivity_info");
        ss_pprintf_3(("update_selectivity_info:relname = %s, force_update = %d, relh=%ld\n", rs_relh_name(cd, relh), force_update, (long)relh));
        SS_PUSHNAME("update_selectivity_info");

        db = rs_sysi_db(cd);
        mainmem = (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY);
        if (relntuples > SS_INT4_MAX) {
            relntuples = SS_INT4_MAX;
        }
        if (!dbe_cfg_userandomkeysampleread) {
            dbe_db_enteraction(db, cd);
        }
        if (mainmem) {
            if (!rs_relh_isglobaltemporary(cd, relh) &&
                !rs_relh_istransient(cd, relh))
            {
                persistent_mainmem = TRUE;
                dbe_db_lockmmemutex(cd, db);
            }
        }
        rs_relh_samplemutex_enter(cd, relh);

        if (!force_update) {
            issamples = rs_relh_issamples_nomutex(cd, relh, sample_ano);
            ss_pprintf_4(("update_selectivity_info:issamples=%d\n", issamples));
            if (issamples) {
                mustrefresh = rs_relh_mustrefreshsamples(cd, relh);
                ss_pprintf_4(("update_selectivity_info:mustrefresh=%d\n", mustrefresh));
                if (!mustrefresh) {
                    ss_pprintf_4(("update_selectivity_info:already samples for ano %d and no need to refresh\n", sample_ano));
                    rs_relh_samplemutex_exit(cd, relh);
                    if (!dbe_cfg_userandomkeysampleread) {
                        dbe_db_exitaction(db, cd);
                    }
                    ss_trigger("update_selectivity_info");
                    goto ret_true;
                } else {
                    bool startp;
                    update_selectivity_info_t* usi;
                    usi = SSMEM_NEW(update_selectivity_info_t);
                    usi->usi_tdb = rs_sysi_tabdb(cd);
                    usi->usi_cd = rs_sysi_init();
                    usi->usi_relid = rs_relh_relid(cd, relh);
                    startp = rs_sysi_starttask(
                                cd, 
                                "update_selectivity_info_task",
                                (su_task_fun_t) update_selectivity_info_task, 
                                usi,
                                usi->usi_cd);
                    if (startp) {
                        ss_pprintf_4(("update_selectivity_info:started task to update samples\n"));
                        rs_relh_samplemutex_exit(cd, relh);
                        if (!dbe_cfg_userandomkeysampleread) {
                            dbe_db_exitaction(db, cd);
                        }
                        ss_trigger("update_selectivity_info");
                        goto ret_true;
                    }
                    rs_sysi_done(usi->usi_cd);
                    SsMemFree(usi);
                }
            }

            if (rs_relh_issamplefailed_nomutex(cd, relh)) {
                ss_pprintf_4(("update_selectivity_info:sample already failed\n"));
                rs_relh_samplemutex_exit(cd, relh);
                if (!dbe_cfg_userandomkeysampleread) {
                    dbe_db_exitaction(db, cd);
                }
                ss_trigger("update_selectivity_info");
                goto ret_false;
            }
        }

        sqli = rs_sysi_sqlinfo(cd);

        minsamplesize = rs_sqli_getestsamplemin(sqli);
        maxsamplesize = rs_sqli_getestsamplemax(sqli);
        sampleinc = rs_sqli_getestsampleinc(sqli);
        alloc_size = rs_sqli_getestsamplecount(sqli);
        if (est_selectivity_test_on) {
            alloc_size = maxsamplesize;
        }

        if (alloc_size < maxsamplesize && relntuples > sampleinc) {
            ulong new_alloc_size;
            new_alloc_size = alloc_size +
                             ((uint)relntuples - sampleinc) / sampleinc;
            if (new_alloc_size < maxsamplesize) {
                alloc_size = (uint)new_alloc_size;
            } else {
                alloc_size = maxsamplesize;
            }
        }

        ss_pprintf_3(("update_selectivity_info:try to get %d samples\n", alloc_size));

        if ((ulong)alloc_size * (ulong)sizeof(sample_vtpl[0]) >
            (ulong)SS_MAXALLOCSIZE) {
            alloc_size = SS_MAXALLOCSIZE / sizeof(sample_vtpl[0]);
        }

        clustkey = rs_relh_clusterkey(cd, relh);
        if (!mainmem) {
            dynvtpl_t range_min = NULL;
            dynvtpl_t range_max = NULL;
            int* sample_estimate = NULL;

            sample_vtpl = SsMemCalloc(sizeof(sample_vtpl[0]), alloc_size);

            /* Generate sample range minimum.
             */
            dynvtpl_setvtpl(&range_min, VTPL_EMPTY);
            va_setlong(&va, rs_key_id(cd, clustkey));
            dynvtpl_appva(&range_min, &va);

            /* Generate sample range maximum.
             */
            dynvtpl_setvtpl(&range_max, VTPL_EMPTY);
            va_setlong(&va, rs_key_id(cd, clustkey) + 1);
            dynvtpl_appva(&range_max, &va);

            if (force_update) {
                rs_relh_samplemutex_exit(cd, relh);
            }

            /* Get samples from the database engine.
             */
            dbe_db_getkeysamples(
                    db,
                    cd,
                    relh,
                    range_min,
                    range_max,
                    sample_vtpl,
                    alloc_size);

            if (force_update) {
                rs_relh_samplemutex_enter(cd, relh);
            }

            /* The returned sample_vtpl may contain holes, compress them
             * out and count the real sample_size.
             */
            for (i = 0, sample_size = 0; i < alloc_size; i++) {
                if (sample_vtpl[i] != NULL) {
                    sample_vtpl[sample_size++] = sample_vtpl[i];
                }
            }

            ss_dprintf_3(("update_selectivity_info:got %d samples\n", sample_size));

            succp = (sample_size >= minsamplesize);

            if (succp) {
                int kpno;
                int nparts;
                int ano;

                nparts = rs_key_nparts(cd, clustkey);
                sample_estimate = SsMemCalloc(sizeof(sample_estimate[0]), sample_size);

                rs_relh_clearsamples_nomutex(cd, relh);

                for (kpno = 0; kpno < nparts; kpno++) {
                    switch (rs_keyp_parttype(cd, clustkey, kpno)) {
                        case RSAT_USER_DEFINED:
                        case RSAT_SYNC:
                        case RSAT_TUPLE_ID:
                        case RSAT_TUPLE_VERSION:
                            ano = rs_keyp_ano(cd, clustkey, kpno);
                            if (!rs_relh_issamples_nomutex(cd, relh, ano)) {
                                get_attribute_selectivity(
                                        cd,
                                        relh,
                                        ano,
                                        sample_vtpl,
                                        sample_estimate,
                                        sample_size);
                                rs_relh_initattrsamples_nomutex(
                                        cd,
                                        relh,
                                        ano,
                                        sample_vtpl,
                                        sample_estimate,
                                        sample_size);
                            }
                            break;
                        default:
                            break;
                    }
                }

            } else {
                ss_dprintf_4(("update_selectivity_info:sample failed, sample_size=%d, minsamplesize=%d\n",
                              sample_size, minsamplesize));
                rs_relh_setsamplefailed_nomutex(cd, relh);
            }

            /* Release memory.
             */
            for (i = 0; i < sample_size; i++) {
                dynvtpl_free(&sample_vtpl[i]);
            }
            SsMemFree(sample_vtpl);
            if (sample_estimate != NULL) {
                SsMemFree(sample_estimate);
            }
            dynvtpl_free(&range_min);
            dynvtpl_free(&range_max);
        } else if (persistent_mainmem) {
            rs_ttype_t* ttype;
            rs_tval_t** sample_tvalarr =
                SsMemCalloc(sizeof(sample_tvalarr[0]), alloc_size);

            if (force_update) {
                rs_relh_samplemutex_exit(cd, relh);
            }

            /* Get samples from the database engine.
             */
            dbe_db_gettvalsamples(
                    rs_sysi_db(cd),
                    cd,
                    relh,
                    sample_tvalarr,
                    alloc_size);

            if (force_update) {
                rs_relh_samplemutex_enter(cd, relh);
            }

            /* The returned sample_tvalarr may contain holes, compress them
             * out and count the real sample_size.
             */
            for (i = 0, sample_size = 0; i < alloc_size; i++) {
                if (sample_tvalarr[i] != NULL) {
                    sample_tvalarr[sample_size++] = sample_tvalarr[i];
                }
            }

            ss_dprintf_3(("update_selectivity_info:got %d samples\n", sample_size));

            succp = (sample_size >= minsamplesize);

            if (succp) {
                int kpno;
                int nparts;
                int ano;

                nparts = rs_key_nparts(cd, clustkey);

                for (kpno = rs_key_first_datapart(cd, clustkey);
                     kpno < nparts;
                     kpno++)
                {
                    switch (rs_keyp_parttype(cd, clustkey, kpno)) {
                        case RSAT_USER_DEFINED:
                        case RSAT_SYNC:
                        case RSAT_TUPLE_ID:
                        case RSAT_TUPLE_VERSION:
                            ano = rs_keyp_ano(cd, clustkey, kpno);
                            if (!rs_relh_issamples_nomutex(cd, relh, ano)) {
                                rs_relh_initattrsamplesbytval_nomutex(
                                        cd,
                                        relh,
                                        ano,
                                        sample_tvalarr,
                                        sample_size);
                            }
                            break;
                        default:
                            break;
                    }
                }

            } else {
                ss_dprintf_4(("update_selectivity_info:sample failed, sample_size=%d, minsamplesize=%d\n",
                              sample_size, minsamplesize));
                rs_relh_setsamplefailed_nomutex(cd, relh);
            }
            ttype = rs_relh_ttype(cd, relh);
            for (i = 0; i < sample_size; i++) {
                rs_tval_free(cd, ttype, sample_tvalarr[i]);
            }
            SsMemFree(sample_tvalarr);
        } else {
            ss_dprintf_1(("update_selectivity_info:samples not collected from temp/transient tables\n"));
            succp = FALSE;
        }

        rs_relh_samplemutex_exit(cd, relh);
        if (!dbe_cfg_userandomkeysampleread) {
            dbe_db_exitaction(db, cd);
        }

        ss_trigger("update_selectivity_info");
 return_sequence:;
        if (persistent_mainmem) {
            dbe_db_unlockmmemutex(cd, db);
        }
        SS_POPNAME;
        return(succp);
 ret_true:;
        succp = TRUE;
        goto return_sequence;
 ret_false:;
        succp = FALSE;
        goto return_sequence;
}

/*##**********************************************************************\
 *
 *		tb_est_ensureselectivityinfo
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	relh -
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
void tb_est_ensureselectivityinfo(rs_sysi_t* cd, rs_relh_t* relh)
{
        est_ensureselectivityinfo(cd, relh, FALSE);
}

/*##**********************************************************************\
 *
 *		tb_est_updateselectivityinfo
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	relh -
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
void tb_est_updateselectivityinfo(rs_sysi_t* cd, rs_relh_t* relh)
{
        est_ensureselectivityinfo(cd, relh, TRUE);
}

/*#***********************************************************************\
 *
 *		data_sample_selectivity_relop
 *
 * Returns selectivity based on data samples for a relop.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	relh -
 *
 *
 *	ano -
 *
 *
 *	relop -
 *
 *
 *	value -
 *
 *
 *	p_selectivity -
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
static bool data_sample_selectivity_relop(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        cons_info_t* cons_info,
        rs_cons_t* cons,
        int relop,
        va_t* value,
        double* p_selectivity)
{
        rs_ano_t ano;
        bool succp;
        ss_int8_t nvalues_i8;
        double nvalues;
        double rel_nvalues;

        ss_dprintf_3(("data_sample_selectivity_relop:relname = %s, relop = %d\n", rs_relh_name(cd, relh), relop));
        ss_trigger("data_sample_selectivity_relop");

        if (cons_info->cons_fixedest) {
            /* Too few key values, use constant approximates.
             */
            ss_dprintf_4(("data_sample_selectivity_relop:too few data key values\n"));
            ss_trigger("data_sample_selectivity_relop");
            return(FALSE);
        }

        if (value == NULL) {
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = rs_cons_atype(cd, cons);
            aval = rs_cons_aval(cd, cons);
            if (aval == NULL) {
                ss_dprintf_4(("data_sample_selectivity_relop:constrain value unknown\n"));
                if (relop != RS_RELOP_EQUAL && relop != RS_RELOP_NOTEQUAL) {
                    ss_trigger("data_sample_selectivity_relop");
                    return(FALSE);
                }
                value = NULL;
            } else {
                value = rs_aval_va(cd, atype, aval);
            }
        }

        rel_nvalues = cons_info->cons_relntuples;
        ano = rs_cons_ano(cd, cons);

        if (!update_selectivity_info(cd, relh, rel_nvalues, ano, FALSE)) {
            ss_dprintf_4(("data_sample_selectivity_relop:failed to get samples\n"));
            ss_trigger("data_sample_selectivity_relop");
            return(FALSE);
        }

        succp = rs_relh_getrelopselectivity(
                    cd,
                    relh,
                    ano,
                    relop,
                    value,
                    rs_cons_escchar(cd, cons),
                    &nvalues_i8);

        if (!succp) {
            ss_dprintf_4(("data_sample_selectivity_relop:rs_relh_getrelopselectivity failed\n"));
            ss_trigger("data_sample_selectivity_relop");
            return(FALSE);
        }

        SsInt8ConvertToDouble(&nvalues, nvalues_i8);
        *p_selectivity = nvalues / rel_nvalues;

        ss_dprintf_4(("data_sample_selectivity_relop:selectivity = %.2lf\n", *p_selectivity));
        ss_trigger("data_sample_selectivity_relop");

        return(TRUE);
}

/*#***********************************************************************\
 *
 *		data_sample_selectivity_range
 *
 * Returns selectivity based on data samples for a key value range.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	relh -
 *
 *
 *	cons_info -
 *
 *
 *	ano -
 *
 *
 *	range_start -
 *
 *
 *	range_start_closed -
 *
 *
 *	range_end -
 *
 *
 *	range_end_closed -
 *
 *
 *	pointlike -
 *
 *
 *	p_selectivity -
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
static bool data_sample_selectivity_range(
        rs_sysi_t*      cd,
        rs_relh_t*      relh,
        cons_info_t*    cons_info,
        rs_ano_t        ano,
        dynvtpl_t       range_start,
        bool            range_start_closed,
        dynvtpl_t       range_end,
        bool            range_end_closed,
        bool            pointlike,
        double*         p_selectivity)
{
        bool succp;
        ss_int8_t nvalues_i8;
        double nvalues;
        double rel_nvalues;

        ss_trigger("data_sample_selectivity_range");
        ss_dprintf_3(("data_sample_selectivity_range:relname = %s, ano = %d\n", rs_relh_name(cd, relh), ano));
        ss_dassert(!cons_info->cons_fixedest);
        ss_rc_dassert(vtpl_vacount(range_start) == 1, vtpl_vacount(range_start));
        ss_rc_dassert(range_end == NULL || vtpl_vacount(range_end) == 1, vtpl_vacount(range_end));

        rel_nvalues = cons_info->cons_relntuples;

        if (!update_selectivity_info(cd, relh, rel_nvalues, ano, FALSE)) {
            ss_dprintf_4(("data_sample_selectivity_range:failed to get samples\n"));
            ss_trigger("data_sample_selectivity_range");
            return(FALSE);
        }

        ss_dprintf_4(("data_sample_selectivity_range:range_start_closed=%d, range_end_closed=%d, pointlike=%d\n",
            range_start_closed, range_end_closed, pointlike));

        succp = rs_relh_getrangeselectivity(
                    cd,
                    relh,
                    ano,
                    vtpl_getva_at(range_start, 0),
                    range_start_closed,
                    range_end != NULL ? vtpl_getva_at(range_end, 0) : NULL,
                    range_end_closed,
                    pointlike,
                    &nvalues_i8);

        if (!succp) {
            ss_dprintf_4(("data_sample_selectivity_range:rs_relh_getrangeselectivity failed\n"));
            ss_trigger("data_sample_selectivity_range");
            return(FALSE);
        }

        SsInt8ConvertToDouble(&nvalues, nvalues_i8);
        *p_selectivity = (double)nvalues / (double)rel_nvalues;

        ss_dprintf_4(("data_sample_selectivity_range:selectivity = %.2lf\n", *p_selectivity));
        ss_trigger("data_sample_selectivity_range");

        return(TRUE);
}

#else /* SS_NOESTSAMPLES */

#define data_sample_selectivity_relop(cd,relh,cons_info,cons,relop,value,p_selectivity) \
        (FALSE)
#define data_sample_selectivity_range(cd,relh,cons_info,ano,range_start,range_start_closed,range_end,range_end_closed,pointlike,p_selectivity) \
        (FALSE)

#endif /* SS_NOESTSAMPLES */

/*#***********************************************************************\
 *
 *		selectivity_for_optn
 *
 * Calculates a new selectivity based on user given optn values.
 *
 * NOTE! Currently used only for vector constraints.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	cons_info -
 *
 *
 *	selectivity -
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
static double selectivity_for_optn(
        rs_sysi_t*   cd,
        cons_info_t* cons_info,
        double       selectivity,
        uint         n_solved_vector_constraints)
{
        double new_selectivity;

        if (cons_info->cons_vectoroptn > 0 &&
            n_solved_vector_constraints > 0)
        {

            if (n_solved_vector_constraints == cons_info->cons_nvectorconstraint) {
                new_selectivity = (double)cons_info->cons_vectoroptn /
                                            cons_info->cons_relntuples;
            } else {
                uint unsolved;
                unsolved = cons_info->cons_nvectorconstraint -
                           n_solved_vector_constraints;
                new_selectivity =
                    selectivity * (double)unsolved /
                    (double)cons_info->cons_nvectorconstraint;
            }
            if (new_selectivity < EST_MIN_SELECTIVITY) {
                new_selectivity = EST_MIN_SELECTIVITY;
            }
            if (new_selectivity < selectivity) {
                if (cons_info->cons_infolevel >= 11) {
                    char buf[255];
                    SsSprintf(buf, "Updating selectivity: %lf -> %lf\n",
                        selectivity,
                        new_selectivity);
                    tb_info_print(cd, NULL, 11, buf);
                }
                selectivity = new_selectivity;
            }
        }
        return(selectivity);
}
#endif /* !SS_MYSQL */

/*#***********************************************************************\
 *
 *		selectivity_for_range
 *
 * Gets a key value range selectivity for attribute.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	relh -
 *
 *
 *	cons_info -
 *
 *
 *	column_no -
 *
 *
 *	consbyano_list -
 *
 *
 *	p_full_scan -
 *
 *
 *	p_is_equality_constraint -
 *
 *
 *	p_range_selectivity -
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
static bool selectivity_for_range(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        cons_info_t* cons_info,
        uint column_no,
        su_list_t* consbyano_list,
        bool* p_full_scan,
        bool* p_is_equality_constraint,
        est_rangetype_t* p_rangetype,
        double* p_range_selectivity)
{
        bool succp;
        dynvtpl_t range_start = NULL;
        bool range_start_closed;
        dynvtpl_t range_end = NULL;
        bool range_end_closed;
        bool pointlike = FALSE;
        double selectivity = 0.0;
        bool is_estimate = FALSE;
        bool emptyrange;

        ss_dprintf_3(("selectivity_for_range\n"));
        ss_dassert(su_list_length(consbyano_list) > 0);

        succp = tb_pla_form_range_constraint(
                    cd,
                    column_no,
                    consbyano_list,
                    NULL,
                    &range_start,
                    &range_start_closed,
                    &range_end,
                    &range_end_closed,
                    &pointlike,
                    &emptyrange);

        if (succp) {

            if (emptyrange) {
                /* Empty range, constraints are contradictory.
                 */
                ss_dprintf_4(("selectivity_for_range:empty range\n"));
                is_estimate = TRUE;
                selectivity = 0.0;
                selectivity_add(cons_info, selectivity, column_no);

            } else if (cons_info->cons_fixedest || !cons_info->cons_rangeest) {
                /* Too few key values for sample based estimates, or
                 * range estimates are not used.
                 */
                ss_dprintf_4(("selectivity_for_range:too few key values\n"));
                ss_dassert(range_start != NULL);
                is_estimate = FALSE;

            } else {
                ss_dassert(range_start != NULL);
                succp = data_sample_selectivity_range(
                            cd,
                            relh,
                            cons_info,
                            column_no,
                            range_start,
                            range_start_closed,
                            range_end,
                            range_end_closed,
                            pointlike,
                            &selectivity);
                ss_dprintf_4(("selectivity_for_range:data_sample_selectivity_range succp=%d\n", succp));
                is_estimate = succp;
                if (succp) {
                    selectivity_add(cons_info, selectivity, column_no);
                }
            }
            if (p_full_scan != NULL) {
                *p_full_scan = FALSE;
            }
            if (pointlike && p_is_equality_constraint != NULL) {
                *p_is_equality_constraint = TRUE;
            }
            if (is_estimate && p_range_selectivity != NULL) {
                *p_range_selectivity = selectivity;
            }
            if (p_rangetype != NULL) {
                if (range_start != NULL) {
                    *p_rangetype |= EST_RANGE_BEGIN;
                }
                if (range_end != NULL) {
                    *p_rangetype |= EST_RANGE_END;
                }
                ss_dprintf_4(("selectivity_for_range:*p_rangetype=%d\n", *p_rangetype));
            }
        }

        if (!is_estimate) {
            rs_cons_t* constraint;
            su_list_node_t* n;
            su_list_do_get (consbyano_list, n, constraint) {
                rs_cons_setestimated(cd, constraint, FALSE);
            }
        }

        dynvtpl_free(&range_start);
        dynvtpl_free(&range_end);

        ss_dprintf_4(("selectivity_for_range:is_estimate=%d\n", is_estimate));
        return(is_estimate);
}

/*##**********************************************************************\
 *
 *		range_selectivity_for_key
 *
 * Estimates the range selectivity using the specified key. For example,
 * selectivity = 0.1 means that the range will contain 10 % of the
 * whole index range for the key.
 *
 * Parameters :
 *
 *	key - in, use
 *		handle to the key
 *
 *	cons_info - in, use
 *		search constraint list
 *
 *      unique_value - out, give
 *		TRUE if the key is unique and the constraints
 *          determine a unique value
 *
 *      full_scan - out, give
 *		TRUE if the full table scan is used
 *
 *
 * Return value :
 *      estimated range selectivity as a value between 0 and 1
 *
 * Limitations  :
 *
 * Globals used :
 */
static double range_selectivity_for_key(cd, relh, key, cons_info,
                                        unique_value, full_scan)
        rs_sysi_t*   cd;
        rs_relh_t*   relh;
        rs_key_t*    key;
        cons_info_t* cons_info;
        bool*        unique_value;
        bool*        full_scan;
{
        bool                finished;
        uint                i;
        uint                n_attributes_in_key;
        su_list_node_t*     n;
        rs_cons_t*          constraint;
        uint                column_no;
        bool                is_equality_constraint;
        bool                found_constraint = FALSE;
        double              selectivity;
        bool                succp;
        est_rangetype_t     rangetype = EST_RANGE_NONE;

        ss_dassert(key);
        ss_dassert(cons_info);

        ss_dprintf_3(("range_selectivity_for_key\n"));

        n_attributes_in_key = rs_key_nparts(cd, key);
        *unique_value = FALSE;
        *full_scan = TRUE;

        i = 0;
        finished = FALSE;

        /* loop through the key parts as long as there is an equality
         * constraint on the previous key part (excluding constvalues)
         */
        while (i < n_attributes_in_key && !finished) {

            ss_dprintf_4(("range_selectivity_for_key:kpno=%d\n", i));
            found_constraint = FALSE;
            rangetype = EST_RANGE_NONE;

            if (!rs_keyp_isconstvalue(cd, key, i)) {
                /* there can be constraints only on non-constant key parts
                 */
                su_list_t* consbyano_list;

                column_no = rs_keyp_ano(cd, key, i);
                is_equality_constraint = FALSE;

                if (su_pa_indexinuse(cons_info->cons_byano, column_no)) {
                    bool ascp;

                    consbyano_list = su_pa_getdata(cons_info->cons_byano, column_no);

                    ascp = rs_keyp_isascending(cd, key, i);

                    /* First try to get a range selectivity.
                     */
                    succp = selectivity_for_range(
                                cd,
                                relh,
                                cons_info,
                                column_no,
                                consbyano_list,
                                full_scan,
                                &is_equality_constraint,
                                &rangetype,
                                NULL);
                    if (succp) {
                        found_constraint = TRUE;
                    }

                    /* loop through the constraints on
                     * this key part
                     */
                    su_list_do_get (consbyano_list, n, constraint) {
                        int relop;

                        ss_dassert(rs_cons_ano(cd, constraint) == (rs_ano_t)column_no);
                        relop = rs_cons_relop(cd, constraint);
                        if (!rs_cons_isestimated(cd, constraint)) {
                            selectivity_add(
                                cons_info,
                                range_selectivity_for_constr(
                                    cd,
                                    relh,
                                    cons_info,
                                    constraint,
                                    ascp,
                                    rangetype,
                                    &found_constraint),
                                column_no);
                            switch (relop) {
                                case RS_RELOP_EQUAL:
                                case RS_RELOP_ISNULL:
                                    is_equality_constraint = TRUE;
                                    *full_scan = FALSE;
                                    break;
                                case RS_RELOP_NOTEQUAL:
                                case RS_RELOP_ISNOTNULL:
                                    break;
                                default:
                                    *full_scan = FALSE;
                                    break;
                            }
                        } else {
                            /* Clear estimated flag for later estimates.
                             */
                            rs_cons_setestimated(cd, constraint, FALSE);
                        }
                    }
                }

                if (is_equality_constraint == FALSE) {
                    finished = TRUE;
                }
            }

            if (!finished) {
                i++;
            }
        }

        /* i is now the index of the first attribute which did not
         * have equality constraint on it, or i == n_attributes_in_key
         */
        if (rs_key_isunique(cd, key) && i > rs_key_lastordering(cd, key)) {
            /* there was equality on all order-determining key parts
             */
            *unique_value = TRUE;
        }

        if (cons_info->cons_vectoroptn == 1 && found_constraint) {
            /* Vector constraints may limit selectivity.
             */
            int j;
            bool found_vector_constraint;
            est_rangetype_t  vector_rangetype = EST_RANGE_NONE;

            finished = FALSE;
            j = i + 1;  /* Skip last key part which was already added above. */
            while (j < (int)n_attributes_in_key && !finished) {
                bool ascp;
                su_list_t* consbyano_list;

                ss_dprintf_4(("range_selectivity_for_key:vector kpno=%d\n", j));
                found_vector_constraint = FALSE;

                switch (rs_keyp_parttype(cd, key, j)) {
                    case RSAT_USER_DEFINED:
                        /* there can be constraints only on user defined
                         * key parts
                         */
                        ss_bassert(!rs_keyp_isconstvalue(cd, key, j));
                        vector_rangetype = EST_RANGE_NONE;
                        column_no = rs_keyp_ano(cd, key, j);
                        ascp = rs_keyp_isascending(cd, key, j);

                        if (su_pa_indexinuse(cons_info->cons_byano, column_no)) {
                            /* loop through the constraints on
                             * this key part
                             */
                            bool vectorp;
                            consbyano_list = su_pa_getdata(cons_info->cons_byano, column_no);
                            su_list_do_get (consbyano_list, n, constraint) {
                                vectorp = FALSE;
                                ss_dassert(rs_cons_ano(cd, constraint) == (rs_ano_t)column_no);
                                switch (rs_cons_relop(cd, constraint)) {
                                    case RS_RELOP_GT_VECTOR:
                                    case RS_RELOP_GE_VECTOR:
                                        if (ascp) {
                                            vectorp = TRUE;
                                            vector_rangetype |= EST_RANGE_BEGIN;
                                        }
                                        break;
                                    case RS_RELOP_LT_VECTOR:
                                    case RS_RELOP_LE_VECTOR:
                                        if (!ascp) {
                                            vectorp = TRUE;
                                            vector_rangetype |= EST_RANGE_BEGIN;
                                        }
                                        break;
                                    default:
                                        break;
                                }
                                if (!vectorp) {
                                    continue;
                                }
                                if (!rs_cons_isestimated(cd, constraint)) {
                                    selectivity = range_selectivity_for_constr(
                                                    cd,
                                                    relh,
                                                    cons_info,
                                                    constraint,
                                                    ascp,
                                                    vector_rangetype,
                                                    &found_vector_constraint);
                                    selectivity_add(
                                        cons_info,
                                        selectivity,
                                        column_no);
                                } else {
                                    /* Clear estimated flag for later estimates.
                                     */
                                    rs_cons_setestimated(cd, constraint, FALSE);
                                }
                            }
                        }
                        break;
                    default:
                        break;
                }
                if (!found_vector_constraint) {
                    finished = TRUE;
                }
                j++;
            }
        }
        selectivity = selectivity_get(cd, cons_info);

        ss_dprintf_4(("range_selectivity_for_key:selectivity=%lf\n", selectivity));

        if (selectivity >= 1.0 && found_constraint) {
            /* All rows selected, so also following constraints
             * may limit the selectivity.
             */
            ss_dassert(rangetype != EST_RANGE_NONE);
            finished = FALSE;
            i++;    /* Skip last key part which was already added above. */
            while (i < n_attributes_in_key && !finished) {
                su_list_t* consbyano_list;

                ss_dprintf_4(("range_selectivity_for_key:recalc kpno=%d, rangetype=%d\n", i, (int)rangetype));
                found_constraint = FALSE;

                switch (rs_keyp_parttype(cd, key, i)) {
                    case RSAT_USER_DEFINED:
                    case RSAT_SYNC:
                        /* there can be constraints only on user defined
                         * key parts
                         */
                        ss_bassert(!rs_keyp_isconstvalue(cd, key, i));

                        column_no = rs_keyp_ano(cd, key, i);

                        if (su_pa_indexinuse(cons_info->cons_byano, column_no)) {
                            consbyano_list = su_pa_getdata(cons_info->cons_byano, column_no);

                            if (rangetype == (EST_RANGE_BEGIN|EST_RANGE_END)) {
                                /* Try to get a range selectivity.
                                 */
                                succp = selectivity_for_range(
                                            cd,
                                            relh,
                                            cons_info,
                                            column_no,
                                            consbyano_list,
                                            full_scan,
                                            &is_equality_constraint,
                                            &rangetype,
                                            &selectivity);
                                if (succp) {
                                    found_constraint = TRUE;
                                    if (selectivity < 1.0) {
                                        finished = TRUE;
                                    }
                                }
                            }
                            /* loop through the constraints on
                             * this key part
                             */
                            su_list_do_get (consbyano_list, n, constraint) {
                                ss_dassert(rs_cons_ano(cd, constraint) == (rs_ano_t)column_no);
                                if (!rs_cons_isestimated(cd, constraint)) {
                                    selectivity = range_selectivity_for_constr(
                                                    cd,
                                                    relh,
                                                    cons_info,
                                                    constraint,
                                                    rs_keyp_isascending(cd, key, i),
                                                    rangetype,
                                                    &found_constraint);
                                    selectivity_add(
                                        cons_info,
                                        selectivity,
                                        column_no);
                                    if (selectivity < 1.0) {
                                        finished = TRUE;
                                    }
                                } else {
                                    /* Clear estimated flag for later estimates.
                                     */
                                    rs_cons_setestimated(cd, constraint, FALSE);
                                }
                            }
                        }
                        break;
                    default:
                        break;
                }
                if (!found_constraint) {
                    finished = TRUE;
                }
                i++;
            }
            selectivity = selectivity_get(cd, cons_info);
            ss_dprintf_4(("range_selectivity_for_key:recalc selectivity=%lf\n", selectivity));
        }

        if (cons_info->cons_infolevel >= 11) {
            char buf[255];
            SsSprintf(buf, "Range selectivity for key: %s %lf\n",
                rs_key_name(cd, key), selectivity);
            tb_info_print(cd, NULL, 11, buf);
        }

        return(selectivity);
}

/*##**********************************************************************\
 *
 *		total_selectivity_for_key
 *
 * Estimates the total selectivity using the specified key. The
 * total selectivity is the product of range and pointwise selectivities.
 *
 * Parameters :
 *
 *	key - in, use
 *		handle to the key
 *
 *	cons_info - in, use
 *		search constraint list
 *
 *
 * Output params:
 *
 * Return value :
 *      estimated total selectivity
 *
 * Limitations  :
 * Globals used :
 */
static double total_selectivity_for_key(cd, relh, key, cons_info,
                                        n_solved_constraints)
        rs_sysi_t*   cd;
        rs_relh_t*   relh;
        rs_key_t*    key;
        cons_info_t* cons_info;
        long*        n_solved_constraints;
{
        uint                i;
        uint                n_attributes_in_key;
        su_list_node_t*     n;
        rs_cons_t*          constraint;
        uint                column_no;
        double              selectivity;
        uint                n_solved_vector_constraints = 0;
        uint                n_user_constraints = 0;

        ss_dprintf_3(("total_selectivity_for_key\n"));
        ss_dassert(key);
        ss_dassert(cons_info);

        n_attributes_in_key = rs_key_nparts(cd, key);

        i = 0;
        *n_solved_constraints = 0;

        while (i < n_attributes_in_key) {

            if (!rs_keyp_isconstvalue(cd, key, i)) {
                su_list_t* consbyano_list;

                n_user_constraints++;

                column_no = rs_keyp_ano(cd, key, i);
                if (su_pa_indexinuse(cons_info->cons_byano, column_no)) {

                    ss_dprintf_4(("total_selectivity_for_key:column_no=%d\n", column_no));

                    consbyano_list = su_pa_getdata(cons_info->cons_byano, column_no);

                    /* First try to get a range selectivity.
                     */
                    selectivity_for_range(
                        cd,
                        relh,
                        cons_info,
                        column_no,
                        consbyano_list,
                        NULL,
                        NULL,
                        NULL,
                        NULL);

                    su_list_do_get (consbyano_list, n, constraint) {
                        switch (rs_cons_relop(cd, constraint)) {
                            case RS_RELOP_GT_VECTOR:
                            case RS_RELOP_GE_VECTOR:
                            case RS_RELOP_LT_VECTOR:
                            case RS_RELOP_LE_VECTOR:
                                if (n_user_constraints == n_solved_vector_constraints) {
                                    n_solved_vector_constraints++;
                                }
                                break;
                            default:
                                break;
                        }
                        ss_dassert(rs_cons_ano(cd, constraint) == (rs_ano_t)column_no);
                        if (!rs_cons_isestimated(cd, constraint)) {
                            selectivity_add(
                                cons_info,
                                selectivity_for_constraint(
                                    cd, relh, cons_info, constraint),
                                column_no);
                        } else {
                            /* Clear estimated flag for later estimates.
                             */
                            rs_cons_setestimated(cd, constraint, FALSE);
                        }
                        (*n_solved_constraints)++;
                    }
                }
            }
            i++;

        }

        selectivity = selectivity_get(cd, cons_info);

        if (cons_info->cons_infolevel >= 11) {
            char buf[255];
            SsSprintf(buf, "Total selectivity for key: %s %lf\n",
                rs_key_name(cd, key), selectivity);
            tb_info_print(cd, NULL, 11, buf);
        }

        ss_dprintf_4(("total_selectivity_for_key:selectivity=%lf\n", (double)selectivity));

        return(selectivity);
}


/*##**********************************************************************\
 *
 *		global_selectivity
 *
 * Calculates the selectivity of all constraints in the constraint
 * list, i.e., the selectivity is not key-specific and it is used
 * in calculation of the number of rows in result set.
 *
 * Parameters :
 *
 *	cons_info - in, use
 *		constraint list
 *
 *
 * Output params:
 *
 * Return value :
 *      selectivity as a number between 0 and 1
 *
 * Limitations  :
 *
 * Globals used :
 */
static double global_selectivity(cd, relh, cons_info)
        rs_sysi_t*   cd;
        rs_relh_t*   relh;
        cons_info_t* cons_info;
{
        su_list_node_t*     n;
        rs_cons_t*          constraint;
        uint                column_no;
        su_list_t*          consbyano_list;
        double              selectivity;

        ss_dassert(cons_info);

        su_pa_do_get(cons_info->cons_byano, column_no, consbyano_list) {

            /* First try to get a range selectivity. */
            selectivity_for_range(
                cd,
                relh,
                cons_info,
                column_no,
                consbyano_list,
                NULL,
                NULL,
                NULL,
                NULL);

            su_list_do_get (consbyano_list, n, constraint) {
                ss_dassert(rs_cons_ano(cd, constraint) == (rs_ano_t)column_no);
                if (!rs_cons_isestimated(cd, constraint)) {
                    selectivity_add(
                        cons_info,
                        selectivity_for_constraint(
                            cd, relh, cons_info, constraint),
                        column_no);
                } else {
                    /* Clear estimated flag for later estimates.
                     */
                    rs_cons_setestimated(cd, constraint, FALSE);
                }
            }
        }

        selectivity = selectivity_get(cd, cons_info);

        if (cons_info->cons_infolevel >= 11) {
            char buf[255];
            SsSprintf(buf, "Global selectivity: %lf\n", selectivity);
            tb_info_print(cd, NULL, 11, buf);
        }

        return(selectivity);
}

#ifdef SS_UNICODE_DATA

/*#***********************************************************************\
 *
 *		get_like_selectivity
 *
 * Gets estimate of like constraint selectivity
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	relh - use
 *		relation handle
 *
 *	cons_info - in, use
 *		constraint_info
 *
 *	constraint - in, use
 *		constraint object
 *
 *	like_value - in, use
 *		like pattern in va format
 *
 * Return value :
 *      a selectivity value in the range (0, 1]
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static double get_like_selectivity(
        rs_sysi_t*   cd,
        rs_relh_t*   relh,
        cons_info_t* cons_info,
        rs_cons_t*   constraint,
        va_t*        like_value)
{
        double like_selectivity;
        double min_like_selectivity;
        double no_selectivity;
        rs_sqlinfo_t* sqli;
        bool succp;
        double selectivity;
        size_t fixedprefixlen;
        size_t numfixedchars;
        size_t numwildcards;

        ss_dprintf_3(("get_like_selectivity\n"));

        succp = data_sample_selectivity_relop(
                    cd,
                    relh,
                    cons_info,
                    constraint,
                    RS_RELOP_LIKE,
                    like_value,
                    &selectivity);
        if (succp) {
            return(selectivity);
        }

        sqli = rs_sysi_sqlinfo(cd);
        fixedprefixlen = rs_cons_likeprefixinfo(
                            cd,
                            constraint,
                            &numfixedchars,
                            &numwildcards);
        like_selectivity = rs_sqli_like_selectivity(sqli);

        if (!est_test_version_on) {

            if (numwildcards == 0) {
                like_selectivity = rs_sqli_equal_selectivity(sqli);
            } else {
                min_like_selectivity = rs_sqli_equal_selectivity(sqli);
                if (fixedprefixlen == 0) {
                    size_t i;

                    no_selectivity = rs_sqli_no_selectivity(sqli);
                    if (numfixedchars != 0) {
                        like_selectivity = rs_sqli_notequal_selectivity(sqli);
                        for (i = 1; i < numfixedchars; i++) {
                            like_selectivity *= 0.75;
                            if (like_selectivity < min_like_selectivity) {
                                like_selectivity = min_like_selectivity;
                                break;
                            }
                        }
                    } else {
                        like_selectivity = no_selectivity;
                    }
                } else {
                    size_t i;

                    for (i = 0; i < fixedprefixlen; i++) {
                        like_selectivity *= 0.75;
                        if (like_selectivity <= min_like_selectivity) {
                            like_selectivity = min_like_selectivity;
                            break;
                        }
                    }
                }
            }
        }
        ss_dprintf_4(("get_like_selectivity:like_selectivity = %.4lf\n", like_selectivity));
        return(like_selectivity);
}

#else /* SS_UNICODE_DATA */
static double get_like_selectivity(
        rs_sysi_t*   cd,
        rs_relh_t*   relh,
        cons_info_t* cons_info,
        rs_cons_t*   constraint,
        va_t*        like_value)
{
        double like_selectivity;
        double min_like_selectivity;
        rs_sqlinfo_t* sqli;
        bool succp;
        double selectivity;
        char* like_string;

        ss_dprintf_3(("get_like_selectivity:'%s'\n", va_getasciiz(like_value)));

        succp = data_sample_selectivity_relop(
                    cd,
                    relh,
                    cons_info,
                    constraint,
                    RS_RELOP_LIKE,
                    like_value,
                    &selectivity);
        if (succp) {
            return(selectivity);
        }

        sqli = rs_sysi_sqlinfo(cd);
        like_string = va_getasciiz(like_value);
        like_selectivity = rs_sqli_like_selectivity(sqli);

        if (!est_test_version_on) {

            if (*like_string == '\0') {
                like_selectivity = rs_sqli_no_selectivity(sqli);

            } else if (*like_string == '%' || *like_string == '_') {
                double no_selectivity;
                no_selectivity = rs_sqli_no_selectivity(sqli);
                while (*like_string != '\0' &&
                       (*like_string == '%' || *like_string == '_')) {
                    like_selectivity *= 1.9;
                    if (like_selectivity >= no_selectivity) {
                        like_selectivity = no_selectivity;
                        break;
                    }
                    like_string++;
                }
                if (*like_string == '\0') {
                    /* Only wildcards found. */
                    like_selectivity = no_selectivity;
                }

            } else {
                min_like_selectivity = rs_sqli_equal_selectivity(sqli);
                like_string++;

                while (*like_string != '\0'
                    && *like_string != '%'
                    && *like_string != '_') {

                    like_selectivity *= 0.75;
                    if (like_selectivity <= min_like_selectivity) {
                        like_selectivity = min_like_selectivity;
                        break;
                    }
                    like_string++;
                }
            }
        }
        ss_dprintf_4(("get_like_selectivity:like_selectivity = %.4lf\n", like_selectivity));
        return(like_selectivity);
}

#endif /* SS_UNICODE_DATA */

/*##**********************************************************************\
 *
 *		range_selectivity_for_constr
 *
 * Estimates the range selectivity for a given constraint
 *
 * Parameters :
 *
 *	relh - in
 *		relation
 *
 *	constraint - in
 *		constraint
 *
 * Output params:
 *
 * Return value :
 *      range selectivity
 *
 * Limitations  :
 *
 * Globals used :
 */
static double range_selectivity_for_constr(
        rs_sysi_t*      cd,
        rs_relh_t*      relh,
        cons_info_t*    cons_info,
        rs_cons_t*      constraint,
        bool            ascp,
        est_rangetype_t rangetype,
        bool*           p_isrange)
{
        bool succp;
        double selectivity;
        rs_sqlinfo_t* sqli;
        uint relop;

        ss_dassert(constraint);
        ss_dprintf_3(("range_selectivity_for_constr:relop=%d\n", rs_cons_relop(cd, constraint)));

        *p_isrange = FALSE;

        sqli = rs_sysi_sqlinfo(cd);

        if (rs_cons_isalwaysfalse(cd, constraint)) {
            return(0.0);
        }

        relop = rs_cons_relop(cd, constraint);

        switch (relop) {
            case RS_RELOP_NOTEQUAL:
                return(rs_sqli_no_selectivity(sqli));
            case RS_RELOP_ISNOTNULL:
                succp = data_sample_selectivity_relop(
                            cd,
                            relh,
                            cons_info,
                            constraint,
                            RS_RELOP_NOTEQUAL,
                            VA_NULL,
                            &selectivity);
                if (succp) {
                    return(selectivity);
                } else {
                    return(rs_sqli_isnotnull_selectivity(sqli));
                }
            case RS_RELOP_ISNULL:
                succp = data_sample_selectivity_relop(
                            cd,
                            relh,
                            cons_info,
                            constraint,
                            RS_RELOP_EQUAL,
                            VA_NULL,
                            &selectivity);
                if (succp) {
                    return(selectivity);
                } else {
                    return(rs_sqli_isnull_selectivity(sqli));
                }
            case RS_RELOP_GT:
            case RS_RELOP_GT_VECTOR:
            case RS_RELOP_GE:
            case RS_RELOP_GE_VECTOR:
                if ((ascp && SU_BFLAG_TEST(rangetype, EST_RANGE_BEGIN)) ||
                    (!ascp && SU_BFLAG_TEST(rangetype, EST_RANGE_END))) {
                    *p_isrange = TRUE;
                    succp = data_sample_selectivity_relop(
                                cd,
                                relh,
                                cons_info,
                                constraint,
                                relop,
                                NULL,
                                &selectivity);
                    if (succp) {
                        return(selectivity);
                    } else {
                        return(rs_sqli_compare_selectivity(sqli));
                    }
                } else {
                    return(rs_sqli_no_selectivity(sqli));
                }
            case RS_RELOP_LT:
            case RS_RELOP_LT_VECTOR:
            case RS_RELOP_LE:
            case RS_RELOP_LE_VECTOR:
                if ((ascp && SU_BFLAG_TEST(rangetype, EST_RANGE_END)) ||
                    (!ascp && SU_BFLAG_TEST(rangetype, EST_RANGE_BEGIN))) {
                    *p_isrange = TRUE;
                    succp = data_sample_selectivity_relop(
                                cd,
                                relh,
                                cons_info,
                                constraint,
                                relop,
                                NULL,
                                &selectivity);
                    if (succp) {
                        return(selectivity);
                    } else {
                        return(rs_sqli_compare_selectivity(sqli));
                    }
                } else {
                    return(rs_sqli_no_selectivity(sqli));
                }
            case RS_RELOP_EQUAL:
                if (rs_cons_aval(cd, constraint) == NULL &&
                    is_equal_constraint(
                        cd,
                        cons_info,
                        rs_cons_ano(cd, constraint),
                        TRUE)) {
                    /* This constraint info is NULL (not known) and there
                     * is known constraint for this column. Do not decrease
                     * selectivity. This case is possible in cases where
                     * SQL interpreter copies constraints from one row to
                     * another (?).
                     */
                    return(rs_sqli_no_selectivity(sqli));
                }
                *p_isrange = TRUE;
                succp = data_sample_selectivity_relop(
                            cd,
                            relh,
                            cons_info,
                            constraint,
                            RS_RELOP_EQUAL,
                            NULL,
                            &selectivity);
                if (succp) {
                    return(selectivity);
                } else {
                    return(rs_sqli_equal_selectivity(sqli));
                }
            case RS_RELOP_LIKE:
                {
                    rs_atype_t* atype;
                    rs_aval_t* aval;

                    atype = rs_cons_atype(cd, constraint);
                    aval = rs_cons_aval(cd, constraint);
                    ss_dassert(atype);

                    if (aval != NULL && !rs_aval_isnull(cd, atype, aval)) {
                        /* If it is possible to find out that like string
                         * is of no use, we neglect the whole selectivity
                         */
#ifdef SS_UNICODE_DATA
                        size_t fixedprefixlen;
                        size_t numwildcards;

                        fixedprefixlen = rs_cons_likeprefixinfo(
                                            cd,
                                            constraint,
                                            NULL,
                                            &numwildcards);
                        if (fixedprefixlen == 0 && numwildcards != 0) {
                            return(rs_sqli_no_selectivity(sqli));
                        } else {
                            return(get_like_selectivity(
                                        cd,
                                        relh,
                                        cons_info,
                                        constraint,
                                        rs_aval_va(cd, atype, aval)));
                        }
#else /* SS_UNICODE_DATA */
                        char* like_string;
                        like_string = rs_aval_getasciiz(cd, atype, aval);
                        ss_dassert(like_string != NULL);
                        if (*like_string == '%' || *like_string == '_') {
                            return(rs_sqli_no_selectivity(sqli));
                        } else {
                            *p_isrange = TRUE;
                            return(get_like_selectivity(
                                        cd,
                                        relh,
                                        cons_info,
                                        constraint,
                                        rs_aval_va(cd, atype, aval)));
                        }
#endif /* SS_UNICODE_DATA */
                    } else {
                        return(rs_sqli_like_selectivity(sqli));
                    }
                }
            default:
                su_rc_error(relop);
                return(0);/* To supress compiler warning */
        }
}

/*##**********************************************************************\
 *
 *		selectivity_for_constraint
 *
 * Estimates the selectivity for a given constraint
 *
 * Parameters :
 *
 *	constraint - in, use
 *		constraint
 *
 *
 * Output params:
 *
 * Return value :
 *      selectivity
 *
 * Limitations  :
 *
 * Globals used :
 */
static double selectivity_for_constraint(cd, relh, cons_info, constraint)
        rs_sysi_t*   cd;
        rs_relh_t*   relh;
        cons_info_t* cons_info;
        rs_cons_t*   constraint;
{
        bool succp;
        double selectivity;
        rs_sqlinfo_t* sqli;

        ss_dassert(constraint);

        sqli = rs_sysi_sqlinfo(cd);

        if (rs_cons_isalwaysfalse(cd, constraint)) {
            return(0.0);
        }

        switch(rs_cons_relop(cd, constraint)) {
            case RS_RELOP_NOTEQUAL:
                succp = data_sample_selectivity_relop(
                            cd,
                            relh,
                            cons_info,
                            constraint,
                            RS_RELOP_NOTEQUAL,
                            NULL,
                            &selectivity);
                if (succp) {
                    return(selectivity);
                } else {
                    return(rs_sqli_notequal_selectivity(sqli));
                }
            case RS_RELOP_ISNOTNULL:
                succp = data_sample_selectivity_relop(
                            cd,
                            relh,
                            cons_info,
                            constraint,
                            RS_RELOP_NOTEQUAL,
                            VA_NULL,
                            &selectivity);
                if (succp) {
                    return(selectivity);
                } else {
                    return(rs_sqli_isnotnull_selectivity(sqli));
                }
            case RS_RELOP_ISNULL:
                succp = data_sample_selectivity_relop(
                            cd,
                            relh,
                            cons_info,
                            constraint,
                            RS_RELOP_EQUAL,
                            VA_NULL,
                            &selectivity);
                if (succp) {
                    return(selectivity);
                } else {
                    return(rs_sqli_isnull_selectivity(sqli));
                }
            case RS_RELOP_GT:
            case RS_RELOP_GT_VECTOR:
            case RS_RELOP_GE:
            case RS_RELOP_GE_VECTOR:
            case RS_RELOP_LT:
            case RS_RELOP_LT_VECTOR:
            case RS_RELOP_LE:
            case RS_RELOP_LE_VECTOR:
                succp = data_sample_selectivity_relop(
                            cd,
                            relh,
                            cons_info,
                            constraint,
                            rs_cons_relop(cd, constraint),
                            NULL,
                            &selectivity);
                if (succp) {
                    return(selectivity);
                } else {
                    return(rs_sqli_compare_selectivity(sqli));
                }
            case RS_RELOP_EQUAL:
                if (rs_cons_aval(cd, constraint) == NULL &&
                    is_equal_constraint(
                        cd,
                        cons_info,
                        rs_cons_ano(cd, constraint),
                        TRUE)) {
                    /* This constraint info is NULL (not known) and there
                     * is known constarint for this column. Do not decrease
                     * selectivity. This case is possible in cases where
                     * SQL interpreter copies constraints from one row to
                     * another (?).
                     */
                    return(rs_sqli_no_selectivity(sqli));
                }
                succp = data_sample_selectivity_relop(
                            cd,
                            relh,
                            cons_info,
                            constraint,
                            RS_RELOP_EQUAL,
                            NULL,
                            &selectivity);
                if (succp) {
                    return(selectivity);
                } else {
                    return(rs_sqli_equal_selectivity(sqli));
                }
            case RS_RELOP_LIKE:
                {
                    rs_atype_t* atype;
                    rs_aval_t* aval;

                    atype = rs_cons_atype(cd, constraint);
                    aval = rs_cons_aval(cd, constraint);
                    ss_dassert(atype);

                    if (aval != NULL && !rs_aval_isnull(cd, atype, aval)) {
                        return(get_like_selectivity(
                                    cd,
                                    relh,
                                    cons_info,
                                    constraint,
                                    rs_aval_va(cd, atype, aval)));
                    } else {
                        return(rs_sqli_like_selectivity(sqli));
                    }
                }
            default:
                ss_error;
                return(0); /* To supress compiler warning */
        }
}

/*##**********************************************************************\
 *
 *		contains_select_list
 *
 * Inspects if the key contains all the attributes wanted in the select
 * list of the query
 *
 * Parameters :
 *
 *	key - in, use
 *		handle to the key
 *
 *	select_list - in, use
 *		list of columns wanted in the select list
 *
 *
 * Output params:
 *
 * Return value :
 *      TRUE if contains
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool contains_select_list(cd, key, select_list)
        rs_sysi_t*  cd;
        rs_key_t*   key;
        int*        select_list;
{
        int*        n;

        ss_dassert(select_list);
        ss_dassert(key);

        n = select_list;
        while (*n != RS_ANO_NULL) {
            if (*n != RS_ANO_PSEUDO &&
                rs_key_searchkpno_data(cd, key, (rs_ano_t)*n) == RS_ANO_NULL)
            {
                return (FALSE);
            }
            n++;
        }
        return(TRUE);
}

#ifdef SS_FAKE

static double est_fake_sigfpe(double d1, double d2)
{
        double drc;

        ss_dprintf_3(("est_fake_sigfpe:d1=%.1lf, d2=%.1lf\n", d1, d2));

        drc = pow(d1, d2);

        ss_dprintf_3(("est_fake_sigfpe:rdc=%.1lf\n", drc));

        return(drc);
}

#endif /* SS_FAKE */

/*##**********************************************************************\
 *
 *		time_estimate_for_key
 *
 * Estimates the search time for given query using the given key in
 * microseconds.
 *
 * Parameters :
 *
 *	table - in, use
 *		handle to the table of the query
 *
 *	key - in, use
 *		handle to the key
 *
 *	cons_info - in, use
 *		search constraints
 *
 *	select_list - in, use
 *		list of the columns the SQL interpreter wants
 *
 *      global_sel - in, use
 *		the total selectivity of all constraints
 *          calculated by the heuristic rules
 *
 *      total_search_time - out, give
 *		estimated total search time
 *
 *      delay_at_start - out, give
 *		estimated time at the start of the search
 *
 *      average_delay_per_row - out, give
 *		estimated additional time for each row
 *          in the result set
 *
 *      n_rows_in_result_set - out, give
 *		estimated number of rows in the result set
 *
 *      unique_value - out, give
 *		TRUE if the key is unique and the constraints
 *          determine a unique value
 *
 *      full_scan - out, give
 *		TRUE if the full table scan is used
 *
 *      must_retrieve - out, give
 *          TRUE if the search is using a non-clustering index and must
 *          retrieve also data tuples from the clustering index
 *
 * Return value :
 *      estimated time for the search in microseconds
 *
 * Limitations  :
 *
 * Globals used :
 */
static void time_estimate_for_key(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   table,
        rs_key_t*    key,
        cons_info_t* cons_info,
        long         n_matching_order_bys __attribute__ ((unused)),
        int*         select_list,
        double       global_sel,
        double*      total_search_time,
        double*      delay_at_start,
        double*      average_delay_per_row,
        double*      n_rows_in_result_set,
        bool*        unique_value,
        bool*        full_scan,
        bool*        must_retrieve)
{
        double      tuple_size;             /* data tuple size in table
                                               (actually the average distance
                                               of two entries in the
                                               clustering index) */
        double      key_entry_size;         /* index entry size in the
                                               index used */
        double      pool_per_table_size;    /* pool size per table size */
        ss_int8_t   n_bytes_i8;
        double      n_bytes;                /* Size of the clustering index
                                               of the table in bytes. If it
                                               is prejoined with another
                                               relation, this should be
                                               the sum of the sizes of the
                                               two relations. */
        ss_int8_t   n_rows_i8;
        double      n_rows;                 /* size of the table in rows */
        double      hit_rate_for_index;     /* estimated hit rate for an
                                               index block in the index
                                               used (which may also
                                               be the clustering index of
                                               the table) */
        double      hit_rate_for_data = 0;  /* estimated hit rate for a
                                               data (= clustering
                                               index) block, if using
                                               non-clustering index and
                                               data retrieving from
                                               clustering index necessary */
        ulong       n_constraints;          /* number of constraints in
                                               constraint list */
        double      index_range_selectivity;/* range selectivity on the index
                                               used */
        double      total_index_selectivity;/* product of range selectivity
                                               and pointwise selectivity
                                               on the index used */
        ulong        n_solved;               /* number of constraints which
                                               can be solved on this key
                                               without retrieving
                                               data tuple */
        double      n_index_entries_in_range; /* number of index entries
                                               in key range */
        double      n_index_entries_in_set; /* number of index entries
                                               in the key range after
                                               pointwise selection */
        double      time_for_range_search;  /* estimated time used in range
                                               search on the index used */
        double      time_for_retrieve;      /* in the case of a nonclustering
                                               index, the estimated time
                                               for retrieving the data
                                               tuples */
        rs_sqlinfo_t* sqli;                 /* SQL info object for estimator
                                               constants.*/
        bool mainmem;
        char info_buf[256];

        ss_dprintf_3(("time_estimate_for_key:table '%s', key '%s'\n",
            rs_relh_name(cd, table), rs_key_name(cd, key)));

        ss_dassert(table);
        ss_dassert(key);
        ss_dassert(select_list);
        ss_dassert(cons_info);

        FAKE_CODE_RESET(FAKE_EST_ARITHERROR, { tuple_size = est_fake_sigfpe(0.0, -2.0); } )

        sqli = rs_sysi_sqlinfo(cd);
        mainmem = (rs_relh_reltype(cd, table) == RS_RELTYPE_MAINMEMORY);

        ss_dprintf_4(("*** STEP 0. FINDING THE DATA SIZE OF THE TABLE ***\n"));
        SS_PUSHNAME("time_estimate_for_key:STEP 0");

        if (est_test_version_on) {
            n_bytes = est_test_n_bytes;
            n_rows  = est_test_n_rows;
        } else if (mainmem && rs_relh_isglobaltemporary(cd, table)) {
            dbe_db_gettemporarytablecardin(
                    cd, rs_sysi_db(cd), table, &n_rows_i8, &n_bytes_i8);
            SsInt8ConvertToDouble(&n_rows, n_rows_i8);
            SsInt8ConvertToDouble(&n_bytes, n_bytes_i8);
        } else {
            bool got_cardin = FALSE;
            dbe_trx_t* trx;

            trx = tb_trans_dbtrx(cd, trans);
            if (trx != NULL) {
                /* In SA transactions the trx may be NULL. */
                dbe_trx_getrelhcardin(trx, table, &n_rows_i8, &n_bytes_i8);
                got_cardin = TRUE;
            }
            if (!got_cardin) {
                rs_relh_cardininfo(cd, table, &n_rows_i8, &n_bytes_i8);
            }
            SsInt8ConvertToDouble(&n_rows, n_rows_i8);
            SsInt8ConvertToDouble(&n_bytes, n_bytes_i8);
        }
        /* If the table seems to be empty, put some default values
           to n_bytes and n_rows. This is to prevent divisions by zero. */
        if (n_bytes == 0 || n_rows == 0) {
            n_bytes = EST_N_BYTES_IN_EMPTY_TABLE;
            n_rows  = EST_N_ROWS_IN_EMPTY_TABLE;
        }

        ss_dprintf_4(("time_estimate_for_key:n_bytes=%lf\n", n_bytes));
        ss_dprintf_4(("time_estimate_for_key:n_rows=%lf\n", n_rows));

        ss_dprintf_4(("*** STEP 1. CALCULATION OF INDEX ENTRY AND DATA TUPLE SIZES ***\n"));
        SS_POPNAME;
        SS_PUSHNAME("time_estimate_for_key:STEP 1");

        /* We calculate the tuple size. If the data is prejoined, tuple_size
            actually means the mean distance between two entries of
            this relation. */
        tuple_size = (double)n_bytes / (double)n_rows;

#ifdef SS_MME
        if (mainmem) {
            /* In MME, the key entry size is always the same for all keys. */
            key_entry_size = 1.0;
        } else
#endif
        {
            /* In calculating the key entry size we take into account also
               the possible prejoining. */
            if (key == rs_relh_clusterkey(cd, table)) {
                key_entry_size = tuple_size;  /* key entry is the data tuple */
            } else {
                if (est_test_version_on) {
                    key_entry_size = tuple_size/rs_sqli_data_size_per_index(sqli);
                } else {
                    key_entry_size = rs_key_maxstoragelen(cd, key);
                }
                if (key_entry_size > rs_sqli_max_key_entry_size(sqli)) {
                    key_entry_size = rs_sqli_max_key_entry_size(sqli);
                }
            }

            if (rs_key_isprejoined(cd, key)) {
                key_entry_size = key_entry_size / rs_sqli_prejoin_density(sqli);
            }
            if (key_entry_size > tuple_size && !est_test_version_on) {
                key_entry_size = tuple_size;
            }
        }

        ss_dprintf_4(("time_estimate_for_key:tuple_size=%lf\n", tuple_size));
        ss_dprintf_4(("time_estimate_for_key:key_entry_size=%lf\n", key_entry_size));

        ss_dprintf_4(("*** STEP 2. CALCULATION OF INDEX HIT RATE ***\n"));
        SS_POPNAME;
        SS_PUSHNAME("time_estimate_for_key:STEP 2");

        /* Calculate the estimated hit rate for reading the index blocks */
        if (mainmem) {
            hit_rate_for_index = 1.0;
        } else {
            if (cons_info->cons_infolevel >= 11) {
                SsSprintf(info_buf, "  Pool size for query=%ld, key_entry_size=%.1lf, tuple_size=%.1lf\n", db_info_poolsize(cd), key_entry_size, tuple_size);
                tb_info_print(cd, NULL, cons_info->cons_infolevel, info_buf);
            }

            hit_rate_for_index = db_info_poolsize(cd) /
                                        (key_entry_size * n_rows);
            /* this calculation is relevant if the range does not fit in memory
                pool */

            if (hit_rate_for_index > rs_sqli_max_hit_rate_for_index(sqli)) {
                hit_rate_for_index = rs_sqli_max_hit_rate_for_index(sqli);
            } else if (hit_rate_for_index < rs_sqli_min_hit_rate_for_index(sqli)) {
                hit_rate_for_index = rs_sqli_min_hit_rate_for_index(sqli);
            }
        }

        ss_dprintf_4(("time_estimate_for_key:hit_rate_for_index=%lf\n", hit_rate_for_index));

        ss_dprintf_4(("*** STEP 3. CALCULATION OF RANGE AND TOTAL SELECTIVITIES ON INDEX ***\n"));
        SS_POPNAME;
        SS_PUSHNAME("time_estimate_for_key:STEP 3");

        index_range_selectivity =
            range_selectivity_for_key(cd, table, key, cons_info,
                                      unique_value, full_scan);

        total_index_selectivity =
            total_selectivity_for_key(cd, table, key, cons_info, (long *)&n_solved);

        /* check if we must retrieve data tuples from another index */

        n_constraints = su_list_length(cons_info->cons_list);

#ifdef SS_MME
        if (mainmem) {
            /* In MME, all indexes point directly to the actual values,
               no separate retrieve is ever needed. */
            *must_retrieve = FALSE;
        } else
#endif
            *must_retrieve = (n_solved < n_constraints
                              || !contains_select_list(cd, key, select_list));

        ss_dprintf_4(("time_estimate_for_key:index_range_selectivity=%lf\n", index_range_selectivity));
        ss_dprintf_4(("time_estimate_for_key:total_index_selectivity=%lf\n", total_index_selectivity));

        ss_dprintf_4(("*** STEP 4. CALCULATION OF SIZES OF SEARCH SETS ***\n"));
        SS_POPNAME;
        SS_PUSHNAME("time_estimate_for_key:STEP 4");

        /* In the case of *unique_value == TRUE, we assume that the range
            would contain exactly one entry, if there were no other
            constraints */
        if (*unique_value) {
            n_index_entries_in_range = 1.0;
            n_index_entries_in_set =
                1 * total_index_selectivity / index_range_selectivity;
            *n_rows_in_result_set = 1 * global_sel
                                        / index_range_selectivity;
        } else {
            n_index_entries_in_range = index_range_selectivity * n_rows;
            n_index_entries_in_set   = total_index_selectivity * n_rows;
            *n_rows_in_result_set    = global_sel              * n_rows;
        }

        ss_dprintf_4(("time_estimate_for_key:n_index_entries_in_range=%lf\n", n_index_entries_in_range));
        ss_dprintf_4(("time_estimate_for_key:n_index_entries_in_set=%lf\n", n_index_entries_in_set));
        ss_dprintf_4(("time_estimate_for_key:n_rows_in_result_set=%lf\n", *n_rows_in_result_set));

        ss_dprintf_4(("*** STEP 5. CALCULATION OF THE INDEX SEARCH TIME ***\n"));
        SS_POPNAME;
        SS_PUSHNAME("time_estimate_for_key:STEP 5");

        if (mainmem) {
            time_for_range_search =
                ceil(n_index_entries_in_range * key_entry_size
                                        / db_info_blocksize(cd))
                                /* above is the number of blocks in range */
                * (2 * rs_sqli_time_for_index_search(sqli)
                + (1 - hit_rate_for_index) * MAINMEM_BLOCK_ACCESS_TIME)
                                /* above is the access time per block */
                + n_index_entries_in_range * rs_sqli_time_per_index_entry(sqli);
                                /* above is the time for scanning */
        } else {
            time_for_range_search =
                ceil(n_index_entries_in_range * key_entry_size
                                        / db_info_blocksize(cd))
                                /* above is the number of blocks in range */
                * (2 * rs_sqli_time_for_index_search(sqli)
                + (1 - hit_rate_for_index) * rs_sqli_block_access_time(sqli))
                                /* above is the access time per block */
                + n_index_entries_in_range * rs_sqli_time_per_index_entry(sqli);
                                /* above is the time for scanning */
        }

        ss_dprintf_4(("time_estimate_for_key:blocks in range=%lf\n",
            (double)ceil(n_index_entries_in_range * key_entry_size / db_info_blocksize(cd))));
        if (mainmem) {
            ss_dprintf_4(("time_estimate_for_key:access time per block=%lf\n",
                (2 * rs_sqli_time_for_index_search(sqli)
                + (1 - hit_rate_for_index) * MAINMEM_BLOCK_ACCESS_TIME)));
        } else {
            ss_dprintf_4(("time_estimate_for_key:access time per block=%lf\n",
                (2 * rs_sqli_time_for_index_search(sqli)
                + (1 - hit_rate_for_index) * rs_sqli_block_access_time(sqli))));
        }
        ss_dprintf_4(("time_estimate_for_key:time for scanning=%lf\n",
            n_index_entries_in_range * rs_sqli_time_per_index_entry(sqli)));
        ss_dprintf_4(("time_estimate_for_key:time_for_range_search=%lf\n", time_for_range_search));

        ss_dprintf_4(("*** STEP 6. CALCULATION OF THE POSSIBLE DATA TUPLE RETRIEVE TIME ***\n"));
        SS_POPNAME;
        SS_PUSHNAME("time_estimate_for_key:STEP 6");

        if (*must_retrieve) {

            ss_dprintf_4(("*** STEP 6.1. CALCULATING THE DATA HIT RATE ***\n"));

            pool_per_table_size =
                 (double)db_info_poolsize(cd) / (double)n_bytes;

            if (mainmem) {
                hit_rate_for_data = 1.0;
            } else if (pool_per_table_size < rs_sqli_max_hit_rate_for_data(sqli)) {
                ss_dprintf_4(("time_estimate_for_key:hit_rate_for_data is pool_per_table_size\n"));
                hit_rate_for_data = pool_per_table_size;
            } else if (pool_per_table_size > 1.0) {
                /* we may assume that the query can load the whole
                   table into memory if it needs */
                hit_rate_for_data =
                    1.0 - ceil((double)n_bytes /
                                     (double) db_info_blocksize(cd))
                                    /* ceil above is the number of blocks */
                         / ceil(n_index_entries_in_set);
                                    /* we divide it by the number of
                                       retrieves necessary */

                if (hit_rate_for_data > rs_sqli_max_hit_rate_for_data(sqli)) {
                    hit_rate_for_data = rs_sqli_max_hit_rate_for_data(sqli);
                } else if (hit_rate_for_data < rs_sqli_min_hit_rate_for_data(sqli)) {
                    hit_rate_for_data = rs_sqli_min_hit_rate_for_data(sqli);
                }
                ss_dprintf_4(("time_estimate_for_key:number of blocks=%lf\n",
                    (double)ceil((double)n_bytes /
                                     (double) db_info_blocksize(cd))));
            } else {
                ss_dprintf_4(("time_estimate_for_key:hit_rate_for_data is rs_sqli_hit_rate_for_data(sqli)\n"));
                hit_rate_for_data = rs_sqli_max_hit_rate_for_data(sqli);
            }

            ss_dprintf_4(("time_estimate_for_key:hit_rate_for_data=%lf\n", hit_rate_for_data));

            ss_dprintf_4(("*** STEP 6.2. CALCULATING THE RETRIEVE TIME ***\n"));

            if (mainmem) {
#ifdef SS_MME
                /* in case of MME there is no extra effort to retrieve
                   data tuples
                 */
                time_for_retrieve = 0.0;
#else
                time_for_retrieve =
                    n_index_entries_in_set *
                    (rs_sqli_time_for_index_search(sqli) +
                        (1 - hit_rate_for_data) * MAINMEM_BLOCK_ACCESS_TIME)
                                    /* each retrieve generates one block
                                    access */
                    + n_index_entries_in_set * rs_sqli_time_per_index_entry(sqli);
                                    /* above is the time for scanning the data
                                    tuple */
#endif
            } else {
                time_for_retrieve =
                    n_index_entries_in_set *
                    (rs_sqli_time_for_index_search(sqli) +
                        (1 - hit_rate_for_data) * rs_sqli_block_access_time(sqli))
                                    /* each retrieve generates one block
                                    access */
                    + n_index_entries_in_set * rs_sqli_time_per_index_entry(sqli);
                                    /* above is the time for scanning the data
                                    tuple */
            }

        } else {
            time_for_retrieve = 0.0;
        }

        ss_dprintf_4(("time_estimate_for_key:time_for_retrieve=%lf\n", time_for_retrieve));
        ss_dprintf_4(("time_estimate_for_key:*must_retrieve=%d\n", *must_retrieve));

        ss_dprintf_4(("*** STEP 7. CALCULATION OF THE START DELAY AND THE ROW DELAY ***\n"));
        SS_POPNAME;
        SS_PUSHNAME("time_estimate_for_key:STEP 7");

        *total_search_time = time_for_range_search + time_for_retrieve;

        /* Let us then calculate the parameters Sepe wants for the
            interpreter: the delay at the start and
            the additional average delay per row for the rows.
        */

        if (mainmem) {
#ifdef SS_MME
            /* For MME, there's no special start delay. */
            *delay_at_start = 0.0;
#else
            *delay_at_start = 2 * rs_sqli_time_for_index_search(sqli) +
                    (1 - hit_rate_for_index) * MAINMEM_BLOCK_ACCESS_TIME;
#endif
        } else {
            *delay_at_start = 2 * rs_sqli_time_for_index_search(sqli) +
                    (1 - hit_rate_for_index) * rs_sqli_block_access_time(sqli);
        }

        if (*must_retrieve) {
            if (mainmem) {
                *delay_at_start = *delay_at_start + rs_sqli_time_for_index_search(sqli)
                    + (1 - hit_rate_for_data) * MAINMEM_BLOCK_ACCESS_TIME;
            } else {
                *delay_at_start = *delay_at_start + rs_sqli_time_for_index_search(sqli)
                    + (1 - hit_rate_for_data) * rs_sqli_block_access_time(sqli);
            }
        }

        if (*delay_at_start > *total_search_time) {
            *delay_at_start = *total_search_time;
        }

#if 0 /* Jarmo removed, Mar 27, 1995 */
        if (*n_rows_in_result_set > 1.0) {
            *average_delay_per_row =
                (*total_search_time - *delay_at_start)
                / *n_rows_in_result_set;
        } else {
            *delay_at_start = *total_search_time;
            *average_delay_per_row = 0.0;
        }
#else
        if (*n_rows_in_result_set >= 2.0) {
            double total_time_for_rows;
            total_time_for_rows = *total_search_time - *delay_at_start;
            *average_delay_per_row =
                total_time_for_rows / *n_rows_in_result_set;
        } else if (*total_search_time >= *delay_at_start) {
            *average_delay_per_row = *total_search_time - *delay_at_start;
        } else {
            *delay_at_start = *total_search_time;
            *average_delay_per_row = 0.0;
        }
#endif
        if (*average_delay_per_row < 0.0) {
            ss_dprintf_1(("adpr=%lf.\n", *average_delay_per_row));
            *average_delay_per_row = 0.0;
        }
        ss_assert(*average_delay_per_row >= 0.0);

#ifdef SS_MME
        if (mainmem) {
            *average_delay_per_row =
                *average_delay_per_row
                * rs_key_costfactor(cd, key, rs_relh_ttype(cd, table));
        }
#endif

        ss_dprintf_4(("total time = %.2lf, start delay = %.2lf, row delay = %.2lf\n",
            *total_search_time, *delay_at_start, *average_delay_per_row));
        ss_dprintf_4(("nrows = %.2lf, unique value = %d\n",
            *n_rows_in_result_set, *unique_value));

        SS_POPNAME;

/* Sorry this function is so long!!!!!! */
}

/*#***********************************************************************\
 *
 *		is_equal_constraint
 *
 * Checks if there is an equal constraint on attribute column_no.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	cons_info - in
 *		list of constraints
 *
 *	column_no - in
 *		column number where the equal constraint must exist
 *
 * Return value :
 *
 *      TRUE    - there is equal constraint
 *      FALSE   - there is no equal constraint
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool is_equal_constraint(cd, cons_info, column_no, non_null)
        rs_sysi_t*   cd;
        cons_info_t* cons_info;
        uint         column_no;
        bool         non_null;
{
        su_list_t*          consbyano_list;
        su_list_node_t*     n;
        rs_cons_t*          constraint;

        ss_dassert(cons_info);

        if (!su_pa_indexinuse(cons_info->cons_byano, column_no)) {
            return(FALSE);
        }

        consbyano_list = su_pa_getdata(cons_info->cons_byano, column_no);

        su_list_do_get(consbyano_list, n, constraint) {
            ss_dassert(rs_cons_ano(cd, constraint) == (rs_ano_t)column_no);
            if (non_null && rs_cons_aval(cd, constraint) == NULL) {
                continue;
            }
            switch (rs_cons_relop(cd, constraint)) {
                case RS_RELOP_EQUAL:
                case RS_RELOP_ISNULL:
                    return(TRUE);
                default:
                    break;
            }
        }
        return(FALSE);
}

/*##**********************************************************************\
 *
 *		count_matching_order_bys
 *
 * Calculates the number of attributes in a key which match to order by
 * criteria
 *
 * Parameters :
 *
 *	    key - in, use
 *		    handle to the key
 *
 *	    order_by_list - in, use
 *		    list of order by criteria
 *
 *      cons_info - in
 *		    list of constraints
 *
 * Output params:
 *
 * Return value :
 *      number of matching attributes
 *
 * Limitations  :
 *
 * Globals used :
 */
static long count_matching_order_bys(
        rs_sysi_t*   cd,
        rs_key_t*    key,
        su_list_t*   order_by_list,
        cons_info_t* cons_info)
{
        rs_ano_t    maxj;
        rs_ano_t    j;
        bool        fails;
        su_list_node_t* n;
        rs_ob_t*    order_by;
        long        n_matching_order_bys;
        bool        first_ob;

        ss_dassert(key);
        ss_dassert(order_by_list);

        maxj = rs_key_nparts(cd, key);
        j = 0;
        fails = FALSE;
        n = su_list_first(order_by_list);
        n_matching_order_bys = 0;

        /* go forward on key parts and order by's
         */
        first_ob = FALSE;
        while (j < maxj && !fails && n != NULL) {
            order_by = su_listnode_getdata(n);

            if (rs_keyp_isconstvalue(cd, key, j)) {
                /* constvalue key parts do not affect ordering */
                j++;
            } else if (!first_ob &&
                        is_equal_constraint(cd, cons_info,
                                            rs_keyp_ano(cd, key, j), FALSE)) {
                /* Check if there is also a matching ordering criterium for
                 * this attribute.
                 */
                if (   (rs_ob_ano(cd, order_by) ==
                                rs_keyp_ano(cd, key, j))
                    && (rs_ob_asc(cd, order_by) ==
                             rs_keyp_isascending(cd, key, j))
                ) {
                    /* There is also an order by to the first ordering
                     * criterium.
                     */
                    n_matching_order_bys++;
                    n = su_list_next(order_by_list, n);
                }
                /* equal key parts before first order by do not affect
                   ordering */
                j++;
            } else {
                if (   (rs_ob_ano(cd, order_by) !=
                                rs_keyp_ano(cd, key, j))
                    || (rs_ob_asc(cd, order_by) !=
                             rs_keyp_isascending(cd, key, j))
                ) {
                    /* the engine does not currently support descending
                       search */
                    fails = TRUE;
                } else {
                    n_matching_order_bys++;
                }
                first_ob = TRUE;
                j++;
                n = su_list_next(order_by_list, n);
            }
        }
        if (!fails && rs_key_isunique(cd, key)) {
            /* We have succesfully scanned through the key parts. The whole
             * key is a prefix of order by list. If the key is unique, it
             * succesfully solves all ordering criteria.
             */
            n_matching_order_bys = su_list_length(order_by_list);
        }
        return(n_matching_order_bys);
}

/*##**********************************************************************\
 *
 *		check_distinct_result_set
 *
 * Checks if result set is distinct, i.e. all columns of some unique key
 * are in select list.
 *
 * Parameters :
 *
 *	    key - in, use
 *		    handle to the key
 *
 *	    select_list - in
 *		    array of selected columns
 *
 *      distinct_values - out
 *		    TRUE is set to *distinct_values if resulst set is distinct.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void check_distinct_result_set(
        rs_sysi_t*   cd __attribute__ ((unused)),
        rs_key_t*    key,
        int*         select_list,
        bool*        distinct_values)
{
        rs_ano_t    j;
        rs_ano_t    lastordering;

        ss_dprintf_3(("check_distinct_result_set\n"));
        ss_dassert(key);

        if (*distinct_values) {
            /* Value is already checked to be distinct. */
            ss_dprintf_4(("check_distinct_result_set:*distinct_values=%d\n", *distinct_values));
            return;
        }
        if (!rs_key_isunique(cd, key)) {
            /* Value can not be distinct. */
            ss_dprintf_4(("check_distinct_result_set:rs_key_isunique(cd, key)=%d\n", rs_key_isunique(cd, key)));
            return;
        }

        lastordering = rs_key_lastordering(cd, key);

        /* go forward on key parts
         */
        for (j = rs_key_first_datapart(cd, key); j <= lastordering; j++) {
            int i;
            bool found;
            rs_ano_t ano;

            ano = rs_keyp_ano(cd, key, j);
            found = FALSE;

            for (i = 0; select_list[i] != RS_ANO_NULL; i++) {
                if (select_list[i] == ano) {
                    found = TRUE;
                    break;
                }
            }
            if (!found) {
                /* Column not in select list, can not be distinct. */
                ss_dprintf_4(("check_distinct_result_set:column %d not in select list\n", ano));
                return;
            }
        }

        /* All unique key parts are selected, value is distinct. */
        ss_dprintf_4(("check_distinct_result_set:set *distinct_values = TRUE\n"));
        *distinct_values = TRUE;
}

/*##**********************************************************************\
 *
 *		tb_est_create_estimate
 *
 * Selects the best key for a search and calculates the
 * cost estimates and stores them to an estimate object.
 * In the selection we use the following principles:
 * 1) first we restrict ourselves to the keys with most matching
 *    ordering criteria;
 * 2) if among them there are unique keys with matching criteria, we
 *    choose an unique one with the shortest total search time,
 *    else we select among them any key with the shortest
 *    total search time.
 *
 * Parameters :
 *
 *	table - in, use
 *		handle to the table
 *
 *	constraint_list - in, use
 *		list of search constraints
 *
 *	select_list - in, use
 *		list of those columns which are wanted
 *          in the result set
 *
 *      order_by_list - in, use
 *		list of ordering criteria
 *
 *
 *      infolevel - in
 *		info level for SQL info output
 *
 *
 *      indexhint - in
 *		If non-NULL, index hint name. Forces to use the named index.
 *          Empty string means clustering key.
 *
 * Output params:
 *
 * Return value - give :
 *      the estimate object
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_est_t* tb_est_create_estimate(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        rs_relh_t*  table,
        su_list_t*  constraint_list,
        int*        select_list,
        su_list_t*  order_by_list,
        int         infolevel,
        rs_key_t*   indexhintkey)
{
        su_pa_t*    key_list;
        rs_key_t*   key;
        uint        k;
        double      global_sel;

        rs_key_t*   best_key_so_far;
        long        best_n_matching_order_bys = 0;
        bool        best_unique_value = FALSE;  /* parameters for the best */
        double      best_total_search_time = 0; /* key so far */
        double      best_delay_at_start = 0;
        double      best_average_delay_per_row = 0;
        double      best_n_rows_in_result_set = 0;
        bool        best_full_scan = FALSE;
        bool        best_must_retrieve = FALSE;

        long        n_matching_order_bys;
        double      total_search_time;
        double      delay_at_start;
        double      average_delay_per_row;
        double      n_rows_in_result_set;
        bool        unique_value;
        bool        full_scan;
        bool        must_retrieve;
        bool        ignore_order_by;
        bool        index_foundp = FALSE;
        bool        distinct_values = FALSE;

        tb_est_t*   estimate;

        cons_info_t cons_info;
        rs_cons_t* cons;
        su_list_t* consbyano_list;
        su_list_node_t* n;
        rs_sqlinfo_t* sqli;
        su_profile_timer;

        ss_trigger("tb_est");

        ss_dprintf_1(("tb_est_create_estimate:indexhint='%s'\n",
            indexhintkey != NULL ? rs_key_name(cd, indexhintkey) : "NONE"));
        ss_assert(constraint_list);
        ss_assert(select_list);
        ss_assert(order_by_list);
        ss_assert(table);
        ss_dassert(sizeof(rs_estcost_t) == sizeof(double));
        SS_PUSHNAME("tb_est_create_estimate");

        sqli = rs_sysi_sqlinfo(cd);

        cons_info.cons_list = constraint_list;
        cons_info.cons_byano = su_pa_init();
        cons_info.cons_selectivarrcount = 0;
        cons_info.cons_selectivarrsize = su_list_length(constraint_list);
        cons_info.cons_selectivarr =
            SsMemAlloc(cons_info.cons_selectivarrsize *
                       sizeof(cons_info.cons_selectivarr[0]));

        estimate = SsMemAlloc(sizeof(tb_est_t));

        ss_debug(estimate->e_chk = TBCHK_EST);
        estimate->e_rowcounts = SsMemCalloc(
                                    sizeof(estimate->e_rowcounts[0]),
                                    rs_relh_nattrs(cd, table));

        cons_info.cons_rowcounts = estimate->e_rowcounts;
        SsInt8ConvertToDouble(&(cons_info.cons_relntuples), rs_relh_ntuples(cd, table));
        cons_info.cons_vectoroptn = 0;
        cons_info.cons_nvectorconstraint = 0;

        /* Gather constraints for each attribute to separate lists in
         * in cons_info.cons_byano indexed by ano.
         */
        su_list_do_get(constraint_list, n, cons) {
            rs_ano_t ano;
            switch (rs_cons_relop(cd, cons)) {
                case RS_RELOP_GT_VECTOR:
                case RS_RELOP_GE_VECTOR:
                case RS_RELOP_LT_VECTOR:
                case RS_RELOP_LE_VECTOR:
                    cons_info.cons_nvectorconstraint++;
                    cons_info.cons_vectoroptn = rs_sqli_getvectoroptn(sqli);
                    break;
                default:
                    break;
            }
            ano = rs_cons_ano(cd, cons);
            ss_dassert(ano != RS_ANO_NULL);
            if (!su_pa_indexinuse(cons_info.cons_byano, ano)) {
                consbyano_list = su_list_init(NULL);
                su_pa_insertat(cons_info.cons_byano, ano, consbyano_list);
            } else {
                consbyano_list = su_pa_getdata(cons_info.cons_byano, ano);
            }
            su_list_insertlast(consbyano_list, cons);
        }

        if (rs_sysi_simpleoptimizerrules(cd, cons_info.cons_relntuples) 
            || indexhintkey != NULL) 
        {
            cons_info.cons_fixedest = TRUE;
        } else {
            cons_info.cons_fixedest = FALSE;
        }
        ss_dprintf_4(("tb_est_create_estimate:rows in table %lf, cons_fixedest %d\n", cons_info.cons_relntuples, cons_info.cons_fixedest));

        cons_info.cons_rangeest = rs_sqli_userangeest(sqli);
        cons_info.cons_infolevel = infolevel;

        su_profile_start;

        key_list            = rs_relh_keys(cd, table);
        global_sel          = global_selectivity(cd, table, &cons_info);
        best_key_so_far     = NULL;
        ignore_order_by     = rs_sqli_estignoreorderby(sqli);

        SS_POPNAME;
        SS_PUSHNAME("tb_est_create_estimate:loop through keys");

        /* loop through the keys for the table */
        su_pa_do_get(key_list, k, key) {
            bool skip_key;

            if (!rs_key_index_ready(cd, key)) {
                continue;
            }

            check_distinct_result_set(cd, key, select_list, &distinct_values);

            if (indexhintkey != NULL) {
                if (indexhintkey != key) {
                    continue;
                }
            }
            n_matching_order_bys =
                count_matching_order_bys(cd, key, order_by_list, &cons_info);
            if (ignore_order_by) {
                skip_key = best_key_so_far != NULL
                           && best_unique_value
                           && !rs_key_isunique(cd, key);
            } else {
                skip_key = (best_key_so_far != NULL
                            && n_matching_order_bys < best_n_matching_order_bys)
                        || (best_key_so_far != NULL
                            && n_matching_order_bys == best_n_matching_order_bys
                            && best_unique_value
                            && !rs_key_isunique(cd, key));
            }
            if (!skip_key) {
                bool is_best_key;
                index_foundp = TRUE;
                time_estimate_for_key(
                    cd,
                    trans,
                    table,
                    key,
                    &cons_info,
                    n_matching_order_bys,
                    select_list,
                    global_sel,
                    &total_search_time,
                    &delay_at_start,
                    &average_delay_per_row,
                    &n_rows_in_result_set,
                    &unique_value,
                    &full_scan,
                    &must_retrieve);
                if (est_test_version_on || infolevel >= 5) {
                    char time_buf[30];
                    char info_buf[128];
                    SsDoubleToAsciiDecimals(total_search_time/1000.0, time_buf, 15, 2);
                    SsSprintf(info_buf, "  Total time for key %.50s %s milliseconds, norderby=%ld, unique=%d\n",
                        rs_key_name(cd, key), time_buf, n_matching_order_bys, unique_value);
                    if (est_test_version_on) {
                        SsPrintf("%s", info_buf);
                    }
                    if (infolevel >= 5) {
                        tb_info_print(cd, NULL, 8, info_buf);
                    }
                }
                if (ignore_order_by) {
                    is_best_key =    (best_key_so_far == NULL)
                                  || (!best_unique_value
                                      && unique_value)
                                  || (best_unique_value == unique_value
                                      && total_search_time < best_total_search_time);
                } else {
                    is_best_key =    (best_key_so_far == NULL)
                                  || (n_matching_order_bys > best_n_matching_order_bys)
                                  || (n_matching_order_bys == best_n_matching_order_bys
                                      && !best_unique_value
                                      && unique_value)
                                  || (n_matching_order_bys == best_n_matching_order_bys
                                      && best_unique_value == unique_value
                                      && total_search_time < best_total_search_time);
                }
                if (is_best_key) {
                    best_key_so_far             = key;
                    best_total_search_time      = total_search_time;
                    best_delay_at_start         = delay_at_start;
                    best_average_delay_per_row  = average_delay_per_row;
                    best_n_rows_in_result_set   = n_rows_in_result_set;
                    best_unique_value           = unique_value;
                    best_full_scan              = full_scan;
                    best_must_retrieve          = must_retrieve;
                    best_n_matching_order_bys   = n_matching_order_bys;
                }
            }
        }

        ss_assert(index_foundp);

        SS_POPNAME;
        SS_PUSHNAME("tb_est_create_estimate:key selected");

        estimate->e_key = best_key_so_far;
        estimate->e_full_scan = best_full_scan;
        estimate->e_must_retrieve = best_must_retrieve;
        estimate->e_unique_value = distinct_values || best_unique_value;
        estimate->e_single_row = best_unique_value;

        /* We have to prevent overflow when passing the
           values to the SQL interpreter. */
        if (best_delay_at_start < EST_MAX_DELAY_TIME) {
            estimate->e_delay_at_start =
                (rs_estcost_t)ceil(best_delay_at_start / 100);
        } else {
            estimate->e_delay_at_start =
                (rs_estcost_t)(EST_MAX_DELAY_TIME / 100);
        }
        if (best_average_delay_per_row < EST_MAX_DELAY_TIME) {
            estimate->e_delay_per_row =
                    (rs_estcost_t)ceil(best_average_delay_per_row / 100);
        } else {
            estimate->e_delay_per_row =
                (rs_estcost_t)(EST_MAX_DELAY_TIME / 100);
        }
        {
            rs_estcost_t nrows;
            nrows = (rs_estcost_t)ceil(best_n_rows_in_result_set);
            if (nrows >= LONG_MAX) {
                estimate->e_n_rows = LONG_MAX;
            } else {
                estimate->e_n_rows = (long)nrows;
            }

        }
        estimate->e_n_order_bys = (uint)best_n_matching_order_bys;
        estimate->e_cons_byano = cons_info.cons_byano;

        est_test_est = estimate;

        su_profile_stop("tb_est_create_estimate");

        ss_dprintf_2(("tb_est_create_estimate:table = %s, key = %s, nrows = %.1lf, norderbys = %d, delay = %.2lf+%.2lf\n",
            rs_relh_name(cd, table),
            rs_key_name(cd, estimate->e_key),
            (double)estimate->e_n_rows,
            estimate->e_n_order_bys,
            (double)estimate->e_delay_at_start,
            (double)estimate->e_delay_per_row));

        SsMemFree(cons_info.cons_selectivarr);

        CHK_EST(estimate);
        SS_POPNAME;
        ss_trigger("tb_est");

        return(estimate);
}


/*##**********************************************************************\
 *
 *		tb_est_get_delays
 *
 * Gets the estimated delays for the query. The delays are given
 * in units of 0.1 milliseconds.
 *
 * Parameters :
 *
 *	estimate - in, use
 *		estimate object
 *
 *
 *	delay_at_start - out, give
 *		delay at the start
 *
 *	average_delay_per_row - out, give
 *		additional average delay for the rows
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_est_get_delays(cd, estimate, delay_at_start, average_delay_per_row)
        rs_sysi_t*    cd;
        tb_est_t*     estimate;
        rs_estcost_t* delay_at_start;
        rs_estcost_t* average_delay_per_row;
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        *delay_at_start = estimate->e_delay_at_start;
        *average_delay_per_row = estimate->e_delay_per_row;
}

/*##**********************************************************************\
 *
 *		tb_est_get_n_rows
 *
 * Gets the estimated number of rows for the query
 *
 * Parameters :
 *
 *	estimate - in, use
 *		estimate object
 *
 *
 *      n_rows - out, give
 *		estimated number of rows
 *
 *
 * Return value :
 *      1 = APPROXIMATE NUMBER
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_est_get_n_rows(cd, estimate, nrows)
        rs_sysi_t*    cd;
        tb_est_t*     estimate;
        rs_estcost_t* nrows;
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        *nrows = (rs_estcost_t)(estimate->e_n_rows);
        return(1);
}

/*##**********************************************************************\
 *
 *		tb_est_get_n_order_bys
 *
 * Gets the number of matching order bys for the key
 *
 * Parameters :
 *
 *	estimate - in, use
 *		estimate object
 *
 *
 * Output params:
 *
 * Return value :
 *      number of matching order bys
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_est_get_n_order_bys(cd, estimate)
        rs_sysi_t* cd;
        tb_est_t* estimate;
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        return(estimate->e_n_order_bys);
}

/*##**********************************************************************\
 *
 *		tb_est_get_full_scan
 *
 * Gets the info if the table is accessed using a full table scan.
 *
 * Parameters :
 *
 *	estimate - in, use
 *		estimate object
 *
 *
 * Output params:
 *
 * Return value :
 *      number of matching order bys
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_est_get_full_scan(cd, estimate)
        rs_sysi_t* cd;
        tb_est_t* estimate;
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        return(estimate->e_full_scan);
}

/*##**********************************************************************\
 *
 *		tb_est_get_must_retrieve
 *
 * Gets the info if data must be retrieved from the clustering key.
 *
 * Parameters :
 *
 *	estimate - in, use
 *		estimate object
 *
 *
 * Output params:
 *
 * Return value :
 *      number of matching order bys
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_est_get_must_retrieve(cd, estimate)
        rs_sysi_t* cd;
        tb_est_t* estimate;
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        return(estimate->e_must_retrieve);
}

/*##**********************************************************************\
 *
 *		tb_est_get_unique_value
 *
 * Are all rows unique.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	estimate -
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
bool tb_est_get_unique_value(
        rs_sysi_t* cd,
        tb_est_t* estimate)
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        return(estimate->e_unique_value);
}

bool tb_est_get_single_row(
        rs_sysi_t* cd,
        tb_est_t* estimate)
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        return(estimate->e_single_row);
}

/*#***********************************************************************\
 *
 *		diffrowcount_qsortcmp
 *
 * Compare function to sort row counts for different columns before
 * calculating total combined rowcount.
 *
 * Parameters :
 *
 *	s1 -
 *
 *
 *	s2 -
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
static int SS_CLIBCALLBACK diffrowcount_qsortcmp(const void* s1, const void* s2)
{
        long d1 = *(long*)s1;
        long d2 = *(long*)s2;

        /* Sort in reverse order. */
        if (d1 < d2) {
            return (1);
        } else if (d1 > d2) {
            return (-1);
        } else {
            return (0);
        }
}

/*##**********************************************************************\
 *
 *		tb_est_getdiffrowcount
 *
 * Return estimate of how many different rows are returned for a given
 * set of group by columns.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	estimate -
 *
 *
 *	relh -
 *
 *
 *	n -
 *
 *
 *	sql_cols -
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
rs_estcost_t tb_est_getdiffrowcount(
        rs_sysi_t*   cd,
        tb_est_t*    estimate,
        rs_relh_t*   relh,
        uint         n,
        uint*        sql_cols)
{
        rs_estcost_t rowcount;
        rs_ttype_t* ttype;
        uint i;
        long* rowcounts;
        long est_nrows;

        ss_dassert(estimate);
        ss_dassert(n > 0);
        CHK_EST(estimate);

        ss_trigger("tb_est");
        ss_dprintf_1(("tb_est_getdiffrowcount\n"));

        tb_est_ensureselectivityinfo(cd, relh);

        rowcounts = SsMemAlloc(n * sizeof(rowcounts[0]));
        ttype = rs_relh_ttype(cd, relh);

        est_nrows = estimate->e_n_rows;
        ss_dprintf_2(("tb_est_getdiffrowcount:est_nrows=%.1lf\n", (double)est_nrows));

        for (i = 0; i < n; i++) {
            rs_ano_t ano;
            long ano_rowcount;
            ano = rs_ttype_sqlanotophys(cd, ttype, sql_cols[i]);
            if (estimate->e_rowcounts[ano] == 0) {
                ano_rowcount = est_nrows;
                ss_dprintf_2(("tb_est_getdiffrowcount:ano=%d, no estimate, ano_rowcount=%.1lf\n", ano, (double)ano_rowcount));
            } else {
                ano_rowcount = (uint)estimate->e_rowcounts[ano];
                ss_dprintf_2(("tb_est_getdiffrowcount:ano=%d, ano_rowcount=%.1lf\n", ano, (double)ano_rowcount));
            }
            rowcounts[i] = rs_relh_getdiffrowcount(cd, relh, ano, ano_rowcount);
        }

        if (n == 1) {
            rowcount = (rs_estcost_t)SS_MIN(est_nrows, rowcounts[0]);

        } else {
            double rowcount_inc_limit;
            double total_rowcount;
            double new_total_rowcount;

            rowcount_inc_limit = rs_sqli_selectivity_drop_limit(rs_sysi_sqlinfo(cd));
            qsort(
                rowcounts,
                n,
                sizeof(rowcounts[0]),
                diffrowcount_qsortcmp);
            total_rowcount = (double)rowcounts[0];
            for (i = 1; i < n; i++) {
                new_total_rowcount = total_rowcount * (double)rowcounts[i] *
                                     rowcount_inc_limit;
                if (new_total_rowcount > total_rowcount) {
                    total_rowcount = new_total_rowcount;
                }
            }
            if (total_rowcount > (double)est_nrows) {
                rowcount = (rs_estcost_t)est_nrows;
            } else {
                rowcount = (rs_estcost_t)total_rowcount;
            }
        }

        if (rowcount < 1) {
            rowcount = 1;
        }

        SsMemFree(rowcounts);

        ss_dprintf_2(("tb_est_getdiffrowcount:rowcount=%ld\n", rowcount));

        ss_trigger("tb_est");

        return(rowcount);
}

/*##**********************************************************************\
 *
 *		tb_est_free_estimate
 *
 * Frees the estimate object. WARNING!!! In the test version it is not freed.
 *
 * Parameters :
 *
 *	estimate - in, take
 *		estimate object
 *
 *
 * Output params:
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_est_free_estimate(cd, estimate)
        rs_sysi_t* cd;
        tb_est_t* estimate;
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        if (!est_test_version_on && !est_selectivity_test_on) {
            su_list_t* consbyano_list;
            uint k;

            su_pa_do_get(estimate->e_cons_byano, k, consbyano_list) {
                su_list_done(consbyano_list);
            }
            su_pa_done(estimate->e_cons_byano);
            SsMemFree(estimate->e_rowcounts);
            SsMemFree(estimate);
        }
}

#ifdef SS_DEBUG
bool tb_est_check(tb_est_t* estimate)
{
        CHK_EST(estimate);
        return(TRUE);
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		tb_est_sortestimate
 *
 * Estimates time for sorting.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	ttype -
 *
 *
 *	lines -
 *
 *
 *  external - in
 *      1 if external sort, 0 if internal sort
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_estcost_t tb_est_sortestimate(
        rs_sysi_t*   cd,
        rs_ttype_t*  ttype __attribute__ ((unused)),
        rs_estcost_t lines,
        bool         external)
{
        rs_estcost_t sort_estimate;
        rs_sqlinfo_t* sqli;
        rs_estcost_t row_sort_time;

        ss_trigger("tb_est");

        sqli = rs_sysi_sqlinfo(cd);

        row_sort_time = rs_sqli_row_sort_time(sqli);

        ss_bassert(lines >= (rs_estcost_t)0);

        if (lines < (rs_estcost_t)0) {
            sort_estimate = 0;

        } else {
            sort_estimate = (rs_estcost_t)ceil(row_sort_time * lines) / 1000;

            if (external) {
                /* Calculate also block access times. We use fixed data size
                 * and database block size for now.
                 */
                rs_sqlinfo_t* sqli;
                long blocksize;
                double key_entry_size;
                double block_access_time;
                double data_size;
                double nblocks;

                sqli = rs_sysi_sqlinfo(cd);
                blocksize = db_info_blocksize(cd);
                /* For block access time we use random access time to compensate more than
                 * one I/O request needed during sorting.
                 */
                block_access_time = rs_sqli_block_access_time(sqli);
                key_entry_size = rs_sqli_max_key_entry_size(sqli);

                data_size = key_entry_size * lines;
                nblocks = data_size / blocksize;
                sort_estimate += (nblocks * block_access_time);
            }
        }

        ss_dprintf_1(("tb_est_sortestimate:external=%d, lines=%ld, sort_estimate=%.2lf\n", external, lines, (double)sort_estimate));

        ss_trigger("tb_est");

        return(sort_estimate);
}
