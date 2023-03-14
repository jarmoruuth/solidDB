/*************************************************************************\
**  source       * ssstdio.h
**  directory    * ss
**  description  * stdio.h
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


#ifndef SSSTDIO_H
#define SSSTDIO_H

#include "ssenv.h"

#include <stdio.h>

#if defined(SS_WIN) || defined(SS_NTGUI) 

#include "ssstdarg.h"
#include "ssconio.h"

#include "ssc.h"

#define printf  ss_printf
#define vprintf ss_vprintf

int SS_CDECL ss_printf(const char *, ...);
int SS_CDECL ss_vprintf(const char *, va_list);



#if defined(SS_WIN) || defined(SS_NTGUI)

#ifdef SS_NTGUI
int SsStdioMessageLoop(void);
#endif




#ifdef putchar
#  undef putchar
#endif
#define putchar  ss_putchar 
#define putch    ss_putch
#define getch    ss_getch
#define kbhit    ss_kbhit
#define gets     ss_gets

int ss_putchar(int);
int ss_putch(int ch);
int ss_getch(void);
int ss_wingetch(void);
int ss_kbhit(void);
char* ss_gets(char*);

#else /* SS_WIN || SS_NTGUI */

#include <stdio.h>

#endif /* defined(SS_WIN) || defined(SS_NTGUI) */

#endif /* SS_WIN */

#endif /* SSSTDIO_H */
