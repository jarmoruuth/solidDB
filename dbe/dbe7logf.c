/************************************************************************\
**  source       * dbe7logf.c
**  directory    * dbe
**  description  * Logging subsystem
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

The logging subsystem has been designed to be as flexible as
possible. It supports 4 (or 5, actually) different write synchronization
modes. They are:

0. The ping-pong write which uses two last disk blocks as the newest and
second-newest version of the same logical incomplete disk block. The
ping-pong toggles the newest version between the two blocks till the
block becomes full.

1. The write-once algorithm:
an incomplete block is always
padded with NOPs. This is practically speaking useless because
it loses lots of disk space when transactions are small and/or
the load is low

2. Overwriting algorithm rewrites inclomplete block at each
commit until it becomes full. It is usable when a data loss
from the last log file disk block is affordable or else when the
disk subsystem guarantees atomic writes for the blocksize in use.
In the latter case the method is as safe as the ping-pong or write once.

3. Lazy write mode is intended for applications that are not critical in
the sense that loosing a few transactions at system crashes is affordable.
On the contrary it offers the best overall performance, because transaction
do not need to wait for disk writes at each commit. This is especially
noteworthy when the applications do their operations in very short
transactions (e.g. autocommit mode).

Choosing which method to use may sometimes be diffcult. In most
cases the ping-pong is the method of choice. If relaxed durability
is tolerable the lazy write mode is fastest. Overwriting algorithm
is both pretty fast and also relatively safe especially when hardware
power failure is ruled out with UPS or disk subsystem has guaranteed atomic
writes for the log blocksize.

Format of records
-----------------

all recods contain at least 1 byte, which indicates the record type.
The records are always byte aligned and integers are in LSB first byte
order. The record formats are presented here as pseudo C language structures
for easy comprehension

struct DBE_LOGREC_NOP {
        ss_byte_t rectype;
};

struct DBE_LOGREC_HEADER {
        ss_byte_t    rectype;
        ss_uint4_t  logfnum;
        ss_uint4_t  cpnum;
        ss_uint4_t  blocksize;
        ss_uint4_t  dbcreatime;
};

struct DBE_LOGREC_INSTUPLE((WITH|NO)BLOBS)? {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  relid;
        vtpl_t  vtuple;
};

#ifdef SS_MME
struct DBE_LOGREC_MME_INSTUPLE((WITH|NO)BLOBS)? {
        ss_byte_t   rectype;
        ss_int4_t   trxid;
        ss_uint4_t  relid;
        mme_rval_t  rval;
};

struct DBE_LOGREC_MME_DELTUPLE {
        ss_byte_t   rectype;
        ss_int4_t   trxid;
        ss_uint4_t  relid;
        mme_rval_t  rval;
        mme_rval_t  rval;
};
#endif /* SS_MME */

struct DBE_LOGREC_DELTUPLE {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  relid;
        vtpl_t  tupleref;   /* vtuple containing a trx id & tupleref vtuple */
};

struct DBE_LOGREC_BLOBSTART_OLD {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        dbe_vablobref_t blobref;    /* in disk format */

};

struct DBE_LOGREC_BLOBALLOCLIST_OLD {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  storage_addresses[datasize/sizeof(ss_uint4_t)];
};

struct DBE_LOGREC_BLOBALLOCLIST_CONT_OLD {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  storage_addresses[datasize/sizeof(ss_uint4_t)];
};

struct DBE_LOGREC_BLOBDATA_OLD {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_byte_t    data[datasize];
};

struct DBE_LOGREC_BLOBDATA_CONT_OLD {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_byte_t    data[datasize];
};

struct DBE_LOGREC_ABORTTRX_OLD {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
};

struct DBE_LOGREC_ABORTTRX_INFO {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_byte_t  info;
};

struct DBE_LOGREC_COMMITTRX_OLD {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
};

struct DBE_LOGREC_COMMITTRX_HSB_OLD {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
};

struct DBE_LOGREC_COMMITTRX_INFO {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_byte_t  info;
};

struct DBE_LOGREC_PREPARETRX {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
};

struct DBE_LOGREC_CHECKPOINT_OLD {
        ss_byte_t    rectype;
        ss_uint4_t  cpnum;
};

struct DBE_LOGREC_SNAPSHOT_OLD {
        ss_byte_t    rectype;
        ss_uint4_t  cpnum;
};

struct DBE_LOGREC_CHECKPOINT_NEW {
        ss_byte_t    rectype;
        ss_uint4_t  cpnum;
        ss_uint4_t  timestamp;
};

struct DBE_LOGREC_SNAPSHOT_NEW {
        ss_byte_t    rectype;
        ss_uint4_t  cpnum;
        ss_uint4_t  timestamp;
};

struct DBE_LOGREC_DELSNAPSHOT {
        ss_byte_t    rectype;
        ss_uint4_t  cpnum;
};

struct DBE_LOGREC_CREATETABLE {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        UWORD   nkeys;
        UWORD   nattrs;
        char    relname[datasize -
                        sizeof(relid) - sizeof(nkeys) - sizeof(nattrs)];
};
struct DBE_LOGREC_CREATETABLE_NEW {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        UWORD   nkeys;
        UWORD   nattrs;
        char    relname[datasize -
                        sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                        strlen(relschema) - 1];
        char    relschema[datasize -
                          sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                          strlen(relname) - 1];
};
struct DBE_LOGREC_CREATETABLE_FULLYQUALIFIED {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        UWORD   nkeys;
        UWORD   nattrs;
        char    relname[datasize -
                        sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                        strlen(relschema) - 1 -
                        strlen(relcatalog) - 1];
        char    relschema[datasize -
                          sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                          strlen(relname) - 1 -
                          strlen(relcatalog) - 1];
        char    relcatalog[datasize -
                          sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                          strlen(relname) - 1 -
                          strlen(relschema) - 1];
};

struct DBE_LOGREC_CREATEINDEX {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        ss_uint4_t  keyid;
        char    relname[datasize - sizeof(relid) - sizeof(keyid)];
};

struct DBE_LOGREC_DROPTABLE {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        char    relname[datasize - sizeof(relid)];
}

struct DBE_LOGREC_TRUNCATETABLE {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        char    relname[datasize - sizeof(relid)];
}

struct DBE_LOGREC_TRUNCATECARDIN {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        char    relname[datasize - sizeof(relid)];
}

struct DBE_LOGREC_DROPINDEX {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        ss_uint4_t  keyid;
        char    relname[datasize - sizeof(relid) - sizeof(keyid)];
};

struct DBE_LOGREC_CREATEVIEW {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        UWORD   nkeys;  /* always 0 */
        UWORD   nattrs; /* always 0 */
        char    relname[datasize -
                        sizeof(relid) - sizeof(nkeys) - sizeof(nattrs)];
};

struct DBE_LOGREC_CREATEVIEW_NEW {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        UWORD   nkeys;  /* always 0 */
        UWORD   nattrs; /* always 0 */
        char    relname[datasize -
                        sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                        strlen(relschema) - 1];
        char    relschema[datasize -
                          sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                          strlen(relname) - 1];
};
struct DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        UWORD   nkeys;  /* always 0 */
        UWORD   nattrs; /* always 0 */
        char    relname[datasize -
                        sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                        strlen(relschema) - 1 -
                        strlen(relcatalog) - 1];
        char    relschema[datasize -
                          sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                          strlen(relname) - 1 -
                          strlen(relcatalog) - 1];
        char    relcatalog[datasize -
                          sizeof(relid) - sizeof(nkeys) - sizeof(nattrs) -
                          strlen(relname) - 1 -
                          strlen(relschema) - 1];
};

struct DBE_LOGREC_DROPVIEW {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  relid;
        char    relname[datasize - sizeof(relid)];
};

struct DBE_LOGREC_CREATEUSER {
        ss_byte_t    rectype;
};


struct DBE_LOGREC_INCID {
        ss_byte_t    rectype;
        ss_byte_t    idctrid;
};

struct DBE_LOGREC_CREATECTR {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  ctrid;
        char    relname[datasize - sizeof(ctrid)];
};
struct DBE_LOGREC_CREATESEQ {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  seqid;
        char    relname[datasize - sizeof(seqid)];
};

struct DBE_LOGREC_DROPCTR {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  ctrid;
        char    relname[datasize - sizeof(ctrid)];
};

struct DBE_LOGREC_DROPSEQ {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  datasize;
        ss_uint4_t  seqid;
        char    relname[datasize - sizeof(seqid)];
};

struct DBE_LOGREC_INCCTR {
        ss_byte_t    rectype;
        ss_uint4_t  ctrid;
};

struct DBE_LOGREC_SETCTR {
        ss_byte_t    rectype;
        ss_uint4_t  ctrid;
        ss_int8_t  value;
};

struct DBE_LOGREC_SETSEQ {
        ss_byte_t    rectype;
        ss_int4_t  trxid;
        ss_uint4_t  seqid;
        ss_int8_t  value;
}

struct DBE_LOGREC_BLOBG2DATA {
        ss_byte_t  rectype;
        ss_uint4_t datasize;
        ss_int8_t  blobid;
        ss_int8_t  offset;
        ss_byte_t  data[datasize - sizeof(blobid) - sizeof(offset)]
}

struct DBE_LOGREC_BLOBG2DROPMEMORYREF{
        ss_byte_t  rectype;
        ss_int8_t  blobid;
}

struct DBE_LOGREC_BLOBG2DATACOMPLETE{
        ss_byte_t  rectype;
        ss_int8_t  blobid;
}

struct DBE_LOGREC_HSBG2_NEW_PRIMARY {
        ss_byte_t  rectype;
        ss_uint4_t originator_nodeid;
        ss_uint4_t primary_nodeid;
}

struct DBE_LOGREC_HSBG2_ABORTALL {
        ss_byte_t    rectype;
};

struct DBE_LOGREC_HSBG2_NEWSTATE {
        ss_byte_t    rectype;
        ss_byte_t    state;
};


Limitations:
-----------

Lazy writing is done for blocks that do not contain commit marks only
if the operating system provides automatic lazy writing.
A future project would be implementing lazy write queue for log
file disk blocks (this, however, requires a multi-threaded OS
or a separate process).

Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------

The log file write operations can be done from several threads, because
all critical sections are automatically protected with a mutex semaphore.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define DBE_LAZYLOG_OPT

#include <ssc.h>
#include <sssem.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssstring.h>
#include <sssprint.h>
#include <sslimits.h>
#include <ssfile.h>
#include <ssltoa.h>
#include <sspmon.h>
#if defined(DBE_LAZYLOG_OPT)
#include <ssthread.h>
#endif

#include <su0icvt.h>
#include <su0vfil.h>
#include <su0svfil.h>
#include <su0cfgst.h>
#include <su0gate.h>
#include <su0mesl.h>
#include <su0prof.h>

#include <ui0msg.h>

#include "dbe9type.h"
#include "dbe0type.h"
#include "dbe6bnod.h"
#include "dbe6log.h"
#include "dbe7logf.h"
#include "dbe0lb.h"
#include "dbe7rfl.h"
#include "dbe0crypt.h"

#ifdef SS_MME
#ifndef SS_MYSQL
#ifdef SS_MMEG2
#include <../mmeg2/mme0rval.h>
#else
#include <../mme/mme0rval.h>
#endif
#endif
#endif

#ifdef SS_HSBG2

#include "dbe0hsbbuf.h"
#include "dbe7lbm.h"

#endif /* SS_HSBG2 */

#ifndef SS_NOLOGGING

#ifdef SS_MT
#define DBE_GROUPCOMMIT_QUEUE
#endif /* SS_MT */

#ifdef SS_SEMSTK_DBG
extern bool ss_debug_disablesemstk;
#endif /* SS_SEMSTK_DBG */

/* Write modes */
typedef enum {
        DBE_LOGWR_PINGPONG = 0, /* Use ping-pong write */
        DBE_LOGWR_ONCE, /* Never write same block twice. Depending on
                         * the group commit delay the commit either
                         * waits for the block to become full or
                         * it null-pads and writes it. Note: group commit
                         * is only possible when the DBE is multithreaded
                         */

        DBE_LOGWR_OVER, /* Overwrite latest block at each commit */
        DBE_LOGWR_LAZY  /* Only full blocks are written! Therefore the
                         * latest transactions may be lost at system crash
                         */
} dbe_logwrmode_t;

typedef enum {
    LOGFILE_WRITE_BUFFERED,
    LOGFILE_WRITE_NOBUFFER,
    LOGFILE_WRITE_FLUSH
} logfile_write_t;

#ifdef DBE_GROUPCOMMIT_QUEUE

#define MAX_HEADER_BUF  12

typedef struct {
        rs_sysi_t*          wq_cd;
        dbe_logrectype_t    wq_logrectype;
        dbe_trxid_t         wq_trxid;
        bool                wq_islpid;
        dbe_hsb_lpid_t      wq_lpid;
        void*               wq_logdata;
        void*               wq_dyndata;
        char                wq_databuf[800];    /* Struct should fit into 1024 byte block. */
        size_t              wq_logdatalen;
        ss_uint4_t          wq_relid_or_originaldatalen;
        char                wq_header_buf[MAX_HEADER_BUF];
        size_t              wq_header_buf_len;
        size_t*             wq_p_logdatalenwritten;
        bool*               wq_p_splitlog;
        bool                wq_flushtodisk;
        bool                wq_split_hsbqueue_force;
        dbe_ret_t*          wq_p_rc;
        ss_debug(bool       wq_iswaiting;)
        void*               wq_next;
} logf_writequeue_t;

typedef struct {
        logf_writequeue_t*  wqb_first;
        logf_writequeue_t*  wqb_last;
        size_t              wqb_logdatalen;
        int                 wqb_len;
} logf_writequeuebuf_t;

typedef struct {
        dbe_logfile_t*      wqi_logfile;
        SsFlatMutexT        wqi_writequeue_mutex;
        SsFlatMutexT        wqi_freequeue_mutex;
        SsMesT*             wqi_writequeue_mes;
        logf_writequeue_t*  wqi_writequeue_first;
        logf_writequeue_t*  wqi_writequeue_last;
        logf_writequeue_t*  wqi_freequeue;
        su_meslist_t*       wqi_writequeue_meslist;
        su_meslist_t        wqi_writequeue_meslist_buf;
        su_meswaitlist_t*   wqi_writequeue_meswaitlist;
        su_meswaitlist_t*   wqi_writequeue_flushmeswaitlist;
        bool                wqi_writeactive;
        bool                wqi_done;
        int                 wqi_nrecords;
        long                wqi_nbytes;
        long                wqi_npendingbytes;
} logf_writequeinfo_t;

#endif /* DBE_GROUPCOMMIT_QUEUE */

/* Log file object */
struct dbe_logfile_st {
        dbe_counter_t*  lf_counter;         /* DBE counter object */
        SsBFileT*       lf_bfile;
        size_t          lf_bufsize;
        size_t          lf_bufdatasize;
        dbe_logpos_t    lf_lp;              /* write position */
#ifdef IO_OPT
        dbe_alogbuf_t*   lf_abuffer; /* Aligned logbuffer object */
#else
        dbe_logbuf_t*   lf_buffer;          /* file write buffer */
#endif

        dbe_logwrmode_t lf_wrmode;          /* write mode */
        su_daddr_t      lf_pingpong_incr;   /* 0 or 1 */
        ss_byte_t       lf_lastblock;

        char*           lf_logdir;
        char*           lf_filename;
        char*           lf_nametemplate;
        char            lf_digittemplate;
        ulong           lf_minsplitsize;
        bool            lf_flushflag;
        ss_uint4_t      lf_dbcreatime;      /* database creation timestamp */
        void          (*lf_errorfunc)(void*);/* error callback function */
        void*           lf_errorctx;        /* parameter for lf_errorfunc */
        bool            lf_errorflag;       /* error flag */
        ulong           lf_filewritecnt;
        int             lf_openflags;
#ifdef DBE_GROUPCOMMIT
        SsSemT*          lf_writemutex;  /* write mutex */
#else /* DBE_GROUPCOMMIT */
#       define          GROUPCOMMITDELAY_NEEDED
        SsMesT*         lf_wait;            /* wait semaphore */
        uint            lf_nwait;           /* # of waiting threads */
        SsSemT*         lf_mutex;
        bool            lf_lazyflushflag;
#endif /* DBE_GROUPCOMMIT */
        bool            lf_flushed;
#if defined(DBE_LAZYLOG_OPT)
#       ifndef GROUPCOMMITDELAY_NEEDED
#           define          GROUPCOMMITDELAY_NEEDED
#       endif /* !GROUPCOMMITDELAY_NEEDED */
        bool            lf_flushthreadp;
#endif /* DBE_LAZYLOG_OPT */
#ifdef GROUPCOMMITDELAY_NEEDED
        ulong           lf_groupcommitdelay; /* # of ms to wait at commit */
#endif /* GROUPCOMMITDELAY_NEEDED */

        dbe_logfnum_t   lf_logfnum;
        su_cipher_t*    lf_cipher;
        dbe_encrypt_t   lf_encrypt;
        dbe_decrypt_t   lf_decrypt;

#ifdef IO_OPT
        /* Replace every reference to writebuffer and writebuffer_address
         * with dbe_aligned_buf_t */
        dbe_aligned_buf_t*  lf_writeabuffer;
#endif
        ss_byte_t*      lf_writebuffer;
        uint            lf_writebuffer_maxbytes;
        uint            lf_writebuffer_nbytes;
        su_daddr_t      lf_writebuffer_address;

#ifdef DBE_GROUPCOMMIT_QUEUE
        logf_writequeinfo_t lf_wqinfo;
        bool                lf_groupcommitqueue;
        bool                lf_groupcommit_flush_queue;

        ulong               lf_lazyflush_delay; /* # of ms to wait at commit */
        bool                lf_lazyflush_threadp;
        bool                lf_lazyflush_request;
        bool                lf_lazyflush_do;
        SsMesT*             lf_lazyflush_wait;   /* wait semaphore */
        bool                lf_lazyflush_extendfile;
        uint                lf_lazyflush_extendincrement;

#endif /* DBE_GROUPCOMMIT_QUEUE */

#ifdef SS_HSBG2
        dbe_hsbbuf_t*       lf_hsbbuf;                  /* Buffer for HSB:has link count and mutex */
        int                 lf_dbg_nbuffers_involved;   /* for checking & debugging */
        dbe_lbm_t*          lf_lbm;                     /* Buffer manager to allocate/release hsbbufs */
        dbe_hsbg2_t*        lf_hsbsvc;                  /* HSB services */
        bool                lf_transform;
        dbe_log_instancetype_t lf_instancetype;
        void*               lf_instance_ctx;
        dbe_logdata_t*      lf_hsbld;                   /* 'current' logrec to record */

        dbe_catchup_logpos_t lf_catchup_logpos;         /* current log position */

        bool                lf_nondurable_commit;
        long                lf_maxwritequeuerecords;
        long                lf_maxwritequeuebytes;
        long                lf_writequeueflushlimit;
        bool                lf_delaymeswait;
        dbe_logfile_idlehsbdurable_t lf_idlehsbdurable; /* is idle time hsb durable mark write enabled */
#endif /* SS_HSBG2 */

};

#ifdef DBE_GROUPCOMMIT

#define logfile_enter_mutex(logfile) SsSemEnter((logfile)->lf_writemutex)
#define logfile_exit_mutex(logfile) SsSemExit((logfile)->lf_writemutex)
#define logfile_exit_mutex2(logfile) logfile_exit_mutex(logfile)
#define logfile_mutex_isentered(logfile) SsSemThreadIsEntered((logfile)->lf_writemutex)

#else /* DBE_GROUPCOMMIT */

#define logfile_enter_mutex(logfile) \
        SsSemEnter((logfile)->lf_mutex)

#define logfile_exit_mutex(logfile) \
        SsSemExit((logfile)->lf_mutex)

#define logfile_exit_mutex2(logfile) \
        logfile_exit_mutex(logfile)

#define logfile_mutex_isentered(logfile) \
        SsSemThreadIsEntered((logfile)->lf_mutex)

#endif /* DBE_GROUPCOMMIT */

static dbe_ret_t logfile_putheader_nomutex(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen,
        size_t* p_logdatalenwritten);

static dbe_ret_t logfile_flush(
        dbe_logfile_t* logfile);

#define LOGFILE_CHECKSHUTDOWN(logfile) \
{\
        if ((logfile)->lf_errorflag) {\
            return (SU_ERR_FILE_WRITE_FAILURE);\
        }\
}

