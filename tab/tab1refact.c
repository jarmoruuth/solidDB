/*************************************************************************\
**  source       * tab1refact.c
**  directory    * tab
**  description  * Referential integrity actions
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

#include <rs0key.h>
#include <rs0tval.h>

#include "tab0relc.h"
#include "tab0relh.h"

#include "tab1dd.h"

#include "tab1refact.h"

typedef enum {
        TB_TRANS_KEYACTION_LOCKREL,
        TB_TRANS_KEYACTION_PREPARE,
        TB_TRANS_KEYACTION_SELECT,
        TB_TRANS_KEYACTION_OPEN,
        TB_TRANS_KEYACTION_DELETE,
        TB_TRANS_KEYACTION_UPDATE
} tb_trans_keyaction_state_no_t;

struct tb_trans_keyaction_state {
        tb_trans_keyaction_state_no_t   tb_trans_keyaction_state_no;
        rs_relh_t*                      tb_trans_keyaction_relh;
        tb_relh_t*                      tb_trans_keyaction_tbrel;
        tb_relpriv_t*                   tb_trans_keyaction_priv;
        int*                            tb_trans_keyaction_selattrs;
        bool*                           tb_trans_keyaction_selflags;
        rs_ttype_t*                     tb_trans_keyaction_forttype;
        forupdate_t                     tb_trans_keyaction_forupdate;
        tb_relcur_t*                    tb_trans_keyaction_relcur;
        rs_key_t*                       tb_trans_keyaction_forkey;
        rs_tval_t*                      tb_trans_keyaction_rtval;
        rs_aval_t**                     tb_trans_keyaction_avals;
        rs_key_t*                       tb_trans_keyaction_refkey;
        rs_ttype_t*                     tb_trans_keyaction_refttype;
};

/*#***********************************************************************\
 *
 *              tb_ref_alloc_state
 *
 * Allocate ref.actions state machine.
 *
 * Parameters :
 *
 *      cd -
 *
 *      trans -
 *
 *      refkey  - the key
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static tb_trans_keyaction_state_t* tb_ref_alloc_state(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        rs_key_t*       refkey,
        rs_ttype_t*     ttype,
        int             action,
        bool            delete,
        rs_err_t**      p_errh)
{
        tb_trans_keyaction_state_t *state;
        ulong                       relid;

        ss_dassert (rs_key_type(cd, refkey)==RS_KEY_PRIMKEYCHK);
        relid = rs_key_refrelid(cd, refkey);

        state = (tb_trans_keyaction_state_t*)SsMemCalloc(1, sizeof(tb_trans_keyaction_state_t));

        state->tb_trans_keyaction_relh = tb_dd_getrelhbyid (
                cd, trans, relid, &state->tb_trans_keyaction_priv, p_errh
        );

        if (state->tb_trans_keyaction_relh == NULL) {
            SsMemFree(state);
            return NULL;
        }

        state->tb_trans_keyaction_tbrel = tb_relh_new(
                cd,
                state->tb_trans_keyaction_relh,
                state->tb_trans_keyaction_priv);

        ss_dassert(state->tb_trans_keyaction_tbrel != NULL);

        state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_LOCKREL;
        state->tb_trans_keyaction_selattrs = NULL;
        state->tb_trans_keyaction_selflags = NULL;
        state->tb_trans_keyaction_forttype = NULL;
        state->tb_trans_keyaction_relcur   = NULL;
        state->tb_trans_keyaction_forkey   = NULL;
        state->tb_trans_keyaction_rtval    = NULL;
        state->tb_trans_keyaction_avals    = NULL;
        state->tb_trans_keyaction_refkey   = refkey;
        state->tb_trans_keyaction_refttype = ttype;

        if (action == SQL_REFACT_RESTRICT) {
            state->tb_trans_keyaction_forupdate = FORUPDATE_NO;
        } else if (delete && action == SQL_REFACT_CASCADE) {
            state->tb_trans_keyaction_forupdate = FORUPDATE_SEARCHED_DELETE;
        } else {
            state->tb_trans_keyaction_forupdate = FORUPDATE_SEARCHED_UPDATE;
        }

        return state;
}

/*#***********************************************************************\
 *
 *              tab_ref_prepare
 *
 * Preparation state for the ref.actions state machine.
 *
 * Parameters :
 *
 *      cd -
 *
 *      trans -
 *
 *      refkey  - the key
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static uint tab_ref_prepare(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        tb_trans_keyaction_state_t* state,
        rs_tval_t*      old_tval,
        rs_err_t**      p_errh)
{
        uint            nsel;
        va_t*           va;
        ulong           forkeyid;
        uint            n_sql_attrs;
        uint            i;
        bool            eoc;
        uint            nrefkeyparts;
        rs_key_t*       refkey = state->tb_trans_keyaction_refkey;
        rs_ttype_t*     ttype = state->tb_trans_keyaction_refttype;

        state->tb_trans_keyaction_relcur = tb_relcur_create(
                cd, trans,
                state->tb_trans_keyaction_tbrel,
                state->tb_trans_keyaction_forupdate,
                FALSE);
        ss_dassert(state->tb_trans_keyaction_relcur!=NULL);

        nsel = rs_ttype_sql_nattrs(
                cd, tb_relh_ttype(cd, state->tb_trans_keyaction_tbrel)
        );

        state->tb_trans_keyaction_selattrs =
                                SsMemAlloc((nsel+1)*sizeof(int));

        for (i=0; i<nsel; i++) {
            state->tb_trans_keyaction_selattrs[i] = i;
        }
        state->tb_trans_keyaction_selattrs[nsel] = -1;

        ss_dassert(rs_keyp_isconstvalue(cd, refkey, 0));
        va = rs_keyp_constvalue(cd, refkey, 0);
        forkeyid = va_getlong(va);
        state->tb_trans_keyaction_forkey = rs_relh_keybyid(
                    cd, state->tb_trans_keyaction_relh, forkeyid
        );
        ss_dassert(state->tb_trans_keyaction_forkey!=NULL);

        state->tb_trans_keyaction_forttype = rs_relh_ttype(
                cd, state->tb_trans_keyaction_relh
        );
        n_sql_attrs = rs_ttype_sql_nattrs(
                cd, state->tb_trans_keyaction_forttype
        );

        state->tb_trans_keyaction_selflags =
                                SsMemAlloc(n_sql_attrs*sizeof(bool));
        for (i=0; i<n_sql_attrs; i++) {
            state->tb_trans_keyaction_selflags[i] = FALSE;
        }

        tb_relcur_project(
                cd,
                state->tb_trans_keyaction_relcur,
                state->tb_trans_keyaction_selattrs);

        nrefkeyparts = rs_key_nparts(cd, refkey);
        state->tb_trans_keyaction_avals = SsMemAlloc (
            sizeof(rs_aval_t*)*nrefkeyparts
        );

        for (i = 0; i < nrefkeyparts; i++) {
            rs_ano_t    ano;
            rs_ano_t    fano;
            rs_atype_t* atype;
            rs_aval_t*  aval;

            if (rs_keyp_isconstvalue(cd, state->tb_trans_keyaction_forkey, i)) {
                ss_dassert(rs_keyp_isconstvalue(cd, refkey, i));
                state->tb_trans_keyaction_avals[i] = NULL;
                continue;
            }
            ss_dassert(!rs_keyp_isconstvalue(cd, refkey, i));
            ano = rs_keyp_ano(cd, refkey, i);
            fano = rs_keyp_ano(cd, state->tb_trans_keyaction_forkey, i);
            atype = rs_ttype_atype(cd, ttype, ano);
            state->tb_trans_keyaction_avals[i] = rs_aval_create(cd, atype);
            aval = rs_tval_aval(cd, ttype, old_tval, ano);
            rs_aval_move(cd, atype, state->tb_trans_keyaction_avals[i],
                         aval);
            tb_relcur_constr(
                cd, state->tb_trans_keyaction_relcur, fano,
                RS_RELOP_EQUAL, atype, state->tb_trans_keyaction_avals[i],
                NULL, NULL
            );

            ss_dprintf_1(("tb_ref_keyaction: Restrict on aval %d: %s\n",
                           fano, rs_aval_print(cd, atype, aval)));

            state->tb_trans_keyaction_selflags[fano] = TRUE;
        }

        eoc = tb_relcur_endofconstr(
                    cd, state->tb_trans_keyaction_relcur, p_errh
        );
        if (!eoc) {
            return TB_CHANGE_ERROR;
        }

        state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_OPEN;

        return TB_CHANGE_SUCC;
}


/*#***********************************************************************\
 *
 *              tab_ref_open
 *
 * Cursor open state for the ref.actions state machine.
 *
 * Parameters :
 *
 *      cd -
 *
 *      trans -
 *
 *      refkey  - the key
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static uint tab_ref_open(
        rs_sysi_t*      cd,
        tb_trans_keyaction_state_t* state,
        rs_tval_t*      old_tval,
        rs_err_t**      p_errh)
{
        rs_key_t*       refkey = state->tb_trans_keyaction_refkey;
        rs_ttype_t*     ttype = state->tb_trans_keyaction_refttype;

        uint            i;
        bool            bcr;

        for (i = rs_key_first_datapart(cd, refkey);
             i <= (uint)rs_key_lastordering(cd, refkey); i++)
        {
            rs_ano_t    ano;
            rs_atype_t* atype;
            rs_aval_t*  aval;

            ss_dassert(!rs_keyp_isconstvalue(cd, refkey, i));
            ano = rs_keyp_ano(cd, refkey, i);
            atype = rs_ttype_atype(cd, ttype, ano);
            aval = rs_tval_aval(cd, ttype, old_tval, ano);
            rs_aval_move(cd, atype, state->tb_trans_keyaction_avals[i],
                         aval);
        }

        bcr = tb_relcur_open(
                cd, state->tb_trans_keyaction_relcur, p_errh
        );
        if (!bcr) {
            return TB_CHANGE_ERROR;
        }
        
        state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_SELECT;
        return TB_CHANGE_SUCC;
}

/*#***********************************************************************\
 *
 *              tab_ref_fetch
 *
 * Fetch state for the ref.actions state machine.
 *
 * Parameters :
 *
 *      cd -
 *
 *      trans -
 *
 *      refkey  - the key
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static uint tab_ref_fetch(
        rs_sysi_t*      cd,
        tb_trans_keyaction_state_t* state,
        rs_tval_t*      tval,
        int             action,
        rs_err_t**      p_errh)
{
        rs_tval_t*      rtval;
        uint            finished;
        rs_key_t*       refkey = state->tb_trans_keyaction_refkey;
        rs_ttype_t*     ttype = state->tb_trans_keyaction_refttype;

        rtval = tb_relcur_next(
                cd, state->tb_trans_keyaction_relcur, &finished, p_errh
        );

        if (rtval == NULL) {
            ss_dassert (finished == TB_FETCH_CONT ||
                        finished == TB_FETCH_ERROR ||
                        finished == TB_FETCH_SUCC);
            return finished;
        }
        state->tb_trans_keyaction_rtval = rtval;

        ss_dassert(finished != TB_FETCH_CONT);

        ss_output_1(
            if (rtval != NULL) {
                ss_dprintf(("tb_ref_keyaction: Cascade to tval:\n"));
                rs_tval_print(
                        cd, state->tb_trans_keyaction_forttype, rtval
                );
            }
        )

        if (state->tb_trans_keyaction_forupdate == FORUPDATE_SEARCHED_DELETE) {
            state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_DELETE;
            return TB_FETCH_SUCC;
        } else if (action == SQL_REFACT_RESTRICT) {
            rs_error_create_key(p_errh, DBE_ERR_CHILDEXIST_S, refkey);
            return TB_FETCH_ERROR;
        } else {
            /* Cascading update. */
            uint    i;

            rtval = rs_tval_copy(
                        cd, state->tb_trans_keyaction_forttype, rtval
            );
            state->tb_trans_keyaction_rtval = rtval;

            for (i=rs_key_first_datapart(cd, refkey);
                 i<= (uint)rs_key_lastordering(cd, refkey); i++)
            {
                rs_ano_t   ano;
                rs_aval_t* naval;
                rs_atype_t* fatype;

                ss_dassert(!rs_keyp_isconstvalue(cd, refkey, i));
                ano = rs_keyp_ano(cd, state->tb_trans_keyaction_forkey, i);

                ss_dprintf_1 (("tb_ref_keyaction: Cascade action on i=%d, ano=%d\n", i, ano));

                fatype = rs_ttype_atype(
                        cd, state->tb_trans_keyaction_forttype, ano
                );

                ss_output_1(
                    ss_dprintf(("tb_ref_keyaction: forttype = "));
                    rs_ttype_print(cd, state->tb_trans_keyaction_forttype);
                    ss_dprintf(("tb_ref_keyaction: fatype="));
                    rs_atype_print(cd, fatype);
                )

                switch (action) {
                case SQL_REFACT_CASCADE:
                    naval = rs_tval_aval(cd, ttype, tval, rs_keyp_ano(cd, refkey, i));
                    rs_tval_setaval(cd, ttype, rtval, ano, naval);
                    ss_dprintf_1(("tb_ref_keyaction: CASCADE %s\n", rs_aval_print(cd, fatype, naval)));
                    break;
                case SQL_REFACT_SETNULL:
                    naval = rs_tval_aval(cd, ttype, rtval, ano);
                    ss_dprintf_1(("tb_ref_keyaction: SET NULL %s\n", rs_aval_print(cd, fatype, naval)));
                    rs_aval_setnull(cd, fatype, naval);
                    break;
                case SQL_REFACT_SETDEFAULT:
                    naval = rs_atype_getcurrentdefault(cd, fatype);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                    if (naval == NULL) {
                        naval = rs_tval_aval(cd, ttype, rtval, ano);
                        rs_aval_setnull(cd, fatype, naval);
                        break;
                    }
#endif
                    rs_tval_setaval(cd, ttype, rtval, ano, naval);
                    break;
                default:
                    ss_error;
                }
            }
            state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_UPDATE;
            return TB_FETCH_SUCC;
        }
}

