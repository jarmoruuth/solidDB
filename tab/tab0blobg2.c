/*************************************************************************\
**  source       * tab0blobg2.c
**  directory    * tab
**  description  * New BLOB (and long data in general) interface
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

see:
//depot/doc/Sputnik/BLOB_2nd_Generation_IS.doc


Limitations:
-----------
Currently seeking to random position is not supported.
This limitation will be removed when necessary and the
design enables efficient implementation of it


Error handling:
--------------

routines that could fail return status code and
error handle when operation fails

Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------

shared data structures are protected implecitly within the
methods

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssthread.h>
#include <dbe0type.h>
#include <su0error.h>
#include <su0err.h>
#include <su0mesl.h>
#include <rs0sysi.h>
#include <dbe0bstr.h>
#include <dbe7cfg.h>
#include "tab0tran.h"
#include "tab0tli.h"
#include "tab1defs.h"
#include "tab0blobg2.h"

static dbe_blobg2id_t blobg2id_null;

char tb_blobg2_sysblobs_create_stmts[] = {
"CREATE TABLE SYS_BLOBS (\n"
"    ID BIGINT NOT NULL,\n"
"    STARTPOS BIGINT NOT NULL,\n"
"    ENDSIZE BIGINT NOT NULL,\n"
"    TOTALSIZE BIGINT, -- NULL when STARTPOS <> 0\n"
"    REFCOUNT INTEGER, -- NULL when STARTPOS <> 0\n"
"    COMPLETE INTEGER, -- Boolean when STARTPOS = 0, NULL otherwise\n"
"    STARTCPNUM INTEGER, -- Checkpoint number at start of BLOB write or NULL when STARTPOS <> 0\n"
"    NUMPAGES INTEGER NOT NULL,\n"
"    P01_ADDR INTEGER,\n"
"    P01_ENDSIZE BIGINT,\n"
"    P02_ADDR INTEGER,\n"
"    P02_ENDSIZE BIGINT,\n"
"    P03_ADDR INTEGER,\n"
"    P03_ENDSIZE BIGINT,\n"
"    P04_ADDR INTEGER,\n"
"    P04_ENDSIZE BIGINT,\n"
"    P05_ADDR INTEGER,\n"
"    P05_ENDSIZE BIGINT,\n"
"    P06_ADDR INTEGER,\n"
"    P06_ENDSIZE BIGINT,\n"
"    P07_ADDR INTEGER,\n"
"    P07_ENDSIZE BIGINT,\n"
"    P08_ADDR INTEGER,\n"
"    P08_ENDSIZE BIGINT,\n"
"    P09_ADDR INTEGER,\n"
"    P09_ENDSIZE BIGINT,\n"
"    P10_ADDR INTEGER,\n"
"    P10_ENDSIZE BIGINT,\n"
"    P11_ADDR INTEGER,\n"
"    P11_ENDSIZE BIGINT,\n"
"    P12_ADDR INTEGER,\n"
"    P12_ENDSIZE BIGINT,\n"
"    P13_ADDR INTEGER,\n"
"    P13_ENDSIZE BIGINT,\n"
"    P14_ADDR INTEGER,\n"
"    P14_ENDSIZE BIGINT,\n"
"    P15_ADDR INTEGER,\n"
"    P15_ENDSIZE BIGINT,\n"
"    P16_ADDR INTEGER,\n"
"    P16_ENDSIZE BIGINT,\n"
"    P17_ADDR INTEGER,\n"
"    P17_ENDSIZE BIGINT,\n"
"    P18_ADDR INTEGER,\n"
"    P18_ENDSIZE BIGINT,\n"
"    P19_ADDR INTEGER,\n"
"    P19_ENDSIZE BIGINT,\n"
"    P20_ADDR INTEGER,\n"
"    P20_ENDSIZE BIGINT,\n"
"    P21_ADDR INTEGER,\n"
"    P21_ENDSIZE BIGINT,\n"
"    P22_ADDR INTEGER,\n"
"    P22_ENDSIZE BIGINT,\n"
"    P23_ADDR INTEGER,\n"
"    P23_ENDSIZE BIGINT,\n"
"    P24_ADDR INTEGER,\n"
"    P24_ENDSIZE BIGINT,\n"
"    P25_ADDR INTEGER,\n"
"    P25_ENDSIZE BIGINT,\n"
"    P26_ADDR INTEGER,\n"
"    P26_ENDSIZE BIGINT,\n"
"    P27_ADDR INTEGER,\n"
"    P27_ENDSIZE BIGINT,\n"
"    P28_ADDR INTEGER,\n"
"    P28_ENDSIZE BIGINT,\n"
"    P29_ADDR INTEGER,\n"
"    P29_ENDSIZE BIGINT,\n"
"    P30_ADDR INTEGER,\n"
"    P30_ENDSIZE BIGINT,\n"
"    P31_ADDR INTEGER,\n"
"    P31_ENDSIZE BIGINT,\n"
"    P32_ADDR INTEGER,\n"
"    P32_ENDSIZE BIGINT,\n"
"    P33_ADDR INTEGER,\n"
"    P33_ENDSIZE BIGINT,\n"
"    P34_ADDR INTEGER,\n"
"    P34_ENDSIZE BIGINT,\n"
"    P35_ADDR INTEGER,\n"
"    P35_ENDSIZE BIGINT,\n"
"    P36_ADDR INTEGER,\n"
"    P36_ENDSIZE BIGINT,\n"
"    P37_ADDR INTEGER,\n"
"    P37_ENDSIZE BIGINT,\n"
"    P38_ADDR INTEGER,\n"
"    P38_ENDSIZE BIGINT,\n"
"    P39_ADDR INTEGER,\n"
"    P39_ENDSIZE BIGINT,\n"
"    P40_ADDR INTEGER,\n"
"    P40_ENDSIZE BIGINT,\n"
"    P41_ADDR INTEGER,\n"
"    P41_ENDSIZE BIGINT,\n"
"    P42_ADDR INTEGER,\n"
"    P42_ENDSIZE BIGINT,\n"
"    P43_ADDR INTEGER,\n"
"    P43_ENDSIZE BIGINT,\n"
"    P44_ADDR INTEGER,\n"
"    P44_ENDSIZE BIGINT,\n"
"    P45_ADDR INTEGER,\n"
"    P45_ENDSIZE BIGINT,\n"
"    P46_ADDR INTEGER,\n"
"    P46_ENDSIZE BIGINT,\n"
"    P47_ADDR INTEGER,\n"
"    P47_ENDSIZE BIGINT,\n"
"    P48_ADDR INTEGER,\n"
"    P48_ENDSIZE BIGINT,\n"
"    P49_ADDR INTEGER,\n"
"    P49_ENDSIZE BIGINT,\n"
"    P50_ADDR INTEGER,\n"
"    P50_ENDSIZE BIGINT,\n"
"    PRIMARY KEY(ID, ENDSIZE))\n"
"@"
"CREATE INDEX SYS_BLOBS_REFCOUNT_IDX ON _SYSTEM.SYS_BLOBS (REFCOUNT,ID)"
};

static dbe_blobg2size_t bs_0;

/* note must mach with the above table definition!!! */
#define BS_PAGEARR_SIZE 50

/* this maybe should be a configurable entity? */
#define WBS_PREALLOC_SIZE 10


#define CHK_WBS(wbs) ss_assert((wbs) != NULL); \
        ss_rc_assert((wbs)->wbs_check == TBCHK_BLOBG2WRITESTREAM, (wbs)->wbs_check)

typedef struct {
        dbe_blobg2size_t bp_size_so_far;
        su_daddr_t bp_addr;
} bpagerec_t;

typedef enum {
    FLUSHMODE_PAGEARRAY_FULL, /* page array becomes full */
    FLUSHMODE_CHECKPOINT,   /* checkpoint creation initiated flush */
    FLUSHMODE_COMPLETE      /* when BLOB stream is completed */
} flushmode_t;

typedef enum {
    BS_STAT_RELEASED,
    BS_STAT_REACHED
} bs_status_t;

typedef struct {
        int imbr_check;
        dbe_blobg2id_t imbr_id; /* BLOB ID, search key for the record */
        size_t imbr_count;      /* in-memory (not persistent ref. count */
        bool imbr_persistent_count_known; /* persistent ref. count known? */
        ss_int4_t imbr_persistent_count_if_known; /* persistent count
                                                   * (if known)
                                                   */
        bool imbr_locked; /* access locked to this record, must wait */
        bool imbr_newblob;
        su_meswaitlist_t* imbr_meswaitlist; /* used for waiting for the lock to be released */
} inmemoryblobrefrec_t;

#define CHK_IMBR(imbr) ss_assert(imbr != NULL && imbr->imbr_check == TBCHK_INMEMORYBLOBG2REF)

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_init
 *
 * Creates a descriptor for in-memory BLOB descriptor.
 * It is used for caching the reference count and also to
 * serialize reference count access for a given BLOB.
 *
 * Parameters:
 *      id - in
 *          BLOB ID
 *
 *      count - in
 *          initial in-memory count
 *
 * Return value - give:
 *       newly allocated in-memory BLOB reference record
 *
 * Limitations:
 *
 * Globals used:
 */
