/*************************************************************************\
**  source       * dbe4srli.c
**  directory    * dbe
**  description  * System relations stuff
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

This file contains initialization data and routines for creating system
relations.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

v-attributes        <uti0va.h>
system definitions  <rs0sdefs.h>
attribute types     <rs0atype.h>
tuple types         <rs0ttype.h>
relation handles    <rs0relh.h>
view handles        <rs0viewh.h>
relation buffer     <rs0relh.h>

Preconditions:
-------------


Multithread considerations:
--------------------------

The initialization is legal only once upon opening the database system.

Example:
-------

main()
{
        ...                         /* init appropriate objects */
        rbuf = rs_rbuf_init(...)    /* create rel. buffer */
        dbe_srli_init(rbuf);        /* init. system relations */
        ...                         /* either open or create database */
        ...                         /* run database */
}

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssdebug.h>
#include <sstime.h>
#include <ssstdlib.h>
#include <ssstring.h>
#include <ssmem.h>

#include <su0bsrch.h>
#include <uti0va.h>
#include <rs0sdefs.h>
#include <rs0atype.h>
#include <rs0ttype.h>
#include <rs0relh.h>
#include <rs0viewh.h>
#include <rs0rbuf.h>
#include <rs0key.h>
#include <rs0entna.h>

#include "dbe4srli.h"

static char no[] = RS_AVAL_NO;
static char yes[] = RS_AVAL_YES;

#define varchar         ((char*)RS_TN_VARCHAR)
#define longvarchar     ((char*)RS_TN_LONG_VARCHAR)
#define character       ((char*)RS_TN_CHAR)
#define integer         ((char*)RS_TN_INTEGER)
#define smallint        ((char*)RS_TN_SMALLINT)
#define timestamp       ((char*)RS_TN_TIMESTAMP)
#define varbinary       ((char*)RS_TN_VARBINARY)
#define bigint          ((char*)RS_TN_BIGINT)
#ifdef SS_UNICODE_SQL
#define wchar           ((char*)RS_TN_WCHAR)
#define wvarchar        ((char*)RS_TN_WVARCHAR)
#define longwvarchar    ((char*)RS_TN_LONG_WVARCHAR)
#endif /* SS_UNICODE_SQL */

static char base_table[] = RS_AVAL_BASE_TABLE;
/* static char view[] = RS_AVAL_VIEW; */
static char sysname[] = RS_AVAL_SYSNAME;
#define syscatalog NULL
/* static char syscatalog[] = RS_AVAL_SYSNAME; */
/* static char information_schema[] = RS_AVAL_INFORMATION_SCHEMA; */


#define INT_NULL    (-1L)

#define KEYID(keyno,tableid) (long)((tableid) + (keyno) + 91)

struct dbe_srli_table_st {
        long            st_id;
        const char*           st_table_name;
        char*           st_table_type;
        char*           st_table_schema;
        char*           st_table_catalog;
        SsTimeT         st_creatime;
        char*           st_check;
        const char*           st_remarks;
};
typedef struct dbe_srli_table_st dbe_srli_table_t;

struct dbe_srli_column_st {
        long            sc_id;
        long            sc_rel_id;
        const char           *sc_column_name;
        long            sc_column_number;
        char           *sc_data_type;
        short           sc_sql_data_type_number;
      /*long            sc_data_type_number; (not used here!) */
        long            sc_char_max_length;
        long            sc_numeric_precision;
        short           sc_numeric_prec_radix;
        short           sc_numeric_scale;
        char           *sc_nullable;    /* "YES" / "NO " */
      /*short           sc_nullable_odbc; (not used here!) */
        char           *sc_format;
        void           *sc_default_val;
        long            sc_attr_type;
        char           *sc_remarks;
};
typedef struct dbe_srli_column_st dbe_srli_column_t;


struct dbe_srli_key_st {
        long            k_id;
        long            k_rel_id;
        const char           *k_key_name;
        char           *k_key_unique;   /* "YES" / "NO " */
        char           *k_clustering;   /* "YES" / "NO " */
        char           *k_primary;      /* "YES" / "NO " */
        char           *k_prejoined;    /* "YES" / "NO " */
        char           *k_schema;
        long            k_nref;
};
typedef struct dbe_srli_key_st dbe_srli_key_t;

struct dbe_srli_keypart_st {
        long            kp_id;
        long            kp_rel_id;
        long            kp_keyp_no;
        long            kp_attr_id;
        long            kp_attr_no;
        long            kp_attr_type;
        void           *kp_const_value;
        char           *kp_ascending;   /* "YES" / "NO " */
};
typedef struct dbe_srli_keypart_st dbe_srli_keypart_t;

struct dbe_srli_user_st {
        long            u_id;
        char           *u_name;
        char           *u_type;
        long            u_priv;
        char           *u_passw;
        long            u_priority;
        long            u_private;
};
typedef struct dbe_srli_user_st dbe_srli_user_t;

struct dbe_srli_ugroup_st {
        long            ug_u_id;
        long            ug_g_id;
};
typedef struct dbe_srli_ugroup_st dbe_srli_ugroup_t;

struct dbe_srli_relauth_st {
        long            ra_rel_id;
        long            ra_ug_id;
        long            ra_priv;
        long            rs_grant_id;
        SsTimeT         rs_grant_tim;
        char           *rs_grant_opt;
};
typedef struct dbe_srli_relauth_st dbe_srli_relauth_t;

struct dbe_srli_attauth_st {
        long            aa_rel_id;
        long            aa_attr_id;
        long            aa_ug_id;
        long            aa_priv;
        long            aa_grant_id;
        SsTimeT         aa_grant_tim;
};
typedef struct dbe_srli_attauth_st dbe_srli_attauth_t;

struct dbe_srli_view_st {
        long            v_v_id;
        char           *v_text;
        char           *v_check;
        char           *v_remarks;
};
typedef struct dbe_srli_view_st dbe_srli_view_t;

struct dbe_srli_cardinal_st {
        long            c_rel_id;
        long            c_cardin;
        SsTimeT         c_last_upd;
};
typedef struct dbe_srli_cardinal_st dbe_srli_cardinal_t;

struct dbe_srli_info_st {
        char           *i_property;
        char           *i_value_str;
        long            i_value_int;
};
typedef struct dbe_srli_info_st dbe_srli_info_t;

