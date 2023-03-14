/*************************************************************************\
**  source       * dbe7binf.h
**  directory    * dbe
**  description  * Blob infi
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


#ifndef DBE7BINF_H
#define DBE7BINF_H

#include <ssc.h>

#include <uti0va.h>
#include <uti0vtpl.h>

#define DBE_BLOBINFO_ITEMLEN    2
#define DBE_BLOBINFO_HEADERLEN  (VA_LENGTHMAXLEN + DBE_BLOBINFO_ITEMLEN)

void dbe_blobinfo_init(
        dynva_t* p_blobinfo,
        uint nparts);

void dbe_blobinfo_append(
        dynva_t* p_blobinfo,
        uint partno);

bool* dbe_blobinfo_getattrs(
        vtpl_t* vtpl,
        int tuple_nattrs,
        int* p_nattrs);

#endif /* DBE7BINF_H */