static inmemoryblobrefrec_t* inmemoryblobrefrec_init(
        dbe_blobg2id_t id,
        size_t count,
        bool newblob)
{
        inmemoryblobrefrec_t* imbr = SSMEM_NEW(inmemoryblobrefrec_t);
        imbr->imbr_check = TBCHK_INMEMORYBLOBG2REF;
        imbr->imbr_id = id;
        imbr->imbr_count = count;
        imbr->imbr_persistent_count_known = FALSE;
        ss_debug(imbr->imbr_persistent_count_if_known = 0xBabeFace);
        imbr->imbr_locked = FALSE;
        imbr->imbr_newblob = newblob;
        imbr->imbr_meswaitlist = su_meswaitlist_init();
        return (imbr);
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_done
 *
 * releases a blob reference descriptor record
 *
 * Parameters:
 *      imbr - in, take
 *          the record to be released
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void inmemoryblobrefrec_done(inmemoryblobrefrec_t* imbr)
{
        CHK_IMBR(imbr);
        imbr->imbr_check = TBCHK_FREEDINMEMORYBLOBG2REF;
        ss_rc_dassert(imbr->imbr_count == 0, (int)imbr->imbr_count);
        SsMemFree(imbr);
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_insertcmp
 *
 * comparison routine for blob reference record for rb-tree inserts
 *
 * Parameters:
 *      imbr1 - in, use
 *          pointer to 1st record to compare
 *
 *      imbr2 - in, use
 *          pointer to 2nd record to compare
 *
 * Return value:
 *      0 means equal
 *      <0 means imbr1 < imbr2
 *      >0 means imbr1 > imbr2
 *
 * Limitations:
 *
 * Globals used:
 */
static int inmemoryblobrefrec_insertcmp(
        inmemoryblobrefrec_t* imbr1,
        inmemoryblobrefrec_t* imbr2)
{
        ss_beta(CHK_IMBR(imbr1));
        ss_beta(CHK_IMBR(imbr2));
        return (DBE_BLOBG2ID_CMP(imbr1->imbr_id, imbr2->imbr_id));
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_searchcmp
 *
 * Comparison for searches, the 1st parameter (the search key is
 * a pointer to BLOB id, not to complete imbr record).
 * otherwise same as inmemoryblobrefrec_insertcmp
 *
 * Parameters:
 *      p_id - in, use
 *          pointer to BLOB ID
 *
 *      imbr - in, use
 *          pointer to BLOB ref record
 *
 * Return value:
 *       (see description above)
 *
 * Limitations:
 *
 * Globals used:
 */
static int inmemoryblobrefrec_searchcmp(
        dbe_blobg2id_t* p_id,
        inmemoryblobrefrec_t* imbr)
{
        ss_beta(CHK_IMBR(imbr));
        return (DBE_BLOBG2ID_CMP(*p_id, imbr->imbr_id));
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_inc
 *
 * Incremets in-memory reference count for a BLOB reference descriptor
 *
 * Parameters:
 *      imbr - in, use
 *          pointer to in-memory BLOB reference descriptor
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void inmemoryblobrefrec_inc(inmemoryblobrefrec_t* imbr)
{
        ss_beta(CHK_IMBR(imbr));
        imbr->imbr_count++;
        ss_dprintf_1(("inmemoryblobrefrec_inc(id=0x%08lX%08lX) count = %d\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(imbr->imbr_id),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(imbr->imbr_id),
                      (int)imbr->imbr_count));
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_dec
 *
 * Decrements in-memory reference count for a BLOB reference descriptor
 *
 * Parameters:
 *      imbr - in, use
 *          pointer to in-memory BLOB reference descriptor
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static size_t inmemoryblobrefrec_dec(inmemoryblobrefrec_t* imbr)
{
        ss_beta(CHK_IMBR(imbr));
        ss_dassert(imbr->imbr_count);
        imbr->imbr_count--;
        ss_dprintf_1(("inmemoryblobrefrec_dec(id=0x%08lX%08lX) count = %d\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(imbr->imbr_id),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(imbr->imbr_id),
                      (int)imbr->imbr_count));
        return (imbr->imbr_count);
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_getpersistentrefcount
 *
 * Gets persistent reference count for a BLOB if both conditions apply:
 * 1. The record is not locked.
 * 2. Persistent reference count is known
 *
 * Parameters:
 *      imbr - in, use
 *          in-memory ref. record
 *
 *      p_persistent_count - out, use
 *          pointer to 32-bit  variable where the
 *          persistent ref count will be stored (if successful)
 *
 * Return value:
 *       TRUE when valid persistent ref. count was gotten
 *       FALSE when persistent count not gotten
 *
 * Limitations:
 *
 * Globals used:
 */
static bool inmemoryblobrefrec_getpersistentrefcount(
        inmemoryblobrefrec_t* imbr,
        ss_int4_t* p_persistent_count)
{
        ss_beta(CHK_IMBR(imbr));
        ss_debug(*p_persistent_count = 0xDeadBeef);
        if (imbr->imbr_locked) {
            return (FALSE);
        }
        if (!imbr->imbr_persistent_count_known) {
            return (FALSE);
        }
        *p_persistent_count = imbr->imbr_persistent_count_if_known;
        return (TRUE);
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_getpersistentrefcount_lockedbymyself
 *
 * Aame as inmemoryblobrefrec_getpersistentrefcount,
 * but now we know the record is locked by
 * the same thread making this call
 *
 * Parameters:
 *      imbr - in, use
 *          in-memory blob ref. record
 *
 *      p_persistent_count - out, use
 *          pointer to 32-bit variable where the
 *          persistent count will be stored
 *
 * Return value:
 *       TRUE - persistent count gotten.
 *       FALSE - persistent count is not known.
 *
 * Limitations:
 *
 * Globals used:
 */
static bool inmemoryblobrefrec_getpersistentrefcount_lockedbymyself(
        inmemoryblobrefrec_t* imbr,
        ss_int4_t* p_persistent_count)
{
        ss_beta(CHK_IMBR(imbr));
        ss_debug(*p_persistent_count = 0xDeadBeef);
        ss_dassert(imbr->imbr_locked);
        if (!imbr->imbr_persistent_count_known) {
            return (FALSE);
        }
        *p_persistent_count = imbr->imbr_persistent_count_if_known;
        return (TRUE);
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_setpersistentrefcount
 *
 * Sets persistent reference count value for in-memory BLOB ref. record.
 * This also sets the the value "known".
 *
 * Parameters:
 *      imbr - out, use
 *          in-memory BLOB ref. record whose persistent ref count is to
 *          be set
 *
 *      persistent_refcount - in
 *          value to set as persistent refcount
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void inmemoryblobrefrec_setpersistentrefcount(
        inmemoryblobrefrec_t* imbr,
        ss_int4_t persistent_refcount)
{
        ss_beta(CHK_IMBR(imbr));
        ss_rc_dassert(persistent_refcount >= 0, persistent_refcount);
        imbr->imbr_persistent_count_known = TRUE;
        imbr->imbr_persistent_count_if_known = persistent_refcount;
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_addtowaitlist
 *
 * Links the ref-record to wait list to wait for a lock for the BLOB
 * reference record to be released.
 *
 * Parameters:
 *      imbr - in out, use
 *          pointer to in-memory blob ref record
 *
 *      meslist - in out, use
 *          message list object where the message object can be "allocated"
 *          from
 *
 * Return value - "give":
 *      pointer to su_mes_t object which can be used to wait for the
 *      lock to be released. (the object is later to be freed by
 *      giving it back to the meslist container)
 *
 * Limitations:
 *
 * Globals used:
 */
static su_mes_t* inmemoryblobrefrec_addtowaitlist(
        inmemoryblobrefrec_t* imbr,
        su_meslist_t* meslist)
{
        su_mes_t* mes;

        ss_beta(CHK_IMBR(imbr));
        mes = su_meslist_mesinit(meslist);
        su_meswaitlist_add(imbr->imbr_meswaitlist, mes);
        return (mes);
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_lock
 *
 * Tries to lock a in-memory blob ref record, fails if it is
 * already locked.
 *
 * Parameters:
 *      imbr - in out, use
 *          in-memory blob ref record
 *
 * Return value:
 *       TRUE - locking successful
 *       FALSE - already locked (by another thread)
 *
 * Limitations:
 *
 * Globals used:
 */
static bool inmemoryblobrefrec_lock(
        inmemoryblobrefrec_t* imbr)
{
        ss_beta(CHK_IMBR(imbr));
        if (imbr->imbr_locked) {
            return (FALSE);
        }
        imbr->imbr_locked = TRUE;
        FAKE_CODE_BLOCK(
                FAKE_TAB_YIELDWHENBLOBG2REFLOCKED,
        {
            SET_FAKE(FAKE_TAB_YIELDWHENBLOBG2REFLOCKED, 100);
            SsThrSleep(1000);
        });

        return (TRUE);
}

/*#***********************************************************************\
 *
 *      inmemoryblobrefrec_unlock
 *
 * unlocks in-mem blob ref record
 *
 * Parameters:
 *      imbr - out, use
 *          pointer to the ref record
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void inmemoryblobrefrec_unlock(
        inmemoryblobrefrec_t* imbr)
{
        ss_beta(CHK_IMBR(imbr));
        ss_dassert(imbr->imbr_locked);
        imbr->imbr_locked = FALSE;
        su_meswaitlist_wakeupall(imbr->imbr_meswaitlist);
}


/*#***********************************************************************\
 *
 *      blobg2_resetsysblobscursor
 *
 * Reset a Tli cursor for SYS_BLOBS table. It allows rebinding
 * columns. Currently Tli cursors do not have a reset method and the
 * the reset is done by dropping old and creating a new cursor.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      tcon - in, use
 *          Tli connection
 *
 *      p_tcur - in out, use
 *          pointer to pointer to Tli cursor object
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void blobg2_resetsysblobscursor(
                    rs_sysi_t* cd __attribute__ ((unused)),
                    TliConnectT* tcon,
                    TliCursorT** p_tcur)
{
        if (*p_tcur != NULL) {
            TliCursorFree(*p_tcur);
        }
        *p_tcur = TliCursorCreate(tcon,
                                  RS_AVAL_DEFCATALOG,
                                  RS_AVAL_SYSNAME,
                                  (char *)RS_RELNAME_BLOBS);
        ss_dassert(*p_tcur != NULL);
}

/*#***********************************************************************\
 *
 *      blobg2_initsysblobsconnectionandcursor
 *
 * Initializes Tli connection and cursor for SYS_BLOBS table
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      p_tcon - out, give
 *          pointer to pointer where to put the address of the new
 *          Tli connection
 *
 *      p_tcur - out, give
 *          pointer to pointer where to put the address of the new
 *          Tli cursor
 *
 *      readonlyp - in
 *          TRUE when a read-only cursor is requested, FALSE otherwise.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void blobg2_initsysblobsconnectionandcursor(
        rs_sysi_t* cd,
        TliConnectT** p_tcon,
        TliCursorT** p_tcur,
        bool readonlyp)
{
        tb_trans_t* trans;
        rs_sysi_t* syscd;

        *p_tcon = TliConnectInit(cd);
        TliConnectSetAppinfo(*p_tcon, (char *)"blobg2_initsysblobsconnectionandcursor");
        ss_dassert(*p_tcon != NULL);
        syscd = TliGetCd(*p_tcon);
        trans = TliGetTrans(*p_tcon);
        tb_trans_settransoption(syscd, trans, TB_TRANSOPT_USEMAXREADLEVEL);
        if (readonlyp) {
            tb_trans_settransoption(syscd, trans, TB_TRANSOPT_READONLY);
        } else {
            tb_trans_settransoption(syscd, trans, TB_TRANSOPT_NOLOGGING);
            /* JarmoR removed Apr 4, 2002
                tb_trans_settransoption(syscd, trans, TB_TRANSOPT_NOHSB); */
#ifdef SS_HSBG2
            tb_trans_settransoption(syscd, trans, TB_TRANSOPT_NOCHECK);
            tb_trans_beginif(syscd, trans);
            if (!dbe_cfg_splitpurge) {
                tb_trans_setforcecommit(syscd, trans);
            }
#endif
        }
        *p_tcur = TliCursorCreate(*p_tcon,
                                  RS_AVAL_DEFCATALOG,
                                  RS_AVAL_SYSNAME,
                                  (char *)RS_RELNAME_BLOBS);
        ss_dassert(*p_tcur != NULL);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_donesysblobsconnectionandcursor
 *
 * frees sysblobs cursor and connection
 *
 * Parameters:
 *      p_tcon - in out, take
 *          pointer to pointer to TliConnectT object,
 *          NULL is assigned to *p_tcon
 *
 *      p_tcur - in out, take
 *          pointer to pointer to TliCursorT object,
 *          NULL is assigned to *p_tcur
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void blobg2mgr_donesysblobsconnectionandcursor(
        TliConnectT** p_tcon,
        TliCursorT** p_tcur)
{
        if (NULL != *p_tcur) {
            TliCursorFree(*p_tcur);
            *p_tcur = NULL;
        }
        if (NULL != *p_tcon) {
            TliConnectDone(*p_tcon);
            *p_tcon = NULL;
        }
}

/* This structure is the Blob G2 Manager object. Only one instance
 * per database is needed
 */
struct tb_blobg2mgr_st {
        int bm_check;
        dbe_db_t* bm_db; /* ref to dbe level database */
        SsSemT* bm_mutex; /* for serializing shared access */
        su_list_t* bm_wblobs; /* list of active BLOB write streams */
        su_rbt_t* bm_inmemoryrefs; /* pool of in-memory BLOB references */
        su_meslist_t* bm_meslist; /* message semaphores for lock waits */
        size_t bm_keycmplen; /* number of data bytes to store to blob reference va */
        bool bm_hsb;
};

#define CHK_BM(bm) \
        ss_assert((bm) != NULL);\
        ss_rc_assert((bm)->bm_check == TBCHK_BLOBG2MGR, (bm)->bm_check);

/* Write blob stream object */
struct tb_wblobg2stream_st {
        int              wbs_check;
        dbe_db_t*        wbs_db; /* ref to database object */
        tb_blobg2mgr_t*  wbs_bm; /* ref to blob manager */
        su_list_node_t*  wbs_bm_listpos; /* backlink to list position in BM */
        dbe_cpnum_t      wbs_startcpnum; /* start checkpoint level */
        dbe_blobg2id_t   wbs_id;         /* BLOB ID */
        dbe_blobg2size_t wbs_loggedsize; /* number of bytes already logged */
        dbe_blobg2size_t wbs_size;       /* number of bytes received total */
        rs_sysi_t*       wbs_cd;         /* client context */
        rs_atype_t*      wbs_atype;      /* ref to column type */
        rs_aval_t*       wbs_aval;       /* ref to column value */
        refdva_t         wbs_rdva; /* raw storage for BLOB header */
        ss_byte_t*       wbs_reachbuf; /* pointer to latest reach buffer or NULL */
        dbe_blobg2size_t wbs_startpos_of_pagearray; /* byte position */
        size_t           wbs_num_pages_in_array;
        bpagerec_t       wbs_pagearray[BS_PAGEARR_SIZE]; /* page record "cache" */
        ss_uint4_t       wbs_num_pagearrays_saved;
        TliConnectT*     wbs_sysconnect; /* connection to table level */
        TliCursorT*      wbs_sysblobscursor; /* cursor to SYS_BLOBS */
        dbe_wblobg2_t*   wbs_dbewblob;   /* dbe level blob write stream */

        /* these are for preallocation of pages */
        size_t           wbs_prealloc_array_size;
        size_t           wbs_prealloc_array_num_idx_in_use;
        size_t           wbs_prealloc_array_pos;
        su_daddr_t*      wbs_prealloc_array;

        bs_status_t      wbs_status; /* reach/release status */
};

/* forward declaration of static routines */
static tb_wblobg2stream_t* wblobg2stream_init(
        void* cd,
        tb_blobg2mgr_t* bm,
        rs_atype_t* atype,
        rs_aval_t* aval);

/* forward declaration of static routines */
static su_ret_t wblobg2stream_flush(
        rs_sysi_t* cd_if_checkpoint,
        tb_wblobg2stream_t* wbs,
        flushmode_t flushmode,
        su_err_t** p_errh);

static su_ret_t blobg2mgr_incrementinmemoryrefcount_byid(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

static su_ret_t blobg2mgr_decrementinmemoryrefcount_byid(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_init
 *
 * Creates a BLOB Manager and initializes dbe-level callbacks
 * to point to appropriate tab-level routines
 *
 * Parameters:
 *      db - in, hold
 *          pointer to database object
 *
 * Return value - give:
 *       The BLOB manager
 *
 * Limitations:
 *
 * Globals used:
 */
tb_blobg2mgr_t* tb_blobg2mgr_init(dbe_db_t* db)
{
        tb_blobg2mgr_t* bm = SSMEM_NEW(tb_blobg2mgr_t);

        dbe_blobg2callback_move_page =
            tb_blobg2mgr_move_page;
        dbe_blobg2callback_incrementinmemoryrefcount =
            blobg2mgr_incrementinmemoryrefcount_byid;
        dbe_blobg2callback_decrementinmemoryrefcount =
            blobg2mgr_decrementinmemoryrefcount_byid;
        dbe_blobg2callback_incrementpersistentrefcount_byva =
            tb_blobg2mgr_incrementpersistentrefcount_byva;
        dbe_blobg2callback_decrementpersistentrefcount =
            tb_blobg2mgr_decrementpersistentrefcount_callback;
        dbe_blobg2callback_wblobinit =
            tb_blobg2mgr_initwblobstream_bycd;
        dbe_blobg2callback_wblobinit_for_recovery =
            tb_blobg2mgr_initwblobstream_for_recovery_bycd;
        dbe_blobg2callback_wblobreach =
            tb_wblobg2stream_reach;
        dbe_blobg2callback_wblobrelease =
            tb_wblobg2stream_release;
        dbe_blobg2callback_wblobdone =
            tb_wblobg2stream_done;
        dbe_blobg2callback_wblobabort =
            tb_wblobg2stream_abort;
        dbe_blobg2callback_delete_unreferenced_blobs_after_recovery =
            tb_blobg2mgr_delete_unreferenced_blobs_after_recovery;
        dbe_blobg2callback_copy_old_blob_to_g2 =
            tb_blobg2mgr_copy_old_blob_to_g2;
        rs_aval_globalinstallblobrefcallbacks(
                tb_blobg2mgr_incrementinmemoryrefcount_byva,
                tb_blobg2mgr_decrementinmemoryrefcount_byva,
                dbe_brefg2_nullifyblobid_from_va,
                dbe_brefg2_isblobg2check_from_aval,
                dbe_brefg2_getsizefromaval,
                dbe_brefg2_getidfromaval);
        bm->bm_check = TBCHK_BLOBG2MGR;
        bm->bm_db = db;
        bm->bm_mutex = SsSemCreateLocal(SS_SEMNUM_BLOBG2MGR);
        bm->bm_wblobs = su_list_init(NULL);
        bm->bm_inmemoryrefs =
            su_rbt_inittwocmp(
                    (int(*)(void*,void*))inmemoryblobrefrec_insertcmp,
                    (int(*)(void*,void*))inmemoryblobrefrec_searchcmp,
                    (void (*)(void*))inmemoryblobrefrec_done);
        bm->bm_meslist = su_meslist_init_nomutex(NULL);
        bm->bm_keycmplen = RS_KEY_MAXCMPLEN;
        bm->bm_hsb = FALSE;
        blobg2id_null = DBE_BLOBG2ID_NULL;
        return (bm);
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_done
 *
 * frees BLOB Manager
 *
 * Parameters:
 *      bm - in, take
 *          BLOB Manager
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void tb_blobg2mgr_done(tb_blobg2mgr_t* bm)
{
        CHK_BM(bm);
        bm->bm_check = TBCHK_FREEDBLOBG2MGR;
#ifdef SS_DEBUG
        {
            ulong len = su_list_length(bm->bm_wblobs);
            if (len != 0) {
                su_list_node_t* ln = su_list_first(bm->bm_wblobs);
                while (ln != NULL) {
                    tb_wblobg2stream_t* wbs = su_listnode_getdata(ln);
                    ss_dprintf_1(("unreleased wblob (id=0x%08lX%08lX)\n",
                        (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(wbs->wbs_id),
                        (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(wbs->wbs_id)));
                    ln = su_list_next(bm->bm_wblobs, ln);
                }
            }
        }
#endif /* SS_DEBUG */
        ss_dassert(su_list_length(bm->bm_wblobs) == 0);
        ss_debug(
            if (su_rbt_nelems(bm->bm_inmemoryrefs) > 0) {
                SsMemChkPrintList();
            });
        su_list_done(bm->bm_wblobs);
        su_rbt_done(bm->bm_inmemoryrefs);
        su_meslist_done(bm->bm_meslist);
        SsSemFree(bm->bm_mutex);
        SsMemFree(bm);
}

/*##**********************************************************************\
 *
 *		tb_blobg2mgr_sethsb
 *
 * Sets HSB mode on or off for blob manager.
 *
 * Parameters :
 *
 *		bm -
 *
 *
 *		hsb -
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
void tb_blobg2mgr_sethsb(tb_blobg2mgr_t* bm, bool hsb)
{
        CHK_BM(bm);

        ss_dprintf_1(("tb_blobg2mgr_sethsb:hsb=%d\n", hsb));

        bm->bm_hsb = hsb;
}

/*#***********************************************************************\
 *
 *      blobg2mgr_getkeycmplen
 *
 * Get compare length of key values of BLOB columns
 *
 * Parameters:
 *      bm - in, use
 *          BLOB manager
 *
 * Return value:
 *
 *
 * Limitations:
 *
 * Globals used:
 */
static size_t blobg2mgr_getkeycmplen(tb_blobg2mgr_t* bm)
{
        ss_beta(CHK_BM(bm));
        return (bm->bm_keycmplen);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_enteraction
 *
 * enters dbe action gate (in shared mode)
 *
 * Parameters:
 *      bm - in, use
 *          BLOB Manager
 *
 *      cd - in out, use
 *          Client context
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void blobg2mgr_enteraction(tb_blobg2mgr_t* bm, rs_sysi_t* cd)
{
        ss_beta(CHK_BM(bm));
        ss_dprintf(("blobg2mgr_enteraction\n"));
        dbe_db_enteraction(bm->bm_db, cd);
        ss_dprintf(("blobg2mgr_enteraction entered.\n"));
}

/*#***********************************************************************\
 *
 *      blobg2mgr_exitaction
 *
 * Exits dbe action gate
 *
 * Parameters:
 *      bm - in, use
 *          BLOB Manager
 *
 *      cd - in out, use
 *          Client context
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void blobg2mgr_exitaction(tb_blobg2mgr_t* bm, rs_sysi_t* cd)
{
        ss_beta(CHK_BM(bm));
        dbe_db_exitaction(bm->bm_db, cd);
        ss_dprintf(("blobg2mgr_exitaction ready.\n"));
}


/*##**********************************************************************\
 *
 *      tb_blobg2mgr_blobdeletebyid_noenteraction
 *
 * deletes a BLOB by its id but this assumes the dbe action is already
 * entered
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB manager
 *
 *      bid - in
 *          blob id
 *
 *      p_errh - out, give
 *          if non-NULL and error occurs, a newly allocated error handle
 *          is put to *p_errh
 *
 * Return value:
 *       SU_SUCCESS or error code when failed
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_blobdeletebyid_noenteraction(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        uint rc = SU_SUCCESS;
        TliRetT tlirc;
        TliConnectT* tcon;
        TliCursorT* tcur;
        ss_int4_t refcount;
        ss_int4_t startcpnum_bind;
        ss_uint4_t rowcount;
        dbe_cpnum_t startcpnum = 0;
        size_t numpages;
        uint i;
        char colname[16];
        su_daddr_t pagearr[BS_PAGEARR_SIZE];
        ss_debug(dbe_blobg2size_t startpos);

        ss_beta(CHK_BM(bm));
        ss_dprintf_1(("tb_blobg2mgr_blobdeletebyid_noenteraction(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        blobg2_initsysblobsconnectionandcursor(cd, &tcon, &tcur, FALSE);
        tlirc = TliCursorConstrInt8t(tcur, RS_ANAME_BLOBS_ID, TLI_RELOP_EQUAL, bid);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);

        tlirc = TliCursorColInt4t(tcur,
                                 RS_ANAME_BLOBS_REFCOUNT,
                                 &refcount);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColInt4t(tcur,
                                 RS_ANAME_BLOBS_STARTCPNUM,
                                 &startcpnum_bind);
        tlirc = TliCursorColSizet(tcur, RS_ANAME_BLOBS_NUMPAGES, &numpages);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        ss_debug(
            tlirc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_STARTPOS, &startpos);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc););
        for (i = 0; i < BS_PAGEARR_SIZE; i++) {
            ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ADDR_TEMPL));
            SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ADDR_TEMPL, i + 1);
            ss_dassert(sizeof(colname) > strlen(colname));
            tlirc = TliCursorColInt4t(tcur, colname, (ss_int4_t*)&pagearr[i]);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        }
        tlirc = TliCursorOpen(tcur);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        for (rowcount = 0;;) {
            tlirc = TliCursorNext(tcur);
            if (tlirc == TLI_RC_END) {
                break;
            }
            if (tlirc != TLI_RC_SUCC) {
                goto cursorop_failed_cleanup;
            }
            rowcount++;
            if (rowcount == 1) {
                startcpnum = (dbe_cpnum_t)startcpnum_bind;
                ss_rc_dassert(refcount == 0, (int)refcount);
                ss_dassert(SsInt8Is0(startpos));
            }
            tlirc = TliCursorDelete(tcur);
            if (tlirc != TLI_RC_SUCC) {
                goto cursorop_failed_cleanup;
            }
            rc = dbe_db_free_n_pages(bm->bm_db,
                                     numpages,
                                     pagearr,
                                     startcpnum,
                                     FALSE);
            if (rc != SU_SUCCESS) {
                goto free_failed_cleanup;
            }
        }
        tlirc = TliCommit(tcon);
        if (tlirc != TLI_RC_SUCC) {
            goto connectop_failed_cleanup;
        }
 cursor_cleanup:;
        blobg2mgr_donesysblobsconnectionandcursor(&tcon, &tcur);
        ss_dprintf_1(("tb_blobg2mgr_blobdeletebyid_noenteraction(id=0x%08lX%08lX) return (rc=%d\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                      (int)rc));
        return (rc);
 cursorop_failed_cleanup:;
        {
            ss_debug(bool succp =) TliCursorCopySuErr(tcur, p_errh);
            ss_dassert(succp);
            ss_debug(succp =) TliCursorErrorInfo(tcur, NULL, &rc);
            ss_dassert(succp);
            ss_rc_dassert(rc != SU_SUCCESS, rc);
            ss_rc_derror(rc); /* should not fail, really! */
        }
 trx_rollback:;
        TliRollback(tcon);
        goto cursor_cleanup;
 free_failed_cleanup:;
        su_err_init_noargs(p_errh, rc);
        goto trx_rollback;
 connectop_failed_cleanup:;
        TliConnectCopySuErr(tcon, p_errh);
        rc = TliErrorCode(tcon);
        goto trx_rollback;
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_blobdeletebyid
 *
 * deletes a BLOB by its id
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB manager
 *
 *      bid - in
 *          blob id
 *
 *      p_errh - out, give
 *          if non-NULL and error occurs, a newly allocated error handle
 *          is put to *p_errh
 *
 * Return value:
 *       SU_SUCCESS or error code when failed
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_blobdeletebyid(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        su_ret_t rc;

        blobg2mgr_enteraction(bm, cd);
        rc = tb_blobg2mgr_blobdeletebyid_noenteraction(cd, bm, bid, p_errh);
        blobg2mgr_exitaction(bm, cd);
        return (rc);
}


/*##**********************************************************************\
 *
 *      tb_blobg2mgr_incrementinmemoryrefcount
 *
 * increments the in-memory reference count for a BLOB
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_errh - out, give
 *          if non-NULL and error occurs, a newly allocated error handle
 *          is put to *p_errh
 *
 * Return value:
 *       SU_SUCCESS or error code when failed
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t  tb_blobg2mgr_incrementinmemoryrefcount(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh __attribute__ ((unused)))
{
        su_ret_t rc = SU_SUCCESS;
        inmemoryblobrefrec_t* imbr;
        su_rbt_node_t* rbtn;
        CHK_BM(bm);

        ss_dprintf_1(("tb_blobg2mgr_incrementinmemoryrefcount(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        if (DBE_BLOBG2ID_CMP(bid, blobg2id_null) == 0) {
            /* special case for DBE_BLOBG2ID_NULL: ignore */
            return (rc);
        }
        SsSemEnter(bm->bm_mutex);
        rbtn = su_rbt_search(bm->bm_inmemoryrefs, &bid);
        if (rbtn == NULL) {
            imbr = inmemoryblobrefrec_init(bid, 1, FALSE);
            su_rbt_insert(bm->bm_inmemoryrefs, imbr);
        } else {
            imbr = su_rbtnode_getkey(rbtn);
            inmemoryblobrefrec_inc(imbr);
        }
        SsSemExit(bm->bm_mutex);
        ss_dprintf_1(("tb_blobg2mgr_incrementinmemoryrefcount(id=0x%08lX%08lX) count=%d return (rc=%d)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                      (int)imbr->imbr_count,
                      (int)rc));
        return (rc);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_fetchblobheaderinfo
 *
 * Fetches information from BLOB header (which is the first row with
 * the given BLOB ID in SYS_BLOBS)
 *
 * Parameters:
 *      cd - in, use
 *          client cntext
 *
 *      bm - in, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_refcount_out_or_NULL - out, use
 *          if non-NULL the persistent ref count is put
 *          to *p_refcount_out_or_NULL
 *
 *      p_startcpnum_out_or_NULL - out, use
 *          if non-NULL the start checkpoint number is put
 *          to *p_startcpnum_out_or_NULL
 *
 *      p_size_out_or_NULL - out, use
 *          if non-NULL the size is put
 *          to *p_size_out_or_NULL
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t blobg2mgr_fetchblobheaderinfo(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm __attribute__ ((unused)),
        dbe_blobg2id_t bid,
        ss_int4_t* p_refcount_out_or_NULL,
        dbe_cpnum_t* p_startcpnum_out_or_NULL,
        dbe_blobg2size_t* p_size_out_or_NULL)
{
        su_ret_t rc = SU_SUCCESS;
        TliRetT tlirc;
        TliConnectT* tcon;
        TliCursorT* tcur;
        ss_int4_t refcount;
        ss_int4_t startcpnum;
        dbe_blobg2size_t size;
        union {
                ss_int4_t refcount;
                dbe_cpnum_t cpnum;
                dbe_blobg2size_t size;
        } dummy;

        ss_dprintf_2(("blobg2mgr_fetchblobheaderinfo(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));

        ss_dassert(p_refcount_out_or_NULL != NULL ||
                   p_startcpnum_out_or_NULL != NULL ||
                   p_size_out_or_NULL != NULL);
        ss_debug(CHK_BM(bm));
        blobg2_initsysblobsconnectionandcursor(cd, &tcon, &tcur, TRUE);
        if (p_refcount_out_or_NULL != NULL) {
            tlirc = TliCursorColInt4t(tcur,
                                      RS_ANAME_BLOBS_REFCOUNT,
                                      &refcount);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        } else {
            ss_purify(refcount = 0);
            p_refcount_out_or_NULL = &dummy.refcount;
        }
        if (p_startcpnum_out_or_NULL != NULL) {
            tlirc = TliCursorColInt4t(tcur,
                                      RS_ANAME_BLOBS_STARTCPNUM,
                                      &startcpnum);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        } else {
            ss_purify(startcpnum = DBE_CPNUM_NULL);
            p_startcpnum_out_or_NULL = &dummy.cpnum;
        }
        if (p_size_out_or_NULL != NULL) {
            tlirc = TliCursorColInt8t(tcur,
                                      RS_ANAME_BLOBS_TOTALSIZE,
                                      &size);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        } else {
            ss_purify(DBE_BLOBG2SIZE_ASSIGN_SIZE(&size, 0));
            p_size_out_or_NULL = &dummy.size;
        }
        tlirc = TliCursorConstrInt8t(tcur, RS_ANAME_BLOBS_ID, TLI_RELOP_EQUAL, bid);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorConstrInt8t(tcur,
                                    RS_ANAME_BLOBS_STARTPOS,
                                    TLI_RELOP_EQUAL,
                                    SsInt8InitFrom2Uint4s(0,0));
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorOpen(tcur);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorNext(tcur);
        if (tlirc == TLI_RC_END) {
            rc = DBE_RC_END;
        } else {
            ss_rc_assert(tlirc == TLI_RC_SUCC, tlirc);
        }
        tlirc = TliCommit(tcon);
        ss_rc_assert(tlirc == TLI_RC_SUCC, tlirc);
        blobg2mgr_donesysblobsconnectionandcursor(&tcon, &tcur);
        *p_refcount_out_or_NULL = refcount;
        *p_startcpnum_out_or_NULL = startcpnum;
        *p_size_out_or_NULL = size;
        return (rc);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_fetchsysblobsrefcount
 *
 * Gets persistent ref count for a BLOB
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 * Return value:
 *       persistent reference count
 *
 * Limitations:
 *
 * Globals used:
 */
#if 0 /* pete removed, not inuse */
static ss_int4_t blobg2mgr_fetchsysblobsrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid)
{
        su_ret_t rc;
        ss_int4_t refcount;

        ss_beta(CHK_BM(bm));
        ss_dprintf_1(("blobg2mgr_fetchsysblobsrefcount(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        rc = blobg2mgr_fetchblobheaderinfo(
                cd,
                bm,
                bid,
                &refcount,
                NULL,
                NULL);
        ss_rc_assert(rc == SU_SUCCESS, rc);
        ss_dprintf_1(("blobg2mgr_fetchsysblobsrefcount(id=0x%08lX%08lX) return (refcount=%lu)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                      (ulong)refcount));
        return (refcount);
}
#endif /* 0 */

/*#***********************************************************************\
 *
 *      blobg2mgr_decrementinmemoryrefcount_nodelete_nomutex
 *
 * decrements in-memory reference count but does NOT delete BLOB
 * even if the count reaches 0. Also the bm mutex is not entered
 * (assumes it is already entered)
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          blob manager
 *
 *      bid - in
 *          BLOB ID
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void blobg2mgr_decrementinmemoryrefcount_nodelete_nomutex(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid)
{
        inmemoryblobrefrec_t* imbr;
        su_rbt_node_t* rbtn;
        CHK_BM(bm);

        ss_dprintf_1(("tb_blobg2mgr_decrementinmemoryrefcount_nodelete_nomutex(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        rbtn = su_rbt_search(bm->bm_inmemoryrefs, &bid);
        if (rbtn == NULL) {
            ss_derror;
            return;
        } else {
            size_t count;
            imbr = su_rbtnode_getkey(rbtn);
            count = inmemoryblobrefrec_dec(imbr);
            if (count == 0) {
                ss_dprintf_1(("tb_blobg2mgr_decrementinmemoryrefcount_nodelete_nomutex(id=0x%08lX%08lX) count = %lu\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                      (ulong)count));
                ss_dassert(imbr->imbr_persistent_count_known
                           && imbr->imbr_persistent_count_if_known == 0);
                su_rbt_delete(bm->bm_inmemoryrefs, rbtn);
            }
        }
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_decrementinmemoryrefcount
 *
 * decrements in-memory reference count for a BLOB
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          blob manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_errh - out, give
 *          if non-NULL and error occurs a newly allocated error handle is
 *          put to *p_errh
 *
 * Return value:
 *       SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_decrementinmemoryrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        bool needtodelete_status_known = FALSE;
        inmemoryblobrefrec_t* imbr;
        su_rbt_node_t* rbtn;

        CHK_BM(bm);

        ss_dprintf_1(("tb_blobg2mgr_decrementinmemoryrefcount(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        if (DBE_BLOBG2ID_CMP(bid, blobg2id_null) == 0) {
            /* special case for DBE_BLOBG2ID_NULL: ignore */
            return (rc);
        }
        SsSemEnter(bm->bm_mutex);
        rbtn = su_rbt_search(bm->bm_inmemoryrefs, &bid);
        if (rbtn == NULL) {
            SsSemExit(bm->bm_mutex);
            ss_derror;
        } else {
            ss_int4_t persistent_count = 0;
            ss_int4_t count;
            bool newblob;
            imbr = su_rbtnode_getkey(rbtn);
            newblob = imbr->imbr_newblob;
            for (;;) {
                count = (ss_int4_t)inmemoryblobrefrec_dec(imbr);
                if (count == 0) {
                    needtodelete_status_known =
                        inmemoryblobrefrec_getpersistentrefcount(imbr, &persistent_count);
                    if (!needtodelete_status_known) {
                        /* either the persistent count has not been fetched or else
                         * the persistent count record is locked!
                         * try to lock it!
                         */
                        inmemoryblobrefrec_inc(imbr);
                        if (!inmemoryblobrefrec_lock(imbr)) {
                            su_mes_t* mestowait =
                                inmemoryblobrefrec_addtowaitlist(imbr, bm->bm_meslist);
                            ss_dassert(mestowait != NULL);
                            SsSemExit(bm->bm_mutex);
                            su_mes_wait(mestowait);
                            SsSemEnter(bm->bm_mutex);
                            su_meslist_mesdone(bm->bm_meslist, mestowait);
                            /* continue for loop */
                        } else {
                            size_t count2;
                            needtodelete_status_known =
                                inmemoryblobrefrec_getpersistentrefcount_lockedbymyself(
                                        imbr,
                                        &persistent_count);
                            inmemoryblobrefrec_unlock(imbr);
                            count2 = inmemoryblobrefrec_dec(imbr);
                            ss_dassert(count2 == (size_t)count);
                            count = (ss_int4_t)count2;
                            break;
                        }
                        /* continue */
                    } else {
                        break;
                    }
                } else {
                    /* not last in-memory reference, we can just proceed */
                    break;
                }
            }
            /* remove the in-memory record because there are no more
             * in-memory references and it is uncertain when the next in-mem
             * reference is taken, leaving this record for caching purposes might
             * result in too big memory usage.
             */
            if (count == 0) {
                dbe_db_t* db;

                su_rbt_delete(bm->bm_inmemoryrefs, rbtn);
                db = bm->bm_db;
                SsSemExit(bm->bm_mutex);

                if (newblob) {
                    su_ret_t dbe_rc;

                    /* JarmoR Aug 20, 2003 We just ignore the return code for now because
                     * error handling is very difficult (called from rs_aval_free) and secondary
                     * should be able to handle the error even if logging fails.
                     */
                    ss_dprintf_2(("tb_blobg2mgr_decrementinmemoryrefcount:dbe_db_logblobg2dropmemoryref\n"));
                    dbe_rc = dbe_db_logblobg2dropmemoryref(cd, db, bid);
                    ss_dprintf_2(("tb_blobg2mgr_decrementinmemoryrefcount:rc=%d\n", dbe_rc));
                }
                if (needtodelete_status_known &&
                    persistent_count == 0 &&
                    rc == SU_SUCCESS)
                {
                    /* also persistent memory reference count is 0,
                     * physical deletion is needed
                     */
                    rc = tb_blobg2mgr_blobdeletebyid(cd, bm, bid, p_errh);
                }
            } else {
                SsSemExit(bm->bm_mutex);
            }
        }
        ss_dprintf_1(("tb_blobg2mgr_decrementinmemoryrefcount(id=0x%08lX%08lX) return (rc=%d)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                      (int)rc));
        return (rc);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_lockblobforaccess
 *
 * Locks the in-memory reference record for BLOB for access.
 * This is needed to serialize access to reference counts
 *
 * Parameters:
 *      bm - in out, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_rbtn - out, ref
 *          pointer where to store pointer to rb-tree node
 *          which contains the found in-memory blob ref. record
 *
 * Return value - ref:
 *       found in-memory BLOB reference record which is also locked
 *
 * Limitations:
 *
 * Globals used:
 */
static inmemoryblobrefrec_t* blobg2mgr_lockblobforaccess(
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_rbt_node_t** p_rbtn)
{
        inmemoryblobrefrec_t* imbr;
        su_rbt_node_t* rbtn;

        ss_dprintf_3(("blobg2mgr_lockblobforaccess\n"));
        CHK_BM(bm);
        SsSemEnter(bm->bm_mutex);
        rbtn = su_rbt_search(bm->bm_inmemoryrefs, &bid);
        if (rbtn == NULL) {
            imbr = inmemoryblobrefrec_init(bid, 1, FALSE);
            rbtn = su_rbt_insert2(bm->bm_inmemoryrefs, imbr);
        } else {
            imbr = su_rbtnode_getkey(rbtn);
            /* to make sure nobody else will remove the record meanwhile */
            inmemoryblobrefrec_inc(imbr);
        }
        if (p_rbtn != NULL) {
            *p_rbtn = rbtn;
        }
        for (;;) {
            if (!inmemoryblobrefrec_lock(imbr)) {
                su_mes_t* mestowait =
                    inmemoryblobrefrec_addtowaitlist(imbr, bm->bm_meslist);
                ss_dassert(mestowait != NULL);
                SsSemExit(bm->bm_mutex);
                ss_dprintf_3(("blobg2mgr_lockblobforaccess:wait\n"));
                su_mes_wait(mestowait);
                SsSemEnter(bm->bm_mutex);
                su_meslist_mesdone(bm->bm_meslist, mestowait);
                /* continue */
            } else {
                break;
            }
        }
        /* note this routine leaves the BLOB manager mutex entered! */
        ss_dprintf_3(("blobg2mgr_lockblobforaccess:got access\n"));
        return (imbr);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_add_new_blob_ref
 *
 * Adds new in-memory BLOB reference record whose in-memory
 * ref. count is 1 and persistent ref. count is known and is 0
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void blobg2mgr_add_new_blob_ref(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        bool newblob)
{
        inmemoryblobrefrec_t* imbr;
        su_rbt_node_t* rbtn;
        CHK_BM(bm);

        ss_dprintf_1(("blobg2mgr_add_new_blob_ref(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        rbtn = su_rbt_search(bm->bm_inmemoryrefs, &bid);
        if (rbtn == NULL) {
            imbr = inmemoryblobrefrec_init(bid, 1, newblob);
            inmemoryblobrefrec_setpersistentrefcount(imbr, 0);
            su_rbt_insert(bm->bm_inmemoryrefs, imbr);
        } else {
            ss_derror;
        }
}

/*#***********************************************************************\
 *
 *		wblobg2stream_insertdummyreference
 *
 * Adds a dummy reference to an already deleted blob. This is possible when
 * aborted row tries to link to a deleted blob in secondary. Because of
 * different read and merge leveles between servers the blob that is visible
 * in primary may be deleted by merge in secondary. We still need to have
 * some reference for merge process to delete in secondary.
 *
 * Parameters :
 *
 *		cd -
 *
 *
 *		bm -
 *
 *
 *		bid -
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
static void wblobg2stream_insertdummyreference(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm __attribute__ ((unused)),
        dbe_blobg2id_t bid)
{
        TliRetT tlirc;
        ss_int4_t refcount;
        ss_int4_t startcpnum;
        int complete;
        TliConnectT* tcon;
        TliCursorT* tcur;
        dbe_blobg2size_t startpos_of_pagearray;
        dbe_blobg2size_t loggedsize;
        size_t num_pages_in_array;

        ss_dprintf_1(("wblobg2stream_insertdummyreference(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        blobg2_initsysblobsconnectionandcursor(cd, &tcon, &tcur, FALSE);

        tlirc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_ID, &bid);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_STARTPOS, &startpos_of_pagearray);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_ENDSIZE, &loggedsize);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColInt(tcur,RS_ANAME_BLOBS_COMPLETE, &complete);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColInt4t(tcur,RS_ANAME_BLOBS_STARTCPNUM, &startcpnum);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColInt4t(tcur, RS_ANAME_BLOBS_REFCOUNT, &refcount);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColSizet(tcur, RS_ANAME_BLOBS_NUMPAGES, &num_pages_in_array);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);

        SsInt8Set0(&startpos_of_pagearray);
        SsInt8Set0(&loggedsize);
        complete = TRUE;
        startcpnum = 0;
        refcount = 1;
        num_pages_in_array = 0;

        tlirc = TliCursorInsert(tcur);
        ss_assert(tlirc == TLI_RC_SUCC);

        tlirc = TliCommit(tcon);
        ss_assert(tlirc == TLI_RC_SUCC);

        blobg2mgr_donesysblobsconnectionandcursor(&tcon, &tcur);
}


/*#***********************************************************************\
 *
 *      blobg2mgr_incordecsysblobsrefcount
 *
 * increments of decrements the persistent ref. count for a BLOB
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_new_persistent_count - out, use
 *          pointer to variable where to store the new persistent ref. count
 *
 *      incr_decr - in
 *          -1 for decrement or 1 for increment
 *
 *      p_errh - out,give
 *          see other p_errh descriptions above
 *
 * Return value:
 *       SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t blobg2mgr_incordecsysblobsrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        bool hsb,
        ss_int4_t* p_new_persistent_count,
        ss_int4_t incr_decr,
        su_err_t** p_errh)
{
        uint rc = SU_SUCCESS;
        TliRetT tlirc;
        TliConnectT* tcon;
        TliCursorT* tcur;
        ss_int4_t refcount;

        ss_beta(CHK_BM(bm));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        *p_new_persistent_count = 0;
        ss_rc_dassert(incr_decr == 1 || incr_decr == -1, incr_decr);
        blobg2_initsysblobsconnectionandcursor(cd, &tcon, &tcur, FALSE);
        tlirc = TliCursorColInt4t(tcur, RS_ANAME_BLOBS_REFCOUNT, &refcount);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorConstrInt8t(tcur, RS_ANAME_BLOBS_ID, TLI_RELOP_EQUAL, bid);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        FAKE_IF(FAKE_TAB_REPRODUCE_BLOBG2REFCOUNTBUG) {
            tlirc = TliCursorConstrInt8t(tcur,
                                         RS_ANAME_BLOBS_ENDSIZE,
                                         TLI_RELOP_GE,
                                         SsInt8InitFrom2Uint4s(0,0));
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        } else {
            tlirc = TliCursorConstrInt8t(tcur,
                                         RS_ANAME_BLOBS_STARTPOS,
                                         TLI_RELOP_EQUAL,
                                         SsInt8InitFrom2Uint4s(0,0));
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        }
        tlirc = TliCursorOpen(tcur);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorNext(tcur);
        if (tlirc == TLI_RC_END) {
            tlirc = TliRollback(tcon);
            blobg2mgr_donesysblobsconnectionandcursor(&tcon, &tcur);
            if (incr_decr > 0) {
                ss_dassert(incr_decr == 1);
                /* Not found for HSB increment, add an empty reference. */
                wblobg2stream_insertdummyreference(cd, bm, bid);
                *p_new_persistent_count = 1;
            }
            return(SU_SUCCESS);
        }
        ss_dprintf_1(("blobg2mgr_incordecsysblobsrefcount(id=0x%08lX%08lX): persistent refcount read from database = %d, incr_decr=%d\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                      (int)refcount, (int)incr_decr));
        ss_rc_assert(tlirc == TLI_RC_SUCC, tlirc);
        ss_dassert(!TliCursorColIsNULL(tcur, RS_ANAME_BLOBS_REFCOUNT));
        ss_rc_dassert(refcount + incr_decr >= 0, refcount + incr_decr);
        ss_rc_dassert(refcount >= 0, refcount);
        refcount += incr_decr;
        tlirc = TliCursorUpdate(tcur);
        if (tlirc != TLI_RC_SUCC) {
            ss_debug(bool succp =)
            TliCursorCopySuErr(tcur, p_errh);
            ss_dassert(succp);
            ss_derror; /* should not fail! */
            ss_debug(succp =)
            TliCursorErrorInfo(tcur, NULL, &rc);
            ss_dassert(succp);
            ss_rc_dassert(rc != SU_SUCCESS, rc);
        }
        if (rc == SU_SUCCESS) {
            tlirc = TliCommit(tcon);
            if (tlirc != TLI_RC_SUCC) {
                ss_debug(bool succp =)
                    TliConnectCopySuErr(tcon, p_errh);
                ss_dassert(succp);
                rc = TliErrorCode(tcon);
            } else {
                *p_new_persistent_count = refcount;
            }
        } else {
            tlirc = TliRollback(tcon);
        }
        blobg2mgr_donesysblobsconnectionandcursor(&tcon, &tcur);
        return (rc);
}


/*#***********************************************************************\
 *
 *      blobg2mgr_decsysblobsrefcount
 *
 * decrement persistent ref count for a BLOB
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_new_persistent_count - out, use
 *          pointer to variable where to put new ref count
 *
 *      p_errh - out, give
 *
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t blobg2mgr_decsysblobsrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        bool hsb,
        ss_int4_t* p_new_persistent_count,
        su_err_t** p_errh)
{
        su_ret_t rc;

        rc = blobg2mgr_incordecsysblobsrefcount(cd, bm, bid, hsb, p_new_persistent_count, -1, p_errh);
        return (rc);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_incsysblobsrefcount
 *
 * increment persistent ref count for a BLOB
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_new_persistent_count - out, use
 *          pointer to variable to store new persistent ref count
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t blobg2mgr_incsysblobsrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        ss_int4_t* p_new_persistent_count,
        su_err_t** p_errh)
{
        su_ret_t rc;

        rc = blobg2mgr_incordecsysblobsrefcount(cd, bm, bid, bm->bm_hsb, p_new_persistent_count, 1, p_errh);
        return (rc);
}


/*#***********************************************************************\
 *
 *      blobg2mgr_decrementpersistentrefcount
 *
 * Decrements persistent ref count for a BLOB and also deletes the
 * BLOB if both persistent and and in-memory ref count are 0.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t blobg2mgr_decrementpersistentrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        bool hsb,
        su_err_t** p_errh)
{
        su_ret_t rc;
        inmemoryblobrefrec_t* imbr;
        su_rbt_node_t* rbtn;
        ss_int4_t persistent_count;
        ss_int4_t count;

        CHK_BM(bm);
        ss_dprintf_1(("tb_blobg2mgr_decrementpersistentrefcount(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        if (DBE_BLOBG2ID_CMP(bid, blobg2id_null) == 0) {
            return (SU_SUCCESS);
        }
        /* the call below both sets Lock to imbr &  enters the
         * manager mutex also increments the in-memory reference
         * count to quarantee it will not be released meanwhile
         */
        imbr = blobg2mgr_lockblobforaccess(bm, bid, &rbtn);
        SsSemExit(bm->bm_mutex);

        rc = blobg2mgr_decsysblobsrefcount(cd, bm, bid, hsb, &persistent_count, p_errh);
        SsSemEnter(bm->bm_mutex);
        /* undo increment done in blobg2mgr_lockblobforaccess */
        count = (ss_int4_t)inmemoryblobrefrec_dec(imbr);
        if (rc == SU_SUCCESS) {
            ss_dprintf_1(("tb_blobg2mgr_decrementpersistentrefcount(id = 0x%08lX%08lX) persistent_count = %ld, inmem_count = %ld\n",
                          (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                          (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                          (long)persistent_count, count));

            inmemoryblobrefrec_setpersistentrefcount(imbr, persistent_count);
            inmemoryblobrefrec_unlock(imbr);
            if (count == 0) {
                /* remove the in-memory record because there are no more
                 * in-memory references and it is uncertain when the next in-mem
                 * reference is taken, leaving this record for caching purposes might
                 * result in too big memory usage
                 */
                su_rbt_delete(bm->bm_inmemoryrefs, rbtn);
            }
        } else {
            inmemoryblobrefrec_unlock(imbr);
        }
        SsSemExit(bm->bm_mutex);
        if (rc == SU_SUCCESS && (count + persistent_count) == 0) {
            /* both reference counts are 0,
             * physical deletion is needed
             */
            rc = tb_blobg2mgr_blobdeletebyid(cd, bm, bid, p_errh);
        }
        ss_dprintf_1(("tb_blobg2mgr_decrementpersistentrefcount(id=0x%08lX%08lX) return (rc=%d)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                      (int)rc));
        return (rc);
}

su_ret_t tb_blobg2mgr_decrementpersistentrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        su_ret_t rc;

        rc = blobg2mgr_decrementpersistentrefcount(
                cd,
                bm,
                bid,
                FALSE,
                p_errh);
        return(rc);
}

su_ret_t tb_blobg2mgr_decrementpersistentrefcount_hsb(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        su_ret_t rc;

        rc = blobg2mgr_decrementpersistentrefcount(
                cd,
                bm,
                bid,
                TRUE,
                p_errh);
        return(rc);
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_decrementpersistentrefcount_callback
 *
 * same as tb_blobg2mgr_decrementpersistentrefcount but this one digs
 * the reference to BLOB Manager from cd.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_decrementpersistentrefcount_callback(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        su_ret_t rc;
        tb_blobg2mgr_t* bm;

        bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
        rc = tb_blobg2mgr_decrementpersistentrefcount(cd, bm, bid, p_errh);
        return (rc);
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_flushallwblobs
 *
 * Flushes all open wblob streams to persistent storage to ensure
 * checkpoint consistency
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_flushallwblobs(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        su_list_node_t* ln;

        CHK_BM(bm);
        ss_dprintf_1(("tb_blobg2mgr_flushallwblobs()\n"));

        for (ln = su_list_first(bm->bm_wblobs);
             ln != NULL;
             ln = su_list_next(bm->bm_wblobs, ln))
        {
            tb_wblobg2stream_t* wbs = su_listnode_getdata(ln);
            su_ret_t rc2 = wblobg2stream_flush(cd,
                                               wbs,
                                               FLUSHMODE_CHECKPOINT,
                                               p_errh);
            if (rc2 != SU_SUCCESS) {
                if (rc == SU_SUCCESS) {
                    /* register the first error only! */
                    rc = rc2;
                    p_errh = NULL;
                }
            }
        }
        return (rc);
}

/*#***********************************************************************\
 *
 *      wblobg2stream_getpageaddrfun
 *
 * Gets page address for a BLOB Write stream. This is a callback
 * for dbe level wblob.
 *
 * Parameters:
 *      getpageaddr_ctx - in out, use
 *          pointer to wblobg2stream
 *
 *      p_daddr - out
 *          pointer to output disk address
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code when failure
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t wblobg2stream_getpageaddrfun(
        void* getpageaddr_ctx,
        su_daddr_t* p_daddr,
        su_err_t** p_errh)
{
        tb_wblobg2stream_t*  wbs = getpageaddr_ctx;

        CHK_WBS(wbs);
        if (wbs->wbs_prealloc_array_size == 0) {
            wbs->wbs_prealloc_array_size = WBS_PREALLOC_SIZE;
            wbs->wbs_prealloc_array = SsMemAlloc(wbs->wbs_prealloc_array_size
                                                 * sizeof(su_daddr_t));
        }
        if (wbs->wbs_prealloc_array_pos >= wbs->wbs_prealloc_array_num_idx_in_use) {
            su_ret_t rc;
            wbs->wbs_prealloc_array_pos = wbs->wbs_prealloc_array_num_idx_in_use = 0;
            rc = dbe_db_alloc_n_pages(wbs->wbs_db,
                                      wbs->wbs_prealloc_array,
                                      wbs->wbs_prealloc_array_size,
                                      &wbs->wbs_prealloc_array_num_idx_in_use,
                                      SU_DADDR_NULL,
                                      FALSE);
            if (rc != SU_SUCCESS) {
                *p_daddr = SU_DADDR_NULL;
                su_err_init_noargs(p_errh, rc);
                return (rc);
            }
        }
        ss_dassert(wbs->wbs_prealloc_array_pos < wbs->wbs_prealloc_array_num_idx_in_use);
        ss_dprintf_1(("wblobg2stream_getpageaddrfun: daddr=%lu\n",
                      (ulong)wbs->wbs_prealloc_array[
                              wbs->wbs_prealloc_array_pos]));
        *p_daddr = wbs->wbs_prealloc_array[wbs->wbs_prealloc_array_pos];
        wbs->wbs_prealloc_array_pos++;
        return (SU_SUCCESS);
}

/*#***********************************************************************\
 *
 *      wblobg2stream_releasepageaddrfun
 *
 * releases page address so that the wblobg2stream can know dbe level
 * wblob has completed writing to it.
 *
 * Parameters:
 *      released_pageaddr_ctx - in out, use
 *          pointer to wblobg2stream
 *
 *      daddr - in
 *          disk address
 *
 *      bytes_in_use - in
 *          number of bytes taken into data use for that page
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t wblobg2stream_releasepageaddrfun(
        void* released_pageaddr_ctx,
        su_daddr_t daddr,
        size_t bytes_in_use,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        tb_wblobg2stream_t* wbs = released_pageaddr_ctx;
        bpagerec_t* pagerec;

        CHK_WBS(wbs);
        ss_rc_dassert(bytes_in_use > 0, (int)bytes_in_use);
        ss_dassert(wbs->wbs_num_pages_in_array < BS_PAGEARR_SIZE);

        pagerec = &wbs->wbs_pagearray[wbs->wbs_num_pages_in_array];
        pagerec->bp_size_so_far = wbs->wbs_loggedsize;
        DBE_BLOBG2SIZE_ADDASSIGN_SIZE(&wbs->wbs_loggedsize, (ss_uint4_t)bytes_in_use);
        pagerec->bp_addr = daddr;
        wbs->wbs_num_pages_in_array++;

        if (wbs->wbs_num_pages_in_array >= BS_PAGEARR_SIZE) {
            ss_dassert(wbs->wbs_num_pages_in_array == BS_PAGEARR_SIZE);
            rc = wblobg2stream_flush(NULL, wbs, FLUSHMODE_PAGEARRAY_FULL, p_errh);
        }
        return (rc);
}



/*##**********************************************************************\
 *
 *      tb_blobg2mgr_incrementpersistentrefcount
 *
 * Increments persistent ref count for a BLOB
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_incrementpersistentrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        inmemoryblobrefrec_t* imbr;
        su_rbt_node_t* rbtn;
        ss_int4_t persistent_count;
        ss_int4_t count;
        su_ret_t rc;

        CHK_BM(bm);
        ss_dprintf_1(("tb_blobg2mgr_incrementpersistentrefcount(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid)));
        if (DBE_BLOBG2ID_CMP(bid, blobg2id_null) == 0) {
            return (SU_SUCCESS);
        }
        /* the call below both sets Lock to imbr &  enters the BLOB manager mutex
         * and also increments the in-memory reference count to quarantee it will not
         * be released meanwhile
         */
        imbr = blobg2mgr_lockblobforaccess(bm, bid, &rbtn);
        SsSemExit(bm->bm_mutex);

        rc = blobg2mgr_incsysblobsrefcount(cd, bm, bid, &persistent_count, p_errh);
        SsSemEnter(bm->bm_mutex);
        /* undo increment done in blobg2mgr_lockblobforaccess */
        count = (ss_int4_t)inmemoryblobrefrec_dec(imbr);
        if (rc == SU_SUCCESS) {
            inmemoryblobrefrec_setpersistentrefcount(imbr, persistent_count);
            inmemoryblobrefrec_unlock(imbr);
            if (count == 0) {
                /* remove the in-memory record because there are no more
                 * in-memory references and it is uncertain when the next in-mem
                 * reference is taken, leaving this record for caching purposes might
                 * result in too big memory usage
                 */
                su_rbt_delete(bm->bm_inmemoryrefs, rbtn);
            }
        }
        SsSemExit(bm->bm_mutex);
        if (rc == SU_SUCCESS) {
            ss_rc_dassert(persistent_count > 0 || bm->bm_hsb, persistent_count);
        }
        return (rc);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_blobidbyva
 *
 * gets BLOB ID from v-attribute containig a BLOB reference
 *
 * Parameters:
 *      va - in, use
 *          v-attribute (low level of column value or expression)
 *
 * Return value:
 *      BLOB ID
 *
 * Limitations:
 *
 * Globals used:
 */
static dbe_blobg2id_t blobg2mgr_blobidbyva(va_t* va)
{
        dbe_vablobg2ref_t bref;
        dbe_brefg2_loadfromva(&bref, va);
        return (dbe_brefg2_getblobg2id(&bref));
}


/*#***********************************************************************\
 *
 *      blobg2mgr_incrementinmemoryrefcount_byva
 *
 * Increment in-memory ref count for a BLOB by its id
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bid - in
 *          Blob id
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t blobg2mgr_incrementinmemoryrefcount_byid(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        su_ret_t rc;
        tb_blobg2mgr_t* bm;

        bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
        rc = tb_blobg2mgr_incrementinmemoryrefcount(
                cd,
                bm,
                bid,
                p_errh);
        return (rc);
}

/*#***********************************************************************\
 *
 *      blobg2mgr_decrementinmemoryrefcount_byid
 *
 * decrements in-memory refcount of a BLOB by id
 *
 * Parameters:
 *      cd - in, use
 *          Client context
 *
 *      bid - in
 *          Blob id
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t blobg2mgr_decrementinmemoryrefcount_byid(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh)
{
        su_ret_t rc;
        tb_blobg2mgr_t* bm;

        bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
        rc = tb_blobg2mgr_decrementinmemoryrefcount(
                cd,
                bm,
                bid,
                p_errh);
        return (rc);
}


/*##**********************************************************************\
 *
 *      tb_blobg2mgr_incrementinmemoryrefcount_byva
 *
 * Increment in-memory ref count for a BLOB so that the referring
 * object is a v-attribute
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      p_va - in, use
 *          v-attribute pointing to a BLOB
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_incrementinmemoryrefcount_byva(
        rs_sysi_t* cd,
        va_t* p_va,
        su_err_t** p_errh)
{
        su_ret_t rc;
        tb_blobg2mgr_t* bm;

        if (!dbe_brefg2_isblobg2check_from_va(p_va)) {
            rc = SU_SUCCESS;
        } else {
            bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
            rc = tb_blobg2mgr_incrementinmemoryrefcount(
                    cd,
                    bm,
                    blobg2mgr_blobidbyva(p_va),
                    p_errh);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_decrementinmemoryrefcount_byva
 *
 * decrements in-memory refcount of a BLOB referred by a v-attribute
 *
 * Parameters:
 *      cd - in, use
 *          Client context
 *
 *      p_va - in, use
 *          va containing a BLOB reference
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_decrementinmemoryrefcount_byva(
        rs_sysi_t* cd,
        va_t* p_va,
        su_err_t** p_errh)
{
        su_ret_t rc;
        tb_blobg2mgr_t* bm;

        if (!dbe_brefg2_isblobg2check_from_va(p_va)) {
            rc = SU_SUCCESS;
        } else {
            bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
            rc = tb_blobg2mgr_decrementinmemoryrefcount(
                    cd,
                    bm,
                    blobg2mgr_blobidbyva(p_va),
                    p_errh);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_incrementpersistentrefcount_byva
 *
 * increments persistent ref. count
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      p_va - in, use
 *          va pointing to BLOB
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_incrementpersistentrefcount_byva(
        rs_sysi_t* cd,
        va_t* p_va,
        su_err_t** p_errh)
{
        su_ret_t rc;
        tb_blobg2mgr_t* bm;

        bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
        rc = tb_blobg2mgr_incrementpersistentrefcount(
                cd,
                bm,
                blobg2mgr_blobidbyva(p_va),
                p_errh);
        return (rc);
}


/*#***********************************************************************\
 *
 *      wblobg2stream_set_bm_listpos
 *
 * sets the list position backlink to write blob stream to enable
 * unlinking it from the list of open write blobs streams.
 *
 * Parameters:
 *      wbs - in out, use
 *          write blob
 *
 *      listpos - in, hold
 *          list position in the Blob Managers list of wbs's
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void wblobg2stream_set_bm_listpos(
        tb_wblobg2stream_t* wbs,
        su_list_node_t* listpos)
{
        ss_debug(CHK_WBS(wbs));
        wbs->wbs_bm_listpos = listpos;
}


/*##**********************************************************************\
 *
 *      tb_blobg2mgr_initwblobstream
 *
 * Creates a new write blob stream object
 *
 * Parameters:
 *      cd - in, use
 *          Client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      atype - in, hold
 *          Reference to parameter (or column) type
 *
 *      aval - in out, hold
 *          Reference to parameter (or column) value
 *
 * Return value - give:
 *      new wblob stream
 *
 * Limitations:
 *
 * Globals used:
 */
tb_wblobg2stream_t*  tb_blobg2mgr_initwblobstream(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        tb_wblobg2stream_t* wbs;
        su_list_node_t* listpos;

        CHK_BM(bm);

        blobg2mgr_enteraction(bm, cd);
        wbs = wblobg2stream_init(cd, bm, atype, aval);
        SsSemEnter(bm->bm_mutex);
        listpos = su_list_insertlast(bm->bm_wblobs, wbs);
        blobg2mgr_add_new_blob_ref(cd, bm, wbs->wbs_id, TRUE);
        SsSemExit(bm->bm_mutex);
        wblobg2stream_set_bm_listpos(wbs, listpos);
        ss_debug(CHK_WBS(wbs));
        blobg2mgr_exitaction(bm, cd);
        return (wbs);
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_initwblobstream_bycd
 *
 * Same as tb_blobg2mgr_initwblobstream but this digs the BLOB
 * Manager from cd
 *
 * Parameters:
 *      cd - in, use
 *
 *      atype - in, hold
 *
 *      aval - in out, hold
 *
 * Return value - give:
 *      new Write BLOB Stream
 *
 * Limitations:
 *
 * Globals used:
 */
tb_wblobg2stream_t* tb_blobg2mgr_initwblobstream_bycd(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        tb_blobg2mgr_t* bm;
        tb_wblobg2stream_t* wbs;

        bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
        wbs = tb_blobg2mgr_initwblobstream(cd, bm, atype, aval);
        return (wbs);
}

/*#***********************************************************************\
 *
 *      wblobg2stream_init_for_recovery
 *
 * Initializes a Blob write stream that possibly has been started before.
 * This is the case when running roll-forward recovery.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in out, use
 *          BLOB Manager
 *
 *      bid - in
 *          BLOB ID, from log
 *
 *      startoffset - in
 *          byte offset where we start the writing now
 *
 * Return value - give:
 *      new Write BLOB Stream
 *
 * Limitations:
 *
 * Globals used:
 */
static tb_wblobg2stream_t* wblobg2stream_init_for_recovery(
        void* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        dbe_blobg2size_t startoffset,
        bool* p_newblob)
{
        su_ret_t rc;
        tb_wblobg2stream_t* wbs = SSMEM_NEW(tb_wblobg2stream_t);

        ss_dprintf_1(("wblobg2stream_init_for_recovery(id=0x%08lX%08lX,offset=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid),
                      (ulong)DBE_BLOBG2SIZE_GETMOSTSIGNIFICANTUINT4(startoffset),
                      (ulong)DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(startoffset)));
        wbs->wbs_check = TBCHK_BLOBG2WRITESTREAM;
        wbs->wbs_db = rs_sysi_db(cd);
        wbs->wbs_bm = bm;
        wbs->wbs_cd = cd;
        wbs->wbs_bm_listpos = NULL;
        wbs->wbs_startcpnum = DBE_CPNUM_NULL;
        wbs->wbs_id = bid;
        wbs->wbs_size = wbs->wbs_loggedsize = startoffset;
        wbs->wbs_atype = NULL;
        wbs->wbs_aval = NULL;
        wbs->wbs_rdva = refdva_init();
        wbs->wbs_reachbuf = NULL;
        wbs->wbs_startpos_of_pagearray = wbs->wbs_loggedsize;
        wbs->wbs_num_pages_in_array = 0;
        wbs->wbs_num_pagearrays_saved = 0;
        wbs->wbs_sysconnect = NULL;
        wbs->wbs_sysblobscursor = NULL;
        wbs->wbs_prealloc_array_size = 0;
        wbs->wbs_prealloc_array_num_idx_in_use = 0;
        wbs->wbs_prealloc_array_pos = 0;
        wbs->wbs_prealloc_array = NULL;

        if (!DBE_BLOBG2SIZE_IS0(startoffset)) {
            /* The blob write has been started before the latest checkpoint!
             * Need to reopen the existing BLOB for append.
             */
            ss_int4_t refcount;
            dbe_blobg2size_t size;
            rc = blobg2mgr_fetchblobheaderinfo(cd, bm, bid,
                                               &refcount,
                                               &wbs->wbs_startcpnum,
                                               &size);
            ss_rc_assert(rc == SU_SUCCESS, rc);
            ss_rc_dassert(refcount == 0, refcount);
            ss_dassert(DBE_BLOBG2SIZE_CMP(size, startoffset) == 0);
            *p_newblob = FALSE;
        } else {
            *p_newblob = TRUE;
        }
        wbs->wbs_dbewblob =
            dbe_wblobg2_init_for_recovery(
                    wbs->wbs_db,
                    &wbs->wbs_startcpnum,
                    wbs->wbs_id,
                    wbs->wbs_loggedsize,
                    wblobg2stream_getpageaddrfun, wbs,
                    wblobg2stream_releasepageaddrfun, wbs);
        ss_dassert(wbs->wbs_dbewblob != NULL);
        wbs->wbs_status = BS_STAT_RELEASED;
        return (wbs);
}


/*##**********************************************************************\
 *
 *      tb_blobg2mgr_initwblobstream_for_recovery_bycd
 *
 * Creates a new Write BLOB Stream for recovery
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      id - in
 *          BLOB ID (from log)
 *
 *      startoffset - in
 *          byte offset where the recovery starts
 *
 * Return value - give:
 *      new Write BLOB Stream
 *
 * Limitations:
 *
 * Globals used:
 */
tb_wblobg2stream_t* tb_blobg2mgr_initwblobstream_for_recovery_bycd(
        rs_sysi_t* cd,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startoffset)
{
        tb_blobg2mgr_t* bm;
        tb_wblobg2stream_t* wbs;
        su_list_node_t* listpos;
        bool newblob;

        bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
        wbs = wblobg2stream_init_for_recovery(
                cd,
                bm,
                id,
                startoffset,
                &newblob);
        SsSemEnter(bm->bm_mutex);
        listpos = su_list_insertlast(bm->bm_wblobs, wbs);
        blobg2mgr_add_new_blob_ref(cd, bm, wbs->wbs_id, newblob);
        SsSemExit(bm->bm_mutex);
        wblobg2stream_set_bm_listpos(wbs, listpos);
        ss_debug(CHK_WBS(wbs));
        return (wbs);
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_delete_unreferenced_blobs_after_recovery
 *
 * deletes all blob whose reference count is 0. This can be done after
 * successful recovery.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      p_nblobs_deleted - out, use
 *          pointer to variable to tell how many BLOBs were actually deleted
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_delete_unreferenced_blobs_after_recovery(
        rs_sysi_t* cd,
        size_t* p_nblobs_deleted,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        TliRetT tlirc;
        TliConnectT* tcon;
        TliCursorT* tcur;
        tb_blobg2mgr_t* bm;
        dbe_blobg2id_t bid;
        size_t n;
        ss_debug(int dbg_refcount;)

        ss_dprintf_1(("tb_blobg2mgr_delete_unreferenced_blobs_after_recovery\n"));
        bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
        blobg2_initsysblobsconnectionandcursor(
                cd,
                &tcon,
                &tcur,
                TRUE);
        tlirc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_ID, &bid);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorConstrInt(tcur,
                                   RS_ANAME_BLOBS_REFCOUNT,
                                   TLI_RELOP_EQUAL,
                                   0);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        ss_debug(
                tlirc = TliCursorColInt(tcur,
                                        RS_ANAME_BLOBS_REFCOUNT,
                                        &dbg_refcount);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            );
        tlirc = TliCursorOpen(tcur);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        for (n = 0;;) {
            su_ret_t rc2;
            tlirc = TliCursorNext(tcur);
            if (tlirc != TLI_RC_SUCC) {
                break;
            }
            ss_rc_dassert(dbg_refcount == 0, dbg_refcount);
            rc2 = tb_blobg2mgr_blobdeletebyid(cd, bm, bid, p_errh);
            if (rc2 != SU_SUCCESS) {
                if (rc == SU_SUCCESS) {
                    /* report only first error */
                    rc = rc2;
                    p_errh = NULL;
                }
            } else {
                n++;
            }
        }
        ss_rc_dassert(tlirc == TLI_RC_END, tlirc);
        if (tlirc == TLI_RC_END) {
            tlirc = TliCommit(tcon);
        } else {
            tlirc = TliRollback(tcon);
        }
        blobg2mgr_donesysblobsconnectionandcursor(&tcon, &tcur);
        *p_nblobs_deleted = n;
        return (rc);
}


/*#***********************************************************************\
 *
 *      wblobg2stream_cancelprealloc
 *
 * Cancels all preallocated pages for a BLOB write stream.
 * This is done when flushing the BLOB to action consistent
 * state during checkpoint creation.
 *
 * Parameters:
 *      wbs - in out, use
 *          Write BLOB Stream
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void wblobg2stream_cancelprealloc(
        tb_wblobg2stream_t* wbs)
{
        ss_debug(CHK_WBS(wbs));

        if (wbs->wbs_prealloc_array_num_idx_in_use
            != wbs->wbs_prealloc_array_pos)
        {

            ss_dassert(wbs->wbs_prealloc_array_pos < wbs->wbs_prealloc_array_num_idx_in_use);
            dbe_db_free_n_pages(
                    wbs->wbs_db,
                    (wbs->wbs_prealloc_array_num_idx_in_use -
                     wbs->wbs_prealloc_array_pos),
                    &wbs->wbs_prealloc_array[wbs->wbs_prealloc_array_pos],
                    wbs->wbs_startcpnum,
                    FALSE);
        }
        wbs->wbs_prealloc_array_num_idx_in_use =
            wbs->wbs_prealloc_array_pos = 0;
}

/*#***********************************************************************\
 *
 *      wblobg2stream_flush
 *
 * flushes the state of a write BLOB stream to database.
 *
 * Parameters:
 *      cd_if_checkpoint - in, use
 *          if flushmode == FLUSHMODE_CHECKPOINT then the
 *          client (system) context. Otherwise this must be NULL
 *          and the context of the wbs is used
 *
 *      wbs - in out, use
 *          Write BLOB Stream
 *
 *      flushmode - in
 *          FLUSHMODE_COMPLETE - The BLOB Write stream is being closed,
 *              also preallocated pages should be cancelled and the last
 *              incomplete block must also be saved (= dbe-level stream
 *              closed)
 *          FLUSHMODE_CHECKPOINT - Checkpoint creation requests action-
 *              consistent state, also preallocated pages should
 *              be cancelled to prevent garbage after possible recovery.
 *          FLUSHMODE_PAGEARRAY_FULL - The array of pages that fits
 *              into one row of SYS_TABLES is full. (no durablility
 *              requirements in this mode)
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t wblobg2stream_flush(
        rs_sysi_t* cd_if_checkpoint,
        tb_wblobg2stream_t* wbs,
        flushmode_t flushmode,
        su_err_t** p_errh)
{
        uint rc = SU_SUCCESS;
        TliRetT tlirc;
        ss_int4_t refcount;
        ss_int4_t startcpnum;
        int complete = FALSE;
        uint i;
        char colname[16];
        rs_sysi_t* cd;

        CHK_WBS(wbs);

        ss_dprintf_1(("wblobg2stream_flush(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(wbs->wbs_id),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(wbs->wbs_id)));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        if (FLUSHMODE_CHECKPOINT == flushmode) {
            ss_dassert(NULL != cd_if_checkpoint);
            cd = cd_if_checkpoint;
            blobg2mgr_donesysblobsconnectionandcursor(&wbs->wbs_sysconnect,
                                                      &wbs->wbs_sysblobscursor);
        } else {
            ss_dassert(NULL == cd_if_checkpoint);
            cd = wbs->wbs_cd;
        }
        if (wbs->wbs_sysblobscursor == NULL) {
            ss_dassert(wbs->wbs_sysconnect == NULL);
            blobg2_initsysblobsconnectionandcursor(
                    cd,
                    &wbs->wbs_sysconnect,
                    &wbs->wbs_sysblobscursor,
                    FALSE);
        } else {
            blobg2_resetsysblobscursor(
                    cd,
                    wbs->wbs_sysconnect,
                    &wbs->wbs_sysblobscursor);
        }
        ss_dassert(wbs->wbs_sysconnect != NULL);
        switch (flushmode) {
            case FLUSHMODE_COMPLETE:
                /* when the blob is complete
                 * the dbe level blob write stream must also be flushed
                 */
                rc = dbe_wblobg2_flush(wbs->wbs_dbewblob, wbs->wbs_cd, p_errh);
                if (rc != SU_SUCCESS) {
                    goto cleanup;
                }
                break;
            case FLUSHMODE_CHECKPOINT:
            case FLUSHMODE_PAGEARRAY_FULL:
                break;
            default:
                ss_rc_error(flushmode);
                break;
        }
        if (wbs->wbs_num_pages_in_array != 0) {
            tlirc = TliCursorColInt8t(wbs->wbs_sysblobscursor,
                                      RS_ANAME_BLOBS_ID,
                                      &wbs->wbs_id);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorColInt8t(wbs->wbs_sysblobscursor,
                                      RS_ANAME_BLOBS_STARTPOS,
                                      &wbs->wbs_startpos_of_pagearray);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorColInt8t(wbs->wbs_sysblobscursor,
                                      RS_ANAME_BLOBS_ENDSIZE,
                                      &wbs->wbs_loggedsize);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            ss_dassert(DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(bs_0) == 0 &&
                       DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(bs_0) == 0);
            if (0 == DBE_BLOBG2SIZE_CMP(wbs->wbs_startpos_of_pagearray, bs_0)) {
                /* first row, AKA BLOB header record */
                if (flushmode == FLUSHMODE_COMPLETE) {
                    complete = TRUE;
                }
                tlirc = TliCursorColInt8t(wbs->wbs_sysblobscursor,
                                          RS_ANAME_BLOBS_TOTALSIZE,
                                          &wbs->wbs_loggedsize);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
                tlirc = TliCursorColInt(wbs->wbs_sysblobscursor,
                                        RS_ANAME_BLOBS_COMPLETE,
                                        &complete);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
                startcpnum = (ss_int4_t)wbs->wbs_startcpnum;
                tlirc = TliCursorColInt4t(wbs->wbs_sysblobscursor,
                                         RS_ANAME_BLOBS_STARTCPNUM,
                                         &startcpnum);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
                refcount = 0;
                tlirc = TliCursorColInt4t(wbs->wbs_sysblobscursor,
                                         RS_ANAME_BLOBS_REFCOUNT,
                                         &refcount);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);

            } else {
                /* not first row for this blob, the below columns are NULL,
                 * because they make sense only in the "header record"
                 */
                tlirc = TliCursorColSetNULL(wbs->wbs_sysblobscursor,
                                            RS_ANAME_BLOBS_TOTALSIZE);
                ss_rc_dassert(tlirc == TLI_RC_SUCC || TLI_RC_COLNOTBOUND,
                              tlirc);
                tlirc = TliCursorColSetNULL(wbs->wbs_sysblobscursor,
                                            RS_ANAME_BLOBS_COMPLETE);
                ss_rc_dassert(tlirc == TLI_RC_SUCC || TLI_RC_COLNOTBOUND,
                              tlirc);
                tlirc = TliCursorColSetNULL(wbs->wbs_sysblobscursor,
                                            RS_ANAME_BLOBS_STARTCPNUM);
                ss_rc_dassert(tlirc == TLI_RC_SUCC || TLI_RC_COLNOTBOUND,
                              tlirc);
                tlirc = TliCursorColSetNULL(wbs->wbs_sysblobscursor,
                                            RS_ANAME_BLOBS_REFCOUNT);
                ss_rc_dassert(tlirc == TLI_RC_SUCC || TLI_RC_COLNOTBOUND,
                              tlirc);
            }
            tlirc = TliCursorColSizet(wbs->wbs_sysblobscursor,
                                      RS_ANAME_BLOBS_NUMPAGES,
                                      &wbs->wbs_num_pages_in_array);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            for (i = 0; i < wbs->wbs_num_pages_in_array; i++) {
                ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ADDR_TEMPL));
                SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ADDR_TEMPL, i + 1);
                ss_dassert(sizeof(colname) > strlen(colname));
                tlirc = TliCursorColInt4t(wbs->wbs_sysblobscursor,
                                         colname,
                                         (ss_int4_t*)&wbs->wbs_pagearray[i].bp_addr);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
                ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL));
                SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL, i + 1);
                ss_dassert(sizeof(colname) > strlen(colname));
                tlirc = TliCursorColInt8t(wbs->wbs_sysblobscursor,
                                          colname,
                                          &wbs->wbs_pagearray[i].bp_size_so_far);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            }
            for (; i < BS_PAGEARR_SIZE; i++) {
                ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ADDR_TEMPL));
                SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ADDR_TEMPL, i + 1);
                ss_dassert(sizeof(colname) > strlen(colname));
                tlirc = TliCursorColSetNULL(wbs->wbs_sysblobscursor,
                                            colname);
                ss_rc_dassert(tlirc == TLI_RC_SUCC || TLI_RC_COLNOTBOUND,
                              tlirc);
                ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL));
                SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL, i + 1);
                ss_dassert(sizeof(colname) > strlen(colname));
                tlirc = TliCursorColSetNULL(wbs->wbs_sysblobscursor,
                                            colname);
                ss_rc_dassert(tlirc == TLI_RC_SUCC || TLI_RC_COLNOTBOUND,
                              tlirc);
            }
            tlirc = TliCursorInsert(wbs->wbs_sysblobscursor);
            if (tlirc != TLI_RC_SUCC) {
                goto cursorop_failed;
            }
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            wbs->wbs_num_pagearrays_saved++;
            wbs->wbs_num_pages_in_array = 0;
            wbs->wbs_startpos_of_pagearray = wbs->wbs_loggedsize;
        }
        if ( DBE_BLOBG2SIZE_CMP(wbs->wbs_startpos_of_pagearray, bs_0) > 0
        &&  (flushmode == FLUSHMODE_COMPLETE
          || flushmode == FLUSHMODE_CHECKPOINT))
        {
            ss_int8_t i8;
            /* we must find the first row and update its contents because the
             * BLOB header record must be in sync in these flush modes
             */
            blobg2_resetsysblobscursor(
                    cd,
                    wbs->wbs_sysconnect,
                    &wbs->wbs_sysblobscursor);
            ss_dassert(wbs->wbs_sysblobscursor != NULL);

            tlirc = TliCursorColInt8t(wbs->wbs_sysblobscursor,
                                      RS_ANAME_BLOBS_TOTALSIZE,
                                      &i8);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorColInt(wbs->wbs_sysblobscursor,
                                    RS_ANAME_BLOBS_COMPLETE,
                                    &complete);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorConstrInt8t(wbs->wbs_sysblobscursor,
                                        RS_ANAME_BLOBS_ID,
                                        TLI_RELOP_EQUAL,
                                        wbs->wbs_id);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorConstrInt8t(wbs->wbs_sysblobscursor,
                                        RS_ANAME_BLOBS_STARTPOS,
                                        TLI_RELOP_EQUAL,
                                        SsInt8InitFrom2Uint4s(0,0));
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorOpen(wbs->wbs_sysblobscursor);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorNext(wbs->wbs_sysblobscursor);
            if (tlirc != TLI_RC_SUCC) {
                goto cursorop_failed;
            }
            i8 = wbs->wbs_loggedsize;
            if (flushmode == FLUSHMODE_COMPLETE) {
                complete = TRUE;
            } else {
                ss_dassert(!complete);
            }
            tlirc = TliCursorUpdate(wbs->wbs_sysblobscursor);
            if (tlirc != TLI_RC_SUCC) {
                goto cursorop_failed;
            }
        }
        tlirc = TliCommit(wbs->wbs_sysconnect);
        if (tlirc != TLI_RC_SUCC) {
            goto connectop_failed;
        }
 cleanup:;
        switch (flushmode) {
            case FLUSHMODE_CHECKPOINT:
                blobg2mgr_donesysblobsconnectionandcursor(
                        &wbs->wbs_sysconnect,
                        &wbs->wbs_sysblobscursor);
                /* FALLTHROUGH */
            case FLUSHMODE_COMPLETE:
                /* if there are surplus preallocated pages, free them */
                wblobg2stream_cancelprealloc(wbs);
                break;
            default:
                ss_rc_derror(flushmode);
            case FLUSHMODE_PAGEARRAY_FULL:
                break;
        }
        return (rc);
 cursorop_failed:;
        {
            ss_debug(bool succp =) TliCursorCopySuErr(wbs->wbs_sysblobscursor, p_errh);
            ss_debug(bool succp2 =) TliCursorErrorInfo(wbs->wbs_sysblobscursor, NULL, &rc);
            ss_dassert(succp);
            ss_dassert(succp2);
            tlirc = TliRollback(wbs->wbs_sysconnect);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            goto cleanup;
        }
 connectop_failed:;
        {
            ss_debug(bool succp =) TliConnectCopySuErr(wbs->wbs_sysconnect, p_errh);
            ss_dassert(succp);
            rc = TliErrorCode(wbs->wbs_sysconnect);
            goto cleanup;
        }
}

/*#***********************************************************************\
 *
 *      wblobg2stream_init
 *
 * local function that the BLOB Manager uses for creating a new Write
 * BLOB Stream.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      bm - in, use
 *          BLOB Manager
 *
 *      atype - in, hold
 *          type of the expression/column/parameter
 *
 *      aval - out, hold
 *          reference to expression/column/parameter value.
 *          This will contain the reference to BLOB after closing
 *          the Write BLOB Stream.
 *
 * Return value - give:
 *      the new Write BLOB Stream
 *
 * Limitations:
 *
 * Globals used:
 */
static tb_wblobg2stream_t* wblobg2stream_init(
        void* cd,
        tb_blobg2mgr_t* bm,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        tb_wblobg2stream_t* wbs = SSMEM_NEW(tb_wblobg2stream_t);

        wbs->wbs_check = TBCHK_BLOBG2WRITESTREAM;
        wbs->wbs_db = rs_sysi_db(cd);
        wbs->wbs_bm = bm;
        wbs->wbs_cd = cd;
        wbs->wbs_bm_listpos = NULL;
        wbs->wbs_startcpnum = DBE_CPNUM_NULL;
        wbs->wbs_id = blobg2id_null;
        SsInt8Set0(&wbs->wbs_loggedsize);
        wbs->wbs_size = wbs->wbs_loggedsize;
        wbs->wbs_atype = atype;
        wbs->wbs_aval = aval;
        ss_dassert(atype != NULL);
        ss_dassert(aval != NULL);
        wbs->wbs_rdva = refdva_init();
        wbs->wbs_reachbuf = NULL;
        wbs->wbs_startpos_of_pagearray = wbs->wbs_loggedsize;
        wbs->wbs_num_pages_in_array = 0;
        wbs->wbs_num_pagearrays_saved = 0;
        wbs->wbs_sysconnect = NULL;
        wbs->wbs_sysblobscursor = NULL;
        wbs->wbs_prealloc_array_size = 0;
        wbs->wbs_prealloc_array_num_idx_in_use = 0;
        wbs->wbs_prealloc_array_pos = 0;
        wbs->wbs_prealloc_array = NULL;

        wbs->wbs_dbewblob =
            dbe_wblobg2_init(
                    wbs->wbs_db,
                    &wbs->wbs_id,
                    &wbs->wbs_startcpnum,
                    wblobg2stream_getpageaddrfun, wbs,
                    wblobg2stream_releasepageaddrfun, wbs);
        ss_dassert(wbs->wbs_dbewblob != NULL);
        ss_dprintf_1(("wblobg2stream_init(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(wbs->wbs_id),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(wbs->wbs_id)));

        wbs->wbs_status = BS_STAT_RELEASED;
        return (wbs);
}

/*#***********************************************************************\
 *
 *      wblobg2stream_physdone
 *
 * Frees the storage of Write BLOB Stream and unlinks it from other
 * data structures.
 *
 * Parameters:
 *      wbs - in, take
 *          the Write BLOB Stream
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void wblobg2stream_physdone(tb_wblobg2stream_t* wbs)
{
        ss_debug(CHK_WBS(wbs));

        ss_dassert(wbs->wbs_bm_listpos != NULL);
        SsSemEnter(wbs->wbs_bm->bm_mutex);
        if (wbs->wbs_bm_listpos != NULL) {
            /* remove wblob from list of open BLOB streams (kept in
             *  BLOB manager).
             */
            blobg2mgr_decrementinmemoryrefcount_nodelete_nomutex(
                    wbs->wbs_cd, wbs->wbs_bm, wbs->wbs_id);
            su_list_remove(wbs->wbs_bm->bm_wblobs, wbs->wbs_bm_listpos);
        }
        SsSemExit(wbs->wbs_bm->bm_mutex);
        refdva_done(&wbs->wbs_rdva);
        blobg2mgr_donesysblobsconnectionandcursor(
                &wbs->wbs_sysconnect,
                &wbs->wbs_sysblobscursor);
        ss_dassert(wbs->wbs_prealloc_array_num_idx_in_use == wbs->wbs_prealloc_array_pos);
        SsMemFreeIfNotNULL(wbs->wbs_prealloc_array);
        SsMemFree(wbs);
}

/*##**********************************************************************\
 *
 *      tb_wblobg2stream_done
 *
 * Closes and frees a Write BLOB Stream
 *
 * Parameters:
 *      wbs - in, take
 *          stream object to close
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or failure
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_wblobg2stream_done(
        tb_wblobg2stream_t* wbs,
        su_err_t** p_errh)
{
        tb_blobg2mgr_t* bm;
        rs_sysi_t* cd;
        su_ret_t rc = SU_SUCCESS;
        su_ret_t rc2;
        CHK_WBS(wbs);

        ss_dprintf_1(("tb_wblobg2stream_done(id=0x%08lX%08lX)\n",
                      (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(wbs->wbs_id),
                      (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(wbs->wbs_id)));
        if (wbs->wbs_status != BS_STAT_RELEASED) {
            ss_derror; /* not a normal situation! */
            tb_wblobg2stream_release(wbs, 0, NULL);
        }
        blobg2mgr_enteraction(wbs->wbs_bm, wbs->wbs_cd);
        rc2 = wblobg2stream_flush(NULL, wbs, FLUSHMODE_COMPLETE, p_errh);
        ss_dassert(rc2 != SU_SUCCESS || 0 == DBE_BLOBG2SIZE_CMP(wbs->wbs_size, wbs->wbs_loggedsize))
        FAKE_CODE_RESET(FAKE_TAB_YIELDINBLOBG2DONE,
                        SsPrintf("Sleeping 3 seconds\n"); SsThrSleep(3000L););
        if (rc2 != SU_SUCCESS) {
            rc = rc2;
            p_errh = NULL;
        }
        if (wbs->wbs_rdva != NULL) {
            /* we do have at least some data!
             * patch the BLOB reference to the referencing va
             */
            ss_byte_t* data;
            va_index_t ds;
            dbe_vablobg2ref_t bref;

            dbe_brefg2_initbuf(&bref, wbs->wbs_id, wbs->wbs_size);
            data = va_getdata(wbs->wbs_rdva, &ds);
            ss_dassert(ds >= RS_VABLOBREF_SIZE);
            dbe_brefg2_storetodiskbuf(&bref, data, ds);
        }
        if (wbs->wbs_aval != NULL) {
            ss_dassert(wbs->wbs_rdva != NULL);
            ss_dassert(wbs->wbs_atype != NULL);
            /* note: the rs_aval_insertrefdva automatically increments the in-memory
             * blob reference counter for this BLOB!
             */
            rs_aval_insertrefdva(wbs->wbs_cd, wbs->wbs_atype, wbs->wbs_aval, wbs->wbs_rdva);
            wbs->wbs_rdva = refdva_init();
        }
        rc2 = dbe_wblobg2_close(wbs->wbs_dbewblob, wbs->wbs_cd, p_errh);
        if (rc2 != SU_SUCCESS) {
            tb_blobg2mgr_blobdeletebyid_noenteraction(
                    wbs->wbs_cd,
                    wbs->wbs_bm,
                    wbs->wbs_id,
                    NULL);
            if (rc == SU_SUCCESS) {
                rc = rc2;
            }
        }
        bm = wbs->wbs_bm;
        cd = wbs->wbs_cd;
        wblobg2stream_physdone(wbs);
        blobg2mgr_exitaction(bm, cd);
        return (rc);
}

/*##**********************************************************************\
 *
 *      tb_wblobg2stream_abort
 *
 * Aborts and frees a Write BLOB Stream. Note this also frees the database
 * storage already taken into use.
 *
 * Parameters:
 *      wbs - in, take
 *          Write BLOB Stream to be aborted
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void tb_wblobg2stream_abort(tb_wblobg2stream_t* wbs)
{
        tb_blobg2mgr_t* bm;
        rs_sysi_t* cd;

        CHK_WBS(wbs);

        if (wbs->wbs_status != BS_STAT_RELEASED) {
            tb_wblobg2stream_release(wbs, 0, NULL);
        }
        blobg2mgr_enteraction(wbs->wbs_bm, wbs->wbs_cd);
        dbe_wblobg2_cancel(wbs->wbs_dbewblob);
        wblobg2stream_cancelprealloc(wbs);
        tb_blobg2mgr_blobdeletebyid_noenteraction(
                wbs->wbs_cd,
                wbs->wbs_bm,
                wbs->wbs_id,
                NULL);
        bm = wbs->wbs_bm;
        cd = wbs->wbs_cd;
        wblobg2stream_physdone(wbs);
        blobg2mgr_exitaction(bm, cd);
}

/*##**********************************************************************\
 *
 *      tb_wblobg2stream_reach
 *
 * Reaches a Write BLOB Stream for writing
 *
 * Parameters:
 *      wbs - in out, use
 *          Write BLOB Stream
 *
 *      pp_buf - out, ref
 *          pointer to pointer to write buffer
 *
 *      p_avail - out, use
 *          pointer to variable that is set to contain the
 *          number of bytes available for writing
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_wblobg2stream_reach(
        tb_wblobg2stream_t* wbs,
        ss_byte_t** pp_buf,
        size_t* p_avail,
        su_err_t** p_errh)
{
        su_ret_t rc;

        CHK_WBS(wbs);
        ss_rc_dassert(wbs->wbs_status == BS_STAT_RELEASED, wbs->wbs_status);

        rc = dbe_wblobg2_reach(wbs->wbs_dbewblob, pp_buf, p_avail, p_errh);
        if (rc == SU_SUCCESS) {
            wbs->wbs_reachbuf = *pp_buf;
            wbs->wbs_status = BS_STAT_REACHED;
        } else {
            wbs->wbs_reachbuf = NULL;
        }
        ss_dprintf_1(("tb_wblobg2stream_reach: got %lu bytes\n",
                      (ulong)*p_avail));
        return (rc);
}

/*##**********************************************************************\
 *
 *      tb_wblobg2stream_release
 *
 * releases a reached write buffer
 *
 * Parameters:
 *      wbs - in out, use
 *          Write BLOB Stream
 *
 *      nbytes - in
 *          number of bytes written to the write buffer
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_wblobg2stream_release(
        tb_wblobg2stream_t* wbs,
        size_t nbytes,
        su_err_t** p_errh)
{
        su_ret_t rc;

        CHK_WBS(wbs);
        ss_rc_dassert(wbs->wbs_status == BS_STAT_REACHED, wbs->wbs_status);
        ss_dprintf_1(("tb_wblobg2stream_release(nbytes=%lu), size=%lu\n",
                      (ulong)nbytes,
                      DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(wbs->wbs_size)));
        if (nbytes > 0) {
            ss_int4_t s;
            size_t keycmplen = blobg2mgr_getkeycmplen(wbs->wbs_bm);
            bool succp = SsInt8ConvertToInt4(&s, wbs->wbs_size);
            if (succp) {
                if (s == 0) {
                    /* this branch has to run even if keycmplen == 0 */
                    refdva_allocblobdata(&wbs->wbs_rdva, (va_index_t)(keycmplen + RS_VABLOBG2REF_SIZE));
                }
                if (s < (ss_int4_t)keycmplen) {
                    /* less than keycmplen bytes have been received; we need to collect
                     * data for the va blob reference
                     */
                    size_t nbytestocopy;
                    ss_byte_t* data;
                    va_index_t ds;

                    data = va_getdata(wbs->wbs_rdva, &ds);
                    ss_dassert(ds == keycmplen + RS_VABLOBG2REF_SIZE);
                    if (s + nbytes < keycmplen) {
                        nbytestocopy = nbytes;
                    } else {
                        nbytestocopy = (size_t)keycmplen - (size_t)s;
                    }
                    memcpy(data + s, wbs->wbs_reachbuf, nbytestocopy);
                }
            }
            /* else the BLOB is already bigger than what fits into
             * a positive 32-bit integer;
             * definitely the keycmplen has been bypassed a long time ago!
             */
            DBE_BLOBG2SIZE_ADDASSIGN_SIZE(&wbs->wbs_size, (ss_uint4_t)nbytes);

        }
        blobg2mgr_enteraction(wbs->wbs_bm, wbs->wbs_cd);
        rc = dbe_wblobg2_release(wbs->wbs_dbewblob, wbs->wbs_cd, nbytes, p_errh);
        blobg2mgr_exitaction(wbs->wbs_bm, wbs->wbs_cd);
        wbs->wbs_status = BS_STAT_RELEASED;
        ss_dprintf_1(("tb_wblobg2stream_release(nbytes=%lu), done (rc = %d, new size=%lu\n",
                      (ulong)nbytes,
                      (int)rc,
                      (ulong)DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(wbs->wbs_size)));
        return (rc);
}

/* structure for representing a read-in row of SYS_BLOBS
 * for use of read blobs
 */
typedef struct {
        size_t rbr_num_pages_in_array;
        dbe_blobg2size_t rbr_endsize;
        bpagerec_t rbr_pagearray[BS_PAGEARR_SIZE];
} rbg2row_t;

typedef struct tb_rbg2s_st tb_rbg2s_t;

/* Read BLOB Stream object */
struct tb_rbg2s_st {
        int              rbs_check;
        dbe_db_t*        rbs_db;
        rs_sysi_t*       rbs_cd;
        dbe_blobg2id_t   rbs_id;
        dbe_blobg2size_t rbs_size;
        dbe_blobg2size_t rbs_readpos;
        dbe_rblobg2_t*   rbs_dberblob;
        TliConnectT*     rbs_sysconnect;
        TliCursorT*      rbs_sysblobscursor;
        bool             rbs_cursor_at_end;
        rbg2row_t        rbs_current_row;
        rbg2row_t        rbs_prefetch_row;
        size_t           rbs_prefetch_numpages_max;
        size_t           rbs_prefetch_numpages_now;
        size_t           rbs_pos_in_array;
        bs_status_t      rbs_status;
};

#define CHK_RBS(rbs) \
        ss_assert((rbs) != NULL);\
        ss_rc_assert((rbs)->rbs_check == TBCHK_BLOBG2READSTREAM, (rbs)->rbs_check);

/*#***********************************************************************\
 *
 *      rbg2s_loadrow
 *
 * loads row from SYS_BLOBS.
 *
 * Parameters:
 *      rbs - in out, use
 *          Read BLOB Stream
 *
 *      destrow - out, use
 *          pointer to structure for storage of essential
 *          SYS_BLOBS row data, the rbs->rbs_prefetch_row is
 *          always used for binding the column values.
 *          if destrow != &rbs->rbs_prefetch_row the result
 *          is then copied to *destrow and the rbs->rbs_prefetch_row
 *          is reset.
 *
 *      offset - in
 *          BLOB byte position where we want to read
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t rbg2s_loadrow(
        tb_rbg2s_t* rbs,
        rbg2row_t* destrow,
        dbe_blobg2size_t offset)
{
        su_ret_t rc;
        TliRetT tlirc;
        uint i;
        char colname[16];

        ss_dprintf_1(("rbg2s_loadrow offset =0x%08lX%08lX\n",
                      (ulong)DBE_BLOBG2SIZE_GETMOSTSIGNIFICANTUINT4(offset),
                      (ulong)DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(offset)));
        CHK_RBS(rbs);

#ifndef MAXREADLEVELBUG_FIXED
        if (rbs->rbs_sysblobscursor == NULL) {
            ss_dassert(rbs->rbs_sysconnect == NULL);
            blobg2_initsysblobsconnectionandcursor(
                    rbs->rbs_cd,
                    &rbs->rbs_sysconnect,
                    &rbs->rbs_sysblobscursor,
                    TRUE);
        } else {
            blobg2_resetsysblobscursor(
                    rbs->rbs_cd,
                    rbs->rbs_sysconnect,
                    &rbs->rbs_sysblobscursor);
        }
        tlirc = TliCursorConstrInt8t(rbs->rbs_sysblobscursor,
                                     RS_ANAME_BLOBS_ID,
                                     TLI_RELOP_EQUAL,
                                     rbs->rbs_id);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorConstrInt8t(rbs->rbs_sysblobscursor,
                                     RS_ANAME_BLOBS_ENDSIZE,
                                     TLI_RELOP_GT,
                                     offset);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColInt8t(rbs->rbs_sysblobscursor,
                                  RS_ANAME_BLOBS_ENDSIZE,
                                  &rbs->rbs_prefetch_row.rbr_endsize);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColSizet(
                rbs->rbs_sysblobscursor,
                RS_ANAME_BLOBS_NUMPAGES,
                &rbs->rbs_prefetch_row.rbr_num_pages_in_array);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        for (i = 0; i < BS_PAGEARR_SIZE; i++) {
            ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ADDR_TEMPL));
            SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ADDR_TEMPL, i + 1);
            ss_dassert(sizeof(colname) > strlen(colname));
            tlirc = TliCursorColInt4t(rbs->rbs_sysblobscursor,
                                      colname,
                                      (ss_int4_t*)&rbs->rbs_prefetch_row.rbr_pagearray[i].bp_addr);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL));
            SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL, i + 1);
            ss_dassert(sizeof(colname) > strlen(colname));
            tlirc = TliCursorColInt8t(rbs->rbs_sysblobscursor,
                                      colname,
                                      &rbs->rbs_prefetch_row.rbr_pagearray[i].bp_size_so_far);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        }
        tlirc = TliCursorOpen(rbs->rbs_sysblobscursor);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
#else /* !MAXREADLEVELBUG_FIXED */
        if (rbs->rbs_sysblobscursor == NULL) {
            ss_dassert(rbs->rbs_sysconnect == NULL);
            blobg2_initsysblobsconnectionandcursor(
                    rbs->rbs_cd,
                    &rbs->rbs_sysconnect,
                    &rbs->rbs_sysblobscursor,
                    TRUE);
            tlirc = TliCursorConstrInt8t(rbs->rbs_sysblobscursor,
                                         RS_ANAME_BLOBS_ID,
                                         TLI_RELOP_EQUAL,
                                         rbs->rbs_id);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorConstrInt8t(rbs->rbs_sysblobscursor,
                                         RS_ANAME_BLOBS_ENDSIZE,
                                         TLI_RELOP_GE,
                                         offset);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            tlirc = TliCursorColSizet(
                    rbs->rbs_sysblobscursor,
                    RS_ANAME_BLOBS_NUMPAGES,
                    &rbs->rbs_prefetch_row.rbr_num_pages_in_array);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            for (i = 0; i < BS_PAGEARR_SIZE; i++) {
                ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ADDR_TEMPL));
                SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ADDR_TEMPL, i + 1);
                ss_dassert(sizeof(colname) > strlen(colname));
                tlirc = TliCursorColInt4t(rbs->rbs_sysblobscursor,
                                          colname,
                                          &rbs->rbs_prefetch_row.rbr_pagearray[i].bp_addr);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
                ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL));
                SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL, i + 1);
                ss_dassert(sizeof(colname) > strlen(colname));
                tlirc = TliCursorColInt8t(rbs->rbs_sysblobscursor,
                                          colname,
                                          &rbs->rbs_prefetch_row.rbr_pagearray[i].bp_size_so_far);
                ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
            }
            tlirc = TliCursorOpen(rbs->rbs_sysblobscursor);
            ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        }
