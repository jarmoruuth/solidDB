/*************************************************************************\
**  source       * dt1dfl.c
**  directory    * dt
**  description  * arithmetic routines for decimal floating-point numbers:
**               * dfloat v-attributes.
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

#include <ssc.h>
#include <ssstddef.h>
#include <ssstring.h>
#include <ssstdio.h>
#include <sslimits.h>
#include <ssscan.h>
#include <ssdebug.h>

#include "dt0dfloa.h"
#include "dt1dfl.h"

#define DFLOATNST_LEN DFL_VA_MAXLEN

int u_isdecimalpoint(char c);

#define	MAXLEN		    (DT_DFL_DATASIZE - 1)
#define OVERFLOW_STRING "###"


/* Define 16 bit signed and unsigned integer type for internal calculations.
   Short is ok for most 16 bit and 32 bit intel 80x86 processors compilers.
   It is assertted that these types are two bytes, but if the byte ordering
   changes, some changes in the code are necessary.
*/

typedef ss_int2_t       df_int16;
typedef ss_uint2_t      df_uint16;

#define int16_msb(w)    ((unsigned char)(((w) >> 8) & 0xFF))
#define int16_lsb(w)    ((unsigned char)((w) & 0xFF))

#define byte(pdf, n)	(*((unsigned char *)pdf + n))
#define neg(pdf)	    (byte(pdf, 2) & (unsigned char)'\200')
#define posit(pdf)	    (!(byte(pdf, 2) & (unsigned char)'\200'))

#define	dfn_len(p_df)	(df_int16)byte(p_df, 0)
#define	dfn_neg(p_df)	(byte(p_df, MAXLEN + 3 - byte(p_df, 0)) & '\200')

/* DFLOATNST_LEN used used for space allocation (in uti.h) and
   DFNST_LEN is used for internal calculations
*/

#define DFNST_LEN       (DFLOATNST_LEN-1)

struct dfloatnstruct {
       char dnst[MAXLEN + 2];
};
typedef  struct dfloatnstruct  dflnst_t;

static df_int16 df_mdiv10(dt_dfl_t *);
static void     df_mmul10(dt_dfl_t *);
static void     df_mdiv10andround(dt_dfl_t *);
static void     df_incsize(dt_dfl_t *);
static void     df_normalize(dt_dfl_t *);
static void     df_compres(dt_dfl_t *);

static void     dnst_copylong(dt_dfl_t *, dflnst_t *);
static void     dfn_chsign(dflnst_t *);
static df_int16 dfn_mdiv10(dflnst_t *);
static void     dfn_mdiv10andround(dflnst_t *);

static void     df_maddint(dt_dfl_t *, df_int16);
static void     df_mulint(dt_dfl_t *, df_int16);
static void     df_mulint2(dt_dfl_t *, df_int16, dt_dfl_t *);
static df_int16 dfn_mmul10n(dflnst_t *,df_int16, df_int16);
static void     dfn_maddint(dflnst_t *, df_int16);

static const char *decimalpoints = ".,";  /* delimiters for decimals */
                                  /* the first one is used when outputting */
static dt_dfl_t df_zero = { '\2', '\0', '\0'};
dt_dfl_t dfl_nan = { '\0' };

/* the initial bytes for dfn_zero, dfn_overflow, dfn_minusone
 * depends for MAXLEN + 2 */
static dflnst_t dfn_zero   = { '\2', '\0', '\0', '\0', '\0', '\0',
			    '\0', '\0' ,'\0', '\0'  };
static dflnst_t dfn_minusone   = { '\2', '\0' , '\377', '\377', '\377',
				'\377', '\377', '\377' ,'\377' ,'\377' };


/******************  EXTERNAL INTERFACE ************************/


/*##**********************************************************************\
 * 
 *		dfl_sum
 * 
 */
dt_dfl_t dfl_sum(df1, df2)
        dt_dfl_t df1;
        dt_dfl_t df2;
{
    dflnst_t *desadr1, *desadr2;
    dflnst_t df11, df22;
#ifndef SS_MT
    static
#endif
    dt_dfl_t df3;
    df_int16 i, addres, carry;
    df_int16 dif1, dif, sign1, sign2, ressign;
    df_int16 msbyte;

    ss_purify(memset(&df3, '\0', sizeof(df3)));

	/* Check if operands are overflowed numbers */
	if ((dfl_overflow(df1)) || (dfl_overflow(df2))) {
	    /* return overflow */
	    byte(&df3,0) = '\0';
	    return(df3);
	}

	/* Check if one of the operands is zero - this avoids
	   a nasty round-off in the situation 0 + 0.xxxxxxxxxxx
	   (the last one has digits to full precision)

	   SV 20.9.1987 */

	if (df_eqzero(&df1)) {
        return(df2);
    }
	if (df_eqzero(&df2)) {
        return(df1);
    }

	df_normalize(&df1);
	df_normalize(&df2);
	if (neg(&df1)) {
	    df11 = dfn_minusone;
	    sign1 = 1;
	} else {
	    df11 = dfn_zero;
	    sign1 = 0;
	}

	if (neg(&df2)) {
	    df22 = dfn_minusone;
	    sign2 = 1;
	} else {
	    df22 = dfn_zero;
	    sign2 = 0;
	}

	if (sign1 == sign2) {
	    ressign = sign1;
	} else {
	    /* -1 means 'not yet known' */
	    ressign = -1;
	}

	dnst_copylong(&df1, &df11);
	dnst_copylong(&df2, &df22);

	/* calculate the difference between the exponents */
	dif1 = (df_int16)byte(&df11, 1) - (df_int16)byte(&df22, 1);

	if (dif1) {
        ss_dprintf_2(("dfl_sum:exponents are not equal, amke them the same\n"));
	    dif = dif1 / 2;
	    if (dif1 > 0) {
	        /* If exponent of df11 > exp.of df22 then decrement df22's
	           exponent (exponent negative => increment is the actual
	           operation) and multiply the mantissa corresponding
	           number of times by 10. */

		    if (dif > 0) {
		        dfn_mmul10n(&df22, dif, (df_int16)100);
		    }
		    /* If exponent difference is >= 2 then multiply df22 by 100
		       dif times and decrement the exponent dif*2 times. */

		    dif = (df_int16)byte(&df11, 1) - (df_int16)byte(&df22, 1);
		    if (dif > 0) {
		        dfn_mmul10n(&df22, dif, (df_int16)10);
		    }
		    /* if the difference between exponents was odd, then do the
		       last one. */

		    while(byte(&df11, 1) > byte(&df22, 1)) {
		        dfn_mdiv10andround(&df11);
		    }
		    /* if df22 is a very small number then the mantissa overflows
		       when trying to compensate for exponent, this corrects it.
		    */

        } else {	       /* as above, now df11>=df22 */
            if (dif < 0) {
                dfn_mmul10n(&df11, -dif, (df_int16)100);
            }
            dif = (df_int16)byte(&df22, 1) - (df_int16)byte(&df11, 1);
	        if (dif > 0) {
	            dfn_mmul10n(&df11, dif, (df_int16)10);
	        }

	        while(byte(&df11, 1) < byte(&df22, 1)) {
	            dfn_mdiv10andround(&df22);
	        }
	    }
	}

	/* Now the exponents are equal for both numbers. The sign can now
	   be found by examining the mantissas. */

	if (byte(&df11, 0) >= byte(&df22, 0)) {
	    if (ressign == -1 && byte(&df11, 0) > byte(&df22, 0)) {
		    ressign = sign1;
	    }
	    msbyte =  (df_int16)MAXLEN + (df_int16)3 - (df_int16)byte(&df11, 0);
	    desadr1 = &df11;
	    desadr2 = &df22;
	} else {
	    ressign = sign2;

	    msbyte =  (df_int16)MAXLEN + (df_int16)3 - (df_int16)byte(&df22, 0);
	    desadr1 = &df22;
	    desadr2 = &df11;
	}

	/* Add the mantissas of df11 and df2 */
	carry = 0;

	for (i = MAXLEN + 1 ; i >= msbyte; i--) {
	    addres = (df_int16)byte(desadr1, i) + (df_int16)byte(desadr2, i) + carry;

	    byte(desadr1, i) = int16_lsb(addres);
	    carry = (df_int16)int16_msb(addres);
	}

	/* It seems that it is OK to forget the last carry value (?)  -- SV

	*/

	/* If the mantissas were of equal length (result's sign is still
	   unresolved) then examine the most significant bit of the sum
	   which is equal to sign bit.
           If the results sign is known then check for overflow of mantissa
	   as well and if neccessary, add one byte to mantissa length.
	 */
	if (ressign == -1) {
	    ressign = (df_int16)(byte(desadr1, i + 1) & '\200');
	} else if (ressign == !(byte(desadr1, i + 1) & '\200')) {
	    byte(desadr1, i) = (ressign ? (unsigned char)'\377' : '\0');
	    byte(desadr1, 0)++;
	    msbyte--;
	}

	if ((df_int16)byte(desadr1, 0) >= MAXLEN) {

	    /* check for overflow */
	    if ((byte(desadr1, 1) == 0) && (byte(desadr1, 0) > MAXLEN)) {
	       /* return overflow */
	       byte(&df3, 0) = '\0';
	       return(df3);
	    }
	    /* divide and round the mantissa until it fits to
	       the normal size */
	    if (ressign) {
		    while (byte(desadr1, msbyte) != (unsigned char)'\377') {
		        dfn_mdiv10andround(desadr1);
		    }

		    while(!(byte(desadr1, msbyte + 1) & '\200')) {
		        dfn_mdiv10andround(desadr1);
		    }
	    } else {
		    while (byte(desadr1, msbyte)) {
		        dfn_mdiv10andround(desadr1);
		    }

		    while (byte(desadr1, msbyte + 1) & '\200') {
		        dfn_mdiv10andround(desadr1);
		    }
	    }
	}

	for (i = MAXLEN + 3 - byte(desadr1, 0); i <= MAXLEN; i++) {
	    if (ressign) {
		    if ((byte(desadr1, i) == (unsigned char)'\377' && byte(desadr1, i + 1) & '\200')) {
		        byte(desadr1, 0)--;
		    } else {
		        break;
		    }
	    } else {
		    if ((!byte(desadr1, i) && !(byte(desadr1, i + 1) & '\200'))) {
		        byte(desadr1, 0)--;
		    } else {
		        break;
		    }
	    }
	}

	dif1 = MAXLEN + 3 - byte(desadr1, 0);
	for (i = dif1; i <= MAXLEN + 1; i++) {
	    byte(&df3, 2 + i - dif1) = byte(desadr1, i);
	}

	byte(&df3, 0) = byte(desadr1, 0);
	byte(&df3, 1) = byte(desadr1, 1);

	return(df3);
}

