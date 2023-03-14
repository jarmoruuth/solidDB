/*************************************************************************\
**  source       * dt0date.h
**  directory    * dt
**  description  * Date routines
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


#ifndef DT0DATE_H
#define DT0DATE_H

#include <sstime.h>
#include <ssc.h>
#include <uti0va.h>
#include "dt0type.h"

extern const char* dt_date_format;

#define DT_DATE_DELIMITER      '-'  /* 1993-12-12 */
#define DT_DATETIME_DELIMITER  ' '  /* Between date and time */
#define DT_TIME_DELIMITER      ':'  /* 12:12:12 */
#define DT_FRACT_DELIMITER     '.'  /* fractions of second delimiter */

typedef enum {
        DT_DATE_SQLDATE,
        DT_DATE_SQLTIME,
        DT_DATE_SQLTIMESTAMP,
        DT_DATE_SQLTYPE_UNKNOWN
} dt_datesqltype_t;

typedef enum {
        DT_DATE_IU_FRAC_SECOND,
        DT_DATE_IU_SECOND,  
        DT_DATE_IU_MINUTE,  
        DT_DATE_IU_HOUR, 
        DT_DATE_IU_DAY,
        DT_DATE_IU_WEEK,
        DT_DATE_IU_MONTH,
        DT_DATE_IU_QUARTER,
        DT_DATE_IU_YEAR
} dt_date_tsiunit_t;    /* Time stamp interval units */

typedef enum {
        DT_DATE_WD_SUN = 0,
        DT_DATE_WD_MON = 1,
        DT_DATE_WD_TUE = 2,
        DT_DATE_WD_WED = 3,
        DT_DATE_WD_THU = 4,
        DT_DATE_WD_FRI = 5,
        DT_DATE_WD_SAT = 6
} dt_date_weekday_t;

dt_date_t*  dt_date_init(void);
void        dt_date_done(dt_date_t* date);

bool dt_date_setdata(
        dt_date_t* date,
        int year,
        int mon,
        int mday,
        int hour,
        int min,
        int sec,
        long fraction);
bool dt_date_setasciiz(dt_date_t* dest_date, char* format, char* ext_datestr);
bool dt_date_setasciiz_ext(dt_date_t* dest_date, char* format,
                           char* ext_date, dt_datesqltype_t* p_datesqltype);
bool dt_date_setva(dt_date_t* dest_date, va_t* va);
bool dt_date_settimet(dt_date_t* dest_date, SsTimeT time);
bool dt_date_setnow(long precision, dt_date_t* d);
bool dt_date_setcurdate(dt_date_t* d);
bool dt_date_setcurtime(long precision, dt_date_t* d);

bool dt_date_datetoasciiz(dt_date_t* src_date, char* format, char* datestr);
bool dt_date_datetoasciiz_sql(dt_date_t* src_date, dt_datesqltype_t datesqltype,
                              char* datestr);
bool dt_date_datetova(dt_date_t* src_date, va_t* va);
bool dt_date_datetotimet(dt_date_t* src_date, SsTimeT* p_time);

int  dt_date_cmp(dt_date_t* date1, dt_date_t* date2);

int   dt_date_year(dt_date_t* date);
int   dt_date_month(dt_date_t* date);
int   dt_date_mday(dt_date_t* date);
int   dt_date_hour(dt_date_t* date);
int   dt_date_min(dt_date_t* date);
int   dt_date_sec(dt_date_t* date);
ulong dt_date_fraction(dt_date_t* date);

bool dt_date_istime(dt_date_t* d);
bool dt_date_padtimewithcurdate(dt_date_t* d);
bool dt_date_truncatetodate(dt_date_t* d);
bool dt_date_truncatetotime(dt_date_t* d);

dt_date_weekday_t dt_date_weekday(dt_date_t* d);
int dt_date_week(dt_date_t* d);
int dt_date_dayofyear(dt_date_t* d);

bool dt_date_tsadd(
        dt_date_tsiunit_t unit,
        long val,
        dt_date_t* src_d,
        dt_date_t* dest_d);

bool dt_date_tsdiff_old(
        dt_date_tsiunit_t unit,
        dt_date_t* d1,
        dt_date_t* d2,
        ss_int8_t* p_result);

bool dt_date_tsdiff_new(
        dt_date_tsiunit_t unit,
        dt_date_t* d1,
        dt_date_t* d2,
        ss_int8_t* p_result);

/* Raw time_t conversions without time zone / dst information included */
bool dt_date_datetotimet_raw(dt_date_t* src_date, SsTimeT* p_time);
bool dt_date_settimet_raw(dt_date_t* dest_date, SsTimeT timet);

bool dt_date_islegal(dt_date_t* date);

bool dt_date_islegalyearmonthday(dt_date_t* date);

bool dt_date_substract(
        dt_date_t* date1,
        dt_date_t* date2,
        ss_int8_t* p_diff);

#endif /* DT0DATE_H */
