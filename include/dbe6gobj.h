/*************************************************************************\
**  source       * dbe6gobj.h
**  directory    * dbe
**  description  * Global object structure.
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


#ifndef DBE6GOBJ_H
#define DBE6GOBJ_H

#include <rs0sysi.h>

#include "dbe9type.h"
#include "dbe6finf.h"
#include "dbe6bkey.h"
#include "dbe0type.h"

typedef enum {
        DBE_GOBJ_MERGE_NONE,
        DBE_GOBJ_MERGE_WAITING,
        DBE_GOBJ_MERGE_STARTED
} dbe_gobj_mergest_t;

typedef struct {
        ulong ts_commitcnt;
        ulong ts_abortcnt;
        ulong ts_rollbackcnt;
        ulong ts_readonlycnt;
        ulong ts_stmtcnt;
        ulong ts_quickmergelimitcnt;
} db_trxstat_t;

/* Type dbe_gobj_t defined dbe0type.h */

struct  dbe_gobj_st {
        dbe_filedes_t*      go_idxfd;   /* Index file. */
        dbe_counter_t*      go_ctr;     /* Counter object. */
        dbe_trxbuf_t*       go_trxbuf;  /* Transaction buffer. */
        dbe_blobmgr_t*      go_blobmgr; /* Blob manager. */
        dbe_cfg_t*          go_cfg;     /* Configuration object. */
#ifdef IO_MANAGER
        dbe_iomgr_t*        go_iomgr;   /* I/O manager object */
#endif
        dbe_file_t*         go_dbfile;  /* database file object */
        bool                go_earlyvld;

#ifndef SS_NOSEQUENCE
        dbe_seq_t*          go_seq;
#endif /* SS_NOSEQUENCE */

        dbe_gtrs_t*         go_gtrs;
        dbe_bkeyinfo_t*     go_bkeyinfo;
        dbe_db_t*           go_db;
        rs_sysinfo_t*       go_syscd; /* no concurrency protection for this! */
#ifdef DBE_REPLICATION
        dbe_rtrxbuf_t*      go_rtrxbuf;
#endif
        dbe_gobj_mergest_t  go_mergest;             /* Merge state. */
        dbe_trxnum_t        go_mergetrxnum;         /* Current merge read level. */
        long                go_splitcount;          /* Number of node splits
                                                       in Bonsai-tree. */
        long                go_splitavoidcount;     /* Number of node splits
                                                       avoided because of
                                                       split merge.*/
        long                go_mergerounds;
        long                go_quickmergerounds;
        long                go_nmergewrites;
        long                go_nindexwrites;
        long                go_ntotindexwrites;
        long                go_nlogwrites;
        long                go_ntotlogwrites;
#ifdef SS_MME
        dbe_mme_t*          go_mme;
#endif
        SsSemT*             go_sem;
        db_trxstat_t        go_trxstat;
};

dbe_gobj_t* dbe_gobj_init(
        void);

void dbe_gobj_done(
        dbe_gobj_t* go);

void dbe_gobj_mergestart(
        dbe_gobj_t* go,
        dbe_trxnum_t mergetrxnum,
        bool full_merge);

void dbe_gobj_mergestop(
        dbe_gobj_t* go);

void dbe_gobj_mergeupdate(
        dbe_gobj_t* go,
        long nkeyremoved,
        long nmergeremoved);

void dbe_gobj_quickmergeupdate(
        dbe_gobj_t* go,
        long nstmtremoved);

void dbe_gobj_addmergewrites(
        dbe_gobj_t* go,
        long nmergewrites);

void dbe_gobj_addindexwrites(
        dbe_gobj_t* go,
        rs_sysi_t* cd,
        long nindexwrites);

void dbe_gobj_addlogwrites(
        dbe_gobj_t* go,
        long nlogwrites);

void dbe_gobj_addtrxstat(
        dbe_gobj_t* go,
        rs_sysi_t* cd,
        dbe_db_trxtype_t trxtype,
        bool count_it,
        bool read_only,
        long stmtcnt,
        long nindexwrites,
        long nlogwrites);

#endif /* DBE6GOBJ_H */
