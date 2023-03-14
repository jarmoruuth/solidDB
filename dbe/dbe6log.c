/*************************************************************************\
**  source       * dbe6log.c
**  directory    * dbe
**  description  * Logical logging subsystem
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

This object implements logical log, which will forward the
operations into physical log subsystems (transaction and
hotstandby log subsystems).

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

This class is re-entrant. However, you must guarantee that
logical operations which are physically multiple methods calls to this
class are protected by a mutex in order to guarantee that ordering is
preserved.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#ifndef SS_NOLOGGING
#define DBE_LAZYLOG_OPT

#include <ssc.h>
#include <sssem.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssstring.h>
#include <sssprint.h>
#include <sslimits.h>
#include <ssfile.h>
#include <ssltoa.h>
#include <sspmon.h>
#include <ssthread.h>

#include <su0icvt.h>
#include <su0vfil.h>
#include <su0svfil.h>
#include <su0cfgst.h>
#include <su0gate.h>
#include <su0mesl.h>

#include <ui0msg.h>

#include "dbe9type.h"
#ifdef DBE_LOGORDERING_FIX
#include "dbe0db.h"
#endif /* DBE_LOGORDERING_FIX */
#include "dbe0type.h"
#include "dbe6bnod.h"
#include "dbe7logf.h"
#include "dbe6log.h"
#include "dbe0hsbg2.h"

#ifdef SS_MME
#ifndef SS_MYSQL
#ifdef SS_MMEG2
#include <../mmeg2/mme0rval.h>
#else
#include <../mme/mme0rval.h>
#endif
#endif
#endif /* SS_MME */

struct dbe_log_st {
    ss_debug(dbe_chk_t  log_chk;)       /* Check field */
    dbe_logfile_t*      log_logfile;    /* Transaction log */
    bool                log_transform;
    int                 log_final_checkpoint; /* 0=No, 1=Pending, 2=Started */

#ifdef SS_HSBG2
    dbe_log_instancetype_t log_instancetype;
    void*               log_instance_ctx;
    dbe_hsbg2_t*        log_hsbsvc;
#endif /* SS_HSBG2 */

#ifdef DBE_LOGORDERING_FIX
    dbe_db_t*           log_db;         /* Reference to the database object */
    /* su_gate_t*          log_gate; */      /* Logical log gate (used in exclusive mode) */
#endif /* DBE_LOGORDERING_FIX */
};

#define CHK_LOG(l) ss_dassert(SS_CHKPTR(l) && (l)->log_chk == DBE_CHK_LOG)

static dbe_ret_t dbe_log_put_durable_markif(
        dbe_log_t* log,
        rs_sysi_t* cd);

static dbe_ret_t log_put_durable_nomutex(
        dbe_log_t* log,
        rs_sysi_t* cd,
        hsb_role_t role,
        dbe_catchup_logpos_t local_durable_logpos);

ss_debug(extern bool dbe_db_hsbg2enabled;)

#ifdef SS_HSBG2

/*##**********************************************************************\
 *
 *              dbe_log_transform_init
 *
 * Creates a logical log object
 *
 * Parameters :
 *
 *      cfg - in, use
 *              pointer to dbe configuration object
 *
 *      counter - in out, hold
 *              pointer to counter object
 *
 *  newdb - in
 *      TRUE when database has just been created then there MUST NOT
 *           be a log file before
 *      FALSE when the log is opened to an already existing
 *            database
 *
 *  dbcreatime - in
 *          database creation time
 *
 * Return value - give :
 *      pointer to created log object
 *      or NULL if logging is not enabled in configuration
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_log_t * dbe_log_transform_init(
        dbe_db_t* db,
        dbe_cfg_t* cfg,
        dbe_counter_t* counter,
        bool newdb,
        ulong dbcreatime,
        dbe_hsbg2_t* hsbsvc,
        void* instance_ctx,
        dbe_log_instancetype_t instancetype)
{
        dbe_log_t *log;
        dbe_logfile_t *logfile;

        log = (dbe_log_t *) SsMemAlloc(sizeof(dbe_log_t));
        ss_debug(log->log_chk = DBE_CHK_LOG;)

        log->log_hsbsvc = hsbsvc;
        log->log_transform = TRUE;
        log->log_instancetype = instancetype;
        log->log_final_checkpoint = 0;
        log->log_instance_ctx = instance_ctx;

        logfile = dbe_logfile_transform_init(
                        log->log_hsbsvc,
                        log->log_instancetype,
                        log->log_instance_ctx,
                        cfg,
                        counter,
                        newdb,
                        dbcreatime);

        log->log_logfile = logfile; /* may return NULL if diskless */
        log->log_db = db;
#ifdef DBE_LOGORDERING_FIX
        /* log->log_gate = su_gate_init_recursive(SS_SEMNUM_DBE_LOG, FALSE); */
#endif
        CHK_LOG(log);

        ss_dprintf_1(("dbe_log_transform_init:db %x, log %x\n", db, log));

        return log;
}
#endif /* SS_HSBG2 */

/*##**********************************************************************\
 *
 *              dbe_log_init
 *
 * Creates a logical log object
 *
 * Parameters :
 *
 *      cfg - in, use
 *              pointer to dbe configuration object
 *
 *      counter - in out, hold
 *              pointer to counter object
 *
 *  newdb - in
 *      TRUE when database has just been created then there MUST NOT
 *           be a log file before
 *      FALSE when the log is opened to an already existing
 *            database
 *
 *  dbcreatime - in
 *          database creation time
 *
 * Return value - give :
 *      pointer to created log object
 *      or NULL if logging is not enabled in configuration
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_log_t * dbe_log_init(
#ifdef DBE_LOGORDERING_FIX
        dbe_db_t* db,
#endif /* DBE_LOGORDERING_FIX */
        dbe_cfg_t* cfg,
        dbe_counter_t* counter,
        bool newdb,
        ulong dbcreatime,
        dbe_log_instancetype_t instancetype
#ifdef SS_HSBG2
        ,dbe_hsbg2_t* hsbsvc
#endif /* SS_HSBG2 */
        )
{
        dbe_log_t *log;
        dbe_logfile_t *logfile;

        ss_dassert(instancetype == DBE_LOG_INSTANCE_LOGGING_STANDALONE ||
                   instancetype == DBE_LOG_INSTANCE_LOGGING_HSB);
        ss_dassert(instancetype == DBE_LOG_INSTANCE_LOGGING_HSB || hsbsvc == NULL || ss_migratehsbg2);

        log = (dbe_log_t *) SsMemAlloc(sizeof(dbe_log_t));
        ss_debug(log->log_chk = DBE_CHK_LOG;)

#ifdef SS_HSBG2
        log->log_instance_ctx = NULL;
#if 0
        ss_dassert(hsbsvc != NULL);
#endif
        log->log_hsbsvc = hsbsvc;
        log->log_instancetype = instancetype;
#endif /* SS_HSBG2 */
        log->log_transform = FALSE;
        log->log_final_checkpoint = 0;

        logfile = dbe_logfile_init(
#ifdef SS_HSBG2
                        log->log_hsbsvc,
                        log->log_instancetype,
#endif /* SS_HSBG2 */
                        cfg,
                        dbe_db_getsyscd(db),
                        counter,
                        newdb,
                        dbcreatime,
                        dbe_file_getcipher(dbe_db_getdbfile(db)));

        log->log_logfile = logfile; /* may return NULL if diskless */
#ifdef DBE_LOGORDERING_FIX
        log->log_db = db;
#endif /* DBE_LOGORDERING_FIX */

        CHK_LOG(log);

        ss_dprintf_1(("dbe_log_init:db %x, log %x\n", db, log));

        return log;
}

