/*************************************************************************\
**  source       * ssfile.c
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

Portable file handling routines.

Limitations:
-----------

None.

Error handling:
--------------

Return codes.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

None.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#ifdef SS_FREEBSD
#include <sys/file.h>
#endif

#include "ssenv.h"

#ifdef IO_OPT /* Direct I/O optimization */
#if defined(SS_LINUX)
#define _GNU_SOURCE
#endif
#endif /* IO_OPT */

#if defined(SS_UNIX)
#include <unistd.h>
#else
#include <direct.h>
#endif

#ifdef SS_FREEBSD
#include <utime.h>
#endif

#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT) || defined(SS_SOLARIS)

#include <sys/types.h>
#include <sys/stat.h>

#endif

#if defined(SS_NATIVE_LISTIO_AVAILABLE) || defined(SS_NATIVE_WRITEV_AVAILABLE)
#include <aio.h>
#endif /* SS_NATIVE_LISTIO_AVAILABLE || SS_NATIVE_WRITEV_AVAILABLE */

#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssstring.h"
#include "sssprint.h"
#include "ssctype.h"
#include "sstime.h"
#include "sspmon.h"
#include "sschcvt.h"
#include "ssfnsplt.h"
#include "ssgetenv.h"

#if defined(SS_UNIX) 
#  include <fcntl.h>
#else
#  include <fcntl.h>
#  include <io.h>
#endif  /* SS_UNIX */

#if defined(WCC)
#  include <dos.h>
#endif

#include <errno.h>

#define SSFILE_MAXRETRIES       4
#define SSFILE_RETRYSLEEPTIME   1000L    /* in millisecons */

#define SSFILE_MODEFLAGS        (S_IREAD | S_IWRITE)

#if defined(SS_LINUX) || defined(SS_SOLARIS)
#define SSFILE_SYNCFLAG		O_DSYNC
#elif defined(SS_FREEBSD)
#define SSFILE_SYNCFLAG     O_FSYNC
#else
#define SSFILE_SYNCFLAG         0
#endif

#ifdef IO_OPT
#if defined(SS_LINUX) || defined(SS_FREEBSD)
#define SSFILE_DIRECTFLAG   O_DIRECT
#else
#define SSFILE_DIRECTFLAG   0
#endif /* defined(SS_LINUX) || defined(SS_FREEBSD) */
#endif /* IO_OPT */


#if defined(ZTC)
#  include <time.h>
#elif defined(BORLANDC) || defined(SS_LINUX)
#  include <utime.h>
#elif defined(SS_UNIX) 
   /* none */
#else
#  include <sys/utime.h>
#endif

#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT)
#include <sys/locking.h>
#endif

# if defined(MSC) || defined(CSET2) || defined(BORLANDC) || defined(MSC_NT) || defined(SS_LINUX) 
# define SSFILE_UTIMBUF_DEFINED
#endif

#define INCL_DOSFILEMGR
#define INCL_DOSERRORS

#ifdef SS_UNIX
#include <sys/file.h>
#include <sys/stat.h>
#endif

#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT)
#include <share.h>
#endif

#if defined(SS_DOS) || defined(SS_WIN)
#include <dos.h>
#endif

#ifdef SS_DOS4GW
#include <i86.h>
#endif

#include "ssc.h"
#include "ssfile.h"
#include "ssmem.h"
#include "ssdebug.h"
#include "ssthread.h"

#ifdef SS_MYSQL
#include <su0error.h>
#include <su0prof.h>
#else
#include "../su/su0error.h"
#include "../su/su0prof.h"
#endif

#define SS_FILE_MAXPATH 512

typedef enum bio_oper_en {
        BOP_READ = 0,
        BOP_WRITE = 1,
        BOP_FLUSH = 2
} bio_op_t;

struct SsBFileStruct {
        int     fd;
        int     err;
        char*   pathname;
        uint    flags;
        size_t  blocksize;  /* in bytes */
        size_t  dlsize;     /* (diskless only) size in bytes */
        SsBFlushTypeT flushtype;
        bio_op_t    lastop;
        char*   buffer;
};

static int file_nopen = 0;
bool   ssfile_diskless = FALSE;

static bool SsBLockLocal(
        SsBFileT*   bfile,
        off_t       rangestart,
        off_t       rangelength,
        uint        flags);

#ifdef SS_FILEPATHPREFIX

char ss_file_pathprefix[SS_FILE_MAXPATH] = "";

char* SsFileNameName(char* fname)
{
        static char buf[SS_FILE_MAXPATH];
        int succ;

        if (fname == NULL){
            return(NULL);
        }

        ss_dprintf_2(("ssfile: SsFileNameName: input file name '%s'\n", fname));

        if (strcmp(ss_file_pathprefix,"") == 0) {
            ss_dprintf_2(("ssfile: SsFileNameName: ss_file_pathprefix is NULL, returning '%s'\n", fname));
            return(fname);
        }

        succ = SsFnMakePath( ss_file_pathprefix, fname, buf, SS_FILE_MAXPATH );
        if (!succ){
            ss_dprintf_2(("ssfile: SsFileNameName: prefix and filename together too long! returning NULL\n"));
        }

        ss_dprintf_2(("ssfile: SsFileNameName: ss_file_pathprefix is '%s', returning '%s'\n",
                      ss_file_pathprefix, buf));

        return(buf);
}

char* SsFileGetPathPrefix(void){
        return(ss_file_pathprefix);
}

void SsFileSetPathPrefix( char* prefix ){

        if (prefix == NULL){
            return;
        }

        strcpy(ss_file_pathprefix, prefix);
}

#else /* SS_FILEPATHPREFIX */

char* SsFileGetPathPrefix(void){
        return("");
}

#endif /* SS_FILEPATHPREFIX */

/*##**********************************************************************\
 *
 *		SsFGlobalFlush
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsFGlobalFlush(void)
{
#if (defined(SS_WIN) || defined(SS_DOS)) && !defined(SS_W32) && !defined(SS_DOS4GW)

#if (MSC==60)
        /* for compatibility */
        union REGS r;
#       define _int86 int86
#else
        union _REGS r;
#endif

#if defined(SS_W16) && defined(WCC)
#  define _int86 int86
#endif /* SS_W16 && WCC */

        ss_pprintf_1(("SsFGlobalFlush() called\n"));
        r.x.ax = 0x4A10;
        r.x.bx = 0x0000;
        r.x.cx = 0xEBAB;
        _int86(0x2F, &r, &r);
        if (r.x.ax != 0xBABE) {
            ss_pprintf_1(("SsFGlobalFlush(): SmartDrv not detected\n"));
            return; /* Smart drive not installed or wrong version */
        }
        r.x.ax = 0x4A10;
        r.x.bx = 0x0001; /* Flush all dirty buffers call */
        _int86(0x2F, &r, &r);
#else /* WIN || DOS */
        ss_pprintf_1(("SsFGlobalFlush() called\n"));
#endif
}

/*##**********************************************************************\
 *
 *		SsFPutBuf
 *
 * Like fputs, but the length is given as a parameter
 *
 * Parameters :
 *
 *	s - in, use
 *		buffer to put
 *
 *	n - in
 *		number of chars in s
 *
 *	fp - use
 *		file stream pointer
 *
 * Return value :
 *      number of characters that have been put or
 *      EOF when failed.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int SsFPutBuf(ss_char1_t* s, size_t n, FILE* fp)
{
        int r;
        size_t i;

        if (n != 0) {
            if (s[n-1] == '\0') {
                n--;
            }
            for (i = n; i > 0; i--, s++) {
                ss_char1_t c = *s;
                r = putc(c, fp);
                if (r == EOF) {
                    return (EOF);
                }
            }
        }
        return (n);
}

/*##**********************************************************************\
 *
 *		SsFOpenB
 *
 * Opens a binary file stream
 *
 * Parameters :
 *
 *	pathname - in, use
 *		file to be opened
 *
 *	flags - in, use
 *		r, w, a, .. as in fopen()
 *
 * Return value - give :
 *
 *      FILE pointer or NULL if unsuccessful
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_FILE *SsFOpenB(pathname, flags)
char *pathname;
char *flags;
{
#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT) 
        /* We'll hope that the flags will never be over 3 chars long.. */
        char tmp[5];

        ss_assert(strlen(flags) < sizeof(tmp) - 2)

        strcpy(tmp, flags);
        strcat(tmp, "b");

        FAKE_RETURN(FAKE_SS_FOPENBFAILS, NULL);
        return(fopen(SS_FILE_MAKENAME(pathname), tmp));
#else
        FAKE_RETURN(FAKE_SS_FOPENBFAILS, NULL);
        return(fopen(SS_FILE_MAKENAME(pathname), flags));
#endif
}


/*##**********************************************************************\
 *
 *		SsFOpenT
 *
 * Opens a text file stream
 *
 * Parameters :
 *
 *	pathname - in, use
 *		file to be opened
 *
 *	flags - in, use
 *		r, w, a, .. as in fopen()
 *
 *
 * Return value - give :
 *
 *      FILE pointer or NULL if unsuccessful
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_FILE *SsFOpenT(pathname, flags)
char *pathname;
char *flags;
{
#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT)
        /* We'll hope that the flags will never be over 3 chars long.. */
        char tmp[5];

        strncpy(tmp, flags, 3);
        strcat(tmp, "t");

        FAKE_RETURN(FAKE_SS_FOPENTFAILS, NULL);
        return(fopen(SS_FILE_MAKENAME(pathname), tmp));
