/*************************************************************************
**  source       * su0collation.c
**  directory    * su
**  description  * Collation sequence support API
**               * 
**  author       * SOLID / pete
**  date         * 2007-02-01
**               * 
**               * Copyright (C) 2007 Solid Information Technology Ltd
*************************************************************************/
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

#define SU0COLLATION_C

#include <ssenv.h>
#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include "su0parr.h"
#include "su0collation.h"

/* Supported character sets and collations */

static uint su_collation_allowed [] = {
        47,     /* latin1/latin1_bin collation          */
        63,     /* binary/binary collation              */
        5,      /* latin1/latin1_german1_ci             */
        8,      /* latin1/latin1_swedish_ci             */
        15,     /* latin1/latin1_danish_ci              */
        31,     /* latin1/latin1_german2_ci             */
        33,     /* utf8/utf8_general_ci                 */
        35,     /* ucs2/ucs2_general_ci                 */
        48,     /* latin1/latin1_general_ci             */
        49,     /* latin1/latin1_general_cs             */
        83,     /* utf8/utf8_bin                        */
        90,     /* ucs2/ucs2_bin collation              */
        94,     /* latin1/latin1_spanish_ci             */
        128,    /* ucs2/ucs2_unicode_ci                 */
        136,    /* ucs2/ucs2_swedish_ci                 */
        192,    /* utf8/utf8_unicode_ci                 */
        200,    /* utf8/utf8_swedish_ci                 */
        129,    /* ucs2/ucs2_icelandic_ci               */
        130,    /* ucs2/ucs2_latvian_ci                 */
        131,    /* ucs2/ucs2_romanian_ci                */
        132,    /* ucs2/ucs2_slovenian_ci               */
        133,    /* ucs2/ucs2_polish_ci                  */
        134,    /* ucs2/ucs2_estonian_ci                */
        135,    /* ucs2/ucs2_spanish_ci                 */
        137,    /* ucs2/ucs2_turkish_ci                 */
        138,    /* ucs2/ucs2_czech_ci                   */
        139,    /* ucs2/ucs2_danish_ci                  */
        140,    /* ucs2/ucs2_lithuanian_ci              */
        141,    /* ucs2/ucs2_slovak_ci                  */
        142,    /* ucs2/ucs2_spanish2_ci                */
        143,    /* ucs2/ucs2_roman_ci                   */
        144,    /* ucs2/ucs2_persian_ci                 */
        145,    /* ucs2/ucs2_esperanto_ci               */
        146,    /* ucs2/ucs2_hungarian_ci               */
        95,     /* cp932/cp932_japanese_ci              */
        97,     /* eucjpms/eucjpms_japanese_ci          */
        193,    /* utf8/utf8_icelandic_ci               */
        194,    /* utf8/utf8_latvian_ci                 */
        195,    /* utf8/utf8_romanian_ci                */
        196,    /* utf8/utf8_slovenian_ci               */
        197,    /* utf8/utf8_polish_ci                  */
        198,    /* utf8/utf8_estonian_ci                */
        199,    /* utf8/utf8_spanish_ci                 */
        201,    /* utf8/utf8_turkish_ci                 */
        202,    /* utf8/utf8_czech_ci                   */
        203,    /* utf8/utf8_danish_ci                  */
        204,    /* utf8/utf8_lithuanian_ci              */
        205,    /* utf8/utf8_slovak_ci                  */
        206,    /* utf8/utf8_spanish2_ci                */
        207,    /* utf8/utf8_roman_ci                   */
        208,    /* utf8/utf8_persian_ci                 */
        209,    /* utf8/utf8_esperanto_ci               */
        210,    /* utf8/utf8_hungarian_ci               */
#ifdef HAVE_UTF8_GENERAL_CS
        254,    /* utf8/utf8_general_cs                 */
#endif /* HAVE_UTF8_GENERAL_CS */
        0       /* End of collations                    */
};

static su_pa_t *su_collations = NULL;

