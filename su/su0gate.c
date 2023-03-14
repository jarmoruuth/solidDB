/*************************************************************************\
**  source       * su0gate.c
**  directory    * su
**  description  * Gate semaphores.
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

This module implements gate semaphores. Gate semaphores allow
different gate modes. For shared access mode other shared mode
operations are allowed, but exclusive operations must wait. 
Exclusive access mode allows only one process to continue, and all
other processes must wait.

The implementation is fair in a sense that all shared mode operations
that become before exclusive operation can proceed. When there is
at least one exclusive operation, no other operations can proceed.
The operations are kept in a queue.

Example:

        Operations S (shared) and E (exclusive) arrive at the following order

        Active: <none>

        Queue: S1 S2 E3 S4 S5 E6

        The S1 gets shared access. The following operation
        S2 can continue immediately. The operation E3 must wait.

        Active: S1 S2

        Queue: E3 S4 S5 E6

        When S1 and S2 are ended, E3 can continue.

        Active: E3

        Queue: S4 S5 E6

        When E3 is ended, S4 and S5 can continue.

        Active: S4 S5

        Queue: E6

        When S4 and S5 are ended, E6 can continue.

        Active: E6

        Queue: <none>

The queueing is done so that each operation has a queueing ticket.
This ticket object is a local variable in the su_gate_enter_xxxx() routines.

Limitations:
-----------

None.

Error handling:
--------------

None.

Objects used:
------------

The implementation utilizes portable semaphores from sssem.?.
(Both message and mutex)

Semaphores      sssem.c
Messages        sssem.c

Preconditions:
-------------


Multithread considerations:
--------------------------

None.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssthread.h>
#include <sslimits.h>
#include <sssprint.h>
#include <sspmon.h>

#include "su0mesl.h"
#include "su0rbtr.h"
#include "su1check.h"
#include "su0gate.h"
#ifdef SS_DEBUG
#include "su0list.h"
#endif

#undef su_gate_init
#ifdef SS_MT

typedef struct su_queue_ticket_st su_qticket_t;

/* the Queueing ticket for getting access to gated resource */
struct su_queue_ticket_st {
        bool            t_shared;   /* shared mode request? */
        su_mes_t*       t_mes;      /* message semaphore */
        su_qticket_t*   t_next;     /* next req in queue */
};

struct su_gate_st {
        int             g_nwait;
        int             g_nshared;      /* # of shared mode accesses */
        int             g_nexclusive;   /* # of exclusive mode accesses */
        int             g_maxexclusive; /* max # of active exclusive mode accesses */
        SsFlatMutexT    g_mutex;          /* concurrency control mutex */
        su_qticket_t*   g_queue_first;  /* ptr to first ticket in queue */
        su_qticket_t*   g_queue_last;   /* ptr to last ticket in queue */
        su_meslist_t    g_meslist;
        ss_profile(SsSemDbgT* g_semdbg;)
        ss_semnum_t             g_sem_num;       /* Order number in hierarchy */
        ss_debug(bool           g_semnum_checkonlyp;)
        ss_debug(unsigned int   g_exclusivethread;)
        ss_debug(su_list_t*     g_sharedthreads;)
        ss_debug(char           g_name[80];)
};

#ifdef SS_DEBUG

typedef struct {
        int     si_thrid;
        char**  si_callstack;
} su_gate_shareinfo_t;

