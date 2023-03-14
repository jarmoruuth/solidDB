/*************************************************************************\
**  source       * ssfilwnt.c
**  directory    * ss
**  description  * File handling routines for Windows NT.
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

#if defined(SS_NT)

#include "sswindow.h"

#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssstring.h"

#include "ssc.h"
#include "ssfile.h"
#include "ssmem.h"
#include "ssdebug.h"
#include "ssthread.h"
#include "sspmon.h"

#include "ssutf.h"

#ifdef SS_MYSQL
#include <su0error.h>
#include <su0prof.h>
#else
#include "../su/su0error.h"
#include "../su/su0prof.h"
#endif

#define SSFILE_MAXRETRIES       4
#define SSFILE_RETRYSLEEPTIME   1000L    /* in millisecons */

struct SsBFileStruct {
        HANDLE  fd;
        int     err;
        char*   pathname;
        uint    flags;
        size_t  blocksize;  /* in bytes */
        size_t  size;       /* in bytes */
        char*   buffer;
};


/*#*#*********************************************************************\
 * 
 *		SsBErrno
 * 
 * Returns the systems errno-value associated with last operation of this
 * file.
 * 
 * Parameters :
 * 
 *      bfile - in, use
 *		SsBFileT pointer
 *
 * Return value : 
 * 
 *      errno value
 * 
 * Limitations  :
 * 
 * Globals used :
 */
int SsBErrno(bfile)
SsBFileT *bfile;
{
        return(bfile->err);
}

static ss_int8_t SsBSizeAsInt8(
        SsBFileT *bfile);

static bool SsBLockInt8(
        SsBFileT *bfile,
        ss_int8_t rangestart,
        ss_int8_t rangelength,
        uint flags)
{
        ss_int8_t tmp_i8;
        bool succp;

        SS_PMON_ADD(SS_PMON_FILELOCK);

        /* no-op for diskless */
        if (bfile->flags & SS_BF_DISKLESS) {
            return TRUE;
        }

        SsInt8SetInt4(&tmp_i8, -1);
        if (SsInt8Equal(tmp_i8, rangelength)) {
            /* The whole file is locked */
            SsInt8Set0(&rangestart);
            rangelength = SsBSizeAsInt8(bfile);
        }

        if (flags != SSF_UNLOCK) {
            /* Lock file region. */
            succp = LockFile(bfile->fd,
                             SsInt8GetLeastSignificantUint4(rangestart),
                             SsInt8GetMostSignificantUint4(rangestart),
                             SsInt8GetLeastSignificantUint4(rangelength),
                             SsInt8GetMostSignificantUint4(rangelength));
        } else {
            /* Unlock file region. */
            succp = UnlockFile(bfile->fd,
                               SsInt8GetLeastSignificantUint4(rangestart),
                               SsInt8GetMostSignificantUint4(rangestart),
                               SsInt8GetLeastSignificantUint4(rangelength),
                               SsInt8GetMostSignificantUint4(rangelength));
        }
        if (succp) {
            bfile->err = 0;
        } else {
            bfile->err = GetLastError();
            ss_pprintf_2(("SsBLock failed, error = %d\n", bfile->err));
            SsErrorMessage(FIL_MSG_SSBLOCK_FAILED_SD, bfile->pathname, bfile->err);
        }
        return(succp);
}

/*#*#*********************************************************************\
 * 
 *		SsBLock
 * 
 * Locks a file range.
 * 
 * Parameters : 
 * 
 *	bfile - in, use
 *		pointer to binary file descriptor structure
 *
 *	rangestart - in
 *		locked area start location
 *
 *	rangelength - in
 *		locked area length (-1 = all)
 *
 *	flags - in
 *		locking flags (see SSFILE.H)
 *
 * 
 * Return value :
 * 
 *      TRUE - success
 *      FALSE - failure
 * 
 * Limitations  :
 * 
 * Globals used : 
 * 
 *      errno
 */
bool SsBLock(bfile, rangestart, rangelength, flags)
        SsBFileT *bfile;
        long rangestart;
        long rangelength;
        uint flags;
{
        ss_int8_t rangestart_i8;
        ss_int8_t rangelength_i8;
        bool succp;

        if (sizeof(long) == sizeof(ss_int8_t)) {
            SsInt8SetNativeInt8(&rangestart_i8, rangestart);
            SsInt8SetNativeInt8(&rangelength_i8, rangelength);
        } else {
            SsInt8SetUint4(&rangestart_i8, rangestart);
            /* below signed conversion is needed, because value
               -1 has special meaning!
            */
            SsInt8SetInt4(&rangelength_i8, rangelength);
        }
        succp = SsBLockInt8(bfile, rangestart_i8, rangelength_i8, flags);
        return(succp);
}

