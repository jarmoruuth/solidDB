/*************************************************************************\
**  source       * uti0vcmp.h
**  directory    * uti
**  description  * v-attribute and v-tuple comparisons and searches
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

#ifndef UTI0VCMP_H
#define UTI0VCMP_H

#include "uti0va.h"
#include "uti0vtpl.h"

/* VTPL compare conditions (for vtpl_condcompare())*/
#define VTPL_CMP_ASC            0   /* asc. NULL is the lowest value */
#define VTPL_CMP_DESC_NC_START  1   /* desc. NULL to start */
#define VTPL_CMP_DESC           2   /* desc. NULL is the lowest value */
                                    /* (and goes to end) */

/* types ***************************************************/

typedef struct {
        vtpl_index_t i_mismatch; /* the index of the first mismatched byte
                                    from the comparison of the largest index
                                    entry smaller than the search key. */
        bool         i_ob1_inuse; /* TRUE if i_ob1_mismatch is in use */
        vtpl_index_t i_ob1_mismatch; /* off-by-one mismatch, the index of the
                                        1st mismatched byte from the comparison
                                        to the first bigger index entry. */
        va_index_t   l_field;    /* the length of the mismatched portion of
                                    the current field */
        ss_byte_t*   p_mismatch; /* a pointer to the first mismatched byte */
        ss_byte_t*   p_key_end;  /* a pointer to the last byte of the key */
        void*        key_data;
        int          (*cmp_fn)(void*, void*);
} search_state;

/* The i_mismatch index is the number of field data bytes this far, plus 1
   for each field. */

#ifndef VTPL_EXPAND_BYTES
#define VTPL_EXPAND_BYTES 200
#endif

typedef struct {
        vtpl_t*       p_vtpl;           /* -> current compressed v-tuple */
        vtpl_index_t  i_mismatch;       /* index of first mismatched byte */
        ss_byte_t*    p_pfx_data;       /* -> prefix buffer */
        ss_byte_t*    p_pfx_data_end;   /* -> end of data in prefix buffer */
        vtpl_index_t* p_pfx_arr;        /* -> array of field data offsets */
        vtpl_index_t* p_pfx_arr_end;    /* -> end of field offset array */
        vtpl_index_t  pfx_buf[VTPL_EXPAND_BYTES/sizeof(vtpl_index_t)];
} vtpl_expand_state;

/* global functions ****************************************/

int va_compare(va_t* va1, va_t* va2);

#ifdef SS_UNICODE_DATA
int va_compare_char1v2(va_t* va1, va_t* va2);
int va_compare_char2v1(va_t* va1, va_t* va2);
#endif /* SS_UNICODE_DATA */

search_state* vtpl_search_state_init(search_state* p_search_state, vtpl_t* key,
                               void* key_data);
SS_INLINE void vtpl_search_setcmpfn(search_state* p_search_state,
                          int (*fn)(void*, void*));
int vtpl_search_step_fn(search_state* p_search_state, vtpl_t* index_entry,
                        void* index_data);
SS_INLINE int vtpl_search_step_sizecheck(
        search_state*   p_search_state,
        vtpl_t*         index_entry,
        void*           index_data,
        void*           floor,
        void*           ceiling);
int vtpl_compare(vtpl_t* vtpl1, vtpl_t* vtpl2);
int vtpl_condcompare(vtpl_t* vtpl1, vtpl_t* vtpl2, uint* condarr);
bool vtpl_equal(vtpl_t* vtpl1, vtpl_t* vtpl2);

vtpl_index_t vtpl_compress(vtpl_t* target_vtpl, vtpl_t* prev_vtpl,
                           vtpl_t* next_vtpl);
vtpl_index_t dynvtpl_compress(dynvtpl_t* p_target_vtpl, vtpl_t* prev_vtpl,
                              vtpl_t* next_vtpl);
vtpl_index_t vtpl_search_compress(vtpl_t* target_vtpl, search_state* cs);
vtpl_index_t dynvtpl_search_compress(dynvtpl_t* p_target_vtpl, search_state* cs);
SS_INLINE vtpl_t* dynvtpl_expand(
        dynvtpl_t* p_target_vtpl,
        vtpl_t* full_vtpl,
        vtpl_t* compressed_vtpl,
        vtpl_index_t i_mismatch);

SS_INLINE bool vtpl_expand_sizecheck(
        vtpl_t* target_vtpl,
        vtpl_t* prev_vtpl,
        vtpl_t* compressed_vtpl,
        vtpl_index_t i_mismatch,
        vtpl_index_t prev_grosslen,
        vtpl_index_t comp_grosslen);

SS_INLINE vtpl_t* vtpl_find_split(vtpl_t* target_vtpl, vtpl_t* full_vtpl,
                        vtpl_t* compressed_vtpl, vtpl_index_t i_mismatch);