/*##**********************************************************************\
 *
 *              logfile_toggle_pingpong
 *
 * Macro for toggling ping-pong increment (which always has value 0 or 1)
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define logfile_toggle_pingpong(logfile) \
do {\
        (logfile)->lf_pingpong_incr = ~(logfile)->lf_pingpong_incr & 1;\
} while (0)


/*#***********************************************************************\
 *
 *              logfile_write
 *
 * Writes data to specified block address of log file
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      loc - in
 *              block address where to write
 *
 *      data - in, use
 *              pointer to write data buffer
 *
 *      size - in
 *              size of data to be written
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      DBE_ERR_LOGWRITEFAILURE when failed
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t logfile_write(
        dbe_logfile_t* logfile,
        logfile_write_t type,
        su_daddr_t loc,
        void* data,
        size_t size)
{
        dbe_ret_t rc;
        bool succp;
        size_t size_pages;
        su_profile_timer;
        char* write_data;

        ss_pprintf_1(("logfile_write:loc %ld, size %d, bufpos %d,"
                " dataptr %x\n",
                (long)loc,
                size,
                logfile->lf_lp.lp_bufpos,
                data));

#ifdef SS_HSBG2
        if(logfile->lf_transform) {
            return (DBE_RC_SUCC);
        }
#endif /* SS_HSBG2 */

        ss_dassert(size != 0 && size % logfile->lf_bufsize == 0);
        ss_dassert(logfile_mutex_isentered(logfile));

        size_pages = size / logfile->lf_bufsize;
        ss_dassert(loc != 0L || ((uchar*)data)[2] == DBE_LOGREC_HEADER);
        rc = DBE_RC_SUCC;
        logfile->lf_flushed = FALSE;
        su_profile_start;

        if (logfile->lf_cipher != NULL) {
#ifdef IO_OPT
            dbe_aligned_buf_t* ab;
#endif
            su_profile_timer;
            su_profile_start;

#ifdef IO_OPT
            ab = dbe_ab_init(size, SS_DIRECTIO_ALIGNMENT);
            write_data = ab->ab_buf;
#else
            write_data = SsMemAlloc(size);
#endif
            memcpy(write_data, data, size);

            logfile->lf_encrypt(logfile->lf_cipher, loc, write_data,
                                size_pages, logfile->lf_bufsize);
            su_profile_stop("logfile_write: encryption");
        } else {
            write_data = data;
            if (type == LOGFILE_WRITE_BUFFERED) {
                size_t startpos;
                size_t newsize;
                ss_assert(size <= logfile->lf_writebuffer_maxbytes);
                if (logfile->lf_writebuffer_nbytes == 0) {
                    startpos = 0;
                    newsize = size;
                } else {
                    startpos = (loc - logfile->lf_writebuffer_address)
                            * logfile->lf_bufsize;
                    newsize = startpos + size;
                }
                ss_dassert(newsize > 0);
                ss_dassert(newsize % logfile->lf_bufsize == 0);
                ss_dprintf_2(("logfile_write:lf_writebuffer_nbytes %ld, "
                        "startpos %ld, newsize %ld\n",
                        (long)logfile->lf_writebuffer_nbytes,
                        (long)startpos,
                        (long)newsize));
                if (newsize > logfile->lf_writebuffer_maxbytes
                    && logfile->lf_writebuffer_nbytes > 0)
                {
                    ss_pprintf_2(("logfile_write:LOGFILE_WRITE_BUFFERED, "
                            "call logfile_flush\n"));
                    rc = logfile_write(
                            logfile,
                            LOGFILE_WRITE_FLUSH,
                            logfile->lf_writebuffer_address,
                            logfile->lf_writebuffer,
                            logfile->lf_writebuffer_nbytes);
                    if (rc != SU_SUCCESS) {
                        return(rc);
                    }
                    logfile->lf_writebuffer_nbytes = 0;
                    logfile->lf_writebuffer_address = -1;
                    startpos = 0;
                    newsize = size;
                }
                ss_dprintf_2(("logfile_write:copy to buffer, "
                        "lf_writebuffer_nbytes %ld\n",
                        (long)logfile->lf_writebuffer_nbytes));
                memcpy(logfile->lf_writebuffer + startpos, data, size);
                if (logfile->lf_writebuffer_nbytes == 0) {
                    ss_dprintf_2(("logfile_write:set "
                            "lf_writebuffer_address %ld\n",
                            (long)logfile->lf_writebuffer_address));
                    ss_dassert(logfile->lf_writebuffer_address == -1);
                    logfile->lf_writebuffer_address = loc;
                }
                logfile->lf_writebuffer_nbytes = newsize;
                logfile->lf_flushed = FALSE;
                return (DBE_RC_SUCC);
            } else if (type == LOGFILE_WRITE_NOBUFFER
                       && logfile->lf_writebuffer_nbytes > 0)
            {
                ss_dprintf_2(("logfile_write:LOGFILE_WRITE_NOBUFFER, "
                        "lf_writebuffer_nbytes %ld\n",
                        (long)logfile->lf_writebuffer_nbytes));
                rc = logfile_write(
                        logfile,
                        LOGFILE_WRITE_FLUSH,
                        logfile->lf_writebuffer_address,
                        logfile->lf_writebuffer,
                        logfile->lf_writebuffer_nbytes);
                if (rc != SU_SUCCESS) {
                    return(rc);
                }
                logfile->lf_writebuffer_nbytes = 0;
                logfile->lf_writebuffer_address = -1;
            }
        }

        succp = SsBWritePages(
                    logfile->lf_bfile,
                    loc,
                    logfile->lf_bufsize,
                    write_data,
                    size_pages);
        if (write_data != data) {
            SsMemFree(write_data);
        }
        su_profile_stop("logfile_write:SsBWritePages");
        logfile->lf_filewritecnt++;

        FAKE_CODE_BLOCK(FAKE_DBE_LOGFILE_WRITE_FAIL, {
            ss_dprintf_1(("FAKE_DBE_LOGFILE_WRITE_FAIL\n"));
            succp = FALSE;
        });

        if (!succp) {
            logfile->lf_errorflag = TRUE;
            if (logfile->lf_errorfunc != (void(*)(void*))0L) {
#ifdef SS_SEMSTK_DBG
                ss_debug_disablesemstk = TRUE;
#endif /* SS_SEMSTK_DBG */
                (*logfile->lf_errorfunc)(logfile->lf_errorctx);
#ifdef SS_SEMSTK_DBG
                ss_debug_disablesemstk = FALSE;
#endif /* SS_SEMSTK_DBG */
            } else {
                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_LOGFILEWRITEFAILURE_SU,
                    logfile->lf_filename,
                    (ulong)loc * logfile->lf_bufsize);
            }
            rc = DBE_ERR_LOGWRITEFAILURE;
        }
        if (logfile->lf_lazyflush_extendincrement > 0 && succp) {
            logfile->lf_lazyflush_extendfile = TRUE;
            SsMesSend(logfile->lf_lazyflush_wait);
        }
        return (rc);
}

/*#***********************************************************************\
 *
 *              logfile_read
 *
 * Reads from logfile
 *
 * Parameters :
 *
 *      logfile - in, use
 *              pointer to logfile
 *
 *      loc - in
 *              block address to read
 *
 *      data - out
 *              pointer to user's data buffer
 *
 *      size - in
 *              number of bytes to read to data buffer
 *
 *      p_bytesread - out
 *              pointer to variable where the # of bytes actually read
 *          will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      SU_ERR_FILE_READ_EOF when EOF reached during read
 *      SU_ERR_FILE_READ_FAILURE when other failures
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t logfile_read(
        dbe_logfile_t* logfile,
        su_daddr_t loc,
        void* data,
        size_t size,
        size_t* p_bytesread)
{
        dbe_ret_t rc;
        size_t rc2;
        size_t npages;

        rc = DBE_RC_SUCC;
        ss_rc_dassert((size % logfile->lf_bufsize) == 0, size);
        ss_dassert(logfile_mutex_isentered(logfile));

        npages = size / logfile->lf_bufsize;
        rc2 = SsBReadPages(
                    logfile->lf_bfile,
                    loc,
                    logfile->lf_bufsize,
                    data,
                    npages);

        if (rc2 != npages) {
            if (rc2 == (size_t)-1) {
                rc = SU_ERR_FILE_READ_FAILURE;
            } else {
                rc = SU_ERR_FILE_READ_EOF;
                *p_bytesread = rc2 * logfile->lf_bufsize;
            }
        } else {
            *p_bytesread = rc2 * logfile->lf_bufsize;
            if (logfile->lf_cipher != NULL) {
                su_profile_timer;
                su_profile_start;
                logfile->lf_decrypt(logfile->lf_cipher, loc, data,
                                    rc2, logfile->lf_bufsize);
                su_profile_stop("logfile_read: decryption");
            }
        }
        return (rc);
}

/*#***********************************************************************\
 *
 *              logfile_getsize
 *
 * Gets size of log file in blocks
 *
 * Parameters :
 *
 *      logfile - in, use
 *              pointer to logfile object
 *
 * Return value :
 *      size of file in blocks
 *
 * Limitations  :
 *
 * Globals used :
 */
static su_daddr_t logfile_getsize(
        dbe_logfile_t* logfile)
{
        su_daddr_t s;

        ss_dassert(logfile_mutex_isentered(logfile));

        s = SsBSizePages(logfile->lf_bfile, logfile->lf_bufsize);
        return (s);
}

ulong dbe_logfile_getsize2(
        dbe_logfile_t* logfile)
{
        ulong size_kb;
        ss_int8_t size_i8;
        ss_int8_t tmp_i8;
        SsInt8SetUint4(&size_i8, logfile->lf_lp.lp_daddr);
        SsInt8SetUint4(&tmp_i8, (ss_uint4_t)logfile->lf_bufsize);
        SsInt8MultiplyByInt8(&size_i8, size_i8, tmp_i8);
        SsInt8SetUint4(&tmp_i8, 1024U);
        SsInt8DivideByInt8(&size_i8, size_i8, tmp_i8);

#ifdef SS_NATIVE_UINT8_T
        if (sizeof(size_kb) == sizeof(SS_NATIVE_UINT8_T)) {
            size_kb = (ulong)SsInt8GetNativeUint8(size_i8);
        } else
#endif /* SS_NATIVE_UINT8_T */
        {
            size_kb = SsInt8GetLeastSignificantUint4(size_i8);
        }
        return (size_kb);
}

su_daddr_t logfile_getbufsize(
        dbe_logfile_t* logfile)
{
    return(logfile->lf_bufsize);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_genname
 *
 * Generates log file name by template and log file number
 *
 * Parameters :
 *
 *      nametemplate - in, use
 *              name template eg. "/database/log/db######.log"
 *
 *      logfnum - in
 *              log file number
 *
 *      digittemplate - in
 *              tells which character in nametemplate serves as placeholder
 *          for digit
 *
 * Return value - give :
 *      pointer to generated file name or
 *      NULL when the nametemplate contained too few digit positions
 *      or was otherwise illegal.
 *
 * Limitations  :
 *
 * Globals used :
 */
char* dbe_logfile_genname(
        char* logdir,
        char* nametemplate,
        dbe_logfnum_t logfnum,
        char digittemplate)
{
        uint ndigits;
        char* p;
        char* firstdigitpos = NULL;
        char* name;
        char numbuf[21];
        size_t namelength;
        size_t dirlength;

        namelength = strlen(nametemplate);
        dirlength = strlen(logdir);
        p = nametemplate + namelength - 1;
        ndigits = 0;
        for (;p >= nametemplate; p--) {
            if (*p == digittemplate) {
                ndigits++;
                firstdigitpos = p;
            }
        }
        if (ndigits < DBE_LOGFILENAME_MINDIGITS
         || ndigits > DBE_LOGFILENAME_MAXDIGITS) {
            return (NULL);
        }
        name = SsMemAlloc(namelength+dirlength+2);
        strcpy(name, logdir);
        if (dirlength > 0 && name[dirlength-1] != '/') {
            name[dirlength] = '/';
            name[dirlength+1] = 0;
            dirlength++;
        }
        strcat(name, nametemplate);
        firstdigitpos = name + dirlength+ (firstdigitpos - nametemplate);
        SsLongToAscii(
            (long)logfnum,
            numbuf,
            10,
            sizeof(numbuf) - 1,
            '0',
            FALSE);
        p = numbuf + (sizeof(numbuf) - 1) - ndigits;
        for ( ; ; ) {
            ss_dassert(*firstdigitpos == digittemplate);
            ss_dassert(*p != '\0');
            *firstdigitpos = *p;
            ndigits--;
            if (ndigits == 0) {
                break;
            }
            p++;
            do {
                firstdigitpos++;
                ss_dassert(*firstdigitpos != '\0');
            } while (*firstdigitpos != digittemplate);
        }
        ss_dprintf_2(("dbe_logfile_genname:%s\n", name));
        return (name);
}


/*#***********************************************************************\
 *
 *              logfile_putheader
 *
 * Puts a header record to logfile. The header records tell which is
 * the checkpoint number at the creation moment of file and which log
 * file number the file is.
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      cpnum - in
 *          checkpoint number to put into header or 0
 *          to read it from the counter object
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t logfile_putheader(
        dbe_logfile_t* logfile,
        dbe_cpnum_t cpnum)
{
        dbe_ret_t rc;
        loghdrdata_t lhdata;
        char lhdata_dbuf[LOGFILE_HEADERSIZE];
        char* p;
        size_t byteswritten;

        p = lhdata_dbuf;
        lhdata.lh_logfnum = dbe_counter_getlogfnum(logfile->lf_counter);
        lhdata.lh_cpnum = (cpnum == 0 ?
            dbe_counter_getcpnum(logfile->lf_counter) : cpnum);
        lhdata.lh_blocksize = (dbe_hdr_blocksize_t)logfile->lf_bufsize;
        lhdata.lh_dbcreatime = logfile->lf_dbcreatime;
        SS_UINT4_STORETODISK(p, lhdata.lh_logfnum);
        p += sizeof(lhdata.lh_logfnum);
        SS_UINT4_STORETODISK(p, lhdata.lh_cpnum);
        p += sizeof(lhdata.lh_cpnum);
        SS_UINT4_STORETODISK(p, lhdata.lh_blocksize);
        p += sizeof(lhdata.lh_blocksize);
        SS_UINT4_STORETODISK(p, lhdata.lh_dbcreatime);

        rc = logfile_putheader_nomutex(
                logfile,
                NULL,
                DBE_TRXID_NULL,
                lhdata_dbuf,
                sizeof(lhdata_dbuf),
                &byteswritten);
        ss_dassert(byteswritten == sizeof(lhdata_dbuf) + 1);
        return (rc);

}

static dbe_ret_t logfile_flush(
        dbe_logfile_t* logfile)
{
        su_profile_timer;

#ifdef SS_HSBG2
        if(logfile->lf_transform) {
            return(SU_SUCCESS);
        }
#endif /* SS_HSBG2 */

        ss_dassert(logfile_mutex_isentered(logfile));

        if (logfile->lf_flushed) {
            /* last operation was flush - no need to flush again */
            return(SU_SUCCESS);
        }
        su_profile_start;

        if (logfile->lf_writebuffer_nbytes > 0) {
            dbe_ret_t rc;
            ss_pprintf_3(("logfile_flush:flush buffered bytes,"
                    " lf_writebuffer_nbytes %ld\n",
                    (long)logfile->lf_writebuffer_nbytes));
            rc = logfile_write(
                    logfile,
                    LOGFILE_WRITE_FLUSH,
                    logfile->lf_writebuffer_address,
                    logfile->lf_writebuffer,
                    logfile->lf_writebuffer_nbytes);
            logfile->lf_writebuffer_nbytes = 0;
            logfile->lf_writebuffer_address = -1;
            if (rc != SU_SUCCESS) {
                return(rc);
            }
        }

        ss_pprintf_3(("logfile_flush:do physical flush\n"));
        SsBFlush(logfile->lf_bfile);

        su_profile_stop("logfile_flush:su_vfh_flush");

#ifdef SS_HSBG2
        if (logfile->lf_hsbsvc != NULL) {
            dbe_hsbg2_log_written_up_to(
                    logfile->lf_hsbsvc,
                    logfile->lf_catchup_logpos,
                    TRUE);
        }
#endif
        SS_PMON_ADD(SS_PMON_LOGFLUSHES_PHYSICAL);
        logfile->lf_flushed = TRUE;
        return(SU_SUCCESS);
}

/*#***********************************************************************\
 *
 *              logfile_writebuf
 *
 * Writes the logfile output buffer to disk
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      flush - in
 *
 *      forcefill - in
 *          TRUE when ping-pong duplicates must be removed
 *
 *
 * Return value :
 *      DBE_RC_SUCC (=SU_SUCCESS) when ok or
 *      error code from failed subroutine otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t logfile_writebuf(
        dbe_logfile_t* logfile,
        bool flush,
        bool forcefill)
{
        dbe_ret_t rc;
        bool pingpong;      /* TRUE when ping-pong write is in effect */
        bool changeblock;   /* TRUE when next write goes to new block */
        bool flushit = FALSE;
        bool need_new_hsbbuf = TRUE;

        ss_pprintf_1(("logfile_writebuf:start %d, flush=%d, forcefill=%d\n",
                      logfile->lf_lp.lp_bufpos, flush, forcefill));

#ifdef IO_OPT
        ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                                   SS_DIRECTIO_ALIGNMENT) );
#endif
        rc = DBE_RC_SUCC;
        pingpong = FALSE;
        changeblock = FALSE;

        if (flush || logfile->lf_flushflag
#ifndef DBE_GROUPCOMMIT
                || logfile->lf_lazyflushflag
#endif /* !DBE_GROUPCOMMIT */
        ) {
            /* Flush all prior written blocks to
             * force the write order be correct.
             * (ie. the buffer that is now written must be the last
             * one to be physically written to disk)
             */
            ss_pprintf_3(("logfile_writebuf:flush=%d || logfile->lf_flushflag=%d:flush prior written block\n",
                          flush, logfile->lf_flushflag));
            flushit = TRUE;
            if (logfile->lf_wrmode == DBE_LOGWR_PINGPONG) {
                rc = logfile_flush(logfile);
                if (rc != SU_SUCCESS) {
                    return(rc);
                }
            }
        }
        if (logfile->lf_lp.lp_bufpos != 0) {

            if (logfile->lf_lp.lp_bufpos < logfile->lf_bufdatasize
            &&  !forcefill)
            {
                /* Block is not full and forcefill flag is not
                 * set
                 */
                ss_pprintf_3(("logfile_writebuf:Block is not full and forcefill flag is not set\n"));
                switch (logfile->lf_wrmode) {
                    case DBE_LOGWR_PINGPONG:
                        pingpong = TRUE;
#ifdef IO_OPT
                        if (logfile->lf_lastblock ==
                            dbe_lb_getblocknumber(logfile->lf_abuffer->alb_buffer))
                        {
                            /* New version of a prior written block */
                            logfile_toggle_pingpong(logfile);
                        } else {
                            /* It's 1st time we write this block */
                            logfile->lf_lastblock =
                                dbe_lb_getblocknumber(logfile->lf_abuffer->alb_buffer);
                            logfile->lf_pingpong_incr = 0L;
                        }
                        break;
                    case DBE_LOGWR_ONCE:
                        changeblock = TRUE;
                        /* FALLTHROUGH */
                    case DBE_LOGWR_OVER:
                    case DBE_LOGWR_LAZY:
                        logfile->lf_pingpong_incr = 0L;
                        logfile->lf_lastblock =
                            dbe_lb_getblocknumber(logfile->lf_abuffer->alb_buffer);
                        break;
                    default:
                        ss_error;
                        break;
                }
                /* Fill the rest of buffer with NOPs */

                ss_dprintf_3(("logfile_writebuf:zero fill %d bytes from %d\n",
                              logfile->lf_lp.lp_bufpos,
                              logfile->lf_bufdatasize
                                      - logfile->lf_lp.lp_bufpos));
                memset(DBE_LB_DATA(logfile->lf_abuffer->alb_buffer)
                        + logfile->lf_lp.lp_bufpos,
                        DBE_LOGREC_NOP,
                        logfile->lf_bufdatasize - logfile->lf_lp.lp_bufpos);
                ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                           SS_DIRECTIO_ALIGNMENT) );

                /* Increment block version */
                dbe_lb_incversion(logfile->lf_abuffer->alb_buffer,
                                  logfile->lf_bufsize);
                ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                           SS_DIRECTIO_ALIGNMENT) );

                rc = logfile_write(
                        logfile,
                        LOGFILE_WRITE_BUFFERED,
                        logfile->lf_lp.lp_daddr +
                            logfile->lf_pingpong_incr,
                        logfile->lf_abuffer->alb_buffer,
                        logfile->lf_bufsize);
                ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                           SS_DIRECTIO_ALIGNMENT) );
            } else {
                /* write whole block or force fill block
                 * changing to a new block
                 */
                ss_pprintf_3(("logfile_writebuf:write whole block\n"));
                if (logfile->lf_lp.lp_bufpos < logfile->lf_bufdatasize) {
                    /* It's force fill
                     */
                    ss_dassert(forcefill);
                    ss_dprintf_3(("logfile_writebuf:zero fill %d bytes "
                            "from %d\n",
                            logfile->lf_lp.lp_bufpos,
                            logfile->lf_bufdatasize
                                    - logfile->lf_lp.lp_bufpos));
                    memset(DBE_LB_DATA(logfile->lf_abuffer->alb_buffer)
                            + logfile->lf_lp.lp_bufpos,
                            DBE_LOGREC_NOP,
                            logfile->lf_bufdatasize
                                    - logfile->lf_lp.lp_bufpos);
                } else {
                    ss_dassert(logfile->lf_lp.lp_bufpos
                            == logfile->lf_bufdatasize);
                    SS_PMON_ADD_N(SS_PMON_LOGFLUSHES_FULLPAGES, flushit);
                }
                ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                           SS_DIRECTIO_ALIGNMENT) );

                changeblock = TRUE;
                if (logfile->lf_wrmode == DBE_LOGWR_PINGPONG) {
                    if (logfile->lf_lastblock ==
                        dbe_lb_getblocknumber(logfile->lf_abuffer->alb_buffer))
                    {
                        pingpong = TRUE;
                        logfile_toggle_pingpong(logfile);
                    }
                }
                dbe_lb_incversion(logfile->lf_abuffer->alb_buffer,
                                  logfile->lf_bufsize);
                ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                           SS_DIRECTIO_ALIGNMENT) );

                rc = logfile_write(
                        logfile,
                        LOGFILE_WRITE_BUFFERED,
                        logfile->lf_lp.lp_daddr +
                            logfile->lf_pingpong_incr,
                        logfile->lf_abuffer->alb_buffer,
                        logfile->lf_bufsize);
                ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                           SS_DIRECTIO_ALIGNMENT) );

                if (pingpong) {
                    if (rc == DBE_RC_SUCC) {
                        if (logfile->lf_pingpong_incr != 0L) {
                            ss_pprintf_3(("logfile_writebuf:ping-pong "
                                    "write\n"));
                            dbe_lb_incversion(logfile->lf_abuffer->alb_buffer,
                                              logfile->lf_bufsize);
                            rc = logfile_flush(logfile);
                            if (rc == DBE_RC_SUCC) {
                                rc = logfile_write(
                                    logfile,
                                    LOGFILE_WRITE_BUFFERED,
                                    logfile->lf_lp.lp_daddr,
                                    logfile->lf_abuffer->alb_buffer,
                                    logfile->lf_bufsize);
                            }
                        }
                        if (rc == DBE_RC_SUCC && forcefill) {
#ifdef SS_HSBG2
                            /* Add the previous buffer to recording logdata
                             */
                            if (logfile->lf_hsbld != NULL) {
                                ss_dassert(logfile->lf_lp.lp_bufpos
                                        == logfile->lf_bufdatasize);
                                dbe_logdata_addbuffer(logfile->lf_hsbld,
                                        logfile->lf_hsbbuf);
                                logfile->lf_dbg_nbuffers_involved++;

                                dbe_logdata_print(logfile->lf_hsbld,
                                        "writebuf:addbuffer");

                            }
                            logfile->lf_hsbbuf =
                                    dbe_lbm_getnext_hsbbuffer(logfile->lf_lbm,
                                        logfile->lf_hsbbuf,
                                        logfile->lf_bufsize);
                            logfile->lf_abuffer =
                                    dbe_hsbbuf_get_alogbuf(logfile->lf_hsbbuf);
                            ss_pprintf_3(("logfile_writebuf:"
                                    "dbe_lbm_getnextbuffer:pingpong:%x\n",
                                    logfile->lf_abuffer->alb_buffer));
                            need_new_hsbbuf = FALSE;
#endif /* SS_HSBG2 */
                            ss_dprintf_3(("logfile_writebuf:zero fill whole "
                                    "buffer\n"));
                            memset(DBE_LB_DATA(
                                    logfile->lf_abuffer->alb_buffer),
                                    DBE_LOGREC_NOP,
                                    logfile->lf_bufdatasize);

                            ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer), SS_DIRECTIO_ALIGNMENT) );

                            dbe_lb_incblock(logfile->lf_abuffer->alb_buffer,
                                        logfile->lf_bufsize);
                            dbe_lb_incversion(logfile->lf_abuffer->alb_buffer,
                                          logfile->lf_bufsize);
                            rc = logfile_flush(logfile);
                            if (rc == DBE_RC_SUCC) {
                                rc = logfile_write(
                                    logfile,
                                    LOGFILE_WRITE_BUFFERED,
                                    logfile->lf_lp.lp_daddr + 1,
                                    logfile->lf_abuffer->alb_buffer,
                                    logfile->lf_bufsize);
                            }
                        }
                    }
                }
                logfile->lf_pingpong_incr = 0L;
            }

            if (rc == DBE_RC_SUCC && changeblock) {
#ifdef SS_HSBG2
                if (logfile->lf_hsbsvc != NULL) {
                    dbe_hsbg2_log_written_up_to(
                            logfile->lf_hsbsvc,
                            logfile->lf_catchup_logpos,
                            TRUE);
                }

                /* Add the previous buffer to recording logdata
                 */
                if (logfile->lf_hsbld != NULL) {
                    dbe_logdata_addbuffer(logfile->lf_hsbld,
                                          logfile->lf_hsbbuf);
                    logfile->lf_dbg_nbuffers_involved++;

                    dbe_logdata_print(logfile->lf_hsbld, "writebuf:addbuffer");

                }
                if (need_new_hsbbuf) {
                    logfile->lf_hsbbuf =
                            dbe_lbm_getnext_hsbbuffer(logfile->lf_lbm,
                                logfile->lf_hsbbuf,
                                logfile->lf_bufsize);
                    logfile->lf_abuffer =
                            dbe_hsbbuf_get_alogbuf(logfile->lf_hsbbuf);
                    ss_dassert(DBE_LB_ALIGNMENT(logfile->lf_abuffer->alb_buffer,
                               SS_DIRECTIO_ALIGNMENT));

                    ss_pprintf_3(("logfile_writebuf:dbe_lbm_getnextbuffer:%x\n",
                                  logfile->lf_abuffer->alb_buffer));
                }
#endif /* SS_HSBG2 */

                logfile->lf_lp.lp_daddr++;
                dbe_lb_incblock(logfile->lf_abuffer->alb_buffer,
                                logfile->lf_bufsize);
                logfile->lf_lp.lp_bufpos = 0;
            }
        }
        ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                   SS_DIRECTIO_ALIGNMENT) );
        if (flushit || pingpong) {
            ss_pprintf_3(("logfile_writebuf:flushit=%d || pingpong=%d\n",
                          flushit,
                          pingpong));
            ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_abuffer->alb_buffer),
                       SS_DIRECTIO_ALIGNMENT) );
            logfile_flush(logfile);
        }
