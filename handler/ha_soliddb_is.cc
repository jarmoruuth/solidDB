/*************************************************************************\
**  source       * ha_soliddb_is.cc
**  directory    * mysql
**  description  * Implementation of Information Schema extensions
**               * of solidDB Storage Engine for MySQL 5.1.
**               * 
**               * Copyright (C) 2007 Solid Information Technology Ltd
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

#define INSIDE_HA_SOLIDDB_CC

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <m_ctype.h>
#include <hash.h>
/* #include <myisampack.h> */
#include <mysys_err.h>
#include <my_base.h>
#include <mysys_err.h>
#include <my_sys.h>
#include <ssmyenv.h>

#include <sql_priv.h>
#include <mysqld_error.h>
#include <table.h>
#include <field.h>

#include <mysql/plugin.h>
#include "ha_soliddb.h" 
#include "ha_soliddb_is.h"

#if MYSQL_VERSION_ID >= 50100
extern struct handlerton *legacy_soliddb_hton;
#endif

static ST_FIELD_INFO soliddb_sys_tables_fields[]=
{
#define IDX_TABLES_ID           0
        {STRUCT_FLD(field_name,         RS_ANAME_TABLES_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Relation id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLES_NAME		1
        {STRUCT_FLD(field_name,         RS_ANAME_TABLES_NAME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Table name"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLES_TYPE		2
        {STRUCT_FLD(field_name,         RS_ANAME_TABLES_TYPE),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Table schema"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLES_SCHEMA	3
        {STRUCT_FLD(field_name,         RS_ANAME_TABLES_SCHEMA),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Table schema"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLES_CATALOG	4
        {STRUCT_FLD(field_name,         RS_ANAME_TABLES_CATALOG),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Table catalog"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLES_CREATIME	5
        {STRUCT_FLD(field_name,         RS_ANAME_TABLES_CREATIME),
         STRUCT_FLD(field_length,       80),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Createtime"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLES_CHECK	6
        {STRUCT_FLD(field_name,         RS_ANAME_TABLES_CHECK),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Check string"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLES_REMARKS	7	
        {STRUCT_FLD(field_name,         RS_ANAME_TABLES_REMARKS),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Remarks"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};


static ST_FIELD_INFO soliddb_sys_columns_fields[]=
{
#define IDX_COLUMNS_ID		0	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_REL_ID		1	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_REL_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Relation id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_COLUMN_NAME		2	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_COLUMN_NAME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Column name"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_COLUMN_NUMBER	3	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_COLUMN_NUMBER),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Column number"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_DATA_TYPE		4
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_DATA_TYPE),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Data type"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_SQLDTYPE_NO		5
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_SQLDTYPE_NO),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "SQL data type number"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_DATA_TYPE_NO	6	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_DATA_TYPE_NO),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Data type number"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_CHAR_MAX_LEN	7	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_CHAR_MAX_LEN),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Char max length"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_NUM_PRECISION	8	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_NUM_PRECISION),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Numeric Precision"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_NUM_PREC_RDX	9	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_NUM_PREC_RDX),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Numeric precision radix"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_NUM_SCALE		10	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_NUM_SCALE),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Numeric scale"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_NULLABLE		11	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_NULLABLE),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Nullable"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_NULLABLE_ODBC	12	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_NULLABLE_ODBC),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Nullable ODBC"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_FORMAT		13	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_FORMAT),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Format"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_DEFAULT_VAL		14	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_DEFAULT_VAL),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Default value"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_ATTR_TYPE		15	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_ATTR_TYPE),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute type"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_REMARKS		16	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_REMARKS),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute type"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_columns_aux_fields[]=
{
#define IDX_COLUMNS_AUX_ID		0	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_AUX_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_AUX_ORIGINAL_DEFAULT		1	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_AUX_ORIGINAL_DEFAULT),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Original default value"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_AUX_AUTO_INC_SEQ_ID		2	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_AUX_AUTO_INC_SEQ_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Sequence id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_AUX_EXTERNAL_DATA_TYPE	3	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_AUX_EXTERNAL_DATA_TYPE),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "External data type"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLUMNS_AUX_EXTERNAL_COLLATION	4	
        {STRUCT_FLD(field_name,         RS_ANAME_COLUMNS_REMARKS),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "External collation"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_keys_fields[]=
{
#define IDX_KEYS_ID			0	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_REL_ID			1	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_REL_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Relation id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_KEY_NAME		2	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_KEY_NAME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key name"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_KEY_UNIQUE		3
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_KEY_UNIQUE),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key unique"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_KEY_NONUNIQ_ODBC	4	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_KEY_NONUNQ_ODBC),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key nonunique in ODBC"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_CLUSTERING		5
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_CLUSTERING),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Clustering"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_PRIMARY		6	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_PRIMARY),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Primary"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_PREJOINED		7
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_PREJOINED),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Prejoined"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_SCHEMA			8	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_SCHEMA),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key schema"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_CATALOG		9
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_CATALOG),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key catalog"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYS_NREF			10
        {STRUCT_FLD(field_name,         RS_ANAME_KEYS_NREF),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key nref"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_keyparts_fields[]=
{
#define IDX_KEYPARTS_ID			0
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Keypart id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_REL_ID		1	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_REL_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Relation id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_KEYP_NO		2	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_KEYP_NO),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Keypart number"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_ATTR_ID		3
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_ATTR_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_ATTR_NO		4
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_ATTR_NO),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute no"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_ATTR_TYPE		5
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_ATTR_TYPE),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute type"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_CONST_VALUE	6
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_CONST_VALUE),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Constant value"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_ASCENDING		7
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_ASCENDING),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Ascending"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_keyparts_aux_fields[]=
{
#define IDX_KEYPARTS_AUX_ID		0	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_AUX_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Keypart id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_AUX_KEYP_NO	1	
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_KEYP_NO),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Keypart number"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_KEYPARTS_AUX_PREFIX_LENGTH	2
        {STRUCT_FLD(field_name,         RS_ANAME_KEYPARTS_AUX_PREFIX_LENGTH),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Prefix length"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_forkeys_fields[]=
{
#define IDX_FORKEYS_ID			0
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Foreign key id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_REF_REL_ID		1
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_REF_REL_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Ref rel id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_CREATE_REL_ID	2
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_CREATE_REL_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Create rel id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_REF_KEY_ID		3
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_REF_KEY_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Ref key id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_REFTYPE		4
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_REFTYPE),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Ref type"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_SCHEMA		5
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_SCHEMA),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Schema"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_CATALOG		6
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_CATALOG),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Catalog"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_NREF		7
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_NREF),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Nref"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_KEY_NAME		8
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_KEY_NAME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key name"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYS_KEY_ACTION		9
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYS_KEY_ACTION),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Key action"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_forkeyparts_fields[]=
{
#define IDX_FORKEYPARTS_ID		0
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYPARTS_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Foreign keypart id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYPARTS_KEYP_NO		1
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYPARTS_KEYP_NO),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Keypart number"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYPARTS_ATTR_NO		2
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYPARTS_ATTR_NO),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute number"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYPARTS_ATTR_ID		3
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYPARTS_ATTR_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYPARTS_ATTR_TYPE	4
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYPARTS_ATTR_TYPE),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Attribute type"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYPARTS_CONST_VALUE	5
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYPARTS_CONST_VALUE),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Constant value"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_FORKEYPARTS_PREFIX_LENGTH	6
        {STRUCT_FLD(field_name,         RS_ANAME_FORKEYPARTS_PREFIX_LENGTH),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Prefix length"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_schemas_fields[]=
{
#define IDX_SCHEMAS_ID			0
        {STRUCT_FLD(field_name,         RS_ANAME_SCHEMAS_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Schema id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SCHEMAS_NAME		1
        {STRUCT_FLD(field_name,         RS_ANAME_SCHEMAS_NAME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Schema name"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SCHEMAS_OWNER		2
        {STRUCT_FLD(field_name,         RS_ANAME_SCHEMAS_OWNER),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Schema owner"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SCHEMAS_CREATIME		3
        {STRUCT_FLD(field_name,         RS_ANAME_SCHEMAS_CREATIME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Schema createtime"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SCHEMAS_CATALOG		4
        {STRUCT_FLD(field_name,         RS_ANAME_SCHEMAS_CATALOG),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Schema catalog"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_sequences_fields[]=
{
#define IDX_SEQUENCES_NAME		0
        {STRUCT_FLD(field_name,         RS_ANAME_SEQUENCES_NAME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Name"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SEQUENCES_ID		1
        {STRUCT_FLD(field_name,         RS_ANAME_SEQUENCES_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Sequence id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SEQUENCES_DENSE		2
        {STRUCT_FLD(field_name,         RS_ANAME_SEQUENCES_DENSE),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Dense"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SEQUENCES_SCHEMA		3
        {STRUCT_FLD(field_name,         RS_ANAME_SEQUENCES_SCHEMA),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Schema"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SEQUENCES_CATALOG		4
        {STRUCT_FLD(field_name,         RS_ANAME_SEQUENCES_CATALOG),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Catalog"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_SEQUENCES_CREATIME		5
        {STRUCT_FLD(field_name,         RS_ANAME_SEQUENCES_CREATIME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Createtime"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_cardinal_fields[]=
{
#define IDX_CARDINAL_REL_ID		0
        {STRUCT_FLD(field_name,         RS_ANAME_CARDINAL_REL_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Relation id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_CARDINAL_CARDIN		1
        {STRUCT_FLD(field_name,         RS_ANAME_CARDINAL_CARDIN),
         STRUCT_FLD(field_length,       8),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONGLONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Cardinality"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_CARDINAL_SIZE		2
        {STRUCT_FLD(field_name,         RS_ANAME_CARDINAL_SIZE),
         STRUCT_FLD(field_length,       8),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONGLONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Size"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_CARDINAL_LAST_UPD		3
        {STRUCT_FLD(field_name,         RS_ANAME_CARDINAL_LAST_UPD),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Last update"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_tablemodes_fields[]=
{
#define IDX_TABLEMODES_ID		0
        {STRUCT_FLD(field_name,         RS_ANAME_TABLEMODES_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLEMODES_MODE		1
        {STRUCT_FLD(field_name,         RS_ANAME_TABLEMODES_MODE),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Mode"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLEMODES_MODTIME		2
        {STRUCT_FLD(field_name,         RS_ANAME_TABLEMODES_MODTIME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Modify time"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_TABLEMODES_MODUSER		3
        {STRUCT_FLD(field_name,         RS_ANAME_TABLEMODES_MODUSER),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Modify user"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_collations_fields[]=
{
#define IDX_COLLATIONS_ID		0
        {STRUCT_FLD(field_name,         RS_ANAME_COLLATIONS_ID),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLLATIONS_CHARSET_NAME	1
        {STRUCT_FLD(field_name,         RS_ANAME_COLLATIONS_CHARSET_NAME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Charset name"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLLATIONS_COLLATION_NAME	2
        {STRUCT_FLD(field_name,         RS_ANAME_COLLATIONS_COLLATION_NAME),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Collation name"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_COLLATIONS_REMARKS		3
        {STRUCT_FLD(field_name,         RS_ANAME_COLLATIONS_REMARKS),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Remarks"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_info_fields[]=
{
#define IDX_INFO_PROPERTY		0
        {STRUCT_FLD(field_name,         RS_ANAME_INFO_PROPERTY),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Property"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_INFO_VALUE_STR		1
        {STRUCT_FLD(field_name,         RS_ANAME_INFO_VALUE_STR),
         STRUCT_FLD(field_length,       254),
         STRUCT_FLD(field_type,         MYSQL_TYPE_STRING),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Value string"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_INFO_VALUE_INT		2
        {STRUCT_FLD(field_name,         RS_ANAME_INFO_VALUE_INT),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Value int"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO soliddb_sys_blobs_fields[]=
{
#define IDX_BLOBS_ID			0
        {STRUCT_FLD(field_name,         RS_ANAME_BLOBS_ID),
         STRUCT_FLD(field_length,       8),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONGLONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Id"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_BLOBS_STARTPOS		1
        {STRUCT_FLD(field_name,         RS_ANAME_BLOBS_STARTPOS),
         STRUCT_FLD(field_length,       8),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONGLONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Start position"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_BLOBS_ENDSIZE		2
        {STRUCT_FLD(field_name,         RS_ANAME_BLOBS_ENDSIZE),
         STRUCT_FLD(field_length,       8),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONGLONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "End size"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_BLOBS_TOTALSIZE		3
        {STRUCT_FLD(field_name,         RS_ANAME_BLOBS_TOTALSIZE),
         STRUCT_FLD(field_length,       8),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONGLONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Total size"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_BLOBS_REFCOUNT		4
        {STRUCT_FLD(field_name,         RS_ANAME_BLOBS_REFCOUNT),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Refcount"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_BLOBS_COMPLETE		5
        {STRUCT_FLD(field_name,         RS_ANAME_BLOBS_COMPLETE),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Complete"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_BLOBS_STARTCPNUM		6
        {STRUCT_FLD(field_name,         RS_ANAME_BLOBS_STARTCPNUM),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Start CP num"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

#define IDX_BLOBS_NUMPAGES		7
        {STRUCT_FLD(field_name,         RS_ANAME_BLOBS_NUMPAGES),
         STRUCT_FLD(field_length,       4),
         STRUCT_FLD(field_type,         MYSQL_TYPE_LONG),
         STRUCT_FLD(value,              0),
         STRUCT_FLD(field_flags,        0),
         STRUCT_FLD(old_name,           "Number of pages"),
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)},

        END_OF_ST_FIELD_INFO
};

static int soliddb_sys_tables_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_columns_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_columns_aux_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_keys_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_keyparts_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_keyparts_aux_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_forkeys_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_forkeyparts_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_schemas_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_sequences_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_cardinal_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_tablemodes_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_collations_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_info_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);
static int soliddb_sys_blobs_fill(MYSQL_THD thd, TABLE_LIST* tables, COND* cond);

static struct st_mysql_information_schema       i_s_info =
{
        MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

struct st_mysql_plugin      i_s_soliddb_sys_tables =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_TABLES"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_tables),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_columns=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_COLUMNS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_columns),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_columns_aux=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_COLUMNS_AUX"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_columns_aux),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_keys=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_KEYS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_keys),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_keyparts=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_KEYPARTS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_keyparts),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_keyparts_aux=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_KEYPARTS_AUX"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_keyparts_aux),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_forkeys=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_FORKEYS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_forkeys),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_forkeyparts=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_FORKEYPARTS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_forkeyparts),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_schemas=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_SCHEMAS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_schemas),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_sequences=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_SEQUENCES"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_sequences),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_cardinal=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_CARDINAL"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_cardinal),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_tablemodes=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_TABLEMODES"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_tablemodes),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_collations=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_COLLATIONS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_collations),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_info=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_INFO"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_info),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

struct st_mysql_plugin      i_s_soliddb_sys_blobs=
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "SOLIDDB_SYS_BLOBS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "solidDB system tables."),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, soliddb_init_sys_blobs),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, soliddb_deinit_is),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL),

        /* Plugin flags */
        /* unsigned long */
        STRUCT_FLD(flags, 0UL),
};

/*#***********************************************************************\
 *
 *              soliddb_sys_tables_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_TABLES table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_tables_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_tables_list;
        su_list_node_t* sys_table_node;

        DBUG_ENTER("soliddb_sys_tables_fill");
        SS_PUSHNAME("soliddb_sys_tables_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_tables_list = su_list_init(NULL);

        tb_dd_fill_sys_tables_list(con->sc_cd, con->sc_trans, sys_tables_list);

        su_list_do(sys_tables_list, sys_table_node) {
            tb_dd_sys_tables_t* sys_table = (tb_dd_sys_tables_t*)su_listnode_getdata(sys_table_node);

            OK(table->field[0]->store((longlong)sys_table->relid, FALSE));
            OK(table->field[1]->store(sys_table->table_name, strlen(sys_table->table_name), system_charset_info));
            OK(table->field[2]->store(sys_table->table_type, strlen(sys_table->table_type), system_charset_info));
            OK(table->field[3]->store(sys_table->table_schema, strlen(sys_table->table_schema), system_charset_info));
            OK(table->field[4]->store(sys_table->table_catalog, strlen(sys_table->table_catalog), system_charset_info));
            OK(table->field[5]->store(sys_table->createtime, strlen(sys_table->createtime), system_charset_info));

            if (sys_table->check) {
                table->field[6]->store(sys_table->check, strlen(sys_table->check), system_charset_info);
            } else {
                table->field[6]->set_null();
            }
            
            if (sys_table->remarks) {
                table->field[7]->store(sys_table->remarks, strlen(sys_table->remarks), system_charset_info);
            } else {
                table->field[7]->set_null();
            }
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            SsMemFree((char *)sys_table->table_name);
            SsMemFree((char *)sys_table->table_type);
            SsMemFree((char *)sys_table->table_schema);
            SsMemFree((char *)sys_table->table_catalog);
            SsMemFree((char *)sys_table->createtime);

            if (sys_table->check) {
                SsMemFree((char *)sys_table->check);
            }

            if (sys_table->remarks) {
                SsMemFree((char *)sys_table->remarks);
            }
        }

        su_list_done(sys_tables_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_columns_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_COLUMNS table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_columns_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_columns_list;
        su_list_node_t* sys_columns_node;

        DBUG_ENTER("soliddb_sys_columns_fill");
        SS_PUSHNAME("soliddb_sys_columns_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_columns_list = su_list_init(NULL);

        tb_dd_fill_sys_columns_list(con->sc_cd, con->sc_trans, sys_columns_list);

        su_list_do(sys_columns_list, sys_columns_node) {
            tb_dd_sys_columns_t* sys_column = (tb_dd_sys_columns_t*)su_listnode_getdata(sys_columns_node);

            table->field[0]->store((longlong)sys_column->id, FALSE);
            table->field[1]->store((longlong)sys_column->rel_id, FALSE);
            table->field[2]->store(sys_column->column_name, strlen(sys_column->column_name), system_charset_info);
            table->field[3]->store((longlong)sys_column->column_number, FALSE);
            table->field[4]->store(sys_column->data_type, strlen(sys_column->data_type), system_charset_info);
            table->field[5]->store((longlong)sys_column->sql_data_type_num, FALSE);
            table->field[6]->store((longlong)sys_column->data_type_number, FALSE);
            table->field[7]->store((longlong)sys_column->char_max_length, FALSE);
            table->field[8]->store((longlong)sys_column->numeric_precision, FALSE);
            table->field[9]->store((longlong)sys_column->numeric_prec_radix, FALSE);
            table->field[10]->store((longlong)sys_column->numeric_scale, FALSE);
            table->field[11]->store(sys_column->nullable, strlen(sys_column->nullable), system_charset_info);
            table->field[12]->store((longlong)sys_column->nullable_odbc, FALSE);

            if (sys_column->format) {
                table->field[13]->store(sys_column->format, strlen(sys_column->format), system_charset_info);
            } else {
                table->field[13]->set_null();
            }

            if (sys_column->default_val) {
                table->field[14]->store(sys_column->default_val, strlen(sys_column->default_val), system_charset_info);
            } else {
                table->field[14]->set_null();
            }
            
            table->field[15]->store((longlong)sys_column->attr_type, FALSE);

            if (sys_column->remarks) {
                table->field[16]->store(sys_column->remarks, strlen(sys_column->remarks), system_charset_info);
            } else {
                table->field[16]->set_null();
            }
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            SsMemFree((char *)sys_column->column_name);
            SsMemFree((char *)sys_column->data_type);
            SsMemFree((char *)sys_column->nullable);

            if (sys_column->format) {
                SsMemFree((char *)sys_column->format);
            }

            if (sys_column->default_val) {
                SsMemFree((char *)sys_column->default_val);
            }
            
            if (sys_column->remarks) {
                SsMemFree((char *)sys_column->remarks);
            }
        }

        su_list_done(sys_columns_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_columns_aux_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_COLUMNS_AUX table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_columns_aux_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_columns_aux_list;
        su_list_node_t* sys_columns_aux_node;

        DBUG_ENTER("soliddb_sys_columns_aux_fill");
        SS_PUSHNAME("soliddb_sys_columns_aux_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_columns_aux_list = su_list_init(NULL);

        tb_dd_fill_sys_columns_aux_list(con->sc_cd, con->sc_trans, sys_columns_aux_list);

        su_list_do(sys_columns_aux_list, sys_columns_aux_node) {
            tb_dd_sys_columns_aux_t* sys_column_aux = (tb_dd_sys_columns_aux_t*)su_listnode_getdata(sys_columns_aux_node);

            table->field[0]->store((longlong)sys_column_aux->id, FALSE);

            if (sys_column_aux->original_default) {
                table->field[1]->store(sys_column_aux->original_default, strlen(sys_column_aux->original_default), system_charset_info);
            } else {
                table->field[1]->set_null();
            }

            table->field[2]->store((longlong)sys_column_aux->auto_inc_seq_id, FALSE);
            table->field[3]->store((longlong)sys_column_aux->external_data_type, FALSE);

            if (sys_column_aux->external_collation) {
                table->field[4]->store(sys_column_aux->external_collation, strlen(sys_column_aux->external_collation), system_charset_info);
            } else {
                table->field[4]->set_null();
            }
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            if (sys_column_aux->original_default) {
                SsMemFree((char *)sys_column_aux->original_default);
            }

            if (sys_column_aux->external_collation) {
                SsMemFree((char *)sys_column_aux->external_collation);
            }
        }

        su_list_done(sys_columns_aux_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_keys_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_KEYS table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_keys_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_keys_list;
        su_list_node_t* sys_keys_node;

        DBUG_ENTER("soliddb_sys_keys_fill");
        SS_PUSHNAME("soliddb_sys_keys_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_keys_list = su_list_init(NULL);

        tb_dd_fill_sys_keys_list(con->sc_cd, con->sc_trans, sys_keys_list);

        su_list_do(sys_keys_list, sys_keys_node) {
            tb_dd_sys_keys_t* sys_key = (tb_dd_sys_keys_t*)su_listnode_getdata(sys_keys_node);

            table->field[0]->store((longlong)sys_key->id, FALSE);
            table->field[1]->store((longlong)sys_key->rel_id, FALSE);
            table->field[2]->store(sys_key->key_name, strlen(sys_key->key_name), system_charset_info);
            table->field[3]->store(sys_key->key_unique, strlen(sys_key->key_unique), system_charset_info);
            table->field[4]->store((longlong)sys_key->key_nonunique_odbc, FALSE);
            table->field[5]->store(sys_key->key_clustering, strlen(sys_key->key_clustering), system_charset_info);
            table->field[6]->store(sys_key->key_prejoined, strlen(sys_key->key_prejoined), system_charset_info);
            table->field[7]->store(sys_key->key_schema, strlen(sys_key->key_schema), system_charset_info);
            table->field[8]->store((longlong)sys_key->key_nref, FALSE);
            table->field[9]->store(sys_key->key_catalog, strlen(sys_key->key_catalog), system_charset_info);
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            SsMemFree((char *)sys_key->key_name);
            SsMemFree((char *)sys_key->key_unique);
            SsMemFree((char *)sys_key->key_clustering);
            SsMemFree((char *)sys_key->key_prejoined);
            SsMemFree((char *)sys_key->key_schema);
            SsMemFree((char *)sys_key->key_catalog);
        }

        su_list_done(sys_keys_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_keyparts_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_KEYPARTS table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_keyparts_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_keyparts_list;
        su_list_node_t* sys_keyparts_node;

        DBUG_ENTER("soliddb_sys_keyparts_fill");
        SS_PUSHNAME("soliddb_sys_keyparts_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_keyparts_list = su_list_init(NULL);

        tb_dd_fill_sys_keyparts_list(con->sc_cd, con->sc_trans, sys_keyparts_list);

        su_list_do(sys_keyparts_list, sys_keyparts_node) {
            tb_dd_sys_keyparts_t* sys_keypart = (tb_dd_sys_keyparts_t*)su_listnode_getdata(sys_keyparts_node);

            table->field[0]->store((longlong)sys_keypart->id, FALSE);
            table->field[1]->store((longlong)sys_keypart->rel_id, FALSE);
            table->field[2]->store((longlong)sys_keypart->keyp_no, FALSE);
            table->field[3]->store((longlong)sys_keypart->attr_id, FALSE);
            table->field[4]->store((longlong)sys_keypart->attr_no, FALSE);
            table->field[5]->store((longlong)sys_keypart->attr_type, FALSE);

            if (sys_keypart->const_value) {
                table->field[6]->store(sys_keypart->const_value, strlen(sys_keypart->const_value), system_charset_info);
            } else {
                table->field[6]->set_null();
            }

            if (sys_keypart->ascending) {
                table->field[7]->store(sys_keypart->ascending, strlen(sys_keypart->ascending), system_charset_info);
            } else {
                table->field[7]->set_null();
            }
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            if (sys_keypart->const_value) {
                SsMemFree((char *)sys_keypart->const_value);
            }

            if (sys_keypart->ascending) {
                SsMemFree((char *)sys_keypart->ascending);
            }
        }

        su_list_done(sys_keyparts_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_keyparts_aux_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_KEYPARTS_AUX table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_keyparts_aux_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_keyparts_aux_list;
        su_list_node_t* sys_keyparts_aux_node;

        DBUG_ENTER("soliddb_sys_keyparts_aux_fill");
        SS_PUSHNAME("soliddb_sys_keyparts_aux_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_keyparts_aux_list = su_list_init(NULL);

        tb_dd_fill_sys_keyparts_aux_list(con->sc_cd, con->sc_trans, sys_keyparts_aux_list);

        su_list_do(sys_keyparts_aux_list, sys_keyparts_aux_node) {
            tb_dd_sys_keyparts_aux_t* sys_keypart_aux = (tb_dd_sys_keyparts_aux_t*)su_listnode_getdata(sys_keyparts_aux_node);

            table->field[0]->store((longlong)sys_keypart_aux->id, FALSE);
            table->field[1]->store((longlong)sys_keypart_aux->keyp_no, FALSE);
            table->field[2]->store((longlong)sys_keypart_aux->prefix_length, FALSE);

            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }
        }

        su_list_done(sys_keyparts_aux_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_forkeys_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_FORKEYS table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_forkeys_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_forkeys_list;
        su_list_node_t* sys_forkeys_node;

        DBUG_ENTER("soliddb_sys_forkeys_fill");
        SS_PUSHNAME("soliddb_sys_forkeys_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_forkeys_list = su_list_init(NULL);

        tb_dd_fill_sys_forkeys_list(con->sc_cd, con->sc_trans, sys_forkeys_list);

        su_list_do(sys_forkeys_list, sys_forkeys_node) {
            tb_dd_sys_forkeys_t* sys_forkey = (tb_dd_sys_forkeys_t*)su_listnode_getdata(sys_forkeys_node);

            table->field[0]->store((longlong)sys_forkey->id, FALSE);
            table->field[1]->store((longlong)sys_forkey->ref_rel_id, FALSE);
            table->field[2]->store((longlong)sys_forkey->create_rel_id, FALSE);
            table->field[3]->store((longlong)sys_forkey->ref_key_id, FALSE);
            table->field[4]->store((longlong)sys_forkey->ref_type, FALSE);
            table->field[5]->store(sys_forkey->schema, strlen(sys_forkey->schema), system_charset_info);
            table->field[6]->store(sys_forkey->catalog, strlen(sys_forkey->catalog), system_charset_info);
            table->field[7]->store((longlong)sys_forkey->nref, FALSE);
            table->field[8]->store(sys_forkey->key_name, strlen(sys_forkey->key_name), system_charset_info);
            table->field[9]->store((longlong)sys_forkey->key_action, FALSE);
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            SsMemFree((char*)sys_forkey->schema);
            SsMemFree((char*)sys_forkey->catalog);
            SsMemFree((char*)sys_forkey->key_name);
        }

        su_list_done(sys_forkeys_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_forkeyparts_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_FORKEYPARTS table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_forkeyparts_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_forkeyparts_list;
        su_list_node_t* sys_forkeyparts_node;

        DBUG_ENTER("soliddb_sys_forkeyparts_fill");
        SS_PUSHNAME("soliddb_sys_forkeyparts_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_forkeyparts_list = su_list_init(NULL);

        tb_dd_fill_sys_forkeyparts_list(con->sc_cd, con->sc_trans, sys_forkeyparts_list);

        su_list_do(sys_forkeyparts_list, sys_forkeyparts_node) {
            tb_dd_sys_forkeyparts_t* sys_forkeypart = (tb_dd_sys_forkeyparts_t*)su_listnode_getdata(sys_forkeyparts_node);

            table->field[0]->store((longlong)sys_forkeypart->id, FALSE);
            table->field[1]->store((longlong)sys_forkeypart->keyp_no, FALSE);
            table->field[2]->store((longlong)sys_forkeypart->attr_no, FALSE);
            table->field[3]->store((longlong)sys_forkeypart->attr_id, FALSE);
            table->field[4]->store((longlong)sys_forkeypart->attr_type, FALSE);

            if (sys_forkeypart->const_value) {
                table->field[5]->store(sys_forkeypart->const_value, strlen(sys_forkeypart->const_value), system_charset_info);
            } else {
                table->field[5]->set_null();
            }

            table->field[6]->store((longlong)sys_forkeypart->prefix_length, FALSE);
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            if (sys_forkeypart->const_value) {
                SsMemFree((char *)sys_forkeypart->const_value);
            }
        }

        su_list_done(sys_forkeyparts_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_schemas_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_SCHEMAS table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_schemas_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_schemas_list;
        su_list_node_t* sys_schemas_node;

        DBUG_ENTER("soliddb_sys_schemas_fill");
        SS_PUSHNAME("soliddb_sys_schemas_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_schemas_list = su_list_init(NULL);

        tb_dd_fill_sys_schemas_list(con->sc_cd, con->sc_trans, sys_schemas_list);

        su_list_do(sys_schemas_list, sys_schemas_node) {
            tb_dd_sys_schemas_t* sys_schema = (tb_dd_sys_schemas_t*)su_listnode_getdata(sys_schemas_node);

            table->field[0]->store((longlong)sys_schema->id, FALSE);
            table->field[1]->store(sys_schema->name, strlen(sys_schema->name), system_charset_info);
            
            if (sys_schema->owner) {
                table->field[2]->store(sys_schema->owner, strlen(sys_schema->owner), system_charset_info);
            } else {
                table->field[2]->set_null();
            }

            if (sys_schema->createtime) {
                table->field[3]->store(sys_schema->createtime, strlen(sys_schema->createtime), system_charset_info);
            } else {
                table->field[3]->set_null();
            }

            if (sys_schema->catalog) {
                table->field[4]->store(sys_schema->catalog, strlen(sys_schema->catalog), system_charset_info);
            } else {
                table->field[4]->set_null();
            }

            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            SsMemFree((char*)sys_schema->name);
            
            if (sys_schema->owner) {
                SsMemFree((char*)sys_schema->owner);
            }

            if (sys_schema->createtime) {
                SsMemFree((char*)sys_schema->createtime);
            }

            if (sys_schema->catalog) {
                SsMemFree((char*)sys_schema->catalog);
            }
        }

        su_list_done(sys_schemas_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_sequences_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_SEQUENCES table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_sequences_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_sequences_list;
        su_list_node_t* sys_sequences_node;

        DBUG_ENTER("soliddb_sys_sequences_fill");
        SS_PUSHNAME("soliddb_sys_sequences_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_sequences_list = su_list_init(NULL);

        tb_dd_fill_sys_sequences_list(con->sc_cd, con->sc_trans, sys_sequences_list);

        su_list_do(sys_sequences_list, sys_sequences_node) {
            tb_dd_sys_sequences_t* sys_sequence = (tb_dd_sys_sequences_t*)su_listnode_getdata(sys_sequences_node);

            table->field[0]->store(sys_sequence->name, strlen(sys_sequence->name), system_charset_info);
            table->field[1]->store((longlong)sys_sequence->id, FALSE);
            table->field[2]->store(sys_sequence->dense, strlen(sys_sequence->dense), system_charset_info);
            table->field[3]->store(sys_sequence->schema, strlen(sys_sequence->schema), system_charset_info);
            table->field[4]->store(sys_sequence->catalog, strlen(sys_sequence->catalog), system_charset_info);
            table->field[5]->store(sys_sequence->createtime, strlen(sys_sequence->createtime), system_charset_info);
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            SsMemFree((char*)sys_sequence->name);
            SsMemFree((char*)sys_sequence->dense);
            SsMemFree((char*)sys_sequence->schema);
            SsMemFree((char*)sys_sequence->catalog);
            SsMemFree((char*)sys_sequence->createtime);
        }

        su_list_done(sys_sequences_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_cardinal_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_CARDINAL table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_cardinal_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_cardinal_list;
        su_list_node_t* sys_cardinal_node;

        DBUG_ENTER("soliddb_sys_cardinal_fill");
        SS_PUSHNAME("soliddb_sys_cardinal_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_cardinal_list = su_list_init(NULL);

        tb_dd_fill_sys_cardinal_list(con->sc_cd, con->sc_trans, sys_cardinal_list);

        su_list_do(sys_cardinal_list, sys_cardinal_node) {
            tb_dd_sys_cardinal_t* sys_cardinal = (tb_dd_sys_cardinal_t*)su_listnode_getdata(sys_cardinal_node);

            table->field[0]->store((longlong)sys_cardinal->rel_id, FALSE);
            table->field[1]->store(sys_cardinal->cardin, FALSE);
            table->field[2]->store(sys_cardinal->size, FALSE);

            if (sys_cardinal->last_update) {
                table->field[3]->store(sys_cardinal->last_update, strlen(sys_cardinal->last_update), system_charset_info);
            } else {
                table->field[3]->set_null();
            }
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            if (sys_cardinal->last_update) {
                SsMemFree((char *)sys_cardinal->last_update);
            }
        }

        su_list_done(sys_cardinal_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_tablemodes_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_TABLEMODES table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_tablemodes_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_tablemodes_list;
        su_list_node_t* sys_tablemodes_node;

        DBUG_ENTER("soliddb_sys_tablemodes_fill");
        SS_PUSHNAME("soliddb_sys_cardinal_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_tablemodes_list = su_list_init(NULL);

        tb_dd_fill_sys_tablemodes_list(con->sc_cd, con->sc_trans, sys_tablemodes_list);

        su_list_do(sys_tablemodes_list, sys_tablemodes_node) {
            tb_dd_sys_tablemodes_t* sys_tablemode = (tb_dd_sys_tablemodes_t*)su_listnode_getdata(sys_tablemodes_node);

            table->field[0]->store((longlong)sys_tablemode->id, FALSE);

            if (sys_tablemode->mode) {
                table->field[1]->store(sys_tablemode->mode, strlen(sys_tablemode->mode), system_charset_info);
            } else {
                table->field[1]->set_null();
            }

            if (sys_tablemode->modify_time) {
                table->field[2]->store(sys_tablemode->modify_time, strlen(sys_tablemode->modify_time), system_charset_info);
            } else {
                table->field[2]->set_null();
            }

            if (sys_tablemode->modify_user) {
                table->field[3]->store(sys_tablemode->modify_user, strlen(sys_tablemode->modify_user), system_charset_info);
            } else {
                table->field[3]->set_null();
            }

            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            if (sys_tablemode->mode) {
                SsMemFree((char *)sys_tablemode->mode);
            }

            if (sys_tablemode->modify_time) {
                SsMemFree((char *)sys_tablemode->modify_time);
            }

            if (sys_tablemode->modify_user) {
                SsMemFree((char *)sys_tablemode->modify_user);
            }
        }

        su_list_done(sys_tablemodes_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_collations_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_COLLATIONS table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_collations_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_collations_list;
        su_list_node_t* sys_collations_node;

        DBUG_ENTER("soliddb_sys_collations_fill");
        SS_PUSHNAME("soliddb_sys_collations_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_collations_list = su_list_init(NULL);

        tb_dd_fill_sys_collations_list(con->sc_cd, con->sc_trans, sys_collations_list);

        su_list_do(sys_collations_list, sys_collations_node) {
            tb_dd_sys_collations_t* sys_collation = (tb_dd_sys_collations_t*)su_listnode_getdata(sys_collations_node);

            table->field[0]->store((longlong)sys_collation->id, FALSE);
            table->field[1]->store(sys_collation->charset_name, strlen(sys_collation->charset_name), system_charset_info);
            table->field[2]->store(sys_collation->collation_name, strlen(sys_collation->collation_name), system_charset_info);

            if (sys_collation->remarks) {
                table->field[3]->store(sys_collation->remarks, strlen(sys_collation->remarks), system_charset_info);
            } else {
                table->field[3]->set_null();
            }

            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            SsMemFree((char*)sys_collation->charset_name);
            SsMemFree((char*)sys_collation->collation_name);
            
            if (sys_collation->remarks) {
                SsMemFree((char*)sys_collation->remarks);
            }
        }

        su_list_done(sys_collations_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_info_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_INFO table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_info_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_info_list;
        su_list_node_t* sys_info_node;

        DBUG_ENTER("soliddb_sys_info_fill");
        SS_PUSHNAME("soliddb_sys_info_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_info_list = su_list_init(NULL);

        tb_dd_fill_sys_info_list(con->sc_cd, con->sc_trans, sys_info_list);

        su_list_do(sys_info_list, sys_info_node) {
            tb_dd_sys_info_t* sys_info = (tb_dd_sys_info_t*)su_listnode_getdata(sys_info_node);

            if (sys_info->property) {
                table->field[0]->store(sys_info->property, strlen(sys_info->property), system_charset_info);
            } else {
                table->field[0]->set_null();
            }

            if (sys_info->val_str) {
                table->field[1]->store(sys_info->val_str, strlen(sys_info->val_str), system_charset_info);
            } else {
                table->field[1]->set_null();
            }

            table->field[2]->store((longlong)sys_info->val_int, TRUE);
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }

            if (sys_info->property) {
                SsMemFree((char*)sys_info->property);
            }

            if (sys_info->val_str) {
                SsMemFree((char*)sys_info->val_str);
            }    
        }

        su_list_done(sys_info_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_sys_blobs_fill
 *
 * Fill INFORMATION_SCHEMA.SOLIDDB_SYS_BLOBS table
 *
 * Parameters :
 *
 *     MYSQL_THD    thd, in, use, MySQL thread
 *     TABLE_LIST   tables, in, use, MySQL table
 *     COND*        cond, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int soliddb_sys_blobs_fill(
        MYSQL_THD   thd,
        TABLE_LIST* tables,
        COND*       cond)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        TABLE *table = tables->table;
        su_list_t*     sys_blobs_list;
        su_list_node_t* sys_blobs_node;

        DBUG_ENTER("soliddb_sys_blobs_fill");
        SS_PUSHNAME("soliddb_sys_blobs_fill");
        con = (SOLID_CONN*) get_solid_ha_data_connection(legacy_soliddb_hton, thd);

        CHK_CONN(con);
        sys_blobs_list = su_list_init(NULL);

        tb_dd_fill_sys_blobs_list(con->sc_cd, con->sc_trans, sys_blobs_list);

        su_list_do(sys_blobs_list, sys_blobs_node) {
            tb_dd_sys_blobs_t* sys_blob = (tb_dd_sys_blobs_t*)su_listnode_getdata(sys_blobs_node);

            table->field[0]->store(sys_blob->id, FALSE);
            table->field[1]->store(sys_blob->startpos, FALSE);
            table->field[2]->store(sys_blob->endsize, FALSE);
            table->field[3]->store(sys_blob->totalsize, FALSE);
            table->field[4]->store((longlong)sys_blob->refcount, FALSE);
            table->field[5]->store((longlong)sys_blob->complete, FALSE);
            table->field[6]->store((longlong)sys_blob->startcpnum, FALSE);
            table->field[7]->store((longlong)sys_blob->numpages, FALSE);
            
            if (schema_table_store_record(thd, table)) {
                SS_POPNAME;
                DBUG_RETURN(1);
            }
        }

        su_list_done(sys_blobs_list);

        SS_POPNAME;
        DBUG_RETURN(0);
}


/*#***********************************************************************\
 *
 *              soliddb_init_sys_tables
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_TABLES table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_tables(void *p)
{
        DBUG_ENTER("soliddb_init_sys_tables");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        /* ss_dassert(schema != NULL); */

        schema->fields_info = soliddb_sys_tables_fields;
        schema->fill_table  = soliddb_sys_tables_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_columns
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_COLUMNS table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_columns(void *p)
{
        DBUG_ENTER("soliddb_init_sys_columns");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_columns_fields;
        schema->fill_table  = soliddb_sys_columns_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_columns_aux
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_COLUMNS_AUX table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_columns_aux(void *p)
{
        DBUG_ENTER("soliddb_init_sys_columns");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_columns_aux_fields;
        schema->fill_table  = soliddb_sys_columns_aux_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_keys
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_KEYS table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_keys(void *p)
{
        DBUG_ENTER("soliddb_init_sys_keys");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_keys_fields;
        schema->fill_table  = soliddb_sys_keys_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_keyparts
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_KEYPARTS table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_keyparts(void *p)
{
        DBUG_ENTER("soliddb_init_sys_keyparts");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_keyparts_fields;
        schema->fill_table  = soliddb_sys_keyparts_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_keyparts_aux
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_KEYPARTS_AUX table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_keyparts_aux(void *p)
{
        DBUG_ENTER("soliddb_init_sys_keyparts_aux");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_keyparts_aux_fields;
        schema->fill_table  = soliddb_sys_keyparts_aux_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_forkeys
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_FORKEYS table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_forkeys(void *p)
{
        DBUG_ENTER("soliddb_init_sys_forkeys");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_forkeys_fields;
        schema->fill_table  = soliddb_sys_forkeys_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_columns
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_FORKEYPARTS table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_forkeyparts(void *p)
{
        DBUG_ENTER("soliddb_init_sys_forkeyparts");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_forkeyparts_fields;
        schema->fill_table  = soliddb_sys_forkeyparts_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_columns
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_SCHEMAS table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_schemas(void *p)
{
        DBUG_ENTER("soliddb_init_sys_schemas");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_schemas_fields;
        schema->fill_table  = soliddb_sys_schemas_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_sequences
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_SEQUENCES table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_sequences(void *p)
{
        DBUG_ENTER("soliddb_init_sys_sequences");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_sequences_fields;
        schema->fill_table  = soliddb_sys_sequences_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_cardinal
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_CARDINAL table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_cardinal(void *p)
{
        DBUG_ENTER("soliddb_init_sys_cardinal");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_cardinal_fields;
        schema->fill_table  = soliddb_sys_cardinal_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_tablemodes
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_TABLEMODES table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_tablemodes(void *p)
{
        DBUG_ENTER("soliddb_init_sys_tablemodes");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_tablemodes_fields;
        schema->fill_table  = soliddb_sys_tablemodes_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_collations
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_COLLATIONS table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_collations(void *p)
{
        DBUG_ENTER("soliddb_init_sys_collations");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_collations_fields;
        schema->fill_table  = soliddb_sys_collations_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_info
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_INFO table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_info(void *p)
{
        DBUG_ENTER("soliddb_init_sys_info");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_info_fields;
        schema->fill_table  = soliddb_sys_info_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_init_sys_blobs
 *
 * Init INFORMATION_SCHEMA.SOLIDDB_SYS_BLOBS table
 *
 * Parameters :
 *
 *    void* p, in, use, ST_SCHEMA_TABLE pointer
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_init_sys_blobs(void *p)
{
        DBUG_ENTER("soliddb_init_sys_blobs");
        ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
        ss_dassert(schema != NULL);

        schema->fields_info = soliddb_sys_blobs_fields;
        schema->fill_table  = soliddb_sys_blobs_fill;
        
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_deinit_is
 *
 * Deinit information schema plugin
 *
 * Parameters :   void *p
 *
 * Return value : 0
 *
 * Globals used :
 */
static
int soliddb_deinit_is(void *p)
{
	DBUG_ENTER("soliddb_deinit_is");

	/* Do nothing */
	DBUG_RETURN(0);
        /* return (0); */
}

