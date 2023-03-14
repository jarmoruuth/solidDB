/*************************************************************************\
**  source       * dbe7ctr.c
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

#define DBE7CTR_C

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssint8.h>
#include <sspmon.h>

#include "dbe8srec.h"
#include "dbe7cfg.h"
#include "dbe7ctr.h"
#include "dbe4srli.h"
#include "dbe0type.h"

#define COUNTER_BIGINC  10000

#define CTR_SEM_ENTER_TRXID(ctr)            SsQsemEnter(ctr->ctr_mutex1)
#define CTR_SEM_ENTER_MAXTRXNUM(ctr)        SsQsemEnter(ctr->ctr_mutex2)
#define CTR_SEM_ENTER_COMMITTRXNUM(ctr)     SsQsemEnter(ctr->ctr_mutex3)
#define CTR_SEM_ENTER_MERGETRXNUM(ctr)      SsQsemEnter(ctr->ctr_mutex4)
#define CTR_SEM_ENTER_STORAGETRXNUM(ctr)    SsQsemEnter(ctr->ctr_mutex5)
#define CTR_SEM_ENTER_CPNUM(ctr)            SsQsemEnter(ctr->ctr_mutex0)
#define CTR_SEM_ENTER_TUPLENUM(ctr)         SsQsemEnter(ctr->ctr_mutex7)
#define CTR_SEM_ENTER_ID(ctr)               SsQsemEnter(ctr->ctr_mutex0)
#define CTR_SEM_ENTER_LOGFNUM(ctr)          SsQsemEnter(ctr->ctr_mutex0)
#define CTR_SEM_ENTER_BLOB(ctr)             SsQsemEnter(ctr->ctr_mutex0)
#define CTR_SEM_ENTER_TUPLEVERSION(ctr)     SsQsemEnter(ctr->ctr_mutex6)
#define CTR_SEM_ENTER_FLOW(ctr)             SsQsemEnter(ctr->ctr_mutex0)

#define CTR_SEM_EXIT_TRXID(ctr)             SsQsemExit(ctr->ctr_mutex1)
#define CTR_SEM_EXIT_MAXTRXNUM(ctr)         SsQsemExit(ctr->ctr_mutex2)
#define CTR_SEM_EXIT_COMMITTRXNUM(ctr)      SsQsemExit(ctr->ctr_mutex3)
#define CTR_SEM_EXIT_MERGETRXNUM(ctr)       SsQsemExit(ctr->ctr_mutex4)
#define CTR_SEM_EXIT_STORAGETRXNUM(ctr)     SsQsemExit(ctr->ctr_mutex5)
#define CTR_SEM_EXIT_CPNUM(ctr)             SsQsemExit(ctr->ctr_mutex0)
#define CTR_SEM_EXIT_TUPLENUM(ctr)          SsQsemExit(ctr->ctr_mutex7)
#define CTR_SEM_EXIT_ID(ctr)                SsQsemExit(ctr->ctr_mutex0)
#define CTR_SEM_EXIT_LOGFNUM(ctr)           SsQsemExit(ctr->ctr_mutex0)
#define CTR_SEM_EXIT_BLOB(ctr)              SsQsemExit(ctr->ctr_mutex0)
#define CTR_SEM_EXIT_TUPLEVERSION(ctr)      SsQsemExit(ctr->ctr_mutex6)
#define CTR_SEM_EXIT_FLOW(ctr)              SsQsemExit(ctr->ctr_mutex0)

#define CTR_SEM_ENTER_ALL(ctr)              {SsQsemEnter(ctr->ctr_mutex0);\
                                             SsQsemEnter(ctr->ctr_mutex1);\
                                             SsQsemEnter(ctr->ctr_mutex2);\
                                             SsQsemEnter(ctr->ctr_mutex3);\
                                             SsQsemEnter(ctr->ctr_mutex4);\
                                             SsQsemEnter(ctr->ctr_mutex5);\
                                             SsQsemEnter(ctr->ctr_mutex6);\
                                             SsQsemEnter(ctr->ctr_mutex7);\
                                            }
#define CTR_SEM_EXIT_ALL(ctr)               {SsQsemExit(ctr->ctr_mutex7);\
                                             SsQsemExit(ctr->ctr_mutex6);\
                                             SsQsemExit(ctr->ctr_mutex5);\
                                             SsQsemExit(ctr->ctr_mutex4);\
                                             SsQsemExit(ctr->ctr_mutex3);\
                                             SsQsemExit(ctr->ctr_mutex2);\
                                             SsQsemExit(ctr->ctr_mutex1);\
                                             SsQsemExit(ctr->ctr_mutex0);\
                                            }



static void trxctr_init(trxctr_t* trxctr, ss_uint4_t low, ss_uint4_t high)
{
        trxctr->tc_lo = low;
        trxctr->tc_hi = high;
}

static ss_uint4_t trxctr_inc(trxctr_t* trxctr, ss_int4_t increment)
{
        ss_uint4_t nv = trxctr->tc_lo + (ss_uint4_t)increment;

        ss_dassert(increment >= 0);
        if ((ss_uint4_t)(nv - 1) >= (ss_uint4_t)(DBE_CTR_MAXLIMIT - 1)) {
            nv = 1;
            trxctr->tc_hi++;
            ss_dprintf_2(("%s:trxctr_inc: counter low wraparound, high = %lu\n",
                          __FILE__,
                          (ulong)trxctr->tc_hi));
        }
        trxctr->tc_lo = nv;
        ss_dprintf_1(("trxctr_inc:increment=%d, tc_hi=%lu, tc_lo=%lu\n",
                          (int)increment, (ulong)trxctr->tc_hi, (ulong)trxctr->tc_lo));
        return (nv);
}

static int trxctr_cmp(trxctr_t* trxctr1, trxctr_t* trxctr2)
{
        ss_uint4_t cmp1;
        ss_uint4_t cmp2;

        if (trxctr1->tc_hi == trxctr2->tc_hi) {
            cmp1 = trxctr1->tc_lo;
            cmp2 = trxctr2->tc_lo;
        } else {
            cmp1 = trxctr1->tc_hi;
            cmp2 = trxctr2->tc_hi;
        }
        if (cmp1 < cmp2) {
            return(-1);
        } else if (cmp1 > cmp2) {
            return(1);
        } else {
            return(0);
        }
}

/*#***********************************************************************\
 *
 *          trxctr_calculatehigh4bytes
 *
 * Calculates the value for high 4 bytes of a trxid or trxnum by the
 * current value of counter and the value of low 4 bytes assuming
 * at most 1 increment to high value can have occured in between.
 *
 * Parameters :
 *       trxctr - in, use
 *           pointer to current trx counter
 *
 *       low4bytes - in
 *           lowest 32 bits of the trx(id|num)
 *
 * Return value :
 *       value for high 32 bits for the given trx(id|num)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static ss_uint4_t trxctr_calculatehigh4bytes(trxctr_t* trxctr, ss_uint4_t low4bytes)
{
        ss_int4_t diff = (ss_int4_t)(trxctr->tc_lo - low4bytes);
        ss_uint4_t hi;

        if (diff < 0 && low4bytes < trxctr->tc_lo) {
            hi = (trxctr->tc_hi + 1U);
        } else if (diff > 0 && low4bytes > trxctr->tc_lo) {
            hi = (trxctr->tc_hi - 1U);
        } else {
            hi = trxctr->tc_hi;
        }
        ss_dassert((ss_int4_t)hi >= 0);
        return (hi);
}

static void trxctr_setlow4bytes(trxctr_t* trxctr, ss_uint4_t low4bytes)
{
        ss_uint4_t high4bytes = trxctr_calculatehigh4bytes(trxctr, low4bytes);
        trxctr->tc_hi = high4bytes;
        trxctr->tc_lo = low4bytes;
        ss_dprintf_1(("trxctr_setlow4bytes:tc_hi=%lu, tc_lo=%lu\n",
                          (ulong)trxctr->tc_hi, (ulong)trxctr->tc_lo));
}

#ifdef SS_FAKE
static void trxctr_setlow4bytes_raw(trxctr_t* trxctr, ss_uint4_t low4bytes)
{
        trxctr->tc_lo = low4bytes;
        ss_dprintf_1(("trxctr_setlow4bytes_raw:tc_hi=%lu, tc_lo=%lu\n",
                          (ulong)trxctr->tc_hi, (ulong)trxctr->tc_lo));
}
#endif /* SS_FAKE */

