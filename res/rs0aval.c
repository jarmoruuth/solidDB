/*************************************************************************\
**  source       * rs0aval.c
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------
This file implements attribute value functions.
Functions that belong to the SQL-function block are presented first.
The rest are for internal use of SOLID database engine and table level.

The basis of every aval is a dynva (dynamic v-attribute).
Every aval always contains a dynva. When the physical value of an aval
is requested in some RSDT_ format, the dynva is converted. The converted
value is also stored in the aval structure so that it will be available when
asked for the next time (lazy evaluation).


Currently supported data types are

        Type            Implemented as

        RSDT_CHAR           char *
        RSDT_INTEGER        long
        RSDT_FLOAT          float
        RSDT_DOUBLE         double
        RSDT_DFLOAT         dt_dfl_t
        RSDT_DATE           dt_date_t

21.1.2002 Added 64-bit integer. tommiv.
        RSDT_BIGINT         ss_int8_t


Limitations:
-----------

Error handling:
--------------
In few functions an rs_err_t error object is created.
Otherwise returns only an error flag. If not possible, ss_dassert.

Objects used:
------------
v-attributes         <uti0va.h>
dfloats              <dt0dfl.h>
dates                <dt0date.h>
stringlike operator  <su0slike.h>
relational operators "rs0cons.h"
attribute types      "rs0atype.h"

8-byte integer       <ssint8.h>

Preconditions:
-------------
None

Multithread considerations:
--------------------------
Code is fully re-entrant.
The same aval object can not be used simultaneously from many threads.

Example:
-------
taval.c

**************************************************************************
#endif /* DOCUMENTATION */

#define RS0AVAL_C

#include <ssmath.h>

#include <ssc.h>
#include <ssstring.h>

#include <ssdebug.h>
#include <ssmem.h>
#include <ssdtoa.h>
#include <ssltoa.h>
#include <ssfloat.h>
#include <sstraph.h>
#include <ssscan.h>
#include <sschcvt.h>
#include <ssutf.h>
#include <sssprint.h>

#include <ssint8.h>

#include <uti0va.h>
#include <uti0vcmp.h>
#include <uti0dyn.h>

#include <su0slike.h>
#include <su0bstre.h>
#include <su0icvt.h>

#include <dt0dfloa.h>
#include <dt0date.h>

#include "rs0types.h"
#include "rs0error.h"
#include "rs0atype.h"
#include "rs0aval.h"
#include "rs0cons.h"
#include "rs0sdefs.h"
#include "rs0sysi.h"

#include <sswctype.h>
#include <su0wlike.h>

char* (*rs_aval_print_externaldatatype)(rs_sysi_t* cd, rs_atype_t* atype, rs_aval_t* aval);

/*##**********************************************************************\
 *
 *		rs_aval_removevtplref
 *
 * Removes vtpl reference from aval (i.e makes a separate copy of data)
 *
 * Parameters :
 *
 *	cd - not used
 *
 *
 *	atype - not used
 *
 *
 *	aval - in out
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
void rs_aval_removevtplref(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval)
{
        size_t glen;

        ss_dassert(SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_FLATVA | RA_ONLYCONVERTED));
        glen = va_grosslen(aval->ra_va);
        if (glen <= sizeof(aval->ra_vabuf)) {
            SU_BFLAG_SET(aval->ra_flags, RA_FLATVA);
            memcpy(&aval->ra_vabuf, aval->ra_va, glen);
            aval->ra_va = &aval->ra_vabuf.va;
        } else {
            refdva_t rdva = refdva_init();
            refdva_setva(&rdva, aval->ra_va);
            aval->ra_va = rdva;
        }
        SU_BFLAG_CLEAR(aval->ra_flags, RA_VTPLREF);
}

#ifdef SS_DEBUG

#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		rs_aval_create
 *
 * Creates a new attribute value object of specified type having an
 * undefined value (NULL).
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 * Return value - give :
 *
 *      Pointer into the newly allocated attribute value object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_aval_t* rs_aval_create(cd, atype)
    void*       cd __attribute__ ((unused));
    rs_atype_t* atype __attribute__ ((unused));
{
        rs_aval_t* aval;

        ss_dprintf_1(("%s: rs_aval_create\n", __FILE__));
        SS_MEMOBJ_INC(SS_MEMOBJ_AVAL, rs_aval_t);

        aval = SSMEM_NEW(rs_aval_t);

        RS_AVAL_CREATEBUF(cd, atype, aval);

        return(aval);
}

/*##**********************************************************************\
 *
 *		rs_aval_sql_create
 *
 * Member of the SQL function block.
 * Creates a new attribute value object of specified type having an
 * undefined value (NULL).
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 * Return value - give :
 *
 *      Pointer into the newly allocated attribute value object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_aval_t* rs_aval_sql_create(cd, atype)
    void*       cd __attribute__ ((unused));
    rs_atype_t* atype __attribute__ ((unused));
{
        rs_aval_t* aval;

        ss_dprintf_1(("%s: rs_aval_sql_create\n", __FILE__));
        SS_MEMOBJ_INC(SS_MEMOBJ_AVAL, rs_aval_t);

        aval = SSMEM_NEW(rs_aval_t);

        RS_AVAL_CREATEBUF(cd, atype, aval);
        SU_BFLAG_SET(aval->ra_flags, RA_UNKNOWN);

        return(aval);
}

/*##**********************************************************************\
 *
 *		rs_aval_createconst
 *
 * Member of the SQL function block.
 * Creates a new attribute value object of specified type
 * corresponding to an SQL constant.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	sqlvalstr - in, use
 *		the SQL constant string. If NULL, the
 *          attribute value will contain NULL value
 *
 *	p_errh - out, give
 *		in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value - give :
 *
 *      Pointer into the newly allocated attribute value object.
 *      NULL is returned in case of error.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_aval_t* rs_aval_createconst(
        void*       cd,
        rs_atype_t* atype,
        char*       sqlvalstr,
        rs_err_t**  p_errh)
{
        rs_atype_t*     orig_const_atype;
        rs_datatype_t   datatype;
        rs_datatype_t   orig_const_datatype;
        rs_aval_t*      aval;
        ss_char2_t      buf[40];
        bool need_conversion = FALSE;

        ss_dprintf_1(("%s: rs_aval_createconst\n", __FILE__));

        aval = rs_aval_create(cd, atype);
        rs_aval_setliteralflag(cd, atype, aval, TRUE);
        SU_BFLAG_CLEAR(aval->ra_flags, RA_UNKNOWN);
        if (sqlvalstr == NULL || strcmp(sqlvalstr, "NULL") == 0) {
            return(aval);
        }
        orig_const_atype = rs_atype_createconst(cd, sqlvalstr, p_errh);
        if (orig_const_atype == NULL) {
            rs_aval_free(cd, atype, aval);
            return (NULL);
        }
        orig_const_datatype = RS_ATYPE_DATATYPE(cd, orig_const_atype);
        datatype = RS_ATYPE_DATATYPE(cd, atype);
        if (orig_const_datatype != datatype) {
            need_conversion = TRUE;
        } else if (datatype == RSDT_DATE) {
            rs_sqldatatype_t sqldt1 =
                RS_ATYPE_SQLDATATYPE(cd, atype);
            rs_sqldatatype_t sqldt2 =
                RS_ATYPE_SQLDATATYPE(cd, orig_const_atype);;
            if (sqldt1 != sqldt2) {
                need_conversion = TRUE;
            }
        }
        if (need_conversion) {
            RS_AVALRET_T rc;

            rs_aval_t* orig_const_aval =
                rs_aval_createconst(cd,
                                    orig_const_atype,
                                    sqlvalstr,
                                    p_errh);
            if (orig_const_aval == NULL) {
                rs_aval_free(cd, atype, aval);
                rs_atype_free(cd, orig_const_atype);
                return (NULL);
            }
            rc = rs_aval_convert_ext(cd,
                                     atype, aval,
                                     orig_const_atype, orig_const_aval,
                                     p_errh);
            rs_aval_free(cd, orig_const_atype, orig_const_aval);
            rs_atype_free(cd, orig_const_atype);
            if (rc == RSAVR_FAILURE) {
                rs_aval_free(cd, atype, aval);
                return (NULL);
            }
            return (aval);
        } else {
            rs_atype_free(cd, orig_const_atype);
        }
        switch (datatype) {
            case RSDT_DATE:
                ss_dprintf_2((
                "%s: aval_createc, atype RSDT_CHAR (sqldt = %d), str=%s\n",
                            __FILE__,
                           rs_atype_sqldatatype(cd, atype),
                           sqlvalstr));
                if (*sqlvalstr == 'D') {
                    ss_dassert(rs_atype_sqldatatype(cd, atype)
                               == RSSQLDT_DATE);
                    ss_dassert(memcmp(sqlvalstr, "DATE", 4) == 0);
                    sqlvalstr += 4; /* skip "DATE" */
                } else if (*sqlvalstr == 'T') {
                    ++sqlvalstr; /* skip "T" */
                    if (memcmp(sqlvalstr, "IMESTAMP", 8) == 0) {
                        ss_dassert(rs_atype_sqldatatype(cd, atype)
                                   == RSSQLDT_TIMESTAMP);
                        sqlvalstr += 8; /* skip "IMESTAMP" */
                    } else {
                        ss_dassert(rs_atype_sqldatatype(cd, atype)
                                   == RSSQLDT_TIME);
                        ss_dassert(memcmp(sqlvalstr, "IME", 3) == 0);
                        sqlvalstr += 3; /* skip "IME" */
                    }
                }
                break;
            case RSDT_BINARY:
                ss_dassert(*sqlvalstr == 'X'); /* 'B' not supported, yet! */
                if (*sqlvalstr == 'X') {
                    sqlvalstr++;
                }
                break;
            case RSDT_CHAR:
                ss_dprintf_2(("%s: aval_createc, atype RSDT_CHAR, str=%s\n",
                            __FILE__, sqlvalstr));
                break;
            case RSDT_UNICODE:
                ss_debug(
                    if (datatype == RSDT_UNICODE) {
                        ss_dprintf_2((
                            "%s: aval_createc, atype RSDT_UNICODE, str=%s\n",
                            __FILE__, sqlvalstr));
                    });
                if (*sqlvalstr == 'N') {
                    sqlvalstr++;
                }
                break;
            case RSDT_INTEGER:
            case RSDT_BIGINT:
            case RSDT_FLOAT:
            case RSDT_DOUBLE:
            case RSDT_DFLOAT:
                if (!rs_aval_set8bitstr_ext(cd, atype, aval, sqlvalstr, NULL)) {
                    rs_aval_free(cd, atype, aval);
                    rs_error_create(
                            p_errh,
                            datatype == RSDT_INTEGER ? E_ILLINTCONST_S :
                            datatype == RSDT_BIGINT ? E_ILLINTCONST_S :
                            datatype == RSDT_FLOAT ? E_ILLREALCONST_S :
                            datatype == RSDT_DOUBLE ? E_ILLDBLCONST_S :
                            datatype == RSDT_DATE ? E_ILLDATECONST_S :
                            E_ILLDECCONST_S,
                            sqlvalstr);
                    return(NULL);
                } else {
                    return(aval);
                }

            default:
                ss_rc_error(datatype);
        }
        /* string with possible type prefix, skip possible
           whitespace
        */
        for (;;sqlvalstr++) {
            switch (*sqlvalstr) {
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    continue;
                default:
                    break;
            }
            break;
        }
        if (*sqlvalstr != RS_STRQUOTECH
            ||  sqlvalstr[strlen(sqlvalstr) - 1] != RS_STRQUOTECH)
        {
            rs_error_create(
                    p_errh,
                    datatype == RSDT_DATE? E_ILLDATECONST_S : E_ILLCHARCONST_S,
                    sqlvalstr);
            rs_aval_free(cd, atype, aval);
            return(NULL);
        }

        {
            /* After the first loop below contains the length */
            uint len = 0;
            uint i;

            for (i = 1; sqlvalstr[i + 1] != '\0'; i++) {
                len++;
                if (sqlvalstr[i] == RS_STRQUOTECH
                &&  sqlvalstr[i + 1] == RS_STRQUOTECH
                &&  sqlvalstr[i + 2])
                {
                    i++;
                }
            }

            {
                /* Here we create a temporary compressed string
                   that does not contain any extra quote characters.
                   Then we set it into aval.
                */
                char*   tmp_str = (char*)&buf[0];

                uint    l = 0;

                if (len > sizeof(buf) - 1) {
                    tmp_str = SsMemAlloc(len+1);
                }
                for (i = 1; sqlvalstr[i + 1] != '\0'; i++) {
                    tmp_str[l] = sqlvalstr[i];
                    l++;
                    if (sqlvalstr[i] == RS_STRQUOTECH
                    &&  sqlvalstr[i + 1] == RS_STRQUOTECH
                    &&  sqlvalstr[i + 2])
                    {
                        i++;
                    }
                }
                ss_dassert(l <= len);
                {
                    RS_AVALRET_T succp;

                    succp = rs_aval_setUTF8data_ext(
                            cd,
                            atype, aval,
                            tmp_str, l,
                            p_errh);
                    if (succp == RSAVR_FAILURE) {
                        rs_aval_free(cd, atype, aval);
                        aval = NULL;
                    }
                }
                if (tmp_str != (char*)&buf[0]) {
                    SsMemFree(tmp_str);
                }
            }
        }
        return(aval);
}

