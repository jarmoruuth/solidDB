/*************************************************************************\
**  source       * dbe0type.h
**  directory    * dbe
**  description  * Dbe type definitions.
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


#ifndef DBE0TYPE_H
#define DBE0TYPE_H

#include <ssstddef.h>

#include <ssc.h>
#include <ssdebug.h>
#include <sslimits.h>
#include <ssint8.h>

#include <uti0va.h>
#include <uti0vtpl.h>

#include <su0types.h>
#include <su0vers.h>
#include <su0bflag.h>

#include <rs0relh.h>

#include "dbe9type.h"
#include "dbe0erro.h"

#define CHK_TRXNUM(t)   ss_dassert(t.chk == DBE_CHK_TRXNUM)
#define CHK_TRXID(t)    ss_dassert(t.chk == DBE_CHK_TRXID)

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)
#define DBE_LOG_INSIDEACTIONGATE
#endif

#define IO_MANAGER
/* #define DBE_NONBLOCKING_PAGEFLUSH *//*Pete removed temporarily */
#define DBE_ONLYDELETEMARK_OPT
#define DBE_NEXTNODEBUG
#define DBE_DDRECOV_BUGFIX

#define DBE_CPNUM_NULL  ((dbe_cpnum_t)0)
#define DBE_CPNUM_MIN  ((dbe_cpnum_t)1)

#define DBE_TRXNUM_INIT(n)      dbe_trxnum_init(n)
#define DBE_TRXNUM_NULL         dbe_trxnum_null
#define DBE_TRXNUM_MIN          dbe_trxnum_min
#define DBE_TRXNUM_MAX          dbe_trxnum_max
#define DBE_TRXNUM_EQUAL(trxnum1,trxnum2)  dbe_trxnum_equal(trxnum1, trxnum2)
#define DBE_TRXNUM_CMP_EX(trxnum1,trxnum2) dbe_trxnum_cmp(trxnum1, trxnum2)
#define DBE_TRXNUM_SUM(t, n)    dbe_trxnum_sum(t, n)
#define DBE_TRXNUM_GETLONG(t)   dbe_trxnum_getlong(t)
#define DBE_TRXNUM_ISNULL(tn)   (dbe_trxnum_equal(tn, dbe_trxnum_null))
#define dbe_trxnum_isnull       DBE_TRXNUM_ISNULL

#define DBE_TRXID_INIT(n)       dbe_trxid_init(n)
#define DBE_TRXID_NULL          dbe_trxid_null
#define DBE_TRXID_ILLEGAL       dbe_trxid_illegal
#define DBE_TRXID_MIN           dbe_trxid_min
#define DBE_TRXID_MAX           dbe_trxid_max
#define DBE_TRXID_EQUAL(trxid1,trxid2)  dbe_trxid_equal(trxid1, trxid2)
#define DBE_TRXID_CMP_EX(trxid1,trxid2) dbe_trxid_cmp(trxid1, trxid2)
#define DBE_TRXID_SUM(t, n)     dbe_trxid_sum(t, n)
#define DBE_TRXID_GETLONG(t)    dbe_trxid_getlong(t)
#define DBE_TRXID_ISNULL(trxid1)  (dbe_trxid_equal(trxid1, dbe_trxid_null))
#define DBE_TRXID_NOTNULL(trxid1)  (!dbe_trxid_equal(trxid1, dbe_trxid_null))
#define dbe_trxid_isnull           DBE_TRXID_NOTNULL

#define DBE_BLOBSIZE_UNKNOWN ((dbe_blobsize_t)-1)
#define DBE_BLOBSIZE_MAX     (SS_INT4_MAX)

#define DBE_BLOBID_CMP(bi1, bi2) \
    (sizeof(dbe_blobid_t) <= sizeof(int) ? \
     ((int)((bi1) - (bi2))) : \
     (((bi1) - (bi2)) < 0 ? -1 : (((bi1) == (bi2)) ? 0 : 1)))

typedef ss_int8_t dbe_blobg2size_t;
typedef ss_int8_t dbe_blobg2id_t;

#define DBE_BLOBG2ID_NULL \
        SsInt8InitFrom2Uint4s(0U, 0U)

