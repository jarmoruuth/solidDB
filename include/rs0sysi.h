/*************************************************************************\
**  source       * rs0sysi.h
**  directory    * res
**  description  * System info data type for "client data"
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


#ifndef RS0SYSI_H
#define RS0SYSI_H

#include <ssstdio.h>
#include <ssstddef.h>

#include <ssc.h>
#include <ssmem.h>
#include <ssqmem.h>
#include <sssem.h>
#include <ssmsglog.h>
#include <ssthread.h>

#include <sstime.h>

#include <uti0va.h>

#include <su0prof.h>
#include <su0rbtr.h>
#include <su0task.h>
#include <su0list.h>
#include <su0parr.h>
#include <su0rand.h>
#include <su0mesl.h>
#include <su0bflag.h>

#include "rs0types.h"
#include "rs0rbuf.h"
#include "rs0sqli.h"
#include "rs0entna.h"
#include "rs0trend.h"
#include "rs0evnot.h"
#include "rs0defno.h"
#include "rs0trend.h"

#ifdef SS_SYNC
#include "rs0bull.h"
#endif /* SS_SYNC */

#define CHK_SYSI(sysi)       ss_dassert(SS_CHKPTR(sysi) && (sysi)->si_chk == RSCHK_SYSI)
#define CHK_SYSILISTNODE(sl) ss_aassert(sl != NULL && (sl)->sl_chk == RSCHK_SYSILISTNODE)

typedef struct sysi_listnode_st {
        void*      sl_next;
        rs_check_t sl_chk;
} sysi_listnode_t;

typedef enum {
        RS_SIGNAL_DDCHANGE,
        RS_SIGNAL_FLUSHSQLCACHE,
        RS_SIGNAL_DEFSCHEMACHANGE,
        RS_SIGNAL_DEFCATALOGCHANGE,
        RS_SIGNAL_HSB_STATE_CHANGE_START,
        RS_SIGNAL_HSB_STATE_CHANGE_END
} rs_signaltype_t;

typedef struct {
        bool rsi_flushproccache;
        bool rsi_flushstmtcache;
        bool rsi_defschemachanged;
        bool rsi_defcatalogchanged;
        int  rsi_tf_rolechange_ctr;
        bool rsi_tf_rolechange_progress;

#ifdef SS_TC_CLIENT
        bool rsi_tc_rebalance;
#endif /* SS_TC_CLIENT */

} rs_signalinfo_t;

typedef enum {
        RS_SYSI_CONNECT_USER,   /* default */
        RS_SYSI_CONNECT_SYSTEM,
        RS_SYSI_CONNECT_HSB
} rs_sysi_connecttype_t;

typedef enum {
        RS_SYSI_MTABLE = SU_BFLAG_BIT(0),        /* Values OR'd in */
        RS_SYSI_DTABLE = SU_BFLAG_BIT(1)
} rs_sysi_tabletype_t;

#ifdef SS_PMEM
typedef enum {
        SYSI_MEMCTX_NOTINSTALLED,
        SYSI_MEMCTX_INSTALLED,
        SYSI_MEMCTX_NEVERINSTALL
} sysi_memctxstat_t;
#endif /* SS_PMEM */

/* Flags.
 */
#define RS_SYSI_FLAG_STORAGETREEONLY    SU_BFLAG_BIT(0)
#define RS_SYSI_FLAG_TC_USE_PRIMARY     SU_BFLAG_BIT(1)
#define RS_SYSI_FLAG_TC_NOT_ALLOWED     SU_BFLAG_BIT(2)
#define RS_SYSI_FLAG_MYSQL              SU_BFLAG_BIT(3)
#define RS_SYSI_FLAG_SEARCH_SIMPLETVAL  SU_BFLAG_BIT(4)


/***** INTERNAL ONLY BEGIN *****/

#define RS_SYSI_MAXLINKSEM      10

#ifdef SS_SYNC
typedef struct {
        char*           ssi_catalog;
        bool            ssi_issyncmaster;
        bool            ssi_issyncreplica;
        char*           ssi_syncnode;
        SsTimeT         ssi_nodetime;
        long            ssi_syncmasterid;
        su_list_t*      ssi_msgnamelocklist;
        void*           ssi_syncmsg_rbt;
        rs_bboard_t*    ssi_bboard;
        rs_bboard_t*    ssi_propertystore;
        char*           ssi_defaultpropagatewherestr;
        void*           ssi_ipub;
        char*           ssi_syncusername;
        dynva_t         ssi_syncpassword;
        long            ssi_syncid;
} sysi_syncinfo_t;
#endif /* SS_SYNC */

struct rssysinfostruct {
        ss_debug(int    si_chk;)
        int             si_nlink;
        SsSemT*         si_sem;
        SsSemT*         si_linksem;
        rs_auth_t*      si_auth;
        rs_rbuf_t*      si_rbuf;
        void*           si_dbuser;
        void*           si_tbcon;
        void*           si_db;
        void*           si_tabdb;
        rs_sqlinfo_t*   si_sqli;
        SsSemT**        si_rslinksemarray;   /* Semaphore array for rs_level link
                                                operations (rs_relh, rs_viewh,
                                                rs_key). */
        void*           si_task;        /* Current user task. */

        void            (*si_eventwaitwithtimeoutfp)(void* task, int event, long timeout, void (*_cbfun)(void*), void* timeout_cbctx);

