/*************************************************************************\
**  source       * xs1merg.h
**  directory    * xs
**  description  * Polyphase merge sort
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


#ifndef XS1MERG_H
#define XS1MERG_H

#include <ssc.h>
#include <su0parr.h>
#include "xs0qsort.h"
#include "xs2stre.h"
#include "xs0type.h"

typedef struct xs_merge_st xs_merge_t;

xs_merge_t* xs_merge_init(
        su_pa_t* readstream_pa,
        xs_stream_t* writestream,
	xs_qcomparefp_t comp_fp,
        void* comp_context,
        ulong step_size_bytes,
        uint step_size_rows);

void xs_merge_done(
        xs_merge_t* mg);

xs_ret_t xs_merge_step(
        xs_merge_t* mg,
        xs_stream_t** p_resultstream,
        rs_err_t** p_errh);




#endif /* XS1MERG_H */
