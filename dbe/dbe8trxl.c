/*************************************************************************\
**  source       * dbe8trxl.c
**  directory    * dbe
**  description  * Validating transactions list that needs to be stored
**               * upon creation of checkpoint.
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

This file implements a list of transaction infos
that is stored to disk upon checkpint creation. Each info record
consist of user transaction number (given at the beginning of transaction)
and commit transaction number (the serialization order number).
Two object classes are defined here:

 - The transaction list, which serves only storing the info to disk

 - The transaction list iterator, which is used for reading the transaction
   info list from disk upon crash recovery or at other backing to checkpoint.

The transaction list / list iterator pair is a LIFO, so the list must
be stored in reverse order to get it in straight order when it is read.

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

Objects that are passed as parameters to the dbe_trxl_init() must be
initialized before.

Multithread considerations:
--------------------------

All concurrent access to an object of this class is forbidden.
An external semaphore must be used in multithreaded application.

Example:
-------

Maybe later.

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssdebug.h>
#include <ssmem.h>
#include "dbe9blst.h"
#include "dbe8trxl.h"

#ifdef SS_DEBUG
static char trxl_writectx[] = "Transaction list write";
static char trxl_delctx[] = "Transaction list delete";
static char trxl_readctx[] = "Transaction list read";
#else
#define trxl_writectx NULL
#define trxl_delctx NULL
#define trxl_readctx  NULL
#endif

/* structure to represent transaction list / transaction list iterator */
struct dbe_trxlist_st {
        dbe_blocktype_t     trxl_type;/* DBE_BLOCK_(R)?TRXLIST or ..STMTTRXLIST */
        dbe_cache_t*        trxl_cache;
        dbe_freelist_t*     trxl_freelist;  /* not used by iterator */
        size_t              trxl_blocksize;
        su_daddr_t          trxl_daddr;
        dbe_blheader_t      trxl_header;
        dbe_cacheslot_t*    trxl_cacheslot;
        void*               trxl_diskbuf;
        uint                trxl_pos;   /* only used in iterator */
};


