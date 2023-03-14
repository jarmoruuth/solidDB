/*************************************************************************\
**  source       * uti0bufva.h
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

#ifndef UTI0BUFVA_H
#define UTI0BUFVA_H

#include "uti0vtpl.h"

void bufva_init(ss_byte_t* buf, size_t bufsize);
SS_INLINE void bufva_done(ss_byte_t* buf, size_t bufsize);
void bufva_move(ss_byte_t* destbuf, size_t destbufsize,
                ss_byte_t* srcbuf, size_t srcbufsize);
SS_INLINE void bufva_setva(ss_byte_t* buf,
                 size_t bufsize,
                 va_t* va);
void bufva_setdata(ss_byte_t* buf, size_t bufsize,
                   void* data, size_t datalen);

void bufva_appdata(ss_byte_t* buf, size_t bufsize,
                   void* data, size_t datalen);
void bufva_appva(ss_byte_t* buf, size_t bufsize,
                 va_t* va);
SS_INLINE va_t* bufva_allocva(ss_byte_t* buf,
                    size_t bufsize,
                    size_t new_grosslen);

SS_INLINE va_t* bufva_getva(ss_byte_t* buf, size_t bufsize);

#define BUFVA_MINBUFSIZE (2 * sizeof(void*))
#define BUFVA_MAXBUFSIZE (VA_LONGVALIMIT - 1)

#define BUFVA_CHKBUFSIZE(bufsize) \
        ss_rc_dassert(BUFVA_MINBUFSIZE <= (bufsize) &&\
                      (bufsize) <= BUFVA_MAXBUFSIZE,\
                      (int)(bufsize))\

#ifndef SS_DEBUG

/* Trivial init is a macro in product compilation **/
#define bufva_init(buf, bufsize) \
        do { ((ss_byte_t*)buf)[0] = (ss_byte_t)0; } while (FALSE)

#endif /* !SS_DEBUG */


void bufvtpl_init(ss_byte_t* buf, size_t bufsize);
void bufvtpl_done(ss_byte_t* buf, size_t bufsize);
void bufvtpl_move(ss_byte_t* destbuf, size_t destbufsize,
                  ss_byte_t* srcbuf, size_t srcbufsize);
void bufvtpl_setvtpl(ss_byte_t* buf,
                     size_t bufsize,
                     vtpl_t* vtpl);
void bufvtpl_setdata(ss_byte_t* buf,
                     size_t bufsize,
                     void* data,
                     size_t datalen);

vtpl_t* bufvtpl_allocvtpl(ss_byte_t* buf,
                          size_t bufsize,
                          size_t new_grosslen);

#ifndef SS_DEBUG
#define bufvtpl_getvtpl(buf, bufsize)  _BUFVTPL_GETVTPL_(buf, bufsize)
#else
vtpl_t* bufvtpl_getvtpl(ss_byte_t* buf, size_t bufsize);
#endif

SS_INLINE void bufvtpl_appva(ss_byte_t* buf,
                   size_t bufsize,
                   va_t* va);
void bufvtpl_appvawithincrement(ss_byte_t* buf,
                                size_t bufsize,
                                va_t* va);
void bufvtpl_appdata_as_va(ss_byte_t* buf, size_t bufsize,
                           void* data, size_t datalen);

#ifndef SS_DEBUG

/* Trivial init is a macro in product compilation **/
#define bufvtpl_init(buf, bufsize) \
        bufva_init(buf, bufsize)

#define bufvtpl_done(buf, bufsize) \
        bufva_done(buf, bufsize)

#define bufvtpl_move(db, dbs, sb, sbs) \
        bufva_move(db, dbs, sb, sbs)

#define bufvtpl_setvtpl(buf, bufsize, vtpl) \
        bufva_setva(buf, bufsize, (va_t*)(ss_byte_t*)(vtpl))

#define bufvtpl_setdata(buf, bufsize, data, datalen) \
        bufva_setdata(buf, bufsize, data, datalen)

#define  bufvtpl_allocvtpl(buf, bufsize, new_grosslen) \
        ((vtpl_t*)(ss_byte_t*)bufva_allocva(buf, bufsize, new_grosslen))


