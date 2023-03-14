/*************************************************************************\
**  source       * sstime.c
**  directory    * ss
**  description  * Portable time functions.
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

#include <time.h>
#if defined(SS_LINUX) || defined (SS_FREEBSD) 
#  include <sys/time.h>
#endif
#include <sys/types.h>

#include <sys/timeb.h>

#include "ssstring.h"

#include "ssc.h"
#include "ssdebug.h"
#include "sssem.h"
#include "sstime.h"
#include "sssprint.h"

#if defined(SS_NT)
#define ftime	_ftime
#endif

#if defined(MSC) && MSC == 70

static ulong timet_gap = 2209017600L + 16L * 60L * 60L;

SsTimeT SsTime(SsTimeT* p_time)
{
        SsTimeT now;

        now = (SsTimeT)time(NULL);

        now -= timet_gap;
        if (p_time != NULL) {
            *p_time = now;
        }

        return(now);
}

char* SsCtime(SsTimeT* p_time, char* buf, int buflen)
{
        time_t msc_time;

        ss_dassert(p_time != NULL);

        msc_time = (time_t)*p_time + timet_gap;
#ifdef SS_NT64
        strcpy(buf, _ctime32(&msc_time));
#else
        strcpy(buf, ctime(&msc_time));
#endif
        return(buf);
}

void SsLocaltime(SsTmT* p_sstm, SsTimeT sstimet)
{
        time_t msc_time;
        struct tm res;
        struct tm* ptm;
        msc_time = (time_t)sstimet + timet_gap;
#if defined (SS_UNIX) && defined (SS_MT)
        ptm = localtime_r(&msc_time,&res);
        ss_dassert(ptm == &res);
#else /* SS_UNIX && SS_MT */
        SsSemEnter(ss_lib_sem);
#ifdef SS_NT64
        ptm = _localtime32(&msc_time);
#else
        ptm = localtime(&msc_time);
#endif
        ss_dassert(ptm != NULL);
        res = *ptm;
        SsSemExit(ss_lib_sem);
#endif
        p_sstm->tm_sec = res.tm_sec;
        p_sstm->tm_min = res.tm_min;
        p_sstm->tm_hour = res.tm_hour;
        p_sstm->tm_mday = res.tm_mday;
        p_sstm->tm_mon = res.tm_mon;
        p_sstm->tm_year = res.tm_year;
        p_sstm->tm_wday = res.tm_wday;
        p_sstm->tm_yday = res.tm_yday;
        p_sstm->tm_isdst = res.tm_isdst;
}


void SsTimeSetZeroTimeTGapFrom1970(ulong new_timet_gap)
{
        timet_gap = new_timet_gap;
}

#else /* MSC == 70 */

static ulong timet_gap = 0L;

/*##**********************************************************************\
 *
 *		SsTime
 *
 * Replacement for SsTime.
 *
 * Parameters :
 *
 *	p_time - out
 *		If non-null, current time in seconds is stored into
 *		*p_time.
 *
 * Return value :
 *
 *      Current time in seconds.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SsTimeT SsTime(SsTimeT* p_time)
{
#ifdef SS_NT64
        SsTimeT t = (SsTimeT)_time32((__time32_t*)p_time);
#else
        SsTimeT t = (SsTimeT)time((time_t*)p_time);
#endif

        FAKE_CODE_BLOCK_GT(
            FAKE_SS_SSTIMECHANGED, 0,
            {
                static int last_offset = 0;
                if (last_offset != (int)fake_cases[FAKE_SS_SSTIMECHANGED]) {
                    last_offset = (int)fake_cases[FAKE_SS_SSTIMECHANGED];
                    printf("FAKE_SS_SSTIMECHANGED:%d\n", last_offset);
                }
                t += last_offset;
            }
        );
        FAKE_CODE_BLOCK(
            FAKE_SS_SSTIMECHANGEDCOUNTER,
            {
                static bool printed;
                if (!printed) {
                    printed = TRUE;
                    printf("FAKE_SS_SSTIMECHANGEDCOUNTER\n");
                }
                t += 60;
            }
        );

        return (t);
}

/*##**********************************************************************\
 *
 *		SsCtime
 *
 * Replacement for ctime().
 *
 * Parameters :
 *
 *	p_time - in
 *		Time value in seconds.
 *	buf - in
 *              Buffer to store the result of ctime_r()
 *      buflen - in
 *              Length of buf. Must be at least 26.
 *
 * Return value - give :
 *
 *      Pointer to local character buffer.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* SsCtime(SsTimeT* p_time, char* buf, int buflen __attribute__ ((unused)))
{

        ss_dassert(p_time != NULL);
        ss_dassert(buf != NULL);
        ss_dassert(buflen >= SS_CTIME_BUFLEN);

#if defined (SS_UNIX) && defined (SS_MT)

#if defined (SS_LINUX) || defined (SS_FREEBSD) 
        ctime_r((time_t*)p_time,buf);
        return(buf);
#else
        ctime_r((time_t*)p_time,buf,buflen);
        return(buf);
#endif /* defined (SS_LINUX) */
#else /* SS_UNIX && SS_MT */
#ifdef SS_NT64
        strcpy(buf, _ctime32((__time32_t*)p_time));
