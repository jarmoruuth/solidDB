/*************************************************************************\
**  source       * rs0tval.c
**  directory    * res
**  description  * Tuple value functions
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
This module implements a tuple value object. tuple value consists of
an array of aval objects.
A tval object is always used together with an ttype object. The access to
single attribute values in tval can be using the physical or
SQL attribute numbering. (See the documentation in rs0ttype.c)

Limitations:
-----------


Error handling:
--------------
None. Dasserts.

Objects used:
------------
Attribute type      <rs0atype.h>
Attribute value     <rs0aval.h>
Tuple type          <rs0ttype.h>


Preconditions:
-------------
None

Multithread considerations:
--------------------------
Code is fully re-entrant.
The same tval object can not be used simultaneously from many threads.


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define RS0TVAL_C

#include <ssc.h>
#include <ssstring.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sslimits.h>
#include <ssint8.h>

#include <uti0vcmp.h>
#include <uti0vtpl.h>

#include <su0parr.h>

#include "rs0types.h"
#include "rs0ttype.h"
#include "rs0atype.h"
#include "rs0aval.h"
#include "rs0tval.h"
#include "rs0sysi.h"
#include "rs0sdefs.h"

#ifdef SS_DEBUG

/*#***********************************************************************\
 *
 *              tval_vtplrefcount
 *
 * Counts number of vtuple references from avals in this tval
 *
 * Parameters :
 *
 *      tval - in, use
 *              tuple value object
 *
 * Return value :
 *      # of references
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
#ifndef SS_MYSQL
static uint tval_vtplrefcount(rs_tval_t* tval)
{
        uint i;
        uint count;

        if (tval->tv_vtpl == NULL) {
            ss_debug(
                for (i = 0; i < (uint)tval->tv_nattrs; i++) {
                    ss_assert(!SU_BFLAG_TEST(tval->tv_attr_arr[i].ra_flags,
                                             RA_VTPLREF));
                }
            );
            return (0);
        }
        for (i = 0, count = 0; i < (uint)tval->tv_nattrs; i++) {
            if (SU_BFLAG_TEST(tval->tv_attr_arr[i].ra_flags, RA_VTPLREF)) {
                count++;
            }
        }
        return (count);
}
#endif /* !SS_MYSQL */

#endif /* SS_DEBUG */

/*#***********************************************************************\
 *
 *              tval_create_empty
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ttype -
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
static rs_tval_t* tval_create_empty(
        rs_sysi_t*  cd,
        rs_ttype_t* ttype
) {
        rs_ano_t   nattrs;
        rs_tval_t* tval;

        ss_dprintf_4(("%s: tval_create_empty\n", __FILE__));
        SS_MEMOBJ_INC(SS_MEMOBJ_TVAL, rs_tval_t);

        nattrs = RS_TTYPE_NATTRS(cd, ttype);
        tval = rs_sysi_qmemctxalloc(cd, sizeof(rs_tval_t) + (nattrs - 1) * sizeof(rs_aval_t));
        ss_debug(tval->tv_check = RSCHK_TUPLEVALUE);
        ss_debug(tval->tv_name = NULL);
        tval->tv_vtpl = NULL;
        tval->tv_vtplalloc = NULL;
        tval->tv_nattrs = nattrs;
        tval->tv_nlink = 1;
        tval->tv_rowflags = 0;

        ss_debug(
            if (ttype->tt_shttype->stt_name != NULL) {
                tval->tv_name = SsMemStrdup(ttype->tt_shttype->stt_name);
            }
        );
        CHECK_TVAL(tval);
        return(tval);
}

/*##**********************************************************************\
 *
 *              rs_tval_create
 *
 * Member of the SQL function block.
 * Creates a new tuple value object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 * Return value - give :
 *
 *      pointer into the newly allocated tuple value object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tval_t* rs_tval_create(
        void*       cd,
        rs_ttype_t* ttype
) {
        rs_ano_t   nattrs;
        rs_ano_t   i;
        rs_tval_t* tval;

        ss_dprintf_3(("%s: rs_tval_create\n", __FILE__));

        nattrs = RS_TTYPE_NATTRS(cd, ttype);

        tval = tval_create_empty(cd, ttype);

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;
            
            atype= RS_TTYPE_ATYPE(cd, ttype, i);
            aval = RS_TVAL_AVAL(cd, ttype, tval, i);

            RS_AVAL_CREATEBUF(
                cd,
                atype,
                aval);
        }

        CHECK_TVAL(tval);
        ss_dprintf_1(("rs_tval_create:tval = %08lx\n", (long)tval));
        return(tval);
}

/*##**********************************************************************\
 *
 *              rs_tval_free
 *
 * Member of the SQL function block.
 * Releases a tuple value object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in, take
 *              tuple value object to be released
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_free(
        rs_sysi_t*  cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
) {

        rs_ano_t i;
        int nattrs;

        CHECK_TVAL(tval);
        ss_dprintf_1(("rs_tval_free:tval = %08lx\n", (long)tval));
        ss_rc_dassert(tval->tv_nlink > 0, tval->tv_nlink);
#ifndef SS_MME
        ss_dassert(tval->tv_nattrs == rs_ttype_nattrs(cd, ttype));
#else
        /* XXX - THIS IS TEMPORARY. */
        ss_dassert(tval->tv_nattrs <= (int)rs_ttype_nattrs(cd, ttype));
