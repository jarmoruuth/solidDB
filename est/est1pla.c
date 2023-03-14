/*************************************************************************\
**  source       * tab1pla.c
**  directory    * est
**  description  * Generates a search plan for the index system
**               * for the given query with the given key
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

Future improvements:
-------------------  
When optimizing joins, the SQL interpreter could store the search
plan which then could be used for repeated searches. This would,
of course, require parametrized search plans.

Another optimization can be found from the header of 
form_range_constraint.

More data types could be added.


Implementation:
--------------
The module can be run in a test mode by calling the function
tb_est_initialize_test(nrows).

There is a document in file tabkeyse.doc in the documentation directory
which may be illuminating.

The algorithm is the following:

1. First we check if there are constraints which are always false.
An example of such is produced from I = 1.5 if I is of integer type.

2. Then we form the search range. We loop through key parts as long
as we can deduce there is an equality constraint on the key part.
A special case is a key part which is constant, e.g., key id.
When there is no equality constraint any more, we try to form
a range constraint on the next key part. E.g., if the key contains
key id (313) and attributes A and B, in this order, and we have
constraints A <= 5, A >= 5, B < 7, the range end becomes
(313, 5, 7) and range start (313, 5, SQL-NULL). Both the end and the start
are open, i.e., do not contain end points. Note that SQL-NULL is
the smallest possible value in a field and any
SQL constraint except IS NULL excludes SQL-NULLS. If the constraint
above would be B <= 7, then the range end would be
(313, 5, 7 conc "\0"), where the last field is zero byte concatenated
to 7.

3. We then extract the rest of constraints which apply to the
key and possibly data tuple if the key is a non-clustering
key and dereferencing is necessary.

4. We form the physical select list which applies either to the key or the
data tuple.

For the comparison order of different field values see the function
header of compare_limits below.


Limitations:
-----------
The use of descending attributes in keys is not supported.
The only data types supported are INT, FLOAT and CHAR.

Error handling:
--------------
No legal errors can be noticed here. All errors cause an assert.

Objects used:
------------


Preconditions:
-------------
The data type conversions have to be performed first on the
constraints.

Multithread considerations:
--------------------------
In test mode the previous plan is stored to a global variable and
left as garbage.

Example:
-------
The main function is tb_pla_create_search_plan.

**************************************************************************
#endif /* DOCUMENTATION */

/* The mystic parameter cd in the functions is a freely defined
parameter not currently in use. */

#include <ssstdio.h>
#include <math.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <uti0vtpl.h>

#include <su0list.h>
#include <su0parr.h>
#include <su0prof.h>
#include <su0vers.h>

#ifdef SS_UNICODE_DATA
#include <sswctype.h>
#include <su0wlike.h>
#endif /* SS_UNICODE_DATA */
#include <rs0cons.h>
#include <rs0key.h>
#include <rs0aval.h>
#include <rs0atype.h>
#include <rs0order.h>
#include <rs0relh.h>
#include <rs0types.h>
#include <rs0pla.h>
#include <rs0sysi.h>

#include <dbe0type.h>

#include "est1pla.h"

/* Flag values for limit_t::lim_flags */
#define LIM_VALUE_NULL      SU_BFLAG_BIT(0)
#define LIM_VALUE_INFINITE  SU_BFLAG_BIT(1)
#define LIM_COPIED_VALUE    SU_BFLAG_BIT(2)
#define LIM_CLOSED          SU_BFLAG_BIT(3)
#ifdef SS_UNICODE_DATA
#define LIM_UNI4CHAR        SU_BFLAG_BIT(4)
#endif /* SS_UNICODE_DATA */
#define LIM_ISSET           SU_BFLAG_BIT(5)
#define LIM_DESC            SU_BFLAG_BIT(6)

#define MAX_LOCAL_RANGE             10
#define PLA_MAX_FIXED_STORAGELEN    240

/* Single key part range endpoint struct */
typedef struct limit_st {
        rs_atype_t* lim_value_type;     /* Type of value */
        rs_aval_t*  lim_value;          /* This and type are NULL
                                           in the case where
                                           lim_value_null == TRUE or
                                           lim_value_infinite == TRUE.
                                        */
        su_bflag_t  lim_flags;
} limit_t;


/* Single key part range struct */
typedef struct range_st {
        limit_t     rng_lower_limit;    /* This is the lower limit of
                                           the search range.
                                        */

        limit_t     rng_upper_limit;    /* This is the upper limit of
                                           the search range.
                                        */
} range_t;

/* Search plan structure */
typedef struct {
        bool        pla_isconsistent;   /* FALSE if the search range is empty,
                                           i.e., the query is inconsistent */
        dynvtpl_t   pla_range_start;    /* start of the search range */
        bool        pla_start_closed;   /* TRUE if the start closed */
        dynvtpl_t   pla_range_end;      /* end of the search range */
        bool        pla_end_closed;     /* TRUE if the end closed */
        su_list_t*  pla_key_constraints;  /* constraints for key entry */
        su_list_t*  pla_data_constraints; /* constraints for data tuple
                                             NULL if no constraints */
        su_list_t*  pla_constraints;    /* All constraints for this search,
                                           in rs_cons objects. */
        su_list_t*  pla_tuple_reference;  /* rules for building the data
                                             tuple reference. NULL if no
                                             dereferencing is necessary */
        int*        pla_select_list;    /* key part numbers of the
                                           selected columns */
        bool        pla_dereference;      /* TRUE if data has to be fetched
                                           by dereferencing to the data
                                           tuple */
        long        pla_nsolved_range_cons; /* number of constraints solved
                                           in the search range constraint */
        long        pla_nsolved_key_cons; /* number of constraints solved
                                           in the index key (with the range
                                           constraints excluded) */
        long        pla_nsolved_data_cons; /* number of constraints solved
                                           on the data tuple */
} tb_pla_t;

#define     UPPER           1  /* flags for the compare_limits function */
#define     LOWER           2
#define     UPPER_LOWER     3

#define     V_INFINITE      100     /* constants for compare_limits */
#define     V_NULL          (-200)
#define     V_BIGFINITE     50
#define     V_SMALLFINITE   (-50)
#define     V_EPSILON       3  
#define     V_UNDEFINED     1000

typedef enum {
        RANGE_INIT      = 0,
        RANGE_EQUAL     = 1,
        RANGE_VECTOR    = 2,
        RANGE_NONE      = 3
} range_state_t;

static void free_limit_struct(
        rs_sysi_t*  cd,
        limit_t*    limit);

static void set_limit_infinite(
        rs_sysi_t*  cd,
        limit_t*    limit,
        rs_atype_t* value_type);

static void set_limit_infinite_notype(
        rs_sysi_t*  cd,
        limit_t*    limit);

static void set_limit_null(
        rs_sysi_t*  cd,
        limit_t*    limit,
        rs_atype_t* value_type,
        bool        closed);

static void set_limit_null_notype(
        rs_sysi_t*  cd,
        limit_t*    limit,
        bool        closed);

static void set_limit_finite(
        rs_sysi_t*  cd,
        limit_t*    limit,
        rs_aval_t*  value,
        rs_atype_t* value_type,
        bool        closed);

static void invert_descending_ranges(
        rs_sysi_t*  cd,
        rs_key_t*   key,
        range_t*    range,
        rs_ano_t    last_match);

static int compare_limits(
        rs_sysi_t*  cd,
        int         flag,
        limit_t*    limit1,
        limit_t*    limit2);

static void make_range_start(
        rs_sysi_t*      cd,
        range_t*        search_range,
        rs_ano_t        last_match,
        dynvtpl_t*      range_start,
        bool*           range_start_closed);

static void make_range_end(
        rs_sysi_t*      cd,
        range_t*        search_range,
        rs_ano_t        last_match,
        dynvtpl_t*      range_end,
        bool*           range_end_closed,
        bool            estimate);

static su_list_t* form_key_constraints(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        rs_key_t*   key,
        su_list_t*  key_constraint_list,
        su_list_t*  constraint_list,
        bool isclust);

#ifndef SS_UNICODE_DATA
static char* like_fixed_prefix(
        rs_sysi_t* cd,
        char*   str,
        int     esc);

static char* like_increment_by_one(
        rs_sysi_t* cd,
        char*   str);
#endif /* !SS_UNICODE_DATA */

/* The following flag is set to TRUE in the test version. */
static bool         pla_test_version_on = FALSE;

static rs_pla_t*    pla_test_plan;  /* The previous search plan
                                       retained for testing purposes */

/*##**********************************************************************\
 * 
 *		tb_pla_initialize_test
 * 
 * Initializes the module to test mode where the previous plan object
 * is always retained.
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void tb_pla_initialize_test(void)
{
        pla_test_version_on = TRUE;
}

/*##**********************************************************************\
 * 
 *		tb_pla_get_plan
 * 
 * In test mode, gives the previous plan object.
 * 
 * Parameters : 	 - none
 * 
 * Return value : out, ref
 *                  the plan object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_pla_t* tb_pla_get_plan(void)
{
        return(pla_test_plan);
}

#ifdef SS_UNICODE_DATA

/*#***********************************************************************\
 * 
 *		like_increment_by_one_char1
 * 
 * Increments last character of fixed prefix of 1-byte char
 * string. If the last char is 0xff the second last char is
 * incremented instead. if all chars are 0xff, none is incremented
 * and FALSE is returned
 * 
 * Parameters : 
 * 
 *	prefix - use
 *		fixed prefix of a like pattern
 *		
 *	prefixlen - in
 *		length of fixed prefix
 *		
 * Return value :
 *      TRUE - success
 *      FALSE - all characters were 0xff
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool like_increment_by_one_char1(
        ss_char1_t* prefix,
        size_t prefixlen)
{
        while (prefixlen--) {
            if ((ss_byte_t)prefix[prefixlen] != (ss_byte_t)~0) {
                (prefix[prefixlen])++;
                return (TRUE);
            }
        }
        return (FALSE);
}

/*#***********************************************************************\
 * 
 *		like_increment_by_one_char2
 * 
 * Increments last character of fixed prefix of 2-byte char
 * string. If the last char is 0xffff the second last char is
 * incremented instead. if all chars are 0xffff, none is incremented
 * and FALSE is returned
 * 
 * Parameters : 
 * 
 *	prefix - use
 *		fixed prefix of a like pattern
 *		
 *	prefixlen - in
 *		length of fixed prefix
 *		
 * Return value :
 *      TRUE - success
 *      FALSE - all characters were 0xffff
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool like_increment_by_one_char2(
        ss_char2_t* prefix,
        size_t prefixlen)
{
        while (prefixlen--) {
            if (prefix[prefixlen] != (ss_char2_t)~0) {
                (prefix[prefixlen])++;
                return (TRUE);
            }
        }
        return (FALSE);
}

/*#***********************************************************************\
 * 
 *		form_like_interval
 * 
 * Forms interval for like pattern
 * 
 * Parameters : 
 * 
 *	cd - use
 *		client data
 *		
 *	lower_limit - use
 *		lower limit structure
 *		
 *	upper_limit - use
 *		upper limit structure
 *		
 *	cons_atype - in, use
 *		constraint attribute type
 *		
 *	cons_aval - in, use
 *		constraint attribute value
 *		
 *	esc - in
 *		escape character of the like query
 *          or RS_CONS_NOESCCHAR if none
 *		
 * Return value :
 *      TRUE if a range constraint could be formed or
 *      FALSE if the like pattern did not contain any fixed prefix
 *      or the prefix was all 0xFF bytes
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool form_like_interval(
        rs_sysi_t*      cd,
        limit_t*        lower_limit,
        limit_t*        upper_limit,
        rs_atype_t*     cons_atype,
        rs_aval_t*      cons_aval,
        int             esc)
{
        ss_char2_t buf2[64];
#define buf1 ((ss_char1_t*)&buf2[0])
        void* p_data;
        va_index_t datalen;
        rs_datatype_t dt;
        va_t* va;
        size_t prefixlen;
        rs_aval_t* aval;
        bool is_range_constraint = TRUE;
        RS_AVALRET_T succp;

        if (rs_aval_isnull(cd, cons_atype, cons_aval)) {
            set_limit_null(cd, lower_limit, cons_atype, TRUE);
            set_limit_null(cd, upper_limit, cons_atype, TRUE);
            return(TRUE);
        }

        va = rs_aval_va(cd, cons_atype, cons_aval);
        p_data = va_getdata(va, &datalen);
        dt = rs_atype_datatype(cd, cons_atype);
        if (dt == RSDT_CHAR) {
            ss_char1_t* p_prefix1;
            ss_char1_t* p_buf1;
            if (datalen <= sizeof(buf2)/sizeof(buf1[0])) {
                p_buf1 = buf1;
            } else {
                p_buf1 = NULL;
            }
            p_prefix1 =
                su_slike_fixedprefix(
                    p_data,
                    datalen-1,
                    esc,
                    &prefixlen,
                    p_buf1);
            if (p_prefix1 != NULL) {
                ss_dassert(prefixlen > 0);
                aval = rs_aval_create(cd, cons_atype);
                succp = rs_aval_set8bitcdata_ext(
                            cd,
                            cons_atype, aval,
                            p_prefix1, prefixlen,
                            NULL);
                ss_dassert(succp != RSAVR_FAILURE);
                set_limit_finite(cd, lower_limit, aval, cons_atype, TRUE);
                /* ownership of *aval moved to lower_limit */
                SU_BFLAG_SET(lower_limit->lim_flags, LIM_COPIED_VALUE);

                if (like_increment_by_one_char1(p_prefix1, prefixlen)) {
                    aval = rs_aval_create(cd, cons_atype);
                    succp = rs_aval_set8bitcdata_ext(
                                cd,
                                cons_atype, aval,
                                p_prefix1, prefixlen,
                                NULL);
                    ss_dassert(succp != RSAVR_FAILURE);
                    set_limit_finite(cd, upper_limit, aval, cons_atype, FALSE);
                    /* ownership of *aval moved to upper_limit */
                    SU_BFLAG_SET(upper_limit->lim_flags, LIM_COPIED_VALUE);
                } else {
                    set_limit_infinite(cd, upper_limit, cons_atype);
                }
                if (p_prefix1 != buf1) {
                    SsMemFree(p_prefix1);
                }
            } else {
                set_limit_null(cd, lower_limit, cons_atype, TRUE);
                set_limit_infinite(cd, upper_limit, cons_atype);
                is_range_constraint = FALSE;
            }
        } else {
            ss_char2_t* p_prefix2;
            ss_char2_t* p_buf2;

            ss_dassert(dt == RSDT_UNICODE);

            ss_dassert(datalen & 1);
            datalen /= sizeof(ss_char2_t);
            if (datalen < sizeof(buf2)/sizeof(buf2[0])) {
                p_buf2 = buf2;
            } else {
                p_buf2 = NULL;
            }    
            p_prefix2 =
                su_wlike_fixedprefix(
                    p_data,
                    datalen,
                    esc,
                    &prefixlen,
                    p_buf2,
                    TRUE, /* input is in va format */
                    FALSE); /* output is in native format */
            if (p_prefix2 != NULL) {
                ss_dassert(prefixlen > 0);
                aval = rs_aval_create(cd, cons_atype);
                succp = rs_aval_setwdata_ext(
                            cd,
                            cons_atype, aval,
                            p_prefix2, prefixlen,
                            NULL);
                ss_dassert(succp != RSAVR_FAILURE);
                set_limit_finite(cd, lower_limit, aval, cons_atype, TRUE);
                /* ownership of *aval moved to lower_limit */
                SU_BFLAG_SET(lower_limit->lim_flags, LIM_COPIED_VALUE);
                if (like_increment_by_one_char2(p_prefix2, prefixlen)) {
                    aval = rs_aval_create(cd, cons_atype);
                    succp = rs_aval_setwdata_ext(
                                cd,
                                cons_atype, aval,
                                p_prefix2, prefixlen,
                                NULL);
                    ss_dassert(succp != RSAVR_FAILURE);
                    set_limit_finite(cd, upper_limit, aval, cons_atype, FALSE);
                    /* ownership of *aval moved to upper_limit */
                    SU_BFLAG_SET(upper_limit->lim_flags, LIM_COPIED_VALUE);
                } else {
                    set_limit_infinite(cd, upper_limit, cons_atype);
                }
                if (p_prefix2 != buf2) {
                    SsMemFree(p_prefix2);
                }
            } else {
                set_limit_null(cd, lower_limit, cons_atype, TRUE);
                set_limit_infinite(cd, upper_limit, cons_atype);
                is_range_constraint = FALSE;
            }
        }
