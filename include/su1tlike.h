/*************************************************************************\
**  source       * su1tlike.h
**  directory    * su
**  description  * Template that instantiates
**               * the like matcher functions for
**               * either ss_char1_t or ss_char2_t strings
**               * This header contains code and must not be
**               * included from other files than
**               * su0slike.c or su0wlike.c
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

#include <sslimits.h>
#include <ssmem.h>
#include <ssdebug.h>

#undef SLIKE_PCHAR_T
#undef SLIKE_SCHAR_T
#undef SLIKE_PUCHAR_T
#undef SLIKE_SUCHAR_T
#undef SU_SLIKE
#undef SU_SLIKE_LEGALPATTERN
#undef SU_SLIKE_FIXEDPREFIX
#undef VA_FORMAT_DEF
#undef VA_FORMAT_DEF_OUTPUT
#undef VA_FORMAT_ARG
#undef LOAD_PCHAR
#undef LOAD_SCHAR
#undef STORE_CHAR
#undef DBG_STR

#if defined(SU_SLIKE_INSTANTIATE_UNICODE)
/* Instantiate routines for unicode */

#define SLIKE_PCHAR_T              ss_char2_t
#define SLIKE_SCHAR_T              ss_char2_t
#define SLIKE_PUCHAR_T             SLIKE_PCHAR_T
#define SLIKE_SUCHAR_T             SLIKE_SCHAR_T
#define SU_SLIKE                su_wlike
#define SU_SLIKE_LEGALPATTERN   su_wlike_legalpattern
#define SU_SLIKE_FIXEDPREFIX    su_wlike_fixedprefix
#define SU_SLIKE_PREFIXINFO     su_wlike_prefixinfo
#define VA_FORMAT_DEF           ,bool va_format
#define VA_FORMAT_DEF_OUTPUT    ,bool va_format_output
#define VA_FORMAT_ARG           ,va_format
#ifdef SS_LSB1ST
        /* Intel byte order */
#   define LOAD_PCHAR(p)            (va_format ? SS_CHAR2_LOAD(p) : *(p))
#   define LOAD_SCHAR(p)            LOAD_PCHAR(p)
#   define STORE_CHAR(p,c) \
        if (va_format_output) {\
            SS_CHAR2_STORE(p,c);\
        } else {\
            *(p) = (c);\
        }
#else /* SS_LSB1ST */
        /* Motorola byte order */
#   define LOAD_PCHAR(p)        SS_CHAR2_LOAD(p)
#   define LOAD_SCHAR(p)        LOAD_PCHAR(p)
#   define STORE_CHAR(p,c)      SS_CHAR2_STORE(p,c)
#endif /* SS_LSB1ST */
#define DBG_STR                 "w"

#elif defined(SU_SLIKE_INSTANTIATE_ASCII)
/* Instantiate routines for ASCII */

#define SLIKE_PCHAR_T           ss_char1_t
#define SLIKE_SCHAR_T           ss_char1_t
#define SLIKE_PUCHAR_T          ss_byte_t
#define SLIKE_SUCHAR_T          ss_byte_t
#define SU_SLIKE                su_slike
#define SU_SLIKE_LEGALPATTERN   su_slike_legalpattern
#define SU_SLIKE_FIXEDPREFIX    su_slike_fixedprefix
#define SU_SLIKE_PREFIXINFO     su_slike_prefixinfo
#define VA_FORMAT_DEF           /* empty */
#define VA_FORMAT_DEF_OUTPUT    /* empty */
#define VA_FORMAT_ARG
#define LOAD_PCHAR(p)           (*(p))
#define LOAD_SCHAR(p)           LOAD_PCHAR(p)
#define STORE_CHAR(p,c)         { *(p) = (c); }
#define DBG_STR                 "s"

#elif defined(SU_SLIKE_INSTANTIATE_UNIPAT4CHAR)
/* instantiate like for unicode pattern and char source */

#define SLIKE_PCHAR_T           ss_char2_t
#define SLIKE_SCHAR_T           ss_char1_t
#define SLIKE_PUCHAR_T          SLIKE_PCHAR_T
#define SLIKE_SUCHAR_T          ss_byte_t
#define SU_SLIKE                su_wslike
#define VA_FORMAT_DEF           ,bool va_format
#define VA_FORMAT_ARG           ,va_format
#   define LOAD_SCHAR(p)        (*(p))
#ifdef SS_LSB1ST
        /* Intel byte order */
#   define LOAD_PCHAR(p)        (va_format ? SS_CHAR2_LOAD(p) : *(p))
#else /* SS_LSB1ST */
        /* Motorola byte order */
#   define LOAD_PCHAR(p)        SS_CHAR2_LOAD(p)
#endif /* SS_LSB1ST */
#define DBG_STR                 "ws"

