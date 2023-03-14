/*************************************************************************\
**  source       * su0burdr.h
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


#ifndef SU0BURDR_H
#define SU0BURDR_H

#include <ssc.h>
#include "su0types.h"
#include "su0error.h"
#include "su0bubp.h"

typedef struct su_backupreader_st su_backupreader_t;

typedef struct su_backupreadspec_st su_backupreadspec_t;

su_backupreadspec_t* su_backupreadspec_init_range(
        su_backupreader_t* reader,
        su_daddr_t start,
        su_daddr_t end);

su_backupreadspec_t* su_backupreadspec_init_striped(
        su_backupreader_t* reader,
        su_daddr_t start,
        su_daddr_t width,
        su_daddr_t skip);

void su_backupreadspec_done(
        su_backupreadspec_t* burs);

su_ret_t su_backupreadspec_reachandreadbuf(
        su_backupreadspec_t* burs,
        void* cd,
        void** p_buf,
        size_t* p_bufbytes);

su_backupreader_t* su_backupreader_init(
        su_backupbufpool_t* bufpool,
        void* file,
        size_t fileblocksize,
        su_ret_t (*readfun)(void* file,
                            su_daddr_t pos, /* in blocks */
                            void* buf,
                            size_t bufsize,
                            size_t* p_bufbytes),
        void (*filedonefun)(void* file));

void su_backupreader_done(
        su_backupreader_t* burdr);

su_ret_t su_backupreader_reachandreadbuf(
        void* cd,
        su_backupreader_t* burdr,
        su_daddr_t pos,
        void** p_buf,
        size_t bytes_max,
        size_t* p_bufbytes);



#endif /* SU0BURDR_H */
