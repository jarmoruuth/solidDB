/*************************************************************************\
**  source       * dbe7ctr.h
**  directory    * dbe
**  description  * db engine counter container
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


#ifndef DBE7CTR_H
#define DBE7CTR_H

#include <sslimits.h>
#include <rs0tnum.h>
#include "dbe8srec.h"
#include "dbe0type.h"

#define CHK_CTR(ctr)            ss_dassert(SS_CHKPTR(ctr))
#define DBE_CTR_MAXLIMIT        (SS_UINT4_MAX - 11000U)

typedef enum {
        DBE_CTR_TRXID,
        DBE_CTR_MAXTRXNUM,
        DBE_CTR_COMMITTRXNUM,
        DBE_CTR_MERGETRXNUM,
        DBE_CTR_CPNUM,
        DBE_CTR_TUPLENUM,
        DBE_CTR_ATTRID,
        DBE_CTR_KEYID,
        DBE_CTR_USERID,
        DBE_CTR_LOGFNUM,
        DBE_CTR_BLOBID,
        DBE_CTR_MERGECTR,
        DBE_CTR_TUPLEVERSION,
        DBE_CTR_SYNCMSGID,          /* SS_SYNC */
        DBE_CTR_SYNCTUPLEVERSION,   /* SS_SYNC */
        DBE_CTR_BLOBG2ID,
        DBE_CTR_STORAGETRXNUM,
} dbe_sysctrid_t;

typedef struct {
        ss_uint4_t tc_lo;
        ss_uint4_t tc_hi;
} trxctr_t;

struct dbe_counter_st {
        trxctr_t       ctr_trxid;         /* Unique transaction id. */
        trxctr_t       ctr_maxtrxnum;     /* Highest committed transaction
                                             number available for searches. */
        trxctr_t       ctr_committrxnum;  /* Last committed transaction
                                             number. This is the transaction
                                             serialization number counter.
                                             Also called as transaction port. */
        trxctr_t       ctr_mergetrxnum;   /* Transaction level below which
                                             all transactions are ended. This
                                             is the merge level. No history
                                             info is preserved below this
                                             level. */
        trxctr_t       ctr_storagetrxnum; /* Transaction level that is fully
                                             merged into storage tree. */
        dbe_cpnum_t    ctr_cpnum;         /* Checkpoint number. */
        rs_tuplenum_t  ctr_tuplenum;      /* Unique tuple number is generated
                                             from here. */
        ulong          ctr_attrid;        /* attribute ID counter */
        ulong          ctr_keyid;         /* Key/relation ID counter */
        ulong          ctr_userid;        /* User/group ID counter */
        dbe_logfnum_t  ctr_logfnum;       /* log file number counter */
        dbe_blobg2id_t ctr_blobg2id;      /* 64 bit BLOB id */
        ulong          ctr_mergectr;      /* merge counter */
        rs_tuplenum_t  ctr_tupleversion;  /* tuple version counter */
#ifdef SS_SYNC
        ulong          ctr_syncmsgid;           /* sync message id */
        rs_tuplenum_t  ctr_synctupleversion;    /* sync tuple version */
#endif
        SsQsemT*       ctr_mutex0;        /* Mutex semaphore */
        SsQsemT*       ctr_mutex1;        /* Mutex semaphore */
        SsQsemT*       ctr_mutex2;        /* Mutex semaphore */
        SsQsemT*       ctr_mutex3;        /* Mutex semaphore */
        SsQsemT*       ctr_mutex4;        /* Mutex semaphore */
        SsQsemT*       ctr_mutex5;        /* Mutex semaphore */
        SsQsemT*       ctr_mutex6;        /* Mutex semaphore */
        SsQsemT*       ctr_mutex7;        /* Mutex semaphore */
        bool*          ctr_attrid_used;   /* Array of unused identifiers
                                             used in database conversion */
        long           ctr_attrid_idx;    /* Index into current pos in
                                             ctr_attrid_used */
        bool*          ctr_keyid_used;    /* Array of unused identifiers
                                             used in database conversion */
        long           ctr_keyid_idx;     /* Index into current pos in
                                             ctr_keyid_used */
        bool           ctr_convert;       /* convert on/off */
        dbe_trxnum_t   ctr_activemergetrxnum; /* Current actibe merge trxnum. */
};

#define trxctr_getlow4bytes(trxctr)     ((trxctr)->tc_lo)
#define trxctr_gethigh4bytes(trxctr)    ((trxctr)->tc_hi)

dbe_counter_t* dbe_counter_init(
        void);

