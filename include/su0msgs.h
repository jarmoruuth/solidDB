/*************************************************************************\
**  source       * su0msgs.h
**  directory    * su
**  description  * Solid error and message texts.
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


#ifndef SU0MSGS_H
#define SU0MSGS_H

typedef enum {
        SU_RCTYPE_RETCODE,
        SU_RCTYPE_WARNING,
        SU_RCTYPE_ERROR,
        SU_RCTYPE_MSG,
        SU_RCTYPE_FATAL
} su_rctype_t;

/* Return code test data structure.
 */
typedef struct {
        int         rct_rc;     /* su_ret_t ir su_msgret_t */
        su_rctype_t rct_type;
        const char*       rct_enumname;
        const char*       rct_text;
} su_rc_text_t;

typedef enum {
        SU_RC_SUBSYS_SU,
        SU_RC_SUBSYS_DBE,
        SU_RC_SUBSYS_TAB,
        SU_RC_SUBSYS_SES,
        SU_RC_SUBSYS_COM,
        SU_RC_SUBSYS_SRV,
        SU_RC_SUBSYS_SQL,
        SU_RC_SUBSYS_SP ,
        SU_RC_SUBSYS_SAP,
        SU_RC_SUBSYS_XS ,
        SU_RC_SUBSYS_RPC,
        SU_RC_SUBSYS_SNC,
        SU_RC_SUBSYS_HSB,
        SU_RC_SUBSYS_SSA,
        SU_MSG_SUBSYS_COM,
        SU_MSG_SUBSYS_SRV,
        SU_MSG_SUBSYS_DBE,
        SU_MSG_SUBSYS_CP,
        SU_MSG_SUBSYS_BCKP,
        SU_MSG_SUBSYS_AT,
        SU_MSG_SUBSYS_LOG,
        SU_MSG_SUBSYS_INI,
        SU_MSG_SUBSYS_HSB,
        SU_MSG_SUBSYS_RPC,
        SU_MSG_SUBSYS_SNC,
        SU_MSG_SUBSYS_XS,
        SU_MSG_SUBSYS_FIL,
        SU_MSG_SUBSYS_TAB,
        SU_RC_NSUBSYS
} su_rc_subsys_t;

