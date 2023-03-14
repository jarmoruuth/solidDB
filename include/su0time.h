/*************************************************************************\
**  source       * su0time.h
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


#ifndef SU0TIME_H
#define SU0TIME_H

typedef struct {
	int  tm_running;
	long tm_time;
        char tm_str[25];
} su_timer_t;

#if defined(WINDOWS) || defined(SS_NT)
typedef struct {
        ss_debug(int chk;)
        __int64      start;
        __int64      freq;
        __int64      count;
        int          running;
} su_high_resolution_timer_t;
#endif /* WINDOWS */

void su_timer_reset(
        su_timer_t* timer);

void su_timer_restart(
        su_timer_t* timer);

void su_timer_start(
        su_timer_t* timer);

void su_timer_stop(
        su_timer_t* timer);

long su_timer_read(
        su_timer_t* timer);

double su_timer_readf(
        su_timer_t* timer);

char* su_timer_readstr(
        su_timer_t* timer);

#if defined(WINDOWS) || defined(SS_NT)
void su_high_resolution_timer_start(
        su_high_resolution_timer_t* timer);

long su_high_resolution_timer_stop(
        su_high_resolution_timer_t* timer);

void su_high_resolution_timer_reset(
        su_high_resolution_timer_t* timer);

double su_high_resolution_timer_readf(
        su_high_resolution_timer_t* timer);
#endif /*WINDOWS*/

#ifdef SU_GENERIC_TIMER

typedef enum {
        SU_GENERIC_TIMER_SP_SQLFETCH,
        SU_GENERIC_TIMER_TAB_SQLFETCH,
        SU_GENERIC_TIMER_DBE_CURSORFETCH,
        SU_GENERIC_TIMER_DBE_INDSEAFETCH,
        SU_GENERIC_TIMER_DBE_BTREEFETCH,
        SU_GENERIC_TIMER_DBE_CHECKCONS,
        SU_GENERIC_TIMER_MAX
} su_generic_timer_t;

#define SU_GENERIC_TIMER_START(n) if (su_generic_timer_active) {\
                                        if (!su_generic_timer_init[n]) {\
                                                su_high_resolution_timer_reset(&su_generic_timer[n]);\
                                                su_generic_timer_init[n] = 1;\
                                                su_generic_counter[n] = 0;\
                                        }\
                                        su_high_resolution_timer_start(&su_generic_timer[n]);\
                                        su_generic_counter[n]++;\
                                  }
#define SU_GENERIC_TIMER_STOP(n)  if (su_generic_timer_active) {\
                                        if (su_generic_timer_init[n]) {\
                                                su_high_resolution_timer_stop(&su_generic_timer[n]);\
                                        }\
                                  }

extern bool                       su_generic_timer_active;
extern su_high_resolution_timer_t su_generic_timer[SU_GENERIC_TIMER_MAX];
extern long                       su_generic_counter[SU_GENERIC_TIMER_MAX];
extern bool                       su_generic_timer_init[SU_GENERIC_TIMER_MAX];

#else  /* SU_GENERIC_TIMER */

#define SU_GENERIC_TIMER_START(n)
#define SU_GENERIC_TIMER_STOP(n)

#endif /* SU_GENERIC_TIMER */

#endif /* SU0TIME_H */
