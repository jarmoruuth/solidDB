/*************************************************************************\
**  source       * dbe7cfg.c
**  directory    * dbe
**  description  * DBE portion of configuration info
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

This 'class' implements a view of the SOLID configuration file object that
is needed for the database engine. The dbe_cfg_t holds link to the original
universal configuration file object.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

The configuration file class dbe_cfgfile_t <su0cfgfi.h>

Preconditions:
-------------


Multithread considerations:
--------------------------

No automatic concurrency control is supplied, but the code is reentrant.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssenv.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssstring.h>
#include <sssprint.h>
#include <sslimits.h>
#include <ssfile.h>
#include <ssscan.h>

#include <ui0msg.h>

#include <su0param.h>
#include <su0cfgst.h>
#ifdef SS_LICENSEINFO_V3
#include <su0li3.h>
#else /* SS_LICENSEINFO_V3 */
#include <su0li2.h>
#endif /* SS_LICENSEINFO_V3 */
#include <su0svfil.h>

#include <dbe0db.h>
#include "dbe9type.h"
#include "dbe0type.h"
#include "dbe7cfg.h"
#include "dbe7logf.h"

#define DBE_DEFAULT_MAXOPENFILES        SS_MAXFILES_DEFAULT
#define DBE_MINIMUM_INDEXBLOCKSIZE      2048
#define DBE_DEFAULT_INDEXMAXSEQALLOC    4

#ifdef FSYNC_OPT
#define DBE_DEFAULT_LOGFILEFLUSH        FALSE
#define DBE_DEFAULT_LOGSYNCWRITE        TRUE
#else
#define DBE_DEFAULT_LOGFILEFLUSH        TRUE
#define DBE_DEFAULT_LOGSYNCWRITE        FALSE
#endif

#define DBE_DEFAULT_LOGMINSPLITSIZE     (10 * 1024L * 1024L)

#ifdef DBE_LAZYLOG_OPT
#define DBE_DEFAULT_LOGCOMMITMAXWAIT    0
#else
#define DBE_DEFAULT_LOGCOMMITMAXWAIT    5000 /* 5 seconds */
#endif

#define DBE_DEFAULT_LOGRELAXEDMAXDELAY  5000 /* 5 seconds */
#define DBE_MIN_LOGRELAXEDMAXDELAY       100 /* minimum accepted non zero value for RELAXEDMAXDELAY */

#define DBE_DEFAULT_LOGWRITEMODE        2   /* overwrite == 2 */
#define DBE_DEFAULT_LOGFNAMETEMPLATE    "sol#####.log"
#define DBE_DEFAULT_LOGDIGITTEMPLATE    '#'
#define DBE_DEFAULT_LOGDIGITTEMPLSTR    "#"
#define DBE_DEFAULT_LOGGROUPCOMMITQUEUE TRUE
#define DBE_DEFAULT_DELAYMESWAIT        TRUE
#ifdef SS_SMALLSYSTEM
#define DBE_DEFAULT_LOGMAXWRITEQUEUERECORDS     100
#define DBE_DEFAULT_LOGMAXWRITEQUEUEBYTES       2*1024
#define DBE_DEFAULT_LOGWRITEQUEUEFLUSHLIMIT     1024
#define DBE_DEFAULT_LOGWRITEBUFFERSIZE          (64*1024)
#define DBE_DEFAULT_LOGEXTENDINCR_BYTES         0
#else
#define DBE_DEFAULT_LOGMAXWRITEQUEUERECORDS     500000
#define DBE_DEFAULT_LOGMAXWRITEQUEUEBYTES       (4*1024*1024)
#define DBE_DEFAULT_LOGWRITEQUEUEFLUSHLIMIT     (64*1024)
#define DBE_DEFAULT_LOGWRITEBUFFERSIZE          (256*1024)
#define DBE_DEFAULT_LOGEXTENDINCR_BYTES         0               /* could be: 10 MB */
#endif
#define DBE_DEFAULT_INDEXFILENAME       "solid.db"

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
#if defined (SS_LINUX) || defined (SS_UNIX)
#define DBE_DEFAULT_BACKUPDIR           "/mybackup"
#endif
#if defined (SS_NT) || defined(WIN_NT) || defined(WIN32)
#define DBE_DEFAULT_BACKUPDIR           "\\mybackup"
#endif
#else
#define DBE_DEFAULT_BACKUPDIR           "backup"
#endif /* SS_MYSQL */

#define DBE_DEFAULT_LOGDIR              ""
#define DBE_DEFAULT_BACKUPCOPYLOG       TRUE
#define DBE_DEFAULT_BACKUP_STEPSTOSKIP  0
#define DBE_DEFAULT_BACKUPDELETELOG     TRUE
#define DBE_DEFAULT_BACKUPCOPYINIFILE   TRUE
#define DBE_DEFAULT_BACKUPCOPYSOLMSGOUT TRUE
#define DBE_DEFAULT_READONLY            FALSE
#define DBE_DEFAULT_DISABLEIDLEMERGE    FALSE
#define DBE_DEFAULT_CHECKESCALATELIMIT  1000L
#define DBE_DEFAULT_READESCALATELIMIT   500L
#define DBE_DEFAULT_LOCKESCALATELIMIT   1000L
#define DBE_DEFAULT_ALLOWLOCKBOUNCE     TRUE
#define DBE_DEFAULT_FREELISTRESERVERSIZE    100
#define DBE_DEFAULT_FREELISTGLOBALLYSORTED FALSE
#define DBE_DEFAULT_SEQSEALIMIT         500
#define DBE_DEFAULT_SEABUFLIMIT         50
#define DBE_DEFAULT_TRXBUFSIZE          4000
#define DBE_DEFAULT_TRXEARLYVALIDATE    TRUE
#define DBE_DEFAULT_USEIOTHREADS        FALSE
#define DBE_DEFAULT_TABLE_LOCK_TO       30L
#define DBE_DEFAULT_LOCKTIMEOUT         60L
#define DBE_DEFAULT_MMELOCKHASHSIZE     1000000
#define DBE_DEFAULT_MMELOCKESCALATION   FALSE
#define DBE_DEFAULT_MMELOCKESCALATIONLIMIT  1000
#ifdef SS_DEBUG
#define DBE_DEFAULT_MMERELEASEMEMORY    TRUE    /* Needed for memory debugging. */
#else /* SS_DEBUG */
#define DBE_DEFAULT_MMERELEASEMEMORY    FALSE
#endif /* SS_DEBUG */
#define DBE_DEFAULT_MMEMAXCACHEUSAGE    (8 * 1024 * 1024)
#ifndef DBE_DEFAULT_MMEMUTEXGRANULARITY /* So that you can override this in
                                           e.g. root.inc. */
#define DBE_DEFAULT_MMEMUTEXGRANULARITY 1
#endif
#define DBE_DEFAULT_RELBUFSIZE          1000
#define DBE_DEFAULT_PREFLUSHPERCENT     25
#define DBE_DEFAULT_PREFLUSHSAMPLESIZE  10
#define DBE_DEFAULT_PREFLUSHDIRTYPERCENT 50
#define DBE_DEFAULT_LASTUSELRUSKIPPERCENT  50
#define DBE_DEFAULT_CLEANPAGESEARCHLIMIT 100
#define DBE_DEFAULT_CPDELETELOG         FALSE
#define DBE_DEFAULT_DEFAULTSTOREISMEMORY    FALSE
/* DBE_DEFAULT_DURABILITYLEVEL is in sse/sse1conf.c too! */
#define DBE_DEFAULT_DURABILITYLEVEL         DBE_DURABILITY_ADAPTIVE
#define DBE_DEFAULT_SPLITPURGE          FALSE
#define DBE_DEFAULT_MERGECLEANUP        FALSE
#define DBE_DEFAULT_USENEWBTREELOCKING  FALSE
#define DBE_DEFAULT_USERANDOMKEYSAMPLEREAD TRUE
#define DBE_DEFAULT_SINGLEDELETEMARK    FALSE
#define DBE_DEFAULT_PHYSICALDROPTABLE   TRUE
#define DBE_DEFAULT_RELAXEDBTRTEELOCKING 0      /* bits: 1=nodepath, 2=nolock, 3=both */
#define DBE_DEFAULT_SPINCOUNT           50      /* only for pthreads */

#define DBE_DEFAULT_MMEMEMLIMIT_STR     "0"
#define DBE_DEFAULT_MMEMEMLIMIT         0
#define DBE_DEFAULT_MMEMEMLOWPERC       90
#define DBE_DEFAULT_USEPESSIMISTICGATE                TRUE
#define DBE_DEFAULT_VERSIONEDPESSIMISTICREADCOMMITTED TRUE
#define DBE_DEFAULT_VERSIONEDPESSIMISTICREPEATABLEREAD TRUE
#define DBE_DEFAULT_FASTDEADLOCKDETECT                 FALSE
#define DBE_DEFAULT_DEADLOCKDETECTMAXDEPTH             10
#define DBE_DEFAULT_DEADLOCKDETECTMAXLOCKS             100000


#define DBE_DEFAULT_SYNCHRONIZEDWRITE   TRUE
#ifdef FSYNC_OPT
#define DBE_DEFAULT_INDEXFILEFLUSH      TRUE
#endif

#ifdef IO_OPT
# ifdef SS_NT
#define DBE_DEFAULT_INDEXDIRECTIO       TRUE
# else
#define DBE_DEFAULT_INDEXDIRECTIO       FALSE
# endif
#endif
#define DBE_DEFAULT_LOGDIRECTIO         FALSE

#define DBE_DEFAULT_USENEWKEYCHECK      TRUE

#define CFG_MAX_NUMIOTHREADS            128

#define DBE_DEFAULT_MAXPAGESEM          0x7FFFFFFFL

#define DBE_MIN_RELBUFSIZE              5
#define DBE_MAX_TRXBUFSIZE              500000L

#if defined(SS_DEBUG)
#  define DBE_DEFAULT_QUICKMERGEINTERVAL    1000L
#elif defined(SS_SMALLSYSTEM)
#  define DBE_DEFAULT_QUICKMERGEINTERVAL    1000L
#elif defined(SS_UNIX) || defined(SS_NT)
#  define DBE_DEFAULT_QUICKMERGEINTERVAL    10000
#elif defined(SS_WIN) || defined(SS_DOS) 
#  define DBE_DEFAULT_QUICKMERGEINTERVAL    1000L
#else /* other */
#error Unknown environment!
#endif

#define DBE_DEFAULT_MERGEMINTIME            0

/* The following #defines are used to calculate the default
 * merge interval.
 */

/* Max percentage of cache blocks for Bonsai-tree. */
#define DBE_MAXBONSAIBLOCKSPERC 20

/* Number of key values per 1024 bytes of index block. */
#define DBE_KEYSPER1024B        (1024 / DBE_CFG_MERGEKEYLEN)

#define DBE_MINMERGEINTERVAL            1000L
#define DBE_MAXMERGEINTERVAL            10000000L
#define DBE_DEFAULT_MAXMERGEINTERVAL    100000L

bool dbefile_diskless = FALSE; /* for diskless, no physical dbfile */
bool dbelog_diskless = FALSE; /* for diskless, disable logging */
bool dbe_cfg_newkeycheck;
bool dbe_cfg_shortkeyopt;
bool dbe_cfg_newtrxwaitreadlevel;
bool dbe_cfg_relaxedreadlevel;
long dbe_cfg_readlevelmaxtime;
long dbe_cfg_ddoperrormaxwait;
bool dbe_cfg_usepessimisticgate;
bool dbe_cfg_versionedpessimisticreadcommitted;
bool dbe_cfg_versionedpessimisticrepeatableread;
int  dbe_cfg_maxmergeparts;
bool dbe_cfg_startupforcemerge;
bool dbe_cfg_fastdeadlockdetect;
bool dbe_cfg_deadlockdetectmaxdepth;
bool dbe_cfg_deadlockdetectmaxlocks;
bool dbe_cfg_splitpurge;
bool dbe_cfg_mergecleanup;
bool dbe_cfg_usenewbtreelocking;
bool dbe_cfg_userandomkeysampleread;
bool dbe_cfg_singledeletemark;
bool dbe_cfg_physicaldroptable;
int  dbe_cfg_relaxedbtreelocking;

ss_beta(bool dbe_filespecs_registered = FALSE;)

static long dbe_maxmergeinterval = 10000;

extern dbe_db_t* sqlsrv_db;

static su_ret_t conf_param_rwstartup_set_cb(
        char* default_value,
        char** default_value_loc,
        char* current_value,
        char** current_value_loc,
        char** factory_value_loc);

static su_ret_t dbe_cpinterval_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value,
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        char* p_mismatch;
        long l;

        if (h_db == NULL) { /* should not EVER happen */
            ss_derror;

            return SU_SUCCESS;
        }

        if (SsStrScanLong(current_value, &l, &p_mismatch)) {
            if (l >= 0) {
                dbe_db_setcpinterval(h_db, l);
                return SU_SUCCESS;
            } else {
                return SU_ERR_PARAM_VALUE_TOOSMALL;
            }
        }
        return SU_ERR_PARAM_VALUE_INVALID;
}

static su_ret_t dbe_locktimeout_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value,
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        char* p_mismatch;
        long l;

        if (h_db == NULL) { /* should not EVER happen */
            ss_derror;

            return SU_SUCCESS;
        }

        if (SsStrScanLong(current_value, &l, &p_mismatch)) {
            if (l >= 0) {
                dbe_db_setlocktimeout(h_db, &l, NULL);
                return SU_SUCCESS;
            } else {
                return SU_ERR_PARAM_VALUE_TOOSMALL;
            }
        }
        return SU_ERR_PARAM_VALUE_INVALID;
}

static su_ret_t dbe_tablelocktimeout_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value,
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        char* p_mismatch;
        long l;

        if (h_db == NULL) { /* should not EVER happen */
            ss_derror;

            return SU_SUCCESS;
        }

        if (SsStrScanLong(current_value, &l, &p_mismatch)) {
            if ((l >= 0) && (l < SU_MAXLOCKTIMEOUT) ) {
                dbe_db_settablelocktimeout(h_db, l);
                return SU_SUCCESS;
            }
        }

        return SU_ERR_PARAM_VALUE_INVALID;
}

static su_ret_t dbe_mergeinterval_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value,
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        char* p_mismatch;
        long l;

        if (h_db == NULL) { /* should not EVER happen */
            ss_derror;

            return SU_SUCCESS;
        }

        if (SsStrScanLong(current_value, &l, &p_mismatch)) {
            if ((l >= DBE_MINMERGEINTERVAL)
                && (l <= DBE_MAXMERGEINTERVAL)) {

                dbe_db_setmergeinterval(h_db, l);

                return SU_SUCCESS;
            }
        }
        return SU_ERR_PARAM_VALUE_INVALID;
}

static su_ret_t dbe_mergemintime_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value,
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        char* p_mismatch;
        long l;

        if (h_db == NULL) { /* should not EVER happen */
            ss_derror;

            return SU_SUCCESS;
        }

        if (SsStrScanLong(current_value, &l, &p_mismatch)) {
            if (l >= 0) {
                dbe_db_setmergemintime(h_db, l);

                return SU_SUCCESS;
            }
        }
        return SU_ERR_PARAM_VALUE_INVALID;
}

static su_ret_t dbe_cpmintime_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value,
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        char* p_mismatch;
        long l;

        if (h_db == NULL) { /* should not EVER happen */
            ss_derror;

            return SU_SUCCESS;
        }

        if (SsStrScanLong(current_value, &l, &p_mismatch)) {
            if (l >= 0) {
                dbe_db_setcpmintime(h_db, l);

                return SU_SUCCESS;
            }
        }
        return SU_ERR_PARAM_VALUE_INVALID;
}

