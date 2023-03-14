/*************************************************************************\
**  source       * dbe0brb.h
**  directory    * dbe
**  description  * Blob Reference Buffer
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


#ifndef DBE0BRB_H
#define DBE0BRB_H

#include <su0rbtr.h>

#include <rs0sysi.h>
#include <rs0atype.h>
#include <rs0aval.h>
#include <uti0va.h>

typedef struct dbe_brb_st dbe_brb_t;

dbe_brb_t* dbe_brb_init(
        void);
void dbe_brb_done(
        dbe_brb_t* brb);
bool dbe_brb_needtocopy_setsaved(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);
bool dbe_brb_defer_delete(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);
bool dbe_brb_defer_delete_va(
        dbe_brb_t* brb,
        va_t* va);
bool dbe_brb_needtodelete_remove(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);
bool dbe_brb_removeif(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);
void dbe_brb_insert(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);
void dbe_brb_insert_removeonsave(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

#endif /* DBE0BRB_H */
