/*************************************************************************\
**  source       * uti0var.c
**  directory    * uti
**  description  * Dynamic reference v-attributes
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

#include <ssstring.h>
#ifdef SS_UNICODE_DATA
#include <sswcs.h>
#endif /* SS_UNICODE_DATA */
#include <ssdebug.h>
#include <ssmem.h>
#include <ssfloat.h>
#include <sslimits.h>
#include "uti0va.h"
#include "uti1val.h"

/* functions ***********************************************/


#ifdef SS_REFDVA_MEMLINK

#define LINKCOUNT_SIZE          0

#else /* SS_REFDVA_MEMLINK */

#define LINKCOUNT_SIZE          2
#define LINKCOUNT_VALUE(p)      (((uchar)(p)[0] << 8) | (uchar)(p)[1])
#define LINKCOUNT_STORE(p, c)   { \
                                    p[0] = (uchar)((c) >> 8); \
                                    p[1] = (uchar)(c); \
                                }

#endif /* SS_REFDVA_MEMLINK */
/*#***********************************************************************\
 * 
 *		refdva_linkcount_init
 * 
 * 
 * 
 * Parameters : 
 * 
 *	refdva - 
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
#ifdef SS_REFDVA_MEMLINK
#define refdva_linkcount_init(refdva) SsMemLinkInit(refdva)
#else /* SS_REFDVA_MEMLINK */
static void refdva_linkcount_init(refdva_t refdva)
{
        uchar* p;

        p = (uchar*)refdva + VA_GROSS_LEN(refdva);

        LINKCOUNT_STORE(p, 1);
}
#endif /* SS_REFDVA_MEMLINK */

/*#***********************************************************************\
 * 
 *		refdva_linkcount_inc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	refdva - 
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
#ifdef SS_REFDVA_MEMLINK
#define refdva_linkcount_inc(refdva) SsMemLinkInc(refdva)
#else /* SS_REFDVA_MEMLINK */
static uint refdva_linkcount_inc(refdva_t refdva)
{
        uint linkcount;
        uchar* p;

        p = (uchar*)refdva + VA_GROSS_LEN(refdva);

        linkcount = LINKCOUNT_VALUE(p);

        ss_dassert(linkcount < (uint)0xffff);
        linkcount++;

        LINKCOUNT_STORE(p, linkcount);

        return(linkcount);
}
#endif /* SS_REFDVA_MEMLINK */

/*##**********************************************************************\
 * 
 *		refdva_linkcount_dec
 * 
 * 
 * 
 * Parameters : 
 * 
 *	refdva - 
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
#ifndef SS_REFDVA_MEMLINK
uint refdva_linkcount_dec(refdva_t refdva)
{
        uint linkcount;

        uchar* p;

        p = (uchar*)refdva + VA_GROSS_LEN(refdva);

        linkcount = LINKCOUNT_VALUE(p);
        ss_dassert(linkcount > 0);

        linkcount--;

        LINKCOUNT_STORE(p, linkcount);

        return(linkcount);
}
#endif /* SS_REFDVA_MEMLINK */

/*##**********************************************************************\
 * 
 *		refdva_init_va
 * 
 * Creates a new refdva and initializes it with given va
 * 
 * Parameters : 
 * 
 *	source_va - in, use
 *	    va to be used as initial value of new refdva	
 *		
 * Return value - give : 
 *       
 *      Pointer to new refdva 
 *       
 * Limitations  : 
 * 
 * Globals used : 
 */
refdva_t refdva_init_va(va_t* source_va)
{
        refdva_t refdva = NULL;

        refdva_setva(&refdva, source_va);
        return(refdva);
}