#else
        strcpy(buf, ctime((time_t*)p_time));
#endif
        return(buf);
#endif  /* defined (SS_UNIX) && defined (SS_MT) */

}

/*##**********************************************************************\
 *
 *		SsLocaltime
 *
 * Replacement for localtime() with a different interface.
 * This version is fully reentrant.
 *
 * Parameters :
 *
 *      p_sstm - out, give
 *          SsTmT structure filled by this function.
 *
 *	timet - in
 *		Time value in seconds.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsLocaltime(SsTmT* p_sstm, SsTimeT sstimet)
{

#ifdef SS_NT64
        __time32_t timet = (__time32_t)sstimet;
#else
        time_t timet = (time_t)sstimet;
#endif
        struct tm* ptm;
        struct tm res;

#if defined (SS_UNIX) && defined (SS_MT)
        ptm = localtime_r(&timet,&res);
        ss_dassert(ptm == &res);
#else /* SS_UNIX && SS_MT */
        SsSemEnter(ss_lib_sem);
#ifdef SS_NT64
        ptm = _localtime32(&timet);
#else
        ptm = localtime(&timet);
#endif
        ss_dassert(ptm != NULL);
        res = *ptm;
        SsSemExit(ss_lib_sem);
#endif
        p_sstm->tm_sec = res.tm_sec;
        p_sstm->tm_min = res.tm_min;
        p_sstm->tm_hour = res.tm_hour;
        p_sstm->tm_mday = res.tm_mday;
        p_sstm->tm_mon = res.tm_mon;
        p_sstm->tm_year = res.tm_year;
        p_sstm->tm_wday = res.tm_wday;
        p_sstm->tm_yday = res.tm_yday;
        p_sstm->tm_isdst = res.tm_isdst;
}

void SsTimeSetZeroTimeTGapFrom1970(ulong new_timet_gap)
{
        SS_NOTUSED(new_timet_gap);
}

#endif /* MSC == 70 */

/*##**********************************************************************\
 *
 *		SsMktime
 *
 * Replacement for function mktime().
 *
 * Parameters :
 *
 *	p_sstm -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SsTimeT SsMktime(SsTmT* p_sstm)
{
        SsTimeT now;
        struct tm tmptm;
#ifdef SS_NT64
        __time32_t t;
        t = _time32(NULL);
#else
        time_t t;
        t = time(NULL);
#endif
#if defined (SS_UNIX) && defined (SS_MT)
        localtime_r(&t,&tmptm);
#else /* SS_UNIX && SS_MT */
        SsSemEnter(ss_lib_sem);
#ifdef SS_NT64
        tmptm = *_localtime32(&t);
#else
        tmptm = *localtime(&t);
#endif
        SsSemExit(ss_lib_sem);
#endif
        tmptm.tm_sec  = p_sstm->tm_sec;
        tmptm.tm_min  = p_sstm->tm_min;
        tmptm.tm_hour = p_sstm->tm_hour;
        tmptm.tm_mday = p_sstm->tm_mday;
        tmptm.tm_mon  = p_sstm->tm_mon;
        tmptm.tm_year = p_sstm->tm_year;
