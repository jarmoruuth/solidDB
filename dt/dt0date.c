/*************************************************************************\
**  source       * dt0date.c
**  directory    * dt
**  description  * Date data type routines 
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

#include <ssctype.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <sssprint.h>
#include <sslimits.h>
#include <ssscan.h>
#include <sschcvt.h>
#include <ssmath.h>

#include <uti0va.h>

#include "dt0date.h"

#ifdef EXAMPLE
struct tm
{
  int   tm_sec;
  int   tm_min;
  int   tm_hour;
  int   tm_mday;
  int   tm_mon;
  int   tm_year;
  int   tm_wday;
  int   tm_yday;
  int   tm_isdst;
};
#endif /* EXAMPLE */

/* Only supported format for now */
const char* dt_date_format = "YYYY-MM-DD HH:NN:SS";

#define DATE_YEAROFFSET     0   /* Year, 2 bytes. 1993 = [19][93] */
#define DATE_MONTHOFFSET    2   /* Month, 1 byte. [1..12] */
#define DATE_MDAYOFFSET     3   /* Day of the month, 1 byte. [1..31] */
#define DATE_HOUROFFSET     4   /* Hour   , 1 byte. [0..23] */
#define DATE_MINOFFSET      5   /* Minute , 1 byte. [0..59] */
#define DATE_SECOFFSET      6   /* Second , 1 byte. [0..59] */
#define DATE_FRACTOFFSET    7   /* Fractions of second 4 bytes [0..999999999] */

/* drift added to when storing or subtracted when loading
 * a year value to make negative year values compare correctly
 */
#define DATE_YEARDRIFT      ((unsigned short)32768L)
#define DATE_YEARMAX        (32767)
#define DATE_YEARMIN        (-32768)

#define DATE_SETFRACTION(date, fract) \
        SS_RETYPE(char *, date)[DATE_FRACTOFFSET + 0]= (char)((fract) >> (CHAR_BIT*3)); \
        SS_RETYPE(char *, date)[DATE_FRACTOFFSET + 1]= (char)((fract) >> (CHAR_BIT*2)); \
        SS_RETYPE(char *, date)[DATE_FRACTOFFSET + 2]= (char)((fract) >> (CHAR_BIT*1)); \
        SS_RETYPE(char *, date)[DATE_FRACTOFFSET + 3]= (char)(fract)

#define DATE_SETDATA(date, yyyy, mm, dd, hh, mi, se, fract) \
        SS_RETYPE(char *, date)[DATE_YEAROFFSET]     = (char)((yyyy) >> CHAR_BIT); \
        SS_RETYPE(char *, date)[DATE_YEAROFFSET + 1] = (char)(yyyy); \
        SS_RETYPE(char *, date)[DATE_MONTHOFFSET]    = (char)(mm); \
        SS_RETYPE(char *, date)[DATE_MDAYOFFSET]     = (char)(dd); \
        SS_RETYPE(char *, date)[DATE_HOUROFFSET]     = (char)(hh); \
        SS_RETYPE(char *, date)[DATE_MINOFFSET]      = (char)(mi); \
        SS_RETYPE(char *, date)[DATE_SECOFFSET]      = (char)(se); \
        DATE_SETFRACTION(date, fract)

typedef enum {
        FMT_YEAR,
        FMT_YEAR_1900,
        FMT_MON,
        FMT_MDAY,
        FMT_HOUR,
        FMT_MIN,
        FMT_SEC,
        FMT_FRACT,
        FMT_LAST
} format_t;

static const struct {
        format_t    fmt_type;
        const char* fmt_str;
        int         fmt_len;
} fmt[] = {
        { FMT_YEAR,     "YYYY", 4 },
        { FMT_YEAR_1900,"YY",   2 },
        { FMT_MON,      "MM",   2 },
        { FMT_MON,      "M",    1 },
        { FMT_MDAY,     "DD",   2 },
        { FMT_MDAY,     "D",    1 },
        { FMT_HOUR,     "HH",   2 },
        { FMT_HOUR,     "H",    1 },
        { FMT_MIN,      "NN",   2 },
        { FMT_MIN,      "N",    1 },
        { FMT_SEC,      "SS",   2 },
        { FMT_SEC,      "S",    1 },
        { FMT_FRACT,    "FFFFFFFFF",    9 },
        { FMT_FRACT,    "FFFFFFFF",    8 },
        { FMT_FRACT,    "FFFFFFF",    7 },
        { FMT_FRACT,    "FFFFFF",    6 },
        { FMT_FRACT,    "FFFFF",    5 },
        { FMT_FRACT,    "FFFF",    4 },
        { FMT_FRACT,    "FFF",    3 },
        { FMT_FRACT,    "FF",    2 },
        { FMT_FRACT,    "F",    1 }
};

#define FRACT_MAXDIGITS 9

static long const fractfmt_coeff[FRACT_MAXDIGITS] = {
        100000000L, /* 1 */
        10000000L,  /* 2 */
        1000000L,   /* 3 */
        100000L,    /* 4 */
        10000L,     /* 5 */
        1000L,      /* 6 */
        100L,       /* 7 */
        10L,        /* 8 */
        1L          /* 9 */
};

#define FRACT_COEFF(l)  ((fractfmt_coeff-1)[l])

static bool date_islegalymd(
        int year,
        int month,
        int day
);

static bool date_islegalhmsf(
        int hour,
        int minute,
        int sec,
        ulong fract
);

static int date_findformat(char* s)
{
        int i;

        for (i = 0; (size_t)i < sizeof(fmt)/sizeof(fmt[0]); i++) {
            if (SsMemcmp(s, fmt[i].fmt_str, fmt[i].fmt_len) == 0) {
                /* Found a match. */
                return(i);
            }
        }
        return(-1);
}