#endif

        tval->tv_nlink--;

        if (tval->tv_nlink == 0) {
            SS_MEMOBJ_DEC(SS_MEMOBJ_TVAL);

            nattrs = tval->tv_nattrs;

            for (i = 0; i < nattrs; i++) {
                rs_atype_t* atype;
                rs_aval_t* aval;

                atype = RS_TTYPE_ATYPE(cd, ttype, i);
                aval = RS_TVAL_AVAL(cd, ttype, tval, i);
                RS_AVAL_FREEBUF(
                    cd,
                    atype,
                    aval);
            }
            if (tval->tv_vtpl != NULL) {
                rs_tval_unlinkfromvtpl(cd, ttype, tval);
            }
            ss_debug(
                if (tval->tv_name != NULL) {
                    SsMemFree(tval->tv_name);
                }
            );
            rs_sysi_qmemctxfree(cd, tval);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_link
 *
 * Increments reference link count to a tval object.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tval -
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
void rs_tval_link(
        void*       cd __attribute__ ((unused)),
        rs_tval_t*  tval
) {
        CHECK_TVAL(tval);

        tval->tv_nlink++;
}

/*##**********************************************************************\
 *
 *              rs_tval_reset
 *
 * Resets a tuple value object to an empty state, all values are NULL.
 * This may be necessary for example when tval contains BLOBs and it is not
 * released otherwise before connection is closed.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in, take
 *              tuple value object to be reset
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_reset(
        rs_sysi_t*  cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
) {

        rs_ano_t i;
        int nattrs;

        CHECK_TVAL(tval);
        ss_dprintf_1(("rs_tval_reset:tval = %08lx\n", (long)tval));
        ss_rc_dassert(tval->tv_nlink > 0, tval->tv_nlink);
#ifndef SS_MME
        ss_dassert(tval->tv_nattrs == rs_ttype_nattrs(cd, ttype));
#else
        /* XXX - THIS IS TEMPORARY. */
        ss_dassert(tval->tv_nattrs <= (int)rs_ttype_nattrs(cd, ttype));
#endif

        nattrs = tval->tv_nattrs;

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = RS_TTYPE_ATYPE(cd, ttype, i);
            aval = RS_TVAL_AVAL(cd, ttype, tval, i);
            rs_aval_setnull(
                cd,
                atype,
                aval);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_updateusecount
 *
 * Increments or decrements reference link count to a tval object.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tval -
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
void rs_tval_updateusecount(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        bool        inc
) {
        CHECK_TVAL(tval);
        ss_rc_dassert(inc == 1 || inc == -1, inc);

        if (inc > 0) {
            rs_tval_link(cd, tval);
        } else {
            rs_tval_free(cd, ttype, tval);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_sql_usecount
 *
 * Returns current link count.
 *
 * Parameters :
 *
 *              cd - in
 *
 *
 *              ttype - in
 *
 *
 *              tval - in
 *
 *
 * Return value :
 *
 *      Current link count, always > 0.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
uint rs_tval_sql_usecount(
        void*       cd __attribute__ ((unused)),
        rs_ttype_t* ttype __attribute__ ((unused)),
        rs_tval_t*  tval
) {
        CHECK_TVAL(tval);

        return(tval->tv_nlink);
}

/*##**********************************************************************\
 *
 *              rs_tval_setaval
 *
 * Sets the value of an attribute of tuple value object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      phys_attr_n - in
 *              physical attibute number in tuple
 *
 *      aval - in, use
 *              the new value for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_setaval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        rs_aval_t*  aval
) {
        ss_dprintf_3(("%s: rs_tval_setaval\n", __FILE__));
        CHECK_TVAL(tval);
        {
            rs_atype_t* atype;
            rs_aval_t*  tval_aval;

            ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);

            tval_aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);

            atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);
            RS_AVAL_FREEBUF(cd, atype, tval_aval);
            RS_AVAL_COPYBUF2(cd, atype, tval_aval, aval);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_sql_setaval
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_tval_setaval.
 *
 */
void rs_tval_sql_setaval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n,
        rs_aval_t*  aval
) {
        CHECK_TVAL(tval);
        {
            rs_ano_t phys_ano = RS_TTYPE_QUICKSQLANOTOPHYS(cd, ttype, sql_attr_n);
            rs_tval_setaval(cd, ttype, tval, phys_ano, aval);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_insertaval
 *
 * Similar to rs_tval_setaval, but does not make a new copy of aval.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in, use
 *              tuple value
 *
 *      phys_attr_n - in
 *              attribute number
 *
 *      aval - in, take
 *              new attribute value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_insertaval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        rs_aval_t*  aval
) {
        ss_dprintf_3(("%s: rs_tval_setaval\n", __FILE__));
        CHECK_TVAL(tval);
        {
            rs_atype_t* atype;
            rs_aval_t*  tval_aval;

            ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);

            atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);

            tval_aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);

            RS_AVAL_FREEBUF(cd, atype, tval_aval);
            RS_AVAL_COPYBUF2(cd, atype, tval_aval, aval);
            rs_aval_free(cd, atype, aval);
        }
}

#ifndef rs_tval_aval
/*##**********************************************************************\
 *
 *              rs_tval_aval
 *
 * Gets a value of a certain attribute in tuple value.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in, use
 *              tuple value
 *
 *      phys_attr_n - in
 *              physical attribute number
 *
 * Return value - ref :
 *
 *      Pointer into a value object containing the attribute value
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_aval_t* rs_tval_aval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n
) {
        rs_aval_t* aval;

        ss_dprintf_3(("%s: rs_tval_aval\n", __FILE__));
        CHECK_TVAL(tval);
        SS_NOTUSED(ttype);
        SS_NOTUSED(cd);
        ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);
        aval = _RS_TVAL_AVAL_(cd, ttype, tval, phys_attr_n);
        return(aval);
}
#endif /* defined(rs_tval_aval) */

/*##**********************************************************************\
 *
 *              rs_tval_sql_aval
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_tval_aval.
 *
 */
rs_aval_t* rs_tval_sql_aval(
        void*       cd __attribute__ ((unused)),
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n
) {
        ss_debug(
            if (tval == NULL || tval->tv_check != RSCHK_TUPLEVALUE) {
                ss_dprintf_1(("rs_tval_sql_aval:tval = %08lx\n", (long)tval));
            }
        );
        CHECK_TVAL(tval);
        {
            rs_ano_t phys_ano = RS_TTYPE_QUICKSQLANOTOPHYS(cd, ttype, sql_attr_n);
            return(RS_TVAL_AVAL(cd, ttype, tval, phys_ano));
        }
}


/*##**********************************************************************\
 *
 *              rs_tval_copy
 *
 * Allocates a copy of a tuple value object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in, use
 *              tuple value
 *
 * Return value - give :
 *
 *      Pointer into the newly allocated copy of the tuple value object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tval_t* rs_tval_copy(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
) {
        ss_dprintf_3(("%s: rs_tval_copy\n", __FILE__));
        CHECK_TVAL(tval);
        ss_dassert((uint)tval->tv_nattrs == rs_ttype_nattrs(cd, ttype));
        {
            rs_tval_t* new_tval;
            rs_aval_t* new_aval;
            rs_atype_t* atype;
            rs_ano_t i;
            rs_ano_t count;
            size_t s;

            SS_MEMOBJ_INC(SS_MEMOBJ_TVAL, rs_tval_t);

            s = sizeof(rs_tval_t) +
                (tval->tv_nattrs - 1) * sizeof(rs_aval_t);
            new_tval = SsMemAlloc(s);
            memcpy(new_tval, tval, s);
#ifdef SS_SYNC
            ss_debug(
                if (new_tval->tv_name != NULL) {
                    new_tval->tv_name = SsMemStrdup(new_tval->tv_name);
                    if (ttype->tt_shttype->stt_name != NULL) {
                        char* p1 = ttype->tt_shttype->stt_name;
                        char* p2 = new_tval->tv_name;
                        char* prefix = (char *)RSK_SYNCHIST_TABLENAMESTR;
                        char* tmp = strchr(prefix, '%');
                        size_t prefixlen = (tmp - prefix);

                        /* Skip the '_SYNCHIST_' etc prefix if present
                         * DANGER! We assume that it contains a '%' sign ...
                         */

                        tmp = strchr(prefix, '%');
                        prefixlen = (tmp != NULL) ? (tmp - prefix) : 0;

                        if (SsStrncmp(p1, prefix, prefixlen) == 0) {
                            p1 += prefixlen;
                        }
                        if (SsStrncmp(p2, prefix, prefixlen) == 0) {
                            p2 += prefixlen;
                        }
                        ss_assert(strcmp(p1, p2) == 0);
                    }
                } else if (ttype->tt_shttype->stt_name != NULL) {
                    new_tval->tv_name = SsMemStrdup(ttype->tt_shttype->stt_name);
                }
            );
