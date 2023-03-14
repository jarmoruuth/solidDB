/*************************************************************************\
**  source       * su0cfgst.h
**  directory    * su
**  description  * Configuration string constants for SOLID dbms
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


#ifndef SU0CFGST_H
#define SU0CFGST_H

#include <ssenv.h>

extern char su_inifile_filename[];

/* Solid configuration file name. */
#define SU_SOLINI_FILENAME  su_inifile_filename

/* Solid Server output file name */
#define SU_SOLID_MSGFILENAME  "solmsg.out"

#if defined(SS_NT)
/* ODBC configuration registry name, WNT and W95 */
#define SU_ODBC_REGISTRYNAME  "software\\odbc\\odbc.ini"
#elif defined(SS_WIN)
#define SU_ODBC_REGISTRYNAME  "odbc.ini"
#else
#define SU_ODBC_REGISTRYNAME  ""
#endif

/* Environment variable that defines Solid default directory  */
#define SU_SOLIDDIR_ENVVAR  "SOLIDDIR"

/* Env variables that define trace trace-option and tracefile */
#define SU_SOLTRACE_ENVIRONVAR        "SOLTRACE"
#define SU_SOLTRACEFILE_ENVIRONVAR    "SOLTRACEFILE"

#define SU_DEFAULT_TRACEFILENAME      "soltrace.out"
#define SU_DEFAULT_TRACEFILELEN       1000000L

#define SU_DBE_BACKUPSECTION    "Backup"
#define     SU_DBE_LOGFILETEMPLATE_FOR_BACKUP "LogFileNameTemplate"
#define     SU_DBE_SOLIDDIR "SolidDir"
#define SU_DBE_LOGDIR "LogDir"

#define SU_DBE_GENERALSECTION   "General"
/* FileWriteFlushMode, int 0 = None, 1 = before read, 2 = after write  */
#define     SU_DBE_WRITEFLUSHMODE   "FileWriteFlushMode"
#define     SU_DBE_MAXOPENFILES     "MaxOpenFiles"              /* int */
#define     SU_DBE_BACKUPDIR        "BackupDirectory"           /* string */
#define     SU_DBE_BACKUP_BLOCKSIZE "BackupBlockSize"           /* int */
#define     SU_DBE_BACKUP_STEPSTOSKIP     "BackupStepsToSkip"         /* int */
#define     SU_DBE_BACKUPCOPYLOG    "BackupCopyLog"             /* yes/no */
#define     SU_DBE_BACKUPDELETELOG  "BackupDeleteLog"           /* yes/no */
#define     SU_DBE_BACKUPCOPYINIFLE "BackupCopyIniFile"         /* yes/no */
#define     SU_DBE_BACKUPCOPYSOLMSGOUT "BackupCopySolmsgOut"    /* yes/no */

#define     SU_DBE_NETBACKUPCONNECT         "NetBackupConnect"
#define     SU_DBE_NETBACKUPCOPYINIFLE      "NetBackupCopyInifile"
#define     SU_DBE_NETBACKUPCOPYLOG         "NetBackupCopyLog"
#define     SU_DBE_NETBACKUPCOPYSOLMSGOUT   "NetBackupCopySolmsgout"
#define     SU_DBE_NETBACKUPDELETELOG       "NetbackupDeleteLog"
#define     SU_DBE_NETBACKUPDIR             "NetBackupDirectory"
#define     SU_DBE_NETBACKUPCONNECTTIMEOUT  "NetBackupConnectTimeout"
#define     SU_DBE_NETBACKUPREADTIMEOUT     "NetBackupReadTimeout"

