/*************************************************************************\
**  source       * dbe4rfwd.c
**  directory    * dbe
**  description  * Roll-forward recovery
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
#include <ssmem.h>
#include <ssservic.h>
#include <su0rbtr.h>
#include <rs0rbuf.h>
#include <rs0relh.h>
#include <rs0viewh.h>

#include "dbe7binf.h"
#include "dbe7logf.h"
#include "dbe4tupl.h"
#include "dbe4rfwd.h"
#include "dbe1seq.h"
#include "dbe0db.h"
#include "dbe0seq.h"

#ifndef SS_MYSQL
#include "dbe0hsb.h"
#endif /* !SS_MYSQL */

#include "dbe7rfl.h"
#include "dbe0blobg2.h"
#include "dbe0logi.h"
#include "dbe0user.h"

#ifdef SS_HSBG2
#include "dbe0hsbg2.h"
#include "dbe0catchup.h"
#endif /* SS_HSBG2 */

bool dbe_recoverignoretimestamps = FALSE; /* ignore timestamps during recovery */

#ifndef SS_NOLOGGING

typedef enum {
        RFSTATE_START,
        RFSTATE_SCANNED,
        RFSTATE_RECOVERED
} rf_state_t;

/* The roll-forward recovery manager object
 */
struct dbe_rollfwd_st {
        rf_state_t      rf_state;
        ulong           rf_ncommits;
        bool            rf_rtrxfound;
        dbe_cfg_t*      rf_cfg;
        dbe_counter_t*  rf_counter;
        rs_sysinfo_t*   rf_cd;              /* client data */
        rs_rbuf_t*      rf_rbuf;            /* relation buffer */
        dbe_trx_t*      rf_trx;             /* roll-forward transaction */
        dbe_trxbuf_t*   rf_trxbuf;          /* trx info buffer */
        dbe_rflog_t*    rf_log;             /* roll-forward log object */
        dbe_file_t*     rf_dbfile;
        dbe_gobj_t*     rf_gobjs;
        dbe_logpos_t    rf_restartpos;
        bool            rf_iscorrupt;
        dbe_logpos_t    rf_corruptpos;
        dbe_trxnum_t    rf_committrxnum;
        dbe_trxid_t     rf_maxtrxid;
        dbe_blobg2id_t  rf_maxblobg2id;
        FOUR_BYTE_T     rf_dbcreatime;
        dbe_db_recovcallback_t* rf_recovcallback;
        SsTimeT         rf_cptimestamp;
        su_rbt_t*       rf_blobpool;
#ifdef DBE_REPLICATION
        dbe_hsbmode_t   rf_hsbmode;         /* Initial replication mode */
        dbe_rtrxbuf_t*  rf_rtrxbuf;
        dbe_trxid_t     rf_reptrxidlast;
        bool            rf_hsbcommitfound;
        bool            rf_reptrxidmaxupdated;
        dbe_logpos_t    rf_hsbstartpos;
        su_list_t*      rf_hsbcommitlist;
#endif /* DBE_REPLICATION */
#ifdef SS_HSBG2
        dbe_hsbg2_t*      rf_hsbsvc;
        long              rf_durablecount;
        dbe_catchup_logpos_t rf_local_durable_logpos;
        dbe_catchup_logpos_t rf_remote_durable_logpos;
        int                  rf_hsbflags;
        dbe_cpnum_t       rf_last_cpnum;
        su_list_t*        rf_savedlogposlist;

#endif /* SS_HSBG2 */
};

/* Structure for representing blob during
 * roll-forward
 */
typedef struct rf_blob_st rf_blob_t;
struct rf_blob_st {
        rs_sysi_t* rfb_cd;
        dbe_blobg2id_t rfb_id;
        dbe_blobg2size_t rfb_startoffset; /* recovery start position */
        tb_wblobg2stream_t* rfb_blob;
        enum {
            RFBSTAT_INIT,
            RFBSTAT_OPEN,
            RFBSTAT_CLOSED,
            RFBSTAT_OPEN_TOBEREMOVED
        } rfb_status;
        /* bool rfb_completed; */
};

/*#***********************************************************************\
 *
 *              rf_blob_insertcmp
 *
 * Compare function for insert operations of rbtree
 *
 * Parameters :
 *
 *      blob1 - in, use
 *              pointer to 1st blob
 *
 *      blob2 - in, use
 *              pointer to 2nd blob
 *
 * Return value :
 *      == 0 when blob1->rfb_id == blob2->rfb_id
 *       < 0 when blob1->rfb_id < blob2->rfb_id
 *       > 0 when blob1->rfb_id > blob2->rfb_id
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static int rf_blob_insertcmp(void* blob1, void* blob2)
{
        int cmp = DBE_BLOBG2ID_CMP(((rf_blob_t*)blob1)->rfb_id,
                                   ((rf_blob_t*)blob2)->rfb_id);
        return (cmp);
}

/*#***********************************************************************\
 *
 *              rf_blob_searchcmp
 *
 * Compare function for search operations of rbtree
 *
 * Parameters :
 *
 *      p_blobid - in, use
 *              pointer to blobid to be searched
 *
 *      blob - in, use
 *              pointer to blob in rbtree
 *
 * Return value :
 *      == 0 when *p_blobid == blob->rfb_id
 *       < 0 when *p_blobid < blob->rfb_id
 *       > 0 when *p_blobid > blob->rfb_id
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static int rf_blob_searchcmp(void* p_blobid, void* blob)
{
        int cmp = DBE_BLOBG2ID_CMP(*(dbe_blobg2id_t*)p_blobid,
                                   ((rf_blob_t*)blob)->rfb_id);
        return (cmp);
}

/*#***********************************************************************\
 *
 *          rf_blob_init
 *
 * Creates a roll-forward blob object
 *
 * Parameters :
 *
 *      cd - in, hold
 *          client data context
 *
 *      id - in
 *          BLOB ID
 *
 *      startoffset - in
 *          byte position where the blob recovery starts
 *
 *      blobinfo - in, hold
 *          pointer to blobinfo object containig the blob
 *
 * Return value - give :
 *      pointer to new blob object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rf_blob_t* rf_blob_init(
        rs_sysi_t* cd,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startoffset)
{
        rf_blob_t* rfblob;

        rfblob = SSMEM_NEW(rf_blob_t);
        rfblob->rfb_cd = cd;
        rfblob->rfb_id = id;
        rfblob->rfb_startoffset = startoffset;
        rfblob->rfb_blob = NULL;
        rfblob->rfb_status = RFBSTAT_INIT;
        return (rfblob);
}

/*#***********************************************************************\
 *
 *          rf_blob_open
 *
 * Opens a roll-forward blob object for writing
 *
 * Parameters :
 *
 *      rfblob - in out, use
 *          pointer to roll-forward blob object
 *
 *      rf - in, use
 *          pointer to roll-forward object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void rf_blob_open(
        rf_blob_t* rfblob,
        dbe_rollfwd_t* rf)
{
        if (rfblob->rfb_status != RFBSTAT_INIT) {
            ss_rc_dassert(rfblob->rfb_status == RFBSTAT_OPEN, rfblob->rfb_status);
            ss_dassert(rfblob->rfb_blob != NULL);
            return;
        }
        ss_dassert(rfblob->rfb_blob == NULL);
        rfblob->rfb_blob =
            (*dbe_blobg2callback_wblobinit_for_recovery)(
                    rf->rf_cd,
                    rfblob->rfb_id,
                    rfblob->rfb_startoffset);
        if (rfblob->rfb_blob != NULL) {
            rfblob->rfb_status = RFBSTAT_OPEN;
        } else {
            ss_derror;
        }
}

/*#***********************************************************************\
 *
 *              rf_blob_done
 *
 * Deletes a roll-forward blob object
 *
 * Parameters :
 *
 *      p - in, take
 *              pointer to roll-forward blob object (type is void* because
 *          this is used as a callback function for rbtree)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void rf_blob_done(void* p)
{
        rf_blob_t* rfblob = p;

        ss_dassert(p != NULL);
        ss_pprintf_1(("rf_blob_done: id = %ld\n", DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(rfblob->rfb_id)));
        switch (rfblob->rfb_status) {
            case RFBSTAT_INIT:
                ss_rc_derror(rfblob->rfb_status);
                break;
            case RFBSTAT_OPEN:
                ss_dassert(rfblob->rfb_blob != NULL);
                (*dbe_blobg2callback_wblobabort)(rfblob->rfb_blob);
                break;
            case RFBSTAT_CLOSED: {
                ss_debug(su_ret_t rc;)
                ss_dassert(rfblob->rfb_blob == NULL);
                ss_debug(rc =)
                    (*dbe_blobg2callback_decrementinmemoryrefcount)(
                            rfblob->rfb_cd,
                            rfblob->rfb_id,
                            NULL);
                ss_rc_dassert(rc == SU_SUCCESS, rc);
                break;
            }
            case RFBSTAT_OPEN_TOBEREMOVED:
                ss_dassert(rfblob->rfb_blob != NULL);
                {
                    ss_debug(su_ret_t rc =)
                        (*dbe_blobg2callback_wblobdone)(rfblob->rfb_blob, NULL);
                    ss_rc_dassert(rc == SU_SUCCESS, rc);
                }
                break;
            default:
                ss_rc_error(rfblob->rfb_status);
                break;
        }
        SsMemFree(rfblob);
}

static void rollfwd_removeblob(
        dbe_rollfwd_t* rf,
        rf_blob_t* blob)
{
        su_rbt_node_t* node;
        dbe_blobg2id_t blobid;

        blobid = blob->rfb_id;
        node = su_rbt_search(rf->rf_blobpool, &blobid);
        ss_dassert(node != NULL);
        if (node != NULL) {
            su_rbt_delete(rf->rf_blobpool, node);
        }

}

/*#***********************************************************************\
 *
 *              rollfwd_findblob
 *
 * Finds a blob from the blobinfo pool
 *
 * Parameters :
 *
 *      rf - in, use
 *              pointer to roll-forward object
 *
 *      blobid - in
 *              blob id
 *
 * Return value - ref :
 *      pointer to roll-forward blob or
 *      NULL if not found
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rf_blob_t* rollfwd_findblob(
        dbe_rollfwd_t* rf,
        dbe_blobg2id_t blobid)
{
        su_rbt_node_t* node;
        rf_blob_t* blob;

        node = su_rbt_search(rf->rf_blobpool, &blobid);
        if (node == NULL) {
            return (NULL);
        }
        blob = su_rbtnode_getkey(node);
        return (blob);
}

/*#***********************************************************************\
 *
 *              rollfwd_updatemaxtrxid
 *
 * Updates the biggest found trxid field in roll-forward object
 *
 * Parameters :
 *
 *      rf - in out, use
 *              pointer to roll-forward object
 *
 *      trxid - in
 *              the newest read-in trx id (or stmt trx id)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void rollfwd_updatemaxtrxid(
        dbe_rollfwd_t* rf,
        dbe_trxid_t trxid)
{
        if (DBE_TRXID_EQUAL(rf->rf_maxtrxid, DBE_TRXID_NULL)
        ||  DBE_TRXID_CMP_EX(trxid,rf->rf_maxtrxid) > 0L)
        {
            rf->rf_maxtrxid = trxid;
        }
}

static void rollfwd_updatemaxblobg2id(
        dbe_rollfwd_t* rf,
        dbe_blobg2id_t bid)
{
        if (DBE_BLOBG2ID_CMP(rf->rf_maxblobg2id, bid) < 0) {
            rf->rf_maxblobg2id = bid;
        }
}

#ifdef DBE_REPLICATION
/*#***********************************************************************\
 *
 *              rollfwd_updatereptrxidmax
 *
 * Updates the biggest found remotetrxid field in roll-forward object
 *
 * Parameters :
 *
 *      rf - in out, use
 *              pointer to roll-forward object
 *
 *      remotetrxid - in
 *              the newest read-in trx id (or stmt trx id)
 *
 *      hsbcommitfound - in
 *              for secondary - always true
 *              for primary - true if hsbcommitmark is found (after last ckpt)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void rollfwd_updatereptrxidlast(
        dbe_rollfwd_t* rf,
        dbe_trxid_t remotetrxid,
        bool hsbcommitfound)
{
        rf->rf_reptrxidlast = remotetrxid;
        rf->rf_hsbcommitfound = hsbcommitfound;
        rf->rf_reptrxidmaxupdated = TRUE;
}

/*#***********************************************************************\
 *
 *              rollfwd_clearreptrxidlast
 *
 * Clears lasr replicated transaction id if we encounter role switch during
 * recovery.
 *
 * Parameters :
 *
 *      rf - in, use
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
static void rollfwd_clearreptrxidlast(
        dbe_rollfwd_t* rf)
{
        rf->rf_hsbcommitfound = FALSE;
}

/*#***********************************************************************\
 *
 *              rollfwd_hsbswitchreset
 *
 * Resets HSB stuff when we find role switch marker during recovery.
 *
 * Parameters :
 *
 *      rf -
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
static void rollfwd_hsbswitchreset(
        dbe_rollfwd_t* rf)
{
        ss_pprintf_3(("rollfwd_hsbswitchreset: call dbe_trxbuf_cleanuncommitted()\n"));
        su_rbt_deleteall(rf->rf_blobpool);
        dbe_trxbuf_cleanuncommitted(rf->rf_trxbuf, dbe_counter_getcommittrxnum(rf->rf_counter));
        dbe_rtrxbuf_deleteall(rf->rf_rtrxbuf);
        rollfwd_clearreptrxidlast(rf);
}

#endif /* DBE_REPLICATION */