#undef buf1
        return (is_range_constraint);
}


/*#***********************************************************************\
 * 
 *		create_va_to_range_start
 * 
 * Creates a dynva to be the range start position
 * by creating a modified version of input va.
 * The original va is UNICODE and the range start
 * va must be a CHAR. That means we have to create a va
 * which is lexically smaller than or equal to original va
 * 
 * Parameters : 
 * 
 *	p_dynva - use
 *		va to be modified
 *		
 *      va - in, use
 *		input va
 *
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void create_va_to_range_start(dynva_t* p_dynva, va_t* va)
{
        ss_char2_t* p_data;
        ss_char1_t* p_data_out;
        va_index_t datalen;
        va_index_t datalen_out;
        va_index_t i;

        p_data = va_getdata(va, &datalen);
        ss_dassert(datalen & 1);
        datalen /= 2;
        ss_dassert(datalen > 0);
        dynva_setdata(p_dynva, NULL, datalen + 1);
        p_data_out = va_getdata(*p_dynva, &datalen_out);
        for (i = 0; i < datalen; i++, p_data++, p_data_out++) {
            ss_char2_t c;

            c = SS_CHAR2_LOAD(p_data);
            if (!ss_isw8bit(c)) {
                break;
            }
            *p_data_out = (ss_char1_t)c;
        }
        for (; i <= datalen; i++, p_data_out++) {
            *p_data_out = (ss_char1_t)~0;
        }
}

/*#***********************************************************************\
 * 
 *		create_va_to_range_end
 * 
 * Creates a dynva to be the range end position
 * by creating a modified version of input va.
 * The original va is UNICODE and the range end
 * va must be a CHAR. That means we have to create a va
 * which is lexically greater than or equal to original va
 * 
 * Parameters : 
 * 
 *	p_dynva - use
 *		va to be modified
 *
 *      va - in, use
 *		input va
 *
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void create_va_to_range_end(
        dynva_t* p_dynva,
        va_t* va)
{
        ss_char2_t* p_data;
        ss_char2_t* p_data2;
        ss_char1_t* p_data_out;
        va_index_t datalen;
        va_index_t datalen_out;
        va_index_t i;

        p_data2 = p_data = va_getdata(va, &datalen);
        ss_dassert(datalen & 1);
        datalen /= 2;
        ss_dassert(datalen > 0);
        for (i = 0; i < datalen; i++, p_data++) {
            ss_char2_t c;

            c = SS_CHAR2_LOAD(p_data);
            if (!ss_isw8bit(c)) {
                break;
            }
        }
        ss_dassert(i < datalen);
        datalen = i;
        ss_dassert(datalen > 0);
        for (p_data = p_data2 + i - 1; i; i--, p_data--) {
            ss_char2_t c;

            c = SS_CHAR2_LOAD(p_data);
            if (c != 0x00FF) {
                break;
            }
        }
        datalen = i;
        ss_dassert(datalen > 0);

        dynva_setdata(p_dynva, NULL, datalen);
        p_data_out = va_getdata(*p_dynva, &datalen_out);
        p_data = p_data2;
        for (i = 0; i < datalen; i++, p_data++, p_data_out++) {
            ss_char2_t c;

            c = SS_CHAR2_LOAD(p_data);
            ss_dassert(ss_isw8bit(c));
            *p_data_out = (ss_char1_t)c;
        }
        (p_data_out[-1])++;
        ss_dassert(p_data_out[-1] != '\0');
}


#endif /* SS_UNICODE_DATA */

/*#**********************************************************************\
 * 
 *		form_interval
 * 
 * Takes a search constraint and calculates a corresponding
 * search range on a key part if the constraint is of a suitable type, e.g.,
 * a comparison like GE, EQUAL, or a LIKE match with a fixed prefix
 * in the pattern (like abc%d_). Returns TRUE if the function was able
 * to form a range constraint. If the function returns FALSE,
 * the interval is set to the maximal possible
 * (the interval from SQL-NULL to infinity).
 * If the function returns TRUE, the parameter solved
 * is set to TRUE if the range given expresses the constraint with
 * full strength, e.g., solved is set to FALSE in the case of LIKE,
 * but TRUE in the case of GE.
 * 
 * Parameters : 
 * 
 *	constraint - in, use
 *		search constraint
 *
 *	lower_limit - in, use
 *		lower limit of the search range, if the constraint
 *          is of range type
 *
 *	upper_limit - in, use
 *		upper limit of the search range, if the constraint
 *          is of range type
 *
 *      solved - out
 *		TRUE if the constraint was expressed totally by the
 *          the range
 *
 * 
 * Return value : TRUE if the constraint was of a range type, e.g.,
 *                not RS_RELOP_NOTEQUAL
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool form_interval(
        rs_sysi_t*      cd,
        rs_cons_t*      constraint,
        limit_t*        lower_limit,
        limit_t*        upper_limit,
        bool*           solved,
        range_state_t*  p_range_state,
        int*            p_vectorno)
{
        rs_atype_t*     cons_atype;
        rs_aval_t*      cons_aval;
        bool            is_range_constraint;
        bool            no_max_range = FALSE;
        int             vectorno;
#ifdef SS_UNICODE_DATA
        bool            is_unicode_constraint_for_char_column;

        ss_dassert(constraint && lower_limit && upper_limit);

        is_unicode_constraint_for_char_column =
            rs_cons_isuniforchar(cd, constraint);
#endif /* SS_UNICODE_DATA */

        ss_dassert(constraint && lower_limit && upper_limit);

        cons_atype = rs_cons_atype(cd, constraint);
        cons_aval = rs_cons_aval(cd, constraint);

        if (cons_aval == NULL || rs_cons_isalwaysfalse(cd, constraint)) {
            /* we set the interval as maximal so that it is initialized
               properly for the freeing procedure */
            set_limit_null_notype(cd, lower_limit, TRUE);
            set_limit_infinite_notype(cd, upper_limit);
            *solved = FALSE;
            return(FALSE);
        }

        *solved = TRUE;
#ifdef SS_UNICODE_DATA
        if (is_unicode_constraint_for_char_column) {
            *solved = FALSE;
        }