/*##**********************************************************************\
 *
 *      dbe_log_done
 *
 * Deletes a logical log object
 *
 * Parameters :
 *
 *  log - in, take
 *      pointer to logical log object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_log_done(dbe_log_t* log)
{
#ifdef SS_HSBG2
        dbe_hsbg2_t *hsbsvc;
#endif /* SS_HSBG2 */

        CHK_LOG(log);

#ifdef SS_HSBG2
        hsbsvc = NULL;
#endif /* SS_HSBG2 */

        if(log->log_logfile != NULL) {
                dbe_logfile_done(log->log_logfile);
        }

#ifdef SS_HSBG2
        if (hsbsvc != NULL) {
            dbe_hsbg2_done(hsbsvc);
        }
#endif /* SS_HSBG2 */

        SsMemFree(log);
}


static dbe_ret_t dbe_log_putdata(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen_or_relid)
{
        dbe_ret_t rc;
        bool log_put_durablemark = FALSE;

        /* DBE_LOGREC_HSBG2_SAVELOGPOS is not written to log so it is allowed
         * even in shutdown checkpoint
         */
        ss_dassert(log->log_final_checkpoint != 2 || logrectype == DBE_LOGREC_HSBG2_SAVELOGPOS);
#ifdef DBE_LOG_INSIDEACTIONGATE
        ss_rc_dassert(logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK ||
                      logrectype == DBE_LOGREC_HSBG2_DURABLE ||
                      logrectype == DBE_LOGREC_HSBG2_NEWSTATE ||
                      logrectype == DBE_LOGREC_CLEANUPMAPPING ||
                      logrectype == DBE_LOGREC_SWITCHTOSECONDARY_NORESET || /* Old HSB. Migrating to new? */
                      log->log_transform || ss_migratehsbg2 ||
                      SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE), logrectype);
#endif /* DBE_LOG_INSIDEACTIONGATE */

#ifdef SS_HSBG2
        if (log->log_hsbsvc != NULL
        &&  !log->log_transform
        &&  logrectype != DBE_LOGREC_ABORTTRX_INFO
        &&  logrectype != DBE_LOGREC_HSBG2_SAVELOGPOS)
        {
            rc = dbe_hsbg2_accept_logdata(log->log_hsbsvc);
            if (rc != DBE_RC_SUCC) {
                return(rc);
            }

            FAKE_CODE_RESET(FAKE_HSBG2_LOGGING_SRV_ERR_HSBCONNBROKEN,
            {
                ss_dprintf_1(("FAKE_HSBG2_LOGGING_SRV_ERR_HSBCONNBROKEN\n"));
                return(SRV_ERR_HSBCONNBROKEN);
            });
        }

        if (!log->log_transform && log->log_final_checkpoint == 0) {
            if (log->log_hsbsvc != NULL
            &&  dbe_db_gethsbg2mode(log->log_db) == DBE_HSB_PRIMARY
            &&  logrectype != DBE_LOGREC_HSBG2_NEWSTATE)
            /* &&  logrectype != DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK) */
            {
                /* Note! DBE_LOGREC_HSBG2_NEWSTATE is generatedduring state change in
                    * dbe0hsbstate.c so we do not generate durablemarks for them. It can
                    * cause debug asserts in hsb0svc.c because ofinconsistent roles.
                    * DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK is difficult because we first check
                    * if durable mark is needed but actual write  happens only after log flush
                    * roles can be changed already. So we just ignore check for now.
                    */
                size_t logdatalen;
                dbe_logfile_getdatalenandlen(
                    logrectype,
                    logdata,
                    logdatalen_or_relid,
                    &logdatalen);
                log_put_durablemark = dbe_hsbg2_need_durable_logrec(log->log_hsbsvc, logdatalen + 5);
            }

            if (log_put_durablemark) {
                if (dbe_db_gethsbg2mode(log->log_db) == DBE_HSB_PRIMARY) {
                    dbe_logfile_set_groupcommit_queue_flush(log->log_logfile);
                } else {
                    log_put_durablemark = FALSE;
                }
            }
            if (!log_put_durablemark) {
                log_put_durablemark = (logrectype == DBE_LOGREC_COMMENT);
            }

        }
#endif /* SS_HSBG2 */

        ss_dprintf_3(("dbe_log_putdata:logrectype=%d, trxid=%ld, logdatalen_or_relid=%ld\n",
            logrectype, DBE_TRXID_GETLONG(trxid), logdatalen_or_relid));

        rc = dbe_logfile_putdata(
                log->log_logfile,
                cd,
                logrectype,
                trxid,
                logdata,
                logdatalen_or_relid,
                NULL);

#ifdef SS_HSBG2
        if (log->log_hsbsvc != NULL) {
            if (log_put_durablemark && !log->log_transform) {
                rc = dbe_log_put_durable_markif(log, cd);
            }
        } else {
            ss_pprintf_1(("dbe_log_putdata:log->log_hsbsvc == NULL\n"));
        }

#endif /* SS_HSBG2 */

        return(rc);
}

#ifdef DBE_LOGORDERING_FIX

/*##**********************************************************************\
 *
 *      dbe_log_lock
 *
 * Lock the log. Calls to this function and corresponding unlocking function
 * may be nested.
 *
 * Parameters :
 *
 *  logfile - in out, use
 *      pointer to logfile object
 *
 * Return value :
 *
 * Comments :
 *      Call to this function is typically embedded inside the logical log
 *      object. Use of this method should be strictly restricted to solving
 *      mutex ordering problems and deadlocks.
 *
 * Globals used :
 *
 * See also :
 */
void dbe_log_lock(
        dbe_log_t* log __attribute__ ((unused)))
{
    CHK_LOG(log);
}

/*##**********************************************************************\
 *
 *      dbe_log_unlock
 *
 * Unlock the log. Calls to this function and corresponding locking function
 * may be nested.
 *
 * Parameters :
 *
 *  logfile - in out, use
 *      pointer to logfile object
 *
 * Return value :
 *
 * Comments :
 *      Call to this function is typically embedded inside the logical log
 *      object. Use of this method should be strictly restricted to solving
 *      mutex ordering problems and deadlocks.
 *
 * Globals used :
 *
 * See also :
 */
void dbe_log_unlock(
        dbe_log_t* log __attribute__ ((unused)))
{
    CHK_LOG(log);
}

#endif /* DBE_LOGORDERING_FIX */

dbe_ret_t dbe_log_puthsbcommitmark(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logi_commitinfo_t info,
        dbe_trxid_t trxid
#ifdef DBE_LOGORDERING_FIX
        , rep_params_t *rp,
        bool *p_replicated)
#else  /* DBE_LOGORDERING_FIX */
        )
#endif /* DBE_LOGORDERING_FIX */
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);

#ifdef DBE_LOGORDERING_FIX
        *p_replicated = FALSE;

        FAKE_CODE(if(rp != (void*)-1))
        dbe_log_lock(log);

        FAKE_CODE(if(rp != (void*)-1))
        if(rp != NULL) {
            ss_dassert(rp->rp_type == REP_COMMIT ||
                       rp->rp_type == REP_COMMIT_NOFLUSH);

            rc = dbe_db_replicate(log->log_db, rp->rp_type, rp);
            switch(rc) {
                    case DBE_RC_SUCC:
                        *p_replicated = TRUE;
                        break;
                    case DBE_RC_CONT:
                        /* tuple insert/delete may not flush */
                        ss_dassert(0);
                        break;
                    case DB_RC_NOHSB:
                        rc = DBE_RC_SUCC;
                        break;
                    default:
                        dbe_log_unlock(log);
                        return(rc);
                        break;
            }
        }
