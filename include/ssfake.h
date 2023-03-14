/*************************************************************************\
**  source       * ssfake.h
**  directory    * ss
**  description  * Fake definitions
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


#ifndef SSFAKE_H
#define SSFAKE_H

/*
 *  Macros to generate fake error cases.
 */
#if defined(SS_DEBUG) || defined(SS_COVER) || defined(SS_PURIFY) || defined(AUTOTEST_RUN)
#define SS_FAKE
#endif

#ifdef SS_FAKE

#include "ssrtcov.h"

typedef enum {
        FAKE_ERROR_BEGIN = 0,       /* Error number */

        /*
         *  SsFilXXX 
         */
        FAKE_SS_FOPENBFAILS             = 1,     
        FAKE_SS_FOPENTFAILS             = 2,     
        FAKE_SS_MODTIMEFAILS            = 3,     
        FAKE_SS_BOPENFAILS              = 4,     
        FAKE_SS_BREADFAILS              = 5,     
        FAKE_SS_BWRITEFAILS             = 6,     
        FAKE_SS_BAPPFAILS               = 7,     
        FAKE_SS_BAPPRESET               = 8,     
        FAKE_SS_FREEMEMCORRUPT          = 9,     
        FAKE_SS_SQLTRCASSERT            = 10,    
        FAKE_SS_TIME_JUMPS_FWD_120S     = 11,
        FAKE_SS_DONTCLEARMEMORY         = 12,
        FAKE_CHECK_1                    = 13,
        FAKE_SS_MEMALLOCCHECK           = 14,
        FAKE_SS_TIME_AT_WRAPAROUND      = 15,
        FAKE_SS_STOPONMUTEXENTER        = 16,
        FAKE_SS_SSTIMECHANGED           = 17,
        FAKE_SS_SSTIMECHANGEDCOUNTER    = 18,
        FAKE_SS_FFMEM2B_TESTRUN         = 19,
        FAKE_SS_NOASSERTEXIT            = 20,
        /*
         *  su0vfil
         */
        FAKE_SU_VFHINITFAILS            = 100, 
        FAKE_SU_VFHBEGINACCESS          = 101,   
        FAKE_SU_VFHBEGINACCPERS         = 102,  
        /*
         *  su0svfil
         */
        FAKE_SU_SVFADDFILE              = 103,
        FAKE_SU_SVFREADEOF              = 104,         
        FAKE_SU_SVFREADILLADDR          = 105,     
        FAKE_SU_SVFWRITEFAILS           = 106,      
        FAKE_SU_SVFWRITEDFULL           = 107,      
        FAKE_SU_STOPONRC                = 108, /* asserts when specific error is generated */
        FAKE_SU_EXPANDFAILATSWITCH      = 109, /* file expansion fails at change to new file */ 
        /*
         * su0li2* & su0lxc
         */
        FAKE_SU_LI2CRACKED              = 120,
        FAKE_SU_LI2DBAGETESTBYPASS      = 121,

        /* su0vmem */
        FAKE_SU_VMEMNOTRUNCATE          = 122,  
        FAKE_SU_VMEMMAXPHYSFILESIZESMALL = 123,

        /* nested mutexes */
        FAKE_SU_NM_LOCK_SHORT_SLEEP     = 124,  
        FAKE_SU_NM_UNLOCK_SHORT_SLEEP   = 125,  
        FAKE_SU_NM_LOCK_LONG_SLEEP      = 126,  
        FAKE_SU_NM_UNLOCK_LONG_SLEEP    = 127,  

        /* External sorter. */
        FAKE_XS_EOS                     = 130,

        /* Relational services (RES) */
        FAKE_RES_ASSERTPUTLONGFAILURE   = 150,
        FAKE_RES_ELBONIAN_COLLATION     = 151, /* when length == 1313 */

        /*
         *  session.c and sesuti.c
         */
        FAKE_SES_DKSREADFAILED          = 200,  
        FAKE_SES_DKSWRITEFAILED         = 201,
        FAKE_SES_DKSREADRESET           = 202, /* same as 200, but occurs only once */
        FAKE_SES_DKSWRITERESET          = 203, /* same as 201, but occurs only once */
        FAKE_SES_DKSTRACEDATA           = 204, /* use SsDbgPrintf for hex converted output */
        FAKE_SES_DLLASSERTBEFOREINIT    = 205, /* xxx_context_init asserts */
        FAKE_SES_DLLASSERTAFTERINIT     = 206, /* xxx_context_init asserts */
        FAKE_SES_DKSPRINTDATA           = 207, /* use SsDbgPrintf for printing the output */
        FAKE_SES_DKSHTTPGET             = 208, /* Test HTTP GET method */
        FAKE_SES_DKSWRITESLEEP          = 209, /* Additional sleep after write */
        FAKE_SES_DKSREADSLEEP           = 210, /* Additional sleep after read */
        FAKE_SES_SETINVALIDSOCKET       = 211, /* set invalid (-1) socked to select */
        FAKE_SES_SLOW_RESOLVER          = 212,
        FAKE_SES_MACHINEID_HOSTNAME     = 213, /* force different hostnames */
        
        /*
         * dbe
         */
        FAKE_DBE_FLSTALLOC_BLOB         = 300,  /* Free list alloc for blob fails */
        FAKE_DBE_BLOBSIZEBUGHUNT        = 301,  /* blob size bug hunt attempt
                                                 * in sse/test/tsrv2 */
        FAKE_DBE_IGNORELOGTIMES         = 302,  /* Ignore all CP & header timestamps of log file */
        FAKE_DBE_INCCOUNTERS            = 303,  /* Increment start record counters at startup by 5000. */
        FAKE_DBE_REJECTFREELIST         = 304,  /* Makes the dbe reject the freelist instead of loading it from disk */
        FAKE_DBE_STARTVALIDATEWAIT      = 305,  /* Wait trx in validate state before it is released by 306 */
        FAKE_DBE_STOPVALIDATE           = 306,  /* Stop validate wait of case 305 */
        FAKE_DBE_NOCOMMITREADLEVELWAIT  = 307,  /* Do not wait read level in commit */
        FAKE_DBE_RANDVALIDATEWAIT       = 308,  /* Randomly wait during trx validate */
        FAKE_DBE_CRASHAFTERCPMARK       = 309,  /* Stop server after cp log mark */
        FAKE_DBE_WRITECFGEXCEEDED       = 310,  /* fake fatal error code to SU_ERR_FILE_WRITE_CFG_EXCEEDED */
        FAKE_DBE_COUNTERWRAP_TOZERO     = 311,  /* set dbe counter values 100
                                                   less than max signed value. */
        FAKE_DBE_COUNTERWRAP_TONEGATIVE = 312,  /* set dbe counter values 100
                                                  less than max unsigned value. */
        FAKE_DBE_COUNTERWRAP_BIGINC     = 313,  /* Increments counter values
                                                   with big steps. */
        FAKE_DBE_BLOBDEBUGPOOL          = 314,  /* track BLOB IDs of alive BLOBs */
        FAKE_DBE_SEC_READMAINMEM        = 315,  /* Allow read from mainmem tables
                                                   in secondary. */
        FAKE_DBE_HSBREPBLOBFAILURE      = 316,  /* Simulate HSB and failure to
                                                   replicate BLOB */
        FAKE_DBE_SLOWBACKUP             = 317,  /* simulate very large backup */
        FAKE_DBE_SLOWCHECKPOINT         = 318,  /* simulate very slow checkpoint */
        FAKE_DBE_TRXL_CFGEXCEEDED       = 319,  /* cfg exceeded in trxlist alloc */
        FAKE_DBE_TRXB_CFGEXCEEDED       = 320,  /* cfg exceeded in trxlist alloc */
        FAKE_DBE_LOG_SLEEP_AFTER_COMMIT = 321,  /* sleep 5 sec after commit */
        FAKE_DBE_CATCHUP_SLEEP_AFTER_LOGDATA = 322, /* sleep 5 sec after fetching
                                                       next logdata from transaction log in catchup */
        FAKE_DBE_CHECKPOINT_SPLITLOG    = 323,  /* force log file split in checkpoint */
        FAKE_DBE_RECOVERY_CRASH_MMEINS  = 324,  /* crash on recovery after N MME-inserts */
        FAKE_DBE_RECOVERY_CRASH_MMEDEL  = 325,  /* crash on recovery after N MME-deletes */
        FAKE_DBE_RECOVERY_CRASH_COMMIT  = 326,  /* crash on recovery after N commits */
        FAKE_DBE_FREELISTDISKFULL       = 327,  /* Disk space alloc fails. */
        FAKE_DBE_SETDBREADONLY          = 328,  /* Set db read only. */
        FAKE_DBE_SLEEP_WHILE_DDOPACTIVE = 329,  /* Sleep while DD operation is active */
        FAKE_DBE_COMMIT_FAIL            = 330, 
        FAKE_DBE_LOGFILE_WRITE_FAIL     = 331,  /* next logfile write will fail */
        FAKE_DBE_SEQNEXTFAIL            = 332,
        FAKE_DBE_FASTIOMGR              = 333, /* simulate fast I/O */
        FAKE_DBE_INDSEA_RESETFETCHNEXT  = 334, /* reset search buffering after
                                                  each fetchnext */

        FAKE_DBE_PAUSEVALIDATE          = 335,
        FAKE_DBE_RESUMEVALIDATE         = 336,
        FAKE_DBE_NETCOPY_ERROR          = 337,
        FAKE_DBE_CPFLUSH_WAIT_20S       = 338,
        FAKE_DBE_SLEEP_BEFORE_LOCKING   = 339,
        FAKE_DBE_PAGERELOCATE_MOVE_SLOT = 340,
        FAKE_DBE_NONBLOCKINGFLUSHSLEEP  = 341,
        FAKE_DBE_PRI_NETCOPY_CRASH      = 342,
        FAKE_DBE_SLEEP_AFTER_STMTCOMMIT = 343,
        FAKE_DBE_RESET_CHGDIR_PREV      = 344,
        FAKE_DBE_REMOVEREADLEVEL_MERGELIMIT = 345,
        FAKE_DBE_REMOVEREADLEVEL_FLOWONLY   = 346,
        FAKE_DBE_SEQUENCE_RECOVERY_BUG      = 347,
        FAKE_DBE_DBE_FLUSHBATCH_LEAK        = 348,
        FAKE_DBE_MME_FLUSHBATCH_LEAK        = 349,
        FAKE_DBE_FLUSHBATCH_LEAK_IGNOREDEBUG = 350,

        /*
         *  sse
         */
        FAKE_SSE_DAXREADFAIL            = 400,  /* Data-at-exec read failure */
        FAKE_SSE_PASSWDLEAK             = 401,  
        FAKE_SSE_EXITPROGRAM            = 402,  /* Exit program with code 0. */
        FAKE_SSE_ASSERT                 = 403,  /* Assert server. */
        FAKE_HSB_PRI_ENABLECONNECT      = 404,   
        FAKE_HSB_PRI_DISABLECONNECT     = 405,   
        FAKE_SSE_CRPC_PREPARE           = 406,  /* Exit after prepare RPC write */
        FAKE_SSE_CRPC_EXECUTE           = 407,  /* Exit after execute RPC write */
        FAKE_SSE_DAXBUG                 = 408,  /* DAX bug hunt with cli\test\tblob.c */
        FAKE_HSB_SEC_TYPEREADFAIL       = 409,    
        FAKE_SSE_DISCONNECTBUG          = 410,    
        FAKE_HSB_PRI_LASTSYNCWRITEFAIL  = 411, 
        FAKE_HSB_SLEEP_ACMD_READY       = 412,
        __FAKE_NOTUSED_413              = 413,       
        __FAKE_NOTUSED_414              = 414,       
        FAKE_SSE_TRANS_TIMEOUT          = 415,       
        FAKE_SSE_CONN_TIMEOUT           = 416,        
    	FAKE_HSB_SEC_POWEROFF           = 417,		
        FAKE_SSE_STMTCOMMIT_WAIT        = 418,     
        FAKE_SSE_STMTCANCEL_ENABLE      = 419,   
        FAKE_SSE_STMTCANCEL_EXEC        = 420,     
        FAKE_SSE_AFTERPREPARERPCWRITESLEEP      = 421,
        FAKE_SSE_AFTERSTMTEXECEVENTWAIT         = 422,
        FAKE_SSE_AFTERSTMTEXECEVENTPOST         = 423,
        FAKE_SSE_TOOMANYUSERS                   = 424,          
        FAKE_SSE_STMT_TIMEOUT                   = 425,          
        FAKE_SSE_SKIPSTMTCOMMIT                 = 426,        
        FAKE_SSE_EXTRAMEMALLOCMB                = 427,        
        FAKE_SSE_RBAKMMECLOSEWRITER             = 428,
        FAKE_SSE_RBAKMMECLOSEREADER             = 429,
        FAKE_SSE_REFUSENETCOPY                  = 430,
        FAKE_SSE_LONGUNLINK                     = 431,

        FAKE_HSBG2_SLEEP_IN_RESET               = 440,
        
        FAKE_HSB_PRI_CRASH_BEFORECOMWRITE       = 450,
        FAKE_HSB_SEC_CRASH_BEFORECOMREAD        = 451,    
        FAKE_HSB_SEC_CRASH_BEFORECOMWRITE       = 452,    
        FAKE_HSB_PRI_CRASH_BEFORECOMREAD        = 453,    
        FAKE_HSB_PRI_LOGTRANSBLOCKSWITCHERROR   = 454,
        FAKE_HSB_WAIT_BEFORE_DELETE             = 455,    
        FAKE_HSB_SEC_CRASH_BEFORECOMMIT         = 456,    

        FAKE_HSB_SEC_CRASH_BEFOREPRICOMREAD     = 457,    
        FAKE_HSB_PRI_BREAKCONN_BEFOREHSBLOGFLUSH= 458,
        __FAKE_NOTUSED_459                      = 459,
        FAKE_HSB_PRI_CRASH_RPCSESREQWRITEEND    = 460,
        FAKE_HSB_SEC_MAKECPAFTERSQL             = 461,    
        FAKE_HSB_PRI_DUMMYCOMMIT                = 462,    
        FAKE_HSB_PRI_OPLISTNOFLUSH              = 463,    
        FAKE_HSB_WAITAFTERCOMMIT                = 464,    
        FAKE_HSB_PRI_SETREADONLY_BEFORECOMREAD  = 465,
        FAKE_HSB_PINGTIMEOUT                    = 466,
        
        FAKE_HSB_SEC_MAKECPAFTEROPSCAN          = 467,  
        FAKE_HSB_ENABLE_ASSERTONCONNECTFAILED   = 468,  
        FAKE_HSB_DISABLE_ASSERTONCONNECTFAILED  = 469, 
        FAKE_HSB_SLEEP_BEFORE_CONNECT_BACK      = 470,  
        FAKE_HSB_SLEEP_BEFORE_DISCONNECT        = 471,  
        FAKE_HSB_RPC_FAIL_IN_DISCONNECT         = 472,  
        FAKE_HSB_RPC_FAIL_IN_CONNECT            = 473,  
        FAKE_HSB_RPC_FAIL_IN_CATCHUP            = 474,  
        FAKE_HSB_SLEEP_AFTER_COPY               = 475,  
        FAKE_HSB_CATCHUP_NEVER_FROM_MEMORY      = 476,  
        FAKE_HSB_CATCHUP_SOME_MORE              = 477,  
        FAKE_HSB_CATCHUP_FROM_DISK              = 478,  
        
        FAKE_HSBG2_CATCHUP_RPC_FAIL_BEFORE_POS  = 479,
        FAKE_HSBG2_CATCHUP_RPC_FAIL_AFTER_POS   = 480,
        FAKE_HSBG2_CATCHUP_RPC_FAIL_DISK_OP     = 481,
        FAKE_HSBG2_CATCHUP_RPC_FAIL_MEM_OP      = 482,
        FAKE_HSBG2_CATCHUP_RPC_FAIL_FLUSH       = 483,
        FAKE_HSBG2_CATCHUP_RPC_FAIL_BEFORE_READY= 484,
        FAKE_HSBG2_CATCHUP_RPC_FAIL_AFTER_READY = 485,

        FAKE_HSBG2_SLEEP_IN_COPY                = 486,    
        FAKE_HSBG2_SLEEP_IN_SWITCH              = 487,    
        FAKE_HSBG2_SLEEP_IN_CONNECT             = 488,    
        FAKE_HSBG2_SLEEP_IN_DISCONNECT          = 489,    
        FAKE_HSBG2_SLEEP_IN_CATCHUP             = 490,    
        FAKE_HSBG2_CATCHUP_HSBNOTSYNC           = 491,    
        FAKE_HSBG2_RPC_FAIL_AFTER_READ_ACK_NEW_PRIMARY = 492,
        FAKE_HSBG2_CATCHUP_PRIMARY_SLEEP        = 493,
        FAKE_HSBG2_CATCHUP_SECONDARY_SLEEP      = 494,
        FAKE_HSBG2_SWITCH_LOGREC_SLEEP          = 495,
        FAKE_HSBG2_STATE_SLEEP                  = 496,
        FAKE_HSBG2_SECEXEC_COMMIT_SLEEP         = 497,
        FAKE_HSB_PRI_SHUTDOWN_AFTERCOMWRITE     = 498,
        FAKE_HSB_SLOW_SECONDARY_EXECUTOR        = 499,

        FAKE_SNC_MSGWRITEFAIL                   = 500,
        FAKE_SNC_MSGREADFAIL                    = 501,
        FAKE_SNC_MASTERMSGRPCREADFAIL           = 502,
        FAKE_SNC_MASTERMSGRPCWRITEFAIL          = 503,
        FAKE_SNC_MASTERREPLYRPCREADFAIL         = 504,
        FAKE_SNC_MASTERREPLYPCWRITEFAIL         = 505,
        FAKE_SNC_MASTEROKRPCREADFAIL            = 506,
        FAKE_SNC_MASTEROKRPCWRITEFAIL           = 507,
        FAKE_SNC_REPLGETRPCWRITEFAIL            = 508,
        FAKE_SNC_REPLGETRPCREADFAIL             = 509,
        FAKE_SNC_REPLOKRPCWRITEFAIL             = 510,
        FAKE_SNC_REPLOKRPCREADFAIL              = 511,
        FAKE_SNC_REPLFWDRPCWRITEFAIL            = 512,
        FAKE_SNC_REPLFWDRPCREADFAIL             = 513,
        FAKE_SNC_MASTERSUBSCCOMMITFAIL          = 514,
        FAKE_SNC_MASTERDIRECTSAVEREAD           = 515,

        FAKE_SNC_MASTERREPLY_TIMEOUT            = 516,
        FAKE_SNC_MASTERCREATEDUMMYERRHWHENNULL  = 517,

        FAKE_SNC_DIRECTRPC_MSGREADFAIL          = 518,
        FAKE_SNC_DIRECTRPC_MSGWRITEFAIL         = 519,

        FAKE_SNC_MASTERPROPAGARECRASHBEFORECOMMIT = 530,
        FAKE_SNC_MASTERPROPAGARECRASHAFTERCOMMIT  = 531,
        FAKE_SNC_CORRUPTED_MESSAGE                = 532,

        FAKE_SNC_MASTER_FORWARD_REPLY_DELAY     = 533,
        FAKE_SNC_MASTER_PAUSE                   = 534,
        FAKE_SNC_MASTER_FORWARD_REPLYEND_DELAY  = 535,
        FAKE_SNC_HSB_MASTER_EXPORT_ASSERT       = 536,

        FAKE_RPC_THREADFAILURE                  = 580,
        FAKE_RPC_SLEEPAFTERWRITEEND             = 581,
        FAKE_RPC_SLEEPAFTER_5SEC                = 582,
        
        FAKE_RPC_CLI_CORRUPTBYTE_POS            = 583, /* from RPC begin */
        FAKE_RPC_CLI_CORRUPTBYTE_TYPE           = 584, /* -1=skip,0=corrupt */
        FAKE_RPC_CLI_CORRUPTBYTE_VAL            = 585, /* when _TYPE=0 */

        
        FAKE_TAB_MIGRATETOUNICODEFAIL           = 600,
        FAKE_TAB_USEHURC                        = 601,
        FAKE_TAB_YIELDWHENBLOBG2REFLOCKED       = 602,
        FAKE_TAB_YIELDINBLOBG2DONE              = 603,
        FAKE_TAB_MMETABLESONLY                  = 604,
        FAKE_TAB_TOGGLETABLETYPES               = 605,
        FAKE_SP_SAC_NLOCAL                      = 606,
        FAKE_SP_SAC_NREMOTE                     = 607,
        FAKE_SP_SAC_NSTMT                       = 608,
        FAKE_TAB_RESET_MUST_BE_FULL             = 609,
        FAKE_TAB_SLEEP_AFTER_FETCH              = 610,
        FAKE_TAB_REPRODUCE_BLOBG2REFCOUNTBUG    = 611,
        FAKE_SP_SLEEPANDOAUSEAFTERPUSH          = 612,
        FAKE_TAB_FORCEDTABLES                   = 613,
        FAKE_SP_PRINTSQLSTRINGTOPURIFYLOG       = 666,       

        FAKE_EST_ARITHERROR                     = 700,

        FAKE_COM_CLIENT_LONGDELAYDURINGRPCWRITE = 800, /* used in ssa1stmr.c! */

        FAKE_CHECK_2                            = 849,

        FAKE_REX_CONNECTREAD_FAIL               = 850, /* RPC connect read fails */
        FAKE_REX_EXECWRITE_FAIL                 = 851, /* RPC Execution write fails */
        FAKE_REX_EXECREAD_FAIL                  = 852, /* Execution read fails */
        FAKE_REX_DISCONNECTWRITE_FAIL           = 853, /* Disconnection write fails */
        FAKE_REX_DISCONNECTREAD_FAIL            = 854, /* Disconnection read fails */
        FAKE_REX_READREPLY_FAIL1                = 855, /* Read reply fail */
        FAKE_REX_READREPLY_FAIL2                = 856, /* Read reply fail */
        FAKE_REX_POOLSIZECHECK                  = 857, /* Poolsize check */
        FAKE_REX_LIFETIMECHECK                  = 858, /*  Connection life time check */
        FAKE_CHECK_3                            = 859,              

        FAKE_SRV_USEDEADLOCKDETECTION           = 870,
        FAKE_SRV_DELAYTASKWAKEUP_HSBREPLYREAD   = 871,
        FAKE_SRV_DELAYTASKWAKEUP                = 872,
        FAKE_SRV_DELAYTASKEVENTSIGNAL           = 873,

        FAKE_HSBG2_SNC_MASTER_SLEEP_READ_MSG    = 900,
        FAKE_HSBG2_SNC_MASTER_CRASH_READ_MSG    = 901,

        FAKE_HSBG2_SNC_MASTER_SLEEP_EXEC_MSG    = 902,
        FAKE_HSBG2_SNC_MASTER_CRASH_EXEC_MSG    = 903,

        FAKE_HSBG2_SNC_MASTER_SLEEP_SUBSCRIBE   = 904,
        FAKE_HSBG2_SNC_MASTER_CRASH_SUBSCRIBE   = 905,

        FAKE_HSBG2_SNC_MASTER_SLEEP_GET_REPLY   = 906,
        FAKE_HSBG2_SNC_MASTER_CRASH_GET_REPLY   = 907,

        FAKE_HSBG2_SNC_MASTER_SLEEP_WRITE_REPLY = 908,
        FAKE_HSBG2_SNC_MASTER_CRASH_WRITE_REPLY = 909,

        FAKE_HSBG2_CATCHUP_DISK_PRIMARY_YIELD   = 910,

        FAKE_HSBG2_DBOPFAIL_D                   = 911,
        FAKE_HSBG2_STMTCOMMITFAIL_D             = 912,
        FAKE_HSBG2_TRXSETFAILED_ATTRXSEMENTER_D = 913,
        FAKE_HSBG2_LOGGING_SRV_ERR_HSBCONNBROKEN= 914,
        FAKE_HSBG2_CATCHUP_HSBCONNBROKEN        = 915,
        FAKE_HSBG2_PRI_CRASH_ON_RECEIVED_ACK    = 916,
        FAKE_HSBG2_SWITCH_PRIMARY_BROKEN_BEFORE_NOTIFY = 917,
        FAKE_HSBG2_SPLIT_QUEUE                  = 918,

        FAKE_SHRINK_DURING_MERGE                = 919,
        FAKE_SHRINK_SHRINKING                   = 920,
        FAKE_SHRINK_BEFORE_FINAL_CP             = 921,

        FAKE_HSBG2_SLEEP_AFTER_SM_RESET         = 922,
        FAKE_HSB_PAUSE_SECONDARY_EXECUTOR       = 923,
        FAKE_HSB_SLOW_SECONDARY_LOGGING         = 924,
        FAKE_HSB_PAUSE_SECONDARY_LOGGING        = 925,

        FAKE_HSBG2_PRI_CRASH_ON_EXECUTED_ACK    = 926,
        FAKE_HSBG2_PRI_CRASH_ON_DURABLE_ACK     = 927,
        FAKE_HSBG2_PRI_CRASH_ON_GIVEN_DURABLE_ACK = 928,
        FAKE_HSBG2_PRI_CP_DELETE_LOG_FILES      = 929,
        FAKE_HSB_PRI_CRASH_BEFORECOMMITPHASE2   = 930,

        FAKE_TF1_SLOW_SWITCH                    = 931,
        FAKE_TF1_DELAY_SECONDARY_CONNECTION     = 932,
        FAKE_TF1_SLOW_STATE_CLEANUP             = 933,
        FAKE_TF1_BROKEN_THREAD                  = 934,
        FAKE_TF1_SLOW_DETECT                    = 935,
        FAKE_TF1_SLOW_MSG_SEND                  = 936,
        FAKE_TF1_SLOW_MSG_RECEIVE               = 937,
        FAKE_TF1_SLOW_THR_CHK                   = 938,
        FAKE_TF1_SLOW_COMMIT                    = 939,

        FAKE_MME_SLOW_CHECKPOINT                = 1000,
        FAKE_MME_HALT_CHECKPOINT                = 1001,

        FAKE_SLEEP10_BEFORE_COMMITREPLY         = 1002, /* what is this doing here? */

        FAKE_MME_SLOW_INDEX_CREATION            = 1003,
        FAKE_MME_SLOW_RVAL_FETCHING             = 1004,
        FAKE_MME_SLOW_BNODE_SEARCH              = 1005,

        FAKE_MME_SLOW_SEARCH_BASE               = 1050,

        /* FAKES 1050 - 1089 are reserved. */
        
        FAKE_MME_FILL_BNODE_BASE                = 1090,

        /* FAKES 1090 - 1099 are reserved. */
        
        /* Don't add anything but MME fakes here.  Next non-MME fake
           is 1200. */

        FAKE_ERROR_END                          = 1200 /* FAKE error table end */
} fake_t;

