/*************************************************************************\
**  source       * dbe0catchup.h
**  directory    * dbe
**  description  * catchup log position object handling.
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


#ifndef DBE0CATCHUP_H
#define DBE0CATCHUP_H

#include <ssenv.h>
#include <ssc.h>
#include <ssdebug.h>
#include <ssthread.h>
#include <ssint8.h>

#include "dbe0type.h"
#include "dbe0hsbstate.h"

#ifdef HSB_LPID
#define dbe_hsb_lpid_t  ss_int8_t
#else
#define dbe_hsb_lpid_t  ss_int4_t
#endif

typedef struct {
        ss_debug(int    lp_check;)      /* check field */
        dbe_logfnum_t   lp_logfnum;     /* log file number */
        su_daddr_t      lp_daddr;       /* block address in physical file */
        size_t          lp_bufpos;      /* position in buffer */
#ifdef HSB_LPID
        dbe_hsb_lpid_t  lp_id;          /* log position id */
        hsb_role_t      lp_role;        /* logging role, hsb_role_t */
#endif
} dbe_catchup_logpos_t;

typedef struct {
        ss_debug(int            cs_chk;)
        dbe_catchup_logpos_t    cs_remote;
        dbe_catchup_logpos_t    cs_local;
        int                     cs_nlinks;
        bool                    cs_durablemark;
} dbe_catchup_savedlogpos_t;

typedef struct dbe_catchup_st dbe_catchup_t;

#ifdef HSB_LPID

#define DBE_LOGPOS_LPIDBUFSIZE     21

#define DBE_LOGPOS_STRINGBUFSIZE   (DBE_LOGPOS_LPIDBUFSIZE + 1 + 3 + 1+ 5 + 1 + 10 + 1 + 6 + 1)
#define DBE_LOGPOS_BINSIZE         (3 * sizeof(ss_uint4_t) + sizeof(dbe_hsb_lpid_t) + 1)
#else
#define DBE_LOGPOS_STRINGBUFSIZE   (5 + 1 + 10 + 1 + 6 + 1)
#define DBE_LOGPOS_BINSIZE         (3 * sizeof(ss_uint4_t))
#endif

#define LOGPOS_DDD(lp) (lp).lp_logfnum, (lp).lp_daddr, (lp).lp_bufpos

#ifdef HSB_LPID
#define LPID_STORETODISK(p,v)   DBE_BLOBG2SIZE_PUTTODISK(p,v)
#define LPID_LOADFROMDISK(p)    DBE_BLOBG2SIZE_GETFROMDISK(p)
#define LPID_SETZERO(l)         SsInt8Set0(&l)
#define LPID_ISZERO(l)          SsInt8Is0(l)
#else
#define LPID_STORETODISK(p,v)   SS_UINT2_STORETODISK(p,v)
#define LPID_LOADFROMDISK(p)    SS_UINT4_LOADFROMDISK(p)
#define LPID_SETZERO(l)         l = 0
#define LPID_ISZERO(l)          (l == 0)
#endif

#ifdef HSB_LPID
#define LPID_GETLONG(l)      SsInt8GetLeastSignificantUint4(l)
#define LOGPOS_DSDDD(lp)     LPID_GETLONG((lp).lp_id), dbe_catchup_role_as_string((lp).lp_role), (lp).lp_logfnum, (lp).lp_daddr, (lp).lp_bufpos
#define LOGPOS_DS(lp)        LPID_GETLONG((lp).lp_id), dbe_catchup_role_as_string((lp).lp_role)
#define LOGPOS_CHECKLOGROLE(role) ss_rc_dassert(role == HSB_ROLE_PRIMARY || role == HSB_ROLE_SECONDARY ||\
                                             role == HSB_ROLE_STANDALONE, role);
#else
#define LPID_GETLONG(l)      l
#define LOGPOS_DSDDD(lp)     0, "NOTUSED", (lp).lp_logfnum, (lp).lp_daddr, (lp).lp_bufpos
#define LOGPOS_DS(lp)        0, "NOTUSED"
#define LOGPOS_CHECKLOGROLE(role)
#endif

#define DBE_CATCHUP_LOGPOS_SET(lp,logfnum,daddr,bufpos) dbe_catchup_logpos_set(&(lp),logfnum,daddr,bufpos)
#define DBE_CATCHUP_LOGPOS_SET_NULL(lp)                 dbe_catchup_logpos_set_null(&(lp))
#define DBE_CATCHUP_LOGPOS_ISNULL(lp)                   dbe_catchup_logpos_is_null(&(lp))