#else  /* SS_SYNC */
            ss_debug(
                if (new_tval->tv_name != NULL) {
                    new_tval->tv_name = SsMemStrdup(new_tval->tv_name);
                    if (ttype->tt_shttype->stt_name != NULL) {
                        ss_assert(strcmp(ttype->tt_shttype->stt_name,
                                         new_tval->tv_name) == 0);
                    }
                } else if (ttype->tt_shttype->stt_name != NULL) {
                    new_tval->tv_name = SsMemStrdup(ttype->tt_shttype->stt_name);
                }
            );
#endif /* SS_SYNC */
            for (i = 0, count = 0; i < new_tval->tv_nattrs; i++) {
                new_aval = RS_TVAL_AVAL(cd, ttype, new_tval, i);
                atype = RS_TTYPE_ATYPE(cd, ttype, i);
                RS_AVAL_FIX_RAWCOPY(
                        cd,
                        atype,
                        new_aval);
                if (SU_BFLAG_TEST(new_aval->ra_flags, RA_VTPLREF)) {
                    count++;
                }
            }
            if (count != 0) {
                ss_dassert(new_tval->tv_vtpl != NULL);
                SsMemLinkInc(new_tval->tv_vtplalloc);
            } else {
                new_tval->tv_vtpl = NULL;
                new_tval->tv_vtplalloc = NULL;
                if (tval->tv_vtpl != NULL) {
                    rs_tval_unlinkfromvtpl(cd, ttype, tval);
                }
            }
            return(new_tval);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_physcopy
 *
 * Allocates a copy of a tuple value object, this also makes
 * a physical copy of each attribute va (no aliasing).
 * This routine is to be used when several threads may access
 * a copy of the same tval.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in, use
 *              tuple value
 *
 * Return value - give :
 *
 *      Pointer into the newly allocated copy of the tuple value object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_tval_t* rs_tval_physcopy(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval)
{
        rs_tval_t* new_tval;
        rs_ano_t i;
        rs_ano_t count;

        ss_dprintf_3(("%s: rs_tval_physcopy\n", __FILE__));
        CHECK_TVAL(tval);
        ss_dassert((uint)tval->tv_nattrs == rs_ttype_nattrs(cd, ttype));

        new_tval = rs_tval_create(cd, ttype);
        count = tval->tv_nattrs;
        for (i = 0; i < count; i++) {
            va_t* p_va = rs_tval_va(cd, ttype, tval, i);
            rs_tval_setva(cd, ttype, new_tval, i, p_va);
        }
        return(new_tval);
}

/*##**********************************************************************\
 *
 *              rs_tval_setva
 *
 * Sets the value of an attribute of tuple value object from a v-attribute.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      phys_attr_n - in
 *              physical attibute number in tuple
 *
 *      va - in, use
 *              the new value for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_setva(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va
) {
        ss_dprintf_3(("%s: rs_tval_setva\n", __FILE__));
        CHECK_TVAL(tval);
        {
            rs_atype_t* atype;
            rs_aval_t*  tval_aval;

            ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);

            atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);
            tval_aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);

            ss_dprintf_2(("rs_tval_setva:%d\n", __LINE__));

            rs_aval_setva(cd, atype, tval_aval, va);

            ss_dprintf_2(("rs_tval_setva:%d\n", __LINE__));
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_setva_unlink
 *
 * Same as rs_tval_setva but unlinks possible v-attribute reference.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      phys_attr_n - in
 *              physical attibute number in tuple
 *
 *      va - in, use
 *              the new value for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_setva_unlink(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va
) {
        ss_dprintf_3(("%s: rs_tval_setva\n", __FILE__));
        CHECK_TVAL(tval);
        {
            rs_atype_t* atype;
            rs_aval_t*  tval_aval;

            ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);

            atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);
            tval_aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);

            ss_dprintf_2(("rs_tval_setva:%d\n", __LINE__));

            rs_aval_unlinkvaref(cd, atype, tval_aval);
            rs_aval_setva(cd, atype, tval_aval, va);

            ss_dprintf_2(("rs_tval_setva:%d\n", __LINE__));
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_sql_setva
 *
 * Sets the value of an attribute of tuple value object from a v-attribute.
 * It maps the attribute number from SQL point of view
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      sql_attr_n - in
 *              SQL attibute number in tuple
 *
 *      va - in, use
 *              the new value for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_sql_setva(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n,
        va_t*       va
) {
        rs_ano_t phys_attr_n;

        ss_dprintf_3(("%s: rs_tval_setva\n", __FILE__));
        CHECK_TVAL(tval);
        phys_attr_n = RS_TTYPE_QUICKSQLANOTOPHYS(cd, ttype, sql_attr_n);
        rs_tval_setva(cd, ttype, tval, phys_attr_n, va);
}

#ifndef SS_NOBLOB

/*##**********************************************************************\
 *
 *              rs_tval_scanblobs
 *
 * Scans for blob attributes from tval.
 *
 * Parameters :
 *
 *      cd - use
 *              Client data.
 *
 *      ttype - in
 *              Tuple type.
 *
 *      tval - in
 *              Tuple value.
 *
 *      p_phys_attr_n - in out, use
 *              Pointer to a variable containing the attribute number. When
 *              calling this function for the first time, *p_phys_attr_n must
 *              be initialized to -1. When this function returns TRUE,
 *              *p_phys_attr_n is updated to containg the next attribute with
 *              a blob va. This returned value must be in *p_phys_attr_n when
 *              this function is called again. Parameter *p_phys_attr_n works
 *              as a loop variable for this function when scanning for blob
 *              attributes,
 *
 * Return value :
 *
 *      TRUE    - blob attribute found
 *      FALSE   - no more blob attributes left
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_tval_scanblobs(
        void*       cd __attribute__ ((unused)),
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        int*        p_phys_attr_n
) {
        rs_atype_t* atype;
        rs_aval_t*  tval_aval;
        int         phys_attr_n;
        int         nattrs;

        ss_dprintf_3(("%s: rs_tval_scanblobs:attr_n = %d\n", __FILE__, *p_phys_attr_n));
        CHECK_TVAL(tval);
        ss_dassert(*p_phys_attr_n >= -1);

        phys_attr_n = *p_phys_attr_n;
        nattrs = tval->tv_nattrs;

        for (phys_attr_n++; phys_attr_n < nattrs; phys_attr_n++) {
            atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);
            tval_aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);
            if (rs_aval_isblob(cd, atype, tval_aval)) {
                *p_phys_attr_n = phys_attr_n;
                return(TRUE);
            }
        }
        return(FALSE);
}

#endif /* SS_NOBLOB */