/*##**********************************************************************\
 * 
 *              dt_date_islegal
 * 
 * Checks that individual fields of dt_date_t are legal.
 * 
 * Parameters : 
 * 
 *      date - 
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
bool dt_date_islegal(dt_date_t* date)
{
        long val;

        val = dt_date_month(date);
        if (val < 0 || val > 12) {
            return(FALSE);
        }
        val = dt_date_mday(date);
        if (val < 0 || val > 31) {
            return(FALSE);
        }
        val = dt_date_hour(date);
        if (val < 0 || val >= 24) {
            return(FALSE);
        }
        val = dt_date_min(date);
        if (val < 0 || val >= 60) {
            return(FALSE);
        }
        val = dt_date_sec(date);
        if (val < 0 || val >= 60) {
            return(FALSE);
        }
        val = dt_date_fraction(date);
        if (val < 0) {
            return(FALSE);
        }
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_islegalyearmonthday
 * 
 * Checks if year, month and day fields of dt_date_t are 0,0,0.
 * 
 * Parameters : 
 * 
 *      date - 
 *              
 *              
 * Return value : TRUE if date is not 0000-00-00
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_islegalyearmonthday(dt_date_t* date)
{
        if (dt_date_year(date) == 0 && dt_date_month(date) == 0 && dt_date_mday(date) == 0) {
                return(FALSE);
        }
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_init
 * 
 * 
 * 
 * Parameters :          - none
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dt_date_t*  dt_date_init(void)
{
        dt_date_t* date = SSMEM_NEW(dt_date_t);
        memset(date, '\0', DT_DATE_DATASIZE);
        ss_dassert(dt_date_islegal(date));
        return(date);
}

/*##**********************************************************************\
 * 
 *              dt_date_done
 * 
 * 
 * 
 * Parameters : 
 * 
 *      date - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dt_date_done(dt_date_t* date)
{
        SsMemFree(date);
}

/*##**********************************************************************\
 * 
 *              dt_date_setdata
 * 
 * Puts data to date object
 * 
 * Parameters : 
 * 
 *      date - out, use
 *              pointer to date object
 *              
 *      year - in
 *              
 *              
 *      mon - in
 *              
 *              
 *      mday - in
 *              
 *              
 *      hour - in
 *              
 *              
 *      minute - in
 *              
 *              
 *      sec - in
 *              
 *              
 *      fraction - in
 *              # of 10^-9 secs ie. nanoseconds
 *              
 * Return value :
 *      TRUE when legal date value or
 *      FALSE otherwise.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_date_setdata(
        dt_date_t* date,
        int year,
        int mon,
        int mday,
        int hour,
        int minute,
        int sec,
        long fraction)
{
        if (!date_islegalymd(year, mon, mday) ||
            !date_islegalhmsf(hour, minute, sec, (ulong)fraction)) {
            return(FALSE);
        }
        year += DATE_YEARDRIFT;
        DATE_SETDATA(date, year, mon, mday, hour, minute, sec, (ulong)fraction);
        ss_dassert(dt_date_islegal(date));
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_setasciiz_ext
 * 
 * 
 * 
 * Parameters : 
 * 
 *      dest_date - 
 *              
 *              
 *      format - 
 *              
 *              
 *      ext_date - 
 *              
 *      p_datesqltype - out
 *          pointer to variable which tells whether the value
 *          was a DATE, TIME or TIMESTAMP literal
 *      
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_date_setasciiz_ext(
        dt_date_t* dest_date,
        char* format,
        char* ext_date,
        dt_datesqltype_t* p_datesqltype)
{
        bool is_time = FALSE;
        bool is_date = FALSE;
        int year = 0;
        int mon = 0;
        int mday = 0;
        int hour = 0;
        int minute  = 0;
        int sec  = 0;
        long fract  = 0;
        bool  b;
        char* ptr;

        if (format != NULL && format != dt_date_format) {
            /* Use user given format.
             */
            bool isfmt[FMT_LAST];
            long number;
            char* tmpptr;

            memset(isfmt, '\0', sizeof(isfmt));

            ptr = ext_date;

            while (*ptr != '\0') {
                int i;
                bool match;

                if (*ptr == *format) {
                    ptr++;
                    format++;
                } else if (ss_isspace(*ptr)) {
                    ptr++;
                } else {
                    i = date_findformat(format);
                    match = (i != -1);
                    if (match) {
                        char* bufend;
                        char buf[50];

                        format += strlen(fmt[i].fmt_str);
                        if (isfmt[fmt[i].fmt_type]) {
                            /* Format already found once. */
                            return(FALSE);
                        }
                        isfmt[fmt[i].fmt_type] = TRUE;
                        switch (fmt[i].fmt_type) {
                            case FMT_YEAR:
                                /* Number format year type.
                                 */
                                b = SsStrScanLong(ptr, &number, &tmpptr);
                                if (*tmpptr == *format) {
                                    ptr = tmpptr;
                                    /* This is delimited format (like
                                     * YYYY-MM-DD). In non-delimited format
                                     * (like YYYYMMDD) the number of digits
                                     * in the year must be exact.
                                     */
                                    if (!b ||
                                        number < DATE_YEARMIN || number > DATE_YEARMAX)
                                    {
                                        return (FALSE);
                                    }
                                    is_date = TRUE;
                                    break;
                                }
                                /* FALLTHROUGH */
                            case FMT_YEAR_1900:
                            case FMT_MON:
                            case FMT_MDAY:
                                /* Number format date types.
                                 */
                                strncpy(buf, ptr, strlen(fmt[i].fmt_str));
                                buf[strlen(fmt[i].fmt_str)] = '\0';
                                if (strlen(buf) != strlen(fmt[i].fmt_str)) {
                                    /* Data and format lengths do not match. */
                                    return(FALSE);
                                }
                                ptr += strlen(buf);
                                b = SsStrScanLong(buf, &number, &bufend);
                                if (!b || *bufend != '\0') {
                                    return (FALSE);
                                }
                                is_date = TRUE;
                                break;
                            case FMT_HOUR:
                            case FMT_MIN:
                            case FMT_SEC:
                            case FMT_FRACT:
                                /* Number format time types.
                                 */
                                if (*ptr != '\0') {
                                    strncpy(buf, ptr, strlen(fmt[i].fmt_str));
                                    buf[strlen(fmt[i].fmt_str)] = '\0';
                                    if (strlen(buf) != strlen(fmt[i].fmt_str)) {
                                        /* Data and format lengths do not match. */
                                        return(FALSE);
                                    }
                                    ptr += strlen(buf);
                                    if (fmt[i].fmt_type == FMT_FRACT) {
                                        b = SsStrScanLong(buf, &fract, &bufend);
                                        fract *= FRACT_COEFF(fmt[i].fmt_len);
                                    } else {
                                        b = SsStrScanLong(buf, &number, &bufend);
                                    }
                                    if (!b || *bufend != '\0') {
                                        return (FALSE);
                                    }
                                    is_time = TRUE;
                                } else {
                                    number = 0L;
                                }
                                break;
                            default:
                                /* Other format types.
                                */
                                ss_error;
                                break;
                        }
                        switch (fmt[i].fmt_type) {
                            case FMT_YEAR:
                                year = (int)number;
                                break;
                            case FMT_YEAR_1900:
                                year = (int)(1900 + number);
                                break;
                            case FMT_MON:
                                mon = (int)number;
                                break;
                            case FMT_MDAY:
                                mday = (int)number;
                                break;
                            case FMT_HOUR:
                                hour = (int)number;
                                break;
                            case FMT_MIN:
                                minute = (int)number;
                                break;
                            case FMT_SEC:
                                sec = (int)number;
                                break;
                            case FMT_FRACT:
                                break;
                            default:
                                ss_error;
                                break;
                        }
                    } else {
                        /* No match found. */
                        return(FALSE);
                    }
                }
            }
#if 0 /* Use defaults values for rest of fields */
            if (*format != '\0') {
                return(FALSE);
            }
#endif

        } else {

            /* Use default format.
             */
            is_date = TRUE;
            b = SsStrScanInt(ext_date, &year, &ptr);
            if (!b) {
                return (FALSE);
            }
            if (*ptr != DT_DATE_DELIMITER) {
                if (*ptr == DT_TIME_DELIMITER) {
                    is_date = FALSE;
                    hour = year;
                    year = 0;
                } else {
                    return(FALSE);
                }
            }
            if (is_date) {
                b = SsStrScanInt(ptr+1, &mon, &ptr);
                if (!b || *ptr != DT_DATE_DELIMITER) {
                    return(FALSE);
                }
                b = SsStrScanInt(ptr+1, &mday, &ptr);
                if (!b) {
                    return(FALSE);
                }
                while (*ptr == DT_DATETIME_DELIMITER) {
                    ptr++;
                }
            }
            if (*ptr != '\0') { /* There is also time */

                if (is_date) {
                    b = SsStrScanInt(ptr, &hour, &ptr);
                    if (!b || *ptr != DT_TIME_DELIMITER) {
                        return(FALSE);
                    }
                }
                b = SsStrScanInt(ptr+1, &minute, &ptr);
                if (!b || *ptr != DT_TIME_DELIMITER) {
                    return(FALSE);
                }
                b = SsStrScanInt(ptr+1, &sec, &ptr);
                if (!b) {
                    return(FALSE);
                }
                is_time = TRUE;
            }
            if (*ptr == DT_FRACT_DELIMITER && is_time) {
                char* start = ptr + 1;
                b = SsStrScanLong(start, &fract, &ptr);
                if (!b) {
                    /* just '.' with no trailing digits: it is legal */
                    fract = 0L;
                } else {
                    ss_dassert(ptr > start);
                    if (ptr - start > FRACT_MAXDIGITS) {
                        char* s = SsMemStrdup(start);
                        char* p;
                        s[FRACT_MAXDIGITS] = '\0';
                        b = SsStrScanLong(s, &fract, &p);
                        SsMemFree(s);
                        if (!b) {
                            return (FALSE);
                        }
                        fract *= FRACT_COEFF(FRACT_MAXDIGITS);
                    } else {
                        fract *= FRACT_COEFF(ptr - start);
                    }
                }
            }
        }
        if (is_date) {
            if (!date_islegalymd(year, mon, mday)) {
                return (FALSE);
            }
        } else {
            if (!is_time) {
                /* We must have either legal date or time or both! */
                return (FALSE);
            }
        }
        if (is_time) {
            if (!date_islegalhmsf(hour, minute, sec, fract)) {
                return(FALSE);
            }
        }
        year += DATE_YEARDRIFT;
        DATE_SETDATA(dest_date, year, mon, mday, hour, minute, sec, fract);
        if (p_datesqltype != NULL) {
            if (is_time) {
                if (is_date) {
                    *p_datesqltype = DT_DATE_SQLTIMESTAMP;
                } else {
                    *p_datesqltype = DT_DATE_SQLTIME;
                }
            } else {
                ss_dassert(is_date);
                *p_datesqltype = DT_DATE_SQLDATE;
            }
        }
        ss_dassert(dt_date_islegal(dest_date));
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_setasciiz
 * 
 * 
 * 
 * Parameters : 
 * 
 *      dest_date - 
 *              
 *              
 *      format - 
 *              
 *              
 *      ext_date - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_date_setasciiz(dt_date_t* dest_date, char* format, char* ext_date)
{
        bool succp;

        succp = dt_date_setasciiz_ext(
                    dest_date,
                    format,
                    ext_date,
                    NULL);
        return (succp);
}

