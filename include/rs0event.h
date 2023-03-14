/*************************************************************************\
**  source       * rs0event.h
**  directory    * res
**  description  * Event handle functions
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


#ifndef RS0EVENT_H
#define RS0EVENT_H

#include "rs0types.h"
#include "rs0entna.h"

rs_event_t* rs_event_init(
	void* cd,
        rs_entname_t* name,
        long eventid,
        int paramcount,
        int* paramtypes);

void rs_event_done(
        void*       cd,
        rs_event_t* eventh
);

void rs_event_link(
	void*      cd,
	rs_event_t* eventh
);

ulong rs_event_eventid(
        void*       cd,
        rs_event_t* eventh
);

int rs_event_paramcount(
        void*       cd,
        rs_event_t* eventh
);

int* rs_event_paramtypes(
        void*       cd,
        rs_event_t* eventh
);

char* rs_event_name(
        void*       cd,
        rs_event_t* eventh
);

rs_entname_t* rs_event_entname(
        void*       cd,
        rs_event_t* eventh
);

char* rs_event_schema(
	void*       cd,
	rs_event_t* eventh
);

char* rs_event_catalog(
        void*      cd,
        rs_event_t* eventh);

#endif /* RS0EVENT_H */