/*#***********************************************************************\
 *              su_collation_init
 *
 * Initialize supported collations
 *
 * Parameters : -
 *
 * Return value : -
 *
 * Globals are not used.
 */
void su_collation_init(void)
{
        uint col;
        
        su_collations = su_pa_init();

        for(col = 0; su_collation_allowed[col] != 0; col++) {
            su_pa_insertat(su_collations, su_collation_allowed[col], &(su_collation_allowed[col]));
        }
}

/*#***********************************************************************\
 *              su_collation_done
 *
 * Deinitialize collation support
 *
 * Parameters : -
 *
 * Return value : -
 *
 * Globals are not used.
 */
void su_collation_done(void)
{
        su_pa_done(su_collations);
}

/*#***********************************************************************\
 *              su_collation_get_collations
 *
 * Get supported collations
 *
 * Parameters : -
 *
 * Return value : pointer to su_pa_t
 *
 * Globals are not used.
 */
su_pa_t* su_collation_get_collations(void)
{
        return (su_collations);
}

/*#***********************************************************************\
 *              su_collation_supported
 *
 * Checks if this (charset collation) pair is supported by Solid.
 *
 * Parameters :
 *
 * uint collation_id MySQL collation id
 *
 * Return value : true if the collation is supported. false otherwise.
 *
 * Globals are not used.
 */

bool su_collation_supported(
        uint collation_id)
{

        if (su_pa_indexinuse(su_collations, collation_id)) {
            return (TRUE);
        } else {
            return (FALSE);
        }
}

/*#***********************************************************************\
 *              su_collation_supported_charset
 *
 * Checks if this charset is supported by Solid.
 *
 * Parameters :
 *
 * const char * charset_name - MySQL charset name.
 *
 * Return value : true if the charset is supported. false otherwise.
 *
 * Globals are not used.
 */

bool su_collation_supported_charset(
        const char * charset_name)
{
        size_t i;
        
        /* Supported character sets: */
        static const char* solid_allowed_charsets[] = {
            "binary",   /* binary charset   */
            "latin1",   /* latin1 charset   */
            "ucs2",     /* ucs2 charset     */
            "utf8",     /* utf8 charset     */
            "cp932",    /* cp932 charset    */
            "eucjpms"   /* eucjpms charset  */
        };

        const size_t n = sizeof(solid_allowed_charsets)/sizeof(solid_allowed_charsets[0]);

        for(i = 0; i < n; ++i) {
            if (!strcmp(charset_name,solid_allowed_charsets[i])) {
                return (TRUE);
            }
        }
    
        return (FALSE);
}

#ifdef SS_FAKE

/* This is a test collation that can be used for testing the mechanisms */
/* Name "Elbonia" stolen from Dilbert comics by Scott Adams */

static void su_collation_fake_elbonian_donebuf(
        su_collation_t* coll)
{
        void* data = coll->coll_data;

        if (coll->coll_name != NULL) {
            ss_dassert(data != NULL);
            coll->coll_name = NULL;
            coll->coll_data = NULL;
            SsMemFree(data);
        }
}

static bool su_collation_fake_elbonian_create_weightstr(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_prefix_charlen,
        void* weightstr_buf,
        size_t weightstr_bufsize,
        size_t* p_weightstr_bytelen)
{
        bool succp = TRUE;
        size_t i;
        size_t n = str_bytelen;

        ELBONONIAN_INITIFNEEDED(coll);
        ss_dprintf_1(("su_collation_fake_elbonian_create_weightstr\n"));

        if (weightstr_bufsize < str_bytelen) {
            succp = FALSE;
            n = weightstr_bufsize;
        }

        ss_dassert(weightstr_bufsize >= str_bytelen);

        for (i = 0; i < n; i++) {
            ((ss_byte_t*)weightstr_buf)[i] =
                ((ss_byte_t*)coll->coll_data)[((ss_byte_t*)str)[i]];
        }

        *p_weightstr_bytelen = i;

        return (succp);
}