#define DBE_BLOBG2SIZE_ASSIGN_SIZE(p_bs, s) \
        SsInt8SetUint4(p_bs, s)

#define DBE_BLOBG2SIZE_ADDASSIGN_SIZE(p_bs, s) \
        SsInt8AddUint4(p_bs, *(p_bs), s)

#define DBE_BLOBG2SIZE_SUBTRACTASSIGN_SIZE(p_bs, s) \
        SsInt8SubtractInt8(p_bs, *(p_bs), SsInt8InitFrom2Uint4s(0, s))

#define DBE_BLOBG2SIZE_SUBTRACTASSIGN_BLOBSIZE(p_bs1, bs2) \
        SsInt8SubtractInt8(p_bs1, *(p_bs1), bs2)

#define DBE_BLOBG2SIZE_PUTTODISK(p_dbuf, bs) \
{\
        ss_uint4_t u4 = SsInt8GetLeastSignificantUint4(bs);\
        ss_byte_t* pd = (ss_byte_t*)(p_dbuf);\
        SS_UINT4_STORETODISK(pd, u4);\
        u4 = SsInt8GetMostSignificantUint4(bs);\
        pd += sizeof(u4);\
        SS_UINT4_STORETODISK(pd, u4);\
}

#define DBE_BLOBG2SIZE_GETFROMDISK(p_dbuf) \
        SsInt8InitFrom2Uint4s(SS_UINT4_LOADFROMDISK((ss_byte_t*)(p_dbuf) + sizeof(ss_uint4_t)),\
                              SS_UINT4_LOADFROMDISK(p_dbuf))

#define DBE_BLOBG2SIZE_IS0(bs) \
        SsInt8Is0(bs)

#define DBE_BLOBG2SIZE_EQUAL(bs1, bs2) \
        SsInt8Equal(bs1, bs2)

#define DBE_BLOBG2SIZE_CMP(bs1, bs2) \
        SsInt8Cmp(bs1, bs2)

#define DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(bs) \
        SsInt8GetLeastSignificantUint4(bs)

#define DBE_BLOBG2SIZE_GETMOSTSIGNIFICANTUINT4(bs) \
        SsInt8GetMostSignificantUint4(bs)

#define DBE_BLOBG2SIZE_SET2UINT4S(p_bs, u4hi, u4lo) \
        SsInt8Set2Uint4s(p_bs, u4hi, u4lo)

#define DBE_BLOBG2ID_PUTTODISK(p_dbuf, bid) \
        DBE_BLOBG2SIZE_PUTTODISK(p_dbuf, bid)

#define DBE_BLOBG2ID_GETFROMDISK(p_dbuf) \
        DBE_BLOBG2SIZE_GETFROMDISK(p_dbuf)

#define DBE_BLOBG2ID_SET2UINT4S(p_bid, u4hi, u4lo) \
        SsInt8Set2Uint4s(p_bid, u4hi, u4lo)

#define DBE_BLOBG2ID_INITFROM2UINT4S(u4hi, u4lo) \
        SsInt8InitFrom2Uint4s(u4hi, u4lo)

#define DBE_BLOBG2ID_ADDASSIGN_UINT2(p_bid, u2) \
        SsInt8AddUint2(p_bid, *p_bid, u2)

#define DBE_BLOBG2ID_ASSIGN_UINT4(p_bid, u4) \
        SsInt8SetUint4(p_bid, u4)

#define DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid) \
        SsInt8GetLeastSignificantUint4(bid)

#define DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid) \
        SsInt8GetMostSignificantUint4(bid)

#define DBE_BLOBG2ID_EQUAL(bid1, bid2) \
        SsInt8Equal(bid1, bid2)

#define DBE_BLOBG2ID_CMP(bid1, bid2) \
        SsInt8Cmp(bid1, bid2)

#define DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bid) \
        SsInt8GetLeastSignificantUint4(bid)

#define DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bid) \
        SsInt8GetMostSignificantUint4(bid)

#ifdef SS_DEBUG

typedef struct {
        ss_debug(int chk;)
        ss_int4_t    num;
} dbe_trxnum_t;                 /* transaction number type */

