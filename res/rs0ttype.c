/*************************************************************************\
**  source       * rs0ttype.c
**  directory    * res
**  description  * Tuple type functions
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
This module implements a tuple type object. A tuple type consists of an
array of attributes, each containing a name and id.
A physical tuple type contains both system and user defined attributes.

Limitations:
-----------


Error handling:
--------------
None. Dasserts

Objects used:
------------
Attribute type  <rs0atype.h>

Preconditions:
-------------
None

Multithread considerations:
--------------------------
Code is fully re-entrant.
The same ttype object can not be used simultaneously from many threads.


Example:
-------
tttype.c

**************************************************************************
#endif /* DOCUMENTATION */

#define RS_INTERNAL
#define RS0TTYPE_C

#include <ssc.h>
#include <ssstring.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <uti0va.h>
#include <uti0vtpl.h>
#include <su0parr.h>
#include <su0rbtr.h>

#include "rs0ttype.h"
#include "rs0types.h"
#include "rs0atype.h"
#include "rs0sdefs.h"
#include "rs0sysi.h"

#define SHTTYPE_SIZE(nattrs) \
    SS_SIZEOF_VARLEN_STRUCT(rs_shttype_t, stt_attr_arr, nattrs)

#define SHTTYPE_LINK_INIT(sshttype) \
        ((sshttype)->stt_nlinks = 1)

#define SHTTYPE_LINK_GET(sshttype) \
        ((sshttype)->stt_nlinks)

#define SHTTYPE_LINK_INC(sshttype) \
        (++((sshttype)->stt_nlinks))

#define SHTTYPE_LINK_DEC(sshttype) \
        (--((sshttype)->stt_nlinks))

typedef struct {
        char* an_name;
        rs_ano_t an_no;
} rs_aname_t;


static int an_insertcmp(
            void* an1,
            void* an2
);

static int an_searchcmp(
            void* name,
            void* an
);


/*#***********************************************************************\
 *
 *              an_insertcmp
 *
 * Function for comparing two an structures by an_name.
 * Used when inserting to su_rbtree.
 *
 * Parameters :
 *
 *      an1 -
 *
 *
 *      an2 -
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
static int an_insertcmp(
            void* an1,
            void* an2
) {
        rs_aname_t* a1 = an1;
        rs_aname_t* a2 = an2;

        ss_dassert(a1->an_name != NULL);
        ss_dassert(a2->an_name != NULL);
        return(strcmp(a1->an_name, a2->an_name));
}

/*#***********************************************************************\
 *
 *              an_searchcmp
 *
 * Used when searching from su_rbtree.
 *
 * Parameters :
 *
 *      name - attribute name
 *
 *
 *      ai -
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
static int an_searchcmp(
            void* name,
            void* ai
) {
        rs_aname_t* a = ai;

        ss_dassert(name != NULL);
        ss_dassert(a->an_name != NULL);
        return(strcmp(name, a->an_name));
}


static rs_aname_t* an_init(char* aname, rs_ano_t ano)
{
        rs_aname_t* an;

        ss_dassert(aname != NULL);
        an = SSMEM_NEW(rs_aname_t);
        an->an_name = aname;
        an->an_no = ano;
        return (an);
}

static void an_done(void* /* rs_aname_t* */ an)
{
        ss_dassert(an != NULL);
        SsMemFree(an);
}

