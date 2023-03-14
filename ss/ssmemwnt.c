/*************************************************************************\
**  source       * ssmemwnt.c
**  directory    * ss
**  description  * Windows memory routines.
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

#if defined (SS_NT)

#include "sswindow.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "ssutiwnt.h"

#ifdef SS_MYSQL
#include <su0error.h>
#else
#include "../su/su0error.h"
#endif

#define ALLOC_TYPEFLAGS         (MEM_RESERVE | MEM_COMMIT)
#define ALLOC_PROTFLAGS         (PAGE_READWRITE)
#define DEALLOC_TYPEFLAGS       (MEM_RELEASE)
#define RESERVE_TYPEFLAGS       (MEM_RESERVE)
#define COMMIT_TYPEFLAGS        (MEM_COMMIT)
#define DECOMMIT_TYPEFLAGS      (MEM_DECOMMIT)
#define DENYACCESS_PROTFLAGS    (PAGE_NOACCESS)
#define ALLOWACCESS_PROTFLAGS   ALLOC_PROTFLAGS

static size_t page_size = 0;

#define _PAGESIZE_ALIGN(s) ((((s) + (page_size-1)) / page_size) * page_size)
#define PAGESIZE_ALIGN(s) \
        (((page_size == 0) ? SsMemPageSize() : page_size), _PAGESIZE_ALIGN(s))

#define GLOBAL_1PAGEPOOLSIZE (200L * 1024L * 1024L)


#define PAGE_ADDRESS_CAPACITY   (page_size / sizeof(ss_uint4_t) - 2)
#define PAGE_NEXTPAGE_OFS       0
#define PAGE_NADDRESS_OFS       1
#define PAGE_ADDRESSARR_OFS     2

static struct {
        SsQsemT* pp_mutex;
        void** pp_freelist;
        uchar* pp_breakvalue;
} page_pool;

static void* pagepool_get1(void)
{
        ss_uint4_t* page;

        page = (ss_uint4_t*)page_pool.pp_freelist;
        ss_dassert(page != NULL);
        if (page[PAGE_NADDRESS_OFS] == 0) {
            page_pool.pp_freelist = (void**)page[PAGE_NEXTPAGE_OFS];
            return ((void*)page);
        } else {
            void* p;
            p = (void*)page[PAGE_ADDRESSARR_OFS + page[PAGE_NADDRESS_OFS] - 1];
            p = VirtualAlloc(
                    p,
                    page_size,
                    COMMIT_TYPEFLAGS,
                    ALLOC_PROTFLAGS);
            if (p != NULL) {
                page[PAGE_NADDRESS_OFS]--;
                return (p);
            }
            return (NULL);
        }
}

static void pagepool_put1(void* p)
{
        ss_uint4_t* page;

        page = (ss_uint4_t*)page_pool.pp_freelist;
        if (page == NULL) {
            page = (ss_uint4_t*)(page_pool.pp_freelist = (void**)p);
            page[PAGE_NADDRESS_OFS] = page[PAGE_NEXTPAGE_OFS] = 0L;
        } else if (page[PAGE_NADDRESS_OFS] == PAGE_ADDRESS_CAPACITY) {
            page = p;
            page[PAGE_NEXTPAGE_OFS] = (ss_uint4_t)page_pool.pp_freelist;
            page[PAGE_NADDRESS_OFS] = 0L;
        } else {
            page[PAGE_ADDRESSARR_OFS + page[PAGE_NADDRESS_OFS]] =
                (ss_uint4_t)p;
            page[PAGE_NADDRESS_OFS]++;
            VirtualFree(p, page_size, DECOMMIT_TYPEFLAGS);
        }

}
void SsMemPageGlobalInit(void)
{
        if (page_pool.pp_mutex != NULL) {
            return;
        }
        page_pool.pp_mutex = malloc(SsQsemSizeLocal());
        SsQsemCreateLocalBuf(page_pool.pp_mutex, SS_SEMNUM_SS_PAGEMEM);
        page_pool.pp_freelist = NULL;
        page_pool.pp_breakvalue =
            VirtualAlloc(
                NULL,
                GLOBAL_1PAGEPOOLSIZE,
                RESERVE_TYPEFLAGS,
                ALLOC_PROTFLAGS);
}

void* SsMemPageAlloc1(void)
{
        void* p;

        SsQsemEnter(page_pool.pp_mutex);
        if (page_pool.pp_freelist == NULL) {
            uchar* brk_tmp;
            brk_tmp = page_pool.pp_breakvalue;
            if ((ulong)brk_tmp % page_size != 0) {
                brk_tmp +=
                    page_size - ((ulong)brk_tmp % page_size);
            }
            brk_tmp = VirtualAlloc(
                        page_pool.pp_breakvalue,
                        page_size,
                        COMMIT_TYPEFLAGS,
                        ALLOC_PROTFLAGS);
            if (brk_tmp != NULL) {
                p = brk_tmp;
                page_pool.pp_breakvalue = brk_tmp + page_size;
            } else {
                p = NULL;
                ss_pprintf_2(("VirtualAlloc failed, error = %d\n", GetLastError()));
                SsErrorMessage(FIL_MSG_VIRTUALALLOC_FAILED_D, GetLastError());
            }
        } else {
            p = pagepool_get1();
        }
        SsQsemExit(page_pool.pp_mutex);
        return (p);
}

void SsMemPageFree1(void* p)
{
        ss_dassert(p != NULL);

        SsQsemEnter(page_pool.pp_mutex);
        pagepool_put1(p);
        SsQsemExit(page_pool.pp_mutex);
}

void* SsMemPageSbrk(long nbytes)
{
        void* p;

        if (nbytes == 0) {
            return (page_pool.pp_breakvalue);
        }
        SsQsemEnter(page_pool.pp_mutex);
        if (nbytes < 0) {
            uchar* base;
            size_t n;

            nbytes = -nbytes;
            base = page_pool.pp_breakvalue - nbytes;
            n = (size_t)((ulong)base % page_size);
            if (n != 0) {
                n = page_size - n;
                base += n;
                if ((long)n > nbytes) {
                    n = 0;
                } else {
                    nbytes -= n;
                }
            }
            if (n != 0) {
                n = (n + (page_size - 1)) / page_size;
                VirtualFree(base, n * page_size, DECOMMIT_TYPEFLAGS);
            }
            p = page_pool.pp_breakvalue;
            page_pool.pp_breakvalue -= nbytes;
        } else {
            p = VirtualAlloc(
                    page_pool.pp_breakvalue,
                    nbytes,
                    COMMIT_TYPEFLAGS,
                    ALLOC_PROTFLAGS);
            if (p == NULL) {
                goto error_return;
            }
            p = page_pool.pp_breakvalue;
            page_pool.pp_breakvalue += nbytes;
        }
        SsQsemExit(page_pool.pp_mutex);
        return (p);
error_return:
        SsQsemExit(page_pool.pp_mutex);
        return ((void*)(ulong)-1L);
}

size_t SsMemPageSize(void)
{
        if (page_size == 0) {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            page_size = si.dwPageSize;
        }
        return (page_size);
}

void* SsMemPageAlloc(size_t s)
{
        void* p;

        p = VirtualAlloc(NULL, s, ALLOC_TYPEFLAGS, ALLOC_PROTFLAGS);
        if (p == NULL) {
            ss_pprintf_2(("VirtualAlloc failed, error = %d\n", GetLastError()));
            SsErrorMessage(FIL_MSG_VIRTUALALLOC_FAILED_D, GetLastError());
        }
        return (p);
}

void SsMemPageFree(void* p, size_t s)
{
        ss_dassert(p != NULL);
        VirtualFree(p, 0, DEALLOC_TYPEFLAGS);
}

void* SsMemPageRealloc(void* p, size_t newsize, size_t oldsize)
{
        size_t old_size;
        MEMORY_BASIC_INFORMATION mi;

        if (p == NULL) {
            p = SsMemPageAlloc(newsize);
            return (p);
        }
        VirtualQuery(p, &mi, sizeof(mi));
        old_size = mi.RegionSize;
        ss_dassert(old_size == PAGESIZE_ALIGN(old_size));
        ss_dassert(PAGESIZE_ALIGN(oldsize) == old_size);
        newsize = PAGESIZE_ALIGN(newsize);
        if (newsize != old_size) {
            void* p2 = SsMemPageAlloc(newsize);
            if (p2 == NULL) {
                return (NULL);
            }
            memcpy(p2, p, SS_MIN(old_size, newsize));
            SsMemPageFree(p, old_size);
            return (p2);
        }
        return (p);
}

#ifdef SS_DEBUG
void SsMemPageDenyAccess(void* p, size_t size)
{
        DWORD oldaccess;
        bool succp;

        ss_dassert(p != NULL);
        succp = VirtualProtect(p, size, DENYACCESS_PROTFLAGS, &oldaccess);
        ss_assert(succp);
}

void SsMemPageAllowAccess(void* p, size_t size)
{
        DWORD oldaccess;
        bool succp;

        ss_dassert(p != NULL);
        succp = VirtualProtect(p, size, ALLOWACCESS_PROTFLAGS, &oldaccess);
        ss_assert(succp);
}
#endif /* SS_DEBUG */

