/*************************************************************************\
**  source       * tab0relc.c
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

#ifdef SS_MYSQL_PERFCOUNT
#include <sswindow.h>
#endif

#include <ssstring.h>
#include <sstraph.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssprint.h>
#include <sspmon.h>
#include <ssqmem.h>

#include <uti0va.h>
#include <uti0vtpl.h>

#include <su0list.h>
#include <su0parr.h>
#include <su0time.h>
#include <su0usrid.h>

#include <dbe0type.h>
#include <dbe0curs.h>
#include <dbe0rel.h>
#include <dbe0tref.h>
#include <dbe0erro.h>

#include <rs0types.h>
#include <rs0atype.h>
#include <rs0ttype.h>
#include <rs0cons.h>
#include <rs0order.h>
#include <rs0relh.h>
#include <rs0pla.h>
#include <rs0key.h>
#include <rs0sdefs.h>
#include <rs0tnum.h>
#include <rs0vbuf.h>

#include "tab1defs.h"
#include "tab1priv.h"
#include "tab1refact.h"
#include "tab0info.h"
#include "tab0blob.h"
#include "tab0tran.h"
#include "tab0trig.h"
#include "tab0relh.h"
#include "tab0seq.h"
#include "tab0sche.h"

#ifdef SS_MYSQL
#include <est1est.h>
#include <est1pla.h>
#else
#include "../est/est1est.h"
#include "../est/est1pla.h"
#endif

#include "tab0relc.h"

#include "tab0hurc.h"

#define JARMOR_UPDATECHANGEDCOLUMNS

#define RELCUR_MAXESTPERF       100
#define RELCUR_COUNT_STEPSIZE   100
#define TB_REVERSESET_ADDCOST   10  /* Cost added to average delay per row
                                       when reversing the result set using
                                       ascending key (value 10 means 1/10
                                       cost addition). */
#define TB_REVERSESET_NONE      0
#define TB_REVERSESET_VECTOR    1
#define TB_REVERSESET_NORMAL    2

#define CHK_RELCUR(cur) ss_dassert(SS_CHKPTR(cur) && (cur)->rc_check == TBCHK_RELCUR)

typedef enum {
        CS_CHANGE_INIT,
        CS_CHANGE_SYNCHISTORY,     /* Update sync history state */
        CS_CHANGE_BEFORETRIGGER,
        CS_CHANGE_CHECK,
        CS_CHANGE_EXEC,
        CS_CHANGE_CASCADE,
        CS_CHANGE_STMTCOMMIT,
        CS_CHANGE_AFTERTRIGGER,
        CS_CHANGE_FREE
} cs_changestate_t;

typedef enum {
        CS_CLOSED,      /* cursor not openend */
        CS_START,       /* start state, next gives the first tuple */
        CS_ROW,         /* row state, rows available */
        CS_NOROW,       /* row state, no row available right now */
        CS_END,         /* end state, prev gives the last tuple */
        CS_EMPTY,       /* no tuples available in search set */
        CS_COUNT,       /* counting number of tuples */
        CS_SAUPDATE,    /* SA update */
        CS_SADELETE,    /* SA delete */
        CS_ERROR        /* error state */
} curstate_t;

struct tbrelcurstruct {

        bool                rc_ishurc;
        ss_debug(tb_check_t rc_check;)      /* check field */
        tb_trans_t*         rc_trans;       /* transaction */
        tb_relh_t*          rc_tbrelh;      /* tab relation handle */
        rs_relh_t*          rc_relh;        /* res relation handle */
        forupdate_t         rc_forupdate;
        bool                rc_insubquery;

        su_list_t*          rc_constr_l;    /* list of rs_const_t* ojects */
        su_list_t*          rc_orderby_l;   /* list of rs_ob_t* objects */
        int                 rc_reverseset;  /* If non-zero, the result set is returned
                                               in reverse order. Used to optimise
                                               desc search when asc index exists
                                               and all order by criteria is in
                                               desc order. Also used by
                                               REVERSE optimizer hint. */
        curstate_t          rc_state;       /* cursor state */
        rs_ano_t*           rc_sellist;     /* the select list, RS_ANO_NULL
                                               terminates the list. Pseudo
                                               columns are marked with
                                               RS_ANO_PSEUDO. Elements are
                                               indices to the physical
                                               ttype (relh_ttype) */
        bool                rc_projectall;
        rs_ttype_t*         rc_selttype;    /* type of the returned tuple, the
                                               type correspond to the rc_sellist
                                            */
        rs_tval_t*          rc_seltvalue;   /* the tuple formed with a sellist.
                                               Pointer to a tval owned by
                                               rc_dbref. */
        rs_vbuf_t*          rc_vbuf;
        int                 rc_prevnextp;
        bool                rc_waitlock;
#ifdef SS_MME
        bool                rc_valueiscopied;   /* TRUE if the current value
                                                   has been copied to this
                                                   cursor. */
        bool                rc_mainmem;
#endif
        tb_est_t*           rc_est;         /* query cost estimate */
        rs_pla_t*           rc_plan;        /* search plan */
        rs_key_t*           rc_plankey;
        bool                rc_plan_reset;  /* If TRUE reset the existing plan. */

        bool                rc_isolationchange_transparent; /* default is FALSE eg. cursor is closed when isolation changes */

        dbe_cursor_t*       rc_dbcur;       /* database cursor */
        dbe_tref_t*         rc_dbtref;      /* tuple reference for db cursor
                                               If non-NULL, pointer to a tref
                                               object owned by rc_dbcur. */
        su_ret_t            rc_errcode;     /* possible error code */
        su_ret_t            rc_fatalerrcode;/* fatal error code, cursor reset can not be used */
        rs_err_t*           rc_errh;

        su_list_t*          rc_pseudo_l;    /* if non-NULL, list of pseudo
                                               attributes */
        su_pa_t*            rc_updaval_pa;  /* Array of avals that are separately
                                               retrieved using function
                                               tb_relcur_aval. */
        ulong               rc_count;       /* Tuple count, used by
                                               relcur_count. */
        ulong               rc_optcount;    /* Optimization count. */
        rs_key_t*           rc_indexhintkey;/* Index selection hint. */
        dbe_cursor_type_t   rc_curtype;
        rs_tval_t*          rc_newtval;     /* Used during update. */
        bool*               rc_phys_selflags; /* Used during update. */
        curstate_t          rc_saoldstate;
        int                 rc_infolevel;
        char*               rc_infostr;
        bool                rc_aggrcount;
        rs_ttype_t*         rc_aggrttype;
        rs_tval_t*          rc_aggrtval;
        cs_changestate_t    rc_changestate;
        void*               rc_trigctx;
        char*               rc_trigstr;
        rs_entname_t*       rc_trigname;
        bool                rc_istrigger;
        bool                rc_isstmtgroup;
        bool                rc_issynchist; /* TRUE, if synchist deleted cursor */
        bool                rc_isvectorconstr;
        bool                rc_canuseindexcursorreset;/* If TRUE we are allowed
                                                         to use dbe index
                                                         reset. */
        bool                rc_resetinternal;   /* Extarnal call to reset cons. */
        bool                rc_resetexternal;   /* Internal reason to reset cons. */
        bool                rc_setcons;         /* Need to set cons. */
        ss_debug(bool       rc_isresetp;)       /* Has there been reset call. */
        bool                rc_longcons;        /* There is long cons since
                                                   last reset. */
        bool                rc_isunknownconstr;
        bool                rc_uselateindexplan;
        bool                rc_isscrollsensitive;
        
        bool                rc_flow_role_resolved;
        bool                rc_ismaster;
        bool                rc_isreplica;
        tb_syncinserthistory_t* rc_sih;
        bool*               rc_p_newplan;
        uint                rc_casc_key;
        bool                rc_casc_key_delc;
        su_pa_t*            rc_casc_states;
        bool                rc_sqlcall;
        ss_beta(long        rc_nrows;)
        long                rc_curid;
        ss_debug(char*      rc_lastcall;)
}; /* rs_relcur_t */

typedef enum {
        CUR_PSEUDO_ROWID,
        CUR_PSEUDO_ROWVER,
        CUR_PSEUDO_ROWFLAGS
#ifdef SS_SYNC
        ,
        CUR_PSEUDO_SYNC_TUPLE_VERSION,
        CUR_PSEUDO_SYNC_ISPUBLTUPLE
#endif /* SS_SYNC */
} cur_pseudotype_t;

typedef struct {
        cur_pseudotype_t    cp_type;
        rs_ano_t            cp_targetano;   /* ano in rc_selttype */
        rs_ano_t            cp_sourceano;   /* ano in rc_selttype */
} cur_pseudocol_t;

ss_beta(long tb_relcur_estnrows_0[20][21];)
ss_beta(long tb_relcur_estnrows_1[20][21];)
ss_beta(long tb_relcur_estnrows_2[20][21];)
ss_beta(long tb_relcur_estnrows_3[20][21];)
ss_beta(su_list_t* tb_relc_estinfolist;)
ss_beta(static void (*relc_estinfooutfp)(char* tablename, tb_relc_estinfo_t* estinfo);)

extern bool rs_sqli_usevectorconstr;

static rs_tval_t* cur_tval(
        void*        cd,
        tb_relcur_t* cur
);
SS_INLINE void cur_tval_free(
        void*        cd,
        tb_relcur_t* cur);

SS_INLINE void cur_tval_freeif(
        void*        cd,
        tb_relcur_t* cur);

static void cur_tval_copy(
        void*        cd,
        tb_relcur_t* cur);

SS_INLINE bool cur_ensure_estimate(
        void*        cd,
        tb_relcur_t* cur
);

static void cur_ensure_search_plan(
        void*        cd,
        tb_relcur_t* cur
);

static bool cur_reset(
        void*        cd,
        tb_relcur_t* cur
);

static bool cur_upd_increment(
        void*        cd,
        rs_ttype_t*  ttype,
    rs_tval_t*   upd_tval,
    rs_tval_t*   new_tval,
    bool*        incrflags,
    rs_err_t**   p_errh
);

static void cur_upd_assign(
        void*        cd,
        rs_ttype_t*  ttype,
    rs_tval_t*   upd_tval,
    rs_tval_t*   new_tval,
    bool*        selflags,
    bool*        incrflags
);

static bool cur_upd_checknotnulls(
        void*        cd,
        rs_ttype_t*  ttype,
        rs_tval_t*   new_tval,
#ifndef NOT_NOTNULL_ANOARRAY
        rs_ano_t*    notnull_anos,
#endif
        bool*        selflags,
        bool*        incrflags,
        bool         triggerp,
        bool*        p_failed_sql_ano);


#ifdef REMOVED_BY_JARMOR
static rs_tval_t* cur_upd_getphysicaltvalue(
        void*        cd,
    tb_relcur_t* cur,
    rs_tval_t*   tvalue
);
#endif /* REMOVED_BY_JARMOR */

static bool cur_upd_constraints(
        void*        cd,
        rs_ttype_t*  ttype,
        rs_tval_t*   new_tval,
        uint         constr_n,
    uint*        constrattrs,
    uint*        constrrelops,
    rs_atype_t** constratypes,
    rs_aval_t**  constravalues,
        rs_ano_t*    p_failed_sql_ano
);

static bool* cur_upd_getphysicalselflags(
        void*         cd,
        tb_relcur_t*  cur,
        bool*         sqlselflags
);

static rs_tval_t* relcur_nextorprev(
    rs_sysi_t*   cd,
    tb_relcur_t* cur,
    uint*        p_finished,
        bool         nextp,
        rs_err_t**   p_errh
);

static uint relcur_update_free(
    void*        cd,
    tb_relcur_t* cur);

/*#***********************************************************************\
 *
 *              cursor_geterrh
 *
 *
 *
 * Parameters :
 *
 *      cur -
 *
 *
 *      p_errh -
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
static void cursor_geterrh(tb_relcur_t *cur, rs_err_t** p_errh)
{
        if (p_errh != NULL) {
            *p_errh = cur->rc_errh;
            cur->rc_errh = NULL;
        }
}

static void cur_pseudocol_done(cur_pseudocol_t* cp)
{
        SsMemFree(cp);
}

/*#***********************************************************************\
 *
 *              cur_pseudoattr_listdelete
 *
 * List delete function for pseudo attributes.
 *
 * Parameters :
 *
 *      p - in, take
 *              Pointer to cur_pseudocol_t.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void cur_pseudoattr_listdelete(void* p)
{
        cur_pseudocol_t* cp = p;

        cur_pseudocol_done(cp);
}

/*#***********************************************************************\
 *
 *              cur_rowid_relopok
 *
 *
 *
 * Parameters :
 *
 *      relop - in
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
static bool cur_rowid_relopok(int relop)
{
        switch (relop) {
            case RS_RELOP_EQUAL:
            case RS_RELOP_NOTEQUAL:
                return(TRUE);
            case RS_RELOP_LT:
            case RS_RELOP_GT:
            case RS_RELOP_LE:
            case RS_RELOP_GE:
            default:
                return(FALSE);
        }
}

/*#***********************************************************************\
 *
 *              cur_rowid_vtplok
 *
 *
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      cur - in, use
 *
 *
 *      vtpl - in, use
 *
 *
 *      len - in
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
static bool cur_rowid_vtplok(
        void* cd,
        tb_relcur_t* cur,
        vtpl_t* vtpl,
        long len)
{
        int nattrs;
        rs_key_t* clustkey;

        if (!vtpl_consistent(vtpl)) {
            return(FALSE);
        }

        if ((ulong)len != vtpl_grosslen(vtpl)) {
            return(FALSE);
        }

        nattrs = vtpl_vacount(vtpl);
        clustkey = rs_relh_clusterkey(cd, cur->rc_relh);

        if (rs_key_nrefparts(cd, clustkey) != nattrs) {
            return(FALSE);
        }
        return(TRUE);
}

/*#***********************************************************************\
 *
 *              cur_rowid_setcons
 *
 * Sets constraints from a tuple reference (ROWID) pseudo attribute.
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      cur - in out, use
 *
 *
 *      c_atype - in
 *
 *
 *      c_aval - in
 *
 *
 *      c_relop - in
 *
 *
 *      c_escchar - in
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
static void cur_rowid_setcons(
        void*           cd,
        tb_relcur_t*    cur,
        su_bflag_t      flags,
        rs_sqlcons_t*   sqlcons,
        int             c_escchar)
{
        rs_atype_t*     c_atype = sqlcons->sc_atype;
        rs_aval_t*      c_aval = sqlcons->sc_aval;
        int             i;
        int             nref;
        rs_key_t*       clustkey;
        rs_atype_t*     atype;
        rs_aval_t*      aval;
        vtpl_t*         vtpl;
        ulong           len;
        rs_ttype_t*     ttype;
        bool            unknown;

        ss_dprintf_3(("cur_rowid_setcons:%ld\n", cur->rc_curid));
        ss_dassert(cur->rc_errcode == SU_SUCCESS);

        /* Check that relop is ok. */
        if (!cur_rowid_relopok(sqlcons->sc_relop)) {
            ss_dprintf_4(("cur_rowid_setcons:%ld:E_ILLPSEUDOCOLRELOP\n", cur->rc_curid));
            cur->rc_errcode = E_ILLPSEUDOCOLRELOP;
            cur->rc_fatalerrcode = cur->rc_errcode;
            return;
        }

        ss_dassert(c_atype != NULL);

        unknown = rs_aval_isunknown(cd, sqlcons->sc_atype, sqlcons->sc_aval);

        /* Check that atype is of correct type. */
        if (c_atype != NULL && rs_atype_datatype(cd, c_atype) != RSDT_BINARY) {
            ss_dprintf_4(("cur_rowid_setcons:%ld:E_ILLPSEUDOCOLDATATYPE\n", cur->rc_curid));
            cur->rc_errcode = E_ILLPSEUDOCOLDATATYPE;
            cur->rc_fatalerrcode = cur->rc_errcode;
            return;
        }
        if (!unknown &&  rs_aval_isnull(cd, c_atype, c_aval)){
            ss_dprintf_4(("cur_rowid_setcons:%ld:E_ILLPSEUDOCOLDATA\n", cur->rc_curid));
            cur->rc_errcode = E_ILLPSEUDOCOLDATA;
            unknown = TRUE;
        }
        if (unknown) {
            vtpl = NULL;
        } else {
            vtpl = rs_aval_getdata(cd, c_atype, c_aval, &len);
            /* Check that data length is sensible. */
            if (len >= 8 * 1024L) {
                /* This surely must be illegal. */
                ss_dprintf_4(("cur_rowid_setcons:%ld:E_ILLPSEUDOCOLDATA, len=%d\n", cur->rc_curid, len));
                cur->rc_errcode = E_ILLPSEUDOCOLDATA;
                unknown = TRUE;
            }

            if (!cur_rowid_vtplok(cd, cur, vtpl, len)) {
                ss_dprintf_4(("cur_rowid_setcons:%ld:E_ILLPSEUDOCOLDATA, vtpl not ok\n", cur->rc_curid));
                cur->rc_errcode = E_ILLPSEUDOCOLDATA;
                unknown = TRUE;
            }
        }

        clustkey = rs_relh_clusterkey(cd, cur->rc_relh);
        nref = rs_key_nrefparts(cd, clustkey);
        atype = rs_atype_initbinary(cd);
        if (unknown) {
            aval = NULL;
        } else {
            aval = rs_aval_create(cd, atype);
        }
        ttype = cur->rc_selttype;

        sqlcons->sc_alias = FALSE;

        ss_dprintf_4(("cur_rowid_setcons:%ld:create constraints for rowid columns\n", cur->rc_curid));

        for (i = 0; i < nref; i++) {

            if (!rs_keyp_constvalue(cd, clustkey, i)) {
                rs_ano_t    ano;
                va_t*       va;
                rs_cons_t*  cons;
                rs_atype_t* c_atype;

                ano = rs_keyp_ano(cd, clustkey, i);

                if (aval != NULL) {
                    va = vtpl_getva_at(vtpl, i);
                    rs_aval_setva(cd, atype, aval, va);
                }

#ifdef SS_MME
                /* MME requires the constraint atype to be the same as
                   the value's atype. */
                c_atype = rs_ttype_atype(cd, ttype, ano);
#else
                c_atype = atype;
#endif
                cons = rs_cons_init(cd,
                                    sqlcons->sc_relop,
                                    ano,
                                    c_atype,
                                    aval,
                                    flags | RS_CONSFLAG_FORCECONVERT,
                                    sqlcons,
                                    c_escchar,
                                    c_atype,
                                    NULL);
                ss_dassert(cons != NULL);
                ss_dassert(cur->rc_errh == NULL);

                rs_cons_setsolved(cd, cons, FALSE);

                sqlcons->sc_alias = TRUE;

                ss_dassert(cur->rc_constr_l);
                su_list_insertlast(cur->rc_constr_l, cons);
                ss_dassert(su_list_length(cur->rc_constr_l) < 1000);
            }
        }
        if (aval != NULL) {
            rs_aval_free(cd, atype, aval);
        }
        rs_atype_free(cd, atype);
}

/*#***********************************************************************\
 *
 *              cur_rowver_setcons
 *
 * Sets version number constraint.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cur - use
 *
 *
 *      c_atype - in
 *
 *
 *      c_aval - in
 *
 *
 *      c_relop - in
 *
 *
 *      c_escchar - in
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
static void cur_rowver_setcons(
        void*         cd,
        tb_relcur_t*  cur,
        su_bflag_t    flags,
        rs_sqlcons_t* sqlcons,
        int           c_escchar)
{
        uint c_relop = sqlcons->sc_relop;
        rs_ttype_t* relh_ttype;
        rs_atype_t* relh_atype;
        rs_aval_t* aval = NULL;
        rs_cons_t* cons;
        rs_ano_t ano;
        char* data;
        ulong datalen;
        bool unknown;

        ss_dprintf_3(("cur_rowver_setcons:%ld\n", cur->rc_curid));

        unknown = rs_aval_isunknown(cd, sqlcons->sc_atype, sqlcons->sc_aval);

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);
        ano = rs_ttype_anobyname(cd, relh_ttype, (char *)RS_ANAME_TUPLE_VERSION);
        ss_dassert(ano != RS_ANO_NULL);
        relh_atype = rs_ttype_atype(cd, relh_ttype, ano);
        if (!unknown) {
            if (rs_aval_isnull(cd, sqlcons->sc_atype, sqlcons->sc_aval)) {
                cur->rc_errcode = E_ILLPSEUDOCOLDATA;
                unknown = TRUE;
            } else {
                data = rs_aval_getdata(cd, sqlcons->sc_atype, sqlcons->sc_aval, &datalen);
                ss_dassert(data != NULL);

                if (datalen == 1 && data[0] == '\0') {
                    /* Empty version number, translate equal relop to NULL relop.
                     */
                    switch (c_relop) {
                        case RS_RELOP_EQUAL:
                            c_relop = RS_RELOP_ISNULL;
                            break;
                        case RS_RELOP_NOTEQUAL:
                            c_relop = RS_RELOP_ISNOTNULL;
                            break;
                        default:
                            break;
                    }
                } else {
                    /* Not empty version number, store it in binary format
                     *  (without trailing zero byte) to the aval.
                     * Note: this code should work on both
                     * possible TUPLE_VERSION datatypes
                     * VARBINARY and BIGINT
                     */
                    va_t va;

                    aval = rs_aval_create(cd, relh_atype);
                    ss_dassert(sizeof(va) > datalen);
                    va_setdata(&va, data, (size_t)datalen);
                    rs_aval_setva(cd, relh_atype, aval, &va);
                }
            }
        }

        cons = rs_cons_init(
                    cd,
                    c_relop,
                    ano,
                    relh_atype,
                    aval,
                    flags | RS_CONSFLAG_FORCECONVERT,
                    sqlcons,
                    c_escchar,
                    relh_atype,
                    NULL);

        ss_dassert(cons != NULL);

        rs_cons_setsolved(cd, cons, FALSE);

        ss_dassert(cur->rc_constr_l);
        su_list_insertlast(cur->rc_constr_l, cons);
        ss_dassert(su_list_length(cur->rc_constr_l) < 1000);

        if (aval != NULL) {
            rs_aval_free(cd, relh_atype, aval);
        }
}

#ifdef SS_SYNC
/*#***********************************************************************\
 *
 *              cur_synctuplevers_setcons
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      c_atype -
 *
 *
 *      c_aval -
 *
 *
 *      c_relop -
 *
 *
 *      c_escchar -
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
static bool cur_synctuplevers_setcons(
        void*         cd,
        tb_relcur_t*  cur,
        su_bflag_t    flags,
        rs_sqlcons_t* sqlcons,
        int           c_escchar)
{
        rs_atype_t* c_atype = sqlcons->sc_atype;
        rs_aval_t*  c_aval = sqlcons->sc_aval;
        rs_ttype_t* relh_ttype;
        rs_atype_t* atype = NULL;
        rs_cons_t* cons;
        rs_ano_t ano;

        ss_dprintf_3(("cur_synctuplevers_setcons:%ld\n", cur->rc_curid));

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);

        ano = rs_ttype_anobyname(cd, relh_ttype, (char *)RS_ANAME_SYNCTUPLEVERS);

        if (ano != RS_ANO_NULL) {

            /* If we have physical sync tuple version attribute, set the
             * constraint.
             */

            atype = rs_ttype_atype(cd, relh_ttype, ano);
            rs_atype_setsync(cd, atype, TRUE);

            cons = rs_cons_init(
                    cd,
                    sqlcons->sc_relop,
                    ano,
                    c_atype,
                    c_aval,
                    flags | RS_CONSFLAG_FORCECONVERT,
                    sqlcons,
                    c_escchar,
                    atype,
                    NULL);

            ss_dassert(cons != NULL);

            /* Need to do this after rs_cons_init because a new atype
             * is created for the cons.
             */
            c_atype = rs_cons_atype(cd, cons);
            if (c_atype != NULL) {
                rs_atype_setsync(cd, c_atype, TRUE);
            }

            rs_cons_setsolved(cd, cons, FALSE);

            ss_dassert(cur->rc_constr_l);
            su_list_insertlast(cur->rc_constr_l, cons);
            ss_dassert(su_list_length(cur->rc_constr_l) < 1000);

            return(TRUE);

        } else {
            return(FALSE);
        }
}
#endif /* SS_SYNC */

#ifdef SS_SYNC
static bool cur_sync_ispubltuple_setcons(
        void*         cd,
        tb_relcur_t*  cur,
        su_bflag_t    flags,
        rs_sqlcons_t* sqlcons,
        int           c_escchar)
{
        rs_atype_t* c_atype = sqlcons->sc_atype;
        rs_aval_t*  c_aval = sqlcons->sc_aval;
        rs_ttype_t* relh_ttype;
        rs_atype_t* atype = NULL;
        rs_cons_t* cons;
        rs_ano_t ano;

        ss_dprintf_3(("cur_sync_ispubltuple_setcons:%ld\n", cur->rc_curid));

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);

        ano = rs_ttype_anobyname(cd, relh_ttype, (char *)RS_ANAME_SYNC_ISPUBLTUPLE);

        if (ano != RS_ANO_NULL) {

            /* If we have physical sync ispubltuple attribute, set the
             * constraint.
             */

            atype = rs_ttype_atype(cd, relh_ttype, ano);
            rs_atype_setsync(cd, atype, TRUE);
            if (c_atype != NULL) {
                rs_atype_setsync(cd, c_atype, TRUE);
            }
            cons = rs_cons_init(
                    cd,
                    sqlcons->sc_relop,
                    ano,
                    c_atype,
                    c_aval,
                    flags | RS_CONSFLAG_FORCECONVERT,
                    sqlcons,
                    c_escchar,
                    atype,
                    NULL);

            ss_dassert(cons != NULL);

            /* Need to do this after rs_cons_init because a new atype
             * is created for the cons.
             */
            c_atype = rs_cons_atype(cd, cons);
            if (c_atype != NULL) {
                rs_atype_setsync(cd, c_atype, TRUE);
            }

            rs_cons_setsolved(cd, cons, FALSE);

            ss_dassert(cur->rc_constr_l);
            su_list_insertlast(cur->rc_constr_l, cons);
            ss_dassert(su_list_length(cur->rc_constr_l) < 1000);

            return(TRUE);

        } else {
            return(FALSE);
        }
}
#endif /* SS_SYNC */

/*#***********************************************************************\
 *
 *              cur_ensure_dbtref
 *
 * Ensures that there is a dbtref.
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      cur - in out, use
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
static void cur_ensure_dbtref(
        void*        cd __attribute__ ((unused)),
        tb_relcur_t* cur)
{
        ss_dassert(cur->rc_state == CS_ROW);

        if (cur->rc_dbtref == NULL) {
            cur->rc_dbtref = dbe_cursor_gettref(
                    cur->rc_dbcur, cur->rc_seltvalue);
#ifdef SS_MME
            ss_dassert(cur->rc_dbtref != NULL
                || cur->rc_mainmem);
#else
            ss_dassert(cur->rc_dbtref != NULL);
#endif
        }
}

/*#***********************************************************************\
 *
 *              cur_pseudoattr_setaval
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      cp -
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
static bool cur_pseudoattr_setaval(
        void*               cd,
        tb_relcur_t*        cur,
        cur_pseudocol_t*    cp)
{
        rs_atype_t* target_atype;
        rs_aval_t* target_aval;
        rs_atype_t* source_atype;
        rs_aval_t* source_aval;
        bool succp = TRUE;
        long flags;

        ss_dprintf_3(("cur_pseudoattr_setaval:%ld\n", cur->rc_curid));

        target_atype = rs_ttype_atype(
                            cd,
                            cur->rc_selttype,
                            cp->cp_targetano);
        target_aval = rs_tval_aval(
                            cd,
                            cur->rc_selttype,
                            cur->rc_seltvalue,
                            cp->cp_targetano);

        switch (cp->cp_type) {

            case CUR_PSEUDO_ROWID:
                ss_dassert(cp->cp_sourceano == RS_ANO_NULL);
                ss_dassert(rs_atype_datatype(cd, target_atype) == RSDT_BINARY);
                cur_ensure_dbtref(cd, cur);
                dbe_tref_setrowiddata(
                    cd,
                    cur->rc_dbtref,
                    cur->rc_selttype,
                    target_atype,
                    target_aval,
                    rs_relh_clusterkey(cd, cur->rc_relh));
                break;

            case CUR_PSEUDO_ROWVER:
                ss_dassert(cp->cp_sourceano != RS_ANO_NULL);
                source_atype = rs_ttype_atype(
                                    cd,
                                    cur->rc_selttype,
                                    cp->cp_sourceano);
                source_aval = rs_tval_aval(
                                    cd,
                                    cur->rc_selttype,
                                    cur->rc_seltvalue,
                                    cp->cp_sourceano);
                if (rs_aval_isnull(cd, source_atype, source_aval)) {
                    /* Translate NULL tuple version into single zero byte.
                     */
                    succp = rs_aval_setbdata_ext(
                                cd,
                                target_atype,
                                target_aval,
                                (char *)"",
                                1,
                                NULL);
                    ss_dassert(succp);
                } else {
                    /* Set the data separately so that the aval data area
                     * contains zero byte at the end like other binary
                     * fields.
                     */
                    va_t* va;
                    char* data;
                    va_index_t len;

                    va = rs_aval_va(cd, source_atype, source_aval);
                    ss_dassert(va != NULL);
                    data = va_getdata(va, &len);
                    ss_dassert(len <= 10);  /* Sanity check */
                    succp = rs_aval_setbdata_ext(
                                cd,
                                target_atype,
                                target_aval,
                                data,
                                len,
                                NULL);
                    ss_dassert(succp);
                }
                break;

            case CUR_PSEUDO_ROWFLAGS:
                /* How should the following dassert be?? */
                ss_dassert(cp->cp_sourceano != RS_ANO_NULL);
                ss_rc_dassert(
                    rs_atype_datatype(cd, target_atype) == RSDT_INTEGER,
                    rs_atype_datatype(cd, target_atype));
                flags = rs_tval_getrowflags(cd, cur->rc_selttype, cur->rc_seltvalue);
                ss_dprintf_4(("cur_pseudoattr_setaval:%ld:CUR_PSEUDO_ROWFLAGS, set RS_AVAL_ROWFLAG_SYNCHISTORYDELETED\n", cur->rc_curid));
                if (cur->rc_issynchist) {
                    ss_dprintf_4(("cur_pseudoattr_setaval:%ld:CUR_PSEUDO_ROWFLAGS, set RS_AVAL_ROWFLAG_SYNCHISTORYDELETED\n", cur->rc_curid));
                    flags |= RS_AVAL_ROWFLAG_SYNCHISTORYDELETED;
                }
                succp = rs_aval_setlong_ext(
                            cd,
                            target_atype,
                            target_aval,
                            flags,
                            NULL);
                break;
#ifdef SS_SYNC
            case CUR_PSEUDO_SYNC_ISPUBLTUPLE:
            case CUR_PSEUDO_SYNC_TUPLE_VERSION:
                if (cp->cp_sourceano != RS_ANO_NULL) {
                    source_atype = rs_ttype_atype(
                                        cd,
                                        cur->rc_selttype,
                                        cp->cp_sourceano);
                    source_aval = rs_tval_aval(
                                        cd,
                                        cur->rc_selttype,
                                        cur->rc_seltvalue,
                                        cp->cp_sourceano);
                    /* VOYAGER_SYNCHIST */
                    if (rs_aval_isnull(cd, source_atype, source_aval)) {
                        if (cp->cp_type == CUR_PSEUDO_SYNC_ISPUBLTUPLE) {
                            succp = rs_aval_setlong_ext(
                                    cd,
                                    target_atype,
                                    target_aval,
                                    rs_sysi_getlocalsyncid(cd),
                                    NULL);
                            /* we use SS_INT4_MIN. Not rs_sysi_getlocalsyncid(cd) */

                            ss_dassert(succp);
                        } else {
                            rs_tuplenum_t tuplevers;
                            rs_tuplenum_init(&tuplevers);

                            succp = rs_tuplenum_setintoaval(
                                        &tuplevers,
                                        cd,
                                        target_atype,
                                        target_aval);
                            ss_dassert(succp);
                        }

                    } else {
                        rs_aval_setva(
                            cd,
                            target_atype,
                            target_aval,
                            rs_aval_va(cd, source_atype, source_aval));
                    }
                } else {
                    rs_aval_setnull(
                        cd,
                        target_atype,
                        target_aval);
                }
                break;

#endif /* SS_SYNC */
            default:
                ss_derror;
        }

        return(succp);
}

/*#***********************************************************************\
 *
 *              cur_pseudoattr_settval
 *
 * Sets tuple value in the cursor to contain also correct pseudo
 * attribute data.
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      cur - in out, use
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
static void cur_pseudoattr_settval(
        void*        cd,
        tb_relcur_t* cur)
{
        su_list_node_t* node;
        cur_pseudocol_t* cp;
        bool succp;

        ss_dprintf_3(("cur_pseudoattr_settval:%ld\n", cur->rc_curid));

        su_list_do_get(cur->rc_pseudo_l, node, cp) {
            ss_dprintf_4(("cur_pseudoattr_settval:%ld:pseudotype = %d\n", cur->rc_curid, cp->cp_type));
            succp = cur_pseudoattr_setaval(cd, cur, cp);
            ss_dassert(succp);

        }
}

/*#***********************************************************************\
 *
 *              cur_pseudocol_init
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      type -
 *
 *
 *      target_ano -
 *
 *
 *      source_ano -
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
static cur_pseudocol_t* cur_pseudocol_init(
        void* cd __attribute__ ((unused)),
        cur_pseudotype_t type,
        rs_ano_t target_ano,
        rs_ano_t source_ano)
{
        cur_pseudocol_t* cp;

        ss_dprintf_3(("cur_pseudocol_init\n"));
        ss_dassert(target_ano != RS_ANO_NULL);

        cp = SSMEM_NEW(cur_pseudocol_t);
        cp->cp_type = type;
        cp->cp_targetano = target_ano;
        cp->cp_sourceano = source_ano;

        return(cp);
}

/*#***********************************************************************\
 *
 *              cur_project_pseudo
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      relh_ttype -
 *
 *
 *      i -
 *
 *
 *      sellist -
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
static cur_pseudocol_t* cur_project_pseudo(
        void* cd,
        rs_ttype_t* relh_ttype,
        int i,
        rs_ano_t* p_ano)
{
        int ano;
        char* aname;
        cur_pseudocol_t* cp = NULL;

        aname = rs_ttype_aname(cd, relh_ttype, i);

        if (strcmp(aname, RS_PNAME_ROWID) == 0) {
            ano = RS_ANO_PSEUDO;
            cp = cur_pseudocol_init(
                    cd,
                    CUR_PSEUDO_ROWID,
                    i,
                    RS_ANO_NULL);
        } else if (strcmp(aname, RS_PNAME_ROWVER) == 0) {
            rs_ano_t version_ano;
            version_ano = rs_ttype_anobyname(
                            cd,
                            relh_ttype,
                            (char *)RS_ANAME_TUPLE_VERSION);
            ss_dassert(version_ano != RS_ANO_NULL);
            ano = version_ano;
            cp = cur_pseudocol_init(
                    cd,
                    CUR_PSEUDO_ROWVER,
                    i,
                    version_ano);
        } else if (strcmp(aname, RS_PNAME_ROWFLAGS) == 0) {
            rs_ano_t rowflags_ano;
            rowflags_ano = rs_ttype_anobyname(
                            cd,
                            relh_ttype,
                            (char *)RS_PNAME_ROWFLAGS);
            ss_dassert(rowflags_ano != RS_ANO_NULL);
            /* Has to be marked with RS_ANO_PSEUDO or estimator chokes */
            /* ano = rowflags_ano; */
            ano = RS_ANO_PSEUDO;
            cp = cur_pseudocol_init(
                    cd,
                    CUR_PSEUDO_ROWFLAGS,
                    i,
                    rowflags_ano);

