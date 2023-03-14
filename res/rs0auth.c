/*************************************************************************\
**  source       * rs0auth.c
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

#define RS0AUTH_C

#include <ssstdio.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <su0list.h>

#include "rs0types.h"
#include "rs0sdefs.h"
#include "rs0auth.h"

static int pc_insert_compare(void* key1, void* key2)
{
        rs_auth_t* a1 = key1;
        rs_auth_t* a2 = key2;

        return(strcmp(a1->a_username, a2->a_username));
}

static int pc_search_compare(void* search_key, void* rbt_key)
{
        char* username = search_key;
        rs_auth_t* a = rbt_key;

        return(strcmp(username, a->a_username));
}

static void pc_delete(void* key)
{
        rs_auth_done(NULL, key);
}

#ifdef SS_SYNC

typedef struct {
        long su_masterid;
        long su_userid;
} auth_syncuserid_t;

static int syncuserid_compare(void* key1, void* key2)
{
        auth_syncuserid_t* su1 = key1;
        auth_syncuserid_t* su2 = key2;

        return(su_rbt_long_compare(su1->su_masterid, su2->su_masterid));
}

static void syncuserid_delete(void* key)
{
        SsMemFree(key);
}

#endif /* SS_SYNC */

/*##**********************************************************************\
 * 
 *              rs_auth_init
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      username - 
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
rs_auth_t* rs_auth_init(
        void* cd,
        char* username,
        long userid,
        bool isadmin)
{
        rs_auth_t* auth;

        SS_NOTUSED(cd);
        ss_dassert(username != NULL);
        ss_dprintf_1(("rs_auth_init:username=%s, userid=%d\n", username, userid));

        auth = SSMEM_NEW(rs_auth_t);

        auth->a_username = SsMemStrdup(username);
        auth->a_userid = userid;
        auth->a_schemactx = rs_entname_init(RS_AVAL_DEFCATALOG, auth->a_username, NULL);
        auth->a_isadmin = isadmin;
        auth->a_isconsole = FALSE;
        auth->a_issyncadmin = FALSE;
        auth->a_issyncregister = FALSE;
#ifdef SS_SYNC
        auth->a_issystem = 0;
        auth->a_syncuseridrbt = 
            su_rbt_init(syncuserid_compare, syncuserid_delete);

        auth->a_syncuserschanged = FALSE;

#endif /* SS_SYNC */
        auth->a_ignoreloginpriv = FALSE;
        auth->a_privbuf = NULL;
        auth->a_curauth = NULL;
        auth->a_authstack = su_list_init(NULL);
        auth->a_schemastack = su_list_init(NULL);
        auth->a_authrbt = su_rbt_inittwocmp(
                            pc_insert_compare,
                            pc_search_compare,
                            pc_delete);

        return(auth);
}

/*##**********************************************************************\
 * 
 *              rs_auth_done
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
void rs_auth_done(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        su_list_done(auth->a_schemastack);
        su_list_done(auth->a_authstack);
        su_rbt_done(auth->a_authrbt);
        if (auth->a_privbuf != NULL) {
            su_rbt_done(auth->a_privbuf);
        }
        
        ss_dassert(auth->a_schemactx != NULL);
        rs_entname_done(auth->a_schemactx);
        su_rbt_done(auth->a_syncuseridrbt);
        SsMemFree(auth->a_username);
        SsMemFree(auth);
}

/*##**********************************************************************\
 * 
 *              rs_auth_setprivbuf
 * 
 * Sets privilege information to the authorization object.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      auth - use
 *              
 *              
 *      privbuf - in, take
 *              Privilege rb-tree.
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_auth_setprivbuf(
        void*      cd,
        rs_auth_t* auth,
        su_rbt_t*  privbuf)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        auth->a_privbuf = privbuf;
}

/*##**********************************************************************\
 * 
 *              rs_auth_privbuf
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
 * Return value - ref : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_rbt_t* rs_auth_privbuf(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        if (auth->a_curauth == NULL) {
            return(auth->a_privbuf);
        } else {
            return(auth->a_curauth->a_privbuf);
        }
}


/*##**********************************************************************\
 * 
 *              rs_auth_username
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
char* rs_auth_username(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        return(auth->a_username);
}

/*##**********************************************************************\
 * 
 *              rs_auth_userid
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
long rs_auth_userid(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        if (auth->a_curauth == NULL) {
            return(auth->a_userid);
        } else {
            return(auth->a_curauth->a_userid);
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_loginuserid
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
long rs_auth_loginuserid(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        return(auth->a_userid);
}

/*##**********************************************************************\
 * 
 *              rs_auth_setschema
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
 *      schema - 
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
void rs_auth_setschema(
        void*      cd __attribute__ ((unused)),
        rs_auth_t* auth,
        char*      schema)
{
        rs_entname_t* en;
        char* oldcatalog = NULL;
        char* oldschema = NULL;

        ss_dprintf_1(("rs_auth_setschema:%s\n", schema));
        
        if (auth->a_curauth != NULL) {
            auth = auth->a_curauth;
        }

        ss_dassert(auth->a_schemactx != NULL);

        oldcatalog = rs_entname_getcatalog(auth->a_schemactx);
        oldschema = rs_entname_getschema(auth->a_schemactx);
        if (oldschema != NULL && schema != NULL && strcmp(oldschema, schema) == 0) {
            return; /* no change needed */
        }
        en = rs_entname_init(oldcatalog, schema, NULL);
        rs_entname_done(auth->a_schemactx);
        auth->a_schemactx = en;
}       

