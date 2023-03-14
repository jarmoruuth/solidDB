/*************************************************************************\
**  source       * dbe6cpm.c
**  directory    * dbe
**  description  * Checkpoint Manager
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

See documentation from file dbecp.doc

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

Re-entrant but cannot be used for same database object in concurrent threads.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstring.h>
#include <sstime.h>

#include <ssc.h>
#include <ssmem.h>

#include <su0icvt.h>

#include "dbe9type.h"
#include "dbe9bhdr.h"
#include "dbe8trxl.h"
#include "dbe8seql.h"
#include "dbe7trxb.h"
#include "dbe1seq.h"
#include "dbe6cpm.h"

#ifdef SS_DEBUG
static char cpm_cprecreadctx[] = "Checkpoint manager read CP record";
static char cpm_cprecwritectx[] = "Checkpoint manager write CP record";
#else 
#define cpm_cprecreadctx    NULL
#define cpm_cprecwritectx   NULL
#endif

/* Checkpoint record structure */
struct dbe_cprec_st {
        su_daddr_t          cp_daddr;       /* disk address where stored */

        /* 1:1 correspondence with disk image:
        ** (only integer byte order & alignment may differ)
        */
        dbe_blocktype_t     cp_blktype;     /* 1 byte */
        dbe_cpnum_t         cp_cpnum;       /* 4 bytes */
        SsTimeT             cp_time;        /* 4 bytes time stamp */
        su_daddr_t          cp_oldcplistaddr;/* 4 bytes old cplist daddr */
        dbe_startrec_t      cp_startrec;    /* 256 bytes start record */
        char                cp_reserved[512-1-4-4-4-256];
};

/* Checkpoint manager object type */
struct dbe_cpmgr_st {
        dbe_blocktype_t cpm_type;       /* DBE_BLOCK_CPRECORD or
                                           DBE_BLOCK_SSRECORD */
        dbe_cpnum_t     cpm_cpnum;      /* checkpoint/snapshot # */
        dbe_file_t*     cpm_dbfile;

        dbe_cpnum_t     cpm_delcpnum;   /* checkpoint/snapshot # to delete */
        dbe_cpnum_t     cpm_nextcpnum;  /* next checkpoint/snapsot # */
        dbe_cpnum_t     cpm_prevcpnum;  /* previous checkpoint/snapsot # */
        int             cpm_flushfileno;
        SsTimeT         cpm_time;
};

/* local functions */
static dbe_ret_t cpmgr_prepare_file(
            dbe_cpmgr_t* cpmgr,
            dbe_filedes_t* filedes);

static void cpmgr_inheritchlist_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes);

static void cpmgr_putcprectochlist_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes);

static void cpmgr_createcp_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes);

static void cpmgr_updateheader_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes);

static void cprec_save(
        dbe_filedes_t* filedes,
        dbe_cprec_t* cprec);

static void cprec_restore(
        dbe_filedes_t* filedes,
        dbe_cprec_t* cprec,
        su_daddr_t daddr);

static void cpmgr_deletecpstep1_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes);

static void cpmgr_deletecpstep2_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes);

static dbe_ret_t cpmgr_restorestartrec_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes);

#ifdef SS_DEBUG

int dbe_cpmgr_dbgcrashpoint = 0;

#endif
/*##**********************************************************************\
 * 
 *		dbe_cpmgr_init
 * 
 * Creates a new Checkpoint manager
 * 
 * Parameters : 
 * 
 *	dbfile - in, hold
 *		pointer to dbfile object
 *
 *      admin_gate - in, hold
 *          pointer to gate semaphore for administrative operations
 *		
 * Return value - give : 
 *      pointer to created checkpoint manager
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_cpmgr_t* dbe_cpmgr_init(
        dbe_file_t* dbfile)
{
        dbe_cpmgr_t* cpmgr;

        ss_dassert(dbfile != NULL);
        cpmgr = SSMEM_NEW(dbe_cpmgr_t);
        cpmgr->cpm_type = DBE_BLOCK_FREE;
        cpmgr->cpm_cpnum = 0L;
        cpmgr->cpm_dbfile = dbfile;

        cpmgr->cpm_delcpnum = 0L;
        cpmgr->cpm_nextcpnum = 0L;
        cpmgr->cpm_prevcpnum = 0L;
        cpmgr->cpm_flushfileno = -1;
        cpmgr->cpm_time = 0L;
        return (cpmgr);
}

/*##**********************************************************************\
 * 
 *		dbe_cpmgr_done
 * 
 * Deletes a checkpoint manager object
 * 
 * Parameters : 
 * 
 *	cpmgr - in, take
 *		pointer to the checkpoint manager to delete
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_cpmgr_done(dbe_cpmgr_t* cpmgr)
{
        ss_dassert(cpmgr != NULL);
        SsMemFree(cpmgr);
}

/*##**********************************************************************\
 * 
 *		dbe_cpmgr_prepare
 * 
 * Makes preparations for checkpoint creation which can be done
 * concurrently with database updates
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager object
 *		
 *	cpnum - in
 *		# of checkpoint to create
 *		
 *	type - in
 *		type of checkpoint (checkpoint or snapshot)
 *
 *      ts - in
 *          checkpoint timestamp
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_ret_t dbe_cpmgr_prepare(
        dbe_cpmgr_t* cpmgr,
        dbe_cpnum_t cpnum,
        dbe_blocktype_t type,
        SsTimeT ts)
{
        dbe_ret_t rc;
        dbe_filedes_t* filedes;
        uint i;

        ss_dassert(cpmgr != NULL);
        ss_dassert(type == DBE_BLOCK_CPRECORD || type == DBE_BLOCK_SSRECORD);

        cpmgr->cpm_type = type;
        cpmgr->cpm_cpnum = cpnum;
        cpmgr->cpm_time = ts;
        rc = cpmgr_prepare_file(
                cpmgr,
                cpmgr->cpm_dbfile->f_indexfile);
        if (cpmgr->cpm_dbfile->f_blobfiles != NULL) {
            ss_error;
            su_pa_do_get(cpmgr->cpm_dbfile->f_blobfiles, i, filedes) {
                cpmgr_prepare_file(cpmgr, filedes);
            }
        }
        return(rc);
}


/*##**********************************************************************\
 * 
 *		dbe_cpmgr_createcp
 * 
 * Does the checkpoint creation (although the whole operation is not
 * completed after this step, yet). After this step the cpnum
 * counter will be incremented and no currently allocated disk block
 * will be overwritten by any client request. 
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager object
 *		
 *	trxbuf - in out, use
 *		pointer to transaction buffer
 *		
 *	seq - in out, use
 *		pointer to sequence object
 *		
 *	bonsairoot - in
 *		bonsai-tree root disk address
 *		
 *	permroot - in
 *		permanent tree root disk address
 *		
 *	mmiroot - in
 *		main memory index root disk address
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_cpmgr_createcp(
        dbe_cpmgr_t* cpmgr,
        dbe_trxbuf_t* trxbuf,
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_seq_t* seq,
        su_daddr_t bonsairoot,
        su_daddr_t permroot,
        su_daddr_t mmiroot)
{
        dbe_filedes_t* filedes;
        uint i;
        dbe_ret_t rc;

        ss_dassert(cpmgr != NULL);

        filedes = cpmgr->cpm_dbfile->f_indexfile;

        filedes->fd_cprec->cp_startrec.sr_bonsairoot = bonsairoot;
        filedes->fd_cprec->cp_startrec.sr_permroot = permroot;
        filedes->fd_cprec->cp_startrec.sr_mmiroot = mmiroot;

        /* save transaction info lists to disk */
        rc = dbe_trxbuf_save(
                trxbuf,
                filedes->fd_cache,
                filedes->fd_freelist,
                cpmgr->cpm_cpnum,
                &filedes->fd_cprec->cp_startrec.sr_trxlistaddr,
                &filedes->fd_cprec->cp_startrec.sr_stmttrxlistaddr);
        ss_assert(rc == DBE_RC_SUCC);
