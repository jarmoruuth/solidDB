/*************************************************************************\
**  source       * su0rbtr.c
**  directory    * su
**  description  * Balanced Red-Black binary tree.
**               * Detailed description of the red-black tree algorithm
**               * can be found from the book:
**               * 
**               *      Thomas H. Cormen, Charles E. Leiserson,
**               *      Ronald L. Rivest :
**               *      Introduction to Algorithms,
**               *      MIT Press, 1990
**               *      pp. 263-280
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

The implementation is otherwise the same as in su0rbtr.c but this one
uses macro substitution to instantiate rb-tree for specific type.
The instantiation requires certain macros defined then just include
this file where you want the rb-tree instantiated.

The Macros needed for both declaration and
instantiation of rb-tree for specific type
(also alternatives/optionals presented):

primary         alternative
--------        -----------
RBTKEY_T        RBTSEARCHKEY_T + RBTINSERTKEY_T + RBTNODEKEY_T
RBTKEY_CMP      RBTKEY_SEARCHCMP + RBTKEY_INSERTCMP
RBTKEY_COPYTONODE
SU_RBT_ST
SU_RBT_T
SU_RBT_NODE_ST
SU_RBT_NODE_T

SU_RBT_INIT
SU_RBT_DONE
SU_RBT_INITBUF
SU_RBT_DONEBUF
SU_RBT_INSERT
SU_RBT_INSERT2
SU_RBT_DELETE
SU_RBT_DELETEALL
SU_RBT_SEARCH
SU_RBT_SEARCH_ATLEAST
SU_RBT_SEARCH_ATMOST
SU_RBT_MIN
SU_RBT_MAX
SU_RBT_SUCC
SU_RBT_PRED
SU_RBT_NELEMS / SU_RBT_ISEMPTY
SU_RBTNODE_GETPTRTOKEY

If also SU_RBT_INSTANTIATE is #defined the
the inclusion of this file also creates an instance
of rb-tree implementation for the desired type.

Limitations:
-----------

Only one instance can be created in one source file, because local
(static) functions would have conflicting names between instances.
Multiple instances can, however, be declared in one source file thus
enabling free usage of the instances.

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
#define RBTKEY_T char
#define RBTKEY_CMP(p_cmp, p_k1, p_k2) \
        do { *(p_cmp) = strcmp(p_k1, p_k2); } while (FALSE)
#define RBTKEY_SIZE(p_k) (strlen(p_k) + 1)
#define RBTKEY_COPYTONODE(p_nodekey, p_origkey) \
        do { strcpy(p_nodekey, p_origkey); } while (FALSE)
#define RBT_MEMALLOC(ctx, size) SsMemAlloc(size)
#define RBT_MEMFREE(ctx, p) SsMemFree(p)

#define SU_RBT_ST SS_CONCAT3(su_rbt_struct_for_,char,_t)
#define SU_RBT_T SS_CONCAT3(su_rbt_for_,char,_t)
#define SU_RBT_NODE_T SS_CONCAT3(su_rbt_node_for_,char,_t)
#define SU_RBT_NODE_ST SS_CONCAT3(su_rbt_node_struct_for_,char,_t)


#define SU_RBT_INIT             SS_CONCAT3(su_rbt_for_,char,_init)
#define SU_RBT_DONE             SS_CONCAT3(su_rbt_for_,char,_done)
#define SU_RBT_INITBUF          SS_CONCAT3(su_rbt_for_,char,_initbuf)
#define SU_RBT_DONEBUF          SS_CONCAT3(su_rbt_for_,char,_donebuf)
#define SU_RBT_INSERT           SS_CONCAT3(su_rbt_for_,char,_insert)
#define SU_RBT_INSERT2          SS_CONCAT3(su_rbt_for_,char,_insert2)
#define SU_RBT_DELETE           SS_CONCAT3(su_rbt_for_,char,_delete)
#define SU_RBT_DELETEALL        SS_CONCAT3(su_rbt_for_,char,_deleteall)
#define SU_RBT_SEARCH           SS_CONCAT3(su_rbt_for_,char,_search)
#define SU_RBT_SEARCH_ATLEAST   SS_CONCAT3(su_rbt_for_,char,_search_atleast)
#define SU_RBT_SEARCH_ATMOST    SS_CONCAT3(su_rbt_for_,char,_search_atmost)
#define SU_RBT_MIN              SS_CONCAT3(su_rbt_for_,char,_min)
#define SU_RBT_MAX              SS_CONCAT3(su_rbt_for_,char,_max)
#define SU_RBT_SUCC             SS_CONCAT3(su_rbt_for_,char,_succ)
#define SU_RBT_PRED             SS_CONCAT3(su_rbt_for_,char,_pred)
#define SU_RBT_NELEMS           SS_CONCAT3(su_rbt_for_,char,_nelems)
#define SU_RBTNODE_GETPTRTOKEY  SS_CONCAT3(su_rbtnode_for_,char,_getptrtokey)

#define SU_RBT_INSTANTIATE

#include "../su0rbtgen.h"


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstdio.h>
#include <ssstdlib.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <su/su1check.h>
#include "su0rbtr.h"

#if defined(RBTSEARCHKEY_T)
# if !defined(RBTNODEKEY_T) || !defined(RBTINSERTKEY_T)
#  error if RBTSEARCHKEY_T is #defined, also RBTNODEKEY_T and RBTINSERTKEY_T \
   must be #defined!
   HogiHogi!!!!
# endif
#else /* RBTSEARCHKEY_T */
# if !defined(RBTKEY_T)
#  error RBTKEY_T macro needs to be #defined!
   HogiHogi!!!!
# endif /* RBTKEY_T */
# define RBTSEARCHKEY_T RBTKEY_T
# define RBTNODEKEY_T   RBTKEY_T
#define RBTINSERTKEY_T  RBTKEY_T
#endif /* RBTSEARCHKEY_T */
   