#endif /* SS_DEBUG */

#define BUFVA_1BYTENETLEN(buf)      (((ss_byte_t*)buf)[0])
#define BUFVA_1BYTEGROSSLEN(buf)    (BUFVA_1BYTENETLEN(buf) + 1)

#define BUFVA_GET_DYNAMIC_VA(buf) SS_PTR_LOAD(&(((va_t**)(buf))[1]))

#define BUFVA_SET_DYNAMIC_VA(buf, va1) \
        do {\
            (buf)[0] = VA_LONGVALIMIT - 1;\
            SS_PTR_STORE(&(((va_t**)(buf))[1]),(va1));\
        } while (FALSE)

#define BUFVTPL_SET_DYNAMIC_VTPL(buf, vtpl1) \
        BUFVA_SET_DYNAMIC_VA((buf), (va_t*)(ss_byte_t*)(vtpl1))
#define BUFVTPL_GET_DYNAMIC_VTPL(buf) ((vtpl_t*)BUFVA_GET_DYNAMIC_VA(buf))
#define BUFVTPL_1BYTEGROSSLEN(buf)  BUFVA_1BYTEGROSSLEN(buf)
#define BUFVTPL_1BYTENETLEN(buf)    BUFVA_1BYTENETLEN(buf)


#define _BUFVTPL_GETVTPL_(buf, bufsize) \
        ((BUFVTPL_1BYTEGROSSLEN(buf) == 1) ? NULL \
         : (BUFVTPL_1BYTEGROSSLEN(buf) > (bufsize)) ? BUFVTPL_GET_DYNAMIC_VTPL(buf) \
         : (vtpl_t *) (buf))

#if defined(UTI0BUFVA_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *      bufva_done
 *
 * Resets v-attribute buffer to init value (and releases possible
 * dynamically allocated buffer)
 *
 * Parameters:
 *      buf - in out, use
 *          v-attribute buffer
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
SS_INLINE void bufva_done(ss_byte_t* buf, size_t bufsize)
{
        BUFVA_CHKBUFSIZE(bufsize);
        if ((size_t)(BUFVA_1BYTEGROSSLEN(buf)) > bufsize) {
            SsMemFree(BUFVA_GET_DYNAMIC_VA(buf));
        }
        buf[0] = (ss_byte_t)0;
}

/*##**********************************************************************\
 *
 *      bufva_setva
 *
 * Sets a v-attribute value into buffered v-attribute.
 * In case va value is bigger than buffer, automatically allocates
 * new storage and sets a pointer into buffer
 *
 * Parameters:
 *      buf - in out, use
 *          v-attribute buffer
 *
 *      bufsize - in
 *          buffer size in bytes
 *
 *      va - in, use
 *          pointer to v-attribute value
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
SS_INLINE void bufva_setva(ss_byte_t* buf, size_t bufsize,
                 va_t* va)
{
        size_t new_grosslen;
        va_t* p_va1;
        
        BUFVA_CHKBUFSIZE(bufsize);
        if (va == NULL) {
            va = (va_t*)(char*)VA_NULL;
            new_grosslen = 1;
            ss_dassert(VA_GROSSLEN((va_t*)va) == 1);
        } else {
            new_grosslen = VA_GROSSLEN((va_t*)va);
        }
        if ((size_t)(BUFVA_1BYTEGROSSLEN(buf)) > bufsize) {
            /* current buffer contents has dynamic va pointer */
            p_va1 = (va_t*) BUFVA_GET_DYNAMIC_VA((va_t*)buf);
            if (new_grosslen > bufsize) {
                p_va1 = (va_t*)SsMemRealloc(p_va1, new_grosslen);
                memcpy(p_va1, va, new_grosslen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                SsMemFree(p_va1);
                memcpy(buf, va, new_grosslen);
            }
        } else {
            if (new_grosslen > bufsize) {
                p_va1 = (va_t*)SsMemAlloc(new_grosslen);
                memcpy(p_va1, va, new_grosslen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                memcpy(buf, va, new_grosslen);
            }
        }
}

