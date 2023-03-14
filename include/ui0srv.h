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


#ifndef UI0SRV_H
#define UI0SRV_H

#include <ssc.h>

bool ui_srv_init(
        char* srvname,
        bool hide_icon,
        bool (*normal_shutdown_fp)(void),
        bool (*quick_shutdown_fp)(void),
        uint (*usercount_fp)(void));

void ui_srv_done(
        void);

void ui_srv_startup(
        void);

void ui_srv_setconfirmshutdown_fp(
        bool (*confirmshutdown_fp)(void));

void ui_srv_setinfo_fp(
        char* (*info_fp)(void));

bool ui_srv_isgui(
        void);

#endif /* UI0SRV_H */
