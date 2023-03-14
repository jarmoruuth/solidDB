/*************************************************************************\
**  source       * ssmath.h
**  directory    * ss
**  description  * Replaces math.h
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


#ifndef SSMATH_H
#define SSMATH_H

#include "ssenv.h"
#include <math.h>

double SsFabs(double d);
double SsAcos(double d);
double SsAsin(double d);
double SsAtan(double d);
double SsAtan2(double d1, double d2);
double SsCos(double d);
double SsCot(double d);
double SsExp(double d);
double SsLog(double d);
double SsLog10(double d);
double SsPow(double d1, double d2);
double SsSin(double d);
double SsSqrt(double d);
double SsTan(double d);
double SsFloor(double d);
double SsCeil(double d);

#endif /* SSMATH_H */
