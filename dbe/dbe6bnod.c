/*************************************************************************\
**  source       * dbe6bnod.c
**  directory    * dbe
**  description  * B+-tree node.
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

This module implements the B+-tree node. Each node consists of fixed
length node header and a variable length node data. The node data
is a sequence of key values. Key values expect the first key value
are compressed to the previous key value.

The node has the following format:

        +------------+
        | header     |
        +------------+
        | key values |
        +------------+

The node header has the following format:

        +-----------------------------+
        | disk block type             |
        +-----------------------------+
        | block checkpoint number     |
        +-----------------------------+
        | node length in bytes        |
        +-----------------------------+
        | key value count             |
        +-----------------------------+
        | number of insert at the end |
        +-----------------------------+
        | node level in the tree      |
        +-----------------------------+
        | node info byte              |
        +-----------------------------+

The node is split when the new key value does not fit into the node in
compressed format. The node is split from the key value that is in the
middle byte position in the node.

Limitations:
-----------


Error handling:
--------------

Errors are handled using asserts.

Objects used:
------------

split virtual file          su0svfil
free list of file blocks    dbe8flst
cache                       dbe8cach
key value system            dbe6bkey
key range search            dbe6bkrs

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------

Performance:
------------

todbcmt f 10 30

old     8153 7674 7764
new     - same results -

todbcmt -c1000 -n1000000 f 10 30

old     9679   9917  9706
new    10429  11204 10822   mismatch
new2   11314  11301 11234   extra level

Accelerator

todbcmtl -c10000000 -n1000000 f 10 30

old         13384 13611
mismatch    13867 14041(*)  *=f 10 60 30

**************************************************************************
#endif /* DOCUMENTATION */

#define DBE6BNOD_C

#include <ssstdio.h>
#include <ssstring.h>
#include <sssetjmp.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sslimits.h>
#include <sssprint.h>
#include <ssthread.h>
#include <sstime.h>

#include <su0error.h>
#include <su0svfil.h>
#include <su0rand.h>

#include "dbe9type.h"
#include "dbe9bhdr.h"
#include "dbe8clst.h"
#include "dbe8flst.h"
#include "dbe8cach.h"
#include "dbe7trxi.h"
#include "dbe7trxb.h"
#include "dbe6gobj.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe6bkrs.h"
#include "dbe6bnod.h"
#include "dbe6finf.h"
#include "dbe6bmgr.h"
#include "dbe0erro.h"
#include "dbe0db.h"
#include "dbe0blobg2.h"

#ifdef SS_DEBUG
static char bnode_readwritectx[] = "Bnode readwrite";
static char bnode_readonlyctx[] = "x Bnode readonly";
#else
#define bnode_readwritectx  NULL
#define bnode_readonlyctx   NULL
#endif

typedef enum {
        BNODE_SPLIT_NO,
        BNODE_SPLIT_NO_CLEANUP,
        BNODE_SPLIT_YES
} bnode_splitst_t;

#define BNODE_REACHINFO_MAX 4

typedef struct {
        ulong       ri_bytes;
        ulong       ri_count;
        SsMutexT*   ri_mutex;
} dbe_bnode_reachinfo_t;

static dbe_bnode_reachinfo_t dbe_bnode_reachinfo[BNODE_REACHINFO_MAX];

/* JarmoP 310399 */
extern bool dbe_estrndnodesp;
extern bool dbe_estrndkeysp;

extern jmp_buf  ss_dbg_jmpbuf[SS_DBG_JMPBUF_MAX];
extern uint     ss_dbg_jmpbufpos;

bool        dbe_reportindex;
long        dbe_curkeyid;
dbe_bkey_t* dbe_curkey;
su_rbt_t*   dbe_keynameid_rbt;
dbe_keynameid_t* dbe_curkey_keynameid;
bool        dbe_bnode_usemismatcharray;

long dbe_bnode_totalnodelength;
long dbe_bnode_totalnodekeycount;
long dbe_bnode_totalnodecount;
long dbe_bnode_totalshortnodecount;

static su_rand_t rnd = 127773;

#ifndef SS_MYSQL
static char blocktype_errmsg[] = "illegal index block \
type %d, addr %ld, routine %s, reachmode %d";
#endif

# if 0
#define xx_output_begin(x)  if (0) {
#define xx_output_end       }
#define xx_output(x)
#define xx_output_1(x)
#define xx_output_2(x)
#define xx_output_3(x)
#define xx_output_4(x)
#define xx_dprintf(x)
#define xx_dprintf_1(x)
#define xx_dprintf_2(x)
#define xx_dprintf_3(x)
#define xx_dprintf_4(x)
#endif /* 0 */

#define bnode_getkeyoffset(n, i, offset) \
    if ((n)->n_count > 0) {\
        ss_byte_t* keysearchinfo_array; \
        ss_rc_dassert(i < (n)->n_count && i >= 0, i);\
        keysearchinfo_array = (n)->n_keysearchinfo_array + 4 * (i); \
        offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]); \
    } else {\
        offset = 0;\
    }

/* Change to unsigned */
static void bnode_setkeysearchinfo(
        unsigned char* keysearchinfo_array, 
        int index, 
        int mismatch, 
        int offset)
{
        ss_dprintf_3(("bnode_setkeysearchinfo:index=%d, mismatch=%d, offset=%d\n", index, mismatch, offset));
        ss_dassert(index <= 255);
        keysearchinfo_array[0] = (ss_byte_t)index;
        keysearchinfo_array[1] = (ss_byte_t)mismatch;
        SS_UINT2_STORETODISK(&(keysearchinfo_array)[2], offset);
}

static void bnode_getkeysearchinfo(
        ss_byte_t* keysearchinfo_array, 
        int* index, 
        char* mismatch)
{
        *index = keysearchinfo_array[0];
        *mismatch = keysearchinfo_array[1];
}

static void bnode_info_setmismatcharray(dbe_bnode_t* n, bool setp)
{
        if (setp && dbe_bnode_usemismatcharray) {
            ss_pprintf_1(("bnode_info_setmismatcharray:n->n_addr=%ld, set\n", n->n_addr));
            n->n_info |= BNODE_MISMATCHARRAY;
        } else {
            ss_pprintf_1(("bnode_info_setmismatcharray:n->n_addr=%ld, clear\n", n->n_addr));
            n->n_info &= ~BNODE_MISMATCHARRAY;
        }
        BNODE_SETINFO(n->n_p, n->n_info);
}

static bool bnode_keysearchinfo_init(dbe_bnode_t* n)
{
        char* keys;
        dbe_bkey_t* k;
        int count;
        uint index;
        uint offset;
        ss_byte_t mismatch;
        int i;
        char* data;
        va_index_t len;
        ss_byte_t* keysearchinfo_array;
        ss_byte_t* last_keysearchinfo_array;
        uint blocksize;

        ss_pprintf_1(("bnode_keysearchinfo_init:n->n_addr=%ld, debugoffset=%d\n", n->n_addr, BNODE_DEBUGOFFSET));
        ss_dassert((n->n_info & BNODE_MISMATCHARRAY) == 0);

        blocksize = (uint)n->n_go->go_idxfd->fd_blocksize;

        if (!dbe_bnode_usemismatcharray || (uint)(BNODE_HEADERLEN + BNODE_DEBUGOFFSET + n->n_len + n->n_count * 4) > blocksize) {
            ss_pprintf_1(("bnode_keysearchinfo_init:no space for mismatch array, n->n_len=%d, n->n_count=%d\n", n->n_len, n->n_count));
            return(FALSE);
        }

        keys = n->n_keys;
        if (BNODE_DEBUGOFFSET > 0) {
            keys += BNODE_DEBUGOFFSET;
            memmove(keys, n->n_keys, n->n_len);
            *n->n_keys = BKEY_ILLEGALINFO;
            n->n_len += BNODE_DEBUGOFFSET;
        }
        count = n->n_count;
        keysearchinfo_array = (ss_byte_t*)(n->n_p + blocksize - n->n_count * 4);
        last_keysearchinfo_array = keysearchinfo_array;

        for (i = 0; i < count; i++, keysearchinfo_array += 4) {
            vtpl_t* tmp_vtpl;
            va_t* tmp_va;

            k = (dbe_bkey_t*)keys;
            index = BKEY_LOADINDEX(k);
            if (index > 255) {
                index = 255;
            }
            offset = (uint)(keys - n->n_keys);
            ss_assert(offset < 64 * 1024);
            tmp_vtpl = BKEY_GETVTPLPTR(k);
            tmp_va = VTPL_GETVA_AT0(tmp_vtpl);
            data = va_getdata(tmp_va, &len);
            if (len != 0) {
                mismatch = *data;
                last_keysearchinfo_array = keysearchinfo_array;
            } else {
#if 1
                mismatch = 0;
#else
                /* For NULL field, use previous value. */
                ss_dassert(i > 0);
                index = last_keysearchinfo_array[0];
                mismatch = last_keysearchinfo_array[1];
#endif
            }
            bnode_setkeysearchinfo(keysearchinfo_array, index, mismatch, offset);
            keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
        }

        n->n_keysearchinfo_array = (ss_byte_t*) n->n_p + blocksize - n->n_count * 4;

        bnode_info_setmismatcharray(n, TRUE);

        ss_dassert(keysearchinfo_array == n->n_p + blocksize);
        ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0);

        n->n_dirty = TRUE;

        SS_PMON_ADD(SS_PMON_BNODE_BUILD_MISMATCH);

        return(TRUE);
}

static bool bnode_keysearchinfo_search(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        dbe_bkey_cmp_t cmptype,
        char** p_keys,
        int* p_pos,
        int* p_cmp,
        int* p_prevklen,
        dbe_bkey_search_t* p_ks)
{
        char* keys;
        int count;
        int skipped = 0;
        int compared = 0;
        uint index;
        ss_uint2_t offset;
        ss_byte_t mismatch;
        dbe_bkey_search_t ks;
        int cmp;
        int i;
        int prevklen = 0;
        uint blocksize;
        ss_byte_t* keysearchinfo_array;

        if ((n->n_info & BNODE_MISMATCHARRAY) == 0) {
            SS_PMON_ADD(SS_PMON_BNODE_SEARCH_KEYS);
            return(FALSE);
        }

        ss_dassert(n->n_count > 0);

        SS_PMON_ADD(SS_PMON_BNODE_SEARCH_MISMATCH);

        ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0);

        bnode_getkeyoffset(n, 0, offset);
        keys = n->n_keys + offset;
        count = n->n_count;

        dbe_bkey_search_init(&ks, k, cmptype);

        dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
        compared++;

        if (cmp <= 0) {
            /* Found. */
            *p_keys = keys;
            *p_pos = 0;
            *p_cmp = cmp;
            if (p_prevklen != NULL) {
                *p_prevklen = prevklen;
            }
            if (p_ks != NULL) {
                *p_ks = ks;
            }
            ss_pprintf_1(("bnode_keysearchinfo_search:return cmp %d, skipped %d, compared %d, pos %d count %d bonsai %d level %d\n", *p_cmp, skipped, compared, *p_pos, n->n_count, n->n_bonsaip, n->n_level));
            ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0);
            return(TRUE);
        }

        ss_dprintf_1(("bnode_keysearchinfo_search:ss i_mismatch pos %d, *p_mismatch byte %d\n", ks.ks_ss.i_mismatch, ZERO_EXTEND_TO_INT(*ks.ks_ss.p_mismatch)));

        blocksize = (uint)n->n_go->go_idxfd->fd_blocksize;
        keysearchinfo_array = n->n_keysearchinfo_array;

        /* Start from the second key. */
        keysearchinfo_array += 4;

        if (ks.ks_ss.l_field != 0) {
            /* If not NULL field. */
            for (i = 1; i < count; i++, keysearchinfo_array += 4) {
                ss_debug(ss_uint2_t tmp_offset;)
                index = keysearchinfo_array[0];
                ss_dassert(index <= 255);
#ifdef SS_DEBUG
                {
                    char* tmp_data;
                    va_index_t tmp_len;
                    dbe_bkey_t* tmp_k;
                    uint tmp_index;
                    ss_byte_t tmp_mismatch;

                    tmp_offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]);
                    ss_dassert(tmp_offset >= 0);
                    ss_dassert(tmp_offset < n->n_len);
                    tmp_k = (dbe_bkey_t*)(n->n_keys + tmp_offset);
                    tmp_data = va_getdata(VTPL_GETVA_AT0(BKEY_GETVTPLPTR(tmp_k)), &tmp_len);
                    if (tmp_len != 0) {
                        tmp_index = BKEY_LOADINDEX(tmp_k);
                        tmp_mismatch = *tmp_data;
                        ss_dassert((index == 255 && tmp_index >= 255) || (index == tmp_index));
                        ss_dassert(tmp_mismatch == keysearchinfo_array[1]);
                    }
                    ss_output_4(dbe_bkey_print_ex(NULL, "bnode_keysearchinfo_search:mismatch:", tmp_k));
                }
#endif
                if (index == 255) {
                    /* Get real index from data. */
                    offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]);
                    keys = n->n_keys + offset;
                    index = BKEY_LOADINDEX((dbe_bkey_t*)keys);
                    ss_assert(index >= 255);
                }
                ss_dprintf_4(("bnode_keysearchinfo_search:i %d, mismatch pos %d, mismatch byte %d, offset %d\n", i, index, (uint)keysearchinfo_array[1], tmp_offset));
                if (ks.ks_ss.i_mismatch > index) {
                    break;
                }
                if (ks.ks_ss.i_mismatch == index) {
                    mismatch = keysearchinfo_array[1];
                    if (ZERO_EXTEND_TO_INT(*ks.ks_ss.p_mismatch) < ZERO_EXTEND_TO_INT(mismatch)) {
                        ss_dprintf_1(("bnode_keysearchinfo_search:i %d, break, mismatch comparison\n", i));
                        break;
                    } else if (ZERO_EXTEND_TO_INT(*ks.ks_ss.p_mismatch) == ZERO_EXTEND_TO_INT(mismatch)) {
                        offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]);
                        keys = n->n_keys + offset;
                        dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                        compared++;
                        if (cmp <= 0) {
                            /* Found. */
                            *p_keys = keys;
                            *p_pos = i;
                            *p_cmp = cmp;
                            if (p_prevklen != NULL) {
                                ss_byte_t* prev_keysearchinfo_array;
                                prev_keysearchinfo_array = keysearchinfo_array - 4;
                                offset = SS_UINT2_LOADFROMDISK(&prev_keysearchinfo_array[2]);
                                keys = n->n_keys + offset;
                                *p_prevklen = dbe_bkey_getlength((dbe_bkey_t*)keys);
                            }
                            if (p_ks != NULL) {
                                *p_ks = ks;
                            }
                            ss_pprintf_1(("bnode_keysearchinfo_search:index == 0, return cmp %d, skipped %d, compared %d, pos %d count %d bonsai %d level %d\n", *p_cmp, skipped, compared, *p_pos, n->n_count, n->n_bonsaip, n->n_level));
                            ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0);
                            return(TRUE);
                        }
                        if (ks.ks_ss.l_field == 0) {
                            ss_dprintf_1(("bnode_keysearchinfo_search:i %d, break, NULL mismatch\n", i));
                            i++;
                            keysearchinfo_array += 4;
                            break;
                        }
                        ss_dprintf_4(("bnode_keysearchinfo_search:new ss i_mismatch pos %d, *p_mismatch byte %d\n", ks.ks_ss.i_mismatch, ZERO_EXTEND_TO_INT(*ks.ks_ss.p_mismatch)));
                    } else {
                        skipped++;
                    }
                } else {
                    skipped++;
                }
            }
            ss_dprintf_1(("bnode_keysearchinfo_search:end of skip loop, i %d\n", i));
        } else {
            i = 1;
            ss_dprintf_1(("bnode_keysearchinfo_search:NULL value, do not enter skip loop, i %d\n", i));
        }

        /* Move to previous key. */
        keysearchinfo_array = keysearchinfo_array - 4;
        offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]);
        keys = n->n_keys + offset;
        prevklen = dbe_bkey_getlength((dbe_bkey_t*)keys);

        if (i == count) {
            /* Not found. */
            ss_dprintf_1(("bnode_keysearchinfo_search:i == count\n"));
            *p_keys = n->n_keys + n->n_len;
            *p_pos = i;
            *p_cmp = 1;
            if (p_prevklen != NULL) {
                keysearchinfo_array = n->n_keysearchinfo_array + (n->n_count - 1) * 4;
                offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]);
                *p_prevklen = dbe_bkey_getlength((dbe_bkey_t*)&n->n_keys[offset]);
            }
            if (p_ks != NULL) {
                *p_ks = ks;
            }
            ss_pprintf_1(("bnode_keysearchinfo_search:i == count, return cmp %d, skipped %d, compared %d, pos %d count %d bonsai %d level %d\n", *p_cmp, skipped, compared, *p_pos, n->n_count, n->n_bonsaip, n->n_level));
            ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0);
            return(TRUE);
        }

        /* Move back to current key. */
        keysearchinfo_array = keysearchinfo_array + 4;
        offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]);
        keys = n->n_keys + offset;

        for (;;) {
            ss_dassert(keysearchinfo_array == n->n_keysearchinfo_array + i * 4);
            ss_dprintf_4(("bnode_keysearchinfo_search:search keys i %d\n", i));
            ss_output_4(dbe_bkey_print_ex(NULL, "bnode_keysearchinfo_search:cmploop:", (dbe_bkey_t*)keys));
            dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
            compared++;
            if (cmp <= 0) {
                break;
            }
            prevklen = dbe_bkey_getlength((dbe_bkey_t*)keys);
            i++;
            if (i == count) {
                break;
            }
            keysearchinfo_array = keysearchinfo_array + 4;
            offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]);
            keys = n->n_keys + offset;
        }
        *p_keys = keys;
        *p_pos = i;
        *p_cmp = cmp;
        if (p_prevklen != NULL) {
            *p_prevklen = prevklen;
        }
        if (p_ks != NULL) {
            *p_ks = ks;
        }
        ss_pprintf_1(("bnode_keysearchinfo_search:return cmp %d, skipped %d, compared %d, pos %d count %d bonsai %d level %d\n", *p_cmp, skipped, compared, *p_pos, n->n_count, n->n_bonsaip, n->n_level));
        ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0);
        return(TRUE);
}

#ifndef SS_LIGHT

/*##**********************************************************************\
 *
 *		dbe_bnode_test
 *
 * Tests that the node is consistent.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 * Return value :
 *
 *      TRUE - just to return something so that this can be used
 *             in asserts
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bnode_test(dbe_bnode_t* n)
{
        int i;
        int kpos;
        dbe_bkey_t* k = NULL;
        dbe_bkey_t* prevk = NULL;
        dbe_blocktype_t blocktype;
        int index;
        char mismatch;
        char* data;
        va_index_t len;
        dbe_bkey_t* pk;
        dbe_bkey_t* fk;

        ss_dprintf_3(("dbe_bnode_test:addr=%ld, count=%d\n", n->n_addr, n->n_count));
        CHK_BNODE(n);

        DBE_BLOCK_GETTYPE(n->n_p, &blocktype);
        ss_assert(blocktype == DBE_BLOCK_TREENODE);
        ss_ct_assert(DBE_BNODE_SIZE_ATLEAST <= sizeof(dbe_bnode_t));
        ss_dassert(n->n_level >= 0);
        ss_dassert(n->n_len + BNODE_HEADERLEN <= n->n_go->go_idxfd->fd_blocksize);

        pk = dbe_bkey_init(n->n_go->go_bkeyinfo);
        fk = dbe_bkey_init(n->n_go->go_bkeyinfo);

        kpos = 0;
        for (i = 0; i < n->n_count; i++) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, i, kpos);
            }
            prevk = k;
            k = (dbe_bkey_t*)&n->n_keys[kpos];
            ss_dassert(dbe_bkey_test(k));
            if (i > 0) {
                dbe_bkey_expand(fk, pk, (dbe_bkey_t*)&n->n_keys[kpos]);
            } else {
                dbe_bkey_copy(fk, k);
            }
            ss_dprintf_3(("dbe_bnode_test:i %d, offset %d\n", i, (char*)k - n->n_keys));
            ss_output_3(dbe_bkey_print_ex(NULL, "dbe_bnode_test:", fk));
            switch (n->n_level) {
                case 0:
                    ss_dassert(dbe_bkey_isleaf(k));
                    break;
                default:
                    ss_dassert(!dbe_bkey_isleaf(k));
                    break;
            }
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int tmp_index;
                data = va_getdata(VTPL_GETVA_AT0(BKEY_GETVTPLPTR(k)), &len);
                bnode_getkeysearchinfo(
                    n->n_keysearchinfo_array + 4 * i,
                    &index,
                    &mismatch);
                if (len != 0) {
                    tmp_index = BKEY_LOADINDEX(k);
                    if (tmp_index > 255) {
                        tmp_index = 255;
                    }
                    ss_rc_dassert(index == tmp_index, tmp_index);
                    ss_rc_dassert(mismatch == *data, *data);
                } else {
                    ss_dassert(i > 0);
#if 1
                    tmp_index = BKEY_LOADINDEX(k);
                    if (tmp_index > 255) {
                        tmp_index = 255;
                    }
                    ss_rc_dassert(index == tmp_index, tmp_index);
                    ss_rc_dassert(mismatch == 0, mismatch);
#else
                    data = va_getdata(VTPL_GETVA_AT0(BKEY_GETVTPLPTR(prevk)), &len);
                    ss_rc_dassert(index == BKEY_LOADINDEX(prevk), BKEY_LOADINDEX(prevk));
                    ss_rc_dassert(mismatch == *data, *data);
#endif
                }
            } else {
                kpos += dbe_bkey_getlength(k);
            }
            ss_dassert(kpos <= n->n_len);
            ss_rc_dassert(i == 0 || dbe_bkey_compare(pk, fk) < 0, i);
            dbe_bkey_copy(pk, fk);
        }
        if (!(n->n_info & BNODE_MISMATCHARRAY)) {
            ss_dassert(kpos == n->n_len);
        }
        dbe_bkey_done(pk);
        dbe_bkey_done(fk);
        return(TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_comparekeys
 *
 * Compares key data on two nodes.
 *
 * Parameters :
 *
 *	n1 - in, use
 *		node pointer
 *
 *	n2 - in, use
 *		node pointer
 *
 * Return value :
 *
 *      TRUE or FALSE
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bnode_comparekeys(dbe_bnode_t* n1, dbe_bnode_t* n2)
{
        int i;
        int kpos1;
        int kpos2;
        dbe_bkey_t* k1;
        dbe_bkey_t* k2;
        int len1;
        int len2;

        CHK_BNODE(n1);
        CHK_BNODE(n2);

        if (n1->n_count != n2->n_count) {
            return(FALSE);
        }

        kpos1 = 0;
        kpos2 = 0;
        for (i = 0; i < n1->n_count; i++) {
            if (n1->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n1, i, kpos1);
            }
            if (n2->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n2, i, kpos2);
            }
            k1 = (dbe_bkey_t*)&n1->n_keys[kpos1];
            k2 = (dbe_bkey_t*)&n2->n_keys[kpos2];
            len1 = dbe_bkey_getlength(k1);
            len2 = dbe_bkey_getlength(k2);
            if (len1 != len2) {
                return(FALSE);
            }
            if (memcmp(k1, k2, len1) != 0) {
                return(FALSE);
            }
            if (!(n1->n_info & BNODE_MISMATCHARRAY)) {
                kpos1 += dbe_bkey_getlength(k1);
            }
            ss_dassert(kpos1 <= n1->n_len);
            if (!(n2->n_info & BNODE_MISMATCHARRAY)) {
                kpos2 += dbe_bkey_getlength(k2);
            }
            ss_dassert(kpos2 <= n2->n_len);
        }
        return(TRUE);
}

#endif /* SS_LIGHT */

#ifdef SS_DEBUG
/*#**********************************************************************\
 *
 *		bnode_test_split
 *
 * Tests that the nodes are consistent after split.
 *
 * Parameters :
 *
 *	pn - in, use
 *		previous node pointer
 *
 *	nn - in, use
 *		next node pointer
 *
 * Return value :
 *
 *      TRUE - just to return something so that this can be used
 *             in asserts
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool bnode_test_split(
        dbe_bnode_t* pn,
        dbe_bnode_t* nn)
{
        int i;
        dbe_bkey_t* pk;
        dbe_bkey_t* k;
        dbe_bkey_t* pn_lastkey;
        dbe_bkey_t* nn_firstkey;
        int kpos;
        dbe_bnode_t* n;

        CHK_BNODE(pn);
        CHK_BNODE(nn);
        SS_PUSHNAME("bnode_test_split");

        pk = dbe_bkey_init(pn->n_go->go_bkeyinfo);
        k = dbe_bkey_init(pn->n_go->go_bkeyinfo);
        pn_lastkey = dbe_bkey_init(pn->n_go->go_bkeyinfo);
        nn_firstkey = dbe_bkey_init(pn->n_go->go_bkeyinfo);

        /* process pn */
        n = pn;
        if (n->n_info & BNODE_MISMATCHARRAY) {
            bnode_getkeyoffset(n, 0, kpos);
            dbe_bkey_copy(k, (dbe_bkey_t*)&n->n_keys[kpos]);
        } else {
            dbe_bkey_copy(k, (dbe_bkey_t*)n->n_keys);
        }
        dbe_bkey_copy(pk, k);
        kpos = 0;
        for (i = 1; i < n->n_count; i++) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, i, kpos);
            } else {
                kpos += dbe_bkey_getlength((dbe_bkey_t*)&n->n_keys[kpos]);
            }
            dbe_bkey_expand(k, k, (dbe_bkey_t*)&n->n_keys[kpos]);
            ss_dassert(dbe_bkey_compare(pk, k) < 0);
            dbe_bkey_copy(pk, k);
        }
        dbe_bkey_copy(pn_lastkey, k);

        /* process nn */
        n = nn;
        if (n->n_info & BNODE_MISMATCHARRAY) {
            bnode_getkeyoffset(n, 0, kpos);
            dbe_bkey_copy(k, (dbe_bkey_t*)&n->n_keys[kpos]);
        } else {
            dbe_bkey_copy(k, (dbe_bkey_t*)n->n_keys);
        }
        ss_dassert(dbe_bkey_compare(pk, k) < 0);
        dbe_bkey_copy(pk, k);
        dbe_bkey_copy(nn_firstkey, k);
        kpos = 0;
        for (i = 1; i < n->n_count; i++) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, i, kpos);
            } else {
                kpos += dbe_bkey_getlength((dbe_bkey_t*)&n->n_keys[kpos]);
            }
            dbe_bkey_expand(k, k, (dbe_bkey_t*)&n->n_keys[kpos]);
            ss_dassert(dbe_bkey_compare(pk, k) < 0);
            dbe_bkey_copy(pk, k);
        }

        if (dbe_cfg_singledeletemark) {
            ss_dassert(dbe_bkey_compare_vtpl(pn_lastkey, nn_firstkey) != 0);
        }

        dbe_bkey_done(pk);
        dbe_bkey_done(k);
        dbe_bkey_done(pn_lastkey);
        dbe_bkey_done(nn_firstkey);

        SS_POPNAME;

        return(TRUE);
}

#endif /* SS_DEBUG */

