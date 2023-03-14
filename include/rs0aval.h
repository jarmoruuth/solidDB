/*************************************************************************\
**  source       * rs0aval.h
**  directory    * res
**  description  * Attribute value functions
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


#ifndef RS0AVAL_H
#define RS0AVAL_H

#include <ssstddef.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssfloat.h>
#include <ssint8.h>

#include <uti0va.h>
#include <uti0bufva.h>

#include <su0types.h>
#include <su0vers.h>
#include <su0chcvt.h>

#include <dt0dfloa.h>
#include <dt0date.h>

#include "rs0types.h"
#include "rs0sysi.h"
#include "rs0atype.h"

#define RS_INTERNAL

#include "rs1aval.h"

#define RS_ODBC_SCALARFUNCS


#define RS_CONVERT_FUNCTIONS        48
#       define RS_FN_CVT_CONVERT        0x00000001L

#define RS_NUMERIC_FUNCTIONS        49
#       define RS_FN_NUM_ABS            0x00000001L
#       define RS_FN_NUM_ACOS           0x00000002L
#       define RS_FN_NUM_ASIN           0x00000004L
#       define RS_FN_NUM_ATAN           0x00000008L
#       define RS_FN_NUM_ATAN2          0x00000010L
#       define RS_FN_NUM_CEILING        0x00000020L
#       define RS_FN_NUM_COS            0x00000040L
#       define RS_FN_NUM_COT            0x00000080L
#       define RS_FN_NUM_EXP            0x00000100L
#       define RS_FN_NUM_FLOOR          0x00000200L
#       define RS_FN_NUM_LOG            0x00000400L
#       define RS_FN_NUM_MOD            0x00000800L
#       define RS_FN_NUM_SIGN           0x00001000L
#       define RS_FN_NUM_SIN            0x00002000L
#       define RS_FN_NUM_SQRT           0x00004000L
#       define RS_FN_NUM_TAN            0x00008000L
#       define RS_FN_NUM_PI             0x00010000L
#       define RS_FN_NUM_RAND           0x00020000L
#       define RS_FN_NUM_DEGREES        0x00040000L
#       define RS_FN_NUM_LOG10          0x00080000L
#       define RS_FN_NUM_POWER          0x00100000L
#       define RS_FN_NUM_RADIANS        0x00200000L
#       define RS_FN_NUM_ROUND          0x00400000L
#       define RS_FN_NUM_TRUNCATE       0x00800000L

#define RS_STRING_FUNCTIONS         50
#       define RS_FN_STR_CONCAT         0x00000001L
#       define RS_FN_STR_INSERT         0x00000002L
#       define RS_FN_STR_LEFT           0x00000004L
#       define RS_FN_STR_LTRIM          0x00000008L
#       define RS_FN_STR_LENGTH         0x00000010L
#       define RS_FN_STR_LOCATE         0x00000020L
#       define RS_FN_STR_LCASE          0x00000040L
#       define RS_FN_STR_REPEAT         0x00000080L
#       define RS_FN_STR_REPLACE        0x00000100L
#       define RS_FN_STR_RIGHT          0x00000200L
#       define RS_FN_STR_RTRIM          0x00000400L
#       define RS_FN_STR_SUBSTRING      0x00000800L
#       define RS_FN_STR_UCASE          0x00001000L
#       define RS_FN_STR_ASCII          0x00002000L
#       define RS_FN_STR_CHAR           0x00004000L
#       define RS_FN_STR_DIFFERENCE     0x00008000L
#       define RS_FN_STR_LOCATE_2       0x00010000L
#       define RS_FN_STR_SOUNDEX        0x00020000L
#       define RS_FN_STR_SPACE          0x00040000L

#define RS_SYSTEM_FUNCTIONS         51
#       define RS_FN_SYS_USERNAME       0x00000001L
#       define RS_FN_SYS_DBNAME         0x00000002L
#       define RS_FN_SYS_IFNULL         0x00000004L

#define RS_TIMEDATE_FUNCTIONS       52
#       define RS_FN_TD_NOW             0x00000001L
#       define RS_FN_TD_CURDATE         0x00000002L
#       define RS_FN_TD_DAYOFMONTH      0x00000004L
#       define RS_FN_TD_DAYOFWEEK       0x00000008L
#       define RS_FN_TD_DAYOFYEAR       0x00000010L
#       define RS_FN_TD_MONTH           0x00000020L
#       define RS_FN_TD_QUARTER         0x00000040L
#       define RS_FN_TD_WEEK            0x00000080L
#       define RS_FN_TD_YEAR            0x00000100L
#       define RS_FN_TD_CURTIME         0x00000200L
#       define RS_FN_TD_HOUR            0x00000400L
#       define RS_FN_TD_MINUTE          0x00000800L
#       define RS_FN_TD_SECOND          0x00001000L
#       define RS_FN_TD_TIMESTAMPADD    0x00002000L
#       define RS_FN_TD_TIMESTAMPDIFF   0x00004000L
#       define RS_FN_TD_DAYNAME         0x00008000L
#       define RS_FN_TD_MONTHNAME       0x00010000L

#define RS_CONVERT_BIGINT           53
#define RS_CONVERT_BINARY           54
#define RS_CONVERT_BIT              55
#define RS_CONVERT_CHAR             56
#define RS_CONVERT_DATE             57
#define RS_CONVERT_DECIMAL          58
#define RS_CONVERT_DOUBLE           59
#define RS_CONVERT_FLOAT            60
#define RS_CONVERT_INTEGER          61
#define RS_CONVERT_LONGVARCHAR      62
#define RS_CONVERT_NUMERIC          63
#define RS_CONVERT_REAL             64
#define RS_CONVERT_SMALLINT         65
#define RS_CONVERT_TIME             66
#define RS_CONVERT_TIMESTAMP        67
#define RS_CONVERT_TINYINT          68
#define RS_CONVERT_VARBINARY        69
#define RS_CONVERT_VARCHAR          70
#define RS_CONVERT_LONGVARBINARY    71
#       define RS_CVT_CHAR              0x00000001L
#       define RS_CVT_NUMERIC           0x00000002L
#       define RS_CVT_DECIMAL           0x00000004L
#       define RS_CVT_INTEGER           0x00000008L
#       define RS_CVT_SMALLINT          0x00000010L
#       define RS_CVT_FLOAT             0x00000020L
#       define RS_CVT_REAL              0x00000040L
#       define RS_CVT_DOUBLE            0x00000080L
#       define RS_CVT_VARCHAR           0x00000100L
#       define RS_CVT_LONGVARCHAR       0x00000200L
#       define RS_CVT_BINARY            0x00000400L
#       define RS_CVT_VARBINARY         0x00000800L
#       define RS_CVT_BIT               0x00001000L
#       define RS_CVT_TINYINT           0x00002000L
#       define RS_CVT_BIGINT            0x00004000L
#       define RS_CVT_DATE              0x00008000L
#       define RS_CVT_TIME              0x00010000L
#       define RS_CVT_TIMESTAMP         0x00020000L
#       define RS_CVT_LONGVARBINARY     0x00040000L

#define RS_TIMEDATE_ADD_INTERVALS   109
#define RS_TIMEDATE_DIFF_INTERVALS  110
#       define RS_FN_TSI_FRAC_SECOND    0x00000001L
#       define RS_FN_TSI_SECOND         0x00000002L
#       define RS_FN_TSI_MINUTE         0x00000004L
#       define RS_FN_TSI_HOUR           0x00000008L
#       define RS_FN_TSI_DAY            0x00000010L
#       define RS_FN_TSI_WEEK           0x00000020L
#       define RS_FN_TSI_MONTH          0x00000040L
#       define RS_FN_TSI_QUARTER        0x00000080L
#       define RS_FN_TSI_YEAR           0x00000100L


/* arithmetical operators, DANGER! copied from sqlint.h */
#define RS_AROP_PLUS       0
#define RS_AROP_MINUS      1
#define RS_AROP_TIMES      2
#define RS_AROP_DIVIDE     3
#define RS_AROP_POWER      4
#define RS_AROP_UNMINUS    5