/*##**********************************************************************\
 * 
 *		dfl_diff
 * 
 */
dt_dfl_t dfl_diff(df1, df2)
        dt_dfl_t df1;
        dt_dfl_t df2;
{
	dfl_change_sign(&df2);
	return(dfl_sum(df1, df2));
}


/*##**********************************************************************\
 * 
 *		dfl_prod
 * 
 */
dt_dfl_t dfl_prod(df1, df2)
        dt_dfl_t df1;
        dt_dfl_t df2;
{
	dt_dfl_t result;
#ifndef SS_MT
	static
#endif
	    dt_dfl_t res;
	df_int16 positive, i, i1, i2, exponent;
	unsigned char resultarea[2 * MAXLEN - 1];
	df_uint16 mulres;
	df_int16 addres, carry, maxarea, ind;

        ss_purify(memset(&res, '\0', sizeof(res)));
        ss_purify(memset(&result, '\0', sizeof(res)));

	/* Check if operands are overflowed numbers */
	if ((dfl_overflow(df1)) || (dfl_overflow(df2))) {
	    /* return overflow */
	    byte(&res,0) = '\0';
	    return(res);
	}
	positive = (neg(&df1) == neg(&df2));
	if (neg(&df1)) dfl_change_sign(&df1);
	if (neg(&df2)) dfl_change_sign(&df2);

	exponent = (df_int16)byte(&df1, 1) + (df_int16)byte(&df2, 1);
	maxarea  = (df_int16)byte(&df1, 0) + (df_int16)byte(&df2, 0) - 2;

	for (i = 0; i <= maxarea; i++) {
            ss_assert(i >= 0 && i < 2 * MAXLEN - 1);
	    resultarea[i] = '\0';
	}

	for (i1 = (df_int16)byte(&df1, 0); i1 >= 2; i1--) {
	    if (byte(&df1, i1)) {
		    for (i2 = (df_int16)byte(&df2, 0); i2 >= 2; i2--) {
		        i = i1 + i2 - 2;
		        mulres = (df_uint16)byte(&df1, i1) *
			        (df_uint16)byte(&df2, i2);
                    ss_assert(i >= 0 && i < 2 * MAXLEN - 1);
		        addres = (df_int16)resultarea[i] + mulres;

                    ss_assert(i >= 0 && i < 2 * MAXLEN - 1);
		        resultarea[i] = (unsigned char)int16_lsb(addres);
		        while (int16_msb(addres)) {
			        i--;
			        addres = (df_int16)resultarea[i] +
                                (df_int16)int16_msb(addres);
                        ss_assert(i >= 0 && i < 2 * MAXLEN - 1);
			        resultarea[i] = (unsigned char)int16_lsb(addres);
		        }
		    }
	    }
	}

	ind = maxarea - MAXLEN + 1;
	if (ind >= 0) {
	    for (;;) {
		/* the result is small enough if the "ind"  highest */
		/* bytes and the msb of the "ind+1" byte are zeroes */
		for (i = 1; i <= ind; i++) {
                ss_assert(i >= 0 && i < 2 * MAXLEN - 1);
		    if (resultarea[i]) break;
		}

            ss_assert(ind + 1 >= 0 && ind + 1 < 2 * MAXLEN - 1);
		if (!(resultarea[ind + 1] & '\200')) {
		    if (i == ind + 1) break;
		}

		/* the result must be truncated because it is
		   too long */

		/* decrease the exponent */
		if (--exponent < 0) {
		    byte(&result, 0) = '\0'; /* overflow */
		    res = result;
		    return(res);
		}

		/* the multiplication result is divided by 10 */
		carry = 0;
		while (i <= maxarea) {
                ss_assert(i >= 0 && i < 2 * MAXLEN - 1);
		    addres = (df_int16)resultarea[i] + (df_int16)256 * carry;
		    carry = addres % 10;
                ss_assert(i >= 0 && i < 2 * MAXLEN - 1);
		    resultarea[i] = (unsigned char)(addres / 10);
		    i++;
		}
	    }

	    for (i = 2; i <= MAXLEN; i++) {
                ss_assert(i + ind - 1 >= 0 && i + ind - 1 < 2 * MAXLEN - 1);
        		byte(&result, i) = resultarea[i + ind - 1];
	    }
	    byte(&result, 0) = MAXLEN;

	} else {
	    byte(&result, 0) = '\1';
	    for (i1 = 1; i1 <= maxarea; i1++) {
                ss_assert(i1 >= 0 && i1 < 2 * MAXLEN - 1);
        		if (resultarea[i1]) break;
	    }

	    if (i1 > maxarea) {
                i1 = maxarea;
            }
            ss_assert(i1 >= 0 && i1 < 2 * MAXLEN - 1);
	    if (resultarea[i1] & '\200') {
                i1--;
            }
	    i2 = i1 - 2;
	    for (i = i1; i <= maxarea; i++) {
                ss_assert(i >= 0 && i < 2 * MAXLEN - 1);
        		byte(&result, i - i2) = resultarea[i];
	    	byte(&result, 0)++;
	    }
	}

	while (exponent > 255) {
	    byte(&result, 1) = '\1';    /* dummy */
	    df_mdiv10(&result);
	    exponent--;
	}

	byte(&result, 1) = (unsigned char)exponent;

	if (!positive) dfl_change_sign(&result);
	df_compres(&result);
	res = result;
	return(res);
}

/*##**********************************************************************\
 * 
 *		dfl_quot
 * 
 */