void* SsMemAllocShared(char* name, size_t size)
{
        void* ptr;
        HANDLE hMap;
        HANDLE hFILE = (HANDLE)0xFFFFFFFF;
        LPSECURITY_ATTRIBUTES lpsa;
        DWORD fdwProtect = PAGE_READWRITE;
        DWORD dwMaximumSizeHigh = 0L;
        DWORD dwMaximumSizeLow = size + 2*sizeof(HANDLE);
        DWORD cbMap = 0; /* All */
        char  nm[100];
        HANDLE* p_hnd;

        ss_dassert(name != NULL && strlen(name) > 0);
        strcpy(nm, name);
        strcat(nm, ".SHM");

        lpsa = SsWntACLInit();

        hMap = CreateFileMapping(
                    hFILE,
                    lpsa,
                    fdwProtect,
                    dwMaximumSizeHigh,
                    dwMaximumSizeLow,
                    nm
                );

        SsWntACLDone(lpsa);

        ss_dprintf_1(("SsMemAllocShared: hMap = %ld\n", hMap));

        if (hMap != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
            ss_dprintf_1(("CreateFileMapping failed, code %ld\n",
                          (long)GetLastError()));
            CloseHandle(hMap);
            /* Already exists */
            return(NULL);
        }

        ptr = MapViewOfFile(
                    hMap,
                    FILE_MAP_ALL_ACCESS,
                    (DWORD)0,   /* OffsetHigh */
                    (DWORD)0,   /* OffsetLow */
                    cbMap);

        if (ptr == NULL) {

            ss_dprintf_1(("MapViewOfFile failed, code %ld\n",
                          (long)GetLastError()));
            CloseHandle(hMap);
            return(NULL);

        }

        p_hnd = ptr;
        p_hnd[0] = hMap;
        ptr = (void *)((char *)ptr + 2*sizeof(HANDLE));

        return(ptr);
}