vtpl_t* dynvtpl_find_split(dynvtpl_t* p_target_vtpl, vtpl_t* full_vtpl,
                           vtpl_t* compressed_vtpl, vtpl_index_t i_mismatch);

void vtpl_init_expand(
        vtpl_expand_state* es);

void vtpl_done_expand(
        vtpl_expand_state* es);

void vtpl_save_expand(
        vtpl_expand_state* es,
        vtpl_index_t       i_next_mismatch,
        vtpl_t*            next_vtpl);

vtpl_index_t  vtpl_copy_expand(
        vtpl_expand_state* es,
        vtpl_t*            p_target_vtpl ,
        vtpl_index_t       l_target_vtpl);

/* internal */
SS_INLINE void uti_expand(
        void* target_vtpl,
        vtpl_t* prev_vtpl,
        vtpl_t* compressed_vtpl,
        vtpl_index_t i_mismatch,
        bool f_dyn,
        bool f_split);

SS_INLINE bool uti_expand_sizecheck(
        void* target_vtpl,
        vtpl_t* prev_vtpl,
        vtpl_t* compressed_vtpl,
        register vtpl_index_t i_mismatch,
        vtpl_index_t prev_grosslen,
        vtpl_index_t comp_grosslen);

/*##**********************************************************************\
 *
 *		vtpl_dbe_search_step
 *
 * Perform a single step in an index search, comparing the key indicated
 * by the search state argument with an index entry.  Calls
 * vtpl_search_step to do the general case.
 *
 * This macro compares the key and the compressed-out prefix of the index
 * entry:
 *   (state.i_mismatch < ie_i_mismatch) means that the change
 *      in the prefix from the previous entry isn't large enough,
 *   (state.i_mismatch > ie_i_mismatch) means that the change
 *      is too large; in this case, the search ends here, hence there
 *      is no need to update search_state.
 *
 * Parameters :
 *
 *      var - out, use
 *		a variable to store the result to
 *
 *	state - in out, use
 *		the search state structure, the search state structure
 *          is updated, if the search did not end
 *
 *      ie_i_mismatch - in
 *		the i_mismatch index of the current index entry
 *
 *	index_entry - in, use
 *		pointer to the index entry (a v-tuple)
 *
 *      index_data - in, use
 *		additional data passed to user comparison function,
 *          if such function is given
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
 * Globals used :
 */
#ifdef SS_DEBUG_USE_ALWAYS_MACRO

/* We have function version here just in case if we need to do debugging.
 */
#define vtpl_dbe_search_step(var, state, ie_i_mismatch, index_entry, index_data) \
        if (((var) = (ie_i_mismatch) - (state).i_mismatch) == 0) \
            (var) = vtpl_search_step_fn(&(state), index_entry, index_data);

#else /* SS_DEBUG_USE_ALWAYS_MACRO */