#endif /* !MAXREADLEVELBUG_FIXED */
        tlirc = TliCursorNext(rbs->rbs_sysblobscursor);

        if (tlirc == TLI_RC_END) {
            ss_dprintf_3(("rbg2s_loadrow:TLI_RC_END\n"));
            rc = DBE_RC_END;
            rbs->rbs_prefetch_row.rbr_num_pages_in_array = 0;
            rbs->rbs_cursor_at_end = TRUE;
        } else if (tlirc == TLI_RC_SUCC) {
            ss_dprintf_3(("rbg2s_loadrow:rbs->rbs_prefetch_row.rbr_num_pages_in_array=%ld\n", (long)rbs->rbs_prefetch_row.rbr_num_pages_in_array));
            rc = SU_SUCCESS;
        } else {
            char* errstr;
            uint errcode;
            bool err_found;
            err_found = TliCursorErrorInfo(rbs->rbs_sysblobscursor,
                                           &errstr,
                                           &errcode);
            ss_assert(err_found);
            rc = (su_ret_t)errcode;
            rbs->rbs_prefetch_row.rbr_num_pages_in_array = 0;
            rbs->rbs_cursor_at_end = TRUE;
            ss_info_assert(FALSE,
            ("rbg2s_loadrow: TliCursorNext returned %d (err=%u,str=%s)\n",
                           (int)tlirc,
                           errcode,
                           errstr));
        }
        tlirc = TliCommit(rbs->rbs_sysconnect);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        if (destrow != &rbs->rbs_prefetch_row) {
            /* dest row is not same as prefetch row, copy
             * result and reset the prefetch row
             */
            ss_dprintf_3(("rbg2s_loadrow:destrow != &rbs->rbs_prefetch_row\n"));
            *destrow = rbs->rbs_prefetch_row;
            rbs->rbs_prefetch_row.rbr_num_pages_in_array = 0;
        }
        return (rc);
}