#ifdef DBE_GROUPCOMMIT
        logfile->lf_flushflag = FALSE;
#else /* DBE_GROUPCOMMIT */

        if (changeblock) {
            logfile->lf_flushflag = FALSE;
        }
        if (logfile->lf_nwait > 0) {
            /* Wake up threads waiting for a group commit */
            SsMesSend(logfile->lf_wait);
        }
        logfile->lf_lazyflushflag = FALSE;
#endif /* DBE_GROUPCOMMIT */
        return (rc);
}
#else /* IO_OPT */
                        if (logfile->lf_lastblock ==
                            dbe_lb_getblocknumber(logfile->lf_buffer))
                        {
                            /* New version of a prior written block */
                            logfile_toggle_pingpong(logfile);
                        } else {
                            /* It's 1st time we write this block */
                            logfile->lf_lastblock =
                                dbe_lb_getblocknumber(logfile->lf_buffer);
                            logfile->lf_pingpong_incr = 0L;
                        }
                        break;
                    case DBE_LOGWR_ONCE:
                        changeblock = TRUE;
                        /* FALLTHROUGH */
                    case DBE_LOGWR_OVER:
                    case DBE_LOGWR_LAZY:
                        logfile->lf_pingpong_incr = 0L;
                        logfile->lf_lastblock =
                            dbe_lb_getblocknumber(logfile->lf_buffer);
                        break;
                    default:
                        ss_error;
                        break;
                }
                /* Fill the rest of buffer with NOPs */

                ss_dprintf_3(("logfile_writebuf:zero fill %d bytes from %d\n",
                              logfile->lf_lp.lp_bufpos,
                              logfile->lf_bufdatasize
                                      - logfile->lf_lp.lp_bufpos));
                memset(DBE_LB_DATA(logfile->lf_buffer) + logfile->lf_lp.lp_bufpos,
                       DBE_LOGREC_NOP,
                       logfile->lf_bufdatasize - logfile->lf_lp.lp_bufpos);
                /* Increment block version */
                dbe_lb_incversion(logfile->lf_buffer, logfile->lf_bufsize);
                rc = logfile_write(
                        logfile,
                        LOGFILE_WRITE_BUFFERED,
                        logfile->lf_lp.lp_daddr +
                            logfile->lf_pingpong_incr,
                        logfile->lf_buffer,
                        logfile->lf_bufsize);
            } else {
                /* write whole block or force fill block
                 * changing to a new block
                 */
                ss_pprintf_3(("logfile_writebuf:write whole block\n"));
                if (logfile->lf_lp.lp_bufpos < logfile->lf_bufdatasize) {
                    /* It's force fill
                     */
                    ss_dassert(forcefill);
                    ss_dprintf_3(("logfile_writebuf:zero fill %d bytes from %d\n", logfile->lf_lp.lp_bufpos, logfile->lf_bufdatasize - logfile->lf_lp.lp_bufpos));
                    memset(
                        DBE_LB_DATA(logfile->lf_buffer) +
                            logfile->lf_lp.lp_bufpos,
                        DBE_LOGREC_NOP,
                        logfile->lf_bufdatasize - logfile->lf_lp.lp_bufpos);
                } else {
                    ss_dassert(logfile->lf_lp.lp_bufpos == logfile->lf_bufdatasize);
                    SS_PMON_ADD_N(SS_PMON_LOGFLUSHES_FULLPAGES, flushit);
                }
                changeblock = TRUE;
                if (logfile->lf_wrmode == DBE_LOGWR_PINGPONG) {
                    if (logfile->lf_lastblock ==
                        dbe_lb_getblocknumber(logfile->lf_buffer))
                    {
                        pingpong = TRUE;
                        logfile_toggle_pingpong(logfile);
                    }
                }
                dbe_lb_incversion(logfile->lf_buffer, logfile->lf_bufsize);
                rc = logfile_write(
                        logfile,
                        LOGFILE_WRITE_BUFFERED,
                        logfile->lf_lp.lp_daddr +
                            logfile->lf_pingpong_incr,
                        logfile->lf_buffer,
                        logfile->lf_bufsize);
                if (pingpong) {
                    if (rc == DBE_RC_SUCC) {
                        if (logfile->lf_pingpong_incr != 0L) {
                            ss_pprintf_4(("logfile_writebuf:ping-pong write\n"));
                            dbe_lb_incversion(logfile->lf_buffer, logfile->lf_bufsize);
                            rc = logfile_flush(logfile);
                            if (rc == DBE_RC_SUCC) {
                                rc = logfile_write(
                                        logfile,
                                        LOGFILE_WRITE_BUFFERED,
                                        logfile->lf_lp.lp_daddr,
                                        logfile->lf_buffer,
                                        logfile->lf_bufsize);
                            }
                        }
                        if (rc == DBE_RC_SUCC && forcefill) {
#ifdef SS_HSBG2
                            /* Add the previous buffer to recording logdata
                             */
                            if (logfile->lf_hsbld != NULL) {
                                ss_dassert(logfile->lf_lp.lp_bufpos == logfile->lf_bufdatasize);
                                dbe_logdata_addbuffer(logfile->lf_hsbld, logfile->lf_hsbbuf);
                                logfile->lf_dbg_nbuffers_involved++;

                                dbe_logdata_print(logfile->lf_hsbld, "writebuf:addbuffer");

                            }
                            logfile->lf_hsbbuf = dbe_lbm_getnext_hsbbuffer(logfile->lf_lbm, logfile->lf_hsbbuf, logfile->lf_bufsize);
                            logfile->lf_buffer = dbe_hsbbuf_get_logbuf(logfile->lf_hsbbuf);
                            ss_pprintf_3(("logfile_writebuf:dbe_lbm_getnextbuffer:pingpong:%x\n", logfile->lf_buffer));
                            need_new_hsbbuf = FALSE;
#endif /* SS_HSBG2 */
                            ss_dprintf_3(("logfile_writebuf:zero fill whole buffer\n"));
                            memset( DBE_LB_DATA(logfile->lf_buffer),
                                    DBE_LOGREC_NOP,
                                    logfile->lf_bufdatasize);
                            dbe_lb_incblock(logfile->lf_buffer,
                                        logfile->lf_bufsize);
                            dbe_lb_incversion(logfile->lf_buffer,
                                          logfile->lf_bufsize);
                            rc = logfile_flush(logfile);
                            if (rc == DBE_RC_SUCC) {
                                rc = logfile_write(
                                        logfile,
                                        LOGFILE_WRITE_BUFFERED,
                                        logfile->lf_lp.lp_daddr + 1,
                                        logfile->lf_buffer,
                                        logfile->lf_bufsize);
                            }
                        }
                    }
                }
                logfile->lf_pingpong_incr = 0L;
            }

            if (rc == DBE_RC_SUCC && changeblock) {
#ifdef SS_HSBG2
                if (logfile->lf_hsbsvc != NULL) {
                    dbe_hsbg2_log_written_up_to(
                            logfile->lf_hsbsvc,
                            logfile->lf_catchup_logpos,
                            TRUE);
                }

                /* Add the previous buffer to recording logdata
                 */
                if (logfile->lf_hsbld != NULL) {
                    dbe_logdata_addbuffer(logfile->lf_hsbld, logfile->lf_hsbbuf);
                    logfile->lf_dbg_nbuffers_involved++;

                    dbe_logdata_print(logfile->lf_hsbld, "writebuf:addbuffer");

                }
                if (need_new_hsbbuf) {
                    logfile->lf_hsbbuf = dbe_lbm_getnext_hsbbuffer(logfile->lf_lbm, logfile->lf_hsbbuf, logfile->lf_bufsize);
                    logfile->lf_buffer = dbe_hsbbuf_get_logbuf(logfile->lf_hsbbuf);
                    ss_pprintf_4(("logfile_writebuf:dbe_lbm_getnextbuffer:%x\n", logfile->lf_buffer));
                }
#endif /* SS_HSBG2 */

                logfile->lf_lp.lp_daddr++;
                dbe_lb_incblock(logfile->lf_buffer, logfile->lf_bufsize);
                logfile->lf_lp.lp_bufpos = 0;
            }
        }
        if (flushit || pingpong) {
            dbe_ret_t flush_rc;
            ss_pprintf_3(("logfile_writebuf:flushit=%d || pingpong=%d\n", flushit, pingpong));
            flush_rc = logfile_flush(logfile);
            if (rc == SU_SUCCESS &&  flush_rc != SU_SUCCESS) {
                rc = flush_rc;
            }
        }
#ifdef DBE_GROUPCOMMIT
        logfile->lf_flushflag = FALSE;
#else /* DBE_GROUPCOMMIT */

        if (changeblock) {
            logfile->lf_flushflag = FALSE;
        }
        if (logfile->lf_nwait > 0) {
            /* Wake up threads waiting for a group commit */
            SsMesSend(logfile->lf_wait);
        }
        logfile->lf_lazyflushflag = FALSE;
#endif /* DBE_GROUPCOMMIT */
        return (rc);
}


#endif /* IO_OPT */




/*#***********************************************************************\
 *
 *              logfile_split
 *
 * Split log (ie. creates a new log file with incremented number)
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      cpnum - in
 *          checkpoint number to put into the header of the file
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t logfile_split(
        dbe_logfile_t* logfile,
        dbe_cpnum_t cpnum)
{
        dbe_ret_t rc;

        ss_dprintf_3(("logfile_split\n"));
        ss_dassert(logfile_mutex_isentered(logfile));

        rc = DBE_RC_SUCC;
        if (logfile->lf_lp.lp_bufpos != 0) {
            rc = logfile_writebuf(logfile, TRUE, TRUE);
        }
        if (rc == DBE_RC_SUCC) {

#ifdef SS_HSBG2
            logfile->lf_dbg_nbuffers_involved++;
            logfile->lf_hsbbuf =
                    dbe_lbm_getnext_hsbbuffer(logfile->lf_lbm,
                                              logfile->lf_hsbbuf,
                                              logfile->lf_bufsize);
#ifdef IO_OPT
            logfile->lf_abuffer = dbe_hsbbuf_get_alogbuf(logfile->lf_hsbbuf);
            ss_pprintf_1(("logfile_split:dbe_lbm_getnextbuffer:%x\n",
                          logfile->lf_abuffer->alb_buffer));
#else
            logfile->lf_buffer = dbe_hsbbuf_get_logbuf(logfile->lf_hsbbuf);
            ss_pprintf_1(("logfile_split:dbe_lbm_getnextbuffer:%x\n",
                          logfile->lf_buffer));
#endif /* IO_OPT */
#endif /* SS_HSBG2 */

            logfile->lf_lp.lp_bufpos = 0;

            logfile->lf_pingpong_incr = 0L;
            logfile->lf_logfnum = dbe_counter_inclogfnum(logfile->lf_counter);
            if (logfile->lf_filename != NULL) {
                SsMemFree(logfile->lf_filename);
            }
            logfile->lf_filename = dbe_logfile_genname(
                                        logfile->lf_logdir,
                                        logfile->lf_nametemplate,
                                        logfile->lf_logfnum,
                                        logfile->lf_digittemplate);
            SsBClose(logfile->lf_bfile);
            logfile->lf_writebuffer_address = -1;
            logfile->lf_writebuffer_nbytes = 0;
            logfile->lf_bfile = SsBOpen(
                                    logfile->lf_filename,
                                    logfile->lf_openflags,
                                    logfile->lf_bufsize);
            logfile->lf_lp.lp_daddr = logfile_getsize(logfile);
            if (logfile->lf_lp.lp_daddr != 0L) {
                su_informative_exit(
                        __FILE__,
                        __LINE__,
                        DBE_ERR_LOGFILEALREADYEXISTS_S,
                        logfile->lf_filename);
            }
            rc = logfile_putheader(logfile, cpnum);
            if (rc == DBE_RC_SUCC) {
                ss_pprintf_1(("dbe_db_logfnumrange:logfile_writebuf after logfile_putheader\n"));
                rc = logfile_flush(logfile);
            }
        }
        return (rc);
}

/*#***********************************************************************\
 *
 *              logfile_committrxinfo_mustflush
 *
 * Checks committrx info flaghs and makes flush decision based on those.
 *
 * Parameters :
 *
 *              logfile -
 *
 *
 *              info -
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
static bool logfile_committrxinfo_mustflush(
        dbe_logfile_t* logfile,
        int info)
{
        if (SU_BFLAG_TEST(info, DBE_LOGI_COMMIT_NOFLUSH)) {
#ifdef DBE_GROUPCOMMIT_QUEUE
            logfile->lf_lazyflush_request = TRUE;
#endif
            ss_pprintf_3(("noflush flag set, no flush\n"));
            SS_RTCOVERAGE_INC(SS_RTCOV_TRX_NOFLUSH);
            return(FALSE);
        }
        if (SU_BFLAG_TEST(info, DBE_LOGI_COMMIT_HSBPRIPHASE1)) {
            if (SU_BFLAG_TEST(info, DBE_LOGI_COMMIT_LOCAL)) {
                ss_pprintf_3(("primary generated phase1, no flush\n"));

                if (!logfile->lf_nondurable_commit) {
                    ss_dprintf_1(("primary phase1 COMMIT, no flush:lf_nondurable_commit=TRUE\n"));
                    logfile->lf_nondurable_commit = TRUE;
                }

                SS_RTCOVERAGE_INC(SS_RTCOV_TRX_PRIPHASE1_NOFLUSH);
                return(FALSE);
            } else {
                ss_pprintf_3(("secondary generated phase1, must flush\n"));
                return(TRUE);
            }
        }
        if (SU_BFLAG_TEST(info, DBE_LOGI_COMMIT_HSBPRIPHASE2)) {
            if (SU_BFLAG_TEST(info, DBE_LOGI_COMMIT_LOCAL)) {
                ss_pprintf_3(("primary generated phase2, must flush\n"));
                SS_RTCOVERAGE_INC(SS_RTCOV_TRX_PRIPHASE2_FLUSH);
                return(TRUE);
            } else {
                ss_pprintf_3(("secondary generated phase2, no flush\n"));
                return(FALSE);
            }
        }
        ss_pprintf_3(("no phase, must flush\n"));
        return(TRUE);
}

/*#***********************************************************************\
 *
 *              logfile_mustflush
 *
 * Returns flush flag value based on logrectype and configuration.
 *
 * Parameters :
 *
 *      logfile -
 *
 *
 *      logrectype -
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
static bool logfile_mustflush(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        ss_byte_t* logdata)
{
        bool flush;

        ss_dassert(logrectype != DBE_LOGREC_SNAPSHOT_NEW &&
                   logrectype != DBE_LOGREC_DELSNAPSHOT &&
                   logrectype != DBE_LOGREC_PREPARETRX);

        switch (logrectype) {
            case DBE_LOGREC_CHECKPOINT_NEW: /* flush this is a must. splitlog parameter is not valid if done asycnhronously */
            case DBE_LOGREC_CLEANUPMAPPING:
#ifdef SS_HSBG2
            case DBE_LOGREC_HSBG2_NEW_PRIMARY:

            /*
            case DBE_LOGREC_HSBG2_DURABLE:
            */
            case DBE_LOGREC_HSBG2_NEWSTATE:
            /* Does not need explit flush, hsb_svc_log_written_up_to should be enough
             * to signal durable mark.
             * case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
             */
#ifndef HSBG2_ASYNC_REMOTE_DURABLE_ACK
            case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
#endif
#endif
            case DBE_LOGREC_FLUSHTODISK:

                flush = TRUE;
                break;

            case DBE_LOGREC_COMMITTRX_INFO:
                ss_rc_dassert(DBE_LOGI_ISVALIDCOMMITINFO(*logdata), *logdata);
                flush = logfile_committrxinfo_mustflush(logfile, *logdata);
#if defined(DBE_GROUPCOMMIT) && !defined(DBE_LAZYLOG_OPT)
#else /* DBE_GROUPCOMMIT */
                if (flush && logfile->lf_wrmode == DBE_LOGWR_LAZY) {
                    ss_pprintf_3(("lazy writemode, overwrite flush to no flush\n"));
                    flush = FALSE;
                }
#endif /* DBE_GROUPCOMMIT */
                break;

            default:
                flush = FALSE;
                break;
        }

        if (flush) {
            ss_pprintf_1(("logfile_mustflush:logrectype %s\n",
                        dbe_logi_getrectypename(logrectype)));
        }

        return(flush);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_getdatalenandlen
 *
 * Returns log data pointer and length.
 *
 * Parameters :
 *
 *      logrectype -
 *
 *
 *      logdata -
 *
 *
 *      logdatalen_or_relid -
 *
 *
 *      p_logdatalen -
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
void* dbe_logfile_getdatalenandlen(
        dbe_logrectype_t logrectype,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalen)
{
        switch (logrectype) {
            case DBE_LOGREC_DELTUPLE:
            case DBE_LOGREC_INSTUPLEWITHBLOBS:
            case DBE_LOGREC_INSTUPLENOBLOBS:
                *p_logdatalen = (size_t)VTPL_GROSSLEN((vtpl_t*)logdata);
                break;
#ifndef SS_MYSQL
            case DBE_LOGREC_MME_DELTUPLE:
            case DBE_LOGREC_MME_INSTUPLEWITHBLOBS:
            case DBE_LOGREC_MME_INSTUPLENOBLOBS:
                logdata = mme_rval_getdata((mme_rval_t*)logdata, p_logdatalen);
                break;
#endif /* SS_MME */

            default:
                *p_logdatalen = (size_t)logdatalen_or_relid;
                break;
        }
        return(logdata);
}

/*#***********************************************************************\
 *
 *		logfile_logdata_close
 *
 * Closes logdata and sends it to HSB subsystem.
 *
 * Parameters :
 *
 *		logfile -
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
static void logfile_logdata_close(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd)
{
#ifdef SS_HSBG2
        if (logfile->lf_hsbld != NULL
            && logfile->lf_instancetype != DBE_LOG_INSTANCE_LOGGING_STANDALONE)
        {
            ss_dprintf_3(("logfile_logdata_close\n"));
            dbe_logdata_close(
                    logfile->lf_hsbld,
                    logfile->lf_hsbbuf,
                    logfile->lf_lp.lp_bufpos,
                    logfile->lf_dbg_nbuffers_involved);

            if (!logfile->lf_transform) {
                dbe_logdata_print(logfile->lf_hsbld, "LOG    ");
            }

            dbe_hsbg2_logdata_take(
                logfile->lf_hsbsvc,
                logfile->lf_hsbld,
                cd,
                logfile->lf_instancetype,
                logfile->lf_instance_ctx);

            logfile->lf_hsbld = NULL;
        }
#endif /* SS_HSBG2 */
}