#ifdef SS_FAKE
static void counter_fakereset(dbe_counter_t* ctr)
{
        FAKE_CODE_RESET(
            FAKE_DBE_COUNTERWRAP_TOZERO,
            {
                ss_dprintf_1(("counter_fakereset %ld\n", DBE_CTR_MAXLIMIT - 100));
                trxctr_setlow4bytes_raw(&ctr->ctr_trxid, DBE_CTR_MAXLIMIT - 100);
                trxctr_setlow4bytes_raw(&ctr->ctr_committrxnum, DBE_CTR_MAXLIMIT - 100);
                trxctr_setlow4bytes_raw(&ctr->ctr_maxtrxnum, DBE_CTR_MAXLIMIT - 100);
                trxctr_setlow4bytes_raw(&ctr->ctr_mergetrxnum, DBE_CTR_MAXLIMIT - 100);
                dbe_type_updateconst(ctr);
            }
        );
        FAKE_CODE_RESET(
            FAKE_DBE_COUNTERWRAP_TONEGATIVE,
            {
                ss_dprintf_1(("counter_fakereset %ld\n", SS_INT4_MAX - 100));
                trxctr_setlow4bytes_raw(&ctr->ctr_trxid, SS_INT4_MAX - 100);
                trxctr_setlow4bytes_raw(&ctr->ctr_committrxnum, SS_INT4_MAX - 100);
                trxctr_setlow4bytes_raw(&ctr->ctr_maxtrxnum, SS_INT4_MAX - 100);
                trxctr_setlow4bytes_raw(&ctr->ctr_mergetrxnum, SS_INT4_MAX - 100);
                dbe_type_updateconst(ctr);
            }
        );
}
#endif /* SS_FAKE */

