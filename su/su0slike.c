/*************************************************************************\
**  source       * su0slike.c
**  directory    * su
**  description  * Implementation of SQL string "LIKE" operator
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
#include "su0slike.h"

#define SU_SLIKE_INSTANTIATE_ASCII
/* The following #include file instantiates the su_slike() etc. */
#include "su1tlike.h" 
