/*************************************************************************\
**  source       * uti0vtpl.h
**  directory    * uti
**  description  * V-tuple external interface
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

#ifndef UTI0VTPL_H
#define UTI0VTPL_H      

#include <ssc.h>
#include <ssstddef.h>
#include "uti0va.h"


/* constants ***********************************************/

/* empty v-tuple */
#define VTPL_EMPTY (&vtpl_null)

/* maximum length of data in a v-tuple */
#define VTPL_MAXINDEX     ((vtpl_index_t)-5)


/* types ***************************************************/

typedef va_index_t vtpl_index_t;

typedef struct {
        ss_byte_t c[6]; /* actually variable-sized */
} vtpl_t;

typedef vtpl_t* dynvtpl_t;

/* structure for fast mapping of attributes inside vtuple */
typedef struct vtpl_vamap_st {
        vtpl_index_t vamap_maxva;
        vtpl_index_t vamap_nva;
        vtpl_t*      vamap_vtpl;
        va_t*        vamap_arr[1]; /* actually variable-sized */
} vtpl_vamap_t;

/* global variables ****************************************/

extern vtpl_t vtpl_null;


/* global functions ****************************************/

vtpl_index_t    vtpl_grosslen(vtpl_t* va);
FOUR_BYTE_T     vtpl_grosslen_long(vtpl_t* va);
vtpl_index_t    vtpl_netlen(vtpl_t* va);

SS_INLINE void dynvtpl_free(dynvtpl_t* p_dynvtpl);

vtpl_t* vtpl_setva(vtpl_t* target_vtpl, va_t* source_va);
vtpl_t* vtpl_setvtpl(vtpl_t* target_vtpl, vtpl_t* source_va);
vtpl_t* dynvtpl_setva(dynvtpl_t* p_target_vtpl, va_t* source_va);
vtpl_t* dynvtpl_setvtpl(dynvtpl_t* p_target_vtpl, vtpl_t* source_va);
vtpl_t* dynvtpl_setvtplwithincrement(
            dynvtpl_t* p_target_vtpl,
            vtpl_t *source_vtpl);
vtpl_t* vtpl_setvtplwithincrement(
        vtpl_t *target_vtpl,
        vtpl_t *source_vtpl);
vtpl_t* dynvtpl_setvtplwithincrement_lastvano(
        dynvtpl_t* p_target_vtpl,
        vtpl_t *source_vtpl,
        int lastvano);

SS_INLINE vtpl_t* vtpl_appva(vtpl_t* target_vtpl, va_t* source_va);
vtpl_t* vtpl_appvtpl(vtpl_t* target_vtpl, vtpl_t* source_va);
vtpl_t* dynvtpl_appva(dynvtpl_t* p_target_vtpl, va_t* source_va);
vtpl_t* vtpl_appvawithincrement(vtpl_t* target_vtpl, va_t* source_va);
vtpl_t* dynvtpl_appvawithincrement(dynvtpl_t* p_target_vtpl, va_t* source_va);
vtpl_t* dynvtpl_appvtpl(dynvtpl_t* p_target_vtpl, vtpl_t* source_va);

va_t*           vtpl_getva_at(vtpl_t* vtpl, vtpl_index_t fieldno);
va_t*           vtpl_skipva(va_t* curr_va);
vtpl_index_t    vtpl_vacount(vtpl_t* vtpl);
bool            vtpl_consistent(vtpl_t* vtpl);

vtpl_vamap_t*   vtpl_vamap_alloc(int max_vacount);
vtpl_vamap_t*   vtpl_vamap_init(vtpl_t* vtpl);
vtpl_vamap_t*   vtpl_vamap_refill(vtpl_vamap_t* vamap, vtpl_t* vtpl);
void            vtpl_vamap_done(vtpl_vamap_t* vamap);
void            vtpl_vamap_removelast(vtpl_vamap_t* vamap);
va_t*           vtpl_vamap_getva_at(vtpl_vamap_t* vamap, vtpl_index_t fieldno);
vtpl_index_t    vtpl_vamap_vacount(vtpl_vamap_t* vamap);
va_t* vtpl_vamap_getvafromcopy_at(vtpl_t* copied_vtpl, vtpl_vamap_t* vamap,
                                  vtpl_index_t fieldno);

vtpl_t* vtpl_normalize(vtpl_t* vtpl);
vtpl_t* dynvtpl_normalize(dynvtpl_t* p_vtpl);
vtpl_t* vtpl_truncate(vtpl_t* vtpl, vtpl_index_t max_len);

bool vtpl_buildvtpltext(char* buf, int buflen, vtpl_t* vtpl);
bool vtpl_printvtpl(void* fp, vtpl_t* vtpl);
bool vtpl_dprintvtpl(int level, vtpl_t* vtpl);