/*##**********************************************************************\
 * 
 *              dt_date_setva
 * 
 * 
 * 
 * Parameters : 
 * 
 *      dest_date - 
 *              
 *              
 *      va - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_date_setva(dt_date_t* dest_date, va_t* va)
{
        char*       data;
        va_index_t  len;
        data = va_getdata(va, &len);
        ss_dassert(len == DT_DATE_DATASIZE);
        memcpy(dest_date, data, DT_DATE_DATASIZE);
        ss_dassert(dt_date_islegal(dest_date));
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_settimet
 * 
 * 
 * 
 * Parameters : 
 * 
 *      dest_date - 
 *              
 *              
 *      timet - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_date_settimet(dt_date_t* dest_date, SsTimeT timet)
{
        int year;
        SsTmT gtm;
        
        if (timet == 0L) {
            memset(dest_date, 0, DT_DATE_DATASIZE);
            return (FALSE);
        }
        SsLocaltime(&gtm, timet);
        if (!date_islegalymd(1900+gtm.tm_year, gtm.tm_mon + 1, gtm.tm_mday) ||
            !date_islegalhmsf(gtm.tm_hour, gtm.tm_min, gtm.tm_sec, 0UL)) {
            return(FALSE);
        }
        year = 1900+gtm.tm_year + DATE_YEARDRIFT;
        DATE_SETDATA(
            dest_date,
            year,
            gtm.tm_mon + 1,
            gtm.tm_mday,
            gtm.tm_hour,
            gtm.tm_min,
            gtm.tm_sec,
            0UL
        );
        ss_dassert(dt_date_islegal(dest_date));
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_setnow
 * 
 * Sets current timestamp to dt_date_t object
 * 
 * Parameters : 
 * 
 *  precision - in
 *      defines number of significant digits in fraction of second
 *
 *      d - out
 *              pointer to destination object
 *              
 * Return value :
 *      TRUE when successful
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_setnow(long precision, dt_date_t* d)
{
        bool succp;
        SsTimeT fraction;
        SsTimeT seconds;
        
        fraction = SsTimeMsS(&seconds);
        succp = dt_date_settimet(d, seconds);
        if(precision > 0) {
            /*precision is controlled in aval_timfun_now and aval_timfun_curtime functions*/
            fraction = (SsTimeT)(SsPow(10, precision) * fraction / 1000);  /*rounding to desired precision*/
            fraction = (SsTimeT)(1000000000 / SsPow(10, precision) * fraction); /*convert to nano seconds*/
            DATE_SETFRACTION(d, fraction);
        }

        return (succp);
}