void rs_auth_setcatalog(
        void*      cd __attribute__ ((unused)),
        rs_auth_t* auth,
        char*      catalog)
{
        rs_entname_t* en;
        char* oldcatalog = NULL;
        char* oldschema = NULL;
        
        ss_dprintf_1(("rs_auth_setcatalog:%s\n", catalog));
        ss_dassert(catalog != NULL);

        if (auth->a_curauth != NULL) {
            auth = auth->a_curauth;
        }
        ss_dassert(auth->a_schemactx != NULL);

        oldcatalog = rs_entname_getcatalog(auth->a_schemactx);
        oldschema = rs_entname_getschema(auth->a_schemactx);
        if (strcmp(oldcatalog, catalog) == 0) {
            return; /* no change needed */
        }
        en = rs_entname_init(catalog, oldschema, NULL);
        rs_entname_done(auth->a_schemactx);
        auth->a_schemactx = en;
}

rs_entname_t* rs_auth_schemacontext(
        void* cd __attribute__ ((unused)),
        rs_auth_t* auth)
{
        ss_dassert(auth != NULL);
        
        if (auth->a_curauth != NULL) {
            auth = auth->a_curauth;
        }
        ss_dassert(auth->a_schemactx != NULL);

        return (auth->a_schemactx);
}

char* rs_auth_catalog(
        void* cd __attribute__ ((unused)),
        rs_auth_t* auth)
{
        ss_dassert(auth != NULL);
        ss_dassert(auth->a_schemactx != NULL);
      
        if (auth->a_curauth != NULL) {
            auth = auth->a_curauth;
        }

        return (rs_entname_getcatalog(auth->a_schemactx));
}

