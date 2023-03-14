/*************************************************************************\
**  source       * rs0sysi.c
**  directory    * res
**  description  * System info type for client data pointer
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

#define RS0SYSI_C

#include <ssc.h>
#include <ssdebug.h>
#include <sssem.h>
#include <ssthread.h>

#include <su0prof.h>
#include <su0vfil.h>
#include <su0rbtr.h>
#include <su0list.h>
#include <su0usrid.h>
#include <su0parr.h>
#include <su0task.h>

#include "rs0types.h"
#include "rs0auth.h"
#include "rs0sqli.h"
#include "rs0sdefs.h"
#include "rs0sysi.h"
#include "rs0defno.h"

#define MAXSAVEDTASKS   2

bool rs_sysi_shutdown = FALSE;

#ifdef SS_PMEM

/* Dummy functions before rs_sysi_staticinstallpmem */
static void pmem_dummy1(SsPmemCtxT* p) { SS_NOTUSED(p); }
static void pmem_dummy2(SsPmemCtxT* p, bool b) { SS_NOTUSED(p); SS_NOTUSED(b); }
static void pmem_dummy3(SsPmemCtxT** p) { SS_NOTUSED(p); }
static void pmem_dummy4(SsPmemCtxT* p) { SS_NOTUSED(p); }

static void (*SsPmemCtxDonePtr)(SsPmemCtxT* ctx) = pmem_dummy1;
static void (*SsPmemSweepCtxPtr)(SsPmemCtxT* ctx, bool global) = pmem_dummy2;
static void (*SsPmemInstallThreadCtxPtr)(SsPmemCtxT** p_ctx) = pmem_dummy3;
static void (*SsPmemUninstallThreadCtxPtr)(SsPmemCtxT* ctx) = pmem_dummy4;

/*##**********************************************************************\
 *
 *              rs_sysi_staticinstallpmem
 *
 * Installs pmem context functions by initializing pointers to them
 *
 * Parameters :
 *
 *      ctxdone - in, hold
 *              pointer to context delete function
 *
 *      ctxsweep - in, hold
 *              pointer to context sweep function
 *
 *      ctxinstall - in, hold
 *              pointer to context install function
 *
 *      ctxuninstall - in, hold
 *              pointer to context uninstall function
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_sysi_staticinstallpmem(
        void (*ctxdone)(SsPmemCtxT*),
        void (*ctxsweep)(SsPmemCtxT*,bool),
        void (*ctxinstall)(SsPmemCtxT**),
        void (*ctxuninstall)(SsPmemCtxT*))
{
        SsPmemCtxDonePtr = ctxdone;
        SsPmemSweepCtxPtr = ctxsweep;
        SsPmemInstallThreadCtxPtr = ctxinstall;
        SsPmemUninstallThreadCtxPtr = ctxuninstall;
}

void rs_sysi_enablememctx(rs_sysi_t* sysi)
{
        if (sysi->si_memctxstat != SYSI_MEMCTX_NEVERINSTALL) {
            return;
        }
        ss_dassert(sysi->si_memctx == NULL);
        sysi->si_memctxstat = SYSI_MEMCTX_NOTINSTALLED;
}

void rs_sysi_installmemctx(rs_sysi_t* sysi)
{
        if (sysi == NULL
        ||  sysi->si_memctxstat == SYSI_MEMCTX_NEVERINSTALL)
        {
            return;
        }
        SsPmemInstallThreadCtxPtr(&sysi->si_memctx);
        sysi->si_memctxstat = SYSI_MEMCTX_INSTALLED;
}

void rs_sysi_uninstallmemctx(rs_sysi_t* sysi)
{
        SsPmemSweepCtxPtr(NULL, TRUE);
        if (sysi == NULL
        ||  sysi->si_memctxstat == SYSI_MEMCTX_NEVERINSTALL)
        {
            return;
        }
        sysi->si_memctxstat = SYSI_MEMCTX_NOTINSTALLED;
        SsPmemUninstallThreadCtxPtr(sysi->si_memctx);
}

#endif /* SS_PMEM */

#ifdef SS_SYNC
static sysi_syncinfo_t* sysi_syncinfo_init(char* catalog)
{
        sysi_syncinfo_t* syncinfo;

        if (catalog == NULL) {
            /* This should be possible only for the system user when
             * opening the database.
             */
            catalog = (char *)"";
        }

        syncinfo = SSMEM_NEW(sysi_syncinfo_t);

        syncinfo->ssi_catalog = SsMemStrdup(catalog);
        syncinfo->ssi_issyncmaster = FALSE;
        syncinfo->ssi_issyncreplica = FALSE;
        syncinfo->ssi_syncnode = NULL;
        syncinfo->ssi_nodetime = 0L;
        syncinfo->ssi_syncmasterid = -1L;
        syncinfo->ssi_msgnamelocklist = su_list_init(NULL);
        syncinfo->ssi_syncmsg_rbt = NULL;
        syncinfo->ssi_bboard = NULL;
        syncinfo->ssi_propertystore = NULL;
        syncinfo->ssi_defaultpropagatewherestr = NULL;
        syncinfo->ssi_ipub = NULL;
        syncinfo->ssi_syncusername = NULL;
        syncinfo->ssi_syncpassword = NULL;
        syncinfo->ssi_syncid = 1 << RS_FIRST_SYNCID_BIT;
        ss_dassert(syncinfo->ssi_syncid == 128);

        return(syncinfo);
}

static void sysi_syncinfo_done(void* data)
{
        sysi_syncinfo_t* syncinfo = data;

        if (syncinfo->ssi_syncusername != NULL) {
            SsMemFree(syncinfo->ssi_syncusername);
            dynva_free(&syncinfo->ssi_syncpassword);
        }
        if (syncinfo->ssi_syncnode != NULL) {
            SsMemFree(syncinfo->ssi_syncnode);
        }
        if (syncinfo->ssi_defaultpropagatewherestr != NULL) {
            SsMemFree(syncinfo->ssi_defaultpropagatewherestr);
        }
#ifndef SS_MYSQL
        if (syncinfo->ssi_bboard != NULL) {
            rs_bboard_done(syncinfo->ssi_bboard);
        }
#endif
        su_list_done(syncinfo->ssi_msgnamelocklist);
        /* ss_dassert(syncinfo->ssi_syncmsg_rbt == NULL); */

        SsMemFree(syncinfo->ssi_catalog);

        SsMemFree(syncinfo);
}

static int sysi_syncinfo_insertcmp(void* key1, void* key2)
{
        sysi_syncinfo_t* syncinfo1 = key1;
        sysi_syncinfo_t* syncinfo2 = key2;

        return(strcmp(syncinfo1->ssi_catalog, syncinfo2->ssi_catalog));
}

static int sysi_syncinfo_searchcmp(void* search_key, void* rbt_key)
{
        sysi_syncinfo_t* rbt_syncinfo = rbt_key;

        return(strcmp(search_key, rbt_syncinfo->ssi_catalog));
}

#endif /* SS_SYNC */

static void seasemlist_done(void* data)
{
        SsSemFree(data);
}

/*##**********************************************************************\
 *
 *              rs_sysi_init
 *
 * Creates a new sysi object.
 *
 * Parameters :
 *
 * Return value - give :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_sysi_t* rs_sysi_init(void)
{
        rs_sysi_t* sysi = SsMemCalloc(sizeof(rs_sysi_t), 1);

        ss_dprintf_1(("rs_sysi_init:%p\n", sysi));

        ss_debug(sysi->si_chk = RSCHK_SYSI);
        sysi->si_nlink = 1;
        /* sysi->si_auth = NULL;                Done by SsMemCalloc */
        /* sysi->si_rbuf = NULL;                Done by SsMemCalloc */
        /* sysi->si_dbuser = NULL;              Done by SsMemCalloc */
        /* sysi->si_tbcon = NULL;               Done by SsMemCalloc */
        /* sysi->si_prof = NULL;                Done by SsMemCalloc */
        /* sysi->si_db = NULL;                  Done by SsMemCalloc */
        /* sysi->si_tabdb = NULL;               Done by SsMemCalloc */
        /* sysi->si_sqli = NULL;                Done by SsMemCalloc */
        /* sysi->si_rslinksem = NULL;           Done by SsMemCalloc */
        /* sysi->si_task = NULL;                Done by SsMemCalloc */
        /* sysi->si_task_callstack = NULL;      Done by SsMemCalloc */
        /* sysi->si_eventwaitwithtimeoutfp = NULL;   Done by SsMemCalloc */
        /* sysi->si_startlockwait_tmofp = NULL;      Done by SsMemCalloc */
        /* sysi->si_startlockwaitfp = NULL;          Done by SsMemCalloc */
        /* sysi->si_lockwaitfp = NULL;          Done by SsMemCalloc */
        /* sysi->si_lockwakeupfp = NULL;        Done by SsMemCalloc */
        /* sysi->si_signalfp = NULL;            Done by SsMemCalloc */
        sysi->si_userid = -1;
        sysi->si_dbuserid = -1;
        /* sysi->si_dateformat = NULL;          Done by SsMemCalloc */
        /* sysi->si_timeformat = NULL;          Done by SsMemCalloc */
        /* sysi->si_wait_events = NULL;         Done by SsMemCalloc */
        /* sysi->si_timestampformat = NULL;     Done by SsMemCalloc */
        /* sysi->si_xsmgr = NULL;               Done by SsMemCalloc */
        sysi->si_sem = SsSemCreateLocal(SS_SEMNUM_RES_SYSI);
        sysi->si_linksem = SsSemCreateLocal(SS_SEMNUM_RES_SYSILINK);
        sysi->si_sqlilevel = -1;                /* Use default from si_sqli */
        /* sysi->si_sqlifname = NULL;           Use default from si_sqli. Done by SsMemCalloc */
        /* sysi->si_sqlifp = NULL;              Use default. Done by SsMemCalloc */
        /* sysi->si_stmttabnameinfo = NULL;     Done by SsMemCalloc */
        sysi->si_sortarraysize  = -1;           /* Use default from si_sqli */
        sysi->si_convertorstounions = -1;       /* Use default from si_sqli */
        sysi->si_sortedgroupby = -1;            /* Use default from si_sqli */
        sysi->si_optimizerows = -1;             /* Use default from si_sqli */
        sysi->si_simpleoptimizerrules = -1;     /* Use default from si_sqli */
        /* sysi->si_blobrefbuf = NULL;          Done by SsMemCalloc */
        /* sysi->si_stmtmaxtime = 0;            Done by SsMemCalloc */
        /* sysi->si_logauditinfo = FALSE;       Done by SsMemCalloc */
        /* sysi->si_disableprintsqlinfo = 0;    Done by SsMemCalloc */
        /* sysi->si_events = NULL;              Done by SsMemCalloc */
        /* sysi->si_proccache = NULL;           Done by SsMemCalloc */
        /* sysi->si_trigcache = NULL;           Done by SsMemCalloc */
        /* sysi->si_trigexec = 0;               Done by SsMemCalloc */
        /* sysi->si_trigdone = 0;               Done by SsMemCalloc */
        sysi->si_trignamestack = su_pa_init();
        sysi->si_procnamestack = su_pa_init();
        /* sysi->si_seqidlist = NULL;           Done by SsMemCalloc */
        sysi->si_trend = rs_trend_init();
        sysi->si_stmttrend = rs_trend_init();
        /* sysi->si_ignoretimeout = 0;          Done by SsMemCalloc */
        sysi->si_prio = SU_TASK_PRIO_DEFAULT;
        /* sysi->si_eventnotifiers = NULL;      Done by SsMemCalloc */
        /* sysi->si_islocaluser = FALSE;        Done by SsMemCalloc */
        sysi->si_connecttype = RS_SYSI_CONNECT_USER;
        /* sysi->si_hsbinfo = 0;                Done by SsMemCalloc */
        /* sysi->si_ishsbconfigured = FALSE;    Done by SsMemCalloc */
        /* sysi->si_iscancelled = FALSE;        Done by SsMemCalloc */
        /* sysi->si_tbcur = NULL;               Done by SsMemCalloc */
        /* sysi->si_stepcountptr = NULL;        Done by SsMemCalloc */
        /* sysi->si_tbcurlist = NULL;           Done by SsMemCalloc */
        /* sysi->si_insidedbeatomicsection = FALSE; Done by SsMemCalloc */