#else
        FAKE_RETURN(FAKE_SS_FOPENTFAILS, NULL);
        return(fopen(SS_FILE_MAKENAME(pathname), flags));
#endif
}

/*##**********************************************************************\
 *
 *		SsFGetModTime
 *
 * Gets a files last modification time.
 *
 * Parameters :
 *
 *      pathname - in, use
 *		file name
 *
 * Return value :
 *
 *      last modification time or -1 on error
 *
 * Limitations  :
 *
 * Globals used :
 */
time_t SsFGetModTime(
        char *pathname)
{
        struct stat s;
        int rc;

        FAKE_RETURN(FAKE_SS_MODTIMEFAILS, (time_t)-1);

        rc = stat(SS_FILE_MAKENAME(pathname), &s);
        ss_dassert(rc == 0);
        if (rc)
            return (time_t)-1L;
        return(s.st_mtime);
}


/*##**********************************************************************\
 *
 *		SsFSetModTime
 *
 * Sets a files modification time
 *
 * Parameters :
 *
 *	pathname - in, use
 *		file to be modified
 *
 *	newtime - in
 *		the new modification time
 *
 * Return value :
 *
 *      TRUE if successful, FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SsFSetModTime(
        char *pathname,
        time_t newtime)
{
        int ret;
        struct stat s;
# ifdef SSFILE_UTIMBUF_DEFINED
        struct utimbuf utb;
        struct utimbuf *times = &utb;
# else
        time_t times[2];
# endif

        ret = stat(SS_FILE_MAKENAME(pathname), &s);
        if (ret) {
            return(FALSE);
        }

# ifdef SSFILE_UTIMBUF_DEFINED
        times->actime = s.st_atime;
        times->modtime = newtime;
# else
        times[0] = s.st_atime;
        times[1] = newtime;
# endif
        if (utime(SS_FILE_MAKENAME(pathname), times)) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}


/*##**********************************************************************\
 *
 *		SsFGetAccTime
 *
 * Gets a files last access time.
 *
 * Parameters :
 *
 *      pathname - in, use
 *		file name
 *
 * Return value :
 *
 *      last access time or -1 on error
 *
 * Limitations  :
 *
 * Globals used :
 */
time_t SsFGetAccTime(
        char *pathname)
{
        struct stat s;
        int rc;

        rc = stat(SS_FILE_MAKENAME(pathname), &s);
        ss_dassert(rc == 0);
        if (rc)
            return (time_t)-1L;
        return(s.st_atime);
}

/*##**********************************************************************\
 *
 *		SsFSetAccTime
 *
 * Sets a files last access time
 *
 * Parameters :
 *
 *	pathname - in, use
 *		file to be modified
 *
 *	newtime - in
 *		the new access time
 *
 * Return value :
 *
 *      TRUE if successful, FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SsFSetAccTime(
        char *pathname,
        time_t newtime)
{
        int ret;
        struct stat s;
# ifdef SSFILE_UTIMBUF_DEFINED
        struct utimbuf utb;
        struct utimbuf *times = &utb;
# else
        time_t times[2];
# endif

        ret = stat(SS_FILE_MAKENAME(pathname), &s);
        if (ret) {
            return(0);
        }

# ifdef SSFILE_UTIMBUF_DEFINED
        times->actime = newtime;
        times->modtime = s.st_mtime;
# else
        times[0] = newtime;
        times[1] = s.st_mtime;
# endif

        if (utime(SS_FILE_MAKENAME(pathname), times)) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *		SsFTmpname
 *
 * Creates a temporary file name and returns it. The user may give
 * a file name prefix. IF prefix is NULL, the function uses default
 * prefix 'ss'.
 *
 * The returned file name must be released with function SsMemFree.
 *
 * Parameters :
 *
 *	prefix_or_null - in
 *		File name prefix, or NULL.
 *
 * Return value - give :
 *
 *      Pointer to the temporary file name. The user must release
 *      the returned pointer with SsMemFree.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* SsFTmpname(char* prefix_or_null)
{
#if defined(WCC) 

        char* fname;

        SS_NOTUSED(prefix_or_null);

        fname = tmpnam(NULL);
        ss_assert(fname != NULL);
        fname = SsMemStrdup(fname);
        return(fname);
#else
#  if !defined(SS_NT)   /* mkstemp() using version */
        char* sysfname;
        char* basepath;
        char* env;
        int fd;
        size_t plen;
        size_t sflen;
        char fname[12]; /* 5 + 6 + 1 = max prefix + XXXXXX + \0 */        

        if (prefix_or_null == NULL) {
            prefix_or_null = (char *)"ss";
        }

        /* Issue:
         *   checked if mkstemp() can be used instead of tempnam(), which
         *   GCC reports as being unsafe.
         *
         * The safety issue is that use of tempnam() allows for snooping or
         * altering of the tempfile by some outsider.  mkstemp() solves this
         * by also opening the file, in exclusive mode.
         *
         * SsFTmpname() however is used to only generate a filename, as we use
         * (at least somewhere) virtual filehandles and mkstemp() opens a real
         * one.  So the solution is to mimic tempnam as closely as possible.
         *
         * tempnam() gets the dir for the file by:
         *   - tries TMPDIR env var
         *   - (using the dir parameter, which we don't use)
         *   - tries P_tmpdir from stdio.h
         *   - implementation dependant: glibc defaults to /tmp if all else
         *     has failed.
         *
         * The lengths one will go to to suppress a warning.
         */

        if ((env = SsGetEnv("TMPDIR")) != NULL && SsDExist(env)) {
            basepath = env;
        } else {
            basepath = P_tmpdir; /* defined in stdio.h */
            if (!SsDExist(basepath)) {
                basepath = "/tmp";
                ss_info_assert(SsDExist(basepath),
                               ("No directory to create temporary files in. %s does not exist.", basepath));
            }
        }

        plen = strlen(prefix_or_null);
        if (plen > 5) {
            plen = 5; /* max prefix length for tempnam() */
        }

        /* possible added separator after basepath, six X's */
        sflen = strlen(basepath) + 1 + plen + 6 + 1;
        sysfname = SsMemAlloc(sflen);
        SsSprintf(fname, "%.*sXXXXXX", plen, prefix_or_null);
#ifdef SS_DEBUG
        {
            int rv = SsFnMakePath(basepath, fname, sysfname, sflen);
            ss_rc_dassert(rv != 0, rv);
        }
#else
        SsFnMakePath(basepath, fname, sysfname, sflen);
#endif

        fd = mkstemp(sysfname);
        ss_info_assert((fd != -1),
                       ("mkstemp() failed with %d: %s",
                        errno, strerror(errno)));

        /* since we only want the name, let's get rid of the created file */
        close(fd);
        SsFRemove(sysfname);

        return sysfname;
#  else /* no mkstemp() */
        char* sysfname;
        char* fname;

        if (prefix_or_null == NULL) {
            prefix_or_null = (char *)"ss";
        }

        sysfname = (char*)tempnam(NULL, prefix_or_null);
        ss_assert(sysfname != NULL);
        fname = SsMemStrdup(sysfname);
        free(sysfname);
        return(fname);
#endif /* mkstemp() supported */
#endif /* non-posix(?) platforms */
}

/*##**********************************************************************\
 *
 *		SsFExist
 *
 * Checks if the given file exists.
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
bool SsFExist(pathname)
char* pathname;
{
        int rc;
        struct stat s;

        rc = stat(SS_FILE_MAKENAME(pathname), &s);

        if (rc == 0 && (s.st_mode & S_IFREG)) {
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *		SsDExist
 *
 * Checks if the given directory exists.
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
bool SsDExist(pathname)
char* pathname;
{
        int rc;
        struct stat s;

        rc = stat(SS_FILE_MAKENAME(pathname), &s);

        if (rc == 0 && (s.st_mode & S_IFDIR)) {
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *		SsChdir
 *
 * Changes the current working directory. In DOS, WIN, OS/2 and NT also
 * changes the drive.
 *
 * Parameters :
 *
 *	dir -
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
bool SsChdir(char* dir)
{
#if defined(SS_UNIX)

        return(chdir(dir) == 0);

#elif defined(SS_DOS) || defined(SS_WIN)||  defined(SS_NT)

        if (chdir(dir) != 0) {
            return(FALSE);
        }

        if (dir[0] != '\0' && dir[1] == ':') {
            /* There is a drive letter. Change the current drive.
             */
            unsigned driveno;
            unsigned curdriveno;

            driveno = ss_toupper(dir[0]) - 'A' + 1;
#  ifdef WCC
            _dos_setdrive(driveno, &curdriveno);
            _dos_getdrive(&curdriveno);
            if (curdriveno != driveno) {
                return(FALSE);
            }
#  else /* WCC */
            if (_chdrive(driveno) != 0) {
                return(FALSE);
            }