/*##**********************************************************************\
 * 
 *		refdva_link
 * 
 * Links one more user to the reference va.
 * 
 * Parameters : 
 * 
 *	refdva - in out, use
 *		refdva variable
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
void refdva_link(refdva)
	refdva_t refdva;
{
        ss_dassert(refdva != NULL);

        refdva_linkcount_inc(refdva);
}

/*##**********************************************************************\
 * 
 *		refdva_setva
 * 
 * Set a dynamic reference v-attribute from another v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *	source_va - in, use
 *		pointer to a v-attribute
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* refdva_setva(p_target_refdva, source_va)
	refdva_t* p_target_refdva;
	va_t* source_va;
{
        va_t* p_va;
#ifdef SS_REFDVA_MEMLINK
        refdva_free(p_target_refdva);
        p_va = dynva_setva(p_target_refdva, source_va);
#else /* SS_REFDVA_MEMLINK */
        va_index_t datalen = VA_GROSS_LEN(source_va);

        refdva_free(p_target_refdva);
        p_va = dynva_setdata(p_target_refdva, NULL, datalen + LINKCOUNT_SIZE);
        memcpy(p_va, source_va, datalen);
#endif /* SS_REFDVA_MEMLINK */

        refdva_linkcount_init(p_va);

        return(p_va);
}


/*##**********************************************************************\
 * 
 *		refdva_setdata
 * 
 * Set a dynamic reference v-attribute from a data area.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *	data - in, use
 *		pointer to a data area, or NULL
 *
 *      datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* refdva_setdata(p_target_refdva, data, datalen)
	refdva_t* p_target_refdva;
	void* data;
	va_index_t datalen;
{
        va_t* p_va;

        refdva_free(p_target_refdva);
#ifdef SS_REFDVA_MEMLINK
        p_va = dynva_setdata(p_target_refdva, data, datalen);
#else /* SS_REFDVA_MEMLINK */
        p_va = dynva_setdata(p_target_refdva, NULL, datalen + LINKCOUNT_SIZE);
        va_setdata(p_va, data, datalen);
#endif /* SS_REFDVA_MEMLINK */
        
        refdva_linkcount_init(p_va);

        return(p_va);
}

/*##**********************************************************************\
 * 
 *		refdva_setdataandnull
 * 
 * Set a dynamic reference v-attribute from a data area with a trailing
 * null byte.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *	data - in, use
 *		pointer to a data area or NULL to allocate only
 *
 *      datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* refdva_setdataandnull(p_target_refdva, data, datalen)
	refdva_t* p_target_refdva;
	void* data;
	va_index_t datalen;
{
        va_t* p_va;
#ifdef SS_REFDVA_MEMLINK
        refdva_free(p_target_refdva);
        p_va = dynva_setdataandnull(p_target_refdva, data, datalen);
#else /* SS_REFDVA_MEMLINK */

        size_t valen;
	va_index_t resultdatalen;
        char* p_vadata;

        refdva_free(p_target_refdva);

        resultdatalen = datalen + 1;
        
        ss_assert(resultdatalen <= VA_MAXINDEX);

        valen = BYTE_LEN_TEST(resultdatalen)
                    ? (resultdatalen + 1)
                    : (resultdatalen + 5);

        p_va = (va_t*)SsMemAlloc(valen + LINKCOUNT_SIZE);

        if (BYTE_LEN_TEST(resultdatalen)) {
            p_va->c[0] = (ss_byte_t)resultdatalen;
            p_vadata = (char*)&p_va->c[1];
        } else {
            SET_FOUR_BYTE_LEN(p_va, resultdatalen)
            p_vadata = (char*)&p_va->c[5];
        }
        if (data != NULL) {
            memcpy(p_vadata, data, datalen);
        }
        p_vadata[datalen] = '\0';
        *p_target_refdva = p_va;
#endif /* SS_REFDVA_MEMLINK */

        refdva_linkcount_init(p_va);

        return(p_va);
}

