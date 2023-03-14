/*************************************************************************\
**  source       * ssnumuti.h
**  directory    * ss
**  description  * Numeric utilities for tests
**               * 
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


#ifndef SSNUMUTI_H
#define SSNUMUTI_H

#include "ssenv.h"
#include "ssc.h"

#if !defined(NO_ANSI) && !defined(NO_ANSI_FLOAT)

long ss_long_rand(void);
void ss_print_float(float f);
void ss_print_double(double f);
float ss_float_rand(void);
double ss_double_rand(void);
int ss_sign(int i);
bool ss_double_near(double d1, double d2, int nbits);

#else

long ss_long_rand();
float ss_float_rand();
double ss_double_rand();
bool ss_double_near();

#endif

#define SS_NUMUTI_INIT

#endif /* SSNUMUTI_H */
