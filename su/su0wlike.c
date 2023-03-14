/*************************************************************************\
**  source       * su0wlike.c
**  directory    * su
**  description  * Implementation of SQL string "LIKE" operator
**               * for UNICODE strings
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

This module implements the LIKE operation of SQL

Limitations:
-----------

None

Error handling:
--------------

None

Objects used:
------------

None

Preconditions:
-------------

None

Multithread considerations:
--------------------------

None

Example:
-------

None

**************************************************************************

#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssstring.h>
#include <ssdebug.h>
#include "su0wlike.h"

#define SU_SLIKE_INSTANTIATE_UNICODE

/* The following #include file instantiates the su_wlike() etc. */
#include "su1tlike.h" 

#undef SU_SLIKE_INSTANTIATE_UNICODE

#define SU_SLIKE_INSTANTIATE_UNIPAT4CHAR

/* The following #include file instantiates the su_wslike() */
#include "su1tlike.h" 

#undef SU_SLIKE_INSTANTIATE_UNIPAT4CHAR

#define SU_SLIKE_INSTANTIATE_CHARPAT4UNI

/* The following #include file instantiates the su_swlike() */
#include "su1tlike.h" 
