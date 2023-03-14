/*************************************************************************\
**  source       * ssmemchk.c
**  directory    * ss
**  description  * Memory check routines.
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

#ifdef SS_DEBUG

#include "ssstdio.h"
#include "ssstring.h"
#include "ssstddef.h"
#include "ssstdlib.h"
#include "sslimits.h"

#include "ssc.h"
#include "ssdebug.h"
#include "sssem.h"
#include "ssqmem.h"
#include "ssmem.h"
#include "ssqmem.h"
#include "ssmemchk.h"
#include "ssthread.h"
#include "sssprint.h"
#include "ssint8.h"
#include "sstlog.h"

#if defined(SSMEM_TRACE)
#include "ssmemtrc.h"
#endif

extern int      ss_memdebug;            /* in ssdebug.c */
extern int      ss_memdebug_segalloc;   /* in ssdebug.c */
extern SsSemT*  ss_memchk_sem;          /* in ssmem.c */

typedef struct ss_alloc_info_st ss_alloc_info_t;

struct ss_alloc_info_st {
        ss_int8_t           ai_totalcount;
        ss_int8_t           ai_totalsize;
        long                ai_curcount;
        long                ai_cursize;
        long                ai_maxcount;
        long                ai_maxsize;
        char*               ai_file;
        int                 ai_line;
        char**              ai_callstack;
        int                 ai_callstacklen;
        ss_alloc_info_t*    ai_next;
        int                 ai_chk;
};

#ifdef SS_SMALLSYSTEM
#define SS_MEMHASH_SIZE     10007
#else
#define SS_MEMHASH_SIZE     1000003
#endif
#define SS_MEMHASH_VAL(p)   ((uint)((ulong)p >> 2L) % SS_MEMHASH_SIZE)

#define SS_ALLOCINFOHASH_SIZE     SS_MEMHASH_SIZE
#define SS_ALLOCINFO_CHK          728765

static SsMemChkListT* ss_memhash[SS_MEMHASH_SIZE];
static long           alloc_timecounter = 0;

static ss_alloc_info_t* allocinfo_hashtable[SS_ALLOCINFOHASH_SIZE];

/* Status parameters for MemChkGetListFirst/MemChkGetListNext
 */
static int            listscan_pos;
static SsMemChkListT* listscan_item;

bool memchk_disableprintlist;

ulong memchk_nptr;
ulong memchk_maxptr;
ulong memchk_alloc;
ulong memchk_calloc;
ulong memchk_realloc;
ulong memchk_free;
ulong memchk_strdup;
ulong memchk_bytes;
ulong memchk_maxbytes;

long memchk_newborn = MEMCHK_DEFAULT_NEWBORN; /* Newborn memory check value. */

static bool callstk_compare(char** cs1, int len1, char** cs2, int len2)
{
        if (len1 != len2) {
            return(FALSE);
        }
        return(memcmp(cs1, cs2, len1 * sizeof(cs1[0])) == 0);
}

static int hash_filename(char* filename, int line)
{
        return((ss_memtrc_hashpjw(filename) + line) % SS_ALLOCINFOHASH_SIZE);
}

static ss_alloc_info_t* MemChkAllocInfoFind(
        char*    file,
        int      line)
{
        int h;
        int len;
        ss_alloc_info_t* ai;
        ss_alloc_info_t* prev_ai = NULL;
        char* callstk[255];

        SsMemTrcGetFunctionStk(callstk, &len);
        h = hash_filename(file, line);
        ai = allocinfo_hashtable[h];
        for (; ai != NULL; ai = ai->ai_next) {
            ss_dassert(ai->ai_chk == SS_ALLOCINFO_CHK);
            if (ai->ai_line == line
            &&  strcmp(ai->ai_file, file) == 0
            &&  callstk_compare(ai->ai_callstack, ai->ai_callstacklen, callstk, len))
            {
                /* Found. */
                if (prev_ai != NULL) {
                    /* Move to the head of list. */
                    prev_ai->ai_next = ai->ai_next;
                    ai->ai_next = allocinfo_hashtable[h];
                    allocinfo_hashtable[h] = ai;
                }
                break;
            }
            prev_ai = ai;
        }
        return(ai);
}

static ss_alloc_info_t* MemChkAllocInfoInc(
        char*    file,
        int      line,
        size_t   size)
{
        ss_alloc_info_t* ai;

        ai = MemChkAllocInfoFind(file, line);

        if (ai == NULL) {
            int h;
            int len;
            char* callstk[255];

            h = hash_filename(file, line);
            SsMemTrcGetFunctionStk(callstk, &len);

            ai = malloc(sizeof(ss_alloc_info_t));
            ss_assert(ai != NULL);
            SsInt8Set2Uint4s(&ai->ai_totalcount, 0, 1);
            SsInt8Set2Uint4s(&ai->ai_totalsize, 0, size);
            ai->ai_curcount = 1;
            ai->ai_cursize = size;
            ai->ai_maxcount = 1;
            ai->ai_maxsize = size;
            ai->ai_file = file;
            ai->ai_line = line;
            ai->ai_callstack = malloc((len + 1) * sizeof(callstk[0]));
            ss_assert(ai->ai_callstack != NULL);
            memcpy(ai->ai_callstack, callstk, (len + 1) * sizeof(callstk[0]));
            ai->ai_callstacklen = len;
            ai->ai_next = allocinfo_hashtable[h];
            ai->ai_chk = SS_ALLOCINFO_CHK;

            allocinfo_hashtable[h] = ai;

        } else {
            ss_dassert(ai->ai_chk == SS_ALLOCINFO_CHK);
            SsInt8AddUint4(&ai->ai_totalcount, ai->ai_totalcount, 1);
            SsInt8AddUint4(&ai->ai_totalsize, ai->ai_totalsize, size);
            ai->ai_curcount++;
            ai->ai_cursize += size;
            ai->ai_maxcount = SS_MAX(ai->ai_maxcount, ai->ai_curcount);
            ai->ai_maxsize = SS_MAX(ai->ai_maxsize, ai->ai_cursize);
        }
        return(ai);
}

static void MemChkAllocInfoDec(
        SsMemChkListT* list)
{
        ss_alloc_info_t* ai;

        ai = list->memlst_allocinfo;
        ss_dassert(ai != NULL);
        ss_dassert(ai->ai_line == list->memlst_line);
        ss_dassert(ai->ai_chk == SS_ALLOCINFO_CHK);

        ai->ai_cursize -= list->memlst_size;
        ai->ai_curcount--;

        ss_dassert(ai->ai_cursize >= 0);
        ss_dassert(ai->ai_curcount >= 0);

        list->memlst_allocinfo = NULL;
}

