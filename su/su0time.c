/*************************************************************************\
**  source       * su0time.c
**  directory    * su
**  description  * Timer functions. Timer works in millisecond precision.
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


#include <ssstdio.h>
#include <sstime.h>

#include <ssc.h>
#include <ssdebug.h>
#include <sssprint.h>
#include <ssdtoa.h>

#include "su0time.h"
#if defined(WINDOWS) || defined(SS_NT)
#include "sswindow.h"
#endif

#ifdef SU_GENERIC_TIMER
bool                       su_generic_timer_active;
su_high_resolution_timer_t su_generic_timer[SU_GENERIC_TIMER_MAX];
long                       su_generic_counter[SU_GENERIC_TIMER_MAX];
bool                       su_generic_timer_init[SU_GENERIC_TIMER_MAX];
#endif /* SU_GENERIC_TIMER */

static long get_time_in_milliseconds(void);
static long correction = 0;

/*#**********************************************************************\
 *
 *		get_time_in_milliseconds
 *
 * Returns current time in milliseconds. Uses SsTimeMs().
 *
 * Input params : none
 *
 * Output params:
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static long get_time_in_milliseconds()
{
        return(SsTimeMs());
}

/*##**********************************************************************\
 *
 *		su_timer_reset
 *
 * Resets timer. After this call timer is zero and stopped.
 *
 * Input params :
 *	timer	- pointer to the timer structure
 *
 * Output params:
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void su_timer_reset(timer)
        su_timer_t* timer;
{
    timer->tm_time = 0;
    timer->tm_running = 0;
}

/*##**********************************************************************\
 *
 *		su_timer_start
 *
 * Resets timer and starts it.
 *
 * Input params :
 *	timer	- pointer to the timer structure
 *
 * Output params:
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void su_timer_start(timer)
        su_timer_t* timer;
{
    su_timer_reset(timer);
    su_timer_restart(timer);
}

/*##**********************************************************************\
 *
 *		su_timer_restart
 *
 * Restarts reset or stopped timer.
 *
 * Input params :
 *	timer	- pointer to the timer structure
 *
 * Output params:
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void su_timer_restart(timer)
        su_timer_t* timer;
{
    static int correction_initializedp = 0;
    su_timer_t t;

    if (!correction_initializedp) {
        correction_initializedp = 1;
        su_timer_reset(&t);
        su_timer_start(&t);
        su_timer_stop(&t);
        correction = su_timer_read(&t);
    }
    if (timer->tm_running) {
            return;
        }

    timer->tm_running = 1;
    timer->tm_time = get_time_in_milliseconds() - timer->tm_time;
}

/*##**********************************************************************\
 *
 *		su_timer_stop
 *
 * Stops a running timer
 *
 * Input params :
 *	timer	- pointer to the timer structure
 *
 * Output params:
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void su_timer_stop(timer)
        su_timer_t* timer;
{
    if (!timer->tm_running) {
            return;
        }

    timer->tm_time = get_time_in_milliseconds() - correction -
                         timer->tm_time;

    if (timer->tm_time < 0) {
            timer->tm_time = 0;
        }
    timer->tm_running = 0;
}

/*##**********************************************************************\
 *
 *		su_timer_read
 *
 * Reads running or stopped timer. Returns elapsed time in
 * milliseconds.
 *
 * Input params :
 *	timer	- pointer to the timer structure
 *
 * Output params:
 *
 * Return value :
 *      elapsed time in milliseconds
 *
 * Limitations  :
 *
 * Globals used :
 */
long su_timer_read(timer)
        su_timer_t *timer;
{
    return(timer->tm_running
               ? get_time_in_milliseconds() - timer->tm_time
           : timer->tm_time);
}

/*##**********************************************************************\
 *
 *		su_timer_readf
 *
 * Reads running or stopped timer. Returns elapsed time in
 * seconds. Fractional part contains milliseconds.
 *
 * Input params :
 *	timer	- pointer to the timer structure
 *
 * Output params:
 *
 * Return value :
 *      elapsed time in seconds, fractional part contains milliseconds
 *
 * Limitations  :
 *
 * Globals used :
 */
double su_timer_readf(timer)
        su_timer_t* timer;
{
    return(((double)su_timer_read(timer)) / 1000 );
}