dt_dfl_t dfl_quot(df1, df2)
        dt_dfl_t df1;
        dt_dfl_t df2;
{
	dt_dfl_t tmp, tmpres;
#ifndef SS_MT
	static
#endif
            dt_dfl_t result;
	df_uint16 dividend, divisor;        /* temp. dividend & divisor */
	df_int16 res, exponent = 0;		       /* temp. result & exponent */
	df_int16 positive, ind, case1, flag = 0;    /* flags */
	df_int16 i, carry, addres;		       /* work integers */

        ss_purify(memset(&result, '\0', sizeof(result)));
        ss_purify(memset(&tmpres, '\0', sizeof(result)));

	/* Check if operands are overflowed numbers */
	if ((dfl_overflow(df1)) || (dfl_overflow(df2))) {
	    /* return overflow */
	    byte(&result,0) = '\0';
	    return(result);
	}

	result = df_zero;
	if (df_eqzero(&df2)) {
	    byte(&result, 0) = '\0'; /* overflow */
	    return(result);
	}
	if (df_eqzero(&df1)) {
            return(result);
        }
	positive = (neg(&df1) == neg(&df2));
	if (neg(&df1)) dfl_change_sign(&df1);
	if (neg(&df2)) dfl_change_sign(&df2);
	if (byte(&df2, 0) == '\2' && byte(&df2, 2) == '\1') {
	    exponent = (df_int16)byte(&df1, 1) - (df_int16)byte(&df2, 1);
	    result = df1;
	    byte(&result, 1) = '\0';
	    goto cont;
	}

	/* now we prepare the dividend & divisor for work */
	/* because of overflow, if they are too long	  */
	if (byte(&df1, 0) == MAXLEN && (byte(&df1, 2) > '\13')) {
	    if (!byte(&df1, 1)) {
		    exponent--;
		    byte(&df1, 1)++;
	    }
	    df_mdiv10andround(&df1);
	}
	if (byte(&df2, 0) == MAXLEN && (byte(&df2, 2) > '\13')) {
	    if (!byte(&df2, 1)) {
		    exponent++;
		    byte(&df2, 1)++;
	    }
	    df_mdiv10andround(&df2);
	}
	while (byte(&df1, 0) > byte(&df2, 0)) {
	    df_mulint(&df2, (df_int16)10);
	    byte(&df2, 1)++;
	}

	/* make the divisor */
	if (byte(&df2, 0) == '\2' || byte(&df2, 2) > '\13') {
	    divisor = (df_uint16)byte(&df2, 2); /* divisor is between */
	    flag = 1;				   /* 12(or 1) and 127	 */
	} else {
            divisor = (((df_uint16)byte(&df2, 3)) & 0xFF) +
                      ((((df_uint16)byte(&df2, 2)) & 0xFF) << 8);
	}

	exponent += (df_int16)byte(&df1, 1) - (df_int16)byte(&df2, 1);
	byte(&df1, 1) = byte(&df2, 1) = '\0';

	for (;;) {
	    /* make the dividend */
	    while (byte(&df2, 0) > byte(&df1, 0)) {
		df_mulint(&df1, (df_int16)10);
		byte(&df1, 1)++;
		df_mulint(&result, (df_int16)10);
		if (byte(&result, 0) == MAXLEN && byte(&result, 2) > '\13') {
		    exponent = exponent + (df_int16)byte(&df1, 1);
		    goto cont;
		}
	    }
	    while (flag &&
		   byte(&df2, 0) == byte(&df1, 0) &&
		   byte(&df2, 2) > byte(&df1, 2)) {

		df_mulint(&df1, (df_int16)10);
		byte(&df1, 1)++;
		df_mulint(&result, (df_int16)10);
		if (byte(&result, 0) == MAXLEN && byte(&result, 2) > '\13') {
		    exponent = exponent + (df_int16)byte(&df1, 1);
		    goto cont;
		}
	    }
	    case1 = 1;
	    if (flag) {
		if (byte(&df1, 0) == byte(&df2, 0)) {
		    dividend = (df_uint16)byte(&df1, 2);
		} else {
                dividend = (((df_uint16)byte(&df1,3)) & 0xFF) +
                           ((((df_uint16)byte(&df1,2)) & 0xFF) << 8);
		    case1 = 0;
		}
	    } else {
            dividend = (((df_uint16)byte(&df1,3)) & 0xFF) +
                       ((((df_uint16)byte(&df1,2)) & 0xFF) << 8);
		while (dividend < divisor) {
		    df_mulint(&df1, (df_int16)10);
		    byte(&df1, 1)++;
		    df_mulint(&result, (df_int16)10);
                dividend &= 0xFF00;
                dividend += (((df_uint16)byte(&df1,3)) & 0xFF);
		    if (byte(&result, 0) == MAXLEN && byte(&result, 2) > '\13') {
        			exponent = exponent + (df_int16)byte(&df1, 1);
	    		goto cont;
		    }
                dividend &= 0x00FF;
                dividend += (((df_uint16)byte(&df1,2)) & 0xFF) << 8;
		}
	    }
	    exponent = exponent + (df_int16)byte(&df1, 1);
	    byte(&df1, 1) = '\0';
	    res = (df_int16)(dividend / divisor);

	    do {
		    ind = 0;
		    df_mulint2(&df2, res, &tmp);
		    if (byte(&tmp, 0) == '\0') break;
		    if (case1) {
		        while (byte(&tmp, 0) != byte(&df2, 0)) {
			    if(!(--res)) goto next;
			    df_mulint2(&df2, res, &tmp);
		        }
		    } else if (byte(&tmp, 0) == byte(&df2, 0)) {
		        df_incsize(&tmp);
		    }
		        for (i = 2; i <= (df_int16)byte(&tmp, 0); i++) {
		            if (byte(&df1, i) > byte(&tmp, i)) break;
		            if (byte(&df1, i) < byte(&tmp, i)) {
			        if (!(--res)) goto next;
			        ind = 1;
			        break;
		        }
		    }
	    } while (ind);

next:
	    if (!res) {
		    df_mulint(&df1, (df_int16)10);
		    byte(&df1, 1)++;
		    df_mulint(&result, (df_int16)10);
		    if (byte(&result, 0) == MAXLEN && byte(&result, 2) > '\13') {
		        exponent = exponent + (df_int16)byte(&df1, 1);
		        break;
		    }
		    continue;
	    }

	    if (byte(&tmp, 0) == '\0') break;
	    df_maddint(&result, res);
	    if (byte(&result, 0) == MAXLEN && byte(&result, 2) > '\13') break;
	    if (neg(&result)) df_incsize(&result);

	    for (i = (df_int16)byte(&tmp, 0) + 1; i <= (df_int16)byte(&df1, 0); i++) {
		byte(&tmp, i) = '\0';
	    }
	    byte(&tmp, 0) = byte(&df1, 0);

	    /* subtraction */
	    dfl_change_sign(&tmp);
	    carry = 0;
	    for (i = (df_int16)byte(&df1, 0); i >= 2; i--) {
		addres = (df_int16)byte(&df1, i) + (df_int16)byte(&tmp, i) + carry;
		byte(&tmpres, i) = int16_lsb(addres);
		carry = (df_int16)int16_msb(addres);
	    }

	    byte(&tmpres, 0) = byte(&df1, 0);
	    byte(&tmpres, 1) = '\0';
	    df_compres(&tmpres);
	    if (df_eqzero(&tmpres)) {
                break;
            }
	    df1 = tmpres;
	}

cont:
	if (exponent > 255) {
	    result = df_zero;
	    /* Underflow is not used, instead a zero is returned */
	    return(result);
	}

	while (exponent < 0) {
	    df_mulint(&result, (df_int16)10);
	    if (dfl_overflow(result)) return(result);
	    exponent++;
	}

        byte(&result, 1) = int16_lsb(exponent);

	if (!positive) dfl_change_sign(&result);
	return(result);
}


/*##**********************************************************************\
 * 
 *		dfl_overflow
 * 
 */