void dbe_counter_done(
        dbe_counter_t* ctr);

void dbe_counter_newdbinit(
        dbe_counter_t* ctr);

void dbe_counter_getinfofromstartrec(
        dbe_counter_t* ctr,
        dbe_startrec_t* sr);

void dbe_counter_putinfotostartrec(
        dbe_counter_t* ctr,
        dbe_startrec_t* sr);

dbe_trxid_t dbe_counter_getnewtrxid(
        dbe_counter_t* ctr);

void dbe_counter_settrxid(
        dbe_counter_t* ctr,
        dbe_trxid_t trxid);

ss_int8_t dbe_counter_getnewint8trxid(
        dbe_counter_t* ctr);

void dbe_counter_setint8trxid(
        dbe_counter_t* ctr,
        ss_int8_t int8trxid);

SS_INLINE dbe_trxid_t dbe_counter_gettrxid(
        dbe_counter_t* ctr);

void dbe_counter_get8bytetrxid(dbe_counter_t* ctr,
                               dbe_trxid_t trxid,
                               ss_uint4_t* p_low4bytes,
                               ss_uint4_t* p_high4bytes);

dbe_trxnum_t dbe_counter_getnewcommittrxnum(
        dbe_counter_t* ctr);

SS_INLINE dbe_trxnum_t dbe_counter_getcommittrxnum(
        dbe_counter_t* ctr);

SS_INLINE dbe_trxnum_t dbe_counter_getmaxtrxnum(
        dbe_counter_t* ctr);

void dbe_counter_setmaxtrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t maxtrxnum);

SS_INLINE dbe_trxnum_t dbe_counter_getmergetrxnum(
        dbe_counter_t* ctr);

void dbe_counter_getactivemergetrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t* p_mergetrxnum,
        dbe_trxnum_t* p_activemergetrxnum);

void dbe_counter_setmergetrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t mergetrxnum);

void dbe_counter_setactivemergetrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t activemergetrxnum);

SS_INLINE dbe_trxnum_t dbe_counter_getstoragetrxnum(
        dbe_counter_t* ctr);

void dbe_counter_setstoragetrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t storagetrxnum);

rs_tuplenum_t dbe_counter_getnewtuplenum(
        dbe_counter_t* ctr);

rs_tuplenum_t dbe_counter_getcurtuplenum(
        dbe_counter_t* ctr);

ss_int8_t dbe_counter_getnewint8tuplenum(
        dbe_counter_t* ctr);

ss_int8_t dbe_counter_getcurint8tuplenum(
        dbe_counter_t* ctr);

ulong dbe_counter_getnewrelid(
        dbe_counter_t* ctr);

ulong dbe_counter_getcurrelid(
        dbe_counter_t* ctr);

void dbe_counter_setnewrelid(
        dbe_counter_t* ctr,
        ulong relid);

ulong dbe_counter_getnewattrid(
        dbe_counter_t* ctr);

ulong dbe_counter_getnewkeyid(
        dbe_counter_t* ctr);

void dbe_counter_setnewkeyid(
        dbe_counter_t* ctr,
        ulong keyid);

ulong dbe_counter_getnewuserid(
        dbe_counter_t* ctr);

SS_INLINE dbe_cpnum_t dbe_counter_getcpnum(
        dbe_counter_t* ctr);

void dbe_counter_setcpnum(
        dbe_counter_t* ctr,
        dbe_cpnum_t cpnum);

dbe_cpnum_t dbe_counter_inccpnum(
        dbe_counter_t* ctr);

dbe_logfnum_t dbe_counter_getlogfnum(
        dbe_counter_t* ctr);

dbe_logfnum_t dbe_counter_inclogfnum(
        dbe_counter_t* ctr);

void dbe_counter_setlogfnum(
        dbe_counter_t* ctr,
        dbe_logfnum_t logfnum);

dbe_blobid_t dbe_counter_getnewblobid(
        dbe_counter_t* ctr);

dbe_blobg2id_t dbe_counter_getnewblobg2id(
        dbe_counter_t* ctr);

void dbe_counter_setblobg2id(
        dbe_counter_t* ctr,
        dbe_blobg2id_t bid);

ulong dbe_counter_getmergectr(
        dbe_counter_t* ctr);

void dbe_counter_setmergectr(
        dbe_counter_t* ctr,
        ulong mergectr);

rs_tuplenum_t dbe_counter_getnewtupleversion(
        dbe_counter_t* ctr);

rs_tuplenum_t dbe_counter_getcurtupleversion(
        dbe_counter_t* ctr);