#ifdef SS_SYNC
        } else if (strcmp(aname, RS_PNAME_SYNCTUPLEVERS) == 0) {
            rs_ano_t version_ano;
            version_ano = rs_ttype_anobyname(
                            cd,
                            relh_ttype,
                            (char *)RS_ANAME_SYNCTUPLEVERS);
            if (version_ano != RS_ANO_NULL) {
                ano = version_ano;
            } else {
                ano = RS_ANO_PSEUDO;
            }
            cp = cur_pseudocol_init(
                    cd,
                    CUR_PSEUDO_SYNC_TUPLE_VERSION,
                    i,
                    version_ano);
        } else if (strcmp(aname, RS_PNAME_SYNC_ISPUBLTUPLE) == 0) {
            rs_ano_t ispubltuple_ano;
            ispubltuple_ano = rs_ttype_anobyname(
                                cd,
                                relh_ttype,
                                (char *)RS_ANAME_SYNC_ISPUBLTUPLE);
            if (ispubltuple_ano != RS_ANO_NULL) {
                ano = ispubltuple_ano;
            } else {
                ano = RS_ANO_PSEUDO;
            }
            cp = cur_pseudocol_init(
                    cd,
                    CUR_PSEUDO_SYNC_ISPUBLTUPLE,
                    i,
                    ispubltuple_ano);

#endif /* SYNC */

        } else {
            /* Ignore other pseudo attributes. */
            ano = RS_ANO_PSEUDO;
            ss_derror;
        }
        if (p_ano != NULL) {
            *p_ano = ano;
        }
        return(cp);
}

/*#***********************************************************************\
 *
 *              cur_pseudoattr_project
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      relh_ttype -
 *
 *
 *      i -
 *
 *
 *      p_ano -
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
static void cur_pseudoattr_project(
    void*        cd,
    tb_relcur_t* cur,
        rs_ttype_t*  relh_ttype,
        int          i,
    int*         p_ano)
{
        cur_pseudocol_t* cp;

        ss_dprintf_3(("cur_pseudoattr_project\n"));

        cp = cur_project_pseudo(
                cd,
                relh_ttype,
                i,
                p_ano);
        if (cp != NULL) {
            if (cur->rc_pseudo_l == NULL) {
                cur->rc_pseudo_l = su_list_init(cur_pseudoattr_listdelete);
            }
            su_list_insertlast(cur->rc_pseudo_l, cp);
        }
}

/*#***********************************************************************\
 *
 *              cur_sql_project_all
 *
 * Creates a select array where all attributes visible to the SQL are
 * selected. The select array is allocated using SsMemAlloc, and the
 * caller is responsible to free the array using SsMemFree. The array
 * is terminated with RS_ANO_NULL.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *          relation cursor pointer
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value:
 *
 * Limitations  :
 *
 * Globals used :
 */
static void cur_sql_project_all(
        void* cd,
        tb_relcur_t* cur,
        rs_relh_t* relh)
{
        rs_ttype_t* relh_ttype;
        uint        nattrs;
        uint        i;

        ss_dprintf_3(("cur_sql_project_all:%ld\n", cur->rc_curid));

        relh_ttype = rs_relh_ttype(cd, relh);
        nattrs     = rs_ttype_nattrs(cd, relh_ttype);

        ss_dassert(cur->rc_selttype == relh_ttype);
        cur->rc_projectall = FALSE;
        cur->rc_sellist = SsMemAlloc((nattrs + 1) * sizeof(rs_ano_t));

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype = rs_ttype_atype(cd, relh_ttype, i);

            if (rs_atype_pseudo(cd, atype)) {
                /* Note! Pseudo columns must be added to the select list
                 * because SQL expects that also pseudo columns are
                 * returned.
                 */
                int ano;
                cur_pseudoattr_project(cd, cur, relh_ttype, i, &ano);
                cur->rc_sellist[i] = ano;

            } else if (rs_atype_isuserdefined(cd, atype)) {

                cur->rc_sellist[i] = i;

            } else {
                /* Ignore other attributes. */
                cur->rc_sellist[i] = RS_ANO_PSEUDO;
            }
        }
        cur->rc_sellist[i] = RS_ANO_NULL;
}

/*#***********************************************************************\
 *
 *              relcur_updaval_clear
 *
 * Clears contents of rc_updaval_pa, but does not remove the pa.
 *
 * Parameters :
 *
 *      cd - in
 *              Client data.
 *
 *      cur - use
 *              Cursor object.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void relcur_updaval_clear(
    void*        cd,
    tb_relcur_t* cur)
{
        int ano;
        rs_aval_t* aval;
        rs_ttype_t* ttype;

        ss_dprintf_3(("relcur_updaval_clear:%ld\n", cur->rc_curid));
        ss_dassert(su_pa_nelems(cur->rc_updaval_pa) > 0);

        ttype = rs_relh_ttype(cd, cur->rc_relh);

        su_pa_do_get(cur->rc_updaval_pa, ano, aval) {
            rs_atype_t* atype;
            atype = rs_ttype_atype(cd, ttype, ano);
            rs_aval_free(cd, atype, aval);
            su_pa_remove(cur->rc_updaval_pa, ano);
        }
}

/*##**********************************************************************\
 *
 *              tb_relcur_create_nohurc_ex
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      tbrelh -
 *
 *
 *      forupdate -
 *
 *
 *      insubquery -
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
tb_relcur_t* tb_relcur_create_nohurc_ex(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        rs_relh_t*  rsrelh,
        forupdate_t forupdate,
        bool        insubquery,
        bool        sqlcall)
{
        tb_relcur_t* cur;
        static long curid; /* For debugging. */

        if (tbrelh != NULL) {
            ss_dassert(rsrelh == NULL);
            rsrelh = tb_relh_rsrelh(cd, tbrelh);
        } else {
            ss_dassert(rsrelh != NULL);
        }

        ss_pprintf_1(("tb_relcur_create: relname='%s', forupdate=%d, insubquery=%d\n",
            rs_relh_name(cd, rsrelh), forupdate, insubquery));
        SS_NOTUSED(cd);

        if (insubquery) {
            forupdate = FORUPDATE_NO;
        }
        if (forupdate == FORUPDATE_NO && rs_sysi_getliveovercommit(cd)) {
            forupdate = FORUPDATE_FAKE;
        }

#ifdef TB_RELCUR_CACHE
        {
            su_list_t* list;
            su_list_node_t* n;

            ss_derror;

            list = rs_sysi_gettbcurlist(cd);
            if (list != NULL) {
                su_list_do_get(list, n, cur) {
                    if (cur->rc_tbrelh == tbrelh
                    &&  cur->rc_trans == trans
                    &&  cur->rc_forupdate == forupdate
                    &&  cur->rc_insubquery == insubquery
                    &&  (forupdate < FORUPDATE_UPDATE ||
                         forupdate > FORUPDATE_SEARCHED_DELETE))
                    {
                        ss_dprintf_2(("REUSE\n"));
                        su_list_remove(list, n);
                        ss_dassert(cur->rc_curtype == DBE_CURSOR_SELECT);
                        tb_relcur_reset(cd, cur);
                        return(cur);
                    }
                }
            }
        }
#endif /* TB_RELCUR_CACHE */

        cur = SSMEM_NEW(tb_relcur_t);

        cur->rc_ishurc = FALSE;

        cur->rc_tbrelh = tbrelh;
        cur->rc_relh = rsrelh;
        cur->rc_trans = trans;
        cur->rc_forupdate = forupdate;
        cur->rc_insubquery = insubquery;
        cur->rc_constr_l = su_list_init(NULL);
        cur->rc_orderby_l = su_list_init(NULL);
        cur->rc_reverseset = TB_REVERSESET_NONE;
        cur->rc_state = CS_CLOSED;
        cur->rc_sellist = NULL;
        cur->rc_selttype = rs_relh_ttype(cd, cur->rc_relh);
        cur->rc_projectall = TRUE;
        cur->rc_seltvalue = NULL;
        
        cur->rc_isscrollsensitive = rs_sysi_isscrollsensitive(cd);
        ss_dprintf_4(("tb_relcur_create_nohurc_ex: cur->rc_isscrollsensitive=%d\n",
                      cur->rc_isscrollsensitive));
        
        if ( (cur->rc_isscrollsensitive == FALSE) &&
             (forupdate == FORUPDATE_NO || forupdate == FORUPDATE_FAKE) ) {
            cur->rc_vbuf = rs_vbuf_init(cd, cur->rc_selttype, 10);
        } else {
            cur->rc_vbuf = rs_vbuf_init(cd, cur->rc_selttype, 2);
        }
        cur->rc_prevnextp = 2;
        cur->rc_waitlock = FALSE;
#ifdef SS_MME
        cur->rc_valueiscopied = FALSE;
        cur->rc_mainmem = (rs_relh_reltype(cd, cur->rc_relh) == RS_RELTYPE_MAINMEMORY);
#endif
        cur->rc_est = NULL;
        cur->rc_plan = NULL;
        cur->rc_plankey = NULL;
        cur->rc_plan_reset = FALSE;
        cur->rc_dbcur = NULL;
        cur->rc_isolationchange_transparent = FALSE; /* default is FALSE eg. cursor is closed when isolation changes */
        cur->rc_dbtref = NULL;
        cur->rc_errcode = SU_SUCCESS;
        cur->rc_fatalerrcode = SU_SUCCESS;
        cur->rc_errh = NULL;
        cur->rc_pseudo_l = NULL;
        cur->rc_updaval_pa = su_pa_init();
        cur->rc_count = 0;
        cur->rc_optcount = 1;
        cur->rc_indexhintkey = NULL;
        cur->rc_newtval = NULL;
        cur->rc_phys_selflags = NULL;
        cur->rc_infolevel = rs_sysi_sqlinfolevel(cd, FALSE);
        cur->rc_infostr = NULL;
        cur->rc_aggrcount = FALSE;
        cur->rc_aggrttype = NULL;
        cur->rc_changestate = CS_CHANGE_INIT;
        cur->rc_trigstr = NULL;
        cur->rc_trigname = NULL;
        cur->rc_trigctx = NULL;
        cur->rc_istrigger = FALSE;
        cur->rc_issynchist = FALSE;
        cur->rc_isvectorconstr = FALSE;
        cur->rc_isstmtgroup = FALSE;
        cur->rc_canuseindexcursorreset = rs_sysi_useindexcursorreset(cd);
        cur->rc_resetinternal = FALSE;
        cur->rc_resetexternal = TRUE;
        cur->rc_setcons = TRUE;
        ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));
        ss_debug(cur->rc_isresetp = FALSE;)
        cur->rc_longcons = FALSE;
        cur->rc_uselateindexplan = rs_sysi_uselateindexplan(cd);
        cur->rc_isunknownconstr = FALSE;

        cur->rc_flow_role_resolved = FALSE;
        cur->rc_ismaster = FALSE;
        cur->rc_isreplica = FALSE;

        cur->rc_sih = NULL;

        cur->rc_p_newplan = rs_sysi_getspnewplanptr(cd);
        cur->rc_casc_key = 0;
        cur->rc_casc_key_delc = TRUE;
        cur->rc_casc_states = NULL;
        cur->rc_sqlcall = sqlcall;

        ss_debug(cur->rc_check = TBCHK_RELCUR);
        ss_beta(cur->rc_nrows = 0);
        cur->rc_curid = curid++;
        ss_debug(cur->rc_lastcall = NULL;)

        switch (forupdate) {
            case FORUPDATE_UPDATE:
                ss_dprintf_2(("tb_relcur_create:%ld:SELECT ... FOR UPDATE\n", cur->rc_curid));
                cur->rc_curtype = DBE_CURSOR_FORUPDATE;   /* SELECT .. FOR UPDATE */
                break;
            case FORUPDATE_SEARCHED_UPDATE:
                ss_dprintf_2(("tb_relcur_create:%ld:UPDATE\n", cur->rc_curid));
                cur->rc_curtype = DBE_CURSOR_UPDATE;      /* searched UPDATE */
                break;
            case FORUPDATE_SEARCHED_DELETE:
                ss_dprintf_2(("tb_relcur_create:%ld:DELETE\n", cur->rc_curid));
                cur->rc_curtype = DBE_CURSOR_DELETE;      /* searched DELETE */
                break;
            case FORUPDATE_FAKE:
                ss_dprintf_2(("tb_relcur_create:%ld:SASELECT\n", cur->rc_curid));
                if (cur->rc_mainmem) {
                    cur->rc_curtype = DBE_CURSOR_SASELECT;
                } else {
                    cur->rc_curtype = DBE_CURSOR_SELECT;
                }
                break;
            default:
                ss_dprintf_2(("tb_relcur_create:%ld:SELECT\n", cur->rc_curid));
                cur->rc_curtype = DBE_CURSOR_SELECT;      /* SELECT */
                break;
        }

        if (sqlcall) {
            if (cur->rc_mainmem) {
                ss_dprintf_2(("tb_relcur_create_nohurc_ex:RS_SYSI_MTABLE\n"));
                rs_sysi_setstmttabletypes(cd, RS_SYSI_MTABLE);
            } else {
                ss_dprintf_2(("tb_relcur_create_nohurc_ex:RS_SYSI_DTABLE\n"));
                rs_sysi_setstmttabletypes(cd, RS_SYSI_DTABLE);
            }
        }

        return(cur);
}

/*##**********************************************************************\
 *
 *              tb_relcur_create_nohurc
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      tbrelh -
 *
 *
 *      forupdate -
 *
 *
 *      insubquery -
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
tb_relcur_t* tb_relcur_create_nohurc(cd, trans, tbrelh, forupdate, insubquery, sqlcall)
    rs_sysi_t*  cd;
    tb_trans_t* trans;
    tb_relh_t*  tbrelh;
    forupdate_t forupdate;
    bool        insubquery;
    bool        sqlcall;
{
    return(tb_relcur_create_nohurc_ex(cd, trans, tbrelh, NULL, forupdate, insubquery, sqlcall));
}

/*##**********************************************************************\
 *
 *              tb_relcur_create
 *
 * Creates a relation cursor into a specified relation handle.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle of the transaction to which the cursor
 *          belongs to (NULL if transactions are not in use)
 *
 *      tbrelh - in, hold
 *              pointer to the relation handle
 *
 *      forupdate - in
 *          1 if SELECT ... FOR UPDATE,
 *          2 if searched UPDATE,
 *          3 if searched DELETE,
 *          4 if source for searched INSERT,
 *          5 if destination for searched INSERT,
 *          6 if destination for INSERT from value list,
 *          0 otherwise
 *
 *      insubquery - in
 *          1 if the table cursor is created for a subquery
 *
 * Return value - give :
 *
 *      pointer into the newly created relation cursor
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_relcur_t* tb_relcur_create(cd, trans, tbrelh, forupdate, insubquery)
    void*       cd;
    tb_trans_t* trans;
    tb_relh_t*  tbrelh;
    forupdate_t forupdate;
    bool        insubquery;
{
        tb_relcur_t* cur;

        SS_PMON_ADD_BETA(SS_PMON_RELCUR_CREATE);

        if (rs_sysi_usehurc(cd)) {
            cur = (tb_relcur_t*)tb_hurc_create(
                        cd,
                        trans,
                        tbrelh,
                        forupdate,
                        insubquery,
                        FALSE);
        } else {
            cur = tb_relcur_create_nohurc_ex(
                        cd,
                        trans,
                        tbrelh,
                        NULL,
                        forupdate,
                        insubquery,
                        FALSE);
        }

        return(cur);
}

/*##**********************************************************************\
 *
 *              tb_relcur_create_sql
 *
 * Member of the SQL function block.
 * Creates a relation cursor into a specified relation handle.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle of the transaction to which the cursor
 *          belongs to (NULL if transactions are not in use)
 *
 *      tbrelh - in, hold
 *              pointer to the relation handle
 *
 *      forupdate - in
 *          1 if SELECT ... FOR UPDATE,
 *          2 if searched UPDATE,
 *          3 if searched DELETE,
 *          4 if source for searched INSERT,
 *          5 if destination for searched INSERT,
 *          6 if destination for INSERT from value list,
 *          0 otherwise
 *
 *      insubquery - in
 *          1 if the table cursor is created for a subquery
 *
 * Return value - give :
 *
 *      pointer into the newly created relation cursor
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_relcur_t* tb_relcur_create_sql(cd, trans, tbrelh, forupdate, insubquery)
        rs_sysi_t*  cd;
        tb_trans_t* trans;
        tb_relh_t*  tbrelh;
        forupdate_t forupdate;
        bool        insubquery;
{
        tb_relcur_t* cur;

        ss_pprintf_1(("tb_relcur_create_sql\n"));

        SS_PMON_ADD_BETA(SS_PMON_RELCUR_CREATE);

        if (rs_sysi_usehurc(cd)) {
            cur = (tb_relcur_t*)tb_hurc_create(
                        cd,
                        trans,
                        tbrelh,
                        forupdate,
                        insubquery,
                        TRUE);
        } else {
            cur = tb_relcur_create_nohurc_ex(
                        cd,
                        trans,
                        tbrelh,
                        NULL,
                        forupdate,
                        insubquery,
                        TRUE);
        }

        return(cur);
}

#if defined(SS_BETA)
/*#***********************************************************************\
 *
 *              cur_getslotnum
 *
 *
 *
 * Parameters :
 *
 *      ntuples -
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
static int cur_getslotnum(long ntuples)
{
        int i;

        for (i = 0; (1 << i) < ntuples && i < 19; i++) {
            continue;
        }
        return(i);
}

/*#***********************************************************************\
 *
 *              relcur_updateestinfo
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
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
static void relcur_updateestinfo(
    void*        cd,
    tb_relcur_t* cur)
{
        if (cur->rc_nrows > 0 && cur->rc_est != NULL) {
            rs_estcost_t est_ret_nrows;
            ss_int8_t ntuples_i8, dummy_i8;
            long est_nrows;
            ss_int4_t ntuples;
            int tabid;
            int est_slotno;
            int cur_slotno;

            tb_est_get_n_rows(cd, cur->rc_est, &est_ret_nrows);
            est_nrows = (long)est_ret_nrows;

            ntuples_i8 = rs_relh_ntuples(cd, cur->rc_relh);

            SsInt8SetInt4(&dummy_i8, SS_INT4_MAX);
            if (SsInt8Cmp(ntuples_i8, dummy_i8) > 0) {
                ntuples_i8 = dummy_i8;
            }
            SsInt8ConvertToInt4(&ntuples, ntuples_i8);

            if (ntuples <= 500) {
                tabid = 0;
            } else if (ntuples <= 10000) {
                tabid = 1;
            } else if (ntuples <= 100000) {
                tabid = 2;
            } else {
                tabid = 3;
            }
            est_slotno = cur_getslotnum((long)est_nrows);
            cur_slotno = cur_getslotnum(cur->rc_nrows);

            switch (tabid) {
                case 0:
                    tb_relcur_estnrows_0[est_slotno][0]++;
                    tb_relcur_estnrows_0[est_slotno][cur_slotno+1]++;
                    break;
                case 1:
                    tb_relcur_estnrows_1[est_slotno][0]++;
                    tb_relcur_estnrows_1[est_slotno][cur_slotno+1]++;
                    break;
                case 2:
                    tb_relcur_estnrows_2[est_slotno][0]++;
                    tb_relcur_estnrows_2[est_slotno][cur_slotno+1]++;
                    break;
                case 3:
                    tb_relcur_estnrows_3[est_slotno][0]++;
                    tb_relcur_estnrows_3[est_slotno][cur_slotno+1]++;
                    break;
                default:
                    ss_error;
            }

            if (tb_relc_estinfolist != NULL) {
                tb_relc_estinfo_t* estinfo;
                estinfo = SSMEM_NEW(tb_relc_estinfo_t);
                estinfo->ep_curnrows = cur->rc_nrows;
                estinfo->ep_estnrows = est_nrows;
                estinfo->ep_tablenrows = ntuples;
                if (relc_estinfooutfp != NULL) {
                    (*relc_estinfooutfp)(
                        rs_relh_name(cd, cur->rc_relh),
                        estinfo);
                }
                rs_sysi_rslinksem_enter(cd);
                if (su_list_length(tb_relc_estinfolist) > RELCUR_MAXESTPERF) {
                    su_list_removefirst(tb_relc_estinfolist);
                }
                su_list_insertlast(tb_relc_estinfolist, estinfo);
                rs_sysi_rslinksem_exit(cd);
            }
        }
        cur->rc_nrows = 0;
}

#endif /* SS_BETA */

/*#***********************************************************************\
 *
 *              free_constr_list
 *
 * Releases a constraint list.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      list - in, take
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
static void free_constr_list(rs_sysi_t* cd, su_list_t* list)
{
        su_list_node_t* listnode;
        rs_cons_t* cons;

        ss_dprintf_3(("free_constr_list\n"));

        su_list_do_get(list, listnode, cons) {
             rs_cons_done(cd, cons);
        }
        su_list_done(list);
}


/*#***********************************************************************\
 *
 *              free_ob_list
 *
 * Releases order by list.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      list - in, take
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
static void free_ob_list(rs_sysi_t* cd, su_list_t* list)
{
        su_list_node_t* listnode;
        rs_ob_t* ob;

        su_list_do_get(list, listnode, ob) {
             rs_ob_done(cd, ob);
        }
        su_list_done(list);
}

static void cur_free_estimateandplan(
        void*        cd,
        tb_relcur_t* cur)
{
        if (cur->rc_est != NULL) {
            tb_est_free_estimate(cd, cur->rc_est);
            cur->rc_est = NULL;
        }
        if (cur->rc_plan != NULL) {
            rs_pla_done(cd, cur->rc_plan);
            cur->rc_plan = NULL;
        }
}

/*#***********************************************************************\
 *
 *              tb_relcur_physfree
 *
 * Does a physical free to a cursor meaning all memory is released.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cur - in, take
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
static void tb_relcur_physfree(
    void*        cd,
    tb_relcur_t* cur)
{
        rs_vbuf_done(cd, cur->rc_vbuf);

#ifndef SS_MYSQL
        if (cur->rc_sih != NULL) {
            tb_relh_syncinsert_done(cur->rc_sih);
            cur->rc_sih = NULL;
        }
#endif /* !SS_MYSQL */

        ss_beta(relcur_updateestinfo(cd, cur));

        if (cur->rc_trigctx != NULL) {
            rs_sysi_trigdone(cd, cur->rc_trigctx);
            cur->rc_trigctx = NULL;
        }
        relcur_update_free(cd, cur);

        if (cur->rc_pseudo_l != NULL) {
            su_list_done(cur->rc_pseudo_l);
        }
        if (cur->rc_errh != NULL) {
            rs_error_free(cd, cur->rc_errh);
            cur->rc_errh = NULL;
        }
        free_constr_list(cd, cur->rc_constr_l);
        free_ob_list(cd, cur->rc_orderby_l);

        cur->rc_state = CS_CLOSED;

        cur_tval_free(cd, cur);

        if (cur->rc_sellist != NULL) {
            ss_dassert(cur->rc_selttype != NULL);
            SsMemFree(cur->rc_sellist);
        }

        if (cur->rc_dbcur != NULL) {
            dbe_cursor_done(cur->rc_dbcur, tb_trans_dbtrx(cd, cur->rc_trans));
        }

        cur_free_estimateandplan(cd, cur);
        if (cur->rc_plankey != NULL) {
            rs_key_done(cd, cur->rc_plankey);
        }

        if (su_pa_nelems(cur->rc_updaval_pa) > 0) {
            relcur_updaval_clear(cd, cur);
        }
        su_pa_done(cur->rc_updaval_pa);

        if (cur->rc_infostr != NULL) {
            SsMemFree(cur->rc_infostr);
        }
        if (cur->rc_aggrcount) {
            rs_tval_free(cd, cur->rc_aggrttype, cur->rc_aggrtval);
            rs_ttype_free(cd, cur->rc_aggrttype);
        }
        if (cur->rc_indexhintkey != NULL) {
            rs_key_done(cd, cur->rc_indexhintkey);
        }

        if (cur->rc_casc_states != NULL) {
            uint i;
            tb_trans_keyaction_state_t *state;
            su_pa_do_get(cur->rc_casc_states, i, state) {
                tb_ref_keyaction_free(cd, &state);
            }
            su_pa_done(cur->rc_casc_states);
        }

        SsMemFree(cur);
}

/*##**********************************************************************\
 *
 *              tb_relcur_free
 *
 * Member of the SQL function block.
 * Releases a relation cursor
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in, take
 *              relation cursor pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_relcur_free(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        if (cur->rc_ishurc) {
            tb_hurc_free(cd, TB_CUR_CAST_HURC(cur));
            return;
        }

        CHK_RELCUR(cur);
        ss_pprintf_1(("tb_relcur_free:%ld\n", cur->rc_curid));

#ifdef TB_RELCUR_CACHE
        {
            su_list_t* list;

            /* Put the cursor to the cache. */
            list = rs_sysi_gettbcurlist(cd);
            if (list != NULL) {
                while (su_list_length(list) >= 5) {
                    tb_relcur_physfree(cd, su_list_removelast(list));
                }
                su_list_insertfirst(list, cur);
                return;
            }
        }
#endif /* TB_RELCUR_CACHE */

        tb_relcur_physfree(cd, cur);
}

/*##**********************************************************************\
 *
 *              tb_relcur_reset
 *
 * The tb_relcur_reset function returns a table cursor into the state
 * where it was after tb_relcur_create call (except tb_relcur_project
 * tb_relcur_orderby and tb_relcur_setoptcount settings are preserved).
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cur - in out, use
 *
 *      resetconstr - in
 *          If 1, the tb_relcur_constr and tb_relcur_vectorconstr
 *          conditions are removed.
 *
 *          If 0, the constraints are preserved (with the constant values
 *          being possibly changed). Also, tb_relcur_endofconstr does not
 *          have to be called before tb_relcur_open.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_relcur_reset(
    void*        cd,
    tb_relcur_t* cur,
        bool         resetconstr)
{
        if (cur->rc_ishurc) {
            tb_hurc_reset(cd, TB_CUR_CAST_HURC(cur), resetconstr);
            return;
        }

        CHK_RELCUR(cur);
        ss_pprintf_1(("tb_relcur_reset:%ld, resetconstr=%d\n", cur->rc_curid, resetconstr));
        ss_debug(cur->rc_isresetp = TRUE;)
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_reset");

        cur->rc_state = CS_CLOSED;
        cur->rc_waitlock = FALSE;
        cur->rc_longcons = FALSE;
        cur->rc_setcons = TRUE;
        cur->rc_prevnextp = 2; /* This will cause the vbuf to be reset in
                                  nextorprev. */

        cur->rc_plan_reset = TRUE;
        cur_tval_freeif(cd, cur);

        /* FAKE_INFO_ASSERT(FAKE_TAB_RESET_MUST_BE_FULL, resetconstr); */
        FAKE_CODE_BLOCK(
                FAKE_TAB_RESET_MUST_BE_FULL,
                {
                    SsPrintf("FAKE_TAB_RESET_MUST_BE_FULL: %s\n",
                             rs_relh_name(cd, cur->rc_relh));
                    ss_assert(resetconstr);
                });

        if (resetconstr) {
            SS_PMON_ADD_BETA(SS_PMON_RELCUR_RESETFULL);

            cur->rc_errcode = SU_SUCCESS;
            cur->rc_fatalerrcode = SU_SUCCESS;
            cur->rc_resetinternal = FALSE;
            cur->rc_resetexternal = TRUE;
            cur->rc_isvectorconstr = FALSE;

            ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));

            free_constr_list(cd, cur->rc_constr_l);
            cur->rc_constr_l = su_list_init(NULL);

            if (cur->rc_orderby_l != NULL) {
                free_ob_list(cd, cur->rc_orderby_l);
            }
            cur->rc_orderby_l = su_list_init(NULL);

            cur_free_estimateandplan(cd, cur);
            if (cur->rc_aggrcount) {
                rs_tval_free(cd, cur->rc_aggrttype, cur->rc_aggrtval);
                rs_ttype_free(cd, cur->rc_aggrttype);
                cur->rc_aggrcount = FALSE;
            }

        } else {
            SS_PMON_ADD_BETA(SS_PMON_RELCUR_RESETSIMPLE);
            cur->rc_resetexternal = FALSE;
            cur->rc_errcode = cur->rc_fatalerrcode;
            ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));
        }
}

/*##**********************************************************************\
 *
 *              tb_relcur_disableinfo
 *
 * Disables SQL Info printing from this cursor. Used with internal system
 * cursors from where the output is not needed.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
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
void tb_relcur_disableinfo(
    void*        cd __attribute__ ((unused)),
    tb_relcur_t* cur)
{
        ss_dprintf_1(("tb_relcur_disableinfo:%ld\n", cur->rc_curid));
        CHK_RELCUR(cur);

        cur->rc_infolevel = 0;
}

/*##**********************************************************************\
 *
 *              tb_relcur_forcegroup
 *
 * Forces statement grouping for the cursor.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
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
void tb_relcur_forcegroup(
    void*        cd __attribute__ ((unused)),
    tb_relcur_t* cur)
{
        ss_dprintf_1(("tb_relcur_forcegroup:%ld\n", cur->rc_curid));
        CHK_RELCUR(cur);

        cur->rc_isstmtgroup = TRUE;
}

/*##**********************************************************************\
 *
 *              tb_relcur_project
 *
 * Member of the SQL function block.
 * Specifies the set of attributes in a relation that will be used
 * to construct the result tuples.
 *
 * If the function is not called, all the attrs in the rel cursor
 * will be shown in the result tuples.
 *
 * Must be called before calling tb_relcur_open.
 * The function may only be called once for a given cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      sellist - in, use
 *              (-1)-terminated array of attribute numbers.
 *          Attribute numbers reference to the base table type.
 *          A private copy of the array will be made.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_relcur_project(cd, cur, sellist)
    void*        cd;
    tb_relcur_t* cur;
    int*         sellist;
{
        uint cnt = 0;
        uint sum = 0;
        rs_ttype_t* relh_ttype;
        uint i;
        int sysidano = RS_ANO_NULL;

        if (cur->rc_ishurc) {
            tb_hurc_project(cd, TB_CUR_CAST_HURC(cur), sellist);
            return;
        }

        CHK_RELCUR(cur);
        ss_pprintf_1(("tb_relcur_project:%ld\n", cur->rc_curid));
        ss_dassert(!cur->rc_isresetp);
        ss_output_1(
            {
                char* buf = SsMemAlloc(2000);
                char* p;
                int i;
                p = buf;
                buf[0] = '\0';
                for (i = 0; sellist[i] != -1; i++) {
                    SsSprintf(p, "%d ", sellist[i]);
                    p += strlen(p);
                }
                SsDbgPrintfFun1("%s\n", buf);
                SsMemFree(buf);
            }
        )
        /* ss_dassert(cur->rc_sellist == NULL); */
        /* ss_dassert(cur->rc_pseudo_l == NULL); */

        ss_dassert(cur->rc_state == CS_CLOSED);
        ss_dassert(cur->rc_seltvalue == NULL);

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);
        cur->rc_projectall = FALSE;
        ss_dassert(cur->rc_selttype == relh_ttype);

        if (rs_relh_rowcheckcolname(cd, cur->rc_relh) != NULL) {
            sysidano = rs_ttype_anobyname(
                            cd,
                            cur->rc_selttype,
                            rs_relh_rowcheckcolname(cd, cur->rc_relh));
            ss_dassert(sysidano != RS_ANO_NULL);
        }

        /* find the size of the list */
        cnt = 0;
        sum = 0;
        while (sellist[cnt] != RS_ANO_NULL) {
            sum += sellist[cnt];
            cnt++;
            if (sellist[cnt] == sysidano) {
                /* Already selected. */
                sysidano = RS_ANO_NULL;
            }
        }

        if (sysidano != RS_ANO_NULL) {
            /* Include space for extra column. */
            cnt++;
        }

        if (cur->rc_sellist != NULL) {
            SsMemFree(cur->rc_sellist);
        }
        if (cur->rc_pseudo_l != NULL) {
            su_list_done(cur->rc_pseudo_l);
            cur->rc_pseudo_l = NULL;
        }

        cur->rc_sellist = SsMemAlloc((cnt + 1) * sizeof(cur->rc_sellist[0]));

        ss_dprintf_2(("tb_relcur_project:%ld, nitems in sellist=%u\n", cur->rc_curid, cnt));

        /* Convert the sql-numbered attribute numbers to physical
         * and store them to cur->rc_sellist.
         */
        for (i = 0; sellist[i] != RS_ANO_NULL; i++) {
            rs_ano_t    phys_ano;
            char*       aname;
            rs_atype_t* atype;

            phys_ano = rs_ttype_sqlanotophys(cd, relh_ttype, sellist[i]);
            ss_dassert(phys_ano != RS_ANO_NULL);
            aname = rs_ttype_aname(cd, relh_ttype, phys_ano);
            atype = rs_ttype_atype(cd, relh_ttype, phys_ano);

            ss_dprintf_3(("tb_relcur_project:%ld adding attr '%s' to seltype\n", cur->rc_curid, aname));

            if (rs_atype_pseudo(cd, atype)) {
                int ano;
                cur_pseudoattr_project(cd, cur, relh_ttype, phys_ano, &ano);
                cur->rc_sellist[i] = ano;

            } else if (rs_atype_isuserdefined(cd, atype)) {

                cur->rc_sellist[i] = phys_ano;

            } else {

                /* Ignore other attributes. */
                cur->rc_sellist[i] = RS_ANO_PSEUDO;
            }
        }
        if (sysidano != RS_ANO_NULL) {
            cur->rc_sellist[i++] = sysidano;
        }
        cur->rc_sellist[i] = RS_ANO_NULL;
}


