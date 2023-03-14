/*************************************************************************\
**  source       * dbe0logi.h
**  directory    * dbe
**  description  * Public interface to the database engine log
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


#ifndef DBE0LOGI_H
#define DBE0LOGI_H

#include <su0bflag.h>

#include "dbe0catchup.h"

typedef enum {
        DBE_LOGI_COMMIT_HSBPRIPHASE1    = SU_BFLAG_BIT(0),
        DBE_LOGI_COMMIT_HSBPRIPHASE2    = SU_BFLAG_BIT(1),
        DBE_LOGI_COMMIT_NOFLUSH         = SU_BFLAG_BIT(2),
        DBE_LOGI_COMMIT_DDRELOAD        = SU_BFLAG_BIT(3),
        DBE_LOGI_COMMIT_LOCAL           = SU_BFLAG_BIT(4),
        DBE_LOGI_COMMIT_2SAFE           = SU_BFLAG_BIT(5)
} dbe_logi_commitinfo_t;

#define DBE_LOGI_ISVALIDCOMMITINFO(i)   (((i) & ~(1+2+4+8+16+32)) == 0)

/* Record type numbers.
 * they all have to range from 0 to 255, because
 * only one byte is used for them in the log file
 */
typedef enum {
        DBE_LOGREC_NOP                      = 0,/* No OPeration, for padding */
        DBE_LOGREC_HEADER                   = 1,/* Log file header */

        /* Tuple operations in index
         */
        DBE_LOGREC_INSTUPLE                 = 2,/* Insert (or updated data) (deprecated) */
        DBE_LOGREC_DELTUPLE                 = 3,/* Delete or (delete old in update) */

        /* BLOB operations
         */
        DBE_LOGREC_BLOBSTART_OLD            = 4,/* BLOB Id record */
        DBE_LOGREC_BLOBALLOCLIST_OLD        = 5,/* BLOB allocation list */
        DBE_LOGREC_BLOBALLOCLIST_CONT_OLD   = 6,/* BLOB all. list continuation */
        DBE_LOGREC_BLOBDATA_OLD             = 7,/* BLOB raw data */
        DBE_LOGREC_BLOBDATA_CONT_OLD        = 8,/* BLOB raw data continuation */
        /* Transaction marks
         */
        DBE_LOGREC_ABORTTRX_OLD             = 9,/* Abort mark (informational only) */
        DBE_LOGREC_COMMITTRX_OLD            =10,/* Commit mark */
        DBE_LOGREC_PREPARETRX               =11,/* Prep. mark for 2 Phase Commit */

        /* Global markers
         */
        DBE_LOGREC_CHECKPOINT_OLD           =12,
        DBE_LOGREC_SNAPSHOT_OLD             =13,
        DBE_LOGREC_DELSNAPSHOT              =14,

        /* Data dictionary operations
         */
        DBE_LOGREC_CREATETABLE              =15,
        DBE_LOGREC_CREATEINDEX              =16,
        DBE_LOGREC_DROPTABLE                =17,
        DBE_LOGREC_DROPINDEX                =18,
        DBE_LOGREC_CREATEVIEW               =19,
        DBE_LOGREC_DROPVIEW                 =20,
        DBE_LOGREC_CREATEUSER               =21,
        DBE_LOGREC_ALTERTABLE               =22,

        /* Two new insert tuple operations in
         * order to support "ALTER TABLE" DD-operation.
         * These two codes replace the old DBE_LOGREC_INSTUPLE
         */
        DBE_LOGREC_INSTUPLEWITHBLOBS        =23,
        DBE_LOGREC_INSTUPLENOBLOBS          =24,

        /* Commit statement is for statement atomicity */
        DBE_LOGREC_COMMITSTMT               =25,

        /* Internal ID counter increment */
        DBE_LOGREC_INCSYSCTR                =26,

        /* Counter and Sequence support */
        DBE_LOGREC_CREATECTR                =27,
        DBE_LOGREC_CREATESEQ                =28,
        DBE_LOGREC_DROPCTR                  =29,
        DBE_LOGREC_DROPSEQ                  =30,
        DBE_LOGREC_INCCTR                   =31,
        DBE_LOGREC_SETCTR                   =32,
        DBE_LOGREC_SETSEQ                   =33,

        /* New checkpoint/snapshot records with timestamp */
        DBE_LOGREC_CHECKPOINT_NEW           =34,
        DBE_LOGREC_SNAPSHOT_NEW             =35,

#ifdef DBE_REPLICATION
        DBE_LOGREC_SWITCHTOPRIMARY          =36,
        DBE_LOGREC_SWITCHTOSECONDARY        =37,
        DBE_LOGREC_REPLICATRXSTART          =38,
        DBE_LOGREC_REPLICASTMTSTART         =39,
        DBE_LOGREC_ABORTSTMT                =40,
#endif /* DBE_REPLICATION */

        DBE_LOGREC_CREATETABLE_NEW          =41,
        DBE_LOGREC_CREATEVIEW_NEW           =42,
        DBE_LOGREC_RENAMETABLE              =43,
        
        DBE_LOGREC_AUDITINFO                =44,

        /* create table/view with both catalog and schema */
        DBE_LOGREC_CREATETABLE_FULLYQUALIFIED =45,
        DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED  =46,
        DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED =47,
        
        DBE_LOGREC_COMMITTRX_NOFLUSH_OLD      =48,/* Commit mark that is not
                                                     flushed to disk. Used
                                                     only internally, never
                                                     written to disk. */
        DBE_LOGREC_COMMITTRX_HSB_OLD          =49,/* Commit that is participated
                                                     to hsb replication. */
        
        DBE_LOGREC_SETHSBSYSCTR               =50,/* Set system counter value. */
#ifdef DBE_REPLICATION
        DBE_LOGREC_SWITCHTOSECONDARY_NORESET  =51,
        DBE_LOGREC_CLEANUPMAPPING             =52,
        DBE_LOGREC_HSBCOMMITMARK_OLD          =53,
        DBE_LOGREC_COMMITTRX_NOFLUSH_HSB_OLD  =54,
#endif
        DBE_LOGREC_BLOBG2DATA                 =55,/* new format blob data */

        DBE_LOGREC_HSBG2_DURABLE              =56,
        DBE_LOGREC_HSBG2_REMOTE_DURABLE       =57,
        DBE_LOGREC_HSBG2_NEW_PRIMARY          =58, /* (switch) */

        /* Two new insert tuple operations for MME */
        DBE_LOGREC_MME_INSTUPLEWITHBLOBS      =60,
        DBE_LOGREC_MME_INSTUPLENOBLOBS        =61,
        DBE_LOGREC_MME_DELTUPLE               =62,
        DBE_LOGREC_COMMENT                    =63,
        
        DBE_LOGREC_BLOBG2DROPMEMORYREF        =64,
        DBE_LOGREC_BLOBG2DATACOMPLETE         =65,
        
        DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK   =66,
        DBE_LOGREC_COMMITTRX_INFO             =67,/* Commit mark with info bits */
        DBE_LOGREC_ABORTTRX_INFO              =68,/* Abort mark with info bits */

        DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT    =69,
        DBE_LOGREC_HSBG2_ABORTALL             =70,
        DBE_LOGREC_HSBG2_SAVELOGPOS           =71, /* Virtual log mark, not written to disk. */
        DBE_LOGREC_HSBG2_NEWSTATE             =72,

        DBE_LOGREC_TRUNCATETABLE              =73,
        DBE_LOGREC_TRUNCATECARDIN             =74,
        
        DBE_LOGREC_FLUSHTODISK                =75, /* Virtual log mark, not written to disk. */

        DBE_LOGREC_1STUNUSED                  =76
} dbe_logrectype_t;

