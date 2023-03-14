/*************************************************************************\
**  source       * rs0sdefs.h
**  directory    * res
**  description  * SOLID system definitions
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


#ifndef RS0SDEFS_H
#define RS0SDEFS_H

#include <ssc.h>
#include <sslimits.h>
#include <su0types.h>

#define RS_RELNAME_TABLES                   "SYS_TABLES"
#define RS_RELNAME_COLUMNS                  "SYS_COLUMNS"
#define RS_RELNAME_COLUMNS_AUX              "SYS_COLUMNS_AUX"
#define RS_RELNAME_USERS                    "SYS_USERS"
#define RS_RELNAME_UROLE                    "SYS_UROLE"
#define RS_RELNAME_RELAUTH                  "SYS_RELAUTH"
#define RS_RELNAME_ATTAUTH                  "SYS_ATTAUTH"
#define RS_RELNAME_VIEWS                    "SYS_VIEWS"     
#define RS_RELNAME_KEYPARTS                 "SYS_KEYPARTS" 
#define RS_RELNAME_KEYPARTS_AUX             "SYS_KEYPARTS_AUX"
#define RS_RELNAME_KEYS                     "SYS_KEYS"     
#define RS_RELNAME_CARDINAL                 "SYS_CARDINAL"
#define RS_RELNAME_INFO                     "SYS_INFO"
#define RS_RELNAME_SYNONYM                  "SYS_SYNONYM"
#define RS_RELNAME_TYPES                    "SYS_TYPES"
#define RS_RELNAME_SQL_LANG                 "SQL_LANGUAGES"
#define RS_RELNAME_PROCEDURES               "SYS_PROCEDURES"
#define RS_RELNAME_TRIGGERS                 "SYS_TRIGGERS"
#define RS_RELNAME_EVENTS                   "SYS_EVENTS"
#define RS_RELNAME_TABLEMODES               "SYS_TABLEMODES"
#define RS_RELNAME_FORKEYS                  "SYS_FORKEYS"
#define RS_RELNAME_FORKEYPARTS              "SYS_FORKEYPARTS"
#define RS_RELNAME_SEQUENCES                "SYS_SEQUENCES"
#define RS_RELNAME_CATALOGS                 "SYS_CATALOGS"
#define RS_RELNAME_SCHEMAS                  "SYS_SCHEMAS"
#define RS_RELNAME_PROCEDURE_COL_INFO       "SYS_PROCEDURE_COLUMNS"
#define RS_RELNAME_SYSPROPERTIES            "SYS_PROPERTIES"
#define RS_RELNAME_HOTSTANDBY               "SYS_HOTSTANDBY"
#define RS_RELNAME_BLOBS                    "SYS_BLOBS"

#ifdef SS_COLLATION
#define RS_RELNAME_COLLATIONS               "SYS_COLLATIONS"
#endif /* SS_COLLATION */

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
#define RS_RELNAME_FORKEYS_UNRESOLVED       "SYS_FORKEYS_UNRESOLVED"
#define RS_RELNAME_FORKEYPARTS_UNRESOLVED   "SYS_FORKEYPARTS_UNRESOLVED"
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

#ifdef SS_SYNC

/*
 * SYNC SEQUENCES
 */

#define SNC_SCHEMA              "_SYSTEM"
#define SNC_MSGID               "SYS_SYNC_INTEGERID"
#define SNC_VERSID              "SYS_SYNC_BINARYID"


/*
 * Sync. bookmarks
 */
#define RS_RELNAME_SYNC_BOOKMARKS           "SYS_SYNC_BOOKMARKS"
#define RS_ANAME_SYNC_BOOKMARKS_ID          "BM_ID"
#define RS_ANAME_SYNC_BOOKMARKS_CATALOG     "BM_CATALOG"
#define RS_ANAME_SYNC_BOOKMARKS_NAME        "BM_NAME"
#define RS_ANAME_SYNC_BOOKMARKS_VERSION     "BM_VERSION"
#define RS_ANAME_SYNC_BOOKMARKS_CREATOR     "BM_CREATOR"
#define RS_ANAME_SYNC_BOOKMARKS_CREATIME    "BM_CREATIME"

/*
 * User maps
 */
#define RS_RELNAME_SYNC_USERMAPS    "SYS_SYNC_USERMAPS"

#define RS_ANAME_SYNC_USERMAPS_REPLICA_UID              "REPLICA_UID"
#define RS_ANAME_SYNC_USERMAPS_REPLICA_USERNAME         "REPLICA_USERNAME"
#define RS_ANAME_SYNC_USERMAPS_MASTER_ID                "MASTER_ID"
#define RS_ANAME_SYNC_USERMAPS_MASTER_USERNAME          "MASTER_USERNAME"
#define RS_ANAME_SYNC_USERMAPS_PWD                      "PASSW"


#define RS_RELNAME_SYNC_USERS           "SYS_SYNC_USERS"
#define RS_ANAME_SYNC_USERS_MASTER_ID   "MASTER_ID"
/*
 * SYNC-VERSION ATTRIBUTE NAME
 */

/*
 * SUBSCRIBE REQUESTS ON MASTER
 */

#define RS_RELNAME_SYNC_MASTER_MSR  "SYS_SYNC_MASTER_SUBSC_REQ"
#define RS_ANAME_MSR_REPLICAID      "REPLICA_ID"
#define RS_ANAME_MSR_MSG_ID         "MSG_ID"
#define RS_ANAME_MSR_ORD_ID         "ORD_ID"
#define RS_ANAME_MSR_TRX_ID         "TRX_ID"
#define RS_ANAME_MSR_STMT_ID        "STMT_ID"
#define RS_ANAME_MSR_REQUESTID      "REQUEST_ID"
#define RS_ANAME_MSR_PUBLID         "PUBL_ID"
#define RS_ANAME_MSR_PUBLCREATIME   "PUBL_CREATIME"
#define RS_ANAME_MSR_VERSID         "VERSION"
#define RS_ANAME_MSR_FULLSUBSC      "FULLSUBSC"
#define RS_ANAME_MSR_REPLICA_VERSID "REPLICA_VERSION"
#define RS_ANAME_MSR_BOOKMARKID     "BOOKMARK_ID"

/*
 * PUBLICATION VERSIONS
 */

#define RS_RELNAME_SYNC_REPLICA_VERS  "SYS_SYNC_REPLICA_VERSIONS"
#define RS_RELNAME_SYNC_MASTER_VERS   "SYS_SYNC_MASTER_VERSIONS"
#define RS_ANAME_VERS_TIME            "VERS_TIME"
#define RS_ANAME_VERS_PUBLID          "PUBL_ID"
#define RS_ANAME_VERS_TABNAME         "TABNAME"
#define RS_ANAME_VERS_TABSCHEMA       "TABSCHEMA"
#define RS_ANAME_VERS_TABCATALOG      "TABCATALOG"
#define RS_ANAME_VERS_VERSION         "VERSION"
#define RS_ANAME_VERS_LOCAL_VERSION   "LOCAL_VERSION"
#define RS_ANAME_VERS_PARAM           "PARAM"
#define RS_ANAME_VERS_PARAM_CRC       "PARAM_CRC"
#define RS_ANAME_VERS_REQUESTID       "REQUEST_ID"
#define RS_ANAME_VERS_MASTERID        "MASTER_ID"
#define RS_ANAME_VERS_REPLICAID       "REPLICA_ID"
#define RS_ANAME_VERS_PUBLNAME        "PUBL_NAME"
#define RS_ANAME_VERS_REPLYID         "REPLY_ID"
#define RS_ANAME_VERS_BOOKMARKID      "BOOKMARK_ID"

/*
 * SAVED STATEMENTS
 */

#define RS_RELNAME_SYNC_SAVED_STMTS         "SYS_SYNC_SAVED_STMTS"
#define RS_RELNAME_SYNC_SAVED_BLOB_ARGS     "SYS_SYNC_SAVED_BLOB_ARGS"