#if !defined(SU_RBT_ST) || !defined(SU_RBT_T) || \
 !defined(SU_RBT_NODE_T) || !defined(SU_RBT_NODE_ST) || \
 !defined(SU_RBT_INIT) || !defined(SU_RBT_DONE) || \
 !defined(SU_RBT_INITBUF) || !defined(SU_RBT_DONEBUF) || \
 !defined(SU_RBT_INSERT) || !defined(SU_RBT_INSERT2) || \
 !defined(SU_RBT_DELETE) || !defined(SU_RBT_DELETEALL) || \
 !defined(SU_RBT_SEARCH) || \
 !defined(SU_RBT_SEARCH_ATLEAST) || !defined(SU_RBT_SEARCH_ATMOST) ||\
 !defined(SU_RBT_MIN) || !defined(SU_RBT_MAX) || \
 !defined(SU_RBT_SUCC) || !defined(SU_RBT_PRED) || \
 !defined(SU_RBTNODE_GETPTRTOKEY) || \
 (!defined(SU_RBT_NELEMS_NOT_NEEDED) && !defined(SU_RBT_NELEMS)) ||\
 (defined(SU_RBT_NELEMS_NOT_NEEDED) && !defined(SU_RBT_ISEMPTY))
#  error All the above macros need to be #defined!
   HogiHogi!!!!
#endif

typedef struct SU_RBT_ST      SU_RBT_T;
typedef struct SU_RBT_NODE_ST SU_RBT_NODE_T;


#ifndef SU_RBT_NELEMS_NOT_NEEDED

ulong SU_RBT_NELEMS(SU_RBT_T* rbt);

#else /* !SU_RBT_NELEMS_NOT_NEEDED */

bool SU_RBT_ISEMPTY(SU_RBT_T* rbt);

#endif /* !SU_RBT_NELEMS_NOT_NEEDED */

RBTNODEKEY_T* SU_RBTNODE_GETPTRTOKEY(SU_RBT_NODE_T *rbtnode);


#define RBT_SHARED_NIL

/* Structure for rbtree node
 */
struct  SU_RBT_NODE_ST {
        ss_debug(int   rn_chk;)  /* check field */
        ss_debug(long  rn_id;)   /* check field */
        SU_RBT_NODE_T* rn_left;  /* used only internally by tree functions */
        SU_RBT_NODE_T* rn_right; /* used only internally by tree functions */
        SU_RBT_NODE_T* rn_p;     /* used only internally by tree functions */
        ss_byte_t      rn_color; /* used only internally by tree functions */
        RBTNODEKEY_T   rn_key;   /* flattened into node structure! */
};


/* Structure for a red-black tree.
 */
struct SU_RBT_ST {
        ss_debug(int   rbt_chk;)                /* check field */
        ss_debug(long  rbt_id;)                 /* check field */
        SU_RBT_NODE_T* rbt_root;                /* tree root node */
#ifndef RBT_SHARED_NIL
        SU_RBT_NODE_T  rbt_nil;                 /* tree nil node */
#endif /* !RBT_SHARED_NIL */
#ifndef SU_RBT_NELEMS_NOT_NEEDED
        ulong          rbt_nelems;              /* # of nodes */
#endif /* SU_RBT_NELEMS_NOT_NEEDED */
        ss_debug(ss_memobjtype_t rbt_objtype;)
};

SU_RBT_T* SU_RBT_INIT(void* memctx);

void SU_RBT_DONE(void* memctx, SU_RBT_T* rbt);

void SU_RBT_INITBUF(void* memctx, SU_RBT_T* rbtbuf);

void SU_RBT_DONEBUF(void* memctx, SU_RBT_T* rbtbuf);

bool SU_RBT_INSERT(
        void* memctx,
        SU_RBT_T* rbt,
        RBTINSERTKEY_T* key);

SU_RBT_NODE_T* SU_RBT_INSERT2(
        void* memctx,
        SU_RBT_T* rbt,
        RBTINSERTKEY_T* key);

void SU_RBT_DELETE(
        void* memctx,
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* z);

void SU_RBT_DELETEALL(
        void* memctx,
        SU_RBT_T* rbt);

SU_RBT_NODE_T* SU_RBT_SEARCH(
        SU_RBT_T* rbt,
        RBTSEARCHKEY_T* key);

SU_RBT_NODE_T* SU_RBT_SEARCH_ATLEAST(
        SU_RBT_T* rbt,
        RBTSEARCHKEY_T* key);

SU_RBT_NODE_T* SU_RBT_SEARCH_ATMOST(
        SU_RBT_T* rbt,
        RBTSEARCHKEY_T* key);

SU_RBT_NODE_T* SU_RBT_MIN(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x);

SU_RBT_NODE_T* SU_RBT_MAX(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x);

SU_RBT_NODE_T* SU_RBT_SUCC(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x);

SU_RBT_NODE_T* SU_RBT_PRED(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x);

#ifndef SU_RBT_NELEMS_NOT_NEEDED
#define _SU_RBT_NELEMS_(rbt) ((rbt)->rbt_nelems)
#endif /* !SU_RBT_NELEMS_NOT_NEEDED */

#define _SU_RBTNODE_GETPTRTOKEY_(rbtn) (&(rbtn)->rn_key)

#ifndef SS_DEBUG
#undef SU_RBT_NELEMS
#undef SU_RBTNODE_GETPTRTOKEY
#ifndef SU_RBT_NELEMS_NOT_NEEDED
#define SU_RBT_NELEMS(rbt)      _SU_RBT_NELEMS_(rbt)
#endif /* !SU_RBT_NELEMS_NOT_NEEDED */
#define SU_RBTNODE_GETPTRTOKEY(rbtn) _SU_RBTNODE_GETPTRTOKEY_(rbtn)
#endif /* !DEBUG */

#ifdef SU_RBT_INSTANTIATE

