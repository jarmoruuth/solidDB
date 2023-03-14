/*************************************************************************\
**  source       * uti0vad.c
**  directory    * uti
**  description  * Dynamic v-attributes
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

#include <ssstring.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssfloat.h>
#include "uti0va.h"
#include "uti1val.h"

/* functions ***********************************************/


/*##**********************************************************************\
 * 
 *		dynva_init_va
 * 
 * Creates a new dynva and initializes it with given va
 * 
 * Parameters : 
 * 
 *	source_va - in, use
 *	    va to be used as initial value of new dynva	
 *		
 * Return value - give : 
 *       
 *      Pointer to new dynva 
 *       
 * Limitations  : 
 * 
 * Globals used : 
 */
dynva_t dynva_init_va(va_t* source_va)
{
        dynva_t dynva = NULL;
        dynva_setva(&dynva, source_va);
        return(dynva);
}

/*##**********************************************************************\
 * 
 *		dynva_free
 * 
 * Releases the memory allocated for a dynva.
 * 
 * Parameters : 
 * 
 *	p_dynva - in out, use
 *		pointer to a dynva variable, *p_dynva set to NULL
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
void dynva_free(p_dynva)
	dynva_t* p_dynva;
{
        if (*p_dynva != NULL) {
            SsMemFree(*p_dynva);
            *p_dynva = NULL;
        }
}

/*##**********************************************************************\
 * 
 *		dynva_setva
 * 
 * Set a dynamic v-attribute from another v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	source_va - in, use
 *		pointer to a v-attribute
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setva(p_target_dynva, source_va)
	dynva_t* p_target_dynva;
	va_t* source_va;
{
        va_index_t datalen = VA_GROSS_LEN(source_va);

        if (*p_target_dynva == NULL) {
            *p_target_dynva = (dynva_t)SsMemAlloc(SS_MAX(DYNVA_MINLEN, datalen));
        } else {
            *p_target_dynva = (dynva_t)SsMemRealloc(*p_target_dynva, SS_MAX(DYNVA_MINLEN, datalen));
        }
        return((va_t*)memcpy(*p_target_dynva, source_va, datalen));
}


/*##**********************************************************************\
 * 
 *		dynva_setdata
 * 
 * Set a dynamic v-attribute from a data area.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	data - in, use
 *		pointer to a data area, or NULL
 *
 *      datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setdata(p_target_dynva, data, datalen)
	dynva_t* p_target_dynva;
	void* data;
	va_index_t datalen;
{
        register va_t* p_va;
        size_t valen;
        
        ss_assert(datalen <= VA_MAXINDEX);

        valen = BYTE_LEN_TEST(datalen) ? (datalen + 1) : (datalen + 5);
        if (*p_target_dynva == NULL) {
            p_va = (va_t*)SsMemAlloc(SS_MAX(DYNVA_MINLEN, valen));
        } else {
            p_va = (va_t*)SsMemRealloc(*p_target_dynva, SS_MAX(DYNVA_MINLEN, valen));
        }

        if (BYTE_LEN_TEST(datalen)) {
            p_va->c[0] = (ss_byte_t)datalen;
            if (data != NULL) {
                memcpy(&p_va->c[1], data, datalen);
            }
        } else {
            SET_FOUR_BYTE_LEN(p_va, datalen);
            if (data != NULL) {
                memcpy(&p_va->c[5], data, datalen);
            }
        }
        *p_target_dynva = p_va;

        return(p_va);
}

/*##**********************************************************************\
 * 
 *		dynva_setblobdata
 * 
 * Set a dynamic v-attribute from a blob data area.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	data - in, use
 *		pointer to a data area or NULL to allocate only
 *
 *      datalen - in
 *		length of the data
 *
 *	blobref - in, use
 *		pointer to the blob reference or NULL to allocate only
 *
 *	blobreflen - in
 *		length of the blob reference
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setblobdata(p_target_dynva, data, datalen, blobref, blobreflen)
	dynva_t* p_target_dynva;
	void* data;
	va_index_t datalen;
	void* blobref;
	va_index_t blobreflen;
{
        register va_t* p_va;
	va_index_t totlen;
        size_t valen;
        
        totlen = datalen + blobreflen;

        ss_assert(totlen <= VA_MAXINDEX);

        valen = totlen + 5;
        if (*p_target_dynva == NULL) {
            p_va = (va_t*)SsMemAlloc(SS_MAX(DYNVA_MINLEN, valen));
        } else {
            p_va = (va_t*)SsMemRealloc(*p_target_dynva, SS_MAX(DYNVA_MINLEN, valen));
        }

        SET_FOUR_BYTE_LEN(p_va, totlen);
        p_va->c[0] = (ss_byte_t)VA_BLOBVABYTE;   /* must be after SET_FOUR_BYTE_LEN */
        if (data != NULL) {
            memcpy(&p_va->c[5], data, datalen);
        }
        if (blobref != NULL) {
            memcpy(&p_va->c[5] + datalen, blobref, blobreflen);
        }

        *p_target_dynva = p_va;

        return(p_va);
}