void* SsMemLinkShared(char* name)
{
        void* ptr;
        HANDLE hMap;
        DWORD cbMap = 0; /* All */
        char nm[100];
        HANDLE* p_hnd;

        ss_dassert(name != NULL && strlen(name) > 0);
        strcpy(nm, name);
        strcat(nm, ".SHM");  
        hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, nm);
        if (hMap == NULL) {
            /* Already exists */
            ss_dprintf_1(("MapViewOfFile failed, code %ld\n",
                          (long)GetLastError()));
            return(NULL);
        }
        ptr = MapViewOfFile(
                    hMap,
                    FILE_MAP_ALL_ACCESS,
                    (DWORD)0,   /* OffsetHigh */
                    (DWORD)0,   /* OffsetLow */
                    cbMap);

        if (ptr == NULL) {

            ss_dprintf_1(("MapViewOfFile failed, code %ld\n",
                          (long)GetLastError()));
            CloseHandle(hMap);
            return(NULL);
        }

        p_hnd = ptr;
        p_hnd[1] = hMap;
        ss_dprintf_1(("MapViewOfFile success, hMap = %lu\n",
                        hMap));
        ptr = (void *)((char *)ptr + 2*sizeof(HANDLE));

        return(ptr);
}

void  SsMemFreeShared(void* ptr)
{
        HANDLE* p_hnd;
        HANDLE  hMap;

        p_hnd = ptr;
        hMap = p_hnd[-2];
        UnmapViewOfFile(p_hnd - 2);
        ss_dprintf_1(("SsMemFreeShared: hMap = %ld\n", hMap));
        CloseHandle(hMap);
}

void  SsMemUnlinkShared(void* ptr)
{
        HANDLE* p_hnd;
        HANDLE  hMap;
        p_hnd = ptr;
        hMap = p_hnd[-1];
        UnmapViewOfFile(p_hnd - 2);
        ss_dprintf_1(("SsMemUnlinkShared: hMap = %ld\n", hMap));
        CloseHandle(hMap);
}

#endif /* NT */
