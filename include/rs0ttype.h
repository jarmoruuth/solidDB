/*************************************************************************\
**  source       * rs0ttype.h
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


#ifndef RS0TTYPE_H
#define RS0TTYPE_H

#include <ssenv.h>

#include <uti0vtpl.h>
#include <su0rbtr.h>
#include <su0parr.h>

#include "rs0types.h"
#include "rs0atype.h"
#include "rs0aval.h"

#if defined(RS_USE_MACROS) || defined(RS_INTERNAL)

typedef struct rs_shttype_st rs_shttype_t;

/* type for tuple type */
struct rstupletypestruct {
        rs_shttype_t* tt_shttype;
}; /* rs_ttype_t */


/* Type for attribute instance.
   Almost the same as presented in rs0atype.c,
   but here a name and attrid fields are added.
*/
typedef struct rsattrinststruct {
        ss_debug(rs_check_t ai_check;)  /* check field */
        char*       ai_name;        /* attribute name */
        ulong       ai_attrid;      /* attribute id in database */
#ifndef RS_SQLANO_EQ_PHYS
        rs_ano_t    ai_sqlano;      /* attribute number in sql */
#endif
        rs_atype_t  ai_atype;       /* attribute type */
        char*       ai_defaultvalue; /* default value of attribute , NULL if not given */
        bool        ai_defaultvalueisnull; /* is defaultvalue NULL? */
        bool        ai_defaultvalueisset; /* default value is set? */
        rs_aval_t*  ai_aval;        /* default value */
} rs_attr_t;

/* type for tuple shared type */
struct rs_shttype_st {
        ss_debug(int stt_check;)    /* rs_check_t check field */
        ss_debug(char* stt_name;)
        su_rbt_t*   stt_aname_rbt;  /* Rbtree for attribute names */
#ifndef RS_SQLANO_EQ_PHYS
        rs_ano_t*   stt_sqltophys;  /* SQL-to-phys ano translate array */
        rs_ano_t    stt_nsqlattrs;
#endif
        bool        stt_readonly;
        uint        stt_nattrs;
        ulong       stt_nlinks;
        SsSemT*     stt_sem;
        rs_attr_t   stt_attr_arr[1]; /* must be last field! */
};

#ifndef RS_SQLANO_EQ_PHYS
#define _RS_TTYPE_QUICKSQLANOTOPHYS_(cd,ttype,sqlano) \
        ((ttype)->tt_shttype->stt_sqltophys[sqlano])
#else
#define _RS_TTYPE_QUICKSQLANOTOPHYS_(cd,ttype,sqlano) (sqlano)
#endif

#define _RS_TTYPE_ATYPE_(cd, ttype, ano) \
        (&(ttype)->tt_shttype->stt_attr_arr[ano].ai_atype)

#define _RS_TTYPE_NATTRS_(cd, ttype) \
        ((ttype)->tt_shttype->stt_nattrs)

#ifdef SS_DEBUG

#define RS_TTYPE_ATYPE(cd, ttype, ano) \
        rs_ttype_atype(cd, ttype, ano)

#define RS_TTYPE_NATTRS(cd, ttype) \
        rs_ttype_nattrs(cd, ttype)

#define RS_TTYPE_QUICKSQLANOTOPHYS(cd,ttype,sqlano) \
        rs_ttype_quicksqlanotophys(cd,ttype,sqlano)

#else   /* SS_DEBUG */

#define RS_TTYPE_ATYPE(cd, ttype, ano) \
        _RS_TTYPE_ATYPE_(cd, ttype, ano)

#define RS_TTYPE_NATTRS(cd, ttype) \
        _RS_TTYPE_NATTRS_(cd, ttype)

#define RS_TTYPE_QUICKSQLANOTOPHYS(cd,ttype,sqlano) \
        _RS_TTYPE_QUICKSQLANOTOPHYS_(cd,ttype,sqlano)