typedef struct {
        ss_debug(int chk;)
        ss_int4_t    id;
} dbe_trxid_t;                  /* transaction id type */

#else /* SS_DEBUG */

typedef ss_int4_t dbe_trxnum_t;
typedef ss_int4_t dbe_trxid_t;

#endif /* SS_DEBUG */

extern dbe_trxnum_t dbe_trxnum_null;
extern dbe_trxnum_t dbe_trxnum_min;
extern dbe_trxnum_t dbe_trxnum_max;

dbe_trxnum_t dbe_trxnum_init(
        long n);

dbe_trxnum_t dbe_trxnum_initfromtrxid(
        dbe_trxid_t trxid);

dbe_trxnum_t dbe_trxnum_sum(
        dbe_trxnum_t trxnum,
        int n);

long dbe_trxnum_getlong(
        dbe_trxnum_t trxnum);

SS_INLINE int dbe_trxnum_cmp(
        dbe_trxnum_t trxnum1,
        dbe_trxnum_t trxnum2);

bool dbe_trxnum_equal(
        dbe_trxnum_t trxnum1,
        dbe_trxnum_t trxnum2);

extern dbe_trxid_t  dbe_trxid_null;
extern dbe_trxid_t  dbe_trxid_illegal;
extern dbe_trxid_t  dbe_trxid_min;
extern dbe_trxid_t  dbe_trxid_max;

dbe_trxid_t dbe_trxid_init(
        long n);

dbe_trxid_t dbe_trxid_initfromtrxnum(
        dbe_trxnum_t trxnum);

dbe_trxid_t dbe_trxid_sum(
        dbe_trxid_t trxid,
        int n);

long dbe_trxid_getlong(
        dbe_trxid_t trxid);

SS_INLINE int dbe_trxid_cmp(
        dbe_trxid_t trxid1,
        dbe_trxid_t trxid2);

bool dbe_trxid_equal(
        dbe_trxid_t trxid1,
        dbe_trxid_t trxid2);

typedef ss_uint4_t dbe_cpnum_t;         /* checkpoint number type */
typedef ss_uint4_t dbe_logfnum_t;       /* log file number type */
typedef ss_uint4_t dbe_blobsize_t;      /* blob total size in bytes */
typedef ss_int4_t  dbe_blobid_t;        /* blob ID */
typedef ss_uint4_t dbe_lockname_t;      /* lock name for lock manager */

/* The following constant defines the size of the blob reference 
 * record in the end of a blob va
 */
#define DBE_VABLOBREF_SIZE \
        (sizeof(dbe_blobid_t) + sizeof(dbe_blobsize_t) +\
         sizeof(su_daddr_t) + sizeof(uchar))

typedef struct dbe_db_st            dbe_db_t;       /* dbe0db.c */
typedef struct dbe_user_st          dbe_user_t;     /* dbe0user.c */
typedef struct dbe_trx_st           dbe_trx_t;      /* dbe0trx.c */

typedef struct dbe_index_st         dbe_index_t;    /* dbe5inde.c */
typedef struct dbe_indsea_st        dbe_indsea_t;   /* dbe5isea.c */
typedef struct dbe_search_st        dbe_search_t;   /* dbe4srch.h */