        void            (*si_startlockwait_tmofp)(void* task, long timeout); /* Start wait with timeout function */
        void            (*si_startlockwaitfp)(void* task);                   /* Start wait function */
        bool            (*si_lockwaitfp)(void* task);     /* Wait function */
        void            (*si_lockwakeupfp)(void* task);   /* Wakeup function */
        void            (*si_signalfp)(               /* Signal function. */
                            int userid,
                            rs_signaltype_t signaltype);
        void            (*si_taskstartfp)(               /* Task start function. */
                                char* task_name,
                                su_task_fun_t task_fun,
                                void* task_data,
                                rs_sysi_t* task_cd);
        int             si_userid;
        long            si_dbuserid;
        char*           si_dateformat;
        char*           si_timeformat;
        char*           si_timestampformat;
        void*           si_xsmgr;
        int             si_sqlilevel;
        char*           si_sqlifname;
        void*           si_sqlifp;
        su_rbt_t*       si_stmttabnameinfo;
        int             si_sortarraysize;
        int             si_convertorstounions;
        int             si_sortedgroupby;
        int             si_optimizerows;
        int             si_simpleoptimizerrules;
        void*           si_blobrefbuf;
        long            si_stmtmaxtime;
        bool            si_logauditinfo;
        int             si_disableprintsqlinfo;
        void*           si_events;
        su_rbt_t*       si_wait_events;
        void*           si_proccache;
        void*           si_trigcache;
        int             (*si_trigexec)(
                            void* cd,
                            void* trans,
                            rs_entname_t* trigname,
                            char* trigstr,
                            rs_ttype_t* ttype,
                            rs_tval_t* old_tval,
                            rs_tval_t* new_tval,
                            void** p_ctx,
                            rs_err_t** p_errh);
        void            (*si_trigdone)(void* ctx);
        su_pa_t*        si_trignamestack;
        su_pa_t*        si_procnamestack;
        su_list_t*      si_seqidlist;
        rs_trend_t*     si_trend;
        rs_trend_t*     si_stmttrend;
        int             si_ignoretimeout;
        bool            si_disablerowspermessage;
        rs_sysi_tabletype_t si_stmttabletypes;      /* Bits for statement table types. */
        void*           si_trxcardininfo;
        SsSemT*         si_trxsem;
        bool            si_taskactivep;
        su_rbt_t**      si_funvaluerbt;
        SuTaskPrioT     si_prio;
        rs_eventnotifiers_t* si_eventnotifiers;
        bool            si_islocaluser;
        rs_sysi_connecttype_t si_connecttype;
        long            si_hsbinfo;
        bool            si_ishsbconfigured;
        bool            si_iscancelled;     /*statement cancellation flag*/
        void*           si_tbcur;
        int*            si_stepcountptr;
        void*           si_tbcurlist;
        bool            si_insidedbeatomicsection;
#ifdef SS_MME
        void*           si_mmememctx; /* for MME memory allocator */
        ss_debug(void*  si_accessed_mme_index;)
#endif /* SS_MME */
#ifdef SS_PMEM
        SsPmemCtxT*     si_memctx;
        sysi_memctxstat_t si_memctxstat;
#endif /* SS_PMEM */
#ifdef SS_SYNC
        sysi_syncinfo_t* si_cursyncinfo;
        su_rbt_t*        si_syncinforbt;
        dynva_t          si_password;
        long             si_syncsavemasterid;
        long             si_replicaid_onpropagate;

        bool             si_subscribewrite;
        bool             si_usehurc;
        bool             si_usehurc_force;

#endif /* SS_SYNC */

        long             (*si_istransactive_fun)(rs_sysi_t* cd, void* ctx);
        void*            si_istransactive_ctx;
        su_list_t*       si_deferredblobunlinklist;

        rs_defnode_t*   si_defaultnode; /* DEFAULT node (START command) */

        void*           si_locktran;
        int             si_locktran_link;
        SsMesT*         si_hsbwaitmes;

        int             si_tc_level;

        bool            (*si_getnewuniquemsgid_fun)(rs_sysi_t* cd,
                                                    ss_int8_t* p_result,
                                                    rs_err_t** p_errh);

        ss_debug(int    si_stepctrthrid;)
        su_rand_t       si_usrrand;
        su_rand_t       si_sysrand;
        int             si_hsbmergecount;
        int             si_hsbstmtcount;
        bool*           si_spnewplan;
        bool*           si_tabnewplan;
        su_list_t*      si_hsbsecactions;       /* HSB actions in secondary. */
        bool            si_isscrollsensitive;   /* scroll sensitive result set cursor*/
        bool            si_liveovercommit;
        bool            si_allowduplicatedelete;
        su_mes_t*       si_logwaitmes;
        su_meslist_t*   si_logwaitmeslist;
        su_ret_t*       si_logwaitrc;
        rs_signalinfo_t* si_signalinfo;
        uint            si_mergewrites;
        void*           si_crypto_par;
        int             si_encryption_level;
        su_bflag_t      si_flags;
        /* SS_TC_CLIENT - BEGIN */
        bool            si_islpid;
        ss_int8_t       si_lpid;
        bool            si_tc_changes;
        long            si_tc_optimisticlocktimeout;
        long            si_tc_locktimeout;
        long            si_tc_idletimeout;
        int             si_tc_isolation;
        int             si_tc_durability;
        int             si_tc_safeness;
        int             si_tc_readonly;
        uint            si_tc_joinpathspan;
        bool            si_tc_autocommit;
        bool            si_tc_write;
        bool            si_tc_rebalance;
        /* SS_TC_CLIENT - END */

        void*           si_bkeybuflist;
        ss_autotest_or_debug(int si_bkeybuflistlen;)
        ss_autotest_or_debug(int si_thrid;)
        SsMemCtxT*      si_qmemctx;
        int             si_dbactioncounter;
        bool            si_dbactionshared;
        ss_autotest_or_debug(bool si_staticcd;)
        ss_debug(char** si_task_callstack;)

        su_list_t*      si_seasemlist;
        void            (*si_logqueue_freefun)(void*);
        void*           si_logqueue;

}; /* rs_sysi_t */

typedef struct {
        long    tc_optimisticlocktimeout;
        long    tc_locktimeout;
        long    tc_idletimeout;
        long    tc_stmtmaxtime;
        int     tc_sortarraysize;
        int     tc_convertorstounions;
        int     tc_sortedgroupby;
        int     tc_optimizerows;
        int     tc_simpleoptimizerrules;
        int     tc_isolation;
        int     tc_durability;
        int     tc_safeness;
        int     tc_readonly;
        uint    tc_joinpathspan;
        bool    tc_autocommit;
        char*   tc_sqlifname;
        int     tc_sqlilevel;
} rs_tcinfo_t;

/***** INTERNAL ONLY END *****/

/* Define old name for compatibility with old source. */
#define rs_sysinfo_t rs_sysi_t

extern bool rs_sysi_shutdown;

rs_sysi_t* rs_sysi_init(
        void);

void rs_sysi_link(
        rs_sysi_t* sysi);

void rs_sysi_done(
        rs_sysi_t* sysi);

void rs_sysi_setconnecttype(
        rs_sysi_t* sysi,
        rs_sysi_connecttype_t type);

rs_sysi_connecttype_t rs_sysi_getconnecttype(
        rs_sysi_t* sysi);

void rs_sysi_insertauth(
        rs_sysi_t* sysi,
        rs_auth_t* auth);

SS_INLINE rs_auth_t* rs_sysi_auth(
        rs_sysi_t* sysi);

void rs_sysi_insertrbuf(
        rs_sysi_t* sysi,
        rs_rbuf_t* rbuf);

rs_rbuf_t* rs_sysi_rbuf(
        rs_sysi_t* sysi);

void  rs_sysi_insertuser(
        rs_sysi_t* sysi,
        void* user);

void* rs_sysi_user(
        rs_sysi_t* sysi);

void rs_sysi_setuserid(
        rs_sysi_t* sysi,
        int userid);

int rs_sysi_userid(
        rs_sysi_t* sysi);

