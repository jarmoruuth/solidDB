/*************************************************************************\
**  source       * dbe6bkey.h
**  directory    * dbe
**  description  * B+-tree key value system.
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


#ifndef DBE6BKEY_H
#define DBE6BKEY_H

#include <su0svfil.h>

#include <uti0vtpl.h>
#include <uti0vcmp.h>

#include <su0icvt.h>

#include <rs0sysi.h>
#include <rs0relh.h>
#include <rs0key.h>
#include <rs0ttype.h>

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe7cfg.h"
#include "dbe0type.h"

/* Position of the key value inside the node returned by
   dbe_btrsea_getnext and dbe_btrsea_getprev. The values are
   or'ed to the returned value, no both DBE_KEYPOS_FIRST and
   DBE_KEYPOS_LAST can be set at the same time.
*/
typedef enum {
        DBE_KEYPOS_MIDDLE = 0,
        DBE_KEYPOS_FIRST  = 1,
        DBE_KEYPOS_LAST   = 2
} dbe_keypos_t;

typedef enum {
        DBE_BKEY_CMP_VTPL = 100,
        DBE_BKEY_CMP_ALL,
        DBE_BKEY_CMP_DELETE
} dbe_bkey_cmp_t;

/* Different key types. These are or'd into the key info byte.
*/
typedef enum {
        BKEY_1LONGUSED   = (1 << 0),    /* One long field used. */
        BKEY_2LONGUSED   = (1 << 1),    /* Two long fields used. */
        BKEY_LEAF        = (1 << 2),    /* Index leaf key value. */
        BKEY_DELETEMARK  = (1 << 3),    /* Deletemark key value. */
        BKEY_COMMITTED   = (1 << 4),    /* Committed key value. */
        BKEY_CLUSTERING  = (1 << 5),    /* Clustering key value. */
        BKEY_BLOB        = (1 << 6),    /* There are blob attributes in key
                                           v-tuple. */
        BKEY_UPDATE      = (1 << 7)     /* Update key value. Update here means
                                           that ordering columns are not
                                           changed. */
} dbe_bkey_info_t;

typedef dbe_bkey_t*               dbe_dynbkey_t;
typedef struct dbe_bkey_search_st dbe_bkey_search_t;


/************************************************************************/
/*** FOR INTERNAL USE ONLY BEGIN ****************************************/

/* Physical key structure:

        1 byte    key info
        2 bytes   mismatch index
        4 bytes   node address, transaction number, or none
        4 bytes   transaction id or none
        n bytes   variable length vtpl_t, len == vtpl_grosslen
*/

#define BKEY_TYPEOK(k)           BKEY_INFOOK(k)

#define BKEY_INITINFO(k, t)      ((k)->k_.pval[BKEY_INFO] = (ss_byte_t)(t))

#define BKEY_STOREINDEX(k, i)    SS_UINT2_STORETODISK(&(k)->k_.pval[BKEY_INDEX], i)
#define BKEY_STORETRXNUM(k, tn)  SS_UINT4_STORETODISK(&(k)->k_.pval[BKEY_TRXNUM], DBE_TRXNUM_GETLONG(tn))
#define BKEY_STOREADDR(k, a)     SS_UINT4_STORETODISK(&(k)->k_.pval[BKEY_ADDR], a)
#define BKEY_STORETRXID(k, ti)   SS_UINT4_STORETODISK(&(k)->k_.pval[BKEY_TRXID], DBE_TRXID_GETLONG(ti))

#define BKEY_ISLEAF(k)           ((BKEY_GETINFO(k) & BKEY_LEAF) != 0)
#define BKEY_GETTRXID(k)         (DBE_TRXID_INIT(BKEY_LOADTRXID(k)))

#define BKEY_MAXLEN ((DBE_CFG_MAXINDEXBLOCKSIZE / 2) - sizeof(double))

#define CHK_BKEYINFO(ki) ss_dassert(SS_CHKPTR(ki) && (ki)->ki_chk == DBE_CHK_BKEYINFO)

#ifdef SS_GEOS
# define DBE_BKEY_FIXEDLEN  (DBE_CFG_MAXINDEXBLOCKSIZE/3)
#else
# define DBE_BKEY_FIXEDLEN  1
#endif

/* Key value structure in in disk format. The disk format is
   used directly also in memory.
*/
#if defined(SS_NT) /* For degugging. */

