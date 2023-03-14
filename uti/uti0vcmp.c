/*************************************************************************\
**  source       * uti0vcmp.c
**  directory    * uti
**  description  * v-attribute and v-tuple comparisons
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

See nstdyn.doc.

Limitations:
-----------

None.

Error handling:
--------------

None.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

None.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define UTI0VCMP_C

#include <ssstring.h>
#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include "uti0va.h"
#include "uti1val.h"
#include "uti0vtpl.h"
#include "uti0vcmp.h"


/* static function prototypes ******************************/

#ifndef NO_ANSI
static vtpl_index_t uti_compress(void* target_vtpl, vtpl_t* prev_vtpl,
                             vtpl_t* next_vtpl, bool f_dyn);
static void make_compressed(void* target_vtpl, bool f_dyn, va_index_t l_mismatch,
                            ss_byte_t* p_next_end, ss_byte_t* p_next_data);
static void expand_grow( vtpl_expand_state* es, vtpl_index_t l_needed );
#else
static vtpl_index_t uti_compress();
#endif


/* functions ***********************************************/

/*##**********************************************************************\
 *
 *		va_compare
 *
 * Compare two v-attributes according to lexicographical order; return value
 * like memcmp.
 *
 * Parameters :
 *
 *	va1, va2 - in, use
 *          the v-attributes to compare
 *
 * Return value :
 *
 *      < 0, if va1 < va2
 *      = 0, if va1 = va2
 *      > 0, if va1 > va2
 *
 * Limitations  :
 *
 * Globals used :
 */
int va_compare(va1, va2)
	register va_t* va1;
	register va_t* va2;
{
        va_index_t len1, len2, len;
        register ss_byte_t* data1;
        register ss_byte_t* data2;
        register int comp_res = 0;

        /* find length and data area */
        VA_GETDATA(data1, va1, len1);
        VA_GETDATA(data2, va2, len2);
        len = SS_MIN(len1, len2);

        comp_res = SsMemcmp(data1, data2, len);

        /* If otherwise equal, the shorter one precedes the longer */
        if (comp_res == 0) return(len1 - len2);
        else return(comp_res);
}

#ifdef SS_UNICODE_DATA

/*##**********************************************************************\
 *
 *		va_compare_char1v2
 *
 * Compares va1 containing 8-bit ascii data against va2
 * containing 16-bit unicode data.
 *
 * Parameters :
 *
 *	va1 - in, use
 *		pointer to 1st v-attribute
 *
 *	va2 - in, use
 *		pointer to 2nd v-attribute
 *
 * Return value :
 *      < 0, if va1 < va2
 *      = 0, if va1 = va2
 *      > 0, if va1 > va2
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int va_compare_char1v2(va_t* va1, va_t* va2)
{
        va_index_t len1, len2, len;
        ss_byte_t* data1;
        ss_char2_t* data2;

        data1 = (ss_byte_t*)va_getdata(va1, &len1);
        data2 = (ss_char2_t*)va_getdata(va2, &len2);
        if (len1 == 0 || len2 == 0) {
            /* either one is null */
            return (len1 - len2);
        }
        ss_dassert(len2 & 1);
        len2 /= 2;
        len1--;
        len = SS_MIN(len1, len2);
        while (len) {
            len--;
            if (sizeof(ss_char2_t) < sizeof(int)) {
                int comp;

                comp = (int)*data1 - (int)SS_CHAR2_LOAD(data2);
                if (comp != 0) {
                    return (comp);
                }
            } else {
                ss_char2_t c1;
                ss_char2_t c2;
                c1 = *data1;
                c2 = SS_CHAR2_LOAD(data2);
                if (c1 != c2) {
                    if (c1 < c2) {
                        return (-1);
                    }
                    return (1);
                }
            }
            data1++;
            data2++;
        }
        return (len1 - len2);
}

/*##**********************************************************************\
 *
 *		va_compare_char2v1
 *
 * Mirror case of va_compare_char1v2
 *
 * Parameters :
 *
 *	va1 - in, use
 *		pointer to 1st v-attribute
 *
 *	va2 - in, use
 *		pointer to 2nd v-attribute
 *
 * Return value :
 *      < 0, if va1 < va2
 *      = 0, if va1 = va2
 *      > 0, if va1 > va2
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int va_compare_char2v1(va_t* va1, va_t* va2)
{
        int comp;

        comp = va_compare_char1v2(va2, va1);
        return (-comp);
}

#endif /* SS_UNICODE_DATA */

/*##**********************************************************************\
 *
 *		vtpl_compare
 *
 * Compares two v-tuples.
 *
 * Parameters :
 *
 *	vtpl1 - in, use
 *		a pointer to a v-tuple
 *
 *	vtpl2 - in, use
 *		a pointer to a v-tuple
 *
 * Return value :
 *
 *      < 0, if vtpl1 < vtpl2
 *      = 0, if vtpl1 = vtpl2
 *      > 0, if vtpl1 > vtpl2
 *
 * Limitations  :
 *
 *      doesn't work with empty v-tuples, using the search
 *      functions causes some overhead
 *
 * Globals used :
 */