/*#***********************************************************************\
 * 
 *		bnode_create_empty
 * 
 * Creates an empty B-tree node.
 * 
 * Parameters : 
 * 
 *		go - 
 *			
 *			
 *		bonsaip - 
 *			
 *			
 *		cacheslot - 
 *			
 *			
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static dbe_bnode_t* bnode_create_empty(
        dbe_gobj_t* go,
        su_daddr_t addr,
        bool bonsaip,
        dbe_cacheslot_t* cacheslot,
        bool dirty)
{
        dbe_bnode_t* n;
        su_ret_t rc = SU_SUCCESS;
        dbe_blocktype_t blocktype;
        bool succp;
        TWO_BYTE_T two_byte;

        ss_dassert(bonsaip == TRUE || bonsaip == FALSE);
        ss_dprintf_1(("bnode_create_empty:addr = %ld\n"));

        n = SSMEM_NEW(dbe_bnode_t);

        n->n_cacheslot = cacheslot;
        n->n_p = dbe_cacheslot_getdata(cacheslot);
        ss_debug(n->n_chk = DBE_CHK_BNODE;)
        n->n_go = go;
        n->n_len = 0;
        n->n_count = 0;
        n->n_level = 0;
        n->n_addr = addr;
        n->n_dirty = dirty;
        n->n_lastuse = FALSE;
        n->n_bonsaip = bonsaip;
        n->n_seqinscount = 0;
        n->n_lastinsindex = 0;
        n->n_info = 0;
        n->n_cpnum = dbe_counter_getcpnum(go->go_ctr);
        n->n_keys = BNODE_GETKEYPTR(n->n_p);

        succp = dbe_cacheslot_setuserdata(n->n_cacheslot, n, n->n_level, n->n_bonsaip);
        ss_dassert(succp);

        blocktype = DBE_BLOCK_TREENODE;
        DBE_BLOCK_SETTYPE(n->n_p, &blocktype);
        DBE_BLOCK_SETCPNUM(n->n_p, &n->n_cpnum);
        if (!dirty) {
            two_byte = (TWO_BYTE_T)n->n_len;
            BNODE_STORELEN(n->n_p, two_byte);
            two_byte = (TWO_BYTE_T)n->n_count;
            BNODE_STORECOUNT(n->n_p, two_byte);
            BNODE_SETSEQINSCOUNT(n->n_p, n->n_seqinscount);
            BNODE_SETINFO(n->n_p, n->n_info);
            BNODE_SETLEVEL(n->n_p, n->n_level);
        }

        return(n);
}

/*#***********************************************************************\
 *
 *		bnode_create_seq
 *
 * Creates a new node onto cache using a sequence in address addr if
 * possible.
 *
 * Parameters :
 *
 *	go - in, hold
 *		global objects
 *
 *	prev_addr - in
 *		previous node address
 *
 *	p_rc - out
 *		error code if function returns NULL
 *
 * Return value - give :
 *
 *      node pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_bnode_t* bnode_create_seq(
        dbe_gobj_t* go,
        su_daddr_t prev_addr,
        bool bonsaip,
        dbe_ret_t* p_rc,
        dbe_info_t* info)
{
        dbe_bnode_t* n;
        su_daddr_t addr = SU_DADDR_NULL;
        su_ret_t rc = SU_SUCCESS;
        dbe_cacheslot_t* cacheslot;

        ss_dassert(bonsaip == TRUE || bonsaip == FALSE);

        if (prev_addr != SU_DADDR_NULL) {
            rc = dbe_fl_seq_alloc(go->go_idxfd->fd_freelist, prev_addr, &addr);
            if (rc != SU_SUCCESS) {
                /* Create a new sequence allocation. */
                rc = dbe_fl_seq_create(go->go_idxfd->fd_freelist, &addr, info);
            }
            if (rc != SU_SUCCESS) {
                addr = SU_DADDR_NULL;
            }
        }
        if (addr == SU_DADDR_NULL) {
            rc = dbe_fl_alloc(go->go_idxfd->fd_freelist, &addr, info);
        }
        ss_dprintf_1(("bnode_create_seq:addr = %ld, dbe_fl_alloc rc = %d\n", addr, rc));

        if (rc != SU_SUCCESS) {
            *p_rc = rc;
            return(NULL);
        }

        *p_rc = SU_SUCCESS;

        cacheslot = dbe_iomgr_reach(
                        go->go_iomgr,
                        addr,
                        DBE_CACHE_WRITEONLY,
                        info->i_flags,
                        NULL,
                        bnode_readwritectx);
        ss_dassert(!dbe_cacheslot_isoldversion(cacheslot));

        n = bnode_create_empty(go, addr, bonsaip, cacheslot, TRUE);

        return(n);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_create
 *
 * Creates a new node onto cache.
 *
 * Parameters :
 *
 *	go - in, hold
 *		global objects
 *
 * Return value - give :
 *
 *      node pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bnode_t* dbe_bnode_create(
        dbe_gobj_t* go,
        bool bonsaip,
        dbe_ret_t* p_rc,
        dbe_info_t* info)
{
        return(bnode_create_seq(go, SU_DADDR_NULL, bonsaip, p_rc, info));
}

/*#***********************************************************************\
 *
 *		bnode_initbyslot
 *
 *
 *
 * Parameters :
 *
 *	cacheslot -
 *
 *
 *	p -
 *
 *
 *	addr -
 *
 *
 *	go -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_bnode_t* bnode_initbyslot(
        dbe_cacheslot_t* cacheslot,
        char* p,
        su_daddr_t addr,
        bool bonsaip,
        dbe_gobj_t* go,
        size_t blocksize)
{
        dbe_bnode_t* n;
        dbe_blocktype_t blocktype;
        TWO_BYTE_T two_byte;
        bool succp;

        if (p == NULL) {
            p = dbe_cacheslot_getdata(cacheslot);
        }
        if (blocksize == 0) {
            ss_dassert(go != NULL);
            blocksize = go->go_idxfd->fd_blocksize;
        }

        n = SSMEM_NEW(dbe_bnode_t);

        ss_debug(n->n_chk = DBE_CHK_BNODE;)
        n->n_cacheslot = cacheslot;
        n->n_p = p;
        n->n_go = go;
        two_byte = BNODE_LOADLEN(n->n_p);
        n->n_len = two_byte;
        two_byte = BNODE_LOADCOUNT(n->n_p);
        n->n_count = two_byte;
        n->n_level = BNODE_GETLEVEL(n->n_p);
        n->n_addr = addr;
        n->n_dirty = FALSE;
        n->n_lastuse = FALSE;
        n->n_bonsaip = bonsaip;
        n->n_seqinscount = BNODE_GETSEQINSCOUNT(n->n_p);
        n->n_lastinsindex = n->n_count - 1;
        n->n_info = BNODE_GETINFO(n->n_p);
        n->n_keys = BNODE_GETKEYPTR(n->n_p);
        if (n->n_info & BNODE_MISMATCHARRAY) {
            n->n_keysearchinfo_array = (ss_byte_t*) n->n_p + blocksize - n->n_count * 4;
        }

        DBE_BLOCK_GETTYPE(n->n_p, &blocktype);

        if (blocktype != DBE_BLOCK_TREENODE && dbe_debug) {
            SsDbgMessage("Illegal index block type %d, addr %ld\n", (int)blocktype, (long)addr);
            SsMemFree(n);
            return(NULL);
        }
        ss_rc_assert(blocktype == DBE_BLOCK_TREENODE, blocktype);

        DBE_BLOCK_GETCPNUM(n->n_p, &n->n_cpnum);

        if (cacheslot != NULL) {
            succp = dbe_cacheslot_setuserdata(n->n_cacheslot, n, n->n_level, n->n_bonsaip);
            if (!succp) {
                SsMemFree(n);
                n = dbe_cacheslot_getuserdata(cacheslot);
                CHK_BNODE(n);
            }
        }

        return(n);
}

dbe_bnode_t* dbe_bnode_initbyslot(
        dbe_cacheslot_t* cacheslot,
        char* p,
        su_daddr_t addr,
        bool bonsaip,
        dbe_gobj_t* go)
{
        return(bnode_initbyslot(
                cacheslot,
                p,
                addr,
                bonsaip,
                go,
                0));
}

void dbe_bnode_reachinfo_init(void)
{
        int i;

        if (dbe_bnode_reachinfo[0].ri_mutex == NULL) {
            for (i = 0; i < BNODE_REACHINFO_MAX; i++) {
                dbe_bnode_reachinfo[i].ri_mutex = SsMutexInit(SS_SEMNUM_ANONYMOUS_SEM);
            }
        }
}

void dbe_bnode_reachinfo_done(void)
{
        int i;

        if (dbe_bnode_reachinfo[0].ri_mutex != NULL) {
            for (i = 0; i < BNODE_REACHINFO_MAX; i++) {
                SsMutexDone(dbe_bnode_reachinfo[i].ri_mutex);
                dbe_bnode_reachinfo[i].ri_mutex = NULL;
            }
        }
}

SS_INLINE void dbe_bnode_reachinfo_update(bool bonsaip, int level, int len)
{
        int index;
        int pmonindex;

        if (!bonsaip) {
            if (level == 0) {
                index = 0;
                pmonindex = SS_PMON_BNODE_STORAGELEAFLEN;
            } else {
                index = 1;
                pmonindex = SS_PMON_BNODE_STORAGEINDEXLEN;
            }
        } else {
            if (level == 0) {
                index = 2;
                pmonindex = SS_PMON_BNODE_BONSAILEAFLEN;
            } else {
                index = 3;
                pmonindex = SS_PMON_BNODE_BONSAIINDEXLEN;
            }
        }
        SsMutexLock(dbe_bnode_reachinfo[index].ri_mutex);
        dbe_bnode_reachinfo[index].ri_bytes += len;
        if (dbe_bnode_reachinfo[index].ri_count++ % 100 == 0
            && dbe_bnode_reachinfo[index].ri_count != 0) 
        {
            SS_PMON_SET(pmonindex, dbe_bnode_reachinfo[index].ri_bytes / dbe_bnode_reachinfo[index].ri_count);
        }
        SsMutexUnlock(dbe_bnode_reachinfo[index].ri_mutex);
}

/*##*********************************************************************\
 *
 *		dbe_bnode_get
 *
 * Gets a node from the cache from the given address in mode specified
 * by parameter reachmode.
 *
 * Parameters :
 *
 *	go - in, hold
 *		global objects
 *
 *	addr - in
 *		node address is cache
 *
 *	reachmode - in
 *		cache reach mode
 *
 * Return value - give :
 *
 *      node pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bnode_t* dbe_bnode_get(
        dbe_gobj_t* go,
        su_daddr_t addr,
        bool bonsaip,
        int level,
        dbe_cache_reachmode_t reachmode,
        dbe_info_t* info)
{
        dbe_bnode_t* n;
        dbe_cacheslot_t* cacheslot;
        char* p;
        dbe_blocktype_t blocktype;
        su_profile_timer;

        ss_dassert(bonsaip == TRUE || bonsaip == FALSE);
        su_profile_start;

        if (dbe_cfg_usenewbtreelocking) {
            if (reachmode == DBE_CACHE_READWRITE) {
                ss_rc_dassert(level == 0, level);
            } else if (reachmode == DBE_CACHE_READWRITE_NOCOPY) {
                ss_rc_dassert(level > 0, level);
            }
        }

        cacheslot = dbe_iomgr_reach(
                        go->go_iomgr,
                        addr,
                        reachmode,
                        info->i_flags,
                        &p,
                        NULL);
        /* ss_dassert(!dbe_cacheslot_isoldversion(cacheslot)); */

        su_profile_stop("dbe_bnode_get:dbe_iomgr_reach");

        if (p == NULL) {
            p = dbe_cacheslot_getdata(cacheslot);
        }
        DBE_BLOCK_GETTYPE(p, &blocktype);
        if (blocktype != DBE_BLOCK_TREENODE) {
            if (SU_BFLAG_TEST(info->i_flags, DBE_INFO_IGNOREWRONGBNODE)) {
                dbe_iomgr_release(
                    go->go_iomgr,
                    cacheslot,
                    DBE_CACHE_CLEAN,
                    bnode_readwritectx);
                return(NULL);
            }
            if (dbe_debug) {
                SsDbgMessage(su_rc_textof(DBE_ERR_ILLINDEXBLOCK_DLSD), (int)blocktype, (long)addr, "bnode_get", (int)reachmode);
                SsDbgMessage("\n");
                n = bnode_create_empty(go, addr, bonsaip, cacheslot, FALSE);
                return(n);
            }
            su_emergency_exit(
                __FILE__,
                __LINE__,
                DBE_ERR_ILLINDEXBLOCK_DLSD,
                (int)blocktype,
                (long)addr,
                "bnode_get",
                (int)reachmode);
        }

        n = dbe_cacheslot_getuserdata(cacheslot);

        if (n == NULL) {
            n = bnode_initbyslot(cacheslot, p, addr, bonsaip, go, 0);
            if (dbe_debug && n == NULL) {
                return(NULL);
            }
            CHK_BNODE(n);
        } else {
            /* Logically this is same as reading a node from disk,
             * so the node cannot be dirty.
             */
            ss_debug(TWO_BYTE_T two_byte;)

            CHK_BNODE(n);

            ss_debug(two_byte = BNODE_LOADLEN(n->n_p));
            ss_dassert(n->n_len == two_byte);
            ss_debug(two_byte = BNODE_LOADCOUNT(n->n_p));
            ss_dassert(n->n_count == two_byte);
            ss_dassert(n->n_level == BNODE_GETLEVEL(p));
            ss_dassert(n->n_seqinscount == BNODE_GETSEQINSCOUNT(p));
            /* ss_dassert(n->n_info == BNODE_GETINFO(p)); Timing issue with current mismatch code. */
            ss_dassert(n->n_keys == BNODE_GETKEYPTR(p));
            ss_dassert(n->n_bonsaip == bonsaip);
            if (n->n_info & BNODE_MISMATCHARRAY) {
                ss_dassert(n->n_keysearchinfo_array == n->n_p + n->n_go->go_idxfd->fd_blocksize - n->n_count * 4);
            }

            /* The dbe_bnode_t structures may be shared by different searches
             * and threads, so we should not overwrite n_dirty flag when
             * reading a block.
             * n->n_dirty = FALSE;
             */
        }

        dbe_bnode_reachinfo_update(n->n_bonsaip, n->n_level, n->n_len);

        ss_dassert(n->n_cacheslot == cacheslot);
        ss_dassert(p == n->n_p);
        ss_dassert(level == -1 || n->n_level == level);

        ss_dprintf_1(("dbe_bnode_get:addr=%ld, reachmode=%s (%d), level=%d, bonsaip=%d\n", addr, dbe_cache_reachmodetostr(reachmode), reachmode, n->n_level, n->n_bonsaip));

        return(n);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_remove
 *
 * Removes a node back to cache. All nodes retrieved using functions
 * dbe_bnode_create or dbe_bnode_read must be released using this
 * function or function dbe_bnode_write.
 *
 * Parameters :
 *
 *	n - in, take
 *		node pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bnode_remove(dbe_bnode_t* n)
{
        su_ret_t rc;
        su_daddr_t addr;
        dbe_cpnum_t cpnum;
        dbe_gobj_t* go;

        ss_dprintf_1(("dbe_bnode_remove: addr = %ld\n", n->n_addr));
        SS_PUSHNAME("dbe_bnode_remove");
        CHK_BNODE(n);

        addr = n->n_addr;
        cpnum = n->n_cpnum;
        go = n->n_go;

        dbe_iomgr_release(
            go->go_iomgr,
            n->n_cacheslot,
            DBE_CACHE_IGNORE,
            bnode_readonlyctx);

        if (cpnum < dbe_counter_getcpnum(go->go_ctr)) {
            rc = dbe_cl_add(go->go_idxfd->fd_chlist, cpnum, addr);
            su_rc_assert(rc == SU_SUCCESS, rc);
        } else {
            ss_dassert(cpnum == dbe_counter_getcpnum(go->go_ctr));
            rc = dbe_fl_free(go->go_idxfd->fd_freelist, addr);
            su_rc_assert(rc == SU_SUCCESS, rc);
        }
        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *		dbe_bnode_writewithmode
 *
 * Writes a node back to cache with a given mode. No physical write is
 * done if the node is not modified. All nodes retrieved using functions
 * dbe_bnode_create or dbe_bnode_read must be released using this function
 * function dbe_bnode_release, or function dbe_bnode_write.
 *
 * Parameters :
 *
 *	n - in, take
 *		node pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bnode_writewithmode(
        dbe_bnode_t* n,
        dbe_cache_releasemode_t mode)
{
        TWO_BYTE_T two_byte;

        CHK_BNODE(n);

        if (n->n_dirty) {
            ss_dassert(n->n_cpnum == dbe_counter_getcpnum(n->n_go->go_ctr));
            BNODE_SETLEVEL(n->n_p, n->n_level);
            two_byte = (TWO_BYTE_T)n->n_len;
            BNODE_STORELEN(n->n_p, two_byte);
            two_byte = (TWO_BYTE_T)n->n_count;
            BNODE_STORECOUNT(n->n_p, two_byte);
            if (n->n_seqinscount > UCHAR_MAX) {
                n->n_seqinscount = UCHAR_MAX;
            }
            BNODE_SETSEQINSCOUNT(n->n_p, n->n_seqinscount);
            BNODE_SETINFO(n->n_p, n->n_info);
            DBE_BLOCK_SETCPNUM(n->n_p, &n->n_cpnum);
            n->n_dirty = FALSE;
        }

        n->n_lastuse = FALSE;

        ss_debug(two_byte = BNODE_LOADLEN(n->n_p));
        ss_dassert(n->n_len == two_byte);
        ss_debug(two_byte = BNODE_LOADCOUNT(n->n_p));
        ss_dassert(n->n_count == two_byte);
        ss_dassert(n->n_level == BNODE_GETLEVEL(n->n_p));
        ss_dassert(n->n_seqinscount == BNODE_GETSEQINSCOUNT(n->n_p));
        /* ss_dassert(n->n_info == BNODE_GETINFO(n->n_p)); Timing issue with current mismatch code. */

        dbe_iomgr_release(
            n->n_go->go_iomgr,
            n->n_cacheslot,
            mode,
            bnode_readwritectx);
}

#if 0
/*##**********************************************************************\
 *
 *		dbe_bnode_flush
 *
 * Flushes a node to the cache. The node is still reserved after this
 * call.
 *
 * Parameters :
 *
 *	n - in out, use
 *		node pointer
 *
 * Return value :
 *
 *      node pointer, may be differenent than input node pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bnode_t* dbe_bnode_flush(dbe_bnode_t* n, dbe_info_t* info)
{
        CHK_BNODE(n);
        ss_dprintf_1(("dbe_bnode_flush:addr = %ld\n", n->n_addr));

        if (n->n_dirty) {
            dbe_gobj_t* go;
            su_daddr_t addr;
            int level;
            bool bonsaip;

            go = n->n_go;
            addr = n->n_addr;
            level = n->n_level;
            bonsaip = n->n_bonsaip;

            dbe_bnode_writewithmode(n, DBE_CACHE_FLUSH);
            n = dbe_bnode_getreadwrite(go, addr, bonsaip, level, info);
        }
        return(n);
}
#endif /* 0 */

/*##**********************************************************************\
 *
 *		dbe_bnode_init_tmp
 *
 * Initializes a new temporary node. The node buffer is allocated
 * from the cache.
 *
 * Parameters :
 *
 *	go - in, hold
 *		global objects
 *
 * Return value - give :
 *
 *      temporary node pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bnode_t* dbe_bnode_init_tmp(dbe_gobj_t* go)
{
        dbe_bnode_t* n;
        dbe_blocktype_t blocktype;

        ss_dprintf_1(("dbe_bnode_init_tmp\n"));

        n = SSMEM_NEW(dbe_bnode_t);

        ss_debug(n->n_chk = DBE_CHK_BNODE;)
        n->n_cacheslot = dbe_cache_alloc(go->go_idxfd->fd_cache, &n->n_p);
        n->n_go = go;
        n->n_len = 0;
        n->n_count = 0;
        n->n_level = 0;
        n->n_addr = 0;
        n->n_dirty = FALSE;
        n->n_seqinscount = 0;
        n->n_lastinsindex = 0;
        n->n_info = 0;
        n->n_cpnum = 0;
        n->n_keys = BNODE_GETKEYPTR(n->n_p);

        blocktype = DBE_BLOCK_TREENODE;
        DBE_BLOCK_SETTYPE(n->n_p, &blocktype);

        return(n);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_done_tmp
 *
 * Releases a temporary node.
 *
 * Parameters :
 *
 *	n - in, take
 *		temporary node pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bnode_done_tmp(dbe_bnode_t* n)
{
        dbe_blocktype_t blocktype;

        ss_dprintf_1(("dbe_bnode_done_tmp\n"));
        CHK_BNODE(n);
        ss_dassert(!n->n_dirty);

        blocktype = DBE_BLOCK_FREE;
        DBE_BLOCK_SETTYPE(n->n_p, &blocktype);
        dbe_cache_free(n->n_go->go_idxfd->fd_cache, n->n_cacheslot);

        ss_debug(n->n_chk = (int)DBE_CHK_BNODE + 1000;)

        SsMemFree(n);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_copy_tmp
 *
 * Copiens content of source node to the target node. The created
 * copy is only a temporary copy, it is not assigned to any disk
 * address and cannot be changed.
 *
 * Parameters :
 *
 *	target - in out, use
 *		target node
 *
 *	source - in, use
 *		source node
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bnode_copy_tmp(dbe_bnode_t* target, dbe_bnode_t* source)
{
        CHK_BNODE(target);
        CHK_BNODE(source);
        ss_dprintf_1(("dbe_bnode_copy_tmp:source addr = %ld, count = %d\n", source->n_addr, source->n_count));

        target->n_len = source->n_len;
        target->n_count = source->n_count;
        target->n_level = source->n_level;
        target->n_info = source->n_info;
        target->n_addr = source->n_addr;
        target->n_seqinscount = source->n_seqinscount;
        target->n_lastinsindex = source->n_lastinsindex;
        target->n_cpnum = source->n_cpnum;
        target->n_bonsaip = source->n_bonsaip;

        memcpy(target->n_p, source->n_p, BNODE_HEADERLEN);
        memcpy(target->n_keys, source->n_keys, target->n_len);

        if (target->n_info & BNODE_MISMATCHARRAY) {
            target->n_keysearchinfo_array = (ss_byte_t*)  
                target->n_p + 
                target->n_go->go_idxfd->fd_blocksize - 
                target->n_count * 4;
            memcpy(
                target->n_keysearchinfo_array, 
                source->n_keysearchinfo_array, 
                target->n_count * 4);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bnode_relocate
 *
 * Relocates the node to a new address. Needed when nodes are
 * changed for the first time after a checkpoint.
 *
 * Parameters :
 *
 *	n - in out, use
 *		node pointer
 *
 *	p_addr - out
 *		New node address.
 *
 *	p_rc - out
 *		error code if function returns NULL
 *
 * Return value :
 *
 *      node pointer, may be different than input node pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bnode_t* dbe_bnode_relocate(
        dbe_bnode_t* n,
        su_daddr_t* p_addr,
        dbe_ret_t* p_rc,
        dbe_info_t* info)
{
        su_ret_t rc;
        su_daddr_t newaddr;
        dbe_cacheslot_t* cacheslot;
        char* p;
        dbe_gobj_t* go;
        dbe_blocktype_t blocktype;
        bool bonsaip;
        su_daddr_t oldaddr;
        dbe_cpnum_t oldcpnum;

        SS_PUSHNAME("dbe_bnode_relocate");
        CHK_BNODE(n);
        ss_dassert(n->n_cpnum < dbe_counter_getcpnum(n->n_go->go_ctr));
        ss_dassert(n->n_level > 0 || !n->n_dirty);

        go = n->n_go;

        /* Allocate a new block for the node. */
        rc = dbe_fl_alloc(go->go_idxfd->fd_freelist, &newaddr, info);
        if (rc != SU_SUCCESS) {
            *p_rc = rc;
            SS_POPNAME;
            return(NULL);
        }

        ss_dprintf_1(("dbe_bnode_relocate:addr %ld -> %ld\n", n->n_addr, newaddr));

        bonsaip = n->n_bonsaip;
        oldaddr = n->n_addr;
        oldcpnum = n->n_cpnum;

        /* Update the changed node address in the cache. */
        cacheslot = dbe_cache_relocate(
                            go->go_idxfd->fd_cache,
                            n->n_cacheslot,
                            newaddr,
                            &p,
                            info->i_flags);

        /* Put old block address to a list of changed blocks. */
        rc = dbe_cl_add(go->go_idxfd->fd_chlist, oldcpnum, oldaddr);
        if (rc != SU_SUCCESS) {
            dbe_db_setfatalerror(go->go_db, rc);
            *p_rc = rc;
            SS_POPNAME;
            return(NULL);
        }

        if (p == NULL) {
            p = dbe_cacheslot_getdata(cacheslot);
        }
        DBE_BLOCK_GETTYPE(p, &blocktype);
        if (blocktype != DBE_BLOCK_TREENODE) {

            su_emergency_exit(
                        __FILE__,
                        __LINE__,
                        DBE_ERR_ILLINDEXBLOCK_DLSD,
                        (int)blocktype,
                        (long)newaddr,
                        "dbe_bnode_relocate",
                        0);
        }
        n = dbe_cacheslot_getuserdata(cacheslot);
        if (n == NULL) {
            n = bnode_initbyslot(cacheslot, p, newaddr, bonsaip, go, 0);
            ss_assert(n != NULL);
        } else {
            CHK_BNODE(n);
            ss_dassert(cacheslot == n->n_cacheslot)
            ss_dassert(p == n->n_p)
        }

        n->n_addr = newaddr;
        n->n_dirty = TRUE;
        n->n_bonsaip = bonsaip;

        *p_addr = newaddr;

        SS_POPNAME;
        return(n);
}

