/*************************************************************************\
**  source       * sspmon.c
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

#include "ssenv.h"

#include "ssstring.h"

#include "ssc.h"
#include "sschcvt.h"
#include "sstime.h"
#include "sspmon.h"

ss_pmon_name_t ss_pmon_names[SS_PMON_MAXVALUES+1] = {
        { "File open",          SS_PMONTYPE_COUNTER }, /* SS_PMON_FILEOPEN */
        { "File read",          SS_PMONTYPE_COUNTER }, /* SS_PMON_FILEREAD */
        { "File write",         SS_PMONTYPE_COUNTER }, /* SS_PMON_FILEWRITE */
        { "File append",        SS_PMONTYPE_COUNTER }, /* SS_PMON_FILEAPPEND */
        { "File flush",         SS_PMONTYPE_COUNTER }, /* SS_PMON_FILEFLUSH */
        { "File lock",          SS_PMONTYPE_COUNTER }, /* SS_PMON_FILELOCK */
        { "Cache find",         SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEFIND */
        { "Cache read",         SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEFILEREAD */
        { "Cache write",        SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEFILEWRITE */
        { "Cache prefetch",     SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEPREFETCH, */
        { "Cache prefetch wait",SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEPREFETCHWAIT, */
        { "Cache preflush",     SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEPREFLUSH, */
        { "Cache LRU write",    SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHELRUWRITE */
        { "Cache slot wait",    SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHESLOTWAIT */
        { "Cache slot replace", SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHESLOTREPLACE */
        { "Cache write storage leaf",  SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEWRITESTORAGELEAF */
        { "Cache write storage index", SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEWRITESTORAGEINDEX */
        { "Cache write bonsai leaf",   SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEWRITEBONSAILEAF */
        { "Cache write bonsai index",  SS_PMONTYPE_COUNTER }, /* SS_PMON_CACHEWRITEBONSAIINDEX */

        { "RPC messages",       SS_PMONTYPE_COUNTER }, /* SS_PMON_RPCMESSAGE */
        { "RPC read",           SS_PMONTYPE_COUNTER }, /* SS_PMON_RPCPHYSREAD */
        { "RPC write",          SS_PMONTYPE_COUNTER }, /* SS_PMON_RPCPHYSWRITE */
        { "RPC uncompressed",   SS_PMONTYPE_COUNTER }, /* SS_PMON_RPCUNCOMPRESSED */
        { "RPC compressed",     SS_PMONTYPE_COUNTER }, /* SS_PMON_RPCCOMPRESSED */
        { "Com sel empty",      SS_PMONTYPE_COUNTER }, /* SS_PMON_COMSELEMPTY */
        { "Com sel found",      SS_PMONTYPE_COUNTER }, /* SS_PMON_COMSELFOUND */
        { "SQL prepare",        SS_PMONTYPE_COUNTER }, /* SS_PMON_SQLPREPARE */
        { "SQL execute",        SS_PMONTYPE_COUNTER }, /* SS_PMON_SQLEXECUTE */
        { "SQL fetch",          SS_PMONTYPE_COUNTER }, /* SS_PMON_SQLFETCH */
        { "DBE insert",         SS_PMONTYPE_COUNTER }, /* SS_PMON_DBEINSERT */
        { "DBE delete",         SS_PMONTYPE_COUNTER }, /* SS_PMON_DBEDELETE */
        { "DBE update",         SS_PMONTYPE_COUNTER }, /* SS_PMON_DBEUPDATE */
        { "DBE fetch",          SS_PMONTYPE_COUNTER }, /* SS_PMON_DBEFETCH, */
        { "DBE fetch unique found",     SS_PMONTYPE_COUNTER }, /* SS_PMON_DBEFETCHUNIQUEFOUND, */
        { "DBE fetch unique not found", SS_PMONTYPE_COUNTER }, /* SS_PMON_DBEFETCHUNIQUENOTFOUND, */
        { "DBE dd operation",   SS_PMONTYPE_VALUE },   /* SS_PMON_DBEDDOP */

        { "Proc exec",          SS_PMONTYPE_COUNTER }, /* SS_PMON_PROCEXEC, */
        { "Trig exec",          SS_PMONTYPE_COUNTER }, /* SS_PMON_TRIGEXEC, */
        { "SA insert",          SS_PMONTYPE_COUNTER }, /* SS_PMON_SAINSERT, */
        { "SA delete",          SS_PMONTYPE_COUNTER }, /* SS_PMON_SADELETE, */
        { "SA update",          SS_PMONTYPE_COUNTER }, /* SS_PMON_SAUPDATE, */
        { "SA fetch",           SS_PMONTYPE_COUNTER }, /* SS_PMON_SAFETCH, */

        { "Trans commit",       SS_PMONTYPE_COUNTER }, /* SS_PMON_TRANSCOMMIT */
        { "Trans abort",        SS_PMONTYPE_COUNTER }, /* SS_PMON_TRANSABORT */
        { "Trans rollback",     SS_PMONTYPE_COUNTER }, /* SS_PMON_TRANSROLLBACK */
        { "Trans readonly",     SS_PMONTYPE_COUNTER }, /* SS_PMON_TRANSRDONLY */
        { "Trans buf",          SS_PMONTYPE_VALUE   }, /* SS_PMON_TRANSBUFCNT */
        { "Trans buf cleanup",  SS_PMONTYPE_VALUE },   /* SS_PMON_TRANSBUFCLEAN */
        { "Trans buf cleanup level", SS_PMONTYPE_VALUE }, /* SS_PMON_TRANSBUFCLEANLEVEL */
        { "Trans buf abort level", SS_PMONTYPE_VALUE },/* SS_PMON_TRANSBUFABORTLEVEL */
        { "Trans buf added",    SS_PMONTYPE_VALUE   }, /* SS_PMON_TRANSBUFADDED */
        { "Trans buf removed",  SS_PMONTYPE_VALUE   }, /* SS_PMON_TRANSBUFREMOVED */
        { "Trans validate",     SS_PMONTYPE_VALUE   }, /* SS_PMON_TRANSVLDCNT */
        { "Trans active",       SS_PMONTYPE_VALUE   }, /* SS_PMON_TRANSACTCNT */
        { "Trans read level",   SS_PMONTYPE_VALUE   }, /* SS_PMON_TRANSREADLEVEL */
        { "Ind write",          SS_PMONTYPE_COUNTER }, /* SS_PMON_INDWRITE */
        { "Ind nomrg write",    SS_PMONTYPE_VALUE   }, /* SS_PMON_INDWRITESAFTERMERGE */
        { "Log write",          SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGWRITES */
        { "Log file write",     SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGFILEWRITE */
        { "Log nocp write",     SS_PMONTYPE_VALUE   }, /* SS_PMON_LOGWRITESAFTERCP */
        { "Log size",           SS_PMONTYPE_VALUE   }, /* SS_PMON_LOGSIZE */
        { "Search active",      SS_PMONTYPE_VALUE   }, /* SS_PMON_SRCHNACTIVE */
        { "Db size",            SS_PMONTYPE_VALUE   }, /* SS_PMON_DBSIZE */
        { "Db free size",       SS_PMONTYPE_VALUE   }, /* SS_PMON_DBFREESIZE */
        { "Mem size",           SS_PMONTYPE_VALUE   }, /* SS_PMON_MEMSIZE */
        { "Merge quickstep",    SS_PMONTYPE_COUNTER }, /* SS_PMON_MERGEQUICKSTEP */
        { "Merge step",         SS_PMONTYPE_COUNTER }, /* SS_PMON_MERGESTEP */
        { "Merge step (purge)", SS_PMONTYPE_COUNTER }, /* SS_PMON_MERGEPURGESTEP */
        { "Merge step (user)",  SS_PMONTYPE_COUNTER }, /* SS_PMON_MERGEUSERSTEP */
        { "Merge oper",         SS_PMONTYPE_COUNTER }, /* SS_PMON_MERGEOPER */
        { "Merge cleanup",      SS_PMONTYPE_VALUE   }, /* SS_PMON_MERGECLEANUP */
        { "Merge active",       SS_PMONTYPE_VALUE   }, /* SS_PMON_MERGEACT */
        { "Merge nomrg write",  SS_PMONTYPE_VALUE   }, /* SS_PMON_MERGEWRITES */
        { "Merge file write",   SS_PMONTYPE_COUNTER }, /* SS_PMON_MERGEFILEWRITE */
        { "Merge file read",    SS_PMONTYPE_COUNTER }, /* SS_PMON_MERGEFILEREAD */
        { "Merge level",        SS_PMONTYPE_VALUE   }, /* SS_PMON_MERGELEVEL */
        { "Backup step",        SS_PMONTYPE_COUNTER }, /* SS_PMON_BACKUPSTEP */
        { "Backup active",      SS_PMONTYPE_VALUE   }, /* SS_PMON_BACKUPACT */
        { "Checkpoint active",  SS_PMONTYPE_VALUE   }, /* SS_PMON_CHECKPOINTACT */
        { "Checkpoint count",   SS_PMONTYPE_VALUE   }, /* SS_PMON_CHECKPOINTCOUNT */
        { "Checkpoint file write", SS_PMONTYPE_COUNTER }, /* SS_PMON_CHECKPOINTFILEWRITE */
        { "Checkpoint file read",  SS_PMONTYPE_COUNTER }, /* SS_PMON_CHECKPOINTFILEREAD */
        { "Est read samples",   SS_PMONTYPE_COUNTER }, /* SS_PMON_ESTSAMPLESREAD, */

        { "Sync repl msg forw", SS_PMONTYPE_COUNTER }, /* SS_PMON_SYNCMSGREPLFORWARD, */
        { "Sync repl msg getr", SS_PMONTYPE_COUNTER }, /* SS_PMON_SYNCMSGREPLGEREPLY, */
        { "Sync repl msg exec", SS_PMONTYPE_COUNTER }, /* SS_PMON_SYNCMSGREPLEXEC, */
        { "Sync mast msg read", SS_PMONTYPE_COUNTER }, /* SS_PMON_SYNCMSGMASTREAD, */
        { "Sync mast msg exec", SS_PMONTYPE_COUNTER }, /* SS_PMON_SYNCMSGMASTEXEC, */
        { "Sync mast msg write",SS_PMONTYPE_COUNTER }, /* SS_PMON_SYNCMSGMASTWRITE, */
        { "Sync mast subs",     SS_PMONTYPE_COUNTER }, /* SS_PMON_SYNCSUBSCRIPTIONS, */

#if defined(SS_BETA)
        { "B:Relcur open",      SS_PMONTYPE_COUNTER }, /* SS_PMON_RELCUROPEN */
        { "B:Indsea reset",     SS_PMONTYPE_COUNTER }, /* SS_PMON_INDSEARESET */
#endif
        { "Log flush (L)",    SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGFLUSHES_LOGICAL */
        { "Log flush (P)",    SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGFLUSHES_PHYSICAL */
        { "Log grpcommwkup",  SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGGROUPCOMMIT_WAKEUPS */
        { "Log flush full",   SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGFLUSHES_FULLPAGES */
        { "Log wait flush",   SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGWAITFLUSH */
        { "Log writeq full rec",SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGMAXWRITEQUEUERECORDS */
        { "Log writeq full byt",SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGMAXWRITEQUEUEBYTES */
        { "Log writeq records",       SS_PMONTYPE_VALUE }, /* SS_PMON_LOGWRITEQUEUERECORDS */
        { "Log writeq bytes",         SS_PMONTYPE_VALUE }, /* SS_PMON_LOGWRITEQUEUEBYTES */
        { "Log writeq pending bytes", SS_PMONTYPE_VALUE }, /* SS_PMON_LOGWRITEQUEUEPENDINGBYTES */
        { "Log writeq add",           SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGWRITEQUEUEADD */
        { "Log writeq write",         SS_PMONTYPE_COUNTER }, /* SS_PMON_LOGWRITEQUEUEWRITE */

        { "HSB operation count",SS_PMONTYPE_COUNTER }, /* SS_PMON_HSB_OPCOUNT */
        { "HSB commit count",   SS_PMONTYPE_COUNTER }, /* SS_PMON_HSB_COMMITCOUNT */
        { "HSB packet count",   SS_PMONTYPE_COUNTER }, /* SS_PMON_HSB_PACKETCOUNT */
        { "HSB flush count",    SS_PMONTYPE_COUNTER }, /* SS_PMON_HSB_FLUSHCOUNT */
        { "HSB cached bytes",   SS_PMONTYPE_VALUE   }, /* SS_PMON_HSB_CACHEBYTES */
        { "HSB cached ops",     SS_PMONTYPE_VALUE   }, /* SS_PMON_HSB_CACHEOPS */
        { "HSB flusher bytes",  SS_PMONTYPE_VALUE   }, /* SS_PMON_HSB_FLUSHERBYTES */
        { "HSB notsent bytes",  SS_PMONTYPE_VALUE   }, /* SS_PMON_HSB_NOTSENTBYTES */
        { "HSB grouped acks",   SS_PMONTYPE_COUNTER }, /* SS_PMON_HSB_ACKGROUP_COUNT */
        { "HSB state",          SS_PMONTYPE_VALUE   }, /* SS_PMON_HSB_STATE */
        { "HSB wait cpmes",     SS_PMONTYPE_VALUE   }, /* SS_PMON_HSB_CPWAITMES */
        { "HSB secondary queues",SS_PMONTYPE_VALUE   },/* SS_PMON_HSB_SEC_QUEUES */
        { "HSB log reqcount",   SS_PMONTYPE_COUNTER },  /* SS_PMON_HSB_LOGSPM_REQCOUNT */
        { "HSB log waitct",     SS_PMONTYPE_COUNTER },  /* SS_PMON_HSB_LOGSPM_WAITCOUNT */
        { "HSB log freespc",    SS_PMONTYPE_VALUE },    /* SS_PMON_HSB_LOGSPM_FREESPACE */
        { "HSB catchup reqcnt", SS_PMONTYPE_COUNTER },/* SS_PMON_HSB_CATCHUPSPM_REQCOUNT */
        { "HSB catchup waitcnt",SS_PMONTYPE_COUNTER },/* SS_PMON_HSB_CATCHUPSPM_WAITCOUNT */
        { "HSB catchup freespc",SS_PMONTYPE_VALUE },  /* SS_PMON_HSB_CATCHUPSPM_FREESPACE */
        { "HSB alone freespc",  SS_PMONTYPE_VALUE },  /* SS_PMON_HSB_ALONEBYTES_FREESPACE */

#if defined(SS_BETA)
        { "B:relcur create",    SS_PMONTYPE_COUNTER }, /* SS_PMON_RELCUR_CREATE */
        { "B:relcur reset full",SS_PMONTYPE_COUNTER }, /* SS_PMON_RELCUR_RESETFULL */
        { "B:relcur reset smpl",SS_PMONTYPE_COUNTER }, /* SS_PMON_RELCUR_RESETSIMPLE */
        { "B:relcur cnstr",     SS_PMONTYPE_COUNTER }, /* SS_PMON_RELCUR_CONSTR */
        { "B:relcur cnstr smpl",SS_PMONTYPE_COUNTER },/* SS_PMON_RELCUR_CONSTRSIMPLE */
        { "B:relcur estimate",  SS_PMONTYPE_COUNTER }, /* SS_PMON_RELCUR_NEWESTIMATE */
        { "B:dbe trx needwchk", SS_PMONTYPE_COUNTER }, /* SS_PMON_DBE_TRX_NEEDWRITECHECK */
        { "B:dbe trx onldelmrk",SS_PMONTYPE_COUNTER }, /* SS_PMON_DBE_TRX_ONLYDELETEMARK */
        { "B:dbe trx escalwchk",SS_PMONTYPE_COUNTER }, /* SS_PMON_DBE_TRX_ESCALATEWRITECHECK */
#endif /* SS_BETA */
        { "Thread count",       SS_PMONTYPE_VALUE   }, /* SS_PMON_SS_THREADCOUNT */
        { "Trans wait readlvl", SS_PMONTYPE_VALUE   }, /* SS_PMON_WAITREADLEVEL_COUNT */
        { "Lock ok",            SS_PMONTYPE_COUNTER }, /* SS_PMON_DBE_LOCK_OK */
        { "Lock timeout",       SS_PMONTYPE_COUNTER }, /* SS_PMON_DBE_LOCK_TIMEOUT */
        { "Lock deadlock",      SS_PMONTYPE_COUNTER }, /* SS_PMON_DBE_LOCK_DEADLOCK */
        { "Lock wait",          SS_PMONTYPE_COUNTER }, /* SS_PMON_DBE_LOCK_WAIT */
        { "Lock count",         SS_PMONTYPE_VALUE },   /* SS_PMON_DBE_LOCK_COUNT */
        { "MME cur num of locks",        SS_PMONTYPE_VALUE }, /* SS_PMON_MME_NLOCKS */
        { "MME max num of locks",        SS_PMONTYPE_VALUE }, /* SS_PMON_MME_MAXNLOCKS */
        { "MME cur num of lock chains",  SS_PMONTYPE_VALUE }, /* SS_PMON_MME_NCHAINS */
        { "MME max num of lock chains",  SS_PMONTYPE_VALUE }, /* SS_PMON_MME_MAXNCHAINS */
        { "MME longest lock chain path", SS_PMONTYPE_VALUE }, /* SS_PMON_MME_MAXPATH */
        { "MME mem used by tuples",      SS_PMONTYPE_VALUE }, /* SS_PMON_MME_TUPLEUSAGE */
        { "MME mem used by indexes",     SS_PMONTYPE_VALUE }, /* SS_PMON_INDEXUSAGE */
        { "MME mem used by page structs",SS_PMONTYPE_VALUE }, /* SS_PMON_PAGEUSAGE */
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        { "MySQL rnd init",             SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_RND_INIT */
        { "MySQL index read",           SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_INDEX_READ */
        { "MySQL fetch next",           SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_FETCH_NEXT */
        { "MySQL cursor create",        SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_CURSOR_CREATE */
        { "MySQL cursor close",         SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_CURSOR_CLOSE */
        { "MySQL cursor reset full",    SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_CURSOR_RESET_FULL */
        { "MySQL cursor reset simple",  SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_CURSOR_RESET_SIMPLE */
        { "MySQL cursor reset fetch",   SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_CURSOR_RESET_FETCH */
        { "MySQL cursor cache find",    SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_CURSOR_CACHE_FIND */
        { "MySQL cursor cache hit",     SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_CURSOR_CACHE_HIT */
        { "MySQL records in range",     SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_RECORDS_IN_RANGE */
        { "MySQL connect",              SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_CONNECT */
        { "MySQL commit",               SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_COMMIT */
        { "MySQL rollback",             SS_PMONTYPE_COUNTER }, /* SS_PMON_MYSQL_ROLLBACK */
#endif /* defined(SS_MYSQL) || defined(SS_MYSQL_AC) */
        { "Index search both",          SS_PMONTYPE_COUNTER }, /* SS_PMON_INDEX_SEARCH_BOTH */
        { "Index search storage",       SS_PMONTYPE_COUNTER }, /* SS_PMON_INDEX_SEARCH_STORAGE */
        { "B-tree node search keys",    SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_SEARCH_KEYS */
        { "B-tree node search mismatch",SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_SEARCH_MISMATCH */
        { "B-tree node build mismatch", SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_BUILD_MISMATCH */
        { "B-tree node split",          SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_NODESPLIT */
        { "B-tree node relocate",       SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_NODERELOCATE */
        { "B-tree node delete empty",   SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_NODEEMPTY */
        { "B-tree node exclusive",      SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_EXCLUSIVE */
        { "B-tree key read",            SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_KEYREAD */
        { "B-tree key read delete",     SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_KEYREADDELETE */
        { "B-tree key read oldversion", SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_KEYREADOLDVERSION */
        { "B-tree key read abort",      SS_PMONTYPE_COUNTER }, /* SS_PMON_BNODE_KEYREADABORT */

        { "B-tree storage leaf len",    SS_PMONTYPE_VALUE },   /* SS_PMON_BNODE_STORAGELEAFLEN,  */
        { "B-tree storage index len",   SS_PMONTYPE_VALUE },   /* SS_PMON_BNODE_STORAGEINDEXLEN,  */
        { "B-tree bonsai leaf len",     SS_PMONTYPE_VALUE },   /* SS_PMON_BNODE_BONSAILEAFLEN,  */
        { "B-tree bonsai index len",    SS_PMONTYPE_VALUE },   /* SS_PMON_BNODE_BONSAIINDEXLEN,  */
        
        { "Bonsai-tree height",         SS_PMONTYPE_VALUE },   /* SS_PMON_BONSAIHEIGHT */
        { "B-tree lock node",           SS_PMONTYPE_COUNTER }, /* SS_PMON_BTREE_LOCK_NODE */
        { "B-tree lock tree",           SS_PMONTYPE_COUNTER }, /* SS_PMON_BTREE_LOCK_TREE */

        { "B-tree lock full path",      SS_PMONTYPE_COUNTER }, /* SS_PMON_BTREE_LOCK_FULLPATH */
        { "B-tree lock partial path",   SS_PMONTYPE_COUNTER }, /* SS_PMON_BTREE_LOCK_PARTIALPATH */
        { "B-tree get no lock",         SS_PMONTYPE_COUNTER }, /* SS_PMON_BTREE_GET_NOLOCK */
        { "B-tree get shared lock",     SS_PMONTYPE_COUNTER }, /* SS_PMON_BTREE_GET_SHAREDLOCK */
        
        { "Pessimistic gate wait",      SS_PMONTYPE_COUNTER }, /* SS_PMON_PESSGATE_WAIT */
        { "Merge gate wait",            SS_PMONTYPE_COUNTER }, /* SS_PMON_MERGEGATE_WAIT */
        { "Storage gate wait",          SS_PMONTYPE_COUNTER }, /* SS_PMON_STORAGEGATE_WAIT */
        { "Bonsai Gate wait",           SS_PMONTYPE_COUNTER }, /* SS_PMON_BONSAIGATE_WAIT */
        { "Gate wait",                  SS_PMONTYPE_COUNTER }, /* SS_PMON_GATE_WAIT */
        { "",                           SS_PMONTYPE_VALUE   }  /* SS_PMON_MAXVALUES */
};

ss_pmon_t ss_pmon;

void SsPmonInit(void)
{
}

void SsPmonClear(void)
{
        memset(&ss_pmon, '\0', sizeof(ss_pmon_t));
}

void SsPmonGetData(ss_pmon_t* pmon)
{
        memcpy(pmon, &ss_pmon, sizeof(ss_pmon_t));
}

bool SsPmonAccept(char* filter, ss_pmon_val_t val)
{
        if (filter == NULL) {
            return(TRUE);
        }

        return(SsStrnicmp(
                    ss_pmon_names[val].pn_name,
                    filter,
                    strlen(filter)) == 0);
}

ulong SsPmonGetSingleValue(ss_pmon_val_t val)
{
        return ss_pmon.pm_values[val];
}

#ifndef SS_MYSQL
ulong SsPmonGetLoad(void)
{
        static SsTimeT tm = 0L;
        static ulong l_fileread  = 0L;
        static ulong l_filewrite = 0L;
        static ulong l_fileflush = 0L;
        static ulong l_rpcmessage= 0L;
        static ulong l_sqlprepare= 0L;
        static ulong l_sqlexecute= 0L;
        static ulong l_sqlfetch  = 0L;
        static ulong l_dbeinsert = 0L;
        static ulong l_dbedelete = 0L;
        static ulong l_dbeupdate = 0L;
        static ulong l_dbefetch  = 0L;
        static ulong l_procexec  = 0L;
        static ulong l_trigexec  = 0L;

        ulong load = 0L;
        ulong diff;
        ulong res;
        SsTimeT now;

        now = SsTimeMs();

#define ADD_LOAD(ID, var, factor) {diff=100*factor*(ss_pmon.pm_values[ID]-var);var=ss_pmon.pm_values[ID];load=load+diff;}

        ADD_LOAD(SS_PMON_FILEREAD,  l_fileread  ,  50);
        ADD_LOAD(SS_PMON_FILEWRITE, l_filewrite ,  70);
        ADD_LOAD(SS_PMON_FILEFLUSH, l_fileflush , 100);
        ADD_LOAD(SS_PMON_RPCMESSAGE,l_rpcmessage,  10);
        ADD_LOAD(SS_PMON_SQLPREPARE,l_sqlprepare,  10);
        ADD_LOAD(SS_PMON_SQLEXECUTE,l_sqlexecute,   5);
        ADD_LOAD(SS_PMON_SQLFETCH  ,l_sqlfetch  ,   2);
        ADD_LOAD(SS_PMON_DBEINSERT ,l_dbeinsert ,   4);
        ADD_LOAD(SS_PMON_DBEDELETE ,l_dbedelete ,   5);
        ADD_LOAD(SS_PMON_DBEUPDATE ,l_dbeupdate ,   5);
        ADD_LOAD(SS_PMON_DBEFETCH  ,l_dbefetch  ,   1);
        ADD_LOAD(SS_PMON_PROCEXEC  ,l_procexec  ,   5);
        ADD_LOAD(SS_PMON_TRIGEXEC  ,l_trigexec  ,   5);

#undef ADD_LOAD

        if (tm == 0L) {
            tm = SsTimeMs();
            load = 0L;
        } else {
            res = now - tm;
            if (res != 0L) {
                load = load / res;
            }
            tm = now;
        }

        return(load);
}
#endif /* !SS_MYSQL */