/*#***********************************************************************\
 *
 *		aval_print_char
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_char(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
        size_t i;
        size_t j;
        ss_char1_t* data;
        va_index_t datasize;
        size_t quotecount = 0;
        size_t totalsize;
        size_t required_bufsize_total;
        ss_char1_t* buf;
        size_t bufsize_utf8;

        data = va_getdata(aval->RA_RDVA, &datasize);
        ss_dassert(datasize >= 1);
        if (sqlquotes) {
            for (quotecount = 0, i = 0; i < datasize; i++) {
                if (data[i] == '\'') {
                    quotecount++;
                }
            }
        }

        bufsize_utf8 = rs_aval_requiredUTF8bufsize(cd, atype, aval);
        required_bufsize_total = bufsize_utf8 + quotecount + 1 + 2;
        buf = SsMemAlloc(required_bufsize_total);

        rs_aval_converttoUTF8(
            cd, atype, aval,
            sqlquotes ? buf + 1 : buf, required_bufsize_total - 3, 0, &totalsize,
            NULL);

        if (sqlquotes) {
            buf[0] = buf[1 + totalsize + quotecount] = '\'';
            buf[1 + totalsize + quotecount + 1] = '\0';
        }

        if (quotecount != 0) {
            for (i = totalsize, j = totalsize + quotecount; i > 0; ) {
                i--;
                j--;
                if (((buf + 1)[j] = (buf + 1)[i]) == '\'') {
                    (buf + 1)[--j] = '\'';
                }
            }
            ss_dassert(i == 0 && j == 0);
        }

        return (buf);
}

/*#***********************************************************************\
 *
 *		aval_convert_to_ws
 *
 * Convert the aval string into wide string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  dst_size    -   the size of the converted string
 *
 * Return value :   ss_uint4_t*
 *
 *  the converted string
 *  Should be freed using SsMemFree.
 *
 * Comments :
 *
 *  uses MySQL MY_CHARSET_HANDLER routines
 *
 * Globals used :
 *
 * See also :
 */
static ss_uint4_t* aval_convert_to_ws(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        size_t*     dst_size)
{
    ss_char1_t*     bytes;
    ss_char1_t*     bytes_end;
    ss_uint4_t*     dbuf;
    ss_uint4_t*     dbuf_work;
    va_index_t      datasize;
    size_t          chars_num;
    int             char_size;
#ifdef SS_COLLATION
    su_collation_t* coll;
#endif /* SS_COLLATION */

    ss_dassert(dst_size);

    bytes = va_getdata(aval->RA_RDVA, &datasize);
    ss_dassert(datasize >= 1);
    bytes_end = bytes + datasize;

#ifdef SS_COLLATION
    coll = rs_atype_collation(cd, atype);
    if (coll) {
        chars_num = su_collation_numcells(coll, bytes, bytes_end);

        *dst_size = chars_num * sizeof(ss_uint4_t) + 1;
        dbuf = (ss_uint4_t*)SsMemAlloc(*dst_size);
        memset(dbuf, 0, *dst_size);

        dbuf_work = dbuf;

        while (bytes < bytes_end) {
            char_size = su_collation_mb_wc(coll, dbuf_work, bytes, bytes_end);
            if (char_size == -101) {        /* MY_CS_TOOSMALL */
                break;
            } if (char_size == -102) {      /* MY_CS_TOOSMALL2 */
                *dbuf_work = 0x0000003F;    /* '?' */
                break;
            } else if (char_size < 0) {
                *dbuf_work = 0x0000003F;    /* '?' */
                bytes += 1;
            } else {
                bytes += char_size;
            }

            ++dbuf_work;
        }
    }
    else
#endif /* SS_COLLATION */
    {
        /* consider we have ASCII string */
        *dst_size = datasize * sizeof(ss_uint4_t) + 1;
        dbuf = (ss_uint4_t*)SsMemAlloc(*dst_size);
        memset(dbuf, 0, *dst_size);

        dbuf_work = dbuf;

        while (bytes < bytes_end) {
            *dbuf_work = ((ss_uint4_t)*bytes) & 0x000000FF;

            ++bytes;
            ++dbuf_work;
        }
    }

    ss_dassert(*dst_size & 1);

    return dbuf;
}

/*#***********************************************************************\
 *
 *		convert_ws_to_ucs2
 *
 * Convert the wide string into UCS2 string.
 *
 * Parameters :
 *
 *  src         -   source string
 *  src_size    -   source string size in bytes
 *  dst_size    -   dest string size in bytes
 *
 * Return value :   ss_char2_t*
 *
 *  the converted string 
 *  Should be freed using SsMemFree.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static ss_char2_t* convert_ws_to_ucs2(
        ss_uint4_t* src,
        size_t      src_size,
        size_t*     dst_size)
{
    ss_char2_t* dbuf;
    ss_char2_t* dbuf_work;
    ss_char2_t  word;
    ss_uint4_t* src_end = src + src_size / sizeof(ss_uint4_t);

    ss_dassert(src_size >= 4);

    *dst_size = src_size / 2 + 1;
    dbuf = (ss_char2_t*)SsMemAlloc(*dst_size);
    memset(dbuf, 0, *dst_size);

    dbuf_work = dbuf;

    while (src < src_end) {
        /* we have to shrink 4 bytes unicode to 2 bytes UCS2 and
         * revert endianness.
         */
        if (*src >> 16) {
        /* this is UCS4 character. we could not recoginze it now.
         */
            word = 0x003F; /* '?' */
        } else {
            word = (ss_char2_t)*src;
        }

        *dbuf_work = ((ss_char2_t)(word >> 8) & 0x00FF) | ((ss_char2_t)(word << 8) & 0xFF00);

        ++dbuf_work;
        ++src;
    }

    ss_dassert(*dst_size & 1);

    return dbuf;
}