#define     SU_DBE_CPINTERVAL       "CheckpointInterval"        /* int */
#define     SU_DBE_CPMINTIME        "MinCheckpointTime"         /* int, secs */
#define     SU_DBE_CPDELETELOG      "CheckpointDeleteLog"       /* yes/no */
#define     SU_DBE_MERGEINTERVAL    "MergeInterval"             /* int */
#define     SU_DBE_QUICKMERGEINTERVAL "QuickMergeInterval"      /* int */
#define     SU_DBE_MERGEMINTIME     "MinMergeTime"              /* int, secs */
#define     SU_DBE_STARTUPFORCEMERGE "StartupForceMerge"        /* yes/no */
#define     SU_DBE_MAXMERGETASKS    "MaxMergeTasks"             /* int */
#define     SU_DBE_USERMERGE        "UserMerge"                 /* yes/no */
#define     SU_DBE_MAXMERGEPARTS    "MaxMergeParts"             /* int */
#define     SU_DBE_MAXUSERMERGESTEPS "MaxUserMergeSteps"        /* int */
#define     SU_DBE_SPLITPURGE       "SplitPurge"                /* yes/no */
#define     SU_DBE_MERGECLEANUP     "MergeCleanup"              /* yes/no */
#define     SU_DBE_USENEWBTREELOCKING     "UseNewBtreeLocking"  /* yes/no */
#define     SU_DBE_USERANDOMKEYSAMPLEREAD "UseRandomKeySampleRead" /* yes/no */
#define     SU_DBE_SINGLEDELETEMARK "SingleDeletemark"          /* yes/no */
#define     SU_DBE_SEQSEALIMIT      "LongSequentialSearchLimit" /* int */
#define     SU_DBE_SEABUFLIMIT      "SearchBufferLimit"         /* int, percentage */
#define     SU_DBE_TRXBUFSIZE       "TransactionHashSize"       /* int */
#define     SU_DBE_TRXEARLYVALIDATE "TransactionEarlyValidate"  /* yes/no */
#define     SU_DBE_USEIOTHREADS     "UseIOThreadsOnlyDeprecated"/* yes/no */
#define     SU_DBE_PESSIMISTIC      "Pessimistic"               /* yes/no */
#define     SU_DBE_USEPESSIMISTICGATE "UsePessimisticGate"      /* yes/no */
#define     SU_DBE_PESSIMISTIC_LOCK_TO "LockWaitTimeOut"           /* int, secs */
#define     SU_DBE_OPTIMISTIC_LOCK_TO  "OptimisticLockWaitTimeOut" /* int, secs */
#define     SU_DBE_TABLE_LOCK_TO    "TableLockWaitTimeOut"         /* int, secs */
#define     SU_DBE_LOCKHASHSIZE     "LockHashSize"              /* int */
#define     SU_DBE_FASTDEADLOCKDETECT "FastDeadlockDetect"      /* int */
#define     SU_DBE_DEADLOCKDETECTMAXDEPTH "DeadlockDetectMaxDepth" /* int */
#define     SU_DBE_DEADLOCKDETECTMAXLOCKS "DeadlockDetectMaxLocks" /* int */
#define     SU_DBE_RELBUFSIZE       "TableBufferSize"           /* int */
#define     SU_DBE_READONLY         "ReadOnly"                  /* yes/no */
#define     SU_DBE_DISABLEIDLEMERGE "DisableIdleMerge"          /* yes/no */
#define     SU_DBE_COLLATION_SEQ    "CollationSeq"              /* ISO 8859-1/FIN */
#define     SU_DBE_CHECKESCALATELIMIT "CheckEscalateLimit"      /* int */
#define     SU_DBE_READESCALATELIMIT  "ReadEscalateLimit"       /* int */
#define     SU_DBE_LOCKESCALATELIMIT  "LockEscalateLimit"       /* int */
#define     SU_DBE_ALLOWLOCKBOUNCE    "AllowLockBounce"         /* yes/no */
#define     SU_DBE_SPLITMERGE         "SplitMerge"              /* yes/no */
#define     SU_DBE_DEFAULTSTOREISMEMORY "DefaultStoreIsMemory"  /* yes/no */
#define     SU_DBE_NUMIOTHREADS         "IOThreads"             /* int */
#define     SU_DBE_NUMWRITERIOTHREADS   "WriterIOThreads"       /* int */
#define     SU_DBE_DEFAULTISTRANSIENT   "DefaultIsTransient"    /* yes/no */
#define     SU_DBE_DEFAULTISGLOBALTEMPORARY "DefaultIsGlobalTemporary" /* yes/no */
#define     SU_DBE_USENEWTRANSWAITREADLEVEL "UseNewTransWaitReadLevel" /* yes/no */
#define     SU_DBE_USERELAXEDREADLEVEL      "UseRelaxedReadLevel"      /* yes/no */
#define     SU_DBE_VERSIONEDPESSIMISTICREADCOMMITTED "VersionedPessimisticReadCommitted"   /* yes/no */
#define     SU_DBE_VERSIONEDPESSIMISTICREPEATABLEREAD "VersionedPessimisticRepeatableRead"   /* yes/no */
#define     SU_DBE_READLEVELMAXTIME          "ReadLevelMaxTime"         /* int, secs */
#define     SU_DBE_DDOPERRORMAXWAIT          "DataDictionaryErrorMaxWait" /* int, secs */
#define     SU_DBE_PHYSICALDROPTABLE         "PhysicalDropTable"
#define     SU_DBE_RELAXEDBTRTEELOCKING      "RelaxedBtreeLocking"      /* bits: 1=nodepath, 2=nolock, 3=both */
#define     SU_DBE_SPINCOUNT                 "SpinCount"

#define SU_DBE_INDEXSECTION     "IndexFile"
#define SU_DBE_BLOBSECTION      "BLOBFile_%u"
#define SU_DBE_LOGSECTION       "Logging"
#define     SU_DBE_FILESPEC         "FileSpec_%u"
#define     SU_DBE_BLOCKSIZE        "BlockSize"             /* int, bytes */
#define     SU_DBE_CACHESIZE        "CacheSize"             /* int, bytes */
#define     SU_DBE_MAXPAGESEM       "MaxPageSemaphores"     /* int, count */
#define     SU_DBE_EXTENDINCR       "ExtendIncrement"       /* blocks */
#define     SU_DBE_MAXSEQALLOC      "MaxSequentialAllocation" /* blocks */
#define     SU_DBE_LOGFILETEMPLATE  "FileNameTemplate"
#define     SU_DBE_LOGDIR           "LogDir"
#define     SU_DBE_LOGDIGITTEMPLATE "DigitTemplateChar"
#define     SU_DBE_LOGMINSPLITSIZE  "MinSplitSize"          /* int, bytes */
#define     SU_DBE_LOGCOMMITMAXWAIT "CommitMaxWait"         /* int, milliseconds */
#define     SU_DBE_LOGWRITEMODE     "LogWriteMode"          /* int, see dbe6log.c */
#define     SU_DBE_LOGENABLED       "LogEnabled"            /* yes/no */
#define     SU_DBE_READAHEADSIZE    "ReadAhead"             /* int, blocks */
#define     SU_DBE_PREFLUSHPERCENT  "PreFlushPercent"       /* int, % 0 .. 90 */