/*#***********************************************************************\
 *
 *          rollfwd_addblob
 *
 * Adds a blob to blob info pool and trxinfo
 *
 * Parameters :
 *
 *      rf - in, use
 *          pointer to roll-forward object
 *
 *      id - in, use
 *          blob id
 *
 *      startoffset - in
 *          starting point for blob (!= 0 when blob write started before
 *          the checkpoint where the recovery started)
 *
 * Return value - ref:
 *      pointer to added roll-forward blob record
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rf_blob_t* rollfwd_addblob(
        dbe_rollfwd_t* rf,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startoffset)
{
        rf_blob_t* blob;

        blob = rf_blob_init(rf->rf_cd,
                            id,
                            startoffset);
        ss_dassert(su_rbt_search(rf->rf_blobpool, &id) == NULL);
        su_rbt_insert(rf->rf_blobpool, blob);
        return (blob);
}

/*#***********************************************************************\
 *
 *              rollfwd_vtplhasblobs
 *
 * Checks whether a vtuple contains blobs
 *
 * Parameters :
 *
 *      rf - in, use
 *              pointer to roll-forward object
 *
 *      p_vtpl - in, use
 *              pointer to vtuple
 *
 *      relh - in, use
 *              pointer to relation handle
 *
 * Return value :
 *      TRUE if vtuple has any blobs or
 *      FALSE otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool rollfwd_vtplhasblobs(
        dbe_rollfwd_t* rf,
        vtpl_t* p_vtpl,
        rs_relh_t* relh)
{
        rs_ano_t nattrs;
        rs_ano_t nkeyparts;
        rs_key_t* clusterkey;

        nattrs = (rs_ano_t)vtpl_vacount(p_vtpl);
        clusterkey = rs_relh_clusterkey(rf->rf_cd, relh);
        ss_dassert(clusterkey != NULL);
        nkeyparts = rs_key_nparts(rf->rf_cd, clusterkey);
        if (nkeyparts < nattrs) {
            ss_dassert(nkeyparts + 1 == nattrs);
            return (TRUE);
        }
        return (FALSE);
}

static void rollfwd_close1blob(
        dbe_rollfwd_t* rf,
        dbe_blobg2id_t id,
        bool remove)
{
        rf_blob_t* blob;
        tb_wblobg2stream_t* wbs;
        
        blob = rollfwd_findblob(rf, id);
        if (blob != NULL) {
            wbs = blob->rfb_blob;
            if (!remove) {
                switch (blob->rfb_status) {
                    case RFBSTAT_INIT:
                        ss_rc_derror(blob->rfb_status);
                        break;
                    case RFBSTAT_OPEN: {
                        ss_debug(su_ret_t rc;)
                        ss_dassert(blob->rfb_blob != NULL);

                        ss_debug(rc =)
                            (*dbe_blobg2callback_incrementinmemoryrefcount)(
                                    rf->rf_cd,
                                    blob->rfb_id,
                                    NULL);
                        ss_rc_dassert(rc == SU_SUCCESS, rc);
                        ss_debug(rc =)
                            (*dbe_blobg2callback_wblobdone)(
                                    blob->rfb_blob, NULL);
                        ss_rc_dassert(rc == SU_SUCCESS, rc);
                        blob->rfb_status = RFBSTAT_CLOSED;
                        blob->rfb_blob = NULL;
                        break;
                    }
                    case RFBSTAT_CLOSED:
                        ss_rc_derror(blob->rfb_status);
                        break;
                    case RFBSTAT_OPEN_TOBEREMOVED:
                        ss_rc_derror(blob->rfb_status);
                        break;
                    default:
                        ss_rc_error(blob->rfb_status);
                        break;
                }
            } else {
                switch (blob->rfb_status) {
                    case RFBSTAT_INIT:
                        ss_rc_derror(blob->rfb_status);
                        break;
                    case RFBSTAT_OPEN:
                        blob->rfb_status = RFBSTAT_OPEN_TOBEREMOVED;
                        break;
                    case RFBSTAT_CLOSED:
                        break;
                    case RFBSTAT_OPEN_TOBEREMOVED:
                        ss_rc_derror(blob->rfb_status);
                        break;
                    default:
                        ss_rc_error(blob->rfb_status);
                        break;
                }
                rollfwd_removeblob(rf, blob);
            }
        } else {
            /* There are 2 possible ways to get into this branch:
               1. There has been a checkpoint between insertion of the
               the blob and the insertion of this row.
               2. The row insertion has a column value which is a copy
               of another column value (reference count system!)
            */
        }
}
        
static void rollfwd_closeblobs(
        dbe_rollfwd_t* rf,
        vtpl_t* p_vtpl)
{
        va_index_t i;
        int nattrs;
        va_t* p_va;
        dbe_vablobg2ref_t blobref;
        bool *blobflags;
        ss_debug(int blobcount = 0;)

        SS_NOTUSED(rf);

        blobflags = dbe_blobinfo_getattrs(p_vtpl, 0, &nattrs);
        p_va = VTPL_GETVA_AT0(p_vtpl);
        for (i = 0; i < (va_index_t)nattrs; i++, p_va = VTPL_SKIPVA(p_va)) {
            if (blobflags[i]) {
                ss_debug(blobcount++);
                dbe_brefg2_loadfromva(&blobref, p_va);
                ss_pprintf_1(("INSTUPLEWITHBLOBS searching for blob (id = 0x%08lX)\n",
                              (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(dbe_brefg2_getblobg2id(&blobref))));
                rollfwd_close1blob(rf, dbe_brefg2_getblobg2id(&blobref), TRUE);
            } else {
                ss_dassert(!va_testblob(p_va));
            }
        }
        ss_rc_dassert(blobcount > 0, blobcount);
        SsMemFree(blobflags);
}

static void rf_savedlogposlist_done(void* data)
{
        dbe_catchup_savedlogpos_done(data);
}

/*##**********************************************************************\
 *
 *              dbe_rollfwd_init
 *
 * Creates a roll-forward recovery object
 *
 * Parameters :
 *
 *      cfg - in, hold
 *              pointer to configuration object
 *
 *      counter - in, hold
 *              pointer to counter object
 *
 *      user - in, hold
 *              pointer to user object
 *
 *      trxbuf - in, hold
 *              pointer to trx buffer object
 *
 *      gobjs - in, hold
 *              pointer to global objects structure
 *
 *      getrelhfun - in, hold
 *              pointer to callback function that gets a relation handle
 *
 *      refreshrbuffun - in, hold
 *          pointer to function that refreshes rbuf (relation buffer)
 *
 *      rbufctx - in, use
 *          context to getrelhfun
 *
 *      dbcreatime - in
 *          database creation time (for checking the log file is correct)
 *
 *      cptimestamp - in
 *          checkpoint timestamp
 *
 *      hsbmode - in
 *          initial replication mode, one of:
 *              DBE_HSB_STANDALONE
 *              DBE_HSB_PRIMARY
 *              DBE_HSB_SECONDARY
 *
 *      rtrxbuf - in, hold
 *          replicated transaction buffer
 *
 *      hsbsvc - in, hold
 *          HSB service, this will receive logdata objects during recovery
 *
 * Return value - give :
 *      pointer to new roll-forward object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_rollfwd_t* dbe_rollfwd_init(
        dbe_cfg_t* cfg,
        dbe_counter_t* counter,
        dbe_user_t* user,
        dbe_trxbuf_t* trxbuf,
        dbe_gobj_t* gobjs,
        dbe_db_recovcallback_t* recovcallback,
        SsTimeT dbcreatime,
        SsTimeT cptimestamp,
        dbe_hsbmode_t hsbmode,
        dbe_trxid_t reptrxidmax,
        dbe_rtrxbuf_t* rtrxbuf
#ifdef SS_HSBG2
        , dbe_hsbg2_t* hsbsvc
#endif /* SS_HSBG2 */
        )
{
        dbe_rollfwd_t* rf;

        ss_pprintf_1(("dbe_rollfwd_init:dbcreatime=%ld, cptimestamp=%ld\n", dbcreatime, cptimestamp));

        rf = SSMEM_NEW(dbe_rollfwd_t);
        rf->rf_ncommits = 0L;
        rf->rf_rtrxfound = FALSE;
        rf->rf_cfg = cfg;
        rf->rf_counter = counter;
        rf->rf_cd = dbe_user_getcd(user);
        rf->rf_rbuf = rs_sysi_rbuf(rf->rf_cd);
        rf->rf_trxbuf = trxbuf;
        rf->rf_dbfile = gobjs->go_dbfile;
        rf->rf_gobjs = gobjs;
        rf->rf_trx = DBE_TRX_NOTRX;
        rf->rf_log = dbe_rflog_init(rf->rf_cfg, rf->rf_cd, rf->rf_counter);
        rf->rf_recovcallback = recovcallback;
        rf->rf_state = RFSTATE_START;
        rf->rf_iscorrupt = FALSE;
        rf->rf_committrxnum = DBE_TRXNUM_NULL;
        rf->rf_maxtrxid = DBE_TRXID_NULL;
        rf->rf_maxblobg2id = DBE_BLOBG2ID_NULL;
        rf->rf_dbcreatime = dbcreatime;
        rf->rf_cptimestamp = cptimestamp;
        rf->rf_blobpool = su_rbt_inittwocmp(rf_blob_insertcmp,
                                            rf_blob_searchcmp,
                                            rf_blob_done);

#ifdef SS_HSBG2
        rf->rf_hsbsvc = hsbsvc;
        rf->rf_hsbcommitlist = su_list_init(NULL);
#endif /* SS_HSBG2 */
#ifdef DBE_REPLICATION
        rf->rf_hsbmode = hsbmode;
        rf->rf_rtrxbuf = rtrxbuf;
        rf->rf_reptrxidlast = reptrxidmax;
        rf->rf_hsbcommitfound = FALSE;
        rf->rf_reptrxidmaxupdated = FALSE;
        memset(&rf->rf_hsbstartpos, 0, sizeof(rf->rf_hsbstartpos));
        rf->rf_savedlogposlist = su_list_init(rf_savedlogposlist_done);
#else  /* DBE_REPLICATION */
        ss_dassert(hsbmode == DBE_HSB_STANDALONE);
        ss_dassert(rtrxbuf == NULL);
#endif  /* DBE_REPLICATION */
        return (rf);
}

/*##**********************************************************************\
 *
 *              dbe_rollfwd_done
 *
 * Deletes a roll-forward object
 *
 * Parameters :
 *
 *      rf - in, take
 *              pointer to roll-forward object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_rollfwd_done(
        dbe_rollfwd_t* rf)
{
        ss_dassert(rf != NULL);
        dbe_rflog_done(rf->rf_log);
        su_rbt_done(rf->rf_blobpool);
        su_list_done(rf->rf_hsbcommitlist);
        su_list_done(rf->rf_savedlogposlist);
        SsMemFree(rf);
}

static dbe_trxinfo_t* rollfwd_trxinfo_init(
        dbe_rollfwd_t* rf,
        dbe_trxid_t trxid)
{
        dbe_trxinfo_t* trxinfo = dbe_trxinfo_init(rf->rf_cd);

        trxinfo->ti_usertrxid = trxid;
        dbe_trxbuf_add(rf->rf_trxbuf, trxinfo);
        dbe_trxinfo_done(trxinfo, rf->rf_cd, dbe_trxbuf_getsembytrxid(rf->rf_trxbuf, trxid));
        trxinfo = dbe_trxbuf_gettrxinfo(
                rf->rf_trxbuf,
                trxid);
        return (trxinfo);
}


/*##**********************************************************************\
 *
 *              dbe_rollfwd_scancommitmarks
 *
 * Scans the log file for transaction commit marks and adds trxinfo for
 * every committed transaction to trx buffer
 *
 * Parameters :
 *
 *      rf - in out, use
 *              pointer to roll-forward object
 *
 *      p_ncommits - out, use
 *              pointer to variable where the #of committed transactions will
 *          be stored
 *
 *      p_rtrxfound - out, use
 *              pointer to variable where info whether any remote trx
 *          were detected.
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 *
 * Comments :
 *      Precondition: the roll-forward object must be in the initialized
 *      state, ie. this function must be preceded by the dbe_rollfwd_init()
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rollfwd_scancommitmarks(
        dbe_rollfwd_t* rf,
        ulong* p_ncommits,
        bool* p_rtrxfound,
        dbe_hsbstatelabel_t* p_starthsbstate)
{
        dbe_ret_t rc;
        dbe_logrectype_t logrectype;
        dbe_trxid_t trxid;
        size_t datasize;
        bool cprec_reached;
        bool atstartpos;
        dbe_trxinfo_t* trxinfo;
        dbe_cpnum_t cpnum1;
        dbe_cpnum_t cpnum2;
        SsTimeT cp_ts;
        uint ctr = 0;
        dbe_trxid_t stmttrxid;
        dbe_hsbmode_t hsbmode;         /* Initial replication mode */
        dbe_logpos_t curlogpos;
        bool ishsbg2;
        bool hsbg2_scan_only;
        dbe_logi_commitinfo_t commitinfo;
        bool standalone_mode;
        bool b;
        dbe_catchup_logpos_t commit_local_durable_logpos;

        ss_dassert(rf != NULL);
        ss_dassert(p_ncommits != NULL);
        ss_dassert(rf->rf_state == RFSTATE_START);
        ss_pprintf_1(("dbe_rollfwd_scancommitmarks\n"));

