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

#include <ssstdio.h>
#include <ssstdlib.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include "su1check.h"
#include "su0rbtr.h"

#define chk_rbtsem(rbt)       (rbt->rbt_sem == NULL || SsSemThreadIsEntered(rbt->rbt_sem))

#ifdef AUTOTEST_RUN
# define CHK_RBT(rbt)          ss_dassert(SS_CHKPTR(rbt) && (rbt)->rbt_chk == SUCHK_RBT && chk_rbtsem(rbt)); ss_info_assert((rbt)->rbt_nelems < (rbt)->rbt_maxnodes, ("Maximum number of nodes (%ld) ecxeeded", (rbt)->rbt_maxnodes))
#else
# define CHK_RBT(rbt)          ss_dassert(SS_CHKPTR(rbt) && (rbt)->rbt_chk == SUCHK_RBT && chk_rbtsem(rbt))
#endif

#define CHK_RBTNODE(rn)       ss_rc_dassert(SS_CHKPTR(rn) && (rn)->rn_chk == SUCHK_RBTNODE, rbt_node_print(rn))
#define CHK_RBTNODEID(rbt,rn) ss_rc_dassert((rn)->rn_id == (rbt)->rbt_id, rbt_node_print(rn))

/* Structure for rbtree node
 */
struct su_rbt_node_st {
        ss_debug(int   rn_chk;)  /* check field */
        ss_debug(long  rn_id;)   /* check field */
        su_rbt_node_t* rn_left;  /* used only internally by tree functions */
        su_rbt_node_t* rn_right; /* used only internally by tree functions */
        su_rbt_node_t* rn_p;     /* used only internally by tree functions */
        su_rbt_color_t rn_color; /* used only internally by tree functions */
        void*          rn_key;   /* user given key of the node */
};

ss_debug(static long rbtid;)

static su_rbt_node_t* su_rbt_node_alloc(void);
static void su_rbt_node_free(su_rbt_node_t* x);
static su_rbt_node_t* iterative_tree_search(su_rbt_t* rbt, su_rbt_node_t* x, void* k);
static bool tree_insert(su_rbt_t* rbt, su_rbt_node_t* z);
static su_rbt_node_t* tree_minimum(su_rbt_t* rbt, su_rbt_node_t* x);
static su_rbt_node_t* tree_maximum(su_rbt_t* rbt, su_rbt_node_t* x);
static su_rbt_node_t* tree_successor(su_rbt_t* rbt, su_rbt_node_t* x);
static su_rbt_node_t* tree_predecessor(su_rbt_t* rbt, su_rbt_node_t* x);
static void left_rotate(su_rbt_t* rbt, su_rbt_node_t* x);
static void right_rotate(su_rbt_t* rbt, su_rbt_node_t* x);
static bool rb_insert(su_rbt_t* rbt, su_rbt_node_t* x);
static void rb_delete_fixup(su_rbt_t* rbt, su_rbt_node_t* x);
static su_rbt_node_t* rb_delete(su_rbt_t* rbt, su_rbt_node_t* z);

#ifdef SS_DEBUG
static int rbt_node_print(su_rbt_node_t* x)
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
 *		su_rbt_node_alloc
 * 
 * Allocates a new tree node.
 * 
 * Parameters : 
 * 
 * Return value - give : 
 * 
 *      Pointer to the allocated new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static su_rbt_node_t* su_rbt_node_alloc()
{
        su_rbt_node_t* node;

        SS_MEMOBJ_INC(SS_MEMOBJ_RBTNODE, su_rbt_node_t);

        node = SSMEM_NEW(su_rbt_node_t);

        ss_debug(node->rn_chk = SUCHK_RBTNODE);

        return(node);
}

