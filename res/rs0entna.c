/*************************************************************************\
**  source       * rs0entna.c
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define RS0ENTNA_C

#include <ssenv.h>

#include <ssstring.h>

#include <ssdebug.h>
#include <ssmem.h>

#include "rs0entna.h"

void rs_entname_initbuf(
        rs_entname_t* en,
        const char* catalog,
        const char* schema,
        const char* name)
{
        ss_debug(en->en_chk = RSCHK_ENTNAME);

        if (catalog != NULL) {
            en->en_catalog = (char *)catalog;
        } else {
            en->en_catalog = NULL;
        }
        if (RS_ENTNAME_ISNAMED((char *)schema)) {
            en->en_schema = (char *)schema;
        } else {
            en->en_schema = NULL;
        }
        if (RS_ENTNAME_ISNAMED((char *)name)) {
            en->en_name = (char *)name;
        } else {
            en->en_name = NULL;
        }
}

rs_entname_t* rs_entname_init(
        const char* catalog,
        const char* schema,
        const char* name)
{
        rs_entname_t* en;

        en = SSMEM_NEW(rs_entname_t);

        ss_debug(en->en_chk = RSCHK_ENTNAME);

        if (catalog != NULL) {
            en->en_catalog = SsMemStrdup((char *)catalog);
        } else {
            en->en_catalog = NULL;
        }
        if (RS_ENTNAME_ISNAMED(schema)) {
            en->en_schema = SsMemStrdup((char *)schema);
        } else {
            en->en_schema = NULL;
        }
        if (RS_ENTNAME_ISNAMED(name)) {
            en->en_name = SsMemStrdup((char *)name);
        } else {
            en->en_name = NULL;
        }
        return(en);
}

rs_entname_t* rs_entname_inittake(
        const char* catalog,
        const char* schema,
        const char* name)
{
        rs_entname_t* en = SSMEM_NEW(rs_entname_t);

        if (!RS_ENTNAME_ISNAMED((char *)schema) && schema != NULL) {
            SsMemFree((char *)schema);
            schema = NULL;
        }

        if (!RS_ENTNAME_ISNAMED((char *)name) && name != NULL) {
            SsMemFree((char *)name);
            name = NULL;
        }

        rs_entname_initbuf(en, catalog, schema, name);

        return (en);
}

void rs_entname_done(
        rs_entname_t* en)
{
        CHK_ENTNAME(en);

        if (en->en_catalog != NULL) {
            SsMemFree(en->en_catalog);
        }
        if (en->en_schema != NULL) {
            SsMemFree(en->en_schema);
        }
        if (en->en_name != NULL) {
            SsMemFree(en->en_name);
        }
        SsMemFree(en);
}

void rs_entname_done_buf(
        rs_entname_t* en)
{
        CHK_ENTNAME(en);

        if (en->en_catalog != NULL) {
            SsMemFree(en->en_catalog);
        }
        if (en->en_schema != NULL) {
            SsMemFree(en->en_schema);
        }
        if (en->en_name != NULL) {
            SsMemFree(en->en_name);
        }
}

rs_entname_t* rs_entname_copy(
        rs_entname_t* en)
{
        CHK_ENTNAME(en);

        return(rs_entname_init(
                en->en_catalog,
                en->en_schema,
                en->en_name));
}

void rs_entname_copybuf(rs_entname_t* dstbuf, rs_entname_t* src)
{
        CHK_ENTNAME(src);

        ss_debug(dstbuf->en_chk = RSCHK_ENTNAME);

        dstbuf->en_catalog = src->en_catalog;
        if (dstbuf->en_catalog != NULL) {
            dstbuf->en_catalog = SsMemStrdup(dstbuf->en_catalog);
        }
        dstbuf->en_schema = src->en_schema;
        if (dstbuf->en_schema != NULL) {
            dstbuf->en_schema = SsMemStrdup(dstbuf->en_schema);
        }
        dstbuf->en_name = src->en_name;
        if (dstbuf->en_name != NULL) {
            dstbuf->en_name = SsMemStrdup(dstbuf->en_name);
        }
}

void rs_entname_setcatalog(
        rs_entname_t* en,
        char* catalog)
{
        CHK_ENTNAME(en);

        if (en->en_catalog != NULL) {
            SsMemFree(en->en_catalog);
        }
        if (catalog != NULL) {
            en->en_catalog = SsMemStrdup(catalog);
        } else {
            en->en_catalog = NULL;
        }
}

int rs_entname_compare(
        rs_entname_t* en1,
        rs_entname_t* en2)
{
        int cmp = 0;

        CHK_ENTNAME(en1);
        CHK_ENTNAME(en2);

        if (en1->en_catalog == NULL) {
            if (en2->en_catalog != NULL) {
                cmp = -1;
            }
        } else if (en2->en_catalog == NULL) {
            cmp = 1;
        } else {
            cmp = strcmp(en1->en_catalog, en2->en_catalog);
        }
        if (cmp != 0) {
            return(cmp);
        }

        ss_dassert(en1->en_name != NULL);
        ss_dassert(en2->en_name != NULL);
        cmp = strcmp(en1->en_name, en2->en_name);
        if (cmp == 0) {
            if (en1->en_schema != NULL && en2->en_schema != NULL) {
                cmp = strcmp(en1->en_schema, en2->en_schema);
            } else if (en1->en_schema != NULL && en2->en_schema == NULL) {
                cmp = 1;
            } else if (en1->en_schema == NULL && en2->en_schema != NULL) {
                cmp = -1;
            } /* else cmp = 0 */
        }
        return(cmp);
}

