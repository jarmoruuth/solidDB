/*************************************************************************\
**  source       * uti0va.h
**  directory    * uti
**  description  * The external interface to v-attributes
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

#ifndef UTI0VA_H
#define UTI0VA_H

#include <ssenv.h>
#include <ssstddef.h>
#include <ssstring.h>
#include <sslimits.h>
#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssfloat.h>
#include <ssint8.h>

/* constants ***********************************************/

/* empty v-attribute */
#define VA_EMPTY    (&va_null)
#define VA_NULL     (&va_null)
#define VA_DEFAULT  (&va_default)
#define VA_MIN      (&va_min)

/* DANGER: The maximums below cannot be portably used in #if's! */

/* maximum length of data in an v-attribute */
#define VA_MAXINDEX     ((va_index_t)-5)
/* maximum length of v-attribute containing int */
#define VA_INTMAXLEN    (sizeof(int)+2)
/* maximum length of v-attribute containing long */
#define VA_LONGMAXLEN   (sizeof(long)+2)
/* maximum length of v-attribute containing ss_int8_t */
#define VA_INT8MAXLEN   (sizeof(ss_int8_t)+2)
/* maximum length of v-attribute containing float */
#define VA_FLOATMAXLEN  (sizeof(float)+1)
/* maximum length of v-attribute containing double */
#define VA_DOUBLEMAXLEN (sizeof(double)+1)
/* maximum # of bytes needed to read the length of a va */
#define VA_LENGTHMAXLEN 5
/* # of bytes to read a short va */
#define VA_LENGTHMINLEN 1

#define VA_LONGVALIMIT  (0xFE)
#define VA_BLOBVABYTE   (0xFF)

#define DYNVA_MINLEN    100


/* types ***************************************************/

typedef ss_uint4_t va_index_t;

typedef struct {
        ss_byte_t c[12]; /* actually variable-sized */
} va_t;

typedef va_t* dynva_t;
typedef va_t* refdva_t;


/* global variables ****************************************/

extern va_t va_null;
extern va_t va_default;
extern va_t va_min;
extern va_t va_minint;
extern va_t va_maxint;


/* global functions ****************************************/

va_t* va_init(va_t* va);
void  va_done(va_t* va);
va_t* va_setva(va_t* target_va, va_t* source_va);
va_t* va_setint(va_t* target_va, int value);
va_t* va_setlong(va_t* target_va, long value);
va_t* va_setint8(va_t* target_va, ss_int8_t value);
#if defined(NO_ANSI_FLOAT)
va_t* va_setfloat(va_t* target_va, double value);
#else
va_t* va_setfloat(va_t* target_va, float value);
#endif
va_t* va_setdouble(va_t* target_va, double value);
va_t* va_setasciiz(va_t* target_va, char* value);
va_t* va_setdata(va_t* target_va, void* data, va_index_t datalen);
va_t* va_setdataandnull(va_t* target_va, void* data, va_index_t datalen);
va_t* va_setdatachar1to2(
        va_t* target_va,
        ss_char1_t* value,
        size_t len);
va_t* va_setdatachar2(
        va_t* target_va,
        ss_char2_t* value,
        size_t len);
bool va_setvadatachar2to1(
        va_t* target_va,
        ss_char2_t* p_data2,
        size_t n);
bool va_setdatachar2to1(
        va_t* target_va,
        ss_char2_t* value,
        size_t len);

va_t* va_setblobdata(va_t* target_va, void* data, va_index_t datalen,
                     void* blobref, va_index_t blobreflen);

va_t* va_appva(va_t* target_va, va_t* source_va);
SS_INLINE va_t* va_appdata(va_t* target_va, void* data, va_index_t datalen);
va_t* va_appdata_ex(va_t* target_va, void* data, va_index_t datalen);
SS_INLINE void* va_appdataarea(va_t* target_va, va_index_t datalen);
void* va_appdataarea_ex(va_t* target_va, va_index_t datalen);

#define dynva_init()        (dynva_t)NULL
#define dynva_done(p_dynva) dynva_free(p_dynva)

va_t* dynva_init_va(va_t* source_va);
va_t* dynva_setva(dynva_t* p_target_dynva, va_t* source_va);
va_t* dynva_setint(dynva_t* p_target_dynva, int value);
va_t* dynva_setlong(dynva_t* p_target_dynva, long value);
va_t* dynva_setint8(dynva_t* p_target_dynva, ss_int8_t value);

