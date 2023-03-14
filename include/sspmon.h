/*************************************************************************\
**  source       * sspmon.h
**  directory    * ss
**  description  * Performance monitoring routines.
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


#ifndef SSPMON_H
#define SSPMON_H

#include "ssc.h"

#define SS_PMON_ADD(v)          ss_pmon.pm_values[v]++
#define SS_PMON_REMOVE(v)       ss_pmon.pm_values[v]--
#define SS_PMON_SET(v,n)        ss_pmon.pm_values[v] = (n)
#define SS_PMON_ADD_N(v,n)      ss_pmon.pm_values[v] += (n)

#if defined(SS_BETA)
# define SS_PMON_ADD_BETA(v)    SS_PMON_ADD(v)
# define SS_PMON_ADD_N_BETA(v,n) SS_PMON_ADD_N(v,n)
#else
# define SS_PMON_ADD_BETA(v)
# define SS_PMON_ADD_N_BETA(v,n)
#endif

/* Index values for different monitored entities.
 *
 * WARNING! When adding new values, all files referencing to ss_pmon
 *          variable must be recompiled. This is because all data is
 *          in static tables, and the table positions change when
 *          SS_PMON_MAXVALUES changes.
 *
 * ADD CORRECT MAKEFILE DEPENDENCIES TO THIS HEADER FROM FILES USING PMON.
 */
