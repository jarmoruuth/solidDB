/*************************************************************************\
**  source       * dbe0user.h
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


#ifndef DBE0USER_H
#define DBE0USER_H

#include <ssc.h>
#include <sssem.h>

#include <su0list.h>

#include <rs0sysi.h>

#include "dbe0type.h"
#include "dbe0trx.h"
#include "dbe0db.h"

#define CHK_USER(u) ss_dassert(SS_CHKPTR(u) && (u)->usr_chk == DBE_CHK_USER)

/* User structure.
*/
struct dbe_user_st {
        ss_debug(int usr_chk;)
        dbe_db_t*    usr_db;        /* Database object where the user is
                                       connected. */
        uint         usr_id;
        rs_sysi_t*   usr_cd;        /* User client data. */
        char*        usr_name;      /* User name. */
        char*        usr_appinfo;   /* Application info. */
        su_list_t*   usr_searches;  /* Active searches. */
        dbe_trx_t*   usr_trx;       /* Current transaction. */
        SsSemT*      usr_sem;
        SsSemT*      usr_trxsem;
        dbe_trxid_t  usr_trxid;
        dbe_index_t* usr_index;
        char*        usr_file;
        int          usr_line;
        char*        usr_sqlstr;   /* SQL info. */
        ss_debug(int usr_thrid;)
};

dbe_user_t* dbe_user_init(
        dbe_db_t* db,
        void* cd,
        char* username,
        char* file,
        int line);

void dbe_user_done(
        dbe_user_t* user);

SS_INLINE SsSemT* dbe_user_gettrxsem(
        dbe_user_t* user);

void dbe_user_setappinfo(
        dbe_user_t* user,
        char* appinfo);

char* dbe_user_getappinfo(
        dbe_user_t* user);

void dbe_user_setsqlstr(
        dbe_user_t* user,
        char* sqlstr);

char* dbe_user_getsqlstr(
        dbe_user_t* user);

int dbe_user_getid(
        dbe_user_t* user);

SS_INLINE dbe_db_t* dbe_user_getdb(
        dbe_user_t* user);

SS_INLINE rs_sysi_t* dbe_user_getcd(
        dbe_user_t* user);

SS_INLINE dbe_index_t* dbe_user_getindex(
        dbe_user_t* user);

SS_INLINE void dbe_user_settrx(
        dbe_user_t* user,
        dbe_trx_t* trx);

SS_INLINE dbe_trx_t* dbe_user_gettrx(
        dbe_user_t* user);

su_list_node_t* dbe_user_addsearch(
        dbe_user_t* user,
        dbe_search_t* sea);

void dbe_user_removesearch(
        dbe_user_t* user,
        su_list_node_t* seaid);

su_list_t* dbe_user_checkoutsearches(
        dbe_user_t* user);

void dbe_user_checkinsearches(
        dbe_user_t* user);

void dbe_user_invalidatesearches(
        dbe_user_t* user,
        dbe_trxid_t usertrxid,
        dbe_search_invalidate_t type);

void dbe_user_restartsearches(
        dbe_user_t* user,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid);

void dbe_user_abortsearchesrelid(
        dbe_user_t* user,
        ulong relid);

void dbe_user_abortsearcheskeyid(
        dbe_user_t* user,
        ulong keyid);

void dbe_user_marksearchesrowold(
        dbe_user_t* user,
        ulong relid,
        dbe_lockname_t lockname);

void dbe_user_newplan(
        dbe_user_t* user,
        ulong       relid);

void dbe_user_printinfo(
        void* fp,
        dbe_user_t* user);

#if defined(DBE0USER_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_user_getdb
 *
 * Returns the database object where the user is connected.
 *
 * Parameters :
 *
 *	user - in, use
 *		User object.
 *
 * Return value - ref :
 *
 *      Pointer to the database object.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_db_t* dbe_user_getdb(
        dbe_user_t* user)
{
        CHK_USER(user);

        return(user->usr_db);
}

/*##**********************************************************************\
 *
 *		dbe_user_getcd
 *
 * Returns the client data object that is associated to the user when
 * the user object was created.
 *
 * Parameters :
 *
 *	user - in, use
 *		User object.
 *
 * Return value - ref :
 *
 *      Pointer to the client data object.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE rs_sysi_t* dbe_user_getcd(
        dbe_user_t* user)
{
        CHK_USER(user);

        return(user->usr_cd);
}

/*##**********************************************************************\
 *
 *		dbe_user_getindex
 *
 * Returns the index object of the database where the user is connected.
 *
 * Parameters :
 *
 *	user - in, use
 *		User object.
 *
 * Return value - ref :
 *
 *      Pointer to the index object.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_index_t* dbe_user_getindex(
        dbe_user_t* user)
{
        CHK_USER(user);

        return(dbe_db_getindex(user->usr_db));
}

/*##**********************************************************************\
 *
 *		dbe_user_gettrxsem
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
SS_INLINE SsSemT* dbe_user_gettrxsem(
        dbe_user_t* user)
{
        CHK_USER(user);

        return(user->usr_trxsem);
}

/*##**********************************************************************\
 *
 *		dbe_user_settrx
 *
 *
 *
 * Parameters :
 *
 *	user -
 *
 *
 *	trx -
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
SS_INLINE void dbe_user_settrx(
        dbe_user_t* user,
        dbe_trx_t* trx)
{
        CHK_USER(user);

        user->usr_trx = trx;
        if (trx == NULL) {
            user->usr_trxid = DBE_TRXID_NULL;
        } else {
            user->usr_trxid = dbe_trx_getusertrxid(trx);
        }
}

/*##**********************************************************************\
 *
 *		dbe_user_gettrx
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
SS_INLINE dbe_trx_t* dbe_user_gettrx(
        dbe_user_t* user)
{
        CHK_USER(user);

        return(user->usr_trx);
}

#endif /* defined(DBE0USER_C) || defined(SS_USE_INLINE) */

#endif /* DBE0USER_H */