void dbe_bnode_setreadonly(
        dbe_bnode_t* n)
{
        CHK_BNODE(n);

        dbe_cache_setslotreadonly(
            n->n_go->go_idxfd->fd_cache,
            n->n_cacheslot);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_setcpnum
 *
 * Sets the cpnum of the node.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 *      cpnum - in
 *          new checkpoint number for the node
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bnode_setcpnum(dbe_bnode_t* n, dbe_cpnum_t cpnum)
{
        CHK_BNODE(n);
        ss_dassert(n->n_cpnum < cpnum);

        n->n_cpnum = cpnum;
        n->n_dirty = TRUE;

        DBE_BLOCK_SETCPNUM(n->n_p, &n->n_cpnum);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getfirstkey
 *
 * Returns a pointer to the first key value in the node. The pointer
 * points to the local storage inside the node and it should not be
 * modified or released by the caller.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 * Return value - ref :
 *
 *      pointer to the first key value
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bkey_t* dbe_bnode_getfirstkey(dbe_bnode_t* n)
{
        CHK_BNODE(n);

        if (n->n_info & BNODE_MISMATCHARRAY) {
            int offset;
            bnode_getkeyoffset(n, 0, offset);
            return((dbe_bkey_t*)&n->n_keys[offset]);

        } else {
            return((dbe_bkey_t*)n->n_keys);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getbonsai
 *
 * Returns a pointer to the first key value in the node. The pointer
 * points to the local storage inside the node and it should not be
 * modified or released by the caller.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 * Return value - ref :
 *
 *      pointer to the first key value
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bnode_getbonsai(dbe_bnode_t* n)
{
        CHK_BNODE(n);

        return(n->n_bonsaip);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_inheritlevel
 *
 * Inherits a node level from a lower level node. This must be called
 * each time the tree grows in height.
 *
 * Parameters :
 *
 *	nn - in out, use
 *		new highest level node
 *
 *	on - in, use
 *		old highest level node
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bnode_inheritlevel(dbe_bnode_t* nn, dbe_bnode_t* on)
{
        CHK_BNODE(nn);
        CHK_BNODE(on);

        nn->n_level = on->n_level + 1;
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getlength
 *
 * Returns the index block length.
 *
 * Parameters :
 *
 *	n -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long dbe_bnode_getlength(
        char* n)
{
        TWO_BYTE_T len;

        len = BNODE_LOADLEN(n);

        return((long)BNODE_HEADERLEN + (long)len);
}

#ifndef SS_LIGHT

static long bnode_getkeyid(va_t* va, int level)
{
        long keyid;

        if (level > 0 || (va_netlen(va) == 1 && *va_getasciiz(va) == '\0')) {
            keyid = 0;
        } else {
            keyid = va_getlong(va);
        }
        return(keyid);
}

static void bnode_print_checkkeychange(dbe_bkey_t* ck, dbe_bkey_t* fk)
{
        bool isclust;
        va_t* va;
        long keyid;

        va = VTPL_GETVA_AT0(dbe_bkey_getvtpl(fk));
        keyid = bnode_getkeyid(va, 0);

        if (keyid != dbe_curkeyid) {
            /* Keyid changed.
             */
            isclust = dbe_bkey_isclustering(fk);
            if (dbe_keynameid_rbt != NULL) {
                dbe_keynameid_t search_dk;
                su_rbt_node_t* rbtnode;

                search_dk.dk_keyid = keyid;

                rbtnode = su_rbt_search(dbe_keynameid_rbt, &search_dk);
                if (rbtnode == NULL) {
                    dbe_curkey_keynameid = NULL;
                } else {
                    dbe_curkey_keynameid = su_rbtnode_getkey(rbtnode);
                }
            }
            SsDbgMessage("Key id = %ld %s\n", keyid, isclust ? "Clustering key" : "");
            dbe_curkeyid = keyid;
        }
        if (dbe_curkey_keynameid != NULL) {
            int keylen;
            int full_keylen;

            keylen = dbe_bkey_getlength(ck);
            SsInt8AddUint4(
                &dbe_curkey_keynameid->dk_compressedbytes, 
                dbe_curkey_keynameid->dk_compressedbytes, 
                (ss_uint4_t)keylen);
            
            full_keylen = dbe_bkey_getlength(fk);
            SsInt8AddUint4(
                &dbe_curkey_keynameid->dk_fullbytes, 
                dbe_curkey_keynameid->dk_fullbytes, 
                (ss_uint4_t)full_keylen);

            if (full_keylen < keylen) {
                SsDbgMessage("Compressed %d bytes\n", keylen);
                dbe_bkey_print(NULL, ck);
                SsDbgMessage("Full %d bytes\n", full_keylen);
                dbe_bkey_print(NULL, fk);
            }
        }
}

/*#**********************************************************************\
 *
 *		bnode_printvalues
 *
 * Prints all key values in a node.
 *
 * Parameters :
 *
 *      fp -
 *
 *
 *	n - in, use
 *		node pointer
 *
 *      level -
 *
 *      info -
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool bnode_printvalues(void* fp, dbe_bnode_t* n, int level, int info, dbe_bkeyinfo_t* ki)
{
        int i;
        int j;
        dbe_bkey_t* k;
        int num = 1;
        int kpos = 0;
        char* keys;
        va_t* va;
        long keyid = 0;
        bool succp = TRUE;

        ss_rc_dassert(ss_dbg_jmpbufpos < SS_DBG_JMPBUF_MAX, ss_dbg_jmpbufpos);
        ss_dbg_jmpbufpos++;
        if (setjmp(ss_dbg_jmpbuf[ss_dbg_jmpbufpos-1]) != 0) {
            /* Error occured */
            ss_dbg_jmpbufpos--;
            return(FALSE);
        }

        if (n == NULL) {
            if (fp != NULL) {
                SsFprintf(fp, "NULL");
            } else {
                SsDbgPrintf("NULL");
            }
            ss_dbg_jmpbufpos--;
            return(FALSE);
        }

        if (dbe_curkey_keynameid != NULL && level == 0) {
            SsInt8AddUint4(
                &dbe_curkey_keynameid->dk_pages, 
                dbe_curkey_keynameid->dk_pages, 
                (ss_uint4_t)1);
        }

        for (j = 0; j < level; j++) {
            if (fp != NULL) {
                SsFprintf(fp, "      ");
            } else {
                SsDbgPrintf("      ");
            }
        }

        k = dbe_bkey_init(ki);
        kpos = 0;

        if (n->n_count > 0) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, 0, kpos);
                keys = n->n_keys +  kpos;
            } else {
                keys = n->n_keys;
            }
            dbe_bkey_copy(k, (dbe_bkey_t*)keys);
        }

        if (info) {
            va = VTPL_GETVA_AT0(dbe_bkey_getvtpl(k));
            keyid = bnode_getkeyid(va, level);
            if (fp != NULL) {
                SsFprintf(fp, "pos %4d, clen %3d, flen %3d, id %5ld\n",
                    keys - n->n_p,
                    (int)dbe_bkey_getlength((dbe_bkey_t*)&keys[0]),
                    (int)dbe_bkey_getlength(k),
                    keyid);
            } else {
                SsDbgPrintf("pos %4d, clen %3d, flen %3d, id %5ld\n",
                    keys - n->n_p,
                    (int)dbe_bkey_getlength((dbe_bkey_t*)&keys[0]),
                    (int)dbe_bkey_getlength(k),
                    keyid);
            }
        } else {
            if (fp != NULL) {
                SsFprintf(fp, "  %d:", num++);
            } else {
                SsDbgPrintf("  %d:", num++);
            }
        }
        if (dbe_reportindex && level == 0) {
            bnode_print_checkkeychange(k, k);
        }
        dbe_bkey_print(fp, k);

        for (i = 0; i < n->n_count - 1; i++) {
            if (i % 100 == 0) {
                SsThrSwitch();  /* Needed for Novell-version */
            }
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, i+1, kpos);
            } else {
                kpos += dbe_bkey_getlength((dbe_bkey_t*)&keys[kpos]);
            }
            dbe_bkey_expand(k, k, (dbe_bkey_t*)&n->n_keys[kpos]);
            for (j = 0; j < level; j++) {
                if (fp != NULL) {
                    SsFprintf(fp, "      ");
                } else {
                    SsDbgPrintf("      ");
                }
            }
            if (info) {
                va = VTPL_GETVA_AT0(dbe_bkey_getvtpl(k));
                keyid = bnode_getkeyid(va, level);
                if (fp != NULL) {
                    SsFprintf(fp, "pos %4d, clen %3d, flen %3d, id %5ld\n",
                        &n->n_keys[kpos] - n->n_p,
                        (int)dbe_bkey_getlength((dbe_bkey_t*)&n->n_keys[kpos]),
                        (int)dbe_bkey_getlength(k),
                        keyid);
                } else {
                    SsDbgPrintf("pos %4d, clen %3d, flen %3d, id %5ld\n",
                        &n->n_keys[kpos] - n->n_p,
                        (int)dbe_bkey_getlength((dbe_bkey_t*)&n->n_keys[kpos]),
                        (int)dbe_bkey_getlength(k),
                        keyid);
                }
            } else {
                if (fp != NULL) {
                    SsFprintf(fp, "  %d:", num++);
                } else {
                    SsDbgPrintf("  %d:", num++);
                }
            }
            if (dbe_reportindex && level == 0) {
                bnode_print_checkkeychange((dbe_bkey_t*)&n->n_keys[kpos], k);
            }
            dbe_bkey_print(fp, k);
        }
        if (n->n_info & BNODE_MISMATCHARRAY) {
            ss_byte_t* keysearchinfo_array;
            SsFprintf(fp, "[i, mismatch pos, mismatch byte, offset\n");
            keysearchinfo_array = n->n_keysearchinfo_array;
            for (i = 0; i < n->n_count; i++, keysearchinfo_array += 4) {
                uint index;
                ss_uint2_t tmp_offset;

                index = keysearchinfo_array[0];
                ss_dassert(index <= 255);
                tmp_offset = SS_UINT2_LOADFROMDISK(&keysearchinfo_array[2]);
                ss_dassert(tmp_offset >= 0);
                ss_dassert(tmp_offset < n->n_len);
                SsFprintf(fp, "[%3d, %3d, %3d, %5d]\n", i, index, (uint)keysearchinfo_array[1], tmp_offset);
            }
        }

        if (dbe_reportindex && level == 0) {
#           define BNODE_MAXKEYID 10
            static int prev_keyid_idx = 0;
            static struct {
                long pk_firstkeyid;
                long pk_lastkeyid;
            } prev_keyid[BNODE_MAXKEYID];
            static bool bad_node_found = FALSE;

            va = VTPL_GETVA_AT0(dbe_bkey_getvtpl((dbe_bkey_t*)keys));
            prev_keyid[prev_keyid_idx].pk_firstkeyid = bnode_getkeyid(va, level);
            prev_keyid[prev_keyid_idx].pk_lastkeyid = keyid;
            prev_keyid_idx++;
            if (prev_keyid_idx == BNODE_MAXKEYID) {
                prev_keyid_idx = 0;
            }
            if (dbe_curkey == NULL) {
                dbe_curkey = dbe_bkey_init(ki);
            } else if (dbe_bkey_compare(dbe_curkey, (dbe_bkey_t*)keys) >= 0) {
                SsDbgMessage("Illegal key order in index leaf\n");
                succp = FALSE;
                bad_node_found = TRUE;
            } else if (bad_node_found) {
                /* First ok node after a bad node. */
                int i;
                int idx;
                int nodes[4];
                idx = prev_keyid_idx;
                for (i = 3; i >= 0; i--) {
                    idx--;
                    if (idx < 0) {
                        idx = BNODE_MAXKEYID-1;
                    }
                    nodes[i] = idx;
                }
                SsDbgMessage("  Key id ranges of possibly bad leaves\n");
                for (i = 0; i < 4; i++) {
                    SsDbgMessage("  %5ld - %5ld\n",
                        prev_keyid[nodes[i]].pk_firstkeyid,
                        prev_keyid[nodes[i]].pk_lastkeyid);
                }
                bad_node_found = FALSE;
            }
            dbe_bkey_copy(dbe_curkey, (dbe_bkey_t*)k);
        }
        dbe_bkey_done(k);
        ss_dbg_jmpbufpos--;
        return(succp);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_print
 *
 *
 *
 * Parameters :
 *
 *	fp -
 *
 *
 *	n -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bnode_print(void* fp, char* p, size_t blocksize)
{
        dbe_bnode_t* n;
        bool b;
        dbe_bkeyinfo_t ki;

        n = bnode_initbyslot(NULL, p, 0L, FALSE, NULL, blocksize);
        if (n == NULL) {
            return(FALSE);
        }

        dbe_bkeyinfo_init(&ki, (uint)blocksize);

        if (fp != NULL) {
            SsFprintf(fp, "len: %d, count: %d, level: %d, seqinscount: %d\n",
                (int)n->n_len, (int)n->n_count, n->n_level, n->n_seqinscount);
        } else {
            SsDbgPrintf("len: %d, count: %d, level: %d, seqinscount: %d\n",
                (int)n->n_len, (int)n->n_count, n->n_level, n->n_seqinscount);
        }
        b = bnode_printvalues(fp, n, 0, TRUE, &ki);
        SsMemFree(n);
        return(b);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_printtree
 *
 * Prints an index tree rooted at n to stdout.
 *
 * Parameters :
 *
 *      fp -
 *
 *
 *	n - in, use
 *		root node
 *
 *	values - in
 *		if TRUE, prints also key values
 *
 * Return value :
 *
 *      TRUE always
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bnode_printtree(void* fp, dbe_bnode_t* n, bool values)
{
        int i;
        int j;
        dbe_bnode_t* rn;
        int kpos = 0;
        dbe_bkey_t* k;
        bool succp = TRUE;
        dbe_bkeyinfo_t ki;
        dbe_info_t info;

        dbe_info_init(info, 0);

        dbe_bkeyinfo_init(&ki, (uint)n->n_go->go_idxfd->fd_blocksize);

        ss_rc_dassert(ss_dbg_jmpbufpos < SS_DBG_JMPBUF_MAX, ss_dbg_jmpbufpos);
        ss_dbg_jmpbufpos++;
        if (setjmp(ss_dbg_jmpbuf[ss_dbg_jmpbufpos-1]) != 0) {
            /* Error occured */
            ss_dbg_jmpbufpos--;
            SsDbgMessage("Error in index block at address %ld, level %d\n",
                (long)n->n_addr, (int)n->n_level);
            return(FALSE);
        }

        if (n == NULL) {
            if (fp != NULL) {
                SsFprintf(fp, "NULL");
            } else {
                SsDbgPrintf("NULL");
            }
            ss_dbg_jmpbufpos--;
            return(FALSE);
        }

        if (dbe_fl_is_free(n->n_go->go_idxfd->fd_freelist, n->n_addr)) {
            SsDbgMessage("B-tree page %ld is also in the free list\n",
                (long)n->n_addr);
#ifdef AUTOTEST_RUN
            ss_error;
#endif
            return FALSE;
        }

        if (dbe_bnode_getlevel(n) == 0) {
            if (fp != NULL) {
                SsFprintf(fp, "L[cnt:%d,len:%d,addr:%ld,cpnum:%ld,mmi:%d]\n",
                    n->n_count, n->n_len, (long)n->n_addr, (long)n->n_cpnum, n->n_info & BNODE_MISMATCHARRAY);
            } else {
                SsDbgPrintf("L[cnt:%d,len:%d,addr:%ld,cpnum:%ld,mmi:%d]\n",
                    n->n_count, n->n_len, (long)n->n_addr, (long)n->n_cpnum, n->n_info & BNODE_MISMATCHARRAY);
            }
            if (values) {
                if (!bnode_printvalues(fp, n, n->n_level, FALSE, &ki)) {
                    succp = FALSE;
                    SsDbgMessage("Illegal index data block at address %ld, level %d\n",
                        (long)n->n_addr, (int)n->n_level);
                    dbe_bnode_print(NULL, n->n_p, n->n_go->go_idxfd->fd_blocksize);
                }
            }
            ss_dbg_jmpbufpos--;
            return(succp);
        }

        for (i = 0; i < n->n_count / 2; i++) {
            SsThrSwitch();  /* Needed for Novell-version */
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, i, kpos);
            }
            k = (dbe_bkey_t*)&n->n_keys[kpos];
            rn = dbe_bnode_getreadonly(n->n_go, dbe_bkey_getaddr(k), n->n_bonsaip, &info);
            if (rn == NULL) {
                succp = FALSE;
                SsDbgMessage("Illegal index block address %ld in index leaf at addr %ld, level %d\n",
                    (long)dbe_bkey_getaddr(k), (long)n->n_addr, (int)n->n_level);
                if (!values) {
                    ss_dbg_jmpbufpos--;
                    return(FALSE);
                }
            } else {
                int nodelen;
                if (!dbe_bnode_printtree(fp, rn, values)) {
                    succp = FALSE;
                }
                nodelen = BNODE_HEADERLEN + rn->n_len + rn->n_count * 4;
                dbe_bnode_totalnodelength += nodelen;
                dbe_bnode_totalnodekeycount += rn->n_count;
                dbe_bnode_totalnodecount++;
                if ((double)(nodelen) <=  0.25 * (double)(rn->n_go->go_idxfd->fd_blocksize)) {
                    dbe_bnode_totalshortnodecount++;
                }
                dbe_bnode_write(rn, FALSE);
            }
            if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                kpos += dbe_bkey_getlength(k);
            }
        }

        for (j = 0; j < n->n_level; j++) {
            if (fp != NULL) {
                SsFprintf(fp, "      ");
            } else {
                SsDbgPrintf("      ");
            }
        }
        if (fp != NULL) {
            SsFprintf(fp, "I%d[cnt:%d,len:%d,addr:%ld,cpnum:%ld,mmi:%d]\n",
                n->n_level, n->n_count, n->n_len, (long)n->n_addr, (long)n->n_cpnum, n->n_info & BNODE_MISMATCHARRAY);
        } else {
            SsDbgPrintf("I%d[cnt:%d,len:%d,addr:%ld,cpnum:%ld,mmi:%d]\n",
                n->n_level, n->n_count, n->n_len, (long)n->n_addr, (long)n->n_cpnum, n->n_info & BNODE_MISMATCHARRAY);
        }
        if (values) {
            if (!bnode_printvalues(fp, n, n->n_level, FALSE, &ki)) {
                succp = FALSE;
                SsDbgMessage("Illegal index block at address %ld, level %d\n",
                    (long)n->n_addr, (int)n->n_level);
                dbe_bnode_print(NULL, n->n_p, n->n_go->go_idxfd->fd_blocksize);
            }
        }

        for (i = n->n_count / 2; i < n->n_count; i++) {
            SsThrSwitch();  /* Needed for Novell-version */
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, i, kpos);
            }
            k = (dbe_bkey_t*)&n->n_keys[kpos];
            rn = dbe_bnode_getreadonly(n->n_go, dbe_bkey_getaddr(k), n->n_bonsaip, &info);
            if (rn == NULL) {
                succp = FALSE;
                SsDbgMessage("Illegal index block address %ld in index leaf at addr %ld, level %d\n",
                    (long)dbe_bkey_getaddr(k), (long)n->n_addr, (int)n->n_level);
                if (!values) {
                    ss_dbg_jmpbufpos--;
                    return(FALSE);
                }
            } else {
                int nodelen;
                if (!dbe_bnode_printtree(fp, rn, values)) {
                    succp = FALSE;
                }
                nodelen = BNODE_HEADERLEN + rn->n_len + rn->n_count * 4;
                dbe_bnode_totalnodelength += nodelen;
                dbe_bnode_totalnodekeycount += rn->n_count;
                dbe_bnode_totalnodecount++;
                if ((double)(nodelen) <=  0.25 * (double)(n->n_go->go_idxfd->fd_blocksize)) {
                    dbe_bnode_totalshortnodecount++;
                }
                dbe_bnode_write(rn, FALSE);
            }
            if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                kpos += dbe_bkey_getlength(k);
            }
        }
        ss_dbg_jmpbufpos--;
        return(succp);
}

/*#***********************************************************************\
 *
 *		bnode_checkvalues
 *
 *
 *
 * Parameters :
 *
 *	n -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool bnode_checkvalues(dbe_bnode_t* n)
{
        int i;
        dbe_bkey_t* k;
        int kpos = 0;
        int count;
        char* keys;
        dbe_bkeyinfo_t ki;

        dbe_bkeyinfo_init(&ki, 8192);
        ss_rc_dassert(ss_dbg_jmpbufpos < SS_DBG_JMPBUF_MAX, ss_dbg_jmpbufpos);
        ss_dbg_jmpbufpos++;
        if (setjmp(ss_dbg_jmpbuf[ss_dbg_jmpbufpos-1]) != 0) {
            /* Error occured */
            ss_dbg_jmpbufpos--;
            return(FALSE);
        }

        if (n == NULL) {
            ss_dbg_jmpbufpos--;
            return(FALSE);
        }

        keys = n->n_keys;
        count = n->n_count;

        k = dbe_bkey_init(&ki);
        if (n->n_info & BNODE_MISMATCHARRAY) {
            bnode_getkeyoffset(n, 0, kpos);
            dbe_bkey_copy(k, (dbe_bkey_t*)&n->n_keys[kpos]);
        } else {
            dbe_bkey_copy(k, (dbe_bkey_t*)keys);
        }
        kpos = 0;

        for (i = 0; i < count - 1; i++) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, i + 1, kpos);
            } else {
                kpos += dbe_bkey_getlength((dbe_bkey_t*)&keys[kpos]);
            }
            dbe_bkey_expand(k, k, (dbe_bkey_t*)&n->n_keys[kpos]);
        }
        dbe_bkey_done(k);
        ss_dbg_jmpbufpos--;
        return(TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_checktree
 *
 * Checks an index tree rooted at n.
 *
 * Parameters :
 *
 *	n - in, use
 *		root node
 *
 *	check_values - in
 *		If TRUE, checks also key values.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bnode_checktree(dbe_bnode_t* n, bool check_values)
{
        int i;
        dbe_bnode_t* rn;
        int kpos = 0;
        dbe_bkey_t* k;
        bool succp = TRUE;
        dbe_info_t info;

        dbe_info_init(info, 0);

        ss_assert(n != NULL);

        SsThrSwitch();  /* Needed for Novell-version */

        if (dbe_fl_is_free(n->n_go->go_idxfd->fd_freelist, n->n_addr)) {
            SsDbgMessage("B-tree page %ld is also in the free list\n",
                (long)n->n_addr);
#ifdef AUTOTEST_RUN
            ss_error;
#endif
            return FALSE;
        }

        if (check_values && !bnode_checkvalues(n)) {
            SsDbgMessage("Bad index block at address %ld, level %d\n",
                (long)n->n_addr, (int)n->n_level);
            return(FALSE);
        }
        if (n->n_level > 0) {
            for (i = 0; i < n->n_count; i++) {
                if (i % 100 == 0) {
                    SsThrSwitch();  /* Needed for Novell-version */
                }
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, i, kpos);
                }
                k = (dbe_bkey_t*)&n->n_keys[kpos];
                rn = dbe_bnode_getreadonly(n->n_go, dbe_bkey_getaddr(k), n->n_bonsaip, &info);
                if (rn == NULL) {
                    SsDbgMessage("Bad index block address %ld found in index leaf at addr %ld, level %d\n",
                        (long)dbe_bkey_getaddr(k), (long)n->n_addr, (int)n->n_level);
                    return(FALSE);
                }
                succp = dbe_bnode_checktree(rn, check_values);
                dbe_bnode_write(rn, FALSE);
                if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                    kpos += dbe_bkey_getlength(k);
                }
            }
        }
        return(succp);
}

#endif /* SS_LIGHT */

#ifndef SS_NOESTSAMPLES

/*#***********************************************************************\
 * 
 *		bnode_getrandomsample
 * 
 * Gets random key value.
 * 
 * Parameters : 
 * 
 *		n - 
 *			
 *			
 *		kb - 
 *			
 *			
 *		ke - 
 *			
 *			
 *		found_key - 
 *			
 *			
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool bnode_getrandomsample(
        dbe_bnode_t* n,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        dbe_bkey_t** p_found_key,
        int random_pos,
        int* p_nkeys)
{
        int i;
        int cmp;
        int count;
        char* keys;
        dbe_bkey_t* k;
        int nkeys = 0;
        int kpos;

        ss_dprintf_3(("bnode_getrandomsample:random_pos=%d\n", random_pos));
        ss_dassert(n != NULL);
        ss_dassert(dbe_bnode_getlevel(n) == 0);
        SS_PUSHNAME("bnode_getrandomsample");

        count = n->n_count;
        if (n->n_info & BNODE_MISMATCHARRAY) {
            bnode_getkeyoffset(n, 0, kpos);
            keys = &n->n_keys[kpos];
        } else {
            keys = n->n_keys;
        }

        if (count == 0) {
            /* Empty node. */
            ss_dprintf_4(("bnode_getrandomsample:empty node\n"));
            SS_POPNAME;
            return(FALSE);
        }

        k = dbe_bkey_init(n->n_go->go_bkeyinfo);
        dbe_bkey_copy(k, (dbe_bkey_t*)keys);

        for (i = 0; i < count; i++) {
            cmp = dbe_bkey_compare(kb, k);
            if (cmp <= 0) {
                break;
            }
            if (i < count-1) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, i + 1, kpos);
                    keys = &n->n_keys[kpos];
                } else {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
                dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
            }
        }

        nkeys = 0;

        for (; i < count; i++) {
            if (dbe_bkey_compare(k, ke) >= 0) {
                /* End of range. */
                break;
            }
            if (random_pos != -1 && nkeys == random_pos) {
                ss_dprintf_4(("bnode_getrandomsample:take this key, nkeys=%d, random_pos=%d\n", nkeys, random_pos));
                *p_found_key = k;
                SS_POPNAME;
                return(TRUE);
            }
            nkeys++;
            if (i < count-1) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, i + 1, kpos);
                    keys = &n->n_keys[kpos];
                } else {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
                dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
            }
        }

        dbe_bkey_done(k);

        ss_dprintf_4(("bnode_getrandomsample:nkeys=%d\n", nkeys));
        SS_POPNAME;

        if (p_nkeys != NULL) {
            *p_nkeys = nkeys;
            return(nkeys > 0);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bnode_getrandomsample
 * 
 * Gets random key value.
 * 
 * Parameters : 
 * 
 *		n - 
 *			
 *			
 *		kb - 
 *			
 *			
 *		ke - 
 *			
 *			
 *		found_key - 
 *			
 *			
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dbe_bnode_getrandomsample(
        dbe_bnode_t* n,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        dbe_bkey_t** p_found_key)
{
        int nkeys = 0;
        bool succp;

        ss_dprintf_3(("dbe_bnode_getrandomsample\n"));
        ss_dassert(n != NULL);
        ss_dassert(dbe_bnode_getlevel(n) == 0);
        SS_PUSHNAME("dbe_bnode_getrandomsample");

        succp = bnode_getrandomsample(n, kb, ke, NULL, -1, &nkeys);
        if (succp) {
            ss_dassert(nkeys > 0);
            succp = bnode_getrandomsample(n, kb, ke, p_found_key, rand() % nkeys, NULL);
        }
        ss_dprintf_4(("bnode_getrangeaddrs:succp=%d\n", succp));

        SS_POPNAME;

        return(succp);
}

/*#***********************************************************************\
 *
 *		dbe_bnode_getrandomaddress
 *
 * Gets random node address from range.
 *
 * Parameters :
 *
 *	n -
 *
 *
 *	kmin -
 *
 *
 *	kmax -
 *
 *
 *	p_addrs -
 *
 *
 *	p_naddrs -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_bnode_getrandomaddress(
        dbe_bnode_t* n,
        dbe_bkey_t* kmin,
        dbe_bkey_t* kmax,
        su_daddr_t* p_addr)
{
        int i;
        int cmp;
        int count;
        char* keys;
        dbe_bkey_t* k;
        su_daddr_t* addrs = NULL;
        int naddrs = 0;
        long prev_addr = -1L;
        int kpos;

        ss_dprintf_3(("dbe_bnode_getrandomaddress\n"));
        ss_dassert(n != NULL);
        ss_dassert(dbe_bnode_getlevel(n) > 0);
        SS_PUSHNAME("dbe_bnode_getrandomaddress");

        count = n->n_count;
        if (n->n_info & BNODE_MISMATCHARRAY) {
            bnode_getkeyoffset(n, 0, kpos);
            keys = &n->n_keys[kpos];
        } else {
            keys = n->n_keys;
        }

        if (count == 0) {
            /* Empty node. */
            ss_dprintf_4(("dbe_bnode_getrandomaddress:empty node\n"));
            *p_addr = 0L;
            SS_POPNAME;
            return(FALSE);
        }

        k = dbe_bkey_init(n->n_go->go_bkeyinfo);
        dbe_bkey_copy(k, (dbe_bkey_t*)keys);

        for (i = 0; i < count; i++) {
            cmp = dbe_bkey_compare(kmin, k);
            if (cmp <= 0) {
                break;
            }
            prev_addr = dbe_bkey_getaddr(k);
            if (i < count-1) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, i + 1, kpos);
                    keys = &n->n_keys[kpos];
                } else {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
                dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
            }
        }

        if ((i > 0 && cmp < 0) || i == count) {
            /* Save the address from the previous key value. */
            ss_dassert(prev_addr != -1L);
            naddrs++;
            addrs = SsMemAlloc(sizeof(addrs[0]) * naddrs);
            addrs[naddrs-1] = prev_addr;
            if (i == count) {
                /* Only one suitable node found (last key value in leaf) */
                ss_dprintf_4(("dbe_bnode_getrandomaddress:only one suitable node found\n"));
                *p_addr = prev_addr;
                dbe_bkey_done(k);
                SsMemFree(addrs);
                SS_POPNAME;
                return(TRUE);
            }
        }

        for (; i < count; i++) {
            if (dbe_bkey_compare(k, kmax) >= 0) {
                /* End of range. */
                break;
            }
            naddrs++;
            if (addrs == NULL) {
                addrs = SsMemAlloc(sizeof(addrs[0]) * naddrs);
            } else {
                addrs = SsMemRealloc(addrs, sizeof(addrs[0]) * naddrs);
            }
            addrs[naddrs-1] = dbe_bkey_getaddr(k);
            ss_dassert(naddrs == 1 || addrs[naddrs-2] != addrs[naddrs-1])
            if (i < count-1) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, i + 1, kpos);
                    keys = &n->n_keys[kpos];
                } else {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
                dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
            }
        }

        dbe_bkey_done(k);

        *p_addr = addrs[rand() % naddrs];

        SsMemFree(addrs);

        ss_dprintf_4(("bnode_getrangeaddrs:addr = %d\n", *p_addr));

        SS_POPNAME;

        return(TRUE);
}

/*#***********************************************************************\
 *
 *		bnode_getrangeaddrs
 *
 * Stores all node addresses between range [kmin, kmax] into *p_addrs.
 *
 * Parameters :
 *
 *	n -
 *
 *
 *	kmin -
 *
 *
 *	kmax -
 *
 *
 *	p_addrs -
 *
 *
 *	p_naddrs -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void bnode_getrangeaddrs(
        dbe_bnode_t* n,
        dbe_bkey_t* kmin,
        dbe_bkey_t* kmax,
        long** p_addrs,
        int*  p_naddrs)
{
        int i;
        int cmp;
        int count;
        char* keys;
        dbe_bkey_t* k;
        long* addrs = NULL;
        int naddrs = 0;
        long prev_addr = -1L;
        int kpos;

        ss_dprintf_3(("bnode_getrangeaddrs\n"));
        ss_dassert(n != NULL);
        ss_dassert(dbe_bnode_getlevel(n) > 0);
        SS_PUSHNAME("bnode_getrangeaddrs");

        count = n->n_count;
        if (n->n_info & BNODE_MISMATCHARRAY) {
            bnode_getkeyoffset(n, 0, kpos);
            keys = &n->n_keys[kpos];
        } else {
            keys = n->n_keys;
        }

        if (count == 0) {
            /* Empty node. */
            ss_dprintf_4(("bnode_getrangeaddrs:empty node\n"));
            *p_addrs = 0L;
            SS_POPNAME;
            return;
        }

        k = dbe_bkey_init(n->n_go->go_bkeyinfo);
        dbe_bkey_copy(k, (dbe_bkey_t*)keys);

        for (i = 0; i < count; i++) {
            cmp = dbe_bkey_compare(kmin, k);
            if (cmp <= 0) {
                break;
            }
            prev_addr = dbe_bkey_getaddr(k);
            if (i < count-1) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, i + 1, kpos);
                    keys = &n->n_keys[kpos];
                } else {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
                dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
            }
        }

        if ((i > 0 && cmp < 0) || i == count) {
            /* Save the address from the previous key value. */
            ss_dassert(prev_addr != -1L);
            naddrs++;
            addrs = SsMemAlloc(sizeof(addrs[0]) * naddrs);
            addrs[naddrs-1] = prev_addr;
            if (i == count) {
                /* Only one suitable node found (last key value in leaf) */
                ss_dprintf_4(("bnode_getrangeaddrs:only one suitable node found\n"));
                *p_addrs = addrs;
                *p_naddrs = naddrs;
                dbe_bkey_done(k);
                SS_POPNAME;
                return;
            }
        }

        for (; i < count; i++) {
            if (dbe_bkey_compare(k, kmax) >= 0) {
                /* End of range. */
                break;
            }
            naddrs++;
            if (addrs == NULL) {
                addrs = SsMemAlloc(sizeof(addrs[0]) * naddrs);
            } else {
                addrs = SsMemRealloc(addrs, sizeof(addrs[0]) * naddrs);
            }
            addrs[naddrs-1] = dbe_bkey_getaddr(k);
            ss_dassert(naddrs == 1 || addrs[naddrs-2] != addrs[naddrs-1])
            if (i < count-1) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, i + 1, kpos);
                    keys = &n->n_keys[kpos];
                } else {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
                dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
            }
        }

        dbe_bkey_done(k);

        *p_addrs = addrs;
        *p_naddrs = naddrs;

        ss_dprintf_4(("bnode_getrangeaddrs:naddrs = %d\n", *p_naddrs));

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *		bnode_getsamplenodes
 *
 * Gets sample nodes and distributes sample_vtpl approximately evenly
 * over found nodes. Recursively descends to lower level nodes.
 *
 * Parameters :
 *
 *	n - in
 *
 *
 *	range_min - in
 *
 *
 *	range_max - in
 *
 *
 *	sample_vtpl - use
 *
 *
 *	sample_size - in
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void bnode_getsamplenodes(
        dbe_bnode_t* n,
        dbe_bkey_t* range_min,
        dbe_bkey_t* range_max,
        dynvtpl_t* sample_vtpl,
        int sample_size,
        bool mergep)
{
        long* addrs;
        int naddrs;
        dbe_bnode_t* cur_n;
        dbe_info_t info;

        dbe_info_init(info, 0);

        ss_pprintf_3(("bnode_getsamplenodes:sample_size=%d, node=%ld, level=%d\n",
            sample_size, n->n_addr, n->n_level));
        ss_dassert(sample_size > 0);

        /* Read all node addresses inside the range.
         */
        bnode_getrangeaddrs(
            n,
            range_min,
            range_max,
            &addrs,
            &naddrs);

        ss_pprintf_4(("bnode_getsamplenodes:naddrs = %d\n", naddrs));

        if (naddrs == 0) {
            return;
        }

        if (naddrs >= sample_size) {
            /* There are more or equal number of nodes than
             * sample positions.
             */
            double d;
            double inc;
            double last;
            int samplepos;

            ss_pprintf_4(("bnode_getsamplenodes:more nodes than sample positions\n"));
            inc = (double)naddrs / (double)sample_size;
            last = (double)naddrs;
            samplepos = 0;
            for (d = 0.0; d < last && samplepos < sample_size; d += inc) {
                cur_n = dbe_bnode_getreadonly(n->n_go, addrs[(int)d], n->n_bonsaip, &info);
                if (cur_n == NULL) {
                    SsDbgMessage("Bad index block address %ld found in index leaf at addr %ld, level %d\n",
                        (long)addrs[(int)d], (long)n->n_addr, (int)n->n_level);
                    return;
                }
                /* Recurse back to dbe_bnode_getkeysamples. */
                dbe_bnode_getkeysamples(
                    cur_n,
                    range_min,
                    range_max,
                    sample_vtpl + samplepos,
                    1, 
                    mergep);
                dbe_bnode_write(cur_n, FALSE);
                samplepos++;
            }

        } else {
            /* There are less nodes than sample positions.
             */
            int i;
            double nsample;
            double samplepos = 0.0;

            ss_pprintf_4(("bnode_getsamplenodes:less nodes than sample positions\n"));
            nsample = (double)sample_size / (double)naddrs;
            for (i = 0; i < naddrs && (int)samplepos < sample_size; i++) {
                cur_n = dbe_bnode_getreadonly(n->n_go, addrs[i], n->n_bonsaip, &info);
                if (cur_n == NULL) {
                    SsDbgMessage("Bad index block address %ld found in index leaf at addr %ld, level %d\n",
                        (long)addrs[i], (long)n->n_addr, (int)n->n_level);
                    return;
                }
                /* Recurse back to dbe_bnode_getkeysamples. */
                dbe_bnode_getkeysamples(
                    cur_n,
                    range_min,
                    range_max,
                    sample_vtpl + (int)samplepos,
                    (int)nsample,
                    mergep);
                dbe_bnode_write(cur_n, FALSE);
                samplepos += nsample;
            }
        }

        SsMemFree(addrs);
}