static int qsort_longcmp(long l1, long l2)
{
        if (l1 < l2) {
            return(-1);
        } else if (l1 > l2) {
            return(1);
        } else {
            return(0);
        }
}

static int SS_CLIBCALLBACK totalcount_cmp(
        const void* p_st1,
        const void* p_st2)
{
        const ss_alloc_info_t* ai1 = *(ss_alloc_info_t**)p_st1;
        const ss_alloc_info_t* ai2 = *(ss_alloc_info_t**)p_st2;

        ss_dassert(ai1->ai_chk == SS_ALLOCINFO_CHK);
        ss_dassert(ai2->ai_chk == SS_ALLOCINFO_CHK);

        return(SsInt8Cmp(ai1->ai_totalcount, ai2->ai_totalcount));
}

static int SS_CLIBCALLBACK totalsize_cmp(
        const void* p_st1,
        const void* p_st2)
{
        const ss_alloc_info_t* ai1 = *(ss_alloc_info_t**)p_st1;
        const ss_alloc_info_t* ai2 = *(ss_alloc_info_t**)p_st2;

        ss_dassert(ai1->ai_chk == SS_ALLOCINFO_CHK);
        ss_dassert(ai2->ai_chk == SS_ALLOCINFO_CHK);

        return(SsInt8Cmp(ai1->ai_totalsize, ai2->ai_totalsize));
}

static int SS_CLIBCALLBACK curcount_cmp(
        const void* p_st1,
        const void* p_st2)
{
        const ss_alloc_info_t* ai1 = *(ss_alloc_info_t**)p_st1;
        const ss_alloc_info_t* ai2 = *(ss_alloc_info_t**)p_st2;

        ss_dassert(ai1->ai_chk == SS_ALLOCINFO_CHK);
        ss_dassert(ai2->ai_chk == SS_ALLOCINFO_CHK);

        return(qsort_longcmp(ai1->ai_curcount, ai2->ai_curcount));
}

static int SS_CLIBCALLBACK cursize_cmp(
        const void* p_st1,
        const void* p_st2)
{
        const ss_alloc_info_t* ai1 = *(ss_alloc_info_t**)p_st1;
        const ss_alloc_info_t* ai2 = *(ss_alloc_info_t**)p_st2;

        ss_dassert(ai1->ai_chk == SS_ALLOCINFO_CHK);
        ss_dassert(ai2->ai_chk == SS_ALLOCINFO_CHK);

        return(qsort_longcmp(ai1->ai_cursize, ai2->ai_cursize));
}

static int SS_CLIBCALLBACK maxcount_cmp(
        const void* p_st1,
        const void* p_st2)
{
        const ss_alloc_info_t* ai1 = *(ss_alloc_info_t**)p_st1;
        const ss_alloc_info_t* ai2 = *(ss_alloc_info_t**)p_st2;

        ss_dassert(ai1->ai_chk == SS_ALLOCINFO_CHK);
        ss_dassert(ai2->ai_chk == SS_ALLOCINFO_CHK);

        return(qsort_longcmp(ai1->ai_maxcount, ai2->ai_maxcount));
}

static int SS_CLIBCALLBACK maxsize_cmp(
        const void* p_st1,
        const void* p_st2)
{
        const ss_alloc_info_t* ai1 = *(ss_alloc_info_t**)p_st1;
        const ss_alloc_info_t* ai2 = *(ss_alloc_info_t**)p_st2;

        ss_dassert(ai1->ai_chk == SS_ALLOCINFO_CHK);
        ss_dassert(ai2->ai_chk == SS_ALLOCINFO_CHK);

        return(qsort_longcmp(ai1->ai_maxsize, ai2->ai_maxsize));
}

static void MemChkAllocInfoPrintTable(void* fp, int size, char* name, ss_alloc_info_t** tab)
{
        int i;
        int j;
        ss_alloc_info_t* ai;

        SsFprintf(fp, "***** BEGIN %s *****\n", name);
        SsFprintf(fp, "  %-16s %-5s %-10s %-10s %-8s %-8s %-8s %-8s\n",
            "file",
            "line",
            "totalcount",
            "totalsize",
            "curcount",
            "cursize",
            "maxcount",
            "maxsize");
        for (i = size-1; i >= 0; i--) {
            ai = tab[i];
            if (ai != NULL) {
                char totalcount[80];
                char totalsize[80];
                SsInt8ToAscii(ai->ai_totalcount, totalcount, 10, 0, 0, FALSE);
                SsInt8ToAscii(ai->ai_totalsize, totalsize, 10, 0, 0, FALSE);
                ss_dassert(ai->ai_chk == SS_ALLOCINFO_CHK);
                SsFprintf(fp, "  %-16s %-5d %-10s %-10s %-8ld %-8ld %-8ld %-8ld %s\n",
                    ai->ai_file,
                    ai->ai_line,
                    totalcount,
                    totalsize,
                    ai->ai_curcount,
                    ai->ai_cursize,
                    ai->ai_maxcount,
                    ai->ai_maxsize,
                    ai->ai_callstack[0] != NULL ? ai->ai_callstack[0] : "no stack");
                for (j = 1; j < ai->ai_callstacklen; j++) {
                    SsFprintf(fp, "%82s %s\n", "", ai->ai_callstack[j]);
                }
            }
        }
        SsFprintf(fp, "  %-16s %-5s %-10s %-10s %-8s %-8s %-8s %-8s\n",
            "file",
            "line",
            "totalcount",
            "totalsize",
            "curcount",
            "cursize",
            "maxcount",
            "maxsize");
        SsFprintf(fp, "***** END %s *****\n", name);
}

