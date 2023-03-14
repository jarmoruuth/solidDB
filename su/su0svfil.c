/*************************************************************************\
**  source       * su0svfil.c
**  directory    * su
**  description  * Splitted virtual file routines for SOLID
**               * DB engine.
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

This module implements 'split virtual file' objects.
A split virtual file object looks like one open random
access file to the client. Actually these files may consist
of several physical files thus allowing the use of several disks
and partitions as a single logical database file. A file address is a 
long integer (at least 32 bit). It is a block address to the file
allowing file size to be blocksize * 2^31 bytes. if direct
byte addressing is required the block size can be set to 1.

The reason why the block addressing was chosen instead of byte addressing
was the fact that most C language environments do not support
longer than 32 bit integer arithmetic and I was too lazy to implement
such a thing. The second reason is that it saves some disk space
in the database file. Another alternative method for byte
addressing would have been a file number - byte offset pair. This
was rejected because the contents of the database file would become
dependent on the physical file boundaries and copying database
to devices with different partition sizes would become awkward.

The files grow in the order given by the client and the
splitting point is always a multiple of block size, ie. the configured
maximum size of the file is actually truncated to the nearest multiple of
block size. The splitting point may also be modified to a smaller value.

For both efficiency and security reasons the block size should be set to
a multiple of the file system file cluster size (typically 512-8192 bytes).

The files can be persistent, which means at least one physical
file handle is always kept open for that file. This enables locking
parts of the file. Locking is not allowed for non-persistent files.
A non-persistent file may be physically closed if it has not been
accessed recently.

Limitations:
-----------

- The block addressing scheme disables byte addressing
(with the exception above).

- su_svf_lockregion() is only allowed for range that is in persistent
files.

Error handling:
--------------

File size extend errors (= disk full or configured physical file size
exceeded) are handled automatically if there is room in the next configured
file and no error code is returned. If the file size extend is not possible
in the next file, then an error code is returned. Also physical file system
errors are returned in the return code of the method.


Objects used:
------------

Virtual file handles "su0vfil.h"
Portable semaphore functions <sssem.h>
Portable file system functions <ssfile.h>
Binary search function su_bsearch() <su0bsrch.h>

Preconditions:
-------------

The global initialization for virtual file handles must be done.
The block size must be known before the split virtual file object
is created.

Multithread considerations:
--------------------------

The methods for split virtual file support multithreaded client
applications. All critical sections are automatically mutex:ed.

Example:
-------

#include <su0svfil.h>

main()
{
        su_svfil_t *svfp;

        ...
        su_vfh_globalinit(10);      /* init. virt. file handles maxopen=10*/
        svfp = su_svf_init(8192, FALSE);   /* create svf object block=8192 */

        /* configure 2 physical database files of approx. 100 MB */
        /* the first handle is perssitent 2 is not */
        su_svf_addfile(svfp, "/disk1/solid/file1.db", 100000000L, TRUE);
        su_svf_addfile(svfp, "/disk2/solid/file2.db", 100000000L, FALSE);

        ...
        /* su_svf_write(), su_svf_read()
         * su_svf_append(), su_svf_extendsize(),
         * su_svf_flush()
         * calls in the application
         */
        ...
        su_svf_done(svfp);
        su_vfh_globaldone();
}

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssenv.h>
#include <ssstring.h>

#include <ssfile.h>
#include <sssem.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssfnsplt.h>
#include <ssint8.h>

#include "su0bsrch.h"
#include "su0svfil.h"
#include "su0vfil.h"
#include "su0prof.h"
#include "su0cipher.h"

#ifndef SS_MYSQL
#include "su0svfcrypt.h"
#endif /* !SS_MYSQL */

#if defined(SS_PERS_HANDLES_ONLY)
#define SU_SVFIL_PERS_ONLY
#endif

/* allocation chunck for array of virt. file ptrs */
#define SVF_VFP_ARRBLK 16   

/* structure to represent a single physical file */
typedef struct vfil_st {
        su_vfilh_t *vf_hp;          /* pointer to virtual file handle */
        su_daddr_t vf_maxszblks;    /* max. size in blocks */
        su_daddr_t vf_startdaddr;   /* 1st disk block address in file */
        su_daddr_t vf_szblks;       /* current size in blocks */
        uint       vf_diskno;       /* phys. disk # of file */
} vfil_t;

/* Splitted virtual file structure */
struct su_svfil_st {
        vfil_t**   svf_vfp_arr;     /* array of vfil_t *'s */
        size_t     svf_vfp_arrused; /* used size of the above^ */
        size_t     svf_vfp_arrsz;   /* size of the above^^ */
        size_t     svf_vfp_arralloc;/* allocated size of the above^^^ */
        SsSemT*    svf_mutex;       /* mutex for multithreaded apps */

        size_t     svf_blksz;       /* database block size */
        su_daddr_t svf_nrblks;      /* # of blocks allocated now in database */
        uint       svf_flags;
        bool svf_fixed_size_filespecs; /* when TRUE the file sizes will
                                        * not be adjusted dynamically
                                        * (for parallel backup target file)
                                        */
        ss_int8_t   svf_nbytes_written; /* Number of bytes written. */
        void*       svf_cipher;

        char*  (SS_CALLBACK *svf_encrypt)(void *cipher, su_daddr_t daddr, char *page,
                                  int npages, size_t pagesize);
        bool   (SS_CALLBACK *svf_decrypt)(void *cipher, su_daddr_t daddr, char *page,
                                  int npages, size_t pagesize);
};

static vfil_t *vf_init(
        char *pathname,
        ss_int8_t maxsz,
        size_t blksz,
        su_daddr_t startdaddr,
        bool persistent,
        uint flags,
        uint diskno,
        bool fixed_filespec);

static void vf_done(
        vfil_t *vfp);

static int su_svf_cmp(
        const void *addrp,
        const void *vfp);

static su_ret_t su_svf_findvfil(
        su_svfil_t *svfp,
        su_daddr_t addr,
        uint *retidx);

static su_ret_t su_svf_extendszlocal(
        su_svfil_t *svfp,
        su_daddr_t sz);

static void su_svf_updateranges(
        su_svfil_t *svfp,
        uint startidx);

static su_ret_t su_svf_readlocal(
        su_svfil_t *svfp,
        su_daddr_t loc,
        bool crypt_enabled,
        void *data,
        size_t size,
        size_t *sizeread,
        SsBFileT *(*p_beginaccess_fun)(
            su_vfilh_t *vfhp,
            su_pfilh_t **pp_pfh));

static su_ret_t su_svf_writelocal(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        SsBFileT *(*p_beginaccess_fun)(
            su_vfilh_t *vfhp,
            su_pfilh_t **pp_pfh));

static su_ret_t svf_addfile2_nomutex(
        su_svfil_t *svfp,
        char *pathname,
        ss_int8_t maxsize,
        bool persistent,
        uint diskno);

static su_ret_t su_svf_crypt_whole_file(
        su_svfil_t*  svfp,
        su_cipher_t* old_cipher);

static su_svfil_t *svf_init(
        size_t blocksize,
        uint flags,
        bool fixed_size_filespecs)
{
        su_svfil_t *svfp;

#ifdef SS_NT /* Windows only allows 512 byte alignment for NOBUFFERING files */
        ss_dassert((blocksize % 512) == 0 ||
                   !(flags & SS_BF_NOBUFFERING));
#endif /* SS_NT */
        svfp = SsMemAlloc(sizeof(su_svfil_t));
        svfp->svf_mutex = SsSemCreateLocal(SS_SEMNUM_SU_SVFIL);
        svfp->svf_vfp_arr =
            SsMemAlloc((svfp->svf_vfp_arralloc = SVF_VFP_ARRBLK) *
                       sizeof(vfil_t*));
        svfp->svf_vfp_arrsz = 0;
        svfp->svf_vfp_arrused = 0;
        svfp->svf_blksz = blocksize;
        svfp->svf_nrblks = 0L;
        svfp->svf_flags = flags;
        svfp->svf_fixed_size_filespecs = fixed_size_filespecs;
        SsInt8Set0(&svfp->svf_nbytes_written);
        svfp->svf_cipher = NULL;
        svfp->svf_encrypt = NULL;
        svfp->svf_decrypt = NULL;

        ss_dprintf_1 (("svf_init: svfp=%p\n", svfp));

        return svfp;
}

/*##**********************************************************************\
 * 
 *		su_svf_init
 * 
 * Creates su_svfil_t structure
 * 
 * Parameters : 
 * 
 *	blocksize - in
 *		block size used
 *
 *      flags - in
 *          file flags, SS_BF_*
 *
 * Return value - give : 
 *          pointer to su_svfil_t structure
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_svfil_t* su_svf_init(
        size_t blocksize,
        uint flags)
{
        su_svfil_t *svfp;

        svfp = svf_init(blocksize, flags, FALSE);
        return (svfp);
}

/*##**********************************************************************\
 * 
 *          su_svf_init_fixed
 * 
 * Creates su_svfil_t structure, with 'fixed size filespec'
 * flag meaning all files are virtually set to their maximum
 * size so that parallelized writing is possible without growing
 * the splitted file first. This is needed for implementing
 * the target file for parallel backup
 * 
 * Parameters : 
 * 
 *      blocksize - in
 *          block size used
 *
 *      flags - in
 *          file flags, SS_BF_*
 *
 * Return value - give : 
 *          pointer to su_svfil_t structure
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_svfil_t* su_svf_init_fixed(
        size_t blocksize,
        uint flags)
{
        su_svfil_t *svfp;

        svfp = svf_init(blocksize, flags, TRUE);
        return (svfp);
}

/*##**********************************************************************\
 * 
 *		su_svf_initcopy
 * 
 * Initializes a new split virtual file for copying. The copying is done
 * from another split virtual file. The directory to where the svfil is
 * copied is given in parameter dir.
 * 
 * Parameters : 
 * 
 *	dir - in, use
 *		Directory where the svfp is copied.
 *		
 *	svfp - in, use
 *		Split virtual file that is copied to dir.
 *
 *      p_copysize - out
 *          pointer to variable where to store the resulting file size
 *          in blocks
 *		
 *      flags - in
 *          file flags, SS_BF_*
 *
 *	p_rc - out
 *		If return value is NULL, the error code is returned in *p_rc.
 *		
 * Return value : 
 * 
 * Comments     : 
 * 
 *      If svfp contains files with different paths, they are all
 *      have the same path in returned svfil.
 * 
 * Globals used : 
 */
