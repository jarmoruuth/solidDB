/*************************************************************************\
**  source       * sstimer.h
**  directory    * ss
**  description  * Timer routines.
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


#ifndef SSTIMER_H
#define SSTIMER_H

#include "sstime.h"

typedef ss_uint4_t SsTimerRequestIdT;

#define SS_TIMER_REQUEST_ID_NULL ((SsTimerRequestIdT)0)

typedef void (*timeout_callbackfun_t)(void*, SsTimerRequestIdT);

extern SsTimeT ss_timer_curtime_sec;

void SsTimerGlobalInit(void);

void SsTimerGlobalDone(void);

SsTimerRequestIdT SsTimerAddRequest(
        long timeout_ms,
        timeout_callbackfun_t callbackfun,
        void* callbackctx);

SsTimerRequestIdT SsTimerAddRequestWithFreefunc(
        long timeout_ms,
        timeout_callbackfun_t callbackfun,
        void* callbackctx,
        void (*freefunc)(void* ctx));

void SsTimerAddPersistentRequest(
        long timeout_ms,
        timeout_callbackfun_t callbackfun,
        void* callbackctx);

long SsTimerCancelRequestGetCtx(
        SsTimerRequestIdT tr,
        void** p_ctx);

long SsTimerCancelRequest(
        SsTimerRequestIdT tr);

bool SsTimerRequestIsValid(
        SsTimerRequestIdT tr);

long SsTimerNextTimeout(void);

#ifndef SS_MT

/* advance timer thread; returns time in ms to next timeout */
long SsTimerAdvance(void);

#endif /* !SS_MT */

#endif /* SSTIMER_H */