/*#***********************************************************************\
 *
 *              shttype_create
 *
 * Creates a shared ttype object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 * Return value - give :
 *      shared ttype object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rs_shttype_t* shttype_create(void* cd)
{
        rs_shttype_t* shttype;

        ss_dprintf_3(("%s: rs_ttype_create\n", __FILE__));
        SS_NOTUSED(cd);
        SS_MEMOBJ_INC(SS_MEMOBJ_TTYPE, rs_shttype_t);

        shttype = SsMemAlloc(SHTTYPE_SIZE(0));
        ss_debug(shttype->stt_check = RSCHK_TUPLETYPE);
        SHTTYPE_LINK_INIT(shttype);
#ifndef RS_SQLANO_EQ_PHYS
        shttype->stt_sqltophys = NULL;
        shttype->stt_nsqlattrs = 0;
#endif
        shttype->stt_nattrs = 0;
        shttype->stt_aname_rbt = su_rbt_inittwocmp(
                                    an_insertcmp,
                                    an_searchcmp,
                                    an_done);
        shttype->stt_sem = rs_sysi_getrslinksem(cd);
        shttype->stt_readonly = FALSE;
        ss_debug(shttype->stt_name = NULL);
        return (shttype);
}

/*#***********************************************************************\
 *
 *              shttype_incsize
 *
 * Increments shared ttype size with one attribute
 *
 * Parameters :
 *
 *      shttype - in, take
 *              shared ttype object
 *
 *      sqlattr - in
 *          TRUE if attr is visible to SQL
 *          FALSE otherwise
 *
 * Return value - give :
 *      possibly reallocated shared ttype
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rs_shttype_t* shttype_incsize(
            rs_shttype_t* shttype,
            bool sqlattr)
{
        rs_attr_t* a;
        uint nlink;

        ss_dassert(!shttype->stt_readonly);
        nlink = SHTTYPE_LINK_DEC(shttype);
        ss_dassert(nlink == 0);
#ifndef RS_SQLANO_EQ_PHYS
        if (sqlattr) {
            shttype->stt_nsqlattrs++;
            if (shttype->stt_sqltophys == NULL) {
                ss_dassert(shttype->stt_nsqlattrs == 1);
                shttype->stt_sqltophys =
                    SsMemAlloc(shttype->stt_nsqlattrs * sizeof(rs_ano_t));
            } else {
                shttype->stt_sqltophys =
                    SsMemRealloc(shttype->stt_sqltophys,
                        shttype->stt_nsqlattrs * sizeof(rs_ano_t));
            }
            shttype->stt_sqltophys[shttype->stt_nsqlattrs - 1] =
                shttype->stt_nattrs;
        }
#else
        SS_NOTUSED(sqlattr);
#endif
        shttype->stt_nattrs++;
        shttype = SsMemRealloc(shttype, SHTTYPE_SIZE(shttype->stt_nattrs));
        SHTTYPE_LINK_INIT(shttype);
        a = &shttype->stt_attr_arr[shttype->stt_nattrs - 1];
        a->ai_name = NULL;
        a->ai_attrid = (ulong)-1;
        a->ai_defaultvalue = NULL;
        a->ai_defaultvalueisnull = FALSE;
        a->ai_defaultvalueisset = FALSE;
        a->ai_aval = NULL;
        RS_ATYPE_INITBUFTOUNDEF(cd, &a->ai_atype);
#ifndef RS_SQLANO_EQ_PHYS
        if (sqlattr) {
            a->ai_sqlano = shttype->stt_nsqlattrs - 1;
        } else {
            a->ai_sqlano = RS_ANO_NULL;
        }
#endif
        return (shttype);
}

/*#***********************************************************************\
 *
 *              shttype_setatype
 *
 * Set atype to a shared ttype
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      shttype - in, take
 *              shared ttype object
 *
 *      attr_n - in
 *              attribute # to set
 *
 *      atype - in, use
 *              attribute type object
 *
 * Return value - give :
 *      possibly reallocated shared ttype
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rs_shttype_t* shttype_setatype(
    void*       cd,
    rs_shttype_t* shttype,
    uint        attr_n,
    rs_atype_t* atype)
{
        rs_attr_t* a;

        ss_dprintf_3(("%s: shttype_setatype\n", __FILE__));

        if (attr_n >= shttype->stt_nattrs) {
            bool sqlattr;

            ss_dassert(attr_n == shttype->stt_nattrs);

            sqlattr = RS_ATYPE_ISUSERDEFINED(cd, atype);
            shttype = shttype_incsize(shttype, sqlattr);
        }
        a = &shttype->stt_attr_arr[attr_n];
        rs_atype_copybuf(cd, &a->ai_atype, atype);
        return (shttype);
}


/*##**********************************************************************\
 *
 *              shttype_setaname_caller
 *
 * Sets attribute name to a shared ttype
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      shttype - in, take
 *              shared ttype object
 *
 *      attr_n - in
 *              attribute # to be named
 *
 *      name - in, use
 *              attribute name
 *
 *      from_sql - in
 *          tells whether caller is SQL
 *
 * Return value - give :
 *      possibly reallocated shared ttype
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_shttype_t* shttype_setaname_caller(
    void*       cd,
    rs_shttype_t* shttype,
    uint        attr_n,
    char*       name,
        bool        from_sql)
{
        rs_attr_t* a;
        su_rbt_node_t* rbtnode;

        ss_dprintf_3(("%s: shttype_setaname_caller\n", __FILE__));
        SS_NOTUSED(cd);

        ss_dassert(name != NULL);
        if (attr_n >= shttype->stt_nattrs) {

            /* The attribute did not exist */
            ss_dassert(from_sql);   /* allowed from sql only */
            ss_dassert(attr_n == shttype->stt_nattrs);
            shttype = shttype_incsize(shttype, from_sql);
            a = &shttype->stt_attr_arr[attr_n];
        } else {
            a = &shttype->stt_attr_arr[attr_n];
            if (a->ai_name != NULL) {
                /* The attribute exists and already contains a name */
                rbtnode = su_rbt_search(shttype->stt_aname_rbt, a->ai_name);
                if (rbtnode != NULL) {
                    rs_aname_t* an = su_rbtnode_getkey(rbtnode);
                    if ((uint)an->an_no == attr_n) {
                        /* It is the same instance */
                        ss_dassert(an->an_name == a->ai_name);
                        su_rbt_delete(shttype->stt_aname_rbt, rbtnode);
                    }
                }
                SsMemFree(a->ai_name);
            }
        }
        rbtnode = su_rbt_search(shttype->stt_aname_rbt, name);
        if (rbtnode != NULL) {
            su_rbt_delete(shttype->stt_aname_rbt, rbtnode);
        }
        a->ai_name = SsMemStrdup(name);
        su_rbt_insert(shttype->stt_aname_rbt, an_init(a->ai_name, attr_n));
        return (shttype);
}

/*#***********************************************************************\
 *
 *              shttype_setattrid
 *
 * Sets attribute ID of an existing attribute within a shared ttype
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      shttype - in out, use
 *              shared ttype object
 *
 *      attr_n - in
 *              attribute #
 *
 *      attr_id - in
 *              attribute ID
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void shttype_setattrid(
        void*       cd,
        rs_shttype_t* shttype,
        uint        attr_n,
        ulong       attr_id)
{
        rs_attr_t* a;

        ss_dprintf_3(("%s: shttype_setattrid\n", __FILE__));
        SS_NOTUSED(cd);

        ss_dassert(attr_n < shttype->stt_nattrs);
        a = &shttype->stt_attr_arr[attr_n];
        a->ai_attrid = attr_id;
}

/*#***********************************************************************\
 *
 *              shttype_setdefaultvalue
 *
 * Sets the default value of a shared ttype
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      shttype - in out, use
 *              shared ttype object
 *
 *      attr_n - in
 *              attribute #
 *
 *      defaval - in
 *              default value
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void shttype_setdefaultvalue(
        void*       cd,
        rs_shttype_t* shttype,
        uint        attr_n,
        char*       default_value,
        bool        default_value_isnull,
        rs_aval_t*  aval)
{
        rs_attr_t* a;

        ss_dprintf_3(("%s: shttype_setdefaultvalue\n", __FILE__));
        SS_NOTUSED(cd);

        ss_dassert(attr_n < shttype->stt_nattrs);
        a = &shttype->stt_attr_arr[attr_n];

        if (default_value != NULL && !default_value_isnull) { /* default value given and not null */
           a->ai_defaultvalue = SsMemStrdup(default_value);
           a->ai_defaultvalueisnull = FALSE;
           a->ai_defaultvalueisset = TRUE;
        } else if (default_value == NULL && default_value_isnull) { /* default value is null */
           a->ai_defaultvalue = NULL;
           a->ai_defaultvalueisnull = TRUE;
           a->ai_defaultvalueisset = TRUE;
        } else if (default_value == NULL && !default_value_isnull) { /* no default value given */
           a->ai_defaultvalue = NULL;
           a->ai_defaultvalueisnull = FALSE;
           a->ai_defaultvalueisset = FALSE;
        } else { /* should not come here! */
           a->ai_defaultvalue = NULL;
           a->ai_defaultvalueisnull = FALSE;
           a->ai_defaultvalueisset = FALSE;
        }
        a->ai_aval = rs_aval_create(cd, &a->ai_atype);
        rs_aval_assign(
              cd,
              &a->ai_atype,
              a->ai_aval,
              &a->ai_atype,
              aval,
              NULL);

}

