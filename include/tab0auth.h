/*************************************************************************\
**  source       * tab0auth.h
**  directory    * tab
**  description  * Authorization functions.
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


#ifndef TAB0AUTH_H
#define TAB0AUTH_H

bool tb_auth_pushschemactx(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* schema,
        char* catalog,
        rs_err_t** p_errh);

void tb_auth_popctx(
        rs_sysi_t* cd);

bool tb_auth_ispushed(
        rs_sysi_t* cd);

bool tb_auth_getusername(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long uid,
        char** p_username,
        rs_err_t** p_errh);

#endif /* TAB0AUTH_H */