#ifdef SS_HSBG2

        rf->rf_durablecount = 0;
        rf->rf_hsbflags = HSBG2_RECOVERY_USELOCALLOGPOS;
        DBE_CATCHUP_LOGPOS_SET_NULL(rf->rf_local_durable_logpos);
        DBE_CATCHUP_LOGPOS_SET_NULL(rf->rf_remote_durable_logpos);
        DBE_CATCHUP_LOGPOS_SET_NULL(commit_local_durable_logpos);

        if (ss_migratehsbg2) {
            ishsbg2 = FALSE;
            hsbg2_scan_only = FALSE;
        } else {
            ishsbg2 = TRUE;
            if (rf->rf_hsbmode != DBE_HSB_STANDALONE) {
                hsbg2_scan_only = TRUE;
            } else {
                hsbg2_scan_only = FALSE;
            }
        }
        standalone_mode = dbe_hsbstate_isstandaloneloggingstate(*p_starthsbstate);
#else /* SS_HSBG2 */
        ishsbg2 = FALSE;
        hsbg2_scan_only = FALSE;
#endif /* SS_HSBG2 */

        hsbmode = rf->rf_hsbmode;

        cpnum1 = dbe_counter_getcpnum(rf->rf_counter) - 1;

#ifdef SS_HSBG2
        rf->rf_last_cpnum = cpnum1;
#endif /* SS_HSBG2 */

        ss_pprintf_2(("dbe_rollfwd_scancommitmarks:cpnum1=%ld, standalone_mode=%d\n", cpnum1, standalone_mode));

        rf->rf_ncommits = 0L;
#ifdef DBE_REPLICATION
        ss_dassert(p_rtrxfound != NULL);
        *p_rtrxfound = FALSE;
#endif  /* DBE_REPLICATION */

        for (cprec_reached = FALSE, atstartpos = FALSE;;) {
            if (ctr++ % 100 == 0) {
                ss_svc_notify_init();
            }

            rc = dbe_rflog_getnextrecheader(
                    rf->rf_log,
                    &logrectype,
                    &trxid,
                    &datasize);
            if (rc != DBE_RC_SUCC) {
                break;
            }
            if (logrectype != DBE_LOGREC_NOP) {
                ss_poutput_2(
                {
                    dbe_ret_t rc;
                    dbe_catchup_logpos_t lp;
                    rc = dbe_rflog_fill_catchuplogpos(rf->rf_log, &lp);
                    ss_rc_dassert(rc == DBE_RC_SUCC, rc);
                    ss_pprintf_2(("dbe_rollfwd_scancommitmarks:(%d,%s,%d,%d,%d):%s, trxid=%lu\n",
                        LOGPOS_DSDDD(lp), dbe_logi_getrectypename(logrectype), DBE_TRXID_GETLONG(trxid)));
                });
            }
            if (atstartpos) {
                atstartpos = FALSE;
                dbe_rflog_saverecordpos(rf->rf_log, &rf->rf_restartpos);
                curlogpos = rf->rf_restartpos;
#ifdef SS_HSBG2
                rf->rf_hsbflags = HSBG2_RECOVERY_USELOCALLOGPOS;
                DBE_CATCHUP_LOGPOS_SET_NULL(rf->rf_local_durable_logpos);
                DBE_CATCHUP_LOGPOS_SET_NULL(rf->rf_remote_durable_logpos);
                rf->rf_durablecount = 0;
#endif /* SS_HSBG2 */
            } else {
                dbe_rflog_saverecordpos(rf->rf_log, &curlogpos);
            }
            switch (logrectype) {
                case DBE_LOGREC_NOP:
                    break;
                case DBE_LOGREC_HEADER:
                    {
                        dbe_logfnum_t logfnum;
                        dbe_cpnum_t cpnum;
                        dbe_hdr_blocksize_t blocksize;
                        FOUR_BYTE_T dbcreatime;

                        rc = dbe_rflog_getlogheaderdata(
                                rf->rf_log,
                                &logfnum,
                                &cpnum,
                                &blocksize,
                                &dbcreatime);
                        if (rc == DBE_RC_SUCC) {
                            FAKE_IF(FAKE_DBE_IGNORELOGTIMES) {
                                dbcreatime = rf->rf_dbcreatime;
                            }
                            if (!dbe_recoverignoretimestamps && dbcreatime != rf->rf_dbcreatime) {
                                char* logfname;

                                logfname = dbe_rflog_getphysicalfname(
                                        rf->rf_log
                                );
                                su_informative_exit(
                                        __FILE__,
                                        __LINE__,
                                        DBE_ERR_WRONG_LOGFILE_S,
                                        logfname);
                            }
                        }
                        if (!cprec_reached) {
                            if (cpnum == cpnum1 + 1) {
                                cprec_reached = TRUE;
                                atstartpos = TRUE;
                            } else {
                                if (cpnum1 < cpnum) {

                                    char* logfname;

                                    logfname = dbe_rflog_getphysicalfname(
                                            rf->rf_log
                                    );
                                    su_informative_exit(
                                            __FILE__,
                                            __LINE__,
                                            DBE_ERR_WRONG_LOGFILE_S,
                                            logfname);
                                }
                            }
                        }
                    }
                    break;

                /* Tuple operations in index
                 */
                case DBE_LOGREC_INSTUPLE:
                case DBE_LOGREC_INSTUPLEWITHBLOBS:
                case DBE_LOGREC_INSTUPLENOBLOBS:
                case DBE_LOGREC_DELTUPLE:
#ifdef SS_MME
                case DBE_LOGREC_MME_INSTUPLEWITHBLOBS:
                case DBE_LOGREC_MME_INSTUPLENOBLOBS:
                case DBE_LOGREC_MME_DELTUPLE:
#endif /* SS_MME */
                case DBE_LOGREC_BLOBG2DATA:
                case DBE_LOGREC_BLOBG2DROPMEMORYREF:
                case DBE_LOGREC_BLOBG2DATACOMPLETE:
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                    rollfwd_updatemaxtrxid(rf, trxid);
                    break;

                /* BLOB operations
                */
                case DBE_LOGREC_BLOBSTART_OLD:
                case DBE_LOGREC_BLOBALLOCLIST_OLD:
                case DBE_LOGREC_BLOBALLOCLIST_CONT_OLD:
                case DBE_LOGREC_BLOBDATA_OLD:
                case DBE_LOGREC_BLOBDATA_CONT_OLD:
                    if (cprec_reached) {
                        ss_rc_error(logrectype);
                    }
                    break;
                /* Transaction marks
                 */
                case DBE_LOGREC_ABORTTRX_INFO:
                case DBE_LOGREC_ABORTTRX_OLD:
#ifdef DBE_REPLICATION
                    if (cprec_reached && !ishsbg2) {
                        dbe_trxid_t remotetrxid;

                        if (hsbmode != DBE_HSB_SECONDARY) {
                            trxinfo = dbe_trxbuf_gettrxinfo(
                                        rf->rf_trxbuf,
                                        trxid);
                            if (trxinfo != NULL) {
                                (void)dbe_trxinfo_setaborted(trxinfo);
                                trxinfo->ti_committrxnum =
                                    dbe_counter_getnewcommittrxnum(rf->rf_counter);
                            }
                            remotetrxid = dbe_rtrxbuf_remotebylocaltrxid(
                                                rf->rf_rtrxbuf, trxid);
                            if (DBE_TRXID_NOTNULL(remotetrxid)) {
                                dbe_rtrxbuf_deletebylocaltrxid(rf->rf_rtrxbuf, trxid);
                            }
                            ss_pprintf_2(("remotetrxid=%ld,trxid=%ld\n",
                                           DBE_TRXID_GETLONG(remotetrxid),
                                           DBE_TRXID_GETLONG(trxid)
                                        ));
                        } else {
                            ss_pprintf_2(("secondary server, skip abort, trxid=%ld\n",
                                           DBE_TRXID_GETLONG(trxid)
                                        ));
                        }
                    }
#endif /* DBE_REPLICATION */
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                    rollfwd_updatemaxtrxid(rf, trxid);
                    break;
                case DBE_LOGREC_ABORTSTMT:
                    if (!hsbg2_scan_only) {
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL) {
                            ss_pprintf_2(("trxid=%ld,stmttrxid=%ld\n",
                                        DBE_TRXID_GETLONG(trxinfo->ti_usertrxid),
                                        DBE_TRXID_GETLONG(trxid)
                                        ));
                            dbe_trxbuf_abortstmt(
                                rf->rf_trxbuf, 
                                dbe_counter_getcommittrxnum(rf->rf_counter),
                                trxid);
                            rollfwd_updatemaxtrxid(rf, trxid);
                        }
                    }
#ifdef DBE_REPLICATION
                    if (!ishsbg2) {
                        dbe_trxid_t remotetrxid;
                        remotetrxid = dbe_rtrxbuf_remotebylocaltrxid(
                                            rf->rf_rtrxbuf, trxid);
                        if (DBE_TRXID_NOTNULL(remotetrxid)) {
                            dbe_rtrxbuf_deletebylocaltrxid(rf->rf_rtrxbuf, trxid);
                        }
                    }
#endif /* DBE_REPLICATION */
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                    rollfwd_updatemaxtrxid(rf, trxid);
                    break;

                case DBE_LOGREC_COMMITTRX_INFO:
                case DBE_LOGREC_COMMITTRX_OLD:
                case DBE_LOGREC_COMMITTRX_HSB_OLD:
                case DBE_LOGREC_HSBCOMMITMARK_OLD:
                    if (cprec_reached) {
                        ss_pprintf_2(("commit this trx\n"));
                        if (logrectype == DBE_LOGREC_COMMITTRX_INFO) {
                            rc = dbe_rflog_getcommitinfo(rf->rf_log, &commitinfo);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                        } else if (logrectype == DBE_LOGREC_COMMITTRX_HSB_OLD) {
                            commitinfo = DBE_LOGI_COMMIT_HSBPRIPHASE2|DBE_LOGI_COMMIT_LOCAL;
                        } else if (logrectype == DBE_LOGREC_HSBCOMMITMARK_OLD) {
                            commitinfo = DBE_LOGI_COMMIT_HSBPRIPHASE1|DBE_LOGI_COMMIT_LOCAL;
                        } else {
                            ss_dassert(logrectype == DBE_LOGREC_COMMITTRX_OLD);
                            commitinfo = DBE_LOGI_COMMIT_LOCAL;
                        }
                        if (SU_BFLAG_TEST(commitinfo, DBE_LOGI_COMMIT_HSBPRIPHASE1)
                        &&  !ishsbg2)
                        {
                            ss_dprintf_2(("add trxid=%ld to rf_hsbcommitlist\n",
                                           DBE_TRXID_GETLONG(trxid)));
                            su_list_insertlast(rf->rf_hsbcommitlist,
                                               (void*)DBE_TRXID_GETLONG(trxid));
                            break;
                        }
                        if (SU_BFLAG_TEST(commitinfo, DBE_LOGI_COMMIT_HSBPRIPHASE1)
                        ||  !SU_BFLAG_TEST(commitinfo, DBE_LOGI_COMMIT_HSBPRIPHASE1|DBE_LOGI_COMMIT_HSBPRIPHASE2))
                        {
                            /* Count phase1 commits and no-phase commits. */
                            rf->rf_ncommits++;
                        }
                        b = dbe_logi_commitinfo_iscommit(commitinfo);
                        if (b) {
                            rc = dbe_rflog_fill_catchuplogpos(rf->rf_log, &commit_local_durable_logpos);
                            if (rc != SU_SUCCESS) {
                                break;
                            }
                            ss_dprintf_1(("dbe_rollfwd_scancommitmarks:commit_local_durable_logpos (%d,%s,%d,%d,%d)\n",
                                            LOGPOS_DSDDD(commit_local_durable_logpos)));
                        }

                        if (!hsbg2_scan_only && b) {
                            trxinfo = dbe_trxbuf_gettrxinfo(
                                        rf->rf_trxbuf,
                                        trxid);
                            if (trxinfo == NULL) {
                                ss_pprintf_2(("create new trxinfo\n"));
                                trxinfo = rollfwd_trxinfo_init(rf, trxid);
                            }
                            ss_dassert(trxinfo != NULL);
#ifdef DBE_REPLICATION
                            if (SU_BFLAG_TEST(commitinfo, DBE_LOGI_COMMIT_HSBPRIPHASE2)
                            &&  !ishsbg2)
                            {
                                if (hsbmode == DBE_HSB_SECONDARY) {
                                    dbe_trxid_t remotetrxid;

                                    ss_pprintf_2(("hsb secondary\n"));
                                    remotetrxid = dbe_rtrxbuf_remotebylocaltrxid(
                                                        rf->rf_rtrxbuf, trxid);
                                    if (DBE_TRXID_NOTNULL(remotetrxid)) {
                                        rollfwd_updatereptrxidlast(rf, remotetrxid, TRUE);
                                    }
                                    ss_pprintf_2(("remotetrxid=%ld,trxid=%ld\n",
                                        DBE_TRXID_GETLONG(remotetrxid), DBE_TRXID_GETLONG(trxid)));
                                } else {
                                    su_list_node_t* n;
                                    void* hsbcommitmarkid;
                                    bool found = FALSE;

                                    ss_pprintf_2(("hsb primary\n"));
                                    su_list_do_get(rf->rf_hsbcommitlist, n, hsbcommitmarkid) {
                                        if ((long)hsbcommitmarkid ==
                                                DBE_TRXID_GETLONG(trxid)) {
                                            found = TRUE;
                                            break;
                                        }
                                    }
                                    if (found) {
                                        /* remove those trxs only have hsbcommitmark
                                        * but no commit before this trx.
                                        * those trxs should be aborted.
                                        */
                                        while (su_list_getfirst(
                                                    rf->rf_hsbcommitlist) !=
                                                (void*)DBE_TRXID_GETLONG(trxid)) {
                                            ss_dprintf_2(("remove trxid=%ld from rf_hsbcommitlist\n", su_list_getfirst(rf->rf_hsbcommitlist)));
                                            ss_dassert(su_list_length(
                                                rf->rf_hsbcommitlist) > 0);
                                            su_list_removefirst(
                                                rf->rf_hsbcommitlist);
                                        }
                                        ss_dprintf_2(("remove trxid=%ld from rf_hsbcommitlist\n", su_list_getfirst(rf->rf_hsbcommitlist)));
                                        su_list_removefirst(rf->rf_hsbcommitlist);
                                        rollfwd_updatereptrxidlast(rf, trxid, TRUE);
                                    } else {
                                        /* hsbcommitmark is log before last ckpt,
                                        * do not roll-forward to the hsbcommitmark
                                        * of this trx, only update lasttrxid
                                        */
                                        rollfwd_updatereptrxidlast(rf, trxid, FALSE);
                                    }
                                }
                            }
#endif /* DBE_REPLICATION */
                            (void)dbe_trxinfo_setcommitted(trxinfo);
                            trxinfo->ti_committrxnum =
                                dbe_counter_getnewcommittrxnum(rf->rf_counter);
                            rf->rf_committrxnum = trxinfo->ti_committrxnum;
                        }
                    }
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                    rollfwd_updatemaxtrxid(rf, trxid);
                    break;
                case DBE_LOGREC_COMMITSTMT:
                    rollfwd_updatemaxtrxid(rf, trxid);
                    rc = dbe_rflog_getcommitstmt(
                            rf->rf_log,
                            &stmttrxid);
                    if (rc == DBE_RC_SUCC) {
                        rollfwd_updatemaxtrxid(rf, stmttrxid);
                    } else if (rc != DBE_RC_SUCC && cprec_reached) {
                        break;
                    }
                    if (cprec_reached) {
#ifdef DBE_REPLICATION
                        if (!ishsbg2) {
                            dbe_trxid_t rtrxid = dbe_rtrxbuf_remotebylocaltrxid(rf->rf_rtrxbuf, trxid);
                            ss_pprintf_2(("trxid=%lu,stmttrxid=%lu\n",
                                        DBE_TRXID_GETLONG(trxid),
                                        DBE_TRXID_GETLONG(stmttrxid)));

                            if (DBE_TRXID_NOTNULL(rtrxid)) {
                                ss_debug(dbe_trxid_t rstmttrxid =
                                        dbe_rtrxbuf_remotebylocaltrxid(rf->rf_rtrxbuf,stmttrxid);)
                                ss_dassert(DBE_TRXID_NOTNULL(rstmttrxid));
                                break;
                            }
                        }
#endif /* DBE_REPLICATION */
                        if (!hsbg2_scan_only) {
                            trxinfo = dbe_trxbuf_gettrxinfo(
                                        rf->rf_trxbuf,
                                        trxid);
                            if (trxinfo == NULL) {
                                trxinfo = rollfwd_trxinfo_init(rf, trxid);
                            }
                            if (!DBE_TRXID_EQUAL(stmttrxid, trxid)
                            &&  dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, stmttrxid)
                                == NULL) {
                                dbe_trxbuf_addstmt(
                                    rf->rf_trxbuf,
                                    stmttrxid,
                                    trxinfo);
                            }
                        }
                    }
                    break;
                case DBE_LOGREC_PREPARETRX:
                    /* not implemented !!!!!!!!!! */
                    rc = DBE_ERR_LOGFILE_CORRUPT;
                    break;

                /* Global markers
                 */
                case DBE_LOGREC_CHECKPOINT_NEW:
                case DBE_LOGREC_SNAPSHOT_NEW:
                    if (!cprec_reached) {
                        rc = dbe_rflog_getcpmarkdata_new(
                                rf->rf_log,
                                &cpnum2,
                                &cp_ts);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        FAKE_IF(FAKE_DBE_IGNORELOGTIMES) {
                            cp_ts = rf->rf_cptimestamp;
                        }
                        if (cpnum2 == cpnum1 && (cp_ts == rf->rf_cptimestamp || dbe_recoverignoretimestamps)) {
                            cprec_reached = TRUE;
                            atstartpos = TRUE;
                        } else {
                            ss_dprintf_1(("be_rollfwd_scancommitmarks:cpnum1=%d,cpnum2=%d,cp_ts=%ld,rf->rf_cptimestamp=%ld\n",
                              cpnum1, cpnum2, cp_ts, rf->rf_cptimestamp));
                            ss_dassert(cpnum2 <= cpnum1);
                        }
                    }
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;
                case DBE_LOGREC_DELSNAPSHOT:
                    break;

                /* Data dictionary operations
                 */
                case DBE_LOGREC_CREATETABLE_FULLYQUALIFIED:
                case DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED:
                case DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED:
                case DBE_LOGREC_CREATETABLE:
                case DBE_LOGREC_CREATETABLE_NEW:
                case DBE_LOGREC_CREATEINDEX:
                case DBE_LOGREC_CREATEVIEW:
                case DBE_LOGREC_CREATEVIEW_NEW:
                case DBE_LOGREC_DROPTABLE:
                case DBE_LOGREC_TRUNCATETABLE:
                case DBE_LOGREC_TRUNCATECARDIN:
                case DBE_LOGREC_DROPINDEX:
                case DBE_LOGREC_DROPVIEW:
                case DBE_LOGREC_ALTERTABLE:
                case DBE_LOGREC_CREATESEQ:
                case DBE_LOGREC_CREATECTR:
                case DBE_LOGREC_DROPCTR:
                case DBE_LOGREC_DROPSEQ:
                case DBE_LOGREC_RENAMETABLE:
                case DBE_LOGREC_AUDITINFO:
                case DBE_LOGREC_CREATEUSER:
                    /* FALLTHROUGH */
                /* Dense (transaction-bound) sequence operations
                 */
                case DBE_LOGREC_SETSEQ:
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                    rollfwd_updatemaxtrxid(rf, trxid);
                    break;
                case DBE_LOGREC_INCSYSCTR:
                case DBE_LOGREC_SETHSBSYSCTR:
                case DBE_LOGREC_INCCTR:
                case DBE_LOGREC_SETCTR:
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;
                case DBE_LOGREC_SWITCHTOPRIMARY:
#ifdef DBE_REPLICATION
                    if (cprec_reached) {
                        ss_pprintf_1(("DBE_LOGREC_SWITCHTOPRIMARY\n"));
                        rollfwd_hsbswitchreset(rf);
                        hsbmode = DBE_HSB_PRIMARY;
                        rf->rf_hsbstartpos = curlogpos;
                    }
#endif /* DBE_REPLICATION */
                    break;
                case DBE_LOGREC_SWITCHTOSECONDARY:
#ifdef DBE_REPLICATION
                    if (cprec_reached) {
                        ss_pprintf_1(("DBE_LOGREC_SWITCHTOSECONDARY\n"));
                        *p_rtrxfound = FALSE;
                        rollfwd_hsbswitchreset(rf);
                        hsbmode = DBE_HSB_SECONDARY;
                        rf->rf_hsbstartpos = curlogpos;
                    }
#endif /* DBE_REPLICATION */
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;
                case DBE_LOGREC_SWITCHTOSECONDARY_NORESET:
#ifdef DBE_REPLICATION
                    if (cprec_reached) {
                        ss_pprintf_1(("DBE_LOGREC_SWITCHTOSECONDARY\n"));
                        *p_rtrxfound = FALSE;
                        hsbmode = DBE_HSB_SECONDARY;
                        rf->rf_hsbstartpos = curlogpos;
                    }
#endif /* DBE_REPLICATION */
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;

#ifndef SS_MYSQL
                case DBE_LOGREC_CLEANUPMAPPING:
                    if (cprec_reached && !ishsbg2) {
                        dbe_hsb_deletealldummybylocal(rf->rf_rtrxbuf);
                    }
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;
#endif /* !SS_MYSQL */

                case DBE_LOGREC_REPLICATRXSTART :
#ifdef DBE_REPLICATION
                    if (cprec_reached && !ishsbg2) {
                        dbe_trxid_t remotetrxid;
                        bool isdummy;

                        rc = dbe_rflog_getreplicatrxstart(
                                rf->rf_log,
                                &remotetrxid);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        if (DBE_TRXID_ISNULL(remotetrxid)) {
                            isdummy = TRUE;
                            remotetrxid = trxid;
                        } else {
                            isdummy = FALSE;
                        }
                        trxinfo = dbe_trxbuf_gettrxinfo(
                                    rf->rf_trxbuf,
                                    trxid);
                        if (trxinfo == NULL) {
                            trxinfo = rollfwd_trxinfo_init(rf, trxid);
                        } else {
                            ss_dassert(DBE_TRXID_EQUAL(trxinfo->ti_usertrxid, trxid));
                            ss_dassert(dbe_trxinfo_isbegin(trxinfo));
                        }
                        ss_dassert(trxinfo != NULL);
                        ss_pprintf_2(("remotetrxid=%ld,trxid=%ld\n",
                                       DBE_TRXID_GETLONG(remotetrxid),
                                       DBE_TRXID_GETLONG(trxid)
                                    ));
                        rc = dbe_rtrxbuf_add(
                                rf->rf_rtrxbuf,
                                remotetrxid,
                                trxid,
                                NULL,
                                isdummy);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        *p_rtrxfound = TRUE;
                    }
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                    rollfwd_updatemaxtrxid(rf, trxid);
#endif /* DBE_REPLICATION */
                    break;
                case DBE_LOGREC_REPLICASTMTSTART:
#ifdef DBE_REPLICATION
                    if (cprec_reached && !ishsbg2) {
                        dbe_trxid_t localstmtid;
                        dbe_trxid_t remotestmtid;
                        bool isdummy;

                        rc = dbe_rflog_getreplicastmtstart(
                                rf->rf_log,
                                &remotestmtid,
                                &localstmtid);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        if (DBE_TRXID_ISNULL(remotestmtid)) {
                            isdummy = TRUE;
                            remotestmtid = localstmtid;
                        } else {
                            isdummy = FALSE;
                        }
                        trxinfo = dbe_trxbuf_gettrxinfo(
                                    rf->rf_trxbuf,
                                    trxid);
                        if (trxinfo == NULL) {
                            /* Jarmo removed derror Jan 16, 2001
                             * it is possible to have NULL trxinfo if there
                             * has been only aborted statements in the
                             * transaction.
                             * ss_derror;
                             */
                            trxinfo = rollfwd_trxinfo_init(rf, trxid);
                        }
                        if (!DBE_TRXID_EQUAL(localstmtid, trxid)
                        &&  dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, localstmtid)
                            == NULL)
                        {
                            dbe_trxbuf_addstmt(
                                rf->rf_trxbuf,
                                localstmtid,
                                trxinfo);
                        }
                        ss_pprintf_2(("trxid=%ld,remotestmtid=%ld,stmtid=%ld\n",
                            DBE_TRXID_GETLONG(trxid),
                            DBE_TRXID_GETLONG(remotestmtid),
                            DBE_TRXID_GETLONG(localstmtid)));
                        if (!DBE_TRXID_EQUAL(localstmtid, trxid)) {
                            if (isdummy
                            &&  DBE_TRXID_ISNULL(
                                    dbe_rtrxbuf_remotebylocaltrxid(
                                        rf->rf_rtrxbuf,
                                        trxid)))
                            {
                                /* Add also trxid to the rtrxbuf in
                                 * case it not yet added.
                                 */
                                dbe_rtrxbuf_add(
                                    rf->rf_rtrxbuf,
                                    trxid,
                                    trxid,
                                    NULL,
                                    isdummy);
                            }
                            rc = dbe_rtrxbuf_add(
                                rf->rf_rtrxbuf,
                                remotestmtid,
                                localstmtid,
                                NULL,
                                isdummy);
                        }
                        rollfwd_updatemaxtrxid(rf, localstmtid);
                        *p_rtrxfound = TRUE;
                    }
                    rollfwd_updatemaxtrxid(rf, trxid);
#endif /* DBE_REPLICATION */
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;

#ifdef SS_HSBG2

                case DBE_LOGREC_HSBG2_DURABLE:
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;
                case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
                case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
                    {
                        dbe_catchup_logpos_t local_durable_logpos;
                        dbe_catchup_logpos_t remote_durable_logpos;

                        rc = dbe_rflog_get_remote_durable(
                                    rf->rf_log,
                                    &local_durable_logpos,
                                    &remote_durable_logpos);

                        ss_dprintf_1(("dbe_rollfwd_scancommitmarks:rf_local_durable_logpos (%d,%s,%d,%d,%d) rf_remote_durable_logpos (%d,%s,%d,%d,%d)\n",
                                        LOGPOS_DSDDD(local_durable_logpos), LOGPOS_DSDDD(remote_durable_logpos)));
                        if (rc == SU_SUCCESS && dbe_catchup_logpos_cmp(local_durable_logpos, rf->rf_local_durable_logpos) >= 0) {
                            ss_dprintf_2(("dbe_rollfwd_scancommitmarks:set new local pos\n"));
                            rf->rf_local_durable_logpos  = local_durable_logpos;
                            rf->rf_remote_durable_logpos = remote_durable_logpos;
                            if (logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK) {
                                rf->rf_hsbflags = HSBG2_RECOVERY_USELOCALLOGPOS;
                            } else {
                                dbe_catchup_savedlogpos_t* savedlogpos;
                                savedlogpos = dbe_catchup_savedlogpos_init(&remote_durable_logpos, TRUE);
                                dbe_catchup_savedlogpos_setpos(
                                    savedlogpos, 
                                    local_durable_logpos.lp_logfnum,
                                    local_durable_logpos.lp_daddr,
                                    local_durable_logpos.lp_bufpos);
                                su_list_insertlast(rf->rf_savedlogposlist, savedlogpos);
                                rf->rf_hsbflags = 0;
                            }
                        }
                    }
                    /* Durable mark is no more trxid.
                     * It is logpos to originators log.
                     *
                     * rollfwd_updatemaxtrxid(rf, trxid);
                     */
                    rf->rf_durablecount++;
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;

