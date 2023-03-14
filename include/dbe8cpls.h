/*************************************************************************\
**  source       * dbe8cpls.h
**  directory    * dbe
**  description  * checkpoint list
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


#ifndef DBE8CPLS_H
#define DBE8CPLS_H

#include <su0svfil.h>
#include "dbe8cach.h"
#include "dbe8flst.h"
#include "dbe8clst.h"

typedef struct dbe_cplist_st dbe_cplist_t;

dbe_cplist_t *dbe_cpl_init(
        su_svfil_t *p_svfile,
        dbe_cache_t *p_cache,
        dbe_freelist_t *p_fl,
        dbe_chlist_t* p_cl,
        su_daddr_t list_daddr);

void dbe_cpl_done(
        dbe_cplist_t *p_cpl);

su_ret_t dbe_cpl_deletefromdisk(
        dbe_cplist_t *p_cpl_current,
        dbe_cpnum_t cpnum,
        su_daddr_t list_to_delete_daddr);

void dbe_cpl_add(
        dbe_cplist_t *p_cpl,
        dbe_cpnum_t cpnum,
        su_daddr_t record_daddr);

void dbe_cpl_remove(
        dbe_cplist_t *p_cpl,
        dbe_cpnum_t cpnum);

bool dbe_cpl_isalive(
        dbe_cplist_t *p_cpl,
        dbe_cpnum_t cpnum);

su_daddr_t dbe_cpl_getdaddr(
        dbe_cplist_t *p_cpl,
        dbe_cpnum_t cpnum);

void dbe_cpl_setdaddr(
        dbe_cplist_t *p_cpl,
        dbe_cpnum_t cpnum,
        su_daddr_t record_daddr);

dbe_cpnum_t dbe_cpl_lastcommon(
        dbe_cplist_t *p_cpl,
        dbe_cplist_t *p_oldcpl);

dbe_cpnum_t dbe_cpl_prevfrom(
        dbe_cplist_t *p_cpl,
        dbe_cpnum_t cpnum);

dbe_cpnum_t dbe_cpl_nextto(
        dbe_cplist_t *p_cpl,
        dbe_cpnum_t cpnum);

dbe_cpnum_t dbe_cpl_last(
        dbe_cplist_t *p_cpl);

su_ret_t dbe_cpl_save(
        dbe_cplist_t *p_cpl,
        dbe_cpnum_t cpnum,
        su_daddr_t *cpl_daddr);



#endif /* DBE8CPLS_H */
