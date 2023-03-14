/*************************************************************************\
**  source       * su0burdr.c
**  directory    * su
**  description  * Backup reader framework
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


#include <ssstdio.h>
#include <ssstdlib.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include "su1check.h"
#include "su0bubp.h"
#include "su0burdr.h"

struct su_backupreader_st {
        int burdr_check;
        su_backupbufpool_t* burdr_bufpool;
        size_t burdr_bufsize;
        size_t burdr_fileblocksize;
        void* burdr_file;
        su_ret_t (*burdr_readfun)(
                void* file,
                su_daddr_t pos, /* in blocks */
                void* buf,
                size_t bufsize,
                size_t* p_bufbytes);
        void (*burdr_filedonefun)(void* file);
};

#define backupreader_getfileblocksize(burdr) ((burdr)->burdr_fileblocksize)

#define backupreader_getbufsize(burdr) ((burdr)->burdr_bufsize)

#define CHK_BURDR(burdr) ss_assert(SS_CHKPTR(burdr)); \
                         ss_rc_assert((burdr)->burdr_check == \
                                      SUCHK_BACKUPREADER,\
                                      (burdr)->burdr_check);

typedef enum {
    BURS_RANGE,
    BURS_STRIPED
} readspec_type_t;

struct su_backupreadspec_st {
        int        burs_check;
        su_backupreader_t* burs_reader;
        readspec_type_t burs_type;
        union {
                struct {
                        su_daddr_t start;
                        su_daddr_t end; /* first address not to read */
                        su_daddr_t pos; /* pos >= start && pos < end */
                } range;
                struct { /* all in blocks, because stripe size must be multiple
                          * of block size
                          */
                        su_daddr_t start;
                        su_daddr_t width;
                        su_daddr_t skip;
                        su_daddr_t pos; /* pos >= start && pos < start + width */
                } stripe;
        } burs_;
};

#define CHK_BURS(burs) ss_assert(SS_CHKPTR(burs));\
        ss_rc_assert((burs)->burs_check == SUCHK_BACKUPREADSPEC, (burs)->burs_check)

su_backupreadspec_t* su_backupreadspec_init_range(
        su_backupreader_t* reader,
        su_daddr_t start,
        su_daddr_t end)
{
        su_backupreadspec_t* burs = SSMEM_NEW(su_backupreadspec_t);

        ss_dassert(end > start);
        burs->burs_check = SUCHK_BACKUPREADSPEC;
        burs->burs_reader = reader;
        burs->burs_type = BURS_RANGE;
        burs->burs_.range.start = start;
        burs->burs_.range.end = end;
        return (burs);
}

su_backupreadspec_t* su_backupreadspec_init_striped(
        su_backupreader_t* reader,
        su_daddr_t start,
        su_daddr_t width,
        su_daddr_t skip)
{
        su_backupreadspec_t* burs = SSMEM_NEW(su_backupreadspec_t);

        burs->burs_check = SUCHK_BACKUPREADSPEC;
        burs->burs_reader = reader;
        burs->burs_type = BURS_STRIPED;
        burs->burs_.stripe.start =
            burs->burs_.stripe.pos = start;
        burs->burs_.stripe.width = width;
        burs->burs_.stripe.skip = skip;
        return (burs);
}

void su_backupreadspec_done(su_backupreadspec_t* burs)
{
        CHK_BURS(burs);
        burs->burs_check = SUCHK_FREEDBACKUPREADSPEC;
        SsMemFree(burs);
}

static su_daddr_t backupreadspec_getpos(
        su_backupreadspec_t* burs,
        su_daddr_t* p_nblocks_left)
{
        su_daddr_t daddr = 0;
        
        CHK_BURS(burs);
        ss_dassert(p_nblocks_left != NULL);
        switch (burs->burs_type) {
            case BURS_RANGE:
                daddr = burs->burs_.range.pos;
                ss_dassert(daddr >= burs->burs_.range.start &&
                           burs->burs_.range.end >= daddr);
                *p_nblocks_left = burs->burs_.range.end - daddr;
                break;
            case BURS_STRIPED:
                daddr = burs->burs_.stripe.pos;
                ss_dassert(daddr >= burs->burs_.stripe.start &&
                           daddr < (burs->burs_.stripe.start +
                                    burs->burs_.stripe.width));
                *p_nblocks_left =
                    burs->burs_.stripe.start +
                    burs->burs_.stripe.width -
                    daddr;
                break;
            default:
                ss_rc_error(burs->burs_type);
                break;
        }
        return (daddr);
}