/*##**********************************************************************\
 * 
 *              dt_date_setcurdate
 * 
 * Sets current date to dt_date_t object
 * 
 * Parameters : 
 * 
 *      d - out
 *              pointer to destination object
 *              
 * Return value : 
 *      TRUE when successful
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_setcurdate(dt_date_t* d)
{
        bool succp;

        succp = dt_date_setnow(0, d);
        if (succp) {
            succp = dt_date_truncatetodate(d);
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *              dt_date_setcurtime
 * 
 * Sets current time to dt_date_t object
 * 
 * Parameters : 
 * 
 *      d - out
 *              pointer to destination object
 *              
 * Return value : 
 *      TRUE when successful
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_setcurtime(long precision, dt_date_t* d)
{
        bool succp;

        succp = dt_date_setnow(precision, d);
        if (succp) {
            succp = dt_date_truncatetotime(d);
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *              dt_date_istime
 * 
 * Checks whether a date value is Time only (no date part)
 * 
 * Parameters : 
 * 
 *      d - in, use
 *              pointer to date variable
 *              
 * Return value :
 *      TRUE if time
 *      FALSE if date or timestamp
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_istime(dt_date_t* d)
{
        return (dt_date_year(d) == 0
            &&  dt_date_month(d) == 0
            &&  dt_date_mday(d) == 0);
}

/*##**********************************************************************\
 * 
 *              dt_date_padtimewithcurdate
 * 
 * Pads a time value with current date
 * 
 * Parameters : 
 * 
 *      d - in out, use
 *              time value
 *              
 * Return value :
 *      TRUE when successful, FALSE when the d was not a time
 *      only (no date part) value.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_padtimewithcurdate(dt_date_t* d)
{
        bool succp;
        SsTimeT t;
        dt_date_t tmpdate;
        int year;
        int month;
        int mday;
        int hour;
        int minute;
        int sec;
        ulong fraction;

        if (!dt_date_istime(d)) {
            return (FALSE);
        }
        t = SsTime(NULL);
        succp = dt_date_settimet(&tmpdate, t);
        ss_dassert(succp);
        year = dt_date_year(&tmpdate);
        month = dt_date_month(&tmpdate);
        mday = dt_date_mday(&tmpdate);
        hour = dt_date_hour(d);
        minute = dt_date_min(d);
        sec = dt_date_sec(d);
        fraction = dt_date_fraction(d);
        succp = dt_date_setdata(
                    d, year, month, mday, hour, minute, sec, fraction);
        return (succp);
}

/*##**********************************************************************\
 * 
 *              dt_date_truncatetodate
 * 
 * Truncates a timestamp value to date value
 * (i.e. sets time portion to 00:00:00.000000000)
 * 
 * Parameters : 
 * 
 *      d - in out, use
 *              pointer to timestamp value
 *              
 * Return value : 
 *      TRUE
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_truncatetodate(dt_date_t* d)
{
        int year;
        int month;
        int mday;

        year = dt_date_year(d);
        month = dt_date_month(d);
        mday = dt_date_mday(d);

        year += DATE_YEARDRIFT;
        DATE_SETDATA(d, year, month, mday, 0, 0, 0, 0L);
        ss_dassert(dt_date_islegal(d));
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_truncatetotime
 * 
 * Truncates a timestamp value to time value
 * (i.e. sets date portion to 0000-00-00
 * 
 * Parameters : 
 * 
 *      d - in out, use
 *              pointer to timestamp value
 *              
 * Return value :
 *      TRUE
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_truncatetotime(dt_date_t* d)
{
        int hour;
        int minute;
        int sec;
        ulong fraction;

        hour = dt_date_hour(d);
        minute = dt_date_min(d);
        sec = dt_date_sec(d);
        fraction = dt_date_fraction(d);

        DATE_SETDATA(d, DATE_YEARDRIFT, 0, 0, hour, minute, sec, fraction);
        ss_dassert(dt_date_islegal(d));
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_datetoasciiz
 * 
 * 
 * 
 * Parameters : 
 * 
 *      src_date - 
 *              
 *              
 *      format - 
 *              
 *              
 *      datestr - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_date_datetoasciiz(dt_date_t* src_date, char* format, char* datestr)
{
        ss_dprintf_1(("dt_date_datetoasciiz:format=%s\n", format != NULL ? format : "NULL"));
        ss_dassert(dt_date_islegal(src_date));

        if (format == NULL || format == dt_date_format) {
            if (dt_date_istime(src_date)) {
                SsSprintf(
                    datestr,
                    "%02d%c%02d%c%02d",
                    dt_date_hour(src_date),
                    DT_TIME_DELIMITER,
                    dt_date_min(src_date),
                    DT_TIME_DELIMITER,
                    dt_date_sec(src_date)
                );
            } else {
                SsSprintf(
                    datestr,
                    "%04d%c%02d%c%02d%c%02d%c%02d%c%02d",
                    dt_date_year(src_date),
                    DT_DATE_DELIMITER,
                    dt_date_month(src_date),
                    DT_DATE_DELIMITER,
                    dt_date_mday(src_date),
                    DT_DATETIME_DELIMITER,
                    dt_date_hour(src_date),
                    DT_TIME_DELIMITER,
                    dt_date_min(src_date),
                    DT_TIME_DELIMITER,
                    dt_date_sec(src_date)
                );
            }
        } else {
            while (*format) {
                int i;
                i = date_findformat(format);
                if (i != -1) {
                    /* Found a format.
                     */
                    long value = 0;
                    switch (fmt[i].fmt_type) {
                        case FMT_YEAR:
                            value = dt_date_year(src_date);
                            break;
                        case FMT_YEAR_1900:
                            value = dt_date_year(src_date) - 1900;
                            ss_dassert(fmt[i].fmt_len == 2);
                            value = value % 100;
                            break;
                        case FMT_MON:
                            value = dt_date_month(src_date);
                            break;
                        case FMT_MDAY:
                            value = dt_date_mday(src_date);
                            break;
                        case FMT_HOUR:
                            value = dt_date_hour(src_date);
                            break;
                        case FMT_MIN:
                            value = dt_date_min(src_date);
                            break;
                        case FMT_SEC:
                            value = dt_date_sec(src_date);
                            break;
                        case FMT_FRACT:
                            value = dt_date_fraction(src_date);
                            value /= FRACT_COEFF(fmt[i].fmt_len);
                            break;
                        default:
                            ss_error;
                            break;
                    }
                    switch (fmt[i].fmt_len) {
                        case 1:
                            SsSprintf(datestr, "%ld", value);
                            break;
                        case 2:
                            SsSprintf(datestr, "%02ld", value);
                            break;
                        case 3:
                            SsSprintf(datestr, "%03ld", value);
                            break;
                        case 4:
                            SsSprintf(datestr, "%04ld", value);
                            break;
                        case 5:
                            SsSprintf(datestr, "%05ld", value);
                            break;
                        case 6:
                            SsSprintf(datestr, "%06ld", value);
                            break;
                        case 7:
                            SsSprintf(datestr, "%07ld", value);
                            break;
                        case 8:
                            SsSprintf(datestr, "%08ld", value);
                            break;
                        case 9:
                            SsSprintf(datestr, "%09ld", value);
                            break;
                        default:
                            ss_error;
                    }
                    format += fmt[i].fmt_len;
                    datestr += strlen(datestr);
                } else {
                    *datestr++ = *format++;
                }
            }
            *datestr = '\0';
        }

        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_datetoasciiz_sql
 * 
 * Copies the date/time/timestamp value to asciiz buffer in SQL format
 * 
 * Parameters : 
 * 
 *      src_date - in, use
 *              pointer to source date variable
 *              
 *      datesqltype - in
 *          tells whether the given variable is a DATE / TIME / TIMESTAMP
 *              
 *      datestr - out
 *              buffer for string
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_datetoasciiz_sql(
        dt_date_t* src_date,
        dt_datesqltype_t datesqltype,
        char* datestr)
{
        int year = 0, month = 0, mday = 0;
        int hour = 0, minute = 0, sec = 0;
        ulong fraction = 0;

        ss_dprintf_1(("dt_date_datetoasciiz_sql:datesqltype=%d\n", datesqltype));
        ss_dassert(dt_date_islegal(src_date));

        if (datesqltype != DT_DATE_SQLTIME) {
            year = dt_date_year(src_date);
            month = dt_date_month(src_date);
            mday = dt_date_mday(src_date);
        }
        if (datesqltype != DT_DATE_SQLDATE) {
            hour = dt_date_hour(src_date);
            minute = dt_date_min(src_date);
            sec = dt_date_sec(src_date);
            fraction = dt_date_fraction(src_date);
        }

        if (datesqltype == DT_DATE_SQLTYPE_UNKNOWN) {
            if (year == 0 && month == 0 && mday == 0) {
                datesqltype = DT_DATE_SQLTIME;
            } else {
                datesqltype = DT_DATE_SQLTIMESTAMP;
            }
        }
        switch (datesqltype) {
            case DT_DATE_SQLDATE:
                SsSprintf(
                    datestr,
                    "%04d%c%02d%c%02d",
                    year,
                    DT_DATE_DELIMITER,
                    month,
                    DT_DATE_DELIMITER,
                    mday);
                break;
            case DT_DATE_SQLTIME:
                if (fraction != 0L) {
                    SsSprintf(
                        datestr,
                        "%02d%c%02d%c%02d%c%09lu",
                        hour,
                        DT_TIME_DELIMITER,
                        minute,
                        DT_TIME_DELIMITER,
                        sec,
                        DT_FRACT_DELIMITER,
                        fraction);

                } else {
                    SsSprintf(
                        datestr,
                        "%02d%c%02d%c%02d",
                        hour,
                        DT_TIME_DELIMITER,
                        minute,
                        DT_TIME_DELIMITER,
                        sec);
                }
                break;
            case DT_DATE_SQLTIMESTAMP:
                if (fraction != 0L) {
                    SsSprintf(
                        datestr,
                        "%04d%c%02d%c%02d%c%02d%c%02d%c%02d%c%09lu",
                        year,
                        DT_DATE_DELIMITER,
                        month,
                        DT_DATE_DELIMITER,
                        mday,
                        DT_DATETIME_DELIMITER,
                        hour,
                        DT_TIME_DELIMITER,
                        minute,
                        DT_TIME_DELIMITER,
                        sec,
                        DT_FRACT_DELIMITER,
                        fraction);
                } else {
                    SsSprintf(
                        datestr,
                        "%04d%c%02d%c%02d%c%02d%c%02d%c%02d",
                        year,
                        DT_DATE_DELIMITER,
                        month,
                        DT_DATE_DELIMITER,
                        mday,
                        DT_DATETIME_DELIMITER,
                        hour,
                        DT_TIME_DELIMITER,
                        minute,
                        DT_TIME_DELIMITER,
                        sec);
                }
                break;
            default:
                return (FALSE);
        }
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_datetova
 * 
 * 
 * 
 * Parameters : 
 * 
 *      src_date - 
 *              
 *              
 *      va - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_date_datetova(dt_date_t* src_date, va_t* va)
{
        if (!dt_date_islegal(src_date)) {
            ss_derror;
            return(FALSE);
        }
        va_setdata(va, src_date, DT_DATE_DATASIZE);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              dt_date_datetotimet
 * 
 * Converts a date value into a time_t illegal values are asserted
 * unless the value is binary 0, which converts to (SsTimeT)0
 * 
 * Parameters : 
 * 
 *      src_date - in, use
 *              source date
 *              
 *      p_time - out, use
 *              pointer to SsTimeT variable
 *              
 * Return value : TRUE
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_date_datetotimet(dt_date_t* src_date, SsTimeT* p_time)
{
        SsTmT gtm;
        SsTimeT t;
        int i;

        ss_dassert(src_date != NULL);
        ss_dassert(p_time != NULL);
        ss_dassert(dt_date_islegal(src_date));

        for (i = 0; i < DT_DATE_DATASIZE; i++) {
            if (src_date->date_data[i] != '\0') {
                break;
            }
        }
        if (i >= DT_DATE_DATASIZE) {
            ss_dassert(i == DT_DATE_DATASIZE);
            *p_time = (SsTimeT)0L;
            return (TRUE);
        }
        gtm.tm_sec = dt_date_sec(src_date);
        gtm.tm_min = dt_date_min(src_date);
        gtm.tm_hour = dt_date_hour(src_date);
        gtm.tm_mday = dt_date_mday(src_date);
        gtm.tm_mon = dt_date_month(src_date) - 1;
        gtm.tm_year = dt_date_year(src_date) - 1900;
        gtm.tm_isdst = 0;
        ss_dassert((uint)gtm.tm_sec < (uint)60);
        ss_dassert((uint)gtm.tm_min < (uint)60);
        ss_dassert((uint)gtm.tm_hour < (uint)24);
        ss_dassert((uint)gtm.tm_mday < (uint)32);
        ss_dassert((uint)gtm.tm_mon < (uint)12);
        ss_dassert(gtm.tm_year >= 0);

        t = SsMktime(&gtm);
        *p_time = t;
        return (TRUE);
}


/*##**********************************************************************\
 * 
 *              dt_date_cmp
 * 
 * Compares two date objects.
 * 
 * Parameters : 
 * 
 *      date1 - in, use
 *          Date 1      
 *              
 *      date2 - in, use
 *          Date 2      
 *              
 * Return value : 
 * 
 *      <0, if date1 < date2
 *      =0, if date1 = date2
 *      >0, if date1 > date2
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
int dt_date_cmp(dt_date_t* date1, dt_date_t* date2)
{
        ss_dassert(dt_date_islegal(date1));
        ss_dassert(dt_date_islegal(date2));
        
        return(SsMemcmp(date1, date2, DT_DATE_DATASIZE));
}


int  dt_date_year(dt_date_t* date)
{
        int year;

        year = (int)(
            (SS_RETYPE(uchar *, date)[DATE_YEAROFFSET] << CHAR_BIT) |
             SS_RETYPE(uchar *, date)[DATE_YEAROFFSET + 1]);
        year -= DATE_YEARDRIFT;
        return(year);
}

int  dt_date_month(dt_date_t* date)
{
        return(SS_RETYPE(char *, date)[DATE_MONTHOFFSET]);
}

int  dt_date_mday(dt_date_t* date)
{
        return(SS_RETYPE(char *, date)[DATE_MDAYOFFSET]);
}

int  dt_date_hour(dt_date_t* date)
{
        return(SS_RETYPE(char *, date)[DATE_HOUROFFSET]);
}

int  dt_date_min(dt_date_t* date)
{
        return(SS_RETYPE(char *, date)[DATE_MINOFFSET]);
}

int  dt_date_sec(dt_date_t* date)
{
        return(SS_RETYPE(char *, date)[DATE_SECOFFSET]);
}

ulong dt_date_fraction(dt_date_t* date)
{
        ulong fract;
        uint i;
        
        for (fract = 0UL, i = DATE_FRACTOFFSET; i < DATE_FRACTOFFSET + 4; i++) {
            fract <<= CHAR_BIT;
            fract |= SS_RETYPE(uchar *, date)[i];
        }
        ss_dassert(fract <= 999999999UL);
        return(fract);
}


/*#***********************************************************************\
 * 
 *              date_islegalymd
 * 
 * Checks that the year, month and day given form a legal date.
 * 
 * Parameters : 
 * 
 *      year - in, use
 *          Year        
 *              
 *      month - in, use
 *              Month
 *              
 *      day - in, use
 *              Day of the month
 *              
 * Return value : 
 * 
 *      TRUE, if date is legal
 *      FALSE, if not
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool date_islegalymd(
        int year,
        int month,
        int day
) {
        return(
            (year == -DATE_YEARDRIFT && month == 0 && day == 0) ||
            (year == 0 && month == 0 && day == 0) ||
#if defined (SS_MYSQL) || defined (SS_MYSQL_AC)
            (year == 0) ||
#endif
            (year != 0 && (year < 0 ? (-year) : year) <= DATE_YEARMAX &&
             (unsigned)(month - 1) < (unsigned)12 &&
             ((unsigned)(day - 1) < (unsigned)SsTimeDaysInMonth((uint)year, month)
               || (year < 0 && (unsigned)(day - 1) < (unsigned)SsTimeDaysInMonth(4U, month))))
            /* I don't know about leap-years of B.C. years !!!!!! */
        );
}

