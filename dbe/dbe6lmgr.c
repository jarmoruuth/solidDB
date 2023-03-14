/*************************************************************************\
**  source       * dbe6lmgr.c
**  directory    * dbe
**  description  * Lock manager.
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

#include <ssstdio.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <sstime.h>
#include <ssthread.h>
#include <sspmon.h>

#include <uti0va.h>

#include <su0list.h>

#include <rs0sysi.h>

#include "dbe9type.h"
#include "dbe0type.h"
#include "dbe6lmgr.h"
#include "dbe6bkey.h"       /* For dbe_bkey_dprintvtpl. */

#ifndef SS_NOLOCKING

#define N_FREE_LOCKS     2000
#define N_FREE_REQUESTS  4000

#ifdef LMGR_SPLITMUTEX
#ifndef LMGR_NMUTEXES
#define LMGR_NMUTEXES 101
#endif
#define LMGR_MUTEX(lm, name)  (((name) % (lm)->lm_hashsize) % lmgr_nmutexes)
#endif


#define CHK_LOCKMGR(lm)  ss_dassert(SS_CHKPTR(lm) && lm->lm_chk == DBE_CHK_LOCKMGR)
#define CHK_LOCKHEAD(lh) ss_dassert(SS_CHKPTR(lh) && lh->lh_chk == DBE_CHK_LOCKHEAD)
#define CHK_LOCKREQ(lr)  ss_dassert(SS_CHKPTR(lr) && lr->lr_chk == DBE_CHK_LOCKREQ)
#define CHK_LOCKTRAN(lt) ss_dassert(SS_CHKPTR(lt) && lt->lt_chk == DBE_CHK_LOCKTRAN)

#define class_max(new_class, old_class) SS_MAX(new_class, old_class)

typedef enum {
        LOCK_INSTANT,
        LOCK_SHORT,
        LOCK_MEDIUM,
        LOCK_LONG,
        LOCK_VERY_LONG
} lock_class_t;

typedef enum {
        LOCK_GRANTED,
        LOCK_CONVERTING,
        LOCK_WAITING,
        LOCK_DENIED,
        LOCK_NSTATUS
} lock_status_t;

typedef struct lock_request_st  lock_request_t;
typedef struct lock_head_st     lock_head_t;

/* Lock request by a transaction.
 */
struct lock_request_st {
        ss_debug(dbe_chk_t lr_chk;)
        lock_request_t* lr_queue;       /* next request in lock queue */
        lock_head_t*    lr_head;        /* pointer back to head of the queue */
        lock_status_t   lr_status;      /* granted, waiting, converting, denied */
        dbe_lock_mode_t lr_mode;        /* mode requested (and granted) */
        dbe_lock_mode_t lr_convert_mode;/* if in convert wait, mode resired */
        int             lr_count;       /* lock count */
        int             lr_okcount;     /* the number of times this lock
                                           request has been granted. */
        lock_class_t    lr_class;       /* lock class (duration) */
        void*           lr_wakeup_task; /* task to wakeup when lock granted */
        dbe_locktran_t* lr_tran;        /* pointer to transaction record */
        lock_request_t* lr_tran_prev;   /* prev req in transaction list */
        lock_request_t* lr_tran_next;   /* next req in transaction list */
        SsTimeT         lr_timeout;     /* Time when timeout expires */
        dbe_lockname_t  lr_name;
        ss_debug(int    lr_userid;)
        ss_debug(long   lr_time;)
};

/* Lock header.
 */
struct lock_head_st {
        ss_debug(dbe_chk_t lh_chk;)
        lock_head_t*    lh_chain;       /* pointer to next in hash chain */
        long            lh_relid;
        dbe_lockname_t  lh_name;        /* the name of this lock */
        lock_request_t* lh_queue;       /* the queue of requests for this lock */
        dbe_lock_mode_t lh_granted_mode;/* the mode of the granted group */
        bool            lh_waiting;     /* flag indicates nonempty wait group */
};

/* Status of a transaction.
 */
struct dbe_locktran_st {
        ss_debug(dbe_chk_t lt_chk;)
        rs_sysi_t*      lt_cd;      /* client data */
#ifdef LMGR_SPLITMUTEX
        lock_request_t* lt_locks[LMGR_NMUTEXES];   /* transaction lock list */
#else
        lock_request_t* lt_locks;   /* transaction lock list */
#endif
        lock_request_t* lt_wait;    /* lock waited by this transaction (or NULL) */
        dbe_locktran_t* lt_cycle;   /* used by deadlock detector */
};

/* Lock manager object. Lock manager maintains a hash table of locked
 * objects.
 */
struct dbe_lockmgr_st {
        ss_debug(dbe_chk_t  lm_chk;)
        lock_head_t**       lm_hash;        /* Hash table. */
        uint                lm_hashsize;    /* Hash table size. */
#ifdef LMGR_SPLITMUTEX
        SsSemT*             lm_mutex[LMGR_NMUTEXES];
#else
        SsSemT*             lm_sem;         /* Lock manager mutex. */
#endif
        long                lm_lockcnt;
        ulong               lm_lockokcnt;
        ulong               lm_lockwaitcnt;
        ulong               lm_retrylockwaitcnt;
        ulong               lm_locktimeoutcnt;
        bool                lm_useescalation;
        ulong               lm_escalatelimit;
        bool                lm_uselocks;
        bool                lm_disablerowspermessage;
        ulong               lm_cachedrelid;
        lock_head_t*        lm_cachedlock;
#ifdef LMGR_SPLITMUTEX
        lock_head_t*        lm_freelock[LMGR_NMUTEXES];
        ulong               lm_nfreelocks[LMGR_NMUTEXES];
        ulong               lm_maxfreelocks;
        lock_request_t*     lm_freerequest[LMGR_NMUTEXES];
        ulong               lm_nfreerequests[LMGR_NMUTEXES];
        ulong               lm_maxfreerequests;
#else
        lock_head_t*        lm_freelock;
        ulong               lm_nfreelocks;
        ulong               lm_maxfreelocks;
        lock_request_t*     lm_freerequest;
        ulong               lm_nfreerequests;
        ulong               lm_maxfreerequests;
#endif
        bool                lm_sharedsem;
        /* PMON counters for MME lock manager.  See sspmon.[ch] */
        long                lm_maxnlocks;
        long                lm_nchains;
        long                lm_maxnchains;
        ulong               lm_maxpath;
};

#ifdef LMGR_SPLITMUTEX
static int lmgr_nmutexes = LMGR_NMUTEXES;
#endif

static void lmgr_unlock(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name);

/*#***********************************************************************\
 * 
 *		lock_modename
 * 
 * 
 * 
 * Parameters : 
 * 
 *	mode - 
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
static char* lock_modename(dbe_lock_mode_t mode)
{
        static char* name_array[LOCK_NMODES] = {
            (char *)"LOCK_FREE",
            (char *)"LOCK_IS",
            (char *)"LOCK_IX",
            (char *)"LOCK_S",
            (char *)"LOCK_SIX",
            (char *)"LOCK_U",
            (char *)"LOCK_X"
        };
        ss_dassert(mode < LOCK_NMODES);

        return(name_array[mode]);
}

/*#***********************************************************************\
 * 
 *		lock_statusname
 * 
 * 
 * 
 * Parameters : 
 * 
 *	status - 
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
static char* lock_statusname(lock_status_t status)
{
        static char* name_array[LOCK_NSTATUS] = {
            (char *)"GRANTED",
            (char *)"CONVERTING",
            (char *)"WAITING",
            (char *)"DENIED"
        };
        ss_dassert(status < LOCK_NSTATUS);

        return(name_array[status]);
}

/*#***********************************************************************\
 * 
 *		lock_max
 * 
 * 
 * 
 * Parameters : 
 * 
 *	req_mode - 
 *		
 *		
 *	granted_mode - 
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
static dbe_lock_mode_t lock_max(dbe_lock_mode_t req_mode, dbe_lock_mode_t granted_mode)
{
        static struct {
            dbe_lock_mode_t new_mode[LOCK_NMODES];
        } convert_matrix[LOCK_NMODES] = {
        { 0,       LOCK_IS, LOCK_IX, LOCK_S,  LOCK_SIX,LOCK_U,  LOCK_X }, /* LOCK_FREE */
        { LOCK_IS, LOCK_IS, LOCK_IX, LOCK_S,  LOCK_SIX,LOCK_U,  LOCK_X }, /* LOCK_IS */
        { LOCK_IX, LOCK_IX, LOCK_IX, LOCK_SIX,LOCK_SIX,LOCK_X,  LOCK_X }, /* LOCK_IX */
        { LOCK_S,  LOCK_S,  LOCK_SIX,LOCK_S,  LOCK_SIX,LOCK_U,  LOCK_X }, /* LOCK_S */
        { LOCK_SIX,LOCK_SIX,LOCK_SIX,LOCK_SIX,LOCK_SIX,LOCK_SIX,LOCK_X }, /* LOCK_SIX */
        { LOCK_U,  LOCK_U,  LOCK_X,  LOCK_U,  LOCK_SIX,LOCK_U,  LOCK_X }, /* LOCK_U */
        { LOCK_X,  LOCK_X,  LOCK_X,  LOCK_X,  LOCK_X,  LOCK_X,  LOCK_X }  /* LOCK_X */
        };

        ss_dassert(req_mode < LOCK_NMODES);
        ss_dassert(granted_mode < LOCK_NMODES);
        ss_dassert(req_mode > LOCK_FREE);

        return(convert_matrix[req_mode].new_mode[granted_mode]);
}

/*#***********************************************************************\
 * 
 *		lock_compat
 * 
 * 
 * 
 * Parameters : 
 * 
 *	req_mode - 
 *		
 *		
 *	granted_mode - 
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
static bool lock_compat(dbe_lock_mode_t req_mode, dbe_lock_mode_t granted_mode)
{
        static struct {
            bool is_compat[LOCK_NMODES];
        } compat_matrix[LOCK_NMODES] = {
        { 0,    LOCK_IS,LOCK_IX,LOCK_S,LOCK_SIX,LOCK_U,LOCK_X }, /* LOCK_FREE */
        { TRUE, TRUE,   TRUE,   TRUE,  TRUE,    FALSE, FALSE },  /* LOCK_IS */
        { TRUE, TRUE,   TRUE,   FALSE, FALSE,   FALSE, FALSE },  /* LOCK_IX */
        { TRUE, TRUE,   FALSE,  TRUE,  FALSE,   FALSE, FALSE },  /* LOCK_S */
        { TRUE, TRUE,   FALSE,  FALSE, FALSE,   FALSE, FALSE },  /* LOCK_SIX */
        { TRUE, FALSE,  FALSE,  TRUE,  FALSE,   FALSE, FALSE },  /* LOCK_U */
        { TRUE, FALSE,  FALSE,  FALSE, FALSE,   FALSE, FALSE }   /* LOCK_X */
        };

        ss_dassert(req_mode < LOCK_NMODES);
        ss_dassert(granted_mode < LOCK_NMODES);
        ss_dassert(req_mode > LOCK_FREE);

        return(compat_matrix[req_mode].is_compat[granted_mode]);
}