#endif /* SS_UNICODE_DATA */
        is_range_constraint = TRUE;
        switch (rs_cons_relop(cd, constraint)) {
            case RS_RELOP_EQUAL:
                ss_dprintf_4(("form_interval:RS_RELOP_EQUAL, set limit finite\n"));
                set_limit_finite(cd, lower_limit,
                                 cons_aval,
                                 cons_atype,
                                 TRUE);
                set_limit_finite(cd, upper_limit,
                                 cons_aval,
                                 cons_atype,
                                 TRUE);
#ifdef SS_UNICODE_DATA
                if (is_unicode_constraint_for_char_column) {
                    SU_BFLAG_SET(lower_limit->lim_flags, LIM_UNI4CHAR);
                    SU_BFLAG_SET(upper_limit->lim_flags, LIM_UNI4CHAR);
                }
#endif /* SS_UNICODE_DATA */
                if (*p_range_state < RANGE_EQUAL) {
                    *p_range_state = RANGE_EQUAL;
                }
                break;
            case RS_RELOP_GT_VECTOR:
                vectorno = rs_cons_getvectorno(cd, constraint);
                if (*p_range_state <= RANGE_VECTOR
                    && (vectorno == -1 || vectorno == *p_vectorno))
                {
                    /* Accept this vector constraint to the range.
                     */
                    *p_range_state = RANGE_VECTOR;
                    (*p_vectorno)++;
                } else {
                    *p_range_state = RANGE_NONE;
                    no_max_range = TRUE;
                    break;
                }
            case RS_RELOP_GT:
                set_limit_finite(cd, lower_limit,
                                 cons_aval,
                                 cons_atype,
                                 FALSE);
                set_limit_infinite(cd, upper_limit, cons_atype);
                *solved = FALSE; /* JarmoR added, Sep 7, 1994, must be
                                    checked at run time */
#ifdef SS_UNICODE_DATA
                if (is_unicode_constraint_for_char_column) {
                    SU_BFLAG_SET(lower_limit->lim_flags, LIM_UNI4CHAR);
                }
#endif /* SS_UNICODE_DATA */
                break;
            case RS_RELOP_GE_VECTOR:
                vectorno = rs_cons_getvectorno(cd, constraint);
                if (*p_range_state <= RANGE_VECTOR
                    && (vectorno == -1 || vectorno == *p_vectorno))
                {
                    /* Accept this vector constraint to the range.
                     */
                    *p_range_state = RANGE_VECTOR;
                    (*p_vectorno)++;
                } else {
                    *p_range_state = RANGE_NONE;
                    no_max_range = TRUE;
                    break;
                }
            case RS_RELOP_GE:
                set_limit_finite(cd, lower_limit,
                                 cons_aval,
                                 cons_atype,
                                 TRUE);
                set_limit_infinite(cd, upper_limit, cons_atype);
#ifdef SS_UNICODE_DATA
                if (is_unicode_constraint_for_char_column) {
                    SU_BFLAG_SET(lower_limit->lim_flags, LIM_UNI4CHAR);
                }
#endif /* SS_UNICODE_DATA */
                break;
            case RS_RELOP_LT_VECTOR:
                vectorno = rs_cons_getvectorno(cd, constraint);
                if (*p_range_state <= RANGE_VECTOR
                    && (vectorno == -1 || vectorno == *p_vectorno))
                {
                    /* Accept this vector constraint to the range.
                     */
                    *p_range_state = RANGE_VECTOR;
                    (*p_vectorno)++;
                } else {
                    *p_range_state = RANGE_NONE;
                    no_max_range = TRUE;
                    break;
                }
            case RS_RELOP_LT:
                set_limit_infinite(cd, lower_limit, cons_atype);
                set_limit_finite(cd, upper_limit,
                                 cons_aval,
                                 cons_atype,
                                 FALSE);
                *solved = FALSE; /* JarmoR added, Sep 7, 1994, must be
                                    checked at run time */
#ifdef SS_UNICODE_DATA
                if (is_unicode_constraint_for_char_column) {
                    SU_BFLAG_SET(upper_limit->lim_flags, LIM_UNI4CHAR);
                }
#endif /* SS_UNICODE_DATA */
                break;
            case RS_RELOP_LE_VECTOR:
                vectorno = rs_cons_getvectorno(cd, constraint);
                if (*p_range_state <= RANGE_VECTOR
                    && (vectorno == -1 || vectorno == *p_vectorno))
                {
                    /* Accept this vector constraint to the range.
                     */
                    *p_range_state = RANGE_VECTOR;
                    (*p_vectorno)++;
                } else {
                    *p_range_state = RANGE_NONE;
                    no_max_range = TRUE;
                    break;
                }
            case RS_RELOP_LE:
                set_limit_infinite(cd, lower_limit, cons_atype);
                set_limit_finite(cd, upper_limit,
                                 cons_aval,
                                 cons_atype,
                                 TRUE);
#ifdef SS_UNICODE_DATA
                if (is_unicode_constraint_for_char_column) {
                    SU_BFLAG_SET(upper_limit->lim_flags, LIM_UNI4CHAR);
                }
#endif /* SS_UNICODE_DATA */
                break;
            case RS_RELOP_LIKE:
#ifdef SS_UNICODE_DATA
                is_range_constraint =
                    form_like_interval(
                        cd,
                        lower_limit,
                        upper_limit,
                        cons_atype,
                        cons_aval,
                        rs_cons_escchar(cd, constraint));
                *solved = FALSE;
                if (is_unicode_constraint_for_char_column
                &&  is_range_constraint)
                {
                    SU_BFLAG_SET(lower_limit->lim_flags, LIM_UNI4CHAR);
                    SU_BFLAG_SET(upper_limit->lim_flags, LIM_UNI4CHAR);
                }
#else /* SS_UNICODE_DATA */
                atype = cons_atype;
                /* Calculate the fixed start of the match pattern */
                like_fix_str = like_fixed_prefix(cd, 
                       rs_aval_getasciiz(cd, atype,
                                         cons_aval),
                       rs_cons_escchar(cd, constraint));
                if (like_fix_str != NULL) {
                    /* If there was fixed start, increment the last char by
                       one, if possible, else this returns NULL. */
                    like_fix_str_up =
                        like_increment_by_one(cd, like_fix_str);
                }
                if (like_fix_str != NULL && like_fix_str_up != NULL) {
                    /* The fixed start was such that we can make
                       a range constraint. */
                    aval = rs_aval_create(cd, atype);
                    rs_aval_setasciiz(cd, atype, aval, like_fix_str);
                    set_limit_finite(cd, lower_limit,
                                    aval,
                                    atype,
                                    TRUE);
                    /* ownership of *aval moved to lower_limit */
                    SU_BFLAG_SET(lower_limit->lim_flags, LIM_COPIED_VALUE);
                    /* we may reuse aval because the above allocated object
                       already hangs in lower_limit */
                    aval = rs_aval_create(cd, atype);
                    rs_aval_setasciiz(cd, atype, aval, like_fix_str_up);
                    set_limit_finite(cd, upper_limit,
                                    aval,
                                    atype,
                                    FALSE);
                    /* the following notice tells us to free the string
                       when freeing the limit struct.
                     */
                    SU_BFLAG_SET(upper_limit->lim_flags, LIM_COPIED_VALUE);
                    *solved = FALSE;
                    SsMemFree(like_fix_str);
                    SsMemFree(like_fix_str_up);
                } else {
                    /* There was no fixed start in the pattern. */
                    if (like_fix_str != NULL) {
                        SsMemFree(like_fix_str);
                    }
                    /* we set the interval as maximal
                    so that it is initialized
                    properly for the freeing procedure */
                    set_limit_null(cd, lower_limit, cons_atype, TRUE);
                    set_limit_infinite(cd, upper_limit, cons_atype);
                    is_range_constraint = FALSE;
                    *solved = FALSE;
                }
#endif /* SS_UNICODE_DATA */
                break;
            case RS_RELOP_ISNULL:
                set_limit_null(cd, lower_limit, cons_atype, TRUE);
                set_limit_null(cd, upper_limit, cons_atype, TRUE);
                break;
            case RS_RELOP_ISNOTNULL:
                set_limit_infinite(cd, lower_limit, cons_atype);
                set_limit_infinite(cd, upper_limit, cons_atype);
                break;
            default:
                no_max_range = TRUE;
                break;
        }
        if (no_max_range) {
            /* we set the interval as maximal so that it is initialized
               properly for the freeing procedure */
            set_limit_null(cd, lower_limit, cons_atype, TRUE);
            set_limit_infinite(cd, upper_limit, cons_atype);
            is_range_constraint = FALSE;
            *solved = FALSE;
        }
        return(is_range_constraint);
}

#ifndef SS_UNICODE_DATA
/*#**********************************************************************\
 * 
 *		like_fixed_prefix
 * 
 * Returns the fixed prefix of a LIKE match string for a range search.
 * E.g., in abc%d_e, the fixed prefix is abc. If the fixed prefix
 * is empty, returns NULL, in which case the LIKE constraint is not
 * suited for range search.
 * 
 * Parameters : 
 * 
 *	str - in, use
 *		the LIKE match pattern
 *
 *	esc - in, use
 *		integer code of escape character in the pattern,
 *          if this is RS_CONS_NOESCCHAR, no escape character
 *
 * 
 * Output params:    
 * 
 * Return value : out, give
 *                the fixed prefix, if it is empty returns NULL
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static char* like_fixed_prefix(cd, str, esc)
        rs_sysi_t* cd;
        char*   str;
        int     esc;
{
        bool    finished;
        char*   strptr;
        char*   res;
        char*   resptr;

        SS_NOTUSED(cd);
        
        ss_dassert(str);
        
        res = SsMemAlloc(strlen(str) + 1);
        resptr = res;
        strptr = str;
        finished = FALSE;

        while (!finished) {
            if (esc != RS_CONS_NOESCCHAR && *strptr == (char)esc) {
                strptr++;
                *resptr = *strptr;
                resptr++;
                strptr++;
            } else if (*strptr != '%' && *strptr != '_' && *strptr != '\0') {
                *resptr = *strptr;
                resptr++;
                strptr++;
            } else {
                finished = TRUE;
            }
        }
        if (resptr != res) {
            *resptr = '\0';
            return(res);
        } else {
            SsMemFree(res);
            return(NULL);
        }
}

/*#**********************************************************************\
 * 
 *		like_increment_by_one
 * 
 * Forms a successor of a fixed prefix of a LIKE match string.
 * E.g., a successor of "abc" is "abd". In the case of
 * a string of a form "char(255)char(255)..." it is not possible to
 * form the successor. In that case returns NULL.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	str - in, use
 *		the fixed prefix
 *
 * 
 * Output params: 
 * 
 * Return value : out, give
 *                a successor string, NULL if not possible to form
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static char* like_increment_by_one(cd, str)
        rs_sysi_t* cd;
        char*   str;
{
        int     i;
        char*   ret;

        SS_NOTUSED(cd);
        ss_dassert(str);

        i = strlen(str) - 1;

        while (i >= 0 && str[i] == '\xFF') {
            i--;
        }

        if (i < 0) {
            return(NULL);
        }

        ret = SsMemAlloc(strlen(str) + 1);

        strcpy(ret, str);
        ret[i]++;
        ret[i + 1] = '\0';
        return(ret);
}

#endif /* !SS_UNICODE_DATA */

/*#***********************************************************************\
 * 
 *		free_limit_struct
 * 
 * Frees a limit struct
 * 
 * Parameters : in, take
 *              limit - pointer to a limit struct
 * 
 * Output params: 
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void free_limit_struct(cd, limit)
        rs_sysi_t*  cd;
        limit_t*    limit;
{
        SS_NOTUSED(cd);
        ss_dassert(SU_BFLAG_TEST(limit->lim_flags, LIM_ISSET));

        if (SU_BFLAG_TEST(limit->lim_flags, LIM_COPIED_VALUE)) {
            /* aval owned by the limit data structure */
            rs_aval_free(cd, limit->lim_value_type, limit->lim_value);
        }
}