/*##**********************************************************************\
 *
 *              dbe_counter_init
 *
 *
 *
 * Parameters :
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_counter_t* dbe_counter_init(void)
{
        dbe_counter_t* ctr;

        ctr = SsMemCalloc(sizeof(dbe_counter_t), 1);

        trxctr_init(&ctr->ctr_trxid, 0, 0);
        trxctr_init(&ctr->ctr_maxtrxnum, 1, 0);
        trxctr_init(&ctr->ctr_committrxnum, 0, 0);
        trxctr_init(&ctr->ctr_mergetrxnum, 0, 0);
        trxctr_init(&ctr->ctr_storagetrxnum, 0, 0);
        ctr->ctr_cpnum = 1L;
        rs_tuplenum_init(&ctr->ctr_tuplenum);
        ctr->ctr_attrid = DBE_SYS_SQL_ID_START;
        ctr->ctr_keyid = DBE_SYS_SQL_ID_START;
        ctr->ctr_userid = 1L;
        ctr->ctr_logfnum = 1L;
        DBE_BLOBG2ID_SET2UINT4S(&(ctr->ctr_blobg2id), 0, 1);
        ctr->ctr_mergectr = 0;
        rs_tuplenum_init(&ctr->ctr_tupleversion);
#ifdef SS_SYNC
        ctr->ctr_syncmsgid = 0;
        rs_tuplenum_init(&ctr->ctr_synctupleversion);
#endif
        ctr->ctr_convert = FALSE;
        ctr->ctr_attrid_used = NULL;
        ctr->ctr_attrid_idx = -1;
        ctr->ctr_keyid_used = NULL;
        ctr->ctr_keyid_idx = -1;
        ctr->ctr_activemergetrxnum = DBE_TRXNUM_NULL;

        ctr->ctr_mutex0 = SsQsemCreateLocal(SS_SEMNUM_DBE_CTR0);
        ctr->ctr_mutex1 = SsQsemCreateLocal(SS_SEMNUM_DBE_CTR1);
        ctr->ctr_mutex2 = SsQsemCreateLocal(SS_SEMNUM_DBE_CTR2);
        ctr->ctr_mutex3 = SsQsemCreateLocal(SS_SEMNUM_DBE_CTR3);
        ctr->ctr_mutex4 = SsQsemCreateLocal(SS_SEMNUM_DBE_CTR4);
        ctr->ctr_mutex5 = SsQsemCreateLocal(SS_SEMNUM_DBE_CTR5);
        ctr->ctr_mutex6 = SsQsemCreateLocal(SS_SEMNUM_DBE_CTR6);
        ctr->ctr_mutex7 = SsQsemCreateLocal(SS_SEMNUM_DBE_CTR7);

        return(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_done
 *
 *
 *
 * Parameters :
 *
 *      ctr - in, take
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_done(dbe_counter_t* ctr)
{
        SsQsemFree(ctr->ctr_mutex7);
        SsQsemFree(ctr->ctr_mutex6);
        SsQsemFree(ctr->ctr_mutex5);
        SsQsemFree(ctr->ctr_mutex4);
        SsQsemFree(ctr->ctr_mutex3);
        SsQsemFree(ctr->ctr_mutex2);
        SsQsemFree(ctr->ctr_mutex1);
        SsQsemFree(ctr->ctr_mutex0);
        SsMemFree(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_convert_init
 *
 *
 *
 * Parameters :
 *
 *      ctr -
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
void dbe_counter_convert_init(
        dbe_counter_t* ctr,
        bool *used_attrid,
        bool *used_keyid)
{
        ctr->ctr_convert = FALSE;
        ctr->ctr_attrid_used = used_attrid;
        ctr->ctr_attrid_idx = DBE_SYS_SQL_ID_START;
        ctr->ctr_keyid_used = used_keyid;
        ctr->ctr_keyid_idx = DBE_SYS_SQL_ID_START;
}

/*##**********************************************************************\
 *
 *              dbe_counter_convert_set
 *
 *
 *
 * Parameters :
 *
 *      ctr -
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
void dbe_counter_convert_set(
        dbe_counter_t* ctr,
        bool b)
{
        ctr->ctr_convert = b;
}

/*##**********************************************************************\
 *
 *              dbe_counter_convert_done
 *
 *
 *
 * Parameters :
 *
 *      ctr -
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
void dbe_counter_convert_done(
        dbe_counter_t* ctr)
{
        ctr->ctr_convert = FALSE;
        SsMemFree(ctr->ctr_attrid_used);
        ctr->ctr_attrid_used = NULL;
        SsMemFree(ctr->ctr_keyid_used);
        ctr->ctr_keyid_used = NULL;
#ifdef SS_SYNC
        rs_tuplenum_inc(&ctr->ctr_synctupleversion);
#endif
}

/*##**********************************************************************\
 *
 *              dbe_counter_newdbinit
 *
 *
 *
 * Parameters :
 *
 *      ctr -
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
void dbe_counter_newdbinit(dbe_counter_t* ctr)
{
        ctr->ctr_attrid = DBE_USER_ID_START;
        ctr->ctr_keyid = DBE_USER_ID_START;
#ifdef SS_SYNC
        rs_tuplenum_inc(&ctr->ctr_synctupleversion);
#endif
}

/*##**********************************************************************\
 *
 *              dbe_counter_getinfofromstartrec
 *
 * Gets info from start record which may be either from file header
 * or from checkpoint record.
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object.
 *
 *      sr - in, use
 *              start record
 *
 * Return value :
 *      TRUE
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_getinfofromstartrec(
        dbe_counter_t* ctr,
        dbe_startrec_t* sr)
{
        CHK_CTR(ctr);
        CTR_SEM_ENTER_ALL(ctr);
        ctr->ctr_cpnum              = sr->sr_cpnum;
        ctr->ctr_logfnum            = sr->sr_logfnum;
        trxctr_init(&ctr->ctr_maxtrxnum, sr->sr_maxtrxnum, sr->sr_maxtrxnum_res);
        trxctr_init(&ctr->ctr_committrxnum, sr->sr_committrxnum, sr->sr_committrxnum_res);
        trxctr_init(&ctr->ctr_mergetrxnum, sr->sr_mergetrxnum, sr->sr_mergetrxnum_res);
        trxctr_init(&ctr->ctr_storagetrxnum, sr->sr_storagetrxnum, sr->sr_storagetrxnum_res);
        trxctr_init(&ctr->ctr_trxid, sr->sr_trxid, sr->sr_trxid_res);
        ctr->ctr_tuplenum           = sr->sr_tuplenum;
        ctr->ctr_attrid             = sr->sr_attrid;
        ctr->ctr_keyid              = sr->sr_keyid;
        ctr->ctr_userid             = sr->sr_userid;
        ctr->ctr_blobg2id           = sr->sr_blobg2id;
        ctr->ctr_mergectr           = sr->sr_mergectr;
        ctr->ctr_tupleversion       = sr->sr_tupleversion;
#ifdef SS_SYNC
        ctr->ctr_syncmsgid          = sr->sr_syncmsgid;
        ctr->ctr_synctupleversion   = sr->sr_synctupleversion;
#endif
        SS_PMON_SET(SS_PMON_MERGELEVEL, trxctr_getlow4bytes(&ctr->ctr_mergetrxnum));
        SS_PMON_SET(SS_PMON_TRANSREADLEVEL, trxctr_getlow4bytes(&ctr->ctr_maxtrxnum));
        CTR_SEM_EXIT_ALL(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_putinfotostartrec
 *
 * Writes certain info to startrecord which may be located either
 * in db header record or in checkpoint record.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 *      sr - out, use
 *              start record
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_putinfotostartrec(
        dbe_counter_t* ctr,
        dbe_startrec_t* sr)
{
        CHK_CTR(ctr);
        CTR_SEM_ENTER_ALL(ctr);
        sr->sr_cpnum            = ctr->ctr_cpnum;
        sr->sr_logfnum          = ctr->ctr_logfnum;
        sr->sr_maxtrxnum        = trxctr_getlow4bytes(&ctr->ctr_maxtrxnum);
        sr->sr_maxtrxnum_res    = trxctr_gethigh4bytes(&ctr->ctr_maxtrxnum);;
        sr->sr_committrxnum     = trxctr_getlow4bytes(&ctr->ctr_committrxnum);
        sr->sr_committrxnum_res = trxctr_gethigh4bytes(&ctr->ctr_committrxnum);
        sr->sr_mergetrxnum      = trxctr_getlow4bytes(&ctr->ctr_mergetrxnum);
        sr->sr_mergetrxnum_res  = trxctr_gethigh4bytes(&ctr->ctr_mergetrxnum);
        sr->sr_storagetrxnum    = trxctr_getlow4bytes(&ctr->ctr_storagetrxnum);
        sr->sr_storagetrxnum_res= trxctr_gethigh4bytes(&ctr->ctr_storagetrxnum);
        sr->sr_trxid            = trxctr_getlow4bytes(&ctr->ctr_trxid);
        sr->sr_trxid_res        = trxctr_gethigh4bytes(&ctr->ctr_trxid);
        sr->sr_tuplenum         = ctr->ctr_tuplenum;
        sr->sr_attrid           = ctr->ctr_attrid;
        sr->sr_keyid            = ctr->ctr_keyid;
        sr->sr_userid           = ctr->ctr_userid;
        sr->sr_blobg2id         = ctr->ctr_blobg2id;
        sr->sr_mergectr         = ctr->ctr_mergectr;
        sr->sr_tupleversion     = ctr->ctr_tupleversion;
#ifdef SS_SYNC
        sr->sr_syncmsgid        = ctr->ctr_syncmsgid;
        sr->sr_synctupleversion = ctr->ctr_synctupleversion;
#endif
        CTR_SEM_EXIT_ALL(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewtrxid
 *
 * Returns a new unique transaction id.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      New unique transaction id.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_trxid_t dbe_counter_getnewtrxid(dbe_counter_t* ctr)
{
        dbe_trxid_t trxid;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_TRXID(ctr);
        FAKE_CODE(counter_fakereset(ctr));

        FAKE_CODE_BLOCK(
            FAKE_DBE_COUNTERWRAP_BIGINC,
            { trxctr_inc(&ctr->ctr_trxid, COUNTER_BIGINC); });

        trxid = DBE_TRXID_INIT(trxctr_inc(&ctr->ctr_trxid, 1));
        ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
        ss_dprintf_2(("dbe_counter_getnewtrxid:trxid = %lu\n", DBE_TRXID_GETLONG(trxid)));
        CTR_SEM_EXIT_TRXID(ctr);

        return(trxid);
}

ss_int8_t dbe_counter_getnewint8trxid(dbe_counter_t* ctr)
{
        ss_int8_t int8trxid;
        ss_debug(ss_int8_t int8before;)

        CHK_CTR(ctr);
        FAKE_CODE(counter_fakereset(ctr));
        CTR_SEM_ENTER_TRXID(ctr);

        ss_debug(SsInt8Set2Uint4s(&int8before, ctr->ctr_trxid.tc_hi, ctr->ctr_trxid.tc_lo);)

        trxctr_inc(&ctr->ctr_trxid, 1);

        SsInt8Set2Uint4s(&int8trxid, ctr->ctr_trxid.tc_hi, ctr->ctr_trxid.tc_lo);

        ss_dassert(SsInt8Cmp(int8before, int8trxid) < 0);

        CTR_SEM_EXIT_TRXID(ctr);

        return(int8trxid);
}

void dbe_counter_setint8trxid(
        dbe_counter_t* ctr,
        ss_int8_t int8trxid)
{
        int cmp;
        ss_int8_t c_int8trxid;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_TRXID(ctr);

        ss_dassert(ctr->ctr_trxid.tc_hi == SsInt8GetMostSignificantUint4(int8trxid) ||
                   ctr->ctr_trxid.tc_hi + 1 == SsInt8GetMostSignificantUint4(int8trxid) ||
                   ctr->ctr_trxid.tc_hi == SsInt8GetMostSignificantUint4(int8trxid) + 1);

        SsInt8Set2Uint4s(&c_int8trxid, ctr->ctr_trxid.tc_hi, ctr->ctr_trxid.tc_lo);

        cmp = SsInt8Cmp(c_int8trxid, int8trxid);
        if (cmp < 0) {
            ss_debug(ss_int8_t int8after;)
            ss_debug(dbe_trxid_t trxidbefore;)
            ss_debug(dbe_trxid_t trxidafter;)

            ss_debug(trxidbefore = DBE_TRXID_INIT(trxctr_getlow4bytes(&ctr->ctr_trxid));)

            ctr->ctr_trxid.tc_hi = SsInt8GetMostSignificantUint4(int8trxid);
            ctr->ctr_trxid.tc_lo = SsInt8GetLeastSignificantUint4(int8trxid);

            ss_debug(SsInt8Set2Uint4s(&int8after, ctr->ctr_trxid.tc_hi, ctr->ctr_trxid.tc_lo);)
            ss_dassert(SsInt8Cmp(c_int8trxid, int8after) < 0);

            ss_debug(trxidafter = DBE_TRXID_INIT(trxctr_getlow4bytes(&ctr->ctr_trxid));)
            ss_dassert(DBE_TRXID_CMP_EX(trxidbefore, trxidafter) < 0);
        }

        CTR_SEM_EXIT_TRXID(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_settrxid
 *
 * Sets new value to trx id counter
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object
 *
 *      trxid - in
 *              new trx id counter value
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_counter_settrxid(dbe_counter_t* ctr, dbe_trxid_t trxid)
{
        dbe_trxid_t c_trxid;

        CHK_CTR(ctr);
        FAKE_CODE(counter_fakereset(ctr));
        CTR_SEM_ENTER_TRXID(ctr);
        c_trxid = DBE_TRXID_INIT(trxctr_getlow4bytes(&ctr->ctr_trxid));
        if (DBE_TRXID_CMP_EX(trxid, c_trxid) > 0) {
            trxctr_setlow4bytes(&ctr->ctr_trxid, (ss_uint4_t)DBE_TRXID_GETLONG(trxid));
        }
        CTR_SEM_EXIT_TRXID(ctr);
}

void dbe_counter_get8bytetrxid(dbe_counter_t* ctr,
                               dbe_trxid_t trxid,
                               ss_uint4_t* p_low4bytes,
                               ss_uint4_t* p_high4bytes)
{
        ss_uint4_t low4bytes = (ss_uint4_t)DBE_TRXID_GETLONG(trxid);
        CHK_CTR(ctr);
        ss_dassert(p_low4bytes != NULL);
        ss_dassert(p_high4bytes != NULL);
        *p_low4bytes = low4bytes;
        *p_high4bytes = trxctr_calculatehigh4bytes(&ctr->ctr_trxid, low4bytes);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewcommittrxnum
 *
 * Returns a new unique commit transaction number. The commit transaction
 * number is the transaction serialization number.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      New unique commit transaction number.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_trxnum_t dbe_counter_getnewcommittrxnum(dbe_counter_t* ctr)
{
        dbe_trxnum_t committrxnum;

        CHK_CTR(ctr);
        if (dbe_cfg_mergecleanup) {
            /* Get also committrxnum from trxid counter. */
            CTR_SEM_ENTER_TRXID(ctr);
            FAKE_CODE(counter_fakereset(ctr));

            FAKE_CODE_BLOCK(
                FAKE_DBE_COUNTERWRAP_BIGINC,
                { trxctr_inc(&ctr->ctr_trxid, COUNTER_BIGINC); });

            committrxnum = DBE_TRXNUM_INIT(trxctr_inc(&ctr->ctr_trxid, 1));
            ss_dassert(!DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL));
            ss_dprintf_2(("dbe_counter_getnewcommittrxnum:committrxnum = %lu\n", DBE_TRXNUM_GETLONG(committrxnum)));
            ctr->ctr_committrxnum = ctr->ctr_trxid;
            CTR_SEM_EXIT_TRXID(ctr);
        } else {
            CTR_SEM_ENTER_COMMITTRXNUM(ctr);

            FAKE_CODE(counter_fakereset(ctr));

            FAKE_CODE_BLOCK(
                FAKE_DBE_COUNTERWRAP_BIGINC,
                { trxctr_inc(&ctr->ctr_committrxnum, COUNTER_BIGINC); });

            /* This must be pre-increment */
            committrxnum = DBE_TRXNUM_INIT(trxctr_inc(&ctr->ctr_committrxnum, 1));

            ss_dprintf_2(("dbe_counter_getnewcommittrxnum:committrxnum = %lu\n", DBE_TRXNUM_GETLONG(committrxnum)));

            CTR_SEM_EXIT_COMMITTRXNUM(ctr);
        }

        return(committrxnum);
}