/*#***********************************************************************\
 * 
 *		lockmgr_getrellockmode
 * 
 * Returns table lock mode from row lock mode.
 * 
 * Parameters : 
 * 
 *	rowlockmode - 
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
static dbe_lock_mode_t lockmgr_getrellockmode(dbe_lock_mode_t rowlockmode)
{
        static dbe_lock_mode_t relmode_table[LOCK_NMODES] = {
            LOCK_FREE,  /* LOCK_FREE */
            LOCK_IS,    /* LOCK_IS */
            LOCK_IX,    /* LOCK_IX */
            LOCK_IS,    /* LOCK_S */
            LOCK_IX,    /* LOCK_SIX */
            LOCK_IX,    /* LOCK_U */
            LOCK_IX     /* LOCK_X */
        };

        ss_dassert(rowlockmode > LOCK_IX);
        ss_dassert(rowlockmode < LOCK_NMODES);

        return(relmode_table[rowlockmode]);
}

/*#***********************************************************************\
 * 
 *		lmgr_print
 * 
 * 
 * 
 * Parameters : 
 * 
 *	lm - 
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
static void lmgr_print(dbe_lockmgr_t* lm)
{
        int i;
        lock_head_t* lock;
        lock_request_t* req;

        SsDbgPrintf("LOCKLIST BEGIN\n");

        for (i = 0; i < lm->lm_hashsize; i++) {
            lock = lm->lm_hash[i];
            while (lock != NULL) {
                CHK_LOCKHEAD(lock);
                SsDbgPrintf("lh_name=%ld, lh_granted_mode=%s, lh_waiting=%d\n",
                    lock->lh_name, lock_modename(lock->lh_granted_mode), lock->lh_waiting);
                for (req = lock->lh_queue; req != NULL; req = req->lr_queue) {
                    CHK_LOCKREQ(req);
                    ss_assert(req->lr_name == lock->lh_name);
                    SsDbgPrintf("  status=%s, mode=%s, convert_mode=%s, tran=%ld\n",
                        lock_statusname(req->lr_status), lock_modename(req->lr_mode), 
                        lock_modename(req->lr_convert_mode), (long)req->lr_tran);

                }
                lock = lock->lh_chain;
            }
        }
        SsDbgPrintf("LOCKLIST END\n");
}

/*#***********************************************************************\
 * 
 *		lock_request_alloc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	me - 
 *		
 *		
 *	lock - 
 *		
 *		
 *	mode - 
 *		
 *		
 *	class - 
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
static lock_request_t* lock_request_alloc(
        dbe_lockmgr_t*      lm,
        dbe_locktran_t*     me,
        lock_head_t*        lock,
        dbe_lock_mode_t     mode,
        lock_class_t        class,
        dbe_lockname_t      name)
{
        lock_request_t* req;
        ss_debug(static long locktime;)

        SS_PMON_ADD(SS_PMON_DBE_LOCK_COUNT);

#ifdef LMGR_SPLITMUTEX
        if (lm->lm_freerequest[LMGR_MUTEX(lm, name)] != NULL) {
            ss_dassert(lm->lm_nfreerequests[LMGR_MUTEX(lm, name)] > 0);
            req = lm->lm_freerequest[LMGR_MUTEX(lm, name)];
            lm->lm_freerequest[LMGR_MUTEX(lm, name)] = req->lr_queue;
            lm->lm_nfreerequests[LMGR_MUTEX(lm, name)]--;
            ss_dassert(req->lr_chk == DBE_CHK_FREELOCKREQ);
        } else {
            ss_dassert(lm->lm_nfreerequests[LMGR_MUTEX(lm, name)] == 0);
            req = SSMEM_NEW(lock_request_t);
        }
#else
        if (lm->lm_freerequest != NULL) {
            ss_dassert(lm->lm_nfreerequests > 0);
            req = lm->lm_freerequest;
            lm->lm_freerequest = req->lr_queue;
            lm->lm_nfreerequests--;
            ss_dassert(req->lr_chk == DBE_CHK_FREELOCKREQ);
        } else {
            ss_dassert(lm->lm_nfreerequests == 0);
            req = SSMEM_NEW(lock_request_t);
        }
#endif

        ss_debug(req->lr_chk = DBE_CHK_LOCKREQ;)
        req->lr_queue = NULL;
        req->lr_head = lock;
        req->lr_status = LOCK_GRANTED;
        req->lr_mode = mode;
        req->lr_convert_mode = LOCK_FREE;
        req->lr_count = 1;
        req->lr_okcount = 0;
        req->lr_class = class;
        req->lr_wakeup_task = NULL;
        req->lr_tran = me;
        req->lr_tran_prev = NULL;
#ifdef LMGR_SPLITMUTEX
        req->lr_tran_next = me->lt_locks[LMGR_MUTEX(lm, name)];
        if (me->lt_locks[LMGR_MUTEX(lm, name)] != NULL) {
            me->lt_locks[LMGR_MUTEX(lm, name)]->lr_tran_prev = req;
        }
        me->lt_locks[LMGR_MUTEX(lm, name)] = req;
#else
        req->lr_tran_next = me->lt_locks;
        if (me->lt_locks != NULL) {
            me->lt_locks->lr_tran_prev = req;
        }
        me->lt_locks = req;
#endif
        req->lr_name = name;
        ss_debug(req->lr_time = locktime++);
        ss_debug(req->lr_userid = rs_sysi_userid(me->lt_cd));

        ss_dprintf_4(("Allocate new lock request, userid %d, time %ld\n",
            req->lr_userid, req->lr_time));

        return(req);
}

/*#***********************************************************************\
 * 
 *		lock_request_free
 * 
 * 
 * 
 * Parameters : 
 * 
 *	req - 
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
static void lock_request_free(
        dbe_lockmgr_t*      lm,
        lock_request_t*     req)
{
        dbe_locktran_t* me;

        CHK_LOCKREQ(req);

        SS_PMON_REMOVE(SS_PMON_DBE_LOCK_COUNT);

        me = req->lr_tran;

        CHK_LOCKTRAN(me);

        if (req->lr_tran_prev != NULL) {
            req->lr_tran_prev->lr_tran_next = req->lr_tran_next;
        }
        if (req->lr_tran_next != NULL) {
            req->lr_tran_next->lr_tran_prev = req->lr_tran_prev;
        }
#ifdef LMGR_SPLITMUTEX
        if (req == me->lt_locks[LMGR_MUTEX(lm, req->lr_name)]) {
            me->lt_locks[LMGR_MUTEX(lm, req->lr_name)] = req->lr_tran_next;
        }

        if (lm->lm_nfreerequests[LMGR_MUTEX(lm, req->lr_name)]
            < lm->lm_maxfreerequests) {
            req->lr_queue = lm->lm_freerequest[LMGR_MUTEX(lm, req->lr_name)];
            lm->lm_freerequest[LMGR_MUTEX(lm, req->lr_name)] = req;
            ss_debug(req->lr_chk = DBE_CHK_FREELOCKREQ);
            lm->lm_nfreerequests[LMGR_MUTEX(lm, req->lr_name)]++;
        } else {
            SsMemFree(req);
        }
#else        
        if (req == me->lt_locks) {
            me->lt_locks = req->lr_tran_next;
        }
        if (lm->lm_nfreerequests < lm->lm_maxfreerequests) {
            req->lr_queue = lm->lm_freerequest;
            lm->lm_freerequest = req;
            ss_debug(req->lr_chk = DBE_CHK_FREELOCKREQ);
            lm->lm_nfreerequests++;
        } else {
            SsMemFree(req);
        }
#endif
}

/*#***********************************************************************\
 * 
 *		lock_head_alloc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	relid - 
 *		
 *		
 *	name - 
 *		
 *		
 *	mode - 
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
static lock_head_t* lock_head_alloc(
        dbe_lockmgr_t*      lm,
        ulong               relid,
        dbe_lockname_t      name,
        dbe_lock_mode_t     mode)
{
        lock_head_t* lock;

#ifdef LMGR_SPLITMUTEX
        if (lm->lm_freelock[LMGR_MUTEX(lm, name)] != NULL) {
            ss_dassert(lm->lm_nfreelocks[LMGR_MUTEX(lm, name)] > 0);
            lock = lm->lm_freelock[LMGR_MUTEX(lm, name)];
            lm->lm_freelock[LMGR_MUTEX(lm, name)] = lock->lh_chain;
            lm->lm_nfreelocks[LMGR_MUTEX(lm, name)]--;
            ss_dassert(lock->lh_chk == DBE_CHK_FREELOCKHEAD);
        } else {
            ss_dassert(lm->lm_nfreelocks[LMGR_MUTEX(lm, name)] == 0);
            lock = SSMEM_NEW(lock_head_t);
        }
#else
        if (lm->lm_freelock != NULL) {
            ss_dassert(lm->lm_nfreelocks > 0);
            lock = lm->lm_freelock;
            lm->lm_freelock = lock->lh_chain;
            lm->lm_nfreelocks--;
            ss_dassert(lock->lh_chk == DBE_CHK_FREELOCKHEAD);
        } else {
            ss_dassert(lm->lm_nfreelocks == 0);
            lock = SSMEM_NEW(lock_head_t);
        }
#endif

        ss_debug(lock->lh_chk = DBE_CHK_LOCKHEAD;)
        lock->lh_chain = NULL;
        lock->lh_relid = relid;
        lock->lh_name = name;
        lock->lh_queue = NULL;
        lock->lh_granted_mode = mode;
        lock->lh_waiting = FALSE;

        return(lock);
}

/*#***********************************************************************\
 * 
 *		lock_head_free
 * 
 * 
 * 
 * Parameters : 
 *
 *      lm - in, use
 *          The lockmanager.
 *
 *      lock - in, take
 *          The lock to be freed.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void lock_head_free(
        dbe_lockmgr_t*  lm,
        lock_head_t*    lock)
{
        CHK_LOCKMGR(lm);
        CHK_LOCKHEAD(lock);

        if (lm->lm_cachedlock == lock) {
            lm->lm_cachedrelid = 0;
            lm->lm_cachedlock = NULL;
        } else {
            ss_dassert(lock->lh_relid != DBE_LOCKRELID_REL
                       || lock->lh_name != lm->lm_cachedrelid);
        }

#ifdef LMGR_SPLITMUTEX
        if (lm->lm_nfreelocks[LMGR_MUTEX(lm, lock->lh_name)]
            < lm->lm_maxfreelocks) {
            lock->lh_chain = lm->lm_freelock[LMGR_MUTEX(lm, lock->lh_name)];
            lm->lm_freelock[LMGR_MUTEX(lm, lock->lh_name)] = lock;
            ss_debug(lock->lh_chk = DBE_CHK_FREELOCKHEAD);
            lm->lm_nfreelocks[LMGR_MUTEX(lm, lock->lh_name)]++;
        } else {
            SsMemFree(lock);
        }
#else
        if (lm->lm_nfreelocks < lm->lm_maxfreelocks) {
            lock->lh_chain = lm->lm_freelock;
            lm->lm_freelock = lock;
            ss_debug(lock->lh_chk = DBE_CHK_FREELOCKHEAD);
            lm->lm_nfreelocks++;
        } else {
            SsMemFree(lock);
        }
#endif
}

/*#***********************************************************************\
 * 
 *		lmgr_isdeadlock
 * 
 * Tests for cycles in lock wait lists.
 * 
 * Parameters : 
 * 
 *		me - 
 *			
 *			
 *		req - 
 *			
 *			
 *		level - 
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
static bool lmgr_isdeadlock(dbe_locktran_t* me, int level, int* p_maxlocks)
{
        dbe_locktran_t* tran;
        lock_request_t* req;
        bool deadlock;

        ss_dprintf_3(("lmgr_isdeadlock:level=%d, *p_maxlocks=%d, me=%ld\n", level, *p_maxlocks, (long)me));
        ss_dassert(dbe_cfg_fastdeadlockdetect);

        level--;

        if (me->lt_wait == NULL) {
            ss_dprintf_4(("lmgr_isdeadlock:FALSE, me->lt_wait == NULL\n"));
            return(FALSE);
        }
        if (level == 0) {
            /* Too many nested levels, stop. */
            ss_dprintf_4(("lmgr_isdeadlock:FALSE, level == 0\n"));
            return(FALSE);
        }
        req = me->lt_wait->lr_head->lh_queue;
        while (req != NULL) {
            if (req->lr_tran == me && req->lr_status != LOCK_GRANTED) {
                break;
            }
            ss_dprintf_3(("lmgr_isdeadlock:req->lr_status=%d, req->lr_mode=%s, me->lt_wait->lr_mode=%s, name=%ld\n",
                req->lr_status,
                lock_modename(req->lr_mode), 
                lock_modename(me->lt_wait->lr_mode),
                (long)req->lr_head->lh_name));
            ss_assert(req->lr_head->lh_name == me->lt_wait->lr_head->lh_name);
            if ((*p_maxlocks)-- <= 0) {
                /* Too many locks checked, stop. */
                ss_dprintf_4(("lmgr_isdeadlock:FALSE, (*p_maxlocks)-- <= 0\n"));
                return(FALSE);
            }
            if (req->lr_tran != me
                && (!lock_compat(req->lr_mode, me->lt_wait->lr_mode)
                    || req->lr_status != LOCK_GRANTED))
            {
                tran = req->lr_tran;
                me->lt_cycle = tran;
                ss_dprintf_4(("lmgr_isdeadlock:check for deadlock:me->lt_cycle = tran, %ld = %ld\n", (long)&me->lt_cycle, (long)tran));
                if (tran->lt_cycle != NULL) {
                    ss_dprintf_4(("lmgr_isdeadlock:tran->lt_cycle=%ld\n", (long)tran->lt_cycle));
                    deadlock = TRUE;
                } else {
                    deadlock = lmgr_isdeadlock(tran, level, p_maxlocks);
                }
                ss_dprintf_4(("lmgr_isdeadlock:check for deadlock:me->lt_cycle = NULL, %ld = 0\n", (long)&me->lt_cycle));
                me->lt_cycle = NULL;
                if (deadlock) {
                    ss_dprintf_4(("lmgr_isdeadlock:TRUE, deadlock\n"));
                    return(TRUE);
                }
            }
            req = req->lr_queue;
        }
        ss_dprintf_4(("lmgr_isdeadlock:FALSE, end\n"));
        return(FALSE);
}