#ifdef SS_PMEM
        /* sysi->si_memctx = NULL;              Done by SsMemCalloc */
        sysi->si_memctxstat = SYSI_MEMCTX_NOTINSTALLED;
#endif /* SS_PMEM */
#ifdef SS_SYNC
        sysi->si_cursyncinfo = sysi_syncinfo_init(RS_AVAL_DEFCATALOG);
        sysi->si_syncinforbt = su_rbt_inittwocmp(
                                    sysi_syncinfo_insertcmp,
                                    sysi_syncinfo_searchcmp,
                                    sysi_syncinfo_done);
        su_rbt_insert(sysi->si_syncinforbt, sysi->si_cursyncinfo);
        /* sysi->si_password = NULL;            Done by SsMemCalloc */
        sysi->si_syncsavemasterid = -1;
        sysi->si_replicaid_onpropagate = -1;

        /* sysi->si_subscribewrite = FALSE;     Done by SsMemCalloc */
        /* sysi->si_usehurc = FALSE;            Done by SsMemCalloc */
        /* sysi->si_usehurc_force = FALSE;      Done by SsMemCalloc */

#endif /* SS_SYNC */

        /* sysi->si_istransactive_fun = NULL;   Done by SsMemCalloc */
        /* sysi->si_istransactive_ctx = NULL;   Done by SsMemCalloc */
        /* sysi->si_defaultnode = NULL;         Done by SsMemCalloc */
        /* sysi->si_locktran = NULL;            Done by SsMemCalloc */
        /* sysi->si_locktran_link = 0;          Done by SsMemCalloc */
        /* sysi->si_hsbwaitmes = NULL;          Done by SsMemCalloc */

        /* sysi->si_getnewuniquemsgid_fun = NULL;   Done by SsMemCalloc */
        /* sysi->si_deferredblobunlinklist = NULL;  Done by SsMemCalloc */
        {
            ss_int4_t seed = (ss_int4_t)SsTime(NULL) +
                (ss_int4_t)(ulong)sysi;
            su_rand_init(&sysi->si_usrrand, seed);
            su_rand_init(&sysi->si_sysrand, seed);
        }

        /* sysi->si_hsbmergecount = 0;          Done by SsMemCalloc */
        /* sysi->si_hsbstmtcount = 0;           Done by SsMemCalloc */
        /* sysi->si_hsbsecactions = 0;          Done by SsMemCalloc */

        /* sysi->si_spnewplan = NULL;           Done by SsMemCalloc */
        /* sysi->si_tabnewplan = NULL;          Done by SsMemCalloc */
        /* sysi->si_liveovercommit = 0;         Done by SsMemCalloc */
        /* sysi->si_allowduplicatedelete = 0;   Done by SsMemCalloc */
        /* sysi->si_logwaitmeslist = NULL;      Done by SsMemCalloc */
        /* sysi->si_logwaitmes = NULL;          Done by SsMemCalloc */
        /* sysi->si_logwaitrc = NULL;           Done by SsMemCalloc */
        /* sysi->si_cryptopar = NULL;           Done by SsMemCalloc */
        /* sysi->si_flags = 0;                  Done by SsMemCalloc */
        /* sysi->si_tc_level = 0;               Done by SsMemCalloc */

        sysi->si_tc_optimisticlocktimeout = -1L;
        sysi->si_tc_locktimeout = -1L;
        sysi->si_tc_idletimeout = -1L;
        sysi->si_tc_write = FALSE;
        sysi->si_tc_rebalance = FALSE;

        sysi->si_seasemlist = su_list_init(seasemlist_done);
        su_list_startrecycle(sysi->si_seasemlist);

        return(sysi);
}

/*##**********************************************************************\
 *
 *              rs_sysi_link
 *
 * Increments link count in sysi object. Link count is decremented
 * by calling function rs_sysi_done.
 *
 * Parameters :
 *
 *      sysi -
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
void rs_sysi_link(
        rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return;
        }
        CHK_SYSI(sysi);
        ss_dprintf_1(("rs_sysi_link:%d\n", sysi->si_nlink));
        ss_dassert(sysi->si_nlink > 0);

        SsSemEnter(sysi->si_linksem);
        sysi->si_nlink++;
        SsSemExit(sysi->si_linksem);
}

/*##**********************************************************************\
 *
 *              rs_sysi_done
 *
 * Releases a sysi object.
 *
 * Parameters :
 *
 *      sysi - in, take
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
void rs_sysi_done(rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return;
        }
        CHK_SYSI(sysi);

        SsSemEnter(sysi->si_linksem);
        ss_dprintf_1(("rs_sysi_done:link=%d\n", sysi->si_nlink));
        ss_dassert(sysi->si_nlink > 0);

        sysi->si_nlink--;

        if (sysi->si_nlink == 0) {
            /* No more links, remove object.
             */
            SsSemExit(sysi->si_linksem);

            rs_trend_stmttransend(sysi->si_stmttrend, sysi, RS_TROP_DONE);
            rs_trend_transend(sysi->si_trend, sysi, RS_TROP_DONE);

            if (sysi->si_dateformat != NULL) {
                SsMemFree(sysi->si_dateformat);
            }
            if (sysi->si_timeformat != NULL) {
                SsMemFree(sysi->si_timeformat);
            }
            if (sysi->si_timestampformat != NULL) {
                SsMemFree(sysi->si_timestampformat);
            }
            if (sysi->si_auth != NULL) {
                rs_auth_done(sysi, sysi->si_auth);
            }
            if (sysi->si_sqlifp != NULL) {
                rs_sqli_closeinfofile(sysi->si_sqli, sysi->si_sqlifp);
            }
            ss_dassert(sysi->si_stmttabnameinfo == NULL);
            if (sysi->si_sqlifname != NULL) {
                SsMemFree(sysi->si_sqlifname);
            }

            su_list_done(sysi->si_seasemlist);

            ss_dassert(sysi->si_locktran == NULL);

            ss_dassert(su_pa_nelems(sysi->si_trignamestack) == 0);
            ss_dassert(su_pa_nelems(sysi->si_procnamestack) == 0);
            ss_dassert(sysi->si_seqidlist == NULL);
            su_pa_done(sysi->si_trignamestack);
            su_pa_done(sysi->si_procnamestack);
            if (sysi->si_wait_events) {
                su_rbt_done(sysi->si_wait_events);
            }
#ifdef SS_PMEM
            if (sysi->si_memctx != NULL) {
                ss_dassert(sysi->si_memctxstat != SYSI_MEMCTX_INSTALLED);
                SsPmemCtxDonePtr(sysi->si_memctx);
            }
#endif /* SS_PMEM */

#ifdef SS_SYNC
            su_rbt_done(sysi->si_syncinforbt);
            dynva_free(&sysi->si_password);
#endif /* SS_SYNC */
            rs_trend_done(sysi->si_stmttrend);
            rs_trend_done(sysi->si_trend);

            sysi->si_getnewuniquemsgid_fun = NULL;

            if (sysi->si_trxsem != NULL) {
                SsSemFree(sysi->si_trxsem);
            }
            SsSemFree(sysi->si_sem);
            SsSemFree(sysi->si_linksem);

            if (sysi->si_userid != -1) {
                su_usrid_done(sysi->si_userid);
            }

            if (sysi->si_defaultnode != NULL) {
                rs_defnode_done(sysi->si_defaultnode);
            }

            ss_dassert(sysi->si_hsbsecactions == NULL);
            if (sysi->si_hsbsecactions != NULL) {
                su_list_done(sysi->si_hsbsecactions);
            }
            ss_dassert(sysi->si_logwaitmes == NULL);
            ss_dassert(sysi->si_logwaitrc == NULL);

            ss_aassert(rs_sysi_testthrid(sysi));

            while (sysi->si_bkeybuflist != NULL) {
                void* bkeybuf;
                sysi_listnode_t* sl;

                bkeybuf = sysi->si_bkeybuflist;
                sl = (sysi_listnode_t*)sysi->si_bkeybuflist;
                CHK_SYSILISTNODE(sl);
                sysi->si_bkeybuflist = sl->sl_next;
                SsMemFree(bkeybuf);
                ss_aassert(sysi->si_bkeybuflistlen > 0);
                ss_autotest_or_debug(sysi->si_bkeybuflistlen--);
            }

            if (sysi->si_logqueue_freefun != NULL
                && sysi->si_logqueue != NULL) 
            {
                (*sysi->si_logqueue_freefun)(sysi->si_logqueue);
            }

            ss_aassert(sysi->si_bkeybuflistlen == 0);
            ss_debug(SsMemTrcFreeCallStk(sysi->si_task_callstack));

            SsMemFree(sysi);

        } else {
            SsSemExit(sysi->si_linksem);
        }
}

void rs_sysi_setconnecttype(
        rs_sysi_t* sysi,
        rs_sysi_connecttype_t type)
{
        CHK_SYSI(sysi);

        sysi->si_connecttype = type;
}

rs_sysi_connecttype_t rs_sysi_getconnecttype(
        rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return(RS_SYSI_CONNECT_USER);
        } else {
            CHK_SYSI(sysi);
            return(sysi->si_connecttype);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_insertrbuf
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      rbuf - in, hold
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
void rs_sysi_insertrbuf(rs_sysi_t* sysi, rs_rbuf_t* rbuf)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_rbuf = rbuf;
}

