/*************************************************************************\
**  source       * su0list.h
**  directory    * su
**  description  * Simple doubly linked list.
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


#ifndef SU0LIST_H
#define SU0LIST_H

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include "su1check.h"

#ifdef AUTOTEST_RUN
#  define CHK_LIST(l)      ss_dassert(SS_CHKPTR(l) && (l)->list_chk == SUCHK_LIST); ss_info_assert((l)->list_length < (l)->list_maxlen, ("Maximum list length (%ld) exceeded", (l)->list_maxlen))
#else 
# define CHK_LIST(l)      ss_dassert(SS_CHKPTR(l) && (l)->list_chk == SUCHK_LIST)
#endif

#define CHK_LISTNODE(ln) ss_dassert(SS_CHKPTR(ln) && (ln)->ln_chk == SUCHK_LISTNODE)
#define CHK_LISTNODENULL(ln) ss_dassert((ln) == NULL || (ln)->ln_chk == SUCHK_LISTNODE)

typedef struct su_list_st      su_list_t;
typedef struct su_list_node_st su_list_node_t;

/***** INTERNAL ONLY BEGIN *****/

/* Structure for a list node. Only field ln_data should be used directly.
*/
struct su_list_node_st {
        void*           ln_data; /* user given data of the node */
        su_list_node_t* ln_next; /* used only internally by list functions */
        su_list_node_t* ln_prev; /* used only internally by list functions */
        ss_debug(int    ln_chk;) /* check field */
};

/* Structure for a list header.
*/
struct su_list_st {
        ss_debug(int    list_chk;)
        ss_autotest(long list_maxlen;)
        su_list_node_t* list_first;     /* begin pointer */
        su_list_node_t* list_last;      /* end pointer */
        uint            list_length;    /* list length */
        bool            list_recyclenodes;
        su_list_node_t* list_savednodes;        /* nil node, begin and end pointer */
        void          (*list_datadel)(void*);   /* delete function, or NULL */
};

/***** INTERNAL ONLY END *****/

su_list_t* su_list_init(
            void (*datadel)(void*));

SS_INLINE void su_list_initbuf(
            su_list_t* list,
            void (*datadel)(void*));

SS_INLINE void su_list_done(
        su_list_t* list);

SS_INLINE void su_list_done_nodebuf(
        su_list_t* list);

SS_INLINE void su_list_donebuf(
        su_list_t* list);

void su_list_startrecycle(
        su_list_t* list);

SS_INLINE su_list_node_t* su_list_insertfirst(
        su_list_t* list,
        void* data);

SS_INLINE su_list_node_t* su_list_insertfirst_nodebuf(
        su_list_t* list,
        su_list_node_t* node,
        void* data);

SS_INLINE su_list_node_t* su_list_insertlast(
        su_list_t* list,
        void* data);

SS_INLINE su_list_node_t* su_list_insertlast_nodebuf(
        su_list_t* list,
        su_list_node_t* node,
        void* data);

SS_INLINE su_list_node_t* su_list_insertafter(
        su_list_t* list,
        su_list_node_t* node,
        void* data);

SS_INLINE su_list_node_t* su_list_insertbefore(
        su_list_t* list,
        su_list_node_t* node,
        void* data);

SS_INLINE void su_list_clear(
        su_list_t* list);

SS_INLINE void* su_list_remove(
        su_list_t* list,
        su_list_node_t* node);

SS_INLINE void* su_list_remove_nodebuf(
        su_list_t* list,
        su_list_node_t* node);

void* su_list_remove_nodatadel(
        su_list_t* list,
        su_list_node_t* node);

SS_INLINE void* su_list_removefirst(
        su_list_t* list);

void* su_list_removelast(
        su_list_t* list);

su_list_node_t* su_list_first(
        su_list_t* list);

su_list_node_t* su_list_last(
        su_list_t* list);

su_list_node_t* su_list_next(
        su_list_t* list,
        su_list_node_t* node);