/* JarmoP 310399 */
static void bnode_getsamplenodes_rnd(
        dbe_bnode_t* n,
        dbe_bkey_t* range_min,
        dbe_bkey_t* range_max,
        dynvtpl_t* sample_vtpl,
        int sample_size,
        bool mergep)
{
        long* addrs;
        int naddrs;
        dbe_bnode_t* cur_n;
        int last_index = -1;
        dbe_info_t info;

        dbe_info_init(info, 0);

        ss_pprintf_3(("bnode_getsamplenodes_rnd:sample_size=%d, node=%ld, level=%d\n",
            sample_size, n->n_addr, n->n_level));
        ss_dassert(sample_size > 0);
        ss_assert(dbe_estrndnodesp);

        /* Read all node addresses inside the range.
         */
        bnode_getrangeaddrs(
            n,
            range_min,
            range_max,
            &addrs,
            &naddrs);

        ss_pprintf_4(("bnode_getsamplenodes_rnd:naddrs=%d\n", naddrs));

        if (naddrs == 0) {
            return;
        }

        if (naddrs >= sample_size) {
            /* There are more or equal number of nodes than
             * sample positions.
             */
            double d;
            double inc;
            double last;
            int samplepos;
            int step;

            ss_pprintf_4(("bnode_getsamplenodes_rnd:more nodes than sample positions\n"));
            step = naddrs / sample_size;
            step = step / 2;
            inc = (double)naddrs / (double)sample_size;
            last = (double)naddrs;
            samplepos = 0;
            for (d = 0.0; d < last && samplepos < sample_size; d += inc) {
                int index;

                /* Index is random between zero and half step. */
                index = su_rand_long(&rnd) % step + 1;
                if ((su_rand_long(&rnd) % 2) == 0) {
                    /* Half steps adjust backwards. */
                    index = -index;
                }
                index = (int)d + index;
                if (index <= last_index) {
                    /* Same address only once. */
                    index = last_index + 1;
                }
                if (index < naddrs) {
                    ss_bassert(index >= 0);
                    last_index = index;
                    ss_pprintf_4(("bnode_getsamplenodes_rnd:index=%d\n", index));
                    cur_n = dbe_bnode_getreadonly(n->n_go, addrs[index], n->n_bonsaip, &info);
                    if (cur_n == NULL) {
                        SsDbgMessage("Bad index block address %ld found in index leaf at addr %ld, level %d\n",
                            (long)addrs[index], (long)n->n_addr, (int)n->n_level);
                        return;
                    }
                    /* Recurse back to dbe_bnode_getkeysamples. */
                    dbe_bnode_getkeysamples(
                        cur_n,
                        range_min,
                        range_max,
                        sample_vtpl + samplepos,
                        1,
                        mergep);
                    dbe_bnode_write(cur_n, FALSE);
                    samplepos++;
                }
            }

        } else {
            /* There are less nodes than sample positions.
             */
            int i;
            double nsample;
            double samplepos = 0.0;

            ss_pprintf_4(("bnode_getsamplenodes_rnd:less nodes than sample positions\n"));
            nsample = (double)sample_size / (double)naddrs;
            for (i = 0; i < naddrs && (int)samplepos < sample_size; i++) {
                cur_n = dbe_bnode_getreadonly(n->n_go, addrs[i], n->n_bonsaip, &info);
                if (cur_n == NULL) {
                    SsDbgMessage("Bad index block address %ld found in index leaf at addr %ld, level %d\n",
                        (long)addrs[i], (long)n->n_addr, (int)n->n_level);
                    return;
                }
                /* Recurse back to dbe_bnode_getkeysamples. */
                dbe_bnode_getkeysamples(
                    cur_n,
                    range_min,
                    range_max,
                    sample_vtpl + (int)samplepos,
                    (int)nsample,
                    mergep);
                dbe_bnode_write(cur_n, FALSE);
                samplepos += nsample;
            }
        }

        SsMemFree(addrs);
}

/*#***********************************************************************\
 *
 *		bnode_getsamplevalues
 *
 * Gets the actual sample values from the leaf nodes. If sample_size is
 * relatively large, several samples are taken from the leaf. Otherwise
 * only one sample is taken from the leaf.
 *
 * Parameters :
 *
 *	n - in
 *
 *
 *	kmin - in
 *
 *
 *	kmax - in
 *
 *
 *	sample_vtpl - use
 *
 *
 *	sample_size - in
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void bnode_getsamplevalues(
        dbe_bnode_t* n,
        dbe_bkey_t* kmin,
        dbe_bkey_t* kmax,
        dynvtpl_t* sample_vtpl,
        int sample_size,
        bool mergep)
{
        int i;
        int cmp;
        int count;
        char* keys;
        dbe_bkey_t* k;
        vtpl_t* vtpl;
        int sample_pos;
        dbe_bkey_search_t ks;
        int accept_limit;

        ss_pprintf_3(("bnode_getsamplevalues:sample_size=%d, node=%ld, keycount=%d\n",
            sample_size, n->n_addr, n->n_count));
        ss_dassert(n != NULL);
        ss_dassert(mergep || dbe_bnode_getlevel(n) == 0);
        ss_dassert(!mergep || dbe_bnode_getlevel(n) <= 1);
        ss_dassert(sample_size > 0);
        SS_PUSHNAME("bnode_getsamplevalues");

        if (mergep) {
            accept_limit = 10;
        } else {
            accept_limit = 3;
        }

        count = n->n_count;
        keys = n->n_keys;

        if (count == 0 || (mergep && count < accept_limit)) {
            /* Empty node or too few keys for merge. */
            ss_pprintf_4(("bnode_getsamplevalues:empty node\n"));
            SS_POPNAME;
            return;
        }

        k = dbe_bkey_init(n->n_go->go_bkeyinfo);

        /* Search the range start position.
         */
        dbe_bkey_search_init(&ks, kmin, DBE_BKEY_CMP_VTPL);
        for (i = 0; i < count; i++) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int offset;
                bnode_getkeyoffset(n, i, offset);
                keys = &n->n_keys[offset];
            }
            dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
            if (cmp <= 0) {
                break;
            }
            if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            }
        }

        if (i == count) {
            /* No acceptable keys found.
             */
            dbe_bkey_done(k);
            ss_pprintf_4(("bnode_getsamplevalues:no acceptable keys found\n"));
            SS_POPNAME;
            return;
        }

        dbe_bkey_expand(k, kmin, (dbe_bkey_t*)keys);

        if (dbe_bkey_compare(k, kmax) >= 0) {
            /* Empty range, the found key does not belong to the range.
             */
            dbe_bkey_done(k);
            ss_pprintf_4(("bnode_getsamplevalues:empty range\n"));
            SS_POPNAME;
            return;
        }

        if (sample_size < accept_limit) {
            /* Take only one key value from this leaf.
             */
            ss_pprintf_4(("bnode_getsamplevalues:small sample_size, save only one key value\n"));
            vtpl = dbe_bkey_getvtpl(k);
            dynvtpl_setvtpl(&sample_vtpl[0], vtpl);

        } else {
            /* There are several places available in the sample_vtpl.
             * Save more than one key value from this leaf to the
             * sample_vtpl.
             */
            int accept_count;
            int accept_interval;
            int saved_i;
            char* saved_keys;

            /* Save current position. */
            saved_i = i;
            saved_keys = keys;

            /* Start range end search. */
            dbe_bkey_search_init(&ks, kmax, DBE_BKEY_CMP_VTPL);
            dbe_bkey_search_step(ks, k, cmp);
            ss_dassert(cmp > 0);

            /* Skip current key. */
            if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            }
            i++;

            /* Search range end key and count key values inside the range.
             */
            for (accept_count = 1; i < count; i++, accept_count++) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    int offset;
                    bnode_getkeyoffset(n, i, offset);
                    keys = &n->n_keys[offset];
                }
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
            }

            if (accept_count <= accept_limit) {
                /* Take only one key value from this leaf.
                 * JarmoR Oct 4, 2000:
                 * Removed condition " || accept_count < sample_size"
                 * from if statement.
                 */
                ss_pprintf_4(("bnode_getsamplevalues:too few accepted keys (%d), save only one key\n",
                    accept_count));
                vtpl = dbe_bkey_getvtpl(k);
                dynvtpl_setvtpl(&sample_vtpl[0], vtpl);

            } else {
                /* Take several key values from this leaf.
                 */
                accept_interval = accept_count / sample_size;
                if (accept_interval < accept_limit) {
                    accept_interval = accept_limit;
                }
                ss_pprintf_4(("bnode_getsamplevalues:accept_count = %d, accept_interval = %d\n",
                    accept_count, accept_interval));

                /* Restore starting position. */
                keys = saved_keys;
                i = saved_i;
                sample_pos = 0;
                for (; accept_count-- > 0 && i < count-1; i++) {
                    if (i % accept_interval == 0) {
                        /* Save current key to sample_vtpl. */
                        vtpl = dbe_bkey_getvtpl(k);
                        dynvtpl_setvtpl(&sample_vtpl[sample_pos++], vtpl);
                        if (sample_pos == sample_size) {
                            break;
                        }
                    }
                    if (n->n_info & BNODE_MISMATCHARRAY) {
                        int offset;
                        bnode_getkeyoffset(n, i + 1, offset);
                        keys = &n->n_keys[offset];
                    } else {
                        keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                    }
                    dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
                }
                ss_pprintf_4(("bnode_getsamplevalues:saved %d samples\n", sample_pos));
            }
        }

        dbe_bkey_done(k);

        SS_POPNAME;
}

static void bnode_getsamplevalues_rnd(
        dbe_bnode_t* n,
        dbe_bkey_t* kmin,
        dbe_bkey_t* kmax,
        dynvtpl_t* sample_vtpl,
        int sample_size)
{
        int i;
        int cmp;
        int count;
        char* keys;
        dbe_bkey_t* k;
        vtpl_t* vtpl;
        int sample_pos;
        dbe_bkey_search_t ks;
        int kpos;

        ss_pprintf_3(("bnode_getsamplevalues_rnd:sample_size=%d, node=%ld, keycount=%d\n",
            sample_size, n->n_addr, n->n_count));
        ss_dassert(n != NULL);
        ss_dassert(dbe_bnode_getlevel(n) == 0);
        ss_dassert(sample_size > 0);
        ss_assert(dbe_estrndkeysp);
        SS_PUSHNAME("bnode_getsamplevalues_rnd");

        count = n->n_count;
        keys = n->n_keys;

        if (count == 0) {
            /* Empty node. */
            ss_pprintf_4(("bnode_getsamplevalues_rnd:empty node\n"));
            SS_POPNAME;
            return;
        }

        k = dbe_bkey_init(n->n_go->go_bkeyinfo);

        dbe_bkey_search_init(&ks, kmin, DBE_BKEY_CMP_VTPL);

        /* Search the range start position.
         */
        for (i = 0; i < count; i++) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, i, kpos);
                keys = &n->n_keys[kpos];
            }
            dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
            if (cmp <= 0) {
                break;
            }
            if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            }
        }

        if (i == count) {
            /* No acceptable keys found.
             */
            dbe_bkey_done(k);
            ss_pprintf_4(("bnode_getsamplevalues_rnd:no acceptable keys found\n"));
            SS_POPNAME;
            return;
        }

        dbe_bkey_expand(k, kmin, (dbe_bkey_t*)keys);

        if (dbe_bkey_compare(k, kmax) >= 0) {
            /* Empty range, the found key does not belong to the range.
             */
            dbe_bkey_done(k);
            ss_pprintf_4(("bnode_getsamplevalues_rnd:empty range\n"));
            SS_POPNAME;
            return;
        }

        if (sample_size < 10) {
            /* Take only one key value from this leaf.
             */
            ss_pprintf_4(("bnode_getsamplevalues_rnd:small sample_size, save only one key value\n"));
            sample_size = 1;
        }
        {
            /* There are several places available in the sample_vtpl.
             * Save more than one key value from this leaf to the
             * sample_vtpl.
             */
            int accept_count;
            int accept_interval;
            int saved_i;
            char* saved_keys;

            /* Save current position. */
            saved_i = i;
            saved_keys = keys;

            /* Start range end search. */
            dbe_bkey_search_init(&ks, kmax, DBE_BKEY_CMP_VTPL);
            dbe_bkey_search_step(ks, k, cmp);
            ss_dassert(cmp > 0);

            /* Skip current key. */
            if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            }
            i++;

            /* Search range end key and count key values inside the range.
             */
            for (accept_count = 1; i < count; i++, accept_count++) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, i, kpos);
                    keys = &n->n_keys[kpos];
                }
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
            }

            if (accept_count <= 10) {
                /* Take only one key value from this leaf.
                 */
                ss_pprintf_4(("bnode_getsamplevalues_rnd:too few accepted keys (%d), save only one key\n",
                    accept_count));
                sample_size = 1;
            }
            {
                /* Take sample_size values from this leaf.
                 */
                int accept_pos;
                ss_debug(int last_accept_pos = -1;)

                if (accept_count < sample_size) {
                    accept_interval = sample_size / accept_count;
                } else {
                    accept_interval = accept_count / sample_size;
                }
                if (accept_interval < 10 && sample_size > 1) {
                    accept_interval = 10;
                }
                ss_pprintf_4(("bnode_getsamplevalues_rnd:accept_count=%d, accept_interval=%d\n",
                    accept_count, accept_interval));

                /* Restore starting position. */
                keys = saved_keys;
                i = saved_i;
                sample_pos = 0;
                if (accept_interval <= 1) {
                    /* Avoid division by zero. */
                    ss_dassert(accept_interval > 0);
                    accept_pos = i + accept_interval;
                } else {
                    accept_pos = i + su_rand_long(&rnd) % (accept_interval/2);
                }
                ss_pprintf_4(("bnode_getsamplevalues_rnd:next accept_pos=%d\n", accept_pos));
                for (; accept_count > 0 && i < count-1; i++) {
                    bool accept_key;
                    accept_key = (i == accept_pos);
                    if (accept_key) {
                        /* Save current key to sample_vtpl. */
                        ss_pprintf_4(("bnode_getsamplevalues_rnd:accept_pos=%d\n", accept_pos));
                        ss_dassert(last_accept_pos != accept_pos);
                        ss_debug(last_accept_pos = accept_pos);
                        vtpl = dbe_bkey_getvtpl(k);
                        dynvtpl_setvtpl(&sample_vtpl[sample_pos++], vtpl);
                        if (sample_pos == sample_size) {
                            break;
                        }
                        accept_pos = su_rand_long(&rnd) % accept_interval + 1;
                        accept_pos = i + accept_interval/2 + accept_pos;
                        accept_count--;
                        ss_pprintf_4(("bnode_getsamplevalues_rnd:next accept_pos=%d\n", accept_pos));
                    }
                    if (n->n_info & BNODE_MISMATCHARRAY) {
                        bnode_getkeyoffset(n, i + 1, kpos);
                        keys = &n->n_keys[kpos];
                    } else {
                        keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                    }
                    dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
                }
                ss_pprintf_4(("bnode_getsamplevalues_rnd:saved %d samples\n", sample_pos));
            }
        }

        dbe_bkey_done(k);

        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getkeysamples
 *
 * Gets key value v-tuple samples from range [range_min, range_max].
 * At most sample_size sampels are taken.
 *
 * Parameters :
 *
 *	n - in
 *
 *
 *	range_min - in
 *
 *
 *	range_max - in
 *
 *
 *	sample_vtpl - use
 *
 *
 *	sample_size - in
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_bnode_getkeysamples(
        dbe_bnode_t* n,
        dbe_bkey_t* range_min,
        dbe_bkey_t* range_max,
        dynvtpl_t* sample_vtpl,
        int sample_size,
        bool mergep)
{
        ss_dassert(n != NULL);
        ss_dprintf_1(("dbe_bnode_getkeysamples:addr = %ld, node level = %d, sample size = %d\n",
            n->n_addr, n->n_level, sample_size));
        ss_dassert(sample_size > 0);

        if (sample_size <= 0) {
            /* For defensive programming, because this should never happen. */
            return;
        }

        if ((!mergep && n->n_level > 0) || (mergep && n->n_level > 1)) {

            if (dbe_estrndnodesp && !mergep) {
                bnode_getsamplenodes_rnd(
                    n,
                    range_min,
                    range_max,
                    sample_vtpl,
                    sample_size,
                    mergep);
            } else {
                bnode_getsamplenodes(
                    n,
                    range_min,
                    range_max,
                    sample_vtpl,
                    sample_size,
                    mergep);
            }
        } else {
            if (dbe_estrndkeysp && !mergep) {
                bnode_getsamplevalues_rnd(
                    n,
                    range_min,
                    range_max,
                    sample_vtpl,
                    sample_size);
            } else {
                bnode_getsamplevalues(
                    n,
                    range_min,
                    range_max,
                    sample_vtpl,
                    sample_size,
                    mergep);
            }
        }
}

#endif /* SS_NOESTSAMPLES */

/*#***********************************************************************\
 *
 *		bnode_split_findrange
 *
 * Finds range start and end positions in the keypos array.
 * Used to select the position range from where the shortest
 * key value is selected as a split key in bnode_split.
 * The range is given as percentages from 'nodelen'.
 *
 * Parameters :
 *
 *	keypos - in, use
 *		array of key position info, lenght is given in keyposcount
 *
 *	keyposcount - in
 *		size of keypos array
 *
 *	nodelen - in
 *		node length
 *
 *	minsplitpos - in
 *		min split position in percentages
 *
 *	maxsplitpos - in
 *		max split position in percentages
 *
 *	p_range_start - out
 *		the range start index is stored into *p_range_start
 *
 *	p_range_end - out
 *		the range end index is stored into *p_range_start
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void bnode_split_findrange(
        bnode_keypos_t* keypos,
        uint keyposcount,
        uint nodelen,
        uint minsplitpos,
        uint maxsplitpos,
        uint* p_range_start,
        uint* p_range_end)
{
        uint i;
        uint pos;

        /* Calculate a guess from where the search is started
           to find range start index. This is done just to make the
           search (hopefully) more efficient. */
        i = (uint)((long)minsplitpos * (long)keyposcount / 100L);

        /* Find range start index.
        */
        pos = (uint)((ulong)minsplitpos * (ulong)nodelen / 100L);
        if (keypos[i].kp_pos < pos) {
            /* The guess is too small, search forwards. */
            while (i < keyposcount && keypos[i].kp_pos < pos) {
                i++;
            }
            *p_range_start = i - 1;
        } else {
            /* The guess is too large, search backwards. */
            while (i > 0 && keypos[i].kp_pos > pos) {
                i--;
            }
            if (i == 0) {
                *p_range_start = 1;
            } else {
                *p_range_start = i;
            }
        }

        /* Find range end index.
        */
        i = *p_range_start;
        pos = (uint)((long)maxsplitpos * (long)nodelen / 100L);
        while (i < keyposcount && keypos[i].kp_pos < pos) {
            i++;
        }
        if (i > keyposcount - 1) {
            *p_range_end = keyposcount - 1;
        } else {
            *p_range_end = i;
        }
}

/*#***********************************************************************\
 *
 *		bnode_split_findsplitkey
 *
 * Finds the split key inside the given range or from an absolute
 * index.
 *
 * Parameters :
 *
 *	n - in, use
 *		node that is split
 *
 *	nsi - out, give
 *		node split info structure that is filled here
 *
 *	keypos - in, use
 *		array of key position info
 *
 *	absolute_split - in
 *		if TRUE, range is not used and the split is done at absolute
 *		index given in *p_split_index
 *
 *	range_start - in
 *		split position range start in percentages from node length
 *
 *	range_end - in
 *		split position range end in percentages from node length
 *
 *	optimal_split_pos - in
 *		Position that is optimal for the split.
 *
 *	p_split_k - out, give
 *		full version of split key is allocated and stored into
 *		*p_split_k
 *
 *	p_keys - out, ref
 *		pointer to split key in node n is stored into *p_keys
 *
 *	p_split_pos - out
 *		split position in bytes is stored inyo *p_split_pos, it
 *		is the byte index of split key in node n
 *
 *	p_split_index - in out
 *		key value index of the split key is stored into *p_split_index,
 *		if absolute_split is TRUE, *p_split_index contains the index
 *		of the absolute split index
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void bnode_split_findsplitkey(
        rs_sysi_t* cd,
        dbe_bnode_t* n,
        dbe_bnode_splitinfo_t* nsi,
        bnode_keypos_t* keypos,
        bool absolute_split,
        uint range_start,
        uint range_end,
        uint optimal_split_pos,
        dbe_bkey_t** p_split_k,
        char** p_keys,
        uint* p_split_pos,
        uint* p_split_index)
{
        int i;                  /* loop index */
        char* keys;             /* current key position in the node, points
                                   to the compressed split key inside node */
        int split_key_minlen;   /* min split key len in split range  */
        int split_key_distance; /* distance of split_key_minlen from the
                                   optimal split position*/
        int split_index;        /* final split key index in keypos */
        int split_pos;          /* split key position in the node */
        dbe_bkey_t* prev_k;     /* full key before split key */
        dbe_bkey_t* split_k;    /* full split key, uncompressed form of key
                                   pointer by variable keys */

        ss_dprintf_3(("bnode_split_findsplitkey\n"));
        CHK_BNODE(n);
        SS_PUSHNAME("bnode_split_findsplitkey");
        ss_dassert(!(n->n_info & BNODE_MISMATCHARRAY));

        /* Find the split key inside the range.
         */
        if (absolute_split) {
            ss_assert(*p_split_index > 0);
            split_key_minlen = keypos[*p_split_index].kp_len;
            split_index = *p_split_index;
            split_pos = keypos[*p_split_index].kp_pos;
        } else {
            ss_assert(range_start > 0);
            split_index = range_start;
            split_pos = keypos[range_start].kp_pos;
            split_key_minlen = keypos[range_start].kp_len;
            if (split_pos < optimal_split_pos) {
                split_key_distance = optimal_split_pos - split_pos;
            } else {
                split_key_distance = split_pos - optimal_split_pos;
            }
            for (i = range_start + 1; i <= range_end; i++) {
                if (keypos[i].kp_len < split_key_minlen) {
                    /* Smallest split so far.
                     */
                    split_key_minlen = keypos[i].kp_len;
                    split_index = i;
                    split_pos = keypos[i].kp_pos;
                    if (split_pos < optimal_split_pos) {
                        split_key_distance = optimal_split_pos - split_pos;
                    } else {
                        split_key_distance = split_pos - optimal_split_pos;
                    }
                } else if (keypos[i].kp_len == split_key_minlen) {
                    /* Same as smallest so far, check if this is closer to
                     * the optimal split position.
                     */
                    int distance;
                    if (keypos[i].kp_pos < optimal_split_pos) {
                        distance = optimal_split_pos - keypos[i].kp_pos;
                    } else {
                        distance = keypos[i].kp_pos - optimal_split_pos;
                    }
                    if (distance < split_key_distance) {
                        /* This is closer to the optimal split position,
                         * change this position to the split position.
                         */
                        split_index = i;
                        split_pos = keypos[i].kp_pos;
                        split_key_distance = distance;
                    }
                }
            }
        }
        ss_dprintf_3(("split_index = %d, split_pos = %d\n", split_index, split_pos));
        ss_assert(split_index > 0);
        ss_assert(split_index < n->n_count);

        /* Get the full split key into variable split_k.
        */
        prev_k = dbe_bkey_init_ex(cd, n->n_go->go_bkeyinfo);
        split_k = dbe_bkey_init_ex(cd, n->n_go->go_bkeyinfo);
        keys = n->n_keys;
        dbe_bkey_copy(prev_k, (dbe_bkey_t*)keys);
        for (i = 0; i < split_index-1; i++) {
            keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            dbe_bkey_expand(prev_k, prev_k, (dbe_bkey_t*)keys);
        }
        keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
        dbe_bkey_expand(split_k, prev_k, (dbe_bkey_t*)keys);
        ss_dassert(keys == n->n_keys + split_pos);
        ss_dassert(dbe_bkey_compare(prev_k, split_k) < 0);

        if (n->n_level == 0) {
            bool use_findsplit = TRUE;

            /* Split an index node. First check if tail compression can
               be used for the key that is used as a separator key between
               the two nodes.
            */
            if (dbe_bkey_equal_vtpl(prev_k, split_k)) {
                /* Keys are equal, cannot use dbe_bkey_findsplit. Try
                 * the next key, if possible.
                 */
                if (split_index < n->n_count - 1 && !absolute_split) {
                    /* Move to the next key. This cannot be done if
                     * absolute_split is set to TRUE.
                     */
                    dbe_bkey_copy(prev_k, split_k);
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                    dbe_bkey_expand(split_k, prev_k, (dbe_bkey_t*)keys);
                    split_index++;
                    split_pos = (int)(keys - n->n_keys);
                    ss_dprintf_3(("new split_index = %d, split_pos = %d\n", split_index, split_pos));
                }
                if (dbe_bkey_equal_vtpl(prev_k, split_k)) {
                    /* Keys are still equal, use split_k as a split key. */
                    use_findsplit = FALSE;
                }
            }
            if (use_findsplit) {
                /* Calculate the smallest possible key between prev_k and
                   split_k and use it as the separator key in higher level
                   nodes between the two nodes. This is a form of tail
                   compression. */
                nsi->ns_k = dbe_bkey_findsplit(
                                cd,
                                n->n_go->go_bkeyinfo,
                                prev_k,
                                (dbe_bkey_t*)keys);
                if (dbe_cfg_splitpurge) {
                    /* Try to keep delete and insert on the sama page. */
                    dbe_bkey_setdeletemark(nsi->ns_k);
                }
            } else {
                /* Use the full split_k as a separator key between the
                   two nodes. */
                nsi->ns_k = dbe_bkey_initsplit(cd, n->n_go->go_bkeyinfo, split_k);
            }
            nsi->ns_level = n->n_level + 1;

#ifdef NEW_SPLITKEY
            if (use_findsplit) {
                ss_aassert(dbe_bkey_compare(nsi->ns_k, split_k) < 0);
            } else {
                ss_aassert(dbe_bkey_compare(nsi->ns_k, split_k) <= 0);
            }
#else
            ss_dassert(dbe_bkey_compare(prev_k, nsi->ns_k) < 0);
            ss_dassert(dbe_bkey_compare(nsi->ns_k, split_k) <= 0);
#endif

        } else {

            /* In higher level nodes (index nodes) the full key values
               must be used as split keys, tail compression cannot be used.
            */
            nsi->ns_k = dbe_bkey_initsplit(cd, n->n_go->go_bkeyinfo, split_k);
            nsi->ns_level = n->n_level + 1;
        }
        dbe_bkey_done_ex(cd, prev_k);

        /* Set the output parameters. */
        *p_split_k = split_k;
        *p_keys = keys;
        *p_split_pos = split_pos;
        *p_split_index = split_index;

        SS_POPNAME;
}

