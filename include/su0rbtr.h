/*************************************************************************\
**  source       * su0rbtr.h
**  directory    * su
**  description  * Balanced Red-Black binary tree.
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


#ifndef SU0RBTR_H     
#define SU0RBTR_H

#include <ssc.h>    /* for ulong */
#include <ssmem.h>
#include <sssem.h>

typedef enum {
        RBT_RED,
        RBT_BLACK
} su_rbt_color_t;

typedef struct su_rbt_st      su_rbt_t;
typedef struct su_rbt_node_st su_rbt_node_t;

/* Structure for a red-black tree.
 */
struct su_rbt_st {
        ss_autotest(long rbt_maxnodes;)
        ss_debug(int   rbt_chk;)                /* check field */
        ss_debug(long  rbt_id;)                 /* check field */
        su_rbt_node_t* rbt_root;                /* tree root node */
        su_rbt_node_t* rbt_nil;                 /* tree nil node */
        ulong          rbt_nelems;              /* # of nodes */
        int            (*rbt_icompare)(         /* key insert compare function */
                            void* key1,
                            void* key2);
        int            (*rbt_scompare)(         /* key search compare function */
                            void* search_key,
                            void* rbt_key);
        void           (*rbt_delete)(           /* key delete function */
                            void* key);
        ss_debug(ss_memobjtype_t rbt_objtype;)
        ss_debug(SsSemT*         rbt_sem;)      /* If not NULL, semaphore protecting
                                                   this tree. Used for debug checks. */
};

su_rbt_t* su_rbt_init(
        int (*rbt_compare)(void* key1, void* key2),
        void (*rbt_delete)(void*));

su_rbt_t* su_rbt_inittwocmp(
        int (*insert_compare)(void* key1, void* key2),
        int (*search_compare)(void* search_key, void* rbt_key),
        void (*rbt_delete)(void*));

#ifdef SS_DEBUG

su_rbt_t* su_rbt_inittwocmp_memobj(
        int (*insert_compare)(void* key1, void* key2),
        int (*search_compare)(void* search_key, void* rbt_key),
        void (*rbt_delete)(void* key),
        ss_memobjtype_t memobjid);

#endif /* SS_DEBUG */

su_rbt_t* su_rbt_initctxcmp(
        int (*rbt_compare)(void* key1, void* key2, void* cmpctx),
        void (*rbt_delete)(void*),
        void* cmp_ctx);

void su_rbt_done(
        su_rbt_t* rbt);

bool su_rbt_insert(
        su_rbt_t* rbt,
        void* key);

su_rbt_node_t* su_rbt_insert2(
        su_rbt_t* rbt,
        void* key);

void* su_rbt_delete(
        su_rbt_t* rbt,
        su_rbt_node_t* z);

void* su_rbt_delete_nodatadel(
        su_rbt_t* rbt,
        su_rbt_node_t* z);

void su_rbt_deleteall(
        su_rbt_t* rbt);

su_rbt_node_t* su_rbt_search(
        su_rbt_t* rbt,
        void* key);

su_rbt_node_t* su_rbt_search_atleast(
        su_rbt_t* rbt,
        void* key);

su_rbt_node_t* su_rbt_search_atmost(
        su_rbt_t* rbt,
        void* key);

su_rbt_node_t* su_rbt_min(
        su_rbt_t* rbt,
        su_rbt_node_t* x);

su_rbt_node_t* su_rbt_max(
        su_rbt_t* rbt,
        su_rbt_node_t* x);

su_rbt_node_t* su_rbt_succ(
        su_rbt_t* rbt,
        su_rbt_node_t* x);

su_rbt_node_t* su_rbt_pred(
        su_rbt_t* rbt,
        su_rbt_node_t* x);


#ifdef AUTOTEST_RUN
void su_rbt_maxnodes(su_rbt_t* rbt, long maxnodes);
#endif

#define _SU_RBT_NELEMS_(rbt) ((rbt)->rbt_nelems)

#ifdef SS_DEBUG
ulong su_rbt_nelems(su_rbt_t* rbt);
#else /* SS_DEBUG */
#define su_rbt_nelems(rbt) _SU_RBT_NELEMS_(rbt)
#endif /* SS_DEBUG */

void* su_rbtnode_getkey(su_rbt_node_t *rbtnode);

int su_rbt_long_compare(long l1, long l2);
int su_rbt_ptr_compare(void* p1, void* p2);

#ifdef SS_DEBUG
bool su_rbt_node_chk(su_rbt_node_t *rbtn);
void su_rbt_setdebugsem(su_rbt_t* rbt, SsSemT* sem);
#endif /* SS_DEBUG */

#endif /* SU0RBTR_H */