static rs_aval_t* relcur_truncate_constr(
    void*        cd __attribute__ ((unused)),
    uint         relop __attribute__ ((unused)),
    rs_atype_t*  atype,
    rs_aval_t*   aval,
    int          len)
{
        bool succp;
    rs_atype_t*  res_atype;
    rs_aval_t*   res_aval;
        rs_datatype_t datatype;
        rs_sqldatatype_t sqldt = 0;

        ss_dprintf_3(("relcur_truncate_constr\n"));
        SS_PUSHNAME("relcur_truncate_constr");
        datatype = rs_atype_datatype(cd, atype);
        if (datatype == RSDT_CHAR) {
            sqldt = RSSQLDT_VARCHAR;
        } else if (datatype == RSDT_UNICODE) {
            sqldt = RSSQLDT_WVARCHAR;
            len = len/(sizeof(ss_char2_t));
        } else {
            ss_error;
        }

        res_atype = rs_atype_initbysqldt(cd, sqldt, len-2, -1);
        res_aval = rs_aval_copy(cd, atype, aval);

        succp = rs_aval_trimchar(cd, res_atype, res_aval, TRUE);
        /* ss_dassert(succp); */

        rs_atype_free(cd, res_atype);
        SS_POPNAME;
        return(res_aval);
}


/*#***********************************************************************\
 *
 *              relcur_constr_checktruncation
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      org_atype -
 *
 *
 *      atype -
 *
 *
 *      aval -
 *
 *
 *      p_atype - out, give
 *
 *
 *      p_aval - out, give
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
static bool relcur_constr_checktruncation(
    void*        cd,
    tb_relcur_t* cur,
        uint         relop,
    rs_atype_t*  org_atype,
    rs_atype_t*  atype,
    rs_aval_t*   aval,
    rs_aval_t**  p_aval)
{
        rs_datatype_t dt;
        bool ok_no_truncation = TRUE;
        size_t datasize = sizeof(ss_byte_t);

        if (p_aval != NULL) {
            *p_aval = aval;
        }
        dt = rs_atype_datatype(cd, atype);

        switch (dt) {
            case RSDT_INTEGER:
            case RSDT_FLOAT:
            case RSDT_DOUBLE:
            case RSDT_DATE:
            case RSDT_DFLOAT:
            case RSDT_BINARY:
            case RSDT_BIGINT:
                break; /* TODO: check these */
            case RSDT_UNICODE:
                datasize = sizeof(ss_char2_t);
                /* FALLTHROUGH */
            case RSDT_CHAR:
                {
                    rs_sqldatatype_t org_sqldt;
                    va_t* c_va;

                    org_sqldt = rs_atype_sqldatatype(cd, org_atype);
                    switch (org_sqldt) {
                        case RSSQLDT_WCHAR:
                        case RSSQLDT_CHAR:
                            {
                                rs_datatype_t org_dt;
                                rs_atype_t* dummy_atype;
                                rs_sqldatatype_t dummy_sqldt;

                                org_dt = rs_atype_datatype(cd, org_atype);
                                if (dt != org_dt) {
                                    if (dt == RSDT_UNICODE) {
                                        dummy_sqldt = RSSQLDT_WCHAR;
                                    } else {
                                        dummy_sqldt = RSSQLDT_CHAR;
                                    }
                                    dummy_atype =
                                        rs_atype_initbysqldt(
                                            cd,
                                            dummy_sqldt,
                                            rs_atype_length(cd, atype),
                                            -1L);
                                    ok_no_truncation =
                                        rs_aval_trimchar(cd, dummy_atype, aval, FALSE);
                                    rs_atype_free(cd, dummy_atype);
                                } else {
                                    ok_no_truncation =
                                        rs_aval_trimchar(cd, org_atype, aval, FALSE);
                                }
                                ss_dassert(ok_no_truncation);
                                /* ok_no_truncation can never be FALSE,
                                * because the last parameter to rs_aval_trimchar
                                * is FALSE
                                */
                            }
                            break;
                        default:
                            break;
                    }
                    if (rs_aval_isblob(
                                cd,
                                atype,
                                aval)){
                        cur->rc_errcode = E_TOOLONGCONSTR;
                        return(FALSE);
                    }
                    c_va = rs_aval_va(cd, atype, aval);
                    if ((ulong)(va_netlen(c_va) / datasize) > (ulong)RS_KEY_MAXCMPLEN) {

                        /* here we are sensitive for long constraints.
                         * also confiquration for the option should be
                         * checked here. TB_MAXCMPLEN should be replaced
                         * (or defined) with feature configuration parameter.
                         */

                        tb_database_t* tdb = rs_sysi_tabdb(cd);
                        int tb_maxcmplen = tb_getmaxcmplen(tdb);

                        ulong type_length = rs_atype_length(cd, org_atype);
                        if (p_aval == NULL || type_length > (ulong)tb_maxcmplen) {
                            /* user defined attribute length is exceed */
                            cur->rc_errcode = E_TOOLONGCONSTR;
                            return(FALSE);
                        }
                        if ((ulong)(va_netlen(c_va) / datasize) > (ulong)tb_maxcmplen) {
                            /* configuration limit is exceeded */
                            cur->rc_errcode = E_TOOLONGCONSTR;
                            return(FALSE);
                        }
                        /* here we create truncated constr for dbe */

                        *p_aval = relcur_truncate_constr(
                                    cd,
                                    relop,
                                    atype,
                                    aval,
                                    RS_KEY_MAXCMPLEN);

                        /* in this case truncation should be succesfull */
                        ss_dassert(*p_aval != NULL);
                    }

                    break;
                }
            default:
                break;
        }

        return(TRUE);
}

static int constr_getescchar(
        void*         cd,
        rs_sqlcons_t* sqlcons)
{
        int c_escchar = RS_CONS_NOESCCHAR;

        if (sqlcons->sc_escatype != NULL) {
            rs_datatype_t dt = rs_atype_datatype(cd, sqlcons->sc_escatype);

            ss_rc_assert(dt == RSDT_CHAR || dt == RSDT_UNICODE, dt);

            if (rs_aval_isnull(cd, sqlcons->sc_escatype, sqlcons->sc_escaval)) {
                c_escchar = 0;
            } else {
                switch (dt) {
                    case RSDT_CHAR:
                        c_escchar = (int)*rs_aval_getasciiz(
                                            cd,
                                            sqlcons->sc_escatype,
                                            sqlcons->sc_escaval);
                        break;
                    case RSDT_UNICODE: {
                        size_t totlen;
                        ss_char2_t wcsbuf[2];
                        RS_AVALRET_T avalret;

                        wcsbuf[0] = (ss_char2_t)0;
                        avalret =
                            rs_aval_converttowcs(
                                cd,
                                sqlcons->sc_escatype,
                                sqlcons->sc_escaval,
                                wcsbuf,
                                2,
                                0,
                                &totlen,
                                NULL);
                        ss_rc_dassert(avalret == RSAVR_SUCCESS ||
                                        avalret == RSAVR_TRUNCATION,
                                        avalret);
                        c_escchar = (int)wcsbuf[0];
                        break;
                    }
                    default:
                        c_escchar = 0;
                        break;
                }
            }
        }

        return c_escchar;
}

static void relcur_constr_ex(
        void*         cd,
        tb_relcur_t*  cur,
        su_bflag_t    flags,
        rs_sqlcons_t* sqlcons,
        rs_cons_t*    cons)
{
        uint        c_relop    = sqlcons->sc_relop;
        uint        c_aindex;
        rs_atype_t* c_atype    = NULL;
        rs_aval_t*  c_aval     = NULL;
        int         c_escchar  = RS_CONS_NOESCCHAR;
        rs_atype_t* col_atype;
        rs_ttype_t* relh_ttype;
        bool        addtoconslist = (cons == NULL);
        bool        nullrelop;
        rs_atype_t* atype = sqlcons->sc_atype;
        rs_aval_t*  aval = sqlcons->sc_aval;

        CHK_RELCUR(cur);
        SS_PUSHNAME("relcur_constr_ex");

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);
        c_aindex = rs_ttype_sqlanotophys(cd, relh_ttype, sqlcons->sc_attrn);
        col_atype = rs_ttype_atype(cd, relh_ttype, c_aindex);
        nullrelop = (c_relop == RS_RELOP_ISNULL || c_relop == RS_RELOP_ISNOTNULL);

        ss_output_3(
        {
            char* buf;
            buf = (aval != NULL)
                    ? rs_aval_print(cd, atype, aval)
                    : SsMemStrdup((char *)"<NULL>");
            ss_dprintf_3(("relcur_constr_ex:%ld:col %s (%d)), relop %s (%d), value %s, *unknownvalue=%d, aval=%ld, flags=%d\n",
                cur->rc_curid, rs_ttype_aname(cd, relh_ttype, c_aindex), sqlcons->sc_attrn,
                rs_cons_relopname(c_relop), c_relop,
                buf,
                nullrelop ? FALSE : rs_aval_isunknown(cd, sqlcons->sc_atype, sqlcons->sc_aval),
                (long)aval,
                flags));
            SsMemFree(buf);
        }
        );
        ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));

        if (!nullrelop) {
            if (!rs_aval_isunknown(cd, sqlcons->sc_atype, sqlcons->sc_aval)) {
                bool succp;
                succp =
                    relcur_constr_checktruncation(
                            cd,
                            cur,
                            c_relop,
                            col_atype,
                            atype,
                            aval,
                            &c_aval);
                if (!succp) {
                    SS_POPNAME;
                    return;
                }
            } else {
                cur->rc_isunknownconstr = TRUE;
            }
            c_atype = atype;
            if (c_aval == NULL) {
                c_aval = aval;
            }
            if (c_relop == RS_RELOP_LIKE) {
                c_escchar = constr_getescchar(cd, sqlcons);
            }
        }

        ss_dassert(c_relop == sqlcons->sc_relop);

        if (rs_atype_pseudo(cd, col_atype)) {
            char* aname;

            ss_dprintf_4(("relcur_constr_ex:%ld:pseudo\n"));
            ss_dassert(cons == NULL);
            ss_dassert(c_relop == sqlcons->sc_relop);

            if (c_aval != aval) {
                /* Function relcur_constr_checktruncation changed constraint
                 * value.
                 */
                ss_dprintf_4(("relcur_constr_ex:%ld:error, c_aval != aval\n"));
                cur->rc_errcode = E_TOOLONGCONSTR;
                if (c_aval != NULL) {
                    rs_aval_free(cd, c_atype, c_aval);
                }
                SS_POPNAME;
                return;
            }

            cur->rc_resetinternal = TRUE;
            ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));

            aname = rs_ttype_aname(cd, relh_ttype, c_aindex);

            if (strcmp(aname, RS_PNAME_ROWID) == 0) {
                cur_rowid_setcons(
                    cd,
                    cur,
                    flags,
                    sqlcons,
                    c_escchar);
            } else if (strcmp(aname, RS_PNAME_ROWVER) == 0) {
                cur_rowver_setcons(
                    cd,
                    cur,
                    flags,
                    sqlcons,
                    c_escchar);
            } else if (strcmp(aname, RS_PNAME_SYNCTUPLEVERS) == 0) {
                bool succp;
                succp = cur_synctuplevers_setcons(
                            cd,
                            cur,
                            flags,
                            sqlcons,
                            c_escchar);
                if (!succp) {
                    cur->rc_errcode = E_ILLPSEUDOCOLRELOP;
                    cur->rc_fatalerrcode = cur->rc_errcode;
                }
            } else if (strcmp(aname, RS_PNAME_SYNC_ISPUBLTUPLE) == 0) {
                bool succp;
                succp = cur_sync_ispubltuple_setcons(
                            cd,
                            cur,
                            flags,
                            sqlcons,
                            c_escchar);
                if (!succp) {
                    cur->rc_errcode = E_ILLPSEUDOCOLRELOP;
                    cur->rc_fatalerrcode = cur->rc_errcode;
                }
            } else {
                /* also RS_PNAME_ROWFLAGS handled here */
                cur->rc_errcode = E_ILLPSEUDOCOLRELOP;
                cur->rc_fatalerrcode = cur->rc_errcode;
            }
            SS_POPNAME;
            return;
        }

        if (c_aval != aval) {
#ifdef SS_MYSQL
            ss_error;
#else /* SS_MYSQL */
            char padc[2];
            /* Here we translate user constraint for dbe.
             * see comments olso in function relcur_constr_checktruncation.
             * Note that dbe-constraints are translated if c_aval!=aval:
             * relcur_constr_checktruncation has created it!
             */

            ss_dprintf_4(("relcur_constr_ex:%ld:long cons\n"));
            cur->rc_resetinternal = TRUE;
            cur->rc_longcons = TRUE;

            ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));

            padc[0] = 0;
            padc[1] = 0;

            ss_dassert(c_relop == sqlcons->sc_relop);

            switch (c_relop) {
                case RS_RELOP_EQUAL:
                    ss_dprintf_4(("relcur_constr_ex:%ld:long cons, equal -> like\n"));
                    c_relop = RS_RELOP_LIKE;
                    padc[0] = '%';
                    break;
                case RS_RELOP_LT:
                case RS_RELOP_LE:
                    ss_dprintf_4(("relcur_constr_ex:%ld:long cons, lt or le -> le\n"));
                    c_relop = RS_RELOP_LE;
                    padc[0] = '\xFF';
                    break;
                case RS_RELOP_GT:
                    ss_dprintf_4(("relcur_constr_ex:%ld:long cons, gt -> ge\n"));
                    c_relop = RS_RELOP_GE;
                    break;
                case RS_RELOP_LIKE:
                    padc[0] = '%';
                    /* FALLTHROUGH */
                default:
                    break;
            }

            if (padc[0] != 0) {
                bool succp;
                int arop = RS_AROP_PLUS;
                rs_datatype_t datatype = rs_atype_datatype(cd, c_atype);
                rs_sqldatatype_t sqldt;
                int len;
                rs_atype_t* pad_atype;
                rs_aval_t*  pad_aval;
                rs_atype_t* res_atype = NULL;
                rs_aval_t*  res_aval = NULL;

                SS_PUSHNAME("relcur_constr_ex:pad");
                if (datatype == RSDT_CHAR) {
                    sqldt = RSSQLDT_LONGVARCHAR;
                    len = 1;
                    pad_atype = rs_atype_initbysqldt(cd, sqldt, len, -1);
                    pad_aval = rs_aval_create(cd, pad_atype);
                    succp = rs_aval_set8bitstr_ext(cd, pad_atype, pad_aval, padc, NULL);
                    ss_dassert(succp);
                } else if (datatype == RSDT_UNICODE) {
                    ss_char2_t wpad[2];
                    wpad[0] = padc[0];
                    wpad[1] = padc[1];
                    sqldt = RSSQLDT_WLONGVARCHAR;
                    len = 1;
                    pad_atype = rs_atype_initbysqldt(cd, sqldt, len, -1);
                    pad_aval = rs_aval_create(cd, pad_atype);
                    succp = rs_aval_setwcs_ext(cd, pad_atype, pad_aval, wpad, NULL);
                    ss_dassert(succp);
                } else {
                    ss_rc_error(datatype);
                }
                SS_POPNAME;
                SS_PUSHNAME("relcur_constr_ex:rs_aval_arith");
                succp = rs_aval_arith(
                            cd,
                            &res_atype,
                            &res_aval,
                            c_atype,
                            c_aval,
                            pad_atype,
                            pad_aval,
                            arop,
                            NULL);
                ss_dassert(succp);
                SS_POPNAME;
                rs_aval_free(cd, pad_atype, pad_aval);
                rs_atype_free(cd, pad_atype);

                if (cons == NULL) {
                    ss_dassert(addtoconslist);
                    cons = rs_cons_init(
                            cd,
                            c_relop,
                            c_aindex,
                            res_atype,
                            res_aval,
                            flags | RS_CONSFLAG_FORCECONVERT,
                            sqlcons,
                            c_escchar,
                            col_atype,
                            &cur->rc_errh);
                } else {
                    ss_dassert(!addtoconslist);
                    ss_dassert(rs_cons_ano(cd, cons) == c_aindex);
                    rs_cons_reset(
                            cd,
                            cons,
                            c_relop,
                            c_aindex,
                            res_atype,
                            res_aval,
                            flags | RS_CONSFLAG_FORCECONVERT,
                            sqlcons,
                            c_escchar,
                            col_atype,
                            &cur->rc_errh);
                }
                rs_aval_free(cd, res_atype, res_aval);
                rs_atype_free(cd, res_atype);
            } else {
                if (cons == NULL) {
                    ss_dassert(addtoconslist);
                    cons = rs_cons_init(cd,
                                        c_relop,
                                        c_aindex,
                                        c_atype,
                                        c_aval,
                                        flags | RS_CONSFLAG_FORCECONVERT,
                                        sqlcons,
                                        c_escchar,
                                        col_atype,
                                        &cur->rc_errh);
                } else {
                    ss_dassert(!addtoconslist);
                    ss_dassert(rs_cons_ano(cd, cons) == c_aindex);
                    rs_cons_reset(
                            cd,
                            cons,
                            c_relop,
                            c_aindex,
                            c_atype,
                            c_aval,
                            flags | RS_CONSFLAG_FORCECONVERT,
                            sqlcons,
                            c_escchar,
                            col_atype,
                            &cur->rc_errh);
                }
            }
#endif /* SS_MYSQL */
        } else {
            if (cons == NULL) {
                ss_dassert(addtoconslist);
                cons = rs_cons_init(cd,
                                    c_relop,
                                    c_aindex,
                                    c_atype,
                                    c_aval,
                                    flags,
                                    sqlcons,
                                    c_escchar,
                                    col_atype,
                                    &cur->rc_errh);
            } else {
                ss_dassert(!cur->rc_resetexternal);
                ss_dassert(!cur->rc_resetinternal);
                ss_dassert(!addtoconslist);
                ss_dassert(rs_cons_relop(cd, cons) == c_relop);
                ss_dassert(rs_cons_ano(cd, cons) == (rs_ano_t)c_aindex);
            }
        }

        ss_dassert(cur->rc_constr_l);
        rs_cons_setsolved(cd, cons, FALSE);
        if (addtoconslist) {
            if (!rs_cons_value_aliased(cd, cons)) {
                ss_dassert(c_atype == atype);
                cur->rc_resetinternal = TRUE;
            } else {
                SS_PMON_ADD_BETA(SS_PMON_RELCUR_CONSTRISSIMPLE);
            }
            su_list_insertlast(cur->rc_constr_l, cons);
            ss_dassert(su_list_length(cur->rc_constr_l) < 1000);
        }
        if (c_aval != aval && c_aval != NULL) {
            rs_aval_free(cd, c_atype, c_aval);
        }
        SS_POPNAME;
}

static void relcur_resetconstr(
        void*        cd,
        tb_relcur_t* cur)
{
        su_list_t* conslist;
        su_list_node_t* n;
        rs_cons_t* cons;
        bool free_conslist;

        ss_dprintf_3(("relcur_resetconstr\n"));
        CHK_RELCUR(cur);
        ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));

        if (cur->rc_resetexternal) {
            /* New cons set by SQL. */
            ss_pprintf_4(("relcur_resetconstr:rc_resetexternal\n"));
            /* Next time try to reuse cons. */
            cur->rc_resetexternal = FALSE;
            ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));
            return;
        }

        conslist = cur->rc_constr_l;
        free_conslist = FALSE;

        if (cur->rc_resetinternal) {
            /* Recreate the constraint list and create a new estimate and
             * plan.
             */
            ss_pprintf_4(("relcur_resetconstr:rc_resetinternal\n"));

            /* By default assume we do not need full reset in the next time.
             * Function relcur_constr_ex sets higher value if needed.
             */
            cur->rc_resetinternal = FALSE;
            free_conslist = TRUE;

            ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));

            cur->rc_constr_l = su_list_init(NULL);
            /* Previous constraints changed relops, we can not use
             * old est.
             */
            cur_free_estimateandplan(cd, cur);
        }

        if (cur->rc_plan != NULL) {
            /* Set plan consistent before checking the constraints.
             */
            rs_pla_setconsistent_once(cd, cur->rc_plan, TRUE);
        }

        /* Give cons to the subsystem. It checks e.g. long cons.
         */
        su_list_do_get(conslist, n, cons) {
            rs_sqlcons_t* sqlcons;

            ss_dassert(su_list_length(conslist) < 1000);
            ss_dassert(su_list_length(cur->rc_constr_l) < 1000);

            sqlcons = rs_cons_getsqlcons(cd, cons);

            ss_dassert(sqlcons->sc_relop == RS_RELOP_ISNULL ||
                       sqlcons->sc_relop == RS_RELOP_ISNOTNULL ||
                       !rs_aval_isunknown(cd, sqlcons->sc_atype, sqlcons->sc_aval));

            ss_dprintf_4(("sqlcons->sc_alias=%d, sqlcons->sc_tabcons=%d\n", sqlcons->sc_alias, sqlcons->sc_tabcons));

            if (!sqlcons->sc_alias) {
                relcur_constr_ex(
                        cd,
                        cur,
                        sqlcons->sc_tabcons
                            ? RS_CONSFLAG_COPYSQLCONS | RS_CONSFLAG_FORCECONVERT
                            : 0,
                        sqlcons,
                        free_conslist ? NULL : cons);
            }
            if (cur->rc_plan != NULL) {
                if (rs_cons_isalwaysfalse_once(cd, cons)) {
                    rs_pla_setconsistent_once(cd, cur->rc_plan, FALSE);
                }
            }
        }
        ss_dassert(n == NULL);
        ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));
        if (cur->rc_resetinternal && cur->rc_est != NULL) {
            /* New constraints changed relops, we can not use
             * old est.
             */
            cur_free_estimateandplan(cd, cur);
        }
        if (free_conslist) {
            free_constr_list(cd, conslist);
        }
}


/*##**********************************************************************\
 *
 *              tb_relcur_constr
 *
 * Member of the SQL function block.
 * Adds a search constraint into a relation cursor.
 * The search constraint is of the form:
 *
 *  (value of column <col_n>) <relop> <value>
 *
 * In case of RS_RELOP_ISNULL or RS_RELOP_ISNOTNULL, the constraint
 * is of the form
 *
 *  (value of column <col_n>) IS (NOT) NULL
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      attr_n - in
 *              number of the attr on which the constraint acts
 *          Attribute number references to the base table type.
 *
 *      relop - in
 *              RS_RELOP_EQUAL, RS_RELOP_NOTEQUAL,
 *          RS_RELOP_LT, RS_RELOP_GT, RS_RELOP_LE,
 *          RS_RELOP_GE, RS_RELOP_LIKE, RS_RELOP_ISNULL
 *          or RS_RELOP_ISNOTNULL
 *
 *      atype - in, hold
 *              type of the value. Not used in case of
 *          RS_RELOP_ISNULL and RS_RELOP_ISNOTNULL
 *
 *      aval - in, hold
 *              the attribute value. Not used in case of
 *          RS_RELOP_ISNULL and RS_RELOP_ISNOTNULL.
 *          Value NULL means "some value" (this is used
 *          only to find out estimates on the cursor
 *          execution. In this case, the function
 *          relcur_open will never be called).
 *
 *      escatype - in, use
 *          used only if relop is RS_RELOP_LIKE. The type
 *          of the escape character (NULL if no escape
 *          character)
 *
 *      escaval - in, use
 *              used only if relop is RS_RELOP_LIKE. The
 *          value containing the escape character (NULL
 *          if no escape character)
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_relcur_constr(
        void*        cd,
        tb_relcur_t* cur,
        uint         attr_n,
        uint         relop,
        rs_atype_t*  atype,
        rs_aval_t*   aval,
        rs_atype_t*  escatype,
        rs_aval_t*   escaval)
{
        rs_sqlcons_t sqlcons;

        if (cur->rc_ishurc) {
            tb_hurc_constr(cd, (tb_hurc_t*)cur, attr_n, relop, atype, aval, escatype, escaval);
            return;
        }

        CHK_RELCUR(cur);
        ss_dassert(cur->rc_resetexternal);
        ss_pprintf_1(("tb_relcur_constr:%ld:col %s, relop %d\n",
            cur->rc_curid,
            rs_ttype_aname(
                cd,
                rs_relh_ttype(
                    cd,
                    cur->rc_relh),
                rs_ttype_sqlanotophys(
                    cd,
                    rs_relh_ttype(
                        cd,
                        cur->rc_relh),
                    attr_n)),
            relop));
        SS_PMON_ADD_BETA(SS_PMON_RELCUR_CONSTR);

        sqlcons.sc_relop = relop;
        sqlcons.sc_attrn = attr_n;
        sqlcons.sc_atype = atype;
        sqlcons.sc_aval = aval;
        sqlcons.sc_escatype = escatype;
        sqlcons.sc_escaval = escaval;
        sqlcons.sc_alias = FALSE;
        sqlcons.sc_tabcons = FALSE;

        relcur_constr_ex(
                cd,
                cur,
                0,
                &sqlcons,
                NULL);  /* cons */
}

void tb_relcur_tabconstr(cd, cur, attr_n, relop, atype, aval, escatype, escaval)
    void*        cd;
    tb_relcur_t* cur;
    uint         attr_n;
    uint         relop;
    rs_atype_t*  atype;
    rs_aval_t*   aval;
    rs_atype_t*  escatype;
    rs_aval_t*   escaval;
{
        rs_sqlcons_t sqlcons;

        if (cur->rc_ishurc) {
            ss_derror;
            tb_hurc_constr(cd, (tb_hurc_t*)cur, attr_n, relop, atype, aval, escatype, escaval);
            return;
        }

        CHK_RELCUR(cur);
        ss_dassert(cur->rc_resetexternal);
        ss_pprintf_1(("tb_relcur_tabconstr:%ld:col %s, relop %d\n",
            cur->rc_curid,
            rs_ttype_aname(
                cd,
                rs_relh_ttype(
                    cd,
                    cur->rc_relh),
                rs_ttype_sqlanotophys(
                    cd,
                    rs_relh_ttype(
                        cd,
                        cur->rc_relh),
                    attr_n)),
            relop));
        SS_PMON_ADD_BETA(SS_PMON_RELCUR_CONSTR);

        sqlcons.sc_relop = relop;
        sqlcons.sc_attrn = attr_n;
        sqlcons.sc_atype = atype;
        sqlcons.sc_aval = aval;
        sqlcons.sc_escatype = escatype;
        sqlcons.sc_escaval = escaval;
        sqlcons.sc_alias = FALSE;
        sqlcons.sc_tabcons = TRUE;

        relcur_constr_ex(
                cd,
                cur,
                RS_CONSFLAG_COPYSQLCONS | RS_CONSFLAG_FORCECONVERT,
                &sqlcons,
                NULL);  /* cons */

        /* With cons coming from table level always use full reset.
         */
        cur->rc_resetexternal = TRUE;

        ss_dprintf_4(("cur->rc_resetexternal=%d, cur->rc_resetinternal=%d\n", cur->rc_resetexternal, cur->rc_resetinternal));
}

/*##**********************************************************************\
 *
 *              tb_relcur_vectorconstr
 *
 * The tb_relcur_vectorconstr adds a vector search constraint into a table
 * cursor. The search constraint is of the form:
 *
 *      (<col_n>, ... , <col_n>) <relop> <value, ... , value>
 *
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      n - in
 *              number of items in the vectors
 *
 *      col_ns - in
 *              array of numbers of the column on which the
 *          constraint acts
 *
 *      relop - in
 *          SQL_RELOP_NOTEQUAL, SQL_RELOP_LT, SQL_RELOP_GT,
 *          SQL_RELOP_LE, SQL_RELOP_GE, SQL_RELOP_ISNULL or
 *          SQL_RELOP_ISNOTNULL
 *          (note: not SQL_RELOP_EQUAL, SQL_RELOP_ISNULL,
 *          SQL_RELOP_LIKE or SQL_RELOP_ISNOTNULL)
 *
 *      atypes - in
 *              array of types of the values
 *
 *      avals - in
 *              array of the value instances
 *
 * Return value :
 *
 *      1 if the constraint is accepted,
 *      0 in the case that vector constraints are not supported
 *        in the data management system (in which case the call
 *        has no effect)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_relcur_vectorconstr(
        void*           cd,
        tb_relcur_t*    cur,
        uint            n,
        uint*           col_ns,
        uint            relop,
        rs_atype_t**    atypes,
        rs_aval_t**     avals)
{
        uint i;
        rs_sqlcons_t sqlcons;
        rs_ttype_t* relh_ttype;
        uint        c_aindex;
        rs_atype_t* c_atype;
        rs_aval_t*  c_aval;
        rs_atype_t* col_atype;
        rs_cons_t*  cons;
        uint        c_relop __attribute__ ((unused));

        c_relop = relop;


        if (cur->rc_ishurc) {
            return(tb_hurc_vectorconstr(
                        cd,
                        TB_CUR_CAST_HURC(cur),
                        n,
                        col_ns,
                        relop,
                        atypes,
                        avals));
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_vectorconstr:%ld\n", cur->rc_curid));
        ss_dassert(cur->rc_resetexternal);

        if (!rs_sqli_usevectorconstr) {
            return(FALSE);
        }

        ss_dassert(n > 0);
        ss_dassert(col_ns != NULL);
        ss_dassert(atypes != NULL);
        ss_dassert(avals != NULL);

        if (cur->rc_isvectorconstr) {
            /* Accept only one vector constraint. */
            return(FALSE);
        }

        switch (relop) {
            case SQL_RELOP_GT:
                relop = RS_RELOP_GT_VECTOR;
                break;
            case SQL_RELOP_GE:
                relop = RS_RELOP_GE_VECTOR;
                break;
            case SQL_RELOP_LT:
                relop = RS_RELOP_LT_VECTOR;
                break;
            case SQL_RELOP_LE:
                relop = RS_RELOP_LE_VECTOR;
                break;
            default:
                return(FALSE);
        }

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);

        for (i = 0; i < n; i++) {
            c_aindex = rs_ttype_sqlanotophys(cd, relh_ttype, col_ns[i]);
        col_atype = rs_ttype_atype(cd, relh_ttype, c_aindex);
            if (rs_atype_pseudo(cd, col_atype)) {
                cur->rc_errcode = E_ILLPSEUDOCOLRELOP;
                cur->rc_fatalerrcode = cur->rc_errcode;
                return(FALSE);
            }
            c_atype = atypes[i];
            c_aval = avals[i];
            if (c_atype != NULL && c_aval != NULL) {
                /* For long than 256 byte scearch this should be verified also:
                 * Is this SA-specific only?
                 */
                if (!relcur_constr_checktruncation(cd, cur, relop, col_atype, c_atype, avals[i], NULL)) {
                    return(FALSE);
                }
            }

            sqlcons.sc_relop = relop;
            sqlcons.sc_attrn = c_aindex;
            sqlcons.sc_atype = c_atype;
            sqlcons.sc_aval = c_aval;
            sqlcons.sc_escatype = NULL;
            sqlcons.sc_escaval = NULL;
            sqlcons.sc_alias = FALSE;
            sqlcons.sc_tabcons = FALSE;

            cons = rs_cons_init(
                        cd,
                        relop,
                        c_aindex,
                        c_atype,
                        c_aval,
                        0,
                        &sqlcons,
                        RS_CONS_NOESCCHAR,
                        col_atype,
                        &cur->rc_errh);
            if (rs_cons_isalwaysfalse(cd, cons)) {
                rs_cons_done(cd, cons);
                return(FALSE);
            }
            rs_cons_setsolved(cd, cons, FALSE);
            rs_cons_setvectorno(cd, cons, i);

            ss_dassert(cur->rc_constr_l);
            su_list_insertlast(cur->rc_constr_l, cons);
            ss_dassert(su_list_length(cur->rc_constr_l) < 1000);

            cur->rc_isvectorconstr = TRUE;
        }

        return(FALSE);
}

/*##**********************************************************************\
 *
 *              tb_relcur_orderby
 *
 * Member of the SQL function block.
 * Tries to have the result rows of a cursor ordered by a column value.
 * Successive calls to the function create an ordering criteria where
 * the first criterion is most significant.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      attr_n - in
 *              number of the attribute to use as ordering criteria
 *          Attribute number references to the base table type.
 *
 *      asc - in
 *              TRUE if ascending, FALSE if descending
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_relcur_orderby(cd, cur, attr_n, asc)
    void*        cd;
    tb_relcur_t* cur;
    uint         attr_n;
    bool         asc;
{
        rs_ob_t* ob;
        rs_ano_t ano;

        if (cur->rc_ishurc) {
            tb_hurc_orderby(cd, TB_CUR_CAST_HURC(cur), attr_n, asc);
            return;
        }

        CHK_RELCUR(cur);

    ano = rs_ttype_sqlanotophys(
                cd,
                rs_relh_ttype(cd, cur->rc_relh),
                attr_n);

        ss_dprintf_1(("tb_relcur_orderby:%ld:col %s, asc %d\n",
            cur->rc_curid,
            rs_ttype_aname(cd, cur->rc_selttype, ano),
            asc));

#if 0
        /* Ensure that the attribute is not already once in orderbylist */
        ss_debug_1({
            su_list_node_t* ln = su_list_first(cur->rc_orderby_l);
            while (ln != NULL) {
                rs_ob_t* o = (rs_ob_t *)(ln->ln_data);
                uint ai = rs_ob_ano(cd, o);
                ss_dassert(ai != ano);
                ln = su_list_next(cur->rc_orderby_l, ln);
            }
        });
#endif

        ob = rs_ob_init(cd, ano, asc);
        rs_ob_setsolved(cd, ob, FALSE);

        su_list_insertlast(cur->rc_orderby_l, ob);
}