#define vtpl_dbe_search_step(var, state, ie_i_mismatch, index_entry, index_data)         \
        if (((var) = (ie_i_mismatch) - (state).i_mismatch) == 0) {                       \
            do {                                                                         \
                search_state M_ss;                                                       \
                vtpl_index_t M_l_ie;                                                     \
                va_index_t M_i_cmp;                                                      \
                va_index_t M_l_cmp, M_l_ie_field;                                        \
                ss_byte_t* M_p_ie_data;                                                  \
                ss_byte_t* M_p_ie_end;                                                   \
                ss_byte_t* M_p_ie_start;                                                 \
                int M_comp_res;                                                          \
                bool M_done = FALSE;                                                     \
                                                                                         \
                /* Find the end and the first field of the index entry */                \
                VTPL_GETDATA(M_p_ie_start, index_entry, M_l_ie);                         \
                ss_dassert(M_l_ie != 0);                                                 \
                                                                                         \
                /* Does simple test for the va's: comparing the length and               \
                 * the content as a whole. Not applicable for the mmeg2 where the        \
                 * order is not the only point of interest because the length of the     \
                 * common prefix with the next vtpl is needed too.                       \
                 */                                                                      \
                VA_GETDATA(M_p_ie_data, (va_t*)M_p_ie_start, M_l_ie_field);              \
                M_ss = (state);                                                          \
                                                                                         \
                M_i_cmp=0; /* Haven't compared anything yet. */                          \
                M_l_cmp = SS_MIN(M_ss.l_field, M_l_ie_field);                            \
                                                                                         \
                ss_dassert(!M_ss.i_ob1_inuse); /* Not mmeg2 in case */                   \
                                                                                         \
                /* Is the length of the tails equal ? */                                 \
                if (M_l_cmp != 0) /* no null fields */ {                                 \
                    /* Is the content of the tails equal? */                             \
                    if ((M_comp_res = ZERO_EXTEND_TO_INT(*M_ss.p_mismatch) -             \
                                    ZERO_EXTEND_TO_INT(*M_p_ie_data)) != 0) {            \
                        var = M_comp_res;                                                \
                        break;                                                           \
                    } else {                                                             \
                        M_i_cmp = 1; /* Have compared one byte, step loop vars */        \
                        M_ss.p_mismatch++; M_p_ie_data++;                                \
                    }                                                                    \
                }                                                                        \
                                                                                         \
                /* Set up the rest */                                                    \
                M_p_ie_end = M_p_ie_start + M_l_ie;                                      \
                M_ss.i_mismatch = (state).i_mismatch;                                    \
                M_ss.p_key_end = (state).p_key_end;                                      \
                                                                                         \
                for (;;) { /* Loop over fields */                                        \
                    /* find mismatch in this field */                                    \
                    /* find mismatch in this field */                                    \
                    ss_debug(var = LONG_MAX);                                            \
                    for (; M_i_cmp < M_l_cmp;                                            \
                        M_ss.p_mismatch++, M_i_cmp++, M_p_ie_data++) {                   \
                        M_comp_res = ZERO_EXTEND_TO_INT(*M_ss.p_mismatch) -              \
                                   ZERO_EXTEND_TO_INT(*M_p_ie_data);                     \
                        if (M_comp_res > 0) { /* *M_ss.p_mismatch < *M_p_ie_data */      \
                            (state).i_mismatch = M_ss.i_mismatch + M_i_cmp;              \
                            (state).l_field = M_ss.l_field - M_i_cmp;                    \
                            (state).p_mismatch = M_ss.p_mismatch;                        \
                            var = 1;                                                     \
                            M_done = TRUE;                                               \
                            break;                                                       \
                        } else if (M_comp_res < 0) {                                     \
                            var = -1;                                                    \
                            M_done = TRUE;                                               \
                            break;                                                       \
                            /* The search ends, no need to update state. */              \
                        }                                                                \
                    }                                                                    \
                    if (M_done) {                                                        \
                        break;                                                           \
                    }                                                                    \
                                                                                         \
                    /* The data bytes are identical upto M_l_cmp, compare lengths:       \
                       the shorter field precedes the longer */                          \
                    /* After this, M_i_cmp equals M_l_cmp, so we use it instead of       \
                       M_l_ie_field or M_ss.l_field, because it is a register var */     \
                    if (M_ss.l_field > M_i_cmp) { /* M_ss.l_field > M_l_ie_field */      \
                        M_ss.i_mismatch += M_i_cmp; M_ss.l_field -= M_i_cmp;             \
                        (state) = M_ss;                                                  \
                        var = 1;                                                         \
                        break;                                                           \
                    } else if (M_i_cmp == M_l_ie_field) { /* M_ss.l_field == M_l_ie_field */ \
                        /* do next field */                                              \
                        if (M_ss.p_key_end <= M_ss.p_mismatch) { /* the key ended */     \
                            if (M_p_ie_end <= M_p_ie_data) /* ie end? */ {               \
                                int fn_cmp;                                              \
                                if ((state).cmp_fn != NULL) {                            \
                                    fn_cmp = (*((state).cmp_fn))(                        \
                                                        (state).key_data,                \
                                                        index_data);                     \
                                } else {                                                 \
                                    fn_cmp = 0;                                          \
                                }                                                        \
                                if (fn_cmp > 0) {                                        \
                                    (state).i_mismatch = M_ss.i_mismatch + M_i_cmp;      \
                                    (state).l_field = M_ss.l_field - M_i_cmp;            \
                                    (state).p_mismatch = M_ss.p_mismatch;                \
                                } else {                                                 \
                                    /* if fn_cmp <= 0, the search ends, no need to       \
                                     * update state. */                                  \
                                }                                                        \
                                var = fn_cmp;                                            \
                                break;                                                   \
                            } else {                                                     \
                                var = -1;                                                \
                                break;                                                   \
                                /* The search ends, no need to update state. */          \
                            }                                                            \
                        } else { /* the key didn't end yet */                            \
                            /* update state */                                           \
                            M_ss.i_mismatch += M_i_cmp + 1;                              \
                            VA_GETDATA(M_ss.p_mismatch, (va_t*)M_ss.p_mismatch, M_ss.l_field); \
                                                                                         \
                            if (M_p_ie_end <= M_p_ie_data) /* ie end? */ {               \
                                (state) = M_ss;                                          \
                                var = 1;                                                 \
                                break;                                                   \
                            }                                                            \
                            /* This is the only branch back to the for loop!             \
                               Setup for next loop */                                    \
                            VA_GETDATA(M_p_ie_data, (va_t*)M_p_ie_data, M_l_ie_field);   \
                            M_l_cmp = SS_MIN(M_ss.l_field, M_l_ie_field); M_i_cmp = 0;   \
                            continue;                                                    \
                        }                                                                \
                    } else { /* M_ss.l_field < M_l_ie_field */                           \
                        /* The search ends, no need to update (state)                    \
                         * update state. */                                              \
                        var = -1;                                                        \
                        break;                                                           \
                    }                                                                    \
                    ss_dassert(var != LONG_MAX);                                         \
                }                                                                        \
            } while (0);                                                                 \
        }