/*#***********************************************************************\
 *
 *              shttype_createbyttype
 *
 * Creates a new shared ttype instance by taking a copy of a ttype
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in
 *              ttype object
 *
 * Return value - give :
 *      new shared ttype object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rs_shttype_t* shttype_createbyttype(
    void*       cd,
    rs_ttype_t* ttype)
{
        uint nattrs;
        uint i;
        size_t size;
        rs_shttype_t* shttype;

        ss_dprintf_3(("%s: shttype_createbyttype\n", __FILE__));
        SS_NOTUSED(cd);
        SS_MEMOBJ_INC(SS_MEMOBJ_TTYPE, rs_shttype_t);
        SS_PUSHNAME("shttype_createbyttype");

        nattrs = ttype->tt_shttype->stt_nattrs;
        size = SHTTYPE_SIZE(nattrs);
        shttype = SsMemAlloc(size);
        memcpy(shttype, ttype->tt_shttype, size);
        SHTTYPE_LINK_INIT(shttype);
        shttype->stt_readonly = FALSE;
        shttype->stt_sem = rs_sysi_getrslinksem(cd);

        shttype->stt_aname_rbt = su_rbt_inittwocmp(
                                    an_insertcmp,
                                    an_searchcmp,
                                    an_done);
#ifndef RS_SQLANO_EQ_PHYS
        if (shttype->stt_sqltophys != NULL) {
            size_t s = shttype->stt_nsqlattrs * sizeof(rs_ano_t);
            ss_dassert(s >= sizeof(rs_ano_t));
            shttype->stt_sqltophys = SsMemAlloc(s);
            memcpy(shttype->stt_sqltophys, ttype->tt_shttype->stt_sqltophys, s);
        }
#endif
        for (i = 0; i < nattrs; i++) {
            rs_attr_t*  a;
            rs_aval_t*  defval;
            
            a = &shttype->stt_attr_arr[i];
            if (a->ai_name != NULL) {
                su_rbt_node_t* rbtnode;

                rbtnode = su_rbt_search(shttype->stt_aname_rbt, a->ai_name);
                if (rbtnode != NULL) {
                    su_rbt_delete(shttype->stt_aname_rbt, rbtnode);
                }
                a->ai_name = SsMemStrdup(a->ai_name);
                su_rbt_insert(shttype->stt_aname_rbt, an_init(a->ai_name, i));
            }
            if (a->ai_defaultvalue != NULL) {
                a->ai_defaultvalue = SsMemStrdup(a->ai_defaultvalue);
            }
            if (a->ai_aval != NULL) {
                a->ai_aval = rs_aval_copy(cd, &a->ai_atype, a->ai_aval);
            }
            defval = rs_atype_getcurrentdefault(cd, &a->ai_atype);
            if (defval != NULL) {
                rs_atype_insertcurrentdefault(
                        cd,
                        &a->ai_atype,
                        rs_aval_copy(cd, &a->ai_atype, defval));
            }
            defval = rs_atype_getoriginaldefault(cd, &a->ai_atype);
            if (defval != NULL) {
                rs_atype_insertoriginaldefault(
                        cd,
                        &a->ai_atype,
                        rs_aval_copy(cd, &a->ai_atype, defval));
            }
        }
        ss_debug(
            if (ttype->tt_shttype->stt_name != NULL) {
                shttype->stt_name = SsMemStrdup(ttype->tt_shttype->stt_name);
            }
        );
        SS_POPNAME;
        return (shttype);
}

/*#***********************************************************************\
 * 
 *		shttype_createbyttypeif_sementerif
 * 
 * Creates a new copy of shttype if needed and enters mutex semaphore
 * if needed.
 * 
 * Parameters : 
 * 
 *		cd - 
 *			
 *			
 *		ttype - 
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
static SsSemT* shttype_createbyttypeif_sementerif(
        rs_sysi_t*  cd,
        rs_ttype_t* ttype)
{
        uint nlink;
        rs_shttype_t* shttype;
        SsSemT* sem;

        ss_dprintf_3(("shttype_createbyttypeif_sementerif\n"));

        shttype = ttype->tt_shttype;

        sem = shttype->stt_sem;

        SsSemEnter(sem);

        nlink = SHTTYPE_LINK_GET(shttype);
        if (nlink > 1) {
            ss_dprintf_4(("shttype_createbyttypeif_sementerif:copy shttype\n"));
            ttype->tt_shttype = shttype_createbyttype(cd, ttype);
            nlink = SHTTYPE_LINK_DEC(shttype);
            ss_dassert(nlink >= 1);
            ss_dassert(!ttype->tt_shttype->stt_readonly);
        }
        return(sem);
}

/*##**********************************************************************\
 *
 *              rs_ttype_create
 *
 * Member of the SQL function block.
 * Creates a new tuple type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 * Return value - give :
 *
 *      Pointer into the newly allocated tuple type object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_ttype_t* rs_ttype_create(cd)
    void *cd;
{
        rs_ttype_t* ttype;

        SS_PUSHNAME("rs_ttype_create");

        ttype = SSMEM_NEW(rs_ttype_t);
        ttype->tt_shttype = shttype_create(cd);
        CHECK_TTYPE(ttype);

        SS_POPNAME;

        return(ttype);
}

/*#***********************************************************************\
 *
 *              shttype_free
 *
 * Frees shared ttype
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      shttype - in, take
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
static void shttype_free(
    void*       cd,
    rs_shttype_t* shttype)
{
        uint i;
        rs_attr_t* a;

        ss_dprintf_3(("%s: shttype_free\n", __FILE__));
        SS_NOTUSED(cd);
        SS_MEMOBJ_DEC(SS_MEMOBJ_TTYPE);

        su_rbt_done(shttype->stt_aname_rbt);
        for (i = 0; i < shttype->stt_nattrs; i++) {
            a = &shttype->stt_attr_arr[i];
            if (a->ai_name != NULL) {
                SsMemFree(a->ai_name);
            }
            if (a->ai_defaultvalue != NULL) {
                SsMemFree(a->ai_defaultvalue);
            }
            if (a->ai_aval != NULL) {
                rs_aval_free(cd, &a->ai_atype, a->ai_aval);
            }
            rs_atype_releasedefaults(cd, &a->ai_atype);
        }
#ifndef RS_SQLANO_EQ_PHYS
        if (shttype->stt_sqltophys != NULL) {
            SsMemFree(shttype->stt_sqltophys);
        }
#endif
        ss_debug(
            if (shttype->stt_name != NULL) {
                SsMemFree(shttype->stt_name);
            });
        SsMemFree(shttype);
}

/*##**********************************************************************\
 *
 *              rs_ttype_free
 *
 * Member of the SQL function block.
 * Releases a tuple type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, take
 *              pointer into a tuple type object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_ttype_free(void* cd, rs_ttype_t* ttype)
{
        uint nlink;
        rs_shttype_t* shttype;

        shttype = ttype->tt_shttype;

        SsSemEnter(shttype->stt_sem);

        nlink = SHTTYPE_LINK_DEC(shttype);
        if (nlink == 0) {
            SsSemExit(shttype->stt_sem);
            shttype_free(cd, shttype);
        } else {
            SsSemExit(shttype->stt_sem);
        }

        SsMemFree(ttype);
}

/*##**********************************************************************\
 *
 *              rs_ttype_addpseudoatypes
 *
 * Adds pseudo atypes to the ttype.
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
void rs_ttype_addpseudoatypes(
        void* cd,
        rs_ttype_t* ttype)
{
#ifndef SS_MYSQL
        int colpos;
        rs_atype_t* atype;

        CHECK_TTYPE(ttype);

        /* Add ROWID pseudo attribute.
         */
        colpos = RS_TTYPE_NATTRS(cd, ttype); /* Get position for ROWID */
        atype = rs_atype_initrowid(cd);

        rs_ttype_setatype(cd, ttype, colpos, atype);
        rs_ttype_setaname(cd, ttype, colpos, (char *)RS_PNAME_ROWID);

        rs_atype_free(cd, atype);

        /* Add ROWVER pseudo attribute.
         */
        colpos = RS_TTYPE_NATTRS(cd, ttype); /* Get position for ROWVER */
        atype = rs_atype_initrowver(cd, TRUE);

        rs_ttype_setatype(cd, ttype, colpos, atype);
        rs_ttype_setaname(cd, ttype, colpos, (char *)RS_PNAME_ROWVER);

        rs_atype_free(cd, atype);

