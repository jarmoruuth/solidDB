/*************************************************************************\
**  source       * tab0srv.c
**  directory    * tab
**  description  * Server routines to table level is normal server code
**               * is not used.
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


#include <ssenv.h>

#include <ssc.h>
#include <ssdebug.h>
#include <sssem.h>
#include <ssthread.h>
#include <sstimer.h>

#include <ui0msg.h>

#include <su0usrid.h>

#include <dbe0db.h>

#include "tab1defs.h"
#include "tab0srv.h"
#include "tab0conn.h"

#ifndef SS_MYSQL
#include "tab0tli.h"
#include "tab1priv.h"
#include "../sc/sscapi.h"
#endif

bool sse_snc_isshutdown(void);
bool mainserver_isserving(void);

#define CHK_LW(lw)  ss_dassert(SS_CHKPTR(lw) && (lw)->lw_chk == TBCHK_SRVLOCKWAIT)

#define SRV_MAXTHR  7

typedef enum {
        TB_SRV_MERGE_RUN,
        TB_SRV_MERGE_FULL,
        TB_SRV_MERGE_QUICK,
        TB_SRV_MERGE_CLEANUP
} tb_srv_mergetype_t;

typedef struct {
        ss_debug(int lw_chk;)
        SsMesT*      lw_mes;
        long         lw_timeout;
        rs_sysi_t*   lw_cd;
} srv_lockwait_t;

typedef struct {
        SsMesT*             ti_mes;
        bool                ti_alive;
        bool                ti_running;
        tb_connect_t*       ti_tbcon;
        tb_srv_mergetype_t  ti_mergetype;
} srv_thrinfo_t;

typedef struct {
        char*           tai_name;
        su_task_fun_t   tai_fun;
        void*           tai_data;
} srv_taskinfo_t;

static bool              srv_shutdown = FALSE;
static int               srv_maxthr = SRV_MAXTHR;
static int               srv_first_merge_extra_thr = 0;
static su_list_t*        srv_tasklist;
static SsSemT*           srv_tasklistmutex;
static SsMesT*           srv_taskmes;
static srv_thrinfo_t     srv_thrinfo[SRV_MAXTHR];
static bool              srv_mergeactive = FALSE;
static mysql_funblock_t* srv_mysql_funblock = NULL;

static su_pa_t*          srv_tab_users = NULL;

static void SS_CALLBACK tb_srv_task_thread(void* param)
{
        srv_thrinfo_t* ti = param;
        SsMesRetT mesret;
        su_ret_t suret;
        dbe_db_t* db;
        rs_sysi_t* cd;
        int i;
        srv_taskinfo_t* tai;

        /* Allocate thread local memory context for this thread. */
        SsQmemLocalCtxInit();

        ss_pprintf_1(("tb_srv_task_thread:start\n"));

        db = tb_getdb(ti->ti_tbcon);
        cd = tb_getclientdata(ti->ti_tbcon);

        while (!srv_shutdown) {
            ss_pprintf_4(("tb_srv_task_thread:check list\n"));
            SsSemEnter(srv_tasklistmutex);
            if (su_list_length(srv_tasklist) > 0) {
                tai = su_list_removefirst(srv_tasklist);
            } else {
                tai = NULL;
            }
            SsSemExit(srv_tasklistmutex);

            if (tai != NULL) {
                int task_rc;
                ss_pprintf_2(("tb_srv_task_thread:execute %s\n", tai->tai_name));
                ti->ti_running = TRUE;
                task_rc = (*tai->tai_fun)(NULL, tai->tai_data);
                SsMemFree(tai);
                ss_pprintf_2(("tb_srv_task_thread:execute done\n"));
            }
            if (ti->ti_running) {
                /* We did execute task, do another check immediately. */
                ti->ti_running = FALSE;
            } else {
                mesret = SsMesRequest(ti->ti_mes, 10000L);
                ss_rc_dassert(mesret == SSMES_RC_SUCC || mesret == SSMES_RC_TIMEOUT, mesret);
            }
        }
        ss_pprintf_1(("tb_srv_task_thread:stop\n"));

        ti->ti_alive = FALSE;

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}

#ifdef SS_MYSQL

