/*************************************************************************\
**  source       * dbe0seq.c
**  directory    * dbe
**  description  * Routines for fast sequence handling.
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

This module implements a low level interface to high-speed sequence
objects. The sequences are kept in main memory to guaranteed fast
access.

Two different kind of sequences are recognized:

        Dense sequences

            Dense sequences guarantee that there are no holes in the
            sequence. The sequence operations are bound to the current
            transaction: if transaction commits then sequence changes also
            commit, if transaction rolls back then also sequence operations
            are rolled back. Every sequence operation is handled as an
            SQL statement by itself, it does not belong to the previous
            or next SQL statement.

            Sparse sequences use dbe lock manager to enforse serial
            access to sequences.

        Sparse sequences

            Sparse sequences guarantee only uniqueness, some values may
            be left unused in the sequences. Sparse sequences are faster
            than dense sequences, because they are not bounded to a
            trasnaction. Sparse sequence operations can be shared, or
            mixed, with several concurrently executing transactions.


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
#include <ssmem.h>
#include <sssem.h>

#ifndef SS_NOSEQUENCE

#include <su0rbtr.h>
#include <rs0tnum.h>

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe8seql.h"
#include "dbe8flst.h"
#include "dbe6lmgr.h"
#include "dbe0type.h"
#include "dbe0trx.h"
#include "dbe0db.h"
#include "dbe0user.h"
#include "dbe1seq.h"
#include "dbe0seq.h"

#define CHK_SEQ(seq)    ss_dassert(SS_CHKPTR(seq) && (seq)->seq_chk == DBE_CHK_SEQ)
#define CHK_SEQVAL(sv)  ss_dassert(SS_CHKPTR(sv) && (sv)->sv_chk == DBE_CHK_SEQVAL)

typedef enum {
        DBE_SEQOPER_CURRENT,
        DBE_SEQOPER_NEXT,
        DBE_SEQOPER_SET
} dbe_seqoper_t;

/* Structure for the sequnce value.
 */
struct dbe_seqvalue_st {
        ss_debug(int    sv_chk;)
        long            sv_id;              /* Seq id. */
        rs_tuplenum_t   sv_value;           /* Current value. */
        rs_tuplenum_t   sv_rollbackvalue;   /* Rollback value. Used in dense
                                               sequences to save original
                                               value before any changes. */
        bool            sv_isrollbackvalue; /* If TRUE, sv_rollbackvalue
                                               contains the rollback value. */
        int             sv_dropcount;
        int             sv_nlink;
        SsSemT*         sv_unlinksem;
};

/* Sequence object.
 */
struct dbe_seq_st {
        ss_debug(int seq_chk;)
        su_rbt_t*    seq_rbt;    /* Search rb-tree, contains dbe_seqvalue_t's
                                   ordered by sv_id. */
        SsSemT*      seq_sem;    /* Semaphore for concurrent access. */
};

