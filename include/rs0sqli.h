/*************************************************************************\
**  source       * rs0sqli.h
**  directory    * res
**  description  * SQL info structure.
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


#ifndef RS0SQLI_H
#define RS0SQLI_H

#include <ssc.h>
#include <ssmsglog.h>

#include <su0inifi.h>
#include <su0cfgl.h>

#include "rs0types.h"

#define RS_SQLI_MAXINFOLEVEL 15

typedef enum {
    RS_SQLI_SORTEDGROUPBY_NONE = 0,
    RS_SQLI_SORTEDGROUPBY_STATIC = 1,
    RS_SQLI_SORTEDGROUPBY_ADAPT = 2
} sqli_sortedgroupby_t;

typedef enum {
        RS_SQLI_ISOLATION_READCOMMITTED = 1,
        RS_SQLI_ISOLATION_REPEATABLEREAD,
        RS_SQLI_ISOLATION_SERIALIZABLE
} rs_sqli_isolation_t;


typedef struct rs_sqlinfo_st rs_sqlinfo_t;

#define CHK_SQLI(sqli)  ss_dassert(SS_CHKPTR(sqli) && (sqli)->sqli_chk == RSCHK_SQLI)

struct rs_sqlinfo_st {
        ss_debug(int    sqli_chk;)
        su_inifile_t*   sqli_inifile;
        uint            sqli_infolevel;     /* Info level, zero if no info. */
        uint            sqli_sqlinfolevel;  /* SQL specific info level, zero if no info. */
        char*           sqli_fname;         /* If non-NULL, default global
                                               info file name. */
        bool            sqli_mustflush;     /* If TRUE, flush SQL info file
                                               after every write. */
        long            sqli_maxfilesize;   /* Maximum ionfo file size. */
        bool            sqli_warning;       /* If TRUE, warnings are displayed. */
        uint            sqli_sortarraysize; /* Sort array size for query. */
        uint            sqli_convertorstounions; /* If nonzero, OR conditions
                                                    are converted to unions
                                                    in SQL. */
        uint            sqli_allowduplicateindex; /* If nonzero, create index allows duplicate index. */
        uint            sqli_sortedgroupby;
        bool            sqli_estignoreorderby;
        uint            sqli_statementcache;/* Per client statement cache size
                                               of precompiled statements. */
        uint            sqli_procedurecache;/* Per client procedure cache size
                                               of precompiled procedure. */
        uint            sqli_triggercache;  /* Per client trigger cache size
                                               of precompiled trigger. */
        uint            sqli_maxnestedtrig; /* Max number of nested triggers. */
        uint            sqli_maxnestedproc; /* Max number of nested procedures. */
        long            sqli_estsamplelimit;/* Estimator sample limit. */
        bool            sqli_simpleoptimizerrules;
        bool            sqli_userelaxedreacommitted;
        uint            sqli_estsamplecount;/* Count of samples per attribute. */
        uint            sqli_estsamplemin;  /* Minimum samples per attribute. */
        uint            sqli_estsamplemax;  /* Maximum samples per attribute. */
        uint            sqli_estsamplemaxeqrowest; /* Maximum rows checked for equal constraint. */
        ulong           sqli_estsampleinc;  /* Rows in table that increment samplecount by one. */
        uint            sqli_optn;          /* Optimize for optn rows. */
        uint            sqli_vectoroptn;    /* Optimize vector constraints for n rows. */
        bool            sqli_simplesqlopt;  /* Optimized simple sql statements. */
        bool            sqli_hurccanreverse;/* Is reverse supported in HURC (tab0hurc.h). */
        bool            sqli_indexcursorreset;/* Should we use index cursor reset. */
        bool            sqli_lateindexplan;    /* Delayedsearch plan. */
        bool            sqli_settranscompatibility3;/* Compatible with old 3.x or older SET behaviour. */
        bool            sqli_charpadding; /* SQL CHAR padding to max len? */
        bool            sqli_cursorcloseatendtran; /* cursor close at commit/rollback. */
        rs_sqli_isolation_t sqli_isolationlevel;/* Default trans isolation level. */
        SsMsgLogT*      sqli_infoml;
        char*           sqli_ver;

        bool            sqli_userangeest;

        /* The estimated selectivity for different types of search constraints */
        double          sqli_equal_selectivity;
        double          sqli_notequal_selectivity;
        double          sqli_compare_selectivity; /* selectivity for <, <= etc. */
        double          sqli_like_selectivity;
        double          sqli_isnull_selectivity;
        double          sqli_isnotnull_selectivity;
        double          sqli_no_selectivity;
        double          sqli_selectivity_drop_limit;

        /* estimated default hit rate for an index block
         */
        double          sqli_min_hit_rate_for_index;
        double          sqli_max_hit_rate_for_index;

        /* estimated default hit rate for a data block
         */
        double          sqli_min_hit_rate_for_data;
        double          sqli_max_hit_rate_for_data;

        /* estimated minimum data size per a single index size
         * test version only
         */
        double          sqli_data_size_per_index;

        /* estimated maximum key entry size in bytes
         */
        double          sqli_max_key_entry_size;

        /* if a secondary index is prejoined, then the following parameter
         * tells how densely are the relevant entries there
         */
        double          sqli_prejoin_density;

        /* time for a B+-tree search in microseconds
         */
        double          sqli_time_for_index_search;

        /* time for processing a single index entry in a block in microseconds
         */
        double          sqli_time_per_index_entry;

        /* time for random access of a disk block in microseconds
         */
        double          sqli_block_access_time;

        /* time for sorting one row
         */
        double          sqli_row_sort_time;
};


