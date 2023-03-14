/*************************************************************************\
**  source       * su0task.h
**  directory    * su
**  description  * Server tasking 
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


#ifndef SU0TASK_H
#define SU0TASK_H

#include <ssc.h>

/* Task classes */
typedef enum {
        SU_TASK_UNKNOWN = 0,

        SU_TASK_EXIT,
        SU_TASK_HOTSTANDBY_G2,
        SU_TASK_HOTSTANDBY,
        SU_TASK_HOTSTANDBY_CATCHUP,
        SU_TASK_CHECKPOINT,
        SU_TASK_CHECK_CHECKPOINT,
        SU_TASK_BACKUP,
        SU_TASK_BACKUP_START,
        SU_TASK_CHECK_MERGE,
        SU_TASK_MERGE,
        SU_TASK_CHECK_TIMEOUT,
        SU_TASK_ATCOMMAND,
        SU_TASK_REMOTEUSERS,
        SU_TASK_LOCALUSERS,
        SU_TASK_SYNC_MESSAGE,
        SU_TASK_SYNC_HISTCLEAN,
        SU_TASK_AFTERCOMMIT,
        SU_TASK_BGSTMT,
        SU_TASK_CHECK_REXEC_EXPIRED,
        SU_TASK_SRVTASK,

        SU_TASK_DEBUG,

        SU_TASK_ENUMEND
} SuTaskClassT;

/* Task states */
typedef enum {
        SU_STATE_DEFAULT = 0,
        SU_STATE_ACTIVE,
        SU_STATE_SUSPEND,

        SU_STATE_ENUMEND
} SuTaskStateT;

typedef enum {
        SU_TASK_PRIO_HIGH,
        SU_TASK_PRIO_NORMAL,
        SU_TASK_PRIO_IDLE,
        SU_TASK_PRIO_DEFAULT
} SuTaskPrioT;

typedef struct srv_task_st su_task_t;

/* Type of the function pointer that is called at each task step.
   Parameter t is the current task handle, parameter td is the
   task data from the client given when the task was started.
*/
typedef int (*su_task_fun_t)(void* t, void* td);

/* Possible events.
 */
#define SRV_EVENT_TASKSWITCHCOUNT           0   /* Every 1000 task switch. */
#ifdef SS_TASKOPTIMIZE
#if !defined(SS_MT)
#define SRV_EVENT_BGTASKCOUNT               1   /* Every bgtaskctr task switch. */
#endif /* SS_MT */
#else /* SS_TASKOPTIMIZE */
#define SRV_EVENT_BGTASKCOUNT               1   /* Every bgtaskctr task switch. */
#endif /* SS_TASKOPTIMIZE */
#define SSE_EVENT_LASTBLOB                  2   /* After last blob insert. */
#define SSE_EVENT_EXITACTION                3   /* After exiting from action gate. */
#define SSE_EVENT_BADCONNECTDELAY           4   /* Delay after too many bad connects. */
#define SSE_EVENT_FLUSHCOMPLETED            5   /* Cache flush completed */
#define SNC_EVENT_NEWSYNCTASK               6   /* New sync task is available */
#define SRV_EVENT_COMREADREADY_ID           7   /* Communication read is ready, 
                                                   requires id */
#define SNC_EVENT_SYNCMSGREADY              8   /* Sync message is ready. */
#define SNC_EVENT_SYNCGATE_ID               9   /* General Sync gate, requires id. */
#define SSE_EVENT_BACKUPCOMPLETED           10  /* Backup completed */
#define SSE_EVENT_CPCOMPLETED               11  /* Checkpoint completed */
#define SSE_EVENT_HSBREPLYREADY             12  /* HSB secondary has replied */
#define SSE_EVENT_HSBWRITEPERMITTED         13  /* HSB log write now permitted */
#define SSE_EVENT_HSBGROUPCOMMITWRITTEN     14  /* HSB group commit written */
#define SSE_EVENT_HSBGROUPCOMMITCOMPLETE    15  /* HSB group commit completed */
#define SSE_EVENT_HSBPRIMARYSTATUS          16  /* HSB primary status is known,
                                                   either switched to alone or
                                                   to secondary. */
#define SSE_EVENT_HSBPRISTATUSCHANGE        17  /* HSB primary status is changed. */
#define SSE_EVENT_RBACKUPREPLYREADY         18  /* Backup server has replied */
#define SSE_EVENT_SNCALERTREPLY             19  /* Synch Alert reply */
#define SSE_EVENT_REMOTEEVENTREPLY          20  /* Remote event reply */
#define SSE_EVENT_BACKUPMODE                21  /* Backup mode set */
#define SSE_EVENT_DEBUG_AFTERSTMTEXEC       22  /* Debug event to wait after stmt exec*/
#define SSE_EVENT_NEWBGSTMT                 23  /* New bg-stmts available */
#define REX_EVENT_CONNECTIONFREED           24  /* Rexec connection unlocked */
#define HSB_EVENT_STATEPENDING              25  /* Rexec connection unlocked */
#define SSE_EVENT_FLUSHBATCH                26  /* dbe_iomgr_flushbatch_t signal */
#define SSE_EVENT_CLUSTERCMD_READY          27  /* is cluster command ready for each spoke */
#define SSE_EVENT_HSBNEWSECONDARY_LPID      28  /* Client is waiting HSB secondary readlevel (lpid) */
#define SRV_EVENT_MAXEVENT                  29  /* System constant, number of 
                                                   possible events. */
#define SRV_EVENT_NOEVENT                   (SRV_EVENT_MAXEVENT + 1)

typedef enum {
        SU_TASK_STOP,
        SU_TASK_CONT,
        SU_TASK_YIELD
} su_taskret_t;

/* ReadThreadMode enum values. When you change these, check file sse1conf.c.
 */
typedef enum {
        SU_TASK_READTHRMODE_TASKTHR       = 0, /* Default, must be zero. */
        SU_TASK_READTHRMODE_SELECTTHR_NOSUP = 1,/* Not supported any more. */
        SU_TASK_READTHRMODE_EXECIMMEDIATE = 2, /* Own thread for each connection.
                                              In sse1thre.c. */
        SU_TASK_READTHRMODE_DIRECTSELECT  = 3, /* Direct select ignoring rpc and
                                              com layers. In sse1thre.c. */
        SU_TASK_READTHRMODE_MAX           = 4  /* Max enum number. */
} su_task_readthrmode_t;

typedef enum {
        SRV_TASK_STOP,
        SRV_TASK_CONT,
        SRV_TASK_YIELD
} srv_taskret_t;


#endif /* SU0TASK_H */