/*##**********************************************************************\
 * 
 *		dynva_setdataandnull
 * 
 * Set a dynamic v-attribute from a data area with a trailing null byte.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	data - in, use
 *		pointer to a data area or NULL to allocate only
 *
 *      datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setdataandnull(p_target_dynva, data, datalen)
	dynva_t* p_target_dynva;
	void* data;
	va_index_t datalen;
{
        register va_t* p_va;
        size_t valen;
	va_index_t resultdatalen;
        char* p_vadata;

        resultdatalen = datalen + 1;
        
        ss_assert(resultdatalen <= VA_MAXINDEX);

        valen = BYTE_LEN_TEST(resultdatalen)
                    ? (resultdatalen + 1)
                    : (resultdatalen + 5);

        if (*p_target_dynva == NULL) {
            p_va = (va_t*)SsMemAlloc(SS_MAX(DYNVA_MINLEN, valen));
        } else {
            p_va = (va_t*)SsMemRealloc(*p_target_dynva, SS_MAX(DYNVA_MINLEN, valen));
        }

        if (BYTE_LEN_TEST(resultdatalen)) {
            p_va->c[0] = (ss_byte_t)resultdatalen;
            p_vadata = (char*)&p_va->c[1];
        } else {
            SET_FOUR_BYTE_LEN(p_va, resultdatalen);
            p_vadata = (char*)&p_va->c[5];
        }
        if (data != NULL) {
            memcpy(p_vadata, data, datalen);
        }
        p_vadata[datalen] = '\0';
        *p_target_dynva = p_va;

        return(p_va);
}


/*##**********************************************************************\
 * 
 *		dynva_setasciiz
 * 
 * Set a dynamic v-attribute from an asciiz string.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	value - in, use
 *		pointer to an asciiz string
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setasciiz(p_target_dynva, value)
	dynva_t* p_target_dynva;
	char* value;
{
        return(dynva_setdata(p_target_dynva, value, strlen(value)+1));
}


/*##**********************************************************************\
 * 
 *		dynva_setint
 * 
 * Set an int to a dynamic v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	value - in
 *		the integer
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setint(p_target_dynva, value)
	dynva_t* p_target_dynva;
	int value;
{
        va_t temp_va;
        register va_index_t len;

        va_setint(&temp_va, value);
        len = temp_va.c[0];
        ss_dassert(len + 1 <= sizeof(temp_va));
        if (*p_target_dynva == NULL) {
            *p_target_dynva = (dynva_t)SsMemAlloc(len + 1);
        } else {
            *p_target_dynva = (dynva_t)SsMemRealloc(*p_target_dynva, len + 1);
        }
        return((va_t*)memcpy(*p_target_dynva, &temp_va, len + 1));
}


/*##**********************************************************************\
 * 
 *		dynva_setlong
 * 
 * Set a long to a dynamic v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	value - in
 *		the long
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setlong(p_target_dynva, value)
	dynva_t* p_target_dynva;
	long value;
{
        va_t temp_va;
        register va_index_t len;

        va_setlong(&temp_va, value);
        len = temp_va.c[0];
        ss_dassert(len + 1 <= sizeof(temp_va));
        if (*p_target_dynva == NULL) {
            *p_target_dynva = (dynva_t)SsMemAlloc(len + 1);
        } else {
            *p_target_dynva = (dynva_t)SsMemRealloc(*p_target_dynva, len + 1);
        }
        return((va_t*)memcpy(*p_target_dynva, &temp_va, len + 1));
}

/* Enabled 2005-12-22, MMEg2 needs this.  apl */
#if 1 /* probably not needed at this time because we use refdva_setint8 / Pete 2001-08-20 */

/*##**********************************************************************\
 * 
 *		dynva_setint8
 * 
 * Set a 8-byte integer to a dynamic v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	value - in
 *		the int8 value
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setint8(dynva_t* p_target_dynva, ss_int8_t value)
{
        va_t temp_va;
        va_index_t len;

        va_setint8(&temp_va, value);
        len = temp_va.c[0];
        ss_dassert(len + 1 <= sizeof(temp_va));
        if (*p_target_dynva == NULL) {
            *p_target_dynva = (dynva_t)SsMemAlloc(len + 1);
        } else {
            *p_target_dynva = (dynva_t)SsMemRealloc(*p_target_dynva, len + 1);
        }
        return((va_t*)memcpy(*p_target_dynva, &temp_va, len + 1));
}

#endif /* 0 */