#ifndef SS_MYSQL
static su_ret_t dbe_relaxedmaxdelay_set_cb(
        char* default_value,
        char** default_value_loc,
        char* current_value,
        char** current_value_loc,
        char** factory_value_loc)
{
        char* p_mismatch;
        long l;

        if (h_db == NULL) { /* should not EVER happen */
            ss_derror;

            return SU_SUCCESS;
        }

        if (SsStrScanLong(current_value, &l, &p_mismatch)) {
            if (l >= 0) {
#if 0 /* FIXME: not implemented */
                /* we might break layering rules: */

                dbe_log_t* log = dbe_db_getlog(h_db);

                if (log != NULL) {
                    ;
                }
#endif
                return SU_SUCCESS;
            }
        }
        return SU_ERR_PARAM_VALUE_INVALID;
}
#endif /* !SS_MYSQL */

static su_ret_t dbe_defaultstoreismemory_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value,
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        bool b = FALSE;

        if (h_db == NULL) { /* should not EVER happen */
            ss_derror;

            return SU_SUCCESS;
        }

        if (SsStrScanYesNo(current_value, &b, NULL)) {

            dbe_db_setdefaultstoreismemory(h_db, b);

            return SU_SUCCESS;
        }

        return SU_ERR_PARAM_VALUE_INVALID;
}

static ulong cfg_defaultmergeinterval(dbe_cfg_t* dbe_cfg);
void dbe_cfg_registerfilespecs(dbe_cfg_t* dbe_cfg);

static su_initparam_t dbe_parameters[] = {

/* General Section */

    {
        SU_DBE_GENERALSECTION, SU_DBE_MAXOPENFILES,
        NULL, DBE_DEFAULT_MAXOPENFILES, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Maximum number of files kept concurrently open"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_BACKUPDIR,
        DBE_DEFAULT_BACKUPDIR, 0, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_STR, SU_PARAM_AM_RWSTARTUP,
        "The directory for backup files",
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_BACKUP_BLOCKSIZE,
        NULL, DBE_DEFAULT_BACKUP_BLOCKSIZE, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Netcopy and backup blocksize",
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_BACKUP_STEPSTOSKIP,
        NULL, DBE_DEFAULT_BACKUP_STEPSTOSKIP, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Controls how frequently netcopy and backup tasks are executed",
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_BACKUPCOPYLOG,
        NULL, 0, 0.0, DBE_DEFAULT_BACKUPCOPYLOG,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, backup operation will copy log files to the backup directory"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_NETBACKUPCOPYLOG,
        NULL, 0, 0.0, DBE_DEFAULT_BACKUPCOPYLOG,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, netbackup operation will copy log files to the backup destination"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_BACKUPDELETELOG,
        NULL, 0, 0.0, DBE_DEFAULT_BACKUPDELETELOG,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, old log files will be deleted after backup operation"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_NETBACKUPDELETELOG,
        NULL, 0, 0.0, DBE_DEFAULT_BACKUPDELETELOG,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, old log files will be deleted after netbackup operation"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_BACKUPCOPYINIFLE,
        NULL, 0, 0.0, DBE_DEFAULT_BACKUPCOPYINIFILE,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, solid.ini file will be copied to the backup directory"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_NETBACKUPCOPYINIFLE,
        NULL, 0, 0.0, DBE_DEFAULT_BACKUPCOPYINIFILE,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, solid.ini file will be copied to the netbackup destination"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_BACKUPCOPYSOLMSGOUT,
        NULL, 0, 0.0, DBE_DEFAULT_BACKUPCOPYSOLMSGOUT,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, solmsg.out file will be copied to the backup directory"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_NETBACKUPCOPYSOLMSGOUT,
        NULL, 0, 0.0, DBE_DEFAULT_BACKUPCOPYSOLMSGOUT,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, solmsg.out file will be copied to the netbackup destination"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_CPINTERVAL,
        NULL, DBE_DEFAULT_CPINTERVAL, 0.0, 0,
        dbe_cpinterval_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RW,
        "Number of inserts that causes automatic checkpoint creation"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_READONLY,
        NULL, 0, 0.0, DBE_DEFAULT_READONLY,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, database is set to read-only mode"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_DISABLEIDLEMERGE,
        NULL, 0, 0.0, DBE_DEFAULT_DISABLEIDLEMERGE,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, database is set to disable idlemerge"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_SEQSEALIMIT,
        NULL, DBE_DEFAULT_SEQSEALIMIT, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "The limit after which search is treated as a long sequential search"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_SEABUFLIMIT,
        NULL, DBE_DEFAULT_SEABUFLIMIT, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Max. percentage of search buffers from total buffered memory reserved for open cursors"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_TRXBUFSIZE,
        NULL, DBE_DEFAULT_TRXBUFSIZE, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "The hash table size for incomplete transactions"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_PESSIMISTIC,
        NULL, 0, 0.0, DBE_DEFAULT_PESSIMISTIC,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, pessimistic concurrency control mode is used by default"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_TABLE_LOCK_TO,
        NULL, DBE_DEFAULT_TABLE_LOCK_TO, 0.0, 0,
        dbe_tablelocktimeout_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RW,
        "Table lock wait timeout in seconds"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_NUMIOTHREADS,
        NULL, DBE_DEFAULT_NUMIOTHREADS, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Number of I/O-threads per device"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_NUMWRITERIOTHREADS,
        NULL, DBE_DEFAULT_NUMWRITERIOTHREADS, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Number of writer I/O-threads per device"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_DEFAULTSTOREISMEMORY,
        NULL, 0, 0.0, DBE_DEFAULT_DEFAULTSTOREISMEMORY,
        dbe_defaultstoreismemory_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RW,
        "Sets the default table storage mode"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_CPDELETELOG,
        NULL, 0, 0.0, DBE_DEFAULT_CPDELETELOG,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Delete transaction log file(s) after every successful checkpoint"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_WRITEFLUSHMODE,
        NULL, DBE_DEFAULT_WRITEFLUSHMODE, 0.0, 0,
        NULL, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Controls flushing of files before reads or after writes"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_PESSIMISTIC_LOCK_TO,
        NULL, DBE_DEFAULT_PESSIMISTIC_LOCK_TO, 0.0, 0,
        dbe_locktimeout_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RW,
        "Specifies the time the engine waits for a lock to be released"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_CPMINTIME,
        NULL, DBE_DEFAULT_CPMINTIME, 0.0, 0,
        dbe_cpmintime_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RW,
        "Minimum time between checkpoints"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_MERGEMINTIME,
        NULL, DBE_DEFAULT_MERGEMINTIME, 0.0, 0,
        dbe_mergemintime_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RW,
        "Minimum time between two merge operations"
    },
    {
        SU_DBE_GENERALSECTION, SU_DBE_VERSIONEDPESSIMISTICREADCOMMITTED,
        NULL, 0, 0.0, DBE_DEFAULT_VERSIONEDPESSIMISTICREADCOMMITTED,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Make pessimistic D-tables use versioned reads with READ COMMITTED isolation"
    },

/* IndexFile Section */

    {
        SU_DBE_INDEXSECTION, SU_DBE_BLOCKSIZE,
        NULL, DBE_DEFAULT_INDEXBLOCKSIZE, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RONLY,
        "Block size of the index file in bytes"
    },
    {
        SU_DBE_INDEXSECTION, SU_DBE_CACHESIZE,
        SS_STRINGIZE_EXPAND_ARG(DBE_DEFAULT_INDEXCACHESIZE),
        0, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_INT8, SU_PARAM_AM_RWSTARTUP,
        "The size of database cache memory in bytes"
    },
    {
        SU_DBE_INDEXSECTION, SU_DBE_EXTENDINCR,
        NULL, DBE_DEFAULT_INDEXEXTENDINCR, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Number of blocks allocated at one time when new space is allocated for database file"
    },
    {
        SU_DBE_INDEXSECTION, SU_DBE_READAHEADSIZE,
        NULL, DBE_DEFAULT_READAHEADSIZE, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Number of prefetch index leafs during long sequential searches"
    },
    {
        SU_DBE_INDEXSECTION, SU_DBE_PREFLUSHPERCENT,
        NULL, DBE_DEFAULT_PREFLUSHPERCENT, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Percentage of page buffer which is kept clean by preflush thread"
    },
    {
        SU_DBE_INDEXSECTION, SU_DBE_USENEWKEYCHECK,
        NULL, 0, 0.0, DBE_DEFAULT_USENEWKEYCHECK,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP|SU_PARAM_AM_UNPUBLISHED,
        "If set to No, the server will be backwards compatible with the non-ANSI-compliant behavior of some older server versions regarding REPEATABLE READ isolation level"
    },
#ifdef FSYNC_OPT
    {
        SU_DBE_INDEXSECTION, SU_DBE_SYNCWRITE,
        NULL, 0, 0.0, DBE_DEFAULT_SYNCHRONIZEDWRITE,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Defines if the index file uses synchronized file IO"
    },
    {
        SU_DBE_INDEXSECTION, SU_DBE_FFILEFLUSH,
        NULL, 0, 0.0, DBE_DEFAULT_INDEXFILEFLUSH,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Forces the use of file flush regardless of other IO settings"
    },
#endif
    {
        SU_DBE_INDEXSECTION, SU_DBE_SYNCHRONIZEDWRITE,
        NULL, 0, 0.0, DBE_DEFAULT_SYNCHRONIZEDWRITE,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Defines if the index file uses synchronized file IO"
    },

#ifdef IO_OPT
    {
        SU_DBE_INDEXSECTION, SU_DBE_DIRECTIO,
        NULL, 0, 0.0, DBE_DEFAULT_INDEXDIRECTIO,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Defines if the index file uses Direct I/O"
    },
#endif
/* Logging Section */

    {
        SU_DBE_LOGSECTION, SU_DBE_LOGENABLED,
        NULL, 0, 0.0, DBE_DEFAULT_LOGENABLED,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Enable logging"
    },
    {
        SU_DBE_LOGSECTION, SU_DBE_BLOCKSIZE,
        NULL, DBE_DEFAULT_LOGBLOCKSIZE, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Default log file block size in bytes"
    },
    {
        SU_DBE_LOGSECTION, SU_DBE_LOGMINSPLITSIZE,
        NULL, DBE_DEFAULT_LOGMINSPLITSIZE, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Minimum size after which logging continues to the next log file"
    },
    {
        SU_DBE_LOGSECTION, SU_DBE_LOGFILETEMPLATE,
        DBE_DEFAULT_LOGFNAMETEMPLATE, 0, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_STR, SU_PARAM_AM_RWSTARTUP,
        "Log file path naming convention"
    },
    {
        SU_DBE_LOGSECTION, SU_DBE_LOGDIR,
        DBE_DEFAULT_LOGDIR, 0, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_STR, SU_PARAM_AM_RWSTARTUP,
        "Log file directory"
    },
    {
        SU_DBE_LOGSECTION, SU_DBE_LOGDIGITTEMPLATE,
        DBE_DEFAULT_LOGDIGITTEMPLSTR, 0, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_STR, SU_PARAM_AM_RWSTARTUP,
        "A template character used in log file name template"
    },

    /* SU_DBE_DURABILITYLEVEL moved to sse/sse1conf.c because of linkage */

    {
        SU_DBE_LOGSECTION, SU_DBE_LOGWRITEMODE,
        NULL, DBE_DEFAULT_LOGWRITEMODE, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Default transaction log write mode"
    },
    {
        SU_DBE_LOGSECTION, SU_DBE_LOGCOMMITMAXWAIT,
        NULL, DBE_DEFAULT_LOGCOMMITMAXWAIT, 0.0, 0,
        NULL, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP|SU_PARAM_AM_UNPUBLISHED,
        "Time in milliseconds the server waits before flushing the transaction log"
    },
#ifdef FSYNC_OPT
    {
        SU_DBE_LOGSECTION, SU_DBE_FFILEFLUSH,
        NULL, 0, 0.0, DBE_DEFAULT_LOGFILEFLUSH,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Forces the use of file flush regardless of other IO settings"
    },
#endif
    {
        SU_DBE_LOGSECTION, SU_DBE_FILEFLUSH,
        NULL, 0, 0.0, DBE_DEFAULT_LOGFILEFLUSH,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Forces the use of file flush regardless of other IO settings"
    },
    {
        SU_DBE_LOGSECTION, SU_DBE_LOGRELAXEDMAXDELAY,
        NULL, DBE_DEFAULT_LOGRELAXEDMAXDELAY, 0.0, 0,
        NULL, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Maximum time (in ms) the server waits before writing committed transactions to the log.  Applies only when durability level is RELAXED"
    },
    {
        SU_DBE_LOGSECTION, SU_DBE_SYNCWRITE,
        NULL, 0, 0.0, DBE_DEFAULT_LOGSYNCWRITE,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Platform supports Synchronized I/O Data Integrity Completion"
    },
#ifdef IO_OPT
    {
        SU_DBE_LOGSECTION, SU_DBE_DIRECTIO,
        NULL, 0, 0.0, DBE_DEFAULT_LOGDIRECTIO,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Defines if the log file uses Direct I/O"
    },
#endif

/* MME Section */

    {
        SU_MME_SECTION, SU_MME_LOCKHASHSIZE,
        NULL, DBE_DEFAULT_MMELOCKHASHSIZE, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Default lock table hash size for M-tables"
    },
    {
        SU_MME_SECTION, SU_MME_LOCKESCALATION,
        NULL, 0, 0.0, DBE_DEFAULT_MMELOCKESCALATION,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Default value for if lock escalation is enabled for M-tables"
    },
    {
        SU_MME_SECTION, SU_MME_LOCKESCALATIONLIMIT,
        NULL, DBE_DEFAULT_MMELOCKESCALATIONLIMIT, 0.0, 0,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Default lock escalation limit for M-tables"
    },
    {
        SU_MME_SECTION, SU_MME_RELEASEMEMORY,
        NULL, 0, 0.0, DBE_DEFAULT_MMERELEASEMEMORY,
        conf_param_rwstartup_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "Enable releasing of M-table memory at shutdown"
    },
    {
        SU_MME_SECTION, SU_MME_LOWWATERMARKPERC,
        NULL, DBE_DEFAULT_MMEMEMLOWPERC, 0.0, 0,
        NULL, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Percentage of memory you may use of ImbdMemoryLimit (if set) before server starts limiting inserts"
    },
    {
        SU_MME_SECTION, SU_MME_MAXCACHEUSAGE,
        NULL, DBE_DEFAULT_MMEMAXCACHEUSAGE, 0.0, 0,
        NULL, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Maximum amount of cache used for M-table checkpointing"
    },
    {
        SU_MME_SECTION, SU_MME_MUTEXGRANULARITY,
        NULL, DBE_DEFAULT_MMEMUTEXGRANULARITY, 0.0, 0,
        NULL, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP
#ifdef MMEG2_MUTEXING
        /* hidden with MMEg2 mutexing, pending proper removal */
        | SU_PARAM_AM_UNPUBLISHED
#endif
        ,
        "MME mutexing granularity"
    },

/* Terminator */

    {
        NULL, NULL, NULL, 0, 0.0, 0, NULL, NULL, 0, 0, NULL
    }
};

/*
 * We have to handle SU_DBE_MERGEINTERVAL separately because
 * factory_default of this parameter is calculated on-the-fly.
 */
static su_initparam_t dbe_specialparam[] = {
    {
        SU_DBE_GENERALSECTION, SU_DBE_MERGEINTERVAL,
        NULL, 0, 0.0, 0,
        dbe_mergeinterval_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RW,
        "Number of index inserts that causes the merge process to start"
    },
    {
        NULL, NULL, NULL, 0, 0.0, 0, NULL, NULL, 0, 0, NULL
    }
};