su_svfil_t *su_svf_initcopy(
        char* dir,
        su_svfil_t *svfp,
        su_daddr_t* p_copysize,
        uint flags,
        su_ret_t* p_rc)
{
        int i;
        su_svfil_t *copy_svfp;

        copy_svfp = su_svf_init(svfp->svf_blksz, flags);
        ss_dassert(copy_svfp != NULL);

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        *p_copysize = svfp->svf_nrblks;

        /* Add all files from svfp to copy_svfp.
         */
        for (i = 0; i < (int)svfp->svf_vfp_arrsz; i++) {
            vfil_t *vfp;
            su_daddr_t maxsize_blks;
            ss_int8_t maxsize_bytes;
            vfp = svfp->svf_vfp_arr[i];

            if (vfp->vf_szblks > 0) {
                /* Current size nonzero, add the file with a different
                 * directory path.
                 */
                char* vf_path;
                char vf_dir[128];
                char vf_file[128];
                char copy_path[255];
                bool b;

                vf_path = su_vfh_getfilename(vfp->vf_hp);
                ss_dassert(vf_path != NULL);

                b = SsFnSplitPath(
                        vf_path,
                        vf_dir,
                        sizeof(vf_dir),
                        vf_file,
                        sizeof(vf_file));
                if (b) {
                    b = SsFnMakePath(
                            dir,
                            vf_file,
                            copy_path,
                            sizeof(copy_path));
                }

                if (!b) {
                    *p_rc = SU_ERR_TOO_LONG_FILENAME; 
                    SsSemExit(svfp->svf_mutex);
                    /***** MUTEXEND *******/
                    su_svf_done(copy_svfp);
                    return(NULL);
                }

                if (i < (int)(svfp->svf_vfp_arrsz - 1)) {
                    /* Not the last file. */
                    maxsize_blks = vfp->vf_szblks;
                } else {
                    /* Last file. */
                    maxsize_blks = SU_DADDR_MAX;
                    if (i > 0) {
                        vfil_t* vfp = svfp->svf_vfp_arr[i - 1];
                        maxsize_blks -= vfp->vf_startdaddr + vfp->vf_maxszblks;
                    }
                }
                {
                    ss_int8_t i8_tmp;
                    SsInt8SetUint4(&maxsize_bytes, maxsize_blks);
                    SsInt8SetUint4(&i8_tmp, (ss_uint4_t)svfp->svf_blksz);
                    SsInt8MultiplyByInt8(&maxsize_bytes,
                                         maxsize_bytes, i8_tmp);
                }
                *p_rc = SU_SUCCESS;
                if (SsFExist(copy_path)) {
                    SsFRemove(copy_path);
                    if (SsFExist(copy_path)) {
                        *p_rc = SU_ERR_BACKUPFILENOTREMOVABLE;
                    }
                }
                if (*p_rc == SU_SUCCESS) {
                    *p_rc = svf_addfile2_nomutex(
                            copy_svfp,
                            copy_path,
                            maxsize_bytes,
                            FALSE,
                            0);
                }

                if (*p_rc != SU_SUCCESS) {
                    SsSemExit(svfp->svf_mutex);
                    /***** MUTEXEND *******/
                    su_svf_done(copy_svfp);
                    return(NULL);
                }
            }
        }

        copy_svfp->svf_cipher = NULL; /* svfp->svf_cipher; */
        copy_svfp->svf_encrypt = svfp->svf_encrypt;
        copy_svfp->svf_decrypt = svfp->svf_decrypt;

        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        return(copy_svfp);
}

/*##**********************************************************************\
 * 
 *		su_svf_done
 * 
 * Deletes a su_svfil_t structure
 * 
 * Parameters : 
 * 
 *	svfp - in, take
 *		pointer to su_svfil_t structure
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void su_svf_done(svfp)
	su_svfil_t *svfp;
{
        uint i;

        SsSemFree(svfp->svf_mutex);
        svfp->svf_mutex = NULL;
        for (i = 0; i < svfp->svf_vfp_arrsz; i++) {
            vf_done(svfp->svf_vfp_arr[i]);
        }
        SsMemFree(svfp->svf_vfp_arr);
        SsMemFree(svfp);
}

static su_ret_t svf_addfile2_nomutex(
        su_svfil_t *svfp,
        char *pathname,
        ss_int8_t maxsize_bytes,
        bool persistent,
        uint diskno)
{
        uint f_idx;         /* index of file in svf_vfp_arr */
        vfil_t *vfp;
        su_daddr_t daddr;   /* disk address of start of file */
        ss_int8_t maxbytesize_possible;

        FAKE_RETURN(FAKE_SU_SVFADDFILE,SU_ERR_FILE_OPEN_FAILURE);

        ss_dprintf_1(("su_svf_addfile2_nomutex: "
                      "svfp=0x%08lX"
                      "pathname=%s,"
                      "maxsize_bytes=0x%08lX%08lX\n",
                      (ulong)(void*)svfp,
                      pathname,
                      (ulong)SsInt8GetMostSignificantUint4(maxsize_bytes),
                      (ulong)SsInt8GetLeastSignificantUint4(maxsize_bytes)));
                      
   
        f_idx = (uint)svfp->svf_vfp_arrsz;
        if (f_idx > 0) {   /* file is not 1st */
            uint i;

            for (i = 0; i < f_idx; i++) {
                if (strcmp(pathname,
                        su_vfh_getfilename(svfp->svf_vfp_arr[i]->vf_hp))
                  == 0)
                {
                    return (SU_ERR_DUPLICATE_FILENAME);
                }
            }
            daddr = svfp->svf_vfp_arr[f_idx-1]->vf_startdaddr +
                    svfp->svf_vfp_arr[f_idx-1]->vf_maxszblks;
        } else {            /* file is 1st */
            daddr = 0L;
        }
        {
            ss_int8_t i8_tmp;
            SsInt8SetUint4(&i8_tmp, (ss_uint4_t)svfp->svf_blksz);
            SsInt8SetUint4(&maxbytesize_possible, SU_DADDR_MAX - daddr);
            SsInt8MultiplyByInt8(&maxbytesize_possible,
                                 maxbytesize_possible,
                                 i8_tmp);
        }
        if (SsInt8Cmp(maxbytesize_possible, maxsize_bytes) < 0) {
            return (SU_ERR_FILE_ADDRESS_SPACE_EXCEEDED);
        }
#if defined(SU_SVFIL_PERS_ONLY)
        vfp = vf_init(
                pathname,
                maxsize_bytes,
                svfp->svf_blksz,
                daddr,
                TRUE,
                svfp->svf_flags,
                diskno,
                svfp->svf_fixed_size_filespecs);
#else
        vfp = vf_init(
                pathname,
                maxsize_bytes,
                svfp->svf_blksz,
                daddr,
                persistent,
                svfp->svf_flags,
                diskno,
                svfp->svf_fixed_size_filespecs);
#endif
        if (vfp == NULL) {
            return (SU_ERR_FILE_OPEN_FAILURE);
        }
        if (svfp->svf_vfp_arralloc <= svfp->svf_vfp_arrsz) {
            svfp->svf_vfp_arralloc *= 2;
            svfp->svf_vfp_arr = SsMemRealloc(svfp->svf_vfp_arr,
                                             svfp->svf_vfp_arralloc *
                                             sizeof(vfil_t*));
        }
        svfp->svf_vfp_arr[f_idx] = vfp;
        svfp->svf_vfp_arrsz++;
        if (vfp->vf_szblks > 0L) {
            svfp->svf_vfp_arrused++;
            if (f_idx > 0) {    /* if not 1st file, check size of previous */
                int f_idx1;
                su_daddr_t fsblocks;
                vfil_t *vfp1;   /* pointer to prev. file */

                for (f_idx1 = f_idx - 1;
                     ;
                     f_idx1--)
                {
                    if (f_idx1 < 0) {
                        vfp->vf_startdaddr = 0;
                        break;
                    }
                    vfp1 = svfp->svf_vfp_arr[f_idx1];
                    fsblocks = vfp1->vf_szblks;
                    if (fsblocks == 0) {
                        /* special case, need to remove the preceding file! */
                        vf_done(vfp1);
                        ss_dassert(f_idx1 + 1 == (int)f_idx);
                        svfp->svf_vfp_arr[f_idx1] = svfp->svf_vfp_arr[f_idx];
                        f_idx--;
                        svfp->svf_vfp_arrsz--;
                    } else {
                        vfp1->vf_maxszblks = fsblocks;
                        vfp->vf_startdaddr =
                            vfp1->vf_maxszblks + vfp1->vf_startdaddr;
                        break; /* no need to continue loop! */
                    }
                }
            }
            svfp->svf_nrblks = vfp->vf_startdaddr + vfp->vf_szblks;
        }
        return SU_SUCCESS;
}

/*##**********************************************************************\
 * 
 *		su_svf_addfile
 * 
 * Adds a file to su_svfil_t structure
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	pathname - in, use
 *		file name
 *
 *	maxsize - in
 *		maximum size in bytes
 *
 *      persistent - in 
 *		the file must be kept open all the time
 *
 * 
 * Return value : SU_SUCCESS if OK or
 *                SU_ERR_FILE_OPEN_FAILURE if not
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t su_svf_addfile(
        su_svfil_t *svfp,
        char *pathname,
        ss_int8_t maxsize,
        bool persistent)
{
        su_ret_t rc;

        rc = su_svf_addfile2(svfp, pathname, maxsize, persistent, 0);
        return (rc);
}

/*##**********************************************************************\
 * 
 *		su_svf_addfile2
 * 
 * Adds a file to su_svfil_t structure
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	pathname - in, use
 *		file name
 *
 *	maxsize - in
 *		maximum size in bytes
 *
 *      persistent - in 
 *		the file must be kept open all the time
 *
 *      diskno - in
 *          physical disk number of the file
 * 
 * Return value : SU_SUCCESS if OK or
 *                SU_ERR_FILE_OPEN_FAILURE if not
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t su_svf_addfile2(
        su_svfil_t *svfp,
        char *pathname,
        ss_int8_t maxsize,
        bool persistent,
        uint diskno)
{
        su_ret_t ret;

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        ret = svf_addfile2_nomutex(
                svfp,
                pathname,
                maxsize,
                persistent,
                diskno);

        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        return(ret);
}

/*##**********************************************************************\
 * 
 *		su_svf_read
 * 
 * Read from splittable virtual file
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	loc - in
 *		file block address
 *
 *	data - out, use
 *		pointer to data buffers
 *
 *	size - in
 *		size of bytes to transfer
 * 
 *	sizeread - out
 *		pointer to size_t objects telling how many bytes read
 *
 * 
 * Return value : SU_SUCCESS if OK
 *                SU_ERR_FILE_READ_EOF if EOF reached during read
 *                SU_ERR_FILE_READ_ILLEGAL_ADDR attempt to start reading
 *                out of file bounds.
 * 
 * Limitations  : start addresses are at block boundary
 * 
 * Globals used : none
 */