#  endif /* WCC */
        }
        return(TRUE);

#else
#error Unknown environment!
XXXX ERROR! XXXX
#endif
}

/*##**********************************************************************\
 *
 *		SsGetcwd
 *
 *
 *
 * Parameters :
 *
 *	buf -
 *
 *
 *	bufsize -
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
char* SsGetcwd(char* buf, int bufsize)
{
        return(getcwd(buf, bufsize));
}

/*##**********************************************************************\
 *
 *		SsFRemove
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
bool SsFRemove(pathname)
    char* pathname;
{
        int rc;

        /* output from here causes an assert in ssmsglog when we are
         * removing the logfile since that's where we'd output the
         * message (forced logsplit does this at server start, caused by
         * at least /SPLIT and -c cmdline option).
         *
         * So I'll comment these out. --mr 20050809
         *
         * we might fix this by just dismissing output into a NULL msglog
         * and not asserting. Or having a buffer in msglog for situations
         * like this..
         *
         */
        rc = remove(SS_FILE_MAKENAME(pathname));

#ifdef SS_DEBUG  
        if (rc != 0) {
            ss_pprintf_2(("SsFRemove: file='%s', error %d: %s\n", pathname, errno, strerror(errno)));
        } else {
            ss_pprintf_2(("SsFRemove: %s removed\n", pathname));
        }
#endif /* SS_DEBUG */

        return(rc == 0);
}

/*##**********************************************************************\
 *
 *		SsFRename
 *
 * Renames a file.
 *
 * Parameters :
 *
 *	oldname -
 *
 *
 *	newname -
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
bool SsFRename(
        ss_char_t* oldname,
        ss_char_t* newname)
{
        int rc;
        char buf_newname[SS_FILE_MAXPATH];
        char buf_oldname[SS_FILE_MAXPATH];

        newname = SS_FILE_MAKENAME_BUF(newname, buf_newname);
        oldname = SS_FILE_MAKENAME_BUF(oldname, buf_oldname);

        rc = rename(oldname, newname);

        return(rc == 0);
}

/*##**********************************************************************\
 *
 *		SsFChsize
 *
 * Changes (decreases) the file size
 *
 * Parameters :
 *
 *	pathname - in, use
 *		file to be modified
 *
 *	newsize - in
 *		new size of the file
 *
 *
 * Return value :
 *
 *      TRUE if successful, FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SsFChsize(pathname, newsize)
char *pathname __attribute__ ((unused));
long newsize __attribute__ ((unused));
{
#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT)
        int fd = open(SS_FILE_MAKENAME(pathname), O_WRONLY);
        int st;

        if (fd == -1) {
            return(FALSE);
        }

        st = chsize(fd, newsize);

        close(fd);

        if (st == 0) {
            return(TRUE);
        } else {
            return(FALSE);
        }
#else
        return(FALSE);
#endif
}


/*##**********************************************************************\
 *
 *		SsFTruncatePossible
 *
 * Tells if a file can be truncated in this system
 *
 * Parameters :
 *
 * Return value :
 *
 *      TRUE or FALSE
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SsFTruncatePossible()
{
#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_UNIX) || defined(SS_NT)
        return(TRUE);
#else
        Error! Unknown environment
#endif
}

/*##**********************************************************************\
 *
 *		SsFTruncate
 *
 * Truncates file.
 *
 * Parameters :
 *
 *	fp -
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
bool SsFTruncate(FILE* fp, long newsize)
{
#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT) || defined(SS_SCO)
        int fd = fileno(fp);
        int st;

        if (fd == -1) {
            return(FALSE);
        }

        st = chsize(fd, newsize);

        if (st == 0) {
            return(TRUE);
        } else {
            return(FALSE);
        }
#elif defined(SS_UNIX)
        int retval;

        retval = ftruncate(fileno(fp), newsize);
        return(retval == 0);
#else
        Error! Unknown environment
#endif
}


/*##**********************************************************************\
 *
 *		SsFExpand
 *
 * Expands file.
 *
 * Parameters :
 *
 *	fp -
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
bool SsFExpand(SS_FILE* fp, long newsize)
{
#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT)

        int fd = fileno(fp);
        int st;

        if (fd == -1) {
            return(FALSE);
        }

        st = chsize(fd, newsize);

        if (st == 0) {
            return(TRUE);
        } else {
            return(FALSE);
        }
#else

        int retval;

        retval = fseek(fp, newsize-1, SEEK_SET);
        if (retval != 0) {
            return(FALSE);
        }
        retval = fwrite("", 1, 1, fp);
        return(retval == 1);
#endif
}


static int maxopen_reserved = 0;
static int maxopen_physical = SS_MAXFILES_PHYSICAL;

/*##**********************************************************************\
 *
 *		SsFSetMaxOpenAbs
 *
 * Sets maximum number of open files to be opened
 *
 * Parameters :
 *
 *	max_open_files - in
 *		max open file limit (excluding stdin, SsStdout, SsStderr)
 *
 * Return value :
 *
 * Comments :
 *
 *      If this function is called from a DLL, it will set the
 *      "max open files" from all the user processes.
 *
 * Globals used :
 *
 * See also :
 */
bool SsFSetMaxOpenAbs(int max_open_files)
{
        ss_dassert(max_open_files >= 0);
        if (max_open_files <= maxopen_physical) {
            maxopen_reserved = max_open_files;
            return (TRUE);
        }
        return (FALSE);
}

/*##**********************************************************************\
 *
 *		SsFSetMaxOpenRel
 *
 * Sets max # of open files limit with relative increment
 *
 * Parameters :
 *
 *	max_open_diff - in
 *		positive value to inrement limit or negative to decrement
 *
 *	p_max_open_total - out
 *          pointer to variable where the total limit will be stored
 *
 *
 * Return value :
 *      TRUE if successful,
 *      FALSE when failed
 *
 * Comments :
 *
 *      If this function is called from a DLL, it will set the
 *      "max open files" from all the user processes.
 *
 * Globals used :
 *
 * See also :
 */
bool SsFSetMaxOpenRel(
        int max_open_diff __attribute__ ((unused)),
        int* p_max_open_total __attribute__ ((unused)))
{
#if defined(SS_WIN) && 0 /* This code is not tested! */

        int handles;
        int max_open_files = maxopen_reserved;

        if (max_open_diff < 0
        ||  maxopen_reserved + max_open_diff <= maxopen_physical)
        {
            maxopen_reserved += max_open_diff;
            if (maxopen_reserved < 0) {
                maxopen_reserved = 0;
            }
            if (p_max_open_total != NULL) {
                *p_max_open_total = maxopen_reserved;
            }
            return (TRUE);
        }
        max_open_files += max_open_diff;
        if (max_open_files < 0) {
            max_open_files = 0;
        }
        handles = SetHandleCount(max_open_files + SS_MAXFILES_SYSRESERVED);
        ss_pprintf_2(("SetHandleCount(%u):handles = %d\n", max_open_files + SS_MAXFILES_SYSRESERVED, handles));
        if (handles < max_open_files + SS_MAXFILES_SYSRESERVED) {
            maxopen_reserved = handles - SS_MAXFILES_SYSRESERVED;
            maxopen_physical = handles - SS_MAXFILES_SYSRESERVED;
            if (p_max_open_total != NULL) {
                *p_max_open_total = handles - SS_MAXFILES_SYSRESERVED;
            }
            return (TRUE);
        }
        if (p_max_open_total != NULL) {
            *p_max_open_total = maxopen_reserved;
        }
        return (FALSE);

#elif 0  
        if (maxopen_reserved + max_open_diff <= maxopen_physical) {
            maxopen_reserved += max_open_diff;
            if (maxopen_reserved < 0) {
                maxopen_reserved = 0;
            }
            if (p_max_open_total != NULL) {
                *p_max_open_total = maxopen_reserved;
            }
            return (TRUE);
        }
        if (p_max_open_total != NULL) {
            *p_max_open_total = maxopen_reserved;
        }
        return (FALSE);
#else
        /* maxopen_reserved doesn't actually mean anything! */
        return TRUE;
#endif 
}

/*##**********************************************************************\
 *
 *		SsFErrno
 *
 * Returns the systems errno-value.
 *
 * Parameters :
 *
 * Return value :
 *
 *      errno value
 *
 * Limitations  :
 *
 * Globals used :
 *
 *      errno
 */
int SsFErrno()
{
        return errno;
}

#ifdef SS_NT
#define SS_MKDIR_SYSCALL(dname) mkdir(dname)
#else /* SS_NT */
#define SS_MKDIR_SYSCALL(dname) mkdir(dname,0777)
#endif /* SS_NT */

bool SsMkdir(char* dirname)
{
        int r;

        r = SS_MKDIR_SYSCALL(dirname);
        ss_dprintf_1(("SsMkdir: mkdir(%s) returns %d, errno = %d\n",
                      dirname, r, SsFErrno()));
        return (r == 0);
}