typedef struct dbe_blobmgr_st       dbe_blobmgr_t;  /* dbe6bmgr.h */
typedef struct dbe_blobwritestream_st dbe_blobwritestream_t; /* dbe6bmgr.h */
typedef struct dbe_file_st          dbe_file_t;     /* dbe6finf.h */
typedef struct dbe_gobj_st          dbe_gobj_t;     /* dbe6gobj.h */
typedef struct dbe_trxinfo_st       dbe_trxinfo_t;  /* dbe7trxi.h */
typedef struct dbe_counter_st       dbe_counter_t;  /* dbe7ctr.h */
typedef struct dbe_gtrs_st          dbe_gtrs_t;     /* dbe7gtrs.h */
typedef struct dbe_startrec_st      dbe_startrec_t; /* dbe8srec.h */
typedef struct dbe_logfile_st       dbe_logfile_t;  /* dbe6log.h */
typedef struct dbe_rflog_st         dbe_rflog_t;    /* dbe6log.h */
typedef struct dbe_lockmgr_st       dbe_lockmgr_t;  /* dbe6lmgr.h */
typedef struct dbe_seq_st           dbe_seq_t;      /* dbe0seq.h */
typedef struct dbe_bkey_st          dbe_bkey_t;     /* dbe0bkey.h */
typedef struct dbe_tuplestate_st    dbe_tuplestate_t;/* dbe4tupl.h */
#ifndef DBE_BNODE_T_DEFINED
#define DBE_BNODE_T_DEFINED
typedef struct dbe_bnode_st         dbe_bnode_t;    /* dbe6bnod.h */
#endif
typedef struct dbe_srk_st           dbe_srk_t;      /* dbe6srk.h */
typedef struct dbe_bkrs_st          dbe_bkrs_t;     /* dbe6bkrs.h */
typedef struct dbe_locktran_st      dbe_locktran_t; /* dbe6lmgr.h */
typedef struct dbe_filedes_st       dbe_filedes_t;  /* dbe6finf.h */
typedef struct dbe_btree_st         dbe_btree_t;    /* dbe6btree.h */
typedef struct dbe_log_st           dbe_log_t;      /* dbe6log.h */
typedef struct dbe_iomgr_st         dbe_iomgr_t;    /* dbe6iom.h */
typedef struct dbe_iomgr_flushbatch_st dbe_iomgr_flushbatch_t;/* dbe6iom.h */
typedef struct dbe_rtrxbuf_st       dbe_rtrxbuf_t;  /* dbe7rtrx.h */
typedef struct dbe_cache_st         dbe_cache_t;    /* dbe8cach.h */
typedef struct dbe_cacheslot_st     dbe_cacheslot_t;/* dbe8cach.h */


typedef char* (SS_CALLBACK *dbe_encrypt_t)(void *c, su_daddr_t daddr, char *page,
                               int npages, size_t pagesize);
typedef bool  (SS_CALLBACK *dbe_decrypt_t)(void *c, su_daddr_t daddr, char *page,
                              int npages, size_t pagesize);

#ifdef SS_MME
#if defined(SS_MMEG2) /* && !defined(DBE_NOMME) */
typedef struct mme_st               mme_t;
typedef struct mme_search_st        mme_search_t;

#define dbe_mme_t                   mme_t
#define dbe_mme_search_t            mme_search_t

#define mme_locklist_t              dbe_mmlocklst_t
#define mme_locklist_init           dbe_mmlocklst_init
#define mme_locklist_replicafree    dbe_mmlocklst_replicafree
#define mme_locklist_commit         dbe_mmlocklst_commit
#define mme_locklist_rollback       dbe_mmlocklst_rollback
#define mme_locklist_stmt_commit    dbe_mmlocklst_stmt_commit
#define mme_locklist_stmt_rollback  dbe_mmlocklst_stmt_rollback

typedef struct mme_locklist_st      mme_locklist_t;

#else

typedef struct dbe_mme_st           dbe_mme_t;
typedef struct dbe_mme_search_st    dbe_mme_search_t;

#define dbe_mme_locklist_t dbe_mmlocklst_t
#define dbe_mme_locklist_init dbe_mmlocklst_init
#define dbe_mme_locklist_replicafree dbe_mmlocklst_replicafree
#define dbe_mme_locklist_commit dbe_mmlocklst_commit
#define dbe_mme_locklist_rollback dbe_mmlocklst_rollback
#define dbe_mme_locklist_stmt_commit dbe_mmlocklst_stmt_commit
#define dbe_mme_locklist_stmt_rollback dbe_mmlocklst_stmt_rollback

typedef struct dbe_mme_locklist_st dbe_mme_locklist_t;

#endif

#define DBE_MIN_MMEPAGES  16
#endif

typedef struct dbe_logdata_st dbe_logdata_t;

typedef enum {
        DBSTATE_NEW = 0,
        DBSTATE_CLOSED,
        DBSTATE_CRASHED,
        DBSTATE_BROKENNETCOPY
} dbe_dbstate_t;

typedef enum {
        DBE_BACKUPLM_DEFAULT, /* temporary type: not specified expicitly */
        DBE_BACKUPLM_DELETELOGS,
        DBE_BACKUPLM_KEEPLOGS
} dbe_backuplogmode_t;