/*#***********************************************************************\
 *
 *		aval_print_unicode
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_unicode(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval,
        bool        sqlquotes)
{
        va_index_t dlen;
        ss_char2_t* dstr;
        ss_char2_t* p_src_tmp;
        ss_byte_t* buf;
        size_t blen;
        size_t blen2;
        ss_byte_t* p_dst_tmp;
        size_t i;
        size_t j;

        dstr = va_getdata(aval->RA_RDVA, &dlen);
        if (rs_aval_isblob(cd, atype, aval)) {
            ss_rc_dassert(dlen >= RS_VABLOBREF_SIZE, dlen);
            dlen -= RS_VABLOBREF_SIZE;
        } else {
            ss_dassert(dlen & 1);
        }

        dlen /= sizeof(ss_char2_t);
        blen = SsUCS2vaByteLenAsUTF8(dstr, dlen);
        buf = SsMemAlloc(blen + 3);
        p_dst_tmp = buf;
        p_src_tmp = dstr;
        SsUCS2vatoUTF8(
            &p_dst_tmp, p_dst_tmp + blen,
            &p_src_tmp, p_src_tmp + dlen);

        if (!sqlquotes) {
            buf[blen] = '\0';
            return ((char*)buf);
        }

        for (blen2 = blen, i = 0; i < blen; i++) {
            if (buf[i] == RS_STRQUOTECH) {
                blen2++;
            }
        }
        if (blen == blen2) {
            memmove(buf + 1, buf, blen);
            buf[0] = buf[blen + 1] = RS_STRQUOTECH;
            buf[blen + 2] = '\0';
            return ((char*)buf);
        } else {
            ss_byte_t* buf2;
            ss_dassert(blen2 > blen);
            buf2 = SsMemAlloc(blen2 + 3);
            buf2[0] = buf2[blen2 + 1] = RS_STRQUOTECH;
            buf2[blen2 + 2] = '\0';
            for (j = 1, i = 0; i < blen; i++, j++) {
                if ((buf2[j] = buf[i]) == RS_STRQUOTECH) {
                    buf2[++j] = RS_STRQUOTECH;
                }
            }
            ss_dassert(buf2[0] == buf2[blen2 + 1]
                    && buf2[0] == RS_STRQUOTECH);
            SsMemFree(buf);
            return ((char*)buf2);
        }
}

/*#***********************************************************************\
 *
 *		aval_print_binary
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_binary(
    void*       cd __attribute__ ((unused)),
    rs_atype_t* atype __attribute__ ((unused)),
    rs_aval_t*  aval)
{
        size_t i;
        size_t len;
        uchar* dstr;
        uchar* buf;
        uchar* p_dest;

        if ((ulong)VA_NETLEN(aval->RA_RDVA) * 2 > (ulong)SS_MAXALLOCSIZE / (ulong)2 - (ulong)16) {
            len = SS_MAXALLOCSIZE / 2 - 16;
        } else {
            len = VA_NETLEN(aval->RA_RDVA);
        }
        dstr = (uchar*)VA_GETASCIIZ(aval->RA_RDVA);
        buf = SsMemAlloc(2 * len + 1);

        for (i = 0, p_dest = buf; i < len; i++, p_dest += 2, dstr++) {
            SsSprintf((char*)p_dest, "%02x", (int)*dstr & 0xff);
        }

        return ((char*)buf);
}

/*#***********************************************************************\
 *
 *		aval_print_char_latin1
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_char_latin1(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
    ss_dprintf_1(("aval_print_char_latin1\n"));

    return aval_print_char(cd, atype, aval, sqlquotes);
}

/*#***********************************************************************\
 *
 *		aval_print_char_latin2
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_char_latin2(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
    ss_dprintf_1(("aval_print_char_latin2\n"));

    return aval_print_char(cd, atype, aval, sqlquotes);
}

/*#***********************************************************************\
 *
 *		aval_print_char_utf8
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_char_utf8(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
        size_t i;
        size_t j;
        ss_char1_t* data;
        va_index_t datasize;
        size_t quotecount = 0;
        size_t required_bufsize_total;
        ss_char1_t* buf;

        ss_dprintf_1(("aval_print_char_utf8\n"));

        data = va_getdata(aval->RA_RDVA, &datasize);
        ss_dassert(datasize >= 1);
        if (sqlquotes) {
            for (quotecount = 0, i = 0; i < datasize; i++) {
                if (data[i] == '\'') {
                    quotecount++;
                }
            }
        }

        required_bufsize_total = datasize + quotecount + 1 + 2;
        buf = SsMemAlloc(required_bufsize_total);

        memset(buf, 0, required_bufsize_total);
        memcpy(sqlquotes ? buf + 1 : buf, data, datasize);

        if (sqlquotes) {
            buf[0] = buf[1 + datasize + quotecount] = '\'';
            buf[1 + datasize + quotecount + 1] = '\0';
        }

        if (quotecount != 0) {
            for (i = datasize, j = datasize + quotecount; i > 0; ) {
                i--;
                j--;
                if (((buf + 1)[j] = (buf + 1)[i]) == '\'') {
                    (buf + 1)[--j] = '\'';
                }
            }
            ss_dassert(i == 0 && j == 0);
        }

        return buf;
}

/*#***********************************************************************\
 *
 *		aval_print_char_ucs2
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_char_ucs2(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
    ss_dprintf_1(("aval_print_char_ucs2\n"));

    return aval_print_unicode(cd, atype, aval, sqlquotes);
}

/*#***********************************************************************\
 *
 *		aval_print_char_bin
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_char_bin(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
    ss_dprintf_1(("aval_print_char_bin\n"));

    return aval_print_binary(cd, atype, aval);
}

/*#***********************************************************************\
 *
 *		aval_print_char_other
 *
 * Prints character aval into a string.
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 *  prints the string encoded using any supported charset
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_char_other(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
    char*       buf_ret = NULL;
    ss_uint4_t* buf;
    size_t      buf_size;
    ss_char2_t* buf2;
    size_t      buf2_size;
    rs_aval_t*  temp_aval;

    ss_dprintf_1(("aval_print_char_other\n"));

    buf = aval_convert_to_ws(cd, atype, aval, &buf_size);
    buf2 = convert_ws_to_ucs2(buf, buf_size, &buf2_size);

    temp_aval = rs_aval_create(cd, atype);
    rs_aval_setdata_raw(cd, atype, temp_aval, buf2, buf2_size);

    buf_ret = aval_print_unicode(cd, atype, temp_aval, sqlquotes);

    rs_aval_free(cd, atype, temp_aval);
    SsMemFree(buf2);
    SsMemFree(buf);

    return buf_ret;
}

/*#***********************************************************************\
 *
 *		aval_print_char_ex
 *
 * Prints character aval into a string. Selects the appropriate routine 
 * depending on the atype's charset
 *
 * Parameters :
 *
 *	cd          -   client data
 *	atype       -   atype
 *	aval        -   aval
 *  sqlquotes   -   do we need to add quotes
 *
 * Return value :   char*
 *
 *  the result string 
 *
 * Comments :
 *
 *  prints the string encoded using any supported charset
 *
 * Globals used :
 *
 * See also :
 */
static char* aval_print_char_ex(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
#ifdef SS_COLLATION
        su_charset_t    cs   = SUC_DEFAULT;
        su_collation_t* coll = atype->at_collation;

        if (coll) {
            cs = su_collation_get_charset(coll);
        }

        switch (cs) {
            case SUC_LATIN1:
                return aval_print_char_latin1(cd, atype, aval, sqlquotes);
            case SUC_LATIN2:
                return aval_print_char_latin2(cd, atype, aval, sqlquotes);
            case SUC_UTF8:
                return aval_print_char_utf8(cd, atype, aval, sqlquotes);
            case SUC_UCS2:
                return aval_print_char_ucs2(cd, atype, aval, sqlquotes);
            case SUC_BIN:
                return aval_print_char_bin(cd, atype, aval, sqlquotes);
            case SUC_CP932:
            case SUC_EUCJPMS:
            case SUC_BIG5:
            case SUC_EUCKR:
            case SUC_GB2312:
            case SUC_GBK:
            case SUC_SJIS:
            case SUC_TIS620:
            case SUC_UJIS:
            case SUC_CP1250:
                return aval_print_char_other(cd, atype, aval, sqlquotes);
            default:
                return aval_print_char(cd, atype, aval, sqlquotes);
        }
#else
        /* consider we have ASCII text*/
        return aval_print_char(cd, atype, aval, sqlquotes);
#endif /* SS_COLLATION */
}

/*##**********************************************************************\
 *
 *		rs_aval_print_ex
 *
 * Generates a string representation of an attribute value instance
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
 *	sqlquotes - in
 *
 *
 * Return value - give :
 *
 *      Pointer into a newly allocated string containing the
 *      string representation.
 *      Should be freed using SsMemFree.
 *
 * Limitations  :
 *
 * Globals used :
 */
