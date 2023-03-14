/*************************************************************************\
**  source       * dbe9type.h
**  directory    * dbe
**  description  * Database engine internal types
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


#ifndef DBE9TYPE_H
#define DBE9TYPE_H

#include <ssc.h>
#include <sslimits.h>
#include <ssdebug.h>

#include <uti0vtpl.h>

#include <su0types.h>
#include <su0bflag.h>
#include <su0error.h>

/* The following define gives the max number of loops in the lower level
 * routines to be performed before returning incomplete return status
 * to the caller. For example, btree search routines try to find the
 * maxloop times before they return DBE_RC_NOTFOUND.
 */
#define DBE_MAXLOOP             5

#define DBE_BUSYPOLL_MAXLOOP    10
#define DBE_BUSYPOLL_MAXSLEEP   10

#define DBE_BNODE_SIZE_ATLEAST 60 /* for debugging purposes */
#define DBE_INDEX_HEADERSIZE    4L

#define DBE_HSBSYSCTR_SIZE      (14 * sizeof(ss_uint4_t))
#define DBE_HSBSYSCTR_OLDSIZE   (10 * sizeof(ss_uint4_t))

#ifndef DBE_BNODE_T_DEFINED
#define DBE_BNODE_T_DEFINED
typedef struct dbe_bnode_st         dbe_bnode_t;    /* dbe6bnod.h */
#endif

typedef ss_uint1_t dbe_blocktype_t;     /* block type field type */

/* number of blocks in list block type */
typedef ss_uint2_t dbe_bl_nblocks_t;

/* block size in db header */
typedef ss_uint4_t dbe_hdr_blocksize_t;

/* running check number in db header */
typedef ss_uint4_t dbe_hdr_chknum_t;

enum dbe_blocktype_values {
        DBE_BLOCK_FREE          =  0,   /* free block (for debug purposes) */
        DBE_BLOCK_FREELIST      =  1,   /* free list block */
        DBE_BLOCK_CHANGELIST    =  2,   /* change list from checkpoint */
        DBE_BLOCK_BADLIST       =  3,   /* bad block list */
        DBE_BLOCK_CPLIST        =  4,   /* live checkpoint list */
        DBE_BLOCK_TRXLIST       =  5,   /* validating transactions list */
        DBE_BLOCK_CPRECORD      =  6,   /* checkpoint record */
        DBE_BLOCK_SSRECORD      =  7,   /* snapshot record */
        DBE_BLOCK_DBHEADER      =  8,   /* database header record */
        DBE_BLOCK_BLOBLIST      =  9,   /* Blob allocation list block */
        DBE_BLOCK_BLOBDATA      = 10,   /* Blob data block */
        DBE_BLOCK_TREENODE      = 11,   /* B+-tree node */
        DBE_BLOCK_BLOB1ST       = 12,   /* Blob 1st block */
        DBE_BLOCK_STMTTRXLIST   = 13,   /* Statement Transaction list */
        DBE_BLOCK_SEQLIST       = 14,   /* Sequence list */
        DBE_BLOCK_RTRXLIST      = 15,   /* Replicated Trx list */
        DBE_BLOCK_FREECACHEPAGE = 16,   /* free cache page (for debug purposes) */
        DBE_BLOCK_BLOBG2PAGE    = 17,   /* new (G2) BLOB page */
        DBE_BLOCK_MMESTORAGE    = 18,   /* Main Memory Engine storage page */
        DBE_BLOCK_MMEPAGEDIR    = 19,   /* Main Memory Engine page directory */
        DBE_BLOCK_LASTNOTUSED           /* Last block number that is not used. */
};

typedef enum {
        DBE_KEYVLD_NONE,
        DBE_KEYVLD_UNIQUE,
        DBE_KEYVLD_PRIMARY,
        DBE_KEYVLD_FOREIGN,
        DBE_KEYVLD_READCOMMITTED_FORUPDATE      /* Check read done in SELECT ... 
                                                   FOR UPDATE and READ COMMITTED 
                                                   isolation level */
} dbe_keyvld_t;

typedef enum {
        DBE_TRXST_BEGIN,        /* begin state, uncertain */
        DBE_TRXST_VALIDATE,     /* validate state */
        DBE_TRXST_COMMIT,       /* ended and committed */
        DBE_TRXST_ABORT,        /* ended and aborted */
        DBE_TRXST_SAVED,        /* temporary state used during dbe_trxbuf_save */
        DBE_TRXST_TOBEABORTED   /* replicated trx: Must abort eventually */
} dbe_trxstate_t;

