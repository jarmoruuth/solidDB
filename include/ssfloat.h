/*************************************************************************\
**  source       * ssfloat.h
**  directory    * ss
**  description  * float.h & description of floats
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


#ifndef SSFLOAT_H
#define SSFLOAT_H

#include "ssenv.h"

#if !defined(NO_ANSI)
#  include <float.h>
#endif

#include "sslimits.h"
#include "ssc.h"

#if defined(SS_CROSSENDIAN) /* !SS_LSB1ST */

#define SS_DEFAULT_FLOAT_CROSSENDIAN

typedef struct {
            unsigned mantissa_15_0  : 16;
            unsigned mantissa_22_16 :  7;
            unsigned exponent       :  8;
            unsigned sign_bit       :  1;
        } ss_float_t;

typedef struct {
            unsigned mantissa_47_32 : 16;
            unsigned mantissa_51_48 :  4;
            unsigned exponent       : 11;
            unsigned sign_bit       :  1;
            unsigned mantissa_15_0  : 16;
            unsigned mantissa_31_16 : 16;
        } ss_double_t;

#elif defined(SS_LSB1ST)

#define SS_DEFAULT_FLOAT_LSB1ST

typedef struct {
            unsigned mantissa_15_0  : 16;
            unsigned mantissa_22_16 :  7;
            unsigned exponent       :  8;
            unsigned sign_bit       :  1;
        } ss_float_t;

typedef struct {
            unsigned mantissa_15_0  : 16;
            unsigned mantissa_31_16 : 16;
            unsigned mantissa_47_32 : 16;
            unsigned mantissa_51_48 :  4;
            unsigned exponent       : 11;
            unsigned sign_bit       :  1;
        } ss_double_t;

#else /* !SS_LSB1ST */

#define SS_DEFAULT_FLOAT_MSB1ST

typedef struct {
            unsigned sign_bit       :  1;
            unsigned exponent       :  8;
            unsigned mantissa_22_16 :  7;
            unsigned mantissa_15_0  : 16;
        } ss_float_t;

typedef struct {
            unsigned sign_bit       :  1;
            unsigned exponent       : 11;
            unsigned mantissa_51_48 :  4;
            unsigned mantissa_47_32 : 16;
            unsigned mantissa_31_16 : 16;
            unsigned mantissa_15_0  : 16;
        } ss_double_t;

#endif /* SS_LSB1ST */

extern ss_double_t ss_mpd, ss_mnd, ss_lpd, ss_lnd, ss_mpf, ss_mnf, ss_lpf, ss_lnf;

#if defined(SCO) || (defined(UNIX) && defined(BANYAN))

/* Don't trust SCO's float.h constants!  These are the real boundaries.
   Apparently the number system is IEEE without the infinities: mpd is
   the largest value below IEEE infinity; mpf is actually one order of
   magnitude beyond the IEEE single-precision range, because SCO does
   all the calculations with 8087 extended reals anyway; lpf is the
   smallest denormalized IEEE float. */

#define ss_define_float_constants \
        double ss_dummy_double_for_alignment = 0.0; \
        ss_double_t ss_mpd = {0xFFFF, 0xFFFF, 0xFFFF, 0xF, 0x7FE, 0}; \
        ss_double_t ss_mnd = {0xFFFF, 0xFFFF, 0xFFFF, 0xF, 0x7FE, 1}; \
        ss_double_t ss_lpd = {0x0001, 0x0000, 0x0000, 0x0, 0x000, 0}; \
        ss_double_t ss_lnd = {0x0001, 0x0000, 0x0000, 0x0, 0x000, 1}; \
        ss_double_t ss_mpf = {0x0000, 0xE000, 0xFFFF, 0xF, 0x47E, 0}; \
        ss_double_t ss_mnf = {0x0000, 0xE000, 0xFFFF, 0xF, 0x47E, 1}; \
        ss_double_t ss_lpf = {0x0000, 0x2000, 0x0000, 0x0, 0x36A, 0}; \
        ss_double_t ss_lnf = {0x0000, 0x2000, 0x0000, 0x0, 0x36A, 1};

#define NO_ANSI_FLOAT

#elif defined(SS_DEFAULT_FLOAT_LSB1ST)

