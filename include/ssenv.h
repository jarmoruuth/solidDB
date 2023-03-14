/*************************************************************************\
**  source       * ssenv.h
**  directory    * ss
**  description  * OS and environment specific compiler options
**               * and definitions.
**               * This file should be included before all other
**               * headers.
**               *
**               * HOW TO PORT SOLID TO A NEW ENVIRONMENT
**               *
**               * At least the following source files need to be modified
**               * when porting solid to a new environment:
**               *
**               *      ssenv.?
**               *      sslimits.h
**               *      ssfloat.h
**               *      sstraph.c
**               *      sstraph2.c
**               *      sssem???.c
**               *      ssc.h
**               *      ssfile.?    (non-unix)
**               *      ssproces.?  (non-unix)
**               *      ses*.c     (possible communication changes)
**               *      com0pdef.c (default listen/connect infos)
**               *      com0cfg.h  (non-unix: default msgsizes)
**               *      css/rguti.c (filename conversion env)
**               *      sse/solinst.c (unixes)
**               *
**               * The following make-environment files need also
**               * modifications:
**               *
**               *      root.inc
**               *          new makefile define for the OS
**               *          add also to so/root.inc
**               *      makefile.inc
**               *          branch for the OS
**               *          new command line define for OS
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

#ifndef SSENV_H
#define SSENV_H

#include "ssmyenv.h"

#ifndef SS_TC_CLIENT
#define SS_TC_CLIENT
#endif

#ifndef SS_MME
#define SS_MME
/* The following are mandatory 2006-10-23 apl */
#define SS_MMEG2
#define MMEG2_MUTEXING
#define MMEG2_VERSIONING
#define MMEG2_SHARED_READS
#define SS_BNODE_INTEGRATED
#define SS_BNODE_JOIN
#define SS_FFMEM_FREELEVELS
#define LMGR_SPLITMUTEX
#endif

#ifndef SS_NOMMINDEX
#define SS_NOMMINDEX
#endif

#ifndef SS_TASKOPTIMIZE
#define SS_TASKOPTIMIZE
#endif

#ifndef SS_ACCOPTIMIZE
#define SS_ACCOPTIMIZE
#endif

#ifndef SS_NETOPTIMIZE
#define SS_NETOPTIMIZE
#endif

#ifdef ENABLE_HISTORY_OPTIMIZATION
#undef ENABLE_HISTORY_OPTIMIZATION
#endif

#ifndef SS_HSBG2
#define SS_HSBG2
#endif

#ifndef RS_FLATAVAL
#define RS_FLATAVAL
#endif

#if !defined(SS_FFMEM) && !defined(SS_MYSQL)
#define SS_FFMEM
#endif

#ifndef SS_COREOPT
#define SS_COREOPT
#endif

#ifdef SS_DEBUG
# ifndef SS_BETA
#  define SS_BETA
# endif /* SS_BETA */
# ifndef SS_PROFILE
#  define SS_PROFILE
# endif /* SS_PROFILE */
#endif /* SS_DEBUG */

#ifdef SS_SEMPROFILE_DEFAULT
#define SS_SEMPROFILE
#endif

#if defined(SS_SEMPROFILE) && !defined(SS_PROFILE)
# define SS_PROFILE
#endif
#if defined(SS_PROFILE) && !defined(SS_SEMPROFILE)
# define SS_SEMPROFILE
#endif

#if defined(SS_DEBUG) || defined(SS_BETA) || defined(SS_PURIFY) || defined(SS_COVER)
# undef SS_HSBG2_TESTGA
#else
# define SS_HSBG2_TESTGA
#endif

#define HSB_LPID
#define HSBG2_NEW_BUSY_DETECT
#define HSBG2_ASYNC_REMOTE_DURABLE_ACK

#ifndef EASE_OF_USE_EVENTS
#define EASE_OF_USE_EVENTS
#endif

#ifndef COLLATION_UPDATE
#define COLLATION_UPDATE
#endif

#ifndef SS_ALTER_TABLE
#define SS_ALTER_TABLE
#endif

