/*************************************************************************\
**  source       * dbe0catchup.c
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
#include <ssscan.h>
#include <ssdebug.h>

#ifdef SS_HSBG2

#include "dbe9type.h"
#include "dbe6gobj.h"
#include "dbe0db.h"
#include "dbe0type.h"
#include "dbe9type.h"
#include "dbe7logf.h"
#include "dbe7rfl.h"
#include "dbe7ctr.h"
#include "dbe0db.h"
#include "dbe0ld.h"
#include "dbe0catchup.h"

#define CHK_LOGPOS(lp)  ss_dassert(SS_CHKPTR(lp) && (lp)->lp_check == DBE_CHK_LOGPOS)
#define CHK_CATCHUP(x)  ss_dassert(SS_CHKPTR(x) && (x)->dcu_check == DBE_CHK_CATCHUP)

struct dbe_catchup_st {
        ss_debug(dbe_chk_t      dcu_check;)
        dbe_db_t *              dcu_db;
        dbe_rflog_t *           dcu_rflog;
        dbe_catchup_logpos_t    dcu_last_logpos;
};

int dbe_catchup_logpos_cmp(
        dbe_catchup_logpos_t logpos1,
        dbe_catchup_logpos_t logpos2)
{
        long diff;

        CHK_LOGPOS(&logpos1);
        CHK_LOGPOS(&logpos2);

        diff = (long)logpos1.lp_logfnum - (long)logpos2.lp_logfnum;
        /* ss_dprintf_1(("dbe_catchup_logpos_cmp:diff %ld fnum\n", diff)); */
        if (diff == 0L) {
            diff = (long)logpos1.lp_daddr - (long)logpos2.lp_daddr;
            /* ss_dprintf_1(("dbe_catchup_logpos_cmp:diff %ld daddr\n", diff)); */
        }
        if (diff == 0L) {
            diff = (long)logpos1.lp_bufpos - (long)logpos2.lp_bufpos;
            /* ss_dprintf_1(("dbe_catchup_logpos_cmp:diff %ld bufpos\n", diff)); */
        }

        if (diff < 0L) {
            return(-1);
        } else if (diff > 0L) {
            return(1);
        }
        return(0);
}


int dbe_catchup_logpos_idcmp(
        dbe_catchup_logpos_t logpos1,
        dbe_catchup_logpos_t logpos2)
{
        long diff;
        bool null1;
        bool null2;

        CHK_LOGPOS(&logpos1);
        CHK_LOGPOS(&logpos2);

#ifdef HSB_LPID

        null1 = dbe_catchup_logpos_is_null(&logpos1);
        null2 = dbe_catchup_logpos_is_null(&logpos2);

        if (null1 && null2) {
            return(0);
        }

        if (null1) {
            return(-1);
        }

        if (null2) {
            return(1);
        }

        diff = SsInt8Cmp(logpos1.lp_id, logpos2.lp_id);

        if (diff < 0L) {
            return(-1);
        } else if (diff > 0L) {
            return(1);
        }
#endif
        return(0);
}



bool dbe_catchup_logpos_cancompare(
        dbe_catchup_logpos_t* p_logpos1,
        dbe_catchup_logpos_t* p_logpos2)
{
#ifdef HSB_LPID
        bool null1;
        bool null2;

        CHK_LOGPOS(p_logpos1);
        CHK_LOGPOS(p_logpos2);

        null1 = dbe_catchup_logpos_is_null(p_logpos1);
        null2 = dbe_catchup_logpos_is_null(p_logpos2);

        if (null1 && null2) {
            return(TRUE);
        } else if (null1) {
            return(p_logpos2->lp_role == HSB_ROLE_PRIMARY || p_logpos2->lp_role == HSB_ROLE_SECONDARY);
        } else if (null2) {
            return(p_logpos1->lp_role == HSB_ROLE_PRIMARY || p_logpos1->lp_role == HSB_ROLE_SECONDARY);
        } else {
            return(
                (p_logpos1->lp_role == HSB_ROLE_PRIMARY && p_logpos2->lp_role == HSB_ROLE_SECONDARY) ||
                (p_logpos2->lp_role == HSB_ROLE_PRIMARY && p_logpos1->lp_role == HSB_ROLE_SECONDARY)
            );
        }
#else
        return(TRUE);
#endif
}

