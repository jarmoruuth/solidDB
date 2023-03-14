/*************************************************************************\
**  source       * xs0qsort.h
**  directory    * xs
**  description  * Quick sort for presorting buffers
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


#ifndef XS0QSORT_H
#define XS0QSORT_H

#include <ssstddef.h>

#include "xs0type.h"

void xs_qsort(
	void* bot,
	size_t nmemb,
        size_t size,
	xs_qcomparefp_t compar,
        void* context);

void xs_qsort_context(
	void* cd,
	void* base,
	size_t item_c,
        size_t size,
	xs_qcomparefp_t compar);

#endif /* XS0QSORT_H */
