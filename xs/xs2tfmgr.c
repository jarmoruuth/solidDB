/*************************************************************************\
**  source       * xs2tfmgr.c
**  directory    * xs
**  description  * Temporary File ManaGeR for eXternal Sort
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
#include <sssem.h>
#include <ssfile.h>
#include <ssfnsplt.h>
#include <ssltoa.h>
#include <ssstring.h>

#include <su0types.h>
#include <su0bmap.h>
#include <su0parr.h>
#include <su0vmem.h>
#include <su0error.h>

#include "xs2tfmgr.h"

typedef struct xs_tfdir_st xs_tfdir_t;

static char xs_tfnprefix[] = "sxs";

struct xs_tfdir_st {
        char*   tfd_name;
        ulong   tfd_maxblocks;
        ulong   tfd_blocksinuse;
        ulong   tfd_blocksreserved;
        SsSemT* tfd_mutex;
};

struct xs_tfmgr_st {
        size_t      tfm_blocksize;
        int         tfm_openflags;
        su_bmap_t*  tfm_bitmap;/* [tfm_maxfiles] */
        ulong       tfm_maxfiles;
        su_pa_t*    tfm_tfdirs;
        uint        tfm_ntfdirs;
        xs_mem_t*   tfm_memmgr;
        ulong       tfm_dbid;
        SsSemT*     tfm_mutex;
};

typedef struct {
        su_daddr_t tfp_block;
        size_t     tfp_offset;
        char*      tfp_buf;
} xs_tfpos_t;

typedef struct {
        su_daddr_t tfs_block;
        size_t     tfs_offset;
} xs_tfsiz_t;

struct xs_tf_st {
        su_vmem_t*      tf_file;
        su_pa_t*        tf_fnumarr;
        char*           tf_fname;
        xs_tfmgr_t*     tf_tfmgr;
        xs_mem_t*       tf_memmgr;
        xs_tfdir_t*     tf_tfdir;
        xs_tfstate_t    tf_state;
        su_pa_t*        tf_hmemarr;
        size_t          tf_blocksize;
        xs_tfpos_t      tf_pos;
        xs_tfsiz_t      tf_size;
        char*           tf_editbuf;
};

/*#***********************************************************************\
 * 
 *		tfmgr_clearfilebit
 * 
 * Clears a file allocation bit from tfmgr bitmap
 * 
 * Parameters : 
 * 
 *	tfmgr - in out, use
 *		tf manager
 *		
 *	fnum - in
 *		file number
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void tfmgr_clearfilebit(xs_tfmgr_t* tfmgr, ulong fnum)
{
        ss_dassert(tfmgr != NULL);

        SsSemEnter(tfmgr->tfm_mutex);
        ss_dassert(tfmgr->tfm_maxfiles > fnum);
        ss_dassert(SU_BMAP_GET(tfmgr->tfm_bitmap, fnum));
        SU_BMAP_SET(tfmgr->tfm_bitmap, fnum, FALSE);
        SsSemExit(tfmgr->tfm_mutex);
}

static ulong tfmgr_getdbid(xs_tfmgr_t* tfmgr)
{
        ss_dassert(tfmgr != NULL);
        
        return (tfmgr->tfm_dbid);
}
/*#***********************************************************************\
 * 
 *		tfdir_init
 * 
 * Creates a Temporary File DIRectory object
 * 
 * Parameters : 
 * 
 *	dirname - in, use
 *		directory name, "" represents CWD
 *		
 *	maxblocks - in
 *		max. # of file blocks to store into this directory
 *		
 *	blocksize - in
 *		size of a file block
 *		
 * Return value - give :
 *      created tfdir object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static xs_tfdir_t* tfdir_init(
        char* dirname,
        ulong maxblocks,
        size_t blocksize __attribute__ ((unused)))
{
        xs_tfdir_t* tfdir;

        ss_dassert(dirname != NULL);
        tfdir = SSMEM_NEW(xs_tfdir_t);
        tfdir->tfd_name = SsMemStrdup(dirname);
        tfdir->tfd_maxblocks = maxblocks;
        tfdir->tfd_blocksinuse = 0;
        tfdir->tfd_blocksreserved = 0;
        tfdir->tfd_mutex = SsSemCreateLocal(SS_SEMNUM_XS_TFDIR);
        return (tfdir);
}

/*#***********************************************************************\
 * 
 *		tfdir_done
 * 
 * Deletes a tfdir
 * 
 * Parameters : 
 * 
 *	tfdir - in, take
 *		pointer to tfdir object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void tfdir_done(xs_tfdir_t* tfdir)
{
        ss_dassert(tfdir != NULL);
        ss_dassert(tfdir->tfd_name != NULL);
        ss_dassert(tfdir->tfd_blocksinuse == 0);
        ss_dassert(tfdir->tfd_blocksreserved == 0);

        SsMemFree(tfdir->tfd_name);
        SsSemFree(tfdir->tfd_mutex);
        SsMemFree(tfdir);
}

#ifndef SS_MYSQL
/*#***********************************************************************\
 * 
 *		tfdir_blocksavail
 * 
 * Tells how many file blocks is available in this directory
 * 
 * Parameters : 
 * 
 *	tfdir - in, use
 *		tfdir object
 *		
 * Return value :
 *      # of file blocks available to be reserved or allocated
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static ulong tfdir_blocksavail(xs_tfdir_t* tfdir)
{
        ulong blocksavail;

        ss_dassert(tfdir != NULL);
        SsSemEnter(tfdir->tfd_mutex);
        blocksavail = tfdir->tfd_maxblocks -
                      tfdir->tfd_blocksinuse -
                      tfdir->tfd_blocksreserved;
        SsSemExit(tfdir->tfd_mutex);
        return (blocksavail);
}

/*#***********************************************************************\
 * 
 *		tfdir_reserveblocks
 * 
 * Reserves a bunch of blocks to be later allocated
 * 
 * Parameters : 
 * 
 *	tfdir - in out, use
 *		tfdir object
 *		
 *	n - in
 *		# of blocks to reserve
 *		
 * Return value :
 *      TRUE when successful
 *      FALSE when not enough blocks are available
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool tfdir_reserveblocks(xs_tfdir_t* tfdir, ulong n)
{
        bool succp;

        ss_dassert(tfdir != NULL);
        SsSemEnter(tfdir->tfd_mutex);
        if (n >
            tfdir->tfd_maxblocks -
            tfdir->tfd_blocksinuse -
            tfdir->tfd_blocksreserved)
        {
            succp = FALSE;   
        } else {
            tfdir->tfd_blocksreserved += n;
            succp = TRUE;
        }
        SsSemExit(tfdir->tfd_mutex);
        return (succp);
}

/*#***********************************************************************\
 * 
 *		tfdir_unreserveblocks
 * 
 * Cancels reservation for a bunch of reserved blocks
 * 
 * Parameters : 
 * 
 *	tfdir - in out, use
 *		tfdir object
 *		
 *	n - in
 *		# of blocks to unreserve
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void tfdir_unreserveblocks(xs_tfdir_t* tfdir, ulong n)
{
        ss_dassert(tfdir != NULL);
        SsSemEnter(tfdir->tfd_mutex);
        ss_dassert(tfdir->tfd_blocksreserved >= n);
        tfdir->tfd_blocksreserved += n;
        SsSemExit(tfdir->tfd_mutex);
}
#endif /* !SS_MYSQL */