rs_sqlinfo_t* rs_sqli_init(
        su_inifile_t* inifile);

void rs_sqli_done(
        rs_sqlinfo_t* sqli);

void rs_sqli_setsqlversion(
        rs_sqlinfo_t* sqli,
        char* ver);

uint rs_sqli_getinfolevel(
        rs_sqlinfo_t* sqli,
        bool sqlp);

void rs_sqli_setinfolevel(
        rs_sqlinfo_t* sqli,
        uint infolevel,
        bool sqlp);

bool rs_sqli_getwarning(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getsortarraysize(
        rs_sqlinfo_t* sqli);

bool rs_sqli_useinternalsorter(
        rs_sqlinfo_t* sqli,
        uint sortarraysize,
        ulong lines,
        bool exact);

bool rs_sqli_usesimpleoptimizerrules(
        rs_sqlinfo_t* sqli,
        double ntuples_in_table);

bool rs_sqli_userelaxedreacommitted(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getestsamplecount(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getestsamplemin(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getestsamplemax(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getestsamplemaxeqrowest(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getestsampleinc(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getconvertorstounions(
        rs_sqlinfo_t* sqli);

bool rs_sqli_getallowduplicateindex(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getsortedgroupby(
        rs_sqlinfo_t* sqli);

bool rs_sqli_estignoreorderby(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getstatementcachesize(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getprocedurecachesize(
        rs_sqlinfo_t* sqli);

uint rs_sqli_gettriggercachesize(
        rs_sqlinfo_t* sqli);

bool rs_sqli_getcharpadding(
        rs_sqlinfo_t* sqli);

bool rs_sqli_cursorcloseatendtran(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getmaxnestedtrig(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getmaxnestedproc(
        rs_sqlinfo_t* sqli);

SS_INLINE uint rs_sqli_getoptn(
        rs_sqlinfo_t* sqli);

uint rs_sqli_getvectoroptn(
        rs_sqlinfo_t* sqli);

bool rs_sqli_simplesqlopt(
        rs_sqlinfo_t* sqli);

/* used in some test */
void rs_sqli_setsimplesqlopt(rs_sqlinfo_t* sqli, bool opt);

bool rs_sqli_hurccanreverse(
        rs_sqlinfo_t* sqli);

bool rs_sqli_getuseindexcursorreset(
        rs_sqlinfo_t* sqli);

bool rs_sqli_getuselateindexplan(
        rs_sqlinfo_t* sqli);

bool rs_sqli_getsettranscompatibility3(
        rs_sqlinfo_t* sqli);
        
SS_INLINE rs_sqli_isolation_t rs_sqli_getisolationlevel(
        rs_sqlinfo_t* sqli);
        
void rs_sqli_settestmode(
        rs_sqlinfo_t* sqli);

bool rs_sqli_userangeest(
        rs_sqlinfo_t* sqli);

double rs_sqli_equal_selectivity(
        rs_sqlinfo_t* sqli);

double rs_sqli_notequal_selectivity(
        rs_sqlinfo_t* sqli);

double rs_sqli_compare_selectivity(
        rs_sqlinfo_t* sqli);

double rs_sqli_like_selectivity(
        rs_sqlinfo_t* sqli);

double rs_sqli_isnull_selectivity(
        rs_sqlinfo_t* sqli);

double rs_sqli_isnotnull_selectivity(
        rs_sqlinfo_t* sqli);

double rs_sqli_no_selectivity(
        rs_sqlinfo_t* sqli);

double rs_sqli_selectivity_drop_limit(
        rs_sqlinfo_t* sqli);

double rs_sqli_min_hit_rate_for_index(
        rs_sqlinfo_t* sqli);

double rs_sqli_max_hit_rate_for_index(
        rs_sqlinfo_t* sqli);

double rs_sqli_min_hit_rate_for_data(
        rs_sqlinfo_t* sqli);

double rs_sqli_max_hit_rate_for_data(
        rs_sqlinfo_t* sqli);

double rs_sqli_data_size_per_index(
        rs_sqlinfo_t* sqli);

double rs_sqli_max_key_entry_size(
        rs_sqlinfo_t* sqli);

double rs_sqli_prejoin_density(
        rs_sqlinfo_t* sqli);

double rs_sqli_time_for_index_search(
        rs_sqlinfo_t* sqli);

double rs_sqli_time_per_index_entry(
        rs_sqlinfo_t* sqli);

double rs_sqli_block_access_time(
        rs_sqlinfo_t* sqli);

double rs_sqli_row_sort_time(
        rs_sqlinfo_t* sqli);

void* rs_sqli_openinfofile(
        rs_sqlinfo_t* sqli,
        char* fname);

void rs_sqli_closeinfofile(
        rs_sqlinfo_t* sqli,
        void* fp);

void rs_sqli_printinfo(
        rs_sqlinfo_t* sqli,
        void* fp,
        int level,
        char* str);

void rs_sqli_addcfgtocfgl(
        rs_sqlinfo_t* sqli,
        su_cfgl_t* cfgl);

#if defined(RS0SQLI_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		rs_sqli_getoptn
 *
 *
 * Parameters :
 *
 *	sqli -
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
SS_INLINE uint rs_sqli_getoptn(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_optn);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getisolationlevel
 *
 * Returns the default isolation level. Illegal isolation level gives
 * a warning and uses default isolation level.
 *
 * Parameters :
 *
 *	sqli -
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
SS_INLINE rs_sqli_isolation_t rs_sqli_getisolationlevel(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_isolationlevel);
}

#endif /* defined(RS0SQLI_C) || defined(SS_USE_INLINE) */

#endif /* RS0SQLI_H */