#pragma pack(1) /* NONPORTABLE */
struct dbe_bkey_st {
        union {
            ss_byte_t pval[DBE_BKEY_FIXEDLEN];
            struct {
                ss_byte_t k_info;
                ss_byte_t k_index;
                union {
                    struct {
                        ulong   trxnum;
                        ulong   trxid;
                        vtpl_t  vtpl;
                    } k_bonsaileaf;
                    struct {
                        ulong   addr;
                        ulong   trxid;
                        vtpl_t  vtpl;
                    } k_bonsaiindex;
                    struct {
                        vtpl_t  vtpl;
                    } k_permleaf;
                    struct {
                        ulong   addr;
                        vtpl_t  vtpl;
                    } k_permindex;
                } _;
            } lval;
        } k_;
};
#pragma pack() /* NONPORTABLE */

#else /* SS_NT */

struct dbe_bkey_st {
        struct {
            ss_byte_t pval[DBE_BKEY_FIXEDLEN];
        } k_;
};

#endif /* SS_NT */

/* Key search state structure.
*/
struct dbe_bkey_search_st {
        search_state    ks_ss; /* V-tuple search state. */
        dbe_bkey_t*     ks_k;  /* Search key. */
};

/* Key field sizes */
#define BKEY_INFOSIZE    sizeof(ss_byte_t)      /* must be 1 byte type */
#define BKEY_INDEXSIZE   sizeof(ss_uint2_t)     /* must be 2 byte type */
#define BKEY_TRXNUMSIZE  sizeof(ss_uint4_t)     /* must be 4 byte type */
#define BKEY_TRXIDSIZE   sizeof(ss_uint4_t)     /* must be 4 byte type */
#define BKEY_ADDRSIZE    sizeof(su_daddr_t)     /* must be 4 byte type */

/* Field offsets in a key value (Note! These cannot be defined using
   size macros above, because end of macro expansion space in msc.)
*/
#define BKEY_INFO        0      /* 1 byte */
#define BKEY_INDEX       1      /* 2 bytes */
#define BKEY_ADDR        3      /* 4 bytes */
#define BKEY_TRXNUM      3      /* 4 bytes */
#define BKEY_TRXID       7      /* 4 bytes */

/* Macros to retrieve different fields from a key value. */
#define BKEY_GETINFO(k)         ((uint)(k)->k_.pval[BKEY_INFO])
#define BKEY_GETVTPLPTR(k)      ((vtpl_t*)&(k)->k_.pval[BKEY_HEADERLEN(k)])
#define BKEY_ISDELETEMARK(k)    ((BKEY_GETINFO(k) & BKEY_DELETEMARK) != 0)
#define BKEY_ISUPDATE(k)        ((BKEY_GETINFO(k) & BKEY_UPDATE) != 0)

#ifdef DBE_MERGEDEBUG
#define BKEY_GETMERGEDEBUGINFOPTR(k) ((char*)&(k)->k_.pval[11])
#endif /* DBE_MERGEDEBUG */

#define BKEY_LOADINDEX(k)       SS_UINT2_LOADFROMDISK(&(k)->k_.pval[BKEY_INDEX])
#define BKEY_LOADADDR(k)        SS_UINT4_LOADFROMDISK(&(k)->k_.pval[BKEY_ADDR])
#define BKEY_LOADTRXNUM(k)      SS_UINT4_LOADFROMDISK(&(k)->k_.pval[BKEY_TRXNUM])
#define BKEY_LOADTRXID(k)       SS_UINT4_LOADFROMDISK(&(k)->k_.pval[BKEY_TRXID])

#define BKEY_SETINFO(k, t)      ((k)->k_.pval[BKEY_INFO] |= (ss_byte_t)(t))
#define BKEY_SETDELETEMARK(k)   BKEY_SETINFO(k, BKEY_DELETEMARK)


#define BKEY_ILLEGALINFO        (BKEY_LEAF|BKEY_1LONGUSED)
#define BKEY_INFOOK(k)          (BKEY_GETINFO(k) != BKEY_ILLEGALINFO)

#ifdef DBE_MERGEDEBUG
#define BKEY_MAXHEADERLEN       11+5
#else
#define BKEY_MAXHEADERLEN       11
#endif

#define BKEY_HEADERLEN_FIXED    3

/* Key headerlen is the same as key v-tuple offset */
#define BKEY_NLONGUSED(k)       (BKEY_GETINFO(k) & (BKEY_1LONGUSED|BKEY_2LONGUSED))

#ifdef DBE_MERGEDEBUG
#define BKEY_HEADERLEN(k)       ((BKEY_NLONGUSED(k) == 2) \
                                    ? (BKEY_MAXHEADERLEN) \
                                    : (BKEY_HEADERLEN_FIXED + (4 * BKEY_NLONGUSED(k))))
