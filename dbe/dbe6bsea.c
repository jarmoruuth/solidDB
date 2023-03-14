/*************************************************************************\
**  source       * dbe6bsea.c
**  directory    * dbe
**  description  * B+-tree search.
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

This module implements a B+-tree search.


Limitations:
-----------


Error handling:
--------------

Errors are handled using asserts.

Objects used:
------------

v-tuples                    uti0vtpl, uti0vcmp
key value system            dbe6bkey
key range search            dbe6bkrs
tree node                   dbe6bnod

Preconditions:
-------------


Multithread considerations:
--------------------------

The code is not reentrant.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define DBE6BSEA_C

#include <ssstdio.h>
#include <ssstring.h>
#include <sssetjmp.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <sssprint.h>
#include <ssthread.h>

#include <uti0va.h>
#include <uti0vtpl.h>
#include <uti0vcmp.h>

#include <su0error.h>
#include <su0list.h>
#include <su0slike.h>
#include <su0wlike.h>
#include <su0time.h>

#include <rs0pla.h>
#include <rs0sysi.h>

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe6bkrs.h"
#include "dbe6bnod.h"
#include "dbe6btre.h"
#include "dbe6bsea.h"
#include "dbe0trx.h"
#include "dbe0user.h"

#define LONGSEQSEA_NREADLEAFS   5

typedef enum {
        BSEA_GETNODE_NEXT,
        BSEA_GETNODE_PREV,
        BSEA_GETNODE_RESET
} bsea_getnode_type_t;

extern jmp_buf  ss_dbg_jmpbuf[SS_DBG_JMPBUF_MAX];
extern uint     ss_dbg_jmpbufpos;

extern dbe_bkey_t* dbe_curkey;

int         dbe_search_noindexassert = 0;
dbe_trxid_t dbe_bsea_disabletrxid;

/*#***********************************************************************\
 *
 *		bsea_errorprint
 *
 *
 *
 * Parameters :
 *
 *	bs -
 *
 *
 *	internal_call -
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
static void bsea_errorprint(dbe_btrsea_t* bs, bool internal_call)
{
        static bool already_here = FALSE;

        if (already_here) {
            return;
        }
        already_here = TRUE;

        if (bs == NULL) {
            SsDbgPrintf("dbe_btrsea_errorprint:NULL search pointer\n");
            already_here = FALSE;
            return;
        }

        dbe_btree_lock_exclusive(bs->bs_b);

        SsDbgFlush();

        if (internal_call) {
            char debugstring[48];
#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
            SsSprintf(
                debugstring,
                "/LOG/UNL/NOD/TID:%u/FLU",
                SsThrGetid());
#else
            SsSprintf(
                debugstring,
                "/LOG/LIM:100000000/NOD/TID:%u/FLU",
                SsThrGetid());
#endif
            SsDbgSet(debugstring);
        }

        SsDbgPrintf("dbe_btrsea_errorprint:\n");

        SsDbgPrintf("kc beginkey:\n");
        dbe_bkey_dprint(0, bs->bs_kc->kc_beginkey);
        SsDbgPrintf("kc endkey:\n");
        dbe_bkey_dprint(0, bs->bs_kc->kc_endkey);

        SsDbgPrintf("bkrs beginkey:\n");
        dbe_bkey_dprint(0, dbe_bkrs_getbeginkey(bs->bs_krs));
        SsDbgPrintf("bkrs endkey:\n");
        dbe_bkey_dprint(0, dbe_bkrs_getendkey(bs->bs_krs));

        SsDbgPrintf("search key:\n");
        dbe_bkey_dprint(0, dbe_srk_getbkey(bs->bs_srk));

        SsDbgPrintf("Current search node:\n");
        dbe_bnode_printtree(NULL, bs->bs_n, TRUE);
        if (dbe_curkey != NULL) {
            dbe_bkey_done(dbe_curkey);
            dbe_curkey = NULL;
        }

        SsDbgFlush();

        dbe_btree_unlock(bs->bs_b);

        already_here = FALSE;
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_checktimecons
 *
 * Checks the current key value against time constraints.
 *
 * Parameters :
 *
 *	bs - in, use
 *		btree search
 *
 *	p_trxresult - in, use
 *		btree search
 *
 * Return value :
 *
 *      TRUE    - current key value accepted
 *      FALSE   - current key value rejected
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_btrsea_checktimecons(
        dbe_btrsea_t* bs,
        dbe_btrsea_timecons_t* tc,
        dbe_bkey_t* k,
        bool bonsaip,
        bool validatesea,
        dbe_trxstate_t* p_trxresult)
{
        dbe_trxstate_t trxresult;
        dbe_trxid_t keytrxid;
        dbe_trxid_t usertrxid;
        dbe_trxnum_t committrxnum = DBE_TRXNUM_NULL;
        bool accept = FALSE;
        bool timeconsacceptall;

        ss_bprintf_3(("dbe_btrsea_checktimecons\n"));
        ss_debug(if (bs != NULL)) CHK_BTRSEA(bs);
        ss_dassert(p_trxresult != NULL);
        ss_dassert(tc != NULL);

        SS_PMON_ADD(SS_PMON_BNODE_KEYREAD);

        if (!bonsaip) {
            /* Key is from the permanent tree. Accept the key value.
             */
            ss_dprintf_3(("key from permanent tree\n"));
            ss_aassert(dbe_bkey_iscommitted(k));
            *p_trxresult = DBE_TRXST_COMMIT;
            return(TRUE);
        }

        ss_dassert(dbe_bkey_istrxnum(k));
        ss_dassert(dbe_bkey_istrxid(k));

        if (bs != NULL) {
            timeconsacceptall = bs->bs_timeconsacceptall;
        } else {
            timeconsacceptall = FALSE;
        }

        keytrxid = dbe_bkey_gettrxid(k);

        ss_dprintf_4(("keytrxid = %ld\n", DBE_TRXID_GETLONG(keytrxid)));

        if (dbe_bkey_iscommitted(k)) {
            if (!DBE_TRXID_ISNULL(dbe_bsea_disabletrxid)
            &&  DBE_TRXID_EQUAL(dbe_bsea_disabletrxid, keytrxid))
            {
                ss_dprintf_3(("trxid is disabled\n"));
                return(FALSE);
            }

            trxresult = DBE_TRXST_COMMIT;
            committrxnum = dbe_bkey_gettrxnum(k);
            ss_dassert(!DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL));
            usertrxid = dbe_bkey_gettrxid(k);
        } else {
            trxresult = dbe_trxbuf_gettrxstate(
                            tc->tc_trxbuf,
                            keytrxid,
                            &committrxnum,
                            &usertrxid);
            if (bs != NULL) {
                dbe_srk_settrxid(bs->bs_srk, usertrxid);
            }
        }

        if (dbe_cfg_newkeycheck
        &&  trxresult == DBE_TRXST_COMMIT
        &&  validatesea
        &&  bs->bs_keyvldtype != DBE_KEYVLD_READCOMMITTED_FORUPDATE
        &&  bs->bs_earlyvld
        &&  DBE_TRXID_CMP_EX(usertrxid, tc->tc_usertrxid) > 0)
        {
            /* Committed transaction with higher transaction id
             * than in current transaction. It means that the transaction
             * is started later and already committed. Treat the
             * transaction as in begin state during validate.
             */
            ss_dprintf_4(("set trxresult = DBE_TRXST_BEGIN\n"));
            trxresult = DBE_TRXST_BEGIN;
        }

        switch (trxresult) {

            case DBE_TRXST_BEGIN:
#ifdef DBE_REPLICATION
            case DBE_TRXST_TOBEABORTED:
#endif /* DBE_REPLICATION */
                /* The transaction that added key value is not yet ended.
                 */
                ss_dprintf_4(("key is uncertain, usertrxid = %ld\n", DBE_TRXID_GETLONG(usertrxid)));
                if (validatesea && bs->bs_earlyvld) {
                    /* During early trx validate, also unfinished
                     * transactions are accepted.
                     */
                    switch (bs->bs_keyvldtype) {
                        case DBE_KEYVLD_NONE:
                        case DBE_KEYVLD_READCOMMITTED_FORUPDATE:
                            /* Normal validate search, accept the key.
                             */
                            accept = TRUE;
                            break;
                        case DBE_KEYVLD_PRIMARY:
                            /* Check that child key value does not exist. */
                            /* FALLTHROUGH */
                        case DBE_KEYVLD_UNIQUE:
                            /* This search is used to do a unique key
                             * check. During unique key check with early
                             * validate, all inserts are accepted, but
                             * delete marks are rejected if they are not
                             * from the current user's transaction.
                             */
                            if (DBE_TRXID_EQUAL(usertrxid, tc->tc_usertrxid)) {
                                /* Current user's transaction. */
                                accept = TRUE;
                            } else {
                                accept = !dbe_bkey_isdeletemark(k) &&
                                         !dbe_bkey_isupdate(k);
                            }
                            break;
                        case DBE_KEYVLD_FOREIGN:
                            /* Check that parent key value does exists.
                             * Accept the key only if it is from the current
                             * user's transaction.
                             * We accept delete marks so that also
                             * potential deletes are active.
                             * Updates are ignored because it means values
                             * are not changed.
                             */
                            accept = (DBE_TRXID_EQUAL(usertrxid, tc->tc_usertrxid)) ||
                                     (dbe_bkey_isdeletemark(k) &&
                                     !dbe_bkey_isupdate(k));
                            break;
                        default:
                            ss_error;
                    }
                } else {
                    /* Accept the key only if it is from the current user's
                     * transaction.
                     */
                    dbe_trxid_t maxtrxid;
                    maxtrxid = tc->tc_maxtrxid;
                    if (timeconsacceptall) {
                        accept = TRUE;
                    } else {
                        accept = (DBE_TRXID_EQUAL(usertrxid, tc->tc_usertrxid)) &&
#ifdef SS_HSBG2
                                 (!DBE_TRXID_EQUAL(maxtrxid, DBE_TRXID_NULL)) &&
#endif
                                 (DBE_TRXID_CMP_EX(keytrxid, maxtrxid) <= 0);
                    }
                }
                break;

            case DBE_TRXST_VALIDATE:
                /* The transaction that added the key value is not yet ended
                 * but is currently under validation.
                 */
                ss_dprintf_4(("key is validate, committrxnum = %ld, usertrxid = %ld\n",
                    DBE_TRXNUM_GETLONG(committrxnum), DBE_TRXID_GETLONG(usertrxid)));
                ss_dassert(!DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL));
                if (validatesea) {
                    /* This search is used for transaction validation.
                     */
                    if (bs->bs_earlyvld) {
                        /* If early validate is used, transactions in validate
                         * phase are treated in the same way as in the begin
                         * phase.
                         */
                        switch (bs->bs_keyvldtype) {
                            case DBE_KEYVLD_NONE:
                            case DBE_KEYVLD_READCOMMITTED_FORUPDATE:
                                /* Normal validate search, accept the key.
                                 */
                                accept = TRUE;
                                break;
                            case DBE_KEYVLD_PRIMARY:
                                /* Check that child key value does not exist. */
                                /* FALLTHROUGH */
                            case DBE_KEYVLD_UNIQUE:
                                /* This search is used to do a unique key
                                 * check. During unique key check with early
                                 * validate, all inserts are accepted, but
                                 * delete marks are rejected if they are not
                                 * from the current user's transaction.
                                 */
                                if (DBE_TRXID_EQUAL(usertrxid, tc->tc_usertrxid)) {
                                    /* Current user's transaction. */
                                    accept = TRUE;
                                } else {
                                    accept = !dbe_bkey_isdeletemark(k) &&
                                             !dbe_bkey_isupdate(k);
                                }
                                break;
                            case DBE_KEYVLD_FOREIGN:
                                /* Check that parent key value does exists.
                                 * Accept the key only if it is from the
                                 * current user's transaction.
                                 * We accept delete marks so that also
                                 * potential deletes are active.
                                 */
                                accept = (DBE_TRXID_EQUAL(usertrxid, tc->tc_usertrxid)) ||
                                         (dbe_bkey_isdeletemark(k) &&
                                         !dbe_bkey_isupdate(k));
                                break;
                            default:
                                ss_error;
                        }
                    } else {
                        /* Accept the key if it is inside the allowed
                         * transaction time range.
                         */
                        if (timeconsacceptall) {
                            accept = TRUE;
                        } else {
                            accept = (DBE_TRXNUM_CMP_EX(committrxnum, tc->tc_mintrxnum) >= 0 &&
                                      DBE_TRXNUM_CMP_EX(committrxnum, tc->tc_maxtrxnum) <= 0);
                        }
                    }
                } else {
                    /* Accept the key only if it is the from current
                     * user's transaction.
                     */
                    accept = (DBE_TRXID_EQUAL(usertrxid, tc->tc_usertrxid));
                }
                break;

            case DBE_TRXST_COMMIT:
                /* The transaction that added key value is committed.
                 * In case of validate search, the caller is responsible
                 * for setting the correct transaction number range.
                 */
                ss_dprintf_4(("key is committed, committrxnum = %ld, tc->tc_mintrxnum = %ld, tc->tc_maxtrxnum = %ld\n", 
                    DBE_TRXNUM_GETLONG(committrxnum), DBE_TRXNUM_GETLONG(tc->tc_mintrxnum), DBE_TRXNUM_GETLONG(tc->tc_maxtrxnum)));
                ss_dassert(!DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL));
                /* Accept committed key value only if it is inside the
                 * allowed transaction time range.
                 */
                if (DBE_TRXNUM_ISNULL(tc->tc_mintrxnum) && DBE_TRXNUM_ISNULL(tc->tc_maxtrxnum)) {
                    /* Accept all. */
                    accept = TRUE;
                } else {
                    accept = (DBE_TRXNUM_CMP_EX(committrxnum, tc->tc_mintrxnum) >= 0 &&
                              DBE_TRXNUM_CMP_EX(committrxnum, tc->tc_maxtrxnum) <= 0);
                }
                if (accept) {
                    if (dbe_bkey_isdeletemark(k)) {
                        SS_PMON_ADD(SS_PMON_BNODE_KEYREADDELETE);
                    }
                } else {
                    SS_PMON_ADD(SS_PMON_BNODE_KEYREADOLDVERSION);
                }
                break;

            case DBE_TRXST_ABORT:
                /* The key value is from an aborted transaction, do not
                 * accept it.
                 */
                ss_dprintf_4(("key is aborted, usertrxid = %ld\n", DBE_TRXID_GETLONG(usertrxid)));
                accept = FALSE;
                SS_PMON_ADD(SS_PMON_BNODE_KEYREADABORT);
                break;

            default:
                ss_rc_error(trxresult);
        }
        ss_dprintf_3(("key is %s\n", accept ? "accepted" : "rejected"));

        *p_trxresult = trxresult;

        return(accept);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_checkkeycons
 *
 * Checks the current key value against key constraints. The check is
 * done only against additional constraints, range constraints are
 * already checked at the lower level (inside the node search).
 *
 * Parameters :
 *
 *	bs - in, use
 *		btree search
 *
 * Return value :
 *
 *      TRUE    - current key value accepted
 *      FALSE   - current key value rejected
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_btrsea_checkkeycons(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_srk_t* srk,
        su_list_t* conslist)
{
        int cmp;
        vtpl_vamap_t* vamap;
        va_t* keyva;
        va_t* consva;
        su_list_node_t* lnode;
        rs_pla_cons_t* cons;
        uint relop;
        bool keyva_isnull;
        bool accept = TRUE;

        ss_bprintf_3(("dbe_btrsea_checkkeycons\n"));

        if (conslist == NULL) {
            return(TRUE);
        }

        lnode = su_list_first(conslist);
        if (lnode == NULL) {
            ss_dprintf_4(("dbe_btrsea_checkkeycons:empty conslist, accept\n"));
            return(TRUE);
        }

        vamap = dbe_srk_getvamap(srk);

        do {
            cons = su_listnode_getdata(lnode);

            consva = RS_PLA_CONS_VA(cd, cons);
            relop = RS_PLA_CONS_RELOP(cd, cons);
            keyva = vtpl_vamap_getva_at(
                        vamap,
                        RS_PLA_CONS_KPINDEX(cd, cons));
            if (keyva == VA_DEFAULT) {
                ss_dprintf_4(("dbe_btrsea_checkkeycons:VA_DEFAULT\n"));
                if (RS_PLA_CONS_DEFVA(cd, cons) != NULL) {
                    ss_dprintf_4(("dbe_btrsea_checkkeycons:is pla cons defva\n"));
                    keyva = RS_PLA_CONS_DEFVA(cd, cons);
                }
            }
            keyva_isnull = va_testnull(keyva);

            /* VOYAGER_SYNCHIST */
            ss_dprintf_3(("dbe_btrsea_checkkeycons:include nulls=%d, keyva_isnull=%d\n", RS_PLA_CONS_INCLUDENULLS(cd, cons), keyva_isnull));

            if (keyva_isnull && RS_PLA_CONS_INCLUDENULLS(cd, cons)) {
                lnode = su_list_next(conslist, lnode);
                continue;
            }

            switch (relop) {
                case RS_RELOP_ISNULL:
                    if (!keyva_isnull) {
                        ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_ISNULL, key not null, reject\n"));
                        return(FALSE);
                    }
                    break;
                case RS_RELOP_ISNOTNULL:
                    if (keyva_isnull) {
                        ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_ISNOTNULL, key null, reject\n"));
                        return(FALSE);
                    }
                    break;
                default:
                    /* WARNING! This return should be only temporary and
                     *          should be replaced with assert.
                     */
                    if (va_testnull(consva)) {
                        /* Accept for new sql version. */
                        ss_dprintf_4(("dbe_btrsea_checkkeycons:va_testnull(consva), reject\n"));
                        return(FALSE);
                    }
                    if (keyva_isnull) {
                        ss_dprintf_4(("dbe_btrsea_checkkeycons:keyva_isnull, reject\n"));
                        return(FALSE);
                    }
#ifdef SS_UNICODE_DATA
                    if (relop == RS_RELOP_LIKE) {
                        void* key;
                        va_index_t keylen;
                        void* constraint;
                        va_index_t conslen;
                        int esc;

                        key = va_getdata(keyva, &keylen);
                        keylen--;
                        constraint = va_getdata(consva, &conslen);
                        conslen--;
                        esc = RS_PLA_CONS_ESCCHAR(cd, cons);

                        if (va_testblob(keyva)) {
                            ss_dassert(keylen >= RS_KEY_MAXCMPLEN);
                            keylen = RS_KEY_MAXCMPLEN;
                            if ((keylen & 1) != 0) {
                                keylen--;
                            }

                        }
                        if (RS_PLA_CONS_ISUNICODE(cd, cons)) {
                            ss_dassert(!RS_PLA_CONS_UNIFORCHAR(cd, cons));
                            ss_dassert((keylen & 1) == 0);
                            ss_dassert((conslen & 1) == 0);
                            keylen /= sizeof(ss_char2_t);
                            conslen /= sizeof(ss_char2_t);
                            accept = su_wlike(
                                        key,
                                        keylen,
                                        constraint,
                                        conslen,
                                        esc,
                                        TRUE); /* va_format */
                        } else if (RS_PLA_CONS_UNIFORCHAR(cd, cons)) {
                            ss_dassert((keylen & 1) == 0);
                            keylen /= sizeof(ss_char2_t);
                            accept = su_wslike(
                                        key,
                                        keylen,
                                        constraint,
                                        conslen,
                                        esc,
                                        TRUE); /* va_format */
                        } else {
                            accept = su_slike(
                                        key,
                                        keylen,
                                        constraint,
                                        conslen,
                                        esc);
                        }
                        if (!accept) {
                            ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_LIKE, reject\n"));
                            return(FALSE);
                        }
                    } else {
                        if (RS_PLA_CONS_UNIFORCHAR(cd, cons)) {
                            /* constraint is UNICODE, column is CHAR */
                            cmp = va_compare_char1v2(keyva, consva);
                            ss_dprintf_4(("dbe_btrsea_checkkeycons:va_compare_char1v2, cmp=%d\n", cmp));
                        } else {
#ifdef SS_COLLATION
                            if (RS_PLA_CONS_ISCOLLATED(cd, cons)) {
                                rs_cons_t* rscons;
                                rs_atype_t* rscons_atype;
                                rs_aval_t* rscons_aval;
                                rs_aval_t key_avalbuf;
                                bool succp;
                                
                                rscons = rs_pla_cons_get_rscons(cd, cons);
                                ss_dassert(rscons != NULL);
                                rscons_atype = rs_cons_atype(cd, rscons);
                                rscons_aval = rs_cons_aval(cd, rscons);
                                ss_dassert(rs_atype_collation(cd, rscons_atype)
                                           != NULL);
                                rs_aval_createbuf(cd, rscons_atype,
                                                  &key_avalbuf);
                                rs_aval_setva(cd, rscons_atype, &key_avalbuf,
                                              keyva);
                                cmp = rs_aval_cmp3_notnull(
                                        cd,
                                        rscons_atype, &key_avalbuf,
                                        rscons_atype, rscons_aval,
                                        &succp, NULL);
                                ss_dassert(succp);
                                rs_aval_freebuf(cd,
                                                rscons_atype,
                                                &key_avalbuf);
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_PLA_CONS_ISCOLLATED, cmp=%d\n", cmp));
                            } else
#endif /* SS_COLLATION */
                            {
                                cmp = va_compare(keyva, consva);
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:va_compare, cmp=%d\n", cmp));
                            }
                        }
                        switch (relop) {
                            case RS_RELOP_EQUAL:
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_EQUAL\n"));
                                if (!(cmp == 0)) {
                                    accept = FALSE;
                                    goto return_accept;
                                }
                                break;
                            case RS_RELOP_NOTEQUAL:
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_NOTEQUAL\n"));
                                if (!(cmp != 0)) {
                                    accept = FALSE;
                                    goto return_accept;
                                }
                                break;
                            case RS_RELOP_LT:
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_LT\n"));
                                if (!(cmp < 0)) {
                                    accept = FALSE;
                                    goto return_accept;
                                }
                                break;
                            case RS_RELOP_LE:
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_LE\n"));
                                if (!(cmp <= 0)) {
                                    accept = FALSE;
                                    goto return_accept;
                                }
                                break;
                            case RS_RELOP_GT:
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_GT\n"));
                                if (!(cmp > 0)) {
                                    accept = FALSE;
                                    goto return_accept;
                                }
                                break;
                            case RS_RELOP_GE:
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_GE\n"));
                                if (!(cmp >= 0)) {
                                    accept = FALSE;
                                    goto return_accept;
                                }
                                break;
                            case RS_RELOP_GT_VECTOR:
                            case RS_RELOP_GE_VECTOR:
                            case RS_RELOP_LT_VECTOR:
                            case RS_RELOP_LE_VECTOR:
                                ss_dprintf_4(("dbe_btrsea_checkkeycons:RS_RELOP_*_VECTOR\n"));
                                break;
                            default:
                                ss_rc_error(relop);
                        }
                    }
#else /* SS_UNICODE_DATA */
                    if (relop != RS_RELOP_LIKE) {
                        cmp = va_compare(keyva, consva);
                    }
                    switch (relop) {
                        case RS_RELOP_EQUAL:
                            if (!(cmp == 0)) {
                                return(FALSE);
                            }
                            break;
                        case RS_RELOP_NOTEQUAL:
                            if (!(cmp != 0)) {
                                return(FALSE);
                            }
                            break;
                        case RS_RELOP_LT:
                            if (!(cmp < 0)) {
                                return(FALSE);
                            }
                            break;
                        case RS_RELOP_LE:
                            if (!(cmp <= 0)) {
                                return(FALSE);
                            }
                            break;
                        case RS_RELOP_GT:
                            if (!(cmp > 0)) {
                                return(FALSE);
                            }
                            break;
                        case RS_RELOP_GE:
                            if (!(cmp >= 0)) {
                                return(FALSE);
                            }
                            break;
                        case RS_RELOP_GT_VECTOR:
                        case RS_RELOP_GE_VECTOR:
                        case RS_RELOP_LT_VECTOR:
                        case RS_RELOP_LE_VECTOR:
                            break;
                        case RS_RELOP_LIKE:
                            accept = su_slike(
                                        VA_GETASCIIZ(keyva),
                                        VA_NETLEN(keyva) - 1,
                                        VA_GETASCIIZ(consva),
                                        VA_NETLEN(consva) - 1,
                                        RS_PLA_CONS_ESCCHAR(cd, cons));
                            if (!accept) {
                                return(FALSE);
                            }
                            break;
                        default:
                            ss_rc_error(relop);
                    }
#endif /* SS_UNICODE_DATA */
                    break;
            }
            lnode = su_list_next(conslist, lnode);
        } while (lnode != NULL);

return_accept:
        ss_dprintf_3(("key is %s\n", accept ? "accepted" : "rejected"));

        return(accept);
}

