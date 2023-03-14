/*************************************************************************\
**  source       * tab0conn.h
**  directory    * tab
**  description  * Connection routines to table level.
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


#ifndef TAB0CONN_H
#define TAB0CONN_H

#include <uti0va.h>

#include <su0inifi.h>
#include <su0list.h>
#include <su0error.h>
#include <su0chcvt.h>
#include <su0rbtr.h>
#include <su0task.h>

#include <rs0sysi.h>
#include <rs0relh.h>
#include <rs0entna.h>

#include <dbe0type.h>
#include <dbe0db.h>
#include <dbe0crypt.h>

#include "tab0sqls.h"
#include "tab0tran.h"
#include "tab0sche.h"
#include "tab0blobg2.h"
#include "tab0type.h"

typedef struct tb_database_st tb_database_t;
typedef struct tb_connect_st tb_connect_t;
typedef struct tb_recovctx_st tb_recovctx_t;

#ifdef SS_HSBG2
#include "tab0sysproperties.h"
#endif /* SS_HSBG2 */

#define tb_sysconnect_init(tdb)                 tb_sysconnect_init_ex(tdb, (char *)__FILE__, __LINE__)
#define tb_sysconnect_initbycd(tdb, cd)         tb_sysconnect_initbycd_ex(tdb, cd, (char *)__FILE__, __LINE__)
#define tb_hsbconnect_init(tdb)                 tb_hsbconnect_init_ex(tdb, (char *)__FILE__, __LINE__)
#define tb_hsbconnect_initbycd(tdb, cd)         tb_hsbconnect_initbycd_ex(tdb, cd, (char *)__FILE__, __LINE__)
#define tb_hsbg2_connect_init(tdb)              tb_hsbg2_connect_init_ex(tdb, (char *)__FILE__, __LINE__)
#define tb_hsbg2_connect_initbycd(tdb, cd)      tb_hsbg2_connect_initbycd_ex(tdb, cd, (char *)__FILE__, __LINE__)

#define tb_connect(tdb,loginid,username,password)         tb_connect_ex(tdb,loginid,username,password,(char *)__FILE__,__LINE__)
#define tb_connect_hsb(tdb,loginid,username,password)     tb_connect_hsb_ex(tdb,loginid,username,password,(char *)__FILE__,__LINE__)
#define tb_connect_console(tdb,loginid,username,password) tb_connect_console_ex(tdb,loginid,username,password,(char *)__FILE__,__LINE__)
#define tb_connect_java(tdb,loginid,username,password)    tb_connect_java_ex(tdb,loginid,username,password,(char *)__FILE__,__LINE__)
#define tb_connect_server(tdb,loginid,username,password)  tb_connect_server_ex(tdb,loginid,username,password,(char *)__FILE__,__LINE__)
#define tb_connect_replica(tdb,loginid,username,password) tb_connect_replica_ex(tdb,loginid,username,password,(char *)__FILE__,__LINE__)
#define tb_connect_replica_byuserid(tdb,loginid,userid)   tb_connect_replica_byuserid_ex(tdb,loginid,userid,(char *)__FILE__,__LINE__)
#define tb_connect_local(loginid,username,password)       tb_connect_local_ex(loginid,username,password,(char *)__FILE__,__LINE__)

typedef struct {
        int     ac_rc;
        char*   ac_str;
        bool    ac_isadmevent;
        void*   ac_cd;
        rs_ttype_t* ac_ttype;
        rs_tval_t* ac_tval;
} tb_acmd_t;

typedef enum {
        TB_CONVERT_NO,
        TB_CONVERT_YES,
        TB_CONVERT_AUTO
} tb_convert_type_t;

typedef enum {
        TB_INIT_SUCC,
        TB_INIT_CONVERT,
        TB_INIT_NOCONVERT,
        TB_INIT_DBEXIST,
        TB_INIT_DBNOTEXIST,
        TB_INIT_NOTCREATE,
        TB_INIT_FILESPECPROBLEM,
        TB_INIT_BROKENNETCOPY
} tb_init_ret_t;

extern void* tb_init_inifile;

void tb_setcreateuser(
        char* username,
        char* password);

tb_connect_t* tb_sysconnect_init_ex(
        tb_database_t* tdb,
        char* file, 
        int line);

tb_connect_t* tb_sysconnect_initbycd_ex(
        tb_database_t* tdb, 
        rs_sysi_t* cd, 
        char* file, 
        int line);

void tb_sysconnect_done(
        tb_connect_t* tc);

void tb_sysconnect_transinit(
        tb_connect_t* tc);

tb_connect_t* tb_hsbconnect_init_ex(
        tb_database_t* tdb,
        char* file, 
        int line);

tb_connect_t* tb_hsbconnect_initbycd_ex(
        tb_database_t* tdb,
        rs_sysi_t* cd,
        char* file, 
        int line);

