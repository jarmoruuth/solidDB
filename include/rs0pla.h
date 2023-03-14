/*************************************************************************\
**  source       * rs0pla.h
**  directory    * res
**  description  * Search plan access functions
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


#ifndef RS0PLA_H
#define RS0PLA_H

#include <ssc.h>

#include <su0list.h>
#include <su0bflag.h>

#include <uti0vtpl.h>

#include "rs0types.h"
#include "rs0cons.h"
#include "rs0key.h"
#include "rs0relh.h"

#define CHECK_PLAN(pla)   ss_bassert(SS_CHKPTR(pla) && (pla)->pla_check == RSCHK_PLAN);
#define CHECK_PLAREF(pr)  ss_bassert(SS_CHKPTR(pr) && (pr)->pr_check == RSCHK_PLAREF);

/* The structure which contains the search plan for the engine.
 */
typedef struct rs_pla_struct rs_pla_t;

/* The structure which contains the constraint for the engine.
 */
typedef struct rs_pla_cons_struct rs_pla_cons_t;

/* The structure which contains the instructions to the engine
 * how to build the tuple reference.
 */
typedef struct rs_pla_ref_struct rs_pla_ref_t;

/* Plan Constraint flags */
#define RS_PLA_CONS_FLAG_UNI4CHAR       SU_BFLAG_BIT(0)
#define RS_PLA_CONS_FLAG_UNICODE        SU_BFLAG_BIT(1)

#define RS_PLA_CONS_FLAG_INCLUDENULLS   SU_BFLAG_BIT(2)
#define RS_PLA_CONS_FLAG_LISTNODEUSED   SU_BFLAG_BIT(3)
#define RS_PLA_CONS_FLAG_COLLATED       SU_BFLAG_BIT(4)

/* Plan flags.
 */
#define PLA_CONSISTENT          SU_BFLAG_BIT(0)
#define PLA_START_CLOSED        SU_BFLAG_BIT(1)
#define PLA_END_CLOSED          SU_BFLAG_BIT(2)
#define PLA_DEREFERENCE         SU_BFLAG_BIT(3)
#define PLA_INCONSISTENT_ONCE   SU_BFLAG_BIT(4)
#define PLA_REPLAN              SU_BFLAG_BIT(5)

/* The structure which contains the constraint for the engine.
 *
 * NOTE! Fields of this structure are accessed directly using
 *       RS_PLA_CONS_* macros, so be careful if changing the
 *       content of this structure,
 */
struct rs_pla_cons_struct {
        rs_ano_t        pc_kpindex;
        uint            pc_relop;
        dynva_t         pc_va;
        int             pc_escchar;
        su_list_node_t  pc_listnode;
        su_bflag_t      pc_flags;
        dynva_t         pc_defva;
#ifdef SS_COLLATION
        rs_cons_t*      pc_rscons;
#endif /* SS_COLLATION */
        ss_debug(int    pc_check;) /* check field */
};

/* Search plan structure.
 */