#ifdef SS_SYNC
        /* Add SYNC_HISTORY_DELETED pseudo attribute.
         */
        colpos = RS_TTYPE_NATTRS(cd, ttype); /* Get position for RS_PNAME_ROWFLAGS */
        atype = rs_atype_initrowflags(cd);

        rs_ttype_setatype(cd, ttype, colpos, atype);
        rs_ttype_setaname(cd, ttype, colpos, (char *)RS_PNAME_ROWFLAGS);

        rs_atype_free(cd, atype);

        /* Add SYNC_ISPUBLTUPLE pseudo attribute.
         */
        colpos = RS_TTYPE_NATTRS(cd, ttype); /* Get position for SYNC_ISPUBLTUPLE */
        atype = rs_atype_initsyncispubltuple(cd, TRUE);
        rs_atype_setsync(cd, atype, TRUE);

        rs_ttype_setatype(cd, ttype, colpos, atype);
        rs_ttype_setaname(cd, ttype, colpos, (char *)RS_PNAME_SYNC_ISPUBLTUPLE);

        rs_atype_free(cd, atype);

        /* Add SYNC_TUPLE_VERSION pseudo attribute.
         */
        colpos = RS_TTYPE_NATTRS(cd, ttype); /* Get position for SYNC_TUPLE_VERSION */
        atype = rs_atype_initsynctuplevers(cd, TRUE);
        rs_atype_setsync(cd, atype, TRUE);

        rs_ttype_setatype(cd, ttype, colpos, atype);
        rs_ttype_setaname(cd, ttype, colpos, (char *)RS_PNAME_SYNCTUPLEVERS);

        rs_atype_free(cd, atype);

#endif /* SS_SYNC */
#endif /* !SS_MYSQL */
}

/*##**********************************************************************\
 *
 *              rs_ttype_setatype
 *
 * Sets the type of an attribute in a tuple type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in out, use
 *              pointer to tuple type object
 *
 *      attr_n - in
 *              number of the attribute to be set ( 0 = first )
 *
 *      atype - in, use
 *              the attribute type object according to which the tuple
 *          type is to be set
 *
 * Return value :
 *
 * Limitations  :
 *
 * According to the original specification, "holes" in a tuple type object
 * are not allowed. This is currently ensured by checking that every new
 * attribute type is inserted immediately after the last attribute type
 * so far.
 *
 * Globals used :
 */
void rs_ttype_setatype(cd, ttype, attr_n, atype)
    void*       cd;
    rs_ttype_t* ttype;
    uint        attr_n;
    rs_atype_t* atype;
{
        SsSemT* sem;

        ss_dprintf_3(("%s: rs_ttype_setatype\n", __FILE__));
        CHECK_TTYPE(ttype);

        sem = shttype_createbyttypeif_sementerif(cd, ttype);

        ttype->tt_shttype = shttype_setatype(cd, ttype->tt_shttype, attr_n, atype);

        if (sem != NULL) {
            SsSemExit(sem);
        }
}