void rs_sysi_setdbuserid(
        rs_sysi_t* sysi,
        long dbuserid);

long rs_sysi_dbuserid(
        rs_sysi_t* sysi);

void rs_sysi_setlocaluser(
        rs_sysi_t* sysi);

bool rs_sysi_islocaluser(
        rs_sysi_t* sysi);

void rs_sysi_inserttbcon(
        rs_sysi_t* sysi,
        void* tbcon);

void* rs_sysi_tbcon(
        rs_sysi_t* sysi);

void rs_sysi_insertdb(
        rs_sysi_t* sysi,
        void* db);

SS_INLINE void* rs_sysi_db(
        rs_sysi_t* sysi);

void rs_sysi_inserttabdb(
        rs_sysi_t* sysi,
        void* tabdb);

void* rs_sysi_tabdb(
        rs_sysi_t* sysi);

void rs_sysi_insertsqlinfo(
        rs_sysi_t* sysi,
        rs_sqlinfo_t* sqli);

rs_sqlinfo_t* rs_sysi_sqlinfo(
        rs_sysi_t* sysi);

void rs_sysi_insertrslinksemarray(
        rs_sysi_t* sysi,
        SsSemT** rslinksemarray);

void rs_sysi_rslinksem_enter(
        rs_sysi_t* sysi);

void rs_sysi_rslinksem_exit(
        rs_sysi_t* sysi);

SsSemT* rs_sysi_getrslinksem(
        rs_sysi_t* sysi);

void rs_sysi_settask_ex(
        rs_sysi_t* sysi,
        void* task);

void rs_sysi_removetaskif(
        rs_sysi_t* sysi,
        void* old_task);

void* rs_sysi_task(
        rs_sysi_t* sysi);

void rs_sysi_eventwaitwithtimeout(
        rs_sysi_t* sysi,
        int event,
        long timeout,
        void (*timeout_cbfun)(void*),
        void* timeout_cbctx);

void rs_sysi_seteventwaitwithtimeoutfun(
        rs_sysi_t* sysi,
        void (*waitfun)(void* task,
                        int event,
                        long timeout,
                        void (*_cbfun)(void*), void* timeout_cbctx));

void rs_sysi_setstartlockwaitfun(
        rs_sysi_t* sysi,
        void (*waitfun)(void* task),
        void (*waitfun_tmo)(void* task, long timeout_ms));

void rs_sysi_setlockwaitfun(
        rs_sysi_t* sysi,
        bool (*waitfun)(void* task));

void rs_sysi_setlockwakeupfun(
        rs_sysi_t* sysi,
        void (*wakeupfun)(void* task));

void rs_sysi_startlockwait(
        rs_sysi_t* sysi);

void rs_sysi_startlockwaitwithtimeout(
        rs_sysi_t* sysi,
        long timeout);

void rs_sysi_lockwakeup(
        rs_sysi_t* sysi);

bool rs_sysi_lockwait(
        rs_sysi_t* sysi);

void rs_sysi_setsignalfun(
        rs_sysi_t* sysi,
        void (*signalfun)(int userid, rs_signaltype_t signaltype));

void rs_sysi_signal(
        rs_sysi_t* sysi,
        rs_signaltype_t signaltype);

void rs_sysi_setstarttaskfun(
        rs_sysi_t* sysi,
        void (*taskstart)(
                char* task_name,
                su_task_fun_t task_fun,
                void* task_data,
                rs_sysi_t* task_cd));

bool rs_sysi_starttask(
        rs_sysi_t* sysi,
        char* task_name,
        su_task_fun_t taskfun,
        void* task_data,
        rs_sysi_t* task_cd);

void rs_sysi_setdateformat(
        rs_sysi_t* sysi,
        char* format);

void rs_sysi_settimeformat(
        rs_sysi_t* sysi,
        char* format);

void rs_sysi_settimestampformat(
        rs_sysi_t* sysi,
        char* format);

char* rs_sysi_dateformat(
        rs_sysi_t* sysi);

char* rs_sysi_timeformat(
        rs_sysi_t* sysi);

char* rs_sysi_timestampformat(
        rs_sysi_t* sysi);

void rs_sysi_setxsmgr(
        rs_sysi_t* sysi,
        void* xsmgr);

void* rs_sysi_xsmgr(
        rs_sysi_t* sysi);

void rs_sysi_setsqlinfolevel(
        rs_sysi_t* sysi,
        int level,
        char* fname);

void rs_sysi_setsortarraysize(
        rs_sysi_t* sysi,
        int size);

void rs_sysi_setconvertorstounions(
        rs_sysi_t* sysi,
        int count);

void rs_sysi_setsortedgroupby(
        rs_sysi_t* sysi,
        int sortedgroupby);

void rs_sysi_setoptimizerows(
        rs_sysi_t* sysi,
        int rows);

void rs_sysi_setsimpleoptimizerrules(
        rs_sysi_t* sysi,
        int value);

int rs_sysi_getsimpleoptimizerrules(
        rs_sysi_t* sysi);

int rs_sysi_sqlinfolevel(
        rs_sysi_t* sysi,
        bool sqlp);

bool rs_sysi_useindexcursorreset(
        rs_sysi_t* sysi);

bool rs_sysi_uselateindexplan(
        rs_sysi_t* sysi);

int rs_sysi_sortarraysize(
        rs_sysi_t* sysi);

int rs_sysi_convertorstounions(
        rs_sysi_t* sysi);

bool rs_sysi_allowduplicateindex(
        rs_sysi_t* sysi);

int rs_sysi_sortedgroupby(
        rs_sysi_t* sysi);

SS_INLINE int rs_sysi_optimizerows(
        rs_sysi_t* sysi);

bool rs_sysi_simpleoptimizerrules(
        rs_sysi_t* sysi,
        double ntuples_in_table);

void rs_sysi_setstmttabnameinfo(
        rs_sysi_t* sysi,
        su_rbt_t* rbt);

void rs_sysi_addstmttabnameinfo(
        rs_sysi_t* sysi,
        rs_entname_t* en);

void rs_sysi_stmttabnameinfo_setestprinted(
        rs_sysi_t* sysi,
        rs_entname_t* en);

void rs_sysi_printsqlinfo(
        rs_sysi_t* sysi,
        int level,
        char* str);

void rs_sysi_printsqlinfo_force(
        rs_sysi_t* sysi,
        int level,
        char* str);

void rs_sysi_setdisableprintsqlinfo(
        rs_sysi_t* sysi,
        bool disablep);

void* rs_sysi_blobrefbuf(
        rs_sysi_t* sysi);

void rs_sysi_setblobrefbuf(
        rs_sysi_t* sysi,
        void* blobrefbuf);