#endif /* DBE_LOGORDERING_FIX */

        if(log->log_logfile != NULL) {
            rc = dbe_log_puttrxmark(
                    log,
                    cd,
                    DBE_LOGREC_COMMITTRX_INFO,
                    info,
                    trxid,
                    DBE_HSB_NOT_KNOWN);
        }

#ifdef DBE_LOGORDERING_FIX
        FAKE_CODE(if(rp != (void*)-1))
        dbe_log_unlock(log);
#endif /* DBE_LOGORDERING_FIX */

        return rc;
}

#ifdef SS_HSBG2

/*##**********************************************************************\
 *
 *              dbe_log_put_hsb_new_primary
 *
 * Writes logmark to record that new primary has been chosen
 *
 * Parameters :
 *
 *      log -
 *
 *
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
dbe_ret_t dbe_log_put_hsb_new_primary(
        dbe_log_t* log,
        long originator_nodeid,
        long primary_nodeid)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        char buf[2 * sizeof(ss_uint4_t)];
        void* p;

        CHK_LOG(log);

        p = buf;

        SS_UINT4_STORETODISK(p, originator_nodeid);
        p = buf + sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(p, primary_nodeid);

        dbe_log_lock(log);
        if(log->log_logfile != NULL) {
            rc = dbe_log_putdata(
                    log,
                    NULL,
                    DBE_LOGREC_HSBG2_NEW_PRIMARY,
                    DBE_TRXID_NULL,
                    buf,
                    sizeof(buf));
        }
        dbe_log_unlock(log);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_set_final_checkpoint
 *
 * Sets log to pending checkpoint state.
 *
 * Parameters :
 *
 *      log -
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
void dbe_log_set_final_checkpoint(
        dbe_log_t* log,
        int final_checkpoint __attribute__ ((unused)))
{
        ss_pprintf_1(("dbe_log_set_final_checkpoint\n"));
        CHK_LOG(log);
        ss_dassert(log->log_final_checkpoint == 0);
        ss_dassert(final_checkpoint == 1);

        log->log_final_checkpoint = 1;
}

#endif /* SS_HSBG2 */


dbe_ret_t dbe_log_put_comment(
        dbe_log_t* log,
        char* data __attribute__ ((unused)),
        size_t datafize __attribute__ ((unused)))
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);

        dbe_log_lock(log);
        if(log->log_logfile != NULL) {
            rc = dbe_log_putdata(
                    log,
                    NULL,
                    DBE_LOGREC_COMMENT,
                    DBE_TRXID_NULL,
                    NULL,
                    0);
        }
        dbe_log_unlock(log);

        return(rc);
}


#ifdef SS_HSBG2

/*##**********************************************************************\
 *
 *              dbe_log_put_remote_durable
 *
 * Writes logmark to record that remote node (secondary) is
 * durable up to this.
 *
 * Parameters :
 *
 *      log -
 *
 *
 *      cd -
 *
 *
 *      trxid -
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
dbe_ret_t dbe_log_put_remote_durable(
        dbe_log_t* log,
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);
        ss_dassert(!DBE_CATCHUP_LOGPOS_ISNULL(remote_durable_logpos));

        if (log->log_hsbsvc != NULL) {
            char buf[2 * DBE_LOGPOS_BINSIZE];
            char* p;

            /* write local durable logpos to buf */
            p = buf;
#ifdef HSB_LPID
            LOGPOS_CHECKLOGROLE(local_durable_logpos.lp_role);
            LPID_STORETODISK(p, local_durable_logpos.lp_id);
            p += sizeof(dbe_hsb_lpid_t);
            *p = local_durable_logpos.lp_role;
            p += 1;
#endif
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_logfnum);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_daddr);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_bufpos);
            p += sizeof(ss_uint4_t);

            /* remote durable logpos to buf */

#ifdef HSB_LPID
            LOGPOS_CHECKLOGROLE(remote_durable_logpos.lp_role);
            LPID_STORETODISK(p, remote_durable_logpos.lp_id);
            p += sizeof(dbe_hsb_lpid_t);
            *p = remote_durable_logpos.lp_role;
            p += 1;
#endif
            SS_UINT4_STORETODISK(p, remote_durable_logpos.lp_logfnum);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, remote_durable_logpos.lp_daddr);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, remote_durable_logpos.lp_bufpos);

            ss_dprintf_1(("dbe_log_putdata:dbe_log_put_remote_durable:(%d,%s,%d,%d,%d)\n",
                            LOGPOS_DSDDD(remote_durable_logpos)));

            /* write REMOTE_DURABLE log record and buf to log */

            dbe_log_lock(log);
            if(log->log_logfile != NULL) {
                rc = dbe_log_putdata(
                        log,
                        NULL,
                        DBE_LOGREC_HSBG2_REMOTE_DURABLE,
                        DBE_TRXID_NULL,
                        buf,
                        sizeof(buf));
            }
            dbe_log_unlock(log);
        }

        return(rc);
}

dbe_ret_t dbe_log_put_remote_durable_ack(
        dbe_log_t* log,
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);
        /* ss_dassert(!DBE_CATCHUP_LOGPOS_ISNULL(local_durable_logpos)); */
        ss_dassert(!DBE_CATCHUP_LOGPOS_ISNULL(remote_durable_logpos));

        if (log->log_hsbsvc != NULL && log->log_final_checkpoint != 2) {
            char buf[2 * DBE_LOGPOS_BINSIZE];
            char* p;

            /* write local durable logpos to buf */

            p = buf;
#ifdef HSB_LPID
            LOGPOS_CHECKLOGROLE(local_durable_logpos.lp_role);
            LPID_STORETODISK(p, local_durable_logpos.lp_id);
            p += sizeof(dbe_hsb_lpid_t);
            *p = local_durable_logpos.lp_role;
            p += 1;
#endif
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_logfnum);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_daddr);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_bufpos);
            p += sizeof(ss_uint4_t);

            /* remote durable logpos to buf */

#ifdef HSB_LPID
            LOGPOS_CHECKLOGROLE(remote_durable_logpos.lp_role);
            LPID_STORETODISK(p, remote_durable_logpos.lp_id);
            p += sizeof(dbe_hsb_lpid_t);
            *p = remote_durable_logpos.lp_role;
            p += 1;
#endif
            SS_UINT4_STORETODISK(p, remote_durable_logpos.lp_logfnum);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, remote_durable_logpos.lp_daddr);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, remote_durable_logpos.lp_bufpos);

            ss_dprintf_1(("dbe_log_putdata:dbe_log_put_remote_durable_ack:(%d,%s,%d,%d,%d)\n",
                            LOGPOS_DSDDD(remote_durable_logpos)));

            /* write REMOTE_DURABLE log record and buf to log */

            dbe_log_lock(log);
            if(log->log_logfile != NULL) {
                rc = dbe_log_putdata(
                        log,
                        NULL,
                        DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK,
                        DBE_TRXID_NULL,
                        buf,
                        sizeof(buf));
            }
            dbe_log_unlock(log);
        }

        return(rc);
}