struct dbe_srli_synonym_st {
        long            s_target_id;
        long            s_synon;
};
typedef struct dbe_srli_synonym_st dbe_srli_synonym_t;


static dbe_srli_table_t sys_tables[] = {
        { RS_RELID_TABLES,     RS_RELNAME_TABLES,   base_table, sysname, syscatalog, 0L, NULL, "System table describing tables" },
        { RS_RELID_COLUMNS,    RS_RELNAME_COLUMNS,  base_table, sysname, syscatalog, 0L, NULL, "System table describing columns" },
#ifdef JYRI_REMOVED
        { RS_RELID_USERS,      RS_RELNAME_USERS,    base_table, sysname, syscatalog, 0L, NULL, "System table describing users"  },
#endif
        { RS_RELID_UROLE,      RS_RELNAME_UROLE,    base_table, sysname, syscatalog, 0L, NULL, "System table describing users' group memberships" },
        { RS_RELID_RELAUTH,    RS_RELNAME_RELAUTH,  base_table, sysname, syscatalog, 0L, NULL, "System table describing users' relationwise privileges" },
        { RS_RELID_ATTAUTH,    RS_RELNAME_ATTAUTH,  base_table, sysname, syscatalog, 0L, NULL, "System table describing users' attributewise privileges" },
        { RS_RELID_VIEWS,      RS_RELNAME_VIEWS,    base_table, sysname, syscatalog, 0L, NULL, "System table describing views" },
        { RS_RELID_KEYPARTS,   RS_RELNAME_KEYPARTS, base_table, sysname, syscatalog, 0L, NULL, "System table describing key parts" },
        { RS_RELID_KEYS,       RS_RELNAME_KEYS,     base_table, sysname, syscatalog, 0L, NULL, "System table describing key names" },
        { RS_RELID_CARDINAL,   RS_RELNAME_CARDINAL, base_table, sysname, syscatalog, 0L, NULL, "System table describing relation cardinality" },
        { RS_RELID_INFO,       RS_RELNAME_INFO,     base_table, sysname, syscatalog, 0L, NULL, "System table describing system information" },
        { RS_RELID_SYNONYM,    RS_RELNAME_SYNONYM,  base_table, sysname, syscatalog, 0L, NULL, "System table describing table synonyms (future extension)" }
};

#define INTDTYPE         integer,    RSSQLDT_INTEGER,    0,                RS_INT_PREC,   RS_INT_RADIX, RS_INT_SCALE
#define SMALLINTDTYPE    smallint,   RSSQLDT_SMALLINT,   0,                RS_INT_PREC,   RS_INT_RADIX, RS_INT_SCALE
#define VARCHARDTYPE     varchar,    RSSQLDT_VARCHAR,    RS_VARCHAR_DEFLEN,0,             INT_NULL,     RS_SCALE_NULL
#define LONGVARCHARDTYPE longvarchar,RSSQLDT_LONGVARCHAR,RS_LENGTH_NULL,   0,             INT_NULL,     RS_SCALE_NULL
#define CHARDTYPE(l)     character,  RSSQLDT_CHAR,       (l),              0,             INT_NULL,     RS_SCALE_NULL
#define DATEDTYPE        timestamp,  RSSQLDT_TIMESTAMP,  0,                RS_DATE_PREC,  INT_NULL,     RS_SCALE_NULL
#define VARBINARYDTYPE   varbinary,  RSSQLDT_VARBINARY,  RS_VARCHAR_DEFLEN,0,             INT_NULL,     RS_SCALE_NULL
#define BIGINTDTYPE      bigint,     RSSQLDT_BIGINT,     0,                RS_BIGINT_PREC,RS_BIGINT_RADIX, RS_BIGINT_SCALE

#ifdef SS_UNICODE_SQL
#define WCHARDTYPE(l)     wchar,      RSSQLDT_WCHAR,        (l),              0,          INT_NULL,     RS_SCALE_NULL
#define WVARCHARDTYPE     wvarchar,   RSSQLDT_WVARCHAR,     RS_VARCHAR_DEFLEN,0,          INT_NULL,     RS_SCALE_NULL
#define WLONGVARCHARDTYPE longwvarchar,RSSQLDT_WLONGVARCHAR,RS_LENGTH_NULL,   0,          INT_NULL,     RS_SCALE_NULL
#else /* SS_UNICODE_SQL */
#define WCHARDTYPE(l)    CHARDTYPE(l)
#define WVARCHARDTYPE    VARCHARDTYPE
#define WLONGVARCHARDTYPE LONGVARCHARDTYPE
#endif /* SS_UNICODE_SQL */

static dbe_srli_column_t sys_columns[] = {
        /*id  rel_id              column_name                   colno data_type         null fmt   defva attr_type          remarks */
        /*--  ------------------- ----------------------------- ----- -------------     ---- ----- ----- ------------------ ------- */
        { 0L, RS_RELID_TABLES,   RS_ANAME_TABLES_ID,              0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_TABLES,   RS_ANAME_TABLES_NAME,            1, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_TABLES,   RS_ANAME_TABLES_TYPE,            2, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_TABLES,   RS_ANAME_TABLES_SCHEMA,          3, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_TABLES,   RS_ANAME_TABLES_CATALOG,         4, WVARCHARDTYPE,    yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_TABLES,   RS_ANAME_TABLES_CREATIME,        5, DATEDTYPE,        no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_TABLES,   RS_ANAME_TABLES_CHECK,           6, WLONGVARCHARDTYPE,yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_TABLES,   RS_ANAME_TABLES_REMARKS,         7, WLONGVARCHARDTYPE,yes, NULL, NULL, RSAT_USER_DEFINED, NULL },

        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_ID,             0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_REL_ID,         1, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_COLUMN_NAME,    2, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_COLUMN_NUMBER,  3, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_DATA_TYPE,      4, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_SQLDTYPE_NO,    5, SMALLINTDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_DATA_TYPE_NO,   6, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_CHAR_MAX_LEN,   7, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_NUM_PRECISION,  8, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_NUM_PREC_RDX,   9, SMALLINTDTYPE,    yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_NUM_SCALE,      10,SMALLINTDTYPE,    yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_NULLABLE,       11,WCHARDTYPE(3),    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_NULLABLE_ODBC,  12,SMALLINTDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_FORMAT,         13,WVARCHARDTYPE,    yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_DEFAULT_VAL,    14,WVARCHARDTYPE,    yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_ATTR_TYPE,      15,INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_COLUMNS,  RS_ANAME_COLUMNS_REMARKS,        16,WLONGVARCHARDTYPE,yes, NULL, NULL, RSAT_USER_DEFINED, NULL },