#if defined(NO_ANSI_FLOAT)
va_t* dynva_setfloat(dynva_t* p_target_dynva, double value);
#else
va_t* dynva_setfloat(dynva_t* p_target_dynva, float value);
#endif
va_t* dynva_setdouble(dynva_t* p_target_dynva, double value);
va_t* dynva_setasciiz(dynva_t* p_target_dynva, char* value);
va_t* dynva_setdata(dynva_t* p_target_dynva, void* data,
                         va_index_t datalen);
va_t* dynva_setdataandnull(dynva_t* p_target_dynva, void* data,
                            va_index_t datalen);
va_t* dynva_setblobdata(dynva_t* p_target_dynva, void* data,
                        va_index_t datalen, void* blobref,
                        va_index_t blobreflen);
void dynva_free(dynva_t* p_dynva);

SS_INLINE va_t* dynva_appva(dynva_t* p_target_dynva, va_t* source_va);
va_t* dynva_appdata(dynva_t* p_target_dynva, void* data,
                         va_index_t datalen);
void* dynva_appdataarea(dynva_t* p_target_dynva, va_index_t datalen);

int    va_getint(va_t* va);
long   va_getlong(va_t* va);
long   va_getlong_check(va_t* va);
ss_int8_t va_getint8(va_t* p_va);
float  va_getfloat(va_t* va);
double va_getdouble(va_t* va);
SS_INLINE void*  va_getdata(va_t* va, va_index_t* p_len);
char*  va_getasciiz(va_t* va);

va_index_t va_netlen(va_t* va);
FOUR_BYTE_T va_netlen_long(va_t* va);
va_index_t va_grosslen(va_t* va);
FOUR_BYTE_T va_grosslen_long(va_t* va);
va_index_t va_lenlen(va_t* va);

va_index_t va_invnetlen(void* p_invlen);
va_t* va_appinvlen(va_t* p_va);
void va_patchinvlen(void* p_target_buf, va_t* p_va);

#define refdva_init()         (refdva_t)NULL
#define refdva_done(p_refdva) refdva_free(p_refdva)

va_t* refdva_init_va(va_t* source_va);
void  refdva_link(refdva_t refdva);
va_t* refdva_setva(refdva_t* p_target_refdva, va_t* source_va);
va_t* refdva_setlong(refdva_t* p_target_refdva, long value);
va_t* refdva_setint8(refdva_t* p_target_refdva, ss_int8_t value);
#if defined(NO_ANSI_FLOAT)
va_t* refdva_setfloat(refdva_t* p_target_refdva, double value);
#else
va_t* refdva_setfloat(refdva_t* p_target_refdva, float value);
#endif
va_t* refdva_setdouble(refdva_t* p_target_refdva, double value);
va_t* refdva_setasciiz(refdva_t* p_target_refdva, char* value);
va_t* refdva_setdata(refdva_t* p_target_refdva, void* data,
                         va_index_t datalen);
va_t* refdva_setdataandnull(refdva_t* p_target_refdva, void* data,
                            va_index_t datalen);
va_t* refdva_allocblobdata(refdva_t* p_target_refdva, va_index_t blobdatalen);
SS_INLINE void refdva_free(refdva_t* p_refdva);

#ifdef SS_UNICODE_DATA

void refdva_setdatachar1to2(
	refdva_t* p_target_refdva,
	ss_char1_t* value,
        size_t len);
void refdva_setdatachar2(
        refdva_t* p_target_refdva,
        ss_char2_t* value,
        size_t len);
bool refdva_setdatachar2to1(
        refdva_t* p_target_refdva,
        ss_char2_t* value,
        size_t len);
void refdva_setasciiztochar2(
	refdva_t* p_target_refdva,
	ss_char1_t* value);
void refdva_setwcs(
        refdva_t* p_target_refdva,
        ss_char2_t* value);
bool refdva_setwcstochar1(
        refdva_t* p_target_refdva,
        ss_char2_t* value);
bool refdva_setvadatachar2to1(
        refdva_t* p_target_refdva,
        ss_char2_t* p_data2,
        size_t n);
bool refdva_setvachar2to1(
        refdva_t* p_target_refdva,
        va_t* p_source_va);
void refdva_setvachar1to2(
        refdva_t* p_target_refdva,
        va_t* p_source_va);

