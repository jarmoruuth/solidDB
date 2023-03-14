/*************************************************************************\
**  source       * su0types.h
**  directory    * su
**  description  * Common types.
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


#ifndef SU0TYPES_H
#define SU0TYPES_H

#include <ssc.h>
#include <sslimits.h>

#ifndef SU_DADDR_T_DEFINED
#define SU_DADDR_T_DEFINED
typedef FOUR_BYTE_T su_daddr_t;
#endif /* SU_DADDR_T_DEFINED */

#ifndef SU_DADDR_NULL
#define SU_DADDR_NULL   ((su_daddr_t)-1L)
#define SU_DADDR_MAX    (SU_DADDR_NULL - 1U)
#endif /* SU_DADDR_NULL */

typedef struct substreamstruct su_bstream_t;

#define SU_MAXNAMELEN   254

#define SU_MAXLOCKTIMEOUT  1000000

#endif /* SU0TYPES_H */
