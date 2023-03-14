/*************************************************************************\
**  source       * tab0relh.h
**  directory    * tab
**  description  * Relation level operations for rs_relh
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


#ifndef TAB0RELH_H
#define TAB0RELH_H

#include <uti0va.h>

#include <rs0types.h>
#include <rs0relh.h>
#include <rs0error.h>
#include <rs0sysi.h>

#include "tab0type.h"
#include "tab0tint.h"
#include "tab1priv.h"


bool tb_relh_syncinfo(
        void*       cd,
        rs_relh_t*  relh,
        bool *p_ismaster,
        bool *p_isreplica);

tb_relh_t* tb_relh_new (
        rs_sysi_t*    cd,
        rs_relh_t*    relh,
        tb_relpriv_t* priv);

tb_relh_t* tb_relh_create(
	void*       cd,
        tb_trans_t* trans,
	char*       relname,
	char*       authid,
        char*       catalog,
	rs_err_t**  p_errh
);

tb_relh_t* tb_relh_sql_create(
	void*       cd,
        tb_trans_t* trans,
	char*       relname,
	char*       authid,
        char*       catalog,
	char*       extrainfo,
        uint        throughview,
        char*       viewname,
        char*       viewauthid,
        char*       viewcatalog,
        tb_viewh_t* parentviewh,
	rs_err_t**  p_errh
);

tb_relh_t* tb_relh_create_synchist(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  base_tbrelh);

void tb_relh_free(
        void*      cd,
        tb_relh_t* tbrel
);

uint tb_relh_issame(
	void*      cd,
        tb_trans_t* trans,
	tb_relh_t* tbrelh,
        char*      tablename,
        char*      schema,
        char*      catalog,
        char*      extrainfo,
        uint       throughview,
        char*      viewname,
        char*      viewschema,
        char*      viewcatalog
);

uint tb_relh_insert(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrel,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        bool*       selflags,
        rs_err_t**  errhandle
);

bool tb_relh_insert_sql(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrel,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        bool*       selflags,
        void**      cont,
        rs_err_t**  errhandle
);

uint tb_relh_sainsert(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrel,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        bool*       selflags,
        bool        truncate,
        rs_err_t**  errhandle
);

tb_relh_t* tb_relh_createjoin(
        void*       cd,
        sqllist_t*  relnames,
        sqllist_t*  authids,
        sqllist_t*  extrainfo,
        sqllist_t*  catalogs,
        sqllist_t*  eqjoins,
        rs_err_t**  errhandle
);

SS_INLINE bool tb_relh_ispriv(
        rs_sysi_t* cd,
        tb_relh_t* tbrelh,
        tb_priv_t checkpriv);

SS_INLINE tb_relpriv_t* tb_relh_priv(
        void*      cd,
        tb_relh_t* tbrelh);

SS_INLINE rs_relh_t* tb_relh_rsrelh(
        void*      cd,
        tb_relh_t* tbrelh);

sql_reverse_t tb_relh_canreverse(
	void*      cd,
	tb_relh_t* tbrelh);

rs_ttype_t* tb_relh_ttype(
	void*      cd,
	tb_relh_t* tbrelh);

char** tb_relh_checkstrings(
	void*      cd,
	tb_relh_t* tbrelh,
    char***    pnamearray);

rs_entname_t* tb_relh_entname(
	void*      cd,
	tb_relh_t* tbrelh);

bool tb_synchist_nextver_to_tval(
	rs_sysi_t*  cd,
	tb_trans_t* trans,
	rs_ttype_t* ttype,
	rs_tval_t*  tvalue,
        va_t*       synctuplevers,
	rs_err_t**  p_errh);

bool tb_synchist_ispubltuple_to_tval(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        bool        ispubltuple,
        bool        isupdate,
        rs_err_t**  p_errh);

bool tb_synchist_is_tval_publtuple(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue);

/*
uint tb_relh_syncinsert(
	void*       cd,
	tb_trans_t* trans,
	rs_relh_t*  relh,
	rs_ttype_t* ttype,
	rs_tval_t*  tval,
	rs_tval_t*  vers_tval,
        bool        waspubltuple,
        bool        master_addsynchistory,
	rs_err_t**  p_errh);
*/

bool tb_relh_getsynchistrelinfo(
	void*       cd,
        tb_relh_t*  tbrelh,
	char**      p_name,
	char**      p_schema,
	char**      p_catalog);

typedef struct tb_syncinserthistory_st tb_syncinserthistory_t;

tb_syncinserthistory_t* tb_relh_syncinsert_init(
	void*       cd,
	tb_trans_t* trans,
	rs_relh_t*  relh,
	rs_ttype_t* ttype,
	rs_tval_t*  tval,
	rs_tval_t*  vers_tval,
        bool        waspubltuple,
        bool        isupdate,
        bool        master_addsynchistory,
        bool        ismaster,
        bool        isreplica,
        uint*       p_retcode,
	rs_err_t**  p_errh);