/*#***********************************************************************\
 * 
 *		tfdir_takeblockstouse
 * 
 * Takes a bunch of blocks to use. They are either reserved before or
 * not
 * 
 * Parameters : 
 * 
 *	tfdir - in out, use
 *		tfdir object
 *		
 *	n - in
 *		# of blocks to take into use
 *		
 *	reserved - in
 *		TRUE if prereserved - FALSE otherwise
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool tfdir_takeblockstouse(xs_tfdir_t* tfdir, ulong n, bool reserved)
{
        bool succp = TRUE;

        ss_dassert(tfdir != NULL);
        SsSemEnter(tfdir->tfd_mutex);
        if (reserved) {
            if (n > tfdir->tfd_blocksreserved) {
                succp = FALSE;
            } else {
                tfdir->tfd_blocksreserved -= n;
                tfdir->tfd_blocksinuse += n;
            }
        } else {
            if (n >
                tfdir->tfd_maxblocks -
                tfdir->tfd_blocksinuse -
                tfdir->tfd_blocksreserved)
            {
                succp = FALSE;
            } else {
                tfdir->tfd_blocksinuse += n;
            }
        }
        SsSemExit(tfdir->tfd_mutex);
        return (succp);
}

/*#***********************************************************************\
 * 
 *		tfdir_releaseblocks
 * 
 * Releases a bunch of block from use
 * 
 * Parameters : 
 * 
 *	tfdir - in out, use
 *		tfdir object
 *		
 *	n - in
 *		# of blocks to release
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void tfdir_releaseblocks(xs_tfdir_t* tfdir, ulong n)
{
        ss_dassert(tfdir != NULL);
        SsSemEnter(tfdir->tfd_mutex);
        ss_dassert(tfdir->tfd_blocksinuse >= n);
        tfdir->tfd_blocksinuse -= n;
        SsSemExit(tfdir->tfd_mutex);
}

/*#***********************************************************************\
 * 
 *		tfdir_getname
 * 
 * Gets directory name
 * 
 * Parameters : 
 * 
 *	tfdir - in, use
 *		tfdir object
 *		
 * Return value - ref :
 *      tfdir name as asciiz string
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static char* tfdir_getname(xs_tfdir_t* tfdir)
{
        ss_dassert(tfdir != NULL);
        ss_dassert(tfdir->tfd_name != NULL);
        return (tfdir->tfd_name);
}

/*#***********************************************************************\
 * 
 *		tf_createfname
 * 
 * Creates a file name from given dir name and file number
 * 
 * Parameters : 
 * 
 *	dir - in, use
 *		directory name
 *		
 *	fnum - in
 *		file #
 *
 *      dbid - in
 *          database id
 *		
 * Return value - give :
 *      newly allocated file name, must be freed using SsMemFree()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static char* tf_createfname(char* dir, ulong fnum, ulong dbid)
{
        bool succp;
        char fnbuf[13];
        char* p;
        char* pathbuf;
        size_t pathbufsize;
        size_t n;

        /* put predix */
        memcpy(fnbuf, xs_tfnprefix, sizeof(xs_tfnprefix) - 1);
        n = sizeof(xs_tfnprefix) - 1;
        p = fnbuf + n;
        n += SsLongToAscii((long)dbid, p, 36, 5, '0', FALSE);
        ss_dassert(n == 5 + 3);
        p = fnbuf + n;
        *p++ = '.';
        n++;
        n += SsLongToAscii((long)fnum, p, 36, 3, '0', FALSE);
        ss_dassert(n == 12);
        ss_dassert(strlen(fnbuf) == n);

        pathbufsize = strlen(dir) + n + 2;
        pathbuf = SsMemAlloc(pathbufsize);
        succp = SsFnMakePath(dir, fnbuf, pathbuf, pathbufsize);
        ss_assert(succp);
        return (pathbuf);
}

static long tf_allocatefileidx(xs_tfmgr_t* tfmgr)
{
        long f_idx;

        f_idx = su_bmap_find1st(tfmgr->tfm_bitmap, tfmgr->tfm_maxfiles, FALSE);
        if (f_idx != -1L) {
            SU_BMAP_SET(tfmgr->tfm_bitmap, (ulong)f_idx, TRUE);
        }
        return (f_idx);
}