/*#**********************************************************************\
 *
 *		bnode_split
 *
 * Splits a node. The node is split at the middle key value. The split
 * position is calculated in bytes from the start of the node.
 *
 * Parameters :
 *
 *	n - in out, use
 *		node pointer
 *
 *	nsi - out, use
 *		pointer to a structure onto which the split information is
 *          stored
 *
 *      newk - in, use
 *          new key that will be added after the split, it is used
 *          to ensure that there is enough space for the key in the
 *          node after the split
 *
 *      p_newktooldnode - out
 *          If the newk should be inserted to the old node, TRUE is
 *          set to *p_newktooldnode. If the newk should be inserted
 *          to the new node resulted from the split, FALSE is set to
 *          *p_newktooldnode.
 *
 *      p_seqins - out
 *          If there is sequential insert flag set in the node, TRUE
 *          is set into *p_seqins or FALSE otherwise.
 *
 *      p_rc - out
 *          Split error case if returned node is NULL.
 *
 * Return value - give :
 *
 *      new node created in split, or
 *      NULL if split failed
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_bnode_t* bnode_split(
        rs_sysi_t* cd,
        dbe_bnode_t* n,
        dbe_bnode_splitinfo_t* nsi,
        dbe_bkey_t* newk,
        bool* p_newktooldnode,
        bool* p_seqins,
        dbe_ret_t* p_rc,
        dbe_info_t* info)
{
        dbe_bnode_t* nn;        /* new node created as a result of the split */
        uint i;                 /* loop index */
        uint count;             /* temporary copy of n->n_count */
        char* keys;             /* current key position in the node, points
                                   to the compressed split key inside node */
        bnode_keypos_t* keypos; /* key position and length info array */
        uint range_start = -1;  /* index of split range start in keypos */
        uint range_end = -1;    /* index of split range end in keypos */
        uint split_index = 0;   /* final split key index in keypos */
        uint split_pos;         /* split key position in the node */
        dbe_bkey_t* split_k;    /* full split key, uncompressed form of key
                                   pointed by variable keys */
        int copylen;            /* number of bytes copied to the new node
                                   from the old node */
        int compr_split_klen;   /* length of compressed split key pointed
                                   by variable keys */
        int full_split_klen;    /* length of full split key at split_k */
        uint minsplitpos;       /* minimum split position in percentages */
        uint maxsplitpos;       /* maximum split position in percentages */
        uint nlen;              /* (old) node length after split */
        uint nnlen;             /* new node length */
        uint newklen;           /* length of parameter newk */
        uint newnodelen;
        uint oldnodelen;
        int direction_changed;  /* Counter use to count the number of
                                   direction changes of which leaf the newk
                                   should be inserted. Used when the key does
                                   not fit into a leaf after the initial
                                   split. */
        bool first_time;        /* TRUE when split loop is executed for the
                                   first time. */
        bool absolute_split;    /* If TRUE, split is done at absolute split
                                   index instead of an index inside a range. */
        bool is_index_node;     /* TRUE if n is an index node and FALSE
                                   if n is a leaf node */
        int optimal_split_pos = -1; /* calculated optimal split position */
        bool inherit_seqinscount = TRUE;
        bool b;

        ss_trigger("bnode_split");

        CHK_BNODE(n);
        ss_dprintf_3(("dbe node split, addr = %ld, len = %d\n", n->n_addr, n->n_len));
        ss_assert(n->n_count > 1);
        ss_dassert(p_seqins != NULL);
        ss_dassert(p_rc != NULL);
        ss_dassert(!(n->n_info & BNODE_MISMATCHARRAY));

        if (BNODE_ISSEQINS(n)) {
            *p_seqins = TRUE;
            nn = bnode_create_seq(n->n_go, n->n_addr, n->n_bonsaip, p_rc, info);
        } else {
            *p_seqins = FALSE;
            nn = dbe_bnode_create(n->n_go, n->n_bonsaip, p_rc, info);
        }
        if (nn == NULL) {
            return(NULL);
        }

        n->n_go->go_splitcount++;

        keys = n->n_keys;
        count = n->n_count;

        /* Generate key position info array.
         */
        is_index_node = n->n_level > 0;
        keypos = SsMemAlloc(sizeof(keypos[0]) * n->n_count);
        for (i = 0; i < count; i++) {
            uint klen;
            klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
            keypos[i].kp_pos = (uint)(keys - n->n_keys);
            if (is_index_node) {
                /* The key length is set to kp_len, because the shortest
                 * key is selected as the split key.
                 */
                keypos[i].kp_len = dbe_bkey_getmismatchindex((dbe_bkey_t*)keys)
                                   + klen;
            } else {
                /* The equal part with the previous key is set to kp_len,
                 * because the key with shortest common part with the
                 * previous key value is selected as the split key. This
                 * split key selection generates the shortest key to the
                 * higher level node.
                 */
                keypos[i].kp_len = dbe_bkey_getmismatchindex((dbe_bkey_t*)keys);
            }
            keys += klen;
            ss_dprintf_3(("keypos[%d]: pos = %d, len = %d\n",
                i, keypos[i].kp_pos, keypos[i].kp_len));
        }

        absolute_split = FALSE;

        /* Find the position range for the node split.
         */
        if (n->n_count < LARGEKEYS_TRESHOLD) {
            /* Select just the shortest key value in the node. */
            minsplitpos = 0;
            maxsplitpos = 100;
            optimal_split_pos = ((long)n->n_len * (long)NORMAL_SPLITPOS)
                                / 100L;
        } else {
            if (BNODE_ISSEQINS(n)) {
                minsplitpos = SEQINSCOUNT_MINSPLITPOS;
                maxsplitpos = SEQINSCOUNT_MAXSPLITPOS;
                if (n->n_lastinsindex < n->n_count - 1) {
                    split_index = n->n_lastinsindex + 1;
                    absolute_split = TRUE;
                } else {
                    optimal_split_pos = ((long)n->n_len * (long)SEQINSCOUNT_SPLITPOS)
                                        / 100L;
                }
                n->n_seqinscount = 0;
                inherit_seqinscount = TRUE;
            } else {
                minsplitpos = NORMAL_MINSPLITPOS;
                maxsplitpos = NORMAL_MAXSPLITPOS;
                optimal_split_pos = ((long)n->n_len * (long)NORMAL_SPLITPOS)
                                    / 100L;
            }
        }

        direction_changed = 0;
        first_time = TRUE;
        newklen = dbe_bkey_getlength(newk);

        for (;;) {
            /* Loop until node is split so that the new key will fit
             * into either half of the split node.
             */
            bool prev_newktooldnode;

            ss_dprintf_3(("split position range %u-%u percentages, optimal_split_pos = %d\n",
                minsplitpos, maxsplitpos, optimal_split_pos));

            if (absolute_split) {
                ss_dprintf_3(("absolute_split at %d\n", split_index));
            } else {
                bnode_split_findrange(
                    keypos,
                    n->n_count,
                    n->n_len,
                    minsplitpos,
                    maxsplitpos,
                    &range_start,
                    &range_end);
                ss_dprintf_3(("range_start = %d, range_end = %d\n",
                    range_start, range_end));
            }

            bnode_split_findsplitkey(
                cd,
                n,
                nsi,
                keypos,
                absolute_split,
                range_start,
                range_end,
                optimal_split_pos,
                &split_k,
                &keys,
                &split_pos,
                &split_index);

            compr_split_klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
            full_split_klen = dbe_bkey_getlength(split_k);

            /* Calculate the number of bytes to be copied to the new node nn
             * from the old node n after split key split_k.
             */
            copylen = n->n_len - split_pos - compr_split_klen;

            /* Calculate lengths of two split nodes. */
            nnlen = full_split_klen + copylen;
            nlen = split_pos;

            if (!first_time) {
                prev_newktooldnode = *p_newktooldnode;
            }

            if (dbe_bkey_compare(newk, nsi->ns_k) < 0) {
                /* Key should be inserted into old node (left half).
                 */
                ss_dprintf_4(("bnode_split:key should be inserted into old node (left half)\n"));
                oldnodelen = BNODE_HEADERLEN + BNODE_DEBUGOFFSET + nlen + newklen + (split_index + 1) * 4;
                newnodelen = BNODE_HEADERLEN + BNODE_DEBUGOFFSET + nnlen + (n->n_count - split_index) * 4;
                *p_newktooldnode = TRUE;
            } else {
                /* Key should be inserted into new node (right half).
                 */
                ss_dprintf_4(("bnode_split:key should be inserted into new node (right half)\n"));
                oldnodelen = BNODE_HEADERLEN + BNODE_DEBUGOFFSET + nlen + split_index * 4;
                newnodelen = BNODE_HEADERLEN + BNODE_DEBUGOFFSET + nnlen + newklen + (n->n_count - split_index + 1) * 4;
                *p_newktooldnode = FALSE;
            }
            ss_dprintf_3(("Store new key to %s half\n",
                *p_newktooldnode ? "left" : "right"));
            ss_dprintf_3(("copylen = %d, nlen = %d, nnlen = %d\n",
                copylen, nlen, nnlen));
            ss_dprintf_3(("newklen = %d, oldnodelen = %d, newnodelen = %d, blocksize = %d\n",
                newklen, oldnodelen, newnodelen, n->n_go->go_idxfd->fd_blocksize));

            if (oldnodelen > n->n_go->go_idxfd->fd_blocksize
                || newnodelen > n->n_go->go_idxfd->fd_blocksize)
            {
                /* New key will not fit into new node. Adjust the split
                 * position.
                 */
                ss_dprintf_3(("Adjust split position, direction_changed = %d\n",
                    direction_changed));
                if (!first_time) {
                    /* Changed to two different if's, possible problem with
                     * purify or compiler.
                     */
                    if (prev_newktooldnode != *p_newktooldnode) {
                        direction_changed++;
                    }
                }

                if (direction_changed == 0) {
                    uint prev_minsplitpos;
                    uint prev_maxsplitpos;

                    prev_minsplitpos = minsplitpos;
                    prev_maxsplitpos = maxsplitpos;

                    if (oldnodelen > n->n_go->go_idxfd->fd_blocksize) {
                        /* Move split position to the left. */
                        ss_dassert(newnodelen <= n->n_go->go_idxfd->fd_blocksize);
                        if (absolute_split) {
                            if (split_index <= 0) {
                                /* Cannot split. */
                                ss_dprintf_3(("absolute split_index %d is too small, cannot split\n", split_index));
                                ss_trigger("bnode_split");
                                *p_rc = DBE_ERR_NODESPLITFAILED;
                                SsMemFree(nn);
                                ss_derror;
                                return(NULL);
                            }
                            split_index--;
                        } else {
                            if (minsplitpos >= SPLITPOS_ADJUST) {
                                minsplitpos -= SPLITPOS_ADJUST;
                            } else {
                                minsplitpos = 0;
                            }
                            if (maxsplitpos > 2 * SPLITPOS_ADJUST) {
                                maxsplitpos -= SPLITPOS_ADJUST;
                            } else {
                                maxsplitpos = maxsplitpos / 2;
                            }
                        }
                    } else {
                        /* Move split position to the right. */
                        ss_dassert(oldnodelen <= n->n_go->go_idxfd->fd_blocksize);
                        if (absolute_split) {
                            if (split_index >= n->n_count - 1) {
                                /* Cannot split. */
                                ss_dprintf_3(("absolute split_index %d is too large, cannot split\n", split_index));
                                ss_trigger("bnode_split");
                                *p_rc = DBE_ERR_NODESPLITFAILED;
                                SsMemFree(nn);
                                ss_derror;
                                return(NULL);
                            }
                            split_index++;
                        } else {
                            if (minsplitpos < 100 - 2 * SPLITPOS_ADJUST) {
                                minsplitpos += SPLITPOS_ADJUST;
                            } else {
                                minsplitpos = minsplitpos + (100 - minsplitpos) / 2;
                            }
                            if (maxsplitpos <= 100 - SPLITPOS_ADJUST) {
                                maxsplitpos += SPLITPOS_ADJUST;
                            } else {
                                maxsplitpos = 100;
                            }
                        }
                    }
                    if (!absolute_split &&
                        minsplitpos == prev_minsplitpos &&
                        maxsplitpos == prev_maxsplitpos) {
                        /* Cannot split. */
                        ss_dprintf_3(("min and max split positions not changed, cannot split\n"));
                        ss_trigger("bnode_split");
                        *p_rc = DBE_ERR_NODESPLITFAILED;
                        SsMemFree(nn);
                        ss_derror;
                        return(NULL);
                    }

                } else if (direction_changed > 1) {

                    /* Direction changed more than once, split failed. */
                    ss_dprintf_3(("direction changed more than once (%d), cannot split\n",
                        direction_changed));
                    ss_trigger("bnode_split");
                    *p_rc = DBE_ERR_NODESPLITFAILED;
                    SsMemFree(nn);
                    ss_derror;
                    return(NULL);

                } else {

                    absolute_split = TRUE;
                    if (*p_newktooldnode) {
                        /* Move split position to the left. */
                        if (split_index <= 1) {
                            /* Cannot split. */
                            ss_dprintf_3(("split_index %d is too small, cannot split\n", split_index));
                            ss_trigger("bnode_split");
                            *p_rc = DBE_ERR_NODESPLITFAILED;
                            SsMemFree(nn);
                            ss_derror;
                            return(NULL);
                        }
                        split_index--;
                    } else {
                        /* Move split position to the right. */
                        if (split_index >= n->n_count - 1) {
                            /* Cannot split. */
                            ss_dprintf_3(("split_index %d is too large, cannot split\n", split_index));
                            ss_trigger("bnode_split");
                            *p_rc = DBE_ERR_NODESPLITFAILED;
                            SsMemFree(nn);
                            ss_derror;
                            return(NULL);
                        }
                        split_index++;
                    }
                }

                /* Clear allocations done in bnode_split_findsplitkey. */
                dbe_bkey_done_ex(cd, split_k);
                dbe_bkey_done_ex(cd, nsi->ns_k);

                /* Retry from the new split position range. */

            } else {

                /* Correct split position found. */
                break;
            }
            first_time = FALSE;
        }
        SsMemFree(keypos);

        /* Create and initialize the new node.
         */
        nn->n_level = n->n_level;
        dbe_bkey_copy((dbe_bkey_t*)nn->n_keys, split_k);
        memcpy(
            &nn->n_keys[full_split_klen],
            keys + compr_split_klen,
            copylen);
        nn->n_len = nnlen;
        nn->n_count = n->n_count - split_index;
        nn->n_dirty = TRUE;
        nn->n_lastinsindex = nn->n_count - 1;
        if (inherit_seqinscount) {
            /* Inherit sequential insert count. */
            nn->n_seqinscount = nn->n_count;
        }
        bnode_info_setmismatcharray(nn, FALSE);

        /* Update old node.
         */
        n->n_len = nlen;
        n->n_count = split_index;
        n->n_dirty = TRUE;
        n->n_lastinsindex = n->n_count - 1;
        bnode_info_setmismatcharray(n, FALSE);
        ss_debug(memset(keys, '\xff', n->n_len - split_pos));

        dbe_bkey_setaddr(nsi->ns_k, nn->n_addr);

        b = bnode_keysearchinfo_init(n);
        ss_dassert(b || !dbe_bnode_usemismatcharray);
        ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0 || !dbe_bnode_usemismatcharray);

        b = bnode_keysearchinfo_init(nn);
        ss_dassert(b || !dbe_bnode_usemismatcharray);
        ss_dassert((nn->n_info & BNODE_MISMATCHARRAY) != 0 || !dbe_bnode_usemismatcharray);

        ss_dprintf_3(("split key:"));
        ss_output_3(dbe_bkey_dprint(3, nsi->ns_k));
        ss_dassert(dbe_bnode_test(n));
        ss_dassert(dbe_bnode_test(nn));
        ss_dassert(bnode_test_split(n, nn));

        ss_dassert(BNODE_HEADERLEN + n->n_len + n->n_count * 4 <= n->n_go->go_idxfd->fd_blocksize);
        ss_dassert(BNODE_HEADERLEN + nn->n_len + nn->n_count * 4 <= nn->n_go->go_idxfd->fd_blocksize);

        dbe_bkey_done_ex(cd, split_k);

        SS_PMON_ADD(SS_PMON_BNODE_NODESPLIT);

        ss_trigger("bnode_split");

        return(nn);
}

static void bnode_reorder(dbe_bnode_t* n, bool init_keysearchinfo)
{
        char* tmp;
        char* p;
        int kpos;
        int len;
        dbe_bkey_t* k;
        int i;

        ss_dprintf_3(("bnode_reorder:n->n_len=%d, n->n_count=%d\n", n->n_len, n->n_count));
        ss_dassert(n->n_info & BNODE_MISMATCHARRAY);

        ss_dprintf_4(("bnode_reorder:recreate n->n_keys\n"));

        tmp = SsMemAlloc(n->n_len);
        p = tmp;
        for (i = 0; i < n->n_count; i++) {
            bnode_getkeyoffset(n, i, kpos);
            k = (dbe_bkey_t*)&n->n_keys[kpos];
            len = dbe_bkey_getlength(k);
            memcpy(p, k, len);
            p += len;
        }
        ss_dprintf_4(("bnode_reorder:new n->n_len %d->%d\n", n->n_len, p - tmp));
        n->n_len = (int)(p - tmp);
        memcpy(n->n_keys, tmp, n->n_len);
        bnode_info_setmismatcharray(n, FALSE);
        if (init_keysearchinfo) {
            bnode_keysearchinfo_init(n);
        }
        SsMemFree(tmp);
}