ss_beta(extern long dbe_logrectype_flushcount[DBE_LOGREC_1STUNUSED];)
ss_beta(extern long dbe_logrectype_waitflushqueuecount[DBE_LOGREC_1STUNUSED];)
ss_beta(extern long dbe_logrectype_writecount[DBE_LOGREC_1STUNUSED];)

typedef enum {
        DBE_LOG_INSTANCE_LOGGING_STANDALONE,
        DBE_LOG_INSTANCE_LOGGING_HSB,
        DBE_LOG_INSTANCE_TRANSFORM_CATCHUP,
        DBE_LOG_INSTANCE_TRANSFORM_RECOVERY
} dbe_log_instancetype_t;

char *dbe_logi_getrectypename(
        dbe_logrectype_t rectype);

dbe_ret_t dbe_logi_put_comment(
        dbe_db_t* db, 
        char* data,
        size_t datasize);

dbe_ret_t dbe_logi_put_hsb_durable(
        dbe_db_t* db);

dbe_ret_t dbe_logi_put_hsb_remote_durable_ack(
        dbe_db_t* db,
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos);

#ifdef SS_HSBG2

dbe_ret_t dbe_logi_new_primary(
        dbe_db_t* db, 
        long originator_nodeid, 
        long new_primary_nodeid);

dbe_ret_t dbe_logi_loghsbsysctr(
        dbe_db_t* db);

bool dbe_logi_commitinfo_iscommit(
        dbe_logi_commitinfo_t ci);

#endif /* SS_HSBG2 */

#ifdef SS_BETA

void dbe_logi_printfinfo(
        void* fp);

void dbe_logi_clearlogrecinfo(
        void);

#endif /* BETA */

#endif /* DBE0LOGI_H */
