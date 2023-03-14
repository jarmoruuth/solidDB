/*************************************************************************
**  source       * su0collation.h
**  directory    * su
**  description  * Collation sequence support API
**               * 
**  author       * SOLID / pete
**  date         * 2007-02-01
**               * 
**               * Copyright (C) 2007 Solid Information Technology Ltd
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

#ifndef SU0COLLATION_H
#define SU0COLLATION_H

#include <ssstring.h>
#include "su0parr.h"

/* Characetr set in which the string is encoded.
 * There are only supported charsets this enum.
 */
typedef enum {
    SUC_DEFAULT = 0,
    SUC_BIN,
    SUC_BIG5,
    SUC_CP932,
    SUC_EUCJPMS,
    SUC_EUCKR,
    SUC_GB2312,
    SUC_GBK,
    SUC_LATIN1,
    SUC_LATIN2,
    SUC_SJIS,
    SUC_TIS620,
    SUC_UCS2,
    SUC_UJIS,
    SUC_UTF8,
    SUC_CP1250,
} su_charset_t;

typedef struct su_collation_st su_collation_t;

typedef bool su_collation_initbuf_t(
        su_collation_t* coll,
        char* collation_name, /* UTF8 encoding! */
        void* data);

typedef void su_collation_donebuf_t(
        su_collation_t* coll);
    
typedef int su_collation_compare_t(
        su_collation_t* coll,
        void* str1,
        size_t str1_bytelen,
        void* str2,
        size_t str2_bytelen);

typedef size_t su_collation_get_maxlen_for_weightstr_t(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_prefix_charlen);

typedef bool su_collation_create_weightstr_t(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_prefix_charlen,
        void* weightstr_buf,
        size_t weightstr_bufsize,
        size_t* p_weightstr_bytelen);

typedef size_t su_collation_get_bytes_in_chars_t(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_charlen);

typedef su_charset_t su_collation_get_charset_t(
        su_collation_t* coll);

typedef size_t su_collation_numcells_t(
        su_collation_t* coll,
        const char* begin,
        const char* end);

typedef int su_collation_mb_wc_t(
        su_collation_t* coll,
        unsigned long* wide_char,
        const unsigned char* begin,
        const unsigned char* end);

struct su_collation_st {
        void* coll_data;
        char* coll_name;
        su_collation_initbuf_t*                  coll_initbuf;
        su_collation_donebuf_t*                  coll_donebuf;
        su_collation_compare_t*                  coll_compare;
        su_collation_get_maxlen_for_weightstr_t* coll_get_maxlen_for_weightstr;
        su_collation_create_weightstr_t*         coll_create_weightstr;
        su_collation_get_bytes_in_chars_t*       coll_get_bytes_in_chars;
        su_collation_get_charset_t*              coll_get_charset;
        su_collation_numcells_t*                 coll_numcells;
        su_collation_mb_wc_t*                    coll_mb_wc;
};

SS_INLINE char*                                   su_collation_getname(su_collation_t* coll);
SS_INLINE su_collation_initbuf_t                  su_collation_initbuf;
SS_INLINE su_collation_donebuf_t                  su_collation_donebuf;
SS_INLINE su_collation_compare_t                  su_collation_compare;
SS_INLINE su_collation_get_maxlen_for_weightstr_t su_collation_get_maxlen_for_weightstr;
SS_INLINE su_collation_create_weightstr_t         su_collation_create_weightstr;
SS_INLINE su_collation_get_bytes_in_chars_t       su_collation_get_bytes_in_chars;
SS_INLINE su_charset_t                            su_collation_get_charset(su_collation_t* coll);
SS_INLINE size_t                                  su_collation_numcells(su_collation_t* coll, const char* begin, const char* end);
SS_INLINE int                                     su_collation_mb_wc(
                                                        su_collation_t* coll,
                                                        unsigned long* wide_char,
                                                        const unsigned char* begin,
                                                        const unsigned char* end);

#ifdef SS_FAKE

su_collation_initbuf_t su_collation_fake_elbonian_initbuf;