/*
 * Fake error cases start to happen when counter value reaches one (1).
 * Fake case is turned off if counter is set to zero (0).
 */

/*
 * This macro can be used to turn on the fake cases in 
 * test programs. Fake cases can also be set in SS_DEBUG environment
 * variable (see ssdebug.c for more details).
 */
#define SET_FAKE(type,val) { fake_cases[type] = val; }

/*
 * These macros can be used in functions where you want fake cases
 * to occur.
 */
#define FAKE_RETURN(type,rc) { if (fake_cases[type] > 1) {\
                                    fake_cases[type]--;\
                             } else if (fake_cases[type] == 1) {\
                                    SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
                                    return rc;\
                             }}

#define FAKE_RET_RESET(type,rc) { if (fake_cases[type] > 1) {  \
                                    fake_cases[type]--; \
                                } else if (fake_cases[type] == 1) {\
                                    fake_cases[type] = 0;\
                                    SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
                                    return rc;\
                                }}

#define FAKE_CODE_BLOCK(type,code)    { if (fake_cases[type] > 1) {\
                                           fake_cases[type]--;\
                                      } else if (fake_cases[type] == 1) {\
                                           code \
                                           SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
                                      }}

#define FAKE_CODE_RESET(type,code)     { if (fake_cases[type] > 1) {  \
                                            fake_cases[type]--; \
                                        } else if (fake_cases[type] == 1) {\
                                            fake_cases[type] = 0;\
                                            code \
                                            SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
                                       }}

