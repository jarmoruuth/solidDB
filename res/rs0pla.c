/*************************************************************************\
**  source       * rs0pla.c
**  directory    * res
**  description  * Query plan, query planner constraint and reference 
**               * building functions.
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
Search plan for db-engine.
Constraint object for query planner and db-engine.
Reference building.

Limitations:
-----------


Error handling:
--------------
None.


Objects used:
------------
v-attributes    <uti0va.h>

Preconditions:
-------------
None.


Multithread considerations:
--------------------------
Code is fully re-entrant.
The same pla_cons object can not be used simultaneously from many threads.


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define RS0PLA_C

#include <ssstdio.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <uti0va.h>
#include <uti0vtpl.h>

#include <su0list.h>

#include "rs0types.h"
#include "rs0sysi.h"
#include "rs0atype.h"
#include "rs0relh.h"
#include "rs0key.h"
#include "rs0pla.h"

#define CHECK_PLACONS(pc) ss_dassert(SS_CHKPTR(pc) && (pc)->pc_check == RSCHK_PLACONS);

rs_pla_t* rs_pla_alloc(
        void*       cd __attribute__ ((unused)))
{
        rs_pla_t* plan;

        plan = SsMemCalloc(sizeof(rs_pla_t), 1);

        ss_beta(plan->pla_check = RSCHK_PLAN);
        su_list_initbuf(&plan->pla_key_constraints_buf, NULL);
        su_list_initbuf(&plan->pla_data_constraints_buf, NULL);
        su_list_initbuf(&plan->pla_tuple_reference_buf, NULL);

        /* These are needed in rs_pla_done if we free before init. */
        plan->pla_nlink = 1;
        SU_BFLAG_SET(plan->pla_flags, PLA_CONSISTENT);

        return(plan);
}