#ifdef JYRI_REMOVED
        { 0L, RS_RELID_USERS,    RS_ANAME_USERS_ID,               0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_USERS,    RS_ANAME_USERS_NAME,             1, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_USERS,    RS_ANAME_USERS_TYPE,             2, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_USERS,    RS_ANAME_USERS_PRIV,             3, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_USERS,    RS_ANAME_USERS_PASSW,            4, VARBINARYDTYPE,   yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_USERS,    RS_ANAME_USERS_PRIORITY,         5, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_USERS,    RS_ANAME_USERS_PRIVATE,          6, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
#endif /* JYRI_REMOVED */

        { 0L, RS_RELID_UROLE,    RS_ANAME_UROLE_U_ID,             0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_UROLE,    RS_ANAME_UROLE_R_ID,             1, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },

        { 0L, RS_RELID_RELAUTH,  RS_ANAME_RELAUTH_REL_ID,         0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_RELAUTH,  RS_ANAME_RELAUTH_UR_ID,          1, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_RELAUTH,  RS_ANAME_RELAUTH_PRIV,           2, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_RELAUTH,  RS_ANAME_RELAUTH_GRANT_ID,       3, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_RELAUTH,  RS_ANAME_RELAUTH_GRANT_TIM,      4, DATEDTYPE,        yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_RELAUTH,  RS_ANAME_RELAUTH_GRANT_OPT,      5, WCHARDTYPE(3),    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_RELAUTH,  RS_ANAME_TUPLE_ID,               6, VARBINARYDTYPE,   no,  NULL, NULL, RSAT_TUPLE_ID,     NULL },

        { 0L, RS_RELID_ATTAUTH,  RS_ANAME_ATTAUTH_REL_ID,         0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_ATTAUTH,  RS_ANAME_ATTAUTH_UR_ID,          1, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_ATTAUTH,  RS_ANAME_ATTAUTH_ATTR_ID,        2, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_ATTAUTH,  RS_ANAME_ATTAUTH_PRIV,           3, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_ATTAUTH,  RS_ANAME_ATTAUTH_GRANT_ID,       4, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_ATTAUTH,  RS_ANAME_ATTAUTH_GRANT_TIM,      5, DATEDTYPE,        yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_ATTAUTH,  RS_ANAME_TUPLE_ID,               6, VARBINARYDTYPE,   no,  NULL, NULL, RSAT_TUPLE_ID,     NULL },

        { 0L, RS_RELID_VIEWS,    RS_ANAME_VIEWS_V_ID,             0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_VIEWS,    RS_ANAME_VIEWS_TEXT,             1, WLONGVARCHARDTYPE,no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_VIEWS,    RS_ANAME_VIEWS_CHECK,            2, WLONGVARCHARDTYPE,yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_VIEWS,    RS_ANAME_VIEWS_REMARKS,          3, WLONGVARCHARDTYPE,yes, NULL, NULL, RSAT_USER_DEFINED, NULL },

        { 0L, RS_RELID_KEYPARTS, RS_ANAME_KEYPARTS_ID,            0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYPARTS, RS_ANAME_KEYPARTS_REL_ID,        1, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYPARTS, RS_ANAME_KEYPARTS_KEYP_NO,       2, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYPARTS, RS_ANAME_KEYPARTS_ATTR_ID,       3, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYPARTS, RS_ANAME_KEYPARTS_ATTR_NO,       4, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYPARTS, RS_ANAME_KEYPARTS_ATTR_TYPE,     5, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYPARTS, RS_ANAME_KEYPARTS_CONST_VALUE,   6, VARBINARYDTYPE,   yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYPARTS, RS_ANAME_KEYPARTS_ASCENDING,     7, WCHARDTYPE(3),    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },

        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_ID,                0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_REL_ID,            1, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_KEY_NAME,          2, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_KEY_UNIQUE,        3, WCHARDTYPE(3),    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_KEY_NONUNQ_ODBC,   4, SMALLINTDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_CLUSTERING,        5, WCHARDTYPE(3),    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_PRIMARY,           6, WCHARDTYPE(3),    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_PREJOINED,         7, WCHARDTYPE(3),    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_SCHEMA,            8, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_NREF,              9, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_TUPLE_VERSION,          10,WVARCHARDTYPE,    yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_KEYS,     RS_ANAME_KEYS_CATALOG,           11,WVARCHARDTYPE,    yes, NULL, NULL, RSAT_USER_DEFINED, NULL },

        { 0L, RS_RELID_CARDINAL, RS_ANAME_CARDINAL_REL_ID,        0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_CARDINAL, RS_ANAME_CARDINAL_CARDIN,        1, BIGINTDTYPE,      no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_CARDINAL, RS_ANAME_CARDINAL_SIZE,          2, BIGINTDTYPE,      no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_CARDINAL, RS_ANAME_CARDINAL_LAST_UPD,      3, DATEDTYPE,        yes, NULL, NULL, RSAT_USER_DEFINED, NULL },

        { 0L, RS_RELID_INFO,     RS_ANAME_INFO_PROPERTY,          0, WVARCHARDTYPE,    no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_INFO,     RS_ANAME_INFO_VALUE_STR,         1, WVARCHARDTYPE,    yes, NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_INFO,     RS_ANAME_INFO_VALUE_INT,         2, INTDTYPE,         yes, NULL, NULL, RSAT_USER_DEFINED, NULL },

        { 0L, RS_RELID_SYNONYM,  RS_ANAME_SYNONYM_TARGET_ID,      0, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL },
        { 0L, RS_RELID_SYNONYM,  RS_ANAME_SYNONYM_SYNON,          1, INTDTYPE,         no,  NULL, NULL, RSAT_USER_DEFINED, NULL }

};

#define KEY(keyno,tableid)  KEYID(keyno,tableid), (long)(tableid)

