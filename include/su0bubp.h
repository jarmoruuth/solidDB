/*************************************************************************\
**  source       * su0bubp.h
**  directory    * su
**  description  * Backup buffer pool
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


#ifndef SU0BUBP_H
#define SU0BUBP_H

#include <ssc.h>

typedef struct su_backupbufpool_st su_backupbufpool_t;

su_backupbufpool_t* su_backupbufpool_init(
        size_t bufsize,
        size_t nbuffers_max,
        void (*outofbuffers_callbackfun)(void* callbackctx),
        void (*bufferfreedsignal_callbackfun)(void* callbackctx),
        void* (*allocfun)(void* ctx, size_t bufsize),
        void (*freefun)(void* ctx, void* buf),
        void* allocandfreectx,
        bool cache);

void su_backupbufpool_done(
        su_backupbufpool_t* bubp);

void* su_backupbufpool_getbuf(
        void* cd,
        su_backupbufpool_t* bubp);

void su_backupbufpool_releasebuf(
        void* cd,
        su_backupbufpool_t* bubp,
        void* buf);

size_t su_backupbufpool_getbufsize(
        su_backupbufpool_t* bubp);

#endif /* SU0BUBP_H */
