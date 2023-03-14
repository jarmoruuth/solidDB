/*************************************************************************\
**  source       * ssfile.h
**  directory    * ss
**  description  * File handling functions
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


#ifndef SSFILE_H
#define SSFILE_H


#include "ssenv.h"
#include "ssstddef.h"
#include "sstime.h"
#include "ssc.h"
#include "ssint8.h"

#define SS_FILE	FILE

#include "ssstdio.h"
#include "ssdebug.h"


#ifdef SS_WIN
#  define SS_FILW16_WINIO
#endif /* SS_WIN */

/* SsFlock flags */

#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT)
#include <sys/locking.h>
#define SSF_SHLOCK      LK_LOCK
#define SSF_EXLOCK      LK_LOCK
#define SSF_NBLOCK      LK_NBLCK
#define SSF_UNLOCK      LK_UNLCK

#else /* SS_UNIX */
#include <fcntl.h>
#define SSF_SHLOCK      F_RDLCK
#define SSF_EXLOCK      F_WRLCK
#define SSF_NBLOCK      0
#define SSF_UNLOCK      F_UNLCK
#endif

#ifdef IO_OPT
# define SS_DIRECTIO_ALIGNMENT 512U
#endif /* IO_OPT */

/* SsBOpen flags
 */
#define SS_BF_EXCLUSIVE     1       /* denies all access from other processes */
#define SS_BF_SEQUENTIAL    2       /* file is accessed sequentially */
#define SS_BF_NOBUFFERING   4       /* no buffering is wanted */
#define SS_BF_WRITEONLY     8       /* File is opened for write only */
#define SS_BF_READONLY      16      /* File is opened for read only */
#define SS_BF_FLUSH_BEFOREREAD  32  /* File is flushed before each SsBRead
                                     * if previous operation has been write */
#define SS_BF_FLUSH_AFTERWRITE  64  /* File is flushed after each SsBWrite */
#define SS_BF_DISKLESS      128     /* in-memory file for diskless  */
#define SS_BF_DLSIZEONLY    256     /* only size is maintained for diskless
                                     * in-memory file. Read is not allowed. */
#define SS_BF_NOFLUSH       512     /* SsBFlush is treated as no-op. */
# define SS_BF_SYNCWRITE    1024    /* File is opened for synchronous I/O */

#ifdef IO_OPT
# define SS_BF_DIRECTIO      2048    /* Uses direct I/O instead of fs caching */
#endif /* IO_OPT */


/*

  Note: The BFile FLUSH (except SS_BF_NOFLUSH) options are currently
  supported only in environments where the SSB routines are not separately
  implemented. For now, it is not implemented in NT, O32 and
  W16 although it could be useful in W16.

  Note: Currently SS_BF_NOBUFFERING and SS_BF_SYNCWRITE are two
  seperate flags because database file is opened as non-buffered by
  default, which is unwanted behavior in non-Windows platforms. Syncwrite is
  currently only operational in Solaris platforms where O_DSYNC open flag is
  implemented. It is anticipated that these flags will be combined
  in future.
  Note: SyncWrite is implemented now in AIX, Free BSD, HP-UX, Linux, and
  in Solaris. vraatikka 3/2007

*/

typedef enum bflush_en {
        SS_BFLUSH_NORMAL = 0,
        SS_BFLUSH_BEFOREREAD = 1,  /* same as BOpen flag */
        SS_BFLUSH_AFTERWRITE = 2   /*    --     --   --  */
} SsBFlushTypeT;

/* Physical and default max open files.
 *
 *      SS_MAXFILES_DEFAULT
 *          Recommended default for max open files.
 *
 *      SS_MAXFILES_SYSPHYSICAL
 *          !!! FOR INTERNAL AND TEST USE ONLY !!!
 *          Max number of files that can be open
 *          at one time, physical(?) system limit.
 *          In OS/2 this is not the case, because
 *          can be increased dynamically. Note that
 *          the current value is tested, to increase
 *          it, ss/test/tfile.c test must be run.
 */
#if defined(SS_DOS) || defined(SS_WIN) 
# define SS_MAXFILES_DEFAULT        6
# define SS_MAXFILES_SYSPHYSICAL    18
#elif defined(SS_UNIX) || defined(SS_NT)
  /* Physical: SS_UNIX = 255, SS_NT = ? */
# define SS_MAXFILES_DEFAULT        300
# define SS_MAXFILES_SYSPHYSICAL    500
#elif defined(SS_LINUX) 
# define SS_MAXFILES_DEFAULT        300
# define SS_MAXFILES_SYSPHYSICAL    500
#else
# error Unknown system!
  Hogihogi!
#endif

/* For internal and test use only. */
#define SS_MAXFILES_SYSRESERVED 12
#define SS_MAXFILES_PHYSICAL    (SS_MAXFILES_SYSPHYSICAL - SS_MAXFILES_SYSRESERVED)

#define SsBFileT struct SsBFileStruct

extern bool ssfile_diskless;

/*************************************************************************\
        Global file system synch
\*************************************************************************/

void SsFGlobalFlush(void);

/*************************************************************************\
        Routines that use FILE.
\*************************************************************************/

SS_FILE* SsFOpenB(
        char *pathname,
        char *flags);

SS_FILE* SsFOpenT(
        char *pathname,
        char *flags);

int SsFPutBuf(
        ss_char1_t* s,
        size_t n,
        FILE* fp);