int vtpl_compare(vtpl1, vtpl2)
	vtpl_t* vtpl1;
	vtpl_t* vtpl2;
{
        char* p_field1;         /* pointer for mapping over the fields */
        char* p_field2;         /* pointer for mapping over the fields */
        char* vtpl_end1;        /* vtpl + gross len - 1 */
        char* vtpl_end2;        /* vtpl + gross len - 1 */
        vtpl_index_t vtpl_len1; /* net length */
        vtpl_index_t vtpl_len2; /* net length */

        if (BYTE_LEN_TEST(vtpl_len1 = vtpl1->c[0])) {
            p_field1 = (char*)vtpl1 + 1;
            vtpl_end1 = (char*)vtpl1 + vtpl_len1;
        } else {
            vtpl_len1 = FOUR_BYTE_LEN(vtpl1);
            p_field1 = (char*)vtpl1 + 5;
            vtpl_end1 = (char*)vtpl1 + vtpl_len1 + 4;
        }
        ss_dassert(vtpl_len1 > 0);
        if (BYTE_LEN_TEST(vtpl_len2 = vtpl2->c[0])) {
            p_field2 = (char*)vtpl2 + 1;
            vtpl_end2 = (char*)vtpl2 + vtpl_len2;
        } else {
            vtpl_len2 = FOUR_BYTE_LEN(vtpl2);
            p_field2 = (char*)vtpl2 + 5;
            vtpl_end2 = (char*)vtpl2 + vtpl_len2 + 4;
        }
        ss_dassert(vtpl_len2 > 0);

        if (vtpl_len1 <= 1 || vtpl_len2 <= 1) {
            /* One of vtpl values is VTPL_NULL. */
            return(vtpl_len1 - vtpl_len2);
        }

        /* Loop over fields. We need to test both field lengths because
         * 'alter table add column' may have resulted different number
         * of physical fields.
         */
        while (p_field1 <= vtpl_end1 && p_field2 <= vtpl_end2) {
            /* Compare va values. */
            va_index_t len1, len2, len;
            ss_byte_t* data1;
            ss_byte_t* data2;
            int comp_res;

            /* find length and data area */
            VA_GETDATA(data1, (va_t*)p_field1, len1);
            VA_GETDATA(data2, (va_t*)p_field2, len2);
            len = SS_MIN(len1, len2);

            comp_res = SsMemcmp(data1, data2, len);

            if (comp_res == 0) {
                if (len1 != len2) {
                    /* If otherwise equal, the shorter one precedes
                     * the longer.
                     */
                    return(len1 - len2);
                }
            } else {
                return(comp_res);
            }

            /* Go to next field. */
            p_field1 = (char *)(data1 + len1);
            p_field2 = (char *)(data2 + len2);
        }

        /* Check if there are fields at the end of one vtpl.
         */
        if (p_field1 <= vtpl_end1) {
            ss_dassert(p_field2 > vtpl_end2);
            return(1);
        } else if (p_field2 <= vtpl_end2) {
            ss_dassert(p_field1 > vtpl_end1);
            return(-1);
        } else {
            ss_dassert(p_field1 > vtpl_end1 && p_field2 > vtpl_end2);
            return(0);
        }
}

/*##**********************************************************************\
 *
 *		vtpl_equal
 *
 * Checks if two v-tuples are equal.
 *
 * Parameters :
 *
 *	vtpl1 - in, use
 *		a pointer to a v-tuple
 *
 *	vtpl2 - in, use
 *		a pointer to a v-tuple
 *
 * Return value :
 *
 *      TRUE    if vtpl1 = vtpl2
 *      FALSE   if vtpl1 != vtpl2
 *
 * Limitations  :
 *
 * Globals used :
 */
bool vtpl_equal(vtpl1, vtpl2)
	vtpl_t* vtpl1;
	vtpl_t* vtpl2;
{
        vtpl_index_t len1;
        vtpl_index_t len2;

        len1 = VTPL_GROSSLEN(vtpl1);
        len2 = VTPL_GROSSLEN(vtpl2);

        if (len1 != len2) {
            return(FALSE);
        } else {
            return(SsMemcmp(vtpl1, vtpl2, len1) == 0);
        }
}


/*##**********************************************************************\
 *
 *		vtpl_search_step_fn
 *
 * Perform a single step in an index search, comparing the key indicated
 * by the search state argument with an index entry.  This is only called
 * for index entries whose i_mismatch = p_search_state->i_mismatch.
 * The members of the search_state are updated after every comparison if
 * the index entry was smaller than the search key. The only exception to that
 * is i_ob1_mismatch (index of off-by-one mismatch), which records the number
 * of bytes the search key and the smallest index entry larger than the
 * search key have in common.
 *
 * Parameters :
 *
 *	p_search_state - in out, use
 *		pointer to the search state structure, the search state
 *          structure is updated, if the search did not end
 *
 *	index_entry - in, use
 *		pointer to the index entry (a v-tuple)
 *
 *      index_data - in, use
 *          data used for additional comparison if v-tuples are
 *          equal, used only if special comparision function
 *          is specified for search state
 *
 * Return value :
 *
 *      < 0, if key < index_entry
 *      = 0, if key = index_entry
 *      > 0, if key > index_entry
 *
 * Limitations  :
 *
 *      assumes a search in the ascending direction
 *
 *      NOTE! THIS FUNCTION IS IMPLEMENTED AS MACRO IN UTI0VCMP.H FOR
 *            PRODUCT COMPILATION.
 *
 * Globals used :
 */