static su_gate_shareinfo_t* su_gate_shareinfo_init(void)
{
        su_gate_shareinfo_t* si;

        si = SSMEM_NEW(su_gate_shareinfo_t);

        si->si_thrid = SsThrGetNativeId();
        si->si_callstack = SsMemTrcCopyCallStk();

        return(si);
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		SU_GATE_LINKTOQUEUE
 * 
 * Links a ticket to the end of the wait queue
 * 
 * Parameters : 
 * 
 *	gate - in out, use
 *		
 *		
 *	ticket - in out, hold
 *		
 *		
 * Return value : none
 * 
 * Limitations  : This is a macro!
 * 
 * Globals used : 
 */
#define SU_GATE_LINKTOQUEUE(gate, p_ticket) \
{\
(gate)->g_nwait++;\
if ((gate)->g_queue_last == NULL) {\
        (gate)->g_queue_last =\
            (gate)->g_queue_first = (p_ticket);\
} else {\
        (gate)->g_queue_last->t_next = (p_ticket);\
        (gate)->g_queue_last = (p_ticket);\
}\
(p_ticket)->t_next = NULL;\
}

static su_gate_t* gate_init(
        ss_semnum_t num __attribute__ ((unused)),
        bool checkonlyp __attribute__ ((unused)))
{
        su_gate_t* gate;

        gate = SSMEM_NEW(su_gate_t);

        gate->g_nwait = 0;
        gate->g_nshared = 0;
        gate->g_nexclusive = 0;
        gate->g_maxexclusive = 1;
        SsFlatMutexInit(&(gate->g_mutex), SS_SEMNUM_NOTUSED); /* No number possible/easy. */
        gate->g_queue_first = NULL;
        gate->g_queue_last = NULL;
        ss_profile(gate->g_semdbg = NULL;)
        ss_debug(gate->g_exclusivethread = 0;)
        ss_debug(gate->g_sharedthreads = NULL;)
        gate->g_sem_num = num;
        ss_debug(gate->g_semnum_checkonlyp = checkonlyp);
        ss_debug(SsSprintf(gate->g_name, "%08lX:%d", (ulong)gate, gate->g_sem_num));

        (void)su_meslist_init_nomutex(&gate->g_meslist);

        return(gate);
}

/*##**********************************************************************\
 * 
 *		su_gate_init
 * 
 * Creates a new gate object
 * 
 * Parameters : 	 - none
 * 
 * Return value - give : 
 *      pointer to gate object
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
su_gate_t* su_gate_init(ss_semnum_t num, bool checkonlyp)
{
        su_gate_t* gate;

        gate = gate_init(num, checkonlyp);

        ss_profile(gate->g_semdbg = SsSemDbgAdd(num, (char *)__FILE__, __LINE__, TRUE);)

        ss_dprintf_1(("su_gate_init(%s, semnum=%d)\n",
                      gate->g_name, (int)num));
        return(gate);
}

#ifdef SS_SEM_DBG
/*##**********************************************************************\
 * 
 *		su_gate_init_dbg
 * 
 * Creates a new gate object
 * 
 * Parameters : 	 - none
 * 
 * Return value - give : 
 *      pointer to gate object
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
su_gate_t* su_gate_init_dbg(ss_semnum_t num, bool checkonlyp, char* file, int line)
{
        su_gate_t* gate;

        gate = gate_init(num, checkonlyp);

        ss_profile(gate->g_semdbg = SsSemDbgAdd(num, file, line, TRUE);)
        ss_dprintf_1(("su_gate_init_dbg(%s, semnum=%d) file=%s line=%d\n",
                      gate->g_name, (int)num, file, line));

        return(gate);
}

#endif /* SS_SEM_DBG */

/*##**********************************************************************\
 * 
 *		su_gate_done
 * 
 * Deletes a gate object
 * 
 * Parameters : 
 * 
 *	gate - in, take
 *		pointer to gate object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_gate_done(su_gate_t* gate)
{
        ss_dassert(gate != NULL);
        ss_rc_dassert(gate->g_nshared == 0, gate->g_sem_num);
        ss_rc_dassert(gate->g_nexclusive == 0, gate->g_sem_num);

        ss_debug({
            if (gate->g_sharedthreads != NULL) {
                su_list_done(gate->g_sharedthreads);
            }
        })
        SsFlatMutexDone(gate->g_mutex);
        su_meslist_done(&gate->g_meslist);
        SsMemFree(gate);
}

/*##**********************************************************************\
 * 
 *		su_gate_enter_shared
 * 
 * Enters the gated section in shared mode
 * 
 * Parameters : 
 * 
 *	gate - in out, use
 *		pointer to gate object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_gate_enter_shared(su_gate_t* gate)
{
        su_qticket_t ticket;
     
        ss_dprintf_1(("su_gate_enter_shared(%s)\n",
                      gate->g_name));
        ss_dassert(gate != NULL);
        ss_dassert(!su_gate_thread_is_shared(gate)
                   && (gate->g_maxexclusive != 1 || !su_gate_thread_is_exclusive(gate)));
        SsFlatMutexLock(gate->g_mutex);

#ifdef SS_DEBUG
        if (gate->g_sem_num != SS_SEMNUM_NOTUSED) {
            if (gate->g_semnum_checkonlyp) {
                SsSemStkEnterCheck(gate->g_sem_num);
            } else {
                SsSemStkEnter(gate->g_sem_num);
            }
        }
#endif
        ss_profile(if (gate->g_semdbg != NULL) gate->g_semdbg->sd_callcnt++;)

        if (gate->g_nexclusive != 0 || gate->g_queue_first != NULL) {

            /* cannot proceed, go to end of the wait queue */
            ticket.t_shared = TRUE;
            ticket.t_mes = su_meslist_mesinit(&gate->g_meslist);
            SU_GATE_LINKTOQUEUE(gate, &ticket);
            ss_profile(if (gate->g_semdbg != NULL) gate->g_semdbg->sd_waitcnt++;)

            SsFlatMutexUnlock(gate->g_mutex);
            switch (gate->g_sem_num) {
	         	case SS_SEMNUM_DBE_PESSGATE:
                    ss_pprintf_1(("su_gate_enter_shared:PESSGATE: need to wait.\n"));
		            SS_PMON_ADD(SS_PMON_PESSGATE_WAIT);
		            break;
				case SS_SEMNUM_DBE_INDEX_MERGEGATE:
                    ss_pprintf_1(("su_gate_enter_shared:MERGEGATE: need to wait.\n"));
		            SS_PMON_ADD(SS_PMON_MERGEGATE_WAIT);
		            break;
				case SS_SEMNUM_DBE_BTREE_STORAGE_GATE:
                    ss_pprintf_1(("su_gate_enter_shared:STORAGE_GATE: need to wait.\n"));
		            SS_PMON_ADD(SS_PMON_STORAGEGATE_WAIT);
		            break;
				case SS_SEMNUM_DBE_BTREE_BONSAI_GATE:
                    ss_pprintf_1(("su_gate_enter_shared:BONSAI_GATE: need to wait.\n"));
		            SS_PMON_ADD(SS_PMON_BONSAIGATE_WAIT);
		            break;
	            default:
                    ss_pprintf_1(("su_gate_enter_shared:generic: need to wait.\n"));
	                SS_PMON_ADD(SS_PMON_GATE_WAIT);
	                break;
            }
            su_mes_wait(ticket.t_mes);
            SsFlatMutexLock(gate->g_mutex);
            su_meslist_mesdone(&gate->g_meslist, ticket.t_mes);
        } else {    /* Got immediate access ! */
            gate->g_nshared++;
        }
        
        ss_rc_dassert(gate->g_nexclusive == 0, gate->g_sem_num);

        ss_debug({
            if (gate->g_sharedthreads != NULL) {
                ss_dassert(gate->g_maxexclusive > 1 || gate->g_exclusivethread == 0);
                su_list_insertlast(
                        gate->g_sharedthreads,
                        su_gate_shareinfo_init());
            }
        })

        SsFlatMutexUnlock(gate->g_mutex);
        ss_dprintf_1(("su_gate_enter_shared(%s): gate entered.\n",
                      gate->g_name));
}