bool SsFMakeDirsForPath(
        char* fpath)
{
        bool succp;
        size_t fpath_size;
        char* dirname_buf;
        char* fname_buf;

        fpath_size = strlen(fpath) + 1;
        dirname_buf = SsMemAlloc(fpath_size);
        fname_buf = SsMemAlloc(fpath_size);
        succp = SsFnSplitPath(fpath,
                              dirname_buf, fpath_size,
                              fname_buf, fpath_size);
        ss_dassert(succp);
        if (strlen(dirname_buf) == 0) {
            goto ret_TRUE;
        }
        if (SsDExist(dirname_buf)) {
            goto ret_TRUE;
        }
        succp = SsFMakeDirsForPath(dirname_buf);
        if (!succp) {
            goto ret_FALSE;
        }
        succp = SsMkdir(dirname_buf);
        if (succp) {
            goto ret_TRUE;
        }
        goto ret_FALSE;
 ret_FALSE:;
        succp = FALSE;
        goto ret_cleanup;
 ret_TRUE:;
        succp = TRUE;
 ret_cleanup:;
        SsMemFree(dirname_buf);
        SsMemFree(fname_buf);
        return (succp);
}

/*************************************************************************\
 *************************************************************************
 *************************************************************************
 *************************************************************************
 **                                                                     **
 **                                                                     **
 **       SSB ROUTINES THAT MAY HAVE SEPARATE VERSIONS                  **
 **                                                                     **
 **                                                                     **
 *************************************************************************
 *************************************************************************
 *************************************************************************
\*************************************************************************/

#if defined(SS_NT) 
#define SEPARATE_SSB_ROUTINES
#endif

#ifdef SS_FILW16_MMIO
#define SEPARATE_SSB_ROUTINES
#endif

#ifdef SS_FILW16_WINIO
#define SEPARATE_SSB_ROUTINES
#endif

#ifndef SEPARATE_SSB_ROUTINES

static off_t SsFSizeOffT(char* pathname)
{
        int rc;
        struct stat s;

        rc = stat(SS_FILE_MAKENAME(pathname), &s);
        if (rc == 0) {
#ifdef SS_LARGEFILE
            ss_dprintf_2(("SsFSizeOffT(%s) size=0x%08lX%08lX\n",
                          pathname,
                          (ulong)(ss_uint4_t)(s.st_size >> 32),
                          (ulong)(ss_uint4_t)s.st_size));

#else /* SS_LARGEFILE */
            ss_dprintf_2(("SsFSizeOffT(%s) size=0x%08lX\n",
                          pathname,
                          (ulong)s.st_size));

#endif /* SS_LARGEFILE */
            return(s.st_size);
        } else {
            ss_dprintf_2(("SsFSizeOffT(%s) size=0x0 (file does not exist)\n",
                          pathname));
            return(0);
        }
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
long SsFSize(
        char* pathname)
{
        return ((long)SsFSizeOffT(pathname));
}

ss_int8_t SsFSizeAsInt8(
        char* pathname)
{
        off_t size;
        ss_int8_t size_i8;
        size = SsFSizeOffT(pathname);
#ifdef SS_LARGEFILE
        ss_ct_assert(sizeof(size) == 8);
        SsInt8SetNativeUint8(&size_i8, size);
#else /* SS_LARGEFILE */
        ss_ct_assert(sizeof(size) == 4);
        SsInt8SetUint4(&size_i8, size);
#endif /* SS_LARGEFILE */
        return (size_i8);
}

/*##**********************************************************************\
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


bool SsBLockLocalMaybeRetry(
        SsBFileT *bfile,
        off_t rangestart,
        off_t rangelength,
        uint flags)
{
        int     retries;
        bool    b;

        SS_PMON_ADD(SS_PMON_FILELOCK);

        /* for diskless, lock is a no-op */
        if (bfile->flags & SS_BF_DISKLESS) {
            return TRUE;
        }

        for (retries = 0; retries < SSFILE_MAXRETRIES; retries++) {
            b = SsBLockLocal(bfile, rangestart, rangelength, flags);
            if (b) {
                return TRUE;
            }
            ss_pprintf_2(("SsBLock failed, retries = %d\n", retries));
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
        }

        return FALSE;
}

/*##**********************************************************************\
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
bool SsBLock(
        SsBFileT *bfile,
        long rangestart,
        long rangelength,
        uint flags)
{
        bool    b;

        ss_pprintf_2(("SsBLock(\"%.80s\", %ld, %ld, flags=%d)\n",
                      bfile->pathname, rangestart, rangelength, flags));
        b = SsBLockLocalMaybeRetry(bfile,
                                   (off_t)rangestart,
                                   (off_t)rangelength,
                                   flags);
        return (b);
}

bool SsBLockPages(
        SsBFileT *bfile,
        ss_uint4_t rangestart,
        size_t pagesize,
        ss_uint4_t rangelengthpages,
        uint flags)
{
        bool    b;

        ss_pprintf_2(("SsBLockPages(\"%.80s\", %ld, %ld, %ld, flags=%u)\n",
                      bfile->pathname, (long)rangestart, (long)pagesize,
                      (long)rangelengthpages, flags));
        b = SsBLockLocalMaybeRetry(
                bfile,
                (off_t)rangestart * pagesize,
                ((rangelengthpages == (ss_uint4_t)-1) ?
                 (off_t)-1 : (off_t)rangelengthpages * pagesize),
                flags);
        return (b);
}
/*##**********************************************************************\
 *
 *      SsBLockRetry
 *
 * Locks a file range. Retries a specified number of times.
 * Between retries a specified number of seconds are waited.
 *
 * Parameters :
 *
 *  bfile - in, use
 *      pointer to binary file descriptor structure
 *
 *  rangestart - in
 *      locked area start location
 *
 *  rangelength - in
 *      locked area length (-1 = all)
 *
 *  flags - in
 *      locking flags (see SSFILE.H)
 *
 *  retries - in
 *      the maximum number of retries that are performed after the first trial.
 *
 *  seconds - in
 *      the length of the delay between consecutive trials in seconds.
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
bool SsBLockRetry(
        SsBFileT*   bfile,
        long        rangestart,
        long        rangelength,
        uint        flags,
        int         retries, /* if fails, this many retries */
        uint        seconds)
{
        uint    delay = 1000L*seconds;
        bool    b;

        SS_PMON_ADD(SS_PMON_FILELOCK);

        /* for diskless, lock is a no-op */
        if (bfile->flags & SS_BF_DISKLESS) {
            return TRUE;
        }

        do {
            b = SsBLockLocal(
                bfile,
                (off_t) rangestart,
                (off_t) rangelength,
                flags);
            if (b) {
                return TRUE;
            }
            if (retries != 0) {
                ss_pprintf_2( ("SsBLock failed, %d retries left\n", retries) );
                SsThrSleep(delay);
            }
            retries--;
        } while (retries >= 0);

        return FALSE;
}

static bool SsBLockLocal(
        SsBFileT    *bfile,
        off_t       rangestart,
        off_t       rangelength,
        uint        flags)
{
        if (rangelength == -1) {
            /* The whole file is locked */

#if defined(SS_DOS) || defined(SS_WIN) || defined(MSC_NT) || defined(MSC_CE)
            rangelength = filelength(bfile->fd);
#else
            rangelength = 0; /* this locks the whole file in BSD */
#endif
            rangestart = 0;
        }

        {
#if defined(SS_DOS) || defined(SS_WIN)

            /* The locking function locks from the current file position
             * so we must save it before locking.
             */
            long ptr = lseek(bfile->fd, 0L, SEEK_CUR);
            long tmp = lseek(bfile->fd, rangestart, SEEK_SET);
            int st;
            ss_assert(tmp == rangestart);

            st = locking(bfile->fd, flags, rangelength);
            if (st) {
                ss_pprintf_2(("SsBLock failed, errno = %d\n", errno));
                SsErrorMessage(FIL_MSG_SSBLOCK_FAILED_SD, bfile->pathname, errno);
            }

            tmp = lseek(bfile->fd, ptr, SEEK_SET);
            ss_assert(tmp == ptr);

            if (st) {
                return(FALSE);
            } else {
                return(TRUE);
            }
#else /* SS_UNIX */

#ifdef SS_PURIFY
        return TRUE;
#else
            struct flock fl;
            int st;

            fl.l_type = flags;
            fl.l_whence = 0;         /* start relative to start of file */
            fl.l_start = rangestart;
            fl.l_len = rangelength;

            st = fcntl(bfile->fd, F_SETLK, &fl);

            if (st) {
                ss_pprintf_2(("SsBLock failed, errno = %d\n", errno));
                SsErrorMessage(FIL_MSG_SSBLOCK_FAILED_SDD, bfile->pathname, errno, bfile->fd);
                return(FALSE);
            } else {
                return(TRUE);
            }
#endif /* SS_PURIFY */

#endif
        }
}