/*#***********************************************************************\
 *
 *              logfile_putdatatobuffer
 *
 * Puts data to logfile
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      logrectype - in
 *              log record type
 *
 *      trxid - in
 *              transaction id or DBE_TRXNUM_NULL when none
 *
 *      relid_or_originaldatalen - in
 *              relid when the data is a tuple value or original data len
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void logfile_putdatatobuffer(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        size_t logdatalen,
        ss_uint4_t relid_or_originaldatalen,
        char* header_buf,
        size_t* p_bytes_before_binary_write)
{
        char *p;
        bool lengthfieldneeded;
#ifdef SS_MME
        bool mmerow = FALSE;
#endif

        SS_PUSHNAME("logfile_putdatatobuffer");
        ss_pprintf_1(("logfile_putdatatobuffer:"
                "logrec %d, relid_or_len %ld\n",
                logrectype,
                relid_or_originaldatalen));

        lengthfieldneeded = FALSE;
        *p_bytes_before_binary_write = 0;

        switch (logrectype) {
            case DBE_LOGREC_FLUSHTODISK:
            case DBE_LOGREC_HSBG2_SAVELOGPOS:
                *p_bytes_before_binary_write = 0;
                SS_POPNAME;
                return;
            case DBE_LOGREC_NOP:
            case DBE_LOGREC_CREATEUSER:
            case DBE_LOGREC_HSBG2_ABORTALL:
                ss_dassert(logdatalen == 0);
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                break;
            case DBE_LOGREC_HSBG2_NEWSTATE:
                ss_dassert(logdatalen == 1);
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                break;
            case DBE_LOGREC_CHECKPOINT_OLD:
            case DBE_LOGREC_SNAPSHOT_OLD:
                ss_error;

#ifdef SS_HSBG2
            case DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT:
#endif
            case DBE_LOGREC_CHECKPOINT_NEW:
            case DBE_LOGREC_SNAPSHOT_NEW:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                break;
            case DBE_LOGREC_DELSNAPSHOT:
                ss_dassert(logdatalen == sizeof(dbe_cpnum_t));
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                break;
            case DBE_LOGREC_HEADER:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                break;
            case DBE_LOGREC_DELTUPLE:
#ifndef SS_NOBLOB
            case DBE_LOGREC_INSTUPLEWITHBLOBS:
#endif /* SS_NOBLOB */
            case DBE_LOGREC_INSTUPLENOBLOBS:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                lengthfieldneeded = TRUE;
                break;
            /* MME operations */
#ifdef SS_MME
            case DBE_LOGREC_MME_DELTUPLE:
            case DBE_LOGREC_MME_INSTUPLEWITHBLOBS:
            case DBE_LOGREC_MME_INSTUPLENOBLOBS:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                lengthfieldneeded = TRUE;
                mmerow = TRUE;
                break;
#endif /* SS_MME */
            case DBE_LOGREC_CREATETABLE:
            case DBE_LOGREC_CREATETABLE_NEW:
            case DBE_LOGREC_CREATEINDEX:
            case DBE_LOGREC_DROPTABLE:
            case DBE_LOGREC_TRUNCATETABLE:
            case DBE_LOGREC_TRUNCATECARDIN:
            case DBE_LOGREC_DROPINDEX:
            case DBE_LOGREC_CREATEVIEW:
            case DBE_LOGREC_CREATEVIEW_NEW:
            case DBE_LOGREC_DROPVIEW:
            case DBE_LOGREC_ALTERTABLE:
            case DBE_LOGREC_RENAMETABLE:
            case DBE_LOGREC_AUDITINFO:
            case DBE_LOGREC_CREATETABLE_FULLYQUALIFIED:
            case DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED:
            case DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED:
                lengthfieldneeded = TRUE;
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                break;
#ifndef SS_NOBLOB
            case DBE_LOGREC_BLOBG2DATA:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                lengthfieldneeded = TRUE;
                break;
            case DBE_LOGREC_BLOBG2DROPMEMORYREF:
            case DBE_LOGREC_BLOBG2DATACOMPLETE:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 8);
                break;
#endif /* SS_NOBLOB */
            case DBE_LOGREC_COMMITTRX_INFO:
                ss_dassert(logdatalen == 1);
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                break;
            case DBE_LOGREC_ABORTTRX_INFO:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 1);
                break;
            case DBE_LOGREC_ABORTSTMT:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 0);
                break;
            case DBE_LOGREC_COMMITSTMT:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == sizeof(ss_uint4_t));
                break;
            case DBE_LOGREC_INCSYSCTR:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 1);
                break;
            case DBE_LOGREC_SETHSBSYSCTR:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == DBE_HSBSYSCTR_SIZE);
                break;
            case DBE_LOGREC_CREATECTR:
            case DBE_LOGREC_CREATESEQ:
            case DBE_LOGREC_DROPCTR:
            case DBE_LOGREC_DROPSEQ:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                lengthfieldneeded = TRUE;
                ss_dassert(logdatalen >= 5);
                break;
            case DBE_LOGREC_INCCTR:
            case DBE_LOGREC_SETCTR:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert((logrectype == DBE_LOGREC_INCCTR) ?
                    (logdatalen == 4) : (logdatalen == 12));
                break;
            case DBE_LOGREC_SETSEQ:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 12);
                break;

            case DBE_LOGREC_SWITCHTOPRIMARY :
            case DBE_LOGREC_SWITCHTOSECONDARY  :
            case DBE_LOGREC_SWITCHTOSECONDARY_NORESET  :
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
            case DBE_LOGREC_CLEANUPMAPPING  :
                ss_dassert(logdatalen == 0);
                break;
            case DBE_LOGREC_REPLICATRXSTART:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 4);
                break;
            case DBE_LOGREC_REPLICASTMTSTART:
                ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 8);
                break;

#ifdef SS_HSBG2
            case DBE_LOGREC_HSBG2_DURABLE:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == DBE_LOGPOS_BINSIZE);
                break;

            case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 2 * DBE_LOGPOS_BINSIZE);
                break;

            case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 2 * DBE_LOGPOS_BINSIZE);
                break;

            case DBE_LOGREC_HSBG2_NEW_PRIMARY:
                ss_dassert(DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
                ss_dassert(logdatalen == 2 * sizeof(ss_uint4_t));
                break;
#endif /* SS_HSBG2 */

            case DBE_LOGREC_COMMENT:
                ss_dassert(logdatalen == 0);
                break;

            case DBE_LOGREC_INSTUPLE:
                ss_error;   /* deprecated */
            default:
                ss_error;
        }

        p = header_buf;
        *p = (uchar)logrectype;
        p++;
        if (!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL)) {
            SS_UINT4_STORETODISK(p, DBE_TRXID_GETLONG(trxid));
            p += sizeof(ss_uint4_t);
        }
        if (lengthfieldneeded) {
            ss_uint4_t lengthfield;

            ss_dprintf_3(("logfile_putdatatobuffer:write lengthfield\n"));
            lengthfield = relid_or_originaldatalen;
            SS_UINT4_STORETODISK(p, lengthfield);
            p += sizeof(ss_uint4_t);
#ifdef SS_MME
            /* Write the length of the MME row value persistent data */
            if (mmerow) {
                size_t mmelengthfield;

                ss_dprintf_3(("logfile_putdatatobuffer:write mmelength\n"));
                mmelengthfield = logdatalen;
                ss_dassert(mmelengthfield <= SS_UINT2_MAX);
                SS_UINT2_STORETODISK(p, mmelengthfield);
                p += sizeof(ss_uint2_t);
            }
#endif
        }
        *p_bytes_before_binary_write = (ss_byte_t*)p - (ss_byte_t*)header_buf;
        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              logfile_putdata_nomutex_splitif
 *
 * Puts data to logfile
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      logrectype - in
 *              log record type
 *
 *      trxid - in
 *              transaction id or DBE_TRXNUM_NULL when none
 *
 *      logdata - in, use
 *              pointer to data buffer
 *
 *      logdatalen - in
 *              data length in bytes
 *
 *      relid_or_originaldatalen - in
 *              relid when the data is a tuple value or original data len
 *
 *      flush - in
 *              do we need to flush
 *
 *      p_logdatalenwritten - out, use
 *              pointer to variable where the number of bytes written
 *          will be stored
 *
 *              p_splitlog - in out, use
 *          on input TRUE forces the log file to split
 *          on output TRUE means log file was split (either
 *          explicitly or implicitly) and FALSE when
 *          log file was split (input was FALSE also)
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t logfile_putdata_nomutex_splitif(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        size_t logdatalen,
        char* header_buf,
        size_t header_buf_len,
        ss_uint4_t relid_or_originaldatalen,
        bool flush,
        bool hsb_split_queue_force,
        size_t* p_logdatalenwritten,
        bool* p_splitlog)
{
        dbe_ret_t rc;
        uchar *p;
        size_t ntowrite;
        size_t room;
        bool lazyflush;
        bool split;
        size_t bytes_before_binary_write;
        dbe_cpnum_t cpnum;
#ifdef HSB_LPID
        dbe_hsb_lpid_t logpos_id;
        hsb_role_t logpos_role = HSB_ROLE_NONE;
#endif

        SS_PUSHNAME("logfile_putdata_nomutex_splitif");
        ss_pprintf_4(("logfile_putdata_nomutex_splitif:"
                "logrec %d, logdatalen %ld, relid_or_len %ld\n",
                logrectype,
                logdatalen,
                relid_or_originaldatalen));
        LOGFILE_CHECKSHUTDOWN(logfile);
        rc = DBE_RC_SUCC;
        lazyflush = FALSE;
        split = FALSE;
        cpnum = 0L;

#ifdef HSB_LPID
        LPID_SETZERO(logpos_id);
#endif

        bytes_before_binary_write = header_buf_len;

        switch (logrectype) {
            case DBE_LOGREC_CHECKPOINT_NEW:
            case DBE_LOGREC_SNAPSHOT_NEW:
                if (*p_splitlog ||
                    logfile->lf_lp.lp_daddr > logfile->lf_minsplitsize)
                {
                    *p_splitlog = split =
                            (logrectype != DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT);
                    cpnum = SS_UINT4_LOADFROMDISK(logdata);
                }
                break;
            case DBE_LOGREC_COMMITTRX_INFO:
                if (SU_BFLAG_TEST(*((ss_byte_t*)logdata), DBE_LOGI_COMMIT_NOFLUSH))
                {
#ifndef DBE_GROUPCOMMIT
                    lazyflush = TRUE;
#endif /* DBE_GROUPCOMMIT */
                } else {
#if defined(DBE_GROUPCOMMIT) && !defined(DBE_LAZYLOG_OPT)
#else /* DBE_GROUPCOMMIT */
                    if (logfile->lf_wrmode == DBE_LOGWR_LAZY) {
                        lazyflush = TRUE;
                    }
#endif /* DBE_GROUPCOMMIT */
                }
                break;

#ifdef SS_HSBG2
            case DBE_LOGREC_HSBG2_DURABLE:
                if (!logfile->lf_transform) {
                    char* p;
                    char* buf;

                    ss_pprintf_4(("logfile_putdata_nomutex_splitif:"
                            "lf_nondurable_commit==FALSE\n"));

                    logfile->lf_nondurable_commit = FALSE;

                    /* write local last logpos to buf */
                    buf = logdata;
                    p = logdata;

#ifdef HSB_LPID
                    logpos_id = dbe_catchup_logpos_get_newid(logfile->lf_counter);

                    /* Role should be correct at this point. Also it may be
                     * standalone or primary so we can not change it.
                     */
                    logpos_role = *(p + sizeof(dbe_hsb_lpid_t));
                    ss_dassert(logpos_role == HSB_ROLE_PRIMARY
                            || logpos_role == HSB_ROLE_STANDALONE);

                    LPID_STORETODISK(p, logpos_id);
                    p += sizeof(dbe_hsb_lpid_t) + 1;
#endif /* HSB_LPID */
                    SS_UINT4_STORETODISK(p, logfile->lf_logfnum);
                    p += sizeof(ss_uint4_t);
                    SS_UINT4_STORETODISK(p, logfile->lf_lp.lp_daddr);
                    p += sizeof(ss_uint4_t);
                    SS_UINT4_STORETODISK(p, logfile->lf_lp.lp_bufpos);
                }
#ifdef HSB_LPID
                else {
                    char* p = logdata;
                    logpos_id = LPID_LOADFROMDISK(p);
                    p += sizeof(dbe_hsb_lpid_t);
                    logpos_role = *p;
                    ss_dassert(logpos_role == HSB_ROLE_PRIMARY
                            || logpos_role == HSB_ROLE_STANDALONE);
                }
#endif /* HSB_LPID */

                break;

            case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
#ifdef HSB_LPID
                {
                    char* p = logdata;
                    logpos_id = LPID_LOADFROMDISK(p);
                    p += sizeof(dbe_hsb_lpid_t);
                    logpos_role = *p;
                }
#endif /* HSB_LPID */
                if (!logfile->lf_transform) {
                    char* p;
                    char* buf;

                    /* write local last logpos to buf */

                    buf = logdata;
#ifdef HSB_LPID
                    p = buf + sizeof(dbe_hsb_lpid_t) + 1;
#else
                    p = logdata;
#endif /* HSB_LPID */

                    SS_UINT4_STORETODISK(p, logfile->lf_logfnum);
                    p += sizeof(ss_uint4_t);
                    SS_UINT4_STORETODISK(p, logfile->lf_lp.lp_daddr);
                    p += sizeof(ss_uint4_t);
                    SS_UINT4_STORETODISK(p, logfile->lf_lp.lp_bufpos);
                }
                break;
#endif

            default:
                break;
        }

        do {
            if (flush) {
                SS_PMON_ADD(SS_PMON_LOGFLUSHES_LOGICAL);
            }
            if (
#ifdef SS_HSBG2
                !logfile->lf_transform &&
#endif
             split) {   /* need to split log file? */
                rc = logfile_split(logfile, cpnum);
                if (rc != DBE_RC_SUCC) {
#ifdef SS_HSBG2
                    logfile->lf_hsbld = NULL;
#endif
                    ss_rc_derror(rc);
                    break;
                }
            }
            ss_dassert(logfile->lf_lp.lp_bufpos < logfile->lf_bufdatasize);

#ifdef SS_HSBG2

            if (logfile->lf_instancetype != DBE_LOG_INSTANCE_LOGGING_STANDALONE) {
                DBE_CATCHUP_LOGPOS_SET(
                    logfile->lf_catchup_logpos,
                    logfile->lf_logfnum,
                    logfile->lf_lp.lp_daddr,
                    logfile->lf_lp.lp_bufpos);
#ifdef HSB_LPID
                if (logpos_role != HSB_ROLE_NONE) {
                    dbe_catchup_logpos_set_id(&logfile->lf_catchup_logpos,
                                               logpos_id,
                                               logpos_role);
                }
#endif
                SS_PUSHNAME("logfile_putdata_nomutex_splitif");
                if (logfile->lf_hsbld == NULL) {
                    logfile->lf_hsbld = dbe_logdata_init(
                                            trxid,
                                            logrectype,
                                            hsb_split_queue_force,
                                            logfile->lf_hsbbuf,
                                            logfile->lf_lp.lp_bufpos,
                                            logfile->lf_catchup_logpos);
                    logfile->lf_dbg_nbuffers_involved = 1;
                }
                SS_POPNAME;
            }
#endif /* SS_HSBG2 */
#ifdef IO_OPT
            p = DBE_LB_DATA(logfile->lf_abuffer->alb_buffer) +
                    logfile->lf_lp.lp_bufpos;
#else /* IO_OPT */
            p = DBE_LB_DATA(logfile->lf_buffer) + logfile->lf_lp.lp_bufpos;
#endif /* IO_OPT */

            ss_dprintf_3(("logfile_putdata_nomutex_splitif:write logrec header\n"));
            room = logfile->lf_bufdatasize - logfile->lf_lp.lp_bufpos;
            if (bytes_before_binary_write <= room) {
                memcpy(p, header_buf, bytes_before_binary_write);
                p += bytes_before_binary_write;
                logfile->lf_lp.lp_bufpos += bytes_before_binary_write;
                if (logfile->lf_lp.lp_bufpos == logfile->lf_bufdatasize) {
                    if ((flush || lazyflush)
                    &&  logdatalen == 0)
                    {
                        rc = logfile_writebuf(logfile, TRUE, FALSE);
                        flush = FALSE;
                    } else {
                        rc = logfile_writebuf(logfile, FALSE, FALSE);
                    }
                    if (rc == DBE_RC_SUCC) {
#ifdef IO_OPT
                        p = DBE_LB_DATA(logfile->lf_abuffer->alb_buffer);
#else
                        p = DBE_LB_DATA(logfile->lf_buffer);
#endif
                        ss_dassert(logfile->lf_lp.lp_bufpos == 0);
                    }
                }
            } else {
                memcpy(p, header_buf, room);
                logfile->lf_lp.lp_bufpos += room;
                rc = logfile_writebuf(logfile, FALSE, FALSE);
                if (rc == DBE_RC_SUCC) {
#ifdef IO_OPT
                    p = DBE_LB_DATA(logfile->lf_abuffer->alb_buffer);
#else
                    p = DBE_LB_DATA(logfile->lf_buffer);
#endif
                    ss_dassert(logfile->lf_lp.lp_bufpos == 0);
                    memcpy(p, header_buf + room, bytes_before_binary_write - room);
                    p += bytes_before_binary_write - room;
                    logfile->lf_lp.lp_bufpos += bytes_before_binary_write - room;
                }
            }
            *p_logdatalenwritten = bytes_before_binary_write;

            FAKE_CODE_BLOCK(FAKE_DBE_LOG_SLEEP_AFTER_COMMIT, {
                if(logrectype == DBE_LOGREC_COMMITTRX_INFO) {
                    ss_dprintf_1(("FAKE_DBE_LOG_SLEEP_AFTER_COMMIT\n"));
                    SsThrSleep(5000);
                }
            });

            if (rc != DBE_RC_SUCC) {
                break;
            }

            while (rc == DBE_RC_SUCC && logdatalen > 0) {

                ss_pprintf_4(("nomutex_splitif:bufpos %d, logdatalen %d, "
                        "bufsize %d, bytes_before %d, datalenwritten %d\n",
                               logfile->lf_lp.lp_bufpos, logdatalen,
                               logfile->lf_bufsize,
                               bytes_before_binary_write,
                               *p_logdatalenwritten));

                if (logfile->lf_lp.lp_bufpos == 0
                &&  logdatalen >= logfile->lf_bufsize
                &&  *p_logdatalenwritten >= bytes_before_binary_write)
                {           /* Do binary write over n blocks */
                    size_t nblocks;

                    nblocks = logdatalen / logfile->lf_bufsize;
                    ntowrite = nblocks * logfile->lf_bufsize;
                    rc = logfile_write(
                        logfile,
                        LOGFILE_WRITE_NOBUFFER,
                        logfile->lf_lp.lp_daddr,
                        logdata,
                        ntowrite);

#ifdef SS_HSBG2
                    /* here we can write whole buf once to hsb system
                     * this does unnesessary splitting
                     */
                    if (logfile->lf_instancetype !=
                        DBE_LOG_INSTANCE_LOGGING_STANDALONE)
                    {
#ifdef IO_OPT
                        dbe_hsbbuf_t* hsbbuf;
                        dbe_alogbuf_t* alogbuf;
                        alogbuf = dbe_alb_init(ntowrite
                                + 2*sizeof(alogbuf->alb_buffer->lb_.chk));
                        ss_dassert(DBE_LB_ALIGNMENT((alogbuf->alb_buffer),
                                                   SS_DIRECTIO_ALIGNMENT));

                        memcpy(DBE_LB_DATA(alogbuf->alb_buffer), logdata, ntowrite);
                        logfile->lf_dbg_nbuffers_involved++;
                        hsbbuf = dbe_hsbbuf_init(alogbuf,
                                ntowrite + 2*sizeof(alogbuf->alb_buffer->lb_.chk));
#else
                        dbe_hsbbuf_t* hsbbuf;
                        dbe_logbuf_t* logbuf;
                        logbuf = dbe_lb_init(ntowrite + 2*sizeof(logbuf->lb_.chk));
                        memcpy(DBE_LB_DATA(logbuf), logdata, ntowrite);
                        logfile->lf_dbg_nbuffers_involved++;
                        hsbbuf = dbe_hsbbuf_init(logbuf,
                                ntowrite + 2*sizeof(logbuf->lb_.chk));
#endif /* IO_OPT */

                        dbe_logdata_addbuffer(logfile->lf_hsbld, hsbbuf);
                        dbe_logdata_print(logfile->lf_hsbld, "putdata:addbuffer");
                        dbe_hsbbuf_done(hsbbuf);
                    }

#endif /* SS_HSBG2 */

                    logdatalen -= ntowrite;
                    logfile->lf_lp.lp_daddr += nblocks;
                    if (flush && logdatalen == 0) {
                        dbe_ret_t flush_rc;
                        flush_rc = logfile_flush(logfile);
                        if (rc == SU_SUCCESS && flush_rc != SU_SUCCESS) {
                            rc = flush_rc;
                        }
                        flush = FALSE;
                    }
                } else {    /* Do write with block headers & footers */
                    room = logfile->lf_bufdatasize - logfile->lf_lp.lp_bufpos;
                    ntowrite = room < logdatalen ? room : logdatalen;
#ifdef IO_OPT
                    ss_dassert(p + ntowrite
                        <= DBE_LB_DATA(logfile->lf_abuffer->alb_buffer) +
                            logfile->lf_bufdatasize);
#else
                    ss_dassert(p + ntowrite
                        <= DBE_LB_DATA(logfile->lf_buffer) +
                            logfile->lf_bufdatasize);
#endif
                    memcpy(p, logdata, ntowrite);
                    logdatalen -= ntowrite;
                    logfile->lf_lp.lp_bufpos += ntowrite;
                    ss_dprintf_3(("logfile_putdata_nomutex_splitif:write data with block headers and footers\n"));
                    if (ntowrite >= room) {
                        ss_dassert(ntowrite == room);
                        if ((flush || lazyflush) && logdatalen == 0) {
                            rc = logfile_writebuf(logfile, TRUE, FALSE);
                            flush = FALSE;
                        } else {
                            rc = logfile_writebuf(logfile, FALSE, FALSE);
                        }
                        if (rc == DBE_RC_SUCC) {
#ifdef IO_OPT
                            p = DBE_LB_DATA(logfile->lf_abuffer->alb_buffer);
#else
                            p = DBE_LB_DATA(logfile->lf_buffer);
#endif
                            ss_dassert(logfile->lf_lp.lp_bufpos == 0);
                        }
                    } else {
                        p += ntowrite;
                    }
                }
                logdata = (char*)logdata + ntowrite;
                *p_logdatalenwritten += ntowrite;
            }
            if (rc != DBE_RC_SUCC) {
                break;
            }
            if (
#ifdef SS_HSBG2
                            !logfile->lf_transform &&
#endif
                            logfile->lf_lp.lp_bufpos > 0) {
#ifdef DBE_GROUPCOMMIT
                logfile->lf_flushflag |= flush;
                if (logfile->lf_flushflag) {
                    ss_dprintf_3(("logfile_putdata_nomutex_splitif:logfile->lf_flushflag\n"));
                    rc = logfile_writebuf(logfile, TRUE, FALSE);
                }
#else /* DBE_GROUPCOMMIT */
                if (flush) {
                    logfile->lf_flushflag = TRUE;
                    if (logfile->lf_groupcommitdelay > 0
                    &&  logfile->lf_wrmode == DBE_LOGWR_ONCE)
                    {
                        /* Note: groupcommitdelay > 0 also implies
                        ** lf_wrmode == DBE_LOGWR_ONCE
                        */
                        su_daddr_t writepos;    /* saved write pos. */
                        dbe_logfnum_t fnum_old; /* saved file # */
                        dbe_logfnum_t fnum_new; /* newest file # */
                        SsMesRetT msg_rc;       /* return code from SsMesRequest */
                        char text[256];

                        writepos = logfile->lf_lp.lp_daddr;
                        fnum_old = dbe_counter_getlogfnum(logfile->lf_counter);
                        do {
                            logfile->lf_nwait++;
                            logfile_exit_mutex(logfile);
                            SsSprintf(text, "logfile_putdata_nomutex_splitif: delay=%d", logfile->lf_groupcommitdelay);
                            SS_PUSHNAME(text);
                            msg_rc = SsMesRequest(
                                        logfile->lf_wait,
                                        logfile->lf_groupcommitdelay);
                            SS_POPNAME;
                            logfile_enter_mutex(logfile);
                            logfile->lf_nwait--;
                            if (logfile->lf_nwait > 0) {
                                SsMesSend(logfile->lf_wait);
                            }
                            fnum_new = dbe_counter_getlogfnum(
                                            logfile->lf_counter);
                            if (msg_rc == SSMES_RC_TIMEOUT)
                            {
                                if (writepos == logfile->lf_lp.lp_daddr
                                &&  fnum_old == fnum_new)
                                {
                                    ss_dprintf_3(("Logfile: Group commit timeout\n"));
                                    rc = logfile_writebuf(logfile, TRUE, TRUE);
                                } else {
                                    ss_pprintf_3(("Logfile: Group commit false timeout\n"));
                                }
                                break;
                            } else {
#ifdef SS_DEBUG
                                    if (writepos == logfile->lf_lp.lp_daddr
                                    &&  fnum_old == fnum_new)
                                    {
                                        ss_pprintf_3(("Logfile: Group commit false wakeup\n"));

                                    } else {
                                        ss_pprintf_3(("Logfile: Group commit no timeout\n"));
                                    }
#endif /* SS_DEBUG */
                                /* some other thread wrote it already
                                ** or it was a false alarm!
                                */
                            }
                        } while (writepos == logfile->lf_lp.lp_daddr
                            &&   fnum_old == fnum_new);
                    } else {
                        ss_dprintf_3(("logfile_putdata_nomutex_splitif:flush\n"));
                        rc = logfile_writebuf(logfile, TRUE, FALSE);
                    }
                } else if (lazyflush) {
                    logfile->lf_lazyflushflag = TRUE;
                }
#endif /* DBE_GROUPCOMMIT */
            }

        } while (FALSE);
        SS_POPNAME;
        return (rc);
}