void SsMemChkAllocInfoPrint(void* fp, int size, char* prefix)
{
        int i;
        ss_alloc_info_t* ai;
        ss_alloc_info_t** totalcount;
        ss_alloc_info_t** totalsize;
        ss_alloc_info_t** curcount;
        ss_alloc_info_t** cursize;
        ss_alloc_info_t** maxcount;
        ss_alloc_info_t** maxsize;
        int count = 0;
        int prefixlen;

        if (ss_memdebug < 2) {
            return;
        }

        SsSemEnter(ss_memchk_sem);

        totalcount = calloc(size, sizeof(ss_alloc_info_t*));
        totalsize = calloc(size, sizeof(ss_alloc_info_t*));
        curcount = calloc(size, sizeof(ss_alloc_info_t*));
        cursize = calloc(size, sizeof(ss_alloc_info_t*));
        maxcount = calloc(size, sizeof(ss_alloc_info_t*));
        maxsize = calloc(size, sizeof(ss_alloc_info_t*));

        if (prefix != NULL && *prefix) {
            prefixlen = strlen(prefix);
        }

        for (i = 0, count = 0; i < SS_ALLOCINFOHASH_SIZE; i++) {
            ai = allocinfo_hashtable[i];
            for (; ai != NULL; ai = ai->ai_next) {
                ss_dassert(ai->ai_chk == SS_ALLOCINFO_CHK);
                if (prefix != NULL && *prefix) {
                    int j;
                    bool found = FALSE;
                    for (j = 0; j < ai->ai_callstacklen; j++) {
                        if (strncmp(ai->ai_callstack[j], prefix, prefixlen) == 0) {
                            found = TRUE;
                            break;
                        }
                    }
                    if (!found) {
                        continue;
                    }
                }
                if (count < size || SsInt8Cmp(ai->ai_totalcount, totalcount[0]->ai_totalcount) > 0) {
                    if (count < size) {
                        totalcount[count] = ai;
                    } else {
                        totalcount[0] = ai;
                        qsort(
                            totalcount,
                            size,
                            sizeof(totalcount[0]),
                            totalcount_cmp);
                    }
                }
                if (count < size || SsInt8Cmp(ai->ai_totalsize, totalsize[0]->ai_totalsize) > 0) {
                    if (count < size) {
                        totalsize[count] = ai;
                    } else {
                        totalsize[0] = ai;
                        qsort(
                            totalsize,
                            size,
                            sizeof(totalsize[0]),
                            totalsize_cmp);
                    }
                }
                if (count < size || ai->ai_curcount > curcount[0]->ai_curcount) {
                    if (count < size) {
                        curcount[count] = ai;
                    } else {
                        curcount[0] = ai;
                        qsort(
                            curcount,
                            size,
                            sizeof(curcount[0]),
                            curcount_cmp);
                    }
                }
                if (count < size || ai->ai_cursize > cursize[0]->ai_cursize) {
                    if (count < size) {
                        cursize[count] = ai;
                    } else {
                        cursize[0] = ai;
                        qsort(
                            cursize,
                            size,
                            sizeof(cursize[0]),
                            cursize_cmp);
                    }
                }
                if (count < size || ai->ai_maxcount > maxcount[0]->ai_maxcount) {
                    if (count < size) {
                        maxcount[count] = ai;
                    } else {
                        maxcount[0] = ai;
                        qsort(
                            maxcount,
                            size,
                            sizeof(maxcount[0]),
                            maxcount_cmp);
                    }
                }
                if (count < size || ai->ai_maxsize > maxsize[0]->ai_maxsize) {
                    if (count < size) {
                        maxsize[count] = ai;
                    } else {
                        maxsize[0] = ai;
                        qsort(
                            maxsize,
                            size,
                            sizeof(maxsize[0]),
                            maxsize_cmp);
                    }
                }
                count++;
                if (count == size) {
                    qsort(
                        totalcount,
                        size,
                        sizeof(totalcount[0]),
                        totalcount_cmp);
                    qsort(
                        totalsize,
                        size,
                        sizeof(totalsize[0]),
                        totalsize_cmp);
                    qsort(
                        curcount,
                        size,
                        sizeof(curcount[0]),
                        curcount_cmp);
                    qsort(
                        cursize,
                        size,
                        sizeof(cursize[0]),
                        cursize_cmp);
                    qsort(
                        maxcount,
                        size,
                        sizeof(maxcount[0]),
                        maxcount_cmp);
                    qsort(
                        maxsize,
                        size,
                        sizeof(maxsize[0]),
                        maxsize_cmp);
                }
            }
        }
        if (count < size) {
            qsort(
                totalcount,
                count,
                sizeof(totalcount[0]),
                totalcount_cmp);
            qsort(
                totalsize,
                count,
                sizeof(totalsize[0]),
                totalsize_cmp);
            qsort(
                curcount,
                count,
                sizeof(curcount[0]),
                curcount_cmp);
            qsort(
                cursize,
                count,
                sizeof(cursize[0]),
                cursize_cmp);
            qsort(
                maxcount,
                count,
                sizeof(maxcount[0]),
                maxcount_cmp);
            qsort(
                maxsize,
                count,
                sizeof(maxsize[0]),
                maxsize_cmp);
        }

        MemChkAllocInfoPrintTable(fp, size, (char *)"totalcount", totalcount);
        MemChkAllocInfoPrintTable(fp, size, (char *)"totalsize", totalsize);
        MemChkAllocInfoPrintTable(fp, size, (char *)"curcount", curcount);
        MemChkAllocInfoPrintTable(fp, size, (char *)"cursize", cursize);
        MemChkAllocInfoPrintTable(fp, size, (char *)"maxcount", maxcount);
        MemChkAllocInfoPrintTable(fp, size, (char *)"maxsize", maxsize);

        SsSemExit(ss_memchk_sem);
}

/*##*********************************************************************\
 *
 *		SsMemChkMessage
 *
 * Displays a message in standard format
 *
 * Parameters :
 *
 *	msg - in, use
 *		message string
 *
 *	file - in, use
 *		source file name
 *
 *	line - in
 *		source line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkMessage(
        char* msg,
        char* file,
        int   line)
{
        static char buf[255];

        SsSprintf(buf, "SsMemChk: %.200s, file %.30s, line %d\n", msg, file, line);

        SsAssertionMessage(buf, file, line);
}

/*#***********************************************************************\
 *
 *		SsMemChkMessageEx
 *
 * Extended version to print memory check info. Prints also call stack
 * to the pointer.
 *
 * Parameters :
 *
 *	msg -
 *
 *
 *	list -
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
static void SsMemChkMessageEx(
        char* msg,
        SsMemChkListT* list)
{
        SsMemChkMessage(msg, list->memlst_file, list->memlst_line);
#if defined(SSMEM_TRACE)
        SsMemTrcPrintCallStk(list->memlst_callstk);
        SsSQLTrcInfoPrint(list->memlst_sqltrcinfo);
#endif
}

/*#***********************************************************************\
 *
 *		SsMemChkAbortEx
 *
 * Extended version of memory check abort. Prints also extra info like
 * call stack.
 *
 * Parameters :
 *
 *	msg -
 *
 *
 *	list -
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
static void SsMemChkAbortEx(
        char* msg,
        SsMemChkListT* list)
{
        SsMemChkMessageEx(msg, list);
        SsErrorExit();
}

/*##*********************************************************************\
 *
 *		SsMemChkAbort
 *
 * Displays a message in standard format and exists the progam after
 * a memory check error.
 *
 * Parameters :
 *
 *	msg - in, use
 *		message string
 *
 *	file - in, use
 *		source file name
 *
 *	line - in
 *		source line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkAbort(
        char* msg,
        char* file,
        int   line)
{
        SsMemChkMessage(msg, file, line);
        SsErrorExit();
}

/*#***********************************************************************\
 *
 *		MemChkGetListFirst
 *
 * Returns pointer to the descriptor of first allocated memory area.
 *
 * Parameters :
 *
 * Return value - ref :
 *
 *      pointer to the first memory area descriptor
 *
 * Limitations  :
 *
 *      Not reentrant.
 *
 * Globals used :
 */