#define RS_ANAME_SYNC_SAVED_STMT_MASTER     "MASTER"
#define RS_ANAME_SYNC_SAVED_STMT_TRXID      "TRX_ID"
#define RS_ANAME_SYNC_SAVED_STMT_ID         "ID"
#define RS_ANAME_SYNC_SAVED_STMT_CLASS      "CLASS"
#define RS_ANAME_SYNC_SAVED_STMT_STRING     "STRING"
#define RS_ANAME_SYNC_SAVED_STMT_ARGCOUNT   "ARG_COUNT"
#define RS_ANAME_SYNC_SAVED_STMT_ARGTYPES   "ARG_TYPES"
#define RS_ANAME_SYNC_SAVED_STMT_ARGVALUES  "ARG_VALUES"
#define RS_ANAME_SYNC_SAVED_STMT_FLAGS      "FLAGS"
#define RS_ANAME_SYNC_SAVED_STMT_USERID     "USER_ID"
#define RS_ANAME_SYNC_SAVED_STMT_REQUESTID  "REQUEST_ID"

#define RS_RELNAME_SYNC_RECEIVED_STMTS      "SYS_SYNC_RECEIVED_STMTS"
#define RS_ANAME_SYNC_SAVED_STMT_REPLICA    "REPLICA"
#define RS_ANAME_SYNC_SAVED_STMT_MSG        "MSG"
#define RS_ANAME_SYNC_SAVED_STMT_ORDID      "ORD_ID"

#define RS_RELNAME_SYNC_RECEIVED_BLOB_ARGS  "SYS_SYNC_RECEIVED_BLOB_ARGS"
#define RS_ANAME_SYNC_SAVED_STMT_ARGNO      "ARGNO"
#define RS_ANAME_SYNC_SAVED_STMT_BLOB_VALUE "ARG_VALUE"

/* tables */
#define RS_RELNAME_SYNC_REPLICA_STORED_BLOB_REFS   "SYS_SYNC_REPLICA_STORED_BLOB_REFS"
#define RS_RELNAME_SYNC_REPLICA_RECEIVED_BLOB_REFS "SYS_SYNC_REPLICA_RECEIVED_BLOB_REFS"
#define RS_RELNAME_SYNC_MASTER_STORED_BLOB_REFS    "SYS_SYNC_MASTER_STORED_BLOB_REFS"
#define RS_RELNAME_SYNC_MASTER_RECEIVED_BLOB_REFS  "SYS_SYNC_MASTER_RECEIVED_BLOB_REFS"

/* attributes */
#define RS_ANAME_BLOBREF_MASTERID               "MASTER_ID"
#define RS_ANAME_BLOBREF_REPLICAID              "REPLICA_ID"
#define RS_ANAME_BLOBREF_MSGID                  "MSG_ID"
#define RS_ANAME_BLOBREF_BLOBNUM                "BLOB_NUM"
#define RS_ANAME_BLOBREF_BLOB                   "DATA"


/*
 * PUBLICATIONS
 */

#define RS_RELNAME_PUBL                     "SYS_PUBLICATIONS"
#define RS_RELNAME_PUBL_ARGS                "SYS_PUBLICATION_ARGS"
#define RS_RELNAME_PUBL_STMTS               "SYS_PUBLICATION_STMTS"
#define RS_RELNAME_PUBL_STMTARGS            "SYS_PUBLICATION_STMTARGS"

#define RS_RELNAME_PUBL_REPL                "SYS_PUBLICATIONS_REPLICA"
#define RS_RELNAME_PUBL_REPL_ARGS           "SYS_PUBLICATION_REPLICA_ARGS"
#define RS_RELNAME_PUBL_REPL_STMTS          "SYS_PUBLICATION_REPLICA_STMTS"
#define RS_RELNAME_PUBL_REPL_STMTARGS       "SYS_PUBLICATION_REPLICA_STMTARGS"

#define RS_ANAME_PUBL_MASTERID              "MASTER_ID"
#define RS_ANAME_PUBL_ID                    "ID"
#define RS_ANAME_PUBL_NAME                  "NAME"
#define RS_ANAME_PUBL_CREATOR               "CREATOR"
#define RS_ANAME_PUBL_CREATIME              "CREATIME"
#define RS_ANAME_PUBL_ARGCOUNT              "ARGCOUNT"
#define RS_ANAME_PUBL_STMTCOUNT             "STMTCOUNT"
#define RS_ANAME_PUBL_TIMEOUT               "TIMEOUT"
#define RS_ANAME_PUBL_TEXT                  "TEXT"
#define RS_ANAME_PUBL_CATALOG               "PUBL_CATALOG"

#define RS_ANAME_PUBL_MASTER_CREATIME       "MASTER_CREATIME"

#define RS_ANAME_PUBL_ARG_MASTERID          "MASTER_ID"
#define RS_ANAME_PUBL_ARG_PUBL              "PUBL_ID"
#define RS_ANAME_PUBL_ARG_NUMBER            "ARG_NUMBER"
#define RS_ANAME_PUBL_ARG_NAME              "NAME"
#define RS_ANAME_PUBL_ARG_TYPE              "TYPE"
#define RS_ANAME_PUBL_ARG_LEN_OR_PREC       "LENGTH_OR_PRECISION"
#define RS_ANAME_PUBL_ARG_SCALE             "SCALE"

#define RS_ANAME_PUBL_STMT_MASTERID         "MASTER_ID"
#define RS_ANAME_PUBL_STMT_PUBL             "PUBL_ID"
#define RS_ANAME_PUBL_STMT_NUMBER           "STMT_NUMBER"
#define RS_ANAME_PUBL_STMT_MASTERCATALOG    "MASTER_CATALOG"
#define RS_ANAME_PUBL_STMT_MASTERSCHEMA     "MASTER_SCHEMA"
#define RS_ANAME_PUBL_STMT_MASTERTABLE      "MASTER_TABLE"
#define RS_ANAME_PUBL_STMT_REPLICACATALOG   "REPLICA_CATALOG"
#define RS_ANAME_PUBL_STMT_REPLICASCHEMA    "REPLICA_SCHEMA"
#define RS_ANAME_PUBL_STMT_REPLICATABLE     "REPLICA_TABLE"
#define RS_ANAME_PUBL_STMT_TABLEALIAS       "TABLE_ALIAS"
#define RS_ANAME_PUBL_STMT_MASTERSELECTSTR  "MASTER_SELECT_STR"
#define RS_ANAME_PUBL_STMT_REPLICASELECTSTR "REPLICA_SELECT_STR"
#define RS_ANAME_PUBL_STMT_MASTERFROMSTR    "MASTER_FROM_STR"
#define RS_ANAME_PUBL_STMT_REPLICAFROMSTR   "REPLICA_FROM_STR"
#define RS_ANAME_PUBL_STMT_WHERESTR         "WHERE_STR"
#define RS_ANAME_PUBL_STMT_DELETEFLAGSTR    "DELETEFLAG_STR"
#define RS_ANAME_PUBL_STMT_LEVEL            "LEVEL"

#define RS_ANAME_PUBL_STMTARG_MASTERID      "MASTER_ID"
#define RS_ANAME_PUBL_STMTARG_PUBL          "PUBL_ID"
#define RS_ANAME_PUBL_STMTARG_NUMBER        "STMT_NUMBER"
#define RS_ANAME_PUBL_STMTARG_STMTARGNUMBER "STMT_ARG_NUMBER"
#define RS_ANAME_PUBL_STMTARG_PUBLARGNUMBER "PUBL_ARG_NUMBER"

/*
 *  Masters (seen by replica)
 */
#define RS_RELNAME_SYNC_MASTERS         "SYS_SYNC_MASTERS"