/*#***********************************************************************\
 *
 *              logfile_putheader_nomutex
 *
 * Puts header data to logfile.
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      trxid - in
 *              transaction id or DBE_TRXNUM_NULL when none
 *
 *      logdata - in, use
 *              pointer to data buffer
 *
 *      logdatalen_or_relid - in
 *              data length in bytes or relid when the data is a vtuple
 *
 *      p_logdatalenwritten - out, use
 *              pointer to variable where the number of bytes written
 *          will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t logfile_putheader_nomutex(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalenwritten)
{
        bool splitlog = FALSE;
        dbe_ret_t rc;
        char header_buf[MAX_HEADER_BUF];
        size_t header_buf_len;

        logfile_putdatatobuffer(
            logfile,
            cd,
            DBE_LOGREC_HEADER,
            trxid,
            logdata,
            (size_t)logdatalen_or_relid,
            logdatalen_or_relid,
            header_buf,
            &header_buf_len);

        rc = logfile_putdata_nomutex_splitif(
                    logfile,
                    cd,
                    DBE_LOGREC_HEADER,
                    trxid,
                    logdata,
                    (size_t)logdatalen_or_relid,
                    header_buf,
                    header_buf_len,
                    logdatalen_or_relid,
                    FALSE, /* Essential! must not request flushing!
                              otherwise logfile_split should be
                              prepared to handle
                              DBE_RC_WAITFLUSH return code!!!
                           */
                    FALSE,
                    p_logdatalenwritten,
                    &splitlog);
        ss_dassert(rc != DBE_RC_WAITFLUSH);
        ss_assert(p_logdatalenwritten != NULL);
        if (rc == DBE_RC_SUCC) {
            logfile_logdata_close(logfile, cd);
        }
        return (rc);
}

#if defined(SS_MT) && defined(DBE_LAZYLOG_OPT)


static void SS_CALLBACK dbe_logfile_flushthread(void* param)
{
        dbe_logfile_t* logfile = param;

        ss_pprintf_1(("dbe_logfile_flushthread\n"));

        logfile->lf_flushthreadp = TRUE;

        while (logfile->lf_groupcommitdelay) {
            SsThrSleep(logfile->lf_groupcommitdelay);
            ss_pprintf_2(("dbe_logfile_flushthread:flush\n"));
            dbe_logfile_flush(logfile);
        }

        logfile->lf_flushthreadp = FALSE;

        ss_pprintf_2(("dbe_logfile_flushthread:stop\n"));

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}

#endif /* SS_MT && DBE_LAZYLOG_OPT */

#ifdef DBE_GROUPCOMMIT_QUEUE

static void SS_CALLBACK logf_lazy_flushthr(void* param)
{
        dbe_logfile_t* logfile = param;
        bool request;
        dbe_catchup_logpos_t local_durable_logpos;
        SsTimeT start_time_ms;
        SsTimeT now_time_ms;
        SsTimeT flush_time_ms;
        long lazyflush_delay;
        ss_debug(SsTimeT flush_last = 0;)
        ss_debug(SsTimeT flush_now;)

        ss_pprintf_1(("logf_lazy_flushthr:delay %ld\n", logfile->lf_lazyflush_delay));

        DBE_CATCHUP_LOGPOS_SET_NULL(local_durable_logpos);

        logfile->lf_lazyflush_threadp = TRUE;
        lazyflush_delay = logfile->lf_lazyflush_delay;

        ss_dassert(logfile->lf_lazyflush_wait != NULL);

        while (logfile->lf_lazyflush_delay > 0L) {
            bool check_lazyflush;
            
            start_time_ms = SsTimeMs();
            
            SsMesRequest(logfile->lf_lazyflush_wait, lazyflush_delay);
            
            now_time_ms = SsTimeMs();
            check_lazyflush = TRUE;

            if (logfile->lf_lazyflush_extendfile) {
                long logfile_size;
                long new_size;
                long cur_addr;
                long limit;
                bool b;

                logfile->lf_lazyflush_extendfile = FALSE;
                limit = logfile->lf_lazyflush_extendincrement;

                logfile_enter_mutex(logfile);

                logfile_size = logfile_getsize(logfile);
                cur_addr = logfile->lf_lp.lp_daddr;

                ss_pprintf_2(("logf_lazy_flushthr:extendfile, limit=%ld, logfile_size=%ld, addr=%ld\n", limit, (long)logfile_size, (long)cur_addr));
                
                if (logfile_size - cur_addr < limit / 2) {
                    new_size = logfile_size + limit;
                    ss_pprintf_2(("logf_lazy_flushthr:extendfile, extend size to %ld\n", (long)new_size));
                    b = SsBChsizePages(logfile->lf_bfile, new_size, logfile->lf_bufsize);
                    ss_dassert(b);
                }

                logfile_exit_mutex2(logfile);

                lazyflush_delay = lazyflush_delay - (now_time_ms - start_time_ms);
                if (lazyflush_delay < 100) {
                    check_lazyflush = TRUE;
                } else {
                    check_lazyflush = FALSE;
                }

            }
            if (check_lazyflush) {

                logfile_enter_mutex(logfile);

                if (logfile->lf_nondurable_commit && logfile->lf_idlehsbdurable == DBE_LOGFILE_IDLEHSBDURABLE_ON) {
                    logfile_exit_mutex2(logfile);
                    ss_pprintf_1(("logf_lazy_flushthr:logfile_put_durable:lf_nondurable_commit==TRUE\n"));
                    dbe_logfile_put_durable(logfile, NULL, HSB_ROLE_PRIMARY, local_durable_logpos);
                    logfile_enter_mutex(logfile);
                }
                if (logfile->lf_lazyflush_delay > 0L) {
                    request = logfile->lf_lazyflush_request;
                } else {
                    request = FALSE;
                }
                if (request) {
                    logfile->lf_lazyflush_request = FALSE;
                    logfile->lf_lazyflush_do = TRUE;
                }
                logfile_exit_mutex2(logfile);
                if (request) {
                    ss_pprintf_2(("logf_lazy_flushthr:flush\n"));
                    ss_debug(flush_now = SsTime(NULL));
                    ss_rc_dassert(flush_now - flush_last >= (logfile->lf_lazyflush_delay / 1000) - 1, flush_now - flush_last);
                    ss_debug(flush_last = flush_now);
                    SsMesSend(logfile->lf_wqinfo.wqi_writequeue_mes);
                }
                lazyflush_delay = logfile->lf_lazyflush_delay;
            }
        }
        logfile->lf_lazyflush_threadp = FALSE;
        ss_pprintf_2(("logf_lazy_flushthr:stop\n"));

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}

static void logf_writequeue_free(logf_writequeue_t* wq)
{
        if (wq->wq_dyndata != NULL) {
            ss_dassert(wq->wq_logdata == wq->wq_dyndata);
            SsMemFree(wq->wq_dyndata);
            wq->wq_dyndata = NULL;
        }
}

static bool logfile_logdata_mustclose(dbe_logrectype_t logrectype)
{
        switch (logrectype) {
            case DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT:
            case DBE_LOGREC_CHECKPOINT_NEW:
            case DBE_LOGREC_SNAPSHOT_NEW:
            case DBE_LOGREC_HSBG2_SAVELOGPOS:
            case DBE_LOGREC_FLUSHTODISK:
            case DBE_LOGREC_HSBG2_DURABLE:
            case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
            case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
            case DBE_LOGREC_HSBG2_NEW_PRIMARY:
            case DBE_LOGREC_HSBG2_NEWSTATE:
            case DBE_LOGREC_COMMITTRX_INFO:
                ss_dprintf_4(("logfile_logdata_mustclose:TRUE\n"));
                return(TRUE);
            default:
                ss_dprintf_4(("logfile_logdata_mustclose:FALSE\n"));
                return(FALSE);
        }
}

static void SS_CALLBACK logf_writequeue_flushthr(void* data)
{
        logf_writequeinfo_t* wqinfo = data;
        dbe_logfnum_t   last_logfnum = 0;
        su_daddr_t      last_daddr = 0;
        size_t          last_bufpos = 0;
        size_t          logdata_groupsize;
        rs_sysi_t*      close_cd;

        wqinfo->wqi_writeactive = TRUE;

        for (;;) {
            su_meswaitlist_t* meswaitlist;
            su_meswaitlist_t* flushmeswaitlist;
            logf_writequeue_t* wq;
            logf_writequeue_t* wq_first;
            logf_writequeue_t* wq_last;
            bool flush;
            bool splitlog = FALSE;
            size_t logdatalenwritten;
            int qlen;
            long written_now;
            dbe_ret_t rc;
            su_profile_timer;

            do {
                wq_first = NULL;
                meswaitlist = NULL;
                flushmeswaitlist = NULL;

                if (wqinfo->wqi_writequeue_first != NULL) {
                    SsFlatMutexLock(wqinfo->wqi_writequeue_mutex);

                    if (wqinfo->wqi_writequeue_first != NULL) {
                        meswaitlist = wqinfo->wqi_writequeue_meswaitlist;
                        wqinfo->wqi_writequeue_meswaitlist = su_meswaitlist_init();
                        flushmeswaitlist = wqinfo->wqi_writequeue_flushmeswaitlist;
                        wqinfo->wqi_writequeue_flushmeswaitlist = su_meswaitlist_init();
                        
                        wq_first = wqinfo->wqi_writequeue_first;
                        wqinfo->wqi_writequeue_first = NULL;
                        wqinfo->wqi_writequeue_last = NULL;
                    }

                    SsFlatMutexUnlock(wqinfo->wqi_writequeue_mutex);

                    if (wq_first != NULL || wqinfo->wqi_logfile->lf_lazyflush_do) {
                        ss_pprintf_1(("logf_writequeue_flushthr:found data from writequeue, lf_lazyflush_do=%d\n", wqinfo->wqi_logfile->lf_lazyflush_do));
                        break;
                    }
                }

                if (wqinfo->wqi_done) {
                    break;
                }

                ss_pprintf_1(("logf_writequeue_flushthr:wait mes\n"));

                SsMesWait(wqinfo->wqi_writequeue_mes);

                SsMesReset(wqinfo->wqi_writequeue_mes);

                ss_pprintf_1(("logf_writequeue_flushthr:got mes\n"));

                SS_PMON_ADD(SS_PMON_LOGGROUPCOMMIT_WAKEUPS);

            } while (TRUE);

            su_profile_start;

            ss_pprintf_1(("logf_writequeue_flushthr:wakeupall meswaitlist\n"));
            su_meswaitlist_wakeupall(meswaitlist);

            flush = FALSE;
            wq_last = NULL;
            rc = DBE_RC_SUCC;
            written_now = 0;
            logdata_groupsize = 0;
            close_cd = NULL;

            logfile_enter_mutex(wqinfo->wqi_logfile);

            ss_dassert(wqinfo->wqi_logfile->lf_hsbld == NULL);

            for (wq = wq_first, qlen = 0; wq != NULL; wq = wq->wq_next, qlen++) {
                bool logdata_mustclose;

                SS_PMON_ADD(SS_PMON_LOGWRITEQUEUEWRITE);

                if (logdata_groupsize == 0) {
                    close_cd = wq->wq_cd;
                }
                logdata_groupsize++;

                ss_pprintf_4(("logf_writequeue_flushthr:wq->wq_trxid=%ld, wq->wq_logrectype=%d %s, logdata_groupsize=%d\n",
                    DBE_TRXID_GETLONG(wq->wq_trxid), wq->wq_logrectype,
                    wq->wq_flushtodisk ? "FLUSH" : "",
                    logdata_groupsize));

                if (wq->wq_flushtodisk) {
                    flush = TRUE;
                    SS_PMON_ADD(SS_PMON_LOGFLUSHES_LOGICAL);
                    ss_beta(dbe_logrectype_flushcount[wq->wq_logrectype]++;)
                }

                written_now += wq->wq_header_buf_len + wq->wq_logdatalen;

                splitlog = FALSE;

                if (flush
                    || wq->wq_split_hsbqueue_force
                    || logfile_logdata_mustclose(wq->wq_logrectype)
                    || logdata_groupsize > 100)
                {
                    logfile_logdata_close(wqinfo->wqi_logfile, close_cd);
                    logdata_groupsize = 0;
                    logdata_mustclose = TRUE;
                } else {
                    logdata_mustclose = FALSE;
                }

                close_cd = wq->wq_cd;

                switch (wq->wq_logrectype) {
                    case DBE_LOGREC_FLUSHTODISK:
                        /* ignore this log record */
                        ss_dassert(wq->wq_flushtodisk);
                        ss_dassert(wq->wq_logdatalen == 0);
                        break;
                    case DBE_LOGREC_HSBG2_SAVELOGPOS:
                        rc = DBE_RC_SUCC;
                        last_logfnum = wqinfo->wqi_logfile->lf_catchup_logpos.lp_logfnum;
                        last_daddr   = wqinfo->wqi_logfile->lf_catchup_logpos.lp_daddr;
                        last_bufpos  = wqinfo->wqi_logfile->lf_catchup_logpos.lp_bufpos;

                        dbe_catchup_savedlogpos_setpos(
                            wq->wq_logdata,
                            last_logfnum,
                            last_daddr,
                            last_bufpos);
                        dbe_catchup_savedlogpos_done(wq->wq_logdata);
                        wq->wq_logdata = NULL;
                        break;
                    default:
                        /*
                        last_logfnum = wqinfo->wqi_logfile->lf_logfnum;
                        last_daddr = wqinfo->wqi_logfile->lf_lp.lp_daddr;
                        last_bufpos = wqinfo->wqi_logfile->lf_lp.lp_bufpos;
                        */
                        rc = logfile_putdata_nomutex_splitif(
                                wqinfo->wqi_logfile,
                                wq->wq_cd,
                                wq->wq_logrectype,
                                wq->wq_trxid,
                                wq->wq_logdata,
                                wq->wq_logdatalen,
                                wq->wq_header_buf,
                                wq->wq_header_buf_len,
                                wq->wq_relid_or_originaldatalen,
                                FALSE,
                                wq->wq_split_hsbqueue_force,
                                wq->wq_p_logdatalenwritten != NULL
                                    ? wq->wq_p_logdatalenwritten
                                    : &logdatalenwritten,
                                wq->wq_p_splitlog != NULL
                                    ? wq->wq_p_splitlog
                                    : &splitlog);
                        break;
                }

                if (rc != DBE_RC_SUCC) {
                    ss_pprintf_1(("logf_writequeue_flushthr:ERROR, rc=%d\n", rc));
                    break;
                }

                if (wq->wq_p_rc != NULL) {
                    ss_dassert(wq->wq_iswaiting);
                    ss_dassert(*wq->wq_p_rc == 0xffff);
                    *wq->wq_p_rc = DBE_RC_SUCC;
                }

                if (wq->wq_next == NULL || logdata_mustclose) {
                    logfile_logdata_close(wqinfo->wqi_logfile, wq->wq_cd);
                    logdata_groupsize = 0;
                }

                logf_writequeue_free(wq);
                wq_last = wq;
            }

            if (rc == DBE_RC_SUCC) {
                if (flush) {
                    ss_pprintf_1(("logf_writequeue_flushthr:FLUSH:call logfile_writebuf\n"));
                    wqinfo->wqi_logfile->lf_lazyflush_do = FALSE;
                    rc = logfile_writebuf(wqinfo->wqi_logfile, TRUE, FALSE);
                    if (rc != DBE_RC_SUCC) {
                        ss_pprintf_1(("logf_writequeue_flushthr:ERROR, rc=%d\n", rc));
                    }
                }
            }

            if (rc != DBE_RC_SUCC) {
                /* Error during log write. Mark error codes to callers.
                 * Go through the full list because we may have stopped
                 * in the middle.
                 */
                for (wq = wq_first, qlen = 0; wq != NULL; wq = wq->wq_next, qlen++) {
                    if (wq->wq_p_rc != NULL) {
                        ss_dassert(wq->wq_iswaiting);
                        ss_dassert(*wq->wq_p_rc == 0xffff);
                        *wq->wq_p_rc = rc;
                    }
                    logf_writequeue_free(wq);
                    wq_last = wq;
                }
            }

            logfile_exit_mutex2(wqinfo->wqi_logfile);

            ss_pprintf_1(("logf_writequeue_flushthr:wakeupall flushmeswaitlist\n"));
            su_meswaitlist_wakeupall(flushmeswaitlist);

            if (wq_last != NULL) {

                /* Add all records in current batch to the free list.
                 */
            
                /* Add records to free list. */
                SsFlatMutexLock(wqinfo->wqi_freequeue_mutex);

                wq_last->wq_next = wqinfo->wqi_freequeue;
                wqinfo->wqi_freequeue = wq_first;

                SsFlatMutexUnlock(wqinfo->wqi_freequeue_mutex);

                /* Update counters. */
                SsFlatMutexLock(wqinfo->wqi_writequeue_mutex);

                wqinfo->wqi_npendingbytes -= written_now;
                ss_dassert(wqinfo->wqi_npendingbytes >= 0);
                SS_PMON_SET(SS_PMON_LOGWRITEQUEUEPENDINGBYTES, wqinfo->wqi_npendingbytes);

                wqinfo->wqi_nrecords -= qlen;
                ss_dassert(wqinfo->wqi_nrecords >= 0);
                ss_rc_dassert(qlen < 3*wqinfo->wqi_logfile->lf_maxwritequeuerecords, qlen);
                SS_PMON_SET(SS_PMON_LOGWRITEQUEUERECORDS, wqinfo->wqi_nrecords);

                SsFlatMutexUnlock(wqinfo->wqi_writequeue_mutex);
            }

            ss_pprintf_1(("logf_writequeue_flushthr:new wqinfo->wqi_nrecords=%d, qlen=%d, pendingbytes=%ld\n",
                            wqinfo->wqi_nrecords, qlen, wqinfo->wqi_npendingbytes));

            if (rc == DBE_RC_SUCC && !flush && wqinfo->wqi_logfile->lf_lazyflush_do) {
                logfile_enter_mutex(wqinfo->wqi_logfile);
                ss_pprintf_1(("logf_writequeue_flushthr:LAZY FLUSH:call logfile_writebuf\n"));
                wqinfo->wqi_logfile->lf_lazyflush_do = FALSE;
                rc = logfile_writebuf(wqinfo->wqi_logfile, TRUE, FALSE);
                if (rc != DBE_RC_SUCC) {
                    ss_pprintf_1(("logf_writequeue_flushthr:lazy flush ERROR, rc=%d\n", rc));
                }
                logfile_exit_mutex2(wqinfo->wqi_logfile);
            }

            /* All records are not written. Check if we in shutdown.
             */
            if (wqinfo->wqi_done) {
                ss_pprintf_1(("logf_writequeue_flushthr:logf_done\n"));
                ss_dassert(wqinfo->wqi_writequeue_first == NULL);
                break;
            }
            su_profile_stop("logf_writequeue_flushthr");

        }

        wqinfo->wqi_writeactive = FALSE;

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}

#endif /* DBE_GROUPCOMMIT_QUEUE */