static dbe_srli_key_t sys_keys[] = {
        /*   id  rel_id               key_name                  unq  clus prim prej schema   nref */
        /*   --  -------------------- ------------------------- ---- ---- ---- ---- -------- ---- */
        { KEY(0, RS_RELID_TABLES),   "SYS_KEY_TABLES_ID",      yes, yes, yes, no,  sysname, 2  },
        { KEY(2, RS_RELID_TABLES),"SYS_KEY_TABLES_NAMESCHEMACATALOG",yes,no,no,no, sysname, 4  },

        { KEY(0, RS_RELID_COLUMNS),  "SYS_KEY_COLUMNS_ID",     yes, yes, yes, no,  sysname, 3  },

        { KEY(0, RS_RELID_USERS),    "SYS_KEY_USERS_ID",       yes, yes, yes, no,  sysname, 2  },
        { KEY(1, RS_RELID_USERS),    "SYS_KEY_USERS_NAME",     yes, no,  no,  no,  sysname, 2  },

        { KEY(0, RS_RELID_UROLE),    "SYS_KEY_UROLE_U_ID",     yes, yes, yes, no,  sysname, 3  },
        { KEY(1, RS_RELID_UROLE),    "SYS_KEY_UROLE_R_ID",     yes, no,  no,  no,  sysname, 3  },

        { KEY(0, RS_RELID_RELAUTH),  "SYS_KEY_RELAUTH",        yes, yes, yes, no,  sysname, 7  }, /* nref:3 */

        { KEY(0, RS_RELID_ATTAUTH),  "SYS_KEY_ATTAUTH",        yes, yes, yes, no,  sysname, 6  }, /* nfer:4 */

        { KEY(0, RS_RELID_VIEWS),    "SYS_KEY_VIEWS_V_ID",     yes, yes, yes, no,  sysname, 2  },

        { KEY(0, RS_RELID_KEYPARTS), "SYS_KEY_KEYPARTS_ID",    yes, yes, yes, no,  sysname, 3  },
        { KEY(1, RS_RELID_KEYPARTS), "SYS_KEY_KEYPARTS_REL",   yes, no,  no,  no,  sysname, 4  },

        { KEY(0, RS_RELID_KEYS),     "SYS_KEY_KEYS_REL",       yes, yes, yes, no,  sysname, 3  },
        { KEY(1, RS_RELID_KEYS),     "SYS_KEY_KEYS_NAME",      yes, no,  no,  no,  sysname, 3  },
        { KEY(3,RS_RELID_KEYS),"SYS_KEY_KEYS_NAMESCHEMACATALOG",yes,no,  no,  no,  sysname, 4  },

        { KEY(0, RS_RELID_CARDINAL), "SYS_KEY_CARDINAL_REL",   yes, yes, yes, no,  sysname, 2  },

        { KEY(0, RS_RELID_INFO),     "SYS_KEY_INFO_PROPERTY",  yes, yes, yes, no,  sysname, 2  },

        { KEY(0, RS_RELID_SYNONYM),  "SYS_KEY_SYNONYM_SYNON",  yes, yes, yes, no,  sysname, 2  },
        { KEY(1, RS_RELID_SYNONYM),  "SYS_KEY_SYNONYM_TARGET", yes, no,  no,  no,  sysname, 3  }
};


/* The keyparts init. data array
** NOTE: the keyparts start from 1 instead of 0 because
** the 0:th keypart is auto-generated system keypart (the key ID)
*/

#define KEYPART(keyno,tableid,keypart) KEY(keyno,tableid), (keypart)

static dbe_srli_keypart_t sys_keyparts[] = {
        /*       id  rel_id        keyp_no attr_id    attr_no    attr_type          const asc */
        /*       --  ----------------- -   -------    ---------- ------------------ ----- --- */
        { KEYPART(0, RS_RELID_TABLES, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_TABLES, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_TABLES, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_TABLES, 4), 3,         3,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_TABLES, 5), 4,         4,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_TABLES, 6), 5,         5,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_TABLES, 7), 6,         6,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_TABLES, 8), 7,         7,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(2, RS_RELID_TABLES, 1), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(2, RS_RELID_TABLES, 2), 3,         3,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(2, RS_RELID_TABLES, 3), 4,         4,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(2, RS_RELID_TABLES, 4), 0,         0,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(0, RS_RELID_COLUMNS, 1), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 2), 3,         3,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 3), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 4), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 5), 4,         4,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 6), 5,         5,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 7), 6,         6,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 8), 7,         7,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 9), 8,         8,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 10),9,         9,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 11),10,        10,        RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 12),11,        11,        RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 13),12,        12,        RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 14),13,        13,        RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 15),14,        14,        RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 16),15,        15,        RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_COLUMNS, 17),16,        16,        RSAT_USER_DEFINED, NULL, yes },

#ifdef JYRI_REMOVED
        { KEYPART(0, RS_RELID_USERS, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_USERS, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_USERS, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_USERS, 4), 3,         3,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_USERS, 5), 4,         4,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_USERS, 6), 5,         5,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_USERS, 7), 6,         6,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(1, RS_RELID_USERS, 1), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(1, RS_RELID_USERS, 2), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
