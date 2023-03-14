/*************************************************************************\
**  source       * su0parr.c
**  directory    * su
**  description  * Pointer array data type.
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

Pointer array is a dynamic array structure used to store pointers.
The array size can dynamically grow or shrink.

Limitations:
-----------

The array is allocated to a single memory block. In segmented memory
architecture the array size is thus limited.

Error handling:
--------------

Asserts.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

The node routines are not reentrant. The caller is responsible for
coordinating the concurrent access to the node.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define SU0PARR_C

#include <ssstdio.h>

#include <ssc.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssdebug.h>

#include "su1check.h"
#include "su0rbtr.h"
#include "su0parr.h"

/*##**********************************************************************\
 * 
 *		su_pa_init
 * 
 * Initializes and returns an empty pointer array.
 * 
 * Parameters :
 * 
 * Return value - give : 
 * 
 *      Pointer array pointer.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_pa_t* su_pa_init(void)
{
        su_pa_t* pa;

        pa = SsMemAlloc(sizeof(su_pa_t));

        pa->pa_nelems = 0;
        pa->pa_size = 0;
        pa->pa_elems = NULL;
        pa->pa_chk = SUCHK_PA;
        pa->pa_freerbt = NULL;
        pa->pa_freearray = NULL;
        pa->pa_maxfreearray = 0;
        pa->pa_curfreearray = 0;
        ss_debug(pa->pa_semnum = 0);
        ss_autotest(pa->pa_maxlen = 10000);

        CHK_PA(pa);

        return(pa);
}

/*##**********************************************************************\
 * 
 *		su_pa_done
 * 
 * Releases resources allocated to the pointer array.
 * 
 * Parameters : 
 * 
 *	pa - in, take
 *		Pointer array pointer.
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_pa_done(su_pa_t* pa)
{
        CHK_PA(pa);

        if (pa->pa_maxfreearray > 0) {
            ss_dassert(pa->pa_freerbt != NULL);
            ss_dassert(pa->pa_freearray != NULL);
            su_rbt_done(pa->pa_freerbt);
            SsMemFree(pa->pa_freearray);
        }
        if (pa->pa_size > 0) {
            SsMemFree(pa->pa_elems);
        }
        SsMemFree(pa);
}

/*#***********************************************************************\
 * 
 *		rbt_compare
 * 
 * 
 * 
 * Parameters : 
 * 
 *	key1 - 
 *		
 *		
 *	key2 - 
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
static int rbt_compare(void* key1, void* key2)
{
        return((int)key1 - (int)key2);
}

/*##**********************************************************************\
 * 
 *		su_pa_setrecyclecount
 * 
 * 
 * 
 * Parameters : 
 * 
 *	pa - 
 *		
 *		
 *	maxsize - 
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
void su_pa_setrecyclecount(su_pa_t* pa, uint maxsize)
{
        int i;

        CHK_PA(pa);
        ss_dassert(maxsize > 0);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        if (pa->pa_maxfreearray == 0) {
            pa->pa_maxfreearray = maxsize;
            pa->pa_curfreearray = 0;
            pa->pa_freerbt = su_rbt_init(rbt_compare, NULL);
            pa->pa_freearray = SsMemAlloc(maxsize * sizeof(pa->pa_freearray[0]));
            for (i = 0; i < (int)maxsize; i++) {
                pa->pa_freearray[i] = -1;
            }
        }
}

/*##**********************************************************************\
 * 
 *		su_pa_insert
 * 
 * Inserts a new element to the pointer array. Returns the index of
 * the new element.
 * 
 * Parameters : 
 * 
 *	pa - in out, use
 *		Pointer array pointer.
 *
 *	data - in, hold
 *		Data put to the pointer array.
 * 
 * Return value : 
 * 
 *      Index of the new element.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint su_pa_insert(su_pa_t* pa, void* data)
{
        CHK_PA(pa);
        ss_dassert(data != NULL);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        if (pa->pa_size == 0) {

            /* empty array */
            pa->pa_elems = SsMemAlloc(sizeof(su_pa_elem_t));
            pa->pa_size = 1;
            pa->pa_nelems = 1;
            pa->pa_elems[0] = data;
            return(0);
        }

        if (pa->pa_size == pa->pa_nelems) {

            /* no free places, increase array size */
            pa->pa_elems = SsMemRealloc(
                                pa->pa_elems,
                                sizeof(su_pa_elem_t) * (pa->pa_size + 1));
            pa->pa_size++;
            pa->pa_nelems++;
            pa->pa_elems[pa->pa_size - 1] = data;
            return(pa->pa_size - 1);

        } else {

            /* find free place and put data onto it */
            int i = -1;
            if (pa->pa_freerbt != NULL) {
                su_rbt_node_t* k;
                k = su_rbt_min(pa->pa_freerbt, NULL);
                if (k != NULL) {
                    i = (int)su_rbtnode_getkey(k);
                    ss_dassert(i < (int)pa->pa_size);
                    ss_dassert(pa->pa_elems[i] == NULL);
                    su_rbt_delete(pa->pa_freerbt, k);
                } else {
                    /* no free places, increase array size */
                    pa->pa_elems = SsMemRealloc(
                                        pa->pa_elems,
                                        sizeof(su_pa_elem_t) * (pa->pa_size + 1));
                    pa->pa_size++;
                    pa->pa_nelems++;
                    pa->pa_elems[pa->pa_size - 1] = data;
                    return(pa->pa_size - 1);
                }

            }
            if (i == -1) {
                for (i = 0; i < (int)pa->pa_size; i++) {
                    if (pa->pa_elems[i] == NULL) {
                        break;
                    }
                }
            }
            ss_dassert(i < (int)pa->pa_size);
            pa->pa_elems[i] = data;
            pa->pa_nelems++;
            return(i);
        }
}