#	define  SsFClose 		fclose
#	define	SsFGetc			fgetc
#	define	SsFPutc			fputc
#	define	SsPutc			putc
#	define	SsGetc			getc
#	define	SsFGets			fgets
#	define	SsFPuts			fputs
#	define	SsFPrintf		fprintf
#	define	SsFRead			fread
#	define	SsFWrite		fwrite
#	define	SsFFlush		fflush
#	define	SsStdout		stdout
#	define	SsStderr		stderr
#	define	SsRewind		rewind
#	define	SsFeof 			feof
#	define	SsFSeek			fseek
#   define  SsFTell         ftell

bool SsFTruncatePossible(
        void);

bool SsFTruncate(
        FILE* fp,
        long newsize);

bool SsFExpand(
        SS_FILE* fp,
        long newsize);

int SsFErrno(
        void);

/*************************************************************************\
        Routines that use file name.
\*************************************************************************/

#ifdef SS_FILEPATHPREFIX

#define SS_FILE_MAKENAME(s)         SsFileNameName(s)
#define SS_FILE_MAKENAME_BUF(s,b)   strcpy(b, SsFileNameName(s))

char* SsFileNameName(char* fname);

#else /* SS_FILEPATHPREFIX */

#define SS_FILE_MAKENAME(s)         s
#define SS_FILE_MAKENAME_BUF(s,b)   s

#endif /* SS_FILEPATHPREFIX */

void SsFileSetPathPrefix(
        char* prefix);

char* SsFileGetPathPrefix(void);

char* SsFTmpname(
        char* prefix_or_null);

bool SsFChsize(
        char *pathname,
        long newsize);

bool SsFExist(
        char* pathname);

bool SsDExist(
        char* pathname);

bool SsMkdir(
        char* dirname);

bool SsFMakeDirsForPath(
        char* fpath);

bool SsChdir(
        char* dir);

char* SsGetcwd(
        char* buf,
        int bufsize);

long SsFSize(
        char* pathname);

ss_int8_t SsFSizeAsInt8(
        char* pathname);

bool SsFRemove(
        char* pathname);

bool SsFRename(
        ss_char_t* oldname,
        ss_char_t* newname);

/*************************************************************************\
        Routines for max open file handle control.
\*************************************************************************/

bool SsFSetMaxOpenAbs(
        int max_open_files);

bool SsFSetMaxOpenRel(
        int max_open_diff,
        int* p_max_open_total);

/*************************************************************************\
        Routines that use SsBFileT.
\*************************************************************************/

SsBFileT *SsBOpen(
        char *pathname,
        uint flags,
        size_t blocksize);

void SsBClose(
        SsBFileT *bfile);

size_t SsBRead(
        SsBFileT *bfile,
        long loc,
        void *buf,
        size_t buf_s);

bool SsBWrite(
        SsBFileT *bfile,
        long loc,
        void *data,
        size_t data_s);

int SsBReadPages(
        SsBFileT* bfile,
        ss_uint4_t pageaddr,
        size_t pagesize,
        void* buf,
        size_t npages);

bool SsBWritePages(
        SsBFileT* bfile,
        ss_uint4_t pageaddr,
        size_t pagesize,
        void* data,
        size_t npages);

bool SsBAppend(
        SsBFileT *bfile,
        void *data,
        size_t data_s);

bool SsBFlush(
        SsBFileT *bfile);

long SsBSize(
        SsBFileT *bfile);

ss_uint4_t SsBSizePages(
        SsBFileT* bfile,
        size_t page_size);

bool SsBChsize(
        SsBFileT *bfile,
        long newsize);

bool SsBExpand(
        SsBFileT *bfile,
        long newsize);

bool SsBChsizePages(
        SsBFileT* bfile,
        ss_uint4_t newsizeinpages,
        size_t pagesize);

bool SsBLock(
        SsBFileT *bfile,
        long rangestart,
        long rangelength,
        uint flags);

bool SsBLockPages(
        SsBFileT *bfile,
        ss_uint4_t rangestart,
        size_t pagesize,
        ss_uint4_t rangelengthpages,
        uint flags);

bool SsBLockRetry(
        SsBFileT*   bfile,
        long        rangestart,
        long        rangelength,
        uint        flags,
        int         retries,
        uint        seconds);
int SsBErrno(
        SsBFileT *bfile);

SsBFlushTypeT SsBSetFlushType(
        SsBFileT *bfile,
        SsBFlushTypeT type);

/*************************************************************************\
        Routines that use file handle.
\*************************************************************************/

#ifdef MME_CP_FIX

#ifdef SS_DEBUG
#define SSFILE_LIO_PREALLOCSIZE 4
#else /* SS_DEBUG */
#define SSFILE_LIO_PREALLOCSIZE 20
#endif /* SS_DEBUG */

typedef enum {
    SS_LIO_WRITE,
    SS_LIO_READ
} SsLIOReqTypeT;

typedef struct {
        SsBFileT* lio_bfile;        /* in/out */
        SsLIOReqTypeT lio_reqtype;  /* SS_LIO_WRITE/SS_LIO_READ */
        ss_uint4_t lio_pageaddr;    /* in */
        size_t lio_pagesize;        /* in */
        void* lio_data;             /* in/out */
        size_t lio_npages;          /* in */
        int lio_naterr;             /* out */
} SsLIOReqT;

bool SsFileListIO(SsLIOReqT req_array[/*nreq*/], size_t nreq);

#endif /* MME_CP_FIX */

#endif /* SSFILE_H */

/* EOF */