SS_INLINE va_t* bufva_allocva(
        ss_byte_t*  buf,
        size_t      bufsize,
        size_t      new_grosslen)
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
            p_va1 = (va_t*) BUFVA_GET_DYNAMIC_VA((va_t*)buf);
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
            SsMemFree(p_va1);
        } else {
            ss_dassert(buf == (ss_byte_t*)p_va1);
        }
        return ((va_t*)buf);
new_dynamic:;
        if (old_dynamic) {
            p_va2 = (va_t*)SsMemRealloc(p_va1, new_grosslen);
        } else {
            p_va2 = (va_t*)SsMemAlloc(new_grosslen);
            ss_dassert(buf == (ss_byte_t*)p_va1);
            memcpy(p_va2, p_va1, old_grosslen);
        }
        BUFVA_SET_DYNAMIC_VA(buf, p_va2);
        return (p_va2);
}

/*##**********************************************************************\
 *
 *      bufva_getva
 *
 * Gets a pointer to v-attribute value either contained in the buffer
 * or pointed to by pointer in the buffer.
 *
 * Parameters:
 *      buf - in, use
 *          v-attribute buffer
 *
 *      bufsize - in
 *          buffer size in bytes
 *
 * Return value - ref, read-only
 *      pointer to v-attribute value
 *
 * Limitations:
 *
 * Globals used:
 */
SS_INLINE va_t* bufva_getva(ss_byte_t* buf, size_t bufsize)
{
        size_t grosslen;

        BUFVA_CHKBUFSIZE(bufsize);
        grosslen = BUFVA_1BYTEGROSSLEN(buf);

        if (grosslen > bufsize) {
            return ((va_t*)BUFVA_GET_DYNAMIC_VA((va_t *)buf));
        }
        return ((va_t*)buf);
}