#define DBE_CATCHUP_LOGPOS_NULL                         dbe_catchup_logpos_nullvalue()

dbe_catchup_t* dbe_catchup_init(
                    dbe_db_t *db, 
                    dbe_catchup_logpos_t logpos);

void dbe_catchup_done(dbe_catchup_t *dcu);

su_ret_t dbe_catchup_next_logdata(
        dbe_catchup_t *dcu, 
        dbe_logdata_t **ld);

int dbe_catchup_logpos_cmp(
        dbe_catchup_logpos_t logpos1,
        dbe_catchup_logpos_t logpos2);

int dbe_catchup_logpos_idcmp(
        dbe_catchup_logpos_t logpos1,
        dbe_catchup_logpos_t logpos2);

bool dbe_catchup_logpos_cancompare(
        dbe_catchup_logpos_t* p_logpos1,
        dbe_catchup_logpos_t* p_logpos2);

long dbe_catchup_logpos_dbg_id(
        dbe_catchup_logpos_t logpos);

dbe_hsb_lpid_t dbe_catchup_logpos_id(
        dbe_catchup_logpos_t logpos);

dbe_catchup_logpos_t dbe_catchup_logpos_dbg_byid(
        long id);

hsb_role_t dbe_catchup_logpos_role(
        dbe_catchup_logpos_t* p_logpos);

void dbe_catchup_logpos_check(
        dbe_catchup_logpos_t logpos);

char* dbe_catchup_role_as_string(
        hsb_role_t role);

char *dbe_catchup_logpos_as_string(
        dbe_catchup_logpos_t lp);

void dbe_catchup_logpos_to_string(
        dbe_catchup_logpos_t lp,
        char *buf);

#ifdef HSB_LPID
char *dbe_catchup_logpos_lpid_as_string(dbe_catchup_logpos_t lp);
#endif

dbe_catchup_logpos_t dbe_catchup_logpos_init_from_string(
        char *buf);

void dbe_catchup_logpos_set(
        dbe_catchup_logpos_t* p_lp,
        dbe_logfnum_t         logfnum,
        su_daddr_t            daddr,
        size_t                bufpos);

void dbe_catchup_logpos_set_newid(
        dbe_catchup_logpos_t* p_lp,
        dbe_db_t* db,
        hsb_role_t role);

ss_int8_t dbe_catchup_logpos_get_newid(
        dbe_counter_t* counter);

void dbe_catchup_logpos_setmaxidfromlogpos(
        dbe_catchup_logpos_t* p_lp,
        dbe_db_t* db);

void dbe_catchup_logpos_set_id(
        dbe_catchup_logpos_t* p_lp,
        dbe_hsb_lpid_t id,
        hsb_role_t role);

void dbe_catchup_logpos_set_role(
        dbe_catchup_logpos_t* p_logpos,
        hsb_role_t role);

void dbe_catchup_logpos_set_null(
        dbe_catchup_logpos_t* p_lp);

bool dbe_catchup_logpos_is_null(
        dbe_catchup_logpos_t* p_lp);

bool dbe_catchup_logpos_is_nullid(
        dbe_catchup_logpos_t* p_lp);

bool dbe_catchup_logpos_is_nulllogaddr(
        dbe_catchup_logpos_t* p_lp);

dbe_catchup_logpos_t dbe_catchup_logpos_getfirstusedlogpos(
        dbe_db_t* db);

void dbe_catchup_logpos_setcatchuplogpos(
        dbe_db_t* db, 
        dbe_catchup_logpos_t lp);

dbe_catchup_savedlogpos_t* dbe_catchup_savedlogpos_init(
        dbe_catchup_logpos_t* remote_logpos,
        bool durablemark);

void dbe_catchup_savedlogpos_done(
        dbe_catchup_savedlogpos_t* cs);

void dbe_catchup_savedlogpos_link(
        dbe_catchup_savedlogpos_t* cs);

void dbe_catchup_savedlogpos_setpos(
        dbe_catchup_savedlogpos_t* cs,
        dbe_logfnum_t logfnum,
        su_daddr_t daddr,
        size_t bufpos);

dbe_catchup_logpos_t dbe_catchup_logpos_nullvalue(
        void);

#endif /* DBE0CATCHUP_H */
