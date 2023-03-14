/*************************************************************************\
**  source       * uti0vtpb.c
**  directory    * uti
**  description  * Basic v-tuple operations
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

See nstdyn.doc.

Limitations:
-----------

None.

Error handling:
--------------

None.

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

#define UTI0VTPL_C

#include <ssstring.h>
#include <sssprint.h>
#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssmsglog.h>
#include "uti0vtpl.h"
#include "uti0va.h"
#include "uti1val.h"

/* Redefine VTPL_GROSSLEN for local use */
#undef VTPL_GROSSLEN
#define VTPL_GROSSLEN(vtpl) VA_GROSS_LEN((va_t*)(vtpl))

/* static functions ****************************************/

#ifndef NO_ANSI
static vtpl_index_t uti_normalize(vtpl_t* vtpl);
#else
static vtpl_index_t uti_normalize();
#endif


/* global variables ****************************************/

vtpl_t vtpl_null = {{0, 0, 0, 0, 0, 0}};


/* functions ***********************************************/


/*##**********************************************************************\
 * 
 *		vtpl_grosslen
 * 
 * Returns the length of the whole v-tuple, truncated to a vtpl_index_t.
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *		pointer to the v-tuple
 *
 * Return value : 
 * 
 *      the length of the v-tuple truncated to a vtpl_index_t
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef vtpl_grosslen
vtpl_index_t vtpl_grosslen(vtpl)
	vtpl_t* vtpl;
{
        register ss_byte_t first_byte;

        if (BYTE_LEN_TEST(first_byte = vtpl->c[0])) {
            return(first_byte + 1);
        } else {
            return(FOUR_BYTE_LEN(vtpl) + 5);
        }
}
#endif /* !vtpl_grosslen */

/*##**********************************************************************\
 * 
 *		vtpl_grosslen_long
 * 
 * Returns the length of the whole v-tuple.
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *		pointer to the v-tuple
 *
 * Return value : 
 * 
 *      the length of the v-tuple
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
FOUR_BYTE_T vtpl_grosslen_long(vtpl)
	vtpl_t* vtpl;
{
        register ss_byte_t first_byte;

        if (BYTE_LEN_TEST(first_byte = vtpl->c[0])) {
            return(first_byte + 1);
        } else {
            return(FOUR_BYTE_LEN_LONG(vtpl) + 5);
        }
}


/*##**********************************************************************\
 * 
 *		vtpl_netlen
 * 
 * Returns the net length of the v-tuple, truncated to a vtpl_index_t.
 * Net length is the data area length.
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *		pointer to the v-tuple
 *
 * Return value : 
 * 
 *      the net length of the v-tuple truncated to a vtpl_index_t
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef vtpl_netlen
vtpl_index_t vtpl_netlen(vtpl)
	vtpl_t* vtpl;
{
        return(va_netlen((va_t*)vtpl));
}
#endif /* !vtpl_netlen */

/*##**********************************************************************\
 * 
 *		vtpl_setva
 * 
 * Set an v-attribute to a v-tuple.
 * 
 * Parameters : 
 * 
 *	target_vtpl - out, use
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
vtpl_t* vtpl_setva(target_vtpl, source_va)
	vtpl_t* target_vtpl;
	va_t* source_va;
{
        return((vtpl_t*)va_setdata((va_t*)target_vtpl, source_va,
                                    VA_GROSS_LEN(source_va)));
}


/*##**********************************************************************\
 * 
 *		dynvtpl_setva
 * 
 * Set an v-attribute to a dynamic v-tuple.
 * 
 * Parameters : 
 * 
 *	p_target_vtpl - in out, give
 *		pointer to the v-tuple
 *
 *	source_va - in, use
 *		pointer to the v-attribute
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
vtpl_t* dynvtpl_setva(p_target_vtpl, source_va)
	dynvtpl_t* p_target_vtpl;
	va_t* source_va;
{
        return((vtpl_t*)dynva_setdata((dynva_t*)p_target_vtpl, source_va,
                                       VA_GROSS_LEN(source_va)));
}


/*##**********************************************************************\
 * 
 *		dynvtpl_setvtplwithincrement
 * 
 * Sets value from source_vtpl but increments the last attribute
 * in p_target_vtpl by appending one null-byte. This is, in fact the
 * smallest possible increment in key comparison.
 * 
 * Parameters : 
 * 
 *	p_target_vtpl - in out, give
 *		target
 *
 *	source_vtpl - in, use
 *		source
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vtpl_t *dynvtpl_setvtplwithincrement(p_target_vtpl, source_vtpl)
	dynvtpl_t* p_target_vtpl;
        vtpl_t *source_vtpl;
{
        ss_byte_t* vtpl_endmark;
        va_t *p_va;
        va_t *p_va2;

        ss_dassert(p_target_vtpl != NULL);
        ss_dassert(source_vtpl != NULL);

        vtpl_endmark = (ss_byte_t*)source_vtpl + VTPL_GROSSLEN((va_t*)source_vtpl);

        dynvtpl_setvtpl(p_target_vtpl, VTPL_EMPTY);
        /* copy all but one attributes */
        p_va2 = p_va = VTPL_GETVA_AT0(source_vtpl);
        for (;;) {
            p_va = VTPL_SKIPVA(p_va);
            if ((ss_byte_t*)p_va < vtpl_endmark) {
                /* p_va2 is not last */
                dynvtpl_appva(p_target_vtpl, p_va2);
            } else {
                /* p_va2 is last attribute */
                break;
            }
            p_va2 = p_va;
        }
        dynvtpl_appvawithincrement(p_target_vtpl, p_va2);  /* append to tuple */
        return *p_target_vtpl;
}