/*##**********************************************************************\
 * 
 *              rs_pla_initbuf
 * 
 * Initializes a search plan.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *              
 *      relh - in, hold
 *              relation handle
 *              
 *      key - in
 *              key handle
 *              
 *      isconsistent - in
 *              FALSE if the search range is empty,
 *          i.e., the query is inconsistent
 *              
 *      range_start - in, take
 *              start of the search range
 *              
 *      start_closed - in
 *              TRUE if the start closed
 *              
 *      range_end - in, take
 *              end of the search range
 *              
 *      end_closed - in
 *              TRUE if the end closed
 *              
 *      key_constraints - in, take
 *              constraints for key entry
 *              
 *      data_constraints - in, take
 *              constraints for data tuple
 *          NULL if no constraints
 *
 *      constraints - in, hold
 *          all constraints of this search, in rs_cons objects.
 *
 *      tuple_reference - in, take
 *              rules for building the data
 *          tuple reference. NULL if no
 *          dereferencing is necessary
 *              
 *      select_list - in, take
 *              key part numbers of the
 *          selected columns
 *              
 *      dereference - in
 *              TRUE if data has to be fetched
 *          by dereferencing to the data
 *          tuple
 *              
 *      nsolved_range_cons - in
 *              number of constraints solved
 *          in the search range constraint
 *              
 *      nsolved_key_cons - in
 *              number of constraints solved
 *          in the index key (with the range
 *          constraints excluded)
 *              
 *      nsolved_data_cons - in
 *              number of constraints solved
 *          on the data tuple
 *
 * Return value - give : 
 * 
 *      Pointer to search plan object.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_initbuf(
        void*       cd,
        rs_pla_t*   plan,
        rs_relh_t*  relh,
        rs_key_t*   key,
        bool        isconsistent,
        dynvtpl_t   range_start,
        bool        start_closed,
        dynvtpl_t   range_end,
        bool        end_closed,
        su_list_t*  key_constraints,
        su_list_t*  data_constraints,
        su_list_t*  constraints,
        su_list_t*  tuple_reference,
        int*        select_list,
        bool        dereference,
        long        nsolved_range_cons,
        long        nsolved_key_cons,
        long        nsolved_data_cons,
        bool        addlinks)
{
        SS_NOTUSED(cd);

        if (addlinks) {
            rs_relh_link(cd, relh);
            SS_MEM_SETLINK(relh);
            rs_key_link(cd, key);
        }

        ss_beta(plan->pla_check = RSCHK_PLAN);
        plan->pla_relh = relh;
        plan->pla_key = key;
        plan->pla_range_start = range_start;
        plan->pla_range_end = range_end;
        plan->pla_range_start_buf = NULL;
        plan->pla_range_end_buf = NULL;
        plan->pla_key_constraints = key_constraints;
        ss_dassert(key_constraints == NULL || &plan->pla_key_constraints_buf == key_constraints);
        plan->pla_data_constraints = data_constraints;
        ss_dassert(data_constraints == NULL || &plan->pla_data_constraints_buf == data_constraints)
        plan->pla_constraints = constraints;
        ss_dassert(tuple_reference == NULL || &plan->pla_tuple_reference_buf == tuple_reference)
        plan->pla_tuple_reference = tuple_reference;
        plan->pla_select_list = select_list;
        plan->pla_nsolved_range_cons = nsolved_range_cons;
        plan->pla_nsolved_key_cons = nsolved_key_cons;
        plan->pla_nsolved_data_cons = nsolved_data_cons;
        plan->pla_nlink = 1;
        plan->pla_flags = 0;
        plan->pla_addlinks = addlinks;
        plan->pla_conslist_maxstoragelength = -1;   /* Unknown value. */

        if (dereference) {
            SU_BFLAG_SET(plan->pla_flags, PLA_DEREFERENCE);
        }
        if (start_closed) {
            SU_BFLAG_SET(plan->pla_flags, PLA_START_CLOSED);
        }
        if (end_closed) {
            SU_BFLAG_SET(plan->pla_flags, PLA_END_CLOSED);
        }
        if (isconsistent) {
            SU_BFLAG_SET(plan->pla_flags, PLA_CONSISTENT);
        } else {
            SU_BFLAG_CLEAR(plan->pla_flags, PLA_CONSISTENT);
        }
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *              rs_pla_check_reset
 * 
 * Checks before reset of a search plan.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *              
 *      plan - in, use
 *              plan
 *              
 *      relh - in, hold
 *              relation handle
 *              
 *      key - in
 *              key handle
 *              
 *      isconsistent - in
 *              FALSE if the search range is empty,
 *          i.e., the query is inconsistent
 *              
 *      range_start - in, take
 *              start of the search range
 *              
 *      start_closed - in
 *              TRUE if the start closed
 *              
 *      range_end - in, take
 *              end of the search range
 *              
 *      end_closed - in
 *              TRUE if the end closed
 *              
 *      key_constraints - in, take
 *              constraints for key entry
 *              
 *      data_constraints - in, take
 *              constraints for data tuple
 *          NULL if no constraints
 *
 *      constraints - in, hold
 *          all constraints of this search, in rs_cons objects.
 *
 *      tuple_reference - in, take
 *              rules for building the data
 *          tuple reference. NULL if no
 *          dereferencing is necessary
 *              
 *      select_list - in, take
 *              key part numbers of the
 *          selected columns
 *              
 *      dereference - in
 *              TRUE if data has to be fetched
 *          by dereferencing to the data
 *          tuple
 *              
 *      nsolved_range_cons - in
 *              number of constraints solved
 *          in the search range constraint
 *              
 *      nsolved_key_cons - in
 *              number of constraints solved
 *          in the index key (with the range
 *          constraints excluded)
 *              
 *      nsolved_data_cons - in
 *              number of constraints solved
 *          on the data tuple
 *
 * Return value - give : 
 * 
 *      Pointer to search plan object.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_check_reset(
        void*       cd,
        rs_pla_t*   plan,
        rs_relh_t*  relh __attribute__ ((unused)),
        rs_key_t*   key __attribute__ ((unused)),
        bool        isconsistent,
        dynvtpl_t   range_start,
        bool        start_closed,
        dynvtpl_t   range_end,
        bool        end_closed,
        su_list_t*  key_constraints __attribute__ ((unused)),
        su_list_t*  data_constraints __attribute__ ((unused)),
        su_list_t*  constraints __attribute__ ((unused)),
        su_list_t*  tuple_reference __attribute__ ((unused)),
        int*        select_list __attribute__ ((unused)),
        bool        dereference,
        long        nsolved_range_cons __attribute__ ((unused)),
        long        nsolved_key_cons __attribute__ ((unused)),
        long        nsolved_data_cons __attribute__ ((unused)))
{

        SS_NOTUSED(cd);

        ss_beta(plan->pla_check = RSCHK_PLAN);
        ss_dassert(plan->pla_relh == relh);
        ss_dassert(plan->pla_key == key);

        ss_dassert(plan->pla_key_constraints == key_constraints);
        ss_dassert(key_constraints == NULL || &plan->pla_key_constraints_buf == key_constraints);
        ss_dassert(plan->pla_data_constraints == data_constraints);
        ss_dassert(data_constraints == NULL || &plan->pla_data_constraints_buf == data_constraints)
        ss_dassert(plan->pla_constraints == constraints);
        ss_dassert(tuple_reference == NULL || &plan->pla_tuple_reference_buf == tuple_reference)
        ss_dassert(plan->pla_tuple_reference == tuple_reference);
        ss_dassert(plan->pla_select_list == select_list);
        ss_dassert(plan->pla_nsolved_range_cons == nsolved_range_cons);
        ss_dassert(plan->pla_nsolved_key_cons == nsolved_key_cons);
        ss_dassert(plan->pla_nsolved_data_cons == nsolved_data_cons);
        if (dereference) {
            ss_dassert(SU_BFLAG_TEST(plan->pla_flags, PLA_DEREFERENCE));
        }
        if (start_closed) {
            ss_dassert(SU_BFLAG_TEST(plan->pla_flags, PLA_START_CLOSED));
        }
        if (end_closed) {
            ss_dassert(SU_BFLAG_TEST(plan->pla_flags, PLA_END_CLOSED));
        }
        if (isconsistent) {
            ss_dassert(SU_BFLAG_TEST(plan->pla_flags, PLA_CONSISTENT));
        }
}

