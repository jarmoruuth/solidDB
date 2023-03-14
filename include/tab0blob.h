/*************************************************************************\
**  source       * tab0blob.h
**  directory    * tab
**  description  * Table level interface for BLObs
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


#ifndef TAB0BLOB_H
#define TAB0BLOB_H

#include <rs0sysi.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <dbe0type.h>

su_ret_t tb_blob_readsmallblobstotvalwithinfo(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        dbe_blobsize_t smallblobsizemax,
        uint* p_nblobs_read,
        uint* p_nblobs_total);

su_ret_t tb_blob_readsmallblobstotval(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        dbe_blobsize_t smallblobsizemax);

bool tb_blob_loadblobtoaval(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        size_t sizelimit);

#endif /* TAB0BLOB_H */