struct rs_pla_struct {
        ss_beta(int pla_check;)            /* Check field */
        rs_relh_t*  pla_relh;               /* Relation the plan is using. */
        rs_key_t*   pla_key;                /* Key the plan is using. */
        su_bflag_t  pla_flags;              /* If does not contain
                                               PLA_CONSISTENT, the search
                                               range is empty, i.e., the
                                               query is inconsistent.
                                               If contains PLA_START_CLOSED,
                                               the start is closed.
                                               If contains PLA_END_CLOSED,
                                               the end is closed.
                                               If contains PLA_DEREFERENCE,
                                               data has to be fetched by
                                               dereferencing to the data
                                               tuple. */
        dynvtpl_t   pla_range_start;        /* start of the search range */
        dynvtpl_t   pla_range_end;          /* end of the search range */
        dynvtpl_t   pla_range_start_buf;    /* start of the search range */
        dynvtpl_t   pla_range_end_buf;      /* end of the search range */
        su_list_t*  pla_key_constraints;    /* constraints for key entry */
        su_list_t   pla_key_constraints_buf;/* key cons list buf */
        su_list_t*  pla_data_constraints;   /* constraints for data tuple
                                               NULL if no constraints */
        su_list_t   pla_data_constraints_buf;/* data cons list buf */
        su_list_t*  pla_constraints;        /* All constraints for this search,
                                               in rs_cons objects. */
        su_list_t*  pla_tuple_reference;    /* rules for building the data
                                               tuple reference. NULL if no
                                               dereferencing is necessary */
        su_list_t   pla_tuple_reference_buf;/* tuple reference list buf */
        int*        pla_select_list;        /* key part numbers of the
                                               selected columns */
        long        pla_nsolved_range_cons; /* number of constraints solved
                                               in the search range constraint */
        long        pla_nsolved_key_cons;   /* number of constraints solved
                                               in the index key (with the
                                               range constraints excluded) */
        long        pla_nsolved_data_cons;  /* number of constraints solved
                                               on the data tuple */
        int         pla_nlink;              /* Number of links to this object */
        bool        pla_addlinks;           /* Should we use relh and key links. */
        int         pla_conslist_maxstoragelength;
};

/* The structure which contains the instructions to the engine
 * how to build the tuple reference.
 */
struct rs_pla_ref_struct {
        ss_beta(rs_check_t pr_check;) /* check field */
        rs_ano_t        pr_kpindex;
        va_t*           pr_constantvalue;
        su_list_node_t  pr_listnode;
};

rs_pla_t* rs_pla_alloc(
        void*       cd);

/* The following function creates a search plan structure and
 * generates the search plan. The table level has to call this
 * first and then the functions which extract information from
 * the returned object.
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
        bool        addlinks);

SS_INLINE void rs_pla_reset(
        void*       cd,
        rs_pla_t*   plan,
        bool        isconsistent,
        dynvtpl_t   range_start,
        dynvtpl_t   range_end);

void rs_pla_check_reset(
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
        long        nsolved_data_cons);

su_list_t* rs_pla_form_tuple_reference(
        rs_sysi_t* cd,
        rs_key_t* clkey,
        su_list_t* list,
        rs_key_t* key);

/* The following function frees the search plan structure and
 * all the pointers which were allocated during the creation of it.
 * Call this function when you have completed the query.
 */
SS_INLINE void rs_pla_done(
        void* cd,
        rs_pla_t* plan);

void rs_pla_done_zerolinks(
        void* cd,
        rs_pla_t* plan);

/* The following function is used to link new user to the plan object.
 * The plan object is not physically deleted until all users have
 * called rs_pla_done.
 */
SS_INLINE void rs_pla_link(
        void* cd,
        rs_pla_t* plan);

SS_INLINE rs_relh_t* rs_pla_getrelh(
        void* cd,
        rs_pla_t* plan);

SS_INLINE ulong rs_pla_getrelid(
        void*       cd,
        rs_pla_t*   plan);

SS_INLINE rs_key_t* rs_pla_getkey(
        void* cd,
        rs_pla_t* plan);

SS_INLINE ulong rs_pla_getkeyid(
        void*       cd,
        rs_pla_t*   plan);

SS_INLINE bool rs_pla_usingclusterkey(
        void* cd,
        rs_pla_t* plan);

/* The following function returns FALSE if the search range is empty,
 * i.e, the query is contradictory. If it is contradictory, just call
 * the rs_pla_done function and do not try to extract more
 * information of the plan.
 */
SS_INLINE bool rs_pla_isconsistent(
        void* cd,
        rs_pla_t* plan);

SS_INLINE void rs_pla_setconsistent_once(
        void* cd,
        rs_pla_t* plan,
        bool isconsistent_once);

