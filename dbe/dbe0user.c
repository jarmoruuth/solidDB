/*************************************************************************\
**  source       * dbe0user.c
**  directory    * dbe
**  description  * User functions.
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

This module implements the database user object. The user is bound to
a database object.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

database object     dbe0db.c

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------

void main(void)
{
        dbe_db_t* db;
        dbe_openinfo_t* oi;
        dbe_user_t* user;
        dbe_trx_t* trx;
        rs_cd_t* cd;

        /* init openinfo */
        oi = ...;

        /* init cd */
        cd = ...;

        /* open the database */
        db = dbe_db_init(&oi);

        /* create user */
        user = dbe_user_init(db, cd, "username");

        /* begin transaction */
        trx = dbe_trx_begin(user);

        /* do database operations */
        ...

        /* commit transaction */
        dbe_trx_commit(trx);

        /* kill the user */
        dbe_user_done(user);

        /* update the system variables to the database */
        dbe_db_updateinfo(db);

        /* close the database */
        dbe_db_done(db);
}

**************************************************************************
#endif /* DOCUMENTATION */

#define DBE0USER_C

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>

#include <rs0aval.h>

#include "dbe9type.h"
#include "dbe5inde.h"
#include "dbe4srch.h"
#include "dbe0type.h"
#include "dbe0db.h"
#include "dbe0user.h"

#define dbe_user_sementer(user)    SsSemEnter((user)->usr_sem)
#define dbe_user_semexit(user)     SsSemExit((user)->usr_sem)

/*##**********************************************************************\
 *
 *		dbe_user_init
 *
 * Initializes (connects) an existing user object to the the database
 * object.
 *
 * Parameters :
 *
 *	db - in, use
 *		Database object.
 *
 *	cd - in, use
 *		Client data.
 *
 *	username - in, use
 *		User name.
 *
 * Return value - give :
 *
 *      Pointer to the user object.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_user_t* dbe_user_init(
        dbe_db_t* db,
        void* cd,
        char* username,
        char* file,
        int line)
{
        dbe_user_t* user;

        ss_dassert(db != NULL);
        ss_dassert(username != NULL);

        user = SsMemAlloc(sizeof(dbe_user_t));

        ss_debug(user->usr_chk = DBE_CHK_USER);
        user->usr_db = db;
        user->usr_cd = cd;
        user->usr_name = SsMemStrdup(username);
        user->usr_appinfo = SsMemStrdup((char *)"");
        user->usr_searches = su_list_init(NULL);
        su_list_startrecycle(user->usr_searches);
        user->usr_sem = SsSemCreateLocal(SS_SEMNUM_DBE_USER);
        user->usr_trxsem = rs_sysi_gettrxsem(cd);
        user->usr_trx = NULL;
        user->usr_trxid = DBE_TRXID_NULL;
        user->usr_index = dbe_db_getindex(db);

        user->usr_file = file;
        user->usr_line = line;
        user->usr_sqlstr = NULL;

        user->usr_id = dbe_db_adduser(db, user);

        ss_debug(user->usr_thrid = SsThrGetid());

        return(user);
}

/*##**********************************************************************\
 *
 *		dbe_user_done
 *
 * Releases resources allocated the the user object.
 *
 * Parameters :
 *
 *	user - in, take
 *		User object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_user_done(
        dbe_user_t* user)
{
        dbe_search_t* sea;
        su_list_node_t* n;

        CHK_USER(user);

        dbe_db_removeuser(user->usr_db, user->usr_id);

        su_list_do_get(user->usr_searches, n, sea) {
            dbe_search_done(sea);
        }
        su_list_done(user->usr_searches);

        /* This has to be done after the searches are freed. */
        dbe_mme_removeuser(user->usr_cd, dbe_db_getmme(user->usr_db));

        SsMemFree(user->usr_name);
        SsMemFree(user->usr_appinfo);
        SsSemFree(user->usr_sem);

        SsMemFree(user);
}