#ifdef DBE_REPLICATION
        if (rtrxbuf != NULL) {
            rc = dbe_rtrxbuf_save(
                    rtrxbuf,
                    filedes->fd_cache,
                    filedes->fd_freelist,
                    cpmgr->cpm_cpnum,
                    &filedes->fd_cprec->cp_startrec.sr_rtrxlistaddr);
        }
#endif /* DBE_REPLICATION */
#ifndef SS_NOSEQUENCE
        rc = dbe_seq_save_nomutex(
                seq,
                filedes->fd_cache,
                filedes->fd_freelist,
                cpmgr->cpm_cpnum,
                &filedes->fd_cprec->cp_startrec.sr_sequencelistaddr);
        su_rc_assert(rc == DBE_RC_SUCC, rc);
#endif /* SS_NOSEQUENCE */

        /* save change list & free list & cp record of each file */
        cpmgr_createcp_file(
            cpmgr,
            filedes);
        if (cpmgr->cpm_dbfile->f_blobfiles != NULL) {
            su_pa_do_get(cpmgr->cpm_dbfile->f_blobfiles, i, filedes) {
                cpmgr_createcp_file(
                    cpmgr,
                    filedes);
            }
        }
}


/*##**********************************************************************\
 * 
 *		dbe_cpmgr_flushstep
 * 
 * Makes 1 step of concurrent flushing. (Currently does everything
 * at one call)
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager object
 *		
 * Return value :
 *      TRUE if the call has to be repeated or
 *      FALSE when the operation is completed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dbe_cpmgr_flushstep(dbe_cpmgr_t* cpmgr)
{
        dbe_filedes_t* filedes;
        bool ismore;

        if (cpmgr->cpm_flushfileno == -1) {
            cpmgr->cpm_flushfileno = 0;
        }
        if (cpmgr->cpm_flushfileno == 0) {
            filedes = cpmgr->cpm_dbfile->f_indexfile;
            ismore = dbe_cache_concurrent_flushstep(filedes->fd_cache, 20, DBE_INFO_CHECKPOINT);
        } else if (cpmgr->cpm_dbfile->f_blobfiles != NULL
            &&     su_pa_indexinuse(cpmgr->cpm_dbfile->f_blobfiles,
                                    cpmgr->cpm_flushfileno))
        {
            filedes = su_pa_getdata(cpmgr->cpm_dbfile->f_blobfiles,
                                    cpmgr->cpm_flushfileno);
            ismore = dbe_cache_concurrent_flushstep(filedes->fd_cache, 20, DBE_INFO_CHECKPOINT);
        } else {
            ismore = FALSE;
        }
        if (!ismore) {
            cpmgr->cpm_flushfileno++;
            if (cpmgr->cpm_dbfile->f_blobfiles != NULL
            &&  su_pa_indexinuse(cpmgr->cpm_dbfile->f_blobfiles,
                                 cpmgr->cpm_flushfileno))
            {
                ismore = TRUE;
            } else {
                cpmgr->cpm_flushfileno = -1;
            }
        }
        return (ismore);
}

/*##**********************************************************************\
 * 
 *		dbe_cpmgr_updateheaders
 * 
 * Updates header records before saving them to disk.
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to cp manager
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_cpmgr_updateheaders(dbe_cpmgr_t* cpmgr)
{
        dbe_filedes_t* filedes;
        uint i;

        filedes = cpmgr->cpm_dbfile->f_indexfile;
        cpmgr_updateheader_file(cpmgr, filedes);
        if (cpmgr->cpm_dbfile->f_blobfiles != NULL) {
            su_pa_do_get(cpmgr->cpm_dbfile->f_blobfiles, i, filedes) {
                cpmgr_updateheader_file(cpmgr, filedes);
            }
        }
}


/*##**********************************************************************\
 * 
 *		dbe_cpmgr_inheritchlist
 * 
 * Inherits live change records from the checkpoint change list
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_cpmgr_inheritchlist(dbe_cpmgr_t* cpmgr)
{
        dbe_filedes_t* filedes;
        uint i;
        su_ret_t rc;

        filedes = cpmgr->cpm_dbfile->f_indexfile;
        cpmgr_inheritchlist_file(cpmgr, filedes);
        if (cpmgr->cpm_dbfile->f_blobfiles != NULL) {
            su_pa_do_get(cpmgr->cpm_dbfile->f_blobfiles, i, filedes) {
                cpmgr_inheritchlist_file(cpmgr, filedes);
            }
        }

        /* "Delete" transaction list from checkpoint.
        ** Actually, the trx list blocks are just put into change
        ** list and freed at deletion of the checkpoint
        */
        filedes = cpmgr->cpm_dbfile->f_indexfile;
        rc = dbe_trxl_deletefromdisk(
                filedes->fd_cprec->cp_startrec.sr_trxlistaddr,
                filedes->fd_cache,
                NULL,   /* filedes->fd_freelist */
                filedes->fd_chlist,
                cpmgr->cpm_cpnum + 1);
        su_rc_assert(rc == SU_SUCCESS, rc);
        rc = dbe_trxl_deletefromdisk(
                filedes->fd_cprec->cp_startrec.sr_stmttrxlistaddr,
                filedes->fd_cache,
                NULL,   /* filedes->fd_freelist */
                filedes->fd_chlist,
                cpmgr->cpm_cpnum + 1);
        su_rc_assert(rc == SU_SUCCESS, rc);