int dfl_overflow(df)
        dt_dfl_t df;
{

    /* this function is called from most of the other functions, so
        we assert df_int16 size here */
    ss_assert(sizeof(df_int16) == 2);
    ss_assert(sizeof(df_uint16) == 2);
    ss_dprintf_1(("dfl_overflow:byte(&df, 0)=%d\n", byte(&df, 0)));

    /* check for oversize dfloats */
    if (byte(&df, 0) > MAXLEN) {
        ss_dprintf_2(("dfl_overflow:is overflow\n"));
        return(1);
    }

    if (byte(&df, 0) == '\0' || byte(&df, 0) == '\1')  {
        ss_dprintf_2(("dfl_overflow:is overflow\n"));
        return(1);
    }

	/* for some odd reason a mantissa like 0x8000.. was
	   treated as overflow. Not any more. SV 2.9.1987.
	   Previous code:

	    if (byte(&df, 2) != (unsigned char)'\200') return(0);

	    for (i = 3; i <= (df_int16)byte(&df, 0); i++) {
	        if (byte(&df, i)) return(0);
	    }

	    return(1);
	 */

	 return(0);
}

/*##**********************************************************************\
 * 
 *		dfl_compare
 * 
 */
int dfl_compare(df1, df2)
        dt_dfl_t df1;
        dt_dfl_t df2;
{
	dt_dfl_t result;
	df_int16 i;

	/* Check for overflowed numbers */
	if (dfl_overflow(df1)) {
	    /* if both numbers are overflows, return equal
	       else overflow is larger */
	    if (dfl_overflow(df2)) return(0);
	    return(1);
	}
	if (dfl_overflow(df2)) return(-1);

	result = dfl_diff(df1, df2);
	if (neg(&result)) return(-1);
	for (i = 2; i <= (df_int16)byte(&result, 0); i++) {
	    if (byte(&result, i)) return(1);
	}

	return(0);
}

/*##**********************************************************************\
 * 
 *		dfl_inttodfl
 * 
 */
dt_dfl_t dfl_inttodfl(m)
        int m;
{
	return(dfl_longetodfl((long)m, 0));
}

/*##**********************************************************************\
 * 
 *		dfl_intetodfl
 * 
 * 
 */
dt_dfl_t dfl_intetodfl(m, e)
        int m;
        int e;
{
	return(dfl_longetodfl((long)m, e));
}

/*##**********************************************************************\
 * 
 *		dfl_longtodfl
 * 
 */
dt_dfl_t dfl_longtodfl(m)
        long m;
{
	return(dfl_longetodfl(m, 0));

}

/*##**********************************************************************\
 * 
 *		dfl_longetodfl
 * 
 */
dt_dfl_t dfl_longetodfl(m, exp)
        long m;
        int exp;
{
#ifndef SS_MT
	static
#endif
        dt_dfl_t result;
        ss_int4_t m4 = (ss_int4_t)m;
        df_int16 e = (df_int16)exp;
	df_int16 long4_len = sizeof(m4);
	df_int16 pos;

        ss_purify(memset(&result, '\0', sizeof(result)));

	byte(&result, 0) = '\1' + (char)sizeof(m4);
	byte(&result, 1) = '\0';
	pos = long4_len + 1;
	while (long4_len > 0) {
	    byte(&result, pos) = (unsigned char)(m4 & 0xFF);
	    pos--;
	    long4_len--;
	    m4 >>= 8;
	}

	/* Multiply result by 10 until e=0 */
	while (e > 0) {
	    e--;
	    df_mmul10(&result);
	    byte(&result, 1) = '\0';
	}

	e = -e;
	byte(&result, 1) = (unsigned char) e;
	df_normalize(&result);
	return(result);
}

/*##**********************************************************************\
 * 
 *		dfl_asciiztodfl
 * 
 */
int dfl_asciiztodfl(s, pdf)
        char* s;
        dt_dfl_t* pdf;
{

	df_int16  sign, found, pointfound, digit, toomanydigits;
	dt_dfl_t df1, df2, orgdf;

    orgdf = *pdf;
	df1 = df_zero;
	*pdf = df_zero;

	while (*s == ' ' || *s == '\t' || *s == '\n') s++;

	sign = 1;

	if (*s == '+') {
	    s++;
	} else if (*s == '-') {
	    sign = -1;
	    s++;
	}

	found = 0;
	pointfound = 0;
	toomanydigits = 0;

	while ((*s >= '0' && *s <= '9') || u_isdecimalpoint(*s)) {

	    if (u_isdecimalpoint(*s)) {
		    if (pointfound) {
                *pdf = orgdf;
                return(0);
            }
		    pointfound = 1;
	    } else {
		    found = 1;

		    digit = (df_int16)*s - (df_int16)'0';

		    if (pointfound) {
		        byte(&df1, 1)++;
		    }

		    df2 = df1;
		    df_mmul10(&df2);

		    if (!dfl_overflow(df2)) {
		        df_maddint(&df2, digit);
		        if (neg(&df2) && (byte(&df2, 0) < MAXLEN)) {
			        df_incsize(&df2);
		        }
		    }

		    if (dfl_overflow(df2)) {
		        if (!toomanydigits) {

			        if (pointfound) {
			            byte(&df1, 1)--;
			            toomanydigits = 1;

			            /* round up if necessary */
			            if (digit >= 5) {
				            df_maddint(&df1, (df_int16)1);
				            if (neg(&df1)) {
                                *pdf = orgdf;
                                return(0);
                            }
			            }
			        } else {
			            return(0);
			        }
		        } else {
			        if (pointfound) byte(&df1, 1)--;
		        }

		        /* if toomanydigits = 1, the result has already
			    been cut and rounded */
		    } else {
		        byte(&df2, 1)--;
		        df1 = df2;
		        if (neg(&df1)) {
                    *pdf = orgdf;
                    return(0);
                }
		    }
	    }
	    s++;
	}

#if 1 /* Ari added, Pete corrected */

        /* If there is an exponent following the mantissa, read it. */

        if (found) {
            if (*s == 'e' || *s == 'E' || *s == 'd' || *s == 'D') {
                int   exp;
                char* ptr;
                s++;
                if (SsStrScanInt(s, &exp, &ptr)) {
                    df_int16 e = (df_int16)exp;
                    int i;
                    if (e > 0) {
                        for (i=0; i<e; i++) {
                            if (byte(&df1,1) == (unsigned char)0) {
                                break;
                            }
                            byte(&df1,1)--;
                        }
                        for (; i<e; i++) {
                            df_mmul10(&df1);
                            if (dfl_overflow(df1)) {
                                return (0);
                            }
                            byte(&df1, 1) = 0;
                        }
                    } else {
                        for (i=0; i>e; i--) {
                            byte(&df1, 1)++;
                            if (byte(&df1, 1) == (unsigned char)0) {
                                return (0);
                            }
                        }
                    }
                }
            }
        }

#endif /* 1 */


#ifdef DELETED /* by jjl */

        /* THESE ARE NEW LINES */
        /* accept extra spaces */
        while (*s == ' ')
            s++;

        /* if there is any garbage at the end reject */
        if (*s) {
            *pdf = orgdf;
            return(0);
        }
#endif /* DELETED */

	if (!found) {
        *pdf = orgdf;
        return(0);
    }

	if (sign == -1) {
	    dfl_change_sign(&df1);
	}
	*pdf = df1;

	return(1);
}

/*##**********************************************************************\
 * 
 *		dfl_nsttodfl
 * 
 */
