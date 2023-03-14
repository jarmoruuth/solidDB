/*************************************************************************\
**  source       * tab1dd.h
**  directory    * tab
**  description  * Functions for data distionaty manipulation
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

#ifndef TAB1DD_H
#define TAB1DD_H

#include <dt0date.h>

#include <su0error.h>
#include <su0rbtr.h>
#include <su0chcvt.h>

#include <rs0types.h>
#include <rs0sysi.h>
#include <rs0error.h>
#include <rs0relh.h>
#include <rs0key.h>
#include <rs0auth.h>
#include <rs0rbuf.h>
#include <rs0sysi.h>
#include <rs0types.h>
#include <rs0entna.h>

#include <tab0admi.h>

#include "tab0type.h"
#include "tab1priv.h"
#include "tab0tran.h"
#include "tab0tli.h"

typedef struct {
        rs_sysinfo_t*  ui_cd;
        tb_trans_t*    ui_trans;
        rs_auth_t*     ui_auth;
        TliConnectT*   ui_tcon;
} tb_dd_sysrel_userinfo_t;


typedef enum {
        DD_UPDATE_CREATETABLE, /* runs command(s) if table doesn't exist */
        DD_UPDATE_CREATEINDEX, /* runs command(s) if index doesn't exist */
        DD_UPDATE_DROPINDEX,   /* runs command(s) if index exists! */
        DD_UPDATE_ADDCOLUMN,   /* runs command(s) if column doesn't exist */
        DD_UPDATE_COMMIT,
        DD_UPDATE_CREATEEVENT,
        DD_UPDATE_SETVALUE,
        DD_UPDATE_CHECKEXISTENCE,
        DD_UPDATE_CREATEVIEW,
        DD_UPDATE_CREATEPROCEDURE,
        DD_UPDATE_CREATESEQUENCE,
        DD_UPDATE_CREATETABLE_SQL
} dd_updatetype_t;

typedef struct {
        const char*           relname;
        const char*           extname;
        dd_updatetype_t       type;
        const char*           sqlstr;
} dd_updatestmt_t;

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
typedef struct tb_sqlforkey_unres {
        tb_sqlforkey_t  for_key;
        long            cre_rel_id;
} tb_sqlforkey_unres_t;
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

typedef struct {
        long          relid;
        const char*   table_name;
        const char*   table_type;
        const char*   table_schema;
        const char*   table_catalog;
        const char*   createtime;
        const char*   check;
        const char*   remarks;
} tb_dd_sys_tables_t;

typedef struct {
        long          id;
        long          rel_id;
        const char*   column_name;
        long          column_number;
        const char*   data_type;
        int           sql_data_type_num;
        long          data_type_number;
        long          char_max_length;
        long          numeric_precision;
        int           numeric_prec_radix;
        int           numeric_scale;
        const char*   nullable;
        int           nullable_odbc;
        const char*   format;
        const char*   default_val;
        long          attr_type;
        const char*   remarks;
} tb_dd_sys_columns_t;

typedef struct {
        long          id;
        const char*   original_default;
        long          auto_inc_seq_id;
        long          external_data_type;
        const char*   external_collation;
} tb_dd_sys_columns_aux_t;

typedef struct {
        long          id;
        long          rel_id;
        const char*   key_name;
        const char*   key_unique;
        int           key_nonunique_odbc;
        const char*   key_clustering;
        const char*   key_primary;
        const char*   key_prejoined;
        const char*   key_schema;
        long          key_nref;
        const char*   key_catalog;
} tb_dd_sys_keys_t;

typedef struct {
        long          id;
        long          rel_id;
        long          keyp_no;
        long          attr_id;
        long          attr_no;
        long          attr_type;
        const char*   const_value;
        const char*   ascending;
} tb_dd_sys_keyparts_t;

typedef struct {
        long          id;
        long          keyp_no;
        long          prefix_length;
} tb_dd_sys_keyparts_aux_t;

typedef struct {
        long          id;
        long          ref_rel_id;
        long          create_rel_id;
        long          ref_key_id;
        long          ref_type;
        const char*   schema;
        const char*   catalog;
        long          nref;
        const char*   key_name;
        long          key_action;
} tb_dd_sys_forkeys_t;

typedef struct {
        long          id;
        long          keyp_no;
        long          attr_no;
        long          attr_id;
        long          attr_type;
        const char*   const_value;
        long          prefix_length;
} tb_dd_sys_forkeyparts_t;