/*#**********************************************************************\
 *
 *		btrsea_nextorprevnode_nolock
 *
 * Retrieves the next tree node in a normal search without B-tree gate lock.
 *
 * Parameters :
 *
 *	bs - in out, use
 *		pointer to a tree search structure
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool btrsea_nextorprevnode_nolock(
        dbe_btrsea_t* bs, 
        bsea_getnode_type_t type)
{
        su_daddr_t addr = 0;
        su_daddr_t rootaddr;
        dbe_bnode_t* n;
        dbe_bnode_t* tmpn;
        volatile su_daddr_t* p_rootaddr;
        int level;
        su_profile_timer;

        ss_pprintf_3(("btrsea_nextorprevnode_nolock:type=%d, lockingread=%d\n", type, bs->bs_lockingread));
        CHK_BTRSEA(bs);

        ss_dassert(!bs->bs_mergesea);
        ss_dassert(bs->bs_tmpn == NULL);
        ss_dassert(!bs->bs_pessimistic || !dbe_cfg_usepessimisticgate || SsSemStkFind(SS_SEMNUM_DBE_PESSGATE));

        if (bs->bs_lockingread && dbe_cfg_usenewbtreelocking) {
            return(FALSE);
        }

        if (dbe_btree_getrootlevel_nomutex(bs->bs_b) == 0) {
            return(FALSE);
        }

        if (bs->bs_n != NULL) {
            dbe_bnode_write(bs->bs_n, bs->bs_longseqsea);
            bs->bs_n = NULL;
        }

        su_profile_start;

        p_rootaddr = (volatile su_daddr_t*)&bs->bs_b->b_rootaddr;

        SU_BFLAG_SET(bs->bs_info->i_flags, DBE_INFO_IGNOREWRONGBNODE);

        rootaddr = *p_rootaddr;

        ss_pprintf_3(("btrsea_nextorprevnode_nolock:rootaddr=%ld\n", (long)rootaddr));

        n = dbe_bnode_getreadonly(bs->bs_go, rootaddr, bs->bs_bonsaip, bs->bs_info);

        if (n == NULL || rootaddr != *p_rootaddr) {
            ss_pprintf_3(("btrsea_nextorprevnode_nolock:%s\n", n == NULL ? "n == NULL" : "addr != *p_rootaddr"));
            if (n != NULL) {
                dbe_bnode_write(n, FALSE);
            }
            goto node_nolock_failed;
        }

        level = dbe_bnode_getlevel(n);
        if (level == 0) {
            ss_pprintf_3(("btrsea_nextorprevnode_nolock:level == 0\n"));
            dbe_bnode_write(n, FALSE);
            goto node_nolock_failed;
        }

        for (;;) {

            ss_poutput_3({ SsThrSwitch(); });

            switch (type) {
                case BSEA_GETNODE_NEXT:
                    addr = dbe_bnode_rsea_nextnode(
                                n,
                                bs->bs_krs,
                                bs->bs_longseqsea,
                                bs->bs_readaheadsize,
                                bs->bs_info);
                    break;
                case BSEA_GETNODE_PREV:
                    addr = dbe_bnode_rsea_prevnode(
                                n,
                                bs->bs_krs,
                                bs->bs_kc->kc_cd);
                    break;
                case BSEA_GETNODE_RESET:
                    addr = dbe_bnode_rsea_resetnode(
                                n,
                                bs->bs_krs,
                                bs->bs_kc->kc_cd);
                    break;
                default:
                    ss_error;
            }

            ss_pprintf_3(("btrsea_nextorprevnode_nolock:addr=%ld\n", (long)addr));

            tmpn = dbe_bnode_getreadonly(bs->bs_go, addr, bs->bs_bonsaip, bs->bs_info);

            if (tmpn == NULL || dbe_bnode_ischanged(n)) {
                ss_pprintf_3(("btrsea_nextorprevnode_nolock:%s\n", tmpn == NULL ? "tmpn == NULL" : "dbe_bnode_ischanged(n)"));
                dbe_bnode_write(n, FALSE);
                if (tmpn != NULL) {
                    dbe_bnode_write(tmpn, FALSE);
                }
                goto node_nolock_failed;
            }

            if (rootaddr != 0 && rootaddr != *p_rootaddr) {
                ss_pprintf_3(("btrsea_nextorprevnode_nolock:2nd check, rootaddr != *p_rootaddr\n"));
                dbe_bnode_write(n, FALSE);
                dbe_bnode_write(tmpn, FALSE);
                goto node_nolock_failed;
            }
            rootaddr = 0;

            dbe_bnode_write(n, FALSE);

            n = tmpn;

            level = dbe_bnode_getlevel(n);
            if (level == 0) {
                break;
            }
        }

        SU_BFLAG_CLEAR(bs->bs_info->i_flags, DBE_INFO_IGNOREWRONGBNODE);

        if (type == BSEA_GETNODE_NEXT && bs->bs_longseqsea == 1) {
            bs->bs_longseqsea = 2;
        }
        bs->bs_n = n;

        SU_BFLAG_CLEAR(bs->bs_info->i_flags, DBE_INFO_IGNOREWRONGBNODE);

        su_profile_stop("btrsea_nextorprevnode_nolock:success");
        ss_pprintf_3(("btrsea_nextorprevnode_nolock:success\n"));

        return(TRUE);

node_nolock_failed:

        SU_BFLAG_CLEAR(bs->bs_info->i_flags, DBE_INFO_IGNOREWRONGBNODE);

        su_profile_stop("btrsea_nextorprevnode_nolock:failed");
        ss_pprintf_3(("btrsea_nextorprevnode_nolock:failed\n"));

        return(FALSE);
}

/*#**********************************************************************\
 *
 *		btrsea_nextorprevnode
 *
 * Retrieves the next tree node in a normal search.
 *
 * Parameters :
 *
 *	bs - in out, use
 *		pointer to a tree search structure
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void btrsea_nextorprevnode(
        dbe_btrsea_t* bs, 
        bsea_getnode_type_t type)
{
        su_daddr_t addr = 0;
        dbe_bnode_t* n;
        dbe_bnode_t* rootnode;
        dbe_bnode_t* tmpn;
        su_profile_timer;

        ss_bprintf_3(("btrsea_nextorprevnode:type=%d, lockingread=%d\n", type, bs->bs_lockingread));
        CHK_BTRSEA(bs);

        ss_dassert(!bs->bs_mergesea);
        ss_dassert(bs->bs_tmpn == NULL);
        ss_dassert(!bs->bs_pessimistic || !dbe_cfg_usepessimisticgate || SsSemStkFind(SS_SEMNUM_DBE_PESSGATE));

        if ((dbe_cfg_relaxedbtreelocking & 2) && btrsea_nextorprevnode_nolock(bs, type)) {
            SS_PMON_ADD(SS_PMON_BTREE_GET_NOLOCK);
            return;
        }

        SS_PMON_ADD(SS_PMON_BTREE_GET_SHAREDLOCK);

        if (bs->bs_n != NULL) {
            dbe_bnode_write(bs->bs_n, bs->bs_longseqsea);
            bs->bs_n = NULL;
        }

        dbe_btree_lock_shared(bs->bs_b);
        su_profile_start;
        bs->bs_unlock_tree = TRUE;

        rootnode = dbe_btree_getrootnode_nomutex(bs->bs_b);
        if (rootnode != NULL) {
            n = rootnode;
        } else {
            addr = dbe_btree_getrootaddr_nomutex(bs->bs_b);
            /* if (dbe_btree_getheight(bs->bs_b) == 1 && !bs->bs_validatesea) { */
            n = dbe_bnode_getreadonly(bs->bs_go, addr, bs->bs_bonsaip, bs->bs_info);
        }

        if (n == NULL) {
            dbe_btree_unlock(bs->bs_b);
            bs->bs_unlock_tree = FALSE;
            su_profile_stop("btrsea_nextorprevnode");
            ss_dassert(dbe_search_noindexassert);
            bs->bs_n = NULL;
            return;
        }

        for (;;) {
            int level;
            bool readonlyp;

            level = dbe_bnode_getlevel(n);
            if (level == 0) {
                break;
            }

            /* If current level is 1, the next node is from level zero
             * which we should get in read only mode. Levels above zero
             * should be got in read/write mode to ensure that the path
             * is correct.
             *
             * JarmoR Oct 1, 2002
             * Why we can not get all nodes in read-only mode?
             * is it because then we may get an onld copy which is not
             * correctly pointing to next level? Read-only nodes
             * do not get locked at all in cache which means lower leves
             * can be changed and thus may invalidate the pointers in
             * upper levels.
             *
             * Now mutex protocol changed so that during normal operations
             * B-tree is entered in shared mode and when we might need to
             * relocate whole path we enter in exclusive mode.
             */
            /* readonlyp = (level == 1 && !bs->bs_validatesea); */
            /* readonlyp = (level == 1); */
            if (bs->bs_lockingread && dbe_cfg_usenewbtreelocking) {
                readonlyp = (level > 1);
            } else {
                readonlyp = TRUE;
            }
            switch (type) {
                case BSEA_GETNODE_NEXT:
                    addr = dbe_bnode_rsea_nextnode(
                                n,
                                bs->bs_krs,
                                bs->bs_longseqsea,
                                bs->bs_readaheadsize,
                                bs->bs_info);
                    break;
                case BSEA_GETNODE_PREV:
                    addr = dbe_bnode_rsea_prevnode(
                                n,
                                bs->bs_krs,
                                bs->bs_kc->kc_cd);
                    break;
                case BSEA_GETNODE_RESET:
                    addr = dbe_bnode_rsea_resetnode(
                                n,
                                bs->bs_krs,
                                bs->bs_kc->kc_cd);
                    break;
                default:
                    ss_error;
            }
            if (readonlyp) {
                tmpn = dbe_bnode_getreadonly(bs->bs_go, addr, bs->bs_bonsaip, bs->bs_info);
            } else {
                tmpn = dbe_bnode_getreadwrite_search(bs->bs_go, addr, bs->bs_bonsaip, level-1, bs->bs_info);
            }
            if (n != rootnode) {
                dbe_bnode_write(n, FALSE);
            }
            rootnode = NULL;
            n = tmpn;
            if (bs->bs_unlock_tree && dbe_cfg_usenewbtreelocking) {
                dbe_btree_unlock(bs->bs_b);
                bs->bs_unlock_tree = FALSE;
            }
        }

        if (bs->bs_unlock_tree) {
            if (!dbe_cfg_usenewbtreelocking || !bs->bs_lockingread) {
                dbe_btree_unlock(bs->bs_b);
                bs->bs_unlock_tree = FALSE;
            }
        }
        su_profile_stop("btrsea_nextorprevnode");

        if (type == BSEA_GETNODE_NEXT && bs->bs_longseqsea == 1) {
            bs->bs_longseqsea = 2;
        }
        ss_aassert(n != rootnode);
        bs->bs_n = n;
}

