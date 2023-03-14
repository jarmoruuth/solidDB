/*************************************************************************\
**  source       * dt0dfloa.h
**  directory    * dt
**  description  * Decimal float routines
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


#ifndef DT0DFLOA_H
#define DT0DFLOA_H

#include <ssc.h>
#include <uti0va.h>
#include "dt0type.h"

#define DFL_DBL_MAX     (3.6028797018963967E16)
#define DFL_DBL_MIN     (-DFL_DBL_MAX)
#define DFL_VA_MAXLEN   12

/* DFLOAT ROUTINES ********************************************************/


/************ Calculation and comparison **************/
bool dt_dfl_sum(dt_dfl_t* p_res_dfl, dt_dfl_t dfl1, dt_dfl_t dfl2);
bool dt_dfl_diff(dt_dfl_t* p_res_dfl, dt_dfl_t dfl1, dt_dfl_t dfl2);
bool dt_dfl_prod(dt_dfl_t* p_res_dfl, dt_dfl_t dfl1, dt_dfl_t dfl2);
bool dt_dfl_quot(dt_dfl_t* p_res_dfl, dt_dfl_t dfl1, dt_dfl_t dfl2);

bool dt_dfl_overflow(dt_dfl_t dfl);
bool dt_dfl_underflow(dt_dfl_t dfl);
int  dt_dfl_compare(dt_dfl_t dfl1, dt_dfl_t dfl2);

#define dt_dfl_eq(d1, d2) (dt_dfl_compare(d1, d2) ==  0)
#define dt_dfl_gt(d1, d2) (dt_dfl_compare(d1, d2) >   0)
#define dt_dfl_lt(d1, d2) (dt_dfl_compare(d1, d2) <   0)
#define dt_dfl_ge(d1, d2) (dt_dfl_compare(d1, d2) >=  0)
#define dt_dfl_le(d1, d2) (dt_dfl_compare(d1, d2) <=  0)

/************ Conversions to dfloat, address of destination dfl used ! *****/
bool dt_dfl_setint(dt_dfl_t* p_dest_dfl, int i);
bool dt_dfl_setinte(dt_dfl_t* p_dest_dfl, int i, int exp);
bool dt_dfl_setlong(dt_dfl_t* p_dest_dfl, long l);
bool dt_dfl_setlonge(dt_dfl_t* p_dest_dfl, long l, int exp);
bool dt_dfl_setdouble(dt_dfl_t* p_dest_dfl, double d);
bool dt_dfl_setasciiz(dt_dfl_t* p_dest_dfl, char* extdfl);
bool dt_dfl_setva(dt_dfl_t* p_dest_dfl, va_t* va);
bool dt_dfl_setdflprecision(dt_dfl_t* p_dest_dfl, dt_dfl_t src_dfl, int len, int scale);
bool dt_dfl_round(dt_dfl_t* p_dest_dfl, dt_dfl_t src_dfl, int scale);
bool dt_dfl_truncate(dt_dfl_t* p_dest_dfl, dt_dfl_t src_dfl, int scale);

void dt_dfl_change_sign(dt_dfl_t* p_dfl);
bool dt_dfl_eqzero(dt_dfl_t* p_dfl);
bool dt_dfl_negative(dt_dfl_t* p_dfl);

/************ Conversions from dfloat to other types ************/
bool dt_dfl_dfltoint(dt_dfl_t dfl, int* p_i);
bool dt_dfl_dfltolong(dt_dfl_t dfl, long* p_l);
bool dt_dfl_dfltodouble(dt_dfl_t dfl, double* p_d);
bool dt_dfl_dfltoasciiz_maxlen(dt_dfl_t dfl, char* res, int maxlen);
bool dt_dfl_dfltoasciiz_dec_maxlen(dt_dfl_t dfl, int n_decimals, char* res, int maxlen);
bool dt_dfl_dfltova(dt_dfl_t dfl, va_t* va);

#ifdef SS_DEBUG
char* dt_dfl_print(dt_dfl_t dfl);
#endif /* SS_DEBUG */

#endif /* DT0DFLOA_H */