#endif /* SS_DEBUG */

/*##*********************************************************************\
 * 
 *		rs_pla_form_tuple_reference
 * 
 * Calculate the instructions for the engine how to form the
 * tuple reference from the key used. 
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	clkey - in, use
 *		    the clustering key = the key where the data tuple resides
 *
 *	list - in out, use
 *		    list where tuple reference info is stored
 *
 *	key - in, use
 *		  the key used
 *
 * 
 * Output params: 
 * 
 * Return value : out, give: a list containing the instructions
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_t* rs_pla_form_tuple_reference(
        rs_sysi_t* cd,
        rs_key_t* clkey,
        su_list_t* list,
        rs_key_t* key)
{
        uint i;
        uint clkey_nref;
        rs_pla_ref_t* ref;
        rs_ano_t kpno;  /* keypart number */
        int maxkpno = -1;

        ss_dassert(key);
        ss_dassert(clkey);
        ss_dassert(rs_key_isclustering(cd, clkey));

        clkey_nref = rs_key_nrefparts(cd, clkey);

        for (i = 0; i < clkey_nref; i++) {
            va_t* constva;
            constva = rs_keyp_constvalue(cd, clkey, i);
            if (constva != NULL) {
                ref = rs_pla_ref_init(cd, RS_ANO_NULL, constva);
            } else {
                rs_ano_t ano;
                /* Find attribute from key. */
                ano = rs_keyp_ano(cd, clkey, i);
                kpno = rs_key_searchkpno_anytype(cd, key, ano);
                ss_dassert(kpno != RS_ANO_NULL);
                maxkpno = SS_MAX(kpno, maxkpno);
                ref = rs_pla_ref_init(cd, kpno, NULL);
            }
            su_list_insertlast_nodebuf(
                list,
                rs_pla_ref_listnode(cd, ref),
                ref);
        }
        rs_key_setmaxrefkeypartno(cd, key, maxkpno);

