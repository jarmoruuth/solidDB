/*************************************************************************\
**  source       * dbe5dsea.h
**  directory    * dbe
**  description  * Data search routines. Used to search the data tuples
**               * using a tuple reference.
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


#ifndef DBE5DSEA_H
#define DBE5DSEA_H

#include <uti0vtpl.h>

#include <su0list.h>

#include "dbe9type.h"
#include "dbe6srk.h"
#include "dbe5inde.h"
#include "dbe5isea.h"
#include "dbe0type.h"
#include "dbe0erro.h"

typedef struct dbe_datasea_st dbe_datasea_t;

dbe_datasea_t* dbe_datasea_init(
        void* cd,
        dbe_index_t* index,
        rs_key_t* key,
        dbe_btrsea_timecons_t* tc,
        su_list_t* conslist,
        bool pessimistic,
        char* caller);

void dbe_datasea_done(
        dbe_datasea_t* datasea);

void dbe_datasea_reset(
        dbe_datasea_t* datasea,
        su_list_t* conslist);

dbe_ret_t dbe_datasea_search(
        dbe_datasea_t* datasea,
        vtpl_t* refvtpl,
        dbe_trxid_t stmttrxid,
        dbe_srk_t** srk);

void dbe_datasea_setlongseqsea(
        dbe_datasea_t* datasea);

void dbe_datasea_clearlongseqsea(
        dbe_datasea_t* datasea);

#endif /* DBE5DSEA_H */
