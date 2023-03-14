/*************************************************************************\
**  source       * dbe4mmeg2.h
**  directory    * dbe
**  description  * Redirector for MMEG2 interface.
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


#ifndef DBE4MMEG2_H
#define DBE4MMEG2_H

#ifdef SS_MME

#include "../mmeg2/mme0mme.h"

#define dbe_mme_createindex_t   mme_createindex_t
#define dbe_mme_dropindex_t     mme_dropindex_t

#define dbe_mme_init            mme_init
#define dbe_mme_done            mme_done
#define dbe_mme_setreadonly     mme_setreadonly
#define dbe_mme_startservice    mme_startservice
#define dbe_mme_getstorage      mme_getstorage
#define dbe_mme_removeuser      mme_removeuser
#define dbe_mme_gettemporarytablecardin     mme_gettemporarytablecardin

#define dbe_mme_insert          mme_insert
#define dbe_mme_delete          mme_delete
#define dbe_mme_update          mme_update
#define dbe_mme_search_init     mme_search_init
#define dbe_mme_search_done     mme_search_done
#define dbe_mme_search_reset    mme_search_reset
#define dbe_mme_search_getaval  mme_search_getaval
#define dbe_mme_search_getsearchinfo    mme_search_getsearchinfo
#define dbe_mme_search_invalidate   mme_search_invalidate
#define dbe_mme_search_restart  mme_search_restart
#define dbe_mme_search_abortrelid   mme_search_abortrelid
#define dbe_mme_search_abortkeyid   mme_search_abortkeyid
#define dbe_mme_search_isactive     mme_search_isactive
#define dbe_mme_search_relock       mme_search_relock
#define dbe_mme_search_nextorprev_n mme_search_nextorprev_n
#define dbe_mme_search_next         mme_search_next
#define dbe_mme_search_prev         mme_search_prev
#define dbe_mme_search_gotoend      mme_search_gotoend
#define dbe_mme_search_setposition  mme_search_setposition
#define dbe_mme_search_gettref      mme_search_gettref
#define dbe_mme_search_setoptinfo   mme_search_setoptinfo
#define dbe_mme_search_newplan      mme_search_newplan
#define dbe_mme_search_printinfo    mme_search_printinfo

#define dbe_mme_locklist_init       mme_locklist_init
#define dbe_mme_locklist_done       mme_locklist_done
#define dbe_mme_locklist_replicafree    mme_locklist_replicafree
#define dbe_mme_locklist_rollback_searches  mme_locklist_rollback_searches
#define dbe_mme_locklist_commit     mme_locklist_commit
#define dbe_mme_locklist_rollback   mme_locklist_rollback
#define dbe_mme_locklist_stmt_commit    mme_locklist_stmt_commit
#define dbe_mme_locklist_stmt_rollback  mme_locklist_stmt_rollback
#define dbe_mme_refkeycheck         mme_refkeycheck

#define dbe_mme_createindex_init    mme_createindex_init
#define dbe_mme_createindex_done    mme_createindex_done
#define dbe_mme_createindex_advance mme_createindex_advance
#define dbe_mme_dropindex_init      mme_dropindex_init
#define dbe_mme_dropindex_done      mme_dropindex_done
#define dbe_mme_dropindex_advance   mme_dropindex_advance

#define dbe_mme_beginrecov          mme_beginrecov
#define dbe_mme_recovinsert         mme_recovinsert
#define dbe_mme_recovplacepage      mme_recovplacepage
#define dbe_mme_recovstmtcommit     mme_recovstmtcommit
#define dbe_mme_recovstmtrollback   mme_recovstmtrollback
#define dbe_mme_recovcommit         mme_recovcommit
#define dbe_mme_recovrollback       mme_recovrollback
#define dbe_mme_abortall            mme_abortall
#define dbe_mme_endrecov            mme_endrecov
#define dbe_mme_rval_init_from_diskbuf  mme_rval_init_from_diskbuf_mutexed
#define dbe_mme_rval_done           mme_rval_done_mutexed

#if !defined(SS_PURIFY) && defined(SS_FFMEM)
#define dbe_mme_getmemctx           mme_getmemctx
#endif /* !SS_PURIFY && SS_FFMEM */

#define dbe_mme_gettvalsamples_nomutex  mme_gettvalsamples_nomutex
#define dbe_mme_lock_mutex              mme_lock_mutex
#define dbe_mme_unlock_mutex            mme_unlock_mutex

#ifdef SS_DEBUG
#define dbe_mme_check_integrity         mme_check_integrity
#endif

#endif /* SS_MME */
#endif /* DBE4MMEG2_H */

