/*************************************************************************\
**  source       * su0mesl.h
**  directory    * su
**  description  * Message list for cacheing of message semaphores
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


#ifndef SU0MESL_H
#define SU0MESL_H

#include <ssc.h>
#include <sssem.h>

/* These data structures should only be used through the functions
** and macros
*/
typedef struct su_mes_st su_mes_t;
struct su_mes_st {
        su_mes_t*  m_next;
        SsMesT*    m_mes;
};

typedef struct su_meslist_st su_meslist_t;
struct su_meslist_st {
        SsQsemT*    ml_mutex;
        su_mes_t*   ml_list;
        bool        ml_dynalloc;
};

su_mes_t* su_meslist_mesinit(su_meslist_t* meslist);
void su_meslist_mesdone(su_meslist_t* meslist, su_mes_t* mes);
su_meslist_t* su_meslist_init(su_meslist_t* meslist_buf);
su_meslist_t* su_meslist_init_nomutex(su_meslist_t* meslist_buf);
void su_meslist_done(su_meslist_t* meslist);

/*##**********************************************************************\
 * 
 *		su_mes_wait
 * 
 * Waits for message to be sent
 * 
 * Parameters : 
 * 
 *	mes - use
 *		message object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define su_mes_wait(mes) \
        SsMesWait((mes)->m_mes)

/*##**********************************************************************\
 * 
 *		su_mes_send
 * 
 * Send a message
 * 
 * Parameters : 
 * 
 *	mes - use
 *		message object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define su_mes_send(mes) \
        SsMesSend((mes)->m_mes)

/*##**********************************************************************\
 * 
 *		su_mes_getssmes
 * 
 * Returns SsMesT object.
 * 
 * Parameters : 
 * 
 *	mes - use
 *		message object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define su_mes_getssmes(mes) \
        ((mes)->m_mes)

/* meswaitlist is for registering
 * waiting threads to wait queue, so that all
 * those threads can be waken up by one thread
 * without waits for the waken thread to confirm the
 * receipt of the message.
 * Note: SsMesSend() is only guaranteed to wake up
 * at least one of the threads waiting for it!
 * It does not necessarily work as a broadcast
 * for all waiting threds.
 */
typedef su_mes_t su_meswaitlist_t;

#define su_meswaitlist_init() (NULL)
#define su_meswaitlist_done(mwlist) su_meswaitlist_wakeupall(mwlist)

uint su_meswaitlist_wakeupallfun(su_meswaitlist_t** p_mwlist);

#define su_meswaitlist_add(mwlist, mes)\
{\
        (mes)->m_next = (mwlist);\
        (mwlist) = (mes);\
}

#define su_meswaitlist_wakeup1st(mwlist) \
{\
        su_meswaitlist_t* next;\
        if ((mwlist) != NULL) {\
            next = ((mwlist)->m_next);\
            su_mes_send(mwlist);\
            (mwlist) = next;\
        }\
}

#define su_meswaitlist_wakeupall(mwlist) \
        su_meswaitlist_wakeupallfun(&mwlist)

#define su_meswaitlist_isempty(mwlist) \
        ((mwlist) == NULL)

#endif /* SU0MESL_H */
