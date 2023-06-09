/*************************************************************************\
**  source       * sswscan.c
**  directory    * ss
**  description  * Portable sscan utilities for wide char strings
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

WINDOWS SDK DLL C-libraries do not offer sscanf(). That is why we have to
implement services that can do the same thing.

There are usually available functions (or macros) like
atoi(), atol(), atof() etc, but we can not use them because they return
zero (0) also when the operation is not succesfull. We usually want to
test if stream contains something valid, and we surely can not do it with
atoi() etc.

So, we do the whole thing by scanning the input stream and checking
it character by character.

Then, the actual return value is generated by atof() and atol().


Limitations:
-----------


Error handling:
--------------

None

Objects used:
------------

None

Preconditions:
-------------


Multithread considerations:
--------------------------

This code is re-entrant

Example:
-------

See tscan.c

**************************************************************************
#endif /* DOCUMENTATION */


#include "ssc.h"
#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssdebug.h"
#include "sswctype.h"
#include "ssfloat.h"
#include "sstraph.h"
#include "ssmem.h"
#include "sswscan.h"

/*##**********************************************************************\
 * 
 *		SsWcsScanDouble
 * 
 * Converts an ascii string to double. 
 * 
 * A valid double is of form 
 * 
 *  [whitespace][{+|-}][digits][.digits][{d|D|e|E]}[sign]digits]
 * 
 * Parameters : 
 * 
 *	src - in, use
 *		source string
 * 
 *	p_d - out
 *		pointer to double
 *
 *      p_mismatch - out
 *		pointer to the first position in src that
 *          does not belong to the double returned
 *
 * Return value : 
 * 
 *      TRUE, if succesfull. p_d and p_mismatch are updated.
 *      FALSE, if failed.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsWcsScanDouble(
	ss_char2_t* src,
	double* p_d,
        ss_char2_t** p_mismatch)
{
        bool valid = FALSE;
        ss_char2_t* p_ch;
        ss_char2_t* org_src = src;

        p_ch = src++;


        /* Allow white spaces in the beginning */
        while (ss_iswspace(*p_ch)) {
            p_ch = src++;
        }

        /* Next, allow sign */
        if ((*p_ch == '-') || (*p_ch == '+')) {
            p_ch = src++;
        } 

        /* Then there may be digits */
        while (ss_iswdigit(*p_ch)) {
            valid = TRUE;
            p_ch = src++;
        }

        /* Allow decimal point */
        if (*p_ch == '.') {
            p_ch = src++;
        }

        /* Then there may be digits */
        while (ss_iswdigit(*p_ch)) {
            valid = TRUE;
            p_ch = src++;
        }

        /* There has to be some digits for now */
        if (!valid) {
            return(FALSE);
        }

        /* we handle here the possible {e|E|d|D}{exponent} */

        if ((*p_ch == 'e') || (*p_ch == 'E') ||
            (*p_ch == 'd') || (*p_ch == 'D') ) {

            int exp;
            bool bo;
            ss_char2_t* ptr;

            p_ch = src++;

            bo = SsWcsScanInt(p_ch, &exp, &ptr);

            if (bo) {
                p_ch = ptr;
            } else {
                p_ch--;
            }
        }

        if (valid) {
            ss_char1_t tmp_buf[32];
            ss_char1_t* tmp_src = tmp_buf;
            ss_char1_t* p1;
            ss_char2_t* p2;

            if ((size_t)(p_ch - org_src) >= sizeof(tmp_buf)) {
                tmp_src = SsMemAlloc(sizeof(ss_char1_t) * (p_ch - org_src + 1));
            }

            /* Copy wide char string to char string temporarily */
            for (p1 = tmp_src, p2 = org_src; p2 < p_ch; p2++, p1++) {
                ss_dassert(ss_isw8bit(*p2));
                *p1 = (ss_char1_t)*p2;
            }
            *p1 = '\0';

            {
            ss_trapcode_t trapcode = SS_TRAP_NONE;

            SS_TRAP_HANDLERSECTION
                case SS_TRAP_FPE:
                case SS_TRAP_FPE_INVALID:
                case SS_TRAP_FPE_INEXACT:
                case SS_TRAP_FPE_STACKFAULT:
                case SS_TRAP_FPE_STACKOVERFLOW:
                case SS_TRAP_FPE_STACKUNDERFLOW:
                case SS_TRAP_FPE_BOUND:
                case SS_TRAP_FPE_DENORMAL:
                case SS_TRAP_FPE_INTOVFLOW:
                case SS_TRAP_FPE_OVERFLOW:
                case SS_TRAP_FPE_INTDIV0:
                case SS_TRAP_FPE_ZERODIVIDE:
                case SS_TRAP_FPE_UNDERFLOW:
                case SS_TRAP_FPE_EXPLICITGEN:
                    trapcode = SS_TRAP_GETCODE();
                    valid = FALSE;
                    ss_dprintf_1(("SsWcsScanDouble FP trap %d\n", trapcode));
                    break;
                default:
                    if (tmp_src != tmp_buf) {
                        SsMemFree(tmp_src);
                    }
                    SS_TRAP_RERAISE();
                    ss_error;
            SS_TRAP_RUNSECTION
                *p_mismatch = p_ch;
                *p_d = atof(tmp_src);
                valid = SS_DOUBLE_IS_PORTABLE(*p_d);
            SS_TRAP_END
            }
            if (tmp_src != tmp_buf) {
                SsMemFree(tmp_src);
            }
        }
        return(valid);
}