#ifndef DBE_UPDATE_OPTIMIZATION
        if (rs_keyp_parttype(cd, clkey, i) == RSAT_TUPLE_VERSION) {
            /* Add also tuple version to the tuple reference.
             */
            rs_ano_t ano;
            /* Find attribute from key. */
            ano = rs_keyp_ano(cd, clkey, i);
            kpno = rs_key_searchkpno_data(cd, key, ano);
            ss_dassert(kpno != RS_ANO_NULL);
            ref = rs_pla_ref_init(cd, kpno, NULL);
            su_list_insertlast_nodebuf(
                list,
                rs_pla_ref_listnode(cd, ref),
                ref);
        }
#endif /* DBE_UPDATE_OPTIMIZATION */

        return(list);
}

/*##**********************************************************************\
 * 
 *              rs_pla_done
 * 
 * The following function frees the search plan structure and
 * all the pointers which were allocated during the creation of it.
 * Call this function when you have completed the query.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      plan - in, take
 *              plan object
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_done_zerolinks(cd, plan)
        void*       cd;
        rs_pla_t*   plan;
{
        su_list_node_t* n;
        rs_pla_cons_t*  cons;

        CHECK_PLAN(plan);
        ss_bassert(plan->pla_nlink == 0);

        if (plan->pla_addlinks) {
            rs_key_done(cd, plan->pla_key);
            SS_MEM_SETUNLINK(plan->pla_relh);
            rs_relh_done(cd, plan->pla_relh);
        }

        /* Free the range end and start. */

        if (plan->pla_range_start != plan->pla_range_start_buf) {
            dynvtpl_free(&plan->pla_range_start);
        }
        dynvtpl_free(&plan->pla_range_start_buf);
        if (plan->pla_range_end != plan->pla_range_end_buf) {
            dynvtpl_free(&plan->pla_range_end);
        }
        dynvtpl_free(&plan->pla_range_end_buf);

        /* Free the key constraint list. */
        
        if (plan->pla_key_constraints != NULL) {
            n = su_list_first(plan->pla_key_constraints);
            while (n != NULL) {
                cons = su_listnode_getdata(n);
                n = su_list_next(plan->pla_key_constraints, n);
                rs_pla_cons_done(cd, cons);
            }
        }

        /* Free the data constraint list. */
        
        if (plan->pla_data_constraints != NULL) {
            n = su_list_first(plan->pla_data_constraints);
            while (n != NULL) {
                cons = su_listnode_getdata(n);
                n = su_list_next(plan->pla_data_constraints, n);
                rs_pla_cons_done(cd, cons);
            }
        }

        /* Free the tuple reference list. */
        
        if (plan->pla_tuple_reference != NULL) {
            rs_pla_clear_tuple_reference_list(cd, plan->pla_tuple_reference);
        }

        /* Free the select list. */
        
        if (plan->pla_select_list != NULL) {
            SsMemFree(plan->pla_select_list);
        }

        SsMemFree(plan);
}