struct dbe_cfg_st {
        bool          cfg_reentrant;
        su_inifile_t* cfg_file;
        size_t        cfg_tmpidxblocksize;
};

struct dbe_filespec_st {
        char* fs_name;
        ss_int8_t fs_maxsize;
        uint  fs_diskno;
};

bool dbe_estrndnodesp = FALSE;
bool dbe_estrndkeysp = FALSE;

static void dbe_cfg_ensurerndsampleflags(
        dbe_cfg_t* dbe_cfg)
{
        bool found;

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    "EstSampleRndNodes",
                    &dbe_estrndnodesp);
        if (!found) {
            dbe_estrndnodesp = FALSE;
        }

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    "EstSampleRndKeys",
                    &dbe_estrndkeysp);
        if (!found) {
            dbe_estrndkeysp = FALSE;
        }
}


/*##**********************************************************************\
 *
 *              dbe_filespec_init
 *
 * Creates a database physical file spec.
 *
 * Parameters :
 *
 *      name - in, use
 *              The file path name (preferably absolute)
 *
 *      maxsize - in
 *          maximum number of bytes that may be stored in this physical
 *          file. The number is actually truncated to nearest multiple of
 *          database file block size.
 *
 *      diskno - in
 *          physical disk number of file. Note different
 *          partition on same disk should have same number
 *
 * Return value - give :
 *      pointer to the created filespec
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_filespec_t* dbe_filespec_init(
        char *name,
        ss_int8_t maxsize,
        uint diskno)
{
        dbe_filespec_t* filespec = SSMEM_NEW(dbe_filespec_t);

        filespec->fs_name = SsMemStrdup(name);
        filespec->fs_maxsize = maxsize;
        filespec->fs_diskno = diskno;
        return (filespec);
}

/*##**********************************************************************\
 *
 *              dbe_filespec_done
 *
 * Deletes a filespec object.
 *
 * Parameters :
 *
 *      filespec - in, take
 *              pointer to the filespec object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_filespec_done(
        dbe_filespec_t* filespec)
{
        ss_dassert(filespec != NULL);
        ss_dassert(filespec->fs_name != NULL);
        SsMemFree(filespec->fs_name);
        SsMemFree(filespec);
}

/*##**********************************************************************\
 *
 *              dbe_filespec_getname
 *
 * Gets name of the file from filespec
 *
 * Parameters :
 *
 *      filespec - in, use
 *              pointer to filespec object
 *
 * Return value - ref :
 *      name of the file
 *
 * Limitations  :
 *
 * Globals used :
 */
char* dbe_filespec_getname(
        dbe_filespec_t* filespec)
{
        ss_dassert(filespec != NULL);
        ss_dassert(filespec->fs_name != NULL);
        return (filespec->fs_name);
}

/*##**********************************************************************\
 *
 *              dbe_filespec_getmaxsize
 *
 * Gets max. size of the filespec
 *
 * Parameters :
 *
 *      filespec - in, use
 *              pointer to filespec object
 *
 * Return value :
 *      max number of bytes that may be stored in that file
 *
 * Limitations  :
 *
 * Globals used :
 */
ss_int8_t dbe_filespec_getmaxsize(
        dbe_filespec_t* filespec)
{
        ss_dassert(filespec != NULL);
        return (filespec->fs_maxsize);
}

/*##**********************************************************************\
 *
 *              dbe_filespec_getdiskno
 *
 *
 *
 * Parameters :
 *
 *      filespec -
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
uint dbe_filespec_getdiskno(
        dbe_filespec_t* filespec)
{
        ss_dassert(filespec != NULL);
        return (filespec->fs_diskno);
}

static su_ret_t conf_param_rwstartup_set_cb(
        char* default_value __attribute__ ((unused)),
        char** default_value_loc __attribute__ ((unused)),
        char* current_value __attribute__ ((unused)),
        char** current_value_loc __attribute__ ((unused)),
        char** factory_value_loc __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

void dbe_cfg_register_su_params(dbe_cfg_t* dbe_cfg)
{
        bool b;
        int i;

        b = su_param_register_array(dbe_parameters);
        ss_dassert(b);

        /*
         * We have to handle SU_DBE_MERGEINTERVAL separately because
         * factory_default of this parameter is calculated on-the-fly.
         */
        i = 0;
        while (dbe_specialparam[i].p_section != NULL) {
            if (SsStrcmp(dbe_specialparam[i].p_keyname, SU_DBE_MERGEINTERVAL) == 0) {
                dbe_specialparam[i].p_defaultlong = cfg_defaultmergeinterval(dbe_cfg);
                break;
            }
            i++;
        }
        ss_dassert(SsStrcmp(dbe_specialparam[i].p_keyname, SU_DBE_MERGEINTERVAL) == 0);
        b = su_param_register_array(dbe_specialparam);
        dbe_cfg_registerfilespecs(dbe_cfg);
}

void dbe_cfg_register_mme_memlimit(
        dbe_cfg_t* cfg __attribute__ ((unused)),
        su_param_set_cb_t change_callback)
{
        ss_dassert(cfg != NULL);
        su_param_register(
                (char *)SU_MME_SECTION,
                (char *)SU_MME_MEMORYLIMIT,
                NULL,
                NULL,
                (char *)DBE_DEFAULT_MMEMEMLIMIT_STR,
                (char *)"Maximum amount of memory allocated for M-tables",
                change_callback,
                NULL,
                SU_PARAM_TYPE_INT8,
                SU_PARAM_AM_RW);
}


/*#***********************************************************************\
 *
 *              cfg_init
 *
 * Creates a dbe_cfg_t object
 *
 * Parameters :
 *
 *      cfg_file - in, hold
 *              configuration file object
 *
 *      reentrant - in
 *          TRUE - use only inifile given, no su_param_XXX services
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_cfg_t* cfg_init(
        su_inifile_t* cfg_file,
        bool reentrant)
{
        bool foundp;
        long l;

        dbe_cfg_t* cfg = SSMEM_NEW(dbe_cfg_t);

        cfg->cfg_file = cfg_file;
        cfg->cfg_reentrant = reentrant;
        cfg->cfg_tmpidxblocksize = 0;
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_USENEWKEYCHECK,
                    &dbe_cfg_newkeycheck);
        if (!foundp) {
            dbe_cfg_newkeycheck = DBE_DEFAULT_USENEWKEYCHECK;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_USESHORTKEYOPT,
                    &dbe_cfg_shortkeyopt);
        if (!foundp) {
            dbe_cfg_shortkeyopt = TRUE;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_USENEWTRANSWAITREADLEVEL,
                    &dbe_cfg_newtrxwaitreadlevel);
        if (!foundp) {
            dbe_cfg_newtrxwaitreadlevel = TRUE;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_USERELAXEDREADLEVEL,
                    &dbe_cfg_relaxedreadlevel);
        if (!foundp) {
            dbe_cfg_relaxedreadlevel = TRUE;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_USEPESSIMISTICGATE,
                    &dbe_cfg_usepessimisticgate);
        if (!foundp) {
            dbe_cfg_usepessimisticgate=
                DBE_DEFAULT_USEPESSIMISTICGATE;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_VERSIONEDPESSIMISTICREADCOMMITTED,
                    &dbe_cfg_versionedpessimisticreadcommitted);
        if (!foundp) {
            dbe_cfg_versionedpessimisticreadcommitted =
                DBE_DEFAULT_VERSIONEDPESSIMISTICREADCOMMITTED;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_VERSIONEDPESSIMISTICREPEATABLEREAD,
                    &dbe_cfg_versionedpessimisticrepeatableread);
        if (!foundp) {
            dbe_cfg_versionedpessimisticrepeatableread =
                DBE_DEFAULT_VERSIONEDPESSIMISTICREPEATABLEREAD;
        }
        if (dbe_cfg_versionedpessimisticrepeatableread) {
            /* Force also read committed to use versioning. */
            dbe_cfg_versionedpessimisticreadcommitted = TRUE;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_FASTDEADLOCKDETECT,
                    &dbe_cfg_fastdeadlockdetect);
        if (!foundp) {
            dbe_cfg_fastdeadlockdetect = DBE_DEFAULT_FASTDEADLOCKDETECT;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_DEADLOCKDETECTMAXDEPTH,
                    &dbe_cfg_deadlockdetectmaxdepth);
        if (!foundp) {
            dbe_cfg_deadlockdetectmaxdepth = DBE_DEFAULT_DEADLOCKDETECTMAXDEPTH;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_DEADLOCKDETECTMAXLOCKS,
                    &dbe_cfg_deadlockdetectmaxlocks);
        if (!foundp) {
            dbe_cfg_deadlockdetectmaxlocks = DBE_DEFAULT_DEADLOCKDETECTMAXLOCKS;
        }

        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_SPLITPURGE,
                    &dbe_cfg_splitpurge);
        if (!foundp) {
            dbe_cfg_splitpurge = DBE_DEFAULT_SPLITPURGE;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_MERGECLEANUP,
                    &dbe_cfg_mergecleanup);
        if (!foundp) {
            dbe_cfg_mergecleanup = DBE_DEFAULT_MERGECLEANUP;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_USENEWBTREELOCKING,
                    &dbe_cfg_usenewbtreelocking);
        if (!foundp) {
            dbe_cfg_usenewbtreelocking = DBE_DEFAULT_USENEWBTREELOCKING;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_USERANDOMKEYSAMPLEREAD,
                    &dbe_cfg_userandomkeysampleread);
        if (!foundp) {
            dbe_cfg_userandomkeysampleread = DBE_DEFAULT_USERANDOMKEYSAMPLEREAD;
        }

        foundp = su_inifile_getlong(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_MAXMERGEPARTS,
                    &l);
        if (!foundp) {
            if (dbe_cfg_mergecleanup) {
                dbe_cfg_maxmergeparts = 1;
            } else {
                dbe_cfg_maxmergeparts = 100;
            }
        } else {
            dbe_cfg_maxmergeparts = (int)l;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_STARTUPFORCEMERGE,
                    &dbe_cfg_startupforcemerge);
        if (!foundp) {
            dbe_cfg_startupforcemerge = FALSE;
        }
        if (!dbe_cfg_relaxedreadlevel) {
            dbe_cfg_readlevelmaxtime = 0;
        } else {
            foundp = su_inifile_getlong(
                        cfg_file,
                        SU_DBE_GENERALSECTION,
                        SU_DBE_READLEVELMAXTIME,
                        &dbe_cfg_readlevelmaxtime);
            if (!foundp) {
                dbe_cfg_readlevelmaxtime = 10;
            }
        }
        foundp = su_inifile_getlong(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_DDOPERRORMAXWAIT,
                    &dbe_cfg_ddoperrormaxwait);
        if (!foundp) {
            dbe_cfg_ddoperrormaxwait = 0;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_SINGLEDELETEMARK,
                    &dbe_cfg_singledeletemark);
        if (!foundp) {
            dbe_cfg_singledeletemark = DBE_DEFAULT_SINGLEDELETEMARK;
        }
        foundp = su_inifile_getbool(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_PHYSICALDROPTABLE,
                    &dbe_cfg_physicaldroptable);
        if (!foundp) {
            dbe_cfg_physicaldroptable = DBE_DEFAULT_PHYSICALDROPTABLE;
        }
        foundp = su_inifile_getlong(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_RELAXEDBTRTEELOCKING,
                    &l);
        if (!foundp) {
            dbe_cfg_relaxedbtreelocking = DBE_DEFAULT_RELAXEDBTRTEELOCKING;
        } else {
            dbe_cfg_relaxedbtreelocking = (int)l;
        }

        foundp = su_inifile_getlong(
                    cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_SPINCOUNT,
                    &l);
        if (foundp) {
            ss_sem_spincount = (int)l;
        } else {
            ss_sem_spincount = DBE_DEFAULT_SPINCOUNT;
        }

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)
        su_inifile_getint(cfg_file, "Special", "ss_semsleep_startnum", &ss_semsleep_startnum);
        su_inifile_getint(cfg_file, "Special", "ss_semsleep_stopnum", &ss_semsleep_stopnum);
        su_inifile_getint(cfg_file, "Special", "ss_semsleep_mintime", &ss_semsleep_mintime);
        su_inifile_getint(cfg_file, "Special", "ss_semsleep_maxtime", &ss_semsleep_maxtime);
        su_inifile_getint(cfg_file, "Special", "ss_semsleep_random_freq", &ss_semsleep_random_freq);
        su_inifile_getint(cfg_file, "Special", "ss_semsleep_loopcount", &ss_semsleep_loopcount);
        su_inifile_getint(cfg_file, "Special", "ss_semsleep_maxloopcount", &ss_semsleep_maxloopcount);
        su_inifile_getint(cfg_file, "Special", "ss_semsleep_loopsem", &ss_semsleep_loopsem);
        su_inifile_getbool(cfg_file, "Special", "ss_semsleep_random", &ss_semsleep_random);
#endif /* defined(SS_DEBUG) || defined(AUTOTEST_RUN) */

        return (cfg);
}

dbe_cfg_t* dbe_cfg_init(
        su_inifile_t* cfg_file)
{
#ifdef SS_MYSQL
        return (cfg_init(cfg_file, TRUE));
#else
        return (cfg_init(cfg_file, FALSE));
#endif
}

dbe_cfg_t* dbe_cfg_init_reentrant(
        su_inifile_t* cfg_file)
{
        return (cfg_init(cfg_file, TRUE));
}

static bool cfg_getbool(
        dbe_cfg_t* dbe_cfg,
        const char* section,
        const char* keyname,
        bool* p_bool)
{
        bool found;

        if (dbe_cfg->cfg_reentrant) {
            found = su_inifile_getbool(dbe_cfg->cfg_file,
                                     section, keyname,
                                     p_bool);
        } else {
            found = su_param_getbool(dbe_cfg->cfg_file,
                                     section, keyname,
                                     p_bool);
        }
        return (found);
 }

static bool cfg_getlong(
        dbe_cfg_t* dbe_cfg,
        const char* section,
        const char* keyname,
        long* p_long)
{
        bool found;

        if (dbe_cfg->cfg_reentrant) {
            found = su_inifile_getlong(dbe_cfg->cfg_file,
                                     section, keyname,
                                     p_long);
        } else {
            found = su_param_getlong(dbe_cfg->cfg_file,
                                     section, keyname,
                                     p_long);
        }
        return (found);
}

static bool cfg_getint8(
        dbe_cfg_t* dbe_cfg,
        const char* section,
        const char* keyname,
        ss_int8_t* p_i8)
{
        bool found;

        if (dbe_cfg->cfg_reentrant) {
            found = su_inifile_getint8(dbe_cfg->cfg_file,
                                     section, keyname,
                                     p_i8);
        } else {
            found = su_param_getint8(dbe_cfg->cfg_file,
                                     section, keyname,
                                     p_i8);
        }
        return (found);
}