#if defined(RS_INTERNAL) || defined(RS_USE_MACROS)
/***** INTERNAL ONLY BEGIN *****/

/* Internal aval bit flags.
 */
#define RA_NULL         SU_BFLAG_BIT(0) /* SQL NULL flag */
#define RA_CONVERTED    SU_BFLAG_BIT(1) /* If set, converted value in ra_
                                           union can be used */
#define RA_LITERAL      SU_BFLAG_BIT(2) /* The value is from a literal */
#define RA_DESC         SU_BFLAG_BIT(3) /* The value is inverted to desc */
#define RA_CHCVT        SU_BFLAG_BIT(4) /* Is character conversion already
                                           done to the data area.*/
#define RA_VTPLREF      SU_BFLAG_BIT(5) /* ref. to v-tuple */

#define RA_AGGR         SU_BFLAG_BIT(6)  /* aggregate aval, used in aggregate
                                           calculations, va does not match
                                           to ra_.* contents. NOT USED!!! */
#define RA_BLOB         SU_BFLAG_BIT(7)  /* aval contains a BLOB reference,
                                          * note: va_testblob(aval->ra_rdva)
                                          * is asserted to be true!
                                          */
#ifdef SS_MME
/* The min/max flags below are NOT taken into account in any comparisons.
   They can just be set and tested. */
#define RA_MIN          SU_BFLAG_BIT(8) /* Minimum value. */
#define RA_MAX          SU_BFLAG_BIT(9) /* Maximum value. */
#endif

#define RA_ACCINIT      SU_BFLAG_BIT(10) /* Accelerator spesific flag,
                                            is value initialized. */

#define RA_ONLYCONVERTED SU_BFLAG_BIT(11) /* aval has only converted format
                                           * of the value. This flag can never
                                           * be set without RA_CONVERTED also
                                           * being set.
                                           */
#define RA_FLATVA       SU_BFLAG_BIT(12) /* va is stored in the local
                                          * buffer that is a member of the
                                          * aval structure (ra_vabuf)
                                          */
#define RA_UNKNOWN      SU_BFLAG_BIT(13) /* SQL value unknown flag */
#define RA_EXTERNALFLATVA SU_BFLAG_BIT(14) /* Debug flag to separate inside
                                          * vtpl stored value  from 
                                          * external stored value.
                                          */
