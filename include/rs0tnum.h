/*************************************************************************\
**  source       * rs0tnum.h
**  directory    * res
**  description  * Tuple number structure, eight byte unsigned long integer.
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


#ifndef RS0TNUM_H
#define RS0TNUM_H

#include <ssc.h>
#include <ssint8.h>
#include <uti0va.h>
#include <rs0types.h>
#include <rs0sysi.h>

#define RS_TUPLENUM_SIZE        8
#define RS_TUPLENUM_ATYPESIZE   10 /* 1 byte length + 8 byte integer + '\0' */

/* Tuple number structure, eight byte unsigned long integer. Memory format
 * is processor independent format
 */
typedef struct {
        uchar tnum_data[RS_TUPLENUM_SIZE];
} rs_tuplenum_t;

void rs_tuplenum_init(
        rs_tuplenum_t* tnum);

void rs_tuplenum_ulonginit(
        rs_tuplenum_t* tnum,
        ulong msl,
        ulong lsl);

void rs_tuplenum_int8init(
        rs_tuplenum_t* tnum,
        ss_int8_t i8);

SS_INLINE void rs_tuplenum_inc(
        rs_tuplenum_t* tnum);

void rs_tuplenum_getva(
        rs_tuplenum_t* tnum,
        va_t* va);

rs_tuplenum_t rs_tuplenum_gettuplenumfromva(
        va_t* va);

int rs_tuplenum_cmp(
        rs_tuplenum_t* tnum1,
        rs_tuplenum_t* tnum2);

long rs_tuplenum_getlsl(
        rs_tuplenum_t* tnum);

long rs_tuplenum_getmsl(
        rs_tuplenum_t* tnum);

SS_INLINE ss_int8_t rs_tuplenum_getint8(
        rs_tuplenum_t* tnum);

bool rs_tuplenum_setintoaval(
        rs_tuplenum_t* tnum,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_tuplenum_print_binary(
        int level,
        char* format,
        rs_tuplenum_t* tnum);

typedef struct {
        ss_uint4_t tl_1;    /* most significant long */
        ss_uint4_t tl_2;    /* least significant long */
} tnum_long_t;

#if defined(RS0TNUM_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              rs_tuplenum_inc
 *
 *
 *
 * Parameters :
 *
 *      tnum -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void rs_tuplenum_inc(
        rs_tuplenum_t* tnum)
{
        tnum_long_t tl;

        tl.tl_2 = SS_UINT4_LOAD_MSB1ST(&tnum->tnum_data[4]);

        if (tl.tl_2 == SS_UINT4_MAX) {

            tl.tl_1 = SS_UINT4_LOAD_MSB1ST(&tnum->tnum_data[0]);
            tl.tl_1++;
            tl.tl_2 = 0;
            SS_UINT4_STORE_MSB1ST(&tnum->tnum_data[0], tl.tl_1);
            SS_UINT4_STORE_MSB1ST(&tnum->tnum_data[4], tl.tl_2);
        } else {

            tl.tl_2++;
            SS_UINT4_STORE_MSB1ST(&tnum->tnum_data[4],tl.tl_2);
        }
}

/*##**********************************************************************\
 *
 *      rs_tuplenum_getint8
 *
 * gets the tuplenum value as 8-byte integer
 *
 * Parameters:
 *      tnum - in, use
 *          tuplenum object
 *
 * Return value:
 *      its value as 8 byte integer
 *
 * Limitations:
 *
 * Globals used:
 */
SS_INLINE ss_int8_t rs_tuplenum_getint8(rs_tuplenum_t* tnum)
{
        ss_uint4_t ms_u4;
        ss_uint4_t ls_u4;
        ss_int8_t i8;

        ms_u4 = SS_UINT4_LOAD_MSB1ST(&tnum->tnum_data[0]);
        ls_u4 = SS_UINT4_LOAD_MSB1ST(&tnum->tnum_data[4]);
        SsInt8Set2Uint4s(&i8, ms_u4, ls_u4);
        return (i8);
}

#endif /* defined(RS0TNUM_C) || defined(SS_USE_INLINE) */

#endif /* RS0TNUM_H */