/*#**********************************************************************\
 * 
 *		VTPL_GROSSLEN
 * 
 * Returns the gross length of a v-tuple. The gross length includes
 * the data length and the length of the length bytes.
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *          the v-tuple
 * 
 * Return value : 
 *                
 *      gross length of vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define VTPL_GROSSLEN(vtpl) \
        VA_GROSSLEN((va_t*)(vtpl))

/*#**********************************************************************\
 * 
 *		VTPL_NETLEN
 * 
 * Returns the net length of a v-tuple. 
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *          the v-tuple
 * 
 * Return value : 
 *                
 *      net length of vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define VTPL_NETLEN(vtpl) \
        VA_NETLEN((va_t*)(vtpl))

/*#**********************************************************************\
 * 
 *		VTPL_LENLEN
 * 
 * Returns the length field length of a v-tuple. 
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *          the v-tuple
 * 
 * Return value : 
 *                
 *      length field length of vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define VTPL_LENLEN(vtpl) \
        VA_LENLEN((va_t*)(vtpl))

/*#**********************************************************************\
 * 
 *		VTPL_GETVA_AT0
 *
 * Returns pointer to first va of a vtuple
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *          the v-tuple
 * 
 * Return value : 
 *                
 *      first va (at index 0) of a vtuple
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define VTPL_GETVA_AT0(vtpl) \
        ((va_t*)VA_GETASCIIZ((va_t*)(vtpl)))

#define VTPL_GETDATA(p_data, p_vtpl, len) \
        VA_GETDATA(p_data, p_vtpl, len)

/*##**********************************************************************\
 * 
 *		VTPL_SKIPVA
 * 
 * Skip to next v-attribute within v-tuple.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to a v-attribute within a v-tuple
 *
 * Return value - ref : 
 * 
 *      pointer to the next v-attribute
 * 
 * Limitations  : 
 * 
 *      Doesn't detect the end of the v-tuple.
 * 
 * Globals used : 
 */
#define VTPL_SKIPVA(va) \
        ((va_t*)((char*)(va) + VA_GROSSLEN(va)))

#ifndef SS_DEBUG
#  ifndef vtpl_grosslen
#    define vtpl_grosslen(vtpl) VTPL_GROSSLEN(vtpl)
#  endif /* !vtpl_grosslen */ 
#  ifndef vtpl_netlen
#    define vtpl_netlen(vtpl) VTPL_NETLEN(vtpl)
#  endif /* !vtpl_netlen */ 
#endif /* !SS_DEBUG */

#define _VTPL_VAMAP_GETVA_AT_(vamap, fno) (((fno) >= (vamap)->vamap_nva) \
                                                ? VA_DEFAULT /* For Alter table */ \
                                                : (vamap)->vamap_arr[fno])
#define _VTPL_SETVTPL_(target_vtpl, source_vtpl) \
         ((vtpl_t*)memcpy(target_vtpl, source_vtpl, VTPL_GROSSLEN(source_vtpl)))


#ifndef SS_DEBUG
#define vtpl_vamap_getva_at(vamap, fno)         _VTPL_VAMAP_GETVA_AT_(vamap, fno)
#define vtpl_setvtpl(target_vtpl, source_vtpl)  _VTPL_SETVTPL_(target_vtpl, source_vtpl)
#endif

#if defined(UTI0VTPL_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		vtpl_appva
 * 
 * Appends a v-attribute to a v-tuple.
 * 
 * Parameters : 
 * 
 *	target_vtpl - in out, use
 *		pointer to the v-tuple
 *
 *	source_va - in, use
 *		pointer to the v-attribute
 *
 * Return value - ref : 
 * 
 *      target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE vtpl_t* vtpl_appva(
	vtpl_t* target_vtpl,
	va_t* source_va)
{
        return((vtpl_t*)va_appdata((va_t*)target_vtpl, source_va,
                                    VA_GROSS_LEN(source_va)));
}

/*##**********************************************************************\
 * 
 *		dynvtpl_free
 * 
 * Releases the memory allocated for a dynamic v-tuple.
 * 
 * Parameters : 
 * 
 *	p_dynvtpl - in out, use
 *		pointer to a dynvtpl variable, *p_dynvtpl set to NULL
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
SS_INLINE void dynvtpl_free(
	dynvtpl_t* p_dynvtpl)
{
        ss_dassert(p_dynvtpl != NULL);

        if (*p_dynvtpl != NULL) {
            SsMemFree(*p_dynvtpl);
            *p_dynvtpl = NULL;
        }
}

#endif /* defined(UTI0VTPL_C) || defined(SS_USE_INLINE) */

#endif /* UTI0VTPL_H */
