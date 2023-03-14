/*************************************************************************\
**  source       * dbe8seql.c
**  directory    * dbe
**  description  * Sequence list object. Used to save sequence values
**               * to the database during checkpoint.
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

This file implements a list of sequence infos
that is stored to disk upon checkpoint creation. Each info record
consist of sequence id (4 bytes) and sequence values (8 bytes).

Limitations:
-----------

Only one thread is allowed to operate on one object of these classes
at a time (ie. no auto-mutexing).

Error handling:
--------------

Routines that call file operations that give status code of type su_ret_t
return the same code. Other failures are currently asserted.

Objects used:
------------

Disk cache "dbe8cach.h"
Free list "dbe8flst.h"

Preconditions:
-------------

Multithread considerations:
--------------------------

All concurrent access to an object of this class is forbidden.
An external semaphore must be used in multithreaded application.

Example:
-------

Maybe later.

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <rs0tnum.h>

#include "dbe9blst.h"
#include "dbe8seql.h"

#ifdef SS_DEBUG
static char seql_writectx[] = "Sequence list write";
static char seql_delctx[] = "Sequence list delete";
static char seql_readctx[] = "Sequence list read";
#else
#define seql_writectx NULL
#define seql_delctx NULL
#define seql_readctx  NULL
#endif

/* Structure to represent sequence list
 */
struct dbe_seqlist_st {
        dbe_cache_t*        seql_cache;
        dbe_freelist_t*     seql_freelist;  /* not used by iterator */
        size_t              seql_blocksize;
        su_daddr_t          seql_daddr;
        dbe_blheader_t      seql_header;
        dbe_cacheslot_t*    seql_cacheslot;
        void*               seql_diskbuf;
        uint                seql_pos;       /* only used in iterator */
};

