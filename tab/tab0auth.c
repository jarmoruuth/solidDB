/*************************************************************************\
**  source       * tab0auth.c
**  directory    * tab
**  description  * Authorization functions.
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

#include <ssc.h>
#include <ssdebug.h>

#include <dt0date.h>

#include <rs0types.h>
#include <rs0sdefs.h>
#include <rs0auth.h>
#include <rs0sysi.h>
#include <rs0cons.h>

#include <dbe0db.h>

#include "tab1priv.h"
#include "tab1dd.h"
#include "tab0conn.h"
#include "tab0tli.h"
#include "tab0admi.h"
#include "tab0sche.h"
#include "tab0cata.h"

#ifndef SS_MYSQL
#include "tab0sync.h"
#endif /* !SS_MYSQL */

#include "tab0auth.h"

/*#***********************************************************************\
 * 
 *              auth_pushctx
 * 
 * Pushes a new authorization information e.g. for the execution of a
 * procedure. Procedures are executed using the authorization context
 * of the creator of the procedure.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              Client data.
 *              
 *      trans - in
 *              transaction object.
 *              
 *      authname - in
 *              Authorization name.
 *              
 *      p_errh - out, give
 *              Error object is returned in *p_errh if function fails.
 *              
 * Return value : 
 * 
 *      TRUE    - authorization context changed
 *      FALSE   - operation failed, maybe illegal user name
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool auth_pushctx(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* authname,
        char* catalog,
        rs_err_t** p_errh)
{
        bool succp;
        rs_auth_t* auth;

        ss_dprintf_1(("auth_pushctx:authname=%s\n", authname));

        auth = rs_sysi_auth(cd);
        ss_dassert(auth != NULL);

        succp = rs_auth_push(cd, auth, authname);
        if (!succp) {
            /* User not found, maybe the user auth context is not yet
             * added.
             */
            long userid;
            tb_upriv_t upriv;
            rs_auth_t* new_auth;
            TliConnectT* tcon;
            bool isadmin;
            dynva_t password = NULL;

            ss_dprintf_2(("auth_pushctx:find authname context\n"));

            tcon = TliConnectInitByTrans(cd, trans);

            succp = tb_priv_usercheck(
                        tcon,
                        authname,
                        NULL,
                        rs_sysi_getconnecttype(cd) == RS_SYSI_CONNECT_HSB,
                        &userid,
                        &upriv,
                        &password);

            TliConnectDone(tcon);

            if (!succp) {
                rs_error_create(p_errh, E_USERNOTFOUND_S, authname);
                return(FALSE);
            }

            isadmin = (upriv & TB_UPRIV_ADMIN) != 0;

            new_auth = rs_auth_init(
                        cd,
                        authname,
                        userid,
                        isadmin);
            if (upriv & TB_UPRIV_CONSOLE) {
                rs_auth_setconsole(cd, new_auth);
            }
            if (upriv & TB_UPRIV_SYNC_ADMIN) {
                rs_auth_setsyncadmin(cd, new_auth);
            }
            if (upriv & TB_UPRIV_SYNC_REGISTER) {
                rs_auth_setsyncregister(cd, new_auth);
            }
            rs_auth_setcatalog(cd, new_auth, catalog);
            tb_priv_initauth(cd, new_auth);
            rs_auth_addnewauthctx(cd, auth, new_auth);

            {
                char* sync_username;
                va_t* sync_password;

                /* Sync username/password overwrites default setup.
                 */
                if (!rs_sysi_syncusername(cd, &sync_username, &sync_password)) {
                    sync_username = authname;
                    sync_password = password;
                }
                tb_priv_getsyncuserids(
                    cd,
                    new_auth,
                    sync_username,
                    sync_password,
                    trans,
                    rs_sysi_getsyncmasterid(cd));
            }

            succp = rs_auth_push(cd, auth, authname);
            ss_dassert(succp);

            dynva_free(&password);
        } else {
            /* Ensure that initial values are correct.
             */
            rs_auth_setschema(cd, auth, authname);
            rs_auth_setcatalog(cd, auth, catalog);
        }

#ifndef SS_MYSQL
        tb_sync_initcatalog(cd); /* Fix to TPR 30481 */
#endif /* !SS_MYSQL */

        return(TRUE);
}

/*##**********************************************************************\
 * 
 *              tb_auth_pushschemactx
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
 *      schema - 
 *              
 *              
 *      catalog - 
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
bool tb_auth_pushschemactx(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* schema,
        char* catalog,
        rs_err_t** p_errh)
{
        bool succp;
        char* username = NULL;

        catalog = tb_catalog_resolve(cd, catalog);

        succp = tb_schema_maptouser(
                    cd,
                    trans,
                    schema,
                    catalog,
                    NULL,
                    &username);
        if (succp) {
            schema = username;
        }
        succp = auth_pushctx(cd, trans, schema, catalog, p_errh);
        if (username != NULL) {
            SsMemFree(username);
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *              tb_auth_popctx
 * 
 * Pops authorization context pushed by tb_auth_pushctx.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              Client data.
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void tb_auth_popctx(
        rs_sysi_t* cd)
{
        rs_auth_t* auth;
        bool authchanged;

        ss_dprintf_1(("tb_auth_popctx\n"));

        auth = rs_sysi_auth(cd);
        ss_dassert(auth != NULL);

        rs_auth_pop(cd, auth, &authchanged);

#ifndef SS_MYSQL
        if (authchanged) {
            ss_dprintf_1(("tb_auth_popctx:tb_sync_initcatalog_force\n"));
            tb_sync_initcatalog_force(cd);
        } else {
            ss_dprintf_1(("tb_auth_popctx:tb_sync_initcatalog\n"));
            tb_sync_initcatalog(cd); /* Fix to TPR 30481 */
        }
#endif /* !SS_MYSQL */
}

/*##**********************************************************************\
 * 
 *              tb_auth_ispushed
 * 
 * Checks if new auth context is pushed by auth_pushctx.
 * 
 * Parameters : 
 * 
 *      cd - 
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
bool tb_auth_ispushed(
        rs_sysi_t* cd)
{
        rs_auth_t* auth;

        ss_dprintf_1(("tb_auth_ispushed\n"));

        auth = rs_sysi_auth(cd);
        ss_dassert(auth != NULL);

        return(rs_auth_ispushed(cd, auth));
}

/*##**********************************************************************\
 * 
 *              tb_auth_getusername
 * 
 * Returns user name from user id.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      trans - 
 *              
 *              
 *      uid - 
 *              
 *              
 *      p_username - out, give
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
bool tb_auth_getusername(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long uid,
        char** p_username,
        rs_err_t** p_errh)
{
        bool succp;
        TliConnectT* tcon;

        tcon = TliConnectInitByTrans(cd, trans);

        succp = tb_priv_usercheck_byid(
                    tcon,
                    uid,
                    p_username,
                    NULL,
                    NULL);

        TliConnectDone(tcon);

        if (!succp) {
            rs_error_create(p_errh, E_USERIDNOTFOUND_D, uid);
        }

        return(succp);
}