#ifndef REFERENTIAL_INTEGRITY
#define REFERENTIAL_INTEGRITY
#endif

#ifndef RS_REFTVALS
#define RS_REFTVALS
#endif

#ifndef SS_REFDVA_MEMLINK
#define SS_REFDVA_MEMLINK
#endif

#ifndef LICENSEINFO_V2
#define LICENSEINFO_V2
#endif

#ifndef SS_SYNC
#define SS_SYNC
#endif

#ifdef SS_UNICODE_ALL
#define SS_UNICODE_DATA
#define SS_UNICODE_SQL
#define SS_UNICODE_CLI
#endif

#ifndef SS_CATALOGSUPP
#define SS_CATALOGSUPP
#endif

#ifndef DBE_REPLICATION
# define DBE_REPLICATION
#endif

#ifndef DBE_HSB_REPLICATION
# define DBE_HSB_REPLICATION
#endif

#ifndef HSB_FAILURE_FIX
# define HSB_FAILURE_FIX
#endif

#ifndef DBE_LOGORDERING_FIX
# define DBE_LOGORDERING_FIX
#endif

#ifndef DBE_BNODE_MISMATCHARRAY
# define DBE_BNODE_MISMATCHARRAY
#endif

/* SOLIDFAST means direct function calls in SQL without function pointer table. */
#ifndef SOLIDFAST
# define SOLIDFAST
#endif

#ifndef SS_FILEPATHPREFIX
# define SS_FILEPATHPREFIX
#endif

#ifndef SS_BRKSOCKET_WITH_SOCKETPAIR
# define SS_BRKSOCKET_WITH_SOCKETPAIR
#endif

#ifndef SS_CONBLOCK
# define SS_CONBLOCK
#endif

typedef enum SsCpuIdEnum {

        SS_CPU_NULL         = 0,
        SS_CPU_IX86         = 1,
        SS_CPU_POWERPC      = 2,
        SS_CPU_SPARC        = 3,
        SS_CPU_IA64	    = 4,
        SS_CPU_AMD64	    = 5,
	SS_CPU_SPARC64      = 6,
        SS_CPU_IA64_32BIT   = 7,

        SS_CPU_ENDVALUE

} SsCpuIdT;
/*
 * Note: SsCpuIdEnum, SsOsIdEnum and SsEnvIdEnum must have exactly the
 * same value across all branches and versions.
 */

typedef enum SsOsIdEnum {

        SS_OS_NULL      = 0,

        SS_OS_DOS       = 1,
        SS_OS_WIN       = 2,
        SS_OS_W95       = 3, /* W95 has osvers 4.0, W98 has osvers 4.10 */
        SS_OS_WNT       = 4,
        SS_OS_LINUX     = 5,
        SS_OS_SOLARIS   = 6,
        SS_OS_FREEBSD   = 7,

        SS_OS_ENDVALUE

} SsOsIdT;
/*
 * Note: SsCpuIdEnum, SsOsIdEnum and SsEnvIdEnum must have exactly the
 * same value across all branches and versions.
 */