su_list_node_t* su_list_prev(
        su_list_t* list,
        su_list_node_t* node);

su_list_node_t* su_list_removeandnext(
        su_list_t* list,
        su_list_node_t* node);

SS_INLINE void* su_list_getfirst(
        su_list_t* list);

SS_INLINE void* su_list_getlast(
        su_list_t* list);

void* su_list_setdata(
        su_list_t* list,
        su_list_node_t* list_node,
        void* data);

uint su_list_length(
        su_list_t* list);

void su_list_sort(su_list_t* list,
                  int (SS_CDECL *cmp_fp)(const void* a, const void* b));

/* internal */
void su_list_donebuf_ex(
        su_list_t* list,
        bool freenodes);

/* internal */
SS_INLINE su_list_node_t* su_list_insert(
        su_list_t* list,
        su_list_node_t* node,
        su_list_node_t* prev_node,
        void* data);
SS_INLINE void su_list_initnode(su_list_node_t* node, void* data);
SS_INLINE su_list_node_t* su_list_allocnode(su_list_t* list, void* data);

#ifdef AUTOTEST_RUN
void su_list_setmaxlen(su_list_t* list, long maxlen);
#endif

/* INTERNAL ONLY */
#define _SU_LIST_FIRST_(list)       ((list)->list_first)
#define _SU_LIST_LAST_(list)        ((list)->list_last)
#define _SU_LIST_NEXT_(node)        ((node)->ln_next)
#define _SU_LIST_PREV_(node)        ((node)->ln_prev)
#define _SU_LIST_LENGTH_(list)      ((list)->list_length)

#ifndef SS_DEBUG

#define su_list_first(list)      _SU_LIST_FIRST_(list)     
#define su_list_last(list)       _SU_LIST_LAST_(list)      
#define su_list_next(list, node) _SU_LIST_NEXT_(node)
#define su_list_prev(list, node) _SU_LIST_PREV_(node)
#define su_list_length(list)     _SU_LIST_LENGTH_(list)

#endif /* !SS_DEBUG */

/*##**********************************************************************\
 * 
 *		su_listnode_getdata
 * 
 * This macro returns the data element in the list node.
 * 
 * Parameters : 
 * 
 *	ln - in
 *		Pointer to a list node.
 * 
 * Return value - ref : 
 * 
 *      Data element in list node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define su_listnode_getdata(ln) ((ln)->ln_data)

/*##**********************************************************************\
 * 
 *		su_listnode_setdata
 * 
 * This macro sets the data element in the list node.
 * 
 * Parameters : 
 * 
 *	ln - in
 *		Pointer to a list node.
 * 
 *      data - in, hold
 *          New data value of list node ln
 * 
 * Return value - ref : 
 * 
 *      Data element in list node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define su_listnode_setdata(ln, data) ((ln)->ln_data = (data))

/*##**********************************************************************\
 * 
 *		su_list_do
 * 
 * This macro can be used to loop over elements in the list.
 * 
 * Example:
 *
 *      su_list_t*      list;
 *      su_list_node_t* node;
 *
 *      list = su_list_init(NULL);
 *      su_list_do(list, node) {
 *          ... process element at node ...
 *      }
 *      su_list_done(list);
 * 
 * Parameters : 
 * 
 *	list - in, use
 *		List pointer.
 *
 *      node - in out
 *          Node variable. For each loop iteration this variable
 *          contains the list node. The list node can be used to get
 *          the data pointer at the node.
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define su_list_do(list, node) \
            for ((node) = su_list_first(list); \
                 (node) != NULL; \
                 (node) = su_list_next(list, node))

/*##**********************************************************************\
 * 
 *		su_list_do_get
 * 
 * This macro can be used to loop over elements in the list.
 * The data element for each loop iteration is stored into parameter
 * data.
 * 
 * Example:
 *
 *      su_list_t*      list;
 *      su_list_node_t* node;
 *      void*           data;
 *
 *      list = su_list_init(NULL);
 *      su_list_do(list, node, data) {
 *          ... process data ...
 *      }
 *      su_list_done(list);
 * 
 * Parameters : 
 * 
 *	list - in, use
 *		List pointer.
 *
 *      node - in out
 *          Node variable. For each loop iteration this variable
 *          contains the list node. The list node can be used to get
 *          the data pointer at the node.
 *
 *      data - in out
 *          Data variable. In each loop iteration the data element is
 *          stored here.
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define su_list_do_get(list, node, data) \
            for ((node) = su_list_first(list); \
                 (node) != NULL && ((data) = su_listnode_getdata(node)) != NULL; \
                 (node) = su_list_next(list, node))

#define su_list_beginloop(list, datatype, data) \
            { \
                su_list_node_t* _loop_n; \
                datatype data; \
                for ((_loop_n) = su_list_first(list); \
                    (_loop_n) != NULL; \
                    (_loop_n) = su_list_next(list, _loop_n)) {\
                    (data) = (datatype)su_listnode_getdata(_loop_n);\
                {

#define su_list_endloop }}}

/* FUNCTIONS BELOW THIS ARE NOT YET TESTED IN TLIST. */