void dfl_nsttodfl(par_nst, pdf)
        char* par_nst;
        dt_dfl_t* pdf;
{
	df_int16 signbit, length, last_exp, i, exp, incres;
	char expchar;
	dt_dfl_t df_save;
	df_int16 d_0_99;
	char nst[DFLOATNST_LEN];

        ss_purify(memset(pdf, '\0', sizeof(*pdf)));

	/* copy the n-string to the local buffer */
	if (byte(par_nst, 0) > DFNST_LEN - 1) {
	    /* the n-string is too long: truncate some
	       bytes from the end */
	    if (*par_nst == '\377') {
		    memmove(nst + 5, par_nst + 1, DFNST_LEN - 1);
	    } else {
		    memmove(nst + 1, par_nst + 1, DFNST_LEN - 1);
	    }
	    *nst = (unsigned char)DFNST_LEN - 1;
	} else {
	    memmove(nst, par_nst, byte(par_nst, 0) + 1);
	}

	/* check for empty n-string or empty mantissa */
	if (!*nst) {
	    *pdf = dfl_nan;
	    return;
	}

	if (*nst == '\1') {
	    *pdf = df_zero;
	    return;
	}

	length = (df_int16)byte(nst, 0);
	signbit = (df_int16)(nst[1] & '\200');
	if (!signbit) {
	    /* the value is negative */

	    /* invert the mantissa and the exponent */

	    byte(nst, 1) ^= '\177';
	    incres = 0;
	    for (i = length; i >= 2; i--) {
		    byte(nst, i) ^= '\377';
		    if (!incres) {
		        incres = (df_int16)++nst[i];
		    }
	    }
	}

	*pdf = df_zero;

	/* invert the first bit of the exponent */
	byte(nst, 1) ^= '\100';

	last_exp = (df_int16)(nst[2] & '\200');
	expchar = (char)((byte(nst, 1) & '\177') << 1);
	if (last_exp) {
            expchar |= '\1';
        }

	exp = (df_int16)(signed char)expchar * (df_int16)2;

	/* zero the first bit of the mantissa */
	nst[2] &= '\177';

	for (i = 2; i <= length; i++) {
	    /* get a value 0-99 from every byte and add it to the mantissa */
	    d_0_99 = (df_int16)byte(nst, i);

	    /* save the current state for the overflow case */
	    df_save = *pdf;

	    /* multiply the mantissa by 100 */
	    df_mulint(pdf, (df_int16)100);

	    /* check for overflow */
	    if (!*(char *)pdf) {
		    *pdf = df_save;
		    break;
	    }

	    /* add the next 0-100 'digit' to the mantissa */
	    df_maddint(pdf, d_0_99);

	    /* check for overflow */
	    if (neg(pdf)) {
		    if (byte(pdf, 0) >= MAXLEN) {
		        *pdf = df_save;
		        break;
		    }

		    df_incsize(pdf);

		    if (byte(pdf, 0) > MAXLEN) {
	            *pdf = dfl_nan;
		        return;
		    }
	    }

	    exp -= 2;
	}

	/* set the final exponent */
	while (exp > 0) {

	    df_mulint(pdf, (df_int16)10);

	    /* check for overflow */
	    if (!*(char *)pdf) {
		    *pdf = dfl_nan;
		    return;
	    }
	    byte(pdf, 1)++;
	    exp--;
	}

	while (exp < -255) {
	    /* set a dummy to the exponent byte */
	    byte(pdf, 1) = '\1';

	    df_mdiv10(pdf);
	    exp++;
	}

	byte(pdf, 1) = (unsigned char)(-exp);

	if (!signbit) {
	    dfl_change_sign(pdf);
	}

	df_normalize(pdf);
}

/*##**********************************************************************\
 * 
 *		dfl_rounded
 * 
 */
dt_dfl_t dfl_rounded(df, exp)
        dt_dfl_t df;
        int exp;
{
        df_int16 e = (df_int16)exp;
	df_int16 exponent;
	df_int16 multiplies;

	/* Check if df is an overflowed number */
	if (dfl_overflow(df)) {
	    /* return overflow */
	    byte(&df, 0) = '\0';
	    return(df);
	}

	exponent = (df_int16)byte(&df, 1);
	if (-e >= exponent) {
	    return(df);
	}

	multiplies = 0;

	while (exponent + e > 1) {
	    if (exponent) {
		df_mdiv10(&df);
		exponent--;
	    } else {
		++byte(&df, 1);
		df_mdiv10(&df);
		multiplies++;
		e--;
	    }
	}

	/* the last digit with rounding */
	if (exponent) {
	    df_mdiv10andround(&df);
	    exponent--;
	} else {
	    ++byte(&df, 1);
	    df_mdiv10andround(&df);
	    multiplies++;
	}

	while(multiplies--) {
	    df_mmul10(&df);
	    byte(&df, 1)--;
	}
	df_compres(&df);
	return(df);
}

/*##**********************************************************************\
 * 
 *		dfl_truncated
 * 
 */
dt_dfl_t dfl_truncated(df, exp)
        dt_dfl_t df;
        int exp;
{
        df_int16 e = (df_int16)exp;
	df_int16 exponent;
	df_int16 multiplies;

	/* Check if df is an overflowed number */
	if (dfl_overflow(df)) {
	    /* return overflow */
	    byte(&df, 0) = '\0';
	    return(df);
	}

	exponent = (df_int16)byte(&df, 1);
	if (-e >= exponent) {
	    return(df);
	}

	multiplies = 0;

	while (exponent + e > 1) {
	    if (exponent) {
		df_mdiv10(&df);
		exponent--;
	    } else {
		++byte(&df, 1);
		df_mdiv10(&df);
		multiplies++;
		e--;
	    }
	}

	/* the last digit with rounding */
	if (exponent) {
	    df_mdiv10(&df);
	    exponent--;
	} else {
	    ++byte(&df, 1);
	    df_mdiv10(&df);
	    multiplies++;
	}

	while(multiplies--) {
	    df_mmul10(&df);
	    byte(&df, 1)--;
	}
	df_compres(&df);
        return (df);
}

/*##**********************************************************************\
 * 
 *		dfl_change_sign
 * 
 */
void dfl_change_sign(pdf)
        dt_dfl_t* pdf;
{

	df_int16 i;
	unsigned char b;

	/* loop while there is carry to add */
	i = (df_int16)byte(pdf, 0);
	while (i >= 2) {
	    byte(pdf, i) ^= '\377';
	    b = ++byte(pdf, i--);

	    if (i == 1 && b == (unsigned char)'\200') {
		    /* insert a new 0 */

		    /* check for overflow */
		    if (byte(pdf, 0) >= MAXLEN) {
		        /* overflow */
		        byte(pdf, 0) = '\0';
		        return;
		    }

		    for (i = (df_int16)byte(pdf, 0); i > 1; i--) {
		        byte(pdf, i + 1) = byte(pdf, i);
		    }
		    byte(pdf, 2) = '\0';
		    byte(pdf, 0)++;
		    return;
	    }

	    if (b) {
		    /* loop to the end only inverting the bits */
		    while (i >= 2) {
		        byte(pdf, i) ^= '\377';
		        i--;
		    }
		    return;
	    }
	}
}

/*##**********************************************************************\
 * 
 *		dfl_dfltoint
 * 
 */
int dfl_dfltoint(df)
        dt_dfl_t df;
{

#if INT_BIT < DF_LONGBIT
        long l;
        
        ss_assert(sizeof(int) < sizeof(long));

        l = dfl_dfltolong(df);

        if (l > (long) INT_MAX) {
            ss_dprintf_1(("%s %d:INT_OVERFLOW\n", __FILE__, __LINE__));
            return INT_MAX;
        }

        if (l < (long) INT_MIN) {
            ss_dprintf_1(("%s %d:INT_OVERFLOW\n", __FILE__, __LINE__));
            return INT_MIN;
        }
        return (int)l;
#elif INT_BIT == DF_LONGBIT
        return((int)dfl_dfltolong(df));
#else
        You lose!
#endif /* INT??BIT */
}


/*##**********************************************************************\
 * 
 *		dfl_dfltolong
 * 
 */