/*##**********************************************************************\
 * 
 *		vtpl_setvtplwithincrement
 * 
 * Sets value from source_vtpl but increments the last attribute
 * in p_target_vtpl by appending one null-byte. This is, in fact the
 * smallest possible increment in key comparison.
 * 
 * Parameters : 
 * 
 *	target_vtpl - in out, use
 *		target
 *
 *	source_vtpl - in, use
 *		source
 *
 * Return value - ref : 
 * 
 *      the new value of target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vtpl_t *vtpl_setvtplwithincrement(target_vtpl, source_vtpl)
        vtpl_t *target_vtpl;
        vtpl_t *source_vtpl;
{
        ss_byte_t* vtpl_endmark;
        va_t *p_va;
        va_t *p_va2;

        ss_dassert(target_vtpl != NULL);
        ss_dassert(source_vtpl != NULL);

        vtpl_endmark = (ss_byte_t*)source_vtpl + VTPL_GROSSLEN((va_t*)source_vtpl);

        vtpl_setvtpl(target_vtpl, VTPL_EMPTY);
        /* copy all but one attributes */
        p_va2 = p_va = VTPL_GETVA_AT0(source_vtpl);
        for (;;) {
            p_va = VTPL_SKIPVA(p_va);
            if ((ss_byte_t*)p_va < vtpl_endmark) {
                /* p_va2 is not last */
                vtpl_appva(target_vtpl, p_va2);
            } else {
                /* p_va2 is last attribute */
                break;
            }
            p_va2 = p_va;
        }
        vtpl_appvawithincrement(target_vtpl, p_va2);  /* append to tuple */
        return target_vtpl;
}

/*##**********************************************************************\
 * 
 *		dynvtpl_setvtplwithincrement_lastvano
 * 
 * Sets value from source_vtpl up to last va number (lastvano) and
 * increments the last attribute by appending one null-byte. This is, 
 * in fact the smallest possible increment in key comparison for given
 * v-attribyte set.
 * 
 * Parameters : 
 * 
 *	p_target_vtpl - in out, give
 *		target
 *
 *	source_vtpl - in, use
 *		source
 *
 *	lastvano - in, use
 *		last attribute number included
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vtpl_t *dynvtpl_setvtplwithincrement_lastvano(p_target_vtpl, source_vtpl, lastvano)
	    dynvtpl_t* p_target_vtpl;
        vtpl_t *source_vtpl;
        int lastvano;
{
        ss_byte_t* vtpl_endmark;
        va_t *p_va;
        va_t *p_va2;
        int vano;

        ss_dassert(p_target_vtpl != NULL);
        ss_dassert(source_vtpl != NULL);

        vtpl_endmark = (ss_byte_t*)source_vtpl + VTPL_GROSSLEN((va_t*)source_vtpl);

        dynvtpl_setvtpl(p_target_vtpl, VTPL_EMPTY);
        /* copy all up to lastvano attributes */
        p_va2 = p_va = VTPL_GETVA_AT0(source_vtpl);
        for (vano = 0; vano < lastvano; vano++) {
            p_va = VTPL_SKIPVA(p_va);
            if ((ss_byte_t*)p_va < vtpl_endmark) {
                /* p_va2 is not last */
                dynvtpl_appva(p_target_vtpl, p_va2);
            } else {
                /* p_va2 is last attribute */
                ss_derror;
                break;
            }
            p_va2 = p_va;
        }
        ss_dassert(vano == lastvano);
        dynvtpl_appvawithincrement(p_target_vtpl, p_va2);  /* append to tuple */
        return *p_target_vtpl;
}