static char* tf_givenewfname_callback(void* ctx, uint num)
{
        xs_tf_t* tf = ctx;
        xs_tfmgr_t* tfmgr = tf->tf_tfmgr;
        char* fname = NULL;
        long f_idx;

        SsSemEnter(tfmgr->tfm_mutex);
        f_idx = tf_allocatefileidx(tfmgr);
        if (f_idx != -1L) {
            ss_dassert(f_idx >= 0L);
            fname = tf_createfname(tfdir_getname(tf->tf_tfdir),
                                   f_idx,
                                   tfmgr_getdbid(tfmgr));
            ss_dassert(fname != NULL);
            ss_dassert(num >= 1);
            ss_dassert(!su_pa_indexinuse(tf->tf_fnumarr, num));
            ss_rc_dassert(su_pa_nelems(tf->tf_fnumarr) == num, num);
            su_pa_insertat(tf->tf_fnumarr, num, (void*)((ulong)f_idx + 1UL));
        }
        SsSemExit(tfmgr->tfm_mutex);
        return (fname);
}

static void tf_releasefname_callback(void* ctx, uint num, char* fname)
{
        xs_tf_t* tf = ctx;
        xs_tfmgr_t* tfmgr = tf->tf_tfmgr;
        ulong f_idx;

        ss_dassert(su_pa_indexinuse(tf->tf_fnumarr, num));
        f_idx = ((ulong)su_pa_remove(tf->tf_fnumarr, num)) - 1UL;
        tfmgr_clearfilebit(tfmgr, f_idx);
        SsMemFree(fname);
}