bool SsBLockPages(
        SsBFileT *bfile,
        ss_uint4_t rangestart,
        size_t pagesize,
        ss_uint4_t rangelengthpages,
        uint flags)
{
        bool    b;
        ss_int8_t pagesize_i8;
        ss_int8_t rangestart_i8;
        ss_int8_t rangelength_i8;

        ss_pprintf_2(("SsBLockPages(\"%.80s\", %ld, %ld, %ld, flags=%u)\n",
                      bfile->pathname, (long)rangestart, (long)pagesize,
                      (long)rangelengthpages, flags));

        SsInt8SetUint4(&pagesize_i8, pagesize);
        if (rangelengthpages == (ss_uint4_t)-1) {
            SsInt8SetInt4(&rangelength_i8, -1);
        } else {
            SsInt8SetUint4(&rangelength_i8, rangelengthpages);
            SsInt8MultiplyByInt8(&rangelength_i8, rangelength_i8, pagesize_i8);
        }
        SsInt8SetUint4(&rangestart_i8, rangestart);
        SsInt8MultiplyByInt8(&rangestart_i8, rangestart_i8, pagesize_i8);
        
        b = SsBLockInt8(
                bfile,
                rangestart_i8,
                rangelength_i8,
                flags);
        return (b);
}

/*#*#*********************************************************************\
 * 
 *		SsBOpen
 * 
 * Opens a file for binary mode access
 * 
 * Parameters : 
 * 
 *	pathname - in, use
 *		file to be opened
 *
 *      flags - in
 *		file open flags
 *
 *      blocksize - in
 *          blocksize (needed for I/O optimization in eg. VMS)
 *
 * Return value : 
 * 
 *      pointer to SsBFileT if successful, NULL otherwise
 * 
 * Limitations  :
 * 
 * Globals used :
 */
