/*************************************************************************\
**  source       * dbe7cfg.h
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


#ifndef DBE7CFG_H
#define DBE7CFG_H

#include <ssc.h>
#include <ssstdlib.h>
#include <ssint8.h>

#include <su0inifi.h>
#include <su0parr.h>
#include <su0cfgl.h>
#include <su0param.h>

#include "dbe0type.h"

/* A key value length used to calculate merge limit. If the actual key
 * value is longer than this value, it is counted as multiple key values.
 * NOTE! Started to use actual number of key values for merge.
 */
#define DBE_CFG_MERGEKEYLEN 50

#ifdef SS_GEOS
# define DBE_CFG_MAXINDEXBLOCKSIZE      8192
#else
# define DBE_CFG_MAXINDEXBLOCKSIZE      (64L*1024L)
#endif

/* Note #define for DBE_DEFAULT_INDEXCACHESIZE must be a plain
   integer literal because it is stringized usin C preprocessor
   to generate a init value for parameter manager!
*/
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
#  define DBE_DEFAULT_INDEXCACHESIZE        67108864 /* need to be a real value, not (64*1024*1024L) */
#elif defined(SS_DEBUG)
#  define DBE_DEFAULT_INDEXCACHESIZE        524288
#elif defined(SS_SMALLSYSTEM)
#  define DBE_DEFAULT_INDEXCACHESIZE        524288
#elif defined(SS_UNIX) || defined(SS_NT)
#  define DBE_DEFAULT_INDEXCACHESIZE        33554432
#elif defined(SS_WIN) || defined(SS_DOS) 
#  define DBE_DEFAULT_INDEXCACHESIZE        524288
#else /* other */
#error Unknown environment!
#endif

#define DBE_DEFAULT_INDEXBLOCKSIZE      (8192)
#define DBE_DEFAULT_LOGBLOCKSIZE        (16 * 1024L)
#define DBE_DEFAULT_BACKUP_BLOCKSIZE    (64UL * 1024UL)
#define DBE_DEFAULT_CPINTERVAL          50000
#define DBE_DEFAULT_CPMINTIME           300
#define DBE_DEFAULT_WRITEFLUSHMODE      SS_BFLUSH_NORMAL
#define DBE_DEFAULT_NUMIOTHREADS        5
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
#define DBE_DEFAULT_LOCKHASHSIZE        1000000
#else
#define DBE_DEFAULT_LOCKHASHSIZE        1000
#endif
#define DBE_DEFAULT_PESSIMISTIC_LOCK_TO 30L
#define DBE_DEFAULT_OPTIMISTIC_LOCK_TO  0L
#define DBE_DEFAULT_LOGENABLED          TRUE
# if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
#define DBE_DEFAULT_PESSIMISTIC         TRUE
# else
#define DBE_DEFAULT_PESSIMISTIC         FALSE
# endif
#define DBE_DEFAULT_NUMWRITERIOTHREADS  1
#ifdef SS_SMALLSYSTEM
#define DBE_DEFAULT_INDEXEXTENDINCR     10
#else
#define DBE_DEFAULT_INDEXEXTENDINCR     500
#endif
#define DBE_DEFAULT_READAHEADSIZE       4
#define DBE_CFG_MINLOGBLOCKSIZE 512

typedef struct dbe_cfg_st dbe_cfg_t;

typedef struct dbe_filespec_st dbe_filespec_t;

extern bool dbe_cfg_newkeycheck;
extern bool dbe_cfg_shortkeyopt;
extern bool dbe_cfg_newtrxwaitreadlevel;
extern bool dbe_cfg_relaxedreadlevel;
extern bool dbe_cfg_usepessimisticgate;
extern bool dbe_cfg_versionedpessimisticreadcommitted;
extern bool dbe_cfg_versionedpessimisticrepeatableread;
extern long dbe_cfg_readlevelmaxtime;
extern int  dbe_cfg_maxmergeparts;
extern bool dbe_cfg_splitpurge;
extern bool dbe_cfg_mergecleanup;
extern bool dbe_cfg_usenewbtreelocking;
extern bool dbe_cfg_userandomkeysampleread;
extern bool dbe_cfg_singledeletemark;
extern bool dbe_cfg_startupforcemerge;
extern bool dbe_cfg_fastdeadlockdetect;
extern bool dbe_cfg_deadlockdetectmaxdepth;
extern bool dbe_cfg_deadlockdetectmaxlocks;
extern bool dbe_cfg_physicaldroptable;
extern int  dbe_cfg_relaxedbtreelocking;

void dbe_cfg_register_su_params(
        dbe_cfg_t* dbe_cfg);