#define     SU_DBE_LASTUSELRUSKIPPERCENT "LowPriorityLRUSkipPercent"
                                         /* int % 0 .. 100 */

#define     SU_DBE_PREFLUSHSAMPLESIZE   "PreFlushSampleSize"   /* int, blocks */
#define     SU_DBE_PREFLUSHDIRTYPERCENT "PreFlushDirtyPercent" /* int, % 0 .. 90 */
#define     SU_DBE_CLEANPAGESEARCHLIMIT "CleanPageSearchLimit" /* int, default: 100 */
#define     SU_DBE_USEBTREEGATE       "UseBtreeGate"           /* bool */

#ifdef FSYNC_OPT
# define    SU_DBE_FFILEFLUSH            "ForceFileFlush"        /* logfile y/n */
#endif

#define     SU_DBE_FILEFLUSH            "FileFlush"            /* logfile y/n */

#define     SU_DBE_SYNCWRITE            "SyncWrite"            /* logfile y/n */

#ifdef IO_OPT
#define     SU_DBE_DIRECTIO             "DirectIO"              /* log&indexfile y/n */
#endif /* IO_OPT */

#define     SU_DBE_SYNCHRONIZEDWRITE    "SynchronizedWrite"    /* indexfile y/n */

#define     SU_DBE_DURABILITYLEVEL      "DurabilityLevel"      /* 1=relaxed, 2=adaptive, 3=strict */
#define     SU_DBE_USENEWKEYCHECK       "UseNewKeyCheck"        /* y/n */
#define     SU_DBE_USESHORTKEYOPT       "UseShortKeyOpt"        /* y/n */
#define     SU_DBE_LOGUSEGROUPCOMMITQUEUE "UseGroupCommitQueue" /* y/n */
#define     SU_DBE_FREELISTRESERVERSIZE "FreeListReserveSize"  /* int, blocks */
#define     SU_DBE_FREELISTGLOBALLYSORTED "FreeListGloballySorted" /* bool */
#define     SU_DBE_LOGRELAXEDMAXDELAY   "RelaxedMaxDelay"       /* int, milliseconds */
#define     SU_DBE_MAXWRITEQUEUERECORDS "MaxWriteQueueRecords"  /* int, records */
#define     SU_DBE_MAXWRITEQUEUEBYTES   "MaxWriteQueueBytes"    /* int, bytes */
#define     SU_DBE_WRITEQUEUEFLUSHLIMIT "WriteQueueFlushLimit"  /* int, bytes */
#define     SU_DBE_LOGWRITEBUFFERSIZE   "WriteBufferSize"       /* int, bytes */
#define     SU_DBE_DELAYMESWAIT         "DelayMesWait"          /* bool */

#define SU_MME_SECTION          "MME"
#define     SU_MME_LOCKHASHSIZE         "LockHashSize"           /* int */
#define     SU_MME_LOCKESCALATION       "LockEscalationEnabled" /* y/n */
#define     SU_MME_LOCKESCALATIONLIMIT  "LockEscalationLimit"   /* int */
#define     SU_MME_LOWWATERMARKPERC     "ImdbMemoryLowPercentage" /* int,%60-99 */
#define     SU_MME_MEMORYLIMIT          "ImdbMemoryLimit"   /* ss_int8_t, bytes */
#define     SU_MME_RELEASEMEMORY        "ReleaseMemoryAtShutdown" /* y/n */
#define     SU_MME_MAXCACHEUSAGE        "MaxCacheUsage"     /* int, bytes */
#define     SU_MME_MUTEXGRANULARITY     "MutexGranularity"      /* int */


#define SU_COM_SECTION          "Com"
#define     SU_COM_LISTEN           "Listen"                /* string */
#define     SU_COM_SYNCLISTEN       "SyncListen"            /* string */
#define     SU_COM_HSBLISTEN        "HSBListen"             /* string */
#define     SU_COM_CONNECT          "Connect"               /* string */
#define     SU_COM_MAXPHYSMSGLEN    "MaxPhysMsgLen"         /* int */
#define     SU_COM_READBUFSIZE      "ReadBufSize"           /* int */
#define     SU_COM_WRITEBUFSIZE     "WriteBufSize"          /* int */