/*#***********************************************************************\
 * 
 *		lmgr_lock
 * 
 * 
 * 
 * Parameters : 
 * 
 *	lm - 
 *		
 *		
 *	me - 
 *		
 *		
 *	relid - 
 *		
 *		
 *	name - 
 *		
 *		
 *	mode - 
 *		
 *		
 *	class - 
 *		
 *		
 *	timeout - 
 *		
 *		
 *	bouncep - 
 *		
 *		
 *	p_req -
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
static dbe_lock_reply_t lmgr_lock(
        dbe_lockmgr_t*      lm,
        dbe_locktran_t*     me,
        ulong               relid,
        dbe_lockname_t      name,
        dbe_lock_mode_t     mode,
        lock_class_t        class,
        long                timeout,
        bool                bouncep,
        lock_request_t**    p_req,
        bool*               p_newlock)
{
        uint                bucket;
        lock_head_t*        lock;
        lock_request_t*     req;
        lock_request_t*     last = NULL;
        dbe_lock_reply_t    error_num;
        ulong               path;

        SS_PUSHNAME("lmgr_lock");
        ss_dprintf_3(("lmgr_lock: mode %s, name %lu, timeout %ld\n",
                      lock_modename(mode), name, timeout));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(me);
        ss_dassert(timeout >= 0);
#ifndef LMGR_SPLITMUTEX
        ss_dassert(SsSemThreadIsEntered(lm->lm_sem));
#endif

        if (p_newlock != NULL) {
            *p_newlock = FALSE;
        }

        if (relid > DBE_LOCKRELID_MAXSPECIAL && lm->lm_disablerowspermessage) {
            rs_sysi_setdisablerowspermessage(me->lt_cd, TRUE);
        }

        if (me->lt_wait != NULL) {
            /* Waiting for a lock.
             */
            ss_dprintf_4(("This transaction is already waiting for a lock\n"));
            req = me->lt_wait;
            CHK_LOCKREQ(req);
            error_num = req->lr_status;
            if (error_num == LOCK_GRANTED) {
                /* Lock granted.
                 */
                me->lt_wait = NULL;
                ss_dprintf_4(("Lock granted, return LOCK_OK\n"));
                lm->lm_lockokcnt++;
                req->lr_okcount++;
                if (p_req != NULL) {
                    *p_req = req;
                }
                SS_PMON_ADD(SS_PMON_DBE_LOCK_OK);
                SS_POPNAME;
                return(LOCK_OK);
            }
            if (error_num == LOCK_WAITING || error_num == LOCK_CONVERTING) {
                /* Still waiting.
                 */
                SsTimeT now;
                SsTimeT timeout_now;

#ifdef DBE_LOCK_MSEC
                now = SsTimeMs();
#else
                now = SsTime(NULL);
#endif
                timeout_now = req->lr_timeout - now;

#ifdef DBE_LOCK_MSEC
                if (SsTimeCmp(now, req->lr_timeout) < 0)
#else
                if (now < req->lr_timeout)
#endif
                {
#ifdef DBE_LOCK_MSEC
                    ss_dprintf_4(("Timeout period not elapsed (time %lu, timeout %lu), return LOCK_WAIT\n",
                        now, req->lr_timeout));
#else
                    ss_dprintf_4(("Timeout period not elapsed (time %ld, timeout %ld), return LOCK_WAIT\n",
                        SsTime(NULL), req->lr_timeout));
#endif
                    ss_assert(me->lt_cd != NULL);
                    ss_assert(rs_sysi_task(me->lt_cd) != NULL);
#ifdef DBE_LOCK_MSEC
                    rs_sysi_startlockwaitwithtimeout(me->lt_cd, timeout_now);
#else
                    rs_sysi_startlockwaitwithtimeout(me->lt_cd, 1000*timeout_now);
#endif
                    lm->lm_retrylockwaitcnt++;
                    SS_POPNAME;
                    SS_PMON_ADD(SS_PMON_DBE_LOCK_WAIT);
                    return(LOCK_WAIT);
                }
                ss_dprintf_4(("Lock timed out\n"));
                if (error_num == LOCK_CONVERTING) {
                    req->lr_status = LOCK_DENIED;
                }
                error_num = LOCK_TIMEOUT;
            }
            /* Request denied (timed out), make sure unlock will work.
             */
            ss_poutput_1(lmgr_print(lm));
            lm->lm_locktimeoutcnt++;
            ss_dprintf_4(("Request denied\n"));
            me->lt_wait = NULL;
            req->lr_class = LOCK_INSTANT;
            lmgr_unlock(lm, me, req->lr_head->lh_relid, req->lr_head->lh_name);
            SS_POPNAME;
            SS_PMON_ADD(SS_PMON_DBE_LOCK_TIMEOUT);
            return(error_num);
        }

        if (!lm->lm_uselocks) {
            ss_dprintf_4(("Do not use locks, return LOCK_OK\n"));
            SS_POPNAME;
            SS_PMON_ADD(SS_PMON_DBE_LOCK_OK);
            return(LOCK_OK);
        }

        if (FALSE && relid == DBE_LOCKRELID_REL && name == lm->lm_cachedrelid) {
            lock = lm->lm_cachedlock;
            CHK_LOCKHEAD(lock);
        } else {
            bucket = name % lm->lm_hashsize;
            lock = lm->lm_hash[bucket];
            path = 1;
            
            while (lock != NULL) {
                CHK_LOCKHEAD(lock);
                if ((ulong)lock->lh_relid == relid && lock->lh_name == name) {
                    break;
                }
                lock = lock->lh_chain;
                path++;
            }
            if (path > lm->lm_maxpath) {
                lm->lm_maxpath = path;
            }
        }

        if (lock == NULL) {
            /* Name not found, lock is free.
             */
            if (bouncep) {
                ss_dprintf_4(("Bounce lock, lock is free, return LOCK_OK\n"));
                SS_POPNAME;
                SS_PMON_ADD(SS_PMON_DBE_LOCK_OK);
                return(LOCK_OK);
            }
            ss_dprintf_4(("Name not found, lock is free, return LOCK_OK\n"));
            lock = lock_head_alloc(lm, relid, name, mode);
            lock->lh_queue = lock_request_alloc(lm, me, lock, mode, class, name);
            lm->lm_lockcnt++;
            lock->lh_queue->lr_okcount++;
            lock->lh_chain = lm->lm_hash[bucket];
            lm->lm_hash[bucket] = lock;
            lm->lm_lockokcnt++;
            if (p_req != NULL) {
                *p_req = lock->lh_queue;
            }
            if (lock->lh_chain == NULL) {
                lm->lm_nchains++;
                if (lm->lm_nchains > lm->lm_maxnchains) {
                    lm->lm_maxnchains = lm->lm_nchains;
                }
            }
            if (p_newlock != NULL) {
                *p_newlock = TRUE;
            }
            SS_POPNAME;
            SS_PMON_ADD(SS_PMON_DBE_LOCK_OK);
            return(LOCK_OK);
        }

        ss_dprintf_4(("Lock found, granted mode %s\n", lock_modename(lock->lh_granted_mode)));
        ss_dassert((ulong)lock->lh_relid == relid);
        ss_dassert(lock->lh_name == name);
        if (relid == DBE_LOCKRELID_REL) {
            lm->lm_cachedrelid = name;
            lm->lm_cachedlock = lock;
        }

        /* Lock is not free. Check if this is a conversion (rerequest).
         */
        for (req = lock->lh_queue; req != NULL; req = req->lr_queue) {
            CHK_LOCKREQ(req);
            if (req->lr_tran == me) {
                break;
            }
            last = req;
        }

        if (req == NULL) {

            /* A new request for this lock by this transaction. Allocate new
             * request and put it at the end of the queue.
             */
            ss_dprintf_4(("A new request for this transaction\n"));
            if (!bouncep) {
                req = lock_request_alloc(lm, me, lock, mode, class, name);
                lm->lm_lockcnt++;
                last->lr_queue = req;
            }
            if (p_newlock != NULL) {
                *p_newlock = TRUE;
            }

            if (!lock->lh_waiting && lock_compat(mode, lock->lh_granted_mode)) {
                /* New request: no wait case (compatible and no other
                 * waiters)
                 */
                if (bouncep) {
                    ss_dprintf_4(("Bounce lock, compatible and no other waiters, return LOCK_OK\n"));
                    SS_POPNAME;
                    SS_PMON_ADD(SS_PMON_DBE_LOCK_OK);
                    return(LOCK_OK);
                }
                ss_dprintf_4(("Lock granted, compatible and no other waiters\n"));
                lock->lh_granted_mode = lock_max(mode, lock->lh_granted_mode);
                ss_dprintf_4(("New granted mode %s, return LOCK_OK\n", lock_modename(lock->lh_granted_mode)));
                lm->lm_lockokcnt++;
                req->lr_okcount++;
                if (p_req != NULL) {
                    *p_req = req;
                }
                SS_POPNAME;
                SS_PMON_ADD(SS_PMON_DBE_LOCK_OK);
                return(LOCK_OK);

            } else {

                /* New request: must wait case. Mark the fact in the header
                 * status and request status.
                 */
                int deadlockdetectmaxlocks = dbe_cfg_deadlockdetectmaxlocks;
                if (bouncep) {
                    ss_dprintf_4(("Bounce lock, not compatible, return LOCK_TIMEOUT\n"));
                    SS_POPNAME;
                    SS_PMON_ADD(SS_PMON_DBE_LOCK_TIMEOUT);
                    return(LOCK_TIMEOUT);
                }
                ss_dprintf_4(("Not compatible, must wait case\n"));
                lm->lm_lockwaitcnt++;
                ss_assert(me->lt_cd != NULL);
                ss_rc_dassert(req->lr_status == LOCK_GRANTED, req->lr_status);
                req->lr_status = LOCK_WAITING;
                me->lt_wait = req;
                ss_poutput_1(lmgr_print(lm));
                if (dbe_cfg_fastdeadlockdetect
                    && timeout != 0
                    && lmgr_isdeadlock(me, dbe_cfg_deadlockdetectmaxdepth, &deadlockdetectmaxlocks))
                {
                    ss_dprintf_4(("Deadlock, set timeout to zero.\n"));
                    timeout = 0;
                }
                if (timeout != 0 && rs_sysi_task(me->lt_cd) != NULL) {
                    /* Wait for the lock. */
                    lock->lh_waiting = TRUE;
#ifdef DBE_LOCK_MSEC
                    req->lr_timeout = SsTimeMs() + timeout;
                    rs_sysi_startlockwaitwithtimeout(me->lt_cd, timeout);
#else
                    req->lr_timeout = SsTime(NULL) + timeout;
                    rs_sysi_startlockwaitwithtimeout(me->lt_cd, 1000*timeout);
#endif
                    ss_dprintf_4(("Wait for the lock, return LOCK_WAIT\n"));
                    SS_POPNAME;
                    SS_PMON_ADD(SS_PMON_DBE_LOCK_WAIT);
                    return(LOCK_WAIT);
                } else {
                    /* Lock expires immediately. */
                    req->lr_status = LOCK_DENIED;
                    req->lr_timeout = 0;
                    lm->lm_locktimeoutcnt++;
                    me->lt_wait = NULL;
                    req->lr_class = LOCK_INSTANT;
                    lmgr_unlock(lm, me, req->lr_head->lh_relid,
                                req->lr_head->lh_name);
                    ss_dprintf_4(("Zero timeout, return LOCK_TIMEOUT\n"));
                    SS_POPNAME;
                    SS_PMON_ADD(SS_PMON_DBE_LOCK_TIMEOUT);
                    return(LOCK_TIMEOUT);
                }
            }

        } else {

            /* Re-request for this lock by this transaction (conversion
             * case). First check if conversion is compatible with other
             * members in the granted group.
             */
            lock_request_t* tmpreq;
            dbe_lock_mode_t granted_mode = LOCK_FREE;
            dbe_lock_mode_t req_mode;

            ss_dprintf_4(("Rerequest of this lock by this transaction (conversion case), userid %d, time %ld\n",
                req->lr_userid, req->lr_time));
            ss_dassert(req->lr_status == LOCK_GRANTED);
            req_mode = lock_max(mode, req->lr_mode);

            ss_dprintf_4(("Requested converted mode is %s\n", lock_modename(req_mode)));

            if (mode <= req->lr_mode) {
                ss_dprintf_4(("Already has lock with mode %s, request of mode %s granted\n", lock_modename(req->lr_mode), lock_modename(mode)));
                req->lr_class = class_max(class, req->lr_class);
                req->lr_count++;
                lm->lm_lockokcnt++;
                req->lr_okcount++;
                if (p_req != NULL) {
                    *p_req = req;
                }
                SS_POPNAME;
                SS_PMON_ADD(SS_PMON_DBE_LOCK_OK);
                return(LOCK_OK);
            }

            /* Check all granted requests others than current transaction's
             * requests.
             */
            for (tmpreq = lock->lh_queue; tmpreq != NULL; tmpreq = tmpreq->lr_queue) {
                CHK_LOCKREQ(tmpreq);
                if (tmpreq->lr_tran != me) {
                    if (tmpreq->lr_status == LOCK_GRANTED ||
                        tmpreq->lr_status == LOCK_CONVERTING) {
                        /* If tmpreq->lr_status == LOCK_CONVERTING
                         * this is propably a deadlock!!!!!!!
                         * (Maybe this conditions needs refinement.)
                         */
                        granted_mode = lock_max(tmpreq->lr_mode, granted_mode);
                    } else {
                        /* End of granted group.
                         */
                        break;
                    }
                }
            }
            ss_dprintf_4(("Granted mode of all others in granted group is %s\n",
                lock_modename(granted_mode)));

            if (lock_compat(req_mode, granted_mode)) {
                /* Compatible with all others in the granted group,
                 * grant immediately. Upgrade request mode and lock mode.
                 */
                ss_dprintf_4(("Lock granted, compatible with others in granted group\n"));
                req->lr_class = class_max(class, req->lr_class);
                req->lr_count++;
                req->lr_status = LOCK_GRANTED;
                req->lr_mode = req_mode;
                lock->lh_granted_mode = lock_max(req->lr_mode, lock->lh_granted_mode);
                ss_dprintf_4(("New granted mode %s, return LOCK_OK\n", lock_modename(lock->lh_granted_mode)));
                lm->lm_lockokcnt++;
                req->lr_okcount++;
                if (p_req != NULL) {
                    *p_req = req;
                }
                SS_POPNAME;
                SS_PMON_ADD(SS_PMON_DBE_LOCK_OK);
                return(LOCK_OK);
            } else {
                /* Not compatible with others in the granted group,
                 * must wait.
                 */
                int deadlockdetectmaxlocks = dbe_cfg_deadlockdetectmaxlocks;
                ss_dprintf_4(("Not compatible with others in granted group, must wait\n"));

                for (tmpreq = lock->lh_queue; tmpreq != NULL; tmpreq = tmpreq->lr_queue) {
                    CHK_LOCKREQ(tmpreq);
                    last = tmpreq;
                }

                req = lock_request_alloc(lm, me, lock, mode, class, name);
                lm->lm_lockcnt++;
                last->lr_queue = req;

                if (p_newlock != NULL) {
                    *p_newlock = TRUE;
                }

                lm->lm_lockwaitcnt++;
                ss_assert(me->lt_cd != NULL);
                ss_rc_dassert(req->lr_status == LOCK_GRANTED, req->lr_status);
                req->lr_status = LOCK_CONVERTING;
                req->lr_convert_mode = mode;
                me->lt_wait = req;
                ss_poutput_1(lmgr_print(lm));
                if (dbe_cfg_fastdeadlockdetect 
                    && timeout != 0
                    && lmgr_isdeadlock(me, dbe_cfg_deadlockdetectmaxdepth, &deadlockdetectmaxlocks)) 
                {
                    ss_dprintf_4(("Deadlock, set timeout to zero.\n"));
                    timeout = 0;
                }
                if (timeout != 0 && rs_sysi_task(me->lt_cd) != NULL) {
                    /* Wait for the lock. */
                    lock->lh_waiting = TRUE;
#ifdef DBE_LOCK_MSEC
                    req->lr_timeout = SsTimeMs() + timeout;
                    rs_sysi_startlockwaitwithtimeout(me->lt_cd, timeout);
#else
                    req->lr_timeout = SsTime(NULL) + timeout;
                    rs_sysi_startlockwaitwithtimeout(me->lt_cd, 1000*timeout);
#endif
                    ss_dprintf_4(("Wait for the lock, return LOCK_WAIT\n"));
#ifdef DBE_LOCK_MSEC
                    ss_dprintf_4(("Time now %lu, timeout %lu\n",
                                  SsTimeMs(), req->lr_timeout));
#endif
                    SS_POPNAME;
                    SS_PMON_ADD(SS_PMON_DBE_LOCK_WAIT);
                    return(LOCK_WAIT);
                } else {
                    /* Lock expires immediately. */
                    req->lr_status = LOCK_DENIED;
                    req->lr_timeout = 0;
                    lm->lm_locktimeoutcnt++;
                    me->lt_wait = NULL;
                    req->lr_class = LOCK_INSTANT;
                    lmgr_unlock(lm, me, req->lr_head->lh_relid, req->lr_head->lh_name);
                    ss_dprintf_4(("Zero timeout, return LOCK_TIMEOUT\n"));
                    SS_POPNAME;
                    SS_PMON_ADD(SS_PMON_DBE_LOCK_TIMEOUT);
                    return(LOCK_TIMEOUT);
                }
            }
        }
}

