/*************************************************************************\
**  source       * ssconio.h
**  directory    * ss
**  description  * Replacement for conio.h.
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


#ifndef SSCONIO_H
#define SSCONIO_H

#include "ssenv.h"

#if (defined(SS_WIN) || defined(SS_NTGUI)) 

#include <conio.h>

#define SS_WINGETCH ss_wingetch()

#elif defined(DOS) || defined(SS_NT) 

#include <conio.h>
#define ss_putchar  putchar  
#define ss_putch    putch    
#define ss_getch    getch    
#define ss_kbhit    kbhit    
#define ss_gets     gets     

#define SS_WINGETCH

#else /* WINDOWS */

#define SS_WINGETCH

#endif /* WINDOWS */

#endif /* SSCONIO_H */