char* rs_aval_print_ex(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes)
{
        uint datatype;
        char* str;

        ss_dprintf_1(("%s: rs_aval_print_ex\n", __FILE__));
        CHECK_AVAL(aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
#ifdef SS_MME
        if (SU_BFLAG_TEST(aval->ra_flags, RA_MIN)) {
            return SsMemStrdup((char *)"*MINIMUM*");
        } else if (SU_BFLAG_TEST(aval->ra_flags, RA_MAX)) {
            return SsMemStrdup((char *)"*MAXIMUM*");
        } else
#endif
        if (SU_BFLAG_TEST(aval->ra_flags, RA_UNKNOWN)) {
            return(SsMemStrdup((char *)"UNKNOWN"));
        }

        if (RS_AVAL_ISNULL(cd, atype, aval)) {
            return(SsMemStrdup((char *)"NULL"));
        }

        if (rs_aval_print_externaldatatype != NULL) {
            str = (*rs_aval_print_externaldatatype)(cd, atype, aval);
            if (str != NULL) {
                return(str);
            }
        }

        datatype = RS_ATYPE_DATATYPE(cd, atype);

        switch (datatype) {

            case RSDT_CHAR:
            case RSDT_BINARY:
                switch (RS_ATYPE_SQLDATATYPE(cd, atype)) {
                    case RSSQLDT_LONGVARBINARY:
                    case RSSQLDT_VARBINARY:
                    case RSSQLDT_BINARY:
                        return(aval_print_binary(cd, atype, aval));
                    default:
                        return(aval_print_char_ex(cd, atype, aval, sqlquotes));
                }
            case RSDT_UNICODE:
                return (aval_print_unicode(cd, atype, aval, sqlquotes));
            case RSDT_INTEGER:
            case RSDT_BIGINT:
            case RSDT_FLOAT:
            case RSDT_DOUBLE:
            case RSDT_DFLOAT:
            case RSDT_DATE:
                {
                    char buf[80];
                    size_t totalsize;
                    bool succp;
                    succp = rs_aval_convertto8bitstr(
                        cd, atype, aval, buf, sizeof(buf), 0, &totalsize, NULL);
                    if (!succp) {
                        strcpy(buf, "### Error ###");
                    }
                    return(SsMemStrdup(buf));
                }
            default:
                ss_rc_error(datatype);

        }
        ss_derror;
        return(NULL);
}

/*##**********************************************************************\
 *
 *		rs_aval_print
 *
 * Member of the SQL function block.
 * Generates a string representation of an attribute value instance
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
 * Return value - give :
 *
 *      Pointer into a newly allocated string containing the
 *      string representation.
 *      Should be freed using SsMemFree.
 *
 * Limitations  :
 *
 * Globals used :
 */
char* rs_aval_print(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval)
{
        ss_dprintf_1(("%s: rs_aval_print\n", __FILE__));
        CHECK_AVAL(aval);

        return(rs_aval_print_ex(cd, atype, aval, TRUE));
}

/*##**********************************************************************\
 *
 *		rs_aval_output
 *
 * Member of the SQL function block.
 * Generates a string representation of an attribute value instance
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
 *	sqlquotes - in
 *
 *
 *	outputfun - in
 *
 *
 *	outputpar - in
 *
 *
 * Return value:
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_aval_output(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        bool        sqlquotes,
        void        (*outputfun)(void*, void*),
        void*       outputpar)
{
        char* val;

        val = rs_aval_print_ex(cd, atype, aval, sqlquotes);
        (*outputfun)(outputpar, val);
        SsMemFree(val);
}


/*##**********************************************************************\
 *
 *		rs_aval_free
 *
 * Member of the SQL function block.
 * Releases an attribute value object.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	aval - in, take
 *		pointer into the attribute value object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_aval_free(cd, atype, aval)
    void*       cd;
    rs_atype_t* atype __attribute__ ((unused));
    rs_aval_t*  aval;
{
        ss_dprintf_1(("%s: rs_aval_free\n", __FILE__));
        CHECK_AVAL(aval);
        SS_MEMOBJ_DEC(SS_MEMOBJ_AVAL);

        RS_AVAL_FREEBUF(cd, atype, aval);
        SsMemFree(aval);
}


#ifdef SS_DEBUG

/*##**********************************************************************\
 *
 *		rs_aval_copybuf2
 *
 * Copies an attribute value to another buffer. Removes
 * possible vtplref from aval.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	res_aval - in out, use
 *		pointer into the result attribute value object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_aval_copybuf2(cd, atype, res_aval, aval)
    void*       cd;
    rs_atype_t* atype;
        rs_aval_t*  res_aval;
    rs_aval_t*  aval;
{
        ss_dprintf_2(("%s: rs_aval_copybuf2\n", __FILE__));
        CHECK_AVAL(aval);

        _RS_AVAL_COPYBUF2_(cd, atype, res_aval, aval);
        CHECK_AVAL(res_aval);
}

/*##**********************************************************************\
 *
 *		rs_aval_fix_rawcopy
 *
 * Same as rs_aval_copybuf2, but this assumes the aval buffer is already
 * copied, so copying is omitted here and this assumes there is not
 * a vtplref
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	atype -
 *
 *
 *	res_aval -
 *
 *
 *	aval -
 *
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
void rs_aval_fix_rawcopy(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  res_aval)
{
        ss_dprintf_2(("%s: rs_aval_fix_rawcopy\n", __FILE__));
        CHECK_AVAL(res_aval);
        _RS_AVAL_FIX_RAWCOPY_(cd,atype,res_aval);
        CHECK_AVAL(res_aval);
}

/*##**********************************************************************\
 *
 *		rs_aval_copybuf4
 *
 * Same as rs_aval_copybuf2, checks whether the old aval
 * buffer needs to be freed, ie. the old aval buffer may have
 * a previous value. This one also preserves vtplref
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	atype -
 *
 *
 *	res_aval -
 *
 *
 *	aval -
 *
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
void rs_aval_copybuf4(cd, atype, res_aval, aval)
    void*       cd;
    rs_atype_t* atype;
    rs_aval_t*  res_aval;
    rs_aval_t*  aval;
{
        ss_dprintf_2(("%s: rs_aval_copybuf4\n", __FILE__));
        CHECK_AVAL(aval);
        _RS_AVAL_COPYBUF4_(cd,atype,res_aval,aval);
        CHECK_AVAL(res_aval);
}

/*##**********************************************************************\
 *
 *		rs_aval_assignbuf
 *
 * Assigns an aval to another legal and initialized aval
 * of same type. The type is not checked here!
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	dst_aval - use
 *		destination aval
 *
 *	src_aval - in, use
 *		source aval
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_aval_assignbuf(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t*  dst_aval,
        rs_aval_t*  src_aval)
{
        ss_dprintf_2(("%s: rs_aval_assignbuf\n", __FILE__));
        CHECK_AVAL(dst_aval);
        CHECK_AVAL(src_aval);
        _RS_AVAL_ASSIGNBUF_(cd, atype, dst_aval, src_aval);
        CHECK_AVAL(dst_aval);
}

void rs_aval_linkblob(
        void* cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);
        ss_dprintf_2(("%s: rs_aval_linkblob\n", __FILE__));
        _RS_AVAL_LINKBLOB_(cd, atype, aval);
}

void rs_aval_unlinkblob(
        void* cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);
        ss_dprintf_2(("%s: rs_aval_unlinkblob\n", __FILE__));
        _RS_AVAL_UNLINKBLOB_(cd, atype, aval);
}

void rs_aval_linkblobif(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);
        ss_dprintf_2(("%s: rs_aval_linkblobif\n", __FILE__));
        _RS_AVAL_LINKBLOBIF_(cd, atype, aval);
}

void rs_aval_unlinkblobif(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);
        ss_dprintf_2(("%s: rs_aval_unlinkblobif\n", __FILE__));
        _RS_AVAL_UNLINKBLOBIF_(cd, atype, aval);
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		rs_aval_setunknown
 *
 * Sets unknown value to aval.
 *
 * Parameters :
 *
 *		cd - in
 *
 *
 *		atype - in
 *
 *
 *		aval - in, use
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
void rs_aval_setunknown(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval)
{
        ss_dprintf_1(("%s: rs_aval_setunknown\n", __FILE__));
        CHECK_AVAL(aval);

        rs_aval_setnull(cd, atype, aval);
        SU_BFLAG_SET(aval->ra_flags, RA_UNKNOWN);
}

/*##**********************************************************************\
 *
 *		rs_aval_copy
 *
 * Member of the SQL function block.
 * Returns a copy of an attribute value
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
 * Return value - give :
 *
 *      Pointer into a newly allocated attribute value object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_aval_t* rs_aval_copy(cd, atype, aval)
    void*       cd;
    rs_atype_t* atype;
    rs_aval_t*  aval;
{
        rs_aval_t* res_aval;
        ss_dprintf_1(("%s: rs_aval_copy\n", __FILE__));
        CHECK_AVAL(aval);
        SS_MEMOBJ_INC(SS_MEMOBJ_AVAL, rs_aval_t);

        res_aval = SSMEM_NEW(rs_aval_t);

        RS_AVAL_COPYBUF2(cd, atype, res_aval, aval);

        return(res_aval);
}

/*##**********************************************************************\
 *
 *		rs_aval_move
 *
 * Copies an attribute value to another buffer. Removes
 * possible vtplref from aval.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		pointer into the attribute type object
 *
 *	res_aval - in out, use
 *		pointer into the result attribute value object
 *
 *	aval - in, use
 *		pointer into the attribute value object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_aval_move(
    void*       cd,
    rs_atype_t* atype,
    rs_aval_t*  res_aval,
    rs_aval_t*  aval)
{
        ss_dprintf_1(("%s: rs_aval_move\n", __FILE__));
        CHECK_AVAL(aval);

        RS_AVAL_FREEBUF(cd, atype, res_aval);
        if (SU_BFLAG_TEST(aval->ra_flags, RA_EXTERNALFLATVA)) {

#if !defined(SS_MYSQL) && !defined(SS_MYSQL_AC)
            ss_derror;
#endif

            rs_aval_setva(cd, atype, res_aval, aval->ra_va);
        } else {
            RS_AVAL_COPYBUF2(cd, atype, res_aval, aval);
        }
}

#ifndef RS_USE_MACROS

/*##**********************************************************************\
 *
 *		rs_aval_isunknown
 *
 * Return TRUE if unknown flag is set for aval.
 *
 * Parameters :
 *
 *		cd -
 *
 *
 *		atype -
 *
 *
 *		aval -
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
bool rs_aval_isunknown(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval)
{
        ss_dprintf_1(("%s: rs_aval_isunknown\n", __FILE__));

        if (aval == NULL) {
            return(TRUE);
        } else {
            CHECK_AVAL(aval);
            return(SU_BFLAG_TEST(aval->ra_flags, RA_UNKNOWN));
        }
}

#endif /* !RS_USE_MACROS */

#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *		rs_aval_isnull
 *
 * Debug compilation only.
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
 *      TRUE if the attribute value contains NULL value
 *      FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_aval_isnull(cd, atype, aval)
    void*       cd;
    rs_atype_t* atype;
    rs_aval_t*  aval;
{
        return(rs_aval_sql_isnull(cd, atype, aval));
}
#endif /* SS_DEBUG */

uint rs_aval_sql_like(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        rs_atype_t* patype,
        rs_aval_t*  paval,
        rs_atype_t* escatype,
        rs_aval_t*  escaval,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        rs_datatype_t pdt;
        rs_datatype_t escdt;
        int escchar = RS_CONS_NOESCCHAR;
        void* str;
        void* pat;
        va_index_t str_len;
        va_index_t pat_len;
        bool match = FALSE;

        ss_dprintf_1(("%s: rs_aval_sql_like\n", __FILE__));
        CHECK_AVAL(aval);
        CHECK_AVAL(paval);

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(paval->ra_flags, RA_DESC));

        dt = RS_ATYPE_DATATYPE(cd, atype);
        pdt = RS_ATYPE_DATATYPE(cd, patype);

        if (RS_AVAL_ISNULL(cd, patype, paval)) {
            return(0);
        }

        if (va_testblob(paval->RA_RDVA)) {
            rs_error_create(p_errh, E_LIKEFAILEDDUETOBLOBPAT);
            return (2);
        }
        if (va_testblob(aval->RA_RDVA)) {
            rs_error_create(p_errh, E_LIKEFAILEDDUETOBLOBVAL);
            return (2);
        }
        if (escatype != NULL) {
            /* In case there is an escape aval, we take the first character
                of the string and make it an escape character because
                there is a possibility that it is not a null-terminated
                string.
            */
            void* p;
            escdt = RS_ATYPE_DATATYPE(cd, escatype);
            p = VA_GETASCIIZ(escaval->RA_RDVA);
            if (escdt == RSDT_UNICODE) {
                ss_char2_t c;

                c = SS_CHAR2_LOAD_MSB1ST(p);
                if (dt == RSDT_UNICODE || ss_isw8bit(c)) {
                    escchar = (int)c;
                }
            } else if (escdt == RSDT_CHAR) {
                escchar = *(ss_byte_t*)p;
            } else {
                rs_error_create(p_errh,
                                E_ILLEGALLIKEESCTYPE_S,
                                rs_atype_name(cd, escatype));
                return (2);
            }
        }
        str = va_getdata(aval->RA_RDVA, &str_len);
        pat = va_getdata(paval->RA_RDVA, &pat_len);
        if (dt == RSDT_CHAR) {
            if (pdt == RSDT_CHAR) {
                match = su_slike(
                            str,
                            str_len - 1,
                            pat,
                            pat_len - 1,
                            escchar);
            } else if (pdt == RSDT_UNICODE) {
                ss_dassert(pat_len & 1);
                pat_len /= sizeof(ss_char2_t);
                match = su_wslike(
                            str,
                            str_len - 1,
                            pat,
                            pat_len,
                            escchar,
                            TRUE);
            }
        } else if (dt == RSDT_UNICODE) {
            ss_dassert(str_len & 1);
            str_len /= sizeof(ss_char2_t);
            if (pdt == RSDT_CHAR) {
                ss_dassert(pat_len >= 1);
                match = su_swlike(
                            str,
                            str_len,
                            pat,
                            pat_len - 1,
                            escchar,
                            TRUE);
            } else {
                ss_dassert(pat_len & 1);
                pat_len /= sizeof(ss_char2_t);
                match = su_wlike(
                            str,
                            str_len,
                            pat,
                            pat_len,
                            escchar,
                            TRUE);
            }
        }
        return (match ? 1 : 0);
}

