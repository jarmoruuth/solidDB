/*************************************************************************\
**  source       * su0parr2.c
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


This module implements a 2-dimensional pointer array data type. The
implementation bases heavily on 1-D pointer array data type and its
methods (see the documentation elsewhere).

Suppose we have a 1-D pointer array data type (su_pa_t).
We can create a 2-D pointer array (su_pa2_t) by function

        su_pa2_init(n_init_rows, n_init_cols)

Input parameters n_init_rows and n_init_cols specify how much space is
allocated at initialization time for becoming elements.
Internally we create <n_init_rows> 1-D arrays and insert them in one
1-D array, thus forming a su_pa2_t object:

        pa2 : su_pa_for_row_0, su_pa_for_row_1, ... su_pa_for_row_<n_init_rows>

Each one of the rows is a pa. We insert temporarily an item at the
<n_init_cols> index to reserve required space at once:

        su_pa_for_row_0: col_0, col_1, col_2, ..., col_<n_init_cols>

After initialization the pa2 object looks like this:

        su_pa_for_row_0: col_0, col_1, col_2, ..., col_<n_init_cols>
        su_pa_for_row_1: col_0, col_1, col_2, ..., col_<n_init_cols>
        su_pa_for_row_2: col_0, col_1, col_2, ..., col_<n_init_cols>
                .
                .
                .

        su_pa_for_row_n: col_0, col_1, col_2, ..., col_<n_init_cols>

There is no data in the pa2 but the required space is allocated.

New data items can be inserted to the pa2 using function

        su_pa2_insertat(pa2, row, col, dataptr)

The data inserted can be accessed using
        
        su_pa2_getdata

To remove an item one must call

        su_pa2_remove

It must be noticed that because of the implementation of the underlying
su_pa_t it is NOT ALLOWED to try to

        - insert another item to the same location (trying to overwrite)
        - access or remove an item that does not exist

Described operations lead to ss_errors. To find out wheter an item exist
or not, one must call

        su_pa2_indexinuse

A handy way to scan through all the elements in the array is to use

        su_pa2_do


Limitations:
-----------

su_pa2_t functions are in no way protected against simultaneous access.
User has to take care of needed mutex semaphores.

It could be nice to have functions

        su_pa2_do_row    (Scan through elements of certain row)
        su_pa2_do_column (Scan through elements of certain column)

At least the su_pa2_do_row is easy an straightforward to implement


Error handling:
--------------

All illegal operation mentioned above lead to ss_errors.
There is no error return mechanism.


Objects used:
------------

ss- asserts
su_pa_t (in su0parr.c)


Preconditions:
-------------
None


Multithread considerations:
--------------------------

See limitations.

Example:
-------

#include <su0parr2.h>

        su_pa2_t* pa2;
        int       i, j;
        static char data1[] = "data1";
        static char data2[] = "data2";

        /* Create a pa2 */
        pa2 = su_pa2_init(0, 0);

        /* insert stuff into pa2 ... */
        su_pa2_insertat(pa2, 1, 1, data1);
        su_pa2_insertat(pa2, 2, 2, data2);


        su_pa2_do(pa2, i, j) {
            /* remove the element at index (i, j) */
            su_pa2_remove(pa2, i, j);
        }

        su_pa_done(pa2);


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstdio.h>
#include <ssmem.h>
#include <ssdebug.h>
#include "su0parr2.h"

/*##**********************************************************************\
 * 
 *		su_pa2_init
 * 
 * Creates a 2-dimensional array.
 * 
 * Parameters : 
 * 
 *	n_init_rows - in
 *	    Number of initial rows to allocate        	
 *		
 *	n_init_cols - in
 *	    Number of initial columns to allocate        	
 *		
 * Return value - give :
 *      pointer to new array object
 *		
 * Limitations  : 
 *      max number of columns is about 64K / sizeof (void *) 
 *      max number of rows is the same 
 * 
 * Globals used : 
 */
su_pa2_t* su_pa2_init(uint n_init_rows, uint n_init_cols)
{
        su_pa_t* row_pa;
        int      i;

        row_pa = su_pa_init();
        ss_autotest(su_pa_setmaxlen(row_pa, 40000));

        for (i=0; i< (int)n_init_rows; i++) {

            /* Initialize row_ea */

            su_pa_t* col_pa = su_pa_init();
            ss_autotest(su_pa_setmaxlen(col_pa, 40000));

            su_pa_insertat(row_pa, i, col_pa);

            if (n_init_cols > 0) {

                /* Reserve space for col_pa by adding a dummy item
                   at n_init_cols */

                su_pa_insertat(col_pa, n_init_cols-1, col_pa);
                su_pa_remove(col_pa, n_init_cols-1);
            }
        }
        return(row_pa);
}