su_ret_t su_svf_read(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        size_t *sizeread)
{
        ss_dprintf_1(("su_svf_read\n"));

        FAKE_RETURN(FAKE_SU_SVFREADEOF,SU_ERR_FILE_READ_EOF);
        FAKE_RETURN(FAKE_SU_SVFREADILLADDR,SU_ERR_FILE_READ_ILLEGAL_ADDR);

#if defined(SU_SVFIL_PERS_ONLY)
        return (su_svf_readlocal(svfp, loc, TRUE, data, size, sizeread, su_vfh_beginaccesspers));
#else
        return (su_svf_readlocal(svfp, loc, TRUE, data, size, sizeread, su_vfh_beginaccess));
#endif
}

/*##**********************************************************************\
 *
 *      su_svf_read_raw
 *
 * Read from splittable virtual file without decryption.
 *
 * Parameters :
 *
 *  svfp - in out, use
 *      pointer to su_svfil_t structure
 *
 *  loc - in
 *      file block address
 *
 *  data - out, use
 *      pointer to data buffers
 *
 *  size - in
 *      size of bytes to transfer
 *
 *  sizeread - out
 *      pointer to size_t objects telling how many bytes read
 *
 *
 * Return value : SU_SUCCESS if OK
 *                SU_ERR_FILE_READ_EOF if EOF reached during read
 *                SU_ERR_FILE_READ_ILLEGAL_ADDR attempt to start reading
 *                out of file bounds.
 *
 * Limitations  : start addresses are at block boundary
 *
 * Globals used : none
 */
su_ret_t su_svf_read_raw(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        size_t *sizeread)
{
        ss_dprintf_1(("su_svf_read\n"));

        FAKE_RETURN(FAKE_SU_SVFREADEOF,SU_ERR_FILE_READ_EOF);
        FAKE_RETURN(FAKE_SU_SVFREADILLADDR,SU_ERR_FILE_READ_ILLEGAL_ADDR);

#if defined(SU_SVFIL_PERS_ONLY)
        return (su_svf_readlocal(svfp, loc, FALSE, data, size, sizeread, su_vfh_beginaccesspers));
#else
        return (su_svf_readlocal(svfp, loc, FALSE, data, size, sizeread, su_vfh_beginaccess));
#endif
}

/*##**********************************************************************\
 * 
 *		su_svf_write
 * 
 * Write to splittable virtual file
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	loc - in
 *		file block address
 *
 *	data - in, use
 *		pointer to data to write
 *
 *	size - in
 *		size in bytes to write
 *
 * 
 * Return value : SU_SUCCESS if OK
 *                SU_ERR_FILE_WRITE_FAILURE if write fails in existing
 *                file range
 *                SU_ERR_FILE_WRITE_DISK_FULL if write fails beyond file
 *                range. 
 * 
 * Limitations  : start addresses are at block boundary
 * 
 * Globals used : none
 */
su_ret_t su_svf_write(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size)
{
        ss_dprintf_1(("su_svf_write\n"));
        
        FAKE_RETURN(FAKE_SU_SVFWRITEFAILS,SU_ERR_FILE_WRITE_FAILURE);
        FAKE_RETURN(FAKE_SU_SVFWRITEDFULL,SU_ERR_FILE_WRITE_DISK_FULL);

        ss_dassert((size % svfp->svf_blksz) == 0 && size != 0);
#if defined(SU_SVFIL_PERS_ONLY)
        return (su_svf_writelocal(svfp, loc, data, size, su_vfh_beginaccesspers));
#else
        return (su_svf_writelocal(svfp, loc, data, size, su_vfh_beginaccess));
#endif
}

/*##**********************************************************************\
 * 
 *		su_svf_readlocked
 * 
 * Read from locked area of splittable virtual file
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	loc - in
 *		file block address
 *
 *	data - out, use
 *		pointer to data buffers
 *
 *	size - in
 *		size of bytes to transfer
 *
 *	sizeread - out
 *		pointer to size_t objects telling how many bytes read
 *
 * 
 * Return value : SU_SUCCESS if OK
 *                SU_ERR_FILE_READ_EOF if EOF reached during read
 *                SU_ERR_FILE_READ_ILLEGAL_ADDR attempt to start reading
 *                out of file bounds.
 * 
 * Limitations  : start addresses are at block boundary
 * 
 * Globals used : none
 */
su_ret_t su_svf_readlocked(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        size_t *sizeread)
{
        ss_dprintf_1(("su_svf_readlocked\n"));
        
        FAKE_RETURN(FAKE_SU_SVFREADEOF,SU_ERR_FILE_READ_EOF);
        FAKE_RETURN(FAKE_SU_SVFREADILLADDR,SU_ERR_FILE_READ_ILLEGAL_ADDR);

        return (su_svf_readlocal(svfp, loc, TRUE, data, size, sizeread, su_vfh_beginaccesspers));
}

/*##**********************************************************************\
 * 
 *		su_svf_readlocked_raw
 * 
 * Read from locked area of splittable virtual file without decryption.
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	loc - in
 *		file block address
 *
 *	data - out, use
 *		pointer to data buffers
 *
 *	size - in
 *		size of bytes to transfer
 *
 *	sizeread - out
 *		pointer to size_t objects telling how many bytes read
 *
 * 
 * Return value : SU_SUCCESS if OK
 *                SU_ERR_FILE_READ_EOF if EOF reached during read
 *                SU_ERR_FILE_READ_ILLEGAL_ADDR attempt to start reading
 *                out of file bounds.
 * 
 * Limitations  : start addresses are at block boundary
 * 
 * Globals used : none
 */
su_ret_t su_svf_readlocked_raw(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        size_t *sizeread)
{
        ss_dprintf_1(("su_svf_readlocked\n"));
        
        FAKE_RETURN(FAKE_SU_SVFREADEOF,SU_ERR_FILE_READ_EOF);
        FAKE_RETURN(FAKE_SU_SVFREADILLADDR,SU_ERR_FILE_READ_ILLEGAL_ADDR);

        return (su_svf_readlocal(svfp, loc, FALSE, data, size, sizeread, su_vfh_beginaccesspers));
}

/*##**********************************************************************\
 * 
 *		su_svf_writelocked
 * 
 * Write to locked area of splittable virtual file
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	loc - in
 *		file block address
 *
 *	data - in, use
 *		pointer to data to write
 *
 *	size - in
 *		size in bytes to write
 *
 * 
 * Return value : SU_SUCCESS if OK
 *                SU_ERR_FILE_WRITE_FAILURE if write fails in existing
 *                file range
 *                SU_ERR_FILE_WRITE_DISK_FULL if write fails beyond file
 *                range. 
 * 
 * Limitations  : start addresses are at block boundary
 * 
 * Globals used : none
 */
su_ret_t su_svf_writelocked(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size)
{
        ss_dprintf_1(("su_svf_writelocked\n"));
        
        FAKE_RETURN(FAKE_SU_SVFWRITEFAILS,SU_ERR_FILE_WRITE_FAILURE);
        FAKE_RETURN(FAKE_SU_SVFWRITEDFULL,SU_ERR_FILE_WRITE_DISK_FULL);
        
        return (su_svf_writelocal(svfp, loc, data, size, su_vfh_beginaccesspers));
}

/*##**********************************************************************\
 * 
 *		su_svf_lockrange
 * 
 * Locks range of database file using exclusive lock
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to split virtual file object
 *
 *	start - in
 *		start block address of lock range
 *
 *	length - in
 *		lock range length in blocks
 *
 * 
 * Return value : SU_SUCCESS if OK or
 *                SU_ERROR_FILE_CFG_EXCEEDED if out of conf. space
 *                SU_ERROR_FILE_LOCK_FAILURE if file system failure
 * 
 * Limitations  : The files under lock range must be persistent
 *                for the locks to remain persistent.
 * 
 * Globals used : none
 */
su_ret_t su_svf_lockrange(
        su_svfil_t *svfp,
        su_daddr_t start,
        su_daddr_t length)
{
        su_pfilh_t *pfhp;
        SsBFileT *bfilep;
        su_ret_t rc;
        bool rc2;
        vfil_t *vfp;
        uint f_idx;             /* file index in svfp->svf_vfp_arr */
        su_daddr_t room_blks;   /* room in current file in blocks */

        ss_dassert(length > 0);
        while (length > 0) {
            /***** MUTEXBEGIN *****/
            SsSemEnter(svfp->svf_mutex);
            rc = su_svf_findvfil(svfp, start, &f_idx);
            SsSemExit(svfp->svf_mutex);
            /***** MUTEXEND *******/
            if (rc != SU_SUCCESS) {
                return rc;
            }
            vfp = svfp->svf_vfp_arr[f_idx];
            room_blks = vfp->vf_startdaddr + vfp->vf_maxszblks - start;
            if (length <= room_blks) {
                room_blks = length;
            }
            bfilep = su_vfh_beginaccesspers(vfp->vf_hp, &pfhp);
            if (bfilep == NULL) {
                rc2 = FALSE;
            } else {
                uint lock_flags;

                lock_flags = (svfp->svf_flags & SS_BF_READONLY) ?
                    SSF_SHLOCK : SSF_EXLOCK;
                rc2 = SsBLockPages(
                    bfilep,
                    (start - vfp->vf_startdaddr),
                    svfp->svf_blksz,
                    room_blks,
                    lock_flags);
                su_vfh_endaccess(vfp->vf_hp, pfhp);
            }
            if (!rc2) {
                return SU_ERR_FILE_LOCK_FAILURE;
            }
            length -= room_blks;
            start += room_blks;
        }
        return SU_SUCCESS;
}

