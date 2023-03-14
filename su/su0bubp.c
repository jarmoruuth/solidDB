/*************************************************************************\
**  source       * su0bubp.c
**  directory    * su
**  description  * Backup buffer pool
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


#include <ssstdio.h>
#include <ssstdlib.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sscacmem.h>

#include "su1check.h"
#include "su0list.h"
#include "su0bubp.h"

struct su_backupbufpool_st {
        int        bubp_check;
        SsSemT*    bubp_mutex;
        size_t     bubp_bufsize;
        size_t     bubp_nbuffers_max;
        size_t     bubp_nbuffers_allocated;
        size_t     bubp_nbuffers_free;
        void     (*bubp_outofbuffers_callbackfun)(void* callbackctx);
        void     (*bubp_bufferfreedsignal_callbackfun)(void* callbackctx);
        void*      bubp_bufferavailability_callbackctx;
        void*    (*bubp_allocfun)(void* ctx, size_t bufsize);
        void     (*bubp_freefun)(void* ctx, void* buf);
        void*      bubp_allocandfreectx;
        bool       bubp_allocandfreectx_cacmem;
        su_list_t* bubp_freebufferslist;
};

#define CHK_BUBP(bubp) ss_bassert(SS_CHKPTR(bubp));\
                       ss_rc_bassert((bubp)->bubp_check==SUCHK_BACKUPBUFPOOL,\
                                     (bubp)->bubp_check)


static void* bubp_defallocfun(
        void* ctx,
        size_t bufsize __attribute__ ((unused)))
{
        void* p = SsCacMemAlloc(ctx);
        return (p);
}

static void bubp_deffreefun(void* ctx, void* buf)
{
        ss_dassert(buf != NULL);
        SsCacMemFree(ctx, buf);
}

/*##**********************************************************************\
 * 
 *		su_backupbufpool_init
 * 
 * Creates a backup buffer pool object
 * 
 * Parameters : 
 * 
 *      bufsize - in
 *          size of each buffer (a power of 2)
 *
 *      nbuffers_max - in
 *          max number of buffers to use
 *      
 *      outofbuffers_callbackfun - in, hold
 *          pointer to callback function to be called if buffer pool
 *          cannot give a buffer immediately. Most probably that callback
 *          function puts the running task to event wait.
 *      
 *      bufferfreedsignal_callbackfun - in, hold
 *          This callback function signals that a free buffer is available.
 *          Probably it signals a tasking system event.
 *      
 *      allocfun - in, hold
 *          memory allocator callback function, can for example give a
 *          dbe cache page. NULL means: let this buffer pool decide
 *          how to obtain memory for buffers. If this parameter is NULL
 *          also the next 2 parameters must be NULL.
 *
 *      freefun - in, hold
 *          memory release function of NULL
 *
 *      allocandfreectx - in, hold
 *          context parameter fot alloc and free
 *
 *      cache - in
 *          tells whether to cache the released buffers or free them
 *          immediately.
 *      
 * Return value - give : 
 *      the created buffer pool object
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_backupbufpool_t* su_backupbufpool_init(
        size_t bufsize,
        size_t nbuffers_max,
        void (*outofbuffers_callbackfun)(void* callbackctx),
        void (*bufferfreedsignal_callbackfun)(void* callbackctx),
        void* (*allocfun)(void* ctx, size_t bufsize),
        void (*freefun)(void* ctx, void* buf),
        void* allocandfreectx,
        bool cache)
{
        su_backupbufpool_t* bubp = SSMEM_NEW(su_backupbufpool_t);

        bubp->bubp_check = SUCHK_BACKUPBUFPOOL;
        bubp->bubp_mutex = SsSemCreateLocal(SS_SEMNUM_SU_BACKUPBUFPOOL);
        bubp->bubp_bufsize = bufsize;
#ifdef SS_DEBUG
        {
            size_t bs = bufsize;
            uint bitcount;

            for (bitcount = 0; bs != 0; bs >>= 1) {
                bitcount += bs & 1U;
            }
            ss_rc_dassert(bitcount == 1, (int)bufsize);
        }
#endif /* SS_DEBUG */
        bubp->bubp_nbuffers_max = nbuffers_max;
        bubp->bubp_nbuffers_allocated = 0;
        bubp->bubp_nbuffers_free = 0;
        bubp->bubp_outofbuffers_callbackfun =
            outofbuffers_callbackfun;
        bubp->bubp_bufferfreedsignal_callbackfun =
            bufferfreedsignal_callbackfun;
        if (allocfun != NULL) {
            ss_dassert(freefun != NULL);
            bubp->bubp_allocfun = allocfun;
            bubp->bubp_freefun = freefun;
            bubp->bubp_allocandfreectx = allocandfreectx;
            bubp->bubp_allocandfreectx_cacmem = FALSE;
        } else {
            ss_dassert(freefun == NULL);
            ss_dassert(allocandfreectx == NULL);
            bubp->bubp_allocfun = bubp_defallocfun;
            bubp->bubp_freefun = bubp_deffreefun;
            bubp->bubp_allocandfreectx = SsCacMemInit(bufsize, nbuffers_max);
            bubp->bubp_allocandfreectx_cacmem = TRUE;
            cache = TRUE;
        }
        if (cache) {
            bubp->bubp_freebufferslist = su_list_init(NULL);
        } else {
            bubp->bubp_freebufferslist = NULL;
        }
        return (bubp);
}

