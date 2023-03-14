/*************************************************************************\
**  source       * dbe0lb.h
**  directory    * dbe
**  description  * Log buffer interface
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

#ifndef DBE0LB_H
#define DBE0LB_H

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

/* Log file disk buffer structure */
typedef struct {
        union {
            ss_byte_t  ctr[2];
            ss_uint2_t chk;
        }               lb_;        /* check fields */
        uchar           lb_data[1]; /* data is actually bigger than 1 */
} dbe_logbuf_t;

#ifdef IO_OPT
typedef struct dbe_aligned_buf_st {
        void*   ab_buf;
        void*   ab_base;
} dbe_aligned_buf_t;


/* Aligned log buffer object */
typedef struct dbe_alogbuf_st {
        dbe_logbuf_t*   alb_buffer;     /* file write buffer */
        void*           alb_baseaddr;   /* The base address for aligned buffer.
                                         * same as lf_buffer if not aligned.
                                         * Memory
                                         * should be freed by using this. */
} dbe_alogbuf_t;
#define DBE_LB_ALIGNMENT(addr,boundary) \
        ( ((ss_ptr_as_scalar_t)(addr))%((ss_ptr_as_scalar_t)(boundary)) == 0)

#endif /* IO_OPT */


#ifndef SS_NOLOGGING

#define LB_BNUM_IDX 0
#define LB_VNUM_IDX 1
#define LB_CHK2ADDR(lb, bufsize) \
        ((dbe_logbuf_t*)((uchar*)(lb) + bufsize - sizeof((lb)->lb_.chk)))

#define DBE_LB_DATA(lb) \
((ss_byte_t*)(lb) + sizeof(ss_uint2_t))

#define LB_CP_CHK(lb1, lb2) \
{ \
        (lb1)->lb_.ctr[LB_BNUM_IDX] = (lb2)->lb_.ctr[LB_BNUM_IDX]; \
        (lb1)->lb_.ctr[LB_VNUM_IDX] = (lb2)->lb_.ctr[LB_VNUM_IDX]; \
}

#define LOGFILE_CHECKSHUTDOWN(logfile) \
{\
        if ((logfile)->lf_errorflag) {\
            return (SU_ERR_FILE_WRITE_FAILURE);\
        }\
}

#ifdef IO_OPT
dbe_aligned_buf_t*      dbe_ab_init(size_t bufsize, size_t boundary);
void                    dbe_ab_done(dbe_aligned_buf_t* ab);
dbe_alogbuf_t*          dbe_alb_init(size_t bufsize);
dbe_logbuf_t*           dbe_get_logbuf(dbe_alogbuf_t* alb);
void*                   dbe_get_alb_baseaddr(dbe_alogbuf_t* alb);
void                    dbe_set_alb_logbuf(dbe_alogbuf_t* alb,
                                           dbe_logbuf_t* lb);
void                    dbe_set_alb_baseaddr(dbe_alogbuf_t* alb,
                                             void* addr);
void                    dbe_alb_done(dbe_alogbuf_t* alb);
#endif

void dbe_lb_initbuf(dbe_logbuf_t* logbuf, size_t bufsize);

dbe_logbuf_t* dbe_lb_init(size_t bufsize);

void dbe_lb_done(dbe_logbuf_t* logbuf);

bool dbe_lb_isconsistent(dbe_logbuf_t* logbuf, size_t bufsize);

/*#***********************************************************************\
 *
 *		dbe_lb_incversion
 *
 * Increments version # of logbuf
 *
 * Parameters :
 *
 *	logbuf - in out, use
 *		pointer to logbuf
 *
 *	bufsize - in
 *		buffer size in bytes
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define dbe_lb_incversion(logbuf, bufsize) \
do {\
        dbe_logbuf_t *lb;\
        (logbuf)->lb_.ctr[LB_VNUM_IDX]++;\
        lb = LB_CHK2ADDR(logbuf, bufsize);\
        LB_CP_CHK(lb, logbuf);\
} while (0)

/*#***********************************************************************\
 *
 *		dbe_lb_incblock
 *
 * Increments the block counter of a logbuf (which identifies whether
 * adjacent blocks are versions of same logical block or not).
 *
 * Parameters :
 *
 *	logbuf - in out, use
 *		pointer to logbuf
 *
 *	bufsize - in
 *		buf size in bytes
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define dbe_lb_incblock(logbuf, bufsize) \
do {\
        dbe_logbuf_t *lb;\
        (logbuf)->lb_.ctr[LB_BNUM_IDX]++;\
        lb = LB_CHK2ADDR(logbuf, bufsize);\
        LB_CP_CHK(lb, logbuf);\
} while (0)

/*#***********************************************************************\
 *
 *		dbe_lb_versioncmp
 *
 * Compares version numbers of two logbufs
 *
 * Parameters :
 *
 *	logbuf1 - in, use
 *		pointer to 1st logbuf
 *
 *	logbuf2 - in, use
 *		pointer to 2nd logbuf
 *
 * Return value :
 *      subtraction result of the version counters of 1st and 2nd
 *
 * Limitations  :
 *
 * Globals used :
 */
#define dbe_lb_versioncmp(logbuf1, logbuf2) \
(\
        (int)(signed char)\
            ((logbuf1)->lb_.ctr[LB_VNUM_IDX] - (logbuf2)->lb_.ctr[LB_VNUM_IDX])\
)

/*#***********************************************************************\
 *
 *		dbe_lb_sameblocknumber
 *
 * Checks whether two blocks are versions of same logical log block
 *
 * Parameters :
 *
 *	logbuf1 - in, use
 *		pointer to 1st logbuf
 *
 *	logbuf2 - in, use
 *		pointer to 2nd logbuf
 *
 * Return value :
 *      TRUE when blocknumber is same or
 *      FALSE when different
 *
 * Limitations  :
 *
 * Globals used :
 */
#define dbe_lb_sameblocknumber(logbuf1, logbuf2) \
(\
        (logbuf1)->lb_.ctr[LB_BNUM_IDX] == (logbuf2)->lb_.ctr[LB_BNUM_IDX]\
)

/*#***********************************************************************\
 *
 *		dbe_lb_sameblock
 *
 * Compares whether two log blocks are exactly same (same version &
 * same block #)
 *
 * Parameters :
 *
 *	logbuf1 - in, use
 *		pointer to 1st logbuf
 *
 *	logbuf2 - in, use
 *		pointer to 2nd logbuf
 *
 * Return value :
 *      TRUE when similar or
 *      FALSE when not
 *
 * Limitations  :
 *
 * Globals used :
 */
#define dbe_lb_sameblock(logbuf1, logbuf2) \
(\
        (logbuf1)->lb_.chk == (logbuf2)->lb_.chk\
)

/*#***********************************************************************\
 *
 *		dbe_lb_getblocknumber
 *
 * Gets block sequence counter of a log buffer
 *
 * Parameters :
 *
 *	lb - in, use
 *		pointer to logbuf
 *
 * Return value :
 *      sequence counter value
 *
 * Limitations  :
 *
 * Globals used :
 */
#define dbe_lb_getblocknumber(lb)   ((lb)->lb_.ctr[LB_BNUM_IDX])

#endif /* SS_NOLOGGING */

#endif /* DBE0LB_H */