long rs_sysi_stmtmaxtime(
        rs_sysi_t* sysi);

void rs_sysi_setstmtmaxtime(
        rs_sysi_t* sysi,
        long maxtime);

void rs_sysi_setlogauditinfo(
        rs_sysi_t* sysi,
        bool b);

SS_INLINE bool rs_sysi_logauditinfo(
        rs_sysi_t* sysi);

void rs_sysi_setevents(
        rs_sysi_t* sysi,
        void* events);

void* rs_sysi_events(
        rs_sysi_t* sysi);

void rs_sysi_insert_wait_events(rs_sysi_t* sysi,
                                su_rbt_t* wait_events);

su_rbt_t* rs_sysi_wait_events(rs_sysi_t* sysi);

void rs_sysi_setproccache(
        rs_sysi_t* sysi,
        void* proccache);

void* rs_sysi_proccache(
        rs_sysi_t* sysi);

void rs_sysi_settrigcache(
        rs_sysi_t* sysi,
        void* trigcache);

void* rs_sysi_trigcache(
        rs_sysi_t* sysi);

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
            rs_err_t** p_errh));

int rs_sysi_trigexec(
        rs_sysi_t* sysi,
        void* trans,
        rs_entname_t* trigname,
        char* trigstr,
        rs_ttype_t* ttype,
        rs_tval_t* old_tval,
        rs_tval_t* new_tval,
        void** p_ctx,
        rs_err_t** p_errh);

void rs_sysi_settrigdonefun(
        rs_sysi_t* sysi,
        void (*trigdone)(void* ctx));

void rs_sysi_trigdone(
        rs_sysi_t* sysi,
        void* ctx);

bool rs_sysi_trigpush(
        rs_sysi_t* sysi,
        rs_entname_t* trigname);

void rs_sysi_trigpop(
        rs_sysi_t* sysi);

int rs_sysi_trigcount(
        rs_sysi_t* sysi);

char* rs_sysi_trigname(
        rs_sysi_t* sysi,
        int index);

char* rs_sysi_trigschema(
        rs_sysi_t* sysi,
        int index);

bool rs_sysi_procpush(
        rs_sysi_t* sysi,
        rs_entname_t* procname);

void rs_sysi_procpop(
        rs_sysi_t* sysi);

int rs_sysi_proccount(
        rs_sysi_t* sysi);

char* rs_sysi_procname(
        rs_sysi_t* sysi,
        int index);

char* rs_sysi_procschema(
        rs_sysi_t* sysi,
        int index);

void rs_sysi_addseqid(
        rs_sysi_t* sysi,
        long seqid);

void rs_sysi_setseqidlist(
        rs_sysi_t* sysi,
        su_list_t* seqidlist);

rs_trend_t* rs_sysi_gettrend(
        rs_sysi_t* sysi);

SS_INLINE rs_trend_t* rs_sysi_getstmttrend(
        rs_sysi_t* sysi);

void rs_sysi_addmergewrites(
        rs_sysi_t* sysi,
        uint mergewrites);

ulong rs_sysi_getmergewrites(
        rs_sysi_t* sysi);

void rs_sysi_clearmergewrites(
        rs_sysi_t* sysi);

void rs_sysi_setflag(
        rs_sysi_t* cd,
        int flag);

void rs_sysi_clearflag(
        rs_sysi_t* cd,
        int flag);

bool rs_sysi_testflag(
        rs_sysi_t* cd,
        int flag);

#ifdef SS_PMEM

void rs_sysi_staticinstallpmem(
        void (*ctxdone)(SsPmemCtxT*),
        void (*ctxsweep)(SsPmemCtxT*,bool),
        void (*ctxinstall)(SsPmemCtxT**),
        void (*ctxuninstall)(SsPmemCtxT*));

void rs_sysi_enablememctx(
        rs_sysi_t* sysi);

void rs_sysi_installmemctx(
        rs_sysi_t* sysi);

void rs_sysi_uninstallmemctx(
        rs_sysi_t* sysi);

#endif /* SS_PMEM */

void rs_sysi_setignoretimeout(
        rs_sysi_t* sysi,
        bool ignorep);

bool rs_sysi_ignoretimeout(
        rs_sysi_t* sysi);

#ifdef SS_SYNC

bool rs_sysi_setsynccatalog(
        rs_sysi_t* sysi,
        char* catalog);

bool rs_sysi_dropsynccatalog(
        rs_sysi_t* sysi,
        char* catalog);

void rs_sysi_setsyncmaster(
        rs_sysi_t* sysi,
        bool valuep);

void rs_sysi_setsyncreplica(
        rs_sysi_t* sysi,
        bool valuep);

void rs_sysi_setsyncnode(
        rs_sysi_t* sysi,
        char* syncnode,
        SsTimeT nodetime);

bool rs_sysi_issyncmaster(
        rs_sysi_t* sysi);

bool rs_sysi_issyncreplica(
        rs_sysi_t* sysi);

char* rs_sysi_getsyncnode(
        rs_sysi_t* sysi);

SsTimeT rs_sysi_getnodetime(
        rs_sysi_t* sysi);

void rs_sysi_setsyncmasterid(
        rs_sysi_t* sysi,
        long syncmasterid);

long rs_sysi_getsyncmasterid(
        rs_sysi_t* sysi);

void rs_sysi_setsync_propagatereplicaid(
        rs_sysi_t* sysi,
        long replicaid);

long rs_sysi_getsync_propagatereplicaid(
        rs_sysi_t* sysi);

bool rs_sysi_subscribe_write(
        rs_sysi_t* sysi);

void rs_sysi_setsubscribe_write(
        rs_sysi_t* sysi,
        bool subscribewrite);

bool rs_sysi_usehurc(
        rs_sysi_t* sysi);

void rs_sysi_setusehurc(
        rs_sysi_t* sysi,
        bool usehurc);

void rs_sysi_setusehurc_force(
        rs_sysi_t* sysi,
        bool usehurc);

/* VOYAGER BEGIN */

void rs_sysi_setsyncsavemasterid(
        rs_sysi_t* sysi,
        long syncmasterid);

long rs_sysi_getsyncsavemasterid(
        rs_sysi_t* sysi);

void rs_sysi_setlocalsyncid(
        rs_sysi_t* sysi,
        long syncid);

long rs_sysi_getlocalsyncid(
        rs_sysi_t* sysi);

/* VOYAGER END */

su_list_t* rs_sysi_getmsgnamelocklist(
        rs_sysi_t* sysi);

char* rs_sysi_getsynccatalog(
        rs_sysi_t* sysi);

void rs_sysi_setsyncmsgholder(
        rs_sysi_t* sysi,
        void* syncmsg_rbt);