/*#***********************************************************************\
 * 
 *		lmgr_unlock
 * 
 * 
 * 
 * Parameters : 
 * 
 *	lm - 
 *		
 *		
 *	me - 
 *		
 *		
 *	relid - 
 *		
 *		
 *	name - 
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
static void lmgr_unlock(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name)
{
        uint bucket;
        lock_head_t* lock;
        lock_head_t* prev = NULL;
        lock_request_t* req;
        lock_request_t* prev_req = NULL;
        dbe_locktran_t* first;
        ulong path;

        ss_dprintf_3(("lmgr_unlock: name %lu\n", name));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(me);
#ifndef LMGR_SPLITMUTEX
        ss_dassert(SsSemThreadIsEntered(lm->lm_sem));
#endif

        /* Find the requestor's request
         */
        bucket = name % lm->lm_hashsize;
        lock = lm->lm_hash[bucket];
        path = 1;
        while (lock != NULL) {
            CHK_LOCKHEAD(lock);
            if ((ulong)lock->lh_relid == relid && lock->lh_name == name) {
                break;
            }
            prev = lock;
            lock = lock->lh_chain;
            path++;
        }
        if (path > lm->lm_maxpath) {
            lm->lm_maxpath = path;
        }
        if (lock == NULL) {
            /* Lock is free. */
            ss_dprintf_4(("Lock name not found, lock is free\n"));
            return;
        }
        
        ss_dassert((ulong)lock->lh_relid == relid);
        ss_dassert(lock->lh_name == name);

        /* Found lock, look request belonging to the caller in the lock
         * queue.
         */
        for (req = lock->lh_queue; req != NULL; req = req->lr_queue) {
            CHK_LOCKREQ(req);
            if (req->lr_tran == me) {
                break;
            }
            prev_req = req;
        }
        if (req == NULL) {
            /* Lock is free. */
            ss_dprintf_4(("Request not found from request queue, lock is free\n"));
            return;
        }
        
        ss_dprintf_4(("Request found, userid %d, time %ld\n", req->lr_userid, req->lr_time));

        /* Found it, if > long class, unlock is a null operation
         */
        if (req->lr_class > LOCK_LONG || req->lr_count > 1) {
            /* Long duration, or count > 1 */
            req->lr_count--;
            ss_dprintf_4(("Long duration or count > 1, null unlock\n"));
            return;
        }
        
        /* End of no op cases where nothing changed.
         */
        if (lock->lh_queue == req && req->lr_queue == NULL) {
            /* Mine is only request. */
            ss_dprintf_4(("Mine is only request, unlock ok\n"));
            if (prev == NULL) {
                lm->lm_hash[bucket] = lock->lh_chain;
                if (lm->lm_hash[bucket] == NULL) {
                    lm->lm_nchains--;
                }
            } else {
                prev->lh_chain = lock->lh_chain;
            }
            lock_head_free(lm, lock);
            lock_request_free(lm, req);
            lm->lm_lockcnt--;
            return;
        }

        /* Interesting case: granted group not null when this request
         * leaves.
         */
        if (prev_req != NULL) {
            prev_req->lr_queue = req->lr_queue;
        } else {
            lock->lh_queue = req->lr_queue;
        }
        lock_request_free(lm, req);
        lm->lm_lockcnt--;

        /* Reset lock header.
         */
        lock->lh_waiting = FALSE;
        lock->lh_granted_mode = LOCK_FREE;

        /* Traverse lock queue: compute granted group, wake compatible
         * waiters.
         */
        ss_dprintf_4(("Compute granted group\n"));
        for (req = lock->lh_queue; req != NULL; req = req->lr_queue) {
            CHK_LOCKREQ(req);

            ss_dprintf_4(("req->lr_status=%d, req->lr_mode=%s, lock->lh_granted_mode=%s, name=%ld\n",
                req->lr_status,
                lock_modename(req->lr_mode), 
                lock_modename(req->lr_head->lh_granted_mode),
                (long)req->lr_head->lh_name));

#if 0 /* Jarmo removed, Feb 28, 1996 */
            if (req->lr_status == LOCK_GRANTED ||
                req->lr_status == LOCK_CONVERTING) {
#else            
            if (req->lr_status == LOCK_GRANTED) {
#endif

                /* Request granted, add to granted mode.
                 */
                ss_dprintf_4(("Request granted\n"));
                lock->lh_granted_mode = lock_max(req->lr_mode, lock->lh_granted_mode);

            } else {

                /* End of granted group.
                 */
                break;
            }
        }
        ss_dprintf_4(("Wake waiters\n"));
        first = lock->lh_queue->lr_tran;
        for (req = lock->lh_queue; req != NULL; req = req->lr_queue) {
            CHK_LOCKREQ(req);

            ss_dprintf_3(("req->lr_status=%d, req->lr_mode=%s, lock->lh_granted_mode=%s, name=%ld, first=%ld, req=%ld\n",
                req->lr_status,
                lock_modename(req->lr_mode), 
                lock_modename(req->lr_head->lh_granted_mode),
                (long)req->lr_head->lh_name,
                (long)first,
                (long)req));

            if (req->lr_tran != first) {
                first = NULL;
            }

            if (req->lr_status == LOCK_WAITING) {
                /* Request waiting.
                 */
                ss_dprintf_4(("Request is waiting\n"));
                if (req->lr_tran == first
                    || lock_compat(req->lr_mode, lock->lh_granted_mode)) 
                {
                    /* Compatible with granted mode, grant it.
                     */
                    ss_dprintf_4(("Waiting request granted, call wakeup\n"));
                    req->lr_status = LOCK_GRANTED;
                    /* Upgrade granted mode.
                     */
                    lock->lh_granted_mode = lock_max(req->lr_mode, lock->lh_granted_mode);
                    /* Wake up waiting task.
                     */
                    ss_assert(req->lr_tran->lt_cd != NULL);
                    /* ss_assert(rs_sysi_task(req->lr_tran->lt_cd) != NULL); */
                    rs_sysi_lockwakeup(req->lr_tran->lt_cd);

                } else {

                    /* Request imcompatible, FIFO impiles it and all
                     * successors must wait.
                     */
                    ss_dprintf_4(("Waiting request imcompatible, must wait\n"));
                    lock->lh_waiting = TRUE;
                    break;
                }

            } else if (req->lr_status == LOCK_CONVERTING) {

                /* Convert case.
                 */
                lock_request_t* tmpreq;
                dbe_lock_mode_t granted_mode = LOCK_FREE;
                dbe_lock_mode_t req_mode;

                ss_dprintf_4(("Convert case\n"));
            
                req_mode = lock_max(req->lr_convert_mode, req->lr_mode);
                ss_dprintf_4(("Requested converted mode is %s\n", lock_modename(req_mode)));
                
                /* Check all granted requests others than current transaction's
                 * request.
                 */
                for (tmpreq = lock->lh_queue; tmpreq != NULL; tmpreq = tmpreq->lr_queue) {
                    CHK_LOCKREQ(tmpreq);
                    if (tmpreq != req) {
                        if (tmpreq->lr_status == LOCK_GRANTED ||
                            tmpreq->lr_status == LOCK_CONVERTING) {
                            granted_mode = lock_max(tmpreq->lr_mode, granted_mode);
                        } else {
                            /* End of granted group.
                             */
                            break;
                        }
                    } else {
                        break;
                    }
                }

                ss_dprintf_4(("Granted mode of all others in granted group is %s\n",
                    lock_modename(granted_mode)));

                if (req->lr_tran == first
                    || lock_compat(req_mode, granted_mode)) 
                {
                    /* Compatible with granted mode, grant it.
                     */
                    ss_dprintf_4(("Lock convert granted, call wakeup\n"));
                    req->lr_status = LOCK_GRANTED;
                    /* Upgrade request mode.
                     */
                    req->lr_mode = req_mode;
                    /* Upgrade granted mode.
                     */
                    lock->lh_granted_mode = lock_max(req_mode, lock->lh_granted_mode);
                    /* Wake up waiting task.
                     */
                    ss_assert(req->lr_tran->lt_cd != NULL);
                    /* ss_assert(rs_sysi_task(req->lr_tran->lt_cd) != NULL); */
                    rs_sysi_lockwakeup(req->lr_tran->lt_cd);

                } else {

                    /* Conversion imcompatible, it and all successors
                     * must wait.
                     */
                    ss_dprintf_4(("Conversion incompatible, must wait\n"));
                    lock->lh_waiting = TRUE;
                    break;
                }
            }
        }
        ss_dprintf_4(("Unlock done, new granted mode %s\n",
            lock_modename(lock->lh_granted_mode)));
}