#endif /* SS_HSBG2 */

                case DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT:
                case DBE_LOGREC_HSBG2_NEW_PRIMARY:
                case DBE_LOGREC_COMMENT:
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;

                case DBE_LOGREC_HSBG2_NEWSTATE:
                    rc = dbe_rflog_get_hsbnewstate(rf->rf_log, p_starthsbstate);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }

                    standalone_mode = dbe_hsbstate_isstandaloneloggingstate(*p_starthsbstate);

                    ss_dprintf_2(("dbe_rollfwd_scancommitmarks:DBE_LOGREC_HSBG2_NEWSTATE:newstate = %s (%d), standalone_mode=%d\n",
                        dbe_hsbstate_getstatestring(*p_starthsbstate), *p_starthsbstate, standalone_mode));
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;

                case DBE_LOGREC_HSBG2_ABORTALL:
                    rc = dbe_rflog_skip_unscanned_data(rf->rf_log);
                    break;

                case DBE_LOGREC_CHECKPOINT_OLD:
                case DBE_LOGREC_SNAPSHOT_OLD:
                default:
                    ss_rc_error(logrectype);
                    break;
            }
            if (rc != DBE_RC_SUCC) {
                break;
            }
        }
        if (rc == DBE_RC_END || rc == DBE_ERR_LOGFILE_CORRUPT) {
            if (rc == DBE_ERR_LOGFILE_CORRUPT) {
                rf->rf_iscorrupt = TRUE;
                dbe_rflog_saverecordpos(rf->rf_log, &rf->rf_corruptpos);
                if (cprec_reached) {
                    rc = DBE_RC_LOGFILE_TAIL_CORRUPT;
                }
                dbe_rflog_cleartoeof(rf->rf_log, &rf->rf_corruptpos);
            } else {
                dbe_rflog_saverecordpos(rf->rf_log, &rf->rf_corruptpos);
                dbe_rflog_cleartoeof(rf->rf_log, &rf->rf_corruptpos);
                rc = DBE_RC_SUCC;
            }
            if (cprec_reached && !atstartpos) {
                dbe_rflog_restorerecordpos(rf->rf_log, &rf->rf_restartpos);
            }
            if (standalone_mode && hsbg2_scan_only) {
                /* Last mode was standalone mode so we need to use last commit logpos
                 * instead of last remote durable ack position.
                 */
                ss_dprintf_2(("dbe_rollfwd_scancommitmarks:last mode is standalone_mode, use last commit position\n"));
                rf->rf_hsbflags = HSBG2_RECOVERY_GENERATENEWID|HSBG2_RECOVERY_USELOCALLOGPOS;
                rf->rf_local_durable_logpos = commit_local_durable_logpos;
                DBE_CATCHUP_LOGPOS_SET_NULL(rf->rf_remote_durable_logpos);
                rf->rf_durablecount++;
            }
        }
        rf->rf_state = RFSTATE_SCANNED;
        rf->rf_rtrxfound = *p_rtrxfound;
        *p_ncommits = rf->rf_ncommits;
        return (rc);
}


static rs_relh_t* rollfwd_relhbyid(dbe_rollfwd_t* rf, ulong relid, dbe_trxid_t stmttrxid, rs_entname_t** p_entname)
{
        bool succp;
        bool retry;
        rs_relh_t* relh = NULL;
        rs_entname_t* relentname;
        void* ctx = NULL;

        if (!DBE_TRXID_ISNULL(stmttrxid)) {
            ctx = dbe_trxbuf_disablestmt(rf->rf_trxbuf, stmttrxid);
        }

        if (p_entname != NULL) {
            *p_entname = NULL;
        }
        for (retry = FALSE; ;retry = TRUE) {
            if (retry) {
                (*rf->rf_recovcallback->rc_refreshrbuffun)(rf->rf_recovcallback->rc_rbufctx);
            }
            succp = rs_rbuf_relnamebyid(rf->rf_cd, rf->rf_rbuf, relid, &relentname);
            if (!succp) {
                if (retry) {
                    /* already made a retry after refresh */
                    relh = (*rf->rf_recovcallback->rc_getrelhfun_byrelid)(
                                rf->rf_recovcallback->rc_rbufctx, 
                                relid);
                    if (relh == NULL) {
                        ss_debug(rs_rbuf_print(rf->rf_cd, rf->rf_rbuf));
                        su_informative_exit(
                                __FILE__,
                                __LINE__,
                                DBE_ERR_RELIDNOTFOUND_D,
                                relid);
                    } else {
                        /* Found relh. */
                        relentname = rs_entname_copy(rs_relh_entname(rf->rf_cd, relh));
                    }
                } else {
                    continue;
                }
            }
            if (relh == NULL) {
                relh = (*rf->rf_recovcallback->rc_getrelhfun)(
                            rf->rf_recovcallback->rc_rbufctx, 
                            relentname, 
                            NULL);
            }
            if (relh == NULL) {
                if (retry) {
                    /* already made a retry after refresh */
                    su_informative_exit(
                            __FILE__,
                            __LINE__,
                            DBE_ERR_RELNAMENOTFOUND_S,
                            rs_entname_getname(relentname)
                    );
                } else {
                    continue;
                }
            }
            break;
        }
        if (p_entname != NULL) {
            *p_entname = relentname;
        }
        if (!DBE_TRXID_ISNULL(stmttrxid)) {
            dbe_trxbuf_enablestmt(rf->rf_trxbuf, ctx, stmttrxid);
        }

        return (relh);
}

static rs_relh_t* rollfwd_truncate_relhbyid(
        dbe_rollfwd_t* rf, 
        ulong relid, 
        dbe_trxid_t stmttrxid,
        char* relname)
{
        void* ctx = NULL;
        rs_relh_t* relh;

        if (!DBE_TRXID_ISNULL(stmttrxid)) {
            ctx = dbe_trxbuf_disablestmt(rf->rf_trxbuf, stmttrxid);
        }

        relh = (*rf->rf_recovcallback->rc_getrelhfun_byrelid)(
                    rf->rf_recovcallback->rc_rbufctx, 
                    relid);

        if (!DBE_TRXID_ISNULL(stmttrxid)) {
            dbe_trxbuf_enablestmt(rf->rf_trxbuf, ctx, stmttrxid);
        }

        if (relh == NULL) {
            su_informative_exit(
                __FILE__,
                __LINE__,
                DBE_ERR_RELNAMENOTFOUND_S,
                relname);
        }

        (*rf->rf_recovcallback->rc_reloadrbuffun)(rf->rf_recovcallback->rc_rbufctx);

        return(relh);
}

rs_entname_t* rollfwd_relnamebyid(dbe_rollfwd_t* rf, ulong relid)
{
        bool succp;
        bool retry;
        rs_entname_t* relentname;

        for (retry = FALSE; ;retry = TRUE) {
            if (retry) {
                (*rf->rf_recovcallback->rc_refreshrbuffun)(rf->rf_recovcallback->rc_rbufctx);
            }
            succp = rs_rbuf_relnamebyid(rf->rf_cd, rf->rf_rbuf, relid, &relentname);
            if (!succp) {
                if (retry) {
                    /* already made a retry after refresh */
                su_informative_exit(
                            __FILE__,
                            __LINE__,
                            DBE_ERR_RELIDNOTFOUND_D,
                            relid);
                } else {
                    continue;
                }
            }
            break;
        }
        return (relentname);
}

rs_entname_t* rollfwd_viewnamebyid(dbe_rollfwd_t* rf, ulong relid)
{
        bool succp;
        bool retry;
        rs_entname_t* relentname;

        for (retry = FALSE; ;retry = TRUE) {
            if (retry) {
                (*rf->rf_recovcallback->rc_refreshrbuffun)(rf->rf_recovcallback->rc_rbufctx);
            }
            succp = rs_rbuf_viewnamebyid(rf->rf_cd, rf->rf_rbuf, relid, &relentname);
            if (!succp) {
                if (retry) {
                    /* already made a retry after refresh */
                su_informative_exit(
                            __FILE__,
                            __LINE__,
                            DBE_ERR_RELIDNOTFOUND_D,
                            relid);
                } else {
                    continue;
                }
            }
            break;
        }
        return (relentname);
}

/*##**********************************************************************\
 *
 *              dbe_rollfwd_recover
 *
 * Recovers from the saved log position (the position has been saved by
 * dbe_rollfwd_scancommitmarks()).
 *
 * Parameters :
 *
 *      rf - in out, use
 *              pointer to roll-forward object
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 *
 * Comments :
 *      Precondition: this routine must run only once and after the
 *      dbe_rollfwd_scancommitmarks.
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rollfwd_recover(
        dbe_rollfwd_t* rf)
{
        dbe_ret_t rc;
        dbe_logrectype_t logrectype;
        dbe_trxid_t trxid;
        size_t datasize;
        dbe_trxinfo_t* trxinfo;
        ulong relid;
        char* relname;
        char* relschema;
        long userid;
        char* info;
        rs_entname_t* relentname;
        ulong keyid;
        bool succp;
        bool hasblobs;
        vtpl_t* p_vtpl;
        rs_relh_t* relh;
        rs_ano_t nkeys;
        rs_ano_t nattrs;
        uint ctr = 0;
#ifdef DBE_REPLICATION
        bool remove_rest;
        bool any_removed = FALSE;
#endif /* DBE_REPLICATION */
        dbe_logpos_t curlogpos;
        bool hsbrecovery = FALSE;
        bool ishsbg2;
        dbe_logi_commitinfo_t commitinfo;
        bool recheader_gotten = FALSE;

        ss_dassert(rf != NULL);
        ss_dassert(rf->rf_state == RFSTATE_SCANNED);
        ss_pprintf_1(("dbe_rollfwd_recover\n"));
        SS_PUSHNAME("dbe_rollfwd_recover");

        dbe_db_enteraction(rs_sysi_db(rf->rf_cd), rf->rf_cd);

#ifdef SS_HSBG2
        /* ss_assert(!ss_migratehsbg2); */ /* can we recover and migrate? */
        if (rf->rf_hsbmode != DBE_HSB_STANDALONE) {
            dbe_ret_t rc;
            ss_pprintf_2(("dbe_rollfwd_recover:call dbe_hsbg2_sec_opscan_recovery\n"));
            rc = dbe_hsbg2_sec_opscan_recovery(
                    rf->rf_recovcallback->rc_rbufctx,
                    rf->rf_log,
                    rf->rf_durablecount,
                    rf->rf_last_cpnum,
                    rf->rf_hsbflags,
                    rf->rf_local_durable_logpos,
                    rf->rf_remote_durable_logpos,
                    rf->rf_savedlogposlist);
            SS_POPNAME;

            dbe_db_exitaction(rs_sysi_db(rf->rf_cd), rf->rf_cd);

            ss_pprintf_2(("dbe_rollfwd_recover:return, rc=%s (%d)\n",
                su_rc_nameof(rc), (int)rc));
            return(rc);
        }
#endif /* SS_HSBG2 */

#ifdef SS_HSBG2
        ishsbg2 = TRUE;
#else
        ishsbg2 = FALSE;
