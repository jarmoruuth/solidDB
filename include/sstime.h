/*************************************************************************\
**  source       * sstime.h
**  directory    * ss
**  description  * Portable time functions.
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


#ifndef SSTIME_H
#define SSTIME_H

#include "ssenv.h"
#include "ssc.h" 

#include <time.h>   

#define SS_TIME_HIRES

#define SECSPERMIN	60L
#define MINSPERHOUR	60L
#define HOURSPERDAY	24L
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY	(SECSPERHOUR * HOURSPERDAY)
#define DAYSPERWEEK	7
#define MONSPERYEAR	12

#define YEAR_BASE	1900
#define EPOCH_YEAR      1970
#define EPOCH_WDAY      4

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

static const int mon_lengths[2][MONSPERYEAR] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static const int year_lengths[2] = {
	365,
	366
};



typedef struct {
	int tm_sec;	    /* seconds after the minute - [0,59] */
	int tm_min;	    /* minutes after the hour - [0,59] */
	int tm_hour;	/* hours since midnight - [0,23] */
	int tm_mday;	/* day of the month - [1,31] */
	int tm_mon;	    /* months since January - [0,11] */
	int tm_year;	/* years since 1900 */
	int tm_wday;	/* days since Sunday - [0,6] */
	int tm_yday;	/* days since January 1 - [0,365] */
	int tm_isdst;	/* daylight savings time flag */
} SsTmT;

typedef struct {
	int tm_sec;	    /* seconds after the minute - [0,59] */
	int tm_min;	    /* minutes after the hour - [0,59] */
	int tm_hour;	/* hours since midnight - [0,23] */
	int tm_mday;	/* day of the month - [1,31] */
	int tm_mon;	    /* months since January - [0,11] */
	int tm_year;	/* years since 1900 */
} SsSimpleTmT;

#define SS_TIME_T_YEARS          (2106 - 1970 + 1)

typedef unsigned long SsTimeT;

SsTimeT SsTime(SsTimeT* p_time);
#define SS_CTIME_BUFLEN          26 /* buf used in SsCtime must be at least 26 */
#define SS_ASCTIME_BUFLEN        26 /* buf used in SsASCtime must be at least 26 */
char*   SsCtime(SsTimeT* p_time, char* buf, int buflen);
void    SsLocaltime(SsTmT* p_sstm, SsTimeT sstime);
SsTimeT SsMktime(SsTmT* p_sstm);
SsTimeT SsTimeMs(void);
SsTimeT SsTimeMsS(SsTimeT* timep);
double  SsTimeStamp(void);

void SsTimeSetZeroTimeTGapFrom1970(unsigned long new_timet_gap);

void SsPrintDateTime(char* buf, int bufsize, SsTimeT tim);
void SsPrintDateTime2(char* buf, int bufsize, int fractionprec);

SsSimpleTmT SsTmtToSimpleTmT(SsTmT *TmT);
char* SsAsctime (SsTmT *tim_p, char *buf, int buflen);

extern int const ss_time_monthdaysum[2][12+1];
extern int const ss_time_daysinmonths[2][12];

/* Note: year is the real year number > 0 (preferably unsigned) */
#define SsTimeLeapsBeforeThis(year) \
(((year) - 1U) / 4U - ((year) - 1U) / 100U + ((year) - 1U) / 400U)

/* Note: year is the real year number > 0 (preferably unsigned) */
#define SsTimeIsLeapYear(year) \
        (SsTimeLeapsBeforeThis((year)+1) - SsTimeLeapsBeforeThis(year))

/* Year is real year number > 0, month is [1,12] */
#define SsTimeDaysInMonth(year, month) \
        (ss_time_daysinmonths[SsTimeIsLeapYear(year)][(month)-1])

/* Year is real year number > 0, month is [1,12] */
#define SsTimeDayOfYear(year, month, mday) \
        (ss_time_monthdaysum[SsTimeIsLeapYear(year)][(month)-1] + mday)

SsTimeT SsMktimeGMT(SsSimpleTmT* p_sstm);
void SsGmtime(SsSimpleTmT* p_sstm, SsTimeT timet);

bool SsTimeIsLegalTime(
        uint hour,
        uint minute,
        uint second,
        ss_uint4_t fraction);

bool SsTimeIsLegalDate(
        int year,
        uint month,
        uint mday);

int SsTimeCmp(SsTimeT t1, SsTimeT t2);

#ifdef SS_TIME_HIRES

#ifdef SS_NT

#include "ssint8.h"

typedef ss_int8_t SsTimeHiResT;

#else /* !SS_NT */

typedef struct {
        long        hr_sec;
        long        hr_usec;
} SsTimeHiResT;

#endif /* !SS_NT */

SsTimeT SsTimeHiResDiff(SsTimeHiResT hr1, SsTimeHiResT hr2);

SsTimeHiResT SsTimeHiRes(void);

#endif /* SS_TIME_HIRES */

#endif /* SSTIME_H */
