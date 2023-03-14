/*************************************************************************\
**  source       * dbe0lb.c
**  directory    * dbe
**  description  * Log buffers
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

#include <ssc.h>
#include <sssem.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssstring.h>
#include <sssprint.h>
#include <sslimits.h>
#include <ssfile.h>
#include <ssltoa.h>
#include <sspmon.h>

#include "dbe0lb.h"

#ifndef SS_NOLOGGING


#ifdef IO_OPT
/*#***********************************************************************\
 *
 *      dbe_ab_init
 *
 * Allocates memory and stores a pointer to address boundary according to
 *  a calling parameter.
 *
 * Parameters :
 *
 *  bufsize - in
 *      buffer size in bytes (multiple of cluster size)
 *
 *  boundary - in
 *      alignment
 *
 * Return value - give:
 *      pointer to created object
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_aligned_buf_t* dbe_ab_init(
        size_t  bufsize,
        size_t  boundary)
{
        dbe_aligned_buf_t*  ab;
        ss_uint4_t          offset;

        ab = SsMemAlloc (sizeof(dbe_aligned_buf_t) );
        ab->ab_base = SsMemAlloc(bufsize + boundary);
        offset = boundary -(((ss_ptr_as_scalar_t) ab->ab_base) % boundary);
        ab->ab_buf = (void *)((ss_ptr_as_scalar_t) ab->ab_base + offset);
        ss_dassert(((ss_ptr_as_scalar_t)ab->ab_buf
                - (ss_ptr_as_scalar_t)ab->ab_base) < boundary);
        ss_dassert( ((ss_ptr_as_scalar_t) ab->ab_base + offset)
                % boundary == 0);
        ss_dassert(DBE_LB_ALIGNMENT((ab->ab_buf), boundary));
        return (ab);
}


void dbe_ab_done(dbe_aligned_buf_t* ab)
{
        ss_dassert(ab != NULL);
        ss_dassert(ab->ab_base != NULL);
        ss_pprintf_1( ("dbe_ab_done:aligned buffer %x\n", ab) );
        SsMemFree(ab->ab_base);
        SsMemFree(ab);
}


/*#***********************************************************************\
 *
 *      dbe_alb_init
 *
 * Creates an aligned logbuf object
 *
 * Parameters :
 *
 *  bufsize - in
 *      buffer size in bytes (multiple of cluster size)
 *
 * Return value - give:
 *      pointer to created object
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_alogbuf_t* dbe_alb_init(
        size_t    bufsize)
{
        dbe_alogbuf_t*   alb;

        ss_dprintf_2(("dbe_alb_init : bufsize %d, boundary %d\n",
                      bufsize,
                      SS_DIRECTIO_ALIGNMENT));
        alb = (dbe_alogbuf_t *) dbe_ab_init(bufsize, SS_DIRECTIO_ALIGNMENT);
        dbe_lb_initbuf(alb->alb_buffer, bufsize);
        ss_dprintf_2(("dbe_alb_init : alb->alb_buffer %lu, mod %d : %d\n",
                      SS_DIRECTIO_ALIGNMENT,
                      (ss_byte_t *)alb->alb_buffer,
                      (ss_ptr_as_scalar_t)alb->alb_buffer
                              % SS_DIRECTIO_ALIGNMENT));

        ss_pprintf_1( ("dbe_alb_init:logbuf %x\n", alb) );
        return (alb);
}


dbe_logbuf_t* dbe_get_logbuf(dbe_alogbuf_t* alb)
{
        ss_dassert(alb != NULL);
        ss_dassert(alb->alb_buffer != NULL);
        return (alb->alb_buffer);
}

void* dbe_get_alb_baseaddr(dbe_alogbuf_t* alb)
{
        ss_dassert(alb != NULL);
        return (alb->alb_baseaddr);
}

void dbe_set_alb_logbuf(dbe_alogbuf_t* alb, dbe_logbuf_t* lb)
{
        ss_dassert(alb != NULL);
        ss_dassert(lb != NULL);
        alb->alb_buffer = lb;
}

void dbe_set_alb_baseaddr(dbe_alogbuf_t* alb, void* addr)
{
        ss_dassert(alb != NULL);
        ss_dassert(addr != NULL);
        alb->alb_baseaddr = addr;
}

/*#***********************************************************************\
 *
 *      dbe_alb_done
 *
 * Deletes an aligned logbuf object
 *
 * Parameters :
 *
 *  alb - in, take
 *      pointer to aligned logbuf
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_alb_done(dbe_alogbuf_t* alb)
{
        ss_dassert(alb != NULL);
        ss_dassert(alb->alb_baseaddr != NULL);
        ss_pprintf_1( ("dbe_alb_done:aligned logbuf %x\n", alb) );

        dbe_ab_done( (dbe_aligned_buf_t *) alb);
}

/*#***********************************************************************\
 *
 *      dbe_alb_isconsistent
 *
 * Checks whether a log file block is completely written to disk.
 *
 * Parameters :
 *
 *  alb - in, use
 *      pointer to aligned logbuf object
 *
 *  bufsize - in
 *      buf size in bytes
 *
 * Return value :
 *      TRUE when ok or
 *      FALSE when block is corrupt
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_alb_isconsistent(dbe_alogbuf_t* alb, size_t bufsize)
{
        dbe_logbuf_t* lb;
        ss_dassert(alb != NULL);
        lb = LB_CHK2ADDR(alb->alb_buffer, bufsize);

        return (lb->lb_.ctr[LB_BNUM_IDX]
                == alb->alb_buffer->lb_.ctr[LB_BNUM_IDX]
                && lb->lb_.ctr[LB_VNUM_IDX]
                == alb->alb_buffer->lb_.ctr[LB_VNUM_IDX]);
}

#endif /* IO_OPT */