/*##**********************************************************************\
 *
 *              rs_tval_vagrosslen
 *
 * Returns the gross length of all attribute vas in the tval.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ttype -
 *
 *
 *      tval -
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
long rs_tval_vagrosslen(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
) {
        int         nattrs;
        long        size;
        int         i;

        ss_dprintf_3(("%s: rs_tval_vagrosslen\n", __FILE__));
        CHECK_TVAL(tval);

        nattrs = tval->tv_nattrs;

        for (size = 0, i = 0; i < nattrs; i++) {
            va_t* va = rs_tval_va(cd, ttype, tval, i);
            size += VA_GROSSLEN(va);
        }
        return(size);
}

/*##**********************************************************************\
 *
 *              rs_tval_vagrosslen_project
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ttype -
 *
 *
 *      attrflags -
 *
 *
 *      tval -
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
long rs_tval_vagrosslen_project(
        void*       cd,
        rs_ttype_t* ttype,
        bool* attrflags,
        rs_tval_t*  tval
) {
        int         nattrs;
        long        size;
        int         i;

        ss_dprintf_3(("%s: rs_tval_vagrosslen_project\n", __FILE__));
        CHECK_TVAL(tval);

        nattrs = tval->tv_nattrs;

        for (size = 0, i = 0; i < nattrs; i++) {
            if (attrflags == NULL || attrflags[i]) {
                va_t* va = rs_tval_va(cd, ttype, tval, i);
                size += VA_GROSSLEN(va);
            }
        }
        return(size);
}


/*##**********************************************************************\
 *
 *              rs_tval_trimchars
 *
 * Truncates trailing spaces from CHAR attributes and
 * truncates VARCHAR, CHAR, BINARY & VARBINARY attributes
 * to their maximum length.
 *
 * Parameters :
 *
 *      cd - in
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      truncate - in
 *          TRUE enables truncation to max. length.
 *          FALSE ignores max. length
 *
 * Return value :
 *      TRUE when everything went OK or
 *      FALSE when 1 or more of the attributes exceeded their max. length
 *          and were therefore truncated.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_tval_trimchars(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        bool truncate)
{
        rs_ano_t i;
        rs_atype_t* atype;
        rs_aval_t* aval;
        bool retp;
        uint count;

        retp = TRUE;

        for (i = 0, count = 0; i < tval->tv_nattrs; i++) {
            atype = RS_TTYPE_ATYPE(cd, ttype, i);
            aval = &(tval->tv_attr_arr[i]);
            switch (RS_ATYPE_DATATYPE(cd, atype)) {
                case RSDT_BINARY:
                    if (!truncate) {
                        break;
                    }
                case RSDT_UNICODE:
                case RSDT_CHAR:
                    if (!rs_aval_trimchar(cd, atype, aval, truncate)) {
                        retp = FALSE;
                    }
                    break;
                default:
                    break;
            }
            if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF)) {
                count++;
            }
        }
        if (tval->tv_vtpl != NULL && count == 0) {
            rs_tval_unlinkfromvtpl(cd, ttype, tval);
        }
        return (retp);
}

/*##**********************************************************************\
 *
 *              rs_tval_ensureconverted
 *
 * Ensures that all attribute values are in converted state, i.e.
 * v-attribute is converted to a C-language data type.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      ttype - in
 *
 *
 *      tval - use
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
void rs_tval_ensureconverted(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        int i;
        int nattrs;

        nattrs = RS_TTYPE_NATTRS(cd, ttype);

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = RS_TTYPE_ATYPE(cd, ttype, i);
            aval = RS_TVAL_AVAL(cd, ttype, tval, i);

            if (!rs_aval_isnull(cd, atype, aval)) {
                switch (RS_ATYPE_DATATYPE(cd, atype)) {
                    case RSDT_CHAR:
                        (void)rs_aval_getasciiz(cd, atype, aval);
                        break;
                    case RSDT_INTEGER:
                        (void)rs_aval_getlong(cd, atype, aval);
                        break;
                    case RSDT_BIGINT:
                        (void)rs_aval_getint8(cd, atype, aval);
                        break;
                    case RSDT_FLOAT:
                        (void)rs_aval_getfloat(cd, atype, aval);
                        break;
                    case RSDT_DOUBLE:
                        (void)rs_aval_getdouble(cd, atype, aval);
                        break;
                    case RSDT_DATE:
                        (void)rs_aval_getdate(cd, atype, aval);
                        break;
                    case RSDT_DFLOAT:
                        (void)rs_aval_getdfloat(cd, atype, aval);
                        break;
                    default:
                        ss_derror;
                }
            }
        }
}

#ifndef SS_NOSQL

/*#***********************************************************************\
 *
 *              rs_tval_realcopy
 *
 * Makes a physical copy of tval, no reference counting is used.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      ttype - in
 *
 *
 *      tval - in
 *
 *
 * Return value - give :
 *
 *      Physical copy of tval.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_tval_t* rs_tval_realcopy(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        int i;
        int nattrs;
        rs_tval_t* new_tval;

        nattrs = RS_TTYPE_NATTRS(cd, ttype);
        new_tval = rs_tval_create(cd, ttype);

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = RS_TTYPE_ATYPE(cd, ttype, i);
            aval = RS_TVAL_AVAL(cd, ttype, tval, i);

            if (!rs_aval_isnull(cd, atype, aval)) {
                rs_tval_setva(
                    cd,
                    ttype,
                    new_tval,
                    i,
                    rs_aval_va(cd, atype, aval));
            }
        }

        return(new_tval);
}

#endif /* SS_NOSQL */