typedef enum {
        TRX_NOWRITES,       /* Initial mode, no writes done in trx. */
        TRX_READONLY,       /* Read-only transaction. */
        TRX_CHECKWRITES,    /* Check only write operations. */
        TRX_CHECKREADS,     /* Check read operations. */
        TRX_NOCHECK,        /* No checks are done, used e.g. during database
                               loading. Also logwrite counter is not
                               updated. */
        TRX_REPLICASLAVE,   /* Hot Standby slave server */
        TRX_NONREPEATABLEREAD  /* Temp mode for dbe_trx_mapisolation */
} trx_mode_t;

typedef enum {
        DBE_CHK_DB = SS_CHKBASE_DBE, /* 10000 */
        DBE_CHK_TRX,                 /* 10001 */
        DBE_CHK_USER,                /* 10002 */
        DBE_CHK_CURSOR,              /* 10003 */
        DBE_CHK_SEARCH,              /* 10004 */
        DBE_CHK_SEAVLD,              /* 10005 */
        DBE_CHK_INDEX,               /* 10006 */
        DBE_CHK_INDSEA,              /* 10007 */
        DBE_CHK_DATASEA,             /* 10008 */
        DBE_CHK_INDMERGE,            /* 10009 */
        DBE_CHK_INDVLD,              /* 10010 */
        DBE_CHK_TREESEA,             /* 10011 */
        DBE_CHK_KEYBUF,              /* 10012 */
        DBE_CHK_TRXBUF,              /* 10013 */
        DBE_CHK_CACHE,               /* 10014 */
        DBE_CHK_SRK,                 /* 10015 */
        DBE_CHK_TRDD,                /* 10016 */
        DBE_CHK_BTRSEA,              /* 10017 */
        DBE_CHK_BNODE,               /* 10018 */
        DBE_CHK_LOCKMGR,             /* 10019 */
        DBE_CHK_LOCKHEAD,            /* 10020 */
        DBE_CHK_LOCKREQ,             /* 10021 */
        DBE_CHK_LOCKTRAN,            /* 10022 */
        DBE_CHK_MMIND,               /* 10023 */
        DBE_CHK_MMSEA,               /* 10024 */
        DBE_CHK_MMNODE,              /* 10025 */
        DBE_CHK_MMNODEINFO,          /* 10026 */
        DBE_CHK_MMLOCKLST,           /* 10027 */
        DBE_CHK_MMLOCK,              /* 10028 */
        DBE_CHK_BKEYINFO,            /* 10029 */
        DBE_CHK_RTRX,                /* 10030 */
        DBE_CHK_RTRXBUF,             /* 10031 */
        DBE_CHK_REP,                 /* 10032 */
        DBE_CHK_SLOTQTICKET,         /* 10033 */
        DBE_CHK_BTREE,               /* 10034 */
        DBE_CHK_TRXINFO,             /* 10035 */
        DBE_CHK_TUPLESTATE,          /* 10036 */
        DBE_CHK_RBACKUPWITER,        /* 10037 */
        DBE_CHK_TRXID,               /* 10038 */
        DBE_CHK_TRXNUM,              /* 10039 */
        DBE_CHK_BLOBREF,             /* 10040 */
        DBE_CHK_BLOBREFBUF,          /* 10041 */
        DBE_CHK_SEQ,                 /* 10042 */
        DBE_CHK_SEQVAL,              /* 10043 */
        DBE_CHK_TRXSTMT,             /* 10044 */
        DBE_CHK_MMRECOVINFO,         /* 10045 */
        DBE_CHK_LOG,                 /* 10046 */
        DBE_CHK_WBLOBG2,             /* 10047 */
        DBE_CHK_RBLOBG2,             /* 10048 */
        DBE_CHK_LOGDATA,             /* 10049 */
        DBE_CHK_LOGPOS_RESERVED,     /* 10050 reserved for logpos */
        DBE_CHK_CATCHUP,             /* 10051 */
        DBE_CHK_FREELOCKREQ,         /* 10052 */
        DBE_CHK_FREELOCKHEAD,        /* 10053 */
        DBE_CHK_DBETRX,              /* 10054 */
#ifdef SS_MME
        DBE_CHK_SEARCH_HEADER,
        DBE_CHK_MME,
        DBE_CHK_MME_INDEX,
        DBE_CHK_MME_ILEVEL,
        DBE_CHK_MME_SEARCH,
        DBE_CHK_MME_ROW,
        DBE_CHK_MME_LOCK,
        DBE_CHK_MME_OPERATION,
        DBE_CHK_MME_LOCKLIST,
        DBE_CHK_MME_RBTNODE,
        DBE_CHK_MME_RBTSEARCHKEY,
        DBE_CHK_MME_RECOVERY,
        DBE_CHK_MME_ASYNC,
        DBE_CHK_MME_FREED,
#endif

        DBE_CHK_FREEDRBACKUPWRITER = SS_CHKBASE_DBE + SS_CHKBASE_FREED_INCR,
        DBE_CHK_FREEDBLOBREF,
        DBE_CHK_FREEDBLOBREFBUF,
        DBE_CHK_FREEDWBLOBG2,
        DBE_CHK_FREEDRBLOBG2,
        DBE_CHK_FREED,
        DBE_CHK_HSBSTATE,
        DBE_CHK_ABORTEDRELH,
        DBE_CHK_LOGPOS,
        DBE_CHK_SAVEDLOGPOS,
        DBE_CHK_SPM,
        DBE_CHK_MERGEPART,
        DBE_CHK_TCI,
        DBE_CHK_CRYPT,
        DBE_CHK_TRXBUFSLOT,
        DBE_CHK_FREED_TRXINFO
} dbe_chk_t;