SS_INLINE bool rs_pla_replan(
        void* cd,
        rs_pla_t* plan);

SS_INLINE void rs_pla_setreplan(
        void* cd,
        rs_pla_t* plan,
        bool b);

int rs_pla_get_conslist_maxstoragelength(
        rs_sysi_t* cd,
        rs_pla_t*  plan);

void rs_pla_get_range_buffers(
        void*       cd,
        rs_pla_t*   plan,
        vtpl_t**    range_start,
        vtpl_t**    range_end);

void rs_pla_set_range_buffers(
        void*       cd,
        rs_pla_t*   plan,
        int         conslist_maxstoragelength,
        vtpl_t*     range_start,
        vtpl_t*     range_end);

/* The following function is used to get the search range start as a v-tuple.
 * The parameter isclosed is true if the start point is contained in the
 * range.
 */
void rs_pla_get_range_start(
        void* cd,
        rs_pla_t* plan,
        vtpl_t** range_start,
        bool* isclosed);

/* The following function is used to get the search range end as a v-tuple.
 * The parameter isclosed is true if the end point is contained in the
 * range.
 */
void rs_pla_get_range_end(
        void* cd,
        rs_pla_t* plan,
        vtpl_t** range_end,
        bool* isclosed);

/* The following function returns as a list of structures
 * of type rs_pla_cons_t the constraints which
 * can be tested to the key entry. The constraints are given
 * using v-attributes and the key part index in the key entry v-tuple.
 */
su_list_t* rs_pla_get_key_constraints(
        void* cd,
        rs_pla_t* plan);

su_list_t* rs_pla_get_key_constraints_buf(
        void* cd,
        rs_pla_t* plan);

SS_INLINE su_list_t* rs_pla_get_constraints(
        void*       cd,
        rs_pla_t*   plan);

SS_INLINE void rs_pla_set_constraints(
        rs_sysi_t*  cd,
        rs_pla_t*   plan,
        su_list_t*  constraints);

/* The following function returns as a list the constraints which
 * must be applied to the data tuple (which is not the key entry).
 * The constraints are given
 * using v-attributes and the key part index in the key entry v-tuple.
 * Empty list is returned if all the tests can be based on the key entry
 * (e.g., in the case when the key is the clustering key).
 */
su_list_t* rs_pla_get_data_constraints(
        void* cd,
        rs_pla_t* plan);

su_list_t* rs_pla_get_data_constraints_buf(
        void* cd,
        rs_pla_t* plan);

/* The following function returns as a list of structures
 * of type rs_pla_ref_t the rules which the index system uses to
 * build the tuple reference of the key entry.
 * Empty list is returned if no tuple dereferencing is necessary.
 */
su_list_t* rs_pla_get_tuple_reference(
        void* cd,
        rs_pla_t* plan);

SS_INLINE su_list_t* rs_pla_get_tuple_reference_buf(
        void* cd,
        rs_pla_t* plan);

void rs_pla_clear_tuple_reference_buf(
        void*       cd,
        rs_pla_t*   plan);

void rs_pla_clear_tuple_reference_list(
        void*       cd,
        su_list_t*  list);

/* The following function is used to get as an integer
 * array ending to -1 the indexes of the
 * attributes which are used to build the select list of the query.
 * The parameter must_dereference gets value TRUE if the
 * selected attributes have to be obtained by dereferencing first
 * to the data tuple and then fetching the attributes from there.
 */
void rs_pla_get_select_list(
        void* cd,
        rs_pla_t* plan,
        int** select_list,
        bool* must_dereference);

/* Functions for testing */

void rs_pla_get_n_solved_cons(
        void* cd,
        rs_pla_t* plan,
        long* n_range,
        long* n_key,
        long* n_data);

/* INIT FUNCTIONS FOR THE PLAN CONSTRAINT */