/*##**********************************************************************\
 * 
 *		dynvtpl_appva
 * 
 * Appends an v-attribute to a dynamic v-tuple.
 * 
 * Parameters : 
 * 
 *	p_target_vtpl - in out, give
 *		pointer to the v-tuple
 *
 *	source_va - in, use
 *		pointer to the v-attribute
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
vtpl_t* dynvtpl_appva(p_target_vtpl, source_va)
	dynvtpl_t* p_target_vtpl;
	va_t* source_va;
{
        return((vtpl_t*)dynva_appdata((dynva_t*)p_target_vtpl, source_va,
                                       VA_GROSS_LEN(source_va)));
}

/*##**********************************************************************\
 * 
 *		vtpl_appvawithincrement
 * 
 * Appends an v-attribute to a v-tuple and appending one null-byte
 * to va.
 * 
 * Parameters : 
 * 
 *	p_target_vtpl - in out, give
 *		pointer to the v-tuple
 *
 *	source_va - in, use
 *		pointer to the v-attribute
 *
 * Return value - ref : 
 * 
 *      the value of target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
vtpl_t* vtpl_appvawithincrement(
	    vtpl_t* target_vtpl,
	    va_t* source_va)
{
        va_index_t len;
        char* data;
        char* dataarea;

        data = va_getdata(source_va, &len);
        dataarea = va_appdataarea(
                    (va_t*)target_vtpl,
                    len + 1 + VA_LEN_LEN(len + 1));
        va_setdataandnull((va_t*)dataarea, data, len);
        return(target_vtpl);
}

/*##**********************************************************************\
 * 
 *		dynvtpl_appvawithincrement
 * 
 * Appends an v-attribute to a dynamic v-tuple and appending one null-byte
 * to va.
 * 
 * Parameters : 
 * 
 *	p_target_vtpl - in out, give
 *		pointer to the v-tuple
 *
 *	source_va - in, use
 *		pointer to the v-attribute
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
vtpl_t* dynvtpl_appvawithincrement(p_target_vtpl, source_va)
	dynvtpl_t* p_target_vtpl;
	va_t* source_va;
{
        va_index_t len;
        char* data;
        char* dataarea;

        data = va_getdata(source_va, &len);
        dataarea = dynva_appdataarea(
                    (dynva_t*)p_target_vtpl,
                    len + 1 + VA_LEN_LEN(len + 1));
        va_setdataandnull((va_t*)dataarea, data, len);
        return(*p_target_vtpl);
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *		vtpl_setvtpl
 * 
 * Set a v-tuple from a v-tuple.
 * 
 * Parameters : 
 * 
 *	target_vtpl - out, use
 *		pointer to the target v-tuple
 *
 *	source_vtpl - in, use
 *		pointer to the source v-tuple
 *
 * Return value - ref : 
 * 
 *      target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vtpl_t* vtpl_setvtpl(target_vtpl, source_vtpl)
	vtpl_t* target_vtpl;
	vtpl_t* source_vtpl;
{
        ss_dassert(target_vtpl != NULL);
        ss_dassert(source_vtpl != NULL);
        return(_VTPL_SETVTPL_(target_vtpl, source_vtpl));
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		dynvtpl_setvtpl
 * 
 * Set a dynamic v-tuple from another v-tuple.
 * 
 * Parameters : 
 * 
 *	p_target_dynvtpl - in out, give
 *		pointer to a dynvtpl variable
 *
 *	source_vtpl - in, use
 *		pointer to a v-tuple
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynvtpl
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
vtpl_t* dynvtpl_setvtpl(p_target_dynvtpl, source_vtpl)
	dynvtpl_t* p_target_dynvtpl;
	vtpl_t* source_vtpl;
{
        register vtpl_index_t datalen;

        ss_dassert(source_vtpl != NULL);
        ss_dassert(p_target_dynvtpl != NULL);

        datalen = VTPL_GROSSLEN(source_vtpl);
        if (*p_target_dynvtpl == NULL) {
             *p_target_dynvtpl = (dynvtpl_t)SsMemAlloc(SS_MAX(DYNVA_MINLEN, datalen));
        } else {
            *p_target_dynvtpl =
                (dynvtpl_t)SsMemRealloc(*p_target_dynvtpl, SS_MAX(DYNVA_MINLEN, datalen));
        }
        return((vtpl_t*)memcpy(*p_target_dynvtpl, source_vtpl, datalen));
}


/*##**********************************************************************\
 * 
 *		vtpl_appvtpl
 * 
 * Appends a v-tuple to a v-tuple.
 * 
 * Parameters : 
 * 
 *	target_vtpl - in out, use
 *		pointer to the target v-tuple
 *
 *	source_vtpl - in, use
 *		pointer to the v-tuple to be appended
 *
 * Return value - ref : 
 * 
 *      target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vtpl_t* vtpl_appvtpl(target_vtpl, source_vtpl)
	vtpl_t* target_vtpl;
	vtpl_t* source_vtpl;
{
        return((vtpl_t*)va_appva((va_t*)target_vtpl, (va_t*)source_vtpl));
}


/*##**********************************************************************\
 * 
 *		dynvtpl_appvtpl
 * 
 * Appends a v-tuple to a dynamic v-tuple.
 * 
 * Parameters : 
 * 
 *	p_target_vtpl - in out, give
 *		pointer to the target v-tuple variable
 *
 *	source_vtpl - in, use
 *		pointer to the v-tuple to be appended
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
vtpl_t* dynvtpl_appvtpl(p_target_vtpl, source_vtpl)
	dynvtpl_t* p_target_vtpl;
	vtpl_t* source_vtpl;
{
        return((vtpl_t*)dynva_appva((dynva_t*)p_target_vtpl, (va_t*)source_vtpl));
}


/*##**********************************************************************\
 * 
 *		vtpl_vacount
 * 
 * Counts the v-attributes in a v-tuple
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *		pointer to the v-tuple
 *
 * Return value : 
 * 
 *      the number of v-attributes
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vtpl_index_t vtpl_vacount(vtpl)
	vtpl_t* vtpl;
{
        char* p_field; /* pointer for mapping over the fields */
        char* vtpl_end; /* vtpl + gross length minus one */
        vtpl_index_t count;
        va_index_t vtpl_len;    /* net length */

        if (BYTE_LEN_TEST(vtpl_len = vtpl->c[0])) {
            vtpl_end = (char*)vtpl + vtpl_len;
            p_field = (char*)vtpl + 1;
        } else {
            vtpl_len = FOUR_BYTE_LEN(vtpl);
            vtpl_end = (char*)vtpl + vtpl_len + 4;
            p_field = (char*)vtpl + 5;
        }
        count = 0;
        while (p_field <= vtpl_end) {
            count++;
            p_field += VA_GROSS_LEN((va_t*)p_field);
        }
        ss_dassert(p_field == vtpl_end + 1);
        return(count);
}