/*##**********************************************************************\
 *
 *              rs_tval_linktovtpl
 *
 * Links tval to dynamically allocated vtuple
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      ttype - in
 *
 *
 *      tval - use
 *
 *
 *      p_vtpl - in, hold
 *              Pointer to v-tuple.
 *
 *      p_vtplalloc - in, hold
 *              Pointer to memory allocation area. This can be different
 *          from p_vtpl if v-tuple is part of another data structure
 *          like dbe_bkey_t.
 *
 *      init - in
 *          TRUE if the vtpl is not linked yet (i.e. just allocated)
 *          FALSE when the vtpl link count has been initialized already
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_tval_linktovtpl(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        vtpl_t* p_vtpl,
        void* p_vtplalloc,
        bool init)
{
        CHECK_TVAL(tval);
        if (tval->tv_vtpl != NULL) {
            rs_ano_t i;
            int nattrs;

            nattrs = tval->tv_nattrs;

            for (i = 0; i < nattrs; i++) {
                rs_atype_t* atype = RS_TTYPE_ATYPE(cd, ttype, i);
                rs_aval_t* aval = RS_TVAL_AVAL(cd, ttype, tval, i);

                rs_aval_unlinkvaref(cd, atype, aval);
            }
            SsMemLinkDecZeroFree(tval->tv_vtplalloc);
        }
        tval->tv_vtpl = p_vtpl;
        tval->tv_vtplalloc = p_vtplalloc;
        if (init) {
            SsMemLinkInit(p_vtplalloc);
        } else {
            SsMemLinkInc(p_vtplalloc);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_linktovtpl_nounlink
 *
 * Same as rs_tval_linktovtpl but does not unlink invidual v-attributes.
 * Those are left as danglink pointers and should be reset by setting
 * them using function rs_tval_setvaref_unlink.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      ttype - in
 *
 *
 *      tval - use
 *
 *
 *      p_vtpl - in, hold
 *              Pointer to v-tuple.
 *
 *      p_vtplalloc - in, hold
 *              Pointer to memory allocation area. This can be different
 *          from p_vtpl if v-tuple is part of another data structure
 *          like dbe_bkey_t.
 *
 *      init - in
 *          TRUE if the vtpl is not linked yet (i.e. just allocated)
 *          FALSE when the vtpl link count has been initialized already
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_tval_linktovtpl_nounlink(
        void* cd __attribute__ ((unused)),
        rs_ttype_t* ttype __attribute__ ((unused)),
        rs_tval_t* tval,
        vtpl_t* p_vtpl,
        void* p_vtplalloc,
        bool init)
{
        CHECK_TVAL(tval);
        if (tval->tv_vtpl != NULL) {
            SsMemLinkDecZeroFree(tval->tv_vtplalloc);
        }
        tval->tv_vtpl = p_vtpl;
        tval->tv_vtplalloc = p_vtplalloc;
        if (init) {
            SsMemLinkInit(p_vtplalloc);
        } else {
            SsMemLinkInc(p_vtplalloc);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_unlinkfromvtpl
 *
 * Unlinks tval from vtuple
 *
 * Parameters :
 *
 *      tval - in out, use
 *              tuple value object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_tval_unlinkfromvtpl(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        CHECK_TVAL(tval);
        SS_NOTUSED(cd);
        SS_NOTUSED(ttype);

        if (tval->tv_vtpl != NULL) {
            ss_dassert(tval->tv_vtplalloc != NULL);
            SsMemLinkDecZeroFree(tval->tv_vtplalloc);
            tval->tv_vtpl = NULL;
            tval->tv_vtplalloc = NULL;
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_setvaref
 *
 * Sets the value of an attribute of tuple value object from a v-attribute.
 * Only reference is assigned in this function
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      phys_attr_n - in
 *              physical attibute number in tuple
 *
 *      va - in, hold
 *              the new value for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_setvaref(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va
) {
        ss_dprintf_3(("%s: rs_tval_setvaref\n", __FILE__));
        CHECK_TVAL(tval);
        {
            rs_atype_t* atype;
            rs_aval_t*  tval_aval;

            ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);

            atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);
            tval_aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);

            rs_aval_setvaref(cd, atype, tval_aval, va);
        }
}

#ifdef SS_DEBUG

void rs_tval_resetexternalflatva(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
) {
        rs_ano_t i;
        int nattrs;

        if (tval == NULL) {
            return;
        }


        CHECK_TVAL(tval);
        ss_dprintf_1(("rs_tval_resetexternalflatva:tval = %08lx\n", (long)tval));

        nattrs = tval->tv_nattrs;

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = RS_TTYPE_ATYPE(cd, ttype, i);
            aval = RS_TVAL_AVAL(cd, ttype, tval, i);

            rs_aval_resetexternalflatva(cd, atype, aval);
        }
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *              rs_tval_setvaref_unlink
 *
 * Same as rs_tval_setvaref but unlinks possible va reference. This does
 * the same calls as rs_tval_linktovtpl so v-tuple can be linked using
 * function rs_tval_linktovtpl_nounlink if exactly same v-attributes are
 * set to tval using this function.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      phys_attr_n - in
 *              physical attibute number in tuple
 *
 *      va - in, hold
 *              the new value for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_setvaref_unlink(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va
) {
        ss_dprintf_3(("%s: rs_tval_setvaref_unlink\n", __FILE__));
        CHECK_TVAL(tval);
        {
            rs_atype_t* atype;
            rs_aval_t*  tval_aval;

            ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);

            atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);
            tval_aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);

            rs_aval_unlinkvaref(cd, atype, tval_aval);

            rs_aval_setvaref(cd, atype, tval_aval, va);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_sql_setvaref
 *
 * Sets the value of an attribute of tuple value object from a v-attribute.
 * It maps the attribute number from SQL point of view
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      sql_attr_n - in
 *              SQL attibute number in tuple
 *
 *      va - in, use
 *              the new value for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_tval_sql_setvaref(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n,
        va_t*       va
) {
        rs_ano_t phys_attr_n;

        ss_dprintf_3(("%s: rs_tval_setvaref\n", __FILE__));
        CHECK_TVAL(tval);
        phys_attr_n = RS_TTYPE_QUICKSQLANOTOPHYS(cd, ttype, sql_attr_n);
        rs_tval_setvaref(cd, ttype, tval, phys_attr_n, va);
}

/*##**********************************************************************\
 *
 *              rs_tval_va
 *
 * Returns reference to va associated with certain ano
 * of tval
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple tyoe
 *
 *      tval - in, use
 *              tval object
 *
 *      phys_attr_n - in
 *              attribute number (from 0)
 *
 * Return value - ref :
 *      pointer to va
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
va_t* rs_tval_va(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n
) {
        rs_atype_t* atype;
        rs_aval_t* aval;
        va_t* va;

        ss_dprintf_3(("%s: rs_tval_va\n", __FILE__));
        CHECK_TVAL(tval);
        ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);
        atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);
        aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);
        va = rs_aval_va(cd, atype, aval);
        return (va);
}

/*##**********************************************************************\
 *
 *              rs_tval_sql_va
 *
 * Same as rs_tval_va() above but this one uses SQL anos
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ttype -
 *
 *
 *      tval -
 *
 *
 *      sql_attr_n -
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
va_t* rs_tval_sql_va(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n
) {
        ss_debug(
            if (tval == NULL || tval->tv_check != RSCHK_TUPLEVALUE) {
                ss_dprintf_1(("rs_tval_sql_aval:tval = %08lx\n", (long)tval));
            }
        );
        CHECK_TVAL(tval);
        {
            rs_ano_t phys_ano = RS_TTYPE_QUICKSQLANOTOPHYS(cd, ttype, sql_attr_n);
            return(rs_tval_va(cd, ttype, tval, phys_ano));
        }
}



/*##**********************************************************************\
 *
 *              rs_tval_removevtplref
 *
 * Removes vtuple reference from tval
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      ttype - in, use
 *
 *
 *      tval - use
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
void rs_tval_removevtplref(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval)
{
        uint attr_cnt;
        uint i;

        CHECK_TVAL(tval);
        attr_cnt = RS_TTYPE_NATTRS(cd, ttype);

        for (i = 0; i < attr_cnt; i++) {
            if (SU_BFLAG_TEST(tval->tv_attr_arr[i].ra_flags, RA_VTPLREF)) {
                rs_atype_t* atype;
                rs_aval_t*  tval_aval;

                ss_dassert(!SU_BFLAG_TEST(tval->tv_attr_arr[i].ra_flags, RA_NULL));
                atype = RS_TTYPE_ATYPE(cd, ttype, i);
                tval_aval = RS_TVAL_AVAL(cd, ttype, tval, i);
                rs_aval_removevtplref(cd, atype, tval_aval);
            }
        }
        if (tval->tv_vtpl != NULL) {
            rs_tval_unlinkfromvtpl(cd, ttype, tval);
        }

}

/*##**********************************************************************\
 *
 *              rs_tval_givevtpl
 *
 * Generates a v-tuple from tuple attributes.
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      ttype - in, use
 *
 *
 *      tval - in, use
 *
 *
 * Return value - give:
 *
 *      A dynamic v-tuple of tuple attributes. Should be returned using
 *      function dynvtpl_free.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dynvtpl_t rs_tval_givevtpl(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        rs_ano_t i;
        rs_ano_t nattrs;
        dynvtpl_t dvtpl = NULL;

        ss_dprintf_1(("rs_tval_givevtpl\n"));

        dynvtpl_setvtpl(&dvtpl, VTPL_EMPTY);
        if (ttype == NULL) {
            ss_dassert(tval == NULL);
            return (dvtpl);
        }
        nattrs = rs_ttype_nattrs(cd, ttype);

        for (i = 0; i < nattrs; i++) {
            va_t* va;

            va = rs_tval_va(cd, ttype, tval, i);
            ss_dprintf_2(("rs_tval_givevtpl:i=%d, va len=%d\n", i, va_grosslen(va)));
            dynvtpl_appva(&dvtpl, va);
        }
        ss_dprintf_2(("rs_tval_givevtpl:vtpl len=%d\n", vtpl_grosslen(dvtpl)));
        return(dvtpl);
}

#ifdef SS_SYNC

/*##**********************************************************************\
 *
 *              rs_tval_initfromvtpl
 *
 * Creates a tval from v-tuple
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      ttype - in, use
 *
 *
 *      vtpl - in, use
 *
 *
 * Return value - give:
 *      new tval object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_tval_t* rs_tval_initfromvtpl(
        void* cd,
        rs_ttype_t* ttype,
        vtpl_t* p_vtpl)
{
        ss_debug(ss_byte_t* vtpl_endmark;)
        rs_ano_t i;
        rs_ano_t nattrs;
        va_t* p_va;
        rs_tval_t* tval;

        ss_dprintf_1(("rs_tval_initfromvtpl\n"));
        ss_dassert(ttype != NULL);
        ss_dassert(p_vtpl != NULL);

        tval = rs_tval_create(cd, ttype);
        nattrs = rs_ttype_nattrs(cd, ttype);
        ss_debug(vtpl_endmark = (ss_byte_t*)p_vtpl + vtpl_grosslen(p_vtpl));
        ss_dassert((char*)p_vtpl < (char*)vtpl_endmark);
        p_va = vtpl_getva_at(p_vtpl, 0);
        for (i = 0; i < nattrs; i++) {
            ss_dprintf_2(("rs_tval_initfromvtpl:attr=%d\n", i));
            ss_dassert((ss_byte_t*)p_va < vtpl_endmark);
            rs_tval_setva(cd, ttype, tval, i, p_va);
            ss_dprintf_2(("rs_tval_initfromvtpl:vtpl_skipva\n"));
            p_va = vtpl_skipva(p_va);
        }
        return (tval);
}
#endif /* SS_SYNC */