#define FAKE_CODE_SLEEP(type,code)    { if (fake_cases[type] > 0) {\
                                            code \
                                            while (fake_cases[type] > 0) {\
                                                SsThrSleep(200);\
                                            }\
                                            SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
                                        }}

#define FAKE_CODE_BLOCK_EQ(type, count, code) \
        { if (fake_cases[type] == count) {\
            code \
            SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
        }}

#define FAKE_CODE_BLOCK_GT(type, count, code) \
        { if (fake_cases[type] != 0 && fake_cases[type] > count) {\
            code \
            SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
        }}

#define FAKE_CODE_BLOCK_LT(type, count, code) \
        { if (fake_cases[type] != 0 && fake_cases[type] < count) {\
            code \
            SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
        }}

#define FAKE_EXECUTE_N_TIMES(type,code) { if (fake_cases[type] > 0) {\
                                           fake_cases[type]--;\
                                           code \
                                           SS_RTCOVERAGE_SET(SS_RTCOV_LAST_FAKE_CODE, type);\
                                        }}

#define FAKE_IF(type) \
        if (fake_cases[type] != 0 && (fake_cases[type] > 1 ? \
            (fake_cases[type]--,FALSE):TRUE))

#define FAKE_IF_RESET(type) \
        if (fake_cases[type] != 0 && (fake_cases[type] > 1 ? \
            (fake_cases[type]--,FALSE):((fake_cases[type] = 0),TRUE)))

