/*************************************************************************\
**  source       * sssysres.c
**  directory    * ss
**  description  * System resource pool object. This object is used
**               * to save system resources that must be released
**               * before the executable exists.
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

#include "ssenv.h"

#include "ssstdio.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssmem.h"

#include "sssysres.h"

typedef struct list_node_st list_node_t;

struct list_node_st {
        list_node_t*    ln_next;
        list_node_t*    ln_prev;
        void*           ln_data;    /* system resource data */
        void            (*ln_freefun)(void* data);
};

static struct {
        list_node_t     list_nil;
} list;

static bool sysres_init = FALSE;

void SsSysResGlobalInit(void)
{
        list.list_nil.ln_next = &list.list_nil;
        list.list_nil.ln_prev = &list.list_nil;

        sysres_init = TRUE;
}

void SsSysResGlobalDone(void)
{
        list_node_t* node;
        list_node_t* next_node;

        if (sysres_init) {

            sysres_init = FALSE;

            node = list.list_nil.ln_next;

            while (node != &list.list_nil) {
                next_node = node->ln_next;
                (*node->ln_freefun)(node->ln_data);
                free(node);
                node = next_node;
            }
            list.list_nil.ln_next = &list.list_nil;
            list.list_nil.ln_prev = &list.list_nil;

        }
}


void* SsSysResAdd(
        void (*freefun)(void* data),
        void* data)
{
        list_node_t* node;
        list_node_t* prev_node = &list.list_nil;

        node = malloc(sizeof(list_node_t));
        ss_assert(node != NULL);

        node->ln_freefun = freefun;
        node->ln_data = data;

        node->ln_next = prev_node->ln_next;
        prev_node->ln_next->ln_prev = node;
        prev_node->ln_next = node;
        node->ln_prev = prev_node;

        ss_assert(node != NULL);
        ss_assert(node->ln_next != NULL);
        ss_assert(node->ln_prev != NULL);

        return(node);
}

void* SsSysResAddLast(
        void (*freefun)(void* data),
        void* data)
{
        list_node_t* node;
        list_node_t* prev_node = list.list_nil.ln_prev;

        node = malloc(sizeof(list_node_t));
        ss_assert(node != NULL);

        node->ln_freefun = freefun;
        node->ln_data = data;

        node->ln_next = prev_node->ln_next;
        prev_node->ln_next->ln_prev = node;
        prev_node->ln_next = node;
        node->ln_prev = prev_node;

        ss_assert(node != NULL);
        ss_assert(node->ln_next != NULL);
        ss_assert(node->ln_prev != NULL);

        return(node);
}

void SsSysResRemove(
        void* resid)
{
        list_node_t* node = resid;
        ss_assert(node != NULL);
        ss_assert(node->ln_next != NULL);
        ss_assert(node->ln_prev != NULL);

        node->ln_prev->ln_next = node->ln_next;
        node->ln_next->ln_prev = node->ln_prev;
	
        free(node);
}

void SsSysResRemoveByData(
        void* data)
{
        list_node_t* node;
        for (node = list.list_nil.ln_next; ;node = node->ln_next) {
            ss_dassert(node != &list.list_nil);
            if (node->ln_data == data) {
                node->ln_prev->ln_next = node->ln_next;
                node->ln_next->ln_prev = node->ln_prev;
                free(node);
                return;

            }
        }
        ss_error;
}