/*#***********************************************************************\
 * 
 *		set_limit_infinite
 * 
 * Sets a limit at an infinite or negative infinite value. Note that
 * the limit is then always open (as there are no infinities in SQL).
 * See the compare_limits function for the ordering of infinities
 * and SQL-NULL.
 *
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	limit - in, use
 *		limit struct
 *
 * 
 *	value_type - in, hold
 *		value type, needed in case of desc index and NULL
 *
 * 
 * Output params: 
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void set_limit_infinite(cd, limit, value_type)
        rs_sysi_t*  cd;
        limit_t*    limit;
        rs_atype_t* value_type;
{
        SS_NOTUSED(cd);
        ss_dassert(limit);
        ss_dassert(value_type);

        limit->lim_value            = NULL;
        limit->lim_value_type       = value_type;
        limit->lim_flags = LIM_VALUE_INFINITE;
}

/*#***********************************************************************\
 * 
 *		set_limit_infinite_notype
 * 
 * Same as set_limit_infinite but handling a special case where type is 
 * not knowm. Just for debugging purposes so we can assert that value_type
 * is given.
 *
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	limit - in, use
 *		limit struct
 *
 * 
 * Output params: 
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void set_limit_infinite_notype(
        rs_sysi_t*  cd,
        limit_t*    limit)
{
        SS_NOTUSED(cd);
        ss_dassert(limit);

        limit->lim_value            = NULL;
        limit->lim_value_type       = NULL;
        limit->lim_flags = LIM_VALUE_INFINITE;
}

/*##**********************************************************************\
 * 
 *		set_limit_null
 * 
 * Sets a limit to NULL (of SQL) value.
 * See the compare_limits function for the ordering of infinities
 * and SQL-NULL.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	limit - in, use
 *		limit struct
 *
 *	value_type - in, hold
 *		value type, needed in case of desc index and NULL
 *
 *	closed - in, use
 *		is the limit closed
 *
 * 
 * Output params: 
 * 
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void set_limit_null(cd, limit, value_type, closed)
        rs_sysi_t*  cd;
        limit_t*    limit;
        rs_atype_t* value_type;
        bool        closed;
{
        SS_NOTUSED(cd);
        ss_dassert(limit);
        ss_dassert(value_type);
        
        limit->lim_value            = NULL;
        limit->lim_value_type       = value_type;
        if (closed) {
            limit->lim_flags = LIM_VALUE_NULL | LIM_CLOSED;
        } else {
            limit->lim_flags = LIM_VALUE_NULL;
        }
}

/*#***********************************************************************\
 * 
 *		set_limit_null_notype
 * 
 * Same as set_limit_null but handling a special case where type is 
 * not knowm. Just for debugging purposes so we can assert that value_type
 * is given.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	limit - in, use
 *		limit struct
 *
 *	closed - in, use
 *		is the limit closed
 *
 * 
 * Output params: 
 * 
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void set_limit_null_notype(
        rs_sysi_t*  cd,
        limit_t*    limit,
        bool        closed)
{
        SS_NOTUSED(cd);
        ss_dassert(limit);
        
        limit->lim_value            = NULL;
        limit->lim_value_type       = NULL;
        if (closed) {
            limit->lim_flags = LIM_VALUE_NULL | LIM_CLOSED;
        } else {
            limit->lim_flags = LIM_VALUE_NULL;
        }
}

/*#***********************************************************************\
 * 
 *		set_limit_finite
 * 
 * Sets a limit to a finite non-NULL (of SQL) value.
 * See the compare_limits function for the ordering of infinities
 * and SQL-NULL.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	limit - in, use
 *		limit struct
 *
 *	value - in, hold
 *		value
 *
 *	value_type - in, hold
 *		type
 *
 *	closed - in, use
 *		is the limit closed
 *
 * 
 * Output params: 
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void set_limit_finite(cd, limit, value, value_type, closed)
        rs_sysi_t*  cd;
        limit_t*    limit;
        rs_aval_t*  value;
        rs_atype_t* value_type;
        bool        closed;
{
        SS_NOTUSED(cd);
        ss_dassert(limit);
        ss_dassert(value);
        ss_dassert(value_type);
        
        limit->lim_value            = value;
        limit->lim_value_type       = value_type;

        if (closed) {
            limit->lim_flags = LIM_CLOSED;
        } else {
            limit->lim_flags = 0UL;
        }
}

/*#***********************************************************************\
 * 
 *		limit_value_asctodesc
 * 
 * Converts limit value from ascending to descending format.
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	limit - use
 *		Limit structure.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void limit_value_asctodesc(
        rs_sysi_t* cd,
        limit_t* limit)
{
        if (!SU_BFLAG_TEST(limit->lim_flags, LIM_VALUE_NULL | LIM_VALUE_INFINITE)) {
            ss_dassert(limit->lim_value != NULL);
            if (!SU_BFLAG_TEST(limit->lim_flags, LIM_COPIED_VALUE)) {
                limit->lim_value = rs_aval_copy(
                                        cd,
                                        limit->lim_value_type,
                                        limit->lim_value);
                SU_BFLAG_SET(limit->lim_flags, LIM_COPIED_VALUE);
            }
            rs_aval_asctodesc(
                cd,
                limit->lim_value_type,
                limit->lim_value);
        }
}

#ifdef SS_COLLATION

static void limit_convert_to_collation_key(
        rs_sysi_t*  cd,
        rs_key_t*   key,
        int         kpno,
        limit_t*    limit)
{
        rs_aval_t* new_aval;

        ss_dprintf_4(("limit_convert_to_collation_key\n"));

        if (SU_BFLAG_TEST(limit->lim_flags, LIM_COPIED_VALUE)
            || limit->lim_value == NULL)
        {
            new_aval = limit->lim_value;
        } else {
            new_aval = rs_aval_copy(
                            cd,
                            limit->lim_value_type,
                            limit->lim_value);
        }
        if (new_aval != NULL) {
            rs_aval_convert_to_collation_key(
                    cd,
                    limit->lim_value_type,
                    new_aval,
                    rs_keyp_collation(cd, key, kpno));
            SU_BFLAG_SET(limit->lim_flags, LIM_COPIED_VALUE);
            limit->lim_value = new_aval;
        }
}

#endif /* SS_COLLATION */

/*##**********************************************************************\
 * 
 *		invert_descending_ranges
 * 
 * This function turns the range constraints on descending
 * attributes in the key upside down. As the engine does not
 * handle descending attributes, this is not necessary currently.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *		
 *	key - in, use
 *		  the key used
 *		
 *	range - in, use
 *		    the range
 *		
 *	last_match - in, use
 *		         index of the last key part with a range constraint on it
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void invert_descending_ranges(cd, key, range, last_match)
        rs_sysi_t*  cd;
        rs_key_t*   key;
        range_t*    range;
        rs_ano_t    last_match;
{
        rs_ano_t    i;
        limit_t     limit;

        ss_dprintf_3(("invert_descending_ranges\n"));
        ss_dassert(key);
        ss_dassert(range);

        for (i = 0; i <= last_match; i++) {
#ifdef SS_COLLATION
            if (rs_keyp_parttype(cd, key, i) == RSAT_COLLATION_KEY) {
                limit_convert_to_collation_key(cd, key, i, &range[i].rng_lower_limit);
                limit_convert_to_collation_key(cd, key, i, &range[i].rng_upper_limit);
            }
#endif /* SS_COLLATION */
            if (!rs_keyp_isconstvalue(cd, key, i)
                && !rs_keyp_isascending(cd, key, i)) {

                /* Invert the upper and lower limits.
                 */
                limit = range[i].rng_lower_limit;
                range[i].rng_lower_limit = range[i].rng_upper_limit;
                range[i].rng_upper_limit = limit;

                range[i].rng_lower_limit.lim_flags |= LIM_DESC;
                range[i].rng_upper_limit.lim_flags |= LIM_DESC;

                /* Convert values to descending format.
                 */
                limit_value_asctodesc(cd, &range[i].rng_lower_limit);
                limit_value_asctodesc(cd, &range[i].rng_upper_limit);
            }
        }
}

/*#***********************************************************************\
 * 
 *		alloc_search_range
 * 
 * Allocates an array where the range constraint information is
 * collected.
 * 
 * Input params : 
 * 
 *	n_elements	- in, use
 *                      number of elements of type range_t in the array
 *                    
 * 
 * Output params: 
 * 
 * Return value :  out, give: the search range object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static range_t* alloc_search_range(
        rs_sysi_t*  cd,
        rs_ano_t    n_elements,
        range_t*    local_range_array)
{
        range_t*    search_range;

        SS_NOTUSED(cd);

        if (n_elements > MAX_LOCAL_RANGE) {
            search_range = SsMemCalloc(n_elements, sizeof(range_t));
        } else {
            search_range = local_range_array;
            memset(search_range, '\0', n_elements * sizeof(range_t));
        }

        /* Ensure that initialization done by calloc is same as NULL. */
        ss_dassert(n_elements == 0 || !SU_BFLAG_TEST(search_range[0].rng_lower_limit.lim_flags, LIM_ISSET));
        ss_dassert(n_elements == 0 || !SU_BFLAG_TEST(search_range[0].rng_upper_limit.lim_flags, LIM_ISSET));

        return(search_range);
}

/*#***********************************************************************\
 * 
 *		free_search_range
 * 
 * Free the search range object
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *		
 *	search_range - in, take
 *                     the search range object
 *		
 *		
 *	n_elements - in, use
 *		         the number of elements in the array
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void free_search_range(
        rs_sysi_t*  cd,
        range_t*    search_range,
        rs_ano_t    n_elements)
{
        rs_ano_t    i;

        ss_dassert(search_range);

        for (i = 0; i < n_elements; i++) {
            if (SU_BFLAG_TEST(search_range[i].rng_lower_limit.lim_flags, LIM_ISSET)) {
                free_limit_struct(cd, &search_range[i].rng_lower_limit);
            }
            if (SU_BFLAG_TEST(search_range[i].rng_upper_limit.lim_flags, LIM_ISSET)) {
                free_limit_struct(cd, &search_range[i].rng_upper_limit);
            }
        }

        if (n_elements > MAX_LOCAL_RANGE) {
            SsMemFree(search_range);
        }
}

/*#***********************************************************************\
 * 
 *		compare_limits
 * 
 * Compares interval limits. As the comparison algorithm differs
 * depending on the type of the limits, the types must be
 * specified by flag. In comparison we have the convention
 *
 * SQL-NULL < -infinite < any finite non-SQL-NULL value < infinite.
 *
 * The function returns 1 if limit1 > limit2, 0 if limit1 == limit2
 * and -1 if limit1 < limit2. Suppose we have two limits L1 and L2,
 * whose suprema or infima are E1 and E2. If L1 is an upper end,
 * we subtract an infinitely small epsilon from E1 if the limit is
 * open, to get E1'. In the case of an open lower end we add epsilon.
 * Let us do similarly for L2. Now we define, e.g., L1 < L2 iff
 * E1' < E2'.
 * 
 * Parameters : 
 * 
 *	flag - in, use
 *		types of limits: UPPER, LOWER or UPPER_LOWER
 *
 *	limit1 - in, use
 *		a limit of a range
 *
 *	limit2 - in, use
 *		a limit of a range
 *
 * 
 * Output params: 
 * 
 * Return value : See above
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static int compare_limits(cd, flag, limit1, limit2)
        rs_sysi_t*  cd;
        int         flag;
        limit_t*    limit1;
        limit_t*    limit2;
{
        int     lim1; /* These numeric values we use as help when we */
        int     lim2; /* compare the limits, these correspond to E1'
                         and E2' above. */
        int     lim1_upper; /* this is 1 if limit1 is upper end, else -1 */
        int     lim2_upper; /* see above */

        ss_dassert(limit1);
        ss_dassert(limit2);

        /* check if lim1 is an upper end of an interval */
        if (flag == UPPER || flag == UPPER_LOWER) {
            lim1_upper = 1;
        } else {
            lim1_upper = -1;
        }
        /* check if lim2 is an upper end of an interval */
        if (flag == UPPER) {
            lim2_upper = 1;
        } else {
            lim2_upper = -1;
        }

        /* map the limits to integers for easier comparison */
        /* NUMEROLOGY STARTS HERE */
        lim1 = V_UNDEFINED;
        lim2 = V_UNDEFINED;
        /* treat the case where a limit is infinite */
        if (SU_BFLAG_TEST(limit1->lim_flags, LIM_VALUE_INFINITE)) {
            lim1 = V_INFINITE * lim1_upper;
        }
        if (SU_BFLAG_TEST(limit2->lim_flags, LIM_VALUE_INFINITE)) {
            lim2 = V_INFINITE * lim2_upper;
        }
        /* treat the case where a limit is NULL */
        if (SU_BFLAG_TEST(limit1->lim_flags, LIM_VALUE_NULL)) {
            lim1 = V_NULL;
        }
        if (SU_BFLAG_TEST(limit2->lim_flags, LIM_VALUE_NULL)) {
            lim2 = V_NULL;
        }
        /* now, if a limit is V_UNDEFINED, it means that
           its value is finite not NULL */
        if (lim1 != V_UNDEFINED && lim2 == V_UNDEFINED) {
            lim2 = V_BIGFINITE;
        } else if (lim1 == V_UNDEFINED && lim2 != V_UNDEFINED) {
            lim1 = V_BIGFINITE;
        /* in the two above cases we already know the order of limits,
            else we have to compare */
        } else if (lim1 == V_UNDEFINED && lim2 == V_UNDEFINED) {
#ifdef SS_UNICODE_DATA
            int cmp;
            bool succp;

            cmp = rs_aval_cmp3_nullallowed(
                    cd,
                    limit1->lim_value_type,
                    limit1->lim_value,
                    limit2->lim_value_type,
                    limit2->lim_value,
                    &succp,
                    NULL);
            ss_assert(succp);
            if (cmp < 0) {
                lim1 = V_SMALLFINITE;
                lim2 = V_BIGFINITE;
            } else if (cmp > 0) {
                lim1 = V_BIGFINITE;
                lim2 = V_SMALLFINITE;
            } else {
                lim1 = V_BIGFINITE;
                lim2 = V_BIGFINITE;
            }
#else /* SS_UNICODE_DATA */
            bool    gt;
            bool    lt;

            gt = rs_aval_cmp_simple(
                    cd,
                    limit1->lim_value_type,
                    limit1->lim_value,
                    limit2->lim_value_type,
                    limit2->lim_value,
                    RS_RELOP_GT);
            lt = rs_aval_cmp_simple(
                    cd,
                    limit1->lim_value_type,
                    limit1->lim_value,
                    limit2->lim_value_type,
                    limit2->lim_value,
                    RS_RELOP_LT);
            
            if (lt) {
                lim1 = V_SMALLFINITE;
                lim2 = V_BIGFINITE;
            } else if (gt) {
                lim1 = V_BIGFINITE;
                lim2 = V_SMALLFINITE;
            } else {
                lim1 = V_BIGFINITE;
                lim2 = V_BIGFINITE;
            }
#endif /* SS_UNICODE_DATA */
        }
        /* if a limit is open, we have to add or subtract epsilon
            depending on if the limit is upper or lower end */
        if (!SU_BFLAG_TEST(limit1->lim_flags, LIM_CLOSED)) {
            lim1 = lim1 - V_EPSILON * lim1_upper;
        }
        if (!SU_BFLAG_TEST(limit2->lim_flags, LIM_CLOSED)) {
            lim2 = lim2 - V_EPSILON * lim2_upper;
        }

        /* NUMEROLOGY ENDS HERE */

        if (lim1 < lim2) {
            return(-1);
        } else if (lim1 > lim2) {
            return(1);
        } else {
            return(0);
        }
}

