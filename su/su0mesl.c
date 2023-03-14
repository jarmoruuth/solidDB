/*************************************************************************\
**  source       * su0mesl.c
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

The methods are multithread-safe because the functions that
modify the list are mutexed implicitly.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssdebug.h>
#include <ssmem.h>
#include "su0mesl.h"

#define MESLIST_SEMENTER(mutex) \
        if ((mutex) != NULL) {\
            SsQsemEnter(mutex);\
        }

#define MESLIST_SEMEXIT(mutex) \
        if ((mutex) != NULL) {\
            SsQsemExit(mutex);\
        }

/*##**********************************************************************\
 * 
 *		su_meslist_mesinit
 * 
 * Creates or reuses a message object
 * 
 * Parameters : 
 * 
 *	meslist - use
 *		list of free messages
 *		
 * Return value - give:
 *      mesage object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_mes_t* su_meslist_mesinit(su_meslist_t* meslist)
{
        su_mes_t* mes;

        MESLIST_SEMENTER(meslist->ml_mutex);
        if (meslist->ml_list == NULL) {
            MESLIST_SEMEXIT(meslist->ml_mutex);
            mes = SSMEM_NEW(su_mes_t);
            mes->m_mes = SsMesCreateLocal();
            /* mes->m_next intentionally left uninitialized */
        } else {
            mes = meslist->ml_list;
            meslist->ml_list = mes->m_next;
            MESLIST_SEMEXIT(meslist->ml_mutex);
        }
        return (mes);
}

/*##**********************************************************************\
 * 
 *		su_meslist_mesdone
 * 
 * Deletes (for reuse) a message object
 * 
 * Parameters : 
 * 
 *	meslist - use
 *		free message list
 *		
 *	mes - in, take
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
void su_meslist_mesdone(su_meslist_t* meslist, su_mes_t* mes)
{
        MESLIST_SEMENTER(meslist->ml_mutex);
        mes->m_next = meslist->ml_list;
        meslist->ml_list = mes;
        SsMesReset(mes->m_mes);
        MESLIST_SEMEXIT(meslist->ml_mutex);
}

/*#***********************************************************************\
 * 
 *		meslist_init
 * 
 * Creates a meslist
 * 
 * Parameters : 
 * 
 *	meslist_buf - in, take
 *          NULL for dynamic allocation, otherwise
 *          buffer of type su_msglist_t
 *		
 *	mutex - in, take
 *		NULL for no implicit mutex-protection or
 *          a local mutex semaphore handle
 *		
 * Return value - give: 
 *      new meslist
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static su_meslist_t* meslist_init(su_meslist_t* meslist_buf, SsQsemT* mutex)
{
        if (meslist_buf == NULL) {
            meslist_buf = SSMEM_NEW(su_meslist_t);
            meslist_buf->ml_dynalloc = TRUE;
        } else {
            meslist_buf->ml_dynalloc = FALSE;
        }
        meslist_buf->ml_mutex = mutex;
        meslist_buf->ml_list = NULL;
        return (meslist_buf);
}
/*##**********************************************************************\
 * 
 *		su_meslist_init
 * 
 * Creates a new message list
 * 
 * Parameters : 
 * 
 *	meslist_buf - in, (take)
 *		pointer to enough space to hold the buffer or
 *          NULL to request dynamic allocation
 *		
 * Return value - give: 
 *      pointer to meslist object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_meslist_t* su_meslist_init(su_meslist_t* meslist_buf)
{
        su_meslist_t* meslist;
        SsQsemT* mutex;

        mutex = SsQsemCreateLocal(SS_SEMNUM_SU_MESLIST);
        meslist = meslist_init(meslist_buf, mutex);
        return (meslist);
}

/*##**********************************************************************\
 * 
 *		su_meslist_init_nomutex
 * 
 * Same as above but this is not mutex-protected
 * 
 * Parameters : 
 * 
 *	meslist_buf - 
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
su_meslist_t* su_meslist_init_nomutex(su_meslist_t* meslist_buf)
{
        su_meslist_t* meslist;

        meslist = meslist_init(meslist_buf, NULL);
        return (meslist);
}

/*##**********************************************************************\
 * 
 *		su_meslist_done
 * 
 * Deletes a message list and frees the buffer if it is dynamically
 * allocated.
 * 
 * Parameters : 
 * 
 *	meslist - in, (take)
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
void su_meslist_done(su_meslist_t* meslist)
{
        su_mes_t* mes;

        MESLIST_SEMENTER(meslist->ml_mutex);
        while (meslist->ml_list != NULL) {
            mes = meslist->ml_list;
            meslist->ml_list = mes->m_next;
            SsMesFree(mes->m_mes);
            SsMemFree(mes);
        }
        MESLIST_SEMEXIT(meslist->ml_mutex);
        if (meslist->ml_mutex != NULL) {
            SsQsemFree(meslist->ml_mutex);
        }
        if (meslist->ml_dynalloc) {
            SsMemFree(meslist);
        }
}

uint su_meswaitlist_wakeupallfun(su_meswaitlist_t** p_mwlist)
{
        uint nwakeups = 0;
        su_meswaitlist_t* next;
        su_meswaitlist_t* mwlist = *p_mwlist;
        
        while (mwlist != NULL) {
            next = mwlist->m_next;
            su_mes_send(mwlist);
            mwlist = next;
            nwakeups++;
        }
        *p_mwlist = mwlist;
        return (nwakeups);
}