int vtpl_search_step_fn(p_search_state, index_entry, index_data)
	search_state* p_search_state;
	vtpl_t* index_entry;
        void* index_data;
{
        search_state ss;
        vtpl_index_t l_ie;
        register va_index_t i_cmp;
        va_index_t l_cmp, l_ie_field;
        ss_byte_t* p_ie_data;
        ss_byte_t* p_ie_end;
        ss_byte_t* p_ie_start;
        register int comp_res;

        /* Find the end and the first field of the index entry */
        VTPL_GETDATA(p_ie_start, index_entry, l_ie);
        ss_dassert(l_ie != 0);

        /* Does simple test for the va's: comparing the length and
         * the content as a whole. Not applicable for the mmeg2 where the
         * order is not the only point of interest because the length of the
         * common prefix with the next vtpl is needed too.
         */
        VA_GETDATA(p_ie_data, (va_t*)p_ie_start, l_ie_field);
        ss = *p_search_state;

        i_cmp=0; /* Haven't compared anything yet. */
        l_cmp = SS_MIN(ss.l_field, l_ie_field);

        if (!ss.i_ob1_inuse) { /* If not mmeg2 in case */
            /* Is the length of the tails equal ? */
            if (l_cmp != 0) /* no null fields */ {
                /* Is the content of the tails equal? */
                if ((comp_res = ZERO_EXTEND_TO_INT(*ss.p_mismatch) -
                                ZERO_EXTEND_TO_INT(*p_ie_data)) != 0) {
                    return(comp_res);
                } else {
                    i_cmp = 1; /* Have compared one byte, step loop vars */
                    ss.p_mismatch++; p_ie_data++;
                }
            }
        }

        /* Set up the rest */
        p_ie_end = p_ie_start + l_ie;
        ss.i_mismatch = (*p_search_state).i_mismatch;
        ss.p_key_end = (*p_search_state).p_key_end;

        for (;;) { /* Loop over fields */
            /* find mismatch in this field */
            for (; i_cmp < l_cmp;
                 ss.p_mismatch++, i_cmp++, p_ie_data++) {
                comp_res = ZERO_EXTEND_TO_INT(*ss.p_mismatch) -
                           ZERO_EXTEND_TO_INT(*p_ie_data);
                if (comp_res > 0) { /* *ss.p_mismatch < *p_ie_data */
                    (*p_search_state).i_mismatch = ss.i_mismatch + i_cmp;
                    (*p_search_state).l_field = ss.l_field - i_cmp;
                    (*p_search_state).p_mismatch = ss.p_mismatch;
                    return(1);
                } else if (comp_res < 0) {
                    (*p_search_state).i_ob1_mismatch = ss.i_mismatch + i_cmp;
                    return(-1);
                    /* The search ends, no need to update *p_search_state */
                }
            }

            /* The data bytes are identical upto l_cmp, compare lengths:
               the shorter field precedes the longer */
            /* After this, i_cmp equals l_cmp, so we use it instead of
               l_ie_field or ss.l_field, because it is a register var */
            if (ss.l_field > i_cmp) { /* ss.l_field > l_ie_field */
                ss.i_mismatch += i_cmp; ss.l_field -= i_cmp;
                *p_search_state = ss;
                return( 1);
            } else if (i_cmp == l_ie_field) { /* ss.l_field == l_ie_field */
                /* do next field */
                if (ss.p_key_end <= ss.p_mismatch) { /* the key ended */
                    if (p_ie_end <= p_ie_data) /* ie end? */ {
                        int fn_cmp;
                        if (p_search_state->cmp_fn != NULL) {
                            fn_cmp = (*p_search_state->cmp_fn)(
                                                p_search_state->key_data,
                                                index_data);
                        } else {
                            fn_cmp = 0;
                        }
                        if (fn_cmp > 0) {
                            (*p_search_state).i_mismatch = ss.i_mismatch + i_cmp;
                            (*p_search_state).l_field = ss.l_field - i_cmp;
                            (*p_search_state).p_mismatch = ss.p_mismatch;
                        } else {
                            /* if fn_cmp <= 0, the search ends, no need to
                             * update *p_search_state except the off-by-1
                             * mismatch. */
                            (*p_search_state).i_ob1_mismatch = ss.i_mismatch + i_cmp;
                        }
                        return(fn_cmp);
                    } else {
                        return(-1);
                        /* The search ends, no need to update *p_search_state */
                    }
                } else { /* the key didn't end yet */
                    /* update state */
                    ss.i_mismatch += i_cmp + 1;
                    VA_GETDATA(ss.p_mismatch, (va_t*)ss.p_mismatch, ss.l_field);

                    if (p_ie_end <= p_ie_data) /* ie end? */ {
                        *p_search_state = ss;
                        return( 1);
                    }
                    /* This is the only branch back to the for loop! */
                }
            } else { /* ss.l_field < l_ie_field */
                /* The search ends, no need to update *p_search_state
                 * update *p_search_state except the off-by-1
                 * mismatch. */
                (*p_search_state).i_ob1_mismatch = ss.i_mismatch + i_cmp;
                return(-1);
            }

            /* Setup for next loop */
            VA_GETDATA(p_ie_data, (va_t*)p_ie_data, l_ie_field);
            l_cmp = SS_MIN(ss.l_field, l_ie_field); i_cmp = 0;
        }
}

/*##**********************************************************************\
 *
 *		vtpl_compress
 *
 * Compress a v-tuple into an index entry with respect to the previous entry.
 *
 * Parameters :
 *
 *	target_vtpl - out, use
 *		a pointer to a v-tuple to write the result to
 *
 *	prev_vtpl - in, use
 *		a pointer to the v-tuple that is the previous entry
 *
 *	next_vtpl - in, use
 *		a pointer to the v-tuple to be compressed
 *
 * Return value :
 *
 *      the i-mismatch index of the first mismatched byte
 *
 * Limitations  :
 *
 *      doesn't work with empty v-tuples,
 *      assumes *prev_vtpl <= *next_vtpl
 *
 * Globals used :
 */
vtpl_index_t vtpl_compress(target_vtpl, prev_vtpl, next_vtpl)
	vtpl_t* target_vtpl;
	vtpl_t* prev_vtpl;
	vtpl_t* next_vtpl;
{
        return(uti_compress(target_vtpl, prev_vtpl, next_vtpl, FALSE));
}