/*##**********************************************************************\
 * 
 *              rs_pla_get_conslist_maxstoragelength
 * 
 * Returns max storage length for constraint list.
 * 
 * Parameters : 
 * 
 *              cd - 
 *                      
 *                      
 *              plan - 
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
int rs_pla_get_conslist_maxstoragelength(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_pla_t*  plan)
{
        CHECK_PLAN(plan);

        return(plan->pla_conslist_maxstoragelength);
}

/*##**********************************************************************\
 * 
 *              rs_pla_get_range_buffers
 * 
 * Returns points to range buffers if those buffers can be reused.
 * 
 * Parameters : 
 * 
 *              cd - 
 *                      
 *                      
 *              plan - 
 *                      
 *                      
 *              range_start - 
 *                      
 *                      
 *              range_end - 
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
void rs_pla_get_range_buffers(
        void*       cd __attribute__ ((unused)),
        rs_pla_t*   plan,
        vtpl_t**    range_start,
        vtpl_t**    range_end)
{
        CHECK_PLAN(plan);

        *range_start = plan->pla_range_start_buf;
        *range_end = plan->pla_range_end_buf;
}

/*##**********************************************************************\
 * 
 *              rs_pla_set_range_buffers
 * 
 * Sets preallocated search range buffers that can be reused when 
 * re-executing the same query.
 * 
 * Parameters : 
 * 
 *              cd - 
 *                      
 *                      
 *              plan - 
 *                      
 *                      
 *              conslist_maxstoragelength - 
 *                      
 *                      
 *              range_start - 
 *                      
 *                      
 *              range_end - 
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
void rs_pla_set_range_buffers(
        void*       cd __attribute__ ((unused)),
        rs_pla_t*   plan,
        int         conslist_maxstoragelength,
        vtpl_t*     range_start,
        vtpl_t*     range_end)
{
        CHECK_PLAN(plan);
        ss_dassert(conslist_maxstoragelength > 0);
        ss_dassert(plan->pla_conslist_maxstoragelength == -1);
        ss_dassert(range_start != NULL);
        ss_dassert(range_end != NULL);
        ss_dassert(plan->pla_range_start_buf == NULL);
        ss_dassert(plan->pla_range_end_buf == NULL);

        plan->pla_conslist_maxstoragelength = conslist_maxstoragelength;
        plan->pla_range_start_buf = range_start;
        plan->pla_range_end_buf = range_end;
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *              rs_pla_get_range_start
 * 
 * The following function is used to get the search range start as a v-tuple.
 * The parameter isclosed is true if the start point is contained in the
 * range.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *
 *      plan - in, use
 *              plan object
 *
 *      range_start - out, ref
 *              range start as a v-tuple
 *
 *      isclosed - out, give
 *              TRUE if contains the start point
 *
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_get_range_start(cd, plan, range_start, isclosed)
        void*       cd;
        rs_pla_t*   plan;
        vtpl_t**    range_start;
        bool*       isclosed;

{
        CHECK_PLAN(plan);
        SS_NOTUSED(cd);

        RS_PLA_GET_RANGE_START(cd, plan, range_start, isclosed);
}


/*##**********************************************************************\
 * 
 *              rs_pla_get_range_end
 * 
 * The following function is used to get the search range end as a v-tuple.
 * The parameter isclosed is true if the end point is contained in the
 * range.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *
 *      plan - in, use
 *              plan object
 *
 * 
 *      range_end - out, ref
 *              range end
 *
 *      isclosed - out, give
 *              TRUE if contains the end point
 *
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_get_range_end(cd, plan, range_end, isclosed)
        void*       cd;
        rs_pla_t*   plan;
        vtpl_t**    range_end;
        bool*       isclosed;
{
        SS_NOTUSED(cd);

        CHECK_PLAN(plan);

        RS_PLA_GET_RANGE_END(cd, plan, range_end, isclosed);
}

/*##**********************************************************************\
 * 
 *              rs_pla_get_key_constraints
 * 
 * The following function returns as a list of structures
 * of type rs_pla_cons_t the constraints which
 * can be tested to the key entry. The constraints are given
 * using v-attributes and the key part index in the key entry v-tuple.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *
 *      plan - in, use
 *              plan object
 *
 * 
 * Return value : out, ref: list of constraints
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_t* rs_pla_get_key_constraints(cd, plan)
        void*       cd;
        rs_pla_t*   plan;
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);
        return(RS_PLA_GET_KEY_CONSTRAINTS(cd, plan));
}

#endif /* SS_DEBUG */

su_list_t* rs_pla_get_key_constraints_buf(cd, plan)
        void*       cd;
        rs_pla_t*   plan;
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);
        return(&plan->pla_key_constraints_buf);
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *              rs_pla_get_data_constraints
 *
 * The following function returns as a list the constraints which
 * must be applied to the data tuple (which is not the key entry).
 * The constraints are given
 * using v-attributes and the key part index in the key entry v-tuple.
 * Empty list is returned if all the tests can be based on the key entry
 * (e.g., in the case when the key is the clustering key). 
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *
 *      plan - in, use
 *              plan object
 *
 * 
 * Return value : out, ref: list of constraints
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_t* rs_pla_get_data_constraints(cd, plan)
        void*       cd;
        rs_pla_t*   plan;
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);

        return(RS_PLA_GET_DATA_CONSTRAINTS(cd, plan));
}

#endif /* SS_DEBUG */