/*#**********************************************************************\
 *
 *		btrsea_merge_nextnode_locked
 *
 * Retrieves the next tree node in a merge search. A copy of the
 * block is taken to the bs->bs_n. This used only during merge
 * search. Also a node cleanup operation is done and the node is
 * relocateed if necessary. After ths call the node is no more
 * reserved for the search.
 *
 * Parameters :
 *
 *	bs - in out, use
 *		pointer to a tree search structure
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t btrsea_merge_nextnode_locked(dbe_btrsea_t* bs)
{
        su_daddr_t addr;
        dbe_bnode_t* n;
        dbe_bnode_t* rootnode;
        su_list_t* path;
        dbe_ret_t rc;
        dbe_bkey_t* k;
        bool shrink = FALSE;
        long nkeyremoved;
        long nmergeremoved;
        int nkeys;
        dbe_pathinfo_t pathinfo;
        su_profile_timer;

        ss_bprintf_3(("btrsea_merge_nextnode_locked\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_mergesea);
        ss_dassert(!bs->bs_pessimistic);

        path = su_list_init(NULL);

        dbe_btree_lock_exclusive(bs->bs_b);
        su_profile_start;

        ss_bprintf_4(("btrsea_merge_nextnode_locked:rw:addr=%ld, nodelevel=%d\n",
            dbe_btree_getrootaddr_nomutex(bs->bs_b),
            dbe_btree_getrootlevel_nomutex(bs->bs_b)));

        if (dbe_btree_getrootlevel_nomutex(bs->bs_b) > 0 && (dbe_cfg_usenewbtreelocking || dbe_cfg_relaxedbtreelocking)) {
            n = dbe_bnode_getreadwrite_nocopy(
                    bs->bs_go,
                    dbe_btree_getrootaddr_nomutex(bs->bs_b),
                    bs->bs_bonsaip,
                    dbe_btree_getrootlevel_nomutex(bs->bs_b),
                    bs->bs_info);
        } else {
            n = dbe_bnode_getreadwrite(
                    bs->bs_go,
                    dbe_btree_getrootaddr_nomutex(bs->bs_b),
                    bs->bs_bonsaip,
                    dbe_btree_getrootlevel_nomutex(bs->bs_b),
                    bs->bs_info);
        }
        su_list_insertlast(path, n);

        rootnode = n;

        for (;;) {
            int level;

            level = dbe_bnode_getlevel(n);
            if (level == 0) {
                break;
            }
            addr = dbe_bnode_rsea_nextnode(
                        n,
                        bs->bs_krs,
                        bs->bs_longseqsea,
                        bs->bs_readaheadsize,
                        bs->bs_info);

            ss_bprintf_4(("btrsea_merge_nextnode_locked:rw:addr=%ld, nodelevel=%d\n", addr, level-1));
            if (level > 1 && (dbe_cfg_usenewbtreelocking || dbe_cfg_relaxedbtreelocking)) {
                n = dbe_bnode_getreadwrite_nocopy(bs->bs_go, addr, bs->bs_bonsaip, level-1, bs->bs_info);
            } else {
                n = dbe_bnode_getreadwrite(bs->bs_go, addr, bs->bs_bonsaip, level-1, bs->bs_info);
            }
            su_list_insertlast(path, n);
        }

        if (bs->bs_longseqsea == 1) {
            bs->bs_longseqsea = 2;
        }
        nkeyremoved = 0;
        nmergeremoved = 0;

        rc = dbe_bnode_cleanup(
                n,
                &nkeyremoved,
                &nmergeremoved,
                bs->bs_kc->kc_cd,
                DBE_BNODE_CLEANUP_MERGE_ALL);
        switch (rc) {
            case DBE_RC_SUCC:
                if (n == rootnode) {
                    dbe_btree_replacerootnode(bs->bs_b, rootnode);
                }
                break;
            case DBE_RC_NODERELOCATE:
                ss_bprintf_4(("btrsea_merge_nextnode_locked:DBE_RC_NODERELOCATE\n"));
                ss_dassert(nkeyremoved == 0);
                ss_dassert(nmergeremoved == 0);
                ss_dassert(bs->bs_info != NULL);
                rc = dbe_btree_nodepath_relocate(path, bs->bs_b, bs->bs_info);
                if (rc == SU_SUCCESS) {
                    su_list_node_t* ln;
                    ln = su_list_last(path);
                    n = su_listnode_getdata(ln);
                    ss_aassert(dbe_bnode_getlevel(n) == 0);
                    rc = dbe_bnode_cleanup(
                            n,
                            &nkeyremoved,
                            &nmergeremoved,
                            bs->bs_kc->kc_cd,
                            DBE_BNODE_CLEANUP_MERGE_ALL);
                    if (rc == DBE_RC_SUCC && n == rootnode) {
                        dbe_btree_replacerootnode(bs->bs_b, rootnode);
                    }
                }
                if (rc != DBE_RC_NODEEMPTY) {
                    break;
                }
                /* Fall to DBE_RC_NODEEMPTY. */
            case DBE_RC_NODEEMPTY:
                /* The last key value is not deleted, because it would
                   become empty but it should be deleted. */
                ss_bprintf_4(("btrsea_merge_nextnode_locked:DBE_RC_NODEEMPTY\n"));
                nkeys = dbe_bnode_getkeycount(n);
                k = dbe_bkey_init_ex(bs->bs_kc->kc_cd, bs->bs_go->go_bkeyinfo);
                ss_aassert(nkeys == 1);
                dbe_bkey_copy(k, dbe_bnode_getfirstkey(n));
                rc = dbe_btree_delete_empty(
                        bs->bs_b,
                        k, path,
                        TRUE,
                        &shrink,
                        TRUE,
                        &pathinfo,
                        bs->bs_kc->kc_cd,
                        bs->bs_info);
                dbe_bkey_done_ex(bs->bs_kc->kc_cd, k);
                break;
            default:
                su_rc_derror(rc);
                break;
        }

        if (rc == DBE_RC_SUCC || rc == DBE_RC_FIRSTLEAFKEY) {
            ss_dassert(bs->bs_tmpn != NULL);
            dbe_bnode_copy_tmp(bs->bs_tmpn, n);
        }

        dbe_btree_nodepath_done(path);

        if ((rc == DBE_RC_SUCC || rc == DBE_RC_FIRSTLEAFKEY) && shrink) {
            dbe_btree_shrink_tree(bs->bs_b, bs->bs_info);
        }

        bs->bs_nkeyremoved += nkeyremoved;
        bs->bs_nmergeremoved += nmergeremoved;

        if (rc == DBE_RC_FIRSTLEAFKEY) {
            dbe_btree_updatepathinfo(bs->bs_b, &pathinfo, bs->bs_kc->kc_cd, bs->bs_info);
            rc = DBE_RC_SUCC;
        }

        dbe_btree_unlock(bs->bs_b);
        su_profile_stop("btrsea_merge_nextnode_locked");

        return(rc);
}

