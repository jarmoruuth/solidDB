/*************************************************************************\
**  source       * ssrtcov.c
**  directory    * ss
**  description  * Runtime coverage monitoring routines.
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

#include "ssenv.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssstring.h"
#include "sschcvt.h"
#include "ssrtcov.h"

#ifdef SS_RTCOVERAGE

ss_rtcoverage_t ss_rtcoverage;

void SsRtCovInit(void)
{
        SsRtCovClear();
}

void SsRtCovClear(void)
{
        memset(&ss_rtcoverage, '\0', sizeof(ss_rtcoverage_t));
}

void SsRtCovGetData(ss_rtcoverage_t* rtcoverage)
{
        memcpy(rtcoverage, &ss_rtcoverage, sizeof(ss_rtcoverage_t));
}

#else /* SS_RTCOVERAGE */

void SsRtCovInit(void)
{
}

void SsRtCovClear(void)
{
}

#endif /* SS_RTCOVERAGE */ 

