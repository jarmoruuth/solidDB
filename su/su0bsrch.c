/**************************************************************************
**  source       * su0bsrch.c
**  directory    * su
**  description  * Almost like bsearch(), but return values & params differ
**               * 
**               * Copyright (C) 2006 Solid Information Technology Ltd
**************************************************************************/
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
Implementation:
--------------

This module implements a binary search algorithm for arrays.
The way it operates is similar to that of the C Standard
library function bsearch() with the following differences:

1. The return value is boolean:
TRUE if object found
FALSE if not

2. The pointer to the array entry is returned as the last
pointer^2 parameter. If the object is not found the returned
pointer is a pointer to an array entry where the object should
be inserted pushing the object which possibly is at that entry
and all the objects at higher index locations to 1 higher
index entry. If the object is bigger than the last existing object
in the array the pointer is one past the last object,
which either may or may not be allocated for the same array.

Thus this version of bsearch() can also be used to search place
for a new object in an array. The first 5 parameters are exactly
as those of the bsearch() in C lib. The 6th parameter is the pointer
to pointer to the array entry, which would be passed to caller
in the return value in the C library bsearch().

The function pointer provided as the 5th parameter is a pointer to
function of form:

        int cmp_function(
            const void *keypointer,
            const void *arrayelementpointer);

Its return value should be as follows:

1. If key pointed to by keypointer is logically bigger than the
array element pointed to by arrayelementpointer, ie. the array entry
for the key would be at higher index than that of the arrayelement
the return value must be > 0.

2. If key is logically equal to the array element the return value
must be = 0.

3. Otherwise the key is logically smaller than the array element and the
return value must be < 0.

In other words: the return value of cmp_dunction is a logical subtraction
result of key and array element.

The keypointer parameter passed to the comparison function is
actually the same as the the key argument passed to the su_bsearch().
The arrayelement pointer passed to the comparison function is
an address of an array element which is under comparison.

Note: The key and element need not be of same data type as long as
there is the logical comparison relation between them and the user
supplies a proper function to compare them.

Limitations:
-----------

All array positions in the search range must be in use and
the array must be sorted in ascending (or descending) order.
The descending order is possible by providing comparison function
which has opposite return values.

Error handling:
--------------

Passing NULL pointers will cause an assertion failure, if compiled
with debug options, except with the 6th parameter, which may also be NULL.

Objects used:
------------

none

Preconditions:
-------------

array must be in ascending (or descending order).
the provided function must operate according to its specification.

Example:
-------


#endif /* DOCUMENTATION */

#include <ssdebug.h>

#include "su0bsrch.h"

/*##**********************************************************************\
 * 
 *		su_bsearch
 * 
 * Binary search in a sorted array
 * 
 * Parameters :
 *      key - in, use
 *		pointer to search key
 *
 *      base - in, use
 *		pointer to the sorted array
 *
 *      n - in
 *		number of elements in the array
 *
 *      elsize - in
 *		size of one array element
 *
 *      cmp - in, use
 *		pointer to function described above
 *
 * 
 *      retpp - out, ref
 *		pointer to pointer where the returned array entry
 *          address will be stored (passing NULL means no output
 *          pointer is wanted and the assign is not done. The return
 *          value, see below, can still be used).
 *
 * 
 * Return value :
 *      TRUE if found
 *      FALSE if not
 * 
 * Limitations  : See documentation above
 * 
 * Globals used : none
 */
bool su_bsearch(key, base, n, elsize, cmp, retpp)
void *key;      
void *base;     
size_t n;       
size_t elsize;  
su_bsearch_cmpfuncptr_t cmp; 
void **retpp;   
{
        void *try;          /* pointer to element under test */
        size_t n_per_2;
        int cmpv;           /* comparison value */

        ss_dassert(key != NULL);
        ss_dassert(base != NULL);
        ss_dassert(cmp != NULL);
		
        while (n) {
            n_per_2 = n >> 1;
            try = (char*)base + n_per_2 * elsize;
            cmpv = (*cmp)(key, try);
            if (cmpv < 0) {
                n = n_per_2;
            } else if (cmpv > 0) {
                base = (char*)try + elsize;
                n -= n_per_2 + 1;
            } else {	/* cmpv == 0 */
                if (retpp != NULL) {
                    *retpp = try;
                }
                return TRUE;
            }
        }
        if (retpp != NULL) {
            *retpp = base;
        }
        return FALSE;
}

/*##**********************************************************************\
 * 
 *		su_bsearch_ctx
 * 
 * Exactly as su_bsearch() above, but this one takes an extra parameter,
 * the comparison context, which is consequently passed to the comparison
 * function. This enables eg. Attaching an ORDER BY list with Asc/Desc
 * partial ordering specifications to a bsearch
 * 
 * Parameters : 
 * 
 *	key - 
 *		
 *		
 *	base - 
 *		
 *		
 *	n - 
 *		
 *		
 *	elsize - 
 *		
 *		
 *	cmp - in, use
 *
 *		
 *	ctx - 
 *		comparison context
 *		
 *		
 *	retpp - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_bsearch_ctx(
        void *key,              
        void *base,             
        size_t n,               
        size_t elsize,          
        su_bsearch_ctxcmpfuncptr_t
            cmp,                
        void* ctx,              
        void **retpp)           
{
        void *try;          /* pointer to element under test */
        size_t n_per_2;
        int cmpv;           /* comparison value */

        ss_dassert(key != NULL);
        ss_dassert(base != NULL);
        ss_dassert(cmp != NULL);
		
        while (n) {
            n_per_2 = n >> 1;
            try = (char*)base + n_per_2 * elsize;
            cmpv = (*cmp)(key, try, ctx);
            if (cmpv < 0) {
                n = n_per_2;
            } else if (cmpv > 0) {
                base = (char*)try + elsize;
                n -= n_per_2 + 1;
            } else {	/* cmpv == 0 */
                if (retpp != NULL) {
                    *retpp = try;
                }
                return TRUE;
            }
        }
        if (retpp != NULL) {
            *retpp = base;
        }
        return FALSE;
}

/*##**********************************************************************\
 * 
 *		su_bsearch_replacement
 * 
 * Replacement for library function bsearch()
 * 
 * Parameters : 
 * 
 *	key - 
 *		
 *		
 *	base - 
 *		
 *		
 *	n - 
 *		
 *		
 *	elsize - 
 *		
 *		
 *	cmp - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void* su_bsearch_replacement(
        void *key,              
        void *base,             
        size_t n,               
        size_t elsize,          
        su_bsearch_cmpfuncptr_t cmp)
{
        bool b;
        void* retp = NULL;
        b = su_bsearch(key, base, n, elsize, cmp, &retp);
        if (b) {
            return(retp);
        } else {
            return(NULL);
        }
}