/*##**********************************************************************\
 *
 *              log_put_durable_nomutex
 *
 * Writes local durable record into log file.
 *
 * Parameters :
 *
 *      log -
 *
 *
 *      cd -
 *
 *
 *      trxid -
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
static dbe_ret_t log_put_durable_nomutex(
        dbe_log_t* log,
        rs_sysi_t* cd,
        hsb_role_t role,
        dbe_catchup_logpos_t local_durable_logpos)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);
        ss_dassert(log->log_final_checkpoint != 2);

        if (log->log_hsbsvc != NULL && log->log_logfile != NULL) {
            rc = dbe_logfile_put_durable(log->log_logfile, cd, role, local_durable_logpos);
        }

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_put_durable
 *
 * Writes local durable record into log file.
 *
 * Parameters :
 *
 *      log -
 *
 *
 *      cd -
 *
 *
 *      trxid -
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
dbe_ret_t dbe_log_put_durable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_catchup_logpos_t local_durable_logpos)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);

        if (log->log_hsbsvc != NULL) {

            dbe_log_lock(log);
            log_put_durable_nomutex(log, cd, HSB_ROLE_PRIMARY, local_durable_logpos);
            dbe_log_unlock(log);
        }

        return(rc);
}

dbe_ret_t dbe_log_put_durable_standalone(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_catchup_logpos_t local_durable_logpos)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);

        if (log->log_hsbsvc != NULL) {

            dbe_log_lock(log);
            log_put_durable_nomutex(log, cd, HSB_ROLE_STANDALONE, local_durable_logpos);
            dbe_log_unlock(log);
        }

        return(rc);
}

dbe_ret_t dbe_log_putabortall(
        dbe_log_t* log)
{
        dbe_ret_t rc;

        CHK_LOG(log);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        rc = dbe_log_putdata(
                log,
                NULL,
                DBE_LOGREC_HSBG2_ABORTALL,
                DBE_TRXID_NULL,
                NULL,
                0);
        return (rc);
}

dbe_ret_t dbe_log_puthsbnewstate(
        dbe_log_t* log,
        dbe_hsbstatelabel_t state)
{
        dbe_ret_t rc;
        char buf[1];

        CHK_LOG(log);

        if(log->log_logfile == NULL || log->log_final_checkpoint == 2) {
            return (DBE_RC_SUCC);
        }
        ss_dprintf_1(("dbe_log_puthsbnewstate:state = %s (%d)\n", dbe_hsbstate_getstatestring(state), state));
        buf[0] = (char)state;
        rc = dbe_log_putdata(
                log,
                NULL,
                DBE_LOGREC_HSBG2_NEWSTATE,
                DBE_TRXID_NULL,
                buf,
                sizeof(buf));
        return (rc);
}

dbe_ret_t dbe_log_putsavelogpos(
        dbe_log_t* log,
        dbe_catchup_savedlogpos_t* savedpos)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_log_putsavelogpos\n"));
        CHK_LOG(log);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        rc = dbe_log_putdata(
                log,
                NULL,
                DBE_LOGREC_HSBG2_SAVELOGPOS,
                DBE_TRXID_NULL,
                savedpos,
                0);
        return (rc);
}

#endif /* SS_HSBG2 */

#ifdef SS_HSBG2

static dbe_ret_t dbe_log_put_durable_markif(
        dbe_log_t* log,
        rs_sysi_t* cd)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);

        if (log->log_hsbsvc != NULL) {

            if (!log->log_transform) {
                dbe_catchup_logpos_t local_durable_logpos;

                DBE_CATCHUP_LOGPOS_SET_NULL(local_durable_logpos);
                rc = log_put_durable_nomutex(log, cd, HSB_ROLE_PRIMARY, local_durable_logpos);
            }
        } else {
            ss_pprintf_1(("dbe_log_putdata:log->log_hsbsvc == NULL\n"));
        }
        return(rc);
}

#endif /* SS_HSBG2 */

/*##**********************************************************************\
 *
 *              dbe_log_puttrxmark
 *
 * Puts transaction commit or abort mark to logical log.
 *
 * Parameters :
 *
 *      log - in out, use
 *              pointer to logical log object
 *
 *      info - in
 *              info bits
 *
 *      logrectype - in
 *              log rec type
 *
 *      trxid - in
 *              transaction id
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_log_puttrxmark(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_logi_commitinfo_t info,
        dbe_trxid_t trxid,
        dbe_hsbmode_t trxhsbmode __attribute__ ((unused)))
{
        dbe_ret_t rc;
        ss_byte_t infobuf[1];

        CHK_LOG(log);
        ss_dassert(DBE_LOGI_ISVALIDCOMMITINFO(info));
        ss_dassert(logrectype == DBE_LOGREC_COMMITTRX_INFO || logrectype == DBE_LOGREC_ABORTTRX_INFO);
        ss_dprintf_1(("dbe_log_puttrxmark:logrectype=%d, info=%d, trxid=%ld\n",
            logrectype, info, DBE_TRXID_GETLONG(trxid)));
        ss_dassert(ss_convertdb
                || logrectype == DBE_LOGREC_ABORTTRX_INFO
                || !dbe_db_hsbg2enabled
                || trxhsbmode != DBE_HSB_PRIMARY    /* is UNCERTAIN possible? */
                || (info & (DBE_LOGI_COMMIT_HSBPRIPHASE1|DBE_LOGI_COMMIT_HSBPRIPHASE2)));

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        infobuf[0] = (ss_byte_t)info;

        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                infobuf,
                1);

        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putcpmark
 *
 * Puts a mark to logical log. (eg. a create checkpoint/snapshot)
 *
 * Parameters :
 *
 *      log - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *              record type
 *
 *      cpnum - in
 *          checkpoint/snapshot # to create/delete
 *
 *              p_splitlog - in out, use
 *          on input TRUE forces the log file to split
 *          on output TRUE means log file was split (either
 *          explicitly or implicitly) and FALSE when
 *          log file was split (input was FALSE also)
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_log_putcpmark(
        dbe_log_t* log,
        dbe_logrectype_t logrectype,
        dbe_cpnum_t cpnum,
        SsTimeT ts,
        bool final_checkpoint,
        bool* p_splitlog)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        size_t byteswritten;
        char cpnum_buf[2 * sizeof(ss_uint4_t)];
        void* p;

        CHK_LOG(log);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        if (rc == DBE_RC_SUCC) {
            SS_UINT4_STORETODISK(cpnum_buf, cpnum);
            p = cpnum_buf + sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, ts);
            rc = dbe_logfile_putdata_splitif(
                    log->log_logfile,
                    NULL,
                    logrectype,
                    DBE_TRXID_NULL,
                    cpnum_buf,
                    sizeof(cpnum_buf),
                    &byteswritten,
                    p_splitlog);
        }
        if (final_checkpoint) {
            log->log_final_checkpoint = 2;
        }

        return (rc);
}


/*##**********************************************************************\
 *
 *              dbe_log_puttuple
 *
 * Puts a tuple to logical log. (either an insert or delete record)
 *
 * Parameters :
 *
 *      log  - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *              record type
 *
 *      trxid - in
 *              transaction id
 *
 *      logdata - in, use
 *              pointer to vtuple type object
 *
 *      rp - in, use
 *              replication parameters or NULL if no replication
 *
 *      replicated - out, use
 *              TRUE if replication was successful, FALSE otherwise
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_log_puttuple(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        dynvtpl_t logdata,
        ulong relid
#ifdef DBE_LOGORDERING_FIX
        , rep_params_t *rp,
        bool *p_replicated)
#else /* DBE_LOGORDERING_FIX */
        )
#endif /* DBE_LOGORDERING_FIX */
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);
        ss_dassert(cd != NULL);
        ss_dassert(logrectype == DBE_LOGREC_INSTUPLEWITHBLOBS
                || logrectype == DBE_LOGREC_INSTUPLENOBLOBS
                || logrectype == DBE_LOGREC_DELTUPLE);


