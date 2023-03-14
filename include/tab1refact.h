/*************************************************************************\
**  source       * tab0tran.h
**  directory    * tab
**  description  * Referential action functions.
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


#ifndef TAB1REFACT_H
#define TAB1REFACT_H

#include <dbe0erro.h>
#include <rs0entna.h>

#include "tab0type.h"

typedef struct tb_trans_keyaction_state tb_trans_keyaction_state_t;

uint tb_ref_keyaction(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        rs_key_t*       refkey,
        rs_ttype_t*     ttype,
        int*            uflags,
        rs_tval_t*      old_tval,
        rs_tval_t*      tval,
        tb_trans_keyaction_state_t **state_p,
        rs_err_t**      p_errh);

void tb_ref_keyaction_free(
        rs_sysi_t*      cd,
        tb_trans_keyaction_state_t **state_p);

#endif /* TAB0TRAN_H */
