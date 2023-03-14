/*************************************************************************\
**  source       * ssmath.c
**  directory    * ss
**  description  * Math replacements
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include "ssmath.h"

#if defined(SS_DIX) || defined(SS_SCO)
/* in these OS'es the only way to detect math errors is errno variable */

#include <errno.h>
#include "sstraph.h"

#define NUMFUNENTER errno = 0
#define NUMFUNEXIT  if (errno != 0) { ss_trap_raise(SS_TRAP_FPE); }

#else /* DIX et al. */

#define NUMFUNENTER
#define NUMFUNEXIT

#endif /* DIX et al. */

double SsFabs(double d)
{
        NUMFUNENTER;
        d = fabs(d);
        NUMFUNEXIT;
        return (d);
}

double SsAcos(double d)
{
        NUMFUNENTER;
        d = acos(d);
        NUMFUNEXIT;
        return (d);
}

double SsAsin(double d)
{
        NUMFUNENTER;
        d = asin(d);
        NUMFUNEXIT;
        return (d);
}

double SsAtan(double d)
{
        NUMFUNENTER;
        d = atan(d);
        NUMFUNEXIT;
        return (d);
}
double SsAtan2(double d1, double d2)
{
        NUMFUNENTER;
        d1 = atan2(d1,d2);
        NUMFUNEXIT;
        return (d1);
}

double SsCos(double d)
{
        NUMFUNENTER;
        d = cos(d);
        NUMFUNEXIT;
        return (d);
}

double SsCot(double d)
{
        NUMFUNENTER;
        d = cos(d) / sin(d);
        NUMFUNEXIT;
        return (d);
}

double SsExp(double d)
{
        NUMFUNENTER;
        d = exp(d);
        NUMFUNEXIT;
        return (d);
}

double SsLog(double d)
{
        NUMFUNENTER;
        d = log(d);
        NUMFUNEXIT;
        return (d);
}

double SsLog10(double d)
{
        NUMFUNENTER;
        d = log10(d);
        NUMFUNEXIT;
        return (d);
}

double SsPow(double d1, double d2)
{
        NUMFUNENTER;
        d1 = pow(d1, d2);
        NUMFUNEXIT;
        return (d1);
}

double SsSin(double d)
{
        NUMFUNENTER;
        d = sin(d);
        NUMFUNEXIT;
        return (d);
}

double SsSqrt(double d)
{
        NUMFUNENTER;
        d = sqrt(d);
        NUMFUNEXIT;
        return (d);
}

double SsTan(double d)
{
        NUMFUNENTER;
        d = tan(d);
        NUMFUNEXIT;
        return (d);
}

double SsCeil(double d)
{
        NUMFUNENTER;
        d = ceil(d);
        NUMFUNEXIT;
        return (d);
}
        
double SsFloor(double d)
{
        NUMFUNENTER;
        d = floor(d);
        NUMFUNEXIT;
        return (d);
}
