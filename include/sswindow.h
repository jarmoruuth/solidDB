/*************************************************************************\
**  source       * sswindow.h
**  directory    * ss
**  description  * windows.h
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


#ifndef SSWINDOW_H
#define SSWINDOW_H

#include "ssenv.h"

#if defined(SS_WIN) 

/* The COMM section causes all those pesky "Non-standard extension" warnings,
   so exclude it by default. */
#ifndef SS_NONOCOMM
#  define NOCOMM
#endif

#define NOMINMAX /* don't use min or max, use MIN and MAX */

/* prevent redefinition warnings */
#undef NULL

#ifdef SS_EXE
/* To exclude definitions introduced in version 3.1 when test executables
 *  are compiled. This allows us to run them in Win OS/2 2.0 full-screen
 */
# define WINVER  0x0300
#endif /* SS_EXE */

#include <windows.h>

/* Following defines for MSC60 */
#if defined(MSC) && (MSC==60)
#  define HINSTANCE HANDLE
#  define HGLOBAL HANDLE
#  define UINT  unsigned int
#  define LPARAM long
#  define WPARAM WORD
#  define LRESULT long
#  define HINSTANCE_ERROR ((HINSTANCE)32)
#  define SEM_NOOPENFILEERRORBOX 0x8000
#  define CALLBACK FAR PASCAL
   typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
#  define HWND_DESKTOP        ((HWND)0)
#endif

#elif defined(SS_NT) /* SS_WIN */

#include "sswinnt.h"

#endif /* SS_WIN */


#endif /* SSWINDOW_H */