#else /* DBE_MERGEDEBUG */
#define BKEY_HEADERLEN(k)       (BKEY_HEADERLEN_FIXED + (4 * BKEY_NLONGUSED(k)))
#endif /* DBE_MERGEDEBUG */

/*** FOR INTERNAL USE ONLY END ******************************************/
/************************************************************************/

typedef struct {
        uint                ki_maxkeylen;
        ss_debug(dbe_chk_t  ki_chk;)
} dbe_bkeyinfo_t;

va_index_t dbe_bkey_getbloblimit_low(
        uint maxkeylen);

va_index_t dbe_bkey_getbloblimit_high(
        uint maxkeylen);

void dbe_bkeyinfo_init(
        dbe_bkeyinfo_t* bkeyinfo,
        uint maxkeylen);

dbe_bkey_t* dbe_bkey_init(
        dbe_bkeyinfo_t* ki);

SS_INLINE dbe_bkey_t* dbe_bkey_init_ex(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki);

dbe_bkey_t* dbe_bkey_initpermleaf(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        vtpl_t* vtpl);

dbe_bkey_t* dbe_bkey_initleaf(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        vtpl_t* vtpl);

dbe_bkey_t* dbe_bkey_initindex(
        dbe_bkeyinfo_t* ki,
        dbe_trxid_t trxid,
        su_daddr_t addr,
        vtpl_t* vtpl);

void dbe_bkey_initshortleafbuf(
        dbe_bkey_t* k);

void dbe_bkey_initlongleafbuf(
        dbe_bkey_t* k);