/*##**********************************************************************\
 * 
 *		dbe_lockmgr_unlock
 * 
 * 
 * 
 * Parameters : 
 * 
 *	lm - 
 *		
 *		
 *	me - 
 *		
 *		
 *	relid - 
 *		
 *		
 *	name - 
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
void dbe_lockmgr_unlock(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name)
{
        lock_head_t* lock;
        lock_request_t* req;
        lock_request_t* next;

        ss_dprintf_1(("dbe_lockmgr_unlock\n"));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(me);

#ifdef LMGR_SPLITMUTEX
        SsSemEnter(lm->lm_mutex[LMGR_MUTEX(lm, name)]);
#else
        SsSemEnter(lm->lm_sem);
#endif

#ifdef LMGR_SPLITMUTEX
        for (req = me->lt_locks[LMGR_MUTEX(lm, name)]; req != NULL; req = next)
#else
        for (req = me->lt_locks; req != NULL; req = next)
#endif
        {
            CHK_LOCKREQ(req);
            next = req->lr_tran_next;
            lock = req->lr_head;
            if ((ulong)lock->lh_relid == relid && lock->lh_name == name) {
                /* Lock found.
                 */
                req->lr_class = LOCK_INSTANT;
                req->lr_count = 1;
                lmgr_unlock(lm, me, relid, name);
                if (req == me->lt_wait) {
                    me->lt_wait = NULL;
                }
                break;
            }
        }

#ifdef LMGR_SPLITMUTEX
        SsSemExit(lm->lm_mutex[LMGR_MUTEX(lm, name)]);
#else
        SsSemExit(lm->lm_sem);
#endif
}
 
/*##**********************************************************************\
 * 
 *		dbe_lockmgr_unlock_shared
 * 
 * 
 * 
 * Parameters : 
 * 
 *	lm - 
 *		
 *		
 *	me - 
 *		
 *		
 *	relid - 
 *		
 *		
 *	name - 
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
void dbe_lockmgr_unlock_shared(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name)
{
        lock_head_t* lock;
        lock_request_t* req;
        lock_request_t* next;

        ss_dprintf_1(("dbe_lockmgr_unlock_shared\n"));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(me);

#ifdef LMGR_SPLITMUTEX
        SsSemEnter(lm->lm_mutex[LMGR_MUTEX(lm, name)]);
#else
        SsSemEnter(lm->lm_sem);
#endif

#ifdef LMGR_SPLITMUTEX
        for (req = me->lt_locks[LMGR_MUTEX(lm, name)]; req != NULL; req = next)
#else
        for (req = me->lt_locks; req != NULL; req = next)
#endif
        {
            CHK_LOCKREQ(req);
            next = req->lr_tran_next;
            lock = req->lr_head;
            if ((ulong)lock->lh_relid == relid && lock->lh_name == name) {
                /* Lock found.
                 */
                lmgr_unlock(lm, me, relid, name);
                break;
            }
        }

#ifdef LMGR_SPLITMUTEX
        SsSemExit(lm->lm_mutex[LMGR_MUTEX(lm, name)]);
#else
        SsSemExit(lm->lm_sem);
#endif
}

/*##**********************************************************************\
 *
 *      dbe_lockmgr_cancelwaiting
 *
 * <function description>
 *
 * Parameters:
 *      lm - <usage>
 *          <description>
 *
 *      me - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_lockmgr_cancelwaiting(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me)
{
        lock_head_t* lock;
        lock_request_t* req;
        dbe_lockname_t  name;

        ss_dprintf_1(("dbe_lockmgr_cancelwaiting\n"));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(me);
        ss_dassert(me->lt_wait != NULL);

        req = me->lt_wait;
        CHK_LOCKREQ(req);
        lock = req->lr_head;
        name = lock->lh_name;

        if (!lm->lm_sharedsem) {
#ifdef LMGR_SPLITMUTEX
            SsSemEnter(lm->lm_mutex[LMGR_MUTEX(lm, name)]);
#else
            SsSemEnter(lm->lm_sem);
#endif
        }

        if (req->lr_status != LOCK_GRANTED) {
            if (req->lr_count > 1) {
                ss_dassert(req->lr_status == LOCK_CONVERTING);
                req->lr_status = LOCK_GRANTED;
                req->lr_count--;
            } else {
                ss_dassert(req->lr_status == LOCK_WAITING);
                req->lr_class = LOCK_INSTANT;
                lmgr_unlock(lm, me, lock->lh_relid, lock->lh_name);
            }
        }

        me->lt_wait = NULL;
        if (!lm->lm_sharedsem) {
#ifdef LMGR_SPLITMUTEX
            SsSemExit(lm->lm_mutex[LMGR_MUTEX(lm, name)]);
#else
            SsSemExit(lm->lm_sem);
#endif
        }

}

/*#***********************************************************************\
 * 
 *		lmgr_unlock_class
 * 
 * 
 * 
 * Parameters : 
 * 
 *	lm - 
 *		
 *		
 *	me - 
 *		
 *		
 *	class - 
 *		
 *		
 *	alt_less - 
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
static void lmgr_unlock_class(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        lock_class_t class,
        bool alt_less)
{
        lock_head_t*    lock;
        lock_request_t* req;
        lock_request_t* next;
        ulong           i __attribute__ ((unused));
        bool            mutexed __attribute__ ((unused));

        ss_dprintf_3(("lmgr_unlock_class\n"));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(me);

#ifdef LMGR_SPLITMUTEX
        for (i = 0; i < lmgr_nmutexes; i++) {
            if (me->lt_locks[i] != NULL) {
                SsSemEnter(lm->lm_mutex[i]);
                mutexed = TRUE;
            } else {
                mutexed = FALSE;
            }
            for (req = me->lt_locks[i]; req != NULL; req = next)
#else
            for (req = me->lt_locks; req != NULL; req = next)
#endif
            {
                CHK_LOCKREQ(req);
                next = req->lr_tran_next;
                lock = req->lr_head;
#ifdef LMGR_SPLITMUTEX
                ss_dassert(LMGR_MUTEX(lm, lock->lh_name) == i);
#endif
                if (req->lr_class == class ||
                    (req->lr_class < class && alt_less)) {
                    /* Duration matches the class, or class is less and
                     * alt_less option is chosen.
                     */
                    req->lr_class = LOCK_INSTANT;
                    req->lr_count = 1;
                    lmgr_unlock(lm, me, lock->lh_relid, lock->lh_name);
                }
            }
