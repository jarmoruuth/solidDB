/*************************************************************************\
**  source       * su0mbsvf.c
**  directory    * su
**  description  * multi-blocksize split virtual file
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

This is a wrapper on top of the su_svfil_t so that this is an array of
one or more of su_svfil_t objects. The blocksize used for addressing
is the minimum of the su_svf_getblocksize():s within that array.
Note: The blocksizes must all be powers of two.

Limitations:
-----------

This has only a subset of the su_svf_XXX service set,
because the usage is limited to the roll-forward log
recovery needs.

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

#include <ssenv.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssfile.h>
#include "su0mbsvf.h"
#include "su0svfil.h"
#include "su1check.h"

/* single blocksize split virtual file */
typedef struct {
        su_daddr_t sf_startpos;
        su_svfil_t* sf_svfil;
} sbsvf_t;


struct su_mbsvfil_st {
        ss_debug(int mf_check;)
        size_t mf_minblocksize; /* used for addressing */
        size_t mf_maxblocksize; /* suitable read/write buffer allocation size */
        size_t mf_lastblocksize;/* latest blocksize in su_mbsvf_addfile */
        size_t mf_num_sbsvf;
        sbsvf_t mf_sbsvf_array[1];
};

#define MBSVF_CHECK(mbsvfil) \
        ss_dassert(SS_CHKPTR(mbsvfil) && (mbsvfil)->mf_check == SUCHK_MBSVFIL)

#define SIZEOF_MBSVFIL(n) \
        ((char*)&((su_mbsvfil_t*)NULL)->mf_sbsvf_array[SS_MAX(1,(n))] -\
         (char*)NULL)
         
/*##**********************************************************************\
 *
 *      su_mbsvf_init
 *
 * Creates a mbsvfil object
 *
 * Parameters:
 *
 * Return value - give:
 *      new multi-blocksize-split-virtual-file object
 *
 * Limitations:
 *
 * Globals used:
 */
