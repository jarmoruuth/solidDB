/*************************************************************************\
**  source       * ui0msg.h
**  directory    * ui
**  description  * Message functions.
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


#ifndef UI0MSG_H
#define UI0MSG_H

#include <ssstdlib.h>
#include <ssstdio.h>
#include <ssc.h>

#include "ssfile.h"
#include "su0msgs.h"

typedef enum {
        UI_MSG_MESSAGE,
        UI_MSG_WARNING,
        UI_MSG_ERROR
} ui_msgtype_t;

#if defined(SS_WIN) || defined(SS_NTGUI)

#ifdef SS_WIN
typedef unsigned int SS_HWND;
#else
typedef unsigned long SS_HWND;
#endif

void* GetTextBox(
        void);

void* GetAboutBox(
        void);

void* TextBoxWin(
        int usetimer,
        SS_HWND hMainWnd,
        char *txt);

void TextBoxWin_done(
        void *textbox);

void AboutBoxWin(
        int usetimer,
        SS_HWND hMainWnd,
        char *l1,
        char *l2,
        char *l3,
        char *l4,
        char *l5,
        char *l6,
        char *l7,
        char *l8);

void AboutBoxWin_done(
        void *aboutbox);

void SS_CDECL ui_msg_message_nogui(
        int msgcode, ...);

#else /* SS_WIN || SS_NTGUI */

#define ui_msg_message_nogui ui_msg_message

#endif /* SS_WIN || SS_NTGUI */

void SS_CDECL ui_msg_message(
        int msgcode, ...);

void SS_CDECL ui_msg_message_status(
        int msgcode, ...);

char* ui_getpass(
        const char* prompt);

void SS_CDECL ui_msg_messagebox(
        char* header,
        int msgcode, ...);

void SS_CDECL ui_msg_warning(
        int msgcode, ...);

void SS_CDECL ui_msg_error_nostop(
        int msgcode, ...);

void SS_CDECL ui_msg_error(
        int msgcode, ...);

bool ui_msg_getdba(
        char* username,
        size_t username_size,
        char* password,
        size_t password_size, 
        char* catalog,
        size_t catalog_size
    );

bool ui_msg_confirmshutdown(
        char* username,
        size_t username_size,
        char* password,
        size_t password_size);

void ui_msg_getuser(
        char* username,
        size_t username_size,
        char* password,
        size_t password_size);

void ui_msg_getuser_UTF8(
        char* username,
        size_t username_size,
        char* password,
        size_t password_size);

void ui_msg_sqlwarning(
        uint code,
        char* str);

void ui_msg_setdba(
        char* username,
        char* password);

void ui_msg_getdefdba(
        char** p_username,
        char** p_password);

void ui_msg_setdefcatalog(
        char* catalog);

char* ui_msg_getdefcatalog(
        void);

void ui_msg_setmessagefp(
        void (*message_fp)(ui_msgtype_t type, su_msgret_t msgcode, char* msg, bool newline));

int ui_msg_fgetc(
        SS_FILE* fp);

char* ui_msg_fgets(
        char* buf,
        size_t bufsize,
        SS_FILE* fp);

void ui_msg_generate_error(
        const char* msg);

#endif /* UI0MSG_H */