#ifdef DBE_LOGORDERING_FIX
        *p_replicated = FALSE;

        dbe_log_lock(log);

        if(rp != NULL) {
            ss_dassert(rp->rp_type == REP_INSERT || rp->rp_type == REP_DELETE);

            rc = dbe_db_replicate(log->log_db, rp->rp_type, rp);
            switch(rc) {
                    case DBE_RC_SUCC:
                        *p_replicated = TRUE;
                        break;

                    case DBE_RC_CONT:
            /*
                         * Replication must not return to tasking system
             * because we need to hold the mutex until
             * we are done, otherwise the ordering
             * between trx and hsb logs is not guaranteed.
             * dbe_db_replicate() should take care of this.
             *
             */
                        ss_dassert(0);
                        break;

                    case DB_RC_NOHSB:
                        rc = DBE_RC_SUCC;
                        break;

                    default:
                        dbe_log_unlock(log);
                        return(rc);
                        break;
            }
        }
#endif /* DBE_LOGORDERING_FIX */

        if(log->log_logfile != NULL) {
            rc = dbe_log_putdata(
                    log,
                    cd,
                    logrectype,
                    trxid,
                    logdata,
                    relid);
        }

#ifdef DBE_LOGORDERING_FIX
        dbe_log_unlock(log);
#endif /* DBE_LOGORDERING_FIX */

        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putmmetuple
 *
 * Puts a MME tuple to logical log. (either an insert or delete record)
 *
 * Parameters :
 *
 *      log  - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *              record type
 *
 *      trxid - in
 *              transaction id
 *
 *      logdata - in, use
 *              pointer to vtuple type object
 *
 *      rp - in, use
 *              replication parameters or NULL if no replication
 *
 *      replicated - out, use
 *              TRUE if replication was successful, FALSE otherwise
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
#ifdef SS_MME
dbe_ret_t dbe_log_putmmetuple(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        mme_rval_t* logdata,
        ulong relid,
        rep_params_t *rp __attribute__ ((unused)),
        bool *p_replicated)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_LOG(log);
        ss_dassert(cd != NULL);
        ss_dassert(logrectype == DBE_LOGREC_MME_INSTUPLEWITHBLOBS
                || logrectype == DBE_LOGREC_MME_INSTUPLENOBLOBS
                || logrectype == DBE_LOGREC_MME_DELTUPLE);

        *p_replicated = FALSE;

    dbe_log_lock(log);
#if 0
        if(rp != NULL) {
            ss_error; /* HSB not supported yet */
        }
#endif

        if(log->log_logfile != NULL) {
            rc = dbe_log_putdata(
                    log,
                    cd,
                    logrectype,
                    trxid,
                    logdata,
                    relid);
        }
        dbe_log_unlock(log);

        return (rc);
}
#endif /* SS_MME */

/*##**********************************************************************\
 *
 *              dbe_log_putcreatetable
 *
 * Puts a create table log record to log file
 *
 * Parameters :
 *
 *      log - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *              log record type - DBE_LOGREC_CREATE(TABLE|VIEW)
 *
 *      trxid - in
 *              Transaction ID
 *
 *      relid - in
 *              Relation (table) ID
 *
 *      schema - in, use
 *              relation (table) schema
 *
 *      relname - in, use
 *              relation (table) name
 *
 *      nkeys - in
 *          number of indexes created including clustering key
 *
 *      nattrs - in
 *          number of attributes (including hidden attributes)
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putcreatetable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        rs_entname_t* name,
        rs_ano_t nkeys,
        rs_ano_t nattrs)
{
        char* p;
        size_t relnamelen_plus_1;
        size_t schemanamelen_plus_1;
        size_t recsize;
        dbe_ret_t rc;
        ss_uint2_t i2;
        char* schema = rs_entname_getschema(name);
        char* relname = rs_entname_getname(name);
        char* catalog = rs_entname_getcatalog(name);
        size_t catalognamelen_plus_1 = ((catalog == NULL)? 1 : (strlen(catalog) + 1));

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        ss_dprintf_1(("dbe_log_putcreatetable:relid %ld, relname %s\n", relid, relname));

        relnamelen_plus_1 = strlen(relname) + 1;
        if (schema != NULL) {
            schemanamelen_plus_1 = strlen(schema) + 1;
        } else {
            schemanamelen_plus_1 = 1;
        }
        recsize =
            sizeof(ss_uint4_t) +
            sizeof(ss_uint2_t) * 2 +
            relnamelen_plus_1 +
            schemanamelen_plus_1
            + catalognamelen_plus_1
            ;
        p = SsMemAlloc(recsize);
        SS_UINT4_STORETODISK(p, relid);
        i2 = (ss_uint2_t)nkeys;
        SS_UINT2_STORETODISK(p + sizeof(ss_uint4_t), i2);
        i2 = (ss_uint2_t)nattrs;
        SS_UINT2_STORETODISK(p + sizeof(ss_uint4_t) + sizeof(i2), i2);
        memcpy(
            p + sizeof(ss_uint4_t) + sizeof(i2) * 2,
            relname,
            relnamelen_plus_1);
        if (schema != NULL) {
            memcpy(
                p + sizeof(ss_uint4_t) + sizeof(i2) * 2 + relnamelen_plus_1,
                schema,
                schemanamelen_plus_1);
        } else {
            p[sizeof(ss_uint4_t) + sizeof(i2) * 2 + relnamelen_plus_1] = '\0';
        }
        if (catalog != NULL) {
            memcpy(
                p +
                sizeof(ss_uint4_t) +
                sizeof(i2) * 2
                + relnamelen_plus_1 +
                + schemanamelen_plus_1,
                catalog,
                catalognamelen_plus_1);
        } else {
            p[sizeof(ss_uint4_t) +
              sizeof(i2) * 2 +
              relnamelen_plus_1 +
              schemanamelen_plus_1] = '\0';
        }
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                p,
                recsize);
        SsMemFree(p);
        return (rc);

}

/*##**********************************************************************\
 *
 *              dbe_log_putrenametable
 *
 *
 *
 * Parameters :
 *
 *      log  -
 *
 *
 *      trxid -
 *
 *
 *      relid -
 *
 *
 *      schema -
 *
 *
 *      relname -
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
dbe_ret_t dbe_log_putrenametable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        ulong relid,
        rs_entname_t* name)
{
        char* p;
        size_t relnamelen_plus_1;
        size_t schemanamelen_plus_1;
        size_t recsize;
        dbe_ret_t rc;
        char* schema = rs_entname_getschema(name);
        char* relname = rs_entname_getname(name);
        char* catalog = rs_entname_getcatalog(name);
        size_t catalognamelen_plus_1 =
            ((catalog == NULL) ? 1 : (strlen(catalog) + 1));

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        ss_dassert(schema != NULL);
        ss_dassert(relname != NULL);

        ss_dprintf_1(("dbe_log_putrenametable:relid %ld, relname %s\n", relid, relname));

        relnamelen_plus_1 = strlen(relname) + 1;
        schemanamelen_plus_1 = strlen(schema) + 1;

        recsize =
            sizeof(ss_uint4_t) +
            relnamelen_plus_1 +
            schemanamelen_plus_1
            + catalognamelen_plus_1
            ;
        p = SsMemAlloc(recsize);
        SS_UINT4_STORETODISK(p, relid);
        memcpy(
            p + sizeof(ss_uint4_t),
            relname,
            relnamelen_plus_1);
        memcpy(
            p + sizeof(ss_uint4_t) + relnamelen_plus_1,
            schema,
            schemanamelen_plus_1);
        if (catalog != NULL) {
            memcpy(p + sizeof(ss_uint4_t) +
                   relnamelen_plus_1 +
                   schemanamelen_plus_1,
                   catalog,
                   catalognamelen_plus_1);
        } else {
            p[sizeof(ss_uint4_t) +
              relnamelen_plus_1 +
              schemanamelen_plus_1] = '\0';
        }

        rc = dbe_log_putdata(
                log,
                cd,
                DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED,
                trxid,
                p,
                recsize);
        SsMemFree(p);
        return (rc);

}

/*##**********************************************************************\
 *
 *              dbe_log_putaltertable
 *
 * Puts an ALTER TABLE record into logical log
 *
 * Parameters :
 *
 *      log - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *          log record type
 *
 *      trxid - in
 *              transaction ID
 *
 *      relid - in
 *              relation ID
 *
 *      relname - in, use
 *              relation name
 *
 *      nnewattrs - in
 *              # of new attributes (0 when DROP COLUMN)
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putaltertable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        char* relname,
        rs_ano_t nnewattrs)
{
        char* p;
        size_t namelen_plus_1;
        size_t recsize;
        dbe_ret_t rc;
        ss_uint2_t i2;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        namelen_plus_1 = strlen(relname) + 1;
        recsize =
            sizeof(ss_uint4_t) +   /* relid */
            sizeof(ss_uint2_t) +    /* nnewattrs */
            namelen_plus_1;         /* relname */
        p = SsMemAlloc(recsize);
        SS_UINT4_STORETODISK(p, relid);
        i2 = (ss_uint2_t)nnewattrs;
        SS_UINT2_STORETODISK(p + sizeof(ss_uint4_t), i2);
        memcpy(
            p + sizeof(ss_uint4_t) + sizeof(i2),
            relname,
            namelen_plus_1);
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                p,
                recsize);
        SsMemFree(p);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putdroptable
 *
 * Puts drop table record to logical log
 *
 * Parameters :
 *
 *      log - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *              log record type - DBE_LOGREC_DROP(TABLE|VIEW)
 *
 *      trxid - in
 *              Transaction ID
 *
 *      relid - in
 *              Relation (table) ID
 *
 *      relname - in, use
 *              relation (table) name
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putdroptable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        char* relname)
{
        char* p;
        size_t namelen_plus_1;
        dbe_ret_t rc;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        ss_dprintf_1(("dbe_log_putdroptable:relid %ld, relname %s\n", relid, relname));

        namelen_plus_1 = strlen(relname) + 1;
        p = SsMemAlloc(sizeof(ss_uint4_t) + namelen_plus_1);
        SS_UINT4_STORETODISK(p, relid);
        memcpy(p + sizeof(ss_uint4_t), relname, namelen_plus_1);
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                p,
                sizeof(ss_uint4_t) + namelen_plus_1);
        SsMemFree(p);
        return (rc);

}