#endif /* SS_DEBUG_USE_ALWAYS_MACRO */

/*##**********************************************************************\
 *
 *              VTPL_MME_SEARCH_STEP
 *
 * If mismatching portions differ in length, vtpl_search_step_fn won't be
 * called. In this case state.i_ob1_mismatch must be set to zero.
 *
 * Parameters :
 *
 *      var - out, use
 *              a variable to store the result to
 *
 *      state - in out, use
 *              the search state structure, the search state structure
 *          is updated, if the search did not end
 *
 *      ie_i_mismatch - in
 *              the i_mismatch index of the current index entry
 *
 *      index_entry - in, use
 *              pointer to the index entry (a v-tuple)
 *
 *      index_data - in, use
 *              additional data passed to user comparison function,
 *          if such function is given
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
 * Globals used :
 */
#define VTPL_MME_SEARCH_STEP(var, state, ie_i_mismatch, index_entry, index_data, floor, ceiling) \
        if (((var) = (ie_i_mismatch) - (state).i_mismatch) == 0) {\
            (var) = vtpl_search_step_sizecheck(&(state), index_entry, index_data, floor, ceiling);\
        } else {\
            (state).i_ob1_mismatch = 0;\
        }

/*##**********************************************************************\
 *
 *		vtpl_search_state_init
 *
 * Initialises a search_state object
 *
 * Parameters :
 *
 *	p_search_state - out, use
 *		pointer to a search state structure, the search state
 *          structure is initialized
 *
 *	key - in, hold
 *		a pointer to the key (a v-tuple) to be searched for
 *
 *      key_data - in hold
 *          additional data for key to use in comparison when
 *          v-tuples are equal, used only when special comparison
 *          function is specified using function vtpl_search_setcmpfn
 *
 * Return value :
 *
 * Limitations  :
 *
 *      Assumes the key isn't empty.
 *
 * Globals used :
 */
#define vtpl_search_state_init(M_p_search_state, M_key, M_key_data) \
{ \
        search_state* L_p_search_state = M_p_search_state; \
        vtpl_t* L_key = M_key; \
        void* L_key_data = M_key_data; \
        vtpl_index_t L_l_key; \
        void* L_first_field; \
        void* L_mismatch; \
 \
        ss_dassert(L_key != NULL); \
        L_p_search_state->i_mismatch = 0; \
        L_p_search_state->i_ob1_mismatch = 0; \
        L_p_search_state->key_data = L_key_data; \
        L_p_search_state->cmp_fn = NULL; \
        VTPL_GETDATA(L_first_field, L_key, L_l_key); \
        ss_dassert(L_l_key != 0); \
        L_p_search_state->p_key_end = (ss_byte_t*)L_first_field + L_l_key; \
        L_mismatch = (void*)L_p_search_state->p_mismatch; \
        VA_GETDATA(L_mismatch, (va_t*)L_first_field, L_p_search_state->l_field); \
        L_p_search_state->p_mismatch = (ss_byte_t*)L_mismatch; \
}

#define VTPL_MME_SEARCH_INIT(p_search_state, key) \
        (p_search_state)->i_ob1_inuse = TRUE; \
        vtpl_search_state_init( (p_search_state), (key), NULL); 


#define vtpl_search_init(p_search_state, key, key_data) \
        (p_search_state)->i_ob1_inuse = FALSE; \
        vtpl_search_state_init( (p_search_state), (key), (key_data) );

/*##**********************************************************************\
 *
 *		vtpl_expand
 *
 * Expand a compressed index entry back to original.
 *
 * Parameters :
 *
 *	target_vtpl - out, use
 *		a pointer to a v-tuple to write to
 *
 *	prev_vtpl - in, use
 *		a pointer to the previous index entry
 *
 *	compressed_vtpl - in, use
 *		a pointer to the entry to be expanded
 *
 *	i_mismatch - in
 *		the i-mismatch index of the first byte in compressed_vtpl
 *
 *
 * Return value - ref :
 *
 *      target_vtpl
 *
 * Limitations  :
 *
 *      *target_vtpl may not overlap the other v-tuples, doesn't
 *      work with empty v-tuples
 *
 * Globals used :
 */
#define vtpl_expand(M_target_vtpl, M_prev_vtpl, M_compressed_vtpl, M_i_mismatch) \
{ \
        vtpl_t* L_target_vtpl = M_target_vtpl; \
        vtpl_t* L_prev_vtpl = M_prev_vtpl; \
        vtpl_t* L_compressed_vtpl = M_compressed_vtpl; \
        vtpl_index_t L_i_mismatch = M_i_mismatch; \
 \
        if (L_i_mismatch == 0) { \
            vtpl_setvtpl(L_target_vtpl, L_compressed_vtpl); \
        } else { \
            uti_expand(L_target_vtpl, \
                       L_prev_vtpl, \
                       L_compressed_vtpl, \
                       L_i_mismatch, \
                       FALSE, \
                       FALSE); \
        } \
}