/* HSB-'roles' in dbe. these values are not persistent so. actual enums can be changed */
typedef enum {
        DBE_HSB_STANDALONE = 0,
        DBE_HSB_PRIMARY = 1,
        DBE_HSB_SECONDARY = 2,
        DBE_HSB_PRIMARY_UNCERTAIN = 8,
        DBE_HSB_NOT_KNOWN = 100
} dbe_hsbmode_t;

typedef enum {
        DBE_HSBCREATECP_BEGINATOMIC,
        DBE_HSBCREATECP_ENDATOMIC,
        DBE_HSBCREATECP_START
} dbe_hsbcreatecp_t;

/* HSBG2 recovery flags.
 */
#define HSBG2_RECOVERY_GENERATENEWID    1
#define HSBG2_RECOVERY_USELOCALLOGPOS   2

/* Insert and delete operation mode flags, or'ed together.
 */
typedef enum {
        DBE_INDEXOP_COMMITTED   = (int)SU_BFLAG_BIT(0),
        DBE_INDEXOP_BLOB        = (int)SU_BFLAG_BIT(1),
        DBE_INDEXOP_CLUSTERING  = (int)SU_BFLAG_BIT(2),
        DBE_INDEXOP_DELETEMARK  = (int)SU_BFLAG_BIT(3),
        DBE_INDEXOP_NOCHECK     = (int)SU_BFLAG_BIT(4),
        DBE_INDEXOP_UPDATE      = (int)SU_BFLAG_BIT(5),
        DBE_INDEXOP_PRELOCKED   = (int)SU_BFLAG_BIT(6)
} dbe_indexop_mode_t;

typedef enum {
        LOCK_FREE,      /* lock is free, the null mode */
        LOCK_IS,        /* intention share lock mode */
        LOCK_IX,        /* intention exclusive lock mode */
        LOCK_S,         /* shared lock mode */
        LOCK_SIX,       /* share and intention exclusive lock mode */
        LOCK_U,         /* update lock mode */
        LOCK_X,         /* exclusive lock mode */
        LOCK_NMODES     /* Number of lock modes */
} dbe_lock_mode_t;

typedef enum {
        LOCK_OK,        /* Lock acquired */
        LOCK_TIMEOUT,   /* Wait timed out. */
        LOCK_DEADLOCK,  /* Deadlock */
        LOCK_WAIT       /* Wait for a lock. */
} dbe_lock_reply_t;

typedef enum {
        DBE_CURSOR_SELECT,      /* SELECT */
        DBE_CURSOR_FORUPDATE,   /* SELECT .. FOR UPDATE */
        DBE_CURSOR_UPDATE,      /* searched UPDATE */
        DBE_CURSOR_DELETE,      /* searched DELETE */
#ifdef SS_MME
        DBE_CURSOR_SYSTEM,      /* system internal search */
#endif
        DBE_CURSOR_SASELECT     /* Sa search from M-table */
} dbe_cursor_type_t;

typedef enum {
        DBE_SEARCH_INVALIDATE_COMMIT,
        DBE_SEARCH_INVALIDATE_ISOLATIONCHANGE
} dbe_search_invalidate_t;

typedef enum {
        DBE_BACKUPFILE_NULL,
        DBE_BACKUPFILE_DB,
        DBE_BACKUPFILE_LOG,
        DBE_BACKUPFILE_INIFILE,
        DBE_BACKUPFILE_SOLMSGOUT,
        DBE_BACKUPFILE_MME,
        DBE_BACKUPFILE_OTHER
} dbe_backupfiletype_t;

typedef enum {
        DBE_DURABILITY_RELAXED  = 1,
        DBE_DURABILITY_ADAPTIVE = 2,
        DBE_DURABILITY_STRICT  = 3
} dbe_durability_t;

typedef enum {
        DBE_DB_TRXTYPE_COMMIT,
        DBE_DB_TRXTYPE_ABORT,
        DBE_DB_TRXTYPE_ROLLBACK
} dbe_db_trxtype_t;