/*#***********************************************************************\
 * 
 *		tf_init
 * 
 * Creates a temporary file object
 * 
 * Parameters : 
 * 
 *	tfmgr - in, hold
 *		tf manager
 *		
 *	tfdir - in, hold
 *		tf dir
 *		
 *	memmgr - in, hold
 *		buffer memory manager
 *		
 *	fnum - in
 *		file number
 *		
 *	blocksize - in
 *		file (and buffer memory) block size
 *		
 * Return value - give :
 *      created temporary file object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static xs_tf_t* tf_init(
        xs_tfmgr_t* tfmgr,
        xs_tfdir_t* tfdir,
        xs_mem_t* memmgr,
        ulong fnum,
        size_t blocksize)
{
        xs_tf_t* tf;
        char* dirname;
        xs_hmem_t* p1;
        void* p2;
        bool succp = TRUE;

        tf = SSMEM_NEW(xs_tf_t);
        tf->tf_fnumarr = su_pa_init();
        su_pa_insertat(tf->tf_fnumarr, 0, (void*)(fnum + 1UL));
        tf->tf_tfdir = tfdir;
        dirname = tfdir_getname(tf->tf_tfdir);
        tf->tf_tfmgr = tfmgr; 
        tf->tf_fname =
            tf_createfname(
                dirname,
                fnum,
                tfmgr_getdbid(tf->tf_tfmgr));
        tf->tf_memmgr = memmgr;
        tf->tf_state = XSTF_WRITE;
        tf->tf_hmemarr = su_pa_init();
        tf->tf_blocksize = blocksize;
        if (SsFExist(tf->tf_fname)) {
            ss_dprintf_1(("tf_init(): file %s already exists\n",
                tf->tf_fname));
            SsFRemove(tf->tf_fname);
        }
        tf->tf_pos.tfp_block = 0;
        tf->tf_pos.tfp_offset = 0;
        tf->tf_pos.tfp_buf = NULL;

        tf->tf_size.tfs_block = 0;
        tf->tf_size.tfs_offset = 0;

        tf->tf_editbuf = NULL;

        p1 = xs_mem_allocreserved(tf->tf_memmgr, &p2);
        ss_dassert(p1 != NULL);
        ss_dassert(p2 != NULL);

        su_pa_insertat(tf->tf_hmemarr, 0, p1);
        tf->tf_file = su_vmem_open(
                            tf->tf_fname,
                            1,
                            &p2,
                            tf->tf_blocksize,
                            tfmgr->tfm_openflags,
                            tf_givenewfname_callback,
                            tf_releasefname_callback,
                            tf);
        if (tf->tf_file == NULL) {
            xs_tf_done(tf);
            tf = NULL;
            succp = FALSE;
        }
        if (succp) {
            succp = xs_tf_close(tf);
            if (!succp) {
                xs_tf_done(tf);
                tf = NULL;
            }
        }
        return (tf);
}

/*#***********************************************************************\
 * 
 *		tf_releasefileblocks
 * 
 * Releases file blocks allocated by a temporary file object (tf)
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void tf_releasefileblocks(xs_tf_t* tf)
{
        ulong nblocks;

        ss_dassert(tf != NULL);
        if (tf->tf_size.tfs_block != 0L
        ||  tf->tf_size.tfs_offset != 0)
        {
            nblocks = tf->tf_size.tfs_block;
            if (tf->tf_size.tfs_offset != 0) {
                nblocks++;
            }
            tfdir_releaseblocks(tf->tf_tfdir, nblocks);
            tf->tf_size.tfs_block = 0L;
            tf->tf_size.tfs_offset = 0;
        }
}

/*##**********************************************************************\
 * 
 *		xs_tf_done
 * 
 * Deletes a temporary file object
 * 
 * Parameters : 
 * 
 *	tf - in, take
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
void xs_tf_done(xs_tf_t* tf)
{
        uint i;
        xs_hmem_t* hmem;
        bool succp;

        ss_dassert(tf != NULL);
        if (tf->tf_pos.tfp_buf != NULL) {
            su_vmem_release(
                tf->tf_file,
                tf->tf_pos.tfp_block,
                FALSE);
            tf->tf_pos.tfp_buf = NULL;
        }
        if (tf->tf_file != NULL) {
            su_vmem_delete(tf->tf_file);
        }
        tfmgr_clearfilebit(
                tf->tf_tfmgr,
                ((ulong)su_pa_getdata(tf->tf_fnumarr, 0)) - 1UL);
        tf_releasefileblocks(tf);
        ss_dassert(tf->tf_fname != NULL);
        SsMemFree(tf->tf_fname);
        ss_dassert(tf->tf_hmemarr != NULL);

        succp = xs_mem_reserveonfree(
                    tf->tf_memmgr,
                    su_pa_nelems(tf->tf_hmemarr));
        ss_dassert(succp);

        su_pa_do_get(tf->tf_hmemarr, i, hmem) {
            xs_mem_free(tf->tf_memmgr, hmem);
        }
        su_pa_done(tf->tf_hmemarr);
        su_pa_done(tf->tf_fnumarr);
        if (tf->tf_editbuf != NULL) {
            SsMemFree(tf->tf_editbuf);
        }
        SsMemFree(tf);
}

/*##**********************************************************************\
 * 
 *		xs_tf_rewrite
 * 
 * Truncates a temporary file object to zero length and reopens it in
 * write mode
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 * Return value :
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tf_rewrite(xs_tf_t* tf)
{
        bool succp = TRUE;

        ss_dassert(tf != NULL);

        if (tf->tf_state == XSTF_CLOSED) {
            succp = xs_tf_open(tf);
            if (!succp) {
                return (FALSE);
            }
        }
        if (tf->tf_pos.tfp_buf != NULL) {
            su_vmem_release(
                tf->tf_file,
                tf->tf_pos.tfp_block,
                FALSE);
            tf->tf_pos.tfp_buf = NULL;
        }
        su_vmem_rewrite(tf->tf_file);

        tf->tf_pos.tfp_block = 0;
        tf->tf_pos.tfp_offset = 0;
        tf->tf_pos.tfp_buf = NULL;

        tf_releasefileblocks(tf);
        if (tf->tf_editbuf != NULL) {
            SsMemFree(tf->tf_editbuf);
            tf->tf_editbuf = NULL;
        }
        tf->tf_state = XSTF_WRITE;
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		xs_tf_close
 * 
 * Closes a temporary file but keeps current status & position
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		temporary file that must be in write mode
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tf_close(xs_tf_t* tf)
{
        bool succp;
        xs_hmem_t* hmem;

        ss_dassert(tf != NULL);

        if (tf->tf_state == XSTF_CLOSED) {
            return (TRUE);
        }
        ss_dassert(tf->tf_state == XSTF_WRITE);
        if (tf->tf_pos.tfp_buf != NULL) {

            bool wrote = (tf->tf_size.tfs_offset != 0);

            su_vmem_release(
                tf->tf_file,
                tf->tf_pos.tfp_block,
                wrote);
            tf->tf_pos.tfp_buf = NULL;
        }
        tf->tf_state = XSTF_CLOSED;
        succp = xs_mem_reserveonfree(tf->tf_memmgr, 1);
        ss_dassert(succp);

        su_vmem_removebuffers(tf->tf_file);
        hmem = su_pa_remove(tf->tf_hmemarr, 0);
        ss_dassert(su_pa_nelems(tf->tf_hmemarr) == 0);
        xs_mem_free(tf->tf_memmgr, hmem);

        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		xs_tf_open
 * 
 * Opens a closed temporary file, if file is already open
 * just returns TRUE
 * 
 * Parameters : 
 * 
 *	tf - in out, use
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
bool xs_tf_open(xs_tf_t* tf)
{
        xs_hmem_t* hmem;
        void* p;

        ss_dassert(tf != NULL);

        if (tf->tf_state == XSTF_WRITE) {
            return (TRUE);
        }
        ss_dassert(tf->tf_state == XSTF_CLOSED);

        tf->tf_state = XSTF_WRITE;
        hmem = xs_mem_allocreserved(tf->tf_memmgr, &p);
        ss_dassert(hmem != NULL);

        su_vmem_addbuffers(tf->tf_file, 1, &p);
        su_pa_insertat(tf->tf_hmemarr, 0, hmem);
        ss_dassert(su_pa_nelems(tf->tf_hmemarr) == 1);

        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		xs_tf_rewind
 * 
 * Sets file pointer to beginning and reopens the file in read mode
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 * Return value : 
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tf_rewind(xs_tf_t* tf)
{
        bool succp = TRUE;

        if (tf->tf_state == XSTF_CLOSED) {
            succp = xs_tf_open(tf);
        }
        if (succp) {
            succp = xs_tf_movetobegin(tf);
            if (succp) {
                tf->tf_state = XSTF_READ;
            }
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_tf_peek
 * 
 * Peeks a piece of data from a temporary file
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 *	nbytes - in
 *		# of bytes to peek
 *		
 * Return value - ref :
 *      pointer to n bytes of data beginning from the current file pointer
 *      location or
 *      NULL when EOF is reached
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* xs_tf_peek(xs_tf_t* tf, size_t nbytes)
{
        su_daddr_t saved_block;
        size_t saved_offset;
        su_daddr_t last_accessed_block;
        size_t last_accessed_offset;
        uint byte_c;
        char* p;
        size_t ncopied;

        ss_dprintf_1(("xs_tf_peek:nbytes=%d, tfp_block=%d, tfp_offset=%d\n",
            nbytes, tf->tf_pos.tfp_block, tf->tf_pos.tfp_offset));
        ss_dassert(tf != NULL);
        ss_dassert(tf->tf_state == XSTF_READ || tf->tf_state == XSTF_CURSOR);
        last_accessed_block = tf->tf_pos.tfp_block +
            (tf->tf_pos.tfp_offset + nbytes - 1) / tf->tf_blocksize;
        last_accessed_offset =
            (tf->tf_pos.tfp_offset + nbytes - 1) % tf->tf_blocksize;
        if (tf->tf_size.tfs_block < last_accessed_block
        ||  (tf->tf_size.tfs_block == last_accessed_block &&
             tf->tf_size.tfs_offset <= last_accessed_offset))
        {
            /* Peek request goes beyond EOF */
            ss_dprintf_2(("xs_tf_peek:return NULL\n"));
            return (NULL);
        }
        if (tf->tf_editbuf != NULL) {
            SsMemFree(tf->tf_editbuf);
            tf->tf_editbuf = NULL;
        }
        if (tf->tf_pos.tfp_buf == NULL) {
            /* Reach a buffer */
            tf->tf_pos.tfp_buf =
                su_vmem_reach(
                    tf->tf_file,
                    tf->tf_pos.tfp_block,
                    &byte_c);
            ss_dassert(byte_c == tf->tf_blocksize);
        }
        ss_dassert(tf->tf_pos.tfp_buf != NULL);
        if (tf->tf_pos.tfp_buf == NULL) {
            ss_dprintf_2(("xs_tf_peek:return NULL\n"));
            return (NULL);
        }
        if (last_accessed_block == tf->tf_pos.tfp_block) {
            ss_dprintf_2(("xs_tf_peek:use last_accessed_block, return buf=%d, offset=%d\n",
                (int)(tf->tf_pos.tfp_buf + tf->tf_pos.tfp_offset),
                tf->tf_pos.tfp_offset));
            return (tf->tf_pos.tfp_buf + tf->tf_pos.tfp_offset);
        }
        saved_block  = tf->tf_pos.tfp_block;
        saved_offset = tf->tf_pos.tfp_offset;
        tf->tf_editbuf = SsMemAlloc(nbytes);
        ncopied = tf->tf_blocksize - tf->tf_pos.tfp_offset;
        ss_dassert(ncopied < nbytes);
        p = tf->tf_editbuf;
        ss_dprintf_2(("xs_tf_peek:memcpy(%d, %d, %d)\n",
            (int)p, (int)(tf->tf_pos.tfp_buf + tf->tf_pos.tfp_offset), ncopied));
        memcpy(
            p,
            tf->tf_pos.tfp_buf + tf->tf_pos.tfp_offset,
            ncopied);
        p += ncopied;
        nbytes -= ncopied;
        do {
            ss_dassert(nbytes > 0);
            su_vmem_release(
                tf->tf_file,
                tf->tf_pos.tfp_block,
                FALSE);
            tf->tf_pos.tfp_block++;
            tf->tf_pos.tfp_offset = 0;
            tf->tf_pos.tfp_buf =
                su_vmem_reach(
                    tf->tf_file,
                    tf->tf_pos.tfp_block,
                    &byte_c);
            ss_dassert(byte_c == tf->tf_blocksize);
            if (tf->tf_pos.tfp_buf == NULL) {
                ss_dprintf_2(("xs_tf_peek:return NULL\n"));
                return (NULL);
            }
            if (byte_c > nbytes) {
                ncopied = nbytes;
            } else {
                ncopied = byte_c;
            }
            ss_dprintf_2(("xs_tf_peek:memcpy(%d, %d, %d)\n",
                (int)p, (int)tf->tf_pos.tfp_buf, ncopied));
            memcpy(p, tf->tf_pos.tfp_buf, ncopied);
            p += ncopied;
            nbytes -= ncopied;
        } while (nbytes > 0);
        su_vmem_release(
            tf->tf_file,
            tf->tf_pos.tfp_block,
            FALSE);
        tf->tf_pos.tfp_buf = NULL;
        tf->tf_pos.tfp_block  = saved_block;
        tf->tf_pos.tfp_offset = saved_offset;
        ss_dprintf_2(("xs_tf_peek:return editbuf=%d\n",
            (int)tf->tf_editbuf));
        return (tf->tf_editbuf);
}