/* The altmath number system is IEEE without infinities and denormals.
   mpf is actually one order of magnitude beyond the IEEE single-precision
   range, probably because all the calculations are done with higher precision. */

#define ss_define_float_constants \
        double ss_dummy_double_for_alignment = 0.0; \
        ss_double_t ss_mpd = {0xFFFF, 0xFFFF, 0xFFFF, 0xF, 0x7FE, 0}; \
        ss_double_t ss_mnd = {0xFFFF, 0xFFFF, 0xFFFF, 0xF, 0x7FE, 1}; \
        ss_double_t ss_lpd = {0x0000, 0x0000, 0x0000, 0x0, 0x001, 0}; \
        ss_double_t ss_lnd = {0x0000, 0x0000, 0x0000, 0x0, 0x001, 1}; \
        ss_double_t ss_mpf = {0x0000, 0xE000, 0xFFFF, 0xF, 0x47E, 0}; \
        ss_double_t ss_mnf = {0x0000, 0xE000, 0xFFFF, 0xF, 0x47E, 1}; \
        ss_double_t ss_lpf = {0x0000, 0x0000, 0x0000, 0x0, 0x381, 0}; \
        ss_double_t ss_lnf = {0x0000, 0x0000, 0x0000, 0x0, 0x381, 1};

#elif defined(SS_DEFAULT_FLOAT_CROSSENDIAN)

/* The altmath number system is IEEE without infinities and denormals.
   mpf is actually one order of magnitude beyond the IEEE single-precision
   range, probably because all the calculations are done with higher precision. */

#define ss_define_float_constants \
        double ss_dummy_double_for_alignment = 0.0; \
        ss_double_t ss_mpd = {0xFFFF, 0xF, 0x7FE, 0, 0xFFFF, 0xFFFF}; \
        ss_double_t ss_mnd = {0xFFFF, 0xF, 0x7FE, 1, 0xFFFF, 0xFFFF}; \
        ss_double_t ss_lpd = {0x0000, 0x0, 0x001, 0, 0x0000, 0x0000}; \
        ss_double_t ss_lnd = {0x0000, 0x0, 0x001, 1, 0x0000, 0x0000}; \
        ss_double_t ss_mpf = {0xFFFF, 0xF, 0x47E, 0, 0x0000, 0xE000}; \
        ss_double_t ss_mnf = {0xFFFF, 0xF, 0x47E, 1, 0x0000, 0xE000}; \
        ss_double_t ss_lpf = {0x0000, 0x0, 0x381, 0, 0x0000, 0x0000}; \
        ss_double_t ss_lnf = {0x0000, 0x0, 0x381, 1, 0x0000, 0x0000};

#elif defined(SS_DEFAULT_FLOAT_MSB1ST)

/* The number system is IEEE without infinities and denormals.
   mpf is actually one order of magnitude beyond the IEEE single-precision
   range, probably because all the calculations are done with higher precision. */

#define ss_define_float_constants \
        double ss_dummy_double_for_alignment = 0.0; \
        ss_double_t ss_mpd = {0, 0x7FE, 0XF, 0xFFFF, 0xFFFF, 0xFFFF}; \
        ss_double_t ss_mnd = {1, 0x7FE, 0XF, 0xFFFF, 0xFFFF, 0xFFFF}; \
        ss_double_t ss_lpd = {0, 0x001, 0x0, 0x0000, 0x0000, 0x0000}; \
        ss_double_t ss_lnd = {1, 0x001, 0x0, 0x0000, 0x0000, 0x0000}; \
        ss_double_t ss_mpf = {0, 0x47E, 0xF, 0xFFFF, 0xE000, 0x0000}; \
        ss_double_t ss_mnf = {1, 0x47E, 0xF, 0xFFFF, 0xE000, 0x0000}; \
        ss_double_t ss_lpf = {0, 0x381, 0x0, 0x0000, 0x0000, 0x0000}; \
        ss_double_t ss_lnf = {1, 0x381, 0x0, 0x0000, 0x0000, 0x0000};