void dbe_bkey_done(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_done_ex(
        rs_sysi_t* cd,
        dbe_bkey_t* k);

SS_INLINE uint dbe_bkey_getlength(
        dbe_bkey_t* k);

SS_INLINE vtpl_t* dbe_bkey_getvtpl(
        dbe_bkey_t* k);

void dbe_bkey_setvtpl(
        dbe_bkey_t* tk,
        vtpl_t* vtpl);

void dbe_bkey_settreeminvtpl(
        dbe_bkey_t* tk);

void dbe_bkey_setsearchminvtpl(
        dbe_bkey_t* tk);

void dbe_bkey_setsearchmaxvtpl(
        dbe_bkey_t* tk);

void dbe_bkey_setbkey(
        dbe_bkey_t* tk,
        dbe_bkey_t* sk);

vtpl_index_t dbe_bkey_getmismatchindex(
        dbe_bkey_t* k);

SS_INLINE su_daddr_t dbe_bkey_getaddr(
        dbe_bkey_t* k);

void dbe_bkey_setaddr(
        dbe_bkey_t* k,
        su_daddr_t addr);

SS_INLINE bool dbe_bkey_isblob(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_setblob(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_setclustering(
        dbe_bkey_t* k);

SS_INLINE bool dbe_bkey_isclustering(
        dbe_bkey_t* k);

SS_INLINE bool dbe_bkey_istrxnum(
        dbe_bkey_t* k);

SS_INLINE dbe_trxnum_t dbe_bkey_gettrxnum(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_settrxnum(
        dbe_bkey_t* k,
        dbe_trxnum_t trxnum);

SS_INLINE bool dbe_bkey_istrxid(
        dbe_bkey_t* k);

SS_INLINE dbe_trxid_t dbe_bkey_gettrxid(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_settrxid(
        dbe_bkey_t* k,
        dbe_trxid_t trxid);

void dbe_bkey_removetrxinfo(
        dbe_bkey_t* k);

#ifdef DBE_MERGEDEBUG

void dbe_bkey_setmergedebuginfo(
        dbe_bkey_t* k,
        dbe_trxnum_t readlevel,
        int trxmode);

#endif /* DBE_MERGEDEBUG */

SS_INLINE bool dbe_bkey_isleaf(
        dbe_bkey_t* k);

SS_INLINE bool dbe_bkey_isdeletemark(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_setdeletemark(
        dbe_bkey_t* k);

void dbe_bkey_removedeletemark(
        dbe_bkey_t* k);

SS_INLINE bool dbe_bkey_isupdate(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_setupdate(
        dbe_bkey_t* k);

SS_INLINE bool dbe_bkey_iscommitted(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_setcommitted(
        dbe_bkey_t* k);

long dbe_bkey_getkeyid(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_copy(
        dbe_bkey_t* tk,
        dbe_bkey_t* sk);

void dbe_bkey_copy_keeptargetformat(
        dbe_bkey_t* tk,
        dbe_bkey_t* sk);

void dbe_bkey_copyheader(
        dbe_bkey_t* tk,
        dbe_bkey_t* sk);

void dbe_bkey_reexpand_delete(
        dbe_bkey_t* tk,
        dbe_bkey_t* dk,
        dbe_bkey_t* nk);

void dbe_bkey_init_expand(
        vtpl_expand_state* es);

void dbe_bkey_done_expand(
        vtpl_expand_state* es);

void dbe_bkey_save_expand(
        vtpl_expand_state* es,
        dbe_bkey_t*        ck);

vtpl_index_t dbe_bkey_copy_expand(
        dbe_bkey_t*        tk,
        vtpl_expand_state* es,
        dbe_bkey_t*        sk_header,
        vtpl_index_t       l_tk_area);

SS_INLINE int dbe_bkey_compare(
        dbe_bkey_t* k1,
        dbe_bkey_t* k2);

SS_INLINE int dbe_bkey_compare_vtpl(
        dbe_bkey_t* k1,
        dbe_bkey_t* k2);

int dbe_bkey_compare_header(
        dbe_bkey_t* k1,
        dbe_bkey_t* k2);

int dbe_bkey_compare_deletemark(
        dbe_bkey_t* k1, 
        dbe_bkey_t* k2);

#define DBE_BKEY_COMPARE(k1, k2, cmp) \
        { \
            vtpl_t* vtpl1 = BKEY_GETVTPLPTR(k1); \
            vtpl_t* vtpl2 = BKEY_GETVTPLPTR(k2); \
            cmp = vtpl_compare(vtpl1, vtpl2);\
            if (cmp == 0) { \
                cmp = dbe_bkey_compare_header(k1, k2); \
            } \
        }

#define DBE_BKEY_COMPARE_VTPL(k1, k2, cmp) \
        { \
            vtpl_t* vtpl1 = BKEY_GETVTPLPTR(k1); \
            vtpl_t* vtpl2 = BKEY_GETVTPLPTR(k2); \
            cmp = vtpl_compare(vtpl1, vtpl2); \
        }

#define DBE_BKEY_COMPARE_DELETE(k1, k2, cmp) \
        { \
                vtpl_t* vtpl1 = BKEY_GETVTPLPTR(k1); \
                vtpl_t* vtpl2 = BKEY_GETVTPLPTR(k2); \
                cmp = vtpl_compare(vtpl1, vtpl2);\
                if (cmp == 0) { \
                    cmp = dbe_bkey_compare_deletemark(k1, k2); \
                } \
        }

SS_INLINE bool dbe_bkey_equal_vtpl(
        dbe_bkey_t* k1,
        dbe_bkey_t* k2);

void dbe_bkey_compress(
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* tk,
        dbe_bkey_t* pk,
        dbe_bkey_t* nk);

void dbe_bkey_recompress_insert(
        dbe_bkey_t* tk,
        dbe_bkey_t* pk,
        dbe_bkey_t* nk);

bool dbe_bkey_print(
        void* fp,
        dbe_bkey_t* k);

bool dbe_bkey_print_ex(
        void* fp,
        char* txt,
        dbe_bkey_t* k);

bool dbe_bkey_printvtpl(
        void* fp,
        vtpl_t* vtpl);

bool dbe_bkey_dprint(
        int level,
        dbe_bkey_t* k);

bool dbe_bkey_dprint_ex(
        int level,
        char* txt,
        dbe_bkey_t* k);

bool dbe_bkey_dprintvtpl(
        int level,
        vtpl_t* vtpl);

bool dbe_bkey_test(
        dbe_bkey_t* k);

SS_INLINE void dbe_bkey_search_init(
        dbe_bkey_search_t* ks,
        dbe_bkey_t* sk,
        dbe_bkey_cmp_t bkey_cmp);

void dbe_bkey_search_compress(
        dbe_bkey_search_t* ks,
        dbe_bkey_t* tk);

dbe_bkey_t* dbe_bkey_initsplit(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* split_key);

dbe_bkey_t* dbe_bkey_findsplit(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* full_key,
        dbe_bkey_t* compressed_key);

dbe_ret_t dbe_bkey_setbkeytotval(
        rs_sysi_t* cd,
        rs_key_t* key,
        dbe_bkey_t* bkey,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

dbe_dynbkey_t dbe_dynbkey_setleaf(
        dbe_dynbkey_t* dynbkey,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        vtpl_t* vtpl);

dbe_dynbkey_t dbe_dynbkey_setbkey(
        dbe_dynbkey_t* dynbkey,
        dbe_bkey_t* bkey);

void dbe_dynbkey_expand(
        dbe_dynbkey_t* dyntk,
        dbe_bkey_t* fk,
        dbe_bkey_t* ck);

void dbe_dynbkey_free(
        dbe_dynbkey_t* dynbkey);

SS_INLINE uint dbe_bkey_getmergekeycount(
        dbe_bkey_t* k);

#ifdef SS_DEBUG
#define dbe_bkey_checkheader(k) BKEY_INFOOK(k)
#else
#define dbe_bkey_checkheader(k)
#endif /* not SS_DEBUG */

/*##**********************************************************************\
 * 
 *		key_search_step
 * 
 * Advance key search over one key value. This is implemented as a macro.
 * 
 * Parameters : 
 * 
 *	ks - in out, use
 *		pointer to a key search structure
 *
 *	ck - in, use
 *		compressed next key value of the search
 *
 *      cmp_header - in
 *		if TRUE, also the header parts of the keys are
 *          compared
 *
 *      ret - out
 *		variable into which the comparison result is stored
 *          stored. The return value stored into ret is
 * 
 *              < 0     search key < current key (index key)
 *              = 0     search key = current key (index key), search ends
 *              < 0     search key > current key (index key), search ends
 *
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define dbe_bkey_search_step(ks, ck, ret) \
            { \
                vtpl_index_t index; \
                vtpl_t* vtpl; \
                ss_dassert(dbe_bkey_checkheader(ck)); \
                index = BKEY_LOADINDEX(ck); \
                vtpl = BKEY_GETVTPLPTR(ck); \
                vtpl_dbe_search_step((ret), (ks).ks_ss, index, vtpl, ck); \
            }

/*##**********************************************************************\
 *
 *		dbe_bkey_expand
 *
 * Expands a key value. Target key can not be the same buffer as 
 * compressed key. Target can be the same buffer as full key.
 *
 * Parameters :
 *
 *	tk - use
 *		target key
 *
 *	fk - in
 *		full key
 *
 *	ck - in
 *		compressed key
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define dbe_bkey_expand(M_tk, M_fk, M_ck) \
{ \
        dbe_bkey_t* L_tk = M_tk; \
        dbe_bkey_t* L_fk = M_fk; \
        dbe_bkey_t* L_ck = M_ck; \
        TWO_BYTE_T L_index; \
 \
        ss_dassert(L_tk != NULL); \
        ss_dassert(L_fk != NULL); \
        ss_dassert(L_ck != NULL); \
        ss_dassert(dbe_bkey_checkheader(L_fk)); \
        ss_dassert(dbe_bkey_checkheader(L_ck)); \
        ss_dassert(BKEY_TYPEOK(L_fk)); \
        ss_dassert(BKEY_TYPEOK(L_ck)); \
        ss_debug(L_index = BKEY_LOADINDEX(L_fk)); \
        ss_dassert(L_index == 0); \
        ss_dassert(L_tk != L_ck); \
 \
        if (L_tk == L_fk && BKEY_HEADERLEN(L_fk) != BKEY_HEADERLEN(L_ck)) { \
            dbe_dynbkey_t dyntk = NULL; \
            dbe_dynbkey_expand(&dyntk, L_fk, L_ck); \
            dbe_bkey_copy(L_tk, dyntk); \
            SsMemFree(dyntk); \
        } else {\
            ss_debug_4(dbe_bkey_test(L_fk)); \
            ss_debug_4(dbe_bkey_test(L_ck)); \
 \
            memcpy(L_tk, L_ck->k_.pval, BKEY_HEADERLEN(L_ck)); \
            L_index = 0; \
            BKEY_STOREINDEX(L_tk, L_index); \
 \
            L_index = BKEY_LOADINDEX(L_ck); \
 \
            vtpl_expand( \
                BKEY_GETVTPLPTR(L_tk), \
                BKEY_GETVTPLPTR(L_fk), \
                BKEY_GETVTPLPTR(L_ck), \
                (vtpl_index_t)L_index); \
 \
            ss_debug_4(dbe_bkey_test(L_tk)); \
        } \
}


#if defined(DBE6BKEY_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_bkey_init_ex
 *
 * Initializes an empty leaf key value. Takes buffer from cd if possible.
 *
 * Parameters :
 *
 * Return value - give :
 *
 *      pointer to a key value
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_bkey_t* dbe_bkey_init_ex(rs_sysi_t* cd, dbe_bkeyinfo_t* ki)
{
        dbe_bkey_t* k = NULL;

        CHK_BKEYINFO(ki);
        ss_dassert(BKEY_INFOSIZE == 1);
        ss_dassert(BKEY_INDEXSIZE == 2);
        ss_dassert(BKEY_TRXNUMSIZE == 4);
        ss_dassert(BKEY_ADDRSIZE == 4);
        ss_dassert(BKEY_TRXIDSIZE == 4);
        SS_MEMOBJ_INC(SS_MEMOBJ_BKEY, dbe_bkey_t);

        if (cd != NULL) {
            k = (dbe_bkey_t*)rs_sysi_getbkeybuf(cd);
        }
        if (k == NULL) {
            k = (dbe_bkey_t*)SsMemAlloc(ki->ki_maxkeylen);
        }

        dbe_bkey_initlongleafbuf(k);

        ss_dassert(dbe_bkey_checkheader(k));

        return(k);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_done_ex
 *
 * Releases a key value to the client data.
 *
 * Parameters :
 *
 *	k - in, take
 *		key value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_done_ex(rs_sysi_t* cd, dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_TYPEOK(k));
        SS_MEMOBJ_DEC(SS_MEMOBJ_BKEY);

        if (cd != NULL) {
            ss_debug(memset(k, '\xfe', 256));
            rs_sysi_putbkeybuf(cd, k);
        } else {
            SsMemFree(k);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bkey_getlength
 *
 * Returns the key value length.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      length in bytes
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE uint dbe_bkey_getlength(dbe_bkey_t* k)
{
        vtpl_t* vtpl;

        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_TYPEOK(k));

        vtpl = BKEY_GETVTPLPTR(k);

        return(BKEY_HEADERLEN(k) + (uint)vtpl_grosslen(vtpl));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_getvtpl
 *
 * Returns a pointer to a v-tuple inside a key value.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value - ref :
 *
 *      v-tuple pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE vtpl_t* dbe_bkey_getvtpl(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_TYPEOK(k));

        return(BKEY_GETVTPLPTR(k));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_isblob
 *
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_isblob(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_GETINFO(k) & BKEY_LEAF);

        return((BKEY_GETINFO(k) & BKEY_BLOB) != 0);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setblob
 *
 *
 *
 * Parameters :
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
SS_INLINE void dbe_bkey_setblob(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_GETINFO(k) & BKEY_LEAF);

        BKEY_SETINFO(k, BKEY_BLOB);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setclustering
 *
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_setclustering(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        BKEY_SETINFO(k, BKEY_CLUSTERING);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_isclustering
 *
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_isclustering(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        return((BKEY_GETINFO(k) & BKEY_CLUSTERING) != 0);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_istrxnum
 *
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_istrxnum(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_GETINFO(k) & BKEY_LEAF);

        return((BKEY_GETINFO(k) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) != 0);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_gettrxnum
 *
 * Returns a transaction number stored into a key value. The key value
 * must be a leaf key value.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      transaction number stored with the key value
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxnum_t dbe_bkey_gettrxnum(dbe_bkey_t* k)
{
        dbe_trxnum_t trxnum;

        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_GETINFO(k) & BKEY_LEAF);
        ss_dassert((BKEY_GETINFO(k) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) != 0);

        trxnum = DBE_TRXNUM_INIT(BKEY_LOADTRXNUM(k));

        return(trxnum);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_settrxnum
 *
 * Sets a transaction number in a key value. The key value
 * must be a leaf key value.
 *
 * Parameters :
 *
 *	k - in out, use
 *		key value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_settrxnum(dbe_bkey_t* k, dbe_trxnum_t trxnum)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_GETINFO(k) & BKEY_LEAF);
        ss_dassert((BKEY_GETINFO(k) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) != 0);
        ss_dassert(!DBE_TRXNUM_EQUAL(trxnum, DBE_TRXNUM_NULL));

        BKEY_STORETRXNUM(k, trxnum);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_istrxid
 *
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_istrxid(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        return((BKEY_GETINFO(k) & BKEY_2LONGUSED) != 0);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_gettrxid
 *
 * Returns a transaction id stored into a key value.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      transaction number stored with the key value
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxid_t dbe_bkey_gettrxid(dbe_bkey_t* k)
{
        dbe_trxid_t trxid;

        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert((BKEY_GETINFO(k) & BKEY_2LONGUSED) != 0);

        trxid = BKEY_GETTRXID(k);

        return(trxid);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_settrxid
 *
 * Sets a transaction id in a key value.
 *
 * Parameters :
 *
 *	k - in out, use
 *		key value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_settrxid(dbe_bkey_t* k, dbe_trxid_t trxid)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert((BKEY_GETINFO(k) & BKEY_2LONGUSED) != 0);

        BKEY_STORETRXID(k, trxid);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_isleaf
 *
 * Checks if a key value is a leaf key value.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      TRUE,  leaf key value, the actual key value data
 *      FALSE, index key value, key value separating index leaves,
 *             used only to navigate in the index tree
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_isleaf(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_TYPEOK(k));

        return(BKEY_ISLEAF(k));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_isdeletemark
 *
 * Checks if the key value is a delete mark.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      TRUE    - key value is a delete mark
 *      FALSE   - key value is not a delete mark
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_isdeletemark(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        return(BKEY_ISDELETEMARK(k));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setdeletemark
 *
 * Marks the key value as deleted.
 *
 * Parameters :
 *
 *	k - in out, use
 *		key value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_setdeletemark(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        BKEY_SETINFO(k, BKEY_DELETEMARK);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_isupdate
 *
 * Checks if the key value is an update.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      TRUE    - key value is an update
 *      FALSE   - key value is not an update
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_isupdate(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        return((BKEY_GETINFO(k) & BKEY_UPDATE) != 0);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setupdate
 *
 * Marks the key value as update.
 *
 * Parameters :
 *
 *	k - in out, use
 *		key value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_setupdate(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        BKEY_SETINFO(k, BKEY_UPDATE);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_iscommitted
 *
 * Checks if the key is a committed key value.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      TRUE    - key is committed
 *      FALSE   - key is not committed
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_iscommitted(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        return((BKEY_GETINFO(k) & BKEY_COMMITTED) != 0);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setcommitted
 *
 * Marks the key value as committed.
 *
 * Parameters :
 *
 *	k - in out, use
 *		key value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_setcommitted(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        BKEY_SETINFO(k, BKEY_COMMITTED);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_compare
 *
 * Compares two key values.
 *
 * Parameters :
 *
 *	k1 - in, use
 *		key value 1
 *
 *	k2 - in, use
 *		key value 2
 *
 * Return value :
 *
 *      < 0     k1 < k2
 *      = 0     k1 = k2
 *      > 0     k1 > k2
 *
 * Comments  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 */
SS_INLINE int dbe_bkey_compare(dbe_bkey_t* k1, dbe_bkey_t* k2)
{
        int cmp;
        ss_debug(TWO_BYTE_T index;)

        ss_dassert(k1 != NULL);
        ss_dassert(k2 != NULL);
        ss_dassert(dbe_bkey_checkheader(k1));
        ss_dassert(dbe_bkey_checkheader(k2));
        ss_dassert(BKEY_TYPEOK(k1));
        ss_dassert(BKEY_TYPEOK(k2));
        ss_debug(index = BKEY_LOADINDEX(k1));
        ss_dassert(index == 0);
        ss_debug(index = BKEY_LOADINDEX(k2));
        ss_dassert(index == 0);

        DBE_BKEY_COMPARE(k1, k2, cmp)

        return(cmp);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_compare_vtpl
 *
 * Compares two key values using only the v-tuples, header parts are not
 * included in the comparison.
 *
 * Parameters :
 *
 *	k1 - in, use
 *		key value 1
 *
 *	k2 - in, use
 *		key value 2
 *
 * Return value :
 *
 *      < 0     k1 < k2
 *      = 0     k1 = k2
 *      > 0     k1 > k2
 *
 * Comments  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 */
SS_INLINE int dbe_bkey_compare_vtpl(dbe_bkey_t* k1, dbe_bkey_t* k2)
{
        int cmp;
        ss_debug(TWO_BYTE_T index;)

        ss_dassert(k1 != NULL);
        ss_dassert(k2 != NULL);
        ss_dassert(dbe_bkey_checkheader(k1));
        ss_dassert(dbe_bkey_checkheader(k2));
        ss_dassert(BKEY_TYPEOK(k1));
        ss_dassert(BKEY_TYPEOK(k2));
        ss_debug(index = BKEY_LOADINDEX(k1));
        ss_dassert(index == 0);
        ss_debug(index = BKEY_LOADINDEX(k2));
        ss_dassert(index == 0);

        DBE_BKEY_COMPARE_VTPL(k1, k2, cmp);

        return(cmp);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_equal_vtpl
 *
 * Checks two key values are equal using only the v-tuples, header parts
 * are not included in the comparison.
 *
 * Parameters :
 *
 *	k1 - in, use
 *		key value 1
 *
 *	k2 - in, use
 *		key value 2
 *
 * Return value :
 *
 *      TRUE    k1 = k2
 *      FALSE   k1 != k2
 *
 * Comments  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 */
SS_INLINE bool dbe_bkey_equal_vtpl(dbe_bkey_t* k1, dbe_bkey_t* k2)
{
        vtpl_t* vtpl1 = BKEY_GETVTPLPTR(k1);
        vtpl_t* vtpl2 = BKEY_GETVTPLPTR(k2);

        ss_debug(TWO_BYTE_T index;)

        ss_dassert(k1 != NULL);
        ss_dassert(k2 != NULL);
        ss_dassert(dbe_bkey_checkheader(k1));
        ss_dassert(dbe_bkey_checkheader(k2));
        ss_dassert(BKEY_TYPEOK(k1));
        ss_dassert(BKEY_TYPEOK(k2));
        ss_debug(index = BKEY_LOADINDEX(k1));
        ss_dassert(index == 0);
        ss_debug(index = BKEY_LOADINDEX(k2));
        ss_dassert(index == 0);

        return(vtpl_equal(vtpl1, vtpl2));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_search_init
 *
 * Initializes a key search. The search structure must be allocated by
 * the caller, typically from the stack. Note that because no resources
 * are actually alloced into key search structure, there is no function
 * to release resources (i.e. there is no dbe_bkey_search_done function).
 *
 * Parameters :
 *
 *	ks - out, use
 *		pointer to a key search structure
 *
 *	sk - in, use
 *		search key
 *
 *      bkey_cmp - in
 *		comparison type
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_search_init(
        dbe_bkey_search_t* ks,
        dbe_bkey_t* sk,
        dbe_bkey_cmp_t bkey_cmp)
{
        vtpl_t* vtpl = BKEY_GETVTPLPTR(sk);

        ss_debug(TWO_BYTE_T index;)

        ss_dassert(sk != NULL);
        ss_dassert(dbe_bkey_checkheader(sk));
        ss_dassert(BKEY_TYPEOK(sk));
        ss_debug(index = BKEY_LOADINDEX(sk));
        ss_dassert(index == 0);

        ks->ks_k = sk;
        vtpl_search_init(&ks->ks_ss, vtpl, sk);
        switch (bkey_cmp) {
            case DBE_BKEY_CMP_VTPL:
                break;
            case DBE_BKEY_CMP_ALL:
                vtpl_search_setcmpfn(
                    &ks->ks_ss,
                    (int (*)(void*, void*))dbe_bkey_compare_header);
                break;
            case DBE_BKEY_CMP_DELETE:
                vtpl_search_setcmpfn(
                    &ks->ks_ss,
                    (int (*)(void*, void*))dbe_bkey_compare_deletemark);
                    break;
            default:
                ss_error;
        }
}

/*##**********************************************************************\
 *
 *		dbe_bkey_getmergekeycount
 *
 * Returns the number of merge keys from a single key value. Merge limit
 * is calculated using a fixed key value length. If the key value length
 * is greater that the fixed length, it is counted as multiple key values
 * in merge key counters.
 *
 * NOTE! Started to use actual number of key values for merge.
 *
 * Parameters :
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
SS_INLINE uint dbe_bkey_getmergekeycount(dbe_bkey_t* k __attribute__ ((unused)))
{
        return(1);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_copy
 *
 * Copies target key into source key.
 *
 * Parameters :
 *
 *	tk - out, use
 *		target key
 *
 *	sk - in, use
 *		source key
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bkey_copy(dbe_bkey_t* tk, dbe_bkey_t* sk)
{
        ss_dassert(tk != NULL);
        ss_dassert(sk != NULL);
        ss_dassert(dbe_bkey_checkheader(sk));
        ss_dassert(BKEY_TYPEOK(sk));

        ss_debug_4(dbe_bkey_test(sk));

        memcpy(tk, sk, dbe_bkey_getlength(sk));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_getaddr
 *
 * Returns a node address stored into a key value. The key value must
 * be an index key value.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      address stored with the key value
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE su_daddr_t dbe_bkey_getaddr(dbe_bkey_t* k)
{
        FOUR_BYTE_T addr;

        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert((BKEY_GETINFO(k) & BKEY_LEAF) == 0);
        ss_dassert((BKEY_GETINFO(k) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) != 0);

        addr = BKEY_LOADADDR(k);

        return((su_daddr_t)addr);
}

#endif /* defined(DBE6BKEY_C) || defined(SS_USE_INLINE) */

#endif /* DBE6BKEY_H */
