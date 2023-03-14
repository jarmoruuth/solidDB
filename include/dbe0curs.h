/*************************************************************************\
**  source       * dbe0curs.h
**  directory    * dbe
**  description  * Relation cursor routines.
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


#ifndef DBE0CURS_H
#define DBE0CURS_H

#include <ssc.h>

#include <su0list.h>

#include <rs0error.h>
#include <rs0types.h>
#include <rs0pla.h>
#include <rs0vbuf.h>

#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe0tref.h"
#include "dbe0trx.h"

typedef struct dbe_search_st dbe_cursor_t;

dbe_cursor_t* dbe_cursor_init(
        dbe_trx_t*          trx,
        rs_ttype_t*         ttype,
        rs_ano_t*           sellist,
        rs_pla_t*           plan,
        dbe_cursor_type_t   cursor_type,
        bool*               p_newplan,
        rs_err_t**          p_errh);

void dbe_cursor_done(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx);

dbe_ret_t dbe_cursor_getunique(
        dbe_trx_t*          trx,
        rs_ttype_t*         ttype,
        rs_ano_t*           sellist,
        rs_pla_t*           plan,
        rs_tval_t*          tval,
        dbe_bkey_t**        p_bkeybuf,
        rs_err_t**          p_errh);

void dbe_cursor_reset(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_ttype_t*   ttype,
        rs_ano_t*     sellist,
        rs_pla_t*     plan);

dbe_ret_t dbe_cursor_reset_fetch(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_ttype_t*   ttype,
        rs_ano_t*     sellist,
        rs_pla_t*     plan,
        rs_tval_t**   p_tval,
        rs_err_t**    p_errh);

void dbe_cursor_setisolation_transparent(
        dbe_cursor_t* cursor,
        bool          transparent);

dbe_ret_t dbe_cursor_nextorprev(
        dbe_cursor_t* cursor,
        bool          nextp,
        dbe_trx_t*    trx,
        rs_tval_t**   p_tval,
        rs_err_t**    p_errh);

dbe_ret_t dbe_cursor_relock(
        dbe_cursor_t*   cursor,
        dbe_trx_t*      trx,
        rs_err_t**      p_errh);

dbe_ret_t dbe_cursor_nextorprev_n(
        dbe_cursor_t*   cursor,
        bool            nextp,
        dbe_trx_t*      trx,
        rs_vbuf_t*      vb,
        rs_err_t**      p_errh);

dbe_ret_t dbe_cursor_next(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_tval_t**   p_tval,
        rs_err_t**    p_errh);

dbe_ret_t dbe_cursor_prev(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_tval_t**   p_tval,
        rs_err_t**    p_errh);

dbe_ret_t dbe_cursor_gotoend(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_err_t**    p_errh);

dbe_ret_t dbe_cursor_setposition(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_tval_t*    tval,
        rs_err_t**    p_errh);

dbe_ret_t dbe_cursor_delete(
        dbe_cursor_t*   cursor,
        rs_tval_t*      tval,
        dbe_trx_t*      trx,
        rs_relh_t*      relh,
        rs_err_t**      p_errh);

dbe_ret_t dbe_cursor_update(
        dbe_cursor_t*   cursor,
        rs_tval_t*      tval,
        dbe_trx_t*      trx,
        rs_relh_t*      relh,
        bool*           upd_attrs,
        rs_tval_t*      upd_tval,
        dbe_tref_t*     new_tref,
        rs_err_t**      p_errh);

rs_aval_t* dbe_cursor_getaval(
        dbe_cursor_t*   cursor,
        rs_tval_t*      tval,
        rs_atype_t*     atype,
        int             kpno);

dbe_tref_t* dbe_cursor_gettref(
        dbe_cursor_t*   cursor,
        rs_tval_t*      tval);

void dbe_cursor_setoptinfo(
        dbe_cursor_t* cursor,
        ulong tuplecount);

void dbe_cursor_restartsearch(
        dbe_cursor_t*   cursor,
        dbe_trx_t*      trx);

#ifdef SS_QUICKSEARCH
void* dbe_cursor_getquicksearch(
        dbe_cursor_t* cursor,
        bool longsearch);
#endif /* SS_QUICKSEARCH */

#endif /* DBE0CURS_H */