/*#***********************************************************************\
 *
 *      rbg2s_resetpos
 *
 * resets the current fetch position
 *
 * Parameters:
 *      rbs - in out, use
 *          Read BLOB Stream
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#if 0 /* pete removed, not in use */
static void rbg2s_resetpos(tb_rbg2s_t* rbs)
{
        ss_debug(CHK_RBS(rbs));

        blobg2mgr_donesysblobsconnectionandcursor(
                &rbs->rbs_sysconnect,
                &rbs->rbs_sysblobscursor);
        rbs->rbs_current_row.rbr_num_pages_in_array =
            rbs->rbs_prefetch_row.rbr_num_pages_in_array = 0;
        rbs->rbs_cursor_at_end = FALSE;
}
#endif /* 0, by pete */


/*#***********************************************************************\
 *
 *      rbg2s_prefetch
 *
 * sends new prefetch request to I/O manager if necessary
 *
 * Parameters:
 *      rbs - in out, use
 *          Read BLOB Stream
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void rbg2s_prefetch(tb_rbg2s_t* rbs)
{
        size_t idx;
        size_t n_to_prefetch;
        su_daddr_t prefetch_array[BS_PAGEARR_SIZE];

        ss_debug(CHK_RBS(rbs));

        if (rbs->rbs_prefetch_numpages_now < rbs->rbs_prefetch_numpages_max) {
            /* prefetch is less ahead than rquired, proceed */
            if (rbs->rbs_pos_in_array + rbs->rbs_prefetch_numpages_max >
                rbs->rbs_current_row.rbr_num_pages_in_array)
            {
                /* prefetch position is beyond the row where is the
                 * current fetch position, proceed
                 */
                if (rbs->rbs_prefetch_row.rbr_num_pages_in_array == 0
                &&  !rbs->rbs_cursor_at_end)
                {
                    /* prefetch row is empty and the SYS_BLOBS
                     * cursor has not reached the end, load new row
                     * from SYS_BLOBS
                     */
                    rbg2s_loadrow(
                            rbs,
                            &rbs->rbs_prefetch_row,
                            rbs->rbs_current_row.rbr_endsize);
                }
            }
        }
        n_to_prefetch = 0;
        for (idx = rbs->rbs_pos_in_array + rbs->rbs_prefetch_numpages_now;
             rbs->rbs_prefetch_numpages_now < rbs->rbs_prefetch_numpages_max;
             rbs->rbs_prefetch_numpages_now++, idx++, n_to_prefetch++)
        {
            if (idx < rbs->rbs_current_row.rbr_num_pages_in_array) {
                prefetch_array[n_to_prefetch] =
                    rbs->rbs_current_row.rbr_pagearray[idx].bp_addr;
            } else {
                /* get the prefetch address from prefetch row */
                size_t idx_tmp = idx - rbs->rbs_current_row.rbr_num_pages_in_array;
                if (idx_tmp < rbs->rbs_prefetch_row.rbr_num_pages_in_array) {
                    prefetch_array[n_to_prefetch] =
                        rbs->rbs_prefetch_row.rbr_pagearray[idx_tmp].bp_addr;
                } else {
                    break;
                }
            }
        }
        if (n_to_prefetch > 0) {
            dbe_db_prefetch_n_pages(rbs->rbs_db, n_to_prefetch, prefetch_array);
        }
}