/*##**********************************************************************\
 * 
 *		su_gate_enter_exclusive
 * 
 * Enters the gated section in exclusive mode
 * 
 * Parameters : 
 * 
 *	gate - in out, use
 *		pointer to gate object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_gate_enter_exclusive(su_gate_t* gate)
{
        su_qticket_t ticket;

        ss_dprintf_1(("su_gate_enter_exclusive(%s)\n",
                      gate->g_name));
        ss_dassert(gate != NULL);

        SsFlatMutexLock(gate->g_mutex);

#ifdef SS_DEBUG
        if (gate->g_sem_num != SS_SEMNUM_NOTUSED) {
            if (gate->g_semnum_checkonlyp) {
                SsSemStkEnterCheck(gate->g_sem_num);
            } else {
                SsSemStkEnter(gate->g_sem_num);
            }
        }
#endif
        ss_profile(if (gate->g_semdbg != NULL) gate->g_semdbg->sd_callcnt++;)

        if (gate->g_nshared != 0 || gate->g_nexclusive >= gate->g_maxexclusive ||
            gate->g_queue_first != NULL)
        {
            /* cannot proceed, go to end of the wait queue */
            ticket.t_shared = FALSE;
            ticket.t_mes = su_meslist_mesinit(&gate->g_meslist);
            SU_GATE_LINKTOQUEUE(gate, &ticket);
            ss_profile(if (gate->g_semdbg != NULL) gate->g_semdbg->sd_waitcnt++;)

            SsFlatMutexUnlock(gate->g_mutex);
            switch (gate->g_sem_num) {
	         	case SS_SEMNUM_DBE_PESSGATE:
                    ss_pprintf_1(("su_gate_enter_exclusive:PESSGATE: need to wait.\n"));
		            SS_PMON_ADD(SS_PMON_PESSGATE_WAIT);
		            break;
				case SS_SEMNUM_DBE_INDEX_MERGEGATE:
                    ss_pprintf_1(("su_gate_enter_exclusive:MERGEGATE: need to wait.\n"));
		            SS_PMON_ADD(SS_PMON_MERGEGATE_WAIT);
		            break;
				case SS_SEMNUM_DBE_BTREE_STORAGE_GATE:
                    ss_pprintf_1(("su_gate_enter_exclusive:STORAGE_GATE: need to wait.\n"));
		            SS_PMON_ADD(SS_PMON_STORAGEGATE_WAIT);
		            break;
				case SS_SEMNUM_DBE_BTREE_BONSAI_GATE:
                    ss_pprintf_1(("su_gate_enter_exclusive:BONSAI_GATE: need to wait.\n"));
		            SS_PMON_ADD(SS_PMON_BONSAIGATE_WAIT);
		            break;
	            default:
                    ss_pprintf_1(("su_gate_enter_exclusive:generic: need to wait.\n"));
	                SS_PMON_ADD(SS_PMON_GATE_WAIT);
	                break;
            }
            su_mes_wait(ticket.t_mes);
            SsFlatMutexLock(gate->g_mutex);
            su_meslist_mesdone(&gate->g_meslist, ticket.t_mes);
        } else {    /* Got immediate access ! */
            gate->g_nexclusive++;
        }
        ss_assert(gate->g_nshared == 0);

        ss_debug({
            if (gate->g_sharedthreads != NULL) {
                ss_dassert(su_list_length(gate->g_sharedthreads) == 0);
                gate->g_exclusivethread = SsThrGetNativeId() + 1;
            }
        })
        
        SsFlatMutexUnlock(gate->g_mutex);
        ss_dprintf_1(("su_gate_enter_exclusive(%s): gate entered, gate->g_nexclusive=%d.\n",
                      gate->g_name, gate->g_nexclusive));
}