/*##**********************************************************************\
 *
 *              rs_sysi_rbuf
 *
 *
 *
 * Parameters :
 *
 *      si - in
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_rbuf_t* rs_sysi_rbuf(rs_sysi_t* sysi)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        return(sysi->si_rbuf);
}

/*##**********************************************************************\
 *
 *              rs_sysi_insertuser
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      user - in, hold
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
void rs_sysi_insertuser(rs_sysi_t* sysi, void* user)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_dbuser = user;
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 *
 *              rs_sysi_user
 *
 *
 *
 * Parameters :
 *
 *      sysi - in
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void* rs_sysi_user(rs_sysi_t* sysi)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        return(sysi->si_dbuser);
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *              rs_sysi_setuserid
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      userid - in
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
void rs_sysi_setuserid(rs_sysi_t* sysi, int userid)
{
        CHK_SYSI(sysi);

        if (userid == -1) {
            userid = su_usrid_init();
        } else {
            su_usrid_link(userid);
        }
        if (sysi->si_userid != -1) {
            su_usrid_done(sysi->si_userid);
        }
        sysi->si_userid = userid;
}

#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *              rs_sysi_userid
 *
 *
 *
 * Parameters :
 *
 *      sysi - in
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
int rs_sysi_userid(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
        }
        return(_RS_SYSI_USERID(sysi));
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *              rs_sysi_setdbuserid
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      dbuserid - in
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
void rs_sysi_setdbuserid(rs_sysi_t* sysi, long dbuserid)
{
        CHK_SYSI(sysi);

        sysi->si_dbuserid = dbuserid;
}

/*##**********************************************************************\
 *
 *              rs_sysi_dbuserid
 *
 *
 *
 * Parameters :
 *
 *      sysi - in
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
long rs_sysi_dbuserid(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_dbuserid);
        } else {
            return(-1);
        }
}

void rs_sysi_setlocaluser(rs_sysi_t* sysi)
{
        ss_dassert(sysi != NULL);

        CHK_SYSI(sysi);
        sysi->si_islocaluser = TRUE;
}


bool rs_sysi_islocaluser(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_islocaluser);
        } else {
            return(FALSE);
        }
}


/*##**********************************************************************\
 *
 *              rs_sysi_inserttbcon
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      tbcon - in, hold
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
void rs_sysi_inserttbcon(rs_sysi_t* sysi, void* tbcon)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_tbcon = tbcon;
}

/*##**********************************************************************\
 *
 *              rs_sysi_tbcon
 *
 *
 *
 * Parameters :
 *
 *      sysi - in
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void* rs_sysi_tbcon(rs_sysi_t* sysi)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        return(sysi->si_tbcon);
}

/*##**********************************************************************\
 *
 *              rs_sysi_insert_wait_events
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      tbcon - in, hold
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
void rs_sysi_insert_wait_events(rs_sysi_t* sysi, su_rbt_t* wait_events)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_wait_events = wait_events;
}

/*##**********************************************************************\
 *
 *              rs_sysi_wait_events
 *
 *
 *
 * Parameters :
 *
 *      sysi - in
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_rbt_t* rs_sysi_wait_events(rs_sysi_t* sysi)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        return(sysi->si_wait_events);
}

/*##**********************************************************************\
 *
 *              rs_sysi_insertauth
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      auth - in, hold
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
void rs_sysi_insertauth(rs_sysi_t* sysi, rs_auth_t* auth)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_auth = auth;
}

/*##**********************************************************************\
 *
 *              rs_sysi_insertdb
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      db - in, hold
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
void rs_sysi_insertdb(rs_sysi_t* sysi, void* db)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_db = db;
}

/*##**********************************************************************\
 *
 *              rs_sysi_inserttabdb
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      tabdb - in, hold
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
void rs_sysi_inserttabdb(rs_sysi_t* sysi, void* tabdb)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_tabdb = tabdb;
}

/*##**********************************************************************\
 *
 *              rs_sysi_tabdb
 *
 *
 *
 * Parameters :
 *
 *      sysi - in
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void* rs_sysi_tabdb(rs_sysi_t* sysi)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        return(sysi->si_tabdb);
}

/*##**********************************************************************\
 *
 *              rs_sysi_insertsqlinfo
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      sqli - in, hold
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
void rs_sysi_insertsqlinfo(rs_sysi_t* sysi, rs_sqlinfo_t* sqli)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_sqli = sqli;
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 *
 *              rs_sysi_sqlinfo
 *
 *
 *
 * Parameters :
 *
 *      sysi - in
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_sqlinfo_t* rs_sysi_sqlinfo(rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return(NULL);
        } else {
            CHK_SYSI(sysi);
            return(sysi->si_sqli);
        }
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *              rs_sysi_insertrslinksemarray
 *
 * Inserts a semaphore reference array to the sysi object. The semaphore use
 * used to protect different link counters in rs objects.
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      rslinksemarray - in, hold
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
void rs_sysi_insertrslinksemarray(rs_sysi_t* sysi, SsSemT** rslinksemarray)
{
        ss_dassert(sysi != NULL);
        ss_dassert(rslinksemarray != NULL);
        CHK_SYSI(sysi);

        sysi->si_rslinksemarray = rslinksemarray;
}

/*##**********************************************************************\
 *
 *              rs_sysi_rslinksem_enter
 *
 * Enters to rs link mutex.
 *
 * Parameters :
 *
 *      sysi - in
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
void rs_sysi_rslinksem_enter(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            SsQsemEnter(sysi->si_linksem);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_rslinksem_exit
 *
 * Enters to rs link mutex.
 *
 * Parameters :
 *
 *      sysi - in
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
void rs_sysi_rslinksem_exit(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            SsQsemExit(sysi->si_linksem);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getrslinksem
 *
 * Returns randomly selected link mutex semaphore from the array.
 *
 * Parameters :
 *
 *              sysi -
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SsSemT* rs_sysi_getrslinksem(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        if (sysi->si_rslinksemarray != NULL) {
            return(sysi->si_rslinksemarray[rand() % RS_SYSI_MAXLINKSEM]);
        } else {
            return(sysi->si_linksem);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_settask
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      task -
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
void rs_sysi_settask_ex(
        rs_sysi_t* sysi,
        void* task)
{
        ss_dprintf_1(("rs_sysi_settask:sysi=%ld, task=%ld\n", (long)sysi, (long)task));

        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (task == NULL) {
                /* TPR 453028: Prevent concurrent access with rs_sysi_lockwakeup */
                SsSemEnter(sysi->si_sem);
            }
            sysi->si_task = task;
            ss_debug(SsMemTrcFreeCallStk(sysi->si_task_callstack));
            ss_debug(sysi->si_task_callstack = SsMemTrcCopyCallStk());
            if (task == NULL) {
                sysi->si_stepcountptr = NULL;
                SsSemExit(sysi->si_sem);
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_removetaskif
 *
 * Removes the task old_task from sysi if it is set as the current task.
 * Otherwise does nothing.
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      old_task -
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
void rs_sysi_removetaskif(
        rs_sysi_t* sysi,
        void* old_task)
{
        ss_dprintf_1(("rs_sysi_removetaskif:sysi=%ld, task=%ld\n", (long)sysi, (long)old_task));

        if (sysi != NULL) {
            CHK_SYSI(sysi);
            SsSemEnter(sysi->si_sem);
            if (sysi->si_task == old_task) {
                sysi->si_task = NULL;
                sysi->si_stepcountptr = NULL;
            }
            SsSemExit(sysi->si_sem);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_task
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
void* rs_sysi_task(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            void* task;

            CHK_SYSI(sysi);

            task = sysi->si_task;

            ss_dprintf_1(("rs_sysi_task:sysi=%ld, task=%ld\n", (long)sysi, (long)task));

            return(task);

        } else {
            ss_dprintf_1(("rs_sysi_task:sysi=NULL, task=NULL\n"));
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setstartlockwaitfun
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      waitfun -
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
void rs_sysi_setstartlockwaitfun(
        rs_sysi_t* sysi,
        void (*waitfun)(void* task),
        void (*waitfun_tmo)(void* task, long timeout_ms))
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_startlockwaitfp = waitfun;
            sysi->si_startlockwait_tmofp = waitfun_tmo;
        }
}

/*##**********************************************************************\
 *
 *		rs_sysi_setlockwaitfun
 *
 * Sets function to wait for a lock.
 *
 * Parameters :
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_sysi_setlockwaitfun(
        rs_sysi_t* sysi,
        bool (*waitfun)(void* task))
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_lockwaitfp = waitfun;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setlockwakeupfun
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      wakeupfun -
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
void rs_sysi_setlockwakeupfun(
        rs_sysi_t* sysi,
        void (*wakeupfun)(void* task))
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_lockwakeupfp = wakeupfun;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_startlockwait
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
void rs_sysi_startlockwait(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(sysi->si_startlockwaitfp != NULL);
            SsSemEnter(sysi->si_sem);
            if (sysi->si_task != NULL) {
                (*sysi->si_startlockwaitfp)(sysi->si_task);
            }
            SsSemExit(sysi->si_sem);
        }
}

void rs_sysi_startlockwaitwithtimeout(rs_sysi_t* sysi, long timeout)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(sysi->si_startlockwait_tmofp != NULL);
            SsSemEnter(sysi->si_sem);
            if (sysi->si_task != NULL) {
                (*sysi->si_startlockwait_tmofp)(sysi->si_task, timeout);
            }
            SsSemExit(sysi->si_sem);
        }
}


/*##**********************************************************************\
 *
 *              rs_sysi_lockwakeup
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
void rs_sysi_lockwakeup(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(sysi->si_lockwakeupfp != NULL);
            SsSemEnter(sysi->si_sem);
            if (sysi->si_task != NULL) {
                (*sysi->si_lockwakeupfp)(sysi->si_task);
            }
            SsSemExit(sysi->si_sem);
        }
}

/*##**********************************************************************\
 *
 *		rs_sysi_lockwait
 *
 * Does actual lock wait if lock wait function pointer is set.
 *
 * Parameters :
 *
 *		sysi -
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
bool rs_sysi_lockwait(rs_sysi_t* sysi)
{
        bool waitp;

        CHK_SYSI(sysi);
        ss_dassert(sysi->si_task != NULL);

        if (sysi->si_lockwaitfp) {
            waitp = (*sysi->si_lockwaitfp)(sysi->si_task);
            return(waitp);
        } else {
            return(FALSE);
        }
}

void rs_sysi_eventwaitwithtimeout(
        rs_sysi_t* sysi,
        int event,
        long timeout,
        void (*timeout_cbfun)(void*),
        void* timeout_cbctx)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(sysi->si_eventwaitwithtimeoutfp != NULL);
            SsSemEnter(sysi->si_sem);
            if (sysi->si_task != NULL) {
                (*sysi->si_eventwaitwithtimeoutfp)(sysi->si_task,event,timeout,timeout_cbfun,timeout_cbctx);
            }
            SsSemExit(sysi->si_sem);
        }
}

void rs_sysi_seteventwaitwithtimeoutfun(
        rs_sysi_t* sysi,
        void (*waitfun)(void* task, int event, long timeout, void (*_cbfun)(void*), void* timeout_cbctx))
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_eventwaitwithtimeoutfp = waitfun;
        }
}


/*##**********************************************************************\
 *
 *              rs_sysi_setsignalfun
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      signalfun - in, hold
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
void rs_sysi_setsignalfun(
        rs_sysi_t* sysi,
        void (*signalfun)(int userid, rs_signaltype_t signaltype))
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_signalfp = signalfun;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_signal
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      event -
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
void rs_sysi_signal(
        rs_sysi_t* sysi,
        rs_signaltype_t signaltype)
{
        if (sysi != NULL &&
            (signaltype == RS_SIGNAL_DEFSCHEMACHANGE ||
             signaltype == RS_SIGNAL_DEFCATALOGCHANGE))
        {
            sysi->si_tc_changes = TRUE;
        }
        if (sysi != NULL && sysi->si_signalfp != NULL) {
            CHK_SYSI(sysi);
            (*sysi->si_signalfp)(sysi->si_userid, signaltype);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setstarttaskfun
 *
 *
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      taskstart - in, hold
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
void rs_sysi_setstarttaskfun(
        rs_sysi_t* sysi,
        void (*taskstart)(
                char* task_name,
                su_task_fun_t task_fun,
                void* task_data,
                rs_sysi_t* task_cd))
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        sysi->si_taskstartfp = taskstart;
}

/*##**********************************************************************\
 *
 *		rs_sysi_starttask
 *
 *
 *
 * Parameters :
 *
 *		sysi -
 *
 *
 *		taskfun -
 *
 *
 *		taskdata -
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
bool rs_sysi_starttask(
        rs_sysi_t* sysi,
        char* task_name,
        su_task_fun_t task_fun,
        void* task_data,
        rs_sysi_t* task_cd)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_taskstartfp) {
                (*sysi->si_taskstartfp)(task_name, task_fun, task_data, task_cd);
                return(TRUE);
            }
        }
        return(FALSE);
}

/*##**********************************************************************\
 *
 *              rs_sysi_setdateformat
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      format -
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
void rs_sysi_setdateformat(
        rs_sysi_t* sysi,
        char* format)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_dateformat != NULL) {
                SsMemFree(sysi->si_dateformat);
            }
            if (format != NULL) {
                sysi->si_dateformat = SsMemStrdup(format);
            } else {
                sysi->si_dateformat = NULL;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_settimeformat
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      format -
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
void rs_sysi_settimeformat(
        rs_sysi_t* sysi,
        char* format)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_timeformat != NULL) {
                SsMemFree(sysi->si_timeformat);
            }
            if (format != NULL) {
                sysi->si_timeformat = SsMemStrdup(format);
            } else {
                sysi->si_timeformat = NULL;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_settimestampformat
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      format -
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
void rs_sysi_settimestampformat(
        rs_sysi_t* sysi,
        char* format)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_timestampformat != NULL) {
                SsMemFree(sysi->si_timestampformat);
            }
            if (format != NULL) {
                sysi->si_timestampformat = SsMemStrdup(format);
            } else {
                sysi->si_timestampformat = NULL;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_dateformat
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
char* rs_sysi_dateformat(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_dateformat);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_timeformat
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
char* rs_sysi_timeformat(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_timeformat);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_timestampformat
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
char* rs_sysi_timestampformat(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_timestampformat);
        } else {
            return(NULL);
        }
}


/*##**********************************************************************\
 *
 *              rs_sysi_setxsmgr
 *
 * Sets pointer to eXternal Sort ManaGeR into sysinfo
 *
 * Parameters :
 *
 *      sysi - use
 *
 *
 *      xsmgr - hold
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
void rs_sysi_setxsmgr(
        rs_sysi_t* sysi,
        void* xsmgr)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_xsmgr = xsmgr;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_xsmgr
 *
 * Returns pointer to eXternal Sort ManaGeR
 *
 * Parameters :
 *
 *      sysi - in
 *
 *
 * Return value - ref :
 *      pointer to xsmgr
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void* rs_sysi_xsmgr(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return (sysi->si_xsmgr);
        }
        return (NULL);
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsqlinfolevel
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      level -
 *
 *
 *      fname -
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
void rs_sysi_setsqlinfolevel(rs_sysi_t* sysi, int level, char* fname)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (level > RS_SQLI_MAXINFOLEVEL) {
                level = RS_SQLI_MAXINFOLEVEL;
            } else if (level < -1) {
                level = -1;
            }
            sysi->si_sqlilevel = level;
            if (level == 0) {
                if (sysi->si_sqlifp != NULL) {
                    rs_sqli_closeinfofile(sysi->si_sqli, sysi->si_sqlifp);
                    sysi->si_sqlifp = NULL;
                }
                sysi->si_stmttabnameinfo = NULL;
            }
            if (level > 0 && fname != NULL && fname[0] != '\0') {
                if (sysi->si_sqlifname != NULL) {
                    SsMemFree(sysi->si_sqlifname);
                }
                sysi->si_sqlifname = SsMemStrdup(fname);
            }
            sysi->si_tc_changes = TRUE;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsortarraysize
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      size -
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
void rs_sysi_setsortarraysize(rs_sysi_t* sysi, int size)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_sortarraysize != size) {
                sysi->si_sortarraysize = size;
                sysi->si_tc_changes = TRUE;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setconvertorstounions
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      count -
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
void rs_sysi_setconvertorstounions(rs_sysi_t* sysi, int count)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_convertorstounions != count) {
                sysi->si_convertorstounions = count;
                sysi->si_tc_changes = TRUE;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsortedgroupby
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      sortedgroupby -
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
void rs_sysi_setsortedgroupby(rs_sysi_t* sysi, int sortedgroupby)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_sortedgroupby != sortedgroupby) {
                sysi->si_sortedgroupby = sortedgroupby;
                sysi->si_tc_changes = TRUE;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setoptimizerows
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      rows -
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
void rs_sysi_setoptimizerows(rs_sysi_t* sysi, int rows)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_optimizerows != rows) {
                sysi->si_optimizerows = rows;
                sysi->si_tc_changes = TRUE;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setoptimizerows
 *
 * Enables or disbales simple optimizer rules. Value -1 means use default.
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      value -
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
void rs_sysi_setsimpleoptimizerrules(rs_sysi_t* sysi, int value)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_simpleoptimizerrules != value) {
                sysi->si_simpleoptimizerrules = value;
                sysi->si_tc_changes = TRUE;
            }
        }
}

int rs_sysi_getsimpleoptimizerrules(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        return sysi->si_simpleoptimizerrules;
}

/*##**********************************************************************\
 *
 *              rs_sysi_sqlinfolevel
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      sqlp -
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
int rs_sysi_sqlinfolevel(rs_sysi_t* sysi, bool sqlp)
{
        CHK_SYSI(sysi);

        if (sysi->si_sqlilevel != -1) {
            return(sysi->si_sqlilevel);
        } else {
            return(rs_sqli_getinfolevel(sysi->si_sqli, sqlp));
        }
}

bool rs_sysi_useindexcursorreset(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(rs_sqli_getuseindexcursorreset(sysi->si_sqli));
}

bool rs_sysi_uselateindexplan(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(rs_sqli_getuselateindexplan(sysi->si_sqli));
}

/*##**********************************************************************\
 *
 *              rs_sysi_sortarraysize
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
int rs_sysi_sortarraysize(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        if (sysi->si_sortarraysize != -1) {
            return(sysi->si_sortarraysize);
        } else {
            return(rs_sqli_getsortarraysize(sysi->si_sqli));
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_convertorstounions
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
int rs_sysi_convertorstounions(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        if (sysi->si_convertorstounions != -1) {
            return(sysi->si_convertorstounions);
        } else {
            return(rs_sqli_getconvertorstounions(sysi->si_sqli));
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_allowduplicateindex
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
bool rs_sysi_allowduplicateindex(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(rs_sqli_getallowduplicateindex(sysi->si_sqli));
}

/*##**********************************************************************\
 *
 *              rs_sysi_sortedgroupby
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
int rs_sysi_sortedgroupby(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        if (sysi->si_sortedgroupby != -1) {
            return(sysi->si_sortedgroupby);
        } else {
            return(rs_sqli_getsortedgroupby(sysi->si_sqli));
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_simpleoptimizerrules
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
bool rs_sysi_simpleoptimizerrules(rs_sysi_t* sysi, double ntuples_in_table)
{
        CHK_SYSI(sysi);

        if (sysi->si_simpleoptimizerrules != -1) {
            return(sysi->si_simpleoptimizerrules);
        } else {
            return(rs_sqli_usesimpleoptimizerrules(sysi->si_sqli, ntuples_in_table));
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setstmttabnameinfo
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      rbt -
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
void rs_sysi_setstmttabnameinfo(
        rs_sysi_t* sysi,
        su_rbt_t* rbt)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_stmttabnameinfo = rbt;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_addstmttabnameinfo
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      en -
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
void rs_sysi_addstmttabnameinfo(
        rs_sysi_t* sysi,
        rs_entname_t* en)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_stmttabnameinfo != NULL &&
                su_rbt_search(sysi->si_stmttabnameinfo, en) == NULL) {
                su_rbt_insert(sysi->si_stmttabnameinfo, rs_entname_copy(en));
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_printsqlinfo
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      level -
 *
 *
 *      str -
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
void rs_sysi_printsqlinfo(rs_sysi_t* sysi, int level, char* str)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_disableprintsqlinfo == 0) {
                rs_sysi_printsqlinfo_force(sysi, level, str);
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_printsqlinfo_force
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      level -
 *
 *
 *      str -
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
void rs_sysi_printsqlinfo_force(rs_sysi_t* sysi, int level, char* str)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_sqlifp == NULL) {
                sysi->si_sqlifp = rs_sqli_openinfofile(
                                    sysi->si_sqli,
                                    sysi->si_sqlifname);
            }
            rs_sqli_printinfo(sysi->si_sqli, sysi->si_sqlifp, level, str);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setdisableprintsqlinfo
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      disablep -
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
void rs_sysi_setdisableprintsqlinfo(
        rs_sysi_t* sysi,
        bool disablep)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (disablep) {
                sysi->si_disableprintsqlinfo++;
            } else {
                ss_dassert(sysi->si_disableprintsqlinfo > 0);
                sysi->si_disableprintsqlinfo--;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_blobrefbuf
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
void* rs_sysi_blobrefbuf(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        return (sysi->si_blobrefbuf);
}

/*##**********************************************************************\
 *
 *              rs_sysi_setblobrefbuf
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      blobrefbuf -
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
void rs_sysi_setblobrefbuf(rs_sysi_t* sysi, void* blobrefbuf)
{
        CHK_SYSI(sysi);
        sysi->si_blobrefbuf = blobrefbuf;
}


/*##**********************************************************************\
 *
 *              rs_sysi_stmtmaxtime
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
long rs_sysi_stmtmaxtime(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_stmtmaxtime);
        } else {
            return(0);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setstmtmaxtime
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      maxtime -
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
void rs_sysi_setstmtmaxtime(
        rs_sysi_t* sysi,
        long maxtime)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_stmtmaxtime != maxtime) {
                sysi->si_stmtmaxtime = maxtime;
                sysi->si_tc_changes = TRUE;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setlogauditinfo
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      b -
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
void rs_sysi_setlogauditinfo(
        rs_sysi_t* sysi,
        bool b)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_logauditinfo = b;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setevents
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      events -
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
void rs_sysi_setevents(
        rs_sysi_t* sysi,
        void* events)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_events = events;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_events
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
void* rs_sysi_events(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_events);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setproccache
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      proccache -
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
void rs_sysi_setproccache(
        rs_sysi_t* sysi,
        void* proccache)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_proccache = proccache;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_proccache
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
void* rs_sysi_proccache(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_proccache);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_settrigcache
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      trigcache -
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
void rs_sysi_settrigcache(
        rs_sysi_t* sysi,
        void* trigcache)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_trigcache = trigcache;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_trigcache
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
void* rs_sysi_trigcache(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_trigcache);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_settrigexecfun
 *
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_sysi_settrigexecfun(
        rs_sysi_t* sysi,
        int (*trigexec)(
            void* cd,
            void* trans,
            rs_entname_t* trigname,
            char* trigstr,
            rs_ttype_t* ttype,
            rs_tval_t* old_tval,
            rs_tval_t* new_tval,
            void** p_ctx,
            rs_err_t** p_errh))
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_trigexec = trigexec;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_trigexec
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      trans -
 *
 *
 *      trigstr -
 *
 *
 *      ttype -
 *
 *
 *      old_tval -
 *
 *
 *      new_tval -
 *
 *
 *      p_ctx -
 *
 *
 *      p_errh -
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
int rs_sysi_trigexec(
        rs_sysi_t* sysi,
        void* trans,
        rs_entname_t* trigname,
        char* trigstr,
        rs_ttype_t* ttype,
        rs_tval_t* old_tval,
        rs_tval_t* new_tval,
        void** p_ctx,
        rs_err_t** p_errh)
{
        if (sysi != NULL) {
            int rc;
            CHK_SYSI(sysi);
            if (sysi->si_trigexec != 0) {
                rc = (*sysi->si_trigexec)(
                    sysi,
                    trans,
                    trigname,
                    trigstr,
                    ttype,
                    old_tval,
                    new_tval,
                    p_ctx,
                    p_errh);
                return(rc);
            }
        }
        if (p_errh != NULL) {
            *p_errh = NULL;
        }
        return(0);
}

/*##**********************************************************************\
 *
 *              rs_sysi_settrigdonefun
 *
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_sysi_settrigdonefun(
        rs_sysi_t* sysi,
        void (*trigdone)(void* ctx))
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_trigdone = trigdone;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_trigdone
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      ctx -
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
void rs_sysi_trigdone(
        rs_sysi_t* sysi,
        void* ctx)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_trigdone != 0) {
                (*sysi->si_trigdone)(ctx);
            }
        }
}

bool rs_sysi_trigpush(
        rs_sysi_t* sysi,
        rs_entname_t* trigname)
{
        uint nelems;

        CHK_SYSI(sysi);

        nelems = su_pa_nelems(sysi->si_trignamestack);

        if (nelems >= rs_sqli_getmaxnestedtrig(sysi->si_sqli)) {
            return(FALSE);
        }
        su_pa_insert(sysi->si_trignamestack, trigname);
        return(TRUE);
}

void rs_sysi_trigpop(
        rs_sysi_t* sysi)
{
        uint nelems;

        CHK_SYSI(sysi);

        nelems = su_pa_nelems(sysi->si_trignamestack);
        ss_dassert(nelems > 0);

        su_pa_remove(sysi->si_trignamestack, nelems - 1);
}

int rs_sysi_trigcount(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(su_pa_nelems(sysi->si_trignamestack));
}

char* rs_sysi_trigname(
        rs_sysi_t* sysi,
        int index)
{
        CHK_SYSI(sysi);

        if (su_pa_indexinuse(sysi->si_trignamestack, index)) {
            rs_entname_t* en;
            en = su_pa_getdata(sysi->si_trignamestack, index);
            return(rs_entname_getname(en));
        } else {
            return(NULL);
        }
}

char* rs_sysi_trigschema(
        rs_sysi_t* sysi,
        int index)
{
        CHK_SYSI(sysi);

        if (su_pa_indexinuse(sysi->si_trignamestack, index)) {
            rs_entname_t* en;
            en = su_pa_getdata(sysi->si_trignamestack, index);
            return(rs_entname_getschema(en));
        } else {
            return(NULL);
        }
}

bool rs_sysi_procpush(
        rs_sysi_t* sysi,
        rs_entname_t* procname)
{
        uint nelems;

        CHK_SYSI(sysi);

        nelems = su_pa_nelems(sysi->si_procnamestack);

        if (nelems >= rs_sqli_getmaxnestedproc(sysi->si_sqli)) {
            return(FALSE);
        }
        su_pa_insert(sysi->si_procnamestack, procname);
        return(TRUE);
}

void rs_sysi_procpop(
        rs_sysi_t* sysi)
{
        uint nelems;

        CHK_SYSI(sysi);

        nelems = su_pa_nelems(sysi->si_procnamestack);
        ss_dassert(nelems > 0);

        su_pa_remove(sysi->si_procnamestack, nelems - 1);
}

int rs_sysi_proccount(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(su_pa_nelems(sysi->si_procnamestack));
}

char* rs_sysi_procname(
        rs_sysi_t* sysi,
        int index)
{
        CHK_SYSI(sysi);

        if (su_pa_indexinuse(sysi->si_procnamestack, index)) {
            rs_entname_t* en;
            en = su_pa_getdata(sysi->si_procnamestack, index);
            return(rs_entname_getname(en));
        } else {
            return(NULL);
        }
}

char* rs_sysi_procschema(
        rs_sysi_t* sysi,
        int index)
{
        CHK_SYSI(sysi);

        if (su_pa_indexinuse(sysi->si_procnamestack, index)) {
            rs_entname_t* en;
            en = su_pa_getdata(sysi->si_procnamestack, index);
            return(rs_entname_getschema(en));
        } else {
            return(NULL);
        }
}

void rs_sysi_addseqid(
        rs_sysi_t* sysi,
        long seqid)
{
        if (sysi != NULL && sysi->si_seqidlist != NULL) {
            CHK_SYSI(sysi);
            su_list_insertlast(sysi->si_seqidlist, (void*)seqid);
        }
}

void rs_sysi_setseqidlist(
        rs_sysi_t* sysi,
        su_list_t* seqidlist)
{
        CHK_SYSI(sysi);
        ss_dassert(seqidlist == NULL || sysi->si_seqidlist == NULL);

        sysi->si_seqidlist = seqidlist;
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 *
 *              rs_sysi_gettrend
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
rs_trend_t* rs_sysi_gettrend(
        rs_sysi_t* sysi)
{
        ss_bassert(sysi != NULL);

        return(_RS_SYSI_GETTREND(sysi));
}

#endif /* SS_DEBUG */

#ifdef SS_DEBUG

void rs_sysi_addmergewrites(
        rs_sysi_t* sysi,
        uint mergewrites)
{
        CHK_SYSI(sysi);

        _RS_SYSI_ADDMERGEWRITES(sysi, mergewrites);
}

ulong rs_sysi_getmergewrites(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(_RS_SYSI_GETMERGEWRITES(sysi));
}

void rs_sysi_setflag(
        rs_sysi_t* cd,
        int flag)
{
        CHK_SYSI(cd);

        _RS_SYSI_SETFLAG(cd, flag);
}

void rs_sysi_clearflag(
        rs_sysi_t* cd,
        int flag)
{
        CHK_SYSI(cd);

        _RS_SYSI_CLEARFLAG(cd, flag);
}

bool rs_sysi_testflag(
        rs_sysi_t* cd,
        int flag)
{
        CHK_SYSI(cd);

        return(_RS_SYSI_TESTFLAG(cd, flag));
}

#endif /* SS_DEBUG */

void rs_sysi_clearmergewrites(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        sysi->si_mergewrites = 0;
}

/*##**********************************************************************\
 *
 *              rs_sysi_setignoretimeout
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      ignorep -
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
void rs_sysi_setignoretimeout(
        rs_sysi_t* sysi,
        bool ignorep)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (ignorep) {
                sysi->si_ignoretimeout++;
            } else {
                ss_dassert(sysi->si_ignoretimeout > 0);
                sysi->si_ignoretimeout--;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_ignoretimeout
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
bool rs_sysi_ignoretimeout(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_ignoretimeout);
        } else {
            return(FALSE);
        }
}


#ifdef SS_SYNC

/*##**********************************************************************\
 *
 *              rs_sysi_setsynccatalog
 *
 * Sets the default catalog for sync operations. Sync parameters are saved
 * separately for each catalog.
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      catalog -
 *
 *
 * Return value :
 *      TRUE  if this is an old catalog.
 *      FALSE if this is a new catalog.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_sysi_setsynccatalog(
        rs_sysi_t* sysi,
        char* catalog)
{
        bool oldcatalog = TRUE;

        ss_dassert(catalog != NULL && catalog[0] != '\0');

        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dprintf_1(("rs_sysi_setsynccatalog:sysi->si_cursyncinfo->ssi_catalog=%s, catalog=%s\n",
                sysi->si_cursyncinfo->ssi_catalog, catalog));
            if (strcmp(sysi->si_cursyncinfo->ssi_catalog, catalog) != 0) {
                su_rbt_node_t* n;
                ss_dprintf_2(("Changing to a different catalog, try to find that catalog.\n"));
                n = su_rbt_search(sysi->si_syncinforbt, catalog);
                if (n != NULL) {
                    ss_dprintf_2(("Found this catalog\n"));
                    sysi->si_cursyncinfo = su_rbtnode_getkey(n);
                } else {
                    bool succp;
                    ss_dprintf_2(("Add new catalog info\n"));
                    sysi->si_cursyncinfo = sysi_syncinfo_init(catalog);
                    succp = su_rbt_insert(sysi->si_syncinforbt, sysi->si_cursyncinfo);
                    ss_dassert(succp);
                    oldcatalog = FALSE;
                }
            }
            ss_dassert(strcmp(sysi->si_cursyncinfo->ssi_catalog, catalog) == 0);
        }
        return(oldcatalog);
}

/*##**********************************************************************\
 *
 *              rs_sysi_dropsynccatalog
 *
 * Drops sync catalog info from memory cache.
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      catalog -
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
bool rs_sysi_dropsynccatalog(
        rs_sysi_t* sysi,
        char* catalog)
{
        ss_dassert(catalog != NULL && catalog[0] != '\0');

        if (sysi != NULL) {
            su_rbt_node_t* n;
            CHK_SYSI(sysi);
            ss_dprintf_2(("rs_sysi_dropsynccatalog:%s\n", catalog));
            n = su_rbt_search(sysi->si_syncinforbt, catalog);
            if (n != NULL) {
                /* Found this catalog. */
                su_rbt_delete(sysi->si_syncinforbt, n);
            }
        }
        return(FALSE);
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsyncmaster
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      valuep -
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
void rs_sysi_setsyncmaster(
        rs_sysi_t* sysi,
        bool valuep)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_cursyncinfo->ssi_issyncmaster = valuep;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsyncreplica
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      valuep -
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
void rs_sysi_setsyncreplica(
        rs_sysi_t* sysi,
        bool valuep)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_cursyncinfo->ssi_issyncreplica = valuep;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsyncnode
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      syncnode -
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
void rs_sysi_setsyncnode(
        rs_sysi_t* sysi,
        char* syncnode,
        SsTimeT nodetime)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_cursyncinfo->ssi_syncnode != NULL) {
                SsMemFree(sysi->si_cursyncinfo->ssi_syncnode);
            }
            if (syncnode != NULL) {
                syncnode = SsMemStrdup(syncnode);
            }
            sysi->si_cursyncinfo->ssi_syncnode = syncnode;
            sysi->si_cursyncinfo->ssi_nodetime = nodetime;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_issyncmaster
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
bool rs_sysi_issyncmaster(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_cursyncinfo->ssi_issyncmaster);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_issyncreplica
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
bool rs_sysi_issyncreplica(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_cursyncinfo->ssi_issyncreplica);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getsyncnode
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
char* rs_sysi_getsyncnode(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return (sysi->si_cursyncinfo->ssi_syncnode);
        } else {
            return (NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getnodetime
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
SsTimeT rs_sysi_getnodetime(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return (sysi->si_cursyncinfo->ssi_nodetime);
        } else {
            return (0L);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsyncmasterid
 *
 * Sets master id
 *
 * Parameters :
 *
 *      sysi - use
 *              sysinfo
 *
 *      syncmasterid - in
 *              master id
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_sysi_setsyncmasterid(
        rs_sysi_t* sysi,
        long syncmasterid)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_cursyncinfo->ssi_syncmasterid = syncmasterid;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getsyncmasterid
 *
 * Gets sync master id.
 *
 * Parameters :
 *
 *      sysi - in, use
 *              sysinfo
 *
 * Return value :
 *      master id
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long rs_sysi_getsyncmasterid(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return (sysi->si_cursyncinfo->ssi_syncmasterid);
        }
        return (-1L);
}

/* VOYAGER BEGIN */


/*##**********************************************************************\
 *
 *              rs_sysi_setsyncsavemasterid
 *
 * Sets master id
 *
 * Parameters :
 *
 *      sysi - use
 *              sysinfo
 *
 *      syncmasterid - in
 *              master id
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_sysi_setsyncsavemasterid(
        rs_sysi_t* sysi,
        long syncmasterid)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_syncsavemasterid = syncmasterid;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getsyncsavemasterid
 *
 * Gets sync master id.
 *
 * Parameters :
 *
 *      sysi - in, use
 *              sysinfo
 *
 * Return value :
 *      master id
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long rs_sysi_getsyncsavemasterid(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return (sysi->si_syncsavemasterid);
        }
        return (-1);
}


/*##**********************************************************************\
 *
 *              rs_sysi_setsync_propagatereplicaid
 *
 * Sets sync replicaid which is now propagating
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      replicaid -
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
void rs_sysi_setsync_propagatereplicaid(
        rs_sysi_t* sysi,
        long replicaid)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_replicaid_onpropagate = replicaid;
        }
}


/*##**********************************************************************\
 *
 *              rs_sysi_getsync_propagatereplicaid
 *
 * Gets sync replicaid which is now propagating
 *
 * Parameters :
 *
 *      sysi -
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
long rs_sysi_getsync_propagatereplicaid(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_replicaid_onpropagate);
        }
        return (-1);
}

bool rs_sysi_subscribe_write(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        return(sysi->si_subscribewrite);
}

void rs_sysi_setsubscribe_write(
        rs_sysi_t* sysi,
        bool subscribewrite)
{
        CHK_SYSI(sysi);
        sysi->si_subscribewrite = subscribewrite;
}

bool rs_sysi_usehurc(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        return(sysi->si_usehurc || sysi->si_usehurc_force);
}

void rs_sysi_setusehurc(
        rs_sysi_t* sysi,
        bool usehurc)
{
        CHK_SYSI(sysi);
        sysi->si_usehurc = usehurc;
}

void rs_sysi_setusehurc_force(
        rs_sysi_t* sysi,
        bool usehurc)
{
        CHK_SYSI(sysi);
        sysi->si_usehurc_force = usehurc;
}

/*##**********************************************************************\
 *
 *              rs_sysi_setlocalsyncid
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      syncid -
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
void rs_sysi_setlocalsyncid(
        rs_sysi_t* sysi,
        long syncid)
{
        CHK_SYSI(sysi);

        ss_bassert(syncid != 1L);
        sysi->si_cursyncinfo->ssi_syncid = syncid << RS_FIRST_SYNCID_BIT;
        ss_dassert(sysi->si_cursyncinfo->ssi_syncid >= 128);
}

/*##**********************************************************************\
 *
 *              rs_sysi_getlocalsyncid
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
long rs_sysi_getlocalsyncid(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return (sysi->si_cursyncinfo->ssi_syncid);
}

/* VOYAGER END */


/*##**********************************************************************\
 *
 *              rs_sysi_getmsgnamelocklist
 *
 * Gets sync messagename locklist
 *
 * Parameters :
 *
 *      sysi -
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
su_list_t* rs_sysi_getmsgnamelocklist(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        return(sysi->si_cursyncinfo->ssi_msgnamelocklist);
}

char* rs_sysi_getsynccatalog(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        return(sysi->si_cursyncinfo->ssi_catalog);
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsyncmsg
 *
 * Sets sync message object to sysi.
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      syncmsg -
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
void rs_sysi_setsyncmsgholder(
        rs_sysi_t* sysi,
        void* syncmsg_rbt)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(sysi->si_cursyncinfo->ssi_syncmsg_rbt == NULL || syncmsg_rbt == NULL);
            sysi->si_cursyncinfo->ssi_syncmsg_rbt = syncmsg_rbt;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getsyncmsg
 *
 * Returns sync message obejct.
 *
 * Parameters :
 *
 *      sysi -
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
void* rs_sysi_getsyncmsgholder(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return (sysi->si_cursyncinfo->ssi_syncmsg_rbt);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setbboard
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      bboard -
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
void rs_sysi_setbboard(rs_sysi_t* sysi, rs_bboard_t* bboard)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(sysi->si_cursyncinfo->ssi_bboard == NULL);
            sysi->si_cursyncinfo->ssi_bboard = bboard;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getbboard
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
rs_bboard_t* rs_sysi_getbboard(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return (sysi->si_cursyncinfo->ssi_bboard);
        } else {
            return NULL;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setipub
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      ipub -
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
void rs_sysi_setipub(rs_sysi_t* sysi, void* ipub)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_cursyncinfo->ssi_ipub = ipub;
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getipub
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
void* rs_sysi_getipub(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_cursyncinfo->ssi_ipub);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setsyncusername
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      username -
 *
 *
 *      password -
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
void rs_sysi_setsyncusername(
        rs_sysi_t* sysi,
        char* username,
        va_t* password)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_cursyncinfo->ssi_syncusername != NULL) {
                SsMemFree(sysi->si_cursyncinfo->ssi_syncusername);
            }
            if (username == NULL) {
                ss_dassert(password == NULL);
                sysi->si_cursyncinfo->ssi_syncusername = NULL;
                dynva_free(&sysi->si_cursyncinfo->ssi_syncpassword);
            } else {
                ss_dassert(password != NULL);
                sysi->si_cursyncinfo->ssi_syncusername = SsMemStrdup(username);
                dynva_setva(&sysi->si_cursyncinfo->ssi_syncpassword, password);
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_syncusername
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      p_username -
 *
 *
 *      p_password -
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
bool rs_sysi_syncusername(
        rs_sysi_t* sysi,
        char** p_username,
        va_t** p_password)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_cursyncinfo->ssi_syncusername == NULL) {
                return(FALSE);
            } else {
                if (p_username != NULL) {
                    *p_username = sysi->si_cursyncinfo->ssi_syncusername;
                }
                if (p_password != NULL) {
                    *p_password = sysi->si_cursyncinfo->ssi_syncpassword;
                }
                return(TRUE);
            }
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_setpassword
 *
 *
 *
 * Parameters :
 *
 *      sysi -
 *
 *
 *      password -
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
#ifndef SS_MYSQL
void rs_sysi_setpassword(
        rs_sysi_t* sysi,
        va_t* password)
{
        if (sysi != NULL && password != NULL) {
            CHK_SYSI(sysi);
            dynva_setva(&sysi->si_password, password);
        }
}
#endif /* !SS_MYSQL */


/*##**********************************************************************\
 *
 *              rs_sysi_password
 *
 *
 *
 * Parameters :
 *
 *      sysi -
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
va_t* rs_sysi_password(
        rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return (NULL);
        }
        CHK_SYSI(sysi);
        return (sysi->si_password);
}

void rs_sysi_setpropertystore(
        rs_sysi_t* sysi,
        rs_bboard_t* propertystore)
{
        CHK_SYSI(sysi);
        ss_bassert(sysi->si_cursyncinfo->ssi_propertystore == NULL);

        sysi->si_cursyncinfo->ssi_propertystore = propertystore;
}

rs_bboard_t* rs_sysi_getpropertystore(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_cursyncinfo->ssi_propertystore);
}

void rs_sysi_setdefaultpropagatewherestr(
        rs_sysi_t* sysi,
        char* wherestr)
{
        CHK_SYSI(sysi);

        if (sysi->si_cursyncinfo->ssi_defaultpropagatewherestr != NULL) {
            SsMemFree(sysi->si_cursyncinfo->ssi_defaultpropagatewherestr);
        }
        if (wherestr == NULL) {
            sysi->si_cursyncinfo->ssi_defaultpropagatewherestr = NULL;
        } else {
            sysi->si_cursyncinfo->ssi_defaultpropagatewherestr = SsMemStrdup(wherestr);
        }

}

char* rs_sysi_getdefaultpropagatewherestr(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_cursyncinfo->ssi_defaultpropagatewherestr);
}

#endif /* SS_SYNC */

#ifdef SS_DEBUG

void rs_sysi_setdisablerowspermessage(
        rs_sysi_t* sysi,
        bool disable)
{
        CHK_SYSI(sysi);
        ss_aassert(rs_sysi_testthrid(sysi));

        _RS_SYSI_SETDISABLEROWSPERMESSAGE(sysi, disable);
}

bool rs_sysi_isdisablerowspermessage(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        ss_aassert(rs_sysi_testthrid(sysi));

        return(_RS_SYSI_ISDISABLEROWSPERMESSAGE(sysi));
}

void rs_sysi_setstmttabletypes(
        rs_sysi_t* sysi,
        rs_sysi_tabletype_t type)
{
        CHK_SYSI(sysi);
        ss_aassert(rs_sysi_testthrid(sysi));

        _RS_SYSI_SETSTMTTABLETYPES(sysi, type);
}

void rs_sysi_clearstmttabletypes(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        ss_aassert(rs_sysi_testthrid(sysi));

        _RS_SYSI_CLEARSTMTTABLETYPES(sysi);
}

rs_sysi_tabletype_t rs_sysi_getstmttabletypes(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        ss_aassert(rs_sysi_testthrid(sysi));

        return(_RS_SYSI_GETSTMTTABLETYPES(sysi));
}

void rs_sysi_settrxcardininfo(
        rs_sysi_t* sysi,
        void* trxcardininfo)
{
        CHK_SYSI(sysi);
        ss_aassert(rs_sysi_testthrid(sysi));

        _RS_SYSI_SETTRXCARDININFO(sysi, trxcardininfo);
}

void* rs_sysi_gettrxcardininfo(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(_RS_SYSI_GETTRXCARDININFO(sysi));
}

void rs_sysi_settaskactive(
        rs_sysi_t* sysi,
        bool activep)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
        }
        _RS_SYSI_SETTASKACTIVE(sysi, activep);
}