#ifdef DBE_REPLICATION
        rc = dbe_trxl_deletefromdisk(
                filedes->fd_cprec->cp_startrec.sr_rtrxlistaddr,
                filedes->fd_cache,
                NULL,   /* filedes->fd_freelist */
                filedes->fd_chlist,
                cpmgr->cpm_cpnum + 1);
        su_rc_assert(rc == SU_SUCCESS, rc);
#endif /* DBE_REPLICATION */

#ifndef SS_NOSEQUENCE
        /* "Delete" also sequence list from checkpoint. */
        rc = dbe_seql_deletefromdisk(
                filedes->fd_cprec->cp_startrec.sr_sequencelistaddr,
                filedes->fd_cache,
                NULL,   /* filedes->fd_freelist */
                filedes->fd_chlist,
                cpmgr->cpm_cpnum + 1);
        su_rc_assert(rc == SU_SUCCESS, rc);
#endif /* SS_NOSEQUENCE */

        /* */
        cpmgr_putcprectochlist_file(cpmgr, filedes);
        if (cpmgr->cpm_dbfile->f_blobfiles != NULL) {
            su_pa_do_get(cpmgr->cpm_dbfile->f_blobfiles, i, filedes) {
                cpmgr_putcprectochlist_file(cpmgr, filedes);
            }
        }
}

/*##**********************************************************************\
 * 
 *		dbe_cpmgr_deletecp
 * 
 * Deletes a checkpoint
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager object
 *		
 *	cpnum - in
 *		checkpoint/snapshot # to delete
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_cpmgr_deletecp(
        dbe_cpmgr_t* cpmgr,
        dbe_cpnum_t cpnum)
{
        dbe_filedes_t* filedes;
        uint i;


        cpmgr->cpm_delcpnum = cpnum;
        filedes = cpmgr->cpm_dbfile->f_indexfile;
        cpmgr->cpm_nextcpnum = dbe_cpl_nextto(filedes->fd_cplist, cpnum);
        ss_dassert(cpmgr->cpm_nextcpnum != 0L);
        cpmgr->cpm_prevcpnum = dbe_cpl_prevfrom(filedes->fd_cplist, cpnum);
        
        cpmgr_deletecpstep1_file(cpmgr, filedes);
        if (cpmgr->cpm_dbfile->f_blobfiles != NULL) {
            su_pa_do_get(cpmgr->cpm_dbfile->f_blobfiles, i, filedes) {
                cpmgr_deletecpstep1_file(cpmgr, filedes);
            }
        }
        dbe_file_saveheaders(cpmgr->cpm_dbfile);
        filedes = cpmgr->cpm_dbfile->f_indexfile;
        cpmgr_deletecpstep2_file(cpmgr, filedes);
        if (cpmgr->cpm_dbfile->f_blobfiles != NULL) {
            su_pa_do_get(cpmgr->cpm_dbfile->f_blobfiles, i, filedes) {
                cpmgr_deletecpstep2_file(cpmgr, filedes);
            }
        }
}


/*##**********************************************************************\
 * 
 *		dbe_cpmgr_prevcheckpoint
 * 
 * Gets previous checpoint number from given position. If previous number
 * either does not exist or is a snapshot, returns 0
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager
 *		
 *	from - in
 *		checkpoint/snapshot number position
 *		
 * Return value :
 *      explained above
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_cpnum_t dbe_cpmgr_prevcheckpoint(
        dbe_cpmgr_t* cpmgr,
        dbe_cpnum_t from)
{
        dbe_filedes_t* filedes;
        dbe_cplist_t* cplist;
        dbe_cpnum_t cpnum;
        su_daddr_t daddr;
        dbe_cacheslot_t* cacheslot;
        char* dbuf;
        dbe_blocktype_t blocktype;

        filedes = cpmgr->cpm_dbfile->f_indexfile;
        cplist = filedes->fd_cplist;
        cpnum = dbe_cpl_prevfrom(cplist, from);
        if (cpnum != 0L) {
            daddr = dbe_cpl_getdaddr(cplist, cpnum);
            ss_dassert(daddr != SU_DADDR_NULL);
            cacheslot = dbe_cache_reach(
                            filedes->fd_cache,
                            daddr,
                            DBE_CACHE_READONLY,
                            DBE_INFO_CHECKPOINT,
                            &dbuf,
                            cpm_cprecreadctx);
            ss_dassert(cacheslot != NULL);
            DBE_BLOCK_GETTYPE(dbuf, &blocktype);
            dbe_cache_release(
                filedes->fd_cache,
                cacheslot,
                DBE_CACHE_CLEANLASTUSE,
                cpm_cprecreadctx);
            if (blocktype == DBE_BLOCK_CPRECORD) {
                return (cpnum);
            }
            if (blocktype == DBE_BLOCK_SSRECORD) {
                return (0L);
            } else {
                ss_dprintf_2(("dbe_cpmgr_prevcheckpoint(): from=%ld, prev=%ld, daddr=%ld blocktype=%d\n",
                             from, cpnum, daddr, (int)blocktype));
                ss_rc_error(blocktype);
            }
        }
        return (cpnum);
}

/*##**********************************************************************\
 * 
 *		dbe_cpmgr_newest
 * 
 * Gets newest checkpoint/snapshot # or 0 if none exists
 * 
 * Parameters : 
 * 
 *	cpmgr - in, use
 *		pointer to checkpoint manager 
 *		
 * Return value :
 *      described above
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_cpnum_t dbe_cpmgr_newest(
        dbe_cpmgr_t* cpmgr)
{
        dbe_cplist_t* cplist;
        dbe_cpnum_t cpnum;

        cplist = cpmgr->cpm_dbfile->f_indexfile->fd_cplist;
        cpnum = dbe_cpl_last(cplist);
        return (cpnum);
}


/*##**********************************************************************\
 * 
 *		dbe_cpmgr_isalive
 * 
 * Check whether given checkpoint/snapshot number is found from live
 * checkpoint list.
 * 
 * Parameters : 
 * 
 *	cpmgr - in, use
 *		pointer to checkpoint manager 
 *		
 *	cpnum - in
 *		checkpoint/snapshot number
 *		
 * Return value :
 *      TRUE if cpnum is alive,
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dbe_cpmgr_isalive(
        dbe_cpmgr_t* cpmgr,
        dbe_cpnum_t cpnum)
{
        dbe_cplist_t* cplist;
        bool isalive;

        cplist = cpmgr->cpm_dbfile->f_indexfile->fd_cplist;
        isalive = dbe_cpl_isalive(cplist, cpnum);
        return (isalive);
}


/*##**********************************************************************\
 * 
 *		dbe_cpmgr_restorestartrec
 * 
 * Restores start record of each database random-access file (at revert
 * to checkpoint operation).
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code when failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_ret_t dbe_cpmgr_restorestartrec(
        dbe_cpmgr_t* cpmgr)
{
        dbe_ret_t rc;
        dbe_filedes_t* filedes;
        uint i;

        filedes = cpmgr->cpm_dbfile->f_indexfile;
        rc = cpmgr_restorestartrec_file(cpmgr, filedes);
        if (rc == DBE_RC_SUCC) {
            cpmgr->cpm_time = filedes->fd_cprec->cp_time;
        }
        DBE_CPMGR_CRASHPOINT(23);
        if (rc == DBE_RC_SUCC && cpmgr->cpm_dbfile->f_blobfiles != NULL) {
            su_pa_do_get(cpmgr->cpm_dbfile->f_blobfiles, i, filedes) {
                rc = cpmgr_restorestartrec_file(cpmgr, filedes);
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                ss_dassert(cpmgr->cpm_time == filedes->fd_cprec->cp_time);
            }
        }
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_cpmgr_gettimestamp
 * 
 * Returns checkpoint timestamp of the newest checkpoint
 * 
 * Parameters : 
 * 
 *	cpmgr - use
 *		cp manager
 *		
 * Return value :
 *      timestamp
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsTimeT dbe_cpmgr_gettimestamp(
        dbe_cpmgr_t* cpmgr)
{
        ss_dassert(cpmgr != NULL);
        return (cpmgr->cpm_time);
}

/*##**********************************************************************\
 * 
 *		dbe_cprec_getstartrec
 * 
 * Gets startrec pointer from cp record
 * 
 * Parameters : 
 * 
 *	cprec - in, use
 *		pointer to cp record
 *		
 * Return value - ref : 
 *      pointer to start record
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_startrec_t* dbe_cprec_getstartrec(
        dbe_cprec_t* cprec)
{
        ss_dassert(cprec != NULL);
        return (&cprec->cp_startrec);
}

#ifdef NOT_USED

/*##**********************************************************************\
 * 
 *		dbe_cprec_setstartrec
 * 
 * Sets the startrec to cp record
 * 
 * Parameters : 
 * 
 *	cprec - out, use
 *		pointer to cp record
 *		
 *	startrec - in, use
 *		pointer to start record
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_cprec_setstartrec(
        dbe_cprec_t* cprec,
        dbe_startrec_t* startrec)
{
        ss_dassert(cprec != NULL);
        ss_dassert(startrec != NULL);
        memcpy(&cprec->cp_startrec, startrec, sizeof(dbe_startrec_t));
}

#endif /* NOT_USED */

