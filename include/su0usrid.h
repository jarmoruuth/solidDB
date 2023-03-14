/*************************************************************************\
**  source       * su0usrid.h
**  directory    * su
**  description  * Server user id generation routines.
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


#ifndef SU0USRID_H
#define SU0USRID_H

#include <ssmsglog.h>

#include "su0bflag.h"
#include "su0list.h"

typedef enum {
        SU_USRID_TRACE_SQL      = SU_BFLAG_BIT(0),
        SU_USRID_TRACE_RPC      = SU_BFLAG_BIT(1),
        SU_USRID_TRACE_SNC      = SU_BFLAG_BIT(2),
        SU_USRID_TRACE_REXEC    = SU_BFLAG_BIT(3),
        SU_USRID_TRACE_BATCH    = SU_BFLAG_BIT(4),
        SU_USRID_TRACE_SNCPLANS = SU_BFLAG_BIT(5),
        SU_USRID_TRACE_EST      = SU_BFLAG_BIT(6),
        SU_USRID_TRACE_ESTINFO  = SU_BFLAG_BIT(7)
} su_usrid_trace_t;

extern bool        su_usrid_usertaskqueue;
extern su_bflag_t  su_usrid_traceflags;

int su_usrid_init(
        void);

void su_usrid_link(
        int usrid);

void su_usrid_done(
        int usrid);

void su_usrid_traceon(
        void);

void su_usrid_traceoff(
        void);

SsMsgLogT* su_usrid_gettracelog(
        void);

bool su_usrid_istrace(
        su_usrid_trace_t type);

void SS_CDECL su_usrid_trace(
        int usrid,
        su_usrid_trace_t type,
        int level,
        char* format,
        ...);

extern int su_usrid_tracelevel;

#define su_usrid_trace_push(usrid, name, info, id) \
        do { \
            if (su_usrid_tracelevel > 0) { \
                su_usrid_trace_push_fun(usrid, name, info, id); \
            } \
        } while (FALSE)

void su_usrid_trace_push_fun(
        int usrid,
        char* name,
        char* info,
        long id);

#define su_usrid_trace_pop(usrid) \
        do { \
            if (su_usrid_tracelevel > 0) { \
                su_usrid_trace_pop_fun(usrid); \
            } \
        } while (FALSE)

void su_usrid_trace_pop_fun(
        int usrid);

su_list_t* su_usrid_trace_activelist(
        void);

bool su_usrid_settracetype(
        char* tracetype);

bool su_usrid_unsettracetype(
        char* tracetype);

void su_usrid_globalinit(
        void);

void su_usrid_globaldone(
        void);

#endif /* SU0USRID_H */
