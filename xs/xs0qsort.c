/*************************************************************************\
**  source       * xs0qsort.c
**  directory    * xs
**  description  * Qsort for presorting of solid sort utility
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


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include "xs0qsort.h"

/*
 * MTHRESH is the smallest partition for which we compare for a median
 * value instead of using the middle value.
 */
#define	MTHRESH	6

/*
 * THRESH is the minimum number of entries in a partition for continued
 * partitioning.
 */
#define	THRESH	4

/*
 * Swap two areas of size number of bytes.  Although qsort(3) permits random
 * blocks of memory to be sorted, sorting pointers is almost certainly the
 * common case (and, were it not, could easily be made so).  Regardless, it
 * isn't worth optimizing; the SWAP's get sped up by the cache, and pointer
 * arithmetic gets lost in the time required for comparison function calls.
 */
#define	SWAP(a, b) { \
	cnt = size; \
	do { \
		ch = *a; \
		*a++ = *b; \
		*b++ = ch; \
	} while (--cnt); \
}

/*
 * Knuth, Vol. 3, page 116, Algorithm Q, step b, argues that a single pass
 * of straight insertion sort after partitioning is complete is better than
 * sorting each small partition as it is created.  This isn't correct in this
 * implementation because comparisons require at least one (and often two)
 * function calls and are likely to be the dominating expense of the sort.
 * Doing a final insertion sort does more comparisons than are necessary
 * because it compares the "edges" and medians of the partitions which are
 * known to be already sorted.
 *
 * This is also the reasoning behind selecting a small THRESH value (see
 * Knuth, page 122, equation 26), since the quicksort algorithm does less
 * comparisons than the insertion sort.
 */
#define	SORT(bot, n, context) { \
	if (n > 1) {\
		if (n == 2) { \
			t1 = bot + size; \
			if (compar(t1, bot, context) < 0) {\
				SWAP(t1, bot); \
            }\
		} else {\
			insertion_sort(bot, n, size, compar, context); \
        }\
    }\
}

/*#***********************************************************************\
 * 
 *		insertion_sort
 * 
 * Insertion sort for internal use, see xs_qsort() comment below
 * 
 * Parameters : 
 * 
 *	bot - in out, use
 *		
 *		
 *	nmemb - in
 *		
 *		
 *	size - in
 *		
 *		
 *	compar - in, use
 *		
 *		
 *	context - in, use
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
static void insertion_sort(
        char *bot,
        size_t nmemb,
        size_t size,
        xs_qcomparefp_t compar,
        void* context)
{
        int cnt;
        uchar ch;
        char *s1, *s2, *t1, *t2, *top;

        /*
         * A simple insertion sort (see Knuth, Vol. 3, page 81, Algorithm
         * S).  Insertion sort has the same worst case as most simple sorts
         * (O N^2).  It gets used here because it is (O N) in the case of
         * sorted data.
         */
        top = bot + nmemb * size;
        for (t1 = bot + size; t1 < top;) {
            for (t2 = t1; (t2 -= size) >= bot && compar(t1, t2, context) < 0;)
                ;
            if (t1 != (t2 += size)) {
                /* Bubble bytes up through each element. */
                for (cnt = size; cnt--; ++t1) {
                    ch = *t1;
                    for (s1 = s2 = t1; (s2 -= size) >= t2; s1 = s2)
                        *s1 = *s2;
                    *s1 = ch;
                }
            } else
                t1 += size;
        }
}

/*#***********************************************************************\
 * 
 *		quick_sort
 * 
 * Internally used quicksort, see xs_qsort() comment below
 * 
 * Parameters : 
 * 
 *	bot - in out, use
 *		
 *		
 *	nmemb - in
 *		
 *		
 *	size - in
 *		
 *		
 *	compar - in, use
 *		
 *		
 *	context - in, use
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
static void quick_sort(
        char *bot,
        size_t nmemb,
        size_t size,
        xs_qcomparefp_t compar,
        void* context)
{
        int cnt;
        uchar ch;
        char *top, *mid, *t1, *t2;
        int n1, n2;
        char *bsv;

        /* bot and nmemb must already be set. */
partition:

        /* find mid and top elements */
        mid = bot + size * (nmemb >> 1);
        top = bot + (nmemb - 1) * size;

        /*
         * Find the median of the first, last and middle element (see Knuth,
         * Vol. 3, page 123, Eq. 28).  This test order gets the equalities
         * right.
         */
        if (nmemb >= MTHRESH) {
            n1 = compar(bot, mid, context);
            n2 = compar(mid, top, context);
            if (n1 < 0 && n2 > 0)
                t1 = compar(bot, top, context) < 0 ? top : bot;
            else if (n1 > 0 && n2 < 0)
                t1 = compar(bot, top, context) > 0 ? top : bot;
            else
                t1 = mid;

            /* if mid element not selected, swap selection there */
            if (t1 != mid) {
                SWAP(t1, mid);
                mid -= size;
            }
        }

        /* Standard quicksort, Knuth, Vol. 3, page 116, Algorithm Q. */