#endif /* JYRI_REMOVED */

        { KEYPART(0, RS_RELID_UROLE, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_UROLE, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(1, RS_RELID_UROLE, 1), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(1, RS_RELID_UROLE, 2), 0,         0,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(0, RS_RELID_RELAUTH, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_RELAUTH, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_RELAUTH, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_RELAUTH, 4), 3,         3,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_RELAUTH, 5), 4,         4,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_RELAUTH, 6), 5,         5,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_RELAUTH, 7), 6,         6,         RSAT_TUPLE_ID,     NULL, yes },

        { KEYPART(0, RS_RELID_ATTAUTH, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_ATTAUTH, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_ATTAUTH, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_ATTAUTH, 4), 3,         3,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_ATTAUTH, 5), 4,         4,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_ATTAUTH, 6), 5,         5,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_ATTAUTH, 7), 6,         6,         RSAT_TUPLE_ID,     NULL, yes },

        { KEYPART(0, RS_RELID_VIEWS, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_VIEWS, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_VIEWS, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_VIEWS, 4), 3,         3,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(0, RS_RELID_KEYPARTS, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYPARTS, 2), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYPARTS, 3), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYPARTS, 4), 3,         3,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYPARTS, 5), 4,         4,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYPARTS, 6), 5,         5,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYPARTS, 7), 6,         6,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYPARTS, 8), 7,         7,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(1, RS_RELID_KEYPARTS, 1), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(1, RS_RELID_KEYPARTS, 2), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(1, RS_RELID_KEYPARTS, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(0, RS_RELID_KEYS, 1), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 2), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 4), 3,         3,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 5), 4,         4,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 6), 5,         5,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 7), 6,         6,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 8), 7,         7,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 9), 8,         8,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 10),9,         9,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 11),10,        10,        RSAT_TUPLE_VERSION,NULL, yes },
        { KEYPART(0, RS_RELID_KEYS, 12),11,        11,        RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(1, RS_RELID_KEYS, 1), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(1, RS_RELID_KEYS, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(1, RS_RELID_KEYS, 3), 0,         0,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(3, RS_RELID_KEYS, 1), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(3, RS_RELID_KEYS, 2), 8,         8,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(3, RS_RELID_KEYS, 3), 11,        11,        RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(3, RS_RELID_KEYS, 4), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(3, RS_RELID_KEYS, 5), 0,         0,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(0, RS_RELID_CARDINAL, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_CARDINAL, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_CARDINAL, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_CARDINAL, 4), 3,         3,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(0, RS_RELID_INFO, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_INFO, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_INFO, 3), 2,         2,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(0, RS_RELID_SYNONYM, 1), 1,         1,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(0, RS_RELID_SYNONYM, 2), 0,         0,         RSAT_USER_DEFINED, NULL, yes },

        { KEYPART(1, RS_RELID_SYNONYM, 1), 0,         0,         RSAT_USER_DEFINED, NULL, yes },
        { KEYPART(1, RS_RELID_SYNONYM, 2), 1,         1,         RSAT_USER_DEFINED, NULL, yes }

};

/*#***********************************************************************\
 *
 *		isyes
 *
 * Converts "YES"/"NO " -valued strings to boolean
 *
 * Parameters :
 *
 *	s - in, use
 *		pointer to string
 *
 * Return value :
 *          TRUE if string starts with 'Y' or
 *          FALSE otherwise.
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool isyes(char *s)
{
        ss_dassert(s != NULL);
        return (*s == 'Y' || *s == 'y');
}

/*#***********************************************************************\
 *
 *		dbe_srli_colcmp
 *
 * Compares two columns
 *
 * Parameters :
 *
 *	p1 - in, use
 *		pointer to 1st column record
 *
 *	p2 - in, use
 *		pointer to 2nd column record
 *
 *
 * Return value :
 *          < 0 if 1st is logically smaller than 2nd or
 *          > 0 if 1st is logically bigger than 2nd or
 *          = 0 if 1st is logically equal to 2nd
 *
 * Limitations  :
 *
 * Globals used :
 */
static int SS_CLIBCALLBACK dbe_srli_colcmp(
        const void *p1,
        const void *p2)
{
        int cmp;

        cmp = (int)(((const dbe_srli_column_t*)p1)->sc_rel_id -
                    ((const dbe_srli_column_t*)p2)->sc_rel_id);
        if (cmp == 0) {
            cmp = (int)(((const dbe_srli_column_t*)p1)->sc_column_number -
                        ((const dbe_srli_column_t*)p2)->sc_column_number);
        }
        return (cmp);
}

/*#***********************************************************************\
 *
 *		dbe_srli_tblcmp
 *
 * Compares two tables
 *
 * Parameters :
 *
 *      p1 - in, use
 *		pointer to 1st table record
 *
 *	p2 - in, use
 *		pointer to 2nd table record
 *
 * Return value :
 *          < 0 if 1st is logically smaller than 2nd or
 *          > 0 if 1st is logically bigger than 2nd or
 *          = 0 if 1st is logically equal to 2nd
 *
 * Limitations  :
 *
 * Globals used :
 */
static int SS_CLIBCALLBACK dbe_srli_tblcmp(
        const void *p1,
        const void *p2)
{
        return ((int)(((const dbe_srli_table_t*)p1)->st_id -
                      ((const dbe_srli_table_t*)p2)->st_id));


}

/*#***********************************************************************\
 *
 *		dbe_srli_keycmp
 *
 * Compares two keys
 *
 * Parameters :
 *
 *      p1 - in, use
 *		pointer to 1st key record
 *
 *	p2 - in, use
 *		pointer to 2nd key record
 *
 * Return value :
 *          < 0 if 1st is logically smaller than 2nd or
 *          > 0 if 1st is logically bigger than 2nd or
 *          = 0 if 1st is logically equal to 2nd
 *
 * Limitations  :
 *
 * Globals used :
 */
static int SS_CLIBCALLBACK dbe_srli_keycmp(
        const void *p1,
        const void *p2)
{
        return ((int)(((const dbe_srli_key_t*)p1)->k_id -
                      ((const dbe_srli_key_t*)p2)->k_id));
}

/*#***********************************************************************\
 *
 *		dbe_srli_keypartcmp
 *
 * Compares two keyparts
 *
 * Parameters :
 *
 *      p1 - in, use
 *		pointer to 1st keypart record
 *
 *	p2 - in, use
 *		pointer to 2nd keypart record
 *
 * Return value :
 *          < 0 if 1st is logically smaller than 2nd or
 *          > 0 if 1st is logically bigger than 2nd or
 *          = 0 if 1st is logically equal to 2nd
 *
 * Limitations  :
 *
 * Globals used :
 */
static int SS_CLIBCALLBACK dbe_srli_keypartcmp(
        const void *p1,
        const void *p2)
{
        int cmp;

        cmp = (int) (((const dbe_srli_keypart_t*)p1)->kp_id -
                     ((const dbe_srli_keypart_t*)p2)->kp_id);
        if (cmp == 0) {
            cmp = (int) (((const dbe_srli_keypart_t*)p1)->kp_keyp_no -
                         ((const dbe_srli_keypart_t*)p2)->kp_keyp_no);
        }
        return (cmp);
}

/*#***********************************************************************\
 *
 *		dbe_srli_viewcmp
 *
 * compares two view records
 *
 * Parameters :
 *
 *      p1 - in, use
 *		pointer to 1st view record
 *
 *	p2 - in, use
 *		pointer to 2nd view record
 *
 * Return value :
 *          < 0 if 1st is logically smaller than 2nd or
 *          > 0 if 1st is logically bigger than 2nd or
 *          = 0 if 1st is logically equal to 2nd
 *
 * Limitations  :
 *
 * Globals used :
 */