/*#**********************************************************************\
 *
 *              rs_ttype_setaname
 *
 * Sets the name of an attribute in a tuple type object
 * For SQL it is allowed to add new attribute to the tuple type by calling
 * rs_ttype_setaname. We dassert it here.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in out, use
 *              pointer to tuple type object
 *
 *      attr_n - in
 *              number of the attribute to be set ( 0 = first )
 *
 *      name - in, use
 *              the new name for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_ttype_setaname(
    void*       cd,
    rs_ttype_t* ttype,
    uint        attr_n,
    char*       name)
{
        SsSemT* sem;

        ss_dprintf_3(("%s: rs_ttype_setaname_caller\n", __FILE__));
        CHECK_TTYPE(ttype);

        sem = shttype_createbyttypeif_sementerif(cd, ttype);

        ttype->tt_shttype =
            shttype_setaname_caller(
                cd, ttype->tt_shttype, attr_n, name, FALSE);

        if (sem != NULL) {
            SsSemExit(sem);
        }
}

/*##**********************************************************************\
 *
 *              rs_ttype_set_default_value
 *
 * Sets the default value of an attribute in a tuple type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in out, use
 *              pointer to tuple type object
 *
 *      attr_n - in
 *              number of the attribute to be set ( 0 = first )
 *
 *      default_value - in, use
 *              the default value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_ttype_set_default_value(cd, ttype, attr_n, default_value, default_value_isnull, aval)
    void*       cd;
    rs_ttype_t* ttype;
    uint        attr_n;
    char*       default_value;
    bool        default_value_isnull;
    rs_aval_t*  aval;
{
        SsSemT* sem;

        ss_dprintf_3(("%s: rs_ttype_setavalue_default\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_TTYPE(ttype);

        sem = shttype_createbyttypeif_sementerif(cd, ttype);

        shttype_setdefaultvalue(cd, ttype->tt_shttype, attr_n, default_value, default_value_isnull, aval);

        if (sem != NULL) {
            SsSemExit(sem);
        }
}

#ifndef rs_ttype_nattrs
/*##**********************************************************************\
 *
 *              rs_ttype_nattrs
 *
 * Finds the total number of attributes (system + user defined)
 * in a tuple type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              pointer to tuple type object
 *
 * Return value :
 *
 *      number of attributes in a tuple type object
 *
 * Limitations  :
 *
 * Globals used :
 */
uint rs_ttype_nattrs(cd, ttype)
    void*       cd;
    rs_ttype_t* ttype;
{
        ss_dprintf_3(("%s: rs_ttype_nattrs\n", __FILE__));
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        return (_RS_TTYPE_NATTRS_(cd, ttype));
}
#endif /* defined(rs_ttype_nattrs) */

#ifndef rs_ttype_atype
/*##**********************************************************************\
 *
 *              rs_ttype_atype
 *
 * Finds the type of a specified attribute of a tuple type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              pointer to tuple type object
 *
 *      attr_n - in
 *              number of the attribute to find ( 0 = first )
 *
 * Return value - ref :
 *
 *      !NULL, pointer into the attribute type object in the given tuple type
 *      NULL, if the tuple type does not have the given attribute
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_ttype_atype(cd, ttype, attr_n)
    void*       cd;
    rs_ttype_t* ttype;
    uint        attr_n;
{
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        ss_dassert(attr_n < ttype->tt_shttype->stt_nattrs);
        return (_RS_TTYPE_ATYPE_(cd, ttype, attr_n));
}
#endif /* defined(rs_ttype_atype) */

/*##**********************************************************************\
 *
 *              rs_ttype_aname
 *
 * Member of the SQL function block.
 * Finds the name of a specified attribute of a tuple type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              pointer to tuple type object
 *
 *      attr_n - in
 *              number of the attribute to be set ( 0 = first )
 *
 * Return value - ref :
 *
 *      Pointer to the attribute name string. Pointer into empty string is
 *      returned if the tuple type does not have the specified attribute
 *      or the attribute name is empty.
 *
 * Limitations  :
 *
 * Globals used :
 */
char* rs_ttype_aname(cd, ttype, attr_n)
    void*       cd;
    rs_ttype_t* ttype;
    uint        attr_n;
{
        rs_attr_t* a = NULL;

        ss_dprintf_3(("%s: rs_ttype_aname\n", __FILE__));
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        if (ttype->tt_shttype->stt_nattrs > attr_n) {
            a = &ttype->tt_shttype->stt_attr_arr[attr_n];
        }
        if (a != NULL && a->ai_name != NULL) {
            return(a->ai_name);
        }
        return((char *)"");
}