/*#**********************************************************************\
 * 
 *		DBE_TRXLNODE_CAPACITY
 * 
 * Macro that calculates # of transactions info records that fit into
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
#define DBE_TRXLNODE_CAPACITY(blocksize) \
        (((blocksize)-DBE_BLIST_DATAOFFSET)/(sizeof(ss_uint4_t)*2))

/*##**********************************************************************\
 * 
 *		dbe_trxl_init
 * 
 * Creates transaction info list object.
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
 *      blocktype - in
 *          either DBE_BLOCK_TRXLIST or DBE_BLOCK_STMTTRXLIST or
 *          DBE_BLOCK_RTRXLIST
 *
 * Return value - give:
 *      pointer to created object
 *
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_trxlist_t* dbe_trxl_init(
	dbe_cache_t* p_cache,
	dbe_freelist_t* p_freelist,
	dbe_cpnum_t cpnum,
        dbe_blocktype_t blocktype)
{
        dbe_trxlist_t* p_trxl;

        ss_dassert(blocktype == DBE_BLOCK_TRXLIST
                || blocktype == DBE_BLOCK_STMTTRXLIST
                || blocktype == DBE_BLOCK_RTRXLIST);

        p_trxl = SSMEM_NEW(dbe_trxlist_t);
        p_trxl->trxl_type = blocktype;
        p_trxl->trxl_cache = p_cache;
        p_trxl->trxl_freelist = p_freelist;
        p_trxl->trxl_blocksize = dbe_cache_getblocksize(p_trxl->trxl_cache);
        p_trxl->trxl_daddr = SU_DADDR_NULL;
        dbe_blh_init(&p_trxl->trxl_header,
                     p_trxl->trxl_type,
                     cpnum);
        p_trxl->trxl_cacheslot = NULL;
        p_trxl->trxl_diskbuf = NULL;
        return (p_trxl);
}


/*##**********************************************************************\
 * 
 *		dbe_trxl_done
 * 
 * Deletes a transaction list object (does not save it).
 * 
 * Parameters : 
 * 
 *	p_trxl - in, take
 *		pointer to transaction list object.
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_trxl_done(p_trxl)
	dbe_trxlist_t* p_trxl;
{
        ss_dassert(p_trxl != NULL);
        if (p_trxl->trxl_cacheslot != NULL) {
            dbe_cache_release(p_trxl->trxl_cache,
                              p_trxl->trxl_cacheslot,
                              DBE_CACHE_DIRTYLASTUSE,
                              trxl_writectx);
        }
        SsMemFree(p_trxl);
}

/*#***********************************************************************\
 * 
 *		trxl_addlocal
 * 
 * Adds one entry into transaction list
 * 
 * Parameters : 
 * 
 *	p_trxl - in out, use
 *		pointer to trx list object
 *		
 *	trxnum1 - in
 *		first trxnum (always usertrxid)
 *		
 *	trxnum2 - in
 *		second trxid (either statement trxid or committrxnum)
 *		
 *	blocktype - in
 *		either DBE_BLOCK_TRXLIST or DBE_BLOCK_STMTTRXLIST or
 *          DBE_BLOCK_RTRXLIST
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
static su_ret_t trxl_addlocal(
        dbe_trxlist_t* p_trxl,
        ss_uint4_t trxnum1,
        ss_uint4_t trxnum2,
        dbe_blocktype_t blocktype __attribute__ ((unused)))
{
        su_ret_t rc;
        char* p_trxinf_dbuf;
        dbe_info_t info;

        dbe_info_init(info, DBE_INFO_DISKALLOCNOFAILURE);
        ss_dassert(blocktype == p_trxl->trxl_type);
        rc = SU_SUCCESS;
        if (p_trxl->trxl_cacheslot == NULL) {
            FAKE_CODE_BLOCK(
                FAKE_DBE_TRXL_CFGEXCEEDED,
                {
                    SsDbgPrintf("FAKE_DBE_TRXL_CFGEXCEEDED\n");
                    return(SU_ERR_FILE_WRITE_CFG_EXCEEDED);
                }            
            )
            rc = dbe_fl_alloc(p_trxl->trxl_freelist, &p_trxl->trxl_daddr, &info);
            if (rc != SU_SUCCESS) {
                return (rc);
            }
            ss_dprintf_4(("trxl_addlocal() alloc: %ld\n",
                              p_trxl->trxl_daddr));
            p_trxl->trxl_cacheslot =
                dbe_cache_reach(p_trxl->trxl_cache,
                                p_trxl->trxl_daddr,
                                DBE_CACHE_WRITEONLY,
                                DBE_INFO_CHECKPOINT,
                                (char**)&p_trxl->trxl_diskbuf,
                                trxl_writectx);
        }
        p_trxinf_dbuf =
            (char*)p_trxl->trxl_diskbuf + DBE_BLIST_DATAOFFSET +
                   p_trxl->trxl_header.bl_nblocks * (sizeof(ss_uint4_t)*2);
        SS_UINT4_STORETODISK(p_trxinf_dbuf, trxnum1);
        p_trxinf_dbuf += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(p_trxinf_dbuf, trxnum2);
        p_trxl->trxl_header.bl_nblocks++;
        if (p_trxl->trxl_header.bl_nblocks ==
            DBE_TRXLNODE_CAPACITY(p_trxl->trxl_blocksize))
        {
            dbe_blh_put(&p_trxl->trxl_header, p_trxl->trxl_diskbuf);
            dbe_cache_release(p_trxl->trxl_cache,
                              p_trxl->trxl_cacheslot,
                              DBE_CACHE_DIRTYLASTUSE,
                              trxl_writectx);
            p_trxl->trxl_header.bl_nblocks = 0;
            p_trxl->trxl_header.bl_next = p_trxl->trxl_daddr;
            p_trxl->trxl_cacheslot = NULL;
            p_trxl->trxl_diskbuf = NULL;
            p_trxl->trxl_daddr = SU_DADDR_NULL;
        }
        return (rc);
}
/*##**********************************************************************\
 * 
 *		dbe_trxl_add
 * 
 * Adds one transaction info record to trxlist
 * 
 * Parameters : 
 * 
 *	p_trxl - in out, use
 *		pointer to trxlist
 *		
 *	usertrxid - in
 *		user trx id
 *		
 *	committrxnum - in
 *		trx commit ordering #
 *		
 * Return value :
 *      SU_SUCCESS if OK or
 *      something else on failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_trxl_add(p_trxl, usertrxid, committrxnum)
	dbe_trxlist_t* p_trxl;
	dbe_trxid_t usertrxid;
	dbe_trxnum_t committrxnum;
{
        su_ret_t rc;

        rc = trxl_addlocal(
                p_trxl,
                (ss_uint4_t)DBE_TRXID_GETLONG(usertrxid),
                (ss_uint4_t)DBE_TRXNUM_GETLONG(committrxnum),
                (dbe_blocktype_t)DBE_BLOCK_TRXLIST);
        return (rc);
}


su_ret_t dbe_trxl_addstmttrx(
        dbe_trxlist_t *p_trxl,
        dbe_trxid_t usertrxid,
        dbe_trxid_t stmttrxid)
{
        su_ret_t rc;

        rc = trxl_addlocal(
                p_trxl,
                (ss_uint4_t)DBE_TRXID_GETLONG(usertrxid),
                (ss_uint4_t)DBE_TRXID_GETLONG(stmttrxid),
                (dbe_blocktype_t)DBE_BLOCK_STMTTRXLIST);
        return (rc);
}

#ifdef DBE_REPLICATION

su_ret_t dbe_trxl_addrtrx(
        dbe_trxlist_t *p_trxl,
        dbe_trxid_t remotetrxid,
        dbe_trxid_t localtrxid)
{
        su_ret_t rc;

        rc = trxl_addlocal(
                p_trxl,
                (ss_uint4_t)DBE_TRXID_GETLONG(remotetrxid),
                (ss_uint4_t)DBE_TRXID_GETLONG(localtrxid),
                (dbe_blocktype_t)DBE_BLOCK_RTRXLIST);
        return (rc);
}

#endif /* DBE_REPLICATION */