void tb_hsbconnect_done(
        tb_connect_t* tc);

tb_connect_t* tb_hsbg2_connect_init_ex(
        tb_database_t* tdb,
        char* file, 
        int line);

tb_connect_t* tb_hsbg2_connect_initbycd_ex(
        tb_database_t* tdb,
        rs_sysi_t* cd,
        char* file, 
        int line);

void tb_hsbg2_connect_done(
        tb_connect_t* tc);


tb_init_ret_t tb_init_server(
        su_inifile_t* inifile,
        bool allow_create_db,
        bool recover_anyway,
        bool createonly,
        tb_convert_type_t convert_type,
        tb_database_t** p_tdb,
        char** p_username,
        char** p_password,
        bool check_existence_of_all_db_files,
        bool force_inmemory_freelist,
        dbe_cryptoparams_t* cp);

void tb_done_server(
        tb_database_t* tdb);

void tb_done_server_nocheckpoint(
        tb_database_t* tdb);

void tb_globalinit(
        void);

void tb_globaldone(
        void);

bool tb_init(
        su_inifile_t* inifile,
        dbe_cryptoparams_t* cp);

#ifdef SS_LOCALSERVER
void tb_setlocal(
        tb_database_t* tbd);

tb_database_t* tb_getlocal(void);
#endif

void tb_done(
        void);

void tb_connect_buildusernamepassword(
        tb_database_t* tdb,
        char* username,
        char* password,
        dstr_t* p_serverusername,
        dynva_t* p_crypt_passw);

tb_connect_t* tb_connect_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line);

tb_connect_t* tb_connect_hsb_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line);

tb_connect_t* tb_connect_admin(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password);

tb_connect_t* tb_connect_console_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line);

tb_connect_t* tb_connect_java_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        char* password,
        char* file,
        int line);

tb_connect_t* tb_connect_server_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        char* password,
        char* file,
        int line);

tb_connect_t* tb_connect_replica_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line);

tb_connect_t* tb_connect_replica_byuserid_ex(
        tb_database_t* tdb,
        int loginid,
        long userid,
        char* file,
        int line);

tb_connect_t *tb_getconnection(
        tb_database_t *tdb);

tb_connect_t* tb_connect_local_ex(
        int loginid,
        char* username,
        char* password,
        char* file,
        int line);

bool tb_rollback_at_disconnect(
        tb_connect_t* tc);

void tb_disconnect(
        tb_connect_t* tc);

void tb_connect_link(
        tb_connect_t* tc);

bool tb_getcryptpwd_by_username(
        tb_connect_t* tc,
        char* username,
        dynva_t* p_cryptpwd_give);

bool tb_checkuser_cryptpwd(
        tb_connect_t* tc,
        char* username,
        va_t* crypt_passw,
        bool* p_isadmin,
        bool* p_isconsole);

bool tb_checkuser(
        tb_connect_t* tc,
        char* username,
        char* password,
        bool* p_isadmin,
        bool* p_isconsole);

bool tb_connect_last_checkpoint_split_log(
        tb_connect_t* tc);

void tb_createcheckpoint_sethsbcopycallback(
        void (*fun)(void));

su_ret_t tb_createcheckpoint(
        tb_connect_t* tc,
        bool splitlog);

su_ret_t tb_createcheckpoint_start_splitlogif(
        tb_connect_t* tc,
        bool splitlog);

su_ret_t tb_createcheckpoint_start(
        tb_connect_t* tc);

su_ret_t tb_createsnapshot_start(
        tb_connect_t* tc);

su_ret_t tb_createcp_step(
        tb_connect_t* tc);

su_ret_t tb_createcp_end(
        tb_connect_t* tc);

rs_sysinfo_t* tb_getclientdata(
        tb_connect_t* tc);

tb_trans_t* tb_getsqltrans(
        tb_connect_t* tc);

sqlsystem_t* tb_getsqls(
        tb_connect_t* tc);

dbe_db_t* tb_getdb(
        tb_connect_t* tc);

dbe_db_t* tb_tabdb_getdb(
        tb_database_t* tdb);

tb_database_t* tb_gettdb(
        tb_connect_t* tc);
                       
long tb_getuserid(
        tb_connect_t* tc);
                       
void tb_setappinfo(
        tb_connect_t* tc,
        char* appinfo);
                       
char* tb_getappinfo(
        tb_connect_t* tc);

int tb_getmaxcmplen(
        tb_database_t* tdb);

void tb_setmaxcmplen(
        tb_database_t* tdb,
        int maxcmplen);

bool tb_getdefaultistransient(
        tb_database_t*  tdb);

bool tb_getdefaultisglobaltemporary(
        tb_database_t*  tdb);

void tb_resetnamebuffers(
        tb_connect_t* tc);
                       