static void SS_CALLBACK tb_srv_merge_extra_thread(void* param)
{
        srv_thrinfo_t* ti = param;
        SsMesRetT mesret;
        dbe_db_t* db;
        rs_sysi_t* cd;
        dbe_mergeadvance_t mergeret;

        /* Allocate thread local memory context for this thread. */
        SsQmemLocalCtxInit();

        ss_pprintf_1(("tb_srv_merge_extra_thread:start thread\n"));

        db = tb_getdb(ti->ti_tbcon);
        cd = tb_getclientdata(ti->ti_tbcon);

        while (!srv_shutdown) {
            switch (ti->ti_mergetype) {
                case TB_SRV_MERGE_RUN:
                    mesret = SsMesRequest(ti->ti_mes, 10000L);
                    ss_rc_dassert(mesret == SSMES_RC_SUCC || mesret == SSMES_RC_TIMEOUT, mesret);
                    if (srv_mergeactive) {
                        ss_pprintf_2(("tb_srv_merge_extra_thread:start merge\n"));
                        ti->ti_running = TRUE;
                        while (srv_mergeactive) {
                            mergeret = dbe_db_mergeadvance_ex(db, cd, 10, TRUE, NULL);
                            if (mergeret == DBE_MERGEADVANCE_END) {
                                ss_pprintf_2(("tb_srv_merge_extra_thread:DBE_MERGEADVANCE_END\n"));
                                break;
                            }
                            if (mergeret == DBE_MERGEADVANCE_PART_END) {
                                ss_pprintf_2(("tb_srv_merge_extra_thread:DBE_MERGEADVANCE_PART_END\n"));
                                SsThrSleep(100);
                            }
                            SsThrSwitch();
                        }
                        ti->ti_running = FALSE;
                        ss_pprintf_2(("tb_srv_merge_extra_thread:stop merge\n"));
                    }
                    break;
                case TB_SRV_MERGE_FULL:
                    mesret = SsMesRequest(ti->ti_mes, 10000L);
                    ss_rc_dassert(mesret == SSMES_RC_SUCC || mesret == SSMES_RC_TIMEOUT, mesret);
                    if (!srv_shutdown && dbe_db_mergechecklimit(db)) {
                        ss_pprintf_2(("tb_srv_merge_extra_thread:start merge:TB_SRV_MERGE_FULL\n"));
                        dbe_db_mergestart_full(cd, db);
                        while (!srv_shutdown) {
                            mergeret = dbe_db_mergeadvance_ex(db, cd, 10, TRUE, NULL);
                            if (mergeret == DBE_MERGEADVANCE_END){
                                ss_pprintf_2(("tb_srv_merge_extra_thread:full merge, DBE_MERGEADVANCE_END\n"));
                                break;
                            }
                            SsThrSwitch();
                        }
                        dbe_db_mergestop(db);
                        ss_pprintf_2(("tb_srv_merge_extra_thread:full merge done\n"));
                    }
                    break;
                case TB_SRV_MERGE_QUICK:
                    mesret = SsMesRequest(ti->ti_mes, 5000L);
                    ss_rc_dassert(mesret == SSMES_RC_SUCC || mesret == SSMES_RC_TIMEOUT, mesret);
                    if (!srv_shutdown && dbe_db_quickmergechecklimit(db)) {
                        ss_pprintf_2(("tb_srv_merge_extra_thread:start merge:TB_SRV_MERGE_QUICK\n"));
                        dbe_db_quickmergestart(cd, db);
                        while (!srv_shutdown) {
                            mergeret = dbe_db_quickmergeadvance(db, cd);
                            if (mergeret == DBE_MERGEADVANCE_END){
                                ss_pprintf_2(("tb_srv_merge_extra_thread:quick merge, DBE_MERGEADVANCE_END\n"));
                                break;
                            }
                            SsThrSwitch();
                        }
                        dbe_db_quickmergestop(db);
                        ss_pprintf_2(("tb_srv_merge_extra_thread:quick merge done\n"));
                    }
                    break;
                case TB_SRV_MERGE_CLEANUP:
                    mesret = SsMesRequest(ti->ti_mes, 2000L);
                    ss_rc_dassert(mesret == SSMES_RC_SUCC || mesret == SSMES_RC_TIMEOUT, mesret);
                    if (dbe_db_mergecleanup(db)) {
                        ss_pprintf_2(("tb_srv_merge_extra_thread:merge cleanup\n"));
                    }
                    break;
                default:
                    ss_error;
            }
        }
        ss_pprintf_1(("tb_srv_merge_extra_thread:stop\n"));

        ti->ti_alive = FALSE;

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}