/*##**********************************************************************\
 * 
 *		su_svf_unlockrange
 * 
 * Unlocks range of database file
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to split virtual file object
 *
 *	start - in
 *		start block address of unlock range
 *
 *	length - in
 *		unlock range length in blocks
 *
 * 
 * Return value : SU_SUCCESS if OK or
 *                SU_ERROR_FILE_CFG_EXCEEDED if out of conf. space
 *                SU_ERROR_FILE_UNLOCK_FAILURE if file system failure
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t su_svf_unlockrange(su_svfil_t *svfp, su_daddr_t start, su_daddr_t length)
{
        su_pfilh_t *pfhp;
        SsBFileT *bfilep;
        su_ret_t rc;
        bool rc2;
        vfil_t *vfp;
        uint f_idx;             /* file index in svfp->svf_vfp_arr */
        su_daddr_t room_blks;   /* room in current file in blocks */

        ss_dassert(length > 0);
        while (length > 0) {
            /***** MUTEXBEGIN *****/
            SsSemEnter(svfp->svf_mutex);
            rc = su_svf_findvfil(svfp, start, &f_idx);
            SsSemExit(svfp->svf_mutex);
            /***** MUTEXEND *******/
            if (rc != SU_SUCCESS) {
                return rc;
            }
            vfp = svfp->svf_vfp_arr[f_idx];
            room_blks = vfp->vf_startdaddr + vfp->vf_maxszblks - start;
            if (length < room_blks) {
                room_blks = length;
            }
            bfilep = su_vfh_beginaccesspers(vfp->vf_hp, &pfhp);
            if (bfilep == NULL) {
                rc2 = FALSE;
            } else {
                rc2 = SsBLock(
                    bfilep,
                    (long)((start - vfp->vf_startdaddr) * svfp->svf_blksz),
                    (long)(room_blks * svfp->svf_blksz),
                    SSF_UNLOCK);
                su_vfh_endaccess(vfp->vf_hp, pfhp);
            }
            if (!rc2) {
                return SU_ERR_FILE_UNLOCK_FAILURE;
            }
            length -= room_blks;
            start += room_blks;
        }
        return SU_SUCCESS;
}

/*##**********************************************************************\
 * 
 *		su_svf_extendsize
 * 
 * extends file to desired size
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	sz - in
 *		new size in blocks
 *
 * Return value : SU_SUCCESS if OK or
 *                SU_ERR_FILE_WRITE_DISK_FULL if disk full
 *                SU_ERR_FILE_WRITE_CFG_EXCEEDED if out of configured space
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t su_svf_extendsize(
        su_svfil_t *svfp,
        su_daddr_t sz)
{
        su_ret_t rc;

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);
        rc = su_svf_extendszlocal(svfp, sz);
        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/
        return rc;
}

/*##**********************************************************************\
 *
 *      svf_set_vfp_size
 *
 * Truncates one OS file in virtual file to specified size.
 *
 * Parameters :
 *
 *  svfp - in out, use
 *      pointer to su_svfil_t structure
 *
 *  n - in
 *      index of the file to be truncated
 *
 *  szblks - in
 *      new file size in blocks
 *
 * Return value : SU_SUCCESS if OK or
 *                SU_ERR_FILE_WRITE_FAILURE in case of i/o error.
 *
 * Limitations  : Does not do any mutexing.
 *
 * Globals used : none
 */

static su_ret_t svf_set_vfp_size (su_svfil_t* svfp, size_t n, su_daddr_t szblks)
{
        su_pfilh_t* pfhp;
        vfil_t *vfp = svfp->svf_vfp_arr[n];
        SsBFileT *bfilep = su_vfh_beginaccess(vfp->vf_hp, &pfhp);
        bool succp;

        ss_dassert(bfilep != NULL);
        if (bfilep == NULL) {
            ss_derror;
            return (SU_ERR_FILE_WRITE_FAILURE);
        }
        ss_dassert(vfp->vf_szblks>=szblks);
        succp = SsBChsizePages(bfilep, szblks, svfp->svf_blksz);
        su_vfh_endaccess(vfp->vf_hp, pfhp);
        if (!succp) {
            ss_derror;
            return (SU_ERR_FILE_WRITE_FAILURE);
        }
        vfp->vf_szblks = szblks;
        return SU_SUCCESS;
}

/*##**********************************************************************\
 *
 *      svf_decreasesize_local
 *
 * Truncates virtual file to specified size.
 *
 * Parameters :
 *
 *  svfp - in out, use
 *      pointer to su_svfil_t structure
 *
 *  daddr - in
 *      new file size in blocks
 *
 * Return value : same as of svf_set_vfp_size
 *
 * Limitations  : Does not do any mutexing.
 *
 * Globals used : none
 */

static su_ret_t svf_decreasesize_local(su_svfil_t* svfp, su_daddr_t daddr)
{
        su_ret_t rc;
        uint f_idx;         /* index of file in svf_vfp_arr */
        vfil_t* vfp;
        long new_blocks;

        if (daddr == 0) {
            ss_derror;
            return (SU_ERR_FILE_WRITE_FAILURE);
        }

        /* Empty all the file array tail */
        for (f_idx = (uint)(svfp->svf_vfp_arrsz - 1); f_idx > 0; --f_idx) {
            vfp = svfp->svf_vfp_arr[f_idx];
            if (vfp->vf_startdaddr < daddr) {
                break;
            }
            rc = svf_set_vfp_size(svfp, f_idx, 0);
            if (rc != SU_SUCCESS) {
                return rc;
            }
        }
        svfp->svf_nrblks = daddr;
        vfp = svfp->svf_vfp_arr[f_idx];
        new_blocks = daddr - vfp->vf_startdaddr;
        ss_dassert(vfp->vf_szblks >= (ulong)new_blocks);
        rc = svf_set_vfp_size(svfp, f_idx, new_blocks);
        return rc;
}

/*##**********************************************************************\
 *
 *      su_svf_decreasesize
 *      
 * Truncates virtual file to specified size.
 * 
 * Parameters :
 * 
 *  svfp - in out, use
 *      pointer to su_svfil_t structure
 *      
 *  daddr - in
 *      new file size in blocks
 *
 * Return value : same as of svf_set_vfp_size
 *
 * Limitations  : none
 *
 * Globals used : none
 */

su_ret_t su_svf_decreasesize(su_svfil_t* svfp, su_daddr_t daddr)
{
        su_ret_t rc;

        ss_dprintf_1(("su_svf_decreasesizeby1()\n"));
        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);
        rc = svf_decreasesize_local(svfp, daddr);
        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/
        return rc;
}

/*##**********************************************************************\
 *
 *      su_svf_decreasesizeby1
 *      
 * Truncates virtual file by 1 block.
 * 
 * Parameters :
 * 
 *  svfp - in out, use
 *      pointer to su_svfil_t structure
 *      
 * Return value : same as of svf_set_vfp_size
 *
 * Limitations  : none
 *
 * Globals used : none
 */

su_ret_t su_svf_decreasesizeby1(su_svfil_t* svfp)
{
        return su_svf_decreasesize (svfp, svfp->svf_nrblks-1);
}

/*##**********************************************************************\
 * 
 *		su_svf_flush
 * 
 * Flushes to permanent storage (disk) all files associated with
 * splittable virtual file.
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void su_svf_flush(svfp)
	su_svfil_t *svfp;
{
        uint i;

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);
        for (i = 0; i < svfp->svf_vfp_arrused; i++) {
            su_vfh_flush(svfp->svf_vfp_arr[i]->vf_hp);
        }
        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/
}

/*##**********************************************************************\
 * 
 *		su_svf_getsize
 * 
 * Returns size of splittable virtual file in blocks
 * 
 * Parameters : 
 * 
 *	svfp - in, use
 *		pointer to su_svfil_t structure
 * 
 * Return value : size in blocks
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_daddr_t su_svf_getsize(
        su_svfil_t *svfp)
{
        ss_dprintf_2(("su_svf_getsize(): nrblks = %ld\n",
                      (long)svfp->svf_nrblks));
        return svfp->svf_nrblks;
}

/*##**********************************************************************\
 * 
 *		su_svf_getblocksize
 * 
 * Returns size of block
 * 
 * Parameters : 
 * 
 *	svfp - in, use
 *		pointer to su_svfil_t structure
 *
 * Return value : block size in bytes
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
size_t su_svf_getblocksize(
        su_svfil_t *svfp)
{
        return svfp->svf_blksz;
}
/*##**********************************************************************\
 * 
 *		su_svf_getmaxsize
 * 
 * Returns maximum size of splittable virtual file in blocks
 * 
 * Parameters : 
 * 
 *	svfp - in, use
 *		pointer to su_svfil_t structure
 * 
 * Return value : max size in blocks
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_daddr_t su_svf_getmaxsize(su_svfil_t *svfp)
{
        uint i;
        su_daddr_t maxsize;
        vfil_t *vfp;

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        maxsize = 0;
        for (i = 0; i < svfp->svf_vfp_arrsz; i++) {
            vfp = svfp->svf_vfp_arr[i];
            maxsize += vfp->vf_maxszblks;
        }
        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        return maxsize;
}
/*#**********************************************************************\
 * 
 *		vf_init
 * 
 * Creates a vfil_t object.
 * 
 * Parameters : 
 * 
 *	pathname - in, use
 *		valid file name
 *
 *	maxsz - in
 *		maximum size in bytes
 *
 *	blksz - in
 *		block size
 *
 *	startdaddr - in
 *		starting disk address in blocks
 *
 *      persistent - in
 *		TRUE if file needs to be open all the time
 *
 *      flags - in
 *          file flags, SS_BF_*
 *
 *      diskno - in
 *          physical disk device number
 *
 *      fixed_filespec - in
 *          TRUE means: emulate file is always its maximum size
 * 
 * Return value - give :
 *          pointer to vfilt_t structure or NULL on failure.
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static vfil_t *vf_init(
        char *pathname,
        ss_int8_t maxsz,
        size_t blksz,
        su_daddr_t startdaddr,
        bool persistent,
        uint flags,
        uint diskno,
        bool fixed_filespec)
{
        vfil_t *vfp;
        SsBFileT *bfilep;
        su_pfilh_t *pfhp;
        ss_int8_t i8;

        vfp = SsMemAlloc(sizeof(vfil_t));
#if defined(SU_SVFIL_PERS_ONLY)
        vfp->vf_hp = su_vfh_init(pathname, TRUE, flags, blksz);
#else
        vfp->vf_hp = su_vfh_init(pathname, persistent, flags, blksz);
#endif
        if (vfp->vf_hp == NULL) {
            SsMemFree(vfp);
            return NULL;
        }

        SsInt8SetUint4(&i8, (ss_uint4_t)blksz);
        SsInt8DivideByInt8(&i8, maxsz, i8);
        
        vfp->vf_maxszblks = SsInt8GetLeastSignificantUint4(i8);
        if (SsInt8GetMostSignificantUint4(i8) != 0) {
            ss_derror; /* should not be possible, see su_svf_addfile2_nomutex */
            vfp->vf_maxszblks = SU_DADDR_MAX;
        }
        vfp->vf_startdaddr = startdaddr;
        vfp->vf_diskno = diskno;
        bfilep = su_vfh_beginaccess(vfp->vf_hp, &pfhp);
        if (bfilep == NULL) {
            if (pfhp != NULL) {
                su_vfh_endaccess(vfp->vf_hp, pfhp);
            }
            su_vfh_done(vfp->vf_hp);
            SsMemFree(vfp);
            return NULL;
        }
        if (fixed_filespec) {
            vfp->vf_szblks = vfp->vf_maxszblks;
        } else {
            vfp->vf_szblks = SsBSizePages(bfilep, blksz);
        }
        if (vfp->vf_maxszblks < vfp->vf_szblks) {
            vfp->vf_szblks = vfp->vf_maxszblks;
        }
        su_vfh_endaccess(vfp->vf_hp, pfhp);
        return vfp;
}

