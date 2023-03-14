/*************************************************************************\
**  source       * su0list.c
**  directory    * su
**  description  * Simple doubly linked list.
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

Each list has a header part from which the actual list of nodes starts.
Also each list have an own nil node, which marks the begin and end of
the list. The previous pointer of the first list node points to the nil
node as well as the next pointer of the last node. The usage of nil node
makes insert and delete operations simpler, no if`s are needed.

Limitations:
-----------

None.

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

The list routines are not reentrant. The caller is responsible for
coordinating the concurrent access to the list.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define SU0LIST_C

#include <stdio.h>
#include <stdlib.h> /* qsort() */
    
#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include "su0list.h"

/*##*********************************************************************\
 * 
 *		su_list_datadel
 * 
 * Deallocates a list node data.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	node - in
 *		Pointer to a node that is deallocated.
 *
 *      datadel - in
 *          If FALSE, data is not deleted even if delete function
 *          is specified.
 *
 * Return value - give : 
 * 
 *      Pointer to the data of the node, if delete function is
 *      not defined for the list. NULL pointer if delete function
 *      is defined for the list.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void* su_list_datadel(list, node, datadel)
        su_list_t* list;
        su_list_node_t* node;
        bool datadel;
{
        void* data = NULL;

        CHK_LISTNODE(node);

        if (datadel && list->list_datadel != NULL) {
            (*list->list_datadel)(node->ln_data);
        } else {
            data = node->ln_data;
        }

        return(data);
}

/*##*********************************************************************\
 * 
 *		su_list_freenode
 * 
 * Deallocates a list node.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	node - in, take
 *		Pointer to a node that is deallocated.
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_list_freenode(list, node)
        su_list_t* list;
        su_list_node_t* node;
{
        CHK_LISTNODE(node);

        if (list->list_recyclenodes) {
            node->ln_next = list->list_savednodes;
            list->list_savednodes = node;
        } else {
            SS_MEMOBJ_DEC(SS_MEMOBJ_LISTNODE);
            SsMemFree(node);
        }
}

/*##**********************************************************************\
 * 
 *		su_list_init
 * 
 * Creates a new empty list.
 * 
 * Parameters : 
 * 
 *      datadel - in, use
 *		Pointer to a function that is used to delete list
 *          data elements. Can be also NULL.
 * 
 * Return value - give : 
 * 
 *      Pointer to a new list.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_t* su_list_init(void (*datadel)(void*))
{
        su_list_t*  list;

        SS_MEMOBJ_INC(SS_MEMOBJ_LIST, su_list_t);
        
        list = SsMemAlloc(sizeof(su_list_t));

        su_list_initbuf(list, datadel);

        CHK_LIST(list);


        return(list);
}

/*##**********************************************************************\
 * 
 *		su_list_donebuf_ex
 * 
 * Removes a list. Removes also all elements from the list.
 * If data delete function is givcen in the init routine,
 * removes also user given data elements.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 * 
 *	freenodes - in, use
 *		If TRUE nodes are released.
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_list_donebuf_ex(
        su_list_t* list,
        bool freenodes)
{
        su_list_node_t* node;
        su_list_node_t* tmp;

        CHK_LIST(list);

        node = list->list_first;

        while (node != NULL) {
            tmp = node->ln_next;
            su_list_datadel(list, node, TRUE);
            if (freenodes) {
                su_list_freenode(list, node);
            }
            node = tmp;
        }
        while (list->list_savednodes != NULL) {
            SS_MEMOBJ_DEC(SS_MEMOBJ_LISTNODE);
            node = list->list_savednodes;
            CHK_LISTNODE(node);
            list->list_savednodes = node->ln_next;
            SsMemFree(node);
        }
}

/*##**********************************************************************\
 * 
 *		su_list_startrecycle
 * 
 * Starts recycling list nodes. Unused nodes are kept and not released
 * until list is removed.
 * 
 * Parameters : 
 * 
 *	list - 
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
void su_list_startrecycle(su_list_t* list)
{
        CHK_LIST(list);

        list->list_recyclenodes = TRUE;
}

/*##**********************************************************************\
 * 
 *		su_list_remove_nodatadel
 * 
 * Removes a node from the list. The delete function is not called
 * for the deleted element.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	node - in, take
 *		Pointer to a node which is removed. After this call
 *          the node pointer is invalid.
 *
 * 
 * Return value - give : 
 * 
 *      NULL, if delete function is specified in list_init, or
 *      data of the deleted node
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void* su_list_remove_nodatadel(list, node)
        su_list_t* list;
        su_list_node_t* node;
{
        void* data;

        CHK_LIST(list);
        CHK_LISTNODE(node);
        ss_dassert(list->list_length > 0);

        if (node == list->list_first) {
            if (node->ln_next != NULL) {
                ss_dassert(list->list_length > 1);
                ss_dassert(list->list_last != node);
                node->ln_next->ln_prev = NULL;
            } else {
                ss_dassert(list->list_length == 1);
                ss_dassert(list->list_last == node);
                list->list_last = NULL;
            }
            list->list_first = node->ln_next;
        } else if (node == list->list_last) {
            ss_dassert(list->list_length > 1);
            node->ln_prev->ln_next = NULL;
            list->list_last = node->ln_prev;
        } else {
            ss_dassert(list->list_length > 1);
            node->ln_prev->ln_next = node->ln_next;
            node->ln_next->ln_prev = node->ln_prev;
        }

        list->list_length--;

        data = su_list_datadel(list, node, FALSE);
        su_list_freenode(list, node);

        return(data);
}

/*##**********************************************************************\
 * 
 *		su_list_removelast
 * 
 * Removes the last node from the list.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 * 
 * Return value - give : 
 * 
 *      NULL, if delete function is specified in list_init or empty list, or
 *      data of the deleted node
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void* su_list_removelast(list)
        su_list_t* list;
{
        su_list_node_t* node;

        CHK_LIST(list);
 
        node = su_list_last(list);
        if (node != NULL) {
            return(su_list_remove(list, node));
        } else {
            return(NULL);
        }
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *		su_list_first
 * 
 * Returns the first node of the list.
 * 
 * Parameters : 
 * 
 *	list - in, use
 *		Pointer to a list.
 * 
 * Return value - ref : 
 * 
 *      Pointer to the first node of the list, or
 *      NULL if empty list
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_node_t* su_list_first(list)
        su_list_t* list;
{
        CHK_LIST(list);
        CHK_LISTNODENULL(list->list_first);

        return(_SU_LIST_FIRST_(list));
}

/*##**********************************************************************\
 * 
 *		su_list_last
 * 
 * Returns the last node of the list.
 * 
 * Parameters : 
 * 
 *	list - in, use
 *		Pointer to a list.
 *
 * Return value - ref : 
 * 
 *      Pointer to the last node of the list, or
 *      NULL if empty list
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_node_t* su_list_last(list)
        su_list_t* list;
{
        CHK_LIST(list);
        CHK_LISTNODENULL(list->list_last);
        
        return(_SU_LIST_LAST_(list));
}

/*##**********************************************************************\
 * 
 *		su_list_next
 * 
 * Returns the next node after 'node' in the list.
 * 
 * Parameters : 
 * 
 *	list - in, use
 *		Pointer to a list.
 *
 *	node - in, use
 *		Pointer to a node the successor of which is returned.
 *
 * Return value - ref : 
 * 
 *      Pointer to the next node of the list, or
 *      NULL if at the end of the list
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_node_t* su_list_next(list, node)
        su_list_t* list;
        su_list_node_t* node;
{
        CHK_LIST(list);
        CHK_LISTNODE(node);
        CHK_LISTNODENULL(node->ln_next);

        return(_SU_LIST_NEXT_(node));
}

/*##**********************************************************************\
 * 
 *		su_list_prev
 * 
 * Returns the previous node before 'node' in the list.
 * 
 * Parameters : 
 * 
 *	list - in, use
 *		Pointer to a list.
 *
 *	node - in, use
 *		Pointer to a node the predecessor of which is returned.
 *
 * Return value - ref : 
 * 
 *      Pointer to the previous node of the list, or
 *      NULL if at the end of the list
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_node_t* su_list_prev(list, node)
        su_list_t* list;
        su_list_node_t* node;
{
        CHK_LIST(list);
        CHK_LISTNODE(node);
        CHK_LISTNODENULL(node->ln_prev);

        return(_SU_LIST_PREV_(node));
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		su_list_removeandnext
 * 
 * Removes the current list node and returns the next node.
 * 
 * Parameters : 
 * 
 *	list - 
 *		
 *		
 *	node - 
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
su_list_node_t* su_list_removeandnext(
        su_list_t* list,
        su_list_node_t* node)
{
        su_list_node_t* nextnode;

        nextnode = su_list_next(list, node);
        su_list_remove(list, node);
        return(nextnode);
}

#ifdef SS_DEBUG
/*##**********************************************************************\
 * 
 *		su_list_length
 * 
 * Returns the length of the list.
 * 
 * Parameters : 
 * 
 *	list - in, use
 *		Pointer to a list.
 * 
 * Return value : 
 * 
 *      Length of the list.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint su_list_length(list)
        su_list_t* list;
{
        CHK_LIST(list);
 
        return(_SU_LIST_LENGTH_(list));
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		su_list_setdata
 * 
 * Sets data to an existing list node. If list_datadel is non-null
 * calls it for the old data value.
 * 
 * Parameters : 
 * 
 *	list -  in out, use
 *		pointer to list
 *		
 *	list_node - in out, use
 *		pointer to node of the list
 *		
 *	data - in, hold/take
 *		pointer to new data value to set
 *		
 * Return value - give :
 *      pointer to old data value, if list_datadel is NULL or
 *      NULL otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void* su_list_setdata(list, list_node, data)
        su_list_t* list;
        su_list_node_t* list_node;
        void* data;
{
        void *old_data;

        CHK_LIST(list);
        CHK_LISTNODE(list_node);

        old_data = list_node->ln_data;
        if (list->list_datadel != NULL
        &&  data != old_data
        &&  old_data != NULL)
        {
            (*list->list_datadel)(old_data);
            old_data = NULL;
        }
        list_node->ln_data = data;
        return (old_data);
}

/* FUNCTIONS BELOW ARE NOT TESTED IN TLIST! */