/*#***********************************************************************\
 *
 *		bnode_splitneeded
 *
 * Checks is node split is needed.
 *
 * Parameters :
 *
 *	n -
 *
 *
 *	nklen -
 *
 *
 *	split_cleanup -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bnode_splitst_t bnode_splitneeded(
        dbe_bnode_t* n,
        uint nklen,
        bool split_cleanup,
        rs_sysi_t* cd)
{
        bnode_splitst_t splitst;
        dbe_ret_t rc;
        long nkeyremoved = 0;
        long nmergeremoved = 0;

        ss_dprintf_3(("bnode_splitneeded:n->n_len=%d, nklen=%d, n->n_count=%d\n", n->n_len, nklen, n->n_count));

        if (!BNODE_SPLITNEEDED(n, nklen)) {
            ss_dprintf_4(("bnode_splitneeded:BNODE_SPLIT_NO\n"));
            return(BNODE_SPLIT_NO);
        }

        if (n->n_bonsaip && n->n_level == 0) {
            ss_dprintf_4(("bnode_splitneeded:call dbe_bnode_cleanup\n"));
            rc = dbe_bnode_cleanup(
                    n,
                    &nkeyremoved,
                    &nmergeremoved,
                    cd,
                    DBE_BNODE_CLEANUP_USER);
            ss_rc_dassert(rc == DBE_RC_SUCC ||
                          rc == DBE_RC_NODERELOCATE ||
                          rc == DBE_RC_NODEEMPTY, rc);
            if (nkeyremoved > 0) {
                ss_dassert(nmergeremoved > 0);
                dbe_gobj_mergeupdate(n->n_go, nkeyremoved, nmergeremoved);
            } else {
                ss_dassert(nmergeremoved == 0);
            }
        }

        if (n->n_info & BNODE_MISMATCHARRAY) {
            bnode_reorder(n, FALSE);
            ss_dassert(!(n->n_info & BNODE_MISMATCHARRAY));
        }

        if (BNODE_SPLITNEEDED(n, nklen + BNODE_DEBUGOFFSET)) {
            ss_dassert(!(n->n_info & BNODE_MISMATCHARRAY));
            splitst = BNODE_SPLIT_YES;
            ss_dprintf_4(("bnode_splitneeded:BNODE_SPLIT_YES\n"));
        } else {
            bnode_keysearchinfo_init(n);
            splitst = BNODE_SPLIT_NO_CLEANUP;
            n->n_go->go_splitavoidcount++;
            ss_dprintf_4(("bnode_splitneeded:BNODE_SPLIT_NO_CLEANUP\n"));
        }

        return(splitst);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_insertkey
 *
 * Inserts a key value into a node. Node is split when necessary.
 *
 * Parameters :
 *
 *	n - in out, use
 *		node pointer
 *
 *	k - in, use
 *		key value which is inserted
 *
 *	split_cleanup - in, use
 *		If TRUE, node cleanup is done before split.
 *
 *      p_isonlydeletemark - out
 *          If it is certain, that key the inserted key value is the only
 *          delete mark in the node, TRUE is set to *p_isonlydeletemark,
 *          Otherwise FALSE is set to *p_isonlydeletemark. If the information
 *          is not needed, p_isonlydeletemark can be NULL.
 *
 *	nsi - out, use
 *		pointer to a node split info into which the split info is
 *          stored in case when a node is split during insert
 *          If this parameter is null, in split case nothing is done.
 *
 * Return value :
 *
 *      DBE_RC_SUCC         - insert ok
 *      DBE_RC_NODESPLIT    - insert ok, node split during insert and
 *                            parameter nsi updated
 *      DBE_RC_NODERELOCATE - the node must be relocateed
 *      DBE_ERR_UNIQUE      - equal key value already exists, nothing
 *                            inserted
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_bnode_insertkey(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        bool split_cleanup,
        bool* p_isonlydeletemark,
        dbe_bnode_splitinfo_t* nsi,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_bkey_t* nk;         /* new key */
        int nkpos;              /* new key position */
        int nklen;              /* new key len */
        dbe_bkey_search_t ks;   /* key search */
        dbe_bnode_t* nn = NULL; /* new node */
        int insindex;
        bnode_splitst_t splitst;
        dbe_ret_t rc;
        bool b;
        char* keys;
        int count;
        int cmp;
        int kindex;
        int index;
        char mismatch;
        char* data;
        va_index_t len;

        ss_pprintf_1(("dbe_bnode_insertkey\n"));
        CHK_BNODE(n);
        ss_dassert(dbe_bkey_test(k));
        ss_dassert((n->n_level > 0 && !dbe_bkey_isleaf(k)) || n->n_level == 0);
        ss_dassert(dbe_bkey_getlength(k) <= dbe_bnode_maxkeylen((uint)n->n_go->go_idxfd->fd_blocksize));
        ss_output_3(dbe_bkey_dprint_ex(3, "dbe_bnode_insertkey:", k));

        if (p_isonlydeletemark != NULL) {
            if (dbe_cfg_singledeletemark) {
                *p_isonlydeletemark = TRUE;
            } else {
                *p_isonlydeletemark = FALSE;
            }
        }

        if (n->n_cpnum != dbe_counter_getcpnum(n->n_go->go_ctr)) {
            /* The node must be relocateed. */
            ss_dprintf_4(("dbe_bnode_insertkey:DBE_RC_NODERELOCATE\n"));
            return(DBE_RC_NODERELOCATE);
        }

        n->n_dirty = TRUE;
        count = n->n_count;

        if (count == 0) {
            /* empty node, insert key as the first key value */
            n->n_count = 1;
            n->n_len = dbe_bkey_getlength(k);
            n->n_seqinscount = 1;
            n->n_lastinsindex = 0;
            dbe_bkey_copy((dbe_bkey_t*)n->n_keys, k);
            b = bnode_keysearchinfo_init(n);
            ss_dassert(b || !dbe_bnode_usemismatcharray);
            ss_dassert(dbe_bnode_test(n));
            ss_dprintf_4(("dbe_bnode_insertkey:DBE_RC_SUCC\n"));
            return(DBE_RC_SUCC);
        }

        if ((n->n_info & BNODE_MISMATCHARRAY) == 0) {
            bnode_keysearchinfo_init(n);
        }

        b = bnode_keysearchinfo_search(
                n,
                k,
                dbe_cfg_singledeletemark ? DBE_BKEY_CMP_DELETE : DBE_BKEY_CMP_ALL,
                &keys,
                &kindex,
                &cmp,
                NULL,
                &ks);

        if (!b) {
            keys = n->n_keys;

            dbe_bkey_search_init(&ks, k, dbe_cfg_singledeletemark ? DBE_BKEY_CMP_DELETE : DBE_BKEY_CMP_ALL);

            /* Search the position into where the key value must be inserted. */
            for (kindex = 0; kindex < count; kindex++) {
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            }
        }

        if (cmp == 0) {

            ss_dprintf_1(("\ndbe_node_insertkey DBE_ERR_UNIQUE\n"));
            ss_dprintf_1(("Key found: kindex = %d, count = %d, level = %d, addr = %ld\n",
                kindex, count, n->n_level, n->n_addr));
            ss_output_1(dbe_bkey_dprint_ex(1, "Key:", k));
            ss_dprintf_1(("Node:\n"));
            ss_output_1(dbe_bnode_print(NULL, n->n_p, n->n_go->go_idxfd->fd_blocksize));
            ss_dprintf_4(("dbe_bnode_insertkey:DBE_ERR_UNIQUE\n"));

            if (dbe_cfg_singledeletemark
                && n->n_level == 0
                && n->n_bonsaip) 
            {
                dbe_bkey_t* old_deletemark;
                old_deletemark = (dbe_bkey_t*)keys;
                if (dbe_bkey_isdeletemark(k) && dbe_bkey_isdeletemark(old_deletemark)) {
                    /* We have found a deletemark for the same key. 
                     */
                    if (dbe_bkey_iscommitted(old_deletemark)) {
                        ss_dprintf_2(("dbe_node_insertkey: Key is committed, we have lost update\n"));
                        ss_dassert(dbe_bnode_test(n));
                        return(DBE_ERR_LOSTUPDATE);
                    } else {
                        /* Check commit status from trxbuf. 
                         */
                        dbe_trxstate_t trxresult;
                        dbe_trxid_t keytrxid;
                        dbe_trxnum_t committrxnum;
                        dbe_trxid_t usertrxid;

                        keytrxid = dbe_bkey_gettrxid(old_deletemark);
                        trxresult = dbe_trxbuf_gettrxstate(
                                        n->n_go->go_trxbuf,
                                        keytrxid,
                                        &committrxnum,
                                        &usertrxid);
                        switch (trxresult) {
                            case DBE_TRXST_BEGIN:
                            case DBE_TRXST_TOBEABORTED:
                            case DBE_TRXST_VALIDATE:
                            case DBE_TRXST_COMMIT:
                                ss_dprintf_2(("dbe_node_insertkey: Old deletemark is not aborted, we have lost update (trxresult=%d)\n", trxresult));
                                ss_dassert(dbe_bnode_test(n));
                                return(DBE_ERR_LOSTUPDATE);
                            case DBE_TRXST_ABORT:
                                /* Same delete mark is aborted, we replace key header
                                 * with our current delete mark.
                                 */
                                ss_dprintf_2(("dbe_node_insertkey: Same delete mark is aborted\n"));
                                dbe_bkey_copyheader(old_deletemark, k);
                                ss_dassert(dbe_bnode_test(n));
                                return(DBE_RC_SUCC);
                            default:
                                ss_rc_error(trxresult);
                        }
                    }
                }
            }
            return(DBE_ERR_UNIQUE_S);
        }

        /* Compress the new key value. */
        nk = dbe_bkey_init_ex(cd, n->n_go->go_bkeyinfo);
        dbe_bkey_search_compress(&ks, nk);
        if (n->n_info & BNODE_MISMATCHARRAY) {
            nkpos = n->n_len;
        } else {
            nkpos = (int)(keys - n->n_keys);
        }
        nklen = dbe_bkey_getlength(nk);
        insindex = kindex;

        splitst = bnode_splitneeded(n, nklen, split_cleanup, cd);

        if (splitst == BNODE_SPLIT_NO_CLEANUP) {
            ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0 || !dbe_bnode_usemismatcharray);
            dbe_bkey_done_ex(cd, nk);
            rc = dbe_bnode_insertkey(n, k, FALSE, p_isonlydeletemark, nsi, cd, info);
            ss_dprintf_4(("dbe_bnode_insertkey:splitst == BNODE_SPLIT_NO_CLEANUP, rc=%d\n", rc));
            return(rc);

        } else if (splitst == BNODE_SPLIT_YES) {
            /* Compressed new key value does not fit into the node. The node
             * must be split.
             */
            dbe_bkey_done_ex(cd, nk);

            if (nsi != NULL) {
                /* Do the actual split.
                 */
                bool newkeytooldnode;
                bool seqins;
                int nofailure = info->i_flags & DBE_INFO_DISKALLOCNOFAILURE;

                /* Can not fail during split. */
                info->i_flags |= DBE_INFO_DISKALLOCNOFAILURE;

                nn = bnode_split(cd, n, nsi, k, &newkeytooldnode, &seqins, &rc, info);

                if (nn == NULL) {

                    ss_dprintf_1(("dbe_node_insertkey: node split failed, rc = %s (%d)\n",
                        su_rc_nameof(rc)));

                    dbe_db_setfatalerror(n->n_go->go_db, rc);
                    ss_dprintf_4(("dbe_bnode_insertkey:node split failed, rc=%d\n", rc));

                    return(rc);
                }

                if (newkeytooldnode) {
                    dbe_fl_seq_flush(nn->n_go->go_idxfd->fd_freelist, nn->n_addr);
                    rc = dbe_bnode_insertkey(n, k, FALSE, p_isonlydeletemark, NULL, cd, info);
                    su_rc_assert(rc == DBE_RC_SUCC, rc);
                } else {
                    dbe_fl_seq_flush(n->n_go->go_idxfd->fd_freelist, n->n_addr);
                    rc = dbe_bnode_insertkey(nn, k, FALSE, p_isonlydeletemark, NULL, cd, info);
                    su_rc_assert(rc == DBE_RC_SUCC, rc);
                }
                if (seqins) {
                    if (newkeytooldnode) {
                        nn->n_lastuse = TRUE;
                    } else {
                        n->n_lastuse = TRUE;
                    }
                }
                dbe_bnode_write(nn, FALSE);
                if (!nofailure) {
                    /* Restore old value (bit cleared) */
                    info->i_flags &= ~DBE_INFO_DISKALLOCNOFAILURE;
                }
            }
            ss_dprintf_4(("dbe_bnode_insertkey:DBE_RC_NODESPLIT\n"));
            return(DBE_RC_NODESPLIT);

        } else {
            vtpl_t* tmp_vtpl;
            va_t* tmp_va;

            ss_dassert((n->n_info & BNODE_MISMATCHARRAY) != 0 || !dbe_bnode_usemismatcharray);

            /* The key will fit into the node.
             */
            if (kindex < count) {
                int next_key_offset;

                /* New key is not the last one in the node. Make
                 * room for the new key and recompress the next key.
                 */
                dbe_bkey_t* next_key;
                dbe_bkey_t* new_next_key;

                ss_dprintf_2(("dbe_bnode_insertkey:not last key, kindex=%d\n", kindex));
                ss_dassert(nkpos <= n->n_len);

                if (n->n_info & BNODE_MISMATCHARRAY) {
                    bnode_getkeyoffset(n, kindex, next_key_offset);
                    next_key = (dbe_bkey_t*)&n->n_keys[next_key_offset];
                } else {
                    next_key = (dbe_bkey_t*)&n->n_keys[nkpos];
                    next_key_offset = nkpos;
                }
                new_next_key = dbe_bkey_init_ex(cd, n->n_go->go_bkeyinfo);

                dbe_bkey_recompress_insert(new_next_key, nk, next_key);

                ss_dprintf_2(("dbe_bnode_insertkey:new_next_key, next_key_offset=%d\n", next_key_offset));
                ss_output_2(dbe_bkey_print_ex(NULL, "dbe_bnode_insertkey:", new_next_key));

                if (n->n_info & BNODE_MISMATCHARRAY) {

                    dbe_bkey_copy(next_key, new_next_key);

                    memmove(
                        n->n_keysearchinfo_array - 4,
                        n->n_keysearchinfo_array,
                        kindex * 4);

                    n->n_keysearchinfo_array -= 4;

                    tmp_vtpl = BKEY_GETVTPLPTR(nk);
                    tmp_va = VTPL_GETVA_AT0(tmp_vtpl);
                    data = va_getdata(tmp_va, &len);
                    if (len != 0) {
                        index = BKEY_LOADINDEX(nk);
                        if (index > 255) {
                            index = 255;
                        }
                        mismatch = *data;
                    } else {
#if 1
                        index = BKEY_LOADINDEX(nk);
                        if (index > 255) {
                            index = 255;
                        }
                        mismatch = 0;
#else
                        /* For NULL field, use previous value. */
                        bnode_getkeysearchinfo(
                            &n->n_keysearchinfo_array[(kindex - 1) * 4],
                            &index,
                            &mismatch);
#endif
                    }
                    ss_dprintf_2(("dbe_bnode_insertkey:new key arrayindex=%d\n", kindex));
                    bnode_setkeysearchinfo(
                        &n->n_keysearchinfo_array[kindex * 4],
                        index,
                        mismatch,
                        n->n_len);

                    tmp_vtpl = BKEY_GETVTPLPTR(new_next_key);
                    tmp_va = VTPL_GETVA_AT0(tmp_vtpl);
                    data = va_getdata(tmp_va, &len);
                    if (len != 0) {
                        index = BKEY_LOADINDEX(new_next_key);
                        if (index > 255) {
                            index = 255;
                        }
                        mismatch = *data;
                    } else {
#if 1

                        index = BKEY_LOADINDEX(new_next_key);
                        if (index > 255) {
                            index = 255;
                        }
                        mismatch = 0;
#else
                        /* For NULL field, use previous value. */
                        bnode_getkeysearchinfo(
                            &n->n_keysearchinfo_array[kindex * 4],
                            &index,
                            &mismatch);
#endif
                    }
                    ss_dprintf_2(("dbe_bnode_insertkey:next key arrayindex=%d\n", kindex + 1));
                    bnode_setkeysearchinfo(
                        &n->n_keysearchinfo_array[(kindex + 1) * 4],
                        index,
                        mismatch,
                        next_key_offset);

                    n->n_len += nklen;

                } else {
                    int gapnow;             /* gap when current key removed */
                    int gapnew;             /* gap needed for the new key and
                                               recompressed next key */

                    gapnow = dbe_bkey_getlength(next_key);
                    gapnew = nklen + dbe_bkey_getlength(new_next_key);

                    /* make room for the new key and recompressed current key */
                    ss_dassert(n->n_len - nkpos - gapnow >= 0);
                    ss_dassert(n->n_len - nkpos - gapnow <= n->n_len);
                    memmove(
                        n->n_keys + nkpos + gapnew,
                        n->n_keys + nkpos + gapnow,
                        n->n_len - nkpos - gapnow);

                    /* copy recompressed next key */
                    dbe_bkey_copy((dbe_bkey_t*)&n->n_keys[nkpos + nklen], new_next_key);
                    n->n_len += (gapnew - gapnow);
                }

                ss_dprintf_4(("dbe_bnode_insertkey:insert key: vtpl len = %d, index = %d\n",
                    vtpl_netlen(dbe_bkey_getvtpl(nk)), dbe_bkey_getmismatchindex(nk)));
                ss_dprintf_4(("dbe_bnode_insertkey:next key: vtpl len = %d, index = %d\n",
                    vtpl_netlen(dbe_bkey_getvtpl(new_next_key)), dbe_bkey_getmismatchindex(new_next_key)));

#ifdef DBE_ONLYDELETEMARK_OPT
                if (!dbe_cfg_singledeletemark
                    && p_isonlydeletemark != NULL 
                    && kindex > 0 
                    && dbe_bkey_isdeletemark(nk)) 
                {
                    /* Inserted key is a delete mark that is not the
                     * first or last key on the node.
                     * The inserted delete mark is the only delete mark in
                     * the leaf if it is not totally compressed (not same
                     * as previous key) and the next key is not a delete
                     * mark that is totally compressed (not same as inserted
                     * delete mark).
                     */
                    bool new_key_totally_compressed;
                    bool next_key_totally_compressed;
                    /* New key is totally compressed if the length is zero.
                     */
                    new_key_totally_compressed =
                        vtpl_netlen(dbe_bkey_getvtpl(nk)) <= 1;
                    /* Next key is totally compressed if the length is zero.
                     */
                    next_key_totally_compressed =
                        vtpl_netlen(dbe_bkey_getvtpl(new_next_key)) <= 1;

                    *p_isonlydeletemark =
                        !new_key_totally_compressed &&
                        !(dbe_bkey_isdeletemark(new_next_key) &&
                          next_key_totally_compressed);

                    ss_dprintf_2(("dbe_bnode_insertkey:*p_isonlydeletemark=%d\n", *p_isonlydeletemark));
                }
#endif /* DBE_ONLYDELETEMARK_OPT */

                dbe_bkey_done_ex(cd, new_next_key);

            } else {

                /* Insert as the last key in the node. */
                ss_dprintf_2(("dbe_bnode_insertkey:insert as a last key\n"));

                if (n->n_info & BNODE_MISMATCHARRAY) {
                    memmove(
                        n->n_keysearchinfo_array - 4,
                        n->n_keysearchinfo_array,
                        n->n_count * 4);
                    n->n_keysearchinfo_array -= 4;
                    ss_dassert(n->n_keys + n->n_len + nklen <= (char*)(n->n_keysearchinfo_array));
                    tmp_vtpl = BKEY_GETVTPLPTR(nk);
                    tmp_va = VTPL_GETVA_AT0(tmp_vtpl);
                    data = va_getdata(tmp_va, &len);
                    if (len != 0) {
                        index = BKEY_LOADINDEX(nk);
                        if (index > 255) {
                            index = 255;
                        }
                        mismatch = *data;
                    } else {
#if 1
                        ss_dprintf_3(("dbe_bnode_insertkey:for NULL field, use zero mismatch byte\n"));
                        index = BKEY_LOADINDEX(nk);
                        if (index > 255) {
                            index = 255;
                        }
                        mismatch = 0;
#else
                        ss_dprintf_3(("dbe_bnode_insertkey:for NULL field, use previous value\n"));
                        bnode_getkeysearchinfo(
                            &n->n_keysearchinfo_array[(n->n_count - 1) * 4],
                            &index,
                            &mismatch);
#endif
                    }
                    bnode_setkeysearchinfo(
                        &n->n_keysearchinfo_array[n->n_count * 4],
                        index,
                        mismatch,
                        n->n_len);
                    ss_dassert(nkpos == n->n_len);
                }
                n->n_len += nklen;
            }

            if (insindex == n->n_lastinsindex + 1) {
                /* Insert was after the last insert to this node. */
                n->n_lastinsindex = insindex;
                n->n_seqinscount++;
            } else {
                n->n_seqinscount = 0;
            }

            ss_dprintf_2(("dbe_bnode_insertkey:new key, offset=%d\n", nkpos));
            ss_output_2(dbe_bkey_print_ex(NULL, "dbe_bnode_insertkey:", nk));

            dbe_bkey_copy((dbe_bkey_t*)&n->n_keys[nkpos], nk);

            n->n_count++;

            dbe_bkey_done_ex(cd, nk);

            ss_dassert(dbe_bnode_test(n));
            ss_dprintf_4(("dbe_bnode_insertkey:DBE_RC_SUCC\n"));

            return(DBE_RC_SUCC);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bnode_insertkey_block
 *
 *
 *
 * Parameters :
 *
 *	n -
 *
 *
 *	k -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_bnode_insertkey_block(
        dbe_bnode_t* n,
        dbe_bkey_t* k)
{
        ss_dprintf_1(("dbe_bnode_insertkey_block\n"));
        CHK_BNODE(n);
        ss_dassert((n->n_level > 0 && !dbe_bkey_isleaf(k)) || n->n_level == 0);

        return(dbe_bnode_insertkey(n, k, FALSE, NULL, NULL, NULL, NULL));
}

/*#***********************************************************************\
 *
 *		bnode_deletekey_atpos
 *
 * Deletes a key value from the given position in the node.
 *
 * Parameters :
 *
 *	n - in out, use
 *		node pointer
 *
 *	keys - in, use
 *		pointer to the position in the node from where the
 *		key value is deleted
 *
 *	k - in, use
 *		full version of deleted key value
 *
 *      deleteblob - in
 *          If TRUE, also possible blob data is deleted
 *
 *      cd - in, use
 *          client data context needed for blob unlink
 *
 * Return value :
 *
 *      TRUE    - node has become empty
 *      FALSE   - there is still key values in the node
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool bnode_deletekey_atpos(
        dbe_bnode_t* n,
        char* keys,
        dbe_bkey_t* k,
        int dkindex,
        bool deleteblob,
        rs_sysi_t* cd,
        bool recursive)
{
        dbe_bkey_t* dk;         /* deleted key */
        int dkpos;
        int dklen;              /* deleted key length */
        bool lastkey;
        int index;
        char mismatch;
        char* data;
        va_index_t len;
        int kpos=0;

        ss_dprintf_3(("bnode_deletekey_atpos:dkindex=%d\n", dkindex));
        CHK_BNODE(n);
        ss_dassert(dbe_bkey_getlength(k) <= dbe_bnode_maxkeylen((uint)n->n_go->go_idxfd->fd_blocksize));

        ss_dprintf_2(("bnode_deletekey_atpos\n"));
        ss_output_2(dbe_bkey_dprint(2, k));
        ss_dprintf_2(("\n"));

#ifdef SS_DEBUG
        if (n->n_info & BNODE_MISMATCHARRAY) {
            int tmppos;
            ss_dassert(dkindex != -1);
            bnode_getkeyoffset(n, dkindex, tmppos);
            ss_dassert(keys == &n->n_keys[tmppos]);
        }
#endif /* SS_DEBUG */

        if (deleteblob &&
            n->n_level == 0 &&
            !dbe_bkey_isdeletemark((dbe_bkey_t*)keys) &&
            dbe_bkey_isclustering((dbe_bkey_t*)keys) &&
            dbe_bkey_isblob((dbe_bkey_t*)keys))
        {
            ss_dassert(dbe_bkey_isleaf((dbe_bkey_t*)keys));
            ss_dassert(dbe_bkey_isleaf(k));

#ifndef SS_NOBLOB
            ss_dassert(cd != NULL);
            {
                su_list_t* deferredblobunlinklist =
                    rs_sysi_getdeferredblobunlinklist(cd);
                
                ss_dassert(deferredblobunlinklist != NULL);
                dbe_blobg2_append_blobids_of_vtpl_to_list(
                        cd,
                        deferredblobunlinklist,
                        dbe_bkey_getvtpl(k));
            }
#else
            ss_error;
#endif /* SS_NOBLOB */
        }

        n->n_dirty = TRUE;

        if (n->n_count == 1) {

            n->n_count = 0;
            n->n_len = 0;
            ss_debug(*keys = BKEY_ILLEGALINFO);

            ss_dassert(dbe_bnode_test(n));

            return(TRUE);
        }

        dk = (dbe_bkey_t*)keys;
        dklen = dbe_bkey_getlength(dk);
        if (n->n_info & BNODE_MISMATCHARRAY) {
            ss_dassert(dkindex != -1);
            bnode_getkeyoffset(n, dkindex, dkpos);
            lastkey = dkindex == (n->n_count - 1);
            ss_dassert(keys == &n->n_keys[dkpos]);
        } else {
            dkpos = (int)(keys - n->n_keys);
            lastkey = (dkpos + dklen) == n->n_len;
        }

        if (lastkey) {

            /* Deleted node is the last key in the node, no need to
               update the next key.
            */
            ss_dprintf_2(("bnode_deletekey_atpos:lastkey\n"));
            if (n->n_info & BNODE_MISMATCHARRAY) {
                memmove(
                    n->n_keysearchinfo_array + 4, 
                    n->n_keysearchinfo_array, 
                    4 * (n->n_count - 1));
                n->n_keysearchinfo_array += 4;
            } else {
                n->n_len -= dklen;
            }
            ss_debug(*keys = BKEY_ILLEGALINFO);

        } else {

            /* Not the last key in the node. The next key must
               be updated.
            */
            dbe_bkey_t* next_key;
            dbe_bkey_t* new_next_key;
            int gapnow;           /* gap when current key removed */
            int gapnew;           /* gap needed for the reexpanded next key */
            vtpl_t* tmp_vtpl;
            va_t* tmp_va;

            ss_dprintf_2(("bnode_deletekey_atpos:not lastkey\n"));
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, dkindex + 1, kpos);
                next_key = (dbe_bkey_t*)&n->n_keys[kpos];
            } else {
                ss_dassert(dkpos + dklen < n->n_len);
                next_key = (dbe_bkey_t*)(keys + dklen);
            }
            new_next_key = dbe_bkey_init_ex(cd, n->n_go->go_bkeyinfo);

            dbe_bkey_reexpand_delete(new_next_key, dk, next_key);

            if (n->n_info & BNODE_MISMATCHARRAY) {
                int next_key_len;
                int new_next_key_len;

                next_key_len = dbe_bkey_getlength(next_key);
                new_next_key_len = dbe_bkey_getlength(new_next_key);
                if (new_next_key_len > next_key_len) {
                    ss_dprintf_2(("bnode_deletekey_atpos:does not fit over next_key\n"));
                    if (dklen >= new_next_key_len || (char*)dk + dklen == (char*)next_key) {
                        ss_dprintf_2(("bnode_deletekey_atpos:dklen=%d, new_next_key_len=%d, dk+dklen=%ld, next_key=%ld\n", 
                            dklen, new_next_key_len, (long)((char*)dk+dklen), (long)next_key));
                        bnode_getkeyoffset(n, dkindex, kpos);
                    } else {
                        ss_dprintf_2(("bnode_deletekey_atpos:recurse after reorder\n"));
                        ss_dassert(!recursive);
                        dbe_bkey_done(new_next_key);
                        bnode_reorder(n, TRUE);
                        ss_dassert(n->n_info & BNODE_MISMATCHARRAY);
                        bnode_getkeyoffset(n, dkindex, kpos);
                        keys = &n->n_keys[kpos];
                        return(bnode_deletekey_atpos(n, keys, k, dkindex, FALSE, cd, TRUE));
                    }
                }

                memmove(
                    n->n_keysearchinfo_array + 4, 
                    n->n_keysearchinfo_array, 
                    4 * dkindex);
                n->n_keysearchinfo_array += 4;
                tmp_vtpl = BKEY_GETVTPLPTR(new_next_key);
                tmp_va = VTPL_GETVA_AT0(tmp_vtpl);
                data = va_getdata(tmp_va, &len);
                if (len != 0) {
                    index = BKEY_LOADINDEX(new_next_key);
                    if (index > 255) {
                        index = 255;
                    }
                    mismatch = *data;
                } else {
#if 1
                    index = BKEY_LOADINDEX(new_next_key);
                    if (index > 255) {
                        index = 255;
                    }
                    mismatch = 0;
#else
                    /* For NULL field, use previous value. */
                    ss_dassert(dkindex > 0);
                    bnode_getkeysearchinfo(
                        &n->n_keysearchinfo_array[(dkindex - 1) * 4],
                        &index,
                        &mismatch);
#endif
                }

                bnode_setkeysearchinfo(
                    &n->n_keysearchinfo_array[dkindex * 4],
                    index,
                    mismatch,
                    kpos);

                ss_debug(*keys = BKEY_ILLEGALINFO);

                /* copy reexpanded next key */
                ss_dprintf_2(("bnode_deletekey_atpos:copy key, offset=kpos, len=%d\n", kpos, new_next_key_len));
                dbe_bkey_copy((dbe_bkey_t*)&n->n_keys[kpos], new_next_key);
            } else {
                gapnow = dbe_bkey_getlength(dk) + dbe_bkey_getlength(next_key);
                gapnew = dbe_bkey_getlength(new_next_key);
                ss_dassert(gapnow > gapnew);

                /* move end part of the node to decrease node length */
                ss_dassert(n->n_len - dkpos - gapnow >= 0);
                ss_dassert(n->n_len - dkpos - gapnow <= n->n_len);
                memmove(
                    n->n_keys + dkpos + gapnew,
                    n->n_keys + dkpos + gapnow,
                    n->n_len - dkpos - gapnow);

                n->n_len -= (gapnow - gapnew);

                /* copy reexpanded next key */
                dbe_bkey_copy((dbe_bkey_t*)&n->n_keys[dkpos], new_next_key);
            }

            dbe_bkey_done_ex(cd, new_next_key);
        }
        n->n_count--;

        ss_dassert(dbe_bnode_test(n));

        return(FALSE);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_deletekey
 *
 * Deletes a key value from a node.
 *
 * Parameters :
 *
 *	n - in out, use
 *		node pointer
 *
 *	k - in, use
 *		key value which is deleted
 *
 *      cmp_also_header - in
 *		if TRUE, also header parts must match, otherwise
 *          a key value with the same v-tuple entry is
 *          deleted
 *
 *      delete_when_empty - in
 *          If TRUE, the key value is deleted even if the node
 *          becomes empty, otherwise the key value is not deleted.
 *
 *      deleteblob - in
 *          If TRUE, also possible blob data is deleted
 *
 *      deletefirstleafkey - in
 *          If TRUE, delete first key in a leaf node. Otherwise error
 *          DBE_RC_FIRSTLEAFKEY is returned.
 *
 *      cd - in, use
 *          client data context needed for blob unlink
 *
 * Return value :
 *
 *      DBE_RC_SUCC             - key deleted from the node
 *      DBE_RC_NODERELOCATE     - the node must be relocateed, always
 *                                returned before DBE_RC_NODEEMPTY, if both
 *                                cases are true
 *      DBE_RC_NODEEMPTY        - the node has become empty
 *      DBE_RC_FIRSTLEAFKEY     - trying to delete first key in a leaf node and
 *                                parameter deletefirstleafkey is FALSE.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_bnode_deletekey(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        bool cmp_also_header,
        bool delete_when_empty,
        bool deleteblob,
        bool deletefirstleafkey,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_bkey_search_t ks;   /* key search */
        char* keys;
        int count;
        int cmp;
        int klen = 0;
        bool emptyp;
        bool b;
        int kpos = -1;

        ss_pprintf_1(("dbe_bnode_deletekey:addr = %ld\n", n->n_addr));
        CHK_BNODE(n);
        ss_dassert(dbe_bkey_getlength(k) <= dbe_bnode_maxkeylen((uint)n->n_go->go_idxfd->fd_blocksize));
        ss_output_4(dbe_bkey_dprint(4, k));
        ss_dprintf_4(("\n"));

        if (n->n_cpnum != dbe_counter_getcpnum(n->n_go->go_ctr)) {
            /* The node must be relocated. */
            ss_dprintf_4(("dbe_bnode_deletekey:DBE_RC_NODERELOCATE\n"));
            return(DBE_RC_NODERELOCATE);
        }

        if ((n->n_info & BNODE_MISMATCHARRAY) == 0) {
            bnode_keysearchinfo_init(n);
        }

        n->n_dirty = TRUE;
        n->n_seqinscount = 0;
        count = n->n_count;

        if (count == 1) {
            /* Only one key in the node, it must the one we
               are looking for.
            */
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int kpos;
                bnode_getkeyoffset(n, 0, kpos);
                keys = &n->n_keys[kpos];
            } else {
                keys = n->n_keys;
            }
#ifdef SS_DEBUG
            if (n->n_level == 0
                && !(n->n_bonsaip 
                     && info != NULL 
                     && SU_BFLAG_TEST(info->i_flags, DBE_INFO_MERGE)
                     && dbe_cfg_splitpurge))
            {
                dbe_bkey_t* dk;         /* deleted key */
                dk = (dbe_bkey_t*)keys;
                ss_dprintf_4(("dbe_bnode_deletekey:only one key in leaf, cmp_also_header=%d\n", cmp_also_header));
                ss_output_4(dbe_bkey_dprint(4, dk));
                ss_dprintf_4(("\n"));
                if (cmp_also_header) {
                    ss_dassert(dbe_bkey_compare(k, dk) == 0);
                } else {
                    ss_dassert(dbe_bkey_compare_vtpl(k, dk) == 0);
                }
            }
#endif /* SS_DEBUG */

            if (delete_when_empty) {
                emptyp = bnode_deletekey_atpos(
                        n, keys, k, 0, deleteblob, cd, FALSE);
                ss_dassert(emptyp);
            }
            ss_dprintf_4(("dbe_bnode_deletekey:DBE_RC_NODEEMPTY\n"));
            return(DBE_RC_NODEEMPTY);
        }

        b = bnode_keysearchinfo_search(
                n,
                k,
                cmp_also_header ? DBE_BKEY_CMP_ALL : DBE_BKEY_CMP_VTPL,
                &keys,
                &kpos,
                &cmp,
                NULL,
                &ks);
        if (!b) {
            keys = n->n_keys;
            if (cmp_also_header) {
                dbe_bkey_search_init(&ks, k, DBE_BKEY_CMP_ALL);
            } else {
                dbe_bkey_search_init(&ks, k, DBE_BKEY_CMP_VTPL);
            }

            /* Search the position from where the key value must be deleted. */
            for (kpos = 0; kpos < count; kpos++) {
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
                keys += klen;
            }
        }

        if (cmp != 0 && n->n_level == 0) {
            if (n->n_bonsaip 
                && info != NULL 
                && SU_BFLAG_TEST(info->i_flags, DBE_INFO_MERGE)
                && dbe_cfg_splitpurge) 
            {
                ss_dprintf_4(("dbe_bnode_deletekey:deleted Bonsai-tree row not found in merge search\n"));
                return(DBE_RC_SUCC);
            }
            {
                ss_debug(char debugstring[80];)

                ss_debug(if (!dbe_cfg_startupforcemerge) SsDbgFlush();)
                ss_debug(if (!dbe_cfg_startupforcemerge) SsSprintf(
                    debugstring,
                    "/LEV:4/FIL:dbe/LOG/UNL/NOD/FLU/TID:%u",
                    SsThrGetid());)
                ss_debug(if (!dbe_cfg_startupforcemerge) SsDbgSet(debugstring);)
                ss_dprintf_1(("\ndbe_node_deletekey\n"));
                ss_dprintf_1(("Key not found: cmp = %d, count = %d, level = %d, cmp_also_header = %d, addr = %ld\n",
                    cmp, count, n->n_level, cmp_also_header, n->n_addr));
                ss_dprintf_1(("Key:\n"));
                ss_output_1(dbe_bkey_dprint(1, k));
                ss_dprintf_1(("Node:\n"));
                ss_output_1(if (!dbe_cfg_startupforcemerge) dbe_bnode_print(NULL, n->n_p, n->n_go->go_idxfd->fd_blocksize));
                ss_output_1(if (!dbe_cfg_startupforcemerge) dbe_trxbuf_print(n->n_go->go_trxbuf));
                ss_debug(if (!dbe_cfg_startupforcemerge) SsDbgFlush();)
                ss_debug(if (!dbe_cfg_startupforcemerge) SsDbgSet("/NOL");)
            }
            if (!dbe_cfg_startupforcemerge) {
                ss_derror;
            }
            return(DBE_ERR_DELETEROWNOTFOUND);
        }

        if (cmp < 0 || kpos == count) {
            /* move back to the previous key value */
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int offset;
                kpos--;
                bnode_getkeyoffset(n, kpos, offset);
                keys = &n->n_keys[offset];
            } else {
                keys -= klen;
            }
        }

#if 0
        if (n->n_level > 0 && keys == n->n_keys) {
            /* We are in higher than the leaf level (level zero) and are
               deleting the first key value. Instead of deleting the
               first key value, delete the second key value and set the
               address from the second key value to the first key value.
               This is needed to ensure that the key value set is not
               decreased by removing the first key value. The first key
               value is used to represent the start of key value set
               in this subtree in higher level nodes.
            */
            klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
            dbe_bkey_setaddr(
                (dbe_bkey_t*)keys,
                dbe_bkey_getaddr((dbe_bkey_t*)(keys + klen)));
            keys += klen;
            /* No need to update cmp or count. */
        }
#else
        if (n->n_level > 0 && kpos == 0 && !deletefirstleafkey) {
            ss_dprintf_4(("dbe_bnode_deletekey:DBE_RC_FIRSTLEAFKEY\n"));
            return(DBE_RC_FIRSTLEAFKEY);
        }