long dfl_dfltolong(df)
        dt_dfl_t df;
{
	df_int16 e;
	ss_int4_t result = 0L;
	df_int16 resultpos, dfpos;

	/* Check if df is an overflowed number.  Then an error message is
	   output and maxint is returned */
	if (dfl_overflow(df)) {
	    /* return maxlong */
            ss_dprintf_1(("%s %d:DFLOAT_OVERFLOW\n", __FILE__, __LINE__));
	    return(DF_LONGMAX);
	}

        /* DANGER: If MAX_LONG is bigger than the maximum value for       */
        /*          dfloat, this comparison may cause an error             */

        if (dfl_compare(df, dfl_longtodfl(DF_LONGMAX)) > 0 ) {

            ss_dprintf_1(("%s %d:LONG_OVERFLOW\n", __FILE__, __LINE__));
            return(DF_LONGMAX);
        }

        if (dfl_compare(df, dfl_longtodfl(DF_LONGMIN)) < 0 ) {

            ss_dprintf_1(("%s %d:LONG_OVERFLOW\n", __FILE__, __LINE__));
            return(DF_LONGMIN);
        }

        e = (df_int16)byte(&df, 1);

	while (e-- > 0) {
	    df_mdiv10(&df);
	}
	
	dfpos = byte(&df, 0);
	resultpos = 0;
	
	while (dfpos > 1 && (size_t)resultpos < sizeof(result)) {
	    result |= ((ss_int4_t)(0xFF&byte(&df, dfpos)) << (resultpos * 8));
	    resultpos++;
	    dfpos--;
	}
	if (neg(&df)) {
	    while ((size_t)resultpos < sizeof(result)) {
	        result |= (0xFFL << (resultpos * 8));
	        resultpos++;
	    }
	}
        ss_dprintf_1(("dfl_dfltolong returning %ld\n", (long)result));
	return((long)result);
}

/*##**********************************************************************\
 * 
 *		dfl_dfltoasciiz_maxlen
 * 
 */
bool dfl_dfltoasciiz_maxlen(df, s, maxlen)
        dt_dfl_t df;
        char* s;
        int maxlen;
{
    dt_dfl_t tmp;
    df_int16 e, zeroscoming, digit, length, i, maxroundsleft;
    df_int16 stgprinted;
    ss_debug(char* buf = s;)

    ss_dassert((uint)maxlen > strlen(OVERFLOW_STRING));

	tmp = df;
	if (dfl_overflow(tmp)) {
        ss_dprintf_2(("dfl_dfltoasciiz_maxlen:overflow, %d\n", __LINE__));
        /* *s = '\0' ; removed 8.7.88 by jjl, next line added */
        strcpy(s, OVERFLOW_STRING);
        return(FALSE);
	}

	if (neg(&tmp)) {
	    *s++ = '-';
	    dfl_change_sign(&tmp);
	}

	e = (df_int16)byte(&tmp, 1);

	length = 0;
	zeroscoming = 0;
	stgprinted = 0;

	maxroundsleft = MAXLEN*2 + 2;
	while (maxroundsleft-- > 0 || e >= 0) {
	    if (e-- == 0) {
		    for (i = length; i >= 1; i--) {
		        s[i] = s[i - 1];
		    }
		    *s = *decimalpoints;
		    length++;
            if (length >= maxlen-1) {
                ss_debug(buf[length] = '\0';)
                ss_dprintf_2(("dfl_dfltoasciiz_maxlen:overflow, length=%d, maxlen=%d, buf=%s, line %d\n", length, maxlen, buf, __LINE__));
                strcpy(s, OVERFLOW_STRING);
                return(FALSE);
            }
	    }

	    if (!byte(&tmp, 1)) {
		    byte(&tmp, 1)++;
		    digit = df_mdiv10(&tmp);
	    } else {
		    digit = df_mdiv10(&tmp);
		    byte(&tmp, 1)++;
	    }

	    if (digit == 0 && e < 0) {
		    zeroscoming++;
	    } else if (digit || stgprinted) {
		    if (zeroscoming) {
		        while (zeroscoming) {
			        for (i = length; i >= 1; i--) {
			            s[i] = s[i - 1];
			        }
			        *s = '0';
			        length++;
                    if (length >= maxlen-1) {
                        ss_debug(buf[length] = '\0';)
                        ss_dprintf_2(("dfl_dfltoasciiz_maxlen:overflow, length=%d, maxlen=%d, buf=%s, line %d\n", length, maxlen, buf, __LINE__));
                        strcpy(s, OVERFLOW_STRING);
                        return(FALSE);
                    }
			        zeroscoming--;
		        }
		    }
		    for (i = length; i >= 1; i--) {
		        s[i] = s[i - 1];
		    }

		    *s = (char)((df_int16)'0' + digit);
		    length++;
            if (length >= maxlen-1) {
                ss_debug(buf[length] = '\0';)
                ss_dprintf_2(("dfl_dfltoasciiz_maxlen:overflow, length=%d, maxlen=%d, buf=%s, line %d\n", length, maxlen, buf, __LINE__));
                strcpy(s, OVERFLOW_STRING);
                return(FALSE);
            }
		    stgprinted = 1;
	    }
	}

	if (u_isdecimalpoint(*s)) {
	    for (i = length; i >= 1; i--) {
		    s[i] = s[i - 1];
	    }
	    *s = '0';
	    length++;
        if (length >= maxlen-1) {
            ss_debug(buf[length] = '\0';)
            ss_dprintf_2(("dfl_dfltoasciiz_maxlen:overflow, length=%d, maxlen=%d, buf=%s, line %d\n", length, maxlen, buf, __LINE__));
            strcpy(s, OVERFLOW_STRING);
            return(FALSE);
        }
	}

	s[length] = '\0';

    return(TRUE);
}

/*##**********************************************************************\
 * 
 *		dfl_dfltoasciiz_dec_maxlen
 * 
 * This function makes the output string of the dfloat number df with
 * n_dec decimals (n_dec >= 0). If n_dec < 0, the effect is the same
 * as in dfl_dfltoasciiz_maxlen.
 *
 *  Extra digits are removed by simply truncating (not rounding)
 * 
 */
bool dfl_dfltoasciiz_dec_maxlen(df, n_dec, s, maxlen)
        dt_dfl_t df;
        int n_dec;
        char *s;
        int maxlen;
{
	char *p;

	if (!dfl_dfltoasciiz_maxlen(df, s, maxlen)) {
            return(FALSE);
        }
	/* if (!*s) return; removed 8.7.88 by jjl, next line added */
	if (strcmp(s, OVERFLOW_STRING) == 0) {
            return(FALSE);
        }

	if (n_dec < 0) {
            return(TRUE);
        }

	/* find the decimal point */
	for (p = s; *p && !u_isdecimalpoint(*p); p++)
            continue;

	ss_assert(u_isdecimalpoint(*p));

	while (n_dec--) {
	    p++;
	    if (!*p) {
	        /* add an extra zero */
	        *p = '0';
		    p[1] = '\0';
	    }
	}

	/* truncate the possible remaining digits */
	p[1] = '\0';

        return(TRUE);
}


/*##**********************************************************************\
 * 
 *		dfl_dfltonst
 * 
 */
void dfl_dfltonst(p_df, nst)
        dt_dfl_t* p_df;
        char*  nst;
{
	dt_dfl_t df;
	df_int16 i, remainder, exp, exp_last, sign, incres;
	df_int16 allzeros;

	/* save overflow as empty n-string */
	if (dfl_overflow(*p_df)) {
	    *nst = '\0';
	    return;
	}

	/* handle the zero case separately */
	if (df_eqzero(p_df)) {
	    *nst = '\1';
	    *(unsigned char *)(nst + 1) = (unsigned char)'\200';
	    return;
	}

	df = *p_df;

	/* form the mantissa: 0 <= m < 1, (in base 100) */

	i = DFNST_LEN;

	sign = (df_int16)(byte(&df, 2) & '\200');
	if (sign) dfl_change_sign(&df);

	exp = -(df_int16)byte(&df, 1);

	allzeros = 1;

	/* if the exponent is not even get one digit */
	if (exp % 2) {
	    /* set a dummy to the exponent byte */
	    byte(&df, 1) = '\1';

	    remainder = 10 * df_mdiv10(&df);
	    if (remainder) allzeros = 0;

	    if (!allzeros) {
		    nst[i] = (char)(unsigned char)remainder;
		    i--;
	    }

	    exp++;
	}

	while (!df_eqzero(&df)) {
	    /* set a dummy to the exponent byte */
	    byte(&df, 1) = '\2';

	    /* get the next two digits */
	    remainder = df_mdiv10(&df);
	    remainder = remainder + 10 * df_mdiv10(&df);
	    if (remainder) allzeros = 0;

	    if (!allzeros) {
		    ss_assert(i >= 0);
		    nst[i] = (char)(unsigned char)remainder;
		    i--;
	    }

	    exp += 2;
	}

	memmove(nst + 2, nst + i + 1, (unsigned)(DFNST_LEN - i));
	nst[0] = (char)(unsigned char)(DFNST_LEN - i + 1);

	/* convert the exponent to base 100 */
	ss_assert(!(exp % 2));
	exp /= 2;

	ss_assert(-128 <= exp && exp <= 127);
	nst[1] = (char)exp;


	/* invert the first bit of the exponent */
	nst[1] ^= '\200';

	/* shift the exponent one bit right */
	exp_last = (df_int16)(nst[1] & '\1');
	nst[1] >>= 1;
	if (exp_last) {
	    nst[2] |= '\200';
	} else {
	    nst[2] &= '\177';
	}

	if (sign) {
	    /* negative: take the 2's 	complement of the exponent
	       and the mantissa */
	    incres = 0;
	    for (i = (df_int16)byte(nst, 0); i >= 1; i--) {
		    byte(nst, i) ^= '\377';
		    if (!incres) {
		        incres = (df_int16)++(nst[i]);
		    }
	    }

	    nst[1] &= '\177';
	} else {
	    /* positive */
	    nst[1] |= '\200';
	}
	ss_assert(*nst <= DFLOATNST_LEN - 1);
}