su_mbsvfil_t* su_mbsvf_init(void)
{
        su_mbsvfil_t* mbsvfil = SsMemAlloc(SIZEOF_MBSVFIL(0));

        ss_debug(mbsvfil->mf_check = SUCHK_MBSVFIL);
        mbsvfil->mf_minblocksize =
            mbsvfil->mf_maxblocksize =
                mbsvfil->mf_lastblocksize = 0;
        mbsvfil->mf_num_sbsvf = 0;
        MBSVF_CHECK(mbsvfil);
        return (mbsvfil);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_done
 *
 * Frees a mbsvfil
 *
 * Parameters:
 *      mbsvfil - in, take
 *          mbsvfil to free
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void su_mbsvf_done(su_mbsvfil_t* mbsvfil)
{
        size_t i;
        
        MBSVF_CHECK(mbsvfil);
        for (i = mbsvfil->mf_num_sbsvf; i > 0; ) {
            i--;
            su_svf_done(mbsvfil->mf_sbsvf_array[i].sf_svfil);
        }
        SsMemFree(mbsvfil);
}


/*##**********************************************************************\
 *
 *      su_mbsvf_addfile
 *
 * adds a physical file specification to be part of the mbsvfil
 *
 * Parameters:
 *      p_mbsvfil - in out, use/take+give
 *          pointer to pointer to mbsvfil object.
 *          NOTE: the mbsvfil object may be allocated in another location
 *          after this call and thus aliasing that pointer is dangerous!
 *
 *      fname - in, use
 *          file name to add
 *
 *      maxsize - in
 *          maximum size of the file in bytes
 *          (usually the actual size in the intended usage!)
 *
 *      blocksize - in
 *          block (or page) size of the file being added.
 *          It must be a power of 2
 *
 * Return value:
 *      SU_SUCCESS when success, error code otherwise
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t su_mbsvf_addfile(
        su_mbsvfil_t** p_mbsvfil,
        char* fname,
        ss_int8_t maxsize,
        size_t blocksize)
{
        su_ret_t rc;
        su_mbsvfil_t* mbsvfil;
        sbsvf_t* sbsvfil;

        ss_dassert(SS_CHKPTR(p_mbsvfil));
        MBSVF_CHECK(*p_mbsvfil);
        mbsvfil = *p_mbsvfil;
        if (blocksize > mbsvfil->mf_maxblocksize) {
            mbsvfil->mf_maxblocksize = blocksize;
        }
        if (mbsvfil->mf_minblocksize == 0) {
            mbsvfil->mf_minblocksize = blocksize;
        } else if (blocksize < mbsvfil->mf_minblocksize) {
            /* patch start positions of all previous sbsvfs */
            size_t i;
            uint coeff = mbsvfil->mf_minblocksize / blocksize;

            ss_dassert(coeff >= 2 && (coeff & (coeff - 1)) == 0);
            for (i = 0; i < mbsvfil->mf_num_sbsvf; i++) {
                mbsvfil->mf_sbsvf_array[i].sf_startpos *= coeff;
            }
            mbsvfil->mf_minblocksize = blocksize;
        }
        if (mbsvfil->mf_lastblocksize != blocksize) {
            su_daddr_t startpos;
            
            /* need to create a new sbsvf */
            mbsvfil = SsMemRealloc(mbsvfil,
                                   SIZEOF_MBSVFIL(
                                           mbsvfil->mf_num_sbsvf + 1));
            *p_mbsvfil = mbsvfil;
            if (mbsvfil->mf_num_sbsvf != 0) {
                /* calculate start position */
                uint coeff;
                sbsvfil = &mbsvfil->mf_sbsvf_array[mbsvfil->mf_num_sbsvf - 1];
                startpos = su_svf_getsize(sbsvfil->sf_svfil);
                coeff = su_svf_getblocksize(sbsvfil->sf_svfil) /
                    mbsvfil->mf_minblocksize;
                startpos *= coeff;
                startpos += sbsvfil->sf_startpos;
            } else {
                startpos = 0;
            }
            sbsvfil = &mbsvfil->mf_sbsvf_array[mbsvfil->mf_num_sbsvf];
            sbsvfil->sf_svfil = su_svf_init(blocksize, SS_BF_SEQUENTIAL);
            sbsvfil->sf_startpos = startpos;
            mbsvfil->mf_lastblocksize = blocksize;
            mbsvfil->mf_num_sbsvf++;
        } else {
            sbsvfil = &mbsvfil->mf_sbsvf_array[mbsvfil->mf_num_sbsvf - 1];
        }
        rc = su_svf_addfile(sbsvfil->sf_svfil, fname, maxsize, FALSE);
        return (rc);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_getblocksize_at_addr
 *
 * Gets file block size at given address
 *
 * Parameters:
 *      mbsvfil - in, use
 *          mbsvfil object
 *
 *      daddr - in
 *          disk address (unit is minimum block size)
 *
 * Return value:
 *      file block size at the given position
 *
 * Limitations:
 *
 * Globals used:
 */
size_t su_mbsvf_getblocksize_at_addr(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t daddr)
{
        size_t i;
        size_t blocksize = 512;
        
        MBSVF_CHECK(mbsvfil);

        for (i = mbsvfil->mf_num_sbsvf; i > 0; ) {
            i--;
            if (mbsvfil->mf_sbsvf_array[i].sf_startpos <= daddr) {
                blocksize =
                    su_svf_getblocksize(mbsvfil->mf_sbsvf_array[i].sf_svfil);
                break;
            }
            ss_dassert(i > 0);
        }
        return (blocksize);
}
                    