/*##**********************************************************************\
 *
 *              dbe_log_putcreateordropidx
 *
 * Puts create/drop index log record to logical log
 *
 * Parameters :
 *
 *      log - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *              log record type - DBE_LOGREC_(CREATE|DROP)INDEX
 *
 *      trxid - in
 *              transactio ID
 *
 *      relid - in
 *              relation (table) ID
 *
 *      keyid - in
 *              key (index) ID
 *
 *      relname - in, use
 *              relation (table) name
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putcreateordropidx(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        ulong keyid,
        char* relname)
{
        char* p;
        size_t namelen_plus_1;
        dbe_ret_t rc;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        namelen_plus_1 = strlen(relname) + 1;
        p = SsMemAlloc(2 * sizeof(ss_uint4_t) + namelen_plus_1);
        SS_UINT4_STORETODISK(p, relid);
        SS_UINT4_STORETODISK(p + sizeof(ss_uint4_t), keyid);
        memcpy(p + 2 * sizeof(ss_uint4_t), relname, namelen_plus_1);
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                p,
                2 * sizeof(ss_uint4_t) + namelen_plus_1);
        SsMemFree(p);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putcreateuser
 *
 *
 *
 * Parameters :
 *
 *      log - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *              log record type - DBE_LOGREC_CREATEUSER
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putcreateuser(
        dbe_log_t* log,
        dbe_logrectype_t logrectype)
{
        dbe_ret_t rc;

        CHK_LOG(log);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        rc = dbe_log_putdata(
                log,
                NULL,
                logrectype,
                DBE_TRXID_NULL,
                NULL,
                0);
        return (rc);
}


/*##**********************************************************************\
 *
 *              dbe_log_putstmtmark
 *
 * Puts Statement commit record to logical log
 *
 * Parameters :
 *
 *      log - in out, use
 *              pointer to logical log object
 *
 *      logrectype - in
 *              log record type - DBE_LOGREC_(COMMIT|ABORT)STMT
 *
 *      trxid - in
 *              transaction ID
 *
 *      stmttrxid - in
 *              statement transaction ID
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putstmtmark(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        dbe_trxid_t stmttrxid)
{
        dbe_ret_t rc;
        char stmttrxidbuf[sizeof(ss_uint4_t)];

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        ss_dassert(sizeof(stmttrxid) >= 4);

        if (logrectype == DBE_LOGREC_COMMITSTMT) {
            ss_dassert(!DBE_TRXID_EQUAL(trxid, stmttrxid));
            SS_UINT4_STORETODISK(stmttrxidbuf, DBE_TRXID_GETLONG(stmttrxid));
            rc = dbe_log_putdata(
                    log,
                    cd,
                    logrectype,
                    trxid,
                    stmttrxidbuf,
                    sizeof(stmttrxidbuf));
        } else {
            rc = dbe_log_putdata(
                    log,
                    cd,
                    logrectype,
                    stmttrxid,
                    NULL,
                    0);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putincsysctr
 *
 * Puts an increment System counter record into logical log
 *
 * Parameters :
 *
 *      log - use
 *              logical log object
 *
 *      logrectype - in
 *              log record type - DBE_LOGREC_INCSYSCTR
 *
 *      ctrid - in
 *              system counter ID
 *
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putincsysctr(
        dbe_log_t* log,
        dbe_logrectype_t logrectype,
        dbe_sysctrid_t ctrid)
{
        dbe_ret_t rc;
        uchar ctridbuf;

        CHK_LOG(log);
        ss_dprintf_1(("dbe_log_putincsysctr:logrectype=%s, ctrid=%ld\n", dbe_logi_getrectypename(logrectype), (long)ctrid));

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        ctridbuf = (char)ctrid;
        rc = dbe_log_putdata(
                log,
                NULL,
                logrectype,
                DBE_TRXID_NULL,
                &ctridbuf,
                sizeof(ctridbuf));
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_puthsbsysctr
 *
 * Puts an set HSB System counter record into log
 *
 * Parameters :
 *
 *      log - use
 *              logical log object
 *
 *      data - in
 *              system counter values
 *
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_puthsbsysctr(
        dbe_log_t* log,
        char* data)
{
        dbe_ret_t rc;

        CHK_LOG(log);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        rc = dbe_log_putdata(
                log,
                NULL,
                DBE_LOGREC_SETHSBSYSCTR,
                DBE_TRXID_NULL,
                data,
                DBE_HSBSYSCTR_SIZE);
        return (rc);
}

/*#***********************************************************************\
 *
 *              log_putcreateordropctrorseq
 *
 * Puts create/drop counter/sequence record into log
 *
 * Parameters :
 *
 *      log - use
 *              logical log object
 *
 *      logrectype - in
 *              record type, DBE_LOGREC_(DROP|CREATE)(CTR|SEQ)
 *
 *      trxid - in
 *              transaction ID
 *
 *      ctrorseqid - in
 *              counter/sequence ID
 *
 *      ctrorseqname - in
 *              counter/sequence name
 *
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t log_putcreateordropctrorseq(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t ctrorseqid,
        char* ctrorseqname)
{
        char* p;
        size_t namelen_plus_1;
        dbe_ret_t rc;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        namelen_plus_1 = strlen(ctrorseqname) + 1;
        p = SsMemAlloc(sizeof(ss_uint4_t) + namelen_plus_1);
        SS_UINT4_STORETODISK(p, ctrorseqid);
        memcpy(p + sizeof(ss_uint4_t), ctrorseqname, namelen_plus_1);
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                p,
                sizeof(ss_uint4_t) + namelen_plus_1);
        SsMemFree(p);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putcreatectrorseq
 *
 * Puts a create sequence/counter record into log
 *
 * Parameters :
 *
 *      log - use
 *              logical log object
 *
 *      logrectype - in
 *              record type, DBE_LOGREC_CREATE(CTR|SEQ)
 *
 *      trxid - in
 *              transaction ID
 *
 *      ctrorseqid - in
 *              counter/sequence ID
 *
 *      ctrorseqname - in
 *              counter/sequence name
 *
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putcreatectrorseq(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t ctrorseqid,
        char* ctrorseqname)
{
        dbe_ret_t rc;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        ss_dassert(logrectype == DBE_LOGREC_CREATECTR
            ||     logrectype == DBE_LOGREC_CREATESEQ);
        rc = log_putcreateordropctrorseq(
                log,
                cd,
                logrectype,
                trxid,
                ctrorseqid,
                ctrorseqname);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putdropctrorseq
 *
 * Puts a drop sequence/counter record into log
 *
 * Parameters :
 *
 *      log - use
 *              logical log object
 *
 *      logrectype - in
 *              record type, DBE_LOGREC_DROP(CTR|SEQ)
 *
 *      trxid - in
 *              transaction ID
 *
 *      ctrorseqid - in
 *              counter/sequence ID
 *
 *      ctrorseqname - in
 *              counter/sequence name
 *
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putdropctrorseq(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t ctrorseqid,
        char* ctrorseqname)
{
        dbe_ret_t rc;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        ss_dassert(logrectype == DBE_LOGREC_DROPCTR
            ||     logrectype == DBE_LOGREC_DROPSEQ);
        rc = log_putcreateordropctrorseq(
                log,
                cd,
                logrectype,
                trxid,
                ctrorseqid,
                ctrorseqname);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putincctr
 *
 * Puts an increment counter record into log
 *
 * Parameters :
 *
 *      log - use
 *              log object
 *
 *      logrectype - in
 *              record type - DBE_LOGREC_INCCTR
 *
 *      ctrid - in
 *              counter ID
 *
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putincctr(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        ss_uint4_t ctrid)
{
        dbe_ret_t rc;
        ss_uint4_t ctridbuf;

        CHK_LOG(log);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        SS_UINT4_STORETODISK(&ctridbuf, ctrid);
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                DBE_TRXID_NULL,
                &ctridbuf,
                sizeof(ctridbuf));
        return (rc);
}


/*##**********************************************************************\
 *
 *              dbe_log_putsetctr
 *
 * Puts counter value change record to log
 *
 * Parameters :
 *
 *      log - use
 *              logical log object
 *
 *      logrectype - in
 *              record type - DBE_LOGREC_SETCTR
 *
 *      ctrid - in
 *              counter ID
 *
 *      value - in, use
 *              counter value (64-bit binary counter)
 *
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putsetctr(
        dbe_log_t* log,
        dbe_logrectype_t logrectype,
        ss_uint4_t ctrid,
        rs_tuplenum_t* value)
{
        dbe_ret_t rc;
        ss_uint4_t lsl;
        ss_uint4_t msl;
        ss_uint4_t dbuf[3];

        CHK_LOG(log);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        lsl = rs_tuplenum_getlsl(value);
        msl = rs_tuplenum_getmsl(value);
        SS_UINT4_STORETODISK(&dbuf[0], ctrid);
        SS_UINT4_STORETODISK(&dbuf[1], lsl);
        SS_UINT4_STORETODISK(&dbuf[2], msl);
        rc = dbe_log_putdata(
                log,
                NULL,
                logrectype,
                DBE_TRXID_NULL,
                &dbuf,
                sizeof(dbuf));
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putsetseq
 *
 * Puts sequence value change record to log
 *
 * Parameters :
 *
 *      log - use
 *              logical log object
 *
 *      logrectype - in
 *              record type - DBE_LOGREC_SETSEQ
 *
 *      trxid - in
 *              transaction ID
 *
 *      seqid - in
 *              sequence ID
 *
 *      value - in, use
 *              sequence value (internally 64-bit binary ctr)
 *
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putsetseq(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t seqid,
        rs_tuplenum_t* value)
{
        dbe_ret_t rc;
        ss_uint4_t lsl;
        ss_uint4_t msl;
        ss_uint4_t dbuf[3];

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        lsl = rs_tuplenum_getlsl(value);
        msl = rs_tuplenum_getmsl(value);
        SS_UINT4_STORETODISK(&dbuf[0], seqid);
        SS_UINT4_STORETODISK(&dbuf[1], lsl);
        SS_UINT4_STORETODISK(&dbuf[2], msl);
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                &dbuf,
                sizeof(dbuf));
        return (rc);
}

#ifdef DBE_REPLICATION

/*##**********************************************************************\
 *
 *              dbe_log_puthotstandbymark
 *
 * Put hot stanby mark (DBE_LOGREC_SWITCHTO(MASTER|SLAVE)) to log.
 *
 * Parameters :
 *
 *      log - use
 *              logical log object
 *
 *      logrectype - in
 *              DBE_LOGREC_SWITCHTO(MASTER|SLAVE)
 *
 *      trxid - in
 *              transaction id
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_puthotstandbymark(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid)
{
        dbe_ret_t rc;

        CHK_LOG(log);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        ss_dassert(logrectype == DBE_LOGREC_SWITCHTOPRIMARY ||
                   logrectype == DBE_LOGREC_SWITCHTOSECONDARY ||
                   logrectype == DBE_LOGREC_SWITCHTOSECONDARY_NORESET ||
                   logrectype == DBE_LOGREC_CLEANUPMAPPING);
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                NULL,
                0);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putreplicatrxstart
 *
 * Puts a replicate-transaction start record to log
 *
 * Parameters :
 *
 *      log  - use
 *
 *
 *      logrectype - in
 *              DBE_LOGREC_REPLICATRXSTART
 *
 *      localtrxid - in
 *
 *
 *      remotetrxid - in
 *
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putreplicatrxstart(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t localtrxid,
        dbe_trxid_t remotetrxid)
{
        dbe_ret_t rc;
        ss_uint4_t dbuf[1];

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        ss_dassert(logrectype == DBE_LOGREC_REPLICATRXSTART);
        SS_UINT4_STORETODISK(&dbuf[0], DBE_TRXID_GETLONG(remotetrxid));
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                localtrxid,
                dbuf,
                sizeof(dbuf));
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putreplicastmtstart
 *
 * Puts a replicate-transaction start record to log
 *
 * Parameters :
 *
 *      log - use
 *
 *
 *      logrectype - in
 *              DBE_LOGREC_REPLICATRXSTART
 *
 *      localtrxid - in
 *
 *
 *      remotestmtid - in
 *
 *      localstmtid - in
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_putreplicastmtstart(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t localtrxid,
        dbe_trxid_t remotestmtid,
        dbe_trxid_t localstmtid)
{
        dbe_ret_t rc;
        ss_uint4_t dbuf[2];

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        ss_dassert(logrectype == DBE_LOGREC_REPLICASTMTSTART);
        ss_dassert(DBE_TRXID_NOTNULL(localstmtid));
        SS_UINT4_STORETODISK(&dbuf[0], DBE_TRXID_GETLONG(remotestmtid));
        SS_UINT4_STORETODISK(&dbuf[1], DBE_TRXID_GETLONG(localstmtid));
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                localtrxid,
                dbuf,
                sizeof(dbuf));
        return (rc);
}

#endif /* DBE_REPLICATION */