SS_INLINE void bufvtpl_appva(ss_byte_t* buf, size_t bufsize,
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
            p_vtpl1 = (vtpl_t *) BUFVTPL_GET_DYNAMIC_VTPL((vtpl_t *)buf);
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
            p_vtpl1 = (vtpl_t*)SsMemRealloc(p_vtpl1, new_grosslen);
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
                p_vtpl1 = (vtpl_t *)SsMemAlloc(new_grosslen);
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

#endif /* defined(UTI0BUFVA_C) || defined(SS_USE_INLINE) */

#ifdef MMEG2_MUTEXING

#include <ssffmem.h>

#define BUFVA_MME_ALLOC(memctx, size) \
    SsFFmemNonVersCountedObjAllocFor(memctx, SS_FFMEM_INDEXES, size)

#define BUFVA_MME_FREE(memctx, ptr) \
    SsFFmemNonVersCountedObjFreeFor(memctx, SS_FFMEM_INDEXES, ptr)

#define BUFVA_MME_REALLOC(memctx, ptr, newsize) \
    SsFFmemNonVersCountedObjReallocFor(memctx, SS_FFMEM_INDEXES, ptr, newsize)


SS_INLINE void bufva_mme_init(void *memctx, ss_byte_t* buf, size_t bufsize);
SS_INLINE void bufva_mme_done(void* memctx, ss_byte_t* buf, size_t bufsize);
SS_INLINE void bufva_mme_setva(void* memctx,
                               ss_byte_t* buf, size_t bufsize,
                               va_t* va);
SS_INLINE va_t* bufva_mme_getva(void* memctx, ss_byte_t* buf, size_t bufsize);
SS_INLINE void bufva_mme_move(
        void* memctx,
        ss_byte_t* destbuf, size_t destbufsize,
        ss_byte_t* srcbuf, size_t srcbufsize);


SS_INLINE void bufvtpl_mme_init(void* memctx, ss_byte_t* buf, size_t bufsize);
SS_INLINE void bufvtpl_mme_done(void* memctx, ss_byte_t* buf, size_t bufsize);
SS_INLINE void bufvtpl_mme_setvtpl(void* memctx,
                                   ss_byte_t* buf, size_t bufsize,
                                   vtpl_t* vtpl);
SS_INLINE vtpl_t* bufvtpl_mme_getvtpl(void* memctx,
                                      ss_byte_t* buf, size_t bufsize);
SS_INLINE void bufvtpl_mme_move(
        void* memctx,
        ss_byte_t* destbuf, size_t destbufsize,
        ss_byte_t* srcbuf, size_t srcbufsize);

void bufvtpl_mme_appva(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        va_t* va);
void bufvtpl_mme_appdata_as_va(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        void* data, size_t datalen);


#if defined(SS_USE_INLINE) || defined(UTI0BUFVA_C)

/*##**********************************************************************\
 *
 *      bufva_mme_init
 *
 * Initializes a buffer to contain empty v-attribute
 *
 * Parameters:
 *      memctx - in, use
 *
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
SS_INLINE void bufva_mme_init(void *memctx, ss_byte_t* buf, size_t bufsize)
{
        BUFVA_CHKBUFSIZE(bufsize);
        buf[0] = (ss_byte_t)0;
}

/*##**********************************************************************\
 *
 *      bufva_mme_done
 *
 * Resets v-attribute buffer to init value (and releases possible
 * dynamically allocated buffer)
 *
 * Parameters:
 *      memctx - in, use
 *
 *      buf - in out, use
 *          v-attribute buffer
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
SS_INLINE void bufva_mme_done(void* memctx, ss_byte_t* buf, size_t bufsize)
{
        BUFVA_CHKBUFSIZE(bufsize);
        if (BUFVA_1BYTEGROSSLEN(buf) > bufsize) {
            BUFVA_MME_FREE(memctx, BUFVA_GET_DYNAMIC_VA(buf));
        }
        buf[0] = (ss_byte_t)0;
}

/*##**********************************************************************\
 *
 *      bufva_mme_setva
 *
 * Sets a v-attribute value into buffered v-attribute.
 * In case va value is bigger than buffer, automatically allocates
 * new storage and sets a pointer into buffer
 *
 * Parameters:
 *      memctx - in, use
 *
 *      buf - in out, use
 *          v-attribute buffer
 *
 *      bufsize - in
 *          buffer size in bytes
 *
 *      va - in, use
 *          pointer to v-attribute value
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
SS_INLINE void bufva_mme_setva(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        va_t* va)
{
        size_t new_grosslen;
        va_t* p_va1;
        
        BUFVA_CHKBUFSIZE(bufsize);
        if (va == NULL) {
            va = (va_t*)(char*)VA_NULL;
            new_grosslen = 1;
            ss_dassert(VA_GROSSLEN((va_t*)va) == 1);
        } else {
            new_grosslen = VA_GROSSLEN((va_t*)va);
        }
        if (BUFVA_1BYTEGROSSLEN(buf) > bufsize) {
            /* current buffer contents has dynamic va pointer */
            p_va1 = BUFVA_GET_DYNAMIC_VA(buf);
            if (new_grosslen > bufsize) {
                p_va1 = BUFVA_MME_REALLOC(memctx, p_va1, new_grosslen);
                memcpy(p_va1, va, new_grosslen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                BUFVA_MME_FREE(memctx, p_va1);
                memcpy(buf, va, new_grosslen);
            }
        } else {
            if (new_grosslen > bufsize) {
                p_va1 = BUFVA_MME_ALLOC(memctx, new_grosslen);
                memcpy(p_va1, va, new_grosslen);
                BUFVA_SET_DYNAMIC_VA(buf, p_va1);
            } else {
                memcpy(buf, va, new_grosslen);
            }
        }
}

/*##**********************************************************************\
 *
 *      bufva_mme_getva
 *
 * Gets a pointer to v-attribute value either contained in the buffer
 * or pointed to by pointer in the buffer.
 *
 * Parameters:
 *      memctx - in, use
 *
 *      buf - in, use
 *          v-attribute buffer
 *
 *      bufsize - in
 *          buffer size in bytes
 *
 * Return value - ref, read-only
 *      pointer to v-attribute value
 *
 * Limitations:
 *
 * Globals used:
 */
SS_INLINE va_t* bufva_mme_getva(void* memctx, ss_byte_t* buf, size_t bufsize)
{
        size_t grosslen;

        BUFVA_CHKBUFSIZE(bufsize);
        grosslen = BUFVA_1BYTEGROSSLEN(buf);

        if (grosslen > bufsize) {
            return (BUFVA_GET_DYNAMIC_VA(buf));
        }
        return ((va_t*)buf);
}

