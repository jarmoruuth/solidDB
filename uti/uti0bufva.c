/*************************************************************************\
**  source       * uti0bufva.c
**  directory    * uti
**  description  * locally buffered v-attribute
**               * 
**               * Copyright (C) 2007 Solid Information Technology Ltd
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

#define UTI0BUFVA_C

#include <ssdebug.h>
#include <ssmem.h>
#include <ssstring.h>
#include "uti0bufva.h"
#include "uti1val.h"

/*##**********************************************************************\
 *
 *      bufva_init
 *
 * Initializes a buffer to contain empty v-attribute
 *
 * Parameters:
 *      buf - in, out, use
 *          memory buffer
 *
 *      bufsize - in
 *          buf size in bytes
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef bufva_init
void bufva_init(ss_byte_t* buf, size_t bufsize)
{
        BUFVA_CHKBUFSIZE(bufsize);
        buf[0] = (ss_byte_t)0;
}
#endif /* !bufva_init */

void bufva_move(ss_byte_t* destbuf, size_t destbufsize,
                ss_byte_t* srcbuf, size_t srcbufsize)
{
        size_t ntocopy;
        
        ss_dassert(destbuf != srcbuf);
        if ((size_t)(BUFVA_1BYTEGROSSLEN(destbuf)) > destbufsize) {
            SsMemFree(BUFVA_GET_DYNAMIC_VA(destbuf));
        }
        ntocopy = BUFVA_1BYTEGROSSLEN(srcbuf);
        if (ntocopy > srcbufsize) {
            /* dynamically allocated */
            va_t* p_va1 = BUFVA_GET_DYNAMIC_VA(srcbuf);
            BUFVA_SET_DYNAMIC_VA(destbuf, p_va1);
        } else {
            if (ntocopy > destbufsize) {
                bufva_setva(destbuf, destbufsize, (va_t*)srcbuf);
            } else {
                memcpy(destbuf, srcbuf, ntocopy);
            }
        }
        srcbuf[0] = 0;
}