/*#**********************************************************************\
 * 
 *		vf_done
 * 
 * Deletes vfil_t object
 * 
 * Parameters : 
 * 
 *	vfp - in, take
 *		pointer to vilt_t structure
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void vf_done(
        vfil_t *vfp)
{
        ss_dassert(vfp != NULL);
        su_vfh_done(vfp->vf_hp);
        SsMemFree(vfp);
}

/*#**********************************************************************\
 * 
 *		su_svf_cmp
 * 
 * Compares file block address and a file address range.
 * Used as a comparison function for su_bsearch()
 * 
 * Parameters : 
 * 
 *	addrp - in, use
 *		pointer to block address
 *
 *	vfpp - in, use
 *		pointer^2 to vfil_t
 *
 * Return value : < 0 if adress is below file's start address,
 *                == 0 if address is in file range
 *                > 0 if address is above file range
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static int su_svf_cmp(
        const void *addrp,
        const void *vfpp)
{
        su_daddr_t start;
        vfil_t *vfp;
        
        vfp = *(vfil_t**)vfpp;
        start = vfp->vf_startdaddr;
        if (*(su_daddr_t*)addrp < start) {
            return -1;
        }
        if (*(su_daddr_t*)addrp >= start + vfp->vf_maxszblks) {
            return 1;
        }
        return 0;
}

/*#**********************************************************************\
 * 
 *		su_svf_findvfil
 * 
 * Find appropriate file for file address
 * 
 * Parameters : 
 * 
 *	svfp - in, use
 *		pointer to su_svfil_t structure
 *
 *	addr - in
 *		file block address
 *
 *	retidx - out
 *		pointer to index of file in svf_vfp_arr
 *
 *
 * Return value : SU_SUCCESS if file found
 *                SU_ERR_FILE_WRITE_CFG_EXCEEDED if not in range
 *                SU_ERR_FILE_WRITE_DISK_FULL if disk is full
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_ret_t su_svf_findvfil(
        su_svfil_t *svfp,
        su_daddr_t addr,
        uint *retidx)
{
        vfil_t **vfpp;
        vfil_t *vfp;
        bool rcb;

        ss_dprintf_1(("su_svf_findvfil(svfp=0x%08lX,addr=%ld)\n",
                      (ulong)(void*)svfp,
                      (long)addr));
        rcb = su_bsearch(&addr,
                         svfp->svf_vfp_arr,
                         svfp->svf_vfp_arrsz,
                         sizeof(vfil_t*),
                         su_svf_cmp,
                         (void**)&vfpp);
        *retidx = (uint)(vfpp - svfp->svf_vfp_arr);

        if (rcb) {
            return SU_SUCCESS;
        }
        if (*retidx > 0) { /* previous file exists */
            vfp = *(vfpp-1);
            /* maxszblks adjusted before ? */
            if (vfp->vf_startdaddr + vfp->vf_maxszblks > addr) {
                ss_dprintf_2(("su_svf_findvfil: "
                              "vfp->vf_startdaddr=%ld, "
                              "vfp->vf_maxszblks=%ld\n",
                              (long)vfp->vf_startdaddr,
                              (long)vfp->vf_maxszblks));
                return SU_ERR_FILE_WRITE_DISK_FULL;
            }
            ss_dprintf_2(("su_svf_findvfil: config exceeded, "
                              "vfp->vf_startdaddr=%ld, "
                              "vfp->vf_maxszblks=%ld\n",
                              (long)vfp->vf_startdaddr,
                              (long)vfp->vf_maxszblks));
        } else {
            ss_dprintf_2(("su_svf_findvfil: *retidx=%d\n",
                          *retidx));
        }
                          

        /* We ran out of configured file space ! */
        return SU_ERR_FILE_WRITE_CFG_EXCEEDED;
}

/*#**********************************************************************\
 * 
 *		su_svf_extendszlocal
 * 
 * Extends file to desired size
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	sz - in
 *		new size in blocks
 *
 * 
 * Return value : SU_SUCCESS if OK or
 *                SU_ERR_FILE_WRITE_DISK_FULL if failed.
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_ret_t su_svf_extendszlocal(
        su_svfil_t *svfp,
        su_daddr_t sz)
{
        SsBFileT *bfilep;       /* binary file 'handle' */
        su_pfilh_t *pfhp;
        uint f_idx;             /* file's index in array */
        vfil_t *vfp;            /* current virtual file pointer */
        su_ret_t rc;            /* return code */
        long newszblks;        /* new desired size for file */
        long actualszblks;     /* the actual measured size */
        bool rc2;               /* return code, too */
        su_daddr_t loc2;        /* file address */

        ss_dassert(sz > svfp->svf_nrblks);
        rc = SU_SUCCESS;
        while (rc == SU_SUCCESS && svfp->svf_nrblks < sz) {
            loc2 = svfp->svf_nrblks;    /* current file end location */
            rc = su_svf_findvfil(svfp, loc2, &f_idx);    /* find file entry */
            if (rc != SU_SUCCESS) {
                break;
            }
            vfp = svfp->svf_vfp_arr[f_idx];
            if (f_idx >= svfp->svf_vfp_arrused) {
                svfp->svf_vfp_arrused++;    /* take new file into use */
            }
            ss_dassert(f_idx < svfp->svf_vfp_arrused);
            /* check whether the size req. can be satisfied with this file? */
            if (vfp->vf_maxszblks + vfp->vf_startdaddr > sz) {
                newszblks = sz - vfp->vf_startdaddr;
            } else {
                newszblks = vfp->vf_maxszblks;
            }
            bfilep = su_vfh_beginaccess(vfp->vf_hp, &pfhp);
            ss_dassert(bfilep != NULL);
            /* extend file */
            FAKE_CODE(rc2 = TRUE);
            FAKE_CODE_BLOCK(
                    FAKE_SU_EXPANDFAILATSWITCH,
                    static int n_times = 0;
                    if (f_idx > 0) {
                        if (++n_times == 3) {
                            FAKE_SET(FAKE_SU_EXPANDFAILATSWITCH, 0);
                            n_times = 0;
                        }
                        rc2 = FALSE;
                    });
            FAKE_CODE(if (rc2))
            {
                rc2 = SsBChsizePages(bfilep, newszblks, svfp->svf_blksz);
                if (svfp->svf_cipher != NULL) {
                    su_daddr_t i;
                    char *page;

                    page = SsMemAlloc(svfp->svf_blksz);

                    for (i=svfp->svf_nrblks; i < (su_daddr_t)newszblks; i++) {
                        su_profile_timer; 
                        su_profile_start;
                        memset(page, 0, svfp->svf_blksz);
                        svfp->svf_encrypt(svfp->svf_cipher, i, page,
                                          1, (int)svfp->svf_blksz);
                        su_profile_stop("su_svfil: encryption");
                        rc2 = SsBWritePages(bfilep, i-vfp->vf_startdaddr, svfp->svf_blksz, page, 1);
                        if (!rc2) {
                            SsBChsizePages(bfilep, i-vfp->vf_startdaddr, svfp->svf_blksz);
                        }
                    }
                    SsMemFree(page);
                }
            }
            if (!rc2) {             /* error! propably disk full */
                actualszblks = SsBSizePages(bfilep, svfp->svf_blksz);
                vfp->vf_maxszblks = actualszblks;
                vfp->vf_szblks = actualszblks;
                su_svf_updateranges(svfp, f_idx + 1);
                if (actualszblks == 0) {
                    /* special case: file is zero sized, remove from
                     * array!
                     */
                    uint i;
                    
                    su_vfh_endaccess(vfp->vf_hp, pfhp);
                    bfilep = NULL;
                    vf_done(vfp);
                    vfp = NULL;
                    for (i = f_idx + 1; i < svfp->svf_vfp_arrsz; i++) {
                        svfp->svf_vfp_arr[i-1] = svfp->svf_vfp_arr[i];
                    }
                    svfp->svf_vfp_arrused--;
                    svfp->svf_vfp_arrsz--;
                }
                if (vfp != NULL) {
                    svfp->svf_nrblks =
                        vfp->vf_startdaddr + vfp->vf_maxszblks;
                }
            } else {
                vfp->vf_szblks = newszblks;
                svfp->svf_nrblks = vfp->vf_startdaddr + newszblks;
            }
            if (bfilep != NULL) {
                su_vfh_endaccess(vfp->vf_hp, pfhp);
            }
        }
        return rc;
}

/*#**********************************************************************\
 * 
 *		su_svf_updateranges
 * 
 * Updates file ranges after file the range of which is adjusted
 * 
 * Parameters : 
 * 
 *	svfp - 
 *		pointer to su_svfil_t structure
 *
 *	startidx - 
 *		array index of 1st file to update
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void su_svf_updateranges(
        su_svfil_t *svfp,
        uint startidx)
{
        uint i;

        for (i = (startidx==0 ? 1 : startidx); i < svfp->svf_vfp_arrsz; i++) {
            svfp->svf_vfp_arr[i]->vf_startdaddr =
                svfp->svf_vfp_arr[i-1]->vf_startdaddr +
                svfp->svf_vfp_arr[i-1]->vf_maxszblks;
        }
}

/*#**********************************************************************\
 * 
 *		su_svf_readlocal
 * 
 * Read from splittable virtual file
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	loc - in
 *		file block address
 *
 *	data - out, use
 *		pointer to data buffers
 *
 *	size - in
 *		size of bytes to transfer
 *                                 
 *	sizeread - out
 *		pointer to size_t objects telling how many bytes read
 *
 *      p_beginaccess_fun - in, use
 *		pointer to beginaccess function
 *          (either su_vfh_beginaccess or
 *          su_vfh_beginaccesspers)
 *
 * Return value : SU_SUCCESS if OK
 *                SU_ERR_FILE_READ_EOF if EOF reached during read
 *                SU_ERR_FILE_READ_ILLEGAL_ADDR attempt to start reading
 *                out of file bounds.
 * 
 * Limitations  : start addresses are at block boundary
 * 
 * Globals used : none
 */
