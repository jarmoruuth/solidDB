/*************************************************************************\
**  source       * rs0evnot.c
**  directory    * res
**  description  * Server events notifier module
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


#ifndef RS0EVNOT_H
#define RS0EVNOT_H

#include <ssc.h>
#include "rs0admev.h"

typedef struct rs_eventcallback_st rs_eventcallback_t;

typedef struct rs_eventnotifier_st rs_eventnotifier_t;

typedef struct rs_eventnotifiers_st rs_eventnotifiers_t;

/* Callback prototype */
typedef int (*rs_notify_fun)(
                char* eventname,
                int  id,
                char* textdata,
                bool notextdata,
                long numdata,
                bool nonumdata,
                long userid,
                bool nouserid,
                void* userdata);


rs_eventnotifiers_t* rs_eventnotifiers_init(
        void);

void rs_eventnotifiers_done(
        rs_eventnotifiers_t*);

uint rs_eventnotifiers_register(
        rs_sysi_t*              cd,
        rs_admevid_t            event,
        rs_notify_fun           callback,
        void*                   userdata);

void rs_eventnotifiers_unregister(
        rs_sysi_t*              cd,
        rs_admevid_t            event,
        uint                    id);

int rs_eventnotifiers_call(
        rs_sysi_t* cd,
        char*      eventname,
        char*      textdata,
        bool       notextdata,
        long       numdata,
        bool       nonumdata,
        long       userid,
        bool       nouserid);

int rs_eventnotifiers_postandcall(
        rs_sysi_t*  cd,
        char*       eventname,
        char*       textdata,
        bool        notextdata,
        long        numdata,
        bool        nonumdata,
        long        userid,
        bool        nouserid,
        char*       debug);

void rs_eventnotifiers_globalinit(
        void (*postfun)(
            rs_sysi_t* cd,
            char* eventname,
            char* textdata,
            bool notextdata,
            long numdata,
            bool nonumdata,
            long userid,
            bool nouserid));

void rs_eventnotifiers_globaldone(
        void);

#endif /* RS0EVNOT_H */