rs_pla_cons_t* rs_pla_cons_init(
        void*       cd,
        rs_ano_t    kpindex,
        uint        relop,
        va_t*       va,
        int         escchar,   /* RS_CONS_NOESCCHAR if none */
        su_bflag_t  flags,
        va_t*       defva
#ifdef SS_COLLATION
        , rs_cons_t*  rscons
#endif /* SS_COLLATION */
);

void rs_pla_cons_done(
        void* cd, 
        rs_pla_cons_t* pla_cons);

void rs_pla_cons_reset(
        void*       cd,
        rs_pla_cons_t* pla_cons,
        va_t*       va
#ifdef SS_COLLATION
        , rs_cons_t*  rscons
#endif /* SS_COLLATION */
);

#ifdef SS_COLLATION

SS_INLINE rs_cons_t* rs_pla_cons_get_rscons(
        void* cd,
        rs_pla_cons_t* pla_cons);

#endif /* SS_COLLATION */

/* ACCESSOR FUNCTIONS FOR THE CONSTRAINT */

uint rs_pla_cons_relop(
        void* cd,
        rs_pla_cons_t* pla_cons);

/* The following returns the physical index of the field in the key
 */
uint rs_pla_cons_kpindex(
        void* cd,
        rs_pla_cons_t* pla_cons);

/* The following returns the v-attribute for the test
 */
va_t* rs_pla_cons_va(
        void* cd,
        rs_pla_cons_t* pla_cons);

/* The following returns the escape char in a LIKE string
 */
int rs_pla_cons_escchar(
        void* cd,
        rs_pla_cons_t* pla_cons);

va_t* rs_pla_cons_defva(
        void*           cd,
        rs_pla_cons_t*  pla_cons);

#ifdef SS_DEBUG
/* The following returns pre-allocated list node for cons
 */
su_list_node_t* rs_pla_cons_listnode(
        void* cd,
        rs_pla_cons_t* pla_cons);
#endif /* SS_DEBUG */

/* INIT FUNCTIONS FOR THE TUPLE REFERENCE INSTRUCTIONS */

rs_pla_ref_t* rs_pla_ref_init(
        void*    cd,
        rs_ano_t kpindex,
        va_t*    constantvalue  /* NULL if none */
);

void rs_pla_ref_done(
        void* cd,
        rs_pla_ref_t* pla_ref);

/* ACCESSOR FUNCTIONS FOR THE TUPLE REFERENCE INSTRUCTIONS */

/* The following is true if the reference contains a constant field,
 * whose value can be asked from the second function.
 */
bool rs_pla_ref_isconstant(
        void* cd,
        rs_pla_ref_t* pla_ref);

SS_INLINE va_t* rs_pla_ref_value(
        void* cd,
        rs_pla_ref_t* pla_ref);

/* If the reference contains a non-constant field, the following
 * function returns the key part index of it in the key used.
 */
uint rs_pla_ref_kpindex(
        void* cd,
        rs_pla_ref_t* pla_ref);

/* The following returns pre-allocated list node for ref
 */
su_list_node_t* rs_pla_ref_listnode(
        void* cd,
        rs_pla_ref_t* pla_ref);

/*** MACRO VERSIONS FOR FAST ACCESS ***/

#define RS_PLA_CONS_RELOP(cd, pla_cons)         ((pla_cons)->pc_relop)
#define RS_PLA_CONS_KPINDEX(cd, pla_cons)       ((pla_cons)->pc_kpindex)
#define RS_PLA_CONS_VA(cd, pla_cons)            ((pla_cons)->pc_va)
#define RS_PLA_CONS_ESCCHAR(cd, pla_cons)       ((pla_cons)->pc_escchar)
#define RS_PLA_CONS_DEFVA(cd, pla_cons)         ((pla_cons)->pc_defva)
#define _RS_PLA_CONS_LISTNODE_(cd, pla_cons)    (&(pla_cons)->pc_listnode)
#define RS_PLA_GET_KEY_CONSTRAINTS(cd, plan)    ((plan)->pla_key_constraints)
#define RS_PLA_GET_RANGE_START(cd, plan, range_start, isclosed) \
                                                { \
                                                *(range_start) = (plan)->pla_range_start; \
                                                *(isclosed) = SU_BFLAG_TEST((plan)->pla_flags, PLA_START_CLOSED); \
                                                }