/*##**********************************************************************\
 *
 *              tb_relcur_setoptcount
 *
 * Member of the SQL function block.
 * Will inform a table cursor for how many
 * result tuples the relation cursor should be optimized for.
 * The value 0 means "optimize for all".
 * When this function is not called, the optimization
 * should be performed for one line.
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      count - in
 *              how many result rows to optimize (0 means "optimize for all")
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_relcur_setoptcount(
        void*        cd __attribute__ ((unused)),
        tb_relcur_t* cur,
        rs_estcost_t count
) {
        if (cur->rc_ishurc) {
            tb_hurc_setoptcount(cd, TB_CUR_CAST_HURC(cur), count);
            return;
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_setoptcount:%ld:count=%d\n", cur->rc_curid, (long)count));

        if (count >= (rs_estcost_t)LONG_MAX) {
            cur->rc_optcount = LONG_MAX;
        } else {
            cur->rc_optcount = (ulong)count;
        }
}

/*##**********************************************************************\
 *
 *              tb_relcur_indexhint
 *
 * Function will inform a table cursor for an optimizer hint on
 * index or full scan usage on the table.
 *
 * Parameters :
 *
 *      cd - in, use
 *              application state
 *
 *      cur - use
 *              pointer into the table cursor
 *
 *      fullscan - in
 *              1 if the FULL SCAN hint was specified
 *
 *      index - in
 *          the name of the index to be advised to be
 *          used by the hint. NULL value means advising
 *          to use primary key on the table. Used only if
 *          fullscan = 0.
 *
 *      reverse - in
 *              1 if the index hint contained the "REVERSE"
 *          keyword. Used only if fullscan = 0.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_relcur_indexhint(
        void*        cd,
        tb_relcur_t* cur,
        bool         fullscan,
        char*        index,
        bool         reverse
) {
        rs_key_t* key;

        if (cur->rc_ishurc) {
            tb_hurc_indexhint(cd, TB_CUR_CAST_HURC(cur), fullscan, index, reverse);
            return;
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_indexhint:%ld:fullscan=%d,index=%s,reverse=%d\n",
            cur->rc_curid, fullscan, index != NULL ? index : "NULL", reverse));
        SS_NOTUSED(cd);

        if (cur->rc_indexhintkey != NULL) {
            rs_key_done(cd, cur->rc_indexhintkey);
            cur->rc_indexhintkey = NULL;
        }

        if (fullscan) {
            /* Use clustering key.
             */
            key = rs_relh_clusterkey(cd, cur->rc_relh);
            ss_bassert(key != NULL);

        } else {
            if (index == NULL) {
                key = rs_relh_primkey(cd, cur->rc_relh);
                if (key == NULL) {
                    cur->rc_errcode = E_NOPRIMKEY_S;
                    cur->rc_fatalerrcode = cur->rc_errcode;
                    rs_error_create(&cur->rc_errh, E_NOPRIMKEY_S, rs_relh_name(cd, cur->rc_relh));
                }
            } else {
                rs_entname_t keyname;
                rs_entname_initbuf(
                        &keyname,
                        NULL,
                        NULL,
                        index);
                key = rs_relh_keybyname(cd, cur->rc_relh, &keyname);
                if (key == NULL) {
                    cur->rc_errcode = E_HINTKEYNOTFOUND_S;
                    cur->rc_fatalerrcode = cur->rc_errcode;
                    rs_error_create(&cur->rc_errh, E_HINTKEYNOTFOUND_S, index);
                }
            }
        }
        if (key != NULL) {
            cur->rc_indexhintkey = key;
            rs_key_link(cd, cur->rc_indexhintkey);
            if (reverse) {
                cur->rc_reverseset = TB_REVERSESET_NORMAL;
            }
        }
}

/*#***********************************************************************\
 * 
 *              relcur_info_print
 * 
 * Adds table name and trailing newline to tb_info_print calls.
 * 
 * Parameters : 
 * 
 *              cd - 
 *                      
 *                      
 *              cur - 
 *                      
 *                      
 *              level - 
 *                      
 *                      
 *              infostr - 
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
static void relcur_info_print(
        rs_sysi_t* cd,
        tb_relcur_t* cur,
        int level,
        const char* infostr)
{
        ss_dassert(strchr(infostr, '\n') == NULL);

        tb_info_print(cd, cur->rc_trans, level, (char *)infostr);
        tb_info_print(cd, cur->rc_trans, level, (char *)" (table ");
        tb_info_print(cd, cur->rc_trans, level, rs_relh_name(cd, cur->rc_relh));
        tb_info_print(cd, cur->rc_trans, level, (char *)")\n");
}

/*#***********************************************************************\
 *
 *              relcur_printestimate
 *
 * Prints estimate info from the table cursor, if info level is not
 * too small.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cur - in
 *
 *
 *      final_estimate - in
 *              If TRUE, this is the final estimate (actual search estimate).
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void relcur_printestimate(
        rs_sysi_t*   cd,
        tb_relcur_t* cur,
        bool         final_estimate)
{
        char buf[255];
        rs_key_t* key;
        rs_estcost_t delay_at_start;
        rs_estcost_t average_delay_per_row;
        int est_type;
        rs_estcost_t n_rows = 0;
        uint norderby;
        bool printinfo;
        bool istrace;

        istrace = su_usrid_istrace(SU_USRID_TRACE_EST);
        printinfo = final_estimate && (cur->rc_infolevel >= 3 || istrace);
        if (!printinfo) {
            return;
        }

        if (cur->rc_infolevel == 3 && !istrace) {
            key = tb_est_get_key(cd, cur->rc_est);
            SsSprintf(buf, "  Table level: using key %s\n", rs_key_name(cd, key));
            tb_info_print(cd, cur->rc_trans, 3, buf);
            return;
        }

        key = tb_est_get_key(cd, cur->rc_est);
        tb_est_get_delays(cd, cur->rc_est, &delay_at_start, &average_delay_per_row);
        est_type = tb_est_get_n_rows(cd, cur->rc_est, &n_rows);
        norderby = tb_est_get_n_order_bys(cd, cur->rc_est);

        if (cur->rc_reverseset == TB_REVERSESET_NORMAL) {
            average_delay_per_row += average_delay_per_row / TB_REVERSESET_ADDCOST;
        }

        if (istrace) {
            su_usrid_trace(rs_sysi_userid(cd), SU_USRID_TRACE_EST|SU_USRID_TRACE_ESTINFO,
                           1, (char *)"Table level estimate");
        }
        if (cur->rc_infolevel) {
            tb_info_print(cd, cur->rc_trans, 4, (char *)"  Table level estimate:\n");
        }

        if (istrace) {
            SsSprintf(buf, "table = %s, key = %s",
                rs_relh_name(cd, cur->rc_relh),
                rs_key_name(cd, key));
            su_usrid_trace(rs_sysi_userid(cd), SU_USRID_TRACE_EST, 1, buf);
        }
        if (cur->rc_infolevel) {
            SsSprintf(buf, "    table = %s, key = %s\n",
                rs_relh_name(cd, cur->rc_relh),
                rs_key_name(cd, key));
            tb_info_print(cd, cur->rc_trans, 4, buf);
        }

        if (istrace) {
            SsSprintf(buf, "delay = %.2lf+%.2lf, count = %.1lf (%s), norderby = %d, distinct = %d",
                (double)delay_at_start,
                (double)average_delay_per_row,
                (double)n_rows,
                est_type == 0 ? "no estimate" :
                est_type == 1 ? "approximate" :
                "exact",
                norderby,
                tb_est_get_unique_value(cd, cur->rc_est));
            su_usrid_trace(rs_sysi_userid(cd), SU_USRID_TRACE_EST, 1, buf);
        }
        if (cur->rc_infolevel) {
            SsSprintf(buf, "    delay = %.2lf+%.2lf, count = %.1lf (%s), norderby = %d, distinct = %d\n",
                (double)delay_at_start,
                (double)average_delay_per_row,
                (double)n_rows,
                est_type == 0 ? "no estimate" :
                est_type == 1 ? "approximate" :
                "exact",
                norderby,
                tb_est_get_unique_value(cd, cur->rc_est));
            tb_info_print(cd, cur->rc_trans, 4, buf);
        }
}

/*#***********************************************************************\
 *
 *              relcur_constrinfo
 *
 * Generates a string containg info about search constraints.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      itemlen -
 *              Item length, or -1.
 *
 *      buf -
 *              Temporary buffer area, at least 255 bytes.
 *
 * Return value :
 *
 *
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* relcur_constrinfo(
        void*        cd,
        tb_relcur_t* cur,
        int          itemlen,
        char*        buf)
{
        rs_cons_t* cons;
        su_list_node_t* listnode;
        rs_ttype_t* ttype;
        char* str;
        char format[50];

        SsSprintf(format, "%%-%ds", itemlen);

        ttype = rs_relh_ttype(cd, cur->rc_relh);

        str = SsMemStrdup((char *)"");

        su_list_do_get(cur->rc_constr_l, listnode, cons) {
            char* aname;
            rs_aval_t* aval;
            char* avalstr;
            const char* relopname;
            bool print_aval;
            int addlen;
            int len;

            print_aval = TRUE;
            switch (rs_cons_relop(cd, cons)) {
                case RS_RELOP_EQUAL:
                    relopname = "=";
                    break;
                case RS_RELOP_NOTEQUAL:
                    relopname = "<>";
                    break;
                case RS_RELOP_LT:
                    relopname = "<";
                    break;
                case RS_RELOP_GT:
                    relopname = ">";
                    break;
                case RS_RELOP_LE:
                    relopname = "<=";
                    break;
                case RS_RELOP_GE:
                    relopname = ">=";
                    break;
                case RS_RELOP_LIKE:
                    relopname = "LIKE";
                    break;
                case RS_RELOP_ISNULL:
                    print_aval = FALSE;
                    relopname = "IS NULL";
                    break;
                case RS_RELOP_ISNOTNULL:
                    print_aval = FALSE;
                    relopname = "IS NOT NULL";
                    break;
                case RS_RELOP_LT_VECTOR:
                    relopname = "VECTOR(<)";
                    break;
                case RS_RELOP_GT_VECTOR:
                    relopname = "VECTOR(>)";
                    break;
                case RS_RELOP_LE_VECTOR:
                    relopname = "VECTOR(<=)";
                    break;
                case RS_RELOP_GE_VECTOR:
                    relopname = "VECTOR(>=)";
                    break;
                default:
                    relopname = "ERROR";
                    break;
            }
            aname = rs_ttype_aname(cd, ttype, rs_cons_ano(cd, cons));
            aval = rs_cons_aval(cd, cons);
            if (!print_aval) {
                avalstr = SsMemStrdup((char *)"");
            } else if (aval == NULL) {
                avalstr = SsMemStrdup((char *)"...");
            } else {
                avalstr = rs_aval_print(cd, rs_cons_atype(cd, cons), aval);
            }

            SsSprintf(buf, "%.80s %.40s %.80s ", aname, relopname, avalstr);
            if (rs_cons_isalwaysfalse(cd, cons)) {
                strcat(buf, "(always false) ");
            }

            addlen = strlen(buf);
            if (addlen < itemlen) {
                addlen = itemlen;
            }

            len = strlen(str);
            str = SsMemRealloc(str, len + addlen + 1);
            if (itemlen == -1) {
                strcpy(str + len, buf);
            } else {
                SsSprintf(str + len, format, buf);
            }

            SsMemFree(avalstr);
        }
        return(str);
}

/*#***********************************************************************\
 *
 *              relcur_printconstr
 *
 * Prints constr info from the table cursor, if info level is not
 * too small.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cur - in
 *
 *
 *      final - in
 *              If TRUE, this is the actual search case.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void relcur_printconstr(
        void*        cd,
        tb_relcur_t* cur,
        bool         final)
{
        bool printinfo;
        char buf[255];

        printinfo = final && cur->rc_infolevel >= 6;
        if (!printinfo) {
            return;
        }

        tb_info_print(cd, cur->rc_trans, 6, (char *)"  Table level constr: ");

        if (su_list_length(cur->rc_constr_l) == 0) {
            tb_info_print(cd, cur->rc_trans, 6, (char *)" no contraints specified");
        } else {
            char* constrinfo;
            constrinfo = relcur_constrinfo(cd, cur, -1, buf);
            tb_info_print(cd, cur->rc_trans, 6, constrinfo);
            SsMemFree(constrinfo);
        }

        tb_info_print(cd, cur->rc_trans, 6, (char *)"\n");
}

/*#***********************************************************************\
 *
 *              relcur_printproject
 *
 * Prints select list info from the table cursor, if info level is not
 * too small.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cur - in
 *
 *
 *      final - in
 *              If TRUE, this is the actual search case.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void relcur_printproject(
        void*        cd,
        tb_relcur_t* cur,
        bool         final)
{
        int i;
        rs_ttype_t* ttype;
        bool printinfo;

        printinfo = final && cur->rc_infolevel >= 6;
        if (!printinfo) {
            return;
        }

        tb_info_print(cd, cur->rc_trans, 6, (char *)"  Table level project: ");

        ttype = cur->rc_selttype;

        if (cur->rc_sellist[0] == RS_ANO_NULL) {
            tb_info_print(cd, cur->rc_trans, 6, (char *)"*");
        }

        for (i = 0; cur->rc_sellist[i] != RS_ANO_NULL; i++) {
            if (cur->rc_sellist[i] != RS_ANO_PSEUDO) {
                tb_info_print(cd, cur->rc_trans, 6, rs_ttype_aname(cd, ttype, cur->rc_sellist[i]));
                tb_info_print(cd, cur->rc_trans, 6, (char *)" ");
            }
        }

        tb_info_print(cd, cur->rc_trans, 6, (char *)"\n");
}

/*#***********************************************************************\
 *
 *              relcur_printorderby
 *
 * Prints order by list info from the table cursor, if info level is not
 * too small.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cur - in
 *
 *
 *      final - in
 *              If TRUE, this is the actual search case.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void relcur_printorderby(
        void*        cd,
        tb_relcur_t* cur,
        bool         final)
{
        rs_ttype_t* ttype;
        rs_ob_t* ob;
        su_list_node_t* listnode;
        bool printinfo;

        printinfo = final && cur->rc_infolevel >= 6;
        if (!printinfo) {
            return;
        }

        tb_info_print(cd, cur->rc_trans, 6, (char *)"  Table level orderby: ");

        ttype = rs_relh_ttype(cd, cur->rc_relh);

        if (su_list_length(cur->rc_orderby_l) == 0) {
            tb_info_print(cd, cur->rc_trans, 6, (char *)"no ordering specified");
        }

        su_list_do_get(cur->rc_orderby_l, listnode, ob) {
            tb_info_print(
                cd,
                cur->rc_trans,
                0,
                rs_ttype_aname(cd, ttype, rs_ob_ano(cd, ob)));
            if (rs_ob_asc(cd, ob)) {
                tb_info_print(cd, cur->rc_trans, 6, (char *)" ASC ");
            } else {
                tb_info_print(cd, cur->rc_trans, 6, (char *)" DESC ");
            }
        }

        tb_info_print(cd, cur->rc_trans, 6, (char *)"\n");
}

/*##**********************************************************************\
 *
 *              tb_relcur_info
 *
 * The tb_relcur_info function returns an information string describing
 * the current state of a table cursor. Typical contents describe search
 * constraints and key selections. There are no limitations to the length
 * of the return string.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 * Return value :
 *
 *      pointer into information string in a private structure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* tb_relcur_info(
        void*        cd,
        tb_relcur_t* cur)
{
        rs_key_t* key;
        const char* infoformat;
        char* constrinfo;
        char buf[255];
        char format[50];
        int itemlen = 50;

        if (cur->rc_ishurc) {
            return(tb_hurc_info(cd, TB_CUR_CAST_HURC(cur)));
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_info:%ld\n", cur->rc_curid));

        SsSprintf(format, "%%-%ds", itemlen);

        if (cur->rc_est == NULL) {
            return((char *)"");
        }

        if (cur->rc_infostr != NULL) {
            SsMemFree(cur->rc_infostr);
        }

        key = tb_est_get_key(cd, cur->rc_est);
        if (tb_est_get_full_scan(cd, cur->rc_est)) {
            if (cur->rc_reverseset) {
                if (rs_key_isclustering(cd, key)) {
                    infoformat = "SCAN TABLE (REVERSE)";
                } else {
                    infoformat = "SCAN %.80s (REVERSE)";
                }
            } else {
                if (rs_key_isclustering(cd, key)) {
                    infoformat = "SCAN TABLE";
                } else {
                    infoformat = "SCAN %.80s";
                }
            }
        } else if (rs_key_isprimary(cd, key)) {
            if (cur->rc_reverseset) {
                infoformat = "PRIMARY KEY (REVERSE)";
            } else {
                infoformat = "PRIMARY KEY";
            }
        } else {
            if (tb_est_get_must_retrieve(cd, cur->rc_est)) {
                if (cur->rc_reverseset) {
                    infoformat = "INDEX %.80s (REVERSE)";
                } else {
                    infoformat = "INDEX %.80s";
                }
            } else {
                if (cur->rc_reverseset) {
                    infoformat = "INDEX ONLY %.80s (REVERSE)";
                } else {
                    infoformat = "INDEX ONLY %.80s";
                }
            }
        }

        constrinfo = relcur_constrinfo(cd, cur, itemlen, buf);

        cur->rc_infostr = SsMemAlloc(strlen(infoformat) + 80 +
                                     strlen(constrinfo) + 1);

        SsSprintf(
            buf,
            infoformat,
            rs_key_name(cd, key));

        SsSprintf(cur->rc_infostr, format, buf);

        strcat(cur->rc_infostr, constrinfo);

        SsMemFree(constrinfo);

        return(cur->rc_infostr);
}

/*#***********************************************************************\
 *
 *              cur_checkupdatepriv
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tbrelh -
 *
 *
 *      nattrs -
 *
 *
 *      upd_attrs -
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
static bool cur_checkupdatepriv(
        void* cd,
        tb_relh_t* tbrelh,
        uint nattrs,
        bool* upd_attrs)
{
        uint i;
        tb_relpriv_t* relpriv;

        /* Maybe this check should be optimized.
         * F.x. stored in tbrelh if check is already done
         */
        rs_relh_t* rs_relh;

        if (tbrelh == NULL) {
            return(TRUE);
        }


        rs_relh = tb_relh_rsrelh(cd, tbrelh);

        if (rs_relh_ishistorytable(cd, rs_relh)) {
            rs_sysi_t* owner_cd = NULL;
            bool foundp;
            char* catalog;
            long catalogid;

            catalog = rs_relh_catalog(cd, rs_relh);

            foundp = tb_schema_find_catalog_mode(cd, catalog, &catalogid, &owner_cd, NULL, NULL);
            ss_dassert(foundp);
            if (cd == owner_cd) {
                return(TRUE);
            }
        }

        if (tb_relh_ispriv(cd, tbrelh, TB_PRIV_UPDATE)) {
            return(TRUE);
        }

        relpriv = tb_relh_priv(cd, tbrelh);

        if (!tb_priv_issomeattrpriv(cd, relpriv)) {
            return(FALSE);
        }

        for (i = 0; i < nattrs; i++) {
            if (upd_attrs[i] &&
                !tb_priv_isattrpriv(cd, relpriv, i, TB_PRIV_UPDATE)) {
                return(FALSE);
            }
        }
        return(TRUE);
}

/*#***********************************************************************\
 *
 *              cur_checkdeletepriv
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tbrelh -
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
static bool cur_checkdeletepriv(
        void* cd,
        tb_relh_t* tbrelh)
{
        /* Maybe this check should be optimized.
         * F.x. stored in tbrelh if check is already done
         */
        rs_relh_t* rs_relh;

        if (tbrelh == NULL) {
            return(TRUE);
        }

        rs_relh = tb_relh_rsrelh(cd, tbrelh);

        if (rs_relh_ishistorytable(cd, rs_relh)) {
            rs_sysi_t* owner_cd = NULL;
            bool foundp;
            char* catalog;
            long catalogid;

            catalog = rs_relh_catalog(cd, rs_relh);

            foundp = tb_schema_find_catalog_mode(cd, catalog, &catalogid, &owner_cd, NULL, NULL);
            ss_dassert(foundp);
            if (cd == owner_cd) {
                return(TRUE);
            }
        }

        return(tb_relh_ispriv(cd, tbrelh, TB_PRIV_DELETE));
}

/*##**********************************************************************\
 *
 *              tb_relcur_endofconstr
 *
 * Member of the SQL function block.
 * Called after the tb_relcur_create and possible tb_relcur_constr,
 * tb_relcur_orderby, tb_relcur_project and tb_relcur_setoptcount calls to
 * indicate that no more constraints will be imposed to the cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_endofconstr(
        void*        cd,
        tb_relcur_t* cur,
        rs_err_t**   p_errh
) {
        bool succp = TRUE;

        if (cur->rc_ishurc) {
            return(tb_hurc_endofconstr(cd, TB_CUR_CAST_HURC(cur), p_errh));
        }

        CHK_RELCUR(cur);
        ss_pprintf_1(("tb_relcur_endofconstr:%ld\n", cur->rc_curid));
        SS_PUSHNAME("tb_relcur_endofconstr");

        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_endofconstr");

        switch (cur->rc_curtype) {
            case DBE_CURSOR_UPDATE:
                ss_dprintf_2(("tb_relcur_endofconstr:%ld:DBE_CURSOR_UPDATE\n", cur->rc_curid));
                if (su_list_length(cur->rc_constr_l) > 0 &&
                    cur->rc_tbrelh != NULL &&
                    !tb_relh_ispriv(cd, cur->rc_tbrelh, TB_PRIV_SELECT)) {
                    /* Searched update needs select privileges.
                     * NIST v.6.0 TEST:0474, file sdl034.sql.
                     */
                    succp = FALSE;
                } else if (cur->rc_tbrelh != NULL
                           && !cur_checkupdatepriv(cd, cur->rc_tbrelh, 0, NULL)) 
                {
                    /* No update privileges for table. */
                    succp = FALSE;
                } else {
                    /* Update privilege depends on updated columns, so we
                     * cannot check the update privilege before the actual
                     * update operation.
                     */
                    succp = TRUE;
                }
                break;
            case DBE_CURSOR_DELETE:
                ss_dprintf_2(("tb_relcur_endofconstr:%ld:DBE_CURSOR_DELETE\n", cur->rc_curid));
                if (su_list_length(cur->rc_constr_l) > 0 &&
                    cur->rc_tbrelh != NULL &&
                    !tb_relh_ispriv(cd, cur->rc_tbrelh, TB_PRIV_SELECT)) {
                    /* Searched delete needs select privileges.
                     * NIST v.6.0 TEST:0474, file sdl034.sql.
                     */
                    succp = FALSE;
                } else {
                    succp = cur_checkdeletepriv(cd, cur->rc_tbrelh);
                }
                break;
            case DBE_CURSOR_FORUPDATE:
            case DBE_CURSOR_SELECT:
            case DBE_CURSOR_SASELECT:
                ss_dprintf_2(("tb_relcur_endofconstr:%ld:normal select\n", cur->rc_curid));
                succp = cur->rc_tbrelh == NULL || 
                        tb_relh_ispriv(cd, cur->rc_tbrelh, TB_PRIV_SELECT);
                break;
            default:
                ss_error;
        }

        if (!succp) {
            ss_dprintf_2(("tb_relcur_endofconstr:%ld:E_NOPRIV\n", cur->rc_curid));
            rs_error_create(p_errh, E_NOPRIV);
            SS_POPNAME;
            return(FALSE);
        }

        if (cur->rc_errh != NULL) {
            /* A previously detected error */
            ss_dprintf_2(("tb_relcur_endofconstr:%ld:cur->rc_errh != NULL\n", cur->rc_curid));
            if (p_errh != NULL) {
                *p_errh = cur->rc_errh;
                cur->rc_errh = NULL;
            }
            SS_POPNAME;
            return (FALSE);
        }
        if (cur->rc_errcode != SU_SUCCESS) {
            ss_dprintf_2(("tb_relcur_endofconstr:%ld:errcode=%d (%s)\n", cur->rc_curid, cur->rc_errcode, su_rc_nameof(cur->rc_errcode)));
            rs_error_create(p_errh, cur->rc_errcode);
            SS_POPNAME;
            return(FALSE);
        }

        if (cur->rc_projectall) {
            /* Create project where all attributes visible
               to the SQL are selected. This is equal to the SELECT *
               statement in SQL.
            */
            ss_dprintf_2(("tb_relcur_endofconstr:%ld:project all\n", cur->rc_curid));
            cur_sql_project_all(cd, cur, cur->rc_relh);
        }

        if (cur->rc_setcons) {
            relcur_resetconstr(cd, cur);
            cur->rc_setcons = FALSE;
        }

        succp = cur_ensure_estimate(cd, cur);
        /* SIGFPE */
        if (!succp) {
            ss_debug(cur->rc_lastcall = (char *)"tb_relcur_endofconstr:cur_ensure_estimate failed");
            ss_dassert(cur->rc_errh != NULL);
            if (p_errh != NULL) {
                *p_errh = cur->rc_errh;
                cur->rc_errh = NULL;
            }
            SS_POPNAME;
            return(FALSE);
        }

        if (cur->rc_infolevel >= 3) {
            relcur_info_print(cd, cur, 3, "  Table level: final estimate, using this plan");
        }
        relcur_printproject(cd, cur, TRUE);
        relcur_printconstr(cd, cur, TRUE);
        relcur_printorderby(cd, cur, TRUE);
        relcur_printestimate(cd, cur, TRUE);

        cur->rc_state = CS_CLOSED;
        ss_dassert(tb_est_check(cur->rc_est));
        SS_POPNAME;
        return(TRUE);
}

/*##**********************************************************************\
 *
 *              tb_relcur_estcount
 *
 * Member of the SQL function block.
 * Returns information of how many lines are resulting from a table cursor
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in, use
 *              relation cursor pointer
 *
 *      p_count - out
 *              if either the exact number of resulting
 *          lines or an estimate is known, the
 *          number of lines is stored in *p_count
 *
 * Return value :
 *
 *      0 if the information is not available
 *      1 if an estimate about the number of lines
 *        is known
 *      2 if the exact number of lines is known
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_relcur_estcount(cd, cur, p_count)
    void*         cd;
    tb_relcur_t*  cur;
    rs_estcost_t* p_count;
{
        uint rc;

        if (cur->rc_ishurc) {
            return(tb_hurc_estcount(cd, TB_CUR_CAST_HURC(cur), p_count));
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_estcount:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);

        if (cur_ensure_estimate(cd, cur)) {
            rc = tb_est_get_n_rows(cd, cur->rc_est, p_count);
            ss_dprintf_2(("tb_relcur_estcount:%ld, returns %u, count=%.1lf\n",
                    cur->rc_curid, rc, (double)*p_count));
            return(rc);
        } else {
            return(0);
        }
}

/*##**********************************************************************\
 *
 *              tb_relcur_estdelay
 *
 * Member of the SQL function block.
 * Returns information of the delay resulting from retrieving all
 * the lines from a table cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in, use
 *              relation cursor pointer
 *
 *      p_c0, p_c1 - out
 *          if an estimate of microseconds consumed
 *          c0 + c1 * n is available, the estimate
 *          constants c0 and c1 are stored in *p_c0 and *p_c1
 *
 * Return value :
 *
 *      TRUE if the estimate is available
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_estdelay(cd, cur, p_c0, p_c1)
    void*         cd;
    tb_relcur_t*  cur;
    rs_estcost_t* p_c0;
    rs_estcost_t* p_c1;
{
        if (cur->rc_ishurc) {
            return(tb_hurc_estdelay(cd, TB_CUR_CAST_HURC(cur), p_c0, p_c1));
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_estdelay:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);

        if (cur_ensure_estimate(cd, cur)) {
            tb_est_get_delays(cd, cur->rc_est, p_c0, p_c1);
            ss_dprintf_2(("tb_relcur_estdelay:%ld, returns p_c0=%.2lf, p_c1=%.2lf\n",
                        cur->rc_curid, (double)*p_c0, (double)*p_c1));
            if (cur->rc_reverseset == TB_REVERSESET_NORMAL) {
                *p_c1 += *p_c1 / TB_REVERSESET_ADDCOST;
                ss_dprintf_2(("tb_relcur_estdelay:%ld, reverseset: new p_c1=%.2lf\n",
                            cur->rc_curid, (double)*p_c1));
            }
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              tb_relcur_estcolset
 *
 * Member of the SQL function block.
 * The tb_relcur_estcolset function returns information of how many
 * different result combinations are resulting from a set of columns
 * in a table cursor.
 *
 * The function may called only after calling the tb_relcur_endofconstr
 * function.
 *
 * Parameters :
 *
 *      cd - in
 *          application state
 *
 *      cur - use
 *          pointer into the table cursor
 *
 *      n - in
 *          number of columns in the column set (at least 1)
 *
 *      cols - in
 *          array of the index numbers of the columns (first column is zero)
 *
 *      p_count - in
 *          if either the exact number of resulting lines or an estimate
 *          is known, the number of lines is stored in *p_count
 *
 *
 * Return value :
 *
 *      0 - if the information is not available
 *      1 - if an estimate about the number of lines is known
 *      2 - if the exact number of lines is known
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
uint tb_relcur_estcolset(
    void*         cd,
    tb_relcur_t*  cur,
        uint          n,
        uint*         cols,
        rs_estcost_t* p_count)
{
        if (cur->rc_ishurc) {
            return(tb_hurc_estcolset(
                        cd,
                        TB_CUR_CAST_HURC(cur),
                        n,
                        cols,
                        p_count));
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_estcolset:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);

        if (cur_ensure_estimate(cd, cur)) {
            *p_count = tb_est_getdiffrowcount(
                            cd,
                            cur->rc_est,
                            cur->rc_relh,
                            n,
                            cols);
            if (cur->rc_infolevel >= 4) {
                uint i;
                char buf[255];
                rs_ttype_t* ttype;
                ttype = rs_relh_ttype(cd, cur->rc_relh);
                SsSprintf(buf, "  Table level: group by estimate %.1lf rows (cols:", (double)*p_count);
                tb_info_print(cd, cur->rc_trans, 4, buf);
                for (i = 0; i < n; i++) {
                    SsSprintf(buf, "%s%s",
                        rs_ttype_sql_aname(cd, ttype, cols[i]),
                        i < n-1 ? "," : "");
                    tb_info_print(cd, cur->rc_trans, 4, buf);
                }
                tb_info_print(cd, cur->rc_trans, 4, (char *)")\n");
            }
            return(1);

        } else {
            return(0);
        }
}

/*##**********************************************************************\
 *
 *              tb_relcur_ordered
 *
 * Member of the SQL function block.
 * Checks how many of the ordering criteria specified with the
 * relcur_orderby calls will a relation cursor be able to obey.
 *
 * The function may called only after calling the relcur_endconstr
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in, use
 *              relation cursor pointer
 *
 *      p_nullcoll - out
 *          pointer to variable where NULL collation info
 *          will be stored
 *
 * Return value :
 *
 *      number of the ordering criteria that will be obeyed
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_relcur_ordered(cd, cur, p_nullcoll)
    void*        cd;
    tb_relcur_t* cur;
        uint*        p_nullcoll;
{
        uint n_ob;
        bool succp;

        if (cur->rc_ishurc) {
            return(tb_hurc_ordered(cd, TB_CUR_CAST_HURC(cur), p_nullcoll));
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_ordered:%ld\n", cur->rc_curid));
        ss_dassert(p_nullcoll != NULL);
        *p_nullcoll = SQL_NULLCOLL_LOW;
        if (su_list_length(cur->rc_orderby_l) == 0) {
            return(0);
        }
        succp = cur_ensure_estimate(cd, cur);
        /* SIGFPE */
        if (!succp) {
            return(0);
        }
        n_ob = tb_est_get_n_order_bys(cd, cur->rc_est);
        ss_dprintf_2(("tb_relcur_ordered:%ld, returns %u\n", cur->rc_curid, n_ob));
        return(n_ob);
}

/*##**********************************************************************\
 *
 *              tb_relcur_tabcurunique
 *
 * The sql_dm_tabcurunique function checks if the results of a table
 * cursor (considering only the columns that are selected with
 * tabcurproject) are known to be unique i.e. have no exact duplicates.
 *
 * The function may called only after calling the endconstr
 * function.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
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
bool tb_relcur_tabcurunique(
        void*        cd,
        tb_relcur_t* cur)
{
        bool succp;
        bool uniquep;

        if (cur->rc_ishurc) {
            return(tb_hurc_tabcurunique(cd, TB_CUR_CAST_HURC(cur)));
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_tabcurunique:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);

        succp = cur_ensure_estimate(cd, cur);
        /* SIGFPE */
        if (!succp) {
            return(0);
        }

        uniquep = tb_est_get_unique_value(cd, cur->rc_est);

        ss_dprintf_2(("tb_relcur_tabcurunique:%ld:returns %u\n", cur->rc_curid, uniquep));

        return(uniquep);
}

/*##**********************************************************************\
 *
 *              tb_relcur_open
 *
 * Member of the SQL function block.
 * Called to move a table cursor into a state where relcur_next and _prev
 * may be made. The cursor will be initially in the begin state.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_open(cd, cur, p_errh)
    void*        cd;
    tb_relcur_t* cur;
        rs_err_t**   p_errh;
{
        bool succp;

        if (cur->rc_ishurc) {
            succp = tb_hurc_open(cd, TB_CUR_CAST_HURC(cur), p_errh);
            return(succp);
        }

        CHK_RELCUR(cur);
        ss_pprintf_1(("tb_relcur_open:%ld\n", cur->rc_curid));
        SS_PMON_ADD_BETA(SS_PMON_RELCUROPEN);
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_open");

        ss_dassert(cur->rc_state == CS_CLOSED);

        if (cur->rc_errcode == SU_SUCCESS && cur->rc_setcons) {
            relcur_resetconstr(cd, cur);
        }
        cur->rc_setcons = TRUE;

        if (cur->rc_errcode != SU_SUCCESS) {
            ss_dprintf_2(("tb_relcur_open:%ld:errcode=%d (%s)\n", cur->rc_curid, cur->rc_errcode, su_rc_nameof(cur->rc_errcode)));
            rs_error_create(p_errh, cur->rc_errcode);
            return(FALSE);
        }

        if (cur->rc_isunknownconstr && cur->rc_uselateindexplan) {
            ss_dprintf_2(("tb_relcur_open:%ld:isuknownconstr, reset est and plan\n", cur->rc_curid));
            cur_free_estimateandplan(cd, cur);
        }

        succp = cur_reset(cd, cur);
        ss_dassert(succp);

        if (cur->rc_reverseset) {
            succp = tb_relcur_begin(cd, cur);
            ss_dassert(succp);
        }
        cur_ensure_estimate(cd, cur);

        return(TRUE);
}

void tb_relcur_tabopen(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        tb_relcur_open(cd, cur, NULL);
}

/*#***********************************************************************\
 *
 *              relcur_count
 *
 * The relcur_count function will return the total count of the
 * rows in the result set of a table cursor. After the function call,
 * the table cursor will remain in undefined state (one of begin, end or
 * row state).
 *
 * The function may called only after calling the tb_relcur_open
 * function.
 *
 * Parameters :
 *
 *      cd - in
 *              client data
 *
 *      cur - use
 *              relation cursor pointer
 *
 *      p_count - out
 *          Total number of rows in the table cursor if the operation
 *          terminated with TB_FETCH_SUCC. Undefined otherwise.
 *
 * Return value :
 *      One of TB_FETCH_CONT, TB_FETCH_SUCC or TB_FETCH_ERROR.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static uint relcur_count(
        void* cd,
        tb_relcur_t *cur,
        ulong* p_count,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;
        dbe_trx_t* dbtrx;
        uint count;
        bool b;

        ss_dprintf_1(("relcur_count:%ld\n", cur->rc_curid));
        SS_PUSHNAME("relcur_count");
        ss_dassert(cur->rc_state != CS_CLOSED);

        if (cur->rc_infolevel >= 8) {
            relcur_info_print(cd, cur, 8, "  Table level count: count rows");
        }

        switch (cur->rc_state) {
            case CS_START:
                ss_dprintf_2(("relcur_count:CS_START\n"));
                cur->rc_state = CS_COUNT;
                break;
            case CS_COUNT:
                ss_dprintf_2(("relcur_count:CS_COUNT\n"));
                break;
            case CS_EMPTY:
                ss_dprintf_2(("relcur_count:CS_EMPTY\n"));
                if (cur->rc_infolevel >= 8) {
                    relcur_info_print(cd, cur, 8, "  Table level count: empty cursor");
                }
                *p_count = 0;
                SS_POPNAME;
                return(TB_FETCH_SUCC);
            case CS_END:
                /* Re-execute the count statement. */
                ss_dprintf_2(("relcur_count:CS_END\n"));
                b = cur_reset(cd, cur);
                if (!b) {
                    cur->rc_state = CS_ERROR;
                    cursor_geterrh(cur, p_errh);
                    SS_POPNAME;
                    return(TB_FETCH_ERROR);
                }
                cur->rc_state = CS_COUNT;
                break;
            case CS_ERROR:
                ss_dprintf_2(("relcur_count:CS_ERROR\n"));
                cursor_geterrh(cur, p_errh);
                SS_POPNAME;
                return(TB_FETCH_ERROR);
            default:
