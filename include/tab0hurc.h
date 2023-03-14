/*************************************************************************\
**  source       * tab0hurc.h
**  directory    * tab
**  description  * Synchronizer historical union relcur
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


#ifndef TAB0HURC_H
#define TAB0HURC_H

#ifdef SS_MYSQL
#include <rs0types.h>
#else
#include <sqlint.h>
#endif

typedef struct tb_hurc_st tb_hurc_t;

#define TB_CUR_CAST_HURC(cur)   ((tb_hurc_t*)cur)

#ifndef SS_MYSQL

tb_hurc_t* tb_hurc_create(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrel,
        forupdate_t forupdate,
        bool        insubquery,
        bool        sqlcall);

void tb_hurc_free(
        void*       cd,
        tb_hurc_t*  cur);

rs_ttype_t* tb_hurc_ttype(
        void*       cd,
        tb_hurc_t*  cur);

void tb_hurc_constr(
        void*       cd,
        tb_hurc_t*  cur,
        uint        attr_n,
        uint        relop,
        rs_atype_t* atype,
        rs_aval_t*  avalue,
        rs_atype_t* esctype,
        rs_aval_t*  escinst);

bool tb_hurc_vectorconstr(
	void*           cd,
	tb_hurc_t*      cur,
        uint            n,
        uint*           col_ns,
        uint            relop,
	rs_atype_t**    atypes,
	rs_aval_t**     avals
);

void tb_hurc_orderby(
        void*       cd,
        tb_hurc_t*  cur,
        uint        attr_n,
        bool        asc);

void tb_hurc_project(
        void*       cd,
        tb_hurc_t*  cur,
        int*        selattrs);

void tb_hurc_setoptcount(
        void*        cd,
        tb_hurc_t*   cur,
        rs_estcost_t count);

void tb_hurc_indexhint(
        void*        cd,
        tb_hurc_t*   hurc,
        bool         fullscan,
        char*        index,
        bool         reverse);

uint tb_hurc_estcount(
        void*         cd,
        tb_hurc_t*    cur,
        rs_estcost_t* p_count);

uint tb_hurc_estcolset(
	void*         cd,
	tb_hurc_t*    cur,
        uint          n,
        uint*         cols,
        rs_estcost_t* p_count);

bool tb_hurc_estdelay(
        void*         cd,
        tb_hurc_t*    cur,
        rs_estcost_t* p_c0,
        rs_estcost_t* p_c1);

bool tb_hurc_endofconstr(
        void*       cd,
        tb_hurc_t*  cur,
        rs_err_t**  errhandle);

void tb_hurc_reset(
        void*       cd,
        tb_hurc_t*  hurc,
        bool        resetconstr);

uint tb_hurc_ordered(
        void*       cd,
        tb_hurc_t*  cur,
        uint*       p_nullcoll);

bool tb_hurc_tabcurunique(
        void*       cd,
        tb_hurc_t*  cur);

bool tb_hurc_open(
        void*       cd,
        tb_hurc_t*  cur,
        rs_err_t**  p_errh);
       
rs_tval_t* tb_hurc_next(
        void*       cd,
        tb_hurc_t*  cur,
	uint*       p_finished,
        rs_err_t**  p_errh);

bool tb_hurc_next_sql(
        void*       cd,
        tb_hurc_t*  cur,
        rs_tval_t** p_result,
        void**      cont,
        rs_err_t**  p_errh);

rs_tval_t* tb_hurc_prev(
        void*       cd,
        tb_hurc_t*  cur,
	uint*       p_finished,
        rs_err_t**  p_errh);

uint tb_hurc_prev_sql(
        void*       cd,
        tb_hurc_t*  cur,
        rs_tval_t** p_result,
        void**      cont,
        rs_err_t**  p_errh);

rs_tval_t* tb_hurc_current(
	void*       cd,
	tb_hurc_t*  cur);

bool tb_hurc_aggr(
        void*       cd,
        tb_hurc_t*  cur,
        uint        groupby_c,
        uint*       groupbyarr,
        uint        aggr_c,
        char**      aggrfunarr,
        int*        aggrargarr,
        bool*       distarr);

rs_aval_t* tb_hurc_aval(
        void*       cd,
        tb_hurc_t*  cur,
        uint        ano);

bool tb_hurc_begin(
        void*       cd,
        tb_hurc_t*  cur);

bool tb_hurc_begin_sql(
        void*       cd,
        tb_hurc_t*  cur,
        void**      cont,
        rs_err_t**  p_errh);

bool tb_hurc_end(
        void*       cd,
        tb_hurc_t*  cur);
        
bool tb_hurc_end_sql(
        void*       cd,
        tb_hurc_t*  cur,
        void**      cont,
        rs_err_t**  p_errh);

uint tb_hurc_update(
        void*        cd,
        tb_hurc_t*   cur,
        rs_tval_t*   tvalue,
        bool*        selflags,
        bool*        incrflags,
        uint         constr_n,
        uint*        constrattrs,
        uint*        constrrelops,
        rs_atype_t** constratypes,
        rs_aval_t**  constravalues,
        rs_err_t**   errhandle);

uint tb_hurc_update_sql(
        void*        cd,
        tb_hurc_t*   cur,
        rs_tval_t*   tvalue,
        bool*        selflags,
        bool*        incrflags,
        uint         constr_n,
        uint*        constrattrs,
        uint*        constrrelops,
        rs_atype_t** constratypes,
        rs_aval_t**  constravalues,
        void**       cont,
        rs_err_t**   errhandle);

uint tb_hurc_delete(
        void*       cd,
        tb_hurc_t*  cur,
        rs_err_t**  errhandle);

bool tb_hurc_delete_sql(
        void*       cd,
        tb_hurc_t*  cur,
        void**      cont,
        rs_err_t**  errhandle);

char* tb_hurc_info(
        void*       cd,
        tb_hurc_t*  cur);

sql_reverse_t tb_hurc_canreverse(
	void*      cd,
	tb_relh_t* tbrelh);

#else /* !SS_MYSQL */