su_list_t* rs_pla_get_data_constraints_buf(cd, plan)
        void*       cd;
        rs_pla_t*   plan;
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);
        return(&plan->pla_data_constraints_buf);
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *              rs_pla_get_tuple_reference
 * 
 * The following function returns as a list of structures
 * of type rs_pla_ref_t the rules which the index system uses to
 * build the tuple reference of the key entry.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *
 *      plan - in, ref
 *              plan object
 *
 * 
 * Return value : out, gilist of rules
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_list_t* rs_pla_get_tuple_reference(cd, plan)
        void*       cd;
        rs_pla_t*   plan;
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);
        return(RS_PLA_GET_TUPLE_REFERENCE(cd, plan));
}

#endif /* SS_DEBUG */

void rs_pla_clear_tuple_reference_buf(
        void*       cd,
        rs_pla_t*   plan)
{
        CHECK_PLAN(plan);

        if (plan->pla_tuple_reference != NULL) {

            ss_dassert(plan->pla_tuple_reference == &plan->pla_tuple_reference_buf);
            rs_pla_clear_tuple_reference_list(cd, plan->pla_tuple_reference);
            su_list_initbuf(&plan->pla_tuple_reference_buf, NULL);
        }
}

void rs_pla_clear_tuple_reference_list(
        void*       cd,
        su_list_t*  list)
{
        su_list_node_t* n;
        rs_pla_ref_t*   ref;

        n = su_list_first(list);
        while (n != NULL) {
            ref = su_listnode_getdata(n);
            n = su_list_next(list, n);
            rs_pla_ref_done(cd, ref);
        }
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *              rs_pla_get_select_list
 * 
 * The following function is used to get as an integer
 * array ending to -1 the indexes of the
 * attributes which are used to build the select list of the query.
 * The parameter must_dereference gets value TRUE if the
 * selected attributes have to be obtained by dereferencing first
 * to the data tuple and then fetching the attributes from there.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *
 *      plan - in, use
 *              plan object
 *
 * 
 *      select_list - out, ref 
 *              select list
 *
 *      must_dereference - out, give
 *              see explanation above
 *
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_get_select_list(cd, plan, select_list, must_dereference)
        void*       cd;
        rs_pla_t*   plan;
        int**       select_list;
        bool*       must_dereference;
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);
        *select_list = plan->pla_select_list;
        *must_dereference = SU_BFLAG_TEST(plan->pla_flags, PLA_DEREFERENCE);
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *              rs_pla_get_n_solved_cons
 * 
 * Returns the number of constraints which can be solved on range
 * constraint, chosen key and data tuple.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *              
 *      plan - in, use
 *             plan object      
 *              
 *      n_range - out, give
 *                    number of constraints constraints resolved by the
 *                range constraint
 *              
 *      n_key - out, give
 *                  number of constraints resolved on the chosen key
 *              
 *      n_data - out, give
 *                   number of constraints resolved on possible
 *               data tuple
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_get_n_solved_cons(
        void*       cd,
        rs_pla_t*   plan,
        long*       n_range,
        long*       n_key,
        long*       n_data)
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);

        *n_range = plan->pla_nsolved_range_cons;
        *n_key   = plan->pla_nsolved_key_cons;
        *n_data  = plan->pla_nsolved_data_cons;
}