/*#***********************************************************************\
 *
 *      rbg2s_getpageaddrfun
 *
 * callback function for dbe level rblob. This  gives the next
 * page address to reach for reading
 *
 * Parameters:
 *      ctx - in out, use
 *          the Read BLOB Stream.
 *          due to visibility reasons this is defined as void*,
 *
 *      offset - in
 *          Byte offset where to read
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *      We do not support seeking to random pos, _yet_
 *
 * Globals used:
 */
static su_daddr_t rbg2s_getpageaddrfun(void* ctx, dbe_blobg2size_t offset)
{
        su_daddr_t daddr;
        tb_rbg2s_t* rbs = ctx;
        su_ret_t rc = SU_SUCCESS;

        CHK_RBS(rbs);

        /* We do not support seeking to random pos, _yet_ !!!!!!!!! */
        ss_dassert(DBE_BLOBG2SIZE_CMP(offset, rbs->rbs_readpos) == 0);
        if (rbs->rbs_pos_in_array >=
            rbs->rbs_current_row.rbr_num_pages_in_array)
        {
            ss_dassert(rbs->rbs_pos_in_array ==
                       rbs->rbs_current_row.rbr_num_pages_in_array);
            if (rbs->rbs_prefetch_row.rbr_num_pages_in_array != 0) {
                rbs->rbs_current_row = rbs->rbs_prefetch_row;
                rbs->rbs_prefetch_row.rbr_num_pages_in_array = 0;
            } else if (!rbs->rbs_cursor_at_end) {
                rc = rbg2s_loadrow(rbs, &rbs->rbs_current_row, offset);
            } else {
                rc = DBE_RC_END;
            }
            if (rc == DBE_RC_END) {
                ss_derror;
                return (SU_DADDR_NULL);
            }
            ss_rc_dassert(rc == SU_SUCCESS, rc);
            rbs->rbs_pos_in_array = 0;
        }
        ss_dassert(rbs->rbs_pos_in_array <
                   rbs->rbs_current_row.rbr_num_pages_in_array);
        ss_dprintf_1(("rbg2s_getpageaddrfun: daddr=%lu\n",
                      (ulong)rbs->rbs_current_row.rbr_pagearray[
                              rbs->rbs_pos_in_array].bp_addr));
        daddr = rbs->rbs_current_row.rbr_pagearray[
                rbs->rbs_pos_in_array++].bp_addr;
        if (rbs->rbs_prefetch_numpages_now > 0) {
            rbs->rbs_prefetch_numpages_now--;
        }
        rbg2s_prefetch(rbs);
        return (daddr);
}