/*##**********************************************************************\
 * 
 *		su_pa2_done
 * 
 * Removes all items from array and deallocates array resources
 * 
 * Parameters : 
 *	arr - in, take 
 *		array to be freed
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_pa2_done(su_pa2_t* arr)
{
        int row;

	su_pa_do(arr, row) {
                  
            if (su_pa_indexinuse(arr, row)) {
                su_pa_removeall(su_pa_getdata(arr, row));
                su_pa_done(su_pa_getdata(arr, row));
            }
	} 

        su_pa_done(arr);
}

/*##**********************************************************************\
 * 
 *		su_pa2_insertat
 * 
 * Inserts a new item into array at position (row, col).
 * Assumes that there is no previous item in the same position.
 * 
 * Parameters : 
 * 
 *	arr - in out, use
 *		array
 *
 *	row - in
 *		row index
 *
 *	col - in
 *		column index
 *
 *	ptr - in, hold
 *		data pointer
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_pa2_insertat(su_pa2_t* arr, uint row, uint col, void* ptr)
{
        su_pa_t* col_pa;

        if (!su_pa_indexinuse(arr, row)) {
            /* This is the first item in this row */
            col_pa = su_pa_init();
            ss_autotest(su_pa_setmaxlen(col_pa, 40000));
            su_pa_insertat(arr, row, col_pa);
        } else {
            col_pa = su_pa_getdata(arr, row);
        }

        if (su_pa_indexinuse(col_pa, col)) {
            /* Remove the old item ? su_pa_remove(col_pa, col); */
            ss_error;
        }

        su_pa_insertat(col_pa, col, ptr);
        ss_dassert(su_pa_indexinuse(col_pa, col));
}

/*##**********************************************************************\
 * 
 *		su_pa2_insertatrow
 * 
 * Inserts a new item into given row of the array.
 * 
 * Parameters : 
 * 
 *	arr - in out, use
 *		array
 *
 *	row - in
 *		row index
 *
 *	data - in, hold
 *		data pointer
 *
 * Return value : 
 *      
 *      the index of the new item
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint su_pa2_insertatrow(arr, row, data)
	su_pa2_t* arr;
	uint row;
	void* data;
{
        su_pa_t* col_pa;
        uint     new_index;

        if (!su_pa_indexinuse(arr, row)) {
            /* This is the first item in this row */
            col_pa = su_pa_init();
            ss_autotest(su_pa_setmaxlen(col_pa, 40000));
            su_pa_insertat(arr, row, col_pa);
        } else {
            col_pa = su_pa_getdata(arr, row);
        }

        new_index = su_pa_insert(col_pa, data);
        ss_dassert(su_pa_indexinuse(col_pa, new_index));

        return(new_index);
}

/*##**********************************************************************\
 * 
 *		su_pa2_getdata
 * 
 * Returns the data pointer in arr at (row, col)
 * 
 * Parameters : 
 * 
 *	arr - in, use
 *		array
 *
 *	row - in
 *		row index
 *
 *	col - in
 *		column index
 *
 * Return value : ref
 * 
 *      data pointer, if item found 
 *      NULL, if empty (See limitation)
 * 
 * Limitations  : Assumes that the index is in use
 * 
 * Globals used : 
 */
void* su_pa2_getdata(su_pa2_t* arr, uint row, uint col)
{
        su_pa_t* col_pa;

        if (!su_pa_indexinuse(arr, row)) {
            ss_error;
        }

        col_pa = su_pa_getdata(arr, row);

        if (su_pa_indexinuse(col_pa, col)) {
            return(su_pa_getdata(col_pa, col));
        } else {
            ss_error;
            return(NULL);
        }
}

/*##**********************************************************************\
 * 
 *		su_pa2_indexinuse
 * 
 * Checks if a given index is in use
 * 
 * Parameters : 
 * 
 *	arr - in, use
 *		array
 *
 *	row - in
 *		row index 
 *
 *	col - in
 *		column index
 *
 * Return value :  
 *
 *      1, index is in use                
 *      0, index is not in use
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_pa2_indexinuse(su_pa2_t* arr, uint row, uint col)
{
        su_pa_t* col_pa;

        if (!su_pa_indexinuse(arr, row)) {
            return(0);
        }

        col_pa = su_pa_getdata(arr, row);

        if (!su_pa_indexinuse(col_pa, col)) {
            return(0);
        } else {
            return(1);
        }
}

/*##**********************************************************************\
 * 
 *		su_pa2_remove
 * 
 * Removes an item from arr position (row, col).
 * Assumes that the position is not empty.
 * 
 * Parameters : 
 * 
 *	arr - in out, use
 *		array
 *
 *	row - in
 *		row index
 *
 *	col - in
 *		column index
 *
 * Return value : ref
 *
 *      user data at the removed location,
 *      NULL, if location was empty
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void* su_pa2_remove(su_pa2_t* arr, uint row, uint col)
{
        su_pa_t* col_pa;

        if (!su_pa_indexinuse(arr, row)) {
            ss_error;
        }

        col_pa = su_pa_getdata(arr, row);

        if (su_pa_indexinuse(col_pa, col)) {
            return(su_pa_remove(col_pa, col));
        } else {
            ss_error;
            return(NULL);
        }
}
