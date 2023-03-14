/*************************************************************************\
**  source       * dbe0hsbbuf.c
**  directory    * dbe
**  description  * HSB buffers
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
#include "dbe0hsbbuf.h"

#ifndef SS_NOLOGGING

struct dbe_hsbbuf_st {
#ifdef IO_OPT
        dbe_alogbuf_t*   hb_alogbuf;
#else
        dbe_logbuf_t*   hb_logbuf;
#endif
        size_t          hb_bufsize;
        int             hb_link;
};

#ifdef IO_OPT
dbe_hsbbuf_t* dbe_hsbbuf_init(
        dbe_alogbuf_t* alogbuf,
        size_t bufsize)
{
        dbe_hsbbuf_t* hb;

        SS_PUSHNAME("dbe_hsbbuf_init");
        ss_dassert(alogbuf != NULL);
        ss_dassert(bufsize > 0L);

        hb = SsMemAlloc(sizeof(struct dbe_hsbbuf_st));
        hb->hb_alogbuf = alogbuf;
        hb->hb_bufsize = bufsize;
        hb->hb_link = 1;

        SS_POPNAME;
        return(hb);
}

#else

dbe_hsbbuf_t* dbe_hsbbuf_init(
        dbe_logbuf_t* logbuf,
        size_t bufsize)
{
    dbe_hsbbuf_t* hb;

    SS_PUSHNAME("dbe_hsbbuf_init");
    ss_dassert(logbuf != NULL);
    ss_dassert(bufsize > 0L);

    hb = SsMemAlloc(sizeof(struct dbe_hsbbuf_st));
    hb->hb_logbuf = logbuf;
    hb->hb_bufsize = bufsize;
    hb->hb_link = 1;

    SS_POPNAME;
    return(hb);
}
#endif /* IO_OPT */

void dbe_hsbbuf_done(
        dbe_hsbbuf_t* hb)
{
        bool done = FALSE;

        ss_dassert(hb != NULL);

        SsSemEnter(ss_lib_sem);
        hb->hb_link--;
        if (hb->hb_link == 0) {
            done = TRUE;
        }
        SsSemExit(ss_lib_sem);
        if (done) {
#ifdef IO_OPT
            dbe_alb_done(hb->hb_alogbuf);
#else
            dbe_lb_done(hb->hb_logbuf);
#endif
            SsMemFree(hb);
        }
}

void dbe_hsbbuf_link(
        dbe_hsbbuf_t* hb)
{
        ss_dassert(hb != NULL);
        SsSemEnter(ss_lib_sem);
        hb->hb_link++;
        SsSemExit(ss_lib_sem);
}

#ifdef IO_OPT
dbe_alogbuf_t* dbe_hsbbuf_get_alogbuf(
        dbe_hsbbuf_t* hb)
{
        ss_dassert(hb != NULL);
        return (hb->hb_alogbuf);
}
#else
dbe_logbuf_t* dbe_hsbbuf_get_logbuf(
        dbe_hsbbuf_t* hb)
{
        ss_dassert(hb != NULL);
        return(hb->hb_logbuf);
}
#endif /* IO_OPT */

size_t dbe_hsbbuf_get_bufsize(
        dbe_hsbbuf_t* hb)
{
        ss_dassert(hb != NULL);
        return(hb->hb_bufsize);
}

#endif /* SS_NOLOGGING */