/*#***********************************************************************\
 *
 *      tb_rbg2s_init
 *
 * creates a new Read BLOB Stream
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      atype - in, use
 *          expression/column type
 *
 *      aval - in, use
 *          expression/column value
 *
 *      p_totalsize - out
 *          pointer to variable where the total size (in bytes)
 *          will be stored
 *
 * Return value - give:
 *      new Read BLOB Stream
 *
 * Limitations:
 *
 * Globals used:
 */
static tb_rbg2s_t* tb_rbg2s_init(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dbe_blobg2size_t* p_totalsize)
{
        tb_rbg2s_t* rbs = SSMEM_NEW(tb_rbg2s_t);
        dbe_vablobg2ref_t bref;

        rbs->rbs_check = TBCHK_BLOBG2READSTREAM;
        rbs->rbs_db = rs_sysi_db(cd);
        rbs->rbs_cd = cd;
        dbe_brefg2_loadfromaval(&bref, cd, atype, aval);
        rbs->rbs_id = dbe_brefg2_getblobg2id(&bref);
        rbs->rbs_size = dbe_brefg2_getblobg2size(&bref);
        if (p_totalsize != NULL) {
            *p_totalsize = rbs->rbs_size;
        }
        DBE_BLOBG2SIZE_ASSIGN_SIZE(&rbs->rbs_readpos, 0);
        rbs->rbs_dberblob =
            dbe_rblobg2_init(
                    rbs->rbs_db,
                    rbs->rbs_id,
                    rbg2s_getpageaddrfun,
                    rbs);
        rbs->rbs_sysconnect = NULL;
        rbs->rbs_sysblobscursor = NULL;
        rbs->rbs_pos_in_array = 0;
        rbs->rbs_current_row.rbr_num_pages_in_array = 0;
        DBE_BLOBG2SIZE_SET2UINT4S(&rbs->rbs_current_row.rbr_endsize, 0, 0);
        rbs->rbs_prefetch_row.rbr_num_pages_in_array = 0;
        rbs->rbs_prefetch_row.rbr_endsize = rbs->rbs_current_row.rbr_endsize;
        rbs->rbs_cursor_at_end = FALSE;
        rbs->rbs_prefetch_numpages_max = dbe_db_getreadaheadsize(rbs->rbs_db);
        if (rbs->rbs_prefetch_numpages_max > BS_PAGEARR_SIZE) {
            rbs->rbs_prefetch_numpages_max = BS_PAGEARR_SIZE;
        }
        rbs->rbs_prefetch_numpages_now = 0;
        rbs->rbs_status = BS_STAT_RELEASED;

        return (rbs);
}