/*##**********************************************************************\
 * 
 *		SsWcsScanLong
 * 
 * Converts an ascii string to long integer. 
 * Leading whitespaces are skipped.
 * 
 *  [whitespace][{+|-}][digits]
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		source string
 *
 *	p_l - out
 *		pointer to long integer
 *
 *      p_mismatch - out
 *		pointer to the first position in src that
 *          does not belong to the long integer returned
 *
 * Return value : 
 * 
 *      TRUE, if succesfull
 *      FALSE, if failed.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsWcsScanLong(ss_char2_t* s, long* p_l, ss_char2_t** p_mismatch)
{
        bool valid;
        bool negative;
        ulong v;
        ulong t;

        ss_dassert(s != NULL);
        ss_dassert(p_l != NULL);
        ss_dassert(p_mismatch != NULL);
        negative = FALSE;
        valid = FALSE;

        while (ss_iswspace(*s)) {
            s++;
        }
        switch (*s) {
            case '-':
                negative = TRUE;
                /* FALLTHROUGH */
            case '+':
                s++;
                break;
            default:
                break;
        }
        for (v = 0, valid = FALSE;
             ;
             v = v * (ulong)10 + t, s++, valid = TRUE)
        {
            switch (*s) {
                case '9':
                    t = 9;
                    continue;
                case '8':
                    t = 8;
                    continue;
                case '7':
                    t = 7;
                    continue;
                case '6':
                    t = 6;
                    continue;
                case '5':
                    t = 5;
                    continue;
                case '4':
                    t = 4;
                    continue;
                case '3':
                    t = 3;
                    continue;
                case '2':
                    t = 2;
                    continue;
                case '1':
                    t = 1;
                    continue;
                case '0':
                    t = 0;
                    continue;
                default:
                    break;
            }
            break;
        }
        if (negative) {
            *p_l = -(long)v;
        } else {
            *p_l = (long)v;
        }
        *p_mismatch = s;
        return (valid);
}


/*##**********************************************************************\
 * 
 *		SsWcsScanInt
 * 
 * Converts an ascii string to integer. 
 *  [whitespace][{+|-}][digits]
 * 
 * Parameters : 
 * 
 *	src - in, use
 *		source string
 *
 *	p_l - out
 *		pointer to integer
 *
 *      p_mismatch - out
 *		pointer to the first position in src that
 *          does not belong to the integer returned
 *
 * Return value : 
 * 
 *      TRUE, if succesfull. p_i and p_mismatch are updated.
 *      FALSE, if failed.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsWcsScanInt(
	ss_char2_t* src,
	int* p_i,
        ss_char2_t** p_mismatch)
{
        long l;

        if (SsWcsScanLong(src, &l, p_mismatch)) {
            *p_i = (int)l;
            return(TRUE);
        } else {
            return(FALSE);
        }
}