typedef struct {
        long          id;
        const char*   name;
        const char*   owner;
        const char*   createtime;
        const char*   catalog;
} tb_dd_sys_schemas_t;

typedef struct {
        const char*   name;
        long          id;
        const char*   dense;
        const char*   schema;
        const char*   catalog;
        const char*   createtime;
} tb_dd_sys_sequences_t;

typedef struct {
        long          rel_id;
        unsigned long long cardin;
        unsigned long long size;
        const char*   last_update;
} tb_dd_sys_cardinal_t;

typedef struct {
        long          id;
        const char*   mode;
        const char*   modify_time;
        const char*   modify_user;
} tb_dd_sys_tablemodes_t;

typedef struct {
        const char*   command;
        const char*   parameters;
} tb_dd_sys_admin_commands_t;

typedef struct {
        long          id;
        const char*   charset_name;
        const char*   collation_name;
        const char*   remarks;
} tb_dd_sys_collations_t;

typedef struct {
        const char*   property;
        const char*   val_str;
        long          val_int;
} tb_dd_sys_info_t;

typedef struct {
        unsigned long long id;
        unsigned long long startpos;
        unsigned long long endsize;
        unsigned long long totalsize;
        int           refcount;
        int           complete;
        int           startcpnum;
        int           numpages; /* Note: There are more attributes on this table
                                   but they are left out */
} tb_dd_sys_blobs_t;

#ifdef SS_COLLATION
extern su_collation_t* (*tb_dd_collation_init)(char* collation_name, uint collation_id);
#endif /* SS_COLLATION */

su_list_t* tb_dd_attrlist_init(
        void);

void tb_dd_attrlist_done(
        su_list_t* attrlist);

void tb_dd_attrlist_add(
        su_list_t*      attrlist,
        char*           attrname,
        rs_atype_t*     atype,
        rs_auth_t*      auth,
        rs_atype_t*     defatype,
        rs_aval_t*      defaval,
        long*           p_newattrid,
        bool            syscall);

bool tb_dd_createsyskeysschemakey(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys);

bool tb_dd_createsyskeyscatalogkey(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys);

bool tb_dd_createsystablescatalogkey(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys);

bool tb_dd_updatesysrelschemakeys(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys);

bool tb_dd_updatesynchistorykeys(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys);

bool tb_dd_runstartupsqlstmts(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys
#ifdef COLLATION_UPDATE
        ,su_chcollation_t chcollation
#endif
        );

bool tb_dd_updatestartupsqlstmts(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf,
        su_rbt_t** p_newrel_rbt);

bool tb_dd_updatecatalogkeys(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        rs_rbuf_t* rbuf,
        su_rbt_t** p_newrel_rbt);

dt_date_t tb_dd_curdate(
        void);

void tb_dd_add_pseudo_atypes(
        void* cd,
        rs_ttype_t* ttype);

bool tb_dd_checkifrelempty(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname);

rs_relh_t* tb_dd_getrelh(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh);

rs_relh_t* tb_dd_getrelhbyid(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        ulong relid,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh);

rs_relh_t* tb_dd_getrelhbyid_ex(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long relid,
        su_err_t** p_errh);
        
rs_relh_t* tb_dd_readrelh_norbufupdate(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        ulong relid);

rs_relh_t* tb_dd_getrelhfromview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname,
        rs_entname_t* viewname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh);

su_rbt_t* tb_dd_readallkeysintorbt(
        rs_sysinfo_t* cd);

bool tb_dd_setrelmode(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        tb_relmode_t relmode,
        bool createrel,
        rs_err_t** p_errh);

bool tb_dd_setrelmodes(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        tb_relmodemap_t relmodes,
        bool createrel,
        rs_err_t** p_errh);

su_ret_t tb_dd_createrel(
        rs_sysinfo_t*  cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_auth_t* auth,
        tb_dd_createrel_t type,
        long relationid,
        rs_err_t** p_errh);

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
        rs_err_t** p_errh);

su_ret_t tb_dd_droprefkey(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char*      name,
        rs_auth_t* auth,
        rs_err_t** p_errh);

dbe_ret_t tb_dd_resolverefkeys(
        rs_sysinfo_t*   cd,
        rs_relh_t*      relh);

void tb_dd_setmmerefkeyinfo(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_relh_t* refrelh);