#define rs_ttype_quicksqlanotophys(cd,ttype,sqlano) \
        _RS_TTYPE_QUICKSQLANOTOPHYS_(cd,ttype,sqlano)

#endif  /* SS_DEBUG */

#define CHECK_TTYPE(ttype) {\
                            ss_dassert(SS_CHKPTR(ttype));\
                            ss_dassert(ttype->tt_shttype != NULL);\
                            ss_dassert(ttype->tt_shttype->stt_check == RSCHK_TUPLETYPE);\
                           }

#endif /* defined(RS_USE_MACROS) || defined(RS_INTERNAL) */

#ifdef SS_DEBUG
rs_ano_t rs_ttype_quicksqlanotophys(
	void*       cd,
	rs_ttype_t* ttype,
	uint        sql_attr_n);
#endif

#ifdef RS_USE_MACROS

#define rs_ttype_atype(cd, ttype, ano) \
        _RS_TTYPE_ATYPE_(cd, ttype, ano)

#define rs_ttype_nattrs(cd, ttype) \
        _RS_TTYPE_NATTRS_(cd, ttype)

#else

uint rs_ttype_nattrs(
        void*       cd,
        rs_ttype_t* ttype);

rs_atype_t* rs_ttype_atype(
        void*       cd,
        rs_ttype_t* ttype,
        uint        phys_attr_n);

#endif /* RS_USE_MACROS */

rs_ttype_t* rs_ttype_create(
        void *cd
);

void rs_ttype_free(
        void*       cd,
        rs_ttype_t* ttype
);

void rs_ttype_addpseudoatypes(
        void* cd,
        rs_ttype_t* ttype
);

void rs_ttype_sql_setatype(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n,
        rs_atype_t* atype
);

void rs_ttype_sql_setaname(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n,
        char*       name
);

SS_INLINE uint rs_ttype_sql_nattrs(
        void*       cd,
        rs_ttype_t* ttype
);

SS_INLINE rs_atype_t* rs_ttype_sql_atype(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
);

char* rs_ttype_sql_aname(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
);

char* rs_ttype_sql_defaultvalue(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
);

bool rs_ttype_sql_defaultvalueisnull(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
);

bool rs_ttype_sql_defaultvalueisset(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
);

rs_aval_t* rs_ttype_sql_defaultaval(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
);

int rs_ttype_sql_anobyname(
        void*       cd,
        rs_ttype_t* ttype,
        char*       attrname
);


rs_ttype_t* rs_ttype_copy(
        void*       cd,
        rs_ttype_t* ttype
);

bool rs_ttype_issame(
        void*       cd,
        rs_ttype_t* ttype1,
        rs_ttype_t* ttype2
);

/* NOTE: Following services are not members of SQL-funblock */

/* These functions access the ttype object basing on physical
   attribute indices.
   This means we do not skip the system attributes in numbering.
*/
void rs_ttype_setatype(
        void*       cd,
        rs_ttype_t* ttype,
        uint        phys_attr_n,
        rs_atype_t* atype
);

void rs_ttype_setaname(
        void*       cd,
        rs_ttype_t* ttype,
        uint        phys_attr_n,
        char*       name
);

void rs_ttype_set_default_value(
        void*       cd,
        rs_ttype_t* ttype,
        uint        phys_attr_n,
        char*       default_value,
        bool        default_value_isnull,
        rs_aval_t*  aval
);

char* rs_ttype_aname(
        void*       cd,
        rs_ttype_t* ttype,
        uint        phys_attr_n
);

int rs_ttype_anobyname(
        void*       cd,
        rs_ttype_t* ttype,
        char*       aname
);

SS_INLINE rs_ano_t rs_ttype_sqlanotophys(
        void*       cd,
        rs_ttype_t* ttype,
        uint        sql_attr_n
);

rs_ano_t rs_ttype_physanotosql(
        void*       cd,
        rs_ttype_t* ttype,
        rs_ano_t    phys_attr_n
);

void rs_ttype_setattrid(
        void*       cd,
        rs_ttype_t* ttype,
        uint        phys_attr_n,
        ulong       attr_id
);