#define	didswap	n1
#define	newbot	t1
#define	replace	t2
        didswap = 0;
        for (bsv = bot;;) {
            for (; bot < mid && compar(bot, mid, context) <= 0; bot += size)
                ;
            while (top > mid) {
                if (compar(mid, top, context) <= 0) {
                    top -= size;
                    continue;
                }
                newbot = bot + size;	/* value of bot after swap */
                if (bot == mid)		/* top <-> mid, mid == top */
                    replace = mid = top;
                else {			/* bot <-> top */
                    replace = top;
                    top -= size;
                }
                goto swap;
            }
            if (bot == mid)
                break;

            /* bot <-> mid, mid == bot */
            replace = mid;
            newbot = mid = bot;		/* value of bot after swap */
            top -= size;

swap:
            SWAP(bot, replace);
            bot = newbot;
            didswap = 1;
        }

        /*
         * Quicksort behaves badly in the presence of data which is already
         * sorted (see Knuth, Vol. 3, page 119) going from O N lg N to O N^2.
         * To avoid this worst case behavior, if a re-partitioning occurs
         * without swapping any elements, it is not further partitioned and
         * is insert sorted.  This wins big with almost sorted data sets and
         * only loses if the data set is very strangely partitioned.  A fix
         * for those data sets would be to return prematurely if the insertion
         * sort routine is forced to make an excessive number of swaps, and
         * continue the partitioning.
         */
        if (!didswap) {
            insertion_sort(bsv, nmemb, size, compar, context);
            return;
        }

        /*
         * Re-partition or sort as necessary.  Note that the mid element
         * itself is correctly positioned and can be ignored.
         */
#define	nlower	n1
#define	nupper	n2
        bot = bsv;
        nlower = (mid - bot) / size;	/* size of lower partition */
        mid += size;
        nupper = nmemb - nlower - 1;	/* size of upper partition */
        
        /*
         * If must call recursively, do it on the smaller partition; this
         * bounds the stack to lg N entries.
         */
        if (nlower > nupper) {
            if (nupper >= THRESH) {
                quick_sort(mid, nupper, size, compar, context);
            } else {
                SORT(mid, nupper, context);
                if (nlower < THRESH) {
                    SORT(bot, nlower, context);
                    return;
                }
            }
            nmemb = nlower;
        } else {
            if (nlower >= THRESH) {
                quick_sort(bot, nlower, size, compar, context);
            } else {
                SORT(bot, nlower, context);
                if (nupper < THRESH) {
                    SORT(mid, nupper, context);
                    return;
                }
            }
            bot = mid;
            nmemb = nupper;
        }
        goto partition;
        /* NOTREACHED */
}

/*##**********************************************************************\
 * 
 *		xs_qsort
 * 
 * Like library routine qsort but takes one more parameter, i.e. context
 * which is also supplied to the compare function as the 3rd parameter.
 * 
 * Parameters : 
 * 
 *	bot - in out, use
 *		pointer to array to be sorted
 *		
 *	nmemb - in
 *		number of array elements
 *		
 *	size - in
 *		array element size
 *		
 *	compar - in, use
 *		comparison function just like that of qsort(), but this one takes
 *          a third parameter, a pointer to a pointer to a 'context'
 *		
 *	context - in, use
 *		context pointer
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_qsort(
        void* bot,
        size_t nmemb,
        size_t size,
        xs_qcomparefp_t compar,
        void* context)
{
        if (nmemb <= 1) {
            return;
        }
        if (nmemb >= THRESH) {
            quick_sort(bot, nmemb, size, compar, context);
        } else {
            insertion_sort(bot, nmemb, size, compar, context);
        }
}

/*##**********************************************************************\
 * 
 *		xs_qsort_context
 * 
 * The qsort_context function replaces the qsort function adding the client
 * data pointer as the context parameter.
 *
 *   Parameters:
 *
 *    cd - 
 *          application state
 *
 *    base - 
 *          base of the sorting area
 *
 *    item_c - 
 *          number of the items in the sort area
 *
 *    size - 
 *          size of a sorting item in bytes
 *
 *    cfun - 
 *          comparing function
 *
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_qsort_context(
	void* cd,
	void* base,
	size_t item_c,
        size_t size,
	xs_qcomparefp_t compar)
{
        xs_qsort(
	    base,
	    item_c,
            size,
	    compar,
            cd);
}