#ifdef SS_DEBUG

static void pla_print_limit(rs_sysi_t* cd, limit_t* limit, char* txt)
{
        char* p;

        if (limit->lim_value_type == NULL && limit->lim_value == NULL) {
            ss_dprintf_4(("%s: atype and aval NULL\n", txt));
            return;
        }
        if (limit->lim_value == NULL) {
            ss_dprintf_4(("%s: aval NULL\n", txt));
            return;
        }
        p = rs_aval_print(cd, limit->lim_value_type, limit->lim_value);
        ss_dprintf_4(("%s: atype=%ld, aval=%ld, value=%s\n", 
            txt, (long)limit->lim_value_type, (long)limit->lim_value, p));
        SsMemFree(p);
}

#endif /* SS_DEBUG */

/*#**********************************************************************\
 * 
 *		form_range_constraint
 * 
 * Calculates the lower and upper limits of a search range. The algorithm
 * is as follows. The function loops through key parts. As long as it can
 * find a pointlike range (i.e, an equality constraint) on key part
 * it loops through. It stops when it finds a non-pointlike range or
 * when it runs out of key parts.
 * 
 * Note2 The following may not be true any more, see last_pointlike
 *       (jarmor xx.xx.19xx)
 * Note that the following subtle optimization is not implemented:
 * if we have constraints KP0 >= 5 and KP1 >=6 on key parts 0 and 1,
 * but no other constraints, we could make the range start [5,6],
 * but at the present the function gives just [5].
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	key - in, use
 *		handle to the key
 *
 *	column_no - in, use
 *		handle to the key
 *
 *	constraint_list - in, use
 *		list of constraints
 *
 *	range_start - out, give
 *		start of the range, NULL if the
 *          function returns FALSE
 *
 *	range_start_closed - out, give
 *		TRUE if start is closed
 *
 *	range_end - out, give
 *		end of the range, NULL if the
 *          function returns FALSE
 *
 *	range_end_closed - out, give
 *		TRUE if end is closed
 * 
 * Return value :
 *      FALSE if the range is empty (contradictory)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool form_range_constraint(
        rs_sysi_t*      cd,
        rs_key_t*       key,
        rs_ttype_t*     ttype,
        rs_ano_t        column_no,
        su_list_t*      constraint_list,
        su_pa_t*        cons_byano,
        dynvtpl_t*      range_start,
        bool*           range_start_closed,
        dynvtpl_t*      range_end,
        bool*           range_end_closed,
        bool*           p_pointlike,
        bool*           p_emptyrange)
{
        range_t*            search_range;
        limit_t             lower_limit;
        limit_t             upper_limit;
        limit_t*            p_lower;
        limit_t*            p_upper;
        bool                constraint_solved;
        bool                is_range_constraint;
        bool                finished;
        bool                contradictory;
        bool                isrange = FALSE;
        rs_ano_t            i;
        rs_ano_t            last_match; /* the last key part with a range
                                           constraint on it */
        rs_ano_t            n_attributes_in_key;
        su_list_node_t*     n;
        rs_cons_t*          constraint;
        rs_atype_t*         constant_type;
        rs_aval_t*          constant_value;
        int                 cmp;
        bool                last_pointlike;
        bool                pointlike;
        bool                estimate;
        range_state_t       range_state = RANGE_INIT;
        int                 vectorno = 0;
        range_t             search_range_array[MAX_LOCAL_RANGE];

        ss_dprintf_3(("form_range_constraint\n"));
        ss_dassert(key != NULL || column_no != RS_ANO_NULL);
        ss_dassert(constraint_list);

        estimate = (key == NULL);
        
        if (estimate) {
            n_attributes_in_key = 1;
        } else {
            n_attributes_in_key = rs_key_nparts(cd, key);
        }

        search_range = alloc_search_range(cd, n_attributes_in_key, &search_range_array[0]);

        i = 0;
        finished = FALSE;
        contradictory = FALSE;
        last_match = -1;
        last_pointlike = FALSE;
        pointlike = FALSE;

        /* Loop through key parts in the key.
         */
        while (i < n_attributes_in_key && !finished) {
           if (!estimate && rs_keyp_isconstvalue(cd, key, i)) {
                /* if a key part is a constant field, we have a pointlike
                 * range
                 */
                constant_value = rs_keyp_constaval(cd, key, i);
                constant_type = rs_keyp_constatype(cd, key, i);

                p_lower = &search_range[i].rng_lower_limit;
                p_upper = &search_range[i].rng_upper_limit;

                set_limit_finite(cd, p_lower,
                                 constant_value,
                                 constant_type,
                                 TRUE);
                set_limit_finite(cd, p_upper,
                                 constant_value,
                                 constant_type,
                                 TRUE);
                SU_BFLAG_SET(p_lower->lim_flags, LIM_ISSET);
                SU_BFLAG_SET(p_upper->lim_flags, LIM_ISSET);

                last_match = i;
           } else {
                /* else we try to find constraints on the ith key part
                 */
                su_list_t* cons_list;
                if (!estimate) {
                    column_no = rs_keyp_ano(cd, key, i);
                }
                p_lower = &search_range[i].rng_lower_limit;
                p_upper = &search_range[i].rng_upper_limit;
                /* put the initial range as wide as possible
                   in the beginning */
                if (key != NULL) {
                    rs_atype_t* atype;
                    atype = rs_ttype_atype(cd, ttype, column_no);
                    set_limit_null(cd, p_lower, atype, TRUE);
                    set_limit_infinite(cd, p_upper, atype);
                } else {
                    set_limit_null_notype(cd, p_lower, TRUE);
                    set_limit_infinite_notype(cd, p_upper);
                }

                SU_BFLAG_SET(p_lower->lim_flags, LIM_ISSET);
                SU_BFLAG_SET(p_upper->lim_flags, LIM_ISSET);

                if (cons_byano != NULL) {
                    if (su_pa_indexinuse(cons_byano, column_no)) {
                        cons_list = su_pa_getdata(cons_byano, column_no);
                    } else {
                        cons_list = NULL;
                    }
                } else {
                    cons_list = constraint_list;
                }

                if (cons_list != NULL) {
                    su_list_do_get(cons_list, n, constraint) {
                        /* test if the constraint is on this attribute
                         */
                        if (rs_cons_ano(cd, constraint) == column_no) {

                            ss_dprintf_4(("form_range_constraint:i=%d, ano=%d, relop=%d\n",
                                i, column_no, rs_cons_relop(cd, constraint)));

                            /* try to form a range (= interval) */
                            is_range_constraint = form_interval(
                                                    cd,
                                                    constraint,
                                                    &lower_limit,
                                                    &upper_limit,
                                                    &constraint_solved,
                                                    &range_state,
                                                    &vectorno);

                            SU_BFLAG_SET(lower_limit.lim_flags, LIM_ISSET);
                            SU_BFLAG_SET(upper_limit.lim_flags, LIM_ISSET);

                            ss_output_4(pla_print_limit(cd, &lower_limit, "form_interval:lower_limit"));
                            ss_output_4(pla_print_limit(cd, &upper_limit, "form_interval:upper_limit"));

                            if (!is_range_constraint) {
                                /* could not form a range */
                                ss_dprintf_4(("form_range_constraint:form_interval could not form a range\n"));
                                free_limit_struct(cd, &lower_limit);
                                free_limit_struct(cd, &upper_limit);
                            } else {
                                /* could form a range */
                                isrange = TRUE;
                                last_match = i;
                                if (!estimate) {
                                    if (constraint_solved && !last_pointlike) {
                                        rs_cons_setsolved(cd, constraint, TRUE);
                                    }
                                } else if (rs_cons_relop(cd, constraint) != RS_RELOP_LIKE) {
                                    rs_cons_setestimated(cd, constraint, TRUE);
                                }
                                /* compare if the lower limit could be raised */
                                if (compare_limits(cd, LOWER, &lower_limit,
                                        &search_range[i].rng_lower_limit)
                                    > 0) {
                                    free_limit_struct(cd, 
                                        &search_range[i].rng_lower_limit);
                                    search_range[i].rng_lower_limit =
                                        lower_limit;
                                    ss_output_4(pla_print_limit(cd, &lower_limit, "set new lower_limit"));
                                } else {
                                    free_limit_struct(cd, &lower_limit);
                                }
                                /* compare if the upper limit could be lowered */
                                if (compare_limits(cd, UPPER, &upper_limit,
                                        &search_range[i].rng_upper_limit)
                                    < 0) {
                                    free_limit_struct(cd, 
                                        &search_range[i].rng_upper_limit);
                                    search_range[i].rng_upper_limit =
                                        upper_limit;
                                    ss_output_4(pla_print_limit(cd, &upper_limit, "set new upper_limit"));
                                } else {
                                    free_limit_struct(cd, &upper_limit);
                                }
                            }
                        }   
                    }
                }
            }

            if (last_match < i) {
                /* could not find any range constraint on this key part,
                   finish */
                finished = TRUE;
            } else {
                /* test if the upper limit on this key part is bigger
                   than the lower limit */
                cmp = compare_limits(cd, UPPER_LOWER,
                                 &search_range[i].rng_upper_limit,
                                 &search_range[i].rng_lower_limit);
                if (cmp > 0) {
                    /* the range was bigger than pointlike */
                    last_pointlike = TRUE;
                    /* finished = TRUE; jarmo changed to use last_pointlike */
                } else if (cmp < 0) {
                    /* the range was empty, constraints are contradictory */
                    ss_dprintf_4(("form_range_constraint:empty range\n"));
                    contradictory = TRUE;
                    finished = TRUE;
                }
                /* else the range was pointlike, continue */
                pointlike = (cmp == 0);
            }

            i++;
        } /* end of while loop through key parts */

        /* The following assertion is true if the key starts with a
            constant key part. */
        ss_assert(estimate || (int)last_match >= 0);

        if (last_match == -1) {
            /* We could not create a range. */
            ss_dprintf_4(("form_range_constraint:could not create a range, last_match == -1\n"));
            ss_dassert(estimate);
            contradictory = TRUE;
        }

        if (!contradictory) {
            if (!estimate) {
                /* If the key part is ordered in descending order
                 * we have to turn the range on that key part upside down.
                 */
                invert_descending_ranges(cd, key, search_range, last_match);
            }

            make_range_start(cd, search_range, last_match, range_start,
                         range_start_closed);
            make_range_end(cd, search_range, last_match, range_end,
                         range_end_closed, estimate);
        }

        free_search_range(cd, search_range, n_attributes_in_key);

        if (p_emptyrange != NULL) {
            *p_emptyrange = contradictory;
        }
        if (p_pointlike != NULL) {
            *p_pointlike = pointlike;
        }
        if (estimate) {
            ss_dprintf_4(("form_range_constraint:return isrange=%d\n", isrange));
            return(isrange);
        } else {
            ss_dprintf_4(("form_range_constraint:return !contradictory=%d\n", !contradictory));
            return(!contradictory);
        }
}

/*##*********************************************************************\
 * 
 *		tb_pla_form_range_constraint
 * 
 * Calculates the lower and upper limits of a search range.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	column_no - in, use
 *		handle to the key
 *
 *	constraint_list - in, use
 *		list of constraints
 *
 *	range_start - out, give
 *		start of the range, NULL if the
 *          function returns FALSE
 *
 *	range_start_closed - out, give
 *		TRUE if start is closed
 *
 *	range_end - out, give
 *		end of the range, NULL if the
 *          function returns FALSE
 *
 *	range_end_closed - out, give
 *		TRUE if end is closed
 *
 *      p_pointlike - out
 *          TRUE is range is pointlike (start and end same)
 *
 * Return value :
 *      FALSE if the range is empty (contradictory)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool tb_pla_form_range_constraint(
        rs_sysi_t*      cd,
        rs_ano_t        column_no,
        su_list_t*      constraint_list,
        su_pa_t*        cons_byano,
        dynvtpl_t*      range_start,
        bool*           range_start_closed,
        dynvtpl_t*      range_end,
        bool*           range_end_closed,
        bool*           p_pointlike,
        bool*           p_emptyrange)
{
        /* Reset to NULL to ensure memory area is not reused. */
        *range_start = NULL;
        *range_end = NULL;

        return(form_range_constraint(
                    cd,
                    NULL,
                    NULL,
                    column_no,
                    constraint_list,
                    cons_byano,
                    range_start,
                    range_start_closed,
                    range_end,
                    range_end_closed,
                    p_pointlike,
                    p_emptyrange));
}

