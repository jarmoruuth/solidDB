/*************************************************************************\
**  source       * sscheck.h
**  directory    * ss
**  description  * Check enum for ss.
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


#ifndef SSCHECK_H
#define SSCHECK_H

#include "ssdebug.h"

typedef enum {
        SS_CHK_NONE = SS_CHKBASE_SS,
        SS_CHK_SEM,
        SS_CHK_TIMER,
        SS_CHK_TIMERREQ,
        SS_CHK_FFMEMCTX,
        SS_CHK_MSGCACHE,
        SS_CHK_MEMLIMIT,
        SS_CHK_FFMEMPRIVCTX,
        SS_CHK_FFMEMPOOL,
        
        SS_CHK_TIMER_FREED = SS_CHKBASE_SS + SS_CHKBASE_FREED_INCR,
        SS_CHK_TIMERREQ_FREED,
        SS_CHK_MEMLIMIT_FREED
} ss_chk_t;

#endif /* SSCHECK_H */