#define RS_ANAME_SYNC_MASTER_NAME           "NAME"
#define RS_ANAME_SYNC_MASTER_ID             "ID"
#define RS_ANAME_SYNC_MASTER_REMOTENAME     "REMOTE_NAME"
#define RS_ANAME_SYNC_MASTER_REPLICANAME    "REPLICA_NAME"
#define RS_ANAME_SYNC_MASTER_REPLICAID      "REPLICA_ID"
#define RS_ANAME_SYNC_MASTER_CONNECT        "CONNECT"
#define RS_ANAME_SYNC_MASTER_CREATOR        "CREATOR"
#define RS_ANAME_SYNC_MASTER_REPLICACATALOG "REPLICA_CATALOG"


/*
 *  Replicas (seen by master)
 */
#define RS_RELNAME_SYNC_REPLICAS          "SYS_SYNC_REPLICAS"

#define RS_ANAME_SYNC_REPLICA_NAME           "NAME"
#define RS_ANAME_SYNC_REPLICA_ID             "ID"
#define RS_ANAME_SYNC_REPLICA_MASTERNAME     "MASTER_NAME"
#define RS_ANAME_SYNC_REPLICA_CONNECT        "CONNECT"
#define RS_ANAME_SYNC_REPLICA_MASTERCATALOG  "MASTER_CATALOG"

/*
 * Messages.
 */
#define RS_RELNAME_SYNC_REPLICA_STORED_MSGS         "SYS_SYNC_REPLICA_STORED_MSGS"
#define RS_RELNAME_SYNC_REPLICA_STORED_MSGPARTS     "SYS_SYNC_REPLICA_STORED_MSGPARTS"

#define RS_RELNAME_SYNC_MASTER_RECEIVED_MSGS        "SYS_SYNC_MASTER_RECEIVED_MSGS"
#define RS_RELNAME_SYNC_MASTER_RECEIVED_MSGPARTS    "SYS_SYNC_MASTER_RECEIVED_MSGPARTS"

#define RS_RELNAME_SYNC_MASTER_STORED_MSGS          "SYS_SYNC_MASTER_STORED_MSGS"
#define RS_RELNAME_SYNC_MASTER_STORED_MSGPARTS      "SYS_SYNC_MASTER_STORED_MSGPARTS"

#define RS_RELNAME_SYNC_REPLICA_RECEIVED_MSGS       "SYS_SYNC_REPLICA_RECEIVED_MSGS"
#define RS_RELNAME_SYNC_REPLICA_RECEIVED_MSGPARTS   "SYS_SYNC_REPLICA_RECEIVED_MSGPARTS"

/* these 2 column names are used in both messages and message parts
 * system tables
 */
#define RS_ANAME_SYNC_MSG_MASTERID                  "MASTER_ID"
#define RS_ANAME_SYNC_MSG_REPLICAID                 "REPLICA_ID"

#define RS_ANAME_SYNC_MSG_ID                        "MSG_ID"
#define RS_ANAME_SYNC_MSG_TIME                      "CREATIME"
#define RS_ANAME_SYNC_MSG_CREATOR                   "CREATOR"

#define RS_ANAME_SYNC_MSGPARTS_MSGID                "MSG_ID"
#define RS_ANAME_SYNC_MSGPARTS_PARTNO               "PART_NUMBER"
#define RS_ANAME_SYNC_MSGPARTS_LENGTH               "DATA_LENGTH"
#define RS_ANAME_SYNC_MSGPARTS_DATA                 "DATA"
#define RS_ANAME_SYNC_MSGPARTS_ORDERID              "ORDER_ID"          /* only in master store */
#define RS_ANAME_SYNC_MSGPARTS_RESULTSETID          "RESULT_SET_ID"     /* only in master store */
#define RS_ANAME_SYNC_MSGPARTS_RESULTSETTYPE        "RESULT_SET_TYPE"   /* only in master store */


#define RS_RELNAME_MASTER_MSGINFO               "SYS_SYNC_MASTER_MSGINFO"
#define RS_RELNAME_REPLICA_MSGINFO              "SYS_SYNC_REPLICA_MSGINFO"

#define RS_ANAME_MSGINFO_STATE                  "STATE"
#define RS_ANAME_MSGINFO_MASTERID               "MASTER_ID"
#define RS_ANAME_MSGINFO_REPLICAID              "REPLICA_ID"
#define RS_ANAME_MSGINFO_CREATE_UID             "CREATE_UID"
#define RS_ANAME_MSGINFO_FORWARD_UID            "FORWARD_UID"
#define RS_ANAME_MSGINFO_MSGID                  "MSG_ID"
#define RS_ANAME_MSGINFO_MSGNAME                "MSG_NAME"
#define RS_ANAME_MSGINFO_MSGTIME                "MSG_TIME"
#define RS_ANAME_MSGINFO_BYTECOUNT              "MSG_BYTE_COUNT"
#define RS_ANAME_MSGINFO_ORDIDCOUNT             "ORD_ID_COUNT"
#define RS_ANAME_MSGINFO_ORDID                  "ORD_ID"
#define RS_ANAME_MSGINFO_TRXID                  "TRX_ID"
#define RS_ANAME_MSGINFO_STMTID                 "STMT_ID"
#define RS_ANAME_MSGINFO_ERRORCODE              "ERROR_CODE"
#define RS_ANAME_MSGINFO_ERRORTEXT              "ERROR_TEXT"
#define RS_ANAME_MSGINFO_MASTERNAME             "MASTER_NAME"
#define RS_ANAME_MSGINFO_FLAGS                  "FLAGS"
#define RS_ANAME_MSGINFO_FAILED_MSGID           "FAILED_MSG_ID"
#define RS_ANAME_MSGINFO_MSG_VERSION            "MSG_VERSION"

/*
 * Bulletin board
 */

#define RS_RELNAME_BULLETIN_BOARD       "SYS_BULLETIN_BOARD"
#define RS_ANAME_BULLETIN_BOARD_NAME    "PARAM_NAME"
#define RS_ANAME_BULLETIN_BOARD_VALUE   "PARAM_VALUE"
#define RS_ANAME_BULLETIN_BOARD_CATALOG "PARAM_CATALOG"

/*
 * History columns
 */

#define RS_RELNAME_HISTORYCOLS          "SYS_SYNC_HISTORY_COLUMNS"
#define RS_ANAME_HISTORYCOLS_RELID      "REL_ID"
#define RS_ANAME_HISTORYCOLS_COLNUM     "COLUMN_NUMBER"

/*
 * Transaction properties
 */

#define RS_RELNAME_TRXPROPERTIES        "SYS_SYNC_TRX_PROPERTIES"
#define RS_ANAME_TRXPROPERTIES_TRXID    "TRX_ID"
#define RS_ANAME_TRXPROPERTIES_NAME     "NAME"
#define RS_ANAME_TRXPROPERTIES_VALUESTR "VALUE_STR"

#define RS_RELNAME_SYNCINFO             "SYS_SYNC_INFO"
#define RS_ANAME_SYNCINFO_NODENAME      "NODE_NAME"
#define RS_ANAME_SYNCINFO_NODECATALOG   "NODE_CATALOG"
#define RS_ANAME_SYNCINFO_ISMASTER      "IS_MASTER"
#define RS_ANAME_SYNCINFO_ISREPLICA     "IS_REPLICA"
#define RS_ANAME_SYNCINFO_CREATIME      "CREATIME"
#define RS_ANAME_SYNCINFO_CREATOR       "CREATOR"

#define RS_RELNAME_SYNC_REPLICA_PROPERTIES  "SYS_SYNC_REPLICA_PROPERTIES"
#define RS_ANAME_REPLICA_PROPERTY_ID        "ID"
#define RS_ANAME_REPLICA_PROPERTY_NAME      "NAME"
#define RS_ANAME_REPLICA_PROPERTY_VALUE     "VALUE"