/*##**********************************************************************\
 *
 *		rs_aval_like
 *
 * Member of the SQL function block.
 * Compares two attribute values wrt. the LIKE operator.
 * The attribute values are assumed to be non-NULL and comparable with LIKE.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		the string argument type
 *
 *	aval - in, use
 *		the string argument value
 *
 *	patype - in, use
 *		the pattern argument type
 *
 *	paval - in, use
 *		the pattern argument value
 *
 *	escatype - in, use
 *          the escape character type (NULL if no escape character)
 *
 *	escaval - in, use
 *		the escape character value (used only if escatype is non-NULL)
 *
 * Return value :
 *
 *      TRUE if the attribute values meet the condition
 *      FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_aval_like(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        rs_atype_t* patype,
        rs_aval_t*  paval,
        rs_atype_t* escatype,
        rs_aval_t*  escaval)
{
        uint r;

        r = rs_aval_sql_like(
                cd,
                atype, aval,
                patype, paval,
                escatype, escaval,
                NULL);
        return (bool)(r & 1);
}

/* NOTE: Following services are not members of SQL-funblock */

/*##**********************************************************************\
 *
 *		rs_aval_setva
 *
 * Converts the given va to the attribute value in destaval.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	destatype - in, use
 *		atype of the destination
 *
 *	destaval - in out, use
 *		destination aval
 *
 *	src_va - in, use
 *		source va
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_aval_setva(
        void*       cd,
        rs_atype_t* destatype,
        rs_aval_t*  destaval,
        va_t*       src_va)
{
        ss_dprintf_1(("%s: rs_aval_setva\n", __FILE__));
        CHECK_AVAL(destaval);
        ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_AGGR));

        if (src_va == VA_DEFAULT) {
            if (rs_atype_getoriginaldefault(cd, destatype) != NULL) {
                rs_aval_assign_ext(
                        cd,
                        destatype,
                        destaval,
                        destatype,
                        rs_atype_getoriginaldefault(cd, destatype),
                        NULL);
            } else {
                ss_dassert(rs_atype_nullallowed(cd, destatype));
                rs_aval_setnull(cd, destatype, destaval);
                ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_CONVERTED | RA_VTPLREF));
            }
        } else if (va_testnull(src_va)) {
            ss_dassert(rs_atype_nullallowed(cd, destatype));
            rs_aval_setnull(cd, destatype, destaval);
            ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_CONVERTED | RA_VTPLREF));
        } else {
            RS_AVAL_UNLINKBLOBIF(cd, destatype, destaval);
            if (SU_BFLAG_TEST(destaval->ra_flags, RA_VTPLREF | RA_FLATVA)) {
                destaval->ra_va = refdva_init();
            }
            {
                size_t glen = va_grosslen(src_va);
                if (glen <= sizeof(destaval->ra_vabuf)) {
                    if (!SU_BFLAG_TEST(destaval->ra_flags,
                                       RA_VTPLREF | RA_ONLYCONVERTED | RA_FLATVA | RA_NULL))
                    {
                        refdva_free(&destaval->ra_va);
                    }
                    memcpy(&destaval->ra_vabuf, src_va, glen);
                    destaval->ra_va = &destaval->ra_vabuf.va;
                    SU_BFLAG_SET(destaval->ra_flags, RA_FLATVA);
                } else {
                    refdva_setva(&destaval->ra_va, src_va);
                    SU_BFLAG_CLEAR(destaval->ra_flags, RA_FLATVA);
                }
                SU_BFLAG_CLEAR(destaval->ra_flags,
                               RA_NULL | RA_CONVERTED | RA_ONLYCONVERTED |
                               RA_VTPLREF | RA_BLOB | RA_UNKNOWN);
            }
            if (va_testblob(src_va)) {
                SU_BFLAG_SET(destaval->ra_flags, RA_BLOB);
                RS_AVAL_LINKBLOB(cd, destatype, destaval);
            }
        }
}

/*##**********************************************************************\
 *
 *		rs_aval_setvaref
 *
 * Converts the given va to the attribute value in destaval.
 * The va is held as a reference to an object held by another handle
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	destatype - in, use
 *		atype of the destination
 *
 *	destaval - in out, use
 *		destination aval
 *
 *	src_va - in, hold
 *		source va
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_aval_setvaref(cd, destatype, destaval, src_va)
        void*       cd;
        rs_atype_t* destatype;
        rs_aval_t*  destaval;
        va_t*       src_va;
{
        ss_dprintf_1(("%s: rs_aval_setvaref\n", __FILE__));
        CHECK_AVAL(destaval);
        ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_AGGR));

        if (src_va == VA_DEFAULT) {
            if (rs_atype_getoriginaldefault(cd, destatype) != NULL) {
                rs_aval_assign_ext(
                        cd,
                        destatype,
                        destaval,
                        destatype,
                        rs_atype_getoriginaldefault(cd, destatype),
                        NULL);
            } else {
                ss_dassert(rs_atype_nullallowed(cd, destatype));
                rs_aval_setnull(cd, destatype, destaval);
                ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_CONVERTED | RA_VTPLREF));
            }
        } else if (va_testnull(src_va)) {
#if 0   /* Removed by Pete 1995-03-29 */
            ss_dassert(rs_atype_nullallowed(cd, destatype)
                || rs_atype_pseudo(cd, destatype)
                || !rs_atype_isuserdefined(cd, destatype));
