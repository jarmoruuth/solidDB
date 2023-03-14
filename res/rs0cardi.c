/*************************************************************************\
**  source       * rs0cardi.c
**  directory    * res
**  description  * Relation cardinality object. Shared with rs_relh_t and
**               * rs_rbuf_t.
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

#define RS0CARDI_C

#include <ssenv.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sslimits.h>
#include <ssint8.h>

#include "rs0sysi.h"
#include "rs0cardi.h"

/*##**********************************************************************\
 *
 *              rs_cardin_init
 *
 * Init a cardin object.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 * Return value - give :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_cardin_t* rs_cardin_init(
        void* cd,
        ulong relid)
{
        rs_cardin_t* cr;

        SS_NOTUSED(cd);

        cr = SSMEM_NEW(rs_cardin_t);

        ss_dprintf_1(("rs_cardin_init:ptr=%ld:%ld\n", (long)cr, relid));

        cr->cr_nlink = 1;
        cr->cr_ischanged = FALSE;
        cr->cr_nsubscribers = 0;
        SsInt8Set0(&(cr->cr_ntuples));
        SsInt8Set0(&(cr->cr_nbytes));
        cr->cr_relid = relid;
        cr->cr_sem = rs_sysi_getrslinksem(cd);
        ss_debug(cr->cr_chk = RSCHK_CARDIN);
        ss_debug(cr->cr_nocheck = FALSE);

        return(cr);
}

/*##**********************************************************************\
 *
 *              rs_cardin_done
 *
 * Delete a cardin object.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cr - in, take
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
void rs_cardin_done(
        void* cd __attribute__ ((unused)),
        rs_cardin_t* cr)
{
        CHK_CARDIN(cr);
        ss_dprintf_1(("rs_cardin_done:ptr=%ld:%ld:ntuples=%ld, nbytes=%ld, nsubscribers=%d\n", 
                       (long)cr, cr->cr_relid, SsInt8GetLeastSignificantUint4(cr->cr_ntuples), 
                       SsInt8GetLeastSignificantUint4(cr->cr_nbytes), cr->cr_nsubscribers));

        SsSemEnter(cr->cr_sem);

        ss_dassert(cr->cr_nlink > 0);

        cr->cr_nlink--;

        if (cr->cr_nlink == 0) {
            ss_dprintf_2(("rs_cardin_done:%ld:free mem\n", cr->cr_relid));
            SsSemExit(cr->cr_sem);
            SsMemFree(cr);
        } else {
            SsSemExit(cr->cr_sem);
        }
}

/*##**********************************************************************\
 *
 *              rs_cardin_link
 *
 * Adds a new usage link to the cardin object.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cr - use
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
void rs_cardin_link(
        void* cd __attribute__ ((unused)),
        rs_cardin_t* cr)
{
        CHK_CARDIN(cr);
        ss_dprintf_1(("rs_cardin_link:%ld:ntuples=%ld, nbytes=%ld\n", 
            cr->cr_relid, SsInt8GetLeastSignificantUint4(cr->cr_ntuples), 
            SsInt8GetLeastSignificantUint4(cr->cr_nbytes)));

        SsSemEnter(cr->cr_sem);

        ss_dassert(cr->cr_nlink > 0);

        cr->cr_nlink++;

        SsSemExit(cr->cr_sem);
}

#ifdef SS_DEBUG

void rs_cardin_setcheck(
        rs_cardin_t* cr,
        rs_entname_t* en)
{
        ss_dprintf_1(("rs_cardin_setcheck:ptr=%ld:%ld\n", (long)cr, cr->cr_relid));
        CHK_CARDIN(cr);

        if (strcmp(rs_entname_getschema(en), RS_AVAL_SYSNAME) == 0
            && (strcmp(rs_entname_getname(en), RS_RELNAME_SYSPROPERTIES) == 0
                || strcmp(rs_entname_getname(en), RS_RELNAME_CARDINAL) == 0))
        {
            ss_dprintf_2(("rs_cardin_setcheck:ptr=%ld:%ld:cr->cr_nocheck = TRUE\n", (long)cr, cr->cr_relid));
            cr->cr_nocheck = TRUE;
        }
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *              rs_cardin_ischanged
 *
 * Checks if cardinality has changed. This is used to decide if the
 * cardinality should be saved in checkpoint or shutdown.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cr - in
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
bool rs_cardin_ischanged(
        void* cd,
        rs_cardin_t* cr)
{
        ss_dprintf_1(("rs_cardin_ischanged:ptr=%ld:%ld:%d\n", (long)cr, cr->cr_relid, cr->cr_ischanged));
        SS_NOTUSED(cd);
        CHK_CARDIN(cr);

        return(cr->cr_ischanged);
}

/*##**********************************************************************\
 *
 *              rs_cardin_clearchanged
 *
 * Marks cardinality as ont changed. This function is used after cardinality
 * has been saved to the database.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cr - use
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
void rs_cardin_clearchanged(
        void* cd,
        rs_cardin_t* cr)
{
        ss_dprintf_1(("rs_cardin_clearchanged:ptr=%ld:%ld\n", (long)cr, cr->cr_relid));
        SS_NOTUSED(cd);
        CHK_CARDIN(cr);

        cr->cr_ischanged = FALSE;
}

/*##**********************************************************************\
 *
 *              rs_cardin_setchanged
 *
 * Marks cardinality as changed.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cr - use
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
void rs_cardin_setchanged(
        void* cd,
        rs_cardin_t* cr)
{
        ss_dprintf_1(("rs_cardin_setchanged:ptr=%ld:%ld\n", (long)cr, cr->cr_relid));
        SS_NOTUSED(cd);
        CHK_CARDIN(cr);

        cr->cr_ischanged = TRUE;
}

/*#***********************************************************************\
 *
 *              cardin_fixvalues
 *
 *
 *
 * Parameters :
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
static void cardin_fixvalues(rs_cardin_t* cr)
{
        if (cr->cr_nsubscribers < 0) {
            ss_derror;
            cr->cr_nsubscribers = 0;
        }

        if (SsInt8IsNegative(cr->cr_ntuples)) {
#ifndef AUTOTEST_RUN
            ss_dassert(ss_debug_nocardinalcheck || cr->cr_nocheck);
#endif
            cr->cr_ischanged = TRUE;
            SsInt8Set0(&(cr->cr_ntuples));
            SsInt8Set0(&(cr->cr_nbytes));
        }
        if (SsInt8IsNegative(cr->cr_nbytes)) {
#ifndef AUTOTEST_RUN
            ss_dassert(ss_debug_nocardinalcheck || cr->cr_nocheck);
#endif
            cr->cr_ischanged = TRUE;
            SsInt8Set0(&(cr->cr_nbytes));
        }
        if (SsInt8UnsignedCmp(cr->cr_nbytes, cr->cr_ntuples) < 0) {
            ss_int8_t new_nbytes;
#ifndef AUTOTEST_RUN
            ss_dassert(ss_debug_nocardinalcheck || cr->cr_nocheck);
#endif
            cr->cr_ischanged = TRUE;
            SsInt8MultiplyByInt2(&new_nbytes, cr->cr_ntuples, (ss_int2_t)5);
#if 0
/*  Higly unlikely that we overflow with int8 */

            if (new_nbytes > (ss_uint4_t)SS_INT4_MAX) {
                /* Overflow */
                cr->cr_nbytes = SS_INT4_MAX;
            } else {
                cr->cr_nbytes = new_nbytes;
            }