/*##**********************************************************************\
 *
 *		dbe_seqvalue_getid
 *
 * Returns sequence id from sequence value.
 *
 * Parameters :
 *
 *	sv -
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
long dbe_seqvalue_getid(
        dbe_seqvalue_t* sv)
{
        CHK_SEQVAL(sv);

        return(sv->sv_id);
}

static void seqvalue_link(dbe_seqvalue_t* sv)
{
        CHK_SEQVAL(sv);
        ss_dassert(SsSemIsEntered(sv->sv_unlinksem));

        sv->sv_nlink++;
}

static void seqvalue_unlink(dbe_seqvalue_t* sv, bool entersemp)
{
        CHK_SEQVAL(sv);
        ss_dassert(sv->sv_nlink > 0);

        if (entersemp) {
            SsSemEnter(sv->sv_unlinksem);
        }
        ss_dassert(SsSemIsEntered(sv->sv_unlinksem));

        sv->sv_nlink--;
        if (sv->sv_nlink == 0) {
            if (entersemp) {
                SsSemExit(sv->sv_unlinksem);
            }
            SsMemFree(sv);
        } else {
            if (entersemp) {
                SsSemExit(sv->sv_unlinksem);
            }
        }
}

/*#***********************************************************************\
 *
 *		seq_insert_compare
 *
 *
 *
 * Parameters :
 *
 *	key1 -
 *
 *
 *	key2 -
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
static int seq_insert_compare(void* key1, void* key2)
{
        dbe_seqvalue_t* sv1 = key1;
        dbe_seqvalue_t* sv2 = key2;

        CHK_SEQVAL(sv1);
        CHK_SEQVAL(sv2);

        return(su_rbt_long_compare(sv1->sv_id, sv2->sv_id));
}

/*#***********************************************************************\
 *
 *		seq_search_compare
 *
 *
 *
 * Parameters :
 *
 *	search_key -
 *
 *
 *	rbt_key -
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
static int seq_search_compare(void* search_key, void* rbt_key)
{
        dbe_seqvalue_t* sv = rbt_key;

        CHK_SEQVAL(sv);

        return(su_rbt_long_compare((long)search_key, sv->sv_id));
}

/*#***********************************************************************\
 *
 *		seq_delete
 *
 *
 *
 * Parameters :
 *
 *	key -
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
static void seq_delete(void* key)
{
        seqvalue_unlink(key, FALSE);
}

/*##**********************************************************************\
 *
 *		dbe_seq_init
 *
 * Initializes a sequence object.
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_seq_t* dbe_seq_init(
        void)
{
        dbe_seq_t* seq;

        ss_dprintf_1(("dbe_seq_init\n"));

        seq = SSMEM_NEW(dbe_seq_t);

        ss_debug(seq->seq_chk = DBE_CHK_SEQ);
        seq->seq_rbt = su_rbt_inittwocmp(
                            seq_insert_compare,
                            seq_search_compare,
                            seq_delete);
        seq->seq_sem = SsSemCreateLocal(SS_SEMNUM_DBE_SEQ);

        return(seq);
}

/*##**********************************************************************\
 *
 *		dbe_seq_done
 *
 * Releases a sequence object.
 *
 * Parameters :
 *
 *	seq -
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
void dbe_seq_done(
        dbe_seq_t* seq)
{
        ss_dprintf_1(("dbe_seq_done\n"));
        CHK_SEQ(seq);

        SsSemEnter(seq->seq_sem);
        su_rbt_done(seq->seq_rbt);
        SsSemExit(seq->seq_sem);
        SsSemFree(seq->seq_sem);
        SsMemFree(seq);
}

/*##**********************************************************************\
 *
 *		dbe_seq_create
 *
 * Creates a new sequence to the sequence object.
 *
 * Parameters :
 *
 *	seq -
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
dbe_ret_t dbe_seq_create(
        dbe_seq_t* seq,
        long seq_id,
        rs_err_t** p_errh)
{
        dbe_seqvalue_t* sv;

        ss_dprintf_1(("dbe_seq_create, seq_id = %ld\n", seq_id));
        CHK_SEQ(seq);

        sv = SSMEM_NEW(dbe_seqvalue_t);

        ss_debug(sv->sv_chk = DBE_CHK_SEQVAL);
        sv->sv_id = seq_id;
        rs_tuplenum_init(&sv->sv_value);
        rs_tuplenum_init(&sv->sv_rollbackvalue);
        sv->sv_isrollbackvalue = FALSE;
        sv->sv_dropcount = 0;
        sv->sv_nlink = 1;
        sv->sv_unlinksem = seq->seq_sem;

        SsSemEnter(seq->seq_sem);

        if (!su_rbt_insert(seq->seq_rbt, sv)) {
            SsSemExit(seq->seq_sem);
            rs_error_create(p_errh, DBE_ERR_SEQEXIST);
            return(DBE_ERR_SEQEXIST);
        } else {
            SsSemExit(seq->seq_sem);
            return(DBE_RC_SUCC);
        }
}

/*##**********************************************************************\
 *
 *		dbe_seq_markdropped
 *
 * Mark the sequence as droppped. This is called, when transaction drops a
 * sequence, but is not yet committed. The drop is committed by calling
 * function dbe_seq_drop and rolled back by calling function
 * dbe_seq_unmarkdropped.
 *
 * Parameters :
 *
 *	seq -
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
dbe_ret_t dbe_seq_markdropped(
        dbe_seq_t* seq,
        long seq_id)
{
        su_rbt_node_t* rn;
        dbe_seqvalue_t* sv;

        CHK_SEQ(seq);

        SsSemEnter(seq->seq_sem);

        ss_dprintf_1(("dbe_seq_markdropped, seq_id = %ld\n", seq_id));

        rn = su_rbt_search(seq->seq_rbt, (void*)seq_id);
        if (rn != NULL) {
            sv = su_rbtnode_getkey(rn);
            CHK_SEQVAL(sv);
            ss_dassert(sv->sv_dropcount >= 0);
            sv->sv_dropcount++;
            SsSemExit(seq->seq_sem);
            return(DBE_RC_SUCC);
        } else {
            SsSemExit(seq->seq_sem);
            return(DBE_ERR_SEQNOTEXIST);
        }
}

/*##**********************************************************************\
 *
 *		dbe_seq_drop
 *
 * Drops an existing sequence from the sequence object. This pysically
 * removes the sequence, so the calling transaction must be committed.
 *
 * Parameters :
 *
 *	seq -
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
dbe_ret_t dbe_seq_drop(
        dbe_seq_t* seq,
        long seq_id,
        rs_err_t** p_errh)
{
        su_rbt_node_t* rn;

        CHK_SEQ(seq);

        SsSemEnter(seq->seq_sem);

        ss_dprintf_1(("dbe_seq_drop, seq_id = %ld\n", seq_id));

        rn = su_rbt_search(seq->seq_rbt, (void*)seq_id);
        if (rn != NULL) {
            su_rbt_delete(seq->seq_rbt, rn);
            SsSemExit(seq->seq_sem);
            return(DBE_RC_SUCC);
        } else {
            SsSemExit(seq->seq_sem);
            rs_error_create(p_errh, DBE_ERR_SEQNOTEXIST);
            return(DBE_ERR_SEQNOTEXIST);
        }
}

/*##**********************************************************************\
 *
 *		dbe_seq_unmarkdropped
 *
 * Unmarks a sequence drop done in function dbe_seq_markdropped. This
 * function is used to roll back the sequence drop operation.
 *
 * Parameters :
 *
 *	seq -
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
dbe_ret_t dbe_seq_unmarkdropped(
        dbe_seq_t* seq,
        long seq_id)
{
        su_rbt_node_t* rn;
        dbe_seqvalue_t* sv;

        CHK_SEQ(seq);

        SsSemEnter(seq->seq_sem);

        ss_dprintf_1(("dbe_seq_unmarkdropped, seq_id = %ld\n", seq_id));

        rn = su_rbt_search(seq->seq_rbt, (void*)seq_id);
        if (rn != NULL) {
            sv = su_rbtnode_getkey(rn);
            CHK_SEQVAL(sv);
            ss_dassert(sv->sv_dropcount > 0);
            sv->sv_dropcount--;
            SsSemExit(seq->seq_sem);
            return(DBE_RC_SUCC);
        } else {
            SsSemExit(seq->seq_sem);
            return(DBE_ERR_SEQNOTEXIST);
        }
}

dbe_ret_t dbe_seq_lock(
        dbe_trx_t* trx,
        long seq_id,
        rs_err_t** p_errh)
{
        dbe_lock_reply_t reply;

        if (dbe_trx_isfailed(trx)) {
            dbe_ret_t rc;
            rc = dbe_trx_geterrcode(trx);
            rs_error_create(p_errh, rc);
            return(rc);
        }

        reply = dbe_trx_lockbyname(
                    trx,
                    DBE_LOCKRELID_SEQ,
                    seq_id,
                    LOCK_X,
                    dbe_trx_getlocktimeout(trx));

        switch (reply) {
            case LOCK_OK:
                return(DBE_RC_SUCC);
            case LOCK_WAIT:
                return(DBE_RC_WAITLOCK);
            default:
                rs_error_create(p_errh, DBE_ERR_DEADLOCK);
                return(DBE_ERR_DEADLOCK);
        }
}

void dbe_seq_unlock(
        dbe_trx_t* trx,
        long seq_id)
{
        if (dbe_trx_isfailed(trx)) {
            return;
        }

        dbe_trx_unlockbyname(
            trx,
            DBE_LOCKRELID_SEQ,
            seq_id);
}

/*#***********************************************************************\
 *
 *		seq_oper
 *
 * Implements a sequence operation, that is sequence next or current.
 * Both dense and sparse sequences are handled here.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	trx -
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
 *	seq_va -
 *
 *
 *	oper -
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
static dbe_ret_t seq_oper(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        bool replicap,
        rs_atype_t* atype,
        rs_aval_t* aval,
        va_t* seq_va,
        dbe_seqoper_t oper,
        rs_err_t** p_errh)
{
        dbe_seqvalue_t* sv;
        su_rbt_node_t* rn;
        dbe_ret_t rc;
        long new_value;
        ss_int8_t i8;
        rs_tuplenum_t saved_tuplenum;
        rs_tuplenum_t used_tuplenum;
        dbe_db_t* db;
        bool entergate = FALSE;
        bool unlinkp = TRUE;

        SS_PUSHNAME("seq_oper");
        CHK_SEQ(seq);
        ss_dassert(trx != NULL);

        db = dbe_trx_getdb(trx);

        if (dbe_trx_isfailed(trx)) {
            dbe_ret_t rc;
            rc = dbe_trx_geterrcode(trx);
            rs_error_create(p_errh, rc);
            SS_POPNAME;
            return(rc);
        }

        switch (oper) {
            case DBE_SEQOPER_CURRENT:
                break;
            case DBE_SEQOPER_SET:
            case DBE_SEQOPER_NEXT:
                if (!dbe_db_setchanged(db, p_errh)) {
                    SS_POPNAME;
                    return(DBE_ERR_DBREADONLY);
                }
                FAKE_IF(FAKE_DBE_SEQNEXTFAIL) {
                    rs_error_create(p_errh, DBE_ERR_DBREADONLY);
                    SS_POPNAME;
                    return(DBE_ERR_DBREADONLY);
                }
                entergate = TRUE;
                break;
            default:
                ss_error;
        }

        if (densep && !replicap) {
            /* Try to lock the sequence id.
             */
            dbe_ret_t rc;
            rc = dbe_seq_lock(trx, seq_id, p_errh);
            if (rc != DBE_RC_SUCC) {
                SS_POPNAME;
                return(rc);
            }
        }

        dbe_db_enteraction(db, NULL);

        switch (oper) {
            case DBE_SEQOPER_CURRENT:
                break;
            case DBE_SEQOPER_SET:
            case DBE_SEQOPER_NEXT:
                rc = dbe_trx_markwrite(trx, densep);
                if (rc != DBE_RC_SUCC) {
                    dbe_db_exitaction(db, NULL);
                    su_err_init(p_errh, rc);
                    SS_POPNAME;
                    return(rc);
                }
                break;
            default:
                ss_error;
        }

        SsSemEnter(seq->seq_sem);

        rn = su_rbt_search(seq->seq_rbt, (void*)seq_id);

        if (rn != NULL) {
            void* cd;
            dbe_user_t* user;

            sv = su_rbtnode_getkey(rn);
            CHK_SEQVAL(sv);

            seqvalue_link(sv);

            if (sv->sv_dropcount > 0) {
                /* There is a pending drop to this sequence.
                 */
                if (unlinkp) {
                    seqvalue_unlink(sv, FALSE);
                }
                SsSemExit(seq->seq_sem);
                dbe_db_exitaction(db, NULL);
                rs_error_create(p_errh, DBE_ERR_SEQDDOP);
                SS_POPNAME;
                return(DBE_ERR_SEQDDOP);
            }

            user = dbe_trx_getuser(trx);
            cd = dbe_user_getcd(user);

            switch (oper) {
                case DBE_SEQOPER_CURRENT:
                    memcpy(&used_tuplenum, &sv->sv_value, sizeof(rs_tuplenum_t));
                    SsSemExit(seq->seq_sem);
                    break;
                case DBE_SEQOPER_NEXT:
                    /* Increment sequence value.
                     */
                    memcpy(&saved_tuplenum, &sv->sv_value, sizeof(rs_tuplenum_t));
                    rs_tuplenum_inc(&sv->sv_value);
                    memcpy(&used_tuplenum, &sv->sv_value, sizeof(rs_tuplenum_t));
                    ss_dprintf_3(("seq_oper:DBE_SEQOPER_NEXT, seqvalue=%ld\n", rs_tuplenum_getlsl(&sv->sv_value)));
                    SsSemExit(seq->seq_sem);

                    if (densep) {
                        /* Mark to the transaction that dense sequence has been
                         * been changed.
                         */
                        rc = dbe_trx_markseqwrite(trx, sv);
                        if (rc == SU_SUCCESS) {
                            unlinkp = FALSE;
                            if (!sv->sv_isrollbackvalue) {
                                /* Save current value for transaction rollback. */
                                ss_dprintf_4(("seq_oper:seq_id = %ld, save rollbackvalue\n", seq_id));
                                sv->sv_isrollbackvalue = TRUE;
                                sv->sv_rollbackvalue = saved_tuplenum;
                            }
                        } else if (rc == DBE_RC_FOUND) {
                            rc = SU_SUCCESS;
                        }

                    } else {
                        /* Log the sparse sequence increment.
                         */
                        rc = dbe_trx_logseqinc(trx, seq_id, &used_tuplenum);
                    }
                    if (rc != DBE_RC_SUCC) {
                        if (!densep) {
                            /* Restore the sparse increment.
                             * Restore is done only if nobody else has not done incrementing
                             */
                            SsSemEnter(seq->seq_sem);
                            ss_dprintf_4(("seq_oper:DBE_SEQOPER_NEXT, FAILED, rc=%ld! seqvalue=%ld\n", rc, rs_tuplenum_getlsl(&sv->sv_value)));
                            if (rs_tuplenum_cmp(&used_tuplenum, &sv->sv_value) == 0) {
                                memcpy(&sv->sv_value, &saved_tuplenum, sizeof(rs_tuplenum_t));
                                ss_dprintf_4(("seq_oper:DBE_SEQOPER_NEXT, FAILED! restore old seqvalue=%ld\n", rs_tuplenum_getlsl(&sv->sv_value)));
                            }
                            SsSemExit(seq->seq_sem);
                        }
                        if (unlinkp) {
                            seqvalue_unlink(sv, TRUE);
                        }
                        dbe_db_exitaction(db, NULL);
                        rs_error_create(p_errh, rc);
                        SS_POPNAME;
                        return(rc);
                    }
                    break;
                case DBE_SEQOPER_SET:
                    /* Set sequence value from aval.
                     */
                    memcpy(&saved_tuplenum, &sv->sv_value, sizeof(rs_tuplenum_t));
                    if (seq_va != NULL) {
                        ss_dassert(atype == NULL);
                        ss_dassert(aval == NULL);
                        memcpy(&sv->sv_value, va_getasciiz(seq_va), sizeof(rs_tuplenum_t));
                    } else {
                        ss_dassert(atype != NULL);
                        ss_dassert(aval != NULL);
                        switch (rs_atype_datatype(cd, atype)) {
                            case RSDT_INTEGER:
                                if (rs_aval_isnull(cd, atype, aval)) {
                                    new_value = 0;
                                } else {
                                    new_value = rs_aval_getlong(cd, atype, aval);
                                }
                                rs_tuplenum_ulonginit(
                                    &sv->sv_value,
                                    0L,
                                    new_value);
                                break;
                            case RSDT_BIGINT:
                                if (rs_aval_isnull(cd, atype, aval)) {
                                    SsInt8Set0(&i8);
                                } else {
                                    i8 = rs_aval_getint8(cd, atype, aval);
                                }
                                rs_tuplenum_int8init(
                                    &sv->sv_value,
                                    i8);
                                break;
                            default:
                                if (unlinkp) {
                                    seqvalue_unlink(sv, FALSE);
                                }
                                SsSemExit(seq->seq_sem);
                                dbe_db_exitaction(db, NULL);
                                rs_error_create(p_errh, DBE_ERR_SEQILLDATATYPE);
                                SS_POPNAME;
                                return(DBE_ERR_SEQILLDATATYPE);
                        }
                    }
                    memcpy(&used_tuplenum, &sv->sv_value, sizeof(rs_tuplenum_t));
                    SsSemExit(seq->seq_sem);
                    if (densep) {
                        /* Mark to the transaction that dense sequence has been
                         * been changed.
                         */
                        rc = dbe_trx_markseqwrite(trx, sv);
                        if (rc == SU_SUCCESS){
                            unlinkp = FALSE;
                            if (!sv->sv_isrollbackvalue) {
                                /* Save current value for transaction rollback. */
                                ss_dprintf_4(("seq_oper:seq_id = %ld, save rollbackvalue\n", seq_id));
                                sv->sv_isrollbackvalue = TRUE;
                                sv->sv_rollbackvalue = saved_tuplenum;
                            }
                        } else if (rc == DBE_RC_FOUND) {
                            rc = SU_SUCCESS;
                        }
                    } else {
                        /* Log the sparse sequence increment.
                         */
                        rc = dbe_trx_logseqvalue(trx, seq_id, FALSE, &used_tuplenum);
                    }
                    if (rc != DBE_RC_SUCC) {
                        if (!densep) {
                            /* Restore the sparse set.
                             * Restore is done only if nobody else has not done incrementing/setting
                             */
                            SsSemEnter(seq->seq_sem);
                            if (rs_tuplenum_cmp(&used_tuplenum, &sv->sv_value) == 0) {
                                memcpy(&sv->sv_value, &saved_tuplenum, sizeof(rs_tuplenum_t));
                            }
                            SsSemExit(seq->seq_sem);
                        }
                        if (unlinkp) {
                            seqvalue_unlink(sv, TRUE);
                        }
                        dbe_db_exitaction(db, NULL);
                        rs_error_create(p_errh, rc);
                        SS_POPNAME;
                        return(rc);
                    }
                    break;
                default:
                    SsSemExit(seq->seq_sem);
                    ss_rc_error(oper);
            }

            if (atype != NULL) {
                /* Move the current value to the user aval. The user
                 * aval must be of long integer or binary data type.
                 */
                bool succp;
                ss_dassert(aval != NULL);
                ss_dassert(seq_va == NULL);
                succp = rs_tuplenum_setintoaval(
                            &used_tuplenum,
                            cd,
                            atype,
                            aval);
                if (!succp) {
                    if (unlinkp) {
                        seqvalue_unlink(sv, TRUE);
                    }
                    dbe_db_exitaction(db, NULL);
                    rs_error_create(p_errh, DBE_ERR_SEQILLDATATYPE);
                    SS_POPNAME;
                    return(DBE_ERR_SEQILLDATATYPE);
                }
            }
            rc = DBE_RC_SUCC;
            if (unlinkp) {
                seqvalue_unlink(sv, TRUE);
            }
        } else {
            SsSemExit(seq->seq_sem);
            rc = DBE_ERR_SEQNOTEXIST;
        }
        dbe_db_exitaction(db, NULL);
        if (rc != DBE_RC_SUCC) {
            rs_error_create(p_errh, DBE_ERR_SEQNOTEXIST);
        }
        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_seq_setvalue
 *
 * Sets sequence value unconditionally. Used in roll-forward recovery
 *
 * Parameters :
 *
 *	seq - use
 *
 *
 *	seq_id - in
 *
 *
 *	value - in, use
 *
 *
 * Return value :
 *      DBE_RC_SUCC or DBE_ERR_SEQNOTEXIST
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_seq_setvalue(dbe_seq_t* seq, long seq_id, rs_tuplenum_t* value)
{
        dbe_seqvalue_t* sv;
        su_rbt_node_t* rn;
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_seq_setvalue, seq_id = %ld\n", seq_id));
        CHK_SEQ(seq);

        SsSemEnter(seq->seq_sem);
        rn = su_rbt_search(seq->seq_rbt, (void*)seq_id);
        if (rn != NULL) {
            sv = su_rbtnode_getkey(rn);
            CHK_SEQVAL(sv);
            ss_dassert(sv->sv_dropcount == 0);
            sv->sv_value = *value;
            rc = DBE_RC_SUCC;
        } else {
            rc = DBE_ERR_SEQNOTEXIST;
        }
        SsSemExit(seq->seq_sem);
        return (rc);
}