#endif /* SS_HSBG2 */

        rs_rbuf_setrecovery(
            rf->rf_cd,
            rf->rf_rbuf,
            TRUE);

        if (rf->rf_hsbcommitfound) {
            /* There is HSB commit mark, remove_rest is set when last HSB
             * commit is found.
             */
            remove_rest = FALSE;
        } else {
            /* No HSB commit mark in log, remove all HSB operations.
             */
            remove_rest = TRUE;
        }

        relname = NULL;
        relschema = NULL;
        relentname = NULL;

        if (dbe_cfg_mergecleanup) {
            dbe_gtrs_mergecleanup_recovery = TRUE;
        }

        /* update the read & merge levels
         */
        if (!DBE_TRXNUM_EQUAL(rf->rf_committrxnum,DBE_TRXNUM_NULL)) {
            dbe_counter_setmaxtrxnum(
                rf->rf_counter, 
                rf->rf_committrxnum);
            if (!dbe_gtrs_mergecleanup_recovery) {
                ss_dprintf_2(("dbe_rollfwd_recover:!dbe_cfg_mergecleanup, mergetrxnum=%ld\n", DBE_TRXNUM_GETLONG(rf->rf_committrxnum)));
                dbe_counter_setmergetrxnum(
                    rf->rf_counter,
                    rf->rf_committrxnum);
            }
        }
        /* update the trxid counter
         */
        if (!DBE_TRXID_EQUAL(rf->rf_maxtrxid, DBE_TRXID_NULL)) {
            dbe_counter_settrxid(
                rf->rf_counter,
                DBE_TRXID_SUM(rf->rf_maxtrxid, 1));
        }

        if (rf->rf_ncommits != 0L || rf->rf_rtrxfound) {
            for (;;) {
                if (ctr++ % 100 == 0) {
                    ss_svc_notify_init();
                }
                recheader_gotten = FALSE;
                rc = dbe_rflog_getnextrecheader(
                        rf->rf_log,
                        &logrectype,
                        &trxid,
                        &datasize);
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                recheader_gotten = TRUE;
                if (logrectype != DBE_LOGREC_NOP) {
                    ss_pprintf_2(("dbe_rollfwd_recover:%s, trxid=%ld\n",
                        dbe_logi_getrectypename(logrectype), DBE_TRXID_GETLONG(trxid)));
                }
                dbe_rflog_saverecordpos(rf->rf_log, &curlogpos);

                hsbrecovery = dbe_logpos_cmp(&curlogpos, &rf->rf_hsbstartpos)
                              >= 0;

                switch (logrectype) {
                    case DBE_LOGREC_NOP:
                    case DBE_LOGREC_HEADER:
                        break;

                    /* Tuple operations in index
                    */
                    case DBE_LOGREC_INSTUPLE:
                    case DBE_LOGREC_INSTUPLEWITHBLOBS:
                    case DBE_LOGREC_INSTUPLENOBLOBS:
                        /* 'Get' new tuple number and ignore it to
                        * keep uniqueness of tuple number.
                        * Do the same thing for the tuple version counter
                        * as well (for similar reasons)
                        */
                        (void)dbe_counter_getnewtuplenum(rf->rf_counter);
                        (void)dbe_counter_getnewtupleversion(rf->rf_counter);

                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL
                        &&  (dbe_trxinfo_iscommitted(trxinfo)
#ifdef DBE_REPLICATION
                            || (rf->rf_hsbmode != DBE_HSB_STANDALONE
                                && !dbe_trxinfo_isaborted(trxinfo)
                                && !remove_rest
                                && !ishsbg2)
#endif /* DBE_REPLICATION */
                            ))
                        {
                            ss_pprintf_4(("  Insert this tuple\n"));
                            ss_dassert(dbe_trxinfo_iscommitted(trxinfo) ||
                                       dbe_logpos_cmp(
                                            &curlogpos,
                                            &rf->rf_hsbstartpos)
                                       > 0)
                            rc = dbe_rflog_getvtupleref(
                                    rf->rf_log,
                                    &p_vtpl,
                                    &relid);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            relh = rollfwd_relhbyid(rf, relid, DBE_TRXID_NULL, &relentname);
                            if (logrectype == DBE_LOGREC_INSTUPLE) {
                                hasblobs = rollfwd_vtplhasblobs(rf, p_vtpl, relh);
                            } else {
                                hasblobs =
                                    (logrectype == DBE_LOGREC_INSTUPLEWITHBLOBS);
                            }
                            rc = dbe_tuple_recovinsert(
                                    rf->rf_cd,
                                    rf->rf_trx,
                                    trxinfo->ti_committrxnum,
                                    trxid,
                                    relh,
                                    p_vtpl,
                                    hasblobs);
                            if (rc != DBE_RC_SUCC) {
                                su_rc_error(rc);
                            }
                            if (hasblobs) {
                                ss_pprintf_4(("vtuple has blobs, relid=%ld\n", relid));
                                rollfwd_closeblobs(
                                    rf,
                                    p_vtpl);
                            } else {
                                ss_pprintf_4(("vtuple has no blobs, relid=%ld\n", relid));
                            }
                            SS_MEM_SETUNLINK(relh);
                            rs_relh_done(rf->rf_cd, relh);
                        }
                        break;
                    case DBE_LOGREC_DELTUPLE:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL) {
#ifdef DBE_REPLICATION
                            bool unsure_replicatrx = FALSE;
#endif /* DBE_REPLICATION */
                            if (dbe_trxinfo_iscommitted(trxinfo)
#ifdef DBE_REPLICATION
                            ||  (unsure_replicatrx =
                                 (rf->rf_hsbmode != DBE_HSB_STANDALONE
                                  && !dbe_trxinfo_isaborted(trxinfo)
                                  && !remove_rest
                                  && !ishsbg2))
#endif /* DBE_REPLICATION */
                            ) {
                                ss_dassert(dbe_trxinfo_iscommitted(trxinfo) ||
                                           dbe_logpos_cmp(
                                                &curlogpos,
                                                &rf->rf_hsbstartpos)
                                           > 0)
                                rc = dbe_rflog_getvtupleref(
                                        rf->rf_log,
                                        &p_vtpl,
                                        &relid);
                                if (rc != DBE_RC_SUCC) {
                                    break;
                                }
                                ss_pprintf_4(("  Delete this tuple, relid=%ld\n", relid));
                                relh = rollfwd_relhbyid(rf, relid, DBE_TRXID_NULL, &relentname);
                                rc = dbe_tuple_recovdelete(
                                        rf->rf_cd,
                                        rf->rf_trx,
                                        trxinfo->ti_committrxnum,
                                        trxid,
                                        relh,
                                        p_vtpl,
                                        unsure_replicatrx);
                                if (rc != DBE_RC_SUCC) {
#ifdef DBE_REPLICATION
                                    if (unsure_replicatrx && !ishsbg2) {
                                        (void)dbe_trxinfo_settobeaborted(trxinfo);
                                        rc = DBE_RC_SUCC;
                                    } else
#endif /* DBE_REPLICATION */
                                    {
                                        su_rc_error(rc);
                                    }
                                }
                                SS_MEM_SETUNLINK(relh);
                                rs_relh_done(rf->rf_cd, relh);
                            }
                        }
                        break;
#if defined(SS_MME) && !defined(SS_MYSQL)
                    case DBE_LOGREC_MME_INSTUPLEWITHBLOBS:
                        ss_error; /* no blobs yet */
                    case DBE_LOGREC_MME_INSTUPLENOBLOBS:
                    case DBE_LOGREC_MME_DELTUPLE:
                        /* 'Get' new tuple number and ignore it to
                        * keep uniqueness of tuple number.
                        * Do the same thing for the tuple version counter
                        * as well (for similar reasons)
                        */
                        (void)dbe_counter_getnewtuplenum(rf->rf_counter);
                        (void)dbe_counter_getnewtupleversion(rf->rf_counter);

                        /* XXX MME recovery */
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL &&
                            dbe_trxinfo_iscommitted(trxinfo))
                        {
                            mme_rval_t* rval;
                            bool deletemark = (logrectype == DBE_LOGREC_MME_DELTUPLE);

                            ss_pprintf_4(("  Insert this tuple\n"));
                            rc = dbe_rflog_getrval(
                                    rf->rf_cd,
                                    rf->rf_log,
                                    &rval,
                                    &relid);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            relh = rollfwd_relhbyid(rf, relid, DBE_TRXID_NULL, &relentname);
                            hasblobs = (logrectype == DBE_LOGREC_INSTUPLEWITHBLOBS);
                            if (hasblobs) {
                                ss_pprintf_4(("rval has blobs\n"));
                                ss_error; /* no blobs yet */
                                /* rollfwd_closeblobs(rf, p_vtpl);*/
                            } else {
                                ss_pprintf_4(("rval has no blobs\n"));
                            }

                            ss_bassert(mme_rval_getdeletemarkflag(rval) == deletemark);
                            dbe_mme_recovinsert(
                                rf->rf_cd,
                                rf->rf_gobjs->go_mme,
                                rf->rf_trxbuf,
                                relh,
                                NULL,
                                rval,
                                trxinfo->ti_usertrxid,
                                trxid);

                            SS_MEM_SETUNLINK(relh);
                            rs_relh_done(rf->rf_cd, relh);

                            FAKE_CODE(
                                if (logrectype == DBE_LOGREC_MME_DELTUPLE) {
                                    FAKE_CODE_BLOCK(FAKE_DBE_RECOVERY_CRASH_MMEDEL,SsExit(0););
                                } else {
                                    FAKE_CODE_BLOCK(FAKE_DBE_RECOVERY_CRASH_MMEINS,SsExit(0););
                                })
                        }

                        break;
#endif /* defined(SS_MME) && !defined(SS_MYSQL) */
                    /* BLOB operations
                    */
                    case DBE_LOGREC_BLOBG2DATA: {
                        dbe_blobg2id_t bid;
                        dbe_blobg2size_t bofs;
                        rf_blob_t* blob;
                        size_t remaining_datasize;
                        tb_wblobg2stream_t* wbs;

                        ss_dassert(DBE_TRXID_CMP_EX(trxid, DBE_TRXID_NULL) == 0);
                        rc = dbe_rflog_getblobg2idandoffset(rf->rf_log,
                                                            &bid,
                                                            &bofs,
                                                            &remaining_datasize);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        rollfwd_updatemaxblobg2id(rf, bid);
                        blob = rollfwd_findblob(rf, bid);
                        if (blob == NULL) {
                            blob = rollfwd_addblob(rf, bid, bofs);
                        }
                        wbs = blob->rfb_blob;
                        if (wbs == NULL) {
                            rf_blob_open(
                                    blob,
                                    rf);
                            wbs = blob->rfb_blob;
                            ss_dassert(wbs != NULL);
                        }
                        while (remaining_datasize != 0) {
                            size_t blobwritebufsize;
                            size_t nbytestowrite;
                            size_t nbytes_read;
                            ss_byte_t* blobwritebuf;
                            rc = dbe_blobg2callback_wblobreach(
                                    wbs,
                                    &blobwritebuf,
                                    &blobwritebufsize,
                                    NULL);
                            if (rc != SU_SUCCESS) {
                                break;
                            }
                            if (remaining_datasize > blobwritebufsize) {
                                nbytestowrite = blobwritebufsize;
                            } else {
                                nbytestowrite = remaining_datasize;
                            }
                            rc = dbe_rflog_readdata(rf->rf_log,
                                                    blobwritebuf,
                                                    nbytestowrite,
                                                    &nbytes_read);
                            if (rc != SU_SUCCESS && rc != DBE_RC_CONT) {
                                dbe_blobg2callback_wblobrelease(wbs, 0, NULL);
                                break;
                            }
                            ss_dassert(nbytes_read == nbytestowrite);
                            rc = dbe_blobg2callback_wblobrelease(wbs,
                                                                 nbytes_read,
                                                                 NULL);
                            if (rc != SU_SUCCESS) {
                                break;
                            }
                            remaining_datasize -= nbytes_read;
                        }
                        break;
                    }
                    case DBE_LOGREC_BLOBG2DATACOMPLETE: {
                        dbe_blobg2id_t bid;
                        rc = dbe_rflog_getblobg2datacomplete(rf->rf_log, &bid);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        rollfwd_close1blob(rf, bid, FALSE);
                        break;
                    }
                    case DBE_LOGREC_BLOBG2DROPMEMORYREF:
                        /* Ignore for now. We should use this to clean up
                         * in-memory blobs during recovery.
                         */
                        break;
                    case DBE_LOGREC_BLOBSTART_OLD:
                    case DBE_LOGREC_BLOBALLOCLIST_OLD:
                    case DBE_LOGREC_BLOBALLOCLIST_CONT_OLD:
                    case DBE_LOGREC_BLOBDATA_OLD:
                    case DBE_LOGREC_BLOBDATA_CONT_OLD:
                    ss_rc_derror(logrectype);
                        rc = DBE_ERR_LOGFILE_CORRUPT;
                        break;

                    /* Transaction marks
                    */
                    case DBE_LOGREC_ABORTSTMT:
#ifdef SS_MME
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL) {
                            dbe_mme_recovstmtrollback(
                                rf->rf_gobjs->go_mme,
                                trxinfo->ti_usertrxid,
                                trxid);
                        }
#endif /* SS_MME */
#ifdef DBE_REPLICATION
                        if (!ishsbg2
                        &&  DBE_TRXID_NOTNULL(
                                dbe_rtrxbuf_remotebylocaltrxid(
                                        rf->rf_rtrxbuf,
                                        trxid))
                           )
                        {
                            dbe_rtrxbuf_deletebylocaltrxid(
                                rf->rf_rtrxbuf,
                                trxid);
                        }
#endif /* DBE_REPLICATION */
                        break;

                    case DBE_LOGREC_ABORTTRX_INFO:
                    case DBE_LOGREC_ABORTTRX_OLD:
#ifdef SS_MME
                        dbe_mme_recovrollback(
                            rf->rf_gobjs->go_mme,
                            trxid);
#endif /* SS_MME */
#ifdef DBE_REPLICATION
                        if (!ishsbg2) {
                            if (rf->rf_hsbmode != DBE_HSB_SECONDARY) {
                                if (DBE_TRXID_NOTNULL(
                                        dbe_rtrxbuf_remotebylocaltrxid(
                                                rf->rf_rtrxbuf,
                                                trxid))
                                    )
                                {
                                    dbe_rtrxbuf_deletebylocaltrxid(
                                        rf->rf_rtrxbuf,
                                        trxid);
                                }
                            } else {
                                ss_pprintf_2(("SECONDARY\n"));
                                if (!remove_rest) {
                                    dbe_trxid_t remotetrxid;

                                    ss_pprintf_2(("!remove_rest, abort the transaction\n"));
                                    trxinfo = dbe_trxbuf_gettrxinfo(
                                                rf->rf_trxbuf,
                                                trxid);
                                    if (trxinfo != NULL) {
                                        (void)dbe_trxinfo_setaborted(trxinfo);
                                        trxinfo->ti_committrxnum =
                                            dbe_counter_getnewcommittrxnum(rf->rf_counter);
                                    }
                                    remotetrxid = dbe_rtrxbuf_remotebylocaltrxid(
                                                        rf->rf_rtrxbuf, trxid);
                                    if (DBE_TRXID_NOTNULL(remotetrxid)) {
                                        dbe_rtrxbuf_deletebylocaltrxid(rf->rf_rtrxbuf, trxid);
                                    }
                                    ss_pprintf_2(("remotetrxid=%ld,trxid=%ld\n", DBE_TRXID_GETLONG(remotetrxid), DBE_TRXID_GETLONG(trxid)));
                                }
                            }
                        }