#define RS_PLA_GET_RANGE_END(cd, plan, range_end, isclosed) \
                                                { \
                                                *(range_end) = (plan)->pla_range_end; \
                                                *(isclosed) = SU_BFLAG_TEST((plan)->pla_flags, PLA_END_CLOSED); \
                                                }
#define RS_PLA_GET_DATA_CONSTRAINTS(cd, plan)   ((plan)->pla_data_constraints)
#define RS_PLA_GET_SELECT_LIST(cd, plan, select_list, must_dereference) \
                                                { \
                                                *(select_list) = (plan)->pla_select_list;\
                                                *(must_dereference) = SU_BFLAG_TEST((plan)->pla_flags, PLA_DEREFERENCE);\
                                                }
#define RS_PLA_GET_TUPLE_REFERENCE(cd, plan)    ((plan)->pla_tuple_reference)

#ifndef SS_DEBUG
#define rs_pla_cons_listnode         _RS_PLA_CONS_LISTNODE_
#define rs_pla_get_key_constraints   RS_PLA_GET_KEY_CONSTRAINTS 
#define rs_pla_get_range_start       RS_PLA_GET_RANGE_START     
#define rs_pla_get_range_end         RS_PLA_GET_RANGE_END       
#define rs_pla_get_data_constraints  RS_PLA_GET_DATA_CONSTRAINTS
#define rs_pla_get_select_list       RS_PLA_GET_SELECT_LIST     
#define rs_pla_get_tuple_reference   RS_PLA_GET_TUPLE_REFERENCE 
#endif /* !SS_DEBUG */

#    define RS_PLA_CONS_UNIFORCHAR(cd, pla_cons) \
        SU_BFLAG_TEST((pla_cons)->pc_flags, RS_PLA_CONS_FLAG_UNI4CHAR)

#    define RS_PLA_CONS_ISUNICODE(cd, pla_cons) \
        SU_BFLAG_TEST((pla_cons)->pc_flags, RS_PLA_CONS_FLAG_UNICODE)

#    define RS_PLA_CONS_INCLUDENULLS(cd, pla_cons) \
        SU_BFLAG_TEST((pla_cons)->pc_flags, RS_PLA_CONS_FLAG_INCLUDENULLS)

#    define RS_PLA_CONS_ISCOLLATED(cd, pla_cons) \
        SU_BFLAG_TEST((pla_cons)->pc_flags, RS_PLA_CONS_FLAG_COLLATED)

#define rs_pla_get_range_start_vtpl(cd, plan)   ((plan)->pla_range_start)
#define rs_pla_get_range_end_vtpl(cd, plan)     ((plan)->pla_range_end)