/*##**********************************************************************\
 *
 *		dynvtpl_compress
 *
 * Compress a v-tuple into an index entry with respect to the previous entry,
 * using a dynamic result buffer.
 *
 * Parameters :
 *
 *	p_target_vtpl - in out, give
 *		a pointer to a dynvtpl variable to write to
 *
 *	prev_vtpl - in, use
 *		a pointer to the v-tuple that is the previous entry
 *
 *	next_vtpl - in, use
 *		a pointer to the v-tuple to be compressed
 *
 * Return value :
 *
 *      the i-mismatch index of the first mismatched byte
 *
 * Limitations  :
 *
 *      doesn't work with empty v-tuples,
 *      assumes *prev_vtpl <= *next_vtpl
 *
 * Globals used :
 */
vtpl_index_t dynvtpl_compress(p_target_vtpl, prev_vtpl, next_vtpl)
	dynvtpl_t* p_target_vtpl;
	vtpl_t* prev_vtpl;
	vtpl_t* next_vtpl;
{
        return(uti_compress(p_target_vtpl, prev_vtpl, next_vtpl, TRUE));
}


/*##**********************************************************************\
 *
 *		vtpl_search_compress
 *
 * Compress a v-tuple used as a key in a search into an index entry with
 * respect to the last entry smaller than it.
 *
 * Parameters :
 *
 *	target_vtpl - out, use
 *		a pointer to a v-tuple to write the result to
 *
 *	cs - in, use
 *		a pointer to a search_state object
 *
 * Return value :
 *
 *      the i-mismatch index of the first mismatched byte
 *
 * Limitations  :
 *
 *      search must have encountered at least one smaller entry
 *
 * Globals used :
 */
vtpl_index_t vtpl_search_compress(target_vtpl, cs)
	vtpl_t* target_vtpl;
        register search_state* cs;
{
        make_compressed(target_vtpl, FALSE, cs->l_field,
                        cs->p_mismatch, cs->p_key_end);
        return(cs->i_mismatch);
}


/*##**********************************************************************\
 *
 *		dynvtpl_search_compress
 *
 * Compress a v-tuple used as a key in a search into an index entry with
 * respect to the last entry smaller than it, using a dynamic result buffer.
 *
 * Parameters :
 *
 *	p_target_vtpl - in out, give
 *		a pointer to a dynvtpl variable to write to
 *
 *	cs - in, use
 *		a pointer to a search_state obejct
 *
 * Return value :
 *
 *      the i-mismatch index of the first mismatched byte
 *
 * Limitations  :
 *
 *      search must have encountered at least one smaller entry
 *
 * Globals used :
 */
vtpl_index_t dynvtpl_search_compress(p_target_vtpl, cs)
	dynvtpl_t* p_target_vtpl;
        register search_state* cs;
{
        make_compressed(p_target_vtpl, TRUE, cs->l_field,
                        cs->p_mismatch, cs->p_key_end);
        return(cs->i_mismatch);
}


/*#***********************************************************************\
 *
 *		uti_compress
 *
 * Compresses v-tuple.
 *
 * Parameters :
 *
 *	target_vtpl - in out, use
 *		target v-tuple
 *
 *	prev_vtpl - in, use
 *		previous v-tuple
 *
 *	next_vtpl - in, use
 *		next v-tuple
 *
 *	f_dyn - in
 *		if TRUE, target v-tuple is allocated dynamically
 *
 * Return value :
 *
 *      the i-mismatch index of the first mismatched byte
 *
 * Limitations  :
 *
 * Globals used :
 */
static vtpl_index_t uti_compress(target_vtpl, prev_vtpl, next_vtpl, f_dyn)
	void* target_vtpl;
	vtpl_t* prev_vtpl;
	vtpl_t* next_vtpl;
        bool f_dyn;
{
        vtpl_index_t l_prev, l_next, i_mismatch = 0;
        va_index_t l_prev_field, l_next_field;
        register va_index_t i, len;
        ss_byte_t* p_prev_end;
        ss_byte_t* p_next_end;
        ss_byte_t* p_prev_data;
        ss_byte_t* p_next_data;

        ss_dassert(prev_vtpl != NULL && next_vtpl != NULL);

        /* find lengths */
        VA_GETDATA(p_prev_data, (va_t*)prev_vtpl, l_prev);
        VA_GETDATA(p_next_data, (va_t*)next_vtpl, l_next);
        ss_dassert(l_prev != 0);
        ss_dassert(l_next != 0);
        p_prev_end = p_prev_data + l_prev;
        p_next_end = p_next_data + l_next;

        /* find point of mismatch */
        VA_GETDATA(p_next_data, (va_t*)p_next_data, l_next_field);
        for (;;) { /* loop over fields */
            VA_GETDATA(p_prev_data, (va_t*)p_prev_data, l_prev_field);
            for (i = 0, len = SS_MIN (l_prev_field, l_next_field);
                 i < len;
                 p_prev_data++, i++, p_next_data++) {
                if (*p_prev_data != *p_next_data) {
                    break;
                }
            }
            i_mismatch += i;
            if (i < len || l_prev_field != l_next_field
                || p_next_data >= p_next_end) {
                break;
            }
            /* step to next field */
            i_mismatch++;
            i = 0;
            VA_GETDATA(p_next_data, (va_t*)p_next_data, l_next_field);
            if (p_prev_data >= p_prev_end) {
                break;
            }
        }

        make_compressed(target_vtpl, f_dyn, l_next_field - i,
                        p_next_data, p_next_end);

        return(i_mismatch);
}