su_rbt_t* tb_getkeynameidrbt(
        tb_connect_t* tc);
                                              
tb_schema_t* tb_getschema(
        tb_connect_t* tc);

void tb_printinfo(
        tb_database_t* tdb,
        void* fp);

void tb_allowsysconnectlogfailure(
        tb_database_t* tdb);

bool tb_connect_logfailureallowed(
        tb_connect_t* tc);

su_list_t* tb_acmd_listinit(
        void);

su_list_t* tb_admincommand(
        rs_sysi_t* cd,
        tb_connect_t* tc,
        char* cmd,
        int* acmdstatus,
        bool* p_finishedp,
        tb_admin_cmd_t* tb_admincmd,
        void** p_ctx);

su_list_t* tb_adminevent(
        rs_sysi_t* cd,
        tb_connect_t* tc,
        char* cmd,
        bool* p_isadmevent,
        bool* p_finishedp,
        bool* p_succp);

void tb_setadmincommandfp(
        tb_connect_t* tc,
        void (*admincommandfp)(
            rs_sysi_t* cd,
            su_list_t* list,
            char* cmd,
            int* acmdstatus,
            bool* p_finishedp,
            tb_admin_cmd_t* tb_admincmd,
            void** p_ctx));

void tb_setadmineventfp(
        tb_connect_t* tc,
        void (*admincommandfp)(rs_sysi_t* cd,
                               su_list_t* list,
                               char* cmd,
                               bool* p_isadmevent,
                               bool* p_finishedp,
                               bool* p_succp));

su_chcollation_t tb_getcollation(
        tb_database_t* tdb);

su_chcollation_t tb_connect_getcollation(
        tb_connect_t* tc);

bool tb_isvalidsetschemaname(
        void* cd,
        tb_trans_t* trans,
        char* username);

void tb_conn_use_hurc(
        tb_connect_t* tc,
        bool usep);

#if defined(DBE_MTFLUSH)

void tb_setflushwakeupcallback(
        tb_database_t* tdb,
        void (*flushwakeupfp)(void*),
        void* flushwakeupctx,
        void  (*flusheventresetfp)(void*),
        void* flusheventresetctx,
        void (*flushbatchwakeupfp)(void*),
        void* flushbatchwakeupctx);


#endif /* DBE_MTFLUSH */

void tb_server_setsysifuns(
        void (*server_sysi_init)(rs_sysi_t* sysi),
        void (*server_sysi_done)(rs_sysi_t* sysi),
        void (*server_sysi_init_functions)(rs_sysi_t* sysi));

void tb_server_setsrvtaskfun(
        void (*server_task_start)(
                 rs_sysinfo_t* cd,
                 SuTaskClassT taskclass,
                 char* task_name,
                 su_task_fun_t task_fun,
                 void* td),
        bool* p_sqlsrv_shutdown_coming);

tb_blobg2mgr_t* tb_connect_getblobg2mgr(
        tb_connect_t* tc);

tb_blobg2mgr_t* tb_database_getblobg2mgr(
        tb_database_t* tdb);

tb_recovctx_t* tb_recovctx_init(
        tb_database_t* tdb);

tb_recovctx_t* tb_recovctx_initbycd(
        tb_database_t* tdb,
        rs_sysi_t* gate_cd);

void tb_recovctx_done(
        tb_recovctx_t* recov_ctx);

rs_relh_t* tb_recovctx_getrelh(
        void* ctx,
        rs_entname_t* en,
        void* priv);

rs_relh_t* tb_recovctx_getrelh_byrelid(
        void* ctx,
        ulong relid);

rs_relh_t* tb_recovctx_getrelh_trxinfo(
        tb_recovctx_t* recov_ctx,
        ulong relid,
        dbe_trxinfo_t* trxinfo,
        dbe_trxid_t readtrxid);

rs_viewh_t* tb_recovctx_getviewh(
        tb_recovctx_t* recov_ctx,
        rs_entname_t* en,
        void* priv);

tb_database_t* tb_recovctx_gettabdb(
        tb_recovctx_t* recov_ctx);

void tb_recovctx_refreshrbuf(
        void* ctx);

void tb_recovctx_refreshrbuf_keepcardinal(
        void *ctx);

void tb_recovctx_fullrefreshrbuf(
        void* ctx);

void tb_reload_rbuf(
        void* ctx);

void tb_reload_rbuf_keepcardinal(
        tb_database_t* tdb,
        rs_sysi_t* gate_cd);

#ifdef SS_HSBG2

void tb_database_set_sysproperties(
        tb_database_t* tbd,
        tb_sysproperties_t* sysproperties);

tb_sysproperties_t* tb_database_get_sysproperties(
        tb_database_t* tbd);

#endif /* SS_HSBG2 */

void tb_database_pmonupdate_nomutex(
        tb_database_t* tdb);

#endif /* TAB0CONN_H */