#endif

        emptyp = bnode_deletekey_atpos(n, keys, k, kpos, deleteblob, cd, FALSE);
        ss_dassert(!emptyp);

        ss_dprintf_4(("dbe_bnode_deletekey:DBE_RC_SUCC\n"));
        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getpathinfo
 *
 * Returns path infor whioch in this case means first and second key values
 * in a leaf node.
 *
 * Parameters :
 *
 *		n -
 *
 *
 *		firstk -
 *
 *
 *		secondk -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_bnode_getpathinfo(
        dbe_bnode_t* n,
        dbe_dynbkey_t* firstk,
        dbe_dynbkey_t* secondk)
{
        ss_dprintf_1(("dbe_bnode_getpathinfo:addr = %ld, level=%d\n", n->n_addr, n->n_level));
        CHK_BNODE(n);
        ss_dassert(n->n_level > 0);
        ss_dassert(n->n_count > 1);

        if (n->n_info & BNODE_MISMATCHARRAY) {
            int offset;
            bnode_getkeyoffset(n, 0, offset);
            dbe_dynbkey_setbkey(firstk, (dbe_bkey_t*)&n->n_keys[offset]);
            bnode_getkeyoffset(n, 1, offset);
            dbe_dynbkey_expand(secondk, *firstk, (dbe_bkey_t*)&n->n_keys[offset]);
        } else {
            dbe_dynbkey_setbkey(firstk, (dbe_bkey_t*)n->n_keys);
            dbe_dynbkey_expand(secondk, *firstk, (dbe_bkey_t*)(n->n_keys + dbe_bkey_getlength(*firstk)));
        }
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getaddrinkey
 *
 * Returns address field for a key specified in parameter 'k'.
 *
 * Parameters :
 *
 *		n - in
 *			node
 *
 *		k - in
 *			Searched key.
 *
 *		p_addr - out
 *			Address found in 'k'.
 *
 * Return value :
 *
 *      TRUE    - found
 *      FALSE   - not found
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_bnode_getaddrinkey(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        su_daddr_t* p_addr)
{
        dbe_bkey_search_t ks;   /* key search */
        char* keys;
        int count;
        int cmp;
        int klen;
        int i;
        int kpos;
        bool b;

        ss_pprintf_1(("dbe_bnode_getaddrinkey:addr = %ld, level=%d\n", n->n_addr, n->n_level));
        CHK_BNODE(n);

        if (n->n_count == 0 || n->n_level == 0) {
            return(FALSE);
        }

        count = n->n_count;

        if (count == 1) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                bnode_getkeyoffset(n, 0, kpos);
                keys = n->n_keys + kpos;
            } else {
                keys = n->n_keys;
            }
            if (dbe_bkey_compare(k, (dbe_bkey_t*)keys) == 0) {
                /* Only one key and it is our key. */
                *p_addr = dbe_bkey_getaddr((dbe_bkey_t*)keys);
                return(TRUE);
            } else {
                /* Key not found */
                return(FALSE);
            }
        }

        b = bnode_keysearchinfo_search(
                n,
                k,
                DBE_BKEY_CMP_ALL,
                &keys,
                &i,
                &cmp,
                NULL,
                NULL);

        if (!b) {

            dbe_bkey_search_init(&ks, k, DBE_BKEY_CMP_ALL);

            keys = n->n_keys;

            /* Search the key value from leaf. */
            for (i = 0; i < count; i++) {
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
                keys += klen;
            }
        }
        if (cmp == 0) {
            /* Found */
            *p_addr = dbe_bkey_getaddr((dbe_bkey_t*)keys);
            return(TRUE);
        } else {
            /* Not found */
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bnode_searchnode
 *
 * Searches a node that contains key k.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 *	k - in, use
 *		searched key
 *
 *      cmp_also_header - in
 *		if TRUE, also header parts must match, otherwise
 *          only the v-tuple of the key is compared
 *
 * Return value :
 *
 *      database file address of the node containing key k
 *
 * Limitations  :
 *
 * Globals used :
 */
su_daddr_t dbe_bnode_searchnode(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        bool cmp_also_header)
{
        dbe_bkey_search_t ks;
        char* keys;
        int klen;
        int count;
        int cmp;
        int pos = 0;
        bool b;
        int offset;

        SS_PUSHNAME("dbe_bnode_searchnode");
        ss_pprintf_1(("dbe_bnode_searchnode:addr = %ld, level=%d\n", n->n_addr, n->n_level));
        CHK_BNODE(n);
        ss_dassert(n->n_level > 0);
        ss_dassert(n->n_count > 0);
        ss_dassert(!dbe_bkey_isleaf(dbe_bnode_getfirstkey(n)));
#ifdef SS_DEBUG
        if (!(dbe_bkey_compare_vtpl(dbe_bnode_getfirstkey(n), k) <= 0)) {
            ss_dprintf_1(("dbe_bnode_searchnode:firstkey\n"));
            ss_output_1(dbe_bkey_print(NULL, dbe_bnode_getfirstkey(n)));
            ss_dprintf_1(("dbe_bnode_searchnode:k\n"));
            ss_output_1(dbe_bkey_print(NULL, k));
            ss_error;
        }
#endif /* SS_DEBUG */

#ifndef DBE_TEST_NEWKEYSEARCH
        b = bnode_keysearchinfo_search(
                n, 
                k, 
                cmp_also_header ? DBE_BKEY_CMP_ALL : DBE_BKEY_CMP_VTPL, 
                &keys,
                &pos,
                &cmp,
                &klen,
                NULL);
        if (b) {
            if (cmp < 0 || pos == n->n_count) {
                /* move back to the previous key value */
                bnode_getkeyoffset(n, pos - 1, offset);
                keys = &n->n_keys[offset];
            }
        } else 
#endif
        {
            klen = 0;
            keys = n->n_keys;
            count = n->n_count;
            if (cmp_also_header) {
                dbe_bkey_search_init(&ks, k, DBE_BKEY_CMP_ALL);
            } else {
                dbe_bkey_search_init(&ks, k, DBE_BKEY_CMP_VTPL);
            }

            while (count--) {
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
                keys += klen;
                pos++;
            }
#ifdef DBE_TEST_NEWKEYSEARCH
            {
                char* tmp_keys;
                int tmp_pos;
                int tmp_cmp;
                int tmp_klen;

                b = bnode_keysearchinfo_search(
                        n, 
                        k, 
                        cmp_also_header ? DBE_BKEY_CMP_ALL : DBE_BKEY_CMP_VTPL, 
                        &tmp_keys,
                        &tmp_pos,
                        &tmp_cmp,
                        &tmp_klen,
                        NULL);
                if (b) {
                    ss_assert(keys == tmp_keys);
                    ss_assert(pos == tmp_pos);
                    ss_assert(cmp == tmp_cmp || (cmp < 0 && tmp_cmp < 0) || (cmp > 0 && tmp_cmp > 0));
                    ss_assert(tmp_klen == klen);
                }
            }
#endif
            if (cmp < 0 || count < 0) {
                /* move back to the previous key value */
                keys -= klen;
            }
        }

        SS_POPNAME;
        return(dbe_bkey_getaddr((dbe_bkey_t*)keys));
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_initst
 *
 * Initializes a node range search.
 *
 * Parameters :
 *
 *      nrs - in out, use
 *          pointer to node range search struct that is initialized
 *
 *	n - in, hold
 *		node pointer
 *
 *	krs - in out, hold
 *		key range search
 *
 * Return value - give :
 *
 *      pointer to the node range search object
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bnode_rsea_initst(
        dbe_bnode_rsea_t* nrs,
        dbe_bnode_t* n,
        dbe_bkrs_t* krs)
{
        ss_dassert(nrs != NULL);
        CHK_BNODE(n);
        ss_dassert(krs != NULL);
        ss_dprintf_1(("dbe_bnode_rsea_initst, addr = %ld\n", n->n_addr));

        nrs->nrs_n = n;
        nrs->nrs_index = 0;
        if (n->n_info & BNODE_MISMATCHARRAY) {
            int kpos;
            bnode_getkeyoffset(n, 0, kpos);
            nrs->nrs_keys = &n->n_keys[kpos];
        } else {
            nrs->nrs_keys = n->n_keys;
        }
        nrs->nrs_count = n->n_count;
        nrs->nrs_krs = krs;
        nrs->nrs_saveindex = -1;
        nrs->nrs_savepos = NULL;
        nrs->nrs_savekey = NULL;
        nrs->nrs_initialized = FALSE;

        ss_dassert(n->n_level == 0);
        ss_dassert(n->n_count == 0 || dbe_bkey_isleaf((dbe_bkey_t*)nrs->nrs_keys));
}

/*##**********************************************************************\
 * 
 *		dbe_bnode_rsea_initst_error
 * 
 * Init after bsearch init error to make donest safe.
 * 
 * Parameters : 
 * 
 *		nrs - 
 *			
 *			
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_bnode_rsea_initst_error(
        dbe_bnode_rsea_t* nrs)
{
        ss_dassert(nrs != NULL);

        nrs->nrs_savekey = NULL;
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_skipleaf
 *
 * Skips the current leaf. Used in some error situations to continue
 * search even if obe leaf is currupted.
 *
 * Parameters :
 *
 *	nrs -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_bnode_rsea_skipleaf(dbe_bnode_rsea_t* nrs)
{
        ss_dassert(nrs != NULL);
        CHK_BNODE(nrs->nrs_n);
        ss_dprintf_1(("dbe_bnode_rsea_skipleaf, addr = %ld\n", nrs->nrs_n->n_addr));

        nrs->nrs_index = nrs->nrs_count;
}

/*#***********************************************************************\
 *
 *		bnode_rsea_nextorprevinit
 *
 *
 *
 * Parameters :
 *
 *	nrs -
 *
 *
 *	srk -
 *
 *
 *	p_cmp - out
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t bnode_rsea_nextorprevinit(
        dbe_bnode_rsea_t* nrs,
        dbe_srk_t* srk,
        int* p_cmp)
{
        int i;
        int cmp;
        int count;
        char* keys;
        dbe_ret_t rc;
        dbe_bkey_search_t ks;
        bool b;

        CHK_BNODE(nrs->nrs_n);
        ss_dassert(dbe_bkrs_isbegin(nrs->nrs_krs));
        ss_dassert(nrs->nrs_n->n_count == nrs->nrs_count);
        ss_dassert(nrs->nrs_count >= 0);

        /* This is the first time the range search is done. Find the
         * search starting position in this node. The key range
         * search begin state is cleared, and this branch is not
         * executed in the next time.
         */
        ss_pprintf_1(("bnode_rsea_nextorprevinit, addr = %ld\n", nrs->nrs_n->n_addr));
        ss_dprintf_4(("begin key\n"));
        ss_output_4(dbe_bkey_dprint(4, dbe_bkrs_getbeginkey(nrs->nrs_krs)));

#ifdef DBE_NEXTNODEBUG
        if (nrs->nrs_count == 0) {
            /* Possible during merge search. */
            *p_cmp = -1;
            dbe_bkrs_clearbegin(nrs->nrs_krs);
            return(DBE_RC_NOTFOUND);
        }
#endif /* DBE_NEXTNODEBUG */

        count = nrs->nrs_count;

#ifdef DBE_TEST_NEWKEYSEARCH
        b = FALSE;
#else
        b = bnode_keysearchinfo_search(
                nrs->nrs_n, 
                dbe_bkrs_getbeginkey(nrs->nrs_krs), 
                DBE_BKEY_CMP_ALL,
                &keys,
                &i,
                &cmp,
                NULL,
                NULL);
#endif
        if (!b) {
            dbe_bkey_search_init(
                &ks,
                dbe_bkrs_getbeginkey(nrs->nrs_krs),
                DBE_BKEY_CMP_ALL);

            keys = nrs->nrs_keys;

            for (i = 0; i < count; i++) {
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            }
#ifdef DBE_TEST_NEWKEYSEARCH
            {
                char* tmp_keys;
                int tmp_pos;
                int tmp_cmp;
                int tmp_klen;

                b = bnode_keysearchinfo_search(
                        nrs->nrs_n, 
                        dbe_bkrs_getbeginkey(nrs->nrs_krs), 
                        DBE_BKEY_CMP_ALL,
                        &tmp_keys,
                        &tmp_pos,
                        &tmp_cmp,
                        &tmp_klen,
                        NULL);
                if (b) {
                    ss_assert(keys == tmp_keys);
                    ss_assert(i == tmp_pos);
                    ss_assert(cmp == tmp_cmp || (cmp < 0 && tmp_cmp < 0) || (cmp > 0 && tmp_cmp > 0));
                }
            }
#endif
        }
        nrs->nrs_index = i;
        nrs->nrs_keys = keys;

        if (i < count) {
            /* Key found.
             */
            dbe_srk_setbkey(srk, dbe_bkrs_getbeginkey(nrs->nrs_krs));
            dbe_srk_expand(srk, (dbe_bkey_t*)keys);

            ss_output_begin(!(dbe_bkey_compare(dbe_bkrs_getbeginkey(nrs->nrs_krs), dbe_srk_getbkey(srk)) <= 0))
                ss_debug(char debugstring[48];)

                ss_debug(SsSprintf(
                    debugstring,
                    "/LEV:4/LOG/NOD/UNL/FLU/TID:%u/FIL:dbe",
                    SsThrGetid());)
                ss_debug(SsDbgSet(debugstring);)
                ss_dprintf_1(("\nbnode_rsea_nextorprevinit\n"));
                ss_dprintf_1(("dbe_bkrs_getbeginkey:\n"));
                ss_output_1(dbe_bkey_dprint(1, dbe_bkrs_getbeginkey(nrs->nrs_krs)));
                ss_dprintf_1(("dbe_srk_getbkey:\n"));
                ss_output_1(dbe_bkey_dprint(1, dbe_srk_getbkey(srk)));
                ss_dprintf_1(("Node:\n"));
                ss_output_1(dbe_bnode_print(NULL, nrs->nrs_n->n_p, nrs->nrs_n->n_go->go_idxfd->fd_blocksize));
                ss_derror;
            ss_output_end

            *p_cmp = cmp;
            rc = DBE_RC_FOUND;

            ss_dprintf_4(("DBE_RC_FOUND, i = %d, cmp = %d\n", i, cmp));
        } else {
            /* Key not found.
             */
            rc = DBE_RC_NOTFOUND;

            ss_dprintf_4(("DBE_RC_NOTFOUND\n"));
        }
        dbe_bkrs_clearbegin(nrs->nrs_krs);
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_next
 *
 * Returns the next key in a node range search.
 *
 * Parameters :
 *
 *	nrs - in, use
 *		node range search pointer
 *
 *	srk - use
 *		current search return key, must contain the value returned by
 *          the previous dbe_bnode_rsea_next call (the value should
 *          not be overwritten)
 *
 * Return value :
 *
 *      DBE_RC_FOUND    - next key found
 *      DBE_RC_NOTFOUND - end of this node
 *      DBE_RC_END      - end of search
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_bnode_rsea_next(
        dbe_bnode_rsea_t* nrs,
        dbe_srk_t* srk)
{
        dbe_ret_t rc;
        dbe_bkrs_rc_t retcode;
        int dummy_cmp;

        ss_dassert(nrs != NULL);
        CHK_BNODE(nrs->nrs_n);
        ss_pprintf_1(("dbe_bnode_rsea_next, addr = %ld\n", nrs->nrs_n->n_addr));

        if (!nrs->nrs_initialized) {

            if (nrs->nrs_n == NULL) {
                ss_dprintf_4(("dbe_bnode_rsea_next:line=%d, DBE_RC_NOTFOUND\n", __LINE__));
                return(DBE_RC_NOTFOUND);
            }
            nrs->nrs_initialized = TRUE;
            nrs->nrs_index = 0;

            if (dbe_bkrs_isbegin(nrs->nrs_krs)) {

                rc = bnode_rsea_nextorprevinit(nrs, srk, &dummy_cmp);
                switch (rc) {
                    case DBE_RC_FOUND:
                        break;
                    case DBE_RC_NOTFOUND:
                        /* There might be correct key values
                         * in the next node.
                         */
                        ss_dprintf_4(("dbe_bnode_rsea_next:line=%d, DBE_RC_NOTFOUND\n", __LINE__));
                        return(DBE_RC_NOTFOUND);
                    default:
                        su_rc_error(rc);
                }

            } else {

                if (nrs->nrs_index == nrs->nrs_count) {
                    /* End of this node. */
                    ss_dprintf_4(("dbe_bnode_rsea_next:line=%d, DBE_RC_NOTFOUND\n", __LINE__));
                    return(DBE_RC_NOTFOUND);
                } else {
                    /* Get the first key of the node. */
                    dbe_srk_setbkey(srk, (dbe_bkey_t*)nrs->nrs_keys);
                }
            }

        } else {

            if (nrs->nrs_index == nrs->nrs_count) {
                /* End of this node. */
                ss_dprintf_4(("dbe_bnode_rsea_next:line=%d, DBE_RC_NOTFOUND\n", __LINE__));
                return(DBE_RC_NOTFOUND);
            }

            /* Continue old search in this node. Move to the next key. */
            nrs->nrs_index++;
            if (nrs->nrs_index == nrs->nrs_count) {
                /* End of this node. */
                ss_dprintf_4(("dbe_bnode_rsea_next:line=%d, DBE_RC_NOTFOUND\n", __LINE__));
                return(DBE_RC_NOTFOUND);
            }
            if (nrs->nrs_n->n_info & BNODE_MISMATCHARRAY) {
                int offset;
                bnode_getkeyoffset(nrs->nrs_n, nrs->nrs_index, offset);
                nrs->nrs_keys = nrs->nrs_n->n_keys + offset;
            } else {
                nrs->nrs_keys += dbe_bkey_getlength((dbe_bkey_t*)nrs->nrs_keys);
            }
            /* Expand the next key in the node. */
            dbe_srk_expand(srk, (dbe_bkey_t*)nrs->nrs_keys);
        }

        retcode = dbe_bkrs_checkrangeend(nrs->nrs_krs, dbe_srk_getbkey(srk));

        if (retcode == BKRS_STOP) {
            /* End of search. */
            ss_dprintf_4(("dbe_bnode_rsea_next:line=%d, DBE_RC_END\n", __LINE__));
            return(DBE_RC_END);
        } else {
            /* Next key accepted to the range. */
            ss_dassert(retcode == BKRS_CONT);
            ss_dprintf_4(("dbe_bnode_rsea_next:line=%d, DBE_RC_FOUND\n", __LINE__));
            return(DBE_RC_FOUND);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_prev
 *
 * Returns the previous key in a node range search.
 *
 * Parameters :
 *
 *	nrs - in, use
 *		node range search pointer
 *
 *	srk - use
 *		current search return key, must contain the value returned by
 *          the previous dbe_bnode_rsea_next call (the value should
 *          not be overwritten)
 *
 * Return value :
 *
 *      DBE_RC_FOUND    - previous key found
 *      DBE_RC_NOTFOUND - end of this node
 *      DBE_RC_END      - end of search
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_bnode_rsea_prev(
        dbe_bnode_rsea_t* nrs,
        dbe_srk_t* srk)
{
        int i;
        int cmp;
        dbe_ret_t rc;
        dbe_bkrs_rc_t retcode;
        bool movetoprev;

        ss_dassert(nrs != NULL);
        CHK_BNODE(nrs->nrs_n);
        ss_trigger("dbe_bnode_rsea_prev");
        ss_pprintf_1(("dbe_bnode_rsea_prev, addr = %ld\n", nrs->nrs_n->n_addr));

        if (!nrs->nrs_initialized && dbe_bkrs_isbegin(nrs->nrs_krs)) {
            ss_dprintf_2(("dbe_bnode_rsea_prev:!init && isbegin\n"));

            nrs->nrs_initialized = TRUE;
            nrs->nrs_index = 0;

            rc = bnode_rsea_nextorprevinit(nrs, srk, &cmp);
            switch (rc) {
                case DBE_RC_FOUND:
                    /* If there was not an equal match, move to the previous
                     * key value in this node.
                     */
                    ss_dassert(cmp <= 0);
                    movetoprev = (cmp < 0);
                    break;
                case DBE_RC_NOTFOUND:
                    if (nrs->nrs_index == nrs->nrs_count) {
                        /* Init went past the last key value in the
                         * node, return the last key value in the node.
                         */
                        movetoprev = TRUE;
                        break;
                    } else {
                        /* There might be correct key values in preceding
                         * node.
                         */
                        ss_trigger("dbe_bnode_rsea_prev");
                        return(DBE_RC_END);
                    }
                default:
                    su_rc_error(rc);
            }

        } else {
            ss_dassert(!dbe_bkrs_isbegin(nrs->nrs_krs));

            if (!nrs->nrs_initialized) {
                /* Get the last key of the node.
                 */
                nrs->nrs_initialized = TRUE;
                nrs->nrs_index = nrs->nrs_count;
            }
            movetoprev = TRUE;
        }

        if (movetoprev) {
            int new_saveindex;

            ss_dprintf_2(("dbe_bnode_rsea_prev:movetoprev, nrs->nrs_index=%d\n", nrs->nrs_index));

            if (nrs->nrs_index == 0) {
                /* Beginning of this node.
                 */
                ss_dprintf_2(("dbe_bnode_rsea_prev:beginning of node\n"));
                ss_trigger("dbe_bnode_rsea_prev");
                return(DBE_RC_NOTFOUND);
            }
            nrs->nrs_index--;

            if (nrs->nrs_saveindex != -1
                && nrs->nrs_saveindex <= nrs->nrs_index) {
                /* Use saved starting position in the node.
                 */
                ss_dprintf_2(("dbe_bnode_rsea_prev:start from position %d\n", nrs->nrs_saveindex));
                ss_dassert(nrs->nrs_savepos != NULL);
                ss_dassert(nrs->nrs_savekey != NULL);
                nrs->nrs_keys = nrs->nrs_savepos;
                dbe_srk_setbkey(srk, nrs->nrs_savekey);
                i = nrs->nrs_saveindex;
                new_saveindex = -1;
            } else {
                /* Start from the beginning of the leaf.
                 */
                ss_dprintf_2(("dbe_bnode_rsea_prev:start from beginning of leaf\n"));
                if (nrs->nrs_n->n_info & BNODE_MISMATCHARRAY) {
                    int offset;
                    bnode_getkeyoffset(nrs->nrs_n, 0, offset);
                    nrs->nrs_keys = &nrs->nrs_n->n_keys[offset];
                } else {
                    nrs->nrs_keys = nrs->nrs_n->n_keys;
                }
                dbe_srk_setbkey(srk, (dbe_bkey_t*)nrs->nrs_keys);
                i = 0;
                new_saveindex = (nrs->nrs_index / 10) * 10;
                if (new_saveindex == 0) {
                    new_saveindex = -1;
                }
            }
            for (; i < nrs->nrs_index; i++) {
                if (new_saveindex != -1 && i == new_saveindex) {
                    /* Save the current key as a starting position for
                     * following prev calls.
                     */
                    ss_dprintf_2(("dbe_bnode_rsea_prev:save position %d\n", i));
                    nrs->nrs_saveindex = i;
                    nrs->nrs_savepos = nrs->nrs_keys;
                    dbe_dynbkey_setbkey(&nrs->nrs_savekey, dbe_srk_getbkey(srk));
                }
                if (nrs->nrs_n->n_info & BNODE_MISMATCHARRAY) {
                    int offset;
                    bnode_getkeyoffset(nrs->nrs_n, i + 1, offset);
                    nrs->nrs_keys = nrs->nrs_n->n_keys + offset;
                } else {
                    nrs->nrs_keys += dbe_bkey_getlength((dbe_bkey_t*)nrs->nrs_keys);
                }
                dbe_srk_expand(srk, (dbe_bkey_t*)nrs->nrs_keys);
            }
        }

        retcode = dbe_bkrs_checkrangebegin(nrs->nrs_krs, dbe_srk_getbkey(srk));

        if (retcode == BKRS_STOP) {
            /* Beginning of search. */
            ss_trigger("dbe_bnode_rsea_prev");
            return(DBE_RC_END);
        } else {
            /* Key accepted to the range. */
            ss_dassert(retcode == BKRS_CONT);
            ss_trigger("dbe_bnode_rsea_prev");
            return(DBE_RC_FOUND);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_nextnode
 *
 * Searches a node that contains next key. The next possible range search
 * starting position is also updated during the search. The next range
 * search starting position is the next node after the node which contains
 * the current starting position. This is stored so that at each level
 * the next node key value is stored into key range search structure (if
 * such a key value exists). The starting position of the next search
 * step is the next key value found from the lowest level of the tree.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 *	krs - in out, use
 *		range search state structure
 *
 * Return value :
 *
 *      database file address of the node containing next range search key
 *
 * Limitations  :
 *
 * Globals used :
 */
su_daddr_t dbe_bnode_rsea_nextnode(
        dbe_bnode_t* n,
        dbe_bkrs_t* krs,
        int longseqsea,
        uint readaheadsize,
        dbe_info_t* info)
{
        int klen;
        int cmp;
        dbe_bkey_search_t ks;
        char* keys;
        int count;
        su_daddr_t addr;
        int pos;
        bool b;
        bool is_first_key;

        CHK_BNODE(n);
        ss_pprintf_1(("dbe_bnode_rsea_nextnode, addr = %ld\n", n->n_addr));
        ss_dprintf_2(("search key:\n"));
        ss_output_2(dbe_bkey_dprint(2, dbe_bkrs_getbeginkey(krs)));

        ss_dassert(n->n_level > 0);
        ss_dassert(n->n_count > 0);

        count = n->n_count;
        ss_dassert(!dbe_bkey_isleaf(dbe_bnode_getfirstkey(n)));

#ifndef DBE_TEST_NEWKEYSEARCH
        b = bnode_keysearchinfo_search(
                n, 
                dbe_bkrs_getbeginkey(krs), 
                DBE_BKEY_CMP_ALL, 
                &keys,
                &pos,
                &cmp,
                &klen,
                NULL);
        if (b) {
            count = n->n_count - pos - 1;
            is_first_key = (pos == 0);
        } else 
#endif
        {
            klen = 0;
            keys = n->n_keys;
            dbe_bkey_search_init(
                &ks,
                dbe_bkrs_getbeginkey(krs),
                DBE_BKEY_CMP_ALL);

            pos = 0;
            while (count--) {
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
                keys += klen;
                pos++;
            }
            is_first_key = (keys == n->n_keys);
#ifdef DBE_TEST_NEWKEYSEARCH
            {
                char* tmp_keys;
                int tmp_pos;
                int tmp_cmp;
                int tmp_klen;

                b = bnode_keysearchinfo_search(
                        n, 
                        dbe_bkrs_getbeginkey(krs), 
                        DBE_BKEY_CMP_ALL, 
                        &tmp_keys,
                        &tmp_pos,
                        &tmp_cmp,
                        &tmp_klen,
                        NULL);
                if (b) {
                    ss_assert(keys == tmp_keys);
                    ss_assert(pos == tmp_pos);
                    ss_assert(cmp == tmp_cmp || (cmp < 0 && tmp_cmp < 0) || (cmp > 0 && tmp_cmp > 0));
                    ss_assert(tmp_klen == klen);
                }
            }
#endif
        }

        if (count > 0 || (count == 0 && cmp < 0)) {
            /* There is a next key, store it as the starting position
               of the next range search step. */
            char* next_key;
            if (is_first_key) {
                /* First key, we need to store the next key as the starting
                 * point of the next step.
                 */
                if (n->n_count > 1) {
                    /* We have more than one key in the node. Because we can
                     * not directly expand the second key we manually create
                     * a new full next key.
                     */
                    dbe_dynbkey_t tmp = NULL;
                    if (n->n_info & BNODE_MISMATCHARRAY) {
                        int offset;
                        bnode_getkeyoffset(n, pos + 1, offset);
                        next_key = &n->n_keys[offset];
                    } else {
                        next_key = keys + dbe_bkey_getlength((dbe_bkey_t*)keys);
                    }
                    ss_dassert(next_key < n->n_keys + n->n_len);
                    dbe_dynbkey_expand(
                        &tmp,
                        (dbe_bkey_t*)keys,
                        (dbe_bkey_t*)next_key);
                    dbe_bkrs_setnextstepbegin_fk(krs, tmp);
                    dbe_dynbkey_free(&tmp);
                }
            } else if (cmp == 0) {
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    int offset;
                    bnode_getkeyoffset(n, pos + 1, offset);
                    next_key = &n->n_keys[offset];
                } else {
                    next_key = keys + dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
                ss_dassert(next_key < n->n_keys + n->n_len);
                dbe_bkrs_setnextstepbegin(
                    krs,
                    (dbe_bkey_t*)next_key);
            } else {
                ss_dassert(keys < n->n_keys + n->n_len);
                dbe_bkrs_setnextstepbegin(
                    krs,
                    (dbe_bkey_t*)keys);
            }
        }

        if (!is_first_key && (cmp < 0 || count < 0)) {
            /* move back to the previous key value */
            ss_dassert(pos > 0);
            pos--;
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int offset;
                bnode_getkeyoffset(n, pos, offset);
                keys = n->n_keys + offset;
            } else {
                keys -= klen;
            }
        }

        dbe_bkrs_setprevstepbegin(krs, dbe_bkrs_getbeginkey(krs));

        addr = dbe_bkey_getaddr((dbe_bkey_t*)keys);

        ss_dprintf_2(("dbe_bnode_rsea_nextnode: next addr = %ld\n", addr));

        if (longseqsea && readaheadsize > 0 && n->n_level == 1) {
            int nskip;
            int nread;
            bool readaheadp;

            if (longseqsea == 1) {
                /* Mode just changed to long sequential search. */
                readaheadp = TRUE;
                nskip = 0;
                nread = 2 * readaheadsize;
            } else if (pos == 0) {
                /* Beginning of leaf. */
                readaheadp = TRUE;
                nskip = 0;
                nread = 2 * readaheadsize;
            } else if (n->n_count - (pos + 1) < readaheadsize) {
                /* All read in this leaf. */
                readaheadp = FALSE;
            } else if ((pos % readaheadsize) == 0) {
                /* Position is multiple of readaheadsize */
                readaheadp = TRUE;
                nskip = readaheadsize;
                nread = readaheadsize;
            } else {
                /* Position not multiple of readaheadsize */
                readaheadp = FALSE;
            }
            if (readaheadp) {
                su_daddr_t* readahead_array;
                int array_size;

                readahead_array = SsMemAlloc((2 * readaheadsize) * sizeof(su_daddr_t));
                array_size = 0;

                while (nskip > 0 && count > 0) {
                    if (n->n_info & BNODE_MISMATCHARRAY) {
                        pos++;
                    } else {
                        keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                    }
                    nskip--;
                    count--;
                }
                if (n->n_info & BNODE_MISMATCHARRAY) {
                    int kpos;
                    bnode_getkeyoffset(n, pos, kpos);
                    keys = &n->n_keys[kpos];
                }
                while (nread > 0 && count > 0) {
                    readahead_array[array_size] = dbe_bkey_getaddr((dbe_bkey_t*)keys);
                    if (n->n_info & BNODE_MISMATCHARRAY) {
                        int kpos;
                        pos++;
                        bnode_getkeyoffset(n, pos, kpos);
                        keys = &n->n_keys[kpos];
                    } else {
                        keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                    }
                    array_size++;
                    nread--;
                    count--;
                }

                /* Add addresses to the readahead pool.
                 */
                dbe_iomgr_prefetch(
                    n->n_go->go_iomgr,
                    readahead_array,
                    array_size,
                    info->i_flags);

                SsMemFree(readahead_array);

            }
            /* Wait until the current address is read by the readahead
             * thread. Prefetch wait is used instead of reach to avoid
             * unnecessary unordered seek request bypassing I/O manager
             * ordering effect. This is assumed to improve database
             * total throughput.
             */
            dbe_iomgr_prefetchwait(
                n->n_go->go_iomgr,
                addr);
        }

        return(addr);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_prevnode
 *
 * Searches a node that contains previous key.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 *	krs - in out, use
 *		range search state structure
 *
 * Return value :
 *
 *      database file address of the node containing previous range
 *      search key
 *
 * Limitations  :
 *
 * Globals used :
 */
su_daddr_t dbe_bnode_rsea_prevnode(
        dbe_bnode_t* n, 
        dbe_bkrs_t* krs,
        rs_sysi_t* cd)
{
        int klen;
        int cmp;
        dbe_bkey_search_t ks;
        char* keys;
        dbe_bkey_t* k;
        int count;
        su_daddr_t addr;
        int i;

        CHK_BNODE(n);
        ss_pprintf_1(("dbe_bnode_rsea_prevnode, addr = %ld\n", n->n_addr));
        ss_dprintf_2(("search key:\n"));
        ss_output_2(dbe_bkey_dprint(2, dbe_bkrs_getbeginkey(krs)));
        SS_PUSHNAME("dbe_bnode_rsea_prevnode");

        ss_dassert(n->n_level > 0);
        ss_dassert(n->n_count > 0);

        klen = 0;
        keys = (char*)dbe_bnode_getfirstkey(n);
        count = n->n_count;
        k = dbe_bkey_init_ex(cd, n->n_go->go_bkeyinfo);
        dbe_bkey_copy(k, (dbe_bkey_t*)keys);
        SS_PMON_ADD(SS_PMON_BNODE_SEARCH_KEYS);
        dbe_bkey_search_init(
            &ks,
            dbe_bkrs_getbeginkey(krs),
            DBE_BKEY_CMP_ALL);

        ss_dassert(!dbe_bkey_isleaf((dbe_bkey_t*)keys));

        for (i = 0; ; ) {
            dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
            if (cmp <= 0) {
                break;
            }
            dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
            i++;
            if (i == n->n_count) {
                break;
            }
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int kpos;
                bnode_getkeyoffset(n, i, kpos);
                keys = &n->n_keys[kpos];
            } else {
                klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
                keys = (char *) ((ss_byte_t*)keys + klen);
            }
        }

        dbe_bkrs_setprevstepbegin(krs, k);

        addr = dbe_bkey_getaddr(k);

        if (i < n->n_count) {
            /* There is a next key. */
            dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
            dbe_bkrs_setnextstepbegin_fk(krs, k);
        }

        dbe_bkey_done_ex(cd, k);

        ss_dprintf_2(("dbe_bnode_rsea_prevnode: prev addr = %ld\n", addr));
        SS_POPNAME;

        return(addr);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_resetnode
 *
 * Finds correct node after search reset.
 *
 * Parameters :
 *
 *	n -
 *
 *
 *	krs -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_daddr_t dbe_bnode_rsea_resetnode(
        dbe_bnode_t* n, 
        dbe_bkrs_t* krs,
        rs_sysi_t* cd)
{
        CHK_BNODE(n);
        ss_dprintf_1(("dbe_bnode_rsea_resetnode, addr = %ld\n", n->n_addr));

        return(dbe_bnode_rsea_prevnode(n, krs, cd));
}

/*##**********************************************************************\
 * 
 *		dbe_bnode_getunique
 * 
 * Gets unique key value.
 * 
 * Parameters : 
 * 
 *		n - 
 *			
 *			
 *		kb - 
 *			
 *			
 *		ke - 
 *			
 *			
 *		found_key - 
 *			
 *			
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_bnode_getunique(
        dbe_bnode_t* n,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        int* p_i,
        char** p_keys,
        dbe_bkey_t* found_key)
{
        int i;
        int cmp;
        int count;
        char* keys;
        dbe_ret_t rc;
        dbe_bkey_search_t ks;
        bool b;

        CHK_BNODE(n);

        ss_pprintf_1(("dbe_bnode_getunique, addr = %ld\n", n->n_addr));
        ss_dprintf_4(("begin key\n"));
        ss_output_4(dbe_bkey_dprint(4, kb));
        ss_dprintf_4(("end key\n"));
        ss_output_4(dbe_bkey_dprint(4, ke));

        if (n->n_count == 0) {
            ss_pprintf_1(("dbe_bnode_getunique, DBE_RC_END, n->n_count == 0\n"));
            return(DBE_RC_END);
        }

        count = n->n_count;

        b = bnode_keysearchinfo_search(
                n, 
                kb, 
                DBE_BKEY_CMP_ALL,
                &keys,
                &i,
                &cmp,
                NULL,
                NULL);
        if (!b) {
            dbe_bkey_search_init(
                &ks,
                kb,
                DBE_BKEY_CMP_ALL);

            keys = n->n_keys;

            for (i = 0; i < count; i++) {
                dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
                if (cmp <= 0) {
                    break;
                }
                keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            }
        }

        if (i < count) {
            /* Key found.
             */
            dbe_bkey_expand(found_key, kb, (dbe_bkey_t*)keys);

            ss_dprintf_4(("found key\n"));
            ss_output_4(dbe_bkey_dprint(4, found_key));

            ss_output_begin(!(dbe_bkey_compare(kb, found_key) <= 0))
                ss_debug(char debugstring[80];)

                ss_debug(SsSprintf(
                    debugstring,
                    "/LEV:4/LOG/NOD/UNL/FLU/TID:%u/FIL:dbe",
                    SsThrGetid());)
                ss_debug(SsDbgSet(debugstring);)
                ss_dprintf_1(("\ndbe_bnode_getunique\n"));
                ss_dprintf_1(("begin key:\n"));
                ss_output_1(dbe_bkey_dprint(1, kb));
                ss_dprintf_1(("found key:\n"));
                ss_output_1(dbe_bkey_dprint(1, found_key));
                ss_dprintf_1(("Node:\n"));
                ss_output_1(dbe_bnode_print(NULL, n->n_p, n->n_go->go_idxfd->fd_blocksize));
                ss_derror;
            ss_output_end

            if (dbe_bkey_compare(found_key, ke) < 0) {
                ss_dprintf_4(("dbe_bnode_getunique, DBE_RC_FOUND\n"));
                *p_i = i;
                *p_keys = keys;
                rc = DBE_RC_FOUND;
            } else {
                rc = DBE_RC_END;
            }

        } else {
            /* Key not found.
             */
            rc = DBE_RC_END;

            ss_dprintf_4(("dbe_bnode_getunique, i == count, DBE_RC_END\n"));
        }
        return(rc);
}

/*##**********************************************************************\
 * 
 *		dbe_bnode_getunique_next
 * 
 * Gets unique key value.
 * 
 * Parameters : 
 * 
 *		n - 
 *			
 *			
 *		ke - 
 *			
 *			
 *		found_key - 
 *			
 *			
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_bnode_getunique_next(
        dbe_bnode_t* n,
        dbe_bkey_t* ke,
        int* p_i,
        char** p_keys,
        dbe_bkey_t* found_key)
{
        char* keys;
        dbe_ret_t rc;

        CHK_BNODE(n);

        ss_pprintf_1(("dbe_bnode_getunique_next, addr = %ld\n", n->n_addr));
        ss_dprintf_4(("end key\n"));
        ss_output_4(dbe_bkey_dprint(4, ke));

        (*p_i)++;

        if (*p_i < n->n_count) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int kpos;
                bnode_getkeyoffset(n, *p_i, kpos);
                keys = &n->n_keys[kpos];
            } else {
                keys = *p_keys + dbe_bkey_getlength((dbe_bkey_t*)(*p_keys));
                *p_keys = keys;
            }

            /* Key found.
             */
            dbe_bkey_expand(found_key, found_key, (dbe_bkey_t*)keys);

            ss_dprintf_4(("found key\n"));
            ss_output_4(dbe_bkey_dprint(4, found_key));

            if (dbe_bkey_compare(found_key, ke) < 0) {
                ss_dprintf_4(("dbe_bnode_getunique_next, DBE_RC_FOUND\n"));
                rc = DBE_RC_FOUND;
            } else {
                rc = DBE_RC_END;
            }

        } else {
            /* Key not found.
             */
            rc = DBE_RC_NOTFOUND;

            ss_dprintf_4(("dbe_bnode_getunique_next, DBE_RC_NOTFOUND\n"));
        }
        return(rc);
}

#ifndef SS_LIGHT

/*##**********************************************************************\
 *
 *		dbe_bnode_keyexists
 *
 * Checks if a key value exists in a node.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 *	k - in, use
 *		key value that is searched
 *
 * Return value :
 *
 *      TRUE, key value found
 *      FALSE, key value not found
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bnode_keyexists(dbe_bnode_t* n, dbe_bkey_t* k)
{
        int cmp = -1;
        dbe_bkey_search_t ks;
        char* keys;
        int i;

        CHK_BNODE(n);

        keys = (char*)dbe_bnode_getfirstkey(n);

        SS_PMON_ADD(SS_PMON_BNODE_SEARCH_KEYS);
        dbe_bkey_search_init(&ks, k, DBE_BKEY_CMP_ALL);

        ss_dassert(n->n_level == 0);
        ss_dassert(dbe_bkey_isleaf((dbe_bkey_t*)keys));

        for  (i = 0; i < n->n_count; i++) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int kpos;
                bnode_getkeyoffset(n, i, kpos);
                keys = &n->n_keys[kpos];
            }
            dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
            if (cmp <= 0) {
                break;
            }
            if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
            }
        }
        return(cmp == 0);
}

#endif /* SS_LIGHT */

/*##**********************************************************************\
 *
 *		dbe_bnode_changechildaddr
 *
 * Changes the child node address pointed by 'k' to address 'newaddr'.
 *
 * Parameters :
 *
 *	n - in out, use
 *		node pointer
 *
 *	k - in, use
 *		key value specifying the child node
 *
 *	newaddr - in
 *		new address of child node
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bnode_changechildaddr(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        su_daddr_t newaddr)
{
        dbe_bkey_search_t ks;
        char* keys;
        int klen;
        int count;
        int cmp;
        int i;

        CHK_BNODE(n);
        ss_dprintf_1(("dbe_bnode_changechildaddr:addr %ld, new child addr = %ld \n", n->n_addr, newaddr));
        ss_dassert(n->n_level > 0);
        ss_dassert(n->n_count > 0);

        klen = 0;
        keys = n->n_keys;
        count = n->n_count;
        SS_PMON_ADD(SS_PMON_BNODE_SEARCH_KEYS);
        dbe_bkey_search_init(&ks, k, DBE_BKEY_CMP_ALL);

        ss_dassert(!dbe_bkey_isleaf(dbe_bnode_getfirstkey(n)));
        ss_dassert(dbe_bkey_compare_vtpl(dbe_bnode_getfirstkey(n), k) <= 0);

        i = 0;
        while (count--) {
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int kpos;
                bnode_getkeyoffset(n, i, kpos);
                keys = &n->n_keys[kpos];
            }
            dbe_bkey_search_step(ks, (dbe_bkey_t*)keys, cmp);
            if (cmp <= 0) {
                break;
            }
            if (!(n->n_info & BNODE_MISMATCHARRAY)) {
                klen = dbe_bkey_getlength((dbe_bkey_t*)keys);
                keys += klen;
            }
            i++;
        }

        if (cmp < 0 || i == n->n_count) {
            /* move back to the previous key value */
            if (n->n_info & BNODE_MISMATCHARRAY) {
                int kpos;
                bnode_getkeyoffset(n, i - 1, kpos);
                keys = &n->n_keys[kpos];
            } else {
                keys -= klen;
            }
        }

        dbe_bkey_setaddr((dbe_bkey_t*)keys, newaddr);

        n->n_dirty = TRUE;
}

