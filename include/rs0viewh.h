/*************************************************************************\
**  source       * rs0viewh.h
**  directory    * res
**  description  * View handle functions
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


#ifndef RS0VIEWH_H
#define RS0VIEWH_H

#include "rs0types.h"
#include "rs0entna.h"

rs_viewh_t* rs_viewh_init(
	void* cd,
	rs_entname_t* viewname,
        ulong viewid,
        rs_ttype_t* ttype
);

void rs_viewh_done(
        void*       cd,
        rs_viewh_t* viewh
);

void rs_viewh_link(
	void*      cd,
	rs_viewh_t* viewh
);

char* rs_viewh_def(
        void*       cd,
        rs_viewh_t* viewh
);

void rs_viewh_setdef(
        void*       cd,
        rs_viewh_t* viewh,
        char*       viewdef 
);

rs_ttype_t* rs_viewh_ttype(
        void*       cd,
        rs_viewh_t* viewh
);

void rs_viewh_setviewid(
        void*       cd,
        rs_viewh_t* viewh,
        ulong       viewid
);

ulong rs_viewh_viewid(
        void*       cd,
        rs_viewh_t* viewh
);

char* rs_viewh_name(
        void*       cd,
        rs_viewh_t* viewh
);

rs_entname_t* rs_viewh_entname(
        void*       cd,
        rs_viewh_t* viewh
);

char* rs_viewh_schema(
	void*       cd,
	rs_viewh_t* viewh
);

char* rs_viewh_catalog(
        void*      cd,
        rs_viewh_t* viewh);

void rs_viewh_setcatalog(
        void*      cd,
        rs_viewh_t* viewh,
        char* newcatalog);

bool rs_viewh_issysview(
	void*       cd,
	rs_viewh_t* viewh
);

#ifdef SS_DEBUG
void rs_viewh_print(
        void*       cd,
        rs_viewh_t* viewh
);
#endif /* SS_DEBUG */

#endif /* RS0VIEWH_H */