#define FAKE_IF_TRUE() if (TRUE)

#define FAKE_CODE(p) p

#define FAKE_SET(type, val) {fake_cases[type] = (val);}

#define FAKE_GET(type, val) {val = fake_cases[type];}
#define FAKE_GETVAL(type) fake_cases[type]

#define FAKE_INFO_ASSERT(type, val) \
{\
        if (fake_cases[type] > 0) { \
            ss_info_dassert(fake_cases[type] == val,\
             ("type=%d fake_cases[type]=%d <> val=%d", type, fake_cases[type], val));\
        }\
}

#define FAKE_PAUSE(type) ss_fake_pause(type)


extern uint fake_cases[FAKE_ERROR_END];

/*#define SS_BLOBBUG removed by pete 1999-03-22 */
#ifdef SS_BLOBBUG
extern uint* fake_blobmon[3];
extern uint fake_blobmon_idx[2];
extern char* fake_prev_blobbuf;
extern uint fake_prev_blobbufsize;
extern ulong fake_prev_blobdaddr;
extern ulong fake_blobdaddr;
void fake_blobmon_output(void);

#define FAKE_BLOBMON_ADD(n, size) \
{\
        ss_dassert((unsigned)(n) <= 1);\
        ss_dassert(fake_blobmon_idx[n] < 1023);\
        if (fake_blobmon[n] == NULL) {\
            fake_blobmon[n] = SsMemAlloc(1023 * sizeof(uint));\
        }\
        fake_blobmon[n][fake_blobmon_idx[n]++] = (size);\
}
#define FAKE_BLOBMON_ADD2(size) \
{\
        ss_dassert(fake_blobmon_idx[1] < 1023);\
        if (fake_blobmon[2] == NULL) {\
            fake_blobmon[2] = SsMemCalloc(1023,sizeof(uint));\
        }\
        fake_blobmon[2][fake_blobmon_idx[1]-1] = (size);\
}
#define FAKE_REGISTER_BLOBBUF(buf, bufsize) \
        fake_prev_blobbuf = (buf); fake_prev_blobbufsize = bufsize;