#ifdef LMGR_SPLITMUTEX
            if (mutexed) {
                SsSemExit(lm->lm_mutex[i]);
            }
        }
#endif
}


bool dbe_lockmgr_getlock(
        dbe_lockmgr_t* lm __attribute__ ((unused)),
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name,
        dbe_lock_mode_t* p_mode,
        bool* p_isverylong_duration)
{
        lock_head_t* lock;
        lock_request_t* req;
        lock_request_t* next;
        ulong           i __attribute__ ((unused));

        ss_dprintf_1(("dbe_lockmgr_getlock\n"));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(me);

#ifdef LMGR_SPLITMUTEX
        for (i = 0; i < lmgr_nmutexes; i++) {
            for (req = me->lt_locks[i]; req != NULL; req = next)
#else
            for (req = me->lt_locks; req != NULL; req = next)
#endif
            {
                CHK_LOCKREQ(req);
                next = req->lr_tran_next;
                lock = req->lr_head;
                if ((ulong)lock->lh_relid == relid && lock->lh_name == name) {
                    /* Lock found.
                     */
                    if (p_mode != NULL) {
                        *p_mode = req->lr_mode;
                    }
                    if (p_isverylong_duration != NULL) {
                        *p_isverylong_duration = (req->lr_class > LOCK_LONG);
                    }
                    return(TRUE);
                }
            }
#ifdef LMGR_SPLITMUTEX
        }
#endif
        
        return(FALSE);
}

dbe_lock_reply_t dbe_lockmgr_lock_convert(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name,
        dbe_lock_mode_t mode,
        bool verylong_duration __attribute__ ((unused)))
{
        dbe_lock_reply_t reply;

        ss_dprintf_1(("dbe_lockmgr_lock_convert\n"));

        ss_dassert(!verylong_duration); /* not supp yet */
        dbe_lockmgr_unlock(lm, me, relid, name);
        reply = dbe_lockmgr_lock(lm, me, relid, name, mode, 0, FALSE, NULL);

        return(reply);
}


/*##**********************************************************************\
 * 
 *		dbe_lockmgr_init
 * 
 * Initializes a lock manager.
 * 
 * Parameters : 
 * 
 *	hashsize - in
 *		Lock manager hash table size.
 *		
 *	escalatelimit - in
 *		Lock escalate limit after which row locks escalate to table locks.
 *		
 * Return value - give : 
 * 
 *      Pointer to the lock manager object.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_lockmgr_t* dbe_lockmgr_init(
        uint        hashsize,
        ulong       escalatelimit,
        SsSemT*     sem)
{
        dbe_lockmgr_t*  lm;
        ulong           i __attribute__ ((unused));

        ss_dprintf_1(("dbe_lockmgr_init\n"));
        ss_dassert(hashsize > 0);

#ifdef LMGR_SPLITMUTEX
        if (dbe_cfg_fastdeadlockdetect) {
            /* With fast deadlock detection we cannot use split mutex
             * because we cannot fix the order in which deadlock detect
             * code would enter mutexes.
             */
            lmgr_nmutexes = 1;
        }
#endif

        lm = SSMEM_NEW(dbe_lockmgr_t);

        ss_debug(lm->lm_chk = DBE_CHK_LOCKMGR;)
        lm->lm_hash = SsMemCalloc(hashsize, sizeof(lm->lm_hash[0]));
        lm->lm_hashsize = hashsize;
#ifdef LMGR_SPLITMUTEX
        ss_dassert(sem == NULL);
        for (i = 0; i < lmgr_nmutexes; i++) {
            lm->lm_mutex[i] = SsSemCreateLocal(SS_SEMNUM_DBE_LOCKMGR);
        }
        lm->lm_sharedsem = FALSE;
#else
        if (sem == NULL) {
            lm->lm_sem = SsSemCreateLocal(SS_SEMNUM_DBE_LOCKMGR);
            lm->lm_sharedsem = FALSE;
        } else {
            lm->lm_sem = sem;
            lm->lm_sharedsem = TRUE;
        }
#endif
        lm->lm_lockcnt = 0;
        lm->lm_lockokcnt = 0;
        lm->lm_lockwaitcnt = 0;
        lm->lm_retrylockwaitcnt = 0;
        lm->lm_locktimeoutcnt = 0;
        lm->lm_escalatelimit = escalatelimit;
        lm->lm_useescalation = FALSE;
        lm->lm_uselocks = TRUE;
        lm->lm_disablerowspermessage = FALSE;
        lm->lm_cachedrelid = 0;
        lm->lm_cachedlock = NULL;
#ifdef LMGR_SPLITMUTEX
        for (i = 0; i < lmgr_nmutexes; i++) {
            lm->lm_freelock[i] = NULL;
            lm->lm_nfreelocks[i] = 0;
        }
        lm->lm_maxfreelocks = N_FREE_LOCKS / lmgr_nmutexes;
        for (i = 0; i < lmgr_nmutexes; i++) {
            lm->lm_freerequest[i] = NULL;
            lm->lm_nfreerequests[i] = 0;
        }
        lm->lm_maxfreerequests = N_FREE_REQUESTS / lmgr_nmutexes;
#else
        lm->lm_freelock = NULL;
        lm->lm_nfreelocks = 0;
        lm->lm_maxfreelocks = N_FREE_LOCKS;
        lm->lm_freerequest = NULL;
        lm->lm_nfreerequests = 0;
        lm->lm_maxfreerequests = N_FREE_REQUESTS;
#endif
        lm->lm_maxnlocks = 0;
        lm->lm_nchains = 0;
        lm->lm_maxnchains = 0;
        lm->lm_maxpath = 0;

        return(lm);
}

/*##**********************************************************************\
 * 
 *		dbe_lockmgr_done
 * 
 * Releases lock manager object.
 * 
 * Parameters : 
 * 
 *	lm - in, take
 *		Lock manager.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_lockmgr_done(dbe_lockmgr_t* lm)
{
        ulong i __attribute__ ((unused));

        ss_dprintf_1(("dbe_lockmgr_done\n"));
        CHK_LOCKMGR(lm);

#ifdef SS_DEBUG
        for (i = 0; i < lm->lm_hashsize; i++) {
#if 0
            ss_dassert(lm->lm_hash[i] == NULL);
#else
            if (lm->lm_hash[i] != NULL) {
                ss_testlog_print((char *)"%d@%s: non-freed lock in lock table.\n",
                                 __LINE__, (char *)__FILE__);
            }
#endif
        }
#endif
        lm->lm_maxfreelocks = 0;
#ifdef LMGR_SPLITMUTEX
        for (i = 0; i < lmgr_nmutexes; i++) {
            while (lm->lm_freelock[i] != NULL) {
                lock_head_t* lock;
                ss_dassert(lm->lm_freelock[i]->lh_chk == DBE_CHK_FREELOCKHEAD);
                ss_debug(lm->lm_freelock[i]->lh_chk = DBE_CHK_LOCKHEAD);
                lock = lm->lm_freelock[i]->lh_chain;
                SsMemFree(lm->lm_freelock[i]);
                lm->lm_freelock[i] = lock;
            }
            lm->lm_maxfreerequests = 0;
            while (lm->lm_freerequest[i] != NULL) {
                lock_request_t* req;
                ss_dassert(lm->lm_freerequest[i]->lr_chk == DBE_CHK_FREELOCKREQ);
                ss_debug(lm->lm_freerequest[i]->lr_chk = DBE_CHK_LOCKREQ);
                req = lm->lm_freerequest[i]->lr_queue;
                SsMemFree(lm->lm_freerequest[i]);
                lm->lm_freerequest[i] = req;
            }
        }
#else
        while (lm->lm_freelock != NULL) {
            lock_head_t* lock;
            ss_dassert(lm->lm_freelock->lh_chk == DBE_CHK_FREELOCKHEAD);
            ss_debug(lm->lm_freelock->lh_chk = DBE_CHK_LOCKHEAD);
            lock = lm->lm_freelock->lh_chain;
            SsMemFree(lm->lm_freelock);
            lm->lm_freelock = lock;
        }
        lm->lm_maxfreerequests = 0;
        while (lm->lm_freerequest != NULL) {
            lock_request_t* req;
            ss_dassert(lm->lm_freerequest->lr_chk == DBE_CHK_FREELOCKREQ);
            ss_debug(lm->lm_freerequest->lr_chk = DBE_CHK_LOCKREQ);
            req = lm->lm_freerequest->lr_queue;
            SsMemFree(lm->lm_freerequest);
            lm->lm_freerequest = req;
        }
#endif
        
#ifdef LMGR_SPLITMUTEX
        for (i = 0; i < lmgr_nmutexes; i++) {
            SsSemFree(lm->lm_mutex[i]);
        }
#else
        if (!lm->lm_sharedsem) {
            SsSemFree(lm->lm_sem);
        }
#endif
        SsMemFree(lm->lm_hash);
        SsMemFree(lm);
}

/*##**********************************************************************\
 * 
 *		dbe_lockmgr_setuselocks
 * 
 * Sets the flag that says if we should use locks or not. We do not want
 * to use locks in Hot Standby secondary server because all conflicts
 * are resolved in perimary server.
 * 
 * Parameters : 
 * 
 *	lm - 
 *		
 *		
 *	uselocks - 
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
void dbe_lockmgr_setuselocks(
        dbe_lockmgr_t* lm,
        bool uselocks)
{
        ss_dprintf_1(("dbe_lockmgr_setuselocks:uselocks=%d\n", uselocks));
        CHK_LOCKMGR(lm);

        lm->lm_uselocks = uselocks;
}

/*##**********************************************************************\
 * 
 *		dbe_lockmgr_printinfo
 * 
 * Prints information of the lock manager.
 * 
 * Parameters : 
 * 
 *	fp - 
 *		
 *		
 *	lm - 
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
void dbe_lockmgr_printinfo(
        void* fp,
        dbe_lockmgr_t* lm)
{
        CHK_LOCKMGR(lm);

#ifndef LMGR_SPLITMUTEX
        SsSemEnter(lm->lm_sem);
#endif

        SsFprintf(fp, "  Lock ok %ld, wait %ld, retry wait %ld, timeout %ld\n",
            lm->lm_lockokcnt,
            lm->lm_lockwaitcnt,
            lm->lm_retrylockwaitcnt,
            lm->lm_locktimeoutcnt);
        SsFprintf(fp, "  Active locks %ld\n",
            lm->lm_lockcnt);

#ifndef LMGR_SPLITMUTEX
        SsSemExit(lm->lm_sem);
#endif
}

/* Voyager */

/*##**********************************************************************\
 * 
 *		dbe_lockmgr_getlockcount
 * 
 * 
 * 
 * Parameters : 
 * 
 *	lm - 
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
ulong dbe_lockmgr_getlockcount(
        dbe_lockmgr_t* lm)
{
        ulong lockcnt;
        CHK_LOCKMGR(lm);

        /* SsSemEnter(lm->lm_sem); */
        if (lm->lm_lockcnt < 0) {
            lockcnt = 0;
        } else {
            lockcnt = lm->lm_lockcnt;
        }
        /* SsSemExit(lm->lm_sem); */

        return(lockcnt);
}
/* Voyager end */