void dfl_print(df)
        dt_dfl_t df;
{

#ifdef WINDOWS
        ss_error;
#else /* WINDOWS */

	char buffer[30];

	dfl_dfltoasciiz_maxlen(df, buffer, 30);

	printf("%s", buffer);
#endif /* WINDOWS */
}

int u_isdecimalpoint(char c)
{
        /* fixed by LAMPIO */
        return(strchr(decimalpoints, c) != NULL && c != '\0');
}

static void dfn_chsign(p_df) dflnst_t *p_df; {

	df_int16 i;
	unsigned char b;

	/* loop while there is carry to add */
	i = MAXLEN + 1 ;
	while (i >= MAXLEN + 3 - dfn_len(p_df)) {
	    byte(p_df, i) ^= '\377';
	    b = ++byte(p_df, i--);

	    if (i < MAXLEN + 3 - dfn_len(p_df) - 1 && b == (unsigned char)'\200') {
		    /* insert a new 0 */

		    /* check for overflow */

		    if (i == 1) {
		        /* return overflow */
		        byte(p_df, 0) = '\0';
		        return;
		    }
		    byte(p_df, i) = '\0';
		    byte(p_df, 0)++;

		    return;
	    }

	    if (b) {
		    /* loop to the end only inverting the bits */
		    while (i >= MAXLEN + 3 - dfn_len(p_df) ) {
		        byte(p_df, i) ^= '\377';
		        i--;
		    }
		    return;
	    }
	}
}

static void dfn_maddint(dflnst_t *p_df, df_int16 smallint)
{
	df_int16 i;

	ss_assert(smallint >= 0 && smallint <= 255);

	if (byte(p_df, MAXLEN + 1) + smallint > 255) {
	    byte(p_df, MAXLEN + 1) = (unsigned char)((df_int16)byte(p_df, MAXLEN + 1)
				     + smallint);
	    for (i = MAXLEN ; i >= MAXLEN + 3 - dfn_len(p_df); i--) {
		    if (++byte(p_df, i)) return;
	    }

	    byte(p_df,i) = 1;
	    byte(p_df,0)++;
	} else {
	    byte(p_df, MAXLEN + 1) =
		  (unsigned char)((df_int16)byte(p_df, MAXLEN + 1) + smallint);
	}
}


/* Multiplies a dflnst_t by 10 num times.
*/

static df_int16 dfn_mmul10n(dflnst_t *p_df, df_int16 dif, df_int16 num)
{
	df_int16 i, carry, mulres, incr;

	if (dfn_neg(p_df)) {
	    if (num > 10) {
		incr = 99;
	    } else {
		incr = 9;
	    }

	    while (dif > 0) {
		if (dfn_len(p_df) > MAXLEN) {
		    return(dif);
		} else {
		    carry = 0;
		    for (i = MAXLEN + 1;
			 i >= (df_int16)MAXLEN + (df_int16)3 - (df_int16)byte(p_df, 0);
			 i--) {
			    mulres = (df_int16)byte(p_df, i) * num + carry;
			    carry = (df_int16)int16_msb(mulres);
			    byte(p_df, i) = int16_lsb(mulres);
		    }

		    if (carry < incr) {
			    byte(p_df, 0)++;
			    carry += (df_int16)byte(p_df, i) - (df_int16)incr;
			    byte(p_df, i) = int16_lsb(carry);
		    }

		    byte(p_df, 1)++;
		    if (num > 10) {
			    byte(p_df, 1)++;
		    }
		}
		dif--;
	    }

	    if (!(byte(p_df, MAXLEN + 3 - dfn_len(p_df)) & '\200')) {
		    byte(p_df, 0)++;
	    }

	    return(0);
	} else {
	    while (dif > 0) {
		if (dfn_len(p_df) > MAXLEN) {
		    return(dif);
		}

		carry = 0;
		for (i = MAXLEN + 1;
		     i >= MAXLEN + 3 - (df_int16)byte(p_df, 0);
		     i--) {
		    mulres = (df_int16)byte(p_df, i) * num + carry;
		    carry = (df_int16)int16_msb(mulres);
		    byte(p_df, i) = int16_lsb(mulres);
		}

		if (carry) {
		    byte(p_df, 0)++;
                byte(p_df, i) = int16_lsb(carry);
		}
		byte(p_df, 1)++;
		if(num > 10) {
		    byte(p_df, 1)++;
		}
		dif--;
	    }

	    if (byte(p_df, MAXLEN + 3 - dfn_len(p_df)) & '\200') {
		    byte(p_df, 0)++;
	    }

	    return(0);
	}
}

static df_int16 dfn_mdiv10(p_df) dflnst_t *p_df; {

	df_int16 i, carry, addres, negative;

	/* ss_assert(byte(p_df, 1)); */

	byte(p_df, 1)--;

	if (dfn_neg(p_df)) {
	    negative = 1;
	    dfn_chsign(p_df);
	} else {
	    negative = 0;
	}

	carry = 0;

	for (i = MAXLEN + 3 - dfn_len(p_df); i <= MAXLEN + 1 ; i++) {
	    addres = (df_int16)byte(p_df, i) + (df_int16)256 * carry;
	    carry = addres % 10;
	    byte(p_df, i) = (unsigned char)(addres / 10);
	}

	if (negative) {
	    dfn_chsign(p_df);
	}

	return(carry);
}

static void dfn_mdiv10andround(p_df) dflnst_t *p_df; {

	df_int16 i, remainder, negative;

	negative = dfn_neg(p_df);
	remainder = dfn_mdiv10(p_df);

	/* the last digit is rounded up */
	if (negative) {
	    if (remainder >= 5) {
		    dfn_chsign(p_df);
		    dfn_maddint(p_df, (df_int16)1);
		    dfn_chsign(p_df);
	    } else {
		    if (byte(p_df,0) == '\2' && !byte(p_df, MAXLEN + 1)) {

		        for(i = 2; i < MAXLEN + 1; i++) {
			        byte(p_df,i) = '\0';
		        }
		    }
	    }
	} else {
	    if (remainder >= 5) {
		    dfn_maddint(p_df, (df_int16)1);
	    }
	}
}

/* Copies a dt_dfl_t to a dflnst_t, which has 8 bits more precision
   in the mantissa. The bytes are placed in the dflnst_t in the
   same order (msb first), as they were in the original dt_dfl_t,
   but lsb is at the last position of the dflnst_t (in dfloat,
   msb is in the first position). Therefore, the empty space in
   the dflnst_t is after the exponent and before the start of the
   mantissa (in dfloat, it is after the mantissa). */

static void dnst_copylong(p_df, p_dfn)
        dt_dfl_t *p_df;
        dflnst_t *p_dfn;
{
	df_int16 i;

	for (i = dfn_len(p_df); i > 1; i--) {
	    byte(p_dfn, MAXLEN + 1 + i - dfn_len(p_df)) = byte(p_df, i);
	}

	byte(p_dfn, 0) = byte(p_df, 0);
	byte(p_dfn, 1) = byte(p_df, 1);
}

