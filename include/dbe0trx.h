/*************************************************************************\
**  source       * dbe0trx.h
**  directory    * dbe
**  description  * Transaction functions.
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


#ifndef DBE0TRX_H
#define DBE0TRX_H

#include <dbe0type.h>

SS_INLINE dbe_trxid_t dbe_trx_getusertrxid(
        dbe_trx_t* trx);

#include <uti0vtpl.h>

#include <su0prof.h>

#include <rs0types.h>
#include <rs0error.h>
#include <rs0key.h>
#include <rs0pla.h>
#include <rs0entna.h>
#include <rs0event.h>
#include <rs0relh.h>
#include <rs0tnum.h>
#include <rs0sqli.h>

#include "dbe0db.h"
#include "dbe0erro.h"
#include "dbe0tref.h"
#include "dbe1trx.h"
#include "dbe0user.h"
#include "dbe4mme.h"

/* Special transaction pointer value used when transactions are
   not really used.
*/
#define DBE_TRX_NOTRX   ((dbe_trx_t*)1L)
#define DBE_TRX_HSBTRX  ((dbe_trx_t*)2L)

/*
        Transaction routines.
*/

void dbe_trx_locktran_init(
        rs_sysi_t* cd);

void dbe_trx_locktran_done(
        rs_sysi_t* cd);

void dbe_trx_sysi_done(
        rs_sysi_t* cd);

dbe_trx_t* dbe_trx_begin(
        dbe_user_t* user);

dbe_trx_t* dbe_trx_beginbuf(
        dbe_trx_t* trxbuf,
        dbe_user_t* user);

void dbe_trx_initbuf(
        dbe_trx_t* trxbuf,
        dbe_user_t* user);

dbe_trx_t* dbe_trx_beginwithmaxreadlevel(
        dbe_user_t* user);

dbe_trx_t* dbe_trx_beginreplicarecovery(
        dbe_user_t* user,
        dbe_trxinfo_t* trxinfo);

dbe_trx_t* dbe_trx_beginreplica(
        dbe_user_t* user);

SS_INLINE void dbe_trx_setopenflag(
        dbe_trx_t* trx,
        bool* p_open);

SS_INLINE void dbe_trx_restartif(
        dbe_trx_t* trx);

bool dbe_trx_setcanremovereadlevel(
        dbe_trx_t* trx,
        bool release_with_writes);

void dbe_trx_setdelayedstmterror(
        dbe_trx_t* trx);

bool dbe_trx_setcheckwriteset(
        dbe_trx_t* trx);

bool dbe_trx_setcheckreadset(
        dbe_trx_t* trx);

bool dbe_trx_setisolation(
        dbe_trx_t* trx,
        rs_sqli_isolation_t isolation);

rs_sqli_isolation_t dbe_trx_getisolation(
        dbe_trx_t* trx);

void dbe_trx_signalisolationchange(
        dbe_trx_t* trx);

int dbe_trx_getwritemode(
        dbe_trx_t* trx);

SS_INLINE bool dbe_trx_isserializable(
        dbe_trx_t*  trx);

SS_INLINE bool dbe_trx_isnonrepeatableread(
        dbe_trx_t* trx);

bool dbe_trx_setreadonly(
        dbe_trx_t* trx);

void dbe_trx_setforcecommit(
        dbe_trx_t* trx);