/*##**********************************************************************\
 * 
 *		dbe_lockmgr_lock
 * 
 * Tries to lock object identified by lockname with a type specified by
 * parameter type.
 * 
 * Parameters : 
 * 
 *	lm - use
 *		Lock manager.
 *		
 *	locktran - use
 *		Lock list.
 *		
 *	relid - in
 *		Relation id.
 *		
 *	lockname - in
 *		Lock name.
 *		
 *	mode - in
 *		Lock mode.
 *		
 *	timeout - in
 *		Lock timeout.
 *		
 *	bouncep - in
 *		If TRUE, nothing is locked but a check is made if the locking
 *		is possible.
 *		
 * Return value : 
 * 
 *      Status os lock operation, one of dbe_lock_reply_t.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_lock_reply_t dbe_lockmgr_lock(
        dbe_lockmgr_t*      lm,
        dbe_locktran_t*     locktran,
        ulong               relid,
        dbe_lockname_t      lockname,
        dbe_lock_mode_t     mode,
        long                timeout,
        bool                bouncep,
        bool*               p_newlock)
{
        dbe_lock_reply_t reply;
        
        SS_PUSHNAME("dbe_lockmgr_lock");
        
        ss_dprintf_1(("dbe_lockmgr_lock:userid = %d, timeout = %ld, relid = %ld, lockname = %ld, locktran = %ld\n",
            rs_sysi_userid(locktran->lt_cd), timeout, relid, (long)lockname, (long)locktran));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(locktran);
#ifdef DBE_LOCK_MSEC
        /* This is a very temporary assertion to check against some bad
           behaviour from outside of MME. */
        /* ss_dassert(timeout == 0 || (timeout % 1000) == 0); */
        ss_dassert(timeout >= 0);
#endif
#if 0
        if (relid == lockname && mode == LOCK_X) {
            relid = 1;
            lockname = 1;
        }
#endif

        FAKE_CODE_BLOCK(
                FAKE_DBE_SLEEP_BEFORE_LOCKING,
                {
                    if (relid >= 10000 && lockname >= 10000) {
                        SsPrintf("FAKE_DBE_SLEEP_BEFORE_LOCKING: sleeping...\n");
                        SsThrSleep(5000);
                        SsPrintf("FAKE_DBE_SLEEP_BEFORE_LOCKING: done sleeping.\n");
                    }
                });

#ifdef LMGR_SPLITMUTEX
        SsSemEnter(lm->lm_mutex[LMGR_MUTEX(lm, lockname)]);
#else
        SsSemEnter(lm->lm_sem);
#endif

#ifdef DBE_LOCKESCALATE_OPT
        ss_dassert(!lm->lm_useescalation);
        if (relid > DBE_LOCKRELID_MAXSPECIAL && lm->lm_useescalation) {
            lock_request_t* req;
            dbe_lock_mode_t rellockmode;
            rellockmode = lockmgr_getrellockmode(mode);
            ss_dprintf_2(("dbe_lockmgr_lock:lock table with mode %d\n", rellockmode));
            ss_error;
            reply = lmgr_lock(
                        lm,
                        locktran,
                        DBE_LOCKRELID_REL,
                        relid,
                        rellockmode,
                        LOCK_LONG,
                        timeout,
                        bouncep,
                        &req,
                        p_newlock);
            if (reply != LOCK_OK) {
                ss_dprintf_2(("dbe_lockmgr_lock:table lock return %d\n", reply));
                goto exit_function;
            }
            
            /* Only try to escalate if not bouncing. */
            if (!bouncep) {
                CHK_LOCKREQ(req);
                if ((ulong)req->lr_count > lm->lm_escalatelimit) {
                    ss_dprintf_2(("dbe_lockmgr_lock:table lock count over escalate limit\n"));
                    if (req->lr_mode > LOCK_IX) {
                        /* Table lock already escalated, lock is ok. */
                        ss_dprintf_2(("dbe_lockmgr_lock:table lock already escalated\n"));
                        reply = LOCK_OK;
                        goto exit_function;
                    } else {
                        /* Lock the whole table. */
                        ss_dprintf_2(("dbe_lockmgr_lock:escalate table lock\n"));
                        lockname = relid;
                        relid = DBE_LOCKRELID_REL;
                    }
                }
            }
        }
#endif /* DBE_LOCKESCALATE_OPT */

        reply = lmgr_lock(
                    lm,
                    locktran,
                    relid,
                    lockname,
                    mode,
                    LOCK_LONG,
                    timeout,
                    bouncep,
                    NULL,
                    p_newlock);

        ss_dprintf_2(("dbe_lockmgr_lock:return %d\n", reply));

 exit_function:
#ifdef LMGR_SPLITMUTEX
        SsSemExit(lm->lm_mutex[LMGR_MUTEX(lm, lockname)]);
#else
        SsSemExit(lm->lm_sem);
#endif
        
        SS_POPNAME;
        return(reply);
}

dbe_lock_reply_t dbe_lockmgr_lock_long(
        dbe_lockmgr_t*      lm,
        dbe_locktran_t*     locktran,
        ulong               relid,
        dbe_lockname_t      lockname,
        dbe_lock_mode_t     mode,
        long                timeout,
        bool                bouncep)
{
        dbe_lock_reply_t reply;

        SS_PUSHNAME("dbe_lockmgr_lock_long");
        ss_dprintf_1(("dbe_lockmgr_lock_long:userid = %d, timeout = %ld, relid = %ld, lockname = %ld, locktran = %ld\n",
            rs_sysi_userid(locktran->lt_cd), timeout, relid, (long)lockname, (long)locktran));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(locktran);
#ifdef DBE_LOCK_MSEC
        /* This is a very temporary assertion to check against some bad
           behaviour from outside of MME. */
        /* ss_dassert(timeout == 0 || (timeout % 1000) == 0); */
        ss_dassert(timeout >= 0);
#endif

#ifdef LMGR_SPLITMUTEX
        SsSemEnter(lm->lm_mutex[LMGR_MUTEX(lm, lockname)]);
#else
        SsSemEnter(lm->lm_sem);
#endif

#ifdef DBE_LOCKESCALATE_OPT
        ss_dassert(!lm->lm_useescalation);
        if (relid > DBE_LOCKRELID_MAXSPECIAL && lm->lm_useescalation) {
            lock_request_t* req;
            dbe_lock_mode_t rellockmode;
            rellockmode = lockmgr_getrellockmode(mode);
            ss_dprintf_2(("dbe_lockmgr_lock:lock table with mode %d\n", rellockmode));
            ss_error;
            reply = lmgr_lock(
                        lm,
                        locktran,
                        DBE_LOCKRELID_REL,
                        relid,
                        rellockmode,
                        LOCK_LONG,
                        timeout,
                        bouncep,
                        &req,
                        NULL);
            if (reply != LOCK_OK) {
                ss_dprintf_2(("dbe_lockmgr_lock:table lock return %d\n", reply));
                goto exit_function;
            }

            /* Only try to escalate if not bouncing. */
            if (!bouncep) {
                CHK_LOCKREQ(req);
                if ((ulong)req->lr_count > lm->lm_escalatelimit) {
                    ss_dprintf_2(("dbe_lockmgr_lock:table lock count over escalate limit\n"));
                    if (req->lr_mode > LOCK_IX) {
                        /* Table lock already escalated, lock is ok. */
                        ss_dprintf_2(("dbe_lockmgr_lock:table lock already escalated\n"));
                        reply = LOCK_OK;
                        goto exit_function;
                    } else {
                        /* Lock the whole table. */
                        ss_dprintf_2(("dbe_lockmgr_lock:escalate table lock\n"));
                        lockname = relid;
                        relid = DBE_LOCKRELID_REL;
                    }
                }
            }
        }
#endif /* DBE_LOCKESCALATE_OPT */

        reply = lmgr_lock(
                    lm,
                    locktran,
                    relid,
                    lockname,
                    mode,
                    LOCK_VERY_LONG,
                    timeout,
                    bouncep,
                    NULL,
                    NULL);

        ss_dprintf_2(("dbe_lockmgr_lock_long:return %d\n", reply));

 exit_function:
#ifdef LMGR_SPLITMUTEX
        SsSemExit(lm->lm_mutex[LMGR_MUTEX(lm, lockname)]);
#else
        SsSemExit(lm->lm_sem);
#endif

        SS_POPNAME;
        return(reply);
}

#define LMGR_FILL_MME_PMON(lm) do { \
        SS_PMON_SET(SS_PMON_MME_NLOCKS, (lm)->lm_lockcnt); \
        if ((lm)->lm_lockcnt > (lm)->lm_maxnlocks) { \
            (lm)->lm_maxnlocks = (lm)->lm_lockcnt; \
        } \
        SS_PMON_SET(SS_PMON_MME_MAXNLOCKS, (lm)->lm_maxnlocks); \
        SS_PMON_SET(SS_PMON_MME_NCHAINS, (lm)->lm_nchains); \
        SS_PMON_SET(SS_PMON_MME_MAXNCHAINS, (lm)->lm_maxnchains); \
        SS_PMON_SET(SS_PMON_MME_MAXPATH, (lm)->lm_maxpath); \
} while (FALSE)