typedef enum {
        MSG_MSGBEGIN = 30000,

        COM_MSG_USER_CONNECTED_SDSS,                    /* 30001 */ 
        COM_MSG_USER_CONNECTION_TIMEOUT_SSDS,           /* 30002 */
        COM_MSG_USER_DISCONNECTED_ABNORMALLY_SSDSS,     /* 30003 */
        COM_MSG_USER_DISCONNECTED_SSDS,                 /* 30004 */
        COM_MSG_ADMIN_USER_CONNECTED_SDSS,              /* 30005 */
        COM_MSG_RCON_USER_CONNECTED_SDSS,               /* 30006 */
        COM_MSG_TRX_IDLETIMEOUT_SDS,                    /* 30007 */
        COM_MSG_TRX_TIMEOUT_SDS,                        /* 30008 */
        COM_MSG_ILLEGAL_USERNAME_SSS,                   /* 30009 */
        COM_MSG_VERSION_MISMATCH_SDDS,                  /* 30010 */
        COM_MSG_COLLATION_VERSION_MISMATCH_SS,          /* 30011 */
        COM_MSG_TOO_MANY_CLIENTS_SS,                    /* 30012 */
        COM_MSG_NEW_CONNECTIONS_ALLOWED,                /* 30013 */
        COM_MSG_NEW_CONNECTIONS_NOT_ALLOWED,            /* 30014 */
        COM_MSG_NO_CONNECTIONS_ALLOWED,                 /* 30015 */
        COM_MSG_LISTENING_STARTED_SS,                   /* 30016 */
        COM_MSG_LISTENING_STOPPED_SS,                   /* 30017 */
        COM_MSG_NO_VALID_LISTENING_NAME_S,              /* 30018 */
        COM_MSG_CANNOT_START_LISTENING_S,               /* 30019 */

        COM_MSG_SRV_IN_FATAL_STATE,                     /* 30020 */
        COM_MSG_UNKNOWN_CONNECTION_RECYCLING,           /* 30021 */

        SRV_MSG_SHUTDOWN_BY_APP = 30100,                /* 30100 */
        SRV_MSG_SHUTDOWN_BY_KILL,                       /* 30101 */
        SRV_MSG_SHUTDOWN_BY_USER_SD,                    /* 30102 */
        SRV_MSG_SHUTDOWN_BY_UNKNOWN_USER,               /* 30103 */
        SRV_MSG_SHUTDOWN_ABORTED,                       /* 30104 */
        SRV_MSG_SHUTDOWN_S,                             /* 30105 */
        SRV_MSG_THREAD_STILL_ACTIVE_D,                  /* 30106 */

        SRV_MSG_SERVICE_INSTALLED_S = 30110,            /* 30110 */
        SRV_MSG_SERVICE_REMOVED_S,                      /* 30111 */
        SRV_MSG_SERVICE_INSTALL_FAILED_SDS,             /* 30112 */
        SRV_MSG_SERVICE_REMOVE_FAILED_SDS,              /* 30113 */
        SRV_MSG_SERVICE_USAGE,                          /* 30114 */
        SRV_MSG_FAILED_TO_CHANGE_DIR_S,                 /* 30115 */
        SRV_MSG_WORKING_DIR_CHANGED_S,                  /* 30116 */
        SRV_MSG_VERSION_S,                              /* 30117 */
        SRV_MSG_COPYRIGHT_S,                            /* 30118 */
        SRV_MSG_STARTUP_TIME_MSG_S,                     /* 30119 */

        SRV_MSG_FAILED_TO_START_SS = 30120,             /* 30120 */
        SRV_MSG_ACCESS_VIOLATION,                       /* 30121 */
        SRV_MSG_INTENTIONAL_INTERNAL_ERROR,             /* 30122 */
        SRV_MSG_ACMD_ERROREXIT_D,                       /* 30123 */
        SRV_MSG_ACMD_ASSERT,                            /* 30124 */
        SRV_MSG_ADMIN_COMMAND_S,                        /* 30125 */
        SRV_MSG_ADMIN_EVENT_S,                          /* 30126 */
        SRV_MSG_INVALID_LICENSE_S,                      /* 30127 */
        SRV_MSG_USING_LICENSE_S,                        /* 30128 */
        SRV_MSG_SIGNAL_D,                               /* 30129 */
        SRV_MSG_ASSERTION,                              /* 30130 */
        SRV_MSG_COMMAND_LINE_S,                         /* 30131 */
        SRV_MSG_SSDEBUG_SETTING_S,                      /* 30132 */
        SRV_MSG_ASYNC_PING_TEST_COMPLETED_S,            /* 30133 */
        SRV_MSG_TOO_LONG_INIFILENAME,                   /* 30134 */
        
        SRV_MSG_PAGEDMEM_OPTION_ERROR = 30140,          /* 30140 */
        SRV_MSG_TESTING_SYSTEM_PERF,                    /* 30141 */
        SRV_MSG_TESTING_WAS_SUCCESSFUL,                 /* 30142 */
        SRV_MSG_TESTING_FAILED,                         /* 30143 */
        SRV_MSG_NOTSUPINBACKUPMODE,                     /* 30144 */
        SRV_MSG_ILLUSERORPASS,                          /* 30145 */
        SRV_MSG_THRCREATEFAIL_S,                        /* 30146 */
        SRV_MSG_HSBCONF_WITHNOLICENSE,                  /* 30147 */
        SRV_MSG_OPTION_SSSSS,                           /* 30148 */
        SRV_FATAL_ABORT,                                /* 30149 */
        SRV_FATAL_NOT_STARTED,                          /* 30150 */
        SRV_MSG_DB_STARTED,                             /* 30151 */
        SRV_MSG_MEMALLOC_EXCEEDED_UUU,                  /* 30152 */
        SRV_MSG_MEMALLOC_DECREASED_UUU,                 /* 30153 */
        SRV_MSG_STMTMEMTRACELIMIT_EXCEEDED_LDDDLDS,     /* 30154 */

        DBE_MSG_CREATING_NEW_DB = 30200,                /* 30200 */ 
        DBE_MSG_DB_CONVERTED,                           /* 30201 */
        DBE_MSG_DB_ALREADY_EXISTS,                      /* 30202 */
        DBE_MSG_CONVERTING_DB,                          /* 30203 */
        DBE_MSG_PLEASE_CONVERT_DB,                      /* 30204 */
        DBE_MSG_NEW_DB_NOT_CREATED,                     /* 30205 */
        DBE_MSG_CANNOT_CREATE_NEW_DB,                   /* 30206 */
        DBE_MSG_FAILED_TO_OPEN_DB_S,                    /* 30207 */
        DBE_MSG_MERGE_DENIED,                           /* 30208 */
        DBE_MSG_IDLE_MERGE_STARTED_D,                   /* 30209 */
        DBE_MSG_MERGE_STARTED_SD,                       /* 30210 */
        DBE_MSG_IDLE_QUICK_MERGE_STARTED,               /* 30211 */
        DBE_MSG_QUICK_MERGE_STARTED,                    /* 30212 */
        DBE_MSG_MERGE_STOPPED_ALL_MERGED,               /* 30213 */
        DBE_MSG_MERGE_STOPPED_N_KEYS_MERGED_D,          /* 30214 */
        DBE_MSG_MERGE_TASK_STARTED_D,                   /* 30215 */
        DBE_MSG_MERGE_USER_ENABLED,                     /* 30216 */
        DBE_MSG_PROCEDURE_CONVERT_ERROR_SS,             /* 30217 */
        DBE_MSG_QUICK_MERGE_STOPPED,                    /* 30218 */

        DBE_MSG_CHECKING_INDEX = 30220,                 /* 30220 */
        DBE_MSG_INDEX_OK,                               /* 20221 */
        DBE_MSG_CANNOT_CHECK_INDEX,                     /* 30222 */
        DBE_MSG_TESTING_INDEX,                          /* 30223 */
        DBE_MSG_INDEX_TESTED_OK,                        /* 30224 */
        DBE_MSG_ERR_INDEX_NOT_OK,                       /* 30225 */
        DBE_MSG_FAILED_TO_OPEN_DB_FOR_TESTING,          /* 30226 */
        DBE_MSG_FAILED_TO_CONNECT_DB_FOR_TESTING,       /* 30227 */
        DBE_MSG_SHRINK_OK,                              /* 30228 */
        DBE_MSG_FAILED_TO_SHRINK,                       /* 30229 */

        DBE_MSG_STARTING_RECOVERY = 30230,              /* 30230 */
        DBE_MSG_RECOVERY_OF_TRXS_COMPLETED_U,           /* 30231 */
        DBE_MSG_RECOVERY_COMPLETED,                     /* 30232 */
        DBE_MSG_WRITING_MMEPAGES_D,                     /* 30233 */
        DBE_MSG_WRITING_MMEPAGES_FIN_D,                 /* 30234 */
        DBE_MSG_LOADING_MME_D,                          /* 30235 */
        DBE_MSG_LOADING_MME_FIN_D,                      /* 30236 */
        DBE_MSG_SHRINK_STARTED,                         /* 30237 */

        DBE_MSG_FAILED_TO_CREATE_NEW_DB = 30240,        /* 30240 */
        DBE_MSG_FAILED_TO_LOGON_TO_DB,                  /* 30241 */
        DBE_MSG_CONNECT_FAILS_SCRIPT_NOT_EXECUTED,      /* 30242 */
        DBE_MSG_FAILED_TO_OPEN_SQL_FILE,                /* 30243 */
        DBE_MSG_SCRIPT_FAILED_SS,                       /* 30244 */
        DBE_MSG_TABLE_NOT_FOUND_S,                      /* 30245 */
        DBE_MSG_CONVERTING_TABLE_S,                     /* 30246 */
        DBE_MSG_TABLE_CONVERTED_S,                      /* 30247 */
        DBE_MSG_NO_NEED_TO_CONVERT_TABLE_S,             /* 30248 */
        DBE_MSG_INIT_FILESPECPROBLEM,                   /* 30249 */
        DBE_MSG_USING_SPLITMERGE,                       /* 30250 */
        DBE_MSG_STARTING_DATABASE_RECREATE,             /* 30251 */
        DBE_MSG_DATABASE_DELETE_OK,                     /* 30252 */
        DBE_MSG_ERR_FAILED_TO_DELETE_DB,                /* 30253 */
        DBE_MSG_INIT_BROKENNETCOPY,                     /* 30254 */
        DBE_FATAL_CRASHAFTERCPMARK,                     /* 30255 */
        DBE_FATAL_DBEXISTS,                             /* 30256 */
        DBE_FATAL_DATERESET,                            /* 30257 */
        DBE_FATAL_DATERESET_ONCE,                       /* 30258 */
        DBE_FATAL_ERRORTEST_SD,                         /* 30259 */

        CP_MSG_CREATION_COMPLETED = 30280,              /* 30280 */
        CP_MSG_CREATION_STARTED,                        /* 30281 */
        CP_MSG_CREATION_NOT_STARTED,                    /* 30282 */
        CP_MSG_NOT_STARTED_BECAUSE_DISABLED,            /* 30283 */
        CP_MSG_CREATION_DENIED,                         /* 30284 */
        CP_MSG_CREATION_START_FAILED_SS,                /* 30285 */
        CP_MSG_DBEFLUSH_TIMEOUT_DD,                     /* 30286 */
        CP_MSG_MMEFLUSH_TIMEOUT_DD,                     /* 30287 */
        CP_MSG_MMEFLUSHBATCH_TIMEOUT,                   /* 30288 */

        BACKUP_MSG_COMPLETED = 30300,                   /* 30300 */
        BACKUP_MSG_STARTED_SS,                          /* 30301 */
        BACKUP_MSG_START_FAILED_WITH_MSG_S,             /* 30302 */
        BACKUP_MSG_ABORTED,                             /* 30303 */
        BACKUP_MSG_FAILED_S,                            /* 30304 */
        BACKUP_MSG_START_DENIED,                        /* 30305 */
        BACKUP_MSG_NOT_SUPPORTED,                       /* 30306 */
        BACKUP_MSG_NOT_STARTED_INDEX_FAILURE,           /* 30307 */

        AT_MSG_BACKUP_S = 30350,                        /* 30350 */
        AT_MSG_MAKECP,                                  /* 30351 */
        AT_MSG_THROWOUT_S,                              /* 30352 */
        AT_MSG_REPORT_S,                                /* 30353 */
        AT_MSG_SHUTDOWN,                                /* 30354 */
        AT_MSG_SYSTEM_S,                                /* 30355 */
        AT_MSG_OPEN,                                    /* 30356 */
        AT_MSG_CLOSE,                                   /* 30357 */
        AT_MSG_ASSERT,                                  /* 30358 */
        AT_MSG_TIME_INCONSISTENCY,                      /* 30359 */
        AT_MSG_CMD_FAILED_S,                            /* 30360 */
        AT_MSG_ILLEGAL_CMD_S,                           /* 30361 */
        AT_MSG_ILLEGAL_IMMEDIATE_CMD_S,                 /* 30362 */
        AT_MSG_CLEANBGI_D,                              /* 30363 */
        
        LOG_MSG_DISABLED = 30400,                       /* 30400 */
        LOG_MSG_USING_WRITE_MODE_D,                     /* 30401 */
        LOG_MSG_CONFLICTING_PARAMETERS,                 /* 30402 */
        LOG_MSG_FILE_WRITE_FAILURE,                     /* 30403 */
        LOG_MSG_CHECK_RESULTS_FROM_FILE_S,              /* 30404 */
        LOG_MSG_CANNOT_OPEN_MSGLOG_S,                   /* 30405 */
        LOG_MSG_CANNOT_OPEN_TRACE_FILE_S,               /* 30406 */
        LOG_MSG_IGNORE_CORRUPTED_PART,                  /* 30407 */

        INI_MSG_VALUE_NOT_MULTIPLE_OF_512_USSU = 30450, /* 30450 */
        INI_MSG_INVALID_INDEXFILE_SPEC_SSSD,            /* 30451 */
        INI_MSG_INVALID_VALUE_IGNORE_FOLLOWING_SS,      /* 30452 */
        INI_MSG_ILLEGAL_VALUE_USSU,                     /* 30453 */
        INI_MSG_FAILED_TO_SAVE_FILE_S,                  /* 30454 */
        INI_MSG_FAILED_TO_SET_MAXOPENFILES_DD,          /* 30455 */
        INI_MSG_USING_CONFIG_FILE_D,                    /* 30456 */
        INI_MSG_NO_CONFIG_FILE_USING_DEFAULTS_D,        /* 30457 */
        INI_MSG_ILLEGAL_ISOLATION_VALUE_USSU,           /* 30458 */
        INI_MSG_INVALID_COMMITMAXWAIT_VALUE_DSSD,       /* 30459 */
        INI_MSG_INVALID_RELAXEDMAXDELAY_VALUE_DSSD,     /* 30460 */
        INI_MSG_INVALID_SAFE_ACK_POLICY_VALUE_DSSD,     /* 30461 */
        INI_MSG_INVALID_SAFENESS_LEVEL_VALUE_DSSD,      /* 30462 */
        INI_MSG_READTHREADMODE_TASKTHREAD_IN_HSB,       /* 30463 */
        INI_MSG_ILLEGAL_VALUE_SSSS,                     /* 30464 */

        HSB_MSG_STARTED_AS_PRIMARY = 30500,             /* 30500 */
        HSB_MSG_STARTED_AS_SECONDARY,                   /* 30501 */
        HSB_MSG_STARTED_AS_SECONDARY_WITH_INFO,         /* 30502 */
        HSB_MSG_FORCING_PRIMARY_TO_SECONDARY,           /* 30503 */
        HSB_MSG_SWITCHED_TO_SECONDARY,                  /* 30504 */
        HSB_MSG_SWITCHED_TO_PRIMARY,                    /* 30505 */
        HSB_MSG_SET_PRI_ALONE_OR_SWITCH_TO_SEC,         /* 30506 */
        HSB_MSG_PRIMARY_ALONE_ON,                       /* 30507 */
        HSB_MSG_PRIMARY_ALONE_OFF,                      /* 30508 */
        HSB_MSG_SWITCH_TO_PRIMARY_FAILED_D,             /* 30509 */
        HSB_MSG_SWITCH_TO_SECONDARY_FAILED_D,           /* 30510 */
        HSB_MSG_FAILED_TO_START_HSB_SD,                 /* 30511 */
        HSB_MSG_SWITCH_ROLE_TO_PRIMARY_FAILED_D,        /* 30512 */
        HSB_MSG_SWITCH_ROLE_TO_SECONDARY_FAILED_D,      /* 30513 */
        HSB_MSG_BOTH_ARE_PRIMARY_START_AS_SEC,          /* 30514 */
        HSB_MSG_BOTH_ARE_PRIMARY,                       /* 30515 */
        HSB_MSG_FAILED_TO_START_HSB_REJECTED_SD,        /* 30516 */
        HSB_MSG_ROLE_IN_SECONDARY_SWITCHED,             /* 30517 */
        HSB_MSG_SWITCHED_TO_STANDALONE,                 /* 30518 */

        HSB_MSG_STARTING_CATCHUP = 30530,               /* 30530 */
        HSB_MSG_CATCHUP_COMPLETED,                      /* 30531 */
        HSB_MSG_CATCHUP_ENDED_ABNORMALLY,               /* 30532 */
        HSB_MSG_CANNOT_START_CATCHUP_NOTINSYNC,         /* 30533 */
        HSB_MSG_CATCHUP_ENDED_ABNORMALLY_STATUS_D,      /* 30534 */
        HSB_MSG_CATCHUP_ENDED_ABNORMALLY_ERROR_D,       /* 30535 */
        HSB_MSG_CATCHUP_ENDED_WITH_COM_ERR,             /* 30536 */
        HSB_MSG_CATCHUP_ENDED_WITH_SEC_ERR_S,           /* 30537 */
        HSB_MSG_LOG_SIZE_EXCEEDED_MAXIMIMUM_DD,         /* 30538 */
        HSB_MSG_FILE_ERR_IN_HSBLOG,                     /* 30539 */
        HSB_MSG_STARTING_TO_RECEIVE_CATCHUP,            /* 30540 */
        HSB_MSG_LOGCORRUPTION_NOTINSYNC,                /* 30541 */
        
        HSB_MSG_CONN_TO_SEC_BROKEN = 30550,             /* 30550 */
        HSB_MSG_CONN_TO_SEC_SS,                         /* 30551 */
        HSB_MSG_SEC_CONNECTED,                          /* 30552 */
        HSB_MSG_PRI_CONNECTED,                          /* 30553 */
        HSB_MSG_CONN_BROKEN_WITH_OPEN_TRX,              /* 30554 */
        HSB_MSG_PING_TIMEOUT,                           /* 30555 */
        HSB_MSG_CONN_TO_SEC_BROKEN_WITH_INFO_SS,        /* 30556 */
        HSB_MSG_NOT_PROPERLY_SYNCRONIZED,               /* 30557 */
        HSB_MSG_CONN_TO_SEC_TIMEOUT,                    /* 30558 */
        HSB_MSG_CONN_BROKEN_WITH_ERR_S,                 /* 30559 */
        HSB_MSG_ERROR_MSG_S,                            /* 30560 */

        HSB_MSG_NETCOPY_COMPLETED = 30570,              /* 30570 */
        HSB_MSG_STARTED_TO_RECEIVE_NETCOPY,             /* 30571 */
        HSB_MSG_DB_STARTED,                             /* 30572 */
        HSB_MSG_NETCOPY_FAILED_S,                       /* 30573 */

        HSB_MSG_FORCING_THREADS_TO_1,                   /* 30574 */
        HSB_MSG_CONFIGURED_BUT_LICENSE_MISSING,         /* 30575 */
        HSB_MSG_INVALID_STATE,                          /* 30576 */
        HSB_MSG_CONN_FAILED,                            /* 30577 */
        __NOT_USED_HSB_MSG_DISCONNECT_FAILED,           /* 30578 */
        HSB_MSG_CONN_ALREADY_ACTIVE,                    /* 30579 */
        __NOT_USED_HSB_MSG_COPY_FAILED_NOT_PRIMARY_ALONE,          /* 30580 */
        HSB_MSG_UNKNOWN_EVENT_S,                        /* 30581 */
        HSB_MSG_SET_PRIMARY_ALONE_FAILED_S,             /* 30582 */
        HSB_MSG_COPY_FAILED_S,                          /* 30583 */
        __NOTUSED_HSB_MSG_NOTSUPP_IN_SECONDARY,         /* 30584 */
        HSB_MSG_STARTED_TO_LISTEN_NETCOPY,              /* 30585 */

        RPC_MSG_ILLEGAL_FREEARRAY_SIZE_D = 30600,       /* 30600 */
        RPC_MSG_ILLEGAL_ATTR_COUNT_DS,                  /* 30601 */
        RPC_MSG_ILLEGAL_RELOP_DS,                       /* 30602 */
        RPC_MSG_ILLEGAL_TABLE_NAME_SS,                  /* 30603 */
        RPC_MSG_ILLEGAL_SELFLAGS_SIZE_DS,               /* 30604 */
        RPC_MSG_CURRENT_CURID_IN_FREEARR_D,             /* 30605 */
        RPC_MSG_ILLEGAL_CURID_IN_FREEARR_D,             /* 30606 */
        RPC_MSG_ILLEGAL_USERID_D,                       /* 30607 */
        RPC_MSG_ILLEGAL_CONNECTID_D,                    /* 30608 */
        RPC_MSG_ILLEGAL_SEQ_NUMBER_DD,                  /* 30609 */
        RPC_MSG_ILLEGAL_CURID_D,                        /* 30610 */
              
        RPC_MSG_ILLEGAL_ATTRID_IN_ORDERLIST_D,          /* 30611 */
        RPC_MSG_ILLEGAL_ATTRID_IN_CONSTLIST_D,          /* 30612 */
        RPC_MSG_ILLEGAL_ATTRID_IN_SELLIST_D,            /* 30613 */
        RPC_MSG_ILLEGAL_LENGTH_PARAM_DS,                /* 30614 */
        RPC_MSG_ILLEGAL_ATTR_NUMBER_PARAM_SD,           /* 30615*/

        RPC_MSG_CANNOT_SEND_UNICODE_OLD_CLIENT,         /* 30616*/
        RPC_MSG_ILLEGAL_TYPE_NUMBER_SDDD,               /* 30617*/
        RPC_MSG_ILLEGAL_DATE_ATTR_JAVA_CLIENT_S,        /* 30618*/
        RPC_MSG_ILLEGAL_ATTR_TYPE_PARAM_SD,             /* 30619*/
        RPC_MSG_CORRUPTED_TUPLE_S,                      /* 30620*/

        RPC_MSG_ILLEGAL_SQLCUR_SYNCARRSIZE_D,           /* 30621*/
        RPC_MSG_ILLEGAL_SQLCURID_D,                     /* 30622*/
        RPC_MSG_ILLEGAL_CONSOLE_INFO,                   /* 30623*/
        RPC_MSG_ILLEGAL_SESSION,                        /* 30624*/
        RPC_MSG_ILLEGAL_DONEARRSIZE_D,                  /* 30625*/

        RPC_MSG_ILLEGAL_SQL_STMTID_DS,                  /* 30626*/
        RPC_MSG_ILLEGAL_SQL_STMTID_WITH_POS_DDS,        /* 30627*/
        RPC_MSG_ILLEGAL_READ_BLOBID_DS,                 /* 30628*/
        RPC_MSG_ILLEGAL_READ_BLOB_BUFSIZE_DS,           /* 30629*/
        RPC_MSG_BLOB_DATA_CRC_FAILED_DS,                /* 30630*/
        RPC_MSG_ILLEGAL_BLOBID_DS,                      /* 30631*/
        RPC_MSG_ILLEGAL_BLOB_PIECE_LENGTH_US,           /* 30632*/
        RPC_MSG_ILLEGAL_DATA_LENGTH_SU,                 /* 30633*/
        RPC_MSG_ILLEGAL_TUPLE_POSITION_D,               /* 30634*/
        RPC_MSG_ILLEGAL_HSB_COUNTER_D,                  /* 30635 */
        RPC_MSG_ILLEGAL_REP_TYPE_PARAM_D,               /* 30636 */

        RCP_MSG_PING_CLIENT_CONNECTED_S,                /* 30637 */
        RPC_MSG_PING_CLIENT_DISCONNECTED_S,             /* 30638 */
        RPC_MSG_ILLEGAL_CURID_WITH_INFO_DS,             /* 30639 */
        RPC_MSG_SRV_ERROR_S,                            /* 30640 */

        SNC_MSG_START_PARALLEL_SYNC_HISTKEY_CONV = 30700,    /* 30700 */
        SNC_MSG_START_SYNC_HISTKEY_CONV,                     /* 30701 */
        SNC_MSG_SYNC_HISTKEY_CONV_DONE,                      /* 30702 */
        SNC_MSG_ERR_NOTMASTERDB,                             /* 30703 */

        HSBG2_MSG_CONNECT_ACTIVE = 30750,               /* 30750 */
        __NOT_USED_HSBG2_MSG_CATCHUP_ACTIVE,            /* 30751 */
        HSBG2_MSG_DISCONNECT_ACTIVE,                    /* 30752 */
        __NOT_USED_HSBG2_MSG_COPY_ACTIVE,               /* 30753 */
        __NOT_USED_HSBG2_MSG_STANDALONE_ONLY_WHEN_ALONE,/* 30754 */
        HSBG2_ASSERT,                                   /* 30755 (pseudoerror)*/
        HSBG2_IGNORE,                                   /* 30756 (pseudoerror)*/
        HSBG2_MSG_SUCC_CONNECTED,                       /* 30757 */
        HSBG2_MSG_BAD_HOTSTANDBY_CMD,                   /* 30758 */
        HSBG2_MSG_SET_STANDALONE,                       /* 30759 */
        HSBG2_MSG_DISCONNECT_STARTED,                   /* 30760 */
        HSBG2_MSG_SWITCH_TO_PRIMARY_STARTED,            /* 30761 */
        HSBG2_MSG_SWITCH_TO_SECONDARY_STARTED,          /* 30762 */
        HSBG2_MSG_CONNECT_STARTED,                      /* 30763 */
        HSBG2_MSG_COPY_STARTED,                         /* 30764 */
        HSBG2_MSG_PARAMETER_PRIMARYALONE_YES,           /* 30765 */
        HSBG2_MSG_PARAMETER_PRIMARYALONE_NO,            /* 30766 */
        HSBG2_MSG_PARAMETER_CONNECT_S,                  /* 30767 */
        HSBG2_MSG_CONNECTION_BROKEN,                    /* 30768 */
        HSBG2_MSG_CONNECTION_ACTIVE,                    /* 30769 */
        __NOT_USED_HSBG2_MSG_BOTH_ARE_PRIMARY,          /* 30770 */
        __NOT_USED_HSBG2_MSG_BOTH_ARE_SECONDARY,        /* 30771 */
        __NOTUSED_HSBG2_MSG_NO_NODEID,                  /* 30772 */
        __NOT_USED_HSBG2_ERR_STANDALONE,                /* 30773 */
        HSBG2_MSG_ALREADY_STANDALONE,                   /* 30774 */
        HSBG2_MSG_PARAMETER_COPYDIR_S,                  /* 30775 */
        HSBG2_MSG_PARAMETER_CONNECTTIMEOUT_D,           /* 30776 */
        HSBG2_MSG_PARAMETER_PINGTIMEOUT_D,              /* 30777 */
        __NOTUSED_HSBG2_MSG_30778,                      /* 30778 */
        HSBG2_MSG_MIGRATING,                            /* 30779 */
        __NOTUSED_HSBG2_MSG_30780,                      /* 30780 */
        __NOTUSED_HSBG2_MSG_30781,                      /* 30781 */
        HSBG2_MSG_ALREADY_PRIMARY_ALONE,                /* 30782 */
        HSBG2_MSG_ALREADY_SECONDARY_ALONE,              /* 30783 */
        HSBG2_MSG_PARAMETER_SET_SD,                     /* 30784 */
        HSBG2_MSG_PARAMETER_SET_SS,                     /* 30785 */
        HSBG2_MSG_PARAMETER_SETSTR_SS,                  /* 30786 */
        HSBG2_FATAL_DOLOGSKIP_DLDLD,                    /* 30787 */
        HSBG2_FATAL_LOGCOPY_WRI_DLDLD,                  /* 30788 */
        HSBG2_FATAL_LOG_OPEN_S,                         /* 30789 */
        HSBG2_FATAL_RESETFILE_LD,                       /* 30790 */
        HSBG2_FATAL_BADTYPE_DLDLD,                      /* 30791 */
        HSBG2_MSG_BOTH_ARE_SECONDARY,                   /* 30792 */

        XS_MSG_UNABLE_TO_RESERVE_MEM_INFO_DDS = 30800,  /* 30800 */
        XS_MSG_UNABLE_TO_RESERVE_MEM_DD,                /* 30801 */
        XS_FATAL_TEMPFILE_D,                            /* 30802 */
        XS_FATAL_PARAM_SSUUU,                           /* 30803 */
        XS_FATAL_DIR_S,                                 /* 30804 */

        FIL_MSG_SSBLOCK_FAILED_SD = 30900,              /* 30900  */
        FIL_MSG_SSBLOCK_FAILED_SDD,                     /* 30901  */
        FIL_MSG_SSBOPENLOCAL_FAILED_SDDD,               /* 30902  */
        FIL_MSG_SSBOPENLOCAL_FAILED_DOS_SDDD,           /* 30903  */
        FIL_MSG_SSBOPENLOCAL_FAILED_DOS_SDD,            /* 30904  */
        FIL_MSG_SSBOPEN_FAILED_SDD,                     /* 30905  */
        FIL_MSG_FILEFLUSH_FAILED_DS,                    /* 30906  */
        FIL_MSG_FILEFLUSH_FAILED_DOS_DDS,               /* 30907  */
        FIL_MSG_FILEFLUSHCLOSE_FAILED_DS,               /* 30908  */
        FIL_MSG_FILEFLUSHOPEN_FAILED_DS,                /* 30909  */
        FIL_MSG_FILESIZEQUERY_FAILED_DSD,               /* 30910  */
        FIL_MSG_FILESIZEQUERYSEEK_FAILED_S,             /* 30911  */
        FIL_MSG_FILESIZECHANGE_FAILED_DSDD,             /* 30912  */
        FIL_MSG_FILESIZECHANGE_FAILED_S,                /* 30913  */
        FIL_MSG_FILEREAD_FAILED_DSDD,                   /* 30914  */
        FIL_MSG_FILEREADSEEK_FAILED_DSDD,               /* 30915  */
        FIL_MSG_FILEWRITE_FAILED_DSDD,                  /* 30916  */
        FIL_MSG_FILEWRITESEEK_FAILED_DSDD,              /* 30917  */
        FIL_MSG_FILEWRITEEND_FAILED_DSD,                /* 30918  */
        FIL_MSG_FILEAPPEND_FAILED_DSD,                  /* 30919  */
        FIL_MSG_FILEAPPENDSEEK_FAILED_DSD,              /* 30920  */
        FIL_MSG_FILESEEK_FAILED_DSDD,                   /* 30921  */
        FIL_MSG_FILESEEK_FAILED_DISKFULL_DSDDD,         /* 30922  */
        FIL_MSG_FILESEEKEND_FAILED_DSD,                 /* 30923  */
        FIL_MSG_FILESEEKTONEWSIZE_FAILED_DSD,           /* 30924  */
        FIL_MSG_FILEEXPANDWRITE_FAILED_S,               /* 30925  */
        FIL_MSG_FILEEXPANDSEEK_FAILED_S,                /* 30926  */
        FIL_MSG_VIRTUALALLOC_FAILED_D,                  /* 30927  */
        FIL_MSG_FILEREADPAGE_FAILED_DSDDDD,             /* 30928  */
        FIL_MSG_FILEWRITEPAGE_FAILED_DSDDDD,            /* 30929  */

        TAB_MSG_BADCURSORSTATE_SD = 31000,              /* 31000  */
        TAB_MSG_FAKERANDOMTABLE_SS,                     /* 31001  */
        MSG_MSGEND
} su_msgret_t;

typedef struct {
        su_rc_subsys_t rcss_subsyscode;
        const char*    rcss_subsysname;
        bool           rcss_sorted;
        su_rc_text_t*  rcss_texts;
        uint           rcss_size;
} su_rc_subsysitem_t;

extern su_rc_subsysitem_t rc_subsys[SU_RC_NSUBSYS];

#endif /* SU0ERROR_H */