SS_INLINE void su_list_append(
        su_list_t* list1,
        su_list_t* list2);

SS_INLINE void su_list_unlink(
        su_list_t*          list,
        su_list_node_t*     node);

void su_list_link(
        su_list_t*          list,
        su_list_node_t*     prev_node,
        su_list_node_t*     node);

void su_list_linkfirst(
        su_list_t*          list,
        su_list_node_t*     node);

void*  su_list_datadel(su_list_t* list, su_list_node_t* node, bool datadel);
void su_list_freenode(su_list_t* list, su_list_node_t* node);

#if defined(SU0LIST_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		su_list_initbuf
 * 
 * Creates a new empty list into user given buffer.
 * 
 * Parameters : 
 * 
 *      list - in out, use
 * 
 *      datadel - in, use
 *		Pointer to a function that is used to delete list
 *          data elements. Can be also NULL.
 * 
 * Return value - give : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void su_list_initbuf(su_list_t* list, void (*datadel)(void*))
{
        list->list_datadel = datadel;
        list->list_length = 0;
        list->list_recyclenodes = FALSE;
        list->list_savednodes = NULL;
        list->list_first = NULL;
        list->list_last = NULL;
        ss_debug(list->list_chk = SUCHK_LIST);
        ss_autotest(list->list_maxlen = 15000);

        CHK_LIST(list);
}