#else /* Unknown system */
#error You must define float constants for this environment!
#define ss_define_float_constants \
        double ss_dummy_double_for_alignment = 0.0; \
        ss_double_t ss_mpd = DBL_MAX; \
        ss_double_t ss_mnd = (-DBL_MAX); \
        ss_double_t ss_lpd = DBL_MIN; \
        ss_double_t ss_lnd = (-DBL_MIN); \
        ss_double_t ss_mpf = FLT_MAX; \
        ss_double_t ss_mnf = (-FLT_MAX); \
        ss_double_t ss_lpf = FLT_MIN; \
        ss_double_t ss_lnf = (-FLT_MIN);

#endif

/* The float constants are defined as doubles because that's the C way */

#define SS_MOST_POSITIVE_FLOAT (*(double*)(void*)&ss_mpf)
/* SS_MOST_POSITIVE_FLOAT = FLT_MAX */
#define SS_MOST_NEGATIVE_FLOAT (*(double*)(void*)&ss_mnf)
#define SS_LEAST_POSITIVE_FLOAT (*(double*)(void*)&ss_lpf)
/* SS_LEAST_POSITIVE_FLOAT != FLT_MIN, because it might not be normalized */
#define SS_LEAST_NEGATIVE_FLOAT (*(double*)(void*)&ss_lnf)

#define SS_MOST_POSITIVE_DOUBLE (*(double*)(void*)&ss_mpd)
/* SS_MOST_POSITIVE_DOUBLE = DBL_MAX */
#define SS_MOST_NEGATIVE_DOUBLE (*(double*)(void*)&ss_mnd)
#define SS_LEAST_POSITIVE_DOUBLE (*(double*)(void*)&ss_lpd)
/* SS_LEAST_POSITIVE_DOUBLE != DBL_MIN, because it might not be normalized */
#define SS_LEAST_NEGATIVE_DOUBLE (*(double*)(void*)&ss_lnd)


/* These constants define the biggest portable range of double
 * and float values
 */
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)

/* 
 * These limits are taken from compiler header file. We do not use them
 * because MySQL allows values outside of these ranges. So right now
 * we just assume that double values we get from MySQL are ok. This may 
 * limit database portability between different systems, tough.
 */

#define SS_MOST_POSITIVE_PORTABLE_DOUBLE    (1.7976931348623158e+308)
#define SS_LEAST_POSITIVE_PORTABLE_DOUBLE   (2.2250738585072014e-308)
#define SS_MOST_POSITIVE_PORTABLE_FLOAT     ((float) 3.402823466e+38)
#define SS_LEAST_POSITIVE_PORTABLE_FLOAT    ((float) 1.175494351e-38)

#define SS_MOST_NEGATIVE_PORTABLE_DOUBLE    (-1.7976931348623158e+308)
#define SS_LEAST_NEGATIVE_PORTABLE_DOUBLE   (-2.2250738585072014e-308)
#define SS_MOST_NEGATIVE_PORTABLE_FLOAT     ((float) -3.402823466e+38)
#define SS_LEAST_NEGATIVE_PORTABLE_FLOAT    ((float) -1.175494351e-38)

#else /* defined(SS_MYSQL) || defined(SS_MYSQL_AC) */

#define SS_MOST_POSITIVE_PORTABLE_DOUBLE    (8.98846567431157854e+307)
#define SS_LEAST_POSITIVE_PORTABLE_DOUBLE   (2.2250738585072014e-308)
#define SS_MOST_POSITIVE_PORTABLE_FLOAT     ((float) 1.7014117e+38)
#define SS_LEAST_POSITIVE_PORTABLE_FLOAT    ((float) 1.175494351e-38)

#define SS_MOST_NEGATIVE_PORTABLE_DOUBLE    (-8.98846567431157854e+307)
#define SS_LEAST_NEGATIVE_PORTABLE_DOUBLE   (-2.2250738585072014e-308)
#define SS_MOST_NEGATIVE_PORTABLE_FLOAT     ((float) -1.7014117e+38)
#define SS_LEAST_NEGATIVE_PORTABLE_FLOAT    ((float) -1.175494351e-38)

#endif /* defined(SS_MYSQL) || defined(SS_MYSQL_AC) */

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)

/* 
 * Because above limits do not work mith MySQL we just assume that double values 
 * we get from MySQL are ok. This may limit database portability between different 
 * systems, tough.
 */

#define SS_DOUBLE_IS_PORTABLE(d) TRUE
#define SS_FLOAT_IS_PORTABLE(f)  TRUE

