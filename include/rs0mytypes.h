/*************************************************************************\
**  source       * rs0mytypes.h
**  directory    * res
**  description  * Global type definitions for SS_MYSQL compilation.
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


#ifndef RS0MYTYPES_H
#define RS0MYTYPES_H

#ifdef SS_MYSQL

/* define NOFLOAT as e.g. 1 if floating-point types are not to be used
   in query estimates or anything else
*/
#ifdef NOFLOAT
#define ESTCONST_T ulong
#define ESTCONST_FORMAT STR("%lu")
#else
#define ESTCONST_T double
#define ESTCONST_FORMAT STR("%lf")
#endif


/* type for field type */
#ifndef SQLFTYPE_T_DEFINED
#define SQLFTYPE_T_DEFINED
typedef struct sqlftypestruct sqlftype_t;
#endif

/* type for row type */
#ifndef SQLRTYPE_T_DEFINED
#define SQLRTYPE_T_DEFINED
typedef struct sqlrtypestruct sqlrtype_t;
#endif

/* type for field value instance */
#ifndef SQLFINST_T_DEFINED
#define SQLFINST_T_DEFINED
typedef struct sqlfinststruct sqlfinst_t;
#endif

/* type for row instance */
#ifndef SQLRINST_T_DEFINED
#define SQLRINST_T_DEFINED
typedef struct sqlrinststruct sqlrinst_t;
#endif

/* type for table handle */
#ifndef SQLTAB_T_DEFINED
#define SQLTAB_T_DEFINED
typedef struct sqltabstruct sqltab_t;
#endif

/* type for table cursor */
#ifndef SQLTABCUR_T_DEFINED
#define SQLTABCUR_T_DEFINED
typedef struct sqltabcurstruct sqltabcur_t;
#endif

/* type for external sort */
#ifndef SQLEXTSORT_T_DEFINED
#define SQLEXTSORT_T_DEFINED
typedef struct sqlextsortstruct sqlextsort_t;
#endif

/* type for view handle */
#ifndef SQLVIEW_T_DEFINED
#define SQLVIEW_T_DEFINED
typedef struct sqlviewstruct sqlview_t;
#endif

/* type for sequence handle */
#ifndef SQLSEQ_T_DEFINED
#define SQLSEQ_T_DEFINED
typedef struct sqlseqstruct sqlseq_t;
#endif

/* type for transaction handle */
#ifndef SQLTRANS_T_DEFINED
#define SQLTRANS_T_DEFINED
typedef struct sqltransstruct sqltrans_t;
#endif

/* type for error handle */
#ifndef SQLERR_T_DEFINED
#define SQLERR_T_DEFINED
typedef struct sqlerrstruct sqlerr_t;
#endif

/* type for SQL system */
#ifndef SQLSYSTEM_T_DEFINED
#define SQLSYSTEM_T_DEFINED
typedef struct sqlsystemstruct sqlsystem_t;
#endif

/* type for SQL cursor */
#ifndef SQLCUR_T_DEFINED
#define SQLCUR_T_DEFINED
typedef struct sqlcurstruct sqlcur_t;
#endif

#ifndef SQLLIST_T_DEFINED
#define SQLLIST_T_DEFINED
/* structure for a linked list item */
typedef struct sqlliststruct sqllist_t;
#endif

/* reverse options */
#ifndef SQL_REVERSE_T_DEFINED
#define SQL_REVERSE_T_DEFINED
typedef enum {
    SQL_REVERSE_YES =       0,
    SQL_REVERSE_NO =        1,
    SQL_REVERSE_POSSIBLY =  2
} sql_reverse_t;
#endif

/* privileges */
#ifndef SQL_PRIVILEGE_T_DEFINED
#define SQL_PRIVILEGE_T_DEFINED
typedef enum {
    SQL_PRIV_ALL =          1,
    SQL_PRIV_SELECT =       2,
    SQL_PRIV_INSERT =       4,
    SQL_PRIV_DELETE =       8,
    SQL_PRIV_UPDATE =      16,
    SQL_PRIV_REFERENCES =  32,
    SQL_PRIV_EXECUTE =     64,
    SQL_PRIV_SUBSCRIBE =   128
} sql_privilege_t;
#endif

/* NULL collation choices */
#ifndef SQL_NULLCOLL_T_DEFINED
#define SQL_NULLCOLL_T_DEFINED
typedef enum {
    SQL_NULLCOLL_LOW =      0,
    SQL_NULLCOLL_HIGH =     1,
    SQL_NULLCOLL_START =    2,
    SQL_NULLCOLL_END =      3,
    SQL_NULLCOLL_UNKNOWN =  4
} sql_nullcoll_t;
#endif

/* structure for ALTER TABLE action */
#ifndef SQLALTERACT_T_DEFINED
#define SQLALTERACT_T_DEFINED
typedef enum {
    SQL_ALTERACT_ADDCOLUMN =        0,
    SQL_ALTERACT_MODIFYCOLUMN =     1,
    SQL_ALTERACT_RENAMECOLUMN =     2,
    SQL_ALTERACT_DROPCOLUMN =       3,
    SQL_ALTERACT_ALTERCOLUMN =      4,
    SQL_ALTERACT_ALTERADDNOTNULL =  5,
    SQL_ALTERACT_ALTERDROPNOTNULL = 6,
    SQL_ALTERACT_MODIFYSCHEMA =     7,
    SQL_ALTERACT_ADDPRIMARYKEY =    8,
    SQL_ALTERACT_ADDUNIQUE =        9,
    SQL_ALTERACT_ADDCONSTRAINT =    10,
    SQL_ALTERACT_ADDFOREIGNKEY =    11,
    SQL_ALTERACT_DROPCONSTRAINT =   12,
    SQL_ALTERACT_SET =              13
} sqlalteract_t;
#endif


/* structure for referential action */
#ifndef SQLREFACT_T_DEFINED
#define SQLREFACT_T_DEFINED
typedef enum {
    SQL_REFACT_CASCADE =    0,
    SQL_REFACT_SETNULL =    1,
    SQL_REFACT_SETDEFAULT = 2,
    SQL_REFACT_RESTRICT =   3,
    SQL_REFACT_NOACTION =   4
} sqlrefact_t;
#endif

/* structure for a join key equality constraint definition */
#ifndef SQLEQJOIN_T_DEFINED
#define SQLEQJOIN_T_DEFINED
typedef struct eqjoinstruct {

    /* table number and column number for the first field (0 = first) */
    uint tab1, col1;

    /* table number and column number for the second field (0 = first) */
    uint tab2, col2;
} eqjoin_t;
#endif

/* relational operators */
#ifndef SQL_RELOP_T_DEFINED
#define SQL_RELOP_T_DEFINED
typedef enum {
    SQL_RELOP_EQUAL =     0,
    SQL_RELOP_NOTEQUAL =  1,
    SQL_RELOP_LT =        2,
    SQL_RELOP_GT =        3,
    SQL_RELOP_LE =        4,
    SQL_RELOP_GE =        5,
    SQL_RELOP_LIKE =      6,
    SQL_RELOP_ISNULL =    7,
    SQL_RELOP_ISNOTNULL = 8
} sql_relop_t;
#endif

#ifndef MME_RVAL_T_DEFINED
#define MME_RVAL_T_DEFINED
typedef struct mme_rval_st mme_rval_t;
#endif

#ifndef RS_HCOL_T_DEFINED
#define RS_HCOL_T_DEFINED
typedef struct rs_hcol_st rs_hcol_t;
#endif

#endif /* SS_MYSQL */

#endif /* RS0MYTYPES_H */