#define     SU_COM_SYNCREAD         "SyncRead"              /* Yes/No */
#define     SU_COM_SYNCWRITE        "SyncWrite"             /* Yes/No */
#define     SU_COM_WPOOLMIN         "MinWritePoolBuffers"   /* int, count */
#define     SU_COM_WPOOLMAX         "MaxWritePoolBuffers"   /* int, count */
#define     SU_COM_WPOOLINCR        "WritePoolIncrement"    /* int, count */
#define     SU_COM_PACKETCONTROL    "PacketControl"         /* Yes/No */
#define     SU_COM_ALLOWYIELD       "AllowYield"            /* Yes/No */
#define     SU_COM_SELTHREAD        "SelectThread"          /* Yes/No */
#define     SU_COM_TRACE            "Trace"                 /* Yes/No */
#define     SU_COM_TRACEFILENAME    "TraceFile"             /* string, filename */
#define     SU_COM_READGATE         "ReadGate"              /* Yes/No */
#define     SU_COM_KEEPDLLSINMEMORY "KeepDLLsInMemory"      /* Yes/No */
#define     SU_COM_ONLYONELISTEN    "OnlyOneListen"         /* Yes/No */
#define     SU_COM_SYNCREADTIMEOUT  "ClientReadTimeout"     /* long, in ms */
#define     SU_COM_ASYNCREADTIMEOUT "ServerReadTimeout"     /* long, in ms */
#define     SU_COM_CONNECTTIMEOUT   "ConnectTimeout"        /* long, in ms */
#define     SU_COM_MAXCONNPOOLSIZE  "RConnectPoolSize"      /* int, count */
#define     SU_COM_CONNLIFETIME     "RConnectLifetime"      /* int, in seconds */
#define     SU_COM_RCONNRPCTIMEOUT  "RConnectRPCTimeout"    /* long, in ms */

#define     SU_COM_TCPKEEPALIVE     "TcpKeepAlive"               /* Yes/No */
#define     SU_COM_TCPKEEPIDLE      "TcpKeepAliveIdleTime"       /* int, idle time before sending 1st keepalive probe, in seconds */
#define     SU_COM_TCPKEEPCNT       "TcpKeepAliveProbeCount"     /* int, number of keepalive probes sent */
#define     SU_COM_TCPKEEPINTVL     "TcpKeepAliveProbeInterval"  /* int, interval between keepalive probes, in seconds */
#define     SU_COM_SOCKETLINGER     "SocketLinger"           /* Yes/No sets SO_LINGER on/off */
#define     SU_COM_SOCKETLINGERTIME "SocketLingerTime"       /* Adjusts the time of SO_LINGER, clock ticks */

#define SU_PROTOCOL_SECTION     "Protocol"