#endif
        }
}

/*##**********************************************************************\
 *
 *              rs_cardin_setdata
 *
 * Sets cardinality data. Usually data is read from the system relation,
 * when the relation is references for the first time.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cr - use
 *
 *
 *      ntuples - in
 *
 *
 *      nbytes - in
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
void rs_cardin_setdata(
        void* cd __attribute__ ((unused)),
        rs_cardin_t* cr,
        ss_int8_t ntuples,
        ss_int8_t nbytes)
{
        ss_dprintf_1(("rs_cardin_setdata:%ld:ntuples=%ld, nbytes=%ld\n", cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(ntuples), SsInt8GetLeastSignificantUint4(nbytes)));

#ifndef AUTOTEST_RUN
        ss_dassert(ss_debug_nocardinalcheck || !SsInt8IsNegative(ntuples));
        ss_dassert(ss_debug_nocardinalcheck || (!SsInt8IsNegative(nbytes) && !SsInt8Is0(nbytes)) || (SsInt8Is0(nbytes) && SsInt8Is0(ntuples)));
#endif
        CHK_CARDIN(cr);

        SsSemEnter(cr->cr_sem);

        cr->cr_ntuples = ntuples;
        cr->cr_nbytes = nbytes;

        if (SsInt8IsNegative(cr->cr_ntuples) || SsInt8IsNegative(cr->cr_nbytes)) {
            cardin_fixvalues(cr);
        }

        SsSemExit(cr->cr_sem);
}

/*##**********************************************************************\
 *
 *              rs_cardin_replace
 *
 * Replaces cardin information in target_cr with data from source_cr.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      target_cr - use
 *              Target cardin info, replaced from source_cr.
 *
 *      source_cr - in
 *              Source cardin info.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_cardin_replace(
        void* cd __attribute__ ((unused)),
        rs_cardin_t* target_cr,
        rs_cardin_t* source_cr)
{
        ss_dprintf_1(("rs_cardin_replace:target ptr=%ld, source ptr=%ld:%ld:before ntuples=%ld, nbytes=%ld\n", 
            (long)target_cr, (long)source_cr, target_cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(target_cr->cr_ntuples), 
            SsInt8GetLeastSignificantUint4(target_cr->cr_nbytes)));
        CHK_CARDIN(target_cr);
        CHK_CARDIN(source_cr);

        SsSemEnter(target_cr->cr_sem);

        target_cr->cr_ntuples = source_cr->cr_ntuples;
        target_cr->cr_nbytes = source_cr->cr_nbytes;
        target_cr->cr_ischanged = source_cr->cr_ischanged;
        target_cr->cr_nsubscribers = source_cr->cr_nsubscribers;

        ss_dprintf_2(("rs_cardin_replace:%ld:after ntuples=%ld, nbytes=%ld\n", target_cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(target_cr->cr_ntuples), 
            SsInt8GetLeastSignificantUint4(target_cr->cr_nbytes)));

        SsSemExit(target_cr->cr_sem);
}

/*##**********************************************************************\
 *
 *              rs_cardin_updatedata
 *
 * Updates cardinality data.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cr - use
 *
 *
 *      ntuples - in
 *
 *
 *      nbytes - in
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
void rs_cardin_updatedata(
        void* cd __attribute__ ((unused)),
        rs_cardin_t* cr,
        long ntuples,
        long nbytes)
{
        ss_int8_t dummy;

        ss_dprintf_1(("rs_cardin_updatedata:%ld:ntuples=%ld, nbytes=%ld\n", cr->cr_relid, ntuples, nbytes));
        CHK_CARDIN(cr);

        SsSemEnter(cr->cr_sem);

        cr->cr_ischanged = TRUE;

        SsInt8SetInt4(&dummy, ntuples);
        SsInt8AddInt8(&cr->cr_ntuples, cr->cr_ntuples, dummy);

        SsInt8SetInt4(&dummy, nbytes);
        SsInt8AddInt8(&cr->cr_nbytes, cr->cr_nbytes, dummy);

        if (SsInt8IsNegative(cr->cr_ntuples) || SsInt8IsNegative(cr->cr_nbytes)) {
            cardin_fixvalues(cr);
        }
#ifndef AUTOTEST_RUN
        ss_dassert(ss_debug_nocardinalcheck || !SsInt8IsNegative(cr->cr_ntuples));
        ss_dassert(ss_debug_nocardinalcheck || (!SsInt8IsNegative(cr->cr_nbytes) && !SsInt8Is0(cr->cr_nbytes)) || (SsInt8Is0(cr->cr_nbytes) && SsInt8Is0(cr->cr_ntuples)));
#endif
        ss_dprintf_2(("rs_cardin_updatedata:%ld:ntuples=%ld, nbytes=%ld\n", cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(cr->cr_ntuples), 
            SsInt8GetLeastSignificantUint4(cr->cr_nbytes)));

        SsSemExit(cr->cr_sem);
}

/*##**********************************************************************\
 *
 *              rs_cardin_getdata
 *
 * Returns cardinality information.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cr - in
 *
 *
 *      p_ntuples - out
 *
 *
 *      p_nbytes - out
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
void rs_cardin_getdata(
        void* cd,
        rs_cardin_t* cr,
        ss_int8_t* p_ntuples,
        ss_int8_t* p_nbytes)
{
        ss_dprintf_1(("rs_cardin_getdata:ptr=%ld:%ld:ntuples=%ld, nbytes=%ld\n", (long)cr, cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(cr->cr_ntuples), 
            SsInt8GetLeastSignificantUint4(cr->cr_nbytes)));
        SS_NOTUSED(cd);
        CHK_CARDIN(cr);

        *p_ntuples = cr->cr_ntuples;
        *p_nbytes = cr->cr_nbytes;
}

/*##**********************************************************************\
 *
 *              rs_cardin_insertbytes
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
 *      nbytes -
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
void rs_cardin_insertbytes(
        void* cd __attribute__ ((unused)),
        rs_cardin_t* cr,
        ulong nbytes)
{
        ss_dprintf_1(("rs_cardin_insertbytes:ptr=%ld:%ld:nbytes=%ld\n", (long)cr, cr->cr_relid, nbytes));
        CHK_CARDIN(cr);

        SsSemEnter(cr->cr_sem);

        cr->cr_ischanged = TRUE;
        SsInt8AddUint4(&cr->cr_ntuples, cr->cr_ntuples, 1);
        SsInt8AddUint4(&cr->cr_nbytes, cr->cr_nbytes, nbytes);

#ifndef AUTOTEST_RUN
        ss_dassert(ss_debug_nocardinalcheck || !SsInt8IsNegative(cr->cr_ntuples));
        ss_dassert(ss_debug_nocardinalcheck || (!SsInt8IsNegative(cr->cr_nbytes) && !SsInt8Is0(cr->cr_nbytes)) || (SsInt8Is0(cr->cr_nbytes) && SsInt8Is0(cr->cr_ntuples)));
#endif
        ss_dprintf_2(("rs_cardin_insertbytes:%ld:ntuples=%ld, nbytes=%ld\n", cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(cr->cr_ntuples), 
            SsInt8GetLeastSignificantUint4(cr->cr_nbytes)));

        SsSemExit(cr->cr_sem);
}

/*##**********************************************************************\
 *
 *              rs_cardin_deletebytes
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
 *      nbytes -
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
void rs_cardin_deletebytes(
        void* cd __attribute__ ((unused)),
        rs_cardin_t* cr,
        ulong nbytes)
{
        ss_int8_t dummy;

        ss_dprintf_1(("rs_cardin_deletebytes:ptr=%ld:%ld:nbytes=%ld\n", (long)cr, cr->cr_relid, nbytes));
        CHK_CARDIN(cr);

        SsSemEnter(cr->cr_sem);

        cr->cr_ischanged = TRUE;

        SsInt8SetUint4(&dummy, 1);
        SsInt8SubtractInt8(&cr->cr_ntuples, cr->cr_ntuples, dummy);

        SsInt8SetUint4(&dummy, nbytes);
        SsInt8SubtractInt8(&cr->cr_nbytes, cr->cr_nbytes, dummy);

#if !defined(AUTOTEST_RUN) && defined(SS_DEBUG)
        if (!cr->cr_nocheck) {
            ss_dassert(ss_debug_nocardinalcheck || !SsInt8IsNegative(cr->cr_ntuples));
            ss_dassert(ss_debug_nocardinalcheck || (!SsInt8IsNegative(cr->cr_nbytes) && !SsInt8Is0(cr->cr_nbytes)) || (SsInt8Is0(cr->cr_nbytes) && SsInt8Is0(cr->cr_ntuples)));
        }
#endif

        if (SsInt8IsNegative(cr->cr_ntuples) || SsInt8IsNegative(cr->cr_nbytes)) {
            /* OLD DATABASES MAY CONTAIN INVALID INFORMATION.
             */
            cardin_fixvalues(cr);
        }

        ss_dprintf_2(("rs_cardin_deletebytes:%ld:ntuples=%ld, nbytes=%ld\n", cr->cr_relid, 
            SsInt8GetLeastSignificantUint4(cr->cr_ntuples), 
            SsInt8GetLeastSignificantUint4(cr->cr_nbytes)));

        SsSemExit(cr->cr_sem);
}