/*#**********************************************************************\
 * 
 *		DBE_SEQLNODE_ITEMSIZE
 * 
 * Macro that calculates the saved item size in the block
 * 
 * Parameters : 
 * 
 * Return value :
 *      as mentioned above
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#define DBE_SEQLNODE_ITEMSIZE   (4+8)

/*#**********************************************************************\
 * 
 *		DBE_SEQLNODE_CAPACITY
 * 
 * Macro that calculates # of sequence info records that fit into
 * one block
 * 
 * Parameters : 
 * 
 *	blocksize - in
 *		size of database file block in bytes
 *
 * Return value : as mentioned above
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#define DBE_SEQLNODE_CAPACITY(blocksize) \
        (((blocksize)-DBE_BLIST_DATAOFFSET)/DBE_SEQLNODE_ITEMSIZE)

/*##**********************************************************************\
 * 
 *		dbe_seql_init
 * 
 * Creates sequence info list object.
 * 
 * Parameters : 
 * 
 *	p_cache - in out, hold
 *		pointer to cache object
 *
 *	p_freelist - in out, hold
 *		pointer to freelist object
 *
 *	cpnum - in
 *		checkpoint number
 *
 * Return value - give:
 *      pointer to created object
 *
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_seqlist_t* dbe_seql_init(
	dbe_cache_t* p_cache,
	dbe_freelist_t* p_freelist,
	dbe_cpnum_t cpnum)
{
        dbe_seqlist_t* p_seql;

        p_seql = SSMEM_NEW(dbe_seqlist_t);
        p_seql->seql_cache = p_cache;
        p_seql->seql_freelist = p_freelist;
        p_seql->seql_blocksize = dbe_cache_getblocksize(p_seql->seql_cache);
        p_seql->seql_daddr = SU_DADDR_NULL;
        dbe_blh_init(
            &p_seql->seql_header,
            DBE_BLOCK_SEQLIST,
            cpnum);
        p_seql->seql_cacheslot = NULL;
        p_seql->seql_diskbuf = NULL;
        return (p_seql);
}

/*##**********************************************************************\
 * 
 *		dbe_seql_done
 * 
 * Deletes a transaction list object (does not save it).
 * 
 * Parameters : 
 * 
 *	p_seql - in, take
 *		pointer to transaction list object.
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_seql_done(
	dbe_seqlist_t* p_seql)
{
        ss_dassert(p_seql != NULL);
        if (p_seql->seql_cacheslot != NULL) {
            dbe_cache_release(
                p_seql->seql_cache,
                p_seql->seql_cacheslot,
                DBE_CACHE_DIRTYLASTUSE,
                seql_writectx);
        }
        SsMemFree(p_seql);
}

/*#***********************************************************************\
 * 
 *		seql_addlocal
 * 
 * Adds one entry into transaction list
 * 
 * Parameters : 
 * 
 *	p_seql - in out, use
 *		pointer to trx list object
 *		
 *	id - in
 *		squence id
 *		
 *	value - in
 *		sequence value
 *		
 * Return value : 
 *      SU_SUCCESS if OK or
 *      something else on failure
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static su_ret_t seql_addlocal(
        dbe_seqlist_t* p_seql,
	FOUR_BYTE_T id,
        rs_tuplenum_t* value)
{
        su_ret_t rc;
        char* p_trxinf_dbuf;
	FOUR_BYTE_T msl;
	FOUR_BYTE_T lsl;
        dbe_info_t info;

        dbe_info_init(info, DBE_INFO_DISKALLOCNOFAILURE);
        rc = SU_SUCCESS;
        if (p_seql->seql_cacheslot == NULL) {
            rc = dbe_fl_alloc(p_seql->seql_freelist, &p_seql->seql_daddr, &info);
            if (rc != SU_SUCCESS) {
                return (rc);
            }
            p_seql->seql_cacheslot =
                dbe_cache_reach(p_seql->seql_cache,
                                p_seql->seql_daddr,
                                DBE_CACHE_WRITEONLY,
                                DBE_INFO_CHECKPOINT,
                                (char**)&p_seql->seql_diskbuf,
                                seql_writectx);
        }
        p_trxinf_dbuf =
            (char*)p_seql->seql_diskbuf + DBE_BLIST_DATAOFFSET +
                   p_seql->seql_header.bl_nblocks * DBE_SEQLNODE_ITEMSIZE;
        SS_UINT4_STORETODISK(p_trxinf_dbuf, id);
        p_trxinf_dbuf += sizeof(FOUR_BYTE_T);
        msl = rs_tuplenum_getmsl(value);
        lsl = rs_tuplenum_getlsl(value);
        SS_UINT4_STORETODISK(p_trxinf_dbuf, lsl);
        p_trxinf_dbuf += sizeof(FOUR_BYTE_T);
        SS_UINT4_STORETODISK(p_trxinf_dbuf, msl);
        p_seql->seql_header.bl_nblocks++;
        if (p_seql->seql_header.bl_nblocks ==
            DBE_SEQLNODE_CAPACITY(p_seql->seql_blocksize))
        {
            dbe_blh_put(&p_seql->seql_header, p_seql->seql_diskbuf);
            dbe_cache_release(p_seql->seql_cache,
                              p_seql->seql_cacheslot,
                              DBE_CACHE_DIRTYLASTUSE,
                              seql_writectx);
            p_seql->seql_header.bl_nblocks = 0;
            p_seql->seql_header.bl_next = p_seql->seql_daddr;
            p_seql->seql_cacheslot = NULL;
            p_seql->seql_diskbuf = NULL;
            p_seql->seql_daddr = SU_DADDR_NULL;
        }
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_seql_add
 * 
 * Adds one sequence info record to seqlist
 * 
 * Parameters : 
 * 
 *	p_seql - in out, use
 *		pointer to seqlist
 *		
 *	id - in
 *		sequence id
 *		
 *	value - in
 *		sequence value
 *		
 * Return value :
 *      SU_SUCCESS if OK or
 *      something else on failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_seql_add(
	dbe_seqlist_t* p_seql,
	FOUR_BYTE_T id,
        rs_tuplenum_t* value)
{
        su_ret_t rc;

        rc = seql_addlocal(p_seql, id, value);
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_seql_save
 * 
 * Saves sequence info list to disk
 * 
 * Parameters : 
 * 
 *	p_seql - in out, use
 *		pointer to seqlist
 *
 *	p_daddr - out
 *		pointer to variable where the list start address
 *          will be stored.
 *
 * 
 * Return value :
 *      SU_SUCCESS if OK or
 *      something else on failure
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_seql_save(
	dbe_seqlist_t* p_seql,
	su_daddr_t* p_daddr)
{
        su_ret_t rc;

        rc = SU_SUCCESS;
        if (p_seql->seql_header.bl_nblocks > 0) {
            ss_dassert(p_seql->seql_cacheslot != NULL);
            ss_dassert(p_seql->seql_diskbuf != NULL);
            dbe_blh_put(&p_seql->seql_header, p_seql->seql_diskbuf);
            dbe_cache_release(p_seql->seql_cache,
                              p_seql->seql_cacheslot,
                              DBE_CACHE_DIRTYLASTUSE,
                              seql_writectx);
            p_seql->seql_header.bl_nblocks = 0;
            p_seql->seql_header.bl_next = p_seql->seql_daddr;
            p_seql->seql_cacheslot = NULL;
            p_seql->seql_diskbuf = NULL;
            p_seql->seql_daddr = SU_DADDR_NULL;
        }
        *p_daddr = p_seql->seql_header.bl_next;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_seql_deletefromdisk
 * 
 * Deletes disk image of sequence info list by putting it either
 * into free list or into change list according to the cp # of the
 * disk blocks.
 * 
 * Parameters : 
 * 
 *	start_daddr - in
 *		start address of the list;
 *          SU_DADDR_NULL indicates empty list
 *		
 *	cache - in out, use
 *		pointer to disk cache object
 *		
 *	freelist - in out, use
 *		pointer to freelist object
 *		
 *	chlist - in out, use
 *		pointer to changelist object
 *		
 *	cpnum - in
 *		current checkpoint number 
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_seql_deletefromdisk(
        su_daddr_t start_daddr,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_chlist_t* chlist,
        dbe_cpnum_t cpnum)
{
        dbe_cacheslot_t* cacheslot;
        char* dbuf;
        dbe_blheader_t blheader;
        su_ret_t rc;

        SS_PUSHNAME("dbe_seql_deletefromdisk");
        rc = SU_SUCCESS;

        if (start_daddr == 0L) {
            SS_POPNAME;
            return (rc);
        }
        while (start_daddr != SU_DADDR_NULL && rc == SU_SUCCESS) {
            cacheslot = dbe_cache_reach(
                            cache,
                            start_daddr,
                            DBE_CACHE_READONLY,
                            DBE_INFO_CHECKPOINT,
                            &dbuf,
                            seql_delctx);
            dbe_blh_get(&blheader, dbuf);
            dbe_cache_release(
                cache,
                cacheslot,
                DBE_CACHE_CLEANLASTUSE,
                seql_delctx);
            ss_dassert(blheader.bl_type == DBE_BLOCK_SEQLIST);
            if (blheader.bl_cpnum == cpnum) {
                ss_dprintf_2(("dbe_seql_deletefromdisk() freeing: %ld\n",
                              start_daddr));
                ss_dassert(freelist != NULL);
                rc = dbe_fl_free(freelist, start_daddr);
            } else if (blheader.bl_cpnum < cpnum) {
                ss_dprintf_2(("dbe_seql_deletefromdisk() add to chlist: cpnum=%ld daddr=%ld\n",
                              blheader.bl_cpnum, start_daddr));
                ss_dassert(chlist != NULL);
                rc = dbe_cl_add(chlist, blheader.bl_cpnum, start_daddr);
            } else {
                ss_error;
            }
            start_daddr = blheader.bl_next;
        }
        SS_POPNAME;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_seqli_init
 * 
 * Creates a list iterator object for sequence list
 * 
 * Parameters : 
 * 
 *	p_cache - in out, hold
 *		pointer to cache object
 *
 *	daddr - in
 *		disk address where the list starts or
 *          SU_DADDR_NULL to indicate list is empty!
 *
 * Return value - give :
 *      pointer to created object or NULL if empty list
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_seqlist_iter_t* dbe_seqli_init(
	dbe_cache_t* p_cache,
	su_daddr_t daddr)
{
        dbe_seqlist_iter_t* p_seqli;

        if (daddr == SU_DADDR_NULL || daddr == 0L) {
            return (NULL);
        }
        p_seqli = SSMEM_NEW(dbe_seqlist_iter_t);
        p_seqli->seql_cache = p_cache;
        p_seqli->seql_freelist = NULL;
        p_seqli->seql_blocksize = dbe_cache_getblocksize(p_seqli->seql_cache);
        p_seqli->seql_daddr = daddr;
        p_seqli->seql_cacheslot =
            dbe_cache_reach(p_seqli->seql_cache,
                            p_seqli->seql_daddr,
                            DBE_CACHE_READONLY,
                            DBE_INFO_CHECKPOINT,
                            (char**)&p_seqli->seql_diskbuf,
                            seql_readctx);
        dbe_blh_get(&p_seqli->seql_header, p_seqli->seql_diskbuf);
        p_seqli->seql_pos = p_seqli->seql_header.bl_nblocks;
        return (p_seqli);
}

/*##**********************************************************************\
 * 
 *		dbe_seqli_done
 * 
 * Deletes a transaction list iterator object
 * 
 * Parameters : 
 * 
 *	p_seqli - in, take
 *		pointer to transaction list iterator object.
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_seqli_done(
	dbe_seqlist_iter_t* p_seqli)
{
        if (p_seqli == NULL) {
            return;
        }
        if (p_seqli->seql_cacheslot != NULL) {
            dbe_cache_release(p_seqli->seql_cache,
                              p_seqli->seql_cacheslot,
                              DBE_CACHE_CLEANLASTUSE,
                              seql_readctx);
            p_seqli->seql_cacheslot = NULL;
        }
        SsMemFree(p_seqli);
}

/*#***********************************************************************\
 * 
 *		seqli_getnextlocal
 * 
 * Local version of seqlist iterator get next service
 * 
 * Parameters : 
 * 
 *	p_seqli - in out, use
 *		transaction list iterator
 *		
 *	p_id - out, use
 *		pointer to variable where the sequence id will be stored
 *		
 *	p_value - out, use
 *		pointer to variable where the sequence value will be stored
 *		
 * Return value :
 *      TRUE when ok or
 *      FALSE when at and of list
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool seqli_getnextlocal(
	dbe_seqlist_iter_t* p_seqli,
	FOUR_BYTE_T* p_id,
        rs_tuplenum_t* p_value)
{
	FOUR_BYTE_T msl;
	FOUR_BYTE_T lsl;
        char* p_trxinf_dbuf;

        if (p_seqli == NULL) {
            return (FALSE);
        }
        while (p_seqli->seql_pos == 0) {
            ss_dassert(p_seqli->seql_cacheslot != NULL);
            dbe_cache_release(p_seqli->seql_cache,
                              p_seqli->seql_cacheslot,
                              DBE_CACHE_CLEANLASTUSE,
                              seql_readctx);
            p_seqli->seql_cacheslot = NULL;
            p_seqli->seql_diskbuf = NULL;
            if (p_seqli->seql_header.bl_next != SU_DADDR_NULL) {
                p_seqli->seql_daddr = p_seqli->seql_header.bl_next;
                p_seqli->seql_cacheslot =
                    dbe_cache_reach(p_seqli->seql_cache,
                                    p_seqli->seql_daddr,
                                    DBE_CACHE_READONLY,
                                    DBE_INFO_CHECKPOINT,
                                    (char**)&p_seqli->seql_diskbuf,
                                    seql_readctx);
                dbe_blh_get(&p_seqli->seql_header, p_seqli->seql_diskbuf);
                p_seqli->seql_pos = p_seqli->seql_header.bl_nblocks;
            } else {
                return (FALSE);
            }
        }
        p_seqli->seql_pos--;
        p_trxinf_dbuf = (char*)p_seqli->seql_diskbuf + DBE_BLIST_DATAOFFSET +
                        p_seqli->seql_pos * DBE_SEQLNODE_ITEMSIZE;
        *p_id = SS_UINT4_LOADFROMDISK(p_trxinf_dbuf);
        p_trxinf_dbuf += sizeof(FOUR_BYTE_T);
        lsl = SS_UINT4_LOADFROMDISK(p_trxinf_dbuf);
        p_trxinf_dbuf += sizeof(FOUR_BYTE_T);
        msl = SS_UINT4_LOADFROMDISK(p_trxinf_dbuf);
        rs_tuplenum_ulonginit(p_value, msl, lsl);
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		dbe_seqli_getnext
 * 
 * Gets next transaction info from list iterator position
 * 
 * Parameters : 
 * 
 *	p_seqli - in out, use
 *		transaction list iterator
 *		
 *	p_id - out, use
 *		pointer to variable where the sequence id will be stored
 *		
 *	p_value - out, use
 *		pointer to variable where the sequence value will be stored
 *		
 *		
 * Return value :
 *      TRUE when ok or
 *      FALSE when at and of list
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dbe_seqli_getnext(
        dbe_seqlist_iter_t *p_seqli,
	FOUR_BYTE_T* p_id,
        rs_tuplenum_t* p_value)
{
        bool succp;

        ss_dassert(p_id != NULL);
        ss_dassert(p_value != NULL);
        succp = seqli_getnextlocal(p_seqli, p_id, p_value);
        return (succp);
}