static void SS_CALLBACK tb_srv_merge_thread(void* param)
{
        srv_thrinfo_t* ti = param;
        SsMesRetT mesret;
        su_ret_t suret;
        dbe_db_t* db;
        rs_sysi_t* cd;
        int i;
        dbe_mergeadvance_t mergeret;
        SsTimeT lastmergetime = SsTime(NULL);

        /* Allocate thread local memory context for this thread. */
        SsQmemLocalCtxInit();

        ss_pprintf_1(("tb_srv_merge_thread:start\n"));

        db = tb_getdb(ti->ti_tbcon);
        cd = tb_getclientdata(ti->ti_tbcon);

        while (!srv_shutdown) {
            if (!dbe_cfg_mergecleanup) {
                bool start_merge;

                ss_pprintf_4(("tb_srv_merge_thread:dbe_db_mergechecklimit, mergewrites=%ld, time since last merge=%ld\n",
                    dbe_db_getmergewrites(db), SsTime(NULL) - lastmergetime));
                start_merge = dbe_db_mergechecklimit(db);
                if (start_merge) {
                    /* Merge will print start and stop messages internally. */
                    ss_pprintf_2(("tb_srv_merge_thread:start merge\n"));
                    lastmergetime = SsTime(NULL);
                    ti->ti_running = TRUE;
                    dbe_db_mergestart(cd, db);
                    srv_mergeactive = TRUE;
                    for (i = srv_first_merge_extra_thr; i < srv_maxthr; i++) {
                        if (srv_thrinfo[i].ti_alive) {
                            SsMesSend(srv_thrinfo[i].ti_mes);
                        }
                    }
                    while (!srv_shutdown) {
                        mergeret = dbe_db_mergeadvance_ex(db, cd, 10, TRUE, NULL);
                        if (mergeret == DBE_MERGEADVANCE_END){
                            ss_pprintf_2(("tb_srv_merge_thread:DBE_MERGEADVANCE_END\n"));
                            break;
                        }
                        SsThrSwitch();
                    }
                    srv_mergeactive = FALSE;
                    for (i = srv_first_merge_extra_thr; i < srv_maxthr; i++) {
                        if (srv_thrinfo[i].ti_running) {
                            ss_pprintf_2(("tb_srv_merge_thread:wait mergethread to stop running, i=%d\n", i));
                            SsThrSleep(100);
                        }
                    }
                    dbe_db_mergestop(db);
                    ss_pprintf_2(("tb_srv_merge_thread:merge done\n"));
                }
                if (ti->ti_running) {
                    /* We did merge, do another check immediately. */
                    ti->ti_running = FALSE;
                } else {
                    mesret = SsMesRequest(ti->ti_mes, 1000L);
                    ss_rc_dassert(mesret == SSMES_RC_SUCC || mesret == SSMES_RC_TIMEOUT, mesret);
                }
            } else {
                mesret = SsMesRequest(ti->ti_mes, 1000L);
                ss_rc_dassert(mesret == SSMES_RC_SUCC || mesret == SSMES_RC_TIMEOUT, mesret);
            }
        }
        ss_pprintf_1(("tb_srv_merge_thread:stop\n"));

        ti->ti_alive = FALSE;

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}

static void SS_CALLBACK tb_srv_checkpoint_thread(void* param)
{
        srv_thrinfo_t* ti = param;
        SsMesRetT mesret;
        su_ret_t suret;
        dbe_db_t* db;
        rs_sysi_t* cd;
        int i;

        /* Allocate thread local memory context for this thread. */
        SsQmemLocalCtxInit();

        ss_pprintf_1(("tb_srv_checkpoint_thread:start\n"));

        db = tb_getdb(ti->ti_tbcon);
        cd = tb_getclientdata(ti->ti_tbcon);

        while (!srv_shutdown) {
            ss_pprintf_4(("tb_srv_checkpoint_thread:dbe_db_cpchecklimit\n"));
            if (dbe_db_cpchecklimit(db)) {
                ss_pprintf_2(("tb_srv_checkpoint_thread:start checkpoint\n"));
                ti->ti_running = TRUE;
                ui_msg_message(CP_MSG_CREATION_STARTED);
                suret = tb_createcheckpoint(ti->ti_tbcon, FALSE);
                if (suret == SU_SUCCESS) {
                    ui_msg_message(CP_MSG_CREATION_COMPLETED);
                } else {
                    su_err_t* errh;
                    su_err_init(&errh, suret);
                    ui_msg_message(
                        CP_MSG_CREATION_START_FAILED_SS,
                        "checkpoint",
                        su_err_geterrstr(errh));
                    su_err_done(errh);
                }
                ss_pprintf_2(("tb_srv_checkpoint_thread:checkpoint done\n"));
                ti->ti_running = FALSE;
            }
            mesret = SsMesRequest(ti->ti_mes, 10000L);
            ss_rc_dassert(mesret == SSMES_RC_SUCC || mesret == SSMES_RC_TIMEOUT, mesret);
        }
        ss_pprintf_1(("tb_srv_checkpoint_thread:stop\n"));

        ti->ti_alive = FALSE;

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}

bool tb_srv_init(
        su_inifile_t* inifile,
        dbe_cryptoparams_t* cp,
        mysql_funblock_t* mysql_fb)
{
        ss_pprintf_1(("tb_srv_init:myfunblock %x\n", mysql_fb));

        srv_tab_users = su_pa_init();

        srv_mysql_funblock = mysql_fb;
        return(tb_init(inifile, cp));
}

void tb_srv_done(void)
{
        ss_pprintf_1(("tb_srv_done\n"));
#ifdef SS_SEMPROFILE
        {
            SS_FILE* fp;
            fp = SsFOpenT("semprofile.csv", "w");
            ss_assert(fp != NULL);
            SsSemDbgPrintFormattedList(fp, FALSE);
            SsFClose(fp);
        }
#endif /* SS_SEMPROFILE */

        if (srv_tab_users != NULL) {
            su_pa_done(srv_tab_users);
            srv_tab_users = NULL;
        }

        tb_done();
}