typedef enum SsEnvIdEnum {

        SS_ENVID_DOS = 0,
        SS_ENVID_D4G,
        SS_ENVID_W16 = 10,
        SS_ENVID_W95 = 11,
        SS_ENVID_NTI = 12,
        SS_ENVID_NTA = 13,
        SS_ENVID_W98 = 15,
        SS_ENVID_O16 = 20,
        SS_ENVID_O32,
        SS_ENVID_OVV = 40,
        SS_ENVID_OVA,
        SS_ENVID_A3X = 50,
        SS_ENVID_A4X,
        SS_ENVID_A4X64BIT,
        SS_ENVID_A5X,
        SS_ENVID_H9X = 60,
        SS_ENVID_H0X,
        SS_ENVID_H1X,
        SS_ENVID_H1X64BIT,
        SS_ENVID_HIA,
        SS_ENVID_HIA64BIT,
        SS_ENVID_SCX = 70,
        SS_ENVID_SSX = 80,
        SS_ENVID_S8X = 81,
        SS_ENVID_S8X64BIT   = 82,
        SS_ENVID_S9X        = 83,
        SS_ENVID_S9X64BIT   = 84,
        SS_ENVID_S0X        = 85,
        SS_ENVID_S0X64BIT   = 86,
        SS_ENVID_S0XI       = 87,
        SS_ENVID_S0XI64BIT  = 88,
        SS_ENVID_IRX = 90,
        SS_ENVID_L2X64 = 95,
        SS_ENVID_LUX = 100,
        SS_ENVID_LXA = 101,
        SS_ENVID_L2X = 102,
        SS_ENVID_LPX = 103,
        SS_ENVID_LSAX = 104,
	SS_ENVID_LXSB = 106,
        SS_ENVID_CLX = 110,
        SS_ENVID_DIX = 120,
        SS_ENVID_VPX,
	SS_ENVID_VMX,
        SS_ENVID_VSSX = 149,
        SS_ENVID_FREEBSD = 150,
        SS_ENVID_FEX = 151,
        SS_ENVID_FEX64 = 152,
        SS_ENVID_PSP = 180,
	SS_ENVID_OSA = 190,
        SS_ENVID_BSI = 192,
	SS_ENVID_QPX = 199,
        SS_ENVID_WNT64 = 202

} SsEnvIdT;
/*
 * Note: SsCpuIdEnum, SsOsIdEnum and SsEnvIdEnum must have exactly the
 * same value across all branches and versions.
 */

/* ---------------------------------------------------------------------
 *          Version number information
 */
#ifndef SS_SERVER_VERSION
#define SS_SERVER_VERSION   ss_versiontext()
#endif

#ifndef SS_SERVER_NAME
#define SS_SERVER_NAME      ss_servername()
#endif

#define SS_GENERIC_DBMS_NAME "Solid Database Engine"
/*NOTE: generic name is also hardcoded in ssver.c, ssversndl.c, ssversnsf.c
sssncver.c, sssereesf.c and makefile.inc */
#define SS_SERVER_VERSNUM   ss_versionnumber()
#define SS_SOLIDVERS        200

#define SS_SERVER_VERSNUM_MINOR     ss_vers_minor()
#define SS_SERVER_VERSNUM_MAJOR     ss_vers_major()
#define SS_SERVER_VERSNUM_RELEASE   ss_vers_release()

/* Copyright string. When you change this, change also makefile.inc.
 */
#define SS_COPYRIGHT        ss_copyright


/* The following variables are in ssdebug.c (to avoid linking problems).
 */
extern const char* ss_company_name;
extern const char* ss_copyright_short;
extern const char* ss_copyright;
extern char* ss_cmdline;
extern int   ss_migratehsbg2; /* bool */
extern int   ss_convertdb;    /* bool */

char*   ss_servername(void);
void    ss_setservername(char* name);
char*   ss_versiontext(void);
int     ss_versionnumber(void);
int     ss_vers_major(void);
int     ss_vers_minor(void);
int     ss_vers_release(void);
long    ss_codebaseversion(void);
int     ss_vers_dll(void);
char*   ss_vers_dllpostfix(void);
int     ss_vers_issync(void);
int     ss_vers_isaccelerator(void);
int     ss_vers_isdiskless(void);

/* ---------------------------------------------------------------------
 *          Operating system dependent part
 *
 *  You have to specify exactly one of the following operating systems
 *
 *      - SS_WIN
 *      - SS_DOS
 *      - SS_UNIX
 *      - SS_LINUX
 *
 *      Example: cc -c -DSS_LINUX -DSS_UNIX
 *
 *
 *  SS_WIN can contain an SS_DLL flag. It indicates that
 *
 *      - All functions in source files containing an SS_EXPORT_C
 *        attribute are going to be exported from the becoming
 *        DLL module.
 *
 *      - All external functions containing an SS_EXPORT_H attribute in
 *        their prototype are located in a DLL.
 *        This means that in WINDOWS they can not be called from the same
 *        module without SsMakeCallbackFun (MakeProcInstance)
 *
 *      Example: cl -c -DSS_WIN -DSS_DLL
 *
 *
 *  For test executables in DLL environment you can define SS_EXE to get
 *  the correct view to the system headers. (scanf in WINDOWS etc.)
 *  The combination SS_DLL and SS_EXE still expands SS_EXPORT to _export.
 *  It also defines WINVER 0x0300 in sswindow.h. The executables can therefore
 *  be run in WIN OS/2 2.0 session.
 *
 */