#endif
            rs_aval_setnull(cd, destatype, destaval);
            ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_CONVERTED | RA_VTPLREF));
        } else {
            RS_AVAL_UNLINKBLOBIF(cd, destatype, destaval);
            if (!SU_BFLAG_TEST(destaval->ra_flags,
                               RA_VTPLREF | RA_NULL |
                               RA_ONLYCONVERTED | RA_FLATVA))
            {
                refdva_done(&destaval->ra_va);
            }
            destaval->ra_va = src_va;
            SU_BFLAG_CLEAR(destaval->ra_flags,
                           RA_NULL |
                           RA_CONVERTED |
                           RA_ONLYCONVERTED |
                           RA_FLATVA |
                           RA_UNKNOWN);
            if (va_testblob(src_va)) {
                SU_BFLAG_SET(destaval->ra_flags, RA_BLOB);
                RS_AVAL_LINKBLOB(cd, destatype, destaval);
            }
            SU_BFLAG_SET(destaval->ra_flags, RA_VTPLREF);
        }
        CHECK_AVAL(destaval);
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *		rs_aval_resetexternalflatva
 * 
 * Debug funtion to clear pointers that point to released memory.
 * 
 * Parameters : 
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
void rs_aval_resetexternalflatva(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* destatype __attribute__ ((unused)),
        rs_aval_t*  destaval)
{
        ss_dprintf_1(("%s: rs_aval_resetexternalflatva\n", __FILE__));
        CHECK_AVAL(destaval);

        if (SU_BFLAG_TEST(destaval->ra_flags, RA_EXTERNALFLATVA)) {
            ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_AGGR));
            ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_BLOB));

            destaval->ra_va = NULL;
            SU_BFLAG_CLEAR(destaval->ra_flags, RA_EXTERNALFLATVA);
        }
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		rs_aval_unlinkvaref
 *
 * Unlinks v-attribute reference.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	destatype -
 *
 *
 *	destaval -
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
void rs_aval_unlinkvaref(
        void*       cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval)
{
        int flags;

        ss_dprintf_1(("%s: rs_aval_unlinkvaref\n", __FILE__));
        ss_dassert(SS_CHKPTR(aval) &&
                   aval->ra_check == RSCHK_ATTRVALUE &&
                   aval->ra_check_end == RSCHK_ATTRVALUE_END);
        /* We can not use CHECK_AVAL(aval) because we may have temporary
           inconsistency between flags and v-attribute content because
           reference v-tuple might have disappered already.
        */

        flags = aval->ra_flags;

        if (SU_BFLAG_TEST(flags, RA_VTPLREF)) {
            ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                      RA_ONLYCONVERTED |
                                      RA_FLATVA));
            RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
            SU_BFLAG_CLEAR(aval->ra_flags, RA_VTPLREF | RA_CONVERTED);
            SU_BFLAG_SET(aval->ra_flags, RA_NULL);
            aval->ra_va = NULL;
        }
        CHECK_AVAL(aval);
}

/*##**********************************************************************\
 *
 *		rs_aval_insertrefdva
 *
 * Similar to rs_aval_setva(), but takes the ownership of the refdva
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	destatype - in, use
 *		atype of destination
 *
 *	destaval - in out, use
 *		aval of destination
 *
 *	src_refdva - in, take
 *		source refdva
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 *      rs_aval_setva()
 */
void rs_aval_insertrefdva(cd, destatype, destaval, src_refdva)
        void*       cd;
        rs_atype_t* destatype;
        rs_aval_t*  destaval;
        refdva_t    src_refdva;
{
        ss_dprintf_1(("%s: rs_aval_insertrefdva\n", __FILE__));
        CHECK_AVAL(destaval);

        if (src_refdva == VA_DEFAULT) {
            /* CR: Is this possible?  Probably not.  Should derror? */
            ss_derror;
            if (rs_atype_getoriginaldefault(cd, destatype) != NULL) {
                rs_aval_assign_ext(
                        cd,
                        destatype,
                        destaval,
                        destatype,
                        rs_atype_getoriginaldefault(cd, destatype),
                        NULL);
            } else {
                ss_dassert(rs_atype_nullallowed(cd, destatype));
                rs_aval_setnull(cd, destatype, destaval);
                ss_dassert(!SU_BFLAG_TEST(destaval->ra_flags, RA_CONVERTED | RA_VTPLREF));
            }
        } else if (va_testnull(src_refdva)) {
            ss_dassert(rs_atype_nullallowed(cd, destatype));
            rs_aval_setnull(cd, destatype, destaval);
        } else {
            size_t glen;
            ss_dassert(destaval->ra_va != src_refdva);
            RS_AVAL_UNLINKBLOBIF(cd, destatype, destaval);
            if (!SU_BFLAG_TEST(destaval->ra_flags,
                               RA_VTPLREF | RA_NULL |
                               RA_FLATVA | RA_ONLYCONVERTED))
            {
                refdva_done(&(destaval->ra_va));
            }
            glen = VA_GROSSLEN(src_refdva);
            SU_BFLAG_CLEAR(destaval->ra_flags,
                RA_NULL | RA_VTPLREF | RA_CONVERTED |
                RA_FLATVA | RA_ONLYCONVERTED | RA_UNKNOWN);
            if (glen <= sizeof(destaval->ra_vabuf)) {
                memcpy(&destaval->ra_vabuf.va, src_refdva, glen);
                SU_BFLAG_SET(destaval->ra_flags, RA_FLATVA);
                destaval->ra_va = &destaval->ra_vabuf.va;
                refdva_done(&src_refdva);
            } else {
                destaval->ra_va = src_refdva;
            }
            if (va_testblob(destaval->ra_va)) {
                SU_BFLAG_SET(destaval->ra_flags, RA_BLOB);
                RS_AVAL_LINKBLOB(cd, destatype, destaval);
            }
        }
}

#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *		rs_aval_va
 *
 * Creates a v-attribute from an attribute value. Note that if the
 * aval is of type RSDT_CHAR, the returned va contains a trailing
 * null byte that does not belong to the data.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		Pointer to attribute type object
 *
 *	aval - in, use
 *		Pointer to attribute value object
 *
 * Return value - ref :
 *
 *      Pointer to va_t object corresponding to the aval
 *
 * Limitations  :
 *
 *      If the aval is of type RSDT_CHAR, the returned va contains
 *      a trailing null byte that does not belong to the data.
 *
 * Globals used :
 */
va_t* rs_aval_va(cd, atype, aval)
        void*       cd;
        rs_atype_t* atype;
        rs_aval_t*  aval;
{
        ss_dprintf_1(("%s: rs_aval_va\n", __FILE__));
        SS_NOTUSED(cd);
        SS_NOTUSED(atype);
        CHECK_AVAL(aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));

        /* WARNING! NULL should not be allowed. */

        return(_RS_AVAL_VA_(cd, atype, aval));
}
#endif /* SS_DEBUG */


/*##**********************************************************************\
 *
 *		rs_aval_getasciiz
 *
 * Returns a pointer to data area containing an asciiz string.
 * If the aval is not of type CHAR, an error is made.
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
 * Return value - ref :
 *
 *      pointer to the data area
 *
 * Limitations  :
 *
 * Globals used :
 */
char* rs_aval_getasciiz(
        void*       cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t*  aval
) {
        ss_dprintf_1(("%s: rs_aval_getasciiz\n", __FILE__));
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_CHAR);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));

        if (!SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED)) {
            aval->ra_.str = VA_GETASCIIZ(aval->RA_RDVA);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED);
        }
        return(aval->ra_.str);
}

#ifndef rs_aval_getlong
/*##**********************************************************************\
 *
 *		rs_aval_getlong
 *
 * Returns the data content from an RSDT_INTEGER type aval.
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
 *      Attribute value in C-long format
 *
 * Limitations  :
 *
 * Globals used :
 */
long rs_aval_getlong(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  aval)
{

        ss_dprintf_1(("%s: rs_aval_getlong\n", __FILE__));
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_INTEGER);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_AGGR));

        return _RS_AVAL_GETLONG_(cd, atype, aval);
}
#endif /* !defined(rs_aval_getlong) */

