/*************************************************************************\
**  source       * uti0vab.c
**  directory    * uti
**  description  * v-attributes basic stuff
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

#define UTI0VA_C

#include <ssstring.h>
#include <ssc.h>
#include <sslimits.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sswcs.h>
#include "uti0va.h"
#include "uti1val.h"

/* global variables ****************************************/

/* va_default is distinguished from va_null by pointer comparison. */
va_t va_null    = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
va_t va_default = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
va_t va_min     = {{1, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
va_t va_minint  = {{1, 1, 0, 0, 0, 0, 0, 0, 0, 0}};
va_t va_maxint  = {{9, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0}};


/* functions ***********************************************/

/*************************************************************************\
 * functions to init and free v-attributes                               *
\*************************************************************************/

/*##**********************************************************************\
 * 
 *		va_init
 * 
 * Initializes a va with VA_EMPTY
 * 
 * Parameters : 	 - none
 * 
 * Return value - ref : 
 * 
 *      Pointer to initialized va
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* va_init(va_t* va)
{
        return(va_setva(va, VA_NULL));
}

/*##**********************************************************************\
 * 
 *		va_done
 * 
 * Clears a va
 * 
 * Parameters : 
 * 
 *	va - in out, use
 *	    va to be cleared	
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void va_done(va_t* va)
{
        va_setva(va, VA_NULL);
}

/*************************************************************************\
 * functions to get length                                               *
\*************************************************************************/


/*##**********************************************************************\
 * 
 *		va_netlen
 * 
 * Returns the length of the data area of a v-attribute, truncated to
 * va_index_t.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the length of the data area truncated to va_index_t
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef va_netlen
va_index_t va_netlen(va)
        va_t* va;
{
        register ss_byte_t first_byte;

        ss_dassert(va != NULL);
        if (BYTE_LEN_TEST(first_byte = va->c[0])) {
            return(first_byte);
        } else {
            return(FOUR_BYTE_LEN(va));
        }
}
#endif /* !va_netlen */

/*##**********************************************************************\
 * 
 *		va_netlen_long
 * 
 * Returns the length of the data area of a v-attribute.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the length of the data area
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
FOUR_BYTE_T va_netlen_long(va)
	va_t* va;
{
        register ss_byte_t first_byte;

        ss_dassert(va != NULL);
        if (BYTE_LEN_TEST(first_byte = va->c[0])) {
            return(first_byte);
        } else {
            return(FOUR_BYTE_LEN_LONG(va));
        }
}


/*##**********************************************************************\
 * 
 *		va_grosslen
 * 
 * Returns the length of the whole v-attribute, truncated to va_index_t.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the length of the v-attribute truncated to va_index_t
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef va_grosslen
va_index_t va_grosslen(va)
	va_t* va;
{
        ss_byte_t first_byte;
        ss_dassert(va != NULL);
        if (BYTE_LEN_TEST(first_byte = va->c[0])) {
            return(first_byte + 1);
        } else {
            return(FOUR_BYTE_LEN(va) + 5);
        }
}
#endif /* !va_grosslen */

/*##**********************************************************************\
 * 
 *		va_grosslen_long
 * 
 * Returns the length of the whole v-attribute.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the length of the v-attribute
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
FOUR_BYTE_T va_grosslen_long(va)
	va_t* va;
{
        ss_byte_t first_byte;

        ss_dassert(va != NULL);
        if (BYTE_LEN_TEST(first_byte = va->c[0])) {
            return(first_byte + 1);
        } else {
            return(FOUR_BYTE_LEN_LONG(va) + 5);
        }
}


/*##**********************************************************************\
 * 
 *		va_lenlen
 * 
 * Returns the length of the length area of a v-attribute.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the length of the length area
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_index_t va_lenlen(va)
	va_t* va;
{
        ss_dassert(va != NULL);
        return(BYTE_LEN_TEST(va->c[0]) ? 1 : 5);
}


/*************************************************************************\
 * functions to store data in v-attributes                                  *
\*************************************************************************/


/*##**********************************************************************\
 * 
 *		va_setva
 * 
 * Set contents of target v-attribute from source v-attribute.
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to target v-attribute
 *
 *	source_va - in, use
 *		pointer to source v-attribute
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* va_setva(target_va, source_va)
	va_t* target_va;
	va_t* source_va;
{
        ss_dassert(source_va != NULL);
        ss_dassert(target_va != NULL);

        return((va_t*)memcpy(target_va, source_va, VA_GROSS_LEN(source_va)));
}


/*##**********************************************************************\
 * 
 *		va_setasciiz
 * 
 * Set a string value to an v-attribute
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to a v-attribute
 *
 *	value - in, use
 *		an ascii-zero terminated string
 *
 * 
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* va_setasciiz(target_va, value)
	register va_t* target_va;
	register char* value;
{
        return(va_setdata(target_va, value, strlen(value)+1));
}


/*##**********************************************************************\
 * 
 *		va_setdata
 * 
 * Set arbitrary data into a v-attribute.
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to a v-attribute
 *
 *	p_data - in, use
 *		pointer to the data, if NULL no data is copied but
 *          the length part of the va is set correctly
 *
 *	datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* va_setdata(target_va, p_data, datalen)
	register va_t* target_va;
	void* p_data;
	register va_index_t datalen;
{
        register void* p_va;

        ss_dassert(target_va != NULL);
        ss_dassert(datalen <= VA_MAXINDEX);

        if (BYTE_LEN_TEST(datalen)) {
            target_va->c[0] = (ss_byte_t)datalen;
            p_va = (void*)&(target_va->c[1]);
        } else {
            SET_FOUR_BYTE_LEN(target_va, datalen);
            p_va = (void*)&(target_va->c[5]);
        }
        if (p_data != NULL) {
            memcpy(p_va, p_data, datalen);
        }
        return(target_va);
}

/*##**********************************************************************\
 * 
 *          va_setdatachar1to2
 * 
 * Sets 1 byte per char data buffer to UNICODE va
 * and also appens a _single_ nul-byte to distinguish between
 * NULL value and empty string value
 * 
 * Parameters : 
 * 
 *      target_va - out, use
 *          pointer to target va
 *		
 *      value - in, use
 *          pointer to character buffer
 *		
 *      len - in
 *          length of data in buffer
 *		
 * Return value :
 *      pointer to va (to follow va_setdata convention)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
va_t* va_setdatachar1to2(
        va_t* target_va,
        ss_char1_t* value,
        size_t len)
{
        va_t* p_va;
        size_t i;
        ss_char2_t* p_data;
        va_index_t datalen;

        p_va = va_setdata(
                target_va,
                NULL,
                len * sizeof(ss_char2_t) + 1);
        p_data = va_getdata(p_va, &datalen);
        for (i = 0; i < len; i++, p_data++, value++) {
            uint c = (uint)(ss_byte_t)*value;
            SS_CHAR2_STORE_MSB1ST(
                p_data, c);
        }
        *((ss_byte_t*)p_data) = '\0';
        return (p_va);
}

/*##**********************************************************************\
 * 
 *          va_setdatachar2
 * 
 * Sets 2 byte per char data buffer to UNICODE va
 * and also appens a _single_ nul-byte to distinguish between
 * NULL value and empty string value
 * 
 * Parameters : 
 * 
 *      target_va - out, use
 *          pointer to target va
 *		
 *      value - in, use
 *          pointer to character buffer
 *		
 *      len - in
 *          length of data in buffer
 *		
 * Return value :
 *      pointer to va (to follow va_setdata convention)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
va_t* va_setdatachar2(
        va_t* target_va,
        ss_char2_t* value,
        size_t len)
{
        va_t* p_va;
        size_t i;
        ss_char2_t* p_data;
        va_index_t datalen;

        p_va = va_setdata(
                target_va,
                NULL,
                len * sizeof(ss_char2_t) + 1);
        p_data = va_getdata(p_va, &datalen);
        for (i = 0; i < len; i++, p_data++, value++) {
            uint c = (uint)*value;
            SS_CHAR2_STORE_MSB1ST(
                p_data, c);
        }
        *((ss_byte_t*)p_data) = '\0';
        return (p_va);
}

/*##**********************************************************************\
 * 
 *		va_setvadatachar2to1
 * 
 * Sets a va-format (MSB 1ST, UNALIGNED) wide char data to va.
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		target va
 *		
 *	p_data2 - in, use
 *		source data
 *		
 *	n - number of wide chars in buffer pointed to by p_data2
 *		
 *		
 * Return value :
 *      TRUE - conversion was successful,
 *      FALSE - loss of data in conversion (done anyway).
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool va_setvadatachar2to1(
        va_t* target_va,
        ss_char2_t* p_data2,
        size_t n)
{
        va_t* p_va;
        va_index_t datalen1;
        ss_char1_t* p_data1;
        bool succp = TRUE;

        p_va = va_setdata(
                target_va,
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
 *		va_setdatachar2to1
 * 
 * Sets 2-byte character buffer data to CHAR va
 * and also appens a _single_ nul-byte to distinguish between
 * NULL value and empty string value
 * 
 * Parameters : 
 * 
 *	target_va - use
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
bool va_setdatachar2to1(
        va_t* target_va,
        ss_char2_t* value,
        size_t len)
{
        va_t* p_va;
        ss_char1_t* p_data;
        va_index_t datalen;
        bool succp;

        p_va = va_setdata(
                target_va,
                NULL,
                len * sizeof(ss_char1_t) + 1);
        p_data = va_getdata(p_va, &datalen);
        succp = SsWbuf2Str(p_data, value, len);
        p_data[len] = '\0';
        return (succp);
}

/*##**********************************************************************\
 * 
 *		va_setdataandnull
 * 
 * Set arbitrary data into a v-attribute with a trailing null byte.
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to a v-attribute
 *
 *	p_data - in, use
 *		pointer to the data, if NULL no data is copied but
 *          the length part of the va is set correctly
 *
 *	datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* va_setdataandnull(target_va, p_data, datalen)
	register va_t* target_va;
	void* p_data;
	va_index_t datalen;
{
	register va_index_t resultdatalen;
        register char* p_vadata;

        resultdatalen = datalen + 1;
        
        ss_dassert(resultdatalen <= VA_MAXINDEX);

        if (BYTE_LEN_TEST(resultdatalen)) {
            target_va->c[0] = (ss_byte_t)resultdatalen;
            p_vadata = (char*)&(target_va->c[1]);
        } else {
            SET_FOUR_BYTE_LEN(target_va, resultdatalen);
            p_vadata = (char*)&(target_va->c[5]);
        }
        if (p_data != NULL) {
            memcpy(p_vadata, p_data, datalen);
        }
        p_vadata[datalen] = '\0';
        return(target_va);
}

/*##**********************************************************************\
 * 
 *		va_setblobdata
 * 
 * Set arbitrary blob data into a v-attribute.
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to a v-attribute
 *
 *	p_data - in, use
 *		pointer to the data, or NULL if setting only length fields
 *
 *	datalen - in
 *		length of the data
 *
 *	p_blobref - in, use
 *		pointer to the blob reference, or NULL if setting only
 *          length fields
 *
 *	blobreflen - in
 *		length of the blob reference
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* va_setblobdata(target_va, p_data, datalen, p_blobref, blobreflen)
	register va_t* target_va;
	void* p_data;
	va_index_t datalen;
	void* p_blobref;
	va_index_t blobreflen;
{
	register va_index_t totlen;
        register char* p_va;

        totlen = datalen + blobreflen;
        
        ss_dassert(totlen <= VA_MAXINDEX);

        SET_FOUR_BYTE_LEN(target_va, totlen);
        target_va->c[0] = (ss_byte_t)VA_BLOBVABYTE;
        p_va = (char*)&(target_va->c[5]);

        if (p_data != NULL) {
            memcpy(p_va, p_data, datalen);
        }
        if (p_blobref != NULL) {
            memcpy(p_va + datalen, p_blobref, blobreflen);
        }

        return(target_va);
}

/*##**********************************************************************\
 * 
 *		va_getasciiz
 * 
 * Returns a char pointer to the data in the v-attribute.  This does not
 * copy the data, which is assumed to be an asciiz string.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value - ref : 
 * 
 *      pointer to the data
 * 
 * Limitations  : 
 * 
 *      doesn't check for the presence of zero bytes in the data
 * 
 * Globals used :
 */
char* va_getasciiz(va)
	va_t* va;
{
        register ss_byte_t first_byte;

        first_byte = va->c[0];
        ss_dassert(first_byte != 0);    /* not null */
        if (BYTE_LEN_TEST(first_byte)) {
            return((void*)&va->c[1]);
        } else {
            return((void*)&va->c[5]);
        }
}


/*************************************************************************\
 * functions to append to v-attributes                                   *
\*************************************************************************/


/*##**********************************************************************\
 * 
 *		va_appva
 * 
 * Appends the data from a v-attribute at the end of another v-attribute.
 * NOTE: There must be enough space in the target, including four extra
 * bytes, if the length grows beyond 255.
 * 
 * Parameters : 
 * 
 *	target_va - in out, use
 *		pointer to the v-attribute to append to
 *
 *	source_va - in, use
 *		pointer to the v-attribute to append from
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 *      the v-attributes may not overlap
 * 
 * Globals used : 
 */
va_t* va_appva(target_va, source_va)
	va_t* target_va;
	va_t* source_va;
{
        va_index_t source_len;
        register void* source_data;

        source_data = va_getdata(source_va, &source_len);
        return(va_appdata(target_va, source_data, source_len));
}


/*##**********************************************************************\
 * 
 *		va_appdata_ex
 * 
 * Appends data at the end of a v-attribute.
 * NOTE: There must be enough space in the target, including four extra
 * bytes, if the length grows beyond 255.
 * 
 * Parameters : 
 * 
 *	target_va - in out, use
 *		pointer to the v-attribute
 *
 *	data - in, use
 *		pointer to the data
 *
 *	datalen - in
 *		length of the data
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 *      the arguments may not overlap
 * 
 * Globals used : 
 */
va_t* va_appdata_ex(
	va_t* target_va,
	void* data,
	va_index_t datalen)
{
        register va_index_t len_target, new_len;
        ss_byte_t* data_target;

        /* find length and data area */
        if (BYTE_LEN_TEST(len_target = target_va->c[0])) {
            data_target = ((ss_byte_t*)target_va) + 1;
        } else {
            len_target = FOUR_BYTE_LEN(target_va);
            data_target = ((ss_byte_t*)target_va) + 5;
        }
        ss_dassert(len_target < VA_MAXINDEX - datalen);
        new_len = len_target + datalen;

        /* If the length grew from one to four bytes, move target data */
        if (BYTE_LEN_TEST(len_target) && !BYTE_LEN_TEST(new_len)) {
            memmove(data_target + 4, data_target, len_target);
            data_target += 4;
        }
        
        memcpy(data_target + len_target, data, datalen);

        if (BYTE_LEN_TEST(new_len)) {
            target_va->c[0] = (ss_byte_t)new_len;
        } else {
            SET_FOUR_BYTE_LEN(target_va, new_len);
        }
        return(target_va);
}

/*##**********************************************************************\
 * 
 *		va_appdataarea_ex
 * 
 * Appends the data area at the end of a v-attribute. No actual
 * data is copied, just the area is reserved. Return value is pointer
 * to the start of the data area.
 * 
 * Parameters : 
 * 
 *	p_target_va - in out
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
void* va_appdataarea_ex(
	    va_t* target_va,
	    va_index_t datalen)
{
        register va_index_t len_target, new_len;
        void* dataarea;

        ss_assert(target_va != NULL);

        /* find length and data area */
        if (!BYTE_LEN_TEST(len_target = target_va->c[0])) {
            len_target = FOUR_BYTE_LEN(target_va);
        }
        ss_assert(len_target < VA_MAXINDEX - datalen);
        new_len = len_target + datalen;

        if (BYTE_LEN_TEST(new_len)) {
            dataarea = &target_va->c[1 + len_target];
            target_va->c[0] = (ss_byte_t)new_len;
        } else {

            /* If the length grew from one to four bytes, move target data */
            if (BYTE_LEN_TEST(len_target)) {
                memmove(&target_va->c[5], &target_va->c[1], len_target);
            }
        
            dataarea = &target_va->c[5 + len_target];
            SET_FOUR_BYTE_LEN(target_va, new_len);
        }

        return(dataarea);
}

/*************************************************************************\
 * functions to support va's (vtuple's) inverse scanning                 *
\*************************************************************************/

/*##**********************************************************************\
 * 
 *		va_invnetlen
 * 
 * Peeks inverse length field from a va. It is needed for va's
 * where there is an additional length field in the end in order
 * to support scanning of va's in inverse direction. This is
 * for reading a sorted file stream in inverse direction
 * 
 * Parameters : 
 * 
 *	p_invlen - in
 *		pointer to last byte position of the inverse length field
 *		
 * Return value :
 *      va net length.
 * 
 * Comments :
 *      this must only be used when the va_lenlen() has returned 5
 * 
 * Globals used : 
 * 
 * See also : 
 */
va_index_t va_invnetlen(void* p_invlen)
{
        va_index_t last_byte;

        last_byte = (va_index_t)*(ss_byte_t*)p_invlen;
        if (BYTE_LEN_TEST(last_byte)) {
            return (last_byte);
        }
        return (FOUR_BYTE_LEN((va_t*)((char*)p_invlen - 5)));
}

/*##**********************************************************************\
 * 
 *		va_appinvlen
 * 
 * Append an inverse length field after va. The va, itself
 * is left untouched, ie. va_grosslen() does not change.
 * 
 * Parameters : 
 * 
 *	p_va - in out, use
 *		pointer to a va where there is at lest va_lenlen() bytes of
 *          extra storage after it
 *		
 * Return value : 
 *      pointer to start of va
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
va_t* va_appinvlen(va_t* p_va)
{
        ss_byte_t* p;

        p = (ss_byte_t*)p_va + va_grosslen(p_va);
        if (!BYTE_LEN_TEST(*(ss_byte_t*)p_va)) {
            memcpy(p, &p_va->c[1], 4);
            p += 4;
        }
        *p = p_va->c[0];
        return (p_va);
}

/*##**********************************************************************\
 * 
 *		va_patchinvlen
 * 
 * Append an inverse length field into target_buf
 * 
 * Parameters : 
 * 
 *	p_target_buf - out,use
 *		
 *		
 *	p_va - in, use
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
void va_patchinvlen(void* p_target_buf, va_t* p_va)
{
        ss_byte_t* p;

        p = (ss_byte_t*)p_target_buf;
        if (!BYTE_LEN_TEST(*(ss_byte_t*)p_va)) {
            memcpy(p, &p_va->c[1], 4);
            p += 4;
        }
        *p = p_va->c[0];
}

#ifdef SS_UNICODE_DATA

/*##**********************************************************************\
 * 
 *		va_copydatachar2
 * 
 * Copies wide character (UNICODE) data from va to user buffer.
 * user buffer contents are in the native byte order.
 * terminating nul is not added automatically
 * 
 * Parameters : 
 * 
 *	p_va - in, use
 *		pointer to UNICODE va
 *		
 *	buf - out, use
 *		buffer where data will be copied
 *		
 *	start - in
 *		number of wide chars to skip from beginning of va
 *		
 *	n - in
 *		max number of wide chars to copy
 *		
 *	n_copied - out, use
 *		pointer where the number of copied wide characters
 *          will be assigned
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void va_copydatachar2(
        va_t* p_va,
        ss_char2_t* buf,
        size_t start,
        size_t n,
        size_t* n_copied)
{
        va_index_t datalen;
        ss_char2_t* p;

        p = va_getdata(p_va, &datalen);
        datalen /= sizeof(ss_char2_t);
        if (start >= datalen) {
            *n_copied = 0;
            return;
        }
        p += start;
        if (datalen > start) {
            datalen -= start;
        } else {
            datalen = 0;
        }
        if (datalen > n) {
            datalen = n;
        } else {
            n = datalen;
        }
        for (; datalen != 0; datalen--, p++, buf++) {
            *buf = SS_CHAR2_LOAD_MSB1ST(p);
        }
        *n_copied = n;
}

/*##**********************************************************************\
 * 
 *		va_copydatachar2to1
 * 
 * Copies data from UNICODE va to user character buffer
 * 
 * Parameters : 
 * 
 *	p_va - in, use
 *		pointer to UNICODE va
 *		
 *	buf - out, use
 *		user buffer
 *		
 *	start - in
 *		number of UNICODE chars to skip from va
 *		
 *	n - in
 *		max number of characters to copy
 *		
 *	n_copied - out, use
 *		pointer to variable where the number of
 *          characters copied will be stored
 *		
 * Return value :
 *      TRUE - success
 *      FALSE - failed because va contains characters
 *      that cannot be expressed in 8-bit chars
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool va_copydatachar2to1(
        va_t* p_va,
        ss_char1_t* buf,
        size_t start,
        size_t n,
        size_t* n_copied)
{
        va_index_t datalen;
        ss_char2_t* p;

        p = va_getdata(p_va, &datalen);
        datalen /= sizeof(ss_char2_t);
        if (start >= datalen) {
            *n_copied = 0;
            return TRUE;
        }
        p += start;
        if (datalen > start) {
            datalen -= start;
        } else {
            datalen = 0;
        }
        if (datalen > n) {
            datalen = n;
        } else {
            n = datalen;
        }
        for (; datalen != 0; datalen--, p++, buf++) {
            uint c;
            c = SS_CHAR2_LOAD_MSB1ST(p);
            if (c > 0x00FF) {
                *n_copied = n - datalen;
                return (FALSE);
            }
            *buf = (ss_char1_t)c;
        }
        *n_copied = n;
        return (TRUE);
}

#endif /* SS_UNICODE_DATA */