static su_ret_t su_svf_readlocal(
        su_svfil_t *svfp,
        su_daddr_t loc,
        bool crypt_enabled,
        void *data,
        size_t size_bytes,
        size_t *sizeread_bytes,
        SsBFileT *(*p_beginaccess_fun)(su_vfilh_t *vfhp, su_pfilh_t **pp_pfh))
{
        vfil_t *vfp;
        uint f_idx;
        SsBFileT *bfilep;
        su_pfilh_t *pfhp;
        int rc2;
        su_ret_t rc;
        su_daddr_t loc_ofs_blks;
        bool lastfile;

        su_daddr_t size_blks;
        su_daddr_t size2_blks;
        su_daddr_t loc2 = loc;
        char *data2 = data;
        su_daddr_t room_blks;

        ss_dassert(svfp != NULL);
        ss_dassert(data != NULL);
        ss_dassert(sizeread_bytes != NULL);
        ss_dassert((size_bytes % svfp->svf_blksz) == 0);
        size2_blks = size_blks = (su_daddr_t)(size_bytes / svfp->svf_blksz);
        *sizeread_bytes = 0L;
        rc = SU_SUCCESS;
        if (loc2 >= svfp->svf_nrblks) {
            rc = SU_ERR_FILE_READ_ILLEGAL_ADDR;
        }
        while (rc == SU_SUCCESS && size2_blks != 0) {
            /***** MUTEXBEGIN *****/
            SsSemEnter(svfp->svf_mutex);
            rc = su_svf_findvfil(svfp, loc2, &f_idx);
            if (rc != SU_SUCCESS) {
                SsSemExit(svfp->svf_mutex);
                /***** MUTEXEND *******/
                break;
            }
            if (f_idx == svfp->svf_vfp_arrused - 1) {
                lastfile = TRUE;
            } else {
                lastfile = FALSE;
            }
            vfp = svfp->svf_vfp_arr[f_idx];
            SsSemExit(svfp->svf_mutex);
            /***** MUTEXEND *******/
            if (rc != SU_SUCCESS) {
                return rc;
            }
            loc_ofs_blks = loc2 - vfp->vf_startdaddr;
            if (lastfile) {
                room_blks = vfp->vf_szblks - loc_ofs_blks;
            } else {
                room_blks = vfp->vf_maxszblks - loc_ofs_blks;
            }
            if (room_blks > size2_blks) {
                room_blks = size2_blks;
            } 
            bfilep = (*p_beginaccess_fun)(vfp->vf_hp, &pfhp);
            ss_dassert(bfilep != NULL);
            ss_dprintf_1((
                "su_svf_read:f_idx=%d,loc_ofs_blks=%ld,tag=%d,cip=%p\n",
                (int)f_idx,
                (long)loc_ofs_blks,
                (int)((ss_byte_t*)data2)[0],
                svfp->svf_cipher));

            rc2 = SsBReadPages(bfilep,
                               loc_ofs_blks, svfp->svf_blksz,
                               data2, room_blks);
            su_vfh_endaccess(vfp->vf_hp, pfhp);
            if (rc2 == -1) {
                rc = SU_ERR_FILE_READ_FAILURE;
            } else {
                *sizeread_bytes += rc2 * svfp->svf_blksz;
                if (rc2 != (int)room_blks) {
                    rc = SU_ERR_FILE_READ_EOF;
                } else if (room_blks < size2_blks && lastfile) {
                    rc = SU_ERR_FILE_READ_EOF;
                } else {
                    size2_blks = size2_blks - room_blks;

                    if (crypt_enabled && svfp->svf_cipher != NULL) {
                        su_profile_timer;
                        su_profile_start;
                        svfp->svf_decrypt(svfp->svf_cipher, loc,
                                          data2, rc2, (int)svfp->svf_blksz);
                        su_profile_stop("su_svfil: decryption");
                    }

                    data2 += (size_t)room_blks * svfp->svf_blksz;
                    loc2 += room_blks;
                }
            }
        }
        return rc;
}

/*#**********************************************************************\
 * 
 *		su_svf_writelocal
 * 
 * Write to splittable virtual file
 * 
 * Parameters : 
 * 
 *	svfp - in out, use
 *		pointer to su_svfil_t structure
 *
 *	loc - in
 *		file block address
 *
 *	data - in, use
 *		pointer to data to write
 *
 *	size - in
 *		size in bytes to write
 *
 *      p_beginaccess_fun - in, use
 *		pointer to beginaccess function
 *          (either su_vfh_beginaccess() or
 *          su_vfh_beginaccesspers())
 *
 * 
 * Return value : SU_SUCCESS if OK
 *                SU_ERR_FILE_WRITE_FAILURE if write fails in existing
 *                file range
 *                SU_ERR_FILE_WRITE_DISK_FULL if write fails beyond file
 *                range. 
 * 
 * Limitations  : start addresses are at block boundary
 * 
 * Globals used : none
 */
static su_ret_t su_svf_writelocal(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size_bytes,
        SsBFileT *(*p_beginaccess_fun)(su_vfilh_t *vfhp, su_pfilh_t **pp_pfh))
{
        vfil_t *vfp;
        uint f_idx;
        SsBFileT *bfilep;
        su_pfilh_t *pfhp;
        su_ret_t rc;
        bool rc2;
        su_daddr_t loc_ofs_blks;

        char *data2 = data;
        su_daddr_t loc2 = loc;
        size_t size_blks;
        size_t size2_blks;
        su_daddr_t room_blks;
        bool lastfile;
        ss_debug(bool first_round = TRUE;)

        ss_dassert((size_bytes % svfp->svf_blksz) == 0);
        size2_blks = size_blks = size_bytes / svfp->svf_blksz; 
        rc = SU_SUCCESS;
        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);
        if (loc2 + size2_blks > svfp->svf_nrblks)
        {
            rc = su_svf_extendszlocal(
                    svfp,
                    (su_daddr_t)(loc2 + size2_blks));
        }
        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        while (rc == SU_SUCCESS && size2_blks != 0) {
            /***** MUTEXBEGIN *****/
            SsSemEnter(svfp->svf_mutex);
            rc = su_svf_findvfil(svfp, loc2, &f_idx);
            if (rc != SU_SUCCESS) {
                SsSemExit(svfp->svf_mutex);
                /***** MUTEXEND *******/
                break;
            }
            if (f_idx == svfp->svf_vfp_arrused - 1) {
                lastfile = TRUE;
            } else {
                lastfile = FALSE;
            }
            vfp = svfp->svf_vfp_arr[f_idx];
            SsSemExit(svfp->svf_mutex);
            /***** MUTEXEND *******/

            loc_ofs_blks = loc2 - vfp->vf_startdaddr;
            room_blks = vfp->vf_szblks - loc_ofs_blks;
            if (room_blks > size2_blks) {
                room_blks = (su_daddr_t)size2_blks;
            } 
            bfilep = (*p_beginaccess_fun)(vfp->vf_hp, &pfhp);
            ss_dassert(bfilep != NULL);
            ss_debug(
                    if (first_round) {
                        ss_dprintf_1((
                        "su_svf_write:f_idx=%d,fileblockpos=%ld,tag=%d,cip=%p\n",
                        (int)f_idx,
                        (long)loc_ofs_blks,
                        (int)((ss_byte_t*)data2)[0],
                        svfp->svf_cipher));
                        first_round = FALSE;
                    });

            if (svfp->svf_cipher != NULL) {
                char *crypt_data;
                size_t crypt_data_size = room_blks*svfp->svf_blksz;
                su_profile_timer;
                su_profile_start;

                crypt_data = SsMemAlloc(crypt_data_size);
                memcpy(crypt_data, data2, crypt_data_size);
                svfp->svf_encrypt(svfp->svf_cipher, loc,
                                  crypt_data, room_blks, (int)svfp->svf_blksz);

                su_profile_stop("su_svfil: encryption");

                ss_dprintf_1(("su_svf_write: encrypt %d bytes\n",
                               crypt_data_size));

                rc2 = SsBWritePages(bfilep, loc_ofs_blks, svfp->svf_blksz,
                                    crypt_data, room_blks);
                SsMemFree(crypt_data);
            } else {
                rc2 = SsBWritePages(bfilep, loc_ofs_blks, svfp->svf_blksz,
                                   data2, room_blks);
            }
            su_vfh_endaccess(vfp->vf_hp, pfhp);
            if (rc2) {
                size2_blks -= room_blks;
                data2 += room_blks * svfp->svf_blksz;
                loc2 += room_blks;
                SsInt8AddUint4(&svfp->svf_nbytes_written,
                               svfp->svf_nbytes_written,
                               (ss_uint4_t)(room_blks * svfp->svf_blksz));
            } else {
                rc = SU_ERR_FILE_WRITE_FAILURE;
            }
        }
        return rc;
}


static vfil_t* su_svf_findvfilp(su_svfil_t* svfp, su_daddr_t daddr)
{
        uint f_idx;
        vfil_t* p_vfil;
        su_ret_t rc;

        rc = su_svf_findvfil(svfp, daddr, &f_idx);
        if (rc != SU_SUCCESS) {
            return (NULL);
        }
        ss_dassert(svfp->svf_vfp_arrsz > f_idx);
        p_vfil = svfp->svf_vfp_arr[f_idx];
        return (p_vfil);
}