static int su_collation_fake_elbonian_compare(
        su_collation_t* coll,
        void* str1,
        size_t str1_bytelen,
        void* str2,
        size_t str2_bytelen)
{
        ss_byte_t buf1[256];
        ss_byte_t buf2[256];
        ss_byte_t* ws1 = buf1;
        ss_byte_t* ws2 = buf2;
        size_t ws1_bytelen;
        size_t ws2_bytelen;
        size_t cmplen;
        bool succp;
        int cmp;

        ELBONONIAN_INITIFNEEDED(coll);
        ss_dprintf_1(("su_collation_fake_elbonian_compare\n"));
        if (str1_bytelen > sizeof(buf1)) {
            ws1 = SsMemAlloc(str1_bytelen);
        }
        if (str2_bytelen > sizeof(buf2)) {
            ws2 = SsMemAlloc(str2_bytelen);
        }
        
        succp = su_collation_fake_elbonian_create_weightstr(
                coll,
                str1,
                str1_bytelen,
                str1_bytelen,
                ws1,
                str1_bytelen,
                &ws1_bytelen);

        ss_dassert(succp);
        ss_dassert(ws1_bytelen == str1_bytelen);

        succp = su_collation_fake_elbonian_create_weightstr(
                coll,
                str2,
                str2_bytelen,
                str2_bytelen,
                ws2,
                str2_bytelen,
                &ws2_bytelen);

        ss_dassert(succp);
        ss_dassert(ws2_bytelen == str2_bytelen);
        cmplen = SS_MIN(ws1_bytelen, ws2_bytelen);
        cmp = SsMemcmp(ws1, ws2, cmplen);
        if (cmp == 0) {
            cmp = (int)ws1_bytelen - (int)ws2_bytelen;
        }
        if (ws1 != buf1) {
            SsMemFree(ws1);
        }
        if (ws2 != buf2) {
            SsMemFree(ws2);
        }
        return (cmp);
                                                             
}

static size_t su_collation_fake_elbonian_get_maxlen_for_weightstr(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_prefix_charlen)
{
        ELBONONIAN_INITIFNEEDED(coll);
        return (str_bytelen);
}


static size_t su_collation_fake_elbonian_get_bytes_in_chars(
        su_collation_t* coll,
        void* str,
        size_t str_bytelen,
        size_t str_charlen)
{
        ELBONONIAN_INITIFNEEDED(coll);
        return (str_charlen);
}

bool su_collation_fake_elbonian_initbuf(
        su_collation_t* coll,
        char* collation_name, /* UTF8 encoding! */
        void* data)
{
        size_t i;

        if (coll->coll_name != NULL) {
            return (TRUE);
        }

        coll->coll_name = "latin1_elbonian_ci";
        coll->coll_data = SsMemAlloc(256);
        for (i = 0; i < 256; i++) {
            ((ss_byte_t*)coll->coll_data)[i] = (ss_byte_t)i;
        }

        /* ASCII range letters case insensitive */
        for (i = 'a'; i <= 'z'; i++) {
             ((ss_byte_t*)coll->coll_data)[i] =
                 (ss_byte_t)('A' + (i - 'a'));
        }

        /* make number collation inverted */
        for (i = 0; i <= 9; i++) {
            ((ss_byte_t*)coll->coll_data)[i + '0'] =
                (ss_byte_t)('0' + (9 - i));
        }

        coll->coll_initbuf                  = su_collation_fake_elbonian_initbuf;
        coll->coll_donebuf                  = su_collation_fake_elbonian_donebuf;
        coll->coll_compare                  = su_collation_fake_elbonian_compare;
        coll->coll_get_maxlen_for_weightstr = su_collation_fake_elbonian_get_maxlen_for_weightstr;
        coll->coll_create_weightstr         = su_collation_fake_elbonian_create_weightstr;
        coll->coll_get_bytes_in_chars       = su_collation_fake_elbonian_get_bytes_in_chars;

        return (TRUE);
}

su_collation_t su_collation_fake_elbonian;

void su_collation_fake_elbonian_link_in(void)
{
}

#endif /* SS_FAKE */