/*#***********************************************************************\
 *
 *		dbe_lb_init
 *
 * Creates logbuf object
 *
 * Parameters :
 *
 *	bufsize - in
 *		buffer size in bytes (multiple of cluster size)
 *
 * Return value - give:
 *      pointer to created object
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_logbuf_t* dbe_lb_init(size_t bufsize)
{
        dbe_logbuf_t*   logbuf;
        logbuf = SsMemAlloc(bufsize);
        dbe_lb_initbuf(logbuf, bufsize);
        ss_pprintf_1(("dbe_lb_init:logbuf %x\n", logbuf));
        return (logbuf);
}


/*#***********************************************************************\
 *
 *		dbe_lb_done
 *
 * Deletes a logbuf object
 *
 * Parameters :
 *
 *	logbuf - in, take
 *		pointer to logbuf
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_lb_done(dbe_logbuf_t* logbuf)
{
        ss_pprintf_1(("dbe_lb_done:logbuf %x\n", logbuf));
        SsMemFree(logbuf);
}


/*#***********************************************************************\
 *
 *      dbe_lb_isconsistent
 *
 * Checks whether a log file block is completely written to disk.
 *
 * Parameters :
 *
 *  logbuf - in, use
 *      pointer to logbuf
 *
 *  bufsize - in
 *      buf size in bytes
 *
 * Return value :
 *      TRUE when ok or
 *      FALSE when block is corrupt
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_lb_isconsistent(dbe_logbuf_t* logbuf, size_t bufsize)
{
        dbe_logbuf_t* lb;
        lb = LB_CHK2ADDR(logbuf, bufsize);

        return (lb->lb_.ctr[LB_BNUM_IDX] == logbuf->lb_.ctr[LB_BNUM_IDX]
             && lb->lb_.ctr[LB_VNUM_IDX] == logbuf->lb_.ctr[LB_VNUM_IDX]);
}

void dbe_lb_initbuf(dbe_logbuf_t* logbuf, size_t bufsize)
{
    dbe_logbuf_t* lb;
    logbuf->lb_.ctr[LB_VNUM_IDX] = (ss_byte_t)1;
    logbuf->lb_.ctr[LB_BNUM_IDX] = (ss_byte_t)1;
    lb = LB_CHK2ADDR(logbuf, bufsize);
    LB_CP_CHK(lb, logbuf);
}
#endif /* SS_NOLOGGING */