SsBFileT *SsBOpen(
        char *pathname,
        uint  flags,
        size_t blocksize)
{
        int fdwAccess;
        int nt_flags = FILE_ATTRIBUTE_ARCHIVE;
        SsBFileT *fp;
        int retries;
        
        ss_pprintf_2(("SsBOpen(\"%.80s\",flags=%d)\n", SS_FILE_MAKENAME(pathname), flags));

        FAKE_RETURN(FAKE_SS_BOPENFAILS, NULL);
        SS_PMON_ADD(SS_PMON_FILEOPEN);

        if (flags & SS_BF_DISKLESS) {
            fp = SsMemAlloc(sizeof(SsBFileT));
            fp->err = 0;
            fp->size = 0;
            fp->pathname = SsMemStrdup( SS_FILE_MAKENAME(pathname) );
            fp->flags = flags;
            fp->blocksize = blocksize;
            fp->buffer = NULL;
            return(fp);
        }
	if (ssfile_diskless) {
	    return NULL;
	}

        fp = SsMemAlloc(sizeof(SsBFileT));
        fp->err = 0;
        fp->flags = flags;
        fp->blocksize = blocksize;

#ifdef IO_OPT
        if ((flags & SS_BF_NOBUFFERING) || (flags & SS_BF_DIRECTIO)) {
#else/* IO_OPT */
        if (flags & SS_BF_NOBUFFERING) {
#endif /* IO_OPT */
            nt_flags |= FILE_FLAG_NO_BUFFERING;
        }
        if (flags & SS_BF_SEQUENTIAL) {
            nt_flags |= FILE_FLAG_SEQUENTIAL_SCAN;
        } else {
            nt_flags |= FILE_FLAG_RANDOM_ACCESS;
        }
        if (flags & SS_BF_WRITEONLY) {
            fdwAccess = GENERIC_WRITE;
        } else if (flags & SS_BF_READONLY) {
            fdwAccess = GENERIC_READ;
        } else {
            fdwAccess = GENERIC_READ | GENERIC_WRITE;
        }
        for (retries = 0; ; retries++) {

            fp->fd = CreateFile(
                        SS_FILE_MAKENAME(pathname),
                        fdwAccess,
                        (flags & SS_BF_EXCLUSIVE)
                            ? 0
                            : (FILE_SHARE_READ | FILE_SHARE_WRITE),
                        NULL,
                        OPEN_ALWAYS,
                        nt_flags,
                        0);
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            if (fp->fd == INVALID_HANDLE_VALUE) {
                ss_pprintf_2(("SsBOpen failed, error = %d, retries = %d\n", GetLastError(), retries));
                SsErrorMessage(FIL_MSG_SSBOPEN_FAILED_SDD, SS_FILE_MAKENAME(pathname), GetLastError(), retries);
                SsThrSleep(SSFILE_RETRYSLEEPTIME);
            } else {
                break;
            }
        }

        if (fp->fd == INVALID_HANDLE_VALUE) {
            SsMemFree(fp);
            return(NULL);
        } else {
            fp->pathname = SsMemStrdup( SS_FILE_MAKENAME(pathname) );
            ss_dprintf_1(("SsBOpen: pathname=\"%.80s\",fd=%ld)\n",
                          fp->pathname,
                          (long)fp->fd))

            return(fp);
        }
}

/*#*#*********************************************************************\
 * 
 *		SsBClose
 * 
 * Closes a file opened by SsBOpen()
 * 
 * Parameters : 
 * 
 *	bfile - in, take
 *		pointer to the SsBFileT structure
 *
 * Return value :
 * 
 * Limitations  :
 * 
 * Globals used :
 */
void SsBClose(bfile)
SsBFileT *bfile;
{
        ss_dassert(bfile != NULL);
        ss_pprintf_2(("SsBClose(\"%.80s\")\n", bfile->pathname));

        if (bfile->flags & SS_BF_DISKLESS) {
            if (bfile->buffer) {
                SsMemFree(bfile->buffer);
            }
            SsMemFree(bfile->pathname);
            SsMemFree(bfile);
            return;
        }

        CloseHandle(bfile->fd);
        SsMemFree(bfile->pathname);
        SsMemFree(bfile);
}

/*#*#*********************************************************************\
 * 
 *		SsBRead
 * 
 * Reads from a file opened by SsBOpen()
 * 
 * Parameters : 
 * 
 *      bfile - in, use
 *		SsBFileT pointer
 *
 *      loc - in
 *		offset in file
 *
 *      buf - out, use
 *		pointer to buffer
 *
 *      buf_s - in
 *		buffer size
 *
 * Return value :
 * 
 *      number of bytes actually read, or (size_t)-1 on error.
 *      (use SsBErrno(bfile) to get error code).
 * 
 * Limitations  : 
 * 
 *      maximum size of buffer is UINT_MAX - 1 due to limitations
 *      in the read() system call.
 *
 * Globals used : 
 * 
 */
size_t SsBRead(bfile, loc, buf, buf_s)
SsBFileT *bfile;
long loc;
void *buf;
size_t buf_s;
{
        int read_cnt;
        int retv;
        int retries;
        bool succp;
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        ss_dassert(buf_s != (size_t)-1);
        ss_pprintf_2(("SsBRead(\"%.80s\",loc=%lu,bsiz=%u) begin\n", bfile->pathname, loc, buf_s));
        ss_dassert(SsSemStkNotFound(SS_SEMNUM_DBE_IOMGR));

        FAKE_RETURN(FAKE_SS_BREADFAILS, (size_t)-1);
        SS_PMON_ADD(SS_PMON_FILEREAD);

        /* for diskless, read is not allow */
        if (bfile->flags & SS_BF_DISKLESS) {
            if (bfile->size < loc + buf_s) {
                return -1;
            }
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                ss_dassert(0);
                return -1;
            }
            memcpy(buf, bfile->buffer + loc, buf_s);
            return buf_s;
        }

        for (retries = 0; ; retries++) {
            if (SetFilePointer(bfile->fd, loc, 0, FILE_BEGIN) != -1) {
                succp = ReadFile(bfile->fd, buf, buf_s, &read_cnt, NULL);
                if (!succp) {
                    retv = -1;
                    bfile->err = GetLastError();
                    ss_pprintf_2(("SsBRead:%d:error %d\n", __LINE__, bfile->err));
                    SsErrorMessage(FIL_MSG_FILEREAD_FAILED_DSDD, 
                        bfile->err, bfile->pathname, loc, retries);
                } else {
                    retv = read_cnt;
                    bfile->err = 0;
                    break;
                }
            } else {
                bfile->err = GetLastError();
                ss_pprintf_2(("SsBRead:%d:error %d\n", __LINE__, bfile->err));
                retv = -1;
                SsErrorMessage(FIL_MSG_FILESEEK_FAILED_DSDD, 
                    bfile->err, bfile->pathname, loc, retries);
            }
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
        }
        su_profile_stop("SsBRead");
        ss_pprintf_3(("SsBRead(\"%.80s\",loc=%lu,bsiz=%u) end\n", bfile->pathname, loc, buf_s));
        return (size_t)retv;
}

