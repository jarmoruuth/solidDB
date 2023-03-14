/*************************************************************************\
**  source       * tab0seq.c
**  directory    * tab
**  description  * Sequence support function
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

#ifndef SS_NOSEQUENCE

#include <dt0date.h>

#include <rs0types.h>
#include <rs0sdefs.h>
#include <rs0auth.h>
#include <rs0sysi.h>

#include <dbe0db.h>
#include <dbe0seq.h>

#include "tab1defs.h"
#include "tab1priv.h"
#include "tab1dd.h"
#include "tab0conn.h"
#include "tab0tli.h"
#include "tab0evnt.h"
#include "tab0admi.h"
#include "tab0cata.h"
#include "tab0seq.h"

#define CHK_SEQ(s)  ss_dassert(SS_CHKPTR(s) && (s)->seq_chk == TBCHK_SEQ)

typedef enum {
        TAB_SEQOPER_CURRENT,
        TAB_SEQOPER_NEXT,
        TAB_SEQOPER_SET
} tab_seqoper_t;

struct sqlseqstruct {
        ss_debug(tb_check_t seq_chk;)
        long        seq_id;
        bool        seq_densep;
        rs_atype_t* seq_atype;
};

/*#***********************************************************************\
 *
 *		tb_seq_create
 *
 * Creates a new sequence object to the database.
 *
 * Parameters :
 *
 *	cd - in
 *
 *
 *	trans - use
 *
 *
 *	name - in
 *		The name of the event.
 *
 *	densep - in
 *
 *
 *	p_errh - out, give
 *		Error object.
 *
 *  seq_id - out, give
 *      sequence identifier stored here if non-NULL
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool tb_seq_create(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* name,
        char* authid,
        char* catalog,
        bool densep,
        rs_err_t** p_errh,
        long* seq_id,
        bool auto_increment_seq __attribute__ ((unused)))
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        long id;
        dt_date_t date;
        dbe_db_t* db;
        rs_auth_t* auth;
        char* authname;
        char* catalogname;
        char* densestr;
        bool succp;
        long current_userid;
        int rc;
        rs_entname_t en;

        ss_dprintf_1(("tb_seq_create\n"));

        catalog = tb_catalog_resolve(cd, catalog);

        if (!tb_dd_checkobjectname(name)) {
            rs_error_create(p_errh, SP_ERR_UNDEFSEQUENCE_S, "");
            return(FALSE);
        }

        if (strlen(name) > RS_MAX_NAMELEN) {
            rs_error_create(p_errh, E_TOOLONGNAME_S, name);
            return(FALSE);
        }

        auth = rs_sysi_auth(cd);

        if (!RS_ENTNAME_ISSCHEMA(authid)) {
            authid = rs_auth_schema(cd, auth);
        }
        rs_entname_initbuf(&en, catalog, authid, name);

        if (!tb_priv_checkschemaforcreateobj(cd, trans, &en, &current_userid, p_errh)) {
            return (FALSE);
        }

        db = rs_sysi_db(cd);
        id = dbe_db_getnewrelid_log(db);

        if (seq_id) {
            *seq_id = id;
        }
        
        if (!rs_rbuf_addname(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_SEQUENCE, id)
            && !dbe_trx_namedeleted(tb_trans_dbtrx(cd, trans), &en))
        {
            rs_error_create(p_errh, E_SEQEXISTS_S, name);
            return (FALSE);
        }
        rc = dbe_trx_insertname(tb_trans_dbtrx(cd, trans), &en);
        if (rc != DBE_RC_SUCC) {
            rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_SEQUENCE);
            rs_error_create(p_errh, rc);
            return (FALSE);
        }

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SEQUENCES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, (char *)RS_ANAME_SEQUENCES_ID, &id);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_NAME, &name);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_DENSE, &densestr);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_SCHEMA, &authname);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_CATALOG, &catalogname);
        ss_dassert(trc == TLI_RC_SUCC);
        catalogname = rs_entname_getcatalog(&en);
        trc = TliCursorColDate(tcur, (char *)RS_ANAME_SEQUENCES_CREATIME, &date);
        ss_dassert(trc == TLI_RC_SUCC);

        date = tb_dd_curdate();
        authname = rs_entname_getschema(&en);
        ss_dassert(authname != NULL);
        if (densep) {
            densestr = (char *)RS_AVAL_YES;
        } else {
            densestr = (char *)RS_AVAL_NO;
        }

        trc = TliCursorInsert(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        TliCursorFree(tcur);

        succp = (trc == TLI_RC_SUCC);

        if (succp) {
            dbe_ret_t rc;
            rc = dbe_trx_insertseq(tb_trans_dbtrx(cd, trans), id, &en, densep);
            if (rc != DBE_RC_SUCC) {
                rs_error_create(p_errh, rc);
                succp = FALSE;
            }
        }

        if (succp) {
            succp = tb_admi_grantcreatorpriv(
                        cd,
                        tcon,
                        id,
                        current_userid,
                        TB_PRIV_SELECT|TB_PRIV_INSERT|TB_PRIV_DELETE|
                        TB_PRIV_UPDATE|TB_PRIV_REFERENCES|TB_PRIV_CREATOR,
                        p_errh);
        }

        if (!succp) {
            rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_SEQUENCE);
        } else {
            FAKE_CODE_BLOCK_GT(FAKE_DBE_SEQUENCE_RECOVERY_BUG, 0,
            {
                SsPrintf("FAKE_DBE_SEQUENCE_RECOVERY_BUG:tb_seq_create\n");
                FAKE_SET(FAKE_DBE_SEQUENCE_RECOVERY_BUG, 2);
            }
            );
        }

        TliConnectDone(tcon);

        return(succp);
}

/*#***********************************************************************\
 *
 *		tb_seq_drop
 *
 * Drops a sequence. Only administrator or the creator of a sequence
 * can drop the sequence.
 *
 * Parameters :
 *
 *	cd - in
 *
 *
 *	trans - use
 *
 *
 *	name - in
 *
 *
 *	p_errh - out, give
 *		Error object.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool tb_seq_drop(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* name,
        char* authid,
        char* catalog,
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        long id;
        char* densestr;
        bool succp = TRUE;

        ss_dprintf_1(("tb_seq_drop\n"));
        ss_dassert(RS_ENTNAME_ISSCHEMA(authid));

        catalog = tb_catalog_resolve(cd, catalog);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SEQUENCES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, (char *)RS_ANAME_SEQUENCES_ID, &id);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_DENSE, &densestr);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SEQUENCES_NAME,
                TLI_RELOP_EQUAL,
                name);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SEQUENCES_SCHEMA,
                TLI_RELOP_EQUAL,
                authid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SEQUENCES_CATALOG,
                TLI_RELOP_EQUAL_OR_ISNULL,
                catalog);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);

        if (trc != TLI_RC_SUCC && trc != TLI_RC_END) {
            TliCursorCopySuErr(tcur, p_errh);
            TliCursorFree(tcur);
            TliConnectDone(tcon);
            return(FALSE);
        }

        if (trc == TLI_RC_SUCC) {
            /* Found. Check creator privilege.
             */
            tb_relpriv_t* priv;

            tb_priv_getrelpriv(cd, id, FALSE, FALSE, &priv);
            succp = tb_priv_isrelpriv(
                        cd,
                        priv,
                        TB_PRIV_CREATOR,
                        RS_SDEFS_ISSYSSCHEMA(authid));

            if (succp) {
                trc = TliCursorDelete(tcur);
                ss_dassert(trc == TLI_RC_SUCC);
                succp = (trc == TLI_RC_SUCC);
            }
        } else {
            /* Not found */
            ss_dassert(trc == TLI_RC_END);
            succp = FALSE;
        }

        if (!succp) {
            rs_error_create(p_errh, E_SEQNOTEXIST_S, name);
        }
        if (succp) {
            succp = tb_priv_droprelpriv(tcon, id, p_errh);
        }
        if (succp) {
            dbe_ret_t rc;
            rs_entname_t en;


            rs_entname_initbuf(&en,
                               catalog,
                               authid,
                               name);

            rc = dbe_trx_deleteseq(
                    tb_trans_dbtrx(cd, trans),
                    id,
                    &en,
                    strcmp(densestr, RS_AVAL_YES) == 0);
            if (rc != DBE_RC_SUCC) {
                rs_error_create(p_errh, rc);
                succp = FALSE;
            } else {
                rc = dbe_trx_deletename(tb_trans_dbtrx(cd, trans), &en);
                if (rc != DBE_RC_SUCC) {
                    rs_error_create(p_errh, rc);
                    succp = FALSE;
                }
            }
        }

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return(succp);
}