typedef enum {
        DBE_MERGEADVANCE_END = 0,       /* Merge completed (must be zero) */
        DBE_MERGEADVANCE_PART_END,      /* One merge part completed. */
        DBE_MERGEADVANCE_CONT           /* Continue merge. */
} dbe_mergeadvance_t;

typedef struct {
        rs_relh_t* (*rc_getrelhfun)(
                        void* ctx,
                        rs_entname_t* relname,
                        void* p_priv);
        rs_relh_t* (*rc_getrelhfun_byrelid)(
                        void* ctx,
                        ulong relid);
        void       (*rc_refreshrbuffun)(void* ctx);
        void       (*rc_reloadrbuffun)(void* ctx);
        void*      rc_rbufctx;
} dbe_db_recovcallback_t;

typedef struct {
        long    esclim_check;
        long    esclim_read;
        long    esclim_lock;
        bool    esclim_allowlockbounce;
} dbe_escalatelimits_t;

typedef struct dbe_quicksearch_st dbe_quicksearch_t;

/* Structure used by the quick search facility.
 */
struct dbe_quicksearch_st {

        /* System fields, these are initialized by the system.
         */
        void*       qs_search;              /* Search pointer. */
        void*       qs_indexsearch;         /* Search pointer. */
        dbe_ret_t   (*qs_nextorprev)(       /* Fetch function. */
                        dbe_quicksearch_t*, /* Quick search object. */
                        bool);              /* Next = TRUE, prev = FALSE, */
        void        (*qs_write)(            /* RPC write function. */
                        dbe_quicksearch_t*);/* Quick search object. */

        /* User fields, these must be initialized by the user.
         */
        int         (*qs_writelong)(        /* RPC long write function */
                        void*,              /* rpc_ses_t* rses */
                        long);              /* long l */
        int         (*qs_writedata)(        /* RPC data write function. */
                        void*,              /* rpc_ses_t* rses */
                        void*,              /* void* data */
                        int);               /* int datalen */
        void*       qs_rses;                /* rpc_ses_t* */
};

/* Different replication operation types.
 * These are here only temporarily.
 */
typedef enum {
        /* External enum values used from dbe. */
        REP_INSERT              = 100,
        REP_DELETE              = 101,
        REP_COMMIT              = 102,
        REP_ABORT               = 103,
        REP_STMTCOMMIT          = 104,
        REP_STMTABORT           = 105,
        REP_SEQSPARSE           = 106,
        REP_SEQDENSE            = 107,
        REP_SQLINIT             = 108,
        REP_SQL                 = 109,
        REP_COMMIT_CHECK        = 110,
        REP_ABORTALL            = 111,
        REP_PING                = 112,
        REP_INC_SYNCTUPLEVERSION= 113,
        REP_INC_SYNCMSGID       = 114,
        REP_STMTCOMMIT_GROUP    = 115,
        REP_STMTABORT_GROUP     = 116,
        REP_COMMIT_NOFLUSH      = 117,
        REP_CATCHUP_DONE        = 118,
        REP_CLEANUP_MAPPING     = 119,
        REP_CLEANSAVEOPLIST     = 120
} rep_type_t;

typedef enum {
        REP_STATE_INIT,
        REP_STATE_OPLIST,
        REP_STATE_RPC,
        REP_STATE_DONE
} rep_state_t;


typedef struct {
        ss_debug(long rp_chk;)
        bool        rp_activep;
        bool        rp_donep;
        su_ret_t    rp_rc;
        rs_relh_t*  rp_relh;
        char*       rp_sqlstr;
        char*       rp_sqlauthid;
        char*       rp_sqlschema;
        char*       rp_sqlcatalog;
        dbe_trxid_t rp_trxid;
        dbe_trxid_t rp_stmttrxid;
        dbe_trxid_t rp_tupletrxid;
        long        rp_seqid;
        vtpl_t*     rp_vtpl;
        va_t        rp_vabuf;
        bool        rp_isblob;
        long        rp_id;
        rs_sysi_t*  rp_cd;
        bool        rp_flushallowed;
        rep_type_t  rp_type;
        bool        rp_isddl;
        bool        rp_hsbflush;
        void*       rp_ep;
        rep_state_t rp_state;
        ss_debug(long rp_count;) /* Calloc initializes to zero */
} rep_params_t;