static bool cfg_getvalue(
        dbe_cfg_t* dbe_cfg,
        const char* section,
        const char* keyname,
        char** p_value)
{
        bool found;

        if (dbe_cfg->cfg_reentrant) {
            found = su_inifile_getstring(dbe_cfg->cfg_file,
                                         section, keyname,
                                         p_value);
        } else {
            found = su_param_getvalue(dbe_cfg->cfg_file,
                                      section, keyname,
                                      p_value);
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_done
 *
 * Deletes a dbe_cfg_t object
 *
 * Parameters :
 *
 *      dbe_cfg - in, take
 *              pointer to the object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_cfg_done(
        dbe_cfg_t* dbe_cfg)
{
        ss_dassert(dbe_cfg != NULL);
        SsMemFree(dbe_cfg);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getinifile
 *
 * Gives reference to inifile
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              pointer to the cfg object
 *
 * Return value - ref :
 *      pointer to inifile object
 *
 * Limitations  :
 *
 * Globals used :
 */
su_inifile_t* dbe_cfg_getinifile(dbe_cfg_t* dbe_cfg)
{
        return (dbe_cfg->cfg_file);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getmaxopenfiles
 *
 * Gets configured max. open files info
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              config. object
 *
 *      p_maxopen - out, use
 *              pointer to variable where the max. number of open files is stored.
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getmaxopenfiles(
        dbe_cfg_t* dbe_cfg,
        uint* p_maxopen)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_maxopen != NULL);

        /* JarmoP 310399 */
        dbe_cfg_ensurerndsampleflags(dbe_cfg);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_MAXOPENFILES,
                    &l);

        if (found) {
            *p_maxopen = (uint)l;
        } else {
            *p_maxopen = DBE_DEFAULT_MAXOPENFILES;
        }
        return (found);
}

/*#***********************************************************************\
 *
 *              cfg_checkblocksize
 *
 *
 *
 * Parameters :
 *
 *      section -
 *
 *
 *      parameter -
 *
 *
 *      value -
 *
 *
 *      give_warning -
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
static bool cfg_checkblocksize(
        const char* section,
        const char* parameter,
        size_t cfgvalue,
        size_t defaultvalue,
        size_t minvalue,
        bool give_warning)
{
        if ((cfgvalue & (cfgvalue - 1)) != 0 || cfgvalue < minvalue) {
            if (give_warning) {
                ui_msg_warning(INI_MSG_VALUE_NOT_MULTIPLE_OF_512_USSU,
                    cfgvalue, section, parameter, defaultvalue);
            }
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *      dbe_cfg_settmpidxblocksize
 *
 * Sets temporary effective value for index block size.
 * That value is never saved to .ini-file
 *
 * Parameters:
 *      dbe_cfg - in out, use
 *          configuration object
 *
 *      blocksize - in
 *          New (temporary) value for blocksize
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_cfg_settmpidxblocksize(dbe_cfg_t* dbe_cfg, size_t blocksize)
{
        ss_dassert(dbe_cfg != NULL);

        dbe_cfg->cfg_tmpidxblocksize = blocksize;
}
/*#***********************************************************************\
 *
 *              cfg_getidxblocksize
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_blocksize -
 *
 *
 *      give_warning -
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
static bool cfg_getidxblocksize(
        dbe_cfg_t* dbe_cfg,
        size_t* p_blocksize,
        bool give_warning)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_blocksize != NULL);

        if (dbe_cfg->cfg_tmpidxblocksize != 0) {
            found = TRUE;
            l = dbe_cfg->cfg_tmpidxblocksize;
        } else {
            found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_BLOCKSIZE,
                    &l);
        }
        if (found) {
            bool size_ok;
            size_ok = cfg_checkblocksize(
                        SU_DBE_INDEXSECTION,
                        SU_DBE_BLOCKSIZE,
                        (size_t)l,
                        DBE_DEFAULT_INDEXBLOCKSIZE,
                        DBE_MINIMUM_INDEXBLOCKSIZE,
                        give_warning);
            if (!size_ok) {
                *p_blocksize = DBE_DEFAULT_INDEXBLOCKSIZE;
            } else if (l >= DBE_CFG_MAXINDEXBLOCKSIZE) {
                *p_blocksize = DBE_CFG_MAXINDEXBLOCKSIZE;
            } else {
                *p_blocksize = (uint)l;
            }
        } else {
            *p_blocksize = DBE_DEFAULT_INDEXBLOCKSIZE;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getidxblocksize
 *
 * Gets configured index file block size
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_blocksize - out, use
 *              pointer to variable where the block size is stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getidxblocksize(
        dbe_cfg_t* dbe_cfg,
        size_t* p_blocksize)
{
        return(cfg_getidxblocksize(
                    dbe_cfg,
                    p_blocksize,
                    TRUE));
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getidxextendincr
 *
 * Gets configured index file extend increment (in blocks)
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_extendincr - out, use
 *              pointer to variable where the extend increment will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getidxextendincr(
        dbe_cfg_t* dbe_cfg,
        uint* p_extendincr)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_extendincr != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_EXTENDINCR,
                    &l);

        if (found) {
            *p_extendincr = (uint)l;
        } else {
            *p_extendincr = DBE_DEFAULT_INDEXEXTENDINCR;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getidxmaxseqalloc
 *
 * Gets configured maximum sequential block allocation count (in blocks).
 * Used during sequential inserts.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_maxseqalloc - out, use
 *              pointer to variable where the maximum sequential allocation
 *          value will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getidxmaxseqalloc(
        dbe_cfg_t* dbe_cfg,
        uint* p_maxseqalloc)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_maxseqalloc != NULL);
        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_MAXSEQALLOC,
                    &l);
        if (found) {
            *p_maxseqalloc = (uint)l;
        } else {
            *p_maxseqalloc = DBE_DEFAULT_INDEXMAXSEQALLOC;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogblocksize
 *
 * Gets the block size of the log file
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_blocksize - out, use
 *              pointer to variable where the blocksize is stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogblocksize(
        dbe_cfg_t* dbe_cfg,
        size_t* p_blocksize)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_blocksize != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_BLOCKSIZE,
                    &l);

        if (found) {
            if (!cfg_checkblocksize(
                    SU_DBE_LOGSECTION,
                    SU_DBE_BLOCKSIZE,
                    (size_t)l,
                    DBE_DEFAULT_LOGBLOCKSIZE,
                    DBE_CFG_MINLOGBLOCKSIZE,
                    TRUE)) {
                *p_blocksize = DBE_DEFAULT_LOGBLOCKSIZE;
            } else {
                *p_blocksize = (uint)l;
            }
        } else {
            *p_blocksize = DBE_DEFAULT_LOGBLOCKSIZE;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogextendincr
 *
 * Gets configured log file extend increment (in blocks)
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_extendincr - out, use
 *              pointer to variable where the extend increment will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogextendincr(
        dbe_cfg_t* dbe_cfg,
        uint* p_extendincr)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_extendincr != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_EXTENDINCR,
                    &l);

        if (found) {
            *p_extendincr = (uint)l;
        } else {
            size_t blocksize;
            dbe_cfg_getlogblocksize(dbe_cfg, &blocksize);
            *p_extendincr = DBE_DEFAULT_LOGEXTENDINCR_BYTES / blocksize;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogwritebuffersize
 *
 * Gets configured log file write buffer size. If possible, log writes
 * are buffered up to this size.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_buffersize - out, use
 *              pointer to variable where the buffer size will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogwritebuffersize(
        dbe_cfg_t* dbe_cfg,
        uint* p_buffersize)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGWRITEBUFFERSIZE,
                    &l);

        if (found) {
            size_t blocksize;
            dbe_cfg_getlogblocksize(dbe_cfg, &blocksize);
            if (l <  blocksize) {
                l = blocksize;
            }
            *p_buffersize = (uint)l;
        } else {
            size_t blocksize;
            dbe_cfg_getlogblocksize(dbe_cfg, &blocksize);
            *p_buffersize = SS_MAX(DBE_DEFAULT_LOGWRITEBUFFERSIZE, 2 * blocksize);
        }
        return (found);
}

bool dbe_cfg_getlogmaxwritequeuerecords(
        dbe_cfg_t* dbe_cfg,
        long* p_maxwritequeuerecords)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_MAXWRITEQUEUERECORDS,
                    p_maxwritequeuerecords);

        if (!found) {
            *p_maxwritequeuerecords = DBE_DEFAULT_LOGMAXWRITEQUEUERECORDS;
        }
        return (found);
}

bool dbe_cfg_getlogmaxwritequeuebytes(
        dbe_cfg_t* dbe_cfg,
        long* p_maxwritequeuebytes)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_MAXWRITEQUEUEBYTES,
                    p_maxwritequeuebytes);

        if (!found) {
            *p_maxwritequeuebytes = DBE_DEFAULT_LOGMAXWRITEQUEUEBYTES;
        }
        return (found);
}

bool dbe_cfg_getlogwritequeueflushlimit(
        dbe_cfg_t* dbe_cfg,
        long* p_writequeueflushlimit)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_WRITEQUEUEFLUSHLIMIT,
                    p_writequeueflushlimit);

        if (!found) {
            size_t blocksize;
            dbe_cfg_getlogblocksize(dbe_cfg, &blocksize);
            *p_writequeueflushlimit = SS_MAX(DBE_DEFAULT_LOGWRITEQUEUEFLUSHLIMIT, blocksize);
        }
        return (found);
}

bool dbe_cfg_getlogdelaymeswait(
        dbe_cfg_t* dbe_cfg,
        bool* p_delaymeswait)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_LOGSECTION,
                    SU_DBE_DELAYMESWAIT,
                    p_delaymeswait);

        if (!found) {
            *p_delaymeswait = DBE_DEFAULT_DELAYMESWAIT;
        }
        return (found);
}
#ifdef FSYNC_OPT
bool dbe_cfg_getlogfileflush(
        dbe_cfg_t* dbe_cfg,
        bool* p_fileflush)
{
        bool found;
        su_param_t* param_old_fileflush;
        
        ss_dassert(dbe_cfg != NULL);
    
        /* Make deprecated parameter invisible */
        param_old_fileflush = su_param_getparam(SU_DBE_LOGSECTION,
                                  SU_DBE_FILEFLUSH);
        ss_dassert(param_old_fileflush != NULL);
        su_param_setvisibility(param_old_fileflush, FALSE);

        
        found = su_inifile_getbool(dbe_cfg->cfg_file,
                                   SU_DBE_LOGSECTION,
                                   SU_DBE_FFILEFLUSH,
                                   p_fileflush);
    
        if (found) {
            return (found);
        }
        
        /* If not found, check deprecated parameter */
        found = su_inifile_getbool(dbe_cfg->cfg_file,
                                   SU_DBE_LOGSECTION,
                                   SU_DBE_FILEFLUSH,
                                   p_fileflush);
        
        if (!found) {
            *p_fileflush = DBE_DEFAULT_LOGFILEFLUSH;
            return (found);
        }
        
        su_param_setvisibility(param_old_fileflush, TRUE);
        return (found);
}
#else /* FSYNC_OPT */
bool dbe_cfg_getlogfileflush(
        dbe_cfg_t* dbe_cfg,
        bool* p_fileflush)
{
        bool found;
    
        ss_dassert(dbe_cfg != NULL);
    
        found = su_inifile_getbool(dbe_cfg->cfg_file,
                                   SU_DBE_LOGSECTION,
                                   SU_DBE_FILEFLUSH,
                                   p_fileflush);
    
        if (!found) {
            *p_fileflush = DBE_DEFAULT_LOGFILEFLUSH;
        }
        return (found);
}
#endif /* FSYNC_OPT */

bool dbe_cfg_getlogsyncwrite(
        dbe_cfg_t* dbe_cfg,
        bool* p_syncwrite)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_LOGSECTION,
                    SU_DBE_SYNCWRITE,
                    p_syncwrite);

        if (!found) {
            *p_syncwrite = DBE_DEFAULT_LOGSYNCWRITE;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getidxcachesize
 *
 * Gets configured index cache size in bytes
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              confiuration object
 *
 *      p_cachesize - out, use
 *              pointer to variable where the size will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getidxcachesize(
        dbe_cfg_t* dbe_cfg,
        size_t* p_cachesize)
{
        bool found;
        ss_int8_t i8;
        size_t cachesize;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_cachesize != NULL);

        found = cfg_getint8(
                    dbe_cfg,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_CACHESIZE,
                    &i8);

        if (found) {
            if (SsInt8ConvertToSizeT(&cachesize, i8)) {
                *p_cachesize = cachesize;
                return (found);
            }
            found = FALSE;
        }
        *p_cachesize = DBE_DEFAULT_INDEXCACHESIZE;
        return (found);
}

bool dbe_cfg_getfilespecs(
        dbe_cfg_t* dbe_cfg,
        su_pa_t* filespecs,
        char* section_name)
{
        bool found;
        bool succp = TRUE;
        int i;
        uint scanindex;
        dbe_filespec_t* p_filespec;
        char* filename;
        ss_int8_t maxsize;
        long diskno;
        char* buf = SsMemAlloc(strlen(SU_DBE_FILESPEC) + (6 - 2 + 1));

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(filespecs != NULL);

        ss_bassert(dbe_filespecs_registered == TRUE);

        for (i = 1; i < (int)SS_INT2_MAX; i++) {
            SsSprintf(buf, SU_DBE_FILESPEC, i);
            scanindex = 0;

            if (!dbe_cfg->cfg_reentrant) {
                found = su_param_scanstring(
                        dbe_cfg->cfg_file,
                        section_name,
                        buf,
                        " \t,",
                        &scanindex,
                        &filename);
                if (!found) {
                    break;
                }
                found = su_param_scanint8(
                        dbe_cfg->cfg_file,
                        section_name,
                        buf,
                        " \t,",
                        &scanindex,
                        &maxsize);
                if (!found) {
                    succp = FALSE;
                    SsMemFree(filename);
                    break;
                }
                found = su_param_scanlong(
                        dbe_cfg->cfg_file,
                        section_name,
                        buf,
                        " \t,",
                        &scanindex,
                        &diskno);
                if (!found) {
                    diskno = 0L;
                }
            } else {
                found = su_inifile_scanstring(
                        dbe_cfg->cfg_file,
                        section_name,
                        buf,
                        " \t,",
                        &scanindex,
                        &filename);
                if (!found) {
                    break;
                }
                found = su_inifile_scanint8(
                        dbe_cfg->cfg_file,
                        section_name,
                        buf,
                        " \t,",
                        &scanindex,
                        &maxsize);
                if (!found) {
                    succp = FALSE;
                    SsMemFree(filename);
                    break;
                }
                found = su_inifile_scanlong(
                        dbe_cfg->cfg_file,
                        section_name,
                        buf,
                        " \t,",
                        &scanindex,
                        &diskno);
                if (!found) {
                    diskno = 0L;
                }
            }

            p_filespec = dbe_filespec_init(filename, maxsize, (uint)diskno);
            SsMemFree(filename);
            su_pa_insert(filespecs, p_filespec);
        }
        ss_dassert(i < SHRT_MAX);
        if (!succp) {
            if (i == 1) {
                ui_msg_warning(INI_MSG_INVALID_INDEXFILE_SPEC_SSSD, section_name, buf, DBE_DEFAULT_INDEXFILENAME, (long)SU_VFIL_SIZE_MAX);
            } else {
                ui_msg_warning(INI_MSG_INVALID_VALUE_IGNORE_FOLLOWING_SS, section_name, buf);
            }
        }
        SsMemFree(buf);
        if (i == 1) {
            ss_int8_t maxsize;
#ifdef SS_LARGEFILE
            size_t blocksize;
            ss_int8_t tmp_i8;

            dbe_cfg_getidxblocksize(dbe_cfg, &blocksize);

            SsInt8SetUint4(&maxsize, SU_DADDR_MAX);
            SsInt8SetUint4(&tmp_i8, (ss_uint4_t)DBE_MINIMUM_INDEXBLOCKSIZE);
            SsInt8MultiplyByInt8(&maxsize, maxsize, tmp_i8);
#else /* SS_LARGEFILE */
            SsInt8SetUint4(&maxsize, SU_VFIL_SIZE_MAX);
#endif /* SS_LARGEFILE */
            p_filespec =
                dbe_filespec_init((char *)DBE_DEFAULT_INDEXFILENAME, maxsize, 0);
            su_pa_insert(filespecs, p_filespec);
            return (FALSE);
        } else {
            return (TRUE);
        }
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getidxfilespecs
 *
 * Gets the filespecs of the index file
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      idxfilespecs - out, use
 *              pointer array of dbe_filespec_t* objects
 *
 * Return value :
 *      TRUE if successful or
 *      FALSE if no filespec for index file was found
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getidxfilespecs(
        dbe_cfg_t* dbe_cfg,
        su_pa_t* idxfilespecs)
{
        bool b =
            dbe_cfg_getfilespecs(
                    dbe_cfg,
                    idxfilespecs,
                    (char *)SU_DBE_INDEXSECTION);
        return (b);
}

#ifdef FSYNC_OPT
/*##**********************************************************************\
 *
 *              dbe_cfg_getsyncwrite
 *
 * Reads the value of configuration parameter IndexFile.SyncWrite
 *  and its alias IndexFile.SynchronizedWrite.
 *  If both syncwrite and synchronizedwrite are specified with different value,
 *  only the current SyncWrite is taken into account.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_syncwrite - in, out
 *              Pointer to boolean value where - if synchronized write is to
 *              be used the result is stored here.
 *
 *      p_syncwritefsync - in, out
 *              Pointer to boolean value where - if synchronized write along
 *              with fsync is to be used - the result is stored here.
 *
 * Return value :
 *      TRUE if paramter value was found
 *      FALSE if no parameter value was specified
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getsyncwrite(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_syncwrite)
{
        bool        found;
        su_param_t* param_old_syncwrite;

        ss_dassert(dbe_cfg != NULL);

        /* Make deprecated parameter invisible */
        param_old_syncwrite = su_param_getparam(SU_DBE_INDEXSECTION,
                                  SU_DBE_SYNCHRONIZEDWRITE);
        ss_dassert(param_old_syncwrite != NULL);
        ss_dprintf_1(("Calling su_param_setvisibility\n"));
        su_param_setvisibility(param_old_syncwrite, FALSE);

        /* Check the new form of synchronized write parameter */
        found = cfg_getbool(
                dbe_cfg,
                SU_DBE_INDEXSECTION,
                SU_DBE_SYNCWRITE,
                p_syncwrite);

        if (found) {
            return (found);
        }

        /* Igf not found, check the deprecated paramter */
        found = cfg_getbool(
                dbe_cfg,
                SU_DBE_INDEXSECTION,
                SU_DBE_SYNCHRONIZEDWRITE,
                p_syncwrite);

        if (!found) {
            *p_syncwrite = DBE_DEFAULT_SYNCHRONIZEDWRITE;
            return (found);
        }

        su_param_setvisibility(param_old_syncwrite, TRUE);
        return (found);
}