#if !defined(RBTKEY_SIZE) || !defined(RBTKEY_COPYTONODE) || \
    !defined(RBT_MEMALLOC) || !defined(RBT_MEMFREE)
# error All the above macros need to be #defined!
  HogiHogi!!!!
#endif /* check everything defined!! */

#ifdef RBTKEY_SEARCHCMP
# ifndef RBTKEY_INSERTCMP
#  error If RBTKEY_SEARCHCMP is defined, also RBTKEY_INSERTCMP must be defined!
   HogiHogi!!!!
# endif /* !RBTKEY_INSERTCMP */
#else /* RBTKEY_SEARCHCMP */
# ifndef RBTKEY_CMP
#  error RBTKEY_CMP must be defined or else RBTKEY_SEARCHCMP and RBTKEY_INSERTCMP must be defined!
   HogiHogi!!!!
# endif /* !RBTKEY_CMP */
# define RBTKEY_SEARCHCMP RBTKEY_CMP
# define RBTKEY_INSERTCMP RBTKEY_CMP
#endif /* RBTKEY_SEARCHCMP */

#define CHK_RBT(rbt)          ss_dassert(SS_CHKPTR(rbt) && (rbt)->rbt_chk == SUCHK_RBT)
#define CHK_RBTNODE(rn) \
       ss_rc_dassert(SS_CHKPTR(rn) && (rn)->rn_chk == SUCHK_RBTNODE,\
                     rbt_node_print(rn))
#define CHK_RBTNODEID(rbt,rn) \
       ss_rc_dassert((rn) == &RBT_NIL(rbt) || (rn)->rn_id == (rbt)->rbt_id, rbt_node_print(rn))

 
#ifdef RBT_SHARED_NIL
  
static SU_RBT_NODE_T rbt_nil;
#define  RBT_NIL(rbt) rbt_nil

#else /* RBT_SHARED_NIL */

#define RBT_NIL(rbt) (rbt)->rbt_nil

#endif /* RBT_SHARED_NIL */

ss_debug(static long rbtid;)

static SU_RBT_NODE_T* iterative_tree_search(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x,
        RBTSEARCHKEY_T* k);
static bool tree_insert(SU_RBT_T* rbt, SU_RBT_NODE_T* z);
static SU_RBT_NODE_T* tree_minimum(SU_RBT_T* rbt, SU_RBT_NODE_T* x);
static SU_RBT_NODE_T* tree_maximum(SU_RBT_T* rbt, SU_RBT_NODE_T* x);
static SU_RBT_NODE_T* tree_successor(SU_RBT_T* rbt, SU_RBT_NODE_T* x);
static SU_RBT_NODE_T* tree_predecessor(SU_RBT_T* rbt, SU_RBT_NODE_T* x);
static void left_rotate(SU_RBT_T* rbt, SU_RBT_NODE_T* x);
static void right_rotate(SU_RBT_T* rbt, SU_RBT_NODE_T* x);
static bool rb_insert(SU_RBT_T* rbt, SU_RBT_NODE_T* x);
static void rb_delete_fixup(SU_RBT_T* rbt, SU_RBT_NODE_T* x);
static SU_RBT_NODE_T* rb_delete(SU_RBT_T* rbt, SU_RBT_NODE_T* z);

#ifdef SS_DEBUG
static int rbt_node_print(SU_RBT_NODE_T* x)
{
        SsDbgPrintf("su_rbt_node_print:node=%lx\n", (long)x);
        if (x != NULL) {
            SsDbgPrintf("su_rbt_node_print:chk=%d\n", x->rn_chk);
            SsDbgPrintf("su_rbt_node_print:id=%ld\n", x->rn_id);
        }
        return((int)x);
}
#endif

/*#**********************************************************************\
 * 
 *		rbt_node_alloc
 * 
 * Allocates a new tree node.
 * 
 * Parameters :
 *      memctx - in, use
 *         memory context for RBT_MEMALLOC
 *
 *      key - in, use
 *         pointer to key value 
 * 
 * Return value - give : 
 * 
 *      Pointer to the allocated new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static SU_RBT_NODE_T* rbt_node_alloc(void* memctx, RBTINSERTKEY_T* key)
{
        size_t size;
        SU_RBT_NODE_T* node;

        ss_dassert(key != NULL);
        size =
            ((ss_byte_t*)&((SU_RBT_NODE_T*)NULL)->rn_key -
             (ss_byte_t*)NULL) +
            RBTKEY_SIZE(key);
        node = RBT_MEMALLOC(memctx, size);
        RBTKEY_COPYTONODE(&node->rn_key, key);
        SS_MEMOBJ_INC(SS_MEMOBJ_RBTNODE, SU_RBT_NODE_T);

        ss_debug(node->rn_chk = SUCHK_RBTNODE);

        return(node);
}

/*#**********************************************************************\
 * 
 *		rbt_node_free
 * 
 * Releases a tree node.
 * 
 * Parameters : 
 *		
 *	x - in, take
 *          Node that is released.
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void rbt_node_free(
        void* memctx,
        SU_RBT_NODE_T* x)
{
        CHK_RBTNODE(x);
        SS_MEMOBJ_DEC(SS_MEMOBJ_RBTNODE);
#ifdef RBTKEY_DELETE
        RBTKEY_DELETE(memctx, &x->rn_key);
#endif /* RBTKEY_DELETE */

        RBT_MEMFREE(memctx, x);
}