/*##**********************************************************************\
 *
 *      SsBReadPages
 *
 * Read 1 or more pages of data from file at specified page address.
 *
 * Parameters:
 *      bfile - in, use
 *          bfile object
 *
 *      pageaddr - in
 *          page address
 *
 *      pagesize - in
 *          page size in bytes
 *
 *      buf - out, use
 *          buffer big enough to store npages * pagesize bytes
 *
 *      npages - in
 *          number of pages to read
 *
 * Return value:
 *      number of pages read or -1 on error
 *
 * Limitations:
 *
 * Globals used:
 */
int SsBReadPages(
        SsBFileT* bfile,
        ss_uint4_t pageaddr,
        size_t pagesize,
        void* buf,
        size_t npages)
{
        int retv;
        int retries;
        SS_NATIVE_UINT8_T fpos;
        size_t buf_s;
        ss_uint4_t hi32;
        su_profile_timer;

        su_profile_start;
        
#ifndef SS_LARGEFILE
        ss_error;
#endif /* SS_LARGEFILE */
        fpos = (SS_NATIVE_UINT8_T)pageaddr * pagesize;
        buf_s = pagesize * npages;
        FAKE_RETURN(FAKE_SS_BREADFAILS, (size_t)-1);
        SS_PMON_ADD(SS_PMON_FILEREAD);
        ss_dassert(SsSemStkNotFound(SS_SEMNUM_DBE_IOMGR));

        ss_dassert(bfile != NULL);

        /* for diskless, the server never do a read */
        if (bfile->flags & SS_BF_DISKLESS) {
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                /* should never come here */
                ss_dassert(0);
                return -1;
            }
            if (bfile->size < fpos + buf_s) {
                return -1;
            }
            memcpy(buf, bfile->buffer + fpos, buf_s);
            return npages;
        }

        ss_pprintf_2(("SsBReadPages(\"%.80s\",pageaddr=%lu,bsiz=%u) begin\n", bfile->pathname, (ulong)pageaddr, buf_s));
        for (retries = 0; ; retries++) {
            hi32 = (ss_uint4_t)(fpos >> 32);
            if (SetFilePointer(bfile->fd,
                               (ss_uint4_t)fpos,
                               &hi32,
                               FILE_BEGIN) != -1)
            {
                DWORD read_cnt;
                bool succp;
                succp = ReadFile(bfile->fd, buf, buf_s, &read_cnt, NULL);
                if (succp) {
                    retv = npages;
                    bfile->err = 0;
                    break;
                } else {
                    bfile->err = GetLastError();
                    ss_pprintf_2(("SsBReadPages:%d:error %d\n",
                                  __LINE__, bfile->err));
                }
            } else {
                bfile->err = GetLastError();
                ss_pprintf_2(("SsBReadPages:%d:error %d\n",
                              __LINE__, bfile->err));
            }
            retv = -1;
            SsErrorMessage(FIL_MSG_FILEREADPAGE_FAILED_DSDDDD,
                           bfile->err,
                           bfile->pathname,
                           (uint)npages,
                           (uint)pagesize,
                           (ulong)pageaddr,
                           retries);
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
        }
        su_profile_stop("SsBReadPages");
        ss_pprintf_3(("SsBReadPages(\"%.80s\",pageaddr=%lu,bsiz=%u) end\n", bfile->pathname, (ulong)pageaddr, buf_s));
        return (size_t)retv;
}

bool SsBWritePages(
        SsBFileT *bfile,
        ss_uint4_t pageaddr,
        size_t pagesize,
        void* data,
        size_t npages)
{
        bool rc;
        int retries;
        DWORD nwritten;
        SS_NATIVE_UINT8_T fpos;
        size_t data_s;
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        ss_pprintf_2(("SsBWritePages(\"%.80s\",daddr=%lu,npages=%u,1stbyte=%d) begin\n",
                      bfile->pathname, (ulong)pageaddr, (uint)npages, (int)((ss_byte_t*)data)[0]));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));
        ss_dassert(SsSemStkNotFound(SS_SEMNUM_DBE_IOMGR));

#ifndef SS_LARGEFILE
        ss_error;
