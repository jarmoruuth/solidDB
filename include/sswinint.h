/*************************************************************************\
**  source       * sswinint.h
**  directory    * ss
**  description  * Windows internals for SS
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


#ifndef SSWININT_H
#define SSWININT_H

#include "ssenv.h"

#if defined(SS_WIN) || defined(SS_NTGUI)

#include "sswindow.h"

BOOL SsWinInit(HANDLE hInst, char* lpszClassName);
BOOL SsWinDone(HANDLE hInst, char* lpszClassName);

#elif defined(SS_NT)

#include "sswindow.h"

#endif /* WINDOWS */

#if defined(SS_NT)
#       define SS_HINST_TYPE    HINSTANCE
        extern SS_HINST_TYPE    SshInst;
        extern HANDLE SshPrevInst;
#elif defined(SS_WIN)
#       define SS_HINST_TYPE    HANDLE
        extern SS_HINST_TYPE    SshInst;
        extern HANDLE SshPrevInst;
#endif /* SS_NT */

#endif /* SSWININT_H */