/*##**********************************************************************\
 * 
 *		dbe_cpmgr_deldeadcheckpoints
 * 
 * Redeletes dead checkpoints at revert to checkpoint operation
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_cpmgr_deldeadcheckpoints(
        dbe_cpmgr_t* cpmgr)
{
        dbe_cplist_t* old_cplist;
        su_daddr_t old_cpliststart;
        dbe_cpnum_t new_cpnum;  /* cpnum from new cplist */
        dbe_cpnum_t old_cpnum;  /* cpnum from old cplist */
        dbe_cpnum_t tmp_cpnum;
        dbe_filedes_t* filedes;

        filedes = cpmgr->cpm_dbfile->f_indexfile;
        ss_dassert(filedes->fd_cprec != NULL);
        old_cpliststart = filedes->fd_cprec->cp_startrec.sr_cplistaddr;
        old_cplist =
            dbe_cpl_init(
                filedes->fd_svfil,
                filedes->fd_cache,
                filedes->fd_freelist,
                filedes->fd_chlist,
                old_cpliststart);
        old_cpnum = dbe_cpl_last(old_cplist);
        new_cpnum = dbe_cpl_last(filedes->fd_cplist);
        while (new_cpnum > old_cpnum) {
            tmp_cpnum = new_cpnum;
            new_cpnum = dbe_cpl_prevfrom(filedes->fd_cplist, new_cpnum);
            dbe_cpl_remove(filedes->fd_cplist, tmp_cpnum);
        }
        ss_dassert(new_cpnum == old_cpnum);
        while (old_cpnum != 0L) {
            if (new_cpnum < old_cpnum) {
                tmp_cpnum = old_cpnum;
                old_cpnum = dbe_cpl_prevfrom(old_cplist, old_cpnum);
                dbe_cpmgr_deletecp(cpmgr, tmp_cpnum);
            } else if (old_cpnum == new_cpnum) {
                new_cpnum = dbe_cpl_prevfrom(filedes->fd_cplist, new_cpnum);
                old_cpnum = dbe_cpl_prevfrom(old_cplist, old_cpnum);
            } else {
                ss_error;
            }
        }
        dbe_cpl_done(old_cplist);
}