#ifdef SS_HSBG2
/*##**********************************************************************\
 *
 *              dbe_logfile_transform_init
 *
 * Creates a logfile object for catchup and recovery
 *
 * Parameters :
 *
 *      cfg - in, use
 *              pointer to dbe configuration object
 *
 *      counter - in out, hold
 *              pointer to counter object
 *
 *      newdb - in
 *          TRUE when database has just been created then there MUST NOT
 *              be a log file before
 *          FALSE when the logfile is opened to an already existing
 *              database
 *
 *      dbcreatime - in
 *              database creation time
 *
 * Return value - give :
 *      pointer to created logfile object
 *      or NULL if logging is not enabled in configuration
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_logfile_t* dbe_logfile_transform_init(
#ifdef SS_HSBG2
        dbe_hsbg2_t *hsbsvc,
        dbe_log_instancetype_t instancetype,
        void* instance_ctx,
#endif /* SS_HSBG2 */
        dbe_cfg_t* cfg,
        dbe_counter_t* counter,
        bool newdb __attribute__ ((unused)),
        ulong dbcreatime)
{
        dbe_logfile_t* logfile;

        logfile = SSMEM_NEW(dbe_logfile_t);

        logfile->lf_hsbld = NULL;

        logfile->lf_lbm = dbe_lbm_init();
        logfile->lf_hsbsvc = hsbsvc;

        dbe_cfg_getlogblocksize(cfg, &logfile->lf_bufsize);
        dbe_cfg_getlogmaxwritequeuerecords(cfg, &logfile->lf_maxwritequeuerecords);
        dbe_cfg_getlogmaxwritequeuebytes(cfg, &logfile->lf_maxwritequeuebytes);
        dbe_cfg_getlogwritequeueflushlimit(cfg, &logfile->lf_writequeueflushlimit);
        dbe_cfg_getlogdelaymeswait(cfg, &logfile->lf_delaymeswait);
        dbe_cfg_getlogextendincr(cfg, &logfile->lf_lazyflush_extendincrement);

        logfile->lf_writebuffer_maxbytes = 0;
        logfile->lf_writebuffer_nbytes = 0;
        logfile->lf_writebuffer = NULL;
        logfile->lf_writebuffer_address = -1;

        logfile->lf_openflags = SS_BF_SEQUENTIAL | SS_BF_FLUSH_AFTERWRITE;
        logfile->lf_errorfunc = (void (*)(void*))0;
        logfile->lf_errorctx = NULL;
        logfile->lf_errorflag = FALSE;
        logfile->lf_bufdatasize = logfile->lf_bufsize -
            (2 * sizeof(ss_uint2_t));
        logfile->lf_lp.lp_bufpos = 0;

#ifdef SS_HSBG2

        logfile->lf_nondurable_commit = FALSE;
        logfile->lf_idlehsbdurable = DBE_LOGFILE_IDLEHSBDURABLE_DISABLE;

        DBE_CATCHUP_LOGPOS_SET_NULL(logfile->lf_catchup_logpos);
        /* current log position */
        logfile->lf_dbg_nbuffers_involved = 0;
#endif /* SS_HSBG2 */
        logfile->lf_hsbbuf =
                dbe_lbm_getnext_hsbbuffer(logfile->lf_lbm,
                                          NULL,
                                          logfile->lf_bufsize);
#ifdef IO_OPT
        logfile->lf_abuffer = dbe_hsbbuf_get_alogbuf(logfile->lf_hsbbuf);
#else
        logfile->lf_buffer = dbe_hsbbuf_get_logbuf(logfile->lf_hsbbuf);
#endif

        logfile->lf_wrmode = DBE_LOGWR_OVER;
#ifdef DBE_GROUPCOMMIT
        logfile->lf_writemutex = SsSemCreateLocal(SS_SEMNUM_DBE_LOG_GATE);
#endif

#if defined(SS_MT) && defined(DBE_LAZYLOG_OPT)
        logfile->lf_groupcommitdelay = 0;
        logfile->lf_flushthreadp = FALSE;

#endif /* SS_MT && DBE_LAZYLOG_OPT */

#if 0
        logfile->lf_lazyflushflag = FALSE;
#endif
        logfile->lf_groupcommitdelay = 0L;
        logfile->lf_pingpong_incr = 0L;
        logfile->lf_lastblock = 0;
        logfile->lf_logdir = NULL;
        logfile->lf_nametemplate = NULL;
        logfile->lf_digittemplate = '\0';
        logfile->lf_minsplitsize = 0L;
        logfile->lf_counter = counter;
        logfile->lf_transform = TRUE;

        logfile->lf_instance_ctx = instance_ctx;
        logfile->lf_instancetype = instancetype;
        logfile->lf_dbcreatime = dbcreatime;

        logfile->lf_logfnum = dbe_counter_getlogfnum(logfile->lf_counter);

        logfile->lf_bfile = NULL;
        logfile->lf_lp.lp_daddr = 0; /* something else is needed also */
        logfile->lf_flushflag = FALSE;
        logfile->lf_filewritecnt = 0;
        logfile->lf_flushed = FALSE;

#ifdef DBE_GROUPCOMMIT_QUEUE
        logfile->lf_groupcommitqueue  = FALSE;
        logfile->lf_lazyflush_threadp = FALSE;
        logfile->lf_lazyflush_wait    = NULL;
        logfile->lf_lazyflush_extendfile = FALSE;
        logfile->lf_lazyflush_request = FALSE;
        logfile->lf_lazyflush_do      = FALSE;
        logfile->lf_groupcommit_flush_queue = TRUE;

#endif /* DBE_GROUPCOMMIT_QUEUE */

        return (logfile);
}
#endif /* SS_HSBG2 */

/*##**********************************************************************\
 *
 *              dbe_logfile_init
 *
 * Creates a logfile object
 *
 * Parameters :
 *
 *      cfg - in, use
 *              pointer to dbe configuration object
 *
 *      counter - in out, hold
 *              pointer to counter object
 *
 *      newdb - in
 *          TRUE when database has just been created then there MUST NOT
 *              be a log file before
 *          FALSE when the logfile is opened to an already existing
 *              database
 *
 *      dbcreatime - in
 *              database creation time
 *
 * Return value - give :
 *      pointer to created logfile object
 *      or NULL if logging is not enabled in configuration
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_logfile_t* dbe_logfile_init(
#ifdef SS_HSBG2
        dbe_hsbg2_t *hsbsvc,
        dbe_log_instancetype_t instancetype,
#endif /* SS_HSBG2 */
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_counter_t* counter,
        bool newdb,
        ulong dbcreatime,
        su_cipher_t* cipher)
{
        dbe_logfile_t* logfile;
        ulong ul;
        size_t bytesread;
        dbe_ret_t rc;
        int wrmode_tmp;
        bool logenabled;
        int writeflushmode;
        bool found;
        bool b;
#ifdef IO_OPT
        bool                directio;
        dbe_aligned_buf_t*  ab;
#endif
        bool blocksize_changed = FALSE;
        ss_int8_t fsize_i8;
        bool old_log_exists;
        void* instance_ctx = NULL;
        dbe_cryptoparams_t* cp = rs_sysi_getcryptopar(cd);

        dbe_cfg_getlogenabled(cfg, &logenabled);
        if (!logenabled && instancetype != DBE_LOG_INSTANCE_LOGGING_HSB) {
            return (NULL);
        }

        logfile = SSMEM_NEW(dbe_logfile_t);

#ifdef SS_HSBG2
        logfile->lf_idlehsbdurable = DBE_LOGFILE_IDLEHSBDURABLE_ON;
        logfile->lf_lbm = dbe_lbm_init();
        logfile->lf_hsbsvc = hsbsvc;
        logfile->lf_transform = FALSE;
        logfile->lf_instance_ctx = instance_ctx;
        logfile->lf_instancetype = instancetype;
        logfile->lf_hsbld = NULL;
#endif /* SS_HSBG2 */

        dbe_cfg_getlogblocksize(cfg, &logfile->lf_bufsize);
        dbe_cfg_getlogmaxwritequeuerecords(cfg, &logfile->lf_maxwritequeuerecords);
        dbe_cfg_getlogmaxwritequeuebytes(cfg, &logfile->lf_maxwritequeuebytes);
        dbe_cfg_getlogwritequeueflushlimit(cfg, &logfile->lf_writequeueflushlimit);
        dbe_cfg_getlogdelaymeswait(cfg, &logfile->lf_delaymeswait);
        dbe_cfg_getlogextendincr(cfg, &logfile->lf_lazyflush_extendincrement);
        dbe_cfg_getlogwritebuffersize(cfg, &logfile->lf_writebuffer_maxbytes);

        /* Make sure lf_writebuffer_maxbytes is aligned with lf_bufsize. */
        logfile->lf_writebuffer_maxbytes = 
            (logfile->lf_writebuffer_maxbytes / logfile->lf_bufsize) * logfile->lf_bufsize;

        if (logfile->lf_writebuffer_maxbytes < logfile->lf_bufsize) {
            logfile->lf_writebuffer_maxbytes = logfile->lf_bufsize;
        }

        logfile->lf_writebuffer_nbytes = 0;
#ifdef IO_OPT
        ab = dbe_ab_init(logfile->lf_writebuffer_maxbytes, SS_DIRECTIO_ALIGNMENT);
        logfile->lf_writeabuffer = ab;
        logfile->lf_writebuffer = ab->ab_buf;
#else
        logfile->lf_writebuffer = SsMemAlloc(logfile->lf_writebuffer_maxbytes);
#endif
        logfile->lf_writebuffer_address = -1;

        logfile->lf_openflags = SS_BF_SEQUENTIAL;

        dbe_cfg_getlogfileflush(cfg, &b);
        if (!b) {
            logfile->lf_openflags |= SS_BF_NOFLUSH;
        }
        
        dbe_cfg_getlogsyncwrite(cfg, &b);
        if (b) {
            logfile->lf_openflags |= SS_BF_SYNCWRITE;
        }

#ifdef IO_OPT
        dbe_cfg_getlogdirectio(cfg, &directio);
        if (directio) {
            logfile->lf_openflags |= SS_BF_DIRECTIO;
        }
#endif /* IO_OPT */

        if (!logenabled) {
            ss_dassert(instancetype == DBE_LOG_INSTANCE_LOGGING_HSB);
            logfile->lf_openflags |= SS_BF_DLSIZEONLY|SS_BF_DISKLESS;
        }

        found = dbe_cfg_getwriteflushmode(cfg, &writeflushmode);
        switch (writeflushmode) {
            case SS_BFLUSH_NORMAL:
                break;
            case SS_BFLUSH_BEFOREREAD:
                logfile->lf_openflags |= SS_BF_FLUSH_BEFOREREAD;
                break;
            case SS_BFLUSH_AFTERWRITE:
                logfile->lf_openflags |= SS_BF_FLUSH_AFTERWRITE;
                break;
            default:
                ss_rc_derror(writeflushmode);
                break;
        }
        logfile->lf_errorfunc = (void (*)(void*))0;
        logfile->lf_errorctx = NULL;
        logfile->lf_errorflag = FALSE;
        logfile->lf_bufdatasize = logfile->lf_bufsize -
            (2 * sizeof(ss_uint2_t));
        logfile->lf_lp.lp_bufpos = 0;
        logfile->lf_cipher = cipher;
        if (cp != NULL) {
            logfile->lf_encrypt = dbe_crypt_getencrypt(cp);
            logfile->lf_decrypt = dbe_crypt_getdecrypt(cp);
        } else {
            ss_dassert(logfile->lf_cipher == NULL);
            logfile->lf_encrypt = NULL;
            logfile->lf_decrypt = NULL;
        }

#ifdef SS_HSBG2
        logfile->lf_nondurable_commit = FALSE;
        logfile->lf_dbg_nbuffers_involved = 0;
        /* current log position */
        DBE_CATCHUP_LOGPOS_SET_NULL(logfile->lf_catchup_logpos);
        logfile->lf_hsbbuf =
                dbe_lbm_getnext_hsbbuffer(logfile->lf_lbm,
                                          NULL,
                                          logfile->lf_bufsize);
#ifdef IO_OPT
        logfile->lf_abuffer = dbe_hsbbuf_get_alogbuf(logfile->lf_hsbbuf);
#else
        logfile->lf_buffer = dbe_hsbbuf_get_logbuf(logfile->lf_hsbbuf);
#endif /* IO_OPT */

#else /* SS_HSBG2 */
#ifdef IO_OPT
        logfile->lf_abuffer = dbe_alb_init(logfile->lf_bufsize);
        ss_dassert(DBE_LB_ALIGNMENT((logfile->lf_buffer->alb_buffer),
                                   SS_DIRECTIO_ALIGNMENT));

#else
        logfile->lf_buffer = dbe_lb_init(logfile->lf_bufsize);
#endif /* IO_OPT */
#endif /* SS_HSBG2 */

        dbe_cfg_getlogwritemode(cfg, &wrmode_tmp);
        switch (wrmode_tmp) {
            case DBE_LOGWR_ONCE:
            case DBE_LOGWR_OVER:
            case DBE_LOGWR_LAZY:
#ifdef SS_DEBUG
                {
                    ui_msg_message(LOG_MSG_USING_WRITE_MODE_D, wrmode_tmp);
                }
#endif /* SS_DEBUG */
            case DBE_LOGWR_PINGPONG:
                logfile->lf_wrmode = (dbe_logwrmode_t)wrmode_tmp;
                break;
            default:
                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_ILLLOGCONF_SSD,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGWRITEMODE,
                    wrmode_tmp);
        }

#ifdef DBE_GROUPCOMMIT
        logfile->lf_writemutex = SsSemCreateLocal(SS_SEMNUM_DBE_LOG_GATE);
#else /* DBE_GROUPCOMMIT */

        logfile->lf_mutex = SsSemCreateLocal(SS_SEMNUM_DBE_LOG);
        logfile->lf_wait = SsMesCreateLocal();
        logfile->lf_nwait = 0;
        logfile->lf_lazyflushflag = FALSE;
#endif /* DBE_GROUPCOMMIT */

#ifdef DBE_LAZYLOG_OPT
        logfile->lf_flushthreadp = FALSE;
        if (logfile->lf_wrmode == DBE_LOGWR_LAZY) {
            dbe_cfg_getlogcommitmaxwait(
                        cfg,
                        &logfile->lf_groupcommitdelay);
#ifdef SS_MT
            {
                SsThreadT* thr;
                ss_pprintf_2(("dbe_logfile_init:Start a thread that flushes the log\n"));
                thr = SsThrInitParam(dbe_logfile_flushthread, "dbe_logfile_flushthread", 24 * 1024, logfile);
                SsThrEnable(thr);
            }
#else /* SS_MT */
            if (logfile->lf_groupcommitdelay != 0) {
                /* !!!!! This message is not correct for this error !!!!! */
                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_ILLLOGCONF_SSD,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGCOMMITMAXWAIT,
                    logfile->lf_groupcommitdelay);
            }
#endif /* SS_MT */
        } else {
            logfile->lf_groupcommitdelay = 0L;
        }
#endif /* DBE_LAZYLOG_OPT */
#ifndef DBE_GROUPCOMMIT
        if (logfile->lf_wrmode == DBE_LOGWR_ONCE) {
            dbe_cfg_getlogcommitmaxwait(
                        cfg,
                        &logfile->lf_groupcommitdelay);
        } else {
            logfile->lf_groupcommitdelay = 0L;
        }
#endif /* !DBE_GROUPCOMMIT */
        logfile->lf_pingpong_incr = 0L;
        logfile->lf_lastblock = 0;
        dbe_cfg_getlogdir(
                    cfg,
                    &logfile->lf_logdir);
        dbe_cfg_getlogfilenametemplate(
                    cfg,
                    &logfile->lf_nametemplate);
        dbe_cfg_getlogdigittemplate(
                    cfg,
                    &logfile->lf_digittemplate);
        dbe_cfg_getlogfileminsplitsize(
                    cfg,
                    &ul);
        logfile->lf_minsplitsize = ul / logfile->lf_bufsize;
        logfile->lf_counter = counter;

        logfile->lf_dbcreatime = dbcreatime;

        if (!logenabled) {
            logfile->lf_logfnum = dbe_counter_inclogfnum(logfile->lf_counter);
        } else {
            logfile->lf_logfnum = dbe_counter_getlogfnum(logfile->lf_counter);
        }
 new_file_restart:;
        logfile->lf_filename = dbe_logfile_genname(
                                logfile->lf_logdir,
                                logfile->lf_nametemplate,
                                logfile->lf_logfnum,
                                logfile->lf_digittemplate);
        if (logfile->lf_filename == NULL) {

            su_informative_exit(
                __FILE__,
                __LINE__,
                DBE_ERR_ILLLOGFILETEMPLATE_SSSDD,
                SU_DBE_LOGSECTION,
                SU_DBE_LOGFILETEMPLATE,
                logfile->lf_nametemplate,
                DBE_LOGFILENAME_MINDIGITS,
                DBE_LOGFILENAME_MAXDIGITS
            );
        }
        old_log_exists = FALSE;
        if (SsFExist(logfile->lf_filename)) {
            fsize_i8 = SsFSizeAsInt8(logfile->lf_filename);
            if (!SsInt8Is0(fsize_i8)) {
                old_log_exists = TRUE;
            }
        }
        if (old_log_exists) {
            ss_int8_t tmp_i8;
            su_daddr_t fsize_blocks;

            if (blocksize_changed) {
                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_OLDLOGFILE_S,
                    logfile->lf_filename);
            }
            SsInt8SetUint4(&tmp_i8, logfile->lf_bufsize);
            SsInt8DivideByInt8(&fsize_i8, fsize_i8, tmp_i8);
            fsize_blocks = SsInt8GetLeastSignificantUint4(fsize_i8);
            if (fsize_blocks > 0) {
                ss_byte_t* p;
                loghdrdata_t hdrdata;
                /* could be same Blocksize! check it out */
#ifdef IO_OPT
                dbe_alogbuf_t* tmp_alogbuf = dbe_alb_init(logfile->lf_bufsize);
                ss_dassert(DBE_LB_ALIGNMENT((tmp_alogbuf->alb_buffer),
                                           SS_DIRECTIO_ALIGNMENT));

#else
                dbe_logbuf_t* tmp_logbuf = dbe_lb_init(logfile->lf_bufsize);
#endif
                logfile->lf_bfile = SsBOpen(
                                        logfile->lf_filename,
                                        logfile->lf_openflags,
                                        logfile->lf_bufsize);
#ifdef IO_OPT
                SsBReadPages(
                        logfile->lf_bfile,
                        0,
                        logfile->lf_bufsize,
                        tmp_alogbuf->alb_buffer,
                        1);

                if (logfile->lf_cipher != NULL) {
                    logfile->lf_decrypt(logfile->lf_cipher, 0,
                                        (char*)tmp_alogbuf->alb_buffer,
                                        1, logfile->lf_bufsize);
                }
                p = DBE_LB_DATA(tmp_alogbuf->alb_buffer);
#else
                SsBReadPages(
                        logfile->lf_bfile,
                        0,
                        logfile->lf_bufsize,
                        tmp_logbuf,
                        1);

                if (logfile->lf_cipher != NULL) {
                    logfile->lf_decrypt(logfile->lf_cipher, 0,
                                        (char*)tmp_logbuf,
                                        1, logfile->lf_bufsize);
                }
                p = DBE_LB_DATA(tmp_logbuf);
#endif
                if (*p != DBE_LOGREC_HEADER) {

                    su_emergency_exit(
                        __FILE__,
                        __LINE__,
                        DBE_ERR_LOGFILE_CORRUPT_S,
                        logfile->lf_filename);
                    /* NOTREACHED */
                }
                ss_dassert(*p == DBE_LOGREC_HEADER);
                p++;
                hdrdata.lh_logfnum = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(hdrdata.lh_logfnum);
                hdrdata.lh_cpnum = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(hdrdata.lh_cpnum);
                hdrdata.lh_blocksize = SS_UINT4_LOADFROMDISK(p);
#ifdef IO_OPT
                dbe_alb_done(tmp_alogbuf);
#else
                dbe_lb_done(tmp_logbuf);
#endif
                if (hdrdata.lh_blocksize != logfile->lf_bufsize) {
                    SsBClose(logfile->lf_bfile);
                    SsMemFree(logfile->lf_filename);
                    logfile->lf_logfnum =
                        dbe_counter_inclogfnum(
                            logfile->lf_counter);
                    blocksize_changed = TRUE;
                    goto new_file_restart;
                }
            } else {
                /* Because file size is > 0 and less than current blocksize
                 * the blocksize has inevitably changed!
                 */
                SsMemFree(logfile->lf_filename);
                logfile->lf_logfnum =
                    dbe_counter_inclogfnum(
                            logfile->lf_counter);
                blocksize_changed = TRUE;
                goto new_file_restart;
            }
        } else {
            logfile->lf_bfile =
                    SsBOpen(logfile->lf_filename,
                            logfile->lf_openflags,
                            logfile->lf_bufsize);
        }

        if (logfile->lf_bfile == NULL) {
            su_informative_exit(
                __FILE__,
                __LINE__,
                DBE_ERR_CANTOPENLOG_SSSSS,
                logfile->lf_filename,
                SU_DBE_LOGSECTION,
                SU_DBE_LOGFILETEMPLATE,
                SU_DBE_LOGSECTION,
                SU_DBE_LOGDIR);
        }
        logfile_enter_mutex(logfile);
        logfile->lf_lp.lp_daddr = logfile_getsize(logfile);
        logfile->lf_flushflag = FALSE;
        logfile->lf_filewritecnt = 0;
        logfile->lf_flushed = FALSE;
        if (logfile->lf_lp.lp_daddr == 0L) {
            rc = logfile_putheader(logfile, 0L);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
        } else {
            /* If the logfile already exists and database is new,
             * abort immediately!
             */
            if (newdb) {

                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_OLDLOGFILE_S,
                    logfile->lf_filename);
            }
#ifdef IO_OPT
            rc = logfile_read(
                    logfile,
                    logfile->lf_lp.lp_daddr - 1,
                    logfile->lf_abuffer->alb_buffer,
                    logfile->lf_bufsize,
                    &bytesread);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
            logfile->lf_lastblock =
                dbe_lb_getblocknumber(logfile->lf_abuffer->alb_buffer);
            dbe_lb_incblock(logfile->lf_abuffer->alb_buffer, logfile->lf_bufsize);
            memset(DBE_LB_DATA(logfile->lf_abuffer->alb_buffer),
                DBE_LOGREC_NOP,
                logfile->lf_bufdatasize);
#else
            rc = logfile_read(
                    logfile,
                    logfile->lf_lp.lp_daddr - 1,
                    logfile->lf_buffer,
                    logfile->lf_bufsize,
                    &bytesread);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
            logfile->lf_lastblock =
                dbe_lb_getblocknumber(logfile->lf_buffer);
            dbe_lb_incblock(logfile->lf_buffer, logfile->lf_bufsize);
            memset(DBE_LB_DATA(logfile->lf_buffer),
                DBE_LOGREC_NOP,
                logfile->lf_bufdatasize);
#endif
        }

