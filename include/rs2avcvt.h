/*************************************************************************\
**  source       * rs2avcvt.h
**  directory    * res
**  description  * Low-level aid routines for aval conversions
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


#ifndef RS2AVCVT_H
#define RS2AVCVT_H

#include <ssc.h>
#include <ssint8.h>
#include "rs0atype.h"
#include "rs0aval.h"

ss_char1_t* rs_aval_tmpstrfromuni(
        void* cd,
        rs_atype_t* src_atype,
        rs_aval_t* src_aval,
        size_t* p_len);

rs_avalret_t rs_aval_putlong(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        long l);

rs_avalret_t rs_aval_putint8(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        ss_int8_t   i8);

rs_avalret_t rs_aval_putdfl(
        void* cd,
        rs_atype_t* dst_atype,
        rs_aval_t* dst_aval,
        dt_dfl_t* p_dfl);

rs_avalret_t rs_aval_putdbltochar(
        void* cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        double d,
        int prec);

rs_avalret_t rs_aval_putdbltouni(
        void* cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        double d,
        int prec);

rs_avalret_t rs_aval_putchartodate(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        ss_char1_t* s);

bool rs_aval_putvadatachar2to1(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* data,
        size_t dlen);

bool rs_aval_putdatachar2to1(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* data,
        size_t dlen);

#endif /* RS2AVCVT_H */
