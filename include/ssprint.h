/*************************************************************************\
**  source       * ssprint.h
**  directory    * ss
**  description  * Wide char printf-like utilities
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


#ifndef SSPRINT_H
#define SSPRINT_H

#include "ssstdio.h"
#include "ssstdarg.h"
#include "ssstddef.h"
#include "ssc.h"

#ifndef SS_PRINT_RET_DEFINED
#define SS_PRINT_RET_DEFINED

typedef enum {
        SS_PRINT_OK,
        SS_PRINT_WRITE_FAILED,
        SS_PRINT_FORMAT_ERROR
} SsPrintRetT;

#endif /* !SS_PRINT_RET_DEFINED */

#ifdef SsFprintf
#undef SsFprintf
#endif /* SsFprintf */

SsPrintRetT SsVGenericPrintf(
        bool (*p_put)(void* ctx, ss_char1_t* p, size_t n),
        void* putctx,
        size_t* p_nwritten,
        ss_char1_t* format,
        va_list ap);

int SS_CDECL SsBufPrintf(
        ss_char1_t* buf,
        size_t n,
        ss_char1_t* format,
        ...);

int SsVBufPrintf(
        ss_char1_t* buf,
        size_t n,
        ss_char1_t* format,
        va_list ap);

int SS_CDECL SsFilePrintf(
        SS_FILE* fp,
        ss_char1_t* format,
        ...);

int SsVFilePrintf(
        SS_FILE* fp,
        ss_char1_t* format,
        va_list ap);


#endif /* SSPRINT_H */