#endif /* SS_DEBUG */

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)

void rs_sysi_setthrid(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dprintf_1(("rs_sysi_setthrid:cd=%ld, thrid=%d\n", (long)sysi, (int)SsThrGetid()));
            ss_aassert(sysi->si_thrid == 0 || sysi->si_thrid == SsThrGetid());
            sysi->si_thrid = SsThrGetid();
            ss_aassert(sysi->si_thrid != 0);
        }
}

void rs_sysi_clearthrid(rs_sysi_t* sysi)
{
        ss_autotest_or_debug(int thrid;)

        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dprintf_1(("rs_sysi_clearthrid:cd=%ld, thrid=%d\n", (long)sysi, (int)SsThrGetid()));
            ss_autotest_or_debug(thrid = sysi->si_thrid;)
            ss_aassert(thrid == 0 || thrid == SsThrGetid());
            sysi->si_thrid = 0;
        }
}

bool rs_sysi_testthrid(rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_thrid == 0 || sysi->si_thrid == SsThrGetid());
        } else {
            return(TRUE);
        }
}

#endif /* defined(SS_DEBUG) || defined(AUTOTEST_RUN) */

SsSemT* rs_sysi_gettrxsem(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        if (sysi->si_trxsem == NULL) {
            sysi->si_trxsem = SsSemCreateLocal(SS_SEMNUM_DBE_TRX);
        }
        return(sysi->si_trxsem);
}

