/*************************************************************************\
**  source       * rs0tnum.c
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

#define RS0TNUM_C

#include <ssenv.h>
#include <ssc.h>
#include <ssstring.h>
#include <sssprint.h>

#include <uti0vcmp.h>

#include "rs0atype.h"
#include "rs0aval.h"
#include "rs0tnum.h"

/*##**********************************************************************\
 *
 *              rs_tuplenum_init
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
void rs_tuplenum_init(
        rs_tuplenum_t* tnum)
{
        memset(tnum, '\0', sizeof(rs_tuplenum_t));
}

/*##**********************************************************************\
 *
 *              rs_tuplenum_ulonginit
 *
 *
 *
 * Parameters :
 *
 *      tnum -
 *
 *
 *      msl -
 *              Most significant long.
 *
 *      lsl -
 *              Least significant long.
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tuplenum_ulonginit(
        rs_tuplenum_t* tnum,
        ulong msl,
        ulong lsl)
{

        SS_UINT4_STORE_MSB1ST(&tnum->tnum_data[0], msl);
        SS_UINT4_STORE_MSB1ST(&tnum->tnum_data[4], lsl);
}

/*##**********************************************************************\
 *
 *              rs_tuplenum_int8init
 *
 *
 *
 * Parameters :
 *
 *      tnum -
 *
 *
 *      i8 -
 *              8 byte integer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tuplenum_int8init(
        rs_tuplenum_t* tnum,
        ss_int8_t i8)
{
        SS_UINT4_STORE_MSB1ST(&tnum->tnum_data[0], SsInt8GetMostSignificantUint4(i8));
        SS_UINT4_STORE_MSB1ST(&tnum->tnum_data[4], SsInt8GetLeastSignificantUint4(i8));
}


/*##**********************************************************************\
 *
 *              rs_tuplenum_getva
 *
 *
 *
 * Parameters :
 *
 *      tnum -
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
void rs_tuplenum_getva(
        rs_tuplenum_t* tnum,
        va_t* va)
{
        int i;
        int len;
        char vadata[sizeof(rs_tuplenum_t) + 1];

        len = sizeof(rs_tuplenum_t);

        /* Skip all leading zero bytes.
         */
        for (i = 0; len > 0 && tnum->tnum_data[i] == '\0'; i++, len--)
            ;

        /* The first data byte in va contains length after all zero
         * bytes are skipped from the beginning of tuplenum.
         */
        vadata[0] = (char)len;
        memcpy(&vadata[1], &tnum->tnum_data[i], len);

        va_setdata(va, vadata, len + 1);
}

/*##**********************************************************************\
 *
 *              rs_tuplenum_gettuplenumfromva
 *
 * Returns tuple number from va.
 *
 * Parameters :
 *
 *              va -
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
rs_tuplenum_t rs_tuplenum_gettuplenumfromva(
        va_t* va)
{
        int i;
        int zerolen;
        va_index_t len;
        char* data;
        char* rawtnum;
        rs_tuplenum_t tnum;
        ss_debug(va_t tmpva;)

        data = va_getdata(va, &len);
        rawtnum = (char*)&tnum;

        len = *data++;
        zerolen = sizeof(rs_tuplenum_t) - len;

        ss_dassert(zerolen + len == sizeof(rs_tuplenum_t));

        for (i = 0; i < zerolen; i++) {
            *rawtnum++ = '\0';
        }
        for (i = 0; i < (int)len; i++) {
            *rawtnum++ = *data++;
        }

        ss_debug(rs_tuplenum_getva(&tnum, &tmpva));
        ss_dassert(va_compare(va, &tmpva) == 0);

        return(tnum);
}

/*##**********************************************************************\
 *
 *              rs_tuplenum_cmp
 *
 * Compares two tuple numbers.
 *
 * Parameters :
 *
 *      tnum1 -
 *
 *
 *      tnum2 -
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
int rs_tuplenum_cmp(
        rs_tuplenum_t* tnum1,
        rs_tuplenum_t* tnum2)
{
        va_t va1;
        va_t va2;

        rs_tuplenum_getva(tnum1, &va1);
        rs_tuplenum_getva(tnum2, &va2);

        return(va_compare(&va1, &va2));
}

/*##**********************************************************************\
 *
 *              rs_tuplenum_getlsl
 *
 * Returns the least significant long part from the tuplenum.
 *
 * Parameters :
 *
 *      tnum -
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
long rs_tuplenum_getlsl(rs_tuplenum_t* tnum)
{
        long lsl;

        lsl = SS_UINT4_LOAD_MSB1ST(&tnum->tnum_data[4]);

        return(lsl);
}

/*##**********************************************************************\
 *
 *              rs_tuplenum_getmsl
 *
 *
 * Returns the most significant long part from the tuplenum.
 *
 *
 * Parameters :
 *
 *      tnum -
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
long rs_tuplenum_getmsl(rs_tuplenum_t* tnum)
{
        long msl;

        msl = SS_UINT4_LOAD_MSB1ST(&tnum->tnum_data[0]);

        return(msl);
}

/*##**********************************************************************\
 *
 *              rs_tuplenum_setintoaval
 *
 * Sets tuple number into integer or binary aval.
 *
 * Parameters :
 *
 *      tnum -
 *
 *
 *      cd -
 *
 *
 *      atype -
 *
 *
 *      aval -
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
bool rs_tuplenum_setintoaval(
        rs_tuplenum_t* tnum,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool succp = TRUE;

        switch (rs_atype_sqldatatype(cd, atype)) {
            case RSSQLDT_BIGINT:
            {
                ss_int8_t i8;
                i8 = SsInt8InitFrom2Uint4s(rs_tuplenum_getmsl(tnum), rs_tuplenum_getlsl(tnum));
                succp = rs_aval_setint8_ext(
                            cd,
                            atype,
                            aval,
                            i8,
                            NULL);
                ss_dassert(succp);
                break;
            }
            case RSSQLDT_INTEGER:
            case RSSQLDT_SMALLINT:
            case RSSQLDT_TINYINT:
                succp = rs_aval_setlong_ext(
                            cd,
                            atype,
                            aval,
                            rs_tuplenum_getlsl(tnum),
                            NULL);
                ss_dassert(succp);
                break;
            case RSSQLDT_BINARY:
            case RSSQLDT_VARBINARY:
            case RSSQLDT_LONGVARBINARY:
                succp = rs_aval_setbdata_ext(
                            cd,
                            atype,
                            aval,
                            (char*)tnum,
                            sizeof(rs_tuplenum_t),
                            NULL);
                ss_dassert(succp);
                break;
            default:
                succp = FALSE;
        }
        return(succp);
}

void rs_tuplenum_print_binary(
        int level,
        char* format,
        rs_tuplenum_t* tnum)
{
        size_t i;
        size_t len;
        uchar* dstr;
        uchar buf[80];
        uchar* p_dest;

        len = sizeof(rs_tuplenum_t);
        dstr = (uchar*)tnum;

        for (i = 0, p_dest = buf; i < len; i++, p_dest += 2, dstr++) {
            SsSprintf((char*)p_dest, "%02x", (int)*dstr & 0xff);
        }

        SsDbgPrintfFunN(level, format, buf);
}

