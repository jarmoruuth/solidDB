/*************************************************************************\
**  source       * su0parr.h
**  directory    * su
**  description  * Pointer array data type.
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


#ifndef SU0PARR_H
#define SU0PARR_H

#include <ssc.h>
#include <ssdebug.h>

#include "su0rbtr.h"
#include "su1check.h"

typedef void* su_pa_elem_t;

/* Structure for a pointer array. This structure is defined here only for
   efficient implementation of su_pa_do macro. The fields should not be
   accessed directly.
*/
struct su_pa_st {
        ss_autotest(long pa_maxlen;)
        uint          pa_nelems;    /* number of used elements */
        uint          pa_size;      /* size of pa_elems */
        su_pa_elem_t* pa_elems;     /* array of user data pointers */
        int           pa_chk;       /* check field */
        void*         pa_freerbt;
        uint*         pa_freearray;
        uint          pa_maxfreearray;
        uint          pa_curfreearray;
        ss_debug(int  pa_semnum;)
};

typedef struct su_pa_st su_pa_t;

su_pa_t* su_pa_init(void);

void su_pa_done(
        su_pa_t* pa);

void su_pa_setrecyclecount(
        su_pa_t* pa,
        uint maxsize);

uint su_pa_insert(
        su_pa_t* pa,
        void* data);

void su_pa_insertat(
        su_pa_t* pa,
        uint index,
        void* data);

void* su_pa_remove(
        su_pa_t* pa,
        uint index);

SS_INLINE void su_pa_removeall(
        su_pa_t* pa);

void* su_pa_getdata(
        su_pa_t* pa,
        uint index);

bool su_pa_indexinuse(
        su_pa_t* pa,
        uint index);

void* su_pa_getnext(
        su_pa_t* pa,
        uint* p_index);

uint su_pa_nelems(
        su_pa_t* pa);

uint su_pa_realsize(
        su_pa_t* pa);

bool su_pa_compress(
        su_pa_t* pa);

#ifdef SS_DEBUG
void su_pa_setsemnum(
        su_pa_t* pa,
        int semnum);
#endif

#ifdef AUTOTEST_RUN
void su_pa_setmaxlen(su_pa_t* pa, long maxlen);
#endif


/*##**********************************************************************\
 * 
 *		su_pa_do
 * 
 * This macro can be used to loop over elements in the pointer array.
 * 
 * Example:
 *
 *      su_pa_t* pa;
 *      int      i;
 *
 *      pa = su_pa_init();
 *      su_pa_do(pa, i) {
 *          ... process element at index i ...
 *      }
 *      su_pa_done(pa);
 * 
 * Parameters : 
 * 
 *	pa - in, use
 *		Pointer array pointer.
 *
 *      i - in out
 *          Index variable. For each loop iteration this variable
 *          contains the pa index. The index can be used to get
 *          the data pointer at the index position.
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define su_pa_do(pa, i) \
            for ((i) = 0; (uint)(i) < (pa)->pa_size; (i)++) \
                if ((pa)->pa_elems[i] != NULL)

/*##**********************************************************************\
 * 
 *		su_pa_do_get
 * 
 * This macro can be used to loop over elements in the pointer array.
 * The data element for each loop iteration is stored into parameter
 * data.
 * 
 * Example:
 *
 *      su_pa_t* pa;
 *      int      i;
 *      void*    data;
 *
 *      pa = su_pa_init();
 *      su_pa_do_get(pa, i, data) {
 *          ... process data ...
 *      }
 *      su_pa_done(pa);
 * 
 * Parameters : 
 * 
 *	pa - in, use
 *		Pointer array pointer.
 *
 *      i - in out
 *          Index variable. For each loop iteration this variable
 *          contains the pa index. The index can be used to get
 *          the data pointer at the index position.
 *
 *      data - in out
 *          Data variable. In each loop iteration the data element is
 *          stored here.
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define su_pa_do_get(pa, i, data) \
            for ((i) = 0; (uint)(i) < (pa)->pa_size; (i)++) \
                if (((data) = (pa)->pa_elems[i]) != NULL)


#define su_pa_datastart(pa) ((void*)(pa)->pa_elems)

#define _SU_PA_GETDATA_(p, i)    ((p)->pa_elems[i])
#define _SU_PA_INDEXINUSE_(p, i) ((uint)(i) < (p)->pa_size && (p)->pa_elems[i] != NULL)
#define _SU_PA_NELEMS_(p)        ((p)->pa_nelems)
#define _SU_PA_REALSIZE_(p)        ((p)->pa_size)

#ifndef SS_DEBUG

#define su_pa_getdata       _SU_PA_GETDATA_
#define su_pa_indexinuse    _SU_PA_INDEXINUSE_
#define su_pa_nelems        _SU_PA_NELEMS_
#define su_pa_realsize      _SU_PA_REALSIZE_

#endif /* not SS_DEBUG */

#ifdef AUTOTEST_RUN
# define CHK_PA(pa)  ss_assert((pa) != NULL && (pa)->pa_chk == SUCHK_PA); ss_info_assert((pa)->pa_size < (pa)->pa_maxlen, ("Maximum list length (%ld) ecxeeded", (pa)->pa_maxlen))
#else
# define CHK_PA(pa) ss_bassert((pa) != NULL && (pa)->pa_chk == SUCHK_PA && (pa)->pa_size < 1000000)
#endif

#if defined(SU0PARR_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		su_pa_removeall
 * 
 * Removes all indexes in the pointer array. After this call the pointer
 * array is empty.
 * 
 * Parameters : 
 * 
 *	pa - in out, use
 *		Pointer array pointer.
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void su_pa_removeall(su_pa_t* pa)
{
        CHK_PA(pa);
        ss_dassert(pa->pa_semnum == 0 || SsSemStkFind(pa->pa_semnum));

        if (pa->pa_size > 0) {
            SsMemFree(pa->pa_elems);
            pa->pa_nelems = 0;
            pa->pa_size = 0;
            pa->pa_elems = NULL;
        }
        if (pa->pa_maxfreearray > 0) {
            int maxarray;

            ss_dassert(pa->pa_freerbt != NULL);
            ss_dassert(pa->pa_freearray != NULL);
            su_rbt_done((su_rbt_t*)pa->pa_freerbt);
            SsMemFree(pa->pa_freearray);
            
            maxarray = pa->pa_maxfreearray;
            pa->pa_maxfreearray = 0;

            su_pa_setrecyclecount(pa, maxarray);
        }
}

#endif /* defined(SU0PARR_C) || defined(SS_USE_INLINE) */

#endif /* SU0PARR_H */
