/*************************************************************************\
**  source       * ssmyenv.h
**  directory    * ss
**  description  * OS and environment specific compiler options
**               * and definitions for MySQL compilation.
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


#ifndef SSMYENV_H
#define SSMYENV_H

#include <mysql_version.h>

/* Innodb Code cleanup for handler */
#define OK(expr)                \
        if ((expr) != 0) {      \
                DBUG_RETURN(1); \
        }


#define SS_MYSQL
#define SS_COLLATION
#define FOREIGN_KEY_CHECKS_SUPPORTED

#ifndef SS_SERVER_ENTER
#define SS_SERVER_ENTER ""
#endif

#ifndef SS_SOLIDDB_SERVER_VERSION
#define SS_SOLIDDB_SERVER_VERSION MYSQL_SERVER_VERSION
#endif


#if MYSQL_VERSION_ID >= 50100 && !defined WIN32 && !defined(_WINDOWS)
#include <config.h>
#endif

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

#ifdef TARGET_OS_LINUX
#ifndef SS_LINUX
#define SS_LINUX
#endif
#endif

/* Uncomment for show soliddb mutex support */
#define SS_PROFILE 

/* Uncomment for solidDB mutex wait profiling */
#define SS_SEMPROFILE_DEFAULT

#if defined(DBUG_ON) || defined(_DEBUG)
#ifndef SS_DEBUG
#define SS_DEBUG
#endif
#endif /* DBUG_ON or _DEBUG */

#ifdef SS_UNIX
#ifndef UNIX
#define UNIX
#endif
#ifndef SS_LINUX
#define SS_LINUX
#endif
#endif

#ifndef SS_MT
#define SS_MT
#endif

#ifndef SS_LICENSEINFO_V3
#define SS_LICENSEINFO_V3
#endif

#ifndef SS_UNICODE_ALL
#define SS_UNICODE_ALL
#endif

#ifndef SS_DEBUG
#ifndef RS_USE_MACROS
#define RS_USE_MACROS
#endif
#endif

#define DBE_NOMME

#ifndef DBE_BNODE_MISMATCHARRAY
# define DBE_BNODE_MISMATCHARRAY
#endif

#if defined(WIN32) || defined(_WINDOWS)

#ifndef MSVC7_NT
#define MSVC7_NT
#endif

#ifndef WIN_NT
#define WIN_NT
#endif  

#ifndef SS_NT
#define SS_NT
#endif

#endif

#ifndef SS_LOCALSERVER
#define SS_LOCALSERVER
#endif


#ifndef SS_SYNC
#define SS_SYNC
#endif

#ifndef SS_MME
#define SS_MME
#endif

#ifndef SS_NOMMINDEX
#define SS_NOMMINDEX
#endif

#ifndef SS_VERS_DLL
#define SS_VERS_DLL 0
#endif

#ifdef WIN32		/* A Microsoft Windows system is in use */

#ifndef MSC_NT
#define MSC_NT
#endif

#ifndef SS_DLL
#define SS_DLL
#endif

#else /* Assume Linux and Intel. TODO How to detect other platforms and processors. */

/* #define SS_LINUX  */
#ifndef SS_UNIX
#define SS_UNIX
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS   64
#endif
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE       500
#endif
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _REENTRANT
#define _REENTRANT
#endif

/* The following should work with GCC. */
#ifdef __LP64__
#define SS_LINUX_64BIT
#define UNIX_64BIT
#endif

#endif /* platform */

#endif /* SSMYENV_H */