/*##**********************************************************************\
 * 
 *		vtpl_consistent
 * 
 * Checks the consistency of v-attributes in a v-tuple
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *		pointer to the v-tuple
 *
 * Return value : 
 * 
 *      TRUE    - v-tuple is consistent
 *      FALSE   - v-tuple is NOT consistent
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool vtpl_consistent(vtpl)
	vtpl_t* vtpl;
{
        register vtpl_index_t i_field; /* index for mapping over the fields */
        vtpl_index_t vtpl_len; /* gross length minus one */

        if (BYTE_LEN_TEST(vtpl_len = vtpl->c[0])) {
            i_field = 1;
        } else {
            vtpl_len = FOUR_BYTE_LEN(vtpl) + 4;
            i_field = 5;
        }

        while (i_field <= vtpl_len) {
            i_field += VA_GROSS_LEN((va_t*)((ss_byte_t*)vtpl + i_field));
        }
        return(i_field == vtpl_len + 1);
}

/*##**********************************************************************\
 * 
 *		vtpl_getva_at
 * 
 * Returns pointer to the v-attribute at (zero-based) index fieldno
 * 
 * Parameters : 
 * 
 *	vtpl - in, use
 *		pointer to a vtpl
 *
 *	fieldno - in
 *		index of v-attribute within vtpl (first == 0)
 *
 * Return value - ref : 
 * 
 *      pointer to the v-attribute
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* vtpl_getva_at(vtpl, fieldno)
	vtpl_t* vtpl;
	register vtpl_index_t fieldno;
{
        char* p_field; /* pointer for mapping over the fields */
        char* vtpl_end; /* vtpl + gross len - 1 */
        vtpl_index_t vtpl_len; /* net length */

        if (BYTE_LEN_TEST(vtpl_len = vtpl->c[0])) {
            p_field = (char*)vtpl + 1;
            vtpl_end = (char*)vtpl + vtpl_len;
        } else {
            vtpl_len = FOUR_BYTE_LEN(vtpl);
            p_field = (char*)vtpl + 5;
            vtpl_end = (char*)vtpl + vtpl_len + 4;
        }

        while (p_field <= vtpl_end && fieldno > 0) {
            fieldno--;
            p_field += VA_GROSS_LEN((va_t*)p_field);
        }
        /* ss_dassert(p_field <= vtpl_end); initial fieldno was within vtpl?
           Assert removed by JarmoR, Jul 15, 1996
           After ALTER TABLE ADD COLUMN there are implicit NULL columns.
         */
        if (p_field > vtpl_end) {
            return (VA_NULL);
        }
        return((va_t*)p_field);
}


