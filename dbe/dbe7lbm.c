/*************************************************************************\
**  source       * dbe7lbm.c
**  directory    * dbe
**  description  * log-buffer manager for HSB
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

None.

Error handling:
--------------

Asserts.


Objects used:
------------

None.


Preconditions:
-------------

None.

Multithread considerations:
--------------------------

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <stdio.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include "dbe0lb.h"
#include "dbe7lbm.h"

/* Structure for logbuffer manager.
*/

struct dbe_lbm_st {
        dbe_hsbbuf_t*   currentbuffer;
        size_t          currentbufsize;
};

/*##**********************************************************************\
 *
 *		dbe_lbm_init
 *
 * Creates buffer manager instance
 *
 * Parameters :
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
dbe_lbm_t* dbe_lbm_init(
        void)
{
        dbe_lbm_t* lbm;

        ss_dprintf_1(("dbe_lbm_init"));
        SS_PUSHNAME("dbe_lbm_init");
        lbm = SsMemAlloc(sizeof(dbe_lbm_t));

        lbm->currentbuffer = NULL;
        lbm->currentbufsize = 0L;

        SS_POPNAME;
        return(lbm);
}

/*##**********************************************************************\
 *
 *		dbe_lbm_done
 *
 *
 * Parameters :
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
void dbe_lbm_done(
        dbe_lbm_t* lbm)
{
        ss_dprintf_1(("dbe_lbm_done"));
        SS_PUSHNAME("dbe_lbm_done");

        ss_pprintf_1(("dbe_lbm_done\n"));
        SsMemFree(lbm);

        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *		dbe_lbm_getnext_hsbbuffer
 *
 * Creates next buffer. If previous buffer is given it is released
 * Note that dbe_hsbbuf_t has link count and mutex for concurrent
 * link/done.
 *
 * Parameters :
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
dbe_hsbbuf_t* dbe_lbm_getnext_hsbbuffer(
        dbe_lbm_t* lbm,
        dbe_hsbbuf_t* prevbuffer,
        size_t bufsize)
{
#ifdef IO_OPT
        dbe_alogbuf_t*  alogbuf;
#else
        dbe_logbuf_t* logbuf;
#endif
        dbe_hsbbuf_t* hsbbuf;
        ss_dprintf_1(("dbe_lbm_getnextbuffer"));
        SS_PUSHNAME("dbe_lbm_getnextbuffer");

        if (prevbuffer == NULL) {
#ifdef IO_OPT
            alogbuf = dbe_alb_init(bufsize);
            ss_dassert(DBE_LB_ALIGNMENT((alogbuf->alb_buffer), 
                       SS_DIRECTIO_ALIGNMENT));

            hsbbuf = dbe_hsbbuf_init(alogbuf, bufsize);
#else
            logbuf = dbe_lb_init(bufsize);
            hsbbuf = dbe_hsbbuf_init(logbuf, bufsize);
#endif
        } else {
#ifdef IO_OPT
            dbe_alogbuf_t   alb;
            dbe_alogbuf_t*  prev_alogbuf;
            prev_alogbuf = dbe_hsbbuf_get_alogbuf(prevbuffer);      
            ss_dassert(DBE_LB_ALIGNMENT((prev_alogbuf->alb_buffer), 
                       SS_DIRECTIO_ALIGNMENT));
            ss_dassert(lbm->currentbuffer == prevbuffer);
            ss_dassert(lbm->currentbufsize == bufsize);
            alogbuf = dbe_alb_init(bufsize);
            ss_dassert(DBE_LB_ALIGNMENT((alogbuf->alb_buffer), 
                       SS_DIRECTIO_ALIGNMENT));

            alogbuf->alb_buffer->lb_.chk = prev_alogbuf->alb_buffer->lb_.chk;
            alb.alb_buffer = LB_CHK2ADDR(alogbuf->alb_buffer, bufsize);
            LB_CP_CHK(alb.alb_buffer, alogbuf->alb_buffer);

            hsbbuf = dbe_hsbbuf_init(alogbuf, bufsize);
            dbe_hsbbuf_done(prevbuffer);
#else
            dbe_logbuf_t* lb;
            dbe_logbuf_t* prev_logbuf;
            prev_logbuf = dbe_hsbbuf_get_logbuf(prevbuffer);
            ss_dassert(lbm->currentbuffer == prevbuffer);
            ss_dassert(lbm->currentbufsize == bufsize);
            logbuf = dbe_lb_init(bufsize);
            logbuf->lb_.chk = prev_logbuf->lb_.chk;
            lb = LB_CHK2ADDR(logbuf, bufsize);
            LB_CP_CHK(lb, logbuf);

            hsbbuf = dbe_hsbbuf_init(logbuf, bufsize);
            dbe_hsbbuf_done(prevbuffer);
#endif
        }
        lbm->currentbufsize = bufsize;
        lbm->currentbuffer = hsbbuf;
        SS_POPNAME;
        return(hsbbuf);
}