bool tb_srv_start(tb_database_t* tdb)
{
        int i;
        int maxmergethreads;
        bool succp;
        SsThreadT* thr[SRV_MAXTHR];

        ss_pprintf_1(("tb_srv_start\n"));

        if (dbe_cfg_mergecleanup) {
            maxmergethreads = 3; /* One server thread, three merge threads. */
        } else {
            maxmergethreads = SRV_MAXTHR;
        }

        srv_shutdown = FALSE;
        srv_mergeactive = FALSE;

        for (i = 0; i < SRV_MAXTHR; i++) {
            srv_thrinfo[i].ti_mes = SsMesCreateLocal();
            srv_thrinfo[i].ti_tbcon = tb_sysconnect_init(tdb);
            ss_assert(srv_thrinfo[i].ti_tbcon != NULL);
            srv_thrinfo[i].ti_alive = FALSE;
            srv_thrinfo[i].ti_running = FALSE;
        }
        thr[0] = SsThrInitParam(
                    tb_srv_checkpoint_thread,
                    "tb_srv_checkpoint_thread",
                    1024*1024,
                    &srv_thrinfo[0]);
        thr[1] = SsThrInitParam(
                    tb_srv_merge_thread,
                    "tb_srv_merge_thread",
                    1024*1024,
                    &srv_thrinfo[1]);
        thr[2] = SsThrInitParam(
                    tb_srv_task_thread,
                    "tb_srv_task_thread",
                    1024*1024,
                    &srv_thrinfo[2]);

        srv_tasklist = su_list_init(NULL);
        srv_tasklistmutex = SsSemCreateLocal(SS_SEMNUM_TAB_SRVTASKLIST);
        srv_taskmes = srv_thrinfo[2].ti_mes;

        srv_first_merge_extra_thr = 3;
        for (i = srv_first_merge_extra_thr; i < SRV_MAXTHR && maxmergethreads > 0; i++, maxmergethreads--) {
            if (dbe_cfg_mergecleanup) {
                switch (i) {
                    case 1:
                        srv_thrinfo[i].ti_mergetype = TB_SRV_MERGE_FULL;
                        break;    
                    case 2:
                        srv_thrinfo[i].ti_mergetype = TB_SRV_MERGE_QUICK;
                        break;    
                    case 3:
                        srv_thrinfo[i].ti_mergetype = TB_SRV_MERGE_CLEANUP;
                        break;    
                    default:
                        ss_error;
                        break;
                }
            } else {
                srv_thrinfo[i].ti_mergetype = TB_SRV_MERGE_RUN;
            }
            thr[i] = SsThrInitParam(
                        tb_srv_merge_extra_thread,
                        "tb_srv_merge_extra_thread",
                        1024*1024,
                        &srv_thrinfo[i]);
        }
        srv_maxthr = i;
        for (i = 0; i < srv_maxthr; i++) {
            succp = SsThrEnableBool(thr[i]);
            if (!succp) {
                break;
            }
            srv_thrinfo[i].ti_alive = TRUE;
        }
        if (!succp) {
            tb_srv_stop();
        }
        for (i = 0; i < srv_maxthr; i++) {
            SsThrDone(thr[i]);
        }

        SsTimerGlobalInit();

        return(succp);
}

void tb_srv_stop(void)
{
        SsTimeT now;
        int nalive;
        int i;

        ss_pprintf_1(("tb_srv_stop\n"));

        srv_mergeactive = FALSE;
        srv_shutdown = TRUE;

        now = SsTime(NULL);
        do {
            nalive = 0;
            for (i = 0; i < srv_maxthr; i++) {
                if (srv_thrinfo[i].ti_alive) {
                    SsMesSend(srv_thrinfo[i].ti_mes);
                    nalive++;
                }
            }
            if (nalive > 0) {
                ss_pprintf_2(("tb_srv_stop:wait, nalive=%d\n", nalive));
                SsThrSleep(100);
            }
            /* AKARYAKIN 19-JAN-2007: Disable shutdown timeout
             because possible checkpoint operation cannot be 
             cancelled and we have to wait for it to complete.
             Bugzilla bug #516 (int).
            */
        } while (nalive > 0 /* && SsTime(NULL) < now + 10 */);
        for (i = 0; i < SRV_MAXTHR; i++) {
            tb_sysconnect_done(srv_thrinfo[i].ti_tbcon);
            SsMesFree(srv_thrinfo[i].ti_mes);
        }
        su_list_done(srv_tasklist);
        SsSemFree(srv_tasklistmutex);
        SsTimerGlobalDone();
        ss_pprintf_2(("tb_srv_stop:return\n"));
}

#else /* SS_MYSQL */

static int solid_started = 0;
static SsSemT* solid_mutex = NULL;
static ssc_serverhandle_t solid_h = NULL;