/*##**********************************************************************\
 * 
 *		refdva_allocblobdata
 * 
 * Allocates a dynamic reference v-attribute for a blob data area.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *      blobdatalen - in
 *		length of the blob data
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* refdva_allocblobdata(p_target_refdva, blobdatalen)
	refdva_t* p_target_refdva;
	va_index_t blobdatalen;
{
        va_t* p_va;

        refdva_free(p_target_refdva);

        p_va = dynva_setblobdata(
                    p_target_refdva,
                    NULL,
                    blobdatalen + LINKCOUNT_SIZE,
                    NULL,
                    0);
        va_setblobdata(p_va, NULL, blobdatalen, NULL, 0);
        
        refdva_linkcount_init(p_va);

        return(p_va);
}

/*##**********************************************************************\
 * 
 *		refdva_setasciiz
 * 
 * Set a dynamic reference v-attribute from an asciiz string.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *	value - in, use
 *		pointer to an asciiz string
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* refdva_setasciiz(p_target_refdva, value)
	refdva_t* p_target_refdva;
	char* value;
{
        return(refdva_setdata(p_target_refdva, value, strlen(value)+1));
}

/*##**********************************************************************\
 * 
 *		refdva_setlong
 * 
 * Set a long to a dynamic reference v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *	value - in
 *		the long
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* refdva_setlong(p_target_refdva, value)
	refdva_t* p_target_refdva;
	long value;
{
        va_t* p_va;
#ifdef SS_REFDVA_MEMLINK
        va_t temp_va;
        size_t size;

        refdva_free(p_target_refdva);
        va_setlong(&temp_va, value);
        size = temp_va.c[0] + 1;
        ss_dassert(size == va_grosslen(&temp_va));
        p_va = *p_target_refdva = SsMemAlloc(size);
        memcpy(*p_target_refdva, &temp_va, size);
#else /* SS_REFDVA_MEMLINK */
        va_t temp_va;
        register va_index_t len;

        va_setlong(&temp_va, value);
        len = temp_va.c[0];

        refdva_free(p_target_refdva);

        p_va = dynva_setdata(p_target_refdva, NULL, len + 1 + LINKCOUNT_SIZE);
        va_setva(p_va, &temp_va);
#endif /* SS_REFDVA_MEMLINK */

        refdva_linkcount_init(p_va);

        return(p_va);
}


/*##**********************************************************************\
 * 
 *		refdva_setint8
 * 
 * Set a 8-byte integer to a dynamic reference v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *	value - in
 *		the long
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* refdva_setint8(
        refdva_t* p_target_refdva,
        ss_int8_t value)
{
        va_t* p_va;
#ifdef SS_REFDVA_MEMLINK
        va_t temp_va;
        size_t size;

        refdva_free(p_target_refdva);
        va_setint8(&temp_va, value);
        size = temp_va.c[0] + 1;
        ss_dassert(size == va_grosslen(&temp_va));
        p_va = *p_target_refdva = SsMemAlloc(size);
        memcpy(*p_target_refdva, &temp_va, size);
#else /* SS_REFDVA_MEMLINK */
        va_t temp_va;
        register va_index_t len;

        va_setint8(&temp_va, value);
        len = temp_va.c[0];

        refdva_free(p_target_refdva);

        p_va = dynva_setdata(p_target_refdva, NULL, len + 1 + LINKCOUNT_SIZE);
        va_setva(p_va, &temp_va);
#endif /* SS_REFDVA_MEMLINK */

        refdva_linkcount_init(p_va);

        return(p_va);
}