/*##**********************************************************************\
 * 
 *		su_pa_insertat
 * 
 * Inserts a new element to the pointer array at the given index.
 * Assumes that the index is not already in use.
 * 
 * Parameters : 
 * 
 *	pa - in out, use
 *		Pointer array pointer.
 *
 *	index - in
 *		Requested index inside the pointer array.
 *
 *	data - in, hold
 *		Data put to the pointer array.
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_pa_insertat(su_pa_t* pa, uint index, void* data)
{
        uint i;

        CHK_PA(pa);
        ss_dassert(data != NULL);
        ss_dassert(!su_pa_indexinuse(pa, index));
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        if (pa->pa_size == 0) {

            /* empty array */

            pa->pa_size = index + 1;
            pa->pa_elems = SsMemAlloc(pa->pa_size * sizeof(su_pa_elem_t));
            pa->pa_nelems = 1;
            pa->pa_elems[index] = data;
            /* initialize empty indices */
            for (i = 0; i < index; i++) {
                pa->pa_elems[i] = NULL;
            }
            if (pa->pa_freerbt != NULL) {
                for (i = 0; i < index; i++) {
                    su_rbt_insert(pa->pa_freerbt, (void*)i);
                }
            }
            return;
        }

        if (pa->pa_size <= index) {

            /* index is beyond the last item, increase array size */

            uint old_size = pa->pa_size;
            uint i;

            pa->pa_size = index + 1;
            pa->pa_elems = SsMemRealloc(
                                pa->pa_elems,
                                sizeof(su_pa_elem_t) * pa->pa_size);
            pa->pa_nelems++;
            pa->pa_elems[index] = data;
            /* initialize empty indices */
            for (i = old_size; i < index; i++) {
                pa->pa_elems[i] = NULL;
            }
            if (pa->pa_freerbt != NULL) {
                for (i = old_size; i < index; i++) {
                    su_rbt_insert(pa->pa_freerbt, (void*)i);
                }
            }
            return;

        } else {

            if (pa->pa_freerbt != NULL) {
                su_rbt_node_t* k;
                k = su_rbt_search(pa->pa_freerbt, (void*)index);
                if (k != NULL) {
                    su_rbt_delete(pa->pa_freerbt, k);
                } else {
                    /* Find index from pa->pa_freearray. */
                    for (i = 0; i < pa->pa_maxfreearray; i++) {
                        if (pa->pa_freearray[i] == index) {
                            pa->pa_freearray[i] = -1;
                            break;
                        }
                    }
                    ss_dassert(i < pa->pa_maxfreearray);
                }
            }

            /* We have already dasserted that the place is free.
               Just put data onto it */

            pa->pa_elems[index] = data;
            pa->pa_nelems++;
            return;
        }
}