#endif /* SS_LARGEFILE */
        fpos = (SS_NATIVE_UINT8_T)pageaddr * pagesize;
        data_s = pagesize * npages;
        FAKE_RETURN(FAKE_SS_BWRITEFAILS, FALSE);
        SS_PMON_ADD(SS_PMON_FILEWRITE);

        /* for diskless, write is a no-op. change the size if needed. */
        if (bfile->flags & SS_BF_DISKLESS) {
            if (fpos + data_s > bfile->size) {
                SsBExpand(bfile, (long)(fpos + data_s));
            }
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                return TRUE;
            }
            memcpy(bfile->buffer + fpos, data, data_s);
            return TRUE;
        }
        rc = FALSE;
        for (retries = 0; ; retries++) {
            ss_uint4_t hi32 = (ss_uint4_t)(fpos >> 32);
            if (SetFilePointer(bfile->fd,
                               (ss_uint4_t)fpos,
                               &hi32,
                               FILE_BEGIN) != -1)
            {
                rc = WriteFile(bfile->fd, data, data_s, &nwritten, NULL);
                if (rc) {
                    ss_dassert(nwritten = data_s);
                    bfile->err = 0;
                    break;
                } else {
                    bfile->err = GetLastError();
                    ss_pprintf_2(("SsBWritePages:%d:error %d\n",
                                  __LINE__, bfile->err));
                }
            } else {
                bfile->err = GetLastError();
                ss_pprintf_2(("SsBWritePages:%d:error %d\n",
                              __LINE__, bfile->err));
            }
            ss_dassert(!rc);
            SsErrorMessage(FIL_MSG_FILEWRITEPAGE_FAILED_DSDDDD,
                           bfile->err,
                           bfile->pathname,
                           (uint)npages,
                           (uint)pagesize,
                           (ulong)pageaddr,
                           retries);
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
        }
        su_profile_stop("SsBWritePages");
        ss_pprintf_3(("SsBWritePages(\"%.80s\",daddr=%lu,npages=%u,1stbyte=%d) end\n",
                      bfile->pathname, (ulong)pageaddr, (uint)npages, (int)((ss_byte_t*)data)[0]));
        return (rc);
}


/*#*#*********************************************************************\
 * 
 *		SsBWrite
 * 
 * Writes to a file opened by SsBOpen()
 * 
 * Parameters : 
 * 
 *      bfile - in, use
 *		SsBFileT pointer
 *
 *      loc - in
 *		offset in file
 *
 *      buf - in, use
 *		pointer to buffer
 *
 *      buf_s - in
 *		buffer size
 *
 * Return value : 
 * 
 *      TRUE if successful, FALSE otherwise
 * 
 * Limitations  : 
 * 
 *      in 16-bit systems max. data size may be 65534 bytes
 * 
 * Globals used : 
 * 
 */
bool SsBWrite(bfile, loc, data, data_s)
SsBFileT *bfile;
long loc;
void *data;
size_t data_s;
{
        bool rc;
        int retries;
        long nwrite;
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        ss_dassert(data_s != (size_t)-1);
        ss_pprintf_2(("SsBWrite(\"%.80s\",loc=%lu,bsiz=%u) begin\n", bfile->pathname, loc, data_s));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));
        ss_dassert(SsSemStkNotFound(SS_SEMNUM_DBE_IOMGR));

        FAKE_RETURN(FAKE_SS_BWRITEFAILS, FALSE);
        SS_PMON_ADD(SS_PMON_FILEWRITE);

        /* for diskless, write is a no-op. change the size if needed. */
        if (bfile->flags & SS_BF_DISKLESS) {
            if (loc + data_s > bfile->size) {
                SsBExpand(bfile, loc + data_s);
            }
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                return TRUE;
            }
            memcpy(bfile->buffer + loc, data, data_s);
            return TRUE;
        }

        for (retries = 0; ; retries++) {
            if (SetFilePointer(bfile->fd, loc, 0, FILE_BEGIN) == -1) {
                bfile->err = GetLastError();
                ss_pprintf_2(("SsBWrite:%d:error %d\n", __LINE__, bfile->err));
                rc = FALSE;
                SsErrorMessage(FIL_MSG_FILESEEK_FAILED_DSDD, 
                    bfile->err, bfile->pathname, loc, retries);
            } else {
                rc = WriteFile(bfile->fd, data, data_s, &nwrite, NULL);
                if (rc) {
                    ss_dassert(nwrite == data_s);
                    bfile->err = 0;
                    break;
                } else {
                    bfile->err = GetLastError();
                    ss_pprintf_2(("SsBWrite:%d:error %d\n", __LINE__, bfile->err));
                    SsErrorMessage(FIL_MSG_FILEWRITE_FAILED_DSDD, 
                        bfile->err, bfile->pathname, loc, retries);
                }
            }
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
        }
        su_profile_stop("SsBWrite");
        ss_pprintf_3(("SsBWrite(\"%.80s\",loc=%lu,bsiz=%u) end\n", bfile->pathname, loc, data_s));
        return (rc);
}