/*##**********************************************************************\
 *
 *		dbe_seq_inc
 *
 * Increments sequence unconditionally. Used in roll-forward recovery
 *
 * Parameters :
 *
 *	seq - use
 *
 *
 *	seq_id - in
 *
 *
 * Return value :
 *      DBE_RC_SUCC or DBE_ERR_SEQNOTEXIST
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_seq_inc(dbe_seq_t* seq, long seq_id)
{
        dbe_seqvalue_t* sv;
        su_rbt_node_t* rn;
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_seq_inc, seq_id = %ld\n", seq_id));
        CHK_SEQ(seq);

        SsSemEnter(seq->seq_sem);
        rn = su_rbt_search(seq->seq_rbt, (void*)seq_id);
        if (rn != NULL) {
            sv = su_rbtnode_getkey(rn);
            CHK_SEQVAL(sv);
            ss_dassert(sv->sv_dropcount == 0);
            rs_tuplenum_inc(&sv->sv_value);
            ss_dprintf_2(("dbe_seq_inc:seqvalue=%ld\n", rs_tuplenum_getlsl(&sv->sv_value)));
            rc = DBE_RC_SUCC;
        } else {
            rc = DBE_ERR_SEQNOTEXIST;
        }
        SsSemExit(seq->seq_sem);
        return (rc);
}

/*##**********************************************************************\
 *
 *		dbe_seq_next
 *
 * Gets the next sequnce value.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	trx -
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
dbe_ret_t dbe_seq_next(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        rs_err_t** p_errh)
{
        ss_dprintf_1(("dbe_seq_next, seq_id = %ld, densep = %d\n", seq_id, densep));
        CHK_SEQ(seq);

        return(seq_oper(
                seq,
                trx,
                seq_id,
                densep,
                FALSE,
                atype,
                aval,
                NULL,
                DBE_SEQOPER_NEXT,
                p_errh));
}

/*##**********************************************************************\
 *
 *		dbe_seq_current
 *
 * Gets the current sequence value.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	trx -
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
dbe_ret_t dbe_seq_current(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        rs_err_t** p_errh)
{
        ss_dprintf_1(("dbe_seq_current, seq_id = %ld, densep = %d\n", seq_id, densep));
        CHK_SEQ(seq);

        return(seq_oper(
                seq,
                trx,
                seq_id,
                densep,
                FALSE,
                atype,
                aval,
                NULL,
                DBE_SEQOPER_CURRENT,
                p_errh));
}

/*##**********************************************************************\
 *
 *		dbe_seq_set
 *
 * Sets the current sequence value.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	trx -
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
dbe_ret_t dbe_seq_set(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        rs_err_t** p_errh)
{
        ss_dprintf_1(("dbe_seq_set, seq_id = %ld, densep = %d\n", seq_id, densep));
        CHK_SEQ(seq);

        return(seq_oper(
                seq,
                trx,
                seq_id,
                densep,
                FALSE,
                atype,
                aval,
                NULL,
                DBE_SEQOPER_SET,
                p_errh));
}

/*##**********************************************************************\
 *
 *		dbe_seq_setreplica
 *
 * Sequence set routine for replication.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	trx -
 *
 *
 *	seq_id -
 *
 *
 *	densep -
 *
 *
 *	va -
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
dbe_ret_t dbe_seq_setreplica(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        va_t* va,
        rs_err_t** p_errh)
{
        ss_dprintf_1(("dbe_seq_setreplica, seq_id = %ld, densep = %d\n", seq_id, densep));
        CHK_SEQ(seq);

        return(seq_oper(
                seq,
                trx,
                seq_id,
                densep,
                TRUE,
                NULL,
                NULL,
                va,
                DBE_SEQOPER_SET,
                p_errh));
}

/*##**********************************************************************\
 *
 *		dbe_seq_commit
 *
 * Commits a dense sequence change.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	trx -
 *
 *
 *	sv -
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
dbe_ret_t dbe_seq_commit(
        dbe_seq_t* seq __attribute__ ((unused)),
        dbe_trx_t* trx,
        dbe_seqvalue_t* sv)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_seq_commit, seq_id = %ld\n", dbe_seqvalue_getid(sv)));
        CHK_SEQ(seq);
        CHK_SEQVAL(sv);
        ss_dassert(sv->sv_isrollbackvalue);

        /* Put the sequence value to the log. */
        rc = dbe_trx_logseqvalue(trx, sv->sv_id, TRUE, &sv->sv_value);

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_seq_transend
 *
 * Ends transaction for sequence identified by sequence value.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	sv -
 *
 *
 *	commitp -
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
void dbe_seq_transend(
        dbe_seq_t* seq __attribute__ ((unused)),
        dbe_seqvalue_t* sv,
        bool commitp)
{
        ss_dprintf_1(("dbe_seq_transend, seq_id = %ld\n", dbe_seqvalue_getid(sv)));
        CHK_SEQ(seq);
        CHK_SEQVAL(sv);
        ss_dassert(sv->sv_isrollbackvalue);

        sv->sv_isrollbackvalue = FALSE;

        if (!commitp) {
            /* Restore saved value. */
            sv->sv_value = sv->sv_rollbackvalue;
        }
        seqvalue_unlink(sv, TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_seq_entermutex
 *
 * Enters sequence mutex.
 *
 * Parameters :
 *
 *	seq -
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
void dbe_seq_entermutex(
        dbe_seq_t* seq)
{
        CHK_SEQ(seq);

        SsSemEnter(seq->seq_sem);
}

/*##**********************************************************************\
 *
 *		dbe_seq_exitmutex
 *
 * Exits sequence mutex.
 *
 * Parameters :
 *
 *	seq -
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
void dbe_seq_exitmutex(
        dbe_seq_t* seq)
{
        CHK_SEQ(seq);

        SsSemExit(seq->seq_sem);
}

/*##**********************************************************************\
 *
 *		dbe_seq_save_nomutex
 *
 * Saves all sequence values to the disk. This should be called
 * during checkpoint creation.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	cache -
 *
 *
 *	freelist -
 *
 *
 *	cpnum -
 *
 *
 *	p_seqlistdaddr -
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
dbe_ret_t dbe_seq_save_nomutex(
        dbe_seq_t* seq,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_cpnum_t cpnum,
        su_daddr_t* p_seqlistdaddr)
{
        dbe_seqvalue_t* sv;
        su_rbt_node_t* rn;
        dbe_ret_t rc = DBE_RC_SUCC;
        dbe_seqlist_t* seqlist;

        ss_dprintf_1(("dbe_seq_save_nomutex\n"));
        CHK_SEQ(seq);
        ss_dassert(SsSemIsEntered(seq->seq_sem));

        /* Save list of committed sequence values.
         */
        seqlist = dbe_seql_init(cache, freelist, cpnum);

        /* Scan the sequence tree and store the records
         */
        rn = su_rbt_min(seq->seq_rbt, NULL);
        while (rn != NULL) {
            sv = su_rbtnode_getkey(rn);
            CHK_SEQVAL(sv);
            ss_dprintf_2(("dbe_seq_save_nomutex:seq_id = %ld\n", sv->sv_id));
            if (sv->sv_isrollbackvalue) {
                /* Save the rollback value, because current value is not
                 * yet committed.
                 */
                rc = dbe_seql_add(
                        seqlist,
                        sv->sv_id,
                        &sv->sv_rollbackvalue);
                su_rc_assert(rc == SU_SUCCESS, rc);
            } else {
                rc = dbe_seql_add(
                        seqlist,
                        sv->sv_id,
                        &sv->sv_value);
                su_rc_assert(rc == SU_SUCCESS, rc);
            }
            rn = su_rbt_succ(seq->seq_rbt, rn);
        }
        rc = dbe_seql_save(
                seqlist,
                p_seqlistdaddr);
        su_rc_assert(rc == SU_SUCCESS, rc);

        dbe_seql_done(seqlist);

        return (DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_seq_restore
 *
 * Restores sequence values from the disk. This function is used to
 * read sequence values from disk when database is opened.
 *
 * Parameters :
 *
 *	seq -
 *
 *
 *	cache -
 *
 *
 *	seqlistdaddr -
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
dbe_ret_t dbe_seq_restore(
        dbe_seq_t* seq,
        dbe_cache_t* cache,
        su_daddr_t seqlistdaddr)
{
        dbe_seqlist_iter_t* seqli;
    FOUR_BYTE_T id;
        rs_tuplenum_t value;
        dbe_seqvalue_t* sv;

        ss_dprintf_1(("dbe_seq_restore\n"));
        CHK_SEQ(seq);

        SsSemEnter(seq->seq_sem);

        seqli = dbe_seqli_init(cache, seqlistdaddr);

        /* Scan the seqlist iterator and restore all seqinfo nodes */
        while (dbe_seqli_getnext(seqli, &id, &value)) {
            bool succp;

            ss_dprintf_2(("dbe_seq_restore:seq_id = %ld\n", id));

            sv = SSMEM_NEW(dbe_seqvalue_t);

            ss_debug(sv->sv_chk = DBE_CHK_SEQVAL);
            sv->sv_id = id;
            sv->sv_value = value;
            rs_tuplenum_init(&sv->sv_rollbackvalue);
            sv->sv_isrollbackvalue = FALSE;
            sv->sv_dropcount = 0;
            sv->sv_nlink = 1;
            sv->sv_unlinksem = seq->seq_sem;

            succp = su_rbt_insert(seq->seq_rbt, sv);
            ss_assert(succp);
        }
        dbe_seqli_done(seqli);

        SsSemExit(seq->seq_sem);

        return (DBE_RC_SUCC);
}

#endif /* SS_NOSEQUENCE */