/*#***********************************************************************\
 *
 *		make_compressed
 *
 * Makes a compressed v-tuple.
 *
 * Parameters :
 *
 *	target_vtpl - in out, use
 *		target v-tuple
 *
 *	f_dyn - in
 *		if TRUE, target v-tuple us allocated dynamically
 *
 *	l_mismatch - in
 *		mismatch index
 *
 *	p_next_data - in, use
 *		pointer to the next v-tuple data
 *
 *	p_next_end - in, use
 *		pointer to the end of next v-tuple data
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void make_compressed(target_vtpl, f_dyn, l_mismatch,
                            p_next_data, p_next_end)
	void* target_vtpl;
        bool f_dyn;
        va_index_t l_mismatch;
        ss_byte_t* p_next_data;
        ss_byte_t* p_next_end;
{
        register vtpl_index_t l_target =
            (vtpl_index_t)(VA_LEN_LEN(l_mismatch) + (p_next_end - p_next_data));
        register ss_byte_t* p_target_data;

        /* allocate target, if necessary */
        if (f_dyn) {
            dynvtpl_t* p_target_dynvtpl = (dynvtpl_t*)target_vtpl;
            vtpl_index_t len = VTPL_LEN_LEN(l_target) + l_target;

            if (*p_target_dynvtpl == NULL)
                 *p_target_dynvtpl = (dynvtpl_t)SsMemAlloc(len);
            else *p_target_dynvtpl = (dynvtpl_t)SsMemRealloc(*p_target_dynvtpl, len);
            p_target_data = (ss_byte_t*)*p_target_dynvtpl;
        } else {
            p_target_data = (ss_byte_t*)target_vtpl;
        }

        /* In what follows the things to be done are:
         * Find out the length of two fields:
         * 1. the length of vtuple length and
         * 2. the length of va length field.
         * When their lengths are known, copy then the compressed tuple
         * to its new location.
         * The length of the vtuple and the va is copied to the
         * beginning of the memory area of the compressed vtuple.
         */
        /* Both vtpl_lenlen and va_lenlen need 1 byte */
        if (BYTE_LEN_TEST(l_target))  {
            /* copy mismatched portion. */
            memmove( (p_target_data+2), p_next_data, p_next_end - p_next_data);
            p_target_data[0]=(ss_byte_t)l_target;
            p_target_data[1]=(ss_byte_t)l_mismatch;
        }
        /* vtpl_lenlen needs 1+4 bytes and va_lenlen needs 1 byte */
        else if (BYTE_LEN_TEST(l_mismatch)) {
            /* copy mismatched portion. */
            memmove( (p_target_data+6), p_next_data, p_next_end - p_next_data);
            /* copy the length information */
            SET_FOUR_BYTE_LEN((vtpl_t *)&p_target_data[0], l_target);
            p_target_data[5]=(ss_byte_t)l_mismatch;
        }
        /* Both vtpl_lenlen and va_lenlen need 1+4 bytes. */
        else {
            /* copy mismatched portion. */
            memmove( (p_target_data+10), p_next_data, p_next_end - p_next_data);
            /* copy the length information */
            SET_FOUR_BYTE_LEN((vtpl_t *)&p_target_data[0], l_target);
            SET_FOUR_BYTE_LEN((vtpl_t *)&p_target_data[5], l_mismatch);
        }
}




/*##**********************************************************************\
 *
 *		dynvtpl_find_split
 *
 * Given two index entries (the first uncompressed and the second compressed)
 * returns an (uncompressed) entry in a dynvtpl buffer that falls between
 * the two, and is as short as possible.
 *
 * Parameters :
 *
 *	p_target_vtpl - in out, give
 *		a pointer to a dynvtpl variable to write to a pointer to
 *          an uncompressed index entry, such that
 *          *prev_vtpl < **p_target_vtpl <= *compressed_vtpl
 *
 *	prev_vtpl - in, use
 *		a pointer to the previous index entry
 *
 *	compressed_vtpl - in, use
 *		a pointer to the compressed entry following prev_vtpl
 *
 *	i_mismatch - in, use
 *		the i-mismatch index of the first byte in compressed_vtpl
 *
 * Return value - ref :
 *
 *      the new value of *p_target_vtpl
 *
 * Limitations  :
 *
 *      doesn't work with empty v-tuples
 *
 * Globals used :
 */
vtpl_t* dynvtpl_find_split(p_target_vtpl, prev_vtpl, compressed_vtpl, i_mismatch)
	dynvtpl_t* p_target_vtpl;
	vtpl_t* prev_vtpl;
	vtpl_t* compressed_vtpl;
	vtpl_index_t i_mismatch;
{
        uti_expand(p_target_vtpl, prev_vtpl, compressed_vtpl, i_mismatch, TRUE, TRUE);
        return(*p_target_vtpl);
}


/*##**********************************************************************\
 *
 *        vtpl_init_expand
 *
 * Initializes a vtpl_expand_state object.
 *
 * Parameters :
 *
 *    es - out, use
 *        pointer to vtpl_expand_state structure
 *
 */
void vtpl_init_expand( vtpl_expand_state* es )
{
        es->p_vtpl = NULL;
        es->i_mismatch = 0;
        es->p_pfx_data =
        es->p_pfx_data_end = (ss_byte_t*)&es->pfx_buf;
        es->p_pfx_arr =
        es->p_pfx_arr_end = (vtpl_index_t*)((ss_byte_t*)(&es->pfx_buf) +
                                            sizeof(es->pfx_buf));
}                                       /* vtpl_init_expand */


/*##**********************************************************************\
 *
 *        vtpl_done_expand
 *
 * Cleans up a vtpl_expand_state object.
 *
 * Parameters :
 *
 *    es - out, use
 *        pointer to vtpl_expand_state structure
 *
 */
void vtpl_done_expand( vtpl_expand_state* es )
{
        if ( es->p_pfx_data != (ss_byte_t*)&es->pfx_buf)
            SsMemFree( es->p_pfx_data );
}                                       /* vtpl_done_expand */