/*#**********************************************************************\
 * 
 *		iterative_tree_search
 * 
 * Iteratively searches a node with key k from a tree rooted at x.
 * 
 * Parameters : 
 * 
 *	rbt - in, use
 *		tree pointer
 *
 *	x - in, use
 *		root of the search
 *
 *	k - in, use
 *		search key
 * 
 * Return value : 
 * 
 *      Node with key k, or NULL.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static SU_RBT_NODE_T* iterative_tree_search(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x,
        RBTSEARCHKEY_T* k)
{
        int cmp;

        while (x != &RBT_NIL(rbt)) {
            RBTKEY_SEARCHCMP(&cmp, k, &x->rn_key);
            if (cmp == 0) {
                break;
            }
            CHK_RBTNODE(x);
            if (cmp < 0) {
                x = x->rn_left;
            } else {
                x = x->rn_right;
            }
        }
        return(x);
}

/*#***********************************************************************\
 * 
 *		iterative_tree_search_atleast
 * 
 * 
 * 
 * Parameters : 
 * 
 *	rbt - 
 *		
 *		
 *	x - 
 *		
 *		
 *	k - 
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
static SU_RBT_NODE_T* iterative_tree_search_atleast(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x,
        RBTSEARCHKEY_T* k)
{
        int cmp;
        SU_RBT_NODE_T* y;

        while (x != &RBT_NIL(rbt)) {
            RBTKEY_SEARCHCMP(&cmp, k, &x->rn_key);
            if (cmp == 0) {
                break;
            }
            CHK_RBTNODE(x);
            if (cmp < 0) {
                y = x->rn_left;
            } else {
                y = x->rn_right;
            }
            if (y == &RBT_NIL(rbt)) {
                /* Key not found. */
                if (cmp > 0) {
                    x = tree_successor(rbt, x);
                }
                return(x);
            }
            x = y;
        }
        return(x);
}

/*#***********************************************************************\
 * 
 *		iterative_tree_search_atmost
 * 
 * 
 * 
 * Parameters : 
 * 
 *	rbt - 
 *		
 *		
 *	x - 
 *		
 *		
 *	k - 
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
static SU_RBT_NODE_T* iterative_tree_search_atmost(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x,
        RBTSEARCHKEY_T* k)
{
        int cmp;
        SU_RBT_NODE_T* y;

        while (x != &RBT_NIL(rbt)) {
            RBTKEY_SEARCHCMP(&cmp, k, &x->rn_key);
            if (cmp == 0) {
                break;
            }
            CHK_RBTNODE(x);
            if (cmp < 0) {
                y = x->rn_left;
            } else {
                y = x->rn_right;
            }
            if (y == &RBT_NIL(rbt)) {
                /* Key not found. */
                if (cmp < 0) {
                    x = tree_predecessor(rbt, x);
                }
                return(x);
            }
            x = y;
        }
        return(x);
}