bool dbe_cfg_getindexfileflush(
        dbe_cfg_t* dbe_cfg,
        bool* p_fileflush)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);

        found = su_inifile_getbool(dbe_cfg->cfg_file,
                                   SU_DBE_INDEXSECTION,
                                   SU_DBE_FFILEFLUSH,
                                   p_fileflush);

        if (!found) {
            *p_fileflush = DBE_DEFAULT_LOGFILEFLUSH;
        }
        return (found);
}


#else /* FSYNC_OPT */
bool dbe_cfg_getsynchronizedwrite(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_synchronizedwrite)
{
        bool        found;

        ss_dassert(dbe_cfg != NULL);

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_SYNCHRONIZEDWRITE,
                    p_synchronizedwrite);

        if (!found) {
            *p_synchronizedwrite = DBE_DEFAULT_SYNCHRONIZEDWRITE;
        }
        return (found);
}
#endif /* FSYNC_OPT */

#ifdef IO_OPT

bool dbe_cfg_getindexdirectio(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_directio)
{
        bool        found;

        ss_dassert(dbe_cfg != NULL);

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_DIRECTIO,
                    p_directio);

        if (!found) {
            *p_directio = DBE_DEFAULT_INDEXDIRECTIO;
        }
        return (found);
}

bool dbe_cfg_getlogdirectio(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_directio)
{
        bool        found;

        ss_dassert(dbe_cfg != NULL);

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_DIRECTIO,
                    p_directio);

        if (!found) {
            *p_directio = DBE_DEFAULT_LOGDIRECTIO;
        }
        return (found);
}

#endif /* IO_OPT */

bool dbe_cfg_getfreelistreserve(
        dbe_cfg_t* cfg,
        long* p_reservesize)
{
        bool found;

        ss_dassert(cfg != NULL);
        ss_dassert(p_reservesize != NULL);

        found = su_inifile_getlong(
                    cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_FREELISTRESERVERSIZE,
                    p_reservesize);

        if (!found) {
            *p_reservesize = DBE_DEFAULT_FREELISTRESERVERSIZE;
        }
        return (found);
}

bool dbe_cfg_getfreelistgloballysorted(
        dbe_cfg_t* cfg,
        bool* p_globallysorted)
{
        bool found;

        ss_dassert(cfg != NULL);
        ss_dassert(p_globallysorted != NULL);

        found = su_inifile_getbool(
                    cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_FREELISTGLOBALLYSORTED,
                    p_globallysorted);

        if (!found) {
            *p_globallysorted = DBE_DEFAULT_FREELISTGLOBALLYSORTED;
        }
        return (found);
}

void dbe_cfg_setfreelistgloballysorted(
        dbe_cfg_t* cfg,
        bool globallysorted)
{
        ss_dassert(cfg != NULL);
        su_inifile_putbool(
                    cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_FREELISTGLOBALLYSORTED,
                    globallysorted);
}

void dbe_cfg_setfreelistreserve(
        dbe_cfg_t* cfg,
        bool globallysorted)
{
        ss_dassert(cfg != NULL);

        su_inifile_putbool(
                    cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_FREELISTGLOBALLYSORTED,
                    globallysorted);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_registerfilespecs
 *
 * Register filespecs
 *
 * Parameters :
 *
 *      dbe_cfg -
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
void dbe_cfg_registerfilespecs(dbe_cfg_t* dbe_cfg)
{
        char* buf = SsMemAlloc(strlen(SU_DBE_FILESPEC) + (6 - 2 + 1));
        bool b = FALSE;
        bool succp = TRUE;
        uint scanindex;
        char* filespec;
        long maxsize;
        int i;

        for (i = 1; i < (int)SS_INT2_MAX; i++) {
            SsSprintf(buf, SU_DBE_FILESPEC, i);
            scanindex = 0;
            b = su_inifile_scanstring(
                        dbe_cfg->cfg_file,
                        SU_DBE_INDEXSECTION,
                        buf,
                        " \t,",
                        &scanindex,
                        &filespec);

            if (!b) {
                break;
            }
            b = su_inifile_scanlong(
                        dbe_cfg->cfg_file,
                        SU_DBE_INDEXSECTION,
                        buf,
                        " \t,",
                        &scanindex,
                        &maxsize);
            if (!b) {
                succp = FALSE;
                SsMemFree(filespec);
                break;
            }
            b = su_param_register(
                        SU_DBE_INDEXSECTION,
                        buf,
                        NULL,
                        NULL,
                        "",
                        "Filespec_n describes the location and the maximum size of the database file",
                        NULL,
                        NULL,
                        SU_PARAM_TYPE_STR,
                        SU_PARAM_AM_RONLY
            );
            SsMemFree(filespec);
            if (!b) {
                succp = FALSE;
                ss_derror;
                break;
            }
        }
        if (!succp) {
            if (i == 1) {
                ui_msg_warning(INI_MSG_INVALID_INDEXFILE_SPEC_SSSD, SU_DBE_INDEXSECTION, buf, DBE_DEFAULT_INDEXFILENAME, (long)SU_VFIL_SIZE_MAX);
            } else {
                ui_msg_warning(INI_MSG_INVALID_VALUE_IGNORE_FOLLOWING_SS, SU_DBE_INDEXSECTION, buf);
            }
        }
        ss_beta(dbe_filespecs_registered = TRUE;);
        SsMemFree(buf);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogenabled
 *
 * Checks whether the logging is enabled
 *
 * Parameters :
 *
 *      cfg - in, use
 *              configuration object
 *
 *
 *      p_logenabled - out
 *              pointer to boolean variable which indeicates whether
 *          logging is enabled
 *
 * Return value :
 *      TRUE if the configuration was given in cfg object or
 *      FALSE if the default was given
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getlogenabled(
        dbe_cfg_t* cfg,
        bool* p_logenabled)
{
        bool found;

        ss_dassert(cfg != NULL);
        ss_dassert(p_logenabled != NULL);

        found = cfg_getbool(
                    cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGENABLED,
                    p_logenabled);

        if (!found) {
            *p_logenabled = DBE_DEFAULT_LOGENABLED;
        }
        /* disable logging for diskless */
        if (dbelog_diskless) {
            *p_logenabled = FALSE;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getloggroupcommitqueue
 *
 * Checks whether group commit queue is enabled for logging.
 *
 * Parameters :
 *
 *      cfg -
 *
 *
 *      p_groupcommitqueue -
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
bool dbe_cfg_getloggroupcommitqueue(
        dbe_cfg_t* cfg,
        bool* p_groupcommitqueue)
{
        bool found;

        ss_dassert(cfg != NULL);

        found = su_inifile_getbool(
                    cfg->cfg_file,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGUSEGROUPCOMMITQUEUE,
                    p_groupcommitqueue);

        if (!found) {
            *p_groupcommitqueue = DBE_DEFAULT_LOGGROUPCOMMITQUEUE;
        }
        return (found);
}

static bool cfg_getlogfilenametemplate_from_section(
        dbe_cfg_t* dbe_cfg,
        char** p_lfnametemplate,
        char* section_name,
        char* keyword)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_lfnametemplate != NULL);

        found = cfg_getvalue(
                    dbe_cfg,
                    section_name,
                    keyword,
                    p_lfnametemplate);

        if (!found) {
            *p_lfnametemplate = SsMemStrdup((char *)DBE_DEFAULT_LOGFNAMETEMPLATE);
        }
        return (found);
}

static bool cfg_getlogdir_from_section(
        dbe_cfg_t* dbe_cfg,
        char** p_logdir,
        char* section_name,
        char* keyword)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_logdir != NULL);

        found = cfg_getvalue(
                    dbe_cfg,
                    section_name,
                    keyword,
                    p_logdir);

        if (!found) {
            *p_logdir = SsMemStrdup((char *)DBE_DEFAULT_LOGDIR);
        }
        return (found);
}