#define FAKE_REGISTER_BLOBDADDR(daddr) \
        fake_prev_blobdaddr = fake_blobdaddr; fake_blobdaddr = (daddr);
#define FAKE_CHECK_BLOBBUF(buf, bufsize) \
        if (fake_prev_blobdaddr == fake_blobdaddr) {\
            if (fake_prev_blobbuf + fake_prev_blobbufsize != (char*)(buf)) {\
                ss_pprintf_1((\
"fake_prev_blobbuf = 0x%08lX, fake_prev_blobbufsize = %d, buf == 0x%08lX",\
                    fake_prev_blobbuf, fake_prev_blobbufsize, (buf)));\
                ss_error;\
            }\
        } FAKE_REGISTER_BLOBBUF(buf, bufsize);


#define FAKE_BLOBMON_OUTPUT() \
        fake_blobmon_output()

#define FAKE_BLOBMON_RESET() \
{\
        if (fake_blobmon[0] != NULL) {\
            SsMemFree(fake_blobmon[0]);\
            fake_blobmon[0] = NULL;\
        }\
        if (fake_blobmon[1] != NULL) {\
            SsMemFree(fake_blobmon[1]);\
            fake_blobmon[1] = NULL;\
        }\
        if (fake_blobmon[2] != NULL) {\
            SsMemFree(fake_blobmon[2]);\
            fake_blobmon[2] = NULL;\
        }\
        fake_blobmon_idx[1] = fake_blobmon_idx[0] = 0;\
}