/*#***********************************************************************\
 *
 *		seq_find_byschema
 *
 * Finds sequence by id or name.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trans -
 *
 *
 *	name -
 *
 *
 *	p_id -
 *
 *
 *	p_authid -
 *
 *
 *	p_isdense -
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
static bool seq_find_byschema(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* name,
        char* authid,
        char* catalog,
        long* p_id,
        char** p_authid,
        char** p_catalog,
        bool* p_isdense,
        bool sqlp,
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char* isdensestr;
        bool succp;
        long id;
        long found_id;
        bool isdense = FALSE;
        bool err_created = FALSE;

        ss_dprintf_3(("seq_find_byschema\n"));

        if (!tb_dd_checkobjectname(name)) {
            ss_dprintf_4(("seq_find_byschema:SP_ERR_UNDEFSEQUENCE_S\n"));
            rs_error_create(p_errh, SP_ERR_UNDEFSEQUENCE_S, "");
            return(FALSE);
        }

        catalog = tb_catalog_resolve(cd, catalog);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SEQUENCES);
        ss_dassert(tcur != NULL);

        ss_dassert(name != NULL);
        trc = TliCursorColLong(tcur, (char *)RS_ANAME_SEQUENCES_ID, &id);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_DENSE, &isdensestr);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_DENSE, &isdensestr);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_SCHEMA, &authid);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SEQUENCES_NAME,
                TLI_RELOP_EQUAL,
                name);
        ss_dassert(trc == TLI_RC_SUCC);
        if (RS_ENTNAME_ISSCHEMA(authid)) {
            trc = TliCursorConstrStr(
                    tcur,
                    (char *)RS_ANAME_SEQUENCES_SCHEMA,
                    TLI_RELOP_EQUAL,
                    authid);
            ss_dassert(trc == TLI_RC_SUCC);
        }
        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SEQUENCES_CATALOG,
                TLI_RELOP_EQUAL_OR_ISNULL,
                catalog);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SEQUENCES_CATALOG, &catalog);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);

        if (trc != TLI_RC_SUCC && trc != TLI_RC_END) {
            TliCursorCopySuErr(tcur, p_errh);
            TliCursorFree(tcur);
            TliConnectDone(tcon);
            ss_dprintf_4(("seq_find_byschema:trc=%d\n", trc));
            return(FALSE);
        }

        succp = (trc == TLI_RC_SUCC);
        found_id = id;

        if (succp) {
            tb_relpriv_t* priv;
            tb_priv_getrelpriv(
                cd,
                found_id,
                FALSE,
                FALSE,
                &priv);
            succp = tb_priv_isrelpriv(
                        cd,
                        priv,
                        TB_PRIV_SELECT,
                        RS_SDEFS_ISSYSSCHEMA(authid));
            if (!succp) {
                rs_error_create(p_errh, E_NOPRIV);
                err_created = TRUE;
            }
        }

        if (succp) {
            isdense = (isdensestr[0] == 'Y');
            if (p_id != NULL) {
                *p_id = found_id;
            }
            if (p_authid != NULL) {
                *p_authid = SsMemStrdup(authid);
            }
            if (p_catalog != NULL) {
                if (catalog != NULL) {
                    catalog = SsMemStrdup(catalog);
                }
                *p_catalog = catalog;
            }

            if (p_isdense != NULL) {
                *p_isdense = isdense;
            }
            trc = TliCursorNext(tcur);
            ss_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END);
            succp = (trc == TLI_RC_END);
            if (!succp) {
                if (p_authid != NULL) {
                    SsMemFree(*p_authid);
                    *p_authid = NULL;
                }
                rs_error_create(p_errh, E_AMBIGUOUS_S, name);
            }
        } else {
            if (!err_created) {
                rs_error_create(p_errh, SP_ERR_UNDEFSEQUENCE_S, name);
            }
        }

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        if (succp && isdense && sqlp) {
            rs_sysi_addseqid(cd, found_id);
        }
        ss_dprintf_4(("seq_find_byschema:succp=%d\n", succp));

        return(succp);
}

/*#***********************************************************************\
 *
 *		seq_find_local
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trans -
 *
 *
 *	name -
 *
 *
 *	authid -
 *
 *
 *	p_authid -
 *
 *
 *	p_id -
 *
 *
 *	p_isdense -
 *
 *
 *	sqlp -
 *
 *
 *	p_errh -
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
static bool seq_find_local(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* name,
        char* authid,
        char* catalog,
        char** p_authid,
        char** p_catalog,
        long* p_id,
        bool* p_isdense,
        bool sqlp,
        rs_err_t** p_errh)
{
        bool succp;
        rs_auth_t* auth;

        ss_dprintf_3(("seq_find_local\n"));

        catalog = tb_catalog_resolve(cd, catalog);

        auth = rs_sysi_auth(cd);
        if (!RS_ENTNAME_ISSCHEMA(authid)) {
            succp = seq_find_byschema(
                        cd,
                        trans,
                        name,
                        rs_auth_schema(cd, auth),
                        catalog,
                        p_id,
                        p_authid,
                        p_catalog,
                        p_isdense,
                        sqlp,
                        NULL);
            if (succp) {
                return(TRUE);
            }
        }
        succp = seq_find_byschema(
                    cd,
                    trans,
                    name,
                    authid,
                    catalog,
                    p_id,
                    p_authid,
                    p_catalog,
                    p_isdense,
                    sqlp,
                    p_errh);
        return(succp);
}

/*##**********************************************************************\
 *
 *		tb_seq_find
 *
 * Tries to find a sequence with name 'name'.
 *
 * Parameters :
 *
 *	cd - in
 *
 *
 *	trans - use
 *
 *
 *	name - in
 *		Sequence name searched from the database.
 *
 *	p_authid - out, give
 *
 *
 *	p_id - out
 *
 *
 *	p_isdense
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
bool tb_seq_find(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* name,
        char* authid,
        char* catalog,
        char** p_authid,
        char** p_catalog,
        long* p_id,
        bool* p_isdense,
        rs_err_t** p_errh)
{
        bool succp;

        ss_dprintf_1(("tb_seq_find\n"));

        succp = seq_find_local(
                    cd,
                    trans,
                    name,
                    authid,
                    catalog,
                    p_authid,
                    p_catalog,
                    p_id,
                    p_isdense,
                    FALSE,
                    p_errh);

        ss_dprintf_2(("tb_seq_find:succp=%d\n", succp));

        return(succp);
}

/*##**********************************************************************\
 *
 *		tb_seq_findbyid
 *
 * Finds sequence name by sequence id.
 *
 * Parameters :
 *
 *	cd - in
 *
 *
 *
 *	id - in
 *		Sequence id.
 *
 *	p_name - out, give
 *		If found and p_name != NULL, sequence name is allocated
 *          to *p_name.
 *
 *	p_authid - out, give
 *		If found and p_authid != NULL, sequence authid is allocated
 *          to *p_authid.
 *
 *	p_isdense - out
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_seq_findbyid(
        rs_sysi_t* cd,
        long id,
        char** p_name,
        char** p_authid,
        char** p_catalog)
{
        bool foundp;
        rs_entname_t* en;

        ss_dprintf_1(("tb_seq_findbyid\n"));

        foundp = rs_rbuf_namebyid(
                    cd,
                    rs_sysi_rbuf(cd),
                    id,
                    RSRBUF_NAME_SEQUENCE,
                    &en);

        if (!foundp) {
            return(FALSE);
        }
        if (p_name != NULL) {
            *p_name = SsMemStrdup(rs_entname_getname(en));
        }
        if (p_authid != NULL) {
            char* schema;
            schema = rs_entname_getschema(en);
            ss_dassert(RS_ENTNAME_ISSCHEMA(schema));
            if (RS_ENTNAME_ISSCHEMA(schema)) {
                *p_authid = SsMemStrdup(schema);
            } else {
                *p_authid = SsMemStrdup((char *)"");
            }
        }
        if (p_catalog != NULL) {
            char* catalog = rs_entname_getcatalog(en);
            if (catalog != NULL) {
                catalog = SsMemStrdup(catalog);
            }
            *p_catalog = catalog;
        }

        rs_entname_done(en);
        return(foundp);
}

/*##**********************************************************************\
 *
 *		tb_seq_lock
 *
 * Locks sequence by id.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trans -
 *
 *
 *	seq_id -
 *
 *
 *	p_errh -
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
dbe_ret_t tb_seq_lock(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        rs_err_t** p_errh)
{
        return(dbe_seq_lock(
                    tb_trans_dbtrx(cd, trans),
                    seq_id,
                    p_errh));
}

/*##**********************************************************************\
 *
 *		tb_seq_unlock
 *
 * UnLocks sequence by id.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trans -
 *
 *
 *	seq_id -
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
void tb_seq_unlock(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id)
{
        dbe_seq_unlock(
            tb_trans_dbtrx(cd, trans),
            seq_id);
}

/*#***********************************************************************\
 *
 *		seq_oper
 *
 * Operetes with a sequence. Both dense and sparse sequences and also
 * next and current operations are handled here.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trans -
 *
 *
 *	seq_id -
 *
 *
 *	densep -
 *
 *
 *	oper -
 *
 *
 *	atype -
 *
 *
 *	aval -
 *
 *
 *	p_finishedp -
 *
 *
 *	p_errh -
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
static bool seq_oper(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        bool densep,
        tab_seqoper_t oper,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool* p_finishedp,
        rs_err_t** p_errh)
{
        dbe_ret_t rc = 0;
        dbe_seq_t* seq;
        dbe_db_t* db;

        ss_dprintf_3(("seq_oper\n"));
        db = rs_sysi_db(cd);
        seq = dbe_db_getseq(db);

        rs_sysi_setdisablerowspermessage(cd, TRUE);

        switch (oper) {
            case TAB_SEQOPER_CURRENT:
                ss_dprintf_3(("seq_oper:TAB_SEQOPER_CURRENT\n"));
                rc = dbe_seq_current(
                        seq,
                        tb_trans_dbtrx(cd, trans),
                        seq_id,
                        densep,
                        atype,
                        aval,
                        p_errh);
                break;
            case TAB_SEQOPER_NEXT:
                ss_dprintf_3(("seq_oper:TAB_SEQOPER_NEXT\n"));
                rc = dbe_seq_next(
                        seq,
                        tb_trans_dbtrx(cd, trans),
                        seq_id,
                        densep,
                        atype,
                        aval,
                        p_errh);
                break;
            case TAB_SEQOPER_SET:
                ss_dprintf_3(("seq_oper:TAB_SEQOPER_SET\n"));
                rc = dbe_seq_set(
                        seq,
                        tb_trans_dbtrx(cd, trans),
                        seq_id,
                        densep,
                        atype,
                        aval,
                        p_errh);
                break;
            default:
                ss_rc_error(oper);
        }
        if (rc == DBE_RC_CONT) {
            *p_finishedp = FALSE;
            return(TRUE);
        } else {
            *p_finishedp = TRUE;
            return(rc == DBE_RC_SUCC);
        }
}

static bool seq_checksysrights(
        rs_sysi_t* cd,
        long seq_id,
        rs_err_t** p_errh)
{
        tb_relpriv_t* priv;
        bool succp;
        char* name = NULL;
        char* authid = NULL;

        ss_dprintf_1(("seq_checksysrights\n"));

        /* System rights needed for drop, set and next */
        succp = tb_seq_findbyid(cd, seq_id, &name, &authid, NULL);
        if (succp && RS_SDEFS_ISSYSSCHEMA(authid)) {
            tb_priv_getrelpriv(cd, seq_id, FALSE, FALSE, &priv);
            succp = tb_priv_isrelpriv(
                        cd,
                        priv,
                        TB_PRIV_CREATOR,
                        RS_SDEFS_ISSYSSCHEMA(authid));
        }
        if (!succp) {
            rs_error_create(p_errh, E_SEQNOTEXIST_S, name);
        }
        if (authid != NULL) {
            SsMemFree(authid);
        }
        if (name != NULL) {
            SsMemFree(name);
        }
        return(succp);

}