/*#***********************************************************************\
 *
 *		SsBOpenLocal
 *
 * Locally used open for binary file
 *
 * Parameters :
 *
 *	fp - in out, use
 *		pointer to binary file object
 *
 *	pathname - in, use
 *		file name
 *
 *	flags - in
 *          File open flags
 *
 *      blocksize - in
 *          File block size (needed for I/O optimization in some OS:s)
 *
 *      createp - in
 *          If TRUE, creates the file if it does not exist.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void SsBOpenLocal(
        SsBFileT* fp,
        char* pathname,
        uint flags,
        size_t blocksize,
        bool createp)
{
        int retries;

        SS_PMON_ADD(SS_PMON_FILEOPEN);
        SS_PMON_ADD(SS_PMON_FILEREAD);

        for (retries = 0; ; retries++) {

#if defined(SS_DOS) || defined(SS_WIN)
            SS_NOTUSED(blocksize);

            fp->fd = sopen(
                        pathname,
                        (createp ? O_CREAT : 0) |
                        ((flags & SS_BF_READONLY)? O_RDONLY : O_RDWR) |
                        O_BINARY,
                        (flags & SS_BF_EXCLUSIVE) ? SH_DENYRW : SH_DENYNO,
                        SSFILE_MODEFLAGS);
            if (fp->fd == -1) {
                fp->err = errno;
                ss_pprintf_2(("SsBOpenLocal failed, errno = %d, retries = %d, open files = %d\n",
                    errno, retries, file_nopen));
                SsErrorMessage(FIL_MSG_SSBOPENLOCAL_FAILED_SDDD, pathname, errno, retries, file_nopen);
            } else {
                fp->err = 0;
                file_nopen++;
                break;
            }
#else /* SS_UNIX, propably */
            SS_NOTUSED(blocksize);
#ifdef SS_DEBUG
            {
                char cwdbuf[256];
                off_t size = SsFSizeOffT(pathname);
                SsGetcwd(cwdbuf, sizeof(cwdbuf));
                ss_dprintf_2(("SsBOpenLocal(%s) fsize=0x%08lX cwd=%s\n",
                              pathname, (long)size, cwdbuf));
            }
#endif /* SS_DEBUG */
#ifdef IO_OPT
            /* Direct IO is chosen to be used automatically with syncwrite */
            ss_dprintf_2(("Applying direct I/O\n"));
            fp->fd = open(
                        pathname,
                        ((createp ? O_CREAT : 0) |
                        ((flags & SS_BF_SYNCWRITE) ? SSFILE_SYNCFLAG : 0) |
                        ((flags & SS_BF_DIRECTIO) ? SSFILE_DIRECTFLAG : 0)|
                        ((flags & SS_BF_READONLY)? O_RDONLY : O_RDWR)),
                        SSFILE_MODEFLAGS);

#if defined(SS_SOLARIS)
            directio(fp->fd,
                     (flags & SS_BF_DIRECTIO) ? DIRECTIO_ON : DIRECTIO_OFF);
#endif /* defined(SS_SOLARIS) */

#else /* IO_OPT */
            fp->fd = open(
                        pathname,
                        ((createp ? O_CREAT : 0) |
                        ((flags & SS_BF_SYNCWRITE) ? SSFILE_SYNCFLAG : 0) |
                        ((flags & SS_BF_READONLY)? O_RDONLY : O_RDWR)),
                        SSFILE_MODEFLAGS);
#endif /* IO_OPT */
            if (fp->fd == -1) {
                fp->err = errno;
                ss_pprintf_2(("SsBOpenLocal failed, errno = %d, retries = %d, open files = %d\n",
                    errno, retries, file_nopen));
                SsErrorMessage(FIL_MSG_SSBOPENLOCAL_FAILED_SDDD,
                    pathname, errno, retries, file_nopen);
            } else {
                if (flags & SS_BF_EXCLUSIVE) {
                    SsBLock(fp, 0L, -1L,
                        ((flags & SS_BF_READONLY)? SSF_SHLOCK : SSF_EXLOCK));
                }
                fp->err = 0;
                file_nopen++;
                break;
            }
#endif /* SS_DOS or WIN */

            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
        }
        fp->lastop = BOP_FLUSH;
}

/*##**********************************************************************\
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
 *          blocksize (needed for I/O optimization in some environments)
 *
 * Return value :
 *
 *      pointer to SsBFileT if successful, NULL otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
SsBFileT *SsBOpen(pathname, flags, blocksize)
char* pathname;
uint flags;
size_t blocksize;
{
        SsBFileT *fp;

        ss_pprintf_2(("SsBOpen(\"%.80s\",flags=%d)\n", pathname, flags));

        FAKE_RETURN(FAKE_SS_BOPENFAILS, NULL);

        fp = SsMemAlloc(sizeof(SsBFileT));
        fp->err = 0;
        fp->flags = flags;
        fp->blocksize = blocksize;
        fp->pathname = SsMemStrdup(SS_FILE_MAKENAME(pathname));
        if (flags & SS_BF_FLUSH_AFTERWRITE) {
            fp->flushtype = SS_BFLUSH_AFTERWRITE;
        } else if (flags & SS_BF_FLUSH_BEFOREREAD) {
            fp->flushtype = SS_BFLUSH_BEFOREREAD;
        } else {
            fp->flushtype = SS_BFLUSH_NORMAL;
        }
        /* for diskless, don't create the file physical. */
        if (flags & SS_BF_DISKLESS) {
            fp->dlsize = 0;
            fp->buffer = NULL;
            return(fp);
        }
        if (ssfile_diskless) {
            SsMemFree(fp->pathname);
            SsMemFree(fp);
            return NULL;
        }
        SsBOpenLocal(fp, fp->pathname, flags, blocksize, TRUE);
        if (fp->fd == -1) {
            SsMemFree(fp->pathname);
            SsMemFree(fp);
            return(NULL);
        } else {
            return(fp);
        }
}

/*#***********************************************************************\
 *
 *		SsBReOpen
 *
 * Closes and reopens a binary file (used for retry after failure)
 *
 * Parameters :
 *
 *	bfile - use
 *
 *
 * Return value :
 *      TRUE when success
 *      FALSE when failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool SsBReOpen(SsBFileT* bfile)
{
        close(bfile->fd);
        file_nopen--;

        SsBOpenLocal(
            bfile,
            bfile->pathname,
            bfile->flags,
            bfile->blocksize,
            FALSE);

        return (bfile->fd != -1);
}

/*##**********************************************************************\
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
        close(bfile->fd);
        file_nopen--;
        SsMemFree(bfile->pathname);
        SsMemFree(bfile);
}

/*##**********************************************************************\
 *
 *     pread, pwrite
 *
 * Read and write from/to specified position.
 * Only needed for systems that do not have corresponding syscalls.
 * Solaris, Linux, FreeBSD do have.
 *
 * Parameters :
 *      f - in
 *      file descriptor
 *
 *      buf - out, use
 *      pointer to buffer
 *
 *      buf_s - in
 *      buffer size
 *
 *      offset - in
 *      file offset.
 *
 * Return value :
 *      number of bytes actually read, or lseek/read error code.
 */

#if defined(BROKEN_PWRITE) || (!defined(SS_LINUX) && !defined(SS_SOLARIS) && !defined(SS_FREEBSD))
#ifdef pread
#undef pread
#endif
#ifdef pwrite
#undef pwrite
#endif

static int ss_pread (int f, void *buf, size_t buf_s, off_t offset)
{
        if (lseek(f, offset, SEEK_SET) != offset) {
            return -1;
        }
        return read(f, buf, buf_s);
}

static int ss_pwrite (int f, const void *buf, size_t buf_s, off_t offset)
{
        if (lseek(f, offset, SEEK_SET) != offset) {
            return -1;
        }
        return write(f, buf, buf_s);
}

#define pread ss_pread
#define pwrite ss_pwrite



#endif