#ifndef AUTOTEST_RUN
                SsErrorMessage(TAB_MSG_BADCURSORSTATE_SD, "relcur_count", cur->rc_state);
#endif
                ss_debug(dbe_db_errorprinttree(rs_sysi_db(cd), TRUE));
                ss_rc_derror(cur->rc_state);
                rs_error_create(p_errh, DBE_ERR_FAILED);
                SS_POPNAME;
                return(TB_FETCH_ERROR);
        }

        dbtrx = tb_trans_dbtrx(cd, cur->rc_trans);
        ss_assert(tb_trans_isactive(cd, cur->rc_trans));

        for (count = 0; count < RELCUR_COUNT_STEPSIZE; count++) {

            if (cur->rc_mainmem) {
                if (cur->rc_waitlock) {
                    rc = dbe_cursor_relock(
                            cur->rc_dbcur,
                            tb_trans_dbtrx(cd, cur->rc_trans),
                            p_errh);
                    if (rc != SU_SUCCESS) {
                        goto error;
                    } else {
                        cur->rc_waitlock = FALSE;
                    }
                }
                /* Reset the vbuf if we've changed directions. */
                if (cur->rc_prevnextp != TRUE) {
                    rs_vbuf_rewind(cd, cur->rc_vbuf);
                    cur->rc_prevnextp = TRUE;
                }
                if (!rs_vbuf_hasdata(cd, cur->rc_vbuf)) {
                    rc = dbe_cursor_nextorprev_n(
                            cur->rc_dbcur,
                            TRUE,
                            tb_trans_dbtrx(cd, cur->rc_trans),
                            cur->rc_vbuf,
                            p_errh);
                } else {
                    rc = DBE_RC_FOUND;
                }
                if (rc == DBE_RC_FOUND) {
                    cur->rc_seltvalue = rs_vbuf_readtval(cd, cur->rc_vbuf);
                    if (cur->rc_seltvalue != NULL) {
                        rc = DBE_RC_FOUND;
                    } else {
                        rc = DBE_RC_END;
                    }
                }
            } else {
                rc = dbe_cursor_nextorprev(
                        cur->rc_dbcur,
                        TRUE,
                        tb_trans_dbtrx(cd, cur->rc_trans),
                        &cur->rc_seltvalue,
                        p_errh);
            }
            
    error:
            switch (rc) {
                case DBE_RC_NOTFOUND:
                    break;
                case DBE_RC_FOUND:
                    cur->rc_count++;
                    break;
                case DBE_RC_WAITLOCK:
                    cur->rc_state = CS_COUNT;
                    cur->rc_waitlock = TRUE;
                    SS_POPNAME;
                    return(TB_FETCH_CONT);
                case DBE_RC_END:
                    if (cur->rc_infolevel >= 8) {
                        relcur_info_print(cd, cur, 8, "  Table level count: end of set");
                    }
                    *p_count = cur->rc_count;
                    cur->rc_count = 0;
                    cur->rc_state = CS_END;
                    SS_POPNAME;
                    return(TB_FETCH_SUCC);
                default:
                    cur->rc_count = 0;
                    cur->rc_state = CS_END;
                    SS_POPNAME;
                    return(TB_FETCH_ERROR);
            }
        }

        SS_POPNAME;
        return(TB_FETCH_CONT);
}

/*#***********************************************************************\
 *
 *              relcur_aggrcount
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_finished -
 *
 *
 *      p_errh -
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
static rs_tval_t* relcur_aggrcount(
    void*        cd,
    tb_relcur_t* cur,
    uint*        p_finished,
        rs_err_t**   p_errh)
{
        ulong count = 0;
        rs_atype_t* atype;
        rs_aval_t* aval;

        ss_dprintf_3(("relcur_aggrcount\n"));

        *p_finished = relcur_count(cd, cur, &count, p_errh);
        switch  (*p_finished) {
            case TB_FETCH_CONT:
            case TB_FETCH_ERROR:
                return(NULL);
            case TB_FETCH_SUCC:
                atype = rs_ttype_atype(cd, cur->rc_aggrttype, 0);
                aval = rs_tval_aval(cd, cur->rc_aggrttype, cur->rc_aggrtval, 0);
                ss_dassert(rs_atype_datatype(cd, atype) == RSDT_INTEGER);
                {
                    ss_debug(bool succp =)
                    rs_aval_setlong_raw(cd, atype, aval, count, NULL);
                    ss_dassert(succp);
                }
                return(cur->rc_aggrtval);
            default:
                ss_error;
                return(NULL);
        }
}

static bool checkuserrowaccess(
        rs_sysi_t* cd,
        tb_relcur_t* cur,
        rs_tval_t* tval)
{
        bool isrowaccess;

        if (rs_relh_rowcheckcolname(cd, cur->rc_relh) != NULL) {
            int ano;
            long id;
            rs_atype_t* atype;
            rs_aval_t* aval;
            tb_relpriv_t* relpriv;
            ano = rs_ttype_anobyname(
                    cd,
                    cur->rc_selttype,
                    rs_relh_rowcheckcolname(cd, cur->rc_relh));
            ss_dassert(ano != RS_ANO_NULL);
            atype = rs_ttype_atype(cd, cur->rc_selttype, ano);
            aval = rs_tval_aval(cd, cur->rc_selttype, tval, ano);
            id = rs_aval_getlong(cd, atype, aval);
            tb_priv_getrelpriv(
                cd,
                id,
                id < RS_USER_ID_START,  /* sysid */
                TRUE,
                &relpriv);
            isrowaccess = tb_priv_issomerelpriv(cd, relpriv);
            if (isrowaccess
            &&  strcmp(rs_relh_name(cd, cur->rc_relh), RS_RELNAME_PROCEDURES)
                == 0)
            {
                if (!tb_priv_iscreatorrelpriv(cd, relpriv)) {
                    /* We are not allowed to show the procedure text.
                     */
                    ano = rs_ttype_anobyname(
                            cd,
                            cur->rc_selttype,
                            (char *)RS_ANAME_PROCEDURES_TEXT);
                    ss_dassert(ano != RS_ANO_NULL);
                    atype = rs_ttype_atype(cd, cur->rc_selttype, ano);
                    aval = rs_tval_aval(cd, cur->rc_selttype, tval, ano);
                    rs_aval_setnull(cd, atype, aval);
                }
            }
        } else {
            isrowaccess = TRUE;
        }
        return(isrowaccess);
}

/*#***********************************************************************\
 *
 *              relcur_cmplong_constr
 *
 * Check if any 'long' constraints exists for this search
 * and if so: do tval pass the constraint.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      ttype -
 *
 *
 *      tval -
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
static bool relcur_cmplong_constr(
        rs_sysi_t* cd,
        tb_relcur_t* cur,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        rs_cons_t* cons;
        tb_database_t* tdb;
        int tb_maxcmplen;
        su_list_node_t* n;

        ss_dprintf_3(("relcur_cmplong_constr\n"));

        if (!cur->rc_longcons) {

            /* here is one owerhead for every row on this implementation
             * other one is hendling new struct member:
             * cur->rc_longcons, which reviewers should dgrep
             */
            ss_dprintf_3(("relcur_cmplong_constr:no long constr\n"));
            return(TRUE);
        }

        ss_dprintf_3(("relcur_cmplong_constr:long constr\n"));

        /*
        This is not wise: do on aval's if needed.

        rc = tb_blob_readsmallblobstotval(cd, ttype, tval, TB_MAXCMPLEN);
        ss_rc_dassert(rc == SU_SUCCESS, rc);
        */

        tdb = rs_sysi_tabdb(cd);
        tb_maxcmplen = tb_getmaxcmplen(tdb);

        su_list_do_get(cur->rc_constr_l, n , cons) {
            rs_sqlcons_t* sqlcons;
            rs_atype_t* atype;
            rs_aval_t* aval;

            sqlcons = rs_cons_getsqlcons(cd, cons);

            atype = rs_ttype_sql_atype(cd, ttype, sqlcons->sc_attrn);
            aval = rs_tval_sql_aval(cd, ttype, tval, sqlcons->sc_attrn);

            ss_dprintf_4(("relcur_cmplong_constr:sqlcons->sc_attrn=%d\n", sqlcons->sc_attrn));

            if (!rs_aval_isnull(cd, atype, aval)) {
                bool passed;

                if (rs_aval_isblob(cd, atype, aval)) {

                    size_t sizelimit = tb_maxcmplen;
                    rs_datatype_t dt;

                    ss_dprintf_4(("relcur_cmplong_constr:load blob\n"));

                    dt = rs_atype_datatype(cd, atype);
                    if (dt == RSDT_UNICODE) {
                        sizelimit = sizelimit * sizeof(ss_char2_t);
                    }

                    passed = tb_blobg2_loadblobtoaval_limit(
                                    cd,
                                    atype,
                                    aval,
                                    sizelimit);
                    ss_dassert(passed);
                }
                if (sqlcons->sc_relop == RS_RELOP_LIKE) {

                    ss_dprintf_4(("relcur_cmplong_constr:like compare\n"));
                    passed = rs_aval_like(cd, atype, aval,
                                          sqlcons->sc_atype, sqlcons->sc_aval,
                                          sqlcons->sc_escatype, sqlcons->sc_escaval);
                } else {
                    ss_dprintf_4(("relcur_cmplong_constr:normal compare\n"));
                    passed = rs_aval_cmp(cd, atype, aval,
                                         sqlcons->sc_atype, sqlcons->sc_aval,
                                         sqlcons->sc_relop);
                }
                if (!passed) {
                    return(FALSE);
                }
            }
        }

        return(TRUE);
}


/*#***********************************************************************\
 *
 *              relcur_nextorprev
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_finished -
 *
 *
 *      nextp -
 *
 *
 *      p_err -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_tval_t* relcur_nextorprev(cd, cur, p_finished, nextp, p_errh)
    rs_sysi_t*   cd;
    tb_relcur_t* cur;
    uint*        p_finished;
        bool         nextp;
        rs_err_t**   p_errh;
{
        dbe_ret_t  dbret;
        rs_tval_t* tval = NULL;

        ss_pprintf_1(("relcur_nextorprev:%ld, nextp = %d\n", cur->rc_curid, nextp));
        ss_dassert(cur->rc_state != CS_CLOSED);

        cur->rc_isunknownconstr = FALSE;

        if (cur->rc_errcode != SU_SUCCESS) {
            ss_pprintf_2(("relcur_nextorprev:%ld:errcode=%d (%s)\n", cur->rc_curid, cur->rc_errcode, su_rc_nameof(cur->rc_errcode)));
            rs_error_create(p_errh, cur->rc_errcode);
            *p_finished = TB_FETCH_ERROR;
            return(NULL);
        }
        if (rs_relh_isaborted(cd, cur->rc_relh)) {
            ss_dprintf_2(("relcur_nextorprev:%ld, E_DDOP\n", cur->rc_curid));
            rs_error_create(p_errh, E_DDOP);
            *p_finished = TB_FETCH_ERROR;
            return(NULL);
        }

#ifdef SS_MME
        if (cur->rc_mainmem) {
            cur_tval_free(cd, cur);
            if (cur->rc_curtype == DBE_CURSOR_FORUPDATE) {
                rs_sysi_setdisablerowspermessage(cd, TRUE);
            }
        } else {
            cur_tval_freeif(cd, cur);
        }
#endif
        if (cur->rc_aggrcount) {
            ss_bassert(nextp);
            return(relcur_aggrcount(cd, cur, p_finished, p_errh));
        }

        if (cur->rc_reverseset) {
            ss_pprintf_2(("relcur_nextorprev:%ld, reverse result set\n", cur->rc_curid));
            nextp = !nextp;
        }

        cur->rc_dbtref = NULL;

        switch (cur->rc_state) {
            case CS_START:
                if (!nextp) {
                    *p_finished = TB_FETCH_SUCC;
                    return(NULL);
                }
                break;
            case CS_END:
                if (nextp) {
                    *p_finished = TB_FETCH_SUCC;
                    return(NULL);
                }
                break;
            case CS_EMPTY:
                if (cur->rc_infolevel >= 8) {
                    relcur_info_print(cd, cur, 8, "  Table level fetch: empty cursor");
                }
                *p_finished = TB_FETCH_SUCC;
                return(NULL);
            case CS_ERROR:
                cursor_geterrh(cur, p_errh);
                *p_finished = TB_FETCH_ERROR;
                return(NULL);
            default:
                break;
        }

        ss_assert(cur->rc_dbcur);

        if (!tb_trans_isactive(cd, cur->rc_trans)) {
            su_err_init(p_errh, E_TRANSNOTACT);
            *p_finished = TB_FETCH_ERROR;
            return(NULL);
        }

        if (su_pa_nelems(cur->rc_updaval_pa) > 0) {
            relcur_updaval_clear(cd, cur);
        }

        if (cur->rc_seltvalue != NULL) {
            rs_tval_clearallrowflags(cd, cur->rc_selttype, cur->rc_seltvalue);
        }

        if (cur->rc_state == CS_ROW 
            && tb_est_get_single_row(cd, cur->rc_est) 
            && nextp) 
        {
            ss_dprintf_2(("relcur_nextorprev:%ld, next, CS_ROW, unique value\n", cur->rc_curid));
            ss_assert(cur->rc_dbcur);
            dbret = dbe_cursor_gotoend(
                        cur->rc_dbcur,
                        tb_trans_dbtrx(cd, cur->rc_trans),
                        NULL);
            if (dbret != DBE_RC_SUCC) {
                goto error;
            }
            su_rc_dassert(dbret == DBE_RC_SUCC, dbret);
            *p_finished = TB_FETCH_SUCC;
            cur->rc_state = CS_END;
            if (cur->rc_infolevel >= 8) {
                relcur_info_print(cd, cur, 8, "  Table level fetch: end of set (unique)");
            }
            return(NULL);
        }

        if (p_errh != NULL) {
            *p_errh = NULL;
        }

        if (cur->rc_mainmem) {
            if (cur->rc_waitlock) {
                dbret = dbe_cursor_relock(
                        cur->rc_dbcur,
                        tb_trans_dbtrx(cd, cur->rc_trans),
                        p_errh);
                if (dbret != SU_SUCCESS) {
                    goto error;
                } else {
                    cur->rc_waitlock = FALSE;
                }
            }
            /* Reset the vbuf if we've changed directions. */
            if (cur->rc_prevnextp != nextp) {
                rs_vbuf_rewind(cd, cur->rc_vbuf);
                cur->rc_prevnextp = nextp;
            }
            if (!rs_vbuf_hasdata(cd, cur->rc_vbuf)) {
                dbret = dbe_cursor_nextorprev_n(
                        cur->rc_dbcur,
                        nextp,
                        tb_trans_dbtrx(cd, cur->rc_trans),
                        cur->rc_vbuf,
                        p_errh);
            } else {
                dbret = DBE_RC_FOUND;
            }
            if (dbret == DBE_RC_FOUND) {
                cur->rc_seltvalue = rs_vbuf_readtval(cd, cur->rc_vbuf);
                if (cur->rc_seltvalue != NULL) {
                    dbret = DBE_RC_FOUND;
                } else {
                    dbret = DBE_RC_END;
                }
            }
        } else {
            
            if (cur->rc_isscrollsensitive){
                /* If the cursor is marked scroll sensitive, a new search must
                 * be started with each fetch (if D-tables are used.)
                 */
                dbe_cursor_restartsearch(
                        cur->rc_dbcur,
                        tb_trans_dbtrx(cd, cur->rc_trans));
            }
            
            dbret = dbe_cursor_nextorprev(
                    cur->rc_dbcur,
                    nextp,
                    tb_trans_dbtrx(cd, cur->rc_trans),
                    &cur->rc_seltvalue,
                    p_errh);
        }

    error:
        switch (dbret) {
            case DBE_RC_SUCC:
                ss_error;
                break;

            case DBE_RC_FOUND:
                ss_pprintf_2(("relcur_nextorprev:%ld:found tuple\n", cur->rc_curid));
                ss_dassert(cur->rc_seltvalue != NULL);
                ss_beta(if (cur->rc_nrows != -1L) cur->rc_nrows++);
                cur->rc_state = CS_ROW;
                if (cur->rc_pseudo_l != NULL) {
                    cur_pseudoattr_settval(cd, cur);
                }
                *p_finished = TB_FETCH_SUCC;
                tval = cur->rc_seltvalue;
                if (cur->rc_infolevel >= 8) {
                    if (nextp) {
                        relcur_info_print(cd, cur, 8, "  Table level fetch: found a row");
                    } else {
                        relcur_info_print(cd, cur, 8, "  Table level reverse: found a row");
                    }
                }
                ss_output_4(rs_tval_print(cd, cur->rc_selttype, tval));

                /* We have a row candidate here.
                 * Now we, in tab-level, check if it matches user constraints
                 */
                if (!relcur_cmplong_constr(cd, cur, cur->rc_selttype, cur->rc_seltvalue)) {
                    cur->rc_state = CS_NOROW;
                    *p_finished = TB_FETCH_CONT;
                }

                if (!checkuserrowaccess(cd, cur, cur->rc_seltvalue)) {
                    cur->rc_state = CS_NOROW;
                    *p_finished = TB_FETCH_CONT;
                }
                break;

            case DBE_RC_NOTFOUND:
            case DBE_RC_WAITLOCK:
                ss_pprintf_2(("relcur_nextorprev:%ld:not found, continue\n", cur->rc_curid));
                cur->rc_state = CS_NOROW;
                cur->rc_waitlock = TRUE;
                *p_finished = TB_FETCH_CONT;
                break;

            case DBE_RC_END:
                /* cursor in end state */
                ss_pprintf_2(("relcur_nextorprev:%ld:not found, end\n", cur->rc_curid));
                *p_finished = TB_FETCH_SUCC;
                if (nextp) {
                    cur->rc_state = CS_END;
                } else {
                    cur->rc_state = CS_START;
                }
                if (cur->rc_infolevel >= 8) {
                    relcur_info_print(cd, cur, 8, "  Table level fetch: end of set");
                }
                break;

           default: 
                /* If update or delete fails to find the tuple by reference
                   this error is returned.
                   It can (should) not be found here.
                */
                if (p_errh != NULL && *p_errh == NULL) {
                    /* Error message has not been set yet. */
                    rs_error_create(p_errh, DBE_ERR_FAILED);
                }
                /* no break. */

            case DBE_ERR_NOTFOUND:
                ss_pprintf_2(("relcur_nextorprev:%ld:not found, ERROR\n", cur->rc_curid));
                *p_finished = TB_FETCH_ERROR;
                cur->rc_state = CS_EMPTY;
                if (cur->rc_infolevel >= 8) {
                    relcur_info_print(cd, cur, 8, "  Table level fetch: error in cursor");
                }
                break;
        }

        FAKE_CODE_BLOCK(
                FAKE_TAB_SLEEP_AFTER_FETCH,
                {
                    if (*p_finished == TB_FETCH_SUCC
                        && !rs_relh_issysrel(cd, cur->rc_relh)) {
                        SsPrintf("FAKE_TAB_SLEEP_AFTER_FETCH: %s\n",
                                 rs_relh_name(cd, cur->rc_relh));
                        SsThrSleep(5000);
                    }
                });

        return(tval);
}

/*##**********************************************************************\
 *
 *              tb_relcur_next
 *
 * Will fetch the next row of the result of a relation cursor.
 *
 * The function may called only after calling the relcuropen
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      p_finished - out
 *          TB_FETCH_SUCC  (1) is stored in *p_finished if the operation
 *                             terminated,
 *          TB_FETCH_CONT  (0) is stored in *p_finished if the operation
 *                             did not complete,
 *          TB_FETCH_ERROR (2) is stored into *p_finished in case of
 *                             error
 *
 *      p_err - out
 *          in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value - ref :
 *
 *      A pointer into a private structure containing the
 *      current row of the relation cursor. NULL is returned if the
 *      cursor moved into the end state or the operation
 *      did not terminate.
 *      The type of the returned row is the same as the table tuple type.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tval_t* tb_relcur_next(cd, cur, p_finished, p_errh)
    void*        cd;
    tb_relcur_t* cur;
    uint*        p_finished;
        rs_err_t**   p_errh;
{
        rs_tval_t* tval;

        CHK_RELCUR(cur);
        SS_PUSHNAME("tb_relcur_next");
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_next");

        tval = relcur_nextorprev(cd, cur, p_finished, TRUE, p_errh);

        SS_POPNAME;

        return(tval);
}


/*##**********************************************************************\
 *
 *              tb_relcur_next_sql
 *
 * Member of the SQL function block.
 * Will fetch the next row of the result of a relation cursor.
 *
 * The function may called only after calling the relcuropen
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      p_result - out
 *           A pointer into a private structure containing the
 *           current row of the relation cursor. NULL is returned if the
 *           cursor moved into the end state or the operation
 *           did not terminate.
 *           The type of the returned row is the same as the table tuple
 *           type.
 *
 *      cont - in out, use
 *          cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - out
 *          in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *          TRUE if operation was successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_next_sql(cd, cur, p_result, cont, p_errh)
    void*        cd;
    tb_relcur_t* cur;
        rs_tval_t**  p_result;
    void**       cont;
        rs_err_t**   p_errh;
{
        uint finished;
        bool succ = TRUE;

        if (cur->rc_ishurc) {
            return(tb_hurc_next_sql(cd, TB_CUR_CAST_HURC(cur), p_result, cont, p_errh));
        }

        CHK_RELCUR(cur);
        SS_PUSHNAME("tb_relcur_next_sql");
        SU_GENERIC_TIMER_START(SU_GENERIC_TIMER_TAB_SQLFETCH);
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_next_sql");

        *p_result = relcur_nextorprev(cd, cur, &finished, TRUE, p_errh);

        switch (finished) {
            case TB_FETCH_CONT :
                *cont = cur;
                *p_result = NULL;
                break;
            case TB_FETCH_SUCC :
                *cont = NULL;
                break;
            case TB_FETCH_ERROR :
                /* fallthrough */
            default :
                *cont = NULL;
                succ = FALSE;
                break;
        };

        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_TAB_SQLFETCH);
        SS_POPNAME;

        return(succ);
}

/*##**********************************************************************\
 *
 *              tb_relcur_prev
 *
 * Will fetch the previous row of the result of a relation cursor.
 *
 * The function may called only after calling the relcuropen
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      p_finished - out
 *          TB_FETCH_SUCC  (1) is stored in *p_finished if the operation
 *                             terminated,
 *          TB_FETCH_CONT  (0) is stored in *p_finished if the operation
 *                             did not complete,
 *          TB_FETCH_ERROR (2) is stored into *p_finished in case of
 *                             error
 *
 *      p_err - out
 *          in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value - ref :
 *
 *      a pointer into a private structure containing the
 *      current row of the relation cursor. NULL is returned if the
 *      cursor moved into the begin state or the operation
 *      did not terminate
 *      The type of the returned row is the same as the table tuple type.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tval_t* tb_relcur_prev(cd, cur, p_finished, p_errh)
    void*        cd;
    tb_relcur_t* cur;
    uint*        p_finished;
        rs_err_t**   p_errh;
{
        rs_tval_t* tval;

        CHK_RELCUR(cur);
        SS_PUSHNAME("tb_relcur_prev");
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_prev");
        ss_beta(cur->rc_nrows = -1L);

        tval = relcur_nextorprev(cd, cur, p_finished, FALSE, p_errh);

        SS_POPNAME;

        return(tval);
}

/*##**********************************************************************\
 *
 *              tb_relcur_prev_sql
 *
 * Member of the SQL function block.
 * Will fetch the next row of the result of a relation cursor.
 *
 * The function may called only after calling the relcuropen
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      p_result - out
 *           A pointer into a private structure containing the
 *           current row of the relation cursor. NULL is returned if the
 *           cursor moved into the end state or the operation
 *           did not terminate.
 *           The type of the returned row is the same as the table tuple
 *           type.
 *
 *      cont - in out, use
 *          cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - out
 *          in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *          TB_PREV_SUCC       in case of successful operation
 *          TB_PREV_NOTALLOWED in case reversing is not possible
 *          TB_PREV_ERROR      in case of error
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_relcur_prev_sql(cd, cur, p_result, cont, p_errh)
    void*        cd;
    tb_relcur_t* cur;
        rs_tval_t**  p_result;
    void**       cont;
        rs_err_t**   p_errh;
{
        uint finished;
        uint succ = TB_PREV_SUCC;

        if (cur->rc_ishurc) {
            return(tb_hurc_prev_sql(cd, TB_CUR_CAST_HURC(cur), p_result, cont, p_errh));
        }

        CHK_RELCUR(cur);
        SS_PUSHNAME("tb_relcur_prev_sql");
        SU_GENERIC_TIMER_START(SU_GENERIC_TIMER_TAB_SQLFETCH);
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_prev_sql");

        ss_beta(cur->rc_nrows = -1L);

        *p_result = relcur_nextorprev(cd, cur, &finished, FALSE, p_errh);

        switch (finished) {
            case TB_FETCH_CONT :
                *cont = cur;
                *p_result = NULL;
                break;
            case TB_FETCH_SUCC :
                *cont = NULL;
                break;
            case TB_FETCH_ERROR :
                /* fallthrough */
            default :
                *cont = NULL;
                succ = TB_PREV_ERROR;
                break;
        };

        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_TAB_SQLFETCH);
        SS_POPNAME;

        return(succ);
}

/*##**********************************************************************\
 *
 *              tb_relcur_current
 *
 * The tb_relcur_current function fetches the current row of a table
 * cursor.
 *
 * Parameters :
 *
 *      cd - in
 *              application state
 *
 *      cur - in
 *              pointer into the table cursor
 *
 * Return value :
 *
 *      a pointer into a private structure containing the
 *          current row of the table cursor,the returned row should
 *          be of type as given by the function sql_dm_tabcurdescribe
 *      NULL is returned if the table cursor is in the begin
 *          or the end state
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_tval_t* tb_relcur_current(
    void*        cd __attribute__ ((unused)),
    tb_relcur_t* cur)
{
        rs_tval_t* tval;

        if (cur->rc_ishurc) {
            return(tb_hurc_current(cd, TB_CUR_CAST_HURC(cur)));
        }

        CHK_RELCUR(cur);
        SS_PUSHNAME("tb_relcur_current");
        ss_dprintf_1(("tb_relcur_current:%ld\n", cur->rc_curid));

        if (cur->rc_state == CS_ROW) {
            tval = cur->rc_seltvalue;
        } else {
            tval = NULL;
        }

        SS_POPNAME;

        return(tval);
}

/*##**********************************************************************\
 *
 *              tb_relcur_aggr
 *
 * This function will see if the result of a set of
 * aggregate operations (i.e. COUNT, SUM) on a table cursor is directly
 * available.
 *
 * If it is, fetching the result set of the cursor will return the
 * lines containing the aggregate results instead of the original result
 * set. In this case, after this call the cursor will be in the begin
 * state of the new. Also, in this case the function tb_relcur_aggr
 * may not be called again.
 *
 * If there are are GROUP BY items, the GROUP BY items will appear at
 * the beginning of the result rows.
 *
 * If the aggregate result is available, the new row type of the cursor
 * will be available bu the call tb_relcur_describe.
 *
 * If the aggregate result is not available, the state of the cursor
 * will not change. In this case, the tb_relcur_aggr function may
 * be called again (possibly with a different set of aggregate operations).
 *
 * The function may called only after calling the tb_relcur_open
 * function.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      groupby_c -
 *              the number of GROUP BY columns (may be zero)
 *
 *      groupbyarr -
 *              array of index numbers of the GROUP BY
 *          columns (used only if groupby_c > 0)
 *
 *      aggr_c -
 *              the number of aggregate items i.e. number
 *          of items in the SELECT list (may be zero only if
 *          groupby_c is non-zero)
 *
 *      aggrfunarr -
 *              array of aggr_c aggregate function names
 *
 *      aggrargarr -
 *              index numbers of the aggr_c argument columns
 *          to the aggregate functions (-1 means "*" as
 *          in COUNT(*))
 *
 *      distarr -
 *              array of aggr_c flags for the key word DISTINCT
 *          like SUM(DISTINCT <col>) (value 1 means DISTINCT,
 *          value 0 means without DISTINCT)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_relcur_aggr(
        void*        cd,
        tb_relcur_t* cur,
        uint         groupby_c,
        uint*        groupbyarr __attribute__ ((unused)),
        uint         aggr_c,
        char**       aggrfunarr,
        int*         aggrargarr,
        bool*        distarr)
{
        if (cur->rc_ishurc) {
            return(tb_hurc_aggr(
                        cd,
                        TB_CUR_CAST_HURC(cur),
                        groupby_c,
                        groupbyarr,
                        aggr_c,
                        aggrfunarr,
                        aggrargarr,
                        distarr));
        }

        ss_dprintf_1(("tb_relcur_aggr:%ld\n", cur->rc_curid));
        CHK_RELCUR(cur);
        ss_dassert(cur->rc_state != CS_CLOSED);

        if (groupby_c == 0
        &&  aggr_c == 1
        &&  strcmp(aggrfunarr[0], "COUNT") == 0
        &&  aggrargarr[0] == -1
        &&  !distarr[0]
        &&  !cur->rc_aggrcount)
        {
            rs_atype_t* atype;

            cur->rc_aggrcount = TRUE;
            /* Build aggregate ttype. */
            cur->rc_aggrttype = rs_ttype_create(cd);
            atype = rs_atype_initlong(cd);
            rs_ttype_setatype(cd, cur->rc_aggrttype, 0, atype);
            rs_atype_free(cd, atype);
            /* Build aggregate tval. */
            cur->rc_aggrtval = rs_tval_create(cd, cur->rc_aggrttype);
            return(TRUE);
        }
        return(FALSE);
}