/*##**********************************************************************\
 *
 *          dbe_cfg_getlogfilenametemplate_for_backup
 *
 * Gets log file name template for a given backup profile
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *          configuration object
 *
 *      p_lfnametemplate - out, give
 *          pointer to variable where a pointer to a copy of
 *          template string will be stored
 *
 *      backup_name - in, use
 *          backup profile name
 *
 * Return value :
 *      TRUE if successful or
 *      FALSE if no template was found (default was given!)
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogfilenametemplate_for_backup(
        dbe_cfg_t* dbe_cfg,
        char** p_lfnametemplate,
        char* backup_name)
{
        bool found =
            cfg_getlogfilenametemplate_from_section(
                    dbe_cfg,
                    p_lfnametemplate,
                    backup_name,
                    (char *)SU_DBE_LOGFILETEMPLATE_FOR_BACKUP);
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogfilenametemplate
 *
 * Gets log file name template
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_lfnametemplate - out, give
 *              pointer to variable where a pointer to a copy of
 *          template string will be stored
 *
 * Return value :
 *      TRUE if successful or
 *      FALSE if no template was found (default was given!)
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogfilenametemplate(
        dbe_cfg_t* dbe_cfg,
        char** p_lfnametemplate)
{
        bool found =
            cfg_getlogfilenametemplate_from_section(
                    dbe_cfg,
                    p_lfnametemplate,
                    (char *)SU_DBE_LOGSECTION,
                    (char *)SU_DBE_LOGFILETEMPLATE);
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogdir
 *
 * Gets log file name template
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_lfnametemplate - out, give
 *              pointer to variable where a pointer to a copy of
 *          template string will be stored
 *
 * Return value :
 *      TRUE if successful or
 *      FALSE if no template was found (default was given!)
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogdir(
        dbe_cfg_t* dbe_cfg,
        char** p_logdir)
{
        bool found =
            cfg_getlogdir_from_section(
                    dbe_cfg,
                    p_logdir,
                    (char *)SU_DBE_LOGSECTION,
                    (char *)SU_DBE_LOGDIR);
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogdigittemplate
 *
 * Gets digit template character used in log file name template.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_digittempl - out, use
 *              pointer to character variable where the result will be
 *          stored.
 *
 * Return value :
 *      TRUE if successful or
 *      FALSE if not found (default is given!)
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogdigittemplate(
        dbe_cfg_t* dbe_cfg,
        char* p_digittempl)
{
        bool found;
        char *s;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_digittempl != NULL);

        found = cfg_getvalue(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGDIGITTEMPLATE,
                    &s);

        if (!found) {
            *p_digittempl = DBE_DEFAULT_LOGDIGITTEMPLATE;
        } else {
            ss_dassert(s != NULL);
            *p_digittempl = *s;
            SsMemFree(s);
        }
        return (found);
}
/*##**********************************************************************\
 *
 *              dbe_cfg_getlogfileminsplitsize
 *
 * Gets minimum log file split size
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_lfminsplitsize - out, use
 *              pointer to variable where the result will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogfileminsplitsize(
        dbe_cfg_t* dbe_cfg,
        ulong* p_lfminsplitsize)
{
        bool found;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_lfminsplitsize != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGMINSPLITSIZE,
                    (long*)p_lfminsplitsize);

        if (!found) {
            *p_lfminsplitsize = (ulong)DBE_DEFAULT_LOGMINSPLITSIZE;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogcommitmaxwait
 *
 * Gets maximum wait delay in milliseconds before commit is written to disk.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_lfcommitmaxwait - out, use
 *              pointer to variable where the value will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogcommitmaxwait(
        dbe_cfg_t* dbe_cfg,
        ulong* p_lfcommitmaxwait)
{
        bool found;

        found = su_inifile_getlong(
                dbe_cfg->cfg_file,
                SU_DBE_LOGSECTION,
                SU_DBE_LOGCOMMITMAXWAIT,
                (long*)p_lfcommitmaxwait);
        if (!found) {
            *p_lfcommitmaxwait = DBE_DEFAULT_LOGCOMMITMAXWAIT;
        } else {
            if (*p_lfcommitmaxwait > 0L && *p_lfcommitmaxwait < 100L) {
                ui_msg_warning(INI_MSG_INVALID_COMMITMAXWAIT_VALUE_DSSD,
                                *p_lfcommitmaxwait,
                                SU_DBE_LOGSECTION,
                                SU_DBE_LOGCOMMITMAXWAIT,
                                (long)DBE_DEFAULT_LOGCOMMITMAXWAIT);

                *p_lfcommitmaxwait = DBE_DEFAULT_LOGCOMMITMAXWAIT;
            }
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogwritemode
 *
 * Gets whether logging is in write-once mode
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_writemode - out, use
 *              pointer to int-size variable where the write mode is stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogwritemode(
        dbe_cfg_t* dbe_cfg,
        int* p_writemode)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                dbe_cfg->cfg_file,
                SU_DBE_LOGSECTION,
                SU_DBE_LOGWRITEMODE,
                &l);
        if (!found) {
            *p_writemode = DBE_DEFAULT_LOGWRITEMODE;
        } else {
            switch ((int)l) {
                case 0:
                case 1:
                case 2:
                case 3:
                    *p_writemode = (int)l;
                    break;
                default:
                    ui_msg_warning(INI_MSG_ILLEGAL_VALUE_USSU,
                        (uint)l, SU_DBE_LOGSECTION, SU_DBE_LOGWRITEMODE, DBE_DEFAULT_LOGWRITEMODE);
                    *p_writemode = DBE_DEFAULT_LOGWRITEMODE;
                    break;
            }
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getbackupdir
 *
 * Returns the backup directory name. There is no default backup
 * directory.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_backupdir - out, give
 *              pointer to a string variable onto where the backup directory
 *              is allocated
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getbackupdir(
        dbe_cfg_t* dbe_cfg,
        char** p_backupdir)
{
        bool found;

        found = cfg_getvalue(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_BACKUPDIR,
                    p_backupdir);

        if (!found) {
            *p_backupdir = SsMemStrdup((char *)DBE_DEFAULT_BACKUPDIR);
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getbackup_blocksize
 *
 * Returns the backup blocksize. Default is 64Kb.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_backup_blocksize - out, give
 *              pointer to a string variable onto where the backup directory
 *              is allocated
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getbackup_blocksize(
        dbe_cfg_t* dbe_cfg,
        long* p_backup_blocksize)
{
        bool found;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_BACKUP_BLOCKSIZE,
                    p_backup_blocksize);

        if (!found) {
            *p_backup_blocksize = DBE_DEFAULT_BACKUP_BLOCKSIZE;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getbackup_stepstoskip
 *
 * Returns the backup steps to be skipped before proceeding. Default is 0.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_backup_blocksize - out, give
 *              pointer to a string variable onto where the backup directory
 *              is allocated
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getbackup_stepstoskip(
        dbe_cfg_t* dbe_cfg,
        long* p_backup_stepstoskip)
{
        bool found;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_BACKUP_STEPSTOSKIP,
                    p_backup_stepstoskip);

        if (!found) {
            *p_backup_stepstoskip = DBE_DEFAULT_BACKUP_STEPSTOSKIP;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getbackupcopylog
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_backupcopylog -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getbackupcopylog(
        dbe_cfg_t* dbe_cfg,
        bool* p_backupcopylog)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_BACKUPCOPYLOG,
                    p_backupcopylog);

        if (!found) {
            *p_backupcopylog = DBE_DEFAULT_BACKUPCOPYLOG;
        }

        return(found);
}

bool dbe_cfg_getnetbackupcopylog(
        dbe_cfg_t* dbe_cfg,
        bool* p_netbackupcopylog)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_NETBACKUPCOPYLOG,
                    p_netbackupcopylog);

        if (!found) {
            *p_netbackupcopylog = DBE_DEFAULT_BACKUPCOPYLOG;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getbackupdeletelog
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_backupdeletelog -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getbackupdeletelog(
        dbe_cfg_t* dbe_cfg,
        bool* p_backupdeletelog)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_BACKUPDELETELOG,
                    p_backupdeletelog);

        if (!found) {
            *p_backupdeletelog = DBE_DEFAULT_BACKUPDELETELOG;
        }

        return(found);
}

bool dbe_cfg_getnetbackupdeletelog(
        dbe_cfg_t* dbe_cfg,
        bool* p_netbackupdeletelog)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_NETBACKUPDELETELOG,
                    p_netbackupdeletelog);

        if (!found) {
            *p_netbackupdeletelog = DBE_DEFAULT_BACKUPDELETELOG;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getbackupcopyinifile
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_copyinifile -
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
bool dbe_cfg_getbackupcopyinifile(
        dbe_cfg_t* dbe_cfg,
        bool* p_copyinifile)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_BACKUPCOPYINIFLE,
                    p_copyinifile);

        if (!found) {
            *p_copyinifile = DBE_DEFAULT_BACKUPCOPYINIFILE;
        }

        return(found);
}

bool dbe_cfg_getnetbackupcopyinifile(
        dbe_cfg_t* dbe_cfg,
        bool* p_copyinifile)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_NETBACKUPCOPYINIFLE,
                    p_copyinifile);

        if (!found) {
            *p_copyinifile = DBE_DEFAULT_BACKUPCOPYINIFILE;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getbackupcopysolmsgout
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_copysolmsgout -
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
bool dbe_cfg_getbackupcopysolmsgout(
        dbe_cfg_t* dbe_cfg,
        bool* p_copysolmsgout)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_BACKUPCOPYSOLMSGOUT,
                    p_copysolmsgout);

        if (!found) {
            *p_copysolmsgout = DBE_DEFAULT_BACKUPCOPYSOLMSGOUT;
        }

        return(found);
}

bool dbe_cfg_getnetbackupcopysolmsgout(
        dbe_cfg_t* dbe_cfg,
        bool* p_copysolmsgout)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_NETBACKUPCOPYSOLMSGOUT,
                    p_copysolmsgout);

        if (!found) {
            *p_copysolmsgout = DBE_DEFAULT_BACKUPCOPYSOLMSGOUT;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getddisableidlemerge
 *
 * Gets disable idle merge flag status from configuration. Idle merge feature
 * is will result changing timestamp on solid.db file. Some customer
 * does not like that.
 *
 * Parameters :
 *
 *      cfg -
 *
 *
 *      p_disableidlemerge -
 *
 *
 * Return value :
 *
 * Comments :
 *             DisableIdleMerge has to be set in solid.in. Default is not set
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getdisableidlemerge(
        dbe_cfg_t* dbe_cfg,
        bool* p_disableidlemerge)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_DISABLEIDLEMERGE,
                    p_disableidlemerge);
        if (!found) {
            *p_disableidlemerge = DBE_DEFAULT_DISABLEIDLEMERGE;
        }

        return(found);
}
/*##**********************************************************************\
 *
 *              dbe_cfg_getreadonly
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_readonly -
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
bool dbe_cfg_getreadonly(
        dbe_cfg_t* dbe_cfg,
        bool* p_readonly)
{
        bool found;

#ifdef SS_LICENSEINFO_V3
        if (su_li3_isreadonly()) {
#else /* SS_LICENSEINFO_V3 */
        if (su_li2_isreadonly()) {
#endif /* SS_LICENSEINFO_V3 */
            *p_readonly = TRUE;
            return(FALSE);
        }
        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_READONLY,
                    p_readonly);

        if (!found) {
            *p_readonly = DBE_DEFAULT_READONLY;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getcheckescalatelimit
 *
 * Gets configuration value for transaction check escalation limit after which
 * check operations are escalated.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_limit -
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
bool dbe_cfg_getcheckescalatelimit(
        dbe_cfg_t* dbe_cfg,
        long* p_limit)
{
        bool found;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_CHECKESCALATELIMIT,
                    p_limit);

        if (!found) {
            *p_limit = DBE_DEFAULT_CHECKESCALATELIMIT;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getreadescalatelimit
 *
 * Gets configuration value for transaction read escalation limit after which
 * read operations are escalated.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_limit -
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
bool dbe_cfg_getreadescalatelimit(
        dbe_cfg_t* dbe_cfg,
        long* p_limit)
{
        bool found;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_READESCALATELIMIT,
                    p_limit);

        if (!found) {
            *p_limit = DBE_DEFAULT_READESCALATELIMIT;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlockescalatelimit
 *
 * Gets configuration value for transaction lock escalation limit after which
 * lock operations are escalated.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_limit -
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
bool dbe_cfg_getlockescalatelimit(
        dbe_cfg_t* dbe_cfg,
        long* p_limit)
{
        bool found;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_LOCKESCALATELIMIT,
                    p_limit);

        if (!found) {
            *p_limit = DBE_DEFAULT_LOCKESCALATELIMIT;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getallowlockebounce
 *
 * Gets configuration value for lock bouncing. If TRUE, optimistic locks
 * other than FOR UPDATE use bounce locks that only check if locking
 * is possible. This method saves memory in operations that handle
 * large sets of tuples.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_allowbounce -
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
bool dbe_cfg_getallowlockebounce(
        dbe_cfg_t* dbe_cfg,
        bool* p_allowbounce)
{
        bool found;

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_ALLOWLOCKBOUNCE,
                    p_allowbounce);

        if (!found) {
            *p_allowbounce = DBE_DEFAULT_ALLOWLOCKBOUNCE;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getcpinterval
 *
 * Returns the interval after how many log inserts a checkpoint should
 * be made.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_cpinterval -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getcpinterval(
        dbe_cfg_t* dbe_cfg,
        long* p_cpinterval)
{
        bool found;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_CPINTERVAL,
                    p_cpinterval);

        if (!found) {
            *p_cpinterval = DBE_DEFAULT_CPINTERVAL;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getcpmintime
 *
 * Returns minimum time in seconds after which checkpoint can be made.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_cpmintime -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getcpmintime(
        dbe_cfg_t* dbe_cfg,
        long* p_cpmintime)
{
        bool found;

        found = su_inifile_getlong(
                dbe_cfg->cfg_file,
                SU_DBE_GENERALSECTION,
                SU_DBE_CPMINTIME,
                p_cpmintime);

        if (found) {
            if (*p_cpmintime < 0) {
                /* Overflow */
                *p_cpmintime = LONG_MAX;
            }
        } else {
            *p_cpmintime = DBE_DEFAULT_CPMINTIME;
        }

        return(found);
}

/*#***********************************************************************\
 *
 *              cfg_defaultmaxbonsaikeys
 *
 * Returns max number of Bonsai-tree keys allowed, based on configured
 * cache size.
 *
 * Parameters :
 *
 *      dbe_cfg -
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
static ulong cfg_defaultmaxbonsaikeys(dbe_cfg_t* dbe_cfg)
{
        size_t idxcachesize;
        ulong idxcacheblocks;
        size_t idxblocksize;
        ulong maxbonsaiblocks;
        ulong maxbonsaikeys;

        cfg_getidxblocksize(dbe_cfg, &idxblocksize, FALSE);
        dbe_cfg_getidxcachesize(dbe_cfg, &idxcachesize);
        idxcacheblocks = idxcachesize / idxblocksize;

        /* Calculate max number of cache blocks that Bonsai-tree
         * is allowed to use.
         */
        maxbonsaiblocks = (idxcacheblocks * DBE_MAXBONSAIBLOCKSPERC) / 100;
        /* Calculate approximate max number of key values that can be
         * in Bonsai-tree. It is calculated from approximate number
         * of key values in 1024 kb of index leaf.
         */
        maxbonsaikeys = (idxblocksize / 1024) * DBE_KEYSPER1024B *
                        maxbonsaiblocks;

        return(maxbonsaikeys);
}

/*#***********************************************************************\
 *
 *              cfg_defaultmergeinterval
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
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
static ulong cfg_defaultmergeinterval(dbe_cfg_t* dbe_cfg)
{
        ulong mergeinterval;

        mergeinterval = cfg_defaultmaxbonsaikeys(dbe_cfg);

        if (mergeinterval < DBE_MINMERGEINTERVAL) {
            mergeinterval = DBE_MINMERGEINTERVAL;
        } else if (mergeinterval > DBE_DEFAULT_MAXMERGEINTERVAL) {
            mergeinterval = DBE_DEFAULT_MAXMERGEINTERVAL;
        }

        return(mergeinterval);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getmergeinterval
 *
 * Returns the interval after how many index inserts a merge process
 * should be run.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_mergeinterval -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getmergeinterval(
        dbe_cfg_t* dbe_cfg,
        long* p_mergeinterval)
{
        bool found;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_MERGEINTERVAL,
                    p_mergeinterval);

        if (!found || (found && *p_mergeinterval <= 0)) {
            *p_mergeinterval = cfg_defaultmergeinterval(dbe_cfg);
        } else {
            if (*p_mergeinterval < DBE_MINMERGEINTERVAL) {
                *p_mergeinterval = DBE_MINMERGEINTERVAL;
            } else if (*p_mergeinterval > DBE_MAXMERGEINTERVAL) {
                *p_mergeinterval = DBE_MAXMERGEINTERVAL;
            }
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getquickmergeinterval
 *
 * Returns the interval after how many transactions a quick merge process
 * should be run. Quick merge process does not remove key values from
 * the Bonsai-tree but patches memory resident trxbuf data to the
 * key values and thus reduces memory overhead.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_quickmergeinterval -
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
bool dbe_cfg_getquickmergeinterval(
        dbe_cfg_t* dbe_cfg,
        long* p_quickmergeinterval)
{
        bool found;

        found = su_inifile_getlong(
                dbe_cfg->cfg_file,
                SU_DBE_GENERALSECTION,
                SU_DBE_QUICKMERGEINTERVAL,
                p_quickmergeinterval);

        if (!found) {
            *p_quickmergeinterval = DBE_DEFAULT_QUICKMERGEINTERVAL;
        } else {
            if (*p_quickmergeinterval < 0) {
                *p_quickmergeinterval = 0; /* FIXME: what is correct value? */
            }
        }

        return(found);
}

bool dbe_cfg_getwriteflushmode(
        dbe_cfg_t* dbe_cfg,
        int* p_writeflushmode)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_WRITEFLUSHMODE,
                    &l);
        if (found) {
            switch ((int)l) {
                case SS_BFLUSH_NORMAL:
                case SS_BFLUSH_BEFOREREAD:
                case SS_BFLUSH_AFTERWRITE:
                    break;
                default:
                    found = FALSE;
                    break;
            }
        }
        if (!found) {
            *p_writeflushmode = DBE_DEFAULT_WRITEFLUSHMODE;
        } else {
            *p_writeflushmode = (int)l;
        }
        return (found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getfakemerge
 *
 * Returns the default fakemerge setting. Fake merge is used in debug
 * version to mark merge active altough it really is not active.
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *              configuration object
 *
 *
 *      p_fakemerge - out
 *              pointer to bool variable where the result will be stored
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getfakemerge(
        dbe_cfg_t* dbe_cfg,
        bool* p_fakemerge)
{
        bool found;

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    "FakeMerge",
                    p_fakemerge);

        if (!found) {
            *p_fakemerge = FALSE;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getmergemintime
 *
 * Returns minimum time in seconds after which merge can start.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_mergemintime -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getmergemintime(
        dbe_cfg_t* dbe_cfg,
        long* p_mergemintime)
{
        bool found;

        found = su_inifile_getlong(
                dbe_cfg->cfg_file,
                SU_DBE_GENERALSECTION,
                SU_DBE_MERGEMINTIME,
                p_mergemintime);

        if (found) {
            if (*p_mergemintime < 0) {
                /* Overflow */
                *p_mergemintime = LONG_MAX;
            }
        } else {
            *p_mergemintime = DBE_DEFAULT_MERGEMINTIME;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getseqsealimit
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_seqsealimit -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getseqsealimit(
        dbe_cfg_t* dbe_cfg,
        ulong* p_seqsealimit)
{
        bool found;
        long l;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_SEQSEALIMIT,
                    &l);

        if (!found) {
            *p_seqsealimit = DBE_DEFAULT_SEQSEALIMIT;
        } else {
            *p_seqsealimit = (ulong)l;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getseabuflimit
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_seabuflimit -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getseabuflimit(
        dbe_cfg_t* dbe_cfg,
        long* p_seabuflimit)
{
        bool found;
        long percentage;
        size_t blocksize;
        size_t cachesize;
        long nblocks;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_SEABUFLIMIT,
                    &percentage);

        if (!found) {
            percentage = DBE_DEFAULT_SEABUFLIMIT;
        }

        cfg_getidxblocksize(dbe_cfg, &blocksize, FALSE);
        dbe_cfg_getidxcachesize(dbe_cfg, &cachesize);

        nblocks = cachesize / blocksize;

        *p_seabuflimit = (percentage * nblocks) / 100L;

        return(found);
}

/*#***********************************************************************\
 *
 *              cfg_defaulttrxbufsize
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
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
static long cfg_defaulttrxbufsize(dbe_cfg_t* dbe_cfg)
{
        long maxbonsaikeys;
        ulong trxbufsize;

        /* The default trx buffer size depends on the cache size.
         */
        maxbonsaikeys = cfg_defaultmaxbonsaikeys(dbe_cfg);
        trxbufsize = SS_MAX(DBE_DEFAULT_TRXBUFSIZE, maxbonsaikeys / 2);
        if (trxbufsize * sizeof(void*) > SS_MAXALLOCSIZE) {
            trxbufsize = SS_MAXALLOCSIZE / sizeof(void*);
        }
        trxbufsize = SS_MIN(DBE_MAX_TRXBUFSIZE, trxbufsize);
        return(trxbufsize);

}

/*##**********************************************************************\
 *
 *              dbe_cfg_gettrxbufsize
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_trxbufsize -
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
bool dbe_cfg_gettrxbufsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_trxbufsize)
{
        bool found;
        long l;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_TRXBUFSIZE,
                    &l);

        if (found) {
            *p_trxbufsize = (uint)l;
        } else {
            *p_trxbufsize = (uint)cfg_defaulttrxbufsize(dbe_cfg);
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getearlyvld
 *
 * Gets early transaction flag from configuration.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_earlyvld -
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
bool dbe_cfg_getearlyvld(
        dbe_cfg_t* dbe_cfg,
        bool* p_earlyvld)
{
        bool found;

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_TRXEARLYVALIDATE,
                    p_earlyvld);

        if (!found) {
            *p_earlyvld = DBE_DEFAULT_TRXEARLYVALIDATE;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getreadaheadsize
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_readaheadsize -
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
bool dbe_cfg_getreadaheadsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_readaheadsize)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_readaheadsize != NULL);

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_READAHEADSIZE,
                    &l);

        if (found) {
            *p_readaheadsize = (uint)l;
        } else {
            *p_readaheadsize = DBE_DEFAULT_READAHEADSIZE;
        }
        return (found);
}

#ifdef SS_BLOCKINSERT
bool dbe_cfg_getblockinsertsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_blockinsertsize)
{
        bool found;
        long l;

        ss_dassert(dbe_cfg != NULL);
        ss_dassert(p_blockinsertsize != NULL);
        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    "BlockInsertSize",
                    &l);
        if (found) {
            *p_blockinsertsize = (uint)l;
        } else {
            *p_blockinsertsize = 0;
        }
        return (found);
}
#endif /* SS_BLOCKINSERT */

/*##**********************************************************************\
 *
 *              dbe_cfg_getuseiothreads
 *
 * Checks whether I/O thread per device scheme is enabled. Otherwise
 * synchronous I/O requests go directly from client thread to file system.
 * The Prefetch/preflush operations are done using asynch threads in both
 * cases in all multithreaded environments.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_useiothreads - out, use
 *              pointer to bool variable where the result will be stored
 *
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getuseiothreads(
        dbe_cfg_t* dbe_cfg,
        bool* p_useiothreads)
{
        bool found;

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_USEIOTHREADS,
                    p_useiothreads);

        if (!found) {
            *p_useiothreads = DBE_DEFAULT_USEIOTHREADS;
        }

        return(found);
}


/*##**********************************************************************\
 *
 *              dbe_cfg_getpessimistic
 *
 * Returns the default table concurrency control mechanism.
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *              configuration object
 *
 *
 *      p_pessimistic - out
 *              pointer to bool variable where the result will be stored
 *
 *
 * Return value :
 *
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getpessimistic(
        dbe_cfg_t* dbe_cfg,
        bool* p_pessimistic)
{
        bool found;

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_PESSIMISTIC,
                    p_pessimistic);

        if (!found) {
            *p_pessimistic = DBE_DEFAULT_PESSIMISTIC;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlocktimeout
 *
 * Returns the default lock time out values.
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *
 *
 *      p_pessimistic_lock_to - out
 *              Lock time out for pessimistic locks.
 *
 *      p_optimistic_lock_to - out
 *              Lock time out for optimistic locks.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getlocktimeout(
        dbe_cfg_t* dbe_cfg,
        long* p_pessimistic_lock_to,
        long* p_optimistic_lock_to)
{
        bool found;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_PESSIMISTIC_LOCK_TO,
                    p_pessimistic_lock_to);

        if (!found) {
            *p_pessimistic_lock_to = DBE_DEFAULT_PESSIMISTIC_LOCK_TO;
        }

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_OPTIMISTIC_LOCK_TO,
                    p_optimistic_lock_to);

        if (!found) {
            *p_optimistic_lock_to = DBE_DEFAULT_OPTIMISTIC_LOCK_TO;
        }

#ifdef DBE_LOCK_MSEC
        *p_pessimistic_lock_to = *p_pessimistic_lock_to * 1000;
        *p_optimistic_lock_to = *p_optimistic_lock_to * 1000;

        if (*p_pessimistic_lock_to > SU_MAXLOCKTIMEOUT) {
            su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_LOCKTIMEOUTTOOLARGE_DD,
                    *p_pessimistic_lock_to,
                    SU_MAXLOCKTIMEOUT);
        }
        if (*p_optimistic_lock_to > SU_MAXLOCKTIMEOUT) {
            su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_LOCKTIMEOUTTOOLARGE_DD,
                    *p_optimistic_lock_to,
                    SU_MAXLOCKTIMEOUT);
        }
#endif

        return(found);
}


/*##**********************************************************************\
 *
 *              dbe_cfg_gettablelocktimeout
 *
 * Returns table lock timeout value.
 * Used in sync conflict resolution.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_table_lock_to -
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
bool dbe_cfg_gettablelocktimeout(
        dbe_cfg_t* dbe_cfg,
        long* p_table_lock_to)
{
        bool found;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_TABLE_LOCK_TO,
                    p_table_lock_to);

        if (!found) {
            *p_table_lock_to = DBE_DEFAULT_TABLE_LOCK_TO;
        }

#ifdef DBE_LOCK_MSEC
        *p_table_lock_to = *p_table_lock_to * 1000;

        if (*p_table_lock_to > SU_MAXLOCKTIMEOUT) {
            su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_LOCKTIMEOUTTOOLARGE_DD,
                    *p_table_lock_to,
                    SU_MAXLOCKTIMEOUT);
        }
#endif

        return(found);
}


/*##**********************************************************************\
 *
 *              dbe_cfg_getlockhashsize
 *
 * Returns the lock table hash size.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_lockhashsize -
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
bool dbe_cfg_getlockhashsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_lockhashsize)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_LOCKHASHSIZE,
                    &l);

        if (found && l > 0) {
            *p_lockhashsize = (uint)l;
        } else {
            *p_lockhashsize = DBE_DEFAULT_LOCKHASHSIZE;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getmmelockhashsize
 *
 * Returns the lock table hash size for mme engine.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_lockhashsize -
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
bool dbe_cfg_getmmelockhashsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_lockhashsize)
{
        bool found;
        long l;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_MME_SECTION,
                    SU_MME_LOCKHASHSIZE,
                    &l);

        if (found && l > 0) {
            *p_lockhashsize = (uint)l;
        } else {
            *p_lockhashsize = DBE_DEFAULT_MMELOCKHASHSIZE;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getmmelockescalation
 *
 * Returns the flag whether lock escalation is enabled for MME.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_lockescalation -
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
bool dbe_cfg_getmmelockescalation(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_lockhashsize)
{
        bool found;
        bool flag;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_MME_SECTION,
                    SU_MME_LOCKESCALATION,
                    &flag);

        if (found) {
            *p_lockhashsize = flag;
        } else {
            *p_lockhashsize = DBE_DEFAULT_MMELOCKESCALATION;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getmmelockescalationlimit
 *
 * Returns the lock escalation limit for MME tables.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_lockescalationlimit -
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
bool dbe_cfg_getmmelockescalationlimit(
        dbe_cfg_t*  dbe_cfg,
        uint*       p_lockescalationlimit)
{
        bool found;
        long l;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_MME_SECTION,
                    SU_MME_LOCKESCALATIONLIMIT,
                    &l);

        if (found && l > 0) {
            *p_lockescalationlimit = (uint)l;
        } else {
            *p_lockescalationlimit = DBE_DEFAULT_MMELOCKESCALATIONLIMIT;
        }

        return(found);
}

bool dbe_cfg_getmmereleasememory(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_releasememory)
{
        bool found;
        bool flag;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_MME_SECTION,
                    SU_MME_RELEASEMEMORY,
                    &flag);

        if (found) {
            *p_releasememory = flag;
        } else {
            *p_releasememory = DBE_DEFAULT_MMERELEASEMEMORY;
        }

        return found;
}

bool dbe_cfg_getmmemaxcacheusage(
        dbe_cfg_t*  dbe_cfg,
        long*       p_maxcacheusage)
{
        bool found;
        long l;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_MME_SECTION,
                    SU_MME_MAXCACHEUSAGE,
                    &l);

        if (found) {
            *p_maxcacheusage = l;
        } else {
            *p_maxcacheusage = DBE_DEFAULT_MMEMAXCACHEUSAGE;
        }

        return found;
}

bool dbe_cfg_getmmemutexgranularity(
        dbe_cfg_t*  dbe_cfg,
        long*       p_mutexgranularity)
{
        bool found;
        long l;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_MME_SECTION,
                    SU_MME_MUTEXGRANULARITY,
                    &l);

        if (found) {
            *p_mutexgranularity = l;
        } else {
            *p_mutexgranularity = DBE_DEFAULT_MMEMUTEXGRANULARITY;
        }

        return found;
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getrelbufsize
 *
 * Returns the number of relations and views buffered in memory.
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *
 *
 *      p_relbufsize - out
 *              Buffer size.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getrelbufsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_relbufsize)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_RELBUFSIZE,
                    &l);

        if (found) {
            if (l == 0) {
                l = UINT_MAX;
            } else if (l < DBE_MIN_RELBUFSIZE) {
                l = DBE_MIN_RELBUFSIZE;
            }
            *p_relbufsize = (uint)l;
        } else {
            *p_relbufsize = DBE_DEFAULT_RELBUFSIZE;
        }

        return(found);
}


/*##**********************************************************************\
 *
 *              dbe_cfg_getidxpreflushperc
 *
 * Gets percentage of cache that is scanned for dirty blocks to be
 * preflushed.
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *
 *
 *      p_preflushpercent - out
 *              preflushpercent
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getidxpreflushperc(
        dbe_cfg_t* dbe_cfg,
        uint* p_preflushpercent)
{
        bool found;
        long l;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_PREFLUSHPERCENT,
                    &l);

        if (found) {
            if (l < 0) {
                l = 1;
            } else if (l > 90) {
                l = 90;
            }
            *p_preflushpercent = (uint)l;
        } else {
            *p_preflushpercent = DBE_DEFAULT_PREFLUSHPERCENT;
        }
        return(found);
}


/*##**********************************************************************\
 *
 *              dbe_cfg_getidxpreflushsamplesize
 *
 * Gets cache LRU area size in blocks that is scanned to check if
 * preflushing is needed.
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *
 *
 *      p_preflushsamplesize - out
 *              sample size in cache blocks
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getidxpreflushsamplesize(
        dbe_cfg_t* dbe_cfg,
        uint* p_preflushsamplesize)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_PREFLUSHSAMPLESIZE,
                    &l);

        if (found) {
            if (l < 0L) {
                l = 0L;
            } else if (l > 1000L) {
                l = 1000L;
            }
            *p_preflushsamplesize = (uint)l;
        } else {
            *p_preflushsamplesize = DBE_DEFAULT_PREFLUSHSAMPLESIZE;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getidxlastuseLRUskipperc
 *
 * Gets percentage of LRU queue that is skipped if slot is released
 * with DBE_CACHE_CLEANLASTUSE or DBE_CACHE_DIRTYLASTUSE mode
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *
 *
 *      p_skippercent - out
 *              skip percent
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getidxlastuseLRUskipperc(
        dbe_cfg_t* dbe_cfg,
        uint* p_skippercent)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_LASTUSELRUSKIPPERCENT,
                    &l);
        if (found) {
            if (l < 0L) {
                l = 0L;
            } else if (l > 100L) {
                l = 100L;
            }
            *p_skippercent = (uint)l;
        } else {
            *p_skippercent = DBE_DEFAULT_LASTUSELRUSKIPPERCENT;
        }
        return(found);
}
/*##**********************************************************************\
 *
 *              dbe_cfg_getidxpreflushdirtyperc
 *
 * Gets percentage of dirty blocks that must be found from samplesize
 * area before preflushing is started.
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *
 *
 *      p_preflushdirtyperc - out
 *              preflush dirty percent
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getidxpreflushdirtyperc(
        dbe_cfg_t* dbe_cfg,
        uint* p_preflushdirtyperc)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_PREFLUSHDIRTYPERCENT,
                    &l);

        if (found) {
            if (l < 0L) {
                l = 0L;
            } else if (l > 90L) {
                l = 90L;
            }
            *p_preflushdirtyperc = (uint)l;
        } else {
            *p_preflushdirtyperc = DBE_DEFAULT_PREFLUSHDIRTYPERCENT;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getidxcleanpagesearchlimit
 *
 * Gets cache LRU area size in blocks that is scanned to check if
 * preflushing is needed.
 *
 * Parameters :
 *
 *      dbe_cfg - in
 *
 *
 *      p_cleanpagesearchlimit - out
 *              sample size in cache blocks
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getidxcleanpagesearchlimit(
        dbe_cfg_t* dbe_cfg,
        uint* p_cleanpagesearchlimit)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_CLEANPAGESEARCHLIMIT,
                    &l);

        if (found) {
            if (l < 1L) {
                l = 1L;
            }
            *p_cleanpagesearchlimit = (uint)l;
        } else {
            *p_cleanpagesearchlimit = DBE_DEFAULT_CLEANPAGESEARCHLIMIT;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getidxmaxpagesemcount
 *
 * Gets max number of semaphores used for protecting
 * index file cache slots
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              config object
 *
 *      p_maxpagesemcnt - out
 *              pointer to variable where the value will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cfg_getidxmaxpagesemcount(
        dbe_cfg_t* dbe_cfg,
        ulong* p_maxpagesemcnt)
{
        bool found;
        long l;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_INDEXSECTION,
                    SU_DBE_MAXPAGESEM,
                    &l);

        if (found) {
            if (l < 10L) {
                l = 10L;
            }
            *p_maxpagesemcnt = (ulong)l;
        } else {
            *p_maxpagesemcnt = DBE_DEFAULT_MAXPAGESEM;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getdefaultstoreismemory
 *
 * Returns configured for default store is memory parameter.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_getdefaultstoreismemory -
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
bool dbe_cfg_getdefaultstoreismemory(
        dbe_cfg_t* dbe_cfg,
        bool* p_getdefaultstoreismemory)
{
        bool found;

        found = cfg_getbool(
                    dbe_cfg,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_DEFAULTSTOREISMEMORY,
                    p_getdefaultstoreismemory);

        if (!found) {
#if 0
            /* Diskless no longer uses M-tables by default.
               2003-11-12 apl */
            if (dbefile_diskless) {
                *p_getdefaultstoreismemory = TRUE;
            } else {
                *p_getdefaultstoreismemory = DBE_DEFAULT_DEFAULTSTOREISMEMORY;
            }
#else
            *p_getdefaultstoreismemory = DBE_DEFAULT_DEFAULTSTOREISMEMORY;
#endif
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getdurabilitylevel
 *
 * Returns configured value for durability level.
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_durabilitylevel -
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
bool dbe_cfg_getdurabilitylevel(
        dbe_cfg_t* dbe_cfg,
        dbe_durability_t* p_durabilitylevel)
{
        bool found;
        long l;

        found = cfg_getlong(
                    dbe_cfg,
                    SU_DBE_LOGSECTION,
                    SU_DBE_DURABILITYLEVEL,
                    &l);

        if (found) {
            switch ((dbe_durability_t)l) {
                case DBE_DURABILITY_RELAXED:
                case DBE_DURABILITY_ADAPTIVE:
                case DBE_DURABILITY_STRICT:
                    *p_durabilitylevel = (dbe_durability_t)l;
                    break;
                default:
                    ui_msg_warning(INI_MSG_ILLEGAL_VALUE_USSU,
                        (int)l, SU_DBE_LOGSECTION, SU_DBE_DURABILITYLEVEL,
                        DBE_DEFAULT_DURABILITYLEVEL);
                    *p_durabilitylevel = DBE_DEFAULT_DURABILITYLEVEL;
                    break;
            }
        } else {
            *p_durabilitylevel = DBE_DEFAULT_DURABILITYLEVEL;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getlogrelaxedmaxdelay
 *
 * Gets maximum wait delay in milliseconds before commit is written to disk.
 *
 * Parameters :
 *
 *      dbe_cfg - in, use
 *              configuration object
 *
 *      p_lfrelaxedmaxdelay - out, use
 *              pointer to variable where the value will be stored
 *
 * Return value :
 *      TRUE if the information is found from config. file or
 *      FALSE when the default is given.
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getlogrelaxedmaxdelay(
        dbe_cfg_t* dbe_cfg,
        ulong* p_lfrelaxedmaxdelay)
{
        bool found;

        found = su_inifile_getlong(
                dbe_cfg->cfg_file,
                SU_DBE_LOGSECTION,
                SU_DBE_LOGRELAXEDMAXDELAY,
                (long*)p_lfrelaxedmaxdelay);
        if (!found) {
            *p_lfrelaxedmaxdelay = DBE_DEFAULT_LOGRELAXEDMAXDELAY;
        } else {
            if (*p_lfrelaxedmaxdelay != 0L && *p_lfrelaxedmaxdelay < DBE_MIN_LOGRELAXEDMAXDELAY) {
                ui_msg_warning(INI_MSG_INVALID_RELAXEDMAXDELAY_VALUE_DSSD,
                                *p_lfrelaxedmaxdelay,
                                SU_DBE_LOGSECTION,
                                SU_DBE_LOGRELAXEDMAXDELAY,
                                (long)DBE_DEFAULT_LOGRELAXEDMAXDELAY);

                *p_lfrelaxedmaxdelay = DBE_DEFAULT_LOGRELAXEDMAXDELAY;
            }
        }
        return (found);
}


/*##**********************************************************************\
 *
 *              dbe_cfg_getcpdeletelog
 *
 * Gets info whether log files preceding the lates checkpoint
 * should be deleted after successful checkpoint creation
 *
 * Parameters :
 *
 *      dbe_cfg - use
 *
 *
 *      p_cpdeletelog - out
 *
 *
 * Return value :
 *      TRUE - info found from cfg file
 *      FALSE - default is given
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cfg_getcpdeletelog(
        dbe_cfg_t* dbe_cfg,
        bool* p_cpdeletelog)
{
        bool found;

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_CPDELETELOG,
                    p_cpdeletelog);

        if (!found) {
            *p_cpdeletelog = DBE_DEFAULT_CPDELETELOG;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_ishsbconfigured
 *
 * Checks if HSB is configured in configuration file.
 *
 * Parameters :
 *
 *      dbe_cfg -
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
bool dbe_cfg_ishsbconfigured(
        dbe_cfg_t* dbe_cfg)
{
        bool found;
        uint scanindex = 0;
        char* str;
        bool ishsb;

#ifdef SS_HSBG2
        if (!ss_migratehsbg2) {
            return(FALSE);
        }
#endif /* SS_HSBG2 */

        found = su_inifile_scanstring(
                    dbe_cfg->cfg_file,
                    SU_REP_HOTSTANDBYSECTION,
                    SU_REP_CONNECT,
                    "\n\0",
                    &scanindex,
                    &str);
        if (found) {
            ishsb = str[0] != '\0';
            SsMemFree(str);
        } else {
            ishsb = FALSE;
        }

        return(ishsb);
}

#ifdef SS_HSBG2
/*##**********************************************************************\
 *
 *              dbe_cfg_ishsbg2configured
 *
 * Checks if HSB G2 is configured in configuration file.
 *
 * Parameters :
 *
 *      dbe_cfg -
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
bool dbe_cfg_ishsbg2configured(
        dbe_cfg_t* dbe_cfg)
{
        bool found, ishsb = FALSE;

#ifdef SS_LICENSEINFO_V3
        if (!su_li3_ishotstandbysupp())
#else /* SS_LICENSEINFO_V3 */
        if (!su_li2_hotstandbysupp())
#endif /* SS_LICENSEINFO_V3 */
        {
            return(FALSE);
        }

        if (ss_migratehsbg2) {
            return(FALSE);
        }

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_REP_HOTSTANDBYSECTION,
                    SU_REP_HSBENABLED,
                    &ishsb);

        if (!found) {
            ishsb = FALSE;
        }

        return(ishsb);
}

#ifdef HSBG2_NOT_NEEDED
bool dbe_cfg_hsbg2safenessusedurabilitylevel(
        dbe_cfg_t* dbe_cfg)
{
        bool found;
        bool safenessusedurabilitylevel;

        found = su_inifile_getbool(
                    dbe_cfg->cfg_file,
                    SU_REP_HOTSTANDBYSECTION,
                    SU_REP_SAFENESSUSEDURABILITYLEVEL,
                    &safenessusedurabilitylevel);

        if (!found) {
            safenessusedurabilitylevel = FALSE;
        }

        return(safenessusedurabilitylevel);
}
#endif /* HSBG2_NOT_NEEDED */


#endif /* SS_HSBG2 */

/*#***********************************************************************\
 *
 *              cfg_cfgl_addidxfiles
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      cfgl -
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
static void cfg_cfgl_addidxfiles(
        dbe_cfg_t* dbe_cfg,
        su_cfgl_t* cfgl)
{
        int i;
        bool found;
        char* s;
        char* buf = SsMemAlloc(strlen(SU_DBE_FILESPEC) + (6 - 2 + 1));

        for (i = 1; i < SHRT_MAX; i++) {
            SsSprintf(buf, SU_DBE_FILESPEC, i);
            found = su_inifile_getstring(
                        dbe_cfg->cfg_file,
                        SU_DBE_INDEXSECTION,
                        buf,
                        &s);
            if (!found) {
                break;
            }
            su_cfgl_addstrparam(cfgl, (char *)SU_DBE_INDEXSECTION, buf, s, s, 0);
            SsMemFree(s);
        }
        ss_dassert(i < SHRT_MAX);
        if (i == 1) {
            /* No files specified. */
            char* param = SsMemAlloc(strlen(DBE_DEFAULT_INDEXFILENAME) + 30);
            SsSprintf(buf, SU_DBE_FILESPEC, 1);
            SsSprintf(param, "%s %ld", DBE_DEFAULT_INDEXFILENAME, SU_VFIL_SIZE_MAX);
            su_cfgl_addstrparam(
                cfgl,
                (char *)SU_DBE_INDEXSECTION,
                buf,
                param,
                param,
                SU_CFGL_ISDEFAULT);
            SsMemFree(param);
        }
        SsMemFree(buf);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_addtocfgl
 *
 *
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      cfgl -
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
void dbe_cfg_addtocfgl(
        dbe_cfg_t* dbe_cfg,
        su_cfgl_t* cfgl)
{
        int flags = 0;
        int constflags = SU_CFGL_ISCONST|SU_CFGL_ISADVANCED;
        int advancedflags = SU_CFGL_ISADVANCED;
        char buf[2];

        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_MAXOPENFILES,
            (long)DBE_DEFAULT_MAXOPENFILES,
            advancedflags);
        su_cfgl_addstr(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_BACKUPDIR,
            DBE_DEFAULT_BACKUPDIR,
            flags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_BACKUP_BLOCKSIZE,
            (long)DBE_DEFAULT_BACKUP_BLOCKSIZE,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_BACKUP_STEPSTOSKIP,
            (long)DBE_DEFAULT_BACKUP_STEPSTOSKIP,
            advancedflags);
        su_cfgl_addbool(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_BACKUPCOPYLOG,
            DBE_DEFAULT_BACKUPCOPYLOG,
            advancedflags);
        su_cfgl_addbool(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_BACKUPDELETELOG,
            DBE_DEFAULT_BACKUPDELETELOG,
            advancedflags);
        su_cfgl_addbool(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_BACKUPCOPYINIFLE,
            DBE_DEFAULT_BACKUPCOPYINIFILE,
            advancedflags);
        su_cfgl_addbool(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_READONLY,
            DBE_DEFAULT_READONLY,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_CPINTERVAL,
            (long)DBE_DEFAULT_CPINTERVAL,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_MERGEINTERVAL,
            (long)cfg_defaultmergeinterval(dbe_cfg),
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_SEQSEALIMIT,
            (long)DBE_DEFAULT_SEQSEALIMIT,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_SEABUFLIMIT,
            (long)DBE_DEFAULT_SEABUFLIMIT,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_TRXBUFSIZE,
            (long)cfg_defaulttrxbufsize(dbe_cfg),
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_PESSIMISTIC_LOCK_TO,
            DBE_DEFAULT_PESSIMISTIC_LOCK_TO,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_RELBUFSIZE,
            DBE_DEFAULT_RELBUFSIZE,
            advancedflags);
        su_cfgl_addbool(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_GENERALSECTION,
            SU_DBE_PESSIMISTIC,
            DBE_DEFAULT_PESSIMISTIC,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_INDEXSECTION,
            SU_DBE_BLOCKSIZE,
            (long)DBE_DEFAULT_INDEXBLOCKSIZE,
            constflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_INDEXSECTION,
            SU_DBE_EXTENDINCR,
            (long)DBE_DEFAULT_INDEXEXTENDINCR,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_INDEXSECTION,
            SU_DBE_CACHESIZE,
            (long)DBE_DEFAULT_INDEXCACHESIZE,
            flags);
        cfg_cfgl_addidxfiles(dbe_cfg, cfgl);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_INDEXSECTION,
            SU_DBE_READAHEADSIZE,
            (long)DBE_DEFAULT_READAHEADSIZE,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_INDEXSECTION,
            SU_DBE_PREFLUSHPERCENT,
            (long)DBE_DEFAULT_PREFLUSHPERCENT,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_INDEXSECTION,
            SU_DBE_MAXPAGESEM,
            (long)DBE_DEFAULT_MAXPAGESEM,
            advancedflags);
        su_cfgl_addbool(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_LOGSECTION,
            SU_DBE_LOGENABLED,
            DBE_DEFAULT_LOGENABLED,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_LOGSECTION,
            SU_DBE_BLOCKSIZE,
            (long)DBE_DEFAULT_LOGBLOCKSIZE,
            constflags);
        su_cfgl_addstr(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_LOGSECTION,
            SU_DBE_LOGFILETEMPLATE,
            DBE_DEFAULT_LOGFNAMETEMPLATE,
            advancedflags);
        su_cfgl_addstr(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_LOGSECTION,
            SU_DBE_LOGDIR,
            DBE_DEFAULT_LOGDIR,
            advancedflags);
        buf[0] = DBE_DEFAULT_LOGDIGITTEMPLATE;
        buf[1] = '\0';
        su_cfgl_addstr(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_LOGSECTION,
            SU_DBE_LOGDIGITTEMPLATE,
            buf,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_LOGSECTION,
            SU_DBE_LOGMINSPLITSIZE,
            (long)DBE_DEFAULT_LOGMINSPLITSIZE,
            advancedflags);
        su_cfgl_addlong(
            cfgl,
            dbe_cfg->cfg_file,
            SU_DBE_LOGSECTION,
            SU_DBE_LOGWRITEMODE,
            (long)DBE_DEFAULT_LOGWRITEMODE,
            advancedflags);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getnumiothreads
 *
 * Gets configuration value for the number of IO-threads
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_limit -
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
bool dbe_cfg_getnumiothreads(
        dbe_cfg_t* dbe_cfg,
        long* p_limit)
{
        bool found;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_NUMIOTHREADS,
                    p_limit);

        if (!found) {
            long limit = DBE_DEFAULT_NUMIOTHREADS;
            size_t cachesize;

            if (dbe_cfg_getidxcachesize(dbe_cfg, &cachesize)) {
                /* cache size up to 1GB increases numiothreads default */
                if (cachesize > (size_t)512U * 1024U * 1024U) {
                    cachesize = (size_t)512U * 1024U * 1024U;
                }
                limit += (long)(cachesize / (1024UL * 1024UL * 100U));
            }
            *p_limit = limit;
        } else if (*p_limit < 1 || *p_limit > CFG_MAX_NUMIOTHREADS) {
            *p_limit = DBE_DEFAULT_NUMIOTHREADS;
        }

        return(found);
}