/*##**********************************************************************\
 *
 *		tb_seq_next
 *
 * Gets the next sequence value.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trans -
 *
 *
 *	seq_id -
 *
 *
 *	densep -
 *
 *
 *	atype -
 *
 *
 *	aval -
 *
 *
 *	p_finishedp -
 *
 *
 *	p_errh -
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
bool tb_seq_next(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool* p_finishedp,
        rs_err_t** p_errh)
{
        ss_dprintf_1(("tb_seq_next\n"));

        /* If system sequence, cannot be incremented by the user. */
        if (!seq_checksysrights(cd, seq_id, p_errh)) {
            return(FALSE);
        }
        return(seq_oper(
                    cd,
                    trans,
                    seq_id,
                    densep,
                    TAB_SEQOPER_NEXT,
                    atype,
                    aval,
                    p_finishedp,
                    p_errh));
}

/*##**********************************************************************\
 *
 *		tb_seq_current
 *
 * Gets the current sequence value.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trans -
 *
 *
 *	seq_id -
 *
 *
 *	densep -
 *
 *
 *	atype -
 *
 *
 *	aval -
 *
 *
 *	p_finishedp -
 *
 *
 *	p_errh -
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
bool tb_seq_current(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool* p_finishedp,
        rs_err_t** p_errh)
{
        ss_dprintf_1(("tb_seq_current\n"));

        return(seq_oper(
                    cd,
                    trans,
                    seq_id,
                    densep,
                    TAB_SEQOPER_CURRENT,
                    atype,
                    aval,
                    p_finishedp,
                    p_errh));
}

/*##**********************************************************************\
 *
 *		tb_seq_set
 *
 * Sets the current sequence value.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trans -
 *
 *
 *	seq_id -
 *
 *
 *	densep -
 *
 *
 *	atype -
 *
 *
 *	aval -
 *
 *
 *	p_finishedp -
 *
 *
 *	p_errh -
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
bool tb_seq_set(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool* p_finishedp,
        rs_err_t** p_errh)
{
        ss_dprintf_1(("tb_seq_set\n"));

        /* If system sequence, cannot be set by the user. */
        if (!seq_checksysrights(cd, seq_id, p_errh)) {
            return(FALSE);
        }
        return(seq_oper(
                    cd,
                    trans,
                    seq_id,
                    densep,
                    TAB_SEQOPER_SET,
                    atype,
                    aval,
                    p_finishedp,
                    p_errh));
}