/*##**********************************************************************\
 *
 *              rs_ttype_anobyname
 *
 * Returns the attribute number of given attribute name.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              pointer to tuple type object
 *
 *      aname - in, use
 *              attribute name
 *
 * Return value :
 *
 *      Physical attribute number (Starts from 0)
 *      -1 (RS_ANO_NULL), if the attribute name is not found
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_ano_t rs_ttype_anobyname(
        void*       cd,
        rs_ttype_t* ttype,
        char*       aname)
{
        rs_shttype_t* shttype;
        rs_aname_t* an;
        su_rbt_node_t* rbtnode;

        SS_NOTUSED(cd);

        if (aname == NULL) {
            return RS_ANO_NULL;
        }
        
        shttype = ttype->tt_shttype;
        rbtnode = su_rbt_search(shttype->stt_aname_rbt, aname);
        if (rbtnode == NULL) {
            ss_dassert(RS_ANO_NULL == -1);
            return (RS_ANO_NULL);
        }
        an = su_rbtnode_getkey(rbtnode);
        ss_dassert(an != NULL);
        ss_dassert(an->an_name != NULL);
        return (an->an_no);
}

/*##**********************************************************************\
 *
 *              rs_ttype_copy
 *
 * Member of the SQL function block.
 * Makes a copy of a tuple type.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              pointer into tuple type object
 *
 * Return value - give :
 *
 *      copy of the tuple type object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_ttype_t* rs_ttype_copy(cd, ttype)
    void*       cd __attribute__ ((unused));
    rs_ttype_t* ttype;
{
        rs_shttype_t* shttype;

        ss_dprintf_3(("%s: rs_ttype_copy\n", __FILE__));
        CHECK_TTYPE(ttype);

        shttype = ttype->tt_shttype;

        ttype = SSMEM_NEW(rs_ttype_t);

        SsSemEnter(shttype->stt_sem);

        ss_dassert(SHTTYPE_LINK_GET(shttype) >= 1);
        (void)SHTTYPE_LINK_INC(shttype);

        SsSemExit(shttype->stt_sem);

        ttype->tt_shttype = shttype;

        return (ttype);
}

/*##**********************************************************************\
 * 
 *		rs_ttype_issame
 * 
 * This function checks if two ttypes are same. They are same if column
 * types and column names are same.
 * 
 * Parameters : 
 * 
 *		cd - 
 *			
 *			
 *		ttype1 - 
 *			
 *			
 *		ttype2 - 
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
bool rs_ttype_issame(
        void*       cd,
        rs_ttype_t* ttype1,
        rs_ttype_t* ttype2)
{
        uint nattrs;
        uint i;

        ss_dprintf_3(("%s: rs_ttype_issame\n", __FILE__));
        CHECK_TTYPE(ttype1);
        CHECK_TTYPE(ttype2);

        nattrs = rs_ttype_nattrs(cd, ttype1);
        if (nattrs != rs_ttype_nattrs(cd, ttype2)) {
            return(FALSE);
        }
        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype1;
            rs_atype_t* atype2;
            char* aname1;
            char* aname2;

            atype1 = rs_ttype_atype(cd, ttype2, i);
            atype2 = rs_ttype_atype(cd, ttype2, i);
            if (!rs_atype_issame(cd, atype1, atype2)) {
                return(FALSE);
            }
            aname1 = rs_ttype_aname(cd, ttype1, i);
            aname2 = rs_ttype_aname(cd, ttype2, i);
            if (strcmp(aname1, aname2) != 0) {
                return(FALSE);
            }
        }
        return(TRUE);
}

/*##**********************************************************************\
 *
 *              rs_ttype_setattrid
 *
 * Sets the attribute id of an attribute.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in out, use
 *              pointer to tuple type object
 *
 *      attr_n - in, use
 *              physical attribute number
 *
 *      attr_id - in
 *              attribute id
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_ttype_setattrid(
        void*       cd,
        rs_ttype_t* ttype,
        uint        attr_n,
        ulong       attr_id)
{
        SsSemT* sem;

        ss_dprintf_3(("%s: rs_ttype_setattrid\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_TTYPE(ttype);

        sem = shttype_createbyttypeif_sementerif(cd, ttype);

        shttype_setattrid(cd, ttype->tt_shttype, attr_n, attr_id);

        if (sem != NULL) {
            SsSemExit(sem);
        }
}

/*##**********************************************************************\
 *
 *              rs_ttype_attrid
 *
 * Returns the attribute id of an attribute.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              pointer to tuple type object
 *
 *      attr_n - in
 *              physical attribute number
 *
 * Return value :
 *
 *      attribute id
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong rs_ttype_attrid(
        void*       cd,
        rs_ttype_t* ttype,
        uint        attr_n)
{
        rs_shttype_t* shttype;
        rs_attr_t* a;

        ss_dprintf_3(("%s: rs_ttype_attrid\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_TTYPE(ttype);

        shttype = ttype->tt_shttype;
        ss_dassert(attr_n < shttype->stt_nattrs);
        a = &shttype->stt_attr_arr[attr_n];
        return(a->ai_attrid);
}

#ifndef RS_SQLANO_EQ_PHYS

/*##**********************************************************************\
 *
 *              rs_ttype_physanotosql
 *
 * Converts the physical attribute number to sql-attribute number.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              pointer to tuple type object
 *
 *      phys_attr_n - in
 *              physical attribute number
 *
 * Return value :
 *
 *      Physical attribute number in tuple
 *      RS_ANO_NULL, the specified attribute is a system attribute and can
 *                    not be accessed from sql
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_ano_t rs_ttype_physanotosql(cd, ttype, phys_attr_n)
    void*       cd;
    rs_ttype_t* ttype;
    rs_ano_t    phys_attr_n;
{
        rs_ano_t ano;

        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);
        ss_dassert(phys_attr_n >= 0
            && phys_attr_n < (rs_ano_t)ttype->tt_shttype->stt_nattrs);

        ano = ttype->tt_shttype->stt_attr_arr[phys_attr_n].ai_sqlano;
        ss_dassert(ano == RS_ANO_NULL ||
            (ano >= 0 && ano < ttype->tt_shttype->stt_nsqlattrs));
        return (ano);
}

#ifndef SS_NOSQL

/*##**********************************************************************\
 *
 *              rs_ttype_sql_setatype
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_ttype_setatype.
 */
void rs_ttype_sql_setatype(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n,
        rs_atype_t* atype
) {
        rs_ano_t phys_ano;
        rs_shttype_t* shttype;

        CHECK_TTYPE(ttype);
        shttype = ttype->tt_shttype;
        ss_dassert(sql_attr_n <= (uint)shttype->stt_nsqlattrs);

        if (sql_attr_n < (uint)shttype->stt_nsqlattrs) {
            phys_ano = shttype->stt_sqltophys[sql_attr_n];
        } else {
            phys_ano = shttype->stt_nattrs;
        }
        rs_ttype_setatype(cd, ttype, phys_ano, atype);
        ss_dassert(!ttype->tt_shttype->stt_readonly);
}

/*##**********************************************************************\
 *
 *              rs_ttype_sql_setaname
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_ttype_setaname_caller.
 */
void rs_ttype_sql_setaname(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n,
        char*       name
) {
        rs_ano_t phys_ano;
        SsSemT* sem;

        CHECK_TTYPE(ttype);

        sem = shttype_createbyttypeif_sementerif(cd, ttype);

        ss_dassert(sql_attr_n <= (uint)ttype->tt_shttype->stt_nsqlattrs);

        if (sql_attr_n < (uint)ttype->tt_shttype->stt_nsqlattrs) {
            phys_ano = ttype->tt_shttype->stt_sqltophys[sql_attr_n];
        } else {
            phys_ano = ttype->tt_shttype->stt_nattrs;
        }
        ttype->tt_shttype =
            shttype_setaname_caller(cd, ttype->tt_shttype, phys_ano, name, TRUE);

        if (sem != NULL) {
            SsSemExit(sem);
        }
}