/*#**********************************************************************\
 * 
 *		tree_insert
 * 
 * Insert node z to the rbt tree.
 * 
 * Parameters : 
 * 
 *	rbt - in out, use
 *		tree pointer
 *
 *	z - in, hold
 *		new node
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool tree_insert(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* z)
{
        int cmp;
        SU_RBT_NODE_T* x;
        SU_RBT_NODE_T* y;

        y = &RBT_NIL(rbt);
        x = rbt->rbt_root;
        while (x != &RBT_NIL(rbt)) {
            CHK_RBTNODE(x);
            y = x;
            RBTKEY_INSERTCMP(&cmp, &z->rn_key, &x->rn_key);
            if (cmp == 0) {
                return(FALSE);
            } else if (cmp < 0) {
                x = x->rn_left;
            } else {
                x = x->rn_right;
            }
        }
        z->rn_p = y;
        if (y == &RBT_NIL(rbt)) {
            rbt->rbt_root = z;
        } else {
            RBTKEY_INSERTCMP(&cmp, &z->rn_key, &y->rn_key);
            if (cmp == 0) {
                ss_error;
                return(FALSE);
            } else if (cmp < 0) {
                y->rn_left = z;
            } else {
                y->rn_right = z;
            }
        }
        return(TRUE);
}

/*#**********************************************************************\
 * 
 *		tree_minimum
 * 
 * Returns the minimum value of the tree rooted at x.
 * 
 * Parameters : 
 * 
 *	rbt - in
 *		tree pointer
 *
 *	x - in, use
 *		tree root
 * 
 * Return value - ref : 
 * 
 *      Minimum tree node at subtree x.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static SU_RBT_NODE_T* tree_minimum(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        while (x->rn_left != &RBT_NIL(rbt)) {
            CHK_RBTNODE(x);
            x = x->rn_left;
        }
        return(x);
}

/*#**********************************************************************\
 * 
 *		tree_maximum
 * 
 * Returns the maximum value of the tree rooted at x.
 * 
 * Parameters : 
 *
 *	rbt - in
 *		tree pointer
 *
 *	x - in, use
 *		tree root
 *
 * Return value - ref : 
 * 
 *      Maximum tree node at subtree x.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static SU_RBT_NODE_T* tree_maximum(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        while (x->rn_right != &RBT_NIL(rbt)) {
            CHK_RBTNODE(x);
            x = x->rn_right;
        }
        return(x);
}

/*#**********************************************************************\
 * 
 *		tree_successor
 * 
 * Returns the successor node of node x.
 * 
 * Parameters : 
 * 
 *	rbt - in
 *		tree pointer
 *
 *	x - in, use
 *		tree node
 * 
 * Return value - ref : 
 * 
 *      Successor of node x, or NULL.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static SU_RBT_NODE_T* tree_successor(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        SU_RBT_NODE_T* y;

        if (x->rn_right != &RBT_NIL(rbt)) {
            return(tree_minimum(rbt, x->rn_right));
        }
        y = x->rn_p;
        while (y != &RBT_NIL(rbt) && x == y->rn_right) {
            CHK_RBTNODE(y);
            x = y;
            y = y->rn_p;
        }
        return(y);
}

/*#**********************************************************************\
 * 
 *		tree_predecessor
 * 
 * Returns the predecessor node of node x.
 * 
 * Parameters : 
 * 
 *	rbt - in
 *		tree pointer
 *
 *	x - in, use
 *		tree node
 * 
 * Return value - ref : 
 * 
 *      Predecessor of node x, or NULL.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static SU_RBT_NODE_T* tree_predecessor(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        SU_RBT_NODE_T* y;

        if (x->rn_left != &RBT_NIL(rbt)) {
            return(tree_maximum(rbt, x->rn_left));
        }
        y = x->rn_p;
        while (y != &RBT_NIL(rbt) && x == y->rn_left) {
            CHK_RBTNODE(y);
            x = y;
            y = y->rn_p;
        }
        return(y);
}

/*#**********************************************************************\
 * 
 *		left_rotate
 * 
 * Rotates the tree to the left.
 * 
 * Parameters : 
 * 
 *	rbt - in out, use
 *		tree pointer
 *
 *	x - in out, use
 *		node around which the tree is rotated
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void left_rotate(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        SU_RBT_NODE_T* y;

        CHK_RBTNODE(x);
        ss_dassert(x->rn_right != &RBT_NIL(rbt));

        y = x->rn_right;
        CHK_RBTNODE(y);
        x->rn_right = y->rn_left;
        if (y->rn_left != &RBT_NIL(rbt)) {
            y->rn_left->rn_p = x;
        }
        y->rn_p = x->rn_p;
        if (x->rn_p == &RBT_NIL(rbt)) {
            rbt->rbt_root = y;
        } else {
            if (x == x->rn_p->rn_left) {
                x->rn_p->rn_left = y;
            } else {
                x->rn_p->rn_right = y;
            }
        }
        y->rn_left = x;
        x->rn_p = y;
}

/*#**********************************************************************\
 * 
 *		right_rotate
 * 
 * Rotates the tree to the right.
 * 
 * Parameters : 
 * 
 *	rbt - in out, use
 *		tree pointer
 *
 *	x - in out, use
 *		node around which the tree is rotated
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void right_rotate(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        SU_RBT_NODE_T* y;

        CHK_RBTNODE(x);
        ss_dassert(x->rn_left != &RBT_NIL(rbt));

        y = x->rn_left;
        CHK_RBTNODE(y);
        x->rn_left = y->rn_right;
        if (y->rn_right != &RBT_NIL(rbt)) {
            y->rn_right->rn_p = x;
        }
        y->rn_p = x->rn_p;
        if (x->rn_p == &RBT_NIL(rbt)) {
            rbt->rbt_root = y;
        } else {
            if (x == x->rn_p->rn_right) {
                x->rn_p->rn_right = y;
            } else {
                x->rn_p->rn_left = y;
            }
        }
        y->rn_right = x;
        x->rn_p = y;
}

/*#**********************************************************************\
 * 
 *		rb_insert
 * 
 * Inserts node x to the tree and balances the tree if necessary.
 * 
 * Parameters : 
 * 
 *	rbt - in out, use
 *		tree pointer
 *
 *	x - in, hold
 *		new node
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool rb_insert(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        SU_RBT_NODE_T* y;

        if (!tree_insert(rbt, x)) {
            return(FALSE);
        }
        x->rn_color = RBT_RED;
        while (x != rbt->rbt_root && x->rn_p->rn_color == RBT_RED) {
            CHK_RBTNODE(x);
            if (x->rn_p == x->rn_p->rn_p->rn_left) {
                y = x->rn_p->rn_p->rn_right;
                if (y->rn_color == RBT_RED) {
                    x->rn_p->rn_color = RBT_BLACK;
                    y->rn_color = RBT_BLACK;
                    x->rn_p->rn_p->rn_color = RBT_RED;
                    x = x->rn_p->rn_p;
                } else {
                    if (x == x->rn_p->rn_right) {
                        x = x->rn_p;
                        left_rotate(rbt, x);
                    }
                    x->rn_p->rn_color = RBT_BLACK;
                    x->rn_p->rn_p->rn_color = RBT_RED;
                    right_rotate(rbt, x->rn_p->rn_p);
                }
            } else {
                y = x->rn_p->rn_p->rn_left;
                if (y->rn_color == RBT_RED) {
                    x->rn_p->rn_color = RBT_BLACK;
                    y->rn_color = RBT_BLACK;
                    x->rn_p->rn_p->rn_color = RBT_RED;
                    x = x->rn_p->rn_p;
                } else {
                    if (x == x->rn_p->rn_left) {
                        x = x->rn_p;
                        right_rotate(rbt, x);
                    }
                    x->rn_p->rn_color = RBT_BLACK;
                    x->rn_p->rn_p->rn_color = RBT_RED;
                    left_rotate(rbt, x->rn_p->rn_p);
                }
            }
        }
        rbt->rbt_root->rn_color = RBT_BLACK;
        return(TRUE);
}

/*#**********************************************************************\
 * 
 *		rb_delete_fixup
 * 
 * Balances the tree after delete.
 * 
 * Parameters : 
 * 
 *	rbt - in out, use
 *		tree pointer
 *
 *	x - in out, use
 *		deleted node?
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void rb_delete_fixup(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        SU_RBT_NODE_T* w;

        while (x != rbt->rbt_root && x->rn_color == RBT_BLACK) {
            CHK_RBTNODE(x);
            if (x == x->rn_p->rn_left) {
                w = x->rn_p->rn_right;
                if (w->rn_color == RBT_RED) {
                    w->rn_color = RBT_BLACK;
                    x->rn_p->rn_color = RBT_RED;
                    left_rotate(rbt, x->rn_p);
                    w = x->rn_p->rn_right;
                }
                if (w->rn_left->rn_color == RBT_BLACK && w->rn_right->rn_color == RBT_BLACK) {
                    w->rn_color = RBT_RED;
                    x = x->rn_p;
                } else {
                    if (w->rn_right->rn_color == RBT_BLACK) {
                        w->rn_left->rn_color = RBT_BLACK;
                        w->rn_color = RBT_RED;
                        right_rotate(rbt, w);
                        w = x->rn_p->rn_right;
                    }
                    w->rn_color = x->rn_p->rn_color;
                    x->rn_p->rn_color = RBT_BLACK;
                    w->rn_right->rn_color = RBT_BLACK;
                    left_rotate(rbt, x->rn_p);
                    x = rbt->rbt_root;
                }
            } else {
                w = x->rn_p->rn_left;
                if (w->rn_color == RBT_RED) {
                    w->rn_color = RBT_BLACK;
                    x->rn_p->rn_color = RBT_RED;
                    right_rotate(rbt, x->rn_p);
                    w = x->rn_p->rn_left;
                }
                if (w->rn_right->rn_color == RBT_BLACK && w->rn_left->rn_color == RBT_BLACK) {
                    w->rn_color = RBT_RED;
                    x = x->rn_p;
                } else {
                    if (w->rn_left->rn_color == RBT_BLACK) {
                        w->rn_right->rn_color = RBT_BLACK;
                        w->rn_color = RBT_RED;
                        left_rotate(rbt, w);
                        w = x->rn_p->rn_left;
                    }
                    w->rn_color = x->rn_p->rn_color;
                    x->rn_p->rn_color = RBT_BLACK;
                    w->rn_left->rn_color = RBT_BLACK;
                    right_rotate(rbt, x->rn_p);
                    x = rbt->rbt_root;
                }
            }
        }
        x->rn_color = RBT_BLACK;
}

/*#**********************************************************************\
 * 
 *		rb_delete
 * 
 * Deletes node from z from the tree and balances the tree if necessary.
 * 
 * Parameters : 
 *
 *	rbt - in out, use
 *		tree pointer
 *
 *	z - in out, use
 *		deleted node
 *
 * Return value - give : 
 * 
 *      The node that is removed from the tree. It may be different
 *      than input parameter z.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static SU_RBT_NODE_T* rb_delete(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* z)
{
        SU_RBT_NODE_T* x;
        SU_RBT_NODE_T* y;
        su_rbt_color_t y_color;

        if (z->rn_left == &RBT_NIL(rbt) || z->rn_right == &RBT_NIL(rbt)) {
            y = z;
        } else {
            y = tree_successor(rbt, z);
        }
        if (y->rn_left != &RBT_NIL(rbt)) {
            x = y->rn_left;
        } else {
            x = y->rn_right;
        }
        x->rn_p = y->rn_p;
        if (y->rn_p == &RBT_NIL(rbt)) {
            rbt->rbt_root = x;
        } else {
            if (y == y->rn_p->rn_left) {
                y->rn_p->rn_left = x;
            } else {
                y->rn_p->rn_right = x;
            }
        }
        y_color = y->rn_color;
        if (y != z) {
#if 0
            z->rn_key = y->rn_key;
#else
            /* Update parent and child pointers. */
            if (z->rn_p->rn_left == z) {
                z->rn_p->rn_left = y;
            }
            if (z->rn_p->rn_right == z) {
                z->rn_p->rn_right = y;
            }
            if (z->rn_left->rn_p == z) {
                z->rn_left->rn_p = y;
            }
            if (z->rn_right->rn_p == z) {
                z->rn_right->rn_p = y;
            }
            /* Copy other fields but the key field. */
            y->rn_left = z->rn_left;
            y->rn_right = z->rn_right;
            y->rn_p = z->rn_p;
            y->rn_color = z->rn_color;

            if (rbt->rbt_root == z) {
                rbt->rbt_root = y;
            }

            y = z;