/*##**********************************************************************\
 * 
 *		su_list_donebuf
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
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void su_list_donebuf(
        su_list_t* list)
{
        if (list->list_first != NULL || list->list_savednodes != NULL) {
            su_list_donebuf_ex(list, TRUE);
        }
}

/*##**********************************************************************\
 * 
 *		su_list_done
 * 
 * Removes a list. Removes also all elements from the list.
 * If data delete function is given in the init routine,
 * removes also user given data elements.
 * 
 * Parameters : 
 * 
 *	list - in, take
 *		Pointer to a list.
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void su_list_done(
        su_list_t* list)
{
        CHK_LIST(list);
        SS_MEMOBJ_DEC(SS_MEMOBJ_LIST);

        if (list->list_first != NULL || list->list_savednodes != NULL) {
            su_list_donebuf_ex(list, TRUE);
        }
        
        SsMemFree(list);
}

/*##**********************************************************************\
 * 
 *		su_list_done_nodebuf
 * 
 * Removes a list. Removes also all elements from the list but does 
 * not free them.
 * If data delete function is given in the init routine,
 * removes also user given data elements.
 * 
 * Parameters : 
 * 
 *	list - in, take
 *		Pointer to a list.
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void su_list_done_nodebuf(
        su_list_t* list)
{
        CHK_LIST(list);
        SS_MEMOBJ_DEC(SS_MEMOBJ_LIST);

        if (list->list_first != NULL || list->list_savednodes != NULL) {
            su_list_donebuf_ex(list, FALSE);
        }
        
        SsMemFree(list);
}

/*##**********************************************************************\
 * 
 *		su_list_insertfirst
 * 
 * Adds item at the beginning of the list.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	data - in, hold/take
 *		Data of the new node.
 *
 * Return value - ref : 
 * 
 *      Pointer to a new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_list_node_t* su_list_insertfirst(
        su_list_t* list,
        void* data)
{
        CHK_LIST(list);
 
        return(su_list_insert(list, NULL, NULL, data));
}

/*##**********************************************************************\
 * 
 *		su_list_insertfirst_nodebuf
 * 
 * Adds item at the beginning of the list.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	node - in, ref
 *		Pointer to a node buffer.
 *
 *	data - in, hold/take
 *		Data of the new node.
 *
 * Return value - ref : 
 * 
 *      Pointer to a new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_list_node_t* su_list_insertfirst_nodebuf(
        su_list_t* list,
        su_list_node_t* node,
        void* data)
{
        CHK_LIST(list);
 
        return(su_list_insert(list, node, NULL, data));
}

/*##**********************************************************************\
 * 
 *		su_list_insertlast
 * 
 * Adds item at the end of the list.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	data - in, hold/take
 *		Data of the new node.
 * 
 * Return value - ref : 
 * 
 *      Pointer to a new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_list_node_t* su_list_insertlast(
        su_list_t* list,
        void* data)
{
        CHK_LIST(list);
 
        return(su_list_insert(list, NULL, list->list_last, data));
}

/*##**********************************************************************\
 * 
 *		su_list_insertlast_nodebuf
 * 
 * Adds item at the end of the list.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	node - in out, ref
 *		Pointer to a node buffer.
 *
 *	data - in, hold/take
 *		Data of the new node.
 * 
 * Return value - ref : 
 * 
 *      Pointer to a new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_list_node_t* su_list_insertlast_nodebuf(
        su_list_t* list,
        su_list_node_t* node,
        void* data)
{
        CHK_LIST(list);
 
        return(su_list_insert(list, node, list->list_last, data));
}

/*##**********************************************************************\
 * 
 *		su_list_insertafter
 * 
 * Adds new item after the current item in the list.
 * 
 * Parameters : 
 * 
 *      list - in out, use
 *		Pointer to a list.
 *
 *	node - in out, use
 *		Pointer to a node after which the data is inserted.
 *
 *	data - in, hold/take
 *		Data of the new node.
 *
 * Return value - ref : 
 *
 *      Pointer to a new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_list_node_t* su_list_insertafter(
        su_list_t* list,
        su_list_node_t* node,
        void* data)
{
        CHK_LIST(list);
        CHK_LISTNODE(node);

        return(su_list_insert(list, NULL, node, data));
}

/*##**********************************************************************\
 * 
 *		su_list_insertbefore
 * 
 * Adds new item before the current item in the list.
 * 
 * Parameters : 
 * 
 *      list - in out, use
 *		Pointer to a list.
 *
 *	node - in out, use
 *		Pointer to a node before which the data is inserted.
 *
 *	data - in, hold/take
 *		Data of the new node.
 * 
 * Return value - ref : 
 *
 *      Pointer to a new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_list_node_t* su_list_insertbefore(
        su_list_t* list,
        su_list_node_t* node,
        void* data)
{
        CHK_LIST(list);
        CHK_LISTNODE(node);
        
        return(su_list_insert(list, NULL, node->ln_prev, data));
}

/*##*********************************************************************\
 * 
 *		su_list_insert
 * 
 * Adds item at the list after prev_node.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	node - in, take
 *		Buffer for list node or NULL.
 *
 *	prev_node - in out, use
 *		Previous list node.
 *
 *	data - in, hold/take
 *		data of the new node
 * 
 * Return value - give : 
 * 
 *      Pointer to a new list node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_list_node_t* su_list_insert(
        su_list_t* list,
        su_list_node_t* node,
        su_list_node_t* prev_node,
        void* data)
{
        if (node == NULL) {
            node = su_list_allocnode(list, data);
        } else {
            su_list_initnode(node, data);
        }

        if (prev_node == NULL) {
            if (list->list_first == NULL) {
                /* empty list */
                ss_dassert(prev_node == NULL);
                list->list_first = node;
                list->list_last = node;
                node->ln_next = NULL;
                node->ln_prev = NULL;
            } else {
                /* insert as the first item in the list */
                node->ln_next = list->list_first;
                list->list_first->ln_prev = node;
                node->ln_prev = NULL;
                list->list_first = node;
                ss_dassert(list->list_first != list->list_last);
                ss_dassert(list->list_first != NULL);
                ss_dassert(list->list_last != NULL);
            }
        } else {
            /* insert middle or end */
            CHK_LISTNODE(prev_node);
            node->ln_next = prev_node->ln_next;
            if (prev_node->ln_next != NULL) {
                prev_node->ln_next->ln_prev = node;
            } else {
                ss_dassert(list->list_last == prev_node);
                list->list_last = node;
            }
            prev_node->ln_next = node;
            node->ln_prev = prev_node;
            ss_dassert(list->list_first != list->list_last);
            ss_dassert(list->list_first != NULL);
            ss_dassert(list->list_last != NULL);
        }

        list->list_length++;

        return(node);
}