void dbe_cfg_register_mme_memlimit(
        dbe_cfg_t* cfg,
        su_param_set_cb_t change_callback);

dbe_cfg_t* dbe_cfg_init(
        su_inifile_t* cfg_file);

dbe_cfg_t* dbe_cfg_init_reentrant(
        su_inifile_t* cfg_file);

void dbe_cfg_done(
        dbe_cfg_t* dbe_cfg);

su_inifile_t* dbe_cfg_getinifile(
        dbe_cfg_t* dbe_cfg);

bool dbe_cfg_getwriteflushmode(
        dbe_cfg_t* dbe_cfg,
        int* p_writeflushmode);

bool dbe_cfg_getmaxopenfiles(
        dbe_cfg_t* dbe_cfg,
        uint* p_maxopen);

void dbe_cfg_settmpidxblocksize(
        dbe_cfg_t* dbe_cfg,
        size_t blocksize);

bool dbe_cfg_getidxblocksize(
        dbe_cfg_t* dbe_cfg,
        size_t* p_blocksize);

bool dbe_cfg_getlogblocksize(
        dbe_cfg_t* dbe_cfg,
        size_t* p_blocksize);

bool dbe_cfg_getlogmaxwritequeuerecords(
        dbe_cfg_t* dbe_cfg,
        long* p_maxwritequeuerecords);

bool dbe_cfg_getlogmaxwritequeuebytes(
        dbe_cfg_t* dbe_cfg,
        long* p_maxwritequeuebytes);

bool dbe_cfg_getlogwritequeueflushlimit(
        dbe_cfg_t* dbe_cfg,
        long* p_writequeueflushlimit);

bool dbe_cfg_getlogdelaymeswait(
        dbe_cfg_t* dbe_cfg,
        bool* p_delaymeswait);

bool dbe_cfg_getlogfileflush(
        dbe_cfg_t* dbe_cfg,
        bool* p_noflush);

bool dbe_cfg_getlogsyncwrite(
        dbe_cfg_t* dbe_cfg,
        bool* p_syncwrite);

#ifdef FSYNC_OPT
bool dbe_cfg_getindexfileflush(
        dbe_cfg_t* dbe_cfg,
        bool* p_fileflush);
#endif /* FSYNC_OPT */

bool dbe_cfg_getidxextendincr(
        dbe_cfg_t* dbe_cfg,
        uint* p_extendincr);

bool dbe_cfg_getlogextendincr(
        dbe_cfg_t* dbe_cfg,
        uint* p_extendincr);

bool dbe_cfg_getlogwritebuffersize(
        dbe_cfg_t* dbe_cfg,
        uint* p_buffersize);

bool dbe_cfg_getidxmaxseqalloc(
        dbe_cfg_t* dbe_cfg,
        uint* p_maxseqalloc);

bool dbe_cfg_getidxcachesize(
        dbe_cfg_t* dbe_cfg,
        size_t* p_cachesize);

bool dbe_cfg_getidxfilespecs(
        dbe_cfg_t* dbe_cfg,
        su_pa_t* idxfilespecs);

#ifdef IO_OPT

bool dbe_cfg_getindexdirectio(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_directio);

bool dbe_cfg_getlogdirectio(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_directio);

#endif

#ifdef FSYNC_OPT
bool dbe_cfg_getsyncwrite(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_syncwrite);
#else /* FSYNC_OPT */
bool dbe_cfg_getsynchronizedwrite(
        dbe_cfg_t* dbe_cfg,
        bool* p_synchronizedwrite);
#endif /* FSYNC_OPT */

bool dbe_cfg_getfreelistreserve(
        dbe_cfg_t* dbe_cfg,
        long* p_reservesize);

bool dbe_cfg_getfreelistgloballysorted(
        dbe_cfg_t* dbe_cfg,
        bool* p_globallysorted);

void dbe_cfg_setfreelistgloballysorted(
        dbe_cfg_t* dbe_cfg,
        bool globallysorted);

bool dbe_cfg_getfilespecs(
        dbe_cfg_t* dbe_cfg,
        su_pa_t* filespecs,
        char* section_name);

bool dbe_cfg_getlogenabled(
        dbe_cfg_t* cfg,
        bool* p_logenabled);

bool dbe_cfg_getloggroupcommitqueue(
        dbe_cfg_t* cfg,
        bool* p_groupcommitqueue);

bool dbe_cfg_getlogfilenametemplate(
        dbe_cfg_t* dbe_cfg,
        char** p_lfnametemplate);

bool dbe_cfg_getlogdir(
        dbe_cfg_t* dbe_cfg,
        char** p_lfnametemplate);