#else /* defined(SS_MYSQL) || defined(SS_MYSQL_AC) */

#define SS_DOUBLE_IS_PORTABLE(d) \
((d) == (d) &&\
 (((d) < 0.0) ?\
  (SS_MOST_NEGATIVE_PORTABLE_DOUBLE <= (d) &&\
   (d) <= SS_LEAST_NEGATIVE_PORTABLE_DOUBLE) :\
  ((d) == 0.0 ||\
   (SS_LEAST_POSITIVE_PORTABLE_DOUBLE <= (d) &&\
    (d) <= SS_MOST_POSITIVE_PORTABLE_DOUBLE))\
 )\
)

#define SS_FLOAT_IS_PORTABLE(f) \
((f) == (f) &&\
 (((f) < 0.0) ?\
  (SS_MOST_NEGATIVE_PORTABLE_FLOAT <= (f) &&\
   (f) <= SS_LEAST_NEGATIVE_PORTABLE_FLOAT) :\
  ((f) == 0.0 ||\
   (SS_LEAST_POSITIVE_PORTABLE_FLOAT <= (f) &&\
    (f) <= SS_MOST_POSITIVE_PORTABLE_FLOAT))\
 )\
)

#endif /* defined(SS_MYSQL) || defined(SS_MYSQL_AC) */

/* SS_DOUBLE_STORE_TO_IEEE_MSB1ST_BUF and SS_DOUBLE_COPY_FROM_IEEE_MSB1ST_BUF
 * macroses have to use 'union' in order to avoid agressive gcc aliasing
 * optimizations (gcc -O2 and more).
 * a1b - stands for array of 1-byte values.
 * a4b - stands for array of 4-byte values.
 */

#if defined(SS_CROSSENDIAN)

#define SS_DOUBLE_STORE_TO_IEEE_MSB1ST_BUF(p_buf, d) \
{\
        union { double d; ss_byte_t a1b[8]; } r = { d }; \
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        pb[0] = r.a1b[3];\
        pb[1] = r.a1b[2];\
        pb[2] = r.a1b[1];\
        pb[3] = r.a1b[0];\
        pb[4] = r.a1b[7];\
        pb[5] = r.a1b[6];\
        pb[6] = r.a1b[5];\
        pb[7] = r.a1b[4];\
}

#define SS_DOUBLE_COPY_FROM_IEEE_MSB1ST_BUF(p_d, p_buf) \
{\
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        union { double d; ss_uint4_t a4b[2]; } r; \
        r.a4b[0] = \
            ((ss_uint4_t)(pb[0]) << 24U) |\
            ((ss_uint4_t)(pb[1]) << 16U) |\
            ((ss_uint4_t)(pb[2]) << 8U)  |\
            (ss_uint4_t)(pb[3]);\
        r.a4b[1] = \
            ((ss_uint4_t)(pb[4]) << 24U) |\
            ((ss_uint4_t)(pb[5]) << 16U) |\
            ((ss_uint4_t)(pb[6]) << 8U)  |\
            (ss_uint4_t)(pb[7]);\
        *p_d = r.d; \
}

#elif defined SS_LSB1ST /* Intel byte order */

#define SS_DOUBLE_STORE_TO_IEEE_MSB1ST_BUF(p_buf, d) \
{\
        union { double d; ss_byte_t a1b[8]; } r = { d }; \
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        pb[0] = r.a1b[7];\
        pb[1] = r.a1b[6];\
        pb[2] = r.a1b[5];\
        pb[3] = r.a1b[4];\
        pb[4] = r.a1b[3];\
        pb[5] = r.a1b[2];\
        pb[6] = r.a1b[1];\
        pb[7] = r.a1b[0];\
}

#define SS_DOUBLE_COPY_FROM_IEEE_MSB1ST_BUF(p_d, p_buf) \
{\
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        union { double d; ss_uint4_t a4b[2]; } r; \
        r.a4b[0] = \
            ((ss_uint4_t)(pb[4]) << 24U) |\
            ((ss_uint4_t)(pb[5]) << 16U) |\
            ((ss_uint4_t)(pb[6]) << 8U)  |\
            (ss_uint4_t)(pb[7]);\
        r.a4b[1] = \
            ((ss_uint4_t)(pb[0]) << 24U) |\
            ((ss_uint4_t)(pb[1]) << 16U) |\
            ((ss_uint4_t)(pb[2]) << 8U)  |\
            (ss_uint4_t)(pb[3]);\
        *p_d = r.d; \
}