void su_list_link(
        su_list_t*          list,
        su_list_node_t*     prev_node,
        su_list_node_t*     node)
{
        CHK_LIST(list);
        CHK_LISTNODE(node);

        if (prev_node == NULL) {
            /* empty list */
            ss_dassert(list->list_first == NULL);
            ss_dassert(list->list_last == NULL);
            node->ln_next = NULL;
            node->ln_prev = NULL;
            list->list_first = node;
            list->list_last = node;
        } else if (prev_node->ln_next == NULL) {
            /* link as last item */
            CHK_LISTNODE(prev_node);
            ss_dassert(list->list_last == prev_node);
            node->ln_next = NULL;
            prev_node->ln_next = node;
            node->ln_prev = prev_node;
            list->list_last = node;
        } else {
            CHK_LISTNODE(prev_node);
            node->ln_next = prev_node->ln_next;
            prev_node->ln_next->ln_prev = node;
            prev_node->ln_next = node;
            node->ln_prev = prev_node;
        }

        list->list_length++;
}

void su_list_linkfirst(
        su_list_t*          list,
        su_list_node_t*     node)
{
        CHK_LIST(list);
        CHK_LISTNODE(node);

        su_list_link(list, list->list_first, node);
}

/*##**********************************************************************\
 *
 *      su_list_sort
 *
 * Sorts list's data in place.  After you do this you should not rely on a
 * node pointer to point to the same data anymore.
 *
 * Parameters:
 *      list - in, use
 *          list to sort
 *
 *      cmp_fp - in
 *          comparison function pointer to compare the data.
 *          return -1 if a < b, 0 if a == b, 1 if a > b.
 *
 *          The comparison function is passed POINTERS to POINTERS to data
 *          so you should have a function something like this (the data is an
 *          integer here):
 *
 *          int SS_CDECL cmp_f(const void* a, const void* b)
 *          {
 *                  int** ia, **ib;
 *
 *                  ia = (int**) a;
 *                  ib = (int**) b;
 *
 *                  if (**ia < **ib) {
 *                      return -1;
 *                  } else if (**ia == **ib) {
 *                      return 0;
 *                  }
 *                  return 1;
 *          }
 *
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void su_list_sort(su_list_t* list, int (SS_CDECL *cmp_fp)(const void* a, const void* b))
{
        unsigned int length;
        void** d_arr;
        su_list_node_t* np;
        unsigned int i;
        
        if (list->list_length < 2) {
            return;
        }

        length = list->list_length;
        
        /* alloc array for list data pointers */
        d_arr = SsMemAlloc(sizeof(void*) * length);

        ss_dassert(d_arr != NULL);
        
        /* project list data pointers into the array */
        for (np = list->list_first, i = 0;
             i < list->list_length;
             i++, np = np->ln_next) {
            
            d_arr[i] = np->ln_data;
        }

        /* qsort on array */
        qsort(d_arr, length, sizeof(void*), cmp_fp);
        
        /* put sorted data into the list */
        for (np = list->list_first, i = 0;
             i < length;
             i++, np = np->ln_next) {

            ss_dassert(np != NULL);
            np->ln_data = d_arr[i];
        }

        ss_dassert(length == list->list_length); /* if this ever
                                                  * breaks we must
                                                  * start using
                                                  * mutexes */
        
        /* this actually changes the order of data in the list, not
         * the order of the nodes in the list so probably every node's
         * data pointer will change value. */
        
        SsMemFree(d_arr);        
}

#ifdef AUTOTEST_RUN
void su_list_setmaxlen(su_list_t* list, long maxlen)
{
        list->list_maxlen = maxlen;
}
#endif


/* EOF */