/*##**********************************************************************\
 * 
 *		vtpl_skipva
 * 
 * Skip to next v-attribute within v-tuple.
 * 
 * Parameters : 
 * 
 *	curr_va - in, use
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
va_t* vtpl_skipva(curr_va)
	va_t* curr_va;
{
        va_t* new_va;
        
        new_va = (va_t*)((ss_byte_t*)curr_va + VA_GROSS_LEN(curr_va));
        ss_assert(new_va > curr_va); /* check for overflow */
        return(new_va);
}


/*#**********************************************************************\
 * 
 *		uti_normalize
 * 
 * Remove trailing empty v-attributes from a v-tuple
 * 
 * Parameters : 
 * 
 *	vtpl - in out, use
 *		pointer to a v-tuple
 *
 * Return value : 
 *                
 *      New gross length of the v-tuple, or 0, if the length
 *      didn't change
 *                
 * Limitations  : 
 * 
 * Globals used : 
 */
static vtpl_index_t uti_normalize(vtpl)
	vtpl_t* vtpl;
{
        register vtpl_index_t i_field; /* index for mapping over the fields */
        vtpl_index_t vtpl_len; /* gross length minus one */
        register vtpl_index_t net_len;
        register vtpl_index_t n_empties = 0; /* number of trailing empty v-attributes */
        /* DANGER: SCO MSC has a buggy memmove that will clobber the third
                   register variable. */

        if (BYTE_LEN_TEST(vtpl_len = vtpl->c[0])) {
            i_field = 1; net_len = vtpl_len;
        } else {
            vtpl_len = FOUR_BYTE_LEN(vtpl) + 4;
            i_field = 5; net_len = vtpl_len - 4;
        }

        /* count trailing empty v-attributes */
        while (i_field <= vtpl_len) {
            vtpl_index_t field_len = VA_GROSS_LEN((va_t*)((ss_byte_t*)vtpl + i_field));

            if (field_len == 1) { /* only empty v-attributes have a length of 1 */
                n_empties++;
            } else {
                n_empties = 0;
            }
            i_field += field_len;
        }

        /* remove the trailing empty v-attributes */
        if (n_empties) {
            net_len -= n_empties;
            if (BYTE_LEN_TEST(net_len)) {
                if (!BYTE_LEN_TEST(vtpl_len)) {
                    /* If the length decreased from four to one bytes, move the data */
                    memmove(&vtpl->c[1], &vtpl->c[5], net_len);
                }
                vtpl->c[0] = (ss_byte_t)net_len;
                return(net_len + 1);
            } else {
                SET_FOUR_BYTE_LEN(vtpl, net_len);
                return(net_len + 5);
            }
        } else {
            return(0); /* no need to reallocate */
        }
}


/*##**********************************************************************\
 * 
 *		vtpl_normalize
 * 
 * Remove trailing empty v-attributes from a v-tuple
 * 
 * Parameters : 
 * 
 *	vtpl - in out, use
 *		pointer to a v-tuple
 *
 * Return value - ref : 
 * 
 *      vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vtpl_t* vtpl_normalize(vtpl)
	vtpl_t* vtpl;
{
        uti_normalize(vtpl);
        return(vtpl);
}


/*##**********************************************************************\
 * 
 *		dynvtpl_normalize
 * 
 * Remove trailing empty v-attributes from a dynamiv v-tuple
 * 
 * Parameters : 
 * 
 *	p_vtpl - in out, give
 *		pointer to a v-tuple variable
 *
 * Return value - ref : 
 * 
 *      the new value of *p_vtpl
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vtpl_t* dynvtpl_normalize(p_vtpl)
	dynvtpl_t* p_vtpl;
{
        vtpl_index_t new_len;
        vtpl_t* vtpl = *p_vtpl;

        ss_assert(vtpl != NULL);
        if ((new_len = uti_normalize(vtpl))) {
            *p_vtpl = vtpl = (vtpl_t*)SsMemRealloc(vtpl, new_len);
        }
        return(vtpl);
}


/*##**********************************************************************\
 * 
 *		vtpl_truncate
 * 
 * Truncates a v-tuple so that it is shorter than the given length.
 * 
 * Parameters : 
 * 
 *	vtpl - in out, use
 *		a pointer to the v-tuple to truncate
 *
 *	max_len - in
 *		the maximum gross length in bytes
 *
 * Return value - ref : 
 * 
 *      vtpl
 * 
 * Limitations  : 
 * 
 *      assumes !BYTE_LEN_TEST(max_len-5)
 * 
 * Globals used : 
 */