/*##**********************************************************************\
 *
 *              tb_relcur_aval
 *
 * The tb_relcur_aval function inquires the current value of
 * a column in the current row of results in a table cursor. The value
 * is assumed to be available at the specified address until any next
 * operation with the cursor.
 *
 * The function is called only when there is a current row in a cursor
 * i.e. after a successful fetch or reverse call. Typically, the function
 * is called in the situation where a value is needed in a positioned
 * UPDATE epxression although it was not in the original SELECT list.
 *
 * Parameters :
 *
 *      cd - in
 *              client data
 *
 *      cur - use
 *              relation cursor pointer
 *
 *      ano - in
 *              Attribute number in relation type.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_aval_t* tb_relcur_aval(
        void*        cd,
        tb_relcur_t* cur,
        uint         ano)
{
        rs_ttype_t* ttype;
        rs_atype_t* atype;
        rs_aval_t* aval;
        rs_key_t* clusterkey;
        int kpno;

        if (cur->rc_ishurc) {
            return(tb_hurc_aval(cd, TB_CUR_CAST_HURC(cur), ano));
        }

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_aval:%ld\n", cur->rc_curid));
        ss_dassert(cur->rc_state == CS_ROW);

        ttype = rs_relh_ttype(cd, cur->rc_relh);

        ano = rs_ttype_sqlanotophys(cd, ttype, ano);
        ss_dassert((int)ano != RS_ANO_NULL);

        if (su_pa_indexinuse(cur->rc_updaval_pa, ano)) {

            aval = su_pa_getdata(cur->rc_updaval_pa, ano);

        } else {

            atype = rs_ttype_atype(cd, ttype, ano);

            clusterkey = rs_relh_clusterkey(cd, cur->rc_relh);
            ss_dassert(rs_key_isclustering(cd, clusterkey));

            kpno = rs_key_searchkpno_data(cd, clusterkey, ano);
            ss_dassert(kpno != RS_ANO_NULL);

            aval = dbe_cursor_getaval(
                    cur->rc_dbcur,
                    cur->rc_seltvalue,
                    atype, kpno);

            su_pa_insertat(cur->rc_updaval_pa, ano, aval);
        }

        return(aval);
}

/*##**********************************************************************\
 *
 *              tb_relcur_begin
 *
 * Moves the cursor into the begin state ('next' will give the first row).
 *
 * The function may be called only after calling the relcuropen
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 * Return value :
 *
 *      TRUE if the operation completed,
 *      FALSE if not
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_begin(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        bool succp;
        bool b;

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_begin:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_begin");

        if (cur->rc_infolevel >= 8) {
            relcur_info_print(cd, cur, 8, "  Table level: goto begin of set");
        }

        if (cur->rc_reverseset) {
            ss_dprintf_2(("tb_relcur_begin:%ld, reverse result set\n", cur->rc_curid));
            cur->rc_reverseset = TB_REVERSESET_NONE; /* Avoid endless recursion. */
            succp = tb_relcur_end(cd, cur);
            cur->rc_reverseset = TB_REVERSESET_NORMAL;
            return(succp);
        }

        switch (cur->rc_state) {
            case CS_START:
                ss_dprintf_2(("tb_relcur_begin:%ld, already in start state\n", cur->rc_curid));
                return(TRUE);
            case CS_EMPTY:
                ss_dprintf_2(("tb_relcur_begin:%ld, search set is empty\n", cur->rc_curid));
                return(TRUE);
            case CS_ERROR:
                return(TRUE);
            default:
                b = cur_reset(cd, cur);
                ss_dassert(b);
                return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              tb_relcur_begin_sql
 *
 * Member of the SQL function block.
 * Moves the cursor into the end state ('prev' will give the last row).
 *
 * The function may be called only after calling the relcuropen
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      cont - in out, use
 *          cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      errhandle - out
 *          in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation completed,
 *      FALSE if not
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_begin_sql(
        void*        cd,
        tb_relcur_t* cur,
        void**       cont,
        rs_err_t**   errhandle)
{
        if (cur->rc_ishurc) {
            return(tb_hurc_begin_sql(cd, TB_CUR_CAST_HURC(cur), cont, errhandle));
        }

        *errhandle = NULL;
        *cont = NULL;

        return( tb_relcur_begin(cd, cur) );
}

/*##**********************************************************************\
 *
 *              tb_relcur_end
 *
 * Moves the cursor into the end state ('prev' will give the last row).
 *
 * The function may be called only after calling the relcuropen
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 * Return value :
 *
 *      TRUE if the operation completed,
 *      FALSE if not
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_end(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        bool succp;
        bool b;
        dbe_ret_t dbret;

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_end:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_end");

        if (cur->rc_infolevel >= 8) {
            relcur_info_print(cd, cur, 8, "  Table level: goto end of set");
        }

        if (cur->rc_reverseset) {
            ss_dprintf_2(("tb_relcur_end:%ld, reverse result set\n", cur->rc_curid));
            cur->rc_reverseset = TB_REVERSESET_NONE; /* Avoid endless recursion. */
            succp = tb_relcur_begin(cd, cur);
            cur->rc_reverseset = TB_REVERSESET_NORMAL;
            return(succp);
        }

        switch (cur->rc_state) {
            case CS_END:
                ss_dprintf_2(("tb_relcur_end:%ld, already in end state\n", cur->rc_curid));
                return(TRUE);
            case CS_EMPTY:
                ss_dprintf_2(("tb_relcur_end:%ld, search set is empty\n", cur->rc_curid));
                return(TRUE);
            case CS_ERROR:
                ss_dprintf_2(("tb_relcur_end:%ld, error\n", cur->rc_curid));
                return(TRUE);
            default:
                b = cur_reset(cd, cur);
                if (b) {
                    ss_assert(tb_trans_isactive(cd, cur->rc_trans));
                    dbret = dbe_cursor_gotoend(
                                cur->rc_dbcur,
                                tb_trans_dbtrx(cd, cur->rc_trans),
                                NULL);
                    if (dbret == DBE_RC_SUCC) {
                        cur->rc_state = CS_END;
                    }
                }
                return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              tb_relcur_end_sql
 *
 * Member of the SQL function block.
 * Moves the cursor into the end state ('prev' will give the last row).
 *
 * The function may be called only after calling the relcuropen
 * function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      cont - in out, use
 *          cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      errhandle - out
 *          in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation completed,
 *      FALSE if not
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_end_sql(
        void*        cd,
        tb_relcur_t* cur,
        void**       cont,
        rs_err_t**   errhandle)
{
        if (cur->rc_ishurc) {
            return(tb_hurc_end_sql(cd, TB_CUR_CAST_HURC(cur), cont, errhandle));
        }

        *errhandle = NULL;
        *cont = NULL;

        return( tb_relcur_end(cd, cur) );
}

/*##**********************************************************************\
 *
 *              tb_relcur_setposition
 *
 * Positions the cursor to a value specified by the values in parameter
 * tval. The tval parameter must be of a same type as a relation ttype.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cur - use
 *
 *
 *      tval - in
 *
 *
 * Return value :
 *
 *      TRUE if the operation was succesful
 *      FALSE if not
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_relcur_setposition(
        void*           cd,
        tb_relcur_t*    cur,
        rs_tval_t*      tval,
        rs_err_t**      p_errh) {
        bool b;
        dbe_ret_t dbret;

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_setposition:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_assert(tb_trans_isactive(cd, cur->rc_trans));
        ss_debug(cur->rc_lastcall = (char *)"tb_relcur_setposition");

        b = cur_reset(cd, cur);
        if (b) {
            dbret = dbe_cursor_setposition(
                        cur->rc_dbcur,
                        tb_trans_dbtrx(cd, cur->rc_trans),
                        tval,
                        p_errh);
            if (dbret != DBE_RC_SUCC) {
                b = FALSE;
            } else {
                cur->rc_state = CS_NOROW;
            }
        }
        return(b);
}

/*##**********************************************************************\
 *
 *              tb_relcur_ttype
 *
 * Member of the SQL function block.
 * Describes a relation cursor by returning a tuple type that corresponds
 * to the cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in, use
 *              relation cursor pointer
 *
 * Return value - ref :
 *
 *      Pointer into a row type that corresponds to the relation handle .
 *      The returned type is always the same as the table tuple type.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_ttype_t* tb_relcur_ttype(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        if (cur->rc_ishurc) {
            return(tb_hurc_ttype(cd, TB_CUR_CAST_HURC(cur)));
        }

        SS_NOTUSED(cd);

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_ttype:%ld\n", cur->rc_curid));

        ss_dassert(cur->rc_selttype != NULL);

        if (cur->rc_aggrcount) {
            ss_dassert(cur->rc_aggrttype != NULL);
            return(cur->rc_aggrttype);
        } else {
            return(cur->rc_selttype);
        }
}

/*#***********************************************************************\
 *
 *              relcur_ensurefulloldtval
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
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
static void relcur_ensurefulloldtval(
        rs_sysi_t* cd,
    tb_relcur_t* cur)
{
        int i;
        int nattrs;
        rs_atype_t* atype;
        rs_aval_t* aval;
        int kpno;
        rs_key_t* clusterkey;
        rs_ttype_t* relh_ttype;
        bool* selflags;

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);
        nattrs = rs_ttype_nattrs(cd, relh_ttype);
        clusterkey = rs_relh_clusterkey(cd, cur->rc_relh);
        ss_dassert(rs_key_isclustering(cd, clusterkey));

        /* Check columns that are already selected.
         */
        selflags = SsMemCalloc(nattrs, sizeof(selflags[0]));
        for (i = 0; cur->rc_sellist[i] != RS_ANO_NULL; i++) {
            int ano;
            ano = cur->rc_sellist[i];
            if (ano != RS_ANO_PSEUDO) {
                selflags[ano] = TRUE;
            }
        }

        /* First get real columns that are not yet selected.
         * Two step process is done because pseudo columns may reference
         * to real column values.
         */
        for (i = 0; i < nattrs; i++) {
            if (!selflags[i]) {
                rs_attrtype_t attrtype;
                atype = rs_ttype_atype(cd, relh_ttype, i);
                attrtype = rs_atype_attrtype(cd, atype);
                if (!rs_atype_pseudo(cd, atype)
                    && (attrtype == RSAT_USER_DEFINED
                        || attrtype == RSAT_SYNC)) {
                    kpno = rs_key_searchkpno_data(cd, clusterkey, i);
                    ss_dassert(kpno != RS_ANO_NULL);
                    aval = dbe_cursor_getaval(
                            cur->rc_dbcur,
                            cur->rc_seltvalue,
                            atype, kpno);
                    rs_tval_insertaval(
                        cd,
                        relh_ttype,
                        cur->rc_seltvalue,
                        i,
                        aval);
                }
            }
        }

        /* Then get pseudocolumns that are not yet selected.
         */
        for (i = 0; i < nattrs; i++) {
            if (!selflags[i]) {
                atype = rs_ttype_atype(cd, relh_ttype, i);
                if (rs_atype_pseudo(cd, atype)) {
                    rs_aval_t* aval;
                    aval = rs_tval_aval(cd, relh_ttype, cur->rc_seltvalue, i);
                    if (rs_aval_isnull(cd, atype, aval)) {
                        cur_pseudocol_t* cp;
                        cp = cur_project_pseudo(
                                cd,
                                relh_ttype,
                                i,
                                NULL);
                        if (cp != NULL) {
                            bool succp;
                            succp = cur_pseudoattr_setaval(cd, cur, cp);
                            ss_dassert(succp);
                            cur_pseudocol_done(cp);
                        }
                    }
                }
            }
        }
        SsMemFree(selflags);
}

/*#***********************************************************************\
 *
 *              cur_upd_setupdallcolumns
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      phys_selflags -
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
static void cur_upd_setupdallcolumns(
        rs_sysi_t* cd,
    tb_relcur_t* cur,
        bool* phys_selflags)
{
        int i;
        int nattrs;
        rs_atype_t* atype;
        rs_ttype_t* relh_ttype;

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);
        nattrs = rs_ttype_nattrs(cd, relh_ttype);

        for (i = 0; i < nattrs; i++) {
            if (!phys_selflags[i]) {
                rs_attrtype_t attrtype;
                atype = rs_ttype_atype(cd, relh_ttype, i);
                attrtype = rs_atype_attrtype(cd, atype);
                if (attrtype == RSAT_SYNC ||
                    (attrtype == RSAT_USER_DEFINED &&
                     !rs_atype_pseudo(cd, atype))) {
                    phys_selflags[i] = TRUE;
                }
            }
        }
}

#ifdef JARMOR_UPDATECHANGEDCOLUMNS

/*#***********************************************************************\
 *
 *              cur_upd_setupdchangedcolumns
 *
 * Marks only changed columns as updated. Does this by comparing to old
 * column values to the new ones.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      phys_selflags -
 *
 *
 *      tval -
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
static void cur_upd_setupdchangedcolumns(
        rs_sysi_t* cd,
    tb_relcur_t* cur,
        bool* phys_selflags,
        rs_tval_t* tval __attribute__ ((unused)))
{
        int i;
        int nattrs;
        rs_atype_t* atype;
        rs_ttype_t* relh_ttype;

        ss_dprintf_3(("cur_upd_setupdchangedcolumns\n"));

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);
        nattrs = rs_ttype_nattrs(cd, relh_ttype);

        for (i = 0; i < nattrs; i++) {
            ss_dprintf_3(("  i=%d\n", i));
            if (phys_selflags[i]) {
                rs_attrtype_t attrtype;
                atype = rs_ttype_atype(cd, relh_ttype, i);
                attrtype = rs_atype_attrtype(cd, atype);
                ss_dprintf_3(("  phys_selflags is TRUE\n"));
                if (attrtype == RSAT_SYNC) {
                    /* Always mark as changed.
                     */
                    phys_selflags[i] = TRUE;
                } else if (attrtype == RSAT_USER_DEFINED &&
                           !rs_atype_pseudo(cd, atype))  {
                    /* Compare new value to the old value.
                     */
                    bool b;
                    rs_aval_t* old_aval;
                    rs_aval_t* new_aval;

                    ss_dassert(i != rs_ttype_anobyname(cd, cur->rc_selttype,
                                                       (char *)RS_ANAME_SYNCTUPLEVERS));

                    old_aval = rs_tval_aval(
                                cd,
                                cur->rc_selttype,
                                cur->rc_seltvalue,
                                i);
                    new_aval = rs_tval_aval(
                                cd,
                                cur->rc_selttype,
                                cur->rc_newtval,
                                i);
                    b = rs_aval_cmp_simple(
                            cd,
                            atype,
                            old_aval,
                            atype,
                            new_aval,
                            RS_RELOP_EQUAL);
                    phys_selflags[i] = !b;
                    ss_output_4(
                    {
                        char* buf1;
                        char* buf2;
                        buf1 = rs_aval_print(cd, atype, old_aval);
                        buf2 = rs_aval_print(cd, atype, new_aval);
                        ss_dprintf_4(("  b=%d, old_aval=%.128s, new_aval=%s.128\n",
                            b,
                            buf1,
                            buf2));
                        SsMemFree(buf1);
                        SsMemFree(buf2);
                    }
                    );
                }
            }
        }
}

#endif /* JARMOR_UPDATECHANGEDCOLUMNS */

/*#***********************************************************************\
 *
 *              relcur_update_init
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      tvalue -
 *
 *
 *      selflags -
 *
 *
 *      incrflags -
 *
 *
 *      constr_n -
 *
 *
 *      constrattrs -
 *
 *
 *      constrrelops -
 *
 *
 *      constratypes -
 *
 *
 *      constravalues -
 *
 *
 *      p_errh -
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
static uint relcur_update_init(
    void*        cd,
    tb_relcur_t* cur,
    rs_tval_t*   tvalue,
    bool*        selflags,
    bool*        incrflags,
    uint         constr_n __attribute__ ((unused)),
    uint*        constrattrs __attribute__ ((unused)),
    uint*        constrrelops __attribute__ ((unused)),
    rs_atype_t** constratypes __attribute__ ((unused)),
    rs_aval_t**  constravalues __attribute__ ((unused)),
    rs_err_t**   p_errh)
{
        rs_ttype_t* relh_ttype;
        bool        b;
        bool        succp;
        bool        ok_no_truncation;

        ss_dprintf_3(("relcur_update_init:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_dassert(cur->rc_relh != NULL);
        ss_dassert(cur->rc_state == CS_ROW);
        ss_dassert(cur->rc_changestate == CS_CHANGE_INIT);
        ss_dassert(cur->rc_trigctx == NULL);

        if (selflags == NULL) {
            ss_dprintf_2(("relcur_update_init:%ld, E_NOUPDPSEUDOCOL\n", cur->rc_curid));
            rs_error_create(p_errh, E_NOUPDPSEUDOCOL);
            return(TB_CHANGE_ERROR);
        }
        if (cur->rc_aggrcount) {
            ss_dprintf_2(("relcur_update_init:%ld, E_UPDNOCUR\n", cur->rc_curid));
            rs_error_create(p_errh, E_UPDNOCUR);
            return(TB_CHANGE_ERROR);
        }
#ifdef SS_MME
        if (cur->rc_curtype == DBE_CURSOR_SELECT) {
            if (cur->rc_curtype == DBE_CURSOR_SELECT && cur->rc_mainmem) {
                ss_dprintf_2(("relcur_update_init:%ld, E_MMEUPDNEEDSFORUPDATE\n", cur->rc_curid));
                rs_error_create(p_errh, E_MMEUPDNEEDSFORUPDATE);
                return(TB_CHANGE_ERROR);
            }
            if (cur->rc_sqlcall && tb_trans_isrelaxedreacommitted(cd, cur->rc_trans)) {
                ss_dprintf_2(("relcur_update_init:%ld, E_READCOMMITTEDUPDNEEDSFORUPDATE\n", cur->rc_curid));
                rs_error_create(p_errh, E_READCOMMITTEDUPDNEEDSFORUPDATE);
                return(TB_CHANGE_ERROR);
            }
        }
#endif /* SS_MME */

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);

#ifdef SS_DEBUG
        /* Check the consistency so that if there is an increment
           specified that attribute is also selected (= marked as updated)
        */
        if (incrflags != NULL) {
            uint i;
            uint nattrs = rs_ttype_sql_nattrs(cd, relh_ttype);
            for (i = 0; i < nattrs; i++) {
                ss_dassert(incrflags[i] == 0 || incrflags[i] == 1);
                if (incrflags[i] == 1) {
                    ss_dassert(selflags[i] == 1);
                }
            }
        }
        ss_dassert(cur->rc_sellist != NULL);
#endif /* SS_DEBUG */

        cur->rc_trigstr = rs_relh_triggerstr(cd, cur->rc_relh, TB_TRIG_BEFOREUPDATE);
        cur->rc_trigname = rs_relh_triggername(cd, cur->rc_relh, TB_TRIG_BEFOREUPDATE);

        cur->rc_istrigger = cur->rc_trigstr != NULL ||
                            rs_relh_triggerstr(cd, cur->rc_relh, TB_TRIG_AFTERUPDATE) != NULL;
        if (cur->rc_istrigger || rs_relh_issync(cd, cur->rc_relh)) {
            relcur_ensurefulloldtval(cd, cur);
        }
        if (cur->rc_istrigger) {
            cur->rc_isstmtgroup = TRUE;
        }

        if (cur->rc_isstmtgroup) {
            tb_trans_setstmtgroup(cd, cur->rc_trans, TRUE);
            cur_tval_copy(cd, cur);
        }

        /* Make copy of tvalue, so increment and assign operations do not
         * change the parameter value.
         */
        cur->rc_newtval = rs_tval_copy(cd, relh_ttype, cur_tval(cd, cur));
        ss_dassert(cur->rc_newtval != NULL);

        /* Apply the increment operations to specified attributes */
        b = cur_upd_increment(
                cd,
                relh_ttype,
                cur->rc_newtval,
                tvalue,
                incrflags,
                p_errh);
        if (!b) {
            ss_dprintf_2(("relcur_update_init:%ld, ERROR cur_upd_increment\n", cur->rc_curid));
            return(TB_CHANGE_CONSTRFAIL);
        }

        /* Apply the assign operations to specified attributes */
        cur_upd_assign(
            cd,
            relh_ttype,
            cur->rc_newtval,
            tvalue,
            selflags,
            incrflags);

        /* Note !!!!!!!!!!!!!!!!!!
         * if ok_no_truncation == FALSE
         * a warning should be given to client !
         */
        ok_no_truncation = rs_tval_trimchars(
                                cd,
                                relh_ttype,
                                cur->rc_newtval,
                                TRUE);

        cur->rc_phys_selflags = cur_upd_getphysicalselflags(cd, cur, selflags);
        if (cur->rc_phys_selflags == NULL) {
            /* One of the updated columns was a pseudo attribute. */
            ss_dprintf_2(("relcur_update_init:%ld, E_NOUPDPSEUDOCOL\n", cur->rc_curid));
            rs_error_create(p_errh, E_NOUPDPSEUDOCOL);
            return(TB_CHANGE_ERROR);
        }

        /* Check privileges. */
        succp = cur_checkupdatepriv(
                    cd,
                    cur->rc_tbrelh,
                    rs_ttype_nattrs(cd, relh_ttype),
                    cur->rc_phys_selflags);
        if (!succp) {
            ss_dprintf_2(("relcur_update_init:%ld, E_NOPRIV\n", cur->rc_curid));
            rs_error_create(p_errh, E_NOPRIV);
            return(TB_CHANGE_ERROR);
        }

#ifdef SS_SYNC

        if (rs_relh_issync(cd, cur->rc_relh) &&
            tb_trans_getsyncstate(cd, cur->rc_trans, 0)!=TB_TRANS_SYNCST_DISABLE_HISTORY) {

            if (!cur->rc_flow_role_resolved) {
                bool foundp;
                foundp = tb_relh_syncinfo(cd, cur->rc_relh, &cur->rc_ismaster, &cur->rc_isreplica);
                ss_dassert(foundp);
                cur->rc_flow_role_resolved = foundp;
            }
            ss_dassert(cur->rc_flow_role_resolved);
#ifdef SS_DEBUG_FAILS_WITH_SET_SYNC_REPLICA_NO
            ss_dassert(cur->rc_ismaster || cur->rc_isreplica);
#endif

            if (cur->rc_ismaster || 
                 (cur->rc_isreplica &&
                 tb_trans_getsyncstate(cd, cur->rc_trans, 0)!=TB_TRANS_SYNCST_SUBSCRIBEWRITE)) {
                cur->rc_changestate = CS_CHANGE_SYNCHISTORY;
            }

        }

        if (cur->rc_changestate != CS_CHANGE_SYNCHISTORY)
#endif
        {
            if (cur->rc_trigstr != NULL) {
                cur_upd_setupdallcolumns(cd, cur, cur->rc_phys_selflags);
                cur->rc_changestate = CS_CHANGE_BEFORETRIGGER;
            } else {
                cur->rc_changestate = CS_CHANGE_CHECK;
            }
        }

        return(TB_CHANGE_CONT);
}

/*#***********************************************************************\
 *
 *              relcur_update_check
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_errh -
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
static uint relcur_update_check(
    void*        cd,
    tb_relcur_t* cur,
    bool*        selflags,
    bool*        incrflags,
    rs_err_t**   p_errh)
{
        rs_ttype_t* relh_ttype;
        bool        b;
        rs_ano_t    failed_sql_ano;

        ss_dprintf_3(("relcur_update_check:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_dassert(cur->rc_relh != NULL);
        ss_dassert(cur->rc_state == CS_ROW);
        ss_dassert(cur->rc_changestate == CS_CHANGE_CHECK);
        ss_dassert(cur->rc_trigctx == NULL);

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);

        /* Apply the assign operations to specified attributes */
        b = cur_upd_checknotnulls(
                cd,
                relh_ttype,
                cur->rc_newtval,
#ifndef NOT_NOTNULL_ANOARRAY
                rs_relh_notnull_anoarray(cd, cur->rc_relh),
#endif
                selflags,
                incrflags,
                cur->rc_istrigger,
                &failed_sql_ano);
        if (!b) {
            /* NULL value for not null column. */
            ss_dprintf_2(("relcur_update_check:%ld, E_NULLNOTALLOWED_S\n", cur->rc_curid));
            rs_error_create(p_errh, E_NULLNOTALLOWED_S, rs_ttype_sql_aname(cd, relh_ttype, failed_sql_ano));
            return(TB_CHANGE_CONSTRFAIL);
        }

        cur->rc_changestate = CS_CHANGE_EXEC;

        return(TB_CHANGE_CONT);
}

/*#***********************************************************************\
 *
 *              relcur_update_exec
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_errh -
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
static uint relcur_update_exec(
    void*        cd,
    tb_relcur_t* cur,
    rs_err_t**   p_errh)
{
        dbe_trx_t*  trx;
        dbe_ret_t   dbrc;

        ss_dprintf_3(("relcur_update_exec:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_dassert(cur->rc_relh != NULL);
        ss_dassert(cur->rc_state == CS_ROW);
        ss_dassert(cur->rc_changestate == CS_CHANGE_EXEC);
        ss_dassert(cur->rc_trigctx == NULL);

#ifdef SS_SYNC
        /* we are deleting tuples from historytable when handling
           incoming publication data to this replica. if database is
           also master this cleanup is done using update e.g.
           UPDATE _SYNCHIST_T SET SYNC_ISPUBLTUPLE=0 WHERE SYNC_ISPUBLTUPLE=1 AND ...

           trans state TB_TRANS_SYNCST_SUBSC_DELHISTORY is set in snc1rset.c
         */
        if (tb_trans_getsyncstate(cd, cur->rc_trans, 0)==TB_TRANS_SYNCST_SUBSC_DELHISTORY) {
            rs_ttype_t* relh_ttype;
            int index;

            /* relh_ttype = cur->rc_selttype; */
            relh_ttype = rs_relh_ttype(cd, cur->rc_relh);

            ss_dprintf_3(("relcur_update_exec:before patch\n"));
            ss_output_3(rs_tval_print(cd, relh_ttype, cur->rc_newtval));

            /* use this explisit set method. */
            tb_synchist_ispubltuple_to_tval(cd, cur->rc_trans, relh_ttype, cur->rc_newtval, FALSE, FALSE, p_errh);
            ss_dprintf_3(("relcur_update_exec:after patch\n"));
            ss_output_3(rs_tval_print(cd, relh_ttype, cur->rc_newtval));

            index = rs_ttype_anobyname(cd, relh_ttype, (char *)RS_ANAME_SYNC_ISPUBLTUPLE);
            ss_dassert(index >= 0);
            cur->rc_phys_selflags[index] = TRUE;
        }
#endif /* SS_SYNC */

        if (rs_relh_isddopactive(cd, cur->rc_relh)) {
            ss_dprintf_2(("relcur_update_exec:%ld, E_DDOP\n", cur->rc_curid));
            rs_error_create(p_errh, E_DDOP);
            return(TB_CHANGE_ERROR);
        }

        /* Do the update
         */
        trx = tb_trans_dbtrx(cd, cur->rc_trans);
        ss_assert(tb_trans_isactive(cd, cur->rc_trans));

#ifdef JARMOR_UPDATECHANGEDCOLUMNS
        cur_upd_setupdchangedcolumns(
            cd,
            cur,
            cur->rc_phys_selflags,
            cur->rc_newtval);
#endif /* JARMOR_UPDATECHANGEDCOLUMNS */

        tb_trans_stmt_begin(cd, cur->rc_trans);

        dbrc = dbe_cursor_update(
                    cur->rc_dbcur,
                    cur->rc_seltvalue,
                    trx,
                    cur->rc_relh,
                    cur->rc_phys_selflags,
                    cur->rc_newtval,
                    NULL,
                    p_errh);

        switch (dbrc) {
            case DBE_RC_SUCC:
                break;
            case DBE_RC_CONT:
            case DBE_RC_WAITLOCK:
                return(TB_CHANGE_CONT);
            default:
                ss_dprintf_2(("relcur_update_exec:%ld, %s\n", cur->rc_curid, su_rc_nameof(dbrc)));
                return(TB_CHANGE_ERROR);
        }

        cur->rc_changestate = CS_CHANGE_CASCADE;
        return(TB_CHANGE_CONT);
}

static uint relcur_change_cascade_key(
    void*           cd,
    tb_relcur_t*    cur,
    int*            uflags,
    rs_tval_t*      new_tval,
    rs_key_t*       key,
    uint            keyno,
    rs_err_t**      p_errh)
{
        uint rc;
        tb_trans_keyaction_state_t *casc_state = NULL;

        if (cur->rc_casc_states == NULL) {
            /* Lazy states array allocation. */
            cur->rc_casc_states = su_pa_init();
        }

        if (su_pa_indexinuse(cur->rc_casc_states, keyno)) {
            casc_state = su_pa_getdata(cur->rc_casc_states, keyno);
        }

        rc = tb_ref_keyaction(cd, cur->rc_trans,
                              key, cur->rc_selttype,
                              uflags,
                              cur->rc_seltvalue, new_tval,
                              &casc_state,
                              p_errh);
        if (su_pa_indexinuse(cur->rc_casc_states, keyno)) {
            su_pa_remove(cur->rc_casc_states, keyno);
        }
        if (casc_state != NULL) {
            su_pa_insertat(cur->rc_casc_states, keyno, casc_state);
        }
        if (rc == TB_CHANGE_CONT) {
            return rc;
        }
        if (rc != TB_CHANGE_SUCC) {
            cur->rc_casc_key = 0;
            cur->rc_casc_key_delc = TRUE;
        }
        return rc;
}

static uint relcur_change_cascade(
    void*           cd,
    tb_relcur_t*    cur,
    int*            uflags,
    rs_tval_t*      new_tval,
    rs_err_t**      p_errh)
{
        su_pa_t *keys = rs_relh_refkeys(cd, cur->rc_relh);

        for (; cur->rc_casc_key < su_pa_realsize(keys); cur->rc_casc_key++) {
            uint keyno = cur->rc_casc_key;
            rs_key_t *key = su_pa_getdata(keys, keyno);

            /* Forcing the order of updates: cascaded deletes should go first. */
            if (rs_key_delete_action(cd, key) == SQL_REFACT_CASCADE) {
                if (!cur->rc_casc_key_delc) {
                    continue;
                }
            } else {
                if (cur->rc_casc_key_delc) {
                    continue;
                }
            }

            if (key != NULL && rs_key_type(cd, key) == RS_KEY_PRIMKEYCHK) {
                uint rc = relcur_change_cascade_key(
                        cd, cur, uflags, new_tval,
                        key, keyno, p_errh);

                if (rc != TB_CHANGE_SUCC) {
                    return rc;
                }
            }
        }

        if (cur->rc_casc_key_delc) {
            cur->rc_casc_key_delc = FALSE;
            cur->rc_casc_key = 0;
            return TB_CHANGE_CONT;
        }

        if (cur->rc_isstmtgroup) {
            cur->rc_changestate = CS_CHANGE_STMTCOMMIT;
        } else {
            cur->rc_changestate = CS_CHANGE_FREE;
        }

        cur->rc_casc_key = 0;
        cur->rc_casc_key_delc = TRUE;

        return TB_CHANGE_CONT;
}

static uint relcur_change_stmtcommit(
    void*           cd,
    tb_relcur_t*    cur,
        bool            deletep,
    rs_err_t**      p_errh)
{
        uint    retcode;
        bool    succp;
        bool    finishedp;

        ss_dprintf_3(("%s: Inside relcur_update_stmtcommit.\n", __FILE__));
        ss_dassert(cur->rc_changestate == CS_CHANGE_STMTCOMMIT);
        ss_dassert(cur->rc_isstmtgroup);

        succp = tb_trans_stmt_commit(cd, cur->rc_trans, &finishedp, p_errh);
        if (!finishedp) {
            return(TB_CHANGE_CONT);
        }

        if (succp) {
            retcode = TB_CHANGE_CONT;
        } else {
            retcode = TB_CHANGE_ERROR;
        }

        if (deletep) {
            cur->rc_trigstr = rs_relh_triggerstr(cd, cur->rc_relh, TB_TRIG_AFTERDELETE);
            cur->rc_trigname = rs_relh_triggername(cd, cur->rc_relh, TB_TRIG_AFTERDELETE);
        } else {
            cur->rc_trigstr = rs_relh_triggerstr(cd, cur->rc_relh, TB_TRIG_AFTERUPDATE);
            cur->rc_trigname = rs_relh_triggername(cd, cur->rc_relh, TB_TRIG_AFTERUPDATE);
        }

        if (cur->rc_trigstr != NULL) {
            cur->rc_changestate = CS_CHANGE_AFTERTRIGGER;
        } else {
            cur->rc_changestate = CS_CHANGE_FREE;
        }

        return(retcode);
}

/*#***********************************************************************\
 *
 *              relcur_update_trigger
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_errh -
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
static uint relcur_update_trigger(
    void*        cd,
    tb_relcur_t* cur,
        rs_err_t**   p_errh)
{
        int rc;
        rs_ttype_t* relh_ttype;

        ss_dprintf_3(("relcur_update_trigger:%ld\n", cur->rc_curid));
        ss_dassert(cur->rc_changestate == CS_CHANGE_BEFORETRIGGER ||
                   cur->rc_changestate == CS_CHANGE_AFTERTRIGGER);
        ss_dassert(cur->rc_isstmtgroup);

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);

        rc = rs_sysi_trigexec(
                cd,
                cur->rc_trans,
                cur->rc_trigname,
                cur->rc_trigstr,
                relh_ttype,
                cur->rc_seltvalue,
                cur->rc_newtval,
                &cur->rc_trigctx,
                p_errh);

        switch (rc) {
            case 0:
                ss_dprintf_2(("relcur_update_trigger:%ld, %s\n",
                    cur->rc_curid, su_rc_nameof(rs_error_geterrcode(cd, *p_errh))));
                return(TB_CHANGE_ERROR);
            case 1:
                return(TB_CHANGE_CONT);
            case 2:
                break;
            default:
                ss_error;
        }

        if (cur->rc_changestate == CS_CHANGE_BEFORETRIGGER) {
            cur->rc_changestate = CS_CHANGE_CHECK;
            return(TB_CHANGE_CONT);
        } else {
            ss_dassert(cur->rc_changestate == CS_CHANGE_AFTERTRIGGER);
            cur->rc_changestate = CS_CHANGE_FREE;
            return(TB_CHANGE_CONT);
        }
}

#ifdef SS_SYNC
/*#***********************************************************************\
 *
 *              relcur_update_synchistory
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_errh -
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
static uint relcur_update_synchistory(
    void*        cd,
    tb_relcur_t* cur,
        rs_err_t**   p_errh)
{
        rs_relh_t*  sync_relh;
        rs_ttype_t* relh_ttype;
        bool succp;
        uint retcode;
        bool master_addhistory;
        bool ispubltuple;
        bool waspubltuple = FALSE;

        ss_dprintf_3(("relcur_update_synchistory:%ld\n", cur->rc_curid));
        SS_PUSHNAME("relcur_update_synchistory");

        ss_dassert(cur->rc_changestate == CS_CHANGE_SYNCHISTORY);

        if (cur->rc_sih != NULL) {
            retcode = tb_relh_syncinsert_exec(
                            cur->rc_sih,
                            cd,
                            cur->rc_trans,
                            p_errh);

            if (retcode != TB_CHANGE_CONT) {
                tb_relh_syncinsert_done(cur->rc_sih);
                cur->rc_sih = NULL;

                if (retcode != TB_CHANGE_SUCC) {
                    ss_dprintf_3(("relcur_update_synchistory: insert error: %s\n", rs_error_geterrstr(cd, *p_errh)));
                    SS_POPNAME;
                    return(TB_CHANGE_ERROR);
                }

                /* tuples tentative flag is allready set */

                cur_upd_setupdallcolumns(cd, cur, cur->rc_phys_selflags);

                if (cur->rc_trigstr != NULL) {
                    cur->rc_changestate = CS_CHANGE_BEFORETRIGGER;
                } else {
                    cur->rc_changestate = CS_CHANGE_CHECK;
                }
            }

            SS_POPNAME;
            return(TB_CHANGE_CONT);

        }

        ss_dassert(cur->rc_flow_role_resolved);
        ss_dassert(cur->rc_ismaster || cur->rc_isreplica);

        relh_ttype = cur->rc_selttype;
        sync_relh = rs_relh_getsyncrelh(cd, cur->rc_relh);

        /* is original version official or not */
        waspubltuple = tb_synchist_is_tval_publtuple(cd, cur->rc_trans, relh_ttype, cur->rc_seltvalue);

        /* are we handling publication data. if so mark tuple as official.
           (check here! can we never enter here because publication data
            is never updates)
        */
        ispubltuple = (tb_trans_getsyncstate(cd, cur->rc_trans, 0) == TB_TRANS_SYNCST_ISPUBLTUPLE);
        succp = tb_synchist_ispubltuple_to_tval(cd, cur->rc_trans, relh_ttype, cur->rc_newtval, ispubltuple, TRUE, p_errh);
        if (!succp) {
            ss_dprintf_3(("relcur_update_synchistory: error: %s\n", rs_error_geterrstr(cd, *p_errh)));
            SS_POPNAME;
            return(TB_CHANGE_ERROR);
        }

        /* Set new history tuple version to tval.
         */
        if (cur->rc_ismaster) {
            rs_hcol_t* hcol;

            succp = tb_synchist_nextver_to_tval(cd, cur->rc_trans, relh_ttype, cur->rc_newtval, NULL, p_errh);
            if (!succp) {
                ss_dprintf_3(("relcur_update_synchistory: error: %s\n", rs_error_geterrstr(cd, *p_errh)));
                SS_POPNAME;
                return(TB_CHANGE_ERROR);
            }
            master_addhistory = tb_trans_update_synchistory(cd, cur->rc_trans);
            /* master_addhistory = tb_sync_issubscribed(cd, cur->rc_relh, cur->rc_trans); */
            if (master_addhistory) {
                hcol = rs_relh_gethcol(cd, cur->rc_relh);
                ss_dassert(hcol != NULL);
                master_addhistory = rs_hcol_historycolschanged(hcol, cur->rc_phys_selflags);
            }
        } else {
            master_addhistory = FALSE;
            if (waspubltuple) {
                succp = tb_synchist_nextver_to_tval(cd, cur->rc_trans, relh_ttype, cur->rc_newtval, NULL, p_errh);
                if (!succp) {
                    ss_dprintf_3(("relcur_update_synchistory: error: %s\n", rs_error_geterrstr(cd, *p_errh)));
                    SS_POPNAME;
                    return(TB_CHANGE_ERROR);
                }
            }
        }

        /* Add row to the sync history table.
         */
        cur->rc_sih = tb_relh_syncinsert_init(
                    cd,
                    cur->rc_trans,
                    sync_relh,
                    relh_ttype,
                    cur->rc_seltvalue,
                    NULL,
                    waspubltuple,
                    TRUE,
                    master_addhistory,
                    cur->rc_ismaster,
                    cur->rc_isreplica,
                    &retcode,
                    p_errh);

        if (cur->rc_sih == NULL) {

            if (retcode != TB_CHANGE_SUCC) {
                ss_dprintf_3(("relcur_update_synchistory: insert error: %s\n", rs_error_geterrstr(cd, *p_errh)));
                SS_POPNAME;
                return(TB_CHANGE_ERROR);
            }

            /* tuples tentative flag is allready set */

            cur_upd_setupdallcolumns(cd, cur, cur->rc_phys_selflags);

            if (cur->rc_trigstr != NULL) {
                cur->rc_changestate = CS_CHANGE_BEFORETRIGGER;
            } else {
                cur->rc_changestate = CS_CHANGE_CHECK;
            }
        }

        SS_POPNAME;
        return(TB_CHANGE_CONT);
}
#endif /* !SS_MYSQL */
#endif /* SS_SYNC */