#ifdef SS_NT64
        t = _mktime32(&tmptm);
#else
        t = mktime(&tmptm);
#endif
        now = (SsTimeT)(t - timet_gap);
        return (now);
}

/*##**********************************************************************\
 *
 *		SsTimeMs
 *
 * Returns current time in milliseconds. Uses ftime.
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SsTimeT SsTimeMs(void)
{
        SsTimeT     sec, msec;
#ifdef SS_FAKE
        static SsTimeT  diff = 0;
#endif

#if defined(UNIX)

        /* gettimeofday should work just fine here.  Feel free to flame me
           if it doesn't.
           2002-08-02 apl
        */
        struct timeval tv;

        /* Should really check the return value here. */
        gettimeofday(&tv, NULL);

        sec = tv.tv_sec;
        msec = tv.tv_usec / 1000UL;

#elif defined(DOS) || defined(WINDOWS) || defined(SS_NT)


#ifdef SS_NT64
        struct __timeb32 time;
        _ftime32(&time);
#else
        struct _timeb time;
        ftime(&time);
#endif

        sec = time.time;
        msec = time.millitm;
#else
        Error! Unknown system;
#endif
        FAKE_CODE_BLOCK(
                FAKE_SS_TIME_AT_WRAPAROUND,
                {
                    if (diff == 0) {
                        diff = sec - 1065151870UL;
                    }

                    sec -= diff;
                    
                    SsPrintf("now %lu, %08lx\n", sec, sec * 1000);
                });
        
        return sec * 1000UL + msec;
}

#ifdef SS_TIME_HIRES

#if defined SS_NT
#include "sswindow.h"
#endif

/*##**********************************************************************\
 *
 *		SsTimeHiResDiff
 *
 * Returns difference in microseconds between two high resolution
 * timer values. Never use the timer value directly, but use this function
 * to compare the values.
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */

SsTimeT SsTimeHiResDiff(SsTimeHiResT hr1, SsTimeHiResT hr2)
{
        SsTimeT diff;

#if !defined(SS_NT)

        diff = (hr2.hr_sec - hr1.hr_sec) * 1000 * 1000
             + hr2.hr_usec - hr1.hr_usec;

#else /* SS_NT */

        {
            __int64 h1, h2;
            __int64 freq = 0;
            QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
            h1 = SsInt8GetNativeInt8(hr1);
            h2 = SsInt8GetNativeInt8(hr2);
            diff = (SsTimeT)((h2 - h1) / freq);
        }

#endif

        return (diff);
}
/*##**********************************************************************\
 *
 *		SsTimeHiRes
 *
 * Returns high resolution timer value. The timer value cannot be
 * used directly, but you can get the difference in microseconds between
 * two values using SsTimeHiResDiff.
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */

SsTimeHiResT SsTimeHiRes(void)
{
        SsTimeHiResT hr;

#if defined(UNIX) 

        {
            struct timeval tv;
            int i;

            i = gettimeofday(&tv, NULL);
            ss_dassert(i == 0);

            hr.hr_sec = tv.tv_sec;
            hr.hr_usec = tv.tv_usec;
        }

#elif defined(SS_NT)

        {
            BOOL b;
            __int64 i;
            b = QueryPerformanceCounter((LARGE_INTEGER*)&i);
            SsInt8SetNativeInt8(&hr, i);
            ss_dassert(b);
        }

#elif defined(DOS) || defined(WINDOWS)

        {
            struct timeb time;
#ifdef SS_NT64
            _ftime32(&time);
#else
            ftime(&time);
#endif
            hr.hr_sec = time.time;
            hr.hr_usec = time.millitm * 1000;
        }
#else
         {
             hr.hr_sec = SsTime(NULL);
             hr.hr_usec = 0L;
         }
#endif

        return (hr);
}
#endif /* SS_TIME_HIRES */