/*##**********************************************************************\
 * 
 *              rs_auth_schema
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
char* rs_auth_schema(
        void*      cd __attribute__ ((unused)),
        rs_auth_t* auth)
{
        if (auth->a_curauth != NULL) {
            auth = auth->a_curauth;
        }
        return (rs_entname_getschema(auth->a_schemactx));
}

/*##**********************************************************************\
 * 
 *              rs_auth_isconsole
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
bool rs_auth_isconsole(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        if (auth->a_curauth == NULL) {
            return(auth->a_isadmin || auth->a_isconsole);
        } else {
            return(auth->a_curauth->a_isadmin || auth->a_curauth->a_isconsole);
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_setconsole
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
void rs_auth_setconsole(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        auth->a_isconsole = TRUE;
}

/*##**********************************************************************\
 * 
 *              rs_auth_issyncadmin
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
bool rs_auth_issyncadmin(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        if (auth->a_curauth == NULL) {
            return(auth->a_isadmin || auth->a_issyncadmin);
        } else {
            return(auth->a_curauth->a_isadmin || auth->a_curauth->a_issyncadmin);
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_setsyncadmin
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
void rs_auth_setsyncadmin(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        auth->a_issyncadmin = TRUE;
}

/*##**********************************************************************\
 * 
 *              rs_auth_issyncregister
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
bool rs_auth_issyncregister(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        if (auth->a_curauth == NULL) {
            return(auth->a_isadmin ||
                   auth->a_issyncadmin ||
                   auth->a_issyncregister);
        } else {
            return(auth->a_curauth->a_isadmin ||
                   auth->a_curauth->a_issyncadmin ||
                   auth->a_curauth->a_issyncregister);
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_setsyncregister
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
void rs_auth_setsyncregister(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        auth->a_issyncregister = TRUE;
}

/*##**********************************************************************\
 * 
 *              rs_auth_push
 * 
 * Pushes a new temporary authorization as the current authorization.
 * This routine can be used to run some routines with a different
 * authorization (e.g. procedures).
 * 
 * Routine fails (returns FALSE) if the user auth context is not added
 * to this auth object using routine rs_auth_addnewauthctx.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      auth - use
 *              
 *              
 *      username - in
 *              
 *              
 * Return value : 
 * 
 *      TRUE    - authorization pushed
 *      FALSE   - unknown user ctx, maybe not added by rs_auth_newauthctx
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool rs_auth_push(
        void*      cd,
        rs_auth_t* auth,
        char*      username)
{
        su_rbt_node_t* n;

        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);
        ss_dprintf_1(("rs_auth_push:username=%s\n", username));

        n = su_rbt_search(auth->a_authrbt, username);
        if (n == NULL) {
            ss_dprintf_2(("rs_auth_push:FAILED\n"));
            return(FALSE);
        } else {

            rs_entname_t* current_en;

            auth->a_curauth = su_rbtnode_getkey(n);
            su_list_insertfirst(auth->a_authstack, auth->a_curauth);

            current_en = rs_entname_copy(auth->a_curauth->a_schemactx);
            su_list_insertfirst(auth->a_schemastack, current_en);

            return(TRUE);
        }
}

/* Call to this in rs_auth_pop resolve TPR 30488
 * if usernames are same.
 */
static bool auth_inherit(
        void*      cd __attribute__ ((unused)),
        rs_auth_t* from_auth,
        rs_auth_t* to_auth)
{
        if (strcmp(from_auth->a_username, to_auth->a_username) == 0) {
            su_rbt_node_t* n;
            su_rbt_node_t* nif;
            auth_syncuserid_t* su;
            n = su_rbt_min(from_auth->a_syncuseridrbt, NULL);
            while (n != NULL) {
                su = (auth_syncuserid_t*)su_rbtnode_getkey(n);

                nif = su_rbt_search(to_auth->a_syncuseridrbt, su);
                if (nif == NULL) {
                    auth_syncuserid_t* suins;
                    suins = SSMEM_NEW(auth_syncuserid_t);
                    suins->su_masterid = su->su_masterid;
                    suins->su_userid = su->su_userid;
                    su_rbt_insert(to_auth->a_syncuseridrbt, suins);
                }
                n = su_rbt_succ(from_auth->a_syncuseridrbt, n);
            }
            return(TRUE);
        }
        return(FALSE);
}