/*##**********************************************************************\
 * 
 *		xs_tf_peekextend
 * 
 * Extends size of previously peeked data
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		
 *		
 *	oldnbytes - in
 *		# of bytes at previous peek - must be correct !
 *		
 *	nbytes - in
 *		# of bytes to seek this time
 *		
 * Return value - ref :
 *      pointer to nbytes of data or NULL if peek request goes
 *      beyond EOF.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* xs_tf_peekextend(xs_tf_t* tf, size_t oldnbytes, size_t nbytes)
{
        su_daddr_t saved_block;
        size_t saved_offset;
        su_daddr_t last_accessed_block;
        size_t last_accessed_offset;
        uint byte_c;
        char* p;
        size_t ncopied;

        ss_dprintf_1(("xs_tf_peekextend:oldnbytes=%d, nbytes=%d, tfp_block=%d, tfp_offset=%d\n",
            oldnbytes, nbytes));
        ss_dassert(tf != NULL);
        ss_dassert(tf->tf_state == XSTF_READ || tf->tf_state == XSTF_CURSOR);
        ss_dassert(nbytes > 0);
        ss_dassert(oldnbytes > 0)

        if (tf->tf_editbuf == NULL) {
            ss_dprintf_2(("xs_tf_peekextend:tf->tf_editbuf == NULL, call xs_tf_peek\n"));
            p = xs_tf_peek(tf, nbytes);
            return (p);
        }
        if (oldnbytes >= nbytes) {
            ss_dprintf_2(("xs_tf_peekextend:return editbuf=%d, oldnbytes=%d >= nbytes=%d\n",
                (int)tf->tf_editbuf, oldnbytes, nbytes));
            return (tf->tf_editbuf);
        }
        ss_dprintf_2(("xs_tf_peekextend:tfp_block=%d, tfp_offset=%d\n",
            tf->tf_pos.tfp_block,  tf->tf_pos.tfp_offset));
        last_accessed_block = tf->tf_pos.tfp_block +
            (tf->tf_pos.tfp_offset + nbytes - 1) / tf->tf_blocksize;
        last_accessed_offset =
            (tf->tf_pos.tfp_offset + nbytes - 1) % tf->tf_blocksize;
        if (tf->tf_size.tfs_block < last_accessed_block
        ||  (tf->tf_size.tfs_block == last_accessed_block &&
             tf->tf_size.tfs_offset <= last_accessed_offset))
        {
            /* Peek request goes beyond EOF */
            ss_dprintf_2(("xs_tf_peekextend:EOF\n"));
            return (NULL);
        }

            
        tf->tf_editbuf = SsMemRealloc(tf->tf_editbuf, nbytes);
        saved_block  = tf->tf_pos.tfp_block;
        saved_offset = tf->tf_pos.tfp_offset;

        ss_dassert(last_accessed_block != saved_block);

        tf->tf_pos.tfp_offset += oldnbytes;
        tf->tf_pos.tfp_block += tf->tf_pos.tfp_offset / tf->tf_blocksize;
        tf->tf_pos.tfp_offset %= tf->tf_blocksize;

        ss_dassert(saved_block != tf->tf_pos.tfp_block);
        if (tf->tf_pos.tfp_buf != NULL) {
            su_vmem_release(
                tf->tf_file,
                saved_block,
                FALSE);
            tf->tf_pos.tfp_buf = NULL;
        }

        p = tf->tf_editbuf + oldnbytes;
        nbytes -= oldnbytes;

        while (nbytes > 0) {
            if (tf->tf_pos.tfp_buf == NULL) {
                tf->tf_pos.tfp_buf =
                    su_vmem_reach(
                        tf->tf_file,
                        tf->tf_pos.tfp_block,
                        &byte_c);
                if (tf->tf_pos.tfp_buf == NULL) {
                    ss_dprintf_2(("xs_tf_peekextend:EOF\n"));
                    return (NULL);
                }
                ss_dassert(byte_c == tf->tf_blocksize);
            }
            if (byte_c - tf->tf_pos.tfp_offset > nbytes) {
                ncopied = nbytes;
            } else {
                ncopied = byte_c - tf->tf_pos.tfp_offset;
            }
            ss_dassert(ncopied > 0);
            ss_dprintf_2(("xs_tf_peekextend:memcpy(%d, %d, %d)\n",
                (int)p, (int)(tf->tf_pos.tfp_buf + tf->tf_pos.tfp_offset), ncopied));
            memcpy(p, tf->tf_pos.tfp_buf + tf->tf_pos.tfp_offset, ncopied);
            p += ncopied;
            nbytes -= ncopied;
            su_vmem_release(
                tf->tf_file,
                tf->tf_pos.tfp_block,
                FALSE);
            tf->tf_pos.tfp_buf = NULL;

            tf->tf_pos.tfp_offset = 0;
            tf->tf_pos.tfp_block++;
        }
        tf->tf_pos.tfp_block  = saved_block;
        tf->tf_pos.tfp_offset = saved_offset;
        ss_dprintf_2(("xs_tf_peekextend:return editbuf=%d\n",
            (int)tf->tf_editbuf));
        return (tf->tf_editbuf);
}