/*##**********************************************************************\
 * 
 *              rs_pla_cons_init
 * 
 * Creates a new plan constraint.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      kpindex - in
 *              key part index in v-tuple in database
 *
 *      relop - in
 *              relational operator
 *
 *      va - in, use
 *              v-attribute that used in comparison
 *
 *      escchar - in
 *              escape character for LIKE type constraint,
 *          RS_CONS_NOESCCHAR if none
 *
 *      flags - in
 *          bit flags:
 *              RS_PLA_CONS_FLAG_UNI4CHAR =
 *                  flag telling if the constraint is
 *                  a UNICODE constraint for CHAR column.
 *                  That is never the case when comparison is
 *                  EQUAL, NOTEQUAL or LIKE.
 *              RS_PLA_CONS_FLAG_UNICODE =
 *                  flag telling the field (and constraint) are
 *                  both UNICODE (information needed in like comparisons).
 *                  this flag is exclusive with RS_PLA_CONS_FLAG_UNI4CHAR
 *
 * Return value - give : 
 * 
 *      pointer to the plan constraint
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_pla_cons_t* rs_pla_cons_init(
        void*       cd,
        rs_ano_t    kpindex,
        uint        relop,
        va_t*       va,
        int         escchar,
        su_bflag_t  flags,
        va_t*       defva
#ifdef SS_COLLATION
        , rs_cons_t*  rscons
#endif /* SS_COLLATION */
    )
{
        rs_pla_cons_t* pla_cons;

        SS_NOTUSED(cd);

        pla_cons = SSMEM_NEW(rs_pla_cons_t);

        ss_debug(pla_cons->pc_check = RSCHK_PLACONS);
        pla_cons->pc_kpindex = kpindex;
        pla_cons->pc_relop = relop;
        pla_cons->pc_va = NULL;
        dynva_setva(&pla_cons->pc_va, va);
        pla_cons->pc_escchar = escchar;
        pla_cons->pc_flags = flags;
        pla_cons->pc_defva = NULL;
#ifdef SS_COLLATION
        pla_cons->pc_rscons = rscons;
#endif /* SS_COLLATION */
        if (defva != NULL) {
            dynva_setva(&pla_cons->pc_defva, defva);
        }

        return(pla_cons);
}

/*##**********************************************************************\
 * 
 *              rs_pla_cons_done
 * 
 * Frees plan constraint.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_cons - in, take
 *              plan constraint
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_cons_done(void* cd, rs_pla_cons_t* pla_cons)
{
        SS_NOTUSED(cd);
        CHECK_PLACONS(pla_cons);

        dynva_free(&pla_cons->pc_va);
        if (pla_cons->pc_defva != NULL) {
            dynva_free(&pla_cons->pc_defva);
        }
        SsMemFree(pla_cons);
}

/*##**********************************************************************\
 * 
 *              rs_pla_cons_reset
 * 
 * Reset the plan constraint with a new v-attribute value.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_cons - in, use
 *              plan constraint
 *
 *      va - in, use
 *              v-attribute that used in comparison
 *
 * Return value: 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_cons_reset(
        void*       cd,
        rs_pla_cons_t* pla_cons,
        va_t*       va
#ifdef SS_COLLATION
        , rs_cons_t*  rscons
#endif /* SS_COLLATION */
    )
{
        SS_NOTUSED(cd);
        CHECK_PLACONS(pla_cons);

#ifdef SS_COLLATION
        ss_dassert(pla_cons->pc_rscons == rscons);
#endif /* SS_COLLATION */
        dynva_setva(&pla_cons->pc_va, va);
}