/*#***********************************************************************\
 * 
 *		cpmgr_prepare_file
 * 
 * Does the preparation steps for one database file (indexfile or blobfile).
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager object
 *		
 *	filedes - in out, use
 *		pointer to database file descriptor
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_ret_t cpmgr_prepare_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes)
{
        su_ret_t rc;
        dbe_info_t info;
        
        ss_dassert(cpmgr != NULL);
        ss_dassert(filedes != NULL);
        ss_dassert(filedes->fd_cplist != NULL);
        ss_dassert(filedes->fd_freelist != NULL);
        dbe_info_init(info, 0);
        if (filedes->fd_cprec == NULL) {
            filedes->fd_cprec = SSMEM_NEW(dbe_cprec_t);
            memset(filedes->fd_cprec, 0, sizeof(dbe_cprec_t));
        }
        rc = dbe_fl_alloc(filedes->fd_freelist, &filedes->fd_cprec->cp_daddr, &info);
        if (rc != SU_SUCCESS) {
            return(rc);
        }
        ss_dassert(!dbe_cpl_isalive(filedes->fd_cplist, cpmgr->cpm_cpnum));
        ss_dprintf_2(("cpmgr_prepare_file(): cp rec cpnum=%ld daddr=%ld\n",
                      cpmgr->cpm_cpnum,
                      filedes->fd_cprec->cp_daddr));
        dbe_cpl_add(
            filedes->fd_cplist,
            cpmgr->cpm_cpnum,
            filedes->fd_cprec->cp_daddr);
        su_rc_assert(rc == SU_SUCCESS, rc);
        filedes->fd_cprec->cp_blktype = cpmgr->cpm_type;
        filedes->fd_cprec->cp_cpnum = cpmgr->cpm_cpnum;
        filedes->fd_cprec->cp_time = cpmgr->cpm_time;
        filedes->fd_cprec->cp_oldcplistaddr = SU_DADDR_NULL;
        filedes->fd_cprec->cp_startrec.sr_cpnum = cpmgr->cpm_cpnum;
        filedes->fd_cprec->cp_startrec.sr_bonsairoot = SU_DADDR_NULL;
        filedes->fd_cprec->cp_startrec.sr_permroot = SU_DADDR_NULL;
        filedes->fd_cprec->cp_startrec.sr_mmiroot = SU_DADDR_NULL;
        filedes->fd_cprec->cp_startrec.sr_freelistaddr = SU_DADDR_NULL;
        filedes->fd_cprec->cp_startrec.sr_chlistaddr = SU_DADDR_NULL;
        filedes->fd_cprec->cp_startrec.sr_cplistaddr = SU_DADDR_NULL;
        filedes->fd_cprec->cp_startrec.sr_trxlistaddr = SU_DADDR_NULL;
        filedes->fd_cprec->cp_startrec.sr_filesize = 0L;
        rc = dbe_cpl_save(
                filedes->fd_cplist,
                cpmgr->cpm_cpnum,
                &filedes->fd_cprec->cp_startrec.sr_cplistaddr);
        ss_dprintf_1(("dbe_cpl_save() at %s %d daddr = %ld cpnum=%ld\n",
                      __FILE__, __LINE__,
                      filedes->fd_cprec->cp_startrec.sr_cplistaddr,
                      cpmgr->cpm_cpnum));
        su_rc_assert(rc == SU_SUCCESS, rc);
        filedes->fd_cprec->cp_oldcplistaddr =
            dbe_header_getcplistaddr(filedes->fd_dbheader);
        return(SU_SUCCESS);
}

/*#***********************************************************************\
 * 
 *		cpmgr_inheritchlist_file
 * 
 * Inherits changelist of one file
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to cp manager
 *		
 *	filedes - in out, use
 *		pointer to file descriptor
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void cpmgr_inheritchlist_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes)
{
        dbe_chlist_t* chlist;           /* temp. change list */
        dbe_chlist_t* chlist_tail;      /* this too */
        dbe_chlist_iter_t* chl_iter;    /* temp. change list iterator */
        su_ret_t rc;                    /* su_ret_t return code */
        dbe_cpnum_t prev_cpnum;
        ulong nblocks_sum;
        su_daddr_t daddr;               /* changed block address */
        dbe_cpnum_t cpnum;              /*  - " -        checkpoint # */
        dbe_cpnum_t block_cpnum;        /* changelist block checkpoint # */
        dbe_cpnum_t old_block_cpnum;    /* -"- of previous block */
        su_daddr_t block_daddr;         /* changelist block address */
        uint block_nblocks;             /* # of change recs in chlist block */


        prev_cpnum = dbe_cpl_prevfrom(filedes->fd_cplist, cpmgr->cpm_cpnum);
        chlist = dbe_cl_init(
                    filedes->fd_svfil,
                    filedes->fd_cache,
                    filedes->fd_freelist,
                    cpmgr->cpm_cpnum,   /* last created, not the global ctr */
                    filedes->fd_cprec->cp_startrec.sr_chlistaddr);
        chlist_tail =
            dbe_cl_init(
                filedes->fd_svfil,
                filedes->fd_cache,
                filedes->fd_freelist,
                1L,                     /* no meaning here */
                SU_DADDR_NULL);
        chl_iter = dbe_ci_init(chlist);
        nblocks_sum = 0L;
        old_block_cpnum = 0L;
        do {
            dbe_ci_getnodeinfo( /* get info about chlist block */
                chl_iter,
                &block_cpnum,
                &block_daddr,
                &block_nblocks);

            if (block_daddr != SU_DADDR_NULL) {
                ss_dprintf_2(("cpmgr_inheritchlist_file(): adding chlist block to new chlist: cpnum=%ld daddr=%ld\n",
                             block_cpnum, block_daddr));
                SS_PUSHNAME("cpmgr_inheritchlist_file:1");
                rc = dbe_cl_add(filedes->fd_chlist, block_cpnum, block_daddr);
                SS_POPNAME;
                su_rc_assert(rc == SU_SUCCESS, rc);
                if (block_cpnum != old_block_cpnum) {
                    if (nblocks_sum != 0L) {
                        ss_dassert(old_block_cpnum != 0L);
                        dbe_cl_linktogether(filedes->fd_chlist, chlist_tail);
                        nblocks_sum = 0L;
                    }
                    while (prev_cpnum >= block_cpnum) {
                        prev_cpnum =
                            dbe_cpl_prevfrom(filedes->fd_cplist, prev_cpnum);
                    }
                    old_block_cpnum = block_cpnum;
                    dbe_cl_setnextcpnum(chlist_tail, block_cpnum);
                }
                while (dbe_ci_getnext(chl_iter, &cpnum, &daddr)) {
                    ss_dassert(cpnum < block_cpnum);
                    if (cpnum <= prev_cpnum) {
                        ss_dprintf_2(("cpmgr_inheritchlist_file(): inheriting chrecord to new chlist: cpnum=%ld daddr=%ld\n",
                                    cpnum, daddr));

                        SS_PUSHNAME("cpmgr_inheritchlist_file:2");
                        rc = dbe_cl_add(chlist_tail, cpnum, daddr);
                        SS_POPNAME;
                        su_rc_assert(rc == SU_SUCCESS, rc);
                        nblocks_sum++;
                    }
                }
            }
        } while (dbe_ci_nextnode(chl_iter));
        if (nblocks_sum != 0) {
            dbe_cl_linktogether(filedes->fd_chlist, chlist_tail);
        }
        dbe_ci_done(chl_iter);
        dbe_cl_done(chlist);
        dbe_cl_done(chlist_tail);
        rc = dbe_cpl_deletefromdisk(
                filedes->fd_cplist,
                cpmgr->cpm_cpnum + 1,
                filedes->fd_cprec->cp_startrec.sr_cplistaddr);
        ss_dprintf_1(("dbe_cpl_deletefromdisk() at %s %d daddr=%ld\n",
                      __FILE__, __LINE__,
                      filedes->fd_cprec->cp_startrec.sr_cplistaddr));
        su_rc_assert(rc == SU_SUCCESS, rc);
        if (filedes->fd_cprec->cp_oldcplistaddr != SU_DADDR_NULL) {
            rc = dbe_cpl_deletefromdisk(
                    filedes->fd_cplist,
                    cpmgr->cpm_cpnum + 1,
                    filedes->fd_cprec->cp_oldcplistaddr);
            ss_dprintf_1(("dbe_cpl_deletefromdisk() at %s %d daddr=%ld\n",
                        __FILE__, __LINE__,
                        filedes->fd_cprec->cp_oldcplistaddr));
            su_rc_assert(rc == SU_SUCCESS, rc);
        }
}

