/*************************************************************************\
**  source       * ssservic.h
**  directory    * ss
**  description  * Service function interface.
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


#ifndef SSSERVIC_H
#define SSSERVIC_H

#include "ssc.h"

typedef enum {
        SS_SVC_INFO,
        SS_SVC_WARNING,
        SS_SVC_ERROR
} ss_svc_msgtype_t;

typedef enum {
        SS_SVC_FOREGROUND_NO,
        SS_SVC_FOREGROUND_YES,
        SS_SVC_FOREGROUND_UNKNOWN
} ss_svc_foreground_t;

void ss_svc_main(
        char* name,
        bool (*init_fp)(void),
        bool (*process_fp)(void),
        bool (*done_fp)(void),
        bool (*stop_fp)(void),
        bool (*quickstop_fp)(void),
        ss_svc_foreground_t* p_foreground);

void ss_svc_stop(
        bool internal_error);

void ss_svc_notify_init(
        void);

void ss_svc_notify_done(
        void);

void ss_svc_logmessage(
        ss_svc_msgtype_t type,
        char* msg);

bool ss_svc_isservice(
        void);

int ss_svc_install(
        char* svc_name,
        char* exe_location,
        bool autostart);

int ss_svc_remove(
        char* svc_name);

char* ss_svc_errorcodetotext(
        int rc);

#endif /* SSSERVIC_H */