/*##**********************************************************************\
 * 
 *		xs_tf_moveposrel
 * 
 * Changes the file position relative to current pos
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 *	diff - in
 *		relative position change in bytes, may also be negative
 *		
 * Return value :
 *      TRUE when successful, 
 *      FALSE when attempt is made to go beyond EOF or BOF
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tf_moveposrel(xs_tf_t* tf, long diff)
{
        ulong abs_diff;
        su_daddr_t newblock;
        size_t newoffset;

        ss_dassert(tf != NULL);
        if (diff < 0) {
            abs_diff = (ulong)-diff;
            newblock = tf->tf_pos.tfp_block - abs_diff / tf->tf_blocksize;
            abs_diff %= tf->tf_blocksize;
            if ((size_t)abs_diff > tf->tf_pos.tfp_offset) {
                newblock--;
                newoffset = tf->tf_blocksize;
            } else {
                newoffset = 0;
            }
            newoffset += tf->tf_pos.tfp_offset;
            newoffset -= (size_t)abs_diff;
            if (newblock > tf->tf_pos.tfp_block) {
                return (FALSE);
            }
        } else {
            abs_diff = (ulong)diff;
            abs_diff += tf->tf_pos.tfp_offset;
            newblock = tf->tf_pos.tfp_block + abs_diff / tf->tf_blocksize;
            newoffset = (size_t)(abs_diff % tf->tf_blocksize);
            if (newblock > tf->tf_size.tfs_block
            ||  (newblock == tf->tf_size.tfs_block &&
                 newoffset > tf->tf_size.tfs_offset))
            {
                return (FALSE);
            }
        }
        if (tf->tf_pos.tfp_block != newblock
        &&  tf->tf_pos.tfp_buf != NULL) {
            su_vmem_release(
                tf->tf_file,
                tf->tf_pos.tfp_block,
                FALSE);
            tf->tf_pos.tfp_buf = NULL;
        }
        tf->tf_pos.tfp_block = newblock;
        tf->tf_pos.tfp_offset = newoffset;
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		xs_tf_movetobegin
 * 
 * Moves the file position to first byte in the file
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 * Return value :
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tf_movetobegin(xs_tf_t* tf)
{
        su_daddr_t block;
        bool wrotep = FALSE;
        bool succp;

        if (tf->tf_state == XSTF_WRITE
        &&  tf->tf_pos.tfp_offset != 0)
        {
            block = tf->tf_pos.tfp_block;
            wrotep = TRUE;
        } else {
            block = tf->tf_pos.tfp_block;
        }
        if (tf->tf_pos.tfp_buf != NULL) {
            su_vmem_release(
                tf->tf_file,
                block,
                wrotep);
        }
        tf->tf_pos.tfp_block = 0;
        tf->tf_pos.tfp_offset = 0;
        tf->tf_pos.tfp_buf = NULL;

        if (tf->tf_editbuf != NULL) {
            SsMemFree(tf->tf_editbuf);
            tf->tf_editbuf = NULL;
        }
        succp = su_vmem_syncsizeifneeded(tf->tf_file);
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_tf_movetoend
 * 
 * Moves the file position to one past last byte in the file
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 * Return value : 
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tf_movetoend(xs_tf_t* tf)
{
        su_daddr_t newblock;
        size_t newoffset;

        ss_dassert(tf != NULL);
        newblock = tf->tf_size.tfs_block;
        newoffset = tf->tf_size.tfs_offset;
        if (newblock != tf->tf_pos.tfp_block && tf->tf_pos.tfp_buf != NULL) {
            su_vmem_release(
                tf->tf_file,
                tf->tf_pos.tfp_block,
                FALSE);
            tf->tf_pos.tfp_buf = NULL;
        }
        tf->tf_pos.tfp_block = newblock;
        tf->tf_pos.tfp_offset = newoffset;
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		xs_tf_append
 * 
 * Writes data to end of file
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 *	data - in, use
 *		pointer to write data
 *		
 *	nbytes - in
 *		# of bytes to write
 *
 *      p_errh - out, give
 *          in case of an error a pointer into a newly allocated error
 *          handle will be returned if p_errh != NULL
 *		
 * Return value : 
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tf_append(
        xs_tf_t* tf,
        void* data,
        size_t nbytes,
        rs_err_t** p_errh)
{
        size_t ntowrite;
        char* p;
        uint byte_c;
        vmem_addr_t addr;
        bool succp;

        ss_dassert(tf != NULL);
        ss_dassert(tf->tf_state == XSTF_WRITE);
        ss_dassert(tf->tf_size.tfs_block == tf->tf_pos.tfp_block);
        ss_dassert(tf->tf_size.tfs_offset == tf->tf_pos.tfp_offset);
        p = data;
        while (nbytes) {
            ntowrite = tf->tf_blocksize - tf->tf_pos.tfp_offset;
            if (nbytes < ntowrite) {
                ntowrite = nbytes;
            }
            if (tf->tf_pos.tfp_buf == NULL) {
                if (su_vmem_sizeinblocks(tf->tf_file)  <= tf->tf_pos.tfp_block) {
                    succp = tfdir_takeblockstouse(tf->tf_tfdir, 1, FALSE);
                    if (!succp) {
                        if (p_errh != NULL) {
                            rs_error_create(p_errh, XS_OUTOFCFGDISKSPACE);
                        }
                        return (FALSE);
                    }
                    tf->tf_pos.tfp_buf =
                        su_vmem_reachnew(
                            tf->tf_file,
                            &addr,
                            &byte_c);
                    if (tf->tf_pos.tfp_buf == NULL) {
                        if (p_errh != NULL) {
                            rs_error_create(p_errh, XS_OUTOFPHYSDISKSPACE);
                        }
                        return (FALSE);
                    }
                    ss_dassert(tf->tf_pos.tfp_block == addr);
                } else {
                    tf->tf_pos.tfp_buf =
                        su_vmem_reach(
                            tf->tf_file,
                            tf->tf_pos.tfp_block,
                            &byte_c);
                    if (tf->tf_pos.tfp_buf == NULL) {
                        if (p_errh != NULL) {
                            rs_error_create(p_errh, XS_ERR_SORTFAILED);
                        }
                        return (FALSE);
                    }
                }
                ss_dassert(byte_c == tf->tf_blocksize);
            }
            memcpy(tf->tf_pos.tfp_buf + tf->tf_pos.tfp_offset, p, ntowrite);
            nbytes -= ntowrite;
            p += ntowrite;
            tf->tf_pos.tfp_offset += ntowrite;
            if (tf->tf_pos.tfp_offset >= tf->tf_blocksize) {
                ss_dassert(tf->tf_pos.tfp_offset == tf->tf_blocksize);
                tf->tf_pos.tfp_offset %= tf->tf_blocksize;
                su_vmem_release(
                    tf->tf_file,
                    tf->tf_pos.tfp_block,
                    TRUE);
                tf->tf_pos.tfp_block++;
                tf->tf_pos.tfp_buf = NULL;
            }
        }
        tf->tf_size.tfs_block = tf->tf_pos.tfp_block;
        tf->tf_size.tfs_offset = tf->tf_pos.tfp_offset;
        return (TRUE);
}



/*##**********************************************************************\
 * 
 *		xs_tf_opencursor
 * 
 * Sets a temp. file to cursor mode and moves the cursor to first byte
 * in the file
 * 
 * Parameters : 
 * 
 *	tf - in out, use
 *		tf object
 *		
 * Return value : 
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tf_opencursor(xs_tf_t* tf)
{
        bool succp = TRUE;

        if (tf->tf_state == XSTF_CLOSED) {
            succp = xs_tf_open(tf);
        }
        if (succp) {
#if 0 /* Removed by pete 1996-10-21 to decrease resource usage */
      /* If You change back, remember to change xs_sorter_merge, too */
            if (tf->tf_state != XSTF_CURSOR) {
                xs_hmem_t* p1;
                void* p2;
                p1 = xs_mem_allocreserved(tf->tf_memmgr, &p2);
                ss_dassert(p1 != NULL);
                ss_dassert(p2 != NULL);
                ss_dassert(!su_pa_indexinuse(tf->tf_hmemarr, 1));
                su_pa_insertat(tf->tf_hmemarr, 1, p1);
                su_vmem_addbuffers(tf->tf_file, 1, &p2);
            }