#endif /* DBE_REPLICATION */
                        break;
                    case DBE_LOGREC_COMMITSTMT: {
#ifdef SS_MME
                        dbe_trxid_t stmttrxid;
                        rc = dbe_rflog_getcommitstmt(
                                rf->rf_log,
                                &stmttrxid);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        dbe_mme_recovstmtcommit(
                            rf->rf_gobjs->go_mme,
                            trxid,
                            stmttrxid);
#endif /* SS_MME */
#ifdef DBE_REPLICATION
                        if (!ishsbg2) {
#ifndef SS_MME
                            dbe_trxid_t stmttrxid;
                            rc = dbe_rflog_getcommitstmt(
                                    rf->rf_log,
                                    &stmttrxid);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
#endif
                            if (DBE_TRXID_NOTNULL(
                                    dbe_rtrxbuf_remotebylocaltrxid(
                                            rf->rf_rtrxbuf,
                                            stmttrxid))
                                )
                            {
                                dbe_rtrxbuf_deletebylocaltrxid(
                                    rf->rf_rtrxbuf,
                                    stmttrxid);
                            }
                        }
#endif /* DBE_REPLICATION */
                        break;
                    }
                    case DBE_LOGREC_COMMITTRX_INFO:
                    case DBE_LOGREC_COMMITTRX_OLD:
                    case DBE_LOGREC_COMMITTRX_HSB_OLD:
                    case DBE_LOGREC_HSBCOMMITMARK_OLD:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        ss_dassert(trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo));
                        if (logrectype == DBE_LOGREC_COMMITTRX_INFO) {
                            rc = dbe_rflog_getcommitinfo(rf->rf_log, &commitinfo);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                        } else if (logrectype == DBE_LOGREC_COMMITTRX_HSB_OLD) {
                            commitinfo = DBE_LOGI_COMMIT_HSBPRIPHASE2|DBE_LOGI_COMMIT_LOCAL;
                        } else if (logrectype == DBE_LOGREC_HSBCOMMITMARK_OLD) {
                            commitinfo = DBE_LOGI_COMMIT_HSBPRIPHASE1|DBE_LOGI_COMMIT_LOCAL;
                        } else {
                            ss_dassert(logrectype == DBE_LOGREC_COMMITTRX_OLD);
                            commitinfo = 0;
                        }
                        if (!dbe_logi_commitinfo_iscommit(commitinfo)) {
                            break;
                        }
#if !defined(SS_HSBG2)
                        if (SU_BFLAG_TEST(commitinfo, DBE_LOGI_COMMIT_HSBPRIPHASE1)) {
                            ss_dassert(rf->rf_hsbmode == DBE_HSB_PRIMARY);
                            if (DBE_TRXID_EQUAL(trxid, rf->rf_reptrxidlast)) {
                                ss_pprintf_4(("  Set remove_rest = TRUE, trxid=%ld\n", DBE_TRXID_GETLONG(trxid)));
                                remove_rest = TRUE;
                            }
                            break;
                        }
#endif /* !SS_HSBG2 */
#ifdef SS_MME
                        dbe_mme_recovcommit(
                            rf->rf_gobjs->go_mme,
                            trxinfo->ti_usertrxid);
#endif /* SS_MME */

                        if (dbe_gtrs_mergecleanup_recovery) {
                            ss_dprintf_2(("dbe_rollfwd_recover:dbe_cfg_mergecleanup, mergetrxnum=%ld\n", DBE_TRXNUM_GETLONG(trxinfo->ti_committrxnum)));
                            ss_dassert(dbe_trxnum_cmp(trxinfo->ti_committrxnum, rf->rf_committrxnum) <= 0);
                            dbe_counter_setmergetrxnum(
                                rf->rf_counter,
                                trxinfo->ti_committrxnum);
                        }

#ifdef DBE_REPLICATION
                        if (!ishsbg2 && SU_BFLAG_TEST(commitinfo, DBE_LOGI_COMMIT_HSBPRIPHASE2)) {
                            dbe_trxid_t remotetrxid;

                            remotetrxid = dbe_rtrxbuf_remotebylocaltrxid(
                                            rf->rf_rtrxbuf,
                                            trxid);
                            if (!DBE_TRXID_ISNULL(remotetrxid)) {
                                dbe_rtrxbuf_deletebylocaltrxid(
                                    rf->rf_rtrxbuf,
                                    trxid);
                            }
                            if (rf->rf_hsbmode == DBE_HSB_SECONDARY) {
                                if (DBE_TRXID_EQUAL(remotetrxid, rf->rf_reptrxidlast)) {
                                    ss_pprintf_4(("  Set remove_rest = TRUE, remotetrxid=%ld\n", DBE_TRXID_GETLONG(remotetrxid)));
                                    remove_rest = TRUE;
                                }
                            }
                        }
#endif /* DBE_REPLICATION */
                        FAKE_CODE_BLOCK(FAKE_DBE_RECOVERY_CRASH_COMMIT,SsExit(0););
                        break;
                    case DBE_LOGREC_PREPARETRX:
                        rc = DBE_ERR_LOGFILE_CORRUPT;
                        /* not implemented, yet !!!!!! */
                        break;

                    /* Global markers
                    */
                    case DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT:
                    case DBE_LOGREC_CHECKPOINT_OLD:
                    case DBE_LOGREC_SNAPSHOT_OLD:
                    case DBE_LOGREC_CHECKPOINT_NEW:
                    case DBE_LOGREC_SNAPSHOT_NEW:
                        break;
                    case DBE_LOGREC_DELSNAPSHOT:
                        break;

                    /* Data dictionary operations
                    */
                    case DBE_LOGREC_CREATETABLE_FULLYQUALIFIED:
                    case DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED:
                    case DBE_LOGREC_CREATETABLE:
                    case DBE_LOGREC_CREATEVIEW:
                    case DBE_LOGREC_CREATETABLE_NEW:
                    case DBE_LOGREC_CREATEVIEW_NEW: {
                        rs_entname_t en;
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        (void)dbe_counter_getnewrelid(rf->rf_counter);
                        rc = dbe_rflog_getcreatetable(
                                rf->rf_log,
                                &relid,
                                &en,
                                &nkeys,
                                &nattrs);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        ss_pprintf_2(("relid=%ld, %s.%s.%s\n",
                            relid,
                            rs_entname_getcatalog(&en),
                            rs_entname_getschema(&en),
                            rs_entname_getname(&en)));
                        while (nattrs--) {
                            (void)dbe_counter_getnewattrid(rf->rf_counter);
                        }
                        while (nkeys--) {
                            (void)dbe_counter_getnewkeyid(rf->rf_counter);
                        }
                        if (trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo)) {
                            if (logrectype == DBE_LOGREC_CREATETABLE ||
                                logrectype == DBE_LOGREC_CREATETABLE_NEW
                             || logrectype == DBE_LOGREC_CREATETABLE_FULLYQUALIFIED
                            )  {
                                rs_rbuf_removerelh(
                                    rf->rf_cd,
                                    rf->rf_rbuf,
                                    &en);
                                succp = rs_rbuf_addrelname(
                                            rf->rf_cd,
                                            rf->rf_rbuf,
                                            &en,
                                            relid);

                                if (!succp) {
                                    char* logfname;
                                    char* tblname;

                                    logfname = dbe_rflog_getphysicalfname(
                                            rf->rf_log
                                    );
                                    tblname = rs_entname_getname(&en);

                                    su_emergency_exit(
                                        __FILE__,
                                        __LINE__,
                                        DBE_ERR_REDEFINITION_SSS,
                                        "table",
                                        tblname,
                                        logfname
                                    );
                                    break;
                                }
                            } else {
                                rs_rbuf_removeviewh(
                                    rf->rf_cd,
                                    rf->rf_rbuf,
                                    &en);
                                succp = rs_rbuf_addviewname(
                                            rf->rf_cd,
                                            rf->rf_rbuf,
                                            &en,
                                            relid);


                                if (!succp) {
                                    char* logfname;
                                    char* viewname;

                                    logfname = dbe_rflog_getphysicalfname(
                                            rf->rf_log
                                    );
                                    viewname = rs_entname_getname(&en);

                                    su_emergency_exit(
                                        __FILE__,
                                        __LINE__,
                                        DBE_ERR_REDEFINITION_SSS,
                                        "view",
                                        viewname,
                                        logfname
                                    );
                                    break;
                                }
                            }
                        }
                        break;
                    }

                    case DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED:
                    case DBE_LOGREC_RENAMETABLE: {
                        rs_entname_t newrel_en;

                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        relschema = NULL;
                        rc = dbe_rflog_getrenametable(
                                rf->rf_log,
                                &relid,
                                &newrel_en);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        if (trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo)) {
                            /* Get old name */
                            relentname = rollfwd_relnamebyid(rf, relid);

                            succp = rs_rbuf_renamerel(
                                        rf->rf_cd,
                                        rf->rf_rbuf,
                                        relentname,
                                        &newrel_en);

                            if (!succp) {

                                char* logfname;
                                char* tblname;

                                logfname = dbe_rflog_getphysicalfname(
                                        rf->rf_log
                                );
                                tblname = rs_entname_getname(&newrel_en);

                                su_emergency_exit(
                                    __FILE__,
                                    __LINE__,
                                    DBE_ERR_REDEFINITION_SSS,
                                    "table",
                                    tblname,
                                    logfname
                                );
                                break;
                            }
                        }
                        break;
                    }
                    case DBE_LOGREC_CREATEINDEX:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        (void)dbe_counter_getnewkeyid(rf->rf_counter);
                        if (trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo)) {
                            rc = dbe_rflog_getcreateordropindex(
                                    rf->rf_log,
                                    &relid,
                                    &keyid,
                                    &relname);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            relentname = rollfwd_relnamebyid(rf, relid);
                            ss_pprintf_2(("relid=%d, keyid=%ld, %s.%s\n", relid, keyid, rs_entname_getprintschema(relentname), rs_entname_getname(relentname)));
                            rs_rbuf_relhunbuffer(
                                rf->rf_cd,
                                rf->rf_rbuf,
                                relentname);
                            relh = (*rf->rf_recovcallback->rc_getrelhfun)(rf->rf_recovcallback->rc_rbufctx, relentname, NULL);
                            if (relh == NULL) {
                                su_emergency_exit(
                                    __FILE__,
                                    __LINE__,
                                    DBE_ERR_ROLLFWDFAILED
                                );
                                break;
                            }
                            rc = dbe_tuple_recovcreateindex(
                                    rf->rf_cd,
                                    trxid,
                                    trxinfo->ti_committrxnum,
                                    relh,
                                    keyid);
                            if (rc != DBE_RC_SUCC) {
                                su_emergency_exit(
                                    __FILE__,
                                    __LINE__,
                                    DBE_ERR_ROLLFWDFAILED
                                );
                            }
                            SS_MEM_SETUNLINK(relh);
                            rs_relh_done(rf->rf_cd, relh);
                        }
                        break;
                    case DBE_LOGREC_DROPTABLE:
                    case DBE_LOGREC_DROPVIEW:
                    case DBE_LOGREC_TRUNCATETABLE:
                    case DBE_LOGREC_TRUNCATECARDIN:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo)) {
                            rc = dbe_rflog_getdroptable(
                                    rf->rf_log,
                                    &relid,
                                    &relname);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            if (logrectype == DBE_LOGREC_DROPTABLE 
                            ||  logrectype == DBE_LOGREC_TRUNCATETABLE
                            ||  logrectype == DBE_LOGREC_TRUNCATECARDIN) 
                            {
                                ss_dprintf_4(("relid=%ld, relname=%s\n", relid, relname));
                                if (logrectype == DBE_LOGREC_TRUNCATETABLE) {
                                    relh = rollfwd_truncate_relhbyid(rf, relid, trxid, relname);
                                } else {
                                    relh = rollfwd_relhbyid(rf, relid, trxid, &relentname);
                                }
                                if (logrectype == DBE_LOGREC_TRUNCATECARDIN) {
                                    rs_rbuf_relhunbuffer_dropcardin(
                                        rf->rf_cd, 
                                        rf->rf_rbuf, 
                                        rs_relh_entname(rf->rf_cd, relh));
                                    rc = DBE_RC_SUCC;
                                } else {
                                    rc = dbe_tuple_recovdroprel(
                                            rf->rf_cd,
                                            trxid,
                                            trxinfo->ti_committrxnum,
                                            relh,
                                            logrectype == DBE_LOGREC_TRUNCATETABLE);
                                }
                                SS_MEM_SETUNLINK(relh);
                                rs_relh_done(rf->rf_cd, relh);
                                if (rc != DBE_RC_SUCC) {
                                    su_emergency_exit(
                                        __FILE__,
                                        __LINE__,
                                        DBE_ERR_ROLLFWDFAILED
                                    );
                                    break;
                                }
                                if (logrectype == DBE_LOGREC_DROPTABLE) {
                                    rs_rbuf_removerelh(
                                        rf->rf_cd,
                                        rf->rf_rbuf,
                                        relentname);
                                    dbe_db_adddropcardinal(
                                            rs_sysi_db(rf->rf_cd),
                                            relid);
                                }
                            } else {    /* it is a view */
                                relentname = rollfwd_viewnamebyid(rf, relid);
                                rs_rbuf_removeviewh(
                                    rf->rf_cd,
                                    rf->rf_rbuf,
                                    relentname);
                            }
                        }
                        break;
                    case DBE_LOGREC_ALTERTABLE:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        rc = dbe_rflog_getaltertable(
                                rf->rf_log,
                                &relid,
                                &relname,
                                &nattrs);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        while (nattrs--) {
                            (void)dbe_counter_getnewattrid(rf->rf_counter);
                        }
                        if (trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo)) {
                            /* First we remove old relh from rbuf */
                            relentname = rollfwd_relnamebyid(rf, relid);
                            succp = rs_rbuf_relhunbuffer(
                                        rf->rf_cd,
                                        rf->rf_rbuf,
                                        relentname);
                            if (!succp) {
                                su_emergency_exit(
                                    __FILE__,
                                    __LINE__,
                                    DBE_ERR_ROLLFWDFAILED
                                );
                                break;
                            }
                        }
                        break;
                    case DBE_LOGREC_DROPINDEX:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo)) {
                            rc = dbe_rflog_getcreateordropindex(
                                    rf->rf_log,
                                    &relid,
                                    &keyid,
                                    &relname);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            ss_dprintf_4(("relid=%ld, keyid=%ld\n", relid, keyid));
                            relh = rollfwd_relhbyid(rf, relid, trxid, &relentname);
                            rc = dbe_tuple_recovdropindex(
                                    rf->rf_cd,
                                    trxid,
                                    trxinfo->ti_committrxnum,
                                    relh,
                                    keyid,
                                    FALSE);
                            SS_MEM_SETUNLINK(relh);
                            rs_relh_done(rf->rf_cd, relh);
                            if (rc != DBE_RC_SUCC) {
                                su_emergency_exit(
                                    __FILE__,
                                    __LINE__,
                                    DBE_ERR_ROLLFWDFAILED
                                );
                                break;
                            }
                            rs_rbuf_relhunbuffer(
                                rf->rf_cd,
                                rf->rf_rbuf,
                                relentname);
                        }
                        break;
                    case DBE_LOGREC_CREATEUSER:
                        (void)dbe_counter_getnewuserid(rf->rf_counter);
                        break;
                    case DBE_LOGREC_CREATESEQ:
                    case DBE_LOGREC_CREATECTR:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        (void)dbe_counter_getnewkeyid(rf->rf_counter);
                        if (trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo)) {
                            FOUR_BYTE_T seqid;
                            char* seqname;

                            rc = dbe_rflog_getcreatectrorseq(
                                    rf->rf_log,
                                    &seqid,
                                    &seqname);
                            if (rc != DBE_RC_SUCC) {
                                ss_derror;
                                break;
                            }
                            ss_pprintf_2(("seqid=%ld, seqname=%s\n", (long)seqid, seqname));
                            rc = dbe_seq_create(
                                    rf->rf_gobjs->go_seq,
                                    seqid,
                                    NULL);
                            if (rc != DBE_RC_SUCC) {
                                ss_derror;
                                break;
                            }
                        }
                        break;
                    case DBE_LOGREC_DROPCTR:
                    case DBE_LOGREC_DROPSEQ:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL && dbe_trxinfo_iscommitted(trxinfo)) {
                            FOUR_BYTE_T seqid;
                            char* seqname;

                            rc = dbe_rflog_getdropctrorseq(
                                    rf->rf_log,
                                    &seqid,
                                    &seqname);
                            if (rc != DBE_RC_SUCC) {
                                ss_derror;
                                break;
                            }
                            ss_pprintf_2(("seqid=%ld, seqname=%s\n", (long)seqid, seqname));
                            rc = dbe_seq_drop(
                                    rf->rf_gobjs->go_seq,
                                    seqid,
                                    NULL);
                            if (rc != DBE_RC_SUCC) {
                                ss_derror;
                                break;
                            }
                        }
                        break;

                    case DBE_LOGREC_INCSYSCTR:
                        {
                            dbe_sysctrid_t ctrid;

                            rc = dbe_rflog_getincsysctr(rf->rf_log, &ctrid);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            dbe_counter_incctrbyid(rf->rf_counter, ctrid);
                        }
                        break;
                    case DBE_LOGREC_SETHSBSYSCTR:
                        {
                            char data[DBE_HSBSYSCTR_SIZE];

                            rc = dbe_rflog_gethsbsysctr(rf->rf_log, data);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            dbe_counter_setreplicacounters(rf->rf_counter, TRUE, data);
                        }
                        break;
                    case DBE_LOGREC_SETSEQ:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL && (dbe_trxinfo_iscommitted(trxinfo)
#ifdef DBE_REPLICATION
                            || (rf->rf_hsbmode != DBE_HSB_STANDALONE
                               && !dbe_trxinfo_isaborted(trxinfo)
                               && !remove_rest
                               && !ishsbg2)
#endif /* DBE_REPLICATION */
                            ))
                        {
                            FOUR_BYTE_T seqid;
                            rs_tuplenum_t seqvalue;

                            rc = dbe_rflog_getsetseq(
                                    rf->rf_log,
                                    &seqid,
                                    &seqvalue);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            rc = dbe_seq_setvalue(
                                    rf->rf_gobjs->go_seq,
                                    seqid,
                                    &seqvalue);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                        }
                        break;
                    case DBE_LOGREC_INCCTR:
                        {
                            FOUR_BYTE_T seqid;

                            rc = dbe_rflog_getincctr(
                                    rf->rf_log,
                                    &seqid);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            rc = dbe_seq_inc(
                                    rf->rf_gobjs->go_seq,
                                    seqid);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                        }
                        break;
                    case DBE_LOGREC_SETCTR:
                        {
                            FOUR_BYTE_T seqid;
                            rs_tuplenum_t seqvalue;

                            rc = dbe_rflog_getsetctr(
                                    rf->rf_log,
                                    &seqid,
                                    &seqvalue);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            rc = dbe_seq_setvalue(
                                    rf->rf_gobjs->go_seq,
                                    seqid,
                                    &seqvalue);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                        }
                        break;
                    case DBE_LOGREC_SWITCHTOPRIMARY:
