/*************************************************************************\
**  source       * dbe8clst.h
**  directory    * dbe
**  description  * Change list management services
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


#ifndef DBE8CLST_H
#define DBE8CLST_H

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe8flst.h"

typedef struct dbe_chlist_st dbe_chlist_t;
typedef struct dbe_chlist_iter_st dbe_chlist_iter_t;

#ifndef NO_ANSI

dbe_chlist_t *dbe_cl_init(
        su_svfil_t *p_svfile,
        dbe_cache_t *p_cache,
        dbe_freelist_t *p_fl,
        dbe_cpnum_t next_cpnum,
        su_daddr_t list_start);

void dbe_cl_done(
        dbe_chlist_t *p_cl);

su_ret_t dbe_cl_add(
        dbe_chlist_t *p_cl,
        dbe_cpnum_t cpnum,
        su_daddr_t daddr);

bool dbe_cl_find(
        dbe_chlist_t *p_cl,
        dbe_cpnum_t *p_cpnum,
        su_daddr_t daddr);

su_ret_t dbe_cl_preparetosave(
        dbe_chlist_t *p_cl);

su_ret_t dbe_cl_save(
        dbe_chlist_t *p_cl,
        dbe_cpnum_t next_cpnum,
        su_daddr_t *chlist_start);

su_ret_t dbe_cl_linktogether(
        dbe_chlist_t *p_cl_head,
        dbe_chlist_t *p_cl_tail);

su_ret_t dbe_cl_linktoend(
        dbe_chlist_t *p_cl,
        su_daddr_t tail_daddr);

void dbe_cl_setnextcpnum(
        dbe_chlist_t *p_cl,
        dbe_cpnum_t next_cpnum);

void dbe_cl_dochlist(
        dbe_chlist_t *p_cl,
        su_list_t *p_deferchlist);

dbe_chlist_iter_t *dbe_ci_init(
        dbe_chlist_t *p_cl);

void dbe_ci_done(
        dbe_chlist_iter_t *p_ci);

void dbe_ci_getnodeinfo(
        dbe_chlist_iter_t *p_ci,
        dbe_cpnum_t *p_block_cpnum,
        su_daddr_t *p_block_daddr,
        uint *p_nblocks);

bool dbe_ci_nextnode(
        dbe_chlist_iter_t *p_ci);

bool dbe_ci_getnext(
        dbe_chlist_iter_t *p_ci,
        dbe_cpnum_t *p_cpnum,
        su_daddr_t *p_daddr);

void dbe_ci_reset(
        dbe_chlist_iter_t *p_ci);

void dbe_ci_resetnode(
        dbe_chlist_iter_t *p_ci);

#else /* NO_ANSI */

dbe_chlist_t *dbe_cl_init();
void dbe_cl_done();
su_ret_t dbe_cl_add();
bool dbe_cl_find();
su_ret_t dbe_cl_preparetosave();
su_ret_t dbe_cl_save();
su_ret_t dbe_cl_linktogether();
su_ret_t dbe_cl_linktoend();
void dbe_cl_setnextcpnum();
dbe_chlist_iter_t *dbe_ci_init();
void dbe_ci_done();
bool dbe_ci_nextnode();
bool dbe_ci_next();
void dbe_ci_reset();
void dbe_ci_resetnode();

#endif /* NO_ANSI */

#endif /* DBE8CLST_H */
