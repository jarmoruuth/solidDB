/*************************************************************************\
**  source       * dbe7trxi.h
**  directory    * dbe
**  description  * Transaction info structure.
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


#ifndef DBE7TRXI_H
#define DBE7TRXI_H

#include <sssem.h>
#include <ssdebug.h>

#include <su0list.h>

#include <rs0sysi.h>
#include <rs0sqli.h>

#include "dbe0type.h"
#include "dbe9type.h"

#define CHK_TRXINFO(ti)     ss_dassert(SS_CHKPTR(ti) && (ti)->ti_chk == DBE_CHK_TRXINFO)

#define DBE_TRXST_BITMASK            7                  /* Bits 0-2 */
#define DBE_TRXST_HSB_UNKNOWN        SU_BFLAG_BIT(3)
#define DBE_TRXST_HSB_OPENDDOP       SU_BFLAG_BIT(4)
#define DBE_TRXST_CANREMOVEREADLEVEL SU_BFLAG_BIT(5)
#define DBE_TRXST_MTABLETRX          SU_BFLAG_BIT(6)

struct dbe_trxinfo_st {
        ss_int4_t       ti_nlinks;          /* link count */
        dbe_trxstate_t  ti_state;           /* trx state */
        dbe_trxid_t     ti_usertrxid;       /* user trx id */
        dbe_trxnum_t    ti_maxtrxnum;       /* trx read level */
        dbe_trxnum_t    ti_committrxnum;    /* commit serialization number */
        su_list_node_t* ti_actlistnode;     /* pointer to active trx list
                                               node in dbe7gtrs.c */
#ifdef SS_HSBG2
        rs_sysi_t*      ti_hsbcd;           /* HSB specific cd. */
#endif /* SS_HSBG2 */
        ss_debug(dbe_chk_t ti_chk;)
        ss_autotest_or_debug(trx_mode_t          ti_trxmode;)
        ss_autotest_or_debug(rs_sqli_isolation_t ti_isolation;)
};

typedef struct {
        int             oti_len;
        dbe_trxid_t     oti_tablemin;
        dbe_trxid_t     oti_tablemax;
        dbe_trxid_t     oti_table[1];   /* Actually variable length, len oti_len. */
} dbe_opentrxinfo_t;

dbe_trxinfo_t* dbe_trxinfo_init(
        rs_sysi_t* cd);

SS_INLINE void dbe_trxinfo_initbuf(
        rs_sysi_t*      cd,
        dbe_trxinfo_t*  ti);

void dbe_trxinfo_done_nomutex(
        dbe_trxinfo_t* ti,
        rs_sysi_t* cd);

SS_INLINE void dbe_trxinfo_donebuf_nomutex(
        dbe_trxinfo_t* ti,
        rs_sysi_t* cd);

SS_INLINE void dbe_trxinfo_done(
        dbe_trxinfo_t* ti,
        rs_sysi_t* cd,
        SsSemT* trxbuf_sem);

SS_INLINE void dbe_trxinfo_donebuf(
        dbe_trxinfo_t* ti,
        rs_sysi_t* cd,
        SsSemT* trxbuf_sem);

SS_INLINE void dbe_trxinfo_link(
        dbe_trxinfo_t* ti,
        SsSemT* trxbuf_sem);

SS_INLINE void dbe_trxinfo_removehsbcd(
        dbe_trxinfo_t* ti);

SS_INLINE ss_int4_t dbe_trxinfo_nlinks(
        dbe_trxinfo_t*  ti);

#define dbe_trxinfo_getstate(ti)        ((ti)->ti_state & DBE_TRXST_BITMASK)

#define dbe_trxinfo_setbegin(ti)        (((ti)->ti_state = DBE_TRXST_BEGIN      ) | ( (ti)->ti_state&(~DBE_TRXST_BITMASK) ))
#define dbe_trxinfo_setvalidate(ti)     (((ti)->ti_state = DBE_TRXST_VALIDATE   ) | ( (ti)->ti_state&(~DBE_TRXST_BITMASK) ))
#define dbe_trxinfo_setcommitted(ti)    (((ti)->ti_state = DBE_TRXST_COMMIT     ) | ( (ti)->ti_state&(~DBE_TRXST_BITMASK) ))
#define dbe_trxinfo_setaborted(ti)      (((ti)->ti_state = DBE_TRXST_ABORT      ) | ( (ti)->ti_state&(~DBE_TRXST_BITMASK) ))
#define dbe_trxinfo_setsaved(ti)        (((ti)->ti_state = DBE_TRXST_SAVED      ) | ( (ti)->ti_state&(~DBE_TRXST_BITMASK) ))
#define dbe_trxinfo_settobeaborted(ti)  (((ti)->ti_state = DBE_TRXST_TOBEABORTED) | ( (ti)->ti_state&(~DBE_TRXST_BITMASK) ))

#define dbe_trxinfo_isbegin(ti)         (((ti)->ti_state&DBE_TRXST_BITMASK) == DBE_TRXST_BEGIN)
#define dbe_trxinfo_isvalidate(ti)      (((ti)->ti_state&DBE_TRXST_BITMASK) == DBE_TRXST_VALIDATE)
#define dbe_trxinfo_iscommitted(ti)     (((ti)->ti_state&DBE_TRXST_BITMASK) == DBE_TRXST_COMMIT)
#define dbe_trxinfo_isaborted(ti)       (((ti)->ti_state&DBE_TRXST_BITMASK) == DBE_TRXST_ABORT)
#define dbe_trxinfo_isended(ti)         (((ti)->ti_state&DBE_TRXST_BITMASK) == DBE_TRXST_COMMIT || \
                                         ((ti)->ti_state&DBE_TRXST_BITMASK) == DBE_TRXST_ABORT)
