/*************************************************************************\
**  source       * tab0relc.h
**  directory    * tab
**  description  * Relation cursor functions
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


#ifndef TAB0RELC_H
#define TAB0RELC_H

/* #define TB_RELCUR_CACHE */

#include <su0list.h>
#include <rs0types.h>

#define TB_FETCH_CONT           0
#define TB_FETCH_SUCC           1
#define TB_FETCH_ERROR          2

#define TB_CHANGE_CONT          3
#define TB_CHANGE_SUCC          1
#define TB_CHANGE_ERROR         0
#define TB_CHANGE_CONSTRFAIL    2

#define TB_PREV_ERROR           0
#define TB_PREV_SUCC            1
#define TB_PREV_NOTALLOWED      2


typedef enum {
    FORUPDATE_NO = 0,       
    FORUPDATE_UPDATE = 1,                      /* SELECT ... FOR UPDATE */
    FORUPDATE_SEARCHED_UPDATE = 2,             /* searched UPDATE */
    FORUPDATE_SEARCHED_DELETE = 3,             /* searched DELETE */
    FORUPDATE_SOURCE_SEARCHED_INSERT = 4,      /* source for searched INSERT */
    FORUPDATE_DESTINATION_SEARCHED_INSERT = 5, /* destination for searched INSERT */
    FORUPDATE_VALUELIST_INSERT = 6,            /* destination for INSERT from value list */
    FORUPDATE_FAKE = 1000                      /* used as fake propery */
} forupdate_t;

tb_relcur_t* tb_relcur_create(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrel,
        forupdate_t forupdate,
        bool        insubquery
);

tb_relcur_t* tb_relcur_create_sql(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrel,
        forupdate_t forupdate,
        bool        insubquery
);

tb_relcur_t* tb_relcur_create_nohurc(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrel,
        forupdate_t forupdate,
        bool        insubquery,
        bool        sqlcall
);

tb_relcur_t* tb_relcur_create_nohurc_ex(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        rs_relh_t*  rsrelh,
        forupdate_t forupdate,
        bool        insubquery,
        bool        sqlcall
);

void tb_relcur_free(
        void*        cd,
        tb_relcur_t* cur
);

void tb_relcur_reset(
	void*        cd,
	tb_relcur_t* cur,
        bool         resetconstr
);

void tb_relcur_setisolation_transparent(
        void*        cd,
        tb_relcur_t* cur,
        bool transparent);

void tb_relcur_disableinfo(
	void*        cd,
	tb_relcur_t* cur
);

void tb_relcur_project(
        void*        cd,
        tb_relcur_t* cur,
        int*         selattrs
);

void tb_relcur_constr(
        void*        cd,
        tb_relcur_t* cur,
        uint         attr_n,
        uint         relop,
        rs_atype_t*  atype,
        rs_aval_t*   avalue,
        rs_atype_t*  esctype,
        rs_aval_t*   escinst
);

void tb_relcur_tabconstr(
        void*        cd,
        tb_relcur_t* cur,
        uint         attr_n,
        uint         relop,
        rs_atype_t*  atype,
        rs_aval_t*   avalue,
        rs_atype_t*  esctype,
        rs_aval_t*   escinst
);

bool tb_relcur_vectorconstr(
	void*           cd,
	tb_relcur_t*    cur,
        uint            n,
        uint*           col_ns,
        uint            relop,
	rs_atype_t**    atypes,
	rs_aval_t**     avals
);

void tb_relcur_orderby(
        void*        cd,
        tb_relcur_t* cur,
        uint         attr_n,
        bool         asc
);

void tb_relcur_setoptcount(
        void*        cd,
        tb_relcur_t* cur,
        rs_estcost_t count
);

void tb_relcur_indexhint(
        void*        cd,
        tb_relcur_t* cur,
        bool         fullscan,
        char*        index,
        bool         reverse
);

char* tb_relcur_info(
        void*        cd,
        tb_relcur_t* cur
);

void tb_relcur_forcegroup(
    void*        cd,
    tb_relcur_t* cur);

bool tb_relcur_endofconstr(
        void*        cd,
        tb_relcur_t* cur,
        rs_err_t**   errhandle
);

uint tb_relcur_estcount(
        void*         cd,
        tb_relcur_t*  cur,
        rs_estcost_t* p_count
);

bool tb_relcur_estdelay(
        void*         cd,
        tb_relcur_t*  cur,
        rs_estcost_t* p_c0,
        rs_estcost_t* p_c1
);

uint tb_relcur_estcolset(
	void*         cd,
	tb_relcur_t*  cur,
        uint          n,
        uint*         cols,
        rs_estcost_t* p_count
);

uint tb_relcur_ordered(
        void*        cd,
        tb_relcur_t* cur,
        uint*        p_nullcoll
);

bool tb_relcur_tabcurunique(
        void*        cd,
        tb_relcur_t* cur
);

bool tb_relcur_open(
        void*        cd,
        tb_relcur_t* cur,
        rs_err_t**   p_errh
);
       