/*##**********************************************************************\
 *
 *		tb_seq_sql_create
 *
 * Member of the SQL function block.
 * Function creates a sequence handle.
 *
 * Parameters :
 *
 *	cd - in
 *		application state
 *
 *	trans - in
 *		handle to the current transaction
 *
 *	name - in
 *		name of the sequence
 *
 *	schema - in
 *		schema name for the sequence (NULL if none)
 *
 *	catalog - in
 *		catalog name for the sequence (NULL if none)
 *
 *	throughview - in
 *		2 if the sequence is referred through a view,
 *          1 if the sequence is referred directly when
 *          testing a CREATE VIEW definition
 *          0 if the sequence is directly referred in the
 *          SQL query being executed
 *
 *	viewname - in
 *		if throughview = 2, the name of the view in
 *          original query that results into the use
 *          of the sequence
 *
 *	viewschema - in
 *		if throughview = 2, the schema of the view in
 *          original query that results into the use
 *          of the sequence (NULL if no schema)
 *
 *	viewcatalog - in
 *		if throughview = 2, the catalog of the view in
 *          original query that results into the use
 *          of the sequence (NULL if no catalog)
 *
 *	viewhandle - in
 *		if throughview = 2, the handle of the view in
 *          original query that results into the use
 *          of the sequence
 *
 *	err - out, give
 *		in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL. NULL is stored in *err
 *          if the sequence is not found
 *
 * Return value - give :
 *
 *      pointer into a newly allocated sequence handle
 *      NULL is returned in case of failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
sqlseq_t *tb_seq_sql_create(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char *name,
        char *schema,
        char *catalog,
        uint throughview __attribute__ ((unused)),
        char *viewname __attribute__ ((unused)),
        char *viewschema __attribute__ ((unused)),
        char *viewcatalog __attribute__ ((unused)),
        sqlview_t *viewhandle __attribute__ ((unused)),
        sqlerr_t **err)
{
        sqlseq_t* seq;
        bool succp;
        long id;
        bool densep;

        catalog = tb_catalog_resolve(cd, catalog);

        succp = seq_find_local(
                    cd,
                    trans,
                    name,
                    schema,
                    catalog,
                    NULL,   /* p_authid */
                    NULL,   /* p_catalog */
                    &id,
                    &densep,
                    TRUE,
                    err);
        if (!succp) {
            if (err != NULL && su_err_geterrcode(*err) == SP_ERR_UNDEFSEQUENCE_S) {
                su_err_done(*err);
                *err = NULL;
            }
            return(NULL);
        }

        seq = SSMEM_NEW(sqlseq_t);

        ss_debug(seq->seq_chk = TBCHK_SEQ);
        seq->seq_id = id;
        seq->seq_densep = densep;
        seq->seq_atype = rs_atype_initbigint(cd);

