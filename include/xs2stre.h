/*************************************************************************\
**  source       * xs2stre.h
**  directory    * xs
**  description  * Stream utility for sort
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


#ifndef XS2STRE_H
#define XS2STRE_H

#include <ssstdio.h>

#include <ssc.h>

#include <uti0vtpl.h>

#include <su0parr.h>
#include <su0list.h>

#include "xs2tfmgr.h"
#include "xs0type.h"

typedef struct sortstreamarr_st xs_streamarr_t;
typedef struct sortstream_st xs_stream_t;

typedef enum {
/* Stream status values */
        SSTA_RUN,   /* Run ongoing                  */
        SSTA_HOLD,  /* Stream suspended, dummy run  */
        SSTA_EOS,   /* End of stream                */
        SSTA_EOR,   /* End of run                   */
        SSTA_BOS,
        SSTA_ERROR  /* Stream has faced an error */
} xs_streamstatus_t;

xs_streamarr_t* xs_streamarr_init(
        int maxstreams,
        xs_tfmgr_t* tfmgr);

void xs_streamarr_done(
        xs_streamarr_t* sa);

xs_stream_t* xs_streamarr_nextstream(
        xs_streamarr_t* sa);

bool xs_streamarr_endofdistribute(
        xs_streamarr_t* sa,
        xs_stream_t** p_writestream,
        su_pa_t** p_readstream_pa);

xs_stream_t* xs_stream_init(
        xs_tfmgr_t* tfmgr);

void xs_stream_done(
        xs_stream_t* stream);

void xs_stream_link(
        xs_stream_t* stream);

bool xs_stream_append(
        xs_stream_t* stream,
        void* data,
        size_t sz,
        rs_err_t** p_errh);

xs_streamstatus_t xs_stream_getnext(
        xs_stream_t* stream,
        void** p_data,
        size_t* p_sz);

xs_streamstatus_t xs_stream_getprev(
        xs_stream_t* stream,
        void** p_data,
        size_t* p_sz);

xs_streamstatus_t xs_stream_rewrite(
        xs_stream_t* stream);

xs_streamstatus_t  xs_stream_rewind(
        xs_stream_t* stream);

xs_streamstatus_t xs_stream_initfetch(
        xs_stream_t* stream);

xs_streamstatus_t xs_stream_cursortobegin(
        xs_stream_t* stream);

xs_streamstatus_t xs_stream_cursortoend(
        xs_stream_t* stream);

xs_streamstatus_t xs_stream_getstatus(
        xs_stream_t* stream);

long xs_stream_nruns(
        xs_stream_t* stream);

bool xs_stream_seteoratend(
        xs_stream_t* stream,
        rs_err_t** p_errh);

xs_streamstatus_t xs_stream_skipeor(
        xs_stream_t* stream);

#endif /* XS2STRE_H */