#if defined(SS_W16)

#  define SS_ZMEM
#  define WINDOWS
#  ifndef _WINDOWS
#    define _WINDOWS
#  endif/* _WINDOWS */
#  if defined (SS_DLL) && !defined(SS_EXE)
#    define _WINDLL
#  endif /* SS_DLL */

#elif defined(SS_NT)

#define SS_LARGEFILE

/* Testing Flat Semaphores */
#define SS_FLATSEM /* enable flat+inline mutexes */
#ifdef SS_DEBUG
#  define SS_ZMEM
#else /* SS_DEBUG */
#  define SS_FLATSEM /* enable flat+inline mutexes */
#endif /* SS_DEBUG */

#  define SS_PAGED_MEMALLOC_AVAILABLE
#ifdef SS_NEW_ST
#  define SS_SMALLSYSTEM
#else
#  define SS_MT
#endif

#if !defined(SS_USE_WIN32TLS_API) && defined(SS_MT)
#define SS_USE_WIN32TLS_API
#endif

/* NT has UNICODE main function support */
#define SS_WMAIN_CAPABLE

#elif defined (SS_DOS)

#  define DOS

#elif defined (SS_DOSX)

#  define SS_DOS
#  define DOS

#elif defined (SS_UNIX)

#  define UNIX

#endif

#if defined(SS_LINUX) || defined(SS_SOLARIS) || defined(SS_FREEBSD)
# define IO_OPT
/*
 * # define FSYNC_OPT 
 * (useless since there is existing optimization, syncwrite/synchronizedwrite & fileflush) 
 * */
#endif

/* ---------------------------------------------------------------------
 *          Compiler dependent part
 *
 * You can define SS_NO_ANSI.
 *
 * If SS_NO_ANSI is NOT defined, the default is always SS_ANSI.
 * SS_ANSI implicates ANSI style function prototypes and definitions.
 *
 */
#ifndef SS_NO_ANSI

#  define SS_ANSI

#endif

#if defined(SS_WIN)
#  define SS_ENV_ID     SS_ENVID_W16
#  define SS_ENV_OS     SS_OS_WIN
#  define SS_ENV_OSVERS 3
#  define SS_ENV_CPU    SS_CPU_IX86
#elif defined(SS_NT) && !defined(SS_NT64)
#  define SS_ENV_ID     (SsEnvId())
#  define SS_ENV_OS     (SsEnvOs())
#  define SS_ENV_OSVERS  (SsEnvOsVersion())
   SsEnvIdT SsEnvId(void);
   SsOsIdT SsEnvOs(void);
   int SsEnvOsVersion(void);
#  define SS_ENV_CPU SS_CPU_IX86

#elif defined(SS_NT64)
#  define SS_ENV_ID     SS_ENVID_WNT64
#  define SS_ENV_OS     SS_OS_WNT
#  define SS_ENV_OSVERS  (SsEnvOsVersion())
   SsEnvIdT SsEnvId(void);
   SsOsIdT SsEnvOs(void);
   int SsEnvOsVersion(void);
#  define SS_ENV_CPU    SS_CPU_AMD64
#  define SS_ENV_64BIT