#endif /* SS_NOSQL */

/*##**********************************************************************\
 *
 *              rs_ttype_sql_aname
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_ttype_aname.
 */
char* rs_ttype_sql_aname(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
) {
        rs_attr_t* a = NULL;
        rs_shttype_t* shttype;

        ss_dprintf_3(("%s: rs_ttype_aname\n", __FILE__));
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        shttype = ttype->tt_shttype;
        if ((uint)shttype->stt_nsqlattrs > sql_attr_n) {
            uint attr_n;

            attr_n = shttype->stt_sqltophys[sql_attr_n];
            a = &shttype->stt_attr_arr[attr_n];
        }
        if (a != NULL && a->ai_name != NULL) {
            return(a->ai_name);
        }
        return((char *)"");
}

/*##**********************************************************************\
 *
 *              rs_ttype_sql_defaultvalue
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_ttype_aname.
 */
char* rs_ttype_sql_defaultvalue(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
) {
        rs_attr_t* a = NULL;
        rs_shttype_t* shttype;

        ss_dprintf_3(("%s: rs_ttype_sql_defaultvalue\n", __FILE__));
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        shttype = ttype->tt_shttype;
        if ((uint)shttype->stt_nsqlattrs > sql_attr_n) {
            uint attr_n;

            attr_n = shttype->stt_sqltophys[sql_attr_n];
            a = &shttype->stt_attr_arr[attr_n];
        }
        if (a != NULL && a->ai_defaultvalue != NULL) {
            return(a->ai_defaultvalue);
        }
        return(NULL);
}

/*##**********************************************************************\
 *
 *              rs_ttype_sql_defaultvalueisnull
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_ttype_aname.
 */
bool rs_ttype_sql_defaultvalueisnull(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
) {
        rs_attr_t* a = NULL;
        rs_shttype_t* shttype;

        ss_dprintf_3(("%s: rs_ttype_sql_defaultvalueisnull\n", __FILE__));
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        shttype = ttype->tt_shttype;
        if ((uint)shttype->stt_nsqlattrs > sql_attr_n) {
            uint attr_n;

            attr_n = shttype->stt_sqltophys[sql_attr_n];
            a = &shttype->stt_attr_arr[attr_n];
        }
        if (a != NULL) {
            return(a->ai_defaultvalueisnull);
        }
        return(FALSE);
}

/*##**********************************************************************\
 *
 *              rs_ttype_sql_defaultvalueisset
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_ttype_aname.
 */
bool rs_ttype_sql_defaultvalueisset(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
) {
        rs_attr_t* a = NULL;
        rs_shttype_t* shttype;

        ss_dprintf_3(("%s: rs_ttype_sql_defaultvalueisset\n", __FILE__));
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        shttype = ttype->tt_shttype;
        if ((uint)shttype->stt_nsqlattrs > sql_attr_n) {
            uint attr_n;

            attr_n = shttype->stt_sqltophys[sql_attr_n];
            a = &shttype->stt_attr_arr[attr_n];
        }
        if (a != NULL) {
            return(a->ai_defaultvalueisset);
        }
        return(FALSE);
}

/*##**********************************************************************\
 *
 *              rs_ttype_sql_defaultaval
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_ttype_aname.
 */
rs_aval_t* rs_ttype_sql_defaultaval(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
) {
        rs_attr_t* a = NULL;
        rs_shttype_t* shttype;
        rs_aval_t* aval = NULL;

        ss_dprintf_3(("%s: rs_ttype_sql_defaultaval\n", __FILE__));
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        shttype = ttype->tt_shttype;
        if ((uint)shttype->stt_nsqlattrs > sql_attr_n) {
            uint attr_n;

            attr_n = shttype->stt_sqltophys[sql_attr_n];
            a = &shttype->stt_attr_arr[attr_n];
        }
        if (a != NULL) {
            aval = a->ai_aval;
        }
        return(aval);
}

/*##**********************************************************************\
 *
 *              rs_ttype_sql_anobyname
 *
 * Member of the SQL function block.
 * Parameter sql_attr_n is treated in a way that SQL sees the tuple type.
 * See the comment of rs_ttype_aname.
 */
int rs_ttype_sql_anobyname(
        void*       cd,
        rs_ttype_t* ttype,
        char*       attrname)
{
        rs_ano_t ano;

        ano = rs_ttype_anobyname(cd, ttype, attrname);
        if (ano == RS_ANO_NULL) {
            return (RS_ANO_NULL);
        }
        ss_dassert(0 <= ano && ano < (rs_ano_t)ttype->tt_shttype->stt_nattrs);
        ss_dassert(ttype->tt_shttype->stt_attr_arr[ano].ai_sqlano == RS_ANO_NULL ||
            (0 <= ttype->tt_shttype->stt_attr_arr[ano].ai_sqlano
            && ttype->tt_shttype->stt_attr_arr[ano].ai_sqlano <
               ttype->tt_shttype->stt_nsqlattrs));
        return (ttype->tt_shttype->stt_attr_arr[ano].ai_sqlano);
}

#endif  /* !RS_SQLANO_EQ_PHYS */

#ifdef SS_SYNC

/*##**********************************************************************\
 *
 *          rs_ttype_givevtpl
 *
 * Converts a ttype into vtpl (only able to
 * store attribute SQL type, length and scale)
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      ttype - in, use
 *          pointer to tuple type object
 *
 * Return value - give :
 *      pointer to new dynamic vtuple
 *
 * Limitations  :
 *
 * Globals used :
 */