#if defined(UTI0VCMP_C) || defined(SS_USE_INLINE)

/*##*********************************************************************\
 *
 *		uti_expand
 *
 * Expands a v-tuple.
 *
 * Parameters :
 *
 *	target_vtpl - in out, use
 *		target v-tuple
 *
 *	prev_vtpl - in, use
 *		previous v-tuple
 *
 *	compressed_vtpl - in, use
 *		next compressed v-tuple
 *
 *	i_mismatch - in
 *		mismatch index
 *
 *	f_dyn - in
 *		if TRUE, target v-tuple is allocated dynamically
 *
 *	f_split - in
 *		if TRUE, actually a split v-tuple is build
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void uti_expand(
        void* target_vtpl,
        vtpl_t* prev_vtpl,
        vtpl_t* compressed_vtpl,
        register vtpl_index_t i_mismatch,
        bool f_dyn,
        bool f_split)
{
        register vtpl_index_t l_target;
        vtpl_index_t l_full, l_comp, l_common_portion, l_new_portion;
        va_index_t l_prev_field, l_comp_field, l_mismatch_field;
        va_t* p_prev_field;
        va_t* p_prev_start;
        va_t* p_comp_start;
        ss_byte_t* p_prev_end;
        ss_byte_t* p_prev_data;
        ss_byte_t* p_comp_data;
        register ss_byte_t* p_target_data;
        ss_byte_t* saved_target_data;
        
        /* find lengths */
        p_prev_field = (va_t*)va_getdata((va_t*)(ss_byte_t*)prev_vtpl, &l_full);
        p_prev_start = p_prev_field;
        p_comp_start = (va_t*)va_getdata((va_t*)(ss_byte_t*)compressed_vtpl, &l_comp);
        p_prev_end = ((ss_byte_t*)p_prev_start + l_full);
        ss_dassert(l_full != 0);
        ss_dassert(l_comp != 0);

        /* find point of mismatch */
        for (;;) { /* loop over fields */
            p_prev_data = (ss_byte_t*)va_getdata(p_prev_field, &l_prev_field);
            if (i_mismatch <= l_prev_field) break;
            i_mismatch -= l_prev_field + 1;
            p_prev_field = (va_t*)(p_prev_data + l_prev_field);
            if (i_mismatch == 0) break;
            ss_dassert((ss_byte_t*)p_prev_field < p_prev_end);
        }

        /* compute lengths */
        p_comp_data = (ss_byte_t*)va_getdata(p_comp_start, &l_comp_field);
        l_common_portion = (vtpl_index_t)((ss_byte_t*)p_prev_field - (ss_byte_t*)p_prev_start);
        if (f_split) {
            /* If prev_vtpl ends at the mismatch at a field boundary
               (p_prev_end == p_prev_field), we add an empty v-attibute,
               otherwise one byte from compressed_vtpl. */
            l_new_portion = (vtpl_index_t)(SS_MIN(1, p_prev_end - (ss_byte_t*)p_prev_field));
            l_mismatch_field = i_mismatch + SS_MIN(l_comp_field, l_new_portion);
        } else {
            l_new_portion = (vtpl_index_t)(((ss_byte_t*)p_comp_start + l_comp) - p_comp_data);
            l_mismatch_field = i_mismatch + l_comp_field;
        }
        l_target = l_common_portion + VA_LEN_LEN(l_mismatch_field) +
                   i_mismatch + l_new_portion;

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

        saved_target_data = p_target_data;

        if (saved_target_data == (ss_byte_t*)prev_vtpl) {
            int lenlen_diff;
            
            lenlen_diff =
                (int)BYTE_LEN_TEST(l_full) -
                (int)BYTE_LEN_TEST(l_target);
            if (lenlen_diff < 0) {
                ss_dassert(lenlen_diff == -1);
                goto copy_from_left_to_right;;
            }
            if (lenlen_diff == 0) {
                lenlen_diff =
                    (int)BYTE_LEN_TEST(l_prev_field) -
                    (int)BYTE_LEN_TEST(l_mismatch_field);
                if (lenlen_diff <= 0) {
                    goto copy_from_left_to_right;
                } /* else copy_from_right_to_left */
            } /* else copy_from_right_to_left */
 /* copy_from_right_to_left:; */
            {
                ss_byte_t* target_len_position;
                ss_byte_t* target_data_position;
                ss_byte_t* mismatch_field_len_position;
                /* ss_byte_t* mismatch_field_data_position; */

                target_len_position = p_target_data;
                p_target_data += VA_LEN_LEN(l_target);
                target_data_position = p_target_data;
                p_target_data += l_common_portion;
                mismatch_field_len_position = p_target_data;
                p_target_data += VA_LEN_LEN(l_mismatch_field);
                /* mismatch_field_data_position = p_target_data; */
                
                memmove(p_target_data, /* = mismatch_field_data_position */
                        p_prev_data, i_mismatch);
                memmove(target_data_position,
                         p_prev_start, l_common_portion);
                VA_SET_LEN_ONLY((va_t*)mismatch_field_len_position,
                                l_mismatch_field);
                VTPL_SET_LEN_ONLY((vtpl_t*)target_len_position, l_target);
            }
        } else {
    copy_from_left_to_right:;
            VTPL_SET_LEN(p_target_data, (vtpl_t*)p_target_data, l_target);
            /* copy common portion */
            memcpy(p_target_data, p_prev_start, l_common_portion);
            p_target_data += l_common_portion;

            /* set length of mismatch field */
            VA_SET_LEN(p_target_data, (va_t*)p_target_data, l_mismatch_field);

            /* copy common portion of the mismatch field */
            memcpy(p_target_data, p_prev_data, i_mismatch);
        }
        p_target_data += i_mismatch;
        /* copy mismatched portion */
        if (f_split) {
            if (l_new_portion != 0)
                *p_target_data = *p_comp_data;
        } else {
            memcpy(p_target_data, p_comp_data, l_new_portion);
        }
}

