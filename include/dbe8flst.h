/*************************************************************************\
**  source       * dbe8flst.h
**  directory    * dbe
**  description  * Interface to free list management in SOLID database
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


#ifndef DBE8FLST_H
#define DBE8FLST_H

#include <su0svfil.h>
#include <su0list.h>
#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe0type.h"

typedef struct dbe_freelist_st dbe_freelist_t;
typedef struct dbe_fl_diff_iter_st dbe_fl_diff_iter_t;

dbe_freelist_t *dbe_fl_init(
        su_svfil_t *p_svfile,
        dbe_cache_t *p_cache,
        su_daddr_t superblock_disk_addr,
        su_daddr_t file_size_at_checkpoint,
        uint extend_increment,
        uint max_seq_alloc,
        bool globaly_sorted,
        dbe_cpnum_t next_cpnum,
        dbe_db_t* db);

void dbe_fl_setchlist(
        dbe_freelist_t *p_fl,
        void *p_chlist);

void dbe_fl_done(
        dbe_freelist_t *p_fl);

void dbe_fl_setreservesize(
        dbe_freelist_t *p_fl,
        int reservesize);

su_ret_t dbe_fl_free(
        dbe_freelist_t *p_fl,
        su_daddr_t daddr);

void dbe_fl_remove(
        dbe_freelist_t *p_fl,
        su_daddr_t daddr);

su_ret_t dbe_fl_alloc(
        dbe_freelist_t *p_fl,
        su_daddr_t *p_daddr,
        dbe_info_t* info);

su_ret_t dbe_fl_save(
        dbe_freelist_t *p_fl,
        dbe_cpnum_t next_cpnum,
        su_daddr_t *p_superblock_daddr,
        su_daddr_t *p_filesize);

su_ret_t dbe_fl_free_deferch(
        dbe_freelist_t *p_fl,
        su_daddr_t daddr,
        su_list_t **p_chl);

su_ret_t dbe_fl_alloc_deferch(
        dbe_freelist_t *p_fl,
        su_daddr_t *p_daddr,
        su_list_t **p_chl,
        dbe_info_t* info);

su_ret_t dbe_fl_save_deferch(
        dbe_freelist_t *p_fl,
        dbe_cpnum_t next_cpnum,
        su_daddr_t *p_superblock_daddr,
        su_daddr_t *p_filesize,
        su_list_t **p_chl);

ulong dbe_fl_getfreeblocks(
        dbe_freelist_t *p_fl);

su_daddr_t dbe_fl_first_free(
        dbe_freelist_t *p_fl);

su_daddr_t dbe_fl_last_busy(
        dbe_freelist_t *p_fl);

bool dbe_fl_is_free(
        dbe_freelist_t *p_fl,
        su_daddr_t daddr);

#ifdef SS_DEBUG

/* uncomment the next line to start using debugging free & alloc */
/* #define DBE_FL_DEBUG */

#ifdef DBE_FL_DEBUG

#define dbe_fl_free(fl, daddr)  dbe_fl_dbgfree(fl, daddr, __FILE__, __LINE__)
#define dbe_fl_alloc(fl, p_daddr, info) dbe_fl_dbgalloc(fl, p_daddr, info, __FILE__, __LINE__)

#endif /* DBE_FL_DEBUG */

void dbe_fl_printlist(
        su_svfil_t *p_svfile,
        dbe_cache_t *p_cache,
        su_daddr_t superblock_disk_addr,
        su_daddr_t file_size_at_checkpoint,
        uint extend_increment,
        dbe_cpnum_t next_cpnum);

su_ret_t dbe_fl_dbgfree(
        dbe_freelist_t *p_fl,
        su_daddr_t daddr,
        char* file,
        int line);

su_ret_t dbe_fl_dbgalloc(
        dbe_freelist_t *p_fl,
        su_daddr_t *p_daddr,
        dbe_info_t* info,
        char* file,
        int line);

#endif /* SS_DEBUG */

su_ret_t dbe_fl_seq_alloc(
        dbe_freelist_t *p_fl,
        su_daddr_t prev_daddr,
        su_daddr_t *p_daddr);

su_ret_t dbe_fl_seq_create(
        dbe_freelist_t *p_fl,
        su_daddr_t *p_daddr,
        dbe_info_t* info);

void dbe_fl_seq_flush(
        dbe_freelist_t *p_fl,
        su_daddr_t daddr);

void dbe_fl_seq_flushall(
        dbe_freelist_t *p_fl);

void dbe_fl_set_filesize(
    dbe_freelist_t *p_fl,
    su_daddr_t filesize);

#endif /* DBE8FLST_H */