dbe_hsb_lpid_t dbe_catchup_logpos_id(
        dbe_catchup_logpos_t logpos)
{
        CHK_LOGPOS(&logpos);

#ifdef HSB_LPID
        return(logpos.lp_id);
#else
        return(dbe_catchup_logpos_dbg_id(logpos));
#endif
}

long dbe_catchup_logpos_dbg_id(
        dbe_catchup_logpos_t logpos)
{
        long id;

        CHK_LOGPOS(&logpos);

#ifdef HSB_LPID
        id = LPID_GETLONG(logpos.lp_id);
#else
        id =  10000L*logpos.lp_daddr;
        id = id + logpos.lp_bufpos;
#endif

        return(id);
}

hsb_role_t dbe_catchup_logpos_role(
        dbe_catchup_logpos_t* p_logpos)
{
        CHK_LOGPOS(p_logpos);

#ifdef HSB_LPID
        return(p_logpos->lp_role);
#else
        return(HSB_ROLE_NONE);
#endif
}

dbe_catchup_logpos_t dbe_catchup_logpos_init_from_string(char *value)
{
        char *mismatch;
        long fnum, daddr, bufpos;
        ss_int8_t id;
        long role;
        bool succp = TRUE;
        dbe_catchup_logpos_t lp;

        fnum = 0L;
        daddr = 0L;
        bufpos = 0L;

        if (value == NULL) {
            succp = FALSE;
            ss_dprintf_4(("dbe_catchup_logpos_init_from_string:NULL\n"));
        }

#ifdef HSB_LPID
        if (succp) {
            succp = SsStrScanInt8(value, &id, &mismatch);
            succp = succp && mismatch != NULL && *mismatch == ':';
            if (!succp) {
                ss_dprintf_4(("dbe_catchup_logpos_init_from_string:id failed:value=%s,mismatch=%s\n", value, mismatch));
                ss_derror;
            }
        }

        if (succp) {
            value = mismatch + 1;
            succp = SsStrScanLong(value, &role, &mismatch);
            succp = succp && mismatch != NULL && *mismatch == ':';
            if (!succp) {
                ss_dprintf_4(("dbe_catchup_logpos_init_from_string:daddr failed:value=%s,mismatch=%s\n", value, mismatch));
                ss_derror;
            }
            ss_rc_dassert(role == HSB_ROLE_PRIMARY || role == HSB_ROLE_SECONDARY ||
                          role == HSB_ROLE_STANDALONE || role == HSB_ROLE_NONE, role);
        }
#endif
        if (succp) {
#ifdef HSB_LPID
            value = mismatch + 1;
#endif
            succp = SsStrScanLong(value, &fnum, &mismatch);
            succp = succp && mismatch != NULL && *mismatch == ':';
            if (!succp) {
                ss_dprintf_4(("dbe_catchup_logpos_init_from_string:fnum failed:value=%s,mismatch=%s\n", value, mismatch));
                ss_derror;
            }
        }

        if (succp) {
            value = mismatch + 1;
            succp = SsStrScanLong(value, &daddr, &mismatch);
            succp = succp && mismatch != NULL && *mismatch == ':';
            if (!succp) {
                ss_dprintf_4(("dbe_catchup_logpos_init_from_string:daddr failed:value=%s,mismatch=%s\n", value, mismatch));
                ss_derror;
            }
        }

        if (succp) {
            value = mismatch + 1;
            succp = SsStrScanLong(value, &bufpos, &mismatch);
            succp = succp && mismatch != NULL && *mismatch == '\0';
            if (!succp) {
                ss_dprintf_4(("dbe_catchup_logpos_init_from_string:bufpos failed:value=%s,mismatch=%s\n", value, mismatch != NULL ? mismatch : "NULL"));
                ss_derror;
            }
        }

        if (succp && fnum > 0L) {
            DBE_CATCHUP_LOGPOS_SET(lp, fnum, daddr, bufpos);
#ifdef HSB_LPID
            dbe_catchup_logpos_set_id(&lp, id, role);
#endif
        } else {
            DBE_CATCHUP_LOGPOS_SET_NULL(lp);
        }

        return (lp);
}