#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *		rs_aval_isblob
 *
 * Tests whether an aval is a blob reference
 *
 * Parameters :
 *
 *	cd - in
 *		client data
 *
 *	atype - in
 *		attribute type
 *
 *	aval - in, use
 *		attribute value
 *
 * Return value :
 *      FALSE aval is not a BLOB
 *      non-FALSE (not 1!) when value is a BLOB
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_aval_isblob(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        SS_NOTUSED(cd);
        SS_NOTUSED(atype);
        CHECK_AVAL(aval);

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(SU_BFLAG_TEST(aval->ra_flags, RA_BLOB) ?
                   (!SU_BFLAG_TEST(aval->ra_flags, RA_NULL)) : TRUE);
        ss_debug(
                if (SU_BFLAG_TEST(aval->ra_flags, RA_BLOB)) {
                    ss_assert(va_testblob(aval->RA_RDVA));
                } else {
                    /* handles NULL correctly! */
                    ss_assert(!va_testblob(rs_aval_va(cd, atype, aval)));
                }
            );
        return (_RS_AVAL_ISBLOB_(cd, atype, aval));
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		rs_aval_trimchar
 *
 * Truncates trailing spaces from CHAR attribute and
 * truncates characters that exceed attribute type length from
 * CHAR, VARCHAR, BINARY and VARBINARY attributes.
 *
 * Parameters :
 *
 *	cd - in
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - in out, use
 *		attribute value
 *
 *      truncate - in
 *          when TRUE, the truncation to Max. length is enabled,
 *          otherwise max. length of atype is ignored
 *
 * Return value :
 *      TRUE when everything OK or
 *      FALSE when data was longer than fits into atype
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_aval_trimchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool truncate)
{
        bool retp;
        ss_byte_t* p;
        va_index_t orig_length;
        va_index_t length;
        ulong atype_length;
        refdva_t rdva;
        rs_sqldatatype_t sqldt;

        CHECK_AVAL(aval);
        ss_dassert(atype != NULL);

        retp = TRUE;
        if (SU_BFLAG_TEST(aval->ra_flags, RA_NULL | RA_BLOB)) {
            CHECK_AVAL(aval);
            return (retp);
        }
        sqldt = RS_ATYPE_SQLDATATYPE(cd, atype);
        switch (sqldt) {
            case RSSQLDT_CHAR:
            case RSSQLDT_VARCHAR:
            case RSSQLDT_BINARY:
            case RSSQLDT_VARBINARY:
                if (truncate) {
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA)) {
                        rdva = refdva_init();
                    } else {
                        rdva = aval->RA_RDVA;
                        refdva_link(aval->RA_RDVA);
                    }
                    p = (ss_byte_t*)va_getdata(aval->RA_RDVA, &length);
                    length--; /* exclude term. null-byte */
                    orig_length = length;
                    atype_length = RS_ATYPE_LENGTH(cd, atype);
                    if (atype_length != 0 && atype_length != RS_LENGTH_NULL) {
                        if (atype_length < (ulong)length) {
                            length = atype_length;
                            retp = FALSE;
                            if (SU_BFLAG_TEST(aval->ra_flags,
                                              RA_VTPLREF | RA_FLATVA))
                            {
                                refdva_setva(&rdva, aval->RA_RDVA);
                                p = (ss_byte_t*)VA_GETASCIIZ(rdva);
                            }
                            {
                                RS_AVALRET_T succp;

                                succp = rs_aval_setbdata_ext(cd, atype, aval, p, (uint)length, NULL);
                                ss_dassert(succp == RSAVR_SUCCESS);
                            }
                        }
                    }
                    refdva_done(&rdva);
                }
                if (sqldt == RSSQLDT_CHAR) {
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA))
                    {
                        rdva = refdva_init();
                    } else {
                        rdva = aval->RA_RDVA;
                        refdva_link(aval->RA_RDVA);
                    }
                    p = (ss_byte_t*)va_getdata(aval->RA_RDVA, &length);
                    length--; /* exclude term. null-byte */
                    orig_length = length;
                    while (length && (p-1)[length] == ' ') {
                        length--;
                    }
                    if (length < orig_length) {
                        if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA))
                        {
                            refdva_setva(&rdva, aval->RA_RDVA);
                            p = (ss_byte_t*)VA_GETASCIIZ(rdva);
                        }
                        {
                            RS_AVALRET_T succp;

                            succp = rs_aval_setbdata_ext(cd, atype, aval, p, (uint)length, NULL);
                            ss_dassert(succp == RSAVR_SUCCESS);
                        }
                    }
                    refdva_done(&rdva);
                }
                break;
            case RSSQLDT_WCHAR:
            case RSSQLDT_WVARCHAR:
                if (truncate) {
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA)) {
                        rdva = refdva_init();
                    } else {
                        rdva = aval->RA_RDVA;
                        refdva_link(aval->RA_RDVA);
                    }
                    p = (ss_byte_t*)va_getdata(aval->RA_RDVA, &length);
                    ss_dassert(length & 1);
                    length /= sizeof(ss_char2_t);
                    orig_length = length;
                    atype_length = RS_ATYPE_LENGTH(cd, atype);
                    if (atype_length != RS_LENGTH_NULL) {
                        ss_dassert(atype_length != 0);
                        if (atype_length < length) {
                            length = atype_length;
                            retp = FALSE;
                            if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA)) {
                                refdva_setva(&rdva, aval->RA_RDVA);
                                aval->RA_RDVA = NULL;
                                p = (ss_byte_t*)VA_GETASCIIZ(rdva);
                            }
                            SU_BFLAG_CLEAR(aval->ra_flags,
                                RA_CONVERTED | RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                            refdva_setdataandnull(
                                &(aval->RA_RDVA),
                                p,
                                length * sizeof(ss_char2_t));
                        }
                    }
                    refdva_done(&rdva);
                }
                if (sqldt == RSSQLDT_WCHAR) {
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA)) {
                        rdva = refdva_init();
                    } else {
                        rdva = aval->RA_RDVA;
                        refdva_link(aval->RA_RDVA);
                    }
                    p = (ss_byte_t*)va_getdata(aval->RA_RDVA, &length);
                    ss_dassert(length & 1);
                    length /= sizeof(ss_char2_t);
                    orig_length = length;
                    {
                        ss_char2_t* p2 = (ss_char2_t*)p + length - 1;

                        for (; length != 0; length--, p2--) {
                            if (SS_CHAR2_LOAD_MSB1ST(p2) != ' ') {
                                break;
                            }
                        }
                    }
                    if (length < orig_length) {
                        if (SU_BFLAG_TEST(aval->ra_flags,
                                          RA_VTPLREF | RA_FLATVA))
                        {
                            refdva_setva(&rdva, aval->RA_RDVA);
                            aval->RA_RDVA = NULL;
                            p = (ss_byte_t*)VA_GETASCIIZ(rdva);
                        }
                        SU_BFLAG_CLEAR(aval->ra_flags,
                            RA_CONVERTED | RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                        refdva_setdataandnull(
                            &(aval->RA_RDVA),
                            p,
                            length * sizeof(ss_char2_t));
#ifdef SS_DEBUG
                        {
                            ss_byte_t* p1;
                            va_index_t l1;

                            p1 = va_getdata(aval->RA_RDVA, &l1);
                            ss_assert(l1 & 1);
                            ss_assert(memcmp(p, p1, l1 - 1) == 0);
                            ss_assert(l1 == length * sizeof(ss_char2_t) + 1);
                            ss_assert(p1[l1-1] == '\0');
                        }
#endif /* SS_DEBUG */
                    }
                    refdva_done(&rdva);
                }
                break;
#ifdef SS_DEBUG
            /* Check the SQL data type is legal! */
            case RSSQLDT_BIT:
            case RSSQLDT_TINYINT:
            case RSSQLDT_BIGINT:
            case RSSQLDT_LONGVARBINARY:
            case RSSQLDT_LONGVARCHAR:
            case RSSQLDT_NUMERIC:
            case RSSQLDT_DECIMAL:
            case RSSQLDT_INTEGER:
            case RSSQLDT_SMALLINT:
            case RSSQLDT_FLOAT:
            case RSSQLDT_REAL:
            case RSSQLDT_DOUBLE:
            case RSSQLDT_DATE:
            case RSSQLDT_TIME:
            case RSSQLDT_TIMESTAMP:
            case RSSQLDT_WLONGVARCHAR:
                break;
            default:
                ss_error;   /* unknown SQL data type */
#else   /* SS_DEBUG */
            default:
                break;
#endif  /* SS_DEBUG */
        }
        CHECK_AVAL(aval);
        return (retp);
}

