/*************************************************************************\
**  source       * su0buwr.c
**  directory    * su
**  description  * Backup writer framework
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
#include "su0buwr.h"

struct su_backupwriter_st {
        int buwr_check;
        su_backupbufpool_t* buwr_bufpool;
        void* buwr_file;
        su_ret_t (*buwr_writefun)(
                void* file,
                su_daddr_t pos, /* in blocks */
                void* buf,
                size_t bufbytes);
        void (*buwr_filedonefun)(void* file);
};


#define CHK_BUWR(buwr) ss_assert(SS_CHKPTR(buwr)); \
                       ss_rc_assert((buwr)->buwr_check == SUCHK_BACKUPWRITER,\
                                    (buwr)->buwr_check);

su_backupwriter_t* su_backupwriter_init(
        su_backupbufpool_t* bufpool,
        void* file,
        su_ret_t (*writefun)(void* file,
                             su_daddr_t pos, /* in blocks */
                             void* buf,
                             size_t bufbytes),
        void (*filedonefun)(void* file))
{
        su_backupwriter_t* buwr = SSMEM_NEW(su_backupwriter_t);
        buwr->buwr_check = SUCHK_BACKUPWRITER;
        buwr->buwr_bufpool = bufpool;
        buwr->buwr_file = file;
        buwr->buwr_writefun = writefun;
        buwr->buwr_filedonefun = filedonefun;
        return (buwr);
}

void su_backupwriter_done(su_backupwriter_t* buwr)
{
        CHK_BUWR(buwr);
        buwr->buwr_check = SUCHK_FREEDBACKUPWRITER;
        if (buwr->buwr_filedonefun != NULL) {
            (*buwr->buwr_filedonefun)(buwr->buwr_file);
        }
        SsMemFree(buwr);
}

su_ret_t su_backupwriter_writeandreleasebuf(
        void* cd,
        su_backupwriter_t* buwr,
        su_daddr_t pos,
        void* buf,
        size_t bufbytes)
{
        su_ret_t rc;
        CHK_BUWR(buwr);
        ss_dassert(bufbytes <= su_backupbufpool_getbufsize(buwr->buwr_bufpool));
        rc = (*buwr->buwr_writefun)(
                buwr->buwr_file,
                pos,
                buf,
                bufbytes);
        su_backupbufpool_releasebuf(
                cd,
                buwr->buwr_bufpool,
                buf);
        return (rc);
}