#elif defined(SU_SLIKE_INSTANTIATE_CHARPAT4UNI)
/* instantiate like for unicode pattern and char source */

#define SLIKE_PCHAR_T           ss_char1_t
#define SLIKE_SCHAR_T           ss_char2_t
#define SLIKE_PUCHAR_T          ss_byte_t
#define SLIKE_SUCHAR_T          SLIKE_SCHAR_T
#define SU_SLIKE                su_swlike
#define VA_FORMAT_DEF           ,bool va_format
#define VA_FORMAT_ARG           ,va_format
#   define LOAD_PCHAR(p)        (*(p))
#ifdef SS_LSB1ST
        /* Intel byte order */
#   define LOAD_SCHAR(p)        (va_format ? SS_CHAR2_LOAD(p) : *(p))
#else /* SS_LSB1ST */
        /* Motorola byte order */
#   define LOAD_SCHAR(p)        SS_CHAR2_LOAD(p)
#endif /* SS_LSB1ST */
#define DBG_STR                 "sw"

#else /* SU_SLIKE_INSTANTIATE_XXX */

/* Header included from erroneous place! */
#error "Misuse of this header"

#endif /* SU_SLIKE_INSTANTIATE_XXX */

/*##**********************************************************************\
 * 
 *		SU_SLIKE
 * 
 * Implements the "LIKE" operator of SQL
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		the string
 *
 *	slen - in, use
 *		the length of the string
 *
 *	patt - in, use
 *		the pattern string (containing "_"s for
 *          any single letter and "%"s for any sequence
 *          of letters (or no letter))
 *
 *	pattlen - in, use
 *		the length of the patern string
 *
 *	esc - in, use
 *		the escape character (SU_SLIKE_NOESCCHAR means "no escape
 *          character")
 *
 * Return value : 
 * 
 *      TRUE, if the LIKE condition is true
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SU_SLIKE(
        SLIKE_SCHAR_T* s,
        size_t slen,
        SLIKE_PCHAR_T*   patt,
        size_t pattlen,
        int esc
        VA_FORMAT_DEF)
{
        ss_dprintf_3(("%s: su_%slike\n", DBG_STR, __FILE__));

        /* skip extra '%'s */
        while (pattlen > 1) {
            SLIKE_PCHAR_T* p;
            if (LOAD_PCHAR(patt) != '%') {
                break;
            }
            p = patt + 1;
            if (LOAD_PCHAR(p) != '%') {
                break;
            }
            pattlen--;
            patt = p;
        }

        while (slen > 0 && pattlen > 0) {
            SLIKE_PUCHAR_T pchar;

            pchar = (SLIKE_PUCHAR_T)LOAD_PCHAR(patt);
            if ((esc != SU_SLIKE_NOESCCHAR) && esc == pchar) {
                patt++;
                if ((SLIKE_SUCHAR_T)LOAD_SCHAR(s) 
                ==  (SLIKE_PUCHAR_T)LOAD_PCHAR(patt)) 
                {
                    s++;
                    slen--;
                    patt++;
                    ss_rc_dassert(pattlen >= 2, pattlen);
                    pattlen -= 2;
                } else {
                    return(FALSE);
                }
            } else if (pchar == '%') {
                size_t i;

                patt++;
                pattlen--;
                if (pattlen == 0) {
                    return(TRUE);
                }
                pchar = (SLIKE_PUCHAR_T)LOAD_PCHAR(patt);
                if (!(esc != SU_SLIKE_NOESCCHAR && esc == pchar)
                &&  pchar != '%'
                &&  pchar != '_')
                {
                    /* No special chars after %, search the next char from s.
                     */
                    while (slen > 0 && (SLIKE_SUCHAR_T)LOAD_SCHAR(s) != pchar) {
                        s++;
                        slen--;
                    }
                }

                for (i = 0; i <= slen; i++) {
                    if (SU_SLIKE(s + i, slen - i, patt, pattlen, esc VA_FORMAT_ARG)) {
                        return(TRUE);
                    }
                }
                return(FALSE);
            } else if (pchar == '_' || pchar == (SLIKE_SUCHAR_T)LOAD_SCHAR(s)) {
                s++;
                slen--;
                patt++;
                pattlen--;
            } else {
                return(FALSE);
            }
        }

        /* see if there are only "%"s left in patt */
        while (pattlen > 0) {
            if (LOAD_PCHAR(patt) != '%') {
                return(FALSE);
            }
            patt++;
            pattlen--;
        }
        if (slen > 0) {
            return (FALSE);
        }
        return(TRUE);
}

#if !defined(SU_SLIKE_INSTANTIATE_UNIPAT4CHAR) && !defined(SU_SLIKE_INSTANTIATE_CHARPAT4UNI)