/*#**********************************************************************\
 * 
 *		su_rbt_node_free
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
static void su_rbt_node_free(x)
	su_rbt_node_t* x;
{
        CHK_RBTNODE(x);
        SS_MEMOBJ_DEC(SS_MEMOBJ_RBTNODE);

        SsMemFree(x);
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
static su_rbt_node_t* iterative_tree_search(rbt, x, k)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
	void* k;
{
        int cmp;

        while (x != rbt->rbt_nil && (cmp = (*rbt->rbt_scompare)(k, x->rn_key)) != 0) {
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
static su_rbt_node_t* iterative_tree_search_atleast(
	su_rbt_t* rbt,
	su_rbt_node_t* x,
	void* k)
{
        int cmp;
	su_rbt_node_t* y;

        while (x != rbt->rbt_nil
               && (cmp = (*rbt->rbt_scompare)(k, x->rn_key)) != 0) {
            CHK_RBTNODE(x);
            if (cmp < 0) {
                y = x->rn_left;
            } else {
                y = x->rn_right;
            }
            if (y == rbt->rbt_nil) {
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
static su_rbt_node_t* iterative_tree_search_atmost(
	su_rbt_t* rbt,
	su_rbt_node_t* x,
	void* k)
{
        int cmp;
	su_rbt_node_t* y;

        while (x != rbt->rbt_nil
               && (cmp = (*rbt->rbt_scompare)(k, x->rn_key)) != 0) {
            CHK_RBTNODE(x);
            if (cmp < 0) {
                y = x->rn_left;
            } else {
                y = x->rn_right;
            }
            if (y == rbt->rbt_nil) {
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
static bool tree_insert(rbt, z)
	su_rbt_t* rbt;
	su_rbt_node_t* z;
{
        int cmp;
        su_rbt_node_t* x;
        su_rbt_node_t* y;

        y = rbt->rbt_nil;
        x = rbt->rbt_root;
        while (x != rbt->rbt_nil) {
            CHK_RBTNODE(x);
            y = x;
            cmp = (*rbt->rbt_icompare)(z->rn_key, x->rn_key);
            if (cmp == 0) {
                return(FALSE);
            } else if (cmp < 0) {
                x = x->rn_left;
            } else {
                x = x->rn_right;
            }
        }
        z->rn_p = y;
        if (y == rbt->rbt_nil) {
            rbt->rbt_root = z;
        } else {
            cmp = (*rbt->rbt_icompare)(z->rn_key, y->rn_key);
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
static su_rbt_node_t* tree_minimum(rbt, x)
        su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        while (x->rn_left != rbt->rbt_nil) {
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
static su_rbt_node_t* tree_maximum(rbt, x)
        su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        while (x->rn_right != rbt->rbt_nil) {
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
static su_rbt_node_t* tree_successor(rbt, x)
        su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        su_rbt_node_t* y;

        if (x->rn_right != rbt->rbt_nil) {
            return(tree_minimum(rbt, x->rn_right));
        }
        y = x->rn_p;
        while (y != rbt->rbt_nil && x == y->rn_right) {
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
static su_rbt_node_t* tree_predecessor(rbt, x)
        su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        su_rbt_node_t* y;

        if (x->rn_left != rbt->rbt_nil) {
            return(tree_maximum(rbt, x->rn_left));
        }
        y = x->rn_p;
        while (y != rbt->rbt_nil && x == y->rn_left) {
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
static void left_rotate(rbt, x)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        su_rbt_node_t* y;

        CHK_RBTNODE(x);
        ss_dassert(x->rn_right != rbt->rbt_nil);

        y = x->rn_right;
        CHK_RBTNODE(y);
        x->rn_right = y->rn_left;
        if (y->rn_left != rbt->rbt_nil) {
            y->rn_left->rn_p = x;
        }
        y->rn_p = x->rn_p;
        if (x->rn_p == rbt->rbt_nil) {
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
static void right_rotate(rbt, x)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        su_rbt_node_t* y;

        CHK_RBTNODE(x);
        ss_dassert(x->rn_left != rbt->rbt_nil);

        y = x->rn_left;
        CHK_RBTNODE(y);
        x->rn_left = y->rn_right;
        if (y->rn_right != rbt->rbt_nil) {
            y->rn_right->rn_p = x;
        }
        y->rn_p = x->rn_p;
        if (x->rn_p == rbt->rbt_nil) {
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
static bool rb_insert(rbt, x)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        su_rbt_node_t* y;

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
static void rb_delete_fixup(rbt, x)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        su_rbt_node_t* w;

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
static su_rbt_node_t* rb_delete(rbt, z)
	su_rbt_t* rbt;
	su_rbt_node_t* z;
{
        su_rbt_node_t* x;
        su_rbt_node_t* y;
        su_rbt_color_t y_color;

        if (z->rn_left == rbt->rbt_nil || z->rn_right == rbt->rbt_nil) {
            y = z;
        } else {
            y = tree_successor(rbt, z);
        }
        if (y->rn_left != rbt->rbt_nil) {
            x = y->rn_left;
        } else {
            x = y->rn_right;
        }
        x->rn_p = y->rn_p;
        if (y->rn_p == rbt->rbt_nil) {
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
 *		su_rbt_inittwocmp
 * 
 * Initializes a red-black tree with separate compare functions
 * for searches and for inserts.
 * 
 * Parameters : 
 * 
 *      insert_compare - in, hold
 *		Function used to compare tree elements in inserts.
 *
 *      search_compare - in, hold
 *		Function used to compare tree elements in searches.
 *
 *      delete - in, hold
 *		Function used to delete tree elements. This can
 *          be NULL.
 *
 * Return value : 
 *
 *      Pointer to a tree structure.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_rbt_t* su_rbt_inittwocmp(insert_compare, search_compare, delete)
        int (*insert_compare)(void* key1, void* key2);
        int (*search_compare)(void* search_key, void* rbt_key);
        void (*delete)(void* key);
{
        su_rbt_t* rbt;
        su_rbt_node_t* nil;

        ss_dassert(insert_compare != NULL);
        ss_dassert(search_compare != NULL);
        SS_MEMOBJ_INC(SS_MEMOBJ_RBT, su_rbt_t);

        nil = su_rbt_node_alloc();
        nil->rn_left = nil;
        nil->rn_right = nil;
        nil->rn_p = nil;
        nil->rn_color = RBT_BLACK;
        nil->rn_key = NULL;

        rbt = SSMEM_NEW(su_rbt_t);

        ss_debug(rbt->rbt_chk = SUCHK_RBT);
        ss_debug(rbt->rbt_id = rbtid++);
        rbt->rbt_root = nil;
        rbt->rbt_nil = nil;
        rbt->rbt_nelems = 0L;
        rbt->rbt_icompare = insert_compare;
        rbt->rbt_scompare = search_compare;
        rbt->rbt_delete = delete;
        ss_debug(rbt->rbt_objtype = SS_MEMOBJ_RBT);
        ss_debug(rbt->rbt_sem = NULL);
        ss_debug(nil->rn_id = rbt->rbt_id);
        ss_autotest(rbt->rbt_maxnodes = 15000);

        CHK_RBT(rbt);
        CHK_RBTNODE(nil);
        CHK_RBTNODEID(rbt, nil);

        return(rbt);
}

#ifdef SS_DEBUG

su_rbt_t* su_rbt_inittwocmp_memobj(insert_compare, search_compare, delete, memobjid)
        int (*insert_compare)(void* key1, void* key2);
        int (*search_compare)(void* search_key, void* rbt_key);
        void (*delete)(void* key);
        ss_memobjtype_t memobjid;
{
        su_rbt_t* rbt;
        su_rbt_node_t* nil;

        ss_dassert(insert_compare != NULL);
        ss_dassert(search_compare != NULL);
        SS_MEMOBJ_INC(memobjid, su_rbt_t);

        nil = su_rbt_node_alloc();
        nil->rn_left = nil;
        nil->rn_right = nil;
        nil->rn_p = nil;
        nil->rn_color = RBT_BLACK;
        nil->rn_key = NULL;

        rbt = SSMEM_NEW(su_rbt_t);

        ss_debug(rbt->rbt_chk = SUCHK_RBT);
        rbt->rbt_root = nil;
        rbt->rbt_nil = nil;
        rbt->rbt_nelems = 0L;
        rbt->rbt_icompare = insert_compare;
        rbt->rbt_scompare = search_compare;
        rbt->rbt_delete = delete;
        ss_debug(rbt->rbt_objtype = memobjid);
        ss_debug(rbt->rbt_sem = NULL);

        CHK_RBT(rbt);
        
        return(rbt);
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		su_rbt_init
 * 
 * Initializes a red-black tree.
 * 
 * Parameters : 
 * 
 *      compare - in, hold
 *		Function used to compare tree elements.
 *
 *      delete - in, hold
 *		Function used to delete tree elements. This can
 *          be NULL.
 *
 * Return value : 
 * 
 *      Pointer to a tree structure.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_rbt_t* su_rbt_init(compare, delete)
        int (*compare)(void* key1, void* key2);
        void (*delete)(void* key);
{
        su_rbt_t* rbt;

        rbt = su_rbt_inittwocmp(compare, compare, delete);
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
 *	rbt - in, take
 *		pointer to the tree structure
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_rbt_done(rbt)
	su_rbt_t* rbt;
{
        su_rbt_node_t* x;

        CHK_RBT(rbt);
        SS_MEMOBJ_DEC(SS_MEMOBJ_RBT);

        x = tree_minimum(rbt, rbt->rbt_root);
        while (x != rbt->rbt_nil) {
            if (rbt->rbt_delete != NULL) {
                (*rbt->rbt_delete)(x->rn_key);
            }
            x = rb_delete(rbt, x);
            su_rbt_node_free(x);
            x = tree_minimum(rbt, rbt->rbt_root);
        }
        su_rbt_node_free(rbt->rbt_nil);

        SsMemFree(rbt);
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
bool su_rbt_insert(rbt, key)
	su_rbt_t* rbt;
	void* key;
{
        su_rbt_node_t* x;

        CHK_RBT(rbt);
        
        x = su_rbt_node_alloc();
        ss_debug(x->rn_id = rbt->rbt_id);
        x->rn_key = key;
        x->rn_left = rbt->rbt_nil;
        x->rn_right = rbt->rbt_nil;

        if (rb_insert(rbt, x)) {
            CHK_RBTNODE(x);
            rbt->rbt_nelems++;
            return(TRUE);
        } else {
            if (rbt->rbt_delete != NULL) {
                (*rbt->rbt_delete)(key);
            }
            su_rbt_node_free(x);
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
su_rbt_node_t* su_rbt_insert2(rbt, key)
	su_rbt_t* rbt;
	void* key;
{
        su_rbt_node_t* x;

        CHK_RBT(rbt);
        
        x = su_rbt_node_alloc();
        ss_debug(x->rn_id = rbt->rbt_id);
        x->rn_key = key;
        x->rn_left = rbt->rbt_nil;
        x->rn_right = rbt->rbt_nil;

        if (rb_insert(rbt, x)) {
            CHK_RBTNODE(x);
            rbt->rbt_nelems++;
            return(x);
        } else {
            if (rbt->rbt_delete != NULL) {
                (*rbt->rbt_delete)(key);
            }
            su_rbt_node_free(x);
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
void* su_rbt_delete(rbt, z)
	su_rbt_t* rbt;
	su_rbt_node_t* z;
{
        void* key;

        CHK_RBT(rbt);
        CHK_RBTNODE(z);
        CHK_RBTNODEID(rbt, z);
        
        if (rbt->rbt_delete != NULL) {
            (*rbt->rbt_delete)(z->rn_key);
            key = NULL;
        } else {
            key = z->rn_key;
        }

        z = rb_delete(rbt, z);
        rbt->rbt_nelems--;
        su_rbt_node_free(z);

        return(key);
}

/*##**********************************************************************\
 * 
 *		su_rbt_delete_nodatadel
 * 
 * Deletes a tree node. Same as su_rbt_delete nut does not call data delete
 * function.
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
 *      key value of the deleted node
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void* su_rbt_delete_nodatadel(
        su_rbt_t* rbt,
        su_rbt_node_t* z)
{
        void* key;

        CHK_RBT(rbt);
        CHK_RBTNODE(z);
        CHK_RBTNODEID(rbt, z);
        
        key = z->rn_key;

        z = rb_delete(rbt, z);
        rbt->rbt_nelems--;
        su_rbt_node_free(z);

        return(key);
}

/*##**********************************************************************\
 * 
 *		su_rbt_deleteall
 * 
 * deletes all nodes in rb-tree
 * 
 * Parameters : 
 * 
 *      rbt - in, use
 *          pointer to the tree structure
 *
 * Return value : 
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_rbt_deleteall(
        su_rbt_t* rbt)
{
        su_rbt_node_t* n;
        
        CHK_RBT(rbt);
        while ((n = su_rbt_min(rbt, NULL)) != NULL) {
            su_rbt_delete(rbt, n);
        }
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
su_rbt_node_t* su_rbt_search(rbt, key)
	su_rbt_t* rbt;
	void* key;
{
        su_rbt_node_t* x;

        CHK_RBT(rbt);
        
        x = iterative_tree_search(rbt, rbt->rbt_root, key);
        CHK_RBTNODE(x);

        if (x == rbt->rbt_nil) {
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
su_rbt_node_t* su_rbt_search_atleast(rbt, key)
	su_rbt_t* rbt;
	void* key;
{
        su_rbt_node_t* x;

        CHK_RBT(rbt);
        
        x = iterative_tree_search_atleast(rbt, rbt->rbt_root, key);
        CHK_RBTNODE(x);

        if (x == rbt->rbt_nil) {
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
su_rbt_node_t* su_rbt_search_atmost(rbt, key)
	su_rbt_t* rbt;
	void* key;
{
        su_rbt_node_t* x;

        CHK_RBT(rbt);
        
        x = iterative_tree_search_atmost(rbt, rbt->rbt_root, key);
        CHK_RBTNODE(x);

        if (x == rbt->rbt_nil) {
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
su_rbt_node_t* su_rbt_min(rbt, x)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        CHK_RBT(rbt);
        
        if (x == NULL) {
            x = rbt->rbt_root;
        }
        CHK_RBTNODE(x);
        CHK_RBTNODEID(rbt, x);

        if (x == rbt->rbt_nil) {
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
su_rbt_node_t* su_rbt_max(rbt, x)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        CHK_RBT(rbt);
        
        if (x == NULL) {
            x = rbt->rbt_root;
        }
        CHK_RBTNODE(x);
        CHK_RBTNODEID(rbt, x);

        if (x == rbt->rbt_nil) {
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
su_rbt_node_t* su_rbt_succ(rbt, x)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        CHK_RBT(rbt);
        CHK_RBTNODE(x);
        CHK_RBTNODEID(rbt, x);
        
        x = tree_successor(rbt, x);
        CHK_RBTNODE(x);

        if (x == rbt->rbt_nil) {
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
su_rbt_node_t* su_rbt_pred(rbt, x)
	su_rbt_t* rbt;
	su_rbt_node_t* x;
{
        CHK_RBT(rbt);
        CHK_RBTNODE(x);
        CHK_RBTNODEID(rbt, x);
        
        x = tree_predecessor(rbt, x);
        CHK_RBTNODE(x);

        if (x == rbt->rbt_nil) {
            return(NULL);
        } else {
            return(x);
        }
}

#ifdef SS_DEBUG
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
ulong su_rbt_nelems(rbt)
	su_rbt_t* rbt;
{
        CHK_RBT(rbt);

        return(_SU_RBT_NELEMS_(rbt));
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		su_rbtnode_getkey
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
void *su_rbtnode_getkey(rbtn)
	su_rbt_node_t *rbtn;
{
        CHK_RBTNODE(rbtn);
        
        return(rbtn->rn_key);
}

int su_rbt_long_compare(long l1, long l2)
{
        if (l1 < l2) {
            return(-1);
        } else if (l1 > l2) {
            return(1);
        } else {
            return(0);
        }
}

int su_rbt_ptr_compare(void* p1, void* p2)
{
        if ((char*)p1 < (char*)p2) {
            return (-1);
        }
        if ((char*)p1 > (char*)p2) {
            return (1);
        }
        return (0);
}

#ifdef SS_DEBUG
bool su_rbt_node_chk(su_rbt_node_t *rbtn)
{
        CHK_RBTNODE(rbtn);

        return(TRUE);
}

void su_rbt_setdebugsem(su_rbt_t* rbt, SsSemT* sem)
{
        ss_dassert(rbt->rbt_sem == NULL || sem == NULL);

        rbt->rbt_sem = sem;

        CHK_RBT(rbt);
}

#endif /* SS_DEBUG */

#ifdef AUTOTEST_RUN
void su_rbt_maxnodes(su_rbt_t* rbt, long maxnodes)
{
        rbt->rbt_maxnodes = maxnodes;
}
#endif