uint tb_relh_syncinsert_exec(
        tb_syncinserthistory_t* sih,
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_err_t** p_errh);

void tb_relh_syncinsert_done(
        tb_syncinserthistory_t* sih);

#ifndef SS_MYSQL
tb_tint_t* tb_relh_tabfunblock( 
        void* cd,
        tb_relh_t*  tbrelh);
#endif /* !SS_MYSQL */

#define CHK_TBRELH(rh)   ss_dassert(SS_CHKPTR(rh) && (rh)->rh_chk == TBCHK_RELHTYPE)

typedef enum {
        TBRELH_INIT,            /* Init state */
        TBRELH_SYNCHISTORY,     /* Update sync history state */
        TBRELH_BEFORETRIGGER,   /* Before insert trigger state */
        TBRELH_CHECK,           /* Check state */
        TBRELH_INSERT,          /* Insert state, insert not yet completed */
        TBRELH_STMTCOMMIT,      /* Statement commit state, if rh_isstmtgroup*/
        TBRELH_AFTERTRIGGER,    /* After insert trigger state */
        TBRELH_FREE             /* Free memory */
} tbrelh_state_t;

struct tbrelhandlestruct {
        rs_relh_t*          rh_relh;
        tb_relpriv_t*       rh_relpriv;
        tbrelh_state_t      rh_state;
        bool                rh_tvalue_copied;   /* Used during insert */
        rs_tval_t*          rh_ins_tval;        /* Used during insert */
        bool                rh_istrigger;       /* Used during insert */
        char*               rh_trigstr;         /* Used during insert */
        rs_entname_t*       rh_trigname;        /* Used during insert */
        void*               rh_trigctx;         /* Used during insert */
        bool                rh_isstmtgroup;     /* Used during insert */
#ifdef SS_SYNC
        dynva_t             rh_synctuplevers;   /* Used during insert */
#endif /* SS_SYNC */
        tb_tint_t*          rh_tabfunb;
        ss_debug(tb_check_t rh_chk;)
};

struct tb_syncinserthistory_st {
        rs_sysi_t*  sih_cd;
        tb_trans_t* sih_trans;
        rs_relh_t*  sih_relh;
        rs_ttype_t* sih_ins_ttype;
        rs_tval_t*  sih_ins_tval;
};

#if defined(TAB0RELH_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *              tb_relh_ispriv
 * 
 * Checks if user of relation handle has prileges given in checkpriv.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      tbrelh - 
 *              
 *              
 *      checkpriv - 
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
SS_INLINE bool tb_relh_ispriv(
        rs_sysi_t* cd,
        tb_relh_t* tbrelh,
        tb_priv_t  checkpriv)
{
        bool succp;
        bool sysid = FALSE;

        /* optimize this & calls to this (most can be done in prepare state)
         * Avoid doing for every row!
         */
        CHK_TBRELH(tbrelh);
        ss_debug(rs_relh_check(cd, tbrelh->rh_relh));

        if (rs_relh_issysrel(cd, tbrelh->rh_relh) || 
                !rs_relh_isbasetable(cd, tbrelh->rh_relh)) {
            sysid = TRUE;
        }

        /* allow user to select/insert/update/delete diskless system table,
         * however alter/drop table is not allow.  
         */
        if (sysid && 
                rs_relh_isdlsysrel(cd, tbrelh->rh_relh) && 
                (checkpriv & (TB_PRIV_SELECT|
                              TB_PRIV_INSERT|
                              TB_PRIV_DELETE|
                              TB_PRIV_UPDATE))) {
            sysid = FALSE;
        }
            
        succp = tb_priv_isrelpriv(
                    cd,
                    tbrelh->rh_relpriv,
                    checkpriv,
                    sysid);
        return(succp);
}

/*##**********************************************************************\
 * 
 *              tb_relh_priv
 * 
 * Returns privileges of the user of relation handle.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      tbrelh - 
 *              
 *              
 * Return value - ref : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE tb_relpriv_t* tb_relh_priv(
        void*      cd,
        tb_relh_t* tbrelh)
{
        CHK_TBRELH(tbrelh);
        SS_NOTUSED(cd);
        ss_debug(rs_relh_check(cd, tbrelh->rh_relh));

        return(tbrelh->rh_relpriv);
}

/*##**********************************************************************\
 * 
 *              tb_relh_rsrelh
 * 
 * Returns rs relation handle.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      tbrelh - 
 *              
 *              
 * Return value - ref : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE rs_relh_t* tb_relh_rsrelh(
        void*      cd,
        tb_relh_t* tbrelh)
{
        CHK_TBRELH(tbrelh);
        SS_NOTUSED(cd);
        ss_debug(rs_relh_check(cd, tbrelh->rh_relh));

        return(tbrelh->rh_relh);
}


#endif /* defined(TAB0RELH_C) || defined(SS_USE_INLINE) */

#endif /* TAB0RELH_H */