/*##**********************************************************************\
 * 
 *              rs_auth_pop
 * 
 * Pops the authorization information pushed by rs_auth_push. The previously
 * pushed authorization or the original user authorization becomes the
 * new authorization.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      auth - use
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
void rs_auth_pop(
        void*      cd,
        rs_auth_t* auth,
        bool* p_changed)
{
        rs_auth_t* from_auth;
        rs_auth_t* to_auth;
        su_list_node_t* n;

        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);
        ss_dassert(su_list_length(auth->a_authstack) > 0);
        ss_dprintf_1(("rs_auth_pop:listlen=%d\n", su_list_length(auth->a_authstack)));

        from_auth = auth->a_curauth;
        ss_dassert(from_auth != NULL);

        rs_entname_done(from_auth->a_schemactx);
        n = su_list_first(auth->a_schemastack);
        from_auth->a_schemactx = su_listnode_getdata(n);
        su_list_removefirst(auth->a_schemastack);

        su_list_removefirst(auth->a_authstack);
        n = su_list_first(auth->a_authstack);
        if (n == NULL) {
            auth->a_curauth = NULL;
            to_auth = auth;
        } else {
            auth->a_curauth = su_listnode_getdata(n);
            to_auth = auth->a_curauth;
        }
        if (from_auth == auth) {
            ss_dprintf_1(("rs_auth_pop:from_auth == auth\n"));
        }
        *p_changed = FALSE;
        if (from_auth != NULL && from_auth != auth) {
            if (!auth_inherit(cd, from_auth, to_auth)) {
                *p_changed = from_auth->a_syncuserschanged;
            }
            auth->a_syncuserschanged = FALSE;
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_addnewauthctx
 * 
 * Adds a new autohorization context to authorization object. The added
 * context can later be used as a temporary authorization object by
 * function rs_auth_push.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      auth - use
 *              
 *              
 *      newauthctx - in, take
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
void rs_auth_addnewauthctx(
        void*      cd,
        rs_auth_t* auth,
        rs_auth_t* newauthctx)
{
        bool succp;

        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);
        ss_dassert(newauthctx != NULL);
        ss_dprintf_1(("rs_auth_addnewauthctx\n"));

        succp = su_rbt_insert(auth->a_authrbt, newauthctx);
        ss_dassert(succp);
}

/*##**********************************************************************\
 * 
 *              rs_auth_ispushed
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
bool rs_auth_ispushed(
        void*      cd,
        rs_auth_t* auth)
{
        SS_NOTUSED(cd);
        ss_dassert(auth != NULL);

        return(auth->a_curauth != NULL);
}

#ifdef SS_SYNC


/*##**********************************************************************\
 * 
 *          rs_auth_setsystempriv
 * 
 * Sets system privilege ON/OFF to auth object
 * 
 * Parameters : 
 * 
 *      cd - in,use
 *          client data context
 *              
 *      auth - in, use
 *          authorization object
 *              
 *      syspriv - in
 *          TRUE - grant, FALSE - revoke
 *
 * Return value :
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_auth_setsystempriv(
        void* cd __attribute__ ((unused)),
        rs_auth_t* auth,
        bool syspriv)
{
        ss_dassert(auth != NULL);
        if (syspriv) {
            auth->a_issystem++;
        } else {
            ss_dassert(auth->a_issystem > 0);
            auth->a_issystem--;
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_removesyncuserid
 * 
 * Removes sync user id for given master.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      auth - 
 *              
 *              
 *      masterid - 
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
bool rs_auth_removesyncuserid(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_auth_t* auth,
        long masterid)
{
        auth_syncuserid_t search_su;
        su_rbt_node_t* n;

        ss_dprintf_1(("rs_auth_removesyncuserid:%lx:masterid=%ld\n", (long)auth, masterid));
        ss_dassert(auth != NULL);

        search_su.su_masterid = masterid;
        n = su_rbt_search(auth->a_syncuseridrbt, &search_su);
        if (n != NULL) {
            auth_syncuserid_t* found_su;
            found_su = su_rbtnode_getkey(n);
            ss_dprintf_2(("rs_auth_removesyncuserid:%lx:masterid=%ld,uid=%ld\n", (long)auth, masterid, found_su->su_userid));
            su_rbt_delete(auth->a_syncuseridrbt, n);
            if (auth->a_userid != -1) {
                ss_dprintf_1(("rs_auth_removesyncuserid:syncuserschanged:%lx:masterid=%ld,uid=%ld\n", (long)auth, masterid, found_su->su_userid));
                auth->a_syncuserschanged = TRUE;
            }
            return(TRUE);
        } else {
            ss_dprintf_2(("rs_auth_removesyncuserid:%lx:masterid=%ld not found\n", (long)auth, masterid));
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_removeallsyncuserids
 * 
 * 
 * 
 * Parameters : 
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
void rs_auth_removeallsyncuserids(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_auth_t* auth)
{
        ss_dprintf_1(("rs_auth_removeallsyncuserids:%lx\n", (long)auth));
        ss_dassert(auth != NULL);

#ifndef TPR_30488_cont /* next tree lines added Feb 14, 2001 */
        if (auth->a_curauth != NULL) {
            auth = auth->a_curauth;
        }