dbe_lock_reply_t dbe_lockmgr_lock_mme(
        dbe_lockmgr_t*      lm,
        dbe_locktran_t*     locktran,
        ulong*              relid,
        dbe_lockname_t*     lockname,
        dbe_lock_mode_t     mode,
        long                timeout,
        bool                bouncep,
        bool                escalatep)
{
        lock_request_t*     rowreq;
        lock_request_t*     tablereq;
        dbe_lock_mode_t     rellockmode;
        dbe_lock_reply_t    reply;
        SsSemT*             mutex_taken = NULL;

        ss_dprintf_1(("dbe_lockmgr_lock_mme: userid = %d, timeout = %ld, relid = %ld, lockname = %ld, locktran = %ld\n",
            rs_sysi_userid(locktran->lt_cd), timeout, *relid, (long) *lockname, (long)locktran));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(locktran);
        ss_dassert(locktran->lt_wait == NULL);
        ss_dassert(timeout >= 0);

        FAKE_CODE_BLOCK(
                FAKE_DBE_SLEEP_BEFORE_LOCKING,
                {
                    SsPrintf("FAKE_DBE_SLEEP_BEFORE_LOCKING: sleeping...\n");
                    SsThrSleep(5000);
                    SsPrintf("FAKE_DBE_SLEEP_BEFORE_LOCKING: done sleeping.\n");
                });

#ifndef LMGR_SPLITMUTEX
        if (!lm->lm_sharedsem) {
            SsSemEnter(lm->lm_sem);
            mutex_taken = lm->lm_sem;
        }
#endif

#ifdef DBE_LOCKESCALATE_OPT
        ss_dassert(*relid > DBE_LOCKRELID_MAXSPECIAL);
        ss_dassert(!bouncep);

        tablereq = NULL;
        rowreq = NULL;

        if (!lm->lm_useescalation) {
            escalatep = FALSE;
        }
        
        if (escalatep) {
#ifdef LMGR_SPLITMUTEX
            mutex_taken = lm->lm_mutex[LMGR_MUTEX(lm, *relid)];
            SsSemEnter(mutex_taken);
#endif
            rellockmode = lockmgr_getrellockmode(mode);
            ss_dprintf_2(("dbe_lockmgr_lock_mme: lock table with mode %d\n", rellockmode));
            reply = lmgr_lock(
                        lm,
                        locktran,
                        DBE_LOCKRELID_REL,
                        *relid,
                        rellockmode,
                        LOCK_LONG,
                        timeout,
                        bouncep,
                        &tablereq,
                        NULL);
            if (reply != LOCK_OK) {
                *lockname = *relid;
                *relid = DBE_LOCKRELID_REL;
                ss_dprintf_2(("dbe_lockmgr_lock_mme: table lock return %d\n", reply));
                goto exit_function;
            }

            /* Only try to escalate if not bouncing. */
            if (!bouncep) {            
                CHK_LOCKREQ(tablereq);
                if ((ulong)tablereq->lr_okcount > lm->lm_escalatelimit) {
                    ss_dprintf_2(("dbe_lockmgr_lock_nomutex:table lock count over escalate limit\n"));
                    /* Let's use X-locks on table level instead of U. */
                    if (mode == LOCK_U) {
                        mode = LOCK_X;
                    }
                    if (tablereq->lr_mode >= mode) {
                        /* Table lock already escalated, lock is ok. */
                        ss_dprintf_2(("dbe_lockmgr_lock_nomutex:table lock already escalated\n"));
                        reply = LOCK_OK;
                        goto exit_function;
                    } else {
                        /* Lock the whole table. */
                        ss_dprintf_2(("dbe_lockmgr_lock_nomutex:escalate table lock\n"));
                        *lockname = *relid;
                        *relid = DBE_LOCKRELID_REL;
                    }
                }
            }

#ifdef LMGR_SPLITMUTEX
            SsSemExit(mutex_taken);
            mutex_taken = NULL;
#endif
        }
#endif /* DBE_LOCKESCALATE_OPT */

#ifdef LMGR_SPLITMUTEX
        mutex_taken = lm->lm_mutex[LMGR_MUTEX(lm, *lockname)];
        SsSemEnter(mutex_taken);
#endif
        
        reply = lmgr_lock(
                    lm,
                    locktran,
                    *relid,
                    *lockname,
                    mode,
                    LOCK_LONG,
                    timeout,
                    bouncep,
                    &rowreq,
                    NULL);

#ifdef DBE_LOCKESCALATE_OPT
        if (reply == LOCK_OK) {
            CHK_LOCKREQ(rowreq);
            if (escalatep
                && *relid != DBE_LOCKRELID_REL
                && rowreq->lr_okcount > 1) {
                /* This row has been locked more than once. */
                /* XXX - this is a bit ugly kludge, but we reduce the number
                   of table lock requests here to count every row as only
                   one lock to the table. */
                ss_dassert(tablereq->lr_okcount > 1);
                if (tablereq->lr_okcount > 1) {
                    tablereq->lr_okcount--;
                }
            }
        }
#endif

 exit_function:
        LMGR_FILL_MME_PMON(lm);
        ss_dprintf_2(("dbe_lockmgr_lock_mme: return %d\n", reply));

#ifdef LMGR_SPLITMUTEX
        SsSemExit(mutex_taken);
#else
        if (!lm->lm_sharedsem) {
            SsSemExit(lm->lm_sem);
        }
#endif

        return(reply);
}

dbe_lock_reply_t dbe_lockmgr_relock_mme(
        dbe_lockmgr_t*      lm,
        dbe_locktran_t*     locktran,
        ulong               relid,
        dbe_lockname_t      lockname,
        dbe_lock_mode_t     mode,
        long                timeout)
{
        dbe_lock_reply_t    reply;
        lock_head_t*        lock __attribute__ ((unused));

        ss_dprintf_1(("dbe_lockmgr_relock_mme: userid = %d, timeout = %ld, relid = %ld, lockname = %ld, locktran = %ld\n",
            rs_sysi_userid(locktran->lt_cd), timeout, relid, (long) lockname, (long)locktran));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(locktran);
        ss_dassert(locktran->lt_wait != NULL);
        ss_dassert(timeout >= 0);
        ss_debug({
            lock = locktran->lt_wait->lr_head;
            ss_dassert((ulong)lock->lh_relid == relid);
            ss_dassert(lock->lh_name == lockname);
        });

#ifdef LMGR_SPLITMUTEX
        SsSemEnter(lm->lm_mutex[LMGR_MUTEX(lm, lockname)]);
#else
        if (!lm->lm_sharedsem) {
            SsSemEnter(lm->lm_sem);
        }
#endif

        reply = lmgr_lock(
                    lm,
                    locktran,
                    relid,
                    lockname,
                    mode,
                    LOCK_LONG,
                    timeout,
                    FALSE,
                    NULL,
                    NULL);

#ifdef LMGR_SPLITMUTEX
        SsSemExit(lm->lm_mutex[LMGR_MUTEX(lm, lockname)]);
#else
        if (!lm->lm_sharedsem) {
            SsSemExit(lm->lm_sem);
        }
#endif
        
        ss_dprintf_2(("dbe_lockmgr_relock_mme: return %d\n", reply));

        return(reply);
}


/*##**********************************************************************\
 * 
 *		dbe_lockmgr_unlockall
 * 
 * Unlocks all objects in the list locktran.
 * 
 * Parameters : 
 * 
 *	lm - use
 *		Lock manager.
 *		
 *	locktran - use
 *		Lock list.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_lockmgr_unlockall(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran)
{
        ss_dprintf_1(("dbe_lockmgr_unlockall:userid = %d, locktran = %ld\n", rs_sysi_userid(locktran->lt_cd), locktran));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(locktran);

        if (locktran->lt_locks != NULL) {

#ifndef LMGR_SPLITMUTEX
            SsSemEnter(lm->lm_sem);
#endif

            lmgr_unlock_class(lm, locktran, LOCK_LONG, TRUE);

            locktran->lt_wait = NULL;

#ifndef LMGR_SPLITMUTEX
            SsSemExit(lm->lm_sem);
#endif
        }
        
        ss_dprintf_2(("dbe_lockmgr_unlockall:return\n"));
}

void dbe_lockmgr_unlockall_nomutex(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran)
{
        ss_dprintf_1(("dbe_lockmgr_unlockall:userid = %d, locktran = %ld\n", rs_sysi_userid(locktran->lt_cd), locktran));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(locktran);

        ss_error;

        lmgr_unlock_class(lm, locktran, LOCK_LONG, TRUE);

        locktran->lt_wait = NULL;

        ss_dprintf_2(("dbe_lockmgr_unlockall_nomutex:return\n"));
}

void dbe_lockmgr_unlockall_mme(
        dbe_lockmgr_t*  lm,
        dbe_locktran_t* locktran)
{
        ss_dprintf_1(("dbe_lockmgr_unlockall_mme:userid = %d, locktran = %ld\n", rs_sysi_userid(locktran->lt_cd), locktran));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(locktran);

        if (locktran->lt_locks == NULL) {
            /* Nothing to unlock, avoid mutexing by returning immediately. */
            ss_dassert(locktran->lt_wait == NULL);
            ss_dprintf_2(("dbe_lockmgr_unlockall_mme:return shortcut\n"));
            return;
        }

#ifndef LMGR_SPLITMUTEX
        if (!lm->lm_sharedsem) {
            SsSemEnter(lm->lm_sem);
        }
#endif

        lmgr_unlock_class(lm, locktran, LOCK_LONG, TRUE);

        locktran->lt_wait = NULL;

        LMGR_FILL_MME_PMON(lm);

#ifndef LMGR_SPLITMUTEX
        if (!lm->lm_sharedsem) {
            SsSemExit(lm->lm_sem);
        }
#endif

        ss_dprintf_2(("dbe_lockmgr_unlockall_mme:return\n"));
}
        

void dbe_lockmgr_unlockall_long(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran)
{
        ss_dprintf_1(("dbe_lockmgr_unlockall:userid = %d, locktran = %ld\n", rs_sysi_userid(locktran->lt_cd), locktran));
        CHK_LOCKMGR(lm);
        CHK_LOCKTRAN(locktran);

#ifndef LMGR_SPLITMUTEX
        SsSemEnter(lm->lm_sem);
#endif

        lmgr_unlock_class(lm, locktran, LOCK_VERY_LONG, TRUE);

        locktran->lt_wait = NULL;

#ifndef LMGR_SPLITMUTEX
        SsSemExit(lm->lm_sem);
#endif
        
        ss_dprintf_2(("dbe_lockmgr_unlockall:return\n"));
}

/*##**********************************************************************\
 * 
 *		dbe_lockmgr_setlockdisablerowspermessage
 * 
 * Enables or disables rows per message optimization when a row is
 * locked. By default rows per message is enabled.
 * 
 * Parameters : 
 * 
 *	lm - in, use
 *		
 *		
 *	lockdisablerowspermessage - in
 *		If TRUE, rows per message is disabled. If FALSE, it is enabled.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_lockmgr_setlockdisablerowspermessage(
        dbe_lockmgr_t* lm,
        bool lockdisablerowspermessage)
{
        CHK_LOCKMGR(lm);

        lm->lm_disablerowspermessage = lockdisablerowspermessage;
}

/*##**********************************************************************\
 *
 *      dbe_lockmgr_setuseescalation
 *
 * Enables or disables lock escalation in this lock manager.
 *
 * Parameters:
 *      lm - in, use
 *          The lock manager.
 *
 *      useescalation - in
 *          New value for using escalation.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_lockmgr_setuseescalation(
        dbe_lockmgr_t*  lm,
        bool            useescalation)
{
        CHK_LOCKMGR(lm);

        lm->lm_useescalation = useescalation;
}

/*##**********************************************************************\
 * 
 *		dbe_locktran_init
 * 
 * 
 * 
 * Parameters :
 * 
 *      cd - in, hold
 *          Client data.
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_locktran_t* dbe_locktran_init(rs_sysi_t* cd)
{
        dbe_locktran_t* locktran;
        ulong           i __attribute__ ((unused));

        ss_dprintf_1(("dbe_locktran_init:userid = %d\n", rs_sysi_userid(cd)));
        
        locktran = SSMEM_NEW(dbe_locktran_t);

        ss_debug(locktran->lt_chk = DBE_CHK_LOCKTRAN;)
        locktran->lt_cd = cd;
#ifdef LMGR_SPLITMUTEX
        for (i = 0; i < lmgr_nmutexes; i++) {
            locktran->lt_locks[i] = NULL;
        }
#else
        locktran->lt_locks = NULL;
#endif
        locktran->lt_wait = NULL;
        locktran->lt_cycle = NULL;

        ss_dprintf_2(("dbe_locktran_init:locktran = %ld\n", (long)locktran));

        return(locktran);
}

/*##**********************************************************************\
 * 
 *		dbe_locktran_done
 * 
 * 
 * 
 * Parameters : 
 * 
 *	locktran - 
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
void dbe_locktran_done(dbe_locktran_t* locktran)
{
        ss_dprintf_1(("dbe_locktran_done:userid = %d, locktran = %ld\n", rs_sysi_userid(locktran->lt_cd), (long)locktran));
        CHK_LOCKTRAN(locktran);

#ifdef LMGR_SPLITMUTEX
        ss_debug({
            ulong i;
            for (i = 0; i < lmgr_nmutexes; i++) {
                ss_dassert(locktran->lt_locks[i] == NULL);
            }
        })
#else
        ss_dassert(locktran->lt_locks == NULL);
#endif
        ss_dassert(locktran->lt_wait == NULL);

        SsMemFree(locktran);
}


#endif /* SS_NOLOCKING */