#else /* SS_LSB1ST */


# ifdef UNALIGNED_LOAD

/* eg. Motorola 680x0 */

#define SS_DOUBLE_STORE_TO_IEEE_MSB1ST_BUF(p_buf, d) \
        { *(double*)(p_buf) = (d); }

#define SS_DOUBLE_COPY_FROM_IEEE_MSB1ST_BUF(p_d, p_buf) \
        { *p_d = *(double*)p_buf; }

# else /* UNALIGNED LOAD */

/* eg. MIPS, SPARC, PowerPC, PA-RISC */

#define SS_DOUBLE_STORE_TO_IEEE_MSB1ST_BUF(p_buf, d) \
{\
        union { double d; ss_byte_t a1b[8]; } r; \
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
		r.d = d; \
        pb[0] = r.a1b[0];\
        pb[1] = r.a1b[1];\
        pb[2] = r.a1b[2];\
        pb[3] = r.a1b[3];\
        pb[4] = r.a1b[4];\
        pb[5] = r.a1b[5];\
        pb[6] = r.a1b[6];\
        pb[7] = r.a1b[7];\
}

#define SS_DOUBLE_COPY_FROM_IEEE_MSB1ST_BUF(p_d, p_buf) \
{\
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        union { double d; ss_uint4_t a4b[2]; } r; \
        r.a4b[0] = \
            ((ss_uint4_t)(pb[0]) << 24U) |\
            ((ss_uint4_t)(pb[1]) << 16U) |\
            ((ss_uint4_t)(pb[2]) << 8U)  |\
            (ss_uint4_t)(pb[3]);\
        r.a4b[1] = \
            ((ss_uint4_t)(pb[4]) << 24U) |\
            ((ss_uint4_t)(pb[5]) << 16U) |\
            ((ss_uint4_t)(pb[6]) << 8U)  |\
            (ss_uint4_t)(pb[7]);\
        *p_d = r.d; \
}

# endif /* UNALIGNED LOAD */

#endif /* SS_LSB1ST / MSB1ST */

#ifndef SS_FLOAT_STORE_TO_IEEE_MSB1ST_BUF

#define SS_FLOAT_STORE_TO_IEEE_MSB1ST_BUF(p_buf, f) \
{\
        union {\
            ss_uint4_t u4;\
            float f4;\
        } r;\
        ss_byte_t *pb = (ss_byte_t*)(p_buf);\
        r.f4 = (float)(f);\
        pb[0] = (ss_byte_t)(r.u4 >> (3 * SS_CHAR_BIT));\
        pb[1] = (ss_byte_t)(r.u4 >> (2 * SS_CHAR_BIT));\
        pb[2] = (ss_byte_t)(r.u4 >> (1 * SS_CHAR_BIT));\
        pb[3] = (ss_byte_t)r.u4;\
}
#endif /* !defined(SS_FLOAT_STORE_TO_IEEE_MSB1ST_BUF) */

#ifndef SS_FLOAT_COPY_FROM_IEEE_MSB1ST_BUF

#define SS_FLOAT_COPY_FROM_IEEE_MSB1ST_BUF(p_f, p_buf) \
{\
        union { float f; ss_uint4_t ai; } r; \
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        r.ai = \
            ((ss_uint4_t)(pb[0]) << (3 * SS_CHAR_BIT)) |\
            ((ss_uint4_t)(pb[1]) << (2 * SS_CHAR_BIT)) |\
            ((ss_uint4_t)(pb[2]) << (1 * SS_CHAR_BIT)) |\
            (ss_uint4_t)(pb[3]);\
        *p_f = r.f; \
}

#endif /* !defined(SS_FLOAT_COPY_FROM_IEEE_MSB1ST_BUF) */