#ifdef SS_MME
typedef enum {
        DBE_SEARCH_DBE = 0x715517,
        DBE_SEARCH_MME = 0xB00B5
} dbe_search_type_t;

/* Header for both DBE and MME search cursors.  THIS MUST BE THE FIRST MEMBER
   OF THE SEARCH CURSOR STRUCTURES. */
struct dbe_search_header_st {
        ss_debug(dbe_chk_t  sh_chk;)
        dbe_search_type_t   sh_type;
};

typedef struct dbe_search_header_st dbe_search_header_t;

typedef enum {
        DBE_CREATEINDEX_DBE = 0x06FB8945,
        DBE_CREATEINDEX_MME = 0x083A623E
} dbe_createindex_type_t;

/* Header for both DBE and MME createindex objects.
   THIS MUST BE THE FIRST MEMBER
   OF THE CREATEINDEX STRUCTURES. */
struct dbe_createindex_header_st {
        ss_debug(dbe_chk_t      ch_chk;)
        dbe_createindex_type_t  ch_type;
};

typedef struct dbe_createindex_header_st dbe_createindex_header_t;

typedef enum {
        DBE_DROPINDEX_DBE = 0x03008BC1,
        DBE_DROPINDEX_MME = 0x00E5FBDE
} dbe_dropindex_type_t;

/* Header for both DBE and MME dropindex objects.
   THIS MUST BE THE FIRST MEMBER
   OF THE DROPINDEX STRUCTURES. */
struct dbe_dropindex_header_st {
        ss_debug(dbe_chk_t      dh_chk;)
        dbe_dropindex_type_t    dh_type;
};

typedef struct dbe_dropindex_header_st dbe_dropindex_header_t;
#endif

/* Search range information (upper and lower limits).
 */
typedef struct {
        vtpl_t* sr_minvtpl;
        bool    sr_minvtpl_closed;
        vtpl_t* sr_maxvtpl;
        bool    sr_maxvtpl_closed;
} dbe_searchrange_t;

typedef struct dbe_trxbuf_st dbe_trxbuf_t;

typedef enum {
        DBE_INFO_DISKALLOCNOFAILURE = SU_BFLAG_BIT(0),
        DBE_INFO_OUTOFDISKSPACE     = SU_BFLAG_BIT(1),
        DBE_INFO_TREEPRELOCKED      = SU_BFLAG_BIT(2),
        DBE_INFO_OPENRANGEEND       = SU_BFLAG_BIT(3),
        DBE_INFO_MERGE              = SU_BFLAG_BIT(4),
        DBE_INFO_CHECKPOINT         = SU_BFLAG_BIT(5),
        DBE_INFO_LRU                = SU_BFLAG_BIT(6),
        DBE_INFO_IGNOREWRONGBNODE   = SU_BFLAG_BIT(7)
} dbe_info_flag_bits_t;

typedef su_bflag_t dbe_info_flags_t;

typedef struct {
        dbe_bnode_t*    btli_bnode;
        bool            btli_btreelocked;
} dbe_btree_lockinfo_t;

typedef struct dbe_info_st dbe_info_t;

struct dbe_info_st {
        dbe_info_flags_t      i_flags;
        su_ret_t              i_rc;
        dbe_btree_lockinfo_t* i_btreelockinfo;
};

#define dbe_info_init(info, flags)  { (info).i_flags = (flags); (info).i_btreelockinfo = NULL; }

extern int  dbe_search_noindexassert;   /* dbe6bsea.c */
extern bool dbe_debug;                  /* dbe8flst.c */

#endif /* DBE9TYPE_H */