/*#***********************************************************************\
 * 
 *		su_list_initnode
 * 
 * Inits previously allocated node.
 * 
 * Parameters : 
 * 
 *	node - 
 *		
 *		
 *	data - 
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
SS_INLINE void su_list_initnode(su_list_node_t* node, void* data)
{
        ss_debug(node->ln_chk = SUCHK_LISTNODE);
        node->ln_data = data;

        CHK_LISTNODE(node);
}

/*#**********************************************************************\
 * 
 *		su_list_allocnode
 * 
 * Allocates a new list node.
 * 
 * Parameters : 
 *
 *	list - in out, use
 * 
 *	data - in, hold/take
 *		Data of the new node.
 * 
 * Return value - give : 
 * 
 *      Pointer to the new node.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_list_node_t* su_list_allocnode(su_list_t* list, void* data)
{
        su_list_node_t* node;

        if (list->list_savednodes != NULL) {
            node = list->list_savednodes;
            list->list_savednodes = node->ln_next;
        } else {
            SS_MEMOBJ_INC(SS_MEMOBJ_LISTNODE, su_list_node_t);
            node = (su_list_node_t*)SsMemAlloc(sizeof(su_list_node_t));
            ss_debug(node->ln_chk = SUCHK_LISTNODE);
        }

        node->ln_data = data;

        CHK_LISTNODE(node);
        
        return(node);
}

/*##**********************************************************************\
 * 
 *		su_list_clear
 * 
 * Removes all nodes from list
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 * Return value : 
 * 
 * Limitations  :
 *
 *      if destructor was not defined user must
 *      take care of deleting objects
 * 
 * Globals used : 
 */
SS_INLINE void su_list_clear(su_list_t* list)
{
        uint count;
        
        CHK_LIST(list);
        
        count = su_list_length(list);

        while (count--) {
            su_list_removefirst(list);
        }
}