#define RS_RELNAME_DL_CONFIG            "SYS_DL_REPLICA_CONFIG"
#define RS_RELNAME_DL_DEFAULT           "SYS_DL_REPLICA_DEFAULT"

#endif /* SS_SYNC */

#define RS_RELNAME_V_USERS         "USERS"
#define RS_RELNAME_V_TABLES        "TABLES"
#define RS_RELNAME_V_COLUMNS       "COLUMNS"
#define RS_RELNAME_V_SERVER_INFO   "SERVER_INFO"

#define RS_ANAME_TABLES_ID                     "ID"
#define RS_ANAME_TABLES_NAME                   "TABLE_NAME"
#define RS_ANAME_TABLES_TYPE                   "TABLE_TYPE"
#define RS_ANAME_TABLES_SCHEMA                 "TABLE_SCHEMA"
#define RS_ANAME_TABLES_CATALOG                "TABLE_CATALOG"
#define RS_ANAME_TABLES_CREATIME               "CREATIME"
#define RS_ANAME_TABLES_CHECK                  "CHECKSTRING"
#define RS_ANAME_TABLES_REMARKS                "REMARKS"

#define RS_ANAME_COLUMNS_ID                    "ID"
#define RS_ANAME_COLUMNS_REL_ID                "REL_ID"
#define RS_ANAME_COLUMNS_COLUMN_NAME           "COLUMN_NAME"
#define RS_ANAME_COLUMNS_COLUMN_NUMBER         "COLUMN_NUMBER"
#define RS_ANAME_COLUMNS_DATA_TYPE             "DATA_TYPE"
#define RS_ANAME_COLUMNS_SQLDTYPE_NO           "SQL_DATA_TYPE_NUM"
#define RS_ANAME_COLUMNS_DATA_TYPE_NO          "DATA_TYPE_NUMBER"
#define RS_ANAME_COLUMNS_CHAR_MAX_LEN          "CHAR_MAX_LENGTH"
#define RS_ANAME_COLUMNS_NUM_PRECISION         "NUMERIC_PRECISION"
#define RS_ANAME_COLUMNS_NUM_PREC_RDX          "NUMERIC_PREC_RADIX"
#define RS_ANAME_COLUMNS_NUM_SCALE             "NUMERIC_SCALE"
#define RS_ANAME_COLUMNS_NULLABLE              "NULLABLE"
#define RS_ANAME_COLUMNS_NULLABLE_ODBC         "NULLABLE_ODBC"
#define RS_ANAME_COLUMNS_FORMAT                "FORMAT"
#define RS_ANAME_COLUMNS_DEFAULT_VAL           "DEFAULT_VAL"
#define RS_ANAME_COLUMNS_ATTR_TYPE             "ATTR_TYPE"
#define RS_ANAME_COLUMNS_REMARKS               "REMARKS"

#define RS_ANAME_COLUMNS_AUX_ID                 "ID"
#define RS_ANAME_COLUMNS_AUX_ORIGINAL_DEFAULT  "ORIGINAL_DEFAULT"
#define RS_ANAME_COLUMNS_AUX_AUTO_INC_SEQ_ID   "AUTO_INC_SEQ_ID"
#define RS_ANAME_COLUMNS_AUX_EXTERNAL_DATA_TYPE  "EXTERNAL_DATA_TYPE"
#ifdef SS_COLLATION
#define RS_ANAME_COLUMNS_AUX_EXTERNAL_COLLATION     "EXTERNAL_COLLATION"
#endif /* SS_COLLATION */

#define RS_ANAME_USERS_ID                      "ID"
#define RS_ANAME_USERS_NAME                    "NAME"
#define RS_ANAME_USERS_TYPE                    "TYPE"
#define RS_ANAME_USERS_PRIV                    "PRIV"
#define RS_ANAME_USERS_PASSW                   "PASSW"
#define RS_ANAME_USERS_PRIORITY                "PRIORITY"
#define RS_ANAME_USERS_PRIVATE                 "PRIVATE"
#define RS_ANAME_USERS_LOGIN_CATALOG           "LOGIN_CATALOG"

#define RS_ANAME_UROLE_U_ID                    "U_ID"
#define RS_ANAME_UROLE_R_ID                    "R_ID"

#define RS_ANAME_RELAUTH_REL_ID                "REL_ID"
#define RS_ANAME_RELAUTH_UR_ID                 "UR_ID"
#define RS_ANAME_RELAUTH_PRIV                  "PRIV"
#define RS_ANAME_RELAUTH_GRANT_ID              "GRANT_ID"
#define RS_ANAME_RELAUTH_GRANT_TIM             "GRANT_TIM"
#define RS_ANAME_RELAUTH_GRANT_OPT             "GRANT_OPT"

#define RS_ANAME_ATTAUTH_REL_ID                "REL_ID"
#define RS_ANAME_ATTAUTH_ATTR_ID               "ATTR_ID"
#define RS_ANAME_ATTAUTH_UR_ID                 "UR_ID"
#define RS_ANAME_ATTAUTH_PRIV                  "PRIV"
#define RS_ANAME_ATTAUTH_GRANT_ID              "GRANT_ID"
#define RS_ANAME_ATTAUTH_GRANT_TIM             "GRANT_TIM"

#define RS_ANAME_VIEWS_V_ID                    "V_ID"
#define RS_ANAME_VIEWS_TEXT                    "TEXT"
#define RS_ANAME_VIEWS_CHECK                   "CHECKSTRING"
#define RS_ANAME_VIEWS_REMARKS                 "REMARKS"

#define RS_ANAME_KEYPARTS_ID                   "ID"
#define RS_ANAME_KEYPARTS_REL_ID               "REL_ID"
#define RS_ANAME_KEYPARTS_KEYP_NO              "KEYP_NO"
#define RS_ANAME_KEYPARTS_ATTR_ID              "ATTR_ID"
#define RS_ANAME_KEYPARTS_ATTR_NO              "ATTR_NO"
#define RS_ANAME_KEYPARTS_ATTR_TYPE            "ATTR_TYPE"
#define RS_ANAME_KEYPARTS_CONST_VALUE          "CONST_VALUE"
#define RS_ANAME_KEYPARTS_ASCENDING            "ASCENDING"

#define RS_ANAME_KEYPARTS_AUX_ID               "ID"
#define RS_ANAME_KEYPARTS_AUX_KEYP_NO          "KEYP_NO"
#define RS_ANAME_KEYPARTS_AUX_PREFIX_LENGTH    "PREFIX_LENGTH"

#define RS_ANAME_KEYS_ID                       "ID"
#define RS_ANAME_KEYS_REL_ID                   "REL_ID"
#define RS_ANAME_KEYS_KEY_NAME                 "KEY_NAME"
#define RS_ANAME_KEYS_KEY_UNIQUE               "KEY_UNIQUE"
#define RS_ANAME_KEYS_KEY_NONUNQ_ODBC          "KEY_NONUNIQUE_ODBC"
#define RS_ANAME_KEYS_CLUSTERING               "KEY_CLUSTERING"
#define RS_ANAME_KEYS_PRIMARY                  "KEY_PRIMARY"
#define RS_ANAME_KEYS_PREJOINED                "KEY_PREJOINED"
#define RS_ANAME_KEYS_SCHEMA                   "KEY_SCHEMA"
#define RS_ANAME_KEYS_CATALOG                  "KEY_CATALOG"
#define RS_ANAME_KEYS_NREF                     "KEY_NREF"

#define RS_ANAME_CARDINAL_REL_ID               "REL_ID"
#define RS_ANAME_CARDINAL_CARDIN               "CARDIN"
#define RS_ANAME_CARDINAL_SIZE                 "SIZE"
#define RS_ANAME_CARDINAL_LAST_UPD             "LAST_UPD"