#define DBE_CHK_REPPARAMS 10099
#define CHK_RP(rp) ss_dassert(SS_CHKPTR(rp) && (rp)->rp_chk == DBE_CHK_REPPARAMS)

typedef struct {
        void*      (*hsbctx_init)(void* initctx);
        void       (*hsbctx_done)(void* ctx);
        void       (*hsbsqlctx_done)(void* ctx);
        rs_sysi_t* (*hsbctx_getcd)(void* ctx);
} dbe_hsbctx_funs_t;

typedef struct {
        long      dk_keyid;
        char*     dk_keyname;
        ss_int8_t dk_compressedbytes;
        ss_int8_t dk_fullbytes;
        ss_int8_t dk_pages;
} dbe_keynameid_t;

void dbe_type_updateconst(dbe_counter_t* ctr);

#ifndef SS_DEBUG
#define dbe_trxnum_init(n)              (dbe_trxnum_t)(n)
#define dbe_trxnum_initfromtrxid(n)     (dbe_trxnum_t)(n)
#define dbe_trxnum_getlong(tn)          (tn)
#define dbe_trxnum_equal(tn1, tn2)      ((tn1) == (tn2))
#define dbe_trxid_init(n)               (dbe_trxid_t)(n)
#define dbe_trxid_initfromtrxnum(n)     (dbe_trxid_t)(n)
#define dbe_trxid_getlong(ti)           (ti)
#define dbe_trxid_equal(ti1, ti2)       ((ti1) == (ti2))
#endif /* SS_DEBUG */

#ifdef SS_MYSQL

#ifndef MME_RVAL_T_DEFINED
#define MME_RVAL_T_DEFINED
typedef struct mme_rval_st mme_rval_t;
#endif

typedef struct mme_storage_st mme_storage_t;
typedef struct mme_vtrie_st  mme_vtrie_t;
typedef struct mme_bnode_st mme_bnode_t;
typedef struct mme_bcur_st  mme_bcur_t;
typedef ss_uint2_t cplen_t;
typedef struct mme_index_st mme_index_t;
typedef struct mme_ipos_st  mme_ipos_t;
typedef struct mme_page_st mme_page_t;
typedef struct mme_pagelink_st mme_pagelist_t;
typedef struct mme_pagescan_st mme_pagescan_t;
typedef enum {
    MME_RVAL_NORMAL,
    MME_RVAL_KEYREF
} mme_rval_type_t;

#endif /* SS_MYSQL */

#if defined(DBE0TYPE_C) || defined(SS_USE_INLINE)

SS_INLINE int dbe_trxnum_cmp(dbe_trxnum_t trxnum1, dbe_trxnum_t trxnum2)
{
        ss_int4_t diff;
        
        CHK_TRXNUM(trxnum1);
        CHK_TRXNUM(trxnum2);

        diff = (ss_int4_t)(dbe_trxnum_getlong(trxnum1) - dbe_trxnum_getlong(trxnum2));
        /* NOTE! NULL num (0) must be smaller than any other value. */
        if (diff == 0) {
            return(0);
        } else if (dbe_trxnum_getlong(trxnum1) == 0) {
            return(-1);
        } else if (dbe_trxnum_getlong(trxnum2) == 0) {
            return (1);
        }
        /* Must always return -1, 0 or 1. */
        return (diff < 0 ? -1 : 1);
}

SS_INLINE int dbe_trxid_cmp(dbe_trxid_t trxid1, dbe_trxid_t trxid2)
{
        ss_int4_t diff;
        
        CHK_TRXID(trxid1);
        CHK_TRXID(trxid2);

        diff = dbe_trxid_getlong(trxid1) - dbe_trxid_getlong(trxid2);
        /* NOTE! NULL id (0) must be smaller than any other value. */
        if (diff == 0) {
            return(0);
        } else if (dbe_trxid_getlong(trxid1) == 0) {
            return(-1);
        } else if (dbe_trxid_getlong(trxid2) == 0) {
            return(1);
        }
        return (diff < 0 ? -1 : 1);
}

#endif /* defined(DBE0TYPE_C) || defined(SS_USE_INLINE) */

#endif /* DBE0TYPE_H */