/*#***********************************************************************\
 * 
 *              date_islegalhmsf
 * 
 * Checks that the hour, minute, sec and fraction given form a legal time.
 * 
 * Parameters : 
 * 
 *      hour - in, use
 *          Hours       
 *              
 *      minute - in, use
 *              Minutes
 *              
 *      sec - in, use
 *              Seconds
 *
 *      fract - in, use
 *          Fractions of a second (.FFFFFFFFF)
 *
 * Return value : 
 * 
 *      TRUE, if time is legal
 *      FALSE, if not
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool date_islegalhmsf(
        int hour,
        int minute,
        int sec,
        ulong fract
) {
        return(
            (unsigned)hour <= (unsigned)23 &&
            (unsigned)minute  <= (unsigned)59 &&
            (unsigned)sec  <= (unsigned)59 &&
            fract <= (ulong)999999999L
        );
}


static int date_weekdayno(dt_date_weekday_t wd)
{
        static const int iso_wdnums[7] = { 7, 1, 2, 3, 4, 5, 6 };
        int wd_no;

        wd_no = iso_wdnums[wd];
        /*
        if (USA || UK || Ireland) {
            wd_no = (int)wd + 1;
        }
        */
        return (wd_no);
}

static dt_date_weekday_t date_weekday(int year, int month, int mday)
{
        long wday;
        int nleaps;
        long nyears;
#define known_year 0x10000L

        /* 65536-01-01 is Wednesday  */
        wday = (long)((int)DT_DATE_WD_WED - (int)DT_DATE_WD_SUN);

        /* get the weekday of year-01-01 */
        nyears = known_year - year;
        nleaps = ((known_year - 1) / 4)
            - ((known_year - 1) / 100)
            + ((known_year - 1) / 400);
        nleaps -= ((year - 1) / 4)
            - ((year - 1) / 100)
            + ((year - 1) / 400);
        wday -= nyears + nleaps;
        wday += 20000L * 7L;
        ss_dassert(wday > 0);

        /* add days passed in this year */
        wday += SsTimeDayOfYear((uint)year, month, mday) - 1;
        wday %= 7;
        return ((dt_date_weekday_t)(wday + (int)DT_DATE_WD_SUN));
}

static int date_week(int year, int month, int mday)
{
        dt_date_weekday_t wd_jan_1;
        dt_date_weekday_t wd_dec_31;
        int startprevyear;
        int difference;
        int week;
        int wd_no;

        startprevyear = 0;
        wd_jan_1 = date_weekday(year, 1, 1);
        wd_no = date_weekdayno(wd_jan_1);
        startprevyear = (wd_no >= 2 && wd_no <= 4);
        difference = -((8 - wd_no) % 7);
        difference += SsTimeDayOfYear((uint)year, month, mday) - 1;
        if (difference < 0) {
            /* week started prev year */
            week = date_week(year - 1, 12, 31);
        } else {
            week = difference / 7 + 1 + startprevyear;
        }
        if (month == 12 && week > 52) {
            wd_dec_31 = date_weekday(year, 12, 31);
            wd_no = date_weekdayno(wd_dec_31);
            if (wd_no <= 3) {
                /* week is first week of next year */
                week = 1;
            }
        }
        return (week);
}

dt_date_weekday_t dt_date_weekday(dt_date_t* d)
{
        int year;
        int month;
        int mday;
        dt_date_weekday_t wd;

        year = dt_date_year(d);
        month = dt_date_month(d);
        mday = dt_date_mday(d);
        wd = date_weekday(year, month, mday);
        return (wd);
}

int dt_date_week(dt_date_t* d)
{
        int year;
        int month;
        int mday;
        int week;

        year = dt_date_year(d);
        month = dt_date_month(d);
        mday = dt_date_mday(d);
        week = date_week(year, month, mday);
        return (week);
}

int dt_date_dayofyear(dt_date_t* d)
{

        int year;
        int month;
        int mday;
        int dayofyear;

        year = dt_date_year(d);
        month = dt_date_month(d);
        mday = dt_date_mday(d);
        dayofyear = SsTimeDayOfYear((int)year, month, mday);
        return (dayofyear);
}