#define RS_ANAME_INFO_PROPERTY                 "PROPERTY"
#define RS_ANAME_INFO_VALUE_STR                "VALUE_STR"
#define RS_ANAME_INFO_VALUE_INT                "VALUE_INT"

#define RS_ANAME_SYNONYM_TARGET_ID             "TARGET_ID"
#define RS_ANAME_SYNONYM_SYNON                 "SYNON"

#define RS_ANAME_TYPES_TYPE_NAME               "TYPE_NAME"
#define RS_ANAME_TYPES_DATA_TYPE               "DATA_TYPE"
#define RS_ANAME_TYPES_PRECISION               "PRECISION"
#define RS_ANAME_TYPES_LITERAL_PREFIX          "LITERAL_PREFIX"
#define RS_ANAME_TYPES_LITERAL_SUFFIX          "LITERAL_SUFFIX"
#define RS_ANAME_TYPES_CREATE_PARAMS           "CREATE_PARAMS"
#define RS_ANAME_TYPES_NULLABLE                "NULLABLE"
#define RS_ANAME_TYPES_CASE_SENSITIVE          "CASE_SENSITIVE"
#define RS_ANAME_TYPES_SEARCHABLE              "SEARCHABLE"
#define RS_ANAME_TYPES_UNSIGNED_ATTR           "UNSIGNED_ATTRIBUTE"
#define RS_ANAME_TYPES_MONEY                   "MONEY"
#define RS_ANAME_TYPES_AUTO_INCREMENT          "AUTO_INCREMENT"
#define RS_ANAME_TYPES_LOCAL_TYPE_NAME         "LOCAL_TYPE_NAME"
#define RS_ANAME_TYPES_MINIMUM_SCALE           "MINIMUM_SCALE"
#define RS_ANAME_TYPES_MAXIMUM_SCALE           "MAXIMUM_SCALE"

#define RS_ANAME_PROCEDURES_ID                 "ID"
#define RS_ANAME_PROCEDURES_NAME               "PROCEDURE_NAME"
#define RS_ANAME_PROCEDURES_TEXT               "PROCEDURE_TEXT"
#define RS_ANAME_PROCEDURES_BIN                "PROCEDURE_BIN"
#define RS_ANAME_PROCEDURES_SCHEMA             "PROCEDURE_SCHEMA"
#define RS_ANAME_PROCEDURES_CATALOG            "PROCEDURE_CATALOG"
#define RS_ANAME_PROCEDURES_CREATIME           "CREATIME"
#define RS_ANAME_PROCEDURES_TYPE               "TYPE"

typedef enum {
        RS_AVAL_ROUTINE_PROCEDURE = 1,  /* procedure */
        RS_AVAL_ROUTINE_FUNCTION  = 2,  /* function */
        RS_AVAL_ROUTINE_JAVA      = 3   /* java */
} rs_aval_routine_t;

#define RS_ANAME_PROCCOLINFO_PROCID            "PROCEDURE_ID"
#define RS_ANAME_PROCCOLINFO_COLUMNNAME        "COLUMN_NAME"
#define RS_ANAME_PROCCOLINFO_COLUMNTYPE        "COLUMN_TYPE"
#define RS_ANAME_PROCCOLINFO_DATATYPE          "DATA_TYPE"
#define RS_ANAME_PROCCOLINFO_TYPENAME          "TYPE_NAME"
#define RS_ANAME_PROCCOLINFO_COLUMNSIZE        "COLUMN_SIZE"
#define RS_ANAME_PROCCOLINFO_BUFFERLENGTH      "BUFFER_LENGTH"
#define RS_ANAME_PROCCOLINFO_DECIMALDIGITS     "DECIMAL_DIGITS"
#define RS_ANAME_PROCCOLINFO_NUMPRECRADIX      "NUM_PREC_RADIX"
#define RS_ANAME_PROCCOLINFO_NULLABLE          "NULLABLE"
#define RS_ANAME_PROCCOLINFO_REMARKS           "REMARKS"
#define RS_ANAME_PROCCOLINFO_COLUMNDEF         "COLUMN_DEF"
#define RS_ANAME_PROCCOLINFO_SQLDATATYPE       "SQL_DATA_TYPE"
#define RS_ANAME_PROCCOLINFO_SQLDATETIMESUB    "SQL_DATETIME_SUB"
#define RS_ANAME_PROCCOLINFO_CHAROCTETLENGTH   "CHAR_OCTET_LENGTH"
#define RS_ANAME_PROCCOLINFO_ORDINALPOSITION   "ORDINAL_POSITION"
#define RS_ANAME_PROCCOLINFO_ISNULLABLE        "IS_NULLABLE"


#define RS_ANAME_TRIGGERS_ID                    "ID"
#define RS_ANAME_TRIGGERS_NAME                  "TRIGGER_NAME"
#define RS_ANAME_TRIGGERS_TEXT                  "TRIGGER_TEXT"
#define RS_ANAME_TRIGGERS_BIN                   "TRIGGER_BIN"
#define RS_ANAME_TRIGGERS_SCHEMA                "TRIGGER_SCHEMA"
#define RS_ANAME_TRIGGERS_CATALOG               "TRIGGER_CATALOG"
#define RS_ANAME_TRIGGERS_CREATIME              "CREATIME"
#define RS_ANAME_TRIGGERS_TYPE                  "TYPE"
#define RS_ANAME_TRIGGERS_RELID                 "REL_ID"
#define RS_ANAME_TRIGGERS_ENABLED               "TRIGGER_ENABLED"

#define RS_ANAME_EVENTS_ID                     "ID"
#define RS_ANAME_EVENTS_NAME                   "EVENT_NAME"
#define RS_ANAME_EVENTS_PARCOUNT               "EVENT_PARAMCOUNT"
#define RS_ANAME_EVENTS_PARTYPES               "EVENT_PARAMTYPES"
#define RS_ANAME_EVENTS_TEXT                   "EVENT_TEXT"
#define RS_ANAME_EVENTS_SCHEMA                 "EVENT_SCHEMA"
#define RS_ANAME_EVENTS_CATALOG                "EVENT_CATALOG"
#define RS_ANAME_EVENTS_CREATIME               "CREATIME"
#define RS_ANAME_EVENTS_TYPE                   "TYPE"

#define RS_ANAME_TABLEMODES_ID                 "ID"
#define RS_ANAME_TABLEMODES_MODE               "MODE"
#define RS_ANAME_TABLEMODES_MODTIME            "MODIFY_TIME"
#define RS_ANAME_TABLEMODES_MODUSER            "MODIFY_USER"

#define RS_ANAME_FORKEYS_ID                     "ID"
#define RS_ANAME_FORKEYS_REF_REL_ID             "REF_REL_ID"
#define RS_ANAME_FORKEYS_CREATE_REL_ID          "CREATE_REL_ID"
#define RS_ANAME_FORKEYS_REF_KEY_ID             "REF_KEY_ID"
#define RS_ANAME_FORKEYS_REFTYPE                "REF_TYPE"
#define RS_ANAME_FORKEYS_SCHEMA                 "KEY_SCHEMA"
#define RS_ANAME_FORKEYS_CATALOG                "KEY_CATALOG"
#define RS_ANAME_FORKEYS_NREF                   "KEY_NREF"
#define RS_ANAME_FORKEYS_KEY_NAME               "KEY_NAME"
#define RS_ANAME_FORKEYS_KEY_ACTION             "KEY_ACTION"

#define RS_ANAME_FORKEYPARTS_ID                 "ID"
#define RS_ANAME_FORKEYPARTS_KEYP_NO            "KEYP_NO"
#define RS_ANAME_FORKEYPARTS_ATTR_NO            "ATTR_NO"
#define RS_ANAME_FORKEYPARTS_ATTR_ID            "ATTR_ID"
#define RS_ANAME_FORKEYPARTS_ATTR_TYPE          "ATTR_TYPE"
#define RS_ANAME_FORKEYPARTS_CONST_VALUE        "CONST_VALUE"
#define RS_ANAME_FORKEYPARTS_PREFIX_LENGTH      "PREFIX_LENGTH"

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED

#define RS_ANAME_FORKEYS_UNRESOLVED_ID                  "ID"
#define RS_ANAME_FORKEYS_UNRESOLVED_CRE_REL_ID          "CRE_REL_ID"
#define RS_ANAME_FORKEYS_UNRESOLVED_FKEY_NAME           "FKEY_NAME"
#define RS_ANAME_FORKEYS_UNRESOLVED_FKEY_EXTERNALNAME   "FKEY_EXTERNALNAME"
#define RS_ANAME_FORKEYS_UNRESOLVED_FKEY_LEN            "FKEY_LEN"
#define RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_NAME        "REF_REL_NAME"
#define RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_SCHEMA      "REF_REL_SCHEMA"
#define RS_ANAME_FORKEYS_UNRESOLVED_REF_REL_CATALOG     "REF_REL_CATALOG"
#define RS_ANAME_FORKEYS_UNRESOLVED_REF_DEL             "REF_DEL"
#define RS_ANAME_FORKEYS_UNRESOLVED_REF_UPD             "REF_UPD"

#define RS_ANAME_FORKEYPARTS_UNRESOLVED_ID              "ID"
#define RS_ANAME_FORKEYPARTS_UNRESOLVED_REF_FIELD_ANO   "REF_FIELD_ANO"
#define RS_ANAME_FORKEYPARTS_UNRESOLVED_REF_FIELD_NAME  "REF_FIELD_NAME"

#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

#define RS_ANAME_SEQUENCES_ID                   "ID"
#define RS_ANAME_SEQUENCES_NAME                 "SEQUENCE_NAME"
#define RS_ANAME_SEQUENCES_DENSE                "DENSE"
#define RS_ANAME_SEQUENCES_SCHEMA               "SEQUENCE_SCHEMA"
#define RS_ANAME_SEQUENCES_CATALOG              "SEQUENCE_CATALOG"
#define RS_ANAME_SEQUENCES_CREATIME             "CREATIME"

#define RS_ANAME_CATALOGS_ID                    "ID"
#define RS_ANAME_CATALOGS_NAME                  "NAME"
#define RS_ANAME_CATALOGS_CREATIME              "CREATIME"
#define RS_ANAME_CATALOGS_CREATOR               "CREATOR"


#define RS_ANAME_SCHEMAS_ID                     "ID"
#define RS_ANAME_SCHEMAS_NAME                   "NAME"
#define RS_ANAME_SCHEMAS_OWNER                  "OWNER"
#define RS_ANAME_SCHEMAS_CREATIME               "CREATIME"
#define RS_ANAME_SCHEMAS_CATALOG                "SCHEMA_CATALOG"

#define RS_ANAME_HOTSTANDBY_PROPERTY            "PROPERTY"
#define RS_ANAME_HOTSTANDBY_VALUE               "VALUE"
#define RS_ANAME_HOTSTANDBY_SCHEMA              "HOTSTANDBY_SCHEMA"
#define RS_ANAME_HOTSTANDBY_TIME                "MODTIME"

#define RS_ANAME_SYSPROPERTY_KEY                "KEY"
#define RS_ANAME_SYSPROPERTY_VALUE              "VALUE"
#define RS_ANAME_SYSPROPERTY_MODTIME            "MODTIME"

#define RS_ANAME_BLOBS_ID                       "ID"
#define RS_ANAME_BLOBS_STARTPOS                 "STARTPOS"
#define RS_ANAME_BLOBS_ENDSIZE                  "ENDSIZE"
#define RS_ANAME_BLOBS_TOTALSIZE                "TOTALSIZE"
#define RS_ANAME_BLOBS_REFCOUNT                 "REFCOUNT"
#define RS_ANAME_BLOBS_COMPLETE                 "COMPLETE"
#define RS_ANAME_BLOBS_STARTCPNUM               "STARTCPNUM"
#define RS_ANAME_BLOBS_NUMPAGES                 "NUMPAGES"
#define RS_ANAME_BLOBS_PAGE_ADDR_TEMPL          "P%02u_ADDR"
#define RS_ANAME_BLOBS_PAGE_ENDSIZE_TEMPL       "P%02u_ENDSIZE"

#ifdef SS_COLLATION
#define RS_ANAME_COLLATIONS_ID                  "ID"
#define RS_ANAME_COLLATIONS_CHARSET_NAME        "CHARSET_NAME"
#define RS_ANAME_COLLATIONS_COLLATION_NAME      "COLLATION_NAME"
#define RS_ANAME_COLLATIONS_REMARKS             "REMARKS"
#endif

/* System attribute names.
 */
#define RS_ANAME_TUPLE_ID                       "RS_ANAME_TUPLE_ID"
#define RS_ANAME_TUPLE_VERSION                  "RS_ANAME_TUPLE_VERSION"
#ifdef SS_SYNC
#define RS_ANAME_SYNCTUPLEVERS                  "RS_ANAME_SYNC_TUPLE_VERSION"
#define RS_ANAME_SYNC_ISPUBLTUPLE               "RS_ANAME_SYNC_ISPUBLTUPLE"  /* JPA */
#endif

/* Background statements 
 */
#define RS_RELNAME_BGJOBINFO                    "SYS_BACKGROUNDJOB_INFO"
#define RS_ANAME_BGJOBS_ID                      "ID"
#define RS_ANAME_BGJOBS_STMT                    "STMT"
#define RS_ANAME_BGJOBS_USER                    "USER_ID"
#define RS_ANAME_BGJOBS_ECODE                   "ERROR_CODE"
#define RS_ANAME_BGJOBS_ETEXT                   "ERROR_TEXT"

/* Pseudo columns.
 */
#define RS_PNAME_ROWID                         "ROWID"
#define RS_PNAME_ROWVER                        "ROWVER"
#ifdef SS_SYNC
#define RS_PNAME_ROWFLAGS                      "RS_PNAME_ROWFLAGS"      /* Row flags including
                                                                           sync deleted flag and
                                                                           update flag. */
#define RS_PNAME_SYNCTUPLEVERS                 "SYNC_TUPLE_VERSION"     /* JPA */
#define RS_PNAME_SYNC_ISPUBLTUPLE              "SYNC_ISPUBLTUPLE"       /* JPA */
#define RS_FIRST_SYNCID_BIT                    7
#endif

typedef enum {
        RS_AVAL_ROWFLAG_SYNCHISTORYDELETED =  1,  /* Delete mark from sync history table. */
        RS_AVAL_ROWFLAG_UPDATE             =  2  /* Last operation on row was update. */
} rs_aval_rowflag_t;


#define RS_AVAL_YES                 "YES"
#define RS_AVAL_NO                  "NO"
#define RS_AVAL_BASE_TABLE          "BASE TABLE"
#define RS_AVAL_SYNC_TABLE          "SYNCHIST TABLE"
#define RS_AVAL_TRUNCATE_TABLE      "TRUNCATE TABLE"
#define RS_AVAL_VIEW                "VIEW"
#ifdef SS_SYNC
#define   RS_AVAL_SYNC              "SYNC"
#define   RS_AVAL_USERISPUBLIC       0
#define   RS_AVAL_USERISPRIVATE      1
#endif
#define RS_AVAL_SYNONYM             "SYNONYM"
#define RS_AVAL_SYSNAME             "_SYSTEM"
#define RS_AVAL_INFORMATION_SCHEMA  "INFORMATION_SCHEMA"
#define RS_AVAL_USER                "USER"
#define RS_AVAL_ROLE                "ROLE"
#define RS_AVAL_DEFCATALOG          NULL
#define RS_AVAL_DEFCATALOG_PRINTNAME ""

