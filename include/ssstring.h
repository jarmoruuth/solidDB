/*************************************************************************\
**  source       * ssstring.h
**  directory    * ss
**  description  * declarations which should be in <string.h>
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


#ifndef SSSTRING_H
#define SSSTRING_H

#include "ssenv.h"
#include "ssc.h" 
#include "ssstddef.h"
#include <string.h>

#if defined(SS_UNICODE)

#include "sswcs.h"  /* 2-byte character string routines */

#define SsTcscat   SsWcscat
#define SsTcschr   SsWcschr
#define SsTcscmp   SsWcscmp
#define SsTcscpy   SsWcscpy
#define SsTcscspn  SsWcscspn
#define SsTcslen   SsWcslen
#define SsTcsncat  SsWcsncat
#define SsTcsncmp  SsWcsncmp
#define SsTcsncpy  SsWcsncpy
#define SsTcspbrk  SsWcspbrk
#define SsTcsrchr  SsWcsrchr
#define SsTcsspn   SsWcsspn
#define SsTcsstr   SsWcswcs

#define SsTcsicmp  SsWcsicmp
#define SsTcsupr   SsWcsupr
#define SsTcslwr   SsWcslwr

#else /* SS_UNICODE */

#include "sschcvt.h"

#define SsTcscat   strcat
#define SsTcschr   strchr
#define SsTcscmp   strcmp
#define SsTcscpy   strcpy
#define SsTcscspn  strcspn
#define SsTcslen   strlen
#define SsTcsncat  strncat
#define SsTcsncmp  strncmp
#define SsTcsncpy  strncpy
#define SsTcspbrk  strpbrk
#define SsTcsrchr  strrchr
#define SsTcsspn   strspn
#define SsTcsstr   strstr

#define SsTcsicmp  SsStricmp
#define SsTcsupr   SsStrupr
#define SsTcslwr   SsStrlwr

#endif /* SS_UNICODE */

/* We must have unsigned memcmp, so we define our own interface */
#if !defined(SS_MEMCMP_SIGNED) && !defined(SS_PURIFY)
#define SsMemcmp memcmp
#else
int SsMemcmp(const void* p1, const void* p2, size_t n);
#endif /* SS_MEMCMP_SIGNED */

char* SsStrTrim(char* str);
char* SsStrTrimLeft(char* str);
char* SsStrTrimRight(char* str);
char* SsStrReplaceDup(
        const char* src, 
        const char* pattern, 
        const char* replacement);

bool SsStrScanString(
        char* scanstr,
        char* separators,
        uint* scanindex,
        char  comment_char,
        char** value_give);

bool SsStrScanStringWQuoting(
        char* scanstr,
        char* separators,
        uint* scanindex,
        char  comment_char,
        char** value_give);

char* SsHexStr(
        char* buf,
        size_t buflen);

char* SsStrUnquote(char* str);

char* SsStrOvercat(char* dst, char* src, int len);
char* SsStrSeparatorOvercat(char* dst, char* src, char* sep, int len);

#endif /* SSSTRING_H */