bool dt_date_tsadd(
        dt_date_tsiunit_t unit,
        long val,
        dt_date_t* src_d,
        dt_date_t* dest_d)
{
        long year;
        int month;
        int mday;
        int hour;
        int minute;
        int sec;
        ulong fraction;
        int leap;
        int mday_decr;
        int ndaysinmonth;
        bool succp;
        long lend = 0L;
        int tmp;

        ss_dassert(dt_date_islegal(src_d));

        memcpy(dest_d, src_d, DT_DATE_DATASIZE);
        if (dt_date_istime(dest_d)) {
            dt_date_padtimewithcurdate(dest_d);
        }
        year  = dt_date_year(dest_d);
        month = dt_date_month(dest_d);
        mday  = dt_date_mday(dest_d);
        hour  = dt_date_hour(dest_d);
        minute   = dt_date_min(dest_d);
        sec   = dt_date_sec(dest_d);
        fraction = dt_date_fraction(dest_d);

        switch (unit) {
            case DT_DATE_IU_FRAC_SECOND:
                if (val < 0L) {
                    val += (long)fraction;
                    while (val < 0) {
                       lend--;
                       val += 1000000000L;
                    }
                } else {
                    val += (long)fraction;
                }
                fraction = val % 1000000000L;
                val /= 1000000000L;
                val += lend;
                if (val == 0L) {
                    break;
                }
                /* FALLTHROUGH */
            case DT_DATE_IU_SECOND:
                if (val < 0L) {
                    tmp = (int)(-val % 60);
                    if (sec - tmp < 0) {
                        lend = -1L;
                        sec += 60;
                    } else {
                        lend = 0L;
                    }
                    sec -= tmp;
                } else {
                    val += sec;
                    sec = val % 60;
                }
                val = val / 60 + lend;
                if (val == 0L) {
                    break;
                }
                /* FALLTHROUGH */
            case DT_DATE_IU_MINUTE:
                if (val < 0L) {
                    tmp = (int)(-val % 60);
                    if (minute - tmp < 0) {
                        lend = -1L;
                        minute += 60;
                    } else {
                        lend = 0L;
                    }
                    minute -= tmp;
                } else {
                    val += minute;
                    minute = val % 60;
                }
                val = val / 60 + lend;
                if (val == 0L) {
                    break;
                }
                /* FALLTHROUGH */
            case DT_DATE_IU_HOUR:
                if (val < 0L) {
                    tmp = (int)(-val % 24);
                    if (hour - tmp < 0) {
                        lend = -1L;
                        hour += 24;
                    } else {
                        lend = 0L;
                    }
                    hour -= tmp;
                } else {
                    val += hour;
                    hour = val % 24;
                }
                val = val / 24 + lend;
                if (val == 0L) {
                    break;
                }
                /* FALLTHROUGH */
            case DT_DATE_IU_DAY:
day_case:;
                if (val < 0L) {
                    val = -val;
                    for (;;) {
                        if (month > 2
                        &&  SsTimeIsLeapYear((uint)year))
                        {
                            leap = 1;
                        } else if (month <= 2
                        &&  SsTimeIsLeapYear((uint)(year-1))) {
                            leap = 1;
                        } else {
                            leap = 0;
                        }
                        if (month == 2 && mday == 29) {
                            mday_decr = -1;
                        } else {
                            mday_decr = 0;
                        }
                        if (val >= 365 + leap) {
                            year--;
                            if (year <= 0) {
                                return (FALSE);
                            }
                            val -= 365 + leap;
                            mday += mday_decr;
                        } else {
                            break;
                        }
                    }
                    for (;;) {
                        int month1;
                        int year1;

                        if (month == 1) {
                            month1 = 12;
                            year1 = year - 1;
                        } else {
                            month1 = month - 1;
                            year1 = year;
                        }
                        ndaysinmonth = SsTimeDaysInMonth((uint)year1, month1);
                        if (mday > ndaysinmonth) {
                            mday_decr = ndaysinmonth - mday;
                        } else {
                            mday_decr = 0;
                        }
                        if (ndaysinmonth <= val + mday_decr) {
                            val = val - ndaysinmonth + mday_decr;
                            mday += mday_decr;
                            month = month1;
                            year = year1;
                            if (year <= 0) {
                                return (FALSE);
                            }
                            ss_dassert(mday > 0);
                            ss_dassert(mday <= SsTimeDaysInMonth((uint)year, month));
                        } else if (mday <= val) {
                            month = month1;
                            year = year1;
                            mday = ndaysinmonth - (val - mday);
                            ss_dassert(mday > 0);
                            ss_dassert(mday <= SsTimeDaysInMonth((uint)year, month));
                            break;
                        } else {
                            mday -= val;
                            ss_dassert(mday > 0);
                            ss_dassert(mday <= SsTimeDaysInMonth((uint)year, month));
                            break;
                        }
                        
                    }
                } else {
                    for (;;) {
                        if (month <= 2 && SsTimeIsLeapYear((uint)year)) {
                            leap = 1;
                        } else if (month > 2 && SsTimeIsLeapYear((uint)(year + 1))) {
                            leap = 1;
                        } else {
                            leap = 0;
                        }
                        if (month == 2 && mday == 29) {
                            leap = 0;
                            mday_decr = -1;
                        } else {
                            mday_decr = 0;
                        }
                        if (val >= 365 + leap) {
                            mday += mday_decr;
                            year++;
                            val -= 365 + leap;
                        } else {
                            break;
                        }
                    }
                    if (year > DATE_YEARMAX) {
                        return (FALSE);
                    }
                    val += mday;
                    for (;;) {
                        ndaysinmonth = SsTimeDaysInMonth((uint)year, month);
                        if (ndaysinmonth < val) {
                            val -= ndaysinmonth;
                            month++;
                            if (month > 12) {
                                month = 1;
                                year++;
                            }
                        } else {
                            mday = val;
                            break;
                        }
                    }
                    if (year > DATE_YEARMAX) {
                        return (FALSE);
                    }
                }
                break;
            case DT_DATE_IU_MONTH:
month_case:;
                if (val < 0L) {
                    val = -val;
                    if ((val % 12) >= month) {
                        year--;
                        month += 12;
                    }
                    month -= val % 12;
                    ss_dassert(1 <= month && month <= 12);
                    val /= 12;
                    val = -val;
                } else {
                    val += month - 1;
                    month = val % 12 + 1;
                    val /= 12;
                }
                /* FALLTHROUGH */
            case DT_DATE_IU_YEAR:
                year += val;
                if (year > DATE_YEARMAX || year <= 0) {
                    return (FALSE);
                }
                ndaysinmonth = SsTimeDaysInMonth((uint)year, month);
                if (mday > ndaysinmonth) {
                    mday = ndaysinmonth;
                }
                break;
            case DT_DATE_IU_WEEK:
                unit = DT_DATE_IU_DAY;
                val *= 7;
                goto day_case;
            case DT_DATE_IU_QUARTER:
                unit = DT_DATE_IU_MONTH;
                val *= 3;
                goto month_case;
            default:
                ss_derror;
                return (FALSE);
        }
        succp = dt_date_setdata(
                    dest_d,
                    (int)year,
                    month,
                    mday,
                    hour,
                    minute,
                    sec,
                    fraction);
        return (succp);
}