#endif /* SS_BLOBBUG */

uint ss_fake_cmd(
        void* cd, 
        char** argv, 
        char* err_text, 
        size_t err_text_size);

bool ss_fake_pause(int fakenum);

#else /* SS_FAKE */

#define FAKE_PAUSE(type)
#define FAKE_RETURN(type,rc)
#define FAKE_RET_RESET(type,rc)
#define FAKE_CODE_BLOCK(type,code)
#define FAKE_CODE_RESET(type,code)
#define FAKE_CODE_SLEEP(type,code)
#define SET_FAKE(err_no,val)
#define FAKE_CODE(p) 
#define FAKE_IF(type) if (FALSE)
#define FAKE_IF_RESET(type) if (FALSE)
#define FAKE_IF_TRUE() if (FALSE)
#define FAKE_CODE_BLOCK_EQ(type, count, code)  
#define FAKE_CODE_BLOCK_GT(type, count, code)  
#define FAKE_CODE_BLOCK_LT(type, count, code)  
#define FAKE_EXECUTE_N_TIMES(type,code)
#define FAKE_SET(type, val)
#define FAKE_GET(type, val)
#define FAKE_GETVAL(type)
#define FAKE_INFO_ASSERT(type, val)


#endif /* SS_FAKE */

#endif /* SSFAKE_H */