#define UTI_CHECK_BOUNDS(ptr, floor, ceiling) \
        ((ss_byte_t*)(floor) <= (ss_byte_t*)(ptr) &&\
         (ss_byte_t*)(ptr) <= (ss_byte_t*)(ceiling))

/*#**********************************************************************\
 *
 *		uti_expand_sizecheck
 *
 * Expands a v-tuple.
 *
 * Parameters :
 *
 *	target_vtpl - in out, use
 *		target v-tuple
 *
 *	prev_vtpl - in, use
 *		previous v-tuple
 *
 *	compressed_vtpl - in, use
 *		next compressed v-tuple
 *
 *	i_mismatch - in
 *		mismatch index
 *
 *	f_split - in
 *		if TRUE, actually a split v-tuple is build
 *
 *      floor - in
 *          lowest allowed pointer value while reading
 *          compressed_vtpl (and, conditionally while reading
 *          prev_vtpl)
 *      ceiling - in
 *          smallest illegal pointer value bigger than
 *          compressed_vtpl (and, conditionally, prev_vtpl)
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool uti_expand_sizecheck(
        void* target_vtpl,
        vtpl_t* prev_vtpl,
        vtpl_t* compressed_vtpl,
        register vtpl_index_t i_mismatch,
        vtpl_index_t prev_grosslen,
        vtpl_index_t comp_grosslen)
{
        register vtpl_index_t l_target;
        vtpl_index_t l_full, l_comp, l_common_portion, l_new_portion;
        va_index_t l_prev_field, l_comp_field, l_mismatch_field;
        va_t* p_prev_field;
        va_t* p_prev_start;
        va_t* p_comp_start;
        ss_byte_t* p_prev_end;
        ss_byte_t* p_prev_data;
        ss_byte_t* p_comp_data;
        register ss_byte_t* p_target_data;
        ss_byte_t* saved_target_data;
        ss_byte_t* prev_floor;
        ss_byte_t* prev_ceiling;
        ss_byte_t* comp_floor;
        ss_byte_t* comp_ceiling;
        va_index_t i;
        va_index_t loop_max;

        prev_floor = (ss_byte_t*)prev_vtpl;
        comp_floor = (ss_byte_t*)compressed_vtpl;
        prev_ceiling = prev_floor + prev_grosslen;
        comp_ceiling = comp_floor + comp_grosslen;
        loop_max = prev_grosslen + comp_grosslen;
        
        /* find lengths */
        p_prev_field = (va_t*)va_getdata((va_t*)(ss_byte_t*)prev_vtpl, &l_full);
        p_prev_start = p_prev_field;
        p_prev_end = ((ss_byte_t*)p_prev_start + l_full);
        if (!UTI_CHECK_BOUNDS(p_prev_end, prev_floor, prev_ceiling)
        ||  !UTI_CHECK_BOUNDS(p_prev_start, prev_floor, prev_ceiling))
        {
            return (FALSE);
        }
        p_comp_start = (va_t*)va_getdata((va_t*)(ss_byte_t*)compressed_vtpl, &l_comp);
        {
            ss_byte_t* p_comp_end = (ss_byte_t*)p_comp_start + l_comp;
            
            if (!UTI_CHECK_BOUNDS(p_comp_start, comp_floor, comp_ceiling)
            ||  !UTI_CHECK_BOUNDS(p_comp_end, comp_floor, comp_ceiling)
            ||  l_full == 0
            ||  l_comp == 0)
            {
                return (FALSE);
            }
        }
        ss_dassert(l_full != 0);
        ss_dassert(l_comp != 0);

        /* find point of mismatch */
        for (i = 0; ; i++) { /* loop over fields */
            if (i >= loop_max) {
                /* avoid endless loop, data has changed! */
                return (FALSE);
            }
            p_prev_data = (ss_byte_t*)va_getdata(p_prev_field, &l_prev_field);
            {
                ss_byte_t* p_prev_data_end = p_prev_data + l_prev_field;

                if (!UTI_CHECK_BOUNDS(p_prev_data, prev_floor, prev_ceiling)
                ||  !UTI_CHECK_BOUNDS(p_prev_data_end, prev_floor, prev_ceiling))
                {
                    return (FALSE);
                }
            }
            if (i_mismatch <= l_prev_field) break;
            i_mismatch -= l_prev_field + 1;
            p_prev_field = (va_t*)(p_prev_data + l_prev_field);
            if (i_mismatch == 0) break;
            ss_assert((ss_byte_t*)p_prev_field < p_prev_end);
        }

        /* compute lengths */
        p_comp_data = (ss_byte_t*)va_getdata(p_comp_start, &l_comp_field);
        {
            ss_byte_t* p_comp_data_end = p_comp_data + l_comp_field;
            if (!UTI_CHECK_BOUNDS(p_comp_data, comp_floor, comp_ceiling)
            ||  !UTI_CHECK_BOUNDS(p_comp_data_end, comp_floor, comp_ceiling))
            {
                return (FALSE);
            }
        }
        l_common_portion = (vtpl_index_t)((ss_byte_t*)p_prev_field - (ss_byte_t*)p_prev_start);
        l_new_portion = (vtpl_index_t)(((ss_byte_t*)p_comp_start + l_comp) - p_comp_data);
        l_mismatch_field = i_mismatch + l_comp_field;
        l_target = l_common_portion + VA_LEN_LEN(l_mismatch_field) +
                   i_mismatch + l_new_portion;

        p_target_data = (ss_byte_t*)target_vtpl;

        saved_target_data = p_target_data;

        if (saved_target_data == (ss_byte_t*)prev_vtpl) {
            int lenlen_diff;
            
            lenlen_diff =
                (int)BYTE_LEN_TEST(l_full) -
                (int)BYTE_LEN_TEST(l_target);
            if (lenlen_diff < 0) {
                ss_dassert(lenlen_diff == -1);
                goto copy_from_left_to_right;;
            }
            if (lenlen_diff == 0) {
                lenlen_diff =
                    (int)BYTE_LEN_TEST(l_prev_field) -
                    (int)BYTE_LEN_TEST(l_mismatch_field);
                if (lenlen_diff <= 0) {
                    goto copy_from_left_to_right;
                } /* else copy_from_right_to_left */
            } /* else copy_from_right_to_left */
 /* copy_from_right_to_left:; */
            {
                ss_byte_t* target_len_position;
                ss_byte_t* target_data_position;
                ss_byte_t* mismatch_field_len_position;
                /* ss_byte_t* mismatch_field_data_position; */

                target_len_position = p_target_data;
                p_target_data += VA_LEN_LEN(l_target);
                target_data_position = p_target_data;
                p_target_data += l_common_portion;
                mismatch_field_len_position = p_target_data;
                p_target_data += VA_LEN_LEN(l_mismatch_field);
                /* mismatch_field_data_position = p_target_data; */
                
                memmove(p_target_data, /* = mismatch_field_data_position */
                        p_prev_data, i_mismatch);
                memmove(target_data_position,
                         p_prev_start, l_common_portion);
                VA_SET_LEN_ONLY((va_t*)mismatch_field_len_position,
                                l_mismatch_field);
                VTPL_SET_LEN_ONLY((vtpl_t*)target_len_position, l_target);
            }
        } else {
    copy_from_left_to_right:;
            VTPL_SET_LEN(p_target_data, (vtpl_t*)p_target_data, l_target);
            /* copy common portion */
            memmove(p_target_data, p_prev_start, l_common_portion);
            p_target_data += l_common_portion;

            /* set length of mismatch field */
            VA_SET_LEN(p_target_data, (va_t*)p_target_data, l_mismatch_field);

            /* copy common portion of the mismatch field */
            memmove(p_target_data, p_prev_data, i_mismatch);
        }
        p_target_data += i_mismatch;
        /* copy mismatched portion */
        memcpy(p_target_data, p_comp_data, l_new_portion);
        return (TRUE);
}