#ifdef DBE_GROUPCOMMIT_QUEUE

        logfile->lf_lazyflush_threadp = FALSE;
        logfile->lf_lazyflush_wait    = NULL;
        logfile->lf_lazyflush_extendfile = FALSE;
        logfile->lf_lazyflush_request = FALSE;
        logfile->lf_lazyflush_do      = FALSE;

        logfile->lf_groupcommit_flush_queue = TRUE;

        dbe_cfg_getloggroupcommitqueue(
            cfg,
            &logfile->lf_groupcommitqueue);
        if (logfile->lf_groupcommitqueue) {
            SsThreadT* thr;

            logfile->lf_lazyflush_wait = SsMesCreateLocal();
            logfile->lf_lazyflush_extendfile = FALSE;

            logfile->lf_wqinfo.wqi_logfile = logfile;
            SsFlatMutexInit(&logfile->lf_wqinfo.wqi_writequeue_mutex, SS_SEMNUM_DBE_LOGWRITEQUEUE);
            logfile->lf_wqinfo.wqi_writequeue_first = NULL;
            logfile->lf_wqinfo.wqi_writequeue_last = NULL;
            logfile->lf_wqinfo.wqi_nrecords = 0;
            logfile->lf_wqinfo.wqi_nbytes = 0;
            logfile->lf_wqinfo.wqi_npendingbytes = 0;
            SsFlatMutexInit(&logfile->lf_wqinfo.wqi_freequeue_mutex, SS_SEMNUM_DBE_LOGFREEQUEUE);
            logfile->lf_wqinfo.wqi_writequeue_mes = SsMesCreateLocal();
            logfile->lf_wqinfo.wqi_freequeue = NULL;
            logfile->lf_wqinfo.wqi_writequeue_meslist = su_meslist_init(&logfile->lf_wqinfo.wqi_writequeue_meslist_buf);
            logfile->lf_wqinfo.wqi_writequeue_meswaitlist = su_meswaitlist_init();
            logfile->lf_wqinfo.wqi_writequeue_flushmeswaitlist = su_meswaitlist_init();
            logfile->lf_wqinfo.wqi_writeactive = FALSE;
            logfile->lf_wqinfo.wqi_done = FALSE;

            ss_pprintf_2(("dbe_logfile_init:start logf_writequeue_flushthr\n"));
            thr = SsThrInitParam(
                    logf_writequeue_flushthr,
                    "logf_writequeue_flushthr",
                    128 * 1024,
                    &logfile->lf_wqinfo);
            SsThrEnable(thr);
            SsThrDone(thr);

            dbe_cfg_getlogrelaxedmaxdelay(
                        cfg,
                        &logfile->lf_lazyflush_delay);
            if (logfile->lf_lazyflush_delay > 0L) {

                /* -----------------------------------------------------
                 * we start the thread always even if Strict=Yes
                 * This is because user can define durability trx by trx
                 */
                ss_pprintf_2(("dbe_logfile_init:start logf_lazy_flushthr:delay %ld\n", logfile->lf_lazyflush_delay));
                thr = SsThrInitParam(
                        logf_lazy_flushthr,
                        "logf_lazy_flushthr",
                        128 * 1024,
                        logfile);
                SsThrEnable(thr);
                SsThrDone(thr);

            }

        }
#endif /* DBE_GROUPCOMMIT_QUEUE */

        logfile_exit_mutex(logfile);
        return (logfile);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_done
 *
 * Deletes a logfile object
 *
 * Parameters :
 *
 *      logfile - in, take
 *              pointer to logfile object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_logfile_done(dbe_logfile_t* logfile)
{
        logfile->lf_lazyflush_extendfile = FALSE;
        logfile->lf_lazyflush_extendincrement = 0;

#ifdef DBE_GROUPCOMMIT_QUEUE

        if (logfile->lf_groupcommitqueue) {
            logf_writequeue_t* wq;
            logf_writequeue_t* wq_next;

            logfile->lf_lazyflush_delay = 0L;
            if (logfile->lf_lazyflush_wait != NULL) {

                while(logfile->lf_lazyflush_threadp) {
                    SsMesSend(logfile->lf_lazyflush_wait);
                    SsThrSleep(100L);
                }
            }

            logfile->lf_wqinfo.wqi_done = TRUE;
            SsMesSend(logfile->lf_wqinfo.wqi_writequeue_mes);
            while (logfile->lf_wqinfo.wqi_writeactive) {
                ss_pprintf_2(("dbe_logfile_done:wait logf_writequeue_flushthr\n"));
                SsThrSleep(1000L);
                SsMesSend(logfile->lf_wqinfo.wqi_writequeue_mes);
            }

            SsMesFree(logfile->lf_wqinfo.wqi_writequeue_mes);
            SsFlatMutexDone(logfile->lf_wqinfo.wqi_freequeue_mutex);
            su_meslist_done(&logfile->lf_wqinfo.wqi_writequeue_meslist_buf);
            su_meswaitlist_done(logfile->lf_wqinfo.wqi_writequeue_meswaitlist);
            su_meswaitlist_done(logfile->lf_wqinfo.wqi_writequeue_flushmeswaitlist);
            if (!logfile->lf_transform) {
                ss_dassert(logfile->lf_wqinfo.wqi_writequeue_first == NULL);
                ss_dassert(logfile->lf_wqinfo.wqi_writequeue_last == NULL);
                SsFlatMutexDone(logfile->lf_wqinfo.wqi_writequeue_mutex);
            }
            wq = logfile->lf_wqinfo.wqi_freequeue;
            while (wq != NULL) {
                wq_next = wq->wq_next;
                SsMemFree(wq);
                wq = wq_next;
            }
            SsMesFree(logfile->lf_lazyflush_wait);
        }
#endif /* DBE_GROUPCOMMIT_QUEUE */

        ss_dassert(logfile != NULL);
#if defined(SS_MT) && defined(DBE_LAZYLOG_OPT)
        logfile->lf_groupcommitdelay = 0;
        while (logfile->lf_flushthreadp) {
            SsThrSleep(100L);
        }

#endif /* SS_MT && DBE_LAZYLOG_OPT */

#ifdef SS_HSBG2

        if (!logfile->lf_transform) {
            if (logfile->lf_lp.lp_bufpos != 0) {
                dbe_logfile_flush(logfile);
            }
            SsBClose(logfile->lf_bfile);
            SsMemFree(logfile->lf_logdir);
            SsMemFree(logfile->lf_nametemplate);
            SsMemFree(logfile->lf_filename);
        }
#else /* SS_HSBG2 */
        if (logfile->lf_lp.lp_bufpos != 0) {
            dbe_logfile_flush(logfile);
        }
        SsBClose(logfile->lf_bfile);
        SsMemFree(logfile->lf_logdir);
        SsMemFree(logfile->lf_nametemplate);
        SsMemFree(logfile->lf_filename);
#ifdef IO_OPT
        dbe_alb_done(logfile->lf_abuffer);
#else
        dbe_lb_done(logfile->lf_buffer);
#endif /* IO_OPT */
#endif /* SS_HSBG2 */

#ifdef DBE_GROUPCOMMIT
        SsSemFree(logfile->lf_writemutex);
#else /* DBE_GROUPCOMMIT */
        SsSemFree(logfile->lf_mutex);
        SsMesFree(logfile->lf_wait);
#endif /* DBE_GROUPCOMMIT */

#ifdef SS_HSBG2
        dbe_hsbbuf_done(logfile->lf_hsbbuf);
        dbe_lbm_done(logfile->lf_lbm);
#endif /* SS_HSBG2 */

        if (logfile->lf_writebuffer != NULL) {
#ifdef IO_OPT
            /* Replace every reference to writebuffer and writebuffer_addr
             * with dbe_aligned_buf_t */
            dbe_ab_done(logfile->lf_writeabuffer);
#else
            SsMemFree(logfile->lf_writebuffer);
#endif
        }

        SsMemFree(logfile);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_flush
 *
 * Forces the log file internal buffers to disk
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 * Return value :
 *      DBE_RC_SUCC when ok
 *      or error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_logfile_flush(
        dbe_logfile_t* logfile)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_logfile_flush\n"));

        logfile_enter_mutex(logfile);
        rc = logfile_writebuf(logfile, TRUE, TRUE);
        logfile_exit_mutex2(logfile);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_idleflush
 *
 * Writes block to disk if the block is dirty and the lf_writemode ==
 * DBE_LOGWR_LAZY
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 * Return value :
 *      DBE_RC_SUCC when ok
 *      or error code otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_logfile_idleflush(
        dbe_logfile_t* logfile)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_logfile_idleflush\n"));

        rc = DBE_RC_SUCC;

#ifdef SS_HSBG2
        if (logfile->lf_nondurable_commit && logfile->lf_idlehsbdurable == DBE_LOGFILE_IDLEHSBDURABLE_ON) {
            dbe_catchup_logpos_t local_durable_logpos;

            DBE_CATCHUP_LOGPOS_SET_NULL(local_durable_logpos);

            ss_dprintf_1(("dbe_logfile_idleflush:lf_nondurable_commit==TRUE\n"));
            rc = dbe_logfile_put_durable(logfile, NULL, HSB_ROLE_PRIMARY, local_durable_logpos);
            return(rc);
        }
#endif

#ifndef DBE_GROUPCOMMIT
        logfile_enter_mutex(logfile);
        if (logfile->lf_wrmode == DBE_LOGWR_LAZY) {
            if (logfile->lf_lazyflushflag) {
                rc = logfile_writebuf(logfile, FALSE, FALSE);
            }
            ss_dassert(!logfile->lf_lazyflushflag);
        }
        logfile_exit_mutex(logfile);
#endif /* !DBE_GROUPCOMMIT */
        return (rc);
}


dbe_ret_t dbe_logfile_flushtodisk(
        dbe_logfile_t* logfile)
{
        dbe_ret_t rc;
        bool use_thread = FALSE;

        rc = DBE_RC_SUCC;

        ss_pprintf_1(("dbe_logfile_flushtodisk\n"));

#ifdef DBE_GROUPCOMMIT_QUEUE
        /* set force flush flag */
        logfile_enter_mutex(logfile);
        if (logfile->lf_groupcommitqueue) {
            use_thread = TRUE;
            logfile->lf_lazyflush_request = FALSE;
            logfile->lf_lazyflush_do = TRUE;
        }
        logfile_exit_mutex2(logfile);

        if (use_thread) {

            rc = dbe_logfile_putdata_splitif(
                    logfile,
                    NULL,
                    DBE_LOGREC_FLUSHTODISK,
                    DBE_TRXID_NULL,
                    NULL,
                    0,
                    NULL,
                    NULL);

        } else
#endif
        {
            rc = dbe_logfile_flush(logfile);
        }

        return (rc);
}

void dbe_logfile_setidlehsbdurable(
        dbe_logfile_t* logfile,
        dbe_logfile_idlehsbdurable_t mode)
{
        ss_dprintf_1(("dbe_logfile_setidlehsbdurable:mode=%d\n", mode));

        if (logfile->lf_idlehsbdurable != DBE_LOGFILE_IDLEHSBDURABLE_DISABLE) {
            logfile->lf_idlehsbdurable = mode;
        }
}

void dbe_logfile_set_groupcommit_queue_flush(
        dbe_logfile_t* logfile)
{
#ifdef DBE_GROUPCOMMIT_QUEUE
        logfile->lf_groupcommit_flush_queue = TRUE;
#endif
}

static bool logfile_flushqueue(dbe_logrectype_t logrectype)
{
        switch (logrectype) {
            case DBE_LOGREC_REPLICATRXSTART:
            case DBE_LOGREC_REPLICASTMTSTART:
            case DBE_LOGREC_COMMITSTMT:
            case DBE_LOGREC_INCCTR:
                return(FALSE);
            default:
                return(TRUE);
        }
}

static void writequeuebuf_done(void* data)
{
        logf_writequeuebuf_t* wqb = data;
        logf_writequeue_t* wqx;

        wqx = wqb->wqb_first;
        while (wqx != NULL) {
            wqb->wqb_first = wqx->wq_next;
            logf_writequeue_free(wqx);
            SsMemFree(wqx);
            wqx = wqb->wqb_first;
        }
        SsMemFree(wqb);
}

/*#***********************************************************************\
 *
 *              logfile_initwritequeuitem
 *
 * Inits logfile writequeue item.
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      logrectype - in
 *              log record type
 *
 *      trxid - in
 *              transaction id or DBE_TRXID_NULL when none
 *
 *      logdata - in, use
 *              pointer to data buffer
 *
 *      logdatalen_or_relid - in
 *              data length in bytes or relid when the data is a vtuple
 *
 *      p_logdatalenwritten - out, use
 *              pointer to variable where the number of bytes written
 *          will be stored
 *
 *              p_splitlog - in out, use
 *          on input TRUE forces the log file to split
 *          on output TRUE means log file was split (either
 *          explicitly or implicitly) and FALSE when
 *          log file was split (input was FALSE also)
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
static logf_writequeue_t* logfile_initwritequeuitem(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalenwritten,
        bool* p_splitlog)
{
        logf_writequeue_t* wq;
        size_t logdatalen;

        ss_pprintf_2(("logfile_initwritequeuitem:get new queue entry\n"));

        SsFlatMutexLock(logfile->lf_wqinfo.wqi_freequeue_mutex);

        wq = logfile->lf_wqinfo.wqi_freequeue;
        if (wq == NULL) {
            SsFlatMutexUnlock(logfile->lf_wqinfo.wqi_freequeue_mutex);
            ss_pprintf_2(("logfile_initwritequeuitem:allocate a new queue entry\n"));
            wq = SSMEM_NEW(logf_writequeue_t);
            wq->wq_dyndata = NULL;
        } else {
            logfile->lf_wqinfo.wqi_freequeue = wq->wq_next;
            SsFlatMutexUnlock(logfile->lf_wqinfo.wqi_freequeue_mutex);
        }
        ss_pprintf_2(("logfile_initwritequeuitem:got it\n"));

        wq->wq_next = NULL;

        ss_debug(wq->wq_iswaiting = FALSE);

        wq->wq_split_hsbqueue_force = FALSE;

        wq->wq_cd = cd;
        wq->wq_logrectype = logrectype;
        wq->wq_trxid = trxid;
        wq->wq_p_logdatalenwritten = p_logdatalenwritten;
        wq->wq_p_splitlog = p_splitlog;
        wq->wq_islpid = FALSE;
        wq->wq_p_rc = NULL;

        logdata = dbe_logfile_getdatalenandlen(
                    logrectype,
                    logdata,
                    logdatalen_or_relid,
                    &logdatalen);

        if (logdatalen == 0) {
            if (logrectype == DBE_LOGREC_HSBG2_SAVELOGPOS) {
                wq->wq_logdata = logdata;
            } else {
                wq->wq_logdata = NULL;
            }
        } else {
            if (logdatalen < sizeof(wq->wq_databuf)) {
                ss_pprintf_2(("logfile_initwritequeuitem:logdatalen=%d, use buffer\n", logdatalen));
                wq->wq_logdata = wq->wq_databuf;
            } else {
                ss_pprintf_2(("logfile_initwritequeuitem:logdatalen=%d, allocate new buffer\n", logdatalen));
                wq->wq_dyndata = SsMemAlloc(logdatalen);
                wq->wq_logdata = wq->wq_dyndata;
            }
            memcpy(wq->wq_logdata, logdata, logdatalen);
        }

        wq->wq_relid_or_originaldatalen = logdatalen_or_relid;
        wq->wq_logdatalen = logdatalen;

        /* Get proper value for flush flag.
         */
        wq->wq_flushtodisk = logfile_mustflush(logfile, logrectype, logdata);

        logfile_putdatatobuffer(
            logfile,
            cd,
            logrectype,
            trxid,
            logdata,
            (size_t)logdatalen,
            logdatalen_or_relid,
            wq->wq_header_buf,
            &wq->wq_header_buf_len);

        return(wq);
}

/*#***********************************************************************\
 *
 *              logfile_putdata_splitif_writequeue
 *
 * Puts data to logfile
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      logrectype - in
 *              log record type
 *
 *      trxid - in
 *              transaction id or DBE_TRXID_NULL when none
 *
 *      logdata - in, use
 *              pointer to data buffer
 *
 *      logdatalen_or_relid - in
 *              data length in bytes or relid when the data is a vtuple
 *
 *      p_logdatalenwritten - out, use
 *              pointer to variable where the number of bytes written
 *          will be stored
 *
 *              p_splitlog - in out, use
 *          on input TRUE forces the log file to split
 *          on output TRUE means log file was split (either
 *          explicitly or implicitly) and FALSE when
 *          log file was split (input was FALSE also)
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t logfile_putdata_splitif_writequeue(
        dbe_logfile_t* logfile,
        logf_writequeuebuf_t* wqb,
        su_mes_t* mes,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalenwritten,
        bool* p_splitlog)
{
        bool flushqueue = FALSE;
        bool waitflush = FALSE;
        bool hsbwaitflush = FALSE;
        bool keepcd = FALSE;
        dbe_ret_t rc = DBE_RC_SUCC;
        bool wait_before_return = TRUE;
        bool take_new_lpid = FALSE;
        logf_writequeue_t* wqx;
        bool flushtodisk;
        su_profile_timer;

        SS_PUSHNAME("logfile_putdata_splitif_writequeue");

        su_profile_start;

        ss_pprintf_1(("logfile_putdata_splitif_writequeue:trxid=%ld, logrectype=%d, datalen_or_relid=%d\n",
            DBE_TRXID_GETLONG(trxid), logrectype, logdatalen_or_relid));

        if (logfile->lf_instancetype == DBE_LOG_INSTANCE_LOGGING_HSB) {
            switch (logrectype) {
                case DBE_LOGREC_HSBG2_DURABLE:
                case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
                case DBE_LOGREC_FLUSHTODISK:
                case DBE_LOGREC_ABORTSTMT:
                case DBE_LOGREC_ABORTTRX_INFO:
                case DBE_LOGREC_HSBG2_ABORTALL:
                case DBE_LOGREC_HSBG2_NEWSTATE:
                case DBE_LOGREC_CHECKPOINT_NEW:
                case DBE_LOGREC_INCSYSCTR:
                    break;

#ifdef SS_TC_CLIENT
                case DBE_LOGREC_COMMITTRX_INFO:
                    /* take_new_lpid = (cd != NULL); */
                    /* FALLTHROUGH */
#endif /* SS_TC_CLIENT */
                default:
                    /* Ask from hsb/cluster if we have space,
                     * eg. if (any) secondary is behind too much
                     */
                    dbe_hsbg2_getspace(logfile->lf_hsbsvc, 1);
                    break;
            }
        }

        wqx = wqb->wqb_last;

        /* Get proper value for flush flag.
         */
        if (wqx->wq_flushtodisk) {
            ss_pprintf_2(("logfile_putdata_splitif_writequeuebe_logfile_putdata_splitif:logfile_mustflush, FLUSH\n"));
            flushqueue = TRUE;
            waitflush = TRUE;
            flushtodisk = TRUE;
        } else {
            flushtodisk = FALSE;
        }

        if (p_logdatalenwritten != NULL || p_splitlog != NULL) {
            ss_pprintf_2(("logfile_putdata_splitif_writequeue:p_logdatalenwritten != NULL || p_splitlog != NULL, FLUSH\n"));
            flushqueue = TRUE;
            waitflush = TRUE;
            flushtodisk = TRUE;
        }

        /* Check if HSB needs a waitflush only if waitflush is not set yet.
         */
        if (!waitflush && logfile->lf_hsbsvc != NULL) {
            if (dbe_logdata_mustflush_logrectype(logrectype, logdata)) {
                ss_pprintf_2(("logfile_putdata_splitif_writequeue:dbe_logdata_mustflush_logrectype, FLUSH\n"));
                flushqueue = TRUE;
                waitflush = TRUE;
                hsbwaitflush = TRUE;
                flushtodisk = TRUE;
            } else {
                if (cd != NULL && rs_sysi_gethsbwaitmes(cd) != NULL) {
                    ss_pprintf_2(("logfile_putdata_splitif_writequeue:cd != NULL && rs_sysi_gethsbwaitmes(cd) != NULL, FLUSH\n"));
                    flushqueue = TRUE;
                    keepcd = TRUE;
                }
            }
        }

        if (cd != NULL) {
            dbe_ret_t* cd_p_rc;
            cd_p_rc = rs_sysi_getlogwaitrc(cd);
            if (cd_p_rc != NULL) {
                *cd_p_rc = DBE_RC_SUCC;
            }
        }

        SsFlatMutexLock(logfile->lf_wqinfo.wqi_writequeue_mutex);

        SS_PMON_ADD(SS_PMON_LOGWRITEQUEUEADD);

        logfile->lf_wqinfo.wqi_nrecords += wqb->wqb_len;
        ss_dassert(logfile->lf_wqinfo.wqi_nrecords >= 0);
        ss_rc_dassert(logfile->lf_wqinfo.wqi_nrecords < 3*logfile->lf_maxwritequeuerecords, logfile->lf_wqinfo.wqi_nrecords);
        SS_PMON_SET(SS_PMON_LOGWRITEQUEUERECORDS, logfile->lf_wqinfo.wqi_nrecords);
        ss_pprintf_2(("logfile_putdata_splitif_writequeue:logfile->lf_wqinfo.wqi_nrecords=%d\n", logfile->lf_wqinfo.wqi_nrecords));
        if (logfile->lf_wqinfo.wqi_nrecords >= logfile->lf_maxwritequeuerecords) {
            /* Over max limit, need to start waiting.
             */
            ss_pprintf_2(("logfile_putdata_splitif_writequeue:logfile->lf_wqinfo.wqi_nrecords=%d, wait FLUSH\n", logfile->lf_wqinfo.wqi_nrecords));
            flushqueue = TRUE;
            waitflush = TRUE;
            SS_PMON_ADD(SS_PMON_LOGMAXWRITEQUEUERECORDS);
        }

        /* Link record to the current queue.
         */
        if (logfile->lf_wqinfo.wqi_writequeue_first == NULL) {
            ss_dassert(logfile->lf_wqinfo.wqi_writequeue_last == NULL);
            logfile->lf_wqinfo.wqi_writequeue_first = wqb->wqb_first;
            logfile->lf_wqinfo.wqi_writequeue_last = wqb->wqb_last;
        } else {
            ss_dassert(logfile->lf_wqinfo.wqi_writequeue_first != NULL);
            logfile->lf_wqinfo.wqi_writequeue_last->wq_next = wqb->wqb_first;
            logfile->lf_wqinfo.wqi_writequeue_last = wqb->wqb_last;
        }

        ss_dassert(wqx->wq_next == NULL);

        logfile->lf_wqinfo.wqi_npendingbytes += wqb->wqb_logdatalen;
        ss_dassert(logfile->lf_wqinfo.wqi_npendingbytes >= 0);
        ss_pprintf_1(("logfile_putdata_splitif_writequeue:npendingbytes %ld\n", logfile->lf_wqinfo.wqi_npendingbytes));
        SS_PMON_SET(SS_PMON_LOGWRITEQUEUEPENDINGBYTES, logfile->lf_wqinfo.wqi_npendingbytes);

        if (logfile->lf_wqinfo.wqi_npendingbytes > logfile->lf_maxwritequeuebytes) {
            /*
             * here we should use task event waiting
             */
            ss_pprintf_1(("logfile_putdata_splitif_writequeue:WAITFLUSH:npendingbytes %ld\n", logfile->lf_wqinfo.wqi_npendingbytes));
            flushqueue = TRUE;
            waitflush = TRUE;
            SS_PMON_ADD(SS_PMON_LOGMAXWRITEQUEUEBYTES);
        }

        logfile->lf_wqinfo.wqi_nbytes += wqb->wqb_logdatalen;
        ss_dassert(logfile->lf_wqinfo.wqi_nbytes >= 0);
        SS_PMON_SET(SS_PMON_LOGWRITEQUEUEBYTES, logfile->lf_wqinfo.wqi_nbytes);
        if (logfile->lf_wqinfo.wqi_nbytes >= logfile->lf_writequeueflushlimit) {
            ss_pprintf_2(("logfile_putdata_splitif_writequeue:logfile->lf_wqinfo.wqi_nbytes=%ld, logfile->lf_writequeueflushlimit=%d, FLUSH\n",
                logfile->lf_wqinfo.wqi_nbytes, logfile->lf_writequeueflushlimit));
            flushqueue = TRUE;
        }