#define SS_DOUBLE_IEEE_MSB1ST_BUF_TO_64BITKEY(p_key, p_buf) \
do {\
        ss_uint4_t _u4_hi;\
        ss_uint4_t _u4_lo;\
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        _u4_hi =\
            ((ss_uint4_t)pb[0] << (3 * SS_CHAR_BIT)) |\
            ((ss_uint4_t)pb[1] << (2 * SS_CHAR_BIT)) |\
            ((ss_uint4_t)pb[2] << (1 * SS_CHAR_BIT)) |\
            (ss_uint4_t)pb[3];\
        _u4_lo =\
            ((ss_uint4_t)pb[4] << (3 * SS_CHAR_BIT)) |\
            ((ss_uint4_t)pb[5] << (2 * SS_CHAR_BIT)) |\
            ((ss_uint4_t)pb[6] << (1 * SS_CHAR_BIT)) |\
            (ss_uint4_t)(pb[7]);\
        if (_u4_hi &\
            ((ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1)))\
        {\
            _u4_hi = ~_u4_hi;\
            _u4_lo = ~_u4_lo;\
        } else {\
            _u4_hi ^= (ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1);\
        }\
        SsInt8Set2Uint4s((ss_int8_t*)(p_key), _u4_hi, _u4_lo);\
} while (FALSE)

#define SS_FLOAT_IEEE_MSB1ST_BUF_TO_64BITKEY(p_key, p_buf) \
do {\
        ss_uint4_t _u4;\
        ss_byte_t *pb = (ss_byte_t*)p_buf;\
        _u4 =\
            ((ss_uint4_t)pb[0] << (3 * SS_CHAR_BIT)) |\
            ((ss_uint4_t)pb[1] << (2 * SS_CHAR_BIT)) |\
            ((ss_uint4_t)pb[2] << (1 * SS_CHAR_BIT)) |\
            (ss_uint4_t)(pb[3]);\
        if (_u4 & ((ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1)))\
        {\
            _u4 = ~_u4;\
        } else {\
            _u4 ^= (ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1);\
        }\
        SsInt8SetUint4((ss_int8_t*)(p_key), _u4);\
} while (FALSE)

#define SS_DOUBLE_64BITKEY_TO_IEEE_MSB1ST_BUF(key, p_buf) \
do {\
        ss_uint4_t _u4_hi = SsInt8GetMostSignificantUint4(key);\
        ss_uint4_t _u4_lo = SsInt8GetLeastSignificantUint4(key);\
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        if (_u4_hi &\
            ((ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1)))\
        {\
            _u4_hi ^= (ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1);\
        } else {\
            _u4_hi = ~_u4_hi;\
            _u4_lo = ~_u4_lo;\
        }\
        pb[0] = _u4_hi << (3 * SS_CHAR_BIT);\
        pb[1] = _u4_hi << (2 * SS_CHAR_BIT);\
        pb[2] = _u4_hi << (1 * SS_CHAR_BIT);\
        pb[3] = _u4_hi << (0 * SS_CHAR_BIT);\
        pb[4] = _u4_lo << (3 * SS_CHAR_BIT);\
        pb[5] = _u4_lo << (2 * SS_CHAR_BIT);\
        pb[6] = _u4_lo << (1 * SS_CHAR_BIT);\
        pb[7] = _u4_lo << (0 * SS_CHAR_BIT);\
} while (FALSE)

#define SS_FLOAT_64BITKEY_TO_IEEE_MSB1ST_BUF(key, p_buf) \
do {\
        ss_uint4_t _u4 = SsInt8GetLeastSignificantUint4(key);\
        ss_byte_t *pb = (ss_byte_t*)p_buf; \
        if (_u4 & ((ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1)))\
        {\
            _u4 ^= (ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1);\
        } else {\
            _u4 = ~_u4;\
        }\
        pb[0] = _u4 << (3 * SS_CHAR_BIT);\
        pb[1] = _u4 << (2 * SS_CHAR_BIT);\
        pb[2] = _u4 << (1 * SS_CHAR_BIT);\
        pb[3] = _u4 << (0 * SS_CHAR_BIT);\
} while (FALSE)

#if !defined(SS_CROSSENDIAN)

#ifdef SS_LSB1ST
#define SS_DOUBLE_MS_U4_IDX 1
#define SS_DOUBLE_LS_U4_IDX 0
#else /* SS_LSB1ST */
#define SS_DOUBLE_MS_U4_IDX 0
#define SS_DOUBLE_LS_U4_IDX 1
#endif /* SS_LSB1ST */