char* dbe_catchup_role_as_string(hsb_role_t role)
{
        if (role == HSB_ROLE_NONE) {
            return((char *)"NONE");
        } else {
            return(dbe_hsbstate_getrolestring(role));
        }
}

/* buf must be at least DBE_LOGPOS_STRINGBUFSIZE bytes */
void dbe_catchup_logpos_to_string(dbe_catchup_logpos_t lp, char *buf)
{
#ifdef USE_LOGFILE_GENNAME

        char *cp;

        CHK_LOGPOS(&lp);

        cp = dbe_logfile_genname("", "#####", lp.lp_logfnum, '#'),

        SsSprintf(
            buf,
            "%s:%010d:%06d",
            cp,
            lp.lp_daddr,
            lp.lp_bufpos);

        SsMemFree(cp);
#else
        CHK_LOGPOS(&lp);

#ifdef HSB_LPID
        SsInt8ToAscii(lp.lp_id, buf, 10, 21, '0', FALSE);
        SsSprintf(
            buf + strlen(buf),
            ":%d:%04d:%010d:%06d",
            lp.lp_role,
            lp.lp_logfnum,
            lp.lp_daddr,
            lp.lp_bufpos);
#else
        SsSprintf(
            buf,
            "%04d:%010d:%06d",
            lp.lp_logfnum,
            lp.lp_daddr,
            lp.lp_bufpos);
#endif
#endif
}


#ifdef HSB_LPID
char *dbe_catchup_logpos_lpid_as_string(dbe_catchup_logpos_t lp)
{
        char *buf;
        char idstr[DBE_LOGPOS_LPIDBUFSIZE+1];
        char *rolestr;

        CHK_LOGPOS(&lp);

        ss_dassert(DBE_LOGPOS_LPIDBUFSIZE >= 21);
        SsInt8ToAscii(lp.lp_id, idstr, 10, 21, '0', FALSE);

        if (lp.lp_role == HSB_ROLE_NONE) {
            rolestr = (char *)"NONE";
        } else {
            rolestr = dbe_hsbstate_getrolestring_user(lp.lp_role);
        }
        ss_dassert(rolestr != NULL);

        buf = SsMemAlloc(DBE_LOGPOS_LPIDBUFSIZE+1+1+strlen(rolestr)+1);
        SsSprintf(buf, "%s:%s", idstr, rolestr);
        return (buf);
}
#endif

char *dbe_catchup_logpos_as_string(dbe_catchup_logpos_t lp)
{
        char *buf;

        CHK_LOGPOS(&lp);

        buf = SsMemAlloc(DBE_LOGPOS_STRINGBUFSIZE);

        dbe_catchup_logpos_to_string(lp, buf);

        return (buf);
}

void dbe_catchup_logpos_check(
        dbe_catchup_logpos_t lp)
{
#ifdef HSB_LPID
        ss_rc_assert(lp.lp_role == HSB_ROLE_NONE      || lp.lp_role == HSB_ROLE_PRIMARY ||
                     lp.lp_role == HSB_ROLE_SECONDARY || lp.lp_role == HSB_ROLE_STANDALONE, lp.lp_role);
#endif
        /* ss_rc_assert((lp).lp_logfnum >= 0 && (lp).lp_logfnum < 2000, (lp).lp_logfnum); */
        ss_rc_dassert((lp).lp_daddr < 10000000, (lp).lp_daddr);
        ss_rc_dassert((lp).lp_bufpos <= 64*1024, (lp).lp_bufpos);
}