bool tb_srv_init(
        su_inifile_t* inifile,
        dbe_cryptoparams_t* cp,
        mysql_funblock_t* mysql_fb)
{
        SscRetT sscret;
        char* args[8];
        int argsc;
        int ntry = 0;

        ss_pprintf_1(("tb_srv_init:myfunblock %x\n", mysql_fb));
        SS_PUSHNAME("tb_srv_init");

        srv_mysql_funblock = mysql_fb;

        if (srv_tab_users == NULL) {
            srv_tab_users = su_pa_init();
        }

        if (!solid_started) {
            args[0]="";
            args[1] = "-Udba";
            args[2] = "-Pdba";
            args[3] = "-Cdba";
            args[4] = "-n";
            args[5] = "solidDB for MySQL with Accelerator";
            args[6] = NULL;
            argsc = 6;
            
            solid_h = NULL;
            tb_init_inifile = inifile;
            
            ss_setservername("solidDB for MySQL with Accelerator");
            ss_pprintf_1(("tb_srv_init:SSCStartServer\n"));
            
            sscret = SSCStartServer(argsc, args, &solid_h, SSC_STATE_OPEN);
            ss_pprintf_1(("tb_srv_init:SSCStartServer:rc %d\n", sscret));

            if (sscret == SSC_BROKENNETCOPY) {
                ss_pprintf_1(("tb_srv_init:SSC_BROKENNETCOPY:re-start in backupserver mode\n"));
                args[0]="";
                args[1] = "-Udba";
                args[2] = "-Pdba";
                args[3] = "-Cdba";
                args[4] = "-n";
                args[5] = "solidDB for MySQL with Accelerator";
                args[6] = "-xbackupserver";
                args[7] = NULL;
                argsc = 7;
                sscret = SSCStartServer(argsc, args, &solid_h, SSC_STATE_OPEN);
                ss_pprintf_1(("tb_srv_init:SSCStartServer:rc %d (start in backupserver mode)\n", sscret));

                ntry = 0;
                if (sscret == SSC_SUCCESS) {
                    sscret = SSC_BROKENNETCOPY;
                    //while(!srv_shutdown && sse_snc_isshutdown() && ntry < 120) {
                    while(!srv_shutdown && sse_snc_isshutdown()) {
                        SsThrSleep(2000);
                        if (ntry % 100 == 0) {
                            printf("sse_snc_isshutdown %d\n", ntry);
                        }
                        ntry++;
                    }
                    if (!sse_snc_isshutdown()) {
                        sscret = SSC_SUCCESS;
                        ss_pprintf_1(("tb_srv_init:Netcopy done\n"));
                    }
                }
            }

            solid_started = (sscret == SSC_SUCCESS && !srv_shutdown);

            if (solid_started) {
                bool succp;
                long userid;
                TliConnectT* tcon;
                rs_auth_t* auth;
                rs_sysi_t* cd;
                TliRetT trc;
                su_err_t* errh;
                int surc;

                /* Create a global mutex for relation cursor list access.
                 * TODO: Semnum is not exactly correct but works ok here.
                 */
                ss_setservername("solidDB for MySQL with Accelerator");
                SsPrintf("%s\n", "solidDB for MySQL with Accelerator");

                tcon = TliConnect("", "dba", "dba");
                ss_assert(tcon != NULL);
                cd = TliGetCd(tcon);
                auth =  rs_sysi_auth(cd);
                rs_auth_setsystempriv(cd, auth, TRUE);

                succp = tb_priv_usercreate(
                            tcon,
                            "MYSQL",
                            "MYSQL",
                            TB_UPRIV_ADMIN,
                            &userid,
                            &errh);
                if (!succp) {
                    surc = su_err_geterrcode(errh);
                    succp = (surc == DBE_ERR_TRXREADONLY);
                }
                ss_rc_assert(succp, surc);
                trc = TliCommit(tcon);
                /* ss_rc_assert(trc == TLI_RC_SUCC, trc); */
                TliConnectDone(tcon);
            }
            srv_shutdown = FALSE;
        }
        SS_POPNAME;
        return(solid_started);
}

void tb_srv_done(void)
{
        ss_pprintf_1(("tb_srv_done\n"));

        srv_shutdown = TRUE;

        if (solid_started) {
            ss_pprintf_1(("Stop solid server...\n"));
            if (SSCStopServer(solid_h, FALSE) != SSC_SUCCESS) {
                ss_pprintf_1(("Stop solid Server (force)...\n"));
                SSCStopServer(solid_h, TRUE);
            }
            solid_started = 0;
            solid_h = NULL;
        }

        if (srv_tab_users != NULL) {
            su_pa_done(srv_tab_users);
            srv_tab_users = NULL;
        }

        ss_pprintf_2(("tb_srv_done:return\n"));
}