/*##**********************************************************************\
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
 *      errno
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
        su_profile_timer;

        su_profile_start;

        FAKE_RETURN(FAKE_SS_BREADFAILS, (size_t)-1);
        SS_PMON_ADD(SS_PMON_FILEREAD);

        ss_dassert(bfile != NULL);
        ss_dassert(buf_s != (size_t)-1);

        /* for diskless, the server never do a read */
        if (bfile->flags & SS_BF_DISKLESS) {
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                /* should never come here */
                ss_dassert(0);
                return -1;
            }
            if (bfile->dlsize < loc + buf_s) {
                return -1;
            }
            memcpy(buf, bfile->buffer + loc, buf_s);
            return buf_s;
        }

        if (bfile->flushtype == SS_BFLUSH_BEFOREREAD && bfile->lastop == BOP_WRITE) {
            ss_pprintf_2(("SsBRead(\"%.80s\") flush before read\n", bfile->pathname));
            SsBFlush(bfile);
        }
        ss_pprintf_2(("SsBRead(\"%.80s\",loc=%lu,bsiz=%u)\n", bfile->pathname, loc, buf_s));
        for (retries = 0; ; retries++) {
            read_cnt = pread(bfile->fd, buf, buf_s, loc);
            if (read_cnt == (int)buf_s) {
                retv = read_cnt;
                bfile->err = 0;
                bfile->lastop = BOP_READ;
                break;
            }
            retv = -1;
            ss_pprintf_2(("SsBRead:errno = %d\n", errno));
            SsErrorMessage(FIL_MSG_FILEREAD_FAILED_DSDD, errno, bfile->pathname, loc, retries);
            bfile->err = errno;
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
            SsBReOpen(bfile);
        }
        su_profile_stop("SsBRead");
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
        int read_cnt;
        int retv;
        int retries;
        off_t fpos;
        size_t buf_s;
        su_profile_timer;

        su_profile_start;

        if (sizeof(off_t) >= 8) {
#ifndef SS_LARGEFILE
            ss_error;
#endif /* SS_LARGEFILE */
            ss_dassert(sizeof(off_t) == 8);
            fpos = pageaddr;
            fpos *= pagesize;
        } else {
            ss_int8_t pagesize_i8;
            ss_int8_t fpos_i8;
            ss_int4_t u4hi;

#ifdef SS_LARGEFILE
            ss_error;
#endif /* SS_LARGEFILE */
            SsInt8SetUint4(&pagesize_i8, (ss_uint4_t)pagesize);
            SsInt8SetUint4(&fpos_i8, pageaddr);
            SsInt8MultiplyByInt8(&fpos_i8, fpos_i8, pagesize_i8);
            u4hi = SsInt8GetMostSignificantUint4(fpos_i8);
            if (u4hi != 0) {
                ss_derror;
                return (-1);
            }
            fpos = SsInt8GetLeastSignificantUint4(fpos_i8);
        }
        buf_s = pagesize * npages;
        FAKE_RETURN(FAKE_SS_BREADFAILS, (size_t)-1);
        SS_PMON_ADD(SS_PMON_FILEREAD);

        ss_dassert(bfile != NULL);

        /* for diskless, the server never do a read */
        if (bfile->flags & SS_BF_DISKLESS) {
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                /* should never come here */
                ss_dassert(0);
                return -1;
            }
            if (bfile->dlsize < fpos + buf_s) {
                return -1;
            }
            memcpy(buf, bfile->buffer + fpos, buf_s);
            return npages;
        }

        if (bfile->flushtype == SS_BFLUSH_BEFOREREAD && bfile->lastop == BOP_WRITE) {
            ss_pprintf_2(("SsBReadPages(\"%.80s\") flush before read\n", bfile->pathname));
            SsBFlush(bfile);
        }
        ss_pprintf_2(("SsBReadPages(\"%.80s\",pageaddr=%lu,bsiz=%u)\n",
                      bfile->pathname, (ulong)pageaddr, buf_s));
        for (retries = 0; ; retries++) {
            read_cnt = pread(bfile->fd, buf, buf_s, fpos);
            if (read_cnt == (int)buf_s) {
                retv = npages;
                bfile->err = 0;
                bfile->lastop = BOP_READ;
                break;
            }
            retv = -1;
            ss_pprintf_2(("SsBRead:errno = %d\n", errno));
            SsErrorMessage(FIL_MSG_FILEREADPAGE_FAILED_DSDDDD,
                           errno,
                           bfile->pathname,
                           (uint)npages,
                           (uint)pagesize,
                           (ulong)pageaddr,
                           retries);
            bfile->err = errno;
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
            SsBReOpen(bfile);
        }
        su_profile_stop("SsBReadPages");
        return (size_t)retv;
}

/*##**********************************************************************\
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
 *      errno
 */
bool SsBWrite(bfile, loc, data, data_s)
        SsBFileT *bfile;
        long loc;
        void *data;
        size_t data_s;
{
        bool rc;
        int retries;
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        ss_dassert(data_s != (size_t)-1);
        ss_pprintf_2(("SsBWrite(\"%.80s\",loc=%lu,bsiz=%u,1stbyte=%d)\n", bfile->pathname, loc, data_s, (int)((ss_byte_t*)data)[0]));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));
        FAKE_RETURN(FAKE_SS_BWRITEFAILS, FALSE);
        SS_PMON_ADD(SS_PMON_FILEWRITE);

        /* for diskless, write is a no-op. change the size if needed. */
        if (bfile->flags & SS_BF_DISKLESS) {
            if (loc + data_s > bfile->dlsize) {
                SsBExpand(bfile, loc + data_s);
            }
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                return TRUE;
            }
            memcpy(bfile->buffer + loc, data, data_s);
            return TRUE;
        }

        for (retries = 0; ; retries++) {
            if (pwrite(bfile->fd, data, data_s, loc) == (int)data_s) {
                bfile->lastop = BOP_WRITE;
                rc = TRUE;
                if (bfile->flushtype == SS_BFLUSH_AFTERWRITE) {
                    ss_pprintf_2(("SsBWrite(\"%.80s\") flush after write\n", bfile->pathname));
                    rc = SsBFlush(bfile);
                }
                break;
            }
            ss_pprintf_2(("SsBWrite:errno = %d\n", errno));
            bfile->err = errno;
            rc = FALSE;
            SsErrorMessage(FIL_MSG_FILEWRITE_FAILED_DSDD, errno, bfile->pathname, loc, retries);
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
            SsBReOpen(bfile);
        }
        su_profile_stop("SsBWrite");
        return (rc);
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
        off_t fpos;
        size_t data_s;
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        ss_pprintf_2(("SsBWritePages(\"%.80s\",daddr=%lu,npages=%u,"
                "1stbyte=%d)\n",
                bfile->pathname,
                (ulong)pageaddr,
                (uint)npages,
                (int)((ss_byte_t*)data)[0]));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));
        if (sizeof(off_t) >= 8) {
#ifndef SS_LARGEFILE
            ss_error;
#endif /* SS_LARGEFILE */
            ss_dassert(sizeof(off_t) == 8);
            fpos = pageaddr;
            fpos *= pagesize;
        } else {
            ss_int8_t pagesize_i8;
            ss_int8_t fpos_i8;
            ss_int4_t u4hi;
#ifdef SS_LARGEFILE
            ss_error;
#endif /* SS_LARGEFILE */
            SsInt8SetUint4(&pagesize_i8, (ss_uint4_t)pagesize);
            SsInt8SetUint4(&fpos_i8, pageaddr);
            SsInt8MultiplyByInt8(&fpos_i8, fpos_i8, pagesize_i8);
            u4hi = SsInt8GetMostSignificantUint4(fpos_i8);
            if (u4hi != 0) {
                ss_derror;
                return (FALSE);
            }
            fpos = SsInt8GetLeastSignificantUint4(fpos_i8);
        }
        data_s = pagesize * npages;
        FAKE_RETURN(FAKE_SS_BWRITEFAILS, FALSE);
        SS_PMON_ADD(SS_PMON_FILEWRITE);


        /* for diskless, write is a no-op. change the size if needed. */
        if (bfile->flags & SS_BF_DISKLESS) {
            if (fpos + data_s > bfile->dlsize) {
                SsBExpand(bfile, fpos + data_s);
            }
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                return TRUE;
            }
            memcpy(bfile->buffer + fpos, data, data_s);
            return TRUE;
        }

        for (retries = 0; ; retries++) {
#ifdef IO_OPT
            ss_dprintf_2(("addr : %lu, mod 512U : %d, count : %d, offset : %lu\n",
                    (ss_byte_t *)data,
                    (ss_ptr_as_scalar_t)data % SS_DIRECTIO_ALIGNMENT,
                    (int)data_s,
                    (ss_byte_t *)fpos) );
#endif /* IO_OPT */
            if (pwrite(bfile->fd, data, data_s, fpos) == (int)data_s) {
                bfile->lastop = BOP_WRITE;
                rc = TRUE;
                if (bfile->flushtype == SS_BFLUSH_AFTERWRITE) {
                    ss_pprintf_2(("SsBWrite(\"%.80s\") flush after write\n",
                                  bfile->pathname));
                    rc = SsBFlush(bfile);
                }
                break;
            } else {
                ss_dassert(fpos >= 0);
            }
            ss_pprintf_2(("SsBWritePages:errno = %d\n", errno));
            bfile->err = errno;
            rc = FALSE;
            SsErrorMessage(FIL_MSG_FILEWRITEPAGE_FAILED_DSDDDD,
                           errno,
                           bfile->pathname,
                           (uint)npages,
                           (uint)pagesize,
                           (ulong)pageaddr,
                           retries);
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
            SsBReOpen(bfile);
        }
        su_profile_stop("SsBWritePages");
        return (rc);
}

/*##**********************************************************************\
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
 *      errno
 */
