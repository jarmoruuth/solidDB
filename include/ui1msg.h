/*************************************************************************\
**  source       * ui0srv.h
**  directory    * ui
**  description  * Server user interface function.
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


#ifndef UI1MSG_H
#define UI1MSG_H

#if defined(SS_WIN) || defined(SS_NTGUI)

#define IDM_ABOUT               100
#define IDM_INFO                101
#define IDM_ASSERT              102
#define IDM_CLEANUP             999
#define IDD_LINE0                   200
#define IDD_LINE1                   201
#define IDD_LINE2                   202
#define IDD_LINE3                   203
#define IDD_LINE4                   204
#define IDD_LINE5                   205
#define IDD_LINE6                   206
#define IDD_LINE7                   207
#define IDD_LINE8                   208
#define IDD_LINE9                   209
#define IDD_LINE10                  210

#define IDD_BASECATALOG     300
#define IDD_USERNAME        301
#define IDD_PASSWORD        302
#define IDD_RETYPEDPASSWORD 303

#define IDD_OK              1
#define IDD_CANCEL          2

#if WIN_NT
#define ABOUTDLGBOX         400
#define TEXTDLGBOX          401
#define UIDBADLG            402
#define UICSDDLG            403
#define SPLASHDLGBOX        404
#else
#define ABOUTDLGBOX         100
#define TEXTDLGBOX          101
#define UIDBADLG            102
#define UICSDDLG            103
#define SPLASHDLGBOX        104
#endif

#define SOLIDBITMAP         500
#define SPLASHBITMAP        501

/* due to tpr 450334, change to 5000 range. */
/* Hope this will not overlap with any other program */
#define UI_WMUSER_MESSAGE   WM_USER+5000
#define UI_WMUSER_WARNING   WM_USER+5001
#define UI_WMUSER_ERROR     WM_USER+5002
#define UI_WMUSER_STARTUP   WM_USER+5003

#endif /* SS_WIN */

#endif /* UI1MSG_H */