void tb_relcur_tabopen(
        void*        cd,
        tb_relcur_t* cur
);

rs_tval_t* tb_relcur_next(
        void*        cd,
        tb_relcur_t* cur,
	uint*        p_finished,
        rs_err_t**   p_errh
);

bool tb_relcur_next_sql(
        void*        cd,
        tb_relcur_t* cur,
        rs_tval_t**  p_result,
        void**       cont,
        rs_err_t**   p_errh );

rs_tval_t* tb_relcur_prev(
        void*        cd,
        tb_relcur_t* cur,
	uint*        p_finished,
        rs_err_t**   p_errh
);

uint tb_relcur_prev_sql(
        void*        cd,
        tb_relcur_t* cur,
        rs_tval_t**  p_result,
        void**       cont,
        rs_err_t**   p_errh );

rs_tval_t* tb_relcur_current(
	void*        cd,
	tb_relcur_t* cur
);

bool tb_relcur_aggr(
        void*        cd,
        tb_relcur_t* cur,
        uint         groupby_c,
        uint*        groupbyarr,
        uint         aggr_c,
        char**       aggrfunarr,
        int*         aggrargarr,
        bool*        distarr
);

rs_aval_t* tb_relcur_aval(
        void*        cd,
        tb_relcur_t* cur,
        uint         ano
);

bool tb_relcur_begin(
        void*        cd,
        tb_relcur_t* cur
);

bool tb_relcur_begin_sql(
        void*        cd,
        tb_relcur_t* cur,
        void**       cont,
        rs_err_t**   errhandle
);

bool tb_relcur_end(
        void*        cd,
        tb_relcur_t* cur
);

bool tb_relcur_end_sql(
        void*        cd,
        tb_relcur_t* cur,
        void**       cont,
        rs_err_t**   errhandle
);

bool tb_relcur_setposition(
        void*           cd,
        tb_relcur_t*    cur,
        rs_tval_t*      tval,
        rs_err_t**      p_errh);

rs_ttype_t* tb_relcur_ttype(
        void*        cd,
        tb_relcur_t* cur
);

uint tb_relcur_update(
        void*        cd,
        tb_relcur_t* cur,
        rs_tval_t*   tvalue,
        bool*        selflags,
        bool*        incrflags,
        uint         constr_n,
        uint*        constrattrs,
        uint*        constrrelops,
        rs_atype_t** constratypes,
        rs_aval_t**  constravalues,
        rs_err_t**   errhandle
);

uint tb_relcur_update_sql(
        void*        cd,
        tb_relcur_t* cur,
        rs_tval_t*   tvalue,
        bool*        selflags,
        bool*        incrflags,
        uint         constr_n,
        uint*        constrattrs,
        uint*        constrrelops,
        rs_atype_t** constratypes,
        rs_aval_t**  constravalues,
        void**       cont,
        rs_err_t**   errhandle
);

uint tb_relcur_delete(
        void*        cd,
        tb_relcur_t* cur,
        rs_err_t**   errhandle
);

bool tb_relcur_delete_sql(
        void*        cd,
        tb_relcur_t* cur,
        void**       cont,
        rs_err_t**   errhandle
);

uint tb_relcur_saupdate(
        void*        cd,
        tb_relcur_t* cur,
        tb_trans_t*  trans,
        rs_tval_t*   tvalue,
        bool*        selflags,
        void*        tref,
        bool         truncate,
        rs_err_t**   errhandle
);

uint tb_relcur_sadelete(
        void*        cd,
        tb_relcur_t* cur,
        tb_trans_t*  trans,
        void*        tref,
        rs_err_t**   errhandle
);

tb_trans_t* tb_relcur_trans(
        void*        cd,
        tb_relcur_t* cur);

void* tb_relcur_copytref(
        void*        cd,
        tb_relcur_t* cur);

void* tb_relcur_gettref(
        void*        cd,
        tb_relcur_t* cur);

#ifdef SS_SYNC
void tb_relcur_setsynchistorydeleted(
        void*        cd,
        tb_relcur_t* cur,
        bool         yes_or_no);
#endif

#ifdef SS_QUICKSEARCH
void* tb_relcur_getquicksearch(
        void*        cd,
        tb_relcur_t* cur,
        bool        longsearch);
#endif /* SS_QUICKSEARCH */

#ifdef SS_BETA

typedef struct {
        long    ep_curnrows;
        long    ep_estnrows;
        long    ep_tablenrows;
} tb_relc_estinfo_t;

/* Protected by rs_sysi_rslinksem */
extern su_list_t* tb_relc_estinfolist;

void tb_relc_estinfolist_init(void);
void tb_relc_estinfolist_done(void);
void tb_relc_estinfolist_setoutfp(
        void (*outfp)(
            char* tablename,
            tb_relc_estinfo_t* estinfo));

#endif /* SS_BETA */

#endif /* TAB0RELC_H */