/*##**********************************************************************\
 *
 *              dbe_log_putauditinfo
 *
 * Puts general info to logical log
 *
 * Parameters :
 *
 *      log -
 *
 *
 *      trxid -
 *
 *
 *      info -
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
dbe_ret_t dbe_log_putauditinfo(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        long userid,
        char* info)
{
        char* p;
        size_t infolen_plus_1;
        size_t recsize;
        dbe_ret_t rc;
        ulong userid_ulong;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        ss_dassert(info != NULL);

        userid_ulong = (ulong)userid;
        infolen_plus_1 = strlen(info) + 1;

        recsize =
            sizeof(ss_uint4_t) +
            infolen_plus_1;
        p = SsMemAlloc(recsize);
        SS_UINT4_STORETODISK(p, userid_ulong);
        memcpy(
            p + sizeof(ss_uint4_t),
            info,
            infolen_plus_1);
        rc = dbe_log_putdata(
                log,
                cd,
                DBE_LOGREC_AUDITINFO,
                trxid,
                p,
                recsize);
        SsMemFree(p);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_putblobstart
 *
 * Puts blob start into logical log
 *
 * Parameters :
 *
 *      log -
 *
 *
 *      trxid -
 *
 *
 *      info -
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
dbe_ret_t dbe_log_putblobstart(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        dbe_vablobref_t blobref)
{
        char blobref_buf[DBE_VABLOBREF_SIZE];
        dbe_ret_t rc;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        dbe_bref_storetodiskbuf(&blobref, blobref_buf);

        rc = dbe_log_putdata(
                log,
                cd,
                DBE_LOGREC_BLOBSTART_OLD,
                trxid,
                blobref_buf,
                sizeof(blobref_buf));

        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_log_blobdata
 *
 * Puts blob data into logical log
 *
 * Parameters :
 *
 *      log -
 *
 *
 *      trxid -
 *
 *
 *      info -
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
dbe_ret_t dbe_log_putblobdata(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        char* data,
        size_t datasize)
{
        dbe_ret_t rc;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }
        rc = dbe_log_putdata(
                log,
                cd,
                logrectype,
                trxid,
                data,
                datasize);

        return (rc);
}

dbe_ret_t dbe_log_putblobg2data(
        dbe_log_t* log,
        rs_sysi_t* cd,
        ss_byte_t* data,
        size_t datasize)
{
        dbe_ret_t rc;

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        rc = dbe_log_putdata(log,
                            cd,
                            DBE_LOGREC_BLOBG2DATA,
                            DBE_TRXID_NULL,
                            data,
                            datasize);
        return (rc);
}

dbe_ret_t dbe_log_putblobg2dropmemoryref(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_blobg2id_t blobid)
{
        dbe_ret_t rc;
        char dbuf[8];

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        DBE_BLOBG2ID_PUTTODISK(dbuf, blobid);

        rc = dbe_log_putdata(log,
                            cd,
                            DBE_LOGREC_BLOBG2DROPMEMORYREF,
                            DBE_TRXID_NULL,
                            &dbuf,
                            sizeof(dbuf));
        return (rc);
}

dbe_ret_t dbe_log_putblobg2datacomplete(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_blobg2id_t blobid)
{
        dbe_ret_t rc;
        char dbuf[8];

        CHK_LOG(log);
        ss_dassert(cd != NULL);

        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        DBE_BLOBG2ID_PUTTODISK(dbuf, blobid);

        rc = dbe_log_putdata(log,
                            cd,
                            DBE_LOGREC_BLOBG2DATACOMPLETE,
                            DBE_TRXID_NULL,
                            &dbuf,
                            sizeof(dbuf));
        return (rc);
}

/*##**********************************************************************\
 *
 *      dbe_log_idleflush
 *
 * If lazy flushing is enabled for the physical trx log, it is carried out
 *
 *
 * Parameters :
 *
 *  log - in out, use
 *      pointer to log object
 *
 * Return value :
 *      DBE_RC_SUCC when ok
 *      or error code otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_log_idleflush(
        dbe_log_t* log)
{
        if(log->log_logfile == NULL) {
            return (DBE_RC_SUCC);
        }

        return(dbe_logfile_idleflush(log->log_logfile));
}

dbe_ret_t dbe_log_flushtodisk(
        dbe_log_t* log)
{
        if(log->log_logfile == NULL) {
            /* solid.ini: '[logging] logenabled=no'? */
            ss_derror;
            return (DBE_RC_SUCC);
        }

        return(dbe_logfile_flushtodisk(log->log_logfile));
}