/*##**********************************************************************\
 *
 *        vtpl_save_expand
 *
 * Advances a vtpl_expand_state object to point to the next
 * compressed v-tuple in sequence.
 *
 * Determines how much data from the current v-tuple (the one
 * given last time) will be needed to expand the next v-tuple
 * (the one given this time).  Caches this information in the
 * vtpl_expand_state object, along with the given v-tuple ptr.
 * Upon return, the caller can expand the v-tuple by calling
 * vtpl_copy_expand().
 *
 * Parameters :
 *
 *    es - in out, use
 *        pointer to vtpl_expand_state structure
 *
 *    i_next_mismatch - in
 *        mismatch index for next_vtpl
 *
 *    next_vtpl - in, hold
 *        pointer to next compressed v-tuple in sequence
 *
 */
void vtpl_save_expand( vtpl_expand_state* es
                     , vtpl_index_t       i_next_mismatch
                     , vtpl_t*            next_vtpl
                     )
{
        vtpl_t*       p_vtpl;
        vtpl_index_t  i_mismatch;

        /* Get current v-tuple ptr.  Stash next v-tuple ptr. */
        p_vtpl     = es->p_vtpl;
        es->p_vtpl = next_vtpl;

        /* Prefix doesn't change when current and next tuple have
           the same mismatch index. */
        if (es->i_mismatch == i_next_mismatch)
            return;

        i_mismatch     = es->i_mismatch;
        es->i_mismatch = i_next_mismatch;

        /* Prefix buffer contains leading portion of previous tuple,
           up to the current tuple's mismatch index: which equals the
           (number of field length indicators + number of data bytes)
           ahead of the current tuple's mismatch point.  For mismatch
           index computation, field length indicators are counted as
           exactly 1 byte, even though their actual size may vary.
           The first field length indicator is not counted. */
        ss_dassert( i_mismatch ==
                      (vtpl_index_t)((es->p_pfx_arr_end - es->p_pfx_arr) +
                                     (es->p_pfx_data_end - es->p_pfx_data)) );
        ss_dassert( es->p_pfx_data <= es->p_pfx_data_end &&
                    es->p_pfx_data_end <= (ss_byte_t*)es->p_pfx_arr &&
                    es->p_pfx_arr <= es->p_pfx_arr_end );
        ss_dassert( es->p_pfx_arr == es->p_pfx_arr_end
                    || (es->p_pfx_data + *es->p_pfx_arr <= es->p_pfx_data_end &&
                        es->p_pfx_arr_end[-1] <= *es->p_pfx_arr) );

        /* If next tuple has fewer matching prefix bytes, then all of
           the needed data (and more) is already in the prefix buffer.
           Discard the excess; keep exactly as much as next tuple needs. */
        if (i_next_mismatch < i_mismatch) { /* shrink prefix */
            vtpl_index_t* p_field_offset;

            /* Find next tuple's point of mismatch in prefix buffer. */
            p_field_offset = es->p_pfx_arr_end;
            while (p_field_offset > es->p_pfx_arr &&
                   i_next_mismatch > p_field_offset[-1]) {
                p_field_offset--;       /* step to next field */
                i_next_mismatch--;      /* exclude field length indicators */
            }

            /* Truncate prefix data at mismatch point. */
            es->p_pfx_data_end = es->p_pfx_data + i_next_mismatch;
            es->p_pfx_arr = p_field_offset;
        } /* shrink prefix */

        /* Next tuple shares all of the current prefix data, plus
           additional data to be taken from the current v-tuple. */
        else {  /* grow prefix */
            vtpl_index_t  l_vtpl;
            ss_byte_t*    p_vtpl_end;
            ss_byte_t*    p_vtpl_field;
            ss_byte_t*    p_vtpl_field_data;
            va_index_t    l_vtpl_field_data;
            vtpl_index_t  l_free_space;
            vtpl_index_t  l_want_space;

            i_next_mismatch -= i_mismatch;

            /* Possible db error if first tuple in sequence is compressed. */
            ss_assert( p_vtpl != NULL );

            /* First field of current v-tuple contains data to be
               grafted onto the previous tuple at the mismatch point. */
            VTPL_GETDATA( p_vtpl_field, p_vtpl, l_vtpl );
            p_vtpl_end = p_vtpl_field + l_vtpl;
            if (l_vtpl > 0) {  /* extend mismatch field */
                VA_GETDATA( p_vtpl_field_data, (va_t*)p_vtpl_field, l_vtpl_field_data );
                p_vtpl_field = p_vtpl_field_data + l_vtpl_field_data;
                ss_assert( p_vtpl_field <= p_vtpl_end );

                /* Grow prefix buffer if necessary. */
                l_want_space = SS_MIN( i_next_mismatch, l_vtpl_field_data );
                l_free_space = (vtpl_index_t)((ss_byte_t*)es->p_pfx_arr - es->p_pfx_data_end);
                if (l_want_space > l_free_space)
                    expand_grow( es, i_next_mismatch );

                /* Append data from first field of current v-tuple
                   to the last field in the prefix buffer. */
                memcpy( es->p_pfx_data_end, p_vtpl_field_data, l_want_space );
                es->p_pfx_data_end += l_want_space;
                i_next_mismatch -= l_want_space;
            } /* extend mismatch field */

            /* Add more fields from v-tuple to prefix buffer if needed. */
            while (i_next_mismatch) { /* copy more fields */

                /* Deduct new field's length indicator from mismatch index. */
                i_next_mismatch--;

                /* Grow prefix buffer if necessary. */
                l_free_space = (vtpl_index_t)((ss_byte_t*)es->p_pfx_arr - es->p_pfx_data_end);
                if (i_next_mismatch + sizeof(*es->p_pfx_arr) > l_free_space)
                    expand_grow( es, i_next_mismatch + sizeof(*es->p_pfx_arr) );

                /* Add an entry to the field array, containing the prefix
                   buffer offset where the new field's data will begin. */
                *(--es->p_pfx_arr) = (vtpl_index_t)(es->p_pfx_data_end - es->p_pfx_data);

                /* Access next field of current v-tuple. */
                if (i_next_mismatch > 0 &&
                    p_vtpl_field < p_vtpl_end) {
                    VA_GETDATA( p_vtpl_field_data, (va_t*)p_vtpl_field, l_vtpl_field_data );
                    p_vtpl_field = p_vtpl_field_data + l_vtpl_field_data;
                    ss_assert( p_vtpl_field <= p_vtpl_end );

                    /* Append field data to prefix buffer. */
                    l_want_space = SS_MIN( i_next_mismatch, l_vtpl_field_data );
                    memcpy( es->p_pfx_data_end, p_vtpl_field_data, l_want_space );
                    es->p_pfx_data_end += l_want_space;
                    i_next_mismatch -= l_want_space;
                }

            } /* copy more fields */
        } /* grow prefix */
}                                       /* vtpl_save_expand */