/*##**********************************************************************\
 * 
 *		su_backupbufpool_done
 * 
 * Deletes a backup buffer pool object
 * 
 * Parameters : 
 * 
 *      bubp - in, take
 *          buffer pool object
 *      
 * Return value: 
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_backupbufpool_done(su_backupbufpool_t* bubp)
{
        CHK_BUBP(bubp);
        bubp->bubp_check = SUCHK_FREEDBACKUPBUFPOOL;
        ss_dassert(bubp->bubp_nbuffers_free == bubp->bubp_nbuffers_allocated);
        if (bubp->bubp_freebufferslist != NULL) {
            void* data;
            ss_dassert(bubp->bubp_nbuffers_allocated ==
                       su_list_length(bubp->bubp_freebufferslist));
            while ((data = su_list_removefirst(bubp->bubp_freebufferslist))
                   != NULL)
            {
                (*bubp->bubp_freefun)(bubp->bubp_allocandfreectx, data);
            }
            su_list_done(bubp->bubp_freebufferslist);
        }
        if (bubp->bubp_allocandfreectx_cacmem) {
            SsCacMemDone(bubp->bubp_allocandfreectx);
        }
        SsSemFree(bubp->bubp_mutex);
        SsMemFree(bubp);
}

/*##**********************************************************************\
 * 
 *		su_backupbufpool_getbuf
 * 
 * Gets a buffer from backup buffer pool object
 * 
 * Parameters : 
 *
 *      cd - use
 *         client data context (used as a parameter for
 *         outofbuffers_callbackfun)
 *
 *      bubp - use
 *          buffer pool object
 *      
 * Return value - give:
 *      pointer to buffer or NULL when buffer pool ran out of buffers
 *      (and because of that the outofbuffers_callbackfun has been called)
 *
 * Comments :
 * 
 * Globals used : 
 * 
 * See also : 
 */
void* su_backupbufpool_getbuf(
        void* cd,
        su_backupbufpool_t* bubp)
{
        void* buf;
        
        CHK_BUBP(bubp);
        ss_debug(buf = (void*)0xBabeFaceUL;)
        SsSemEnter(bubp->bubp_mutex);
        if (bubp->bubp_nbuffers_free == 0) {
            if (bubp->bubp_nbuffers_allocated >= bubp->bubp_nbuffers_max) {
                ss_dassert(bubp->bubp_nbuffers_allocated
                           == bubp->bubp_nbuffers_max);
                (*bubp->bubp_outofbuffers_callbackfun)(cd);
                buf = NULL;
            } else {
                buf = (*bubp->bubp_allocfun)(bubp->bubp_allocandfreectx,
                                            bubp->bubp_bufsize);
                ss_dassert(buf != NULL);
                bubp->bubp_nbuffers_allocated++;
            }
        } else {
            ss_dassert(bubp->bubp_freebufferslist != NULL);
            bubp->bubp_nbuffers_free--;
            buf = su_list_removefirst(bubp->bubp_freebufferslist);
        }
        SsSemExit(bubp->bubp_mutex);
        return (buf);
}

/*##**********************************************************************\
 * 
 *		su_backupbufpool_releasebuf
 * 
 * Releases a buffer back to backup buffer pool object
 * and calls bufferfreedsignal_callbackfun
 * 
 * Parameters : 
 *
 *      cd - use
 *         client data context (used as a parameter for
 *         bufferfreedsignal_callbackfun)
 *
 *      bubp - use
 *          buffer pool object
 *
 *      buf - in, take
 *          pointer to buffer
 *
 * Return value:
 *
 * Comments :
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_backupbufpool_releasebuf(
        void* cd,
        su_backupbufpool_t* bubp,
        void* buf)
{
        CHK_BUBP(bubp);
        SsSemEnter(bubp->bubp_mutex);
        if (bubp->bubp_freebufferslist != NULL) {
            su_list_insertfirst(bubp->bubp_freebufferslist,
                                buf);
            bubp->bubp_nbuffers_free++;
        } else {
            ss_rc_dassert(bubp->bubp_nbuffers_free == 0,
                          bubp->bubp_nbuffers_free);
            (*bubp->bubp_freefun)(bubp->bubp_allocandfreectx,
                                  buf);
        }
        SsSemExit(bubp->bubp_mutex);
        /* Note this is called AFTER exiting mutexed section to
         * avoid unnecessary double wakeups
         */
        (*bubp->bubp_bufferfreedsignal_callbackfun)(cd);
}

/*##**********************************************************************\
 * 
 *		su_backupbufpool_getbufsize
 * 
 * gets buffer size of this pool.
 * 
 * Parameters : 
 *
 *      bubp - in, use
 *          buffer pool object
 *
 * Return value:
 *      buffer size used.
 *
 * Comments :
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t su_backupbufpool_getbufsize(su_backupbufpool_t* bubp)
{
        CHK_BUBP(bubp);
        return (bubp->bubp_bufsize);
}