/*#***********************************************************************\
 * 
 *		cpmgr_putcprectochlist_file
 * 
 * Puts latest CP record address to newest change list
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to cp manager
 *		
 *	filedes - in out, use
 *		pointer to file descriptor
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void cpmgr_putcprectochlist_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes)
{
        su_ret_t rc;

        SS_PUSHNAME("cpmgr_putcprectochlist_file");
        rc = dbe_cl_add(
                filedes->fd_chlist,
                cpmgr->cpm_cpnum,
                filedes->fd_cprec->cp_daddr);
        su_rc_assert(rc == SU_SUCCESS, rc);
        SS_POPNAME;
}

/*#***********************************************************************\
 * 
 *		cpmgr_createcp_file
 * 
 * Does the checkpoint creation to one database file descriptor
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager object
 *		
 *	filedes - in out, use
 *		pointer to file descriptor
 *
 *      mtflush - in
 *          use multithreaded flushing
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void cpmgr_createcp_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes)
{
        su_ret_t rc;

        /* allocate 1 extra block from freelist */
        rc = dbe_cl_preparetosave(filedes->fd_chlist);
        su_rc_assert(rc == SU_SUCCESS, rc);
        rc = dbe_fl_save(   /* save freelist */
                filedes->fd_freelist,
                cpmgr->cpm_cpnum + 1,
                &filedes->fd_cprec->cp_startrec.sr_freelistaddr,
                &filedes->fd_cprec->cp_startrec.sr_filesize);
        su_rc_assert(rc == SU_SUCCESS, rc);
        rc = dbe_cl_save(   /* save change list */
                filedes->fd_chlist,
                cpmgr->cpm_cpnum + 1,
                &filedes->fd_cprec->cp_startrec.sr_chlistaddr);
        su_rc_assert(rc == SU_SUCCESS, rc);
        cprec_save(filedes, NULL);  /* save checkpoint record */
#if !defined(DBE_MTFLUSH)
        dbe_cache_concurrent_flushinit(filedes->fd_cache);
#endif /* !DBE_MTFLUSH */
}