vtpl_t* vtpl_truncate(vtpl, max_len)
	vtpl_t* vtpl;
	vtpl_index_t max_len;
{
        register vtpl_index_t i_field; /* index for mapping over the fields */
        vtpl_index_t vtpl_len; /* gross length minus one */
        register va_index_t new_l_field;
        va_index_t l_field = 0, new_net_l_field;
        ss_byte_t* p_data;

        if (BYTE_LEN_TEST(vtpl_len = vtpl->c[0])) {
            return(vtpl);
        } else {
            vtpl_len = FOUR_BYTE_LEN(vtpl) + 4;
            i_field = 5;
        }

        if (vtpl_len < max_len) return(vtpl);

        /* Find last field to include */
        while (i_field <= vtpl_len) {
            l_field = VA_GROSS_LEN((va_t*)((ss_byte_t*)vtpl + i_field));
            if (i_field > max_len - l_field) break;
            i_field += l_field;
        }

        new_l_field = max_len - i_field;

        /* Figure out how many bytes of field data we can keep */
        /* DANGER: This code knows the length field representation */
        if (new_l_field == 0) {
            VTPL_SET_LEN(p_data, vtpl, max_len - 5);
            return(vtpl);
        } else if (BYTE_LEN_TEST(new_l_field - 1)) {
            new_net_l_field = new_l_field - 1;
        } else if (BYTE_LEN_TEST(new_l_field - 5)) {
            new_net_l_field = VA_LONGVALIMIT-1; /* as many as possible with byte length */
            new_l_field = VA_LONGVALIMIT; /* left some bytes unused */
        } else {
            new_net_l_field = new_l_field - 5;
        }

        VA_SET_LEN(p_data, (va_t*)((ss_byte_t*)vtpl + i_field), new_net_l_field);
        if (BYTE_LEN_TEST(new_net_l_field) && !BYTE_LEN_TEST(l_field)) {
            /* The length shrank from four to one byte, move the data */
            memmove(p_data, p_data + 4, new_net_l_field);
        }
        VTPL_SET_LEN(p_data, vtpl, i_field + new_l_field - 5);
        return(vtpl);
}

#define VAMAP_STARTSIZE 29
#define VAMAP_SIZEOF(n) \
((sizeof(vtpl_vamap_t) - sizeof(va_t*)) + sizeof(va_t*) * (n))
#define VAMAP_DOUBLE(n) (((n) + 3U) * 2U - 3U)

/*##**********************************************************************\
 * 
 *		vtpl_vamap_alloc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	max_vacount - 
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
vtpl_vamap_t* vtpl_vamap_alloc(int max_vacount)
{
        vtpl_vamap_t* vamap;

        vamap = SsMemAlloc(VAMAP_SIZEOF(max_vacount));
        vamap->vamap_nva = 0;
        vamap->vamap_maxva = max_vacount;

        return(vamap);
}

/*##**********************************************************************\
 * 
 *		vtpl_vamap_init
 * 
 * Creates a fast va-mapping object for a vtuple
 * 
 * Parameters : 
 * 
 *	vtpl - in, hold
 *		pointer to vtuple
 *		
 * Return value - give :
 *      created vamap object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
vtpl_vamap_t* vtpl_vamap_init(vtpl_t* vtpl)
{
        register vtpl_vamap_t* vamap;
        register va_t* p_va;
        register ss_byte_t* vtpl_endmark;
        register vtpl_index_t count = 0;
        vtpl_index_t vtpl_len; /* gross length */

        vamap = SsMemAlloc(VAMAP_SIZEOF(VAMAP_STARTSIZE));
        vamap->vamap_nva = VAMAP_STARTSIZE;
        vamap->vamap_vtpl = vtpl;

        if (BYTE_LEN_TEST(vtpl_len = vtpl->c[0])) {
            vtpl_len += 1;
            p_va = (va_t*)((ss_byte_t*)vtpl + VA_LENGTHMINLEN);
        } else {
            vtpl_len = FOUR_BYTE_LEN(vtpl) + VA_LENGTHMAXLEN;
            p_va = (va_t*)((ss_byte_t*)vtpl + VA_LENGTHMAXLEN);
        }
        vtpl_endmark = (ss_byte_t*)vtpl + vtpl_len;
        while ((ss_byte_t*)p_va < vtpl_endmark) {
            if (count >= vamap->vamap_nva) { /* need to alloc more space? */
                ss_dassert(count == vamap->vamap_nva);
                vamap->vamap_nva = VAMAP_DOUBLE(vamap->vamap_nva);
                vamap = SsMemRealloc(vamap, VAMAP_SIZEOF(vamap->vamap_nva));
            }
            vamap->vamap_arr[count] = p_va;
            p_va = (va_t*)((ss_byte_t*)p_va + VA_GROSS_LEN(p_va));
            count++;
        }
        ss_dassert((ss_byte_t*)p_va == vtpl_endmark);
        if (count != vamap->vamap_nva) {
            ss_dassert(count < vamap->vamap_nva)
            vamap->vamap_nva = count;    
            vamap = SsMemRealloc(vamap, VAMAP_SIZEOF(vamap->vamap_nva));
        }
        vamap->vamap_maxva = count;    
        return(vamap);
}