#endif
        }
        if (y_color == RBT_BLACK) {
            rb_delete_fixup(rbt, x);
        }
        return(y);
}

/*##**********************************************************************\
 * 
 *		su_rbt_initbuf
 * 
 * Initializes a red-black tree into an existing buffer 
 * 
 * Parameters : 
 * 
 *      memctx - in, use
 *          memory context to be given as first argument to RBT_MEMALLOC
 *
 *      rbtbuf - in out, use
 *          pointer to structure where the rb-tree is initialized
 *
 * Return value : 
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
void SU_RBT_INITBUF(
        void* memctx,
        SU_RBT_T* rbtbuf)
{
        SU_RBT_NODE_T* nil;

        nil = &RBT_NIL(rbtbuf);
        nil->rn_left = nil;
        nil->rn_right = nil;
        nil->rn_p = nil;
        nil->rn_color = RBT_BLACK;
        ss_debug(nil->rn_chk = SUCHK_RBTNODE);

        ss_debug(rbtbuf->rbt_chk = SUCHK_RBT);
        ss_debug(rbtbuf->rbt_id = rbtid++);
        rbtbuf->rbt_root = nil;
#ifndef SU_RBT_NELEMS_NOT_NEEDED
        rbtbuf->rbt_nelems = 0L;
#endif /* !SU_RBT_NELEMS_NOT_NEEDED */
        ss_debug(rbtbuf->rbt_objtype = SS_MEMOBJ_RBT);
        ss_debug(nil->rn_id = rbtbuf->rbt_id);

        CHK_RBT(rbtbuf);
        CHK_RBTNODE(nil);
        CHK_RBTNODEID(rbtbuf, nil);
}

