/*************************************************************************\
**  source       * rs0bull.h
**  directory    * res
**  description  * Bulletin Board
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


#ifndef SNC0BULL_H
#define SNC0BULL_H

#include "rs0sysi.h"

typedef struct bboard_st rs_bboard_t;

rs_bboard_t* rs_bboard_init(
        rs_sysi_t* cd);

void rs_bboard_done(
        rs_bboard_t* bb);

bool rs_bboard_put(
        rs_bboard_t* bb,
        char* key,
        char* data,
        size_t data_len);

bool rs_bboard_putsysval(
        rs_bboard_t* bb,
        char* key,
        char* data,
        size_t data_len);

bool rs_bboard_puttentativesysval(
        rs_bboard_t* bb,
        char* key,
        char* data,
        size_t data_len);

bool rs_bboard_get(
        rs_bboard_t* bb,
        char* key,
        char** data,
        size_t* data_len);

bool rs_bboard_exists(
        rs_bboard_t* bb,
        char* key);

bool rs_bboard_remove(
        rs_bboard_t* bb,
        char* key);

void* rs_bboard_first(
        rs_bboard_t* bb,
        char** key,
        char** data,
        size_t* data_len,
        bool* p_issys);

void* rs_bboard_next(
        rs_bboard_t* bb,
        void* bbnode,
        char** key,
        char** data,
        size_t* data_len,
        bool* p_issys);

void rs_bboard_clear(
        rs_bboard_t* bb);

void rs_bboard_trxend(
        rs_bboard_t* bb,
        bool commitp);

#endif /* SNC0BULL_H */