static SsMemChkListT* MemChkGetListFirst(void)
{
        listscan_pos = 0;

        while (listscan_pos < SS_MEMHASH_SIZE) {
            listscan_item = ss_memhash[listscan_pos];
            while (listscan_item != NULL) {
                if (listscan_item->memlst_ptr != NULL) {
                    return(listscan_item);
                }
                listscan_item = listscan_item->memlst_next;
            }
            listscan_pos++;
        }
        return(NULL);
}

/*#***********************************************************************\
 *
 *		MemChkGetListNext
 *
 * Returns pointer to the descriptor of next allocated memory area.
 *
 * Parameters :
 *
 * Return value - ref :
 *
 *      pointer to the next memory area descriptor, or NULL at end
 *
 * Limitations  :
 *
 *      Not reentrant.
 *
 * Globals used :
 */
static SsMemChkListT* MemChkGetListNext(void)
{
        /* move to the next item */
        if (listscan_item != NULL) {
            listscan_item = listscan_item->memlst_next;
        }

        /* try to find from current bucket list (listscan_item) */
        while (listscan_item != NULL) {
            if (listscan_item->memlst_ptr != NULL) {
                return(listscan_item);
            }
            listscan_item = listscan_item->memlst_next;
        }

        /* move to the next bucket */
        listscan_pos++;

        /* try to find next bucket with allocated pointer */
        while (listscan_pos < SS_MEMHASH_SIZE) {
            listscan_item = ss_memhash[listscan_pos];
            while (listscan_item != NULL) {
                if (listscan_item->memlst_ptr != NULL) {
                    return(listscan_item);
                }
                listscan_item = listscan_item->memlst_next;
            }
            listscan_pos++;
        }
        return(NULL);
}

/*#***********************************************************************\
 *
 *		SsMemChkCheckListItem
 *
 * Checks if a memory pointer in 'item' is ok. The check detects
 * if someone has underwritten the beginning of the pointer or
 * overwritten the end of the pointer.
 *
 * Parameters :
 *
 *	item - in, use
 *		item which is checked
 *
 *      qmem_too - in
 *          TRUE when Qmem slot number should also be checked
 *          FALSE when not
 *
 * Return value - ref :
 *
 *      NULL if item is ok, or otherwise
 *      pointer to a string that contains the error message
 *
 * Limitations  :
 *
 * Globals used :
 */
char* SsMemChkCheckListItem(
        SsMemChkListT* item,
        bool qmem_too)
{
        ss_memchk_t chk1;
        ss_memchk_t chk2;
        char* ret = NULL;

        if (item->memlst_ptr == NULL) {
            return(NULL);
        }
        memcpy(
            &chk1,
            MEMCHK_GETBEGINMARKPTR(item->memlst_ptr),
            MEMCHK_CHKSIZE);
        memcpy(
            &chk2,
            MEMCHK_GETENDMARKPTR(item->memlst_ptr, item->memlst_size),
            MEMCHK_CHKSIZE);
        if (qmem_too && !ss_memdebug_segalloc) {
            size_t slot_no;

            slot_no = SsQmemGetSlotNo(item->memlst_ptr);
            if (slot_no != item->memlst_qmemslotno) {
                ret = SsQmemAlloc(128);
                SsSprintf(ret, "Qmem slot no of pointer 0x%08lX corrupted expected %ld, found %ld",
                    (ulong)item->memlst_ptr,
                    (long)item->memlst_qmemslotno,
                    (long)slot_no);
                return (ret);
            }
        }
        if ((chk1.mc_size != item->memlst_size && chk2.mc_size != item->memlst_size) || (chk1.mc_memobj != chk2.mc_memobj)) {
            ret = SsQmemAlloc(200);
            SsSprintf(ret, "Beginning and end of pointer 0x%08lX corrupted, size=%d, chk1.mc_size=%d, chk2.mc_size=%d",
                      (ulong)item->memlst_ptr, item->memlst_size, chk1.mc_size, chk2.mc_size);
        } else if (chk1.mc_size != item->memlst_size) {
            ret = SsQmemAlloc(200);
            SsSprintf(ret, "Beginning of pointer 0x%08lX corrupted, size=%d, chk1.mc_size=%d, chk2.mc_size=%d",
                      (ulong)item->memlst_ptr, item->memlst_size, chk1.mc_size, chk2.mc_size);
        } else if (chk2.mc_size != item->memlst_size) {
            ret = SsQmemAlloc(200);
            SsSprintf(ret, "End of pointer 0x%08lX corrupted, size=%d, chk1.mc_size=%d, chk2.mc_size=%d",
                      (ulong)item->memlst_ptr, item->memlst_size, chk1.mc_size, chk2.mc_size);
        }
        return(ret);
}