/*#***********************************************************************\
 * 
 *		cpmgr_updateheader_file
 * 
 * Updates header record of one file by copying the startrec from cprecord
 * to startrec of header. Also the old version of header is saved.
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager
 *		
 *	filedes - in out, use
 *		pointer to file descriptor
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void cpmgr_updateheader_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes)
{
        su_ret_t rc;
        su_daddr_t cplistaddr;

        if (filedes->fd_olddbheader != NULL) {
            SsMemFree(filedes->fd_olddbheader);
            filedes->fd_olddbheader = NULL;
        }
        dbe_header_setstartrec(
            filedes->fd_dbheader,
            &filedes->fd_cprec->cp_startrec);
        rc = dbe_cpl_save(
                filedes->fd_cplist,
                cpmgr->cpm_cpnum + 1,
                &cplistaddr);
        ss_dprintf_1(("dbe_cpl_save() at %s %d daddr = %ld cpnum=%ld\n",
                      __FILE__, __LINE__,
                      cplistaddr, cpmgr->cpm_cpnum + 1));
        su_rc_assert(rc == SU_SUCCESS, rc);
        dbe_header_setcplistaddr(filedes->fd_dbheader, cplistaddr);
        dbe_header_setcpnum(filedes->fd_dbheader, cpmgr->cpm_cpnum + 1);
}


/*#***********************************************************************\
 * 
 *		cprec_save
 * 
 * Saves (=writes to disk) a checkpoint record
 * 
 * Parameters : 
 * 
 *	filedes - in, use
 *		pointer to file descriptor
 *
 *      cprec - in, use
 *          pointer to checkpoint record object or NULL
 *          to use filedes->fd_cprec;
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void cprec_save(dbe_filedes_t* filedes, dbe_cprec_t* cprec)
{
        dbe_cacheslot_t* cacheslot;
        char* dbuf;

        ss_dassert(filedes != NULL);
        if (cprec == NULL) {
            cprec = filedes->fd_cprec;
        }
        ss_dassert(cprec->cp_daddr != SU_DADDR_NULL);
        ss_dprintf_2(("cprec_save(): daddr = %ld\n", cprec->cp_daddr));
        cacheslot =
            dbe_cache_reach(
                filedes->fd_cache,
                cprec->cp_daddr,
                DBE_CACHE_WRITEONLY,
                DBE_INFO_CHECKPOINT,
                &dbuf,
                cpm_cprecwritectx);
        memset(dbuf, 0, filedes->fd_blocksize);
        DBE_BLOCK_SETTYPE(dbuf, &cprec->cp_blktype);
        DBE_BLOCK_SETCPNUM(dbuf, &cprec->cp_cpnum);
        dbuf += DBE_BLOCKCPNUMOFFSET + sizeof(cprec->cp_cpnum);
        SS_UINT4_STORETODISK(dbuf, cprec->cp_time);
        dbuf += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbuf, cprec->cp_oldcplistaddr);
        dbuf += sizeof(cprec->cp_oldcplistaddr);
        dbuf = dbe_srec_puttodisk(&cprec->cp_startrec, dbuf);

        dbe_cache_release(
            filedes->fd_cache,
            cacheslot,
            DBE_CACHE_DIRTY,
            cpm_cprecwritectx);
}

/*#***********************************************************************\
 * 
 *		cprec_restore
 * 
 * Restores (=reads from disk) a checkpoint record
 * 
 * Parameters : 
 * 
 *	filedes - in out, use
 *		pointer to file descriptor
 *
 *      cprec - out, use
 *          pointer to checkpoint record or NULL
 *          to use the filedes->fd_cprec
 *		
 *	daddr - in
 *		disk address where the record is located
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void cprec_restore(
        dbe_filedes_t* filedes,
        dbe_cprec_t* cprec,
        su_daddr_t daddr)
{
        dbe_cacheslot_t* cacheslot;
        char* dbuf;

        ss_dprintf_3(("cprec_restore\n"));
        ss_dassert(filedes != NULL);
        ss_dassert(daddr != SU_DADDR_NULL);
        if (cprec == NULL) {
            cprec = filedes->fd_cprec;
        }
        ss_dassert(cprec != NULL);
        cprec->cp_daddr = daddr;
        cacheslot =
            dbe_cache_reach(
                filedes->fd_cache,
                daddr,
                DBE_CACHE_READONLY,
                DBE_INFO_CHECKPOINT,
                &dbuf,
                cpm_cprecreadctx);
        DBE_BLOCK_GETTYPE(dbuf, &cprec->cp_blktype);
        DBE_BLOCK_GETCPNUM(dbuf, &cprec->cp_cpnum);
        dbuf += DBE_BLOCKCPNUMOFFSET + sizeof(cprec->cp_cpnum);
        cprec->cp_time = SS_UINT4_LOADFROMDISK(dbuf);
        dbuf += sizeof(ss_uint4_t);
        cprec->cp_oldcplistaddr = SS_UINT4_LOADFROMDISK(dbuf);
        dbuf += sizeof(cprec->cp_oldcplistaddr);

        dbuf = dbe_srec_getfromdisk(&cprec->cp_startrec, dbuf);

        dbe_cache_release(
            filedes->fd_cache,
            cacheslot,
            DBE_CACHE_CLEAN,
            cpm_cprecreadctx);
}



/*#***********************************************************************\
 * 
 *		cpmgr_deletecpstep1_file
 * 
 * Step 1 of checkpoint/snapshot deletion for one random access
 * database file (index or BLOB file). Step 1 consists of:
 *      1.1 Saving old version of db header in memory
 *      1.2 Removing the checkpoint from the checkpoint list and writing
 *          the new version of the list to disk.
 *      1.3 Modifying the db header to contain the new saved cp list addr
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to cp manager object
 *		
 *	filedes - in out, use
 *		pointer to file descriptor object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void cpmgr_deletecpstep1_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes)
{
        su_ret_t rc;
        su_daddr_t cplist_daddr;

        /* 1.1 */
        if (filedes->fd_olddbheader != NULL) {
            dbe_header_done(filedes->fd_olddbheader);
            filedes->fd_olddbheader = NULL;
        }

        /* 1.2 */
        if (dbe_cpl_isalive(filedes->fd_cplist, cpmgr->cpm_delcpnum)) {
            filedes->fd_olddbheader =
                dbe_header_makecopyof(filedes->fd_dbheader);
            dbe_cpl_remove(filedes->fd_cplist, cpmgr->cpm_delcpnum);
            DBE_CPMGR_CRASHPOINT(24);
            cplist_daddr = dbe_header_getcplistaddr(filedes->fd_dbheader);
            rc = dbe_cpl_save(
                    filedes->fd_cplist,
                    cpmgr->cpm_cpnum + 1,
                    &cplist_daddr);
            ss_dprintf_1(("dbe_cpl_save() at %s %d daddr = %ld cpnum=%ld\n",
                        __FILE__, __LINE__,
                        cplist_daddr, cpmgr->cpm_cpnum + 1));
            su_rc_assert(rc == SU_SUCCESS, rc);
            /* 1.3 */
            dbe_header_setcplistaddr(filedes->fd_dbheader, cplist_daddr);
        }
        DBE_CPMGR_CRASHPOINT(25);

}