bool SsBAppend(bfile, data, data_s)
        SsBFileT *bfile;
        void *data;
        size_t data_s;
{
        bool rc;
        int retries;
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        ss_dassert(data_s != (size_t)-1);
        ss_pprintf_2(("SsBAppend(\"%.80s\", bsiz=%u)\n", bfile->pathname, data_s));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));
        SS_PMON_ADD(SS_PMON_FILEAPPEND);

        FAKE_RETURN(FAKE_SS_BAPPFAILS, FALSE);
        FAKE_RET_RESET(FAKE_SS_BAPPRESET, FALSE);

        if (bfile->flags & SS_BF_DISKLESS) {
            return SsBWrite(bfile, bfile->dlsize, data, data_s);
        }

        for (retries = 0; ; retries++) {
            if (lseek(bfile->fd, 0L, SEEK_END) == -1L) {
                ss_pprintf_2(("SsBAppend:errno = %d\n", errno));
                bfile->err = errno;
                rc = FALSE;
                SsErrorMessage(FIL_MSG_FILEWRITEEND_FAILED_DSD, errno, bfile->pathname, retries);
            } else {
                if (write(bfile->fd, data, data_s) == (int)data_s) {
                    bfile->lastop = BOP_WRITE;
                    rc = TRUE;
                    if (bfile->flushtype == SS_BFLUSH_AFTERWRITE) {
                        ss_pprintf_2(("SsBWrite(\"%.80s\") flush after append write\n", bfile->pathname));
                        rc = SsBFlush(bfile);
                    }
                    break;
                } else {
                    ss_pprintf_2(("SsBAppend:errno = %d\n", errno));
                    bfile->err = errno;
                    rc = FALSE;
                    SsErrorMessage(FIL_MSG_FILEAPPEND_FAILED_DSD, errno, bfile->pathname, retries);
                }
            }
            if (retries >= SSFILE_MAXRETRIES) {
                break;
            }
            SsThrSleep(SSFILE_RETRYSLEEPTIME);
            SsBReOpen(bfile);
        }
        su_profile_stop("SsBAppend");
        return rc;
}

/*##**********************************************************************\
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
 *      errno
 */
bool SsBFlush(bfile)
        SsBFileT *bfile;
{
        su_profile_timer;

        su_profile_start;

        ss_dassert(!(bfile->flags & SS_BF_READONLY));

        /* for diskless flush is a no-op. */
        if (bfile->flags & SS_BF_DISKLESS) {
            return TRUE;
        }

        if (bfile->flags & SS_BF_NOFLUSH) {
            return TRUE;
        }

        if (bfile->flushtype != SS_BFLUSH_NORMAL &&
            bfile->lastop == BOP_FLUSH) {
            /* The previous file operation was flush, skip */
            ss_pprintf_2(("SsBFlush(\"%.80s\"), skipping\n", bfile->pathname));
            return(TRUE);
        }
        ss_pprintf_2(("SsBFlush(\"%.80s\")\n", bfile->pathname));
        SS_PMON_ADD(SS_PMON_FILEFLUSH);

        bfile->lastop = BOP_FLUSH;
        {

#if defined(SS_LINUX) || defined(SS_SOLARIS)
        /* LINUX & SOLARIS have fsync() */
        int rc;

        rc = fsync(bfile->fd);
        if (rc == 0) {
            su_profile_stop("SsBFlush");
            return TRUE;
        }
        bfile->err = errno;
        SsErrorMessage(FIL_MSG_FILEFLUSH_FAILED_DS,
            bfile->err, bfile->pathname);
        su_profile_stop("SsBFlush");
        return (FALSE);

#else /* defined(SS_DOS) || defined(SS_WIN) || defined(SS_UNIX) */
        int rc;

        ss_dassert(bfile != NULL);
        rc = close(bfile->fd);
        file_nopen--;
        if (rc == -1) {
            ss_pprintf_2(("SsBFlush:errno = %d\n", errno));
            bfile->err = errno;
            SsErrorMessage(FIL_MSG_FILEFLUSHCLOSE_FAILED_DS,
                bfile->err, bfile->pathname);
            su_profile_stop("SsBFlush");
            return FALSE;
        }
        SsBOpenLocal(
            bfile,
            bfile->pathname,
            bfile->flags,
            bfile->blocksize,
            FALSE);
        if (bfile->fd == -1) {
            SsErrorMessage(FIL_MSG_FILEFLUSHOPEN_FAILED_DS,
                bfile->err, bfile->pathname);
            su_profile_stop("SsBFlush");
            return FALSE;
        }
        su_profile_stop("SsBFlush");
        return TRUE;
#endif
        }
}


static off_t SsBSizeAsOffT(
        SsBFileT *bfile)
{
#if defined(SS_DOS) || defined(SS_WIN) 
        off_t len;

        ss_dassert(bfile != NULL);
        if (bfile->flags & SS_BF_DISKLESS) {
            return bfile->dlsize;
        }

        len = filelength(bfile->fd);
        if (len == -1) {
            bfile->err = errno;
            SsErrorMessage(FIL_MSG_FILESIZEQUERY_FAILED_DSD,
                bfile->err, bfile->pathname, 0);
        }
        return(len);
#else
        struct stat s;
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        if (bfile->flags & SS_BF_DISKLESS) {
            su_profile_stop("SsBSize");
            return bfile->dlsize;
        }

        if (fstat(bfile->fd, &s)) {
            bfile->err = errno;
            SsErrorMessage(FIL_MSG_FILESIZEQUERY_FAILED_DSD,
                bfile->err, bfile->pathname, 0);
            ss_derror;
            return (0);
        } else {
#ifdef SS_LARGEFILE
            ss_dprintf_2(("SsBSizeAsOffT(%s) size=0x%08lX%08lX\n",
                          bfile->pathname,
                          (ulong)(ss_uint4_t)(s.st_size >> 32),
                          (ulong)(ss_uint4_t)s.st_size));

            ss_ct_assert(sizeof(s.st_size) == 8);
#else /* SS_LARGEFILE */
            ss_dprintf_2(("SsBSizeAsOffT(%s) size=0x%08lX\n",
                          bfile->pathname,
                          (ulong)s.st_size));

#endif /* SS_LARGEFILE */
            su_profile_stop("SsBSize");
            return(s.st_size);
        }
#endif
}

ss_int8_t SsBSizeAsInt8(
        SsBFileT *bfile)
{
        off_t fsize;
        ss_int8_t i8;

        fsize = SsBSizeAsOffT(bfile);
#ifdef SS_NATIVE_UINT8_T
        SsInt8SetNativeUint8(&i8, (SS_NATIVE_UINT8_T)fsize);
#else /* SS_NATIVE_UINT8_T */
#  ifdef SS_LARGEFILE
        {
            ss_uint4_t u4hi;
            ss_uint4_t u4lo;

            ss_ct_assert(sizeof(fsize) == 8);
            u4lo = (ss_uint4_t)fsize;
            u4hi = (ss_uint4_t)(fsize >> 32);
            SsInt8Set2Uint4s(&i8, u4hi, u4lo);
        }
#  else /* SS_LARGEFILE */
        ss_ct_assert(sizeof(fsize) == 4);
        SsInt8SetUint4(&i8, (ss_uint4_t)fsize);
#  endif /* SS_LARGEFILE */
#endif /* SS_NATIVE_UINT8_T */
        return (i8);
}

/*##**********************************************************************\
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
        return ((long)SsBSizeAsOffT(bfile));
}

/*##**********************************************************************\
 *
 *      SsBSizePages
 *
 * gives file size as pages (assuming file size is a multiple of page size
 *
 * Parameters:
 *      bfile - in, use
 *          bfile object
 *
 *      pagesize - in
 *          page size in bytes
 *
 * Return value:
 *      filesizeinbytes / pagesize
 *
 * Limitations:
 *
 * Globals used:
 */
ss_uint4_t SsBSizePages(
        SsBFileT* bfile,
        size_t pagesize)
{
        off_t fsize;

        ss_dassert(pagesize != 0);
        fsize = SsBSizeAsOffT(bfile);
        return ((ss_uint4_t)(fsize / pagesize));
}


static bool SsBChsizeOffT(
        SsBFileT* bfile,
        off_t newsize)
{
#ifdef SS_LARGEFILE
        ss_dprintf_2(("SsBChsizeOffT: file:%s, newsize=0x%08lX%08lX\n",
                      bfile->pathname,
                      (ulong)(ss_uint4_t)(newsize >> 32),
                      (ulong)(ss_uint4_t)newsize));
#else /* SS_LARGEFILE */
        ss_dprintf_2(("SsBChsizeOffT: file:%s, newsize=0x%08lX\n",
                      bfile->pathname,
                      (ulong)newsize));

#endif /* SS_LARGEFILE */
        if (bfile->flags & SS_BF_DISKLESS) {
            bfile->dlsize = newsize;
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                return TRUE;
            }
            if (bfile->buffer) {
                bfile->buffer = SsMemRealloc(bfile->buffer, newsize);
            } else {
                bfile->buffer = SsMemAlloc(newsize);
            }
            return TRUE;
        }
#if defined(SS_DOS) || defined(SS_WIN)
        {
            int st;

            ss_dassert(!(bfile->flags & SS_BF_READONLY));
            ss_dassert(bfile != NULL);
            st = chsize(bfile->fd, newsize);
            if (st == 0) {
                return(TRUE);
            } else {
                bfile->err = errno;
                SsErrorMessage(FIL_MSG_FILESIZECHANGE_FAILED_DSDD,
                               bfile->err, bfile->pathname, newsize, 0);
                return(FALSE);
            }
        }
#elif defined(SS_UNIX) 
        {
            int retval;
            retval = ftruncate(bfile->fd, newsize);
            if (retval != 0) {
                bfile->err = errno;
                SsErrorMessage(FIL_MSG_FILESIZECHANGE_FAILED_DSDD,
                               bfile->err, bfile->pathname, (long)newsize, 0);
                return(FALSE);
            } else {
                return (TRUE);
            }
        }
#else
        Error! Truncate must be possible, porting must be completed!!
        ss_derror;
        return(FALSE);
#endif
}