/*##**********************************************************************\
 * 
 *		SU_SLIKE_LEGALPATTERN
 * 
 * Checks whether a like pattern is legal according to SQL Grammar rules
 * 
 * Parameters : 
 * 
 *	patt - in, use
 *		pattern string
 *		
 *	pattlen - in
 *		length of patt
 *		
 *	esc - in
 *		escape character or SU_SLIKE_NOESCCHAR
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool SU_SLIKE_LEGALPATTERN(
        SLIKE_PCHAR_T*   patt,
        size_t  pattlen,
        int     esc
        VA_FORMAT_DEF)
{
        ss_dprintf_3(("%s: su_%slike_legalpattern\n", DBG_STR, __FILE__));
        if (esc == SU_SLIKE_NOESCCHAR) {
            return (TRUE);
        }
        for (; pattlen != 0; patt++, pattlen--) {
            if ((SLIKE_PUCHAR_T)LOAD_PCHAR(patt) == esc) {
                SLIKE_PCHAR_T* p;
                SLIKE_PUCHAR_T pchar;
                if (pattlen == 1) {
                    return (FALSE);
                }
                p = patt + 1;
                pchar = (SLIKE_PUCHAR_T)LOAD_PCHAR(p);
                switch (pchar) {
                    case '%':
                    case '_':
                        break;
                    default:
                        if (pchar != esc) {
                            return (FALSE);
                        }
                        break;
                }
                pattlen--;
                patt++;
            }
        }
        return (TRUE);
}

SLIKE_PCHAR_T* SU_SLIKE_FIXEDPREFIX(
        SLIKE_PCHAR_T*   patt,
        size_t  pattlen,
        int     esc,
        size_t* p_prefixlen,
        SLIKE_PCHAR_T buf_or_null[/* pattlen + 1 */]
        VA_FORMAT_DEF
        VA_FORMAT_DEF_OUTPUT)
{
        SLIKE_PCHAR_T* strptr = patt;
        SLIKE_PCHAR_T* resptr;
        SLIKE_PCHAR_T* res = buf_or_null;
        size_t reslen = 0;
        SLIKE_PUCHAR_T c;
        size_t i;
        int add_nullterm = (p_prefixlen == NULL) ? 1 : 0;

        ss_dprintf_3(("%s: su_%slike_fixedprefix\n", DBG_STR, __FILE__));
        
        resptr = res;
        for (i = 0; i < pattlen; ) {
            c = (SLIKE_PUCHAR_T)LOAD_PCHAR(strptr);
            if (esc != SU_SLIKE_NOESCCHAR && c == esc) {
                strptr++;
                i++;
                ss_dassert(i < pattlen);
                c = (SLIKE_PUCHAR_T)LOAD_PCHAR(strptr);
            } else if (c == '%' || c == '_') {
                break;
            }
            if (res == NULL) {
                resptr = res = SsMemAlloc((pattlen + add_nullterm)* sizeof(SLIKE_PCHAR_T));
            }
            i++;
            strptr++;
            reslen++;
            STORE_CHAR(resptr, c);
            resptr++;
        }
        if (res != NULL && add_nullterm) {
            STORE_CHAR(resptr, 0);
        } else {
            *p_prefixlen = reslen;
        }
        if (reslen != 0) {
            return (res);
        }
        return (NULL);
}

size_t SU_SLIKE_PREFIXINFO(
        SLIKE_PCHAR_T*   patt,
        size_t  pattlen,
        int     esc,
        size_t* p_numfixedchars,
        size_t* p_numwildcards
        VA_FORMAT_DEF)
{
        SLIKE_PCHAR_T* strptr = patt;
        SLIKE_PUCHAR_T c;
        size_t i;
        size_t is_prefix = 1;
        size_t numfixedchars = 0;
        size_t numwildcards = 0;
        size_t prefixlen = 0;

        ss_dprintf_3(("%s: su_%slike_fixedprefixlen\n", DBG_STR, __FILE__));
        
        for (i = 0; i < pattlen; ) {
            c = (SLIKE_PUCHAR_T)LOAD_PCHAR(strptr);
            if (esc != SU_SLIKE_NOESCCHAR && c == esc) {
                strptr++;
                i++;
                ss_dassert(i < pattlen);
                c = (SLIKE_PUCHAR_T)LOAD_PCHAR(strptr);
                numfixedchars++;
            } else if (c == '%' || c == '_') {
                is_prefix = 0;
                numwildcards++;
            } else {
                numfixedchars++;
            }
            i++;
            strptr++;
            prefixlen += is_prefix;
        }
        if (p_numfixedchars != NULL) {
            *p_numfixedchars = numfixedchars;
        }
        if (p_numwildcards != NULL) {
            *p_numwildcards = numwildcards;
        }
        return ((int)prefixlen);
}

#endif /* !SU_SLIKE_INSTANTIATE_UNIPAT4CHAR && !SU_SLIKE_INSTANTIATE_CHARPAT4UNI*/