#define SU_SRV_SECTION          "Srv"
#define     SU_SRV_NUMTHREADS       "Threads"           /* int */
#define     SU_SRV_MAXACTIVETHR     "MaxActiveThreads"  /* int */
#define     SU_SRV_FORCETHREADS2SYSSCOPE "ForceThreadsToSystemScope" /* Yes/No */
#define     SU_SRV_AT               "At"                /* comma delimited string */
#define     SU_SRV_BASEPRIORITY     "ProcessBasePriority"   /* int */
#define     SU_SRV_IDLEPRIORITY     "ProcessIdlePriority"   /* int */
#define     SU_SRV_WORKINGSETSIZE   "ProcessWorkingSetSize" /* int, bytes */
#define     SU_SRV_CONNECTTIMEOUT   "ConnectTimeOut"    /* int, minutes */
#define     SU_SRV_ABORTTIMEOUT     "AbortTimeOut"      /* int, minutes */
#define     SU_SRV_ADAPTIVEROWSPERMESSAGE "AdaptiveRowsPerMessage" /* yes/no */
#define     SU_SRV_ROWSPERMESSAGE   "RowsPerMessage"    /* int */
#define     SU_SRV_EXECROWSPERMESSAGE     "ExecRowsPerMessage" /* int */
#define     SU_SRV_SYNCEXECROWSPERMESSAGE "SyncExecRowsPerMessage" /* int, 0/1 */
#define     SU_SRV_TUPLESPERMESSAGE "TuplesPerMessage"  /* synonym to SU_SRV_ROWSPERMESSAGE */
#define     SU_SRV_ECHO             "Echo"              /* yes/no */
#define     SU_SRV_SILENT           "Silent"            /* yes/no */
#define     SU_SRV_DISABLEMERGE     "DisableMerge"      /* yes/no */
#define     SU_SRV_SIGNALCATCH      "SignalCatch"       /* yes/no */
#define     SU_SRV_USEREADTHREAD    "UseReadThread"     /* obsolete, use ReadThreadMode instead (yes/no) */
#define     SU_SRV_READTHREADMODE   "ReadThreadMode"    /* 0,1,2 */
#define     SU_SRV_MAXREADTHREADPOOL "MaxReadThreadPoolSize" /* int */
#define     SU_SRV_NAME             "Name"              /* string */
#define     SU_SRV_ALLOWCONNECT     "AllowConnect"      /* yes/no */
#define     SU_SRV_AFTERBACKUP      "AfterBackup"       /* string, at commands */
#define     SU_SRV_TIMESLICE        "TimeSlice"         /* int 1..1000 */
#define     SU_SRV_MSGLOGSIZE       "MessageLogSize"    /* int, bytes */
#define     SU_SRV_TRACELOGSIZE     "TraceLogSize"      /* int, bytes */
#define     SU_SRV_CONFIRMSHUTDOWN  "ConfirmShutdown"   /* yes/no */
#define     SU_SRV_MONITOR          "Monitor"           /* yes/no */
#define     SU_SRV_SSDEBUG          "SsDebug"           /* string */
#define     SU_SRV_UNLOADMAXWAIT    "UnloadMaxWait"     /* int, sec */
#define     SU_SRV_LOGAUDITINFO     "LogAuditInfo"      /* yes/no */
#define     SU_SRV_LOGADMINCOMMANDS "LogAdminCommands"  /* yes/no */
#define     SU_SRV_MAXOPENCURSORS   "MaxOpenCursors"    /* int, count */
#define     SU_SRV_TRACETYPE        "TraceType"         /* string */
#define     SU_SRV_TRACELEVEL       "TraceLevel"        /* int */
#define     SU_SRV_DISABLEOUTPUT    "DisableOutput"     /* yes/no */
#define     SU_SRV_PRINTMSGCODE     "PrintMsgCode"      /* yes/no */
#define     SU_SRV_PESSIMISTICUSENFETCH "PessimisticTableUseNFetch" /* Yes/No */
#define     SU_SRV_USERTASKQUEUE    "UserTaskQueue"     /* Yes/No */
#define     SU_SRV_MAXCMPLEN        "MaxConstraintLength" /* int, default is 254 */
#define     SU_SRV_MAXBGTASKINTERVAL "MaxBgTaskInterval"   /* int */
#define     SU_SRV_CONNECTIONCHECKINTERVAL "ConnectionCheckInterval"   /* int */
#define     SU_SRV_NLOCALSTARTTASKS "LocalStartTasks"   /* int, count */
#define     SU_SRV_NREMOTESTARTTASKS    "RemoteStartTasks"  /* int, count */
#define     SU_SRV_MAXSACSTMTS      "MaxStartStatements"    /* int, count */
#define     SU_SRV_MAXRPCDATALENGTH "MaxRpcDataLength"  /* int, bytes */
#define     SU_SRV_REPORTINTERVAL   "ReportInterval"    /* int, seconds */
#define     SU_SRV_TRACESECDECIMALS "TraceSecDecimals"  /* int */
#define     SU_SRV_ALLOWCORE        "AllowCore"         /* yes/no */
#define     SU_SRV_NETBACKUPROOTDIR "NetBackupRootDir"  /* string */
#define     SU_SRV_MEMREPORTLIMIT   "MemoryReportLimit"     /* int */
#define     SU_SRV_MEMREPORTDELTA   "MemoryReportDelta"  /* int */
#define     SU_SRV_MEMSIZEREPORTINTERVAL "MemorySizeReportInterval"   /* int, bytes */
#define     SU_SRV_DBSIZEREPORTINTERVAL  "DatabaseSizeReportInterval" /* int, bytes */
#define     SU_SRV_STMTMEMTRACELIMIT    "StatementMemoryTraceLimit"  /* int */

#define SU_CLI_SECTION          "Client"
#define     SU_CLI_CHSET            "CharacterSet"      /* string */
#define     SU_CLI_SSDEBUG          "SsDebug"           /* string */
#define     SU_CLI_BLOBCRC          "BlobCRC"           /* yes/no */
#define     SU_CLI_STATEMENTCACHE   "StatementCache"      /* int, count */
#define     SU_CLI_ROWSPERMESSAGE   "RowsPerMessage"    /* int */
#define     SU_CLI_EXECROWSPERMESSAGE "ExecRowsPerMessage" /* int */
#define     SU_CLI_NOASSERTMESSAGES "NoAssertMessages"  /* yes/no */