ss_int8_t dbe_counter_getnewint8tupleversion(
        dbe_counter_t* ctr);

ss_int8_t dbe_counter_getcurint8tupleversion(
        dbe_counter_t* ctr);

void dbe_counter_settupleversion(
        dbe_counter_t* ctr,
        rs_tuplenum_t tupleversion);

#ifdef SS_SYNC

ulong dbe_counter_getnewsyncmsgid(
        dbe_counter_t* ctr);

rs_tuplenum_t dbe_counter_getsynctupleversion(
        dbe_counter_t* ctr);

rs_tuplenum_t dbe_counter_getnewsynctupleversion(
        dbe_counter_t* ctr);

#endif /* SS_SYNC */

void dbe_counter_incctrbyid(
        dbe_counter_t* ctr,
        dbe_sysctrid_t ctrid);

void dbe_counter_getreplicacounters(
        dbe_counter_t* ctr,
        bool hsbg2,
        char** p_data,
        int* p_size);

bool dbe_counter_setreplicacounters(
        dbe_counter_t* ctr,
        bool hsbg2,
        char* data);

void dbe_counter_printinfo(
        void* fp,
        dbe_counter_t* ctr);

void dbe_counter_convert_init(
        dbe_counter_t* ctr,
        bool *used_attrid,
        bool *used_keyid);

void dbe_counter_convert_set(
        dbe_counter_t* ctr,
        bool b);

void dbe_counter_convert_done(
        dbe_counter_t* ctr);

#if defined(DBE7CTR_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              dbe_counter_getstoragetrxnum
 *
 * Returns the storage transaction number.
 *
 * The storage transaction number which is a transaction number below which
 * all versions has been moved to storage tree.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      storage transaction number.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxnum_t dbe_counter_getstoragetrxnum(dbe_counter_t* ctr)
{
        dbe_trxnum_t storagetrxnum;

        CHK_CTR(ctr);
        storagetrxnum = DBE_TRXNUM_INIT(trxctr_getlow4bytes(&ctr->ctr_storagetrxnum));

        return(storagetrxnum);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getmaxtrxnum
 *
 * Returns the maximum committed transaction number.
 *
 * The maximum transaction number is a transaction visibility number
 * below which all transactions are either committed or aborted.
 * It is used as a read level for new transactions.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      Maximum transaction visibility number.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxnum_t dbe_counter_getmaxtrxnum(dbe_counter_t* ctr)
{
        dbe_trxnum_t maxtrxnum;

        CHK_CTR(ctr);

        maxtrxnum = DBE_TRXNUM_INIT(trxctr_getlow4bytes(&ctr->ctr_maxtrxnum));

        return(maxtrxnum);
}

/*##**********************************************************************\
 *
 *              dbe_counter_gettrxid
 *
 * Returns the current maximum used transaction id.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      Current maximum transaction id.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxid_t dbe_counter_gettrxid(dbe_counter_t* ctr)
{
        dbe_trxid_t trxid;

        CHK_CTR(ctr);

        trxid = DBE_TRXID_INIT(trxctr_getlow4bytes(&ctr->ctr_trxid));

        return(trxid);
}

SS_INLINE dbe_trxnum_t dbe_counter_getcommittrxnum(dbe_counter_t* ctr)
{
        dbe_trxnum_t committrxnum;

        CHK_CTR(ctr);

        committrxnum = DBE_TRXNUM_INIT(trxctr_getlow4bytes(&ctr->ctr_committrxnum));

        return(committrxnum);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getmergetrxnum
 *
 * Returns the merge transaction number.
 *
 * The merge transaction number is a transaction number below which
 * no versions of key values are needed. That means that all transactions
 * have a read level that is higher than the merge transaction number.
 * This is used as the index merge level.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      Merge transaction number.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxnum_t dbe_counter_getmergetrxnum(dbe_counter_t* ctr)
{
        dbe_trxnum_t mergetrxnum;

        CHK_CTR(ctr);

        mergetrxnum = DBE_TRXNUM_INIT(trxctr_getlow4bytes(&ctr->ctr_mergetrxnum));

        return(mergetrxnum);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getcpnum
 *
 * Gets cp # counter value
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object
 *
 * Return value :
 *      checkpoint counter value
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_cpnum_t dbe_counter_getcpnum(dbe_counter_t* ctr)
{
        dbe_cpnum_t cpnum;

        CHK_CTR(ctr);
        cpnum = ctr->ctr_cpnum;

        return (cpnum);
}

#endif /* defined(DBE7CTR_C) || defined(SS_USE_INLINE) */

#endif /* DBE7CTR_H */