bool tb_srv_start(tb_database_t* tdb)
{
        SsThreadT* thr;
        bool succp;

        ss_pprintf_1(("tb_srv_start\n"));

        srv_thrinfo[0].ti_mes = SsMesCreateLocal();
        srv_thrinfo[0].ti_tbcon = tb_sysconnect_init(tdb);
        ss_assert(srv_thrinfo[0].ti_tbcon != NULL);
        srv_thrinfo[0].ti_alive = FALSE;
        srv_thrinfo[0].ti_running = FALSE;

        srv_tasklist = su_list_init(NULL);
        srv_tasklistmutex = SsSemCreateLocal(SS_SEMNUM_TAB_SRVTASKLIST);
        srv_taskmes = srv_thrinfo[0].ti_mes;

        thr = SsThrInitParam(
                    tb_srv_task_thread,
                    "tb_srv_task_thread",
                    1024*1024,
                    &srv_thrinfo[0]);

        succp = SsThrEnableBool(thr);
        ss_assert(succp);
        SsThrDone(thr);

        srv_thrinfo[0].ti_alive = TRUE;

        return(TRUE);
}

void tb_srv_stop(void)
{
        ss_pprintf_1(("tb_srv_stop\n"));

        srv_shutdown = TRUE;

        for (;;) {
            if (srv_thrinfo[0].ti_alive) {
                SsMesSend(srv_thrinfo[0].ti_mes);
                SsThrSleep(100);
            } else {
                break;
            }
        }
        tb_sysconnect_done(srv_thrinfo[0].ti_tbcon);
        SsMesFree(srv_thrinfo[0].ti_mes);

        su_list_done(srv_tasklist);
        SsSemFree(srv_tasklistmutex);
}

#endif /* SS_MYSQL */

void tb_srv_shutdown(void)
{
        uint i;
        void* ctx;

        ss_pprintf_1(("tb_srv_shutdown\n"));
        su_pa_do(srv_tab_users, i) {
            if (su_pa_indexinuse(srv_tab_users, i)) {
                ctx = su_pa_getdata(srv_tab_users, i);
                ss_pprintf_1(("tb_srv_shutdown:throw out user %d\n", i));
                srv_mysql_funblock->con_throwout(ctx);
            }
        }
}

/*##**********************************************************************\
 *
 *              tb_srv_connect_local
 *
 * Connects client to the table level and crypts the password.
 * Also characters sets are converted from server character set to
 * database character set.
 * Stores connection ctx for throwout.
 *
 * Parameters :
 *
 *      loginid -
 *              login id
 *
 *      username -
 *              user name
 *
 *      password -
 *              password
 *
 * Return value - give :
 *
 *      connection pointer, or
 *      NULL if connection failed
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_connect_t* tb_srv_connect_local(
        void* ctx,
        int loginid,
        char* username,
        char* password,
        int* p_id)
{
        tb_connect_t* tbcon;
        int id;

        tbcon = tb_connect_local(-1, (char *)"mysql", (char *)"mysql");

        if (tbcon != NULL) {
            SsSemEnter(srv_tasklistmutex);
            id = su_pa_insert(srv_tab_users, ctx);
            SsSemExit(srv_tasklistmutex);
            *p_id = id;
        }

        return(tbcon);
}

void tb_srv_disconnect(
        tb_connect_t* tbcon,
        void* ctx,
        int id)
{
        SsSemEnter(srv_tasklistmutex);
        ss_dassert(su_pa_indexinuse(srv_tab_users, id));
        su_pa_remove(srv_tab_users, id);
        SsSemExit(srv_tasklistmutex);

        tb_disconnect(tbcon);
}


static void srv_lockwakeup(void* task)
{
        srv_lockwait_t* lw = task;

        CHK_LW(lw);
        ss_pprintf_1(("srv_lockwakeup\n"));

        SsMesSend(lw->lw_mes);
}

static bool srv_lockwait(void* task)
{
        srv_lockwait_t* lw = task;
        long timeout;
        su_profile_timer;

        CHK_LW(lw);
        ss_pprintf_1(("srv_lockwait:timeout=%ld\n", lw->lw_timeout));

        timeout = lw->lw_timeout;
        lw->lw_timeout = 0;
        rs_sysi_setlockwaitfun(lw->lw_cd, NULL);

        su_profile_start;

        SsMesRequest(lw->lw_mes, timeout);

        su_profile_stop("srv_lockwait");

        ss_pprintf_1(("srv_lockwait:wake up\n"));

        return(TRUE);
}

static void srv_startlockwait(void* task)
{
        srv_lockwait_t* lw = task;

        CHK_LW(lw);
        ss_pprintf_1(("srv_startlockwait\n"));

        lw->lw_timeout = SSSEM_INDEFINITE_WAIT;

        rs_sysi_setlockwaitfun(lw->lw_cd, srv_lockwait);
}

static void srv_startlockwait_timeout(void* task, long timeout_ms)
{
        srv_lockwait_t* lw = task;

        CHK_LW(lw);
        ss_pprintf_1(("srv_startlockwait_timeout:timeout_ms=%ld\n", timeout_ms));

        lw->lw_timeout = timeout_ms;

        rs_sysi_setlockwaitfun(lw->lw_cd, srv_lockwait);
}

static void tb_srv_taskstart(
        char* task_name,
        su_task_fun_t task_fun,
        void* task_data,
        rs_sysi_t* task_cd)
{
        srv_taskinfo_t* tai;

        ss_pprintf_1(("tb_srv_taskstart:%s\n", task_name));

        tai = SSMEM_NEW(srv_taskinfo_t);
        tai->tai_name = task_name;
        tai->tai_fun = task_fun;
        tai->tai_data = task_data;

        SsSemEnter(srv_tasklistmutex);
        su_list_insertlast(srv_tasklist, tai);
        SsSemExit(srv_tasklistmutex);

        SsMesSend(srv_taskmes);
}

void tb_srv_initcd(rs_sysi_t* cd)
{
        srv_lockwait_t* lw;

        ss_pprintf_1(("tb_srv_initcd\n"));

        lw = SSMEM_NEW(srv_lockwait_t);

        ss_debug(lw->lw_chk = TBCHK_SRVLOCKWAIT);
        lw->lw_mes = SsMesCreateLocal();
        lw->lw_cd = cd;
        lw->lw_timeout = 0;
        CHK_LW(lw);

        rs_sysi_settask(cd, lw);
        rs_sysi_setlockwakeupfun(cd, srv_lockwakeup);
        rs_sysi_setstartlockwaitfun(cd, srv_startlockwait, srv_startlockwait_timeout);
        rs_sysi_setstarttaskfun(cd, tb_srv_taskstart);
}

void tb_srv_donecd(rs_sysi_t* cd)
{
        srv_lockwait_t* lw;

        ss_pprintf_1(("tb_srv_donecd\n"));

        lw = rs_sysi_task(cd);
        CHK_LW(lw);
        ss_dassert(lw->lw_timeout == 0);

        SsMesFree(lw->lw_mes);
        SsMemFree(lw);
}


bool tb_srv_frm_to_disk(
        tb_connect_t* tcon,
        rs_sysi_t* cd,
        vtpl_t* vtpl,
        bool deletep)
{
        bool succp = TRUE;
        tb_trans_t* trans;
        rs_entname_t *en = NULL;
        char *frmname;
        tb_database_t* tdb;

        if (srv_mysql_funblock->frm_from_db_to_disk != NULL) {

            tdb = (tb_database_t*)rs_sysi_tabdb(cd);
            ss_dassert(tdb != NULL);
            tcon = tb_getconnection(tdb);
            ss_dassert(tcon != NULL);
            /* trans = tb_getsqltrans(tcon); */
            trans = tb_trans_init(cd);
            ss_assert(trans != NULL);
            frmname = NULL;
            /* succp = srv_mysql_funblock->frm_from_db_to_disk(tcon, cd, trans, en, frmname, deletep); */
            succp = srv_mysql_funblock->frm_from_db_to_disk(tcon, cd, trans, en, deletep);
            tb_trans_done(cd, trans);
        }
        return(succp);
}