/*#***********************************************************************\
 * 
 *		cpmgr_deletecpstep2_file
 * 
 * Step 2 of checkpoint/snapshot deletion for one random access
 * database file (index or BLOB file). Step 3 consists of:
 *      2.1 Deletion of old version of cp list from disk
 *      2.2 Deletion of old version of db header
 *      2.3 Reading the cp record of the cp with next bigger number to
 *          the number of the cp to be deleted.
 *      2.4 Releasing the blocks that die upon deletion of the checkpoint
 *          by iterating the change list of the checkpoint.
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager object
 *		
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void cpmgr_deletecpstep2_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes)
{
        dbe_chlist_t* chlist;           /* temp. change list */
        dbe_chlist_iter_t* chl_iter;    /* temp. change list iterator */
        dbe_cprec_t* cprec;             /* temp. checkpoint record */
        su_daddr_t cprec_daddr;         /* disk addr of cprec */
        su_ret_t rc;                    /* su_ret_t return code */
        su_daddr_t oldcplist_daddr;     /* */

        /* 2.1 */
        if (filedes->fd_olddbheader != NULL) {
            oldcplist_daddr = dbe_header_getcplistaddr(filedes->fd_olddbheader);
            if (oldcplist_daddr != SU_DADDR_NULL) {
                /* Delete old version of checkpoint list */
                rc = dbe_cpl_deletefromdisk(
                        filedes->fd_cplist,
                        cpmgr->cpm_cpnum + 1,
                        oldcplist_daddr);
                ss_dprintf_1(("dbe_cpl_deletefromdisk() at %s %d daddr=%ld\n",
                            __FILE__, __LINE__,
                            oldcplist_daddr));
                su_rc_assert(rc == SU_SUCCESS, rc);
                DBE_CPMGR_CRASHPOINT(26);
            }

            /* 2.2 */
            dbe_header_done(filedes->fd_olddbheader);
            filedes->fd_olddbheader = NULL;
        }

        /* 2.3 */
        cprec = SSMEM_NEW(dbe_cprec_t);
        memset(cprec, 0, sizeof(dbe_cprec_t));
        ss_dassert(dbe_cpl_isalive(filedes->fd_cplist, cpmgr->cpm_nextcpnum));
        cprec_daddr =
            dbe_cpl_getdaddr(filedes->fd_cplist, cpmgr->cpm_nextcpnum);
        cprec_restore(filedes, cprec, cprec_daddr);
        DBE_CPMGR_CRASHPOINT(27);

        /* 2.4 */
        chlist = dbe_cl_init(
                    filedes->fd_svfil,
                    filedes->fd_cache,
                    filedes->fd_freelist,
                    cpmgr->cpm_cpnum,
                    cprec->cp_startrec.sr_chlistaddr);
        SsMemFree(cprec);
        chl_iter = dbe_ci_init(chlist);
        do {
            su_daddr_t daddr;           /* changed block address */
            dbe_cpnum_t cpnum;          /*  - " -        checkpoint # */
            dbe_cpnum_t block_cpnum;    /* changelist block checkpoint # */
            su_daddr_t block_daddr;     /* changelist block address */
            uint block_nblocks;         /* # of change recs in chlist block */

            dbe_ci_getnodeinfo( /* get info about chlist block */
                chl_iter,
                &block_cpnum,
                &block_daddr,
                &block_nblocks);
            
            /* we don't need to examine change list blocks that are
            ** inherited from earlier checkpoints than the one to be
            ** deleted
            */
            if (block_cpnum <= cpmgr->cpm_delcpnum) {
                break;
            }
            while (dbe_ci_getnext(chl_iter, &cpnum, &daddr)) {
                /* The freeing condition is:
                ** The removal of the checkpoint made the block
                ** dead, ie. it was seen by only this checkpoint
                */
                if (cpnum > cpmgr->cpm_prevcpnum
                &&  cpnum <= cpmgr->cpm_delcpnum)
                {
                    rc = dbe_fl_free(filedes->fd_freelist, daddr);
                    su_rc_assert(rc == SU_SUCCESS, rc);
                    DBE_CPMGR_CRASHPOINT(28);
                }
            }
        } while (dbe_ci_nextnode(chl_iter));
        dbe_ci_done(chl_iter);
        dbe_cl_done(chlist);
}



/*#***********************************************************************\
 * 
 *		cpmgr_restorestartrec_file
 * 
 * Restores startrecord from 1 database file.
 * 
 * Parameters : 
 * 
 *	cpmgr - in out, use
 *		pointer to checkpoint manager
 *		
 *	filedes - in out, use
 *		pointer to filedes object
 *		
 * Return value :
 *      DBE_RC_SUCC if succesful or
 *      DBE_ERR_NOCHECKPOINT if no checkpoint record is available
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_ret_t cpmgr_restorestartrec_file(
        dbe_cpmgr_t* cpmgr,
        dbe_filedes_t* filedes)
{
        dbe_ret_t rc;
        su_daddr_t cprec_daddr = 0;
        su_daddr_t cplist_daddr;
        dbe_cpnum_t cpnum;
        dbe_cplist_t* tmp_cplist;

        rc = DBE_RC_SUCC;
        if (filedes->fd_cplist == NULL) {
            cplist_daddr = dbe_header_getcplistaddr(filedes->fd_dbheader);
            tmp_cplist =
                dbe_cpl_init(
                    filedes->fd_svfil,
                    filedes->fd_cache,
                    NULL,
                    NULL,
                    cplist_daddr);
        } else {
            tmp_cplist = filedes->fd_cplist;
        }
        cpnum = dbe_cpl_last(tmp_cplist);
        if (cpnum == 0L) {
            rc = DBE_ERR_NOCHECKPOINT;
        } else {
            cprec_daddr = dbe_cpl_getdaddr(tmp_cplist, cpnum);
        }
        if (tmp_cplist != filedes->fd_cplist) {
            dbe_cpl_done(tmp_cplist);
        }
        if (rc == DBE_RC_SUCC) {
            ss_dprintf_2((
                "cpmgr_restorestartrec_file() cpnum: %ld\n",
                cpnum));
            cpmgr->cpm_cpnum = cpnum;
            ss_dassert(cprec_daddr != SU_DADDR_NULL);
            if (filedes->fd_cprec == NULL) {
                filedes->fd_cprec = SSMEM_NEW(dbe_cprec_t);
                memset(filedes->fd_cprec, 0, sizeof(dbe_cprec_t));
            }
            cprec_restore(filedes, NULL, cprec_daddr);
            if (filedes->fd_olddbheader != NULL) {
                dbe_header_done(filedes->fd_olddbheader);
            }
            filedes->fd_olddbheader =
                dbe_header_makecopyof(filedes->fd_dbheader);
            dbe_header_setstartrec(
                filedes->fd_dbheader,
                dbe_cprec_getstartrec(filedes->fd_cprec));
            dbe_header_setchlistaddr(filedes->fd_dbheader, SU_DADDR_NULL);
            dbe_header_setcplistaddr(
                filedes->fd_dbheader,
                dbe_header_getcplistaddr(filedes->fd_olddbheader));
            dbe_header_setcpnum(
                filedes->fd_dbheader,
                cpnum + 1);
            dbe_header_done(filedes->fd_olddbheader);
            filedes->fd_olddbheader = NULL;
        }
        return (rc);
}

#ifdef NOT_USED

void dbe_cpmgr_getsrec(char* pagedata, dbe_startrec_t* startrec)
{
        char* startrecpos =
            pagedata +
            DBE_BLOCKCPNUMOFFSET +
            sizeof(dbe_cpnum_t) +
            sizeof(ss_uint4_t) + /* cp time */
            sizeof(su_daddr_t);
        dbe_srec_getfromdisk(startrec, startrecpos);
}

#endif /* NOT_USED */

#ifdef SS_MME

void dbe_cpmgr_setfirstmmeaddr(
        dbe_cpmgr_t* cpmgr,
        su_daddr_t firstmmepageaddr)
{
        dbe_filedes_t* filedes;

        ss_dassert(cpmgr != NULL);
        filedes = cpmgr->cpm_dbfile->f_indexfile;
        ss_dassert(filedes != NULL);
        filedes->fd_cprec->cp_startrec.sr_firstmmeaddrpage = firstmmepageaddr;
}

#endif
