/*************************************************************************\
**  source       * ssstdlib.h
**  directory    * ss
**  description  * stdlib.h
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


#ifndef SSSTDLIB_H
#define SSSTDLIB_H

#include "ssenv.h"

#ifndef NO_ANSI

#  include <stdlib.h>

#elif (defined(UNIX) && defined(BANYAN))

double atof(); /* defined in math.h */

#define RAND_MAX 32767

#ifdef abs
#undef abs /* macros.h has an erroneous definition */
#endif
#define abs(x) ((x) < 0 ? -(x) : (x))
#define labs(x) abs(x)

/* The following don't exist: atol, strtod, strtol, strtoul, atexit, getenv,
   bsearch, div, and ldiv. */

#else

   Implementation unknown!

#endif


#ifndef __min
#define __min(a, b)   ((a) < (b) ? (a) : (b))
#endif

#ifndef __max
#define __max(a, b)   ((a) < (b) ? (b) : (a))
#endif


#endif /* SSSTDLIB_H */