/*#***********************************************************************\
 *
 *              relcur_update_free
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
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
static uint relcur_update_free(
    void*        cd,
    tb_relcur_t* cur)
{
        ss_dprintf_3(("relcur_update_free:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_dassert(cur->rc_relh != NULL);
        ss_dassert(cur->rc_trigctx == NULL);

        if (cur->rc_newtval != NULL) {
            rs_ttype_t* relh_ttype;
            relh_ttype = rs_relh_ttype(cd, cur->rc_relh);
            rs_tval_free(cd, relh_ttype, cur->rc_newtval);
            cur->rc_newtval = NULL;
        }

        if (cur->rc_phys_selflags != NULL) {
            SsMemFree(cur->rc_phys_selflags);
            cur->rc_phys_selflags = NULL;
        }

        cur->rc_state = CS_NOROW;
        cur->rc_changestate = CS_CHANGE_INIT;

        return(TB_CHANGE_SUCC);
}

/*##**********************************************************************\
 *
 *              tb_relcur_update
 *
 * Tries to update the current row of a relation cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      tvalue - in, use
 *              tuple value containing new attr values.
 *          The type of tvalue is the type of base table.
 *
 *      selflags - in, use
 *              if non-NULL, a boolean array containing
 *          value 1 for those attributes that should be
 *          updated (allowing in principle the situation
 *          that the other attrs have been updated
 *          meanwhile). If NULL, all the attrs are
 *          updated.
 *          Selflags are indices to the base table type.
 *
 *      incrflags - in, use
 *              if non-NULL, a boolean array containing
 *          value 1 for those attributes that are performed
 *          as incremental updates (the new value is
 *          added to the old value, allowing in principle
 *          the situation that someone else may have
 *          updated the value meanwhile)
 *          Incrflags are indices to the base table type.
 *
 *      constr_n - in
 *              if non-zero, number of constraints specified
 *          with the arrays constrattrs, constrrelops,
 *          constatypes and constavalues. Constraints have
 *          the form
 *
 *              <col n> <relop> <value>
 *
 *      constrattrs - in, use
 *              used only if constr_n is non-zero. Array of
 *          attribute numbers
 *
 *      constrrelops - in, use
 *              used only if constr_n is non-zero. Array of
 *          relational operations (one of RS_RELOP_EQUAL,
 *          RS_RELOP_NOTEQUAL, RS_RELOP_LT, RS_RELOP_GT,
 *          RS_RELOP_LE, RS_RELOP_GE or RS_RELOP_LIKE
 *
 *      constratypes - in, use
 *              used only if constr_n is non-zero. Array of
 *          types for the constant values
 *
 *      constravalues - in, use
 *              used only if constr_n is non-zero. Array of
 *          constant values
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TB_CHANGE_SUCC       (1) if the operation is successful
 *      TB_CHANGE_CONSTRFAIL (2) if one of the constraints was not satisfied
 *      TB_CHANGE_CONT       (3) if the operation did not terminate
 *      TB_CHANGE_ERROR      (0) in case of other errors
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_relcur_update(cd, cur, tvalue, selflags, incrflags,
                      constr_n, constrattrs, constrrelops,
                      constratypes, constravalues, p_errh)
    void*        cd;
    tb_relcur_t* cur;
    rs_tval_t*   tvalue;
    bool*        selflags;
    bool*        incrflags;
    uint         constr_n;
    uint*        constrattrs;
    uint*        constrrelops;
    rs_atype_t** constratypes;
    rs_aval_t**  constravalues;
    rs_err_t**   p_errh;
{
        uint retcode = 0;

        CHK_RELCUR(cur);
        ss_pprintf_1(("tb_relcur_update:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_dassert(cur->rc_relh != NULL);

        switch (cur->rc_state) {

            case CS_NOROW:
            case CS_EMPTY:
            case CS_END:
                ss_pprintf_2(("tb_relcur_update:%ld, E_UPDNOCUR\n", cur->rc_curid));
                rs_error_create(p_errh, E_UPDNOCUR);
                retcode = TB_CHANGE_ERROR;
                break;

            case CS_ERROR:
                ss_pprintf_2(("tb_relcur_update:%ld, CS_ERROR\n", cur->rc_curid));
                retcode = TB_CHANGE_ERROR;
                cursor_geterrh(cur, p_errh);
                break;

            case CS_ROW:
                switch (cur->rc_changestate) {
                    case CS_CHANGE_INIT:
                        ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_INIT\n", cur->rc_curid));
                        retcode = relcur_update_init(
                                    cd,
                                    cur,
                                    tvalue,
                                    selflags,
                                    incrflags,
                                    constr_n,
                                    constrattrs,
                                    constrrelops,
                                    constratypes,
                                    constravalues,
                                    p_errh);
                        if (retcode != TB_CHANGE_CONT ||
                            cur->rc_changestate != CS_CHANGE_CHECK) {
                            break;
                        }
                        /* FALLTHROUGH */
                    case CS_CHANGE_CHECK:
                        ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_CHECK\n", cur->rc_curid));
                        retcode = relcur_update_check(
                                    cd,
                                    cur,
                                    selflags,
                                    incrflags,
                                    p_errh);
                        if (retcode != TB_CHANGE_CONT ||
                            cur->rc_changestate != CS_CHANGE_EXEC) {
                            break;
                        }
                        /* FALLTHROUGH */
                    case CS_CHANGE_EXEC:
                        ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_EXEC\n", cur->rc_curid));
                        ss_pprintf_2(("tb_relcur_update:table %.255s\n", rs_relh_name(cd, cur->rc_relh)));
                        ss_poutput_2(rs_tval_print(cd, cur->rc_selttype, cur->rc_seltvalue));
                        retcode = relcur_update_exec(cd, cur, p_errh);
                        if (retcode != TB_CHANGE_CONT ||
                            cur->rc_changestate != CS_CHANGE_CASCADE) {
                            break;
                        }
                        /* FALLTHROUGH */
                    case CS_CHANGE_CASCADE:
                        ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_CASCADE\n", cur->rc_curid));
                        retcode = relcur_change_cascade(cd, cur, selflags, cur->rc_newtval, p_errh);
                        if (retcode != TB_CHANGE_CONT ||
                            cur->rc_changestate != CS_CHANGE_STMTCOMMIT) {
                            break;
                        }
                        /* FALLTHROUGH */
                    case CS_CHANGE_STMTCOMMIT:
                        ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_STMTCOMMIT\n", cur->rc_curid));
                        retcode = relcur_change_stmtcommit(cd, cur, FALSE, p_errh);
                        break;
                    case CS_CHANGE_BEFORETRIGGER:
                        ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_BEFORETRIGGER\n", cur->rc_curid));
                    case CS_CHANGE_AFTERTRIGGER:
                        ss_dprintf_2(("tb_relcur_update:%ld, CS_CHANGE_AFTERTRIGGER\n", cur->rc_curid));
                        retcode = relcur_update_trigger(cd, cur, p_errh);
                        break;
#ifdef SS_SYNC
#ifndef SS_MYSQL
                    case CS_CHANGE_SYNCHISTORY:
                        ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_SYNCHISTORY\n", cur->rc_curid));
                        retcode = relcur_update_synchistory(cd, cur, p_errh);
                        break;
#endif /* !SS_MYSQL */
#endif
                    case CS_CHANGE_FREE:
                        ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_FREE\n", cur->rc_curid));
                        retcode = TB_CHANGE_SUCC;
                        break;

                    default:
                        ss_error;
                }
                if (cur->rc_changestate == CS_CHANGE_FREE
                    && retcode == TB_CHANGE_CONT) 
                {
                    ss_pprintf_2(("tb_relcur_update:%ld, CS_CHANGE_FREE and TB_CHANGE_CONT, stop\n", cur->rc_curid));
                    retcode = TB_CHANGE_SUCC;
                }
                if (retcode != TB_CHANGE_CONT) {
                    ss_debug(cur->rc_lastcall = (char *)"tb_relcur_update");
                    relcur_update_free(cd, cur);
                }
                break;

            default:
#ifndef AUTOTEST_RUN
                SsErrorMessage(TAB_MSG_BADCURSORSTATE_SD, "tb_relcur_update", cur->rc_state);
#endif
                ss_debug(dbe_db_errorprinttree(rs_sysi_db(cd), TRUE));
                ss_rc_derror(cur->rc_state);
                rs_error_create(p_errh, DBE_ERR_FAILED);
                retcode = TB_CHANGE_ERROR;
                break;
        }
        if (retcode != TB_CHANGE_CONT && cur->rc_isstmtgroup) {
            tb_trans_stmt_begin(cd, cur->rc_trans);
            tb_trans_setstmtgroup(cd, cur->rc_trans, FALSE);
            cur->rc_isstmtgroup = FALSE;
        }
        ss_pprintf_2(("tb_relcur_update:%ld:return %d\n", cur->rc_curid, retcode));
        return(retcode);
}

/*##**********************************************************************\
 *
 *              tb_relcur_update_sql
 *
 * Member of the SQL function block.
 * Tries to update the current row of a relation cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      tvalue - in, use
 *              tuple value containing new attr values.
 *          The type of tvalue is the type of base table.
 *
 *      selflags - in, use
 *              if non-NULL, a boolean array containing
 *          value 1 for those attributes that should be
 *          updated (allowing in principle the situation
 *          that the other attrs have been updated
 *          meanwhile). If NULL, all the attrs are
 *          updated.
 *          Selflags are indices to the base table type.
 *
 *      incrflags - in, use
 *              if non-NULL, a boolean array containing
 *          value 1 for those attributes that are performed
 *          as incremental updates (the new value is
 *          added to the old value, allowing in principle
 *          the situation that someone else may have
 *          updated the value meanwhile)
 *          Incrflags are indices to the base table type.
 *
 *      constr_n - in
 *              if non-zero, number of constraints specified
 *          with the arrays constrattrs, constrrelops,
 *          constatypes and constavalues. Constraints have
 *          the form
 *
 *              <col n> <relop> <value>
 *
 *      constrattrs - in, use
 *              used only if constr_n is non-zero. Array of
 *          attribute numbers
 *
 *      constrrelops - in, use
 *              used only if constr_n is non-zero. Array of
 *          relational operations (one of RS_RELOP_EQUAL,
 *          RS_RELOP_NOTEQUAL, RS_RELOP_LT, RS_RELOP_GT,
 *          RS_RELOP_LE, RS_RELOP_GE or RS_RELOP_LIKE
 *
 *      constratypes - in, use
 *              used only if constr_n is non-zero. Array of
 *          types for the constant values
 *
 *      constravalues - in, use
 *              used only if constr_n is non-zero. Array of
 *          constant values
 *
 *      cont - in out, use
 *          cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TB_CHANGE_SUCC       (1) if the operation is successful
 *      TB_CHANGE_CONSTRFAIL (2) if one of the constraints was not satisfied
 *      TB_CHANGE_ERROR      (0) in case of other errors
 *
 * Limitations  :
 *
 * Globals used :
 */
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
        rs_err_t**   errhandle)
{
        uint ret;

        if (cur->rc_ishurc) {
            return(tb_hurc_update_sql(cd,
                                     TB_CUR_CAST_HURC(cur),
                                     tvalue,
                                     selflags,
                                     incrflags,
                                     constr_n,
                                     constrattrs,
                                     constrrelops,
                                     constratypes,
                                     constravalues,
                                     cont,
                                     errhandle));
        }

        ret = tb_relcur_update( cd,
                                cur,
                                tvalue,
                                selflags,
                                incrflags,
                                constr_n,
                                constrattrs,
                                constrrelops,
                                constratypes,
                                constravalues,
                                errhandle);
        switch( ret ){
            case TB_CHANGE_SUCC :
                /*fallthrough*/
            case TB_CHANGE_CONSTRFAIL :
                *cont = NULL;
                break;
            case TB_CHANGE_CONT :
                *cont = cur;
                ret = TB_CHANGE_SUCC;
                break;
            case TB_CHANGE_ERROR :
                /* fallthrough */
            default:
                ret = TB_CHANGE_ERROR;
                *cont = NULL;
        };
        return( ret );
}

/*#***********************************************************************\
 *
 *              relcur_delete_init
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_errh -
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
static uint relcur_delete_init(
    void*        cd,
    tb_relcur_t* cur,
        rs_err_t**   p_errh)
{
        ss_dprintf_3(("relcur_delete_init:%ld\n", cur->rc_curid));
        ss_dassert(cur->rc_trigctx == NULL);

        if (cur->rc_tbrelh != NULL && !cur_checkdeletepriv(cd, cur->rc_tbrelh)) {
            ss_dprintf_2(("relcur_delete_init:%ld, E_NOPRIV\n", cur->rc_curid));
            rs_error_create(p_errh, E_NOPRIV);
            return(TB_CHANGE_ERROR);
        }
        if (cur->rc_aggrcount) {
            ss_dprintf_2(("relcur_delete_init:%ld, E_DELNOCUR\n", cur->rc_curid));
            rs_error_create(p_errh, E_DELNOCUR);
            return(TB_CHANGE_ERROR);
        }
#ifdef SS_MME
        if (cur->rc_curtype == DBE_CURSOR_SELECT) {
            if (cur->rc_mainmem) {
                ss_dprintf_2(("relcur_delete_init:%ld, E_MMEDELNEEDSFORUPDATE\n", cur->rc_curid));
                rs_error_create(p_errh, E_MMEDELNEEDSFORUPDATE);
                return(TB_CHANGE_ERROR);
            }
            if (cur->rc_sqlcall && tb_trans_isrelaxedreacommitted(cd, cur->rc_trans)) {
                ss_dprintf_2(("relcur_update_init:%ld, E_READCOMMITTEDDELNEEDSFORUPDATE\n", cur->rc_curid));
                rs_error_create(p_errh, E_READCOMMITTEDDELNEEDSFORUPDATE);
                return(TB_CHANGE_ERROR);
            }
        }
#endif /* SS_MME */
        cur->rc_trigstr = rs_relh_triggerstr(cd, cur->rc_relh, TB_TRIG_BEFOREDELETE);
        cur->rc_trigname = rs_relh_triggername(cd, cur->rc_relh, TB_TRIG_BEFOREDELETE);

        cur->rc_istrigger = cur->rc_trigstr != NULL ||
                            rs_relh_triggerstr(cd, cur->rc_relh, TB_TRIG_AFTERDELETE) != NULL;
        if (cur->rc_istrigger || rs_relh_issync(cd, cur->rc_relh)) {
            relcur_ensurefulloldtval(cd, cur);
        }
        if (cur->rc_istrigger) {
            cur->rc_isstmtgroup = TRUE;
        }

        if (cur->rc_isstmtgroup) {
            tb_trans_setstmtgroup(cd, cur->rc_trans, TRUE);
            cur_tval_copy(cd, cur);
        }

#ifdef SS_SYNC
        if (rs_relh_issync(cd, cur->rc_relh) &&
            tb_trans_getsyncstate(cd, cur->rc_trans, 0)!=TB_TRANS_SYNCST_DISABLE_HISTORY) {

            if (!cur->rc_flow_role_resolved) {
                bool foundp;
                foundp = tb_relh_syncinfo(cd, cur->rc_relh, &cur->rc_ismaster, &cur->rc_isreplica);
                ss_dassert(foundp);
                cur->rc_flow_role_resolved = foundp;
            }
            ss_dassert(cur->rc_flow_role_resolved);
#ifdef SS_DEBUG_FAILS_WITH_SET_SYNC_REPLICA_NO
            ss_dassert(cur->rc_ismaster || cur->rc_isreplica);
#endif

            if (cur->rc_ismaster ||
                (cur->rc_isreplica &&
                 tb_trans_getsyncstate(cd, cur->rc_trans, 0)!=TB_TRANS_SYNCST_SUBSCRIBEWRITE)) {
                cur->rc_changestate = CS_CHANGE_SYNCHISTORY;
            }

        }

        if (cur->rc_changestate != CS_CHANGE_SYNCHISTORY)
#endif

        {
            if (cur->rc_trigstr != NULL) {
                cur->rc_changestate = CS_CHANGE_BEFORETRIGGER;
            } else {
                cur->rc_changestate = CS_CHANGE_EXEC;
            }
        }
        return(TB_CHANGE_CONT);
}

/*#***********************************************************************\
 *
 *              relcur_delete_exec
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_errh -
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
static uint relcur_delete_exec(
    void*        cd,
    tb_relcur_t* cur,
        rs_err_t**   p_errh)
{
        dbe_trx_t*  trx;
        dbe_ret_t   rc;

        ss_dprintf_3(("relcur_delete_exec:%ld\n", cur->rc_curid));
        ss_dassert(cur->rc_trigctx == NULL);

        if (rs_relh_isddopactive(cd, cur->rc_relh)) {
            ss_dprintf_2(("relcur_delete_exec:%ld, E_DDOP\n", cur->rc_curid));
            cur->rc_state = CS_NOROW;
            rs_error_create(p_errh, E_DDOP);
            return(TB_CHANGE_ERROR);
        }

        trx = tb_trans_dbtrx(cd, cur->rc_trans);
        ss_assert(tb_trans_isactive(cd, cur->rc_trans));

        tb_trans_stmt_begin(cd, cur->rc_trans);

        rc = dbe_cursor_delete(
                cur->rc_dbcur,
                cur->rc_seltvalue,
                trx,
                cur->rc_relh,
                p_errh);

        switch (rc) {
            case DBE_RC_SUCC:
                break;
            case DBE_RC_CONT:
            case DBE_RC_WAITLOCK:
                return(TB_CHANGE_CONT);
            default:
                ss_dprintf_2(("relcur_delete_exec:%ld, %s\n", cur->rc_curid, su_rc_nameof(rc)));
                cur->rc_state = CS_NOROW;
                return(TB_CHANGE_ERROR);
        }

        cur->rc_changestate = CS_CHANGE_CASCADE;

        return(TB_CHANGE_CONT);
}

/*#***********************************************************************\
 *
 *              relcur_delete_trigger
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_errh -
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
static uint relcur_delete_trigger(
    void*        cd,
    tb_relcur_t* cur,
        rs_err_t**   p_errh)
{
        int rc;
        rs_ttype_t* relh_ttype;
        bool issubscribe = FALSE;

        ss_dprintf_3(("relcur_delete_trigger:%ld\n", cur->rc_curid));
        ss_dassert(cur->rc_changestate == CS_CHANGE_BEFORETRIGGER ||
                   cur->rc_changestate == CS_CHANGE_AFTERTRIGGER);
        ss_dassert(cur->rc_isstmtgroup);

        relh_ttype = rs_relh_ttype(cd, cur->rc_relh);

        issubscribe = rs_sysi_subscribe_write(cd);
        rs_sysi_setsubscribe_write(cd, FALSE);

        rc = rs_sysi_trigexec(
                cd,
                cur->rc_trans,
                cur->rc_trigname,
                cur->rc_trigstr,
                relh_ttype,
                cur->rc_seltvalue,
                NULL,
                &cur->rc_trigctx,
                p_errh);

        rs_sysi_setsubscribe_write(cd, issubscribe);

        switch (rc) {
            case 0:
                ss_dprintf_2(("relcur_delete_trigger:%ld, %s\n",
                    cur->rc_curid, su_rc_nameof(rs_error_geterrcode(cd, *p_errh))));
                return(TB_CHANGE_ERROR);
            case 1:
                return(TB_CHANGE_CONT);
                break;
            case 2:
                break;
            default:
                ss_error;
        }
        if (cur->rc_changestate == CS_CHANGE_BEFORETRIGGER) {
            cur->rc_changestate = CS_CHANGE_EXEC;
        } else {
            cur->rc_changestate = CS_CHANGE_FREE;
        }
        return(TB_CHANGE_CONT);
}

/*#***********************************************************************\
 *
 *              relcur_delete_synchistory
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 *      p_errh -
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
#ifdef SS_SYNC
#ifndef SS_MYSQL
static uint relcur_delete_synchistory(
    void*        cd,
    tb_relcur_t* cur,
        rs_err_t**   p_errh)
{
        rs_relh_t*  sync_relh;
        rs_ttype_t* relh_ttype;
        uint retcode;
        bool waspubltuple = FALSE;

        ss_dprintf_3(("relcur_delete_synchistory:%ld\n", cur->rc_curid));
        SS_PUSHNAME("relcur_delete_synchistory");

        if (!cur->rc_isreplica && !tb_trans_update_synchistory(cd, cur->rc_trans)) {
            if (cur->rc_trigstr != NULL) {
                cur->rc_changestate = CS_CHANGE_BEFORETRIGGER;
            } else {
                cur->rc_changestate = CS_CHANGE_EXEC;
            }

            SS_POPNAME;
            return(TB_CHANGE_CONT);
        }

        ss_dassert(cur->rc_changestate == CS_CHANGE_SYNCHISTORY);
        ss_dassert(cur->rc_ismaster || cur->rc_isreplica);

        if (cur->rc_sih != NULL) {
            retcode = tb_relh_syncinsert_exec(
                            cur->rc_sih,
                            cd,
                            cur->rc_trans,
                            p_errh);

            if (retcode != TB_CHANGE_CONT) {
                tb_relh_syncinsert_done(cur->rc_sih);
                cur->rc_sih = NULL;

                if (retcode != TB_CHANGE_SUCC) {
                    ss_dprintf_3(("relcur_delete_synchistory: insert error: %s\n", rs_error_geterrstr(cd, *p_errh)));
                    SS_POPNAME;
                    return(TB_CHANGE_ERROR);
                }

                if (cur->rc_trigstr != NULL) {
                    cur->rc_changestate = CS_CHANGE_BEFORETRIGGER;
                } else {
                    cur->rc_changestate = CS_CHANGE_EXEC;
                }
            }

            SS_POPNAME;
            return(TB_CHANGE_CONT);

        }

        relh_ttype = cur->rc_selttype;
        sync_relh = rs_relh_getsyncrelh(cd, cur->rc_relh);

        /* is original version official or not */
        waspubltuple = tb_synchist_is_tval_publtuple(cd, cur->rc_trans, relh_ttype, cur->rc_seltvalue);

        cur->rc_sih = tb_relh_syncinsert_init(
                    cd,
                    cur->rc_trans,
                    sync_relh,
                    relh_ttype,
                    cur->rc_seltvalue,
                    NULL,
                    waspubltuple,
                    FALSE,
                    TRUE,
                    cur->rc_ismaster,
                    cur->rc_isreplica,
                    &retcode,
                    p_errh);

        if (cur->rc_sih == NULL) {

            if (retcode != TB_CHANGE_SUCC) {
                ss_dprintf_3(("relcur_delete_synchistory: insert error: %s\n", rs_error_geterrstr(cd, *p_errh)));
                SS_POPNAME;
                return(TB_CHANGE_ERROR);
            }

            if (cur->rc_trigstr != NULL) {
                cur->rc_changestate = CS_CHANGE_BEFORETRIGGER;
            } else {
                cur->rc_changestate = CS_CHANGE_EXEC;
            }
        }

        SS_POPNAME;
        return(TB_CHANGE_CONT);
}
#endif /* !SS_MYSQL */
#endif /* SS_SYNC */

/*##**********************************************************************\
 *
 *              tb_relcur_delete
 *
 * Tries to delete the current row of a cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 *
 * Return value :
 *
 *      TB_CHANGE_SUCC  (1) if the operation is successful
 *      TB_CHANGE_CONT  (3) if the operation did not terminate
 *      TB_CHANGE_ERROR (0) in case of other errors
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_relcur_delete(cd, cur, p_errh)
    void*        cd;
    tb_relcur_t* cur;
    rs_err_t**   p_errh;
{
        uint        retcode = 0;

        CHK_RELCUR(cur);
        ss_pprintf_1(("tb_relcur_delete:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        SS_NOTUSED(p_errh);
        ss_dassert(cur->rc_relh != NULL);

#ifdef SS_FAKE
        if (cur->rc_changestate == CS_CHANGE_INIT) {
            FAKE_CODE_RESET(
                FAKE_HSB_WAIT_BEFORE_DELETE,
                {
                    rs_sysi_eventwaitwithtimeout(
                        cd,
                        22, /* SSE_EVENT_DEBUG_AFTERSTMTEXEC */
                        0,  /* timeout, 0 = forever */
                        NULL,
                        NULL);
                    return(TB_CHANGE_CONT);
                }
            );
        }
#endif /* SS_FAKE */

        switch (cur->rc_state) {

            case CS_NOROW:
            case CS_EMPTY:
            case CS_END:
                ss_pprintf_2(("tb_relcur_delete:%ld, E_DELNOCUR\n", cur->rc_curid));
                rs_error_create(p_errh, E_DELNOCUR);
                retcode = TB_CHANGE_ERROR;
                break;

            case CS_ERROR:
                ss_pprintf_2(("tb_relcur_delete:%ld, CS_ERROR\n", cur->rc_curid));
                retcode = TB_CHANGE_ERROR;
                cursor_geterrh(cur, p_errh);
                break;

            case CS_ROW:
                switch (cur->rc_changestate) {
                    case CS_CHANGE_INIT:
                        retcode = relcur_delete_init(cd, cur, p_errh);
                        if (retcode != TB_CHANGE_CONT ||
                            cur->rc_changestate != CS_CHANGE_EXEC) {
                            break;
                        }
                        /* FALLTHROUGH */
                    case CS_CHANGE_EXEC:
                        ss_pprintf_2(("tb_relcur_delete:table %.255s\n", rs_relh_name(cd, cur->rc_relh)));
                        ss_poutput_2(rs_tval_print(cd, cur->rc_selttype, cur->rc_seltvalue));
                        retcode = relcur_delete_exec(cd, cur, p_errh);
                        if (retcode != TB_CHANGE_CONT ||
                            cur->rc_changestate != CS_CHANGE_CASCADE) {
                            break;
                        }
                        /* FALLTHROUGH */
                    case CS_CHANGE_CASCADE:
                        retcode = relcur_change_cascade(cd, cur, NULL, NULL, p_errh);
                        if (retcode != TB_CHANGE_CONT ||
                            cur->rc_changestate != CS_CHANGE_STMTCOMMIT) {
                            break;
                        }
                        /* FALLTHROUGH */
                    case CS_CHANGE_STMTCOMMIT:
                        retcode = relcur_change_stmtcommit(cd, cur, TRUE, p_errh);
                        break;
                    case CS_CHANGE_BEFORETRIGGER:
                    case CS_CHANGE_AFTERTRIGGER:
                        retcode = relcur_delete_trigger(cd, cur, p_errh);
                        break;
#ifdef SS_SYNC
#ifndef SS_MYSQL
                    case CS_CHANGE_SYNCHISTORY:
                        retcode = relcur_delete_synchistory(cd, cur, p_errh);
                        break;
#endif /* !SS_MYSQL */
#endif
                    case CS_CHANGE_FREE:
                        retcode = TB_CHANGE_SUCC;
                        break;
                    default:
                        ss_error;
                }
                if (cur->rc_changestate == CS_CHANGE_FREE
                    && retcode == TB_CHANGE_CONT) 
                {
                    ss_pprintf_2(("tb_relcur_delete:%ld, CS_CHANGE_FREE and TB_CHANGE_CONT, stop\n", cur->rc_curid));
                    retcode = TB_CHANGE_SUCC;
                }
                if (retcode != TB_CHANGE_CONT) {
                    ss_dassert(cur->rc_trigctx == NULL);
                    cur->rc_changestate = CS_CHANGE_INIT;
                    ss_debug(cur->rc_lastcall = (char *)"tb_relcur_delete");
                }
                break;

            default:
#ifndef AUTOTEST_RUN
                SsErrorMessage(TAB_MSG_BADCURSORSTATE_SD, "tb_relcur_delete", cur->rc_state);
#endif
                ss_debug(dbe_db_errorprinttree(rs_sysi_db(cd), TRUE));
                ss_rc_derror(cur->rc_state);
                rs_error_create(p_errh, DBE_ERR_FAILED);
                retcode = TB_CHANGE_ERROR;
                break;
        }

        if (retcode != TB_CHANGE_CONT && cur->rc_isstmtgroup) {
            tb_trans_stmt_begin(cd, cur->rc_trans);
            tb_trans_setstmtgroup(cd, cur->rc_trans, FALSE);
            cur->rc_isstmtgroup = FALSE;
        }

        ss_dprintf_2(("tb_relcur_delete:%ld:return %d\n", cur->rc_curid, retcode));

        return(retcode);
}


/*##**********************************************************************\
 *
 *              tb_relcur_delete_sql
 *
 * Member of the SQL function block.
 * Tries to delete the current row of a cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      cont - in out, use
 *          cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      errhandle - out, give
 *          in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *          TRUE if operation was successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_relcur_delete_sql(
        void*        cd,
        tb_relcur_t* cur,
        void**       cont,
        rs_err_t**   errhandle)
{
        bool succ = TRUE;
        uint ret;

        if (cur->rc_ishurc) {
            return(tb_hurc_delete_sql(cd,
                                     TB_CUR_CAST_HURC(cur),
                                     cont,
                                     errhandle));
        }

        ret = tb_relcur_delete( cd, cur, errhandle );

        switch( ret ){
            case TB_CHANGE_CONT :
                *cont = cur;
                break;
            case TB_CHANGE_SUCC :
                *cont = NULL;
                break;
            case TB_CHANGE_ERROR :
                /* fallthrough */
            default :
                *cont = NULL;
                succ = FALSE;
                break;
        };

        return( succ );
}

/*##**********************************************************************\
 *
 *              tb_relcur_saupdate
 *
 * Tries to update the current row of a relation cursor in a specified
 * transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      trans - use
 *
 *
 *      tvalue - in, use
 *              tuple value containing new attr values.
 *          The type of tvalue is the type of base table.
 *
 *      selflags - in, use
 *              if non-NULL, a boolean array containing
 *          value 1 for those attributes that should be
 *          updated (allowing in principle the situation
 *          that the other attrs have been updated
 *          meanwhile). If NULL, all the attrs are
 *          updated.
 *          Selflags are indices to the base table type.
 *
 *      tref - in
 *          Tuple reference of the updated tuple.
 *
 *      truncate - in
 *          TRUE if truncation to max. length allowed FALSE when not
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TB_CHANGE_SUCC  (1) if the operation is successful
 *      TB_CHANGE_CONT  (3) if the operation did not terminate
 *      TB_CHANGE_ERROR (0) in case of other errors
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_relcur_saupdate(cd, cur, trans, tvalue, selflags, tref, truncate, p_errh)
    void*        cd;
    tb_relcur_t* cur;
        tb_trans_t*  trans;
    rs_tval_t*   tvalue;
    bool*        selflags;
        void*        tref;
        bool         truncate;
    rs_err_t**   p_errh;
{
        uint            retcode = 0;
        dbe_trx_t*      trx;
        dbe_ret_t       rc;
        bool            succp = TRUE;

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_saupdate:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        ss_dassert(cur->rc_relh != NULL);
        ss_dassert(tvalue != NULL);

        switch (cur->rc_state) {

            case CS_NOROW:
            case CS_EMPTY:
                rs_error_create(p_errh, E_UPDNOCUR);
                retcode = TB_CHANGE_ERROR;
                break;

            case CS_ERROR:
                retcode = TB_CHANGE_ERROR;
                cursor_geterrh(cur, p_errh);
                break;

            case CS_ROW:
            case CS_END:
                if (cur->rc_aggrcount) {
                    rs_error_create(p_errh, E_UPDNOCUR);
                    return(TB_CHANGE_ERROR);
                }
                if (selflags == NULL) {
                    rs_error_create(p_errh, E_NOUPDPSEUDOCOL);
                    return(TB_CHANGE_ERROR);
                }
                cur->rc_phys_selflags = cur_upd_getphysicalselflags(cd, cur, selflags);
                if (cur->rc_phys_selflags == NULL) {
                    /* One of the updated columns was a pseudo attribute. */
                    rs_error_create(p_errh, E_NOUPDPSEUDOCOL);
                    return(TB_CHANGE_ERROR);
                }
                /* Check privileges. */
                succp = cur_checkupdatepriv(
                        cd,
                        cur->rc_tbrelh,
                        rs_ttype_nattrs(cd, rs_relh_ttype(cd, cur->rc_relh)),
                        cur->rc_phys_selflags);
                if (!succp) {
                    ss_dprintf_2(("tb_relcur_saupdate:%ld:E_NOPRIV\n", cur->rc_curid));
                    SsMemFree(cur->rc_phys_selflags);
                    cur->rc_phys_selflags = NULL;
                    rs_error_create(p_errh, E_NOPRIV);
                    return(TB_CHANGE_ERROR);
                }

                if (truncate) {
                    bool ok_no_truncation;
                    ok_no_truncation = rs_tval_trimchars(
                                            cd,
                                            rs_relh_ttype(cd, cur->rc_relh),
                                            tvalue,
                                            TRUE);
                    /* Note !!!!!!!!!!!!!!!!!!
                     * if ok_no_truncation == FALSE
                     * a warning should be given to client !
                     */
                }

                if (trans != NULL) {
                    tb_trans_inheritreadlevel(cd, trans, cur->rc_trans);
                }

                cur->rc_saoldstate = cur->rc_state;
                cur->rc_state = CS_SAUPDATE;
                /* FALLTHROUGH */

            case CS_SAUPDATE:
                if (trans == NULL) {
                    trans = cur->rc_trans;
                }
                trx = tb_trans_dbtrx(cd, trans);
                ss_assert(tb_trans_isactive(cd, cur->rc_trans));

                if (rs_relh_isddopactive(cd, cur->rc_relh)) {
                    rs_error_create(p_errh, E_DDOP);
                    rc = E_DDOP;
                } else {
                    rc = dbe_rel_update(
                            trx,
                            cur->rc_relh,
                            tref,
                            cur->rc_phys_selflags,
                            tvalue,
                            NULL,
                            p_errh);
                }

                switch (rc) {
                    case DBE_RC_SUCC:
                        retcode = TB_CHANGE_SUCC;
                        break;

                    case DBE_RC_WAITLOCK:
                    case DBE_RC_CONT:
                        return(TB_CHANGE_CONT);

                    default:
                        retcode = TB_CHANGE_ERROR;
                        break;
                }

                if (cur->rc_phys_selflags != NULL) {
                    SsMemFree(cur->rc_phys_selflags);
                    cur->rc_phys_selflags = NULL;
                }
                cur->rc_state = cur->rc_saoldstate;
                break;

            default:
                ss_rc_error(cur->rc_state);
        }
        return(retcode);
}

