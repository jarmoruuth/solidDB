/*************************************************************************\
**  source       * ssdtoa.h
**  directory    * ss
**  description  * Double to ascii conversion 
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



#ifndef SSDTOA_H
#define SSDTOA_H

#include "ssenv.h"
#include "ssstddef.h" /* size_t */

typedef enum {
        SS_DBLTRUNC_OK,
        SS_DBLTRUNC_TRUNCATED,
        SS_DBLTRUNC_VALUELOST
} SsDoubleTruncateRetT;

char* SsDoubleToAscii(
        double value,
        char* str,
        int count);

char* SsDoubleToAsciiE(
        double value,
        char* str,
        int count);

SsDoubleTruncateRetT SsTruncateAsciiDoubleValue(
        char* buffer,
        size_t maxsize);

char* SsDoubleToAsciiDecimals(
        double d,
        char* str,
        int maxlen,
        int decimals);

#endif /* SSDTOA_H */