/*##**********************************************************************\
 * 
 *              rs_pla_cons_relop
 * 
 * Retuns the relational operator of plan constraint.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_cons - in, use
 *              plan constraint
 *
 * Return value : 
 * 
 *      relational operator
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint rs_pla_cons_relop(void* cd, rs_pla_cons_t* pla_cons)
{
        SS_NOTUSED(cd);
        CHECK_PLACONS(pla_cons);

        return(RS_PLA_CONS_RELOP(cd, pla_cons));
}

/*##**********************************************************************\
 * 
 *              rs_pla_cons_kpindex
 * 
 * Returns the physical index of the field in the key.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_cons - in, use
 *              plan constraint
 *
 * Return value : 
 * 
 *      key part index
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint rs_pla_cons_kpindex(void* cd, rs_pla_cons_t* pla_cons)
{
        SS_NOTUSED(cd);
        CHECK_PLACONS(pla_cons);

        return(RS_PLA_CONS_KPINDEX(cd, pla_cons));
}

/*##**********************************************************************\
 * 
 *              rs_pla_cons_va
 * 
 * Returns the v-attribute for the test.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_cons - in, use
 *              plan constraint
 *
 * Return value - ref : 
 * 
 *      v-attribute
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* rs_pla_cons_va(void* cd, rs_pla_cons_t* pla_cons)
{
        SS_NOTUSED(cd);
        CHECK_PLACONS(pla_cons);

        return(RS_PLA_CONS_VA(cd, pla_cons));
}

/*##**********************************************************************\
 * 
 *              rs_pla_cons_escchar
 * 
 * Returns the escape char in a LIKE string or RS_CONS_NOESCCHAR if none.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_cons - in, use
 *              plan constraint
 *
 * Return value : 
 * 
 *      escape character
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
int rs_pla_cons_escchar(void* cd, rs_pla_cons_t* pla_cons)
{
        SS_NOTUSED(cd);
        CHECK_PLACONS(pla_cons);

        return(RS_PLA_CONS_ESCCHAR(cd, pla_cons));
}

va_t* rs_pla_cons_defva(void* cd, rs_pla_cons_t* pla_cons)
{
        SS_NOTUSED(cd);
        CHECK_PLACONS(pla_cons);

        return(RS_PLA_CONS_DEFVA(cd, pla_cons));
}

#ifdef SS_DEBUG
su_list_node_t* rs_pla_cons_listnode(void* cd, rs_pla_cons_t* pla_cons)
{
        SS_NOTUSED(cd);
        CHECK_PLACONS(pla_cons);

        ss_debug(SU_BFLAG_SET(pla_cons->pc_flags, RS_PLA_CONS_FLAG_LISTNODEUSED));

        return(_RS_PLA_CONS_LISTNODE_(cd, pla_cons));
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *              rs_pla_ref_init
 * 
 * Creates a new plan reference.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      kpindex - in, use
 *              key part index in v-tuple
 *
 *      constantvalue - in, use
 *              constant value, or NULL if not constant
 * 
 * Return value - give : 
 * 
 *      pointer to the new plan reference
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_pla_ref_t* rs_pla_ref_init(
        void*    cd,
        rs_ano_t kpindex,
        va_t*    constantvalue)
{
        rs_pla_ref_t* pla_ref;

        SS_NOTUSED(cd);

        pla_ref = SSMEM_NEW(rs_pla_ref_t);

        ss_beta(pla_ref->pr_check = RSCHK_PLAREF);
        pla_ref->pr_kpindex = kpindex;
        pla_ref->pr_constantvalue = NULL;
        if (constantvalue != NULL) {
            dynva_setva(&pla_ref->pr_constantvalue, constantvalue);
        }

        return(pla_ref);
}

/*##**********************************************************************\
 * 
 *              rs_pla_ref_done
 * 
 * Frees plan reference.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_ref - in, take
 *              plan reference
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_pla_ref_done(void* cd, rs_pla_ref_t* pla_ref)
{
        SS_NOTUSED(cd);
        CHECK_PLAREF(pla_ref);

        dynva_free(&pla_ref->pr_constantvalue);
        SsMemFree(pla_ref);
}

/*##**********************************************************************\
 * 
 *              rs_pla_ref_isconstant
 * 
 * Returns true if the reference contains a constant field,
 * whose value can be asked from the second function.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_ref - in, use
 *              plan reference
 *
 * Return value : 
 * 
 *      TRUE  - reference is constant
 *      FALSE - reference is not constant
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_pla_ref_isconstant(void* cd, rs_pla_ref_t* pla_ref)
{
        SS_NOTUSED(cd);
        CHECK_PLAREF(pla_ref);

        return(pla_ref->pr_constantvalue != NULL);
}

/*##**********************************************************************\
 * 
 *              rs_pla_ref_kpindex
 * 
 * If the reference contains a non-constant field, the following
 * function returns the key part index of it in the key used.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_ref - in, use
 *              plan reference
 * 
 * Return value : 
 * 
 *      key part index
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint rs_pla_ref_kpindex(void* cd, rs_pla_ref_t* pla_ref)
{
        SS_NOTUSED(cd);
        CHECK_PLAREF(pla_ref);

        return(pla_ref->pr_kpindex);
}

su_list_node_t* rs_pla_ref_listnode(
        void* cd,
        rs_pla_ref_t* pla_ref)
{
        SS_NOTUSED(cd);
        CHECK_PLAREF(pla_ref);

        return(&pla_ref->pr_listnode);
}
