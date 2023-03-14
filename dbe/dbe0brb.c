/*************************************************************************\
**  source       * dbe0brb.c
**  directory    * dbe
**  description  * Blob Reference Buffer for holding blob references
**               * that have not been saved to database rows yet.
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
#include <ssmem.h>
#include <ssdebug.h>
#include "dbe0type.h"
#include "dbe6log.h"
#include "dbe6bmgr.h"
#include "dbe0brb.h"

#ifndef SS_NOBLOB

typedef enum {
    BLOBREF_NOTSAVED,
    BLOBBREF_NOTSAVED_REMOVEONSAVE,
    BLOBREF_SAVED,
    BLOBREF_DELETED
} blobref_status_t;

typedef struct {
        int br_check;
        blobref_status_t br_status;
        dbe_vablobref_t br_buf;
} blobref_t;

struct dbe_brb_st {
        int brb_check;
        SsSemT* brb_mutex;
        su_rbt_t* brb_pool;
};

#define CHK_BR(br)   ss_assert(SS_CHKPTR(br) && (br)->br_check == DBE_CHK_BLOBREF)
#define CHK_BRB(brb) ss_assert(SS_CHKPTR(brb) && (brb)->brb_check == DBE_CHK_BLOBREFBUF)

/*#***********************************************************************\
 * 
 *		br_cmp
 * 
 * Compares two blob references based on blob id
 * 
 * Parameters : 
 * 
 *	br1 - in, use
 *		blob ref 1
 *		
 *	br2 - in, use
 *		blob ref 2
 *		
 * Return value : 
 *      logical subtraction result of the blob id's
 *      
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static int br_cmp(blobref_t* br1, blobref_t* br2)
{
        dbe_blobid_t bi1;
        dbe_blobid_t bi2;
        int cmp;
        
        CHK_BR(br1);
        CHK_BR(br2);
        bi1 = dbe_bref_getblobid(&(br1->br_buf));
        bi2 = dbe_bref_getblobid(&(br2->br_buf));
        cmp = DBE_BLOBID_CMP(bi1, bi2);
        return (cmp);
}

/*#***********************************************************************\
 * 
 *		br_delete
 * 
 * Deletes one blob reference
 * 
 * Parameters : 
 * 
 *	br - in, take
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
static void br_delete(void* br)
{
        CHK_BR((blobref_t*)br);
        ss_dprintf_1(("br_delete: removing blob id: %lu\n",
                      dbe_bref_getblobid(&(((blobref_t*)br)->br_buf))));
        SsMemFree(br);
}

static void br_initbuf_va(
        blobref_t* br,
        va_t* va,
        blobref_status_t initial_status)
{
        br->br_check = DBE_CHK_BLOBREF;
        br->br_status = initial_status;
        dbe_bref_loadfromva(&(br->br_buf), va);
}

static void br_initbuf(
        blobref_t* br,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        blobref_status_t initial_status)
{
        va_t* va;
        
        ss_dassert(rs_aval_isblob(cd, atype, aval));
        va = rs_aval_va(cd, atype, aval);
        br_initbuf_va(br, va, initial_status);
}

static blobref_t* br_init(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        blobref_status_t initial_status)
{
        blobref_t* br = SSMEM_NEW(blobref_t);
        br_initbuf(br, cd, atype, aval, initial_status);
        ss_dprintf_1(("br_init: adding blob id: %lu\n",
                      dbe_bref_getblobid(&(br->br_buf))));
        return (br);
}
        

/*##**********************************************************************\
 * 
 *		dbe_brb_init
 * 
 * Creates a blob ref. buffer
 * 
 * Parameters : 	 - none
 * 
 * Return value - give :
 *      pointer to created blob ref. buffer
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_brb_t* dbe_brb_init(void)
{
        dbe_brb_t* brb = SSMEM_NEW(dbe_brb_t);

        brb->brb_check = DBE_CHK_BLOBREFBUF;
        brb->brb_mutex = SsSemCreateLocal(SS_SEMNUM_DBE_BLOBREFBUF);
        brb->brb_pool = su_rbt_init((int(*)(void*,void*))br_cmp, br_delete);
        return (brb);
}

/*##**********************************************************************\
 * 
 *		dbe_brb_done
 * 
 * Deletes a blob ref. buffer
 * 
 * Parameters : 
 * 
 *	brb - in, take
 *		blob ref. buffer
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_brb_done(dbe_brb_t* brb)
{
        CHK_BRB(brb);
        brb->brb_check = DBE_CHK_FREEDBLOBREFBUF;
        SsSemFree(brb->brb_mutex);
        su_rbt_done(brb->brb_pool);
        SsMemFree(brb);
}


static su_rbt_node_t* brb_find_node_va(
        dbe_brb_t* brb,
        va_t* va)
{
        blobref_t blobref;
        su_rbt_node_t* rbtnode;

        br_initbuf_va(&blobref, va, BLOBREF_NOTSAVED);
        rbtnode = su_rbt_search(brb->brb_pool, &blobref);
        ss_dprintf_1(("brb_find_node_va searching blob id: %lu found=0x%08lX\n",
                      dbe_bref_getblobid(&(blobref.br_buf)), (ulong)rbtnode));
        return (rbtnode);
}

static blobref_t* brb_find_br_va(
        dbe_brb_t* brb,
        va_t* va)
{
        su_rbt_node_t* rbtnode;
        rbtnode = brb_find_node_va(brb, va);
        if (rbtnode != NULL) {
            blobref_t* br = su_rbtnode_getkey(rbtnode);
            CHK_BR(br);
            return (br);
        }
        return (NULL);
}

static su_rbt_node_t* brb_find_node(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        blobref_t blobref;
        su_rbt_node_t* rbtnode;

        br_initbuf(&blobref, cd, atype, aval, BLOBREF_NOTSAVED);
        rbtnode = su_rbt_search(brb->brb_pool, &blobref);
        ss_dprintf_1(("brb_find_node searching blob id: %lu found=0x%08lX\n",
                      dbe_bref_getblobid(&(blobref.br_buf)), (ulong)rbtnode));
        return (rbtnode);
}

#ifndef SS_MYSQL
static blobref_t* brb_find_br(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        su_rbt_node_t* rbtnode;
        rbtnode = brb_find_node(brb, cd, atype, aval);
        if (rbtnode != NULL) {
            blobref_t* br = su_rbtnode_getkey(rbtnode);
            CHK_BR(br);
            return (br);
        }
        return (NULL);
}
#endif /* !SS_MYSQL */

