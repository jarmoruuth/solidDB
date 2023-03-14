/*************************************************************************\
**  source       * dbe6lmrg.h
**  directory    * dbe
**  description  * Lock manager.
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


#ifndef DBE6LMRG_H
#define DBE6LMRG_H

#include <ssenv.h>

#include <ssc.h>

#include <sspmon.h>

#include <uti0vtpl.h>

#include <rs0sysi.h>

#include "dbe0type.h"

#define DBE_LOCKRELID_REL           0L
#define DBE_LOCKRELID_SEQ           1L
#define DBE_LOCKRELID_MMI           2L
#define DBE_LOCKRELID_MAXSPECIAL    10L

dbe_lockmgr_t* dbe_lockmgr_init(
        uint        hashsize,
        ulong       escalatelimit,
        SsSemT*     sem);

void dbe_lockmgr_done(
        dbe_lockmgr_t* lm);

void dbe_lockmgr_setuselocks(
        dbe_lockmgr_t* lm,
        bool uselocks);

dbe_lock_reply_t dbe_lockmgr_lock(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran,
        ulong relid,
        dbe_lockname_t lockname,
        dbe_lock_mode_t mode,
        long timeout,
        bool bouncep,
        bool* p_newlock);

dbe_lock_reply_t dbe_lockmgr_lock_long(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran,
        ulong relid,
        dbe_lockname_t lockname,
        dbe_lock_mode_t mode,
        long timeout,
        bool bouncep);

dbe_lock_reply_t dbe_lockmgr_lock_mme(
        dbe_lockmgr_t*      lm,
        dbe_locktran_t*     locktran,
        ulong*              relid,
        dbe_lockname_t*     lockname,
        dbe_lock_mode_t     mode,
        long                timeout,
        bool                bouncep,
        bool                escalatep);

dbe_lock_reply_t dbe_lockmgr_relock_mme(
        dbe_lockmgr_t*      lm,
        dbe_locktran_t*     locktran,
        ulong               relid,
        dbe_lockname_t      lockname,
        dbe_lock_mode_t     mode,
        long                timeout);

void dbe_lockmgr_unlockall(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran);

void dbe_lockmgr_unlockall_nomutex(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran);

void dbe_lockmgr_unlockall_mme(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran);

void dbe_lockmgr_unlockall_long(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* locktran);

void dbe_lockmgr_unlock(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name);

void dbe_lockmgr_unlock_shared(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name);

void dbe_lockmgr_cancelwaiting(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me);

void dbe_lockmgr_unlock_shared(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name);

bool dbe_lockmgr_getlock(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name,
        dbe_lock_mode_t* p_mode,
        bool* p_isverylong_duration);

dbe_lock_reply_t dbe_lockmgr_lock_convert(
        dbe_lockmgr_t* lm,
        dbe_locktran_t* me,
        ulong relid,
        dbe_lockname_t name,
        dbe_lock_mode_t mode,
        bool verylong_duration);

dbe_locktran_t* dbe_locktran_init(
        rs_sysi_t* cd);

void dbe_locktran_done(
        dbe_locktran_t* locktran);

void dbe_lockmgr_printinfo(
        void* fp,
        dbe_lockmgr_t* lm);

ulong dbe_lockmgr_getlockcount(
        dbe_lockmgr_t* lm);

void dbe_lockmgr_setlockdisablerowspermessage(
        dbe_lockmgr_t* lm,
        bool lockdisablerowspermessage);

void dbe_lockmgr_setuseescalation(
        dbe_lockmgr_t*  lm,
        bool            useescalation);

#endif /* DBE6LMRG_H */