/*##**********************************************************************\
 *
 *              tb_ref_keyaction
 *
 * Implementation of referential integrity key actions.
 *
 * Parameters :
 *
 *      cd -
 *
 *      trans -
 *
 *      tbrel - relation handle for rel to be updated
 *
 *      refkey  - the key
 *
 *      old_tval - old tuple
 *
 *      tval     - new tuple (NULL for DELETE)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
uint tb_ref_keyaction(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        rs_key_t*       refkey,
        rs_ttype_t*     ttype,
        bool*           uflags,
        rs_tval_t*      old_tval,
        rs_tval_t*      tval,
        tb_trans_keyaction_state_t **state_p,
        rs_err_t**      p_errh)
{
        uint        rc;
        tb_trans_keyaction_state_t *state = *state_p;
        bool        delete = (tval == NULL);
        int         action = delete ?
                        rs_key_delete_action(cd, refkey) :
                        rs_key_update_action(cd, refkey);

        ss_output_1(
                ss_dprintf(("tb_ref_keyaction: Keyaction.\n"));
                rs_tval_print(cd, ttype, old_tval);
                rs_ttype_print(cd, ttype);
                if (tval != NULL) {
                    rs_tval_print(cd, ttype, tval);
                } else {
                    ss_dprintf(("tb_ref_keyaction: Empty new tval\n"));
                }
        )

        if (tval != NULL) {
            /* Check if we really need to cascade this update. */
            bool need_update = FALSE;
            uint i;

            for (i=rs_key_first_datapart(cd, refkey);
                 i<= (uint) rs_key_lastordering(cd, refkey); i++)
            {
                rs_ano_t ano;
                ano = rs_keyp_ano(cd, refkey, i);
                if (uflags[ano]) {
                    need_update = TRUE;
                    break;
                }
            }
            if (!need_update) {
                ss_dprintf(("tb_ref_keyaction: no update needed.\n"));
                return TB_CHANGE_SUCC;
            }
        }

        if (action == SQL_REFACT_NOACTION ||
            (action == SQL_REFACT_RESTRICT && delete))
        {
            ss_dprintf(("tb_ref_keyaction: no action needed.\n"));
            return TB_CHANGE_SUCC;
        }

        if (state == NULL) {
            state = tb_ref_alloc_state(
                   cd, trans, refkey, ttype, action, delete, p_errh
            );
            if (state == NULL) {
                ss_dprintf(("tb_ref_keyaction: state is not allocated.\n"));
                return TB_CHANGE_ERROR;
            }
            *state_p = state;
        }

        ss_dassert(action != SQL_REFACT_NOACTION);

        do {
            int ecode;

            switch (state->tb_trans_keyaction_state_no) {

            case TB_TRANS_KEYACTION_LOCKREL:
                ss_dprintf(("tb_ref_keyaction: TB_TRANS_KEYACTION_LOCKREL\n"));
                rc = dbe_trx_lockrelh(
                       tb_trans_dbtrx(cd, trans), state->tb_trans_keyaction_relh, FALSE, 0L
                );
                if (rc == DBE_RC_SUCC) {
                    state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_PREPARE;
                    rc = TB_CHANGE_SUCC;
                } else if (rc == DBE_RC_WAITLOCK) {
                    return TB_CHANGE_CONT;
                } else {
                    rs_error_create(p_errh, rc);
                    rc = TB_CHANGE_ERROR;
                }
                break;

            case TB_TRANS_KEYACTION_PREPARE:
                ss_dprintf(("tb_ref_keyaction: TB_TRANS_KEYACTION_PREPARE\n"));
                rc = tab_ref_prepare(cd, trans, state, old_tval, p_errh);
                break;

            case TB_TRANS_KEYACTION_OPEN:
                ss_dprintf(("tb_ref_keyaction: TB_TRANS_KEYACTION_OPEN\n"));
                rc = tab_ref_open(cd, state, old_tval, p_errh);
                break;

            case TB_TRANS_KEYACTION_SELECT:
                ss_dprintf(("tb_ref_keyaction: TB_TRANS_KEYACTION_SELECT\n"));
                rc = tab_ref_fetch(cd, state, tval, action, p_errh);

                if (rc == TB_FETCH_CONT) {
                    ss_dprintf(("tb_ref_keyaction: select -> TB_FETCH_CONT\n"));
                    return TB_CHANGE_CONT;
                } else if (rc == TB_FETCH_SUCC) {
                    ss_dprintf(("tb_ref_keyaction: select -> TB_FETCH_SUCC\n"));
                    if (state->tb_trans_keyaction_state_no == TB_TRANS_KEYACTION_SELECT) {
                        rc = TB_CHANGE_SUCC;
                        goto finished;
                    } else {
                        rc = TB_CHANGE_SUCC;
                    }
                } else if (rc == TB_FETCH_ERROR) {
                    ss_dprintf(("tb_ref_keyaction: select -> TB_FETCH_ERROR\n"));
                    rc = TB_CHANGE_ERROR;
                } else {
                    ss_error;
                }
                break;

            case TB_TRANS_KEYACTION_UPDATE:
                ss_dprintf(("tb_ref_keyaction: TB_TRANS_KEYACTION_UPDATE\n"));

                rs_sysi_setallowduplicatedelete(cd, TRUE);
                rc = tb_relcur_update(
                        cd,
                        state->tb_trans_keyaction_relcur,
                        state->tb_trans_keyaction_rtval,
                        state->tb_trans_keyaction_selflags,
                        NULL, 0, NULL, NULL, NULL, NULL, p_errh
                );
                rs_sysi_setallowduplicatedelete(cd, FALSE);

                if (rc == TB_CHANGE_CONT) {
                    ss_dprintf(("tb_ref_keyaction: update -> TB_CHANGE_CONT\n"));
                    return rc;
                }

                rs_tval_free(
                        cd,
                        state->tb_trans_keyaction_forttype,
                        state->tb_trans_keyaction_rtval
                );

                if (rc == TB_CHANGE_SUCC) {
                    ss_dprintf(("tb_ref_keyaction: update -> TB_CHANGE_SUCC\n"));
                    state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_SELECT;
                } else {
                    ecode = rs_error_geterrcode(cd, *p_errh);
                    if (ecode == DBE_ERR_UNIQUE_S || ecode == DBE_ERR_NOTFOUND
                        || ecode == MME_RC_DUPLICATE_DELETE) {
                        ss_dprintf(("tb_ref_keyaction: delete -> mask error\n"));
                        rs_error_free(cd, *p_errh);
                        *p_errh = NULL;
                        rc = TB_CHANGE_SUCC;
                        state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_SELECT;
                    } else {
                        rc = TB_CHANGE_ERROR;
                    }
                }
                
                break;

            case TB_TRANS_KEYACTION_DELETE:
                ss_dprintf(("tb_ref_keyaction: TB_TRANS_KEYACTION_DELETE\n"));
                rs_sysi_setallowduplicatedelete(cd, TRUE);
                rc = tb_relcur_delete(
                        cd, state->tb_trans_keyaction_relcur, p_errh
                );
                rs_sysi_setallowduplicatedelete(cd, FALSE);
                switch (rc) {
                case TB_CHANGE_CONT:
                    ss_dprintf(("tb_ref_keyaction: delete -> TB_CHANGE_CONT\n"));
                    return rc;
                case TB_CHANGE_SUCC:
                    ss_dprintf(("tb_ref_keyaction: delete -> TB_CHANGE_SUCC\n"));
                    state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_SELECT;
                    break;
                case TB_CHANGE_ERROR:
                    ss_dprintf(("tb_ref_keyaction: delete -> TB_CHANGE_ERROR\n"));
                    /* Here we do mask caseaded double-delete */
                    ecode = rs_error_geterrcode(cd, *p_errh);
                    if (ecode == DBE_ERR_UNIQUE_S || ecode == DBE_ERR_NOTFOUND
                        || ecode == MME_RC_DUPLICATE_DELETE) {
                        ss_dprintf(("tb_ref_keyaction: delete -> mask error\n"));
                        rs_error_free(cd, *p_errh);
                        *p_errh = NULL;
                        rc = TB_CHANGE_SUCC;
                        state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_SELECT;
                    }
                    break;
                default:
                    rc = TB_CHANGE_ERROR;
                }
                break;

            default:
                ss_derror;
                rc = TB_CHANGE_ERROR;
            }
        } while (rc == TB_CHANGE_SUCC);

 finished:

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        if (state->tb_trans_keyaction_state_no != TB_TRANS_KEYACTION_LOCKREL
            && state->tb_trans_keyaction_state_no != TB_TRANS_KEYACTION_PREPARE) 
        {
            ss_dprintf(("tb_ref_keyaction: reset, ends with rc = %d\n", rc));
#else
        if (rc == TB_CHANGE_SUCC) {
            ss_dprintf(("tb_ref_keyaction: ends with TB_CHANGE_SUCC\n"));
#endif
            tb_relcur_reset(cd, state->tb_trans_keyaction_relcur, FALSE);
            state->tb_trans_keyaction_state_no = TB_TRANS_KEYACTION_OPEN;
        } else {
            ss_dprintf(("tb_ref_keyaction: ends with rc = %d\n", rc));
        }
        
        return rc;
}

