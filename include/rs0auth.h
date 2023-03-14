/*************************************************************************\
**  source       * rs1auth.h
**  directory    * res
**  description  * Authorization object
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


#ifndef RS0AUTH_H
#define RS0AUTH_H

#include <su0rbtr.h>

#include "rs0types.h"
#include "rs0entna.h"

/* rs_auth_t object is intended to be attached to every object in
   table-level. It could contain authorization and security info about

        - creator
        - user
        - ...
        
*/

rs_auth_t* rs_auth_init(
        void* cd,
        char* username,
        long userid,
        bool isadmin);

void rs_auth_done(
        void*      cd,
        rs_auth_t* auth);

void rs_auth_setprivbuf(
        void*      cd,
        rs_auth_t* auth,
        su_rbt_t*  privbuf);

su_rbt_t* rs_auth_privbuf(
        void*      cd,
        rs_auth_t* auth);

char* rs_auth_username(
        void*      cd,
        rs_auth_t* auth);

long rs_auth_userid(
        void*      cd,
        rs_auth_t* auth);

long rs_auth_loginuserid(
        void*      cd,
        rs_auth_t* auth);

void rs_auth_setschema(
        void*      cd,
        rs_auth_t* auth,
        char*      schema);

bool rs_auth_issetschema(
        void*      cd,
        rs_auth_t* auth);

char* rs_auth_schema(
        void*      cd,
        rs_auth_t* auth);

SS_INLINE bool rs_auth_isadmin(
        void*      cd,
        rs_auth_t* auth);

bool rs_auth_isconsole(
        void*      cd,
        rs_auth_t* auth);

void rs_auth_setconsole(
        void*      cd,
        rs_auth_t* auth);

bool rs_auth_issyncadmin(
        void*      cd,
        rs_auth_t* auth);

void rs_auth_setsyncadmin(
        void*      cd,
        rs_auth_t* auth);

bool rs_auth_issyncregister(
        void*      cd,
        rs_auth_t* auth);

void rs_auth_setsyncregister(
        void*      cd,
        rs_auth_t* auth);

bool rs_auth_push(
        void*      cd,
        rs_auth_t* auth,
        char*      username);

void rs_auth_pop(
        void*      cd,
        rs_auth_t* auth,
        bool* p_changed);

void rs_auth_addnewauthctx(
        void*      cd,
        rs_auth_t* auth,
        rs_auth_t* newauthctx);

bool rs_auth_ispushed(
        void*      cd,
        rs_auth_t* auth);

#ifdef SS_SYNC

void rs_auth_setsystempriv(
        void* cd,
        rs_auth_t* auth,
        bool syspriv);

SS_INLINE bool rs_auth_issystempriv(
        void* cd,
        rs_auth_t* auth);

bool rs_auth_removesyncuserid(
        rs_sysi_t* cd,
        rs_auth_t* auth,
        long masterid);

void rs_auth_removeallsyncuserids(
        rs_sysi_t* cd,
        rs_auth_t* auth);

void rs_auth_addsyncuserid(
        rs_sysi_t* cd,
        rs_auth_t* auth,
        long masterid,
        long userid);

long rs_auth_syncuserid(
        rs_sysi_t* cd,
        rs_auth_t* auth,
        long masterid);

#endif /* SS_SYNC */

void rs_auth_setignoreloginpriv(
        rs_sysi_t* cd,
        rs_auth_t* auth,
        bool b);

bool rs_auth_ignoreloginpriv(
        rs_sysi_t* cd,
        rs_auth_t* auth);

char* rs_auth_catalog(
        void* cd,
        rs_auth_t* auth);

void rs_auth_setcatalog(
        void*      cd,
        rs_auth_t* auth,
        char*      catalog);

rs_entname_t* rs_auth_schemacontext(
        void* cd,
        rs_auth_t* auth);

rs_auth_t* rs_auth_curauth(
        rs_sysi_t* cd,
        rs_auth_t* auth);

struct rsauthstruct {
        char*       a_username;
        long        a_userid;
        rs_entname_t* a_schemactx;
        bool        a_isadmin;
        bool        a_isconsole;
        bool        a_issyncadmin;
        bool        a_issyncregister;
#ifdef SS_SYNC
        int         a_issystem;     /* temporarily grant system privileges */
        su_rbt_t*   a_syncuseridrbt;
        bool        a_syncuserschanged;
#endif /* SS_SYNC */
        bool        a_ignoreloginpriv;
        su_rbt_t*   a_privbuf;
        rs_auth_t*  a_curauth;
        su_list_t*  a_schemastack;  /* Stack of catalog/schema information. */
        su_list_t*  a_authstack;    /* Stack of privilege context information. */
        su_rbt_t*   a_authrbt;      /* Different privilege contexes for
                                       this user. Used during procedure
                                       execution. Procedures are executed
                                       in the context of the creator. */
};

#if defined(RS0AUTH_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *              rs_auth_isadmin
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      auth - 
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
SS_INLINE bool rs_auth_isadmin(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        if (auth->a_issystem) {
            return(TRUE);
        } else if (auth->a_curauth == NULL) {
            return(auth->a_isadmin);
        } else {
            return(auth->a_curauth->a_isadmin);
        }
}

/*##**********************************************************************\
 * 
 *          rs_auth_issystempriv
 * 
 * tells if system privileges have been granted to this auth object
 * 
 * Parameters : 
 * 
 *      cd - in,use
 *          client data context
 *              
 *      auth - in, use
 *          authorization object
 *              
 * Return value :
 *      TRUE - system privilege granted
 *      FALSE - not granted 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE bool rs_auth_issystempriv(
        void* cd __attribute__ ((unused)),
        rs_auth_t* auth)
{
        ss_dassert(auth != NULL);
        return (auth->a_issystem != 0);
}

#endif /* defined(RS0AUTH_C) || defined(SS_USE_INLINE) */

#endif /* RS0AUTH_H */