/*#***********************************************************************\
 * 
 *		make_range_start
 * 
 * Builds a v-tuple as the lower limit of the search range
 * The constraints on the key parts are such that there is
 * a pointlike interval constraint (i.e., X in [a,a]) on every
 * key part 0, 1, ..., last_match - 1, and then any interval
 * constraint on key part last_match.
 * 
 * Parameters : 
 * 
 *	search_range - in, use
 *		an array containing the information of
 *          the range start and end
 *
 *	last_match - in, use
 *		index of the last (relevant) element in
 *          the array
 *
 *	range_start - out, give
 *		range start as a v-tuple
 *
 *	range_start_closed - out, give
 *		TRUE if the range start is closed, i.e.,
 *          contains also the start point
 *
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void make_range_start(cd, search_range, last_match, range_start,
                         range_start_closed)
        rs_sysi_t*      cd;
        range_t*        search_range;
        rs_ano_t        last_match;
        dynvtpl_t*      range_start;
        bool*           range_start_closed;
{
        int         i;
        dynvtpl_t   vtuple;
        va_t*       vattribute;
        rs_atype_t* value_type;
        rs_aval_t*  value;
        bool        use_dynvtpl = (*range_start == NULL);
#ifdef SS_UNICODE_DATA
        dynva_t     rangestart_va = NULL;
#endif /* SS_UNICODE_DATA */

        ss_assert(last_match >= 0);
        ss_dprintf_3(("make_range_start\n"));

        if (use_dynvtpl) {
            vtuple = NULL;
        } else {
            vtuple = *range_start;
        }

        if (use_dynvtpl) {
            dynvtpl_setvtpl(&vtuple, VTPL_EMPTY);
        } else {
            vtpl_setvtpl(vtuple, VTPL_EMPTY);
        }

        ss_dassert(search_range);
        ss_dassert(SU_BFLAG_TEST(search_range[last_match].rng_lower_limit.lim_flags, LIM_ISSET));

        /* Loop through the key parts with a range constraint on them. */
        for (i = 0; i <= last_match; i++) {
            if (SU_BFLAG_TEST(search_range[i].rng_lower_limit.lim_flags,
                              LIM_VALUE_NULL))
            {
                if (SU_BFLAG_TEST(search_range[i].rng_lower_limit.lim_flags, LIM_DESC)) {
                    ss_dprintf_4(("make_range_start:i=%d:invnull\n", i));
                    vattribute = rs_aval_invnull_va(cd, search_range[i].rng_lower_limit.lim_value_type);
                } else {
                    ss_dprintf_4(("make_range_start:i=%d:null\n", i));
                    vattribute = VA_NULL;
                }
            } else if (SU_BFLAG_TEST(search_range[i].rng_lower_limit.lim_flags,
                              LIM_VALUE_INFINITE))
            {
                ss_dprintf_4(("make_range_start:i=%d:infinite\n", i));
#if 0 /* Jarmo removed Nov 18, 1994 */
                /* Note that the limit cannot be infinite for i < last_match
                    because the interval on such key part is pointlike. */
                if (i < last_match) {
                    ss_dassert(
                      !SU_BFLAG_TEST(
                        search_range[i].rng_lower_limit.lim_flags,
                        LIM_VALUE_INFINITE));
                }
#endif /* 0 */
                /* Use VA_MIN instead of VA_NULL for infinite lower
                 * range to filter out NULL values.
                 * JarmoR Jul 27, 2003
                 */
                vattribute = VA_MIN;
            } else {
                ss_dprintf_4(("make_range_start:i=%d:value=%ld\n", i, (long)search_range[i].rng_lower_limit.lim_value));
                value      = search_range[i].rng_lower_limit.lim_value;
                value_type =
                        search_range[i].rng_lower_limit.lim_value_type;
                vattribute = rs_aval_va(cd, value_type, value);
#ifdef SS_UNICODE_DATA
                if (SU_BFLAG_TEST(search_range[i].rng_lower_limit.lim_flags,
                                  LIM_UNI4CHAR))
                {
                    create_va_to_range_start(&rangestart_va, vattribute);
                    vattribute = rangestart_va;
                }
#endif /* SS_UNICODE_DATA */
            }
            /* The data should now be in the v-attribute */
            if (use_dynvtpl) {
                dynvtpl_appva(&vtuple, vattribute);
            } else {
                vtpl_appva(vtuple, vattribute);
            }
            ss_output_4((vtpl_dprintvtpl(4, vtuple)));
        }
        dynva_free(&rangestart_va);
        *range_start = vtuple;
        /* Note that if for the last field lim_value_infinite was TRUE,
            lim_closed is FALSE as it should be, as the constraint is then
            field > SQL-NULL. */
        if (SU_BFLAG_TEST(search_range[last_match].rng_lower_limit.lim_flags,
                          LIM_CLOSED))
        {
            *range_start_closed = TRUE;
        } else {
            *range_start_closed = FALSE;
        }
}

/*#***********************************************************************\
 * 
 *		make_range_end
 * 
 * Builds a v-tuple as the upper limit of the search range.
 * The constraints on the key parts are such that there is
 * a pointlike interval constraint (i.e., X in [a,a]) on every
 * key part 0, 1, ..., last_match - 1, and then any interval
 * constraint on key part last_match.
 * 
 * Parameters : 
 * 
 *	search_range - in, use
 *		an array containing the information of
 *          the range end and end
 *
 *	last_match - in, use
 *		index of the last (relevant) element in
 *          the array
 *
 *	range_end - out, give
 *		range end as a v-tuple
 *
 *	range_end_closed - out, give
 *		TRUE if the range end is closed, i.e.,
 *          contains also the end point
 *
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void make_range_end(cd, search_range, last_match, range_end,
                         range_end_closed, estimate)
        rs_sysi_t*      cd;
        range_t*        search_range;
        rs_ano_t        last_match;
        dynvtpl_t*      range_end;
        bool*           range_end_closed;
        bool            estimate;
{
        int         i;
        dynvtpl_t   vtuple;
        va_t*       vattribute;
        bool        isdynva;
        rs_atype_t* value_type;    
        rs_aval_t*  value;
        bool        use_dynvtpl = (*range_end == NULL);
        int         last_finite_or_null_limit;
        bool        original_range_end_closed;
        bool        closed_already = FALSE;

        ss_dassert(search_range);
        ss_dassert(SU_BFLAG_TEST(search_range[last_match].rng_upper_limit.lim_flags, LIM_ISSET));
        ss_dprintf_3(("make_range_end\n"));

        if (use_dynvtpl) {
            vtuple = NULL;
        } else {
            vtuple = *range_end;
        }
        vattribute = NULL;
        if (use_dynvtpl) {
            dynvtpl_setvtpl(&vtuple, VTPL_EMPTY);
        } else {
            vtpl_setvtpl(vtuple, VTPL_EMPTY);
        }
        /* Note that the interval on key part last_match -1 is
           pointlike, so the upper limit cannot be then infinite. */
        for (i = 0;
             i < last_match &&
             !SU_BFLAG_TEST(search_range[i].rng_upper_limit.lim_flags, LIM_VALUE_INFINITE);
             i++)
            ;
        if (SU_BFLAG_TEST(search_range[i].rng_upper_limit.lim_flags, LIM_VALUE_INFINITE)) {
            ss_assert(i >= 0);
            if (i == 0) {
                ss_assert(estimate);
                if (use_dynvtpl) {
                    dynvtpl_free(&vtuple);
                }
                *range_end = NULL;
                *range_end_closed = FALSE;
                return;
            }
            if (SU_BFLAG_TEST(search_range[i].rng_upper_limit.lim_flags, LIM_DESC)) {
                last_finite_or_null_limit = i;
            } else {
                last_finite_or_null_limit = i-1;
            }
        } else {
            last_finite_or_null_limit = i;
        }

        original_range_end_closed =
            SU_BFLAG_TEST(search_range[last_finite_or_null_limit].rng_upper_limit.lim_flags,
                          LIM_CLOSED);

        /* Loop through the key parts with a range constraint on them. */
        for (i = 0; i <= last_finite_or_null_limit; i++) {
            isdynva = FALSE;
            if (SU_BFLAG_TEST(search_range[i].rng_upper_limit.lim_flags, LIM_VALUE_NULL)) {
                if (SU_BFLAG_TEST(search_range[i].rng_upper_limit.lim_flags, LIM_DESC)) {
                    ss_dprintf_4(("make_range_end:i=%d:invnull\n", i));
                    vattribute = rs_aval_invnull_va(cd, search_range[i].rng_upper_limit.lim_value_type);
                } else {
                    ss_dprintf_4(("make_range_end:i=%d:null\n", i));
                    vattribute = VA_NULL;
                }

            } else if (SU_BFLAG_TEST(search_range[i].rng_upper_limit.lim_flags, 
                                     LIM_VALUE_INFINITE)
                    && SU_BFLAG_TEST(search_range[i].rng_upper_limit.lim_flags, 
                                     LIM_DESC))
            {
                ss_dprintf_4(("make_range_end:i=%d:infinite desc\n", i));
                vattribute = rs_aval_invnull_va(cd, search_range[i].rng_upper_limit.lim_value_type);

            } else {
                va_t*       tmp_va;

                ss_dprintf_4(("make_range_end:i=%d:value=%ld\n", i, (long)search_range[i].rng_upper_limit.lim_value));
                ss_dassert(search_range[i].rng_upper_limit.lim_value != NULL);
                value      = search_range[i].rng_upper_limit.lim_value;
                value_type =
                    search_range[i].rng_upper_limit.lim_value_type;

                tmp_va = rs_aval_va(cd, value_type, value);
                if (SU_BFLAG_TEST(
                        search_range[i].rng_upper_limit.lim_flags,
                        LIM_UNI4CHAR))
                {
                    isdynva = TRUE;
                    vattribute = NULL;
                    create_va_to_range_end((dynva_t*)&vattribute, tmp_va);
                    closed_already = TRUE;
                } else {
                    vattribute = tmp_va;
                }
            }
            /* The data should now be in the v-attribute */
            /* If the upper limit is closed, we have to find
                the immediate
                successor of the last attribute to include in the
                range all values of subsequent attributes.
                If the next limit value is NULL, then this is the last
                limit value that can be used. */
            if (i == last_finite_or_null_limit
            &&  original_range_end_closed
            &&  !closed_already)
            {
                if (use_dynvtpl) {
                    dynvtpl_appvawithincrement(&vtuple, vattribute);
                } else {
                    vtpl_appvawithincrement(vtuple, vattribute);
                }
            } else {
                if (use_dynvtpl) {
                    dynvtpl_appva(&vtuple, vattribute);
                } else {
                    vtpl_appva(vtuple, vattribute);
                }
            }
            if (isdynva) {
                dynva_free((dynva_t*)&vattribute);
            }
            ss_output_4((vtpl_dprintvtpl(4, vtuple)));
        }
        /* Currently, the range end is always open. */
        *range_end_closed = FALSE;
        *range_end = vtuple;
}