/*#***********************************************************************\
 *
 *      tb_rbg2s_reach
 *
 * reaches a read buffer from a Read BLOB Stream
 *
 * Parameters:
 *      rbs - in out, use
 *          Read BLOB Stream
 *
 *      pp_buf - out, ref
 *          for output of read(only) buffer
 *
 *      p_nbytes - out, use
 *          for output of number of bytes available for reading
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t tb_rbg2s_reach(
        tb_rbg2s_t* rbs,
        ss_byte_t** pp_buf,
        size_t* p_nbytes,
        su_err_t** p_errh __attribute__ ((unused)))
{
        su_ret_t rc;

        CHK_RBS(rbs);
        ss_rc_dassert(rbs->rbs_status == BS_STAT_RELEASED, rbs->rbs_status);
        if (DBE_BLOBG2SIZE_CMP(rbs->rbs_readpos, rbs->rbs_size) >= 0) {
            ss_dassert(DBE_BLOBG2SIZE_CMP(rbs->rbs_readpos, rbs->rbs_size) == 0);
            rc = DBE_RC_END;
            *p_nbytes = 0;
            *pp_buf = NULL;
        } else {
            rc = dbe_rblobg2_reach(
                    rbs->rbs_dberblob,
                    pp_buf,
                    p_nbytes);
            if (rc == SU_SUCCESS) {
                rbs->rbs_status = BS_STAT_REACHED;
            }
        }
        return (rc);
}


/*#***********************************************************************\
 *
 *      tb_rbg2s_release
 *
 * Releases a reached BLOB read buffer
 *
 * Parameters:
 *      rbs - in out, use
 *          Read BLOB Stream
 *
 *      nbytes - in
 *          number of bytes read
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
static su_ret_t tb_rbg2s_release(
        tb_rbg2s_t* rbs,
        size_t nbytes,
        su_err_t** p_errh __attribute__ ((unused)))
{
        su_ret_t rc = SU_SUCCESS;

        CHK_RBS(rbs);
        ss_rc_dassert(rbs->rbs_status == BS_STAT_REACHED, rbs->rbs_status);
        DBE_BLOBG2SIZE_ADDASSIGN_SIZE(&rbs->rbs_readpos, (ss_uint4_t)nbytes);
        dbe_rblobg2_release(rbs->rbs_dberblob, nbytes);
        rbs->rbs_status = BS_STAT_RELEASED;
        return (rc);
}

/*#***********************************************************************\
 *
 *      tb_rbg2s_done
 *
 * Frees a Read BLOB Stream object
 *
 * Parameters:
 *      rbs - in, take
 *          Read BLOB Stream
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void tb_rbg2s_done(tb_rbg2s_t* rbs)
{
        CHK_RBS(rbs);
        if (rbs->rbs_status == BS_STAT_REACHED) {
            tb_rbg2s_release(rbs, 0, NULL);
        }
        blobg2mgr_donesysblobsconnectionandcursor(
                &rbs->rbs_sysconnect,
                &rbs->rbs_sysblobscursor);
        dbe_rblobg2_done(rbs->rbs_dberblob);
        rbs->rbs_check = TBCHK_FREEDBLOBG2READSTREAM;
        SsMemFree(rbs);
}


/*##**********************************************************************\
 *
 *      tb_blobg2_loadblobtoaval_limit
 *
 * Loads BLOB data to aval object (into memory) if the size does not
 * exceed the given limit. otherwise does nothing
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      atype - in, use
 *          type of the value
 *
 *      aval - in out, use
 *          atype where the value would be loaded
 *
 *      sizelimit_or_0 - <usage>
 *          max number of bytes that is allowed to e loaded.
 *          0 means: load unconditionally.
 *          Note: the size is net size end nul-byte and va-header
 *          length is not counted.
 *
 * Return value:
 *      TRUE - the value was loaded into aval,
 *      FALSE - the BLOB size exceeded the limit, aval remains
 *              untouched.
 *
 * Limitations:
 *
 * Globals used:
 */