/* ---------------------------------------------------------------------
 * performance output stuff
 */
typedef struct {
        SS_FILE*       pd_fp;
        uint           pd_interval;
        bool           pd_names;
        bool           pd_diff;
        int            pd_userid;
        bool           pd_raw;
        char*          pd_comment;
        long           pd_activectr;
        tb_database_t* pd_tdb;
} pmon_difftask_t;

static long pmon_difftask_activectr = 0;
static bool pmon_difftask_active = FALSE;

static void pmon_print(pmon_difftask_t* pd, bool raw, bool names, bool diff)
{
        char** filters = NULL;
        static ss_pmon_t last_pmon;
        static bool is_last_pmon;
        SsTimeT time_ms;
        static SsTimeT last_time_ms;
        bool print_timediff = TRUE;
        bool print_time = FALSE;
        SsTimeT timediff_ms;

        if (raw || names || diff) {
            /* Raw, name or diff output.
             */
            int i;
            int j;
            ss_pmon_t* pmon;
            dstr_t ds = NULL;
            bool firstp = TRUE;
            char buf[80];

            filters = (char**)SsMemAlloc(sizeof(filters[0]) * 2);
            filters[0] = "";
            filters[1] = NULL;

            pmon = SSMEM_NEW(ss_pmon_t);

            tb_database_pmonupdate_nomutex(pd->pd_tdb);
            SsPmonGetData(pmon);
            if (names || diff) {
                time_ms = SsTimeMs();
                timediff_ms = time_ms - last_time_ms;
                last_time_ms = time_ms;
            }

            dstr_set(&ds, "");

            if (diff && !is_last_pmon) {
                memcpy(&last_pmon, pmon, sizeof(ss_pmon_t));
                is_last_pmon = TRUE;
            }
            if (print_time) {
                if (names) {
                    SsSprintf(buf, "Time");
                } else {
                    SsSprintf(buf, "%04ld", SsTime(NULL)%10000);
                }
                dstr_app(&ds, buf);
                firstp = FALSE;
            }
            if (print_timediff) {
                if (names) {
                    SsSprintf(buf, "TimeMs");
                } else {
                    SsSprintf(buf, "%ld", timediff_ms);
                }
                dstr_app(&ds, buf);
                firstp = FALSE;
            }
            for (j = 0; filters[j] != NULL; j++) {
                for (i = 0; i < SS_PMON_MAXVALUES; i++) {
                    if (SsPmonAccept(filters[j], (ss_pmon_val_t)i)) {
                        if (names) {
                            SsSprintf(buf, "%s%s", firstp ? "" : ",", ss_pmon_names[i].pn_name);
                            memcpy(&last_pmon, pmon, sizeof(ss_pmon_t));
                            is_last_pmon = TRUE;
                        } else if (diff) {
                            if (ss_pmon_names[i].pn_type == SS_PMONTYPE_VALUE) {
                                SsSprintf(buf, "%s%lu",  firstp ? "" : ",", pmon->pm_values[i]);
                            } else {
                                long valdiff;
                                valdiff = pmon->pm_values[i] - last_pmon.pm_values[i];
                                if (raw) {
                                    SsSprintf(buf, "%s%lu",  firstp ? "" : ",", valdiff);
                                } else {
                                    double val;
                                    double secs;
                                    secs = (double)timediff_ms / 1000.0;
                                    if (secs < 0.0001) {
                                        val = 0.0;
                                    } else {
                                        val = valdiff / secs;
                                    }
                                    SsSprintf(buf, "%s%.2lf",  firstp ? "" : ",", val);
                                }
                            }
                        } else {
                            SsSprintf(buf, "%lu ", pmon->pm_values[i]);
                        }
                        dstr_app(&ds, buf);
                        firstp = FALSE;
                    }
                }
            }
            if (print_timediff) {
                if (names) {
                    SsSprintf(buf, ",CurTimeSec");
                } else {
                    SsSprintf(buf, ",%ld", SsTime(NULL)%100000);
                }
                dstr_app(&ds, buf);
            }
            if (diff) {
                memcpy(&last_pmon, pmon, sizeof(ss_pmon_t));
            }

            SsFPrintf(pd->pd_fp, "%s\n", ds);

            dstr_free(&ds);
            SsMemFree(pmon);
            SsMemFree(filters);
        }
}