/*#**********************************************************************\
 * 
 *		form_key_constraints
 * 
 * Forms the list of constraints for the engine
 * which can be tested against the
 * given key.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	key - in, use
 *		given key
 *
 *	key_constraint_list - in out, use
 *		pre-allocated empty list where constraints are stored
 *
 *	constraint_list - in, use
 *		list of constraints
 *
 *      isclust - in
 *          TRUE means consttrains is for data columns, not for a columns
 *          in secondary index.
 * 
 * Output params: 
 * 
 * Return value : out, give: list of objects of type rs_pla_cons_t which
 *                contains the constraints in the form for
 *                the database engine, i.e., attribute index is physical
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static su_list_t* form_key_constraints(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        rs_key_t*   key,
        su_list_t*  key_constraint_list,
        su_list_t*  constraint_list,
        bool        isclust)
{
        su_list_node_t* n;
        rs_cons_t*      cons;
        rs_pla_cons_t*  pla_cons;
        rs_ano_t        col_no;
        rs_ano_t        j;
        bool            contains;
        su_list_t*      ret;
        su_list_node_t* ret_n;
        uint            relop;
        ss_debug(int    key_cons_listlen = -1;)

        ss_dassert(key);
        ss_dassert(constraint_list);
        ss_dassert(key_constraint_list);

        ret = key_constraint_list;
        if (su_list_length(ret) == 0) {
            ret_n = NULL;
        } else {
            ret_n = su_list_first(ret);
            ss_debug(key_cons_listlen = su_list_length(ret));
        }

        /* Loop through constraints in the list.
         */
        su_list_do_get(constraint_list, n, cons) {
            su_bflag_t pla_cons_flags = 0;

            if (rs_cons_aval(cd, cons) == NULL) {
                ss_dassert(rs_cons_isalwaysfalse_once(cd, cons));
                rs_cons_setsolved(cd, cons, TRUE);
                continue;
            }

            if (!rs_cons_issolved(cd, cons)) {
                /* If not already solved, check if the attribute
                 * is contained in the key.
                 */
                col_no = rs_cons_ano(cd, cons);
#ifdef SS_COLLATION
                j = rs_key_searchkpno_anytype(cd, key, col_no);
                if (j != RS_ANO_NULL) {
                    if (rs_keyp_collation(cd, key, j) != NULL) {
                        j = RS_ANO_NULL;
                        if (isclust) {
                            rs_ano_t k;
                            k = rs_key_searchkpno_data(cd, key, col_no);
                            if (k != RS_ANO_NULL) {
                                ss_dassert(rs_keyp_collation(cd, key, k)
                                           == NULL);
                                j = k;
                                pla_cons_flags |= RS_PLA_CONS_FLAG_COLLATED;
                            } else {
                                ss_derror;
                            }
                        }
                    } else if (isclust) {
                        rs_atype_t* atype;
                        atype = rs_cons_atype(cd, cons);
                        if (rs_atype_collation(cd, atype) != NULL) {
                            pla_cons_flags |= RS_PLA_CONS_FLAG_COLLATED;
                        }
                    }
                }
#else /* SS_COLLATION */
                j = rs_key_searchkpno_data(cd, key, col_no);
#endif /* SS_COLLATION */
                contains = (j != RS_ANO_NULL) &&
                           !rs_keyp_isconstvalue(cd, key, j);

                if (contains) {
                    rs_atype_t* atype;
                    rs_aval_t* aval;
                    rs_aval_t* desc_aval;
                    bool include_nulls = FALSE;

                    ss_dassert(rs_keyp_ano(cd, key, j) == col_no);
                    atype = rs_cons_atype(cd, cons);
                    aval = rs_cons_aval(cd, cons);
                    relop = rs_cons_relop(cd, cons);
                    rs_cons_setsolved(cd, cons, TRUE);
                    if (rs_keyp_isascending(cd, key, j)) {
                        desc_aval = NULL;
                        switch (relop) {
                            case RS_RELOP_LT_VECTOR:
                                include_nulls = TRUE;
                                break;
                            case RS_RELOP_GT_VECTOR:
                                include_nulls = TRUE;
                                break;
                            case RS_RELOP_LE_VECTOR:
                                include_nulls = TRUE;
                                break;
                            case RS_RELOP_GE_VECTOR:
                                include_nulls = TRUE;
                                break;
                            default:
                                break;
                        }
                    } else {
                        /* Convert aval to descending format. */
                        desc_aval = rs_aval_copy(cd, atype, aval);
                        if (relop == RS_RELOP_LIKE) {
                            int old_esc;
                            old_esc = rs_cons_escchar(cd, cons);
                            if (old_esc == SU_SLIKE_NOESCCHAR) {
                                rs_cons_setescchar(cd, cons, '\\');
                            }
                            rs_aval_likepatasctodesc(
                                cd,
                                atype,
                                desc_aval,
                                old_esc,
                                rs_cons_escchar(cd, cons));
                        } else {
                            rs_aval_asctodesc(cd, atype, desc_aval);
                        }
                        aval = desc_aval;
                        /* Invert relop. */
                        switch (relop) {
                            case RS_RELOP_LT:
                                relop = RS_RELOP_GT;
                                break;
                            case RS_RELOP_GT:
                                relop = RS_RELOP_LT;
                                break;
                            case RS_RELOP_LE:
                                relop = RS_RELOP_GE;
                                break;
                            case RS_RELOP_GE:
                                relop = RS_RELOP_LE;
                                break;
                            case RS_RELOP_LT_VECTOR:
                                relop = RS_RELOP_GT_VECTOR;
                                include_nulls = TRUE;
                                break;
                            case RS_RELOP_GT_VECTOR:
                                relop = RS_RELOP_LT_VECTOR;
                                include_nulls = TRUE;
                                break;
                            case RS_RELOP_LE_VECTOR:
                                relop = RS_RELOP_GE_VECTOR;
                                include_nulls = TRUE;
                                break;
                            case RS_RELOP_GE_VECTOR:
                                relop = RS_RELOP_LE_VECTOR;
                                include_nulls = TRUE;
                                break;
                            default:
                                break;
                        }
                    }
                    if (rs_cons_isuniforchar(cd, cons)) {
                        pla_cons_flags |= RS_PLA_CONS_FLAG_UNI4CHAR;
                    } else if (rs_atype_datatype(cd, atype) == RSDT_UNICODE) {
                        pla_cons_flags |= RS_PLA_CONS_FLAG_UNICODE;
                    }

                    if (include_nulls) {
                        pla_cons_flags |= RS_PLA_CONS_FLAG_INCLUDENULLS;
                    } else {
                        if (rs_atype_issync(cd, atype)) {
                            if (relop == RS_RELOP_LT ||
                                relop == RS_RELOP_LE ||
                                relop == RS_RELOP_EQUAL) {
                                    pla_cons_flags |= RS_PLA_CONS_FLAG_INCLUDENULLS;
                                }
                        }
                    }
                    {
                        rs_ttype_t*     ttype;
                        rs_atype_t*     defatype;
                        rs_aval_t*      defaval;
                        va_t*           defva;

                        ttype = rs_relh_ttype(cd, relh);
                        defatype = rs_ttype_atype(cd, ttype, col_no);
                        defaval = rs_atype_getoriginaldefault(cd, defatype);
                        if (defaval != NULL) {
                            defva = rs_aval_va(cd, defatype, defaval);
                        } else {
                            defva = NULL;
                        }
                        if (ret_n == NULL) {
                            pla_cons = rs_pla_cons_init(
                                    cd,
                                    j,
                                    relop,
                                    rs_aval_va(cd, atype, aval),
                                    rs_cons_escchar(cd, cons),
                                    pla_cons_flags,
                                    defva
#ifdef SS_COLLATION
                                    , cons
#endif /* SS_COLLATION */
                                );
                        } else {
                            pla_cons = su_listnode_getdata(ret_n);
                            ss_dassert((rs_ano_t)rs_pla_cons_kpindex(cd, pla_cons) == j);
                            ss_dassert(rs_pla_cons_relop(cd, pla_cons) == relop);
                            ss_dassert(rs_pla_cons_escchar(cd, pla_cons) == rs_cons_escchar(cd, cons));
                            ss_dassert(rs_pla_cons_listnode(cd, pla_cons) == ret_n);
                            rs_pla_cons_reset(
                                cd,
                                pla_cons,
                                rs_aval_va(cd, atype, aval)
#ifdef SS_COLLATION
                                , cons
#endif /* SS_COLLATION */
                                );
                        }
                    }
                    
                    if (ret_n == NULL) {
                        su_list_insertlast_nodebuf(
                            ret,
                            rs_pla_cons_listnode(cd, pla_cons),
                            pla_cons);
                    } else {
                        ret_n = su_list_next(ret, ret_n);
                    }
                    if (desc_aval != NULL) {
                        rs_aval_free(cd, atype, desc_aval);
                    }
                }
            }
        }
        ss_dassert(key_cons_listlen == -1 || key_cons_listlen == (int)su_list_length(ret));
        ss_dassert(ret_n == NULL || su_list_next(ret, ret_n) == NULL);;
        return(ret);
}

/*##*********************************************************************\
 * 
 *		tb_pla_form_select_list_buf
 * 
 * Form the select list in the form where the column numbers
 * are their physical index in the key. Uses a pre-allocated buffer.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	clkey - in, use
 *		    the clustering key of the relation
 *
 *	key - in, use
 *		  the key where the search is done
 *
 *	select_list - in, use
 *		          the table level select list
 *
 *	must_dereference1 - in, use
 *                          TRUE if some of the constraints must be tested
 *                          on the data tuple		
 *
 *	p_must_dereference2 - out, give
 *		                  TRUE if either some of the constraints
 *                            must be tested against the data tuple
 *                            or if some columns of the select list
 *                            must be retrieved from the data tuple.
 *                            If this is TRUE then the select list
 *                            is taken from the data tuple, i.e., clkey,
 *                            if this is FALSE, then the select list is
 *                            taken from key.
 *
 * 
 * Output params: 
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void tb_pla_form_select_list_buf(
        rs_sysi_t* cd,
        rs_key_t* clkey,
        rs_key_t* key,
        rs_ano_t* select_list,
        rs_ano_t* key_select_list,
        bool must_dereference1,
        bool* p_must_dereference2)
{
        uint i;
        rs_key_t* select_key;
        rs_ano_t kpno;  /* key part number */

        ss_dassert(clkey != NULL);
        ss_dassert(key != NULL);
        ss_dassert(key_select_list != NULL);
        ss_dassert(select_list != NULL);
        ss_dassert(p_must_dereference2 != NULL);

        if (must_dereference1) {
            select_key = clkey;
            *p_must_dereference2 = TRUE;
        } else {
            /* Check if we can find all attributes from key. */
            select_key = key;
            *p_must_dereference2 = FALSE;
            for (i = 0; select_list[i] != RS_ANO_NULL; i++) {
                if (select_list[i] != RS_ANO_PSEUDO) {
                    kpno = rs_key_searchkpno_data(cd, key, select_list[i]);
                    if (kpno == RS_ANO_NULL) {
                        /* Attribute not found from the key */
                        select_key = clkey;
                        *p_must_dereference2 = TRUE;
                        break;
                    }
                }
            }
        }

        /* Find the attribute numbers specified in select_list from
           select_key.
        */
        for (i = 0; select_list[i] != RS_ANO_NULL; i++) {
            /* Find attribute number select_list[i] from select_key. */
            if (select_list[i] == RS_ANO_PSEUDO) {
                key_select_list[i] = RS_ANO_PSEUDO;
            } else {
                kpno = rs_key_searchkpno_data(cd, select_key, select_list[i]);
                ss_dassert(kpno != RS_ANO_NULL); /* Ensure that the attribute is found. */
                key_select_list[i] = kpno;
            }
        }
        key_select_list[i] = RS_ANO_NULL;
}

/*##*********************************************************************\
 * 
 *		tb_pla_form_select_list
 * 
 * Form the select list in the form where the column numbers
 * are their physical index in the key.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	clkey - in, use
 *		    the clustering key of the relation
 *
 *	key - in, use
 *		  the key where the search is done
 *
 *	select_list - in, use
 *		          the table level select list
 *
 *	must_dereference1 - in, use
 *                          TRUE if some of the constraints must be tested
 *                          on the data tuple		
 *
 *	p_must_dereference2 - out, give
 *		                  TRUE if either some of the constraints
 *                            must be tested against the data tuple
 *                            or if some columns of the select list
 *                            must be retrieved from the data tuple.
 *                            If this is TRUE then the select list
 *                            is taken from the data tuple, i.e., clkey,
 *                            if this is FALSE, then the select list is
 *                            taken from key.
 *
 * 
 * Output params: 
 * 
 * Return value : out, give: The physical select list as an array.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ano_t* tb_pla_form_select_list(
        rs_sysi_t* cd,
        rs_key_t* clkey,
        rs_key_t* key,
        rs_ano_t* select_list,
        bool must_dereference1,
        bool* p_must_dereference2)
{
        uint i;
        uint nselect;
        rs_ano_t* key_select_list;

        ss_dassert(clkey != NULL);
        ss_dassert(key != NULL);
        ss_dassert(select_list != NULL);
        ss_dassert(p_must_dereference2 != NULL);

        /* Count the size of key_select_list and allocate array
           for it.
        */
        for (i = 0, nselect = 0; select_list[i] != RS_ANO_NULL; i++) {
            nselect++;
        }
        key_select_list = SsMemAlloc((nselect + 1) * sizeof(key_select_list[0]));

        tb_pla_form_select_list_buf(
            cd,
            clkey,
            key,
            select_list,
            key_select_list,
            must_dereference1,
            p_must_dereference2);

        return(key_select_list);
}