bool dbe_cfg_getlogdigittemplate(
        dbe_cfg_t* dbe_cfg,
        char* p_digittempl);

bool dbe_cfg_getlogfileminsplitsize(
        dbe_cfg_t* dbe_cfg,
        ulong* p_lfminsplitsize);

bool dbe_cfg_getlogcommitmaxwait(
        dbe_cfg_t* dbe_cfg,
        ulong* p_lfcommitmaxwait);

bool dbe_cfg_getlogwritemode(
        dbe_cfg_t* dbe_cfg,
        int* p_writemode);

bool dbe_cfg_getbackupdir(
        dbe_cfg_t* dbe_cfg,
        char** p_backupdir);

bool dbe_cfg_getbackupcopylog(
        dbe_cfg_t* dbe_cfg,
        bool* p_backupcopylog);

bool dbe_cfg_getnetbackupcopylog(
        dbe_cfg_t* dbe_cfg,
        bool* p_netbackupcopylog);

bool dbe_cfg_getbackupdeletelog(
        dbe_cfg_t* dbe_cfg,
        bool* p_backupdeletelog);

bool dbe_cfg_getnetbackupdeletelog(
        dbe_cfg_t* dbe_cfg,
        bool* p_netbackupdeletelog);

bool dbe_cfg_getbackupcopyinifile(
        dbe_cfg_t* dbe_cfg,
        bool* p_copyinifile);

bool dbe_cfg_getnetbackupcopyinifile(
        dbe_cfg_t* dbe_cfg,
        bool* p_copyinifile);

bool dbe_cfg_getbackupcopysolmsgout(
        dbe_cfg_t* dbe_cfg,
        bool* p_copysolmsgout);

bool dbe_cfg_getnetbackupcopysolmsgout(
        dbe_cfg_t* dbe_cfg,
        bool* p_copysolmsgout);

bool dbe_cfg_getcpdeletelog(
        dbe_cfg_t* dbe_cfg,
        bool* p_cpdeletelog);

bool dbe_cfg_getreadonly(
        dbe_cfg_t* dbe_cfg,
        bool* p_readonly);

bool dbe_cfg_getdisableidlemerge(
        dbe_cfg_t* dbe_cfg,
        bool* p_disableidlemerge);

bool dbe_cfg_getcheckescalatelimit(
        dbe_cfg_t* dbe_cfg,
        long* p_limit);

bool dbe_cfg_getreadescalatelimit(
        dbe_cfg_t* dbe_cfg,
        long* p_limit);

bool dbe_cfg_getlockescalatelimit(
        dbe_cfg_t* dbe_cfg,
        long* p_limit);

bool dbe_cfg_getallowlockebounce(
        dbe_cfg_t* dbe_cfg,
        bool* p_allowbounce);

bool dbe_cfg_getmergeinterval(
        dbe_cfg_t* dbe_cfg,
        long* p_mergeinterval);

bool dbe_cfg_getquickmergeinterval(
        dbe_cfg_t* dbe_cfg,
        long* p_quickmergeinterval);

bool dbe_cfg_getsplitmerge(
        dbe_cfg_t* dbe_cfg,
        bool* p_splitmerge);

bool dbe_cfg_getfakemerge(
        dbe_cfg_t* dbe_cfg,
        bool* p_fakemerge);

bool dbe_cfg_getmergemintime(
        dbe_cfg_t* dbe_cfg,
        long* p_mergemintime);

bool dbe_cfg_getcpinterval(
        dbe_cfg_t* dbe_cfg,
        long* p_cpinterval);

bool dbe_cfg_getcpmintime(
        dbe_cfg_t* dbe_cfg,
        long* p_cpmintime);

bool dbe_cfg_getseqsealimit(
        dbe_cfg_t* dbe_cfg,
        ulong* p_seqsealimit);

bool dbe_cfg_getseabuflimit(
        dbe_cfg_t* dbe_cfg,
        long* p_seabuflimit);

bool dbe_cfg_gettrxbufsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_trxbufsize);

bool dbe_cfg_getearlyvld(
        dbe_cfg_t* dbe_cfg,
        bool* p_earlyvld);

bool dbe_cfg_getreadaheadsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_readaheadsize);

#ifdef SS_BLOCKINSERT
bool dbe_cfg_getblockinsertsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_blockinsertsize);
#endif /* SS_BLOCKINSERT */

bool dbe_cfg_getuseiothreads(
        dbe_cfg_t* dbe_cfg,
        bool* p_useiothreads);

bool dbe_cfg_getpessimistic(
        dbe_cfg_t* dbe_cfg,
        bool* p_pessimistic);

