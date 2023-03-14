/*************************************************************************\
**  source       * dbe5ivld.h
**  directory    * dbe
**  description  * Index validate routines for transaction validation.
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


#ifndef DBE5IVLD_H
#define DBE5IVLD_H

#include <uti0vtpl.h>

#include <su0list.h>

#include <rs0sysi.h>

#include "dbe9type.h"
#include "dbe6srk.h"
#include "dbe6bnod.h"
#include "dbe6bsea.h"
#include "dbe0type.h"
#include "dbe0erro.h"

/* Index validation search.
*/
typedef struct dbe_indvld_st {
        ss_debug(dbe_chk_t iv_chk;)
        dbe_index_t*            iv_index;        /* index system used */
        dbe_bkey_t*             iv_beginkey;     /* Range begin. */
        dbe_bkey_t*             iv_endkey;       /* Range end. */
        dbe_btrsea_t            iv_bonsaisearch; /* bonsai tree search */
        dbe_btrsea_keycons_t    iv_kc;           /* Key constraints */
        dbe_btrsea_timecons_t   iv_tc;           /* Time constraints */
        rs_sysi_t*              iv_cd;
        ss_debug(dbe_bkey_t*    iv_lastkey;)
} dbe_indvld_t;

dbe_indvld_t* dbe_indvld_initbuf(
        dbe_indvld_t* indvld,
        rs_sysi_t* cd,
        dbe_index_t* index,
        dbe_trxid_t usertrxid,
        dbe_trxnum_t maxtrxnum,
        dbe_trxnum_t mintrxnum,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        rs_key_t* key,
        dbe_keyvld_t keyvldtype,
        bool earlyvld,
        bool pessimistic);

void dbe_indvld_donebuf(
        dbe_indvld_t* indvld);

dbe_ret_t dbe_indvld_next(
        dbe_indvld_t* indvld,
        dbe_srk_t** p_srk);

#endif /* DBE5IVLD_H */
