/*************************************************************************\
**  source       * sssprint.h
**  directory    * ss
**  description  * sprintf replacement
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


#ifndef SSSPRINT_H
#define SSSPRINT_H

#include "ssenv.h"
#include "ssstdarg.h"
#include "ssc.h"

#ifdef NO_ANSI
int SsSprintf();
#else
int SS_CDECL SsSprintf(char* str, const char* format, ...);
#endif


#ifdef NO_ANSI
int SsVsprintf();
#else
int SsVsprintf(char* str, char* format, va_list ap);
#endif


#endif /* SSSPRINT_H */