#define SU_SQL_SECTION          "SQL"
#define     SU_SQL_CHARPADDING          "CharPadding"         /* yes/no */
#define     SU_SQL_CURSORCLOSEATENDTRAN "CursorCloseAtTransEnd" /* yes/no */
#define     SU_SQL_INFO                 "Info"                /* int, level, 0-9 */
#define     SU_SQL_SQLINFO              "SQLInfo"             /* int, level, 0-4 */
#define     SU_SQL_INFOFILENAME         "InfoFileName"        /* string */
#define     SU_SQL_INFOFILESIZE         "InfoFileSize"        /* int, bytes */
#define     SU_SQL_INFOFILEFLUSH        "InfoFileFlush"       /* yes/no */
#define     SU_SQL_WARNING              "Warning"             /* yes/no */
#define     SU_SQL_SORTARRAYSIZE        "SortArraySize"       /* int */
#define     SU_SQL_CONVERTORSTOUNIONS   "ConvertOrsToUnions"  /* yes/no */
#define     SU_SQL_CONVERTORSTOUNIONSCOUNT "ConvertOrsToUnionsCount" /* int, count */
#define     SU_SQL_ALLOWDUPLICATEINDEX  "AllowDuplicateIndex" /* yes/no */
#define     SU_SQL_STATEMENTCACHE       "StatementCache"      /* int, count */
#define     SU_SQL_PROCEDURECACHE       "ProcedureCache"      /* int, count */
#define     SU_SQL_TRIGGERCACHE         "TriggerCache"        /* int, count */
#define     SU_SQL_MAXNESTEDTRIG        "MaxNestedTriggers"   /* int, count */
#define     SU_SQL_MAXNESTEDPROC        "MaxNestedProcedures" /* int, count */
#define     SU_SQL_ESTSAMPLELIMIT       "EstSampleLimit"      /* int, count */
#define     SU_SQL_ESTSAMPLECOUNT       "EstSampleCount"      /* int, count */
#define     SU_SQL_ESTSAMPLEMIN         "EstSampleMin"        /* int, count */
#define     SU_SQL_ESTSAMPLEMAX         "EstSampleMax"        /* int, count */
#define     SU_SQL_ESTSAMPLEINC         "EstSampleInc"        /* int, count */
#define     SU_SQL_ESTSAMPLEMAXEQROWEST "EstSampleMaxEqualRowEstimate" /* int, count */
#define     SU_SQL_OPTN                 "OptimizeRows"        /* int, count */
#define     SU_SQL_VECTOROPTN           "VectorConstrOptimizeRows" /* int, count */
#define     SU_SQL_EQUAL_USERANGEEST    "UseRangeEstimates"   /* yes/no */
#define     SU_SQL_EQUAL_SELECTIVITY    "EqualSelectivity"    /* double, [0.0..1.0] */
#define     SU_SQL_NOTEQUAL_SELECTIVITY "NotequalSelectivity" /* double, [0.0..1.0] */
#define     SU_SQL_COMPARE_SELECTIVITY  "CompareSelectivity"  /* double, [0.0..1.0] */
#define     SU_SQL_LIKE_SELECTIVITY     "LikeSelectivity"     /* double, [0.0..1.0] */
#define     SU_SQL_ISNULL_SELECTIVITY   "IsnullSelectivity"   /* double, [0.0..1.0] */
#define     SU_SQL_ISNOTNULL_SELECTIVITY "IsnotnullSelectivity" /* double, [0.0..1.0] */
#define     SU_SQL_NO_SELECTIVITY       "NoSelectivity"       /* double, [0.0..1.0] */
#define     SU_SQL_SELECTIVITY_DROP_LIMIT "SelectivityDropLimit" /* double, [0.0..1.0] */
#define     SU_SQL_MIN_HIT_RATE_FOR_INDEX   "MinHitRateForIndex"     /* double, [0.0..1.0] */
#define     SU_SQL_MAX_HIT_RATE_FOR_INDEX   "MaxHitRateForIndex"     /* double, [0.0..1.0] */
#define     SU_SQL_MIN_HIT_RATE_FOR_DATA    "MinHitRateForData"      /* double, [0.0..1.0] */
#define     SU_SQL_MAX_HIT_RATE_FOR_DATA    "MaxHitRateForData"      /* double, [0.0..1.0] */
#define     SU_SQL_MAX_KEY_ENTRY_SIZE   "MaxKeyEntrySize"     /* double, bytes */
#define     SU_SQL_PREJOIN_DENSITY      "PrejoinDensity"      /* double, [0.0..1.0] */
#define     SU_SQL_TIME_FOR_INDEX_SEARCH "TimeForIndexSearch" /* double, microseconds */
#define     SU_SQL_TIME_PER_INDEX_ENTRY "TimePerIndexEntry"   /* double, microseconds */
#define     SU_SQL_BLOCK_ACCESS_TIME    "BlockAccessTime"     /* double, microseconds */
#define     SU_SQL_ROW_SORT_TIME        "RowSortTime"         /* double, microseconds */
#define     SU_SQL_SORTEDGROUPBY        "SortedGroupBy"       /* int [0,1,2] */
#define     SU_SQL_ESTIGNOREORDERBY     "EstIgnoreOrderBy"    /* int [0,1,2] */
#define     SU_SQL_UPCASEQUOTED         "UpCaseQuotedIdentifiers" /* yes/no */
#define     SU_SQL_CHAR2BINASSIGN       "AllowChar2BinAssign" /* yes/no */
#define     SU_SQL_USERVECTORCONSTR     "UseVectorConstr"     /* yes/no */
#define     SU_SQL_BLOBEXPRLIMIT        "MaxBlobExpressionSize" /* long, bytes */
#define     SU_SQL_EMULATEOLDTIMESTAMPDIFF "EmulateOldTimeStampDiff" /* yes/no */
#define     SU_SQL_SIMPLESQLOPT         "SimpleSQLOpt"        /* yes/no */
#define     SU_SQL_HURCREVERSE          "HurcReverse"         /* yes/no */
#define     SU_SQL_INDEXCURSORRESET     "IndexCursorReset"    /* yes/no */
#define     SU_SQL_ISOLATIONLEVEL       "IsolationLevel"            /* int */
#define     SU_SQL_SETTRANSCOMPATIBILITY3 "SetTransCompatibility3" /* yes/no */
#define     SU_SQL_TIMESTAMPDISPLAYSIZE19 "TimestampDisplaySize19" /* yes/no */
#define     SU_SQL_LATEINDEXPLAN          "EnableLateIndexPlan"    /* yes/no */
#define     SU_SQL_SIMPLEOPTIMIZERRULES   "SimpleOptimizerRules"    /* yes/no */
#define     SU_SQL_USERELAXEDREACOMMITTED "RelaxedReadCommitted"    /* yes/no */