static int SS_CLIBCALLBACK dbe_srli_viewcmp(
        const void *p1,
        const void *p2)
{
        return (int) (((const dbe_srli_view_t*)p1)->v_v_id -
                      ((const dbe_srli_view_t*)p2)->v_v_id);
}
/*#***********************************************************************\
 *
 *		dbe_srli_findcol
 *
 * Finds column from array according to relation id and column number
 *
 * Parameters :
 *
 *	columns_arr - in, use
 *		pointer to array of columns
 *
 *	ncolumns - in
 *		array size
 *
 *	rel_id - in
 *		relation id
 *
 *	column_number - in
 *		column number
 *
 * Return value - ref :
 *          pointer to found column or NULL if not found
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_srli_column_t *dbe_srli_findcol(
        dbe_srli_column_t  *columns_arr,
        uint ncolumns,
        long rel_id,
        long column_number)
{
        dbe_srli_column_t *result;
        dbe_srli_column_t col;

        col.sc_column_number = column_number;
        col.sc_rel_id = rel_id;
        result =
            bsearch(&col,
                    columns_arr,
                    ncolumns,
                    sizeof(col),
                    dbe_srli_colcmp);
        return (result);
}


/*#***********************************************************************\
 *
 *		dbe_srli_patch
 *
 * Patches the system relation init. arrays into consistent state
 *
 * Parameters :
 *
 *	tables_arr - in out, use
 *		sys_tables[]
 *
 *	ntables - in
 *		array size of previous
 *
 *	columns_arr - in out, use
 *		sys_columns[]
 *
 *	ncolumns - in
 *		array size of previous
 *
 *	keys_arr - in out, use
 *		sys_keys[]
 *
 *	nkeys - in
 *		array size of previous
 *
 *	keyparts_arr - in out, use
 *          sys_keyparts[]
 *
 *	nkeyparts - in
 *		array size of previous
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dbe_srli_patch(
        dbe_srli_table_t   *tables_arr,   uint ntables,
        dbe_srli_column_t  *columns_arr,  uint ncolumns,
        dbe_srli_key_t     *keys_arr,     uint nkeys,
        dbe_srli_keypart_t *keyparts_arr, uint nkeyparts,
        dbe_srli_view_t    *views_arr,    uint nviews)
{
        SsTimeT creatime;
        uint i;

        if (tables_arr[0].st_creatime != 0L) {
            return; /* tables are patched already! */
        }
        creatime = SsTime(NULL);
        for (i = 0; i < ntables; i++) {
            tables_arr[i].st_creatime = creatime;
        }
        qsort(tables_arr, ntables, sizeof(tables_arr[0]), dbe_srli_tblcmp);
        qsort(columns_arr, ncolumns, sizeof(columns_arr[0]), dbe_srli_colcmp);
        qsort(keys_arr, nkeys, sizeof(keys_arr[0]), dbe_srli_keycmp);
        qsort(keyparts_arr, nkeyparts, sizeof(keyparts_arr[0]),
              dbe_srli_keypartcmp);
        if (views_arr != NULL) {
            qsort(views_arr, nviews, sizeof(views_arr[0]),
                dbe_srli_viewcmp);
        }
        for (i = 0; i < ncolumns; i++) {
            columns_arr[i].sc_id = columns_arr[i].sc_rel_id +
                                   columns_arr[i].sc_column_number + 1;
        }
        for (i = 0; i < nkeyparts; i++) {
            dbe_srli_column_t *p_column;

            p_column =
                dbe_srli_findcol(
                    columns_arr,
                    ncolumns,
                    keyparts_arr[i].kp_rel_id,
                    keyparts_arr[i].kp_attr_id);
            ss_dassert(p_column != NULL);
            keyparts_arr[i].kp_attr_id = p_column->sc_id;
        }
}

static void srli_addtupleversion(
        rs_sysi_t* cd,
        rs_ttype_t* p_ttype,
        int col_no,
        long attr_id)
{
        rs_atype_t *p_atype;

        ss_dprintf_3(("srli_addtupleversion:col_no=%d, attr_id=%ld\n", col_no, attr_id));

        /* Add tuple version system attribute */
        p_atype = rs_atype_init(
                        cd,
                        RSAT_TUPLE_VERSION,
#ifdef SS_UNICODE_DATA
                        RSDT_BINARY,
#else /* SS_UNICODE_DATA */
                        RSDT_CHAR,
#endif /* SS_UNICODE_DATA */
                        RSSQLDT_VARBINARY,
                        10, /* 1 byte length + 8 byte integer + '\0' */
                        RS_SCALE_NULL,
                        TRUE);
        ss_dassert(p_atype != NULL);
        rs_ttype_setatype(cd, p_ttype, col_no, p_atype);
        rs_ttype_setaname(cd, p_ttype, col_no, (char *)RS_ANAME_TUPLE_VERSION);

        rs_ttype_setattrid(
            cd,
            p_ttype,
            col_no,
            attr_id);

        rs_atype_free(cd, p_atype);
}