void* rs_sysi_getsyncmsgholder(
        rs_sysi_t* sysi);

void rs_sysi_setbboard(
        rs_sysi_t* sysi,
        rs_bboard_t* bboard);

rs_bboard_t* rs_sysi_getbboard(
        rs_sysi_t* sysi);

void rs_sysi_setipub(
        rs_sysi_t* sysi,
        void* ipub);

void* rs_sysi_getipub(
        rs_sysi_t* sysi);

void rs_sysi_setsyncusername(
        rs_sysi_t* sysi,
        char* username,
        va_t* password);

bool rs_sysi_syncusername(
        rs_sysi_t* sysi,
        char** p_username,
        va_t** p_password);

void rs_sysi_setpassword(
        rs_sysi_t* sysi,
        va_t* password);

va_t* rs_sysi_password(
        rs_sysi_t* sysi);

void rs_sysi_setpropertystore(
        rs_sysi_t* sysi,
        rs_bboard_t* propertystore);

rs_bboard_t* rs_sysi_getpropertystore(
        rs_sysi_t* sysi);

void rs_sysi_setdefaultpropagatewherestr(
        rs_sysi_t* sysi,
        char* wherestr);

char* rs_sysi_getdefaultpropagatewherestr(
        rs_sysi_t* sysi);

#endif /* SS_SYNC */

void rs_sysi_setdisablerowspermessage(
        rs_sysi_t* sysi,
        bool disable);

bool rs_sysi_isdisablerowspermessage(
        rs_sysi_t* sysi);

void rs_sysi_setstmttabletypes(
        rs_sysi_t* sysi,
        rs_sysi_tabletype_t type);

void rs_sysi_clearstmttabletypes(
        rs_sysi_t* sysi);

rs_sysi_tabletype_t rs_sysi_getstmttabletypes(
        rs_sysi_t* sysi);

void* rs_sysi_gettrxcardininfo(
        rs_sysi_t* sysi);

void rs_sysi_settrxcardininfo(
        rs_sysi_t* sysi,
        void* trxcardininfo);

SsSemT* rs_sysi_gettrxsem(
        rs_sysi_t* sysi);

void rs_sysi_settaskactive(
        rs_sysi_t* sysi,
        bool activep);

bool rs_sysi_istaskactive(
        rs_sysi_t* sysi);

su_rbt_t** rs_sysi_getfunvalues(
        rs_sysi_t* sysi);

void rs_sysi_setfunvalues(
        rs_sysi_t* sysi,
        su_rbt_t** p_rbt);

void rs_sysi_setprio(
        rs_sysi_t* sysi,
        SuTaskPrioT prio);

SuTaskPrioT rs_sysi_getprio(
        rs_sysi_t* sysi);

rs_eventnotifiers_t* rs_sysi_geteventnotifiers(
        rs_sysi_t* sysi);

void rs_sysi_seteventnotifiers(
        rs_sysi_t* sysi,
        rs_eventnotifiers_t* events);

void rs_sysi_sethsbinfo(
        rs_sysi_t* sysi,
        long info);

long rs_sysi_gethsbinfo(
        rs_sysi_t* sysi);

bool rs_sysi_ishsbconfigured(
        rs_sysi_t* sysi);

void rs_sysi_sethsbconfigured(
        rs_sysi_t* sysi,
        bool isconfigured);

SS_INLINE void rs_sysi_setcancelled(
        rs_sysi_t* sysi,
        bool cancelp);

bool rs_sysi_iscancelled(
        rs_sysi_t* sysi);

void* rs_sysi_tbcur(
        rs_sysi_t* sysi);

void rs_sysi_settbcur(
        rs_sysi_t* sysi,
        void* cur);

SS_INLINE int rs_sysi_decstepctr(
        rs_sysi_t* sysi);

bool rs_sysi_sqlyieldrequired(
        rs_sysi_t* sysi);

int rs_sysi_getstepctr(
        rs_sysi_t* sysi);

SS_INLINE void rs_sysi_setstepctr(
        rs_sysi_t* sysi,
        int* p_stepctr);

void* rs_sysi_gettbcurlist(
        rs_sysi_t* sysi);

void rs_sysi_settbcurlist(
        rs_sysi_t* sysi,
        void* tbcurlist);

void rs_sysi_setinsidedbeatomicsection(
        rs_sysi_t* sysi,
        bool insidep);

bool rs_sysi_isinsidedbeatomicsection(
        rs_sysi_t* sysi);

void rs_sysi_set_istransactive_fun(
        rs_sysi_t* sysi,
        void* ctx,
        long (*isactivefun)(rs_sysi_t* cd, void* ctx));

long rs_sysi_istransactive(
        rs_sysi_t* sysi);

void rs_sysi_setdefnode(
        rs_sysi_t* sysi,
        rs_defnode_t* defnode);

rs_defnode_t* rs_sysi_getdefnode(
        rs_sysi_t* sysi);

void rs_sysi_setlocktran(
        rs_sysi_t* sysi,
        void* locktran);

void* rs_sysi_getlocktran(
        rs_sysi_t* sysi);

void rs_sysi_locktran_link(
        rs_sysi_t* sysi,
        void* locktran);

void rs_sysi_locktran_unlink(
        rs_sysi_t* sysi,
        void* locktran);

void rs_sysi_sethsbwaitmes(
        rs_sysi_t* sysi,
        SsMesT* hsbwaitmes);

SsMesT* rs_sysi_gethsbwaitmes(
        rs_sysi_t* sysi);

void rs_sysi_set_uniquemsgid_fun(
        rs_sysi_t* sysi,
        bool (*getnewuniqueidfun)(rs_sysi_t*, ss_int8_t*,rs_err_t**));

bool rs_sysi_getnewuniquemsgid( rs_sysi_t* sysi, ss_int8_t* p_result, rs_err_t** p_errh);

void* rs_sysi_getistransactive_ctx( rs_sysi_t* sysi );

void rs_sysi_setdeferredblobunlinklist(
        rs_sysi_t* cd,
        su_list_t* deferredblobunlinklist);

su_list_t* rs_sysi_getdeferredblobunlinklist(
        rs_sysi_t* cd);

#ifdef SS_MME
void* rs_sysi_getmmememctx(rs_sysi_t* cd);
void rs_sysi_setmmememctx(rs_sysi_t* cd, void* ctx);
/* SS_INLINE void rs_sysi_set_accessed_mmeindex(rs_sysi_t* cd, void* index); */
#endif /* SS_MME */

ss_int4_t rs_sysi_sysrand(rs_sysi_t* cd);
void rs_sysi_usrrandseed(rs_sysi_t* cd, ss_int4_t seed);
ss_int4_t rs_sysi_usrrand(rs_sysi_t* cd);