void dbe_catchup_logpos_set(
        dbe_catchup_logpos_t* p_lp,
        dbe_logfnum_t         logfnum,
        su_daddr_t            daddr,
        size_t                bufpos)
{
        ss_dassert(logfnum > 0);

        ss_debug(p_lp->lp_check = DBE_CHK_LOGPOS);
#ifdef HSB_LPID
        SsInt8Set0(&p_lp->lp_id);
        p_lp->lp_role = HSB_ROLE_NONE;
#endif
        p_lp->lp_logfnum = logfnum;
        p_lp->lp_daddr = daddr;
        p_lp->lp_bufpos = bufpos;
}

void dbe_catchup_logpos_set_newid(
        dbe_catchup_logpos_t* p_lp,
        dbe_db_t* db,
        hsb_role_t role)
{
        CHK_LOGPOS(p_lp);

#ifdef HSB_LPID
        ss_dassert(role == HSB_ROLE_PRIMARY);

        p_lp->lp_id = dbe_catchup_logpos_get_newid(dbe_db_getcounter(db));
        p_lp->lp_role = role;
#endif
}

dbe_hsb_lpid_t dbe_catchup_logpos_get_newid(
        dbe_counter_t* counter)
{
#ifdef HSB_LPID
        dbe_hsb_lpid_t lpid;

        lpid = dbe_counter_getnewint8trxid(counter);

        ss_dprintf_1(("dbe_catchup_logpos_get_newid:newid=%ld\n", LPID_GETLONG(lpid)));

        return(lpid);
#else
        return(0);
#endif
}

void dbe_catchup_logpos_setmaxidfromlogpos(
        dbe_catchup_logpos_t* p_lp,
        dbe_db_t* db)
{
        CHK_LOGPOS(p_lp);

#ifdef HSB_LPID
        ss_dprintf_1(("dbe_catchup_logpos_setmaxidfromlogpos:id=%ld\n", LPID_GETLONG(p_lp->lp_id)));

        dbe_counter_setint8trxid(dbe_db_getcounter(db), p_lp->lp_id);
#endif
}

void dbe_catchup_logpos_set_id(
        dbe_catchup_logpos_t* p_lp,
        dbe_hsb_lpid_t id,
        hsb_role_t role)
{
        CHK_LOGPOS(p_lp);

#ifdef HSB_LPID
        ss_rc_dassert(role == HSB_ROLE_NONE      || role == HSB_ROLE_PRIMARY ||
                      role == HSB_ROLE_SECONDARY || role == HSB_ROLE_STANDALONE, role);
        p_lp->lp_id = id;
        p_lp->lp_role = role;
#endif
}

void dbe_catchup_logpos_set_role(
        dbe_catchup_logpos_t* p_logpos,
        hsb_role_t role)
{
        CHK_LOGPOS(p_logpos);

#ifdef HSB_LPID
        ss_rc_dassert(role == HSB_ROLE_NONE      || role == HSB_ROLE_PRIMARY ||
                      role == HSB_ROLE_SECONDARY || role == HSB_ROLE_STANDALONE, role);
        p_logpos->lp_role = role;
#endif
}

void dbe_catchup_logpos_set_null(dbe_catchup_logpos_t* p_lp)
{
        memset(p_lp, '\0', sizeof(dbe_catchup_logpos_t));
#ifdef HSB_LPID
        p_lp->lp_role = HSB_ROLE_NONE;
#endif
        ss_debug(p_lp->lp_check = DBE_CHK_LOGPOS);
}

bool dbe_catchup_logpos_is_null(dbe_catchup_logpos_t* p_lp)
{
        CHK_LOGPOS(p_lp);

        return(
#ifdef HSB_LPID
            SsInt8Is0(p_lp->lp_id) &&
            p_lp->lp_role == HSB_ROLE_NONE &&
#endif
            p_lp->lp_logfnum == 0 &&
            p_lp->lp_daddr == 0 &&
            p_lp->lp_bufpos == 0);
}

