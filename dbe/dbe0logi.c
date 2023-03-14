/*************************************************************************\
**  source       * dbe0logi.c
**  directory    * dbe
**  description  * Public database log interface
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

This object implements interface to the database log which is
visible outside the DBE subsystem.

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
#include <ssdebug.h>
#include <sssprint.h>

#include "dbe0db.h"
#include "dbe0trx.h"
#include "dbe0ld.h"
#include "dbe0logi.h"
#include "dbe0catchup.h"
#include "dbe0db.h"
#include "dbe6log.h"

ss_beta(long dbe_logrectype_flushcount[DBE_LOGREC_1STUNUSED];)
ss_beta(long dbe_logrectype_waitflushqueuecount[DBE_LOGREC_1STUNUSED];)
ss_beta(long dbe_logrectype_writecount[DBE_LOGREC_1STUNUSED];)

/*##**********************************************************************\
 *
 *              dbe_logi_getrectypename
 *
 * Returns log record type name.
 *
 * Parameters :
 *
 *      rectype -
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
char* dbe_logi_getrectypename(dbe_logrectype_t rectype)
{
        const char* rectypename;

        switch (rectype) {
            case DBE_LOGREC_NOP:
                rectypename = "DBE_LOGREC_NOP";
                break;
            case DBE_LOGREC_HEADER:
                rectypename = "DBE_LOGREC_HEADER";
                break;
            case DBE_LOGREC_INSTUPLE:
                rectypename = "DBE_LOGREC_INSTUPLE";
                break;
            case DBE_LOGREC_DELTUPLE:
                rectypename = "DBE_LOGREC_DELTUPLE";
                break;
            case DBE_LOGREC_BLOBSTART_OLD:
                rectypename = "DBE_LOGREC_BLOBSTART_OLD";
                break;
            case DBE_LOGREC_BLOBALLOCLIST_OLD:
                rectypename = "DBE_LOGREC_BLOBALLOCLIST_OLD";
                break;
            case DBE_LOGREC_BLOBALLOCLIST_CONT_OLD:
                rectypename = "DBE_LOGREC_BLOBALLOCLIST_CONT_OLD";
                break;
            case DBE_LOGREC_BLOBDATA_OLD:
                rectypename = "DBE_LOGREC_BLOBDATA_OLD";
                break;
            case DBE_LOGREC_BLOBDATA_CONT_OLD:
                rectypename = "DBE_LOGREC_BLOBDATA_CONT_OLD";
                break;
            case DBE_LOGREC_BLOBG2DATA:
                rectypename = "DBE_LOGREC_BLOBG2DATA";
                break;
            case DBE_LOGREC_BLOBG2DATACOMPLETE:
                rectypename = "DBE_LOGREC_BLOBG2DATACOMPLETE";
                break;
            case DBE_LOGREC_BLOBG2DROPMEMORYREF:
                rectypename = "DBE_LOGREC_BLOBG2DROPMEMORYREF";
                break;
            case DBE_LOGREC_ABORTTRX_OLD:
                rectypename = "DBE_LOGREC_ABORTTRX_OLD";
                break;
            case DBE_LOGREC_ABORTTRX_INFO:
                rectypename = "DBE_LOGREC_ABORTTRX_INFO";
                break;
            case DBE_LOGREC_COMMITTRX_OLD:
                rectypename = "DBE_LOGREC_COMMITTRX_OLD";
                break;
            case DBE_LOGREC_COMMITTRX_NOFLUSH_OLD:
                rectypename = "DBE_LOGREC_COMMITTRX_NOFLUSH_OLD";
                break;
            case DBE_LOGREC_COMMITTRX_HSB_OLD:
                rectypename = "DBE_OLD_LOGREC_COMMITTRX_HSB_OLD";
                break;
            case DBE_LOGREC_COMMITTRX_INFO:
                rectypename = "DBE_LOGREC_COMMITTRX_INFO";
                break;
            case DBE_LOGREC_HSBCOMMITMARK_OLD:
                rectypename = "DBE_LOGREC_HSBCOMMITMARK_OLD";
                break;
            case DBE_LOGREC_COMMITSTMT:
                rectypename = "DBE_LOGREC_COMMITSTMT";
                break;
            case DBE_LOGREC_PREPARETRX:
                rectypename = "DBE_LOGREC_PREPARETRX";
                break;
            case DBE_LOGREC_CHECKPOINT_OLD:
                rectypename = "DBE_LOGREC_CHECKPOINT_OLD";
                break;
            case DBE_LOGREC_SNAPSHOT_OLD:
                rectypename = "DBE_LOGREC_SNAPSHOT_OLD";
                break;
            case DBE_LOGREC_CHECKPOINT_NEW:
                rectypename = "DBE_LOGREC_CHECKPOINT_NEW";
                break;
            case DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT:
                rectypename = "DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT";
                break;
            case DBE_LOGREC_SNAPSHOT_NEW:
                rectypename = "DBE_LOGREC_SNAPSHOT_NEW";
                break;
            case DBE_LOGREC_DELSNAPSHOT:
                rectypename = "DBE_LOGREC_DELSNAPSHOT";
                break;
            case DBE_LOGREC_CREATETABLE:
                rectypename = "DBE_LOGREC_CREATETABLE";
                break;
            case DBE_LOGREC_CREATETABLE_NEW:
                rectypename = "DBE_LOGREC_CREATETABLE_NEW";
                break;
            case DBE_LOGREC_CREATEINDEX:
                rectypename = "DBE_LOGREC_CREATEINDEX";
                break;
            case DBE_LOGREC_DROPTABLE:
                rectypename = "DBE_LOGREC_DROPTABLE";
                break;
            case DBE_LOGREC_DROPINDEX:
                rectypename = "DBE_LOGREC_DROPINDEX";
                break;
            case DBE_LOGREC_CREATEVIEW:
                rectypename = "DBE_LOGREC_CREATEVIEW";
                break;
            case DBE_LOGREC_CREATEVIEW_NEW:
                rectypename = "DBE_LOGREC_CREATEVIEW_NEW";
                break;
            case DBE_LOGREC_DROPVIEW:
                rectypename = "DBE_LOGREC_DROPVIEW";
                break;
            case DBE_LOGREC_CREATEUSER:
                rectypename = "DBE_LOGREC_CREATEUSER";
                break;
            case DBE_LOGREC_ALTERTABLE:
                rectypename = "DBE_LOGREC_ALTERTABLE";
                break;
            case DBE_LOGREC_INSTUPLEWITHBLOBS:
                rectypename = "DBE_LOGREC_INSTUPLEWITHBLOBS";
                break;
            case DBE_LOGREC_INSTUPLENOBLOBS:
                rectypename = "DBE_LOGREC_INSTUPLENOBLOBS";
                break;
            case DBE_LOGREC_INCSYSCTR:
                rectypename = "DBE_LOGREC_INCSYSCTR";
                break;
            case DBE_LOGREC_SETHSBSYSCTR:
                rectypename = "DBE_LOGREC_SETHSBSYSCTR";
                break;
            case DBE_LOGREC_CREATECTR:
                rectypename = "DBE_LOGREC_CREATECTR";
                break;
            case DBE_LOGREC_CREATESEQ:
                rectypename = "DBE_LOGREC_CREATESEQ";
                break;
            case DBE_LOGREC_DROPCTR:
                rectypename = "DBE_LOGREC_DROPCTR";
                break;
            case DBE_LOGREC_DROPSEQ:
                rectypename = "DBE_LOGREC_DROPSEQ";
                break;
            case DBE_LOGREC_INCCTR:
                rectypename = "DBE_LOGREC_INCCTR";
                break;
            case DBE_LOGREC_SETCTR:
                rectypename = "DBE_LOGREC_SETCTR";
                break;
            case DBE_LOGREC_SETSEQ:
                rectypename = "DBE_LOGREC_SETSEQ";
                break;
            case DBE_LOGREC_SWITCHTOPRIMARY:
                rectypename = "DBE_LOGREC_SWITCHTOPRIMARY";
                break;
            case DBE_LOGREC_SWITCHTOSECONDARY:
                rectypename = "DBE_LOGREC_SWITCHTOSECONDARY";
                break;
            case DBE_LOGREC_SWITCHTOSECONDARY_NORESET:
                rectypename = "DBE_LOGREC_SWITCHTOSECONDARY_NORESET";
                break;
            case DBE_LOGREC_CLEANUPMAPPING:
                rectypename = "DBE_LOGREC_CLEANUPMAPPING";
                break;
            case DBE_LOGREC_REPLICATRXSTART:
                rectypename = "DBE_LOGREC_REPLICATRXSTART";
                break;
            case DBE_LOGREC_REPLICASTMTSTART:
                rectypename = "DBE_LOGREC_REPLICASTMTSTART";
                break;
            case DBE_LOGREC_ABORTSTMT:
                rectypename = "DBE_LOGREC_ABORTSTMT";
                break;
            case DBE_LOGREC_RENAMETABLE:
                rectypename = "DBE_LOGREC_RENAMETABLE";
                break;
            case DBE_LOGREC_AUDITINFO:
                rectypename = "DBE_LOGREC_AUDITINFO";
                break;
            case DBE_LOGREC_CREATETABLE_FULLYQUALIFIED:
                rectypename = "DBE_LOGREC_CREATETABLE_FULLYQUALIFIED";
                break;
            case DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED:
                rectypename = "DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED";
                break;
            case DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED:
                rectypename = "DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED";
                break;
            case DBE_LOGREC_MME_INSTUPLEWITHBLOBS:
                rectypename = "DBE_LOGREC_MME_INSTUPLEWITHBLOBS";
                break;
            case DBE_LOGREC_MME_INSTUPLENOBLOBS:
                rectypename = "DBE_LOGREC_MME_INSTUPLENOBLOBS";
                break;
            case DBE_LOGREC_MME_DELTUPLE:
                rectypename = "DBE_LOGREC_MME_DELTUPLE";
                break;
            case DBE_LOGREC_HSBG2_DURABLE:
                rectypename = "DBE_LOGREC_HSBG2_DURABLE";
                break;
            case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
                rectypename = "DBE_LOGREC_HSBG2_REMOTE_DURABLE";
                break;
            case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
                rectypename = "DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK";
                break;

#ifdef DBE_REPLICATION
            case DBE_LOGREC_COMMITTRX_NOFLUSH_HSB_OLD:
                rectypename = "DBE_LOGREC_COMMITTRX_NOFLUSH_HSB_OLD";
                break;
#endif

            case DBE_LOGREC_HSBG2_NEW_PRIMARY:
                rectypename = "DBE_LOGREC_HSBG2_NEW_PRIMARY";
                break;

            case DBE_LOGREC_COMMENT:
                rectypename = "DBE_LOGREC_COMMENT";
                break;

            case DBE_LOGREC_HSBG2_ABORTALL:
                rectypename = "DBE_LOGREC_HSBG2_ABORTALL";
                break;

            case DBE_LOGREC_HSBG2_SAVELOGPOS:
                rectypename = "DBE_LOGREC_HSBG2_SAVELOGPOS";
                break;

            case DBE_LOGREC_HSBG2_NEWSTATE:
                rectypename = "DBE_LOGREC_HSBG2_NEWSTATE";
                break;

            case DBE_LOGREC_TRUNCATETABLE:
                rectypename = "DBE_LOGREC_TRUNCATETABLE";
                break;

            case DBE_LOGREC_TRUNCATECARDIN:
                rectypename = "DBE_LOGREC_TRUNCATECARDIN";
                break;

            case DBE_LOGREC_FLUSHTODISK:
                rectypename = "DBE_LOGREC_FLUSHTODISK";
                break;

            default:
                {
                    static char buf[30];
                    SsSprintf(buf, "Rectype = %d", (int)rectype);
                    rectypename = buf;
                }
                ss_rc_derror((int)rectype);
                break;
        }
        return((char *)rectypename);
}

dbe_ret_t dbe_logi_new_primary(
        dbe_db_t* db,
        long originator_nodeid,
        long primary_nodeid)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        dbe_log_t* log;

        dbe_db_setchanged(db, NULL);

        log = dbe_db_getlog(db);

        if (log != NULL) {
            FAKE_CODE_BLOCK_GT(
                FAKE_HSBG2_SWITCH_LOGREC_SLEEP, 0,
                {
                    SsPrintf("%s:FAKE_HSBG2_SWITCH_LOGREC_SLEEP%d secs\n", __FILE__, fake_cases[FAKE_HSBG2_SWITCH_LOGREC_SLEEP]);
                    SsThrSleep(1000 * fake_cases[FAKE_HSBG2_SWITCH_LOGREC_SLEEP]);
                }
            );
            rc = dbe_log_put_hsb_new_primary(
                    log,
                    originator_nodeid,
                    primary_nodeid);
        }

        return(rc);
}

dbe_ret_t dbe_logi_loghsbsysctr(
        dbe_db_t* db)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        dbe_log_t* log;

        dbe_db_setchanged(db, NULL);

        log = dbe_db_getlog(db);

        if (log != NULL) {
            char* ctr_data;
            int ctr_size;

            dbe_db_getreplicacounters(db, TRUE, &ctr_data, &ctr_size);

            rc = dbe_log_puthsbsysctr(log, ctr_data);

            SsMemFree(ctr_data);
        }

        return(rc);
}

dbe_ret_t dbe_logi_put_comment(
        dbe_db_t* db,
        char* data,
        size_t datasize)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        dbe_log_t* log;

        dbe_db_setchanged(db, NULL);

        log = dbe_db_getlog(db);

        if (log != NULL) {
            rc = dbe_log_put_comment(
                    log,
                    data,
                    datasize);
        }
        return(rc);
}

#ifdef SS_HSBG2
dbe_ret_t dbe_logi_put_hsb_durable(
        dbe_db_t* db)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        dbe_log_t* log;
        dbe_catchup_logpos_t dummy_logpos;

        log = dbe_db_getlog(db);

        if (log != NULL) {
            DBE_CATCHUP_LOGPOS_SET_NULL(dummy_logpos);
            rc = dbe_log_put_durable(
                    log,
                    NULL,
                    dummy_logpos);
        }
        return(rc);
}

dbe_ret_t dbe_logi_put_hsb_remote_durable_ack(
        dbe_db_t* db,
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos)
{
        dbe_ret_t rc;
        dbe_log_t* log;

        log = dbe_db_getlog(db);

        ss_dprintf_1(("dbe_logi_put_hsb_remote_durable_ack:getlog %ld\n", (long)log));

        if (log != NULL) {
            rc = dbe_log_put_remote_durable_ack(
                        log,
                        NULL,
                        local_durable_logpos,
                        remote_durable_logpos);
        } else {
            rc = DBE_RC_SUCC;
        }
        return(rc);
}

bool dbe_logi_commitinfo_iscommit(dbe_logi_commitinfo_t ci)
{
        if (!SU_BFLAG_TEST(ci, DBE_LOGI_COMMIT_HSBPRIPHASE1|DBE_LOGI_COMMIT_HSBPRIPHASE2)) {
            ss_dprintf_4(("No PHASE, execute\n"));
            return(TRUE);
        }
        if (SU_BFLAG_TEST(ci, DBE_LOGI_COMMIT_HSBPRIPHASE1)) {
            if (SU_BFLAG_TEST(ci, DBE_LOGI_COMMIT_LOCAL)) {
                ss_dprintf_4(("HSBPRIPHASE1+LOCAL, skip\n"));
                return(FALSE);
            } else {
                ss_dprintf_4(("HSBPRIPHASE1, not local, execute\n"));
                return(TRUE);
            }
        } else {
            ss_dassert(SU_BFLAG_TEST(ci, DBE_LOGI_COMMIT_HSBPRIPHASE2));
            if (!SU_BFLAG_TEST(ci, DBE_LOGI_COMMIT_LOCAL)) {
                ss_dprintf_4(("HSBPRIPHASE2, not local, skip\n"));
                return(FALSE);
            } else {
                ss_dprintf_4(("HSBPRIPHASE2+DBE_LOGI_COMMIT_LOCAL, execute\n"));
                return(TRUE);
            }
        }
}

# endif /* SS_HSBG2 */


