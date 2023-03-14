/*************************************************************************\
**  source       * tab0opensrc.c
**  directory    * tab
**  description  * TAB-level stubs for solidDB opensource
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


#include <ssstring.h>
#include <sstime.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssutf.h>
    
#include <uti0vcmp.h>

#include <su0rbtr.h>
#include <su0sdefs.h>

#include <rs0sdefs.h>
#include <rs0sysi.h>
#include <rs0auth.h>
#include <rs0error.h>

#include "tab1priv.h"
#include "tab1dd.h"
#include "tab0publ.h"

#define SU_CRYPT_REQUIRED_DATABUFSIZE(datalen) ((((datalen)+7)/8)*8)

#define SYSREL_PRIVILEGES   (TB_PRIV_SELECT|TB_PRIV_REFERENCES)

struct tb_relpriv_st {
        char* dummy;
};

/*##**********************************************************************\
 *
 *		tb_priv_cryptpassword
 *
 *
 *
 * Parameters :
 *
 *	password -
 *
 *
 *	p_crypt_passw -
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
void tb_priv_cryptpassword(
        char* password __attribute__ ((unused)),
        dynva_t* p_crypt_passw)
{
        *p_crypt_passw = NULL;
}

bool tb_priv_checkusernamepassword(
        char* username __attribute__ ((unused)),
        char* password __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

void tb_priv_done(tb_relpriv_t* priv __attribute__ ((unused)))
{
}

bool tb_priv_usercreate(
        TliConnectT* tcon,
        char* username,
        char* password,
        tb_upriv_t upriv,
        long* p_userid,
        rs_err_t **p_errh)
{
        TliCursorT* tcur;
        TliRetT trc;
        long uid;
        char* type = RS_AVAL_USER;
        dbe_db_t* db = TliGetDb(tcon);
        dynva_t crypt_passw = NULL;
        tb_trans_t* trans;
        dbe_trx_t* trx;
        void* cd;
        su_ret_t rc;
        long userpriv;
        long secval;
        char* passw_data;
        int passw_len;
        char* login_catalog = RS_AVAL_DEFCATALOG;

        ss_dprintf_3(("tb_priv_usercreate\n"));
        
        ss_dassert(tcon != NULL);
        ss_dassert(username != NULL);
        ss_dassert(p_userid != NULL);

        uid = dbe_db_getnewuserid_log(db);
        userpriv = upriv;
        secval = RS_AVAL_USERISPUBLIC;

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               RS_AVAL_SYSNAME,
                               RS_RELNAME_USERS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, RS_ANAME_USERS_ID, &uid);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_USERS_NAME, &username);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_USERS_TYPE, &type);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_USERS_PRIV, &userpriv);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColLong(tcur, RS_ANAME_USERS_PRIVATE, &secval);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColVa(tcur, RS_ANAME_USERS_PASSW, &crypt_passw);
        ss_dassert(trc == TLI_RC_SUCC);
        /* always default to RS_AVAL_DEFCATALOG as login catalog */
        trc = TliCursorColStr(tcur, RS_ANAME_USERS_LOGIN_CATALOG, &login_catalog);

        passw_len = 16;
        passw_data = SsMemAlloc(passw_len);
        passw_data[0] =  (char)-113;
        passw_data[1] =  (char)-71;
        passw_data[2] =  (char)34;
        passw_data[3] =  (char)64;
        passw_data[4] =  (char)69;
        passw_data[5] =  (char)-46;
        passw_data[6] =  (char)15;
        passw_data[7] =  (char)-8;
        passw_data[8] =  (char)-59;
        passw_data[9] =  (char)-53;
        passw_data[10] = (char)74;
        passw_data[11] = (char)100;
        passw_data[12] = (char)-95;
        passw_data[13] = (char)117;
        passw_data[14] = (char)71;
        passw_data[15] = (char)-61;

        dynva_setdata(&crypt_passw, passw_data, passw_len);

        trc = TliCursorInsert(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        TliCursorFree(tcur);

        SsMemFree(passw_data);
        dynva_free(&crypt_passw);

        *p_userid = uid;

        cd = TliGetCd(tcon);
        trans = TliGetTrans(tcon);
        ss_dassert(trans != NULL);
        trx = tb_trans_dbtrx(cd, trans);

        rc = dbe_trx_createuser(trx);
        if (rc == SU_SUCCESS) {
            return(TRUE);
        } else {
            rs_error_create(p_errh, rc);
            return(FALSE);
        }
}