#define SU_HINTS_SECTION        "Hints"
#define     SU_HINTS_ENABLE         "EnableHints"       /* yes/no */

#define SU_XS_SECTION           "Sorter"
#define     SU_XS_POOLPERCENTTOTAL  "MaxCacheUsePercent"/* int, % (10..50) */
#define     SU_XS_POOLSIZEPER1SORT  "MaxMemPerSort"     /* int, bytes */
#define     SU_XS_MAXFILESPER1SORT  "MaxFilesPerSort"   /* int, count  */
#define     SU_XS_MAXFILESTOTAL     "MaxFilesTotal"     /* int, count */
#define     SU_XS_TMPDIR_1          "TmpDir_1"          /* for admin command 'par' */
#define     SU_XS_TMPDIR            "TmpDir_%u"         /* string [int] */
#define     SU_XS_MAXBYTESPERSTEP   "MaxBytesPerStep"   /* int, size */
#define     SU_XS_MAXROWSPERSTEP    "MaxRowsPerStep"    /* int, count */
#define     SU_XS_BLOCKSIZE         "BlockSize"         /* int, bytes (0=same as indexfile blocksize */
#define     SU_XS_FILEBUFFERING     "FileBuffering"     /* yes/no */
#define     SU_XS_SORTERENABLED     "SorterEnabled"     /* yes/no */

#define SU_SF_SUPERFASTSECTION    "Accelerator"

#define     SU_SF_IMPLICITSTART         "ImplicitStart" /* Can SF started implicitly */

#define     SU_SF_CHECKPOINT_PRIO       "CheckpointPriority"    /* prio values are 'High','Normal' or 'Idle' */
#define     SU_SF_BACKUP_PRIO           "BackupPriority"
#define     SU_SF_MERGE_PRIO            "MergePriority"
#define     SU_SF_LOCALUSERS_PRIO       "LocalUserPriority"
#define     SU_SF_REMOTEUSERS_PRIO      "RemoteUserPriority"
#define     SU_SF_SYNC_HISTCLEAN_PRIO   "SyncHistCleanPriority"
#define     SU_SF_SYNC_MESSAGE_PRIO     "SyncMessagePriority"
#define     SU_SF_HOTSTANDBY_PRIO       "HotStandByPriority"
#define     SU_SF_HOTSTANDBY_CATCHUP_PRIO       "HotStandByCatchupPriority"

#define SU_REP_CLUSTERSECTION       "Cluster"
#define     SU_REP_CLUSTERENABLED       "ClusterEnabled"
#define     SU_REP_SPOKE_NAME           "Name"              /* string */
#define     SU_REP_READMOSTLY_LOADPERCENT_AT_PRIMARY "ReadMostlyLoadPercentAtPrimary" /* int 0-100 (percent) */

#define SU_REP_HOTSTANDBYSECTION    "HotStandby"
#define     SU_REP_MASTER               "Primary"
#define     SU_REP_CONNECT              "Connect"
#define     SU_REP_AUTOSWITCH           "AutoSwitch"
#define     SU_REP_SYNCDIR              "CopyDirectory"
#define     SU_REP_STARTUPFORCESLAVE    "StartupForceSecondary"
#define     SU_REP_STARTUPSTANDALONE    "StartupStandAlone"
#define     SU_REP_ROLESWITCHTHROWOUT   "RoleSwitchThrowout"
#define     SU_REP_MAXLOGSIZE           "MaxLogSize"
#define     SU_REP_MAXMEMLOGSIZE        "MaxMemLogSize"
#define     SU_REP_LOGBLOCKSIZESIZE     "LogBlockSize"
#define     SU_REP_TIMEOUT              "Timeout"           /* Milliseconds */
#define     SU_REP_PINGTIMEOUT          "PingTimeout"       /* Milliseconds */
#define     SU_REP_READTIMEOUT          "ReadTimeout"       /* Milliseconds */
#define     SU_REP_NETCOPYRPCTIMEOUT    "NetcopyRpcTimeout"       /* Milliseconds */
#define     SU_REP_ALONEDISABLEPING     "AloneDisablePing"  /* yes/no */
#define     SU_REP_RECONNECTCTR         "ReconnectCounter"  /* Operations */
#define     SU_REP_RECONNECTTIME        "ReconnectTime"     /* Seconds */
#define     SU_REP_MAXRPCOPCOUNT        "MaxRPCOperationCount" /* Operations */
#define     SU_REP_MAXOPLISTLEN         "MaxOpListLen"
#define     SU_REP_PRIMARYALONE         "PrimaryAlone"      /* yes/no */
#define     SU_REP_AUTOPRIMARYALONE     "AutoPrimaryAlone"  /* yes/no */
#define     SU_REP_CATCHUP_STEPSTOSKIP  "CatchupStepsToSkip"
#define     SU_REP_WAITFORCATCHUP       "WaitForCatchup"
#define     SU_REP_DUMMYCOMMITLIMIT     "DummyCommitLimit"
#define     SU_REP_CONNECTTIMEOUT       SU_COM_CONNECTTIMEOUT
#define     SU_REP_2SAFEACKPOLICY        "2SafeAckPolicy"
#define     SU_REP_SAFENESSLEVEL        "SafenessLevel"
#define     SU_REP_SAFENESSUSEDURABILITYLEVEL  "SafenessUseDurabilityLevel" /* yes/no, dbe7cfg.c */
#define     SU_REP_HSBENABLED           "HSBEnabled"
#define     SU_REP_PINGINTERVAL         "PingInterval"       /* Milliseconds */
#define     SU_REP_DURABLE_LOGREC_BYTES "DurableLogByteslimit"
#define     SU_REP_DURABLE_LOGREC_ROWS  "DurableLogRowslimit"