bool dbe_trx_forcecommit(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_puthsbmarkstolog(
        dbe_trx_t* trx,
        dbe_trxid_t remotetrxid,
        dbe_trxid_t remotestmttrxid,
        bool isdummy);

SS_INLINE dbe_ret_t dbe_trx_markwrite(
        dbe_trx_t* trx,
        bool stmtoper);

dbe_ret_t dbe_trx_markwrite_nolog(
        dbe_trx_t* trx,
        bool stmtoper);

dbe_ret_t dbe_trx_markwrite_enteraction(
        dbe_trx_t* trx,
        bool stmtoper);

dbe_ret_t dbe_trx_markreplicate(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_setddop(
        dbe_trx_t* trx);

SS_INLINE bool dbe_trx_isreadonly(
        dbe_trx_t* trx);

bool dbe_trx_iswrites(
        dbe_trx_t* trx);

bool dbe_trx_isstmtgroupactive(
        dbe_trx_t* trx);

bool dbe_trx_setnocheck(
        dbe_trx_t* trx);

SS_INLINE bool dbe_trx_isnocheck(
        dbe_trx_t* trx);

bool dbe_trx_setnointegrity(
        dbe_trx_t* trx);

bool dbe_trx_setrefintegrity_check(
        dbe_trx_t* trx,
        bool refintegrity);

bool dbe_trx_check_refintegrity(
        dbe_trx_t* trx);

void dbe_trx_set2safe_user(
        dbe_trx_t* trx,
        bool is2safe);

void dbe_trx_setflush(
        dbe_trx_t* trx,
        bool flushp);

bool dbe_trx_setfailed(
        dbe_trx_t* trx,
        dbe_ret_t rc,
        bool entercaction);

bool dbe_trx_setfailed_nomutex(
        dbe_trx_t* trx,
        dbe_ret_t rc);

dbe_ret_t dbe_trx_geterrcode(
        dbe_trx_t* trx);

void dbe_trx_seterrkey(
        dbe_trx_t* trx,
        rs_key_t* key);

void dbe_trx_builduniquerrorkeyvalue(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key,
        vtpl_t* vtpl,
        dbe_btrsea_timecons_t* tc);

rs_key_t* dbe_trx_giveerrkey(
        dbe_trx_t* trx);

dstr_t dbe_trx_giveuniquerrorkeyvalue(
        dbe_trx_t* trx);

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
void dbe_trx_set_foreign_key_check(
        dbe_trx_t* trx,
        bool value);

bool dbe_trx_get_foreign_key_check(
        dbe_trx_t* trx);
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

void dbe_trx_settimeout_nomutex(
        dbe_trx_t* trx);

void dbe_trx_setdeadlock(
        dbe_trx_t* trx);

void dbe_trx_setdeadlock_noenteraction(
        dbe_trx_t* trx);

SS_INLINE void dbe_trx_startnewsearch(
        dbe_trx_t* trx);

void dbe_trx_inheritreadlevel(
        dbe_trx_t* trx,
        dbe_trx_t* readtrx);

SS_INLINE bool dbe_trx_isfailed(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_commit(
        dbe_trx_t* trx,
        bool waitp,
        rs_err_t** p_errh);

dbe_ret_t dbe_trx_rollback(
        dbe_trx_t* trx,
        bool count_trx,
        rs_err_t** p_errh);

void dbe_trx_done(
        dbe_trx_t* trx);

void dbe_trx_donebuf(
        dbe_trx_t*  trx,
        bool        issoft,
        bool        isactive);

void dbe_trx_replicaend(
        dbe_trx_t* trx);

void dbe_trx_stmt_begin(
        dbe_trx_t* trx);

void dbe_trx_stmt_beginreplica(
        dbe_trx_t* trx);

void dbe_trx_stmt_beginreplicarecovery(
        dbe_trx_t* trx,
        dbe_trxid_t stmttrxid);

dbe_ret_t dbe_trx_stmt_addphysdel(
        dbe_trx_t* trx,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode);

dbe_ret_t dbe_trx_stmt_commit(
        dbe_trx_t* trx,
        bool groupstmtp,
        rs_err_t** p_errh);

dbe_ret_t dbe_trx_stmt_rollback(
        dbe_trx_t* trx,
        bool groupstmtp,
        rs_err_t** p_errh);

dbe_ret_t dbe_trx_replicatesql(
        dbe_trx_t* trx,
        char* sqlcatalog,
        char* sqlschema,
        char* sqlstr);

/*
        Utility routines.
*/

void dbe_trx_getusertrxid_aval(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_atype_t* atype,
        rs_aval_t* aval);

SS_INLINE dbe_trxid_t dbe_trx_getstmttrxid(
        dbe_trx_t* trx);

SS_INLINE dbe_trxid_t dbe_trx_getreadtrxid(
        dbe_trx_t* trx);

void dbe_trx_setreadtrxid(
        dbe_trx_t* trx,
        dbe_trxid_t readtrxid);

SS_INLINE dbe_trxnum_t dbe_trx_getcommittrxnum(
        dbe_trx_t* trx);

SS_INLINE dbe_trxnum_t dbe_trx_getmaxtrxnum(
        dbe_trx_t* trx);

long dbe_trx_getreadlevel_long(
        dbe_trx_t* trx);

dbe_trxnum_t dbe_trx_getsearchtrxnum(
        dbe_trx_t* trx);

dbe_trxnum_t dbe_trx_getstmtsearchtrxnum(
        dbe_trx_t* trx);

void dbe_trx_resetstmtsearchtrxnum(
        dbe_trx_t* trx);

SS_INLINE dbe_user_t* dbe_trx_getuser(
        dbe_trx_t* trx);

SS_INLINE dbe_db_t* dbe_trx_getdb(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_setdb(
        dbe_trx_t* trx,
        dbe_db_t *db);

SS_INLINE void* dbe_trx_getcd(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_setcd(
        dbe_trx_t* trx,
        void *cd);

#ifdef DBE_REPLICATION

void dbe_trx_setreplicaslave(
        dbe_trx_t* trx);

void dbe_trx_setstmtfailed(
        dbe_trx_t* trx,
        dbe_ret_t rc);

dbe_ret_t dbe_trx_getstmterrcode(
        dbe_trx_t* trx);

bool dbe_trx_stmtactive(
        dbe_trx_t* trx);

void dbe_trx_sethsbflushallowed(
        dbe_trx_t* trx,
        bool hsbflushallowed);

#endif /* DBE_REPLICATION */

dbe_ret_t dbe_trx_addtolog(
        dbe_trx_t* trx,
        bool insertp,
        rs_key_t* clustkey,
        dbe_tref_t* tref,
        vtpl_t* vtpl,
        rs_relh_t* relh,
        bool isblobattrs);

dbe_ret_t dbe_trx_mme_addtolog(
        dbe_trx_t* trx,
        bool insertp,
        rs_key_t* clustkey,
        mme_rval_t* mmerval,
        rs_relh_t* relh,
        bool isblobattrs);

bool dbe_trx_uselogging(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_reptuple(
        dbe_trx_t* trx,
        bool insertp,
        dbe_tref_t* tref,
        vtpl_t* vtpl,
        rs_relh_t* relh,
        bool isblobattrs);

dbe_ret_t dbe_trx_addwrite(
        dbe_trx_t* trx,
        bool insertp,
        rs_key_t* key,
        dbe_tref_t* tref,
        uint nmergekeys,
        bool isonlydeletemark,
        rs_relh_t* relh,
        rs_reltype_t reltype);

dbe_ret_t dbe_trx_checklostupdate(
        dbe_trx_t* trx,
        dbe_tref_t* tref,
        rs_key_t* key,
        bool pessimistic);

dbe_ret_t dbe_trx_checkoldupdate(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* tref);

dbe_ret_t dbe_trx_adduniquecheck(
        void* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key,
        vtpl_t* key_vtpl,
        bool* upd_attrs,
        rs_reltype_t reltype);

dbe_ret_t dbe_trx_addrefkeycheck(
        void*           cd,
        dbe_trx_t*      trx,
        rs_relh_t*      relh,
        rs_key_t*       clustkey,
        rs_key_t*       refkey,
        rs_relh_t*      refrelh,
        vtpl_vamap_t*   key_vamap,
        bool*           upd_attrs,
        rs_reltype_t    reltype);

void dbe_trx_addreadcheck(
        dbe_trx_t* trx,
        rs_pla_t* plan,
        dynvtpl_t lastkey,
        dbe_trxid_t lasttrxid);

bool dbe_trx_keypartsupdated(
        void* cd,
        rs_key_t* key,
        uint nparts,
        bool* upd_attrs);

dbe_ret_t dbe_trx_insertrel(
        dbe_trx_t* trx,
        rs_relh_t* relh);

dbe_ret_t dbe_trx_deleterel(
        dbe_trx_t* trx,
        rs_relh_t* relh);

dbe_ret_t dbe_trx_truncaterel(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool physdelete);

dbe_ret_t dbe_trx_alterrel(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool add);

int dbe_trx_newcolumncount(
        dbe_trx_t* trx,
        rs_relh_t* relh);

dbe_ret_t dbe_trx_renamerel(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_entname_t* newname);

dbe_ret_t dbe_trx_insertindex(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key);

dbe_ret_t dbe_trx_deleteindex(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key);

dbe_ret_t dbe_trx_insertseq(
        dbe_trx_t* trx,
        long seq_id,
        rs_entname_t* seq_name,
        bool densep);

dbe_ret_t dbe_trx_deleteseq(
        dbe_trx_t* trx,
        long seq_id,
        rs_entname_t* seq_name,
        bool densep);

dbe_ret_t dbe_trx_insertname(
        dbe_trx_t* trx,
        rs_entname_t* name);

dbe_ret_t dbe_trx_deletename(
        dbe_trx_t* trx,
        rs_entname_t* name);

bool dbe_trx_namedeleted(
        dbe_trx_t* trx,
        rs_entname_t* name);

dbe_ret_t dbe_trx_insertevent(
        dbe_trx_t* trx,
        rs_event_t* event);

dbe_ret_t dbe_trx_deleteevent(
        dbe_trx_t* trx,
        rs_entname_t* name);

dbe_ret_t dbe_trx_insertview(
        dbe_trx_t* trx,
        rs_viewh_t* viewh);

dbe_ret_t dbe_trx_deleteview(
        dbe_trx_t* trx,
        rs_viewh_t* viewh);

dbe_ret_t dbe_trx_setrelhchanged(
        dbe_trx_t* trx,
        rs_relh_t* relh);

bool dbe_trx_relinserted(
        dbe_trx_t* trx,
        rs_entname_t* relname,
        rs_relh_t** p_relh);

bool dbe_trx_reldeleted(
        dbe_trx_t* trx,
        rs_entname_t* relname);

bool dbe_trx_indexinserted(
        dbe_trx_t* trx,
        rs_entname_t* indexname,
        rs_relh_t** p_relh,
        rs_key_t** p_key);

bool dbe_trx_viewinserted(
        dbe_trx_t* trx,
        rs_entname_t* viewname,
        rs_viewh_t** p_viewh);

bool dbe_trx_viewdeleted(
        dbe_trx_t* trx,
        rs_entname_t* viewname);

dbe_ret_t dbe_trx_createuser(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_logauditinfo(
        dbe_trx_t* trx,
        long userid,
        char* info);

void dbe_trx_stoplogging(
        dbe_trx_t* trx);

void dbe_trx_setlocktimeout(
        dbe_trx_t* trx,
        long timeout);

SS_INLINE long dbe_trx_getlocktimeout(
        dbe_trx_t* trx);

void dbe_trx_setoptimisticlocktimeout(
        dbe_trx_t* trx,
        long timeout);

dbe_lock_reply_t dbe_trx_lockcatalog_shared(
        dbe_trx_t* trx,
        ulong catalogid);

dbe_lock_reply_t dbe_trx_lock(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* tref,
        dbe_lock_mode_t mode,
        long timeout,
        bool bouncep,
        bool* p_newlock);

void dbe_trx_unlock(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* tref);

dbe_lock_reply_t dbe_trx_lockbyname(
        dbe_trx_t* trx,
        ulong relid,
        long lock_name,
        dbe_lock_mode_t mode,
        long timeout);

void dbe_trx_unlockbyname(
        dbe_trx_t* trx,
        ulong relid,
        long lock_name);

dbe_lock_reply_t dbe_trx_lockbyname_cd(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        ulong relid,
        long lock_name,
        dbe_lock_mode_t mode,
        long timeout);


dbe_ret_t dbe_trx_lockrelh(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool exclusive,
        long timeout);

dbe_ret_t dbe_trx_lockrelid(
        dbe_trx_t*  trx,
        long        relid,
        bool        exclusive,
        long        timeout);

void dbe_trx_unlockrelid(
        dbe_trx_t*  trx,
        long        relid);

dbe_ret_t dbe_trx_lockrelh_cd(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool exclusive,
        long timeout);

dbe_ret_t dbe_trx_lockrelh_long(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool exclusive,
        long timeout);

void dbe_trx_unlockall_long(
        rs_sysi_t* cd);

void dbe_trx_unlockrelh(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh);

bool dbe_trx_getlockrelh(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool* p_isexclusive,
        bool* p_isverylong_duration);

dbe_ret_t dbe_trx_lockrelh_convert(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool exclusive,
        bool verylong_duration);

bool dbe_trx_uselocking(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_lock_mode_t mode,
        long* p_timeout,
        bool* p_optimistic_lock);

SS_INLINE dbe_mmlocklst_t* dbe_trx_getmmlocklist(
        dbe_trx_t* trx);

void dbe_trx_rollback_searches(
        dbe_trx_t*  trx);

dbe_ret_t dbe_trx_markseqwrite(
        dbe_trx_t* trx,
        void* sv);

dbe_ret_t dbe_trx_logseqvalue(
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        void* seq_value);

dbe_ret_t dbe_trx_logseqinc(
        dbe_trx_t* trx,
        long seq_id,
        void* seq_value);

void dbe_trx_insertbytes(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        long        nbytes,
        ulong       nchanges);

void dbe_trx_deletebytes(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        long        nbytes,
        ulong       nchanges);

void dbe_trx_getrelhcardin(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        ss_int8_t* p_ntuples,
        ss_int8_t* p_nbytes);

#ifdef SS_SYNC

long dbe_trx_getnewsavestmtid(
        dbe_trx_t* trx);

long dbe_trx_getnewsyncmsgid(
        dbe_trx_t* trx);

long dbe_trx_getnewsyncnodeid(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_getnewsynctupleversion(
        dbe_trx_t* trx,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void dbe_trx_getcurrentsynctupleversion(
        dbe_trx_t* trx,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void dbe_trx_getminsynctupleversion(
        dbe_trx_t* trx,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void dbe_trx_getfirstsynctupleversion(
        dbe_trx_t* trx,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

#endif /* SS_SYNC */

void* dbe_trx_gethsbctx(
        dbe_trx_t* trx);

void dbe_trx_sethsbctx(
        dbe_trx_t* trx,
        void* ctx,
        dbe_hsbctx_funs_t* hsbctxfuns);

void* dbe_trx_gethsbsqlctx(
        dbe_trx_t* trx);

void dbe_trx_sethsbsqlctx(
        dbe_trx_t* trx,
        void* sqlctx);

rep_params_t* dbe_trx_getrepparams(
        dbe_trx_t* trx);

void dbe_trx_incsyncmsgid(
        dbe_trx_t* trx);

void dbe_trx_incsynctupleversion(
        dbe_trx_t* trx);

bool dbe_trx_ishsbopactive(
        dbe_trx_t* trx);

bool dbe_trx_ishsbcommitsent(
        dbe_trx_t* trx);

bool dbe_trx_hsbenduncertain(
        dbe_trx_t* trx,
        dbe_trxid_t trxid,
        su_ret_t rc);

bool dbe_trx_initrepparams(
        dbe_trx_t* trx,
        rep_type_t type);

#ifdef SS_MIXED_REFERENTIAL_INTEGRITY
dbe_ret_t dbe_trx_refkeycheck(
        void*           cd,
        dbe_trx_t*      trx,
        rs_key_t*       clustkey,
        rs_key_t*       refkey,
        rs_ttype_t*     ttype,
        rs_tval_t*      tval);

dbe_ret_t dbe_trx_mme_refkeycheck(
        rs_sysi_t*          cd,
        dbe_trx_t*          trx,
        rs_key_t*           refkey,
        rs_relh_t*          refrelh,
        dbe_trxid_t         stmtid,
        dynvtpl_t           value);
#endif

dbe_ret_t dbe_trx_readrelh(
        dbe_trx_t*      trx,
        ulong           relid);

void dbe_trx_abortrelh(
        dbe_trx_t*      trx,
        ulong           relid);

void dbe_trx_error_create(
        dbe_trx_t* trx,
        dbe_ret_t rc,
        rs_err_t** p_errh);

#if defined(DBE0TRX_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *      dbe_trx_restartif
 * 
 * Restarts the transaction if read level is released by gtrs.
 * 
 * Parameters : 
 * 
 *      trx - 
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
SS_INLINE void dbe_trx_restartif(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_restartif\n"));

        if (dbe_trxinfo_canremovereadlevel(trx->trx_info)) {
            dbe_trx_restart(trx);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_getuser
 *
 * Returns the user object of the transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value - ref :
 *
 *      Pointer to the user object.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_user_t* dbe_trx_getuser(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_user);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getusertrxid
 *
 * Returns the user transaction id.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxid_t dbe_trx_getusertrxid(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_usertrxid);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getstmttrxid
 *
 * Returns the statement transaction id.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxid_t dbe_trx_getstmttrxid(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_stmttrxid);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getreadtrxid
 *
 * Returns transaction id read level. This trxid read level means highest
 * trxid that should be visible to a search.
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE dbe_trxid_t dbe_trx_getreadtrxid(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_readtrxid);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getcommittrxnum
 *
 * Returns the commit transaction number.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value :
 *
 *      commit transaction number
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxnum_t dbe_trx_getcommittrxnum(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dassert(trx->trx_forcecommit ||
                   (!dbe_trxinfo_isbegin(trx->trx_info) &&
                    !dbe_trxinfo_istobeaborted(trx->trx_info)));

        return(trx->trx_info->ti_committrxnum);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getmaxtrxnum
 *
 * Returns the max transaction number. This is the transaction read level.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value :
 *
 *      max transaction number
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trxnum_t dbe_trx_getmaxtrxnum(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));

        if (!DBE_TRXNUM_EQUAL(trx->trx_tmpmaxtrxnum, DBE_TRXNUM_NULL)) {
            return(trx->trx_tmpmaxtrxnum);
        } else {
            return(trx->trx_info->ti_maxtrxnum);
        }
}

SS_INLINE void dbe_trx_setopenflag(
        dbe_trx_t* trx,
        bool* p_open)
{
        CHK_TRX(trx);
        ss_dassert(*p_open == TRUE);
        trx->trx_openflag = p_open;
        if (trx->trx_commitst == TRX_COMMITST_DONE) {
            *p_open = FALSE;
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_startnewsearch
 *
 * Informs the transaction object that a new search is started, used e.g.
 * to select a new read level in non-repeatable read mode.
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE void dbe_trx_startnewsearch(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        if (trx->trx_nonrepeatableread) {
            /* Set to NULL so dbe_trx_getsearchtrxnum() will allocate a new
             * read level.
             */
            trx->trx_searchtrxnum = DBE_TRXNUM_NULL;
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_getdb
 *
 * Returns the db object of the transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value - ref :
 *
 *      Pointer to the db object.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_db_t* dbe_trx_getdb(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_db);
}

/*##**********************************************************************\
 *
 *              dbe_trx_isnocheck
 *
 * Returns TRUE if transaction is in nocheck mode.
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE bool dbe_trx_isnocheck(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_mode == TRX_NOCHECK);
}

/*##**********************************************************************\
 *
 *              dbe_trx_isreadonly
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE bool dbe_trx_isreadonly(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_mode == TRX_READONLY);
}

/*##**********************************************************************\
 *
 *              dbe_trx_markwrite
 *
 * Marks the transaction as a write transaction. The current mode is
 * changed.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      stmtoper -
 *              If TRUE this is statement related operation. FALSE means
 *              this not statement related like for example sparse sequence
 *              increment.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE dbe_ret_t dbe_trx_markwrite(dbe_trx_t* trx, bool stmtoper)
{
        return dbe_trx_markwrite_local(trx, stmtoper, TRUE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getcd
 *
 * Returns the cd object of the transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value - ref :
 *
 *      Pointer to the db object.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void* dbe_trx_getcd(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_cd);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getmmlocklist
 *
 * Returns main memory index lock list.
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE dbe_mmlocklst_t* dbe_trx_getmmlocklist(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

#ifdef SS_MME
        if (trx->trx_mmll == NULL) {
            dbe_db_t*           db;
            dbe_mme_t*          mme;

            db = trx->trx_db;
            mme = dbe_db_getmme(db);
            trx->trx_mmll = dbe_mmlocklst_init(trx->trx_cd, mme);
        }
#elif !defined(SS_NOMMINDEX)
        if (trx->trx_mmll == NULL) {
            trx->trx_mmll = dbe_mmlocklst_init(
                                dbe_user_getmmind(trx->trx_user),
                                trx->trx_cd);
        }
#endif /* SS_NOMMINDEX */
        return(trx->trx_mmll);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getlocktimeout
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE long dbe_trx_getlocktimeout(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_getlocktimeout, userid = %d\n",
            dbe_user_getid(trx->trx_user)));

        return(trx->trx_pessimistic_lock_to);
}

/*##**********************************************************************\
 *
 *              dbe_trx_isnonrepeatableread
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE bool dbe_trx_isnonrepeatableread(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_nonrepeatableread);
}

SS_INLINE bool dbe_trx_isserializable(
        dbe_trx_t*  trx)
{
        return trx->trx_defaultwritemode == TRX_CHECKREADS;
}

/*##**********************************************************************\
 *
 *              dbe_trx_isfailed
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE bool dbe_trx_isfailed(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        if (trx == NULL) {
            ss_derror;
            return(DBE_ERR_FAILED);
        }
        return(trx->trx_errcode != DBE_RC_SUCC);
}

#endif /* defined(DBE0TRX_C) || defined(SS_USE_INLINE) */

#endif /* DBE0TRX_H */