/*##**********************************************************************\
 *
 *      su_mbsvf_read
 *
 * reads data from mnbsvfil
 *
 * Parameters:
 *      mbsvfil - in, use
 *          mbsvfil object
 *
 *      daddr - in
 *          disk address (unit is minblocksize bytes)
 *
 *      buf - out, use
 *          pointer to buffer of at least bufsize bytes
 *
 *      bufsize - in
 *          buffer size to read
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t su_mbsvf_read(su_mbsvfil_t* mbsvfil,
                       su_daddr_t daddr,
                       void* buf,
                       size_t bufsize)
{
        su_ret_t rc = 0;
        size_t i;
        size_t blocksize;
        size_t sizeread;

        MBSVF_CHECK(mbsvfil);
        for (i = mbsvfil->mf_num_sbsvf; i > 0; ) {
            i--;
            if (mbsvfil->mf_sbsvf_array[i].sf_startpos <= daddr) {
                blocksize =
                    su_svf_getblocksize(mbsvfil->mf_sbsvf_array[i].sf_svfil);
                daddr -= mbsvfil->mf_sbsvf_array[i].sf_startpos;
                daddr /= blocksize / mbsvfil->mf_minblocksize;
                rc = su_svf_read(mbsvfil->mf_sbsvf_array[i].sf_svfil,
                                 daddr,
                                 buf, bufsize, &sizeread);
                ss_dassert(rc != SU_SUCCESS || sizeread == bufsize);
                break;
            }
            ss_dassert(i > 0);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_write
 *
 * writes data to mbsvfil
 *
 * Parameters:
 *      mbsvfil - in out, use
 *          multiblocksize svfil object
 *
 *      daddr - in
 *          disk address
 *
 *      buf - in, use
 *          pointer to buffer contaning the data to be written
 *
 *      bufsize - in
 *          size of data
 *
 * Return value:
 *      SU_SUCCESS or error code.
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t su_mbsvf_write(su_mbsvfil_t* mbsvfil,
                        su_daddr_t daddr,
                        void* buf,
                        size_t bufsize)
{
        su_ret_t rc = 0;
        size_t i;

        MBSVF_CHECK(mbsvfil);
        for (i = mbsvfil->mf_num_sbsvf; i > 0; ) {
            i--;
            if (mbsvfil->mf_sbsvf_array[i].sf_startpos <= daddr) {
                size_t blocksize;
                su_daddr_t tmp_daddr = daddr;
                blocksize =
                    su_svf_getblocksize(mbsvfil->mf_sbsvf_array[i].sf_svfil);
                tmp_daddr -= mbsvfil->mf_sbsvf_array[i].sf_startpos;
                tmp_daddr /= blocksize / mbsvfil->mf_minblocksize;
                rc = su_svf_write(mbsvfil->mf_sbsvf_array[i].sf_svfil,
                                  tmp_daddr,
                                  buf, bufsize);
                break;
            }
            ss_dassert(i > 0);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_decreasesize
 *
 * Truncates mvsvfil to smaller size.
 *
 * Parameters:
 *      mbsvfil - in, use
 *          mbsvfil object.
 *
 *      newsize - in
 *          new size (unit=minblocksize)
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t su_mbsvf_decreasesize(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t newsize)
{
        su_ret_t rc = SU_SUCCESS;
        size_t i;
        bool exact_match = FALSE;

        MBSVF_CHECK(mbsvfil);
        for (i = mbsvfil->mf_num_sbsvf; i > 0; ) {
            i--;
            if (mbsvfil->mf_sbsvf_array[i].sf_startpos >= newsize) {
                if (mbsvfil->mf_sbsvf_array[i].sf_startpos == newsize) {
                    exact_match = TRUE;
                }
                rc = su_svf_decreasesize(
                        mbsvfil->mf_sbsvf_array[i].sf_svfil,
                        0);
                if (rc != SU_SUCCESS) {
                    break;
                }
                su_svf_done(mbsvfil->mf_sbsvf_array[i].sf_svfil);
                mbsvfil->mf_num_sbsvf--;
                if (exact_match) {
                    break;
                }
            } else {
                su_daddr_t tmp_newsize = newsize -
                    mbsvfil->mf_sbsvf_array[i].sf_startpos;
                tmp_newsize /=
                    su_svf_getblocksize(
                            mbsvfil->mf_sbsvf_array[i].sf_svfil) /
                    mbsvfil->mf_minblocksize;
                rc = su_svf_decreasesize(
                        mbsvfil->mf_sbsvf_array[i].sf_svfil,
                        tmp_newsize);
                break;
            }
            ss_dassert(i > 0)
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_getsize
 *
 * Gets file size (unit = minblocksize)
 *
 * Parameters:
 *      mbsvfil - in, use
 *          mbsfil object
 *
 * Return value:
 *      size (as minblocksizes)
 *
 * Limitations:
 *
 * Globals used:
 */