bool rs_sysi_istaskactive(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);

            return(sysi->si_taskactivep);
        } else {
            return(FALSE);
        }
}

su_rbt_t** rs_sysi_getfunvalues(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_funvaluerbt);
        } else {
            return(FALSE);
        }
}

#ifdef SS_DEBUG
void rs_sysi_setfunvalues(
        rs_sysi_t* sysi,
        su_rbt_t** p_rbt)
{
        CHK_SYSI(sysi);
        _RS_SYSI_SETFUNVALUES(sysi, p_rbt);
}
#endif /* SS_DEBUG */

void rs_sysi_setprio(
        rs_sysi_t* sysi,
        SuTaskPrioT prio)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_prio = prio;
        }
}

#ifdef SS_DEBUG
SuTaskPrioT rs_sysi_getprio(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
        }
        return(_RS_SYSI_GETPRIO(sysi));
}
#endif /* SS_DEBUG */

rs_eventnotifiers_t* rs_sysi_geteventnotifiers(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_eventnotifiers);
}

void rs_sysi_seteventnotifiers(
        rs_sysi_t* sysi,
        rs_eventnotifiers_t* events)
{
        CHK_SYSI(sysi);
        ss_dassert(sysi->si_eventnotifiers == NULL);

        sysi->si_eventnotifiers = events;
}
void rs_sysi_sethsbinfo(
        rs_sysi_t* sysi,
        long info)
{
        CHK_SYSI(sysi);

        sysi->si_hsbinfo = info;
}