/*#**********************************************************************\
 *
 *		MemChkListCheck
 *
 * Checks all pointers in the list.
 *
 * Parameters :
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void MemChkListCheck(char* file, int line)
{
        int error = 0;
        char* msg;
        SsMemChkListT* list;

        list = MemChkGetListFirst();
        while (list != NULL) {
            msg = SsMemChkCheckListItem(list, TRUE);
            if (msg != NULL) {
                SsMemChkMessageEx(msg, list);
                SsQmemFree(msg);
                error = 1;
            }
            list = MemChkGetListNext();
        }

        if (error) {
            SsMemChkAbort((char *)"Aborting", file, line);
        }
}


/*##*********************************************************************\
 *
 *		SsMemChkListCheck
 *
 * Checks all pointers in the list.
 *
 * Parameters :
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkListCheck(
        char* file,
        int   line)
{
        SsSemEnter(ss_memchk_sem);

        MemChkListCheck(file, line);

        SsSemExit(ss_memchk_sem);
}

/*##**********************************************************************\
 *
 *		SsMemChkListCheckQmem
 *
 * Same as SsMemChkListCheck, but this one also checks that the
 * Qmem slot number has not been corrupted.
 *
 * Parameters :
 *
 *	file -
 *
 *
 *	line -
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
void SsMemChkListCheckQmem(
        char* file,
        int   line)
{
        int error = 0;
        char* msg;
        SsMemChkListT* list;

        SsSemEnter(ss_memchk_sem);

        list = MemChkGetListFirst();
        while (list != NULL) {
            msg = SsMemChkCheckListItem(list, TRUE);
            if (msg != NULL) {
                SsMemChkMessageEx(msg, list);
                SsQmemFree(msg);
                error = 1;
            }
            list = MemChkGetListNext();
        }

        SsSemExit(ss_memchk_sem);

        if (error) {
            SsMemChkAbort((char *)"Aborting", file, line);
        }
}

/*##**********************************************************************\
 *
 *		SsMemChkListCheckQmemNoMutex
 *
 * Same as SsMemChkListCheck, but this one also checks that the
 * Qmem slot number has not been corrupted. Does not acquire mutex.
 *
 * Parameters :
 *
 *	file -
 *
 *
 *	line -
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
void SsMemChkListCheckQmemNoMutex(
        char* file,
        int   line)
{
        int error = 0;
        char* msg;
        SsMemChkListT* list;

        list = MemChkGetListFirst();
        while (list != NULL) {
            msg = SsMemChkCheckListItem(list, TRUE);
            if (msg != NULL) {
                SsMemChkMessageEx(msg, list);
                SsQmemFree(msg);
                error = 1;
            }
            list = MemChkGetListNext();
        }

        if (error) {
            SsMemChkAbort((char *)"Aborting", file, line);
        }
}

SsMemChkListT* SsMemChkListFindTest(
        void* ptr,
        char* file,
        int   line)
{
        char* msg;
        SsMemChkListT* list;
        SsMemChkListT* list_head;

        if (ptr == NULL) {
            SsMemChkAbort((char *)"NULL pointer parameter", file, line);
        }
        list_head = ss_memhash[SS_MEMHASH_VAL(ptr)];
        for (list = list_head; list != NULL; list = list->memlst_next) {
            if (ss_memdebug) {
                msg = SsMemChkCheckListItem(list, TRUE);
                if (msg != NULL) {
                    SsMemChkAbortEx(msg, list);
                }
            }
            if (list->memlst_ptr == ptr) {
                SsMemChkListT* list2;

                for (list2 = list->memlst_next;
                     list2 != NULL;
                     list2 = list2->memlst_next)
                {
                    if (list2->memlst_ptr == list->memlst_ptr) {
                        msg = SsQmemAlloc(128);
                        SsSprintf(msg, "Pointer 0x%08lX is twice in list p1:%.50s %d p2:%.50s %d ",
                                  (ulong)list2->memlst_ptr,
                                  list->memlst_file,
                                  list->memlst_line,
                                  list2->memlst_file,
                                  list2->memlst_line);
                        SsMemChkAbort(msg, file, line);
                    }
                }
                return(list);
            }
        }
        return(NULL);
}

/*##*********************************************************************\
 *
 *		SsMemChkListFind
 *
 * Tries to find given pointer from the list of currently allocated
 * pointers. Aborts the program if pointer is not found.
 *
 * Parameters :
 *
 *	ptr - in, use
 *		pointer to be found
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value - ref :
 *
 *      Pointer to list entry with ptr.
 *
 * Limitations  :
 *
 * Globals used :
 */
SsMemChkListT* SsMemChkListFind(
        void* ptr,
        char* file,
        int   line)
{
        SsMemChkListT* list;

        list = SsMemChkListFindTest(ptr, file, line);
        if (list != NULL) {
            return(list);
        }
        SsMemChkAbort((char *)"Pointer not found", file, line);
        return(NULL);
}


/*##*********************************************************************\
 *
 *		SsMemChkListSet
 *
 * Sets check list node information.
 *
 * Parameters :
 *
 *	list - out, use
 *		list node
 *
 *	ptr - in, use
 *		new pointer value
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkListSet(
        SsMemChkListT* list,
        void*          ptr,
        size_t         size,
        char*          file,
        int            line)
{
        list->memlst_ptr = ptr;
        list->memlst_size = size;
        list->memlst_file = file;
        list->memlst_line = line;
        list->memlst_counter = ++alloc_timecounter;
        list->memlst_qmemslotno = SsQmemGetSlotNo(ptr);
#if defined(SSMEM_TRACE)
        list->memlst_callstk = SsMemTrcCopyCallStk();
        list->memlst_sqltrcinfo = SsSQLTrcInfoCopy();
#endif
        list->memlst_linkinfo = NULL;
}

/*##*********************************************************************\
 *
 *		SsMemChkListAdd
 *
 * Adds a pointer to the list of currently allocated pointers. If an error
 * occures, aborts the program.
 *
 * Parameters :
 *
 *	ptr - in, use
 *		pointer to be added
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkListAdd(
        void*    ptr,
        size_t   size,
        char*    file,
        int      line)
{
        char* msg;
        SsMemChkListT* list;
        SsMemChkListT* list_head;

        if (ptr == NULL) {
            SsMemChkAbort((char *)"Out of memory", file, line);
        }

        list_head = ss_memhash[SS_MEMHASH_VAL(ptr)];
        if (ss_memdebug) {
            SsMemChkListT* list2;

            for (list2 = list_head; list2 != NULL; list2 = list2->memlst_next) {
                msg = SsMemChkCheckListItem(list2, TRUE);
                if (msg != NULL) {
                    SsMemChkAbortEx(msg, list2);
                }
                if (list2->memlst_ptr == ptr) {
                    msg = SsQmemAlloc(128);
                    SsSprintf(msg, "Pointer 0x%08lX (new: %.50s, %d) already in list",
                        ptr, file, line);
                    SsMemChkAbortEx(msg, list2);
                }
            }
        }
        /* try to find unused list node */
        for (list = list_head; list != NULL; list = list->memlst_next) {
            if (ss_memdebug) {
                msg = SsMemChkCheckListItem(list, TRUE);
                if (msg != NULL) {
                    SsMemChkAbortEx(msg, list);
                }
            }
            if (list->memlst_ptr == NULL) {
                break;
            }
        }
        if (list == NULL) {
            /* empty list or no free nodes */
            list = SsQmemAlloc(sizeof(SsMemChkListT));
            if (list == NULL) {
                SsMemChkAbort((char *)"Out of memory", (char *)__FILE__, __LINE__);
            }
            list->memlst_next = list_head;
            ss_memhash[SS_MEMHASH_VAL(ptr)] = list;
        }
        SsMemChkListSet(list, ptr, size, file, line);
        if (ss_memdebug > 1) {
            list->memlst_allocinfo = MemChkAllocInfoInc(file, line, size);
        }
}