/*##**********************************************************************\
 *
 *      rs_cardin_applydelta
 *
 * Applies the given delta directly to the cardinality.  The delta can be
 * positive or negative.
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      cr - <usage>
 *          <description>
 *
 *      delta_rows - <usage>
 *          <description>
 *
 *      delta_bytes - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void rs_cardin_applydelta(
        void*           cd __attribute__ ((unused)),
        rs_cardin_t*    cr,
        long            delta_rows,
        long            delta_bytes)
{
        ss_int8_t dummy;

        ss_dprintf_1(("rs_cardin_applydelta:ptr=%ld:%ld nrows=%ld nbytes=%ld\n",
                      (long)cr, cr->cr_relid, delta_rows, delta_bytes));
        CHK_CARDIN(cr);

        SsSemEnter(cr->cr_sem);

        cr->cr_ischanged = TRUE;

        SsInt8SetInt4(&dummy, delta_rows);
        SsInt8AddInt8(&cr->cr_ntuples, cr->cr_ntuples, dummy);

        SsInt8SetInt4(&dummy, delta_bytes);
        SsInt8AddInt8(&cr->cr_nbytes, cr->cr_nbytes, dummy);

        if (SsInt8IsNegative(cr->cr_ntuples) || SsInt8IsNegative(cr->cr_nbytes)) {
            /* OLD DATABASES MAY CONTAIN INVALID INFORMATION.
             */
            cardin_fixvalues(cr);
        }
        ss_dprintf_2(("rs_cardin_applydelta:ptr=%ld:%ld:new cardin nrows=%ld nbytes=%ld\n",
                      (long)cr, cr->cr_relid, 
                      SsInt8GetLeastSignificantUint4(cr->cr_ntuples), 
                      SsInt8GetLeastSignificantUint4(cr->cr_nbytes)));

        SsSemExit(cr->cr_sem);
}