static void df_maddint(dt_dfl_t *pdf, df_int16 smallint)
{
	df_int16 i, l;

	ss_assert(smallint >= 0 && smallint <= 255);

	l = (df_int16)byte(pdf, 0);

	if ((df_int16)byte(pdf, l) + smallint > 255) {
	    byte(pdf, l) = (unsigned char)((df_int16)byte(pdf, l) + smallint);
	    for (i = l - 1; i >= 2; i--) {
		    if (++byte(pdf, i))
                    break;
	    }
	} else {
	    byte(pdf, l) =
	        (unsigned char)((df_int16)byte(pdf, l) + smallint);
	}
}

static void df_normalize(pdf) dt_dfl_t *pdf; {

	dt_dfl_t df;

	while (byte(pdf, 1)) {
	    df = *pdf;
	    if (df_mdiv10(&df))
                break;
		if (byte(&df, 0) > MAXLEN) {
	        *pdf = dfl_nan;
		    return;
		}
	    *pdf = df;
	}
	df_compres(pdf);
}


static void df_compres(pdf)
        dt_dfl_t *pdf;
{

	df_int16 i;

	if (byte(pdf, 0) > MAXLEN) {
	    *pdf = dfl_nan;
		return;
	}

	if (byte(pdf, 0) == '\2')
            return;

	while ((byte(pdf, 2) == '\0' && !(byte(pdf, 3) & (unsigned char)'\200')) ||
	       (byte(pdf, 2) == (unsigned char)'\377' && byte(pdf, 3) & (unsigned char)'\200')) {

	    for (i = 3; i <= (df_int16)byte(pdf, 0); i++) {
		    byte(pdf, i-1) = byte(pdf, i);
	    }
	    if (--byte(pdf, 0) == '\2')
                break;
	    if (byte(pdf, 0) > MAXLEN) {
	        *pdf = dfl_nan;
		    return;
	    }
	}
}

static void df_mmul10(pdf) dt_dfl_t *pdf; {

	df_int16 i, carry, mulres, negative = 0;

	if (neg(pdf)) {
	    negative = 1;
	    dfl_change_sign(pdf);
	}

	carry = 0;

	for (i = (df_int16)byte(pdf, 0); i >= 2; i--) {
	    mulres = (df_int16)byte(pdf, i) * 10 + carry;
	    carry = (df_int16)int16_msb(mulres);
	    byte(pdf, i) = int16_lsb(mulres);
	}

	if (!++byte(pdf, 1)) {
	    byte(pdf, 0) = '\0'; /* overflow */
	    return;
	}

	if (carry || neg(pdf)) {
	    if (byte(pdf, 0) >= MAXLEN) {
		    byte(pdf, 0) = '\0'; /* overflow */
		    return;
	    }

	    byte(pdf, 0)++;
	    for (i = (df_int16)byte(pdf, 0); i >= 3; i--)
	        byte(pdf, i) = byte(pdf, i - 1);
            byte(pdf, 2) = int16_lsb(carry);
	}
	if (negative) {
	    dfl_change_sign(pdf);
	}
}

static df_int16 df_mdiv10(pdf) dt_dfl_t *pdf; {

	df_int16 i, carry, addres, negative;

	ss_assert(byte(pdf, 1));
	byte(pdf, 1)--;

	negative = 0;
	if (neg(pdf)) {
	    negative = 1;
	    dfl_change_sign(pdf);
	}

	carry = 0;

	for (i = 2; i <= (df_int16)byte(pdf, 0); i++) {
	    addres = (df_int16)byte(pdf, i) + 256 * carry;
	    carry = addres % 10;
	    byte(pdf, i) = (unsigned char)(addres / 10);
	}

	if (negative) {
	    dfl_change_sign(pdf);
	}
	df_compres(pdf);
	return(carry);
}

static void df_mdiv10andround(pdf) dt_dfl_t *pdf; {

	df_int16 remainder;
    bool isnegative = neg(pdf);

	remainder = df_mdiv10(pdf);

	if (remainder >= 5) {
	    /* the last digit is rounded up */
	    if (isnegative) {
		    dfl_change_sign(pdf);
		    df_maddint(pdf, (df_int16)1);
		    /* check for overflow JL */
		    if (neg(pdf) && (byte(pdf, 0) < MAXLEN)) {
		        df_incsize(pdf);
		    }
		    dfl_change_sign(pdf);
	    } else {
		    df_maddint(pdf, (df_int16)1);
		    /* check for overflow JL */
		    if (neg(pdf) && (byte(pdf, 0) < MAXLEN)) {
		        df_incsize(pdf);
		    }
	    }
	}
}

/* the I/O functions follow */


#ifdef	DEBUGGING_FUNCTIONS

void df_printbits(df) dt_dfl_t df; {

	df_int16 i, i2;

	for (i = 0; i <= (df_int16)byte(&df, 0); i++) {
	    for(i2 = 0; i2 <= 7; i2++) {
		    printf("%c", *((char *)&df + i) &
			     ((unsigned char)'\200' >> i2) ? '1' : '0');
	    }
	    printf(" ");
	}
	printf("\n");
}

static void df_printarea(area, n) unsigned char *area; df_int16 n; {

	while (n--) printf("%d ", (unsigned)*area++);
	printf("\n");
}

#endif /* DEBUGGING_FUNCTIONS */

static void df_incsize(pdf)
        dt_dfl_t *pdf;
{
	df_int16 i;
	for (i = (df_int16)byte(pdf, 0); i >= 2; i--)
            byte(pdf, i+1) = byte(pdf, i);
	byte(pdf, 2) = '\0';
	byte(pdf, 0)++;
}

int df_eqzero(dt_dfl_t* p_df)
{
	if (byte(p_df, 0) == '\2' && byte(p_df, 2) == '\0') {
            return(1);
        }
	return(0);
}

static void df_mulint(dt_dfl_t *pdf, df_int16 smallint)
{
	/* only for positive df */
	df_int16 i, carry, mulres;

	ss_assert(0 <= smallint && smallint <= 255);

	carry = 0;
	for (i = (df_int16)byte(pdf, 0); i >= 2; i--) {
	    mulres = (df_int16)byte(pdf, i) * smallint + carry;
            carry = (df_int16)int16_msb(mulres);
            byte(pdf, i) = int16_lsb(mulres);
	}

	if (carry || neg(pdf)) {
	    if (byte(pdf, 0) >= MAXLEN) {
		    byte(pdf, 0) = '\0'; /* overflow */
		    return;
	    }
	    byte(pdf, 0)++;
	    for (i = (df_int16)byte(pdf, 0); i >= 3; i--)
	        byte(pdf, i) = byte(pdf, i - 1);
            byte(pdf, 2) = int16_lsb(carry);
	}
}

static void df_mulint2(dt_dfl_t *pdf1, df_int16 smallint, dt_dfl_t *pdf2)
{
	/* only for positive df */
	df_int16 i, carry, mulres;

	ss_assert(0 <= smallint && smallint <= 255);

	carry = 0;
	for (i = (df_int16)byte(pdf1, 0); i >= 2; i--) {
	    mulres = (df_int16)byte(pdf1, i) * smallint + carry;
	    carry = (df_int16)int16_msb(mulres);
	    byte(pdf2, i) = int16_lsb(mulres);
	}
	byte(pdf2, 0) = byte(pdf1, 0);
	byte(pdf2, 1) = byte(pdf1, 1);
	if (carry || neg(pdf2)) {
	    if (byte(pdf2, 0) == MAXLEN) {
		    byte(pdf2, 0) = '\0'; /* overflow */
		    return;
	    }
	    byte(pdf2, 0)++;
	    for (i = (df_int16)byte(pdf2, 0); i >=3; i--)
	        byte(pdf2, i) = byte(pdf2, i - 1);
	    byte(pdf2, 2) = int16_lsb(carry);
	}
}

int   dfl_negative(dt_dfl_t* p_dfl)
{
        ss_dassert(!dfl_overflow(*p_dfl));
        return (neg(p_dfl) != 0);
}