/*##**********************************************************************\
 * 
 *		su_rbt_donebuf
 * 
 * Releases resources allocated for a red-back tree. After this function
 * the tree cannot be used any more. This versions does not free
 * the structure pointed to by rbtbuf
 * 
 * Parameters : 
 *
 *      memctx - in, use
 *          memory context to be given as first parameter to
 *          RBT_MEMFREE
 *
 *      rbtbuf - in, (take)
 *          pointer to the tree structure.
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void SU_RBT_DONEBUF(
        void* memctx,
        SU_RBT_T* rbtbuf)
{
        SU_RBT_NODE_T* x;

        CHK_RBT(rbtbuf);

        x = tree_minimum(rbtbuf, rbtbuf->rbt_root);
        while (x != &RBT_NIL(rbtbuf)) {
            x = rb_delete(rbtbuf, x);
            rbt_node_free(memctx, x);
            x = tree_minimum(rbtbuf, rbtbuf->rbt_root);
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_init
 * 
 * Initializes a red-black tree 
 * 
 * Parameters : 
 * 
 *      memctx - in, use
 *          memory context to be given as first argument to RBT_MEMALLOC
 *
 * Return value : 
 *
 *      Pointer to a tree structure.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SU_RBT_T* SU_RBT_INIT(
        void* memctx)
{
        SU_RBT_T* rbt;

        SS_MEMOBJ_INC(SS_MEMOBJ_RBT, SU_RBT_T);
        rbt = RBT_MEMALLOC(memctx, sizeof(SU_RBT_T));

        SU_RBT_INITBUF(memctx, rbt);
        return(rbt);
}


/*##**********************************************************************\
 * 
 *		su_rbt_done
 * 
 * Releases resources allocated for a red-back tree. After this function
 * the tree cannot be used any more.
 * 
 * Parameters : 
 *
 *      memctx - in, use
 *          memory context to be given as first parameter to
 *          RBT_MEMFREE
 *
 *      rbt - in, take
 *          pointer to the tree structure
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void SU_RBT_DONE(
        void* memctx,
        SU_RBT_T* rbt)
{
        CHK_RBT(rbt);
        SS_MEMOBJ_DEC(SS_MEMOBJ_RBT);

        SU_RBT_DONEBUF(memctx, rbt);
        RBT_MEMFREE(memctx, rbt);
}

/*##**********************************************************************\
 * 
 *		su_rbt_insert
 * 
 * Inserts a new key to the tree.
 * 
 * Parameters : 
 * 
 *	rbt - in out, use
 *		pointer to the tree structure
 *
 *	key - in, hold
 *		new key 
 *
 * Return value: 
 *
 *      TRUE if insert succeeded
 *      FALSE if insert failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SU_RBT_INSERT(
        void* memctx,
        SU_RBT_T* rbt,
        RBTINSERTKEY_T* key)
{
        SU_RBT_NODE_T* x;

        CHK_RBT(rbt);
        
        x = rbt_node_alloc(memctx, key);
        ss_debug(x->rn_id = rbt->rbt_id);
        x->rn_left = &RBT_NIL(rbt);
        x->rn_right = &RBT_NIL(rbt);

        if (rb_insert(rbt, x)) {
            CHK_RBTNODE(x);
#ifndef SU_RBT_NELEMS_NOT_NEEDED
            rbt->rbt_nelems++;
#endif /* !SU_RBT_NELEMS_NOT_NEEDED */
            return(TRUE);
        } else {
            rbt_node_free(memctx, x);
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_insert2
 * 
 * Inserts a new key to the tree.
 * 
 * Parameters : 
 * 
 *	rbt - in out, use
 *		pointer to the tree structure
 *
 *	key - in, hold
 *		new key 
 *
 * Return value: 
 *
 *      New node or NULL.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SU_RBT_NODE_T* SU_RBT_INSERT2(
        void* memctx,
        SU_RBT_T* rbt,
        RBTINSERTKEY_T* key)
{
        SU_RBT_NODE_T* x;

        CHK_RBT(rbt);
        
        x = rbt_node_alloc(memctx, key);
        ss_debug(x->rn_id = rbt->rbt_id);
        x->rn_left = &RBT_NIL(rbt);
        x->rn_right = &RBT_NIL(rbt);

        if (rb_insert(rbt, x)) {
            CHK_RBTNODE(x);
#ifndef SU_RBT_NELEMS_NOT_NEEDED
            rbt->rbt_nelems++;
#endif /* !SU_RBT_NELEMS_NOT_NEEDED */
            return(x);
        } else {
            rbt_node_free(memctx, x);
            return(NULL);
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_delete
 * 
 * Deletes a tree node. The node must be a pointer to a node structure
 * returned by functions su_rbt_search, su_rbt_min, su_rbt_max,
 * su_rbt_succ, su_rbt_pred or rbt_insert.
 * 
 * Parameters : 
 * 
 *	rbt - in out, use
 *		pointer to the tree structure
 *
 *	z - in, take
 *		node which is deleted
 *
 * Return value - give : 
 * 
 *      NULL, if delete function is specified in rbt_init, or
 *      key value of the deleted node
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void SU_RBT_DELETE(
        void* memctx,
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* z)
{
        CHK_RBT(rbt);
        CHK_RBTNODE(z);
        CHK_RBTNODEID(rbt, z);

        z = rb_delete(rbt, z);
#ifndef SU_RBT_NELEMS_NOT_NEEDED
        rbt->rbt_nelems--;
#endif /* !SU_RBT_NELEMS_NOT_NEEDED */
        rbt_node_free(memctx, z);
}

/*##**********************************************************************\
 * 
 *		su_rbt_search
 * 
 * Searched given key from the tree.
 * 
 * Parameters : 
 * 
 *	rbt - in, use
 *		pointer to the tree structure
 *
 *	key - in, use
 *		key which is searched
 *
 * Return value - ref : 
 *
 *      NULL, if key is not found, or
 *      pointer to a node structure containing the key
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SU_RBT_NODE_T* SU_RBT_SEARCH(
        SU_RBT_T* rbt,
        RBTSEARCHKEY_T* key)
{
        SU_RBT_NODE_T* x;

        CHK_RBT(rbt);
        
        x = iterative_tree_search(rbt, rbt->rbt_root, key);
        CHK_RBTNODE(x);

        if (x == &RBT_NIL(rbt)) {
            return(NULL);
        } else {
            return(x);
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_search_atleast
 * 
 * 
 * 
 * Parameters : 
 * 
 *	rbt - 
 *		
 *		
 *	key - 
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
SU_RBT_NODE_T* SU_RBT_SEARCH_ATLEAST(
        SU_RBT_T* rbt,
        RBTSEARCHKEY_T* key)
{
        SU_RBT_NODE_T* x;

        CHK_RBT(rbt);
        
        x = iterative_tree_search_atleast(rbt, rbt->rbt_root, key);
        CHK_RBTNODE(x);

        if (x == &RBT_NIL(rbt)) {
            return(NULL);
        } else {
            return(x);
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_search_atmost
 * 
 * 
 * 
 * Parameters : 
 * 
 *	rbt - 
 *		
 *		
 *	key - 
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
SU_RBT_NODE_T* SU_RBT_SEARCH_ATMOST(
        SU_RBT_T* rbt,
        RBTSEARCHKEY_T* key)
{
        SU_RBT_NODE_T* x;

        CHK_RBT(rbt);
        
        x = iterative_tree_search_atmost(rbt, rbt->rbt_root, key);
        CHK_RBTNODE(x);

        if (x == &RBT_NIL(rbt)) {
            return(NULL);
        } else {
            return(x);
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_min
 * 
 * Returns the smallest key in the tree.
 * 
 * Parameters : 
 * 
 *	rbt - in, use
 *		pointer to the tree structure
 *
 *	x - in, use
 *		pointer to the subtree the minimum of which is returned.
 *          If this is NULL, then the minimum of the whole tree is
 *          returned.
 *
 * Return value - ref : 
 *
 *      NULL, if tree is empty, or
 *      pointer to a node structure containing the smallest key
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SU_RBT_NODE_T* SU_RBT_MIN(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        CHK_RBT(rbt);
        
        if (x == NULL) {
            x = rbt->rbt_root;
        }
        CHK_RBTNODE(x);
        CHK_RBTNODEID(rbt, x);

        if (x == &RBT_NIL(rbt)) {
            return(NULL);
        } else {
            return(tree_minimum(rbt, x));
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_max
 * 
 * Returns the largest key in the tree.
 * 
 * Parameters : 
 * 
 *	rbt - in, use
 *		pointer to the tree structure
 *
 *	x - in, use
 *		pointer to the subtree the maximum of which is returned.
 *          If this is NULL, then the maximum of the whole tree is
 *          returned.
 *
 * Return value - ref : 
 *
 *      NULL, if tree is empty, or
 *      pointer to a node structure containing the largest key
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SU_RBT_NODE_T* SU_RBT_MAX(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        CHK_RBT(rbt);
        
        if (x == NULL) {
            x = rbt->rbt_root;
        }
        CHK_RBTNODE(x);
        CHK_RBTNODEID(rbt, x);

        if (x == &RBT_NIL(rbt)) {
            return(NULL);
        } else {
            return(tree_maximum(rbt, x));
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_succ
 * 
 * Returns a successor of a node.
 * 
 * Parameters : 
 * 
 *	rbt - in, use
 *		pointer to the tree structure
 *
 *	x - in, use
 *		node the successor of which is returned
 *
 * Return value - ref : 
 *
 *      NULL, if no successor, or
 *      pointer to a node structure containing the node successor
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SU_RBT_NODE_T* SU_RBT_SUCC(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        CHK_RBT(rbt);
        CHK_RBTNODE(x);
        CHK_RBTNODEID(rbt, x);
        
        x = tree_successor(rbt, x);
        CHK_RBTNODE(x);

        if (x == &RBT_NIL(rbt)) {
            return(NULL);
        } else {
            return(x);
        }
}

/*##**********************************************************************\
 * 
 *		su_rbt_pred
 * 
 * Returns a predecessor of a node.
 * 
 * Parameters : 
 * 
 *	rbt - in, use
 *		pointer to the tree structure
 *
 *	x - in, use
 *		node the predecessor of which is returned
 *
 * Return value - ref : 
 *
 *      NULL, if no predecessor, or
 *      pointer to a node structure containing the node predecessor
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SU_RBT_NODE_T* SU_RBT_PRED(
        SU_RBT_T* rbt,
        SU_RBT_NODE_T* x)
{
        CHK_RBT(rbt);
        CHK_RBTNODE(x);
        CHK_RBTNODEID(rbt, x);
        
        x = tree_predecessor(rbt, x);
        CHK_RBTNODE(x);

        if (x == &RBT_NIL(rbt)) {
            return(NULL);
        } else {
            return(x);
        }
}

#ifdef SU_RBT_NELEMS_NOT_NEEDED
bool SU_RBT_ISEMPTY(SU_RBT_T* rbt)
{
        CHK_RBT(rbt);
        return (rbt->rbt_root == &RBT_NIL(rbt));
}
#endif /* SU_RBT_NELEMS_NOT_NEEDED */

#if defined(SS_DEBUG)

# ifndef SU_RBT_NELEMS_NOT_NEEDED
/*##**********************************************************************\
 * 
 *		su_rbt_nelems
 * 
 * Gets the number of tree elements
 * 
 * Parameters : 
 * 
 *	rbt - in, use
 *		pointer to the tree structure
 *
 * Return value : 
 * 
 *      number of tree elements
 * 
 * Limitations  :
 * 
 * Globals used :
 */
ulong SU_RBT_NELEMS(
        SU_RBT_T* rbt)
{
        CHK_RBT(rbt);

        return(_SU_RBT_NELEMS_(rbt));
}
# endif /* !SU_RBT_NELEMS_NOT_NEEDED */

/*##**********************************************************************\
 * 
 *		su_rbtnode_getptrtokey
 * 
 * Gets key (= key + data) value from rbtree node
 * 
 * Parameters : 
 * 
 *	rbtn - in, use
 *		pointer to rbtree node
 *
 * Return value - ref : 
 *      
 *      pointer to user supplied key
 * 
 * Limitations  :
 * 
 * Globals used :
 */
RBTNODEKEY_T *SU_RBTNODE_GETPTRTOKEY(
        SU_RBT_NODE_T *rbtn)
{
        CHK_RBTNODE(rbtn);
        
        return (_SU_RBTNODE_GETPTRTOKEY_(rbtn));
}
#endif /* SS_DEBUG */

#endif /* SU_RBT_INSTANTIATE */
