/*************************************************************************\
**  source       * ssdtow.h
**  directory    * ss
**  description  * Double to wide-char string conversion
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


#ifndef SSDTOW_H
#define SSDTOW_H

#include "ssc.h"
#include "ssdtoa.h"

ss_char2_t* SsDoubleToWcs(
        double value,
        ss_char2_t* wcs,
        int count);

ss_char2_t* SsDoubleToWcsE(
        double value,
        ss_char2_t* wcs,
        int count);

SsDoubleTruncateRetT SsTruncateWcsDoubleValue(
        ss_char2_t* buffer,
        size_t maxsize);

ss_char2_t* SsDoubleToWcsDecimals(
        double d,
        ss_char2_t* wcs,
        int maxlen,
        int decimals);





#endif /* SSDTOW_H */