long rs_sysi_gethsbinfo(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_hsbinfo);
}

bool rs_sysi_ishsbconfigured(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_ishsbconfigured);
}

void rs_sysi_sethsbconfigured(
        rs_sysi_t* sysi,
        bool isconfigured)
{
        CHK_SYSI(sysi);

        sysi->si_ishsbconfigured = isconfigured;
}

bool rs_sysi_iscancelled(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);

            return(sysi->si_iscancelled);

        } else {
            return(FALSE);
        }
}

void* rs_sysi_tbcur(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_tbcur);
}

void rs_sysi_settbcur(
        rs_sysi_t* sysi,
        void* cur)
{
        CHK_SYSI(sysi);

        sysi->si_tbcur = cur;
}

bool rs_sysi_sqlyieldrequired(
        rs_sysi_t* sysi)
{
        return(rs_sysi_decstepctr(sysi) <= 0);
}

int rs_sysi_getstepctr(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        if (sysi->si_stepcountptr != NULL) {
            ss_dassert(sysi->si_stepctrthrid == (int)SsThrGetid());
            return (*sysi->si_stepcountptr);
        } else {
            return(0);
        }
}

void* rs_sysi_gettbcurlist(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_tbcurlist);
}

void rs_sysi_settbcurlist(
        rs_sysi_t* sysi,
        void* tbcurlist)
{
        CHK_SYSI(sysi);

        sysi->si_tbcurlist = tbcurlist;
}