/*##**********************************************************************\
 * 
 *		vtpl_vamap_refill
 * 
 * Refills vamap from a given v-tuple. Note that the given vamap pointer
 * may not be valid after calling this function. The new, valid pointer
 * value is returned by this function.
 * 
 * Parameters : 
 * 
 *	vamap - in, take
 *		
 *		
 *	vtpl - in
 *		
 *		
 * Return value - give : 
 * 
 *      Pointer to vamap object, which might be different from input
 *      parameter vamap.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
vtpl_vamap_t* vtpl_vamap_refill(vtpl_vamap_t* vamap, vtpl_t* vtpl)
{
        va_t* p_va;
        ss_byte_t* vtpl_endmark;
        vtpl_index_t count = 0;
        vtpl_index_t vtpl_len; /* gross length */

        vamap->vamap_vtpl = vtpl;
        if (BYTE_LEN_TEST(vtpl_len = vtpl->c[0])) {
            vtpl_len += 1;
            p_va = (va_t*)((ss_byte_t*)vtpl + 1);
        } else {
            vtpl_len = FOUR_BYTE_LEN(vtpl) + 5;
            p_va = (va_t*)((ss_byte_t*)vtpl + 5);
        }
        vtpl_endmark = (ss_byte_t*)vtpl + vtpl_len;
        while ((ss_byte_t*)p_va < vtpl_endmark) {
            if (count == vamap->vamap_maxva) {
                vamap->vamap_maxva++;
                vamap = SsMemRealloc(vamap, VAMAP_SIZEOF(vamap->vamap_maxva));
            }
            vamap->vamap_arr[count] = p_va;
            p_va = (va_t*)((ss_byte_t*)p_va + VA_GROSS_LEN(p_va));
            count++;
        }
        ss_dassert((ss_byte_t*)p_va == vtpl_endmark);
        vamap->vamap_nva = count;

        return(vamap);
}

/*##**********************************************************************\
 * 
 *		vtpl_vamap_done
 * 
 * Deletes a va map
 * 
 * Parameters : 
 * 
 *	vamap - in, take
 *		pointer to va-map object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void vtpl_vamap_done(vtpl_vamap_t* vamap)
{
        ss_dassert(vamap != NULL);
        SsMemFree(vamap);
}

/*##**********************************************************************\
 * 
 *		vtpl_vamap_removelast
 * 
 * Removes the last va from the va.
 * 
 * Parameters : 
 * 
 *	vamap - use
 *		pointer to va map
 *		
 * Return value :
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void vtpl_vamap_removelast(vtpl_vamap_t* vamap)
{
        ss_dassert(vamap != NULL);
        ss_dassert(vamap->vamap_nva > 1);
        vamap->vamap_nva--;
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *		vtpl_vamap_getva_at
 * 
 * Fast replacement for vtpl_getva_at() which operates in constant time
 * 
 * Parameters : 
 * 
 *	vamap - in, use
 *		pointer to va map
 *		
 *	fieldno - in
 *		attribute number
 *		
 * Return value - ref :
 *      pointer to va inside the vtuple
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
va_t* vtpl_vamap_getva_at(vtpl_vamap_t* vamap, vtpl_index_t fieldno)
{
        ss_dassert(vamap != NULL);

        return(_VTPL_VAMAP_GETVA_AT_(vamap, fieldno));
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		vtpl_vamap_getvafromcopy_at
 * 
 * Same as vtpl_vamap_getva_at() but this allows usage of vamap
 * to a copy of the original vtpl
 * 
 * Parameters : 
 * 
 *	copied_vtpl - in, use
 *		pointer to copy of original vtuple
 *		
 *	vamap - in, use
 *		va map
 *		
 *	fieldno - in
 *		attribute number from vtpl
 *		
 * Return value - ref :
 *      pointer to va inside copied_vtpl
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
va_t* vtpl_vamap_getvafromcopy_at(
        vtpl_t* copied_vtpl,
        vtpl_vamap_t* vamap,
        vtpl_index_t fieldno)
{

        ss_dassert(vamap != NULL);
        if (fieldno >= vamap->vamap_nva) {
            /* For Alter table */
            return(VA_DEFAULT);
        } else {
            va_index_t offset;
            offset = (va_index_t)((char*)vamap->vamap_arr[fieldno] - (char*)vamap->vamap_vtpl);
            return ((va_t*)((char*)copied_vtpl + offset));
        }
}