/*#**********************************************************************\
 *
 *		btrsea_merge_nextnode
 *
 * Retrieves the next tree node in a merge search. A copy of the
 * block is taken to the bs->bs_n. This used only during merge
 * search. Also a node cleanup operation is done and the node is
 * relocateed if necessary. After ths call the node is no more
 * reserved for the search.
 *
 * Parameters :
 *
 *	bs - in out, use
 *		pointer to a tree search structure
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t btrsea_merge_nextnode(dbe_btrsea_t* bs)
{
        su_daddr_t addr;
        dbe_bnode_t* n;
        dbe_bnode_t* rootnode;
        dbe_ret_t rc;
        long nkeyremoved;
        long nmergeremoved;
        dbe_bkrs_t* tmpkrs;
        su_profile_timer;

        ss_bprintf_3(("btrsea_merge_nextnode\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_mergesea);
        ss_dassert(!bs->bs_pessimistic);

        dbe_btree_lock_shared(bs->bs_b);
        su_profile_start;
        bs->bs_unlock_tree = TRUE;

        if (dbe_btree_getheight(bs->bs_b) == 1) {
            ss_bprintf_4(("btrsea_merge_nextnode:rw:addr=%ld, nodelevel=%d\n",
                    dbe_btree_getrootaddr_nomutex(bs->bs_b),
                    dbe_btree_getrootlevel_nomutex(bs->bs_b)));

            dbe_btree_unlock(bs->bs_b);
            bs->bs_unlock_tree = FALSE;
            su_profile_stop("btrsea_merge_nextnode");

            return(btrsea_merge_nextnode_locked(bs));
        }

        ss_bprintf_4(("btrsea_merge_nextnode:r:addr=%ld\n",
                dbe_btree_getrootaddr_nomutex(bs->bs_b)));
        n = dbe_bnode_getreadonly(
                bs->bs_go,
                dbe_btree_getrootaddr_nomutex(bs->bs_b),
                bs->bs_bonsaip,
                bs->bs_info);

        rootnode = n;

        tmpkrs = dbe_bkrs_copy(bs->bs_krs);

        for (;;) {
            int level;
            dbe_bnode_t* prevn;

            level = dbe_bnode_getlevel(n);
            if (level == 0) {
                break;
            }
            prevn = n;
            addr = dbe_bnode_rsea_nextnode(
                        n,
                        bs->bs_krs,
                        bs->bs_longseqsea,
                        bs->bs_readaheadsize,
                        bs->bs_info);
            ss_bprintf_4(("btrsea_merge_nextnode:r:addr=%ld, nodelevel=%d\n", addr, level));
            if (level == 1) {
                ss_bprintf_4(("btrsea_merge_nextnode:rw:addr=%ld, nodelevel=%d\n", addr, level));
                n = dbe_bnode_getreadwrite(bs->bs_go, addr, bs->bs_bonsaip, level-1, bs->bs_info);
            } else {
                ss_bprintf_4(("btrsea_merge_nextnode:r:addr=%ld, nodelevel=%d\n", addr, level));
                n = dbe_bnode_getreadonly(bs->bs_go, addr, bs->bs_bonsaip, bs->bs_info);
            }
            dbe_bnode_write(prevn, FALSE);
            if (bs->bs_unlock_tree && dbe_cfg_usenewbtreelocking) {
                dbe_btree_unlock(bs->bs_b);
                bs->bs_unlock_tree = FALSE;
            }
            rootnode = NULL;
        }

        if (bs->bs_longseqsea == 1) {
            bs->bs_longseqsea = 2;
        }
        nkeyremoved = 0;
        nmergeremoved = 0;

        rc = dbe_bnode_cleanup(
                n,
                &nkeyremoved,
                &nmergeremoved,
                bs->bs_kc->kc_cd,
                DBE_BNODE_CLEANUP_MERGE);
        switch (rc) {
            case DBE_RC_SUCC:
                ss_aassert(n != rootnode);
                break;
            case DBE_RC_NODERELOCATE:
            case DBE_RC_NODEEMPTY:
                /* Restart this setp. */
                dbe_bnode_write(n, FALSE);
                if (bs->bs_unlock_tree) {
                    dbe_btree_unlock(bs->bs_b);
                    bs->bs_unlock_tree = FALSE;
                }
                su_profile_stop("btrsea_merge_nextnode");
                dbe_bkrs_done(bs->bs_krs);
                bs->bs_krs = tmpkrs;
                return(btrsea_merge_nextnode_locked(bs));
            default:
                su_rc_derror(rc);
                break;
        }

        if (rc == DBE_RC_SUCC) {
            ss_dassert(bs->bs_tmpn != NULL);
            dbe_bnode_copy_tmp(bs->bs_tmpn, n);
        }

        dbe_bnode_write(n, FALSE);

        if (bs->bs_unlock_tree) {
            dbe_btree_unlock(bs->bs_b);
            bs->bs_unlock_tree = FALSE;
        }
        su_profile_stop("btrsea_merge_nextnode");

        bs->bs_nkeyremoved += nkeyremoved;
        bs->bs_nmergeremoved += nmergeremoved;

        dbe_bkrs_done(tmpkrs);

        return(rc);
}