/*##**********************************************************************\
 * 
 *		su_list_remove
 * 
 * Removes a node from the list.
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
SS_INLINE void* su_list_remove(
        su_list_t*      list,
        su_list_node_t* node)
{
        void* data;

        CHK_LIST(list);
        CHK_LISTNODE(node);
        ss_dassert(list->list_length > 0);

        data = su_list_remove_nodebuf(list, node);

        su_list_freenode(list, node);

        return(data);
}

/*##**********************************************************************\
 * 
 *		su_list_remove_nodebuf
 * 
 * Removes a node from the list.
 * 
 * Parameters : 
 * 
 *	list - in out, use
 *		Pointer to a list.
 *
 *	node - in
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
SS_INLINE void* su_list_remove_nodebuf(
        su_list_t*      list,
        su_list_node_t* node)
{
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

        return(su_list_datadel(list, node, TRUE));
}

/*##**********************************************************************\
 * 
 *		su_list_removefirst
 * 
 * Removes the first node from the list.
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
SS_INLINE void* su_list_removefirst(
        su_list_t* list)
{
        su_list_node_t* node;

        CHK_LIST(list);
 
        node = su_list_first(list);
        if (node != NULL) {
            return(su_list_remove(list, node));
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 * 
 *		su_list_getfirst
 * 
 * Returns the data of first node of the list.
 * 
 * Parameters : 
 * 
 *	list - in, use
 *		Pointer to a list.
 * 
 * Return value - ref : 
 * 
 *      Pointer to the data of first node of the list, or
 *      NULL if empty list
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void* su_list_getfirst(
        su_list_t* list)
{
        CHK_LIST(list);
        CHK_LISTNODENULL(list->list_first);

        return(list->list_first != NULL ? list->list_first->ln_data : NULL);
}

SS_INLINE void* su_list_getlast(
        su_list_t* list)
{
        CHK_LIST(list);
        CHK_LISTNODENULL(list->list_last);

        return(list->list_last != NULL ? list->list_last->ln_data : NULL);
}

/*##**********************************************************************\
 * 
 *		su_list_append
 * 
 * Appends the nodes of list2 to the end of list1.  List2 becomes empty,
 * but is not freed.  Be careful with the data destructor functions!
 * If list2 is empty, does nothing.
 * 
 * Parameters : 
 * 
 *	list1 -  in out, use
 *		pointer to the list to append to
 *		
 *	list2 - in out, use
 *		pointer to the list to append from
 *		
 * Return value:
 * 
 * Limitations: 
 * 
 * Globals used: 
 */
SS_INLINE void su_list_append(
        su_list_t*  list1,
        su_list_t*  list2)
{
        su_list_node_t *last1, *first2, *last2;

        CHK_LIST(list1);
        CHK_LIST(list2);

        if (list2->list_length > 0) {
            if (list1->list_length == 0) {
                list1->list_first = list2->list_first;
                list1->list_last = list2->list_last;
                list1->list_length = list2->list_length;
                list2->list_first = NULL;
                list2->list_last = NULL;
                list2->list_length = 0;
            } else {
                last1 = list1->list_last;
                last2 = list2->list_last;
                first2 = list2->list_first;
            
                last1->ln_next = first2;
                first2->ln_prev = last1;
                list1->list_last = last2;

                list2->list_first = NULL;
                list2->list_last = NULL;

                list1->list_length += list2->list_length;
                list2->list_length = 0;
            }
        }
}

SS_INLINE void su_list_unlink(
        su_list_t*          list,
        su_list_node_t*     node)
{
        CHK_LIST(list);
        CHK_LISTNODE(node);
        ss_dassert(list->list_length > 0);

        if (node == list->list_first) {
            if (node->ln_next != NULL) {
                ss_dassert(list->list_length > 1);
                node->ln_next->ln_prev = NULL;
            } else {
                ss_dassert(list->list_length == 1);
                list->list_last = NULL;
            }
            list->list_first = node->ln_next;
        } else if (node == list->list_last) {
            ss_dassert(list->list_length > 1);
            node->ln_prev->ln_next = NULL;
            list->list_last = node->ln_prev;
        } else {
            /*su_list_node_t* tmp;*/ /* removed 20031204 mr: unused */
            ss_dassert(list->list_length > 1);
            node->ln_prev->ln_next = node->ln_next;
            node->ln_next->ln_prev = node->ln_prev;
        }

        list->list_length--;
}


#endif /* defined(SU0LIST_C) || defined(SS_USE_INLINE) */

#endif /* SU0LIST_H */