void rs_sysi_inchsbmergecount(
        rs_sysi_t* sysi);

int rs_sysi_gethsbmergecount(
        rs_sysi_t* sysi);

void rs_sysi_inchsbstmtcount(
        rs_sysi_t* sysi);

int rs_sysi_gethsbstmtcount(
        rs_sysi_t* sysi);

su_list_t* rs_sysi_gethsbsecactions(
        rs_sysi_t* sysi);

void rs_sysi_sethsbsecactions(
        rs_sysi_t* sysi,
        su_list_t* list);

SS_INLINE void rs_sysi_setspnewplanptr(
        rs_sysi_t*  sysi,
        bool*       p_newplan);

bool* rs_sysi_getspnewplanptr(
        rs_sysi_t*  sysi);

bool rs_sysi_isscrollsensitive(
        rs_sysi_t*  sysi);

void rs_sysi_setscrollsensitive(
        rs_sysi_t*  sysi,
        bool        scrollsensitive);

void rs_sysi_settabnewplanptr(
        rs_sysi_t*  sysi,
        bool*       p_newplan);

bool* rs_sysi_gettabnewplanptr(
        rs_sysi_t*  sysi);

bool rs_sysi_getliveovercommit(
        rs_sysi_t*  sysi);

void rs_sysi_setliveovercommit(
        rs_sysi_t*  sysi,
        bool        liveovercommit);

bool rs_sysi_getallowduplicatedelete(
        rs_sysi_t*  sysi);

void rs_sysi_setallowduplicatedelete(
        rs_sysi_t*  sysi,
        bool        allowduplicatedelete);

su_mes_t* rs_sysi_getlogwaitmes(
        rs_sysi_t*  sysi);

void rs_sysi_removelogwaitmes(
        rs_sysi_t*  sysi);

void rs_sysi_setlogwaitmes(
        rs_sysi_t*      sysi,
        su_meslist_t*   meslist,
        su_mes_t*       mes);

su_ret_t* rs_sysi_getlogwaitrc(
        rs_sysi_t*  sysi);

void rs_sysi_setlogwaitrc(
        rs_sysi_t*  sysi,
        su_ret_t*   rc);

SS_INLINE void* rs_sysi_getbkeybuf(
        rs_sysi_t* sysi);

SS_INLINE void rs_sysi_putbkeybuf(
        rs_sysi_t* sysi,
        void* bkeybuf);

SS_INLINE SsSemT* rs_sysi_givesearchsem(
        rs_sysi_t* sysi);

SS_INLINE void rs_sysi_insertsearchsem(
        rs_sysi_t* sysi,
        SsSemT* sem);

SS_INLINE void* rs_sysi_getlogqueue(
        rs_sysi_t* sysi);

SS_INLINE void rs_sysi_setlogqueue(
        rs_sysi_t* sysi,
        void* queue,
        void  (*freefun)(void*));

#ifdef SS_DEBUG

SsMemCtxT* rs_sysi_getqmemctx(
        rs_sysi_t* sysi);

#endif /* SS_DEBUG */

void rs_sysi_setqmemctx(
        rs_sysi_t* sysi);

void rs_sysi_clearqmemctx(
        rs_sysi_t* sysi);

void rs_sysi_setsignalinfo(
        rs_sysi_t* cd,
        rs_signalinfo_t* signalinfo);

rs_signalinfo_t* rs_sysi_getsignalinfo(
        rs_sysi_t* cd);

void *rs_sysi_getcryptopar(
         rs_sysi_t*  sysi);

void rs_sysi_setcryptopar(
        rs_sysi_t*  sysi,
        void*       cryptopar);

long rs_sysi_encryption_level(
        rs_sysi_t*  sysi);

void rs_sysi_set_encryption_level(
        rs_sysi_t*  sysi,
        int         level);

bool rs_sysi_iscluster(
        rs_sysi_t*  sysi);

int rs_sysi_tc_level(
        rs_sysi_t*  sysi);

void rs_sysi_set_tc_level(
        rs_sysi_t*  sysi,
        int         tc_level);

void rs_sysi_setlpid_int8(
        rs_sysi_t*  sysi,
        ss_int8_t   lpid);

bool rs_sysi_getlpid_int8(
        rs_sysi_t*  sysi,
        ss_int8_t*  p_lpid);

void rs_sysi_clearlpid(
        rs_sysi_t*  sysi);

void rs_sysi_tc_setoptimisticlocktimeout(
        rs_sysi_t*  sysi,
        long val);

void rs_sysi_tc_setlocktimeout(
        rs_sysi_t*  sysi,
        long        val);

void rs_sysi_tc_setidletimeout(
        rs_sysi_t*  sysi,
        long        val);

SS_INLINE void rs_sysi_tc_setisolation(
        rs_sysi_t*  sysi,
        int         option);

void rs_sysi_tc_setdurability(
        rs_sysi_t*  sysi,
        int         option);

void rs_sysi_tc_setsafeness(
        rs_sysi_t*  sysi,
        int         option);

void rs_sysi_tc_setreadonly(
        rs_sysi_t*  sysi,
        int         option);

void rs_sysi_tc_setjoinpathspan(
        rs_sysi_t*  sysi,
        uint        val);

uint rs_sysi_tc_joinpathspan(
        rs_sysi_t*  sysi);

void rs_sysi_tc_setautocommit(
        rs_sysi_t*  sysi,
        bool        autocommit);

bool rs_sysi_get_tc_changes(
        rs_sysi_t*  sysi);

void rs_sysi_forget_tc_changes(
        rs_sysi_t*  sysi);

bool rs_sysi_get_tc_write(
        rs_sysi_t* sysi);

bool rs_sysi_get_tc_rebalance(
        rs_sysi_t*  sysi);

void rs_sysi_set_tc_write(
        rs_sysi_t*  sysi,
        bool write);

void rs_sysi_set_tc_rebalance(
        rs_sysi_t*  sysi,
        bool rebalance);

rs_tcinfo_t* rs_tcinfo_init(
        rs_sysi_t*  sysi);

void rs_tcinfo_done(
        rs_tcinfo_t* tcinfo);

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)

void rs_sysi_setthrid(
        rs_sysi_t* sysi);

void rs_sysi_clearthrid(
        rs_sysi_t* sysi);

bool rs_sysi_testthrid(
        rs_sysi_t* sysi);

#endif /* defined(SS_DEBUG) || defined(AUTOTEST_RUN) */

