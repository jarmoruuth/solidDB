/*************************************************************************\
**  source       * tab1priv.h
**  directory    * tab
**  description  * Security functions.
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


#ifndef TAB1PRIV_H
#define TAB1PRIV_H

#include <ssc.h>

#include <uti0va.h>

#include <su0rbtr.h>

#include <rs0error.h>
#include <rs0atype.h>
#include <rs0sysi.h>
#include <rs0auth.h>

#include "tab0type.h"
#include "tab0tli.h"

#define PUBLIC_UID      0
#define PUBLIC_USERNAME "PUBLIC"

void tb_priv_cryptpassword(
        char* password,
        dynva_t* p_crypt_passw);

bool tb_priv_checkusernamepassword(
        char* username,
        char* password,
        rs_err_t **p_errh);

bool tb_priv_usercreate(
        TliConnectT* tcon,
        char* username,
        char* password,
        tb_upriv_t upriv,
        long* p_userid,
        rs_err_t **p_errh);

bool tb_priv_userdrop(
        TliConnectT* tcon,
        char* username,
        rs_err_t **p_errh);

bool tb_priv_userchangepassword(
        TliConnectT* tcon,
        char* username,
        char* new_password,
        rs_err_t **p_errh);

bool tb_priv_syncusermap_create(
        TliConnectT* tcon,
        char* user_name,
        char* master_name,
        char* master_user,
        char* master_pwd,
        rs_err_t **p_errh);

bool tb_priv_syncusermap_drop(
        TliConnectT* tcon,
        char* user_name,
        char* master_name,
        rs_err_t **p_errh);

bool tb_priv_setreplicaaccessonly(
        TliConnectT* tcon,
        char* username,
        bool setp,
        rs_err_t **p_errh);

bool tb_priv_usercheck(
        TliConnectT* tcon,
        char* username,
        va_t* password,
        bool hsbconnect,
        long* p_uid,
        tb_upriv_t* p_upriv,
        dynva_t* p_give_password);

bool tb_priv_usercheck_byid(
        TliConnectT* tcon,
        long uid,
        char** p_username,
        dynva_t* p_password,
        tb_upriv_t* p_upriv);

void tb_priv_getsyncuserids(
        rs_sysi_t* cd,
        rs_auth_t* auth,
        char* username,
        va_t* password,
        tb_trans_t* trans,
        long masterid);

bool tb_priv_setuserprivate(
        TliConnectT* tcon,
        char* user_name,
        bool setprivate,
        rs_err_t **p_errh);

long tb_priv_getuid(
        TliConnectT* tcon,
        char* username);

bool tb_priv_isuser(
        TliConnectT* tcon,
        char* username,
        long* p_uid);

bool tb_priv_isrole(
        TliConnectT* tcon,
        char* rolename,
        long* p_rid);

bool tb_priv_rolecreate(
        TliConnectT* tcon,
        char* rolename,
        tb_upriv_t upriv,
        long* p_roleid,
        rs_err_t **p_errh);

bool tb_priv_roledrop(
        TliConnectT* tcon,
        char* rolename,
        rs_err_t **p_errh);

bool tb_priv_roleadduser(
        TliConnectT* tcon,
        char* rolename,
        char* username,
        rs_err_t **p_errh);

bool tb_priv_roleremoveuser(
        TliConnectT* tcon,
        char* rolename,
        char* username,
        rs_err_t **p_errh);

bool tb_priv_droprelpriv(
        TliConnectT* tcon,
        long relid,
	rs_err_t** p_errh);

bool tb_priv_setrelpriv(
        TliConnectT* tcon,
        bool revoke,
        long grant_id,
        bool grant_opt,
        long id,
        tb_priv_t tbpriv,
        long* uid_array,
	rs_err_t** p_errh);

bool tb_priv_setrelpriv_ex(
        TliConnectT* tcon,
        bool revoke,
        long grant_id,
        bool grant_opt,
        long id,
        tb_priv_t tbpriv,
        long* uid_array,
        bool syscall,
	rs_err_t** p_errh);

bool tb_priv_setattrpriv(
        TliConnectT* tcon,
        bool revoke,
        long grant_id,
        long id,
        tb_priv_t tbpriv,
        long* uid_array,
        rs_ano_t* ano_array,
	rs_err_t** p_errh);

void tb_priv_getrelpriv(
        rs_sysinfo_t* cd,
        ulong relorviewid,
        bool sysrelorview,
        bool relp,
        tb_relpriv_t** p_relpriv);

bool tb_priv_getrelprivfromview(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_entname_t* en,
        ulong relid,
        bool sysrel,
        bool relp,
        ulong viewid,
        char* view_creator,
        tb_relpriv_t* viewpriv,
        rs_err_t** p_errh);

void tb_priv_createrelorview(
        rs_sysinfo_t* cd,
        ulong relorviewid,
        tb_priv_t priv,
        bool grant_opt);

bool tb_priv_iscreatorrelpriv(
        rs_sysinfo_t* cd,
        tb_relpriv_t* relpriv);

bool tb_priv_issomerelpriv(
        rs_sysinfo_t* cd,
        tb_relpriv_t* relpriv);

bool tb_priv_isrelpriv(
        rs_sysinfo_t* cd,
        tb_relpriv_t* relpriv,
        tb_priv_t checkpriv,
        bool sysid);

bool tb_priv_isrelgrantopt(
        rs_sysinfo_t* cd,
        ulong relorviewid,
        bool sysrelorview,
        bool relp,
        tb_priv_t checkpriv);

bool tb_priv_issomeattrpriv(
        rs_sysinfo_t* cd,
        tb_relpriv_t* relpriv);

bool tb_priv_isattrpriv(
        rs_sysinfo_t* cd,
        tb_relpriv_t* relpriv,
        uint ano,
        tb_priv_t checkpriv);

void tb_priv_initauth(
        rs_sysinfo_t* cd,
        rs_auth_t* auth);

void tb_priv_initsysrel(
        rs_sysinfo_t* cd);

void tb_priv_updatesysrel(
        rs_sysinfo_t* cd,
        su_rbt_t* newrel_rbt);

bool tb_priv_checkadminaccess(
        rs_sysi_t* cd,
        char* relname,
        bool* p_isaccess);

bool tb_priv_checkschemaforcreateobj(
	rs_sysi_t* cd,
	tb_trans_t* trans,
        rs_entname_t* en,
        long* p_userid,
	rs_err_t** p_errh);

#endif /* TAB1PRIV_H */