/*##**********************************************************************\
 *
 *		dbe_user_setappinfo
 *
 *
 *
 * Parameters :
 *
 *	user -
 *
 *
 *	appinfo -
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
void dbe_user_setappinfo(
        dbe_user_t* user,
        char* appinfo)
{
        char* old_appinfo;

        CHK_USER(user);

        dbe_user_sementer(user);
        old_appinfo = user->usr_appinfo;
        user->usr_appinfo = SsMemStrdup(appinfo);
        dbe_user_semexit(user);

        SsMemFree(old_appinfo);
}

/*##**********************************************************************\
 *
 *		dbe_user_getappinfo
 *
 *
 *
 * Parameters :
 *
 *	user -
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
char* dbe_user_getappinfo(
        dbe_user_t* user)
{
        CHK_USER(user);

        return(user->usr_appinfo);
}

/*##**********************************************************************\
 *
 *		dbe_user_setsqlstr
 *
 *
 *
 * Parameters :
 *
 *	user -
 *
 *
 *	appinfo -
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
void dbe_user_setsqlstr(
        dbe_user_t* user,
        char* sqlstr)
{
        CHK_USER(user);
        ss_dassert(sqlstr == NULL || *sqlstr != '\xff'); /* Check that string is valid. */

        user->usr_sqlstr = sqlstr;
}

char* dbe_user_getsqlstr(
        dbe_user_t* user)
{
        CHK_USER(user);

        return(user->usr_sqlstr);
}

/*##**********************************************************************\
 *
 *		dbe_user_getid
 *
 * Returns the user id.
 *
 * Parameters :
 *
 *	user - in, use
 *		User object.
 *
 * Return value :
 *
 *      User id.
 *
 * Limitations  :
 *
 * Globals used :
 */
int dbe_user_getid(
        dbe_user_t* user)
{
        CHK_USER(user);

        return(rs_sysi_userid(user->usr_cd));
}

/*##**********************************************************************\
 *
 *		dbe_user_addsearch
 *
 * Adds a search to the user structure.
 *
 * Parameters :
 *
 *	user - in out, use
 *		User object.
 *
 *	sea - in, hold
 *		Search object.
 *
 * Return value :
 *
 *      Search id in the user object.
 *
 * Limitations  :
 *
 * Globals used :
 */
su_list_node_t* dbe_user_addsearch(
        dbe_user_t* user,
        dbe_search_t* sea)
{
        su_list_node_t* n;

        CHK_USER(user);

        dbe_user_sementer(user);
        n = su_list_insertlast(user->usr_searches, sea);
        dbe_user_semexit(user);

        dbe_db_searchstarted(user->usr_db);

        return(n);
}

/*##**********************************************************************\
 *
 *		dbe_user_removesearch
 *
 * Removes search identified by seaid from the user object.
 *
 * Parameters :
 *
 *	user - in out, use
 *		User object.
 *
 *	seaid - in
 *		Search id returned by dbe_user_addsearch.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_user_removesearch(
        dbe_user_t* user,
        su_list_node_t* n)
{
        CHK_USER(user);

        dbe_db_searchstopped(user->usr_db);

        dbe_user_sementer(user);
        su_list_remove(user->usr_searches, n);
        dbe_user_semexit(user);
}

/*##**********************************************************************\
 *
 *		dbe_user_checkoutsearches
 *
 * Returns a pointer array of all active searches.
 *
 * Note that the searches pointer array must be released by calling
 * function dbe_user_checkinsearches.
 *
 * Parameters :
 *
 *	user - in, use
 *		User object.
 *
 * Return value - ref :
 *
 *      Pointer array of active searches.
 *
 * Limitations  :
 *
 * Globals used :
 */
su_list_t* dbe_user_checkoutsearches(
        dbe_user_t* user)
{
        CHK_USER(user);

        dbe_user_sementer(user);

        return(user->usr_searches);
}