/*##**********************************************************************\
 *
 *		dbe_btrsea_initbufvalidate_ex
 *
 * Initializes a range search in the index tree.
 *
 * Parameters :
 *
 *	bs - use
 *
 *
 *	b - in, hold
 *		index tree
 *
 *	kc - in, hold
 *		key range constraints
 *
 *	tc - in, hold
 *		time range constraints, or NULL if no constraints
 *
 *	mergesea - in
 *		if TRUE, this is a merge search. During merge search aborted
 *		key values are removed and transaction numbers are patched
 *
 *	validatesea - in
 *		If TRUE, this is a validate search.
 *
 *	earlyvld - in
 *		If TRUE, early validate is used
 *
 * Return value - give :
 *
 *      Btree search pointer.
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_btrsea_initbufvalidate_ex(
        dbe_btrsea_t* bs,
        dbe_btree_t* b,
        dbe_btrsea_keycons_t* kc,
        dbe_btrsea_timecons_t* tc,
        bool mergesea,
        bool validatesea,
        dbe_keyvld_t keyvldtype,
        bool earlyvld,
        bool lockingread,
        bool pessimistic)
{
        bool succp;

        ss_bprintf_1(("dbe_btrsea_initbufvalidate_ex:id=%ld, lockingread=%d, bonsaip=%d, pessimistic=%d\n", (long)bs, lockingread, b->b_bonsaip, pessimistic));
        ss_dassert(kc != NULL);
        ss_dassert(mergesea ? (!validatesea && !earlyvld) : TRUE);
        SS_MEMOBJ_INC(SS_MEMOBJ_BTRSEA, dbe_btrsea_t);

        if (!validatesea) {
            earlyvld = FALSE;
        }

        ss_debug(bs->bs_chk = DBE_CHK_BTRSEA;)
        bs->bs_b = b;
        bs->bs_go = dbe_btree_getgobj(b);
        bs->bs_pos = BSEA_POS_FIRST;
        bs->bs_krs = dbe_bkrs_init(
                        kc->kc_cd, 
                        bs->bs_go->go_bkeyinfo,
                        kc->kc_beginkey,
                        kc->kc_endkey,
                        mergesea
                            ? (bs->bs_info->i_flags & DBE_INFO_OPENRANGEEND)
                            : FALSE);
        ss_debug(bs->bs_krs->krs_btree = b);
        if (mergesea) {
            bs->bs_tmpn = dbe_bnode_init_tmp(bs->bs_go);
        } else {
            bs->bs_tmpn = NULL;
        }
        bs->bs_n = bs->bs_tmpn;
        bs->bs_srk = dbe_srk_initbuf(
                        &bs->bs_srk_buf, 
                        kc->kc_cd, 
                        bs->bs_go->go_bkeyinfo);
        if (mergesea) {
            ss_dassert(tc != NULL);
            bs->bs_peeksrk = dbe_srk_initbuf(
                                &bs->bs_peeksrk_buf,
                                kc->kc_cd, 
                                bs->bs_go->go_bkeyinfo);
        } else {
            bs->bs_peeksrk = NULL;
        }
        bs->bs_peeked = FALSE;
        bs->bs_kc = kc;
        bs->bs_tc = tc;
        bs->bs_mergesea = mergesea;
        bs->bs_validatesea = validatesea;
        bs->bs_keyvldtype = keyvldtype;
        bs->bs_earlyvld = earlyvld;
        bs->bs_pessimistic = pessimistic;
        bs->bs_longseqsea = 0;
        bs->bs_nkeyremoved = 0;
        bs->bs_nmergeremoved = 0;
        bs->bs_nreadleafs = 0;
        bs->bs_readaheadsize = 0;
        bs->bs_keypos = DBE_KEYPOS_LAST;
        bs->bs_bonsaip = b->b_bonsaip;
        bs->bs_mergetrxnum = DBE_TRXNUM_NULL;
        bs->bs_lockingread = lockingread;
        bs->bs_unlock_tree = FALSE;
        bs->bs_timeconsacceptall = FALSE;
        if (!mergesea) {
            dbe_info_init(bs->bs_infobuf, 0);
            bs->bs_info = &bs->bs_infobuf;
        }
        ss_debug(bs->bs_freebnode = FALSE;)
        ss_debug(bs->bs_usagectr = 0;)

        dbe_srk_setbkey(bs->bs_srk, dbe_bkrs_getbeginkey(bs->bs_krs));

        /* Find the first node. */
        succp = dbe_bkrs_startnextstep(bs->bs_krs);
        ss_assert(succp);
        if (mergesea) {
            bs->bs_mergerc = btrsea_merge_nextnode(bs);
            if (bs->bs_mergerc == DBE_RC_SUCC) {
                dbe_bnode_rsea_initst(&bs->bs_nrs, bs->bs_n, bs->bs_krs);
            } else {
                dbe_bnode_rsea_initst_error(&bs->bs_nrs);
            }
        } else {
            btrsea_nextorprevnode(bs, BSEA_GETNODE_NEXT);
            dbe_bnode_rsea_initst(&bs->bs_nrs, bs->bs_n, bs->bs_krs);
        }
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_initbufmerge
 *
 * Initializes a range search in the index tree.
 *
 * Parameters :
 *
 *	b - in, hold
 *		index tree
 *
 *	kc - in, hold
 *		key range constraints
 *
 *	tc - in, hold
 *		time range constraints, or NULL if no constraints
 *
 *	mergesea - in
 *		if TRUE, this is a merge search. During merge search aborted
 *		key values are removed and transaction numbers are patched
 *
 * Return value - give :
 *
 *      Btree search pointer.
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_btrsea_initbufmerge(
        dbe_btrsea_t* bs,
        dbe_btree_t* b,
        dbe_btrsea_keycons_t* kc,
        dbe_btrsea_timecons_t* tc,
        dbe_info_t* info)
{
        ss_bprintf_1(("dbe_btrsea_initbuf\n"));
        ss_dassert(kc != NULL);

        bs->bs_info = info;

        dbe_btrsea_initbufvalidate_ex(
                bs,
                b,
                kc,
                tc,
                TRUE,
                FALSE,
                DBE_KEYVLD_NONE,
                FALSE,
                FALSE,
                FALSE);

        ss_dassert(bs->bs_info == info);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_donebuf
 *
 * Releases resources allocated to am index tree search.
 *
 * Parameters :
 *
 *	bs - in, take
 *		pointer to a tree search structure
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_btrsea_donebuf(dbe_btrsea_t* bs)
{
        ss_bprintf_1(("dbe_btrsea_donebuf\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);
        SS_MEMOBJ_DEC(SS_MEMOBJ_BTRSEA);
        ss_dassert(!bs->bs_unlock_tree);

        dbe_bkrs_done(bs->bs_krs);
        dbe_bnode_rsea_donest(&bs->bs_nrs);
        dbe_srk_donebuf(&bs->bs_srk_buf, bs->bs_kc->kc_cd);
        if (bs->bs_mergesea) {
            dbe_srk_donebuf(&bs->bs_peeksrk_buf, bs->bs_kc->kc_cd);
        }
        if (bs->bs_tmpn == NULL) {
            ss_dassert(!bs->bs_mergesea);
            if (bs->bs_n != NULL) {
                dbe_bnode_write(bs->bs_n, bs->bs_longseqsea);
            }
        } else {
            ss_dassert(bs->bs_mergesea);
            ss_dassert(bs->bs_n == bs->bs_tmpn);
            dbe_bnode_done_tmp(bs->bs_tmpn);
        }

        ss_debug(bs->bs_chk = -1;)
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_reset
 *
 * Resets B-tree search. After reset the search is in the same state
 * as after init.
 *
 * Parameters :
 *
 *	bs - in, use
 *		B-tree search object.
 *
 *	kc - in, hold
 *		New key constraints.
 *
 *	tc - in, hold
 *		New time constraints.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_btrsea_reset(
        dbe_btrsea_t* bs,
        dbe_btrsea_keycons_t* kc,
        dbe_btrsea_timecons_t* tc,
        bool lockingread)
{
        bool succp;

        ss_bprintf_1(("dbe_btrsea_reset:id=%ld, lockingread=%d\n", (long)bs, lockingread));
        ss_dassert(kc != NULL);
        ss_dassert(!bs->bs_mergesea);
        ss_dassert(bs->bs_tmpn == NULL);

        if (bs->bs_n != NULL) {
            dbe_bnode_write(bs->bs_n, bs->bs_longseqsea);
            bs->bs_n = NULL;
        }

        bs->bs_pos = BSEA_POS_FIRST;
        dbe_btrsea_resetkeycons(bs, kc);
        bs->bs_tc = tc;
        bs->bs_longseqsea = 0;
        bs->bs_nkeyremoved = 0;
        bs->bs_nmergeremoved = 0;
        bs->bs_nreadleafs = 0;
        bs->bs_readaheadsize = 0;
        bs->bs_keypos = DBE_KEYPOS_LAST;
        bs->bs_lockingread = lockingread;
        ss_dassert(!bs->bs_unlock_tree);

        ss_debug(bs->bs_freebnode = FALSE;)
        ss_debug(bs->bs_usagectr = 0;)

        dbe_srk_setbkey(bs->bs_srk, dbe_bkrs_getbeginkey(bs->bs_krs));

        dbe_bnode_rsea_donest(&bs->bs_nrs);

        /* Find the first node. */
        succp = dbe_bkrs_startnextstep(bs->bs_krs);
        ss_assert(succp);
        btrsea_nextorprevnode(bs, BSEA_GETNODE_NEXT);
        dbe_bnode_rsea_initst(&bs->bs_nrs, bs->bs_n, bs->bs_krs);
}

void dbe_btrsea_resetkeycons(
        dbe_btrsea_t* bs,
        dbe_btrsea_keycons_t* kc)
{
        dbe_bkrs_reset(
            bs->bs_krs,
            kc->kc_beginkey,
            kc->kc_endkey,
            bs->bs_mergesea
                ? (bs->bs_info->i_flags & DBE_INFO_OPENRANGEEND)
                : FALSE);
        bs->bs_kc = kc;
}

void dbe_btrsea_setnodereadonly(
        dbe_btrsea_t* bs)
{
        ss_bprintf_1(("dbe_btrsea_setnodereadonly:id=%ld\n", (long)bs));
        ss_dassert(!bs->bs_mergesea);
        ss_dassert(bs->bs_n != NULL);

        if (dbe_cfg_usenewbtreelocking && bs->bs_lockingread) {
            if (bs->bs_unlock_tree) {
                ss_bprintf_2(("dbe_btrsea_setnodereadonly:unlock tree\n"));
                ss_dassert(dbe_btree_getrootnode_nomutex(bs->bs_b) == NULL || dbe_btree_getrootnode_nomutex(bs->bs_b) == bs->bs_n);
                dbe_btree_unlock(bs->bs_b);
                bs->bs_unlock_tree = FALSE;
            } else {
                ss_bprintf_2(("dbe_btrsea_setnodereadonly:unlock node\n"));
                dbe_bnode_setreadonly(bs->bs_n);
            }
            bs->bs_lockingread = FALSE;
        }
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_getnkeyremoved
 *
 * Returns the number of aborted key values removed during merge search.
 *
 * Parameters :
 *
 *	bs - in
 *		pointer to a tree search structure
 *
 * Return value :
 *
 *      Number of aborted key values removed.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long dbe_btrsea_getnkeyremoved(
        dbe_btrsea_t* bs)
{
        ss_dprintf_1(("dbe_btrsea_getnkeyremoved\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr == 0);
        ss_dassert(bs->bs_mergesea);

        return(bs->bs_nkeyremoved);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_getnmergeremoved
 *
 * Returns the number of aborted merge key values removed during merge
 * search. Merge key count is a calculated value that may be greater
 * than the actual key value count.
 *
 * Parameters :
 *
 *	bs - in
 *		pointer to a tree search structure
 *
 * Return value :
 *
 *      Number of aborted key values removed.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long dbe_btrsea_getnmergeremoved(
        dbe_btrsea_t* bs)
{
        ss_dprintf_1(("dbe_btrsea_getnmergeremoved\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr == 0);
        ss_dassert(bs->bs_mergesea);

        return(bs->bs_nmergeremoved);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_setresetkey
 *
 * Resets a range search in the index tree. The new search position is
 * set to position pointer by k.
 *
 * Parameters :
 *
 *	bs - in out, use
 *		pointer to a tree search structure
 *
 *	k - in
 *		new current position of the search
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_btrsea_setresetkey(
        dbe_btrsea_t* bs,
        dbe_bkey_t* k,
        bool lockingread)
{
        bool succp;

        ss_dprintf_1(("dbe_btrsea_setresetkey:id=%ld, lockingread=%d\n", (long)bs, lockingread));
        CHK_BTRSEA(bs);
        ss_dassert(!bs->bs_mergesea);
        ss_dassert(bs->bs_usagectr++ == 0);

        ss_debug(bs->bs_freebnode = FALSE;)

        if (bs->bs_peeked) {
            ss_dassert(bs->bs_mergesea);
            bs->bs_peeked = FALSE;
        }

        if (k == NULL) {
            k = dbe_srk_getbkey(bs->bs_srk);
        }

        dbe_bnode_rsea_donest(&bs->bs_nrs);

        bs->bs_pos = BSEA_POS_MIDDLE;
        bs->bs_keypos = DBE_KEYPOS_LAST;
        bs->bs_lockingread = lockingread;

        /* Find the first node. */
        dbe_bkrs_setresetkey(bs->bs_krs, k);
        succp = dbe_bkrs_startnextstep(bs->bs_krs);
        ss_assert(succp);
        btrsea_nextorprevnode(bs, BSEA_GETNODE_RESET);

        dbe_bnode_rsea_initst(&bs->bs_nrs, bs->bs_n, bs->bs_krs);

        ss_debug(bs->bs_usagectr--);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_freebnode
 *
 * Frees current bnode from the search. This is used to release the
 * currently reserved cache block to other use without closing the
 * search.
 *
 * This function can be used only when later the client code calls
 * dbe_btrsea_setresetkey, otherwise the search state and structure content
 * is not correct and system will fail.
 *
 * Parameters :
 *
 *	bs - use
 *		pointer to a tree search structure
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_btrsea_freebnode(
        dbe_btrsea_t* bs)
{
        ss_bprintf_1(("dbe_btrsea_freebnode\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);
        ss_dassert(!bs->bs_mergesea);

        if (bs->bs_n != NULL) {
            /* Release bnode.
             */
            ss_dassert(!bs->bs_freebnode);
            ss_debug(bs->bs_freebnode = TRUE;)

            dbe_bnode_write(bs->bs_n, bs->bs_longseqsea);

            bs->bs_n = NULL;
        }
        ss_debug(bs->bs_usagectr--);
}

/*#***********************************************************************\
 *
 *		btrsea_checkcons
 *
 * Checks time and key constraints of a key value.
 *
 * Parameters :
 *
 *	bs - in
 *
 *
 * Return value :
 *
 *      DBE_RC_FOUND
 *      DBE_RC_NOTFOUND
 *      DBE_RC_LOCKTUPLE
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t btrsea_checkcons(
        dbe_btrsea_t* bs)
{
        dbe_trxstate_t trxresult;
        bool succp;

        ss_bprintf_3(("btrsea_checkcons\n"));
#if defined(SS_DEBUG) || defined(SS_BETA)
        if (!(dbe_bkey_compare(dbe_bkrs_getbeginkey(bs->bs_krs), dbe_srk_getbkey(bs->bs_srk)) <= 0)) {
            bsea_errorprint(bs, TRUE);
            ss_error;
        }
        if (!(dbe_bkey_compare(dbe_srk_getbkey(bs->bs_srk), dbe_bkrs_getendkey(bs->bs_krs)) <= 0)) {
            bsea_errorprint(bs, TRUE);
            ss_error;
        }
#endif /* defined(SS_DEBUG) || defined(SS_BETA) */
        SU_GENERIC_TIMER_START(SU_GENERIC_TIMER_DBE_CHECKCONS);

        if (bs->bs_pessimistic) {
            succp = dbe_btrsea_checktimecons(
                        bs, 
                        bs->bs_tc, 
                        dbe_srk_getbkey(bs->bs_srk),
                        bs->bs_bonsaip,
                        bs->bs_validatesea,
                        &trxresult);
            if (succp) {
                ss_dassert(trxresult != DBE_TRXST_ABORT)
                succp = dbe_btrsea_checkkeycons(
                            bs->bs_kc->kc_cd,
                            bs->bs_srk,
                            bs->bs_kc->kc_conslist);
            } else if (trxresult == DBE_TRXST_BEGIN ||
#ifdef DBE_REPLICATION
                       trxresult == DBE_TRXST_TOBEABORTED ||
#endif /* DBE_REPLICATION */
                       trxresult == DBE_TRXST_VALIDATE) {
                /* Special case, in pessimistic search uncertain key
                 * values (with state DBE_TRXST_BEGIN or DBE_TRXST_VALIDATE)
                 * return lock request on that tuple.
                 */
                if (dbe_btrsea_checkkeycons(
                        bs->bs_kc->kc_cd,
                        bs->bs_srk,
                        bs->bs_kc->kc_conslist)) {
                    /* Inside search range, this key must be locked if not
                     * from the current statement.
                     */
                    dbe_user_t* user;
                    dbe_trx_t* trx;
                    user = rs_sysi_user(bs->bs_kc->kc_cd);
                    trx = dbe_user_gettrx(user);
                    ss_dassert(trx != NULL);
                    ss_dprintf_4(("btrsea_checkcons:trx stmttrxid = %ld, key trxid = %ld\n", DBE_TRXID_GETLONG(dbe_trx_getstmttrxid(trx)),
                                    DBE_TRXID_GETLONG(dbe_srk_getkeytrxid(bs->bs_srk))));
                    if (trx != NULL
                        && DBE_TRXID_EQUAL(dbe_trx_getstmttrxid(trx),
                                         dbe_srk_getkeytrxid(bs->bs_srk)))
                    {
#ifdef DBE_REPLICATION
                        ss_dassert(trxresult == DBE_TRXST_BEGIN
                            ||     trxresult == DBE_TRXST_TOBEABORTED);
#else /* DBE_REPLICATION */
                        ss_dassert(trxresult == DBE_TRXST_BEGIN);
#endif /* DBE_REPLICATION */
                        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_CHECKCONS);
                        return(DBE_RC_NOTFOUND);
                    } else {
                        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_CHECKCONS);
                        return(DBE_RC_LOCKTUPLE);
                    }
                } else {
                    /* Not inside search range, ignore this key.
                     */
                    SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_CHECKCONS);
                    return(DBE_RC_NOTFOUND);
                }
            }
        } else {
            succp = dbe_btrsea_checktimecons(
                        bs, 
                        bs->bs_tc, 
                        dbe_srk_getbkey(bs->bs_srk),
                        bs->bs_bonsaip,
                        bs->bs_validatesea,
                        &trxresult) 
                    &&
                    dbe_btrsea_checkkeycons(
                        bs->bs_kc->kc_cd,
                        bs->bs_srk,
                        bs->bs_kc->kc_conslist);
        }
        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_CHECKCONS);
        if (succp) {
            ss_aassert(trxresult != DBE_TRXST_ABORT)
            return(DBE_RC_FOUND);
        } else {
            return(DBE_RC_NOTFOUND);
        }
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_getnext
 *
 * Returns the next key in the search.
 *
 * Return codes and values of output parameters:
 *
 *      rc               *p_srk         dbe_srk_getbkey(*p_srk)
 *      ---------------  -----------    -----------------------
 *      DBE_RC_FOUND     current srk    current key
 *
 *      DBE_RC_NOTFOUND  current srk    current key
 *
 *      DBE_RC_END       NULL           error
 *
 * Parameters :
 *
 *	bs - in out, use
 *		pointer to a tree search structure
 *
 *      p_srk - out, ref
 *          Search result.
 *
 * Return value :
 *
 *      DBE_RC_FOUND     - next key found
 *      DBE_RC_NOTFOUND  - found key did not match search constraints
 *      DBE_RC_END       - end of search
 *      DBE_RC_LOCKTUPLE - lock the current tuple
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_btrsea_getnext(
        dbe_btrsea_t* bs,
        dbe_srk_t** p_srk)
{
        int nloop;
        dbe_ret_t rc;
        dbe_ret_t cons_rc;
        su_profile_timer;

        ss_bprintf_1(("dbe_btrsea_getnext:id=%ld, mergesea=%d, bonsaip=%d\n", (long)bs, bs->bs_mergesea, bs->bs_bonsaip));
        SS_PUSHNAME("dbe_btrsea_getnext");

        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);
        ss_dassert(p_srk != NULL);
        ss_dassert(!bs->bs_freebnode);
        ss_dassert(!bs->bs_pessimistic || !dbe_cfg_usepessimisticgate || SsSemStkFind(SS_SEMNUM_DBE_PESSGATE));
        SU_GENERIC_TIMER_START(SU_GENERIC_TIMER_DBE_BTREEFETCH);
        su_profile_start;

        if (bs->bs_peeked) {
            dbe_srk_t* tmp_srk;

            ss_dassert(bs->bs_mergesea);
            bs->bs_peeked = FALSE;
            /* Swap bs_srk and bs_peeksrk.
             */
            tmp_srk = bs->bs_srk;
            bs->bs_srk = bs->bs_peeksrk;
            bs->bs_peeksrk = tmp_srk;
            *p_srk = bs->bs_srk;
            ss_debug(bs->bs_usagectr--);
            ss_bprintf_2(("dbe_btrsea_getnext:bs_peeked, rc=%s\n", su_rc_nameof(bs->bs_peekrc)));
            SS_POPNAME;
            SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
            su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
            return(bs->bs_peekrc);
        }

        if (bs->bs_pos == BSEA_POS_LAST) {
            *p_srk = NULL;
            ss_debug(bs->bs_usagectr--);
            ss_bprintf_2(("dbe_btrsea_getnext:DBE_RC_END\n"));
            SS_POPNAME;
            SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
            su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
            return(DBE_RC_END);
        }

        if (bs->bs_mergesea) {
            if (bs->bs_mergerc != DBE_RC_SUCC) {
                ss_debug(bs->bs_usagectr--);
                ss_bprintf_2(("dbe_btrsea_getnext:bs->bs_mergerc=%s\n", su_rc_nameof(bs->bs_mergerc)));
                SS_POPNAME;
                SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
                su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
                return(bs->bs_mergerc);
            }
        }

        if (dbe_search_noindexassert) {
            ss_rc_dassert(ss_dbg_jmpbufpos < SS_DBG_JMPBUF_MAX, ss_dbg_jmpbufpos);
            ss_dbg_jmpbufpos++;
            if (setjmp(ss_dbg_jmpbuf[ss_dbg_jmpbufpos-1]) != 0) {
                /* Error in search. */
                dbe_bnode_rsea_skipleaf(&bs->bs_nrs);
                ss_dbg_jmpbufpos--;
                ss_debug(bs->bs_usagectr--);
                SS_POPNAME;
                SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
                su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
                return(DBE_ERR_ASSERT);
            }
        }

        for (nloop = 0; ; nloop++) {
            rc = dbe_bnode_rsea_next(&bs->bs_nrs, bs->bs_srk);
            switch (rc) {
                case DBE_RC_END:
                    /* End of search.
                     */
                    ss_bprintf_2(("dbe_btrsea_getnext:DBE_RC_END\n"));
                    *p_srk = NULL;
                    bs->bs_pos = BSEA_POS_LAST;
                    if (dbe_search_noindexassert) {
                        ss_dbg_jmpbufpos--;
                    }
                    ss_debug(bs->bs_usagectr--);
                    SS_POPNAME;
                    SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
                    su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
                    return(DBE_RC_END);

                case DBE_RC_FOUND:
                    /* Next key found.
                     */
                    *p_srk = bs->bs_srk;
                    bs->bs_pos = BSEA_POS_MIDDLE;

                    dbe_bnode_rsea_getkeypos(&bs->bs_nrs, &bs->bs_keypos);
                    dbe_srk_setkeypos(bs->bs_srk, bs->bs_keypos);

                    ss_output_2(dbe_bkey_dprint_ex(2, "dbe_btrsea_getnext:Key:", dbe_srk_getbkey(bs->bs_srk)));

                    cons_rc = btrsea_checkcons(bs);

                    switch (cons_rc) {

                        case DBE_RC_FOUND:
                        case DBE_RC_LOCKTUPLE:
                            ss_bprintf_2(("dbe_btrsea_getnext:%s (%d)\n", su_rc_nameof(cons_rc), cons_rc));
                            if (dbe_search_noindexassert) {
                                ss_dbg_jmpbufpos--;
                            }
                            ss_debug(bs->bs_usagectr--);
                            SS_POPNAME;
                            SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
                            su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
                            return(cons_rc);

                        case DBE_RC_NOTFOUND:
#if 1 /* Jarmo changed, Jun 1, 1996
         Maybe this is because otherwise we cannot guarantee the key state
         (delete from bonsai or permanent tree). */
                            if (bs->bs_mergesea) {
                                ss_bprintf_2(("dbe_btrsea_getnext:merge search, skip key, nloop = %d\n", nloop));
                                break;
                            }
#endif
                            if (bs->bs_keypos == DBE_KEYPOS_MIDDLE
                                && nloop < DBE_MAXLOOP
                                && !bs->bs_mergesea) {
                                /* Key not found and current key is middle key
                                 * on the leaf. Continue to the next key value.
                                 */
                                ss_bprintf_2(("dbe_btrsea_getnext:skip key, nloop = %d\n", nloop));
                                break;
                            } else {
                                ss_bprintf_2(("dbe_btrsea_getnext:DBE_RC_NOTFOUND\n"));
                                if (dbe_search_noindexassert) {
                                    ss_dbg_jmpbufpos--;
                                }
                                ss_debug(bs->bs_usagectr--);
                                SS_POPNAME;
                                SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
                                su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
                                return(DBE_RC_NOTFOUND);
                            }
                        default:
                            su_rc_error(cons_rc);
                    }
                    break;

                case DBE_RC_NOTFOUND:
                    /* End of node. End the node range search.
                     */
                    ss_bprintf_2(("dbe_btrsea_getnext:find next node\n"));
                    ss_aassert(bs->bs_keypos != DBE_KEYPOS_MIDDLE);
                    dbe_bnode_rsea_donest(&bs->bs_nrs);
                    if (!dbe_bkrs_startnextstep(bs->bs_krs)) {
                        /* No more nodes. End of search.
                         */
                        ss_bprintf_2(("dbe_btrsea_getnext:DBE_RC_END\n"));
                        *p_srk = NULL;
                        bs->bs_pos = BSEA_POS_LAST;
                        if (dbe_search_noindexassert) {
                            ss_dbg_jmpbufpos--;
                        }
                        ss_debug(bs->bs_usagectr--);
                        SS_POPNAME;
                        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
                        su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
                        return(DBE_RC_END);
                    }
                    /* Find the next node.
                     */
                    if (bs->bs_mergesea) {
                        rc = btrsea_merge_nextnode(bs);
                        if (rc != DBE_RC_SUCC) {
                            ss_debug(bs->bs_usagectr--);
                            SS_POPNAME;
                            SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_BTREEFETCH);
                            su_profile_stop(bs->bs_mergesea ? "dbe_btrsea_getnext:merge" : "dbe_btrsea_getnext");
                            return(rc);
                        }
                    } else {
                        btrsea_nextorprevnode(bs, BSEA_GETNODE_NEXT);
                    }
                    /* Start a new node range search.
                     */
                    dbe_bnode_rsea_initst(&bs->bs_nrs, bs->bs_n, bs->bs_krs);
                    bs->bs_pos = BSEA_POS_MIDDLE;
                    bs->bs_nreadleafs++;
                    if (!bs->bs_longseqsea &&
                        bs->bs_nreadleafs > LONGSEQSEA_NREADLEAFS) {
                        bs->bs_longseqsea = 1;
                    }
                    break;

                default:
                    su_rc_error(rc);
            }
        }
        ss_error;
        return(DBE_RC_END);
}

int dbe_btrsea_getequalrowestimate(
        rs_sysi_t* cd,
        dbe_btree_t* b,
        vtpl_t* range_begin,
        vtpl_t* range_end)
{
        ss_debug(int chk1 = 11;)
        dbe_btrsea_t bs;
        dbe_btrsea_keycons_t kc;
        dbe_btrsea_timecons_t tc;
        dbe_srk_t* srk;
        dbe_ret_t rc;
        int estimate;
        int maxrows;
        dbe_gobj_t* go;
        ss_debug(int chk2 = 22;)

        ss_dprintf_1(("dbe_btrsea_getequalrowestimate\n"));

        maxrows = rs_sqli_getestsamplemaxeqrowest(rs_sysi_sqlinfo(cd));

        if (maxrows <= 0) {
            ss_dprintf_1(("dbe_btrsea_getequalrowestimate:maxrows=%d, no estimate\n", maxrows));
            ss_dassert(chk1 == 11);
            ss_dassert(chk2 == 22);
            return(0);
        }


        kc.kc_beginkey = NULL;
        kc.kc_endkey = NULL;
        kc.kc_conslist = NULL;
        kc.kc_cd = cd;
        kc.kc_key = NULL;

        dbe_dynbkey_setleaf(
            &kc.kc_beginkey,
            DBE_TRXNUM_NULL,
            DBE_TRXID_NULL,
            range_begin);

        dbe_dynbkey_setleaf(
            &kc.kc_endkey,
            DBE_TRXNUM_NULL,
            DBE_TRXID_MAX,
            range_end);

        go = dbe_btree_getgobj(b);

        tc.tc_mintrxnum = DBE_TRXNUM_NULL;
        tc.tc_maxtrxnum = DBE_TRXNUM_NULL;
        tc.tc_usertrxid = DBE_TRXID_NULL;
        tc.tc_maxtrxid = DBE_TRXID_NULL;
        tc.tc_trxbuf = go->go_trxbuf;

        dbe_btrsea_initbufvalidate(
            &bs,
            b,
            &kc,
            &tc,
            FALSE,
            FALSE,
            DBE_KEYVLD_NONE,
            TRUE);

        dbe_btrsea_settimeconsacceptall(&bs);

        for (estimate = 0; 
             estimate < maxrows && (rc = dbe_btrsea_getnext(&bs, &srk)) == DBE_RC_FOUND; 
             )
        {
            estimate++;
        }

        if (rc == DBE_RC_FOUND) {
            ss_dprintf_2(("dbe_btrsea_getequalrowestimate:found more than maxrows %d\n", maxrows));
            estimate = 0;
        } else if (estimate == 0) {
            /* If we have found something do not return zero because
             * it means unknown value.
             */
            estimate = 1;
        }

        dbe_btrsea_donebuf(&bs);

        dbe_dynbkey_free(&kc.kc_beginkey);
        dbe_dynbkey_free(&kc.kc_endkey);

        ss_dprintf_2(("dbe_btrsea_getequalrowestimate:estimate=%d\n", estimate));

        ss_dassert(chk1 == 11);
        ss_dassert(chk2 == 22);

        return(estimate);
}

#ifdef SS_QUICKSEARCH

dbe_ret_t dbe_btrsea_getnextblock(
        dbe_btrsea_t* bs)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_btrsea_getnextblock\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);
        ss_dassert(!bs->bs_freebnode);
        ss_dassert(!bs->bs_mergesea);
        ss_dassert(!bs->bs_peeked);

        if (bs->bs_pos == BSEA_POS_LAST) {
            ss_debug(bs->bs_usagectr--);
            ss_dprintf_2(("dbe_btrsea_getnextblock:DBE_RC_END at %d\n", __LINE__));
            return(DBE_RC_END);
        }

        rc = dbe_bnode_rsea_next(&bs->bs_nrs, bs->bs_srk);
        if (rc == DBE_RC_END) {
            ss_dprintf_2(("dbe_btrsea_getnextblock:DBE_RC_END at %d\n", __LINE__));
            bs->bs_pos = BSEA_POS_LAST;
            ss_debug(bs->bs_usagectr--);
            return(DBE_RC_END);
        }

        ss_dprintf_2(("dbe_btrsea_getnextblock:find next node\n"));
        dbe_bnode_rsea_donest(&bs->bs_nrs);
        if (!dbe_bkrs_startnextstep(bs->bs_krs)) {
            /* No more nodes. End of search.
             */
            ss_dprintf_2(("dbe_btrsea_getnextblock:DBE_RC_END at %d\n", __LINE__));
            bs->bs_pos = BSEA_POS_LAST;
            ss_debug(bs->bs_usagectr--);
            return(DBE_RC_END);
        }

        /* Find the next node.
         */
        btrsea_nextorprevnode(bs, BSEA_GETNODE_NEXT, NULL);

        /* Start a new node range search.
         */
        dbe_bnode_rsea_initst(&bs->bs_nrs, bs->bs_n, bs->bs_krs);
        bs->bs_pos = BSEA_POS_MIDDLE;

        ss_debug(bs->bs_usagectr--);

        return(DBE_RC_FOUND);
}

void dbe_btrsea_getnodedata(
        dbe_btrsea_t* bs,
        char** p_data,
        int* p_len)
{
        CHK_BTRSEA(bs);

        if (bs->bs_n == NULL) {
            *p_data = "";
            *p_len = 0;
        } else {
            dbe_bnode_getdata(bs->bs_n, p_data, p_len);
        }
}
#endif /* SS_QUICKSEARCH */

/*##**********************************************************************\
 *
 *		dbe_btrsea_getnext_quickmerge
 *
 *
 *
 * Parameters :
 *
 *	bs -
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
dbe_ret_t dbe_btrsea_getnext_quickmerge(
        dbe_btrsea_t* bs)
{
        dbe_ret_t rc;

        ss_bprintf_1(("dbe_btrsea_getnext_quickmerge\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);
        ss_dassert(!bs->bs_freebnode);
        ss_dassert(bs->bs_mergesea);
        ss_dassert(!bs->bs_peeked);

        if (bs->bs_mergerc != DBE_RC_SUCC) {
            ss_debug(bs->bs_usagectr--);
            return(bs->bs_mergerc);
        }

        if (bs->bs_pos == BSEA_POS_LAST) {
            ss_debug(bs->bs_usagectr--);
            ss_bprintf_2(("dbe_btrsea_getnext_quickmerge:DBE_RC_END at %d\n", __LINE__));
            return(DBE_RC_END);
        }

        rc = dbe_bnode_rsea_next(&bs->bs_nrs, bs->bs_srk);
        if (rc == DBE_RC_END) {
            ss_bprintf_2(("dbe_btrsea_getnext_quickmerge:DBE_RC_END at %d\n", __LINE__));
            bs->bs_pos = BSEA_POS_LAST;
            ss_debug(bs->bs_usagectr--);
            return(DBE_RC_END);
        }

        ss_bprintf_2(("dbe_btrsea_getnext_quickmerge:find next node\n"));
        dbe_bnode_rsea_donest(&bs->bs_nrs);
        if (!dbe_bkrs_startnextstep(bs->bs_krs)) {
            /* No more nodes. End of search.
             */
            ss_bprintf_2(("dbe_btrsea_getnext_quickmerge:DBE_RC_END at %d\n", __LINE__));
            bs->bs_pos = BSEA_POS_LAST;
            ss_debug(bs->bs_usagectr--);
            return(DBE_RC_END);
        }

        /* Find the next node.
         */
        rc = btrsea_merge_nextnode(bs);
        if (rc != DBE_RC_SUCC) {
            ss_debug(bs->bs_usagectr--);
            return(rc);
        }

        /* Start a new node range search.
         */
        dbe_bnode_rsea_initst(&bs->bs_nrs, bs->bs_n, bs->bs_krs);
        bs->bs_pos = BSEA_POS_MIDDLE;

        ss_debug(bs->bs_usagectr--);

        return(DBE_RC_FOUND);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_peeknext
 *
 * Returns the next key in the search but does not advance the search.
 * Allowed only during merge search.
 *
 * Parameters :
 *
 *	bs - in out, use
 *		pointer to a tree search structure
 *
 *      p_srk - out, ref
 *          Search result.
 *
 * Return value :
 *
 *      DBE_RC_FOUND    - next key found
 *      DBE_RC_NOTFOUND - found key did not match search constraints
 *      DBE_RC_END      - end of search
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_btrsea_peeknext(
        dbe_btrsea_t* bs,
        dbe_srk_t** p_srk)
{
        ss_bprintf_1(("dbe_btrsea_peeknext\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);
        ss_dassert(bs->bs_mergesea);
        ss_dassert(!bs->bs_freebnode);

        if (!bs->bs_peeked) {
            dbe_srk_t* tmp_srk;
            dbe_srk_t* dummy_srk;

            /* Get the next key into bs_peeksrk by setting bs_peeksrk
             * temporarily to bs_srk. The current bs_srk is not changed.
             */
            tmp_srk = bs->bs_srk;
            dbe_srk_copy(bs->bs_peeksrk, bs->bs_srk);
            bs->bs_srk = bs->bs_peeksrk;

            ss_debug(bs->bs_usagectr--);

            bs->bs_peekrc = dbe_btrsea_getnext(bs, &dummy_srk);

            ss_dassert(bs->bs_usagectr++ == 0);

            bs->bs_peeked = TRUE;
            bs->bs_srk = tmp_srk;
        }

        *p_srk = bs->bs_peeksrk;

        ss_debug(bs->bs_usagectr--);

        return(bs->bs_peekrc);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_getprev
 *
 * Returns the previous key in the search.
 *
 * Return codes and values of output parameters:
 *
 *      rc               *p_srk         dbe_srk_getbkey(*p_srk)
 *      ---------------  -----------    -----------------------
 *      DBE_RC_FOUND     current srk    current key
 *
 *      DBE_RC_NOTFOUND  current srk    current key
 *
 *      DBE_RC_END       NULL           error
 *
 * Parameters :
 *
 *	bs - in out, use
 *		pointer to a tree search structure
 *
 *      p_srk - out, ref
 *          Search result.
 *
 * Return value :
 *
 *      DBE_RC_FOUND     - previous key found
 *      DBE_RC_NOTFOUND  - found key did not match search constraints
 *      DBE_RC_END       - end of search
 *      DBE_RC_LOCKTUPLE - lock the current tuple
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_btrsea_getprev(
        dbe_btrsea_t* bs,
        dbe_srk_t** p_srk)
{
        int nloop;
        dbe_ret_t rc;
        dbe_ret_t cons_rc;
        su_profile_timer;

        SS_PUSHNAME("dbe_btrsea_getprev");
        ss_bprintf_1(("dbe_btrsea_getprev\n"));
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);
        ss_dassert(p_srk != NULL);
        ss_dassert(!bs->bs_mergesea);
        ss_dassert(!bs->bs_validatesea);
        ss_dassert(!bs->bs_peeked);
        ss_dassert(!bs->bs_freebnode);
        ss_dassert(!bs->bs_pessimistic || !dbe_cfg_usepessimisticgate || SsSemStkFind(SS_SEMNUM_DBE_PESSGATE));

        su_profile_start;

        if (bs->bs_pos == BSEA_POS_FIRST) {
            *p_srk = NULL;
            ss_debug(bs->bs_usagectr--);
            SS_POPNAME;
            su_profile_stop("dbe_btrsea_getprev");
            return(DBE_RC_END);
        }

        for (nloop = 0; ; nloop++) {
            rc = dbe_bnode_rsea_prev(&bs->bs_nrs, bs->bs_srk);
            switch (rc) {
                case DBE_RC_END:
                    /* End of search.
                     */
                    *p_srk = NULL;
                    bs->bs_pos = BSEA_POS_FIRST;
                    ss_debug(bs->bs_usagectr--);
                    SS_POPNAME;
                    su_profile_stop("dbe_btrsea_getprev");
                    return(DBE_RC_END);

                case DBE_RC_FOUND:
                    /* Previous key found.
                     */
                    *p_srk = bs->bs_srk;
                    bs->bs_pos = BSEA_POS_MIDDLE;

                    dbe_bnode_rsea_getkeypos(&bs->bs_nrs, &bs->bs_keypos);
                    dbe_srk_setkeypos(bs->bs_srk, bs->bs_keypos);

                    ss_output_2(dbe_bkey_dprint_ex(2, "dbe_btrsea_getprev:Key:", dbe_srk_getbkey(bs->bs_srk)));

                    cons_rc = btrsea_checkcons(bs);

                    switch (cons_rc) {

                        case DBE_RC_FOUND:
                        case DBE_RC_LOCKTUPLE:
                            ss_bprintf_2(("dbe_btrsea_getprev:%s (%d)\n", su_rc_nameof(cons_rc), cons_rc));
                            ss_debug(bs->bs_usagectr--);
                            SS_POPNAME;
                            su_profile_stop("dbe_btrsea_getprev");
                            return(cons_rc);

                        case DBE_RC_NOTFOUND:
                            if (bs->bs_keypos == DBE_KEYPOS_MIDDLE
                                && nloop < DBE_MAXLOOP) {
                                /* Key not found and current key is middle key
                                 * on the leaf. Continue to the next key value.
                                 */
                                ss_bprintf_2(("dbe_btrsea_getprev:skip key, nloop = %d\n", nloop));
                                break;
                            } else {
                                ss_bprintf_2(("dbe_btrsea_getprev:DBE_RC_NOTFOUND\n"));
                                ss_debug(bs->bs_usagectr--);
                                SS_POPNAME;
                                su_profile_stop("dbe_btrsea_getprev");
                                return(DBE_RC_NOTFOUND);
                            }
                        default:
                            su_rc_error(cons_rc);
                    }
                    break;

                case DBE_RC_NOTFOUND:
                    /* End of node. End the node range search.
                     */
                    ss_dassert(bs->bs_keypos != DBE_KEYPOS_MIDDLE);
                    dbe_bnode_rsea_donest(&bs->bs_nrs);
                    if (!dbe_bkrs_startprevstep(bs->bs_krs)) {
                        /* No more nodes. End of search.
                         */
                        *p_srk = NULL;
                        bs->bs_pos = BSEA_POS_FIRST;
                        ss_debug(bs->bs_usagectr--);
                        SS_POPNAME;
                        su_profile_stop("dbe_btrsea_getprev");
                        return(DBE_RC_END);
                    }
                    /* Find the previous node.
                     */
                    btrsea_nextorprevnode(bs, BSEA_GETNODE_PREV);
                    dbe_bkrs_fixprevstep(bs->bs_krs);

                    /* Start a new node range search.
                     */
                    dbe_bnode_rsea_initst(&bs->bs_nrs, bs->bs_n, bs->bs_krs);
                    bs->bs_pos = BSEA_POS_MIDDLE;
                    bs->bs_nreadleafs++;
                    if (!bs->bs_longseqsea &&
                        bs->bs_nreadleafs > LONGSEQSEA_NREADLEAFS) {
                        bs->bs_longseqsea = 1;
                    }
                    break;

                default:
                    su_rc_error(rc);
            }
        }
        ss_error;
        return(DBE_RC_END);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_getcurrange_prev
 *
 * Returns the range begin and end keys of the current search buffer
 * range.
 *
 * Parameters :
 *
 *	bs - in, use
 *		btree search
 *
 *	p_begin - out, ref
 *		if non-NULL, pointer to the local buffer of range begin key
 *		is stored into *p_begin
 *
 *	p_end - out, ref
 *		if non-NULL, pointer to the local buffer of range end key
 *		is stored into *p_end
 *
 * Return value :
 *
 *      TRUE    - not end of search
 *      FALSE   - end of search
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_btrsea_getcurrange_prev(
        dbe_btrsea_t* bs,
        dbe_bkey_t** p_begin,
        dbe_bkey_t** p_end)
{
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);

        if (bs->bs_pos == BSEA_POS_FIRST) {
            ss_debug(bs->bs_usagectr--);
            return(FALSE);
        }

        if (p_begin != NULL) {
            *p_begin = dbe_bkrs_getbeginkey(bs->bs_krs);
        }
        if (p_end != NULL) {
            *p_end = dbe_bkrs_getendkey(bs->bs_krs);
        }
        ss_debug(bs->bs_usagectr--);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_isbegin
 *
 * Checks if the btree search is in the begin state.
 *
 * Parameters :
 *
 *	bs - in, use
 *		btree search
 *
 * Return value :
 *
 *      TRUE    - the search is at the begin state
 *      FALSE   - the search is not at the begin state
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_btrsea_isbegin(dbe_btrsea_t* bs)
{
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr == 0);

        return(bs->bs_pos == BSEA_POS_FIRST);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_isend
 *
 * Checks if the btree search is in the end state.
 *
 * Parameters :
 *
 *	bs - in, use
 *		btree search
 *
 * Return value :
 *
 *      TRUE    - the search is at the end state
 *      FALSE   - the search is not at the end state
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_btrsea_isend(dbe_btrsea_t* bs)
{
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr == 0);

        return(bs->bs_pos == BSEA_POS_LAST);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_setlongseqsea
 *
 *
 *
 * Parameters :
 *
 *	bs -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_btrsea_setlongseqsea(dbe_btrsea_t* bs)
{
        CHK_BTRSEA(bs);

        if (!bs->bs_longseqsea) {
            bs->bs_longseqsea = 1;
        }
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_clearlongseqsea
 *
 *
 *
 * Parameters :
 *
 *	bs -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_btrsea_clearlongseqsea(dbe_btrsea_t* bs)
{
        CHK_BTRSEA(bs);

        bs->bs_longseqsea = 0;
        bs->bs_nreadleafs = 0;
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_setreadaheadsize
 *
 *
 *
 * Parameters :
 *
 *	bs -
 *
 *
 *	readaheadsize -
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
void dbe_btrsea_setreadaheadsize(
        dbe_btrsea_t* bs,
        uint readaheadsize)
{
        CHK_BTRSEA(bs);

        bs->bs_readaheadsize = readaheadsize;
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_errorprint
 *
 *
 *
 * Parameters :
 *
 *	bs -
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
void dbe_btrsea_errorprint(dbe_btrsea_t* bs)
{
        bsea_errorprint(bs, FALSE);
}