/*##**********************************************************************\
 *
 *		dynvtpl_expand
 *
 * Expand a compressed index entry back to original, using a dynvtpl result
 * buffer.
 *
 * Parameters :
 *
 *	p_target_vtpl - in out, give
 *		a pointer to a dynvtpl variable to write to
 *
 *	prev_vtpl - in, use
 *		a pointer to the previous index entry
 *
 *	compressed_vtpl - in, use
 *		a pointer to the entry to be expanded
 *
 *	i_mismatch - in
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
SS_INLINE vtpl_t* dynvtpl_expand(
        dynvtpl_t* p_target_vtpl,
        vtpl_t* prev_vtpl,
        vtpl_t* compressed_vtpl,
        vtpl_index_t i_mismatch)
{
        uti_expand(p_target_vtpl, prev_vtpl, compressed_vtpl, i_mismatch, TRUE, FALSE);
        return(*p_target_vtpl);
}


SS_INLINE bool vtpl_expand_sizecheck(
        vtpl_t* target_vtpl,
        vtpl_t* prev_vtpl,
        vtpl_t* compressed_vtpl,
        vtpl_index_t i_mismatch,
        vtpl_index_t prev_grosslen,
        vtpl_index_t comp_grosslen)
{
        bool succp;
        if (i_mismatch == 0) {
            /* uti_expand(target_vtpl, VTPL_EMPTY, compressed_vtpl,
                          0, FALSE, FALSE)
               fails assertion.
            */
            memcpy(target_vtpl, compressed_vtpl, comp_grosslen);
            succp = TRUE;
        } else {
            succp = uti_expand_sizecheck(target_vtpl,
                                         prev_vtpl,
                                         compressed_vtpl,
                                         i_mismatch,
                                         prev_grosslen,
                                         comp_grosslen);
        }
        return (succp);
}