bool dbe_cfg_getlocktimeout(
        dbe_cfg_t* dbe_cfg,
        long* p_pessimistic_lock_to,
        long* p_optimistic_lock_to);

bool dbe_cfg_gettablelocktimeout(
        dbe_cfg_t* dbe_cfg,
        long* p_table_lock_to);

bool dbe_cfg_getlockhashsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_lockhashsize);

bool dbe_cfg_getmmelockhashsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_lockhashsize);

bool dbe_cfg_getmmelockescalation(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_lockescalation);

bool dbe_cfg_getmmelockescalationlimit(
        dbe_cfg_t*  dbe_cfg,
        uint*       p_lockescalationlimit);

bool dbe_cfg_getmmereleasememory(
        dbe_cfg_t*  dbe_cfg,
        bool*       p_releasememory);

bool dbe_cfg_getmmemaxcacheusage(
        dbe_cfg_t*  dbe_cfg,
        long*       p_maxcacheusage);

bool dbe_cfg_getmmemutexgranularity(
        dbe_cfg_t*  dbe_cfg,
        long*       p_mutexgranularity);

bool dbe_cfg_getrelbufsize(
        dbe_cfg_t* dbe_cfg,
        uint* p_relbufsize);

bool dbe_cfg_getidxpreflushperc(
        dbe_cfg_t* dbe_cfg,
        uint* p_preflushpercent);

bool dbe_cfg_getidxlastuseLRUskipperc(
        dbe_cfg_t* dbe_cfg,
        uint* p_skippercent);

bool dbe_cfg_getidxpreflushsamplesize(
        dbe_cfg_t* dbe_cfg,
        uint* p_preflushsamplesize);

bool dbe_cfg_getidxpreflushdirtyperc(
        dbe_cfg_t* dbe_cfg,
        uint* p_preflushdirtyperc);

bool dbe_cfg_getidxcleanpagesearchlimit(
        dbe_cfg_t* dbe_cfg,
        uint* p_cleanpagesearchlimit);

bool dbe_cfg_getidxmaxpagesemcount(
        dbe_cfg_t* dbe_cfg,
        ulong* p_maxpagesemcnt);

bool dbe_cfg_getdefaultstoreismemory(
        dbe_cfg_t* dbe_cfg,
        bool* p_defaultstoreismemory);

bool dbe_cfg_getdurabilitylevel(
        dbe_cfg_t* dbe_cfg,
        dbe_durability_t* p_durabilitylevel);

bool dbe_cfg_getlogrelaxedmaxdelay(
        dbe_cfg_t* dbe_cfg,
        ulong* p_lfrelaxedmaxdelay);

dbe_filespec_t* dbe_filespec_init(
        char *name,
        ss_int8_t maxsize,
        uint diskno);

void dbe_filespec_done(
        dbe_filespec_t* filespec);

char* dbe_filespec_getname(
        dbe_filespec_t* filespec);

ss_int8_t dbe_filespec_getmaxsize(
        dbe_filespec_t* filespec);

uint dbe_filespec_getdiskno(
        dbe_filespec_t* filespec);

void dbe_cfg_addtocfgl(
        dbe_cfg_t* dbe_cfg,
        su_cfgl_t* cfgl);

bool dbe_cfg_ishsbconfigured(
        dbe_cfg_t* dbe_cfg);

#ifdef SS_HSBG2

bool dbe_cfg_ishsbg2configured(
        dbe_cfg_t* dbe_cfg);

/*
bool dbe_cfg_hsbg2safenessusedurabilitylevel(
        dbe_cfg_t* dbe_cfg);
*/

#endif /* SS_HSBG2 */

bool dbe_cfg_getbackup_blocksize(
        dbe_cfg_t* dbe_cfg,
        long* p_backup_blocksize);

bool dbe_cfg_getbackup_stepstoskip(
        dbe_cfg_t* dbe_cfg,
        long* p_backup_stepstoskip);

bool dbe_cfg_getnumiothreads(
        dbe_cfg_t* dbe_cfg,
        long* p_limit);

bool dbe_cfg_getnumwriteriothreads(
        dbe_cfg_t* dbe_cfg,
        long* p_limit);

bool dbe_cfg_getmmememlimit(
        dbe_cfg_t* cfg,
        ss_int8_t* memlimit);

bool dbe_cfg_getmmememlowpercent(
        dbe_cfg_t* cfg,
        long* low_percent);

void dbe_cfg_registerfilespecs(
        dbe_cfg_t* dbe_cfg);

#endif /* DBE7CFG_H */
