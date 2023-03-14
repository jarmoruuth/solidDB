/*************************************************************************\
**  source       * ssctype.h
**  directory    * ss
**  description  * Portable interface to <ctype.h> services
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


#ifndef SSCTYPE_H
#define SSCTYPE_H

#include <ctype.h>
#include "sschcvt.h"

#if defined(UNIX) && defined(BSD)

/* BSD Unix has weird toupper() & tolower(); here's a fix
 */
#undef toupper
#undef tolower
#define toupper(c)  (islower(c) ? ((c) - ('a' - 'A')) : (c))
#define tolower(c)  (isupper(c) ? ((c) + ('a' - 'A')) : (c))

#endif /* BSD UNIX */

#endif /* SSCTYPE_H */