#define SS_DOUBLE_TO_64BITKEY(p_key, d) \
do {\
        union { double d; ss_uint4_t a4b[2]; } r; \
        ss_uint4_t _u4_hi;\
        ss_uint4_t _u4_lo;\
        r.d = d; \
        _u4_hi = r.a4b[SS_DOUBLE_MS_U4_IDX];\
        _u4_lo = r.a4b[SS_DOUBLE_LS_U4_IDX];\
        if (_u4_hi &\
            ((ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1)))\
        {\
            _u4_hi = ~_u4_hi;\
            _u4_lo = ~_u4_lo;\
        } else {\
            _u4_hi ^= (ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1);\
        }\
        SsInt8Set2Uint4s((ss_int8_t*)(p_key), _u4_hi, _u4_lo);\
} while (FALSE)


#define SS_FLOAT_TO_64BITKEY(p_key, f) \
do {\
        ss_uint4_t _u4 = *(ss_uint4_t*)&(f);\
        if (_u4 & ((ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1)))\
        {\
            _u4 = ~_u4;\
        } else {\
            _u4 ^= (ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1);\
        }\
        SsInt8SetUint4((ss_int8_t*)(p_key), _u4);\
} while (FALSE)

#define SS_64BITKEY_TO_DOUBLE(key, p_d) \
do {\
        ss_uint4_t _u4_hi = SsInt8GetMostSignificantUint4(key);\
        ss_uint4_t _u4_lo = SsInt8GetLeastSignificantUint4(key);\
        union { double d; ss_uint4_t a4b[2]; } r; \
        if (_u4_hi &\
            ((ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1)))\
        {\
            _u4_hi ^= (ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1);\
        } else {\
            _u4_hi = ~_u4_hi;\
            _u4_lo = ~_u4_lo;\
        }\
        r.a4b[SS_DOUBLE_MS_U4_IDX] = _u4_hi;\
        r.a4b[SS_DOUBLE_LS_U4_IDX] = _u4_lo;\
        *p_d = r.d; \
} while (FALSE)


#define SS_64BITKEY_TO_FLOAT(key, p_f) \
do {\
        ss_uint4_t _u4 = SsInt8GetLeastSignificantUint4(key);\
        if (_u4 & ((ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1)))\
        {\
            _u4 ^= (ss_uint4_t)1 << (SS_CHAR_BIT * sizeof(ss_uint4_t) - 1);\
        } else {\
            _u4 = ~_u4;\
        }\
        *(ss_uint4_t*)(p_f) = _u4;\
} while (FALSE)


#else /* !SS_CROSSENDIAN */

#define SS_DOUBLE_TO_64BITKEY(p_key, d) \
do {\
        ss_byte_t buf[sizeof(double)];\
        SS_DOUBLE_STORE_TO_IEEE_MSB1ST_BUF(buf, d);\
        SS_DOUBLE_IEEE_MSB1ST_BUF_TO_64BITKEY(p_key, buf);\
} while (FALSE)

#define SS_FLOAT_TO_64BITKEY(p_key, f) \
do {\
        ss_byte_t buf[sizeof(float)];\
        SS_FLOAT_STORE_TO_IEEE_MSB1ST_BUF(buf,f);\
        SS_FLOAT_IEEE_MSB1ST_BUF_TO_64BITKEY(p_key,buf);\
} while (FALSE)

#define SS_64BITKEY_TO_DOUBLE(key, p_d) \
do {\
        ss_byte_t buf[sizeof(double)];\
        SS_DOUBLE_64BITKEY_TO_IEEE_MSB1ST_BUF(key, buf);\
        SS_DOUBLE_COPY_FROM_IEEE_MSB1ST_BUF(p_d, buf);\
} while (FALSE)

#define SS_64BITKEY_TO_FLOAT(key, p_f) \
do {\
        ss_byte_t buf[sizeof(float)];\
        SS_FLOAT_64BITKEY_TO_IEEE_MSB1ST_BUF(key, buf);\
        SS_FLOAT_COPY_FROM_IEEE_MSB1ST_BUF(p_f, buf);\
} while (FALSE)

#endif /* !SS_CROSSENDIAN */

#endif /* SSFLOAT_H */