/*##**********************************************************************\
 * 
 *          su_svf_getphysfilenamewithrange
 * 
 * Gets physical file name of svfil and disk address
 * 
 * Parameters : 
 * 
 *      svfp - in, use
 *		
 *      daddr - in
 *
 *      p_filespecno - out, use
 *          pointer to variable where the file spec number
 *          (index starting from 1) will be stored
 *
 *      p_blocks_left - out,use
 *          pointer to variable where number of blocks left
 *          starting from daddr in this physical file.
 *
 * Return value - ref:
 *      pointer to file name string or NULL when daddr is not valid
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* su_svf_getphysfilenamewithrange(
        su_svfil_t* svfp,
        su_daddr_t daddr,
        int* p_filespecno,
        su_daddr_t* p_blocks_left)
{
        vfil_t* p_vfil;
        char* fname;
        su_daddr_t range_left;
        int filespecno;
        su_ret_t rc;
        uint f_idx;

        ss_dassert(p_filespecno != NULL);
        ss_dassert(p_blocks_left != NULL);

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        rc = su_svf_findvfil(svfp, daddr, &f_idx);
        if (rc != SU_SUCCESS) {
            filespecno = -1;
            range_left = 0;
            fname = NULL;
        } else {
            ss_dassert(svfp->svf_vfp_arrsz > f_idx);
            p_vfil = svfp->svf_vfp_arr[f_idx];
            filespecno = f_idx + 1;
            ss_dassert(daddr >= p_vfil->vf_startdaddr);
            range_left = p_vfil->vf_maxszblks -
                (daddr - p_vfil->vf_startdaddr);
            ss_dassert(range_left >= 1);
            fname = su_vfh_getfilename(p_vfil->vf_hp);
        }
        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/
        
        *p_filespecno = filespecno;
        *p_blocks_left = range_left;
        return (fname);
}

/*##**********************************************************************\
 * 
 *		su_svf_getphysfilename
 * 
 * Gets physical file name of svfil and disk address
 * 
 * Parameters : 
 * 
 *	svfp - in, use
 *		
 *		
 *	daddr - in
 *		
 *		
 * Return value - ref:
 *      pointer to file name string or NULL when daddr is not valid
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* su_svf_getphysfilename(
        su_svfil_t* svfp,
        su_daddr_t daddr)
{
        char* fname;
        int dummy_filespecno;
        su_daddr_t dummy_blocks_left;

        fname = su_svf_getphysfilenamewithrange(svfp,
                                                daddr,
                                                &dummy_filespecno,
                                                &dummy_blocks_left);
        return (fname);
}

/*##**********************************************************************\
 * 
 *		su_svf_getdiskno
 * 
 * Gets physical disk number from svfil and disk address
 * 
 * Parameters : 
 * 
 *	svfp - in, use
 *		
 *		
 *	daddr - in
 *		
 *		
 * Return value :
 *      disk number (>= 0) or
 *      -1 when daddr is not valid
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int su_svf_getdiskno(
        su_svfil_t* svfp,
        su_daddr_t daddr)
{
        int result;
        vfil_t* p_vfil;

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        p_vfil = su_svf_findvfilp(svfp, daddr);
        if (p_vfil == NULL) {
            SsSemExit(svfp->svf_mutex);
            /***** MUTEXEND *******/
            return (-1);
        }
        result = (int)p_vfil->vf_diskno;

        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        return result;
}

/*##**********************************************************************\
 * 
 *		su_svf_getfilespecno_and_physdaddr
 * 
 * Returns file spec number (starting from 1) and block address in the 
 * underlying physical file.
 * 
 * Parameters : 
 * 
 *	svfp - use
 *		splitted virtual file object
 *		
 *	daddr - in
 *		disk block address for the file
 *		
 *	filespecno - out
 *      number 1 .. maxfile of file specification where daddr resides, or
 *      -1 when daddr is out of range.
 *		
 *	physdaddr - out
 *		disk block address for the physical file
 *		
 * Return value :
 *      TRUE if address was found, FALSE otherwise
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_svf_getfilespecno_and_physdaddr(
        su_svfil_t* svfp,
        su_daddr_t daddr,
        int *filespecno,
        su_daddr_t *physdaddr)
{
        uint f_idx;
        su_ret_t rc;
        vfil_t* vfil;
        bool b;

        ss_dassert(filespecno != NULL);
        ss_dassert(physdaddr != NULL);

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        rc = su_svf_findvfil(svfp, daddr, &f_idx);
        if (rc != SU_SUCCESS) {
            *filespecno = -1;
            *physdaddr = 0;
            b = FALSE;
        } else {
            ss_dassert(svfp->svf_vfp_arrsz > f_idx);
            vfil = svfp->svf_vfp_arr[f_idx];
            *filespecno = f_idx + 1;
            ss_dassert(daddr >= vfil->vf_startdaddr);
            *physdaddr = daddr - vfil->vf_startdaddr;
            b = TRUE;
        }

        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        return (b);
}

/*##**********************************************************************\
 * 
 *		su_svf_getfilespecno
 * 
 * Returns file spec number starting from 1.
 * 
 * 
 * Parameters : 
 * 
 *	svfp - use
 *		splitted virtual file object
 *		
 *	daddr - in
 *		disk block address for the file
 *		
 * Return value :
 *      number 1 .. maxfile or
 *      -1 when daddr is out of range.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int su_svf_getfilespecno(
        su_svfil_t* svfp,
        su_daddr_t daddr)
{
        int result;
        uint f_idx;
        su_ret_t rc;

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        rc = su_svf_findvfil(svfp, daddr, &f_idx);
        if (rc != SU_SUCCESS) {
            SsSemExit(svfp->svf_mutex);
            /***** MUTEXEND *******/
            return (-1);
        }
        result = (int)f_idx + 1;

        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        return result;
}

/*##**********************************************************************\
 * 
 *		su_svf_isreadonly
 * 
 * Checks if file is read only
 * 
 * Parameters : 
 * 
 *	svfp - in use
 *		
 *		
 * Return value :
 *      FALSE if read-write
 *      TRUE (or non-FALSE if read only)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_svf_isreadonly(su_svfil_t* svfp)
{
        return (svfp->svf_flags & SS_BF_READONLY);
}

/*##**********************************************************************\
 * 
 *		su_svf_removelastfile
 *
 *  Removes last physical file from the virtual file (if the file is not
 *  in use)
 *
 * 
 * Parameters : 
 * 
 *		
 * Return value : 
 *
 *      SU_SUCCESS if OK i.e. last physical file removed from virtual file.
 *      SU_ERR_FILE_REMOVE_FAILURE if last physical file cannot be removed.
 *                                 File already in use.
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_ret_t su_svf_removelastfile(su_svfil_t* svfp)
{
        uint f_arrsz;
        uint f_arrused;
        vfil_t* vfil;
        char *tmp, *filename;

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        /* Check if array is empty */
        f_arrsz = (uint)svfp->svf_vfp_arrsz;
        if (f_arrsz == 0) {
            SsSemExit(svfp->svf_mutex);
            return SU_ERR_FILE_REMOVE_FAILURE;
        }

        /* Check if last file is already in use */
        f_arrused = (uint)svfp->svf_vfp_arrused;
        if (f_arrsz == f_arrused) {
            SsSemExit(svfp->svf_mutex);
            return SU_ERR_FILE_REMOVE_FAILURE;
        }
        vfil = svfp->svf_vfp_arr[f_arrsz - 1];
        ss_dassert(vfil != NULL);

        svfp->svf_vfp_arr[f_arrsz - 1] = NULL;
        svfp->svf_vfp_arrsz--;

        tmp = su_vfh_getfilename(vfil->vf_hp);
        filename = SsMemStrdup(tmp);

        vf_done(vfil);

        if (filename != NULL) {
            SsFRemove(filename);
        }

        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        return SU_SUCCESS;
}

void su_svf_fileusageinfo(
        su_svfil_t* svfp,
        double* maxsize,
        double* currsize,
        float* totalperc,
        uint nth,
        float* vfilperc)
{
        su_daddr_t maxblocks;
        su_daddr_t usedblocks;
        double maxbytes;
        double usedbytes;
        vfil_t* vfil;

        usedblocks = su_svf_getsize(svfp);
        maxblocks = su_svf_getmaxsize(svfp);

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        if (maxblocks == 0) {
            if (maxsize != NULL) {
                *maxsize = 0;
            }
            if (currsize != NULL) {
                *currsize = 0;
            }
            if (totalperc != NULL) {
                *totalperc = 0.0;
            }
            if (vfilperc != NULL) {
                *vfilperc = 0.0;
            }
        } else {
            maxbytes = ((double)maxblocks) * ((double)svfp->svf_blksz);
            usedbytes = ((double)usedblocks) * ((double)svfp->svf_blksz);

            if (maxsize != NULL) {
                *maxsize = maxbytes / ((double)(1024 * 1024));
            }
            if (currsize != NULL) {
                *currsize = usedbytes / ((double)(1024 * 1024));
            }
            if (totalperc != NULL) {
                *totalperc = ((usedblocks / (float)maxblocks) * 100);
            }
            if (vfilperc != NULL && nth > 0 && nth <= svfp->svf_vfp_arrused) {
                vfil = svfp->svf_vfp_arr[nth - 1];
                usedblocks = (su_daddr_t)vfil->vf_szblks;
                *vfilperc = ((usedblocks / (float)vfil->vf_maxszblks) * 100);
            } else {
                if (vfilperc != NULL) {
                    *vfilperc = 0.0;
                }
            }
        }
        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/
}


/*##**********************************************************************\
 * 
 *		su_svf_filenameinuse
 * 
 * Checks if given physical filename is already in use
 * 
 * Parameters : 
 * 
 *	svfp - 
 *		
 *		
 *	pathname - 
 *		
 *		
 * Return value : 
 *
 *      SU_SUCCESS - filename is not in use
 *      SU_ERR_DUPLICATE_FILENAME  - filename is already in use
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_ret_t su_svf_filenameinuse(su_svfil_t* svfp, char* pathname)
{

        uint f_idx;         /* index of file in svf_vfp_arr */
        uint i;

        /***** MUTEXBEGIN *****/
        SsSemEnter(svfp->svf_mutex);

        f_idx = (uint)svfp->svf_vfp_arrsz;
        for (i = 0; i < f_idx; i++) {
            if (strcmp(pathname,
                    su_vfh_getfilename(svfp->svf_vfp_arr[i]->vf_hp)) == 0)
            {
                SsSemExit(svfp->svf_mutex);
                /***** MUTEXEND *******/
                return SU_ERR_DUPLICATE_FILENAME;
            }
        }
        SsSemExit(svfp->svf_mutex);
        /***** MUTEXEND *******/

        return SU_SUCCESS;
}

ss_int8_t su_svf_getnbyteswritten(
        su_svfil_t*     svfp)
{
        return svfp->svf_nbytes_written;
}

void su_svf_zeronbyteswritten(
        su_svfil_t*     svfp)
{
        SsInt8Set0(&svfp->svf_nbytes_written);
}

#ifdef MME_CP_FIX