#ifndef SS_MYSQL

/*##**********************************************************************\
 *
 *              rs_tval_uni2charif
 *
 * Converts UNICODE columns from tval to corresponding CHAR
 * columns to emulate old non-unicode server to old clients
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in, use
 *              tuple value
 *
 *      p_new_ttype - out, give/ref
 *              if any conversion was made, a newly allocated
 *          ttype object or just original ttype if no changes
 *          were done.
 *
 *      p_new_tval - out, give/ref
 *              if any conversion was made a newly allocated converted
 *          tval object ot just original tval if no changes were done.
 *
 *
 *      p_errh - out, give
 *          in case of error and p_errh != NULL a newly allocated error
 *          handle will be given
 *
 *
 * Return value :
 *      TRUE - conversion successful or no conversion
 *      FALSE - conversion failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_tval_uni2charif(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        rs_ttype_t** p_new_ttype,
        rs_tval_t** p_new_tval,
        rs_err_t** p_errh)
{
        bool need_conversion;
        bool succp;
        rs_ano_t first_unicode_ano;
        rs_ano_t ano;
        rs_ano_t nattrs;
        rs_ttype_t* new_ttype;
        rs_tval_t* new_tval;
        rs_atype_t* atype;
        rs_aval_t* aval;
        rs_atype_t* new_atype;
        rs_aval_t* new_aval;
        RS_AVALRET_T avalrc;
        va_t* va;

        need_conversion = FALSE;
        first_unicode_ano = RS_ANO_NULL;
        nattrs = RS_TTYPE_NATTRS(cd, ttype);
        for (ano = 0; ano < nattrs; ano++) {
            atype = RS_TTYPE_ATYPE(cd, ttype, ano);
            if (RS_ATYPE_DATATYPE(cd, atype) == RSDT_UNICODE) {
                if (first_unicode_ano == RS_ANO_NULL) {
                    first_unicode_ano = ano;
                }
                aval = RS_TVAL_AVAL(cd, ttype, tval, ano);
                if (!RS_AVAL_ISNULL(cd, atype, aval)) {
                    va = rs_aval_va(cd, atype, aval);
                    if (va_netlen(va) > 1) {
                        need_conversion = TRUE;
                        break;
                    } /* else only terminating '\0', ie. empty string */
                }
            }
        }
        if (!need_conversion) {
            return (TRUE);
        }
        *p_new_ttype = new_ttype = rs_ttype_copy(cd, ttype);
        for (ano = first_unicode_ano; ano < nattrs; ano++) {
            atype = RS_TTYPE_ATYPE(cd, ttype, ano);
            if (RS_ATYPE_DATATYPE(cd, atype) == RSDT_UNICODE) {
                new_atype = rs_atype_unitochar(cd, atype);
                rs_ttype_setatype(cd, new_ttype, ano, new_atype);
                rs_atype_free(cd, new_atype);
            }
        }
        *p_new_tval = new_tval = rs_tval_create(cd, new_ttype);
        succp = TRUE;
        for (ano = 0; ano < nattrs; ano++) {
            atype = RS_TTYPE_ATYPE(cd, ttype, ano);
            aval = RS_TVAL_AVAL(cd, ttype, tval, ano);
            if (RS_ATYPE_DATATYPE(cd, atype) == RSDT_UNICODE) {
                if (!RS_AVAL_ISNULL(cd, atype, aval)) {
                    new_atype = rs_ttype_atype(cd, new_ttype, ano);
                    new_aval = rs_tval_aval(cd, new_ttype, new_tval, ano);
                    avalrc = rs_aval_convert_ext(
                                cd,
                                new_atype, new_aval,
                                atype, aval,
                                p_errh);
                    if (avalrc == RSAVR_FAILURE) {
                        rs_atype_t* atypearr[2];
                        rs_aval_t* avalarr[2];
                        rs_atype_t* res_atype = NULL;
                        rs_aval_t* res_aval = NULL;

                        p_errh = NULL;
                        avalarr[0] = aval;
                        avalarr[1] = NULL;
                        atypearr[0] = atype;
                        atypearr[1] = NULL;
                        succp = rs_aval_callfun(
                                    cd,
                                    "CONVERT_TOUTF8",
                                    NULL,
                                    atypearr,
                                    avalarr,
                                    &res_atype,
                                    &res_aval,
                                    NULL,
                                    NULL);
                        if (!succp) {
                            /* probable reason: aval is BLOB */
                            rs_aval_setnull(cd, new_atype, new_aval);
                            if (res_aval != NULL) {
                                rs_aval_free(cd, res_atype, res_aval);
                            }
                            if (res_atype != NULL) {
                                rs_atype_free(cd, res_atype);
                            }
                        } else {
                            avalrc = rs_aval_assign_ext(
                                        cd,
                                        new_atype,
                                        new_aval,
                                        res_atype,
                                        res_aval,
                                        NULL);
                            ss_rc_dassert(avalrc == RSAVR_SUCCESS, avalrc);
                            rs_aval_free(cd, res_atype, res_aval);
                            rs_atype_free(cd, res_atype);
                        }
                        succp = FALSE;
                    }
                    ss_rc_dassert(avalrc == RSAVR_SUCCESS, avalrc);
                }
            } else {
                rs_tval_setaval(cd, new_ttype, new_tval, ano, aval);
            }
        }
        return (succp);
}