/*##**********************************************************************\
 * 
 *		su_gate_exit
 * 
 * Exits the gated section. This function serves both shared and exclusive
 * access modes.
 * 
 * Parameters : 
 * 
 *	gate - in out, use
 *		pointer to gate object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_gate_exit(su_gate_t* gate)
{
        su_qticket_t* p_ticket;

        ss_dprintf_1(("su_gate_exit(%s)\n", gate->g_name));
        ss_dassert(gate != NULL);

        SsFlatMutexLock(gate->g_mutex);
#ifdef SS_DEBUG
        if (gate->g_sem_num != SS_SEMNUM_NOTUSED &&
            !gate->g_semnum_checkonlyp) {
            SsSemStkExit(gate->g_sem_num);
        }
#endif
        if (gate->g_nexclusive != 0) {

            ss_assert(gate->g_nshared == 0);
            gate->g_nexclusive--;

            ss_debug({
                gate->g_exclusivethread = 0;
            })

        } else {

            ss_assert(gate->g_nshared > 0);
            gate->g_nshared--;

            ss_debug({
                if (gate->g_sharedthreads != NULL) {
                    su_list_node_t* n;
                    su_gate_shareinfo_t* si;
                    bool foundp = FALSE;
                    su_list_do_get(gate->g_sharedthreads, n, si) {
                        if (si->si_thrid == SsThrGetNativeId()) {
                            su_list_remove(gate->g_sharedthreads, n);
                            foundp = TRUE;
                            break;
                        }
                    }
                    ss_dassert(foundp);
                }
            })
        }

        p_ticket = gate->g_queue_first;

        if (p_ticket != NULL) {
            /* Queue is not empty. */
            if (!p_ticket->t_shared) {
                /* First in queue is exclusive, send message to gate->g_nexclusive exclusive mode
                   requesters in queue before the first shared request.
                */
                if (gate->g_nshared == 0) {
                    ss_dassert(gate->g_nexclusive < gate->g_maxexclusive);
                    do {
                        gate->g_nexclusive++;
                        ss_rc_dassert(p_ticket->t_mes != NULL, gate->g_sem_num);
                        ss_rc_dassert(gate->g_nwait > 0, gate->g_sem_num);
                        gate->g_nwait--;
                        su_mes_send(p_ticket->t_mes);
                        gate->g_queue_first = p_ticket->t_next;
                        p_ticket->t_next = NULL;
                        p_ticket = gate->g_queue_first;
                    } while (p_ticket != NULL && !p_ticket->t_shared
                             && gate->g_nexclusive < gate->g_maxexclusive);
                }
            } else {

                /* First in queue is shared, send message to all shared mode
                   requesters in queue before the first exclusive request.
                */
                if (gate->g_nexclusive == 0) {
                    do {
                        gate->g_nshared++;
                        ss_rc_dassert(p_ticket->t_mes != NULL, gate->g_sem_num);
                        ss_rc_dassert(gate->g_nwait > 0, gate->g_sem_num);
                        gate->g_nwait--;
                        su_mes_send(p_ticket->t_mes);
                        gate->g_queue_first = p_ticket->t_next;
                        p_ticket->t_next = NULL;
                        p_ticket = gate->g_queue_first;
                    } while (p_ticket != NULL && p_ticket->t_shared);
                }
            }
            if (gate->g_queue_first == NULL) {
                gate->g_queue_last = NULL;
            }
        }
        SsFlatMutexUnlock(gate->g_mutex);
}