/*##**********************************************************************\
 * 
 *		refdva_setfloat
 * 
 * Set a float to a dynamic reference v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *	value - in
 *		the float
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
#ifdef NO_ANSI
va_t* refdva_setfloat(p_target_refdva, value)
	register refdva_t* p_target_refdva;
	double value;
#elif defined(NO_ANSI_FLOAT)
va_t* refdva_setfloat(register refdva_t* p_target_refdva, double value)
#else
va_t* refdva_setfloat(register refdva_t* p_target_refdva, float value)
#endif
{
        va_t* p_va;

        refdva_free(p_target_refdva);
        *p_target_refdva = (refdva_t)SsMemAlloc(VA_FLOATMAXLEN + LINKCOUNT_SIZE);
        p_va = va_setfloat(*p_target_refdva, value);

        refdva_linkcount_init(p_va);

        return(p_va);
}


/*##**********************************************************************\
 * 
 *		refdva_setdouble
 * 
 * Set a double to a dynamic reference v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - in out, give
 *		pointer to a refdva variable
 *
 *	value - in
 *		the double
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_refdva
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
va_t* refdva_setdouble(p_target_refdva, value)
	register refdva_t* p_target_refdva;
	double value;
{
        va_t* p_va;

        refdva_free(p_target_refdva);

        *p_target_refdva = (refdva_t)SsMemAlloc(VA_DOUBLEMAXLEN + LINKCOUNT_SIZE);
        p_va = va_setdouble(*p_target_refdva, value);

        refdva_linkcount_init(*p_target_refdva);

        return(p_va);
}

#ifdef SS_UNICODE_DATA

/*##**********************************************************************\
 * 
 *		refdva_setdatachar1to2
 * 
 * Sets 1 byte per char data buffer to UNICODE va
 * and also appends a _single_ nul-byte to distinguish between
 * NULL value and empty string value
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		pointer to refdva
 *		
 *	value - in, use
 *		pointer to character buffer
 *		
 *	len - in
 *		length of data in buffer
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void refdva_setdatachar1to2(
	refdva_t* p_target_refdva,
	ss_char1_t* value,
        size_t len)
{
        va_t* p_va;
        size_t i;
        ss_char2_t* p_data;
        va_index_t datalen;

        p_va = refdva_setdata(
                p_target_refdva,
                NULL,
                len * sizeof(ss_char2_t) + 1);
        p_data = va_getdata(p_va, &datalen);
        for (i = 0; i < len; i++, p_data++, value++) {
            uint c = (uint)(ss_byte_t)*value;
            SS_CHAR2_STORE_MSB1ST(
                p_data, c);
        }
        *((ss_byte_t*)p_data) = '\0';
}

/*##**********************************************************************\
 * 
 *		refdva_setdatachar2
 * 
 * Sets wide character buffer contents to UNICODE refdva
 * and also appens a _single_ nul-byte to distinguish between
 * NULL value and empty string value
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		target
 *		
 *	value - in, use
 *		source buffer
 *		
 *	len - in
 *		length of source data in 2-byte characters
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void refdva_setdatachar2(
        refdva_t* p_target_refdva,
        ss_char2_t* value,
        size_t len)
{
        va_t* p_va;
        size_t i;
        ss_char2_t* p_data;
        va_index_t datalen;

        p_va = refdva_setdata(
                p_target_refdva,
                NULL,
                len * sizeof(ss_char2_t) + 1);
        p_data = va_getdata(p_va, &datalen);
        for (i = 0; i < len; i++, p_data++, value++) {
            uint c = (uint)*value;
            SS_CHAR2_STORE_MSB1ST(
                p_data, c);
        }
        *((ss_byte_t*)p_data) = '\0';
}

/*##**********************************************************************\
 * 
 *		refdva_setdatachar2to1
 * 
 * Sets 2-byte character buffer data to CHAR va
 * and also appens a _single_ nul-byte to distinguish between
 * NULL value and empty string value
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		target va
 *		
 *	value - in, use
 *		source buffer
 *		
 *	len - in
 *		source buffer length in 2-byte characters in native byte
 *      order and native alignment.
 *		
 * Return value :
 *      TRUE when successful
 *      FALSE when 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool refdva_setdatachar2to1(
        refdva_t* p_target_refdva,
        ss_char2_t* value,
        size_t len)
{
        va_t* p_va;
        ss_char1_t* p_data;
        va_index_t datalen;
        bool succp;

        p_va = refdva_setdata(
                p_target_refdva,
                NULL,
                len * sizeof(ss_char1_t) + 1);
        p_data = va_getdata(p_va, &datalen);
        succp = SsWbuf2Str(p_data, value, len);
        p_data[len] = '\0';
        return (succp);
}

/*##**********************************************************************\
 * 
 *		refdva_setasciiztochar2
 * 
 * Sets a char string to ((LONG)?VAR)?UNICODE va
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		pointer to target refdva
 *		
 *	value - in, use
 *		string
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void refdva_setasciiztochar2(
	refdva_t* p_target_refdva,
	ss_char1_t* value)
{
        size_t len;

        len = strlen(value);
        refdva_setdatachar1to2(p_target_refdva, value, len);
}

/*##**********************************************************************\
 * 
 *		refdva_setwcs
 * 
 * Sets wide char string to refdva
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		pointer to target refdva
 *		
 *	value - in, use
 *		wide char string
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void refdva_setwcs(refdva_t* p_target_refdva, ss_char2_t* value)
{
        size_t len;

        len = SsWcslen(value);
        refdva_setdatachar2(p_target_refdva, value, len);
}

/*##**********************************************************************\
 * 
 *		refdva_setwcstochar1
 * 
 * Copies Wide-char string to ((LONG)?VAR)?CHAR
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		pointer to target refdva
 *		
 *	value - in, use
 *		wide char string
 *		
 * Return value :
 *      TRUE if assignment is successful
 *      FALSE if assign failed (WCS contains chars > 0x00FF)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool refdva_setwcstochar1(refdva_t* p_target_refdva, ss_char2_t* value)
{
        size_t len;
        bool succp;

        len = SsWcslen(value);
        succp = refdva_setdatachar2to1(p_target_refdva, value, len);
        return (succp);
}

/*##**********************************************************************\
 * 
 *		refdva_setvadatachar2to1
 * 
 * Sets a va-format (MSB 1ST, UNALIGNED) wide char data to refdva.
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		target refdva
 *		
 *	p_data2 - in, use
 *		source data
 *		
 *	n - number of wide chars in buffer pointed to by p_data2
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
bool refdva_setvadatachar2to1(
        refdva_t* p_target_refdva,
        ss_char2_t* p_data2,
        size_t n)
{
        va_t* p_va;
        va_index_t datalen1;
        ss_char1_t* p_data1;
        bool succp = TRUE;

        p_va = refdva_setdata(
                p_target_refdva,
                NULL,
                n + 1);
        p_data1 = va_getdata(p_va, &datalen1);
        ss_dassert(datalen1 == n + 1);
        for (; n != 0; n--, p_data2++, p_data1++) {
            ss_char2_t c;

            c = SS_CHAR2_LOAD_MSB1ST(p_data2);
            if (c & (ss_char2_t)~0x00ff) {
                succp = FALSE;
                *p_data1 = SsWcs2StrGetDefChar();
            } else {
                *p_data1 = (ss_char1_t)c;
            }
        }
        *p_data1 = '\0';
        return (succp);
}
/*##**********************************************************************\
 * 
 *		refdva_setvachar2to1
 * 
 * Assigns a UNICODE va to CHAR refdva
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		pointer to target refdva
 *		
 *	p_source_va - in, use
 *		pointer to source va
 *		
 * Return value :
 *      TRUE - success
 *      FALSE - failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool refdva_setvachar2to1(refdva_t* p_target_refdva, va_t* p_source_va)
{
        ss_char2_t* p_data2;
        va_index_t datalen2;
        bool succp;

        p_data2 = va_getdata(p_source_va, &datalen2);
        if (datalen2 == 0) {
            refdva_setva(p_target_refdva, p_source_va);
            return (TRUE);
        }
        ss_dassert(datalen2 & 1);
        datalen2 /= 2;
        succp = refdva_setvadatachar2to1(p_target_refdva, p_data2, datalen2);
        return (succp);
}

/*##**********************************************************************\
 * 
 *		refdva_setvachar1to2
 * 
 * Sets value of CHAR va to UNICODE va
 * 
 * Parameters : 
 * 
 *	p_target_refdva - use
 *		pointer to target refdva
 *		
 *	p_source_va - in, use
 *		pointer to source va
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void refdva_setvachar1to2(refdva_t* p_target_refdva, va_t* p_source_va)
{
        va_t* p_va;
        ss_char1_t* p_data1;
        ss_char2_t* p_data2;
        va_index_t datalen1;
        va_index_t datalen2;
        size_t len;

        p_data1 = va_getdata(p_source_va, &datalen1);
        if (datalen1 == 0) {
            len = 0;
        } else {
            len = (datalen1 - 1) * sizeof(ss_char2_t) + 1;
        }
        p_va = refdva_setdata(
                p_target_refdva,
                NULL,
                len);
        p_data2 = va_getdata(p_va, &datalen2);
        for (datalen2 /= 2; datalen2 != 0; datalen2--, p_data1++, p_data2++) {
            uint c = (uint)(ss_byte_t)*p_data1;
            SS_CHAR2_STORE_MSB1ST(p_data2, c);
        }
        *((ss_byte_t*)p_data2) = '\0';
}

#endif /* SS_UNICODE_DATA */