/*#***********************************************************************\
 *
 *		dbe_srli_makerelh
 *
 * Makes a relation handle without keys basing on a system table init.
 * record plus the system columns init. records
 *
 * Parameters :
 *
 *	p_table - in, use
 *		pointer to system table init. data
 *
 *	columns_arr - in, use
 *		pointer to system rows array
 *
 *	ncolumns - in
 *		number of elems in the previous
 *
 *	column_idx - in out
 *		pointer to variable containig current position in
 *          the (presorted) column array. Must be initialized
 *          to the first column index of the relation. If the
 *          relations are handled in order starting from the first,
 *          the variable only has to be initialized to 0 before first
 *          call of this function.
 *
 *      unicode_enabled - in
 *          tells whether the data dictionary columns
 *          may contain unicode
 *
 * Output params:
 *
 * Return value : pointer to newly created relh
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_relh_t *dbe_srli_makerelh(
        rs_sysi_t* cd,
        dbe_srli_table_t *p_table,
        dbe_srli_column_t *columns_arr,
        uint ncolumns,
        uint *column_idx,
        bool unicode_enabled)
{
        rs_relh_t *p_relh;
        rs_ttype_t *p_ttype;
        rs_atype_t *p_atype;
        uint col_no;
        uint i;
        long len;
        long scale;
        rs_datatype_t datatype;
        rs_sqldatatype_t sqldatatype;
        rs_entname_t en;
        bool tuple_version_added = FALSE;

        p_ttype = rs_ttype_create(cd);

        col_no = 0;
        i = *column_idx; /* Start at column *column_idx */
        while (i < ncolumns && columns_arr[i].sc_rel_id == p_table->st_id) {
            ss_dprintf_4(("dbe_srli_makerelh:relid=%d, colname=%s, col_no=%d\n",
                p_table->st_id, columns_arr[i].sc_column_name, col_no));
            if (strcmp(columns_arr[i].sc_column_name, RS_ANAME_TUPLE_VERSION) == 0) {
                srli_addtupleversion(
                    cd,
                    p_ttype,
                    col_no,
                    columns_arr[i].sc_id);
                tuple_version_added = TRUE;
            } else {
                scale = columns_arr[i].sc_numeric_scale;
                sqldatatype =
                    (rs_sqldatatype_t)columns_arr[i].sc_sql_data_type_number;
#ifdef SS_UNICODE_SQL
                if (!unicode_enabled) {
                    switch (sqldatatype) {
                        case RSSQLDT_WCHAR:
                            sqldatatype = RSSQLDT_CHAR;
                            break;
                        case RSSQLDT_WVARCHAR:
                            sqldatatype = RSSQLDT_VARCHAR;
                            break;
                        case RSSQLDT_WLONGVARCHAR:
                            sqldatatype = RSSQLDT_LONGVARCHAR;
                            break;
                        default:
                            break;
                    }
                }
#endif /* SS_UNICODE_SQL */
                datatype = rs_atype_sqldttodt(cd, sqldatatype);
                switch (datatype) {
#ifdef SS_UNICODE_DATA
                    case RSDT_UNICODE:
                    case RSDT_BINARY:
#endif /* SS_UNICODE_DATA */
                    case RSDT_CHAR:
                        len = columns_arr[i].sc_char_max_length;
                        break;
                    default:
                        len = columns_arr[i].sc_numeric_precision;
                        break;
                }
                p_atype =
                    rs_atype_init(
                        cd,
                        (rs_attrtype_t)columns_arr[i].sc_attr_type,
                        datatype,
                        sqldatatype,
                        len,
                        scale,
                        isyes(columns_arr[i].sc_nullable));
                ss_dassert(p_atype != NULL);
                rs_ttype_setatype(cd, p_ttype, col_no, p_atype);
                rs_atype_free(cd, p_atype);
                p_atype = NULL;
                rs_ttype_setaname(
                    cd,
                    p_ttype,
                    col_no,
                    (char *)columns_arr[i].sc_column_name);
                rs_ttype_setattrid(
                    cd,
                    p_ttype,
                    col_no,
                    columns_arr[i].sc_id);
                ss_dassert(columns_arr[i].sc_default_val == NULL);
            }
            col_no++;
            i++;
        }
        *column_idx = i;    /* update *column_idx */

        if (!tuple_version_added) {
            srli_addtupleversion(
                cd,
                p_ttype,
                col_no,
                columns_arr[i-1].sc_id+1);
        }

        rs_entname_initbuf(
                &en,
                (p_table->st_table_catalog == NULL ?
                 RS_AVAL_DEFCATALOG : p_table->st_table_catalog),
                p_table->st_table_schema,
                (char *)p_table->st_table_name);

        rs_ttype_addpseudoatypes(cd, p_ttype);

        p_relh =
            rs_relh_init(
                cd,
                &en,
                p_table->st_id,
                p_ttype);
        ss_dassert(p_relh != NULL);
        SS_MEM_SETLINK(p_relh);
        rs_ttype_free(cd, p_ttype);
        p_ttype = NULL;
        return (p_relh);
}

