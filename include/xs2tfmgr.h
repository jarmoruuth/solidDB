/*************************************************************************\
**  source       * xs2tfmgr.h
**  directory    * xs
**  description  * eXternal Sorter Temporary File ManaGeR
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


#ifndef XS2TFMGR_H
#define XS2TFMGR_H

#include <ssc.h>
#include "xs2mem.h"
#include "xs0type.h"

typedef enum {
        XSTF_READ,
        XSTF_WRITE,
        XSTF_CURSOR,
        XSTF_CLOSED
} xs_tfstate_t;

typedef struct xs_tf_st xs_tf_t;
typedef struct xs_tfmgr_st xs_tfmgr_t;

void xs_tf_done(xs_tf_t* tf);
bool xs_tf_rewrite(xs_tf_t* tf);
bool xs_tf_rewind(xs_tf_t* tf);
char* xs_tf_peek(xs_tf_t* tf, size_t nbytes);
char* xs_tf_peekextend(xs_tf_t* tf, size_t oldnbytes, size_t nbytes);
bool xs_tf_moveposrel(xs_tf_t* tf, long diff);
bool xs_tf_movetobegin(xs_tf_t* tf);
bool xs_tf_movetoend(xs_tf_t* tf);
bool xs_tf_append(xs_tf_t* tf, void* data, size_t nbytes, rs_err_t** p_errh);
bool xs_tf_opencursor(xs_tf_t* tf);

bool xs_tf_close(xs_tf_t* tf);
bool xs_tf_open(xs_tf_t* tf);

xs_tfmgr_t* xs_tfmgr_init(
        ulong maxfiles,
        xs_mem_t* memmgr,
        ulong dbid,
        int openflags);
void xs_tfmgr_done(xs_tfmgr_t* tfmgr);
bool xs_tfmgr_adddir(xs_tfmgr_t* tfmgr, char* dirname, ulong maxblocks);
xs_tf_t* xs_tfmgr_tfinit(xs_tfmgr_t* tfmgr);

#endif /* XS2TFMGR_H */