/*##**********************************************************************\
 *
 *              rs_tval_char2uniif
 *
 * Converts UNICODE columns from tval. The columns have
 * values that are actually CHAR because they are from old
 * non-unicode-capable client.
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - use
 *              tuple value
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_tval_char2uniif(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        rs_atype_t* atype;
        rs_aval_t* aval;
        rs_ano_t ano;
        rs_ano_t nattrs;

        nattrs = RS_TTYPE_NATTRS(cd, ttype);
        for (ano = 0; ano < nattrs; ano++) {
            atype = RS_TTYPE_ATYPE(cd, ttype, ano);
            if (RS_ATYPE_DATATYPE(cd, atype) == RSDT_UNICODE) {
                aval = RS_TVAL_AVAL(cd, ttype, tval, ano);
                if (!RS_AVAL_ISNULL(cd, atype, aval)) {
                    refdva_t rdva;
                    void* data;
                    va_index_t datalen;
                    va_t* va = rs_aval_va(cd, atype, aval);
                    data = va_getdata(va, &datalen);
                    if (datalen > 1) {
                        rdva = refdva_init();
                        refdva_setdatachar1to2(&rdva, data, datalen - 1);
                        rs_aval_insertrefdva(cd, atype, aval, rdva);
                    }
                }
            }
        }
}

#endif /* !SS_MYSQL */

uint rs_tval_nullifyblobids(
        rs_sysi_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* orig_tval_in,
        rs_tval_t** tval_in_out)
{
        uint n;
        rs_ano_t i;
        rs_tval_t* tval;

        if (*tval_in_out == NULL) {
            *tval_in_out = tval = orig_tval_in;
        } else {
            tval = *tval_in_out;
        }
        for (i = 0, n = 0; i < tval->tv_nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval = &tval->tv_attr_arr[i];
            if (SU_BFLAG_TEST(aval->ra_flags, RA_BLOB)) {
                n++;
                if (tval == orig_tval_in) {
                    *tval_in_out = tval = rs_tval_copy(cd, ttype, tval);
                    aval = &tval->tv_attr_arr[i];
                }
                atype = rs_ttype_atype(cd, ttype, i);
                rs_aval_nullifyblobid(cd, atype, aval);
            }
        }
        return (n);
}

