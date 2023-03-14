/*************************************************************************\
**  source       * xs2cmp.h
**  directory    * xs
**  description  * compare functions for solid sort utility
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


#ifndef XS2CMP_H
#define XS2CMP_H

#include <uti0vtpl.h>
#include <su0list.h>
#include "xs0acnd.h"

int xs_qsort_cmp(
        vtpl_t** vtpl1,
        vtpl_t** vtpl2,
        uint* cmpcondarr);

#endif /* XS2CMP_H */