#if defined(RS0PLA_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *              rs_pla_reset
 * 
 * Reset a search plan.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *              
 *      plan - in
 *          plan        FALSE if the search range is empty,
 *              
 *      range_start - in, take
 *              start of the search range
 *              
 *      range_end - in, take
 *              end of the search range
 *              
 *      isconsistent - in
 *              FALSE if the search range is empty,
 *          i.e., the query is inconsistent
 *
 * Return value - give : 
 * 
 *      Pointer to search plan object.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void rs_pla_reset(
        void*       cd,
        rs_pla_t*   plan,
        bool        isconsistent,
        dynvtpl_t   range_start,
        dynvtpl_t   range_end)
{
        ss_beta(plan->pla_check = RSCHK_PLAN);

        if (plan->pla_range_start != plan->pla_range_start_buf) {
            dynvtpl_free(&plan->pla_range_start);
        }
        if (plan->pla_range_end != plan->pla_range_end_buf) {
            dynvtpl_free(&plan->pla_range_end);
        }
        plan->pla_range_start = range_start;
        plan->pla_range_end = range_end;

        if (!isconsistent) {
            SU_BFLAG_CLEAR(plan->pla_flags, PLA_CONSISTENT);
        }
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
SS_INLINE void rs_pla_done(
        void*       cd,
        rs_pla_t*   plan)
{
        CHECK_PLAN(plan);
        ss_bassert(plan->pla_nlink > 0);

        plan->pla_nlink--;

        /* If there are still links to the plan, do not delete it. */
        if (plan->pla_nlink > 0) {
            return;
        }

        rs_pla_done_zerolinks(cd, plan);
}

/*##**********************************************************************\
 * 
 *              rs_pla_link
 * 
 * Increments the counter of links to this object. The object is
 * physically deleted when the link count comes to zero. Each
 * rs_pla_done decrements the counter. Function rs_pla_init sets 
 * the counter to one.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *              
 *      plan - in out, use
 *              search plan
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void rs_pla_link(void* cd, rs_pla_t* plan)
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);
        ss_bassert(plan->pla_nlink > 0);

        plan->pla_nlink++;
}

SS_INLINE su_list_t* rs_pla_get_tuple_reference_buf(
        void*       cd,
        rs_pla_t*   plan)
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);
        return(&plan->pla_tuple_reference_buf);
}

/*##**********************************************************************\
 * 
 *              rs_pla_usingclusterkey
 * 
 * Returns TRUE if the search plan uses clustering key. If this function
 * returns FALSE but getdata is TRUE, then the clustering key is used
 * only to get the data tuple.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      plan - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE bool rs_pla_usingclusterkey(
        void*       cd,
        rs_pla_t*   plan)
{
        SS_NOTUSED(cd);

        CHECK_PLAN(plan);

        return(rs_key_isclustering(cd, plan->pla_key));
}

/*##**********************************************************************\
 * 
 *              rs_pla_getrelh
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      plan - 
 *              
 *              
 * Return value - ref : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE rs_relh_t* rs_pla_getrelh(
        void* cd,
        rs_pla_t* plan)
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);

        return(plan->pla_relh);
}

/*##**********************************************************************\
 * 
 *              rs_pla_getrelid
 * 
 * Returns the relation id of the plan.
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
 * Return value :
 * 
 *      relation id
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE ulong rs_pla_getrelid(
        void*       cd,
        rs_pla_t*   plan)
{
        SS_NOTUSED(cd);

        CHECK_PLAN(plan);

        return(rs_relh_relid(cd, plan->pla_relh));
}

/*##**********************************************************************\
 * 
 *              rs_pla_getkeyid
 * 
 * Returns the key id of the plan.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *
 *      plan - in, use
 *              plan object
 *
 * Return value :
 * 
 *      key id
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE ulong rs_pla_getkeyid(
        void*       cd,
        rs_pla_t*   plan)
{
        SS_NOTUSED(cd);

        CHECK_PLAN(plan);

        return(rs_key_id(cd, plan->pla_key));
}

/*##**********************************************************************\
 * 
 *              rs_pla_isconsistent
 * 
 * The following function returns FALSE if the search range is empty,
 * i.e, the query is contradictory. If it is contradictory, just call
 * the tb_pla_done function and do not try to extract more
 * information of the plan. 
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
 * Return value : out, give: TRUE if consistent, else FALSE
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE bool rs_pla_isconsistent(
        void*       cd,
        rs_pla_t*   plan)
{
        SS_NOTUSED(cd);

        CHECK_PLAN(plan);

        return(SU_BFLAG_TEST(plan->pla_flags, PLA_CONSISTENT)
               && !SU_BFLAG_TEST(plan->pla_flags, PLA_INCONSISTENT_ONCE));
}

/*##**********************************************************************\
 * 
 *              rs_pla_setconsistent_once
 * 
 * The following function sets the plan consistency flag. This flag is
 * used between cursor reset calls, it doesn not mean that even after reset
 * the plan is not consistent.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              
 *
 *      plan - in, use
 *              plan object
 *
 *      isconsistent - in, use
 *              TRUE or FALSE, note that TRUE always forces the plan to be consistent
 *      so it should be used only when reseting the flag.
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void rs_pla_setconsistent_once(
        void*       cd,
        rs_pla_t*   plan,
        bool        isconsistent_once)
{
        SS_NOTUSED(cd);

        CHECK_PLAN(plan);

        if (isconsistent_once) {
            SU_BFLAG_CLEAR(plan->pla_flags, PLA_INCONSISTENT_ONCE);
        } else {
            SU_BFLAG_SET(plan->pla_flags, PLA_INCONSISTENT_ONCE);
        }
}

SS_INLINE bool rs_pla_replan(
        void*       cd,
        rs_pla_t*   plan)
{
        SS_NOTUSED(cd);

        CHECK_PLAN(plan);

        return(SU_BFLAG_TEST(plan->pla_flags, PLA_REPLAN));
}

SS_INLINE void rs_pla_setreplan(
        void*       cd,
        rs_pla_t*   plan,
        bool        b)
{
        SS_NOTUSED(cd);

        CHECK_PLAN(plan);

        if (b) {
            SU_BFLAG_SET(plan->pla_flags, PLA_REPLAN);
        } else {
            SU_BFLAG_CLEAR(plan->pla_flags, PLA_REPLAN);
        }
}

SS_INLINE su_list_t* rs_pla_get_constraints(
        void*       cd __attribute__ ((unused)),
        rs_pla_t*   plan)
{
        CHECK_PLAN(plan);

        return plan->pla_constraints;
}

SS_INLINE void rs_pla_set_constraints(
        rs_sysi_t*  cd,
        rs_pla_t*   plan,
        su_list_t*  constraints)
{
        su_list_node_t*     n;
        rs_cons_t*          cons;
        void*               consptr;
        
        plan->pla_constraints = constraints;
        SU_BFLAG_SET(plan->pla_flags, PLA_CONSISTENT);

        su_list_do_get(constraints, n, consptr) {
            cons = (rs_cons_t*)consptr;
            ss_dassert(!rs_cons_issolved(cd, cons));
            if (rs_cons_isalwaysfalse(cd, cons)) {
                SU_BFLAG_CLEAR(plan->pla_flags, PLA_CONSISTENT);
                break;
            }
        }
}

/*##**********************************************************************\
 * 
 *              rs_pla_getkey
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      plan - 
 *              
 *              
 * Return value - ref : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE rs_key_t* rs_pla_getkey(
        void* cd,
        rs_pla_t* plan)
{
        SS_NOTUSED(cd);
        CHECK_PLAN(plan);

        return(plan->pla_key);
}

/*##**********************************************************************\
 * 
 *              rs_pla_ref_value
 * 
 * Returns the constant value.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      pla_ref - in, use
 *              plan reference
 *
 * Return value - ref : 
 * 
 *      v-attribute
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE va_t* rs_pla_ref_value(void* cd, rs_pla_ref_t* pla_ref)
{
        SS_NOTUSED(cd);
        CHECK_PLAREF(pla_ref);

        return(pla_ref->pr_constantvalue);
}

#ifdef SS_COLLATION

SS_INLINE rs_cons_t* rs_pla_cons_get_rscons(
        void* cd,
        rs_pla_cons_t* pla_cons)
{
        return (pla_cons->pc_rscons);
}

#endif /* SS_COLLATION */

#endif /* defined(RS0PLA_C) || defined(SS_USE_INLINE) */

#endif /* RS0PLA_H */