int rs_entname_comparenames(
        rs_entname_t* en1,
        rs_entname_t* en2)
{
        int cmp = 0;

        CHK_ENTNAME(en1);
        CHK_ENTNAME(en2);

        if (en1->en_catalog == NULL) {
            if (en2->en_catalog != NULL) {
                cmp = -1;
            }
        } else if (en2->en_catalog == NULL) {
            cmp = 1;
        } else {
            cmp = strcmp(en1->en_catalog, en2->en_catalog);
        }
        if (cmp != 0) {
            return(cmp);
        }

        ss_dassert(en1->en_name != NULL);
        ss_dassert(en2->en_name != NULL);
        cmp = strcmp(en1->en_name, en2->en_name);
        return(cmp);
}

char* rs_entname_getprintschema(
        rs_entname_t* en)
{
        CHK_ENTNAME(en);

        if (en->en_schema == NULL) {
            return((char *)"NULL");
        } else {
            return(en->en_schema);
        }
}

char* rs_entname_getprintcatalog(
        rs_entname_t* en)
{
        CHK_ENTNAME(en);

        if (en->en_catalog == NULL) {
            return((char *)"NULL");
        } else {
            return(en->en_catalog);
        }
}

/*##**********************************************************************\
 * 
 *		rs_entname_printname
 * 
 * Prints entity name. Prints also catalog and schema info, if they are
 * available. Returned buffer is allocated with SsMemAlloc and caller
 * must release it.
 * 
 * Parameters : 
 * 
 *	en - in
 *		Entity name.
 *		
 * Return value - give : 
 *      Name in <catalog>.<schema>.<name> format. If catalog or schema
 *      is not available, they are not printed.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_entname_printname(
        rs_entname_t* en)
{
        int len = 0;
        char* name;

        CHK_ENTNAME(en);

        if (en->en_schema != NULL) {
            if (en->en_catalog != NULL) {
                len += strlen(en->en_catalog);
            }
            len += strlen(en->en_schema);
        }
        len += strlen(en->en_name);

        name = SsMemAlloc(len + 2 + 1);

        name[0] = '\0';

        if (en->en_schema != NULL) {
            if (en->en_catalog != NULL) {
                strcat(name, en->en_catalog);
                strcat(name, ".");
            }
            strcat(name, en->en_schema);
            strcat(name, ".");
        }
        strcat(name, en->en_name);

        return(name);
}