#define dbe_trxinfo_issaved(ti)         (((ti)->ti_state&DBE_TRXST_BITMASK) == DBE_TRXST_SAVED)
#define dbe_trxinfo_istobeaborted(ti)   (((ti)->ti_state&DBE_TRXST_BITMASK) == DBE_TRXST_TOBEABORTED)

#ifdef SS_HSBG2

#define dbe_trxinfo_setunknown(ti)      ((ti)->ti_state = DBE_TRXST_HSB_UNKNOWN | (ti)->ti_state)
#define dbe_trxinfo_isunknown(ti)       ((ti)->ti_state & DBE_TRXST_HSB_UNKNOWN)
#define dbe_trxinfo_setopenddop(ti)     ((ti)->ti_state = DBE_TRXST_HSB_OPENDDOP | (ti)->ti_state)
#define dbe_trxinfo_isopenddop(ti)      ((ti)->ti_state & DBE_TRXST_HSB_OPENDDOP)

#endif /* SS_HSBG2 */

#define dbe_trxinfo_setcanremovereadlevel(ti)   ((ti)->ti_state |= DBE_TRXST_CANREMOVEREADLEVEL)
#define dbe_trxinfo_clearcanremovereadlevel(ti) ((ti)->ti_state &= ~DBE_TRXST_CANREMOVEREADLEVEL)
#define dbe_trxinfo_canremovereadlevel(ti)      ((ti)->ti_state & DBE_TRXST_CANREMOVEREADLEVEL)

#define dbe_trxinfo_setmtabletrx(ti)    ((ti)->ti_state |= DBE_TRXST_MTABLETRX)
#define dbe_trxinfo_ismtabletrx(ti)     ((ti)->ti_state & DBE_TRXST_MTABLETRX)

#if defined(DBE7TRXI_C) || defined(SS_USE_INLINE)

SS_INLINE void dbe_trxinfo_initbuf(
        rs_sysi_t*      cd,
        dbe_trxinfo_t*  ti)
{
        ti->ti_nlinks = 1;
        ti->ti_state = DBE_TRXST_BEGIN;
        ti->ti_usertrxid = DBE_TRXID_NULL;
        ti->ti_maxtrxnum = DBE_TRXNUM_NULL;
        ti->ti_committrxnum = DBE_TRXNUM_NULL;
        ti->ti_actlistnode = NULL;
#ifdef SS_HSBG2
        ti->ti_hsbcd = NULL;                        /* HSB specific cd. */
#endif /* SS_HSBG2 */
        ss_debug(ti->ti_chk = DBE_CHK_TRXINFO);
}

SS_INLINE void dbe_trxinfo_done(dbe_trxinfo_t* ti, rs_sysi_t* cd, SsSemT* trxbuf_sem)
{
        SsSemEnter(trxbuf_sem);

        dbe_trxinfo_done_nomutex(ti, cd);

        SsSemExit(trxbuf_sem);
}

SS_INLINE void dbe_trxinfo_donebuf(
        dbe_trxinfo_t* ti,
        rs_sysi_t* cd,
        SsSemT* trxbuf_sem)
{
        SsSemEnter(trxbuf_sem);

        dbe_trxinfo_donebuf_nomutex(ti, cd);

        SsSemExit(trxbuf_sem);
}

SS_INLINE void dbe_trxinfo_donebuf_nomutex(
        dbe_trxinfo_t* ti,
        rs_sysi_t* cd)
{
#ifdef SS_HSBG2
        if (ti->ti_hsbcd != NULL) {
            rs_sysi_done(ti->ti_hsbcd);
            ti->ti_hsbcd = NULL;
        }
#endif /* SS_HSBG2 */
        ss_debug(ti->ti_chk = DBE_CHK_FREED_TRXINFO);
}

SS_INLINE void dbe_trxinfo_link(dbe_trxinfo_t* ti, SsSemT* trxbuf_sem)
{
        ss_debug(ss_int4_t dbg_nlinks;)

        CHK_TRXINFO(ti);

        SsSemEnter(trxbuf_sem);

        /* MUTEX BEGIN */
        ss_debug(dbg_nlinks = ti->ti_nlinks);

        ss_dassert(ti->ti_nlinks > 0);

        ti->ti_nlinks++;

        /* MUTEX END */
        ss_rc_dassert(dbg_nlinks + 1 == ti->ti_nlinks, ti->ti_nlinks);

        SsSemExit(trxbuf_sem);
}

SS_INLINE void dbe_trxinfo_removehsbcd(dbe_trxinfo_t* ti)
{
        CHK_TRXINFO(ti);

        /* MUTEX BEGIN */
        if (ti->ti_hsbcd != NULL) {
            rs_sysi_done(ti->ti_hsbcd);
            ti->ti_hsbcd = NULL;
        }
        /* MUTEX END */
}

SS_INLINE ss_int4_t dbe_trxinfo_nlinks(
        dbe_trxinfo_t*  ti)
{
        CHK_TRXINFO(ti);

        return ti->ti_nlinks;
}

#endif /* defined(DBE7TRXI_C) || defined(SS_USE_INLINE) */

#endif /* DBE7TRXI_H */