#endif
            succp = xs_tf_movetobegin(tf);
        }
        tf->tf_state = XSTF_CURSOR;
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_tfmgr_init
 * 
 * Creates a temporary file manager object
 * 
 * Parameters : 
 * 
 *	maxfiles - in
 *		max. # of temporary files
 *		
 *	memmgr - hold
 *		buffer memory manager object
 *
 *      dbid - in
 *          database id #
 *
 * Return value - give :
 *      created tfmgr object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_tfmgr_t* xs_tfmgr_init(
        ulong maxfiles,
        xs_mem_t* memmgr,
        ulong dbid,
        int openflags)
{
        xs_tfmgr_t* tfmgr;

        ss_dassert(memmgr != NULL);
        tfmgr = SSMEM_NEW(xs_tfmgr_t);
        tfmgr->tfm_blocksize = xs_mem_getblocksize(memmgr);
        tfmgr->tfm_openflags = openflags;
        tfmgr->tfm_maxfiles = maxfiles;
        tfmgr->tfm_bitmap = SU_BMAP_INIT(tfmgr->tfm_maxfiles, FALSE);
        tfmgr->tfm_tfdirs = su_pa_init();
        tfmgr->tfm_ntfdirs = 0;
        tfmgr->tfm_mutex = SsSemCreateLocal(SS_SEMNUM_XS_TFMGR);
        tfmgr->tfm_memmgr = memmgr;
        tfmgr->tfm_dbid = dbid;
        return (tfmgr);
}

