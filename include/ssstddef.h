/*************************************************************************\
**  source       * ssstddef.h
**  directory    * ss
**  description  * stddef.h
**               * 
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


#ifndef SSSTDDEF_H
#define SSSTDDEF_H

#include "ssenv.h"

#if !defined(NO_ANSI)

#  include <stddef.h>

#elif (defined(UNIX) && defined(BANYAN))

#  ifndef NULL
#    define NULL 0L
#  endif

#  include <errno.h>

#  define ptrdiff_t int
#  define size_t unsigned int

#else

#  error Implementation unknown!

#endif


#endif /* SSSTDDEF_H */