#ifdef SS_HSBG2

        if (!flushqueue) {
            flushqueue = logfile->lf_groupcommit_flush_queue;
            logfile->lf_groupcommit_flush_queue = FALSE;
        }

#endif
        if (flushqueue) {
            logfile->lf_wqinfo.wqi_nbytes = 0;
            SS_PMON_SET(SS_PMON_LOGWRITEQUEUEBYTES, logfile->lf_wqinfo.wqi_nbytes);
        }

#ifdef SS_TC_CLIENT
        if (take_new_lpid && flushqueue) {
            /* We should also check that 'cd' is Cluster Client.
             * If not then this is not nesessary.
             */
            ss_dassert(cd != NULL);
            /* take new lpid and set it to wq->xxx -->
             * so it can be set to hsb lohdata when it is created in logging thread.
             * set this lpid also to cd (it must be valid cd) and signal cd that
             * it has such lpid. This lpid is then send to client (in proli).
             */
            wqx->wq_lpid = dbe_catchup_logpos_get_newid(logfile->lf_counter);
            rs_sysi_setlpid_int8(cd, wqx->wq_lpid);
            wqx->wq_islpid = TRUE;
            ss_pprintf_2(("logfile_putdata_splitif_writequeue:new_lpid %ld\n", LPID_GETLONG(wqx->wq_lpid)));
        }
#endif /* SS_TC_CLIENT */

        if (waitflush) {
            ss_bassert(flushqueue);
            mes = su_meslist_mesinit(logfile->lf_wqinfo.wqi_writequeue_meslist);
            ss_pprintf_2(("logfile_putdata_splitif_writequeue:wait for log flusher\n"));
            if (flushtodisk) {
                su_meswaitlist_add(logfile->lf_wqinfo.wqi_writequeue_flushmeswaitlist, mes);
            } else {
                su_meswaitlist_add(logfile->lf_wqinfo.wqi_writequeue_meswaitlist, mes);
            }
            if (cd != NULL
                && rs_sysi_getlogwaitrc(cd) != NULL
                && logfile->lf_instancetype == DBE_LOG_INSTANCE_LOGGING_STANDALONE
                && logfile->lf_delaymeswait) 
            {
                wait_before_return = FALSE;
            }
            if (flushtodisk) {
                if (wait_before_return) {
                    wqx->wq_p_rc = &rc;
                } else {
                    wqx->wq_p_rc = rs_sysi_getlogwaitrc(cd);
                }
                ss_debug(*wqx->wq_p_rc = 0xffff);
            } else {
                wqx->wq_p_rc = NULL;
            }
            ss_debug(wqx->wq_iswaiting = TRUE);
        } else {
            wqx->wq_p_rc = NULL;
            if (!keepcd) {
                wqx->wq_cd = NULL;
            }
        }

        ss_beta(dbe_logrectype_writecount[logrectype]++;)

        SsFlatMutexUnlock(logfile->lf_wqinfo.wqi_writequeue_mutex);

        if (flushqueue || waitflush) {
            ss_pprintf_1(("logfile_putdata_splitif_writequeue:logrectype %s, flushqueue %d, waitflush %d\n",
                        dbe_logi_getrectypename(logrectype),
                        flushqueue, waitflush));
        }

        if (flushqueue) {
            ss_pprintf_1(("logfile_putdata_splitif_writequeue:wake flusher thread\n"));
            SsMesSend(logfile->lf_wqinfo.wqi_writequeue_mes);

            if (waitflush) {

                SS_PMON_ADD(SS_PMON_LOGWAITFLUSH);

                if (wait_before_return) {
                    su_profile_timer;
                    ss_pprintf_2(("logfile_putdata_splitif_writequeue:wait for flush\n"));
                    su_profile_start;
                    su_mes_wait(mes);
                    su_profile_stop("logfile_putdata_splitif_writequeue:flush wait");
                    ss_dassert(rc != 0xffff);
                    ss_pprintf_2(("logfile_putdata_splitif_writequeue:wakeup from flush\n"));
                    su_meslist_mesdone(logfile->lf_wqinfo.wqi_writequeue_meslist, mes);
                } else {
                    ss_pprintf_2(("logfile_putdata_splitif_writequeue:set wait mes to cd\n"));
                    rs_sysi_setlogwaitmes(cd, logfile->lf_wqinfo.wqi_writequeue_meslist, mes);
                }

                ss_beta(dbe_logrectype_waitflushqueuecount[logrectype]++;)
            }
        }

        ss_pprintf_2(("logfile_putdata_splitif_writequeue:rc=%d\n", rc));

        SS_POPNAME;
        su_profile_stop("logfile_putdata_splitif_writequeue");
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_putdata_splitif
 *
 * Puts data to logfile
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      logrectype - in
 *              log record type
 *
 *      trxid - in
 *              transaction id or DBE_TRXID_NULL when none
 *
 *      logdata - in, use
 *              pointer to data buffer
 *
 *      logdatalen_or_relid - in
 *              data length in bytes or relid when the data is a vtuple
 *
 *      p_logdatalenwritten - out, use
 *              pointer to variable where the number of bytes written
 *          will be stored
 *
 *              p_splitlog - in out, use
 *          on input TRUE forces the log file to split
 *          on output TRUE means log file was split (either
 *          explicitly or implicitly) and FALSE when
 *          log file was split (input was FALSE also)
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_logfile_putdata_splitif(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalenwritten,
        bool* p_splitlog)
{

#ifdef DBE_GROUPCOMMIT_QUEUE
        if (
#ifdef SS_HSBG2
            !logfile->lf_transform &&
#endif /* SS_HSBG2 */
            logfile->lf_groupcommitqueue)
        {
            logf_writequeue_t* wqx;
            su_mes_t* mes = NULL;
            dbe_ret_t rc;
            bool flushqueue;
            logf_writequeuebuf_t* wqb;
            logf_writequeuebuf_t  wqb_buf;

            SS_PUSHNAME("dbe_logfile_putdata_splitif");
            ss_pprintf_1(("dbe_logfile_putdata_splitif:trxid=%ld, logrectype=%s (%d), datalen_or_relid=%d\n",
                DBE_TRXID_GETLONG(trxid), dbe_logi_getrectypename(logrectype), logrectype, logdatalen_or_relid));
            ss_dassert(cd != NULL || DBE_TRXID_ISNULL(trxid) || logfile->lf_instancetype != DBE_LOG_INSTANCE_LOGGING_STANDALONE);

            wqx = logfile_initwritequeuitem(
                        logfile,
                        cd,
                        logrectype,
                        trxid,
                        logdata,
                        logdatalen_or_relid,
                        p_logdatalenwritten,
                        p_splitlog);

            if (cd == NULL || logfile->lf_instancetype != DBE_LOG_INSTANCE_LOGGING_STANDALONE) {
                ss_pprintf_2(("dbe_logfile_putdata_splitif:NULL cd or HSB, write to the log queue immediately\n"));
                wqb = &wqb_buf;
                wqb->wqb_first = wqx;
                wqb->wqb_last = wqx;
                wqb->wqb_logdatalen = wqx->wq_header_buf_len + wqx->wq_logdatalen;
                wqb->wqb_len = 1;
                flushqueue = TRUE;
            } else {
                ss_pprintf_2(("dbe_logfile_putdata_splitif:put into write queue in cd\n"));
                wqb = rs_sysi_getlogqueue(cd);
                if (wqb == NULL) {
                    wqb = SsMemCalloc(1, sizeof(logf_writequeuebuf_t));
                    rs_sysi_setlogqueue(cd, wqb, writequeuebuf_done);
                }
                if (wqb->wqb_first == NULL) {
                    wqb->wqb_first = wqx;
                    wqb->wqb_last = wqx;
                } else {
                    wqb->wqb_last->wq_next = wqx;
                    wqb->wqb_last = wqx;
                }
                wqb->wqb_len++;
                wqb->wqb_logdatalen += wqx->wq_header_buf_len + wqx->wq_logdatalen;
                flushqueue = wqx->wq_flushtodisk || logfile_flushqueue(logrectype);
            }

            if (flushqueue) {

                ss_pprintf_2(("dbe_logfile_putdata_splitif:write to queue\n"));
                rc = logfile_putdata_splitif_writequeue(
                        logfile,
                        wqb,
                        mes,
                        cd,
                        logrectype,
                        trxid,
                        logdata,
                        logdatalen_or_relid,
                        p_logdatalenwritten,
                        p_splitlog);

                wqb->wqb_first = NULL;
                wqb->wqb_last = NULL;
                wqb->wqb_logdatalen = 0;
                wqb->wqb_len = 0;

            } else {
                ss_pprintf_2(("dbe_logfile_putdata_splitif:record added to queue in cd\n"));
                rc = DBE_RC_SUCC;
            }

            ss_pprintf_2(("dbe_logfile_putdata_splitif:rc=%d\n", rc));

            SS_POPNAME;
            return (rc);
        } else

#endif /* DBE_GROUPCOMMIT_QUEUE */
        {
            dbe_ret_t rc;
            size_t logdatalenwritten;
            bool splitlog = FALSE;
            size_t logdatalen;
            char header_buf[MAX_HEADER_BUF];
            size_t header_buf_len;

            SS_PUSHNAME("dbe_logfile_putdata_splitif");

            if (p_logdatalenwritten == NULL) {
                p_logdatalenwritten = &logdatalenwritten;
            }
            if (p_splitlog == NULL) {
                p_splitlog = &splitlog;
            }

            logdata = dbe_logfile_getdatalenandlen(
                        logrectype,
                        logdata,
                        logdatalen_or_relid,
                        &logdatalen);

            logfile_putdatatobuffer(
                logfile,
                cd,
                logrectype,
                trxid,
                logdata,
                logdatalen,
                logdatalen_or_relid,
                header_buf,
                &header_buf_len);

            logfile_enter_mutex(logfile);
            rc = logfile_putdata_nomutex_splitif(
                    logfile,
                    cd,
                    logrectype,
                    trxid,
                    logdata,
                    logdatalen,
                    header_buf,
                    header_buf_len,
                    logdatalen_or_relid,
                    logfile_mustflush(logfile, logrectype, logdata),
                    FALSE,
                    p_logdatalenwritten,
                    p_splitlog);
            if (rc == DBE_RC_SUCC) {
                logfile_logdata_close(logfile, cd);
            }
            logfile_exit_mutex2(logfile);
            SS_POPNAME;
            return (rc);
        }
}

/*##**********************************************************************\
 *
 *              dbe_logfile_waitflushmes
 *
 * Wait for a flush to complete.
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      cd - in
 *              client data
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_logfile_waitflushmes(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd)
{
        dbe_ret_t rc;
        su_mes_t* mes;

        ss_dassert(cd != NULL);

        mes = rs_sysi_getlogwaitmes(cd);
        if (mes != NULL) {
            ss_pprintf_2(("dbe_logfile_waitflushmes:wait for flush\n"));
            su_mes_wait(mes);
            rc = *rs_sysi_getlogwaitrc(cd);
            ss_pprintf_2(("dbe_logfile_waitflushmes:wakeup from flush, rc = %d\n", rc));
            rs_sysi_removelogwaitmes(cd);
        } else {
            ss_pprintf_2(("dbe_logfile_waitflushmes:no need to wait for flush\n"));
            rc = DBE_RC_SUCC;
        }
        rs_sysi_setlogwaitrc(cd, NULL);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_putdata
 *
 * Puts data to logfile
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      logrectype - in
 *              log record type
 *
 *      trxid - in
 *              transaction id or DBE_TRXID_NULL when none
 *
 *      logdata - in, use
 *              pointer to data buffer
 *
 *      logdatalen_or_relid - in
 *              data length in bytes or relid when the data is a vtuple
 *
 *      p_logdatalenwritten - out, use
 *              pointer to variable where the number of bytes written
 *          will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_logfile_putdata(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalenwritten)
{
        dbe_ret_t rc;

        rc = dbe_logfile_putdata_splitif(
                logfile,
                cd,
                logrectype,
                trxid,
                logdata,
                logdatalen_or_relid,
                p_logdatalenwritten,
                NULL);

        return (rc);
}


/*##**********************************************************************\
 *
 *              dbe_logfile_put_durable
 *
 * Puts data LOGREC_HSBG2_DURABLE logrec to logfile
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_logfile_put_durable(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        hsb_role_t role,
        dbe_catchup_logpos_t local_durable_logpos)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        if (logfile->lf_hsbsvc != NULL) {
            char buf[DBE_LOGPOS_BINSIZE];
            char* p;

            ss_pprintf_1(("dbe_logfile_put_durable:DBE_LOGREC_HSBG2_DURABLE:durable logpos (%d,%s,%d,%d,%d)\n",
                           LOGPOS_DSDDD(local_durable_logpos)));

            /* write local durable logpos to buf */
            /* This not needed anymore because in dbe7logf we patch
             * current / correct log position to this logrec.
             * So the logpos parameter should be removed.
             */
            p = buf;
#ifdef HSB_LPID
            LPID_STORETODISK(p, local_durable_logpos.lp_id);
            p += sizeof(dbe_hsb_lpid_t);
            *p = role;
            p += 1;
#endif
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_logfnum);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_daddr);
            p += sizeof(ss_uint4_t);
            SS_UINT4_STORETODISK(p, local_durable_logpos.lp_bufpos);

            /* write DURABLE log record and buf to log */

            dbe_logfile_set_groupcommit_queue_flush(logfile);

            rc = dbe_logfile_putdata(
                        logfile,
                        cd,
                        DBE_LOGREC_HSBG2_DURABLE,
                        DBE_TRXID_NULL,
                        buf,
                        sizeof(buf),
                        NULL);
        }

        return(rc);
}


/*##**********************************************************************\
 *
 *              dbe_logfile_getsize
 *
 * Gets size sum (in kbytes) of all log files
 *
 * Parameters :
 *
 *      logfile - in, use
 *              pointer to logfile object
 *
 * Return value :
 *      size sum of log files in kilobytes
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
ulong dbe_logfile_getsize(dbe_logfile_t* logfile)
{
        su_daddr_t size;
        dbe_logfnum_t logfnum;
        char* logfname;
        bool existp;

        ss_dassert(logfile != NULL)
        logfile_enter_mutex(logfile);
        size = logfile->lf_lp.lp_daddr;
        if (logfile->lf_lp.lp_bufpos != 0) {
            size++;
        }
        logfnum = dbe_counter_getlogfnum(logfile->lf_counter);
        logfile_exit_mutex(logfile);
        do {
            logfnum--;
            if (logfnum == 0) {
                break;
            }
            logfname = dbe_logfile_genname(
                            logfile->lf_logdir,
                            logfile->lf_nametemplate,
                            logfnum,
                            logfile->lf_digittemplate);
            if (logfname == NULL) {
                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_ILLLOGFILETEMPLATE_SSSDD,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGFILETEMPLATE,
                    logfile->lf_nametemplate,
                    DBE_LOGFILENAME_MINDIGITS,
                    DBE_LOGFILENAME_MAXDIGITS);
            }
            existp = SsFExist(logfname);
            if (existp) {
                size += SsFSize(logfname) / logfile->lf_bufsize;
            }
            SsMemFree(logfname);
        } while (existp);
        size = (long)
            ((double)size * ((double)logfile->lf_bufsize / 1024.0) + 0.5);
        return (size);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_seterrorhandler
 *
 * Sets an error handler function for failed log write. This enables
 * the database to create checkpoint when e.g. the hard disk containing
 * the log file crashes.
 *
 * Parameters :
 *
 *      logfile - in out, use
 *              pointer to logfile object
 *
 *      errorfunc - in, hold
 *          pointer to error function
 *
 *      errorctx - in, hold
 *          context pointer to be passed as argument of errorfunc
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_logfile_seterrorhandler(
        dbe_logfile_t* logfile,
        void (*errorfunc)(void*),
        void* errorctx)
{
        logfile->lf_errorfunc = errorfunc;
        logfile->lf_errorctx = errorctx;
}

/*##**********************************************************************\
 *
 *              dbe_logfile_getfilewritecnt
 *
 * Returns the number of file writes done in this log object.
 *
 * Parameters :
 *
 *      logfile -
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
ulong dbe_logfile_getfilewritecnt(dbe_logfile_t* logfile)
{
        return(logfile->lf_filewritecnt);
}

/*##**********************************************************************\
 *
 *              dbe_logpos_cmp
 *
 * Compares two log positions.
 *
 * Parameters :
 *
 *      lp1 -
 *
 *
 *      lp2 -
 *
 *
 * Return value :
 *      < 0, 0, or > 0
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long dbe_logpos_cmp(dbe_logpos_t* lp1, dbe_logpos_t* lp2)
{
        if (lp1->lp_daddr == lp2->lp_daddr) {
            return((long)lp1->lp_bufpos - (long)lp2->lp_bufpos);
        } else {
            return((long)lp1->lp_daddr - (long)lp2->lp_daddr);
        }

}

/*##**********************************************************************\
 *
 *              dbe_logfile_getlogpos
 *
 * Returns current log position.
 *
 * Parameters :
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_catchup_logpos_t dbe_logfile_getlogpos(dbe_logfile_t* logfile)
{
        dbe_catchup_logpos_t logpos;

        ss_dassert(logfile != NULL);

        logfile_enter_mutex(logfile);
        logpos = logfile->lf_catchup_logpos;
        logfile_exit_mutex(logfile);

        ss_dprintf_1(("dbe_logfile_getlogpos:logpos(%d,%s,%d,%d,%d)\n", LOGPOS_DSDDD(logpos)));

        return(logpos);
}

/*##**********************************************************************\
 *
 *              dbe_logfile_encrypt
 *
 * Encrypts existing log files with new encryption key.
 *
 * Parameters :
 *
 *      cfg - in, use
 *
 *      dbheader - in, use
 *
 *      cipher - in, use
 *
 *      old_cipher - in, use
 *
 *
 * Return value :
 *      SU_SUCCESS,
 *      SU_ERR_FILE_READ_FAILURE -
 *      SU_ERR_FILE_WRITE_FAILURE -
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_logfile_encrypt(
        dbe_cfg_t*    cfg,
        dbe_header_t* dbheader,
        su_cipher_t*  cipher,
        su_cipher_t*  old_cipher,
        dbe_encrypt_t encrypt,
        dbe_decrypt_t decrypt)
{
        size_t      size;
        int         openflags = SS_BF_SEQUENTIAL | SS_BF_FLUSH_AFTERWRITE;
        su_vfilh_t* vfil;
        char*       data;
        char*       logdir;
        char*       nametemplate;
        char        digittemplate;
        dbe_logfnum_t logfnum;
        dbe_counter_t* ctr = dbe_counter_init();
        dbe_startrec_t* sr;
        uint         i;
        int         j;
        dbe_ret_t   rc = SU_SUCCESS;

        dbe_cfg_getlogblocksize(cfg, &size);
        dbe_cfg_getlogdir(cfg, &logdir);
        dbe_cfg_getlogfilenametemplate(cfg, &nametemplate);
        dbe_cfg_getlogdigittemplate(cfg, &digittemplate);

        sr = dbe_header_getstartrec(dbheader);
        dbe_counter_getinfofromstartrec(ctr, sr);

        data = SsMemAlloc(size);

        logfnum = dbe_counter_getlogfnum(ctr);

        ss_dprintf_1(("dbe_logfile_encrypt: cipher=%p old=%p logfnum=%d\n",
                      cipher, old_cipher, logfnum));

        for (j=logfnum; j>=0; j--) {
            char* fname = dbe_logfile_genname(logdir, nametemplate, j, digittemplate);
            SsBFileT* bfile;
            su_pfilh_t* pfh;
            size_t nrblks;

            ss_dprintf_1(("dbe_logfile_encrypt: fname = %s\n", fname));

            if (!SsFExist(fname)) {
                ss_dprintf_1(("dbe_logfile_encrypt: file %s does not exist\n",
                              fname));
                SsMemFree(fname);
                break;
            }

            vfil = su_vfh_init(fname, TRUE, openflags, size);
            bfile = su_vfh_beginaccesspers(vfil, &pfh);
            nrblks = SsBSizePages(bfile, size);

            ss_dprintf_1(("dbe_logfile_encrypt: filesize = %d\n", nrblks));

            for (i=0; i<nrblks; i++) {
                int   rc2;
                char* write_data;

                rc2 = SsBReadPages(bfile, i, size, data, 1);
                if (rc2 == -1) {
                    rc = SU_ERR_FILE_READ_FAILURE;
                }
                ss_dassert(rc2==1);

                if (old_cipher != NULL) {
                    ss_dprintf_1(("dbe_logfile_encrypt: decrypt %d\n", i));
                    decrypt(old_cipher, i, data, 1, size);
                }

                write_data = data;

                if (cipher != NULL) {
                    ss_dprintf_1(("dbe_logfile_encrypt: encrypt %d\n", i));
                    write_data = encrypt(cipher, i, data, 1, size);
                }

                rc2 = SsBWritePages(bfile, i, size, write_data, 1);
                if (write_data != data) {
                    SsMemFree(write_data);
                }

                if (rc2 == -1) {
                    rc = SU_ERR_FILE_WRITE_FAILURE;
                }
                ss_dassert(rc2==1);
            }

            su_vfh_endaccess(vfil, pfh);

            su_vfh_done(vfil);
            SsMemFree(fname);
        }
        SsMemFree(data);
        SsMemFree(nametemplate);
        SsMemFree(logdir);
        dbe_counter_done(ctr);

        return rc;
}

#endif /* SS_NOLOGGING */
