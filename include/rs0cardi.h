/*************************************************************************\
**  source       * rs0cardi.h
**  directory    * res
**  description  * Relation cardinality object.
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


#ifndef RS0CARDI_H
#define RS0CARDI_H

#include <ssc.h>

#include "rs0types.h"
#include "rs0entna.h"
#include <ssint8.h>

#define CHK_CARDIN(cr)   ss_dassert(SS_CHKPTR(cr) && (cr)->cr_chk == RSCHK_CARDIN)

/* Relation cardinality stucture.
 */
struct rs_cardin_st {
        ss_debug(rs_check_t cr_chk;)
        int         cr_nlink;       /* Number of usage links. */
        bool        cr_ischanged;   /* If TRUE, cardinality has been changed. */
        ss_int8_t   cr_ntuples;     /* Number of tuples. */
        ss_int8_t   cr_nbytes;      /* Number of bytes (approximate). */
        int         cr_nsubscribers; /* Number of subscribers on this 'table' */
        long        cr_relid;       /* Relation id. */
        SsSemT*     cr_sem;
        ss_debug(bool cr_nocheck;)
};

rs_cardin_t* rs_cardin_init(
        void* cd,
        ulong relid);

void rs_cardin_done(
        void* cd,
        rs_cardin_t* cr);

void rs_cardin_link(
        void* cd,
        rs_cardin_t* cr);

#ifdef SS_DEBUG

void rs_cardin_setcheck(
        rs_cardin_t* cr,
        rs_entname_t* en);

#endif /* SS_DEBUG */

bool rs_cardin_ischanged(
        void* cd,
        rs_cardin_t* cr);

void rs_cardin_clearchanged(
        void* cd,
        rs_cardin_t* cr);

void rs_cardin_setchanged(
        void* cd,
        rs_cardin_t* cr);

void rs_cardin_setdata(
        void* cd,
        rs_cardin_t* cr,
        ss_int8_t ntuples,
        ss_int8_t nbytes);

void rs_cardin_replace(
        void* cd,
        rs_cardin_t* target_cr,
        rs_cardin_t* source_cr);

void rs_cardin_updatedata(
        void* cd,
        rs_cardin_t* cr,
        long ntuples,
        long nbytes);

void rs_cardin_getdata(
        void* cd,
        rs_cardin_t* cr,
        ss_int8_t* p_ntuples,
        ss_int8_t* p_nbytes);

SS_INLINE ss_int8_t rs_cardin_ntuples(
        void* cd,
        rs_cardin_t* cr);

SS_INLINE ss_int8_t rs_cardin_nbytes(
        void* cd,
        rs_cardin_t* cr);

void rs_cardin_insertbytes(
        void* cd,
        rs_cardin_t* cr,
        ulong nbytes);

void rs_cardin_deletebytes(
        void* cd,
        rs_cardin_t* cr,
        ulong nbytes);

void rs_cardin_applydelta(
        void*           cd,
        rs_cardin_t*    cr,
        long            delta_rows,
        long            delta_bytes);

void rs_cardin_applydelta_nomutex(
        void*           cd,
        rs_cardin_t*    cr,
        long            delta_rows,
        long            delta_bytes);

void rs_cardin_addsubscriber(
        void*           cd,
        rs_cardin_t*    cr,
        bool            addp);

int rs_cardin_nsubscribers(
        void*           cd,
        rs_cardin_t*    cr);

#if defined(RS0CARDI_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              rs_cardin_ntuples
 *
 * Returns the number of tuples in the realtion.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cr -
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
SS_INLINE ss_int8_t rs_cardin_ntuples(
        void* cd,
        rs_cardin_t* cr)
{
        ss_dprintf_1(("rs_cardin_ntuples:ptr=%ld:%ld:ntuples=%ld\n", (long)cr, cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(cr->cr_ntuples)));
        SS_NOTUSED(cd);
        CHK_CARDIN(cr);

        return(cr->cr_ntuples);
}

/*##**********************************************************************\
 *
 *              rs_cardin_nbytes
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cr -
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
SS_INLINE ss_int8_t rs_cardin_nbytes(
        void* cd,
        rs_cardin_t* cr)
{
        ss_dprintf_1(("rs_cardin_nbytes:%ld:nbytes=%ld\n", cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(cr->cr_nbytes)));
        SS_NOTUSED(cd);
        CHK_CARDIN(cr);

        return(cr->cr_nbytes);
}

#endif /* defined(RS0CARDI_C) || defined(SS_USE_INLINE) */

#endif /* RS0CARDI_H */
