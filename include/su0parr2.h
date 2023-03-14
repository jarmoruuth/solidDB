/*************************************************************************\
**  source       * su0parr2.h
**  directory    * su
**  description  * 2-D pointer array data type
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


#ifndef SU0PARR2_H
#define SU0PARR2_H

#include <ssc.h>
#include "su0parr.h"

typedef su_pa_t su_pa2_t; 

su_pa2_t* su_pa2_init(uint n_init_rows, uint n_init_cols);
void      su_pa2_done(su_pa2_t* arr);

void  su_pa2_insertat(su_pa2_t* arr, uint row, uint col, void* data);
uint  su_pa2_insertatrow(su_pa2_t* arr, uint row, void* data);
void* su_pa2_remove(su_pa2_t* arr, uint row, uint col);

void* su_pa2_getdata(su_pa2_t* arr, uint row, uint col);
bool  su_pa2_indexinuse(su_pa2_t* arr, uint row, uint col);

/* Macro su_pa2_do can be used to loop over all elements in the 2-D pa.

   Example:

        su_pa2_t* pa2;
        int       i, j;

        pa2 = su_pa2_init(0, 0);

        ... insert stuff into pa2 ...

        su_pa2_do(pa2, i, j) {
            ... process element at index (i, j) ...
        }
        su_pa_done(pa2);
*/

#define su_pa2_do(pa2, row, col) \
            su_pa_do((pa2), row) \
                su_pa_do((su_pa_t *)su_pa_getdata((pa2), row), col)


#define su_pa2_dorow(pa2, row, col) \
            su_pa_do((su_pa_t *)su_pa_getdata((pa2), row), col)

#endif /* SU0PARR2_H */