SS_INLINE void bufva_mme_move(
        void* memctx,
        ss_byte_t* destbuf, size_t destbufsize,
        ss_byte_t* srcbuf, size_t srcbufsize)
{
        size_t ntocopy;
        
        ss_dassert(destbuf != srcbuf);
        if (BUFVA_1BYTEGROSSLEN(destbuf) > destbufsize) {
            BUFVA_MME_FREE(memctx, BUFVA_GET_DYNAMIC_VA(destbuf));
        }
        ntocopy = BUFVA_1BYTEGROSSLEN(srcbuf);
        if (ntocopy > srcbufsize) {
            /* dynamically allocated */
            va_t* p_va1 = BUFVA_GET_DYNAMIC_VA(srcbuf);
            BUFVA_SET_DYNAMIC_VA(destbuf, p_va1);
        } else {
            if (ntocopy > destbufsize) {
                bufva_mme_setva(memctx, destbuf, destbufsize, (va_t*)srcbuf);
            } else {
                memcpy(destbuf, srcbuf, ntocopy);
            }
        }
        srcbuf[0] = 0;
}

/*##**********************************************************************\
 *
 *      bufvtpl_mme_init
 *
 * Initializes a buffer to contain empty v-tuple
 *
 * Parameters:
 *      memctx - in, use
 *
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
SS_INLINE void bufvtpl_mme_init(void* memctx, ss_byte_t* buf, size_t bufsize)
{
        bufva_mme_init(memctx, buf, bufsize);
}

/*##**********************************************************************\
 *
 *      bufvtpl_mme_done
 *
 * Resets v-tuple buffer to init value (and releases possible
 * dynamically allocated buffer)
 *
 * Parameters:
 *      memctx - in, use
 *
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
SS_INLINE void bufvtpl_mme_done(void* memctx, ss_byte_t* buf, size_t bufsize)
{
        bufva_mme_done(memctx, buf, bufsize);
}

/*##**********************************************************************\
 *
 *      bufvtpl_mme_setvtpl
 *
 * Sets a v-tuple value into buffered v-tuple.
 * In case vtuple value is bigger than buffer, automatically allocates
 * new storage and sets a pointer into buffer
 *
 * Parameters:
 *      memctx - in, use
 *
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
SS_INLINE void bufvtpl_mme_setvtpl(
        void* memctx,
        ss_byte_t* buf, size_t bufsize,
        vtpl_t* vtpl)
{
        bufva_mme_setva(memctx, buf, bufsize, (va_t*)(ss_byte_t*)vtpl);
}

/*##**********************************************************************\
 *
 *      bufvtpl_mme_getvtpl
 *
 * Gets a pointer to v-tuple value either contained in the buffer
 * or pointed to by pointer in the buffer.
 *
 * Parameters:
 *      memctx - in, use
 *
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
SS_INLINE vtpl_t* bufvtpl_mme_getvtpl(
        void* memctx,
        ss_byte_t* buf, size_t bufsize)
{
        size_t grosslen;

        BUFVA_CHKBUFSIZE(bufsize);
        grosslen = BUFVTPL_1BYTEGROSSLEN(buf);
        if (grosslen == 1) {
            return (NULL);
        }
        if (grosslen > bufsize) {
            return (BUFVTPL_GET_DYNAMIC_VTPL(buf));
        }
        return ((vtpl_t*)buf);
}

/*##**********************************************************************\
 *
 *      bufvtpl_mme_move
 *
 * Moves value from srcbuf to destbuf and makes srcbuf empty.
 * Possible existing value in destbuf is freed.
 *
 * Parameters:
 *      memctx - in, use
 *
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
SS_INLINE void bufvtpl_mme_move(
        void* memctx,
        ss_byte_t* destbuf, size_t destbufsize,
        ss_byte_t* srcbuf, size_t srcbufsize)
{
        bufva_mme_move(memctx,
                       destbuf, destbufsize,
                       srcbuf, srcbufsize);
}


#endif /* SS_USE_INLINE || UTI0BUFVA_C */
#endif /* MMEG2_MUTEXING */
#endif /* UTI0BUFVA_H */