/*#*#*********************************************************************\
 * 
 *		SsBAppend
 * 
 * Append data to file
 * 
 * Parameters : 
 * 
 *	bfile - in, use
 *		pointer to SsBFileT structure
 *
 *	data - in, use
 *		pointer to data buffer
 *
 *	data_s - in
 *		size of data buffer
 *
 * Return value : 
 * 
 *      TRUE when successful, FALSE otherwise
 * 
 * Limitations  : 
 * 
 *      in 16-bit systems max. data size may be 65534 bytes
 * 
 * Globals used : 
 * 
 */
bool SsBAppend(
        SsBFileT *bfile,
        void *data,
        size_t data_s)
{
        bool rc;
        int retries;
        int nwrite;
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        ss_dassert(data_s != (size_t)-1);
        ss_pprintf_2(("SsBAppend(\"%.80s\",bsiz=%u)\n", bfile->pathname, data_s));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));
        SS_PMON_ADD(SS_PMON_FILEAPPEND);
        ss_dassert(SsSemStkNotFound(SS_SEMNUM_DBE_IOMGR));

        FAKE_RETURN(FAKE_SS_BAPPFAILS, FALSE);
        FAKE_RET_RESET(FAKE_SS_BAPPRESET, FALSE);

        if (bfile->flags & SS_BF_DISKLESS) {
            return SsBWrite(bfile, bfile->size, data, data_s);
        }

        for (retries = 0; ; retries++) {
            if (SetFilePointer(bfile->fd, 0, 0, FILE_END) == -1) {
                bfile->err = GetLastError();
                ss_pprintf_2(("SsBAppend:%d:error %d\n", __LINE__, bfile->err));
                rc = FALSE;
                SsErrorMessage(FIL_MSG_FILESEEKEND_FAILED_DSD, 
                    bfile->err, bfile->pathname, retries);
            } else {
                rc = WriteFile(bfile->fd, data, data_s, &nwrite, NULL);
                if (rc) {
                    bfile->err = 0;
                    break;
                } else {
                    bfile->err = GetLastError();
                    ss_pprintf_2(("SsBAppend:%d:error %d\n", __LINE__, bfile->err));
                    SsErrorMessage(FIL_MSG_FILEAPPEND_FAILED_DSD,
                        bfile->err, bfile->pathname, retries);
                }
            }
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
        }
        su_profile_stop("SsBAppend");
        return (rc);
}

/*#*#*********************************************************************\
 * 
 *		SsBFlush
 * 
 * Flushes memory buffers of open file to permanent storage (disk).
 * 
 * Parameters : 
 * 
 *	bfile - in, use
 *		pointer to SsBFileT
 * 
 * Return value : 
 *                
 *      TRUE when OK
 *      FALSE on error
 * 
 * Limitations  :
 * 
 * Globals used : 
 * 
 */
bool SsBFlush(bfile)
        SsBFileT *bfile;
{
        bool b;
        su_profile_timer;

        su_profile_start;

        ss_pprintf_2(("SsBFlush(\"%.80s\") begin\n", bfile->pathname));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));
        SS_PMON_ADD(SS_PMON_FILEFLUSH);
        ss_dassert(SsSemStkNotFound(SS_SEMNUM_DBE_IOMGR));

        /* for diskless, flush is a no-op. */
        if (bfile->flags & SS_BF_DISKLESS) {
            return TRUE;
        }
        if (bfile->flags & SS_BF_NOFLUSH) {
            return TRUE;
        }

        b = FlushFileBuffers(bfile->fd);

        if (b) {
            bfile->err = 0;
        } else {
            bfile->err = GetLastError();
            ss_pprintf_2(("SsBFlush:%d:error %d\n", __LINE__, bfile->err));
            SsErrorMessage(FIL_MSG_FILEFLUSH_FAILED_DS,
                bfile->err, bfile->pathname);
        }
        su_profile_stop("SsBFlush");
        ss_pprintf_3(("SsBFlush(\"%.80s\") end\n", bfile->pathname));

        return(b);
}

/*#***********************************************************************\
 * 
 *		SsBSizeAsInt8
 * 
 * Tells the size of a file opened by SsBOpen()
 * 
 * Parameters : 
 * 
 *	bfile - in, use
 *		SsBFileT pointer
 *
 * Return value : 
 * 
 *      size of the file
 * 
 * Limitations  :
 * 
 * Globals used :
 */
static ss_int8_t SsBSizeAsInt8(
        SsBFileT *bfile)
{
        ss_int8_t size_i8;
        DWORD       dw_Size_low32;
        DWORD       dw_Size_high32;

        if (bfile->flags & SS_BF_DISKLESS) {
            if (sizeof(bfile->size) == sizeof(size_i8)) {
                SsInt8SetNativeUint8(&size_i8, bfile->size);
            } else {
                SsInt8SetUint4(&size_i8, (ss_uint4_t)bfile->size);
            }
        } else {
            dw_Size_low32 = GetFileSize(bfile->fd, &dw_Size_high32);
            SsInt8Set2Uint4s(&size_i8, dw_Size_high32, dw_Size_low32);
        }
        return(size_i8);
}