/*##**********************************************************************\
 *
 *              dbe_counter_setmaxtrxnum
 *
 * Sets the maximum committed transaction number.
 *
 * The maximum transaction number is a transaction visibility number
 * below which all transactions are either committed or aborted.
 * It is used as a read level for new transactions.
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object.
 *
 *      maxtrxnum - in
 *              New maximum committed tramsaction number.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setmaxtrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t maxtrxnum)
{
        dbe_trxnum_t c_trxnum;
        int cmp;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_MAXTRXNUM(ctr);
        FAKE_CODE(counter_fakereset(ctr));
        ss_dprintf_1(("dbe_counter_setmaxtrxnum:maxtrxnum=%lu\n",
                          (ulong)DBE_TRXNUM_GETLONG(maxtrxnum)));
        c_trxnum = DBE_TRXNUM_INIT(trxctr_getlow4bytes(&ctr->ctr_maxtrxnum));
        cmp = DBE_TRXNUM_CMP_EX(maxtrxnum, c_trxnum);
        if (cmp > 0) {
            trxctr_setlow4bytes(&ctr->ctr_maxtrxnum,
                                (ss_uint4_t)DBE_TRXNUM_GETLONG(maxtrxnum));
            SS_PMON_SET(SS_PMON_TRANSREADLEVEL, trxctr_getlow4bytes(&ctr->ctr_maxtrxnum));
        }
        CTR_SEM_EXIT_MAXTRXNUM(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_setmergetrxnum
 *
 * Sets the merge transaction number.
 *
 * The merge transaction number is a transaction number below which
 * no versions of key values are needed. That means that all transactions
 * have a read level that is higher than the merge transaction number.
 * The is used as the index merge level.
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object.
 *
 *      mergetrxnum - in
 *              new merge transaction number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setmergetrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t mergetrxnum)
{
        dbe_trxnum_t c_trxnum;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_MERGETRXNUM(ctr);
        FAKE_CODE(counter_fakereset(ctr));
        c_trxnum =  DBE_TRXNUM_INIT(trxctr_getlow4bytes(&ctr->ctr_mergetrxnum));
        if (DBE_TRXNUM_CMP_EX(mergetrxnum, c_trxnum) > 0)
        {
            trxctr_setlow4bytes(&ctr->ctr_mergetrxnum,
                                (ss_uint4_t)DBE_TRXNUM_GETLONG(mergetrxnum));
            SS_PMON_SET(SS_PMON_MERGELEVEL, trxctr_getlow4bytes(&ctr->ctr_mergetrxnum));
            ss_dprintf_1(("dbe_counter_setmergetrxnum:new mergetrxnum=%ld\n", trxctr_getlow4bytes(&ctr->ctr_mergetrxnum)));
        }
        CTR_SEM_EXIT_MERGETRXNUM(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_setactivemergetrxnum
 *
 * Sets the current active merge transaction number.
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object.
 *
 *      active - in
 *              current merge transaction number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setactivemergetrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t activemergetrxnum)
{
        dbe_trxnum_t c_trxnum;

        CHK_CTR(ctr);

        CTR_SEM_ENTER_MERGETRXNUM(ctr);

        ctr->ctr_activemergetrxnum = activemergetrxnum;

        CTR_SEM_EXIT_MERGETRXNUM(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getactivemergetrxnum
 *
 * Returns current merge level and active merge level.
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object.
 *
 *      p_mergetrxnum - out
 *              current merge transaction number
 *
 *      p_activemergetrxnum - out
 *              active merge transaction number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_getactivemergetrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t* p_mergetrxnum,
        dbe_trxnum_t* p_activemergetrxnum)
{
        dbe_trxnum_t c_trxnum;

        CHK_CTR(ctr);
        
        CTR_SEM_ENTER_MERGETRXNUM(ctr);

        *p_mergetrxnum = DBE_TRXNUM_INIT(trxctr_getlow4bytes(&ctr->ctr_mergetrxnum));
        *p_activemergetrxnum = ctr->ctr_activemergetrxnum;

        ss_dprintf_1(("dbe_counter_getactivemergetrxnum:current mergetrxnum=%ld, active mergetrxnum=%ld\n", DBE_TRXNUM_GETLONG(*p_mergetrxnum), DBE_TRXNUM_GETLONG(*p_activemergetrxnum)));

        CTR_SEM_EXIT_MERGETRXNUM(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_setstoragetrxnum
 *
 * Sets the storage transaction number.
 *
 * The storage transaction number is a transaction number below which
 * all versions has been moved to storage tree.
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object.
 *
 *      storagetrxnum - in
 *              new storage transaction number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setstoragetrxnum(
        dbe_counter_t* ctr,
        dbe_trxnum_t storagetrxnum)
{
        dbe_trxnum_t c_trxnum;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_STORAGETRXNUM(ctr);
        FAKE_CODE(counter_fakereset(ctr));
        c_trxnum =  DBE_TRXNUM_INIT(trxctr_getlow4bytes(&ctr->ctr_storagetrxnum));
        if (DBE_TRXNUM_CMP_EX(storagetrxnum, c_trxnum) > 0)
        {
            trxctr_setlow4bytes(&ctr->ctr_storagetrxnum,
                                (ss_uint4_t)DBE_TRXNUM_GETLONG(storagetrxnum));
        }
        CTR_SEM_EXIT_STORAGETRXNUM(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewtuplenum
 *
 * Returns a new unique tuple number.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      New unique tuple number.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tuplenum_t dbe_counter_getnewtuplenum(dbe_counter_t* ctr)
{
        rs_tuplenum_t tuplenum;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_TUPLENUM(ctr);
        rs_tuplenum_inc(&ctr->ctr_tuplenum);
        tuplenum = ctr->ctr_tuplenum;
        ss_dprintf_2(("dbe_counter_getnewtuplenum:tuplenum = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tuplenum)));
        CTR_SEM_EXIT_TUPLENUM(ctr);

        return(tuplenum);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getcurtuplenum
 *
 * Returns current tuple number. Used for debugging.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      Current tuple number.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tuplenum_t dbe_counter_getcurtuplenum(dbe_counter_t* ctr)
{
        rs_tuplenum_t tuplenum;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_TUPLENUM(ctr);
        tuplenum = ctr->ctr_tuplenum;
        ss_dprintf_2(("dbe_counter_getcurtuplenum:tuplenum = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tuplenum)));
        CTR_SEM_EXIT_TUPLENUM(ctr);

        return(tuplenum);
}

/*##**********************************************************************\
 *
 *      dbe_counter_getnewint8tuplenum
 *
 * Returns a new unique tuple number as int8
 *
 * Parameters:
 *      ctr - in out, use
 *          counter object
 *
 * Return value:
 *      new tuple id value
 *
 * Limitations:
 *
 * Globals used:
 */