static bool backupreadspec_advancepos(
        su_backupreadspec_t* burs,
        su_daddr_t nblocks)
{
        bool more_to_read = TRUE;
        
        CHK_BURS(burs);
        switch (burs->burs_type) {
            case BURS_RANGE:
                burs->burs_.range.pos += nblocks;
                if (burs->burs_.range.pos >= burs->burs_.range.end) {
                    ss_dassert(burs->burs_.range.pos == burs->burs_.range.end);
                    more_to_read = FALSE;
                }
                break;
            case BURS_STRIPED:
                burs->burs_.stripe.pos += nblocks;
                if (burs->burs_.stripe.pos >=
                    burs->burs_.stripe.start + burs->burs_.stripe.width)
                {
                    ss_dassert(burs->burs_.stripe.pos ==
                               (burs->burs_.stripe.start +
                                burs->burs_.stripe.width));
                    burs->burs_.stripe.start +=
                        burs->burs_.stripe.width +
                        burs->burs_.stripe.skip;
                    burs->burs_.stripe.pos = burs->burs_.stripe.start;
                }
                break;
            default:
                ss_rc_error(burs->burs_type);
                break;
        }
        return (more_to_read);
}

su_ret_t su_backupreadspec_reachandreadbuf(
        su_backupreadspec_t* burs,
        void* cd,
        void** p_buf,
        size_t* p_bufbytes)
{
        su_ret_t rc;
        su_daddr_t readpos;
        su_daddr_t nblocks = 0;
        size_t fileblocksize;
        size_t bufsize;
        su_daddr_t nblockstoread;
        size_t bytestoread;
        
        CHK_BURS(burs);
        fileblocksize = backupreader_getfileblocksize(burs->burs_reader);
        bufsize = backupreader_getbufsize(burs->burs_reader);
        readpos = backupreadspec_getpos(burs, &nblocks);
        if (nblocks == 0) {
            *p_buf = NULL;
            *p_bufbytes = 0;
            return (SU_RC_END);
        }
        nblockstoread = bufsize / fileblocksize;
        ss_rc_dassert(nblockstoread > 0, nblockstoread);
        if (nblocks < nblockstoread) {
            nblockstoread = nblocks;
        }
        bytestoread = nblockstoread * fileblocksize;
        rc = su_backupreader_reachandreadbuf(
                cd,
                burs->burs_reader,
                readpos,
                p_buf,
                bytestoread,
                p_bufbytes);
        if (rc == SU_SUCCESS) {
            ss_rc_dassert((*p_bufbytes % fileblocksize) == 0, (int)*p_bufbytes)
            backupreadspec_advancepos(burs, *p_bufbytes / fileblocksize);
        }
        return (rc);
}
        
        
su_backupreader_t* su_backupreader_init(
        su_backupbufpool_t* bufpool,
        void* file,
        size_t fileblocksize,
        su_ret_t (*readfun)(void* file,
                            su_daddr_t pos, /* in blocks */
                            void* buf,
                            size_t bufsize,
                            size_t* p_bufbytes),
        void (*filedonefun)(void* file))
{
        su_backupreader_t* burdr = SSMEM_NEW(su_backupreader_t);
        burdr->burdr_check = SUCHK_BACKUPREADER;
        burdr->burdr_bufpool = bufpool;
        burdr->burdr_bufsize = su_backupbufpool_getbufsize(bufpool);
        burdr->burdr_fileblocksize = fileblocksize;
        burdr->burdr_file = file;
        burdr->burdr_readfun = readfun;
        burdr->burdr_filedonefun = filedonefun;
        return (burdr);
}

void su_backupreader_done(su_backupreader_t* burdr)
{
        CHK_BURDR(burdr);
        burdr->burdr_check = SUCHK_FREEDBACKUPREADER;
        if (burdr->burdr_filedonefun != NULL) {
            (*burdr->burdr_filedonefun)(burdr->burdr_file);
        }
        SsMemFree(burdr);
}

su_ret_t su_backupreader_reachandreadbuf(
        void* cd,
        su_backupreader_t* burdr,
        su_daddr_t pos,
        void** p_buf,
        size_t bytes_max __attribute__ ((unused)),
        size_t* p_bufbytes)
{
        su_ret_t rc;
        void* buf;
        
        CHK_BURDR(burdr);
        ss_dassert(p_buf != NULL);
        ss_dassert(p_bufbytes != NULL);
        *p_buf = buf =
            su_backupbufpool_getbuf(cd, burdr->burdr_bufpool);
        if (buf == NULL) {
            rc = SU_RC_OUTOFBUFFERS;
            *p_bufbytes = 0;
        } else {
            rc = (*burdr->burdr_readfun)(
                    burdr->burdr_file,
                    pos,
                    buf,
                    burdr->burdr_bufsize,
                    p_bufbytes);
            if (rc == SU_SUCCESS && *p_bufbytes == 0) {
                su_backupbufpool_releasebuf(
                        cd,
                        burdr->burdr_bufpool,
                        buf);
                *p_buf = NULL;
                rc = SU_RC_END;
            }
        }
        return (rc);
}