/*##**********************************************************************\
 *
 *              tb_relcur_sadelete
 *
 * Tries to delete the current row of a cursor in a specified transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 *      trans - use
 *
 *
 *      tref - in
 *          Tuple reference of the deleted tuple.
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 *
 * Return value :
 *
 *      TB_CHANGE_SUCC  (1) if the operation is successful
 *      TB_CHANGE_CONT  (3) if the operation did not terminate
 *      TB_CHANGE_ERROR (0) in case of other errors
 *
 * Limitations  :
 *
 * Globals used :
 */
uint tb_relcur_sadelete(cd, cur, trans, tref, p_errh)
    void*        cd;
    tb_relcur_t* cur;
        tb_trans_t*  trans;
        void*        tref;
    rs_err_t**   p_errh;
{
        uint        retcode = 0;
        dbe_trx_t*  trx;
        dbe_ret_t   rc;

        CHK_RELCUR(cur);
        ss_dprintf_1(("tb_relcur_delete:%ld\n", cur->rc_curid));
        SS_NOTUSED(cd);
        SS_NOTUSED(p_errh);
        ss_dassert(cur->rc_relh != NULL);

        switch (cur->rc_state) {

            case CS_NOROW:
            case CS_EMPTY:
                rs_error_create(p_errh, E_DELNOCUR);
                retcode = TB_CHANGE_ERROR;
                break;

            case CS_ERROR:
                retcode = TB_CHANGE_ERROR;
                cursor_geterrh(cur, p_errh);
                break;

            case CS_ROW:
            case CS_END:
                if (!cur_checkdeletepriv(cd, cur->rc_tbrelh)) {
                    ss_dprintf_2(("tb_relcur_sadelete:%ld:E_NOPRIV\n", cur->rc_curid));
                    rs_error_create(p_errh, E_NOPRIV);
                    return(TB_CHANGE_ERROR);
                }
                if (trans != NULL) {
                    tb_trans_inheritreadlevel(cd, trans, cur->rc_trans);
                }

                cur->rc_saoldstate = cur->rc_state;
                cur->rc_state = CS_SADELETE;
                /* FALLTHROUGH */

            case CS_SADELETE:
                if (trans == NULL) {
                    trans = cur->rc_trans;
                }
                if (cur->rc_aggrcount) {
                    rs_error_create(p_errh, E_DELNOCUR);
                    return(TB_CHANGE_ERROR);
                }
                trx = tb_trans_dbtrx(cd, trans);
                ss_assert(tb_trans_isactive(cd, cur->rc_trans));

                if (rs_relh_isddopactive(cd, cur->rc_relh)) {
                    rs_error_create(p_errh, E_DDOP);
                    rc = E_DDOP;
                } else {
                    rc = dbe_rel_delete(
                            trx,
                            cur->rc_relh,
                            tref,
                            p_errh);
                }

                switch (rc) {
                    case DBE_RC_SUCC:
                        retcode = TB_CHANGE_SUCC;
                        break;

                    case DBE_RC_CONT:
                    case DBE_RC_WAITLOCK:
                        return(TB_CHANGE_CONT);

                    default:
                        retcode = TB_CHANGE_ERROR;
                        break;
                }
                cur->rc_state = cur->rc_saoldstate;
                break;

            default:
                ss_rc_error(cur->rc_state);
        }

        return(retcode);
}

/*#***********************************************************************\
 *
 *              cur_ensure_estimate
 *
 * Ensures that the given cursor contains an up to date estimate object.
 * If estimate is missing, it is created. If all information is not
 * available, the estimate cannot be generated and the function
 * returns FALSE.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 * Return value :
 *
 *      TRUE  - estimate created or it it has previously been created
 *      FALSE - estimate could not be created because of lack of
 *              information
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool cur_ensure_estimate(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        su_list_node_t* listnode;
        rs_ob_t* ob;
        tb_est_t* est;
        tb_est_t* desc_est;

        ss_dprintf_3(("cur_ensure_estimate:%ld\n", cur->rc_curid));
        ss_dassert(cur->rc_sellist != NULL);

        if (cur->rc_est != NULL) {
            ss_pprintf_4(("cur_ensure_estimate:already estimate, use old est\n"));
            ss_dassert(tb_est_check(cur->rc_est));
            return(TRUE);
        }

        ss_pprintf_2(("cur_ensure_estimate:create estimate\n"));
        ss_dassert(cur->rc_plan == NULL);

        /* If we create a new estimate we can not use old dbe level
         * cursor.
         */
        if (cur->rc_dbcur != NULL) {
            dbe_cursor_done(cur->rc_dbcur, tb_trans_dbtrx(cd, cur->rc_trans));
            cur->rc_dbcur = NULL;
        }

        /* SIGFPE */
        SS_TRAP_HANDLERSECTION
            case SS_TRAP_FPE_ANY:
                SS_TRAP_QUITHANDLER();
                ss_dprintf_4(("cur_ensure_estimate:%ld: E_ESTARITHERROR\n", cur->rc_curid));
                ss_assert(cur->rc_est == NULL);
                cur->rc_state = CS_ERROR;
                rs_error_create(&cur->rc_errh, E_ESTARITHERROR);
                return(FALSE);
            default:
                ss_error;

        SS_TRAP_RUNSECTION
            SS_PMON_ADD_BETA(SS_PMON_RELCUR_NEWESTIMATE);
            est = tb_est_create_estimate(
                            cd,
                            cur->rc_trans,
                            cur->rc_relh,
                            cur->rc_constr_l,
                            cur->rc_sellist,
                            cur->rc_orderby_l,
                            cur->rc_infolevel,
                            cur->rc_indexhintkey);
            ss_dassert(est != NULL);

            if (cur->rc_indexhintkey == NULL &&
                rs_sqli_usevectorconstr &&
                su_list_length(cur->rc_orderby_l) > tb_est_get_n_order_bys(cd, est)) {
                /* As a special trick reverse order by's and try estimate
                 * also using possible reverse direction.
                 */
                uint n_order_bys = tb_est_get_n_order_bys(cd, est);
                su_list_do_get(cur->rc_orderby_l, listnode, ob) {
                    if (rs_ob_asc(cd, ob)) {
                        rs_ob_setdesc(cd, ob);
                    } else {
                        rs_ob_setasc(cd, ob);
                    }
                }
                ss_dprintf_4(("cur_ensure_estimate:%ld, try to reverse result set\n", cur->rc_curid));
                desc_est = tb_est_create_estimate(
                                cd,
                                cur->rc_trans,
                                cur->rc_relh,
                                cur->rc_constr_l,
                                cur->rc_sellist,
                                cur->rc_orderby_l,
                                cur->rc_infolevel,
                                cur->rc_indexhintkey);
                ss_dassert(desc_est != NULL);
                if (tb_est_get_n_order_bys(cd, desc_est) > n_order_bys) {
                    /* We get better order by using new estimate.
                     */
                    ss_dprintf_4(("cur_ensure_estimate:%ld, use reversed result set\n", cur->rc_curid));
                    tb_est_free_estimate(cd, est);
                    est = desc_est;
                    cur->rc_reverseset = TB_REVERSESET_VECTOR;
                } else {
                    ss_dprintf_4(("cur_ensure_estimate:%ld, use original result set\n", cur->rc_curid));
                    su_list_do_get(cur->rc_orderby_l, listnode, ob) {
                        if (rs_ob_asc(cd, ob)) {
                            rs_ob_setdesc(cd, ob);
                        } else {
                            rs_ob_setasc(cd, ob);
                        }
                    }
                    tb_est_free_estimate(cd, desc_est);
                }
            } else if (cur->rc_indexhintkey == NULL &&
                       su_list_length(cur->rc_orderby_l) > 0 &&
                       tb_est_get_n_order_bys(cd, est) == 0) {
                /* Try to change all descending orders to ascending order.
                 */
                bool alldesc;

                alldesc = TRUE;
                su_list_do_get(cur->rc_orderby_l, listnode, ob) {
                    if (rs_ob_asc(cd, ob)) {
                        alldesc = FALSE;
                        break;
                    }
                }
                if (alldesc) {
                    /* All order by's are in descending order, change them
                     * to ascending order.
                     */
                    ss_dprintf_4(("cur_ensure_estimate:%ld, try to reverse result set\n", cur->rc_curid));
                    su_list_do_get(cur->rc_orderby_l, listnode, ob) {
                        rs_ob_setasc(cd, ob);
                    }
                    desc_est = tb_est_create_estimate(
                                    cd,
                                    cur->rc_trans,
                                    cur->rc_relh,
                                    cur->rc_constr_l,
                                    cur->rc_sellist,
                                    cur->rc_orderby_l,
                                    cur->rc_infolevel,
                                    cur->rc_indexhintkey);
                    ss_dassert(desc_est != NULL);
                    if (tb_est_get_n_order_bys(cd, desc_est) > 0) {
                        /* We get better order by using new estimate.
                         */
                        ss_dprintf_4(("cur_ensure_estimate:%ld, use reversed result set\n", cur->rc_curid));
                        tb_est_free_estimate(cd, est);
                        est = desc_est;
                        cur->rc_reverseset = TB_REVERSESET_NORMAL;
                    } else {
                        ss_dprintf_4(("cur_ensure_estimate:%ld, use original result set\n", cur->rc_curid));
                        su_list_do_get(cur->rc_orderby_l, listnode, ob) {
                            rs_ob_setdesc(cd, ob);
                        }
                        tb_est_free_estimate(cd, desc_est);
                    }
                }
            }
            ss_dassert(tb_est_check(est));
            cur->rc_est = est;
            ss_dassert(cur->rc_est != NULL);

            relcur_printestimate(cd, cur, FALSE);
        SS_TRAP_END

        ss_dassert(tb_est_check(cur->rc_est));

        return(TRUE);
}

/*#***********************************************************************\
 *
 *              cur_ensure_search_plan
 *
 * Ensures that the given cursor contains an up to date search plan object.
 * If search plan is missing, it is created.
 *
 * We assume that the cost estimate object is already created, because
 * from there we can get the best key that is used in search plan.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in out, use
 *              relation cursor pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void cur_ensure_search_plan(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        ss_dassert(cur->rc_est != NULL);
        ss_dassert(tb_est_check(cur->rc_est));

        if (cur->rc_plan == NULL || cur->rc_plan_reset) {
            rs_key_t* best_key;
            su_list_node_t* n;
            rs_cons_t* cons;
#ifdef SS_MYSQL_PERFCOUNT
            __int64 startcount;
            __int64 endcount;
            bool plan_create = cur->rc_plan == NULL;

            if (mysql_enable_perfcount > 1) {
                QueryPerformanceCounter((LARGE_INTEGER*)&startcount);
            }
#endif /* SS_MYSQL_PERFCOUNT */

            ss_pprintf_3(("cur_ensure_search_plan:%ld:create a new plan\n", cur->rc_curid));

            cur->rc_plan_reset = FALSE;

            best_key = tb_est_get_key(cd, cur->rc_est);
            if (cur->rc_plankey != best_key && cur->rc_plankey != NULL) {
                rs_key_done(cd, cur->rc_plankey);
                cur->rc_plankey = NULL;
            }
            if (cur->rc_plankey == NULL) {
                cur->rc_plankey = best_key;
                rs_key_link(cd, cur->rc_plankey);
            }

            cur->rc_plan = tb_pla_create_search_plan(
                                cd,
                                cur->rc_plan,
                                cur->rc_relh,
                                best_key,
                                cur->rc_constr_l,
                                tb_est_get_cons_byano(cd, cur->rc_est),
                                cur->rc_sellist,
                                FALSE
                          );
            ss_dassert(cur->rc_plan != NULL);

            /* Set plan consistent before checking the constraints.
             */
            rs_pla_setconsistent_once(cd, cur->rc_plan, TRUE);

            su_list_do_get(cur->rc_constr_l, n, cons) {
                if (rs_cons_isalwaysfalse_once(cd, cons)) {
                    rs_pla_setconsistent_once(cd, cur->rc_plan, FALSE);
                }
            }
#ifdef SS_MYSQL_PERFCOUNT
            if (mysql_enable_perfcount > 1) {
                QueryPerformanceCounter((LARGE_INTEGER*)&endcount);
                if (plan_create) {
                    tb_pla_create_perfcount += endcount - startcount;
                    tb_pla_create_callcount++;
                } else {
                    tb_pla_reset_perfcount += endcount - startcount;
                    tb_pla_reset_callcount++;
                }
            }
#endif /* SS_MYSQL_PERFCOUNT */

        } else {
            ss_pprintf_3(("cur_ensure_search_plan:%ld:use old plan\n", cur->rc_curid));
        }
}

/*#***********************************************************************\
 *
 *              cur_upd_increment
 *
 * Perform the increment operations specified in cursor_update.
 * Updates the cursor's seltvalue by adding the marked attributes in
 * 'tvalue' to it.
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      ttype - in, use
 *              ttype of upd_tval and inc_tval
 *
 *      upd_tval - in, use
 *              tval containing the attribute values to be updated by
 *          incrementing
 *
 *      inc_tval - in, use
 *              tval containing the attribute values to be
 *          added to marked attributes of upd_tval
 *
 *      incrflags - in, use
 *              Array containing 1 for every attribute that is to be
 *          updated by incrementing
 *          see the comment in relcur_update
 *
 * Return value :
 *
 *      TRUE, if succeeded
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool cur_upd_increment(cd, ttype, upd_tval, inc_tval, incrflags, p_errh)
        void*        cd __attribute__ ((unused));
        rs_ttype_t*  ttype __attribute__ ((unused));
        rs_tval_t*   upd_tval __attribute__ ((unused));
        rs_tval_t*   inc_tval __attribute__ ((unused));
        bool*        incrflags;
        rs_err_t**   p_errh __attribute__ ((unused));
{
        ss_dprintf_3(("cur_upd_increment\n"));

        if (incrflags != NULL) {
#ifdef SS_MYSQL
            ss_error;
#else /* SS_MYSQL */
            rs_ano_t ano;
            rs_ano_t attr_n;

            attr_n = rs_ttype_sql_nattrs(cd, ttype);

            for (ano = 0; ano < attr_n; ano++) {
                if (incrflags[ano]) {
                    /* There is an increment flag specified for this
                     * attribute.
                     * We sum the attribute value of given tvalue (av) to the
                     * corresponding attribute value in the cursor's
                     * current tuple.
                     */
                    rs_atype_t* at = rs_ttype_sql_atype(cd, ttype, ano);
                    rs_aval_t*  upd_av = rs_tval_sql_aval(cd, ttype, upd_tval, ano);
                    rs_aval_t*  inc_av = rs_tval_sql_aval(cd, ttype, inc_tval, ano);
                    rs_aval_t*  res_av = NULL;
                    rs_atype_t* res_at = NULL;
                    bool        b;

                    b = rs_aval_arith(
                            cd,
                            &res_at,
                            &res_av,
                            at,
                            upd_av,
                            at,
                            inc_av,
                            RS_AROP_PLUS,
                            p_errh);
                    if (!b) {
                        if (res_av != NULL) {
                            rs_aval_free(cd, res_at, res_av);
                        }
                        if (res_at != NULL) {
                            rs_atype_free(cd, res_at);
                        }
                        return(FALSE);
                    }
                    /* Assign result to the upd_tval.
                     */
                    b = rs_aval_assign(
                            cd,
                            at,
                            upd_av,
                            res_at,
                            res_av,
                            p_errh);
                    rs_aval_free(cd, res_at, res_av);
                    rs_atype_free(cd, res_at);
                    if (!b) {
                        return(FALSE);
                    }
                }
            }
#endif /* SS_MYSQL */
        }
        return(TRUE);
}

/*#***********************************************************************\
 *
 *              cur_upd_assign
 *
 * Assign the selected attribute values from new_tval to upd_tval.
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      upd_tval - out, give
 *              tvalue to be updated
 *
 *      new_tval - in, use
 *              new values for attributes
 *
 *      selflags - in, use
 *              Array containing 1 for every attribute that is to be
 *          updated either by incrementing or assigning. Only attributes
 *          not containing an increment flag are assigned.
 *
 *      incrflags - in, use
 *              Array containing 1 for every attribute that is to be
 *          updated by incrementing. Those attributes are not touched here.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void cur_upd_assign(
        void*        cd,
        rs_ttype_t*  ttype,
    rs_tval_t*   upd_tval,
    rs_tval_t*   new_tval,
    bool*        selflags,
    bool*        incrflags)
{
        rs_ano_t ano;
        rs_ano_t attr_n;

        ss_dprintf_3(("cur_upd_assign\n"));
        ss_dassert(selflags != NULL);

        attr_n = rs_ttype_sql_nattrs(cd, ttype);

        for (ano = 0; ano < attr_n; ano++) {
            if (selflags[ano] && (incrflags == NULL || !incrflags[ano])) {
                rs_aval_t* new_aval;
                ss_dprintf_3(("cur_upd_assign: ano %d\n", (int)ano));
                new_aval = rs_tval_sql_aval(cd, ttype, new_tval, ano);
                rs_tval_sql_setaval(cd, ttype, upd_tval, ano, new_aval);
            }
        }
}

/*#***********************************************************************\
 *
 *              cur_upd_checknotnulls
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ttype -
 *
 *
 *      new_tval -
 *
 *
 *      p_failed_sql_ano -
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
static bool cur_upd_checknotnulls(
        void*        cd __attribute__ ((unused)),
        rs_ttype_t*  ttype,
        rs_tval_t*   new_tval,
#ifndef NOT_NOTNULL_ANOARRAY
        rs_ano_t*    notnull_anos,
#endif
        bool*        selflags,
        bool*        incrflags,
        bool         triggerp __attribute__ ((unused)),
        bool*        p_failed_sql_ano)
{

#ifndef NOT_NOTNULL_ANOARRAY
        rs_ano_t ano;
        int i;

        ss_dassert(notnull_anos != NULL);
        i = 0;
        while ((ano = notnull_anos[i]) != RS_ANO_NULL) {
            if ( /* triggerp */ selflags[ano] && (incrflags == NULL || !incrflags[ano])) {
                rs_atype_t* atype;
                rs_aval_t* aval;
                atype = rs_ttype_atype(cd, ttype, ano);
                aval = rs_tval_aval(cd, ttype, new_tval, ano);
                if (rs_aval_isnull(cd, atype, aval)) {
                    /* Null not allowed. */
                    *p_failed_sql_ano = ano;
                    return(FALSE);
                }
            }
            i++;
        }
#else /* NOT_NOTNULL_ANOARRAY */
        rs_ano_t ano;
        rs_ano_t attr_n = rs_ttype_sql_nattrs(cd, ttype);

        ss_dprintf_3(("cur_upd_checknotnulls\n"));

        for (ano = 0; ano < attr_n; ano++) {
            if (triggerp ||
                (selflags[ano] && (incrflags == NULL || !incrflags[ano]))) {
                rs_atype_t* atype;
                rs_aval_t* aval;
                atype = rs_ttype_atype(cd, ttype, ano);
                if (rs_atype_attrtype(cd, atype) == RSAT_USER_DEFINED &&
                    !rs_atype_nullallowed(cd, atype)) {
                    aval = rs_tval_aval(cd, ttype, new_tval, ano);
                    if (rs_aval_isnull(cd, atype, aval)) {
                        /* Null not allowed. */
                        *p_failed_sql_ano = ano;
                        return(FALSE);
                    }
                }
            }
        }
#endif /* NOT_NOTNULL_ANOARRAY */

        return(TRUE);
}

#if 0
/* My change */
/*#***********************************************************************\
 *
 *              cur_upd_constraints
 *
 * Checks if the new tval meets all the constraints.
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      ttype - in, use
 *
 *
 *      tvalue - in, use
 *
 *
 *      constr_n - in
 *
 *
 *      constrattrs - in, use
 *
 *
 *      constrrelops - in, use
 *
 *
 *      constratypes - in, use
 *
 *
 *      constravalues - in, use
 *
 *      p_failed_sql_ano - out
 *
 *
 * Return value :
 *      TRUE, if given tvalue meets all the given constraints
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool cur_upd_constraints(
        void*        cd,
    rs_ttype_t*  ttype,
    rs_tval_t*   tvalue,
    uint         constr_n,
    uint*        constrattrs,
    uint*        constrrelops,
    rs_atype_t** constratypes,
    rs_aval_t**  constravalues,
    rs_ano_t*    p_failed_sql_ano)
{
        uint i;

        /* optimise this! */
        for (i = 0; i < constr_n; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = rs_ttype_sql_atype(cd, ttype, constrattrs[i]);
            aval = rs_tval_sql_aval(cd, ttype, tvalue, constrattrs[i]);

            if (!rs_aval_isnull(cd, atype, aval)) {
                bool passed;
                passed = rs_aval_cmp(
                            cd,
                            atype,
                            aval,
                            constratypes[i],
                            constravalues[i],
                            constrrelops[i]);
                if (!passed) {
                    *p_failed_sql_ano = constrattrs[i];
                    return(FALSE);
                }
            }
        }

        return(TRUE);
}
#endif


/*#***********************************************************************\
 *
 *              cur_upd_getphysicalselflags
 *
 * Creates a boolean array containing physical selflags.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in, use
 *              pointer to relation cursor
 *
 *      sqlselflags - in, use
 *              sql's opinion on the selflag array.
 *          NULL means all the attributes.
 *
 * Return value - give :
 *
 *      Pointer to physical selflag array. Can be freed using SsMemFree.
 *      NULL if not allowed to update the attributes, e.g. trying to
 *      update pseudo attribute.
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool* cur_upd_getphysicalselflags(cd, cur, sqlselflags)
    void*        cd;
    tb_relcur_t* cur;
    bool*        sqlselflags;
{
        rs_ano_t    n_physflags;
        rs_ano_t    n_sqlflags;
        bool*       phys_selflags;
        rs_ano_t    ano;
        rs_ttype_t* ttype;

        ss_dassert(sqlselflags != NULL);

        ttype = rs_relh_ttype(cd, cur->rc_relh);
        n_sqlflags = rs_ttype_sql_nattrs(cd, ttype);
        n_physflags = rs_ttype_nattrs(cd, ttype);

        /* Allocate phys_selflags and initialize to FALSE.
         */
        phys_selflags = SsMemCalloc(n_physflags, sizeof(bool));

        ss_dprintf_3(("cur_upd_getphysicalselflags:sqlselflags="));

        /* Now we set the right ones to TRUE.
         */
        for (ano = 0; ano < n_sqlflags; ano++) {
            ss_dprintf_3(("%d ", sqlselflags[ano]));
            if (sqlselflags[ano]) {
                rs_atype_t* atype;
                rs_ano_t phys_ano = rs_ttype_sqlanotophys(cd, ttype, ano);
                atype = rs_ttype_atype(cd, ttype, phys_ano);
#ifdef SS_SYNC
                if (rs_atype_syncpublinsert_pseudo(cd, atype)) {
#else
                if (rs_atype_pseudo(cd, atype)) {
#endif
                    /* It is illegal to update a pseudo attribute. */
                    SsMemFree(phys_selflags);
                    return(NULL);
                }
                phys_selflags[phys_ano] = TRUE;
            }
        }
        ss_dprintf_3(("\n"));

        return(phys_selflags);
}

/*#***********************************************************************\
 *
 *              cur_tval
 *
 * Returns the current tuple value of the cursor
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in, use
 *              pointer to relation cursor
 *
 * Return value - ref :
 *
 *     pointer to tval
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_tval_t* cur_tval(cd, cur)
        void*        cd;
        tb_relcur_t* cur;
{
        SS_NOTUSED(cd);
        ss_dassert(cur->rc_sellist != NULL);

        return(cur->rc_seltvalue);
}

/*#***********************************************************************\
 *
 *              cur_tval_free
 *
 * Frees the current tuple value of the cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cur - in, use
 *              pointer to relation cursor
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void cur_tval_free(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        if (cur->rc_seltvalue != NULL) {

            ss_dassert(cur->rc_sellist != NULL);
            ss_dassert(cur->rc_selttype != NULL);

#if defined(SS_MME) && defined(MME_SEARCH_KEEP_TVAL)
            /* For MME searches, the result tval is held and freed by
               the index level search. */
            if (!cur->rc_mainmem
                || cur->rc_valueiscopied) {
                rs_tval_free(cd, cur->rc_selttype, cur->rc_seltvalue);
            }
            cur->rc_valueiscopied = FALSE;
#else
            rs_tval_free(cd, cur->rc_selttype, cur->rc_seltvalue);
#endif

            cur->rc_seltvalue = NULL;
        }
}

/*#***********************************************************************\
 *
 *              cur_tval_freeif
 *
 * Releases current tval if usecount is greater than one which means that
 * someone else owns the tval.
 *
 * Parameters :
 *
 *              cd - in
 *
 *
 *              cur - in, use
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
SS_INLINE void cur_tval_freeif(
        void*        cd,
        tb_relcur_t* cur)
{
        if (cur->rc_seltvalue != NULL
        &&  rs_tval_usecount(cd, cur->rc_selttype, cur->rc_seltvalue) > 1)
        {
            cur_tval_free(cd, cur);
        }
}

static void cur_tval_copy(cd, cur)
    void*        cd;
    tb_relcur_t* cur;
{
        if (cur->rc_seltvalue != NULL) {

            ss_dassert(cur->rc_sellist != NULL);
            ss_dassert(cur->rc_selttype != NULL);

#if defined(SS_MME) && defined(MME_SEARCH_KEEP_TVAL)
            /* For MME searches, the result tval is held and freed by
               the index level search. */
            if (cur->rc_mainmem
                && !cur->rc_valueiscopied) {
                cur->rc_seltvalue = rs_tval_copy(cd, cur->rc_selttype,
                                                 cur->rc_seltvalue);
                cur->rc_valueiscopied = TRUE;
            }
#endif
        }
}

/*#***********************************************************************\
 *
 *              cur_reset
 *
 * Sets a cursor in its initial state. Reopens the database cursor.
 *
 * Parameters :
 *
 *      cd - in, use
 *          Client data
 *
 *      cur - in out, use
 *              pointer to relation cursor
 *
 * Return value :
 *      TRUE, if succeeded
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool cur_reset(void* cd, tb_relcur_t* cur)
{
        /* DANGER!!
           This is selectively copied from relcur_free.
           Here we prepare the cursor so that it looks like we were newly
           opening it.
        */

        ss_dprintf_3(("cur_reset:%ld\n", cur->rc_curid));
        SS_PUSHNAME("cur_reset");

        ss_beta(relcur_updateestinfo(cd, cur));

        rs_vbuf_reset(cd, cur->rc_vbuf);
        cur_tval_free(cd, cur);

        cur_ensure_estimate(cd, cur);
        cur_ensure_search_plan(cd, cur);

        if (cur->rc_dbcur != NULL && !cur->rc_canuseindexcursorreset) {
            dbe_cursor_done(cur->rc_dbcur, tb_trans_dbtrx(cd, cur->rc_trans));
            cur->rc_dbcur = NULL;
        }
        cur->rc_dbtref = NULL;

        {
            dbe_trx_t*  trx;

            if (!rs_pla_isconsistent(cd, cur->rc_plan)) {
                rs_pla_done(cd, cur->rc_plan);
                cur->rc_plan = NULL;
                cur->rc_state = CS_EMPTY;
                SS_POPNAME;
                return(TRUE);
            }

#if 0
            if (!tb_trans_isactive(cd, cur->rc_trans)) {
                cur->rc_state = CS_ERROR;
                rs_error_create(&cur->rc_errh, E_TRANSNOTACT);
                SS_POPNAME;
                return(FALSE);
            }
#else
            tb_trans_beginif(cd, cur->rc_trans);
#endif

            trx = tb_trans_dbtrx(cd, cur->rc_trans);

            if (cur->rc_dbcur != NULL && cur->rc_canuseindexcursorreset) {
                ss_dprintf_4(("cur_reset:reset old dbe cursor\n"));
                /* XXX Content (and not just values) of constraint list
                 * is changed even with simple cons.
                 */
                dbe_cursor_reset(
                    cur->rc_dbcur,
                    trx,
                    cur->rc_selttype,
                    cur->rc_sellist,
                    cur->rc_plan);
            } else {
                ss_dprintf_4(("cur_reset:create new dbe cursor\n"));
                ss_dassert(cur->rc_dbcur == NULL); /* Memory leak? */
                cur->rc_dbcur = dbe_cursor_init(
                                    trx,
                                    cur->rc_selttype,
                                    cur->rc_sellist,
                                    cur->rc_plan,
                                    cur->rc_curtype,
                                    cur->rc_p_newplan,
                                    NULL);
                if (cur->rc_dbcur == NULL) {
                    ss_derror;  /* JarmoR Feb 3, 1998 */
                    rs_pla_done(cd, cur->rc_plan);
                    cur->rc_plan = NULL;
                    cur->rc_state = CS_EMPTY;
                    SS_POPNAME;
                    return(TRUE);
                } else if (cur->rc_optcount != 1) {
                    dbe_cursor_setoptinfo(
                        cur->rc_dbcur,
                        cur->rc_optcount);
                }
                dbe_cursor_setisolation_transparent(cur->rc_dbcur, cur->rc_isolationchange_transparent);
            }
        }

        {
            rs_key_t* cur_key;

            cur_key = tb_est_get_key(cd, cur->rc_est);

            if (rs_relh_isaborted(cd, cur->rc_relh) ||
                rs_key_isaborted(cd, cur_key)) {

                if (cur->rc_errh != NULL) {
                    rs_error_free(cd, cur->rc_errh);
                    cur->rc_errh = NULL;
                }

                rs_error_create(&cur->rc_errh, E_DDOP);
                cur->rc_state = CS_ERROR;
                SS_POPNAME;
                return(TRUE);
            }
        }
        cur->rc_state = CS_START;
        SS_POPNAME;
        return(TRUE);
}


void tb_relcur_setisolation_transparent(
        void*        cd,
        tb_relcur_t* cur,
        bool transparent)
{
        CHK_RELCUR(cur);
        SS_NOTUSED(cd);
        cur->rc_isolationchange_transparent = transparent;
        if (cur->rc_dbcur != NULL) {
            dbe_cursor_setisolation_transparent(cur->rc_dbcur, cur->rc_isolationchange_transparent);
        }        
}

/*##**********************************************************************\
 *
 *              tb_relcur_trans
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
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
tb_trans_t* tb_relcur_trans(
        void*        cd,
        tb_relcur_t* cur)
{
        CHK_RELCUR(cur);
        SS_NOTUSED(cd);
        return(cur->rc_trans);
}

/*##**********************************************************************\
 *
 *              tb_relcur_copytref
 *
 * Makes a copy of the tuple reference of the current tuple in search.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
 *
 *
 * Return value - give :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void* tb_relcur_copytref(
        void*        cd,
        tb_relcur_t* cur)
{
        CHK_RELCUR(cur);
        ss_dassert(cur->rc_state == CS_ROW);

        cur_ensure_dbtref(cd, cur);

        return(dbe_tref_copy(cd, cur->rc_dbtref));
}

/*##**********************************************************************\
 *
 *              tb_relcur_gettref
 *
 * Returns the tuple reference of the current tuple in search.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cur -
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
void* tb_relcur_gettref(
        void*        cd,
        tb_relcur_t* cur)
{
        CHK_RELCUR(cur);
        ss_dassert(cur->rc_state == CS_ROW);

        cur_ensure_dbtref(cd, cur);

        return(cur->rc_dbtref);
}

#ifdef SS_SYNC
void tb_relcur_setsynchistorydeleted(
        void*        cd __attribute__ ((unused)),
        tb_relcur_t* cur,
        bool         yes_or_no)
{
        CHK_RELCUR(cur);
        cur->rc_issynchist = yes_or_no;
}
#endif

#ifdef SS_QUICKSEARCH
void* tb_relcur_getquicksearch(
        void*        cd,
        tb_relcur_t* cur,
        bool         longsearch)
{
        ss_dassert(cur->rc_state != CS_CLOSED);

        return(dbe_cursor_getquicksearch(cur->rc_dbcur, longsearch));
}
#endif /* SS_QUICKSEARCH */

#ifdef SS_BETA

static void tb_relc_estinfolist_delete(void* data)
{
        SsMemFree(data);
}

void tb_relc_estinfolist_init(void)
{
        tb_relc_estinfolist = su_list_init(tb_relc_estinfolist_delete);
}

void tb_relc_estinfolist_done(void)
{
        if (tb_relc_estinfolist != NULL) {
            su_list_done(tb_relc_estinfolist);
        }
}

void tb_relc_estinfolist_setoutfp(
        void (*outfp)(
            char* tablename,
            tb_relc_estinfo_t* estinfo))
{
        relc_estinfooutfp = outfp;
}

#endif /* SS_BETA */