static void pmon_difftask(void* ctx, SsTimerRequestIdT req_id)
{
        pmon_difftask_t* pd = (pmon_difftask_t*)ctx;
        char* str = "pmon_difftask";
        SsTimerRequestIdT tid = 0;
        bool diff = TRUE;

        ss_dprintf_2(("pmon_difftask:tid %ld\n", (long)req_id));

        if (!SsTimerRequestIsValid(req_id)) {
            return;
        }
        if (srv_shutdown || pmon_difftask_activectr != pd->pd_activectr) {
            SsTimerCancelRequest(req_id);
            su_usrid_done(pd->pd_userid);
            if (pd->pd_fp != NULL) {
                SsFClose(pd->pd_fp);
            }
            if (pd->pd_comment != NULL) {
                SsMemFree(pd->pd_comment);
            }
            SsMemFree(pd);
            return;
        }

        pmon_print(pd, pd->pd_raw, pd->pd_names, pd->pd_diff);
        pd->pd_names = FALSE;
        pd->pd_diff = TRUE;
        tid = SsTimerAddRequest(pd->pd_interval, pmon_difftask, pd);
}


bool tb_srv_pmondiff_start(
        rs_sysi_t* cd,
        uint interval,
        char* filename,
        bool append,
        bool raw,
        char* comment,
        su_err_t** p_errh)
{
        bool succp = TRUE;
        pmon_difftask_t* pd;

        if (pmon_difftask_active) {
            su_err_init(p_errh, DBE_ERR_FAILED);
            return(FALSE);
        }

        pd = (pmon_difftask_t*)SsMemCalloc(sizeof(pmon_difftask_t), 1);
        pd->pd_activectr = pmon_difftask_activectr;
        pd->pd_names = TRUE;
        pd->pd_diff = FALSE;
        pd->pd_tdb = (tb_database_t*)rs_sysi_tabdb(cd);
        pd->pd_comment = NULL;
        pd->pd_interval = interval;
        pd->pd_raw = raw;

        pd->pd_userid = su_usrid_init();
        if (append) {
            pd->pd_fp = SsFOpenT(filename, "a+");
        } else {
            pd->pd_fp = SsFOpenT(filename, "w");
        }
        if (pd->pd_fp == NULL) {
            succp = FALSE;
            su_err_init(p_errh, DBE_ERR_FAILED);
        } else {
            pmon_difftask_active = TRUE;
            SsTimerAddRequest(pd->pd_interval, pmon_difftask, pd);
        }

        return(succp);
}

bool tb_srv_pmondiff_stop(
        rs_sysi_t* cd,
        su_err_t** p_errh)
{
        bool succp = TRUE;

        pmon_difftask_activectr++;
        pmon_difftask_active = FALSE;

        return(succp);
}

void tb_srv_pmondiff_clear(
        rs_sysi_t* cd)
{
        SsPmonClear();
        /* sse_admin_pmonclear(); */
        ss_beta(dbe_logi_clearlogrecinfo();)
}

