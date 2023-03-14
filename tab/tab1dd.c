/*************************************************************************\
**  source       * tab1dd.c
**  directory    * tab
**  description  * Data dictionary handling.
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

#include <ssstring.h>
#include <sstime.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssprint.h>
#include <ssservic.h>
#include <sschcvt.h>
#include <ssthread.h>
#include <ssfile.h>

#include <uti0dyn.h>

#include <su0parr.h>
#include <su0prof.h>
#include <su0cfgst.h>
#include <su0li3.h>

#include <ui0msg.h>

#include <rs0types.h>
#include <rs0sdefs.h>
#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0key.h>
#include <rs0relh.h>
#include <rs0viewh.h>
#include <rs0event.h>
#include <rs0auth.h>
#include <rs0sysi.h>
#include <rs0rbuf.h>
#include <rs0cardi.h>
#include <rs0entna.h>
#include <rs0tnum.h>
#include <rs0admev.h>
#include <dt0date.h>

#include <dbe0db.h>
#include <dbe0trx.h>

#include "tab1defs.h"
#include "tab1priv.h"

#include "tab0conn.h"
#include "tab0tli.h"

#ifndef SS_MYSQL
#include "tab1hcol.h"
#include "tab0sqls.h"
#include "tab0sql.h"
#include "tab0sync.h"
#endif /* !SS_MYSQL */

#include "tab0trig.h"
#include "tab0admi.h"
#include "tab0relh.h"
#include "tab0cata.h"
#include "tab0sche.h"
#include "tab0blobg2.h"
#include "tab1dd.h"
#include "tab2dd.h"
#include "tab0minisql.h"

#define DD_RELMODE_OPTIMISTIC   "OPTIMISTIC"
#define DD_RELMODE_PESSIMISTIC  "PESSIMISTIC"
#define DD_RELMODE_NOCHECK      "NOCHECK"
#define DD_RELMODE_CHECK        "CHECK"
#define DD_RELMODE_MAINMEMORY   "MAINMEMORY"
#define DD_RELMODE_SYNC         "SYNCHISTORY"
#define DD_RELMODE_TRANSIENT    "TRANSIENT"
#define DD_RELMODE_GLOBALTEMPORARY  "GLOBALTEMPORARY"

typedef enum {
        DD_KEYFOUND_ZERO,
        DD_KEYFOUND_ONE,
        DD_KEYFOUND_MANY,
        DD_KEYFOUND_ERROR
} dd_keyfound_t;

typedef enum {
        DD_READRELH_MINIMAL,
        DD_READRELH_KEYS,
        DD_READRELH_FULL
} dd_readrelh_t;

typedef enum {
        DD_TRUNCATE_AFTERCOMMIT_INIT,
        DD_TRUNCATE_AFTERCOMMIT_CONT
} dd_truncatetable_state_t;

typedef struct {
        dd_truncatetable_state_t    ti_state;
        long                        ti_oldrelid;
        long                        ti_newrelid;
        TliConnectT*                ti_tcon;
        tb_connect_t*               ti_tbcon;
        tb_trans_t*                 ti_trans;
} dd_truncatetable_t;

#ifndef SS_NODDUPDATE

int  tb_dd_syncconvertthreads = 0;
#ifdef SS_MME
bool tb_dd_enablemainmem = TRUE;
#else
bool tb_dd_enablemainmem = FALSE;
#endif

#ifdef SS_COLLATION
su_collation_t* (*tb_dd_collation_init)(char* collation_name, uint collation_id);
#endif /* SS_COLLATION */

static char dd_dropsystypesstmt[] = "DROP TABLE SYS_TYPES";

static bool dd_setrelmodes(
        rs_sysinfo_t* cd,
        TliConnectT* tcon,
        rs_relh_t* relh,
        long relid,
        tb_relmodemap_t relmodes,
        bool createrel,
        rs_err_t** p_errh);

#ifndef SS_MYSQL
static void dd_checkexistenceaction(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys,
        tb_trans_t* trans,
        bool name_in_use,
        char* option);

static void populatecolumninfotable(
        rs_sysinfo_t* cd,
        sqlsystem_t* sqlsys,
        tb_trans_t* trans);

static bool dd_createsynchistoryversionkey(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_ttype_t* ttype,
        rs_err_t** p_errh);
#endif /* !SS_MYSQL */

static void dd_droprefkeyparts(
        TliConnectT* tcon,
        long key);

static bool dd_isforkeyref(
        TliConnectT* tcon,
        rs_relh_t* relh,
        bool* p_isforkeyref,
        rs_err_t** p_errh);

static void dd_createrelh(
        TliConnectT* tcon,
        rs_relh_t* relh,
        tb_dd_createrel_t type,
        rs_entname_t* newname,
        long relationid);

static rs_relh_t* dd_readrelh(
        TliConnectT*    tcon,
        ulong           relid,
        dd_readrelh_t   readrelh_type,
        bool            dolock,
        tb_trans_t*     usertrans,
        dbe_ret_t*      p_rc);

static bool dd_droprelh(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_err_t** p_errh);

static void dd_dropkeyparts(
        TliConnectT* tcon,
        long keyid);

static rs_key_t *dd_resolverefkey(
        rs_sysinfo_t*   cd,
        rs_relh_t*      relh,
        rs_key_t*       refkey);

static su_ret_t dd_createnamedcheck(
        TliConnectT* tcon,
        long relid,
        char* name,
        char* checkstring);

static void dd_readrefkeyparts(
        TliConnectT* tcon,
        rs_ttype_t* ttype,
        rs_key_t* key);

#define DD_SYSVARCHAR_MACRO     "$(VCHR)"
#define DD_SYSVARCHAR_NAME      RS_TN_SYSVARCHAR

#define DD_SYSDEFCAT_MACRO      "$(DEFCAT)"
#define DD_SYSDEFCAT_NEW_MACRO  "$(DEFCAT_NEW)"
#define DD_NOWTIMESTAMP_MACRO   "$(NOW)"

#define DD_SYNCVERSTYPE_MACRO   "$(SYNCVERS)"

#ifdef SS_UNICODE_SQL

bool tb_dd_migratingtounicode = FALSE;
bool tb_dd_migratetounicode_done = FALSE;

#endif /* SS_UNICODE_SQL */


#ifndef SS_HSBG2
static char* dd_rowlevelhsbsysrels[] = {
        RS_RELNAME_SYNC_BOOKMARKS,
        /* RS_RELNAME_SYNC_USERMAPS, */
        RS_RELNAME_SYNC_USERS,
        RS_RELNAME_SYNC_MASTER_MSR,
        RS_RELNAME_SYNC_REPLICA_VERS,
        RS_RELNAME_SYNC_MASTER_VERS,
        RS_RELNAME_SYNC_SAVED_STMTS,
        RS_RELNAME_SYNC_SAVED_BLOB_ARGS,
        RS_RELNAME_SYNC_RECEIVED_STMTS,
        RS_RELNAME_SYNC_RECEIVED_BLOB_ARGS,
        RS_RELNAME_PUBL,
        RS_RELNAME_PUBL_ARGS,
        RS_RELNAME_PUBL_STMTS,
        RS_RELNAME_PUBL_STMTARGS,
        RS_RELNAME_PUBL_REPL,
        RS_RELNAME_PUBL_REPL_ARGS,
        RS_RELNAME_PUBL_REPL_STMTS,
        RS_RELNAME_PUBL_REPL_STMTARGS,
        RS_RELNAME_SYNC_MASTERS,
        RS_RELNAME_SYNC_REPLICAS,
        RS_RELNAME_SYNC_REPLICA_STORED_MSGS,
        RS_RELNAME_SYNC_REPLICA_STORED_MSGPARTS,
        RS_RELNAME_SYNC_MASTER_RECEIVED_MSGS,
        RS_RELNAME_SYNC_MASTER_RECEIVED_MSGPARTS,
        RS_RELNAME_SYNC_MASTER_STORED_MSGS,
        RS_RELNAME_SYNC_MASTER_STORED_MSGPARTS,
        RS_RELNAME_SYNC_REPLICA_RECEIVED_MSGS,
        RS_RELNAME_SYNC_REPLICA_RECEIVED_MSGPARTS,
        RS_RELNAME_MASTER_MSGINFO,
        RS_RELNAME_REPLICA_MSGINFO,
        /* RS_RELNAME_BULLETIN_BOARD, */
        RS_RELNAME_HISTORYCOLS,
        RS_RELNAME_TRXPROPERTIES,
        /* RS_RELNAME_SYNCINFO, */

        RS_RELNAME_SYNC_REPLICA_STORED_BLOB_REFS,
        RS_RELNAME_SYNC_REPLICA_RECEIVED_BLOB_REFS,
        RS_RELNAME_SYNC_MASTER_STORED_BLOB_REFS,
        RS_RELNAME_SYNC_MASTER_RECEIVED_BLOB_REFS,

        RS_RELNAME_DL_CONFIG,
        RS_RELNAME_DL_DEFAULT,
        NULL
};
#endif /* !SS_HSBG2 */

static const char dd_createsystypesstmt[] =
"CREATE TABLE SYS_TYPES \
(TYPE_NAME $(VCHR) NOT NULL,\
 DATA_TYPE SMALLINT NOT NULL,\
 PRECISION INTEGER,\
 LITERAL_PREFIX $(VCHR),\
 LITERAL_SUFFIX $(VCHR),\
 CREATE_PARAMS $(VCHR),\
 NULLABLE SMALLINT,\
 CASE_SENSITIVE SMALLINT,\
 SEARCHABLE SMALLINT,\
 UNSIGNED_ATTRIBUTE SMALLINT,\
 MONEY SMALLINT,\
 AUTO_INCREMENT SMALLINT,\
 LOCAL_TYPE_NAME $(VCHR),\
 MINIMUM_SCALE SMALLINT,\
 MAXIMUM_SCALE SMALLINT,\
 PRIMARY KEY (DATA_TYPE,TYPE_NAME))";

                           /*  Name               DT# Prec        Pref    Suff CreaPar           Nul Cas Sea Uns  Mon AutI LocN MinS MaxS */
                           /*  -----------------  --- -------     ------- ---- ----------------- --- --- --- ---- --- ---- ---- ---- ---- */
static char dd_systypesdata0[] =
"INSERT INTO SYS_TYPES VALUES ('CHAR',            1,  32767,      '''',   '''','length',         1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata1[] =
"INSERT INTO SYS_TYPES VALUES ('NUMERIC',         2,  16,         NULL,   NULL,'precision,scale',1,  0,  2,  0,   1,  NULL,NULL,0   ,16  )";
static char dd_systypesdata2[] =
"INSERT INTO SYS_TYPES VALUES ('DECIMAL',         3,  16,         NULL,   NULL,'precision,scale',1,  0,  2,  0,   1,  NULL,NULL,0   ,16  )";
static char dd_systypesdata3[] =
"INSERT INTO SYS_TYPES VALUES ('INTEGER',         4,  10,         NULL,   NULL,NULL,             1,  0,  2,  0,   0,  NULL,NULL,0   ,0   )";
static char dd_systypesdata4[] =
"INSERT INTO SYS_TYPES VALUES ('SMALLINT',        5,  5,          NULL,   NULL,NULL,             1,  0,  2,  0,   0,  NULL,NULL,0   ,0   )";
static char dd_systypesdata5[] =
"INSERT INTO SYS_TYPES VALUES ('FLOAT',           6,  15,         NULL,   NULL,'precision',      1,  0,  2,  0,   0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata6[] =
"INSERT INTO SYS_TYPES VALUES ('REAL',            7,  7,          NULL,   NULL,NULL,             1,  0,  2,  0,   0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata7[] =
"INSERT INTO SYS_TYPES VALUES ('DOUBLE PRECISION',8,  15,         NULL,   NULL,NULL,             1,  0,  2,  0,   0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata8[] =
"INSERT INTO SYS_TYPES VALUES ('DATE',            9,  10,         '''',   '''',NULL,             1,  0,  2,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata9[] =
"INSERT INTO SYS_TYPES VALUES ('TIME',            10, 8,          '''',   '''',NULL,             1,  0,  2,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata10[] =
"INSERT INTO SYS_TYPES VALUES ('TIMESTAMP',       11, 29,         '''',   '''',NULL,             1,  0,  2,  NULL,0,  NULL,NULL,0   ,9   )";
static char dd_systypesdata11[] =
"INSERT INTO SYS_TYPES VALUES ('VARCHAR',         12, 32767,      '''',   '''','length',         1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata12[] =
"INSERT INTO SYS_TYPES VALUES ('LONG VARCHAR',    -1, 2147483647, '''',   '''',NULL,             1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata13[] =
"INSERT INTO SYS_TYPES VALUES ('BINARY',          -2, 32767,      '''',   '''','length',         1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata14[] =
"INSERT INTO SYS_TYPES VALUES ('VARBINARY',       -3, 32767,      '''',   '''','length',         1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata15[] =
"INSERT INTO SYS_TYPES VALUES ('LONG VARBINARY',  -4, 2147483647, '''',   '''',NULL,             1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata16[] =
"INSERT INTO SYS_TYPES VALUES ('BIGINT',          -5, 19,         NULL,   NULL,NULL,             1,  0,  2,  0,   0,  NULL,NULL,0   ,0   )";
static char dd_systypesdata17[] =
"INSERT INTO SYS_TYPES VALUES ('TINYINT',         -6, 3,          NULL,   NULL,NULL,             1,  0,  2,  0,   0,  NULL,NULL,0   ,0   )";
#ifdef SS_UNICODE_DATA
static char dd_systypesdata18[] =
"INSERT INTO SYS_TYPES VALUES ('WCHAR',           -8,32767,      '''',   '''','length',         1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata19[] =
"INSERT INTO SYS_TYPES VALUES ('WVARCHAR',        -9,32767,      '''',   '''','length',         1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
static char dd_systypesdata20[] =
"INSERT INTO SYS_TYPES VALUES ('LONG WVARCHAR',   -10,2147483647, '''',   '''',NULL,             1,  1,  3,  NULL,0,  NULL,NULL,NULL,NULL)";
#endif /* SS_UNICODE_DATA */


static const char* dd_startupstmts[] = {

"CREATE TABLE SYS_USERS\n\
(ID INTEGER NOT NULL,\n\
NAME $(VCHR) NOT NULL,\n\
TYPE $(VCHR) NOT NULL,\n\
PRIV INTEGER NOT NULL,\n\
PASSW VARBINARY(254),\n\
PRIORITY INTEGER,\n\
PRIVATE  INTEGER NOT NULL,\n\
LOGIN_CATALOG $(VCHR),\n\
PRIMARY KEY (ID))",

"INSERT INTO SYS_INFO VALUES ('IDENTIFIER_LENGTH','254',254)",
"INSERT INTO SYS_INFO VALUES ('ROW_LENGTH','2147483647',2147483647)",
"INSERT INTO SYS_INFO VALUES ('USERID_LENGTH','254',254)",
"INSERT INTO SYS_INFO VALUES ('TXN_ISOLATION','SERIALIZABLE',4)",
#ifndef COLLATION_UPDATE
"INSERT INTO SYS_INFO VALUES ('COLLATION_SEQ','ISO 8859-1', NULL)",
#endif

"CREATE TABLE SQL_LANGUAGES (\
 SOURCE           $(VCHR) NOT NULL,\
 SOURCE_YEAR      $(VCHR),\
 CONFORMANCE      $(VCHR),\
 INTEGRITY        $(VCHR),\
 IMPLEMENTATION   $(VCHR),\
 BINDING_STYLE    $(VCHR),\
 PROGRAMMING_LANG $(VCHR))",

dd_createsystypesstmt,
"COMMIT WORK",
dd_systypesdata0,
dd_systypesdata1,
dd_systypesdata2,
dd_systypesdata3,
dd_systypesdata4,
dd_systypesdata5,
dd_systypesdata6,
dd_systypesdata7,
dd_systypesdata8,
dd_systypesdata9,
dd_systypesdata10,
dd_systypesdata11,
dd_systypesdata12,
dd_systypesdata13,
dd_systypesdata14,
dd_systypesdata15,
dd_systypesdata16,
dd_systypesdata17,
dd_systypesdata18,
dd_systypesdata19,
dd_systypesdata20,
NULL
};

#ifndef SS_MYSQL
static const char* dd_startupstmts_be[] = {


"CREATE VIEW TABLES\
 (TABLE_CATALOG,TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,REMARKS)\
 AS SELECT TABLE_CATALOG,TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,REMARKS FROM SYS_TABLES",


"CREATE VIEW SERVER_INFO\
 (SERVER_ATTRIBUTE,ATTRIBUTE_VALUE) AS\
 SELECT PROPERTY,VALUE_STR FROM SYS_INFO",

NULL};

#endif /* !SS_MYSQL */

/* NOTE! One extra space after table name needed in
 * dd_updatesysrelschemakeys.
 */
static char dd_create_table_sys_sequences[] =
"CREATE TABLE SYS_SEQUENCES \
(SEQUENCE_NAME $(VCHR) NOT NULL,\
 ID INTEGER NOT NULL,\
 DENSE $(VCHR)(3) NOT NULL,\
 SEQUENCE_SCHEMA $(VCHR) NOT NULL,\
 SEQUENCE_CATALOG $(VCHR) NOT NULL,\
 CREATIME TIMESTAMP,\
 PRIMARY KEY(ID))\
@\
 CREATE UNIQUE INDEX \"$$SYS_SEQUENCES_UNQKEY_0\" ON SYS_SEQUENCES (SEQUENCE_NAME,SEQUENCE_SCHEMA,SEQUENCE_CATALOG)";


/* dd_updatetype_t typedef moved to tab1dd.h : JVu*/


static dd_updatestmt_t dd_updatestmts[] = {
/* Create new table if it does not exist. */
{
"SYS_BLOBS", NULL, DD_UPDATE_CREATETABLE,
tb_blobg2_sysblobs_create_stmts
},
{
"", "", DD_UPDATE_COMMIT,
""
},

{
"SYS_USERS", "SYS_KEY_USERS_NAME", DD_UPDATE_CREATEINDEX,
"CREATE UNIQUE INDEX SYS_KEY_USERS_NAME ON SYS_USERS (NAME)"
},

#ifdef REFERENTIAL_INTEGRITY

{
"SYS_FORKEYS", NULL, DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_FORKEYS\
(ID INTEGER NOT NULL,\
 REF_REL_ID INTEGER NOT NULL,\
 CREATE_REL_ID INTEGER NOT NULL,\
 REF_KEY_ID INTEGER NOT NULL,\
 REF_TYPE INTEGER NOT NULL,\
 KEY_SCHEMA $(VCHR),\
 KEY_CATALOG $(VCHR) NOT NULL,\
 KEY_NREF INTEGER NOT NULL,\
 KEY_NAME $(VCHR),\
 KEY_ACTION INTEGER,\
 PRIMARY KEY(REF_REL_ID, ID))"
/* KEY_ACTION INTEGER NOT NULL DEFAULT 0 */
},

{
"SYS_FORKEYS", "SYS_KEY_FORKEYS_CREATE_REL_ID", DD_UPDATE_CREATEINDEX,

"CREATE INDEX SYS_KEY_FORKEYS_CREATE_REL_ID ON SYS_FORKEYS\
(CREATE_REL_ID)"
},

{
"SYS_FORKEYPARTS", NULL, DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_FORKEYPARTS\
(ID INTEGER NOT NULL,\
 KEYP_NO INTEGER NOT NULL,\
 ATTR_NO INTEGER NOT NULL,\
 ATTR_ID INTEGER NOT NULL,\
 ATTR_TYPE INTEGER NOT NULL,\
 CONST_VALUE VARBINARY, "
#ifdef SS_COLLATION
 RS_ANAME_FORKEYPARTS_PREFIX_LENGTH " INTEGER, "
#endif /* SS_COLLATION */
"PRIMARY KEY(ID, KEYP_NO))"
},

#ifdef SS_COLLATION
{
RS_RELNAME_FORKEYPARTS, RS_ANAME_FORKEYPARTS_PREFIX_LENGTH, DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\"."
RS_RELNAME_FORKEYPARTS
" ADD COLUMN "
RS_ANAME_FORKEYPARTS_PREFIX_LENGTH
" INTEGER @COMMIT WORK@"
},
#endif /* SS_COLLATION */
    
{
"SYS_FORKEYS", "KEY_NAME", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_FORKEYS ADD COLUMN KEY_NAME $(VCHR) @COMMIT WORK@"
},
{
"SYS_FORKEYS", "KEY_ACTION", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_FORKEYS ADD COLUMN KEY_ACTION INTEGER @COMMIT WORK@"
},
{
"", "", DD_UPDATE_COMMIT,
""
},

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
{
"SYS_FORKEYS_UNRESOLVED", NULL, DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_FORKEYS_UNRESOLVED\
(ID INTEGER NOT NULL,\
 CRE_REL_ID INTEGER NOT NULL,\
 FKEY_NAME $(VCHR),\
 FKEY_EXTERNALNAME $(VCHR),\
 FKEY_LEN INTEGER NOT NULL,\
 REF_REL_NAME $(VCHR) NOT NULL,\
 REF_REL_SCHEMA $(VCHR),\
 REF_REL_CATALOG $(VCHR),\
 REF_DEL INTEGER NOT NULL,\
 REF_UPD INTEGER NOT NULL,\
 PRIMARY KEY(ID))"
},

{
"SYS_FORKEYS_UNRESOLVED", "SYS_KEY_FORKEYS_UNRESOLVED", DD_UPDATE_CREATEINDEX,

"CREATE INDEX SYS_KEY_FORKEYS_UNRESOLVED ON SYS_FORKEYS_UNRESOLVED\
(REF_REL_NAME, REF_REL_SCHEMA, REF_REL_CATALOG)"
},

{
"", "", DD_UPDATE_COMMIT,
""
},

{
"SYS_FORKEYPARTS_UNRESOLVED", NULL, DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_FORKEYPARTS_UNRESOLVED\
(ID INTEGER NOT NULL,\
 REF_FIELD_ANO INTEGER NOT NULL,\
 REF_FIELD_NAME $(VCHR) NOT NULL,\
 PRIMARY KEY(ID))"
},

{
"SYS_FORKEYPARTS_UNRESOLVED", "SYS_KEY_FORKEYPARTS_UNRESOLVED", DD_UPDATE_CREATEINDEX,

"CREATE INDEX SYS_KEY_FORKEYPARTS_UNRESOLVED ON SYS_FORKEYPARTS_UNRESOLVED\
(REF_FIELD_ANO, REF_FIELD_NAME)"
},

{
"", "", DD_UPDATE_COMMIT,
""
},
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */
#endif /* REFERENTIAL_INTEGRITY */


{
"SYS_CHECKSTRINGS", NULL, DD_UPDATE_CREATETABLE,
"CREATE TABLE SYS_CHECKSTRINGS \
(REL_ID INTEGER NOT NULL,\
 CONSTRAINT_NAME $(VCHR) NOT NULL,\
 CONSTRAINT LONG $(VCHR) NOT NULL,\
 PRIMARY KEY(REL_ID, CONSTRAINT_NAME))"
},
{
"SYS_PROCEDURES", NULL, DD_UPDATE_CREATETABLE,
"CREATE TABLE SYS_PROCEDURES\
(ID INTEGER NOT NULL,\
 PROCEDURE_NAME $(VCHR) NOT NULL,\
 PROCEDURE_TEXT LONG $(VCHR) NOT NULL,\
 PROCEDURE_BIN LONG VARBINARY,\
 PROCEDURE_SCHEMA $(VCHR) NOT NULL,\
 PROCEDURE_CATALOG $(VCHR) NOT NULL,\
 CREATIME TIMESTAMP,\
 TYPE INTEGER,\
 PRIMARY KEY(ID))\
@\
 CREATE UNIQUE INDEX \"$$SYS_PROCEDURES_UNQKEY_0\" ON SYS_PROCEDURES (PROCEDURE_NAME,PROCEDURE_SCHEMA,PROCEDURE_CATALOG)\
@\
COMMIT WORK"
},

{
"SYS_TRIGGERS", NULL, DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_TRIGGERS\
(ID INTEGER NOT NULL,\
 TRIGGER_NAME $(VCHR) NOT NULL,\
 TRIGGER_TEXT LONG $(VCHR) NOT NULL,\
 TRIGGER_BIN LONG VARBINARY,\
 TRIGGER_SCHEMA $(VCHR) NOT NULL,\
 TRIGGER_CATALOG $(VCHR) NOT NULL,\
 CREATIME TIMESTAMP,\
 TYPE INTEGER NOT NULL,\
 REL_ID INTEGER NOT NULL,\
 TRIGGER_ENABLED $(VCHR)(3) NOT NULL,\
 PRIMARY KEY(ID))\
@\
 CREATE UNIQUE INDEX \"$$SYS_TRIGGERS_UNQKEY_0\" ON SYS_TRIGGERS (TRIGGER_NAME,TRIGGER_SCHEMA,TRIGGER_CATALOG)\
@\
 CREATE UNIQUE INDEX \"$$SYS_TRIGGERS_UNQKEY_1\" ON SYS_TRIGGERS (REL_ID,TYPE)\
@\
COMMIT WORK"
},

{
"SYS_TRIGGERS", "TRIGGER_ENABLED", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE SYS_TRIGGERS ADD COLUMN TRIGGER_ENABLED $(VCHR)(3)\
@\
COMMIT WORK\
@\
UPDATE SYS_TRIGGERS SET TRIGGER_ENABLED = 'YES'\
@\
COMMIT WORK"
},

{
"SYS_TABLEMODES", NULL, DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_TABLEMODES\
(ID INTEGER NOT NULL,\
 MODE $(VCHR) NOT NULL,\
 MODIFY_TIME TIMESTAMP,\
 MODIFY_USER $(VCHR),\
 PRIMARY KEY(ID))"
},

{
"SYS_EVENTS", NULL, DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_EVENTS\
(ID INTEGER NOT NULL,\
 EVENT_NAME $(VCHR) NOT NULL,\
 EVENT_PARAMCOUNT INTEGER NOT NULL,\
 EVENT_PARAMTYPES LONG VARBINARY,\
 EVENT_TEXT $(VCHR),\
 EVENT_SCHEMA $(VCHR) NOT NULL,\
 EVENT_CATALOG $(VCHR) NOT NULL,\
 CREATIME TIMESTAMP,\
 TYPE INTEGER,\
 PRIMARY KEY(ID))\
@\
 CREATE UNIQUE INDEX \"$$SYS_EVENTS_UNQKEY_0\" ON SYS_EVENTS (EVENT_NAME,EVENT_SCHEMA,EVENT_CATALOG)"
},

{
"SYS_CATALOGS", NULL, DD_UPDATE_CREATETABLE,
"CREATE TABLE SYS_CATALOGS\
(ID INTEGER NOT NULL,\
 NAME $(VCHR) NOT NULL,\
 CREATIME TIMESTAMP NOT NULL,\
 CREATOR $(VCHR) NOT NULL,\
 PRIMARY KEY(ID),\
 UNIQUE(NAME))\
@\
COMMIT WORK\
@\
INSERT INTO SYS_CATALOGS VALUES (1,$(DEFCAT_NEW),$(NOW),'_SYSTEM')\
@\
COMMIT WORK"
},

#ifdef SS_MYSQL

{
"SYS_SCHEMAS", NULL, DD_UPDATE_CREATETABLE,
"CREATE TABLE SYS_SCHEMAS\
(ID INTEGER NOT NULL,\
 NAME $(VCHR) NOT NULL,\
 OWNER $(VCHR) NOT NULL,\
 CREATIME TIMESTAMP NOT NULL,\
 SCHEMA_CATALOG $(VCHR),\
 PRIMARY KEY(ID))\
@\
COMMIT WORK\
@\
CREATE UNIQUE INDEX SYS_SCHEMAS_NAME_IDX ON SYS_SCHEMAS(NAME,SCHEMA_CATALOG)\
@\
COMMIT WORK\
@\
INSERT INTO _SYSTEM.SYS_SCHEMAS(ID,NAME,OWNER,CREATIME,SCHEMA_CATALOG) \
VALUES(1,'DBA','DBA',$(NOW),'DBA')"
},

#else /* SS_MYSQL */

{
"SYS_SCHEMAS", NULL, DD_UPDATE_CREATETABLE,
"CREATE TABLE SYS_SCHEMAS\
(ID INTEGER NOT NULL,\
 NAME $(VCHR) NOT NULL,\
 OWNER $(VCHR) NOT NULL,\
 CREATIME TIMESTAMP NOT NULL,\
 SCHEMA_CATALOG $(VCHR),\
 PRIMARY KEY(ID))\
@\
COMMIT WORK\
@\
CREATE UNIQUE INDEX SYS_SCHEMAS_NAME_IDX ON SYS_SCHEMAS(NAME,SCHEMA_CATALOG)\
@\
COMMIT WORK\
@\
INSERT INTO _SYSTEM.SYS_SCHEMAS(ID,NAME,OWNER,CREATIME,SCHEMA_CATALOG) \
SELECT ID,NAME,NAME,NOW(),$(DEFCAT) FROM _SYSTEM.SYS_USERS"
},
#endif

#ifndef SS_NOSEQUENCE

{
"SYS_SEQUENCES", NULL, DD_UPDATE_CREATETABLE,

dd_create_table_sys_sequences,

},

#endif /* SS_NOSEQUENCE */

#ifdef SS_HSBG2
{
"SYS_PROPERTIES", NULL,  DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_PROPERTIES\
(KEY $(VCHR) NOT NULL,\
 VALUE $(VCHR) NOT NULL,\
 MODTIME TIMESTAMP,\
 PRIMARY KEY(KEY))"
},

{
"", "", DD_UPDATE_COMMIT,
""
},
#endif /* SS_HSBG2 */

#if defined(DBE_HSB_REPLICATION)

{
"SYS_HOTSTANDBY", NULL,  DD_UPDATE_CREATETABLE,

"CREATE TABLE SYS_HOTSTANDBY\
(PROPERTY $(VCHR) NOT NULL,\
 VALUE $(VCHR) NOT NULL,\
 HOTSTANDBY_SCHEMA $(VCHR),\
 MODTIME TIMESTAMP,\
 PRIMARY KEY(PROPERTY))"
},

#endif /* defined(DBE_HSB_REPLICATION) */

#ifdef SS_SYNC
{
"SYS_USERS", "PRIVATE", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE SYS_USERS ADD COLUMN PRIVATE INTEGER"
},
{
"", "", DD_UPDATE_COMMIT,
""
},
{
"SYS_USERS", "SYS_KEY_USERS_ISPRIVATE", DD_UPDATE_CREATEINDEX,
"CREATE INDEX SYS_KEY_USERS_ISPRIVATE ON SYS_USERS (PRIVATE, NAME)"
},
{
"", "", DD_UPDATE_COMMIT,
""
},
{
"", "", DD_UPDATE_COMMIT,
""
},

{
"SYS_SEQUENCES", "SEQUENCE_CATALOG", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_SEQUENCES ADD COLUMN SEQUENCE_CATALOG $(VCHR)"
},
{
"", "", DD_UPDATE_COMMIT,
""
},
{
"SYS_PROCEDURES", "PROCEDURE_CATALOG", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_PROCEDURES ADD COLUMN PROCEDURE_CATALOG $(VCHR)"
},
{
"", "", DD_UPDATE_COMMIT,
""
},
{
"SYS_TRIGGERS", "TRIGGER_CATALOG", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_TRIGGERS ADD COLUMN TRIGGER_CATALOG $(VCHR)"
},
{
"", "", DD_UPDATE_COMMIT,
""
},
{
"SYS_EVENTS", "EVENT_CATALOG", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_EVENTS ADD COLUMN EVENT_CATALOG $(VCHR)"
},
{
"", "", DD_UPDATE_COMMIT,
""
},
{
"SYS_FORKEYS", "KEY_CATALOG", DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_FORKEYS ADD COLUMN KEY_CATALOG $(VCHR)"
},
{
"", "", DD_UPDATE_COMMIT,
""
},
{
RS_RELNAME_USERS, RS_ANAME_USERS_LOGIN_CATALOG , DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_USERS ADD COLUMN LOGIN_CATALOG $(VCHR)"
},
{
"", "", DD_UPDATE_COMMIT,
""
},

#endif /* SS_SYNC */

{
"", "", DD_UPDATE_COMMIT,
""
},

/* Create new table if it does not exist. */
{
"SYS_BLOBS", NULL, DD_UPDATE_CREATETABLE,
tb_blobg2_sysblobs_create_stmts
},
{
"", "", DD_UPDATE_COMMIT,
""
},

    /* Default values.  Create the SYS_COLUMNS_AUX table for
       the original default
       and change the type of DEFAULT_VAL to WVARCHAR. */
{
"SYS_COLUMNS_AUX", NULL, DD_UPDATE_CREATETABLE,
"\
 CREATE TABLE SYS_COLUMNS_AUX\
(ID INTEGER NOT NULL, \
 ORIGINAL_DEFAULT $(VCHR),\
 AUTO_INC_SEQ_ID INTEGER, \
 EXTERNAL_DATA_TYPE INTEGER, \
 EXTERNAL_COLLATION $(VCHR), \
 PRIMARY KEY(ID))@\
 COMMIT WORK@\
"
},

{
"", "", DD_UPDATE_COMMIT,
""
},

{
RS_RELNAME_COLUMNS_AUX, RS_ANAME_COLUMNS_AUX_EXTERNAL_DATA_TYPE, DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_COLUMNS_AUX ADD COLUMN EXTERNAL_DATA_TYPE INTEGER"
},

{
"", "", DD_UPDATE_COMMIT,
""
},

#ifdef SS_COLLATION
{
RS_RELNAME_KEYPARTS_AUX, NULL, DD_UPDATE_CREATETABLE,
"CREATE TABLE " RS_RELNAME_KEYPARTS_AUX "("
 RS_ANAME_KEYPARTS_AUX_ID " INTEGER NOT NULL,"
 RS_ANAME_KEYPARTS_AUX_KEYP_NO " INTEGER NOT NULL,"
 RS_ANAME_KEYPARTS_AUX_PREFIX_LENGTH " INTEGER,"
"PRIMARY KEY (" RS_ANAME_KEYPARTS_AUX_ID ","
                RS_ANAME_KEYPARTS_AUX_KEYP_NO "))@"
"COMMIT WORK@"
},
{
RS_RELNAME_COLUMNS_AUX, RS_ANAME_COLUMNS_AUX_EXTERNAL_COLLATION, DD_UPDATE_ADDCOLUMN,
"ALTER TABLE \"_SYSTEM\".SYS_COLUMNS_AUX ADD COLUMN EXTERNAL_COLLATION $(VCHR)"
},
#endif /* SS_COLLATION */

{
"", "", DD_UPDATE_COMMIT,
""
},

#ifdef SS_COLLATION
{
RS_RELNAME_COLLATIONS, NULL, DD_UPDATE_CREATETABLE,
"\
CREATE TABLE SYS_COLLATIONS (\
  ID INTEGER NOT NULL,\
  CHARSET_NAME $(VCHR) NOT NULL,\
  COLLATION_NAME $(VCHR) NOT NULL,\
  REMARKS $(VCHR),\
  PRIMARY KEY(ID))@\
COMMIT WORK@\
"
},
#endif /* SS_COLLATION */
    
#ifdef SS_MYSQL_AC
{
"SYS_MYFILES", NULL, DD_UPDATE_CREATETABLE,
"\
CREATE TABLE SYS_MYFILES (\
    TYPE INTEGER,\
    SSCHEMA char(128),\
    SNAME char(128),\
    FNAME char(128),\
    DATA LONG VARBINARY,\
    PRIMARY KEY(SSCHEMA, SNAME, TYPE))@\
 COMMIT WORK@\
"
},
#endif

{
NULL,
NULL,
DD_UPDATE_CREATETABLE,
NULL
}

};

static dd_updatestmt_t update_timestamp19[] = {
{
"SYS_TYPES", "", DD_UPDATE_SETVALUE,
"UPDATE SYS_TYPES SET PRECISION = 19 WHERE DATA_TYPE = 11 AND TYPE_NAME = 'TIMESTAMP' AND PRECISION <> 19"
},
{
NULL,
NULL,
DD_UPDATE_CREATETABLE,
NULL
}
};

static char *dd_systypesdata[] = {
dd_systypesdata0,
dd_systypesdata1,
dd_systypesdata2,
dd_systypesdata3,
dd_systypesdata4,
dd_systypesdata5,
dd_systypesdata6,
dd_systypesdata7,
dd_systypesdata8,
dd_systypesdata9,
dd_systypesdata10,
dd_systypesdata11,
dd_systypesdata12,
dd_systypesdata13,
dd_systypesdata14,
dd_systypesdata15,
dd_systypesdata16,
dd_systypesdata17,
dd_systypesdata18,
dd_systypesdata19,
dd_systypesdata20,
NULL
};

struct dd_attr_st {
        char*           attrname;
        rs_atype_t*     atype;
        rs_auth_t*      auth;
        rs_atype_t*     defatype;
        rs_aval_t*      defaval;
        long*           p_newattrid;
        bool            syscall;
};
typedef struct dd_attr_st dd_attr_t;

static void dd_attr_done(
        void* data)
{
        dd_attr_t* attr = data;

        SsMemFree(attr->attrname);
        SsMemFree(attr);
}

su_list_t* tb_dd_attrlist_init(
        void)
{
        su_list_t* attrlist;
        attrlist = su_list_init(dd_attr_done);

        return attrlist;
}

void tb_dd_attrlist_done(
        su_list_t* attrlist)
{
        su_list_done(attrlist);
}

void tb_dd_attrlist_add(
        su_list_t*      attrlist,
        char*           attrname,
        rs_atype_t*     atype,
        rs_auth_t*      auth,
        rs_atype_t*     defatype,
        rs_aval_t*      defaval,
        long*           p_newattrid,
        bool            syscall)
{
        dd_attr_t* attr;

        attr = SsMemAlloc(sizeof(dd_attr_t));
        attr->attrname = SsMemStrdup(attrname);
        attr->atype = atype;
        attr->auth = auth;
        attr->defatype = defatype;
        attr->defaval = defaval;
        attr->p_newattrid = p_newattrid;
        attr->syscall = syscall;

        su_list_insertlast(attrlist, attr);
}

/*#***********************************************************************\
 *
 *          dd_execsql
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      sys -
 *
 *
 *      trans -
 *
 *
 *      sqlstr -
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
static bool dd_execsql(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys __attribute__ ((unused)),
        tb_trans_t* trans,
        char* sqlstr,
        rs_err_t** p_errh)
{
        bool succp = TRUE;
        char* tmp_sqlstr;
        char* new_sqlstr;
        char* tmp;
        bool return_errors = TRUE;
        char buf[RS_MAX_NAMELEN * 3 + 2 + 1]; /* note: in UTF-8 format */

        ss_pprintf_3(("dd_execsql:'%.2048s'\n", sqlstr));

        tmp_sqlstr = SsStrReplaceDup(sqlstr,
                                     DD_SYSVARCHAR_MACRO,
                                     DD_SYSVARCHAR_NAME);
        ss_dassert(tmp_sqlstr != NULL);

        /* Type for sync versions. Note that this type must be in sync
         * with atype created in function rs_atype_initsynctuplevers.
         */
        SsSprintf(buf, "BINARY(%d)", RS_TUPLENUM_ATYPESIZE);
        new_sqlstr = SsStrReplaceDup(
                        tmp_sqlstr,
                        DD_SYNCVERSTYPE_MACRO,
                        buf);
        ss_dassert(new_sqlstr != NULL);
        SsMemFree(tmp_sqlstr);
        {
            char* defcatalog = RS_AVAL_DEFCATALOG;
            if (defcatalog == NULL) {
                strcpy(buf, "NULL");
            } else {
                SsSprintf(buf,
                          "'%s'",
                          defcatalog);
            }
        }
        tmp_sqlstr = new_sqlstr;
        new_sqlstr = SsStrReplaceDup(
                        tmp_sqlstr,
                        DD_SYSDEFCAT_MACRO,
                        buf);
        ss_dassert(new_sqlstr != NULL);
        SsMemFree(tmp_sqlstr);
        {
            char* defcatalog_new = RS_AVAL_DEFCATALOG_NEW;
            if (defcatalog_new == NULL) {
                strcpy(buf, "NULL");
            } else {
                SsSprintf(buf,
                          "'%s'",
                          defcatalog_new);
            }
        }
        tmp_sqlstr = new_sqlstr;
        new_sqlstr = SsStrReplaceDup(
                        tmp_sqlstr,
                        DD_SYSDEFCAT_NEW_MACRO,
                        buf);
        ss_dassert(new_sqlstr != NULL);
        SsMemFree(tmp_sqlstr);
        {
            char now_buf[4+1+2+1+2+1+2+1+2+1+2+1+9+1];
            dt_date_t now;
            ss_debug(bool succp;)

            ss_dassert(strlen("1999-12-12 12:12:12.123456789") + 1
                       == sizeof(now_buf));
            ss_debug(succp=)
                dt_date_setnow(0, &now);
            ss_dassert(succp);
            ss_debug(succp=)
            dt_date_datetoasciiz(&now, NULL, now_buf);
            ss_dassert(succp);
            SsSprintf(buf, "'%s'", now_buf);
            ss_dprintf_1(("dd_execsql: now buffer = %s\n", buf));
        }
        tmp_sqlstr = new_sqlstr;
        new_sqlstr = SsStrReplaceDup(
                        tmp_sqlstr,
                        DD_NOWTIMESTAMP_MACRO,
                        buf);
        ss_dassert(new_sqlstr != NULL);
        SsMemFree(tmp_sqlstr);

        if (new_sqlstr[0] == '-') {
            return_errors = FALSE;
            tmp = new_sqlstr + 1;
        } else {
            tmp = new_sqlstr;
        }

        /* Same SQL string may contain several SQL statements */
        tmp = strtok(tmp, "@");
        while (tmp != NULL) {
            tb_trans_beginif(cd, trans);
            /* succp = sql_execdirect(cd, sys, trans, tmp); */

#ifdef SS_MYSQL
            succp = tb_minisql_execdirect(cd, trans, NULL, tmp, p_errh); 
#else
            succp = tb_sql_execdirect(cd, sys, trans, tmp, p_errh);
#endif /* SS_MYSQL */

            if (!succp) {
                char* errstr;

#ifdef SS_MYSQL
                    errstr = (char *)"Operation failed.";
#else

                if (p_errh == NULL) {
                    uint sqlerrcode = 0;
                    char* sqlerrstr;
                    uint dmerrcode = 0;
                    char* dmerrstr;
                    sql_errorinfo(cd, sys, &sqlerrcode, &sqlerrstr, &dmerrcode, &dmerrstr);
                    errstr = dmerrcode == 0 ? sqlerrstr : dmerrstr;
                } else {
                    tb_sqls_builderrh(cd, sys, p_errh);
                    errstr = rs_error_geterrstr(cd, *p_errh);
                }
#endif /* SS_MYSQL */
                ss_pprintf_1(("dd_execsql:%s\n%s\n", tmp, errstr));
                SsLogErrorMessage(errstr);
                if (return_errors) {
                    ss_derror;
                }
                break;
            }
            tmp = strtok(NULL, "@");
        }
        SsMemFree(new_sqlstr);
        if (return_errors) {
            return(succp);
        } else {
            return(TRUE);
        }
}

/*#***********************************************************************\
 *
 *              dd_execsqlinstmt
 *
 * Executes SQL statement inside a statement.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      sys -
 *
 *
 *      trans -
 *
 *
 *      sqlstr -
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
static void dd_execsqlinstmt(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys,
        tb_trans_t* trans,
        char* sqlstr)
{
        bool finished;
        bool succp;
        rs_err_t* err = NULL;

        ss_dprintf_3(("dd_execsqlinstmt\n"));

        tb_trans_stmt_begin(cd, trans);

        succp = dd_execsql(cd, sys, trans, sqlstr, NULL);
        ss_assert(succp);

        do {
            succp = tb_trans_stmt_commit(cd, trans, &finished, &err);
        } while (!finished);
        if (!succp) {
            ss_dprintf_1(("dd_execsqlinstmt\n  %s\n  %s\n", sqlstr, rs_error_geterrstr(cd, err)));
            SsLogErrorMessage(rs_error_geterrstr(cd, err));
        }
        ss_assert(succp);
}

void tb_dd_execsqlinstmt(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys,
        tb_trans_t* trans,
        char* sqlstr)
{
        dd_execsqlinstmt(cd, sys, trans, sqlstr);
}

/*#***********************************************************************\
 *
 *              dd_renametable
 *
 * Renames a table.
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      oldname -
 *
 *
 *      newname -
 *
 *  synchist_relid - relid of the table the history table is for
 *
 *  hassynchistkeys - if true, rename also synchist keys
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool dd_renametable(
        TliConnectT* tcon,
        rs_entname_t* oldentname,
        char* newname,
        char* newschema)
{
        bool succp = TRUE;
        TliCursorT* tcur;
        TliRetT trc;
        char* tablename;
        char* tableschema;
        char* catalogname;
        ulong relid;

        ss_dprintf_3(("dd_renametable:'%s' to '%s'\n", rs_entname_getname(oldentname), newname));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_assert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &tablename);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        if (newschema != NULL) {
            trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &tableschema);
            ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        }

        trc = TliCursorColLong(
                tcur,
                (char *)RS_ANAME_TABLES_ID,
                (long*)&relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_TABLES_NAME,
                TLI_RELOP_EQUAL,
                rs_entname_getname(oldentname));
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_TABLES_SCHEMA,
                TLI_RELOP_EQUAL,
                rs_entname_getschema(oldentname));
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        catalogname = rs_entname_getcatalog(oldentname);
        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_TABLES_CATALOG,
                catalogname == NULL ? TLI_RELOP_ISNULL : TLI_RELOP_EQUAL,
                catalogname == NULL ? (char *)"" : catalogname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        if (trc == TLI_RC_SUCC) {

            tablename = newname;
            tableschema = newschema;

            trc = TliCursorUpdate(tcur);
            succp = (trc == TLI_RC_SUCC);
            ss_rc_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        } else {
            ss_rc_assert(trc == TLI_RC_END || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
            succp = FALSE;
        }

        TliCursorFree(tcur);

        return(succp);
}


/*##**********************************************************************\
 *
 *              tb_dd_renametable_ex
 *
 * Renames a table.
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      trans - in, use
 *
 *
 *      tbrelh - in, use
 *              Table to be renamed.
 *
 *      newname - in
 *              New table name.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dd_renametable_ex(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        tb_relh_t* tbrelh,
        char* newname,
        char* newschema,
        su_err_t** p_errh)
{
        bool succp = TRUE;
        bool synchist_succp = TRUE;
        TliConnectT* tcon;
        dbe_ret_t rc;
        rs_entname_t* en;
        rs_entname_t* synchist_en;
        rs_entname_t* relh_en;
        rs_entname_t* synchist_relh_en;
        rs_relh_t* relh;
        rs_relh_t* synchist_relh;
        char buf[RS_MAX_NAMELEN * 3 + 2 + 1];

        if (!tb_relh_ispriv(cd, tbrelh, TB_PRIV_CREATOR)) {
            return(FALSE);
        }

        relh = tb_relh_rsrelh(cd, tbrelh);
        relh_en = rs_relh_entname(cd, relh);

        if (newschema != NULL && !tb_schema_find(cd, trans, newschema, rs_entname_getcatalog(relh_en))) {
            rs_error_create(p_errh, E_SCHEMANOTEXIST_S, newschema);
            return(FALSE);
        }

        en = rs_entname_init(
                rs_entname_getcatalog(relh_en),
                newschema == NULL ? rs_entname_getschema(relh_en) : newschema,
                newname);

        tcon = TliConnectInitByTrans(cd, trans);

        succp = dd_renametable(
            tcon,
            relh_en,
            newname,
            newschema);

        if (!succp) {
            rs_error_create(p_errh, E_RELNOTEXIST_S, rs_relh_name(cd, relh));
            TliConnectDone(tcon);
            rs_entname_done(en);
            return(FALSE);
        }

        rc = dbe_trx_renamerel(
                tb_trans_dbtrx(cd, trans),
                relh,
                en);

        if (rc != SU_SUCCESS) {
            rs_error_create(p_errh, rc);
            succp = FALSE;
        }

        TliConnectDone(tcon);
        rs_entname_done(en);

        if (succp && rs_relh_issync(cd, relh)) {
            /* and rename also the sync history table, if one is found */
            synchist_relh = rs_relh_getsyncrelh(cd, relh);
            synchist_relh_en = rs_relh_entname(cd, synchist_relh);

            SsSprintf(buf, "_SYNCHIST_%s", newname);
            synchist_en = rs_entname_init(
                rs_entname_getcatalog(synchist_relh_en),
                rs_entname_getschema(synchist_relh_en),
                buf);

            tcon = TliConnectInitByTrans(cd, trans);

            synchist_succp = dd_renametable(
                tcon,
                synchist_relh_en,
                buf,
                NULL);

            if (!synchist_succp) {
                rs_error_create(p_errh, E_RELNOTEXIST_S, rs_relh_name(cd, synchist_relh));
                TliConnectDone(tcon);
                rs_entname_done(synchist_en);
                return(FALSE);
            }

            rc = dbe_trx_renamerel(
                tb_trans_dbtrx(cd, trans),
                synchist_relh,
                synchist_en);

            if (rc != SU_SUCCESS) {
                rs_error_create(p_errh, rc);
                synchist_succp = FALSE;
                succp = FALSE;
            }

            TliConnectDone(tcon);
            rs_entname_done(synchist_en);
        }

        return(succp);
}

bool tb_dd_renametable(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        tb_relh_t* tbrelh,
        char* newname,
        su_err_t** p_errh)
{
        bool b;

        b = tb_dd_renametable_ex(
                cd,
                trans,
                tbrelh,
                newname,
                NULL,
                p_errh);
        return(b);
}

static void dd_newidinforefkeys(
        TliConnectT*    tcon,
        rs_relh_t*      relh,
        long            old_keyid,
        long            new_keyid)
{
        TliCursorT*     tcur;
        TliRetT         trc;
        rs_sysinfo_t*   cd = TliGetCd(tcon);
        long            id;

        ss_dprintf_3(("dd_newidinforefkeys\n"));

        if (rs_relh_issysrel(cd, relh)) {
            /* System tables have no foreign keys.
             */
            return;
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_KEY_ID, &id);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REF_KEY_ID,
                TLI_RELOP_EQUAL,
                old_keyid);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            ss_dassert(id == old_keyid);
            id = new_keyid;
            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

#ifdef SS_COLLATION

static void dd_newidintokeyparts_aux(
        TliConnectT* tcon,
        long old_keyid,
        long new_keyid)
{
        long keyid;
        TliCursorT* tcur;
        TliRetT trc;
        ss_dprintf_3(("dd_newidintokeyparts_aux\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS_AUX);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_AUX_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYPARTS_ID,
                TLI_RELOP_EQUAL,
                old_keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            ss_dassert(keyid == old_keyid);
            keyid = new_keyid;
            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_rc_dassert(trc == TLI_RC_END, trc);
        TliCursorFree(tcur);
}
         
#endif /* SS_COLLATION */

/*#***********************************************************************\
 *
 *              dd_newidintokeyparts
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      old_keyid -
 *
 *
 *      new_keyid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_newidintokeyparts(
        TliConnectT* tcon,
        long old_keyid,
        long new_keyid)
{
        TliCursorT* tcur;
        TliRetT trc;
        int kptype;
        va_t* constvalue;
        va_t keyid_va;
        va_t old_keyid_va;
        rs_sysinfo_t* cd __attribute__ ((unused));
        long  old_keyid_old = old_keyid;
        int   refkeyid;

        cd = TliGetCd(tcon);

        ss_dprintf_3(("dd_newidintokeyparts\n"));

#ifdef SS_COLLATION
        dd_newidintokeyparts_aux(tcon, old_keyid, new_keyid);
#endif /* SS_COLLATION */

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ID, &old_keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_ATTR_TYPE, &kptype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColVa(tcur, RS_ANAME_KEYPARTS_CONST_VALUE, &constvalue);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYPARTS_ID,
                TLI_RELOP_EQUAL,
                old_keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        va_setlong(&keyid_va, new_keyid);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            old_keyid = new_keyid;
            if (constvalue != NULL) {
                switch (kptype) {
                    case RSAT_KEY_ID:
                        constvalue = &keyid_va;
                        break;
                    default:
                        break;
                }
            }

            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYPARTS);

        ss_dassert(tcur != NULL);
        trc = TliCursorColVa(tcur, RS_ANAME_FORKEYPARTS_CONST_VALUE, &constvalue);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_ID, &refkeyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        va_setlong(&old_keyid_va, old_keyid_old);

        trc = TliCursorConstrVa(
                tcur,
                RS_ANAME_FORKEYPARTS_CONST_VALUE,
                TLI_RELOP_EQUAL,
                &old_keyid_va);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYPARTS_KEYP_NO,
                TLI_RELOP_EQUAL,
                0);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYPARTS_ATTR_TYPE,
                TLI_RELOP_EQUAL,
                RSAT_KEY_ID);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        va_setlong(&keyid_va, new_keyid);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            constvalue = &keyid_va;
            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(
tcur));
            constvalue = &old_keyid_va;
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_newidintokeys
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_newidintokeys(
        TliConnectT* tcon,
        rs_relh_t* relh)
{
        TliCursorT* tcur;
        TliRetT trc;
        long old_keyid;
        long new_keyid;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        rs_rbuf_t* rbuf = rs_sysi_rbuf(cd);

        ss_dprintf_3(("dd_newidintokeys\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, &old_keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYS_REL_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            long i = 0;
            rs_key_t* key;

            new_keyid = dbe_db_getnewkeyid_log(rs_sysi_db(cd));
            ss_dprintf_4(("dd_newidintokeys:old_keyid=%ld, new_keyid=%ld\n", old_keyid, new_keyid));
            dd_newidintokeyparts(tcon, old_keyid, new_keyid);
            dd_newidinforefkeys(tcon, relh, old_keyid, new_keyid);

            su_pa_do_get(rs_relh_refkeys(cd, relh), i, key) {
                long relid = rs_key_refrelid(cd, key);
                rs_entname_t* relname;
                bool found;

                found = rs_rbuf_relnamebyid(cd, rbuf, relid, &relname);
                if (found) {
                    ulong check_relid;
                    rs_rbuf_present_t rp;
                    rs_relh_t *upd_relh;

                    rp = rs_rbuf_relpresent(cd, rbuf, relname, &upd_relh, &check_relid);
                    ss_dassert(check_relid == relid);
                    if (rp == RSRBUF_BUFFERED) {
                        rs_key_t* key_upd;
                        int j;

                        ss_dassert(upd_relh != NULL);
                        su_pa_do_get(rs_relh_refkeys(cd, upd_relh), j, key_upd) {
                            va_t* va;

                            ss_dassert(rs_keyp_isconstvalue(cd, key_upd, 0));
                            va = rs_keyp_constvalue(cd, key_upd, 0);
                            if (va_getlong(va) == old_keyid) {
                                va_setlong(va, new_keyid);
                            }
                        }
                        SS_MEM_SETUNLINK(upd_relh);
                        rs_relh_done(cd, upd_relh);
                    }
                    rs_entname_done(relname);
                }
            }

            old_keyid = new_keyid;
            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

static void dd_removetruncatejob(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        TliConnectT* tcon,
        long relid)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* key;
        char keybuf[80];

        ss_dprintf_3(("dd_removetruncatejob:relid=%ld\n", relid));

        tcur = TliCursorCreate(tcon,
                                RS_AVAL_DEFCATALOG,
                                RS_AVAL_SYSNAME,
                                RS_RELNAME_SYSPROPERTIES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_SYSPROPERTY_KEY, &key);
        ss_dassert(trc == TLI_RC_SUCC);

        SsSprintf(keybuf, "TRUNCATE TABLE %d", relid);
        key = keybuf;
        ss_dprintf_4(("dd_removetruncatejob:%s\n", keybuf));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_SYSPROPERTY_KEY,
                TLI_RELOP_EQUAL,
                key);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);
        ss_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        if (trc == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));
        }

        TliCursorFree(tcur);
}

static rs_trfunret_t truncatetable_trend(
        rs_sysi_t* caller_cd,
        sqltrans_t* caller_trans __attribute__ ((unused)),
        rs_trop_t trop,
        void* ctx)
{
        dd_truncatetable_t* ti = ctx;
        rs_trfunret_t trc;
        su_ret_t surc;
        dbe_trx_t* trx;
        bool succp;
        bool finishedp;
        rs_relh_t* relh;

        ss_dprintf_3(("truncatetable_trend\n"));

        switch (trop) {
            case RS_TROP_AFTERROLLBACK:
                ss_dprintf_4(("truncatetable_trend:RS_TROP_AFTERROLLBACK\n"));
                SsMemFree(ti);
                trc = RS_TRFUNRET_REMOVE;
                break;
            case RS_TROP_AFTERCOMMIT:
                ss_dprintf_4(("truncatetable_trend:RS_TROP_AFTERCOMMIT, ti->ti_state=%d\n", ti->ti_state));
                if (ti->ti_state == DD_TRUNCATE_AFTERCOMMIT_INIT) {
                    rs_sysi_t* cd;
                    sqltrans_t* trans;
                    
                    ti->ti_state = DD_TRUNCATE_AFTERCOMMIT_CONT;

                    ti->ti_tbcon = tb_sysconnect_init(rs_sysi_tabdb(caller_cd));
                    tb_sysconnect_transinit(ti->ti_tbcon);
                    
                    cd = tb_getclientdata(ti->ti_tbcon);
                    trans = tb_getsqltrans(ti->ti_tbcon);

                    tb_trans_begintransandstmt(cd, trans);
                    
                    ti->ti_tcon = TliConnectInitByTrans(cd, trans);
                    
                    dd_removetruncatejob(cd, ti->ti_tcon, ti->ti_oldrelid);

                    trx = tb_trans_dbtrx(cd, trans);

                    relh = dd_readrelh(ti->ti_tcon, ti->ti_newrelid, DD_READRELH_KEYS, FALSE, NULL, NULL);
                    ss_dassert(relh != NULL);
                    succp = (relh != NULL);

                    if (succp) {
                        surc = dbe_trx_truncaterel(trx, relh, TRUE);
                        ss_dassert(surc == DBE_RC_SUCC);
                        succp = (surc == DBE_RC_SUCC);
                    }

                    if (succp) {
                        succp = dd_droprelh(ti->ti_tcon, relh, NULL);
                        ss_dassert(succp);
                    }

                    if (relh != NULL) {
                        SS_MEM_SETUNLINK(relh);
                        rs_relh_done(cd, relh);
                    }

                    if (succp) {
                        succp = tb_trans_stmt_commit_onestep(cd, trans, NULL);
                        ss_dassert(succp);
                    } else {
                        succp = tb_trans_stmt_rollback_onestep(cd, trans, NULL);
                        ss_dassert(succp);
                    }

                    dbe_db_setmergedisabled(rs_sysi_db(cd), TRUE);
                }

                succp = tb_trans_commit(
                            tb_getclientdata(ti->ti_tbcon),
                            tb_getsqltrans(ti->ti_tbcon),
                            &finishedp,
                            NULL);
                ss_dassert(succp);
                if (!finishedp) {
                    trc = RS_TRFUNRET_CONT;
                } else {
                    dbe_db_setmergedisabled(rs_sysi_db(tb_getclientdata(ti->ti_tbcon)), FALSE);
                    TliConnectDone(ti->ti_tcon);
                    tb_sysconnect_done(ti->ti_tbcon);
                    SsMemFree(ti);
                    trc = RS_TRFUNRET_REMOVE;
                }
                break;
            default:
                ss_dprintf_4(("truncatetable_trend:trop=%d\n", trop));
                ss_dassert(trop != RS_TROP_AFTERSTMTCOMMIT && trop != RS_TROP_AFTERSTMTROLLBACK);
                trc = RS_TRFUNRET_KEEP;
                break;
        }

        ss_dprintf_4(("truncatetable_trend:trc=%d\n", trc));

        return (trc);
}

static rs_trfunret_t truncatetable_stmttrend(
        rs_sysi_t* cd,
        sqltrans_t* trans __attribute__ ((unused)),
        rs_trop_t trop,
        void* ctx)
{
        dd_truncatetable_t* ti = ctx;
        rs_trfunret_t trc;

        ss_dprintf_3(("truncatetable_stmttrend\n"));

        switch (trop) {
            case RS_TROP_AFTERSTMTCOMMIT:
                ss_dprintf_4(("truncatetable_stmttrend:RS_TROP_AFTERSTMTCOMMIT\n"));
                rs_trend_addfun_first(
                    rs_sysi_gettrend(cd),
                    NULL, /* callback does not need transaction */
                    truncatetable_trend,
                    ti);
                trc = RS_TRFUNRET_REMOVE;
                break;
            case RS_TROP_AFTERSTMTROLLBACK:
                ss_dprintf_4(("truncatetable_stmttrend:RS_TROP_AFTERSTMTROLLBACK\n"));
                SsMemFree(ti);
                trc = RS_TRFUNRET_REMOVE;
                break;
            default:
                ss_dprintf_4(("truncatetable_stmttrend:trop=%d\n", trop));
                ss_dassert(trop != RS_TROP_AFTERCOMMIT && trop != RS_TROP_AFTERROLLBACK);
                trc = RS_TRFUNRET_KEEP;
                break;
        }

        ss_dprintf_4(("truncatetable_stmttrend:trc=%d\n", trc));

        return (trc);
}

static void dd_addtruncatejob(
        rs_sysinfo_t* cd,
        TliConnectT* tcon,
        rs_relh_t* relh,
        long newrelid)
{
        TliCursorT* tcur;
        TliRetT trc;
        dt_date_t date;
        char* keyptr;
        char keybuf[80];
        char* valueptr;
        char valuebuf[80];
        dd_truncatetable_t* ti;

        ss_dprintf_3(("dd_addtruncatejob\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_SYSPROPERTIES);
        if (tcur == NULL) {
            ss_dprintf_4(("tb_sysproperties_update:table %s does not exist\n", RS_RELNAME_SYSPROPERTIES));
            ss_derror;
            return;
        }

        trc = TliCursorColStr(tcur, RS_ANAME_SYSPROPERTY_KEY, &keyptr);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_SYSPROPERTY_VALUE, &valueptr);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColDate(tcur, RS_ANAME_SYSPROPERTY_MODTIME, &date);
        ss_dassert(trc == TLI_RC_SUCC);

        date = tb_dd_curdate();

        SsSprintf(keybuf, "TRUNCATE TABLE %d", rs_relh_relid(cd, relh));
        SsSprintf(valuebuf, "%d", newrelid);
        ss_dprintf_4(("dd_addtruncatejob:%s %s\n", keybuf, valuebuf));
        keyptr = keybuf;
        valueptr = valuebuf;

        trc = TliCursorInsert(tcur);
        ss_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        ti = SSMEM_NEW(dd_truncatetable_t);
        ti->ti_state = DD_TRUNCATE_AFTERCOMMIT_INIT;
        ti->ti_oldrelid = rs_relh_relid(cd, relh);
        ti->ti_newrelid = newrelid;
        ti->ti_tcon = NULL;

        rs_trend_addstmtfun(
            rs_sysi_getstmttrend(cd),
            TliGetTrans(tcon),
            truncatetable_stmttrend,
            ti);

        TliCursorFree(tcur);
}

bool tb_dd_truncaterel(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname,
        bool issyncrel,
        rs_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        rs_relh_t* relh;
        tb_relpriv_t* priv;
        rs_auth_t* auth;
        dbe_trx_t* trx;
        bool succp = TRUE;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relname != NULL);

        if (!tb_dd_checkobjectname(rs_entname_getname(relname))) {
            rs_error_create(p_errh, E_RELNOTEXIST_S,"");
            return(FALSE);
        }

        ss_dprintf_1(("tb_dd_truncaterel'%s.%s'\n",
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));

        relh = tb_dd_getrelh(cd, trans, relname, &priv, p_errh);
        if (relh == NULL) {
            return(FALSE);
        }

        auth = rs_sysi_auth(cd);
        relname = rs_relh_entname(cd, relh);

        trx = tb_trans_dbtrx(cd, trans);

        if (!issyncrel && !tb_priv_isrelpriv(cd, priv, TB_PRIV_DELETE, rs_relh_issysrel(cd, relh))) {
            succp = FALSE;
            rs_error_create(p_errh, E_NOPRIV);

        } else {
            bool isforkeyref = FALSE;
            TliConnectT* tcon;

            tcon = TliConnectInitByTrans(cd, trans);

            succp = dd_isforkeyref(tcon, relh, &isforkeyref, p_errh);

            if (succp) {
                if (isforkeyref) {
                    rs_error_create(p_errh, E_FORKEYREFEXIST_S, rs_relh_name(cd, relh));
                    succp = FALSE;
                } else {
                    rs_entname_t* newname;
                    char* name;
                    char* dynname;
                    long newrelid;
                    tb_relmodemap_t relmodes;

                    name = rs_entname_getname(relname);
                    dynname = SsMemAlloc(strlen("$TRUNCATE") + strlen(name) + 2);
                    strcpy(dynname, "$TRUNCATE");
                    strcat(dynname, " ");
                    strcat(dynname, name);
                    
                    newname = rs_entname_init(
                                rs_entname_getcatalog(relname), 
                                rs_entname_getschema(relname), 
                                dynname);
                    newrelid = dbe_db_getnewrelid_log(rs_sysi_db(cd));

                    dd_newidintokeys(tcon, relh);
                    dd_addtruncatejob(cd, tcon, relh, newrelid);
                    dd_createrelh(
                        tcon, 
                        relh, 
                        TB_DD_CREATEREL_TRUNCATE, 
                        newname,
                        newrelid);
                    relmodes = 0;
                    if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
                        relmodes |= TB_RELMODE_MAINMEMORY;
                    }
                    if (rs_relh_istransient(cd, relh)) {
                        relmodes |= TB_RELMODE_TRANSIENT;
                    }
                    if (rs_relh_isglobaltemporary(cd, relh)) {
                        relmodes |= TB_RELMODE_GLOBALTEMPORARY;
                    }
                    if (relmodes != 0) {
                        succp = dd_setrelmodes(
                                    cd,
                                    tcon,
                                    relh,
                                    newrelid,
                                    relmodes,
                                    TRUE,
                                    p_errh);
                    }
                    if (succp) {
                        rc = dbe_trx_truncaterel(trx, relh, FALSE);
                        if (rc != DBE_RC_SUCC) {
                            ss_dprintf_2(("tb_dd_truncaterel:dbe_trx_truncaterel rc=%d\n", rc));
                            rs_error_create(p_errh, rc, "");
                            succp = FALSE;
                        }
                    }
                    rs_entname_done(newname);
                    SsMemFree(dynname);
                }
            }
            TliConnectDone(tcon);
        }

        SS_MEM_SETUNLINK(relh);
        rs_relh_done(cd, relh);

        ss_dprintf_2(("tb_dd_truncaterel:succp=%d\n", succp));

        return(succp);
}


/*##**********************************************************************\
 *
 *              tb_dd_updatesysrelschemakeys
 *
 * Updates some system table unique keys to contain also a schema name.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      sys -
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
bool tb_dd_updatesysrelschemakeys(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char keyname[255];
        char* sqlstr;
        tb_trans_t* trans;
        long keyid;
        char* p;
        rs_entname_t en;

        ss_dprintf_1(("dd_updatesysrelschemakeys\n"));

        tcon = TliConnectInit(cd);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, &keyid);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        SsSprintf(keyname, "$%s_UNQKEY_%u", RS_RELNAME_PROCEDURES, 0);

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                keyname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_assert(trc == TLI_RC_SUCC || trc == TLI_RC_END);

        TliCursorFree(tcur);

        if (trc != TLI_RC_SUCC) {
            /* New format database. */
            ss_dprintf_2(("dd_updatesysrelschemakeys:new format database, no changes\n"));
            TliConnectDone(tcon);
            return(FALSE);
        }

        sqlstr = SsMemAlloc(strlen(dd_create_table_sys_sequences) + 255);
        trans = TliGetTrans(tcon);

        /* SYS_PROCEDURES:
         * Change UNIQUE(PROCEDURE_NAME) to
         * UNIQUE(PROCEDURE_NAME,PROCEDURE_SCHEMA)
         */
        ss_dprintf_2(("dd_updatesysrelschemakeys:SYS_PROCEDURES\n"));
        SsSprintf(keyname, "$%s_UNQKEY_%u", RS_RELNAME_PROCEDURES, 0);
        SsSprintf(sqlstr, "DROP INDEX \"%s\"", keyname);
        dd_execsqlinstmt(cd, sys, trans, sqlstr);

        SsSprintf(keyname, RSK_OLD_UNQKEYSTR, RS_AVAL_SYSNAME, RS_RELNAME_PROCEDURES, 0);
        SsSprintf(sqlstr, "CREATE UNIQUE INDEX \"%s\" ON SYS_PROCEDURES(PROCEDURE_NAME,PROCEDURE_SCHEMA)", keyname);
        dd_execsqlinstmt(cd, sys, trans, sqlstr);

        /* SYS_EVENTS:
         * Change UNIQUE(EVENT_NAME) to
         * UNIQUE(EVENT_NAME,EVENT_SCHEMA)
         */
        ss_dprintf_2(("dd_updatesysrelschemakeys:SYS_EVENTS\n"));
        SsSprintf(keyname, "$%s_UNQKEY_%u", RS_RELNAME_EVENTS, 0);
        SsSprintf(sqlstr, "DROP INDEX \"%s\"", keyname);
        dd_execsqlinstmt(cd, sys, trans, sqlstr);

        SsSprintf(keyname, RSK_OLD_UNQKEYSTR, RS_AVAL_SYSNAME, RS_RELNAME_EVENTS, 0);
        SsSprintf(sqlstr, "CREATE UNIQUE INDEX \"%s\" ON SYS_EVENTS(EVENT_NAME,EVENT_SCHEMA)", keyname);
        dd_execsqlinstmt(cd, sys, trans, sqlstr);

        /* SYS_SEQUENCES:
         * Old table had PRIMARY KEY(SEQUENCE_NAME), so we have to do the
         * following steps:
         *   1) create a new table with different name SYS_SEQUENCES2
         *   2) copy data from old table to new table
         *   3) drop old table
         *   4) rename table SYS_SEQUENCES2 to SYS_SEQUENCES
         */
        ss_dprintf_2(("dd_updatesysrelschemakeys:SYS_SEQUENCES\n"));
        /* step 1 */
        strcpy(sqlstr, dd_create_table_sys_sequences);
        p = strchr(sqlstr, '(');
        ss_assert(p != NULL);
        p[-1] = '2';
        dd_execsqlinstmt(cd, sys, trans, sqlstr);

        /* step 2 */
        dd_execsqlinstmt(cd, sys, trans,
                         (char *)"INSERT INTO SYS_SEQUENCES2 SELECT * FROM SYS_SEQUENCES");

        /* step 3 */
        dd_execsqlinstmt(cd, sys, trans, (char *)"DROP TABLE SYS_SEQUENCES");

        /* step 4 (need to be done using TLI) */
        rs_entname_initbuf(
            &en,
            RS_AVAL_DEFCATALOG,
            RS_AVAL_SYSNAME,
            "SYS_SEQUENCES2");

        dd_renametable(tcon, &en, (char *)RS_RELNAME_SEQUENCES, NULL);

        trc = TliCommit(tcon);
        ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        TliConnectDone(tcon);

        SsMemFree(sqlstr);
        ss_dprintf_2(("dd_updatesysrelschemakeys:success\n"));

        return(TRUE);
}

/*##**********************************************************************\
 *
 *              tb_dd_createsyskeysschemakey
 *
 * Creates a new key to SYS_KEYS system table for (NAME, SCHEMA) and
 * changes existing key (NAME) to (NAME,REL_ID).
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      sys -
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
bool tb_dd_createsyskeysschemakey(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys __attribute__ ((unused)))
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        tb_trans_t* trans;
        dbe_trx_t* trx;
        rs_relh_t* relh;
        rs_key_t* key;
        long nref;
        rs_entname_t en;
        bool succp;
        su_ret_t rc;
        char* newkeyname = (char *)"SYS_KEY_KEYS_NAMESCHEMA";
        char* oldkeyname = (char *)"SYS_KEY_KEYS_NAME";

        ss_dprintf_1(("tb_dd_createsyskeysschemakey\n"));

        tcon = TliConnectInit(cd);

        /* Try to find the new key name from system tables.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_NREF, &nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                newkeyname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_assert(trc == TLI_RC_SUCC || trc == TLI_RC_END);

        TliCursorFree(tcur);

        if (trc != TLI_RC_END) {
            /* New format database. */
            ss_dprintf_2(("tb_dd_createsyskeysschemakey:new format database, no changes\n"));
            TliConnectDone(tcon);
            return(FALSE);
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_NREF, &nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                (char *)"SYS_KEY_KEYS_NAMESCHEMACATALOG");
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_assert(trc == TLI_RC_SUCC || trc == TLI_RC_END);

        TliCursorFree(tcur);

        if (trc != TLI_RC_END) {
            /* New format database. */
            ss_dprintf_2(("tb_dd_createsyskeysschemakey:new format database, no changes\n"));
            TliConnectDone(tcon);
            return(FALSE);
        }

        /* Take the key info for the new key away from relh while
         * we do system table updates.
         */
        rs_entname_initbuf(&en,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_KEYS);
        relh = tb_dd_getrelh(cd, TliGetTrans(tcon), &en, NULL, NULL);
        ss_assert(relh != NULL);
        rs_entname_initbuf(&en,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           newkeyname);
        key = rs_relh_takekeybyname(cd, relh, &en);
        ss_assert(key != NULL);

        /* Find old key name nref from index to increment it.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_NREF, &nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                oldkeyname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), trc);
        ss_rc_assert(nref == 2 || TliTransIsFailed(tcon), nref);

        nref++;
        trc = TliCursorUpdate(tcur);
        ss_bassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        TliCursorFree(tcur);

        /* Create the new key. The key definition is already in system relh.
         */
        trans = TliGetTrans(tcon);

        trx = tb_trans_dbtrx(cd, trans);

        rs_key_link(cd, key);

        rc = tb_dd_createindex(
                cd,
                trans,
                relh,
                rs_relh_ttype(cd, relh),
                key,
                rs_sysi_auth(cd),
                NULL,
                TB_DD_CREATEREL_SYSTEM,
                NULL);
        su_rc_assert(rc == SU_SUCCESS, rc);

        trc = TliCommit(tcon);
        su_rc_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliErrorCode(tcon));

        TliConnectDone(tcon);

        succp = rs_relh_insertkey(cd, relh, key);
        ss_assert(succp);
        SS_MEM_SETUNLINK(relh);
        rs_relh_done(cd, relh);

        rs_relh_setnoddopactive(cd, relh);
        ss_assert(!rs_relh_isddopactive(cd, relh));

        ss_dprintf_2(("tb_dd_createsyskeysschemakey:success\n"));

        return(TRUE);
}


/*##**********************************************************************\
 *
 *              tb_dd_createsyskeyscatalogkey
 *
 * Creates a new key to SYS_KEYS system table for (NAME, SCHEMA, CATALOG) and
 * remove existing key (NAME,SCHEMA). Rows of old key (NAME,SCHEMA) are left
 * as garbage to the database, only the key info is removed from system
 * table.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      sys -
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
bool tb_dd_createsyskeyscatalogkey(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys __attribute__ ((unused)))
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        tb_trans_t* trans;
        dbe_trx_t* trx;
        rs_relh_t* relh;
        rs_key_t* key;
        long nref;
        rs_entname_t en;
        bool succp;
        su_ret_t rc;
        rs_ttype_t* ttype;
        rs_atype_t* atype;
        su_list_t* attrlist;
        char* newkeyname = (char *)"SYS_KEY_KEYS_NAMESCHEMACATALOG";
        char* oldkeyname = (char *)"SYS_KEY_KEYS_NAME";

        ss_dprintf_1(("tb_dd_createsyskeyscatalogkey\n"));

        tcon = TliConnectInit(cd);

        trans = TliGetTrans(tcon);

        /* Try to find the new key name from system tables.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_NREF, &nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                newkeyname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_assert(trc == TLI_RC_SUCC || trc == TLI_RC_END);

        TliCursorFree(tcur);

        if (trc == TLI_RC_SUCC) {
            /* New format database. */
            ss_dprintf_2(("tb_dd_createsyskeyscatalogkey:new format database, no changes\n"));
            TliConnectDone(tcon);
            return(FALSE);
        }

        /* Take the key info for the new key away from relh while
         * we do system table updates.
         */
        rs_entname_initbuf(&en,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_KEYS);
        relh = tb_dd_getrelh(cd, TliGetTrans(tcon), &en, NULL, NULL);
        ss_assert(relh != NULL);
        ss_assert(!rs_relh_isddopactive(cd, relh));

        rs_entname_initbuf(&en,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           newkeyname);
        key = rs_relh_takekeybyname(cd, relh, &en);
        ss_assert(key != NULL);

        tb_trans_stmt_begin(cd, trans);

        /* Delete old key information from system tables.
         * We leave the old key info as garbage to the database.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_NREF, &nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                oldkeyname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_bassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        succp = tb_trans_stmt_commit_onestep(cd, trans, NULL);
        ss_assert(succp);

        /* Add new column info to the system table.
         */

        tb_trans_stmt_begin(cd, trans);

        attrlist = tb_dd_attrlist_init();

        ttype = rs_relh_ttype(cd, relh);
        atype = rs_ttype_atype(
                    cd,
                    ttype,
                    rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_KEYS_CATALOG));

        tb_dd_attrlist_add(
                attrlist,
                (char *)RS_ANAME_KEYS_CATALOG,
                atype,
                rs_sysi_auth(cd),
                NULL,
                NULL,
                NULL,
                TRUE);

        rc = tb_dd_addattributelist(
                cd,
                trans,
                relh,
                attrlist,
                TRUE,
                NULL);
        ss_rc_assert(rc == SU_SUCCESS, rc);

        tb_dd_attrlist_done(attrlist);

        succp = tb_trans_stmt_commit_onestep(cd, trans, NULL);
        ss_assert(succp);

        rs_relh_setnoddopactive(cd, relh);
        ss_assert(!rs_relh_isddopactive(cd, relh));

        /* Create the new key. The key definition is already in system relh.
         */

        tb_trans_stmt_begin(cd, trans);

        trx = tb_trans_dbtrx(cd, trans);

        rs_key_link(cd, key);

        rc = tb_dd_createindex(
                cd,
                trans,
                relh,
                rs_relh_ttype(cd, relh),
                key,
                rs_sysi_auth(cd),
                NULL,
                TB_DD_CREATEREL_SYSTEM,
                NULL);
        su_rc_assert(rc == SU_SUCCESS, rc);

        succp = tb_trans_stmt_commit_onestep(cd, trans, NULL);
        ss_assert(succp);

        trc = TliCommit(tcon);
        su_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        TliConnectDone(tcon);

        succp = rs_relh_insertkey(cd, relh, key);
        ss_assert(succp);

        rs_relh_setnoddopactive(cd, relh);
        ss_assert(!rs_relh_isddopactive(cd, relh));

        ss_dprintf_2(("tb_dd_createsyskeyscatalogkey:success\n"));

        return(TRUE);
}

/*##**********************************************************************\
 *
 *              tb_dd_createsystablescatalogkey
 *
 * Creates a new key to SYS_TABLES system table for (NAME, SCHEMA, CATALOG)
 * and remove existing key (NAME,SCHEMA). Rows of old key (NAME,SCHEMA) are
 * left as garbage to the database, only the key info is removed from system
 * table.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      sys -
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
bool tb_dd_createsystablescatalogkey(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys __attribute__ ((unused)))
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        tb_trans_t* trans;
        dbe_trx_t* trx;
        rs_relh_t* relh;
        rs_key_t* key;
        long nref;
        rs_entname_t en;
        bool succp;
        su_ret_t rc;
        char* newkeyname = (char *)"SYS_KEY_TABLES_NAMESCHEMACATALOG";
        char* oldkeyname = (char *)"SYS_KEY_TABLES_NAME";

        ss_dprintf_1(("tb_dd_createsystablescatalogkey\n"));

        tcon = TliConnectInit(cd);

        trans = TliGetTrans(tcon);

        /* Try to find the new key name from system tables.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_NREF, &nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                newkeyname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_assert(trc == TLI_RC_SUCC || trc == TLI_RC_END);

        TliCursorFree(tcur);

        if (trc == TLI_RC_SUCC) {
            /* New format database. */
            ss_dprintf_2(("tb_dd_createsystablescatalogkey:new format database, no changes\n"));
            TliConnectDone(tcon);
            return(FALSE);
        }

        /* Take the key info for the new key away from relh while
         * we do system table updates.
         */
        rs_entname_initbuf(&en,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_TABLES);
        relh = tb_dd_getrelh(cd, TliGetTrans(tcon), &en, NULL, NULL);
        ss_assert(relh != NULL);
        ss_assert(!rs_relh_isddopactive(cd, relh));

        rs_entname_initbuf(&en,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           newkeyname);
        key = rs_relh_takekeybyname(cd, relh, &en);
        ss_assert(key != NULL);

        tb_trans_stmt_begin(cd, trans);

        /* Delete old key information from system tables.
         * We leave the old key info as garbage to the database.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_NREF, &nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                oldkeyname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_bassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        succp = tb_trans_stmt_commit_onestep(cd, trans, NULL);
        ss_assert(succp);

        /* Create the new key. The key definition is already in system relh.
         */

        tb_trans_stmt_begin(cd, trans);

        trx = tb_trans_dbtrx(cd, trans);

        rs_key_link(cd, key);

        rc = tb_dd_createindex(
                cd,
                trans,
                relh,
                rs_relh_ttype(cd, relh),
                key,
                rs_sysi_auth(cd),
                NULL,
                TB_DD_CREATEREL_SYSTEM,
                NULL);
        su_rc_assert(rc == SU_SUCCESS, rc);

        succp = tb_trans_stmt_commit_onestep(cd, trans, NULL);
        ss_assert(succp);

        trc = TliCommit(tcon);
        su_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        TliConnectDone(tcon);

        succp = rs_relh_insertkey(cd, relh, key);
        ss_assert(succp);

        rs_relh_setnoddopactive(cd, relh);
        ss_assert(!rs_relh_isddopactive(cd, relh));

        ss_dprintf_2(("tb_dd_createsystablescatalogkey:success\n"));

        return(TRUE);
}

#ifndef SS_MYSQL
static bool dd_updatesynchistorykey(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        char* tablename,
        char* tableschema,
        char* tablecatalog)
{
        rs_err_t* errh = NULL;
        rs_relh_t* relh;
        bool succp;
        rs_entname_t en;

        ss_pprintf_3(("dd_updatesynchistorykey:tablename=%s, tableschema=%s, tablecatalog=%s\n",
            tablename, tableschema, tablecatalog != NULL ? tablecatalog : "NULL"));

        tb_trans_stmt_begin(cd, trans);

        rs_entname_initbuf(
            &en,
            tablecatalog,
            tableschema,
            tablename);

        relh = tb_dd_getrelh(cd, trans, &en, NULL, NULL);
        ss_assert(relh != NULL);

        succp = dd_createsynchistoryversionkey(
                    cd,
                    trans,
                    relh,
                    rs_relh_ttype(cd, relh),
                    &errh);

        SS_MEM_SETUNLINK(relh);
        rs_relh_done(cd, relh);

        if (!succp) {
            int errcode;
            errcode = su_err_geterrcode(errh);
            su_rc_assert(errcode == E_KEYNAMEEXIST_S, errcode);
            su_err_done(errh);
        } else {
            succp = tb_trans_stmt_commit_onestep(cd, trans, &errh);
            ss_assert(succp);

        }

        return(succp);
}
#endif /* !SS_MYSQL */


#ifdef SS_MT

typedef struct {
        TliConnectT*    sh_tcon;
        char*           sh_tablename;
        char*           sh_tableschema;
        char*           sh_tablecatalog;
} dd_synchistkey_t;

typedef struct {
        SsSemT*    st_sem;
        bool       st_succp;
        int        st_nthr;
        su_list_t* st_list;
} dd_synchistkey_thrinfo_t;

#ifndef SS_MYSQL
static su_list_t* synchistkey_list;
#endif

/*#***********************************************************************\
 *
 *              dd_synchistkey_done
 *
 * Releases memory from dd_synchistkey_t object.
 *
 * Parameters :
 *
 *      sh -
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
#ifndef SS_MYSQL
static void dd_synchistkey_done(dd_synchistkey_t* sh)
{
        SsMemFree(sh->sh_tablename);
        if (sh->sh_tableschema != NULL) {
            SsMemFree(sh->sh_tableschema);
        }
        if (sh->sh_tablecatalog != NULL) {
            SsMemFree(sh->sh_tablecatalog);
        }
        TliConnectDone(sh->sh_tcon);
}
#endif /* !SS_MYSQL */

/*#***********************************************************************\
 *
 *              dd_updatesynchistorykey_parallel
 *
 * Parallel version of sync history key conversion. If variable
 * tb_dd_syncconvertthreads is set to non-zero from server command line,
 * uses that many threads to create new sync indixes to user tables.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      tablename -
 *
 *
 *      tableschema -
 *
 *
 *      tablecatalog -
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
#ifndef SS_MYSQL
static bool dd_updatesynchistorykey_parallel(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        char* tablename,
        char* tableschema,
        char* tablecatalog)
{
        dd_synchistkey_t* sh;

        ss_dprintf_3(("dd_updatesynchistorykey_parallel\n"));

        if (tb_dd_syncconvertthreads == 0) {
            return(dd_updatesynchistorykey(
                    cd,
                    trans,
                    tablename,
                    tableschema,
                    tablecatalog));
        } else {
            if (synchistkey_list == NULL) {
                synchistkey_list = su_list_init(NULL);
            }

            sh = SSMEM_NEW(dd_synchistkey_t);

            sh->sh_tcon = TliConnectInit(cd);
            sh->sh_tablename = SsMemStrdup(tablename);
            sh->sh_tableschema = tableschema != NULL ? SsMemStrdup(tableschema) : NULL;
            sh->sh_tablecatalog = tablecatalog != NULL ? SsMemStrdup(tablecatalog) : NULL;

            su_list_insertlast(synchistkey_list, sh);

            return(TRUE);
        }
}
#endif /* !SS_MYSQL */

/*#***********************************************************************\
 *
 *              dd_synchistkey_create_thread
 *
 * Thread to create new sync history keys. Reads create info from a list.
 * Exits automatically when list is empty.
 *
 * Parameters :
 *
 *      prm -
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
#ifndef SS_MYSQL
static void SS_CALLBACK dd_synchistkey_create_thread(void* prm)
{
        dd_synchistkey_thrinfo_t* st = prm;

        ss_dprintf_3(("dd_synchistkey_create_thread\n"));

        while (st->st_succp) {
            rs_sysi_t* cd;
            tb_trans_t* trans;
            dd_synchistkey_t* sh;
            bool succp;

            SsSemEnter(st->st_sem);
            sh = su_list_removefirst(st->st_list);
            SsSemExit(st->st_sem);

            if (sh == NULL) {
                /* End of list */
                break;
            }

            cd = TliGetCd(sh->sh_tcon);
            trans = TliGetTrans(sh->sh_tcon);

            TliRollback(sh->sh_tcon);

            tb_trans_settransoption(cd, trans, TB_TRANSOPT_NOLOGGING);
            tb_trans_settransoption(cd, trans, TB_TRANSOPT_NOCHECK);

            tb_trans_beginif(cd, trans);

#ifdef SS_BETA
            ui_msg_message_nogui(DBE_MSG_CONVERTING_TABLE_S, sh->sh_tablename);
#endif /* SS_BETA */

            succp = dd_updatesynchistorykey(
                        cd,
                        trans,
                        sh->sh_tablename,
                        sh->sh_tableschema,
                        sh->sh_tablecatalog);

            if (succp) {
                succp = (TliCommit(sh->sh_tcon) == TLI_RC_SUCC);
            }

#ifdef SS_BETA
            if (succp) {
                ui_msg_message_nogui(DBE_MSG_TABLE_CONVERTED_S, sh->sh_tablename);
            } else {
                ui_msg_message_nogui(DBE_MSG_NO_NEED_TO_CONVERT_TABLE_S, sh->sh_tablename);
            }
#endif /* SS_BETA */

            dd_synchistkey_done(sh);

            if (!succp) {
                st->st_succp = FALSE;
                break;
            }
        }

        SsSemEnter(st->st_sem);
        st->st_nthr--;
        SsSemExit(st->st_sem);

        SsThrExit();
}
#endif /* !SS_MYSQL */

/*#***********************************************************************\
 *
 *              dd_synchistkey_create_parallel
 *
 * Starts actual parallel sync history key create. Starts threads and
 * waits until all threads have compeleted.
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
#ifndef SS_MYSQL
static bool dd_synchistkey_create_parallel(void)
{
        bool succp = TRUE;
        int nthr = tb_dd_syncconvertthreads;
        dd_synchistkey_t* sh;

        if (nthr == 0) {
            return(TRUE);
        }

        if (su_list_length(synchistkey_list) != 0) {
            int i;
            SsThreadT** threads;
            dd_synchistkey_thrinfo_t st;
            bool done;

            ss_beta(ui_msg_message_nogui(SNC_MSG_START_PARALLEL_SYNC_HISTKEY_CONV));

            st.st_sem = SsSemCreateLocal(SS_SEMNUM_ANONYMOUS_SEM);
            st.st_succp = TRUE;
            st.st_nthr = nthr;
            st.st_list = synchistkey_list;

            threads = SsMemAlloc(sizeof(SsThreadT*) * nthr);
            for (i = 0; i < nthr; i++) {
                threads[i] = SsThrInitParam(
                                dd_synchistkey_create_thread,
                                "dd_synchistkey_create_thread",
                                SS_THREAD_USEDEFAULTSTACKSIZE,
                                &st);
                SsThrEnable(threads[i]);
            }

            do {
                SsThrSleep(1000L);

                SsSemEnter(st.st_sem);
                done = st.st_nthr == 0;
                SsSemExit(st.st_sem);
            } while (!done);

            for (i = 0; i < nthr; i++) {
                SsThrDone(threads[i]);
            }
            SsMemFree(threads);
            SsSemFree(st.st_sem);
            succp = st.st_succp;
        }

        while ((sh = su_list_removefirst(synchistkey_list)) != NULL) {
            dd_synchistkey_done(sh);
        }
        su_list_done(synchistkey_list);
        synchistkey_list = NULL;
        return(succp);
}
#endif /* !SS_MYSQL */

#define dd_updatesynchistorykey dd_updatesynchistorykey_parallel

#endif /* SS_MT */

#ifndef SS_MYSQL
bool tb_dd_updatesynchistorykeys(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        tb_trans_t* trans;
        char* tablename;
        char* tableschema;
        char* tablecatalog;
        bool succp = FALSE;
        bool firstp = TRUE;

        ss_pprintf_1(("tb_dd_updatesynchistorykeys\n"));

        if (!ss_vers_issync()) {
            ss_pprintf_2(("tb_dd_updatesynchistorykeys: no conversion, not sync\n"));
            return(FALSE);
        }

        tcon = TliConnectInit(cd);

        trans = TliGetTrans(tcon);

        /* Scan through all sync tables and update keys if necessary.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_assert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &tablename);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &tableschema);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CATALOG, &tablecatalog);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_TABLES_TYPE,
                TLI_RELOP_EQUAL,
                RS_AVAL_SYNC_TABLE);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            /* Update history table. */
            succp = dd_updatesynchistorykey(
                        cd,
                        trans,
                        tablename,
                        tableschema,
                        tablecatalog);
            if (succp) {
                /* Update base table. */
                ss_beta(ui_msg_message_nogui(SNC_MSG_START_SYNC_HISTKEY_CONV));
                succp = dd_updatesynchistorykey(
                            cd,
                            trans,
                            tablename + 10, /* Skip _SYNCHIST_ */
                            tableschema,
                            tablecatalog);
            }
            if (!succp) {
                ss_assert(firstp);
                break;
            }
            firstp = FALSE;
        }
        ss_assert(trc == TLI_RC_SUCC || trc == TLI_RC_END);

        TliCursorFree(tcur);

        if (succp) {
            trc = TliCommit(tcon);
            su_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
        } else {
            TliRollback(tcon);
        }

#ifdef SS_MT
        if (succp) {
            succp = dd_synchistkey_create_parallel();
        }
#endif /* SS_MT */

        TliConnectDone(tcon);

        ss_pprintf_2(("tb_dd_updatesynchistorykeys:%d\n", succp));
        if (!firstp) {
            ss_beta(ui_msg_message_nogui(SNC_MSG_SYNC_HISTKEY_CONV_DONE));
        }

        return(succp);
}
#endif /* !SS_MYSQL */

/*##**********************************************************************\
 *
 *              tb_dd_addinfo
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      property -
 *
 *
 *      value_str -
 *
 *
 *      value_long -
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
void tb_dd_addinfo(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        char* property,
        char* value_str,
        long value_long)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char* relname = (char *)RS_RELNAME_INFO;
        char* bind_value_str;
        long bind_value_long;

        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               relname);
        ss_assert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_INFO_PROPERTY, &property);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_INFO_VALUE_STR, &bind_value_str);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_INFO_VALUE_INT, &bind_value_long);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_INFO_PROPERTY,
                TLI_RELOP_EQUAL,
                property);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        bind_value_str = value_str;
        bind_value_long = value_long;
        if (trc == TLI_RC_SUCC) {
            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        } else {
            trc = TliCursorInsert(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*##**********************************************************************\
 *
 *              tb_dd_removeinfo
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      property -
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
bool tb_dd_removeinfo(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        char* property)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        bool foundp;
        char* relname = (char *)RS_RELNAME_INFO;

        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               relname);
        ss_assert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_INFO_PROPERTY, &property);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_INFO_PROPERTY,
                TLI_RELOP_EQUAL,
                property);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        foundp = (trc == TLI_RC_SUCC);
        if (foundp) {
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return (foundp);
}

/*##**********************************************************************\
 *
 *              tb_dd_getinfo
 *
 * Gets info from SYS_INFO table
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      property - in
 *              info keyword
 *
 *      p_value_long - out
 *              pointer to info value as long integer
 *
 *      p_value_str - out, give
 *              pointer to char pointer for info value as string, in case
 *              of SQL NULL, *p_value_str is set to NULL pointer
 *
 * Return value :
 *      TRUE - info found
 *      FALSE - not found
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dd_getinfo(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        char* property,
        long* p_value_long,
        bool* p_value_long_is_null,
        char** p_value_str)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        bool foundp;
        char* relname = (char *)RS_RELNAME_INFO;

        ss_dassert(p_value_long != NULL);
        ss_dassert(p_value_long_is_null != NULL);
        ss_dassert(p_value_str != NULL);

        *p_value_str = NULL;

        if (trans == NULL) {
            tcon = TliConnectInit(cd);
        } else {
            tcon = TliConnectInitByTrans(cd, trans);
        }
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               relname);
        ss_assert(tcur != NULL);

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_INFO_PROPERTY,
                TLI_RELOP_EQUAL,
                property);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(
                tcur,
                RS_ANAME_INFO_VALUE_STR,
                p_value_str);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorColLong(
                tcur,
                RS_ANAME_INFO_VALUE_INT,
                p_value_long);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        if ((foundp = (trc == TLI_RC_SUCC))) {
            if (TliCursorColIsNULL(tcur, RS_ANAME_INFO_VALUE_INT)) {
                *p_value_long_is_null = TRUE;
            } else {
                *p_value_long_is_null = FALSE;
            }
            if (TliCursorColIsNULL(tcur, RS_ANAME_INFO_VALUE_STR)) {
                *p_value_str = NULL;
            } else {
                *p_value_str = SsMemStrdup(*p_value_str);
            }
        }
        TliCursorFree(tcur);
        TliConnectDone(tcon);
        return (foundp);
}


#ifdef COLLATION_UPDATE

static char dd_insertsysinfo_collation_str[] =
        "INSERT INTO SYS_INFO(PROPERTY,VALUE_STR,VALUE_INT) VALUES ('COLLATION_SEQ', '%s', %d)";

#ifndef SS_MYSQL
static char dd_updatesysinfo_collation_str[] =
"UPDATE SYS_INFO SET VALUE_STR='FIN', VALUE_INT=1\
 WHERE PROPERTY='COLLATION_SEQ' AND VALUE_INT IS NULL";
#endif

/*##**********************************************************************\
 *
 *              tb_dd_getcollation
 *
 * Reads current collation from SYS_INFO.
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      p_collation - out, use
 *          pointer to variable for collation enum code
 *
 * Return value :
 *      TRUE - collation found
 *      FALSE - collation not found from SYS_INFO
 *              (*p_collation = SU_CHCOLLATION_FIN)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dd_getcollation(
        rs_sysinfo_t* cd,
        su_chcollation_t* p_collation)
{
        bool succp;
        long l;
        bool l_isnull;
        char* collation_name;

        succp = tb_dd_getinfo(
                    cd,
                    NULL,
                    (char *)"COLLATION_SEQ",
                    &l,
                    &l_isnull,
                    &collation_name);
        if (!succp) {
            *p_collation = SU_CHCOLLATION_FIN;
            return (FALSE);
        }
        ss_dassert(succp);
        if (!l_isnull) {
            *p_collation = (su_chcollation_t)l;
            ss_dassert((long)*p_collation == l);
            ss_dassert(su_chcollation_byname(collation_name) == *p_collation);
        } else {
            /* Old version compatibility, value updated in database
             * in later routines...
             */
            *p_collation = SU_CHCOLLATION_FIN;
        }
        if (collation_name != NULL) {
            SsMemFree(collation_name);
        }
        return (TRUE);
}

/*##**********************************************************************\
 *
 *              tb_dd_insertsysinfo_collation
 *
 * Inserts collation info to SYS_INFO
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      trans - use
 *              transaction
 *
 *      sys - use
 *              sql system
 *
 *      chcollation - in
 *              collation info see <su0chcvt.h>
 *
 * Return value :
 *      TRUE - success
 *      FALSE - failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dd_insertsysinfo_collation(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        su_chcollation_t chcollation)
{
        bool succp;
        char* chcollation_name;
        size_t s;
        char* sqlstr;

        chcollation_name = su_chcollation_name(chcollation);
        s = sizeof(dd_insertsysinfo_collation_str);
        s += strlen(chcollation_name);
        s += 11 - 4 + 1; /* 4 == strlen("%s") + strlen("%d") */
        sqlstr = SsMemAlloc(s);
        SsSprintf(
            sqlstr,
            dd_insertsysinfo_collation_str,
            chcollation_name,
            (int)chcollation);
        succp = dd_execsql(cd, sys, trans, sqlstr, NULL);
        SsMemFree(sqlstr);
        return (succp);
}

#ifndef SS_MYSQL
/*#***********************************************************************\
 *
 *              dd_updatesysinfo_collation
 *
 * Updates old version SYS_INFO where PROPERTY = 'COLLATION_SEQ'
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      sys -
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
static void dd_updatesysinfo_collation(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys)
{
        bool succp;

        succp = dd_execsql(
                    cd,
                    sys,
                    trans,
                    dd_updatesysinfo_collation_str,
                    NULL);
        ss_dassert(succp);
}
#endif /* !SS_MYSQL */

#endif /* COLLATION_UPDATE */

/*##**********************************************************************\
 *
 *              tb_dd_runstartupsqlstmts
 *
 * Creates some system views and tables which can be created using
 * SQL interpreter (ie. They are neither used by the database engine nor
 * the SQL interpreter).
 *
 * Parameters :
 *
 *      cd - in, use
 *              sysinfo
 *
 *      trans - in out, use
 *              transaction
 *
 *      sys - in, use
 *              sql system
 *
 * Return value :
 *      TRUE when succesful or
 *      FALSE when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dd_runstartupsqlstmts(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys
#ifdef COLLATION_UPDATE
        ,su_chcollation_t chcollation
#endif
        )
{
        char **p;
        bool succp = TRUE;

        ss_dprintf_1(("tb_dd_runstartupsqlstmts\n"));

        SS_PUSHNAME("tb_dd_runstartupsqlstmts");

        for (p = (char **)dd_startupstmts; *p != NULL; p++) {
            ss_dprintf_2(("tb_dd_runstartupsqlstmts:'%s'\n", *p));
            ss_svc_notify_init();
            succp = dd_execsql(cd, sys, trans, *p, NULL);
            if (!succp) {
                break;
            }
        }

#ifndef SS_MYSQL
        for (p = dd_startupstmts_be; *p != NULL; p++) {
            ss_dprintf_2(("tb_dd_runstartupsqlstmts:'%s'\n", *p));
            ss_svc_notify_init();
            succp = dd_execsql(cd, sys, trans, *p, NULL);
            if (!succp) {
                break;
            }
        }
#endif /* !SS_MYSQL */


#ifdef COLLATION_UPDATE
        if (succp) {
            succp = tb_dd_insertsysinfo_collation(
                        cd,
                        trans,
                        sys,
                        chcollation);
        }

#endif

        ss_dprintf_2(("tb_dd_runstartupsqlstmts:end\n"));
        SS_POPNAME;
        return (succp);
}

/*#***********************************************************************\
 *
 *              dd_tablehascolumn
 *
 * Checks whether a table has a certain column
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              transaction
 *
 *      rbuf - in, use
 *              relation buffer
 *
 *      tabname - in, use
 *              table name
 *
 *      colname - in, use
 *              column name
 *
 * Return value :
 *      TRUE when table has a column with name given
 *      FALSE when table does not have such a column or
 *          table does not exist
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool dd_tablehascolumn(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        rs_entname_t* tabname,
        char* colname)
{
        rs_rbuf_present_t rp;
        TliConnectT* tcon;
        ulong relid;
        TliCursorT* tcur;
        TliRetT trc;
        bool foundp;

        ss_dassert(tabname != NULL);
        ss_dassert(colname != NULL);

        rp = rs_rbuf_relpresent(    /* Find relid by name */
                cd,
                rbuf,
                tabname,
                NULL,
                &relid);
        ss_dassert(rp != RSRBUF_AMBIGUOUS);
        if (rp == RSRBUF_NOTEXIST || rp == RSRBUF_AMBIGUOUS) {
            return (FALSE);
        }
        su_rc_dassert(rp == RSRBUF_EXISTS || rp == RSRBUF_BUFFERED, rp);

        tcon = TliConnectInitByTrans(cd, trans);
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_assert(tcur != NULL);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_COLUMNS_COLUMN_NAME,
                TLI_RELOP_EQUAL,
                colname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        foundp = (trc == TLI_RC_SUCC);
        TliCursorFree(tcur);
        TliConnectDone(tcon);
        return (foundp);
}

/*#***********************************************************************\
 *
 *              dd_updatesystypes
 *
 * Updates SYS_TYPES table to meet the ODBC 2.0 SQLGetTypeInfo
 * specification. (ie. adds two columns into sys_tables)
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      sys -
 *
 *
 *      rbuf -
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
static bool dd_updatesystypes(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf)
{
        uint i;
        bool existp;
        bool succp;
        TliConnectT* tcon;
        TliRetT trc;
        rs_entname_t en;

        rs_entname_initbuf(&en,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_TYPES);

        existp = dd_tablehascolumn(
                    cd,
                    trans,
                    rbuf,
                    &en,
                    (char *)RS_ANAME_TYPES_MINIMUM_SCALE);
        if (existp) {
            return (FALSE);
        }
        succp = dd_execsql(
                    cd,
                    sys,
                    trans,
                    dd_dropsystypesstmt,
                    NULL);
        ss_assert(succp);

        tcon = TliConnectInitByTrans(cd, trans);
        trc = TliCommit(tcon);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
        trc = TliBeginTransact(tcon);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
        TliConnectDone(tcon);

        succp = dd_execsql(
                    cd,
                    sys,
                    trans,
                    (char *)dd_createsystypesstmt,
                    NULL);
        ss_assert(succp);
        for (i = 0; dd_systypesdata[i] != NULL; i++) {
            ss_svc_notify_init();
            succp = dd_execsql(
                        cd,
                        sys,
                        trans,
                        dd_systypesdata[i],
                        NULL);
            su_rc_assert(succp, i);
        }
        return (TRUE);
}

/*#***********************************************************************\
 *
 *              dd_updatesysevents
 *
 * Updates SYS_EVENTS table.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      sys -
 *
 *
 *      rbuf -
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
#ifndef SS_MYSQL
static bool dd_updatesysevents(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf)
{
        rs_admevid_t admevid;
        char* buf;
        bool name_in_use;
        bool succp;
        rs_entname_t en;
        bool changes = FALSE;
        char* eventname;

        ss_dprintf_3(("tb_dd_updatesysevents\n"));

        buf = SsMemAlloc(1024);

        rs_entname_initbuf(
            &en,
            RS_AVAL_DEFCATALOG,
            RS_AVAL_SYSNAME,
            "SYS_EVENT_HSBCONNECTSTATUS");
        name_in_use = rs_rbuf_nameinuse(cd, rbuf, &en);
        if (!name_in_use) {
            /* Old format db. Delete all old system events and
             * recreate them.
             */
            ss_dprintf_4(("tb_dd_updatesysevents:old format db, name SYS_EVENT_HSBCONNECTSTATUS not in use\n"));
            for (admevid = RS_ADMEVENT_NOTIFY; admevid != RS_ADMEVENT_ENDOFENUM; admevid++) {
                eventname = rs_admev_eid2ename(admevid);
                rs_entname_initbuf(
                    &en,
                    RS_AVAL_DEFCATALOG,
                    RS_AVAL_SYSNAME,
                    eventname);
                name_in_use = rs_rbuf_nameinuse(cd, rbuf, &en);
                if (name_in_use) {
                    SsSprintf(
                        buf,
                        "DROP EVENT \"%s\".%s.%s",
                        RS_AVAL_DEFCATALOG,
                        RS_AVAL_SYSNAME,
                        eventname);
                    ss_dprintf_4(("tb_dd_updatesysevents:%s\n", buf));
                    changes = TRUE;
                    ss_svc_notify_init();

                    succp = dd_execsql(
                            cd,
                            sys,
                            trans,
                            buf,
                            NULL);
                    ss_rc_assert(succp, admevid);
                } else {
                    break;
                }
            }
        }

        if (changes) {
            succp = dd_execsql(
                    cd,
                    sys,
                    trans,
                    "COMMIT WORK",
                    NULL);
            ss_assert(succp);
        }

        for (admevid = RS_ADMEVENT_NOTIFY; admevid != RS_ADMEVENT_ENDOFENUM; admevid++) {
            eventname = rs_admev_eid2ename(admevid);
            ss_dprintf_4(("tb_dd_updatesysevents:eventname=%d\n", eventname));
            rs_entname_initbuf(
                &en,
                RS_AVAL_DEFCATALOG,
                RS_AVAL_SYSNAME,
                eventname);
            name_in_use = rs_rbuf_nameinuse(cd, rbuf, &en);
            if (!name_in_use) {
                ss_dprintf_4(("tb_dd_updatesysevents:name %s not in use\n", eventname));
                SsSprintf(
                    buf,
"CREATE EVENT %s(\
ENAME $(VCHR),\
POSTSRVTIME TIMESTAMP,\
UID INTEGER,\
NUMDATAINFO INTEGER,\
TEXTDATA $(VCHR))",
                    eventname);
                ss_dprintf_4(("tb_dd_updatesysevents:%s\n", buf));
                changes = TRUE;
                ss_svc_notify_init();

                succp = dd_execsql(
                        cd,
                        sys,
                        trans,
                        buf,
                        NULL);
                ss_rc_assert(succp, admevid);
            }
        }
        SsMemFree(buf);
        return (changes);
}
#endif /* !SS_MYSQL */

/*#***********************************************************************\
 *
 *              newrel_compare
 *
 * Compare function for rbt of new relations created during startup
 * SQL statement update.
 *
 * Parameters :
 *
 *      key1 -
 *
 *
 *      key2 -
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
static int newrel_compare(void* key1, void* key2)
{
        return(strcmp(key1, key2));
}

static bool dd_updatestartupsystables(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf)
{
        bool changes = FALSE;

        SS_PUSHNAME("dd_updatestartupsystables");

        if (dd_updatesystypes(cd, trans, sys, rbuf)) {
            changes = TRUE;
        }
#ifndef SS_MYSQL
        if (dd_updatesysevents(cd, trans, sys, rbuf)) {
            changes = TRUE;
        }

        if (tb_sync_convertoldsyncinfo(cd, trans)) {
            changes = TRUE;
        }
#endif /* !SS_MYSQL */

#ifdef COLLATION_UPDATE
#ifndef SS_MYSQL

        dd_updatesysinfo_collation(cd, trans, sys);

#endif /* !SS_MYSQL */
#endif /* COLLATION_UPDATE */

        SS_POPNAME;

        return(changes);
}

static bool dd_updatestartupsqlstmts(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf,
        su_rbt_t** p_newrel_rbt,
        dd_updatestmt_t updatestmts[],
        bool updatestartupsystables)
{
        uint i;
        bool succp = TRUE;
        bool changes = FALSE;
        rs_relh_t* relh= NULL;
        bool execp = FALSE;

        SS_PUSHNAME("dd_updatestartupsqlstmts");

        for (i = 0; updatestmts[i].relname != NULL; i++) {
            bool name_in_use;
            rs_entname_t en;

            if (updatestmts[i].relname[0] == '\0') {
                name_in_use = FALSE;
                ss_rc_dassert(updatestmts[i].type == DD_UPDATE_COMMIT,
                              updatestmts[i].type);
            } else {
                rs_entname_initbuf(&en,
                                   RS_AVAL_DEFCATALOG,
                                   RS_AVAL_SYSNAME,
                                   updatestmts[i].relname);
                name_in_use = rs_rbuf_nameinuse(cd, rbuf, &en);
            }

            switch (updatestmts[i].type) {
                case DD_UPDATE_CREATEVIEW:
                    ss_dassert(updatestmts[i].extname == NULL);
                    if (!name_in_use) {
                        ss_svc_notify_init();
                        ss_dprintf_4(("dd_updatestartupsqlstmts:%s\n",
                                        updatestmts[i].sqlstr));
                        succp = dd_execsql(
                                    cd,
                                    sys,
                                    trans,
                                    (char *)updatestmts[i].sqlstr,
                                    NULL);
                        ss_rc_assert(succp, i);
                        changes = TRUE;
                    }
                    break;
                case DD_UPDATE_CREATETABLE:
                    ss_dassert(updatestmts[i].extname == NULL);
                    if (!name_in_use) {
                        ss_svc_notify_init();
                        ss_dprintf_4(("dd_updatestartupsqlstmts:%s\n",
                                        updatestmts[i].sqlstr));
                        succp = dd_execsql(
                                    cd,
                                    sys,
                                    trans,
                                    (char *)updatestmts[i].sqlstr,
                                    NULL);
                        ss_rc_assert(succp, i);
                        changes = TRUE;
                        if (*p_newrel_rbt == NULL) {
                            *p_newrel_rbt = su_rbt_init(newrel_compare, NULL);
                        }
                        su_rbt_insert(*p_newrel_rbt, (char *)updatestmts[i].relname);
                    }
                    break;

                case DD_UPDATE_CREATEINDEX:
                case DD_UPDATE_DROPINDEX:
                case DD_UPDATE_ADDCOLUMN:
                    ss_dassert(updatestmts[i].extname != NULL);

                    if (!name_in_use) {
                        break;
                    }

                    relh = tb_dd_getrelh(cd, trans, &en, NULL, NULL);
                    ss_bassert(relh != NULL);

                    if (updatestmts[i].type == DD_UPDATE_ADDCOLUMN) {
                        rs_ttype_t* ttype;
                        rs_ano_t ano;
                        ss_dassert(updatestmts[i].type == DD_UPDATE_ADDCOLUMN);
                        ttype = rs_relh_ttype(cd, relh);
                        ano = rs_ttype_anobyname(
                                    cd,
                                    ttype,
                                    (char *)updatestmts[i].extname);
                        execp = (ano == RS_ANO_NULL);
                    } else {
                        rs_entname_t key_en;
                        rs_entname_initbuf(&key_en,
                                           RS_AVAL_DEFCATALOG,
                                           RS_AVAL_SYSNAME,
                                           updatestmts[i].extname);
                        execp = (rs_relh_keybyname(cd, relh, &key_en) == NULL);
                        if (updatestmts[i].type == DD_UPDATE_DROPINDEX) {
                            execp = !execp;
                        }
                    }

                    if (execp) {
                        /* Object not found, create it. */
                        succp = dd_execsql(
                                    cd,
                                    sys,
                                    trans,
                                    (char *)updatestmts[i].sqlstr,
                                    NULL);
                        ss_rc_assert(succp, i);
                        changes = TRUE;
                    }

                    SS_MEM_SETUNLINK(relh);
                    rs_relh_done(cd, relh);
                    break;

                case DD_UPDATE_SETVALUE:
                    if (name_in_use) {

                        succp = dd_execsql(
                                    cd,
                                    sys,
                                    trans,
                                    (char *)updatestmts[i].sqlstr,
                                    NULL);
                        ss_rc_assert(succp, i);
                    }
                    break;

#ifndef SS_MYSQL
                case DD_UPDATE_CHECKEXISTENCE:
                    dd_checkexistenceaction(
                        cd,
                        sys,
                        trans,
                        name_in_use,
                        updatestmts[i].extname);
                    break;
#endif /* !SS_MYSQL */

                case DD_UPDATE_COMMIT:
                    succp = dd_execsql(
                                cd,
                                sys,
                                trans,
                                (char *)"COMMIT WORK",
                                NULL);
                    ss_rc_assert(succp, i);
                    tb_trans_beginif(cd, trans);
                    break;

                case DD_UPDATE_CREATEEVENT:
                    ss_dassert(updatestmts[i].extname == NULL);
                    if (!name_in_use) {
                        ss_svc_notify_init();
                        ss_dprintf_4(("updatestartupsqlstmts:%s\n",
                                        updatestmts[i].sqlstr));
                        succp = dd_execsql(
                                    cd,
                                    sys,
                                    trans,
                                    (char *)updatestmts[i].sqlstr,
                                    NULL);
                        ss_rc_assert(succp, i);
                        changes = TRUE;
/*                          if (*p_newrel_rbt == NULL) { */
/*                              *p_newrel_rbt = su_rbt_init(newrel_compare, NULL); */
/*                          } */
/*                          su_rbt_insert(*p_newrel_rbt, updatestmts[i].relname); */
                    }
                    break;
              case DD_UPDATE_CREATEPROCEDURE:
                  ss_dassert(updatestmts[i].extname == NULL);
                  if (!name_in_use) {
                      ss_svc_notify_init();
                      ss_dprintf_4(("updatestartupsqlstmts:%.1024s\n",
                                    updatestmts[i].sqlstr));
                      succp = dd_execsql(
                                    cd,
                                    sys,
                                    trans,
                                    (char *)updatestmts[i].sqlstr,
                                    NULL);
                      ss_rc_assert(succp, i);
                  }
                  break;
              case DD_UPDATE_CREATESEQUENCE:
                  ss_dassert(updatestmts[i].extname == NULL);
                  if (!name_in_use) {
                      ss_svc_notify_init();
                      ss_dprintf_4(("updatestartupsqlstmts:%.1024s\n",
                                    updatestmts[i].sqlstr));
                      succp = dd_execsql(
                                    cd,
                                    sys,
                                    trans,
                                    (char *)updatestmts[i].sqlstr,
                                    NULL);
                      ss_rc_assert(succp, i);
                  }
                  break;
                default:
                    ss_rc_error(updatestmts[i].type);
            }
        }
        if (updatestartupsystables) {
            if (dd_updatestartupsystables(cd, trans, sys, rbuf)) {
                changes = TRUE;
            }
        }

        SS_POPNAME;
        return(changes);
}


/* Modify a column of a system table */
typedef struct dd_modifycolumn_st {
        const char* mc_relname;
        const char* mc_colname;
        const char* mc_coltype;
        rs_sqldatatype_t mc_coldatatype;
} dd_modifycolumn_t;

#ifndef SS_MYSQL
static dd_modifycolumn_t dd_modifycolumns [] =

{
        {RS_RELNAME_CARDINAL, RS_ANAME_CARDINAL_CARDIN, "BIGINT", RSSQLDT_BIGINT},
        {RS_RELNAME_CARDINAL, RS_ANAME_CARDINAL_SIZE, "BIGINT", RSSQLDT_BIGINT},
        {RS_RELNAME_CHECKSTRINGS, RS_ANAME_CHECKSTRINGS_CONSTRAINT,
            "LONG $(VCHR)", RSSQLDT_WLONGVARCHAR },
        {NULL, NULL, NULL, 0}
};

/* Get sql datatype for a given column from the table SYS_COLUMNS */
static bool dd_getcolsqltype(
        rs_sysinfo_t*   cd,
        long            relid,
        char*           colname,
        int*            sqldatatype)
{
        TliConnectT*    tcon;
        TliCursorT*     tcur;
        TliRetT         trc;

        tcon = TliConnectInit(cd);
        ss_dassert(tcon != NULL);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_SQLDTYPE_NO, sqldatatype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_COLUMNS_COLUMN_NAME,
                TLI_RELOP_EQUAL,
                colname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        if (trc == TLI_RC_SUCC) {
            /* Check that only one row found. */
            trc = TliCursorNext(tcur);
            ss_rc_dassert(trc == TLI_RC_END, TliCursorErrorCode(tcur));
        }
        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return(TRUE);
}

static bool dd_modifycolumnsstartup(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf,
        dd_modifycolumn_t modcolumns[])
{
        uint i;
        bool succp;
        bool changes = FALSE;
        bool name_in_use;
        rs_entname_t en;
        rs_relh_t* relh;
        int sqltype;
        ulong relid;

        for (i = 0; modcolumns[i].mc_relname != NULL; i++) {

            rs_entname_initbuf(&en,
                RS_AVAL_DEFCATALOG,
                RS_AVAL_SYSNAME,
                modcolumns[i].mc_relname);

            name_in_use = rs_rbuf_nameinuse(cd, rbuf, &en);
            ss_assert(name_in_use);

            relh = tb_dd_getrelh(cd, trans, &en, NULL, NULL);
            ss_assert(relh != NULL);

            /* We have to check the column name from SYS_COLUMNS,
               because system tables are hardcoded in dbe4srli.c.
               And they seem to be always correct even if they
               are incorrect (old version) in the SYS_COLUMNS. */
            relid  = rs_relh_relid(cd, relh);

            dd_getcolsqltype(cd, relid, (char *)modcolumns[i].mc_colname, &sqltype);

            /* Is the type different? */
            if (sqltype != modcolumns[i].mc_coldatatype) {
                char sqlstr[512];
                SsSprintf(sqlstr,
                    "ALTER TABLE %.80s MODIFY COLUMN %.80s %.80s",
                    modcolumns[i].mc_relname,
                    modcolumns[i].mc_colname,
                    modcolumns[i].mc_coltype);

                succp = dd_execsql(
                            cd,
                            sys,
                            trans,
                            sqlstr,
                            NULL);
                ss_assert(succp);
                changes = TRUE;
            }
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);
        }
        succp = dd_execsql(
                    cd,
                    sys,
                    trans,
                    (char *)"COMMIT WORK",
                    NULL);
        ss_assert(succp);

        return(changes);
}

static bool dd_modifykeysstartup(
        rs_sysinfo_t*   cd,
        tb_trans_t*     trans,
        sqlsystem_t*    sys __attribute__ ((unused)),
        rs_rbuf_t*      rbuf __attribute__ ((unused)))
{
        TliConnectT* tcon = TliConnectInitByTrans(cd, trans);

        char*       name;
        TliCursorT* tcur;
        TliRetT     trc;
        bool        ret = FALSE;
        rs_atype_t* atype = rs_atype_initchar(cd);
        int         relid;
        int         keyid;
        int         nordering;

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_KEY_NAME, &name);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_REF_REL_ID, &relid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_ID, &keyid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_KEYS_NREF, &nordering);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REFTYPE,
                TLI_RELOP_EQUAL,
                1);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrIsNull(
                tcur,
                atype,
                (char *)RS_ANAME_FORKEYS_KEY_NAME);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            rs_err_t*   errh = NULL;
            tb_relpriv_t* priv;
            rs_key_t*   rkey;
            rs_key_t*   key;
            rs_relh_t*  relh;

            relh = tb_dd_getrelhbyid(cd, trans, relid, &priv, &errh);
            ss_assert(relh != NULL && errh == NULL);

            key = rs_key_init(
                    cd,
                    (char *)"",
                    keyid,
                    FALSE,
                    FALSE,
                    FALSE,
                    FALSE,
                    nordering,
                    NULL);

            dd_readrefkeyparts(tcon, rs_relh_ttype(cd, relh), key);
            rkey = dd_resolverefkey(cd, relh, key);
            ss_dassert(rkey != NULL);

            trc = TliCursorColClearNULL(tcur, RS_ANAME_FORKEYS_KEY_NAME);
            ss_assert(trc == TLI_RC_SUCC);

            name = rs_key_name(cd, rkey);
            ss_dassert(name != NULL && *name != '\0');
            trc = TliCursorUpdate(tcur);
            ss_assert(trc == TLI_RC_SUCC);
            rs_key_done(cd, key);
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);

            ss_dprintf_1(("dd_modifykeysstartup: %d %d %s\n", relid, keyid, name));

            ret = TRUE;
        }

        TliCursorFree(tcur);

        if (ret) {
            trc = TliCommit(tcon);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
        }
        TliConnectDone(tcon);

        rs_atype_free(cd, atype);
        return ret;
}

static bool dd_modifychecktartup(
        rs_sysinfo_t*   cd,
        tb_trans_t*     trans,
        sqlsystem_t*    sys __attribute__ ((unused)),
        rs_rbuf_t*      rbuf __attribute__ ((unused)))
{
        TliConnectT* tcon = TliConnectInitByTrans(cd, trans);

        TliCursorT* tcur;
        TliRetT     trc;
        bool        ret = FALSE;
        int         relid;
        char*       relname;
        char*       checkstr;
        rs_atype_t* atype = rs_atype_initchar(cd);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_assert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &relname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CHECK, &checkstr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_TABLES_ID, &relid);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            char* name;
            char* fmt = (char *)RSK_CHECKSTR;
            if (checkstr == NULL) {
                continue;
            }
            name = SsMemAlloc(strlen(relname) +
                            strlen(fmt) +
                            8+1);
            SsSprintf(name, fmt, relname, 0);
            trc = dd_createnamedcheck(tcon, relid, name, checkstr);
            ss_assert(trc == TLI_RC_SUCC);
            SsMemFree(name);
            checkstr = NULL;
            trc = TliCursorUpdate(tcur);
            ss_assert(trc == TLI_RC_SUCC);
            ret = TRUE;
        }

        TliCursorFree(tcur);

        if (ret) {
            trc = TliCommit(tcon);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
        }
        TliConnectDone(tcon);

        rs_atype_free(cd, atype);
        return ret;
}
#endif /* !SS_MYSQL */

dd_updatestmt_t dd_updateunqkeystmts[] = {
{
RS_RELNAME_SEQUENCES, "$_SYSTEM$SYS_SEQUENCES_UNQKEY_0", DD_UPDATE_DROPINDEX,
"DROP INDEX \"$_SYSTEM$SYS_SEQUENCES_UNQKEY_0\"\
@\
COMMIT WORK\
@\
CREATE UNIQUE INDEX \"$$SYS_SEQUENCES_UNQKEY_0\"\
 ON _SYSTEM.SYS_SEQUENCES (SEQUENCE_NAME,SEQUENCE_SCHEMA,SEQUENCE_CATALOG)\
@\
COMMIT WORK"
},
{
RS_RELNAME_PROCEDURES, "$_SYSTEM$SYS_PROCEDURES_UNQKEY_0", DD_UPDATE_DROPINDEX,
"DROP INDEX \"$_SYSTEM$SYS_PROCEDURES_UNQKEY_0\"\
@\
COMMIT WORK\
@\
CREATE UNIQUE INDEX \"$$SYS_PROCEDURES_UNQKEY_0\"\
 ON _SYSTEM.SYS_PROCEDURES (PROCEDURE_NAME,PROCEDURE_SCHEMA,PROCEDURE_CATALOG)\
@\
COMMIT WORK"
},
{
RS_RELNAME_TRIGGERS, "$_SYSTEM$SYS_TRIGGERS_UNQKEY_0", DD_UPDATE_DROPINDEX,
"DROP INDEX \"$_SYSTEM$SYS_TRIGGERS_UNQKEY_0\"\
@\
COMMIT WORK\
@\
CREATE UNIQUE INDEX \"$$SYS_TRIGGERS_UNQKEY_0\"\
 ON _SYSTEM.SYS_TRIGGERS (TRIGGER_NAME,TRIGGER_SCHEMA,TRIGGER_CATALOG)\
@\
COMMIT WORK"
},
{
RS_RELNAME_EVENTS, "$_SYSTEM$SYS_EVENTS_UNQKEY_0", DD_UPDATE_DROPINDEX,
"DROP INDEX \"$_SYSTEM$SYS_EVENTS_UNQKEY_0\"\
@\
COMMIT WORK\
@\
CREATE UNIQUE INDEX \"$$SYS_EVENTS_UNQKEY_0\"\
 ON _SYSTEM.SYS_EVENTS (EVENT_NAME,EVENT_SCHEMA,EVENT_CATALOG)\
@\
COMMIT WORK"
},
{
NULL,
NULL,
DD_UPDATE_CREATETABLE,
NULL
}
};

bool tb_dd_updatecatalogkeys(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf,
        su_rbt_t** p_newrel_rbt)
{
        bool changes;
        SS_PUSHNAME("tb_dd_updatecatalogkeys");
        changes = dd_updatestartupsqlstmts(
                    cd,
                    trans,
                    sys,
                    rbuf,
                    p_newrel_rbt,
                    dd_updateunqkeystmts,
                    TRUE);
        SS_POPNAME;
        return (changes);
}

#ifndef SS_MYSQL
static bool dd_updateproceduredefs(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf,
        su_rbt_t** p_newrel_rbt){

        dd_updatestmt_t *procedurestmts;
        bool version_property_found;
        bool changes;
        bool drop_existing_procedures = FALSE;
        bool existing_version_isnull;
        char* existing_version_name;
        long existing_version;
        long latest_version;
            
        SS_PUSHNAME("dd_updateproceduredefs");

        /* Find out the latest sync procedure version number.
         *
         * IMPORTANT NOTE!
         * If you made a fix to sync procedure definitions, be sure
         * you increased the SYNC_PROCEDURE_VERSION counter in
         * tab2dd.c file. This ensures that your fixes are also
         * propagated to already-existing database files.
         */
        latest_version = tb_dd_getlatestsyncprocedureversion();

        /* look for property SYNC_PROCEDURE_VERSION in SYS_INFO table */
        version_property_found = tb_dd_getinfo(
                cd,
                NULL,
                "SYNC_PROCEDURE_VERSION",
                &existing_version,
                &existing_version_isnull,
                &existing_version_name);
        
        if (version_property_found){
            
            if (existing_version < latest_version){
                
                char sql[256];
                bool ok;
                
                SsSprintf(sql,
                          "UPDATE SYS_INFO SET VALUE_STR='%d', VALUE_INT=%d WHERE PROPERTY='SYNC_PROCEDURE_VERSION'",
                          latest_version,
                          latest_version);

                /* Sync procedure version property found
                 * but the existing version is outdated.
                 *
                 * Update the new version into SYS_INFO table and
                 * drop the existing procedures.
                 */
                ok = dd_execsql(
                        cd,
                        sys,
                        trans,
                        sql,
                        NULL);
                
                ss_assert(ok);
                
                drop_existing_procedures = TRUE;
            }
            
        } else {
            
            char sql[256];
            bool ok;
            
            SsSprintf(sql,
                      "INSERT INTO SYS_INFO VALUES('SYNC_PROCEDURE_VERSION','%d',%d)",
                      latest_version,
                      latest_version);

            /* Sync procedure version property not found.
             * Insert it into SYS_INFO table.
             *
             * If there are existing versions of sync procedures
             * they must be dropped because they are most likely outdated.
             */
            ok = dd_execsql(
                    cd,
                    sys,
                    trans,
                    sql,
                    NULL);

            drop_existing_procedures = TRUE;
            
            ss_assert(ok);
        }

        if (drop_existing_procedures){

            bool ok;
            bool found;
            char* dropstmts = NULL;
            
            found = tb_dd_getsyncproceduredropstmts(cd, rbuf, &dropstmts);

            if (found){
                ok = dd_execsql(
                        cd,
                        sys,
                        trans,
                        dropstmts,
                        NULL);
                ss_assert(ok);
            }
            if (dropstmts != NULL){
                SsMemFree(dropstmts);
            }
        }

        /* get the sync procedure creation statements */
        procedurestmts = tb_dd_syncproceduredefs_init();

        if (procedurestmts != NULL) {
            changes = dd_updatestartupsqlstmts(
                            cd,
                            trans,
                            sys,
                            rbuf,
                            p_newrel_rbt,
                            procedurestmts,
                            TRUE);
        
            tb_dd_syncproceduredefs_done(procedurestmts);
        }

        if (existing_version_name != NULL){
            SsMemFree(existing_version_name);
        }
        
        SS_POPNAME;
        
        return (changes);
}
#endif /* !SS_MYSQL */


/*##**********************************************************************\
 *
 *              tb_dd_updatestartupsqlstmts
 *
 * Updates system data dictionary in the database. Creates those system
 * tables that do not already exist in the database.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trans - use
 *
 *
 *      sys - in
 *
 *
 *      rbuf - in
 *
 *
 *      p_newrel_rbt - out, give
 *              If there were new relations created, the names are
 *              stored into this rbt. The rbt is created if it is NULL.
 *
 * Return value :
 *
 *      TRUE    - data dictionary was changed
 *      FALSE   - no changes
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dd_updatestartupsqlstmts(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf,
        su_rbt_t** p_newrel_rbt)
{
        bool changes;

        dd_updatestmt_t* dd_updatestmts_be = tb_dd_updatestmts_be();

#ifndef SS_MYSQL        
        /* System sequences */
        dd_updatestmt_t* sequencestmts = tb_dd_syncsequencedefs();
#endif
        
        SS_PUSHNAME("tb_dd_updatestartupsqlstmts");
        changes =
            dd_updatestartupsqlstmts(
                    cd,
                    trans,
                    sys,
                    rbuf,
                    p_newrel_rbt,
                    dd_updatestmts,
                    FALSE);

        if (dd_updatestmts_be != NULL) {
            changes |=
                dd_updatestartupsqlstmts(
                        cd,
                        trans,
                        sys,
                        rbuf,
                        p_newrel_rbt,
                        dd_updatestmts_be,
                        TRUE);
        } else {
            changes |= dd_updatestartupsystables(cd, trans, sys, rbuf);
        }

        {
            bool ts19 = FALSE;
            bool foundp;

            foundp = su_inifile_getbool(
                        dbe_db_getinifile(rs_sysi_db(cd)),
                        SU_SQL_SECTION,
                        SU_SQL_TIMESTAMPDISPLAYSIZE19,
                        &ts19);

            if (foundp && ts19) {
                changes |= dd_updatestartupsqlstmts(
                    cd,
                    trans,
                    sys,
                    rbuf,
                    p_newrel_rbt,
                    update_timestamp19,
                    TRUE);
            }
        }

#ifndef SS_MYSQL
        changes |= dd_updateproceduredefs(
                    cd,
                    trans,
                    sys,
                    rbuf,
                    p_newrel_rbt );

        changes |= dd_updatestartupsqlstmts(
                    cd,
                    trans,
                    sys,
                    rbuf,
                    p_newrel_rbt,
                    sequencestmts,
                    TRUE);

        changes |= dd_modifycolumnsstartup(
                    cd,
                    trans,
                    sys,
                    rbuf,
                    dd_modifycolumns);

        changes |= dd_modifykeysstartup(
                    cd,
                    trans,
                    sys,
                    rbuf);

        changes |= dd_modifychecktartup(
                    cd,
                    trans,
                    sys,
                    rbuf);

#endif /* !SS_MYSQL */

        SS_POPNAME;
        return (changes);
}

#ifndef SS_MYSQL
static void dd_checkexistenceaction(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys,
        tb_trans_t* trans,
        bool name_in_use,
        char* option)
{
        if (name_in_use && (strcmp(option, "COLINFOTABLE_UPDATE") == 0)) {
            populatecolumninfotable(cd, sys, trans);
        }
}


static void populatecolumninfotable(
        rs_sysinfo_t* cd,
        sqlsystem_t* sqlsys,
        tb_trans_t* trans)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        char* procsql;
        long procid;
        char* procname;
        char* procschema;
        char* proccatalog;
        TliRetT trc;
        rs_entname_t en;
        rs_err_t* errh = NULL;
        bool b;


        /*
         * Check that column info table is empty.
         */
        tcon = TliConnectInitByTrans(cd, trans);
        ss_assert(tcon != NULL);
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_PROCEDURE_COL_INFO);
        ss_assert(tcur != NULL);

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        if (trc != TLI_RC_END) {
            /* Table is not empty */
            TliCursorFree(tcur);
            TliConnectDone(tcon);
            return;
        }
        TliCursorFree(tcur);

        /*
         * Recreate all procedures.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_PROCEDURES);
        ss_assert(tcur != NULL);

        TliCursorSetMaxBlobSize(tcur, SS_MAXALLOCSIZE);

        trc = TliCursorColLong(tcur, RS_ANAME_PROCEDURES_ID, &procid);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorColStr(tcur, RS_ANAME_PROCEDURES_NAME, &procname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorColStr(tcur, RS_ANAME_PROCEDURES_SCHEMA, &procschema);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorColStr(tcur, RS_ANAME_PROCEDURES_CATALOG, &proccatalog);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_PROCEDURES_TEXT, &procsql);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            rs_entname_initbuf(&en, proccatalog, procschema, procname);
            b = tb_sql_generateproccolinfo(cd, sqlsys, trans, procid, &en, procsql, &errh);
            if (!b) {
                ss_assert(errh != NULL);
                ui_msg_message(DBE_MSG_PROCEDURE_CONVERT_ERROR_SS, procname, su_err_geterrstr(errh));
                rs_error_free(cd, errh);
                errh = NULL;
            }
        }
        ss_dassert(trc == TLI_RC_END || TliTransIsFailed(tcon));

        TliCursorFree(tcur);
        TliConnectDone(tcon);

}
#endif /* !SS_MYSQL */


static char* dd_findrelmodestr(char* str, char* relmode);

static void dd_removerelmodestr(char* str, char* relmode)
{
        char* pos;

        pos = dd_findrelmodestr(str, relmode);
        if (pos != NULL) {
            char* copy_start;
            copy_start = pos + strlen(relmode);
            while (ss_isspace(*copy_start)) {
                copy_start++;
            }
            strcpy(pos, copy_start);
        }
}

static void dd_trimspaces(char* str)
{
        char* p;

        if (str != NULL) {
            p = str;
            while (ss_isspace(*str)) {
                str++;
            }
            while (*str) {
                if (ss_isspace(*str)) {
                    /* Output one space. */
                    *p++ = ' ';
                    /* Skip all other spaces. */
                    while (ss_isspace(*str)) {
                        str++;
                    }
                } else {
                    *p++ = *str++;
                }
            }
            *p = '\0';
        }
}

#endif /* !SS_NODDUPDATE */

static char* dd_findrelmodestr(char* str, char* relmode)
{
        char* pos;

        if (str != NULL) {
            pos = strstr(str, relmode);
            if (pos != NULL) {
                if (pos != str && pos[-1] != ' ') {
                    /* Found inside other relmode string, not accepted. */
                    pos = NULL;
                }
            }
        } else {
            pos = NULL;
        }
        return(pos);
}

/*#***********************************************************************\
 *
 *              dd_checkifrelempty_tcon
 *
 * Checks if the relation is empty using a TLI connect object.
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relname -
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
static bool dd_checkifrelempty_tcon(
        TliConnectT* tcon,
        rs_entname_t* relname)
{
        bool emptyp;
        TliCursorT* tcur;
        TliRetT trc;

        tcur = TliCursorCreateEn(
                    tcon,
                    relname);
        if (tcur == NULL) {
            return(TRUE);
        }

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        emptyp = (TliCursorNext(tcur) != TLI_RC_SUCC);

        TliCursorFree(tcur);

        return(emptyp);
}

/*##**********************************************************************\
 *
 *              tb_dd_checkifrelempty
 *
 * Checks if the relation is empty. Check is done from user transaction
 * and from most recent read level.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      relname -
 *
 *
 * Return value :
 *
 *      TRUE    - relation is empty
 *      FALSE   - relation is not empty
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dd_checkifrelempty(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname)
{
        bool emptyp;
        TliConnectT* tcon;

        tcon = TliConnectInit(cd);
        emptyp = dd_checkifrelempty_tcon(tcon, relname);
        TliConnectDone(tcon);

        if (emptyp) {
            tcon = TliConnectInitByTrans(cd, trans);
            emptyp = dd_checkifrelempty_tcon(tcon, relname);
            TliConnectDone(tcon);
        }

        return(emptyp);
}

/*##**********************************************************************\
 *
 *              tb_dd_curdate
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dt_date_t tb_dd_curdate(void)
{
        dt_date_t date;
        bool      b;

        b = dt_date_setnow(0, &date);
        ss_dassert(b);
        return(date);
}

#ifndef SS_NODDUPDATE

/*#***********************************************************************\
 *
 *              dd_getboolstr
 *
 * Returns an ascii zero string that corresponds to a boolean value.
 *
 * Parameters :
 *
 *      b - in
 *              boolean value
 *
 *
 * Output params:
 *
 * Return value - ref :
 *
 *      string corresponding the boolean value
 *
 * Limitations  :
 *
 * Globals used :
 */
static char* dd_getboolstr(bool b)
{
        return(b ? (char *)RS_AVAL_YES : (char *)RS_AVAL_NO);
}

#endif /* SS_NODDUPDATE */

/*#***********************************************************************\
 *
 *              dd_getboolnum
 *
 * Returns a number that corresponds to a boolean value.
 *
 * Parameters :
 *
 *      b - in, use
 *              boolean value
 *
 * Output params:
 *
 * Return value :
 *
 *       number corresponding the boolean value
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_getboolnum(char* bstr)
{
        ss_dassert(bstr != NULL);

        return(*bstr == 'Y' || *bstr == 'y');
}

#ifdef SS_COLLATION
static bool dd_readkeyparts_aux(TliConnectT* tcon,
                                long keyid,
                                long kpno,
                                int* p_prefixlen)
{
        bool foundp;
        TliCursorT* tcur;
        TliRetT trc;

        ss_dprintf_3(("dd_readkeyparts_aux\n"));
        ss_dassert(p_prefixlen != NULL);

        *p_prefixlen = 0;
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS_AUX);
        ss_dassert(tcur != NULL);
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_AUX_PREFIX_LENGTH,
                              p_prefixlen);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrLong(tcur, RS_ANAME_KEYPARTS_AUX_ID,
                                  TLI_RELOP_EQUAL,
                                  keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrLong(tcur, RS_ANAME_KEYPARTS_AUX_KEYP_NO,
                                  TLI_RELOP_EQUAL,
                                  kpno);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorNext(tcur);
        if (trc == TLI_RC_SUCC) {
            foundp = TRUE;
        } else {
            ss_rc_dassert(trc == TLI_RC_END, trc);
            foundp = FALSE;
        }
        TliCursorFree(tcur);
        return (foundp);
}
#endif /* SS_COLLATION */

/*#***********************************************************************\
 *
 *              dd_readkeyparts
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      key -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_readkeyparts(
        TliConnectT* tcon,
        rs_ttype_t* ttype,
        rs_key_t* key)
{
        TliCursorT* tcur;
        TliRetT trc;
        int i;
        int kptype;
        int kpno;
        char* ascstr;
        int ano;
        va_t* constvalue;
        rs_sysinfo_t* cd = TliGetCd(tcon);

        ss_dprintf_3(("dd_readkeyparts\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_ATTR_TYPE, &kptype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYPARTS_ASCENDING, &ascstr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_ATTR_NO, &ano);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColVa(tcur, RS_ANAME_KEYPARTS_CONST_VALUE, &constvalue);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_KEYP_NO, &kpno);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYPARTS_ID,
                TLI_RELOP_EQUAL,
                rs_key_id(cd, key));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        for (i = 0; (trc = TliCursorNext(tcur)) == TLI_RC_SUCC; i++) {
#ifdef SS_COLLATION
            int prefix_length = 0;
            if (kptype == RSAT_COLLATION_KEY) {
                ss_debug(bool foundp;)
                ss_dassert(constvalue == NULL);
                constvalue = (va_t*)rs_atype_collation(cd, rs_ttype_atype(cd, ttype, ano));
                ss_dassert(kpno == i);
                ss_debug(foundp =)
                dd_readkeyparts_aux(tcon,
                                    rs_key_id(cd, key),
                                    kpno,
                                    &prefix_length);
                ss_dassert(foundp ?
                           (prefix_length > 0) :
                           (prefix_length == 0));
            }
#endif /* SS_COLLATION */

            rs_key_addpart(
                cd,
                key,
                i,
                kptype,
                dd_getboolnum(ascstr),
                ano,
                constvalue);

#ifdef SS_COLLATION
            if (prefix_length != 0) {
                ss_dassert(prefix_length > 0);
                rs_keyp_setprefixlength(cd, key, i, prefix_length);
            }
#endif /* SS_COLLATION */
        }

        ss_dassert(trc == TLI_RC_END);
        ss_dassert(i > 0 || TliCursorErrorCode(tcur) != SU_SUCCESS);

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_readkeys
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_readkeys(
        TliConnectT* tcon,
        rs_relh_t* relh)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* keyname;
        long keyid;
        char* unique;
        char* clustering;
        char* primary;
        char* prejoined;
        int nordering;
        char* schema;
        char* catalog;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        long relid;

        relid = rs_relh_relid(cd, relh);

        ss_dprintf_3(("dd_readkeys:relid=%ld\n", relid));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_NAME, &keyname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_UNIQUE, &unique);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CLUSTERING, &clustering);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_PRIMARY, &primary);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_PREJOINED, &prejoined);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYS_NREF, &nordering);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_SCHEMA, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            rs_key_t* key;
            ss_dprintf_4(("dd_readkeys:relid=%ld, keyid=%ld\n", relid, keyid));
            key = rs_key_init(
                    cd,
                    keyname,
                    keyid,
                    dd_getboolnum(unique),
                    dd_getboolnum(clustering),
                    dd_getboolnum(primary),
                    dd_getboolnum(prejoined),
                    nordering,
                    NULL);
            dd_readkeyparts(tcon, rs_relh_ttype(cd, relh), key);
            rs_key_setindex_ready(cd, key);
            rs_relh_insertkey(cd, relh, key);
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

static int keynameid_compare(void* key1, void* key2)
{
        dbe_keynameid_t* dk1 = key1;
        dbe_keynameid_t* dk2 = key2;

        return(su_rbt_long_compare(dk1->dk_keyid, dk2->dk_keyid));
}

static void keynameid_delete(void* key)
{
        dbe_keynameid_t* dk = key;

        SsMemFree(dk->dk_keyname);
        SsMemFree(dk);
}

su_rbt_t* tb_dd_readallkeysintorbt(
        rs_sysinfo_t* cd)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char* keyname;
        long keyid;
        su_rbt_t* rbt;

        ss_dprintf_3(("tb_dd_readallkeysintorbt\n"));

        tcon = TliConnectInit(cd);

        rbt = su_rbt_init(keynameid_compare, keynameid_delete);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_NAME, &keyname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            dbe_keynameid_t* dk;
            dk = SsMemCalloc(sizeof(dbe_keynameid_t), 1);
            dk->dk_keyid = keyid;
            dk->dk_keyname = SsMemStrdup(keyname);
            su_rbt_insert(rbt, dk);
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return(rbt);
}

#ifdef REFERENTIAL_INTEGRITY

/*#***********************************************************************\
 *
 *              dd_readrefkeyparts
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      key -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_readrefkeyparts(
        TliConnectT* tcon,
        rs_ttype_t* ttype,
        rs_key_t* key)
{
        TliCursorT* tcur;
        TliRetT trc;
        int i;
        int kptype;
        int ano;
        va_t* constvalue;
        rs_sysinfo_t* cd = TliGetCd(tcon);
#ifdef SS_COLLATION
        int prefix_length;
#endif /* SS_COLLATION */
        ss_dprintf_3(("dd_readrefkeyparts\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYPARTS);
        ss_assert(tcur != NULL);

        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_ATTR_TYPE, &kptype);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_ATTR_NO, &ano);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColVa(tcur, RS_ANAME_FORKEYPARTS_CONST_VALUE, &constvalue);
        ss_assert(trc == TLI_RC_SUCC);
#ifdef SS_COLLATION
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_PREFIX_LENGTH,
                              &prefix_length);
        ss_assert(trc == TLI_RC_SUCC);
#endif /* SS_COLLATION */

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYPARTS_ID,
                TLI_RELOP_EQUAL,
                rs_key_id(cd, key));
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        for (i = 0; ; i++) {
#ifdef SS_COLLATION
            prefix_length = 0;
#endif /* SS_COLLATION */

            trc = TliCursorNext(tcur);
            if (trc != TLI_RC_SUCC) {
                break;
            }
            
#ifdef SS_COLLATION
            if (kptype == RSAT_COLLATION_KEY) {
                ss_dassert(constvalue == NULL);
                constvalue = (va_t*)rs_atype_collation(cd, rs_ttype_atype(cd, ttype, ano));
            }
#endif /* SS_COLLATION */

            rs_key_addpart(
                cd,
                key,
                i,
                kptype,
                TRUE,
                ano,
                constvalue);

#ifdef SS_COLLATION
            if (prefix_length != 0) {
                ss_dassert(kptype == RSAT_COLLATION_KEY);
                ss_dassert(prefix_length > 0);
                rs_keyp_setprefixlength(cd, key, i, prefix_length);
            }
#endif /* SS_COLLATION */
        }

        ss_rc_assert(trc == TLI_RC_END, trc);
        ss_assert(i > 0);

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_readrefkeys
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t dd_readrefkeys(
        TliConnectT*    tcon,
        rs_relh_t*      relh)
{
        TliCursorT*     tcur;
        TliRetT         trc;
        long            keyid;
        int             reftype;
        int             nordering;
        char*           schema;
        char*           name;
        int             action;
        char*           catalog;
        rs_sysinfo_t*   cd = TliGetCd(tcon);
        dbe_ret_t       rc = SU_SUCCESS;

        ss_dprintf_3(("dd_readrefkeys\n"));

        if (rs_relh_issysrel(cd, relh)) {
            /* System tables have no foreign keys.
             */
            return SU_SUCCESS;
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, &keyid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_REFTYPE, &reftype);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_NREF, &nordering);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_SCHEMA, &schema);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_KEY_NAME, &name);
        if (trc != TLI_RC_SUCC) {
            /* This might happen only during recovery when converting from
             * older database version to current.
             */
            ss_dassert(dbe_db_migrateneeded(TliGetDb(tcon)));
            name = NULL;
            action = (SQL_REFACT_NOACTION<<8) | SQL_REFACT_NOACTION;
        } else {
            trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_KEY_ACTION, &action);
            ss_assert(trc == TLI_RC_SUCC);
        }
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_CATALOG, &catalog);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REF_REL_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            rs_key_t*   key;
            long        refkeyid;
            long        refrelid;
            va_t*       va;
            rs_relh_t*  refrelh;
            TliCursorT* tcur2;

            if (name == NULL) {
                name = (char *)"";
            }

            key = rs_key_init(
                    cd,
                    name,
                    keyid,
                    FALSE,
                    FALSE,
                    FALSE,
                    FALSE,
                    nordering,
                    NULL);
            rs_key_settype(cd, key, reftype);
            rs_key_setaction(cd, key, action);
            dd_readrefkeyparts(tcon, rs_relh_ttype(cd, relh), key);
            rs_key_setindex_ready(cd, key);
            rs_relh_insertrefkey(cd, relh, key);

            /* Read the relh that is the "target" of this foreign key. */
            ss_dassert(rs_keyp_parttype(cd, key, 0) == RSAT_KEY_ID);
            va = rs_keyp_constvalue(cd, key, 0);
            refkeyid = va_getlong(va);

            tcur2 = TliCursorCreate(
                    tcon,
                    RS_AVAL_DEFCATALOG,
                    RS_AVAL_SYSNAME,
                    RS_RELNAME_KEYS);
            ss_assert(tcur2 != NULL);

            trc = TliCursorColLong(tcur2, RS_ANAME_KEYS_REL_ID, &refrelid);
            ss_assert(trc == TLI_RC_SUCC);
            trc = TliCursorConstrLong(
                    tcur2,
                    RS_ANAME_KEYS_ID,
                    TLI_RELOP_EQUAL,
                    refkeyid);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorOpen(tcur2);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorNext(tcur2);
            /* It is possible that the referred table doesn't exist,
               at least during a cascade schema/catalog drop. */
            if (trc == TLI_RC_SUCC) {
                rs_key_setrefrelid(cd, key, refrelid);
                refrelh = dd_readrelh(
                        tcon,
                        (unsigned) refrelid,
                        DD_READRELH_MINIMAL,
                        FALSE,
                        NULL,
                        &rc);
                if (refrelh == NULL) {
                    ss_dassert(rc != SU_SUCCESS);
                } else {
                    ss_dassert(rc == SU_SUCCESS);
                    tb_dd_setmmerefkeyinfo(cd, key, refrelh);
                    SS_MEM_SETUNLINK(refrelh);
                    rs_relh_done(cd, refrelh);
                }

                ss_debug({
                    trc = TliCursorNext(tcur2);
                    ss_dassert(trc == TLI_RC_END);
                });
            }
            TliCursorFree(tcur2);
        }
        ss_assert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        return rc;
}

/*#***********************************************************************\
 *
 *              dd_resolverefkey
 *
 * Finds key corresponding to foreign key.
 *
 * Parameters :
 *
 *      cd - 
 *
 *      relh - foreign key relh
 *
 *      refkey - referencing key
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 *
 */
static rs_key_t *dd_resolverefkey(
        rs_sysinfo_t*   cd,
        rs_relh_t*      relh,
        rs_key_t*       refkey)
{
        su_pa_t*        keys = rs_relh_keys(cd, relh);
        ulong           y;
        rs_key_t*       key;

        ss_dprintf_4(("resolving refkey %d %s\n",
                      rs_key_id(cd, refkey), rs_key_name(cd, refkey)));

        su_pa_do_get(keys, y, key) {
            rs_ano_t kp, last_kp;

            ss_dprintf_4(("Matching to key %d %s\n",
                          rs_key_id(cd, key), rs_key_name(cd, key)));
            
            last_kp = rs_key_lastordering(cd, refkey);

            if (last_kp != (rs_ano_t)rs_key_lastordering(cd, key)) {
                goto next_key;
            }

            for (kp = 0; kp <= last_kp; kp++) {
                rs_ano_t ano1 = rs_keyp_ano(cd, key, kp);
                rs_ano_t ano2 = rs_keyp_ano(cd, refkey, kp);

                ss_dprintf_4(("matching kp %d: (%d) vs (%d)\n",
                              kp,
                              ano1,
                              ano2));

                if (ano1 != ano2) {
                    goto next_key;
                }
            }

            ss_dprintf_4(("match found\n"));
            return key;
 next_key:
            ;
        }
        
        return NULL;
}

/*##**********************************************************************\
 *
 *              tb_dd_resolverefkeys
 *
 * Resolves all the relh foreign keys.
 *
 * Parameters :
 *
 *      cd -  
 *
 *      relh - foreign key relh
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 *
 */
dbe_ret_t tb_dd_resolverefkeys(
        rs_sysinfo_t*   cd,
        rs_relh_t*      relh)
{
        su_pa_t*        refkeys;
        ulong           x;
        rs_key_t*       refkey;

        ss_dprintf_3(("tb_dd_resolverefkeys\n"));

        if (rs_relh_issysrel(cd, relh)) {
            /* System tables have no foreign keys.
             */
            return SU_SUCCESS;
        }

        refkeys = rs_relh_refkeys(cd, relh);
        su_pa_do_get(refkeys, x, refkey) {
            rs_key_t* key = dd_resolverefkey(cd, relh, refkey);
            ss_dassert(key != NULL);
            rs_key_addrefkey(cd, key, refkey);
        }

        return SU_SUCCESS;
}

/*##**********************************************************************\
 * 
 *      tb_dd_setmmerefkeyinfo
 * 
 * Sets MME specific reference key info.
 * 
 * Parameters : 
 * 
 *      cd - 
 *          
 *          
 *      key - 
 *          
 *          
 *      refrelh - 
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
void tb_dd_setmmerefkeyinfo(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_relh_t* refrelh)
{
        ss_dprintf_1(("tb_dd_setmmerefkeyinfo:refrelname=%.80s, keyname=%.80s\n", rs_relh_name(cd, refrelh), rs_key_name(cd, key)));
        if (rs_relh_reltype(cd, refrelh) == RS_RELTYPE_MAINMEMORY) {
            ss_dprintf_2(("tb_dd_setmmerefkeyinfo:rs_key_setrefmme\n"));
            rs_key_setrefmme(cd, key, TRUE);
            if (rs_relh_isglobaltemporary(cd, refrelh)) {
                rs_key_setreftemporary(cd, key, TRUE);
            }
        }
}

/*#***********************************************************************\
 *
 *              tb_dd_find_depending_rels
 *
 * Build a list of relids having direct or indirect cascading update effect
 * on given table (specified by relid).
 *
 * Parameters :
 *
 *      tcon - 
 *
 *      relid - 
 *
 *      rel_list -
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 *
 */
bool tb_dd_find_depending_rels(
        TliConnectT*    tcon,
        long            relid,
        su_pa_t*        rel_list,
        rs_err_t**      p_errh)
{
        uint            itno = 0;
        TliCursorT*     tcur;
        TliRetT         trc;

        ss_dprintf_1(("tb_dd_find_depending_rels: enter with relid = %d\n",
                      relid));

        while (1) {
            int     action;
            int     refkeyid;

            ss_dprintf_1(("tb_dd_find_depending_rels: loop with relid = %d\n",
                           relid));

            tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
            ss_assert(tcur != NULL);

            trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_KEY_ACTION, &action);
            ss_assert(trc == TLI_RC_SUCC);
            trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_REF_KEY_ID, &refkeyid);
            ss_assert(trc == TLI_RC_SUCC);
            trc = TliCursorConstrLong(
                    tcur,
                    RS_ANAME_FORKEYS_REF_REL_ID,
                    TLI_RELOP_EQUAL,
                    relid);
            ss_assert(trc == TLI_RC_SUCC);
            trc = TliCursorConstrLong(
                    tcur,
                    RS_ANAME_FORKEYS_REFTYPE,
                    TLI_RELOP_EQUAL,
                    RS_KEY_PRIMKEYCHK);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorOpen(tcur);
            ss_assert(trc == TLI_RC_SUCC);

            while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                uint            i;
                int             counter;
                void*           frrelid;
                int             refrelid;
                TliCursorT*     tcur2;

                if (((action>>8)-1   == SQL_REFACT_NOACTION ||
                     (action>>8)-1   == SQL_REFACT_RESTRICT) &&
                    ((action&0xFF)-1 == SQL_REFACT_NOACTION ||
                     (action&0xFF)-1 == SQL_REFACT_RESTRICT))
                {
                    continue;
                }

                tcur2 = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);

                trc = TliCursorColInt(tcur2, RS_ANAME_KEYS_REL_ID, &refrelid);
                ss_dassert(trc == TLI_RC_SUCC);

                trc = TliCursorConstrLong(
                    tcur2,
                    RS_ANAME_KEYS_ID,
                    TLI_RELOP_EQUAL,
                    refkeyid);
                ss_dassert(trc == TLI_RC_SUCC);

                trc = TliCursorOpen(tcur2);
                ss_assert(trc == TLI_RC_SUCC);

                trc = TliCursorNext(tcur2);
                ss_assert(trc == TLI_RC_SUCC);

                trc = TliCursorNext(tcur2);
                ss_assert(trc == TLI_RC_END);

                TliCursorFree(tcur2);

                counter = 0;
                su_pa_do_get(rel_list, i, frrelid) {
                    long rrelid = (long) frrelid;
                    if (rrelid == refrelid) {
                        counter++;
                    }
                }

                ss_dprintf_1(("tb_dd_find_depending_rels: inserting %d with counter %d\n",
                              refrelid, counter));

                if (counter < 2) {
                    su_pa_insert(rel_list, (void*)refrelid);
                }
            }

            if (trc != TLI_RC_END) {
                bool succp;
                succp = TliCursorCopySuErr(tcur, p_errh);
                ss_dassert(succp);
                TliCursorFree(tcur);
                return FALSE;
            }

            TliCursorFree(tcur);

            if (itno >= su_pa_nelems(rel_list)) {
                ss_dassert(itno == su_pa_nelems(rel_list));
                break;
            }

            relid = (long)su_pa_getdata(rel_list, itno);
            itno++;
        }

        ss_dprintf_1(("tb_dd_find_depending_rels: exiting\n"));

        return TRUE;
}

/*#***********************************************************************\
 *
 *              tb_dd_find_dependent_rels
 *
 * Build a list of relids that are directly or indirectly updated with
 * cascading changes from given table (specified by relid).
 *
 * Parameters :
 *
 *      tcon - 
 *
 *      relid - 
 *
 *      rel_list -
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 *
 */
bool tb_dd_find_dependent_rels(
        TliConnectT*    tcon,
        long            relid,
        su_pa_t*        rel_list,
        rs_err_t**      p_errh)
{
        uint            itno = 0;
        TliCursorT*     tcur;
        TliRetT         trc;

        ss_dprintf_1(("tb_dd_find_dependent_rels: enter with relid = %d\n",
                      relid));

        while (1) {
            int     refrelid;
            int     action;

            ss_dprintf_1(("tb_dd_find_dependent_rels: relid = %d\n", relid));

            tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
            ss_assert(tcur != NULL);

            trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_KEY_ACTION, &action);
            ss_assert(trc == TLI_RC_SUCC);
            trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_REF_REL_ID, &refrelid);
            ss_assert(trc == TLI_RC_SUCC);
            trc = TliCursorConstrLong(
                    tcur,
                    RS_ANAME_FORKEYS_CREATE_REL_ID,
                    TLI_RELOP_EQUAL,
                    relid);
            ss_assert(trc == TLI_RC_SUCC);
            trc = TliCursorConstrLong(
                    tcur,
                    RS_ANAME_FORKEYS_REFTYPE,
                    TLI_RELOP_EQUAL,
                    RS_KEY_PRIMKEYCHK);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorOpen(tcur);
            ss_assert(trc == TLI_RC_SUCC);

            while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                uint    i;
                int     counter;
                void*   frrelid;

                if (((action>>8)-1 == SQL_REFACT_NOACTION ||
                     (action>>8)-1 == SQL_REFACT_RESTRICT) &&
                    ((action&0xFF)-1 == SQL_REFACT_NOACTION ||
                     (action&0xFF)-1 == SQL_REFACT_RESTRICT))
                {
                    continue;
                }

                counter = 0;
                su_pa_do_get(rel_list, i, frrelid) {
                    long rrelid = (long)frrelid;
                    if (rrelid == refrelid) {
                        counter++;
                    }
                }
                ss_dprintf_1(("tb_dd_find_dependent_rels: counter for %d = %d\n",
                              refrelid, counter));

                if (counter < 2) {
                    su_pa_insert(rel_list, (void*)refrelid);
                    ss_dprintf_1(("tb_dd_find_dependent_rels: inserting %d\n",
                                  refrelid));
                } else {
                    /* Should never happen. Existing loop dependency. */
                    ss_error;
                }
            }

            if (trc != TLI_RC_END) {
                bool succp;
                succp = TliCursorCopySuErr(tcur, p_errh);
                ss_dassert(succp);
                TliCursorFree(tcur);
                return FALSE;
            }

            TliCursorFree(tcur);

            if (itno >= su_pa_nelems(rel_list)) {
                ss_dassert(itno == su_pa_nelems(rel_list));
                break;
            }

            relid = (long) su_pa_getdata(rel_list, itno);
            itno++;
        }

        ss_dprintf_1(("tb_dd_find_dependent_rels: exit\n"));

        return TRUE;
}

#ifndef SS_NODDUPDATE

/*#***********************************************************************\
 *
 *              dd_isforkeyref
 *
 * Checks if there is a foreign key reference to a given table.
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      p_isforkeyref -
 *
 *
 *      p_errh -
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
static bool dd_isforkeyref(
        TliConnectT* tcon,
        rs_relh_t* relh,
        bool* p_isforkeyref,
        rs_err_t** p_errh)
{
        TliCursorT* tcur;
        TliRetT trc;
        long relid;
        long keyid;
        bool succp;
        rs_sysinfo_t* cd;

        cd = TliGetCd(tcon);
        relid = rs_relh_relid(cd, relh);

        ss_dprintf_3(("dd_isforkeyref:relname=%s,relid=%ld\n",
            rs_relh_name(cd, relh), rs_relh_relid(cd, relh)));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_KEY_ID, &keyid);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REF_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_CREATE_REL_ID,
                TLI_RELOP_NOTEQUAL,
                relid);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);

        if (trc == TLI_RC_SUCC || trc == TLI_RC_END) {
            *p_isforkeyref = (trc == TLI_RC_SUCC);
            succp = TRUE;
        } else {
            succp = TliCursorCopySuErr(tcur, p_errh);
            ss_bassert(succp);
            succp = FALSE;
        }

        TliCursorFree(tcur);

        return(succp);
}

#endif /* SS_NODDUPDATE */

#endif /* REFERENTIAL_INTEGRITY */


/*##**********************************************************************\
 *
 *              tb_dd_readcardinal
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tb_dd_readcardinal(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_cardin_t* cardin)
{
        TliCursorT* tcur;
        TliRetT trc;
        ss_int8_t ntuples;
        ss_int8_t nbytes;
        rs_sysinfo_t* cd = TliGetCd(tcon);

        ss_dprintf_1(("tb_dd_readcardinal:relid=%ld\n", rs_relh_relid(cd, relh)));

        if (cardin != NULL) {
            ss_dprintf_2(("tb_dd_readcardinal:set old cardin\n"));
            rs_relh_setcardin(cd, relh, cardin);
            return;
        }

#ifndef SS_MYSQL
        tb_sync_readsubsccount(cd, TliGetTrans(tcon), relh);
#endif /* !SS_MYSQL */

#ifdef SS_HSBG2

        /*
         * This problem is visible only in HSBG2. The problem happens
         * when relation buffer is refreshed (old one is released,
         * new one is created from scratch). We then try to read the
         * cardinality for SYS_CARDINAL table, it causes infinite
         * recursion.
         *
         * There was another similar problem related to conversion of
         * databases to new ones, because they rely heavily on ALTER TABLE
         * functions and each alter table removed the entry from
         * relation buffer. That was fixed by Tommi by avoiding removal
         * of SYS_CARDINAL table from relbuf.
         *
         * The values below means that we always use same cardinality
         * values for SYS_CARDINAL. In practice the values should be
         * meaningless, because all queries to SYS_CARDINAL are
         * simple queries without joins.
         *
         * // mikko 10.10.02
         *
         */

        if(rs_relh_relid(cd, relh) == RS_RELID_CARDINAL) {
            SsInt8SetUint4(&ntuples, 100);
            SsInt8SetUint4(&nbytes, 10000);
            rs_relh_setcardinal(cd, relh, ntuples, nbytes);
            return;
        }

#endif /* SS_HSBG2 */

#if defined(SS_MME) && defined(MME_ALTERNATE_CARDINALITY) && !defined(MME_ALTERNATE_CARDINALITY_FIX)
        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            /* MME keeps cardinality internally. */
            SsInt8Set0(&ntuples);
            SsInt8Set0(&nbytes);
            rs_relh_setcardinal(cd, relh, ntuples, nbytes);
            return;
        }
#endif

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CARDINAL);
        ss_dassert(tcur != NULL);

        trc = TliCursorColInt8t(tcur, RS_ANAME_CARDINAL_CARDIN, &ntuples);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt8t(tcur, RS_ANAME_CARDINAL_SIZE, &nbytes);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_CARDINAL_REL_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        if (TliCursorNext(tcur) == TLI_RC_SUCC) {
#ifndef AUTOTEST_RUN
            ss_dassert(!SsInt8IsNegative(ntuples));
            ss_dassert(!SsInt8IsNegative(nbytes));
#endif
            rs_relh_setcardinal(cd, relh, ntuples, nbytes);
        }

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *      dd_readoriginaldefault
 *
 * <function description>
 *
 * Parameters:
 *      tcon - <usage>
 *          <description>
 *
 *      colid - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void dd_readoriginaldefault(
        TliConnectT*    tcon,
        ulong           colid,
        rs_atype_t*     atype)
{
        TliCursorT*     tcur;
        TliRetT         trc;
        rs_sysinfo_t*   cd = TliGetCd(tcon);
        rs_aval_t*      originaldefault;
        rs_atype_t*     defaultatype;
        rs_aval_t*      aval;

        ss_dprintf_3(("dd_readoriginaldefault\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS_AUX);
        if (tcur == NULL) {
            /* The table doesn't yet exist while booting. */
            return;
        }

        defaultatype = rs_atype_init_sqldt(cd, RSSQLDT_WVARCHAR);
        originaldefault = rs_aval_create(cd, defaultatype);
        
        trc = TliCursorColAval(tcur, RS_ANAME_COLUMNS_AUX_ORIGINAL_DEFAULT,
                               defaultatype, originaldefault);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_AUX_ID,
                TLI_RELOP_EQUAL,
                colid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        if (trc == TLI_RC_SUCC) {
            if (!rs_aval_isnull(cd, defaultatype, originaldefault)) {
                RS_AVALRET_T    r;
                
                aval = rs_aval_create(cd, atype);
                r = rs_aval_convert_ext(
                        cd,
                        atype,
                        aval,
                        defaultatype,
                        originaldefault,
                        NULL);
                su_rc_bassert(r == RSAVR_SUCCESS, r);
                rs_atype_insertoriginaldefault(cd, atype, aval);
            }
            ss_debug(trc = TliCursorNext(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        rs_aval_free(cd, defaultatype, originaldefault);
        rs_atype_free(cd, defaultatype);
}

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
/*#***********************************************************************\
 *
 *      dd_readautoincseqid
 *
 * Read auto_increment sequence identifier from SYS_COLUMNS_AUX
 *
 * Parameters:
 *      tcon - <usage>
 *          <description>
 *
 *      colid - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void dd_readautoincseqid(
        TliConnectT*    tcon,
        ulong           colid,
        rs_atype_t*     atype)
{
        TliCursorT*     tcur;
        TliRetT         trc;
        rs_sysinfo_t*   cd = TliGetCd(tcon);
        long            seq_id = 0;
        long            mysqldatatype = 0;

        ss_dprintf_3(("dd_readautoincseqid\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS_AUX);

        if (tcur == NULL) {
            /* The table doesn't yet exist while booting. */
            return;
        }
        
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_AUTO_INC_SEQ_ID, &seq_id);
        ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_ERR_COLNAMEILL, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_EXTERNAL_DATA_TYPE, &mysqldatatype);
        ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_ERR_COLNAMEILL, TliCursorErrorCode(tcur));

        if (trc == TLI_RC_SUCC ) {
            /* Column exists on table. During recovery before conversions the 
             * column does not exist yet.
             */
            trc = TliCursorConstrLong(
                    tcur,
                    RS_ANAME_COLUMNS_AUX_ID,
                    TLI_RELOP_EQUAL,
                    colid);

            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            trc = TliCursorOpen(tcur);

            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            trc = TliCursorNext(tcur);
            
            if (trc == TLI_RC_SUCC) {
                if (seq_id != 0) {
                    rs_atype_setautoinc(cd, atype, TRUE, seq_id);
                } else {
                    rs_atype_setautoinc(cd, atype, FALSE, seq_id);
                }
                rs_atype_setmysqldatatype(cd, atype, (rs_mysqldatatype_t)mysqldatatype);
                
                ss_debug(trc = TliCursorNext(tcur));
            }

            ss_dassert(trc == TLI_RC_END);
        }

        TliCursorFree(tcur);
}
#endif /* SS_MYSQL || SS_MYSQL_AC */

#ifdef SS_COLLATION

/*#***********************************************************************\
 *
 *      dd_readexternalcollation
 *
 * <function description>
 *
 * Parameters:
 *      tcon - <usage>
 *          <description>
 *
 *      colid - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void dd_readexternalcollation(
        TliConnectT*    tcon,
        ulong           colid,
        rs_atype_t*     atype)
{
        TliCursorT*     tcur;
        TliRetT         trc;
        rs_sysinfo_t*   cd = TliGetCd(tcon);
        char*           collation_name;

        ss_dprintf_3(("dd_readexternalcollation\n"));

        if (tb_dd_collation_init == NULL) {
            ss_dprintf_4(("dd_readexternalcollation:tb_dd_collation_init is NULL\n"));
            return;
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS_AUX);
        if (tcur == NULL) {
            /* The table doesn't yet exist while booting. */
            return;
        }

        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_AUX_EXTERNAL_COLLATION, &collation_name);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_AUX_ID,
                TLI_RELOP_EQUAL,
                colid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        if (trc == TLI_RC_SUCC) {
            if (collation_name != NULL) {
                su_collation_t* collation;
                collation = (*tb_dd_collation_init)(collation_name, 0);
                rs_atype_setcollation(cd, atype, collation);
            }
            ss_debug(trc = TliCursorNext(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

#endif /* SS_COLLATION */

/*#***********************************************************************\
 *
 *              dd_readttype
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_ttype_t* dd_readttype(
        TliConnectT*    tcon,
        ulong           relid,
        bool            readoriginaldefault)
{
        TliCursorT* tcur;
        TliRetT trc;
        int i;
        long attrid;
        int attrtype;
        int sqldatatype;
        int datatype;
        long charmaxlen;
        long len;
        int scale;
        char* nullallowed;
        char* attrname;
        rs_ttype_t* ttype;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        rs_aval_t*      currentdefault;
        rs_atype_t*     defaultatype;

        ss_dprintf_3(("dd_readttype\n"));
        SS_PUSHNAME("dd_readttype");

        defaultatype = rs_atype_init_sqldt(cd, RSSQLDT_WVARCHAR);
        currentdefault = rs_aval_create(cd, defaultatype);
        
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_ID, &attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_ATTR_TYPE, &attrtype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_SQLDTYPE_NO, &sqldatatype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_DATA_TYPE_NO, &datatype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_CHAR_MAX_LEN, &charmaxlen);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_NUM_PRECISION, &len);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NUM_SCALE, &scale);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_NULLABLE, &nullallowed);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_COLUMN_NAME, &attrname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColAval(tcur, RS_ANAME_COLUMNS_DEFAULT_VAL,
                               defaultatype, currentdefault);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        ttype = rs_ttype_create(cd);

        for (i = 0; (trc = TliCursorNext(tcur)) == TLI_RC_SUCC; i++) {
            rs_atype_t*     atype;
            rs_aval_t*      aval;

            if (datatype == RSDT_CHAR
#ifdef SS_UNICODE_DATA
            ||  datatype == RSDT_BINARY
            ||  datatype == RSDT_UNICODE
#endif /* SS_UNICODE_DATA */
            ) {
                len = charmaxlen;
            } else {
                if (TliCursorColIsNULL(tcur, RS_ANAME_COLUMNS_NUM_PRECISION)) {
                    len = RS_LENGTH_NULL;
                }
                if (TliCursorColIsNULL(tcur, RS_ANAME_COLUMNS_NUM_SCALE)) {
                    scale = RS_SCALE_NULL;
                }
            }
            atype = rs_atype_init(
                        cd,
                        attrtype,
                        datatype,
                        sqldatatype,
                        len,
                        scale,
                        dd_getboolnum(nullallowed));
            if (!rs_aval_isnull(cd, defaultatype, currentdefault)) {
                RS_AVALRET_T    r;
                
                aval = rs_aval_create(cd, atype);
                r = rs_aval_convert_ext(
                        cd,
                        atype,
                        aval,
                        defaultatype,
                        currentdefault,
                        NULL);
                su_rc_bassert(r == RSAVR_SUCCESS, r);
                rs_atype_insertcurrentdefault(cd, atype, aval);
            }
            
            /* Don't read SYS_COLUMNS_AUX for system tables, or we'll
               end up with an infinite recursion. */

            if (readoriginaldefault) {
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                dd_readautoincseqid(tcon, attrid, atype);
#endif
                dd_readoriginaldefault(tcon, attrid, atype);
#ifdef SS_COLLATION
                dd_readexternalcollation(tcon, attrid, atype);
#endif /* SS_COLLATION */
            }

            rs_ttype_setatype(cd, ttype, i, atype);
            rs_atype_free(cd, atype);
            rs_ttype_setattrid(cd, ttype, i, attrid);
            rs_ttype_setaname(cd, ttype, i, attrname);

        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        rs_aval_free(cd, defaultatype, currentdefault);
        rs_atype_free(cd, defaultatype);

        SS_POPNAME;

        return(ttype);
}

/*#***********************************************************************\
 *
 *              dd_readrelmode
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
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
static void dd_readrelmode(
        TliConnectT* tcon,
        rs_relh_t* relh)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* relmode;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        long relid;

        ss_dprintf_3(("dd_readrelmode\n"));

        relid = rs_relh_relid(cd, relh);

        if (rs_relh_issysrel(cd, relh)) {

            /* Some Flow system tables can be nocheck 
             */

            /* This should be (atleast) configurable

            char *relname = rs_relh_name(cd, relh);

            ss_dprintf_1(("dd_readrelmode:%s\n", relname));
            if (SsStrcmp(relname, RS_RELNAME_SYNC_RECEIVED_STMTS) == 0) {
                rs_relh_setnocheck(cd, relh);
                ss_dprintf_1(("dd_readrelmode:set nocheck\n"));
            }
            */

            /* System tables are always optimistic.
             */
            return;
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLEMODES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TABLEMODES_MODE, &relmode);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLEMODES_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (trc == TLI_RC_SUCC) {
            /* Mode found.
             */
#ifndef SS_NOLOCKING
            if (dd_findrelmodestr(relmode, (char *)DD_RELMODE_PESSIMISTIC) != NULL) {
                rs_relh_setreltype(cd, relh, RS_RELTYPE_PESSIMISTIC);
            }
#endif /* SS_NOLOCKING */
            if (dd_findrelmodestr(relmode, (char *)DD_RELMODE_NOCHECK) != NULL
            &&  !rs_sysi_ishsbconfigured(cd))
            {
                rs_relh_setnocheck(cd, relh);
            }
            if (dd_findrelmodestr(relmode, (char *)DD_RELMODE_MAINMEMORY) != NULL) {
                rs_relh_setreltype(cd, relh, RS_RELTYPE_MAINMEMORY);
            }
#ifdef SS_SYNC
            if (dd_findrelmodestr(relmode, (char *)DD_RELMODE_SYNC) != NULL) {
                if (ss_vers_issync()) {
                    rs_relh_setsync(cd, relh, TRUE);
                } else {
                    rs_relh_setreadonly(cd, relh);
                }
            }
#endif /* SS_SYNC */
            if (dd_findrelmodestr(relmode, (char *)DD_RELMODE_TRANSIENT) != NULL) {
                rs_relh_settransient(cd, relh, TRUE);
            }
            if (dd_findrelmodestr(relmode, (char *)DD_RELMODE_GLOBALTEMPORARY) != NULL) {
                rs_relh_setglobaltemporary(cd, relh, TRUE);
            }
        } else {
            /* Mode not found, use default.
             */
            dbe_db_t* db;
            ss_dassert(trc == TLI_RC_END);
            db = TliGetDb(tcon);
#ifndef SS_NOLOCKING
            if (dbe_db_ispessimistic(db) && rs_relh_isbasetable(cd, relh)) {
                rs_relh_setreltype(cd, relh, RS_RELTYPE_PESSIMISTIC);
            }
#endif /* SS_NOLOCKING */
        }

        TliCursorFree(tcur);
}

#ifndef SS_HSBG2
static void dd_setrowlevelhsb(
        rs_sysinfo_t* cd,
        rs_relh_t* relh)
{
        int i;
        char* relname;

        relname = rs_relh_name(cd, relh);
        for (i = 0; dd_rowlevelhsbsysrels[i] != NULL; i++) {
            if (strcmp(dd_rowlevelhsbsysrels[i], relname) == 0) {
                rs_relh_setrowlevelhsb(cd, relh, TRUE);
                return;
            }
        }
}
#endif /* !SS_HSBG2 */

/*#***********************************************************************\
 *
 *              dd_readrelh
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_relh_t* dd_readrelh(
        TliConnectT*    tcon,
        ulong           relid,
        dd_readrelh_t   readrelh_type,
        bool            dolock,
        tb_trans_t*     usertrans,
        dbe_ret_t*      p_rc)
{
        TliCursorT*     tcur;
        TliRetT         trc;
        char*           relname;
        char*           reltype;
        char*           schema;
        char*           catalog;

        char*           checkstr;
        rs_relh_t*      relh;
        rs_ttype_t*     ttype;
        rs_sysinfo_t*   cd = TliGetCd(tcon);
        rs_entname_t    en;
        bool            basetable;
        bool            historytable;
        su_ret_t        rc = SU_SUCCESS;
        char*           cons;
        char*           consname;

        ss_dprintf_3(("dd_readrelh:relid=%ld\n", relid));
        ss_rc_dassert(relid != RS_RELID_KEYS, relid);

        tb_trans_beginif(cd, TliGetTrans(tcon));
        if (p_rc != NULL) {
            *p_rc = SU_SUCCESS;
            if (dolock) {
                ss_dassert(usertrans != NULL);
                rc = dbe_trx_lockrelid(
                        tb_trans_dbtrx(cd, usertrans),
                        relid, FALSE, 0L);
                if (rc != SU_SUCCESS) {
                    *p_rc = rc;

                    return NULL;
                }
            }
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
            /* #ifdef added to allow MySQL access a table from prepared statement
               after the table has been changed. MySQL executes any DDL operation,
               including creating and dropping secondary indices, by creating
               a modified table copy and pumping data into it.
             */
            rc = SU_SUCCESS;
#else
            rc = dbe_trx_readrelh(
                    tb_trans_dbtrx(cd, TliGetTrans(tcon)),
                    relid);
#endif
            if (rc != SU_SUCCESS) {
                *p_rc = rc;
                if (dolock) {
                    dbe_trx_unlockrelid(tb_trans_dbtrx(cd, usertrans), relid);
                }
                return NULL;
            }
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &relname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_TYPE, &reltype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CHECK, &checkstr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLES_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, trc);

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, trc);

        trc = TliCursorNext(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END, trc);

        if (trc != TLI_RC_SUCC ||
            (!ss_vers_issync() && strcmp(reltype, RS_AVAL_SYNC_TABLE) == 0))
        {
            uint errcode = TliCursorErrorCode(tcur);
            ss_dprintf_4(("dd_readrelh:trc=%d\n", trc));
            TliCursorFree(tcur);
            ss_dprintf_4(("dd_readrelh:relh=NULL\n"));

            if (p_rc != NULL) {
                if (errcode == SU_SUCCESS) {
                    errcode = E_RELNOTEXIST_S;
                }
                *p_rc = errcode;
            }
            if (dolock) {
                dbe_trx_unlockrelid(tb_trans_dbtrx(cd, usertrans), relid);
            }
            return(NULL);
        }

        ttype = dd_readttype(
                tcon,
                relid,
                /* Don't read SYS_COLUMNS_AUX for system tables, or we'll
                   end up with an infinite recursion. */
                SsStrcmp(schema, RS_AVAL_SYSNAME) != 0);
        rs_ttype_addpseudoatypes(cd, ttype);

        ss_dassert(strcmp(reltype, RS_AVAL_BASE_TABLE) == 0 ||
                   strcmp(reltype, RS_AVAL_SYNC_TABLE) == 0 ||
                   strcmp(reltype, RS_AVAL_TRUNCATE_TABLE) == 0);

        ss_debug(rs_ttype_setname(cd, ttype, relname));
        ss_dassert(schema != NULL);
        rs_entname_initbuf(
                &en,
                catalog,
                schema,
                relname);
        relh = rs_relh_init(
                    cd,
                    &en,
                    relid,
                    ttype);
        SS_MEM_SETLINK(relh);

        rs_relh_setreadonlyttype(cd, relh);

        rs_ttype_free(cd, ttype);

        basetable = (strcmp(reltype, RS_AVAL_BASE_TABLE) == 0);
        historytable = (strcmp(reltype, RS_AVAL_SYNC_TABLE) == 0);

        rs_relh_setbasetable(
            cd,
            relh,
            basetable);
#ifndef SS_HSBG2
        if (basetable) {
            dd_setrowlevelhsb(cd, relh);
        }
#endif /* !SS_HSBG2 */

        rs_relh_sethistorytable(
            cd,
            relh,
            historytable);

        ss_dassert(checkstr == NULL);  /* After conversion in dd_modifycheck */

        TliCursorFree(tcur);
        tcur = NULL;

        /* Check to avoid infinite recursion when reading
         * SYS_CHECKSTRINGS relh.
         */
        if (strcmp(RS_RELNAME_CHECKSTRINGS, rs_relh_name(cd, relh))!=0 ||
            strcmp(RS_AVAL_SYSNAME, rs_relh_schema(cd, relh))!=0)
        {
            tcur = TliCursorCreate(tcon,
                                   RS_AVAL_DEFCATALOG,
                                   RS_AVAL_SYSNAME,
                                   RS_RELNAME_CHECKSTRINGS);
        }

        if (tcur!=NULL) {
            /* tcur is NULL only during system schema update */
            ss_assert(tcur);
            trc = TliCursorColStr(tcur, RS_ANAME_CHECKSTRINGS_CONSTRAINT, &cons);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_CHECKSTRINGS_NAME, &consname);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            trc = TliCursorConstrLong(tcur, RS_ANAME_CHECKSTRINGS_REL_ID,
                                      TLI_RELOP_EQUAL, relid);
            ss_rc_dassert(trc == TLI_RC_SUCC, trc);

            trc = TliCursorOpen(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC, trc);
            while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                rs_relh_addcheckstring(cd, relh, cons, consname);
            }
            ss_rc_dassert(trc == TLI_RC_END, trc);
            TliCursorFree(tcur);
        }

        dd_readrelmode(tcon, relh);

        if (readrelh_type != DD_READRELH_MINIMAL) {
            ss_dassert(readrelh_type == DD_READRELH_KEYS || readrelh_type == DD_READRELH_FULL);
            dd_readkeys(tcon, relh);
        }
        if (readrelh_type == DD_READRELH_FULL) {
            rs_cardin_t* cardin;

            cardin = rs_rbuf_getcardin(cd, rs_sysi_rbuf(cd), rs_relh_entname(cd, relh));

            tb_dd_readcardinal(tcon, relh, cardin);

            if (cardin != NULL) {
                rs_cardin_done(cd, cardin);
            }

            if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
                if (!dbe_db_ismme(rs_sysi_db(cd))) {
                    rc = E_MMENOSUP;
                    goto exit_function;
                }
                if (!su_li3_ismainmemsupp()) {
                    rc = E_MMENOLICENSE;
                    goto exit_function;
                }
            }

#ifdef REFERENTIAL_INTEGRITY
            rc = dd_readrefkeys(tcon, relh);
            if (rc != SU_SUCCESS) {
                goto exit_function;
            }
            rc = tb_dd_resolverefkeys(cd, relh);
            if (rc != SU_SUCCESS) {
                goto exit_function;
            }
#endif

#ifndef SS_NOPROCEDURE
#ifndef SS_MYSQL
            tb_trig_findall(cd, TliGetTrans(tcon), relh);
#endif /* !SS_MYSQL */
#endif

#ifdef SS_SYNC
#ifndef SS_MYSQL
            tb_hcol_initbyrelh(cd, TliGetTrans(tcon), relh);
            /* tb_sync_readsubsccount(cd, TliGetTrans(tcon), relh); */
#endif /* !SS_MYSQL */
#endif
        }

 exit_function:
        if (rc != SU_SUCCESS) {
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);
            relh = NULL;

            if (p_rc != NULL) {
                *p_rc = rc;
            }
        }
        if (dolock && relh == NULL) {
            dbe_trx_unlockrelid(tb_trans_dbtrx(cd, usertrans), relid);
        }

        return relh;
}

/*##**********************************************************************\
 *
 *              tb_dd_readrelh_norbufupdate
 *
 * Reads relation handle in given transaction.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relid -
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
rs_relh_t* tb_dd_readrelh_norbufupdate(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        ulong relid)
{
        rs_relh_t* relh;
        TliConnectT* tcon;

        ss_dprintf_1(("tb_dd_readrelh_norbufupdate\n"));

        tcon = TliConnectInitByTrans(cd, trans);

        relh = dd_readrelh(tcon, relid, DD_READRELH_FULL, FALSE, NULL, NULL);

        TliConnectDone(tcon);

        ss_dprintf_2(("tb_dd_readrelh_norbufupdate:relh=%ld\n", (long)relh));

        return(relh);
}

#ifndef SS_NODDUPDATE

#ifdef SS_COLLATION
static void dd_insert_to_keyparts_aux(
        TliConnectT* tcon,
        rs_relh_t* relh __attribute__ ((unused)),
        long keyid,
        long kpno,
        int prefix_len)
{
        TliCursorT* tcur;
        TliRetT trc;

        ss_dassert(prefix_len > 0);
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS_AUX);
        ss_dassert(tcur != NULL);
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_AUX_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_AUX_KEYP_NO, &kpno);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_AUX_PREFIX_LENGTH,
                              &prefix_len);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorInsert(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        TliCursorFree(tcur);
}
#endif /* SS_COLLATION */

/*#***********************************************************************\
 *
 *              dd_createkeyparts
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      key -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_createkeyparts(
        TliConnectT* tcon,
        rs_relh_t* relh __attribute__ ((unused)),
        long relid,
        rs_ttype_t* ttype,
        rs_key_t* key,
        tb_dd_createrel_t type)
{
        TliCursorT* tcur;
        TliRetT trc;
        int kpno;
        long keyid;
        int kptype;
        long attrid;
        char* ascstr;
        int ano;
        int nparts;
        va_t relid_va;
        va_t keyid_va;
        va_t* constvalue;
        rs_sysinfo_t* cd = TliGetCd(tcon);

        ss_dprintf_3(("dd_createkeyparts\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_REL_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_KEYP_NO, &kpno);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ATTR_ID, &attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_ATTR_NO, &ano);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_ATTR_TYPE, &kptype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYPARTS_ASCENDING, &ascstr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColVa(tcur, RS_ANAME_KEYPARTS_CONST_VALUE, &constvalue);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        nparts = rs_key_nparts(cd, key);
        keyid = rs_key_id(cd, key);
        va_setlong(&relid_va, relid);
        va_setlong(&keyid_va, keyid);

        for (kpno = 0; kpno < nparts; kpno++) {

            kptype = rs_keyp_parttype(cd, key, kpno);
            ascstr = dd_getboolstr(rs_keyp_isascending(cd, key, kpno));
            ano = rs_keyp_ano(cd, key, kpno);
            if (ano == RS_ANO_NULL) {
                attrid = (ulong)-1;
            } else {
                attrid = rs_ttype_attrid(cd, ttype, ano);
            }

            if (type == TB_DD_CREATEREL_SYSTEM) {
                if (rs_keyp_isconstvalue(cd, key, kpno)) {
                    constvalue = rs_keyp_constvalue(cd, key, kpno);
                } else {
                    constvalue = NULL;
                }
            } else {
                /* Update the constant key parts in the key. */
                switch (kptype) {
                    case RSAT_RELATION_ID:
                        rs_keyp_setconstvalue(cd, key, kpno, &relid_va);
                        constvalue = &relid_va;
                        break;
                    case RSAT_KEY_ID:
                        rs_keyp_setconstvalue(cd, key, kpno, &keyid_va);
                        constvalue = &keyid_va;
                        break;
                    default:
                        constvalue = NULL;
                        break;
                }
            }

            ss_dprintf_4(("dd_createkeyparts:relid=%ld, keyid=%ld, kpno = %d\n",
                relid, keyid, kpno));

            trc = TliCursorInsert(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

#ifdef SS_COLLATION
            if (kptype == RSAT_COLLATION_KEY) {
                int prefix_length = rs_keyp_getprefixlength(cd, key, kpno);
                if (prefix_length > 0) {
                    dd_insert_to_keyparts_aux(tcon,
                                              relh,
                                              keyid,
                                              kpno,
                                              prefix_length);
                }
            }
#endif /* SS_COLLATION */
        }

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              td_dd_createonekey
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      key -
 *
 *
 *      auth -
 *
 *
 *      type -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void td_dd_createonekey(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_ttype_t* ttype,
        rs_key_t* key,
        rs_auth_t* auth __attribute__ ((unused)),
        tb_dd_createrel_t type)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* keyname;
        long relid;
        long keyid;
        char* unique;
        int nonunique_odbc;
        char* clustering;
        char* primary;
        char* prejoined;
        int nordering;
        char* schema;
        char* catalog;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        dbe_db_t* db = TliGetDb(tcon);

        ss_dprintf_3(("td_dd_createonekey\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_NAME, &keyname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_REL_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_UNIQUE, &unique);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYS_KEY_NONUNQ_ODBC, &nonunique_odbc);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CLUSTERING, &clustering);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_PRIMARY, &primary);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_PREJOINED, &prejoined);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYS_NREF, &nordering);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_SCHEMA, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        relid = rs_relh_relid(cd, relh);
        if (type == TB_DD_CREATEREL_SYSTEM) {
            keyid = rs_key_id(cd, key);
        } else {
            keyid = dbe_db_getnewkeyid_log(db);
            rs_key_setid(cd, key, keyid);
            ss_dprintf_4(("td_dd_createonekey:rs_key_setid, keyid=%ld\n", keyid));
        }
        keyname = rs_key_name(cd, key);
        nonunique_odbc = !rs_key_isunique(cd, key);
        unique = dd_getboolstr(!nonunique_odbc);
        clustering = dd_getboolstr(rs_key_isclustering(cd, key));
        primary = dd_getboolstr(rs_key_isprimary(cd, key));
        prejoined = dd_getboolstr(rs_key_isprejoined(cd, key));
        nordering = rs_key_lastordering(cd, key) + 1;
        schema = rs_relh_schema(cd, relh);
        ss_dassert(schema != NULL);
        catalog = rs_relh_catalog(cd, relh);

        ss_dprintf_4(("td_dd_createonekey:relid=%ld, keyid=%ld\n", relid, keyid));

        trc = TliCursorInsert(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        dd_createkeyparts(tcon, relh, rs_relh_relid(cd, relh), ttype, key, type);
}

/*#***********************************************************************\
 *
 *              dd_createkeys
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_createkeys(
        TliConnectT* tcon,
        rs_relh_t* relh,
        long relid,
        tb_dd_createrel_t type)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* keyname;
        char* dynkeyname = NULL;
        long keyid;
        char* unique;
        int nonunique_odbc;
        char* clustering;
        char* primary;
        char* prejoined;
        int nordering;
        char* schema;
        char* catalog;
        uint i;
        su_pa_t* keys;
        rs_key_t* key;
        rs_ttype_t* ttype;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        dbe_db_t* db = TliGetDb(tcon);

        ss_dprintf_3(("dd_createkeys\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_NAME, &keyname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_REL_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_UNIQUE, &unique);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYS_KEY_NONUNQ_ODBC, &nonunique_odbc);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CLUSTERING, &clustering);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_PRIMARY, &primary);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_PREJOINED, &prejoined);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYS_NREF, &nordering);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_SCHEMA, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        keys = rs_relh_keys(cd, relh);
        ttype = rs_relh_ttype(cd, relh);

        su_pa_do_get(keys, i, key) {
            if (type == TB_DD_CREATEREL_SYSTEM || type == TB_DD_CREATEREL_TRUNCATE) {
                keyid = rs_key_id(cd, key);
            } else {
                keyid = dbe_db_getnewkeyid_log(db);
                rs_key_setid(cd, key, keyid);
                ss_dprintf_4(("dd_createkeys:rs_key_setid, keyid=%ld\n", keyid));
            }
            keyname = rs_key_name(cd, key);
            if (type == TB_DD_CREATEREL_TRUNCATE) {
                dynkeyname = SsMemAlloc(100);
                SsSprintf(dynkeyname, "$TRUNCATEKEY %ld %ld", relid, keyid);
                keyname = dynkeyname;
            }
            nonunique_odbc = !rs_key_isunique(cd, key);
            unique = dd_getboolstr(!nonunique_odbc);
            clustering = dd_getboolstr(rs_key_isclustering(cd, key));
            primary = dd_getboolstr(rs_key_isprimary(cd, key));
            prejoined = dd_getboolstr(rs_key_isprejoined(cd, key));
            nordering = rs_key_lastordering(cd, key) + 1;
            schema = rs_relh_schema(cd, relh);
            ss_dassert(schema != NULL);
            catalog = rs_relh_catalog(cd, relh);

            ss_dprintf_4(("dd_createkeys: relid=%ld, keyid=%ld\n",
                relid, keyid));

            trc = TliCursorInsert(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

            dd_createkeyparts(tcon, relh, relid, ttype, key, type);

            if (dynkeyname != NULL) {
                SsMemFree(dynkeyname);
                dynkeyname = NULL;
            }
        }

        TliCursorFree(tcur);
}

#ifdef REFERENTIAL_INTEGRITY

/*#***********************************************************************\
 *
 *              dd_createrefkeyparts
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      key -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_createrefkeyparts(
        TliConnectT* tcon,
        rs_key_t* key,
        rs_ttype_t* ttype)
{
        TliCursorT* tcur;
        TliRetT trc;
        int kpno;
        long keyid;
        int kptype;
        int ano;
        int nparts;
        long attrid;
        va_t* constvalue;
        rs_sysinfo_t* cd = TliGetCd(tcon);
#ifdef SS_COLLATION 
        int prefix_length;
#endif /* SS_COLLATION */
        ss_dprintf_3(("dd_createrefkeyparts\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYPARTS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ID, &keyid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_KEYP_NO, &kpno);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_ATTR_NO, &ano);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ATTR_ID, &attrid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_ATTR_TYPE, &kptype);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColVa(tcur, RS_ANAME_FORKEYPARTS_CONST_VALUE, &constvalue);
        ss_assert(trc == TLI_RC_SUCC);
#ifdef SS_COLLATION
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_PREFIX_LENGTH,
                              &prefix_length);
        ss_assert(trc == TLI_RC_SUCC);
#endif /* SS_COLLATION */

        nparts = rs_key_nparts(cd, key);
        keyid = rs_key_id(cd, key);

        for (kpno = 0; kpno < nparts; kpno++) {
#ifdef SS_COLLATION
            prefix_length = rs_keyp_getprefixlength(cd, key, kpno);
            ss_dassert(prefix_length == 0 ||
                       rs_keyp_parttype(cd, key, kpno) == RSAT_COLLATION_KEY);
#endif /* SS_COLLATION */
            kptype = rs_keyp_parttype(cd, key, kpno);
            ano = rs_keyp_ano(cd, key, kpno);

            if (rs_keyp_isconstvalue(cd, key, kpno)) {
                /* Get the constant key parts in the key. */
                constvalue = rs_keyp_constvalue(cd, key, kpno);
                attrid = -1L;
            } else {
                constvalue = NULL;
                attrid = rs_ttype_attrid(cd, ttype, ano);
            }
            trc = TliCursorInsert(tcur);
            ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));
        }

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_createonerefkey
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      key -
 *
 *
 *      auth -
 *
 *
 *      type -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_createonerefkey(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_key_t* key,
        rs_ttype_t* ttype,
        long ref_relid,
        long ref_keyid,
        long create_relid,
        rs_auth_t* auth __attribute__ ((unused)))
{
        TliCursorT* tcur;
        TliRetT trc;
        long keyid;
        int nordering;
        int reftype;
        char* schema;
        char* name;
        int action;
        char* catalog;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        dbe_db_t* db = TliGetDb(tcon);

        ss_dprintf_3(("dd_createonerefkey\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, &keyid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, &ref_relid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_KEY_ID, &ref_keyid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_CREATE_REL_ID, &create_relid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_NREF, &nordering);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_REFTYPE, &reftype);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_SCHEMA, &schema);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_KEY_NAME, &name);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYS_KEY_ACTION, &action);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        keyid = dbe_db_getnewkeyid_log(db);
        rs_key_setid(cd, key, keyid);
        ss_dprintf_4(("dd_createonerefkey:rs_key_setid, keyid=%ld\n", keyid));
        reftype = rs_key_type(cd, key);
        nordering = rs_key_lastordering(cd, key) + 1;
        schema = rs_relh_schema(cd, relh);
        ss_dassert(schema != NULL);
        name = rs_key_name(cd, key);
        ss_dassert(name!=NULL);
        action = rs_key_action(cd, key);
        catalog = rs_relh_catalog(cd, relh);

        trc = TliCursorInsert(tcur);
        ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        TliCursorFree(tcur);

        dd_createrefkeyparts(tcon, key, ttype);
}

/*##**********************************************************************\
 *
 *              tb_dd_createrefkey
 *
 * Adds one reference key to the database. Reference keys are used for
 * referential integrity checks.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trans - use
 *
 *
 *      key - use
 *
 *
 *      ref_relid - in
 *
 *
 *      create_relid - in
 *
 *
 *      auth - in
 *
 *
 *      p_errh - in
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
su_ret_t tb_dd_createrefkey(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_key_t* key,
        rs_ttype_t* ttype,
        long ref_relid,
        long ref_keyid,
        long create_relid,
        rs_auth_t* auth,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        TliConnectT* tcon;
        su_ret_t rc = SU_SUCCESS;

        ss_dprintf_1(("tb_dd_createrefkey\n"));

        tcon = TliConnectInitByTrans(cd, trans);

        dd_createonerefkey(
            tcon,
            relh,
            key,
            ttype,
            ref_relid,
            ref_keyid,
            create_relid,
            auth);

        TliConnectDone(tcon);

        return(rc);
}

/*##**********************************************************************\
 *
 *              tb_dd_droprefkey
 *
 * Removes redference key from the database. Reference keys are used for
 * referential integrity checks.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trans - use
 *
 *
 *      key - use
 *
 *
 *      ref_relid - in
 *
 *
 *      create_relid - in
 *
 *
 *      auth - in
 *
 *
 *      p_errh - in
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
static su_ret_t dd_droprefkey(
        TliConnectT* tcon,
        rs_relh_t*   relh,
        char*        name,
        rs_err_t** p_errh)
{
        ulong       keyid, keyid2;
        TliCursorT* tcur;
        TliRetT     trc;
        ulong       keytype;
        rs_sysinfo_t* cd = TliGetCd(tcon);
#ifndef SS_MYSQL
        bool        found = 0;
#endif
        ulong       relid = rs_relh_relid(cd, relh);

        /* Find it in sys_forkeys one record */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, (long *)&keyid);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REFTYPE, (long *)&keytype);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REF_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REFTYPE,
                TLI_RELOP_EQUAL,
                1);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_FORKEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                name);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);
        ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        trc = TliCursorDelete(tcur);
        ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        /* drop parts */
        dd_droprefkeyparts(tcon, keyid); 

        ss_dassert (TliCursorNext(tcur) == TLI_RC_END);
        TliCursorFree(tcur);

        /* Find it in sys_keys - one record */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, (long *)&keyid);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_assert(trc == TLI_RC_SUCC);
 
        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_KEY_NAME,
                TLI_RELOP_EQUAL,
                name);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);
        ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        if (keytype == RS_KEY_FORKEYCHK) {
            su_pa_t *keys;
            rs_key_t *key;
            unsigned int i;

            dd_dropkeyparts(tcon, keyid);
            trc = TliCursorDelete(tcur);
            ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

            keys = rs_relh_keys(cd, relh);
            su_pa_do_get(keys, i, key) {
                if (rs_key_id(cd, key) == keyid) {
                    dbe_ret_t rc;

                    rc = dbe_trx_deleteindex(
                            tb_trans_dbtrx(cd, TliGetTrans(tcon)),
                            relh,
                            key);
                    if (rc != DBE_RC_SUCC) {
                        rs_error_create(p_errh, rc);
                        TliCursorFree(tcur);
                        return rc;
                    }
                    su_pa_remove(keys, i);
                    rs_key_done(cd, key);
                }
            }
        }

        ss_dassert (TliCursorNext(tcur) == TLI_RC_END);
        TliCursorFree(tcur);

            /* find it in sys_forkeys */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, (long *)&keyid2);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REF_KEY_ID,
                TLI_RELOP_EQUAL,
                keyid);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);
        ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));
        ss_dassert(keyid2 != 0);

        trc = TliCursorDelete(tcur);
        ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

                /* drop parts */
        dd_droprefkeyparts(tcon, keyid2);

        ss_dassert (TliCursorNext(tcur) == TLI_RC_END);
        TliCursorFree(tcur);

        return SU_SUCCESS;
}

su_ret_t tb_dd_droprefkey(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char*      name,
        rs_auth_t* auth __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        su_ret_t rc;

        tcon = TliConnectInitByTrans(cd, trans);
        rc = dd_droprefkey(tcon, relh, name, p_errh);
        TliConnectDone(tcon);

        return rc;
}

#endif /* REFERENTIAL_INTEGRITY */

/*#***********************************************************************\
 *
 *              dd_createnamedcheck
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      name -
 *
 *
 *      checkstring -
 *
 *
 *      auth -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static su_ret_t dd_createnamedcheck(
        TliConnectT* tcon,
        long relid,
        char* name,
        char* checkstring)
{
        TliCursorT* tcur;
        TliRetT trc;

        ss_dprintf_3(("dd_createnamedcheck\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CHECKSTRINGS);
        ss_assert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_CHECKSTRINGS_REL_ID, &relid);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_CHECKSTRINGS_NAME, &name);
        ss_assert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_CHECKSTRINGS_CONSTRAINT, &checkstring);
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorInsert(tcur);

        TliCursorFree(tcur);
        return trc==TLI_RC_SUCC ? SU_SUCCESS: E_CONSTRAINT_NAME_CONFLICT_S;
}

/*#***********************************************************************\
 *
 *              dd_hasnamedcheck
 *
 *  Checks if given relation has check constraint with given name.
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      name -
 *
 *
 *      checkstring -
 *
 *
 *      auth -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_hasnamedcheck(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* name)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        ss_dprintf_3(("dd_hasnamedcheck\n"));

        tcon = TliConnectInitByTrans(cd, trans);
        ss_assert(tcon!=NULL);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CHECKSTRINGS);
        ss_assert(tcur != NULL);

        trc = TliCursorConstrLong(
                tcur, 
                RS_ANAME_CHECKSTRINGS_REL_ID,
                TLI_RELOP_EQUAL, 
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur, 
                RS_ANAME_CHECKSTRINGS_NAME,
                TLI_RELOP_EQUAL, 
                name);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return trc==TLI_RC_SUCC;
}

/*#***********************************************************************\
 *
 *              dd_dropnamedcheck
 *
 *  Deletes named check constraint on given table.
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      name -
 *
 *
 *      checkstring -
 *
 *
 *      auth -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_dropnamedcheck(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* name)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        bool dropped;

        ss_dprintf_3(("dd_hasnmaedcheck\n"));

        tcon = TliConnectInitByTrans(cd, trans);
        ss_assert(tcon!=NULL);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CHECKSTRINGS);
        ss_assert(tcur != NULL);

        trc = TliCursorConstrLong(
                tcur, 
                RS_ANAME_CHECKSTRINGS_REL_ID,
                TLI_RELOP_EQUAL, 
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrStr(
                tcur, 
                RS_ANAME_CHECKSTRINGS_NAME,
                TLI_RELOP_EQUAL, 
                name);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        dropped = FALSE;
        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
            dropped = TRUE;
        }

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return dropped;
}

/*##**********************************************************************\
 *
 *              tb_dd_createrefkey
 *
 * Adds one reference key to the database. Reference keys are used for
 * referential integrity checks.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trans - use
 *
 *
 *      key - use
 *
 *
 *      ref_relid - in
 *
 *
 *      create_relid - in
 *
 *
 *      auth - in
 *
 *
 *      p_errh - in
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
su_ret_t tb_dd_createnamedcheck(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* name,
        char* checkstring,
        rs_auth_t* auth __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        su_ret_t rc;

        ss_dprintf_1(("tb_dd_createrenamedcheck\n"));

        tcon = TliConnectInitByTrans(cd, trans);

        rc = dd_createnamedcheck(
                tcon,
                rs_relh_relid(cd, relh),
                name,
                checkstring);

        TliConnectDone(tcon);

        if (rc!=TLI_RC_SUCC) {
            rs_error_create(p_errh, rc, name);
        }

        return(rc);
}


/*#***********************************************************************\
 *
 *              dd_getleninfo
 *
 * Returns length and precision info for a SQL data type.
 *
 * Parameters :
 *
 *      sqldatatype - in
 *
 *
 *      p_len - in out
 *
 *
 *      p_charmaxlen - out
 *
 *
 *      p_precision - out
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
static void dd_getleninfo(
        TliConnectT* tcon,
        int sqldatatype,
        long* p_len,
        long* p_charmaxlen,
        long* p_precision)
{
        switch (sqldatatype) {
            case RSSQLDT_BIT:
                *p_precision = 1;
                *p_charmaxlen = 1;
                break;
            case RSSQLDT_TINYINT:
                *p_precision = RS_TINYINT_PREC;
                *p_charmaxlen = 1;
                break;
            case RSSQLDT_BIGINT:
                *p_precision = 20;
                *p_charmaxlen = 20;
                break;
            case RSSQLDT_BINARY:
                if (*p_len == 0 || *p_len == RS_LENGTH_NULL) {
                    *p_len = 1;
                }
                /* FALLTHROUGH */
            case RSSQLDT_VARBINARY:
            case RSSQLDT_LONGVARBINARY:
                if (*p_len == 0) {
                    *p_len = RS_LENGTH_NULL;
                }
                *p_precision = *p_len;
                *p_charmaxlen = *p_len;
                break;
            case RSSQLDT_CHAR:
#ifdef SS_UNICODE_DATA
            case RSSQLDT_WCHAR:
#endif /* SS_UNICODE_DATA */
                if (*p_len == 0 || *p_len == RS_LENGTH_NULL) {
                    *p_len = 1;
                }
                /* FALLTHROUGH */
            case RSSQLDT_VARCHAR:
            case RSSQLDT_LONGVARCHAR:
#ifdef SS_UNICODE_DATA
            case RSSQLDT_WVARCHAR:
            case RSSQLDT_WLONGVARCHAR:
#endif /* SS_UNICODE_DATA */
                if (*p_len == 0) {
                    *p_len = RS_LENGTH_NULL;
                }
                *p_precision = *p_len;
                *p_charmaxlen = *p_len;
                break;
            case RSSQLDT_NUMERIC:
            case RSSQLDT_DECIMAL:
                if (*p_len > RS_DFLOAT_MAXPREC) {
                    *p_len = RS_DFLOAT_MAXPREC;
                }
                *p_precision = *p_len;
                *p_charmaxlen = *p_len + 2;
                break;
            case RSSQLDT_INTEGER:
                *p_precision = RS_INT_PREC;
                *p_charmaxlen = 4;
                break;
            case RSSQLDT_SMALLINT:
                *p_precision = RS_SMALLINT_PREC;
                *p_charmaxlen = 2;
                break;
            case RSSQLDT_REAL:
                *p_precision = RS_REAL_PREC;
                *p_charmaxlen = 4;
                break;
            case RSSQLDT_FLOAT:
            case RSSQLDT_DOUBLE:
                *p_precision = RS_DOUBLE_PREC;
                *p_charmaxlen = 8;
                break;
            case RSSQLDT_DATE:
                *p_precision = RS_DATE_DISPSIZE;
                *p_charmaxlen = 6;
                break;
            case RSSQLDT_TIME:
                *p_precision = RS_TIME_DISPSIZE;
                *p_charmaxlen = 6;
                break;
            case RSSQLDT_TIMESTAMP:
                *p_precision = RS_TIMESTAMP_DISPSIZE_MAX;
                *p_charmaxlen = 16;
                {
                    bool ts19 = FALSE;
                    bool foundp;

                    foundp = su_inifile_getbool(
                        dbe_db_getinifile(TliGetDb(tcon)),
                        SU_SQL_SECTION,
                        SU_SQL_TIMESTAMPDISPLAYSIZE19,
                        &ts19);
                    if (foundp && ts19) {
                        *p_precision = 19;
                    }
                }

                break;
            default:
                ss_error;
        }
}

/*#***********************************************************************\
 *
 *          dd_writesyscolumnsaux
 *
 * Write sys_columns_aux info to dictionary
 *
 * Parameters :
 *
 *      cd -
 *
 *      tcon -
 *
 *      attrid -
 *
 *      atype -
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void dd_writesyscolumnsaux(
        rs_sysinfo_t*   cd __attribute__ ((unused)),
        TliConnectT*    tcon,
        long            attrid,
        rs_atype_t*     atype)
{
        TliCursorT*     tcur;
        TliRetT         trc;
        long            seq_id;
        long            mysqldatatype;
#ifdef SS_COLLATION
        char*           collation_name;
#endif /* SS_COLLATION */

        ss_dprintf_3(("dd_writesyscolumnsaux\n"));

        if (rs_atype_autoinc(cd, atype)
            || rs_atype_mysqldatatype(cd, atype) != RS_MYSQLTYPE_NONE) 
        {
            tcur = TliCursorCreate(tcon,
                                   RS_AVAL_DEFCATALOG,
                                   RS_AVAL_SYSNAME,
                                   RS_RELNAME_COLUMNS_AUX);
            ss_dassert(tcur != NULL);

            trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_ID, &attrid);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            if (rs_atype_autoinc(cd, atype)) {
                /* If this attribute type is auto_increment, write auto_increment sequence
                   identification to SYS_COLUMNS_AUX table. */
                trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_AUTO_INC_SEQ_ID, &seq_id);
                ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
                seq_id = rs_atype_getautoincseqid(cd, atype);
            }

            if (rs_atype_mysqldatatype(cd, atype) != RS_MYSQLTYPE_NONE) {
                trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_EXTERNAL_DATA_TYPE, &mysqldatatype);
                ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
                mysqldatatype = rs_atype_mysqldatatype(cd, atype);
                ss_dassert(mysqldatatype > RS_MYSQLTYPE_NONE && mysqldatatype <= RS_MYSQLTYPE_BIT);
            }
            
#ifdef SS_COLLATION
            if (rs_atype_collation(cd, atype) != NULL) {
                su_collation_t* collation;
                collation = rs_atype_collation(cd, atype);
                trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_AUX_EXTERNAL_COLLATION, &collation_name);
                ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
                collation_name = su_collation_getname(collation);
            }
#endif /* SS_COLLATION */

            trc = TliCursorInsert(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

            TliCursorFree(tcur);
        }
}

/*#***********************************************************************\
 *
 *              dd_createttype
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relid -
 *
 *
 *      ttype -
 *
 *
 *      type -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_ttype_t* dd_createttype(
        TliConnectT* tcon,
        ulong relid,
        rs_ttype_t* ttype,
        tb_dd_createrel_t type)
{
        TliCursorT* tcur;
        TliRetT trc;
        int i;
        int nattrs;
        long attrid;
        int attrtype;
        int ano;
        int sqldatatype;
        int datatype;
        char* datatypestr;
        long charmaxlen;
        long precision;
        long len;
        int scale;
        int rdx;
        char* nullallowed;
        int nullallowed_odbc;
        char* attrname;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        dbe_db_t* db = TliGetDb(tcon);
        rs_aval_t*      currentdefault;
        rs_atype_t*     defaultatype;
        rs_aval_t*      defval;

        ss_dprintf_3(("dd_createttype\n"));

        defaultatype = rs_atype_init_sqldt(cd, RSSQLDT_WVARCHAR);
        currentdefault = rs_aval_create(cd, defaultatype);
        
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_ID, &attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_REL_ID, (long*)&relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_COLUMN_NAME, &attrname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_COLUMN_NUMBER, &ano);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_DATA_TYPE, &datatypestr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_ATTR_TYPE, &attrtype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_SQLDTYPE_NO, &sqldatatype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_DATA_TYPE_NO, &datatype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_CHAR_MAX_LEN, &charmaxlen);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_NUM_PRECISION, &precision);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX, &rdx);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NUM_SCALE, &scale);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_NULLABLE, &nullallowed);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NULLABLE_ODBC, &nullallowed_odbc);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColAval(tcur, RS_ANAME_COLUMNS_DEFAULT_VAL,
                               defaultatype, currentdefault);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        nattrs = rs_ttype_nattrs(cd, ttype);

        for (i = 0; i < nattrs; i++) {
            rs_atype_t* atype;
            atype = rs_ttype_atype(cd, ttype, i);

            if (rs_atype_pseudo(cd, atype)) {
                continue;
            }

            if (type == TB_DD_CREATEREL_SYSTEM) {
                attrid = rs_ttype_attrid(cd, ttype, i);
            } else {
                attrid = dbe_db_getnewattrid_log(db);
                rs_ttype_setattrid(cd, ttype, i, attrid);
            }
            ano = i;
            attrtype = rs_atype_attrtype(cd, atype);
            sqldatatype = rs_atype_sqldatatype(cd, atype);
            datatypestr = rs_atype_name(cd, atype);
            datatype = rs_atype_datatype(cd, atype);
            len = rs_atype_length(cd, atype);
            scale = rs_atype_scale(cd, atype);
            nullallowed_odbc = rs_atype_nullallowed(cd, atype);
            nullallowed = dd_getboolstr(nullallowed_odbc);
            attrname = rs_ttype_aname(cd, ttype, i);

            /* The SYS_COLUMNS has been changed to be ODBC
             * compliant when doing the select for SQLColumns
             */
            dd_getleninfo(
                tcon,
                sqldatatype,
                &len,
                &charmaxlen,
                &precision);

            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_CHAR_MAX_LEN);
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_PRECISION);
            if (datatype == RSDT_CHAR
#ifdef SS_UNICODE_DATA
            ||  datatype == RSDT_BINARY
            ||  datatype == RSDT_UNICODE
#endif /* SS_UNICODE_DATA */
            ||  scale == RS_SCALE_NULL)
            {
                TliCursorColSetNULL(tcur, RS_ANAME_COLUMNS_NUM_SCALE);
            } else {
                TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_SCALE);
            }
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX);
            rdx = rs_atype_datatyperadix(cd, datatype);
            if (rdx == RS_RADIX_NULL) {
                TliCursorColSetNULL(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX);
            }

            defval = rs_atype_getcurrentdefault(cd, atype);
            if (defval == NULL) {
                rs_aval_setnull(cd, defaultatype, currentdefault);
            } else {
                rs_aval_convert_ext(
                        cd,
                        defaultatype,
                        currentdefault,
                        atype,
                        defval,
                        NULL);
                TliCursorColClearNULL(tcur, rs_atype_name(cd, atype));
            }

            trc = TliCursorInsert(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

            dd_writesyscolumnsaux(cd, tcon, attrid, atype);
        }

        TliCursorFree(tcur);

        rs_aval_free(cd, defaultatype, currentdefault);
        rs_atype_free(cd, defaultatype);

        return(ttype);
}

#ifdef SS_ALTER_TABLE

/*#***********************************************************************\
 *
 *              dd_addonekeypart
 *
 * Adds one key part to a key.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      key -
 *
 *
 *      kptype -
 *
 *
 *      ano -
 *
 *
 *      attrid -
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
static void dd_addonekeypart(
        void* cd __attribute__ ((unused)),
        TliConnectT* tcon,
        ulong relid,
        long keyid,
        int kpno,
        int kptype,
        rs_ano_t ano,
        long attrid)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* ascstr;
        va_t* constvalue;
        int attr_no = ano;

        ss_dprintf_3(("dd_addonekeypart\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_REL_ID, (long*)&relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_KEYP_NO, &kpno);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ATTR_ID, (long*)&attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_ATTR_NO, &attr_no);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_ATTR_TYPE, &kptype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYPARTS_ASCENDING, &ascstr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColVa(tcur, RS_ANAME_KEYPARTS_CONST_VALUE, &constvalue);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        ascstr = dd_getboolstr(TRUE);
        ss_dassert(ano != RS_ANO_NULL);
        constvalue = NULL;

        trc = TliCursorInsert(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);
}

static void dd_writeoriginaldefault(
        rs_sysinfo_t*   cd __attribute__ ((unused)),
        TliConnectT*    tcon,
        long            attrid,
        rs_atype_t*     defatype,
        rs_aval_t*      defaval)
{
        TliCursorT*     tcur;
        TliRetT         trc;

        ss_dprintf_3(("dd_writeoriginaldefault\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS_AUX);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_ID, &attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColAval(tcur, RS_ANAME_COLUMNS_AUX_ORIGINAL_DEFAULT,
                               defatype, defaval);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_AUX_ID);
        TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_AUX_ORIGINAL_DEFAULT);

        trc = TliCursorInsert(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);
}


/*#***********************************************************************\
 *
 *              dd_addoneattribute
 *
 * Adds one attribute to a relation.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      attrname -
 *
 *
 *      atype -
 *
 *
 *      ano -
 *
 *
 *      attrid -
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
static void dd_addoneattribute(
        rs_sysinfo_t*   cd,
        TliConnectT*    tcon,
        rs_relh_t*      relh,
        char*           attrname,
        rs_atype_t*     atype,
        rs_ano_t        ano,
        rs_atype_t*     defatype,
        rs_aval_t*      defaval,
        long            attrid)
{
        TliCursorT* tcur;
        TliRetT trc;
        int attrtype;
        int sqldatatype;
        int datatype;
        char* datatypestr;
        long charmaxlen;
        long precision;
        long len;
        int scale;
        int rdx;
        ulong relid;
        char* nullallowed;
        int nullallowed_odbc;
        int attr_no = ano;

        ss_dprintf_3(("dd_addoneattribute\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_ID, &attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_REL_ID, (long*)&relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_COLUMN_NAME, &attrname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_COLUMN_NUMBER, &attr_no);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_DATA_TYPE, &datatypestr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_ATTR_TYPE, &attrtype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_SQLDTYPE_NO, &sqldatatype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_DATA_TYPE_NO, &datatype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_CHAR_MAX_LEN, &charmaxlen);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_NUM_PRECISION, &precision);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX, &rdx);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NUM_SCALE, &scale);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_NULLABLE, &nullallowed);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NULLABLE_ODBC, &nullallowed_odbc);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        if (defatype != NULL) {
            trc = TliCursorColAval(tcur, RS_ANAME_COLUMNS_DEFAULT_VAL,
                                   defatype, defaval);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_DEFAULT_VAL);
            dd_writeoriginaldefault(cd, tcon, attrid, defatype, defaval);
        }

        ss_dassert(!rs_atype_pseudo(cd, atype));

        relid = rs_relh_relid(cd, relh);

        attrtype = rs_atype_attrtype(cd, atype);
        sqldatatype = rs_atype_sqldatatype(cd, atype);
        datatypestr = rs_atype_name(cd, atype);
        datatype = rs_atype_datatype(cd, atype);
        len = rs_atype_length(cd, atype);
        scale = rs_atype_scale(cd, atype);
        nullallowed_odbc = rs_atype_nullallowed(cd, atype);
        nullallowed = dd_getboolstr(nullallowed_odbc);

        /* The SYS_COLUMNS has been changed to be ODBC
         * compliant when doing the select for SQLColumns
         */
        dd_getleninfo(
            tcon,
            sqldatatype,
            &len,
            &charmaxlen,
            &precision);

        TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_CHAR_MAX_LEN);
        TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_PRECISION);
        if (datatype == RSDT_CHAR
#ifdef SS_UNICODE_DATA
        ||  datatype == RSDT_UNICODE
        ||  datatype == RSDT_BINARY
#endif /* SS_UNICODE_DATA */
        ||  scale == RS_SCALE_NULL)
        {
            TliCursorColSetNULL(tcur, RS_ANAME_COLUMNS_NUM_SCALE);
        } else {
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_SCALE);
        }
        TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX);
        rdx = rs_atype_datatyperadix(cd, datatype);
        if (rdx == RS_RADIX_NULL) {
            TliCursorColSetNULL(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX);
        }
        trc = TliCursorInsert(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        dd_writesyscolumnsaux(cd, tcon, attrid, atype);

        TliCursorFree(tcur);
}

#ifndef SS_MYSQL
/*#***********************************************************************\
 *
 *              dd_ttype_getnewano
 *
 *
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
static rs_ano_t dd_ttype_getnewano(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype)
{
        rs_ano_t ano;
        int nattrs;
        int i;

        nattrs = rs_ttype_nattrs(cd, ttype);
        for (i = 0, ano = 0; i < nattrs; i++) {
            rs_atype_t* tt_atype;
            tt_atype = rs_ttype_atype(cd, ttype, i);
            if (!rs_atype_pseudo(cd, tt_atype)) {
                ano++;
            }
        }
        return(ano);
}
#endif /* !SS_MYSQL */

su_ret_t tb_dd_addattributelist(
        rs_sysinfo_t*   cd,
        tb_trans_t*     trans,
        rs_relh_t*      relh,
        su_list_t*      attrlist,
        bool            syskeyadd,
        rs_err_t**      p_errh)
{
        TliConnectT* tcon;
        rs_ano_t ano;
        rs_ano_t ano_use;
        int i;
        int nattrs;
        long attrid;
        su_ret_t rc;
        dbe_db_t* db;
        rs_ttype_t* ttype;
        su_list_node_t* n;
        dd_attr_t* attr;
        rs_key_t* key;
        ulong relid;
        long keyid;
        int kpno;
        int newcolcount;

        ss_dprintf_1(("tb_dd_addattributelist\n"));

        ttype = rs_relh_ttype(cd, relh);

        nattrs = rs_ttype_nattrs(cd, ttype);
        for (i = 0, ano = 0; i < nattrs; i++) {
            rs_atype_t* tt_atype;
            tt_atype = rs_ttype_atype(cd, ttype, i);
            if (!rs_atype_pseudo(cd, tt_atype)) {
                ano++;
            }
        }

        ano_use = ano;
        tcon = TliConnectInitByTrans(cd, trans);

        db = TliGetDb(tcon);

        key = rs_relh_clusterkey(cd, relh),
        relid = rs_relh_relid(cd, relh);
        keyid = rs_key_id(cd, key);
        kpno = rs_key_nparts(cd, key);

        /* Check validity of attribute names.
         */
        su_list_do_get(attrlist, n, attr) {
            ss_dprintf_1(("tb_dd_addattributelist:attrname = %s\n", attr->attrname));

            ano = rs_ttype_anobyname(cd, ttype, attr->attrname);
            if (!attr->syscall) {
                if (ano != RS_ANO_NULL || rs_sdefs_sysaname(attr->attrname)) {
                    /* Attribute name already exists. */
                    TliConnectDone(tcon);
                    rs_error_create(p_errh, E_ATTREXISTONREL_SS, attr->attrname, rs_relh_name(cd, relh));
                    return(E_ATTREXISTONREL_SS);
                }
            }
        }

        newcolcount = dbe_trx_newcolumncount(
                        tb_trans_dbtrx(cd, trans),
                        relh);

        ss_dprintf_2(("tb_dd_addattributelist:newcolcount=%d\n", newcolcount));

        rc = dbe_trx_alterrel(
                tb_trans_dbtrx(cd, trans),
                relh,
                su_list_length(attrlist));
        if (rc != SU_SUCCESS) {
            rs_error_create(p_errh, rc);
        }

        if (rc == SU_SUCCESS) {
            /* Add attributes.
             */
            bool emptyp;

            ano_use += newcolcount;
            kpno += newcolcount;

            emptyp = tb_dd_checkifrelempty(cd, trans, rs_relh_entname(cd, relh));
            su_list_do_get(attrlist, n, attr) {
                ss_dprintf_1(("tb_dd_addattributelist:attrname = %s\n", attr->attrname));

                /* This should not be an issue anymore, now that we have
                   default values.
                   2004-03-29 apl */
#if 0
                if (!emptyp && !rs_atype_nullallowed(cd, attr->atype)) {
                    rc = E_ILLNULLALLOWED_S;
                    rs_error_create(p_errh, E_ILLNULLALLOWED_S, attr->attrname);
                    break;
                }
#endif

                if (syskeyadd) {
                    ss_dassert(su_list_length(attrlist) == 1);
                    ano_use = rs_ttype_anobyname(cd, ttype, attr->attrname);
                    ss_dassert(ano_use == 11);
                    attrid = rs_ttype_attrid(cd, ttype, ano_use);
                    kpno = rs_key_searchkpno_data(cd, key, ano_use);
                    ss_dprintf_2(("tb_dd_addattributelist:syskeyadd:attrname=%s, ano_use=%d, attrid=%ld, kpno=%d\n",
                        attr->attrname, ano_use, attrid, kpno));
                } else {
                    ano = rs_ttype_anobyname(cd, ttype, attr->attrname);
                    ss_dassert(ano == RS_ANO_NULL);
                    attrid = dbe_db_getnewattrid_log(db);
                }


                if (attr->p_newattrid != NULL) {
                    *attr->p_newattrid = attrid;
                }

                dd_addoneattribute(
                    cd,
                    tcon,
                    relh,
                    attr->attrname,
                    attr->atype,
                    ano_use,
                    attr->defatype,
                    attr->defaval,
                    attrid);

                dd_addonekeypart(
                    cd,
                    tcon,
                    relid,
                    keyid,
                    kpno,
                    rs_atype_attrtype(cd, attr->atype),
                    ano_use,
                    attrid);

                kpno++;
                ano_use++;
            }
        }

        TliConnectDone(tcon);

        return(rc);
}

/*##**********************************************************************\
 *
 *              tb_dd_removeattribute
 *
 * Removes an attribute from a table.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      attrname -
 *
 *
 *      auth -
 *
 *
 *      p_errh -
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
su_ret_t tb_dd_removeattribute(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* attrname,
        rs_auth_t* auth __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char* new_attrname;
        int attrtype;
        char* nullallowed;
        int nullallowed_odbc;
        su_ret_t rc;
        long attrid;
        int i;
        int nattrs;
        int nuserattr;
        rs_ttype_t* ttype;
        rs_ano_t sqlano;

        ss_dprintf_1(("tb_dd_removeattribute:attrname = %s\n", attrname));
        SS_PUSHNAME("tb_dd_removeattribute");

        ttype = rs_relh_ttype(cd, relh);

        sqlano = rs_ttype_sql_anobyname(cd, ttype, attrname);
        if (sqlano == RS_ANO_NULL || rs_sdefs_sysaname(attrname)) {
            /* Attribute name does not exists. */
            rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, attrname, rs_relh_name(cd, relh));
            SS_POPNAME;
            return(E_ATTRNOTEXISTONREL_SS);
        }

        /* Count the number of user defined physical attributes.
         */
        nattrs = rs_ttype_nattrs(cd, ttype);
        for (i = 0, nuserattr = 0; i < nattrs; i++) {
            rs_atype_t* tt_atype;
            tt_atype = rs_ttype_atype(cd, ttype, i);
            if (rs_atype_isuserdefined(cd, tt_atype) &&
                !rs_atype_pseudo(cd, tt_atype)) {
                nuserattr++;
            }
        }
        if (nuserattr <= 1) {
            rs_error_create(p_errh, E_LASTCOLUMN);
            SS_POPNAME;
            return(E_LASTCOLUMN);
        }

        /* Check that attribute is not part of any unique key or reference key
         */
        {
            int i;
            su_pa_t* keys;
            rs_key_t* key;
            rs_ano_t physano;
            int keytype;

            physano = rs_ttype_anobyname(cd, ttype, attrname);
            ss_bassert(physano != RS_ANO_NULL);
            keys = rs_relh_keys(cd, relh);
            su_pa_do_get(keys, i, key) {
                if (rs_key_isunique(cd, key)) {
                    int kpno;
                    int lastordering;
                    lastordering = rs_key_lastordering(cd, key);
                    for (kpno = 0; kpno <= lastordering; kpno++) {
                        if (rs_keyp_ano(cd, key, kpno) == physano) {
                            rs_error_create_key(p_errh,
                                                E_CANNOTDROPUNQCOL_S, key);
                            SS_POPNAME;
                            return(E_CANNOTDROPUNQCOL_S);
                        }
                    }
                }
            }
            keys = rs_relh_refkeys(cd, relh);
            su_pa_do_get(keys, i, key) {
                keytype = rs_key_type(cd, key);
                if (keytype == RS_KEY_FORKEYCHK) {
                    int kpno;
                    int lastordering;
                    lastordering = rs_key_lastordering(cd, key);
                    for (kpno = 0; kpno <= lastordering; kpno++) {
                        if (rs_keyp_ano(cd, key, kpno) == physano) {
                            rs_error_create_key(p_errh,
                                                E_CANNOTDROPFORKEYCOL_S, key);
                            SS_POPNAME;
                            return(E_CANNOTDROPFORKEYCOL_S);
                        }
                    }
                }
            }
        }

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_ID, &attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_COLUMN_NAME, &attrname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_ATTR_TYPE, &attrtype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_NULLABLE, &nullallowed);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NULLABLE_ODBC, &nullallowed_odbc);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_REL_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_COLUMNS_COLUMN_NAME,
                TLI_RELOP_EQUAL,
                attrname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (trc != TLI_RC_SUCC) {
            ss_dassert(trc == TLI_RC_END);
            TliCursorFree(tcur);
            TliConnectDone(tcon);
            rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, attrname, rs_relh_name(cd, relh));
            SS_POPNAME;
            return(E_ATTRNOTEXISTONREL_SS);
        }

        new_attrname = SsMemAlloc(strlen("$SYS_DELETED_") + 10 + 1 + strlen(attrname) + 1);
        SsSprintf(new_attrname, "$SYS_DELETED_%ld_%s", attrid, attrname);

        attrtype = RSAT_REMOVED;
        attrname = new_attrname;
        nullallowed_odbc = TRUE;
        nullallowed = dd_getboolstr(TRUE);

        trc = TliCursorUpdate(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        SsMemFree(new_attrname);

        TliCursorFree(tcur);

        /* Update also key part info. Attribute type is coped there.
         */

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColInt(tcur, RS_ANAME_KEYPARTS_ATTR_TYPE, &attrtype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYPARTS_ATTR_ID,
                TLI_RELOP_EQUAL,
                attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        attrtype = RSAT_REMOVED;

        trc = TliCursorUpdate(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        TliConnectDone(tcon);

        rc = dbe_trx_alterrel(
                tb_trans_dbtrx(cd, trans),
                relh,
                0);

        if (rc != SU_SUCCESS) {
            rs_error_create(p_errh, rc);
        }

        SS_POPNAME;

        return(rc);
}

static bool dd_modifyattribute_checktypes(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname,
        rs_atype_t* old_atype,
        rs_atype_t* new_atype)
{
        rs_datatype_t old_datatype;
        rs_datatype_t new_datatype;
        rs_sqldatatype_t old_sqldt;
        rs_sqldatatype_t new_sqldt;

        old_sqldt = rs_atype_sqldatatype(cd, old_atype);
        old_datatype = rs_atype_datatype(cd, old_atype);
        new_sqldt = rs_atype_sqldatatype(cd, new_atype);
        new_datatype = rs_atype_datatype(cd, new_atype);

#ifdef SS_UNICODE_SQL
        if (tb_dd_migratingtounicode
        &&  old_datatype == RSDT_CHAR
        &&  new_datatype == RSDT_UNICODE)
        {
            return(TRUE);
        }
#endif /* SS_UNICODE_SQL */

        if (new_datatype == RSDT_BIGINT &&
            old_datatype == RSDT_INTEGER)
        {
            /* TINYINT, SMALLINT, INTEGER (or BIGINT) to BIGINT ok */
            return (TRUE);
        } else if (old_datatype == RSDT_BIGINT &&
                   new_datatype == RSDT_INTEGER) {
            goto mustbeempty;
        }
        if (old_datatype != new_datatype) {
            /* Can not change base type
             * (except for RSDT_INTEGER & RSDT_BIGINT)
             */
            return (FALSE);
        }
        switch (old_sqldt) {
            case RSSQLDT_CHAR:
            case RSSQLDT_VARCHAR:
            case RSSQLDT_LONGVARCHAR:
            case RSSQLDT_WCHAR:
            case RSSQLDT_WVARCHAR:
            case RSSQLDT_WLONGVARCHAR:
            case RSSQLDT_BINARY:
            case RSSQLDT_VARBINARY:
            case RSSQLDT_LONGVARBINARY:
                if (rs_atype_length(cd, old_atype) > rs_atype_length(cd, new_atype)) {
                    /* Can not change to a shorter length if there are rows
                     * in the table. Causes problems because actual data
                     * is not truncated.
                     */
                    goto mustbeempty;
                }
                break;
            case RSSQLDT_INTEGER:
                if (new_sqldt != RSSQLDT_INTEGER) {
                    goto mustbeempty;
                }
                break;
            case RSSQLDT_SMALLINT:
                if (new_sqldt == RSSQLDT_TINYINT) {
                    goto mustbeempty;
                }
                break;
            case RSSQLDT_TINYINT:
                break;
            case RSSQLDT_DATE:
                if (new_sqldt == RSSQLDT_TIME) {
                    return (FALSE);
                }
                break;
            case RSSQLDT_TIME:
                if (new_sqldt != RSSQLDT_TIME) {
                    return (FALSE);
                }
                break;
            case RSSQLDT_TIMESTAMP:
                if (new_sqldt != RSSQLDT_TIMESTAMP) {
                    return (FALSE);
                }
                break;
            default:
                break;
        }
        return(TRUE);
 mustbeempty:;
        return(tb_dd_checkifrelempty(cd, trans, relname));
}

/*##**********************************************************************\
 *
 *              tb_dd_modifyattribute
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      attrname -
 *
 *
 *      atype -
 *
 *
 *      auth -
 *
 *
 *      p_errh -
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
su_ret_t tb_dd_modifyattribute(
        rs_sysinfo_t*   cd,
        tb_trans_t*     trans,
        rs_relh_t*      relh,
        char*           attrname,
        bool            updatenotnull,
        rs_atype_t*     atype,
        rs_auth_t*      auth __attribute__ ((unused)),
        rs_atype_t*     defatype,
        rs_aval_t*      defaval,
        rs_err_t**      p_errh,
        bool            force)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        rs_ttype_t* ttype;
        rs_ano_t ano;
        rs_atype_t* old_atype;
        int sqldatatype;
        int datatype;
        char* datatypestr;
        long charmaxlen;
        long precision;
        long len;
        int scale;
        int rdx;
        TliRetT trc;
        su_ret_t rc = SU_SUCCESS;
        rs_aval_t*      currentdefault = NULL;
        rs_atype_t*     defaultatype = NULL;
        char *          nullallowed;
        int             nullallowed_odbc;

        ss_dprintf_1(("tb_dd_modifyattribute\n"));

        ttype = rs_relh_ttype(cd, relh);

        ano = rs_ttype_sql_anobyname(cd, ttype, attrname);
        if ((ano == RS_ANO_NULL || rs_sdefs_sysaname(attrname)) && !force) {
            /* Attribute name does not exists. */
            rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, attrname, rs_relh_name(cd, relh));
            return(E_ATTRNOTEXISTONREL_SS);
        }

        if (atype != NULL && !force) {
            old_atype = rs_ttype_sql_atype(cd, ttype, ano);
            
            if (!dd_modifyattribute_checktypes(
                        cd,
                        trans,
                        rs_relh_entname(cd, relh),
                        old_atype,
                        atype))
            {
                /* Attribute types are not compatible, cannot modify.
                 */
                char* oldname;
                char* newname;
                oldname = rs_atype_givecoltypename(cd, old_atype);
                newname = rs_atype_givecoltypename(cd, atype);
                rs_error_create(
                        p_errh,
                        E_INCOMPATMODIFY_SSS,
                        attrname,
                        oldname,
                        newname);
                SsMemFree(oldname);
                SsMemFree(newname);
                return(E_INCOMPATMODIFY_SSS);
            }
        }

        tcon = TliConnectInitByTrans(cd, trans);

        ss_dassert(!force
                   || dd_checkifrelempty_tcon(
                           tcon,
                           rs_relh_entname(cd, relh)));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_dassert(tcur != NULL);

        if (atype != NULL) {
            trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_DATA_TYPE, &datatypestr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_SQLDTYPE_NO, &sqldatatype);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_CHAR_MAX_LEN, &charmaxlen);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_NUM_PRECISION, &precision);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX, &rdx);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NUM_SCALE, &scale);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_NULLABLE, &nullallowed);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColInt(tcur, RS_ANAME_COLUMNS_NULLABLE_ODBC, &nullallowed_odbc);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        } else {
            defaultatype = rs_atype_init_sqldt(cd, RSSQLDT_WVARCHAR);
            currentdefault = rs_aval_create(cd, defaultatype);
            trc = TliCursorColAval(tcur, RS_ANAME_COLUMNS_DEFAULT_VAL,
                                   defaultatype, currentdefault);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_DEFAULT_VAL);
        }
        
        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_REL_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_COLUMNS_COLUMN_NAME,
                TLI_RELOP_EQUAL,
                attrname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (trc != TLI_RC_SUCC) {
            ss_dassert(trc == TLI_RC_END);
            TliCursorFree(tcur);
            TliConnectDone(tcon);
            rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, attrname, rs_relh_name(cd, relh));
            return(E_ATTRNOTEXISTONREL_SS);
        }

        if (atype != NULL) {
            sqldatatype = rs_atype_sqldatatype(cd, atype);
            datatypestr = rs_atype_name(cd, atype);
            len = rs_atype_length(cd, atype);
            scale = rs_atype_scale(cd, atype);
            datatype = rs_atype_datatype(cd, atype);
            
            /* The SYS_COLUMNS has been changed to be ODBC
             * compliant when doing the select for SQLColumns
             */
            dd_getleninfo(
                    tcon,
                    sqldatatype,
                    &len,
                    &charmaxlen,
                    &precision);
            
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_CHAR_MAX_LEN);
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_PRECISION);
            if (datatype == RSDT_CHAR
#ifdef SS_UNICODE_DATA
                ||  datatype == RSDT_BINARY
                ||  datatype == RSDT_UNICODE
#endif /* SS_UNICODE_DATA */
                || scale == RS_SCALE_NULL)
            {
                TliCursorColSetNULL(tcur, RS_ANAME_COLUMNS_NUM_SCALE);
            } else {
                TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_SCALE);
            }
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX);
            rdx = rs_atype_datatyperadix(cd, datatype);
            if (rdx == RS_RADIX_NULL) {
                TliCursorColSetNULL(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX);
            }
            if(updatenotnull) {
                /* Update NULLABLE attribute value */
                nullallowed_odbc = rs_atype_nullallowed(cd, atype);
                nullallowed = dd_getboolstr(nullallowed_odbc);
            }
        } else {
            rs_aval_convert_ext(
                    cd,
                    defaultatype,
                    currentdefault,
                    defatype,
                    defaval,
                    NULL);
            TliCursorColClearNULL(tcur, RS_ANAME_COLUMNS_DEFAULT_VAL);
        }

        trc = TliCursorUpdate(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);
        TliConnectDone(tcon);

#ifdef SS_UNICODE_SQL
        if (!tb_dd_migratingtounicode)
#endif /* SS_UNICODE_SQL */
        {
            rc = dbe_trx_alterrel(
                    tb_trans_dbtrx(cd, trans),
                    relh,
                    0);

            if (rc != SU_SUCCESS) {
                rs_error_create(p_errh, rc);
            }
        }

        if (defaultatype != NULL) {
            rs_aval_free(cd, defaultatype, currentdefault);
            rs_atype_free(cd, defaultatype);
        }
        
        return(rc);
}

/*##**********************************************************************\
 *
 *              tb_dd_renameattribute
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      attrname -
 *
 *
 *      new_attrname -
 *
 *
 *      auth -
 *
 *
 *      p_errh -
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
su_ret_t tb_dd_renameattribute(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* attrname,
        char* new_attrname,
        rs_auth_t* auth __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        su_ret_t rc;
        rs_ttype_t* ttype;
        rs_ano_t ano;

        ss_dprintf_1(("tb_dd_renameattribute\n"));

        ttype = rs_relh_ttype(cd, relh);

        ano = rs_ttype_sql_anobyname(cd, ttype, attrname);
        if (ano == RS_ANO_NULL || rs_sdefs_sysaname(attrname)) {
            /* Attribute name does not exists. */
            rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, attrname, rs_relh_name(cd, relh));
            return(E_ATTRNOTEXISTONREL_SS);
        }

        ano = rs_ttype_sql_anobyname(cd, ttype, new_attrname);
        if (ano != RS_ANO_NULL || rs_sdefs_sysaname(new_attrname)) {
            /* Attribute name does not exists. */
            rs_error_create(p_errh, E_ATTREXISTONREL_SS, new_attrname, rs_relh_name(cd, relh));
            return(E_ATTREXISTONREL_SS);
        }

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_COLUMN_NAME, &attrname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_REL_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_COLUMNS_COLUMN_NAME,
                TLI_RELOP_EQUAL,
                attrname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (trc != TLI_RC_SUCC) {
            ss_dassert(trc == TLI_RC_END);
            TliCursorFree(tcur);
            TliConnectDone(tcon);
            rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, attrname, rs_relh_name(cd, relh));
            return(E_ATTRNOTEXISTONREL_SS);
        }

        attrname = new_attrname;

        trc = TliCursorUpdate(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        rc = dbe_trx_alterrel(
                tb_trans_dbtrx(cd, trans),
                relh,
                0);

        if (rc != SU_SUCCESS) {
            rs_error_create(p_errh, rc);
        }

        return(rc);
}

/*#***********************************************************************\
 *
 *              dd_changekeyschema
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relid -
 *
 *
 *      new_schema -
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
static void dd_changekeyschema(
        TliConnectT* tcon,
        long relid,
        char* old_schema,
        char* new_schema)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* schemaname;

        ss_dprintf_1(("dd_changekeyschema\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_SCHEMA, &schemaname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_KEYS_SCHEMA,
                TLI_RELOP_EQUAL,
                old_schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            schemaname = new_schema;
            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_rc_assert(trc == TLI_RC_END, TliCursorErrorCode(tcur));

        TliCursorFree(tcur);
}

static bool dd_changetriggerschema(
        TliConnectT* tcon,
        long relid,
        char* old_schema,
        char* new_schema,
        rs_err_t** p_errh)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* catalogname;
        char* schemaname;
        char* trigname;
        rs_sysi_t* cd;
        tb_trans_t* trans;
        bool succp = TRUE;
        long trigid;

        ss_dprintf_1(("dd_changetriggerschema\n"));

        cd = TliGetCd(tcon);
        trans = TliGetTrans(tcon);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TRIGGERS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TRIGGERS_CATALOG, &catalogname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TRIGGERS_SCHEMA, &schemaname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TRIGGERS_NAME, &trigname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_TRIGGERS_ID, &trigid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TRIGGERS_RELID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrStr(
                tcur,
                RS_ANAME_TRIGGERS_SCHEMA,
                TLI_RELOP_EQUAL,
                old_schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            dbe_ret_t rc;
            rs_entname_t en;

            rs_entname_initbuf(&en, catalogname, schemaname, trigname);
            rc = dbe_trx_deletename(tb_trans_dbtrx(cd, trans), &en);
            if (rc != DBE_RC_SUCC) {
                succp = FALSE;
                break;
            }

            rs_entname_initbuf(&en, catalogname, new_schema, trigname);
            if (!rs_rbuf_addname(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_TRIGGER, trigid)) {
                rs_error_create(p_errh, SP_ERR_TRIGEXIST_S, trigname);
                succp = FALSE;
                break;
            }
            rc = dbe_trx_insertname(tb_trans_dbtrx(cd, trans), &en);
            if (rc != DBE_RC_SUCC) {
                rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_TRIGGER);
                rs_error_create(p_errh, rc);
                succp = FALSE;
                break;
            }

            schemaname = new_schema;

            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_rc_assert(trc == TLI_RC_SUCC || trc == TLI_RC_END, TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_dd_changeschema
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      new_schema -
 *
 *
 *      auth -
 *
 *
 *      p_errh -
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
su_ret_t tb_dd_changeschema(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* new_schema,
        rs_auth_t* auth,
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        su_ret_t rc;
        char* schemaname;
        char* old_schemaname;
        long new_userid;
        long old_userid;
        long uid_array[2];
        bool b;
        bool succp;
        rs_entname_t en;

        ss_dprintf_1(("tb_dd_changeschema\n"));

        tcon = TliConnectInitByTrans(cd, trans);

        b = tb_schema_maptouser(
                cd,
                trans,
                new_schema,
                rs_relh_catalog(cd, relh),
                &new_userid,
                NULL);
        if (!b) {
            TliConnectDone(tcon);
            rs_error_create(p_errh, E_ILLUSERNAME_S, new_schema);
            return(E_ILLUSERNAME_S);
        }
        rs_entname_initbuf(
                &en,
                rs_relh_catalog(cd, relh),
                new_schema,
                rs_relh_name(cd, relh));
        if (!rs_rbuf_addname(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION, rs_relh_relid(cd, relh))) {
            TliConnectDone(tcon);
            rs_error_create(p_errh, E_RELEXIST_S, rs_relh_name(cd, relh));
            return (E_RELEXIST_S);
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &schemaname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLES_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        old_schemaname = SsMemStrdup(schemaname);
        schemaname = new_schema;

        trc = TliCursorUpdate(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        b = tb_schema_maptouser(
                cd,
                trans,
                old_schemaname,
                rs_relh_catalog(cd, relh),
                &old_userid,
                NULL);
        if (!b) {
            rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
            TliConnectDone(tcon);
            rs_error_create(p_errh, E_ILLUSERNAME_S, old_schemaname);
            SsMemFree(old_schemaname);
            return(E_ILLUSERNAME_S);
        }

        dd_changekeyschema(
            tcon,
            rs_relh_relid(cd, relh),
            old_schemaname,
            new_schema);

        succp = dd_changetriggerschema(
                    tcon,
                    rs_relh_relid(cd, relh),
                    old_schemaname,
                    new_schema,
                    p_errh);

        SsMemFree(old_schemaname);

        if (!succp) {
            TliConnectDone(tcon);
            return(DBE_ERR_FAILED);
        }

        rc = dbe_trx_renamerel(
                tb_trans_dbtrx(cd, trans),
                relh,
                &en);

        if (rc != SU_SUCCESS) {
            TliConnectDone(tcon);
            rs_error_create(p_errh, rc);
            return(rc);
        }

        /* Grant new owner all access rights.
         */
        uid_array[0] = new_userid;
        uid_array[1] = -1L;
        b = tb_priv_setrelpriv(
                tcon,
                FALSE,
                rs_auth_userid(cd, auth),
                TRUE,
                rs_relh_relid(cd, relh),
                TB_PRIV_SELECT|TB_PRIV_INSERT|TB_PRIV_DELETE|
                TB_PRIV_UPDATE|TB_PRIV_REFERENCES|TB_PRIV_CREATOR,
                uid_array,
                p_errh);
        if (!b) {
            TliConnectDone(tcon);
            return(rs_error_geterrcode(cd, *p_errh));
        }

        /* Revoke creator access rights from old owner.
         */
        uid_array[0] = old_userid;
        uid_array[1] = -1L;
        b = tb_priv_setrelpriv_ex(
                tcon,
                TRUE,
                old_userid,
                TRUE,
                rs_relh_relid(cd, relh),
                TB_PRIV_CREATOR,
                uid_array,
                TRUE,
                p_errh);
        if (!b) {
            TliConnectDone(tcon);
            return(rs_error_geterrcode(cd, *p_errh));
        }

        TliConnectDone(tcon);

        return(rc);
}

#endif /* SS_ALTER_TABLE */

#ifndef SS_MYSQL
#ifdef SS_SYNC

#define SYNCHISTORYKEYATTRIB_COUNT          1
#define SYNCISPUBLHISTORYKEYATTRIB_COUNT    2

static bool dd_createsyncispublhistoryversionkey(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_ttype_t* ttype,
        rs_err_t** p_errh)
{
        bool succp;
        char* columns[SYNCISPUBLHISTORYKEYATTRIB_COUNT];
        bool desc[SYNCISPUBLHISTORYKEYATTRIB_COUNT];
        long relid;
        char* keyname;
        int colno;

        ss_pprintf_3(("dd_createsyncispublhistoryversionkey\n"));

        relid = rs_relh_relid(cd, relh);

        keyname = SsMemAlloc(strlen(RSK_SYNCHIST_VERKEYNAMESTR_NEW) +
                             strlen(RS_ANAME_SYNCTUPLEVERS) +
                             (RSK_RELID_DISPLAYSIZE + 1) +
                             1);
        SsSprintf(keyname, RSK_SYNCHIST_VERKEYNAMESTR_NEW, relid, RS_ANAME_SYNCTUPLEVERS);
        colno = 0;

        columns[colno] = RS_ANAME_SYNC_ISPUBLTUPLE;
        desc[colno] = FALSE;
        colno++;
        columns[colno] = RS_ANAME_SYNCTUPLEVERS;
        desc[colno] = FALSE;

        succp = tb_createindex_ext(
                    cd,
                    trans,
                    keyname,
                    rs_relh_schema(cd, relh),
                    rs_relh_catalog(cd, relh),
                    relh,
                    ttype,
                    FALSE,          /* unique */
                    SYNCISPUBLHISTORYKEYATTRIB_COUNT,
                    columns,
                    desc,
#ifdef SS_COLLATION
                    NULL,
#endif /* SS_COLLATION */
                    TB_DD_CREATEREL_SYNC,
                    p_errh);

        SsMemFree(keyname);

        return(succp);
}

static bool dd_createsynchistoryversionkey(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_ttype_t* ttype,
        rs_err_t** p_errh)
{
        bool succp;
        char* columns[SYNCHISTORYKEYATTRIB_COUNT];
        bool desc[SYNCHISTORYKEYATTRIB_COUNT];
        long relid;
        char* keyname;
        int colno;

        ss_pprintf_3(("dd_createsynchistoryversionkey\n"));

        relid = rs_relh_relid(cd, relh);

        keyname = SsMemAlloc(strlen(RSK_SYNCHIST_VERKEYNAMESTR_2_NEW) +
                             strlen(RS_ANAME_SYNCTUPLEVERS) +
                             (RSK_RELID_DISPLAYSIZE + 1) +
                             1);
        SsSprintf(keyname, RSK_SYNCHIST_VERKEYNAMESTR_2_NEW, relid, RS_ANAME_SYNCTUPLEVERS);
        colno = 0;

        columns[colno] = RS_ANAME_SYNCTUPLEVERS;
        desc[colno] = FALSE;

        succp = tb_createindex_ext(
                    cd,
                    trans,
                    keyname,
                    rs_relh_schema(cd, relh),
                    rs_relh_catalog(cd, relh),
                    relh,
                    ttype,
                    FALSE,          /* unique */
                    SYNCHISTORYKEYATTRIB_COUNT,
                    columns,
                    desc,
#ifdef SS_COLLATION
                    NULL,
#endif /* SS_COLLATION */
                    TB_DD_CREATEREL_SYNC,
                    p_errh);

        SsMemFree(keyname);

        return(succp);
}

static bool dd_copysynchistorykey(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_key_t* key,
        rs_ttype_t* ttype,
        rs_err_t** p_errh)
{
        int i;
        int col_c;
        int nordering;
        int alloc_c;
        char** columns;
        bool* desc;
        bool succp = TRUE;
        char* keyname;
        char* relname;

        ss_dprintf_3(("dd_copysynchistorykey\n"));

        relname = rs_relh_name(cd, relh);

        nordering = rs_key_lastordering(cd, key) + 1;
        alloc_c = nordering + 1;
        columns = SsMemAlloc(alloc_c * sizeof(columns[0]));
        desc = SsMemAlloc(alloc_c * sizeof(desc[0]));

        /* Add all user defined columns of this key to history table
         * key.
         */
        col_c = 0;
        for (i = 0; i < nordering; i++) {
            if (rs_keyp_parttype(cd, key, i) == RSAT_USER_DEFINED) {
                rs_ano_t ano;
                ano = rs_keyp_ano(cd, key, i);
                columns[col_c] = rs_ttype_aname(cd, ttype, ano);
                desc[col_c] = !rs_keyp_isascending(cd, key, i);
                col_c++;
            }
        }

        /* Genarate key name. */
        if (rs_key_issyskey(cd, key)) {
            keyname = SsMemAlloc(strlen(RSK_SYNCHIST_SYSKEYNAMESTR) + strlen(relname) + 21);
            SsSprintf(keyname, RSK_SYNCHIST_SYSKEYNAMESTR, relname, rs_key_id(cd, key));
        } else {
            char* oldname;
            oldname = rs_key_name(cd, key);
            keyname = SsMemAlloc(strlen(RSK_SYNCHIST_USERKEYNAMESTR) + strlen(oldname) + 1);
            SsSprintf(keyname, RSK_SYNCHIST_USERKEYNAMESTR, oldname);
        }

        if (col_c > 0) {
            /* There were columns, create the key.
             * First add sync tuple version column.
             */
            columns[col_c] = RS_ANAME_SYNCTUPLEVERS;
            desc[col_c] = FALSE;
            col_c++;

            succp = tb_createindex_ext(
                        cd,
                        trans,
                        keyname,
                        rs_relh_schema(cd, relh),
                        rs_relh_catalog(cd, relh),
                        relh,
                        rs_relh_ttype(cd, relh),
                        FALSE,          /* unique */
                        col_c,
                        columns,
                        desc,
#ifdef SS_COLLATION
                        NULL,
#endif /* SS_COLLATION */
                        TB_DD_CREATEREL_SYNC,
                        p_errh);
        }

        SsMemFree(columns);
        SsMemFree(desc);
        SsMemFree(keyname);

        return(succp);
}

static bool dd_createsynchistorytable(
        rs_sysinfo_t*   cd,
        tb_trans_t*     trans,
        rs_relh_t*      relh,
        rs_err_t**      p_errh)
{
        int             i;
        char*           histrelname;
        bool            succp = TRUE;
        rs_ttype_t*     relh_ttype;
        su_pa_t*        keys;
        rs_key_t*       key;
        rs_relh_t*      sync_relh = NULL;
        char*           baserelname;
        bool            addhistorycols;
        rs_relh_t*      relh_refresh = NULL;
        rs_ttype_t*     new_ttype = NULL;

        ss_dprintf_3(("dd_createsynchistorytable\n"));
        ss_assert(ss_vers_issync());

        baserelname = rs_relh_name(cd, relh);

        if (rs_relh_primkey(cd, relh) == NULL) {
            rs_error_create(p_errh, E_NOPRIMKEY_S, baserelname);
            return(FALSE);
        }

        baserelname = rs_relh_name(cd, relh);
        histrelname = rs_sdefs_buildsynchistorytablename(baserelname);

        relh_ttype = rs_relh_ttype(cd, relh);

        if (succp) {
            /* Check if history columns already exist in base table. */
            int ano;
            ano = rs_ttype_anobyname(cd, relh_ttype, RS_ANAME_SYNCTUPLEVERS);
            addhistorycols = (ano == RS_ANO_NULL);
        }

        if (succp && addhistorycols) {
            /* Create history version attribute and key to base table.
             */
            su_ret_t        rc;
            rs_ano_t        ano_use = RS_ANO_NULL;
            long            vers_attrid;
            rs_atype_t*     vers_atype;
            long            ispubl_attrid;
            rs_atype_t*     ispubl_atype;
            su_list_t*      attrlist;
            rs_atype_t*     def_atype;
            rs_aval_t*      def_aval;

            attrlist = tb_dd_attrlist_init();

            /* Add history 'ispubl' attribute. */
            ispubl_atype = rs_atype_initsyncispubltuple(cd, FALSE);

            rs_atype_setnullallowed(cd, ispubl_atype, TRUE);

            ano_use = dd_ttype_getnewano(cd, relh_ttype);

            tb_dd_attrlist_add(
                attrlist,
                RS_ANAME_SYNC_ISPUBLTUPLE,
                ispubl_atype,
                rs_sysi_auth(cd),
                NULL,
                NULL,
                &ispubl_attrid,
                TRUE);

            vers_atype = rs_atype_initsynctuplevers(cd, FALSE);

            rs_atype_setnullallowed(cd, vers_atype, TRUE);

            /* use VARCHAR define */
            def_atype = rs_atype_create(cd, "VARCHAR", NULL, TRUE, p_errh);
            ss_dassert(def_atype != NULL);
            def_aval = rs_aval_create(cd, def_atype);

            /* avoid literals */
            succp = rs_aval_set8bitstr_ext(cd, def_atype, def_aval, "0000000000000000", NULL);
            ss_dassert(succp);

            tb_dd_attrlist_add(
                    attrlist,
                    RS_ANAME_SYNCTUPLEVERS,
                    vers_atype,
                    rs_sysi_auth(cd),
                    def_atype,
                    def_aval,
                    &vers_attrid,
                    TRUE);

            rc = tb_dd_addattributelist(
                    cd,
                    trans,
                    relh,
                    attrlist,
                    FALSE,
                    p_errh);
            succp = (rc == SU_SUCCESS);
            tb_dd_attrlist_done(attrlist);

            rs_aval_free(cd, def_atype, def_aval);
            rs_atype_free(cd, def_atype);
            rs_atype_free(cd, ispubl_atype);
            rs_atype_free(cd, vers_atype);
        }

        /*
         * Create indexes
         */
        if (succp) {

            TliConnectT* tcon;

            succp = tb_trans_stmt_commitandbegin(cd, trans, p_errh);

            if (succp) {
                su_ret_t    rc;

                tcon = TliConnectInitByTrans(cd, trans);
                relh_refresh = dd_readrelh(tcon,
                                           rs_relh_relid(cd, relh),
                                           DD_READRELH_FULL,
                                           FALSE,
                                           NULL,
                                           NULL);
                if (relh_refresh == NULL) {
                    rc = E_RELNOTEXIST_S;
                    rs_error_create(
                            p_errh,
                            E_RELNOTEXIST_S,
                            rs_relh_name(cd, relh));
                    succp = FALSE;
                }

                if (succp) {
                    new_ttype = rs_relh_ttype(cd, relh_refresh);

                    succp = dd_createsyncispublhistoryversionkey(
                            cd,
                            trans,
                            relh_refresh,
                            new_ttype,
                            p_errh);

                    if (!succp) {
                        int errcode;
                        errcode = su_err_geterrcode(*p_errh);
                        if (errcode == E_KEYNAMEEXIST_S) {
                            succp = TRUE;
                            su_err_done(*p_errh);
                        }
                    }
                }

                if (succp) {
                    succp = dd_createsynchistoryversionkey(
                                cd,
                                trans,
                                relh_refresh,
                                new_ttype,
                                p_errh);

                    if (!succp) {
                        int errcode;
                        errcode = su_err_geterrcode(*p_errh);
                        if (errcode == E_KEYNAMEEXIST_S) {
                            succp = TRUE;
                            su_err_done(*p_errh);
                        }
                    }
                }

                if (!succp) {
                    ss_pprintf_3(("dd_createsynchistorytable:'%.80s'\n", rs_error_geterrstr(cd, *p_errh)));
                }

                TliConnectDone(tcon);
            }

        }

        if (succp) {
            /* Create history relation. */
            ss_dassert(relh_refresh != NULL);
            ss_dassert(new_ttype != NULL);
            succp = tb_createrelation_ext(
                        cd,
                        trans,
                        histrelname,
                        rs_relh_schema(cd, relh_refresh),
                        rs_relh_catalog(cd, relh_refresh),
                        NULL,
                        new_ttype,
                        NULL,                       /* primkey */
                        0,                          /* unique_c */
                        NULL,                       /* unique */
                        0,                          /* forkey_c */
                        NULL,                       /* forkeys */
                        NULL,                       /* def */
                        NULL,                       /* defvalue */
                        0,                          /* check_c */
                        NULL,                       /* checks */
                        NULL,                       /* checknames */
                        TB_DD_CREATEREL_SYNC,
                        (rs_relh_istransient(cd, relh)
                         ? TB_DD_PERSISTENCY_TRANSIENT
                         : TB_DD_PERSISTENCY_PERMANENT),
                        ((rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY)
                         ? TB_DD_STORE_MEMORY
                         : TB_DD_STORE_DISK),
                        /* XXX - should we use relaxed durability if
                           the base table is relaxed? */
                        TB_DD_DURABILITY_STRICT,
                        TB_RELMODE_SYSDEFAULT,
                        &sync_relh,
                        p_errh);
        }

        if (succp) {
            /* Create keys to history table. All primary and unique keys are
             * created as normal keys.
             */
            keys = rs_relh_keys(cd, relh);
            su_pa_do_get(keys, i, key) {
                succp = dd_copysynchistorykey(
                            cd,
                            trans,
                            sync_relh,
                            key,
                            relh_ttype,
                            p_errh);
                if (!succp) {
                    break;
                }
            }
            if (succp) {
                succp = tb_trans_stmt_commitandbegin(cd, trans, p_errh);
            }
            if (succp) {
                rs_ttype_t* sync_relh_ttype;
                sync_relh_ttype = rs_relh_ttype(cd, sync_relh);

                succp = dd_createsynchistoryversionkey(
                            cd,
                            trans,
                            sync_relh,
                            sync_relh_ttype,
                            p_errh);
            }
        }

        if (relh_refresh != NULL) {
            SS_MEM_SETUNLINK(relh_refresh);
            rs_relh_done(cd, relh_refresh);
        }

        SsMemFree(histrelname);
        if (sync_relh != NULL) {
            SS_MEM_SETUNLINK(sync_relh);
            rs_relh_done(cd, sync_relh);
        }

        return(succp);
}

static bool dd_dropsynchistorytable(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_err_t** p_errh)
{
        bool succp;
        char* relname;

        ss_assert(ss_vers_issync());

        relname = rs_sdefs_buildsynchistorytablename(rs_relh_name(cd, relh));

        succp = tb_droprelation_ext(
                    cd,
                    trans,
                    FALSE,
                    relname,
                    rs_relh_schema(cd, relh),
                    rs_relh_catalog(cd, relh),
                    FALSE,
                    TRUE,
                    NULL,
                    TRUE,
                    p_errh);

        SsMemFree(relname);

#ifndef SS_MYSQL
        if (succp) {
            succp = tb_hcol_removeallcolumns(
                        cd,
                        trans,
                        relh,
                        p_errh);
        }
#endif /* !SS_MYSQL */

        return(succp);
}

#endif /* SS_SYNC */
#endif /* !SS_MYSQL */

static bool dd_setrelmodes(
        rs_sysinfo_t* cd,
        TliConnectT* tcon,
        rs_relh_t* relh,
        long relid,
        tb_relmodemap_t relmodes,
        bool createrel,
        rs_err_t** p_errh)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* relmodestr = NULL;
        dt_date_t date;
        rs_auth_t* auth;
        char* schema;
        dstr_t relmode_ds = NULL;
        dstr_t new_relmodes_ds = NULL;
        bool changerelmode = TRUE;
        bool succp = TRUE;
        rs_entname_t* en;

        ss_dprintf_3(("dd_setrelmodes\n"));

        en = rs_relh_entname(cd, relh);

        auth = rs_sysi_auth(cd);

        if (rs_relh_issysrel(cd, relh)) {
            /* System table mode cannot be changed.
             */
            rs_error_create(p_errh, E_NOPRIV);
            return(FALSE);
        }

        if (!createrel) {
            if (!tb_trans_setrelhchanged(
                        cd,
                        TliGetTrans(tcon),
                        relh,
                        p_errh)) {
                return FALSE;
            }
        }

        if (relmodes & TB_RELMODE_MAINMEMORY) {
            if (!dbe_db_ismme(rs_sysi_db(cd))) {
                rs_error_create(p_errh, E_MMENOSUP);
                return(FALSE);
            }
            if (!su_li3_ismainmemsupp()) {
                rs_error_create(p_errh, E_MMENOLICENSE);
                return(FALSE);
            }
#ifndef SS_MME
            if (su_pa_nelems(rs_relh_refkeys(cd, relh)) != 0) {
                /* Foreign keys are not supported with main memory
                 * tables.
                 */
                rs_error_create(p_errh, E_MMINOFORKEY);
                return(FALSE);
            }
#endif
#if !defined(SS_MME)
            if (!ssfile_diskless && !tb_dd_enablemainmem) {
                rs_error_create(p_errh, E_MMIONLYFORDISKLESS);
                return(FALSE);
            }
#endif /* !SS_MME */
        }

        if (relmodes & TB_RELMODE_TRANSIENT) {
            if (rs_relh_reltype(cd, relh) != RS_RELTYPE_MAINMEMORY
                && !(relmodes & TB_RELMODE_MAINMEMORY)) {
                rs_error_create(p_errh, E_TRANSIENTONLYFORMME);
                return FALSE;
            }
            if (rs_relh_isglobaltemporary(cd, relh) ||
                (relmodes & TB_RELMODE_GLOBALTEMPORARY)) {
                rs_error_create(p_errh, E_TEMPORARYNOTRANSIENT);
                return FALSE;
            }
        }

        if (relmodes & TB_RELMODE_GLOBALTEMPORARY) {
            if (rs_relh_reltype(cd, relh) != RS_RELTYPE_MAINMEMORY
                && !(relmodes & TB_RELMODE_MAINMEMORY))
            {
                rs_error_create(p_errh, E_TEMPORARYONLYFORMME);
                return FALSE;
            }
            if (rs_relh_istransient(cd, relh)) {
                rs_error_create(p_errh, E_TRANSIENTNOTEMPORARY);
                return FALSE;
            }
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLEMODES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLEMODES_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLEMODES_MODE, &relmodestr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_TABLEMODES_MODTIME, &date);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLEMODES_MODUSER, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLEMODES_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (relmodestr != NULL) {
            dstr_set(&relmode_ds, relmodestr);
        }

        if (relmodes & TB_RELMODE_OPTIMISTIC) {
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_OPTIMISTIC) != NULL) {
                changerelmode = FALSE;
            } else if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_MAINMEMORY)
                       != NULL)
            {
                succp = FALSE;
                rs_error_create(p_errh, E_ILLTABMODECOMB);
            } else {
                rs_relh_setreltype(cd, relh, RS_RELTYPE_OPTIMISTIC);
                if (new_relmodes_ds != NULL) {
                    dstr_app(&new_relmodes_ds, " ");
                    dstr_app(&new_relmodes_ds, DD_RELMODE_OPTIMISTIC);
                } else {
                    dstr_set(&new_relmodes_ds, DD_RELMODE_OPTIMISTIC);
                }
                dd_removerelmodestr(relmode_ds, (char *)DD_RELMODE_PESSIMISTIC);
            }
        }
#ifndef SS_NOLOCKING
        if (relmodes & TB_RELMODE_PESSIMISTIC) {
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_PESSIMISTIC)
                != NULL)
            {
                changerelmode = FALSE;
            } else {
                rs_relh_setreltype(cd, relh, RS_RELTYPE_PESSIMISTIC);
                if (new_relmodes_ds != NULL) {
                    dstr_app(&new_relmodes_ds, " ");
                    dstr_app(&new_relmodes_ds, DD_RELMODE_PESSIMISTIC);
                } else {
                    dstr_set(&new_relmodes_ds, DD_RELMODE_PESSIMISTIC);
                }
                dd_removerelmodestr(relmode_ds, (char *)DD_RELMODE_OPTIMISTIC);
            }
        }
#endif /* SS_NOLOCKING */
        if (relmodes & TB_RELMODE_NOCHECK) {
            if (rs_sysi_ishsbconfigured(cd)) {
                changerelmode = FALSE;
            } else if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_NOCHECK)
                       != NULL)
            {
                changerelmode = FALSE;
            } else {
                rs_relh_setnocheck(cd, relh);
                if (new_relmodes_ds != NULL) {
                    dstr_app(&new_relmodes_ds, " ");
                    dstr_app(&new_relmodes_ds, DD_RELMODE_NOCHECK);
                } else {
                    dstr_set(&new_relmodes_ds, DD_RELMODE_NOCHECK);
                }
                dd_removerelmodestr(relmode_ds, (char *)DD_RELMODE_CHECK);
            }
        }
        if (relmodes & TB_RELMODE_CHECK) {
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_CHECK) != NULL) {
                changerelmode = FALSE;
            } else {
                rs_relh_setcheck(cd, relh);
                if (new_relmodes_ds != NULL) {
                    dstr_app(&new_relmodes_ds, " ");
                    dstr_app(&new_relmodes_ds, DD_RELMODE_CHECK);
                } else {
                    dstr_set(&new_relmodes_ds, DD_RELMODE_CHECK);
                }
                dd_removerelmodestr(relmode_ds, (char *)DD_RELMODE_NOCHECK);
            }
        }
        if (relmodes & TB_RELMODE_MAINMEMORY) {
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_MAINMEMORY) != NULL) {
                changerelmode = FALSE;
            } else {
                if (!createrel && !dd_checkifrelempty_tcon(tcon, en)) {
                    succp = FALSE;
                    rs_error_create(
                            p_errh,
                            E_RELNOTEMPTY_S,
                            rs_entname_getname(en));
                } else if (rs_relh_issync(cd, relh)) {
                    succp = FALSE;
                    rs_error_create(
                            p_errh,
                            E_CANNOTCHANGESTOREIFSYNCHIST);
                } else {
                    rs_relh_setreltype(cd, relh, RS_RELTYPE_MAINMEMORY);
                    if (new_relmodes_ds != NULL) {
                        dstr_app(&new_relmodes_ds, " ");
                        dstr_app(&new_relmodes_ds, DD_RELMODE_MAINMEMORY);
                    } else {
                        dstr_set(&new_relmodes_ds, DD_RELMODE_MAINMEMORY);
                    }
                    dd_removerelmodestr(relmode_ds, (char *)DD_RELMODE_OPTIMISTIC);
                }
            }
        }
        if (relmodes & TB_RELMODE_DISK) {
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_MAINMEMORY) != NULL) {
                /* We have main memory mode set. Remove main memory mode
                 * which implies disk based table.
                 */
                if (!createrel && !dd_checkifrelempty_tcon(tcon, en)) {
                    succp = FALSE;
                    rs_error_create(
                            p_errh,
                            E_RELNOTEMPTY_S,
                            rs_entname_getname(en));
                } else if (rs_relh_issync(cd, relh)) {
                    succp = FALSE;
                    rs_error_create(
                            p_errh,
                            E_CANNOTCHANGESTOREIFSYNCHIST);
                } else if (rs_relh_istransient(cd, relh)
                           || (relmodes & TB_RELMODE_TRANSIENT)) {
                    succp = FALSE;
                    rs_error_create(p_errh, E_TRANSIENTONLYFORMME);
                } else if (rs_relh_isglobaltemporary(cd, relh)
                           || (relmodes & TB_RELMODE_GLOBALTEMPORARY)) {
                    succp = FALSE;
                    rs_error_create(p_errh, E_TEMPORARYONLYFORMME);
                } else {
                    changerelmode = TRUE;
                    dd_removerelmodestr(relmode_ds, (char *)DD_RELMODE_MAINMEMORY);
                    if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_PESSIMISTIC) != NULL) {
                        /* If we have explicit pessimistic setting then we
                         * set also disk table to pessimistic.
                         */
                        rs_relh_setreltype(cd, relh, RS_RELTYPE_PESSIMISTIC);
                    } else {
                        rs_relh_setreltype(cd, relh, RS_RELTYPE_OPTIMISTIC);
                    }
                }
            } else {
                changerelmode = FALSE;
            }
        }
#ifdef SS_SYNC
        if (relmodes & TB_RELMODE_SYNC) {
            ss_assert(ss_vers_issync());
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_SYNC) != NULL) {
                changerelmode = FALSE;
            } else {
#if 0 /* JarmoR enabled SYNC for MAINMEMORY Aug 23, 2001 */
                if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_MAINMEMORY) != NULL) {
                    rs_error_create(p_errh, E_ILLTABMODECOMB);
                    succp = FALSE;
                } else {
                    if (new_relmodes_ds != NULL) {
                        dstr_app(&new_relmodes_ds, " ");
                        dstr_app(&new_relmodes_ds, DD_RELMODE_SYNC);
                    } else {
                        dstr_set(&new_relmodes_ds, DD_RELMODE_SYNC);
                    }
                }
#else
                if (new_relmodes_ds != NULL) {
                    dstr_app(&new_relmodes_ds, " ");
                    dstr_app(&new_relmodes_ds, DD_RELMODE_SYNC);
                } else {
                    dstr_set(&new_relmodes_ds, DD_RELMODE_SYNC);
                }
#endif
            }
        }
        if (relmodes & TB_RELMODE_NOSYNC) {
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_SYNC) != NULL) {
                /* rs_relh_setsync(cd, relh, FALSE); jarmor 250998 */
                dd_removerelmodestr(relmode_ds, (char *)DD_RELMODE_SYNC);
            } else {
                changerelmode = FALSE;
            }
        }
#endif /* SS_SYNC */
        if (relmodes & TB_RELMODE_TRANSIENT) {
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_TRANSIENT) != NULL) {
                changerelmode = FALSE;
            } else {
                if (createrel || dd_checkifrelempty_tcon(tcon, en)) {
                    rs_relh_settransient(cd, relh, TRUE);
                    if (new_relmodes_ds != NULL) {
                        dstr_app(&new_relmodes_ds, " ");
                        dstr_app(&new_relmodes_ds, DD_RELMODE_TRANSIENT);
                    } else {
                        dstr_set(&new_relmodes_ds, DD_RELMODE_TRANSIENT);
                    }

                } else {
                    succp = FALSE;
                    rs_error_create(
                            p_errh,
                            E_RELNOTEMPTY_S,
                            rs_entname_getname(en));
                }
            }
        }
        if (relmodes & TB_RELMODE_GLOBALTEMPORARY) {
            if (dd_findrelmodestr(relmode_ds, (char *)DD_RELMODE_GLOBALTEMPORARY)
                != NULL)
            {
                changerelmode = FALSE;
            } else {
                if (createrel || dd_checkifrelempty_tcon(tcon, en)) {
                    rs_relh_setglobaltemporary(cd, relh, TRUE);
                    if (new_relmodes_ds != NULL) {
                        dstr_app(&new_relmodes_ds, " ");
                        dstr_app(&new_relmodes_ds, DD_RELMODE_GLOBALTEMPORARY);
                    } else {
                        dstr_set(&new_relmodes_ds, DD_RELMODE_GLOBALTEMPORARY);
                    }

                } else {
                    succp = FALSE;
                    rs_error_create(
                            p_errh,
                            E_RELNOTEMPTY_S,
                            rs_entname_getname(en));
                }
            }
        }

        if (succp && changerelmode) {

            tb_trans_t* trans = TliGetTrans(tcon);

            if (new_relmodes_ds != NULL) {
                if (relmode_ds == NULL) {
                    dstr_set(&relmode_ds, new_relmodes_ds);
                } else {
                    dstr_app(&relmode_ds, " ");
                    dstr_app(&relmode_ds, new_relmodes_ds);
                }
            }

            dd_trimspaces(relmode_ds);

            relmodestr = relmode_ds;
            schema = rs_auth_username(cd, auth);
            ss_dassert(schema != NULL);

            date = tb_dd_curdate();

            tb_trans_setstmtgroup(cd, trans, TRUE);

            if (trc == TLI_RC_SUCC) {
                /* Mode found. Update old mode.
                 */
                if (relmodestr[0] == '\0') {
                    /* If the mode became empty, remove it rather than leave
                       an empty entry into SYS_RELMODES. */
                    trc = TliCursorDelete(tcur);
                } else {
                    trc = TliCursorUpdate(tcur);
                }
                ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
            } else {
                /* Mode not found, Add a new mode setting.
                 */
                trc = TliCursorInsert(tcur);
                ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
            }

#ifndef SS_MYSQL
#ifdef SS_SYNC
            if (succp) {
                if (relmodes & TB_RELMODE_SYNC) {
                    tb_trans_stmt_begin(cd, trans);
                    succp = dd_createsynchistorytable(
                            cd,
                            trans,
                            relh,
                            p_errh);
                    if (succp) {
                        succp = tb_trans_stmt_commit_onestep(cd, trans, p_errh);
                    }
                }
                if (relmodes & TB_RELMODE_NOSYNC) {
                    tb_trans_stmt_begin(cd, trans);
                    succp = dd_dropsynchistorytable(
                            cd,
                            trans,
                            relh,
                            p_errh);
                    if (succp) {
                        succp = tb_trans_stmt_commit_onestep(cd, trans, p_errh);
                    }
                }
                if (!(relmodes & (TB_RELMODE_NOSYNC | TB_RELMODE_SYNC)) &&
                    rs_relh_issync(cd, relh))
                {
                    succp = FALSE;
                    rs_error_create(p_errh, E_SYNCHISTREL);
                }
            }
#endif /* SS_SYNC */
#endif /* !SS_MYSQL */
            tb_trans_stmt_begin(cd, trans);
            tb_trans_setstmtgroup(cd, trans, FALSE);
        }

        TliCursorFree(tcur);
        dstr_free(&relmode_ds);
        dstr_free(&new_relmodes_ds);

        return(succp);
}

#if 0 /* pete removed */
/*#***********************************************************************\
 *
 *              dd_setrelmode
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      relmode -
 *
 *
 *      p_errh -
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
static bool dd_setrelmode(
        rs_sysinfo_t* cd,
        TliConnectT* tcon,
        rs_relh_t* relh,
        tb_relmode_t relmode,
        bool createrel,
        rs_err_t** p_errh)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* relmodestr = NULL;
        char* new_relmodestr;
        long relid;
        dt_date_t date;
        rs_auth_t* auth;
        char* schema;
        dstr_t relmode_ds = NULL;
        bool changerelmode = TRUE;
        bool succp = TRUE;
        rs_entname_t* en;

        ss_dprintf_3(("dd_setrelmode\n"));

        en = rs_relh_entname(cd, relh);

        relid = rs_relh_relid(cd, relh);
        auth = rs_sysi_auth(cd);

        if (rs_relh_issysrel(cd, relh)) {
            /* System table mode cannot be changed.
             */
            rs_error_create(p_errh, E_NOPRIV);
            return(FALSE);
        }
        if (relmode == TB_RELMODE_MAINMEMORY) {
#ifndef SS_MME
            if (su_pa_nelems(rs_relh_refkeys(cd, relh)) != 0) {
                /* Foreign keys are not supported with main memory
                 * tables.
                 */
                rs_error_create(p_errh, E_MMINOFORKEY);
                return(FALSE);
            }
#endif
#if !defined(SS_MME)
            if (!ssfile_diskless && !tb_dd_enablemainmem) {
                rs_error_create(p_errh, E_MMIONLYFORDISKLESS);
                return(FALSE);
            }
#endif /* !SS_MME */
        }

        if (relmode == TB_RELMODE_TRANSIENT) {
            if (rs_relh_reltype(cd, relh) != RS_RELTYPE_MAINMEMORY) {
                rs_error_create(p_errh, E_TRANSIENTONLYFORMME);
                return FALSE;
            }
            if (rs_relh_isglobaltemporary(cd, relh)) {
                rs_error_create(p_errh, E_TEMPORARYNOTRANSIENT);
                return FALSE;
            }
        }

        if (relmode == TB_RELMODE_GLOBALTEMPORARY) {
            if (rs_relh_reltype(cd, relh) != RS_RELTYPE_MAINMEMORY) {
                rs_error_create(p_errh, E_TEMPORARYONLYFORMME);
                return FALSE;
            }
            if (rs_relh_istransient(cd, relh)) {
                rs_error_create(p_errh, E_TRANSIENTNOTEMPORARY);
                return FALSE;
            }
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLEMODES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLEMODES_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLEMODES_MODE, &relmodestr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_TABLEMODES_MODTIME, &date);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLEMODES_MODUSER, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLEMODES_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (relmodestr != NULL) {
            dstr_set(&relmode_ds, relmodestr);
        }

        relid = rs_relh_relid(cd, relh);

        switch (relmode) {
            case TB_RELMODE_OPTIMISTIC:
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_OPTIMISTIC) != NULL) {
                    changerelmode = FALSE;
                } else if (dd_findrelmodestr(relmode_ds, DD_RELMODE_MAINMEMORY) != NULL) {
                    succp = FALSE;
                    rs_error_create(p_errh, E_ILLTABMODECOMB);
                } else {
                    rs_relh_setreltype(cd, relh, RS_RELTYPE_OPTIMISTIC);
                    new_relmodestr = DD_RELMODE_OPTIMISTIC;
                    dd_removerelmodestr(relmode_ds, DD_RELMODE_PESSIMISTIC);
                }
                break;
#ifndef SS_NOLOCKING
            case TB_RELMODE_PESSIMISTIC:
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_PESSIMISTIC) != NULL) {
                    changerelmode = FALSE;
                } else {
                    rs_relh_setreltype(cd, relh, RS_RELTYPE_PESSIMISTIC);
                    new_relmodestr = DD_RELMODE_PESSIMISTIC;
                    dd_removerelmodestr(relmode_ds, DD_RELMODE_OPTIMISTIC);
                }
                break;
#endif /* SS_NOLOCKING */
            case TB_RELMODE_NOCHECK:
                if (rs_sysi_ishsbconfigured(cd)) {
                    changerelmode = FALSE;
                } else if (dd_findrelmodestr(relmode_ds, DD_RELMODE_NOCHECK) != NULL) {
                    changerelmode = FALSE;
                } else {
                    rs_relh_setnocheck(cd, relh);
                    new_relmodestr = DD_RELMODE_NOCHECK;
                    dd_removerelmodestr(relmode_ds, DD_RELMODE_CHECK);
                }
                break;
            case TB_RELMODE_CHECK:
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_CHECK) != NULL) {
                    changerelmode = FALSE;
                } else {
                    rs_relh_setcheck(cd, relh);
                    new_relmodestr = DD_RELMODE_CHECK;
                    dd_removerelmodestr(relmode_ds, DD_RELMODE_NOCHECK);
                }
                break;
            case TB_RELMODE_MAINMEMORY:
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_MAINMEMORY) != NULL) {
                    changerelmode = FALSE;
                } else {
                    if (dd_checkifrelempty_tcon(tcon, en)) {
                        rs_relh_setreltype(cd, relh, RS_RELTYPE_MAINMEMORY);
                        new_relmodestr = DD_RELMODE_MAINMEMORY;
                        dd_removerelmodestr(relmode_ds, DD_RELMODE_OPTIMISTIC);
                    } else {
                        succp = FALSE;
                        rs_error_create(
                            p_errh,
                            E_RELNOTEMPTY_S,
                            rs_entname_getname(en));
                    }
                }
                break;
            case TB_RELMODE_DISK:
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_MAINMEMORY) != NULL) {
                    /* We have main memory mode set. Remove main memory mode
                     * which implies disk based table.
                     */
                    if (dd_checkifrelempty_tcon(tcon, en)) {
                        changerelmode = TRUE;
                        new_relmodestr = NULL;
                        dd_removerelmodestr(relmode_ds, DD_RELMODE_MAINMEMORY);
                        if (dd_findrelmodestr(relmode_ds, DD_RELMODE_PESSIMISTIC) != NULL) {
                            /* If we have explicit pessimistic setting then we
                            * set also disk table to pessimistic.
                            */
                            rs_relh_setreltype(cd, relh, RS_RELTYPE_PESSIMISTIC);
                        } else {
                            rs_relh_setreltype(cd, relh, RS_RELTYPE_OPTIMISTIC);
                        }
                    } else {
                        succp = FALSE;
                        rs_error_create(
                            p_errh,
                            E_RELNOTEMPTY_S,
                            rs_entname_getname(en));
                    }
                } else {
                    changerelmode = FALSE;
                }
                break;
#ifdef SS_SYNC
            case TB_RELMODE_SYNC:
                ss_assert(ss_vers_issync());
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_SYNC) != NULL) {
                    changerelmode = FALSE;
                } else {
#if 0 /* JarmoR enabled SYNC for MAINMEMORY Aug 23, 2001 */
                    if (dd_findrelmodestr(relmode_ds, DD_RELMODE_MAINMEMORY) != NULL) {
                        rs_error_create(p_errh, E_ILLTABMODECOMB);
                        succp = FALSE;
                    } else {
                        new_relmodestr = DD_RELMODE_SYNC;
                    }
#else
                    new_relmodestr = DD_RELMODE_SYNC;
#endif
                }
                break;
            case TB_RELMODE_NOSYNC:
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_SYNC) != NULL) {
                    /* rs_relh_setsync(cd, relh, FALSE); jarmor 250998 */
                    dd_removerelmodestr(relmode_ds, DD_RELMODE_SYNC);
                    new_relmodestr = NULL;
                } else {
                    changerelmode = FALSE;
                }
                break;
#endif /* SS_SYNC */
            case TB_RELMODE_TRANSIENT:
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_TRANSIENT) != NULL) {
                    changerelmode = FALSE;
                } else {
                    if (dd_checkifrelempty_tcon(tcon, en)) {
                        rs_relh_settransient(cd, relh, TRUE);
                        new_relmodestr = DD_RELMODE_TRANSIENT;
                    } else {
                        succp = FALSE;
                        rs_error_create(
                            p_errh,
                            E_RELNOTEMPTY_S,
                            rs_entname_getname(en));
                    }
                }
                break;
            case TB_RELMODE_GLOBALTEMPORARY:
                if (dd_findrelmodestr(relmode_ds, DD_RELMODE_GLOBALTEMPORARY) != NULL) {
                    changerelmode = FALSE;
                } else {
                    if (dd_checkifrelempty_tcon(tcon, en)) {
                        rs_relh_setglobaltemporary(cd, relh, TRUE);
                        new_relmodestr = DD_RELMODE_GLOBALTEMPORARY;
                    } else {
                        succp = FALSE;
                        rs_error_create(
                            p_errh,
                            E_RELNOTEMPTY_S,
                            rs_entname_getname(en));
                    }
                }
                break;
            default:
                ss_error;
        }

        if (succp && changerelmode && !createrel) {
            succp = tb_trans_setrelhchanged(cd, TliGetTrans(tcon), relh, p_errh);
        }

        if (succp && changerelmode) {

            tb_trans_t* trans = TliGetTrans(tcon);

            if (new_relmodestr != NULL) {
                if (relmode_ds == NULL) {
                    dstr_set(&relmode_ds, new_relmodestr);
                } else {
                    dstr_app(&relmode_ds, " ");
                    dstr_app(&relmode_ds, new_relmodestr);
                }
            }

            dd_trimspaces(relmode_ds);

            relmodestr = relmode_ds;
            schema = rs_auth_username(cd, auth);
            ss_dassert(schema != NULL);

            date = tb_dd_curdate();

            tb_trans_setstmtgroup(cd, trans, TRUE);

            if (trc == TLI_RC_SUCC) {
                /* Mode found. Update old mode.
                 */
                trc = TliCursorUpdate(tcur);
                ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
            } else {
                /* Mode not found, Add a new mode setting.
                 */
                trc = TliCursorInsert(tcur);
                ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
            }

            succp = tb_trans_stmt_commit_onestep(cd, trans, p_errh);

#ifndef SS_MYSQL
#ifdef SS_SYNC
            if (succp) {
                switch (relmode) {
                    case TB_RELMODE_SYNC:
                        {
                            tb_trans_stmt_begin(cd, trans);
                            succp = dd_createsynchistorytable(
                                        cd,
                                        trans,
                                        relh,
                                        p_errh);
                            if (succp) {
                                succp = tb_trans_stmt_commit_onestep(cd, trans, p_errh);
                            }
                        }
                        break;
                    case TB_RELMODE_NOSYNC:
                        {
                            tb_trans_stmt_begin(cd, trans);
                            succp = dd_dropsynchistorytable(
                                        cd,
                                        trans,
                                        relh,
                                        p_errh);
                            if (succp) {
                                succp = tb_trans_stmt_commit_onestep(cd, trans, p_errh);
                            }
                        }
                        break;
                    default:
                        if (rs_relh_issync(cd, relh)) {
                            succp = FALSE;
                            rs_error_create(p_errh, E_SYNCHISTREL);
                        }
                        break;
                }
            }
#endif /* SS_SYNC */
#endif /* !SS_MYSQL */

            tb_trans_stmt_begin(cd, trans);
            tb_trans_setstmtgroup(cd, trans, FALSE);
        }

        TliCursorFree(tcur);
        dstr_free(&relmode_ds);

        return(succp);
}
#endif /* 0, pete removed */

/*##**********************************************************************\
 *
 *              tb_dd_setrelmode
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      relmode -
 *
 *
 *      p_errh -
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
bool tb_dd_setrelmode(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        tb_relmode_t relmode,
        bool createrel,
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        bool succp;

        ss_dprintf_1(("tb_dd_setrelmode\n"));

        tcon = TliConnectInitByTrans(cd, trans);

        succp = dd_setrelmodes(cd, tcon, relh, rs_relh_relid(cd, relh), (tb_relmodemap_t)relmode, createrel, p_errh);

        TliConnectDone(tcon);

        return(succp);
}

bool tb_dd_setrelmodes(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        tb_relmodemap_t relmodes,
        bool createrel,
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        bool succp;

        ss_dprintf_1(("tb_dd_setrelmode\n"));

        tcon = TliConnectInitByTrans(cd, trans);

        succp = dd_setrelmodes(cd, tcon, relh, rs_relh_relid(cd, relh), relmodes, createrel, p_errh);

        TliConnectDone(tcon);

        return(succp);
}

/*#***********************************************************************\
 *
 *              dd_createrelh
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      type -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_createrelh(
        TliConnectT* tcon,
        rs_relh_t* relh,
        tb_dd_createrel_t type,
        rs_entname_t* newname,
        long relationid)
{
        TliCursorT* tcur;
        TliRetT trc;
        long relid;
        char* relname;
        char* dynrelname = NULL;
        char* reltype;
        char* schema;
        char* checkstr;
        dt_date_t date;
        rs_ttype_t* ttype;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        char* catalog;

        ss_dprintf_3(("dd_createrelh\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLES_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &relname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_TYPE, &reltype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CHECK, &checkstr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_TABLES_CREATIME, &date);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        if (type == TB_DD_CREATEREL_SYSTEM) {
            relid = rs_relh_relid(cd, relh);
        } else {
            relid = relationid;
            if (type != TB_DD_CREATEREL_TRUNCATE) {
                rs_relh_setrelid(cd, relh, relid);
            }
        }
        if (newname == NULL) {
            newname = rs_relh_entname(cd, relh);
        }
        relname = rs_entname_getname(newname);
        schema = rs_entname_getschema(newname);
        ss_dassert(schema != NULL);
        catalog = rs_entname_getcatalog(newname);
        if (type == TB_DD_CREATEREL_SYNC) {
            reltype = (char *)RS_AVAL_SYNC_TABLE;
        } else if (type == TB_DD_CREATEREL_TRUNCATE) {
            reltype = (char *)RS_AVAL_TRUNCATE_TABLE;
        } else {
            reltype = (char *)RS_AVAL_BASE_TABLE;
        }
        checkstr = NULL;
        date = tb_dd_curdate();

        trc = TliCursorInsert(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        ttype = rs_relh_ttype(cd, relh);

        dd_createttype(tcon, relid, ttype, type);

        dd_createkeys(tcon, relh, relid, type);

        if (dynrelname != NULL) {
            SsMemFree(dynrelname);
        }
}

/*#***********************************************************************\
 *
 *              dd_dropkeyparts
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      keyid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_dropkeyparts(
        TliConnectT* tcon,
        long keyid)
{
        TliCursorT* tcur;
        TliRetT trc;

        ss_dprintf_3(("dd_dropkeyparts:keyid=%ld\n", keyid));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYPARTS_ID,
                TLI_RELOP_EQUAL,
                keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_droponekey
 *
 *  Deletes one one foreign key.
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 *      key -
 *
 *
 *      p_errh - out, give
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_droponekey(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_key_t* key,
        su_err_t** p_errh)
{
        TliCursorT* tcur;
        TliRetT trc;
        rs_sysinfo_t* cd __attribute__ ((unused));
        long relid;

        cd = TliGetCd(tcon);
        
        ss_dprintf_3(("dd_droponekey\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_REL_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYS_REL_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYS_ID,
                TLI_RELOP_EQUAL,
                rs_key_id(cd, key));
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END, TliCursorErrorCode(tcur));

        if (trc != TLI_RC_SUCC) {
            su_err_init(p_errh, E_DDOP);
            TliCursorFree(tcur);
            return(FALSE);
        }

        trc = TliCursorDelete(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        dd_dropkeyparts(tcon, rs_key_id(cd, key));

        return(TRUE);
}

/*#***********************************************************************\
 *
 *              dd_droprefkeys
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_droprefkeys(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_err_t** p_errh)
{
        TliCursorT* tcur;
        TliRetT     trc;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        char       *keyname;
        long        createrelid;
        long        relid;
        su_pa_t    *refkeys;
        int         i;
        rs_key_t   *key;
        dbe_ret_t   rc;
        tb_trans_t *trans = TliGetTrans(tcon);
        dbe_trx_t  *trx = tb_trans_dbtrx(cd, trans);
        rs_relh_t*  createrelh = NULL;

        refkeys = rs_relh_refkeys(cd, relh);
        if (su_pa_nelems(refkeys) == 0) {
            return TRUE;
        }

        relid = rs_relh_relid(cd, relh);

        ss_dprintf_3(("dd_droprefkeys:relid=%ld\n", relid));

        /* Drop referencing foreign keys */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);

        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_KEY_NAME, &keyname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_CREATE_REL_ID, &createrelid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                    tcur,
                    RS_ANAME_FORKEYS_REF_REL_ID,
                    TLI_RELOP_EQUAL,
                    relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        rc = dbe_trx_lockrelh(trx, relh, TRUE, 0);
        if (rc != DBE_RC_SUCC) {
            rs_error_create(p_errh, rc);
            return FALSE;
        }
        
        if (!tb_trans_setrelhchanged(cd, trans, relh, p_errh)) {
             TliCursorFree(tcur);
             return FALSE;
        }

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_relpriv_t* priv;
            su_pa_t *crefkeys;
            rs_key_t *mkey;

            su_pa_do_get(refkeys, i, mkey) {
                if (strcmp(rs_key_name(cd, mkey), keyname)==0) {
                    int i;
                    bool dropped;

                    if (createrelid == relid) {
                        createrelh = relh;
                    } else {
                        createrelh = tb_dd_getrelhbyid(cd, trans, createrelid, &priv, p_errh);
                    }

                    if (createrelh) {
                        crefkeys = rs_relh_refkeys(cd, createrelh);
                        dropped = FALSE;
                        su_pa_do_get(crefkeys, i, key) {
                            if (strcmp(rs_key_name(cd, key), keyname)==0) {
                                if (!dropped) {
                                    rc = dbe_trx_lockrelh(trx, createrelh, TRUE, 0);

                                    if (rc != DBE_RC_SUCC) {
                                        rs_error_create(p_errh, rc);
                                        goto error_exit;
                                    }
                                
                                    rc = dd_droprefkey(tcon, createrelh, keyname, p_errh);

                                    if (rc != SU_SUCCESS) {
                                        goto error_exit;
                                    }

                                    dropped = TRUE;

                                    if (!tb_trans_setrelhchanged(cd, trans, createrelh, p_errh)) {
                                        goto error_exit;
                                    }
                                }
                                rs_key_done(cd, key);
                                su_pa_remove(crefkeys, i);
                            }
                        }
                    

                        if (relh != createrelh) {
                            SS_MEM_SETUNLINK(createrelh);
                            rs_relh_done(cd, createrelh);
                        }
                    }
                }
            }
        }
        ss_dassert(trc == TLI_RC_END);
        TliCursorFree(tcur);

        su_pa_do_get(refkeys, i, key) {
            rs_key_done(cd, key);
        }
        su_pa_removeall(refkeys);

        return TRUE;

 error_exit:
        TliCursorFree(tcur);
        if (relh != createrelh) {
            SS_MEM_SETUNLINK(createrelh);
            rs_relh_done(cd, createrelh);
        }
        return FALSE;

}

/*#***********************************************************************\
 *
 *              dd_dropkeys
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_dropkeys(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        TliCursorT* tcur;
        TliRetT trc;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        long keyid;
        long relid;
        su_pa_t *keys;
        rs_key_t *key;
        int i;

        relid = rs_relh_relid(cd, relh);

        ss_dprintf_3(("dd_dropkeys:relid=%ld\n", relid));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_KEYS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        keys = rs_relh_keys(cd, relh);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            su_pa_do_get(keys, i, key) {
                if (rs_key_id (cd, key) == (ulong)keyid) {
                    dd_dropkeyparts(tcon, keyid);
                    trc = TliCursorDelete(tcur);
                    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
                    break;
                }
            }
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        return TRUE;
}

/*#***********************************************************************\
 *
 *              dd_setcardinaltransaoptions
 *
 * Updates transaction flags for cardinal update transaction. We need
 * special flags to be able to run this in HSB G2 secondary during checkpoint.
 *
 * Parameters :
 *
 *              tcon -
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
static void dd_setcardinaltransaoptions(TliConnectT* tcon)
{
#ifdef SS_HSBG2
        rs_sysinfo_t* cd;
        tb_trans_t* trans;

        cd = TliGetCd(tcon);
        trans = TliGetTrans(tcon);

        tb_trans_settransoption(cd, trans, TB_TRANSOPT_NOLOGGING);
        tb_trans_settransoption(cd, trans, TB_TRANSOPT_NOCHECK);

        tb_trans_beginif(cd, trans);
        tb_trans_setforcecommit(cd, trans);
#endif /* SS_HSBG2 */
}

/*##**********************************************************************\
 *
 *              tb_dd_dropcardinal
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_dd_dropcardinal(
        TliConnectT* tcon,
        long relid)
{
        TliCursorT* tcur;
        TliRetT trc;

        ss_dprintf_3(("tb_dd_dropcardinal:relid=%ld\n", relid));

        dd_setcardinaltransaoptions(tcon);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CARDINAL);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_CARDINAL_REL_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_CARDINAL_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while (TliCursorNext(tcur) == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }

        TliCursorFree(tcur);
}

static void dd_dropcolaux(
        TliConnectT*    tcon,
        ulong           attrid)
{
        TliCursorT* tcur;
        TliRetT trc;

        ss_dprintf_3(("dd_dropcolaux\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS_AUX);
        if (tcur == NULL) {
            /* This is possible when running startup SQLs. */
            return;
        }

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_ID, (long*)&attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_AUX_ID,
                TLI_RELOP_EQUAL,
                attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_dropttype
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_dropttype(
        TliConnectT* tcon,
        ulong relid)
{
        TliCursorT*     tcur;
        TliRetT         trc;
        long            attrid;

        ss_dprintf_3(("dd_dropttype\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_REL_ID, (long*)&relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_ID, &attrid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_COLUMNS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            dd_dropcolaux(tcon, attrid);
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_droprelmode
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
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
static void dd_droprelmode(
        TliConnectT* tcon,
        rs_relh_t* relh)
{
        TliCursorT* tcur;
        TliRetT trc;
        rs_sysinfo_t* cd __attribute__ ((unused));
        long relid;

        cd = TliGetCd(tcon);
        ss_dprintf_3(("dd_droprelmode\n"));

        relid = rs_relh_relid(cd, relh);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLEMODES);
        ss_dassert(tcur != NULL);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLEMODES_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (trc == TLI_RC_SUCC) {
            /* Mode found, delete it.
             */
            trc = TliCursorDelete(tcur);
        }

        TliCursorFree(tcur);
}

#ifdef REFERENTIAL_INTEGRITY

/*#***********************************************************************\
 *
 *              dd_droprefkeyparts
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      key -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_droprefkeyparts(
        TliConnectT* tcon,
        long key)
{
        TliCursorT* tcur;
        TliRetT trc;
        long keyid;

        ss_dprintf_3(("dd_droprefkeyparts\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYPARTS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ID, &keyid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYPARTS_ID,
                TLI_RELOP_EQUAL,
                key);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              tb_dd_droprefkeys_ext
 *
 *     Wrapper function for dd_droprefkeys
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_droprefkeys_ext(
    TliConnectT* tcon,
    rs_relh_t* relh,
    rs_err_t** p_errh)
{
        return dd_droprefkeys(tcon, relh, p_errh);
}

#endif /* REFERENTIAL_INTEGRITY */


/*#***********************************************************************\
 *
 *              dd_droprelh
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_droprelh(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_err_t** p_errh)
{
        TliCursorT* tcur;
        TliRetT trc;
        long relid;
        rs_sysinfo_t* cd __attribute__ ((unused));
#ifndef SS_MYSQL
        bool b;
#endif
        
        ss_dprintf_3(("dd_droprelh\n"));

        cd = TliGetCd(tcon);
        relid = rs_relh_relid(cd, relh);

        if (!dd_dropkeys(tcon, relh, p_errh)) {
            return FALSE;
        }

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CHECKSTRINGS);
        ss_dassert(tcur != NULL);

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_CHECKSTRINGS_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
        ss_rc_dassert(trc == TLI_RC_END, TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLES_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLES_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        if (trc == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

            TliCursorFree(tcur);

            dd_dropttype(tcon, relid);
            dd_droprelmode(tcon, relh);
#ifndef SS_NOPROCEDURE
#ifndef SS_MYSQL
            tb_trig_droprelh(tcon, relh);
#endif /* !SS_MYSQL */
#endif /* SS_NOPROCEDURE */
#ifdef SS_SYNC
#ifndef SS_MYSQL
            b = tb_hcol_removeallcolumns(cd, TliGetTrans(tcon), relh, NULL);
            ss_dassert(b);
#endif /* !SS_MYSQL */
#endif
            return(TRUE);
        } else {
            ss_rc_dassert(trc == TLI_RC_END, TliCursorErrorCode(tcur));
            TliCursorFree(tcur);
            rs_error_create(p_errh, E_DDOP);
            return(FALSE);
        }
}

#endif /* SS_NODDUPDATE */

#ifndef SS_NOVIEW

/*#***********************************************************************\
 *
 *              dd_readviewh
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      viewid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_viewh_t* dd_readviewh(
        TliConnectT* tcon,
        ulong viewid)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* viewname;
        char* viewtype;
        char* viewtext = (char *)"";
        char* schema;
        rs_viewh_t* viewh;
        rs_ttype_t* ttype;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        rs_entname_t en;
        char* catalog;

        ss_dprintf_3(("dd_readviewh\n"));

        ttype = dd_readttype(tcon, viewid, TRUE);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &viewname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_TYPE, &viewtype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLES_ID,
                TLI_RELOP_EQUAL,
                viewid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END, TliCursorErrorCode(tcur));

        if (trc != TLI_RC_SUCC) {
            rs_ttype_free(cd, ttype);
            TliCursorFree(tcur);
            return(NULL);
        }

        ss_dassert(strcmp(viewtype, RS_AVAL_VIEW) == 0);

        ss_dassert(schema != NULL);
        rs_entname_initbuf(&en,
                           catalog,
                           schema,
                           viewname);

        viewh = rs_viewh_init(
                    cd,
                    &en,
                    viewid,
                    ttype);

        rs_ttype_free(cd, ttype);

        TliCursorFree(tcur);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_VIEWS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_VIEWS_TEXT, &viewtext);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_VIEWS_V_ID,
                TLI_RELOP_EQUAL,
                viewid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        rs_viewh_setdef(cd, viewh, viewtext);

        TliCursorFree(tcur);

        return(viewh);
}

#ifndef SS_NODDUPDATE

/*#***********************************************************************\
 *
 *              dd_createviewh
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      viewh -
 *
 *
 *      auth -
 *
 *
 *      sysview -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_createviewh(
        TliConnectT* tcon,
        rs_viewh_t* viewh,
        rs_auth_t* auth __attribute__ ((unused)),
        bool sysview)
{
        TliCursorT* tcur;
        TliRetT trc;
        ulong viewid;
        char* viewname;
        char* viewtype;
        char* schema;
        dt_date_t date;
        char* viewtext;
        rs_ttype_t* ttype;
        rs_sysinfo_t* cd = TliGetCd(tcon);
        dbe_db_t* db = TliGetDb(tcon);
        char* catalog;

        ss_dprintf_3(("dd_createviewh\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLES_ID, (long*)&viewid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &viewname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_TYPE, &viewtype);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &schema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_TABLES_CREATIME, &date);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        if (sysview) {
            viewid = rs_viewh_viewid(cd, viewh);
        } else {
            viewid = dbe_db_getnewrelid_log(db);
            rs_viewh_setviewid(cd, viewh, viewid);
        }
        viewname = rs_viewh_name(cd, viewh);
        viewtype = (char *)RS_AVAL_VIEW;
        schema = rs_viewh_schema(cd, viewh);
        ss_dassert(schema != NULL);
        catalog = rs_viewh_catalog(cd, viewh);

        date = tb_dd_curdate();

        trc = TliCursorInsert(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_VIEWS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_VIEWS_V_ID, (long*)&viewid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_VIEWS_TEXT, &viewtext);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        viewid = rs_viewh_viewid(cd, viewh);
        viewtext = rs_viewh_def(cd, viewh);

        trc = TliCursorInsert(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        ttype = rs_viewh_ttype(cd, viewh);
        if (ttype != NULL) {
            dd_createttype(
                tcon,
                viewid,
                ttype,
                sysview ? TB_DD_CREATEREL_SYSTEM : TB_DD_CREATEREL_USER);
        }
}

/*#***********************************************************************\
 *
 *              dd_dropviewh
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      viewh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_dropviewh(
        TliConnectT* tcon,
        rs_viewh_t* viewh,
        su_err_t** p_errh)
{
        TliCursorT* tcur;
        TliRetT trc;
        long viewid;
        bool succp = TRUE;
        rs_sysinfo_t* cd = TliGetCd(tcon);

        ss_dprintf_3(("dd_dropviewh\n"));
        ss_dassert(p_errh != NULL);

        viewid = rs_viewh_viewid(cd, viewh);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLES_ID, &viewid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLES_ID,
                TLI_RELOP_EQUAL,
                viewid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END, TliCursorErrorCode(tcur));

        if (trc == TLI_RC_END) {
            TliCursorFree(tcur);
            su_err_init(p_errh, E_DDOP);
            return(FALSE);
        }

        trc = TliCursorDelete(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));

        TliCursorFree(tcur);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_VIEWS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_VIEWS_V_ID, &viewid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_VIEWS_V_ID,
                TLI_RELOP_EQUAL,
                viewid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        if (trc != TLI_RC_SUCC) {
            TliCursorCopySuErr(tcur, p_errh);
            succp = FALSE;
        }

        if (succp) {
            trc = TliCursorDelete(tcur);
            if (trc != TLI_RC_SUCC) {
                TliCursorCopySuErr(tcur, p_errh);
                succp = FALSE;
            }
        }

        TliCursorFree(tcur);

        if (succp) {
            dd_dropttype(tcon, viewid); /* any memory-leaks */
        }
        return(succp);
}

#endif /* SS_NODDUPDATE */

#endif /* SS_NOVIEW */

/*#***********************************************************************\
 *
 *              dd_relpresent
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      relname -
 *
 *
 *      p_relid -
 *
 *
 *      p_trxrelh -
 *
 *
 *      p_errh -
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
static rs_relh_t* dd_relpresent(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        rs_entname_t* relname,
        ulong* p_relid,
        bool* p_trxrelh,
        rs_err_t** p_errh)
{
        rs_rbuf_present_t rp;
        rs_rbuf_ret_t rbufret;
        TliConnectT* tcon;
        TliRetT trc;
        dbe_trx_t* trx;
        rs_relh_t* relh;
        dbe_ret_t       rc;
        int nloop = 0;
        ss_debug(bool systemschema;)

        ss_debug(systemschema = rs_entname_getschema(relname) != NULL && strcmp(rs_entname_getschema(relname), RS_AVAL_SYSNAME) == 0;)

        *p_trxrelh = FALSE;

        do {
            SS_PUSHNAME("dd_relpresent:rs_rbuf_relpresent");
            rp = rs_rbuf_relpresent(cd, rbuf, relname, &relh, p_relid);
            SS_POPNAME;

            switch (rp) {

                case RSRBUF_BUFFERED:
                    /* The relation is already buffered, return the entry
                     * from the buffer.
                     */
                    ss_debug(if (!systemschema)) ss_dprintf_2(("dd_relpresent RSRBUF_BUFFERED\n"));
                    break;

                case RSRBUF_EXISTS:
                    /* Relation exists but is not buffered. Read the relation
                     * information and add it to the buffer.
                     */
                    ss_debug(if (!systemschema)) ss_dprintf_2(("dd_relpresent RSRBUF_EXISTS\n"));
#ifdef DBE_NO_OLD_RELH_FIX
                    tcon = TliConnectInit(cd);
#else
                    tcon = TliConnectInitByReadlevel(cd, trans);
#endif
                    SS_PUSHNAME("dd_relpresent:dd_readrelh");
                    relh = dd_readrelh(tcon, *p_relid, DD_READRELH_FULL, TRUE, trans, &rc);
                    SS_POPNAME;

                    if (relh == NULL) {
                        ss_dprintf_2(("dd_relpresent Failed to read relh from system tables\n"));
                        if (rc == E_RELNOTEXIST_S) {
                            rs_error_create(p_errh, E_RELNOTEXIST_S, rs_entname_getname(relname));
                        } else {
                            rs_error_create(p_errh, rc);
                        }

                        trc = TliCommit(tcon);
                        ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                        TliConnectDone(tcon);

                        return(NULL);
                    }

                    SS_PUSHNAME("dd_relpresent:rs_rbuf_insertrelh_ex");
                    rbufret = rs_rbuf_insertrelh_ex(cd, rbuf, relh);
                    SS_POPNAME;
                    switch (rbufret) {
                        case RSRBUF_SUCCESS:
                            break;
                        case RSRBUF_ERR_EXISTS:
                            /* Insert failed, maybe some other thread added
                             * the info to the buffer.
                             */
                            SS_MEM_SETUNLINK(relh);
                            rs_relh_done(cd, relh);
                            relh = NULL;
                            break;
                        case RSRBUF_ERR_INVALID_ARG:
                            /* Insert failed, relation may be dropped.
                             */
                            SS_MEM_SETUNLINK(relh);
                            rs_relh_done(cd, relh);
                            relh = NULL;
                            rp = RSRBUF_NOTEXIST;
                            break;
                        default:
                            ss_rc_error(rbufret);
                    }

                    dbe_trx_unlockrelid(tb_trans_dbtrx(cd, trans), *p_relid);

                    trc = TliCommit(tcon);
                    ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                    TliConnectDone(tcon);
                    break;

                case RSRBUF_NOTEXIST:
                    /* Relation does not exist. */
                    ss_dprintf_2(("dd_relpresent RSRBUF_NOTEXIST\n"));
                    relh = NULL;
                    break;

                case RSRBUF_AMBIGUOUS:
                    ss_dprintf_2(("dd_relpresent RSRBUF_AMBIGUOUS\n"));
                    rs_error_create(p_errh, E_AMBIGUOUS_S, rs_entname_getname(relname));
                    return(NULL);

                default:
                    ss_error;
            }

            if (rp == RSRBUF_EXISTS && nloop > 2) {
                SsThrSleep(100);
            }
            nloop++;

        } while (rp == RSRBUF_EXISTS && nloop < 10);

#ifndef SS_MYSQL
        ss_bassert(nloop++ < 10);
#endif

        trx = tb_trans_dbtrx(cd, trans);

        SS_PUSHNAME("dd_relpresent:trx");
        if (trx != NULL) {
            if (relh != NULL) {
                /* Found */
                if (dbe_trx_reldeleted(trx, rs_relh_entname(cd, relh))) {
                    SS_MEM_SETUNLINK(relh);
                    rs_relh_done(cd, relh);
                    relh = NULL;
                }
            } else {
                /* Not found. */
                if (dbe_trx_relinserted(trx, relname, &relh)) {
                    rs_relh_link(cd, relh);
                    SS_MEM_SETLINK(relh);
                    *p_trxrelh = TRUE;
                } else {
                    relh = NULL;
                }
            }
        }
        SS_POPNAME;
        if (relh == NULL) {
            rs_error_create(p_errh, E_RELNOTEXIST_S, rs_entname_getname(relname));
        }
        return(relh);
}

/*#***********************************************************************\
 *
 *              dd_relpresentbyid
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      relid -
 *
 *
 *      p_trxrelh -
 *
 *
 *      p_errh -
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
static rs_relh_t* dd_relpresentbyid(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        ulong relid,
        bool* p_trxrelh,
        rs_err_t** p_errh)
{
        rs_rbuf_present_t rp;
        rs_rbuf_ret_t rbufret;
        TliConnectT* tcon;
        TliRetT trc;
        dbe_trx_t* trx;
        rs_relh_t* relh;
        dbe_ret_t  rc;
        ulong relid2;
        rs_entname_t* relname;
        int nloop = 0;

        SS_PUSHNAME("dd_relpresentbyid");

        *p_trxrelh = FALSE;
        relname = NULL;

        if (relid == 0) {
            rs_error_create(p_errh, E_RELNOTEXIST_S, "");
            SS_POPNAME;
            return(NULL);
        }

        do {
            if (relname != NULL) {
                rs_entname_done(relname);
                relname = NULL;
            }

            if (rs_rbuf_relnamebyid(cd, rbuf, relid, &relname)) {
                rp = rs_rbuf_relpresent(cd, rbuf, relname, &relh, &relid2);
            } else {
                rp = RSRBUF_EXISTS;
                relid2 = relid;
            }

            switch (rp) {

                case RSRBUF_BUFFERED:
                    /* The relation is already buffered, return the entry
                     * from the buffer.
                     */
                    ss_dprintf_2(("dd_relpresent RSRBUF_BUFFERED\n"));
                    break;

                case RSRBUF_EXISTS:
                    /* Relation exists but is not buffered. Read the relation
                     * information and add it to the buffer.
                     */
                    ss_dprintf_2(("dd_relpresent RSRBUF_EXISTS\n"));
#ifdef DBE_NO_OLD_RELH_FIX
                    tcon = TliConnectInit(cd);
#else
                    tcon = TliConnectInitByReadlevel(cd, trans);
#endif
                    relh = dd_readrelh(tcon, relid2, DD_READRELH_FULL, TRUE, trans, &rc);

                    if (relh == NULL) {
                        ss_dprintf_2(("dd_relpresent Failed to read relh from system tables\n"));
                        if (rc == E_RELNOTEXIST_S) {
                            rs_error_create(p_errh, E_RELNOTEXIST_S,
                                            relname ? rs_entname_getname(relname): "");
                        } else {
                            rs_error_create(p_errh, rc);
                        }

                        trc = TliCommit(tcon);
                        ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                        TliConnectDone(tcon);

                        if (relname != NULL) {
                            rs_entname_done(relname);
                        }
                        SS_POPNAME;

                        return(NULL);
                    }
                    ss_dassert(relid2 == relid);

                    rbufret = rs_rbuf_insertrelh_ex(cd, rbuf, relh);
                    switch (rbufret) {
                        case RSRBUF_SUCCESS:
                            break;
                        case RSRBUF_ERR_EXISTS:
                            /* Insert failed, maybe some other thread added
                             * the info to the buffer.
                             */
                            SS_MEM_SETUNLINK(relh);
                            rs_relh_done(cd, relh);
                            relh = NULL;
                            break;
                        case RSRBUF_ERR_INVALID_ARG:
                            /* Insert failed, relation may be dropped.
                             */
                            SS_MEM_SETUNLINK(relh);
                            rs_relh_done(cd, relh);
                            relh = NULL;
                            rp = RSRBUF_NOTEXIST;
                            break;
                        default:
                            ss_rc_error(rbufret);
                    }

                    dbe_trx_unlockrelid(tb_trans_dbtrx(cd, trans), relid);

                    trc = TliCommit(tcon);
                    ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                    TliConnectDone(tcon);
                    break;

                case RSRBUF_NOTEXIST:
                    /* Relation does not exist. */
                    ss_dprintf_2(("dd_relpresent RSRBUF_NOTEXIST\n"));
                    relh = NULL;
                    break;

                case RSRBUF_AMBIGUOUS:
                    ss_dprintf_2(("dd_relpresent RSRBUF_AMBIGUOUS\n"));
                    rs_error_create(p_errh, E_AMBIGUOUS_S, rs_entname_getname(relname));
                    rs_entname_done(relname);
                    SS_POPNAME;
                    return(NULL);

                default:
                    ss_error;
            }

            if (rp == RSRBUF_EXISTS && nloop > 2) {
                SsThrSleep(100);
            }
            nloop++;

        } while (rp == RSRBUF_EXISTS && nloop < 10);

#ifndef SS_MYSQL
        ss_bassert(nloop++ < 10);
#endif

        trx = tb_trans_dbtrx(cd, trans);

        if (trx != NULL) {
            if (relh != NULL) {
                /* Found */
                if (dbe_trx_reldeleted(trx, rs_relh_entname(cd, relh))) {
                    SS_MEM_SETUNLINK(relh);
                    rs_relh_done(cd, relh);
                    relh = NULL;
                }
            } else {
                /* Not found. */
                if (relname != NULL && dbe_trx_relinserted(trx, relname, &relh)) {
                    rs_relh_link(cd, relh);
                    SS_MEM_SETLINK(relh);
                    *p_trxrelh = TRUE;
                } else {
                    relh = NULL;
                }
            }
        }

        if (relh == NULL) {
            rs_error_create(p_errh, E_RELNOTEXIST_S, relname != NULL ? rs_entname_getname(relname) : "-unknown-");
        }

        if (relname != NULL) {
            rs_entname_done(relname);
        }
        SS_POPNAME;
        
        return(relh);
}

/*#***********************************************************************\
 *
 *              dd_getrelh
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      rbuf -
 *
 *
 *      relname -
 *
 *
 *      p_priv - out, ref
 *
 *
 *      p_errh - out, give
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_relh_t* dd_getrelh(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        rs_entname_t* relname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_relh_t* relh;
        ulong relid;
        bool trxrelh;
        ss_debug(bool systemschema;)

        ss_dassert(cd != NULL);
        ss_dassert(relname != NULL);

        ss_debug(systemschema = rs_entname_getschema(relname) != NULL && strcmp(rs_entname_getschema(relname), RS_AVAL_SYSNAME) == 0;)
        ss_debug(if (!systemschema)) ss_dprintf_1(("dd_getrelh '%s.%s.%s'\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));
        SS_PUSHNAME("dd_getrelh");

        relh = dd_relpresent(cd, trans, rbuf, relname, &relid, &trxrelh, p_errh);
        if (relh == NULL) {
            SS_POPNAME;
            return(NULL);
        }

        if (!trxrelh && p_priv != NULL) {
            tb_priv_getrelpriv(
                cd,
                relid,
                rs_relh_issysrel(cd, relh) || !rs_relh_isbasetable(cd, relh),
                TRUE,
                p_priv);
        }

        SS_POPNAME;

        return(relh);
}

/*#***********************************************************************\
 *
 *              dd_getrelhbyid
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      rbuf -
 *
 *
 *      relid -
 *
 *
 *      p_priv - out, ref
 *
 *
 *      p_errh - out, give
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_relh_t* dd_getrelhbyid(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        ulong relid,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_relh_t* relh;
        bool trxrelh;

        SS_PUSHNAME("dd_getrelhbyid");

        relh = dd_relpresentbyid(cd, trans, rbuf, relid, &trxrelh, p_errh);

        if (relh != NULL && !trxrelh && p_priv != NULL) {
            tb_priv_getrelpriv(
                cd,
                relid,
                rs_relh_issysrel(cd, relh) || !rs_relh_isbasetable(cd, relh),
                TRUE,
                p_priv);
        }
        SS_POPNAME;
        return(relh);
}

/*##**********************************************************************\
 *
 *              tb_dd_getrelh
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relname -
 *
 *
 *      p_priv - out, ref
 *
 *
 *      p_errh - out, give
 *
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_relh_t* tb_dd_getrelh(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_auth_t* auth;
        rs_rbuf_t* rbuf;
        rs_relh_t* relh;
        rs_entname_t en;
        char* schema;
        char* catalog;

        if (p_priv != NULL) {
            *p_priv = NULL;
        }

        rbuf = rs_sysi_rbuf(cd);
        schema = rs_entname_getschema(relname);
        if (schema == NULL) {
            /* No schema given . Check first catalog and if catalog not given try with username.
             */
            rs_rbuf_present_t rp;

            catalog = rs_entname_getcatalog(relname);
            auth = rs_sysi_auth(cd);
            if (catalog == NULL) {
                catalog = rs_auth_catalog(cd, auth);
            }
            rs_entname_initbuf(
                &en,
                catalog,
                rs_auth_schema(cd, auth),
                rs_entname_getname(relname));

            relh = dd_getrelh(cd, trans, rbuf, &en, p_priv, NULL);
            if (relh != NULL) {
                /* Found a relh. */
                return(relh);
            }

            /* Try to find a view from own schema.
             */
            rp = rs_rbuf_viewpresent(cd, rbuf, &en, NULL, NULL);
            switch (rp) {
                case RSRBUF_BUFFERED:
                case RSRBUF_EXISTS:
                    /* Found a view from own schema, do not try to find a
                     * relh any more
                     */
                    rs_error_create(p_errh, E_RELNOTEXIST_S, rs_entname_getname(relname));
                    return(NULL);
                case RSRBUF_NOTEXIST:
                case RSRBUF_AMBIGUOUS:
                    break;
                default:
                    ss_error;
            }
        } else {
            if (strcmp(schema, RS_AVAL_SYSNAME) == 0) {
                rs_entname_initbuf(&en,
                                   RS_AVAL_DEFCATALOG,
                                   schema,
                                   rs_entname_getname(relname));
                relname = &en;
            } else if (rs_entname_getcatalog(relname) == NULL) {
                auth = rs_sysi_auth(cd);
                ss_dassert(auth != NULL);
                rs_entname_initbuf(&en,
                                   rs_auth_catalog(cd, auth),
                                   schema,
                                   rs_entname_getname(relname));
                relname = &en;
            }
        }
        relh = dd_getrelh(cd, trans, rbuf, relname, p_priv, p_errh);
        return(relh);
}

/*##**********************************************************************\
 *
 *              tb_dd_getrelhbyid
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relid -
 *
 *
 *      p_priv - out, ref
 *
 *
 *      p_errh - out, give
 *
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_relh_t* tb_dd_getrelhbyid(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        ulong relid,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_rbuf_t* rbuf;
        rs_relh_t* relh;

        rbuf = rs_sysi_rbuf(cd);

        relh = dd_getrelhbyid(cd, trans, rbuf, relid, p_priv, p_errh);
        return relh;
}

/*#***********************************************************************\
 *
 *              tb_dd_getrelhbyid_ex
 *
 * Find solidDB relation by relation id
 *
 * Parameters :
 *
 *      rs_sysi_t*    cd, in, use, system information
 *      tb_trans_t*   trans, in, use, transaction
 *      long          relid, in, use, relation id
 *      su_err_t*     p_errh, in, use, error structure
 *
 * Return value : rs_relh_t* relation if found or NULL if not found
 *
 * Globals used : -
 */
rs_relh_t* tb_dd_getrelhbyid_ex(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long relid,
        su_err_t** p_errh)
{
        rs_relh_t* relh;
        dbe_ret_t rc;

        SS_PUSHNAME("solid_getrelhbyid");

        if (tb_trans_dbtrx(cd, trans) == NULL) {
            rc = tb_trans_getdberet(cd, trans);

            if (rc == SU_SUCCESS) {
                rc = E_DDOP;
            }

            if (p_errh) {
                rs_error_create(p_errh, rc);
            }

            SS_POPNAME;
            return(NULL);
        } else {
            /* Check if relid is not valid. */
            rc = dbe_trx_readrelh(tb_trans_dbtrx(cd, trans), relid);
        }

        if (rc != SU_SUCCESS) {
            if (p_errh) {
                rs_error_create(p_errh, E_DDOP);
            }

            SS_POPNAME;
            return(NULL);
        }

        /* Read the relh. */
        relh = tb_dd_getrelhbyid(cd, trans, relid, NULL, p_errh);
        SS_POPNAME;

        return(relh);
}

#ifndef SS_NOVIEW

/*#************************************************************************\
 *
 *              dd_getrelhfromview
 *
 * Gets relation handle for a case where the relation is accessed through
 * a view. This case needs special handling for the privileges, because
 * the relation privileges should be a combination of the view privileges
 * of the user and relation privileges of the creator of the view.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      relname -
 *
 *
 *      viewname -
 *
 *
 *      p_priv -
 *
 *
 *      p_errh -
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
static rs_relh_t* dd_getrelhfromview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        rs_entname_t* relname,
        rs_viewh_t* viewh,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_relh_t* relh;
        ulong relid;
        bool trxrelh;
        bool succp;

        ss_dassert(cd != NULL);
        ss_dassert(relname != NULL);

        ss_dprintf_1(("dd_getrelhfromview '%s.%s', view = '%s.%s'\n",
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname),
            rs_viewh_schema(cd, viewh),
            rs_viewh_name(cd, viewh)));
        SS_PUSHNAME("dd_getrelhfromview");

        relh = dd_relpresent(cd, trans, rbuf, relname, &relid, &trxrelh, p_errh);
        if (relh == NULL) {
            SS_POPNAME;
            return(NULL);
        }

        if (!trxrelh && p_priv != NULL) {
            succp = tb_priv_getrelprivfromview(
                        cd,
                        trans,
                        rs_viewh_entname(cd, viewh),
                        relid,
                        rs_relh_issysrel(cd, relh)
                        || !rs_relh_isbasetable(cd, relh),
                        TRUE,
                        rs_viewh_viewid(cd, viewh),
                        rs_viewh_schema(cd, viewh),
                        *p_priv,
                        p_errh);
            if (!succp) {
                SS_MEM_SETUNLINK(relh);
                rs_relh_done(cd, relh);
                relh = NULL;
            }
        }

        SS_POPNAME;

        return(relh);
}

/*##**********************************************************************\
 *
 *              tb_dd_getrelhfromview
 *
 * Gets relation handle for a case where the relation is accessed through
 * a view. This case needs special handling for the privileges, because
 * the relation privileges should be a combination of the view privileges
 * of the user and relation privileges of the creator of the view.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relname -
 *
 *
 *      viewname -
 *
 *
 *      p_priv -
 *
 *
 *      p_errh -
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
rs_relh_t* tb_dd_getrelhfromview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname,
        rs_entname_t* viewname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_relh_t* relh;
        rs_viewh_t* viewh;
        rs_rbuf_t* rbuf;
        rs_entname_t en;

        if (p_priv != NULL) {
            *p_priv = NULL;
        }
        if (p_errh != NULL) {
            *p_errh = NULL;
        }

        viewh = tb_dd_getviewh(cd, trans, viewname, p_priv, p_errh);
        if (viewh == NULL) {
            /* View does not exist. */
            return(NULL);
        }

        rbuf = rs_sysi_rbuf(cd);

        if (rs_entname_getschema(relname) == NULL) {
            /* No schema given. First try with view schema.
             */
            rs_entname_initbuf(
                &en,
                rs_viewh_catalog(cd, viewh),
                rs_viewh_schema(cd, viewh),
                rs_entname_getname(relname));
            relh = dd_getrelhfromview(
                        cd,
                        trans,
                        rbuf,
                        &en,
                        viewh,
                        p_priv,
                        p_errh);
            if (relh != NULL) {
                /* Found. */
                rs_viewh_done(cd, viewh);
                return(relh);
            }
            if (p_errh != NULL) {
                rs_error_free(cd, *p_errh);
            }
        } else if (rs_entname_getcatalog(relname) == NULL) {
            rs_entname_initbuf(&en,
                               rs_viewh_catalog(cd, viewh),
                               rs_entname_getschema(relname),
                               rs_entname_getname(relname));
            relname = &en;
        }
        relh = dd_getrelhfromview(
                    cd,
                    trans,
                    rbuf,
                    relname,
                    viewh,
                    p_priv,
                    p_errh);

        rs_viewh_done(cd, viewh);

        return(relh);
}

#endif /* SS_NOVIEW */

#ifndef SS_NODDUPDATE

/*##**********************************************************************\
 *
 *              tb_dd_createrel
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      auth -
 *
 *
 *      type -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
su_ret_t tb_dd_createrel(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_auth_t* auth,
        tb_dd_createrel_t type,
        long relationid,
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        su_ret_t rc = SU_SUCCESS;
        dbe_trx_t* trx;

        SS_NOTUSED(auth);
        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relh != NULL);

        ss_dprintf_1(("tb_dd_createrel '%s', type=%d\n",
            rs_relh_name(cd, relh), type));

        trx = tb_trans_dbtrx(cd, trans);

        if (type != TB_DD_CREATEREL_SYSTEM) {
            rs_relh_t* tmprelh;
            rs_entname_t* tmprelname;

            tmprelname = rs_relh_entname(cd, relh);
            tmprelh = tb_dd_getrelh(
                        cd,
                        trans,
                        tmprelname,
                        NULL,
                        NULL);

            if (tmprelh != NULL && !dbe_trx_reldeleted(trx, tmprelname)) {
                rs_error_create(p_errh, E_RELEXIST_S, rs_entname_getname(tmprelname));
                SS_MEM_SETUNLINK(tmprelh);
                rs_relh_done(cd, tmprelh);
                return(E_RELEXIST_S);
            }
        }

        tcon = TliConnectInitByTrans(cd, trans);

        dd_createrelh(tcon, relh, type, NULL, relationid);

        if (type != TB_DD_CREATEREL_SYSTEM) {
            /* Add the new relation to the relation buffer. */
            rc = dbe_trx_insertrel(trx, relh);
        }

        if (rc != SU_SUCCESS) {
            rs_error_create(p_errh, rc);
        }

        TliConnectDone(tcon);

        return(rc);
}


/*##**********************************************************************\
 *
 *              tb_dd_droprel
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relname -
 *
 *
 *      p_relid -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_droprel(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        bool usercall,
        rs_entname_t* relname,
        bool issyncrel,
        long* p_relid,
        rs_entname_t** p_relname,
        bool* p_issyncrel,
        bool checkforkeys,
        bool cascade,
        rs_err_t** p_errh)
{
        rs_relh_t* relh;
        tb_relpriv_t* priv;
        rs_auth_t* auth;
        bool       succp = TRUE;
        dbe_ret_t  rc;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relname != NULL);

        if (!tb_dd_checkobjectname(rs_entname_getname(relname))) {
            rs_error_create(p_errh, E_RELNOTEXIST_S,"");
            return(FALSE);
        }

        ss_dprintf_1(("tb_dd_droprel '%s.%s'\n",
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));

        relh = tb_dd_getrelh(cd, trans, relname, &priv, p_errh);
        if (relh == NULL) {
            return(FALSE);
        }


        if (p_issyncrel != NULL) {
            ss_dassert(!issyncrel);
            *p_issyncrel = rs_relh_issync(cd, relh);
        }

        auth = rs_sysi_auth(cd);
        relname = rs_relh_entname(cd, relh);

#ifndef SS_MYSQL
        tb_sync_hist_cleanup_abort(cd, trans, relname);
#endif /* !SS_MYSQL */

        if (usercall && !rs_relh_isbasetable(cd, relh)) {
            succp = FALSE;
            rs_error_create(p_errh, E_NOTBASETABLE);
        } else if (!issyncrel && !tb_priv_iscreatorrelpriv(cd, priv)) {
            succp = FALSE;
            rs_error_create(p_errh, E_RELNOTEXIST_S, rs_entname_getname(relname));

        } else {
            bool isforkeyref = FALSE;
            TliConnectT* tcon;
            dbe_trx_t* trx;

            trx = tb_trans_dbtrx(cd, trans);

            rc = dbe_trx_lockrelh(trx, relh, TRUE, 0);
            if (rc != DBE_RC_SUCC) {
                rs_error_create(p_errh, rc);
                SS_MEM_SETUNLINK(relh);
                rs_relh_done(cd, relh);
                return FALSE;
            }
            
            tcon = TliConnectInitByTrans(cd, trans);

            *p_relid = rs_relh_relid(cd, relh);

            if (checkforkeys) {
                succp = dd_isforkeyref(tcon, relh, &isforkeyref, p_errh);
            }

            if (succp) {
                /* JVu: Aug-5-2003
                 * In drop (catalog|schema) cascade situation, the foreign
                 * keys have already being 'dropped' but the changes haven't
                 * been committed yet. Thus we can't produce errors from foreign
                 * keys in this state.
                 */
                if (isforkeyref && !cascade) {
                    rs_error_create(p_errh, E_FORKEYREFEXIST_S, rs_relh_name(cd, relh));
                    succp = FALSE;
                } else {
                    succp = dd_droprefkeys(tcon, relh, p_errh);
                    if (succp) {
                        su_ret_t rc;
                        rc = dbe_trx_deleterel(trx, relh);
                        if (rc != DBE_RC_SUCC) {
                            rs_error_create(p_errh, rc);
                            succp = FALSE;
                        }
                        if (succp) {
                            succp = dd_droprelh(tcon, relh, p_errh);
                        }
                    }
                }
            }
            TliConnectDone(tcon);
        }

        if (succp && p_relname != NULL) {
            *p_relname = rs_entname_copy(rs_relh_entname(cd, relh));
        }
        SS_MEM_SETUNLINK(relh);
        rs_relh_done(cd, relh);

        return(succp);
}

#ifndef SS_MYSQL
/*#***********************************************************************\
 *
 *              dd_getrelidbykeyname
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      keyname -
 *
 *
 *      p_relid -
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
static dd_keyfound_t dd_getrelidbykeyname(
        TliConnectT*    tcon,
        rs_entname_t*   keyname,
        long*           p_relid,
        rs_err_t**      p_errh)
{
        long relid;
        dd_keyfound_t kf;
        bool succp;
        tb_sql_ret_t rc;
        void* cd = TliGetCd(tcon);
        tb_trans_t* trans = TliGetTrans(tcon);
        sqlsystem_t* sqls = tb_sqls_init(cd);
        char* sqlstr;
        char* syscatalog = RS_AVAL_DEFCATALOG;
        static char sqlstr_fmt[] =
"SELECT T.ID\n\
FROM \"%s\"._SYSTEM.SYS_KEYS AS K, \"%s\"._SYSTEM.SYS_TABLES AS T\n\
WHERE T.ID = K.REL_ID AND\n\
 K.KEY_NAME = ?%s%s";
        tb_sql_t* sc;
        char* catalog_cond = " AND\n T.TABLE_CATALOG = ?";
        static char schema_cond_nonnull[] = " AND\n T.TABLE_SCHEMA = ?";
        char* schema_cond = "";
        char* schema = rs_entname_getschema(keyname);
        char* catalog = rs_entname_getcatalog(keyname);
        char* indexname = rs_entname_getname(keyname);
        uint parambindcount = 0;

        catalog = tb_catalog_resolve(cd, catalog);

        if (schema != NULL) {
            schema_cond = schema_cond_nonnull;
        }
        if (catalog == NULL) {
            /* Can be NULL during db conversion. */
            catalog_cond = "";
            syscatalog = "";
        }
        sqlstr = SsMemAlloc(sizeof(sqlstr_fmt) +
                            2 * strlen(syscatalog) +
                            strlen(indexname) +
                            strlen(catalog_cond) +
                            strlen(schema_cond));
        SsSprintf(sqlstr,
                  sqlstr_fmt,
                  syscatalog,
                  syscatalog,
                  catalog_cond,
                  schema_cond);
        sc = tb_sql_init(cd, sqls, trans, sqlstr);
        ss_dassert(sc != NULL);
        succp = tb_sql_prepare(sc, NULL);
        ss_dassert(succp);
        succp = tb_sql_setparamstr(sc, parambindcount, indexname, NULL);
        ss_dassert(succp);
        parambindcount++;
        if (catalog != NULL) {
            succp = tb_sql_setparamstr(sc, parambindcount, catalog, NULL);
            ss_dassert(succp);
            parambindcount++;
        }
        if (schema != NULL) {
            succp = tb_sql_setparamstr(sc, parambindcount, schema, NULL);
            ss_dassert(succp);
            parambindcount++;
        }
        ss_dassert(tb_sql_getparcount(sc) == parambindcount);
        succp = tb_sql_execute(sc, NULL);
        ss_dassert(succp);
        rc = tb_sql_fetchwithretcode(sc, TRUE, NULL, NULL);
        if (rc == TB_SQL_END) {
            goto found_none;
        }

        if (rc != TB_SQL_SUCC) {
            kf = DD_KEYFOUND_ERROR;
            goto cleanup_exit;
        }

        ss_rc_dassert(rc == TB_SQL_SUCC, rc);
        succp = tb_sql_getcollong(sc, 0, &relid, NULL);
        ss_dassert(succp);
        if (p_relid != NULL) {
            *p_relid = relid;
        }
        rc = tb_sql_fetchwithretcode(sc, TRUE, NULL, NULL);
        if (rc == TB_SQL_SUCC) {
            goto found_many;
        }
        if (rc != TB_SQL_SUCC && rc != TB_SQL_END) {
            kf = DD_KEYFOUND_ERROR;
            goto cleanup_exit;
        }

        ss_rc_dassert(rc == TB_SQL_END, rc);
        kf = DD_KEYFOUND_ONE;
        goto cleanup_exit;
 found_none:;
        kf = DD_KEYFOUND_ZERO;
        goto cleanup_exit;
 found_many:;
        kf = DD_KEYFOUND_MANY;
 cleanup_exit:;

        if (kf == DD_KEYFOUND_ERROR) {
            tb_trans_geterrcode(cd, trans, p_errh);
        }

        SsMemFree(sqlstr);
        tb_sql_done(sc);
        tb_sqls_done(cd, sqls);
        return (kf);
}
#endif /* !SS_MYSQL */


/*##**********************************************************************\
 *
 *              tb_dd_createindex
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      ttype -
 *
 *
 *      key -
 *
 *
 *      auth -
 *
 *
 *      keyname -
 *
 *
 *      type -
 *
 *
 *      p_errh -
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
su_ret_t tb_dd_createindex(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_ttype_t* ttype,
        rs_key_t* key,
        rs_auth_t* auth,
        rs_entname_t* keyname __attribute__ ((unused)),
        tb_dd_createrel_t type,
        rs_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        TliConnectT* tcon;

        SS_NOTUSED(auth);
        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(ttype != NULL);
        ss_dassert(key != NULL);

        ss_dprintf_1(("tb_dd_createindex '%s'\n", rs_relh_name(cd, relh)));

        if (type == TB_DD_CREATEREL_USER) {
            tb_relpriv_t* priv;

            tb_priv_getrelpriv(
                cd,
                rs_relh_relid(cd, relh),
                rs_relh_issysrel(cd, relh)
                || !rs_relh_isbasetable(cd, relh),
                TRUE,
                &priv);

            if (!tb_priv_iscreatorrelpriv(cd, priv)) {
                rs_key_done(cd, key);
                rs_error_create(p_errh, E_RELNOTEXIST_S, rs_relh_name(cd, relh));
                return(E_KEYNOTEXIST_S);
            }
        }

        tcon = TliConnectInitByTrans(cd, trans);

#ifndef SS_MYSQL
        if (keyname != NULL) {
            long relid;
            switch (dd_getrelidbykeyname(tcon, keyname, &relid, p_errh)) {
                case DD_KEYFOUND_ZERO:
                    break;
                case DD_KEYFOUND_ONE:
                case DD_KEYFOUND_MANY:
                    rc = E_KEYNAMEEXIST_S;
                    rs_error_create(p_errh, E_KEYNAMEEXIST_S, rs_key_name(cd, key));
                    break;
                case DD_KEYFOUND_ERROR:
                    rc = rs_error_geterrcode(cd, *p_errh);
                    break;
                default:
                    ss_error;
            }
        }
#endif /* !SS_MYSQL */

        if (rc == SU_SUCCESS) {
            td_dd_createonekey(tcon, relh, ttype, key, auth, type);

            rc = dbe_trx_insertindex(tb_trans_dbtrx(cd, trans), relh, key);

            switch (rc) {
                case SU_SUCCESS:
                    break;
                case E_KEYNAMEEXIST_S:
                    rs_error_create(p_errh, rc, rs_key_name(cd, key));
                    break;
                default:
                    rs_error_create(p_errh, rc);
                    break;
            }
        }

        TliConnectDone(tcon);
        rs_key_done(cd, key);

        return(rc);
}

#ifndef SS_MYSQL
/*#***********************************************************************\
 *
 *              dd_getrelnamebyrelid
 *
 *
 *
 * Parameters :
 *
 *      tcon - in, use
 *
 *
 *      relid - in, use
 *
 *
 *      p_relname - out, give
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_getrelnamebyrelid(
        TliConnectT*    tcon,
        long            relid,
        rs_entname_t**  p_relname)
{
        TliCursorT* tcur;
        TliRetT trc;
        char* relname;
        char* relauthid;
        char* catalog;

        ss_dprintf_3(("dd_getrelnamebyrelid\n"));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &relname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &relauthid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CATALOG, &catalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLES_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        ss_dassert(trc == TLI_RC_END || trc == TLI_RC_SUCC);

        if (trc == TLI_RC_SUCC) {
            *p_relname = rs_entname_init(
                               catalog,
                               relauthid,
                               relname);
        }

        TliCursorFree(tcur);

        return(trc == TLI_RC_SUCC);
}
#endif /* !SS_MYSQL */

/*##**********************************************************************\
 *
 *              tb_dd_dropindex
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      keyname -
 *
 *
 *      p_issynctable -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
su_ret_t tb_dd_dropindex(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* keyname,
        bool issynctable,
        bool* p_issynctable,
        rs_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        rs_relh_t* relh = NULL;
        bool trxfoundp;
        rs_key_t* key;
        dbe_trx_t* trx;
        TliConnectT* tcon;
#ifndef SS_MYSQL
        rs_entname_t* relname = NULL;
        rs_auth_t* auth;
#endif
        bool key_linked = FALSE;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(keyname != NULL);

        ss_dprintf_1(("tb_dd_dropindex '%s.%s'\n",
            rs_entname_getprintschema(keyname),
            rs_entname_getname(keyname)));

        trx = tb_trans_dbtrx(cd, trans);

        tcon = TliConnectInitByTrans(cd, trans);

        trxfoundp = dbe_trx_indexinserted(trx, keyname, &relh, &key);

        if (!trxfoundp) {
            tb_relpriv_t* priv __attribute__ ((unused));
            /* Not created in this transaction. Search from
             * the database.
             */
#ifndef SS_MYSQL
            long relid;
            switch (dd_getrelidbykeyname(tcon, keyname, &relid, p_errh)) {
                case DD_KEYFOUND_ZERO:
                    rc = E_KEYNOTEXIST_S;
                    rs_error_create(p_errh, E_KEYNOTEXIST_S, rs_entname_getname(keyname));
                    break;
                case DD_KEYFOUND_ONE:
                    if (dd_getrelnamebyrelid(tcon, relid, &relname)) {
                        relh = tb_dd_getrelh(cd, trans, relname, &priv, NULL);
                        auth = rs_sysi_auth(cd);
                        rs_entname_done(relname);
                        if (relh == NULL ||
                            (!tb_priv_iscreatorrelpriv(cd, priv) && !issynctable)) {
                            rc = E_KEYNOTEXIST_S;
                            rs_error_create(p_errh, E_KEYNOTEXIST_S, rs_entname_getname(keyname));
                        }
                    } else {
                        rc = E_KEYNOTEXIST_S;
                        rs_error_create(p_errh, E_KEYNOTEXIST_S, rs_entname_getname(keyname));
                    }
                    break;
                case DD_KEYFOUND_MANY:
                    rc = E_AMBIGUOUS_S;
                    rs_error_create(p_errh, E_AMBIGUOUS_S, rs_entname_getname(keyname));
                    break;
                case DD_KEYFOUND_ERROR:
                    rc = rs_error_geterrcode(cd, *p_errh);
                    break;
                default:
                    ss_error;
            }
#endif /* !SS_MYSQL */
            if (rc == SU_SUCCESS) {
                key = rs_relh_keybyname(cd, relh, keyname);
                if (key == NULL) {
                    rc = E_KEYNOTEXIST_S;
                    rs_error_create(p_errh, E_KEYNOTEXIST_S, rs_entname_getname(keyname));
                }
            }
        }

        if (p_issynctable != NULL) {
            ss_dassert(!issynctable);
            *p_issynctable = relh != NULL && rs_relh_issync(cd, relh);
        }

        if (!issynctable &&
            rc == SU_SUCCESS &&
            relh != NULL &&
            !rs_relh_isbasetable(cd, relh))
        {
            rc = E_NOTBASETABLE;
            rs_error_create(p_errh, E_NOTBASETABLE);
        }

        if (rc == SU_SUCCESS && rs_key_isclustering(cd, key)) {
            rc = DBE_ERR_FAILED;
            rs_error_create(p_errh, DBE_ERR_FAILED);
        }

        if (rc == DBE_RC_SUCC) {
            TliCursorT* tcur;
            TliRetT     trc;
            long        keyid;
            char*       keyname;

            tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
            ss_assert(tcur != NULL);

            trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, &keyid);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_KEY_NAME, &keyname);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REF_KEY_ID,
                TLI_RELOP_EQUAL,
                rs_key_id(cd, key));
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorOpen(tcur);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorNext(tcur);
            if (trc!=TLI_RC_END) {
                rc = E_INDEX_IS_USED_S;
                if (keyname == NULL || keyname[0] == '$') {
                    keyname = (char *)"";
                }
                rs_error_create(p_errh, rc, keyname);
            }
            TliCursorFree(tcur);
        }

        if (rc == DBE_RC_SUCC) {
            rs_key_link(cd, key);
            key_linked = TRUE;
            rc = dbe_trx_deleteindex(tb_trans_dbtrx(cd, trans), relh, key);
            if (rc != DBE_RC_SUCC) {
                rs_error_create(p_errh, rc);
            }
        }

        if (rc == DBE_RC_SUCC) {
            if (!dd_droponekey(tcon, relh, key, p_errh)) {
                rc = DBE_ERR_FAILED;
            }
        }

        if (key_linked) {
            rs_key_done(cd, key);
        }

        if (relh != NULL && !trxfoundp) {
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);
        }

        TliConnectDone(tcon);

        return(rc);
}

/*##**********************************************************************\
 *
 *              tb_dd_dropindex_relh
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *      relh -
 *
 *      keyname -
 *
 *
 *      p_issynctable -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
su_ret_t tb_dd_dropindex_relh(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_entname_t* keyname,
        bool issynctable,
        bool* p_issynctable,
        rs_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        rs_key_t* key;
        TliConnectT* tcon;
        bool key_linked = FALSE;

        ss_assert(cd != NULL);
        ss_assert(trans != NULL);
        ss_assert(keyname != NULL);
        ss_assert(relh != NULL);

        ss_dprintf_1(("tb_dd_dropindex '%s.%s'\n",
            rs_entname_getprintschema(keyname),
            rs_entname_getname(keyname)));

        tcon = TliConnectInitByTrans(cd, trans);

            /*
              Search from the database.
             */

        key = rs_relh_keybyname(cd, relh, keyname);

        if (key == NULL) {
            rc = E_KEYNOTEXIST_S;
            rs_error_create(p_errh, E_KEYNOTEXIST_S, rs_entname_getname(keyname));
        }

        if (p_issynctable != NULL) {
            ss_dassert(!issynctable);
            *p_issynctable = rs_relh_issync(cd, relh);
        }

        if (!issynctable &&
            rc == SU_SUCCESS &&
            !rs_relh_isbasetable(cd, relh))
        {
            rc = E_NOTBASETABLE;
            rs_error_create(p_errh, E_NOTBASETABLE);
        }

        if (rc == SU_SUCCESS && rs_key_isclustering(cd, key)) {
            rc = DBE_ERR_FAILED;
            rs_error_create(p_errh, DBE_ERR_FAILED);
        }

        if (rc == DBE_RC_SUCC) {
            TliCursorT* tcur;
            TliRetT     trc;
            long        keyid;
            char*       keyname;

            tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);
            ss_assert(tcur != NULL);

            trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, &keyid);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_KEY_NAME, &keyname);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_FORKEYS_REF_KEY_ID,
                TLI_RELOP_EQUAL,
                rs_key_id(cd, key));
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorOpen(tcur);
            ss_assert(trc == TLI_RC_SUCC);

            trc = TliCursorNext(tcur);
            if (trc!=TLI_RC_END) {
                rc = E_INDEX_IS_USED_S;
                if (keyname == NULL || keyname[0] == '$') {
                    keyname = (char *)"";
                }
                rs_error_create(p_errh, rc, keyname);
            }
            TliCursorFree(tcur);
        }

        if (rc == DBE_RC_SUCC) {
            rs_key_link(cd, key);
            key_linked = TRUE;
            rc = dbe_trx_deleteindex(tb_trans_dbtrx(cd, trans), relh, key);
            if (rc != DBE_RC_SUCC) {
                rs_error_create(p_errh, rc);
            }
        }

        if (rc == DBE_RC_SUCC) {
            if (!dd_droponekey(tcon, relh, key, p_errh)) {
                rc = DBE_ERR_FAILED;
            }
        }

        if (key_linked) {
            rs_key_done(cd, key);
        }

        TliConnectDone(tcon);

        return(rc);
}

#endif /* SS_NODDUPDATE */

#ifndef SS_NOVIEW

static rs_viewh_t* dd_viewpresent(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        rs_entname_t* viewname,
        ulong* p_viewid,
        bool* p_trxviewh,
        rs_err_t** p_errh)
{
        rs_viewh_t* viewh;
        dbe_trx_t* trx;
        TliRetT trc;
        TliConnectT* tcon;
        bool succp;
        rs_rbuf_present_t vp;
        ss_beta(int nloop = 0;)

        ss_dassert(cd != NULL);
        ss_dassert(viewname != NULL);

        *p_trxviewh = FALSE;

        ss_dprintf_1(("dd_viewpresent '%s.%s'\n",
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));

        do {
            vp = rs_rbuf_viewpresent(cd, rbuf, viewname, &viewh, p_viewid);

            switch (vp) {

                case RSRBUF_BUFFERED:
                    /* The view is already buffered, return the entry
                     * from the buffer.
                     */
                    ss_dprintf_2(("dd_viewpresent RSRBUF_BUFFERED\n"));
                    break;

                case RSRBUF_EXISTS:
                    /* Relation exists but is not buffered. Read the view
                     * information and add it to the buffer.
                     */
                    ss_dprintf_2(("dd_viewpresent RSRBUF_EXIST\n"));
                    tcon = TliConnectInit(cd);

                    viewh = dd_readviewh(tcon, *p_viewid);

                    trc = TliCommit(tcon);
                    ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                    TliConnectDone(tcon);

                    if (viewh == NULL) {
                        ss_dprintf_2(("dd_viewpresent Failed to read viewh from system tables\n"));
                        rs_error_create(p_errh, E_VIEWNOTEXIST_S, rs_entname_getname(viewname));
                        return(NULL);
                    }

                    succp = rs_rbuf_insertviewh(cd, rbuf, viewh);
                    if (!succp) {
                        /* Insert failed, maybe some other thread added
                         * the info to the buffer.
                         */
                        rs_viewh_done(cd, viewh);
                        viewh = NULL;
                    }
                    break;

                case RSRBUF_NOTEXIST:
                    /* Relation does not exist. */
                    ss_dprintf_2(("dd_viewpresent RSRBUF_NOTEXIST\n"));
                    viewh = NULL;
                    break;

                case RSRBUF_AMBIGUOUS:
                    /* Relation does not exist. */
                    ss_dprintf_2(("dd_viewpresent RSRBUF_AMBIGUOUS\n"));
                    rs_error_create(p_errh, E_AMBIGUOUS_S, rs_entname_getname(viewname));
                    return(NULL);

                default:
                    ss_error;
            }

            ss_bassert(nloop++ < 10);

        } while (vp == RSRBUF_EXISTS);

        trx = tb_trans_dbtrx(cd, trans);

        if (trx != NULL) {
            if (viewh != NULL) {
                /* Found */
                if (dbe_trx_viewdeleted(trx, rs_viewh_entname(cd, viewh))) {
                    rs_viewh_done(cd, viewh);
                    viewh = NULL;
                }
            } else {
                /* Not found. */
                if (dbe_trx_viewinserted(trx, viewname, &viewh)) {
                    rs_viewh_link(cd, viewh);
                    *p_viewid = rs_viewh_viewid(cd, viewh);
                    *p_trxviewh = TRUE;
                } else {
                    viewh = NULL;
                }
            }
        }

        if (viewh == NULL) {
            rs_error_create(p_errh, E_VIEWNOTEXIST_S, rs_entname_getname(viewname));
        }

        return(viewh);
}

/*#***********************************************************************\
 *
 *              dd_getviewh
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      viewname -
 *
 *
 *      p_priv -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_viewh_t* dd_getviewh(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        rs_entname_t* viewname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_viewh_t* viewh;
        ulong viewid;
        bool trxviewh;

        ss_dassert(cd != NULL);
        ss_dassert(viewname != NULL);

        ss_dprintf_1(("dd_getviewh '%s.%s'\n",
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));

        viewh = dd_viewpresent(cd, trans, rbuf, viewname, &viewid, &trxviewh, p_errh);
        if (viewh == NULL) {
            return(NULL);
        }

        if (!trxviewh && p_priv != NULL) {
            tb_priv_getrelpriv(
                cd,
                viewid,
                rs_viewh_issysview(cd, viewh),
                FALSE,
                p_priv);
        }

        return(viewh);
}

/*##**********************************************************************\
 *
 *              tb_dd_getviewh
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      viewname -
 *
 *
 *      p_priv -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_viewh_t* tb_dd_getviewh(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* viewname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_viewh_t* viewh;
        rs_rbuf_t* rbuf;
        rs_entname_t en;
        rs_auth_t* auth;

        ss_dprintf_1(("tb_dd_getviewh '%s.%s'\n",
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));

        if (p_priv != NULL) {
            *p_priv = NULL;
        }

        rbuf = rs_sysi_rbuf(cd);

        if (rs_entname_getschema(viewname) == NULL) {
            /* No schema given. First try with username.
             */
            rs_viewh_t* viewh;
            rs_rbuf_present_t rp;

            auth = rs_sysi_auth(cd);
            rs_entname_initbuf(
                &en,
                rs_auth_catalog(cd, auth),
                rs_auth_schema(cd, auth),
                rs_entname_getname(viewname));

            viewh = dd_getviewh(cd, trans, rbuf, &en, p_priv, NULL);
            if (viewh != NULL) {
                /* Found a view. */
                return(viewh);
            }

            /* Try to find a relation from own schema.
             */
            rp = rs_rbuf_relpresent(cd, rbuf, &en, NULL, NULL);
            switch (rp) {
                case RSRBUF_BUFFERED:
                case RSRBUF_EXISTS:
                    /* Found a relation from own schema, do not try to
                     * find a viewh any more
                     */
                    rs_error_create(p_errh, E_VIEWNOTEXIST_S, rs_entname_getname(viewname));
                    return(NULL);
                case RSRBUF_NOTEXIST:
                case RSRBUF_AMBIGUOUS:
                    break;
                default:
                    ss_error;
            }
        } else if (rs_entname_getcatalog(viewname) == NULL) {
            auth = rs_sysi_auth(cd);
            ss_dassert(auth != NULL);
            rs_entname_initbuf(&en,
                               rs_auth_catalog(cd, auth),
                               rs_entname_getschema(viewname),
                               rs_entname_getname(viewname));
            viewname = &en;
        }
        viewh = dd_getviewh(cd, trans, rbuf, viewname, p_priv, p_errh);
        return(viewh);
}

/*#***********************************************************************\
 *
 *              dd_getviewhfromview
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      viewname -
 *
 *
 *      refviewh -
 *
 *
 *      p_priv -
 *
 *
 *      p_errh -
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
static rs_viewh_t* dd_getviewhfromview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        rs_entname_t* viewname,
        rs_viewh_t* refviewh,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_viewh_t* viewh;
        ulong viewid;
        bool trxviewh;
        bool succp;

        ss_dassert(cd != NULL);
        ss_dassert(viewname != NULL);

        ss_dprintf_1(("dd_getviewhfromview '%s.%s', refview = '%s.%s'\n",
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname),
            rs_viewh_schema(cd, refviewh),
            rs_viewh_name(cd, refviewh)));
        SS_PUSHNAME("dd_getviewhfromview");

        viewh = dd_viewpresent(cd, trans, rbuf, viewname, &viewid, &trxviewh, p_errh);
        if (viewh == NULL) {
            SS_POPNAME;
            return(NULL);
        }

        if (!trxviewh && p_priv != NULL) {
            succp = tb_priv_getrelprivfromview(
                        cd,
                        trans,
                        viewname,
                        viewid,
                        rs_viewh_issysview(cd, viewh),
                        FALSE,
                        rs_viewh_viewid(cd, refviewh),
                        rs_viewh_schema(cd, refviewh),
                        *p_priv,
                        p_errh);
            if (!succp) {
                rs_viewh_done(cd, viewh);
                viewh = NULL;
            }
        }

        SS_POPNAME;

        return(viewh);
}

/*##**********************************************************************\
 *
 *              tb_dd_getviewhfromview
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      viewname -
 *
 *
 *      refviewname -
 *
 *
 *      p_priv -
 *
 *
 *      p_errh -
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
rs_viewh_t* tb_dd_getviewhfromview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* viewname,
        rs_entname_t* refviewname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh)
{
        rs_viewh_t* viewh;
        rs_viewh_t* refviewh;
        rs_rbuf_t* rbuf;
        rs_entname_t en;

        if (p_priv != NULL) {
            *p_priv = NULL;
        }
        if (p_errh != NULL) {
            *p_errh = NULL;
        }
        refviewh = tb_dd_getviewh(cd, trans, refviewname, p_priv, p_errh);
        if (refviewh == NULL) {
            /* Referencing view does not exist. */
            return(NULL);
        }
        ss_dprintf_1(("tb_dd_getviewhfromview '%s.%s.%s', refview = '%s.%s.%s'\n",
            rs_entname_getprintcatalog(viewname),
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname),
            rs_viewh_catalog(cd, refviewh),
            rs_viewh_schema(cd, refviewh),
            rs_viewh_name(cd, refviewh)));

        rbuf = rs_sysi_rbuf(cd);
        if (rs_entname_getschema(viewname) == NULL) {
            /* No schema given. First try with refview schema.
             */
            rs_rbuf_present_t rp;

            ss_dprintf_2(("tb_dd_getviewhfromview:no schema given, try with refview schema\n"));
            rs_entname_initbuf(
                &en,
                rs_viewh_catalog(cd, refviewh),
                rs_viewh_schema(cd, refviewh),
                rs_entname_getname(viewname));
            viewh = dd_getviewhfromview(
                        cd,
                        trans,
                        rbuf,
                        &en,
                        refviewh,
                        p_priv,
                        p_errh);
            if (viewh != NULL) {
                /* Found. */
                rs_viewh_done(cd, refviewh);
                return(viewh);
            }
            if (p_errh != NULL) {
                rs_error_free(cd, *p_errh);
            }
            /* Check if we have a table in a view schema. 
             */
            rp = rs_rbuf_relpresent(cd, rbuf, &en, NULL, NULL);
            switch (rp) {
                case RSRBUF_BUFFERED:
                case RSRBUF_EXISTS:
                    /* Found a table from view schema, do not try to find a
                     * viewh any more
                     */
                    rs_viewh_done(cd, refviewh);
                    rs_error_create(p_errh, E_VIEWNOTEXIST_S, rs_entname_getname(&en));
                    return(NULL);
                case RSRBUF_NOTEXIST:
                case RSRBUF_AMBIGUOUS:
                    break;
                default:
                    ss_error;
            }
        } else if (rs_entname_getcatalog(viewname) == NULL) {
            rs_entname_initbuf(&en,
                               rs_viewh_catalog(cd, refviewh),
                               rs_entname_getschema(viewname),
                               rs_entname_getname(viewname));
            viewname = &en;
        }
        viewh = dd_getviewhfromview(
                    cd,
                    trans,
                    rbuf,
                    viewname,
                    refviewh,
                    p_priv,
                    p_errh);
        rs_viewh_done(cd, refviewh);
        return(viewh);
}

#ifndef SS_NODDUPDATE

/*#***********************************************************************\
 *
 *              dd_createview_sysif
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      viewh -
 *
 *
 *      auth -
 *
 *
 *      sysview -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static su_ret_t dd_createview_sysif(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_viewh_t* viewh,
        rs_auth_t* auth,
        bool sysview,
        rs_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        TliConnectT* tcon;
        dbe_trx_t* trx;

        SS_NOTUSED(auth);
        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(viewh != NULL);
        SS_PUSHNAME("dd_createview_sysif");

        ss_dprintf_1(("dd_createview_sysif '%s', sysview = %d\n",
            rs_viewh_name(cd, viewh), sysview));

        trx = tb_trans_dbtrx(cd, trans);

        if (!sysview) {
            rs_viewh_t* tmpviewh;
            rs_entname_t* tmpviewname;

            tmpviewname = rs_viewh_entname(cd, viewh);
            tmpviewh = tb_dd_getviewh(
                            cd,
                            trans,
                            tmpviewname,
                            NULL,
                            NULL);

            if (tmpviewh != NULL && !dbe_trx_viewdeleted(trx, tmpviewname)) {
                rs_error_create(p_errh, E_VIEWEXIST_S, rs_entname_getname(tmpviewname));
                rs_viewh_done(cd, tmpviewh);
                SS_POPNAME;
                return(E_VIEWEXIST_S);
            }
            if (tmpviewh != NULL) {
                rs_viewh_done(cd, tmpviewh);
            }
        }

        tcon = TliConnectInitByTrans(cd, trans);

        dd_createviewh(tcon, viewh, auth, sysview);

        if (!sysview) {
            /* Insert new view into the relation buffer.
               In case of system views, that will not be necessary
            */
            rc = dbe_trx_insertview(trx, viewh);
        }

        TliConnectDone(tcon);

        if (rc != SU_SUCCESS) {
            rs_error_create(p_errh, rc);
        }

        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *              tb_dd_createview
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      viewh -
 *
 *
 *      auth -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
su_ret_t tb_dd_createview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_viewh_t* viewh,
        rs_auth_t* auth,
        rs_err_t** p_errh)
{
        return(dd_createview_sysif(cd, trans, viewh, auth, FALSE, p_errh));
}

/*##**********************************************************************\
 *
 *              tb_dd_createsysview
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      viewh -
 *
 *
 *      auth -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
su_ret_t tb_dd_createsysview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_viewh_t* viewh,
        rs_auth_t* auth,
        rs_err_t** p_errh)
{
        return(dd_createview_sysif(cd, trans, viewh, auth, TRUE, p_errh));
}

/*##**********************************************************************\
 *
 *              tb_dd_dropview
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      viewname -
 *
 *
 *      p_viewid -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_dropview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* viewname,
        long* p_viewid,
        rs_entname_t** p_viewname,
        rs_err_t** p_errh)
{
        bool succp = TRUE;
        su_ret_t rc;
        rs_viewh_t* viewh;
        tb_relpriv_t* priv;
        rs_auth_t* auth;
        dbe_trx_t* trx;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(viewname != NULL);

        if (!tb_dd_checkobjectname(rs_entname_getname(viewname))) {
            rs_error_create(p_errh, E_VIEWNOTEXIST_S, "");
            return(FALSE);
        }

        ss_dprintf_1(("tb_dd_dropview '%s.%s'\n",
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));

        viewh = tb_dd_getviewh(cd, trans, viewname, &priv, p_errh);
        if (viewh == NULL) {
            return(FALSE);
        }
        auth = rs_sysi_auth(cd);
        viewname = rs_viewh_entname(cd, viewh);

        trx = tb_trans_dbtrx(cd, trans);

        if (!tb_priv_iscreatorrelpriv(cd, priv)) {
            succp = FALSE;
            rs_error_create(p_errh, E_VIEWNOTEXIST_S, rs_entname_getname(viewname));
        } else {
            TliConnectT* tcon;

            *p_viewid = rs_viewh_viewid(cd, viewh);

            rc = dbe_trx_deleteview(trx, viewh);

            if (rc == DBE_RC_SUCC) {

                tcon = TliConnectInitByTrans(cd, trans);

                succp = dd_dropviewh(tcon, viewh, p_errh);

                TliConnectDone(tcon);

            } else {
                rs_error_create(p_errh, rc);
                succp = FALSE;
            }
        }

        if (succp && p_viewname != NULL) {
            *p_viewname = rs_entname_copy(rs_viewh_entname(cd, viewh));
        }
        rs_viewh_done(cd, viewh);

        return(succp);
}

#endif /* SS_NODDUPDATE */

#endif /* SS_NOVIEW */

/*##**********************************************************************\
 *
 *              tb_dd_updatecardinal
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      relid -
 *
 *
 *      cardin -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_dd_updatecardinal(
        TliConnectT* tcon,
        long relid,
        rs_cardin_t* cardin)
{
        TliCursorT* tcur;
        TliRetT trc;
        ss_int8_t ntuples;
        ss_int8_t nbytes;
        dt_date_t date;
        rs_sysinfo_t* cd = TliGetCd(tcon);

        ss_dprintf_1(("tb_dd_updatecardinal: relid=%ld\n", relid));

        dd_setcardinaltransaoptions(tcon);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CARDINAL);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_CARDINAL_REL_ID, &relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt8t(tcur, RS_ANAME_CARDINAL_CARDIN, &ntuples);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt8t(tcur, RS_ANAME_CARDINAL_SIZE, &nbytes);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_CARDINAL_LAST_UPD, &date);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_CARDINAL_REL_ID,
                TLI_RELOP_EQUAL,
                relid);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (cardin == NULL) {
            SsInt8Set0(&ntuples);
            SsInt8Set0(&nbytes);
        } else {
            ntuples = rs_cardin_ntuples(cd, cardin);
            nbytes = rs_cardin_nbytes(cd, cardin);
#ifndef AUTOTEST_RUN
            ss_dassert(ss_debug_nocardinalcheck || !SsInt8IsNegative(ntuples));
            ss_dassert(ss_debug_nocardinalcheck || (!SsInt8IsNegative(nbytes) && !SsInt8Is0(nbytes)) || (SsInt8Is0(nbytes) && SsInt8Is0(ntuples)));
#endif
        }
        date = tb_dd_curdate();

/*        ss_dprintf_2(("tb_dd_updatecardinal: ntuples = %ld, nbytes = %ld\n",
            ntuples, nbytes));*/

        if (trc == TLI_RC_SUCC) {
            ss_dprintf_2(("tb_dd_updatecardinal: Update old cardinal\n"));
            trc = TliCursorUpdate(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || dbe_db_isreadonly(rs_sysi_db(cd)) || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        } else if (trc == TLI_RC_END) {
            ss_dprintf_2(("tb_dd_updatecardinal: Add a new cardinal\n"));
            ss_dassert(trc == TLI_RC_END);
            trc = TliCursorInsert(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || dbe_db_isreadonly(rs_sysi_db(cd)) || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        } else {
            ss_dassert(dbe_db_isreadonly(rs_sysi_db(cd)));
        }

        TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              event_partypesfromva
 *
 * Converts event parameter types from database format to memory format.
 *
 * Parameters :
 *
 *      p_paramcount - out
 *              Number of parameters.
 *
 *      p_paramtypes - out, give
 *              Newly allocated array of parameter types is stored into
 *          *p_paramtypes.
 *
 *      partypes_va - in
 *              Para,eter types in database format.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void event_partypesfromva(
        int* p_paramcount,
        int** p_paramtypes,
        va_t* partypes_va)
{
        int i;
        int parcount;
        va_index_t len;
        ss_uint2_t* dbtypes = NULL;

        ss_dassert(p_paramtypes != NULL);

        if (partypes_va != NULL) {
            dbtypes = va_getdata(partypes_va, &len);
            parcount = len / sizeof(ss_uint2_t);
        } else {
            parcount = 0;
        }

        if (p_paramcount != NULL) {
            *p_paramcount = parcount;
        }
        *p_paramtypes = SsMemAlloc(parcount * sizeof(int));

        for (i = 0; i < parcount; i++) {
            ss_uint2_t type;
            type = SS_UINT2_LOADFROMDISK(&dbtypes[i]);
            (*p_paramtypes)[i] = (int)type;
        }
}

/*#***********************************************************************\
 *
 *              dd_readsysrelcardinalfun
 *
 *
 *
 * Parameters :
 *
 *      userinfo -
 *
 *
 *      is_rel -
 *
 *
 *      relh_or_viewh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_readsysrelcardinalfun(
        void* userinfo,
        bool is_rel,
        void* relh_or_viewh,
        long id __attribute__ ((unused)),
        void* cardin __attribute__ ((unused)))
{
        rs_relh_t* relh;
        tb_dd_sysrel_userinfo_t* ui;

        ui = userinfo;

        if (is_rel && relh_or_viewh != NULL) {
            relh = relh_or_viewh;

            ss_dprintf_3(("dd_readsysrelcardinalfun: rel %s\n", rs_relh_name(ui->ui_cd, relh)));

            tb_dd_readcardinal(ui->ui_tcon, relh, NULL);
        }
}

/*#***********************************************************************\
 *
 *              dd_readsysrelcardinal
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_readsysrelcardinal(
        rs_sysinfo_t* cd,
        rs_rbuf_t* rbuf)
{
        tb_dd_sysrel_userinfo_t ui;
        TliConnectT* tcon;
        TliRetT trc;
        su_profile_timer;

        su_profile_start;

        tcon = TliConnectInit(cd);

        ui.ui_cd = cd;
        ui.ui_tcon = tcon;

        rs_rbuf_iterate(cd, rbuf, &ui, dd_readsysrelcardinalfun);

        trc = TliCommit(tcon);
        ss_dassert(trc == TLI_RC_SUCC);

        TliConnectDone(tcon);

        su_profile_stop("dd_readsysrelcardinal");
}

/*##**********************************************************************\
 *
 *              tb_dd_loadrbuf
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      full_refresh - in
 *              If TRUE, thw whole rbuf is read from system tables.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_dd_loadrbuf(
        rs_sysinfo_t* cd,
        rs_rbuf_t* rbuf,
        bool full_refresh,
        bool keep_cardinal)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char* namestr;
        char* schemastr;
        char* catalogstr;
        char* typestr;
        long id;
        rs_entname_t en;
        rs_rbuf_t* new_rbuf;
        su_profile_timer;

        ss_dassert(cd != NULL);
        ss_dassert(rbuf != NULL);

        ss_dprintf_1(("tb_dd_loadrbuf:full_refresh=%d, keep_cardinal=%d\n", full_refresh, keep_cardinal));
        su_profile_start;

        if (full_refresh) {
            ss_dprintf_2(("tb_dd_loadrbuf:reset old content\n"));
            new_rbuf = rs_rbuf_init_replace(cd, rbuf);
            su_profile_stop("tb_dd_loadrbuf:rbuf_init_replace");
            su_profile_start;
        } else {
            new_rbuf = rbuf;
        }

        ss_dprintf_2(("tb_dd_loadrbuf:read new content\n"));

        tcon = TliConnectInit(cd);

        /* Read table and view names.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLES_ID, &id);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, &namestr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, &schemastr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CATALOG, &catalogstr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_TYPE, &typestr);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            rs_entname_initbuf(&en,
                               catalogstr,
                               schemastr,
                               namestr);
            if (strcmp(typestr, RS_AVAL_BASE_TABLE) == 0 
                || strcmp(typestr, RS_AVAL_SYNC_TABLE) == 0) 
            {
                rs_cardin_t* cardin;

                if (rbuf != new_rbuf && keep_cardinal) {
                    cardin = rs_rbuf_getcardin(cd, rbuf, &en);
                } else {
                    cardin = NULL;
                }

                rs_rbuf_addrelnameandcardin(cd, new_rbuf, &en, id, cardin);

                if (cardin != NULL) {
                    rs_cardin_done(cd, cardin);
                }

                if (strcmp(schemastr, RS_AVAL_SYSNAME) == 0 && rbuf != new_rbuf) {
                    /* Need do add system tables to old rbuf also, otherwise
                     * following reads from SYS_PROCEDURE etc. will fail
                     * because table info is not in rbuf.
                     */
                    rs_rbuf_addrelname(cd, rbuf, &en, id);
                }
            } else if (strcmp(typestr, RS_AVAL_VIEW) == 0) {
#ifndef SS_NOVIEW
                rs_rbuf_addviewname(cd, new_rbuf, &en, id);
#endif /* SS_NOVIEW */
            }
            if (tb_dd_migratingtounicode &&
                strcmp(schemastr, RS_AVAL_SYSNAME) == 0 &&
                strcmp(typestr, RS_AVAL_BASE_TABLE) == 0) {
                tb_relh_t* tbrelh;
                tbrelh = tb_relh_create(
                            cd,
                            TliGetTrans(tcon),
                            namestr,
                            schemastr,
                            catalogstr,
                            NULL);
                ss_dassert(tbrelh != NULL);
                tb_relh_free(cd, tbrelh);
            }
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        su_profile_stop("tb_dd_loadrbuf:tables and views");
        su_profile_start;

#ifndef SS_NOPROCEDURE
        /* Read procedure names.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_PROCEDURES);
        ss_dassert(tcur != NULL);
        if (tcur != NULL) {
            trc = TliCursorColLong(tcur, RS_ANAME_PROCEDURES_ID, &id);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_PROCEDURES_NAME, &namestr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_PROCEDURES_SCHEMA, &schemastr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_PROCEDURES_CATALOG, &catalogstr);
            if (trc != TLI_RC_SUCC) {
                catalogstr = NULL;
            }

            trc = TliCursorOpen(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                rs_entname_initbuf(&en,
                                   catalogstr,
                                   schemastr,
                                   namestr);
                rs_rbuf_addname(cd, new_rbuf, &en, RSRBUF_NAME_PROCEDURE, id);
            }
            ss_dassert(trc == TLI_RC_END);

            TliCursorFree(tcur);
        }
#endif /* SS_NOPROCEDURE */

        su_profile_stop("tb_dd_loadrbuf:procedures");
        su_profile_start;

#ifndef SS_NOEVENT
        /* Read event names.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_EVENTS);
        ss_dassert(tcur != NULL);
        if (tcur != NULL) {
            va_t* partypes_va;
            int parcount;

            trc = TliCursorColLong(tcur, RS_ANAME_EVENTS_ID, &id);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_EVENTS_NAME, &namestr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_EVENTS_SCHEMA, &schemastr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_EVENTS_CATALOG, &catalogstr);
            if (trc != TLI_RC_SUCC) {
                catalogstr = NULL;
            }

            trc = TliCursorColVa(tcur, RS_ANAME_EVENTS_PARTYPES, &partypes_va);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorColInt(tcur, RS_ANAME_EVENTS_PARCOUNT, &parcount);
            ss_dassert(trc == TLI_RC_SUCC);

            trc = TliCursorOpen(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                int pcount;
                int* paramtypes;
                rs_event_t* event;
                bool succp;

                rs_entname_initbuf(&en,
                                   catalogstr,
                                   schemastr,
                                   namestr);

                event_partypesfromva(&pcount, &paramtypes, partypes_va);
                event = rs_event_init(cd, &en, id, pcount, paramtypes);
                succp = rs_rbuf_event_add(cd, new_rbuf, event);
                rs_event_done(cd, event);
                SsMemFree(paramtypes);
            }
            ss_dassert(trc == TLI_RC_END);

            TliCursorFree(tcur);
        }
#endif /* SS_NOEVENT */

        su_profile_stop("tb_dd_loadrbuf:events");
        su_profile_start;

#ifndef SS_NOSEQUENCE
        /* Read sequence names.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_SEQUENCES);
        ss_dassert(tcur != NULL);
        if (tcur != NULL) {
            trc = TliCursorColLong(tcur, RS_ANAME_SEQUENCES_ID, &id);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_SEQUENCES_NAME, &namestr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_SEQUENCES_SCHEMA, &schemastr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_SEQUENCES_CATALOG, &catalogstr);
            if (trc != TLI_RC_SUCC) {
                catalogstr = NULL;
            }

            trc = TliCursorOpen(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                rs_entname_initbuf(&en,
                                   catalogstr,
                                   schemastr,
                                   namestr);
                rs_rbuf_addname(cd, new_rbuf, &en, RSRBUF_NAME_SEQUENCE, id);
            }
            ss_dassert(trc == TLI_RC_END);

            TliCursorFree(tcur);
        }
#endif /* SS_NOSEQUENCE */

        su_profile_stop("tb_dd_loadrbuf:sequences");
        su_profile_start;

#ifndef SS_NOPROCEDURE
        /* Read trigger names.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TRIGGERS);
        ss_dassert(tcur != NULL);
        if (tcur != NULL) {
            trc = TliCursorColLong(tcur, RS_ANAME_TRIGGERS_ID, &id);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_TRIGGERS_NAME, &namestr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_TRIGGERS_SCHEMA, &schemastr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_TRIGGERS_CATALOG, &catalogstr);
            if (trc != TLI_RC_SUCC) {
                catalogstr = NULL;
            }

            trc = TliCursorOpen(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                rs_entname_initbuf(&en,
                                   catalogstr,
                                   schemastr,
                                   namestr);
                rs_rbuf_addname(cd, new_rbuf, &en, RSRBUF_NAME_TRIGGER, id);
            }
            ss_dassert(trc == TLI_RC_END);

            TliCursorFree(tcur);
        }
#endif /* SS_NOPROCEDURE */

        su_profile_stop("tb_dd_loadrbuf:triggers");
        su_profile_start;

#ifdef SS_SYNC
        /* Read publication names.
         */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               SNC_SCHEMA,
                               RS_RELNAME_PUBL);
        if (tcur != NULL) {
            trc = TliCursorColLong(tcur, RS_ANAME_PUBL_ID, &id);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_PUBL_NAME, &namestr);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            trc = TliCursorColStr(tcur, RS_ANAME_PUBL_CATALOG, &catalogstr);
            if (trc != TLI_RC_SUCC) {
                catalogstr = NULL;
            }

            trc = TliCursorOpen(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                rs_entname_initbuf(
                        &en,
                        catalogstr,
                        SNC_SCHEMA,
                        namestr);
                rs_rbuf_addname(cd, new_rbuf, &en, RSRBUF_NAME_PUBLICATION, id);
            }
            ss_dassert(trc == TLI_RC_END);

            TliCursorFree(tcur);
        }
#endif /* SS_SYNC */

        su_profile_stop("tb_dd_loadrbuf:publications");
        su_profile_start;

        if (full_refresh) {
            rs_rbuf_replace(cd, rbuf, new_rbuf);
            su_profile_stop("tb_dd_loadrbuf:rs_rbuf_replace");
            su_profile_start;
        }

        trc = TliCommit(tcon);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        if (!keep_cardinal) {
            ss_dprintf_2(("tb_dd_loadrbuf:update system table cardinal\n"));
            dd_readsysrelcardinal(cd, rbuf);
        }

        TliConnectDone(tcon);

        su_profile_stop("tb_dd_loadrbuf:dd_readsysrelcardinal");
}

#ifndef SS_MYSQL

typedef struct {
        char* tabname;
        char* colname;
} dd_updatenullcol_t;

static dd_updatenullcol_t dd_catalog_cols [] = {
    { RS_RELNAME_KEYS, RS_ANAME_KEYS_CATALOG },
    { RS_RELNAME_USERS, RS_ANAME_USERS_LOGIN_CATALOG },
    { RS_RELNAME_SEQUENCES, RS_ANAME_SEQUENCES_CATALOG },
    { RS_RELNAME_FORKEYS, RS_ANAME_KEYS_CATALOG },
    { RS_RELNAME_PROCEDURES, RS_ANAME_PROCEDURES_CATALOG },
    { RS_RELNAME_TRIGGERS, RS_ANAME_TRIGGERS_CATALOG },
    { RS_RELNAME_EVENTS, RS_ANAME_EVENTS_CATALOG },
    { RS_RELNAME_SCHEMAS, RS_ANAME_SCHEMAS_CATALOG },

    { RS_RELNAME_SYNC_BOOKMARKS, RS_ANAME_SYNC_BOOKMARKS_CATALOG },
    { RS_RELNAME_SYNC_MASTER_VERS, RS_ANAME_VERS_TABCATALOG },
    { RS_RELNAME_PUBL, RS_ANAME_PUBL_CATALOG },
    { RS_RELNAME_PUBL_STMTS, RS_ANAME_PUBL_STMT_MASTERCATALOG },
    { RS_RELNAME_PUBL_REPL_STMTS, RS_ANAME_PUBL_STMT_REPLICACATALOG },
    { RS_RELNAME_SYNC_MASTERS, RS_ANAME_SYNC_MASTER_REPLICACATALOG },
    { RS_RELNAME_SYNC_REPLICAS, RS_ANAME_SYNC_REPLICA_MASTERCATALOG },
    { RS_RELNAME_BULLETIN_BOARD, RS_ANAME_BULLETIN_BOARD_CATALOG },
    { RS_RELNAME_SYNCINFO, RS_ANAME_SYNCINFO_NODECATALOG },

    { RS_RELNAME_TABLES, RS_ANAME_TABLES_CATALOG }
};

static void dd_updatenullcolumn(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        char* tablename,
        char* colname,
        char* newvalue)
{
        bool succp;
        static char sqlstr_fmt[] =
            "UPDATE _SYSTEM.%s SET %s = '%s' WHERE %s IS NULL";
        char* sqlstr = SsMemAlloc(sizeof(sqlstr_fmt) +
                                  strlen(tablename) +
                                  strlen(colname) * 2 +
                                  strlen(newvalue));
        SsSprintf(sqlstr, sqlstr_fmt, tablename, colname, newvalue, colname);
        succp = dd_execsql(cd, sys, trans, sqlstr, NULL);
        ss_dassert(succp);
        succp = dd_execsql(cd, sys, trans, "COMMIT WORK", NULL);
        ss_dassert(succp);
        tb_trans_beginif(cd, trans);
        SsMemFree(sqlstr);
}

bool tb_dd_updatedefcatalog(
        rs_sysinfo_t* cd,
        tb_trans_t* trans)
{
        uint i;

        ss_dprintf_1(("tb_dd_updatedefcatalog\n"));

        if (RS_AVAL_DEFCATALOG == NULL) {
            sqlsystem_t* sys = tb_sqls_init(cd);
            for (i = 0; i < sizeof(dd_catalog_cols) / sizeof(dd_catalog_cols[0]); i++) {
                dd_updatenullcolumn(
                        cd,
                        trans,
                        sys,
                        dd_catalog_cols[i].tabname,
                        dd_catalog_cols[i].colname,
                        RS_AVAL_DEFCATALOG_NEW);
            }
            rs_rbuf_replacenullcatalogs(cd, rs_sysi_rbuf(cd), RS_AVAL_DEFCATALOG_NEW);
            rs_sdefs_setcurrentdefcatalog(RS_AVAL_DEFCATALOG_NEW);
            ss_dassert(strcmp(RS_AVAL_DEFCATALOG, RS_AVAL_DEFCATALOG_NEW) == 0);
            tb_sqls_done(cd, sys);
            dbe_db_migratetocatalogsuppmarkheader(rs_sysi_db(cd));
            return(TRUE);
        } else {
            return(FALSE);
        }
}
#endif /* !SS_MYSQL */


/*#***********************************************************************\
 *
 *              dd_checkobjectname
 *
 * Checks that the given name for a db object (table/view/procedure/sequence/event) is valid.
 *
 * Parameters :
 *
 *      objectname - in, use
 *              boolean value
 *
 * Output params:
 *
 * Return value :
 *
 *       TRUE object name valid
 *
 *       FALSE object name is not valid
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_checkobjectname(char* objectname)
{
        if (objectname == NULL || objectname[0] == '\0') {
            return(FALSE);
        } else {
            return(TRUE);
        }
}


bool tb_dd_checkdefaultvalue(
        rs_sysi_t*      cd,
        rs_atype_t*     natatype,
        rs_atype_t*     defatype,
        rs_aval_t*      defaval,
        rs_err_t**      p_errh)
{
        rs_atype_t*     sysatype;
        rs_aval_t*      nataval;
        rs_aval_t*      sysaval;
        RS_AVALRET_T    r;

        sysatype = rs_atype_init_sqldt(cd, RSSQLDT_WVARCHAR);
        sysaval = rs_aval_create(cd, sysatype);
        nataval = rs_aval_create(cd, natatype);

        /* SsPrintf("defaval: %s\n", rs_aval_print(cd, defatype, defaval)); */
        
        r = rs_aval_convert_ext(
                cd,
                sysatype,
                sysaval,
                defatype,
                defaval,
                p_errh);

        /* SsPrintf("sysaval: %s\n", rs_aval_print(cd, sysatype, sysaval)); */
        
        if (r == RSAVR_SUCCESS) {
            r = rs_aval_convert_ext(
                    cd,
                    natatype,
                    nataval,
                    sysatype,
                    sysaval,
                    p_errh);
        }

        /* SsPrintf("nataval: %s\n", rs_aval_print(cd, natatype, nataval)); */
        
        /* This might not be necessary. */
        if (r == RSAVR_SUCCESS) {
            r = rs_aval_convert_ext(
                    cd,
                    sysatype,
                    sysaval,
                    natatype,
                    nataval,
                    p_errh);
        }

        rs_aval_free(cd, sysatype, sysaval);
        rs_aval_free(cd, natatype, nataval);
        rs_atype_free(cd, sysatype);
        
        return (r == RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *              tb_dd_isforkeyref
 *
 * Checks if there is a foreign key reference to a given table.
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     rs_relh_t*  relh, in, use, relation
 *     rs_err_t**  p_errh, in, use, error structure
 *
 * Return value : TRUE if there is, FALSE otherwice
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_isforkeyref(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_err_t** p_errh)
{
        TliConnectT* tcon;
        bool isforkeyref = FALSE;
        bool succp = TRUE;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relh != NULL);
        
        tcon = TliConnectInitByTrans(cd, trans);

        succp = dd_isforkeyref(tcon, relh, &isforkeyref, p_errh);

        TliConnectDone(tcon);

        if (succp) {
            return (isforkeyref);
        } else {
            return (FALSE);
        }
}

/*#***********************************************************************\
 *
 *              tb_dd_get_createtime
 *
 * Get create time of the relation from the system tables
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     rs_relh_t*  relh, in, use, relation
 *     dt_date_t*  time, inout, create time
 *
 * Return value : TRUE if create time found
 *                FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_get_createtime(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        dt_date_t*   time)
{
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(time != NULL);

        memset(time, 0, sizeof(dt_date_t));

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);

        ss_dassert(tcur != NULL);

        trc = TliCursorColDate(tcur, RS_ANAME_TABLES_CREATIME, time);

        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_TABLES_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));

        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);

        if (trc == TLI_RC_END) {
            memset(time, 0, sizeof(dt_date_t)); /* If not found, initialize */
        }

        ss_rc_assert((trc == TLI_RC_SUCC || trc == TLI_RC_END), TliCursorErrorCode(tcur));
        
        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return(trc == TLI_RC_SUCC);
}

/*#***********************************************************************\
 *
 *              tb_dd_get_updatetime
 *
 * Get update time of the relation from the system tables
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     rs_relh_t*  relh, in, use, relation
 *     dt_date_t*  time, inout, update time
 *
 * Return value : TRUE if update time found
 *                FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_get_updatetime(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        dt_date_t*   time)
{
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(time != NULL);

        memset(time, 0, sizeof(dt_date_t));

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CARDINAL);

        ss_dassert(tcur != NULL);

        trc = TliCursorColDate(tcur, RS_ANAME_CARDINAL_LAST_UPD, time);

        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(
                tcur,
                RS_ANAME_CARDINAL_REL_ID,
                TLI_RELOP_EQUAL,
                rs_relh_relid(cd, relh));

        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        
        ss_assert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);

        if (trc == TLI_RC_END) {
            memset(time, 0, sizeof(dt_date_t)); /* If not found, initialize */
        }

        ss_rc_assert((trc == TLI_RC_SUCC || trc == TLI_RC_END), TliCursorErrorCode(tcur));
        
        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return(trc == TLI_RC_SUCC);
}

#ifdef SS_DEBUG
static void tb_dd_print_system_forkeys(
        rs_sysi_t*   cd,
        tb_trans_t*  trans)
{
        long        refrelid = 0;
        long        createrelid = 0;
        long        keyid = 0;
        long        refkeyid = 0;
        long         keyp_no, attr_no, attr_id;
        int          kptype;
        va_t*        const_value = NULL;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        tcon = TliConnectInitByTrans(cd, trans);
        ss_dassert(tcon != NULL);

        /* Print out SYS_FORKEYS */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);

        ss_dassert(tcur != NULL);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, &keyid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, &refrelid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_KEY_ID, &refkeyid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_CREATE_REL_ID, &createrelid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, TLI_RELOP_GE, 0);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorNext(tcur);

        ss_dprintf_1(("SYS_FORKEYS\n"));
        ss_dprintf_1(("ID  REF_REL_ID CREATE_REL_ID REF_KEY_ID\n"));
 
        while (trc == TLI_RC_SUCC) {
            ss_dprintf_1(("%lu %lu %lu %lu\n", keyid, refrelid, createrelid, refkeyid));

            if (trc == TLI_RC_SUCC) {
                trc = TliCursorNext(tcur);
            }            
        }

        TliCursorFree(tcur);

        /* Print out SYS_FORKEYPARTS */
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYPARTS);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ID, &keyid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_KEYP_NO, &keyp_no);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ATTR_NO, &attr_no);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ATTR_ID, &attr_id);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColInt(tcur, RS_ANAME_FORKEYPARTS_ATTR_TYPE, &kptype);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColVa(tcur, RS_ANAME_FORKEYPARTS_CONST_VALUE, &const_value);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYPARTS_ID, TLI_RELOP_GE, 0);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorNext(tcur);

        ss_dprintf_1(("SYS_FORKEYSPARTS\n"));
        ss_dprintf_1(("ID KEYP_NO ATTR_NO ATTR_ID CONST_VALUE\n"));
 
        while (trc == TLI_RC_SUCC) {
            long const_keyid;
            
            if (const_value != NULL) {
                switch(kptype) {
                    case RSAT_KEY_ID:
                        const_keyid = va_getlong(const_value);
                        break;
                    default:
                        const_keyid = 0;
                }
            } else {
                const_keyid = 0;
            }
            
            ss_dprintf_1(("%ld %ld %ld %ld %ld\n", keyid, keyp_no, attr_no, attr_id, const_keyid));

            if (trc == TLI_RC_SUCC) {
                trc = TliCursorNext(tcur);
            }            
        }

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}
#endif /* SS_DEBUG */

/*#***********************************************************************\
 *
 *              tb_dd_update_forkeys
 *
 * Update all foreign keys from old_relid to new_relid.
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     rs_relh_t*  relh, in, use, old relation
 *     ulong       old_relid, in, use, old relation id
 *     ulong       new_relid, in, use, new relation id
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_update_forkeys(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        ulong        old_relid,
        ulong        new_relid,
        rs_err_t**   p_errh)
{
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;
        long        refrelid = 0;
        long        createrelid = 0;
        long        keyid = 0;
        long        refkeyid = 0;
        rs_relh_t*   refrelh = NULL;
        rs_relh_t*   old_relh = NULL;
        rs_ttype_t*  ttype = rs_relh_ttype(cd, relh); 
        rs_ttype_t*  old_ttype;
        /* va_t*        const_value; */
        bool         succp = TRUE;

        ss_dassert(cd != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relh != NULL);

        SS_PUSHNAME("tb_dd_update_forkeys");
        ss_dprintf_1(("tb_dd_update_forkeys\n"));

        tcon = TliConnectInitByTrans(cd, trans);
        ss_dassert(tcon != NULL);

        old_relh = tb_dd_getrelhbyid(cd, trans, old_relid, NULL, p_errh);

        if (old_relh == NULL) {
            rs_error_create(p_errh, E_FORKNOTUNQK);
            succp = FALSE;
            goto error_handling;
        }

        ss_dprintf_1(("old relh %s id %lu\n", rs_relh_name(cd, old_relh), rs_relh_relid(cd, old_relh)));
        
        old_ttype = rs_relh_ttype(cd, old_relh);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);

        ss_dassert(tcur != NULL);

        /* First we need to check that all foreign keys referencing to this parent table
           are consistent after the alter table. Find all foreign keys referencing to this
           table. */

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, &keyid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, &refrelid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_KEY_ID, &refkeyid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_CREATE_REL_ID, &createrelid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, TLI_RELOP_EQUAL, old_relid);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorNext(tcur);
 
        while (trc == TLI_RC_SUCC) {
            /* Find out the relation referencing to this parent table */
            refrelh = tb_dd_getrelhbyid(cd, trans, createrelid, NULL, p_errh);
            
            if (refrelh) {
                rs_key_t* primkey;
                uint j = 0;
                rs_key_t* refkey = rs_relh_refkeybyid(cd, old_relh, keyid);
                rs_ttype_t* ref_ttype = rs_relh_ttype(cd, refrelh);

                primkey = dd_resolverefkey(cd, relh, refkey);

                if (!primkey) {
                    rs_error_create(p_errh, E_FORKNOTUNQK);
                    succp = FALSE;
                    goto error_handling;
                }

                /* Now we should know both referencing foreign key and referenced key */

                for (j = 0; j < rs_key_nparts(cd, refkey); j++) {

                    if (rs_keyp_parttype(cd, refkey, j) == RSAT_USER_DEFINED) {
                        rs_atype_t* ref_atype;
                        rs_atype_t* atype;
                        rs_ano_t ref_ano;
                        rs_ano_t ano;
                        
                        ref_ano = rs_keyp_ano(cd, refkey, j);
                        ano = rs_keyp_ano(cd, primkey, j);
                        ref_atype = rs_ttype_sql_atype(cd, ref_ttype, ref_ano);
                        atype = rs_ttype_sql_atype(cd, ttype, ano);

#ifdef SS_COLLATION
                        if (rs_keyp_collation(cd, refkey, j) != rs_keyp_collation(cd, primkey, j)) {
                            /* Collations do not match */
                            
                            rs_error_create(p_errh, E_FORKINCOMPATDTYPE_S, rs_ttype_sql_aname(cd, ttype, ano));
                            succp = FALSE;
                            goto error_handling;
                        }
#endif /* SS_COLLATION */
                    
                        if (ref_atype && atype) { 
                            rs_datatype_t ref_dt;
                            rs_datatype_t dt;
                       
                            ref_dt = rs_atype_datatype(cd, ref_atype);
                            dt = rs_atype_datatype(cd, atype);
            
                            if (ref_dt != dt) {
                                /* Data types do not match. */

                                rs_error_create(p_errh, E_FORKINCOMPATDTYPE_S, rs_ttype_sql_aname(cd, ttype, ano));
                                succp = FALSE;
                                goto error_handling;
                            }

                            /* Referential action SET NULL is not possible if referencing columns are not NOT NULL */
                            if (rs_key_update_action(cd, refkey) == SQL_REFACT_SETNULL ||
                                rs_key_delete_action(cd, refkey) == SQL_REFACT_SETNULL) {

                                if (!(rs_atype_nullallowed(cd, atype))) {
                                    rs_error_create(p_errh, E_NULLNOTALLOWED_S,
                                                    rs_ttype_sql_aname(cd, ttype, ano));

                                    succp = FALSE;
                                    goto error_handling;
                                }
                            }
                        }
                    }
                }

                SS_MEM_SETUNLINK(refrelh);
                rs_relh_done(cd, refrelh);
                refrelh = NULL;
            }

            if (trc == TLI_RC_SUCC) {
                trc = TliCursorNext(tcur);
            }
        }

        TliCursorFree(tcur);
        tcur = NULL;

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);

        ss_dassert(tcur != NULL);

        /* Secondly, now that we know foreign keys remain consistent after the alter table operation
         we can update foreign key and foreign key part information to match the new table. */

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, &keyid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, &refrelid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_KEY_ID, &refkeyid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_CREATE_REL_ID, &createrelid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, TLI_RELOP_EQUAL, old_relid);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorNext(tcur);

        while (trc == TLI_RC_SUCC) {
            TliCursorT*  tcur2 = NULL;
            long attr_id;
            long keyp_no;
            va_t* const_value;
            va_t keyid_va;
            int kptype;
            rs_ano_t attr_ano;
            long ref_foreign_id;
            su_pa_t* refkeys;
            rs_key_t* refkey;
            rs_key_t* primkey;

            ss_dprintf_1(("tb_dd_update_forkeys before keyid %lu refrelid %lu refkeyid %lu createrelid %lu\n",
                   keyid, refrelid, refkeyid, createrelid));

            refrelh = tb_dd_getrelhbyid(cd, trans, createrelid, NULL, NULL);
            ss_dassert(refrelh != NULL);

            ss_dprintf_1(("refrelh %s id %lu\n", rs_relh_name(cd, refrelh), rs_relh_relid(cd, refrelh)));
            refrelid = rs_relh_relid(cd, relh);
            refkey = rs_relh_refkeybyid(cd, old_relh, keyid);
            ss_dassert(refkey != NULL);
            ss_dprintf_1(("refkey %s id %lu\n", rs_key_name(cd, refkey), rs_key_id(cd, refkey)));
            primkey = dd_resolverefkey(cd, relh, refkey);
            ss_dassert(primkey != NULL);
            ss_dprintf_1(("primkey %s id %lu\n", rs_key_name(cd, primkey), rs_key_id(cd, primkey)));

            ss_dprintf_1(("tb_dd_update_forkeys updated keyid %lu refrelid %lu refkeyid %lu createrelid %lu\n",
                   keyid, refrelid, refkeyid, createrelid));

            /* Update referenced relation id */
            trc = TliCursorUpdate(tcur);
            ss_dassert(trc == TLI_RC_SUCC);

            ss_dprintf_1(("tb_dd_update_forkeys updated keyid %lu refrelid %lu refkeyid %lu createrelid %lu\n",
                   keyid, refrelid, refkeyid, createrelid));
#ifdef SS_DEBUG
            tb_dd_print_system_forkeys(cd ,trans);
#endif
            
            tcur2 = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYPARTS);

            /* Find out key parts of the foreign key */ 
            trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYPARTS_ID, &keyid);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYPARTS_KEYP_NO, &keyp_no);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYPARTS_ATTR_ID, &attr_id);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorColInt(tcur2, RS_ANAME_FORKEYPARTS_ATTR_TYPE, &kptype);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorColVa(tcur2, RS_ANAME_FORKEYPARTS_CONST_VALUE, &const_value);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorConstrLong(tcur2, RS_ANAME_FORKEYPARTS_ID, TLI_RELOP_EQUAL, keyid);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorOpen(tcur2);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorNext(tcur2);
            attr_ano = 0;

            while (trc == TLI_RC_SUCC) {

                if (attr_id != -1) {
                    /* Set new attribute id using the name */

                    ss_dprintf_1(("tb_dd_update_forkeys old attrid %lu attr no %lu name %s\n",
                           attr_id, rs_keyp_ano(cd, primkey, attr_ano),
                           rs_ttype_aname(cd, ttype, rs_keyp_ano(cd, primkey, attr_ano))));

                    attr_id = rs_ttype_attrid(cd, ttype, rs_keyp_ano(cd, primkey, attr_ano));

                    ss_dprintf_1(("tb_dd_update_forkeys new attrid %lu name %s\n",
                           attr_id, rs_ttype_aname(cd, ttype, rs_keyp_ano(cd, primkey, attr_ano))));

                    trc = TliCursorUpdate(tcur2);
                    ss_dassert(trc == TLI_RC_SUCC);

#ifdef SS_DEBUG
                    tb_dd_print_system_forkeys(cd ,trans);
#endif

                }

                attr_ano++;

                if (trc == TLI_RC_SUCC) {
                    trc = TliCursorNext(tcur2);
                }
            }

            TliCursorFree(tcur2);
            tcur2 = NULL;

            /* Now we need to find referencing foreign key from the child table, actually here is enought to find
             all of them. */

            ss_dprintf_1(("refkeys from %s id %lu\n", rs_relh_name(cd, refrelh), rs_relh_relid(cd, refrelh)));
            refkeys = rs_relh_refkeys(cd, refrelh);

            if (refkeys) {
                uint refkeyi = 0;

                su_pa_do(refkeys, refkeyi) {
                    rs_key_t* key = (rs_key_t *)su_pa_getdata(refkeys, refkeyi);

                    ss_dassert(key != NULL);

                    ss_dprintf_1(("refkeys %s %lu\n", rs_key_name(cd, key), rs_key_id(cd, key)));

                    if (rs_key_type(cd, key) == RS_KEY_FORKEYCHK && rs_key_refrelid(cd, key) == old_relid) {
                        bool update = FALSE;
                        ulong old_refkeyid;
                        
                        /* In this key we need to fix refkeyid and constant value to match a new key id*/

                        tcur2 = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);

                        ss_dassert(tcur2 != NULL);

                        trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYS_ID, &keyid);
                        ss_dassert(trc == TLI_RC_SUCC);
                        trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYS_REF_KEY_ID, (long int *) &old_refkeyid);
                        ss_dassert(trc == TLI_RC_SUCC);
                        trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYS_CREATE_REL_ID, &createrelid);
                        ss_dassert(trc == TLI_RC_SUCC);
                        trc = TliCursorConstrLong(tcur2, RS_ANAME_FORKEYS_ID, TLI_RELOP_EQUAL, rs_key_id(cd, key));
                        ss_dassert(trc == TLI_RC_SUCC);
                        trc = TliCursorOpen(tcur2);
                        ss_dassert(trc == TLI_RC_SUCC);
                        trc = TliCursorNext(tcur2);

                        /* Foreign key needs update only if we can find it */
                        if (rs_relh_keybyid(cd, old_relh, old_refkeyid)) {
                            rs_key_t* fk = rs_relh_keybyid(cd, old_relh, old_refkeyid);

                            ss_dprintf_1(("fk key %s %lu\n", rs_key_name(cd, fk), rs_key_id(cd, fk)));
                            
                            ss_dprintf_1(("tb_dd_update_forkeys old refkeyid %lu\n", old_refkeyid));
                            old_refkeyid = rs_key_id(cd, dd_resolverefkey(cd, relh, fk));
                            ss_dprintf_1(("tb_dd_update_forkeys new refkeyid %lu\n", old_refkeyid));

                            trc = TliCursorUpdate(tcur2);
                            ss_dassert(trc == TLI_RC_SUCC);
                            TliCursorFree(tcur2);

#ifdef SS_DEBUG
                            tb_dd_print_system_forkeys(cd ,trans);
#endif

                            tcur2 = NULL;

                            tcur2 = TliCursorCreate(tcon,
                                                    RS_AVAL_DEFCATALOG,
                                                    RS_AVAL_SYSNAME,
                                                    RS_RELNAME_FORKEYPARTS);

                            /* Find out key parts of the foreign key */
                            trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYPARTS_ID, &keyid);
                            ss_dassert(trc == TLI_RC_SUCC);
                            trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYPARTS_KEYP_NO, &keyp_no);
                            ss_dassert(trc == TLI_RC_SUCC);
                            trc = TliCursorColLong(tcur2, RS_ANAME_FORKEYPARTS_ATTR_ID, &attr_id);
                            ss_dassert(trc == TLI_RC_SUCC);
                            trc = TliCursorColInt(tcur2, RS_ANAME_FORKEYPARTS_ATTR_TYPE, &kptype);
                            ss_dassert(trc == TLI_RC_SUCC);
                            trc = TliCursorColVa(tcur2, RS_ANAME_FORKEYPARTS_CONST_VALUE, &const_value);
                            ss_dassert(trc == TLI_RC_SUCC);
                            trc = TliCursorConstrLong(tcur2, RS_ANAME_FORKEYPARTS_ID, TLI_RELOP_EQUAL, keyid);
                            ss_dassert(trc == TLI_RC_SUCC);
                            trc = TliCursorOpen(tcur2);
                            ss_dassert(trc == TLI_RC_SUCC);
                            trc = TliCursorNext(tcur2);

                            while (trc == TLI_RC_SUCC) {

                                if (const_value != NULL) {
                                    switch(kptype) {
                                        case RSAT_KEY_ID:
                                            /* We need to update */
                                            ref_foreign_id = va_getlong(const_value);
                                            ss_dprintf_1(("tb_dd_update_forkeys old key %s id = %lu const_value = %lu\n",
                                                    rs_key_name(cd, key), rs_key_id(cd, key), ref_foreign_id));
                                            va_setlong(&keyid_va, old_refkeyid);
                                            const_value = &keyid_va;
                                            update = TRUE;
                                            ss_dprintf_1(("tb_dd_update_forkeys new const value = %ld\n", va_getlong(const_value)));
                                            break;
                                    default:
                                        break;
                                    }

                                    if (update) {
                                        trc = TliCursorUpdate(tcur2);
                                        ss_dassert(trc == TLI_RC_SUCC);
                                        update = FALSE;

#ifdef SS_DEBUG
                                        tb_dd_print_system_forkeys(cd ,trans);
#endif

                                    }
                                }
                            
                                if (trc == TLI_RC_SUCC) {
                                    trc = TliCursorNext(tcur2);
                                }
                            }
                        }

                        TliCursorFree(tcur2);
                        tcur2 = NULL;
                    }
                }
            }

            ss_dprintf_1(("tb_trans_setrelhchanged %s id %lu\n", rs_relh_name(cd, refrelh), rs_relh_relid(cd, refrelh)));
            tb_trans_setrelhchanged(cd, trans, refrelh, NULL);
            SS_MEM_SETUNLINK(refrelh);
            rs_relh_done(cd, refrelh);
            refrelh = NULL;

            /* Do a statement commit */
            if (tb_trans_isstmtactive(cd, trans)) {
                bool finished = FALSE;

                do {
                    succp = tb_trans_stmt_commit(cd, trans, &finished, p_errh);
                } while (!finished);
            }

            if (!succp) {
                goto error_handling;
            }
            
            trc = TliCursorNext(tcur);
        }

 error_handling:
        
        trc = TliCommit(tcon);
        ss_dassert(trc == TLI_RC_SUCC);

        TliCursorFree(tcur);
        TliConnectDone(tcon);

#ifdef SS_DEBUG
        tb_dd_print_system_forkeys(cd, trans);
#endif

        if (refrelh) {
            SS_MEM_SETUNLINK(refrelh);
            rs_relh_done(cd, refrelh);
        }
        
        if (old_relh) {
            SS_MEM_SETUNLINK(old_relh);
            rs_relh_done(cd, old_relh);
        }

        SS_POPNAME;

        return (succp);
}

#ifdef SS_COLLATION
bool tb_dd_update_system_collations(
        tb_database_t* tdb,
        uint           charset_number,
        const char*    charset_name,
        const char*    charset_csname)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        /* tb_trans_t* trans; */
        uint charset_id;
        char* value_csname;
        char* value_colname;
        char* remarks;

        ss_dprintf_1(("tb_dd_update_system_collations\n"));
        SS_PUSHNAME("tb_dd_update_system_collations");

        tcon = TliConnectInitByTabDb(tdb);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLLATIONS);

        ss_dassert(tcur != NULL);

        trc = TliCursorColInt(tcur, RS_ANAME_COLLATIONS_ID, (int *) &charset_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLLATIONS_CHARSET_NAME, &value_csname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLLATIONS_COLLATION_NAME, &value_colname);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLLATIONS_REMARKS, &remarks);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        
        trc = TliCursorConstrInt(
                tcur,
                RS_ANAME_COLLATIONS_ID,
                TLI_RELOP_EQUAL,
                charset_number);

        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
 
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);

        if (trc == TLI_RC_END) {
            /* This collation is not yet on system table, insert information */
            
            charset_id = charset_number;
            value_csname = (char *)charset_csname;
            value_colname = (char *)charset_name;
            remarks = (char *)NULL;
            
            trc = TliCursorInsert(tcur);
            ss_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));
            trc = TliCommit(tcon);
            ss_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        } else if (trc != TLI_RC_SUCC) {
            ss_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END);
        }
        
        TliCursorFree(tcur);
        TliConnectDone(tcon);
        SS_POPNAME;

        return (TRUE);
}
#endif /* SS_COLLATION */

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
/*#***********************************************************************\
 *
 *              tb_dd_get_refrelid_by_relid
 *
 * Get child relation id using parent relation id
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     ulong       relid, in, use, parent relation id
 *
 * Return value : relid or 0
 *
 * Limitations  : 
 *
 * Globals used :
 */
ulong tb_dd_get_refrelid_by_relid(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        ulong        relid)
{
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;
        ulong        refrelid = 0;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);

        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_CREATE_REL_ID, (long int *) &refrelid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, TLI_RELOP_EQUAL, relid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorNext(tcur);
        ss_rc_assert((trc == TLI_RC_SUCC || trc == TLI_RC_END), TliCursorErrorCode(tcur));

        if (trc == TLI_RC_END) {
            refrelid = 0; /* Not found */
        }
        
        TliCursorFree(tcur);
        TliConnectDone(tcon);

        return(refrelid);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_tables_list
 *
 * Fill list of tables from SYS_TABLES
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_tables_list, inout, list of tables
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_tables_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_tables_list)
{
        dt_date_t date;
        tb_dd_sys_tables_t sys_table;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLES);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLES_ID, &sys_table.relid);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_NAME, (char **) &sys_table.table_name);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_SCHEMA, (char **) &sys_table.table_schema);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CATALOG, (char **) &sys_table.table_catalog);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_TYPE, (char **) &sys_table.table_type);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_TABLES_CREATIME, &date);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_CHECK, (char **) &sys_table.check);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLES_REMARKS, (char **) &sys_table.remarks);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_tables_t* table;

            table = SsMemAlloc(sizeof(tb_dd_sys_tables_t));
            memset(table, 0, sizeof(tb_dd_sys_tables_t));
            table->relid = sys_table.relid;
            table->table_name = (const char *)SsMemStrdup((char*)sys_table.table_name);
            table->table_schema = (const char *)SsMemStrdup((char*)sys_table.table_schema);
            table->table_catalog = (const char *)SsMemStrdup((char*)sys_table.table_catalog);
            table->table_type = (const char *)SsMemStrdup((char*)sys_table.table_type);
            table->createtime = (const char *)SsMemAlloc(64);
            dt_date_datetoasciiz_sql(&date, DT_DATE_SQLTIMESTAMP, (char *)table->createtime);

            if (sys_table.check) {
                table->check = (const char *)SsMemStrdup((char*)sys_table.check);
            } 
                
            if( sys_table.remarks) {
                table->remarks = (const char *)SsMemStrdup((char*)sys_table.remarks);
            } 
            
            su_list_insertlast(sys_tables_list, table);
        }

        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_columns_list
 *
 * Fill list of columns from SYS_COLUMNS
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_tables_list, inout, list of columns
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_columns_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_columns_list)
{
        tb_dd_sys_columns_t sys_columns;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS);

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_ID, &sys_columns.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_REL_ID, &sys_columns.rel_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_COLUMN_NAME, (char **) &sys_columns.column_name);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_COLUMN_NUMBER, &sys_columns.column_number);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_DATA_TYPE, (char **) &sys_columns.data_type);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_SQLDTYPE_NO, (long int *) &sys_columns.sql_data_type_num);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_DATA_TYPE_NO, &sys_columns.data_type_number);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_CHAR_MAX_LEN, &sys_columns.char_max_length);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_NUM_PRECISION, &sys_columns.numeric_precision);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_NUM_PREC_RDX, (long int *) &sys_columns.numeric_prec_radix);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_NUM_SCALE, (long int *) &sys_columns.numeric_scale);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_NULLABLE, (char **) &sys_columns.nullable);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_NULLABLE_ODBC, (long int *) &sys_columns.nullable_odbc);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_FORMAT, (char **) &sys_columns.format);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_DEFAULT_VAL, (char **) &sys_columns.default_val);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_ATTR_TYPE, &sys_columns.attr_type);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_REMARKS, (char **) &sys_columns.remarks);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_columns_t* column;

            column = SsMemAlloc(sizeof(tb_dd_sys_columns_t));
            memset(column, 0, sizeof(tb_dd_sys_columns_t));
            column->id = sys_columns.id;
            column->rel_id = sys_columns.rel_id;
            column->column_name = (const char *)SsMemStrdup((char*)sys_columns.column_name);
            column->column_number = sys_columns.column_number;
            column->data_type = (const char *)SsMemStrdup((char*)sys_columns.data_type);
            column->sql_data_type_num = sys_columns.sql_data_type_num;
            column->data_type_number = sys_columns.data_type_number;
            column->char_max_length = sys_columns.char_max_length;
            column->numeric_precision = sys_columns.numeric_precision;
            column->numeric_prec_radix = sys_columns.numeric_prec_radix;
            column->numeric_scale = sys_columns.numeric_scale;
            column->nullable = (const char *)SsMemStrdup((char*)sys_columns.nullable);
            column->nullable_odbc = sys_columns.nullable_odbc;

            if (sys_columns.format) {
                column->format = (const char *)SsMemStrdup((char*)sys_columns.format);
            } 
                
            if (sys_columns.default_val) {
                column->default_val = (const char*)SsMemStrdup((char*)sys_columns.default_val);
            } 
                
            column->attr_type = sys_columns.attr_type;

            if (column->remarks) {
                column->remarks = (const char *)SsMemStrdup((char*)sys_columns.remarks);
            } 
                
            su_list_insertlast(sys_columns_list, column);
        }

        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_columns_aux
 *
 * Fill list of column_aux from SYS_COLUMNS_AUX
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_columns_aux_list, inout, list of column aux info
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_columns_aux_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_columns_aux_list)
{
        tb_dd_sys_columns_aux_t sys_column_aux;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLUMNS_AUX);

        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_ID, &sys_column_aux.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_AUX_ORIGINAL_DEFAULT, (char **) &sys_column_aux.original_default);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_AUTO_INC_SEQ_ID, &sys_column_aux.auto_inc_seq_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_COLUMNS_AUX_EXTERNAL_DATA_TYPE, &sys_column_aux.external_data_type);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLUMNS_AUX_EXTERNAL_COLLATION, (char **) &sys_column_aux.external_collation);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_columns_aux_t* col_aux;

            col_aux = SsMemAlloc(sizeof(tb_dd_sys_columns_aux_t));
            memset(col_aux, 0, sizeof(tb_dd_sys_columns_aux_t));
            col_aux->id = sys_column_aux.id;

            if (sys_column_aux.original_default) {
                col_aux->original_default = (const char *)SsMemStrdup((char*)sys_column_aux.original_default);
            }

            col_aux->auto_inc_seq_id = sys_column_aux.auto_inc_seq_id;
            col_aux->external_data_type = sys_column_aux.external_data_type;
            
                
            if (sys_column_aux.external_collation) {
                col_aux->external_collation = (const char *)SsMemStrdup((char*)sys_column_aux.external_collation);
            }
                
            su_list_insertlast(sys_columns_aux_list, col_aux);
        }

        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_keys
 *
 * Fill list of keys from SYS_KEYS
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_keys_list, inout, list of keys
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_keys_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_keys_list)
{
        tb_dd_sys_keys_t sys_keys;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYS);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_ID, &sys_keys.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_REL_ID, &sys_keys.rel_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_NAME, (char **) &sys_keys.key_name);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_KEY_UNIQUE, (char **) &sys_keys.key_unique);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_KEY_NONUNQ_ODBC, (long int *) &sys_keys.key_nonunique_odbc);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CLUSTERING, (char **) &sys_keys.key_clustering);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_PRIMARY, (char **) &sys_keys.key_primary);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_PREJOINED, (char **) &sys_keys.key_prejoined);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_SCHEMA, (char **) &sys_keys.key_schema);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYS_CATALOG, (char **) &sys_keys.key_catalog);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYS_NREF, &sys_keys.key_nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_keys_t* key;

            key = SsMemAlloc(sizeof(tb_dd_sys_keys_t));
            memset(key, 0, sizeof(tb_dd_sys_keys_t));
            
            key->id = sys_keys.id;
            key->rel_id = sys_keys.rel_id;
            key->key_nonunique_odbc = sys_keys.key_nonunique_odbc;
            key->key_nref = sys_keys.key_nref;

            key->key_name = (const char *)SsMemStrdup((char*)sys_keys.key_name);
            key->key_unique = (const char *)SsMemStrdup((char*)sys_keys.key_unique);
            key->key_clustering = (const char *)SsMemStrdup((char*)sys_keys.key_clustering);
            key->key_primary = (const char *)SsMemStrdup((char*)sys_keys.key_primary);
            key->key_prejoined = (const char *)SsMemStrdup((char*)sys_keys.key_prejoined);
            key->key_schema = (const char *)SsMemStrdup((char*)sys_keys.key_schema);
            key->key_catalog = (const char *)SsMemStrdup((char*)sys_keys.key_catalog);

            su_list_insertlast(sys_keys_list, key);
        }

        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_keyparts
 *
 * Fill list of keyparts from SYS_KEYPARTS
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_keyparts_list, inout, list of keyparts
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_keyparts_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_keyparts_list)
{
        tb_dd_sys_keyparts_t sys_keyparts;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ID, &sys_keyparts.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_REL_ID, &sys_keyparts.rel_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_KEYP_NO, &sys_keyparts.keyp_no);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ATTR_ID, &sys_keyparts.attr_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ATTR_NO, &sys_keyparts.attr_no);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_ATTR_TYPE, &sys_keyparts.attr_type);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYPARTS_CONST_VALUE, (char **) &sys_keyparts.const_value);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYPARTS_ASCENDING, (char **) &sys_keyparts.ascending);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_keyparts_t* keypart;

            keypart = SsMemAlloc(sizeof(tb_dd_sys_keyparts_t));
            memset(keypart, 0, sizeof(tb_dd_sys_keyparts_t));

            keypart->id = sys_keyparts.id;
            keypart->rel_id = sys_keyparts.rel_id;
            keypart->keyp_no = sys_keyparts.keyp_no;
            keypart->attr_id = sys_keyparts.attr_id;
            keypart->attr_no = sys_keyparts.attr_no;
            keypart->attr_type = sys_keyparts.attr_type;

            if (sys_keyparts.const_value) {
                keypart->const_value = (const char *)SsMemStrdup((char*)sys_keyparts.const_value);
            }

            keypart->ascending = (const char *)SsMemStrdup((char*)sys_keyparts.ascending);

            su_list_insertlast(sys_keyparts_list, keypart);
        }

        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_keyparts_aux
 *
 * Fill list of keypart_aux from SYS_KEYPARTS_AUX
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_keyparts_aux_list, inout, list of keyparts_aux
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_keyparts_aux_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_keyparts_aux_list)
{
        tb_dd_sys_keyparts_aux_t sys_keyparts_aux;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_KEYPARTS_AUX);

        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_AUX_ID, &sys_keyparts_aux.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_AUX_KEYP_NO, &sys_keyparts_aux.keyp_no);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_KEYPARTS_AUX_PREFIX_LENGTH, &sys_keyparts_aux.prefix_length);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_keyparts_aux_t* keypart_aux;

            keypart_aux = SsMemAlloc(sizeof(tb_dd_sys_keyparts_aux_t));
            memset(keypart_aux, 0, sizeof(tb_dd_sys_keyparts_aux_t));
            
            keypart_aux->id = sys_keyparts_aux.id;
            keypart_aux->keyp_no = sys_keyparts_aux.keyp_no;
            keypart_aux->prefix_length = sys_keyparts_aux.prefix_length;

            su_list_insertlast(sys_keyparts_aux_list, keypart_aux);
        }

        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_forkeys
 *
 * Fill list of foreign keys from SYS_FORKEYS
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_forkeys_list, inout, list of foreign keys
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_forkeys_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_forkeys_list)
{
        tb_dd_sys_forkeys_t sys_forkeys;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_ID, &sys_forkeys.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_REL_ID, &sys_forkeys.ref_rel_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_CREATE_REL_ID, &sys_forkeys.create_rel_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REF_KEY_ID, &sys_forkeys.ref_key_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_REFTYPE, &sys_forkeys.ref_type);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_SCHEMA, (char **) &sys_forkeys.schema);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_CATALOG, (char **) &sys_forkeys.catalog);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_NREF, &sys_forkeys.nref);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_KEY_NAME, (char **) &sys_forkeys.key_name);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_KEY_ACTION, &sys_forkeys.key_action);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_forkeys_t* forkey;

            forkey = SsMemAlloc(sizeof(tb_dd_sys_forkeys_t));
            memset(forkey, 0, sizeof(tb_dd_sys_forkeys_t));
            
            forkey->id = sys_forkeys.id;
            forkey->ref_rel_id = sys_forkeys.ref_rel_id;
            forkey->create_rel_id = sys_forkeys.create_rel_id;
            forkey->ref_key_id = sys_forkeys.ref_key_id;
            forkey->ref_type = sys_forkeys.ref_type;
            forkey->nref = sys_forkeys.nref;
            forkey->key_action = sys_forkeys.key_action;

            forkey->schema = (const char *)SsMemStrdup((char*)sys_forkeys.schema);
            forkey->catalog = (const char *)SsMemStrdup((char*)sys_forkeys.catalog);
            forkey->key_name = (const char *)SsMemStrdup((char*)sys_forkeys.key_name);

            su_list_insertlast(sys_forkeys_list, forkey);
        }

        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_forkeyparts
 *
 * Fill list of foreign key parts from SYS_FORKEYPARTS
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_forkeyparts_list, inout, list of foreign key parts
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_forkeyparts_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_forkeyparts_list)
{
        tb_dd_sys_forkeyparts_t sys_forkeyparts;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYPARTS);

        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ID, &sys_forkeyparts.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_KEYP_NO, &sys_forkeyparts.keyp_no);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ATTR_NO, &sys_forkeyparts.attr_no);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ATTR_ID, &sys_forkeyparts.attr_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_ATTR_TYPE, &sys_forkeyparts.attr_type);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_KEYPARTS_CONST_VALUE, (char **) &sys_forkeyparts.const_value);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_PREFIX_LENGTH, &sys_forkeyparts.prefix_length);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_forkeyparts_t* forkeypart;

            forkeypart = SsMemAlloc(sizeof(tb_dd_sys_forkeyparts_t));
            memset(forkeypart, 0, sizeof(tb_dd_sys_forkeyparts_t));

            forkeypart->id = sys_forkeyparts.id;
            forkeypart->keyp_no = sys_forkeyparts.keyp_no;
            forkeypart->attr_id = sys_forkeyparts.attr_id;
            forkeypart->attr_no = sys_forkeyparts.attr_no;
            forkeypart->attr_type = sys_forkeyparts.attr_type;
            forkeypart->prefix_length = sys_forkeyparts.prefix_length;

            if (sys_forkeyparts.const_value) {
                forkeypart->const_value = (const char *)SsMemStrdup((char*)sys_forkeyparts.const_value);
            }
                
            su_list_insertlast(sys_forkeyparts_list, forkeypart);
        }

        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_schemas_list
 *
 * Fill list of schemas from SYS_SCHEMAS
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_schemas_list, inout, list of schemas
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_schemas_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_schemas_list)
{
        dt_date_t date;
        tb_dd_sys_schemas_t sys_schema;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_SCHEMAS);

        trc = TliCursorColLong(tcur, RS_ANAME_SCHEMAS_ID, &sys_schema.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_SCHEMAS_NAME, (char **) &sys_schema.name);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_SCHEMAS_OWNER,  (char **) &sys_schema.owner);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_SCHEMAS_CREATIME, &date);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_SCHEMAS_CATALOG, (char **) &sys_schema.catalog);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_schemas_t* schema;

            schema = SsMemAlloc(sizeof(tb_dd_sys_schemas_t));
            memset(schema, 0, sizeof(tb_dd_sys_schemas_t));

            schema->id = sys_schema.id;
            schema->name = (const char *)SsMemStrdup((char*)sys_schema.name);
            schema->owner = (const char *)SsMemStrdup((char*)sys_schema.owner);
            schema->catalog = (const char *)SsMemStrdup((char*)sys_schema.catalog);
            schema->createtime = (const char *)SsMemAlloc(64);
            dt_date_datetoasciiz_sql(&date, DT_DATE_SQLTIMESTAMP, (char*)schema->createtime);

            su_list_insertlast(sys_schemas_list, schema);
        }
        
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_sequences_list
 *
 * Fill list of sequences from SYS_SEQUENCES
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_sequences_list, inout, list of sequences
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_sequences_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_sequences_list)
{
        dt_date_t date;
        tb_dd_sys_sequences_t sys_sequence;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_SEQUENCES);

        trc = TliCursorColStr(tcur, RS_ANAME_SEQUENCES_NAME, (char **) &sys_sequence.name);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_SEQUENCES_ID, &sys_sequence.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_SEQUENCES_DENSE, (char **) &sys_sequence.dense);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_SEQUENCES_SCHEMA, (char **) &sys_sequence.schema);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_SEQUENCES_CATALOG, (char **) &sys_sequence.catalog);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_SEQUENCES_CREATIME, &date);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_sequences_t* seq;

            seq = SsMemAlloc(sizeof(tb_dd_sys_sequences_t));
            memset(seq, 0, sizeof(tb_dd_sys_sequences_t));

            seq->id = sys_sequence.id;
            seq->name = (const char *)SsMemStrdup((char*)sys_sequence.name);
            seq->dense = (const char *)SsMemStrdup((char*)sys_sequence.dense);
            seq->schema = (const char *)SsMemStrdup((char*)sys_sequence.schema);
            seq->catalog = (const char *)SsMemStrdup((char*)sys_sequence.catalog);
            seq->createtime = (const char *)SsMemAlloc(64);
            dt_date_datetoasciiz_sql(&date, DT_DATE_SQLTIMESTAMP, (char*)seq->createtime);

            su_list_insertlast(sys_sequences_list, seq);
        }
        
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_cardinal_list
 *
 * Fill list of table cardinalities from SYS_CARDINAL
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_cardinal_list, inout, list of table cardinalities
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_cardinal_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_cardinal_list)
{
        dt_date_t date;
        tb_dd_sys_cardinal_t sys_cardinal;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;
        ss_int8_t    cardin, size;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_CARDINAL);

        trc = TliCursorColLong(tcur, RS_ANAME_CARDINAL_REL_ID, &sys_cardinal.rel_id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt8t(tcur, RS_ANAME_CARDINAL_CARDIN, &cardin);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt8t(tcur, RS_ANAME_CARDINAL_SIZE, &size);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_CARDINAL_LAST_UPD, &date);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_cardinal_t* cardinal;

            cardinal = SsMemAlloc(sizeof(tb_dd_sys_cardinal_t));
            memset(cardinal, 0, sizeof(tb_dd_sys_cardinal_t));

            cardinal->rel_id = sys_cardinal.rel_id;
            cardinal->cardin = SsInt8GetNativeUint8(cardin);
            cardinal->size = SsInt8GetNativeUint8(size);
            cardinal->last_update = (const char *)SsMemAlloc(64);
            dt_date_datetoasciiz_sql(&date, DT_DATE_SQLTIMESTAMP, (char*)cardinal->last_update);

            su_list_insertlast(sys_cardinal_list, cardinal);
        }
        
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_tablemodes_list
 *
 * Fill list of tablemodes from SYS_TABLEMODES
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_tablemodes_list, inout, list of tablemodes
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_tablemodes_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_tablemodes_list)
{
        dt_date_t date;
        tb_dd_sys_tablemodes_t sys_tbmode;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_TABLEMODES);

        trc = TliCursorColLong(tcur, RS_ANAME_TABLEMODES_ID, &sys_tbmode.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLEMODES_MODE, (char **) &sys_tbmode.mode);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColDate(tcur, RS_ANAME_TABLEMODES_MODTIME, &date);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_TABLEMODES_MODUSER, (char **) &sys_tbmode.modify_user);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_tablemodes_t* tbmode;

            tbmode = SsMemAlloc(sizeof(tb_dd_sys_tablemodes_t));
            memset(tbmode, 0, sizeof(tb_dd_sys_tablemodes_t));

            tbmode->id = sys_tbmode.id;
            tbmode->mode = (const char *)SsMemStrdup((char*)sys_tbmode.mode);
            tbmode->modify_time = (const char *)SsMemAlloc(64);
            dt_date_datetoasciiz_sql(&date, DT_DATE_SQLTIMESTAMP, (char *)tbmode->modify_time);

            if (sys_tbmode.modify_user) {
                tbmode->modify_user = (const char *)SsMemStrdup((char*)sys_tbmode.modify_user);
            }
            
            su_list_insertlast(sys_tablemodes_list, tbmode);
        }
        
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_collations_list
 *
 * Fill list of collations from SYS_COLLATIONS
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_collations_list, inout, list of supported collations
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_collations_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_collations_list)
{
        tb_dd_sys_collations_t sys_colla;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_COLLATIONS);

        trc = TliCursorColLong(tcur, RS_ANAME_COLLATIONS_ID, &sys_colla.id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLLATIONS_CHARSET_NAME, (char **) &sys_colla.charset_name);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLLATIONS_COLLATION_NAME, (char **) &sys_colla.collation_name);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_COLLATIONS_REMARKS, (char **) &sys_colla.remarks);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_collations_t* coll;

            coll = SsMemAlloc(sizeof(tb_dd_sys_collations_t));
            memset(coll, 0, sizeof(tb_dd_sys_collations_t));

            coll->id = sys_colla.id;
            coll->charset_name = (const char *)SsMemStrdup((char*)sys_colla.charset_name);
            coll->collation_name = (const char *)SsMemStrdup((char*)sys_colla.collation_name);

            if (sys_colla.remarks) {
                coll->remarks = (const char *)SsMemStrdup((char*)sys_colla.remarks);
            }
            
            su_list_insertlast(sys_collations_list, coll);
        }
        
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_info_list
 *
 * Fill list of system information from SYS_INFO
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_info_list, inout, list of system information
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_info_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_info_list)
{
        tb_dd_sys_info_t sys_info;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_INFO);

        trc = TliCursorColStr(tcur, RS_ANAME_INFO_PROPERTY, (char **) &sys_info.property);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_INFO_VALUE_STR, (char **) &sys_info.val_str);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_INFO_VALUE_INT, &sys_info.val_int);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_info_t* info;

            info = SsMemAlloc(sizeof(tb_dd_sys_info_t));
            memset(info, 0, sizeof(tb_dd_sys_info_t));

            if (sys_info.property) {
                info->property = (const char *)SsMemStrdup((char*)sys_info.property);
            }
            
            info->val_int = sys_info.val_int;

            if (sys_info.val_str) {
                info->val_str = (const char *)SsMemStrdup((char*)sys_info.val_str);
            }
            
            su_list_insertlast(sys_info_list, info);
        }
        
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_fill_sys_blobs_list
 *
 * Fill list of blobs from SYS_BLOBS
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, sysinfo
 *     tb_trans_t* trans, in, use, transaction
 *     su_list_t*  sys_blobs_list, inout, list of blobs
 *
 * Return value : -
 *
 * Limitations  : 
 *
 * Globals used :
 */
void tb_dd_fill_sys_blobs_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_blobs_list)
{
        tb_dd_sys_blobs_t sys_blob;
        TliConnectT* tcon = NULL;
        TliCursorT*  tcur = NULL;
        TliRetT      trc = TLI_RC_SUCC;
        ss_int8_t    startpos, endsize, totalsize, id;

        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);

        tcon = TliConnectInitByTrans(cd, trans);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_BLOBS);

        trc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_ID, &id);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_STARTPOS, &startpos);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_ENDSIZE, &endsize);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColInt8t(tcur, RS_ANAME_BLOBS_TOTALSIZE, &totalsize);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_BLOBS_REFCOUNT, (long int *)&sys_blob.refcount);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_BLOBS_COMPLETE, (long int *)&sys_blob.complete);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_BLOBS_STARTCPNUM, (long int *)&sys_blob.startcpnum);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_BLOBS_NUMPAGES, (long int *)&sys_blob.numpages);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        
        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_dd_sys_blobs_t* blob;

            blob = SsMemAlloc(sizeof(tb_dd_sys_blobs_t));
            memset(blob, 0, sizeof(tb_dd_sys_blobs_t));


            blob->id = SsInt8GetNativeUint8(id);
            blob->refcount =sys_blob.refcount;
            blob->complete = sys_blob.complete;
            blob->startcpnum = sys_blob.startcpnum;
            blob->numpages = sys_blob.numpages;
            blob->startpos = SsInt8GetNativeUint8(startpos);
            blob->endsize  = SsInt8GetNativeUint8(endsize);
            blob->totalsize = SsInt8GetNativeUint8(totalsize);
            
            su_list_insertlast(sys_blobs_list, blob);
        }
        
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);
}

#endif /* SS_MYSQL || SS_MYSQL_AC */

#ifdef REFERENTIAL_INTEGRITY
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED

/*#***********************************************************************\
 *
 *              dd_insert_into_fkeyparts_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_insert_into_fkeyparts_unresolved(
    TliConnectT* tcon,
    long id,
    long field_ano,
    char* ref_field_name)
{
    TliCursorT* tcur;
    TliRetT     trc;
    long        loc_id;
    long        loc_field_ano;
    char*       loc_ref_field_name;

    ss_dassert(id != 0);

    ss_dprintf_3(("dd_insert_into_fkeyparts_unresolved\n"));

    loc_id = id;
    loc_field_ano = field_ano;
    loc_ref_field_name = ref_field_name ? SsMemStrdup(ref_field_name) : NULL;
    
    tcur = TliCursorCreate(tcon,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_FORKEYPARTS_UNRESOLVED);
    ss_assert(tcur != NULL);

    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_ID, &loc_id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_REF_FIELD_ANO, &loc_field_ano);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColStr(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_REF_FIELD_NAME, &loc_ref_field_name);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    trc = TliCursorInsert(tcur);
    ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

    TliCursorFree(tcur);
    
    if (loc_ref_field_name) {
        SsMemFree(loc_ref_field_name);
    }
}

/*#***********************************************************************\
 *
 *              dd_delete_all_from_fkeyparts_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_delete_all_from_fkeyparts_unresolved(
    TliConnectT* tcon,
    long id)
{
    TliCursorT* tcur;
    TliRetT     trc;

    ss_dassert(id != 0);
    
    ss_dprintf_3(("dd_delete_from_fkeyparts_unresolved:id=%ld\n", id));

    tcur = TliCursorCreate(tcon,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_FORKEYPARTS_UNRESOLVED);
    ss_dassert(tcur != NULL);

    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_ID, &id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_ID, TLI_RELOP_EQUAL, id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    trc = TliCursorOpen(tcur);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
        trc = TliCursorDelete(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
    }
    ss_dassert(trc == TLI_RC_END);

    TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_drop_fkeyparts_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_drop_fkeyparts_unresolved(
    TliConnectT* tcon)
{
    TliCursorT* tcur;
    TliRetT     trc;

    ss_dprintf_3(("dd_drop_fkeyparts_unresolved\n"));

    tcur = TliCursorCreate(tcon,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_FORKEYPARTS_UNRESOLVED);
    ss_dassert(tcur != NULL);

    trc = TliCursorOpen(tcur);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
    
        trc = TliCursorDelete(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
    }
    ss_dassert(trc == TLI_RC_END);

    TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_get_from_fkeyparts_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_get_from_fkeyparts_unresolved(
    TliConnectT* tcon,
    long id,
    long nfields,
    long* field_ano_ar,
    char** ref_field_name_ar)
{
    TliCursorT* tcur;
    TliRetT     trc;
    long        field_ano;
    char*       ref_field_name;
    bool        found = FALSE;
    long i;

    ss_dassert(id != 0);
    ss_dassert(nfields > 0);
    ss_dassert(field_ano_ar != NULL);
    ss_dassert(ref_field_name_ar != NULL);
    
    ss_dprintf_3(("dd_get_from_fkeyparts_unresolved:id=%ld\n", id));
    
    tcur = TliCursorCreate(tcon,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_FORKEYPARTS_UNRESOLVED);
    ss_dassert(tcur != NULL);

    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_ID, &id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_REF_FIELD_ANO, &field_ano);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColStr(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_REF_FIELD_NAME, &ref_field_name);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYPARTS_UNRESOLVED_ID, TLI_RELOP_EQUAL, id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    trc = TliCursorOpen(tcur);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    i = 0;
    while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
    
        ss_dassert(i < nfields);
        
        field_ano_ar[i]      = field_ano;
        ref_field_name_ar[i] = SsMemStrdup(ref_field_name);
        found = TRUE;
        ++i;
    }
    ss_dassert(trc == TLI_RC_END);

    TliCursorFree(tcur);
    
    return found;
}

/*#***********************************************************************\
 *
 *              dd_insert_into_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value : 
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_insert_into_fkeys_unresolved(
    TliConnectT*    tcon,
    long            cre_rel_id,
    uint            n_forkey,
    tb_sqlforkey_t* forkeys,
    long*           ar_ids)
{
    TliCursorT*     tcur;
    TliRetT         trc;
    tb_sqlforkey_t* fkey;
    dbe_db_t*       db;
    int i;
    int j;
    long            loc_id;
    long            loc_cre_rel_id;
    char*           loc_name;
    char*           loc_mysqlname;
    long            loc_len;
    char*           loc_reftable;
    char*           loc_refschema;
    char*           loc_refcatalog;
    long            loc_delrefact;
    long            loc_updrefact;
        
    ss_dassert(n_forkey > 0);
    ss_dassert(forkeys != NULL);
    ss_dassert(ar_ids != NULL);
        
    db = TliGetDb(tcon);
    
    ss_dprintf_3(("dd_insert_into_fkeys_unresolved\n"));

    for (i = 0; i < n_forkey; ++i) {
        fkey = &forkeys[i];
        
        if (!fkey->unresolved) {
            continue;
        }
        
        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_FORKEYS_UNRESOLVED);
        ss_assert(tcur != NULL);
        
        loc_id          = dbe_db_getnewkeyid_log(db);
        loc_cre_rel_id  = cre_rel_id;
        loc_name        = fkey->name ? SsMemStrdup(fkey->name) : NULL;
        loc_mysqlname   = fkey->mysqlname ? SsMemStrdup(fkey->mysqlname) : NULL;
        loc_len         = fkey->len;
        loc_reftable    = fkey->reftable ? SsMemStrdup(fkey->reftable) : NULL;
        loc_refschema   = fkey->refschema ? SsMemStrdup(fkey->refschema) : NULL;
        loc_refcatalog  = fkey->refcatalog ? SsMemStrdup(fkey->refcatalog) : NULL;
        loc_delrefact   = (long)fkey->delrefact;
        loc_updrefact   = (long)fkey->updrefact;
    
        ar_ids[i] = loc_id;
        
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_ID, &loc_id);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_CRE_REL_ID, &loc_cre_rel_id);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_FKEY_NAME, &loc_name);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_FKEY_EXTERNALNAME, &loc_mysqlname);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_FKEY_LEN, &loc_len);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_NAME, &loc_reftable);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_SCHEMA, &loc_refschema);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_CATALOG, &loc_refcatalog);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_DEL, &loc_delrefact);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_UPD, &loc_updrefact);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            
        trc = TliCursorInsert(tcur);
        ss_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));

        for (j = 0; j < fkey->len; ++j) {
            dd_insert_into_fkeyparts_unresolved(tcon, loc_id, fkey->fields[j], fkey->reffields[j]);
        }
        
        TliCursorFree(tcur);
        
        if (loc_name) {
            SsMemFree(loc_name);
        }
        if (loc_mysqlname) {
            SsMemFree(loc_mysqlname);
        }
        if (loc_reftable) {
            SsMemFree(loc_reftable);
        }
        if (loc_refschema) {
            SsMemFree(loc_refschema);
        }
        if (loc_refcatalog) {
            SsMemFree(loc_refcatalog);
        }
    }
}

/*#***********************************************************************\
 *
 *              dd_update_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_update_fkeys_unresolved(
    TliConnectT* tcon,
    long*        ar_ids,
    long         n_ids,
    long         cre_rel_id)
{
    TliCursorT*     tcur;
    TliRetT         trc;
    long            id;
    long            rel_id;
    /* tb_sqlforkey_t* fkey; */
    dbe_db_t*       db; 
    int i;
        
    ss_dassert(ar_ids != NULL);
    ss_dassert(n_ids > 0);
    ss_dassert(cre_rel_id > 0);
        
    db = TliGetDb(tcon);
    
    ss_dprintf_3(("dd_update_fkeys_unresolved\n"));
    
    tcur = TliCursorCreate(tcon,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_FORKEYS_UNRESOLVED);
    ss_assert(tcur != NULL);
    
    for (i = 0; i < n_ids; ++i) {
    
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_ID, &id);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
        trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_CRE_REL_ID, &rel_id);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_ID, TLI_RELOP_EQUAL, ar_ids[i]);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

        trc = TliCursorNext(tcur);
        if (trc == TLI_RC_SUCC) {
        
            rel_id = cre_rel_id;
        
            trc = TliCursorUpdate(tcur);
            ss_rc_assert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        }
    }

    TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_delete_from_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_delete_from_fkeys_unresolved(
        TliConnectT* tcon,
        long cre_rel_id)
{
    TliCursorT* tcur;
    TliRetT     trc;
    long        keyparts_id;

    ss_dprintf_3(("dd_delete_from_fkeys_unresolved:id=%ld\n", cre_rel_id));

    tcur = TliCursorCreate(tcon,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_FORKEYS_UNRESOLVED);
    ss_dassert(tcur != NULL);

    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_CRE_REL_ID, &cre_rel_id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_ID, &keyparts_id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    trc = TliCursorConstrLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_CRE_REL_ID, TLI_RELOP_EQUAL, cre_rel_id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    trc = TliCursorOpen(tcur);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
    
        trc = TliCursorDelete(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
        
        dd_delete_all_from_fkeyparts_unresolved(tcon, keyparts_id);
    }

    TliCursorFree(tcur);
}

/*#***********************************************************************\
 *
 *              dd_delete_all_from_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dd_drop_fkeys_unresolved(
        TliConnectT* tcon)
{
    TliCursorT* tcur;
    TliRetT     trc;

    ss_dprintf_3(("dd_delete_all_from_fkeys_unresolved\n"));

    tcur = TliCursorCreate(tcon,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_FORKEYS_UNRESOLVED);
    ss_dassert(tcur != NULL);

    trc = TliCursorOpen(tcur);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    
    while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
    
        trc = TliCursorDelete(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon), TliCursorErrorCode(tcur));
    }
    ss_dassert(trc == TLI_RC_END);

    TliCursorFree(tcur);
    
    dd_drop_fkeyparts_unresolved(tcon);
}

/*#***********************************************************************\
 *
 *              dd_get_from_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dd_get_from_fkeys_unresolved(
        TliConnectT*            tcon,
        char*                   ref_rel_name,
        char*                   ref_rel_schema,
        char*                   ref_rel_catalog,
        long*                   p_n_forkey,
        tb_sqlforkey_unres_t**  pp_forkeys)
{
    TliCursorT* tcur;
    TliRetT     trc;
    
    bool found = FALSE;
    char* name;
    char* mysql_name;
    long  id;
    long  cre_rel_id;
    long  len;
    long  ref_del;
    long  ref_upd;
    uint  n_forkey;
    tb_sqlforkey_unres_t* forkeys;
    tb_sqlforkey_unres_t* for_key_unres;
    tb_sqlforkey_t*       for_key;
    
    ss_dassert(ref_rel_name != NULL && *ref_rel_name != '\0');
    ss_dassert(p_n_forkey != NULL);
    ss_dassert(pp_forkeys != NULL);

    ss_dprintf_3(("dd_get_from_fkeys_unresolved:ref_rel_name=%s, ref_rel_schema=%s, ref_rel_catalog=%s\n",
                ref_rel_name,
                ref_rel_schema,
                ref_rel_catalog));
                
    tcur = TliCursorCreate(tcon,
                           RS_AVAL_DEFCATALOG,
                           RS_AVAL_SYSNAME,
                           RS_RELNAME_FORKEYS_UNRESOLVED);
    ss_dassert(tcur != NULL);

    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_ID, &id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_CRE_REL_ID, &cre_rel_id);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_FKEY_NAME, &name);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_FKEY_EXTERNALNAME, &mysql_name);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_FKEY_LEN, &len);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_NAME, &ref_rel_name);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_SCHEMA, &ref_rel_schema);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_CATALOG, &ref_rel_catalog);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_DEL, &ref_del);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    trc = TliCursorColLong(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_UPD, &ref_upd);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    
    trc = TliCursorConstrStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_NAME, TLI_RELOP_EQUAL, ref_rel_name);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    
    trc = TliCursorConstrStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_SCHEMA,
        ref_rel_schema == NULL ? TLI_RELOP_ISNULL : TLI_RELOP_EQUAL,
        ref_rel_schema == NULL ? (char*)"" : ref_rel_schema);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    
    /*
    trc = TliCursorConstrStr(tcur, RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_CATALOG,
        ref_rel_catalog == NULL ? TLI_RELOP_ISNULL : TLI_RELOP_EQUAL,
        ref_rel_catalog == NULL ? (char*)"" : ref_rel_catalog);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
    */
    
    trc = TliCursorOpen(tcur);
    ss_rc_dassert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

    n_forkey    = 0;
    *pp_forkeys = NULL;
    forkeys     = NULL;
    
    while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
        ++n_forkey;
        
        if (!forkeys) {
            forkeys = (tb_sqlforkey_unres_t*)SsMemAlloc(sizeof(tb_sqlforkey_unres_t));
        } else {
            forkeys = (tb_sqlforkey_unres_t*)SsMemRealloc(forkeys, n_forkey * sizeof(tb_sqlforkey_unres_t));
        }
        
        for_key_unres = &(forkeys[n_forkey - 1]);
        memset(for_key_unres, 0, sizeof(tb_sqlforkey_unres_t));
        
        for_key = (tb_sqlforkey_t*)for_key_unres;
        
        for_key->name       = name ? SsMemStrdup(name) : NULL;
        for_key->mysqlname  = mysql_name ? SsMemStrdup(mysql_name) : NULL;
        for_key->reftable   = ref_rel_name ? SsMemStrdup(ref_rel_name) : NULL;
        for_key->refschema  = ref_rel_schema ? SsMemStrdup(ref_rel_schema) : NULL;
        for_key->refcatalog = ref_rel_catalog ? SsMemStrdup(ref_rel_catalog) : NULL;
        for_key->delrefact  = (sqlrefact_t)ref_del;
        for_key->updrefact  = (sqlrefact_t)ref_upd;
        for_key->len        = len;
        
        for_key->fields    = (uint*)SsMemAlloc(sizeof(uint) * for_key->len);
        for_key->reffields = (char**)SsMemAlloc(sizeof(char*) * for_key->len);
        
        for_key->unresolved = FALSE;
        
        for_key_unres->cre_rel_id = cre_rel_id;
        
        dd_get_from_fkeyparts_unresolved(tcon, id, for_key->len, (long*)for_key->fields, for_key->reffields);
        
        found = TRUE;
    }
    ss_dassert(trc == TLI_RC_END);
    
    *p_n_forkey = n_forkey;
    *pp_forkeys = forkeys;

    TliCursorFree(tcur);
    
    return found;
}

/*#***********************************************************************\
 *
 *              tb_dd_create_fkeys_unresolved_info
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_dd_create_fkeys_unresolved_info(
        rs_sysinfo_t*   cd,
        tb_trans_t*     trans,
        rs_relh_t*      cre_relh,
        uint            n_forkey,
        tb_sqlforkey_t* forkeys,
        long*           ar_ids)
{
    TliConnectT* tcon;
    TliRetT      trc;
    
    ss_dassert(cd != NULL);
    ss_dassert(trans != NULL);
    ss_dassert(n_forkey > 0);
    ss_dassert(forkeys != NULL);
    ss_dassert(ar_ids != NULL);
    
    ss_dprintf_1(("tb_dd_create_fkeys_unresolved_info"));
    
    tcon = TliConnectInitByTrans(cd, trans);
    ss_dassert(tcon != NULL);
    
    trc = TliBeginTransact(tcon);
    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
    
    dd_insert_into_fkeys_unresolved(tcon, rs_relh_relid(cd, cre_relh), n_forkey, forkeys, ar_ids);
    
    trc = TliCommit(tcon);
    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
    
    TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_update_fkeys_unresolved_info
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_dd_update_fkeys_unresolved_info(
        rs_sysinfo_t* cd,
        tb_trans_t*   trans,
        long*         ar_ids,
        long          n_ids,
        long          cre_rel_id)
{
    TliConnectT* tcon;
    TliRetT      trc;
    
    ss_dassert(cd != NULL);
    ss_dassert(trans != NULL);
    ss_dassert(ar_ids != NULL);
    ss_dassert(n_ids > 0);
    ss_dassert(cre_rel_id > 0);
    
    ss_dprintf_1(("tb_dd_update_fkeys_unresolved_info"));
    
    tcon = TliConnectInitByTrans(cd, trans);
    ss_dassert(tcon != NULL);
    
    trc = TliBeginTransact(tcon);
    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
    
    dd_update_fkeys_unresolved(tcon, ar_ids, n_ids, cre_rel_id);
    
    trc = TliCommit(tcon);
    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
    
    TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_get_from_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_dd_get_from_fkeys_unresolved(
        rs_sysinfo_t*           cd,
        tb_trans_t*             trans,
        char*                   ref_rel_name,
        char*                   ref_rel_schema,
        char*                   ref_rel_catalog,
        long                    cre_rel_id,         /* ignore if 0 */
        long*                   p_n_forkey,
        tb_sqlforkey_unres_t**  pp_forkeys)
{
    TliConnectT* tcon;
    bool         bret = FALSE;
    
    ss_dassert(cd != NULL);
    ss_dassert(trans != NULL);
    ss_dassert(ref_rel_name != NULL);
    ss_dassert(p_n_forkey != NULL);
    ss_dassert(pp_forkeys != NULL);
    
    ss_dprintf_1(("tb_dd_get_from_fkeys_unresolved"));
    
    tcon = TliConnectInitByTrans(cd, trans);
    ss_dassert(tcon != NULL);
    
    bret = dd_get_from_fkeys_unresolved(tcon, ref_rel_name, ref_rel_schema, ref_rel_catalog,
                                        p_n_forkey, pp_forkeys);
    
    TliConnectDone(tcon);
    
    return bret;
}

/*#***********************************************************************\
 *
 *              tb_dd_delete_from_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_dd_delete_from_fkeys_unresolved(
        rs_sysinfo_t* cd,
        tb_trans_t*   trans,
        long cre_rel_id)
{
    TliConnectT* tcon;
    TliRetT      trc;
    
    ss_dassert(cd != NULL);
    ss_dassert(trans != NULL);
    ss_dassert(cre_rel_id > 0);
    
    ss_dprintf_1(("tb_dd_delete_from_fkeys_unresolved"));
    
    tcon = TliConnectInitByTrans(cd, trans);
    ss_dassert(tcon != NULL);
    
    trc = TliBeginTransact(tcon);
    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
    
    dd_delete_from_fkeys_unresolved(tcon, cre_rel_id);
    
    trc = TliCommit(tcon);
    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
    
    TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_drop_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_dd_drop_fkeys_unresolved(
        tb_database_t* tdb)
{
    TliConnectT* tcon;
    TliRetT      trc;
    
    ss_dassert(tdb != NULL);
    
    ss_dprintf_1(("tb_dd_delete_all_from_fkeys_unresolved"));
    
    tcon = TliConnectInitByTabDb(tdb);
    ss_dassert(tcon != NULL);
    
    trc = TliBeginTransact(tcon);
    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
    
    dd_drop_fkeys_unresolved(tcon);
    
    trc = TliCommit(tcon);
    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
    
    TliConnectDone(tcon);
}

/*#***********************************************************************\
 *
 *              tb_dd_free_fkeys_unresolved
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_dd_free_fkeys_unresolved(
        long n_forkeys,
        tb_sqlforkey_unres_t* forkeys_unres)
{
    long i;
    long j;
    tb_sqlforkey_t* forkey;

    ss_dprintf_1(("tb_dd_free_fkeys_unresolved"));

    if (n_forkeys <= 0 || forkeys_unres == NULL) {
        return;
    }
    
    for (i = 0; i < n_forkeys; ++i) {
    
        forkey = (tb_sqlforkey_t*)&forkeys_unres[i];
    
        for (j = 0; j < forkey->len; ++j) {
            if (forkey->reffields[j]) {
                SsMemFree(forkey->reffields[j]);
            }
        }
        
        if (forkey->reffields) {
            SsMemFree(forkey->reffields);
        }
        
        if (forkey->fields) {
            SsMemFree(forkey->fields);
        }
        
        if (forkey->name) {
            SsMemFree(forkey->name);
        }

        if (forkey->mysqlname) {
            SsMemFree(forkey->mysqlname);
        }

        if (forkey->reftable) {
            SsMemFree(forkey->reftable);
        }

        if (forkey->refschema) {
            SsMemFree(forkey->refschema);
        }

        if (forkey->refcatalog) {
            SsMemFree(forkey->refcatalog);
        }
    }
    
    SsMemFree(forkeys_unres);
}

#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */
#endif /* REFERENTIAL_INTEGRITY */

