/*************************************************************************\
**  source       * sschcvt.h
**  directory    * ss
**  description  * Character conversions between different character sets.
**               * 
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


#ifndef SSCHCVT_H
#define SSCHCVT_H

#include "ssstddef.h"
#include "ssenv.h"
#include "ssc.h"

#define CHTYPE_U ((unsigned)0x01) /* uppercase letter */
#define CHTYPE_L ((unsigned)0x02) /* lowercase letter */
#define CHTYPE_D ((unsigned)0x04) /* digit [0-9] */
#define CHTYPE_S ((unsigned)0x08) /* space, tab, cr, lf, vt or form feed */
#define CHTYPE_P ((unsigned)0x10) /* punctuation character */
#define CHTYPE_C ((unsigned)0x20) /* control character */
#define CHTYPE_B ((unsigned)0x40) /* space bar */
#define CHTYPE_X ((unsigned)0x80) /* hexadecimal digit [0-9] [a-f] |A-F] */

extern ss_byte_t ss_chtype[], ss_chtolower[], ss_chtoupper[];
extern ss_byte_t ss_chtable_dos2iso[];
extern ss_byte_t ss_chtable_iso2dos[];
extern ss_byte_t ss_chtable_mac2iso[];
extern ss_byte_t ss_chtable_iso2mac[];

#define ss_isalpha(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & (CHTYPE_U|CHTYPE_L))
#define ss_isupper(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & CHTYPE_U)
#define ss_islower(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & CHTYPE_L)
#define ss_isdigit(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & CHTYPE_D)
#define ss_isxdigit(c)  ((ss_chtype+1)[(ss_byte_t)(c)] & (CHTYPE_X|CHTYPE_D))
#define ss_isspace(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & CHTYPE_S)
#define ss_ispunct(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & CHTYPE_P)
#define ss_isalnum(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & (CHTYPE_U|CHTYPE_L|CHTYPE_D))
#define ss_isprint(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & (CHTYPE_U|CHTYPE_L|CHTYPE_B|CHTYPE_P|CHTYPE_D))
#define ss_isgraph(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & (CHTYPE_U|CHTYPE_L|CHTYPE_P|CHTYPE_D))
#define ss_iscntrl(c)   ((ss_chtype+1)[(ss_byte_t)(c)] & CHTYPE_C)
#define ss_isascii(c)   ((ss_byte_t)(c) < (unsigned)0x0080)

#define ss_tolower(c)   ss_chtolower[(ss_byte_t)(c)]
#define ss_toupper(c)   ss_chtoupper[(ss_byte_t)(c)]
#define ss_toascii(c)   ((c) & (unsigned)0x7f)

#ifndef NO_ANSI

/* The string functions handle strings that are in ISO 8859/1 format */
ss_char1_t* SsStrlwr(ss_char1_t* string);
ss_char1_t* SsStrupr(ss_char1_t* string);
int SsStrcmp(const ss_char1_t* s1, const ss_char1_t* s2);
int SsStrncmp(const ss_char1_t* s1, const ss_char1_t* s2, size_t len);
int SsStricmp(const ss_char1_t* s1, const ss_char1_t* s2);
int SsStrnicmp(const ss_char1_t* s1, const ss_char1_t* s2, size_t len);
ss_char1_t* SsStristr(const ss_char1_t* src, const ss_char1_t* pat);

/* Iso translation to Finnish alphabet ordering */
int SsChCvtIso2Fin(int ch);
#define SsChCvtFin2Iso(ch) SsChCvtIso2Fin(ch)  

int SsChCvtDos2Iso(int ch);
int SsChCvtIso2Dos(int ch);
int SsChCvtMac2Iso(int ch);
int SsChCvtIso2Mac(int ch);
int SsChCvt7bitscand2Iso(int ch);
int SsChCvtIso27bitscand(int ch);

/* System default translation to and from ISO charset */
int SsChCvtDef2Iso(int ch);
int SsChCvtIso2Def(int ch);

void SsChCvtBuf(
        void* p_dest,
        void* p_src,
        size_t nbytes,
        ss_byte_t* chcvt_table);

#else /* not NO_ANSI */

ss_char1_t* SsStrlwr();
ss_char1_t* SsStrupr();
int SsStrcmp();
int SsStrncmp();
int SsStricmp();
int SsStrnicmp();

int SsChCvtIso2Fin();
int SsChCvtDos2Iso();
int SsChCvtIso2Dos();
int SsChCvtMac2Iso();
int SsChCvtIso2Mac();
int SsChCvt7bitscand2Iso();
int SsChCvtIso27bitscand();
void SsChCvtBuf();

#endif /* NO_ANSI */

#endif /* SSCHCVT_H */