/* Use this ONLY FROM INSIDE THE MME SEMAPHORE! */
void rs_cardin_applydelta_nomutex(
        void*           cd __attribute__ ((unused)),
        rs_cardin_t*    cr,
        long            delta_rows,
        long            delta_bytes)
{
        ss_int8_t dummy;

        ss_dprintf_1(("rs_cardin_applydelta_nomutex:ptr=%ld:%ld nrows=%ld nbytes=%ld\n",
                      (long)cr, cr->cr_relid, delta_rows, delta_bytes));
        CHK_CARDIN(cr);

        cr->cr_ischanged = TRUE;

        SsInt8SetInt4(&dummy, delta_rows);
        SsInt8AddInt8(&cr->cr_ntuples, cr->cr_ntuples, dummy);

        SsInt8SetInt4(&dummy, delta_bytes);
        SsInt8AddInt8(&cr->cr_nbytes, cr->cr_nbytes, dummy);

#ifndef AUTOTEST_RUN
        ss_dassert(ss_debug_nocardinalcheck || !SsInt8IsNegative(cr->cr_ntuples));
        ss_dassert(ss_debug_nocardinalcheck || (!SsInt8IsNegative(cr->cr_nbytes) && !SsInt8Is0(cr->cr_nbytes)) || (SsInt8Is0(cr->cr_nbytes) && SsInt8Is0(cr->cr_ntuples)));
#endif

        if (SsInt8IsNegative(cr->cr_ntuples) || SsInt8IsNegative(cr->cr_nbytes)) {
            /* OLD DATABASES MAY CONTAIN INVALID INFORMATION.
             */
            cardin_fixvalues(cr);
        }
        ss_dprintf_2(("rs_cardin_applydelta_nomutex:ptr=%ld:%ld:new cardin nrows=%ld nbytes=%ld\n",
                      (long)cr, cr->cr_relid, 
                      SsInt8GetLeastSignificantUint4(cr->cr_ntuples), 
                      SsInt8GetLeastSignificantUint4(cr->cr_nbytes)));
}

void rs_cardin_addsubscriber(
        void*           cd __attribute__ ((unused)),
        rs_cardin_t*    cr,
        bool            addp)
{
        ss_dprintf_1(("rs_cardin_addsubscriber:nsubscribers=%ld, add %d\n", cr->cr_nsubscribers, addp));
        CHK_CARDIN(cr);

        SsSemEnter(cr->cr_sem);

        if (addp) {
            cr->cr_nsubscribers++;
        } else {
            cr->cr_nsubscribers--;
            if (cr->cr_nsubscribers < 0) {
                cardin_fixvalues(cr);
            }
        }

        SsSemExit(cr->cr_sem);
}


int rs_cardin_nsubscribers(
        void*           cd,
        rs_cardin_t*    cr)
{
        ss_dprintf_1(("rs_cardin_nsubscribers:%ld:nsubscribers=%ld\n", cr->cr_relid, cr->cr_nsubscribers));
        SS_NOTUSED(cd);
        CHK_CARDIN(cr);

        return(cr->cr_nsubscribers);
}