/*##**********************************************************************\
 *
 *		SsTimeMsS
 *
 * Returns milliseconds part of current time and assigns current time as seconds
 * to the parameter. Uses ftime.
 *
 * Parameters : 	 - timep (current time as seconds will be assigned to this)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SsTimeT SsTimeMsS(SsTimeT* timep)
{
        SsTimeT     msec;
#if defined(UNIX)

        /* gettimeofday should work just fine here.  Feel free to flame me
           if it doesn't.
           2002-08-02 apl
        */
        struct timeval tv;

        /* Should really check the return value here. */
        gettimeofday(&tv, NULL);

		*timep = tv.tv_sec;
        msec = tv.tv_usec / 1000UL;

#elif defined(DOS) || defined(WINDOWS) || defined(SS_NT)


#ifdef SS_NT64
        struct __timeb32 time;
        _ftime32(&time);
#else
        struct _timeb time;
        ftime(&time);
#endif
        *timep = (SsTimeT)time.time;
        msec = (SsTimeT)time.millitm;
#else
        Error! Unknown system.
#endif
        return msec;
}

/*##**********************************************************************\
 *
 *		SsTimeStamp
 *
 *
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
double SsTimeStamp(void)
{
#if defined(UNIX) 

        return((double)SsTime(NULL));

#elif defined(DOS) || defined(WINDOWS) || defined(SS_NT)

#ifdef SS_NT64
        struct __timeb32 time;
        _ftime32(&time);
#else
        struct _timeb time;
        ftime(&time);
#endif
        return((double)time.time + (double)time.millitm / 1000.0);
#else
        Error! Unknown system.
#endif
}

/*##**********************************************************************\
 *
 *		SsPrintDateTime
 *
 * Prints date and time in standard Solid format.
 *
 * Parameters :
 *
 *	buf -
 *
 *
 *	bufsize -
 *
 *
 *	tim -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsPrintDateTime(char* buf, int bufsize, SsTimeT tim)
{
        char buf_time[80];
        SsTmT tm;
        
        ss_dassert(buf != NULL);
        ss_dassert(bufsize > 0);

        SsLocaltime(&tm, tim);
        SsSprintf(buf_time, "%02d.%02d %02d:%02d:%02d",
            tm.tm_mday, tm.tm_mon+1, tm.tm_hour, tm.tm_min, tm.tm_sec);
        strncpy(buf, buf_time, SS_MIN((size_t)bufsize, sizeof(buf_time)));
        buf[bufsize-1] = '\0';
}

void SsPrintDateTime2(char* buf, int bufsize, int fractionprec)
{
        char buf_time[80];
        SsTmT tm;
        SsTimeT seconds;
        SsTimeT fraction;

        ss_dassert(buf != NULL);
        ss_dassert(bufsize > 0);
    
        fraction = SsTimeMsS(&seconds);
		SsLocaltime(&tm, seconds);

        if (fractionprec == 0) {
            SsSprintf(buf_time, "%02d.%02d %02d:%02d:%02d",
                tm.tm_mday, tm.tm_mon+1, tm.tm_hour, tm.tm_min, tm.tm_sec);
		} else if (fractionprec == 1) {
            SsSprintf(buf_time, "%02d.%02d %02d:%02d:%02d.%01d",
                tm.tm_mday, tm.tm_mon+1, tm.tm_hour, tm.tm_min, tm.tm_sec, fraction * 10 / 1000);
		} else if (fractionprec == 2) {
            SsSprintf(buf_time, "%02d.%02d %02d:%02d:%02d.%02d",
                tm.tm_mday, tm.tm_mon+1, tm.tm_hour, tm.tm_min, tm.tm_sec, fraction * 100 / 1000);
		} else {
            SsSprintf(buf_time, "%02d.%02d %02d:%02d:%02d.%03d",
                tm.tm_mday, tm.tm_mon+1, tm.tm_hour, tm.tm_min, tm.tm_sec, fraction);
		}

        strncpy(buf, buf_time, SS_MIN((size_t)bufsize, sizeof(buf_time)));
        buf[bufsize-1] = '\0';
}

#define SS_RAWDATE
#ifdef SS_RAWDATE

#define DATE_YEAR_DAY(year_since_70) \
((year_since_70) * 365L +\
 SsTimeLeapsBeforeThis((year_since_70)+1970) - SsTimeLeapsBeforeThis(1970))

int const ss_time_monthdaysum[2][12+1] = {
        { /* Non leap-year */
            0,
            31,
            31+28,
            31+28+31,
            31+28+31+30,
            31+28+31+30+31,
            31+28+31+30+31+30,
            31+28+31+30+31+30+31,
            31+28+31+30+31+30+31+31,
            31+28+31+30+31+30+31+31+30,
            31+28+31+30+31+30+31+31+30+31,
            31+28+31+30+31+30+31+31+30+31+30,
            9999 /* just end marker */
        },
        { /* year */
            0,
            31,
            31+29,
            31+29+31,
            31+29+31+30,
            31+29+31+30+31,
            31+29+31+30+31+30,
            31+29+31+30+31+30+31,
            31+29+31+30+31+30+31+31,
            31+29+31+30+31+30+31+31+30,
            31+29+31+30+31+30+31+31+30+31,
            31+29+31+30+31+30+31+31+30+31+30,
            9999 /* just end marker */
        }
};