/*##**********************************************************************\
 *
 *              dbe_cfg_getnumwriteriothreads
 *
 * Gets configuration value for the number of Writer IO-threads
 *
 * Parameters :
 *
 *      dbe_cfg -
 *
 *
 *      p_limit -
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
bool dbe_cfg_getnumwriteriothreads(
        dbe_cfg_t* dbe_cfg,
        long* p_limit)
{
        bool found;
        long numiothreads;

        found = su_inifile_getlong(
                    dbe_cfg->cfg_file,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_NUMWRITERIOTHREADS,
                    p_limit);

        if (!found) {
            long limit = DBE_DEFAULT_NUMWRITERIOTHREADS;
            size_t cachesize;

            if (dbe_cfg_getidxcachesize(dbe_cfg, &cachesize)) {
                /* cache size up to 1GB increases numwriteriothreads default */
                if (cachesize > (size_t)512U * 1024U * 1024U) {
                    cachesize = (size_t)512U * 1024U * 1024U;
                }
                limit += (long)(cachesize / (1024UL * 1024UL * 100U));
            }
            *p_limit = limit;
        } else if (*p_limit < 0 || *p_limit > CFG_MAX_NUMIOTHREADS) {
            *p_limit = DBE_DEFAULT_NUMWRITERIOTHREADS;
        }
        dbe_cfg_getnumiothreads(dbe_cfg, &numiothreads);
        if (numiothreads <= *p_limit) {
            *p_limit = numiothreads - 4;
            if (*p_limit < 1) {
                *p_limit = DBE_DEFAULT_NUMWRITERIOTHREADS;
            }
        }
        if (numiothreads == 1) {
            *p_limit = 0;
        }
        return(found);
}

