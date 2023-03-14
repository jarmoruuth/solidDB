/*************************************************************************\
**  source       * ha_soliddb_is.h
**  directory    * mysql
**  description  * Function interface of Information Schema extensions
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

#ifndef HA_SOLIDDB_IS_H
#define HA_SOLIDDB_IS_H

#if !defined __STRICT_ANSI__ && defined __GNUC__ && (__GNUC__) > 2 && !defined __INTEL_COMPILER
#define STRUCT_FLD(name, value) name: value
#else
#define STRUCT_FLD(name, value) value
#endif

#define END_OF_ST_FIELD_INFO \
        {STRUCT_FLD(field_name,         NULL), \
         STRUCT_FLD(field_length,       0), \
         STRUCT_FLD(field_type,         MYSQL_TYPE_NULL), \
         STRUCT_FLD(value,              0), \
         STRUCT_FLD(field_flags,        0), \
         STRUCT_FLD(old_name,           ""), \
         STRUCT_FLD(open_method,        SKIP_OPEN_TABLE)}

const char plugin_author[] = "SolidDB Corporation + Jonathon Coombes";

bool schema_table_store_record(THD *thd, TABLE *table);

extern struct st_mysql_plugin   i_s_soliddb_sys_tables;
extern struct st_mysql_plugin   i_s_soliddb_sys_columns;
extern struct st_mysql_plugin   i_s_soliddb_sys_columns_aux;
extern struct st_mysql_plugin   i_s_soliddb_sys_keys;
extern struct st_mysql_plugin   i_s_soliddb_sys_keyparts;
extern struct st_mysql_plugin   i_s_soliddb_sys_keyparts_aux;
extern struct st_mysql_plugin   i_s_soliddb_sys_forkeys;
extern struct st_mysql_plugin   i_s_soliddb_sys_forkeyparts;
extern struct st_mysql_plugin   i_s_soliddb_sys_schemas;
extern struct st_mysql_plugin   i_s_soliddb_sys_sequences;
extern struct st_mysql_plugin   i_s_soliddb_sys_cardinal;
extern struct st_mysql_plugin   i_s_soliddb_sys_tablemodes;
extern struct st_mysql_plugin   i_s_soliddb_sys_info;
extern struct st_mysql_plugin   i_s_soliddb_sys_blobs;

int soliddb_init_sys_tables(void *p);
int soliddb_init_sys_columns(void *p);
int soliddb_init_sys_columns_aux(void *p);
int soliddb_init_sys_keys(void *p);
int soliddb_init_sys_keyparts(void *p);
int soliddb_init_sys_keyparts_aux(void *p);
int soliddb_init_sys_forkeys(void *p);
int soliddb_init_sys_forkeyparts(void *p);
int soliddb_init_sys_schemas(void *p);
int soliddb_init_sys_sequences(void *p);
int soliddb_init_sys_cardinal(void *p);
int soliddb_init_sys_tablemodes(void *p);
int soliddb_init_sys_collations(void *p);
int soliddb_init_sys_info(void *p);
int soliddb_init_sys_blobs(void *p);
static int soliddb_deinit_is(void *p);


#endif /* HA_SOLIDDB_IS_H */