#elif defined(SS_LINUX) && !defined(SS_LINUX_64BIT)
#define SS_LARGEFILE
#ifndef SS_VALGRIND /* MSG_DONTWAIT doesn't seem to work in valgrind */
#define SS_HAS_MSG_DONTWAIT
#endif
# if defined(SS_PPC)
#    define SS_ENV_ID     SS_ENVID_LPX
#    define SS_ENV_CPU    SS_CPU_POWERPC
# else
#    define SS_ENV_ID     SS_ENVID_LUX
#    define SS_ENV_CPU    SS_CPU_IX86
/*#    define ss_int8_t     signed long long int*/
#  endif
#  define SS_ENV_OS     SS_OS_LINUX
#  define SS_ENV_OSVERS 1
#  define SS_PAGED_MEMALLOC_AVAILABLE
#  define SS_NANOSLEEP_THREADSCOPE /* suspends execution of only one thread, not all threads */
#  if defined (SS_MT)
#    define SS_PTHREAD
#    define SS_KERNELTHREADS
#    define SS_GETTIMEOFDAY_AVAILABLE
#  endif /* SS_MT */

#elif defined(SS_LINUX) && defined(SS_LINUX_64BIT)
#define SS_LARGEFILE
#ifndef SS_VALGRIND /* MSG_DONTWAIT doesn't seem to work in valgrind */
#define SS_HAS_MSG_DONTWAIT
#endif
#  define SS_ENV_ID     SS_ENVID_L2X64
#  define SS_ENV_CPU    SS_CPU_AMD64
#  define SS_ENV_OS     SS_OS_LINUX
#  define SS_ENV_OSVERS 1
#  define SS_PAGED_MEMALLOC_AVAILABLE
#  define SS_NANOSLEEP_THREADSCOPE /* suspends execution of only one thread, not all threads */
#  define SS_PTHREAD
#  define SS_KERNELTHREADS
#  define SS_GETTIMEOFDAY_AVAILABLE
#  define SS_ENV_64BIT
#elif defined(SS_DOS)
#  if defined(SS_DOSX)
#    define SS_ENV_ID   SS_ENVID_D4G
#  else
#    define SS_ENV_ID   SS_ENVID_DOS
#  endif
#  define SS_ENV_OS     SS_OS_DOS
#  define SS_ENV_OSVERS 6
#  define SS_ENV_CPU    SS_CPU_IX86
#elif defined(SS_SOLARIS)
#   ifdef IO_OPT
#       define SS_PAGED_MEMALLOC_AVAILABLE
#   endif /* IO_OPT */
#define SS_LARGEFILE
#  if (SS_SOLARIS == 28)
#    define SS_ENV_OSVERS 28
#   if defined(SS_SOLARIS_64BIT)
#    define SS_ENV_ID     SS_ENVID_S8X64BIT
#    define SS_ENV_CPU    SS_CPU_SPARC64
#    define SS_ENV_64BIT
#   else
#    define SS_ENV_ID     SS_ENVID_S8X
#    define SS_ENV_CPU    SS_CPU_SPARC
#   endif
#  elif (SS_SOLARIS == 29)
#    define SS_ENV_OSVERS 29
#   if defined(SS_SOLARIS_64BIT)
#    define SS_ENV_ID     SS_ENVID_S9X64BIT
#    define SS_ENV_CPU    SS_CPU_SPARC64
#    define SS_ENV_64BIT
#   else
#    define SS_ENV_ID     SS_ENVID_S9X
#    define SS_ENV_CPU    SS_CPU_SPARC
#   endif
#  elif (SS_SOLARIS == 210)
#    define SS_ENV_OSVERS 210
#    if defined(SS_SPARC)
#     if defined(SS_SOLARIS_64BIT)
#      define SS_ENV_ID     SS_ENVID_S0X64BIT
#      define SS_ENV_CPU    SS_CPU_SPARC64
#      define SS_ENV_64BIT
#     else
#      define SS_ENV_ID     SS_ENVID_S0X
#      define SS_ENV_CPU    SS_CPU_SPARC
#     endif
#    else
#     if defined(SS_SOLARIS_64BIT)
#      define SS_ENV_ID     SS_ENVID_S0XI64BIT
#      define SS_ENV_CPU    SS_CPU_AMD64
#      define SS_ENV_64BIT
#     else
#      define SS_ENV_ID     SS_ENVID_S0XI
#      define SS_ENV_CPU    SS_CPU_IX86
#     endif
#    endif
#  else
#    define SS_ENV_ID     SS_ENVID_SSX
#    define SS_ENV_OSVERS 2
#  endif
#  define SS_ENV_OS     SS_OS_SOLARIS
#  define SS_NANOSLEEP_THREADSCOPE /* suspends execution of only one thread, not all threads */
#  if defined (SS_MT)
#    define SS_GETTIMEOFDAY_AVAILABLE
#    define SS_PTHREAD
#  endif /* SS_MT */
#elif defined(SS_FREEBSD)
#    define MAP_ANONYMOUS MAP_ANON
#    define MAP_ANON 	0x1000
#    define SS_PAGED_MEMALLOC_AVAILABLE
#    define SS_LARGEFILE
#    ifdef SS_MYSQL
#      define SS_PTHREAD
#    endif
#    define SS_ENV_OSVERS 8
#    if defined(SS_FEX_64BIT)
#       define SS_GETTIMEOFDAY_AVAILABLE
#       define SS_NANOSLEEP_THREADSCOPE
#       define SS_ENV_ID     SS_ENVID_FEX64
#       define SS_ENV_64BIT
#    else
#       define SS_GETTIMEOFDAY_AVAILABLE
#       define SS_NANOSLEEP_THREADSCOPE
#       define SS_ENV_ID     SS_ENVID_FEX
#    endif
#  define SS_ENV_OS     SS_OS_FREEBSD
#  if defined(SS_AMD64)
#       define SS_ENV_CPU    SS_CPU_AMD64
#  else
#       define SS_ENV_CPU    SS_CPU_IX86
#  endif