/*##**********************************************************************\
 * 
 *		su_pa_remove
 * 
 * Removes element at index from the pointer array.
 * 
 * Parameters : 
 * 
 *	pa - in out, use
 *		Pointer array pointer.
 *
 *	index - in
 *		index of the element to be removed
 *
 * Return value - give : 
 * 
 *      User data at the removed element.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void* su_pa_remove(su_pa_t* pa, uint index)
{
        void* data;

        CHK_PA(pa);
        ss_dassert(index < pa->pa_size);
        ss_dassert(pa->pa_elems[index] != NULL);
        ss_dassert(pa->pa_nelems > 0);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        data = pa->pa_elems[index];

        pa->pa_elems[index] = NULL;
        pa->pa_nelems--;

        if (pa->pa_maxfreearray > 0) {
            if ((int)(pa->pa_freearray[pa->pa_curfreearray]) != -1) {
                su_rbt_insert(pa->pa_freerbt, (void*)pa->pa_freearray[pa->pa_curfreearray]);
            }
            pa->pa_freearray[pa->pa_curfreearray] = index;
            pa->pa_curfreearray++;
            if (pa->pa_curfreearray == pa->pa_maxfreearray) {
                pa->pa_curfreearray = 0;
            }
        }

        return(data);
}

/*##**********************************************************************\
 * 
 *		su_pa_getnext
 * 
 * Returns the next existing element after *p_index in pa.
 * "Wraps" around in the end of array. -1 gives the first element.
 * If there is only one item, it is returned.
 * 
 * Parameters : 
 * 
 *	pa - in, use
 *		Pointer array pointer.
 *		
 *	p_index - in out, use
 *		index in pointer array where to start scanning.
 *		On return, contains the index of the returned element.
 *		
 * Return value : 
 * 
 *      Pointer to data
 *      NULL, if not found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void* su_pa_getnext(
        su_pa_t* pa,
        uint*    p_index
) {
        uint i;
        
        CHK_PA(pa);
        ss_dassert(p_index != NULL);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        i = *p_index;
        if (pa->pa_nelems == 0) {
            return(NULL);
        }
        for (i >= pa->pa_size ? i = 0 : i++;
             ;
             i == pa->pa_size ? i = 0 : i++) {

            if (_SU_PA_INDEXINUSE_(pa, i)) {
                (*p_index) = i;
                return (_SU_PA_GETDATA_(pa, i));
            }
        }
        ss_error;
        return(NULL);
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *		su_pa_getdata
 * 
 * Returns user data at given index.
 * 
 * Parameters : 
 * 
 *	pa - in, use
 *		Pointer array pointer.
 *
 *	index - in
 *		index in pointer array
 * 
 * Return value - ref : 
 * 
 *      User data at index.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void* su_pa_getdata(su_pa_t* pa, uint index)
{
        CHK_PA(pa);
        ss_dassert(index < pa->pa_size);
        ss_dassert(pa->pa_elems[index] != NULL);
        ss_dassert(pa->pa_nelems > 0);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        return(_SU_PA_GETDATA_(pa, index));
}

/*##**********************************************************************\
 * 
 *		su_pa_indexinuse
 * 
 * Checks if a given index in in use.
 * 
 * Parameters : 
 * 
 *	pa - in, use
 *		Pointer array pointer.
 *
 *	index - in
 *		index that is checked
 * 
 * Return value : 
 * 
 *      TRUE    - index is in use
 *      FALSE   - index is not in use
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool su_pa_indexinuse(su_pa_t* pa, uint index)
{
        CHK_PA(pa);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        return(_SU_PA_INDEXINUSE_(pa, index));
}

/*##**********************************************************************\
 * 
 *		su_pa_nelems
 * 
 * Returns the number of elements in the pointer array.
 * 
 * Parameters : 
 * 
 *	pa - in, use
 *		Pointer array pointer.
 *
 * Return value : 
 * 
 *      Number of elements in pa.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint su_pa_nelems(su_pa_t* pa)
{
        CHK_PA(pa);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        return(pa->pa_nelems);
}

/*##**********************************************************************\
 * 
 *		su_pa_realsize
 * 
 * Get allocated size of the pointer array
 * 
 * Parameters : 
 * 
 *	pa - in, use
 *		Pointer array pointer.
 *
 * Return value : 
 * 
 *      Real allocated size of the pointer array (NULL slots included)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint su_pa_realsize(su_pa_t* pa)
{
        CHK_PA(pa);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        return(pa->pa_size);
}


#endif /* SS_DEBUG */

bool su_pa_compress(su_pa_t* pa)
{
        uint i;
        uint j;
        bool moves = FALSE;

        CHK_PA(pa);
        ss_dassert(pa->pa_maxfreearray == 0);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        if (pa->pa_nelems == pa->pa_size) {
            return (moves);
        }
        if (pa->pa_nelems == 0) {
            SsMemFree(pa->pa_elems);
            pa->pa_elems = NULL;
            pa->pa_size = 0;
            return (moves);
        }
        for (j = i = 0; i < pa->pa_size; i++) {
            if (pa->pa_elems[i] != NULL) {
                if (j != i) {
                    moves = TRUE;
                }
                pa->pa_elems[j++] = pa->pa_elems[i];
            }
        }
        pa->pa_size = pa->pa_nelems = j;
        pa->pa_elems = SsMemRealloc(pa->pa_elems, pa->pa_size * sizeof(void*));
        return (moves);
}

#ifdef SS_DEBUG
void su_pa_setsemnum(
        su_pa_t* pa,
        int semnum)
{
        CHK_PA(pa);

        pa->pa_semnum = semnum;
}
#endif /* SS_DEBUG */

#ifdef AUTOTEST_RUN
void su_pa_setmaxlen(su_pa_t* pa, long maxlen)
{
        pa->pa_maxlen = maxlen;
}
#endif