/*##**********************************************************************\
 *
 *              rs_tval_setrowflags
 *
 * Sets tval rowflags. Note that new aflag are appended to old flags so
 * possible previous flags remain in the row.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              ttype -
 *
 *
 *              tval -
 *
 *
 *              flags -
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
void rs_tval_setrowflags(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_ttype_t* ttype __attribute__ ((unused)),
        rs_tval_t* tval,
        rs_aval_rowflag_t flags)
{
        CHECK_TVAL(tval);

        tval->tv_rowflags |= flags;
}

/*##**********************************************************************\
 *
 *              rs_tval_clearallrowflags
 *
 * Clears all row flags in a tval.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              ttype -
 *
 *
 *              tval -
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
void rs_tval_clearallrowflags(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_ttype_t* ttype __attribute__ ((unused)),
        rs_tval_t* tval)
{
        CHECK_TVAL(tval);

        tval->tv_rowflags = 0;
}

/*##**********************************************************************\
 *
 *              rs_tval_getrowflags
 *
 * Returns tval row flags.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              ttype -
 *
 *
 *              tval -
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
rs_aval_rowflag_t rs_tval_getrowflags(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_ttype_t* ttype __attribute__ ((unused)),
        rs_tval_t* tval)
{
        CHECK_TVAL(tval);

        return(tval->tv_rowflags);
}

#if defined(SS_DEBUG) || defined(SS_BETA) || TRUE
/*##**********************************************************************\
 *
 *              rs_tval_print
 *
 * Prints tval content using function SsDbgPrintf.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ttype -
 *
 *
 *      tval -
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
void rs_tval_print(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        rs_ano_t i;
        rs_ano_t nattrs;

        SsDbgPrintf("rs_tval_print:\n");

        nattrs = rs_ttype_nattrs(cd, ttype);

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;
            char* aname;
            char* p;

            atype = rs_ttype_atype(cd, ttype, i);
            aval = rs_tval_aval(cd, ttype, tval, i);
            p = rs_aval_print(cd, atype, aval);
            aname = rs_ttype_aname(cd, ttype, i);

            SsDbgPrintf("  %25s: %.1024s\n", aname, p);

            SsMemFree(p);
        }
}

/*##**********************************************************************\
 *
 *              rs_tval_printtostring
 *
 * Prints columns to a string.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ttype -
 *
 *
 *      tval -
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
char* rs_tval_printtostring(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        rs_ano_t i;
        rs_ano_t nattrs;
        dstr_t dstr = NULL;

        nattrs = rs_ttype_nattrs(cd, ttype);

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;
            char* p;

            atype = rs_ttype_atype(cd, ttype, i);
#ifdef SS_MME
            if (i < rs_tval_nattrs(cd, ttype, tval)) {
#endif
                aval = rs_tval_aval(cd, ttype, tval, i);
                if (rs_atype_attrtype(cd, atype) == RSAT_REMOVED) {
                    dstr_app(&dstr, (char *)"*REMOVED* ");
                } else if (rs_aval_isnull(cd, atype, aval)) {
                    dstr_app(&dstr, (char *)"'NULL' ");
                } else {
                    p = rs_aval_print(cd, atype, aval);

                    dstr_app(&dstr, (char *)"'");
                    dstr_app(&dstr, p);
                    dstr_app(&dstr, (char *)"' ");

                    SsMemFree(p);
                }
#ifdef SS_MME
            } else {
                dstr_app(&dstr, (char *)"*NONEXISTENT* ");
            }
#endif
        }
        return(dstr);
}

#endif /* defined(SS_DEBUG) || defined(SS_BETA) */

/*##**********************************************************************\
 *
 *              rs_tval_printtostring_likesolsql
 *
 * Prints columns to a string using similar formatting as solsql.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ttype -
 *
 *
 *      tval -
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
char* rs_tval_printtostring_likesolsql(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        rs_ano_t i;
        rs_ano_t nattrs;
        dstr_t dstr = NULL;

        nattrs = rs_ttype_nattrs(cd, ttype);

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            rs_aval_t* aval;
            char* p;

            atype = rs_ttype_atype(cd, ttype, i);
#ifdef SS_MME
            if (i < rs_tval_nattrs(cd, ttype, tval)) {
#endif
                aval = rs_tval_aval(cd, ttype, tval, i);
                if (rs_atype_attrtype(cd, atype) == RSAT_REMOVED) {
                    dstr_app(&dstr, (char *)"*REMOVED*         ");
                } else if (rs_aval_isnull(cd, atype, aval)) {
                    dstr_app(&dstr, (char *)"'NULL             ");
                } else {
                    int len;
                    int plen;
                    p = rs_aval_print(cd, atype, aval);
                    dstr_app(&dstr, p);
                    switch (rs_atype_sqldatatype(cd, atype)) {
                        case RSSQLDT_INTEGER     :
                            len = 9;
                            break;
                        case RSSQLDT_SMALLINT    :
                            len = 6;
                            break;
                        case RSSQLDT_TINYINT     :
                            len = 2;
                            break;
                        case RSSQLDT_REAL        :
                            len = 12;
                            break;
                        case RSSQLDT_DOUBLE      :
                        case RSSQLDT_FLOAT       :
                            len = 14;
                            break;
                        case RSSQLDT_BIGINT      :
                            len = 20;
                            break;
                        case RSSQLDT_WLONGVARCHAR:
                        case RSSQLDT_WVARCHAR    :
                        case RSSQLDT_WCHAR       :
                        case RSSQLDT_LONGVARCHAR :
                        case RSSQLDT_VARCHAR     :
                        case RSSQLDT_CHAR        :
                            len = 18;
                            break;
                        case RSSQLDT_DATE        :
                            len = 10;
                            break;
                        case RSSQLDT_TIME        :
                            len = 8;
                            break;
                        case RSSQLDT_TIMESTAMP   :
                            len = 10;
                            break;
                        case RSSQLDT_NUMERIC     :
                        case RSSQLDT_DECIMAL     :
                            len = 14;
                            break;
                        default:
                            len = 14;
                            break;
                    }
                    plen = strlen(p);
                    while (plen < len) {
                        dstr_app(&dstr, (char *)" ");
                        plen++;
                    }
                    dstr_app(&dstr, (char *)" ");
                    SsMemFree(p);
                }
#ifdef SS_MME
            } else {
                dstr_app(&dstr, (char *)"*NONEXISTENT*     ");
            }
#endif
        }
        return(dstr);
}

/* XXX - THIS IS A TEMPORARY FUNCTION. */
int rs_tval_nattrs(
        void*           cd __attribute__ ((unused)),
        rs_ttype_t*     ttype __attribute__ ((unused)),
        rs_tval_t*      tval)
{
        CHECK_TVAL(tval);

#ifndef SS_MME
        ss_error;
#endif

        return tval->tv_nattrs;
}