bool dbe_catchup_logpos_is_nullid(dbe_catchup_logpos_t* p_lp)
{
        CHK_LOGPOS(p_lp);

        return(SsInt8Is0(p_lp->lp_id));
}

bool dbe_catchup_logpos_is_nulllogaddr(dbe_catchup_logpos_t* p_lp)
{
        CHK_LOGPOS(p_lp);

        return(
            p_lp->lp_logfnum == 0 &&
            p_lp->lp_daddr == 0 &&
            p_lp->lp_bufpos == 0);
}

dbe_catchup_logpos_t dbe_catchup_logpos_getfirstusedlogpos(dbe_db_t* db)
{
        dbe_catchup_logpos_t logpos;
        dbe_hsbg2_t *svc;

        svc = dbe_db_gethsbsvc(db);
        logpos = dbe_hsbg2_getfirstusedlogpos(svc);
        CHK_LOGPOS(&logpos);

        return(logpos);
}

dbe_catchup_savedlogpos_t* dbe_catchup_savedlogpos_init(
        dbe_catchup_logpos_t* remote_logpos,
        bool durablemark)
{
        dbe_catchup_savedlogpos_t* cs;

        cs = SSMEM_NEW(dbe_catchup_savedlogpos_t);

        cs->cs_remote = *remote_logpos;
        DBE_CATCHUP_LOGPOS_SET_NULL(cs->cs_local);
#ifdef HSB_LPID
        cs->cs_local.lp_id = remote_logpos->lp_id;
        cs->cs_local.lp_role = remote_logpos->lp_role;
#endif
        cs->cs_durablemark = durablemark;
        cs->cs_nlinks = 1;
        ss_debug(cs->cs_chk = DBE_CHK_SAVEDLOGPOS);

        return(cs);
}

void dbe_catchup_savedlogpos_done(dbe_catchup_savedlogpos_t* cs)
{
        bool freep;

        ss_dassert(cs->cs_chk == DBE_CHK_SAVEDLOGPOS);

        SsSemEnter(ss_lib_sem);
        cs->cs_nlinks--;
        freep = (cs->cs_nlinks == 0);
        SsSemExit(ss_lib_sem);

        if (freep) {
            SsMemFree(cs);
        }
}

void dbe_catchup_savedlogpos_link(dbe_catchup_savedlogpos_t* cs)
{
        ss_dassert(cs->cs_chk == DBE_CHK_SAVEDLOGPOS);

        SsSemEnter(ss_lib_sem);
        cs->cs_nlinks++;
        SsSemExit(ss_lib_sem);
}

void dbe_catchup_savedlogpos_setpos(
        dbe_catchup_savedlogpos_t* cs,
        dbe_logfnum_t logfnum,
        su_daddr_t daddr,
        size_t bufpos)
{
        ss_dassert(cs->cs_chk == DBE_CHK_SAVEDLOGPOS);

        cs->cs_local.lp_logfnum = logfnum;
        cs->cs_local.lp_daddr = daddr;
        cs->cs_local.lp_bufpos = bufpos;

        ss_dprintf_1(("dbe_catchup_savedlogpos_setpos:remote (%d,%s,%d,%d,%d) local (%d,%s,%d,%d,%d)\n",
            LOGPOS_DSDDD(cs->cs_remote), LOGPOS_DSDDD(cs->cs_local)));
}

dbe_catchup_logpos_t dbe_catchup_logpos_nullvalue(void)
{
        static dbe_catchup_logpos_t catchup_logpos_nullvalue;

        ss_debug(catchup_logpos_nullvalue.lp_check = DBE_CHK_LOGPOS);

#ifdef HSB_LPID
        if (catchup_logpos_nullvalue.lp_role == 0) {
            catchup_logpos_nullvalue.lp_role = HSB_ROLE_NONE;
        }
#endif
        return(catchup_logpos_nullvalue);
}

#endif /* SS_HSBG2 */