/*##**********************************************************************\
 * 
 *		xs_tfmgr_done
 * 
 * Deletes a temporary file manager object
 * 
 * Parameters : 
 * 
 *	tfmgr - in, take
 *		tf manager
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_tfmgr_done(xs_tfmgr_t* tfmgr)
{
        uint i;
        xs_tfdir_t* tfdir;
        ss_debug(long f_idx);

        ss_dassert(tfmgr != NULL);
        ss_debug(f_idx = su_bmap_find1st(tfmgr->tfm_bitmap, tfmgr->tfm_maxfiles, TRUE));
        ss_dassert(f_idx == -1L);
        su_pa_do_get(tfmgr->tfm_tfdirs, i, tfdir) {
            tfdir_done(tfdir);
        }
        su_pa_done(tfmgr->tfm_tfdirs);
        SU_BMAP_DONE(tfmgr->tfm_bitmap);
        SsSemFree(tfmgr->tfm_mutex);
        SsMemFree(tfmgr);
}

/*#***********************************************************************\
 * 
 *		tfmgr_checkdir
 * 
 * Checks that directory exists and there is a write permission for it.
 * 
 * Parameters : 
 * 
 *	dirname - 
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
static bool tfmgr_checkdir(char* dirname)
{
        char testpbuf[256];
        static char tmpfname[] = "solxsZZ.tmp";
        static char txt[] = "Solid external sorter test file\n";
        SS_FILE* fp;
        bool b;

        b = SsFnMakePath(dirname, tmpfname, testpbuf, sizeof(testpbuf));
        if (!b) {
            return(FALSE);
        }
        fp = SsFOpenB(testpbuf, (char *)"w");
        if (fp == NULL) {
            return(FALSE);
        }
        if (SsFWrite(txt, sizeof(txt) - 1, 1, fp) != 1) {
            b = FALSE;
        }
        SsFClose(fp);
        SsFRemove(testpbuf);
        return(b);
}

/*##**********************************************************************\
 * 
 *		xs_tfmgr_adddir
 * 
 * Adds a temorary file directory to manager
 * 
 * Parameters : 
 * 
 *	tfmgr - in out, use
 *		tf mgr object
 *		
 *	dirname - in, use
 *		directory name
 *		
 *	maxblocks - in
 *          max # of file blocks to be allocated in this directory
 *		
 * Return value : 
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tfmgr_adddir(xs_tfmgr_t* tfmgr, char* dirname, ulong maxblocks)
{
        xs_tfdir_t* tfdir;

        ss_dassert(tfmgr != NULL);
        ss_dassert(dirname != NULL);
        ss_dassert(maxblocks > 0);

        if (dirname[0] != '\0' && !tfmgr_checkdir(dirname)) {
            return (FALSE);
        }
        SsSemEnter(tfmgr->tfm_mutex);
        tfdir = tfdir_init(dirname, maxblocks, tfmgr->tfm_blocksize);
        su_pa_insertat(tfmgr->tfm_tfdirs, tfmgr->tfm_ntfdirs, tfdir);
        tfmgr->tfm_ntfdirs++;
        SsSemExit(tfmgr->tfm_mutex);
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		xs_tfmgr_tfinit
 * 
 * Creates a temporary file (tf) object
 * 
 * Parameters : 
 * 
 *	tfmgr - in out, use
 *		tf mgr object
 *		
 * Return value - give :
 *      created tf object. It must be freed using xs_tf_done().
 *      NULL when tfmgr ran out of resources
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_tf_t* xs_tfmgr_tfinit(xs_tfmgr_t* tfmgr)
{
        long f_idx;
        xs_tf_t* tf;
        uint tfd_idx;
        xs_tfdir_t* tfdir;

        SsSemEnter(tfmgr->tfm_mutex);
        f_idx = tf_allocatefileidx(tfmgr);
        if (f_idx == -1L) {
            /* Max. # of tmp files reached */
            tf = NULL;
            goto exitcode;
        }
        tfd_idx = f_idx % tfmgr->tfm_ntfdirs;
        tfdir = su_pa_getdata(tfmgr->tfm_tfdirs, tfd_idx);
        tf = tf_init(
                tfmgr,
                tfdir,
                tfmgr->tfm_memmgr,
                (ulong)f_idx,
                tfmgr->tfm_blocksize);
        if (tf == NULL) {
            /* file creation failed due to lack of resources */
            SU_BMAP_SET(tfmgr->tfm_bitmap, (ulong)f_idx, FALSE);
        }
 exitcode:;
        SsSemExit(tfmgr->tfm_mutex);
        return (tf);
}