bool tb_priv_userdrop(
        TliConnectT* tcon __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_userchangepassword(
        TliConnectT* tcon __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        char* new_password __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_syncusermap_create(
        TliConnectT* tcon __attribute__ ((unused)),
        char* user_name __attribute__ ((unused)),
        char* master_name __attribute__ ((unused)),
        char* master_user __attribute__ ((unused)),
        char* master_pwd __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_syncusermap_drop(
        TliConnectT* tcon __attribute__ ((unused)),
        char* user_name __attribute__ ((unused)),
        char* master_name __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_setuserprivate(
        TliConnectT* tcon __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        bool setprivate __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_setreplicaaccessonly(
        TliConnectT* tcon __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        bool setp __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_usercheck(
        TliConnectT* tcon __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        va_t* password __attribute__ ((unused)),
        bool hsbconnect __attribute__ ((unused)),
        long* p_uid,
        tb_upriv_t* p_upriv,
        dynva_t* p_give_password __attribute__ ((unused)))
{
        if (p_uid != NULL) {
            *p_uid = 0L;
        }

        if (p_upriv != NULL) {
            *p_upriv = (tb_upriv_t)0;
        }

        return(TRUE);
}

bool tb_priv_usercheck_byid(
        TliConnectT* tcon __attribute__ ((unused)),
        long uid __attribute__ ((unused)),
        char** p_username __attribute__ ((unused)),
        dynva_t* p_password __attribute__ ((unused)),
        tb_upriv_t* p_upriv __attribute__ ((unused)))
{
        return(TRUE);
}

void tb_priv_getsyncuserids(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_auth_t* auth __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        va_t* password __attribute__ ((unused)),
        tb_trans_t* trans __attribute__ ((unused)),
        long masterid __attribute__ ((unused)))
{
}

long tb_priv_getuid(
        TliConnectT* tcon __attribute__ ((unused)),
        char* username __attribute__ ((unused)))
{
        return(0L);
}


bool tb_priv_isuser(
        TliConnectT* tcon __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        long* p_uid __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_isrole(
        TliConnectT* tcon __attribute__ ((unused)),
        char* rolename __attribute__ ((unused)),
        long* p_rid __attribute__ ((unused)))
{
        return(FALSE);
}

bool tb_priv_rolecreate(
        TliConnectT* tcon __attribute__ ((unused)),
        char* rolename __attribute__ ((unused)),
        tb_upriv_t upriv __attribute__ ((unused)),
        long* p_roleid __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_roledrop(
        TliConnectT* tcon __attribute__ ((unused)),
        char* rolename __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_roleadduser(
        TliConnectT* tcon __attribute__ ((unused)),
        char* rolename __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_roleremoveuser(
        TliConnectT* tcon __attribute__ ((unused)),
        char* rolename __attribute__ ((unused)),
        char* username __attribute__ ((unused)),
        rs_err_t **p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_droprelpriv(
        TliConnectT* tcon __attribute__ ((unused)),
        long relid __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_setrelpriv_ex(
        TliConnectT* tcon __attribute__ ((unused)),
        bool revoke __attribute__ ((unused)),
        long grant_id __attribute__ ((unused)),
        bool grant_opt __attribute__ ((unused)),
        long id __attribute__ ((unused)),
        tb_priv_t tbpriv __attribute__ ((unused)),
        long* uid_array __attribute__ ((unused)),
        bool syscall __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_setrelpriv(
        TliConnectT* tcon __attribute__ ((unused)),
        bool revoke __attribute__ ((unused)),
        long grant_id __attribute__ ((unused)),
        bool grant_opt __attribute__ ((unused)),
        long id __attribute__ ((unused)),
        tb_priv_t tbpriv __attribute__ ((unused)),
        long* uid_array __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_setattrpriv(
        TliConnectT* tcon __attribute__ ((unused)),
        bool revoke __attribute__ ((unused)),
        long grant_id __attribute__ ((unused)),
        long id __attribute__ ((unused)),
        tb_priv_t tbpriv __attribute__ ((unused)),
        long* uid_array __attribute__ ((unused)),
        rs_ano_t* ano_array __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

void tb_priv_getrelpriv(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        ulong relorviewid __attribute__ ((unused)),
        bool sysrelorview __attribute__ ((unused)),
        bool relp __attribute__ ((unused)),
        tb_relpriv_t** p_relpriv __attribute__ ((unused)))
{
}

bool tb_priv_getrelprivfromview(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        tb_trans_t* trans __attribute__ ((unused)),
        rs_entname_t* en __attribute__ ((unused)),
        ulong relid __attribute__ ((unused)),
        bool sysrel __attribute__ ((unused)),
        bool relp __attribute__ ((unused)),
        ulong viewid __attribute__ ((unused)),
        char* view_creator __attribute__ ((unused)),
        tb_relpriv_t* viewpriv __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

void tb_priv_createrelorview(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        ulong relorviewid __attribute__ ((unused)),
        tb_priv_t priv __attribute__ ((unused)),
        bool grant_opt __attribute__ ((unused)))
{
}

bool tb_priv_iscreatorrelpriv(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        tb_relpriv_t* relpriv __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_issomerelpriv(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        tb_relpriv_t* relpriv __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_isrelpriv(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        tb_relpriv_t* relpriv __attribute__ ((unused)),
        tb_priv_t checkpriv __attribute__ ((unused)),
        bool sysid __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_priv_isrelgrantopt(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        ulong relorviewid __attribute__ ((unused)),
        bool sysrelorview __attribute__ ((unused)),
        bool relp __attribute__ ((unused)),
        tb_priv_t checkpriv __attribute__ ((unused)))
{
        return(FALSE);
}

bool tb_priv_issomeattrpriv(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        tb_relpriv_t* relpriv __attribute__ ((unused)))
{
        return(FALSE);
}

bool tb_priv_isattrpriv(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        tb_relpriv_t* relpriv __attribute__ ((unused)),
        uint ano __attribute__ ((unused)),
        tb_priv_t checkpriv __attribute__ ((unused)))
{
        return(FALSE);
}

void tb_priv_initauth(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        rs_auth_t* auth __attribute__ ((unused)))
{
}

void tb_priv_initsysrel(rs_sysinfo_t* cd __attribute__ ((unused)))
{
}

void tb_priv_updatesysrel(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        su_rbt_t* newrel_rbt __attribute__ ((unused)))
{
}

bool tb_priv_checkadminaccess(
        rs_sysi_t* cd __attribute__ ((unused)),
        char* relname __attribute__ ((unused)),
        bool* p_isaccess __attribute__ ((unused)))
{
        return(FALSE);
}

bool tb_priv_checkschemaforcreateobj(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_trans_t* trans __attribute__ ((unused)),
        rs_entname_t* en __attribute__ ((unused)),
        long* p_userid __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_proc_drop(
        void* cd __attribute__ ((unused)),
        tb_trans_t* trans __attribute__ ((unused)),
        char* procname __attribute__ ((unused)),
        char* procschema __attribute__ ((unused)),
        char* proccatalog __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_event_drop(
        void* cd __attribute__ ((unused)),
        tb_trans_t* trans __attribute__ ((unused)),
        char* eventname __attribute__ ((unused)),
        char* eventschema __attribute__ ((unused)),
        char* eventcatalog __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

bool tb_dropview(
        void*       cd __attribute__ ((unused)),
        tb_trans_t* trans __attribute__ ((unused)),
        char*       viewname __attribute__ ((unused)),
        char*       authid __attribute__ ((unused)),
        char*       catalog __attribute__ ((unused)),
        char*       extrainfo __attribute__ ((unused)),
        bool        cascade __attribute__ ((unused)),
        void**      cont __attribute__ ((unused)),
        rs_err_t**  p_errh __attribute__ ((unused)))
{
        return(TRUE);
}

dd_updatestmt_t* tb_dd_syncsequencedefs()
{
        return(NULL);
}

dd_updatestmt_t* tb_dd_updatestmts_be()
{
        return(NULL);
}

bool tb_publ_drop(
        void* cd __attribute__ ((unused)),
        tb_trans_t* trans __attribute__ ((unused)),
        char* pubname __attribute__ ((unused)),
        char* schema __attribute__ ((unused)),
        char* catalog __attribute__ ((unused)),
        char* extrainfo __attribute__ ((unused)),
        void** cont __attribute__ ((unused)),
        su_err_t** err __attribute__ ((unused)))
{
        return(TRUE);
}

char* tb_catalog_resolve_withschema(
        rs_sysi_t* cd,
        char* catalog,
        char* schema)
{
        char* newcatalog;

        if (catalog == NULL) {
            if (schema != NULL && RS_SDEFS_ISSYSSCHEMA(schema)) {
                newcatalog = RS_AVAL_DEFCATALOG;
            } else {
                newcatalog = rs_auth_catalog(cd, rs_sysi_auth(cd));
            }
        } else if (catalog[0] == '\0') {
            newcatalog = RS_AVAL_DEFCATALOG;
        } else {
            newcatalog = catalog;
        }
        ss_dprintf_1(("tb_catalog_resolve:%s->%s\n",
            catalog != NULL ? catalog : "NULL", newcatalog));
        return(newcatalog);
}

char* tb_catalog_resolve(rs_sysi_t* cd, char* catalog)
{
        return(tb_catalog_resolve_withschema(cd, catalog, NULL));
}