void bufva_setdata(
        ss_byte_t* buf, size_t bufsize,
        void* data, size_t datalen)
{
        size_t new_grosslen;
        va_t* p_va1;
        
        BUFVA_CHKBUFSIZE(bufsize);
        if (datalen == 0) {
            data = (void*)VA_NULL;
        }
        new_grosslen = datalen + 1;
        if ((size_t)(BUFVA_1BYTEGROSSLEN(buf)) > bufsize) {
            /* current buffer contents has dynamic va pointer */
            p_va1 = BUFVA_GET_DYNAMIC_VA(buf);
            if (new_grosslen > bufsize) {
                dynva_setdata(&p_va1, data, (va_index_t)datalen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                dynva_free(&p_va1);
                va_setdata((va_t*)buf, data, (va_index_t)datalen);
            }
        } else {
            if (new_grosslen > bufsize) {
                p_va1 = dynva_init();
                dynva_setdata(&p_va1, data, (va_index_t)datalen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                va_setdata((va_t*)buf, data, (va_index_t)datalen);
            }
        }
}


void bufva_appdata(ss_byte_t* buf, size_t bufsize,
                   void* data, size_t datalen)
{
        size_t new_netlen;
        size_t new_grosslen;
        size_t old_netlen;
        size_t old_lenlen;
        size_t new_lenlen;
        va_t* p_va1;

        BUFVA_CHKBUFSIZE(bufsize);
        new_netlen = datalen;
        old_netlen = BUFVA_1BYTENETLEN(buf);
        if (old_netlen >= bufsize) {
            /* current buffer contents has dynamic va pointer */
            p_va1 = BUFVA_GET_DYNAMIC_VA(buf);
            old_netlen = VA_NETLEN(p_va1);
            if (BYTE_LEN_TEST(old_netlen)) {
                old_lenlen = 1;
            } else {
                old_lenlen = 5;
            }
            new_netlen += old_netlen;
            if (BYTE_LEN_TEST(new_netlen)) {
                new_lenlen = 1;
            } else {
                new_lenlen = 5;
            }
            new_grosslen = new_lenlen + new_netlen;
            p_va1 = SsMemRealloc(p_va1, new_grosslen);
            if (new_lenlen > old_lenlen) {
                ss_dassert(new_lenlen == 5 && old_lenlen == 1);
                memmove((ss_byte_t*)p_va1 + new_lenlen,
                        (ss_byte_t*)p_va1 + old_lenlen,
                        old_netlen);
            }
            /* the below va_setdata call only sets the length field! */
            va_setdata((va_t*)(ss_byte_t*)p_va1, NULL, new_netlen);
            memcpy((ss_byte_t*)p_va1 + new_lenlen + old_netlen,
                   data,
                   datalen);
            BUFVA_SET_DYNAMIC_VA(buf, p_va1);
        } else {
            /* note: old_lenlen is not used, but literal 1 */
            new_netlen += old_netlen;
            if (new_netlen >= bufsize) {
                /* new value won't fit */
                if (BYTE_LEN_TEST(new_netlen)) {
                    new_lenlen = 1;
                } else {
                    new_lenlen = 5;
                }
                new_grosslen = new_netlen + new_lenlen;
                p_va1 = SsMemAlloc(new_grosslen);
                va_setdata(p_va1,
                           NULL,
                           (va_index_t)new_netlen);
                memcpy((ss_byte_t*)p_va1 + new_lenlen,
                       buf + 1,
                       old_netlen);
                memcpy((ss_byte_t*)p_va1 + new_lenlen + old_netlen,
                       data, datalen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                /* (hopefully) the most common case! */
                memcpy(buf + 1 + old_netlen,
                       data,
                       datalen);
                buf[0] = (ss_byte_t)new_netlen;
            }
        }
}

void bufva_appva(ss_byte_t* buf, size_t bufsize,
                 va_t* va)
{
        void* data;
        va_index_t datalen;

        VA_GETDATA(data, va, datalen);
        bufva_appdata(buf, bufsize, data, datalen);
}

/* buffered v-tuple begin */

/*##**********************************************************************\
 *
 *      bufvtpl_init
 *
 * Initializes a buffer to contain empty v-tuple
 *
 * Parameters:
 *      buf - in, out, use
 *          memory buffer
 *
 *      bufsize - in
 *          buf size in bytes
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef bufvtpl_init
void bufvtpl_init(ss_byte_t* buf, size_t bufsize)
{
        bufva_init(buf, bufsize);
}
#endif /* !bufvtpl_init */

/*##**********************************************************************\
 *
 *      bufvtpl_done
 *
 * Resets v-tuple buffer to init value (and releases possible
 * dynamically allocated buffer)
 *
 * Parameters:
 *      buf - in out, use
 *          v-tuple buffer
 *
 *      bufsize - in
 *          buffer size
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef bufvtpl_done
void bufvtpl_done(ss_byte_t* buf, size_t bufsize)
{
        bufva_done(buf, bufsize);
}
#endif /* !bufvtpl_done */


/*##**********************************************************************\
 *
 *      bufvtpl_move
 *
 * Moves value from srcbuf to destbuf and makes srcbuf empty.
 * Possible existing value in destbuf is freed.
 *
 * Parameters:
 *      destbuf - in out, use
 *          destination vtuple buffer
 *
 *      destbufsize - in
 *          
 *
 *      srcbuf - in out, use
 *          src buffer, emptied after data move
 *
 *      srcbufsize - in
 *          
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef bufvtpl_move
void bufvtpl_move(ss_byte_t* destbuf, size_t destbufsize,
                  ss_byte_t* srcbuf, size_t srcbufsize)
{
        bufva_move(destbuf, destbufsize,
                   srcbuf, srcbufsize);
}
#endif /* !bufvtpl_move */

/*##**********************************************************************\
 *
 *      bufvtpl_setvtpl
 *
 * Sets a v-tuple value into buffered v-tuple.
 * In case vtuple value is bigger than buffer, automatically allocates
 * new storage and sets a pointer into buffer
 *
 * Parameters:
 *      buf - in out, use
 *          v-tuple buffer
 *
 *      bufsize - in
 *          buffer size in bytes
 *
 *      vtpl - in, use
 *          pointer to v-tuple value
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef bufvtpl_setvtpl
void bufvtpl_setvtpl(ss_byte_t* buf, size_t bufsize,
                     vtpl_t* vtpl)
{
        bufva_setva(buf, bufsize, (va_t*)(ss_byte_t*)vtpl);
}
#endif /* !bufvtpl_setvtpl */

#ifndef bufvtpl_setdata
void bufvtpl_setdata(ss_byte_t* buf, size_t bufsize,
                     void* data, size_t datalen)
{
        bufva_setdata(buf, bufsize, data, datalen);
}
#endif /* !bufvtpl_setdata */

/*##**********************************************************************\
 *
 *      bufvtpl_allocvtpl
 *
 * Allocates a vtuple to be used for a write buffer for vtuple value
 * given later.
 *
 * Parameters:
 *      buf - in out, use
 *          vtuple buffer
 *
 *      bufsize - in
 *          buffer size
 *
 *      new_grosslen - in
 *          grosslen for new vtuple
 *
 * Return value - ref:
 *      pointer to vtuple buffer whose length is as requested but
 *      net contents is uninitialized.
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef bufvtpl_allocvtpl
vtpl_t* bufvtpl_allocvtpl(ss_byte_t* buf, size_t bufsize,
                          size_t new_grosslen)
{
        return ((vtpl_t*)(ss_byte_t*)
                bufva_allocva(buf, bufsize, new_grosslen));
}
#endif /* !bufvtpl_allocvtpl */

/*##**********************************************************************\
 *
 *      bufvtpl_getvtpl
 *
 * Gets a pointer to v-tuple value either contained in the buffer
 * or pointed to by pointer in the buffer.
 *
 * Parameters:
 *      buf - in, use
 *          v-tuple buffer
 *
 *      bufsize - in
 *          buffer size in bytes
 *
 * Return value - ref, read-only
 *      pointer to v-tuple value
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef bufvtpl_getvtpl
vtpl_t* bufvtpl_getvtpl(ss_byte_t* buf, size_t bufsize)
{
        BUFVA_CHKBUFSIZE(bufsize);
        return _BUFVTPL_GETVTPL_(buf, bufsize);
}
#endif

void bufvtpl_appvawithincrement(ss_byte_t* buf, size_t bufsize,
                                va_t* va)
{
        size_t new_netlen;
        size_t new_grosslen;
        size_t new_lenlen;
        size_t old_netlen;
        size_t old_lenlen;
        size_t new_netlen_increase;
        vtpl_t* p_vtpl1;
        size_t va_datalen;
        ss_byte_t*  va_data;
        dynva_t dynva = NULL;
        
        BUFVA_CHKBUFSIZE(bufsize);
        VA_GETDATA(va_data, va, va_datalen);
        if (BYTE_LEN_TEST(va_datalen) && !BYTE_LEN_TEST(va_datalen + 1)) {
            dynva_setdataandnull(&dynva, va_data, va_datalen);
        } else {
            dynva = va;
            /* The below is a trick: with NULL data pointer
               only the length field of va will be updated,
               so the added byte is never written and the operation is safe.
               The original length is restored before return
            */
            va_setdata(dynva, NULL, va_datalen + 1);
        }
        new_netlen_increase = va_grosslen(dynva);
        new_netlen = new_netlen_increase;
        old_netlen = BUFVTPL_1BYTENETLEN(buf);
        if (old_netlen >= bufsize) {
            p_vtpl1 = BUFVTPL_GET_DYNAMIC_VTPL(buf);
            old_netlen = VA_NETLEN((va_t*)(ss_byte_t*)p_vtpl1);
            if (BYTE_LEN_TEST(old_netlen)) {
                old_lenlen = 1;
            } else {
                old_lenlen = 5;
            }
            new_netlen += old_netlen;
            if (BYTE_LEN_TEST(new_netlen)) {
                new_lenlen = 1;
            } else {
                new_lenlen = 5;
            }
            new_grosslen = new_lenlen + new_netlen;
            p_vtpl1 = SsMemRealloc(p_vtpl1, new_grosslen);
            if (new_lenlen > old_lenlen) {
                ss_dassert(new_lenlen == 5 && old_lenlen == 1);
                memmove((ss_byte_t*)p_vtpl1 + new_lenlen,
                        (ss_byte_t*)p_vtpl1 + old_lenlen,
                        old_netlen);
            }
            va_setdata((va_t*)(ss_byte_t*)p_vtpl1, NULL, new_netlen);
            memcpy((ss_byte_t*)p_vtpl1 + new_lenlen + old_netlen,
                   dynva, new_netlen_increase - 1);
            ((ss_byte_t *) p_vtpl1 + new_lenlen + old_netlen)[new_netlen_increase - 1] = 0;
            BUFVTPL_SET_DYNAMIC_VTPL(buf, p_vtpl1);
        } else {
            /* previous value did fit into buffer */
            new_netlen += old_netlen;
            if (new_netlen >= bufsize) {
                /* new value won't fit */
                if (BYTE_LEN_TEST(new_netlen)) {
                    new_lenlen = 1;
                } else {
                    new_lenlen = 5;
                }
                new_grosslen = new_netlen + new_lenlen;
                p_vtpl1 = SsMemAlloc(new_grosslen);
                va_setdata((va_t*)(ss_byte_t*)p_vtpl1,
                           NULL,
                           (va_index_t)new_netlen);
                memcpy((ss_byte_t*)p_vtpl1 + new_lenlen,
                       buf + 1,
                       old_netlen);
                memcpy((ss_byte_t*)p_vtpl1 + new_lenlen + old_netlen,
                       dynva, new_netlen_increase - 1);
                ((ss_byte_t *) p_vtpl1 + new_lenlen + old_netlen)[new_netlen_increase - 1] = 0;
                BUFVTPL_SET_DYNAMIC_VTPL(buf, p_vtpl1);
            } else {
                /* (hopefylly) the most common case where no allocation
                 * is needed. New value fits into buffer.
                 */
                memcpy(buf + 1 + old_netlen,
                       dynva,
                       new_netlen_increase - 1);
                ((ss_byte_t *) buf + 1 + old_netlen)[new_netlen_increase - 1] = 0;
                buf[0] = (ss_byte_t)new_netlen;
            }
        }
        if (dynva != va) {
            dynva_free(&dynva);
        } else {
            /* Restore the original length field to va */
            va_setdata(dynva, NULL, va_datalen);
        }
}

void bufvtpl_appdata_as_va(ss_byte_t* buf, size_t bufsize,
                           void* data, size_t datalen)
{
        size_t new_netlen;
        size_t new_grosslen;
        size_t new_netlen_increase;
        size_t old_netlen;
        size_t old_lenlen;
        size_t new_lenlen;
        vtpl_t* p_vtpl1;

        BUFVA_CHKBUFSIZE(bufsize);
        new_netlen_increase =
            datalen +
            (BYTE_LEN_TEST(datalen)? 1 : 5);
        new_netlen = new_netlen_increase;
        old_netlen = BUFVTPL_1BYTENETLEN(buf);
        if (old_netlen >= bufsize) {
            /* current buffer contents has dynamic vtpl pointer */
            p_vtpl1 = BUFVTPL_GET_DYNAMIC_VTPL(buf);
            old_netlen = VA_NETLEN((va_t*)(ss_byte_t*)p_vtpl1);
            if (BYTE_LEN_TEST(old_netlen)) {
                old_lenlen = 1;
            } else {
                old_lenlen = 5;
            }
            new_netlen += old_netlen;
            if (BYTE_LEN_TEST(new_netlen)) {
                new_lenlen = 1;
            } else {
                new_lenlen = 5;
            }
            new_grosslen = new_lenlen + new_netlen;
            p_vtpl1 = SsMemRealloc(p_vtpl1, new_grosslen);
            if (new_lenlen > old_lenlen) {
                ss_dassert(new_lenlen == 5 && old_lenlen == 1);
                memmove((ss_byte_t*)p_vtpl1 + new_lenlen,
                        (ss_byte_t*)p_vtpl1 + old_lenlen,
                        old_netlen);
            }
            /* the below va_setdata call only sets the length field! */
            va_setdata((va_t*)(ss_byte_t*)p_vtpl1, NULL, new_netlen);
            va_setdata((va_t*)((ss_byte_t*)p_vtpl1 + new_lenlen + old_netlen),
                       data, (va_index_t)datalen);
            BUFVTPL_SET_DYNAMIC_VTPL(buf, p_vtpl1);
        } else {
            new_netlen += old_netlen;
            if (new_netlen >= bufsize) {
                /* new value won't fit */
                if (BYTE_LEN_TEST(new_netlen)) {
                    new_lenlen = 1;
                } else {
                    new_lenlen = 5;
                }
                new_grosslen = new_netlen + new_lenlen;
                p_vtpl1 = SsMemAlloc(new_grosslen);
                va_setdata((va_t*)(ss_byte_t*)p_vtpl1,
                           NULL,
                           (va_index_t)new_netlen);
                memcpy((ss_byte_t*)p_vtpl1 + new_lenlen,
                       buf + 1,
                       old_netlen);
                va_setdata((va_t*)
                           ((ss_byte_t*)p_vtpl1 + new_lenlen + old_netlen),
                           data, (va_index_t)datalen);
                BUFVTPL_SET_DYNAMIC_VTPL(buf, p_vtpl1);
            } else {
                /* (hopefully) the most common case! */
                va_setdata((va_t*)(buf + 1 + old_netlen),
                           data,
                           datalen);
                buf[0] = (ss_byte_t)new_netlen;
            }
        }
}

#if defined(MMEG2_MUTEXING)

void bufvtpl_mme_appva(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        va_t* va)
{
        size_t new_netlen;
        size_t new_grosslen;
        size_t new_lenlen;
        size_t old_netlen;
        size_t old_lenlen;
        size_t new_netlen_increase;
        vtpl_t* p_vtpl1;
        
        BUFVA_CHKBUFSIZE(bufsize);
        new_netlen_increase = VA_GROSSLEN(va);
        new_netlen = new_netlen_increase;
        old_netlen = BUFVTPL_1BYTENETLEN(buf);
        if (old_netlen >= bufsize) {
            p_vtpl1 = BUFVTPL_GET_DYNAMIC_VTPL(buf);
            old_netlen = VA_NETLEN((va_t*)(ss_byte_t*)p_vtpl1);
            if (BYTE_LEN_TEST(old_netlen)) {
                old_lenlen = 1;
            } else {
                old_lenlen = 5;
            }
            new_netlen += old_netlen;
            if (BYTE_LEN_TEST(new_netlen)) {
                new_lenlen = 1;
            } else {
                new_lenlen = 5;
            }
            new_grosslen = new_lenlen + new_netlen;
            p_vtpl1 = BUFVA_MME_REALLOC(memctx, p_vtpl1, new_grosslen);
            if (new_lenlen > old_lenlen) {
                ss_dassert(new_lenlen == 5 && old_lenlen == 1);
                memmove((ss_byte_t*)p_vtpl1 + new_lenlen,
                        (ss_byte_t*)p_vtpl1 + old_lenlen,
                        old_netlen);
            }
            va_setdata((va_t*)(ss_byte_t*)p_vtpl1, NULL, new_netlen);
            memcpy((ss_byte_t*)p_vtpl1 + new_lenlen + old_netlen,
                   va, new_netlen_increase);
            BUFVTPL_SET_DYNAMIC_VTPL(buf, p_vtpl1);
        } else {
            /* previous value did fit into buffer */
            new_netlen += old_netlen;
            if (new_netlen >= bufsize) {
                /* new value won't fit */
                if (BYTE_LEN_TEST(new_netlen)) {
                    new_lenlen = 1;
                } else {
                    new_lenlen = 5;
                }
                new_grosslen = new_netlen + new_lenlen;
                p_vtpl1 = BUFVA_MME_ALLOC(memctx, new_grosslen);
                va_setdata((va_t*)(ss_byte_t*)p_vtpl1,
                           NULL,
                           (va_index_t)new_netlen);
                memcpy((ss_byte_t*)p_vtpl1 + new_lenlen,
                       buf + 1,
                       old_netlen);
                memcpy((ss_byte_t*)p_vtpl1 + new_lenlen + old_netlen,
                       va, new_netlen_increase);
                BUFVTPL_SET_DYNAMIC_VTPL(buf, p_vtpl1);
            } else {
                /* (hopefylly) the most common case where no allocation
                 * is needed. New value fits into buffer.
                 */
                memcpy(buf + 1 + old_netlen,
                       va,
                       new_netlen_increase);
                buf[0] = (ss_byte_t)new_netlen;
            }
        }
}

void bufvtpl_mme_appdata_as_va(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        void* data, size_t datalen)
{
        size_t new_netlen;
        size_t new_grosslen;
        size_t new_netlen_increase;
        size_t old_netlen;
        size_t old_lenlen;
        size_t new_lenlen;
        vtpl_t* p_vtpl1;

        BUFVA_CHKBUFSIZE(bufsize);
        new_netlen_increase =
            datalen +
            (BYTE_LEN_TEST(datalen)? 1 : 5);
        new_netlen = new_netlen_increase;
        old_netlen = BUFVTPL_1BYTENETLEN(buf);
        if (old_netlen >= bufsize) {
            /* current buffer contents has dynamic vtpl pointer */
            p_vtpl1 = BUFVTPL_GET_DYNAMIC_VTPL(buf);
            old_netlen = VA_NETLEN((va_t*)(ss_byte_t*)p_vtpl1);
            if (BYTE_LEN_TEST(old_netlen)) {
                old_lenlen = 1;
            } else {
                old_lenlen = 5;
            }
            new_netlen += old_netlen;
            if (BYTE_LEN_TEST(new_netlen)) {
                new_lenlen = 1;
            } else {
                new_lenlen = 5;
            }
            new_grosslen = new_lenlen + new_netlen;
            p_vtpl1 = BUFVA_MME_REALLOC(memctx, p_vtpl1, new_grosslen);
            if (new_lenlen > old_lenlen) {
                ss_dassert(new_lenlen == 5 && old_lenlen == 1);
                memmove((ss_byte_t*)p_vtpl1 + new_lenlen,
                        (ss_byte_t*)p_vtpl1 + old_lenlen,
                        old_netlen);
            }
            /* the below va_setdata call only sets the length field! */
            va_setdata((va_t*)(ss_byte_t*)p_vtpl1, NULL, new_netlen);
            va_setdata((va_t*)((ss_byte_t*)p_vtpl1 + new_lenlen + old_netlen),
                       data, (va_index_t)datalen);
            BUFVTPL_SET_DYNAMIC_VTPL(buf, p_vtpl1);
        } else {
            new_netlen += old_netlen;
            if (new_netlen >= bufsize) {
                /* new value won't fit */
                if (BYTE_LEN_TEST(new_netlen)) {
                    new_lenlen = 1;
                } else {
                    new_lenlen = 5;
                }
                new_grosslen = new_netlen + new_lenlen;
                p_vtpl1 = BUFVA_MME_ALLOC(memctx, new_grosslen);
                va_setdata((va_t*)(ss_byte_t*)p_vtpl1,
                           NULL,
                           (va_index_t)new_netlen);
                memcpy((ss_byte_t*)p_vtpl1 + new_lenlen,
                       buf + 1,
                       old_netlen);
                va_setdata((va_t*)
                           ((ss_byte_t*)p_vtpl1 + new_lenlen + old_netlen),
                           data, (va_index_t)datalen);
                BUFVTPL_SET_DYNAMIC_VTPL(buf, p_vtpl1);
            } else {
                /* (hopefully) the most common case! */
                va_setdata((va_t*)(buf + 1 + old_netlen),
                           data,
                           datalen);
                buf[0] = (ss_byte_t)new_netlen;
            }
        }
}

#if 0 /* can be enabled later as needed */

va_t* bufva_mme_allocva(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        size_t new_grosslen)
{
        bool old_dynamic;
        va_t* p_va1;
        va_t* p_va2;
        size_t netlen;
        size_t old_grosslen;
        
        BUFVA_CHKBUFSIZE(bufsize);
        ss_dassert(new_grosslen >= 1);
 restart:;
        netlen = new_grosslen - 1;
        if (!BYTE_LEN_TEST(netlen)) {
            netlen = new_grosslen - 5;
        }
        old_grosslen = BUFVA_1BYTEGROSSLEN(buf);
        if (old_grosslen > bufsize) {
            /* current buffer contents has dynamic va pointer */
            old_dynamic = TRUE;
            p_va1 = BUFVA_GET_DYNAMIC_VA(buf);
            old_grosslen = VA_GROSS_LEN(p_va1);
            if (old_grosslen > new_grosslen) {
                new_grosslen = old_grosslen;
                goto restart;
            }
            if (new_grosslen > bufsize) {
                goto new_dynamic;
            }
        } else {
            old_dynamic = FALSE;
            p_va1 = (va_t*)buf;
            if (new_grosslen > bufsize) {
                goto new_dynamic;
            }
        }
        if (old_dynamic) {
            memcpy(buf, p_va1, old_grosslen);
            BUFVA_MME_FREE(memctx, p_va1);
        } else {
            ss_dassert(buf == (ss_byte_t*)p_va1);
        }
        return ((va_t*)buf);
new_dynamic:;
        if (old_dynamic) {
            p_va2 = BUFVA_MME_REALLOC(memctx, p_va1, new_grosslen);
        } else {
            p_va2 = BUFVA_MME_ALLOC(memctx, new_grosslen);
            ss_dassert(buf == (ss_byte_t*)p_va1);
            memcpy(p_va2, p_va1, old_grosslen);
        }
        BUFVA_SET_DYNAMIC_VA(buf, p_va2);
        return (p_va2);
}

void bufva_mme_setdata(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        void* data, size_t datalen)
{
        size_t new_grosslen;
        va_t* p_va1;
        
        BUFVA_CHKBUFSIZE(bufsize);
        if (datalen == 0) {
            data = (void*)VA_NULL;
        }
        new_grosslen = datalen + 1;
        if (BUFVA_1BYTEGROSSLEN(buf) > bufsize) {
            /* current buffer contents has dynamic va pointer */
            p_va1 = BUFVA_GET_DYNAMIC_VA(buf);
            if (new_grosslen > bufsize) {
                if (!BYTE_LEN_TEST(datalen)) {
                    new_grosslen = datalen + VA_LENGHTMAXLEN;
                }
                p_va1 = BUFVA_MME_REALLOC(memctx, p_va1, new_grosslen);
                va_setdata(p_va1, data, (va_index_t)datalen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                BUFVA_MME_FREE(memctx, p_va1);
                va_setdata((va_t*)buf, data, (va_index_t)datalen);
            }
        } else {
            if (new_grosslen > bufsize) {
                if (!BYTE_LEN_TEST(datalen)) {
                    new_grosslen = datalen + VA_LENGHTMAXLEN;
                }
                p_va1 = BUFVA_MME_ALLOC(memctx, new_grosslen); 
                va_setdata(p_va1, data, (va_index_t)datalen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                va_setdata((va_t*)buf, data, (va_index_t)datalen);
            }
        }
}


void bufva_mme_appdata(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        void* data, size_t datalen)
{
        size_t new_netlen;
        size_t new_grosslen;
        size_t old_netlen;
        size_t old_lenlen;
        size_t new_lenlen;
        va_t* p_va1;

        BUFVA_CHKBUFSIZE(bufsize);
        new_netlen = datalen;
        old_netlen = BUFVA_1BYTENETLEN(buf);
        if (old_netlen >= bufsize) {
            /* current buffer contents has dynamic va pointer */
            p_va1 = BUFVA_GET_DYNAMIC_VA(buf);
            old_netlen = VA_NETLEN(p_va1);
            if (BYTE_LEN_TEST(old_netlen)) {
                old_lenlen = 1;
            } else {
                old_lenlen = 5;
            }
            new_netlen += old_netlen;
            if (BYTE_LEN_TEST(new_netlen)) {
                new_lenlen = 1;
            } else {
                new_lenlen = 5;
            }
            new_grosslen = new_lenlen + new_netlen;
            p_va1 = BUFVA_MME_REALLOC(memctx, p_va1, new_grosslen);
            if (new_lenlen > old_lenlen) {
                ss_dassert(new_lenlen == 5 && old_lenlen == 1);
                memmove((ss_byte_t*)p_va1 + new_lenlen,
                        (ss_byte_t*)p_va1 + old_lenlen,
                        old_netlen);
            }
            /* the below va_setdata call only sets the length field! */
            va_setdata((va_t*)(ss_byte_t*)p_va1, NULL, new_netlen);
            memcpy((ss_byte_t*)p_va1 + new_lenlen + old_netlen,
                   data,
                   datalen);
            BUFVA_SET_DYNAMIC_VA(buf, p_va1);
        } else {
            /* note: old_lenlen is not used, but literal 1 */
            new_netlen += old_netlen;
            if (new_netlen >= bufsize) {
                /* new value won't fit */
                if (BYTE_LEN_TEST(new_netlen)) {
                    new_lenlen = 1;
                } else {
                    new_lenlen = 5;
                }
                new_grosslen = new_netlen + new_lenlen;
                p_va1 = BUFVA_MME_ALLOC(memctx, new_grosslen);
                va_setdata(p_va1,
                           NULL,
                           (va_index_t)new_netlen);
                memcpy((ss_byte_t*)p_va1 + new_lenlen,
                       buf + 1,
                       old_netlen);
                memcpy((ss_byte_t*)p_va1 + new_lenlen + old_netlen,
                       data, datalen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                /* (hopefully) the most common case! */
                memcpy(buf + 1 + old_netlen,
                       data,
                       datalen);
                buf[0] = (ss_byte_t)new_netlen;
            }
        }
}

void bufva_mme_appva(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        va_t* va)
{
        void* data;
        va_index_t datalen;

        VA_GETDATA(data, va, datalen);
        bufva_mme_appdata(memctx, buf, bufsize, data, datalen);
}

/* buffered v-tuple begin */






/*##**********************************************************************\
 *
 *      bufvtpl_mme_allocvtpl
 *
 * Allocates a vtuple to be used for a write buffer for vtuple value
 * given later.
 *
 * Parameters:
 *      memctx - in, use
 *
 *      buf - in out, use
 *          vtuple buffer
 *
 *      bufsize - in
 *          buffer size
 *
 *      new_grosslen - in
 *          grosslen for new vtuple
 *
 * Return value - ref:
 *      pointer to vtuple buffer whose length is as requested but
 *      net contents is uninitialized.
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef bufvtpl_mme_allocvtpl
vtpl_t* bufvtpl_mme_allocvtpl(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        size_t new_grosslen)
{
        return ((vtpl_t*)(ss_byte_t*)
                bufva_mme_allocva(memctx, buf, bufsize, new_grosslen));
}
#endif /* !bufvtpl_allocvtpl */

#endif /* 0 */
#endif /* MMEG2_MUTEXING */