/*##**********************************************************************\
 *
 *		SsMemChkListRemove
 *
 * Removes pointer from the list.
 *
 * Parameters :
 *
 *		list -
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
void SsMemChkListRemove(SsMemChkListT* list)
{
        if (ss_memdebug > 1) {
            MemChkAllocInfoDec(list);
        }
        list->memlst_ptr = NULL;
}

/*#*********************************************************************\
 *
 *		MemChkSetCheck
 *
 * Adds under and overwrite check marks to a pointer.
 *
 * Parameters :
 *
 *	p - in out, use
 *		pointer to where the checks are added
 *
 *      size - in
 *		size of the allocated area in p, or MEMCHK_UNKNOWNSIZE
 *
 *      clear - in
 *		if TRUE, clears the memory area
 *
 *      filler - in
 *          set value for clearing (meaningful only when clear != FALSE)
 *
 *      p_oldsize - in
 *		if not NULL, old area size will be stored into *p_oldsize
 *
 * Return value - give :
 *
 *      pointer the user data area after the check mark at the beginning
 *
 * Limitations  :
 *
 * Globals used :
 */
static void* MemChkSetCheck(
        char* p,
        size_t size,
        bool clear,
        uint fill,
        size_t* p_oldsize,
        ss_memobjtype_t mo,
        bool freep)
{
        ushort filler = (ushort)fill;
        ss_memchk_t chk;
        size_t chk_size = size;
        ss_memchk_t oldchk;

        chk.mc_size = size;
        chk.mc_memobj = mo;

        if (p_oldsize != NULL && !ss_memdebug_segalloc) {
            ss_dassert(freep);
            memcpy(
                &oldchk,
                MEMCHK_GETBEGINMARKPTR(p),
                MEMCHK_CHKSIZE);
            *p_oldsize = oldchk.mc_size;
            SsMemObjDecNomutex(oldchk.mc_memobj, oldchk.mc_size);
        } else {
            ss_dassert(!freep);
            ss_dassert(size != MEMCHK_UNKNOWNSIZE);
            SsMemObjIncNomutex(mo, size);
        }

        if (size == MEMCHK_UNKNOWNSIZE && !ss_memdebug_segalloc) {
            memcpy(
                &chk,
                MEMCHK_GETBEGINMARKPTR(p),
                MEMCHK_CHKSIZE);
            chk_size = chk.mc_size;
            chk.mc_size = MEMCHK_ILLEGALCHK;
        }
        FAKE_CODE_BLOCK(FAKE_SS_DONTCLEARMEMORY,
            clear = FALSE;
        );
        if (clear && chk_size != MEMCHK_UNKNOWNSIZE) {
            char* p_end;
            ushort* ptr;
            ushort filler_size;

            /* calculate the filled area size and truncate it to
             * nearest two byte boundary
             */
            filler_size = (ushort)((chk_size / sizeof(ushort)) * sizeof(ushort));

            if (!ss_memdebug_segalloc) {
                p_end = (p + filler_size + MEMCHK_ALLOCSIZE);
            } else {
                p_end = (p + filler_size);
            }
            ptr = (ushort*)p;

            while ((char *)ptr < p_end) {
                *ptr++ = filler;
            }
            /* This may end a few bytes before the actual end of
               the allocated memory, but the end mark will overwrite
               it anyway. */
        }

        if (!ss_memdebug_segalloc) {

            memcpy(
                MEMCHK_GETBEGINMARKPTR(p),
                &chk,
                MEMCHK_CHKSIZE);
            memcpy(
                MEMCHK_GETENDMARKPTR(p, chk_size),
                &chk,
                MEMCHK_CHKSIZE);

            return(MEMCHK_GETUSERPTR(p));

        } else {

            return(p);
        }
}

/*##*********************************************************************\
 *
 *		SsMemChkSetCheck
 *
 * Adds under and overwrite check marks to a pointer.
 *
 * Parameters :
 *
 *	p - in out, use
 *		pointer to where the checks are added
 *
 *      size - in
 *		size of the allocated area in p, or MEMCHK_UNKNOWNSIZE
 *
 *      clear - in
 *		if TRUE, clears the memory area
 *
 *      filler - in
 *          set value for clearing (meaningful only when clear != FALSE)
 *
 *      p_oldsize - in
 *		if not NULL, old area size will be stored into *p_oldsize
 *
 * Return value - give :
 *
 *      pointer the user data area after the check mark at the beginning
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsMemChkSetCheck(
        char* p,
        size_t size,
        bool clear,
        uint fill,
        size_t* p_oldsize,
        ss_memobjtype_t mo)
{
        return(MemChkSetCheck(
                p,
                size,
                clear,
                fill,
                p_oldsize,
                mo,
                FALSE));
}

/*##*********************************************************************\
 *
 *		SsMemChkFreeCheck
 *
 * Adds under and overwrite check marks to a freed pointer.
 *
 * Parameters :
 *
 *	p - in out, use
 *		pointer to where the checks are added
 *
 *      size - in
 *		size of the allocated area in p, or MEMCHK_UNKNOWNSIZE
 *
 *      clear - in
 *		if TRUE, clears the memory area
 *
 *      filler - in
 *          set value for clearing (meaningful only when clear != FALSE)
 *
 *      p_oldsize - in
 *		if not NULL, old area size will be stored into *p_oldsize
 *
 * Return value - give :
 *
 *      pointer the user data area after the check mark at the beginning
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsMemChkFreeCheck(
        char* p,
        size_t size,
        bool clear,
        uint fill,
        size_t* p_oldsize)
{
        return(MemChkSetCheck(
                p,
                size,
                clear,
                fill,
                p_oldsize,
                0,
                TRUE));
}

/*##**********************************************************************\
 *
 *		SsMemChkGetSize
 *
 * Returns memory allocation size.
 *
 * Parameters :
 *
 *	p -
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
uint SsMemChkGetSize(char* p)
{
        ss_memchk_t oldchk;

        memcpy(
            &oldchk,
            MEMCHK_GETBEGINMARKPTR(p),
            MEMCHK_CHKSIZE);

        return(oldchk.mc_size);
}

/*##*********************************************************************\
 *
 *		SsMemChkCheckPtr
 *
 * Checks that check parks at the beginning and end of the pointer are
 * correct. If they are not, then the pointer is already deallocated,
 * the pointer is never allocated or someone has overwritten one or both
 * of the check marks. This function is intended to be used in the case
 * when the pointer size is not known, i.e. when the pointers are not
 * added to the list (ss_memdebug == 0). When the pointers are in the
 * list, use function SsMemChkCheckListItem. If the pointer check marks
 * are corrupted, aborts the program.
 *
 * Parameters :
 *
 *	p - in, use
 *		pointer to be checked
 *
 *	file - in, use
 *		file name of the call
 *
 *	line - in
 *		line number of the call
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkCheckPtr(p, file, line)
        char* p;
        char* file;
        int line;
{
        ss_memchk_t chk1;
        ss_memchk_t chk2;

        if (ss_memdebug_segalloc) {
            return;
        }
        memcpy(
            &chk1,
            MEMCHK_GETBEGINMARKPTR(p),
            MEMCHK_CHKSIZE);
        if ((int)chk1.mc_size == MEMCHK_ILLEGALCHK) {
            SsMemChkAbort((char *)"Already deallocated pointer passed to SsMemFree",
                          file, line);
        } else if ((int)chk1.mc_size <= 0 || chk1.mc_size > SS_MAXALLOCSIZE) {
            SsMemChkAbort("Invalid pointer passed to SsMemFree or memory underwrite",
                           file, line);
        } else {
            memcpy(
                &chk2,
                MEMCHK_GETENDMARKPTR(p, chk1.mc_size),
                MEMCHK_CHKSIZE);
        }
        if (chk1.mc_size != chk2.mc_size || chk1.mc_memobj != chk2.mc_memobj) {
            SsMemChkAbort((char *)"Invalid pointer passed to SsMemFree or memory overwrite",
                           file, line);
        }
}



/*##**********************************************************************\
 *
 *		SsMemChkGetListFirst
 *
 * Returns pointer to the descriptor of first allocated memory area.
 *
 * Parameters :
 *
 * Return value - ref :
 *
 *      pointer to the first memory area descriptor
 *
 * Limitations  :
 *
 *      Not reentrant.
 *
 * Globals used :
 */