int const ss_time_daysinmonths[2][12] = {
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
            { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

/*##**********************************************************************\
 *
 *		SsMktimeGMT
 *
 *
 *
 * Parameters :
 *
 *	p_sstm -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SsTimeT SsMktimeGMT(SsSimpleTmT* p_sstm)
{
        SsTimeT t;
        int years_since_70;
        long days;

        int year    = p_sstm->tm_year + 1900;
        int month   = p_sstm->tm_mon + 1;
        int mday    = p_sstm->tm_mday;
        int hour    = p_sstm->tm_hour;
        int min     = p_sstm->tm_min;
        int sec     = p_sstm->tm_sec;

        ss_dassert(1 <= month && month <= 12);
        ss_dassert(1 <= mday && mday <= SsTimeDaysInMonth((uint)year, month));

        if (year < 1970 || (year-1970) >= SS_TIME_T_YEARS) {
            return ((SsTimeT)-1L);
        }
        years_since_70 = (uint)year - 1970;
        days = DATE_YEAR_DAY((uint)years_since_70)
             + (SsTimeDayOfYear((uint)year, month, mday) - 1);
        t = (ulong)days * (24UL * 60UL * 60UL) +
            ((ulong)hour * 60UL + (ulong)min) * 60UL + (ulong)sec;
        return (t);
}

/*##**********************************************************************\
 *
 *		SsGmtime
 *
 *
 *
 * Parameters :
 *
 *	p_sstm -
 *
 *
 *	timet -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsGmtime(SsSimpleTmT* p_sstm, SsTimeT timet)
{
        ulong t;
        int year;
        int month;
        int mday;
        int hour;
        int min;
        int sec;


        t = timet;
        sec = t % (uint)60;
        t /= (uint)60;
        min = t % (uint)60;
        t /= (uint)60;
        hour = t % (uint)24;
        t /= (uint)24;
        {
            int lo;
            int hi;
            int try;
            const int* p;
            ulong yday;

            lo = 0;
            hi = SS_TIME_T_YEARS - 1;
            for (;;) {
                try = (hi + lo) / 2;
                yday = DATE_YEAR_DAY((uint)try);
                if (yday > t) {
                    hi = try - 1;
                } else {
                    int try1 = try + 1;
                    if (DATE_YEAR_DAY((uint)try1) <= t) {
                        lo = try1;
                    } else {
                        break;
                    }
                }
            }
            year = try + 1970;
            t -= yday;

            p = ss_time_monthdaysum[SsTimeIsLeapYear((uint)year)];
            lo = 0;
            hi = 11;
            for (;;) {
                try = (hi + lo) / 2;
                if (p[try] > (long)t) {
                    hi = try - 1;
                } else if (p[try + 1] <= (long)t) {
                    lo = try + 1;
                } else {
                    break;
                }
            }
            month = try + 1;
            mday = t - p[try] + 1;
        }
        p_sstm->tm_year = year - 1900;
        p_sstm->tm_mon  = month - 1;
        p_sstm->tm_mday = mday;
        p_sstm->tm_hour = hour;
        p_sstm->tm_min  = min;
        p_sstm->tm_sec  = sec;
}

#endif /* SS_RAWDATE */

bool SsTimeIsLegalTime(
        uint hour,
        uint minute,
        uint second,
        ss_uint4_t fraction)
{
        return (hour < 24U &&
                minute < 60U &&
                second < 60U &&
                fraction < 1000000000UL);
}

bool SsTimeIsLegalDate(
        int year,
        uint month,
        uint mday)
{
        if (year == 0 || year > 32767 || year < -32767) {
            return (FALSE);
        }
        if ((month - 1U) > (12U - 1U)) {
            return (FALSE);
        }
        if (year < 0) {
            /* calculate a positive equivalent of
               year for leap-year calculation
            */
            int incr = -(year + 1);
            incr /= 400;
            incr++;
            incr *= 400;
            year += 1 + incr;
        }
        ss_dassert(year > 0);
        if ((mday - 1U) >= (uint)SsTimeDaysInMonth(year, month)) {
            return (FALSE);
        }
        return (TRUE);
}


/*##**********************************************************************\
 *
 *		SsAsctime
 *
 * Replacement for asctime().
 *
 * Author : Mika H
 *
 * Parameters :
 *
 *	tim_p - in, use
 *		Time value in seconds.
 *	buf - in
 *              Buffer to store the result of ctime_r()
 *  buflen - in
 *              Length of buf. Must be at least 26.
 *
 * Return value - give :
 *
 *      Pointer to local character buffer.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* SsAsctime (SsTmT *tim_p, char *buf, int buflen __attribute__ ((unused)))
{
        static const char day_name[7][3] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };
        static const char mon_name[12][3] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };

        ss_dassert(tim_p != NULL);
        ss_dassert(buf != NULL);
        ss_dassert(buflen >= SS_ASCTIME_BUFLEN);

        sprintf (buf, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
        day_name[tim_p->tm_wday],
        mon_name[tim_p->tm_mon],
        tim_p->tm_mday, tim_p->tm_hour, tim_p->tm_min,
        tim_p->tm_sec, 1900 + tim_p->tm_year);

        return buf;
}



/*##**********************************************************************\
 *
 *		SsTmtToSimpleTmT
 *
 * Converts SsTmT struct to SimpleTmT
 *
 * Parameters :
 *
 *	TmT - in, use
 *		Pointer to SsTmT struct to convert
 *
 * Return value :
 *
 *      SsSimpleTmT struct.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SsSimpleTmT SsTmtToSimpleTmT(SsTmT *TmT) {

        SsSimpleTmT SimpleTmT;

        SimpleTmT.tm_sec    = TmT->tm_sec;	    /* seconds after the minute - [0,59] */
        SimpleTmT.tm_min    = TmT->tm_min;	    /* minutes after the hour - [0,59] */
        SimpleTmT.tm_hour   = TmT->tm_hour;	    /* hours since midnight - [0,23] */
        SimpleTmT.tm_mday   = TmT->tm_mday;	    /* day of the month - [1,31] */
        SimpleTmT.tm_mon    = TmT->tm_mon;	    /* months since January - [0,11] */
        SimpleTmT.tm_year   = TmT->tm_year;	    /* years since 1900 */

        return SimpleTmT;
}

/*##**********************************************************************\
 *
 *      SsTimeCmp
 *
 * Compare to time values if the values can wrap over (ie., are in
 * milliseconds).
 *
 * Parameters:
 *      t1 - use
 *          Time 1.
 *
 *      t2 - use
 *          Time 2.
 *
 * Return value:
 *      < 0 if t1 < t2, 0 if t1 == t2, > 0 if t1 > t2.
 *
 * Limitations:
 *
 * Globals used:
 */
int SsTimeCmp(SsTimeT t1, SsTimeT t2)
{
        long diff;

        /* This is basically a copy of dbe_trxid_cmp. */
        diff = t1 - t2;

        /* compiler optimizes away one of the below if/else branches! */
        if (sizeof(int) >= sizeof(diff)) {
            return (diff);
        } else {
            /* 16-bit int ! */
            return (diff < 0 ? -1 : (diff == 0 ? 0 : 1));
        }
}
