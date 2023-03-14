/*************************************************************************\
**  source       * dbe9crec.h
**  directory    * dbe
**  description  * Change record declaration for freelist & change list
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


#ifndef DBE9CREC_H
#define DBE9CREC_H

#include "dbe9type.h"
#include "dbe0type.h"

/* one change record */
struct dbe_clrecord_st {
        dbe_cpnum_t clr_cpnum;  /* checkpoint which created replaced block */
        su_daddr_t  clr_daddr;  /* disk address of replaced block */
};
typedef struct dbe_clrecord_st dbe_clrecord_t;

#endif /* DBE9CREC_H */