su_ret_t tb_dd_createnamedcheck(
         rs_sysinfo_t* cd,
         tb_trans_t* trans,
         rs_relh_t* relh,
         char* name,
         char* checkstring,
         rs_auth_t* auth,
         rs_err_t** p_errh);

bool tb_dd_hasnamedcheck(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* name);

bool tb_dd_dropnamedcheck(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* name);

bool tb_dd_droprel(
        rs_sysinfo_t*  cd,
        tb_trans_t* trans,
        bool usercall,
        rs_entname_t* relname,
        bool issyncrel,
        long* p_relid,
        rs_entname_t** p_relname,
        bool* p_issyncrel,
        bool checkforkeys,
        bool cascade,
        rs_err_t** p_errh);

su_ret_t tb_dd_addattributelist(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        su_list_t* attrlist,
        bool syskeyadd,
        rs_err_t** p_errh);

su_ret_t tb_dd_removeattribute(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* attrname,
    	rs_auth_t* auth,
        rs_err_t** p_errh);

su_ret_t tb_dd_modifyattribute(
        rs_sysinfo_t*   cd,
        tb_trans_t*     trans,
        rs_relh_t*      relh,
        char*           attrname,
        bool            updatenotnull,
        rs_atype_t*     atype,
        rs_auth_t*      auth,
        rs_atype_t*     defatype,
        rs_aval_t*      defaval,
        rs_err_t**      p_errh,
        bool            force);

su_ret_t tb_dd_renameattribute(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* attrname,
        char* new_attrname,
    	rs_auth_t* auth,
        rs_err_t** p_errh);

su_ret_t tb_dd_changeschema(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        char* new_schema,
    	rs_auth_t* auth,
        rs_err_t** p_errh);

bool tb_dd_renametable(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        tb_relh_t* tbrelh,
        char* newname,
        su_err_t** p_errh);

bool tb_dd_renametable_ex(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        tb_relh_t* tbrelh,
        char* newname,
        char* newschema,
        su_err_t** p_errh);

bool tb_dd_truncaterel(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* relname,
        bool issyncrel,
        rs_err_t** p_errh);

su_ret_t tb_dd_createindex(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_ttype_t* ttype,
        rs_key_t* key,
        rs_auth_t* auth,
        rs_entname_t* keyname,
        tb_dd_createrel_t type,
        rs_err_t** p_errh);

void td_dd_createonekey(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_ttype_t* ttype,
        rs_key_t* key,
        rs_auth_t* auth,
        tb_dd_createrel_t type);

su_ret_t tb_dd_dropindex(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* keyname,
        bool issynctable,
        bool* p_issynctable,
        rs_err_t** p_errh);

su_ret_t tb_dd_dropindex_relh(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_entname_t* keyname,
        bool issynctable,
        bool* p_issynctable,
        rs_err_t** p_errh);

rs_viewh_t* tb_dd_getviewh(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* viewname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh);

rs_viewh_t* tb_dd_getviewhfromview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* viewname,
        rs_entname_t* refviewname,
        tb_relpriv_t** p_priv,
        rs_err_t** p_errh);

su_ret_t tb_dd_createview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_viewh_t* viewh,
    	rs_auth_t* auth,
        rs_err_t** p_errh);

su_ret_t tb_dd_createsysview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_viewh_t* viewh,
    	rs_auth_t* auth,
        rs_err_t** p_errh);

bool tb_dd_dropview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* viewname,
        long* viewid,
        rs_entname_t** p_viewname,
        rs_err_t** p_errh);

void tb_dd_updatecardinal(
        TliConnectT* tcon,
        long relid,
        rs_cardin_t* cardin);

void tb_dd_dropcardinal(
        TliConnectT* tcon,
        long relid);

void tb_dd_loadrbuf(
        rs_sysinfo_t* cd,
        rs_rbuf_t* rbuf,
        bool full_refresh,
        bool keep_cardinal);

bool tb_dd_getcollation(
        rs_sysinfo_t* cd,
        su_chcollation_t* p_collation);

bool tb_dd_insertsysinfo_collation(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys,
        su_chcollation_t chcollation);

void tb_dd_addinfo(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        char* property,
        char* value_str,
        long value_long);

bool tb_dd_removeinfo(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        char* property);

bool tb_dd_getinfo(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        char* property,
        long* p_value_long,
        bool* p_value_long_is_null,
        char** p_value_str);

bool tb_dd_find_dependent_rels(
        TliConnectT*    tcon,
        long            relid,
        su_pa_t*        rel_list,
        rs_err_t**      p_errh);