bool tb_blobg2_loadblobtoaval_limit(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        size_t sizelimit_or_0)
{
        dbe_blobg2size_t blobsize;
        bool readp = TRUE;

        ss_dassert(rs_aval_isblob(cd, atype, aval));
        blobsize = dbe_brefg2_getsizefromaval(cd, atype, aval);
        if (sizelimit_or_0 != 0) {
            dbe_blobg2size_t sizelimit;

            DBE_BLOBG2SIZE_ASSIGN_SIZE(&sizelimit, (ss_uint4_t)sizelimit_or_0);
            if (DBE_BLOBG2SIZE_CMP(blobsize, sizelimit) > 0) {
                readp = FALSE;
            }
        }
        if (readp) {
            su_ret_t rc;
            bool succp;
            tb_rblobg2stream_t* rbs;
            dbe_blobg2size_t totalsize;
            ss_byte_t* p_data;
            ulong datasize;
            ulong ncopied;
            size_t nbytes_avail;
            size_t ntocopy;
            ss_byte_t* p_blobdata;
            ulong blobsize_low;
            rs_sqldatatype_t new_sqldt;
            rs_atype_t* new_atype;
            rs_aval_t* new_aval;

            rbs = tb_rblobg2stream_init(cd, atype, aval, &totalsize);
            ss_dassert(rbs != NULL);
            ss_dassert(DBE_BLOBG2SIZE_CMP(totalsize, blobsize) == 0);
            blobsize_low = DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(blobsize);
            ss_dassert(DBE_BLOBG2SIZE_GETMOSTSIGNIFICANTUINT4(blobsize) == 0);

            switch (rs_atype_datatype(cd, atype)) {
                case RSDT_CHAR:
                    new_sqldt = RSSQLDT_LONGVARCHAR;
                    break;
                case RSDT_UNICODE:
                    new_sqldt = RSSQLDT_WLONGVARCHAR;
                    break;
                default:
                    ss_rc_derror(rs_atype_datatype(cd, atype));
                    /* FALLTHROUGH */
                case RSDT_BINARY:
                    new_sqldt = RSSQLDT_LONGVARBINARY;
                    break;
            }
            new_atype = rs_atype_initbysqldt(cd, new_sqldt, -1L, -1L);
            new_aval = rs_aval_create(cd, new_atype);
            succp = rs_aval_setbdata_ext(
                    cd,
                    new_atype,
                    new_aval,
                    NULL,
                    blobsize_low,
                    NULL);
            ss_rc_dassert(succp == RSAVR_SUCCESS, succp);
            p_data = rs_aval_getdata(cd, atype, new_aval, &datasize);
            ncopied = 0;

            do {
                rc = tb_rblobg2stream_reach(
                        rbs,
                        &p_blobdata,
                        &nbytes_avail,
                        NULL);
                ss_rc_dassert(rc == SU_SUCCESS, rc);

                if (nbytes_avail <= (blobsize_low - ncopied)) {
                    ntocopy = nbytes_avail;
                } else {
                    ss_derror;
                    ntocopy = (size_t)(blobsize_low - ncopied);
                }
                ss_dprintf_2(("tb_blobg2_loadblobtoaval_limit:ncopied=%ld, ntocopy=%ld\n", (long)ncopied, (long)ntocopy));
                memcpy(p_data + (size_t)ncopied, p_blobdata, ntocopy);
                ncopied = (ulong)(ncopied + ntocopy);
                rc = tb_rblobg2stream_release(rbs, ntocopy, NULL);
                ss_rc_dassert(rc == SU_SUCCESS, rc);
            } while (ncopied < blobsize_low);
            tb_rblobg2stream_done(rbs);
            succp = rs_aval_assign_ext(cd, atype, aval, new_atype, new_aval, NULL);
            ss_dassert(succp != RSAVR_FAILURE);
            rs_aval_free(cd, new_atype, new_aval);
            rs_atype_free(cd, new_atype);
        }
        return (readp);
}

/*##**********************************************************************\
 *
 *		tb_blobg2_readsmallblobstotvalwithinfo
 *
 * Reads small BLOBs to tval as normal avals, the big
 * ones are preserved as a reference.
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      ttype - in, use
 *          tuple type
 *
 *      tval - in out, use
 *          tuple value
 *
 *      smallblobsizemax - in
 *          max size of blob that will be read to aval
 *
 *      p_nblobs_read - out, use
 *          pointer to variable telling the number of blobs
 *          read in (ie. count of blobs the size of which is
 *          below or equal to smallblobsizemax)
 *
 *      p_nblobs_total - out, use
 *          pointer t variable telling the number of blob attributes
 *
 * Return value :
 *      SU_SUCCESS when successful
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_ret_t tb_blobg2_readsmallblobstotvalwithinfo(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        size_t smallblobsizemax_size_t,
        uint* p_nblobs_read,
        uint* p_nblobs_total)
{
        int i;
        rs_aval_t* aval;
        rs_atype_t* atype;

        *p_nblobs_read = *p_nblobs_total = 0;
        i = -1; /* Initial value for scanning. */
        while (rs_tval_scanblobs(cd, ttype, tval, &i)) {
            bool readp;
            atype = rs_ttype_atype(cd, ttype, i);
            aval = rs_tval_aval(cd, ttype, tval, i);
            ss_dassert(rs_aval_isblob(cd, atype, aval));
            ss_dprintf_2((
                    "tb_blobg2_readsmallblobstotvalwithinfo:blob at id %d\n",
                    i));
            (*p_nblobs_total)++;
            readp = tb_blobg2_loadblobtoaval_limit(cd,
                                                   atype,
                                                   aval,
                                                   smallblobsizemax_size_t);
            ss_dprintf_2((
                    "tb_blobg2_readsmallblobstotvalwithinfo:read blob=%d\n",
                    (int)readp));
            ss_rc_dassert(readp == 0 || readp == 1, readp);
            (*p_nblobs_read) += readp;
        }
        return (SU_SUCCESS);
}

/*##**********************************************************************\
 *
 *		tb_blobg2_readsmallblobstotval
 *
 * Reads small BLOBs to tval as normal avals, the big
 * ones are preserved as a reference.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	ttype - in, use
 *		tuple type
 *
 *	tval - in out, use
 *		tuple value
 *
 *	smallblobsizemax - in
 *		max size of blob that will be read to aval
 *
 * Return value :
 *      SU_SUCCESS when successful
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_ret_t tb_blobg2_readsmallblobstotval(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        size_t smallblobsizemax)
{
        su_ret_t rc;
        uint nblobs_read;
        uint nblobs_total;

        rc = tb_blobg2_readsmallblobstotvalwithinfo(
                cd,
                ttype, tval,
                smallblobsizemax, &nblobs_read, &nblobs_total);
        return (rc);
}


/* Below are the data structures plus methods needed for
 * implementation of "compatibility-mode" Read BLOB Stream.
 * which seamlessly can handle older BLOB format and this G2
 * BLOB format
 */
typedef enum {
    RBTYPE_G2BLOB, /* new */
    RBTYPE_OLDBLOB /* old */
} rbtype_t;

struct tb_rblobg2stream_st {
        int rbw_check;
        rbtype_t rbw_type; /*  RBTYPE_G2BLOB or RBTYPE_OLDBLOB */
        union {
                dbe_rblob_t* old_rblob;
                tb_rbg2s_t* g2_rblob;
        } rbw_;
};

#define CHK_RBW(rbw) \
        ss_dassert(SS_CHKPTR(rbw) &&\
                   rbw->rbw_check == TBCHK_BLOBREADSTREAMWRAPPER)

/*##**********************************************************************\
 *
 *      tb_rblobg2stream_init
 *
 * Creates a new Read BLOB Stream, the BLOB format is checked
 * from the reference record in the aval
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      atype - in, use
 *          type of aval
 *
 *      aval - in, use
 *          aval that must be a BLOB reference
 *
 *      p_totalsize - out
 *          total BLOB size
 *
 * Return value - give:
 *      New Read BLOB Stream
 *
 * Limitations:
 *
 * Globals used:
 */
tb_rblobg2stream_t* tb_rblobg2stream_init(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dbe_blobg2size_t* p_totalsize)
{
        va_t* va;
        tb_rblobg2stream_t* rbw = SSMEM_NEW(tb_rblobg2stream_t);

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        ss_dassert(rs_aval_isblob(cd, atype, aval));
        rbw->rbw_check = TBCHK_BLOBREADSTREAMWRAPPER;
        va = rs_aval_va(cd, atype, aval);
        if (dbe_brefg2_isblobg2check_from_va(va)) {
            rbw->rbw_type = RBTYPE_G2BLOB;
            rbw->rbw_.g2_rblob =
                tb_rbg2s_init(
                        cd,
                        atype,
                        aval,
                        p_totalsize);
        } else {
            dbe_blobsize_t oldblob_size;
            rbw->rbw_type = RBTYPE_OLDBLOB;
            rbw->rbw_.old_rblob =
                dbe_rblob_init(
                        rs_sysi_db(cd),
                        va,
                        &oldblob_size);
            DBE_BLOBG2SIZE_ASSIGN_SIZE(p_totalsize, oldblob_size);
        }
        ss_debug(CHK_RBW(rbw));
        return (rbw);
}

/*##**********************************************************************\
 *
 *      tb_rblobg2stream_done
 *
 * Frees a Read BLOB Stream wrapper object plus the REAL read blob stream.
 *
 * Parameters:
 *      rbw - in, take
 *          Read BLOB Stream
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void tb_rblobg2stream_done(tb_rblobg2stream_t* rbw)
{
        CHK_RBW(rbw);

        switch(rbw->rbw_type) {
            case RBTYPE_G2BLOB:
                tb_rbg2s_done(rbw->rbw_.g2_rblob);
                break;
            default:
                ss_rc_derror(rbw->rbw_type);
            case RBTYPE_OLDBLOB:
                dbe_rblob_done(rbw->rbw_.old_rblob);
                break;
        }
        rbw->rbw_check = TBCHK_FREEDBLOBREADSTREAMWRAPPER;
        SsMemFree(rbw);
}

/*##**********************************************************************\
 *
 *      tb_rblobg2stream_reach
 *
 * Reaches a read buffer.
 *
 * Parameters:
 *      rbw - in out, use
 *          Read BLOB Stream
 *
 *      pp_buf - out, ref
 *          pointer to pointer where the reference to read (only)
 *          buffer will be put
 *
 *      p_nbytes - out
 *          for output of number of bytes available for reading
 *
 *      p_errh - out, give
 *          for output of error handle
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_rblobg2stream_reach(
        tb_rblobg2stream_t* rbw,
        ss_byte_t** pp_buf,
        size_t* p_nbytes,
        su_err_t** p_errh)
{
        su_ret_t rc;

        CHK_RBW(rbw);
        switch(rbw->rbw_type) {
            case RBTYPE_G2BLOB:
                rc = tb_rbg2s_reach(rbw->rbw_.g2_rblob,
                                            pp_buf,
                                            p_nbytes,
                                            p_errh);
                break;
            default:
                ss_rc_derror(rbw->rbw_type);
            case RBTYPE_OLDBLOB:
                *pp_buf = (ss_byte_t*)dbe_rblob_reach(rbw->rbw_.old_rblob, p_nbytes);
                if (NULL == *pp_buf || 0 == *p_nbytes) {
                    ss_dassert(NULL == *pp_buf && 0 == *p_nbytes);
                    rc = DBE_RC_END;
                } else {
                    rc = SU_SUCCESS;
                }
                break;
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *      tb_rblobg2stream_release
 *
 * releases a reached read buffer
 *
 * Parameters:
 *      rbw - in out, use
 *          Read BLOB Stream
 *
 *      nbytes - in
 *          number of bytes read
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_rblobg2stream_release(
        tb_rblobg2stream_t* rbw,
        size_t nbytes,
        su_err_t** p_errh)
{
        su_ret_t rc;

        CHK_RBW(rbw);
        switch(rbw->rbw_type) {
            case RBTYPE_G2BLOB:
                rc = tb_rbg2s_release(rbw->rbw_.g2_rblob,
                                              nbytes,
                                              p_errh);
                break;
            default:
                ss_rc_derror(rbw->rbw_type);
            case RBTYPE_OLDBLOB:
                rc = SU_SUCCESS;
                dbe_rblob_release(rbw->rbw_.old_rblob, nbytes);
                break;
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_copy_old_blob_to_g2
 *
 * copies old format BLOB referenced by the aval parameter to new format
 * and also changes the reference to point to the new format BLOB
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      atype - in, use
 *          type of aval
 *
 *      aval - in out, use
 *          attribute type, on input a reference to old format BLOB
 *          on output a reference to new format BLOB with same value.
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_copy_old_blob_to_g2(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        su_err_t** p_errh)
{
        su_ret_t rc;
        tb_wblobg2stream_t* wbs;
        dbe_blobg2size_t totalsize;
        dbe_blobg2size_t n_copied;
        tb_rblobg2stream_t* rbs =
            tb_rblobg2stream_init(cd, atype, aval, &totalsize);

        ss_dassert(rbs->rbw_type == RBTYPE_OLDBLOB);
        ss_dassert(DBE_BLOBG2SIZE_GETMOSTSIGNIFICANTUINT4(totalsize) == 0);
        wbs = tb_blobg2mgr_initwblobstream_bycd(cd, atype, aval);
        DBE_BLOBG2SIZE_ASSIGN_SIZE(&n_copied, 0);
        while (DBE_BLOBG2SIZE_CMP(n_copied, totalsize) < 0) {
            ss_byte_t* readbuf;
            ss_byte_t* writebuf;
            size_t n_avail_in_readbuf;
            size_t n_avail_in_writebuf;
            size_t n_to_copy;
            rc = tb_rblobg2stream_reach(rbs,
                                        &readbuf,
                                        &n_avail_in_readbuf,
                                        p_errh);
            if (rc != SU_SUCCESS) {
                ss_rc_derror(rc);
                goto error_cleanup;
            }
            rc = tb_wblobg2stream_reach(wbs,
                                        &writebuf,
                                        &n_avail_in_writebuf,
                                        p_errh);
            if (rc != SU_SUCCESS) {
                /* should not fail in reach */
                ss_rc_derror(rc);
                goto error_cleanup;
            }
            n_to_copy = SS_MIN(n_avail_in_writebuf, n_avail_in_readbuf);
            memcpy(writebuf, readbuf, n_to_copy);
            rc = tb_rblobg2stream_release(rbs, n_to_copy, p_errh);
            if (rc != SU_SUCCESS) {
                ss_rc_derror(rc);
                goto error_cleanup;
            }
            rc = tb_wblobg2stream_release(wbs, n_to_copy, p_errh);
            if (rc != SU_SUCCESS) {
                goto error_cleanup;
            }
            DBE_BLOBG2SIZE_ADDASSIGN_SIZE(&n_copied, (ss_uint4_t)n_to_copy);
        }
        rc = tb_wblobg2stream_done(wbs, p_errh);
 return_rc:;
        tb_rblobg2stream_done(rbs);
        return (rc);
 error_cleanup:;
        tb_wblobg2stream_abort(wbs);
        goto return_rc;
}

/*##**********************************************************************\
 *
 *      tb_blobg2mgr_move_page
 *
 * Moves a BLOB g2 page to new disk address
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      old_daddr - in
 *          old disk address of the page
 *
 *      new_daddr - in
 *          new disk address
 *
 *      page_data - in, take
 *          pointer to data buffer containing the page contents
 *
 *      page_slot - in, take
 *          reached cache slot for the page
 *
 * Return value:
 *      SU_SUCCESS - page succefully moved
 *      DBE_RC_NOTFOUND - page was not found from SYS_BLOBS
 *      DBE_ERR_FAILED - (should not happen in practice)
 *      failed to update the SYS_BLOBS row
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t tb_blobg2mgr_move_page(
        rs_sysi_t* cd,
        su_daddr_t old_daddr,
        su_daddr_t new_daddr,
        ss_byte_t* page_data,
        void* page_slot)
{
        uint i;
        dbe_blobg2id_t bid;
        dbe_blobg2size_t startpos;
        dbe_blobg2size_t endpos;
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT tlirc;
        su_ret_t rc = SU_SUCCESS;
        tb_blobg2mgr_t* bm;
        char colname[16];
        size_t num_pages_in_array;
        bpagerec_t pagearr[BS_PAGEARR_SIZE];
        ss_int4_t startcpnum_bind;

        bm = tb_connect_getblobg2mgr(rs_sysi_tbcon(cd));
        CHK_BM(bm);
        dbe_blobg2_get_id_and_endpos_from_page(
                cd,
                page_data,
                &bid,
                &startpos,
                &endpos);
        blobg2_initsysblobsconnectionandcursor(
                cd,
                &tcon,
                &tcur,
                FALSE);
        tlirc = TliCursorConstrInt8t(tcur,
                                     RS_ANAME_BLOBS_ID,
                                     TLI_RELOP_EQUAL,
                                     bid);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorConstrInt8t(tcur,
                                     RS_ANAME_BLOBS_ENDSIZE,
                                     TLI_RELOP_GE,
                                     endpos);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColSizet(
                    tcur,
                    RS_ANAME_BLOBS_NUMPAGES,
                    &num_pages_in_array);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorColInt4t(
                    tcur,
                    RS_ANAME_BLOBS_STARTCPNUM,
                    &startcpnum_bind);
        for (i = 0; i < BS_PAGEARR_SIZE; i++) {
            ss_dassert(sizeof(colname) > strlen(RS_ANAME_BLOBS_PAGE_ADDR_TEMPL));
            SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ADDR_TEMPL, i + 1);
            ss_dassert(sizeof(colname) > strlen(colname));
            TliCursorColInt4t(tcur,
                              colname,
                              (ss_int4_t*)&pagearr[i].bp_addr);
            SsSprintf(colname, RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL, i + 1);
            ss_dassert(sizeof(colname) > strlen(colname));
            tlirc = TliCursorColInt8t(tcur,
                                      colname,
                                      &pagearr[i].bp_size_so_far);
        }
        tlirc = TliCursorOpen(tcur);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        tlirc = TliCursorNext(tcur);
        if (tlirc == TLI_RC_END) {
            rc = DBE_RC_NOTFOUND;
            goto failed_return;
        } else if (tlirc != TLI_RC_SUCC) {
            ss_rc_derror(tlirc);
            rc = DBE_ERR_FAILED;
            goto failed_return;
        }
        for (i = 0; i < num_pages_in_array; i++) {
            if (pagearr[i].bp_addr == old_daddr) {
                ss_dassert(DBE_BLOBG2SIZE_CMP(pagearr[i].bp_size_so_far,
                                              startpos) == 0);
                break;
            }
        }
        if (i >= num_pages_in_array) {
            ss_dassert(i == num_pages_in_array);
            rc = DBE_RC_NOTFOUND;
            goto failed_return;
        }
        pagearr[i].bp_addr = new_daddr;
        tlirc = TliCursorUpdate(tcur);
        ss_rc_dassert(tlirc == TLI_RC_SUCC, tlirc);
        rc = dbe_blobg2_relocate_page(cd,
                                      page_data,
                                      page_slot,
                                      new_daddr);
        tlirc = TliCommit(tcon);
        ss_rc_assert(tlirc == TLI_RC_SUCC, tlirc);
        blobg2mgr_donesysblobsconnectionandcursor(&tcon, &tcur);
        rc = dbe_db_free_n_pages(rs_sysi_db(cd),
                                 1,
                                 &old_daddr,
                                 (dbe_cpnum_t)startcpnum_bind,
                                 FALSE);
        return (rc);
 failed_return:;
        TliRollback(tcon);
        blobg2mgr_donesysblobsconnectionandcursor(&tcon, &tcur);
        return (rc);
}