/* Named check constraints */
#define RS_RELNAME_CHECKSTRINGS          "SYS_CHECKSTRINGS"
#define RS_ANAME_CHECKSTRINGS_REL_ID     "REL_ID"
#define RS_ANAME_CHECKSTRINGS_NAME       "CONSTRAINT_NAME"
#define RS_ANAME_CHECKSTRINGS_CONSTRAINT "CONSTRAINT"


/*  Note: these defines must have same values as
 *  SQL_PARAM_INPUT, SQL_PARAM_INPUT_OUTPUT, SQL_RESULT_COL 
 *  and SQL_PARAM_OUTPUT in cli0defs.h and in sqlext.h
 */
#define RS_AVAL_SQLPARAMINPUT           1
#define RS_AVAL_SQLPARAMINPUT_OUTPUT    2
#define RS_AVAL_SQLRESULTCOL            3
#define RS_AVAL_SQLPARAMOUTPUT          4

/*  Note: these defines must have same values as
 *  SQL_NULLABLE and SQL_NO_NULLS in cli0defs.h
 */
#define RS_AVAL_SQLNULLABLE         1
#define RS_AVAL_SQLNONULLS          0


#define RS_MAX_NAMELEN              SU_MAXNAMELEN
#define RS_MAX_COLUMNS              1000

#define RS_PUBLNAME_SYNCCONFIG          "SYNC_CONFIG"
#define RS_PUBLNAME_REGISTERREPLICA     "REGISTER_REPLICA"
#define RS_PUBLNAME_UNREGISTERREPLICA   "UNREGISTER_REPLICA"

/*
        All system generated keys should start with the following
        system key prefix char. It is used to identify user created
        keys from system keys.
*/
#define RS_SYSKEY_PREFIXCHAR    '$'

#define RSK_RELID_DISPLAYSIZE 20

/*
   When relation is created, a clustering key is created.
   The name of the clustering key is formed as

        ($<schema><relation name><RSK_CLUSTERKEYSTR>) obsolete
        $$<relation name>_CLUSTKEY
        $$<relation name>_CLUSTKEY_<relation id>
        
*/
# define RSK_NEW_CLUSTERKEYSTR  "$$%.254s_CLUSTKEY_%ld"
/*# define RSK_CLUSTERKEYSTR       "$$%.254s_CLUSTKEY"*/
# define RSK_OLD_CLUSTERKEYSTR   "$%.254s$%.254s_CLUSTKEY"
/*
   When relation is created, a set of unique constraints are given.
   Every constraint contains one or more attributes.
   The constraints are obeyed by creating a unique key for each of them.
   The name of the keys are formed as

        ($<schema><relation name><RSK_UNQKEYSTR><id>) (obsolete)
        now:
        $$<relation name>_UNQKEY_<id>
        $$<relation name>_UNQKEY_<relation id>_<id>
*/
# define RSK_OLD_UNQKEYSTR   "$%.254s$%.254s_UNQKEY_%u"
/*# define RSK_UNQKEYSTR       "$$%.254s_UNQKEY_%u"*/
# define RSK_NEW_UNQKEYSTR  "$$%.254s_UNQKEY_%ld_%u"
# define RSK_NEW_UNQKEYCONSTRSTR "UNQ$CONSTR_%s"

/*
   When relation is created the primary key definition may be given.
   The resulting key is named as

        ($<schema><relation name><RSK_PRIMKEYSTR>)
        (not anymore), now:
        $$<relation name>_PRIMARYKEY
        for user defined tables now:
        $$<relation name>_PRIMARYKEY_<relation id>

*/
# define RSK_USER_PRIMKEYSTR "$$%.254s_PRIMARYKEY_%ld"
# define RSK_PRIMKEYSTR      "$$%.254s_PRIMARYKEY"
# define RSK_OLD_PRIMKEYSTR  "$%.254s$%.254s_PRIMARYKEY"

/* Foreign key name template:

        ($<schema><relation name><RSK_FORKEYSTR><id>) (obsolete)
        now:
        $$<relation name>_FORKEY_<id>

*/
# define RSK_FORKEYSTR   "$$%.254s_FORKEY_%u"
# define RSK_OLD_FORKEYSTR   "$%.254s$%.254s_FORKEY_%u"

/* Sync history table name template:

        $<schema><relation name><RSK_FORKEYSTR><id>

*/
#define RSK_SYNCHIST_TABLENAMESTR    "_SYNCHIST_%.254s"
#define RSK_SYNCHIST_SYSKEYNAMESTR   "%.254s_KEY_%ld"
#define RSK_SYNCHIST_USERKEYNAMESTR  "_SYNCHIST_%.254s"
#define RSK_SYNCHIST_VERKEYNAMESTR_NEW   "_SYNCHIST_%ld_KEY_%s"
#define RSK_SYNCHIST_VERKEYNAMESTR_2_NEW "_SYNCHIST_%ld_KEY2_%s"
/*#define RSK_SYNCHIST_VERKEYNAMESTR   "%.254s_KEY_%s"*/
/*#define RSK_SYNCHIST_VERKEYNAMESTR_2 "%.254s_KEY2_%s"*/
#define RSK_SYNCHIST_CREATEVERSTR    "_SYNCHIST_CREATEVER"
#define RSK_SYNCHIST_DELETEVERSTR    "_SYNCHIST_DELETEVER"

/* Check constraint name format */
#define RSK_CHECKSTR "$$%.254s_CHECK_%u"

/* Database INTEGER properties */
#define RS_INT_PREC     10  /* 10 decimal digits (31 bits = 9.33 digits) */
#define RS_INT_RADIX    10  /* decimal radix for precision */
#define RS_INT_SCALE    0   /* always zero for integer types */
#define RS_INT_MAX      SS_INT4_MAX
#define RS_INT_MIN      SS_INT4_MIN

/* Database BIGINT properties */
#define RS_BIGINT_PREC  19  /* 19 decimal digits (63 bits = 18.97 digits) */
#define RS_BIGINT_RADIX 10  /* decimal radix for precision */
#define RS_BIGINT_SCALE 0   /* always zero for integer types */

/* Database TINYINT properties */
#define RS_TINYINT_PREC     3  /* decimal digits */
#define RS_TINYINT_RADIX    10  /* decimal radix for precision */
#define RS_TINYINT_SCALE    0   /* always zero for integer types */
#define RS_TINYINT_MAX      SS_INT1_MAX
#define RS_TINYINT_MIN      SS_INT1_MIN

/* Database SMALLINT properties */
#define RS_SMALLINT_PREC    5   /* decimal digits */
#define RS_SMALLINT_RADIX   10  /* decimal radix for precision */
#define RS_SMALLINT_SCALE   0   /* always zero for integer types */
#define RS_SMALLINT_MAX     SHRT_MAX
#define RS_SMALLINT_MIN     SHRT_MIN

/* Database DOUBLE PRECISION properties */
#define RS_DOUBLE_PREC      52  /* 52 binary digits */
#define RS_DOUBLE_RADIX     2   
#define RS_DOUBLE_DECPREC   15
#define RS_DOUBLE_SCALE     0   /* always zero for approx. numeric types */
#define RS_DOUBLE_DISPSIZE  22  

/* Database REAL properties */
#define RS_REAL_PREC        23
#define RS_REAL_RADIX       2
#define RS_REAL_DECPREC     7
#define RS_REAL_SCALE       0   /* always zero for approx. numeric types */
#define RS_REAL_DISPSIZE    13

/* Database DECIMAL properties */
#define RS_DFLOAT_MAXPREC   16  /* used when the precision is not given */
#define RS_DFLOAT_DEFSCALE  2   /* scale used when the scale is not given */
#define RS_DFLOAT_RADIX     10