/*##**********************************************************************\
 *
 *		dbe_bnode_cleanup
 *
 * Does a cleanup (or garbage collection) for a node. Transaction
 * numbers are patched and key values from aborted transactions are
 * removed.
 *
 * Parameters :
 *
 *	n - in out, use
 *		node pointer
 *
 *	p_nkeyremoved - out
 *		Number of key values removed from the node.
 *
 *	p_nmergeremoved - out
 *		Number of merge key values removed from the node.
 *
 *      cd - in, use
 *          client data context needed for blob unlink
 *
 * Return value :
 *
 *      DBE_RC_SUCC             - cleanup done
 *      DBE_RC_NODERELOCATE   - node needs relocateing, nothing done,
 *                                this is always returned before
 *                                before DBE_RC_NODEEMPTY, if both situations
 *                                are true
 *      DBE_RC_NODEEMPTY        - node would become empty, the last key
 *                                value not deleted, but should be deleted
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_bnode_cleanup(
        dbe_bnode_t* n,
        long* p_nkeyremoved,
        long* p_nmergeremoved,
        rs_sysi_t* cd,
        dbe_bnode_cleanup_t cleanup_type)
{
        dbe_trxid_t trxid;
        dbe_trxnum_t committrxnum;
        dbe_trxid_t usertrxid;
        dbe_trxstate_t trxresult;
        bool emptyp;
        bool must_relocate;
        char* keys;
        dbe_bkey_t* k;
        dbe_bkey_t* prevk = NULL;
        dbe_bkey_t* tmpk;
        bool key_deleted;
        dbe_ret_t rc = DBE_RC_SUCC;
        int i;
        int offset;
        dbe_counter_t* ctr;
        su_list_t* mergelist = NULL;
        dbe_trxnum_t mergetrxnum;
        dbe_trxnum_t activemergetrxnum;

        CHK_BNODE(n);
        ss_dprintf_1(("dbe_bnode_cleanup, addr=%ld\n", n->n_addr));
        ss_dassert(p_nkeyremoved != NULL);
        ss_dassert(p_nmergeremoved != NULL);
        ss_dassert(n->n_bonsaip == TRUE);
        ss_dassert(cleanup_type >= DBE_BNODE_CLEANUP_USER);
        ss_trigger("dbe_bnode_cleanup");
        SS_PUSHNAME("dbe_bnode_cleanup");

        *p_nkeyremoved = 0;
        *p_nmergeremoved = 0;

        ctr = n->n_go->go_ctr;

        must_relocate = (n->n_cpnum != dbe_counter_getcpnum(n->n_go->go_ctr));

        if ((n->n_info & BNODE_MISMATCHARRAY) == 0 && !must_relocate) {
            bnode_keysearchinfo_init(n);
        }

        if ((n->n_info & BNODE_MISMATCHARRAY) != 0) {
            bnode_getkeyoffset(n, 0, offset);
            keys = n->n_keys + offset;
        } else {
            keys = n->n_keys;
        }
        k = dbe_bkey_init_ex(cd, n->n_go->go_bkeyinfo);
        dbe_bkey_copy(k, (dbe_bkey_t*)keys);

        ss_dassert(n->n_level == 0);
        ss_dassert(dbe_bkey_isleaf((dbe_bkey_t*)keys));

        for (i = 0; i < n->n_count && rc == DBE_RC_SUCC; ) {
#ifdef SS_DEBUG
            if ((n->n_info & BNODE_MISMATCHARRAY) != 0) {
                bnode_getkeyoffset(n, i, offset);
            } else {
                offset = (int)(keys - n->n_keys);
            }
#endif
            ss_dprintf_2(("dbe_bnode_cleanup:key, i=%d, count=%d, pos=%d\n", i, n->n_count, offset));
            ss_output_2(dbe_bkey_dprint(3, k));
            key_deleted = FALSE;
            if (dbe_bkey_iscommitted((dbe_bkey_t*)keys)) {
                ss_dprintf_2(("dbe_bnode_cleanup:DBE_TRXST_COMMIT\n"));
                trxresult = DBE_TRXST_COMMIT;
            } else {
                trxid = dbe_bkey_gettrxid((dbe_bkey_t*)keys);
                trxresult = dbe_trxbuf_gettrxstate(
                                n->n_go->go_trxbuf,
                                trxid,
                                &committrxnum,
                                &usertrxid);
                switch (trxresult) {
                    case DBE_TRXST_BEGIN:
#ifdef DBE_REPLICATION
                    case DBE_TRXST_TOBEABORTED:
#endif /* DBE_REPLICATION */
                        ss_dprintf_2(("dbe_bnode_cleanup:DBE_TRXST_BEGIN\n"));
                        break;
                    case DBE_TRXST_VALIDATE:
                        ss_dprintf_2(("dbe_bnode_cleanup:DBE_TRXST_VALIDATE\n"));
                        ss_dassert(!DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL));
                        break;
                    case DBE_TRXST_COMMIT:
                        /* Patch the transaction number. */
                        ss_dprintf_2(("dbe_bnode_cleanup:DBE_TRXST_COMMIT\n"));
                        if (must_relocate) {
                            rc = DBE_RC_NODERELOCATE;
                            continue;
                        }
                        dbe_bkey_setcommitted((dbe_bkey_t*)keys);
                        ss_dassert(!DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL));
                        dbe_bkey_settrxnum((dbe_bkey_t*)keys, committrxnum);
                        n->n_dirty = TRUE;
                        break;
                    case DBE_TRXST_ABORT:
                        /* Delete the key value. */
                        ss_dprintf_2(("dbe_bnode_cleanup:DBE_TRXST_ABORT\n"));
                        if (cleanup_type == DBE_BNODE_CLEANUP_USER
                            && dbe_bkey_isblob(k)) 
                        {
                            /* We can not remove keys with blob reference
                             * during merge because there is no way
                             * to keep blob references up to date.
                             */
                            break;
                        }
                        if (must_relocate) {
                            rc = DBE_RC_NODERELOCATE;
                            continue;
                        }
                        (*p_nkeyremoved)++;
                        (*p_nmergeremoved) += dbe_bkey_getmergekeycount(k);
                        if (n->n_count == 1) {
                            /* Node would become empty. */
                            rc = DBE_RC_NODEEMPTY;
                            continue;
                        }
                        emptyp = bnode_deletekey_atpos(n, keys, k, i, TRUE, cd, FALSE);
                        ss_dassert(!emptyp);
                        n->n_dirty = TRUE;
                        key_deleted = TRUE;
                        break;
                    default:
                        ss_rc_error(trxresult);
                }
            }
            if (!key_deleted) {
                /* In key delete case the bnode_delete has moved the next key
                 * value to the current position. Otherwise move to the
                 * next key value.
                 */
                i++;
                if (i >= n->n_count) {
                    break;
                }
                if ((n->n_info & BNODE_MISMATCHARRAY) != 0) {
                    bnode_getkeyoffset(n, i, offset);
                    keys = n->n_keys + offset;
                } else {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }
            } else {
                if (i >= n->n_count) {
                    break;
                }
                if ((n->n_info & BNODE_MISMATCHARRAY) != 0) {
                    bnode_getkeyoffset(n, i, offset);
                    keys = n->n_keys + offset;
                }
            }
            /* Not at the last key, expand the current key.
             */
            dbe_bkey_expand(k, k, (dbe_bkey_t*)keys);
        }

        if (dbe_cfg_splitpurge && rc == DBE_RC_SUCC) {

            dbe_counter_getactivemergetrxnum(
                ctr,
                &mergetrxnum,
                &activemergetrxnum);

            if ((n->n_info & BNODE_MISMATCHARRAY) != 0) {
                bnode_getkeyoffset(n, 0, offset);
                keys = n->n_keys + offset;
            } else {
                keys = n->n_keys;
            }
            dbe_bkey_copy(k, (dbe_bkey_t*)keys);

            ss_dassert(n->n_level == 0);
            ss_dassert(dbe_bkey_isleaf((dbe_bkey_t*)keys));

            for (i = 0; i < n->n_count && rc == DBE_RC_SUCC; ) {
#ifdef SS_DEBUG
                if ((n->n_info & BNODE_MISMATCHARRAY) != 0) {
                    bnode_getkeyoffset(n, i, offset);
                } else {
                    offset = (int)(keys - n->n_keys);
                }
#endif
                ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, key, i=%d, count=%d, pos=%d\n", i, n->n_count, offset));

                i++;
                if (i >= n->n_count) {
                    break;
                }
                if ((n->n_info & BNODE_MISMATCHARRAY) != 0) {
                    bnode_getkeyoffset(n, i, offset);
                    keys = n->n_keys + offset;
                } else {
                    keys += dbe_bkey_getlength((dbe_bkey_t*)keys);
                }

                if (prevk == NULL) {
                    prevk = dbe_bkey_init_ex(cd, n->n_go->go_bkeyinfo);
                }
                tmpk = prevk;
                prevk = k;
                k = tmpk;

                /* Not at the last key, expand the current key.
                 */
                dbe_bkey_expand(k, prevk, (dbe_bkey_t*)keys);

                ss_dassert(dbe_bkey_compare(prevk, k) != 0);

                if (dbe_bkey_iscommitted(prevk) && dbe_bkey_iscommitted(k)) {
                    bool delete_ok;
                    dbe_trxnum_t prevktrxnum;
                    dbe_trxnum_t curktrxnum;

                    ss_output_2(dbe_bkey_dprint_ex(3, "prevk:", prevk));
                    ss_output_2(dbe_bkey_dprint_ex(3, "k:", k));
                    ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, DBE_TRXST_COMMIT\n"));
                    if (cleanup_type == DBE_BNODE_CLEANUP_USER
                        && (dbe_bkey_isblob(prevk) || dbe_bkey_isblob(k))) 
                    {
                        ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, user cleanup and blob, do NOT delete\n"));
                        delete_ok = FALSE;
                    } else {
                        ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, not user cleanup or not blob, delete ok\n"));
                        delete_ok = TRUE;
                    }
                    prevktrxnum = dbe_bkey_gettrxnum(prevk);
                    curktrxnum = dbe_bkey_gettrxnum(k);
                    if (delete_ok
                        && dbe_bkey_isdeletemark(prevk) 
                        && !dbe_bkey_isdeletemark(k)
                        && dbe_bkey_equal_vtpl(prevk, k)
                        && dbe_trxnum_cmp(prevktrxnum, mergetrxnum) <= 0)
                    {
                        ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, delete/insert pair below merge level, delete ok\n"));
                        delete_ok = TRUE;
                    } else {
                        delete_ok = FALSE;
                        ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, not delete/insert pair or not below merge level, do NOT delete\n"));
                    }
                    if (delete_ok) {
                        int prevkcmp;
                        int curkcmp;

                        prevkcmp = dbe_trxnum_cmp(prevktrxnum, activemergetrxnum);
                        ss_dassert(prevkcmp == -1 || prevkcmp == 0 || prevkcmp == 1);
                        
                        curkcmp = dbe_trxnum_cmp(curktrxnum, activemergetrxnum);
                        ss_dassert(curkcmp == -1 || curkcmp == 0 || curkcmp == 1);

                        ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, mergetrxnum=%ld, activemergetrxnum=%ld, prevktrxnum=%ld, curktrxnum=%ld, prevkcmp=%d, curkcmp=%d\n", 
                            DBE_TRXNUM_GETLONG(mergetrxnum), DBE_TRXNUM_GETLONG(activemergetrxnum), 
                            DBE_TRXNUM_GETLONG(prevktrxnum), DBE_TRXNUM_GETLONG(curktrxnum),
                            prevkcmp, curkcmp));

                        delete_ok = (curkcmp != 0) && (prevkcmp == curkcmp);

                        if (delete_ok) {
                            ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, both delete and insert below or above activemergetrxnum, delete ok\n"));
                        } else {
                            ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, delete and insert not below or above activemergetrxnum, do NOT delete\n"));
                        }
                    }
                    if (delete_ok) {
                        dbe_dynbkey_t dk;

                        ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, add to mergelist:keytrxnum=%ld, mergetrxnum=%ld\n", 
                            DBE_TRXNUM_GETLONG(prevktrxnum), 
                            DBE_TRXNUM_GETLONG(mergetrxnum)));
                        ss_dassert(!DBE_TRXNUM_EQUAL(prevktrxnum, DBE_TRXNUM_NULL));
                        ss_dassert(!DBE_TRXNUM_EQUAL(curktrxnum, DBE_TRXNUM_NULL));
                        ss_dassert(dbe_trxnum_cmp(curktrxnum, mergetrxnum) <= 0);
                        if (must_relocate) {
                            rc = DBE_RC_NODERELOCATE;
                            continue;
                        }
                        if (mergelist == NULL) {
                            mergelist = su_list_init(NULL);
                        }
                        dk = NULL;
                        dbe_dynbkey_setbkey(&dk, prevk);
                        su_list_insertlast(mergelist, dk);
                        dk = NULL;
                        dbe_dynbkey_setbkey(&dk, k);
                        su_list_insertlast(mergelist, dk);
                    }
                }
            }
        }
        dbe_bkey_done_ex(cd, k);
        if (prevk != NULL) {
            dbe_bkey_done_ex(cd, prevk);
        }
        if (rc == DBE_RC_SUCC && mergelist != NULL) {
            ss_aassert(su_list_length(mergelist) % 2 == 0);
            while ((k = su_list_getfirst(mergelist)) != NULL) {
                ss_dprintf_2(("dbe_bnode_cleanup:splitpurge, delete from mergelist\n"));
                if (cleanup_type == DBE_BNODE_CLEANUP_MERGE_ALL ) {
                    if (n->n_count <= 1) {
                        /* Node would become empty. */
                        ss_aassert(su_list_length(mergelist) >= 1);
                        ss_aassert(n->n_count != 0);
                        rc = DBE_RC_NODEEMPTY;
                        break;
                    }
                } else {
                    int len;
                    len = su_list_length(mergelist);
                    if (len % 2 == 0 && n->n_count <= 2) {
                        /* Node would become empty. */
                        ss_aassert(len >= 2);
                        ss_aassert(n->n_count != 0);
                        rc = DBE_RC_NODEEMPTY;
                        break;
                    }
                }
                ss_aassert(n->n_count >= 2);
                (*p_nkeyremoved)++;
                (*p_nmergeremoved) += dbe_bkey_getmergekeycount(k);
                rc = dbe_bnode_deletekey(n, k, TRUE, FALSE, TRUE, FALSE, cd, NULL);
                su_rc_assert(rc == DBE_RC_SUCC, rc);
                n->n_dirty = TRUE;
                SsMemFree(k);
                su_list_removefirst(mergelist);
                SS_PMON_ADD(SS_PMON_MERGEPURGESTEP);
            }
            while ((k = su_list_getfirst(mergelist)) != NULL) {
                SsMemFree(k);
                su_list_removefirst(mergelist);
            }
            su_list_done(mergelist);
        }

        ss_dassert(dbe_bnode_test(n));

        ss_trigger("dbe_bnode_cleanup");
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_maxkeylen
 *
 * Returns the maximum key length that can be inserted to the tree node.
 *
 * Parameters :
 *
 *	bufsize - in
 *		buffer size
 *
 * Output params:
 *
 * Return value :
 *
 *      maximum key length
 *
 * Limitations  :
 *
 * Globals used :
 */
uint dbe_bnode_maxkeylen(uint bufsize)
{
        return((bufsize - BNODE_HEADERLEN) / 3);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getdata
 *
 *
 *
 * Parameters :
 *
 *	n -
 *
 *
 *	p_data -
 *
 *
 *	p_len -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_bnode_getdata(
        dbe_bnode_t* n,
        char** p_data,
        int* p_len)
{
        CHK_BNODE(n);

        *p_data = n->n_keys;
        *p_len = n->n_len;
}

/*##**********************************************************************\
 *
 *		dbe_bnode_initempty
 *
 * Initializes an empty b-tree node to a user given buffer. The buffer
 * must be initialized to zero.
 *
 * Parameters :
 *
 *	buf -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_bnode_initempty(
        char* buf)
{
        va_t va;
        dbe_blocktype_t blocktype;

        blocktype = DBE_BLOCK_TREENODE;
        DBE_BLOCK_SETTYPE(buf, &blocktype);
        dbe_bkey_initshortleafbuf((dbe_bkey_t*)BNODE_GETKEYPTR(buf));

        va_setlong(&va, RS_RELID_BADROWS);
        vtpl_appva(dbe_bkey_getvtpl((dbe_bkey_t*)BNODE_GETKEYPTR(buf)), &va);

        va_setlong(&va, SsTime(NULL));
        vtpl_appva(dbe_bkey_getvtpl((dbe_bkey_t*)BNODE_GETKEYPTR(buf)), &va);
}