/*##**********************************************************************\
 *
 *        vtpl_copy_expand
 *
 * Expands the current v-tuple into caller's buffer.
 *
 * Parameters :
 *
 *    es - in, use
 *        Pointer to vtpl_expand_state structure, containing
 *        the current compressed v-tuple ptr and prefix info
 *        which were cached by vtpl_save_expand().
 *
 *    p_target_vtpl - out, use
 *        Pointer to destination area to get expanded v-tuple.
 *        Destination must not overlap with compressed v-tuple.
 *        Ignored if l_target_vtpl is less than the required size.
 *
 *    l_target_vtpl - in
 *        Size of destination area in bytes.
 *        If less than the size required to hold the expanded
 *        v-tuple, the expansion is not done and p_target_vtpl
 *        is ignored, but the required size is returned.
 *
 * Return value :
 *    Size of expanded v-tuple (number of bytes).  Always > 0.
 *
 */
vtpl_index_t vtpl_copy_expand( vtpl_expand_state* es
                             , vtpl_t*            p_target_vtpl
                             , vtpl_index_t       l_target_vtpl
                             )
{
        vtpl_index_t  l_vtpl;
        ss_byte_t*    p_vtpl_end;
        ss_byte_t*    p_vtpl_field;
        ss_byte_t*    p_vtpl_mismatch;
        va_index_t    l_vtpl_mismatch;
        va_index_t    l_pfx_mismatch;
        vtpl_index_t  l_pfx_field;
        vtpl_index_t  l_full_vtpl = 0;
        vtpl_index_t  off;
        vtpl_index_t* p_offset;

        /* Access first field of current compressed v-tuple. */
        ss_dassert( es->p_vtpl != NULL );
        VTPL_GETDATA( p_vtpl_field, es->p_vtpl, l_vtpl );
        p_vtpl_end = p_vtpl_field + l_vtpl;
        if (l_vtpl == 0) {
            p_vtpl_mismatch = p_vtpl_end;
            l_vtpl_mismatch = 0;
        } else {
            VA_GETDATA( p_vtpl_mismatch, (va_t*)p_vtpl_field, l_vtpl_mismatch );
            ss_assert( p_vtpl_mismatch + l_vtpl_mismatch <= p_vtpl_end );
        }

        /* Get sizes of field length indicators + data for
           all but the last field in prefix buffer.  */
        off = 0;
        p_offset = es->p_pfx_arr_end;
        while (p_offset > es->p_pfx_arr) {
            l_pfx_field = *(--p_offset) - off;
            l_full_vtpl += VA_LEN_LEN( l_pfx_field ) + l_pfx_field;
            off = *p_offset;
        }

        /* Add size of the field length indicator for the mismatch field.
           Data length of the mismatch field = data length of last
           field in prefix buffer + data length of first field of
           current compressed v-tuple. */
        l_pfx_mismatch = (vtpl_index_t)(es->p_pfx_data_end - es->p_pfx_data - off);
        l_full_vtpl += VA_LEN_LEN( l_pfx_mismatch + l_vtpl_mismatch );

        /* Add length of last prefix buffer field (up to the mismatch point). */
        l_full_vtpl += l_pfx_mismatch;

        /* Add length of tail of compressed v-tuple... the data and
           field length indicators from the mismatch point onward. */
        l_full_vtpl += (vtpl_index_t)(p_vtpl_end - p_vtpl_mismatch);

        /* Add size of tuple length indicator. */
        l_full_vtpl += VTPL_LEN_LEN( l_full_vtpl );

        /* Move expanded v-tuple to caller's buffer. */
        if (l_target_vtpl >= l_full_vtpl) {
            ss_byte_t*    p_tgt;
            ss_byte_t*    p_pfx_field;

            ss_dassert( p_target_vtpl != NULL );

            /* tuple length */
            VTPL_SET_LEN( p_tgt, p_target_vtpl, l_full_vtpl );

            /* fields preceding the mismatch field */
            off = 0;
            p_offset = es->p_pfx_arr_end;
            while (p_offset > es->p_pfx_arr) {
                p_pfx_field = es->p_pfx_data + off;
                l_pfx_field = *(--p_offset) - off;
                off = *p_offset;
                VA_SET_LEN( p_tgt, (va_t*)p_tgt, l_pfx_field );
                memcpy( p_tgt, p_pfx_field, l_pfx_field );
                p_tgt += l_pfx_field;
            }

            /* length of the mismatch field */
            VA_SET_LEN( p_tgt, (va_t*)p_tgt, l_pfx_mismatch + l_vtpl_mismatch );

            /* beginning of mismatch field, up to the mismatch point */
            p_pfx_field = es->p_pfx_data + off;
            memcpy( p_tgt, p_pfx_field, l_pfx_mismatch );
            p_tgt += l_pfx_mismatch;

            /* the rest of the v-tuple from the mismatch point onward */
            memcpy( p_tgt, p_vtpl_mismatch, p_vtpl_end - p_vtpl_mismatch );

            ss_dassert( (ss_byte_t*)p_target_vtpl + l_full_vtpl ==
                        p_tgt + (p_vtpl_end - p_vtpl_mismatch) );
        }

        return l_full_vtpl;             /* expanded vtpl size in bytes */
}                                       /* vtpl_expand_get_vtpl */


