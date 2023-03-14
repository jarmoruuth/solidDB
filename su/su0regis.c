/*************************************************************************\
**  source       * su0regis.c
**  directory    * su
**  description  * Contains the main register of parameters.
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

#include <ssenv.h>

#include <su0regis.h>

/*
 * Structure is like this because:
 * - it was fast to generate and this is, after all, quick-and-dirty.
 * - no, there are no more reasons.
 *
 * NOTE: this code was later officially declared not dirty so now it is
 *      just quick, or quick-and-clean.
 *
 * It could consist of (section, parameter) pairs and use defines from
 * su0cfgst.h but, well, see the reason.
 *
 */

const char* su_regis_simple_register[] = {
    "Accelerator.BackupPriority",
    "Accelerator.CheckpointPriority",
    "Accelerator.HotStandByCatchupPriority",
    "Accelerator.HotStandByPriority",
    "Accelerator.ImplicitStart",
    "Accelerator.LocalUserPriority",
    "Accelerator.MergePriority",
    "Accelerator.RemoteUserPriority",
    "Accelerator.SyncHistCleanPriority",
    "Accelerator.SyncMessagePriority",

    "Client.BlobCRC",
    "Client.CharacterSet",
    "Client.ExecRowsPerMessage",
    "Client.NoAssertMessages",
    "Client.RowsPerMessage",
    "Client.SsDebug",
    "Client.StatementCache",

    "Com.AllowYield",
    "Com.ClientReadTimeout",
    "Com.Connect",
    "Com.ConnectTimeout",
    "Com.HSBListen",
    "Com.KeepDLLsInMemory",
    "Com.Listen",
    "Com.MaxPhysMsgLen",
    "Com.MaxWritePoolBuffers",
    "Com.MinWritePoolBuffers",
    "Com.OnlyOneListen",
    "Com.PacketControl",
    "Com.RConnectLifetime",
    "Com.RConnectPoolSize",
    "Com.RConnectRPCTimeout",
    "Com.ReadBufSize",
    "Com.ReadGate",
    "Com.SelectThread",
    "Com.ServerReadTimeout",
    "Com.SyncListen",
    "Com.SyncRead",
    "Com.SyncWrite",
    "Com.TraceFile",
    "Com.Trace",
    "Com.WriteBufSize",
    "Com.WritePoolIncrement",
    "Com.TcpKeepAlive",
    "Com.TcpKeepAliveIdleTime",
    "Com.TcpKeepAliveProbeCount",
    "Com.TcpKeepAliveProbeInterval",
    "Com.SocketLinger",
    "Com.SocketLingerTime",

    "General.AllowLockBounce",
    "General.BackupBlockSize",
    "General.BackupCopyIniFile",
    "General.BackupCopyLog",
    "General.BackupCopySolmsgOut",
    "General.BackupDeleteLog",
    "General.BackupDirectory",

    "General.NetBackupConnect",
    "General.NetBackupCopyInifile",
    "General.NetBackupCopyLog",
    "General.NetBackupCopySolmsgout",
    "General.NetbackupDeleteLog",
    "General.NetBackupDirectory",
    "General.NetBackupConnectTimeout",
    "General.NetBackupReadTimeout",

    "General.BackupStepsToSkip",
    "General.CheckEscalateLimit",
    "General.CheckpointDeleteLog",
    "General.CheckpointInterval",
    "General.CollationSeq",
    "General.DefaultIsGlobalTemporary",
    "General.DefaultIsTransient",
    "General.DefaultStoreIsMemory",
    "General.DisableIdleMerge",
    "General.FakeMerge",
    "General.FileWriteFlushMode",
    "General.IOThreads",
    "General.LockEscalateLimit",
    "General.LockHashSize",
    "General.LockWaitTimeOut",
    "General.FastDeadlockDetect",
    "General.DeadlockDetectMaxDepth",
    "General.DeadlockDetectMaxLocks",
    "General.LongSequentialSearchLimit",
    "General.MaxOpenFiles",
    "General.MergeInterval",
    "General.MinCheckpointTime",
    "General.MinMergeTime",
    "General.StartupForceMerge",
    "General.MaxMergeTasks",
    "General.UserMerge",
    "General.MaxMergeParts",
    "General.MaxUserMergeSteps",
    "General.SplitPurge",
    "General.MergeCleanup",
    "General.UseNewBtreeLocking",
    "General.UseRandomKeySampleRead",
    "General.SingleDeletemark",
    "General.OptimisticLockWaitTimeOut",
    "General.Pessimistic",
    "General.UsePessimisticGate",
    "General.QuickMergeInterval",
    "General.ReadEscalateLimit",
    "General.ReadOnly",
    "General.SearchBufferLimit",
    "General.SplitMerge",
    "General.TableBufferSize",
    "General.TableLockWaitTimeOut",
    "General.TransactionEarlyValidate",
    "General.TransactionHashSize",
    "General.UseIOThreadsOnlyDeprecated",
    "General.UseNewTransWaitReadLevel",
    "General.UseRelaxedReadLevel",
    "General.VersionedPessimisticReadCommitted",
    "General.VersionedPessimisticRepeatableRead",
    "General.ReadLevelMaxTime",
    "General.DataDictionaryErrorMaxWait",
    "General.WriterIOThreads",
    "General.PhysicalDropTable",
    "General.RelaxedBtreeLocking",
    "General.SpinCount",

#ifdef SS_TC_CLIENT_NOT_IN_SAANA
    "Cluster.ClusterEnabled",
#endif
#ifdef SS_SAANA_CLUSTER
    "Cluster.Name",
#endif /* SS_SAANA_CLUSTER */

    "Cluster.ReadMostlyLoadPercentAtPrimary",

    "HotStandby.1SafeMaxDelay",
    "HotStandby.2SafeAckPolicy",
    "HotStandby.AloneDisablePing",
    "HotStandby.ASSERTONCONNECTFAILED",
    "HotStandby.AutoPrimaryAlone",
    "HotStandby.AutoSwitch",
    "HotStandby.CatchupBlockSizeBytes",
    "HotStandby.CatchupSpeedRate",
    "HotStandby.Connect",
    "HotStandby.ConnectTimeout",
    "HotStandby.CopyDirectory",
    "HotStandby.DummyCommitLimit",
    "HotStandby.DurableLogByteslimit",
    "HotStandby.DurableLogRowslimit",
    "HotStandby.HSBEnabled",
    "HotStandby.LogBlockSize",
    "HotStandby.MaxLogSize",
    "HotStandby.MaxOpListLen",
    "HotStandby.MaxMemLogSize",
    "HotStandby.MaxRPCOperationCount",
    "HotStandby.PingInterval",
    "HotStandby.PingTimeOut",
    "HotStandby.Primary",           /* What the hell is this? */
    "HotStandby.PrimaryAlone",
    "HotStandby.ReadTimeOut",
    "HotStandby.NetcopyRpcTimeout",
    "HotStandby.ReconnectCounter",
    "HotStandby.ReconnectTime",
    "HotStandby.RoleSwitchThrowout",
    "HotStandby.SafeAckPolicy",
    "HotStandby.SafenessLevel",
    "HotStandby.SafenessUseDurabilityLevel",
    "HotStandby.SecondaryBusyLimit",
    "HotStandby.StartupForceSecondary",
    "HotStandby.StartupStandAlone",
    "HotStandby.WaitForCatchup",

    "HTTP.AllowConnect",
    "HTTP.UseCookie",

    "IndexFile.BlockSize",
    "IndexFile.CacheSize",
    "IndexFile.CleanPageSearchLimit",
#ifdef IO_OPT
    "IndexFile.DirectIO",
#endif
    "IndexFile.EstSampleRndKeys",
    "IndexFile.EstSampleRndNodes",
    "IndexFile.ExtendIncrement",
#if 0 /* filespec params are accepted without check */
    "IndexFile.FileSpec_1",
    "IndexFile.FileSpec_2",
#endif /* 0 */
    "IndexFile.FreeListReserveSize",
    "IndexFile.MaxPageSemaphores",
    "IndexFile.MaxSequentialAllocation",
    "IndexFile.PreFlushDirtyPercent",
    "IndexFile.LowPriorityLRUSkipPercent",
    "IndexFile.PreFlushPercent",
    "IndexFile.PreFlushSampleSize",
    "IndexFile.ReadAhead",
#ifdef FSYNC_OPT
    "IndexFile.SyncWrite",
    "IndexFile.ForceFileFlush",
#endif
    "IndexFile.SynchronizedWrite",
    "IndexFile.UseBtreeGate",
    "IndexFile.UseNewKeyCheck",
    "IndexFile.UseShortKeyOpt",
    "IndexFile.FreeListGloballySorted",

    /* not really parameters; license system just uses su0inifi to read the
     * license file */
    "License.LicenseKey",
    "License.LicenseVersion",

    "Logging.BlockSize",
    "Logging.CommitMaxWait",
    "Logging.DigitTemplateChar",
#ifdef IO_OPT
    "Logging.DirectIO",
#endif
    "Logging.DurabilityLevel",
#ifdef FSYNC_OPT
    "Logging.ForceFileFlush",
#endif /* FSYNC_OPT */
    "Logging.FileFlush",
    "Logging.FileNameTemplate",
    "Logging.LogDir",
    "Logging.LogEnabled",
    "Logging.LogWriteMode",
    "Logging.MinSplitSize",
    "Logging.RelaxedMaxDelay",
    "Logging.SyncWrite",
    "Logging.UseGroupCommitQueue",
    "Logging.MaxWriteQueueRecords",
    "Logging.MaxWriteQueueBytes",
    "Logging.WriteQueueFlushLimit",
    "Logging.DelayMesWait",
    "Logging.ExtendIncrement",
    "Logging.WriteBufferSize",

    "MME.ImdbMemoryLimit",
    "MME.ImdbMemoryLowPercentage",
    "MME.LockEscalationEnabled",
    "MME.LockEscalationLimit",
    "MME.LockHashSize",
    "MME.ReleaseMemoryAtShutdown",
    "MME.MaxCacheUsage",
    "MME.MutexGranularity",

    "Sorter.BlockSize",
    "Sorter.FileBuffering",
    "Sorter.MaxBytesPerStep",
    "Sorter.MaxCacheUsePercent",
    "Sorter.MaxFilesPerSort",
    "Sorter.MaxFilesTotal",
    "Sorter.MaxMemPerSort",
    "Sorter.MaxRowsPerStep",
    "Sorter.SorterEnabled",
#if 0
    "Sorter.TmpDir_1",
    "Sorter.TmpDir_2",
#endif /* 0 */

    "Special.CircumventWarpLockBug",
    "Special.ss_semsleep_loopcount",
    "Special.ss_semsleep_loopsem",
    "Special.ss_semsleep_maxloopcount",
    "Special.ss_semsleep_maxtime",
    "Special.ss_semsleep_mintime",
    "Special.ss_semsleep_random_freq",
    "Special.ss_semsleep_random",
    "Special.ss_semsleep_startnum",
    "Special.ss_semsleep_stopnum",

    "SQL.AllowChar2BinAssign",
    "SQL.BlockAccessTime",
    "SQL.CharPadding",
    "SQL.CompareSelectivity",
    "SQL.ConvertOrsToUnionsCount",
    "SQL.ConvertOrsToUnions",
    "SQL.AllowDuplicateIndex",
    "SQL.DataSizePerIndex",
    "SQL.EnableHints",
    "SQL.EmulateOldTimeStampDiff",
    "SQL.EqualSelectivity",
    "SQL.EstIgnoreOrderBy",
    "SQL.EstSampleCount",
    "SQL.EstSampleInc",
    "SQL.EstSampleLimit",
    "SQL.EstSampleMax",
    "SQL.EstSampleMin",
    "SQL.EstSampleMaxEqualRowEstimate",
    "SQL.MinHitRateForData",
    "SQL.MaxHitRateForData",
    "SQL.MinHitRateForIndex",
    "SQL.MaxHitRateForIndex",
    "SQL.HurcReverse",
    "SQL.IndexCursorReset",
    "SQL.EnableLateIndexPlan",
    "SQL.SimpleOptimizerRules",
    "SQL.InfoFileFlush",
    "SQL.InfoFileName",
    "SQL.InfoFileSize",
    "SQL.Info",
    "SQL.IsnotnullSelectivity",
    "SQL.IsnullSelectivity",
    "SQL.IsolationLevel",
    "SQL.LikeSelectivity",
    "SQL.MaxBlobExpressionSize",
    "SQL.MaxKeyEntrySize",
    "SQL.MaxNestedProcedures",
    "SQL.MaxNestedTriggers",
    "SQL.NoSelectivity",
    "SQL.NotequalSelectivity",
    "SQL.OptimizeRows",
    "SQL.PrejoinDensity",
    "SQL.ProcedureCache",
    "SQL.RowSortTime",
    "SQL.SelectivityDropLimit",
    "SQL.SetTransCompatibility3",
    "SQL.SimpleSQLOpt",
    "SQL.SortArraySize",
    "SQL.SortedGroupBy",
    "SQL.SQLInfo",
    "SQL.StatementCache",
    "SQL.TimeForIndexSearch",
    "SQL.TimePerIndexEntry",
    "SQL.TriggerCache",
    "SQL.UpCaseQuotedIdentifiers",
    "SQL.UseRangeEstimates",
    "SQL.UseVectorConstr",
    "SQL.VectorConstrOptimizeRows",
    "SQL.Warning",
    "SQL.TimestampDisplaySize19",
    "SQL.CursorCloseAtTransEnd",
    "SQL.EnableLateIndexPlan",
    "SQL.SimpleOptimizerRules",
    "SQL.RelaxedReadCommitted",

#if 0
    "SQL Editor.DSN",
    "SQL Editor.DelimChar",
    "SQL Editor.AutoConnect",
#endif /* 0 */

    "Srv.AbortTimeOut",
    "Srv.AfterBackup",
    "Srv.AllowConnect",
    "Srv.At",
    "Srv.ConfirmShutdown",
    "Srv.ConnectionCheckInterval",
    "Srv.ConnectTimeOut",
    "Srv.DisableMerge",
    "Srv.DisableOutput",
#ifdef SS_COREOPT
    "Srv.AllowCore",
#endif
    "Srv.Echo",
    "Srv.Silent",
    "Srv.EnableMainMememoryTables",
    "Srv.AdaptiveRowsPerMessage",
    "Srv.ExecRowsPerMessage",
    "Srv.ForceThreadsToSystemScope",
    "Srv.LocalStartTasks",
    "Srv.LogAdminCommands",
    "Srv.LogAuditInfo",
    "Srv.MaxBgTaskInterval",
    "Srv.MaxConstraintLength",
    "Srv.MaxOpenCursors",
    "Srv.MaxReadThreadPoolSize",
    "Srv.MaxRpcDataLength",
    "Srv.MaxStartStatements",
    "Srv.MessageLogSize",
    "Srv.Monitor",
    "Srv.Name",
    "Srv.PessimisticTableUseNFetch",
    "Srv.PrintMsgCode",
    "Srv.ProcessBasePriority",
    "Srv.ProcessIdlePriority",
    "Srv.ProcessWorkingSetSize",
    "Srv.ReadThreadMode",
    "Srv.RemoteStartTasks",
    "Srv.ReportInterval",
    "Srv.MemoryReportLimit",
    "Srv.MemoryReportDelta",
    "Srv.MemorySizeReportInterval",
    "Srv.DatabaseSizeReportInterval",
    "Srv.StatementMemoryTraceLimit",
    "Srv.RowsPerMessage",
    "Srv.script_table",
    "Srv.SignalCatch",
    "Srv.SsDebug",
    "Srv.SyncExecRowsPerMessage",
    "Srv.Threads",
    "Srv.MaxActiveThreads",
    "Srv.TimeSlice",
    "Srv.TraceLevel",
    "Srv.TraceLogSize",
    "Srv.TraceSecDecimals",
    "Srv.TraceType",
    "Srv.TuplesPerMessage",
    "Srv.UnloadMaxLimit",
    "Srv.UserTaskQueue",
    "Srv.UseReadThread", /* deprecated */
    "Srv.NetBackupRootDir",
    "Srv.NetBackupEnabled",

    "Synchronizer.ConnectStrForMaster",
    "Synchronizer.MasterExecNoLogFLush",
    "Synchronizer.MasterMessageAutostart",
    "Synchronizer.MultiLevel",
    "Synchronizer.RpcEventThresholdBytecount",
    "Synchronizer.SubscribeDeleteFirstOpt",
    "Synchronizer.ConnectTimeout",
    "Synchronizer.MasterStatementCache",
    "Synchronizer.RefreshIsolationLevel",
    "Synchronizer.RefreshReadLevelRows",
    "Synchronizer.ReplicaRefreshLoad",

    "TransparentFailover.ReconnectTimeout",
    "TransparentFailover.WaitTimeout",
    0
};