static bool date_substract_greater1st(
        dt_date_t* date1,
        dt_date_t* date2,
        ss_int8_t* p_diff)
{
        int year1, year2, month1, month2, mday1, mday2;
        int hour1, hour2, min1, min2, sec1, sec2;
        long frac1, frac2;
        int yday1;
        int yday2;
        long date_diff_in_days;
        int hour_diff;
        int min_diff;
        int sec_diff;
        long frac_diff;
        long year1_in_days;
        long year2_in_days;
        bool succp = TRUE;

        ss_debug(int comp = dt_date_cmp(date1, date2);)

        ss_rc_dassert(comp > 0, comp);

        year1 = dt_date_year(date1);
        month1 = dt_date_month(date1);
        mday1 = dt_date_mday(date1);
        hour1 = dt_date_hour(date1);
        min1 = dt_date_min(date1);
        sec1 = dt_date_sec(date1);
        frac1 = dt_date_fraction(date1);
        yday1 = SsTimeDayOfYear(year1, month1, mday1);
        year1_in_days = 365L * year1 + SsTimeLeapsBeforeThis(year1);
        
        year2 = dt_date_year(date2);
        month2 = dt_date_month(date2);
        mday2 = dt_date_mday(date2);
        hour2 = dt_date_hour(date2);
        min2 = dt_date_min(date2);
        sec2 = dt_date_sec(date2);
        frac2 = dt_date_fraction(date2);
        yday2 = SsTimeDayOfYear(year2, month2, mday2);
        year2_in_days = 365L * year2 + SsTimeLeapsBeforeThis(year2);
            
        if (year1 < 0 || year2 < 0) {
            return (FALSE);
        }
        date_diff_in_days = 0;
        hour_diff = 0;
        min_diff = 0;
        sec_diff = 0;
        frac_diff = 0;

        frac_diff = frac1 - frac2;
        if (frac_diff < 0) {
            frac_diff += 1000000000L;
            sec2 += 1;
        }
        sec_diff = sec1 - sec2;
        if (sec_diff < 0) {
            sec_diff += 60;
            min2 += 1;
        }
        min_diff = min1 - min2;
        if (min_diff < 0) {
            min_diff += 60;
            hour2 += 1;
        }
        hour_diff = hour1 - hour2;
        if (hour_diff < 0) {
            hour_diff += 24;
            yday2 += 1;
        }
        date_diff_in_days = year1_in_days + yday1 - year2_in_days - yday2;

        ss_dassert(date_diff_in_days >= 0);
        SsInt8SetInt4(p_diff, date_diff_in_days);
        succp = SsInt8MultiplyByInt2(p_diff, *p_diff, 24) && succp;
        ss_dassert(hour_diff >= 0);
        SsInt8AddUint4(p_diff, *p_diff, hour_diff);
        succp = SsInt8MultiplyByInt2(p_diff, *p_diff, 60) && succp;
        ss_dassert(min_diff >= 0);
        SsInt8AddUint4(p_diff, *p_diff, min_diff);
        succp = SsInt8MultiplyByInt2(p_diff, *p_diff, 60) && succp;
        ss_dassert(sec_diff >= 0);
        SsInt8AddUint4(p_diff, *p_diff, sec_diff);
        {
            ss_int8_t mult;
            SsInt8SetInt4(&mult, 1000000000);
            succp = SsInt8MultiplyByInt8(p_diff, *p_diff, mult) && succp;
        }
        ss_dassert(frac_diff >= 0);
        SsInt8AddUint4(p_diff, *p_diff, frac_diff);
        return succp;
}

bool dt_date_substract(
        dt_date_t* date1,
        dt_date_t* date2,
        ss_int8_t* p_diff)
{
        bool succp;
        int comp;
        int negator;

        comp = dt_date_cmp(date1, date2);
        if (comp == 0) {
            SsInt8Set0(p_diff);
            return (TRUE);
        }
        if (comp < 0) {
            dt_date_t* tmp_date = date1;
            date1 = date2;
            date2 = tmp_date;
            negator = -1;
        } else {
            negator = 1;
        }
        succp = date_substract_greater1st(date1, date2, p_diff);
        if (succp) {
            if (negator == -1) {
                SsInt8Negate(p_diff, *p_diff);
            } else {
                ss_dassert(negator == 1);
            }
        }
        return (succp);
}

bool dt_date_tsdiff_old(
        dt_date_tsiunit_t unit,
        dt_date_t* d1,
        dt_date_t* d2,
        ss_int8_t* p_result)
{
        bool succp;
        long year1, year2;
        int  month1, month2;
        int  mday1, mday2;
        int  hour1, hour2;
        int  minute1, minute2;
        int  sec1, sec2;
        long fraction1, fraction2;
        int comp;
        int negator = 1;

        ss_dassert(dt_date_islegal(d1));
        ss_dassert(dt_date_islegal(d2));

        comp = dt_date_cmp(d1,d2);
        if (comp == 0) {
            SsInt8Set0(p_result);
            return (TRUE);
        }
        if (comp > 0) {
            dt_date_t* d_tmp;
            negator = -1;
            d_tmp = d1;
            d1 = d2;
            d2 = d_tmp;
        }
        switch (unit) {
            case DT_DATE_IU_FRAC_SECOND:
            case DT_DATE_IU_SECOND:
            case DT_DATE_IU_MINUTE:
            case DT_DATE_IU_HOUR:
            case DT_DATE_IU_DAY:
            {

                succp = date_substract_greater1st(d2, d1, p_result);
                if (!succp) {
                    return (FALSE);
                }
                if (unit == DT_DATE_IU_FRAC_SECOND) {
                    break;
                } else {
                    ss_int8_t div;
                    SsInt8SetInt4(&div, 1000000000);

                    switch (unit) {
                    case DT_DATE_IU_SECOND:
                        break;
                    case DT_DATE_IU_MINUTE:
                        succp = SsInt8MultiplyByInt2(&div, div, 60) && succp;
                        break;
                    case DT_DATE_IU_HOUR:
                        succp = SsInt8MultiplyByInt2(&div, div, 60 * 60) && succp;
                        break;
                    case DT_DATE_IU_DAY:
                        succp = SsInt8MultiplyByInt2(&div, div, 60 * 60) && succp;
                        succp = SsInt8MultiplyByInt2(&div, div, 24) && succp;
                        break;
                    default:
                        ss_error;
                    }
                    SsInt8DivideByInt8(p_result, *p_result, div);
                }
                break;
            }
            case DT_DATE_IU_MONTH:
            case DT_DATE_IU_YEAR:
            {
                ss_uint4_t diff;
                year1 = dt_date_year(d1);
                year2 = dt_date_year(d2);
                month1 = dt_date_month(d1);
                month2 = dt_date_month(d2);
                mday1 = dt_date_mday(d1);
                mday2 = dt_date_mday(d2);
                hour1 = dt_date_hour(d1);
                hour2 = dt_date_hour(d2);
                minute1 = dt_date_min(d1);
                minute2 = dt_date_min(d2);
                sec1 = dt_date_sec(d1);
                sec2 = dt_date_sec(d2);
                fraction1 = dt_date_fraction(d1);
                fraction2 = dt_date_fraction(d2);
                comp = 0;
                if (unit == DT_DATE_IU_YEAR) {
                    if (month1 < month2) {
                        comp = -1;
                    } else if (month1 > month2) {
                        comp = 1;
                    }
                }
                if (comp == 0) {
                    if (mday1 < mday2) {
                        comp = -1;
                    } else if (mday1 > mday2) {
                        comp = 1;
                    }
                }
                if (comp == 0) {
                    if (hour1 < hour2) {
                        comp = -1;
                    } else if (hour1 > hour2) {
                        comp = 1;
                    }
                }
                if (comp == 0) {
                    if (minute1 < minute2) {
                        comp = -1;
                    } else if (minute1 > minute2) {
                        comp = 1;
                    }
                }
                if (comp == 0) {
                    if (sec1 < sec2) {
                        comp = -1;
                    } else if (sec1 > sec2) {
                        comp = 1;
                    }
                }
                if (comp == 0) {
                    if (fraction1 < fraction2) {
                        comp = -1;
                    } else if (fraction1 > fraction2) {
                        comp = 1;
                    }
                }
                switch (unit) {
                    case DT_DATE_IU_MONTH:
                        diff = (year2 - year1) * 12;
                        diff += month2 - month1;
                        ss_dassert(diff >= 0);
                        if (comp < 0) {
                            diff++;
                        }
                        succp = TRUE;
                        break;
                    case DT_DATE_IU_YEAR:
                        diff = (year2 - year1);
                        ss_dassert(diff >= 0);
                        if (comp < 0) {
                            diff++;
                        }
                        succp = TRUE;
                        break;
                    default:
                        ss_derror;
                        return (FALSE);
                }
                SsInt8SetUint4(p_result, diff);
                break;
            }
            case DT_DATE_IU_WEEK:
            {
                ss_int8_t i7;
                SsInt8SetInt4(&i7, 7);
                unit = DT_DATE_IU_DAY;
                succp = dt_date_tsdiff_old(unit, d1, d2, p_result);
                SsInt8AddUint2(p_result, *p_result, 7 - 1);
                SsInt8DivideByInt8(p_result, *p_result, i7);
                break;
            }
            case DT_DATE_IU_QUARTER:
            {
                ss_int8_t i3;
                SsInt8SetInt4(&i3, 3);
                unit = DT_DATE_IU_MONTH;
                succp = dt_date_tsdiff_old(unit, d1, d2, p_result);
                SsInt8AddUint2(p_result, *p_result, 3 - 1);
                SsInt8DivideByInt8(p_result, *p_result, i3);
                break;
            }
            default:
                ss_derror;
                return (FALSE);
        }

        if (negator == -1) {
            SsInt8Negate(p_result, *p_result);
        } else {
            ss_dassert(negator == 1);
        }

        return (succp);
}