/*##**********************************************************************\
 *
 *        expand_grow
 *
 * Called by vtpl_expand_step to increase the free area within the
 * prefix buffer.
 *
 * Parameters :
 *
 *    es - in out, use
 *        pointer to vtpl_expand_state structure
 *
 *    l_needed - in
 *        minimum number of bytes required in the free area
 *
 */
static void expand_grow( vtpl_expand_state* es, vtpl_index_t l_needed )
{
        vtpl_index_t l_free;
        vtpl_index_t l_data;
        vtpl_index_t l_arr;
        vtpl_index_t l_new_free;
        vtpl_index_t l_new_total;
        ss_byte_t*   p_new;

        ss_dassert( es->p_pfx_data <= es->p_pfx_data_end &&
                    es->p_pfx_data_end <= (ss_byte_t*)es->p_pfx_arr &&
                    es->p_pfx_arr <= es->p_pfx_arr_end );

        l_data = (vtpl_index_t)(es->p_pfx_data_end - es->p_pfx_data);
        l_free = (vtpl_index_t)((ss_byte_t*)es->p_pfx_arr - es->p_pfx_data_end);
        l_arr  = (vtpl_index_t)((ss_byte_t*)es->p_pfx_arr_end - (ss_byte_t*)es->p_pfx_arr);

        /* Allocate new prefix buffer area. */
        l_new_free  = 20*sizeof(*es->p_pfx_arr);    /* a little extra */
        l_new_free += l_needed - l_free;
        l_new_total = l_data + l_new_free + l_arr;
        l_new_total &= - (int)sizeof(*es->p_pfx_arr);     /* round down to a multiple of
                                                       array entry size, for alignment */
        p_new = SsMemAlloc( l_new_total );

        /* Move prefix buffer contents to new area. */
        memcpy( p_new, es->p_pfx_data, l_data );
        memcpy( p_new + l_new_total - l_arr, es->p_pfx_arr, l_arr );

        /* Free old area. */
        if ( es->p_pfx_data != (ss_byte_t*)&es->pfx_buf)
            SsMemFree( es->p_pfx_data );

        /* Set ptrs to new area. */
        es->p_pfx_data     = p_new;
        es->p_pfx_data_end = p_new + l_data;
        es->p_pfx_arr      = (vtpl_index_t*)(p_new + l_new_total - l_arr);
        es->p_pfx_arr_end  = (vtpl_index_t*)(p_new + l_new_total);
}                                       /* expand_grow */

/*##**********************************************************************\
 *
 *		vtpl_condcompare
 *
 * Vtuple comparison with ORDER BY (asc|desc) conditions.
 *
 * Parameters :
 *
 *	vtpl1 - in, use
 *		pointer to 1st vtuple
 *
 *	vtpl2 - in, use
 *		pointer to 2nd vtuple
 *
 *
 *	condarr - in, use
 *		array of unsigned integers with the following
 *          semantics:
 *          condarr[0]      - # of order by columns
 *          condarr[1 - #]  - VTPL_CMP_ASC when sort condition is
 *                              ascending or
 *                            VTPL_CMP_DESC when sort condition is
 *                              descending (NULL collates to low value)
 *                            VTPL_CMP_DESC_NC_START when sort condition
 *                              is descending and NULL collates to start.
 *
 * Return value :
 *      == 0 when vtuples compare equal up to length of order by list
 *      <  0 when vtpl1 < vtpl2 (logically)
 *      >  0 otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int vtpl_condcompare(vtpl_t* vtpl1, vtpl_t* vtpl2, uint* condarr)
{
        uint ncond;
        int cmp;
        va_t* va1;
        va_t* va2;
        ss_byte_t* p1;
        ss_byte_t* p2;
        va_index_t l1;
        va_index_t l2;
        va_index_t l;

        va1 = (va_t*)va_getdata((va_t*)(ss_byte_t*)vtpl1, &l1);
        va2 = (va_t*)va_getdata((va_t*)(ss_byte_t*)vtpl2, &l2);
        ncond = condarr[0];
        ss_dassert(ncond != 0);
        for (cmp = 0; ncond; ncond--) {
            condarr++;
            p1 = (ss_byte_t*)va_getdata(va1, &l1);
            VA_GETDATA(p2, va2, l2);
            l = SS_MIN(l1, l2);
            if (l != 0) {
                cmp = SsMemcmp(p1, p2, l);
            }
            if (cmp == 0) {
                cmp = (int)(l1 - l2);
                if (cmp != 0) {
                    break;
                }
            } else {
                break;
            }
            va1 = (va_t*)(p1 + l1);
            va2 = (va_t*)(p2 + l2);
        }
        if (cmp != 0 && *condarr != VTPL_CMP_ASC) {
            if (*condarr != VTPL_CMP_DESC_NC_START
            ||  (!va_testnull(va1) && !va_testnull(va2)))
            {
                return (-cmp);
            }
        }
        return (cmp);
}