su_ret_t su_svf_listio(
        su_svfil_t* svf,
        su_svf_lioreq_t req_array[],
        size_t nreq)
{
        su_ret_t rc = 0;
        vfil_t *vfp;
        uint f_idx;             /* file index in svf->svf_vfp_arr */
        size_t req_idx = 0;
        SsLIOReqT lioreq_preallocbuf[SSFILE_LIO_PREALLOCSIZE];
        SsLIOReqT* lioreq_array = lioreq_preallocbuf;
        char *write_preallocbuffers[SSFILE_LIO_PREALLOCSIZE];
        char **write_buffers = write_preallocbuffers;

        size_t lioreq_n_alloc =
            sizeof(lioreq_preallocbuf) /
            sizeof(lioreq_preallocbuf[0]);

        for (req_idx = 0;
             req_idx < nreq;
             req_idx++)
        {
            uint n_in_batch = 0;

            /***** MUTEXBEGIN *****/
            SsSemEnter(svf->svf_mutex);
            rc = su_svf_findvfil(svf,
                                 req_array[req_idx].lr_daddr,
                                 &f_idx);
            for (;;) {
                vfp = svf->svf_vfp_arr[f_idx];
                if (n_in_batch >= lioreq_n_alloc) {
                    ss_dassert(n_in_batch == lioreq_n_alloc);
                    lioreq_n_alloc *= 2;
                    if (lioreq_array != lioreq_preallocbuf) {
                        lioreq_array = SsMemRealloc(lioreq_array,
                                                    lioreq_n_alloc *
                                                    sizeof(lioreq_array[0]));
                        if (svf->svf_cipher != NULL) {
                            write_buffers = SsMemRealloc(write_buffers,
                                                         lioreq_n_alloc *
                                                         sizeof(char*));
                        }
                    } else {
                        lioreq_array = SsMemAlloc(lioreq_n_alloc *
                                                  sizeof(lioreq_array[0]));
                        memcpy(lioreq_array,
                               lioreq_preallocbuf,
                               sizeof(lioreq_preallocbuf));

                        if (svf->svf_cipher != NULL) {
                            write_buffers = SsMemAlloc(lioreq_n_alloc *
                                                       sizeof(char*));
                            memcpy(write_buffers,
                                   write_preallocbuffers,
                                   sizeof(write_preallocbuffers));
                        }
                    }
                } 
                lioreq_array[n_in_batch].lio_reqtype =
                    req_array[req_idx].lr_reqtype;
                lioreq_array[n_in_batch].lio_pageaddr =
                    req_array[req_idx].lr_daddr -
                    vfp->vf_startdaddr;
                lioreq_array[n_in_batch].lio_pagesize =
                    svf->svf_blksz;
                lioreq_array[n_in_batch].lio_data =
                    req_array[req_idx].lr_data;
                lioreq_array[n_in_batch].lio_npages =
                    req_array[req_idx].lr_size / svf->svf_blksz;
                lioreq_array[n_in_batch].lio_naterr = 0;

                if (svf->svf_cipher != NULL) {
                    write_buffers[n_in_batch] = NULL;

                    if (lioreq_array[n_in_batch].lio_reqtype == SS_LIO_WRITE) {
                        su_profile_timer;
                        su_profile_start;

                        write_buffers[n_in_batch] =
                            SsMemAlloc(lioreq_array[n_in_batch].lio_pagesize);
                        memcpy(write_buffers[n_in_batch],
                               lioreq_array[n_in_batch].lio_data,
                               lioreq_array[n_in_batch].lio_pagesize);
                        svf->svf_encrypt(svf->svf_cipher,
                                         req_array[req_idx].lr_daddr,
                                         write_buffers[n_in_batch],
                                         1,
                                         (int)lioreq_array[n_in_batch].lio_pagesize);
                        lioreq_array[n_in_batch].lio_data = 
                                               write_buffers[n_in_batch];
                        su_profile_stop("su_svfil: encryption");
                    }
                }

                SsInt8AddUint4(
                        &svf->svf_nbytes_written,
                        svf->svf_nbytes_written,
                        (ss_uint4_t)req_array[req_idx].lr_size);

                ss_dassert(n_in_batch < lioreq_n_alloc);
                ss_dassert((lioreq_array == lioreq_preallocbuf &&
                            lioreq_n_alloc == SSFILE_LIO_PREALLOCSIZE) ||
                           (lioreq_array != lioreq_preallocbuf &&
                            lioreq_n_alloc > SSFILE_LIO_PREALLOCSIZE));

                n_in_batch++;

                if (req_idx + 1 < nreq) {
                    uint next_f_idx;
                    req_idx++;
                    rc = su_svf_findvfil(svf,
                                         req_array[req_idx].lr_daddr,
                                         &next_f_idx);
                    if (next_f_idx != f_idx) {
                        req_idx--;
                        break;
                    } else {
                        f_idx = next_f_idx;
                    }
                } else {
                    break;
                }

            }
            SsSemExit(svf->svf_mutex);
            /***** MUTEXEND *******/
            if (n_in_batch != 0) {
                size_t i;
                SsBFileT* bfile;
                su_pfilh_t *pfhp;

                bfile = su_vfh_beginaccess(vfp->vf_hp, &pfhp);
                for (i = 0; i < n_in_batch; i++) {
                    lioreq_array[i].lio_bfile = bfile;
                }
                SsFileListIO(lioreq_array, n_in_batch);
                su_vfh_endaccess(vfp->vf_hp, pfhp);
            }

            if (svf->svf_cipher != NULL) {
                uint i;
                for (i=0; i < n_in_batch; i++) {
                    if (write_buffers[i] != NULL) {
                        SsMemFree(write_buffers[i]);
                    }
                    if (lioreq_array[i].lio_reqtype == SS_LIO_READ) {
                        su_profile_timer;
                        su_profile_start;
                        svf->svf_decrypt(svf->svf_cipher,
                                          lioreq_array[i].lio_pageaddr+vfp->vf_startdaddr,
                                          lioreq_array[i].lio_data,
                                          1,
                                          (int)lioreq_array[i].lio_pagesize);
                        su_profile_stop("su_svfil: decryption");
                    }
                }
            }
        }
            
        if (lioreq_array != lioreq_preallocbuf) {
            SsMemFree(lioreq_array);
        }

        if (write_buffers != write_preallocbuffers) {
            SsMemFree(write_buffers);
        }

        return (rc);
}

#endif /* MME_CP_FIX */

/*##**********************************************************************\
 *
 *      su_svf_setcipher
 *
 * Sets chiper to be used to encrypt and decrypt the file.
 *
 * Parameters :
 *
 *  svfp -
 *
 *  cipher -
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
void su_svf_setcipher(
        su_svfil_t* svf,
        void* cipher,
        char *(SS_CALLBACK *encrypt)(void *cipher, su_daddr_t daddr, char *page,
                         int npages, size_t pagesize),
        bool (SS_CALLBACK *decrypt)(void *cipher, su_daddr_t daddr, char *page,
                        int npages, size_t pagesize))
{
        ss_dprintf_1(("su_svf_setcipher: %p %p\n", svf, cipher));

        if (cipher != NULL) {
            ss_assert(encrypt != NULL);
            ss_assert(encrypt != NULL);
        }
        svf->svf_cipher = cipher;
        svf->svf_encrypt = encrypt;
        svf->svf_decrypt = decrypt;
}

void *su_svf_getcipher(su_svfil_t* svf)
{
        return svf->svf_cipher;
}

/*##**********************************************************************\
 *
 *      su_svf_encryptall
 *
 * Sets chiper to be used to encrypt and decrypt the file.
 *
 * Parameters :
 *
 *  svfp -
 *
 *  cipher -
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
su_ret_t su_svf_encryptall(
        su_svfil_t*  svfp)
{
        return su_svf_crypt_whole_file(svfp, NULL);
}

su_ret_t su_svf_decryptall(
        su_svfil_t*  svfp,
        su_cipher_t* old_cipher)
{
        return su_svf_crypt_whole_file(svfp, old_cipher);
}

static su_ret_t su_svf_crypt_whole_file(
        su_svfil_t*  svfp,
        su_cipher_t* old_cipher)
{
        su_daddr_t i;
        su_ret_t   rc = 0;
        size_t     size = svfp->svf_blksz;
        char*      data = SsMemAlloc(size);

#ifndef SS_MYSQL
        ss_dassert(svfp->svf_encrypt == svfc_encrypt_dbfile);
        ss_dassert(svfp->svf_decrypt == svfc_decrypt_dbfile);
#endif /* !SS_MYSQL */

        for (i=2; i < svfp->svf_nrblks; i++) {
            su_daddr_t  loc;
            SsBFileT*   bfilep;
            su_pfilh_t* pfhp;
            uint        f_idx;
            vfil_t*     vfp;
            int         rc2;
            int         j;

            SsSemEnter(svfp->svf_mutex);
            rc = su_svf_findvfil(svfp, i, &f_idx);
            if (rc != SU_SUCCESS) {
                SsSemExit(svfp->svf_mutex);
                break;
            }
            vfp = svfp->svf_vfp_arr[f_idx];
            SsSemExit(svfp->svf_mutex);

            bfilep = su_vfh_beginaccess(vfp->vf_hp, &pfhp);
            ss_dassert(i>=vfp->vf_startdaddr);
            loc = i - vfp->vf_startdaddr;

            rc2 = SsBReadPages(bfilep, loc, size, data, 1);
            if (rc2 == -1) {
                rc = SU_ERR_FILE_READ_FAILURE;
            }
            ss_dassert(rc2==1);
            ss_dprintf_1(("su_svf_encryptall: page = %d\n", (int)loc));
            for (j=0; j<20; j++) {
                ss_dprintf_1(("%d ", data[j]));
            }
            ss_dprintf_1(("\n"));
 
            if (old_cipher != NULL) {
                su_cipher_decrypt_page(old_cipher, data, size);
                ss_dprintf_1(("su_svf_encryptall: after decrypt:\n"));
                for (j=0; j<20; j++) {
                    ss_dprintf_1(("%d ", data[j]));
                }
                ss_dprintf_1(("\n"));
            }

            if (svfp->svf_cipher != NULL) {
                su_cipher_encrypt_page((su_cipher_t*)svfp->svf_cipher,
                                        data, size);
                ss_dprintf_1(("su_svf_encryptall: after encrypt:\n"));
                for (j=0; j<20; j++) {
                    ss_dprintf_1(("%d ", data[j]));
                }
                ss_dprintf_1(("\n"));
            }

            rc2 = SsBWritePages(bfilep, loc, size, data, 1);
            if (rc2 == -1) {
                rc = SU_ERR_FILE_WRITE_FAILURE;
            }
            ss_dassert(rc2==1);
            su_vfh_endaccess(vfp->vf_hp, pfhp);
        }

        SsMemFree(data);

        return rc;
}