#ifdef SS_BETA

static void logi_printfoneinfo(
        void* fp,
        long counts[DBE_LOGREC_1STUNUSED])
{
        int i;

        for (i = 0; i < DBE_LOGREC_1STUNUSED; i++) {
            if (counts[i] != 0) {
                SsFprintf(fp, "%-38s %6ld\n", dbe_logi_getrectypename(i), counts[i]);
            }
        }
}

void dbe_logi_printfinfo(
        void* fp)
{
        SsFprintf(fp, "flushcount\n");
        logi_printfoneinfo(fp, dbe_logrectype_flushcount);
        SsFprintf(fp, "waitflushqueuecount\n");
        logi_printfoneinfo(fp, dbe_logrectype_waitflushqueuecount);
        SsFprintf(fp, "writecount\n");
        logi_printfoneinfo(fp, dbe_logrectype_writecount);
}

void dbe_logi_clearlogrecinfo(void)
{
        memset(dbe_logrectype_flushcount, '\0', sizeof(dbe_logrectype_flushcount));
        memset(dbe_logrectype_waitflushqueuecount, '\0', sizeof(dbe_logrectype_waitflushqueuecount));
        memset(dbe_logrectype_writecount, '\0', sizeof(dbe_logrectype_waitflushqueuecount));
}

#endif /* BETA */