/*##**********************************************************************\
 *
 *		vtpl_search_setcmpfn
 *
 * Sets a special comparisin function for a search state. This comparison
 * function is used in case when v-tuples are equal.
 *
 * Parameters :
 *
 *      p_search_state - in out, use
 *          the search state structure
 *
 *      fn - in, hold
 *          comparison function
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void vtpl_search_setcmpfn(
        search_state* p_search_state,
        int (*fn)(void*, void*))
{
        p_search_state->cmp_fn = fn;
}

/*##**********************************************************************\
 *
 *		vtpl_find_split
 *
 * Given two index entries (the first uncompressed and the second compressed)
 * returns an (uncompressed) entry that falls between the two, and is as
 * short as possible.
 *
 * Parameters :
 *
 *	target_vtpl - out, use
 *		a pointer to a v-tuple to write to an uncompressed index
 *          entry, such that *prev_vtpl < *target_vtpl <= *compressed_vtpl
 *
 *	prev_vtpl - in, use
 *		a pointer to the previous index entry
 *
 *	compressed_vtpl - in, use
 *		a pointer to the compressed entry following prev_vtpl
 *
 *	i_mismatch - in
 *		the i-mismatch index of the first byte in compressed_vtpl
 *
 * Return value - ref :
 *
 *      target_vtpl
 *
 * Limitations  :
 *
 *      *target_vtpl may not overlap the other v-tuples, doesn't
 *      work with empty v-tuples
 *
 * Globals used :
 */
SS_INLINE vtpl_t* vtpl_find_split(
	vtpl_t* target_vtpl,
	vtpl_t* prev_vtpl,
	vtpl_t* compressed_vtpl,
	vtpl_index_t i_mismatch)
{
        uti_expand(target_vtpl, prev_vtpl, compressed_vtpl, i_mismatch, FALSE, TRUE);
        return(target_vtpl);
}

SS_INLINE int vtpl_search_step_sizecheck(
        search_state*   p_search_state,
        vtpl_t*         index_entry,
        void*           index_data,
        void*           floor,
        void*           ceiling)
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
        /* ss_dassert(l_ie != 0); */

        if (p_ie_start < (ss_byte_t*)floor
        ||  p_ie_start > (ss_byte_t*)ceiling)
        {
            return 1000;
        }
        p_ie_end = p_ie_start + l_ie;
        if (p_ie_end < (ss_byte_t*)floor
        ||  p_ie_end > (ss_byte_t*)ceiling)
        {
            return 10000;
        }

        /* Does simple test for the va's: comparing the length and
         * the content as a whole. Not applicable for the mmeg2 where the
         * order is not the only point of interest because the length of the
         * common prefix with the next vtpl is needed too.
         */
        p_ie_data = (ss_byte_t*)va_getdata((va_t*)p_ie_start, &l_ie_field);
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
        ss.i_mismatch = (*p_search_state).i_mismatch;
        ss.p_key_end = (*p_search_state).p_key_end;

        for (;;) { /* Loop over fields */
            /* find mismatch in this field */
            if (p_ie_data > (ss_byte_t*)ceiling) {
                /* Check against floor not needed because
                 * p_ie_data can be at most 4 bytes too far
                 */
                return (1000);
            }
            {
                ss_byte_t* p_ie_field_end = p_ie_data + l_ie_field;
                if (p_ie_field_end > (ss_byte_t*)ceiling
                ||  p_ie_field_end < (ss_byte_t*)floor)
                {
                    return (10000);
                }
            }
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

#endif /* defined(UTI0VCMP_C) || defined(SS_USE_INLINE) */

#endif /* UTI0VCMP_H */