/*##**********************************************************************\
 * 
 *		dynva_setfloat
 * 
 * Set a float to a dynamic v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	value - in
 *		the float
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
#ifdef NO_ANSI
va_t* dynva_setfloat(p_target_dynva, value)
	register dynva_t* p_target_dynva;
	double value;
#elif defined(NO_ANSI_FLOAT)
va_t* dynva_setfloat(register dynva_t* p_target_dynva, double value)
#else
va_t* dynva_setfloat(register dynva_t* p_target_dynva, float value)
#endif
{
        if (*p_target_dynva == NULL) {
            *p_target_dynva = (dynva_t)SsMemAlloc(VA_FLOATMAXLEN);
        } else {
            *p_target_dynva = (dynva_t)SsMemRealloc(*p_target_dynva, VA_FLOATMAXLEN);
        }
        return(va_setfloat(*p_target_dynva, value));
}


/*##**********************************************************************\
 * 
 *		dynva_setdouble
 * 
 * Set a double to a dynamic v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_dynva - in out, give
 *		pointer to a dynva variable
 *
 *	value - in
 *		the double
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* dynva_setdouble(p_target_dynva, value)
	register dynva_t* p_target_dynva;
	double value;
{
        if (*p_target_dynva == NULL) {
             *p_target_dynva = (dynva_t)SsMemAlloc(VA_DOUBLEMAXLEN);
        } else {
            *p_target_dynva = (dynva_t)SsMemRealloc(*p_target_dynva, VA_DOUBLEMAXLEN);
        }
        return(va_setdouble(*p_target_dynva, value));
}


/*##**********************************************************************\
 * 
 *		dynva_appdata
 * 
 * Appends the data from a data area at the end of a dynamic v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_va - in out, give
 *		pointer to the v-attribute variable to append to
 *
 *	data - in, use
 *		pointer to the data area to append from
 *
 *      datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 *      the v-attribute may not overlap the data area
 * 
 * Globals used :
 */
va_t* dynva_appdata(p_target_dynva, data, datalen)
	dynva_t* p_target_dynva;
	void* data;
	va_index_t datalen;
{
        register va_index_t len_target, new_len;
        va_t* target_va = *p_target_dynva;

        if (target_va == NULL) {
            return dynva_setdata(p_target_dynva, data, datalen);
        }

        /* find length and data area */
        if (!BYTE_LEN_TEST(len_target = target_va->c[0])) {
            len_target = FOUR_BYTE_LEN(target_va);
        }
        ss_assert(len_target < VA_MAXINDEX - datalen);
        new_len = len_target + datalen;

        if (BYTE_LEN_TEST(new_len)) {
            target_va = (va_t*)SsMemRealloc(target_va, SS_MAX(DYNVA_MINLEN, new_len + 1));
            memcpy(&target_va->c[1 + len_target], data, datalen);
            target_va->c[0] = (ss_byte_t)new_len;
        } else {
            target_va = (va_t*)SsMemRealloc(target_va, SS_MAX(DYNVA_MINLEN, new_len + 5));

            /* If the length grew from one to four bytes, move target data */
            if (BYTE_LEN_TEST(len_target)) {
                memmove(&target_va->c[5], &target_va->c[1], len_target);
            }
        
            memcpy(&target_va->c[5 + len_target], data, datalen);
            SET_FOUR_BYTE_LEN(target_va, new_len);
        }

        *p_target_dynva = target_va;
        return(target_va);
}

/*##**********************************************************************\
 * 
 *		dynva_appdataarea
 * 
 * Appends the data area at the end of a dynamic v-attribute. No actual
 * data is copied, just the area is reserved. Return value is pointer
 * to the start of the data area.
 * 
 * Parameters : 
 * 
 *	p_target_va - in out, give
 *		pointer to the v-attribute variable to append to
 *
 *      datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      pointer to the appended data area
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
void* dynva_appdataarea(p_target_dynva, datalen)
	dynva_t* p_target_dynva;
	va_index_t datalen;
{
        register va_index_t len_target, new_len;
        va_t* target_va = *p_target_dynva;
        void* dataarea;

        if (target_va != NULL) {
            /* find length and data area */
            if (!BYTE_LEN_TEST(len_target = target_va->c[0])) {
                len_target = FOUR_BYTE_LEN(target_va);
            }
            ss_assert(len_target < VA_MAXINDEX - datalen);
        } else {
            len_target = 0;
        }
        new_len = len_target + datalen;

        if (BYTE_LEN_TEST(new_len)) {
            target_va = (va_t*)SsMemRealloc(target_va, SS_MAX(DYNVA_MINLEN, new_len + 1));
            dataarea = &target_va->c[1 + len_target];
            target_va->c[0] = (ss_byte_t)new_len;
        } else {
            target_va = (va_t*)SsMemRealloc(target_va, SS_MAX(DYNVA_MINLEN, new_len + 5));

            /* If the length grew from one to four bytes, move target data */
            if (BYTE_LEN_TEST(len_target)) {
                memmove(&target_va->c[5], &target_va->c[1], len_target);
            }
        
            dataarea = &target_va->c[5 + len_target];
            SET_FOUR_BYTE_LEN(target_va, new_len);
        }

        *p_target_dynva = target_va;
        return(dataarea);
}