void va_copydatachar2(
        va_t* p_va,
        ss_char2_t* buf,
        size_t start,
        size_t n,
        size_t* n_copied);

bool va_copydatachar2to1(
        va_t* p_va,
        ss_char1_t* buf,
        size_t start,
        size_t n,
        size_t* n_copied);


#endif /* SS_UNICODE_DATA */

/*##**********************************************************************\
 *
 *		va_testnull
 *
 * Tests if the v-attribute is null v-attribute.
 *
 * Parameters :
 *
 *	va - in, use
 *          the v-attributes that is tested
 *
 * Return value :
 *
 *      TRUE    - va is null
 *      FALSE   - va is not null
 *
 * Limitations  :
 *
 * Globals used :
 */
#define va_testnull(va) ((va)->c[0] == 0)

/*##**********************************************************************\
 *
 *		va_testblob
 *
 * Tests if the v-attribute is blob v-attribute.
 *
 * Parameters :
 *
 *	va - in, use
 *          the v-attributes that is tested
 *
 * Return value :
 *
 *      TRUE    - va is blob
 *      FALSE   - va is not blob
 *
 * Limitations  :
 *
 * Globals used :
 */
#define va_testblob(va) ((va)->c[0] == VA_BLOBVABYTE)

#define VA_LEN_LEN(len) (BYTE_LEN_TEST(len) ? 1 : 5)

#define VTPL_LEN_LEN(len) (BYTE_LEN_TEST(len) ? 1 : 5)

/* Set the length of the v-attribute pointed to by p_va (multiply evaluated!)
   to len, and set p_data (can be p_va) to point to the first data byte. */
#define VA_SET_LEN(p_data, p_va, len) \
        if (BYTE_LEN_TEST(len)) { \
            (p_va)->c[0] = (ss_byte_t)(len); \
            (p_data) = (ss_byte_t*)&(p_va)->c[1]; \
        } else { \
            SET_FOUR_BYTE_LEN(p_va, len) \
            (p_data) = (ss_byte_t*)&(p_va)->c[5]; \
        }

#define VTPL_SET_LEN(p_data, p_vtpl, len) \
        VA_SET_LEN(p_data, p_vtpl, len)

#define VA_SET_LEN_ONLY(p_va, len) \
        if (BYTE_LEN_TEST(len)) { \
            (p_va)->c[0] = (ss_byte_t)(len); \
        } else { \
            SET_FOUR_BYTE_LEN(p_va, len) \
        }

#define VTPL_SET_LEN_ONLY(p_vtpl, len) \
        VA_SET_LEN_ONLY((va_t*)(p_vtpl), len)

/*##**********************************************************************\
 *
 *		SET_FOUR_BYTE_LEN
 *
 * Set the length of the data area of a large v-attribute.
 *
 * Parameters :
 *
 *	va - out, use
 *		pointer to the v-attribute
 *
 *      datalen - in
 *		the length
 *
 * Return value :
 *
 * Limitations  :
 *
 *      On 16-bit machines, this will always store zero in the
 *      two high bytes.
 *
 * Globals used :
 */
#if defined(UNALIGNED_LOAD) && defined(SS_LSB1ST) && WORD_SIZE >= 4
#    define SET_FOUR_BYTE_LEN(va, datalen) {\
            (va)->c[0] = VA_LONGVALIMIT; \
            *(FOUR_BYTE_T*)&(va)->c[1] = (datalen); \
        }
#elif defined(UNALIGNED_LOAD) && defined(SS_LSB1ST) && WORD_SIZE == 2
#    define SET_FOUR_BYTE_LEN(va, datalen) {\
            (va)->c[0] = VA_LONGVALIMIT; \
            *(TWO_BYTE_T*)&(va)->c[1] = (TWO_BYTE_T)(datalen); \
            *(TWO_BYTE_T*)&(va)->c[3] = (TWO_BYTE_T)((datalen) >> 16); \
        }
#else
#    define SET_FOUR_BYTE_LEN(va, datalen) {\
            register va_index_t len_ = (datalen); \
            \
            (va)->c[0] = VA_LONGVALIMIT; \
            (va)->c[1] = (ss_byte_t)len_; \
            (va)->c[2] = (ss_byte_t)(len_ >>= 8); \
            (va)->c[3] = (ss_byte_t)(len_ >>= 8); \
            (va)->c[4] = (ss_byte_t)(len_ >> 8); \
        }