#ifdef DBE_REPLICATION
                        ss_dassert(rf->rf_hsbmode != DBE_HSB_PRIMARY);
                        rf->rf_hsbmode = DBE_HSB_PRIMARY;
#endif /* DBE_REPLICATION */
                        break;
                    case DBE_LOGREC_SWITCHTOSECONDARY:
                    case DBE_LOGREC_SWITCHTOSECONDARY_NORESET:
#ifdef DBE_REPLICATION
                        ss_dassert(rf->rf_hsbmode != DBE_HSB_SECONDARY);
                        rf->rf_hsbmode = DBE_HSB_SECONDARY;
#endif /* DBE_REPLICATION */
                        break;
                    case DBE_LOGREC_CLEANUPMAPPING:
                        break;
                    case DBE_LOGREC_REPLICASTMTSTART:
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL &&
                                dbe_trxinfo_iscommitted(trxinfo)) {
                            break;
                        }

#ifdef DBE_REPLICATION
                        if (!ishsbg2 && remove_rest && hsbrecovery) {
                            dbe_trxid_t localstmtid;
                            dbe_trxid_t remotestmtid;
                            dbe_trxstate_t trxstate;

                            rc = dbe_rflog_getreplicastmtstart(
                                    rf->rf_log,
                                    &remotestmtid,
                                    &localstmtid);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                            /* save the trxstate before remove it */
                            trxstate = dbe_trxbuf_gettrxstate(
                                           rf->rf_trxbuf,
                                           localstmtid,
                                           NULL,
                                           NULL);
                            /* this function actually remove the entry */
                            dbe_trxbuf_marktoberemoved(
                                rf->rf_trxbuf,
                                localstmtid);
                            any_removed = TRUE;
                            rc = dbe_rtrxbuf_deletebylocaltrxid(
                                    rf->rf_rtrxbuf,
                                    localstmtid);
                            if (rc != DBE_RC_SUCC) {
                                if (trxstate != DBE_TRXST_ABORT) {
                                    /* Aborted transactions are removed
                                     * during scanning phase.
                                     */
                                    break;
                                } else {
                                    rc = DBE_RC_SUCC;
                                }
                            }
                        }
#endif /* DBE_REPLICATION */
                        break;
                    case DBE_LOGREC_REPLICATRXSTART :
                        trxinfo = dbe_trxbuf_gettrxinfo(rf->rf_trxbuf, trxid);
                        if (trxinfo != NULL &&
                                dbe_trxinfo_iscommitted(trxinfo)) {
                            break;
                        }
#ifdef DBE_REPLICATION
                        if (!ishsbg2 && remove_rest && hsbrecovery) {
                            dbe_trxstate_t trxstate;

                            /* save the trxstate before remove it */
                            trxstate = dbe_trxbuf_gettrxstate(
                                            rf->rf_trxbuf,
                                            trxid,
                                            NULL,
                                            NULL);
                            /* this function actually remove the entry */
                            dbe_trxbuf_marktoberemoved(
                                rf->rf_trxbuf,
                                trxid);
                            any_removed = TRUE;
                            rc = dbe_rtrxbuf_deletebylocaltrxid(
                                    rf->rf_rtrxbuf,
                                    trxid);
                            if (rc != DBE_RC_SUCC) {
                                if (trxstate != DBE_TRXST_ABORT) {
                                    /* Aborted transactions are removed
                                     * during scanning phase.
                                     */
                                    break;
                                } else {
                                    rc = DBE_RC_SUCC;
                                }
                            }
                        }
#endif /* DBE_REPLICATION */

                        break;

                    case DBE_LOGREC_AUDITINFO:
                        rc = dbe_rflog_getauditinfo(rf->rf_log, &userid, &info);
                        if (rc != DBE_RC_SUCC) {
                            break;
                        }
                        ss_pprintf_2(("userid=%ld, info='%s'\n", userid, info));
                        break;

                    case DBE_LOGREC_HSBG2_DURABLE:
                    case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
                    case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
                        break;

                    case DBE_LOGREC_HSBG2_NEW_PRIMARY:
#ifdef SS_HSBG2
                        {
                            long originator_nodeid;
                            long primary_nodeid;
                            rc = dbe_rflog_get_hsb_new_primary(
                                        rf->rf_log,
                                        &originator_nodeid,
                                        &primary_nodeid);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                        }
#endif /* SS_HSBG2 */
                        break;

                    case DBE_LOGREC_HSBG2_NEWSTATE:
                        break;

                    case DBE_LOGREC_HSBG2_ABORTALL:
                        break;

                    case DBE_LOGREC_COMMENT:
                        break;

                    default:
                        su_rc_error(logrectype);
                        break;
                }
                if (relentname != NULL) {
                    rs_entname_done(relentname);
                    relentname = NULL;
                }
                if (rc != DBE_RC_SUCC) {
                    break;
                }
            }
        } else {
            rc = DBE_RC_END;
        }
        if (rc == DBE_RC_END) {
            if (rf->rf_iscorrupt) {
                rc = DBE_RC_LOGFILE_TAIL_CORRUPT;
            } else {
                rc = DBE_RC_SUCC;
            }
            if (recheader_gotten) {
                ss_derror; /* should not be here, because
                              dbe_rollfwd_scancommitmarks should take
                              care of this situation!
                           */
                dbe_rflog_saverecordpos(rf->rf_log, &rf->rf_corruptpos);
                dbe_rflog_cleartoeof(rf->rf_log, &rf->rf_corruptpos);
            }
        }
        rf->rf_state = RFSTATE_RECOVERED;
        DBE_BLOBG2ID_ADDASSIGN_UINT2(&rf->rf_maxblobg2id, 1);
        dbe_counter_setblobg2id(rf->rf_counter, rf->rf_maxblobg2id);
#ifdef DBE_REPLICATION
        if (any_removed) {
            dbe_trxbuf_removemarked(rf->rf_trxbuf);
        }
#endif /* DBE_REPLICATION */

        dbe_db_exitaction(rs_sysi_db(rf->rf_cd), rf->rf_cd);

        rs_rbuf_setrecovery(
            rf->rf_cd,
            rf->rf_rbuf,
            FALSE);

        dbe_gtrs_mergecleanup_recovery = FALSE;

        SS_POPNAME;

        ss_pprintf_2(("dbe_rollfwd_recover:return, rc=%s (%d)\n",
            su_rc_nameof(rc), (int)rc));

        return (rc);
}

#ifdef DBE_REPLICATION

dbe_hsbmode_t dbe_rollfwd_gethsbmode(dbe_rollfwd_t* rf)
{
        ss_pprintf_1(("dbe_rollfwd_gethsbmode:hsbmode=%d\n", rf->rf_hsbmode));

        return (rf->rf_hsbmode);
}

dbe_trxid_t dbe_rollfwd_getreptrxidmax(dbe_rollfwd_t* rf)
{
        ss_pprintf_1(("dbe_rollfwd_getreptrxidmax:reptrxidmax=%ld\n", DBE_TRXID_GETLONG(rf->rf_reptrxidlast)));

        return (rf->rf_reptrxidlast);
}

bool dbe_rollfwd_getreptrxidmaxupdated(dbe_rollfwd_t* rf)
{
        ss_pprintf_1(("dbe_rollfwd_getreptrxidmaxupdated:reptrxidmaxupdated=%d\n", rf->rf_reptrxidmaxupdated));

        return (rf->rf_reptrxidmaxupdated);
}

#endif /* DBE_REPLICATION */

#endif /* SS_NOLOGGING */