ss_int8_t dbe_counter_getnewint8tuplenum(dbe_counter_t* ctr)
{
        ss_int8_t i8;

        CHK_CTR(ctr);

        CTR_SEM_ENTER_TUPLENUM(ctr);
        rs_tuplenum_inc(&ctr->ctr_tuplenum);
        i8 = rs_tuplenum_getint8(&ctr->ctr_tuplenum);
        ss_dprintf_2(("dbe_counter_getnewint8tuplenum:tuplenum = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tuplenum)));
        CTR_SEM_EXIT_TUPLENUM(ctr);
        return (i8);
}

/*##**********************************************************************\
 *
 *      dbe_counter_getcurint8tuplenum
 *
 * Returns current tuple number as int8. Used for debugging.
 *
 * Parameters:
 *      ctr - in out, use
 *          counter object
 *
 * Return value:
 *      current tuple id value
 *
 * Limitations:
 *
 * Globals used:
 */
ss_int8_t dbe_counter_getcurint8tuplenum(dbe_counter_t* ctr)
{
        ss_int8_t i8;

        CHK_CTR(ctr);

        CTR_SEM_ENTER_TUPLENUM(ctr);
        i8 = rs_tuplenum_getint8(&ctr->ctr_tuplenum);
        ss_dprintf_2(("dbe_counter_getcurint8tuplenum:tuplenum = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tuplenum)));
        CTR_SEM_EXIT_TUPLENUM(ctr);
        return (i8);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewrelid
 *
 * Returns a new unique relation identifier.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      New unique relation identifier.
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_counter_getnewrelid(dbe_counter_t* ctr)
{
        ulong key_id = 0;

        CHK_CTR(ctr);

        CTR_SEM_ENTER_ID(ctr);
        if (ctr->ctr_convert) {
            while (ctr->ctr_keyid_idx < DBE_USER_ID_START) {
                if(!ctr->ctr_keyid_used[ctr->ctr_keyid_idx]) {
                    ctr->ctr_keyid_used[ctr->ctr_keyid_idx] = TRUE;
                    ss_dprintf_4(("counter keyid used %d\n", ctr->ctr_keyid_idx));
                    key_id = ctr->ctr_keyid_idx;
                    break;
                }
                ctr->ctr_keyid_idx++;
            }
            ss_dassert(ctr->ctr_keyid_idx < DBE_USER_ID_START);
        } else {
            key_id = ctr->ctr_keyid++;
        }
        ss_dprintf_2(("dbe_counter_getnewrelid:relid = %ld\n", key_id));
        CTR_SEM_EXIT_ID(ctr);

        return(key_id);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getcurrelid
 *
 * Returns current relation identifier.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      Current relation identifier.
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_counter_getcurrelid(dbe_counter_t* ctr)
{
        ulong key_id;

        CHK_CTR(ctr);

        CTR_SEM_ENTER_ID(ctr);
        key_id = ctr->ctr_keyid;
        CTR_SEM_EXIT_ID(ctr);

        return(key_id);
}

/*##**********************************************************************\
 *
 *              dbe_counter_setnewrelid
 *
 * Sets a new relation identifier if given value larger than current value.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 *      relid - in
 *              New relid value.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setnewrelid(dbe_counter_t* ctr, ulong relid)
{
        CHK_CTR(ctr);

        CTR_SEM_ENTER_ID(ctr);
        if (relid > ctr->ctr_keyid) {
            ss_dprintf_2(("dbe_counter_setnewrelid:relid = %ld\n", relid));
            ctr->ctr_keyid = relid;
        }
        CTR_SEM_EXIT_ID(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewattrid
 *
 * Returns a new unique attribute identifier.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      New unique attribute identifier.
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_counter_getnewattrid(dbe_counter_t* ctr)
{
        ulong attr_id = 0;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_ID(ctr);
        if (ctr->ctr_convert) {
            while (ctr->ctr_attrid_idx < DBE_USER_ID_START) {
                if(!ctr->ctr_attrid_used[ctr->ctr_attrid_idx]) {
                    ss_dprintf_4(("counter attrid used %d\n", ctr->ctr_attrid_idx));
                    ctr->ctr_attrid_used[ctr->ctr_attrid_idx] = TRUE;
                    attr_id = ctr->ctr_attrid_idx;
                    break;
                }
                ctr->ctr_attrid_idx++;
            }
            ss_assert(ctr->ctr_attrid_idx < DBE_USER_ID_START);
        } else {
            attr_id = ctr->ctr_attrid++;
        }
        CTR_SEM_EXIT_ID(ctr);

        return(attr_id);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewkeyid
 *
 * Returns a new unique key id.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      New unique key identifier.
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_counter_getnewkeyid(dbe_counter_t* ctr)
{
        ulong key_id = 0;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_ID(ctr);
        if (ctr->ctr_convert) {
            while (ctr->ctr_keyid_idx < DBE_USER_ID_START) {
                if(!ctr->ctr_keyid_used[ctr->ctr_keyid_idx]) {
                    ss_dprintf_4(("used keyid %d\n", ctr->ctr_keyid_idx));
                    ctr->ctr_keyid_used[ctr->ctr_keyid_idx] = TRUE;
                    key_id = ctr->ctr_keyid_idx;
                    break;
                }
                ctr->ctr_keyid_idx++;
            }
            ss_dassert(ctr->ctr_keyid_idx < DBE_USER_ID_START);
        } else {
            key_id = ctr->ctr_keyid++;
        }
        ss_dprintf_2(("dbe_counter_getnewkeyid:relid = %ld\n", key_id));
        CTR_SEM_EXIT_ID(ctr);

        return(key_id);
}

/*##**********************************************************************\
 *
 *              dbe_counter_setnewkeyid
 *
 * Sets a new key identifier if given value larger than current value.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 *      keyid - in
 *              New keyid value.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setnewkeyid(dbe_counter_t* ctr, ulong keyid)
{
        CHK_CTR(ctr);

        dbe_counter_setnewrelid(ctr, keyid);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewuserid
 *
 * Returns a new unique user id.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      New unique user identifier.
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_counter_getnewuserid(dbe_counter_t* ctr)
{
        ulong user_id;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_ID(ctr);
        user_id = ctr->ctr_userid++;
        CTR_SEM_EXIT_ID(ctr);

        return(user_id);
}

/*##**********************************************************************\
 *
 *              dbe_counter_setcpnum
 *
 * Sets cp # counter value
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object
 *
 *      cpnum - in
 *          checkpoint counter value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setcpnum(dbe_counter_t* ctr, dbe_cpnum_t cpnum)
{
        CHK_CTR(ctr);
        CTR_SEM_ENTER_CPNUM(ctr);
        ctr->ctr_cpnum = cpnum;
        CTR_SEM_EXIT_CPNUM(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_inccpnum
 *
 * Increments the cpnum counter and returns the incremented value
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object
 *
 * Return value :
 *      Incremented cp counter value
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_cpnum_t dbe_counter_inccpnum(dbe_counter_t* ctr)
{
        dbe_cpnum_t cpnum;
        CHK_CTR(ctr);
        CTR_SEM_ENTER_CPNUM(ctr);
        cpnum = ++ctr->ctr_cpnum;
        CTR_SEM_EXIT_CPNUM(ctr);
        return (cpnum);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getlogfnum
 *
 * Gets log file number counter
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object
 *
 * Return value :
 *      log file number counter value
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_logfnum_t dbe_counter_getlogfnum(dbe_counter_t* ctr)
{
        dbe_logfnum_t logfnum;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_LOGFNUM(ctr);
        logfnum = ctr->ctr_logfnum;
        CTR_SEM_EXIT_LOGFNUM(ctr);

        return (logfnum);
}

/*##**********************************************************************\
 *
 *              dbe_counter_inclogfnum
 *
 * Increments and then returns the log file number counter
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object
 *
 * Return value :
 *      incremented log file number counter
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_logfnum_t dbe_counter_inclogfnum(dbe_counter_t* ctr)
{
        dbe_logfnum_t logfnum;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_LOGFNUM(ctr);
        logfnum = ++ctr->ctr_logfnum;
        CTR_SEM_EXIT_LOGFNUM(ctr);

        return (logfnum);

}

/*##**********************************************************************\
 *
 *              dbe_counter_setlogfnum
 *
 * Sets log file counter value
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object
 *
 *      logfnum - in
 *              log file number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setlogfnum(dbe_counter_t* ctr, dbe_logfnum_t logfnum)
{
        CHK_CTR(ctr);
        CTR_SEM_ENTER_LOGFNUM(ctr);
        ctr->ctr_logfnum = logfnum;
        CTR_SEM_EXIT_LOGFNUM(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewblobid
 *
 * Gives a new unique BLOB ID
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_blobid_t dbe_counter_getnewblobid(dbe_counter_t* ctr)
{
        dbe_blobid_t blob_id;
        ss_uint2_t increment = 1;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_BLOB(ctr);
        blob_id = DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(ctr->ctr_blobg2id);
        if (blob_id == 0) {
            blob_id = 1;
            increment = 2;
        }
        DBE_BLOBG2ID_ADDASSIGN_UINT2(&(ctr->ctr_blobg2id), increment);
        CTR_SEM_EXIT_BLOB(ctr);
        return (blob_id);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewblobg2id
 *
 * Gives a new unique BLOB ID (64 bit)
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object
 *
 * Return value :
 *      New 64 bit BLOB id
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_blobg2id_t dbe_counter_getnewblobg2id(dbe_counter_t* ctr)
{
        dbe_blobg2id_t blobg2_id;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_BLOB(ctr);
        blobg2_id = ctr->ctr_blobg2id;
        DBE_BLOBG2ID_ADDASSIGN_UINT2(&(ctr->ctr_blobg2id), (ss_uint2_t)1);
        CTR_SEM_EXIT_BLOB(ctr);
        return (blobg2_id);
}

/*##**********************************************************************\
 *
 *      dbe_counter_setblobg2id
 *
 * updates counter for BLOB G2 ID.
 * if the value in counter is bigger or equal to the parameter, nothing is
 * done
 *
 * Parameters:
 *      ctr - in out, use
 *          counter object
 *
 *      bid - in
 *          blob g2 id
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_counter_setblobg2id(dbe_counter_t* ctr, dbe_blobg2id_t bid)
{
        CHK_CTR(ctr);
        CTR_SEM_ENTER_BLOB(ctr);
        if (DBE_BLOBG2ID_CMP(ctr->ctr_blobg2id, bid) < 0) {
            ctr->ctr_blobg2id = bid;
        }
        CTR_SEM_EXIT_BLOB(ctr);
}
/*##**********************************************************************\
 *
 *              dbe_counter_getmergectr
 *
 *
 *
 * Parameters :
 *
 *      ctr -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_counter_getmergectr(dbe_counter_t* ctr)
{
        CHK_CTR(ctr);

        return(ctr->ctr_mergectr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_setmergectr
 *
 *
 *
 * Parameters :
 *
 *      ctr -
 *
 *
 *      mergectr -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_counter_setmergectr(dbe_counter_t* ctr, ulong mergectr)
{
        CHK_CTR(ctr);

        ctr->ctr_mergectr = mergectr;
}

/*##**********************************************************************\
 *
 *              dbe_counter_getnewtupleversion
 *
 * Returns a new unique tuple version number.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      New unique tuple version number.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tuplenum_t dbe_counter_getnewtupleversion(dbe_counter_t* ctr)
{
        rs_tuplenum_t tupleversion;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_TUPLEVERSION(ctr);
        rs_tuplenum_inc(&ctr->ctr_tupleversion);
        tupleversion = ctr->ctr_tupleversion;
        ss_dprintf_2(("dbe_counter_getnewtupleversion:tupleversion = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tupleversion)));
        CTR_SEM_EXIT_TUPLEVERSION(ctr);

        return(tupleversion);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getcurtupleversion
 *
 * Returns current tuple version number. Used for debugging.
 *
 * Parameters :
 *
 *      ctr - in, use
 *              Counter object.
 *
 * Return value :
 *
 *      Current tuple version number.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tuplenum_t dbe_counter_getcurtupleversion(dbe_counter_t* ctr)
{
        rs_tuplenum_t tupleversion;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_TUPLEVERSION(ctr);
        tupleversion = ctr->ctr_tupleversion;
        CTR_SEM_EXIT_TUPLEVERSION(ctr);

        return(tupleversion);
}

/*##**********************************************************************\
 *
 *      dbe_counter_getnewint8tupleversion
 *
 * Returns a new unique tuple version number as int8.
 *
 * Parameters:
 *      ctr - in out, use
 *          Counter object
 *
 * Return value:
 *      new tuple version value
 *
 * Limitations:
 *
 * Globals used:
 */
ss_int8_t dbe_counter_getnewint8tupleversion(dbe_counter_t* ctr)
{
        ss_int8_t i8;

        CHK_CTR(ctr);

        CTR_SEM_ENTER_TUPLEVERSION(ctr);
        rs_tuplenum_inc(&ctr->ctr_tupleversion);
        i8 = rs_tuplenum_getint8(&ctr->ctr_tupleversion);
        ss_dprintf_2(("dbe_counter_getnewint8tupleversion:tupleversion = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tupleversion)));
        CTR_SEM_EXIT_TUPLEVERSION(ctr);
        return (i8);
}

/*##**********************************************************************\
 *
 *      dbe_counter_getcurint8tupleversion
 *
 * Returns current tuple version number as int8. Used for debugging.
 *
 * Parameters:
 *      ctr - in out, use
 *          Counter object
 *
 * Return value:
 *      current tuple version value
 *
 * Limitations:
 *
 * Globals used:
 */
ss_int8_t dbe_counter_getcurint8tupleversion(dbe_counter_t* ctr)
{
        ss_int8_t i8;

        CHK_CTR(ctr);

        CTR_SEM_ENTER_TUPLEVERSION(ctr);
        i8 = rs_tuplenum_getint8(&ctr->ctr_tupleversion);
        CTR_SEM_EXIT_TUPLEVERSION(ctr);
        return (i8);
}

/*##**********************************************************************\
 *
 *              dbe_counter_settupleversion
 *
 * Sets tuple version counter to new value
 *
 * Parameters :
 *
 *      ctr - in out, use
 *              Counter object.
 *
 *      tupleversion - in
 *              new tuple version counter value
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_counter_settupleversion(
        dbe_counter_t* ctr,
        rs_tuplenum_t tupleversion)
{
        CHK_CTR(ctr);
        CTR_SEM_ENTER_TUPLEVERSION(ctr);
        ctr->ctr_tupleversion = tupleversion;
        ss_dprintf_2(("dbe_counter_settupleversion:tupleversion = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tupleversion)));
        CTR_SEM_EXIT_TUPLEVERSION(ctr);
}

#ifdef SS_SYNC

ulong dbe_counter_getnewsyncmsgid(dbe_counter_t* ctr)
{
        ulong syncmsgid;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_FLOW(ctr);
        syncmsgid = ++ctr->ctr_syncmsgid;
        CTR_SEM_EXIT_FLOW(ctr);

        return(syncmsgid);
}

rs_tuplenum_t dbe_counter_getsynctupleversion(dbe_counter_t* ctr)
{
        rs_tuplenum_t tupleversion;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_FLOW(ctr);
        tupleversion = ctr->ctr_synctupleversion;
        CTR_SEM_EXIT_FLOW(ctr);

        return(tupleversion);
}

rs_tuplenum_t dbe_counter_getnewsynctupleversion(dbe_counter_t* ctr)
{
        rs_tuplenum_t tupleversion;

        CHK_CTR(ctr);

        CTR_SEM_ENTER_FLOW(ctr);
        
        rs_tuplenum_inc(&ctr->ctr_synctupleversion);
        tupleversion = ctr->ctr_synctupleversion;

        CTR_SEM_EXIT_FLOW(ctr);

        return(tupleversion);
}

#endif /* SS_SYNC */

/*##**********************************************************************\
 *
 *              dbe_counter_incctrbyid
 *
 * increments a counter identified by counter id. This is only needed
 * in roll-forward recovery and possibly in hot-standby server
 *
 * Parameters :
 *
 *      ctr - use
 *              counter object
 *
 *      ctrid - in
 *              counter id
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_counter_incctrbyid(
        dbe_counter_t* ctr,
        dbe_sysctrid_t ctrid)
{
        dbe_trxnum_t trxnum;
        ulong mergectr;

        CHK_CTR(ctr);

        switch (ctrid) {
            case DBE_CTR_TRXID:
                (void)dbe_counter_getnewtrxid(ctr);
                break;
            case DBE_CTR_MAXTRXNUM:
                /* JarmoR Aug 1, 2002. If this is called then we need to
                 * update read level rbt for split merge in dbe7gtrs.c.
                 */
                ss_derror;
                trxnum = dbe_counter_getmaxtrxnum(ctr);
                dbe_counter_setmaxtrxnum(ctr, DBE_TRXNUM_SUM(trxnum, 1));
                break;
            case DBE_CTR_COMMITTRXNUM:
                (void)dbe_counter_getnewcommittrxnum(ctr);
                break;
            case DBE_CTR_MERGETRXNUM:
                trxnum = dbe_counter_getmergetrxnum(ctr);
                dbe_counter_setmergetrxnum(ctr, DBE_TRXNUM_SUM(trxnum, 1));
                break;
            case DBE_CTR_CPNUM:
                (void)dbe_counter_inccpnum(ctr);
                break;
            case DBE_CTR_TUPLENUM:
                (void)dbe_counter_getnewtuplenum(ctr);
                break;
            case DBE_CTR_ATTRID:
                (void)dbe_counter_getnewattrid(ctr);
                break;
            case DBE_CTR_KEYID:
                (void)dbe_counter_getnewkeyid(ctr);
                break;
            case DBE_CTR_USERID:
                (void)dbe_counter_getnewuserid(ctr);
                break;
            case DBE_CTR_LOGFNUM:
                (void)dbe_counter_inclogfnum(ctr);
                break;
            case DBE_CTR_BLOBID:
                (void)dbe_counter_getnewblobid(ctr);
                break;
            case DBE_CTR_BLOBG2ID:
                (void)dbe_counter_getnewblobg2id(ctr);
                break;
            case DBE_CTR_MERGECTR:
                mergectr = dbe_counter_getmergectr(ctr);
                mergectr++;
                dbe_counter_setmergectr(ctr, mergectr);
                break;
            case DBE_CTR_TUPLEVERSION:
                (void)dbe_counter_getnewtupleversion(ctr);
                break;
#ifdef SS_SYNC
            case DBE_CTR_SYNCMSGID:
                (void)dbe_counter_getnewsyncmsgid(ctr);
                break;
            case DBE_CTR_SYNCTUPLEVERSION:
                (void)dbe_counter_getnewsynctupleversion(ctr);
                break;
#endif
            default:
                ss_error;
        }
}

#ifdef DBE_REPLICATION

/*#***********************************************************************\
 *
 *              counter_replicaupdate
 *
 * Updates those counter values that must be in sync during
 * replication.
 *
 * Parameters :
 *
 *      ctr -
 *
 *
 *      keyid -
 *
 *
 *      attrid -
 *
 *
 *      userid -
 *
 *
 *      p_tuplenum -
 *
 *
 *      p_tupleversion -
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
static bool counter_replicaupdate(
        dbe_counter_t* ctr,
        bool hsbg2,
        ulong keyid,
        ulong attrid,
        ulong userid,
        rs_tuplenum_t* p_tuplenum,
        rs_tuplenum_t* p_tupleversion,
        ulong syncmsgid,
        rs_tuplenum_t* p_synctupleversion,
        trxctr_t* p_trxid,
        dbe_blobg2id_t blobg2id)
{
        bool changes = FALSE;

        CHK_CTR(ctr);
        CTR_SEM_ENTER_ALL(ctr);

        ss_dprintf_2(("dbe_counter_replicaupdate:remote tuplenum = %ld, local tuplenum = %ld\n",
            rs_tuplenum_getlsl(p_tuplenum),
            rs_tuplenum_getlsl(&ctr->ctr_tuplenum)));
        ss_dprintf_2(("dbe_counter_replicaupdate:remote tupleversion = %ld, local tupleversion = %ld\n",
            rs_tuplenum_getlsl(p_tupleversion),
            rs_tuplenum_getlsl(&ctr->ctr_tupleversion)));
        ss_dprintf_2(("dbe_counter_replicaupdate:remote keyid = %ld, local keyid = %ld\n",
            keyid,ctr->ctr_keyid));

        if (ctr->ctr_keyid < keyid) {
            ctr->ctr_keyid = keyid;
            changes = TRUE;
        }
        if (ctr->ctr_attrid < attrid) {
            ctr->ctr_attrid = attrid;
            changes = TRUE;
        }
        if (ctr->ctr_userid < userid) {
            ctr->ctr_userid = userid;
            changes = TRUE;
        }
        if (rs_tuplenum_cmp(&ctr->ctr_tuplenum, p_tuplenum) < 0) {
            memcpy(&ctr->ctr_tuplenum, p_tuplenum, sizeof(rs_tuplenum_t));
            changes = TRUE;
        }
        if (rs_tuplenum_cmp(&ctr->ctr_tupleversion, p_tupleversion) < 0) {
            memcpy(&ctr->ctr_tupleversion, p_tupleversion, sizeof(rs_tuplenum_t));
            changes = TRUE;
        }
        if (ctr->ctr_syncmsgid < syncmsgid) {
            ctr->ctr_syncmsgid = syncmsgid;
            changes = TRUE;
        }
        if (rs_tuplenum_cmp(&ctr->ctr_synctupleversion, p_synctupleversion) < 0) {
            memcpy(&ctr->ctr_synctupleversion, p_synctupleversion, sizeof(rs_tuplenum_t));
            changes = TRUE;
        }

        if (hsbg2) {
            if (trxctr_cmp(&ctr->ctr_trxid, p_trxid) < 0) {
                ctr->ctr_trxid = *p_trxid;
                changes = TRUE;
            }
            if (SsInt8Cmp(ctr->ctr_blobg2id, blobg2id) < 0) {
                ctr->ctr_blobg2id = blobg2id;
                changes = TRUE;
            }
        }

        CTR_SEM_EXIT_ALL(ctr);

        return(changes);
}

/*#***********************************************************************\
 *
 *              counter_getreplicacounters
 *
 * Returns counter values that must be in sync during replication.
 *
 * Parameters :
 *
 *      ctr -
 *
 *
 *      p_keyid -
 *
 *
 *      p_attrid -
 *
 *
 *      p_userid -
 *
 *
 *      p_tuplenum -
 *
 *
 *      p_tupleversion -
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
static void counter_getreplicacounters(
        dbe_counter_t* ctr,
        ulong* p_keyid,
        ulong* p_attrid,
        ulong* p_userid,
        rs_tuplenum_t* p_tuplenum,
        rs_tuplenum_t* p_tupleversion,
        ulong* p_syncmsgid,
        rs_tuplenum_t* p_synctupleversion,
        trxctr_t* p_trxid,
        dbe_blobg2id_t* p_blobg2id)
{
        CHK_CTR(ctr);
        CTR_SEM_ENTER_ALL(ctr);

        ss_dprintf_2(("dbe_counter_getreplicacounters:local tuplenum = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tuplenum)));
        ss_dprintf_2(("dbe_counter_getreplicacounters:local tupleversion = %ld\n",
            rs_tuplenum_getlsl(&ctr->ctr_tupleversion)));

        *p_keyid = ctr->ctr_keyid;
        *p_attrid = ctr->ctr_attrid;
        *p_userid = ctr->ctr_userid;
        memcpy(p_tuplenum, &ctr->ctr_tuplenum, sizeof(rs_tuplenum_t));
        memcpy(p_tupleversion, &ctr->ctr_tupleversion, sizeof(rs_tuplenum_t));
        *p_syncmsgid = ctr->ctr_syncmsgid;
        memcpy(p_synctupleversion, &ctr->ctr_synctupleversion, sizeof(rs_tuplenum_t));
        *p_trxid = ctr->ctr_trxid;
        *p_blobg2id = ctr->ctr_blobg2id;

        CTR_SEM_EXIT_ALL(ctr);
}

/*##**********************************************************************\
 *
 *              dbe_counter_getreplicacounters
 *
 *
 *
 * Parameters :
 *
 *      ctr -
 *
 *
 *      p_data -
 *
 *
 *      p_size -
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
void dbe_counter_getreplicacounters(
        dbe_counter_t* ctr,
        bool hsbg2,
        char** p_data,
        int* p_size)
{
        ss_uint4_t* p;
        ss_uint4_t fb;
        ulong keyid;
        ulong attrid;
        ulong userid;
        rs_tuplenum_t tuplenum;
        rs_tuplenum_t tupleversion;
        ulong syncmsgid;
        rs_tuplenum_t synctupleversion;
        trxctr_t trxid;
        dbe_blobg2id_t blobg2id;

        counter_getreplicacounters(
            ctr,
            &keyid,
            &attrid,
            &userid,
            &tuplenum,
            &tupleversion,
            &syncmsgid,
            &synctupleversion,
            &trxid,
            &blobg2id);

        p = SsMemAlloc(DBE_HSBSYSCTR_SIZE);

        SS_UINT4_STORETODISK(&p[0], keyid);
        SS_UINT4_STORETODISK(&p[1], attrid);
        SS_UINT4_STORETODISK(&p[2], userid);

        fb = rs_tuplenum_getmsl(&tuplenum);
        SS_UINT4_STORETODISK(&p[3], fb);
        fb = rs_tuplenum_getlsl(&tuplenum);
        SS_UINT4_STORETODISK(&p[4], fb);

        fb = rs_tuplenum_getmsl(&tupleversion);
        SS_UINT4_STORETODISK(&p[5], fb);
        fb = rs_tuplenum_getlsl(&tupleversion);
        SS_UINT4_STORETODISK(&p[6], fb);

        SS_UINT4_STORETODISK(&p[7], syncmsgid);

        fb = rs_tuplenum_getmsl(&synctupleversion);
        SS_UINT4_STORETODISK(&p[8], fb);
        fb = rs_tuplenum_getlsl(&synctupleversion);
        SS_UINT4_STORETODISK(&p[9], fb);

        fb = trxctr_gethigh4bytes(&trxid);
        SS_UINT4_STORETODISK(&p[10], fb);
        fb = trxctr_getlow4bytes(&trxid);
        SS_UINT4_STORETODISK(&p[11], fb);

        fb = SsInt8GetMostSignificantUint4(blobg2id);
        SS_UINT4_STORETODISK(&p[12], fb);
        fb = SsInt8GetLeastSignificantUint4(blobg2id);
        SS_UINT4_STORETODISK(&p[13], fb);

        *p_data = (char*)p;
        if (hsbg2) {
            *p_size = DBE_HSBSYSCTR_SIZE;
        } else {
            *p_size = DBE_HSBSYSCTR_OLDSIZE;
        }
}

/*##**********************************************************************\
 *
 *              dbe_counter_setreplicacounters
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      data -
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
bool dbe_counter_setreplicacounters(
        dbe_counter_t* ctr,
        bool hsbg2,
        char* data)
{
        ss_uint4_t* p;
        ss_uint4_t fb1;
        ss_uint4_t fb2;
        ulong keyid;
        ulong attrid;
        ulong userid;
        rs_tuplenum_t tuplenum;
        rs_tuplenum_t tupleversion;
        ulong syncmsgid;
        rs_tuplenum_t synctupleversion;
        trxctr_t trxid;
        dbe_blobg2id_t blobg2id;

        p = (ss_uint4_t*)data;

        keyid = SS_UINT4_LOADFROMDISK(&p[0]);
        attrid = SS_UINT4_LOADFROMDISK(&p[1]);
        userid = SS_UINT4_LOADFROMDISK(&p[2]);

        fb1 = SS_UINT4_LOADFROMDISK(&p[3]);
        fb2 = SS_UINT4_LOADFROMDISK(&p[4]);
        rs_tuplenum_ulonginit(&tuplenum, fb1, fb2);

        fb1 = SS_UINT4_LOADFROMDISK(&p[5]);
        fb2 = SS_UINT4_LOADFROMDISK(&p[6]);
        rs_tuplenum_ulonginit(&tupleversion, fb1, fb2);

        syncmsgid = SS_UINT4_LOADFROMDISK(&p[7]);

        fb1 = SS_UINT4_LOADFROMDISK(&p[8]);
        fb2 = SS_UINT4_LOADFROMDISK(&p[9]);
        rs_tuplenum_ulonginit(&synctupleversion, fb1, fb2);

        if (hsbg2) {
            fb1 = SS_UINT4_LOADFROMDISK(&p[10]);
            fb2 = SS_UINT4_LOADFROMDISK(&p[11]);
            trxctr_init(&trxid, fb2, fb1);

            fb1 = SS_UINT4_LOADFROMDISK(&p[12]);
            fb2 = SS_UINT4_LOADFROMDISK(&p[13]);
            blobg2id = SsInt8InitFrom2Uint4s(fb1, fb2);
        } else {
            /* These are just to silence compiler, purify, etc. */
            trxctr_init(&trxid, 0, 0);
            SsInt8Set0(&blobg2id);
        }

        return(counter_replicaupdate(
                    ctr,
                    hsbg2,
                    keyid,
                    attrid,
                    userid,
                    &tuplenum,
                    &tupleversion,
                    syncmsgid,
                    &synctupleversion,
                    &trxid,
                    blobg2id));
}

#endif /* DBE_REPLICATION */


#ifndef SS_LIGHT
/*##**********************************************************************\
 *
 *              dbe_counter_printinfo
 *
 *
 *
 * Parameters :
 *
 *      fp -
 *
 *
 *      ctr -
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
void dbe_counter_printinfo(
        void* fp,
        dbe_counter_t* ctr)
{
        CHK_CTR(ctr);

        CTR_SEM_ENTER_ALL(ctr);

        SsFprintf(fp, "  Trxi     Maxtn    Comtn    Mrgtn    Strgtn   Cpn    Tpln\n");
        SsFprintf(fp, "  %-8ld %-8ld %-8ld %-8ld %-8ld %-6ld %ld\n",
            (ulong)ctr->ctr_trxid.tc_lo,
            (ulong)ctr->ctr_maxtrxnum.tc_lo,
            (ulong)ctr->ctr_committrxnum.tc_lo,
            (ulong)ctr->ctr_mergetrxnum.tc_lo,
            (ulong)ctr->ctr_storagetrxnum.tc_lo,
            (ulong)ctr->ctr_cpnum,
            (ulong)rs_tuplenum_getlsl(&ctr->ctr_tuplenum));
        SsFprintf(fp, "  Atti   Keyi   Usri  Logfn  Blobi   Mrgct  Tver\n");
        SsFprintf(fp, "  %-6ld %-6ld %-5ld %-6ld %-7ld %-6ld %ld\n",
            (ulong)ctr->ctr_attrid,
            (ulong)ctr->ctr_keyid,
            (ulong)ctr->ctr_userid,
            (ulong)ctr->ctr_logfnum,
            (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(ctr->ctr_blobg2id),
            (ulong)ctr->ctr_mergectr,
            (ulong)rs_tuplenum_getlsl(&ctr->ctr_tupleversion));

        CTR_SEM_EXIT_ALL(ctr);
}
#endif /* SS_LIGHT */
