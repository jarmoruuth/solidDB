/*************************************************************************\
**  source       * rs0entna.h
**  directory    * res
**  description  * Entity name object.
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


#ifndef RS0ENTNA_H
#define RS0ENTNA_H

#include <ssdebug.h>

#include <rs0types.h>
#include <rs0sdefs.h>

#define CHK_ENTNAME(en)     ss_dassert(SS_CHKPTR(en) && (en)->en_chk == RSCHK_ENTNAME)

typedef struct {
        ss_debug(rs_check_t en_chk;)
        char*   en_catalog;
        char*   en_schema;
        char*   en_name;
} rs_entname_t;

#define RS_ENTNAME_ISNAMED(schema_or_catalog) \
        ((schema_or_catalog) != NULL && (schema_or_catalog)[0] != '\0')

#define RS_ENTNAME_ISSCHEMA(sch) RS_ENTNAME_ISNAMED(sch)

void rs_entname_initbuf(
        rs_entname_t* en,
        const char*   catalog,
        const char* schema,
        const char* name);

rs_entname_t* rs_entname_init(
        const char* catalog,
        const char* schema,
        const char* name);

rs_entname_t* rs_entname_inittake(
        const char* catalog,
        const char* schema,
        const char* name);

void rs_entname_done(
        rs_entname_t* en);

void rs_entname_done_buf(
        rs_entname_t* en);

rs_entname_t* rs_entname_copy(
        rs_entname_t* en);

void rs_entname_copybuf(
        rs_entname_t* dstbuf,
        rs_entname_t* src);

void rs_entname_setcatalog(
        rs_entname_t* en,
        char* catalog);

SS_INLINE char* rs_entname_getcatalog(
        rs_entname_t* en);

SS_INLINE char* rs_entname_getschema(
        rs_entname_t* en);

SS_INLINE char* rs_entname_getname(
        rs_entname_t* en);

int rs_entname_compare(
        rs_entname_t* en1,
        rs_entname_t* en2);

int rs_entname_comparenames(
        rs_entname_t* en1,
        rs_entname_t* en2);

char* rs_entname_printname(
        rs_entname_t* en);

char* rs_entname_getprintcatalog(
        rs_entname_t* en);

char* rs_entname_getprintschema(
        rs_entname_t* en);

#if defined(RS0ENTNA_C) || defined(SS_USE_INLINE)

SS_INLINE char* rs_entname_getcatalog(
        rs_entname_t* en)
{
        CHK_ENTNAME(en);

        return(en->en_catalog);
}

SS_INLINE char* rs_entname_getschema(
        rs_entname_t* en)
{
        CHK_ENTNAME(en);

        return(en->en_schema);
}

SS_INLINE char* rs_entname_getname(
        rs_entname_t* en)
{
        CHK_ENTNAME(en);

        return(en->en_name);
}

#endif /* defined(RS0ENTNA_C) || defined(SS_USE_INLINE) */

#endif /* RS0ENTNA_H */