/*
 * Here we add the parameters that have been replaced with another but the
 * functionality is exactly same.  These have to be on the registered list
 * too.
 *
 * structure:
 *   pairs of (old-parameter, new-official-parameter)
 *
 */

/* The version numbers concerning at which version the deprecation was
 * done are of "at least in this version, possibly earlier" type.
 */

su_regis_deprecated_t su_regis_simple_replaced_register[] = {

    /* replaced */
    { "HotStandby.PrimaryAlone", "HotStandby.AutoPrimaryAlone" },  /* 4.1 */
    { "Srv.UseReadThread",       "Srv.ReadThreadMode" },           /* 4.1 */

    { "SQL.ConvertOrsToUnions", "SQL.ConvertOrsToUnionsCount" }, /* 5.0, but
                                                                  * 3.0 or so
                                                                  * really.
                                                                  */
#ifdef FSYNC_OPT
    { "IndexFile.SynchronizedWrite", "IndexFile.SyncWrite" },   /* 6.0 */
    { "Logging.FileFlush", "Logging.ForceFileFlush" },   /* 6.0 */
#endif
    { 0, 0 }
};

/*
 * See previous comment on structure.
 *
 * If new-official-parameter != NULL (0) then the parameter has a replacement
 * that differs a lot so they don't/can't/won't coexist.
 *
 */

su_regis_deprecated_t su_regis_simple_discontinued_register[] = {
    /* discontinued */
    { "HotStandby.CatchupStepsToSkip", "HotStandby.CatchupSpeedRate" },/* 4.1*/
    { "HotStandby.Master",             0 },                         /* 4.1 */
    { "HotStandby.NodeId",             0 },                         /* 4.1 */
    { "HotStandby.StartupForceSlave",  0 },                         /* 4.1 */
    { "HotStandby.Timeout",            0 },                         /* 4.1 */
    { 0, 0 }
};

/*  EOF  */