#else
#  error Hogihogi!
#endif

#if !defined(SS_ENV_ID)
#  error Hogihogi!
#endif

#define SS_ENV_OSNAME (SsEnvNameCurr())

char* SsEnvName(int cpu, int os, int osvers, char* outbuf, int bufsize);
char* SsEnvLicenseName(int cpu, int os, int osvers, char* outbuf, int bufsize);
const char* SsEnvCpuName(int cpu);
const char* SsEnvOsName(int os);
char* SsEnvNameCurr(void);
int   SsEnvOsversFull(int* p_major, int* p_minor);
/* char* SsEnvOsversString(void); */

/* 3-letter tokens (w32, o32, nta, etc) for Dll/lib names etc */
char*    SsEnvTokenById(SsEnvIdT envid);
SsEnvIdT SsEnvIdByToken(const char* token);
char*    SsEnvTokenCurr(void);

char*    SsHostname(void);

#if defined(SS_MT)
# define DBE_MTFLUSH /* Enables Multi-threaded flushing of db files */
# define DBE_GROUPCOMMIT /* Enables group commit in MT environments */

# if defined(SS_UNIX)
#  if defined(SS_LINUX) /* listio has been deemed working only on linux so far. */
/* #   define SS_NATIVE_LISTIO_AVAILABLE -- disabled by apl 2006-05-26 */
#   define SS_NATIVE_LISTIO_AVAILABLE 
#  endif /* SS_LINUX */
#  if !defined(SS_DLLQMEM)
#   define SS_QMEM_THREADCTX
#  endif /* SS_DLLQMEM */

#  if !defined(SS_PTHREAD)
#   error "SS_PTHREAD should be defined here!"
#  endif
# endif

#endif

#if (defined(SS_NT) && (defined(SS_MYSQL) || defined(SS_MYSQL_AC))) || defined(SS_LINUX)
#define SS_USE_INLINE
#endif

#ifdef SS_USE_INLINE

#if defined(SS_NT)
#define SS_INLINE       __inline
#elif defined(SS_SOLARIS)
#define SS_INLINE       inline
#elif defined(SS_UNIX)
#define SS_INLINE       static inline
#endif

#else /* SS_USE_INLINE */

#define SS_INLINE

#endif /* SS_USE_INLINE */

/* ---------------------------------------------------------------------
 *          Light version
 *
 */