void rs_sysi_setinsidedbeatomicsection(
        rs_sysi_t* sysi,
        bool insidep)
{
        if (sysi == NULL) {
            return;
        }
        CHK_SYSI(sysi);

        sysi->si_insidedbeatomicsection = insidep;
}

#ifdef SS_DEBUG
bool rs_sysi_isinsidedbeatomicsection(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dprintf_1(("rs_sysi_isinsidedbeatomicsection() returns %d\n",
                          sysi->si_insidedbeatomicsection));
        }
        return(_RS_SYSI_ISINSIDEDBEATOMICSECTION(sysi));
}
#endif /* SS_DEBUG */

void rs_sysi_set_istransactive_fun(
        rs_sysi_t* sysi,
        void* ctx,
        long (*isactivefun)(rs_sysi_t* cd, void* ctx))
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            sysi->si_istransactive_fun = isactivefun;
            sysi->si_istransactive_ctx = ctx;
        }
}

long rs_sysi_istransactive(
        rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return(0L);
        }
        CHK_SYSI(sysi);
        if (sysi->si_istransactive_fun == NULL) {
            return(0L);
        }
        ss_dassert(sysi->si_istransactive_ctx != NULL);

        return(sysi->si_istransactive_fun(sysi, sysi->si_istransactive_ctx));
}

/*##**********************************************************************\
 *
 *              rs_sysi_setdefnode
 *
 *  Sets the DEFAULT node member.
 *
 * Parameters :
 *
 *      sysi - in, use
 *
 *      defnode - in, take
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_sysi_setdefnode(
        rs_sysi_t* sysi,
        rs_defnode_t* defnode)
{
        CHK_SYSI(sysi);

        sysi->si_defaultnode = defnode;

        return;
}

/*##**********************************************************************\
 *
 *              rs_sysi_getdefnode
 *
 *  Returns the DEFAULT node member.
 *
 * Parameters :
 *
 *      sysi - in, use
 *
 * Return value :
 *
 *      defnode - out, ref
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_defnode_t* rs_sysi_getdefnode(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return sysi->si_defaultnode;
}

void rs_sysi_setlocktran(
        rs_sysi_t* sysi,
        void* locktran)
{
        CHK_SYSI(sysi);

        ss_dassert(sysi->si_locktran == NULL);
        sysi->si_locktran = locktran;
        sysi->si_locktran_link = 1;
}

#ifdef SS_DEBUG

void* rs_sysi_getlocktran(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_locktran);

}

#endif /* SS_DEBUG */

void rs_sysi_locktran_link(
        rs_sysi_t* sysi,
        void* locktran __attribute__ ((unused)))
{
        ss_dassert(sysi->si_locktran != NULL);
        sysi->si_locktran_link++;
}

void rs_sysi_locktran_unlink(
        rs_sysi_t* sysi,
        void* locktran __attribute__ ((unused)))
{
        ss_dassert(sysi->si_locktran != NULL);
        ss_dassert(sysi->si_locktran_link > 0);
        ss_dassert(sysi->si_locktran == locktran);

        sysi->si_locktran_link--;
        if (sysi->si_locktran_link == 0) {
            sysi->si_locktran = NULL;
        }
}

void rs_sysi_sethsbwaitmes(
        rs_sysi_t* sysi,
        SsMesT* hsbwaitmes)
{
        CHK_SYSI(sysi);

        sysi->si_hsbwaitmes = hsbwaitmes;
}

SsMesT* rs_sysi_gethsbwaitmes(
        rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return(NULL);
        } else {
            CHK_SYSI(sysi);

            return(sysi->si_hsbwaitmes);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_set_uniquemsgid_fun
 *
 *      Initializes the function pointer for rs_sysi_getnewuniquemsgid
 *      function calls.
 *
 * Parameters :
 *
 *      sysi - in, use
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_sysi_set_uniquemsgid_fun(
        rs_sysi_t* sysi,
        bool (*getnewuniquemsgidfun)(rs_sysi_t*,ss_int8_t*,rs_err_t**) )
{
        if (sysi == NULL) {
            ss_derror;
            return;
        }
        CHK_SYSI(sysi);
        sysi->si_getnewuniquemsgid_fun = getnewuniquemsgidfun;
}

/*##**********************************************************************\
 *
 *              rs_sysi_getnewuniquemsgid
 *
 *      Returns a new sequencer value used in unique message id's.
 *
 * Parameters :
 *
 *      sysi - in, use
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_sysi_getnewuniquemsgid(rs_sysi_t* sysi, ss_int8_t* p_result, rs_err_t** p_errh)
{
        if (sysi == NULL) {
            ss_derror;
            return(0L);
        }
        CHK_SYSI(sysi);
        if (sysi->si_getnewuniquemsgid_fun == NULL) {
            ss_derror;
            return(0L);
        }
        return (sysi->si_getnewuniquemsgid_fun(sysi, p_result, p_errh));
}

/*##**********************************************************************\
 *
 *              rs_sysi_getistransactive_ctx
 *
 *      Returns the context (transaction) of client defined in sysi.
 *
 * Parameters :
 *
 *      sysi - in, use
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void* rs_sysi_getistransactive_ctx( rs_sysi_t* sysi ){
        if (sysi == NULL) {
            ss_derror;
            return(NULL);
        }
        CHK_SYSI(sysi);
        return(sysi->si_istransactive_ctx);
}

void rs_sysi_setdeferredblobunlinklist(
        rs_sysi_t* sysi,
        su_list_t* deferredblobunlinklist)
{
        if (sysi == NULL) {
            return;
        }
        CHK_SYSI(sysi);
        sysi->si_deferredblobunlinklist = deferredblobunlinklist;
}

su_list_t* rs_sysi_getdeferredblobunlinklist(
        rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return (NULL);
        }
        CHK_SYSI(sysi);
        return (sysi->si_deferredblobunlinklist);
}

#ifdef SS_MME
    void* si_mmememctx; /* for MME memory allocator */
#endif /* SS_MME */

#if defined(SS_MME) && defined(SS_DEBUG)

void* rs_sysi_getmmememctx(rs_sysi_t* cd)
{
        CHK_SYSI(cd);
        return (_RS_SYSI_GETMMEMEMCTX_(cd));
}

void rs_sysi_setmmememctx(rs_sysi_t* cd, void* ctx)
{
        CHK_SYSI(cd);
        _RS_SYSI_SETMMEMEMCTX_(cd, ctx);
}
#endif /* SS_MME && SS_DEBUG */

ss_int4_t rs_sysi_sysrand(rs_sysi_t* cd)
{
        CHK_SYSI(cd);
        return (su_rand_long(&cd->si_sysrand));
}

void rs_sysi_usrrandseed(rs_sysi_t* cd, ss_int4_t seed)
{
        CHK_SYSI(cd);
        su_rand_init(&cd->si_usrrand, seed);
}

ss_int4_t rs_sysi_usrrand(rs_sysi_t* cd)
{
        CHK_SYSI(cd);
        return (su_rand_long(&cd->si_usrrand));
}

void rs_sysi_inchsbmergecount(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        sysi->si_hsbmergecount++;
}

int rs_sysi_gethsbmergecount(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_hsbmergecount);
}

