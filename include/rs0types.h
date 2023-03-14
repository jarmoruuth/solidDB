/*************************************************************************\
**  source       * rs0types.h
**  directory    * res
**  description  * Global type definitions
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


#ifndef RS0TYPES_H
#define RS0TYPES_H

/******************************************************************

!!! THIS FILE SHOULD BE INCLUDED BEFORE ANY OTHER RS-HEADERS !!!

The following renaming has been taken in use

        SQL/TABVIA          prefix      TABSOLID            prefix
        ----------------------------------------------------------
        "field type"        ftype   ->  "attribute type"    atype
        "field instance"    finst   ->  "attribute value"   aval

        "row type"          rtype   ->  "tuple type"        ttype
        "row instance"      rinst   ->  "tuple value"       tval

        "table type"        tab     ->  "relation handle"   relh
        "view type"         view    ->  "view handle"       viewh

*******************************************************************/

#include <ssstdlib.h>

#include <ssc.h>

#include <su0err.h>

typedef enum {

        RSCHK_FREED = -2,
        RSCHK_ATYPE = 100,
        RSCHK_ATTRVALUE,
        RSCHK_ATTRVALUE_END, /* new: end mark for aval! */
        RSCHK_ATTRINST,
        RSCHK_TUPLETYPE,
        RSCHK_TUPLEVALUE,
        RSCHK_RELHTYPE,
        RSCHK_KEYTYPE,
        RSCHK_KEYPARTTYPE,
        RSCHK_RELCUR,
        RSCHK_TRANS,
        RSCHK_VIEWHTYPE,
        RSCHK_ANOCNVTYPE,
        RSCHK_CONSTR,
        RSCHK_ORDERBY,
        RSCHK_RBUF,
        RSCHK_RBUFDATA,
        RSCHK_PLAN,
        RSCHK_PLACONS,
        RSCHK_PLAREF,
        RSCHK_AVALARR,
        RSCHK_SQLI,
        RSCHK_REFI,
        RSCHK_SYSI,
        RSCHK_BB,
        RSCHK_BBV,
        RSCHK_HCOL,
        RSCHK_TREND,
        RSCHK_RSETCURINFO,
        RSCHK_RSETATTRINFO,
        RSCHK_RSETINFO,
        RSCHK_CARDIN,
        RSCHK_ADMINEVENT,
        RSCHK_EVENTTYPE,
        RSCHK_DEFNODE,
        RSCHK_VBUF,
        RSCHK_AGGRCTX,
        RSCHK_SYSILISTNODE,
        RSCHK_ENTNAME
} rs_check_t;

/*
        Common types for SQL interpreter and res-level.
*/

#define  sqlftypestruct     rsattrtypestruct
#define  sqlftype_t         rs_atype_t
 
#define  sqlrtypestruct     rstupletypestruct
#define  sqlrtype_t         rs_ttype_t

#define  sqlfinststruct     rsattrvaluestruct
#define  sqlfinst_t         rs_aval_t

#define  sqlrinststruct     rstuplevaluestruct
#define  sqlrinst_t         rs_tval_t

#define  sqltabstruct       tbrelhandlestruct
#define  sqltab_t           tb_relh_t

#define  sqltabcurstruct    tbrelcurstruct
#define  sqltabcur_t        tb_relcur_t

#define  sqlviewstruct      tbviewhstruct
#define  sqlview_t          tb_viewh_t

#define  sqltransstruct     tbtransstruct
#define  sqltrans_t         tb_trans_t

#define  SQLERR_T_DEFINED
#define  sqlerrstruct       suerrstruct
#define  sqlerr_t           su_err_t
#define  rs_err_t           su_err_t

#define  sqlextsortstruct   xs_sorter_st
#define  sqlextsort_t       xs_sorter_t

/*
        Types only for res-level.
*/

typedef struct rsrelhandlestruct rs_relh_t;
typedef struct rsviewhstruct     rs_viewh_t;
typedef struct rs_event_st       rs_event_t;
typedef struct rs_defnode_st     rs_defnode_t;
typedef struct rs_aval_accinfo_st rs_aval_accinfo_t;


/* type for key type */
typedef struct tbkeystruct rs_key_t;

/* type for authorization */
typedef struct rsauthstruct rs_auth_t;

/* type for relation cardinality */
typedef struct rs_cardin_st rs_cardin_t;

/* Client data type. */
typedef struct rssysinfostruct rs_sysi_t;

/*
#ifdef SS_MYSQL
#include <rs0mytypes.h>
#else
#include <c.h>
#include <sqlint.h>
#endif
*/
#include <rs0mytypes.h>

typedef int rs_ano_t;

/* Type used in estimate cost calculations. Type ESTCONST_T is defined
 * by SQL in sqlint.h
 */
#define rs_estcost_t ESTCONST_T

#define RS_ANO_NULL     (rs_ano_t)(-1)
#define RS_ANO_PSEUDO   (rs_ano_t)(-2)

#endif /* RS0TYPES_H */
