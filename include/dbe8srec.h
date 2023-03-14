/*************************************************************************\
**  source       * dbe8srec.h
**  directory    * dbe
**  description  * Start record that is shared between database header
**               * and checkpoint record
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


#ifndef DBE8SREC_H
#define DBE8SREC_H

#include <su0svfil.h>
#include <rs0tnum.h>
#include "dbe9type.h"
#include "dbe0type.h"

#define DBE_STARTREC_SIZE 256

/* 256 byte start record */
struct dbe_startrec_st {
        dbe_cpnum_t     sr_cpnum;           /* checkpoint/snapshot # */
        su_daddr_t      sr_bonsairoot;      /* used in index file only */
        su_daddr_t      sr_permroot;        /* used in index file only */
        su_daddr_t      sr_freelistaddr;    /* free list start disk address */
        su_daddr_t      sr_chlistaddr;      /* change list -"- */
        su_daddr_t      sr_cplistaddr;      /* checkpoint list -"- */
        su_daddr_t      sr_trxlistaddr;     /* transaction info list -"- */
        su_daddr_t      sr_filesize;        /* current file size in blocks */
        ss_uint4_t      sr_maxtrxnum;       /* current max trx number */
        ss_uint4_t      sr_maxtrxnum_res;   /* -"- reserve */
        ss_uint4_t      sr_committrxnum;    /* current commit trx number */
        ss_uint4_t      sr_committrxnum_res;
        ss_uint4_t      sr_mergetrxnum;     /* current merge trx number */
        ss_uint4_t      sr_mergetrxnum_res;
        ss_uint4_t      sr_trxid;           /* current trx id */
        ss_uint4_t      sr_trxid_res;
        rs_tuplenum_t   sr_tuplenum;        /* current tuple number */
        ss_uint4_t      sr_attrid;          /* current attribute id */
        ss_uint4_t      sr_keyid;           /* current key id */
        ss_uint4_t      sr_userid;          /* current key id */
        dbe_logfnum_t   sr_logfnum;         /* current log file number */
        dbe_blobg2id_t  sr_blobg2id;        /* current BLOB ID, note: high 32 bits are
                                             * taken from sr_reserved! low 32 bits are stored
                                             * at this position!
                                             */
        ss_uint4_t      sr_mergectr;        /* current merge counter */
        rs_tuplenum_t   sr_tupleversion;    /* global version number ctr */
        su_daddr_t      sr_stmttrxlistaddr; /* statement trx list disk addr */
        su_daddr_t      sr_sequencelistaddr;/* sequence list disk addr */
        su_daddr_t      sr_mmiroot;         /* used in main memory index only */
        su_daddr_t      sr_rtrxlistaddr;    /* Replication trx list daddr */
        ss_uint4_t      sr_syncmsgid;       /* sync message id */
        rs_tuplenum_t   sr_synctupleversion;/* sync version number ctr */
        /* Note: High 4 bytes of BLOB id are stored here! */
        su_daddr_t      sr_firstmmeaddrpage;/* Address of the first MME address page */
        ss_uint4_t      sr_storagetrxnum;   /* current storage trx number */
        ss_uint4_t      sr_storagetrxnum_res;
        char            sr_reserved[108];   /* for future expansion */
};

char* dbe_srec_puttodisk(
        dbe_startrec_t* sr,
        char* dbufpos);

char* dbe_srec_getfromdisk(
        dbe_startrec_t* sr,
        char* dbufpos);

#endif /* DBE8SREC_H */
