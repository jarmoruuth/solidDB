/*************************************************************************\
**  source       * dt1dfl.h
**  directory    * dt
**  description  * Internal dfloat definitions
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


#ifndef DT1DFL_H
#define DT1DFL_H

#include "dt0type.h"

#if LONG_BIT == 64
#  define DF_LONGBIT    32
#  define DF_LONGMAX    SS_INT4_MAX
#  define DF_LONGMIN    SS_INT4_MIN
#else 
#  define DF_LONGBIT    LONG_BIT
#  define DF_LONGMAX    LONG_MAX
#  define DF_LONGMIN    LONG_MIN
#endif /* LONG_BIT == 64 */

extern dt_dfl_t dfl_nan;

dt_dfl_t dfl_sum(dt_dfl_t d1, dt_dfl_t d2);
dt_dfl_t dfl_diff(dt_dfl_t d1, dt_dfl_t d2);
dt_dfl_t dfl_prod(dt_dfl_t d1, dt_dfl_t d2);
dt_dfl_t dfl_quot(dt_dfl_t d1, dt_dfl_t d2);
int   dfl_overflow(dt_dfl_t dfl);
int   dfl_compare(dt_dfl_t d1, dt_dfl_t d2);
int   dfl_negative(dt_dfl_t* p_dfl);

dt_dfl_t dfl_inttodfl(int i);
dt_dfl_t dfl_intetodfl(int i, int exp);
dt_dfl_t dfl_longtodfl(long);
dt_dfl_t dfl_longetodfl(long, int);
int   dfl_asciiztodfl(char *, dt_dfl_t *);
void  dfl_nsttodfl(char* par_nst, dt_dfl_t* pdf);
dt_dfl_t dfl_rounded(dt_dfl_t df, int exp);
dt_dfl_t dfl_truncated(dt_dfl_t df, int exp);
void  dfl_change_sign(dt_dfl_t* df);
int df_eqzero(dt_dfl_t* p_df);

int   dfl_dfltoint(dt_dfl_t df);
long  dfl_dfltolong(dt_dfl_t df);
bool  dfl_dfltoasciiz_maxlen(dt_dfl_t df, char* buf, int maxlen);
bool  dfl_dfltoasciiz_dec_maxlen(dt_dfl_t df, int dec, char* buf, int maxlen);
void  dfl_dfltonst(dt_dfl_t* p_df, char* nst);
void  dfl_print(dt_dfl_t df);

#endif /* DT1DFL_H */