SsMemChkListT* SsMemChkGetListFirst()
{
        SsMemChkListT* ml;

        SsSemEnter(ss_memchk_sem);

        ml = MemChkGetListFirst();

        SsSemExit(ss_memchk_sem);

        return(ml);
}

/*##**********************************************************************\
 *
 *		SsMemChkGetListNext
 *
 * Returns pointer to the descriptor of next allocated memory area.
 *
 * Parameters :
 *
 * Return value - ref :
 *
 *      pointer to the next memory area descriptor, or NULL at end
 *
 * Limitations  :
 *
 *      Not reentrant.
 *
 * Globals used :
 */
SsMemChkListT* SsMemChkGetListNext()
{
        SsMemChkListT* ml;

        SsSemEnter(ss_memchk_sem);

        ml = MemChkGetListNext();

        SsSemExit(ss_memchk_sem);

        return(ml);
}

/*##**********************************************************************\
 *
 *		SsMemChkGetCounter
 *
 * Returs the current memory allocation counter. Counter is incremented
 * each time a new allocation is done, and that allocation counter is
 * associated to current memory allocation in memory check list.
 *
 * The counter can be used as a time stamp, the greater the counter value,
 * the later the allocation has occured. This can be used for example to
 * detect which allocation are done inside some routine. To do that, save the
 * counter value before entering the routine. After the routine returns, all
 * pointer values in the list which counter value is greater than the
 * saved counter value and where the memory pointer is not NULL, are
 * allocated (and not released) in that routine.
 *
 * Parameters :
 *
 * Return value :
 *
 *      current memory check counter value
 *
 * Limitations  :
 *
 *      counter is long, is that large enough (maybe unsigned long?)
 *
 * Globals used :
 */
long SsMemChkGetCounter()
{
        return(alloc_timecounter);
}