/*##**********************************************************************\
 *
 *      dbe_cfg_getmmememlimit
 *
 * returns mme memory limit
 *
 * Parameters:
 *      cfg - in, use
 *          dbe config object
 *
 *      memlimit - out, use
 *          pointer to 64-bit integer (ss_int8_t) where the
 *          limit value is stored (0 = no limit)
 *
 * Return value:
 *      TRUE - configuration found
 *      FALSE - config not found, using default
 *
 * Limitations:
 *
 * Globals used:
 */
bool dbe_cfg_getmmememlimit(
        dbe_cfg_t* cfg,
        ss_int8_t* memlimit)
{
        bool found =
            su_param_getint8(cfg->cfg_file,
                             SU_MME_SECTION,
                             SU_MME_MEMORYLIMIT,
                             memlimit);
        if (!found) {
            SsInt8SetInt4(memlimit, DBE_DEFAULT_MMEMEMLIMIT);
        }
        return (found);
}

bool dbe_cfg_getmmememlowpercent(
        dbe_cfg_t* cfg,
        long* low_percent)
{
        bool found = su_inifile_getlong(cfg->cfg_file,
                                        SU_MME_SECTION,
                                        SU_MME_LOWWATERMARKPERC,
                                        low_percent);
        if (!found) {
            *low_percent = DBE_DEFAULT_MMEMEMLOWPERC;
        }
        return (found);
}

/* EOF */