#define rs_sysi_enterdbaction(s)        ((s)->si_dbactioncounter++ == 0)
#define rs_sysi_exitdbaction(s)         ((s)->si_dbactioncounter-- == 1)
#define rs_sysi_setdbactionshared(s, p) ((s)->si_dbactionshared = (p))
#define rs_sysi_isdbactionshared(s)     ((s)->si_dbactionshared)
#define rs_sysi_copydbactioncounter(t, s) ((t)->si_dbactioncounter = (s)->si_dbactioncounter)
#define rs_sysi_copydbactionshared(t, s)  ((t)->si_dbactionshared = (s)->si_dbactionshared)
#define rs_sysi_setstaticcd(t)            ((t)->si_staticcd = TRUE)
#define rs_sysi_clearstaticcd(t)          ((t)->si_staticcd = FALSE)
#define rs_sysi_isstaticcd(t)             ((t) != NULL && (t)->si_staticcd)

#define rs_sysi_getdisableprintsqlinfo(cd)      ((cd)->si_disableprintsqlinfo > 0)

/* MACROS FOR PRODUCT VERSION */

#define _RS_SYSI_SETDISABLEROWSPERMESSAGE(s, d) ((s)->si_disablerowspermessage = (d))
#define _RS_SYSI_ISDISABLEROWSPERMESSAGE(s)     ((s)->si_disablerowspermessage)
#define _RS_SYSI_SETSTMTTABLETYPES(s, d)        ((s)->si_stmttabletypes |= (d))
#define _RS_SYSI_CLEARSTMTTABLETYPES(s)         ((s)->si_stmttabletypes = 0)
#define _RS_SYSI_GETSTMTTABLETYPES(s)           ((s)->si_stmttabletypes)
#define _RS_SYSI_SETTRXCARDININFO(s, i)         ((s)->si_trxcardininfo = (i))
#define _RS_SYSI_GETTRXCARDININFO(s)            ((s)->si_trxcardininfo)
#define _RS_SYSI_SETFUNVALUES(s, r)             ((s)->si_funvaluerbt = (r))
#define _RS_SYSI_ISINSIDEDBEATOMICSECTION(s)    ((s) != NULL ? (s)->si_insidedbeatomicsection : FALSE)
#define _RS_SYSI_USERID(s)                      ((s) != NULL ? (s)->si_userid : -1)
#define _RS_SYSI_USER(s)                        ((s)->si_dbuser)
#define _RS_SYSI_SETTASKACTIVE(s,a)             if ((s) != NULL) (s)->si_taskactivep = (a)
#define _RS_SYSI_GETPRIO(s)                     ((s) != NULL ? (s)->si_prio : SU_TASK_PRIO_DEFAULT)
#define _RS_SYSI_GETLOGWAITMES(s)               ((s)->si_logwaitmes)
#define _RS_SYSI_GETLOGWAITRC(s)                ((s)->si_logwaitrc)
#define _RS_SYSI_GETLOCKTRAN(s)                 ((s)->si_locktran)
#define _RS_SYSI_ADDMERGEWRITES(s,n)            (s)->si_mergewrites += (n)
#define _RS_SYSI_GETMERGEWRITES(s)              ((s)->si_mergewrites)
#define _RS_SYSI_GETQMEMCTX(s)                  ((s)->si_qmemctx)
#define _RS_SYSI_SQLINFO(s)                     ((s) != NULL ? (s)->si_sqli : NULL)
#define _RS_SYSI_SETFLAG(s,f)                   SU_BFLAG_SET((s)->si_flags, f)
#define _RS_SYSI_CLEARFLAG(s,f)                 SU_BFLAG_CLEAR((s)->si_flags, f)
#define _RS_SYSI_TESTFLAG(s,f)                  SU_BFLAG_TEST((s)->si_flags, f)
#define _RS_SYSI_GETTREND(s)                    ((s)->si_trend)

#ifdef SS_MME
#define _RS_SYSI_GETMMEMEMCTX_(cd)      ((cd)->si_mmememctx)
#define _RS_SYSI_SETMMEMEMCTX_(cd,ctx)  \
        do { (cd)->si_mmememctx = (ctx); } while (FALSE)
        extern void* si_mmememctx; /* for MME memory allocator */
#endif /* SS_MME */

#ifndef SS_DEBUG
#define rs_sysi_setfunvalues             _RS_SYSI_SETFUNVALUES
#define rs_sysi_setdisablerowspermessage _RS_SYSI_SETDISABLEROWSPERMESSAGE
#define rs_sysi_isdisablerowspermessage  _RS_SYSI_ISDISABLEROWSPERMESSAGE
#define rs_sysi_setstmttabletypes        _RS_SYSI_SETSTMTTABLETYPES
#define rs_sysi_clearstmttabletypes      _RS_SYSI_CLEARSTMTTABLETYPES
#define rs_sysi_getstmttabletypes        _RS_SYSI_GETSTMTTABLETYPES
#define rs_sysi_settrxcardininfo         _RS_SYSI_SETTRXCARDININFO
#define rs_sysi_gettrxcardininfo         _RS_SYSI_GETTRXCARDININFO
#define rs_sysi_isinsidedbeatomicsection _RS_SYSI_ISINSIDEDBEATOMICSECTION
#define rs_sysi_userid                   _RS_SYSI_USERID
#define rs_sysi_user                     _RS_SYSI_USER
#define rs_sysi_settaskactive            _RS_SYSI_SETTASKACTIVE
#define rs_sysi_getprio                  _RS_SYSI_GETPRIO
#define rs_sysi_getlogwaitmes            _RS_SYSI_GETLOGWAITMES
#define rs_sysi_getlogwaitrc             _RS_SYSI_GETLOGWAITRC
#define rs_sysi_getlocktran              _RS_SYSI_GETLOCKTRAN
#define rs_sysi_addmergewrites           _RS_SYSI_ADDMERGEWRITES
#define rs_sysi_getmergewrites           _RS_SYSI_GETMERGEWRITES
#define rs_sysi_getqmemctx               _RS_SYSI_GETQMEMCTX
#define rs_sysi_sqlinfo                  _RS_SYSI_SQLINFO
#define rs_sysi_setflag                  _RS_SYSI_SETFLAG
#define rs_sysi_clearflag                _RS_SYSI_CLEARFLAG
#define rs_sysi_testflag                 _RS_SYSI_TESTFLAG
#define rs_sysi_gettrend                 _RS_SYSI_GETTREND
#ifdef SS_MME
#define rs_sysi_getmmememctx             _RS_SYSI_GETMMEMEMCTX_
#define rs_sysi_setmmememctx             _RS_SYSI_SETMMEMEMCTX_
#endif /* SS_MME */

#endif /* !SS_DEBUG */

#ifdef SS_DEBUG
#define rs_sysi_settask                  rs_sysi_settask_ex
#else
#define rs_sysi_settask(cd, t)           do { \
                                             if ((cd) != NULL && (t) != NULL) { \
                                                 (cd)->si_task = (t); \
                                             } else { \
                                                rs_sysi_settask_ex(cd, t); \
                                             } \
                                         }while (0)