/*##**********************************************************************\
 * 
 *		dbe_brb_needtocopy_setsaved
 * 
 * Checks if a blob reference addressed by a blob ref aval is contained
 * in blob ref. buffer and status requires copying if saved as a column
 * of a table. When copying is NOT needed the status of the reference is also
 * automatically set to SAVED.
 * 
 * Parameters : 
 * 
 *	brb - in, use
 *		blob ref. buffer
 *		
 *	cd - in, use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 * Return value :
 *      TRUE - blob ref. found from buffer and status == SAVED or DELETED
 *      FALSE - not found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dbe_brb_needtocopy_setsaved(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool needtocopy = TRUE;
        blobref_t* br;
        su_rbt_node_t* rbtnode;

        CHK_BRB(brb);
        SsSemEnter(brb->brb_mutex);
        rbtnode = brb_find_node(brb, cd, atype, aval);
        if (rbtnode != NULL) {
            br = su_rbtnode_getkey(rbtnode);
            switch (br->br_status) {
                default:
                    ss_rc_derror(br->br_status);
                    /* FALLTHROUGH - not really */
                case BLOBREF_SAVED:
                case BLOBREF_DELETED:
                    break;
                case BLOBREF_NOTSAVED:
                    ss_dprintf_1(("dbe_brb_needtocopy_setsaved: setting status of blob id: %lu to BLOBREF_SAVED\n",
                                  dbe_bref_getblobid(&(br->br_buf))));
                    ss_dprintf_1(("dbe_brb_needtocopy_setsaved: nblobs = %lu blobid = %lu\n",
                                  (ulong)su_rbt_nelems(brb->brb_pool),
                                  (long)dbe_bref_getblobid(&(br->br_buf))));

                    br->br_status = BLOBREF_SAVED;
                    needtocopy = FALSE;
                    break;
                case BLOBBREF_NOTSAVED_REMOVEONSAVE:
                    needtocopy = FALSE;
                    su_rbt_delete(brb->brb_pool, rbtnode);
                    break;
            }
        }
        SsSemExit(brb->brb_mutex);
        ss_dprintf_1(("dbe_brb_needtocopy_setsaved needtocopy = %d\n",
                     needtocopy));
        return (needtocopy);
}

bool dbe_brb_defer_delete_va(
        dbe_brb_t* brb,
        va_t* va)
{
        bool defer_delete = FALSE;
        blobref_t* br;

        CHK_BRB(brb);
        SsSemEnter(brb->brb_mutex);
        br =  brb_find_br_va(brb, va);
        if (br != NULL) {
            switch (br->br_status) {
                case BLOBREF_NOTSAVED:
                case BLOBBREF_NOTSAVED_REMOVEONSAVE:
                case BLOBREF_DELETED:
                default:
                    ss_rc_derror(br->br_status);
                    /* FALLTHROUGH */ /* not really! */
                case BLOBREF_SAVED:
                    defer_delete = TRUE;
                    ss_dprintf_1(("dbe_brb_defer_delete_va: setting status of blobid = %lu to BLOBREF_DELETED nblobs = %lu\n",
                                  dbe_bref_getblobid(&(br->br_buf)),
                                  (ulong)su_rbt_nelems(brb->brb_pool)));
                    br->br_status = BLOBREF_DELETED;
                    break;
            }
        }
        SsSemExit(brb->brb_mutex);
        ss_dprintf_1(("dbe_brb_needtocopy_defer_delete_va defer_delete = %d\n",
                     defer_delete));
        return (defer_delete);
}