#define ELBONONIAN_INITIFNEEDED(coll) \
    do {\
        if ((coll)->coll_name == NULL) { \
            su_collation_fake_elbonian_initbuf(coll, NULL, NULL); \
        }\
    } while (FALSE)
#else /* SS_FAKE */
#define ELBONONIAN_INITIFNEEDED(coll)
#endif /* SS_FAKE */

#if defined(SS_USE_INLINE) || defined(SU0COLLATION_C)

SS_INLINE char* su_collation_getname(su_collation_t* coll)
{
        ELBONONIAN_INITIFNEEDED(coll);
        return (coll->coll_name);
}

SS_INLINE bool su_collation_initbuf(
        su_collation_t* coll,
        char* collation_name, /* UTF8 encoding! */
        void* data)
{
        bool success;

        ELBONONIAN_INITIFNEEDED(coll);
        success = (*coll->coll_initbuf)(coll, collation_name, data);
        ss_dassert(strcmp(coll->coll_name, collation_name) == 0);
        return (success);
}

SS_INLINE void su_collation_donebuf(
        su_collation_t* coll)
{
        (*coll->coll_donebuf)(coll);
}
    
SS_INLINE int su_collation_compare(
        su_collation_t* coll,
        void* str1,
        size_t str1_bytelen,
        void* str2,
        size_t str2_bytelen)
{
        int cmp;

        ELBONONIAN_INITIFNEEDED(coll);
        cmp = (*coll->coll_compare)(coll,
                                    str1, str1_bytelen,
                                    str2, str2_bytelen);
        return (cmp);
}

SS_INLINE size_t su_collation_get_maxlen_for_weightstr(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_prefix_charlen)
{
        size_t maxlen;

        ELBONONIAN_INITIFNEEDED(coll);
        maxlen = (*coll->coll_get_maxlen_for_weightstr)(
                    coll,
                    str,
                    str_bytelen,
                    str_prefix_charlen);

        return (maxlen);
}

SS_INLINE bool su_collation_create_weightstr(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_prefix_charlen,
        void* weightstr_buf,
        size_t weightstr_bufsize,
        size_t* p_weightstr_bytelen)
{
        bool success;

        ELBONONIAN_INITIFNEEDED(coll);
        success = (*coll->coll_create_weightstr)(coll,
                                                 str,
                                                 str_bytelen,
                                                 str_prefix_charlen,
                                                 weightstr_buf,
                                                 weightstr_bufsize,
                                                 p_weightstr_bytelen);
        return (success);
}

SS_INLINE size_t su_collation_get_bytes_in_chars(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_charlen)
{
        size_t res;

        ELBONONIAN_INITIFNEEDED(coll);
        res = (*coll->coll_get_bytes_in_chars)(coll,
                                                   str,
                                                   str_bytelen,
                                                   str_charlen);
        return (res);
}

SS_INLINE su_charset_t su_collation_get_charset(su_collation_t* coll)
{
        su_charset_t cs;

        ELBONONIAN_INITIFNEEDED(coll);
        cs = (*coll->coll_get_charset)(coll);

        return cs;
}

SS_INLINE size_t su_collation_numcells(
        su_collation_t* coll,
        const char* begin,
        const char* end)
{
        size_t num;

        ELBONONIAN_INITIFNEEDED(coll);
        num = (*coll->coll_numcells)(coll, begin, end);

        return num;
}

SS_INLINE int su_collation_mb_wc(
        su_collation_t* coll,
        unsigned long* wide_char,
        const unsigned char* begin,
        const unsigned char* end)
{
        int res;

        ELBONONIAN_INITIFNEEDED(coll);
        res = (*coll->coll_mb_wc)(coll, wide_char, begin, end);

        return res;
}

#endif /* SS_USE_INLINE || SU0COLLATION_C */

void su_collation_init(void);

void su_collation_done(void);

bool su_collation_supported(uint collation_id);

bool su_collation_supported_charset(const char * charset_name);

su_pa_t* su_collation_get_collations(void);

#ifdef SS_FAKE

extern su_collation_t su_collation_fake_elbonian;
void su_collation_fake_elbonian_link_in(void);

#endif /* SS_FAKE */

#endif /* SU0COLLATION_H */