/*##**********************************************************************\
 *
 *		SsMemChkFPrintInfo
 *
 * Prints memory allocation information to a file pointer.
 *
 * Parameters :
 *
 *      fp - use
 *          File pointer, or NULL.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkFPrintInfo(void* fp)
{
        SsFprintf(fp, "Memory allocation information:\n");
        SsFprintf(fp, "  allocated bytes                          : %lu\n", memchk_bytes);
        SsFprintf(fp, "  max allocated bytes                      : %lu\n", memchk_maxbytes);
        SsFprintf(fp, "  allocated qmem slot bytes                : %lu\n", ss_qmem_stat.qms_slotbytecount);
        SsFprintf(fp, "  allocated qmem system bytes              : %lu\n", ss_qmem_stat.qms_sysbytecount);
        SsFprintf(fp, "  number of allocated pointers             : %lu\n", memchk_nptr);
        SsFprintf(fp, "  number of qmem allocated pointers        : %lu\n", ss_qmem_nptr);
        SsFprintf(fp, "  number of qmem allocated system pointers : %lu\n", ss_qmem_nsysptr);
        SsFprintf(fp, "  number of calls to SsMemAlloc            : %lu\n", memchk_alloc);
        SsFprintf(fp, "  number of calls to SsMemRealloc          : %lu\n", memchk_realloc);
        SsFprintf(fp, "  number of calls to SsMemCalloc           : %lu\n", memchk_calloc);
        SsFprintf(fp, "  number of calls to SsMemFree             : %lu\n", memchk_free);
        SsFprintf(fp, "  number of calls to SsMemStrdup           : %lu\n", memchk_strdup);
        SsQmemFPrintInfo(fp);
        SsMemChkAllocInfoPrint(fp, 50, (char *)"");
}

/*##**********************************************************************\
 *
 *		SsMemChkPrintInfo
 *
 * Prints memory allocation information.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkPrintInfo(void)
{
        SsMemChkFPrintInfo(NULL);
}

/*##**********************************************************************\
 *
 *		SsMemChkPrintToFile
 *
 *
 *
 * Parameters :
 *
 *	filename -
 *
 *
 *	message -
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
void SsMemChkPrintToFile(char* filename, char* message)
{
        ulong countall = 0;
        ulong memall = 0;
        SsMemChkListT* list;
        SS_FILE* fp;

        if (filename == NULL) {
            filename = (char *)"ssmemchk.out";
        }
        fp = SsFOpenT(filename, (char *)"a+");
        if (fp == NULL) {
            return;
        }

        if (message != NULL) {
            SsFPrintf(fp, "%s\n", message);
        }

        SsSemEnter(ss_memchk_sem);

        list = MemChkGetListFirst();

        if (list == NULL) {

            SsFPrintf(fp, "Total of %ld allocated pointers.\n", memchk_nptr);

        } else {

            SsFPrintf(fp, "File:        Line: Size: Counter: Slot: Ptr:\n");
            while (list != NULL) {
                if (list->memlst_ptr != NULL) {
                    memall += list->memlst_size;
                    countall++;
                    SsFPrintf(fp, "%-12s %4d  %5u %6ld  %5d %08lx\n",
                        list->memlst_file,
                        list->memlst_line,
                        list->memlst_size,
                        list->memlst_counter,
                        listscan_pos,
                        (long)list->memlst_ptr);
                }
                list = MemChkGetListNext();
            }
            SsFPrintf(fp, "Total: %5lu pointers, %5lu bytes\n", countall, memall);
        }

        SsSemExit(ss_memchk_sem);

        SsFFlush(fp);
        SsFClose(fp);
}

/*#***********************************************************************\
 *
 *		MemChkPrintListByCounter
 *
 *
 *
 * Parameters :
 *
 *	mincounter -
 *
 *
 *	minsize -
 *
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
static void MemChkPrintListByCounter(
        long mincounter,
        long minsize)
{
        ulong countall = 0;
        ulong countlist = 0;
        ulong memall = 0;
        ulong memlist = 0;
        SsMemChkListT* list;
        char* sqlstr;
        char* appinfo;

        if (memchk_disableprintlist) {
            return;
        }

        SsSemEnter(ss_memchk_sem);

        list = MemChkGetListFirst();

        if (list == NULL) {

            /* 
             * Note: Test logging leaves one pointer  
             */
            if (0 && memchk_nptr > 1 && strncmp(ss_testlog_gettestname(), "solid", 5) == 0) {
                ss_testlog_print((char *)"LEAK: %s\n", ss_testlog_gettestname());
                ss_testlog_print((char *)"LEAK: Total of %ld allocated pointers.\n", memchk_nptr - 1);
            }
            SsDbgPrintf("Total of %ld allocated pointers.\n", memchk_nptr);

        } else {

            if (0 && memchk_nptr > 1 && strncmp(ss_testlog_gettestname(), "solid", 5) == 0) {
                ss_testlog_print((char *)"==================================================\n");
            }
            SsDbgPrintf("File:        Line: Size: Counter: Ptr:\n");
            while (list != NULL) {
                if (list->memlst_ptr != NULL) {
                    memall += list->memlst_size;
                    countall++;
                    if (list->memlst_counter >= mincounter &&
                        list->memlst_size > (ulong)minsize) {
                        memlist += list->memlst_size;
                        countlist++;
                        SsDbgPrintf("%-12s %4d  %5u %6ld   %p\n",
                            list->memlst_file,
                            list->memlst_line,
                            list->memlst_size,
                            list->memlst_counter,
                            list->memlst_ptr);

                        /* 
                         * Note: Test logging leaves one pointer  
                         */
                        if (0 && memchk_nptr > 1 && strncmp(ss_testlog_gettestname(), "solid", 5) == 0) {
                            sqlstr = SsSQLTrcInfoGetStr(list->memlst_sqltrcinfo);
                            appinfo = list->memlst_callstk[0];
                            if (sqlstr != NULL) {
                                ss_testlog_print((char *)"LEAK: %-12s %4d %.128s %.128s\n",
                                    list->memlst_file,
                                    list->memlst_line,
                                    appinfo == NULL ? "" : appinfo,
                                    sqlstr == NULL ? "" : sqlstr);
                            } else {
                                ss_testlog_print((char *)"LEAK: %-12s %4d %.128s\n",
                                    list->memlst_file,
                                    list->memlst_line,
                                    appinfo == NULL ? "" : appinfo);
                            }
                        }

#if defined(SSMEM_TRACE)
                        SsMemTrcPrintCallStk(list->memlst_callstk);
                        SsSQLTrcInfoPrint(list->memlst_sqltrcinfo);
#endif
                        if (list->memlst_linkinfo != NULL) {
                            int i;
                            SsDbgPrintf("    Link info\n");
                            SsDbgPrintf("    File:        Line: Type:  Count:\n");
                            for (i = 0; list->memlst_linkinfo[i].ml_file != NULL; i++) {
                                SsDbgPrintf("    %-12s %4d  %s     %d\n",
                                    list->memlst_linkinfo[i].ml_file,
                                    list->memlst_linkinfo[i].ml_line,
                                    list->memlst_linkinfo[i].ml_linkp
                                        ? "link  "
                                        : "unlink",
                                    list->memlst_linkinfo[i].ml_count);
                            }
                        }
                    }
                }
                list = MemChkGetListNext();
            }
            SsDbgPrintf("List:  %5lu pointers, %5lu bytes\n", countlist, memlist);
            SsDbgPrintf("Total: %5lu pointers, %5lu bytes\n", countall, memall);
            if (0 && memchk_nptr > 1 && strncmp(ss_testlog_gettestname(), "solid", 5) == 0) {
                ss_testlog_print((char *)"LEAK: %s\n", ss_testlog_gettestname());
                ss_testlog_print((char *)"LEAK: Total: %5lu pointers, %5lu bytes\n", countall, memall);
                ss_testlog_print((char *)"==================================================\n");
            }
        }

        SsSemExit(ss_memchk_sem);
}

/*##*********************************************************************\
 *
 *		SsMemChkPrintListByCounter
 *
 * Prints a list of allocated memory pointers where counter value is
 * greater than or equal to mincounter
 *
 * Parameters :
 *
 *	mincounter - in
 *		smallest counter value that is displayed
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkPrintListByCounter(long mincounter)
{
        MemChkPrintListByCounter(mincounter, 0L);
}

/*##**********************************************************************\
 *
 *		SsMemChkPrintListByMinSize
 *
 * Prints pointers with greater size than minsize.
 *
 * Parameters :
 *
 *	minsize -
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
void SsMemChkPrintListByMinSize(long minsize)
{
        MemChkPrintListByCounter(0L, minsize);
}

/*##**********************************************************************\
 *
 *		SsMemChkPrintList
 *
 * Prints a list of allocated memory pointers.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkPrintList()
{
        SsMemChkPrintListByCounter(0L);
}

/*##**********************************************************************\
 *
 *      SsMemChkPrintListByAllocatedPtrs
 *
 * Prints a list of allocated memory pointer if N or more pointers are still
 * allocated.
 *
 * Parameters:
 *      thisormore - in
 *          boundary; if this many or more are allocced, print list.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void SsMemChkPrintListByAllocatedPtrs(long thisormore)
{
        if (memchk_nptr >= (ulong)thisormore) {
            SsMemChkPrintListByCounter(0L);
        }
}

/*##**********************************************************************\
 *
 *		SsMemChkPrintNewList
 *
 * Prints a list of allocated memory pointers since the last call to
 * this function.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkPrintNewList()
{
        static long last_counter = 0;

        SsMemChkPrintListByCounter(last_counter);
        last_counter = alloc_timecounter;
}

#endif /* SS_DEBUG */