static bool date_isintimetrange(dt_date_t* date)
{
        static bool initialized = FALSE;
        static dt_date_t min_timet;
        static dt_date_t max_timet;
        
        if (!initialized) {
            bool succp;

            succp = dt_date_settimet_raw(&min_timet, 0L);
            ss_dassert(succp);
            succp = dt_date_settimet_raw(&max_timet, SS_INT4_MAX);
            ss_dassert(succp);
            initialized = TRUE;
        }
        if (dt_date_cmp(&min_timet, date) > 0) {
            return (FALSE);
        }
        if (dt_date_cmp(&max_timet, date) < 0) {
            return (FALSE);
        }
        return (TRUE);
}

bool dt_date_datetotimet_raw(dt_date_t* src_date, SsTimeT* p_time)
{
        SsSimpleTmT gtm;
        SsTimeT t;
        int i;

        ss_dassert(src_date != NULL);
        ss_dassert(p_time != NULL);
        ss_dassert(dt_date_islegal(src_date));

        for (i = 0; i < DT_DATE_DATASIZE; i++) {
            if (src_date->date_data[i] != '\0') {
                break;
            }
        }
        if (i >= DT_DATE_DATASIZE) {
            ss_dassert(i == DT_DATE_DATASIZE);
            *p_time = (SsTimeT)0L;
            return (TRUE);
        }
        if (!date_isintimetrange(src_date)) {
            return (FALSE);
        }
        gtm.tm_sec = dt_date_sec(src_date);
        gtm.tm_min = dt_date_min(src_date);
        gtm.tm_hour = dt_date_hour(src_date);
        gtm.tm_mday = dt_date_mday(src_date);
        gtm.tm_mon = dt_date_month(src_date) - 1;
        gtm.tm_year = dt_date_year(src_date) - 1900;
        ss_dassert((uint)gtm.tm_sec < (uint)60);
        ss_dassert((uint)gtm.tm_min < (uint)60);
        ss_dassert((uint)gtm.tm_hour < (uint)24);
        ss_dassert((uint)(gtm.tm_mday-1) < (uint)31);
        ss_dassert((uint)gtm.tm_mon < (uint)12);
        ss_dassert(gtm.tm_year >= 0);

        t = SsMktimeGMT(&gtm);
        *p_time = t;
        if (t == (SsTimeT)-1L) {
            return (FALSE);
        }
        return (TRUE);
}

bool dt_date_settimet_raw(dt_date_t* dest_date, SsTimeT timet)
{
        int year;
        SsSimpleTmT gtm;

        SsGmtime(&gtm, timet);
        if (!date_islegalymd(1900+gtm.tm_year, gtm.tm_mon + 1, gtm.tm_mday) ||
            !date_islegalhmsf(gtm.tm_hour, gtm.tm_min, gtm.tm_sec, 0UL)) {
            return(FALSE);
        }
        year = 1900+gtm.tm_year + DATE_YEARDRIFT;
        DATE_SETDATA(
            dest_date,
            year,
            gtm.tm_mon + 1,
            gtm.tm_mday,
            gtm.tm_hour,
            gtm.tm_min,
            gtm.tm_sec,
            0UL
        );
        ss_dassert(dt_date_islegal(dest_date));
        return(TRUE);
}



#if 1
bool dt_date_tsdiff_new(
        dt_date_tsiunit_t unit,
        dt_date_t* d1,
        dt_date_t* d2,
        ss_int8_t* p_result)
{
        bool succp;
        int comp;
        int negator = 1;

        ss_dassert(dt_date_islegal(d1));
        ss_dassert(dt_date_islegal(d2));

        comp = dt_date_cmp(d1,d2);
        if (comp == 0) {
            SsInt8Set0(p_result);
            return (TRUE);
        }
        if (comp > 0) {
            dt_date_t* d_tmp;
            negator = -1;
            d_tmp = d1;
            d1 = d2;
            d2 = d_tmp;
        }
        switch (unit) {
            case DT_DATE_IU_FRAC_SECOND:
            case DT_DATE_IU_SECOND:
            case DT_DATE_IU_MINUTE:
            case DT_DATE_IU_HOUR:
            case DT_DATE_IU_DAY:
            case DT_DATE_IU_WEEK:
            {
                succp = date_substract_greater1st(d2, d1, p_result);
                if (!succp) {
                    return (FALSE);
                }
                if (unit == DT_DATE_IU_FRAC_SECOND) {
                    break;
                } else {
                    ss_int8_t div;
                    SsInt8SetInt4(&div, 1000000000);

                    switch (unit) {
                    case DT_DATE_IU_SECOND:
                        break;
                    case DT_DATE_IU_MINUTE:
                        succp = SsInt8MultiplyByInt2(&div, div, 60) && succp;
                        break;
                    case DT_DATE_IU_HOUR:
                        succp = SsInt8MultiplyByInt2(&div, div, 60 * 60) && succp;
                        break;
                    case DT_DATE_IU_DAY:
                        succp = SsInt8MultiplyByInt2(&div, div, 60 * 60) && succp;
                        succp = SsInt8MultiplyByInt2(&div, div, 24) && succp;
                        break;
                    case DT_DATE_IU_WEEK:
                        succp = SsInt8MultiplyByInt2(&div, div, 60 * 60) && succp;
                        succp = SsInt8MultiplyByInt2(&div, div, 24 * 7) && succp;
                        break;
                    default:
                        ss_error;
                    }
                    SsInt8DivideByInt8(p_result, *p_result, div);
                }
                succp = TRUE;
                break;
            }
            case DT_DATE_IU_MONTH:
            case DT_DATE_IU_YEAR: {
                ss_uint4_t diff;
                int year1, year2, month1, month2, mday1, mday2;
                int hour1, hour2, min1, min2, sec1, sec2;
                long frac1, frac2;
                int daysinmonth2;

                year1 = dt_date_year(d1);
                month1 = dt_date_month(d1);
                mday1 = dt_date_mday(d1);
                hour1 = dt_date_hour(d1);
                min1 = dt_date_min(d1);
                sec1 = dt_date_sec(d1);
                frac1 = dt_date_fraction(d1);
        
                year2 = dt_date_year(d2);
                month2 = dt_date_month(d2);
                mday2 = dt_date_mday(d2);
                hour2 = dt_date_hour(d2);
                min2 = dt_date_min(d2);
                sec2 = dt_date_sec(d2);
                frac2 = dt_date_fraction(d2);

                daysinmonth2 = SsTimeDaysInMonth(year2, month2);
                
                diff = 0;
                if (month1 > month2) {
                    month2 += 12;
                    year2 -= 1;;
                }
                diff = month2 - month1;
                diff += (year2 - year1) * 12;
                if (daysinmonth2 < mday1) {
                    mday1 = daysinmonth2;
                }
                if (mday1 > mday2) {
                    goto dec_diff;
                }
                if (mday1 < mday2) {
                    goto diff_ok;
                }
                if (hour1 > hour2) {
                    goto dec_diff;
                }
                if (hour1 < hour2) {
                    goto diff_ok;
                }
                if (min1 > min2) {
                    goto dec_diff;
                }
                if (min1 < min2) {
                    goto diff_ok;
                }
                if (sec1 > sec2) {
                    goto dec_diff;
                }
                if (sec1 < sec2) {
                    goto diff_ok;
                }
                if (frac1 > frac2) {
                    goto dec_diff;
                }
        diff_ok:;
                if (unit == DT_DATE_IU_YEAR) {
                    diff /= 12;
                }
                SsInt8SetInt4(p_result, diff);
                succp = TRUE;
                break;
        dec_diff:;
                diff -= 1;
                goto diff_ok;
            }
            case DT_DATE_IU_QUARTER:
            {
                ss_int8_t i3;
                SsInt8SetInt4(&i3, 3);
                unit = DT_DATE_IU_MONTH;
                succp = dt_date_tsdiff_new(unit, d1, d2, p_result);
                SsInt8DivideByInt8(p_result, *p_result, i3);
                break;
            }
            default:
                ss_derror;
                return (FALSE);
        }

        if (negator == -1) {
            SsInt8Negate(p_result, *p_result);
        } else {
            ss_dassert(negator == 1);
        }
        return succp;
}
#endif /* 0 */