/*#***********************************************************************\
 *
 *		dbe_srli_addkeys
 *
 * Adds keys to system relation handle
 *
 * Parameters :
 *
 *	p_relh - in out, use
 *		pointer to relation handle
 *
 *	p_table - in, use
 *		pointer to system table under creation
 *
 *	keys_arr - in, use
 *		pointer to keys init. array
 *
 *	nkeys - in
 *		number of elems in the previous
 *
 *	keyparts_arr - in, use
 *		pointer to keyparts init. array
 *
 *	nkeyparts - in
 *		number of elems in the previous
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
static void dbe_srli_addkeys(rs_sysi_t* cd,
                             rs_relh_t *p_relh,
                             dbe_srli_table_t *p_table,
                             dbe_srli_key_t *keys_arr,
                             uint nkeys,
                             dbe_srli_keypart_t *keyparts_arr,
                             uint nkeyparts)

{
        rs_ano_t keyno;
        rs_ano_t kpno;
        rs_ano_t ano;

        dbe_srli_key_t *p_key;
        dbe_srli_key_t tmp_key;
        dbe_srli_keypart_t *p_keypart;
        dbe_srli_keypart_t tmp_keypart;
        rs_key_t *p_rskey;
        va_t constvalue;

        rs_ano_t nattrs;
        bool tuple_version_added;

        nattrs = rs_relh_nattrs(cd, p_relh);
        p_rskey = NULL;
        for (keyno = 0; ; keyno++) {
            /* Try to find i:th key record for the sys. table in question
            ** from keys_arr
            */
            tuple_version_added = FALSE;
            tmp_key.k_id = KEYID(keyno, p_table->st_id);
            p_key = bsearch(&tmp_key,
                               keys_arr,
                               nkeys,
                               sizeof(tmp_key),
                               dbe_srli_keycmp);
            if (p_key == NULL) {
                if (keyno < 5) {
                    continue;
                } else {
                    break;
                }
            }
            p_rskey =           /* create a key record */
                rs_key_init(
                    cd,
                    (char *)p_key->k_key_name,
                    p_key->k_id,
                    isyes(p_key->k_key_unique),
                    isyes(p_key->k_clustering),
                    isyes(p_key->k_primary),
                    isyes(p_key->k_prejoined),
                    (uint)p_key->k_nref,
                    NULL);
            ss_dassert(p_rskey != NULL);
            /* find first keypart of this key */
            tmp_keypart.kp_id = tmp_key.k_id;
            tmp_keypart.kp_keyp_no = 1;
            p_keypart = bsearch(&tmp_keypart,
                               keyparts_arr,
                               nkeyparts,
                               sizeof(tmp_keypart),
                               dbe_srli_keypartcmp);
            ss_dassert(p_keypart != NULL);

            /* first create the system-generated keypart #0
            ** which is the unique key ID constant to distinguish the records
            ** from other keys
            */
            va_setlong(&constvalue, p_keypart->kp_id);
            rs_key_addpart(
                cd,
                p_rskey,
                (rs_ano_t)0,
                (rs_attrtype_t)RSAT_KEY_ID,
                TRUE,
                (rs_ano_t)RS_ANO_NULL,
                &constvalue);

            /* Then create the keyparts defined in the init. table
            ** (the 'user-defined')
            */
            for (kpno = (rs_ano_t)(p_keypart - keyparts_arr); /* = index in keyparts_arr */
                 (uint)kpno < nkeyparts && p_keypart->kp_id == p_key->k_id;
                 p_keypart++, kpno++)
            {
                rs_key_addpart(
                    cd,
                    p_rskey,
                    (rs_ano_t)p_keypart->kp_keyp_no,
                    (rs_attrtype_t)p_keypart->kp_attr_type,
                    isyes(p_keypart->kp_ascending),
                    (rs_ano_t)p_keypart->kp_attr_no,
                    (va_t*)p_keypart->kp_const_value);
                if (p_keypart->kp_attr_type == RSAT_TUPLE_VERSION) {
                    tuple_version_added = TRUE;
                }
            }

            if (!tuple_version_added) {
                /* Then, we search for tuple version attribute */
                for (ano = 0; ano < nattrs; ano++) {
                    rs_ttype_t* ttype = rs_relh_ttype(cd, p_relh);
                    rs_atype_t* atype = rs_ttype_atype(cd, ttype, ano);
                    if (rs_atype_attrtype(NULL, atype) == RSAT_TUPLE_VERSION) {
                        break; /* found */
                    }
                }
                ss_dassert(ano < nattrs); /* must be found */


                /* Add keypart for TUPLE_VERSION system attribute */
                rs_key_addpart(
                    cd,
                    p_rskey,
                    rs_key_nparts(cd, p_rskey),
                    RSAT_TUPLE_VERSION,
                    TRUE,
                    ano,
                    NULL
                );
            }

            rs_key_setindex_ready(cd, p_rskey);
            rs_relh_insertkey(cd, p_relh, p_rskey);
            p_rskey = NULL;
        }
}


/*##**********************************************************************\
 *
 *		dbe_srli_init
 *
 * Initializes system relations into relation info buffer
 *
 * Parameters :
 *
 *	rbuf - in out, use
 *		pointer to relation buffer object
 *
 *      unicode_enabled - in
 *          tells whether the data dictionary columns
 *          may contain unicode
 *
 * Output params:
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_srli_init(rs_sysi_t* cd, rs_rbuf_t *rbuf, bool unicode_enabled)
{
        uint i;
        uint column_idx;
        rs_relh_t *relh;
        rs_rbuf_ret_t ret;

        ss_dprintf_1(("dbe_srli_init:unicode_enabled=%d\n", unicode_enabled));

        dbe_srli_patch(sys_tables, sizeof(sys_tables)/sizeof(sys_tables[0]),
                       sys_columns, sizeof(sys_columns)/sizeof(sys_columns[0]),
                       sys_keys, sizeof(sys_keys)/sizeof(sys_keys[0]),
                       sys_keyparts, sizeof(sys_keyparts)/sizeof(sys_keyparts[0]),
                       NULL, 0);

        column_idx = 0;
        for (i = 0; i < sizeof(sys_tables) / sizeof(sys_tables[0]); i++) {
            if (strcmp(sys_tables[i].st_table_type, base_table) == 0) {
                relh =
                    dbe_srli_makerelh(
                        cd,
                        sys_tables + i,
                        sys_columns,
                        sizeof(sys_columns)/sizeof(sys_columns[0]),
                        &column_idx,
                        unicode_enabled);
                dbe_srli_addkeys(
                    cd,
                    relh,
                    sys_tables + i,
                    sys_keys,
                    sizeof(sys_keys)/sizeof(sys_keys[0]),
                    sys_keyparts,
                    sizeof(sys_keyparts)/sizeof(sys_keyparts[0]));
                ret = rs_rbuf_insertrelh_ex_nomutex(cd, rbuf, relh);
                ss_rc_dassert(ret == RSRBUF_SUCCESS, ret);
            } else {
                ss_error;
            }
        }
}

#ifdef SS_DEBUG
void dbe_srli_testinit(void)
{
        uint i;
        uint column_idx;
        rs_relh_t *relh;

        dbe_srli_patch(sys_tables, sizeof(sys_tables)/sizeof(sys_tables[0]),
                       sys_columns, sizeof(sys_columns)/sizeof(sys_columns[0]),
                       sys_keys, sizeof(sys_keys)/sizeof(sys_keys[0]),
                       sys_keyparts, sizeof(sys_keyparts)/sizeof(sys_keyparts[0]),
                       NULL, 0);
        column_idx = 0;
        for (i = 0; i < sizeof(sys_tables) / sizeof(sys_tables[0]); i++) {
            if (strcmp(sys_tables[i].st_table_type, base_table) == 0) {
                relh =
                    dbe_srli_makerelh(
                        NULL,
                        sys_tables + i,
                        sys_columns,
                        sizeof(sys_columns)/sizeof(sys_columns[0]),
                        &column_idx,
                        TRUE);
                dbe_srli_addkeys(
                    NULL,
                    relh,
                    sys_tables + i,
                    sys_keys,
                    sizeof(sys_keys)/sizeof(sys_keys[0]),
                    sys_keyparts,
                    sizeof(sys_keyparts)/sizeof(sys_keyparts[0]));
                rs_relh_print(NULL, relh);
                SS_MEM_SETUNLINK(relh);
                rs_relh_done(NULL, relh);
            } else {
                ss_error;
            }
        }
}
#endif /* SS_DEBUG */