/* Database DATE properties */
#define RS_DATE_PREC        19  /* yyyy-mm-dd hh:mm:ss */
#define RS_DATE_SCALE       0   /* Not applicable */

#define RS_TIMESTAMP_DISPSIZE_MAX 29 /* YYYY-MM-DD HH-MM-SS.FFFFFFFFF */
#define RS_TIMESTAMP_DISPSIZE_MIN 19 /* YYYY-MM-DD HH-MM-SS */
#define RS_DATE_DISPSIZE          10 /* YYYY-MM-DD */
#define RS_TIME_DISPSIZE          8  /* HH-MM-SS */

#define RS_SCALE_NULL       (-1)
#define RS_LENGTH_NULL      (SS_INT4_MAX) /* note! maybe needs to be edited later!!!! */
#define RS_RADIX_NULL       (-1)
#define RS_VARCHAR_DEFLEN   (254L)
#define RS_CHAR_DEFLEN      (1)

/* Note: must be same as DBE_VABLOBREF_SIZE in dbe0type.h !!!!!!!!!!!! */
#define RS_VABLOBREF_SIZE   (4 + 4 + 4 + 1)

/* new (G2) BLOB reference size in va */
#define RS_VABLOBG2REF_SIZE   (8 + 8 + 1)

/* Start value for user's relation id:s. Smaller
** values are reserved for system use !
** The same value should be used also for attribute
** and key id's.
*/
#define RS_USER_ID_START       10000L

/* First id for system tables and views generated using SQL.
 */
#define RS_SYS_SQL_ID_START    8000L

/* 
 * First id to be used in migration to start checking for unused
 * ids

 */
#define RS_MIGRATION_ID_START   100L

#define RS_SDEFS_ISSYSID(id)            (id < RS_USER_ID_START)
#define RS_SDEFS_ISSYSSCHEMA(schema)    (strcmp(schema, RS_AVAL_SYSNAME) == 0)

/* Fixed relations id's for system tables created in dbe4srli.c. Key id's
 * allocated with numbres relid+n, where n is a key number.
 */
enum rs_sdefs_tabid_t  {
        RS_RELID_TABLES    =  100,
        RS_RELID_COLUMNS   =  200,
        RS_RELID_USERS     =  300,
        RS_RELID_UROLE     =  400,
        RS_RELID_RELAUTH   =  500,
        RS_RELID_ATTAUTH   =  600,
        RS_RELID_VIEWS     =  700,
        RS_RELID_KEYPARTS  =  800,
        RS_RELID_KEYS      =  900,
        RS_RELID_CARDINAL  = 1000,
        RS_RELID_INFO      = 1100,
        RS_RELID_TRANSACT  = 1200,
        RS_RELID_SYNONYM   = 1300,
        RS_RELID_TYPES     = 1400,
        RS_RELID_BADROWS   = 9999
};

#define RS_STRQUOTECH   '\''

/* System parameters and values for bulletin board.
 */
/* Read-only, in master and replica */
#define RS_BBOARD_PAR_NODE_NAME         "SYNC NODE"
#define RS_BBOARD_PAR_SYNC_ID           "SYS_SYNC_ID"
#define RS_BBOARD_PAR_SYNCMASTER        "SYNC MASTER"
#define RS_BBOARD_PAR_SYNCREPLICA       "SYNC REPLICA"
#define RS_BBOARD_PAR_SYNCMODE          "SYNC_MODE" /* NORMAL or MAINTENANCE */
                                                   
/* Read-only, only in master */
#define RS_BBOARD_PAR_TRANID            "SYS_TRAN_ID"
#define RS_BBOARD_PAR_TRANUSERID        "SYS_TRAN_USERID"
#define RS_BBOARD_PAR_ISPROPAGATE       "SYS_IS_PROPAGATE"

/* Read-only. Used to check (in trigger code) if trx is subscribing as replica */
#define RS_BBOARD_PAR_ISSUBSCRIBE       "SYS_IS_SUBSCRIBED"
#define RS_BBOARD_PAR_SYNC_OPERATION_TYPE "SYS_SYNC_OPERATION_TYPE" 
#define RS_BBOARD_PAR_SYNC_RESULTSET_TYPE "SYS_SYNC_RESULTSET_TYPE" 

/* Read-write, only in master, no SET SYNC PARAMETER */
#define RS_BBOARD_PAR_ROLLBACK          "SYS_ROLLBACK"

/* Read-write */
#define RS_BBOARD_PAR_ERROR_CODE        "SYS_ERROR_CODE"
#define RS_BBOARD_PAR_ERROR_TEXT        "SYS_ERROR_TEXT"
#define RS_BBOARD_PAR_MSGEXECTYPE       "SYS_MSG_EXECTYPE"  /* OPTIMISTIC or PESSIMISTIC */
#define RS_BBOARD_PAR_TRANMAXRETRY      "SYS_TRAN_MAXRETRY"
#define RS_BBOARD_PAR_TRANRETRYTIMEOUT  "SYS_TRAN_RETRYTIMEOUT"

#define RS_BBOARD_PAR_DEFAULT_PROPAGATE_ERRORMODE   "SYNC_DEFAULT_PROPAGATE_ERRORMODE"
#define RS_BBOARD_PAR_DEFAULT_PROPAGATE_SAVEMODE    "SYNC_DEFAULT_PROPAGATE_SAVEMODE"
#define RS_BBOARD_PAR_DEFAULT_SAVE_ERRORMODE        "SYNC_DEFAULT_SAVE_ERRORMODE"
#define RS_BBOARD_PAR_DEFAULT_SAVE_SAVEMODE         "SYNC_DEFAULT_SAVE_SAVEMODE"


/* Read-write, no PUT_PARAM */
#define RS_BBOARD_PAR_R_MAXBYTES_OUT    "SYS_R_MAXBYTES_OUT"
#define RS_BBOARD_PAR_R_MAXBYTES_IN     "SYS_R_MAXBYTES_IN"
#define RS_BBOARD_NOSYNCESTIMATE        "SYS_NOSYNCESTIMATE"
#define RS_BBOARD_SYNCFULLCOST          "SYS_SYNCFULLCOST"
#define RS_BBOARD_SYNC_APPVERSION       "SYNC_APP_SCHEMA_VERSION"
#define RS_BBOARD_KEEPLOCALCHANGES      "SYS_SYNC_KEEPLOCALCHANGES"
#define RS_BBOARD_REPLICA_REFRESH_LOAD  "SYS_SYNC_REPLICA_REFRESH_LOAD"

#define RS_BBOARD_VAL_YES               RS_AVAL_YES
#define RS_BBOARD_VAL_MODE_NORMAL       "NORMAL"
#define RS_BBOARD_VAL_MODE_MAINTENANCE  "MAINTENANCE"

bool rs_sdefs_sysaname(
        char* aname);

bool rs_sdefs_sysparam(
        char* pname);

char* rs_sdefs_buildsynchistorytablename(
        char* relname);

#undef RS_AVAL_DEFCATALOG
#undef RS_AVAL_DEFCATALOG_PRINTNAME

#define RS_AVAL_DEFCATALOG           (rs_sdefs_getcurrentdefcatalog())
#define RS_AVAL_DEFCATALOG_NEW       (rs_sdefs_getnewdefcatalog())
#define RS_AVAL_DEFCATALOG_PRINTNAME RS_AVAL_DEFCATALOG

char* rs_sdefs_getnewdefcatalog(void);
char* rs_sdefs_getcurrentdefcatalog(void);
void rs_sdefs_setnewdefcatalog(char* defcatalog);
void rs_sdefs_setcurrentdefcatalog(char* defcatalog);
void rs_sdefs_globaldone(void);

/* System sequences */
#define RS_SEQ_BGJOBID                  "SYS_SEQ_BGJOBID"
#define RS_SEQ_MSGID                    "SYS_SEQ_MSGID"

#endif /* RS0SDEFS_H */