bool SsBChsizePages(
        SsBFileT *bfile,
        ss_uint4_t newsizeinpages,
        size_t pagesize)
{
        bool succp;
        off_t newsize;

        ss_ct_assert(sizeof(off_t) == 8);
        newsize = newsizeinpages;
        newsize *= pagesize;
        succp = SsBChsizeOffT(bfile, newsize);
        return (succp);
}

/*##**********************************************************************\
 *
 *		SsBChsize
 *
 * Changes the file size, both increase and decrease are possible.
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
 *      errno
 */
bool  SsBChsize(bfile, newsize)
        SsBFileT *bfile;
        long newsize;
{
        bool succp;

        succp = SsBChsizeOffT(bfile, newsize);
        return (succp);
}

/*##**********************************************************************\
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

#if defined(SS_DOS) || defined(SS_WIN)

        ss_dassert(bfile != NULL);
        ss_dassert(newsize > SsBSize(bfile));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));

        return(SsBChsize(bfile, newsize));
#else
        int rc;
        char tmpch = '\0';   /* needed for buffer of 1-byte writes */
        su_profile_timer;

        su_profile_start;

        ss_dassert(bfile != NULL);
        ss_dassert(newsize > SsBSize(bfile));
        ss_dassert(!(bfile->flags & SS_BF_READONLY));

        if (bfile->flags & SS_BF_DISKLESS) {
            bfile->dlsize = newsize;
            if (bfile->flags & SS_BF_DLSIZEONLY) {
                return TRUE;
            }
            if (bfile->buffer) {
                bfile->buffer = SsMemRealloc(bfile->buffer, newsize);
            } else {
                bfile->buffer = SsMemAlloc(newsize);
            }
            return TRUE;
        }
        /* Write to the new file size position. */
        rc = SsBWrite(bfile, newsize - 1, &tmpch, 1);

        su_profile_stop("SsBExpand");
        return(rc);
#endif
}


SsBFlushTypeT SsBSetFlushType(
        SsBFileT *bfile,
        SsBFlushTypeT type)
{
        SsBFlushTypeT oldtype = bfile->flushtype;
        ss_rc_dassert(type == SS_BFLUSH_NORMAL ||
                      type == SS_BFLUSH_BEFOREREAD ||
                      type == SS_BFLUSH_AFTERWRITE, type);
        bfile->flushtype = type;
        return(oldtype);
}

#else /* !SEPARATE_SSB_ROUTINES */

SsBFlushTypeT SsBSetFlushType(
        SsBFileT *bfile,
        SsBFlushTypeT type)
{
        /* dummy */
        SS_NOTUSED(bfile);
        SS_NOTUSED(type);
        return(SS_BFLUSH_NORMAL);
}

#endif /* !SEPARATE_SSB_ROUTINES */

#ifdef MME_CP_FIX

#if defined(SS_NATIVE_LISTIO_AVAILABLE)

bool SsFileListIO(
        SsLIOReqT req_array[/*nreq*/],
        size_t nreq)
{
        bool succp = TRUE;
        int ret;
        size_t i;
        uint retryctr = 0;

        struct aiocb aiocb_prealloc_buf[SSFILE_LIO_PREALLOCSIZE];
        struct aiocb* aiocb_ptr_prealloc_buf[SSFILE_LIO_PREALLOCSIZE];
        struct aiocb* aiocb_array = aiocb_prealloc_buf;
        struct aiocb** aiocb_ptr_array = aiocb_ptr_prealloc_buf;

        if (nreq > SSFILE_LIO_PREALLOCSIZE) {
            aiocb_array =
                SsMemCalloc(
                    nreq,
                    sizeof(aiocb_prealloc_buf[0]));
            aiocb_ptr_array =
                SsMemAlloc(nreq *
                           sizeof(aiocb_ptr_prealloc_buf[0]));
        } else {
            memset(aiocb_ptr_prealloc_buf,
                   0,
                   (sizeof(aiocb_ptr_prealloc_buf[0]) *
                    nreq));
        }
        for (i = 0; i < nreq; i++) {
            aiocb_ptr_array[i] = &aiocb_array[i];
            switch (req_array[i].lio_reqtype) {
                case SS_LIO_WRITE:
                    aiocb_array[i].aio_lio_opcode = LIO_WRITE;
                    break;
                case SS_LIO_READ:
                    aiocb_array[i].aio_lio_opcode = LIO_READ;
                    break;
                default:
                    ss_rc_error(req_array[i].lio_reqtype);
            }
            aiocb_array[i].aio_fildes = req_array[i].lio_bfile->fd;
            aiocb_array[i].aio_reqprio = 0;
            aiocb_array[i].aio_buf = req_array[i].lio_data;
            aiocb_array[i].aio_nbytes =
                req_array[i].lio_pagesize *
                req_array[i].lio_npages;

            if (sizeof(off_t) >= 8) {
#ifndef SS_LARGEFILE
                ss_error;
#endif /* SS_LARGEFILE */
                ss_dassert(sizeof(off_t) == 8);
                aiocb_array[i].aio_offset =
                    (off_t)req_array[i].lio_pageaddr *
                    req_array[i].lio_pagesize;
            } else {
                ss_int8_t pagesize_i8;
                ss_int8_t fpos_i8;
                ss_uint4_t u4hi;
                SsInt8SetUint4(&pagesize_i8,
                               (ss_uint4_t)req_array[i].lio_pagesize);
                SsInt8SetUint4(&fpos_i8, req_array[i].lio_pageaddr);
                SsInt8MultiplyByInt8(&fpos_i8, fpos_i8, pagesize_i8);
                u4hi = SsInt8GetMostSignificantUint4(fpos_i8);
                if (u4hi != 0) {
                    ss_derror;
                }
                aiocb_array[i].aio_offset =
                    SsInt8GetLeastSignificantUint4(fpos_i8);
#ifdef SS_LARGEFILE
                ss_error;
#endif /* SS_LARGEFILE */
            }
        }
 retry:;
        ret = lio_listio(LIO_WAIT, aiocb_ptr_array, nreq, NULL);
        if (ret != 0) {
            bool need_retry = FALSE;
            /* we should handle EAGAIN return codes here! */
            ss_dassert(ret == -1);
            for (i = 0; i < nreq; i++) {
                req_array[i].lio_naterr = aio_error(&aiocb_array[i]);
                switch (req_array[i].lio_naterr) {
                    case EAGAIN:
                        retryctr++;
                        /* FALLTHROUGH */
                    case EINTR:
                        need_retry = TRUE;;
                        break;
                    default: /* non-recoverable error */
                        succp = FALSE;
                        /* FALLTHROUGH */
                    case 0:
                        /* make pointers to already succeeded or
                           irrecoverably failed requests NULL
                           in order to ignore it on possible retry
                        */
                        aiocb_ptr_array[i] = NULL;
                        break;
                }
            }
            ss_dassert(!succp || need_retry);
            if (succp && need_retry) {
                if (retryctr <= SSFILE_MAXRETRIES) {
                    SsThrSleep(SSFILE_RETRYSLEEPTIME);
                    goto retry;
                } else {
                    succp = FALSE;
                }
            }
        }
        if (nreq > SSFILE_LIO_PREALLOCSIZE) {
            SsMemFree(aiocb_array);
            SsMemFree(aiocb_ptr_array);
        }
        return (succp);
}

#elif defined(SS_NATIVE_WRITEV_AVAILABLE) /* SS_NATIVE_LISTIO_AVAILABLE */

#else /* SS_NATIVE_LISTIO_AVAILABLE */

bool SsFileListIO(
        SsLIOReqT req_array[/*nreq*/],
        size_t nreq)
{
        bool succp = TRUE;
        size_t i;

        for (i = 0; i < nreq; i++) {
            switch (req_array[i].lio_reqtype) {
                case SS_LIO_WRITE: {
                    bool succp2;
                    succp2 = SsBWritePages(
                            req_array[i].lio_bfile,
                            req_array[i].lio_pageaddr,
                            req_array[i].lio_pagesize,
                            req_array[i].lio_data,
                            req_array[i].lio_npages);
                    if (succp2) {
                        /* req_array[i].lio_npagesprocessed =
                           req_array[i].lio_npages; */
                        req_array[i].lio_naterr = 0;
                    } else {
                        succp = FALSE;
                        /* req_array[i].lio_npagesprocessed = 0; */
                        req_array[i].lio_naterr =
                            SsBErrno(req_array[i].lio_bfile);
                    }
                    break;
                }
                case SS_LIO_READ: {
                    int npages_read;
                    npages_read =
                        SsBWritePages(
                                req_array[i].lio_bfile,
                                req_array[i].lio_pageaddr,
                                req_array[i].lio_pagesize,
                                req_array[i].lio_data,
                                req_array[i].lio_npages);
                    if (npages_read != (int)req_array[i].lio_npages) {
                        succp = FALSE;
                    }
                    break;
                }
                default:
                    ss_rc_error(req_array[i].lio_reqtype);
            }
        }
        return (succp);
}
#endif /* SS_NATIVE_LISTIO_AVAILABLE */
#endif /* MME_CP_FIX */

/* EOF */