dynvtpl_t rs_ttype_givevtpl(
        void* cd,
        rs_ttype_t* ttype)
{
        rs_ano_t i;
        rs_ano_t nattrs;
        dynvtpl_t dvtpl = NULL;

        dynvtpl_setvtpl(&dvtpl, VTPL_EMPTY);
        if (ttype == NULL) {
            return (dvtpl);
        }
        nattrs = rs_ttype_nattrs(cd, ttype);
        for (i = 0; i < nattrs; i++) {
            va_t va;
            va_t* p_va;
            long length;
            long scale;
            rs_sqldatatype_t sqldt;
            rs_atype_t* atype;

            atype = RS_TTYPE_ATYPE(cd, ttype, i);
            sqldt = RS_ATYPE_SQLDATATYPE(cd, atype);
            length = RS_ATYPE_LENGTH(cd, atype);
            scale = rs_atype_scale(cd, atype);
            va_setlong(&va, (long)sqldt);
            dynvtpl_appva(&dvtpl, &va);
            if (length == RS_LENGTH_NULL) {
                p_va = VA_NULL;
            } else {
                p_va = &va;
                va_setlong(&va, length);
            }
            dynvtpl_appva(&dvtpl, p_va);
            if (scale == RS_SCALE_NULL) {
                p_va = VA_NULL;
            } else {
                p_va = &va;
                va_setlong(&va, scale);
            }
            dynvtpl_appva(&dvtpl, p_va);
        }
        return (dvtpl);
}

/*##**********************************************************************\
 *
 *          rs_ttype_initfromvtpl
 *
 * Converts a vtuple into ttype (a counterpart of
 * rs_ttype_givevtpl)
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      p_vtpl - in, use
 *          pointer to vtuple that contains tuple type info
 *
 * Return value - give :
 *      pointer to newly create tuple type object or
 *      NULL if vtuple is empty
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_ttype_t* rs_ttype_initfromvtpl(
        void* cd,
        vtpl_t* p_vtpl)
{
        rs_ano_t i;
        va_t* p_va;
        ss_byte_t* vtpl_endmark;
        rs_ttype_t* ttype;
        va_index_t vtpl_glen;

        ss_dassert(p_vtpl != NULL);
        vtpl_glen = vtpl_grosslen(p_vtpl);
        if (vtpl_glen == 1) {
            return (NULL);
        }
        vtpl_endmark = (ss_byte_t*)p_vtpl + vtpl_glen;
        p_va = vtpl_getva_at(p_vtpl, 0);
        ttype = rs_ttype_create(cd);
        for (i = 0; (ss_byte_t*)p_va < vtpl_endmark; i++) {
            long sqldt;
            long scale;
            long length;
            rs_atype_t* atype;

            ss_dassert(va_netlen(p_va) >= 1);
            sqldt = va_getlong(p_va);
            p_va = vtpl_skipva(p_va);
            ss_dassert((ss_byte_t*)p_va < vtpl_endmark);
            if (va_testnull(p_va)) {
                length = (long)RS_LENGTH_NULL;
            } else {
                length = va_getlong(p_va);
            }
            p_va = vtpl_skipva(p_va);
            if (va_testnull(p_va)) {
                scale = (long)RS_SCALE_NULL;
            } else {
                scale = va_getlong(p_va);
            }
            p_va = vtpl_skipva(p_va);
            atype = rs_atype_initbysqldt(cd, (rs_sqldatatype_t)sqldt, length, scale);
            rs_ttype_setatype(cd, ttype, i, atype);
            rs_atype_free(cd, atype);
        }
        return (ttype);
}
#endif /* SS_SYNC */

#ifdef SS_DEBUG

void rs_ttype_setname(void* cd, rs_ttype_t* ttype, char* name)
{
        SsSemT* sem;

        ss_dprintf_3(("%s: rs_ttype_setname\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_TTYPE(ttype);

        sem = shttype_createbyttypeif_sementerif(cd, ttype);

        if (ttype->tt_shttype->stt_name != NULL) {
            SsMemFree(ttype->tt_shttype->stt_name);
        }
        if (name != NULL) {
            ttype->tt_shttype->stt_name = SsMemStrdup(name);
        } else {
            ttype->tt_shttype->stt_name = NULL;
        }

        if (sem != NULL) {
            SsSemExit(sem);
        }
}

/*##**********************************************************************\
 *
 *              rs_ttype_print
 *
 * Prints tuple type using SsDbgPrintf.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in
 *              tuple type
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_ttype_print(cd, ttype)
    void*       cd;
    rs_ttype_t* ttype;
{
        uint i;

        CHECK_TTYPE(ttype);
        ss_dassert(ttype->tt_shttype->stt_attr_arr != NULL);

        SsDbgPrintf("%-3s %-20s %-5s %-14s %-10s %-2s %-2s %-3s\n",
            "ANO", "ANAME", "ID", "TYPE", "VALTYPE", "LN", "SC", "NULLABLE");

        for (i = 0; i < ttype->tt_shttype->stt_nattrs; i++) {
            rs_attr_t* attr;
            attr = &ttype->tt_shttype->stt_attr_arr[i];
            SsDbgPrintf(
                "%3d %-20s %5ld ",
                i,
                attr->ai_name != NULL ? attr->ai_name : "NULL",
                attr->ai_attrid);
            rs_atype_print(cd, &attr->ai_atype);
        }
}

/*##**********************************************************************\
 *
 *              rs_ttype_quicksqlanotophys
 *
 * Quick SQL ano to physical ano mapper. This cannot be used to find out
 * the first free physical attr index !!!
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              pointer to tuple type object
 *
 *      sql_attr_n - in
 *              attribute number from SQL's point of view.
 *
 * Return value :
 *
 *      Physical attribute number
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_ano_t rs_ttype_quicksqlanotophys(cd, ttype, sql_attr_n)
    void*       cd;
    rs_ttype_t* ttype;
    uint        sql_attr_n;
{
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        ss_dassert((uint)ttype->tt_shttype->stt_nsqlattrs > sql_attr_n);
        return (_RS_TTYPE_QUICKSQLANOTOPHYS_(cd,ttype,sql_attr_n));
}

#endif /* SS_DEBUG */

void rs_ttype_setreadonly(
    void*       cd,
    rs_ttype_t* ttype,
        bool        readonlyp)
{
        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        ttype->tt_shttype->stt_readonly = readonlyp;
}