#endif


/*#**********************************************************************\
 *
 *		VA_GROSSLEN
 *
 * Returns the gross length of a v-attribute. The gross length includes
 * the data length and the length of the length bytes.
 *
 * Parameters :
 *
 *	va - in, use
 *          the v-attribute
 *
 * Return value :
 *
 *      gross length of va
 *
 * Limitations  :
 *
 * Globals used :
 */
#define VA_GROSSLEN(va) \
        (BYTE_LEN_TEST((va)->c[0]) ? \
            (va_index_t)(va)->c[0] + VA_LENGTHMINLEN : \
            SS_UINT4_LOADFROMDISK(&(va)->c[1]) + VA_LENGTHMAXLEN)

/*#**********************************************************************\
 *
 *		VA_NETLEN
 *
 * Returns the net length of a v-attribute.
 *
 * Parameters :
 *
 *	va - in, use
 *          the v-attribute
 *
 * Return value :
 *
 *      net length of va
 *
 * Limitations  :
 *
 * Globals used :
 */
#define VA_NETLEN(va) \
        (BYTE_LEN_TEST((va)->c[0]) ? \
            (va_index_t)(va)->c[0] : \
            SS_UINT4_LOADFROMDISK(&(va)->c[1]))


/*#**********************************************************************\
 *
 *		VA_LENLEN
 *
 * Returns the length field length of a v-attribute.
 *
 * Parameters :
 *
 *	va - in, use
 *          the v-attribute
 *
 * Return value :
 *
 *      length field length of va
 *
 * Limitations  :
 *
 * Globals used :
 */
#define VA_LENLEN(va)   \
        ((va)->c[0] < VA_LONGVALIMIT ? VA_LENGTHMINLEN : VA_LENGTHMAXLEN)

/*#**********************************************************************\
 *
 *		VA_GETASCIIZ
 *
 * Returns the pointer to data of v-attribute.
 *
 * Parameters :
 *
 *	va - in, use
 *          the v-attribute
 *
 * Return value :
 *
 *      pointer to va data
 *
 * Limitations  :
 *
 * Globals used :
 */
#define VA_GETASCIIZ(va) \
        ((char*)(va) + VA_LENLEN(va))

#define VA_GROSSLENBYNETLEN(netlen) \
        ((netlen) +\
         ((netlen) < VA_LONGVALIMIT ? VA_LENGTHMINLEN : VA_LENGTHMAXLEN))


#define VA_GETDATA(p_data, p_va, len) \
        (len) = (p_va)->c[0]; \
        if (BYTE_LEN_TEST(len)) { \
            (p_data) = (ss_byte_t*)&(p_va)->c[1];    \
        } else { \
            (len) = FOUR_BYTE_LEN(p_va);    \
            (p_data) = (ss_byte_t*)&(p_va)->c[5];    \
        }

/*#**********************************************************************\
 *
 *              BYTE_LEN_TEST
 *
 * Tests whether the length of an v-attribute is stored as a single byte.
 *
 * Parameters :
 *
 *      first_byte - in, use
 *              the first byte of the v-attribute
 *
 * Return value :
 *
 *      non-zero if the length is a single byte
 *
 * Limitations  :
 *
 * Globals used :
 */
#define BYTE_LEN_TEST(first_byte) ((first_byte) < (ss_byte_t)VA_LONGVALIMIT)

/*#**********************************************************************\
 *
 *		FOUR_BYTE_LEN
 *
 * Returns the length of the data area of a large v-attribute,
 * truncated to a va_index_t.
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
 *      The length of the v-attribute must be in the four-byte
 *      format.  On 16-bit machines, this will only access the
 *      two low bytes.
 *
 * Globals used :
 */
#if defined(UNALIGNED_LOAD) && defined(SS_LSB1ST)
#  define FOUR_BYTE_LEN(va) (*((va_index_t*)&(va)->c[1]))
#else
#  define FOUR_BYTE_LEN(va) \
        (((((((va_index_t)(va)->c[4] << 8) | (va)->c[3]) << 8) | \
            (va)->c[2]) << 8) | (va)->c[1])
#endif

#ifndef SS_DEBUG
#  ifndef va_grosslen
#    define va_grosslen(va) VA_GROSSLEN(va)
#  endif /* !va_grosslen */
#  ifndef va_netlen
#    define va_netlen(va) VA_NETLEN(va)
#  endif /* !va_netlen */
#endif /* !SS_DEBUG */