uint tb_ref_keyaction_dbx(
        rs_sysi_t*      cd,
        dbe_trx_t*      dbtrx,
        rs_key_t*       refkey,
        rs_ttype_t*     ttype,
        int*            uflags,
        rs_tval_t*      old_tval,
        rs_tval_t*      tval,
        tb_trans_keyaction_state_t **state_p,
        rs_err_t**      p_errh)
{
        tb_trans_t* trx = tb_trans_rep_init(cd, dbtrx); 
        uint ret = tb_ref_keyaction(cd, trx, refkey, ttype, uflags, old_tval,
                                    tval, state_p, p_errh);
        tb_trans_rep_done(cd, trx);
        return ret;
}

void tb_ref_keyaction_free(
        rs_sysi_t*      cd,
        tb_trans_keyaction_state_t **state_p)
{
        tb_trans_keyaction_state_t *state = *state_p;
        rs_key_t*   refkey = state->tb_trans_keyaction_refkey;
        ulong       nrefkeyparts;
        ulong       i;

        if (state->tb_trans_keyaction_relcur != NULL) {
            tb_relcur_free(cd, state->tb_trans_keyaction_relcur);
        }

        if (state->tb_trans_keyaction_tbrel != NULL) {
            tb_relh_free(cd, state->tb_trans_keyaction_tbrel);
        }

        nrefkeyparts = rs_key_nparts(cd, refkey);
        if (state->tb_trans_keyaction_avals != NULL) {
            for (i = 0; i < nrefkeyparts; i++) {
                if (state->tb_trans_keyaction_avals[i] != NULL) {
                    rs_atype_t* atype;
                    rs_ano_t    ano;
                    
                    ano = rs_keyp_ano(cd, refkey, i);
                    atype = rs_ttype_atype(
                            cd,
                            state->tb_trans_keyaction_refttype,
                            ano);
                    rs_aval_free(
                            cd,
                            atype,
                            state->tb_trans_keyaction_avals[i]);
                }
            }
            SsMemFree(state->tb_trans_keyaction_avals);
        }

        if (state->tb_trans_keyaction_selflags != NULL) {
            SsMemFree(state->tb_trans_keyaction_selflags);
        }
        if (state->tb_trans_keyaction_selattrs != NULL) {
            SsMemFree(state->tb_trans_keyaction_selattrs);
        }
        SsMemFree(state);
        *state_p = NULL;
}
