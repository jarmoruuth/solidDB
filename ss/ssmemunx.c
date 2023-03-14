/*************************************************************************\
**  source       * ssmemunx.c
**  directory    * ss
**  description  * Memory page routines for UNIXes
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

The UNIX system must have mmap() and munmap() available to enable
freeing pages back to the operating system.
The SsMemPageRealloc() is more efficient if also mremap() is
available. Currently such function is only available in Linux.
Currently the support for MAP_ANONYMOUS is also required, but this
can be, if needed, circumvented using /dev/null file handle instead

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

#include "ssc.h"
#include "sslimits.h"
#include "ssdebug.h"
#include "ssmempag.h"
#include "ssstring.h"

#if defined(SS_UNIX) && defined(SS_PAGED_MEMALLOC_AVAILABLE)

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if defined(SS_LINUX) && !defined(__USE_GNU)
/* needed to enable MREMAP_MAYMOVE definition */
#define __USE_GNU
#endif /* SS_LINUX && !__USE_GNU */
#include <sys/mman.h>

static size_t page_size = 0;

#define _PAGESIZE_ALIGN(s) ((((s) + (page_size-1)) / page_size) * page_size)
#define PAGESIZE_ALIGN(s) \
        (((page_size == 0) ? SsMemPageSize() : 0), _PAGESIZE_ALIGN(s))

void SsMemPageGlobalInit(void)
{
        (void)SsMemPageSize();
}

size_t SsMemPageSize(void)
{
#if defined(_SC_PAGE_SIZE) && !defined(_SC_PAGESIZE)
#  define _SC_PAGESIZE _SC_PAGE_SIZE
#endif /* _SC_PAGE_SIZE with underscore */
        if (page_size == 0) {
            page_size = sysconf(_SC_PAGESIZE);
            if (page_size < 4096) {
                page_size = 4096;
            }
        }
        return (page_size);
}

void* SsMemPageAlloc1(void)
{
        void* p;

        p = SsMemPageAlloc(1);
        return (p);
}

void SsMemPageFree1(void* p)
{
        SsMemPageFree(p,1);
}

void* SsMemPageAlloc(size_t s)
{
        void* p;

        s = PAGESIZE_ALIGN(s);
        p = mmap(
                NULL, 
                s, 
                PROT_READ | PROT_WRITE, 
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1, 0L);
        if ((long)p == -1L) {
            return (NULL);
        }
        return (p);
}

void SsMemPageFree(void* p, size_t s)
{
        if (p != NULL) {
            int r;
            s = PAGESIZE_ALIGN(s);
            r = munmap(p, s);
            ss_rc_assert(r == 0, errno);
        }
}

#ifdef SS_LINUX

void* SsMemPageRealloc(void* p, size_t newsize, size_t oldsize)
{
        if (p == NULL) {
            ss_dassert(oldsize == 0);
            p = SsMemPageAlloc(newsize);
        } else {
            newsize = PAGESIZE_ALIGN(newsize);
            oldsize = PAGESIZE_ALIGN(oldsize);
            if (oldsize != newsize) {
                p = mremap(p, oldsize, newsize, MREMAP_MAYMOVE);
                if ((long)p == -1L) {
                    p = NULL;
                }
            }
        }
        return (p);
}

#else /* SS_LINUX */

void* SsMemPageRealloc(void* p, size_t newsize, size_t oldsize)
{
        if (p == NULL) {
            ss_dassert(oldsize == 0);
            p = SsMemPageAlloc(newsize);
        } else {
            newsize = PAGESIZE_ALIGN(newsize);
            oldsize = PAGESIZE_ALIGN(oldsize);
            if (oldsize != newsize) {
                void* new_p;
                new_p = SsMemPageAlloc(newsize);
                memcpy(new_p, p, SS_MIN(newsize, oldsize));
                SsMemPageFree(p, oldsize);
                p = new_p;
            }
        }
        return (p);
}

#endif /* SS_LINUX */
#endif /* UNIX with SS_PAGED_MEMALLOC_AVAILABLE */