typedef enum {
        SS_PMON_FILEOPEN,
        SS_PMON_FILEREAD,
        SS_PMON_FILEWRITE,
        SS_PMON_FILEAPPEND,
        SS_PMON_FILEFLUSH,
        SS_PMON_FILELOCK,
        SS_PMON_CACHEFIND,
        SS_PMON_CACHEFILEREAD,
        SS_PMON_CACHEFILEWRITE,
        SS_PMON_CACHEPREFETCH,
        SS_PMON_CACHEPREFETCHWAIT,
        SS_PMON_CACHEPREFLUSH,
        SS_PMON_CACHELRUWRITE,
        SS_PMON_CACHESLOTWAIT,
        SS_PMON_CACHESLOTREPLACE,
        SS_PMON_CACHEWRITESTORAGELEAF,
        SS_PMON_CACHEWRITESTORAGEINDEX,
        SS_PMON_CACHEWRITEBONSAILEAF,
        SS_PMON_CACHEWRITEBONSAIINDEX,

        SS_PMON_RPCMESSAGE,
        SS_PMON_RPCPHYSREAD,
        SS_PMON_RPCPHYSWRITE,
        SS_PMON_RPCUNCOMPRESSED,
        SS_PMON_RPCCOMPRESSED,
        SS_PMON_COMSELEMPTY,
        SS_PMON_COMSELFOUND,
        SS_PMON_SQLPREPARE,
        SS_PMON_SQLEXECUTE,
        SS_PMON_SQLFETCH,

        SS_PMON_DBEINSERT,
        SS_PMON_DBEDELETE,
        SS_PMON_DBEUPDATE,
        SS_PMON_DBEFETCH,
        SS_PMON_DBEFETCHUNIQUEFOUND,
        SS_PMON_DBEFETCHUNIQUENOTFOUND,
        SS_PMON_DBEDDOP,

        SS_PMON_PROCEXEC,
        SS_PMON_TRIGEXEC,
        SS_PMON_SAINSERT,
        SS_PMON_SADELETE,
        SS_PMON_SAUPDATE,
        SS_PMON_SAFETCH,

        SS_PMON_TRANSCOMMIT,
        SS_PMON_TRANSABORT,
        SS_PMON_TRANSROLLBACK,
        SS_PMON_TRANSRDONLY,
        SS_PMON_TRANSBUFCNT,
        SS_PMON_TRANSBUFCLEAN,
        SS_PMON_TRANSBUFCLEANLEVEL,
        SS_PMON_TRANSBUFABORTLEVEL,
        SS_PMON_TRANSBUFADDED,
        SS_PMON_TRANSBUFREMOVED,
        SS_PMON_TRANSVLDCNT,
        SS_PMON_TRANSACTCNT,
        SS_PMON_TRANSREADLEVEL,
        SS_PMON_INDWRITE,
        SS_PMON_INDWRITESAFTERMERGE,
        SS_PMON_LOGWRITES,
        SS_PMON_LOGFILEWRITE,
        SS_PMON_LOGWRITESAFTERCP,
        SS_PMON_LOGSIZE,
        SS_PMON_SRCHNACTIVE,
        SS_PMON_DBSIZE,
        SS_PMON_DBFREESIZE,
        SS_PMON_MEMSIZE,
        SS_PMON_MERGEQUICKSTEP,
        SS_PMON_MERGESTEP,
        SS_PMON_MERGEPURGESTEP,
        SS_PMON_MERGEUSERSTEP,
        SS_PMON_MERGEOPER,
        SS_PMON_MERGECLEANUP,
        SS_PMON_MERGEACT,
        SS_PMON_MERGEWRITES,
        SS_PMON_MERGEFILEWRITE,
        SS_PMON_MERGEFILEREAD,
        SS_PMON_MERGELEVEL,
        SS_PMON_BACKUPSTEP,
        SS_PMON_BACKUPACT,
        SS_PMON_CHECKPOINTACT,
        SS_PMON_CHECKPOINTCOUNT,
        SS_PMON_CHECKPOINTFILEWRITE,
        SS_PMON_CHECKPOINTFILEREAD,
        SS_PMON_ESTSAMPLESREAD,

        SS_PMON_SYNCMSGREPLFORWARD,
        SS_PMON_SYNCMSGREPLGEREPLY,
        SS_PMON_SYNCMSGREPLEXEC,
        SS_PMON_SYNCMSGMASTREAD,
        SS_PMON_SYNCMSGMASTEXEC,
        SS_PMON_SYNCMSGMASTWRITE,
        SS_PMON_SYNCSUBSCRIPTIONS,

#if defined(SS_BETA)
        SS_PMON_RELCUROPEN,
        SS_PMON_INDSEARESET,
#endif /* SS_BETA */
        SS_PMON_LOGFLUSHES_LOGICAL,
        SS_PMON_LOGFLUSHES_PHYSICAL,
        SS_PMON_LOGGROUPCOMMIT_WAKEUPS,
        SS_PMON_LOGFLUSHES_FULLPAGES,
        SS_PMON_LOGWAITFLUSH,
        SS_PMON_LOGMAXWRITEQUEUERECORDS,
        SS_PMON_LOGMAXWRITEQUEUEBYTES,
        SS_PMON_LOGWRITEQUEUERECORDS,
        SS_PMON_LOGWRITEQUEUEBYTES,
        SS_PMON_LOGWRITEQUEUEPENDINGBYTES,
        SS_PMON_LOGWRITEQUEUEADD,
        SS_PMON_LOGWRITEQUEUEWRITE,

        SS_PMON_HSB_OPCOUNT,
        SS_PMON_HSB_COMMITCOUNT,
        SS_PMON_HSB_PACKETCOUNT,
        SS_PMON_HSB_FLUSHCOUNT,
        SS_PMON_HSB_CACHEBYTES,
        SS_PMON_HSB_CACHEOPS,
        SS_PMON_HSB_FLUSHERBYTES,
        SS_PMON_HSB_NOTSENTBYTES,
        SS_PMON_HSB_ACKGROUP_COUNT,
        SS_PMON_HSB_STATE,
        SS_PMON_HSB_CPWAITMES,
        SS_PMON_HSB_SEC_QUEUES,
        SS_PMON_HSB_LOGSPM_REQCOUNT,
        SS_PMON_HSB_LOGSPM_WAITCOUNT,
        SS_PMON_HSB_LOGSPM_FREESPACE,
        SS_PMON_HSB_CATCHUPSPM_REQCOUNT,
        SS_PMON_HSB_CATCHUPSPM_WAITCOUNT,
        SS_PMON_HSB_CATCHUPSPM_FREESPACE,
        SS_PMON_HSB_ALONEBYTES_FREESPACE,

#if defined(SS_BETA)
        SS_PMON_RELCUR_CREATE,
        SS_PMON_RELCUR_RESETFULL,
        SS_PMON_RELCUR_RESETSIMPLE,
        SS_PMON_RELCUR_CONSTR,
        SS_PMON_RELCUR_CONSTRISSIMPLE,
        SS_PMON_RELCUR_NEWESTIMATE,
        SS_PMON_DBE_TRX_NEEDWRITECHECK,
        SS_PMON_DBE_TRX_ONLYDELETEMARK,
        SS_PMON_DBE_TRX_ESCALATEWRITECHECK,
#endif /* SS_BETA */
        SS_PMON_SS_THREADCOUNT,
        SS_PMON_WAITREADLEVEL_COUNT,
        SS_PMON_DBE_LOCK_OK,
        SS_PMON_DBE_LOCK_TIMEOUT,
        SS_PMON_DBE_LOCK_DEADLOCK,
        SS_PMON_DBE_LOCK_WAIT,
        SS_PMON_DBE_LOCK_COUNT,
        SS_PMON_MME_NLOCKS,
        SS_PMON_MME_MAXNLOCKS,
        SS_PMON_MME_NCHAINS,
        SS_PMON_MME_MAXNCHAINS,
        SS_PMON_MME_MAXPATH,
        SS_PMON_MME_TUPLEUSAGE,
        SS_PMON_MME_INDEXUSAGE,
        SS_PMON_MME_PAGEUSAGE,
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        SS_PMON_MYSQL_RND_INIT,
        SS_PMON_MYSQL_INDEX_READ,
        SS_PMON_MYSQL_FETCH_NEXT,
        SS_PMON_MYSQL_CURSOR_CREATE,
        SS_PMON_MYSQL_CURSOR_CLOSE,
        SS_PMON_MYSQL_CURSOR_RESET_FULL,
        SS_PMON_MYSQL_CURSOR_RESET_SIMPLE,
        SS_PMON_MYSQL_CURSOR_RESET_FETCH,
        SS_PMON_MYSQL_CURSOR_CACHE_FIND,
        SS_PMON_MYSQL_CURSOR_CACHE_HIT,
        SS_PMON_MYSQL_RECORDS_IN_RANGE,
        SS_PMON_MYSQL_CONNECT,
        SS_PMON_MYSQL_COMMIT,
        SS_PMON_MYSQL_ROLLBACK,
#endif /* defined(SS_MYSQL) || defined(SS_MYSQL_AC) */
        SS_PMON_INDEX_SEARCH_BOTH,
        SS_PMON_INDEX_SEARCH_STORAGE,
        SS_PMON_BNODE_SEARCH_KEYS,
        SS_PMON_BNODE_SEARCH_MISMATCH,
        SS_PMON_BNODE_BUILD_MISMATCH,
        SS_PMON_BNODE_NODESPLIT,
        SS_PMON_BNODE_NODERELOCATE,
        SS_PMON_BNODE_NODEEMPTY,
        SS_PMON_BNODE_EXCLUSIVE,
        SS_PMON_BNODE_KEYREAD,
        SS_PMON_BNODE_KEYREADDELETE,
        SS_PMON_BNODE_KEYREADOLDVERSION,
        SS_PMON_BNODE_KEYREADABORT,
        SS_PMON_BNODE_STORAGELEAFLEN,
        SS_PMON_BNODE_STORAGEINDEXLEN,
        SS_PMON_BNODE_BONSAILEAFLEN,
        SS_PMON_BNODE_BONSAIINDEXLEN,
        SS_PMON_BONSAIHEIGHT,
        SS_PMON_BTREE_LOCK_NODE,
        SS_PMON_BTREE_LOCK_TREE,
        SS_PMON_BTREE_LOCK_FULLPATH,
        SS_PMON_BTREE_LOCK_PARTIALPATH,
        SS_PMON_BTREE_GET_NOLOCK,
        SS_PMON_BTREE_GET_SHAREDLOCK,
        SS_PMON_PESSGATE_WAIT,
        SS_PMON_MERGEGATE_WAIT,
        SS_PMON_STORAGEGATE_WAIT,
        SS_PMON_BONSAIGATE_WAIT,
        SS_PMON_GATE_WAIT,
        SS_PMON_MAXVALUES
} ss_pmon_val_t;

typedef struct {
        ulong pm_values[SS_PMON_MAXVALUES];
        ulong pm_time;
} ss_pmon_t;

typedef enum {
        SS_PMONTYPE_COUNTER,
        SS_PMONTYPE_VALUE
} ss_pmon_type_t;

typedef struct {
        const char*           pn_name;
        ss_pmon_type_t  pn_type;
} ss_pmon_name_t;

extern ss_pmon_t ss_pmon;

extern ss_pmon_name_t ss_pmon_names[SS_PMON_MAXVALUES+1];

void SsPmonInit(void);
void SsPmonClear(void);
void SsPmonGetData(ss_pmon_t* pmon);

ulong SsPmonGetSingleValue(ss_pmon_val_t val);
bool SsPmonAccept(char* filter, ss_pmon_val_t val);

ulong SsPmonGetLoad(void);

#endif /* SSPMON_H */