/*##**********************************************************************\
 * 
 *		dbe_trxl_save
 * 
 * Saves transaction info list to disk
 * 
 * Parameters : 
 * 
 *	p_trxl - in out, use
 *		pointer to trxlist
 *
 *	p_daddr - out
 *		pointer to variable where the list start address
 *          will be stored.
 *
 * 
 * Return value : SU_SUCCESS if OK or
 *                something else on failure
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_trxl_save(p_trxl, p_daddr)
	dbe_trxlist_t* p_trxl;
	su_daddr_t* p_daddr;
{
        su_ret_t rc;

        rc = SU_SUCCESS;
        if (p_trxl->trxl_header.bl_nblocks > 0) {
            ss_dassert(p_trxl->trxl_cacheslot != NULL);
            ss_dassert(p_trxl->trxl_diskbuf != NULL);
            dbe_blh_put(&p_trxl->trxl_header, p_trxl->trxl_diskbuf);
            dbe_cache_release(p_trxl->trxl_cache,
                              p_trxl->trxl_cacheslot,
                              DBE_CACHE_DIRTYLASTUSE,
                              trxl_writectx);
            p_trxl->trxl_header.bl_nblocks = 0;
            p_trxl->trxl_header.bl_next = p_trxl->trxl_daddr;
            p_trxl->trxl_cacheslot = NULL;
            p_trxl->trxl_diskbuf = NULL;
            p_trxl->trxl_daddr = SU_DADDR_NULL;
        }
        *p_daddr = p_trxl->trxl_header.bl_next;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_trxl_deletefromdisk
 * 
 * Deletes disk image of transaction info list by putting it either
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
su_ret_t dbe_trxl_deletefromdisk(
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

        SS_PUSHNAME("dbe_trxl_deletefromdisk");
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
                            trxl_delctx);
            dbe_blh_get(&blheader, dbuf);
            dbe_cache_release(
                cache,
                cacheslot,
                DBE_CACHE_CLEANLASTUSE,
                trxl_delctx);
            ss_dassert(blheader.bl_type == DBE_BLOCK_TRXLIST
                    || blheader.bl_type == DBE_BLOCK_STMTTRXLIST
                    || blheader.bl_type == DBE_BLOCK_RTRXLIST);
            if (blheader.bl_cpnum == cpnum) {
                ss_dprintf_2(("dbe_trxl_deletefromdisk() freeing: %ld\n",
                              start_daddr));
                ss_dassert(freelist != NULL);
                rc = dbe_fl_free(freelist, start_daddr);
            } else if (blheader.bl_cpnum < cpnum) {
                ss_dprintf_2(("dbe_trxl_deletefromdisk() add to chlist: cpnum=%ld daddr=%ld\n",
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
 *		dbe_trxli_init
 * 
 * Creates a list iterator object for transaction list
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
 *      blocktype - in
 *          either DBE_BLOCK_TRXLIST or DBE_BLOCK_STMTTRXLIST
 *          or DBE_BLOCK_RTRXLIST
 *
 * Return value - give :
 *      pointer to created object or NULL if empty list
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_trxlist_iter_t* dbe_trxli_init(
	dbe_cache_t* p_cache,
	su_daddr_t daddr,
        dbe_blocktype_t blocktype)
{
        dbe_trxlist_iter_t* p_trxli;

        ss_dassert(blocktype == DBE_BLOCK_TRXLIST
                || blocktype == DBE_BLOCK_STMTTRXLIST
                || blocktype == DBE_BLOCK_RTRXLIST);
        if (daddr == SU_DADDR_NULL || daddr == 0L) {
            return (NULL);
        }
        p_trxli = SSMEM_NEW(dbe_trxlist_iter_t);
        p_trxli->trxl_type = blocktype;
        p_trxli->trxl_cache = p_cache;
        p_trxli->trxl_freelist = NULL;
        p_trxli->trxl_blocksize = dbe_cache_getblocksize(p_trxli->trxl_cache);
        p_trxli->trxl_daddr = daddr;
        p_trxli->trxl_cacheslot =
            dbe_cache_reach(p_trxli->trxl_cache,
                            p_trxli->trxl_daddr,
                            DBE_CACHE_READONLY,
                            DBE_INFO_CHECKPOINT,
                            (char**)&p_trxli->trxl_diskbuf,
                            trxl_readctx);
        dbe_blh_get(&p_trxli->trxl_header, p_trxli->trxl_diskbuf);
        p_trxli->trxl_pos = p_trxli->trxl_header.bl_nblocks;
        return (p_trxli);
}


/*##**********************************************************************\
 * 
 *		dbe_trxli_done
 * 
 * Deletes a transaction list iterator object
 * 
 * Parameters : 
 * 
 *	p_trxli - in, take
 *		pointer to transaction list iterator object.
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_trxli_done(p_trxli)
	dbe_trxlist_iter_t* p_trxli;
{
        if (p_trxli == NULL) {
            return;
        }
        if (p_trxli->trxl_cacheslot != NULL) {
            dbe_cache_release(p_trxli->trxl_cache,
                              p_trxli->trxl_cacheslot,
                              DBE_CACHE_CLEANLASTUSE,
                              trxl_readctx);
            p_trxli->trxl_cacheslot = NULL;
        }
        SsMemFree(p_trxli);
}


/*#***********************************************************************\
 * 
 *		trxli_getnextlocal
 * 
 * Local version of trxlist iterator get next service
 * 
 * Parameters : 
 * 
 *	p_trxli - in out, use
 *		transaction list iterator
 *		
 *	p_trxnum1 - out, use
 *		pointer to variable where the 1st trx id will be stored
 *          (always the usertrxid)
 *		
 *	p_committrxnum - out, use
 *		pointer to variable where the second trx id will be stored
 *          (either commit trxnum or statement trx id)
 *		
 *	blocktype - in
 *		type of trx list (for check purposes)
 *		
 * Return value : TRUE when ok or
 *                FALSE when at and of list
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool trxli_getnextlocal(
        dbe_trxlist_iter_t* p_trxli,
        ss_uint4_t* p_trxnum1,
        ss_uint4_t* p_trxnum2,
        dbe_blocktype_t blocktype __attribute__ ((unused)))
{
        char* p_trxinf_dbuf;

        if (p_trxli == NULL) {
            return (FALSE);
        }
        ss_dassert(p_trxnum1 != NULL);
        ss_dassert(p_trxnum2 != NULL);
        ss_dassert(p_trxli->trxl_type == blocktype);
        while (p_trxli->trxl_pos == 0) {
            ss_dassert(p_trxli->trxl_cacheslot != NULL);
            dbe_cache_release(p_trxli->trxl_cache,
                              p_trxli->trxl_cacheslot,
                              DBE_CACHE_CLEANLASTUSE,
                              trxl_readctx);
            p_trxli->trxl_cacheslot = NULL;
            p_trxli->trxl_diskbuf = NULL;
            if (p_trxli->trxl_header.bl_next != SU_DADDR_NULL) {
                p_trxli->trxl_daddr = p_trxli->trxl_header.bl_next;
                p_trxli->trxl_cacheslot =
                    dbe_cache_reach(p_trxli->trxl_cache,
                                    p_trxli->trxl_daddr,
                                    DBE_CACHE_READONLY,
                                    DBE_INFO_CHECKPOINT,
                                    (char**)&p_trxli->trxl_diskbuf,
                                    trxl_readctx);
                dbe_blh_get(&p_trxli->trxl_header, p_trxli->trxl_diskbuf);
                p_trxli->trxl_pos = p_trxli->trxl_header.bl_nblocks;
            } else {
                return (FALSE);
            }
        }
        p_trxli->trxl_pos--;
        p_trxinf_dbuf = (char*)p_trxli->trxl_diskbuf + DBE_BLIST_DATAOFFSET +
                        p_trxli->trxl_pos * (sizeof(ss_uint4_t)*2);
        *p_trxnum1 = SS_UINT4_LOADFROMDISK(p_trxinf_dbuf);
        p_trxinf_dbuf += sizeof(ss_uint4_t);
        *p_trxnum2 = SS_UINT4_LOADFROMDISK(p_trxinf_dbuf);
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		dbe_trxli_getnext
 * 
 * Gets next transaction info from list iterator position
 * 
 * Parameters : 
 * 
 *	p_trxli - in out, use
 *		transaction list iterator
 *		
 *	p_usertrxid - out, use
 *		pointer to variable where the user trx id will be stored
 *		
 *	p_committrxnum - out, use
 *		pointer to variable where the commit trx number will be stored
 *		
 *		
 * Return value : TRUE when ok or
 *                FALSE when at and of list
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dbe_trxli_getnext(
        dbe_trxlist_iter_t *p_trxli,
        dbe_trxid_t* p_usertrxid,
        dbe_trxnum_t* p_committrxnum)
{
        ss_uint4_t trxid1;
        ss_uint4_t trxnum2;
        bool succp;

        ss_dassert(p_usertrxid != NULL);
        ss_dassert(p_committrxnum != NULL);
        succp = trxli_getnextlocal(p_trxli, &trxid1, &trxnum2, DBE_BLOCK_TRXLIST);
        if (succp) {
            *p_usertrxid = DBE_TRXID_INIT(trxid1);
            *p_committrxnum = DBE_TRXNUM_INIT(trxnum2);
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *		dbe_trxli_getnextstmttrx
 * 
 * Gets next statement trx record
 * 
 * Parameters : 
 * 
 *	p_trxli - in out, use
 *		transaction list iterator
 *		
 *	p_usertrxid - out, use
 *		pointer to variable where the user trx id will be stored
 *		
 *	p_stmttrxid - out, use
 *		pointer to variable where the statement trx id will be stored
 *		
 * Return value : TRUE when ok or
 *                FALSE when at and of list
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dbe_trxli_getnextstmttrx(
        dbe_trxlist_iter_t *p_trxli,
        dbe_trxid_t* p_usertrxid,
        dbe_trxid_t* p_stmttrxid)
{
        ss_uint4_t trxid1;
        ss_uint4_t trxid2;
        bool succp;

        ss_dassert(p_usertrxid != NULL);
        ss_dassert(p_stmttrxid != NULL);
        succp = trxli_getnextlocal(p_trxli, &trxid1, &trxid2, DBE_BLOCK_STMTTRXLIST);
        if (succp) {
            *p_usertrxid = DBE_TRXID_INIT(trxid1);
            *p_stmttrxid = DBE_TRXID_INIT(trxid2);
        }
        return (succp);
}

#ifdef DBE_REPLICATION

bool dbe_trxli_getnextrtrx(
        dbe_trxlist_iter_t *p_trxli,
        dbe_trxid_t* p_remotetrxid,
        dbe_trxid_t* p_localtrxid)
{
        ss_uint4_t trxid1;
        ss_uint4_t trxid2;
        bool succp;

        ss_dassert(p_remotetrxid != NULL);
        ss_dassert(p_localtrxid != NULL);
        succp = trxli_getnextlocal(p_trxli, &trxid1, &trxid2, DBE_BLOCK_RTRXLIST);
        if (succp) {
            *p_remotetrxid = DBE_TRXID_INIT(trxid1);
            *p_localtrxid = DBE_TRXID_INIT(trxid2);
        }
        return (succp);
}

#endif /* DBE_REPLICATION */