/*##**********************************************************************\
 *
 *		rs_aval_isliteral
 *
 * Checks whether an aval is from a literal
 *
 * Parameters :
 *
 *	cd - in
 *		client data
 *
 *	atype - in, use
 *		attribute type (not used)
 *
 *	aval - in, use
 *		attribute value to test
 *
 * Return value :
 *      TRUE if literal or
 *      FALSE otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_aval_isliteral(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        SS_NOTUSED(atype);

        return (SU_BFLAG_TEST(aval->ra_flags, RA_LITERAL));
}

/*##**********************************************************************\
 *
 *		rs_aval_setliteralflag
 *
 * Sets or clears aval literal flag
 *
 * Parameters :
 *
 *	cd - in
 *		client data
 *
 *	atype - in, use
 *		attribute type (not used)
 *
 *	aval - in out, use
 *		attribute value
 *
 *	literal_flag - in
 *		TRUE to set, FALSE to clear the literal flag
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_aval_setliteralflag(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool literal_flag)
{
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        SS_NOTUSED(atype);

        if (literal_flag) {
            SU_BFLAG_SET(aval->ra_flags, RA_LITERAL);
        } else {
            SU_BFLAG_CLEAR(aval->ra_flags, RA_LITERAL);
        }
}


/*##**********************************************************************\
 *
 *		rs_aval_setchcvt
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	atype -
 *
 *
 *	aval -
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
void rs_aval_setchcvt(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);
        ss_dassert(!rs_aval_ischcvt(cd, atype, aval));
        SS_NOTUSED(atype);

        SU_BFLAG_SET(aval->ra_flags, RA_CHCVT);
}

/*##**********************************************************************\
 *
 *		rs_aval_ischcvt
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	atype -
 *
 *
 *	aval -
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
bool rs_aval_ischcvt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);
        SS_NOTUSED(cd);
        SS_NOTUSED(atype);

        return (SU_BFLAG_TEST(aval->ra_flags, RA_CHCVT));
}

/*##**********************************************************************\
 *
 *		rs_aval_cmp_simple
 *
 * Simple comparison function, compared base types must be equal.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	atype1 -
 *
 *
 *	aval1 -
 *
 *
 *	atype2 -
 *
 *
 *	aval2 -
 *
 *
 *	relop -
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
bool rs_aval_cmp_simple(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        uint        relop
) {
        int comp;

        ss_dassert(RS_ATYPE_DATATYPE(cd, atype1) == RS_ATYPE_DATATYPE(cd, atype2));

        comp = va_compare(
                rs_aval_va(cd, atype1, aval1),
                rs_aval_va(cd, atype2, aval2));

        switch (relop) {
            case RS_RELOP_EQUAL:
                return (comp == 0);
            case RS_RELOP_NOTEQUAL:
                return (comp != 0);
            case RS_RELOP_LT:
                return (comp < 0);
            case RS_RELOP_GT:
                return (comp > 0);
            case RS_RELOP_LE:
                return (comp <= 0);
            case RS_RELOP_GE:
                return (comp >= 0);
            default:
                ss_error;
                return (FALSE);
        }
}

static su_ret_t aval_dummy_blobrefcount_dec(
        rs_sysi_t* cd __attribute__ ((unused)),
        va_t* p_blobrefva __attribute__ ((unused)),
        su_err_t** p_errh __attribute__ ((unused)))
{
        ss_dprintf_1(("aval_dummy_blobrefcount_dec called\n"));
        return (SU_SUCCESS);
}

static su_ret_t aval_dummy_blobrefcount_inc(
        rs_sysi_t* cd __attribute__ ((unused)),
        va_t* p_blobrefva __attribute__ ((unused)),
        su_err_t** p_errh __attribute__ ((unused)))
{
        ss_dprintf_1(("aval_dummy_blobrefcount_inc called\n"));
        return (SU_SUCCESS);
}


static bool aval_dummy_isblobg2(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval __attribute__ ((unused)))
{
        ss_derror;
        return (FALSE);
}

static ss_int8_t aval_dummy_getblobg2idorsize(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval __attribute__ ((unused)))
{
        static const ss_int8_t i8 = {0};
        ss_derror;
        return (i8);
}

static rs_aval_blobrefcallbackfun_t* rs_aval_blobrefcount_inc_callbackfun =
        aval_dummy_blobrefcount_inc;

static rs_aval_blobrefcallbackfun_t* rs_aval_blobrefcount_dec_callbackfun =
        aval_dummy_blobrefcount_dec;

static rs_aval_blobidnullifycallback_t* rs_aval_nullifyblobid_callbackfun =
        NULL;

static rs_aval_isblobg2callbackfun_t* rs_aval_isblobg2_callbackfun =
        aval_dummy_isblobg2;

static rs_aval_getblobg2idorsizecallbackfun_t*
        rs_aval_getblobg2size_callbackfun =
            aval_dummy_getblobg2idorsize;

static rs_aval_getblobg2idorsizecallbackfun_t*
        rs_aval_getblobg2id_callbackfun =
            aval_dummy_getblobg2idorsize;

void rs_aval_nullifyblobid(rs_sysi_t* cd,
                           rs_atype_t* atype __attribute__ ((unused)),
                           rs_aval_t* aval)
{
        refdva_t tmp_rdva = refdva_init();
        ss_dassert(SU_BFLAG_TEST(aval->ra_flags, RA_BLOB));
        RS_AVAL_UNLINKBLOB(cd, atype, aval);
        SU_BFLAG_SET(aval->ra_flags, RA_BLOB);
        refdva_setva(&tmp_rdva, aval->RA_RDVA);
        (*rs_aval_nullifyblobid_callbackfun)(tmp_rdva);
        if (!SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA)) {
            refdva_free(&aval->RA_RDVA);
        } else {
            SU_BFLAG_CLEAR(aval->ra_flags, RA_VTPLREF | RA_FLATVA);
        }
        aval->RA_RDVA = tmp_rdva;
}

su_ret_t rs_aval_blobrefcount_inc(
        rs_sysi_t* cd,
        rs_aval_t* aval,
        su_err_t** p_errh)
{
        su_ret_t rc;
        ss_dprintf_1(("rs_aval_blobrefcount_inc(aval=0x%08lX)\n",
                      (ulong)aval));
        rc = (*rs_aval_blobrefcount_inc_callbackfun)(
                cd, aval->RA_RDVA, p_errh);
        return (rc);
}

su_ret_t rs_aval_blobrefcount_dec(
        rs_sysi_t* cd,
        rs_aval_t* aval,
        su_err_t** p_errh)
{
        su_ret_t rc;
        ss_dprintf_1(("rs_aval_blobrefcount_dec(aval=0x%08lX)\n",
                      (ulong)aval));
        rc = (*rs_aval_blobrefcount_dec_callbackfun)(
                cd, aval->RA_RDVA, p_errh);
        return (rc);
}

bool rs_aval_isblobg2(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        return ((*rs_aval_isblobg2_callbackfun)(cd, atype, aval));
}

ss_int8_t rs_aval_getblobsize(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        return ((*rs_aval_getblobg2size_callbackfun)(cd, atype, aval));
}

ss_int8_t rs_aval_getblobid(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        return ((*rs_aval_getblobg2id_callbackfun)(cd, atype, aval));
}


static bool aval_defaultreadblobavalfun(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval __attribute__ ((unused)),
        size_t sizelimit __attribute__ ((unused)))
{
        return (FALSE);
}

static size_t aval_loadbloblimit = RS_AVAL_DEFAULTMAXLOADBLOBLIMIT;

rs_aval_loadblobcallbackfun_t* rs_aval_loadblob = aval_defaultreadblobavalfun;

void rs_aval_setloadblobcallbackfun(
        void* cd __attribute__ ((unused)),
        rs_aval_loadblobcallbackfun_t* callbackfp)
{
        rs_aval_loadblob = callbackfp;
}

void rs_aval_setloadblobsizelimit(
        void* cd __attribute__ ((unused)),
        size_t sizelimit)
{
        aval_loadbloblimit = sizelimit;
}

size_t rs_aval_getloadblobsizelimit(void* cd __attribute__ ((unused)))
{
        return (aval_loadbloblimit);
}

void rs_aval_globalinstallblobrefcallbacks(
        rs_aval_blobrefcallbackfun_t* blobref_inc_callbackfun,
        rs_aval_blobrefcallbackfun_t* blobref_dec_callbackfun,
        rs_aval_blobidnullifycallback_t* blobidnullify_callbackfun,
        rs_aval_isblobg2callbackfun_t* isblobg2_callbackfun,
        rs_aval_getblobg2idorsizecallbackfun_t* getblobg2size_callbackfun,
        rs_aval_getblobg2idorsizecallbackfun_t* getblobg2id_callbackfun)
{
        rs_aval_blobrefcount_inc_callbackfun = blobref_inc_callbackfun;
        rs_aval_blobrefcount_dec_callbackfun = blobref_dec_callbackfun;
        rs_aval_nullifyblobid_callbackfun = blobidnullify_callbackfun;
        rs_aval_isblobg2_callbackfun = isblobg2_callbackfun;
        rs_aval_getblobg2size_callbackfun = getblobg2size_callbackfun;
        rs_aval_getblobg2id_callbackfun = getblobg2id_callbackfun;

}

#ifdef SS_MME
/* WARNING - THE FUNCTIONS BELOW ARE NOT TESTED IN res/test. */

#ifndef rs_aval_ismin
bool rs_aval_ismin(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype __attribute__ ((unused)),
        rs_aval_t*      aval)
{
        return _RS_AVAL_ISMIN_(cd, atype, aval);
}
#endif /* !defined(rs_aval_ismin) */

#ifndef rs_aval_setmin
void rs_aval_setmin(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype __attribute__ ((unused)),
        rs_aval_t*      aval,
        bool            flag)
{
        _RS_AVAL_SETMIN_(cd, atype, aval, flag);
}
#endif /* !defined(rs_aval_setmin) */

#ifndef rs_aval_ismax
bool rs_aval_ismax(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype __attribute__ ((unused)),
        rs_aval_t*      aval)
{
        return _RS_AVAL_ISMAX_(cd, atype, aval);
}
#endif /* !defined(rs_aval_ismax) */

#ifndef rs_aval_setmax
void rs_aval_setmax(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype __attribute__ ((unused)),
        rs_aval_t*      aval,
        bool            flag)
{
        _RS_AVAL_SETMAX_(cd, atype, aval, flag);
}
#endif /* !defined(rs_aval_setmax) */

#endif /* SS_MME */
