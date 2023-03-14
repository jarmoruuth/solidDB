/*************************************************************************\
**  source       * su0buwr.h
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


#ifndef SU0BUWR_H
#define SU0BUWR_H

#include <ssc.h>
#include "su0types.h"
#include "su0error.h"
#include "su0bubp.h"

typedef struct su_backupwriter_st su_backupwriter_t;

su_backupwriter_t* su_backupwriter_init(
        su_backupbufpool_t* bufpool,
        void* file,
        su_ret_t (*writefun)(void* file,
                             su_daddr_t pos, /* in blocks */
                             void* buf,
                             size_t bufbytes),
        void (*filedonefun)(void* file));

void su_backupwriter_done(
        su_backupwriter_t* buwr);

su_ret_t su_backupwriter_writeandreleasebuf(
        void* cd,
        su_backupwriter_t* buwr,
        su_daddr_t pos,
        void* buf,
        size_t bufbytes);

#endif /* SU0BUWR_H */