#ifndef VA_GROSS_LEN

#define VA_GROSS_LEN(va) \
        (BYTE_LEN_TEST((va)->c[0]) ? \
            (va)->c[0] + 1 : \
            (FOUR_BYTE_LEN(va) + 5))

#endif /* VA_GROSS_LEN */

/* internal */
#ifdef SS_REFDVA_MEMLINK
#define refdva_linkcount_dec(refdva) SsMemLinkDec(refdva)
#else /* SS_REFDVA_MEMLINK */
uint refdva_linkcount_dec(refdva_t refdva);
#endif /* SS_REFDVA_MEMLINK */

#if defined(UTI0VA_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		va_getdata
 * 
 * Get a pointer to the data in a v-attribute, and the length of the data.
 * This does not copy the data!
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 *      p_len - out
 *		pointer to an va_index_t variable, the length of the
 *          data is stored into *p_len
 *
 * Return value - ref : 
 * 
 *      pointer to the data
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void* va_getdata(
	va_t* va,
	va_index_t* p_len)
{
        ss_byte_t first_byte;

        if (BYTE_LEN_TEST(first_byte = va->c[0])) {
            *p_len = first_byte;
            return((void*)&va->c[1]);
        } else {
            *p_len = FOUR_BYTE_LEN(va);
            return((void*)&va->c[5]);
        }
}

/*##**********************************************************************\
 * 
 *		va_appdata
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
SS_INLINE va_t* va_appdata(
	va_t* target_va,
	void* data,
	va_index_t datalen)
{
        va_index_t len_target, new_len;
        ss_byte_t* data_target;

        len_target = target_va->c[0];
        new_len = len_target + datalen;

        if (!BYTE_LEN_TEST(new_len)) {
            /* Not a short v-atttribute. */
            return(va_appdata_ex(target_va, data, datalen));
        }

        data_target = ((ss_byte_t*)target_va) + 1;
        
        memcpy(data_target + len_target, data, datalen);

        target_va->c[0] = (ss_byte_t)new_len;

        return(target_va);
}

/*##**********************************************************************\
 * 
 *		va_appdataarea
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
SS_INLINE void* va_appdataarea(
	va_t* target_va,
	va_index_t datalen)
{
        va_index_t len_target, new_len;
        void* dataarea;

        ss_dassert(target_va != NULL);

        len_target = target_va->c[0];
        new_len = len_target + datalen;

        /* find length and data area */
        if (!BYTE_LEN_TEST(new_len)) {
            return(va_appdataarea_ex(target_va, datalen));
        }

        dataarea = &target_va->c[1 + len_target];
        target_va->c[0] = (ss_byte_t)new_len;

        return(dataarea);
}

/*##**********************************************************************\
 * 
 *		dynva_appva
 * 
 * Appends the data from a v-attribute at the end of a dynamic v-attribute.
 * 
 * Parameters : 
 * 
 *	p_target_va - in out, give
 *		pointer to the v-attribute variable to append to
 *
 *	source_va - in, use
 *		pointer to the v-attribute to append from
 *
 * Return value - ref : 
 * 
 *      the new value of *p_target_dynva
 * 
 * Limitations  : 
 * 
 *      the v-attributes may not overlap
 * 
 * Globals used :
 */
SS_INLINE va_t* dynva_appva(
	dynva_t* p_target_dynva,
	va_t* source_va)
{
        va_index_t source_len;
        void* source_data;

        source_data = va_getdata(source_va, &source_len);
        return(dynva_appdata(p_target_dynva, source_data, source_len));
}

/*##**********************************************************************\
 * 
 *		refdva_free
 * 
 * Releases the memory allocated for a refdva.
 * 
 * Parameters : 
 * 
 *	p_refdva - in out, use
 *		pointer to a refdva variable, *p_refdva set to NULL
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
SS_INLINE void refdva_free(
	refdva_t* p_refdva)
{
        ss_dassert(p_refdva != NULL);

        if (*p_refdva != NULL) {

            if (refdva_linkcount_dec(*p_refdva) == 0) {
                SsMemFree(*p_refdva);
            }
            *p_refdva = NULL;
        }
}

#endif /* defined(UTI0VA_C) || defined(SS_USE_INLINE) */

#endif /* UTI0VA_H */