#define tb_hurc_create(cd,trans,tbrel,forupdate,insubquery,sqlcall) NULL
#define tb_hurc_free(cd,cur) 
#define tb_hurc_ttype(cd,cur)  NULL
#define tb_hurc_constr(cd,cur,attr_n,relop,atype,avalue,esctype,escinst) 
#define tb_hurc_vectorconstr(cd,cur,n,col_ns,relop,atypes,avals)  0
#define tb_hurc_orderby(cd,cur,attr_n,asc) 
#define tb_hurc_project(cd,cur,selattrs) 
#define tb_hurc_setoptcount(cd,cur,count) 
#define tb_hurc_indexhint(cd,hurc,fullscan,index,reverse) 
#define tb_hurc_estcount(cd,cur,p_count)  0
#define tb_hurc_estcolset(cd,cur,n,cols,p_count)  0
#define tb_hurc_estdelay(cd,cur,p_c0,p_c1)  0
#define tb_hurc_endofconstr(cd,cur,errhandle) 0 
#define tb_hurc_reset(cd,hurc,resetconstr) 
#define tb_hurc_ordered(cd,cur,p_nullcoll) 0
#define tb_hurc_tabcurunique(cd,cur) 0
#define tb_hurc_open(cd,cur,p_errh) 0
#define tb_hurc_next(cd,cur,p_finished,p_errh) NULL
#define tb_hurc_next_sql(cd,cur,p_result,cont,p_errh)   0
#define tb_hurc_prev(cd,cur,p_finished,p_errh) NULL
#define tb_hurc_prev_sql(cd,cur,p_result,cont,p_errh) 0
#define tb_hurc_current(cd,cur) NULL
#define tb_hurc_aggr(cd,cur,groupby_c,groupbyarr,aggr_c,aggrfunarr,aggrargarr,distarr) 0
#define tb_hurc_aval(cd,cur,ano) NULL
#define tb_hurc_begin(cd,cur) 0
#define tb_hurc_begin_sql(cd,cur,cont,p_errh) 0
#define tb_hurc_end(cd,cur) 0
#define tb_hurc_end_sql(cd,cur,cont,p_errh) 0
#define tb_hurc_update(cd,cur,tvalue,selflags,incrflags,constr_n,constrattrs,constrrelops,constratypes,constravalues,errhandle) 0
#define tb_hurc_update_sql(cd,cur,tvalue,selflags,incrflags,constr_n,constrattrs,constrrelops,constratypes,constravalues,cont,errhandle) 0
#define tb_hurc_delete(cd,cur,errhandle) 0
#define tb_hurc_delete_sql(cd,cur,cont,errhandle) 0
#define tb_hurc_info(cd,cur) NULL
#define tb_hurc_canreverse(cd,tbrelh) 0

#endif /* SS_MYSQL */

#endif /* TAB0HURC_H */