su_daddr_t su_mbsvf_getsize(su_mbsvfil_t* mbsvfil)
{
        su_daddr_t size;
        sbsvf_t* sbsvf;
        su_svfil_t* svf;
        
        
        MBSVF_CHECK(mbsvfil);
        if (mbsvfil->mf_num_sbsvf == 0) {
            return (0);
        }
        sbsvf = &mbsvfil->mf_sbsvf_array[mbsvfil->mf_num_sbsvf - 1];
        svf = sbsvf->sf_svfil;
        size = su_svf_getsize(svf);
        size *= su_svf_getblocksize(svf) / mbsvfil->mf_minblocksize;
        size += sbsvf->sf_startpos;
        return (size);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_getminblocksize
 *
 * Gets minimum blocksize (addressing unit)
 *
 * Parameters:
 *      mbsvfil - in, use
 *          mbsvfil object
 *
 * Return value:
 *      minblocksize
 *
 * Limitations:
 *
 * Globals used:
 */
size_t su_mbsvf_getminblocksize(su_mbsvfil_t* mbsvfil)
{
        MBSVF_CHECK(mbsvfil);
        return (mbsvfil->mf_minblocksize);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_getmaxblocksize
 *
 * Gets maximum blocksize (for buffer allocation)
 *
 * Parameters:
 *      mbsvfil - in, use
 *          mbsvfil object
 *
 * Return value:
 *      maximum blocksize
 *
 * Limitations:
 *
 * Globals used:
 */
size_t su_mbsvf_getmaxblocksize(su_mbsvfil_t* mbsvfil)
{
        MBSVF_CHECK(mbsvfil);
        return (mbsvfil->mf_maxblocksize);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_getfilespecno_and_physdaddr
 *
 * "Digs" filespec number (1..n) and physical page address in that file
 *
 * Parameters:
 *      mbsvfil - in, use
 *          mbsvfil object
 *
 *      daddr - in
 *          disk address in minblocksizes
 *
 *      filespecno - out, use
 *          file spec number 1..nfiles
 *
 *      physdaddr - out, give
 *          physical block address withing the file and using
 *          the block size of this particular file as unit.
 *
 * Return value:
 *      TRUE - success
 *      FALSE - daddr did not match any file
 *
 * Limitations:
 *
 * Globals used:
 */
bool su_mbsvf_getfilespecno_and_physdaddr(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t daddr,
        int* filespecno,
        su_daddr_t* physdaddr)
{
        bool succp = FALSE;
        size_t i;
        size_t j;

        MBSVF_CHECK(mbsvfil);
        ss_dassert(mbsvfil->mf_num_sbsvf);
        for (i = mbsvfil->mf_num_sbsvf; i > 0; ) {
            i--;
            if (mbsvfil->mf_sbsvf_array[i].sf_startpos <= daddr) {
                int tmp_filespecno;
                su_daddr_t tmp_physdaddr;
                size_t coeff =
                    su_svf_getblocksize(mbsvfil->mf_sbsvf_array[i].sf_svfil) /
                    mbsvfil->mf_minblocksize;

                ss_dassert(coeff >= 1);
                succp = su_svf_getfilespecno_and_physdaddr(
                        mbsvfil->mf_sbsvf_array[i].sf_svfil,
                        (daddr - mbsvfil->mf_sbsvf_array[i].sf_startpos)
                        / coeff,
                        &tmp_filespecno,
                        &tmp_physdaddr);
                if (succp) {
                    for (j = i; j > 0; ) {
                        j--;
                        tmp_filespecno +=
                            su_svf_getfilespecno(
                                mbsvfil->mf_sbsvf_array[j].sf_svfil,
                                su_svf_getsize(
                                        mbsvfil->mf_sbsvf_array[j].sf_svfil)
                                - 1);
                    }
                    *filespecno = tmp_filespecno;
                    *physdaddr = tmp_physdaddr;
                }
                break;
            }
            ss_dassert(i > 0);
        }
        return (succp);
}

/*##**********************************************************************\
 *
 *      su_mbsvf_getphysfilename
 *
 * Gets physical file name at given address
 *
 * Parameters:
 *      mbsvfil - in, use
 *          mbsvfil object
 *
 *      daddr - in
 *          disk address in minpagesizes
 *
 * Return value - ref:
 *      pointer to file name string or NULL when daddr is not valid
 *
 * Limitations:
 *
 * Globals used:
 */
char* su_mbsvf_getphysfilename(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t daddr)
{
        char* fname = NULL;
        size_t i;

        MBSVF_CHECK(mbsvfil);
        for (i = mbsvfil->mf_num_sbsvf; i > 0; ) {
            i--;
            if (mbsvfil->mf_sbsvf_array[i].sf_startpos <= daddr) {
                size_t coeff =
                    su_svf_getblocksize(mbsvfil->mf_sbsvf_array[i].sf_svfil) /
                    mbsvfil->mf_minblocksize;

                ss_dassert(coeff >= 1);
                fname = su_svf_getphysfilename(
                        mbsvfil->mf_sbsvf_array[i].sf_svfil,
                        (daddr - mbsvfil->mf_sbsvf_array[i].sf_startpos) /
                        coeff);
                break;
            }
            ss_dassert(i > 0);
        }
        return (fname);
}

