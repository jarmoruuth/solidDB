/*************************************************************************\
**  source       * rs0rbuf.h
**  directory    * res
**  description  * Relation (and view) information buffering
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


#ifndef RS0RBUF_H
#define RS0RBUF_H

#include <ssenv.h>

#include <ssstddef.h>
#include <ssstring.h>

#include <ssc.h>

#include <uti0dyn.h>

#include "rs0types.h"
#include "rs0entna.h"
#include "rs0cardi.h"

typedef enum {
        RSRBUF_EXISTS,   /* Relation/view exists in db but is not buffered */
        RSRBUF_NOTEXIST, /* Relation/view does not exist in db */
        RSRBUF_BUFFERED, /* Relation/view exists and is already buffered */
        RSRBUF_AMBIGUOUS /* Ambiguous name, multiple entities with same name */
} rs_rbuf_present_t;

typedef enum {
        RSRBUF_SUCCESS,
        RSRBUF_ERR_EXISTS,
        RSRBUF_ERR_INVALID_ARG
}  rs_rbuf_ret_t;

typedef enum {
        RSRBUF_NAME_GENERIC,
        RSRBUF_NAME_RELATION,
        RSRBUF_NAME_VIEW,
        RSRBUF_NAME_PROCEDURE,
        RSRBUF_NAME_SEQUENCE,
        RSRBUF_NAME_EVENT,
        RSRBUF_NAME_TRIGGER,
        RSRBUF_NAME_PUBLICATION
} rs_rbuf_nametype_t;

typedef struct relbufstruct rs_rbuf_t;

rs_rbuf_t* rs_rbuf_init(
        void*      cd,
        rs_auth_t* auth
);

void rs_rbuf_done(
        void*      cd,
        rs_rbuf_t* rbuf
);

rs_rbuf_t* rs_rbuf_init_replace(
        void*      cd,
        rs_rbuf_t* old_rbuf
);

void rs_rbuf_replace(
        void*      cd,
        rs_rbuf_t* target_rbuf,
        rs_rbuf_t* source_rbuf
);

void rs_rbuf_setresetcallback(
        void*      cd,
        rs_rbuf_t* rbuf,
        void       (*resetcallback)(rs_sysi_t* cd, rs_rbuf_t* rbuf, bool unicode_enabled),
        bool       unicode_enabled
);
        
void rs_rbuf_setmaxbuffered(
        void*      cd,
        rs_rbuf_t* rbuf,
        uint       maxbuffered
);

void rs_rbuf_setrecovery(
        void*      cd,
        rs_rbuf_t* rbuf,
        bool       isrecovery
);

/************  NAMES **************/

bool rs_rbuf_addname(
        void*               cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       name,
        rs_rbuf_nametype_t  type,
        long                id
);

bool rs_rbuf_removename(
        void*               cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       name,
        rs_rbuf_nametype_t  type
);

bool rs_rbuf_namebyid(
        void*               cd,
        rs_rbuf_t*          rbuf,
        ulong               id,
        rs_rbuf_nametype_t  type,
        rs_entname_t**      p_name);

bool rs_rbuf_nameinuse(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname);

void rs_rbuf_removeallnames(
        void*       cd,
        rs_rbuf_t*  rbuf
);

/************  RELATIONS  **************/

rs_rbuf_present_t rs_rbuf_relpresent(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        rs_relh_t**     p_relh,    /* Returned, if return value == BUFFERED */
        ulong*          p_relid    /* Returned, if RELEXISTS or BUFFERED */
);

bool rs_rbuf_relnamebyid(
        void*           cd,
        rs_rbuf_t*      rbuf,
        ulong           relid,
        rs_entname_t**  p_relname);

bool rs_rbuf_relhbyid(
        void*           cd,
        rs_rbuf_t*      rbuf,
        ulong           relid,
        rs_relh_t**     p_relh,
        rs_entname_t**  p_relname);

bool rs_rbuf_addrelname(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        ulong           relid
);

bool rs_rbuf_addrelnameandcardin(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        ulong           relid,
        rs_cardin_t*    cardin
);

rs_cardin_t* rs_rbuf_getcardin(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname
);

bool rs_rbuf_updaterelname(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        ulong           relid,
        rs_cardin_t*    cardin
);

rs_rbuf_ret_t rs_rbuf_insertrelh_ex(
        void*      cd,
        rs_rbuf_t* rbuf,
        rs_relh_t* relh
);

rs_rbuf_ret_t rs_rbuf_insertrelh_ex_nomutex(
        void*      cd,
        rs_rbuf_t* rbuf,
        rs_relh_t* relh
);

bool rs_rbuf_removerelh(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname
);

bool rs_rbuf_relhunbuffer(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname);

bool rs_rbuf_relhunbuffer_dropcardin(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname);

bool rs_rbuf_renamerel(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   oldname,
        rs_entname_t*   newname);

rs_relh_t* rs_rbuf_linkrelh(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname
);

/************  VIEWS  **************/

rs_rbuf_present_t rs_rbuf_viewpresent(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname,
        rs_viewh_t**    p_viewh,    /* Returned, if return value == BUFFERED */
        ulong*          p_viewid    /* Returned, if RELEXISTS or BUFFERED */
);

bool rs_rbuf_viewnamebyid(
        void*           cd,
        rs_rbuf_t*      rbuf,
        ulong           viewid,
        rs_entname_t**  p_viewname);

bool rs_rbuf_insertviewh(
        void*       cd,
        rs_rbuf_t*  rbuf,
        rs_viewh_t* viewh
);

bool rs_rbuf_addviewname(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname,
        ulong           viewid
);

bool rs_rbuf_updateviewname(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname,
        ulong           viewid
);

bool rs_rbuf_removeviewh(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname
);

bool rs_rbuf_viewhunbuffer(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname);

rs_viewh_t* rs_rbuf_linkviewh(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname
);

/************  EVENTS  **************/

bool rs_rbuf_event_add(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_event_t*     event);

bool rs_rbuf_event_remove(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   name);

bool rs_rbuf_event_findref(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   name,
        rs_event_t**    p_event);


/************  EVENTS end **************/

void rs_rbuf_iterate(
        void*      cd,
        rs_rbuf_t* rbuf,
        void*      userinfo,
        void       (*iterfun)(
                        void* ui,
                        bool is_rel,
                        void* relh_or_viewh,
                        long id,
                        void* cardin)
);

bool rs_rbuf_ischemaobjects(
        void*      cd,
        rs_rbuf_t* rbuf,
        char*      schemaname);

void rs_rbuf_printinfo(
        void*      fp,
        rs_rbuf_t* rbuf
);

#ifdef SS_DEBUG

void rs_rbuf_print(
        void*      cd,
        rs_rbuf_t* rbuf);

#endif /* SS_DEBUG */

void rs_rbuf_modifysystablechartypes(
        void* cd,
        rs_rbuf_t* rbuf);

void rs_rbuf_replacenullcatalogs(
        void* cd,
        rs_rbuf_t* rbuf,
        char* newcatalog);

#endif /* RS0RBUF_H */