/*##**********************************************************************\
 * 
 *		vtpl_vamap_vacount
 * 
 * Gets va count from a va-map (fast replacement for vtpl_vacount())
 * 
 * Parameters : 
 * 
 *	vamap - in, use
 *		pointer to va map
 *		
 * Return value :
 *      count of v-attributes in a vtuple
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
vtpl_index_t vtpl_vamap_vacount(vtpl_vamap_t* vamap)
{
        ss_dassert(vamap != NULL);
        return (vamap->vamap_nva);    
}

bool vtpl_buildvtpltext(char* buf, int buflen, vtpl_t* vtpl)
{
        vtpl_t* vt;
        va_t* va;
        vtpl_index_t field_count;
        vtpl_index_t fieldno;
        va_index_t i;
        va_index_t len;
        char* p;
        char* bufend = buf + buflen - 6; /* 6 = \nn + ) + ) + 0 */
        char localbuf[80];

        if (vtpl == NULL) {
            strcpy(buf, "NULL");
            return(TRUE);
        }

        buf[0] = '\0';

        vt = vtpl;
        SsSprintf(localbuf, "%u(", (uint)vtpl_netlen(vt));
        strcpy(buf, localbuf);
        buf += strlen(localbuf);
        field_count = vtpl_vacount(vt);
        for (fieldno = 0; fieldno < field_count && buf < bufend; fieldno++) {
            va = vtpl_getva_at(vt, fieldno);
            SsSprintf(localbuf, "%u(", (uint)va_netlen(va));
            strcpy(buf, localbuf);
            buf += strlen(localbuf);
            p = va_getdata(va, &len);
            for (i = 0; i < len && buf < bufend; i++) {
                int ch;
                ch = p[i] & 0xff;
                if (ch <= 0x7f && (ss_isalnum(ch) || ch == '_' || ch == ' ')) {
                    SsSprintf(buf, "%c", (uchar)ch);
                    buf += 1;
                } else {
                    SsSprintf(buf, "\\%02X", ch);
                    buf += 3;
                }
            }
            strcpy(buf, ")");
            buf += 1;
        }
        strcpy(buf, ")");
        buf += 1;

        return(TRUE);
}

bool vtpl_printvtpl(void* fp, vtpl_t* vtpl)
{
        char* buf;
        int len;
        char format[10];

        if (vtpl == NULL) {
            SsFprintf(fp, "NULL");
            return(TRUE);
        }

        SsSprintf(format, "%%.%ds\n", SS_MSGLOG_BUFSIZE-128);
        len = 4 * vtpl_grosslen(vtpl);
        buf = SsMemAlloc(len);

        vtpl_buildvtpltext(buf, len, vtpl);

        SsFprintf(fp, format, buf);

        SsMemFree(buf);

        return(TRUE);
}

bool vtpl_dprintvtpl(int level, vtpl_t* vtpl)
{
        char* buf;
        int len;
        char format[10];

        SsSprintf(format, "%%.%ds\n", SS_MSGLOG_BUFSIZE-128);
        len = 4 * vtpl_grosslen(vtpl);
        buf = SsMemAlloc(len);

        vtpl_buildvtpltext(buf, len, vtpl);

        switch (level) {
            case 1:
                SsDbgPrintfFun1(format, buf);
                break;
            case 2:
                SsDbgPrintfFun2(format, buf);
                break;
            case 3:
                SsDbgPrintfFun3(format, buf);
                break;
            case 4:
                SsDbgPrintfFun4(format, buf);
                break;
            default:
                SsDbgPrintf(format, buf);
                break;
        }

        SsMemFree(buf);

        return(TRUE);
}

