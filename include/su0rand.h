/*************************************************************************\
**  source       * su0rand.h
**  directory    * su
**  description  * Simple re-entrant pseudorandom generator
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


#ifndef SU0RAND_H
#define SU0RAND_H

#include <ssrand.h>

typedef ss_rand_t su_rand_t;

#define su_rand_init(r,s) ss_rand_init(r,s)
#define su_rand_long(r)   ss_rand_int4(r)

#endif /* SU0RAND_H */