dbe_ret_t dbe_log_waitflushmes(
        dbe_log_t* log,
        rs_sysi_t* cd)
{
        if (log->log_logfile == NULL) {
            rs_sysi_setlogwaitrc(cd, NULL);
            return (DBE_RC_SUCC);
        }

        return(dbe_logfile_waitflushmes(log->log_logfile, cd));
}

void dbe_log_setidlehsbdurable(
        dbe_log_t* log,
        dbe_logfile_idlehsbdurable_t mode)
{
        if (log->log_logfile != NULL) {
            dbe_logfile_setidlehsbdurable(log->log_logfile, mode);
        }
}

/*##**********************************************************************\
 *
 *      dbe_log_getsize
 *
 * Gets size sum (in kbytes) of all (trx and hsb) log files
 *
 * Parameters :
 *
 *  log - in, use
 *      pointer to logfile object
 *
 * Return value :
 *      size sum of log files in kilobytes
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
ulong dbe_log_getsize(dbe_log_t* log, bool lastonly)
{
        if(log == NULL || log->log_logfile == NULL) {
            return (0);
        }
        if(lastonly) {
            return (dbe_logfile_getsize2(log->log_logfile));
        } else {
            return (dbe_logfile_getsize(log->log_logfile));
        }
}

/*##**********************************************************************\
 *
 *      dbe_log_seterrorhandler
 *
 * Sets an error handler function for failed log writes. This enables
 * the database to create checkpoint when e.g. the hard disk containing
 * the log file crashes.
 *
 * Parameters :
 *
 *  logfile - in out, use
 *      pointer to logfile object
 *
 *      errorfunc - in, hold
 *          pointer to error function
 *
 *      errorctx - in, hold
 *          context pointer to be passed as argument of errorfunc
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_log_seterrorhandler(
        dbe_log_t* log,
        void (*errorfunc)(void*),
        void* errorctx)
{
        if(log->log_logfile == NULL) {
            return;
        }
        dbe_logfile_seterrorhandler(log->log_logfile, errorfunc, errorctx);
}

ulong dbe_log_getfilewritecnt(
        dbe_log_t* log)
{
    if(log->log_logfile == NULL) {
        return(0);
        }
        return dbe_logfile_getfilewritecnt(log->log_logfile);
}

#endif /* SS_NOLOGGING */

dbe_catchup_logpos_t dbe_log_getlogpos(dbe_log_t* log)
{
        dbe_catchup_logpos_t logpos;
        CHK_LOG(log);

        logpos = dbe_logfile_getlogpos(log->log_logfile);
        return(logpos);
}