void rs_sysi_inchsbstmtcount(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        sysi->si_hsbstmtcount++;
}

int rs_sysi_gethsbstmtcount(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_hsbstmtcount);
}

su_list_t* rs_sysi_gethsbsecactions(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_hsbsecactions);
}

void rs_sysi_sethsbsecactions(
        rs_sysi_t* sysi,
        su_list_t* list)
{
        CHK_SYSI(sysi);

        sysi->si_hsbsecactions = list;
}

bool* rs_sysi_getspnewplanptr(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);

        return sysi->si_spnewplan;
}

bool rs_sysi_isscrollsensitive(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);

        return (sysi->si_isscrollsensitive);
}


void rs_sysi_setscrollsensitive(
        rs_sysi_t*  sysi,
        bool        scrollsensitive)
{
        CHK_SYSI(sysi);

        sysi->si_isscrollsensitive = scrollsensitive;
}

void rs_sysi_settabnewplanptr(
        rs_sysi_t*  sysi,
        bool*       p_newplan)
{
        CHK_SYSI(sysi);

        sysi->si_tabnewplan = p_newplan;
}

bool* rs_sysi_gettabnewplanptr(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);

        return sysi->si_tabnewplan;
}

bool rs_sysi_getliveovercommit(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);

        return sysi->si_liveovercommit;
}

void rs_sysi_setliveovercommit(
        rs_sysi_t*  sysi,
        bool        liveovercommit)
{
        CHK_SYSI(sysi);

        sysi->si_liveovercommit = liveovercommit;
}

bool rs_sysi_getallowduplicatedelete(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);

        return sysi->si_allowduplicatedelete;
}

void rs_sysi_setallowduplicatedelete(
        rs_sysi_t*  sysi,
        bool        allowduplicatedelete)
{
        CHK_SYSI(sysi);

        sysi->si_allowduplicatedelete = allowduplicatedelete;
}

void *rs_sysi_getcryptopar(
         rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        return sysi->si_crypto_par;
}

void rs_sysi_setcryptopar(
        rs_sysi_t*  sysi,
        void* cryptopar)
{
        CHK_SYSI(sysi);
        sysi->si_crypto_par = cryptopar;
}

void rs_sysi_set_encryption_level(
        rs_sysi_t*  sysi,
        int level)
{
        CHK_SYSI(sysi);
        sysi->si_encryption_level = level;
}

long rs_sysi_encryption_level(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        return sysi->si_encryption_level;
}

#ifdef SS_DEBUG

su_mes_t* rs_sysi_getlogwaitmes(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);

        return(_RS_SYSI_GETLOGWAITMES(sysi));
}

su_ret_t* rs_sysi_getlogwaitrc(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);

        return(_RS_SYSI_GETLOGWAITRC(sysi));
}

#endif /* SS_DEBUG */

void rs_sysi_removelogwaitmes(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);

        ss_dassert(sysi->si_logwaitmeslist != NULL);
        ss_dassert(sysi->si_logwaitmes != NULL);

        su_meslist_mesdone(sysi->si_logwaitmeslist, sysi->si_logwaitmes);
        sysi->si_logwaitmes = NULL;
}

void rs_sysi_setlogwaitmes(
        rs_sysi_t*      sysi,
        su_meslist_t*   meslist,
        su_mes_t*       mes)
{
        CHK_SYSI(sysi);

        ss_dassert(sysi->si_logwaitmes == NULL);
        sysi->si_logwaitmeslist = meslist;
        sysi->si_logwaitmes = mes;
}

void rs_sysi_setlogwaitrc(
        rs_sysi_t*      sysi,
        su_ret_t*       rc)
{
        CHK_SYSI(sysi);

        ss_dassert(sysi->si_logwaitrc == NULL || rc == NULL);
        sysi->si_logwaitrc = rc;
}

#ifdef SS_DEBUG

SsMemCtxT* rs_sysi_getqmemctx(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);
        ss_aassert(rs_sysi_testthrid(sysi));

        return(sysi->si_qmemctx);
}

#endif /* SS_DEBUG */

void rs_sysi_setqmemctx(
        rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return;
        }

        CHK_SYSI(sysi);

        if (sysi->si_qmemctx == NULL) {
            sysi->si_qmemctx = SsQmemLocalCtxInit();
        }
}

void rs_sysi_clearqmemctx(
        rs_sysi_t* sysi)
{
        if (sysi == NULL) {
            return;
        }

        CHK_SYSI(sysi);

        sysi->si_qmemctx = NULL;
}

void rs_sysi_setsignalinfo(rs_sysi_t* cd, rs_signalinfo_t* signalinfo)
{
        CHK_SYSI(cd);

        cd->si_signalinfo = signalinfo;
}

rs_signalinfo_t* rs_sysi_getsignalinfo(rs_sysi_t* cd)
{
        if (cd == NULL) {
            return(NULL);
        } else {
            CHK_SYSI(cd);
            return(cd->si_signalinfo);
        }
}

bool rs_sysi_iscluster(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        return(sysi->si_tc_level > 0);
}

int rs_sysi_tc_level(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        return(sysi->si_tc_level);
}

void rs_sysi_set_tc_level(
        rs_sysi_t*  sysi,
        int         tc_level)
{
        CHK_SYSI(sysi);
        sysi->si_tc_level = tc_level;
}

void rs_sysi_setlpid_int8(
        rs_sysi_t*  sysi,
        ss_int8_t   lpid)
{
        CHK_SYSI(sysi);
        if (sysi->si_tc_level > 0) {
            ss_dassert(SsInt8Cmp(sysi->si_lpid, lpid) <= 0);
            sysi->si_islpid = TRUE;
            sysi->si_lpid = lpid;
        }
}

bool rs_sysi_getlpid_int8(
        rs_sysi_t*  sysi,
        ss_int8_t*  p_lpid)
{
        CHK_SYSI(sysi);
        if (sysi->si_islpid) {
            *p_lpid = sysi->si_lpid;
        }
        return(sysi->si_islpid);
}

void rs_sysi_clearlpid(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        sysi->si_islpid = FALSE;
}

void rs_sysi_tc_setoptimisticlocktimeout(
        rs_sysi_t*  sysi,
        long        val)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_optimisticlocktimeout != val) {
                sysi->si_tc_optimisticlocktimeout = val;
                sysi->si_tc_changes = TRUE;
            }
        }
}

void rs_sysi_tc_setlocktimeout(
        rs_sysi_t*  sysi,
        long        val)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_locktimeout != val) {
                sysi->si_tc_locktimeout = val;
                sysi->si_tc_changes = TRUE;
            }
        }
}

void rs_sysi_tc_setidletimeout(
        rs_sysi_t*  sysi,
        long        val)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_idletimeout != val) {
                sysi->si_tc_idletimeout = val;
                sysi->si_tc_changes = TRUE;
            }
        }
}

void rs_sysi_tc_setdurability(
        rs_sysi_t*  sysi,
        int         val)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_durability != val) {
                sysi->si_tc_durability = val;
                sysi->si_tc_changes = TRUE;
            }
        }
}

void rs_sysi_tc_setsafeness(
        rs_sysi_t*  sysi,
        int         val)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_safeness != val) {
                sysi->si_tc_safeness = val;
                sysi->si_tc_changes = TRUE;
            }
        }
}

void rs_sysi_tc_setreadonly(
        rs_sysi_t*  sysi,
        int         option)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_readonly != option) {
                sysi->si_tc_readonly = option;
                sysi->si_tc_changes = TRUE;
            }
        }
}

void rs_sysi_tc_setjoinpathspan(
        rs_sysi_t*  sysi,
        uint        val)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_joinpathspan != val) {
                sysi->si_tc_joinpathspan = val;
                sysi->si_tc_changes = TRUE;
            }
        }
}

uint rs_sysi_tc_joinpathspan(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        return sysi->si_tc_joinpathspan;
}

void rs_sysi_tc_setautocommit(
        rs_sysi_t*  sysi,
        bool        autocommit)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_autocommit != autocommit) {
                sysi->si_tc_autocommit = autocommit;
                sysi->si_tc_changes = TRUE;
            }
        }
}

bool rs_sysi_get_tc_write(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        ss_dassert(sysi->si_tc_level > 0);
        return sysi->si_tc_write;
}

void rs_sysi_set_tc_write(
        rs_sysi_t*  sysi,
        bool tc_write)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(sysi->si_tc_level > 0);
            if (sysi->si_tc_write != tc_write) {
                sysi->si_tc_write = tc_write;
                sysi->si_tc_changes = TRUE;
            }
        }
}

bool rs_sysi_get_tc_rebalance(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        ss_dassert(sysi->si_tc_level > 0);
        return sysi->si_tc_rebalance;
}

void rs_sysi_set_tc_rebalance(
        rs_sysi_t*  sysi, 
        bool rebalance)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(sysi->si_tc_level > 0);
            if (sysi->si_tc_rebalance != rebalance) {
                sysi->si_tc_rebalance = rebalance;
                if (rebalance) {
                    sysi->si_tc_changes = TRUE;
                }
            }
        }
}

bool rs_sysi_get_tc_changes(
        rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        return sysi->si_tc_changes;
}

void rs_sysi_forget_tc_changes(
         rs_sysi_t*  sysi)
{
        CHK_SYSI(sysi);
        sysi->si_tc_changes = FALSE;
}

rs_tcinfo_t* rs_tcinfo_init(
        rs_sysi_t*  sysi)
{
        rs_tcinfo_t* tc_info = SsMemAlloc(sizeof(rs_tcinfo_t));

        tc_info->tc_optimisticlocktimeout = sysi->si_tc_optimisticlocktimeout;
        tc_info->tc_locktimeout = sysi->si_tc_locktimeout;
        tc_info->tc_idletimeout = sysi->si_tc_idletimeout;
        tc_info->tc_stmtmaxtime = sysi->si_stmtmaxtime;
        tc_info->tc_sortarraysize = sysi->si_sortarraysize;
        tc_info->tc_convertorstounions = sysi->si_convertorstounions;
        tc_info->tc_sortedgroupby = sysi->si_sortedgroupby;
        tc_info->tc_optimizerows = sysi->si_optimizerows;
        tc_info->tc_simpleoptimizerrules = sysi->si_simpleoptimizerrules;
        tc_info->tc_isolation = sysi->si_tc_isolation;
        tc_info->tc_durability = sysi->si_tc_durability;
        tc_info->tc_safeness = sysi->si_tc_safeness;
        tc_info->tc_readonly = sysi->si_tc_readonly;
        tc_info->tc_joinpathspan = sysi->si_tc_joinpathspan;
        tc_info->tc_autocommit = sysi->si_tc_autocommit;
        tc_info->tc_sqlilevel = sysi->si_sqlilevel;

        if (sysi->si_sqlifname != NULL) {
            tc_info->tc_sqlifname = SsMemStrdup(sysi->si_sqlifname);
        } else {
            tc_info->tc_sqlifname = NULL;
        }

        return tc_info;
}

void rs_tcinfo_done(
        rs_tcinfo_t* tcinfo)
{
        if (tcinfo->tc_sqlifname != NULL) {
            SsMemFree(tcinfo->tc_sqlifname);
        }
        SsMemFree(tcinfo);
}