bool tb_dd_find_depending_rels(
        TliConnectT*    tcon,
        long            relid,
        su_pa_t*        rel_list,
        rs_err_t**      p_errh);

#ifdef SS_UNICODE_SQL
extern bool tb_dd_migratingtounicode;
extern bool tb_dd_migratetounicode_done;
#else
#define tb_dd_migratingtounicode FALSE
#define tb_dd_migratetounicode_done FALSE
#endif /* SS_UNICODE_SQL */

bool tb_dd_updatedefcatalog(
        rs_sysinfo_t* cd,
        tb_trans_t* trans);

bool tb_dd_checkobjectname(char* objectname);

bool tb_dd_droprefkeys_ext(
        TliConnectT* tcon,
        rs_relh_t* relh,
        rs_err_t** p_errh);

bool tb_dd_checkdefaultvalue(
        rs_sysi_t*      cd,
        rs_atype_t*     natatype,
        rs_atype_t*     defatype,
        rs_aval_t*      defaval,
        rs_err_t**      p_errh);

void tb_dd_execsqlinstmt(
        rs_sysinfo_t* cd,
        sqlsystem_t* sys,
        tb_trans_t* trans,
        char* sqlstr);

bool tb_dd_isforkeyref(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_err_t** p_errh);

bool tb_dd_get_createtime(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        dt_date_t*   time);

bool tb_dd_get_updatetime(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        dt_date_t*   time);

bool tb_dd_update_forkeys(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        ulong        old_relid,
        ulong        new_relid,
        rs_err_t**   p_errh);

#ifdef SS_COLLATION
bool tb_dd_update_system_collations(
        tb_database_t* tdb,
        uint           charset_number,
        const char*    charset_name,
        const char*    charset_csname);
#endif

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)

ulong tb_dd_get_refrelid_by_relid(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        ulong       relid);

void tb_dd_fill_sys_tables_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_tables_list);

void tb_dd_fill_sys_columns_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_columns_list);

void tb_dd_fill_sys_columns_aux_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_columns_aux_list);

void tb_dd_fill_sys_keys_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_keys_list);

void tb_dd_fill_sys_keyparts_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_keyparts_list);

void tb_dd_fill_sys_keyparts_aux_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_keyparts_aux_list);

void tb_dd_fill_sys_forkeys_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_forkeys_list);

void tb_dd_fill_sys_forkeyparts_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_forkeyparts_list);

void tb_dd_fill_sys_schemas_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_schemas_list);

void tb_dd_fill_sys_sequences_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_sequences_list);

void tb_dd_fill_sys_cardinal_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_cardinal_list);

void tb_dd_fill_sys_tablemodes_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_tablemodes_list);

void tb_dd_fill_sys_admin_commands_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_admin_list);

void tb_dd_fill_sys_collations_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_collations_list);

void tb_dd_fill_sys_info_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_info_list);

void tb_dd_fill_sys_blobs_list(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_list_t*  sys_blobs_list);

#endif /* SS_MYSQL || SS_MYSQL_AC */

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
void tb_dd_create_fkeys_unresolved_info(
        rs_sysinfo_t*   cd,
        tb_trans_t*     trans,
        rs_relh_t*      cre_relh,
        uint            n_forkey,
        tb_sqlforkey_t* forkeys,
        long*           ar_ids);

void tb_dd_update_fkeys_unresolved_info(
        rs_sysinfo_t* cd,
        tb_trans_t*   trans,
        long*         ar_ids,
        long          n_ids,
        long          cre_rel_id);

bool tb_dd_get_from_fkeys_unresolved(
        rs_sysinfo_t*           cd,
        tb_trans_t*             trans,
        char*                   ref_rel_name,
        char*                   ref_rel_schema,
        char*                   ref_rel_catalog,
        long                    cre_rel_id,         /* ignore if 0 */
        long*                   p_n_forkey,
        tb_sqlforkey_unres_t**  pp_forkeys);

void tb_dd_delete_from_fkeys_unresolved(
        rs_sysinfo_t* cd,
        tb_trans_t*   trans,
        long cre_rel_id);

void tb_dd_drop_fkeys_unresolved(
        tb_database_t* tdb);
        
void tb_dd_free_fkeys_unresolved(
        long n_forkeys,
        tb_sqlforkey_unres_t* forkeys_unres);
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

#endif /* TAB1DD_H */