/*##**********************************************************************\
 *
 *		dbe_user_checkinsearches
 *
 * Releases the exclusive access to the user searches.
 *
 * Parameters :
 *
 *	user - in, use
 *		User object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_user_checkinsearches(
        dbe_user_t* user)
{
        CHK_USER(user);

        dbe_user_semexit(user);
}


/*##**********************************************************************\
 *
 *		dbe_user_restartsearches
 *
 * Restarts user searches with new time constraints. Typically used when
 * cursor is not closed in transaction commit and new transaction starts.
 *
 * Parameters :
 *
 *	user -
 *
 *
 *	trx -
 *
 *
 *	usertrxid -
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
void dbe_user_restartsearches(
        dbe_user_t* user,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid)
{
        su_list_node_t* n;
        dbe_search_t* sea;

        dbe_user_sementer(user);
        su_list_do_get(user->usr_searches, n, sea) {
            dbe_search_restart(sea, trx, dbe_trxnum_null, usertrxid);
        }
        dbe_user_semexit(user);
}

/*##**********************************************************************\
 *
 *		dbe_user_invalidatesearches
 *
 * Invalidates user searches with usertrxid. Typically used when cursor
 * is not closed in transaction commit.
 *
 * Parameters :
 *
 *	user -
 *
 *
 *	maxtrxnum -
 *
 *
 *	usertrxid -
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
void dbe_user_invalidatesearches(
        dbe_user_t* user,
        dbe_trxid_t usertrxid,
        dbe_search_invalidate_t type)
{
        su_list_node_t* n;
        dbe_search_t* sea;

        dbe_user_sementer(user);
        su_list_do_get(user->usr_searches, n, sea) {
            dbe_search_invalidate(sea, usertrxid, type);
        }
        dbe_user_semexit(user);
}

/*##**********************************************************************\
 *
 *		dbe_user_abortsearchesrelid
 *
 *
 *
 * Parameters :
 *
 *	user -
 *
 *
 *	relid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_user_abortsearchesrelid(
        dbe_user_t* user,
        ulong relid)
{
        dbe_search_t* sea;
        su_list_node_t* n;

        dbe_user_sementer(user);
        su_list_do_get(user->usr_searches, n, sea) {
            dbe_search_abortrelid(sea, relid);
        }
        dbe_user_semexit(user);
}

/*##**********************************************************************\
 *
 *		dbe_user_abortsearcheskeyid
 *
 *
 *
 * Parameters :
 *
 *	user -
 *
 *
 *	keyid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_user_abortsearcheskeyid(
        dbe_user_t* user,
        ulong keyid)
{
        dbe_search_t* sea;
        su_list_node_t* n;

        dbe_user_sementer(user);
        su_list_do_get(user->usr_searches, n, sea) {
            dbe_search_abortkeyid(sea, keyid);
        }
        dbe_user_semexit(user);
}

/*##**********************************************************************\
 *
 *		dbe_user_marksearchesrowold
 *
 * Marks current row as old for all searches with same relid and
 * lock name.
 *
 * Parameters :
 *
 *	user -
 *
 *
 *	relid -
 *
 *
 *	lockname -
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
void dbe_user_marksearchesrowold(
        dbe_user_t* user,
        ulong relid,
        dbe_lockname_t lockname)
{
        dbe_search_t* sea;
        su_list_node_t* n;

        dbe_user_sementer(user);
        su_list_do_get(user->usr_searches, n, sea) {
            dbe_search_markrowold(sea, relid, lockname);
        }
        dbe_user_semexit(user);
}

void dbe_user_newplan(
        dbe_user_t* user,
        ulong       relid)
{
        su_list_node_t* n;
        dbe_search_t* sea;

        dbe_user_sementer(user);
        su_list_do_get(user->usr_searches, n, sea) {
            dbe_search_newplan(sea, relid);
        }
        dbe_user_semexit(user);
}

#ifndef SS_LIGHT
/*##**********************************************************************\
 *
 *		dbe_user_printinfo
 *
 *
 *
 * Parameters :
 *
 *	fp -
 *
 *
 *	user -
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
void dbe_user_printinfo(
        void* fp,
        dbe_user_t* user)
{
        dbe_search_t* sea;
        su_list_node_t* n;

        dbe_user_sementer(user);

        SsFprintf(fp, "  User Id %d Name %s Trx id %ld readlevel %ld MergeWrites %d AppInfo %s (%d@%s) SQLstr %s\n",
            rs_sysi_userid(user->usr_cd),
            user->usr_name,
            DBE_TRXID_GETLONG(user->usr_trxid),
            dbe_trx_getreadlevel_long(user->usr_trx),
            rs_sysi_getmergewrites(user->usr_cd),
            user->usr_appinfo,
            user->usr_line,
            user->usr_file,
            user->usr_sqlstr ? user->usr_sqlstr : "NULL");

        SsFprintf(fp, "    USER SEARCHES:\n");
        dbe_search_printinfoheader(fp);
        su_list_do_get(user->usr_searches, n, sea) {
            dbe_search_printinfo(fp, sea);
        }
        dbe_user_semexit(user);
}
#endif /* SS_LIGHT */

