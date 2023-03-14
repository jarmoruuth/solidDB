/*************************************************************************\
**  source       * tab0type.h
**  directory    * tab
**  description  * Type definitions.
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


#ifndef TAB0TYPE_H
#define TAB0TYPE_H

#include <su0bflag.h>

typedef struct tb_admin_cmd_st tb_admin_cmd_t;

typedef enum {
        TB_SQL_CONT,
        TB_SQL_SUCC,
        TB_SQL_END,
        TB_SQL_ERROR
} tb_sql_ret_t;


typedef enum {
        TB_DD_CREATEREL_SYSTEM,
        TB_DD_CREATEREL_USER,
        TB_DD_CREATEREL_SYNC,
        TB_DD_CREATEREL_TRUNCATE
} tb_dd_createrel_t;

typedef enum {
        TB_DD_PERSISTENCY_PERMANENT = 0,
        TB_DD_PERSISTENCY_TEMPORARY,
        TB_DD_PERSISTENCY_GLOBAL_TEMPORARY,
        TB_DD_PERSISTENCY_TRANSIENT
} tb_dd_persistency_t;

typedef enum {
        TB_DD_STORE_DEFAULT = 0,
        TB_DD_STORE_MEMORY,
        TB_DD_STORE_DISK
} tb_dd_store_t;

typedef enum {
        TB_DD_DURABILITY_DEFAULT,
        TB_DD_DURABILITY_STRICT,
        TB_DD_DURABILITY_RELAXED
} tb_dd_durability_t;

typedef enum {
        /* system default disregarding DefaultStoreIsMemory setting! */
        TB_RELMODE_SYSDEFAULT       = 0,
        
        TB_RELMODE_OPTIMISTIC       = SU_BFLAG_BIT(0),
        TB_RELMODE_PESSIMISTIC      = SU_BFLAG_BIT(1),
        TB_RELMODE_NOCHECK          = SU_BFLAG_BIT(2),
        TB_RELMODE_CHECK            = SU_BFLAG_BIT(3),
        TB_RELMODE_MAINMEMORY       = SU_BFLAG_BIT(4),
        TB_RELMODE_SYNC             = SU_BFLAG_BIT(5),
        TB_RELMODE_NOSYNC           = SU_BFLAG_BIT(6),
        TB_RELMODE_DISK             = SU_BFLAG_BIT(7),
        TB_RELMODE_TRANSIENT        = SU_BFLAG_BIT(8),
        TB_RELMODE_GLOBALTEMPORARY  = SU_BFLAG_BIT(9)
} tb_relmode_t;

typedef ss_uint4_t tb_relmodemap_t;

typedef enum {
        TB_PRIV_SELECT       = (int)SU_BFLAG_BIT(0),
        TB_PRIV_INSERT       = (int)SU_BFLAG_BIT(1),
        TB_PRIV_DELETE       = (int)SU_BFLAG_BIT(2),
        TB_PRIV_UPDATE       = (int)SU_BFLAG_BIT(3),
        TB_PRIV_REFERENCES   = (int)SU_BFLAG_BIT(4),
        TB_PRIV_CREATOR      = (int)SU_BFLAG_BIT(5),
        TB_PRIV_EXECUTE      = (int)SU_BFLAG_BIT(6),
        TB_PRIV_REFRESH_PUBL = (int)SU_BFLAG_BIT(7)
} tb_priv_t;

#define TB_PRIV_ADMINBITS  (TB_PRIV_CREATOR)

typedef enum {
        TB_UPRIV_ADMIN          = (int)SU_BFLAG_BIT(0),
        TB_UPRIV_CONSOLE        = (int)SU_BFLAG_BIT(1),
        TB_UPRIV_SYNC_ADMIN     = (int)SU_BFLAG_BIT(2),
        TB_UPRIV_SYNC_REGISTER  = (int)SU_BFLAG_BIT(3),
        TB_UPRIV_REPLICAONLY    = (int)SU_BFLAG_BIT(4)
} tb_upriv_t;

typedef struct tb_sql_cache_st  tb_sql_cache_t;
typedef struct tb_relpriv_st tb_relpriv_t;
typedef struct tb_sqlcur_st  tb_sqlcur_t;
typedef struct tb_trig_st  tb_trig_t;

typedef struct {
        void (*sp_cur_globalinit)(void);
        void (*sp_cur_globaldone)(void);
        tb_sqlcur_t* (*sp_cur_init)(
            void* cd,
            sqlsystem_t* sqlsystem,
            sqltrans_t* sqltrans,
            char* sqlstr,
            void* event,
            tb_sql_cache_t* proccache,
            rs_err_t** p_errh);
        void (*sp_cur_done)(
            tb_sqlcur_t* cur);
        bool (*sp_cur_open)(
            tb_sqlcur_t* cur);
        bool (*sp_cur_exec)(
            tb_sqlcur_t* cur);
        bool (*sp_cur_fetch)(
            tb_sqlcur_t* cur,
            bool nextp,
            rs_tval_t** p_tval);
        void (*sp_cur_reset)(
            tb_sqlcur_t* cur);
        bool (*sp_cur_setpartval)(
            tb_sqlcur_t* cur,
            rs_ttype_t* ttype,
            rs_tval_t* tval);
        bool (*sp_cur_setparaval)(
            tb_sqlcur_t* cur,
            int parno,
            rs_atype_t* atype,
            rs_aval_t* aval);
        rs_ttype_t* (*sp_cur_getttype)(
            tb_sqlcur_t* cur);
        uint (*sp_cur_getparcount)(
            tb_sqlcur_t* cur);
        bool (*sp_cur_iserror)(
            tb_sqlcur_t* cur);
        bool (*sp_cur_isfetch)(
            tb_sqlcur_t* cur);
        bool (*sp_cur_isproc)(
            tb_sqlcur_t* cur);
        bool (*sp_cur_estimate)(
            tb_sqlcur_t* cur,
            rs_estcost_t* p_lines,
            rs_estcost_t* p_c0,
            rs_estcost_t* p_c1);
        bool (*sp_cur_generateproccolinfo)(
            rs_sysi_t* cd,
            sqlsystem_t* sqls,
            tb_trans_t* trans,
            long procid,
            rs_entname_t* en,
            char* sqlstr,
            su_err_t** errh);
        ulong (*sp_cur_rowshandled)(
            tb_sqlcur_t* cur);

        tb_sql_cache_t* (*sp_cur_cache_init)(
            int cache_size);

        void (*sp_cur_cache_done)(
            tb_sql_cache_t* cur_cache);

        void (*sp_cur_cache_flush)(
            tb_sql_cache_t* cur_cache);

        void (*sp_cur_set_cached)(
            tb_sqlcur_t* cur,
            bool enabled);

} tb_sqlcur_funblock_t;

typedef struct {
        int (*sp_trig_execdirect)(
            void* cd,
            void* trans,
            rs_entname_t* trigname,
            char* trigstr,
            rs_ttype_t* ttype,
            rs_tval_t* old_tval,
            rs_tval_t* new_tval,
            void** p_ctx,
            rs_err_t** p_errh);
        void (*sp_trig_done_nocache)(
            tb_trig_t* trig);
} tb_trigger_funblock_t;

#endif /* TAB0TYPE_H */