#define RA_COLLATION_KEY SU_BFLAG_BIT(15) /* original value converted to
                                           * collation key, a.k.a.
                                           * weight string. Reverse conversion
                                           * is NOT possible
                                           */ 
#define RA_RDVA ra_va

struct rsattrvaluestruct {

        ss_debug(int    ra_check;)  /* rs_check_t RSCHK_ATTRVALUE   */

        su_bflag_t      ra_flags;   /* Null, converted, literal, etc. */
        va_t*           ra_va;      /* Attribute value in va-format,
                                       always present, if !RA_NULL &&
                                       !RA_ONLYCONVERTED. */
        rs_aval_accinfo_t* ra_accinfo; /* Accelerator specific info. */
        union {
            /* va is converted to original datatype
             * when first time needed (in arithmetics etc)
             */
            char*       str;        /* string value, pointer into ra_rdva */
            dt_date_t*  date;       /* date value, pointer into ra_rdva */
            long        l;          /* integer value */
            float       f;          /* float value   */
            double      d;          /* double value  */
            dt_dfl_t    dfl;        /* dfloat value */
            ss_int8_t   i8;         /* 8 byte integer */
        } ra_;
        union {
                va_t va;            /* to match type */
                ss_byte_t to_adjust_desired_size[
                        64 - SS_ALIGNMENT - 
                        sizeof(su_bflag_t) -
                        sizeof(va_t*) -
                        sizeof(rs_aval_accinfo_t*) -
                        (((sizeof(dt_dfl_t) + SS_ALIGNMENT - 1) /
                          SS_ALIGNMENT) *
                         SS_ALIGNMENT)];
        } ra_vabuf;                 /* local buffer for flat storage
                                       for small values */
        ss_debug(int    ra_check_end;)  /* rs_check_t RSCHK_ATTRVALUE_END   */
}; /* rs_aval_t */

#define _RS_AVAL_GETLONG_(cd, atype, aval) \
        (!SU_BFLAG_TEST((aval)->ra_flags, RA_CONVERTED) \
         ? (SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED), \
            (aval)->ra_.l = va_getlong((aval)->RA_RDVA)) \
         : (aval)->ra_.l)

#define _RS_AVAL_ISMIN_(cd, atype, aval) \
        (SU_BFLAG_TEST((aval)->ra_flags, RA_MIN))

#define _RS_AVAL_SETMIN_(cd, atype, aval, flag) \
        do { \
            if (flag) { \
                SU_BFLAG_CLEAR((aval)->ra_flags, RA_MAX); \
                SU_BFLAG_SET((aval)->ra_flags, RA_MIN); \
            } else { \
                SU_BFLAG_CLEAR((aval)->ra_flags, RA_MIN); \
            } \
        } while (FALSE)

#define _RS_AVAL_ISMAX_(cd, atype, aval) \
        (SU_BFLAG_TEST((aval)->ra_flags, RA_MAX))

#define _RS_AVAL_SETMAX_(cd, atype, aval, flag) \
        do { \
            if (flag) { \
                SU_BFLAG_CLEAR((aval)->ra_flags, RA_MIN); \
                SU_BFLAG_SET((aval)->ra_flags, RA_MAX); \
            } else { \
                SU_BFLAG_CLEAR((aval)->ra_flags, RA_MAX); \
            } \
        } while (FALSE)

#define _RS_AVAL_ISUNKNOWN_(cd, atype, aval) \
        (((aval) == NULL) ? TRUE : SU_BFLAG_TEST((aval)->ra_flags, RA_UNKNOWN))

/***** INTERNAL ONLY END *****/
#endif /* defined(RS_INTERNAL) || defined(RS_USE_MACROS) */

extern char* (*rs_aval_print_externaldatatype)(rs_sysi_t* cd, rs_atype_t* atype, rs_aval_t* aval);

#ifdef RS_USE_MACROS

#define rs_aval_getlong(cd, atype, aval)  _RS_AVAL_GETLONG_(cd, atype, aval)

#define rs_aval_ismin(cd, atype, aval)  _RS_AVAL_ISMIN_(cd, atype, aval)
#define rs_aval_setmin(cd, atype, aval, flag) \
        _RS_AVAL_SETMIN_(cd, atype, aval, flag)
#define rs_aval_ismax(cd, atype, aval)  _RS_AVAL_ISMAX_(cd, atype, aval)
#define rs_aval_setmax(cd, atype, aval, flag) \
        _RS_AVAL_SETMAX_(cd, atype, aval, flag)
#define rs_aval_isunknown(cd, atype, aval) _RS_AVAL_ISUNKNOWN_(cd, atype, aval)

#else

long rs_aval_getlong(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval);

#ifdef SS_MME
/* WARNING - THE FUNCTIONS BELOW ARE NOT TESTED IN res/test. */
bool rs_aval_ismin(
        rs_sysi_t*      cd,
        rs_atype_t*     atype,
        rs_aval_t*      aval);

void rs_aval_setmin(
        rs_sysi_t*      cd,
        rs_atype_t*     atype,
        rs_aval_t*      aval,
        bool            flag);

bool rs_aval_ismax(
        rs_sysi_t*      cd,
        rs_atype_t*     atype,
        rs_aval_t*      aval);