#ifdef SS_LIGHT

#define SS_NOBACKUP         /* Database backup. */
#define SS_NOBLOB           /* Blob support. */
#define SS_NOLOCKING        /* Locking support. */
#define SS_NOLOGGING        /* Recovery log support. */
#define SS_NOMMINDEX        /* Main memory index. */
#define SS_NOREPLICATION    /* Replication. */
#define SS_NOTRXREADCHECK   /* No read checks in transactions. */
#define SS_NODDUPDATE       /* Data dictionary updates. */
#define SS_NOSQL            /* SQL support. */
#define SS_NOVIEW           /* Views. */
#define SS_NOEVENT          /* Events. */
#define SS_NOPROCEDURE      /* Procedures */
#define SS_NOSEQUENCE       /* Sequences. */
#define SS_NOERRORTEXT      /* Error texts. */
#define SS_NOCOLLATION      /* Special collations. */
#define SS_NOESTSAMPLES     /* Estimator samples for atributes. */

#endif /* SS_LIGHT */

#ifndef SS_UNICODE_DATA
# ifdef SS_DEBUG
/* #  define SS_UNICODE_DATA */
# endif /* SS_DEBUG */
#endif /* !SS_UNICODE_DATA */

#ifdef SS_UNICODE_SQL
#define SS_CONNECTWITHPROLI
#define SS_PREPAREWITHPROLI
#define SS_CLIPREPAREWITHPROLI
#define SS_EXECWITHPROLI
#define SS_EXECBATCHWITHPROLI
#define SS_CLIEXECWITHPROLI
#endif /* SS_UNICODE_SQL */

#if defined(SS_PMEM) && !defined(SS_PAGED_MEMALLOC_AVAILABLE)
/* Pmem requires Paged Memalloc availability! */
#define SS_PAGED_MEMALLOC_AVAILABLE
#endif /* SS_PMEM */

/*#define SS_CATALOGSUPP*/

#ifdef SS_MME
/* These are now defaults for MME
   apl 2002-12-23 */
#define MME_SEARCH_KEEP_TVAL
#define MME_ALTERNATE_CARDINALITY
#define MME_ALTERNATE_CARDINALITY_FIX
/* This is now default for MME
   apl 2003-02-08 */
#define MME_RVAL_INDEXING
/* This is now default for MME
   apl 2003-02-13 */
#define DBE_LOCKESCALATE_OPT
/* These are now default for MME
   apl 2003-03-02 */
#define SS_TIMER_MSEC
#define DBE_LOCK_MSEC
/* This is now default for MME
   apl 2003-03-18 */
#define MME_LIKE_PREFIX
/* This is now default for MME
   apl 2003-05-21 */
#define SS_MIXED_REFERENTIAL_INTEGRITY
/* This is now default for MME
   apl 2004-01-22 */
#define MME_LOCK_FIX
/* This is now default for MME
   apl 2004-11-16 */
#define MME_CP_FIX

/* Possibility to configure the number of IO-threads per device.
   tommiv 2003-1-30 */
#if defined(SS_MT)
#ifndef SS_NTHREADS_PER_DEVQUEUE
#define SS_NTHREADS_PER_DEVQUEUE
#endif /* SS_NTHREADS_PER_DEVQUEUE */
#endif /* SS_MT */

#ifndef SS_DEBUG
#define RS_USE_MACROS
#endif

#endif /* SS_MME */

#if !defined(SS_NEW_ST)
/* checkpoint page flushing optimizations */
#  define DBE_NONBLOCKING_PAGEFLUSH
#  define DBE_NONBLOCKING_PAGEFLUSH_AVOIDCOPY
#endif /* !SS_NEW_ST */

#if defined(SS_PTHREAD) && !defined(SS_DEBUG) && !defined(SS_FLATSEM)
/* flattened+inlined mutex calls available for PTHREAD systems */
#define SS_FLATSEM
#endif /* SS_PTHREAD && !SS_DEBUG && !SS_FLATSEM */

#endif /* SSENV_H */