/*##**********************************************************************\
 * 
 *		dbe_brb_needtodelete_remove
 * 
 * Checks if a blob reference addressed by a blob ref aval is contained
 * in blob ref. buffer and its status is either BLOBREF_NOTSAVED
 * or BLOBREF_DELETED. It is removed
 * from blobrefbuf. This need to done this way due
 * to atomicity requirements to avoid conflicts with running merge process.
 *
 * 
 * Parameters : 
 * 
 *	brb - in, use
 *		blob ref. buffer
 *		
 *	cd - in, use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 * Return value :
 *      TRUE - blob ref. found from buffer and need to delete it
 *      FALSE - not found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dbe_brb_needtodelete_remove(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool needtodelete = FALSE;
        su_rbt_node_t* rbtnode;

        CHK_BRB(brb);
        SsSemEnter(brb->brb_mutex);
        rbtnode =  brb_find_node(brb, cd, atype, aval);
        if (rbtnode != NULL) {
            blobref_t* br = su_rbtnode_getkey(rbtnode);
            switch (br->br_status) {
                case BLOBBREF_NOTSAVED_REMOVEONSAVE:
                    ss_rc_error(br->br_status); /* should not happen! */
                case BLOBREF_DELETED:
                case BLOBREF_NOTSAVED:
                    needtodelete = TRUE;
                    /* FALLTHROUGH */
                case BLOBREF_SAVED:
                    ss_dprintf_1(("dbe_brb_needtodelete_remove: status = %d needtodelete = %d nblobs = %lu blobid = %lu\n",
                                  br->br_status,
                                  needtodelete,
                                  (ulong)su_rbt_nelems(brb->brb_pool),
                                  (long)dbe_bref_getblobid(&(br->br_buf))));
                    su_rbt_delete(brb->brb_pool, rbtnode);
                    break;
                default:
                    ss_rc_error(br->br_status);
            }
        }
        SsSemExit(brb->brb_mutex);
        ss_dprintf_1(("dbe_brb_needtodelete_remove: needtodelete = %d\n",
                     needtodelete));
        return (needtodelete);
}

/*##**********************************************************************\
 * 
 *		dbe_brb_removeif
 * 
 * Checks if a blob reference addressed by a blob ref aval is contained
 * in blob ref. buffer and if it is there, removes it unconditionally.
 * 
 * Parameters : 
 * 
 *	brb - in, use
 *		blob ref. buffer
 *		
 *	cd - in, use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 * Return value :
 *      TRUE - blob ref. found from buffer and removed
 *      FALSE - not found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dbe_brb_removeif(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool found = FALSE;
        su_rbt_node_t* rbtnode;

        CHK_BRB(brb);
        SsSemEnter(brb->brb_mutex);
        rbtnode =  brb_find_node(brb, cd, atype, aval);
        if (rbtnode != NULL) {
            found = TRUE;
            ss_debug(
            {
                blobref_t* br = su_rbtnode_getkey(rbtnode);
                ss_dprintf_1(("dbe_brb_removeif nblobs = %lu blobid = %lu\n",
                              (ulong)su_rbt_nelems(brb->brb_pool),
                              (long)dbe_bref_getblobid(&(br->br_buf))));
            });
            su_rbt_delete(brb->brb_pool, rbtnode);
        }
        SsSemExit(brb->brb_mutex);
        return (found);
}

/*##**********************************************************************\
 * 
 *		dbe_brb_insert
 * 
 * Inserts a new blob ref to buffer
 * 
 * Parameters : 
 * 
 *	brb - use
 *		blob ref. buffer
 *		
 *	cd - in, use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 * Return value :
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void brb_insert(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        blobref_status_t initial_status)
{
        va_t* va __attribute__ ((unused));
        blobref_t* br;

        CHK_BRB(brb);
        br = br_init(cd, atype, aval, initial_status);
        SsSemEnter(brb->brb_mutex);
        ss_dassert(brb_find_node(brb, cd, atype, aval) == NULL);
        ss_dprintf_1(("dbe_brb_insert nblobs = %lu blobid = %lu\n",
                      (ulong)su_rbt_nelems(brb->brb_pool),
                      (long)dbe_bref_getblobid(&(br->br_buf))));
        su_rbt_insert(brb->brb_pool, br);
        SsSemExit(brb->brb_mutex);
}

void dbe_brb_insert(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        brb_insert(brb, cd, atype, aval, BLOBREF_NOTSAVED);
}

void dbe_brb_insert_removeonsave(
        dbe_brb_t* brb,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        brb_insert(brb, cd, atype, aval, BLOBBREF_NOTSAVED_REMOVEONSAVE);
}

#endif /* SS_NOBLOB */