ulong rs_ttype_attrid(
        void*       cd,
        rs_ttype_t* ttype,
        uint        phys_attr_n
);

#ifdef SS_SYNC

dynvtpl_t rs_ttype_givevtpl(
        void* cd,
        rs_ttype_t* ttype);

rs_ttype_t* rs_ttype_initfromvtpl(
        void* cd, 
        vtpl_t* p_vtpl);

#endif /* SS_SYNC */

#ifdef SS_DEBUG

void rs_ttype_setname(
        void* cd,
        rs_ttype_t* ttype,
        char* name);

void rs_ttype_print(
        void*       cd,
        rs_ttype_t* ttype);

#endif /* SS_DEBUG */

void rs_ttype_setreadonly(
	void*       cd,
	rs_ttype_t* ttype,
        bool        readonlyp);

#ifdef RS_SQLANO_EQ_PHYS

#define rs_ttype_sql_setatype   rs_ttype_setatype
#define rs_ttype_sql_setaname   rs_ttype_setaname
#define rs_ttype_sql_nattrs     rs_ttype_nattrs
#define rs_ttype_sql_aname      rs_ttype_aname
#define rs_ttype_sql_anobyname  rs_ttype_anobyname
#define rs_ttype_physanotosql(cd,ttype,n) (n)
#define rs_ttype_sqlanotophys(cd,ttype,n) (n)

#endif

#if defined(RS0TTYPE_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              rs_ttype_sql_nattrs
 *
 * Member of the SQL function block.
 * Finds the number of user defined (SQL) attributes in a tuple type object
 * See the comment of rs_ttype_nattrs.
 */
SS_INLINE uint rs_ttype_sql_nattrs(
        void*       cd,
        rs_ttype_t* ttype)
{
        SS_NOTUSED(cd);
        CHECK_TTYPE(ttype);
        return (ttype->tt_shttype->stt_nsqlattrs);
}

/*##**********************************************************************\
 *
 *              rs_ttype_sql_atype
 *
 * Member of the SQL function block.
 *
 * Finds the type of a specified attribute of a tuple type object.
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
SS_INLINE rs_atype_t* rs_ttype_sql_atype(
        void*       cd,
        rs_ttype_t* ttype,
        uint        attr_n)
{
        rs_shttype_t* shttype;
        uint nattrs;
        rs_atype_t* atype;

        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        shttype = ttype->tt_shttype;
#ifndef RS_SQLANO_EQ_PHYS
        nattrs = shttype->stt_nsqlattrs;
#else
        nattrs = shttype->stt_nattrs;
#endif
        if (attr_n >= nattrs) {
            return (NULL);
        }
#ifndef RS_SQLANO_EQ_PHYS
        attr_n = shttype->stt_sqltophys[attr_n];
#endif
        atype = &shttype->stt_attr_arr[attr_n].ai_atype;
        if (RS_ATYPE_BUFISUNDEF(cd, atype)) {
            return (NULL);
        }
        return (atype);
}

/*##**********************************************************************\
 *
 *              rs_ttype_sqlanotophys
 *
 * Converts the SQL attribute number to physical number.
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
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE rs_ano_t rs_ttype_sqlanotophys(
    void*       cd,
    rs_ttype_t* ttype,
    uint        sql_attr_n)
{
        rs_shttype_t* shttype;

        CHECK_TTYPE(ttype);
        SS_NOTUSED(cd);

        shttype = ttype->tt_shttype;

        if ((uint)shttype->stt_nsqlattrs > sql_attr_n) {
            return (shttype->stt_sqltophys[sql_attr_n]);
        }
        if ((uint)shttype->stt_nsqlattrs == sql_attr_n) {
            return (shttype->stt_nattrs);
        }
        return(RS_ANO_NULL);
}

#endif /* defined(RS0TTYPE_C) || defined(SS_USE_INLINE) */


#endif /* RS0TTYPE_H */