void rs_aval_setmax(
        rs_sysi_t*      cd,
        rs_atype_t*     atype,
        rs_aval_t*      aval,
        bool            flag);
#endif /* SS_MME */

bool rs_aval_isunknown(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

#endif /* RS_USE_MACROS */

rs_aval_t* rs_aval_create(
        void*       cd,
        rs_atype_t* atype
);

rs_aval_t* rs_aval_sql_create(
        void*       cd,
        rs_atype_t* atype
);

SS_INLINE void rs_aval_createbuf(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  avalbuf);

rs_aval_t* rs_aval_createconst(
        void*       cd,
        rs_atype_t* atype,
        char*       sqlvalstr,
        rs_err_t**  errhandle
);

char* rs_aval_print_ex(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes
);

char* rs_aval_print(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

void rs_aval_output(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes,
        void        (*outputfun)(void*, void*),
        void*       outputpar);

void rs_aval_free(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

SS_INLINE void rs_aval_freebuf(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  avalbuf);

SS_INLINE void rs_aval_setnull(
        rs_sysi_t*  cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

void rs_aval_setunknown(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

rs_aval_t* rs_aval_copy(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

void rs_aval_move(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  res_aval,
        rs_aval_t*  aval);

void rs_aval_copybuf2(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  res_aval,
        rs_aval_t*  aval);

void rs_aval_copybuf3(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  res_aval,
        rs_aval_t*  aval);

void rs_aval_copybuf4(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  res_aval,
        rs_aval_t*  aval);

SS_INLINE int rs_aval_sql_isnull(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

bool rs_aval_isnull(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

int rs_aval_cmp(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        uint        relop
);

int rs_aval_cmp_nullallowed(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        uint        relop);

int rs_aval_sql_cmpwitherrh(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        rs_err_t**  p_errh);


bool rs_aval_cmp_simple(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        uint        relop
);

bool rs_aval_cmpwitherrh(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        uint        relop,
        bool*       p_succp,
        rs_err_t**  p_errh);

bool rs_aval_like(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        rs_atype_t* patt_atype,
        rs_aval_t*  patt_aval,
        rs_atype_t* esc_atype,
        rs_aval_t*  esc_aval
);

uint rs_aval_sql_like(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        rs_atype_t* patt_atype,
        rs_aval_t*  patt_aval,
        rs_atype_t* esc_atype,
        rs_aval_t*  esc_aval,
        rs_err_t**  p_errh);

uint rs_aval_describefun(
        void* cd,
        char* fname,
        void** phandle,
        bool* p_pure,
        rs_atype_t** p_defpartype,
        uint parno
);

void rs_aval_releasefunhandle(
    void* cd,
    void* fhandle
);

bool rs_aval_callfun(
        void*        cd,
        char*        fname,
        void*        fhandle,
        rs_atype_t*  atypearray[],
        rs_aval_t*   avalarray[],
        rs_atype_t** p_res_atype,
        rs_aval_t**  p_res_aval,
        void**       cont,
        rs_err_t**   errhandle
);

bool rs_aval_callaggr(
        void*        cd,
        char*        fname,
        void*        fhandle,
        uint         op,
        rs_atype_t*  atypearray[],
        rs_aval_t*   avalarray[],
        ulong*       p_count,
        rs_ttype_t*  res_ttype,
        rs_tval_t*   res_tval,
        uint         sql_attr_n,
        void**       resourcepp,
        rs_err_t**   p_errh
);

/* NOTE: Following services are not members of SQL-funblock */

void rs_aval_set(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  destaval,
        rs_aval_t*  srcaval
);

void rs_aval_setva(
        void*       cd,
        rs_atype_t* destatype,
        rs_aval_t*  destaval,
        va_t*       src_va
);

void rs_aval_setvaref(
        void*       cd,
        rs_atype_t* destatype,
        rs_aval_t*  destaval,
        va_t*       src_va
);

SS_INLINE void rs_aval_setflatvarefflag(
        void*       cd,
        rs_atype_t* destatype,
        rs_aval_t*  destaval
);

#ifdef SS_DEBUG

void rs_aval_resetexternalflatva(
        void*       cd,
        rs_atype_t* destatype,
        rs_aval_t*  destaval);

#endif /* SS_DEBUG */

void rs_aval_unlinkvaref(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

void rs_aval_insertrefdva(
        void*       cd,
        rs_atype_t* destatype,
        rs_aval_t*  destaval,
        refdva_t    src_refdva
);

va_t* rs_aval_va(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

uint rs_aval_copydata(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        char*       data,
        uint        maxlen,
        uint*       p_copylen
);

uint rs_aval_copyasciizwithdtformat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        char*       dtformat,
        char*       asciiz,
        uint        maxlen
);

uint rs_aval_copyasciiz(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        char*       asciiz,
        uint        maxlen
);

SS_INLINE void* rs_aval_getdata(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        ulong*      p_length
);

char* rs_aval_getasciiz(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

SS_INLINE ss_int8_t rs_aval_getint8(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

SS_INLINE float rs_aval_getfloat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

SS_INLINE double rs_aval_getdouble(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

SS_INLINE dt_dfl_t rs_aval_getdfloat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);

SS_INLINE dt_date_t* rs_aval_getdate(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval
);                

bool rs_aval_setdatawithdtformat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        char*       data,
        uint        len,
        char*       dtformat
);

bool rs_aval_setdatawithdtformat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        char*       data,
        uint        len,
        char*       dtformat
);

bool rs_aval_setdata(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        char*       data,
        uint        len
);

bool rs_aval_setasciizwithdtformat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        char*       asciiz,
        char*       dtformat
);

bool rs_aval_setasciiz(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        char*       asciiz
);

bool rs_aval_setlong(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        long        l
);

#ifdef NO_ANSI_FLOAT
bool rs_aval_setfloat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        double       f
);
#else
bool rs_aval_setfloat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        float       f
);
#endif

bool rs_aval_setdouble(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        double      d
);

bool rs_aval_setdfloat(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        dt_dfl_t    dfl
);

bool rs_aval_setdate(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        dt_date_t*  date
);


long rs_aval_cnvlong(
            void*       cd,
            rs_atype_t* atype,
            rs_aval_t*  aval
);

float rs_aval_cnvfloat(
            void*       cd,
            rs_atype_t* atype,
            rs_aval_t*  aval
);

double rs_aval_cnvdouble(
            void*       cd,
            rs_atype_t* atype,
            rs_aval_t*  aval
);

dt_dfl_t rs_aval_cnvdfloat(
            void*       cd,
            rs_atype_t* atype,
            rs_aval_t*  aval
);

bool rs_aval_cnvdate(
            void*       cd,
            rs_atype_t* atype,
            rs_aval_t*  aval,
            dt_date_t*  p_date);

bool rs_aval_cnvtime(
            void*       cd,
            rs_atype_t* atype,
            rs_aval_t*  aval,
            dt_date_t*  p_time);

bool rs_aval_cnvtimestamp(
            void*       cd,
            rs_atype_t* atype,
            rs_aval_t*  aval,
            dt_date_t*  p_timestamp);

bool rs_aval_isblob(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval
);

bool rs_aval_trimchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool truncate);

bool rs_aval_isliteral(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_setliteralflag(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool literal_flag);

bool rs_aval_isdesc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_setdesc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

bool rs_aval_asctodesc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_desctoasc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_likepatasctodesc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        int old_esc,
        int new_esc);

va_t* rs_aval_invnull_va(
        void* cd,
        rs_atype_t* atype);

void rs_aval_setchcvt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

bool rs_aval_ischcvt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);


ulong rs_aval_funinfo(
        void* cd,
        int funclassid);

char* rs_aval_funfind(
        void* cd,
        char* funname);

void rs_aval_removevtplref(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

#ifdef SS_MYSQL

#define rs_aval_avfunglobalinit(chcollation)
#define rs_aval_avfunglobaldone()

#else /* SS_MYSQL */

void rs_aval_avfunglobalinit(
        su_chcollation_t chcollation);

void rs_aval_avfunglobaldone(
        void);

#endif /* SS_MYSQL */

typedef enum {
        RSAVR_FAILURE,
        RSAVR_SUCCESS,
        RSAVR_TRUNCATION
} rs_avalret_t;

typedef int RS_AVALRET_T;

/* Set native C types to aval */

/* UTF8 string */
RS_AVALRET_T rs_aval_setstr_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_setcdata_ext( /* 8-bit or UTF-8 char data */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_setlong_ext(   /* long (32-bit) integer */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setint8_ext(   /* 64-bit integer */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setdouble_ext( /* double */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setfloat_ext(  /* float */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setdate_ext(   /* dt_date_t */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        dt_datesqltype_t datesqltype,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setdfloat_ext( /* dt_dfl_t */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setwcs_ext(    /* 2-byte char null-terminated string */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setwdata_ext(  /* 2-byte char buffer (not null-terminated) */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

void rs_aval_setdata_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        void* buf, /* buffer contents must be legal for the data type! */
        size_t bufsize);

RS_AVALRET_T rs_aval_setbdata_ext(   /* binary buffer (not null-terminated) */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        void* buf,
        size_t bufsize,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_set8bitstr_ext( /* ordinary asciiz string */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_set8bitcdata_ext(   /* character buffer (not null-terminated) */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_setUTF8data_ext(   /* UTF8 buffer */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_setUTF8str_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh);



/* Set native C types to _corresponding_ SQL types */

/* UTF8 string */
RS_AVALRET_T rs_aval_setstr_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_setcdata_raw( /* 8-bit or UTF-8 char data */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);


RS_AVALRET_T rs_aval_setlong_raw(   /* long (32-bit) integer */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setdouble_raw( /* double */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setfloat_raw(  /* float */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setdate_raw(   /* dt_date_t */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        dt_datesqltype_t datesqltype,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setdfloat_raw( /* dt_dfl_t */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setwcs_raw(    /* 2-byte char null-terminated string */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_setwdata_raw(  /* 2-byte char buffer (not null-terminated) */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

#define rs_aval_setbdata_raw rs_aval_setbdata_ext

RS_AVALRET_T rs_aval_setUTF8data_raw(   /* UTF8 buffer */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_setUTF8str_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_set8bitstr_raw( /* ordinary asciiz string */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_set8bitcdata_raw(   /* character buffer (not null-terminated) */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_setint8_raw(   /* BIG (64-bit) integer */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh);

/* Get native C types */

RS_AVALRET_T rs_aval_converttostr(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_givestr(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t** p_str_give,
        size_t* p_length,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_giveUTF8str(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t** p_UTF8str_give,
        size_t* p_length,
        rs_err_t** p_errh);

RS_AVALRET_T rs_aval_converttolong( /* long (32-bit) integer */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh);
bool rs_aval_converttofloat(    /* float */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh);
bool rs_aval_converttodouble(   /* double */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh);
bool rs_aval_converttodfloat(   /* dt_dfl_t */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_convertto8bitstr(   /* ordinary 8-bit asciiz string */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_converttoUTF8(    /* UTF-8 buffer (null-terminated string) */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh);

/* Measure size needed for UTF-8 buffer */
size_t rs_aval_requiredUTF8bufsize(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

/* Measure length needed for 8-bit string buffer */
size_t rs_aval_required8bitstrbufsize(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

size_t rs_aval_requiredstrbufsize(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

/* Measure length needed for wcs buffer */
size_t rs_aval_requiredwcsbufsize(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

RS_AVALRET_T rs_aval_converttowcs(      /* 2-byte char string (null-terminated) */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_converttodate(     /* dt_date_t DATE */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_converttotime(     /* dt_date_t TIME */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_converttotimestamp(/* dt_date_t TIMESTAMP */
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh);
RS_AVALRET_T rs_aval_converttobinary(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        void* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh);


/* aval->aval conversions */

RS_AVALRET_T rs_aval_assign_ext(
	void*       cd,
	rs_atype_t* dst_atype,
	rs_aval_t*  dst_aval,
	rs_atype_t* src_atype,
	rs_aval_t*  src_aval,
	rs_err_t**  p_errh);

RS_AVALRET_T rs_aval_sql_assign(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh);

RS_AVALRET_T rs_aval_convert_ext(
	void*       cd,
	rs_atype_t* dst_atype,
	rs_aval_t*  dst_aval,
	rs_atype_t* src_atype,
	rs_aval_t*  src_aval,
	rs_err_t**  p_errh);

void rs_aval_setchar2binassignallowed(
        bool allowed);

#ifdef SS_MYSQL

#define rs_aval_arithglobaldone()

#else /* SS_MYSQL */

void rs_aval_arithglobaldone(
        void);

#endif /* SS_MYSQL */

bool rs_aval_arith_ext(
        void*        cd,
        rs_atype_t** p_res_atype,
        rs_aval_t**  p_res_aval,
        rs_atype_t*  atype1,
        rs_aval_t*   aval1,
        rs_atype_t*  atype2,
        rs_aval_t*   aval2,
        uint         op,
        rs_err_t**   p_errh);

int rs_aval_cmp3_nullallowed(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh);

int rs_aval_cmp3_notnull(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh);

bool rs_aval_doublearith(
        void* cd,
        rs_atype_t* res_atype,
        rs_aval_t* res_aval,
        double d1,
        double d2,
        uint op,
        rs_err_t** p_errh);


#define rs_aval_arith       rs_aval_arith_ext
#define rs_aval_assign      rs_aval_assign_ext
#define rs_aval_convert     rs_aval_convert_ext
#define rs_aval_cmp3        rs_aval_cmp3_notnull

typedef bool rs_aval_loadblobcallbackfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        size_t sizelimit);

extern rs_aval_loadblobcallbackfun_t* rs_aval_loadblob;

void rs_aval_setloadblobcallbackfun(
        void* cd,
        rs_aval_loadblobcallbackfun_t* callbackfp);

void rs_aval_setloadblobsizelimit(
        void* cd,
        size_t sizelimit);

size_t rs_aval_getloadblobsizelimit(
        void* cd);

void rs_aval_settsdiffmode(
        bool old);

void rs_aval_sethsbstatecallback(
        rs_sysi_t* cd,
        char* (*fun)(rs_sysi_t* cd));

void rs_aval_sethsbrolecallback(
        rs_sysi_t* cd,
        char* (*fun)(rs_sysi_t* cd));

void rs_aval_sethsbconnectstatuscallback(
        rs_sysi_t* cd,
        char* (*fun)(rs_sysi_t* cd));

typedef su_ret_t (rs_aval_blobrefcallbackfun_t)(
        rs_sysi_t* cd,
        va_t*  blobref_va,
        su_err_t** p_errh);

typedef void (rs_aval_blobidnullifycallback_t)(
        va_t* va);

typedef bool (rs_aval_isblobg2callbackfun_t)(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

typedef ss_int8_t (rs_aval_getblobg2idorsizecallbackfun_t)(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_globalinstallblobrefcallbacks(
        rs_aval_blobrefcallbackfun_t* blobref_inc_callbackfun,
        rs_aval_blobrefcallbackfun_t* blobref_dec_callbackfun,
        rs_aval_blobidnullifycallback_t* blobidnullify_callbackfun,
        rs_aval_isblobg2callbackfun_t* isblobg2_callbackfun,
        rs_aval_getblobg2idorsizecallbackfun_t* getblobg2size_callbackfun,
        rs_aval_getblobg2idorsizecallbackfun_t* getblobg2id_callbackfun);

void rs_aval_nullifyblobid(rs_sysi_t* cd,
                           rs_atype_t* atype,
                           rs_aval_t* aval);

bool rs_aval_isblobg2(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

ss_int8_t rs_aval_getblobsize(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

ss_int8_t rs_aval_getblobid(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

va_t* rs_aval_deconvert(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

#ifdef SS_COLLATION
va_t* rs_aval_getkeyva(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        su_collation_t* collation,
        rs_attrtype_t attrtype,
        bool ascending,
        ss_byte_t buf[/* bufsize */],
        size_t bufsize,
        size_t prefixlen /* characters */
        );

void rs_aval_convert_to_collation_key(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        su_collation_t* collation);

#endif /* SS_COLLATION */

#define _RS_AVAL_VA_(cd, at, av) \
        (SU_BFLAG_TEST((av)->ra_flags, RA_NULL) ?\
         VA_NULL :\
         (SU_BFLAG_TEST((av)->ra_flags, RA_ONLYCONVERTED) ?\
          rs_aval_deconvert(cd, at, av) : (av)->ra_va))

#define _RS_AVAL_ISBLOB_(cd, at, av)      SU_BFLAG_TEST((av)->ra_flags, RA_BLOB)
#define _RS_AVAL_ISNULL_(cd, atype, aval) SU_BFLAG_TEST((aval)->ra_flags, RA_NULL)

#ifndef SS_DEBUG
#define rs_aval_va      _RS_AVAL_VA_
#define rs_aval_isblob  _RS_AVAL_ISBLOB_
#define rs_aval_isnull  _RS_AVAL_ISNULL_
#endif /* SS_DEBUG */

#if defined(RS0AVAL_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		rs_aval_createbuf
 *
 * Creates a new attribute value object of specified type having an
 * undefined value to a user given buffer.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *      avalbuf - out, give
 *          aval buffer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void rs_aval_createbuf(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  avalbuf)
{
#ifdef SS_DEBUG
        switch (rs_atype_datatype(cd, atype)) {

            case RSDT_CHAR:
            case RSDT_DATE:
            case RSDT_INTEGER:
            case RSDT_FLOAT:
            case RSDT_DOUBLE:
            case RSDT_DFLOAT:
            case RSDT_BINARY:
            case RSDT_UNICODE:
            case RSDT_BIGINT:
                break;
            default:
                ss_rc_error(rs_atype_datatype(cd, atype));
        }
        avalbuf->ra_check = RSCHK_ATTRVALUE;
        avalbuf->ra_check_end = RSCHK_ATTRVALUE_END;
#endif
        avalbuf->ra_flags = RA_NULL;
        avalbuf->RA_RDVA = refdva_init();
        ss_purify(avalbuf->ra_.d = 0.0);
        avalbuf->ra_accinfo = NULL;
}

/*##**********************************************************************\
 *
 *		rs_aval_freebuf
 *
 * Releases an attribute value object data from user given buffer.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	avalbuf - in, use
 *		pointer into the attribute value object buffer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void rs_aval_freebuf(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  avalbuf)
{
        CHECK_AVAL(avalbuf);
        RS_AVAL_UNLINKBLOBIF((rs_sysi_t*)cd, atype, avalbuf);
        if (!SU_BFLAG_TEST(avalbuf->ra_flags, 
                           RA_VTPLREF | RA_NULL |
                           RA_ONLYCONVERTED | RA_FLATVA)) {
            refdva_done(&(avalbuf->RA_RDVA));
        } else {
            ss_debug(
                if (SU_BFLAG_TEST(avalbuf->ra_flags, 
                                  RA_NULL|RA_ONLYCONVERTED)) {
                    ss_assert(avalbuf->RA_RDVA == NULL);
                });
            SU_BFLAG_CLEAR(avalbuf->ra_flags, RA_VTPLREF);
        }
        if (avalbuf->ra_accinfo != NULL) {
            rs_aval_accinfo_free((avalbuf->ra_accinfo));
        }
}


/*##**********************************************************************\
 *
 *		rs_aval_setnull
 *
 * Member of the SQL function block.
 * Sets a NULL value to attribute value
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in out, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void rs_aval_setnull(
    rs_sysi_t*  cd,
    rs_atype_t* atype __attribute__ ((unused)),
    rs_aval_t*  aval)
{
        ss_dprintf_1(("%s: rs_aval_setnull\n", __FILE__));
        CHECK_AVAL(aval);

        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        if (!SU_BFLAG_TEST(aval->ra_flags,
                           RA_VTPLREF | RA_NULL |
                           RA_FLATVA | RA_ONLYCONVERTED))
        {
            refdva_done(&(aval->ra_va));
        } else {
            aval->ra_va = refdva_init();
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_CONVERTED | RA_VTPLREF | RA_BLOB |
                       RA_FLATVA | RA_ONLYCONVERTED | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_NULL);
}

/*##**********************************************************************\
 *
 *		rs_aval_sql_isnull
 *
 * Member of the SQL function block.
 * Checks if an attribute value object contains NULL value.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 *      1 if the field value instance contains NULL value
 *      2 if the field value instance contains "unknown" value
 *      0 if the field value instance contains an ordinary value
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE int rs_aval_sql_isnull(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval)
{
        ss_dprintf_1(("%s: rs_aval_sql_isnull\n", __FILE__));
        SS_NOTUSED(cd);
        SS_NOTUSED(atype);
        CHECK_AVAL(aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        if (SU_BFLAG_TEST(aval->ra_flags, RA_UNKNOWN)) {
            return(2);
        } else {
            return _RS_AVAL_ISNULL_(cd, atype, aval);
        }
}

/*##**********************************************************************\
 *
 *		rs_aval_getint8
 *
 * Returns the data content from an RSDT_BIGINT type aval.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 *      Attribute value in ss_int8_t format
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE ss_int8_t rs_aval_getint8(
        void*       cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval
) {

        ss_dprintf_1(("%s: rs_aval_getint8\n", __FILE__));
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_BIGINT);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));

        if (!SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED)) {
            aval->ra_.i8 = va_getint8(aval->RA_RDVA);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED);
        }
        return(aval->ra_.i8);
}

/*##**********************************************************************\
 *
 *		rs_aval_getfloat
 *
 * Returns the data content from a RSDT_FLOAT type aval.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 *      Attribute value in C-float format
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE float rs_aval_getfloat(
        void*       cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval
) {
        ss_dprintf_1(("%s: rs_aval_getfloat\n", __FILE__));
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_FLOAT);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));

        if (!SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED)) {
            aval->ra_.f = va_getfloat(aval->RA_RDVA);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED);
        }
        return(aval->ra_.f);
}

/*##**********************************************************************\
 *
 *		rs_aval_getdouble
 *
 * Returns the data content from a RSDT_DOUBLE type aval.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 *      Attribute value in C-double format
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE double rs_aval_getdouble(
        void*       cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval
) {
        ss_dprintf_1(("%s: rs_aval_getdouble\n", __FILE__));
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_DOUBLE);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));

        if (!SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED)) {
            aval->ra_.d = va_getdouble(aval->RA_RDVA);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED);
        }
        return(aval->ra_.d);
}

/*##**********************************************************************\
 *
 *		rs_aval_getdfloat
 *
 * Returns the data content from a RSDT_DFLOAT type aval.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 *      Attribute value in dt_dfl_t format
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dt_dfl_t rs_aval_getdfloat(
        void*       cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval
) {
        ss_dprintf_1(("%s: rs_aval_getdfloat\n", __FILE__));
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_DFLOAT);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));

        if (!SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED)) {
            dt_dfl_setva(&aval->ra_.dfl, aval->ra_va);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED);
        }
        return(aval->ra_.dfl);
}

/*##**********************************************************************\
 *
 *		rs_aval_getdate
 *
 * Returns the data content from a RSDT_DATE type aval.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 *      Attribute value in dt_date_t format
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dt_date_t*  rs_aval_getdate(
        void*       cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval
) {
        ss_dprintf_1(("%s: rs_aval_getdate\n", __FILE__));
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_DATE);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));

        if (!SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED)) {
            aval->ra_.date = (dt_date_t*)VA_GETASCIIZ(aval->RA_RDVA);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED);
        }
        return(aval->ra_.date);
}

/*##**********************************************************************\
 * 
 *		rs_aval_setflatvarefflag
 * 
 * Sets flatva bit in aval.
 * 
 * Parameters : 
 * 
 *		cd - 
 *			
 *			
 *		destatype - 
 *			
 *			
 *		destaval - 
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
SS_INLINE void rs_aval_setflatvarefflag(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* destatype __attribute__ ((unused)),
        rs_aval_t*  destaval)
{
        ss_dprintf_1(("%s: rs_aval_setflatvarefflag\n", __FILE__));
        CHECK_AVAL(destaval);
        ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_AGGR));
        ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_BLOB));

        if (!SU_BFLAG_TEST(destaval->ra_flags, RA_NULL)) {
            SU_BFLAG_SET(destaval->ra_flags, RA_FLATVA|RA_EXTERNALFLATVA);
            SU_BFLAG_CLEAR(destaval->ra_flags, RA_VTPLREF);
        }
}

/*##**********************************************************************\
 *
 *		rs_aval_getdata
 *
 * Returns a pointer to data area. If the aval is not of type CHAR,
 * an error is made. If p_length is not NULL, the data area length
 * is returned in *p_length.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 *      p_length - out, use
 *		if not NULL, the data area length is returned
 *          in *p_length
 *
 * Return value - ref :
 *
 *      pointer to the data area
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void* rs_aval_getdata(
        void*       cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval,
        ulong*      p_length
) {
        void* data;
        va_index_t len;

        ss_dprintf_1(("%s: rs_aval_getdata\n", __FILE__));
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));
        ss_debug({
            rs_datatype_t dt = rs_atype_datatype(cd, atype);
            ss_rc_dassert(
                    dt == RSDT_CHAR
                ||  dt == RSDT_UNICODE
                ||  dt == RSDT_BINARY,
                dt);
        });
        data = va_getdata(aval->RA_RDVA, &len);

        if (p_length != NULL) {
            *p_length = (ulong)len - 1;
        }
        return(data);
}

#endif /* defined(RS0AVAL_C) || defined(SS_USE_INLINE) */

#endif /* RS0AVAL_H */