/*#*#*********************************************************************\
 * 
 *		SsBSize
 * 
 * Tells the size of a file opened by SsBOpen()
 * 
 * Parameters : 
 * 
 *	bfile - in, use
 *		SsBFileT pointer
 *
 * Return value : 
 * 
 *      size of the file
 * 
 * Limitations  :
 * 
 * Globals used :
 */
long SsBSize(
        SsBFileT *bfile)
{
        long size;
        ss_int8_t size_i8;

        size_i8 = SsBSizeAsInt8(bfile);
        if (sizeof(long) == sizeof(size_i8)) {
            size = (long)SsInt8GetNativeInt8(size_i8);
        } else {
            size = SsInt8GetLeastSignificantUint4(size_i8);
        }
        return (size);
}

ss_uint4_t SsBSizePages(
        SsBFileT* bfile,
        size_t pagesize)
{
        ss_int8_t fsize_i8;
        ss_int8_t pagesize_i8;

        ss_dassert(pagesize != 0);
        SsInt8SetUint4(&pagesize_i8, pagesize);
        fsize_i8 = SsBSizeAsInt8(bfile);
        SsInt8DivideByInt8(&fsize_i8, fsize_i8, pagesize_i8);
        return (SsInt8GetLeastSignificantUint4(fsize_i8));
}

static bool SsBChsizeInt8(
        SsBFileT *bfile,
        ss_int8_t newsize)
{
        bool b;
        long ns;
        ss_uint4_t hi32;
        su_profile_timer;

        su_profile_start;

        ss_dassert(!(bfile->flags & SS_BF_READONLY));

        if (bfile->flags & SS_BF_DISKLESS) {
            if (sizeof(bfile->size) == sizeof(ss_int8_t)) {
                bfile->size = (size_t)SsInt8GetNativeUint8(newsize);
            } else {
                bfile->size = SsInt8GetLeastSignificantUint4(newsize);
            }
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                return TRUE;
            }
            if (bfile->buffer != NULL) { 
                bfile->buffer = SsMemRealloc(bfile->buffer, bfile->size);
            } else {
                bfile->buffer = SsMemAlloc(bfile->size);
            }
            return TRUE;
        }
        if (sizeof(ns) == sizeof(newsize)) {
            ns = (long)SsInt8GetNativeInt8(newsize);
        } else {
            ns = SsInt8GetLeastSignificantUint4(newsize);
        }

        hi32 = SsInt8GetMostSignificantUint4(newsize);
        if (SetFilePointer(bfile->fd,
                           SsInt8GetLeastSignificantUint4(newsize),
                           &hi32,
                           FILE_BEGIN) == -1)
        {
            bfile->err = GetLastError();
            ss_pprintf_2(("SsBChsize:%d:error %d\n", __LINE__, bfile->err));
            ss_dprintf_1(("SsBChsizeInt8 failed: hi32=%ld,ns=%lu,pathname=%s,fd=%ld\n",
                          (long)SsInt8GetMostSignificantUint4(newsize),
                          (ulong)ns,
                          bfile->pathname,
                          bfile->fd));
            SsErrorMessage(FIL_MSG_FILESEEKTONEWSIZE_FAILED_DSD, 
                bfile->err, bfile->pathname, ns);
            return(FALSE);
        }

        b = SetEndOfFile(bfile->fd);

        if (b) {
            bfile->err = 0;
        } else {
            bfile->err = GetLastError();
            ss_pprintf_2(("SsBChsize:%d:error %d\n", __LINE__, bfile->err));
            SsErrorMessage(FIL_MSG_FILESIZECHANGE_FAILED_DSDD, 
                bfile->err, bfile->pathname, ns, 0);
        }
        su_profile_stop("SsBChsize");

        return(b);
}

/*#*#*********************************************************************\
 * 
 *		SsBChsize
 * 
 * Changes (decreases) the file size
 * 
 * Parameters : 
 * 
 *	bfile - in, use
 *		pointer to binary file to be modified
 *
 *	newsize - in
 *		new size of the file
 *
 * Return value : 
 * 
 *      TRUE if successful, FALSE otherwise
 * 
 * Limitations  :
 * 
 * Globals used : 
 * 
 */
