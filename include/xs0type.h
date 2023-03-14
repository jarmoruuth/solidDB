/*************************************************************************\
**  source       * xs0type.h
**  directory    * xs
**  description  * eXternal Sorter TYPE definitions
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


#ifndef XS0TYPE_H
#define XS0TYPE_H

#include <rs0types.h>
#include <rs0error.h>

#ifndef SQLEXTSORT_T_DEFINED
#define SQLEXTSORT_T_DEFINED
typedef struct xs_sorter_st xs_sorter_t;
#endif

typedef enum {
        XS_RC_CONT = 0,
        XS_RC_SUCC = 1,
        XS_RC_ERROR = 2
} xs_retvalues_t;

typedef uint xs_ret_t;

typedef int (*xs_qcomparefp_t) (void* arg1, void* arg2, void* context);

#endif /* XS0TYPE_H */