/*#***********************************************************************\
 * 
 *		tb_pla_initrsplan
 * 
 * Initializes a rs_pla_t object from the local tb_pla_t object.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *		
 *	rs_plan - in out, use
 *		Local plan object.
 *		
 *	table - in, use
 *		table handle
 *
 *	key - in, use
 *		key handle
 *
 *	tb_plan - in, take
 *		Local plan object.
 *		
 * Return value - give : 
 * 
 *      rs-plan object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void tb_pla_initrsplan(
        rs_sysi_t* cd,
        rs_pla_t* rs_plan,
        rs_relh_t* table,
        rs_key_t* key,
        tb_pla_t* tb_plan,
        bool addlinks)
{
        rs_pla_initbuf(
            cd,
            rs_plan,
            table,
            key,
            tb_plan->pla_isconsistent,
            tb_plan->pla_range_start,
            tb_plan->pla_start_closed,
            tb_plan->pla_range_end,
            tb_plan->pla_end_closed,
            tb_plan->pla_key_constraints,
            tb_plan->pla_data_constraints,
            tb_plan->pla_constraints,
            tb_plan->pla_tuple_reference,
            tb_plan->pla_select_list,
            tb_plan->pla_dereference,
            tb_plan->pla_nsolved_range_cons,
            tb_plan->pla_nsolved_key_cons,
            tb_plan->pla_nsolved_data_cons,
            addlinks);

        if (pla_test_version_on) {
            /* If the test version flag is on, we keep the plan for
               inspection and it is left as garbage. */
            rs_pla_link(cd, rs_plan);
            pla_test_plan = rs_plan;
        }
}

/*#***********************************************************************\
 * 
 *		tb_pla_resetrsplan
 * 
 * Reset a rs_pla_t object from the local tb_pla_t object.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *		
 *	rs_plan - in out, use
 *		Local plan object.
 *		
 *	table - in, use
 *		table handle
 *
 *	key - in, use
 *		key handle
 *
 *	tb_plan - in, take
 *		Local plan object.
 *		
 * Return value - give : 
 * 
 *      rs-plan object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void tb_pla_resetrsplan(
        rs_sysi_t* cd,
        rs_pla_t* rs_plan,
        rs_relh_t* table,
        rs_key_t* key,
        tb_pla_t* tb_plan,
        bool addlinks __attribute__ ((unused)))
{
        ss_debug(rs_pla_check_reset(
            cd,
            rs_plan,
            table,
            key,
            tb_plan->pla_isconsistent,
            tb_plan->pla_range_start,
            tb_plan->pla_start_closed,
            tb_plan->pla_range_end,
            tb_plan->pla_end_closed,
            tb_plan->pla_key_constraints,
            tb_plan->pla_data_constraints,
            tb_plan->pla_constraints,
            tb_plan->pla_tuple_reference,
            tb_plan->pla_select_list,
            tb_plan->pla_dereference,
            tb_plan->pla_nsolved_range_cons,
            tb_plan->pla_nsolved_key_cons,
            tb_plan->pla_nsolved_data_cons));

        rs_pla_reset(
            cd,
            rs_plan,
            tb_plan->pla_isconsistent,
            tb_plan->pla_range_start,
            tb_plan->pla_range_end);

        if (pla_test_version_on) {
            /* If the test version flag is on, we keep the plan for
               inspection and it is left as garbage. */
            rs_pla_link(cd, rs_plan);
            pla_test_plan = rs_plan;
        }
}

static rs_pla_t* pla_forcenewrsplan(
        rs_sysi_t*  cd,
        rs_pla_t*   rspla)
{
        if (rspla != NULL) {
            rs_pla_done(cd, rspla);
        }
        rspla = rs_pla_alloc(cd);
        return(rspla);
}

/*##**********************************************************************\
 * 
 *		tb_pla_create_search_plan
 * 
 * The following function creates a search plan structure and
 * generates the search plan. The table level has to call this
 * first and then the functions which extract information from
 * the returned object.
 * 
 * The returned plan object is rs_pla_t*.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		
 *
 *	table - in, use
 *		table handle
 *
 *	key - in , use
 *		key handle
 *
 *	constraint_list - in, hold
 *		constraint list
 *
 *	select_list - in, use
 *		list of selected columns
 *
 * 
 * Output params: 
 * 
 * Return value : out, give: search plan object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_pla_t* tb_pla_create_search_plan(
        rs_sysi_t*  cd,
        rs_pla_t*   rspla,
        rs_relh_t*  table,
        rs_key_t*   key,
        su_list_t*  constraint_list,
        su_pa_t*    cons_byano,
        int*        select_list,
        bool        addlinks)
{
        su_list_node_t* n;
        rs_cons_t*      cons;
        vtpl_t*         range_start;
        bool            range_start_closed;
        vtpl_t*         range_end;
        bool            range_end_closed;
        vtpl_t*         range_start_buf = NULL;
        vtpl_t*         range_end_buf = NULL;
        bool            nonempty;
        su_list_t*      data_constraints;
        tb_pla_t        plan;
        rs_key_t*       clkey;
        bool            must_dereference1;
        bool            must_dereference2;
        bool            reset_plan = (rspla != NULL);
        int             conslist_maxstoragelength = 0;
        bool            ismainmem;
        bool            isallocated = FALSE;
        su_profile_timer;

        ss_dassert(table);
        ss_dassert(key);
        ss_dassert(constraint_list);
        ss_dassert(select_list);
        
        su_profile_start;

        if (rspla == NULL) {
            rspla = pla_forcenewrsplan(cd, NULL);
        }

        memset(&plan, '\0', sizeof(tb_pla_t));

        if (!reset_plan) {
            rs_ano_t    i;
            rs_ano_t    n_attributes_in_key;
            rs_atype_t* atype;
            int         len;

            n_attributes_in_key = rs_key_nparts(cd, key);

            for (i = 0; i < n_attributes_in_key; i++) {
                if (rs_keyp_isconstvalue(cd, key, i)) {
                    atype = rs_keyp_constatype(cd, key, i);
                    len = rs_atype_maxstoragelength(cd, atype);
                    if (len > PLA_MAX_FIXED_STORAGELEN) {
                        conslist_maxstoragelength = -1;
                        break;
                    } else {
                        conslist_maxstoragelength = conslist_maxstoragelength + len;
                        if (conslist_maxstoragelength > PLA_MAX_FIXED_STORAGELEN) {
                            conslist_maxstoragelength = -1;
                            break;
                        }
                    }
                } else {
                    break;
                }
            }
        }

        /* Test first if there are always false constraints.
         * Also calculate constraint max storage length.
         */
        su_list_do_get(constraint_list, n, cons) {
            ss_dassert(!rs_cons_issolved(cd, cons));
            if (rs_cons_isalwaysfalse(cd, cons)) {
                plan.pla_isconsistent = FALSE;
                rspla = pla_forcenewrsplan(cd, rspla);
                tb_pla_initrsplan(cd, rspla, table, key, &plan, addlinks);
                su_profile_stop("tb_pla_create_search_plan");
                return(rspla);
            }
            if (!reset_plan && conslist_maxstoragelength != -1) {
                rs_atype_t* atype;
                int len;

                atype = rs_cons_atype(cd, cons);
                ss_dassert(atype != NULL);
                len = rs_atype_maxstoragelength(cd, atype);
                if (len > PLA_MAX_FIXED_STORAGELEN) {
                    conslist_maxstoragelength = -1;
                } else {
                    conslist_maxstoragelength = conslist_maxstoragelength + len;
                    if (conslist_maxstoragelength > PLA_MAX_FIXED_STORAGELEN) {
                        conslist_maxstoragelength = -1;
                    }
                }
            }
        }

        plan.pla_constraints = constraint_list;

#ifdef SS_MME

        ismainmem = rs_relh_reltype(cd, table) == RS_RELTYPE_MAINMEMORY;

        if (ismainmem) {

            plan.pla_isconsistent = TRUE;

        } else
#endif /* SS_MME */
        {

        if (reset_plan) {
            /* Use previously calculated value. */
            ss_dassert(conslist_maxstoragelength == 0);
            conslist_maxstoragelength = rs_pla_get_conslist_maxstoragelength(cd, rspla);
        } else {
            ss_dassert(conslist_maxstoragelength != 0);
        }
        if (conslist_maxstoragelength == -1) {
            /* Length unknown or too long. */
            range_start = NULL;
            range_end = NULL;
        } else if (reset_plan) {
            /* Using old plan, get data area pointers. */
            rs_pla_get_range_buffers(
                cd,
                rspla,
                &range_start,
                &range_end);
        } else {
            /* Allocate memory. */
            isallocated = TRUE;
            range_start_buf = SsMemAlloc(conslist_maxstoragelength);
            range_end_buf = SsMemAlloc(conslist_maxstoragelength + 5);
            range_start = range_start_buf;
            range_end = range_end_buf;
        }

        /* Form the search range.
         */
        nonempty = form_range_constraint(cd, key, rs_relh_ttype(cd, table), RS_ANO_NULL, 
                            constraint_list, cons_byano,
                            &range_start, &range_start_closed,
                            &range_end, &range_end_closed,
                            NULL, NULL);

        /* If the range is empty, the search constraints are inconsistent.
         */
        plan.pla_isconsistent = nonempty;
        if (!nonempty) {
            if (isallocated) {
                SsMemFree(range_start_buf);
                SsMemFree(range_end_buf);
            }
            rspla = pla_forcenewrsplan(cd, rspla);
            tb_pla_initrsplan(cd, rspla, table, key, &plan, addlinks);
            su_profile_stop("tb_pla_create_search_plan");
            return(rspla);
        }

        plan.pla_nsolved_range_cons = 0;

        su_list_do_get(constraint_list, n, cons) {
            if (rs_cons_issolved(cd, cons)) {
                (plan.pla_nsolved_range_cons)++;
            }
        }
          
        plan.pla_range_start = range_start;
        plan.pla_start_closed = range_start_closed;
        plan.pla_range_end = range_end;
        plan.pla_end_closed = range_end_closed;

        /* find the clustering key */
        clkey = rs_relh_clusterkey(cd, table);
        ss_dassert(clkey != NULL);

        /* Calculate the lists of constraints which must
         * be tested on the chosen key and data tuple.
         */
        plan.pla_key_constraints =
            form_key_constraints(
                    cd,
                    table,
                    key,
                    rs_pla_get_key_constraints_buf(cd, rspla),
                    constraint_list,
                    key == clkey);
        data_constraints =
            form_key_constraints(
                    cd,
                    table,
                    clkey,
                    rs_pla_get_data_constraints_buf(cd, rspla),
                    constraint_list,
                    TRUE);
        
        /* If there are constraints which must be tested on data tuple
         * we know that we must dereference.
         */
        if (su_list_first(data_constraints) == NULL) {
            must_dereference1 = FALSE;
        } else {
            must_dereference1 = TRUE;
        }
        plan.pla_data_constraints = data_constraints;

        if (reset_plan) {
            rs_pla_get_select_list(
                cd,
                rspla,
                &plan.pla_select_list,
                &plan.pla_dereference);
            plan.pla_tuple_reference = rs_pla_get_tuple_reference(cd, rspla);
        } else {
            /* Form the select list and check if we must dereference for that.
             */
            plan.pla_select_list =
                    tb_pla_form_select_list(cd, clkey, key, select_list,
                                    must_dereference1, &must_dereference2);
            plan.pla_dereference = must_dereference1 || must_dereference2;
            plan.pla_tuple_reference = rs_pla_form_tuple_reference(
                                        cd,
                                        clkey,
                                        rs_pla_get_tuple_reference_buf(cd, rspla),
                                        key);
        }

        plan.pla_nsolved_key_cons =
            su_list_length(plan.pla_key_constraints);

        plan.pla_nsolved_data_cons =
            su_list_length(plan.pla_data_constraints);

        if (!(plan.pla_nsolved_range_cons
                  + plan.pla_nsolved_key_cons
                  + plan.pla_nsolved_data_cons ==
                  (int)su_list_length(constraint_list)))
        {
            plan.pla_isconsistent = FALSE;
        }

        }

        if (reset_plan) {
            tb_pla_resetrsplan(cd, rspla, table, key, &plan, addlinks);
        } else {
            tb_pla_initrsplan(cd, rspla, table, key, &plan, addlinks);
            if (!ismainmem && conslist_maxstoragelength > 0) {
                rs_pla_set_range_buffers(
                    cd,
                    rspla,
                    conslist_maxstoragelength,
                    range_start_buf,
                    range_end_buf);
            }
        }
        su_profile_stop("tb_pla_create_search_plan");
        return(rspla);
}