#endif

#define rs_sysi_qmemctxalloc(cd, s)     SsMemAlloc(s)
#define rs_sysi_qmemctxfree(cd, p)      SsMemFree(p)

#if defined(RS0SYSI_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              rs_sysi_auth
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
SS_INLINE rs_auth_t* rs_sysi_auth(rs_sysi_t* sysi)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        return(sysi->si_auth);
}

/*##**********************************************************************\
 *
 *              rs_sysi_optimizerows
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
SS_INLINE int rs_sysi_optimizerows(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        if (sysi->si_optimizerows != -1) {
            return(sysi->si_optimizerows);
        } else {
            return(rs_sqli_getoptn(sysi->si_sqli));
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_logauditinfo
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
SS_INLINE bool rs_sysi_logauditinfo(
        rs_sysi_t* sysi)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            return(sysi->si_logauditinfo);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_getstmttrend
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
SS_INLINE rs_trend_t* rs_sysi_getstmttrend(
        rs_sysi_t* sysi)
{
        ss_bassert(sysi != NULL);

        return(sysi->si_stmttrend);
}

SS_INLINE void rs_sysi_setcancelled(
        rs_sysi_t* sysi,
        bool cancelp)
{
        CHK_SYSI(sysi);

        sysi->si_iscancelled = cancelp;
}

SS_INLINE int rs_sysi_decstepctr(
        rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        if (sysi->si_stepcountptr != NULL) {
            if (rs_sysi_shutdown) {
                *sysi->si_stepcountptr = 0;
                return(0);
            }
            ss_dassert(sysi->si_stepctrthrid == (int)SsThrGetid());
            return ((*sysi->si_stepcountptr)--);
        } else {
            return(0);
        }
}

SS_INLINE void rs_sysi_setstepctr(
        rs_sysi_t* sysi,
        int* p_stepctr)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            ss_dassert(p_stepctr == NULL || *p_stepctr >= 0);

            ss_debug(sysi->si_stepctrthrid = SsThrGetid());
            sysi->si_stepcountptr = p_stepctr;
        }
}

SS_INLINE void rs_sysi_setspnewplanptr(
        rs_sysi_t*  sysi,
        bool*       p_newplan)
{
        CHK_SYSI(sysi);

        sysi->si_spnewplan = p_newplan;
}

SS_INLINE void* rs_sysi_getbkeybuf(
        rs_sysi_t* sysi)
{
        void* bkeybuf;

        CHK_SYSI(sysi);
        ss_aassert(rs_sysi_testthrid(sysi));

        bkeybuf = sysi->si_bkeybuflist;
        if (bkeybuf != NULL) {
            sysi_listnode_t* sl;

            ss_dprintf_1(("rs_sysi_getbkeybuf:bkeybuflistlen=%d\n", sysi->si_bkeybuflistlen));
            sl = (sysi_listnode_t*)bkeybuf;
            CHK_SYSILISTNODE(sl);
            sysi->si_bkeybuflist = sl->sl_next;
            sl->sl_chk = (rs_check_t)0;
            ss_aassert(sysi->si_bkeybuflistlen > 0);
            ss_autotest_or_debug(sysi->si_bkeybuflistlen--);
        }
        return(bkeybuf);
}

SS_INLINE void rs_sysi_putbkeybuf(
        rs_sysi_t* sysi,
        void* bkeybuf)
{
        sysi_listnode_t* sl;

        CHK_SYSI(sysi);
        ss_dprintf_1(("rs_sysi_putbkeybuf:bkeybuflistlen=%d\n", sysi->si_bkeybuflistlen));
        ss_aassert(rs_sysi_testthrid(sysi));

        sl = (sysi_listnode_t*)bkeybuf;
        sl->sl_next = sysi->si_bkeybuflist;
        sl->sl_chk = RSCHK_SYSILISTNODE;
        sysi->si_bkeybuflist = sl;
        ss_autotest_or_debug(sysi->si_bkeybuflistlen++);
        ss_aassert(sysi->si_bkeybuflistlen < 10000);
}

SS_INLINE SsSemT* rs_sysi_givesearchsem(
        rs_sysi_t* sysi)
{
        SsSemT* sem;
        su_list_node_t* n;

        CHK_SYSI(sysi);

        n = su_list_first(sysi->si_seasemlist);
        if (n != NULL) {
            sem = (SsSemT*)su_list_remove_nodatadel(sysi->si_seasemlist, n);
        } else {
            sem = SsSemCreateLocal(SS_SEMNUM_DBE_INDEX_SEARCHACTIVE);
        }

        return(sem);
}

SS_INLINE void rs_sysi_insertsearchsem(
        rs_sysi_t* sysi,
        SsSemT* sem)
{
        CHK_SYSI(sysi);

        su_list_insertfirst(sysi->si_seasemlist, sem);
}

SS_INLINE void* rs_sysi_getlogqueue(rs_sysi_t* sysi)
{
        CHK_SYSI(sysi);

        return(sysi->si_logqueue);
}

SS_INLINE void rs_sysi_setlogqueue(
        rs_sysi_t* sysi,
        void* queue,
        void  (*freefun)(void*))
{
        CHK_SYSI(sysi);
        ss_dassert(sysi->si_logqueue == NULL);

        sysi->si_logqueue = queue;
        sysi->si_logqueue_freefun = freefun;
}

SS_INLINE void rs_sysi_tc_setisolation(
        rs_sysi_t*  sysi,
        int         val)
{
        if (sysi != NULL) {
            CHK_SYSI(sysi);
            if (sysi->si_tc_isolation != val) {
                sysi->si_tc_isolation = val;
                sysi->si_tc_changes = TRUE;
            }
        }
}

/*##**********************************************************************\
 *
 *              rs_sysi_db
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
SS_INLINE void* rs_sysi_db(rs_sysi_t* sysi)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);

        return(sysi->si_db);
}
#if defined(SS_DEBUG) && defined(SS_MME)
SS_INLINE void rs_sysi_set_accessed_mmeindex(
        rs_sysi_t* sysi,
        void* index)
{
        ss_dassert(sysi != NULL);
        CHK_SYSI(sysi);
        if (sysi->si_accessed_mme_index != index) {
            if (sysi->si_accessed_mme_index != NULL) {
                ss_assert(index == NULL);
            } else if (index != NULL) {
                ss_assert(sysi->si_accessed_mme_index == NULL);
            }
            sysi->si_accessed_mme_index = index;
        }
}
#endif /* SS_DEBUG && SS_MME */
#endif /* defined(RS0SYSI_C) || defined(SS_USE_INLINE) */

#endif /* RS0SYSI_H */