#define     SU_HSB_1SAFEMAXDELAY        "1SafeMaxDelay" /* int, milliseconds */

#define     SU_HSB_CATCHUP_BLOCK_BYTES  "CatchupBlockSizeBytes" /* int 128bytes - 512Kb */
#define     SU_HSB_CATCHUP_BLOCK_PERCENT "CatchupSpeedRate"  /* int 1-99 */
#define     SU_HSB_SECONDARY_BUSY_LIMIT "SecondaryBusyLimit"  /* int default is 256 */

#define SU_SYNC_SECTION             "Synchronizer"
#define     SU_SYNC_MLEVEL                      "MultiLevel"           /* yes/no */
/* #define     SU_SYNC_MASTERDIRECTSAVE            "MasterDirectSave"  */   /* yes/no */
#define     SU_SYNC_MASTEREXECNOLOGFLUSH        "MasterExecNoLogFLush" /* yes/no */
/* #define     SU_SYNC_NEEDPUBLICATIONREGISTRATION "NeedPublicationRegistration" */ /* yes/no */
#define     SU_SYNC_MASTERMSGAUTOSTART          "MasterMessageAutostart"  /* yes(=dfault)/no */
#define     SU_SYNC_SUBSCRIBE_DELETEFIRST_OPT_ENABLED "SubscribeDeleteFirstOpt"  /* yes/no(=default) */
#define     SU_SYNC_CONNECTSTR_FOR_MASTER       "ConnectStrForMaster"
#define     SU_SYNC_RPCEVENT_THRESHOLD_BYTECOUNT "RpcEventThresholdByteCount" /* long,bytes (0=default -> no rpc event posting) */
#define     SU_SYNC_CONNECTTIMEOUT              SU_COM_CONNECTTIMEOUT
#define     SU_SYNC_STMT_CACHE_SIZE             "MasterStatementCache" /* int (default is 10 in Saana) */

#define     SU_SYNC_ISOLATIONLEVEL              "RefreshIsolationLevel"           /* int */
#define     SU_SYNC_REFRESHREADLEVELROWS        "RefreshReadLevelRows"            /* int */
#define     SU_SYNC_REFRESH_LOAD_PERCENTAGE     "ReplicaRefreshLoad"              /* int percentage 0-100 (default is 100%) */

#define SU_LICENSE_SECTION      "License"
#define     SU_LICENSEVERSION   "LicenseVersion" /* int, starting at 4 */
#define     SU_LICENSEKEY       "LicenseKey"     /* hexdump string */

#define SU_SPECIAL_SECTION      "Special"
#define     SU_CIRCUMVENT_WARP_LOCK_BUG "CircumventWarpLockBug" /* yes/no */

#define SU_HTTP_SECTION         "HTTP"
#define     SU_HTTP_ALLOWCONNECT    "AllowConnect"  /* yes/no */
#define     SU_HTTP_USECOOKIE       "UseCookie"     /* yes/no */

/* DANGER! [SQL Editor] (W16) parameters are read through Windows API
 * GetPrivateProfilexxx() functions.
 */
#define SU_SED_SECTION          "SQL Editor"
#define     SU_SED_DATASOURCENAME       "DSN"           /* netwkname */
#define     SU_SED_DELIMCHAR            "DelimChar"     /* string, default ';' */
#define     SU_SED_AUTOCONNECT          "AutoConnect"   /* yes/no */

#define SU_RC_SECTION           "RCon"
#define     SU_RC_SERVERDEF         "ServerDef_%u"      /* netwkname, descr */

#define SU_DSN_SECTION "Data Sources"
                /* format: <datasourcename> = <netwkname>, <descr> */

#define SU_TF_SECTION           "TransparentFailover"
#define     SU_TF_TIMEOUT               "ReconnectTimeout"
#define     SU_TF_WAIT_TIMEOUT          "WaitTimeout"

/* NetworkName keyword that is searched from odbc.ini */

#define     SU_ODBC_NETWORKNAME "NetworkName"

#ifdef SS_MYSQL
#define SU_MYSQLD_SECTION       "mysqld"
#define     SU_MYSQLD_DBE_BLOCKSIZE     "soliddb_db_block_size"
#endif

#endif /* SU0CFGST_H */