#endif
        su_rbt_done(auth->a_syncuseridrbt);
        auth->a_syncuseridrbt = 
            su_rbt_init(syncuserid_compare, syncuserid_delete);

        if (auth->a_userid != -1) {
            ss_dprintf_1(("rs_auth_removeallsyncuserids:syncuserschanged\n"));
            auth->a_syncuserschanged = TRUE;
        }
}

/*#***********************************************************************\
 * 
 *              auth_syncuserid
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
 *      masterid - 
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
static long auth_syncuserid(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_auth_t* auth,
        long masterid)
{
        auth_syncuserid_t search_su;
        su_rbt_node_t* n;

        ss_dassert(auth != NULL);

        search_su.su_masterid = masterid;
        n = su_rbt_search(auth->a_syncuseridrbt, &search_su);
        if (n != NULL) {
            auth_syncuserid_t* found_su;
            found_su = su_rbtnode_getkey(n);
            ss_dprintf_2(("rs_auth_syncuserid:%lx:masterid=%ld,uid=%ld\n", (long)auth, masterid, found_su->su_userid));
            return(found_su->su_userid);
        } else {
            ss_dprintf_2(("rs_auth_syncuserid:%lx:masterid=%ld not found\n", (long)auth, masterid));
            return(-1L);
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_addsyncuserid
 * 
 * 
 * 
 * Parameters : 
 * 
 *      auth - 
 *              
 *              
 *      masterid - 
 *              
 *              
 *      userid - 
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
void rs_auth_addsyncuserid(
        rs_sysi_t* cd,
        rs_auth_t* auth,
        long masterid,
        long userid)
{
        ss_dassert(auth != NULL);
        ss_rc_dassert(masterid >= -1L, masterid);

        if (auth->a_curauth != NULL) {
            auth = auth->a_curauth;
        }

        ss_dprintf_1(("rs_auth_addsyncuserid:%lx:masterid=%ld, userid=%ld\n", (long)auth, masterid, userid));

        if (auth_syncuserid(cd, auth, masterid) == -1L) {
            auth_syncuserid_t* su;
            su = SSMEM_NEW(auth_syncuserid_t);
            su->su_masterid = masterid;
            su->su_userid = userid;
            su_rbt_insert(auth->a_syncuseridrbt, su);
            if (auth->a_userid != -1) {
                ss_dprintf_1(("rs_auth_addsyncuserid:syncuserschanged\n"));
                auth->a_syncuserschanged = TRUE;
            }
        }
}

/*##**********************************************************************\
 * 
 *              rs_auth_syncuserid
 * 
 * 
 * 
 * Parameters : 
 * 
 *      auth - 
 *              
 *              
 *      masterid - 
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
long rs_auth_syncuserid(
        rs_sysi_t* cd,
        rs_auth_t* auth,
        long masterid)
{
        long uid;

        ss_dprintf_1(("rs_auth_syncuserid:%lx:masterid=%ld\n", (long)auth, masterid));
        ss_dassert(auth != NULL);

        if (auth->a_curauth != NULL) {
            auth = auth->a_curauth;
        }

        uid = auth_syncuserid(cd, auth, masterid);

        ss_dprintf_2(("rs_auth_syncuserid:%lx:uid=%ld\n", (long)auth, uid));

        return(uid);
}

#endif /* SS_SYNC */

void rs_auth_setignoreloginpriv(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_auth_t* auth,
        bool b)
{
        ss_dprintf_1(("rs_auth_setignoreloginpriv:%d\n", b));
        ss_dassert(auth != NULL);

        auth->a_ignoreloginpriv = b;
}

bool rs_auth_ignoreloginpriv(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_auth_t* auth)
{
        ss_dassert(auth != NULL);
        ss_dprintf_1(("rs_auth_ignoreloginpriv:%d\n", auth->a_ignoreloginpriv));

        return(auth->a_ignoreloginpriv);
}

rs_auth_t* rs_auth_curauth(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_auth_t* auth)
{
        ss_dassert(auth != NULL);

        return (auth->a_curauth == NULL ? auth : auth->a_curauth);
}