#ifdef SS_TC_CLIENT
        /* seq.nextval must be executed on primary. 
         * Here (in prepare) we do not know if nextval or curval, so
         * all sequence opers are done in primary.
         */
        rs_sysi_setflag(cd, RS_SYSI_FLAG_TC_USE_PRIMARY);
#endif /* SS_TC_CLIENT */

        return(seq);
}

/*##**********************************************************************\
 *
 *		tb_seq_sql_free
 *
 * Member of the SQL function block.
 * Function releases a sequence handle.
 *
 * Parameters :
 *
 *	cd - in
 *		application state
 *
 *	seq - in, take
 *		pointer into the seq handle
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_seq_sql_free(
        rs_sysi_t* cd,
        sqlseq_t *seq)
{
        CHK_SEQ(seq);

        rs_atype_free(cd, seq->seq_atype);
        SsMemFree(seq);
}

/*##**********************************************************************\
 *
 *		tb_seq_sql_oper
 *
 * Member of the SQL function block.
 * Function performs a sequence operation.
 *
 * Parameters :
 *
 *	cd - in
 *		application state
 *
 *	trans - in
 *		handle to the current transaction
 *
 *	seq - in
 *		pointer into the seq handle
 *
 *	operstr - in
 *		the operation identifier. Typical operations
 *          are "CURRVAL" and "NEXTVAL"
 *
 *	resval - out, give
 *		if non-NULL, the result value of the operation
 *          is stored into *resval. If NULL, only the
 *          result type of the operation is evaluated
 *
 *	sideeffects - out
 *		if non-NULL, in successful operation into
 *          *sideeffects is stored 1 in the case of an
 *          operation with side effects and 0 without
 *          any side effects. Typically, CURRVAL does
 *          not have any side effects while NEXTVAL
 *          has
 *
 *	err - out, give
 *		in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *
 *      pointer into the sqlftype_t object in the sequence handle
 *      describing the type of the result of the operation
 *      NULL in case of error
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
sqlftype_t *tb_seq_sql_oper(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        sqlseq_t *seq,
        char *operstr,
        sqlfinst_t **resval,
        bool *sideeffects,
        sqlerr_t **err)
{
        bool succp = TRUE;
        bool nextp;

        CHK_SEQ(seq);
        ss_dprintf_1(("tb_seq_sql_oper:operstr=%s\n", operstr));
        SS_PUSHNAME("tb_seq_sql_oper");

        if (strcmp(operstr, "CURRVAL") == 0 || strcmp(operstr, "CURRENT") == 0) {
            nextp = FALSE;
        } else if (strcmp(operstr, "NEXTVAL") == 0 || strcmp(operstr, "NEXT") == 0) {
            nextp = TRUE;
        } else {
            /* Error, let sql generate error handle. */
            if (err != NULL) {
                *err = NULL;
            }
            SS_POPNAME;
            ss_dprintf_2(("tb_seq_sql_oper:error, unknown operstr\n"));
            return(NULL);
        }

        if (sideeffects != NULL) {
            *sideeffects = nextp;
        }

        if (resval != NULL) {
            bool finishedp;
            *resval = rs_aval_create(cd, seq->seq_atype);
            if (nextp) {
                succp = tb_seq_next(
                            cd,
                            trans,
                            seq->seq_id,
                            seq->seq_densep,
                            seq->seq_atype,
                            *resval,
                            &finishedp,
                            err);
                ss_dassert(finishedp);
            } else {
                succp = tb_seq_current(
                            cd,
                            trans,
                            seq->seq_id,
                            seq->seq_densep,
                            seq->seq_atype,
                            *resval,
                            &finishedp,
                            err);
                ss_dassert(finishedp);
            }
            if (!succp) {
                rs_aval_free(cd, seq->seq_atype, *resval);
            }
        }

        SS_POPNAME;

        if (succp) {
            ss_dprintf_2(("tb_seq_sql_oper:success\n"));
            return(seq->seq_atype);
        } else {
            ss_dprintf_2(("tb_seq_sql_oper:error\n"));
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *		tb_createseq
 *
 * Member of the SQL function block.
 * Function creates a new sequence into the database.
 *
 * Parameters :
 *
 *	cd - in
 *		application state
 *
 *	trans - in
 *		handle to the current transaction
 *
 *	seqname - in
 *		name of the sequence to be created
 *
 *	schema - in
 *		schema name for the sequence (NULL if none)
 *
 *	catalog - in
 *		catalog name for the sequence (NULL if none)
 *
 *	dense - in
 *		1 if the sequence is to be dense, 0 if coarse
 *
 *      cont - in/out, give
 *          *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *	err - out, give
 *		in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *
 *      1 if the operation is successful
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_createseq(
        rs_sysi_t* cd,
        sqltrans_t *trans,
        char *seqname,
        char *schema,
        char *catalog,
        char* extrainfo __attribute__ ((unused)),
        bool dense,
        void** cont,
        sqlerr_t **err)
{
        *cont = NULL;
        return(tb_seq_create(
                    cd,
                    trans,
                    seqname,
                    schema,
                    catalog,
                    dense,
                    err,
                    NULL,
                    FALSE));
}

/*##**********************************************************************\
 *
 *		tb_createseqandid
 *
 * Member of the SQL function block.
 * Function creates a new sequence into the database and returns it id.
 *
 * Parameters :
 *
 *	cd - in
 *		application state
 *
 *	trans - in
 *		handle to the current transaction
 *
 *	seqname - in
 *		name of the sequence to be created
 *
 *	schema - in
 *		schema name for the sequence (NULL if none)
 *
 *	catalog - in
 *		catalog name for the sequence (NULL if none)
 *
 *	dense - in
 *		1 if the sequence is to be dense, 0 if coarse
 *
 *      cont - in/out, give
 *          *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *	err - out, give
 *		in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 *  seqid - out, give
 *
 *      sequence identifier is stored here if seqid is non-NULL
 *
 * Return value :
 *
 *      1 if the operation is successful
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_createseqandid(
        rs_sysi_t* cd,
        sqltrans_t *trans,
        char *seqname,
        char *schema,
        char *catalog,
        char* extrainfo __attribute__ ((unused)),
        bool dense,
        void** cont,
        sqlerr_t **err,
        long* seq_id)
{
        *cont = NULL;
        return(tb_seq_create(
                    cd,
                    trans,
                    seqname,
                    schema,
                    catalog,
                    dense,
                    err,
                    seq_id,
                    TRUE));
}

/*##**********************************************************************\
 *
 *		tb_dropseq
 *
 * Member of the SQL function block.
 * Function drops a sequence from the database.
 *
 * Parameters :
 *
 *	cd - in
 *		application state
 *
 *	trans - in
 *		handle to the current transaction
 *
 *	seqname - in
 *		name of the sequence to be dropped
 *
 *	schema - in
 *		schema name for the sequence (NULL if none)
 *
 *	catalog - in
 *		catalog name for the sequence (NULL if none)
 *
 *	cascade - in
 *		1 = cascade, 0 = restrict
 *
 *      cont - in/out, give
 *          *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *	err - out, give
 *		in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *
 *      1 if the operation is successful
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dropseq(
        rs_sysi_t* cd,
        sqltrans_t *trans,
        char *seqname,
        char *schema,
        char *catalog,
        char* extrainfo __attribute__ ((unused)),
        bool cascade __attribute__ ((unused)),
        void** cont,
        sqlerr_t **err)
{
        char* findschema;
        char* findcatalog;

        bool succp;

        *cont = NULL;
        catalog = tb_catalog_resolve(cd, catalog);

        succp = tb_seq_find(
                    cd,
                    trans,
                    seqname,
                    schema,
                    catalog,
                    &findschema,
                    &findcatalog,
                    NULL,
                    NULL,
                    err);
        if (!succp) {
            return(FALSE);
        }

        succp = tb_seq_drop(
                    cd,
                    trans,
                    seqname,
                    findschema,
                    findcatalog,
                    err);

        SsMemFree(findschema);
        if (findcatalog != NULL) {
            SsMemFree(findcatalog);
        }
        return(succp);
}

#endif /* SS_NOSEQUENCE */