/*##**********************************************************************\
 *
 *		su_timer_readstr
 *
 * Reads running or stopped timer. Returns elapsed time in an asciiz string.
 * that contains seconds.fraction, where fraction is in ms
 * The fraction part is always 2 characters, except in SS_WIN where it
 * may sometimes bw only 1 char (I dunno why)
 *
 * Parameters :
 *
 *	timer -
 *
 *
 * Return value - ref:
 *
 *      Reference to a static buffer containing the timer value string
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* su_timer_readstr(
        su_timer_t* timer)
{
        double d;
        d = su_timer_readf(timer);
#ifdef SS_WIN
        /* Get the output with two decimals, if it doesn't happen to
         * be in e-format
         */
        SsDoubleToAsciiDecimals(d, timer->tm_str, 15, 2);
#else /* SS_WIN */
        SsSprintf(timer->tm_str, "%.2lf", d);
#endif /* SS_WIN */

        return(timer->tm_str);
}


#if defined(WINDOWS) || defined(SS_NT)

#define HRS_CHK 76454

/*##**********************************************************************\
 *
 *		su_high_resolution_timer_start
 *
 * Starts high resolution timer.
 *
 * Input params :
 *	timer	- pointer to the high resolution timer structure
 *
 * Output params:
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void su_high_resolution_timer_start(su_high_resolution_timer_t *timer)
{
        BOOL b;

        ss_dassert(timer->chk == HRS_CHK);
        ss_dassert(!timer->running);

        /* Get current counter value */
        b = QueryPerformanceCounter((LARGE_INTEGER*)&timer->start);
        ss_assert(b);
        ss_dassert(timer->start >= 0);

        timer->running = 1;
}

/*##**********************************************************************\
 *
 *		su_high_resolution_timer_stop
 *
 * Stops high resolution timer and increments elapsed time since previous
 * timer start.
 *
 * Input params :
 *	timer	- pointer to the high resolution timer structure
 *
 * Output params:
 *
 * Return value :
 *  long    - elapsed time as microseconds since previous start
 *
 * Limitations  :
 *
 * Globals used :
 */
long su_high_resolution_timer_stop(su_high_resolution_timer_t* timer)
{
        __int64 endcount;
        __int64 elapsed_time;

        ss_dassert(timer->chk == HRS_CHK);

        if (timer->running) {
            BOOL b;
            b = QueryPerformanceCounter((LARGE_INTEGER*)&endcount);
            ss_assert(b);
            ss_dassert(endcount >= 0);

            elapsed_time = endcount - timer->start;
            ss_dassert(elapsed_time > 0);
            timer->count += elapsed_time;

            timer->running = 0;
            return (long)elapsed_time;
        } else {
            return(0);
        }
}

/*##**********************************************************************\
 *
 *		su_high_resolution_timer_reset
 *
 * Stops high resolution timer and increments elapsed time since previous
 * timer start.
 *
 * Input params :
 *	timer	- pointer to the high resolution timer structure
 *
 * Output params:
 *
 * Return value :
 *  long    - elapsed time as microseconds since previous start
 *
 * Limitations  :
 *
 * Globals used :
 */
void su_high_resolution_timer_reset(su_high_resolution_timer_t* timer)
{
        ss_debug(timer->chk = HRS_CHK);
        timer->running = 0;
        timer->start = 0;
        timer->count = 0;

        /* Get counter frequency as counts per second */
        QueryPerformanceFrequency((LARGE_INTEGER*)&timer->freq);
        ss_dassert(timer->freq > 0);
}

/*##**********************************************************************\
 *
 *		su_high_resolution_timer_readf
 *
 * Reads running or stopped high resolution timer.
 *
 * Input params :
 *	timer	- pointer to the high resolution timer structure
 *
 * Output params:
 *
 * Return value :
 *  double  - elapsed time as seconds since previous timer start
 *
 * Limitations  :
 *
 * Globals used :
 */
double su_high_resolution_timer_readf(su_high_resolution_timer_t* timer)
{
        double seconds;

        ss_dassert(timer->chk == HRS_CHK);

        if (!timer->running) {
            seconds = (double)timer->count / (double)timer->freq;
        } else {
            BOOL b;
            __int64 endcount;
            __int64 elapsed_time;

            /* Get current counter value */
            b = QueryPerformanceCounter((LARGE_INTEGER*)&endcount);
            ss_assert(b);

            elapsed_time = endcount - timer->start;

            seconds = (double)elapsed_time / (double)timer->freq;
        }
        return seconds;
}

#endif /* (WINDOWS) || (SS_NT) */

