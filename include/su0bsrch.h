/*************************************************************************\
**  source       * su0bsrch.h
**  directory    * su
**  description  * su_bsearch() function, which gives pointer
**               * for the correct place for the search key in the
**               * array even if the key was not found
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


#ifndef SU0BSRCH_H
#define SU0BSRCH_H

#include <ssc.h>
#include <ssstddef.h>
#include <ssstdlib.h>

typedef int su_bsearch_cmpfunc_t(const void *keyp, const void *elemp);
typedef su_bsearch_cmpfunc_t* su_bsearch_cmpfuncptr_t;

bool su_bsearch(
        void *key,              /* ptr to search key */
        void *base,             /* ptr to array */
        size_t n,               /* number of array elements in search range */
        size_t elsize,          /* size of array element */
        su_bsearch_cmpfuncptr_t
            cmp,                /* int (*comparison)(void *keyp, void *elemp) */
        void **retpp);          /* ptr to returned array element address */

typedef int su_bsearch_ctxcmpfunc_t(const void *keyp, const void *elemp, void* ctx);
typedef su_bsearch_ctxcmpfunc_t* su_bsearch_ctxcmpfuncptr_t;

bool su_bsearch_ctx(
        void *key,              /* ptr to search key */
        void *base,             /* ptr to array */
        size_t n,               /* number of array elements in search range */
        size_t elsize,          /* size of array element */
        su_bsearch_ctxcmpfuncptr_t
            cmp,                /* int (*comparison)(void *keyp, void *elemp, void* ctx) */
        void* ctx,              /* Comparison context (eg. Asc/Desc conditions) */
        void **retpp);          /* ptr to returned array element address */


/* note CMP can be a macro or function of type:
 * void cmpexample(int* cmpres, void* key, void* elem, void* ctx)
 * The idea between this instantiation is to avoid function
 * pointer calls which are known to be slow.
 */

#define SU_BSEARCH_INSTANTIATE(TYPE,CMP)                                      \
bool SS_CONCAT(su_bsearch_ctx_for_,TYPE)(                                     \
        void *key,                                                            \
        void *base,                                                           \
        size_t n,                                                             \
        size_t elsize,                                                        \
        void* ctx,                                                            \
        void **retpp)                                                         \
{                                                                             \
        void *try;          /* pointer to element under test */               \
        size_t n_per_2;                                                       \
        int cmpv;           /* comparison value */                            \
                                                                              \
        ss_dassert(key != NULL);                                              \
        ss_dassert(base != NULL);                                             \
                                                                              \
        while (n) {                                                           \
            n_per_2 = n >> 1;                                                 \
            try = (char*)base + n_per_2 * elsize;                             \
            CMP(&cmpv, key, try, ctx);                                        \
            if (cmpv < 0) {                                                   \
                n = n_per_2;                                                  \
            } else if (cmpv > 0) {                                            \
                base = (char*)try + elsize;                                   \
                n -= n_per_2 + 1;                                             \
            } else {	/* cmpv == 0 */                                       \
                if (retpp != NULL) {                                          \
                    *retpp = try;                                             \
                }                                                             \
                return TRUE;                                                  \
            }                                                                 \
        }                                                                     \
        if (retpp != NULL) {                                                  \
            *retpp = base;                                                    \
        }                                                                     \
        return FALSE;                                                         \
}

/* bsearch() replacement for certain environments.
 * All modules calling bsearch() should include this header su0bsrch.h
 * to ensure that replacement takes place.
 *
 */
void* su_bsearch_replacement(
        void *key,              /* ptr to search key */
        void *base,             /* ptr to array */
        size_t n,               /* number of array elements in search range */
        size_t elsize,          /* size of array element */
        su_bsearch_cmpfuncptr_t
            cmp);                /* int (*comparison)(void *keyp, void *elemp) */

#endif /* SU0BSRCH_H */
