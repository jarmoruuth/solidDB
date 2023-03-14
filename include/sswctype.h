/*************************************************************************\
**  source       * sswctype.h
**  directory    * ss
**  description  * Wide character ctype
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


#ifndef SSWCTYPE_H
#define SSWCTYPE_H

#include "ssc.h"
#include "sschcvt.h"

#define ss_isw8bit(c)   (((c) & (~0xFFU)) == 0)
#define ss_iswalpha(c)  (ss_isw8bit(c) && ss_isalpha(c))
#define ss_iswupper(c)  (ss_isw8bit(c) && ss_isupper(c)) 
#define ss_iswlower(c)  (ss_isw8bit(c) && ss_islower(c)) 
#define ss_iswdigit(c)  (ss_isw8bit(c) && ss_isdigit(c)) 
#define ss_iswxdigit(c) (ss_isw8bit(c) && ss_isxdigit(c))
#define ss_iswspace(c)  (ss_isw8bit(c) && ss_isspace(c)) 
#define ss_iswpunct(c)  (ss_isw8bit(c) && ss_ispunct(c)) 
#define ss_iswalnum(c)  (ss_isw8bit(c) && ss_isalnum(c)) 
#define ss_iswprint(c)  (ss_isw8bit(c) && ss_isprint(c)) 
#define ss_iswgraph(c)  (ss_isw8bit(c) && ss_isgraph(c)) 
#define ss_iswcntrl(c)  (ss_isw8bit(c) && ss_iscntrl(c)) 
#define ss_iswascii(c)  (ss_isw8bit(c) && ss_isascii(c)) 

#define ss_towlower(c)  (ss_isw8bit(c)? (ss_char2_t)ss_tolower(c) : (c))
#define ss_towupper(c)  (ss_isw8bit(c)? (ss_char2_t)ss_toupper(c) : (c))
#define ss_towascii(c)  ss_toascii(c)

#endif /* SSWCTYPE_H */