bool SsBChsize(
        SsBFileT *bfile,
        long newsize)
{
        ss_int8_t newsize_i8;

        if (sizeof(newsize) == sizeof(newsize_i8)) {
            SsInt8SetNativeInt8(&newsize_i8, newsize);
        } else {
            SsInt8SetUint4(&newsize_i8, newsize);
        }
        return (SsBChsizeInt8(bfile, newsize_i8));
}


bool SsBChsizePages(
        SsBFileT *bfile,
        ss_uint4_t newsizeinpages,
        size_t pagesize)
{
        ss_int8_t pagesize_i8;
        ss_int8_t newsize;

        SsInt8SetUint4(&newsize, newsizeinpages);
        SsInt8SetUint4(&pagesize_i8, pagesize);
        SsInt8MultiplyByInt8(&newsize, newsize, pagesize_i8);
        return (SsBChsizeInt8(bfile, newsize));
}

/*#*#*********************************************************************\
 * 
 *		SsBExpand
 * 
 * Expands the file size to newsize.
 * 
 * Parameters : 
 * 
 *	bfile - 
 *		
 *		
 *	newsize - 
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
bool SsBExpand(bfile, newsize)
        SsBFileT *bfile;
        long newsize;
{
        ss_dassert(bfile != NULL);
        ss_dassert(newsize > SsBSize(bfile));

        ss_dassert(!(bfile->flags & SS_BF_READONLY));
        return(SsBChsize(bfile, newsize));
}


/*##**********************************************************************\
 * 
 *		SsFSizeAsInt8
 * 
 * 
 * 
 * Parameters : 
 * 
 *	pathname - 
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
ss_int8_t SsFSizeAsInt8(char* pathname)
{
        ss_char2_t   *tc_buffer_use;
		CHAR        *utf_string_use;
        int         i_strlen;
        SsUtfRetT   utf_status;

        bool        b_rc;
        DWORD       dw_Size_low32;
        DWORD       dw_Size_high32;
        ss_int8_t   size_i8;
        HANDLE      h_file;
        ss_char2_t  tc_string[512];
                
        b_rc = TRUE;     
        SsInt8Set0(&size_i8);
    
        tc_buffer_use	= tc_string;
		utf_string_use	= SS_FILE_MAKENAME(pathname);
        
        i_strlen = strlen( SS_FILE_MAKENAME(pathname) )+1;
        ss_assert(i_strlen<=512);

        /* Convert to unicode */
        utf_status = SsUTF8toUCS2(
                        &tc_buffer_use,                 /* ss_char2_t** p_dst,*/
                        tc_buffer_use + i_strlen ,     /* ss_char2_t* dst_end */
                        &utf_string_use,                /* ss_byte_t** p_src, */
                        utf_string_use + i_strlen           /* ss_byte_t* src_end */
                        );
        ss_assert( SS_UTF_OK ==utf_status );        

    
           
        /* Open filehandle for query */
        h_file = CreateFileW(
            tc_string, 
            0,                      /* dwDesiredAccess      : Only query */ 
            0,                      /* dwShareMode          : No sharing is needed */ 
            NULL,                   /* lpSecurityAttributes : No security in CE */  
            OPEN_EXISTING,          /* dwCreationDispostion : File allready exists */  
            FILE_ATTRIBUTE_NORMAL,  /* dwFlagsAndAttributes : Normal file */ 
            NULL);                  /* hTemplateFile        : Not used */


        /* General error */
        if( INVALID_HANDLE_VALUE == h_file || 0 == h_file) {
            
            ss_dprintf_1(( "SsFSize / CreateFile failed / INVALID_HANDLE_VALUE / pathname : %s / GetLastError:%d\n", SS_FILE_MAKENAME(pathname), GetLastError() ));    
            b_rc = FALSE;
            goto error_exit;   
        }

        
        dw_Size_low32 = GetFileSize (h_file, &dw_Size_high32) ; 
        if (dw_Size_low32 == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
            b_rc = FALSE;
        } else {
            SsInt8Set2Uint4s(&size_i8, dw_Size_high32, dw_Size_low32);
        }

        /* Now we can free the file handle */            
        CloseHandle( h_file );
        
error_exit:        

        return (size_i8);
}

/*##**********************************************************************\
 * 
 *		SsFSize
 * 
 * 
 * 
 * Parameters : 
 * 
 *	pathname - 
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
long SsFSize(char* pathname)
{
        long size;
        ss_int8_t size_i8;

        size_i8 = SsFSizeAsInt8(pathname);
        if (sizeof(size) == sizeof(size_i8)) {
            size = (long)SsInt8GetNativeUint8(size_i8);
        } else {
            size = (long)SsInt8GetLeastSignificantUint4(size_i8);
        }
        return (size);
}

#endif /* !SS_NT */