/*##**********************************************************************\
 * 
 *		su_gate_ninqueue
 * 
 * Gets number of waiters currently waiting for that gate.
 * the amount is not mutex-protected, so the result can change at any
 * moment.
 * 
 * Parameters : 
 * 
 *	gate - in, use
 *		gate object
 *		
 * Return value : 
 *      wait queue length.
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
uint su_gate_ninqueue(su_gate_t* gate)
{
        return (gate->g_nwait);
}

/*##**********************************************************************\
 * 
 *		su_gate_setmaxexclusive
 * 
 * Sets max number of concurrent exclusive mode accessors to a gate.
 * Default is one so only one exclusive access is allowed.
 * 
 * Parameters : 
 * 
 *	gate - in, use
 *		gate object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_gate_setmaxexclusive(su_gate_t* gate, int maxexclusive)
{
        if (maxexclusive == 0) {
            gate->g_maxexclusive = INT_MAX;
        } else {
            gate->g_maxexclusive = maxexclusive;
        }
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 *
 *      su_gate_thread_is_shared
 *
 * Tells if the gate has been entered by the current thread in shared mode.
 *
 * Parameters:
 *      gate - <usage>
 *          <description>
 *
 * Return value:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
bool su_gate_thread_is_shared(su_gate_t* gate)
{
        su_list_node_t*      n;
        su_gate_shareinfo_t* si;
        bool                 foundp = FALSE;

        if (gate->g_sharedthreads == NULL) {
            return FALSE;
        }

        SsFlatMutexLock(gate->g_mutex);

        su_list_do_get(gate->g_sharedthreads, n, si) {
            if (si->si_thrid == SsThrGetNativeId()) {
                foundp = TRUE;
                break;
            }
        }

        SsFlatMutexUnlock(gate->g_mutex);

        return foundp;
}

/*##**********************************************************************\
 *
 *      su_gate_thread_is_exclusive
 *
 * Tells if the gate has been entered by the current thread in
 * exclusive mode.
 *
 * Parameters:
 *      gate - <usage>
 *          <description>
 *
 * Return value:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
bool su_gate_thread_is_exclusive(su_gate_t* gate)
{
        ss_dassert(gate->g_maxexclusive == 1);

        if (gate->g_sharedthreads == NULL) {
            return FALSE;
        }
        
        return gate->g_exclusivethread == SsThrGetNativeId() + 1;
}

static void g_sharedthreads_listdone(void* data)
{
        su_gate_shareinfo_t* si = data;

        SsMemTrcFreeCallStk(si->si_callstack);
        SsMemFree(si);
}

/*##**********************************************************************\
 *
 *      su_gate_set_thread_tracking
 *
 * Enables or disables tracking of which thread(s) are entered in this gate.
 *
 * Parameters:
 *      gate - <usage>
 *          <description>
 *
 *      flag - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void su_gate_set_thread_tracking(su_gate_t* gate, bool flag)
{
        SsFlatMutexLock(gate->g_mutex);
        
        if (flag && gate->g_sharedthreads == NULL) {
            gate->g_sharedthreads = su_list_init(g_sharedthreads_listdone);
            su_list_startrecycle(gate->g_sharedthreads);
        } else if (!flag && gate->g_sharedthreads != NULL) {
            su_list_done(gate->g_sharedthreads);
            gate->g_sharedthreads = NULL;
        }

        SsFlatMutexUnlock(gate->g_mutex);
}

#endif /* SS_DEBUG */

#endif /* SS_MT */
