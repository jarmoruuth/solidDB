/*************************************************************************\
**  source       * tab0conn.c
**  directory    * tab
**  description  * Connection routines for table level.
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

#include <ssstdio.h>
#include <ssstdlib.h>
#include <ssconio.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssdebug.h>
#include <sssprint.h>
#include <ssservic.h>
#include <sstime.h>
#include <sstraph.h>

#include <su0time.h>
#include <su0prof.h>
#include <su0inifi.h>
#include <su0chcvt.h>
#include <su0sdefs.h>
#include <su0cfgst.h>
#include <su0param.h>
#include <su0usrid.h>
#include <su0prof.h>
#include <su0task.h>

#include <su0li3.h>

#include <ui0msg.h>

#include <rs0types.h>
#include <rs0sysi.h>
#include <rs0sqli.h>
#include <rs0relh.h>
#include <rs0viewh.h>
#include <rs0sdefs.h>
#include <rs0aval.h>
#include <rs0cardi.h>
#include <rs0cons.h>
#include <rs0entna.h>
#include <rs0evnot.h>

#include <dbe0db.h>
#include <dbe0user.h>
#include <dbe0brb.h>
#include <dbe0crypt.h>

#include <dbe0catchup.h>
#include <dbe0logi.h>

#include <xs0mgr.h>

#ifndef SS_MYSQL
#include "../srv/srv0task.h"
#endif


#include "tab1defs.h"
#include "tab1erro.h"
#include "tab1dd.h"
#include "tab2dd.h"
#include "tab1priv.h"
#include "tab0tran.h"
#include "tab0conn.h"

#ifndef SS_MYSQL
#include "tab1upd.h"
#include "tab0sqls.h"
#include "tab0sql.h"
#include "tab0hsb.h"
#include "tab0sync.h"
#endif

#include "tab0blobg2.h"
#include "tab0evnt.h"
#include "tab0sche.h"
#ifdef SS_HSBG2
#include "tab0sysproperties.h"
#endif /* SS_HSBG2 */
#include "tab0seq.h"

#define CHK_TDB(tdb)    ss_dassert(SS_CHKPTR(tdb) && (tdb)->tdb_chk == TBCHK_TDB)
#define CHK_TCON(tc)    ss_dassert(SS_CHKPTR(tc) && (tc)->tc_chk == TBCHK_TCON)

#define TB_DEFAULT_MAXCMPLEN    RS_KEY_MAXCMPLEN

struct tb_recovctx_st {
        tb_database_t*  rc_tdb;
        TliConnectT*    rc_tlicon;
        rs_sysi_t*      rc_cd;
        tb_trans_t*     rc_trans;
} /* tb_recovctx_t */;

/* Structure for a table connection.
*/

struct tb_connect_st {
        ss_debug(tb_check_t tc_chk;)
        rs_sysinfo_t*   tc_sysinfo;     /* system info passed as client
                                           data to sql interpreter */
        sqltrans_t*     tc_sqltrans;    /* transaction handle */
        sqlsystem_t*    tc_sqls;        /* SQL system */

        sqlsystem_t*    tc_sqls_maintenance; /* SQL system for maintenancemode */
        int             tc_sqls_use_maintenance; /* referense count for hurc SQL system */

        dbe_db_t*       tc_db;          /* database */
        int             tc_nlink;
        bool            tc_allowlogfailure;
        bool            tc_sysconnect;
        tb_database_t*  tc_tdb;
        long            tc_userid;
};

struct tb_database_st {
        ss_debug(tb_check_t tdb_chk;)
        dbe_db_t*            tdb_db;
        tb_connect_t*        tdb_sysconnect;
#ifdef SS_HSBG2
        tb_sysproperties_t*  tdb_sysproperties;
#endif /* SS_HSBG2 */
        rs_sqlinfo_t*        tdb_sqli;
        SsQsemT*             tdb_rslinksemarray[RS_SYSI_MAXLINKSEM];
        bool                 tdb_cpactive;
        xs_mgr_t*            tdb_xsmgr;
        void                 (*tdb_admincommandfp)(
                                 rs_sysi_t* cd,
                                 su_list_t* list,
                                 char* cmd,
                                 int* acmdstatus,
                                 bool* p_finishedp,
                                 tb_admin_cmd_t* tb_admincmd,
                                 void** p_ctx);
        void                 (*tdb_admineventfp)(
                                 rs_sysi_t* cd,
                                 su_list_t* list,
                                 char* cmd,
                                 bool* isadmevent,
                                 bool* finishedp,
                                 bool* succp);

        su_chcollation_t     tdb_chcollation;
        tb_schema_t*         tdb_schema;
        rs_eventnotifiers_t* tdb_events;
        long                 tdb_uniquemsgid_seq_id; /* sys sequence id for
                                                      * RS_SEQ_MSGID */
        tb_blobg2mgr_t*      tdb_blobg2mgr;
        int                  tdb_maxcmplen;
        bool                 tdb_defaultistransient;
        bool                 tdb_defaultisglobaltemporary;
};

typedef enum {
        TB_CONNECT_NORMAL,
        TB_CONNECT_REPLICA,
        TB_CONNECT_REPLICA_BYUSERID,
        TB_CONNECT_ADMIN,
        TB_CONNECT_CONSOLE,
        TB_CONNECT_HSB
} tb_connecttype_t;

void* tb_init_inifile = NULL;

/* The following variables are used in single user versions.
 */
static tb_database_t* tb_local_tdb;
static int            tb_init_counter;

static void (*tb_server_sysi_init)(rs_sysi_t* sysi);
static void (*tb_server_sysi_done)(rs_sysi_t* sysi);

static void (*tb_server_task_start)(
                rs_sysinfo_t* cd,
                SuTaskClassT taskclass,
                char* task_name,
                su_task_fun_t task_fun,
                void* td);
static bool* tb_server_shutdown_coming;

static void tb_done_server_ex(tb_database_t* tdb, bool convertp, bool docheckpoint);

static void (*tb_createcheckpoint_hsbcopycallback)(void);

static void tb_resetschema(tb_database_t* tdb, rs_sysi_t *cd);

/*#***********************************************************************\
 *
 *              tb_setuniquemsgid_sequence
 *
 *      Sets the id of "uniquemsgid sequencer" defined in RS_SEQ_MSGID
 *      to the tdb_uniquemsgid_seq_id field in tb_database_st struct.
 *
 * Parameters :
 *      sysi - in, use
 *          Client data.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#ifndef SS_MYSQL
static void tb_setuniquemsgid_sequence( rs_sysi_t* sysi ){

        ss_dassert( sysi != NULL );

        if (sysi!=NULL){
            tb_connect_t* tc = (tb_connect_t*)rs_sysi_tbcon(sysi);
            tb_trans_t* trans = tb_trans_init(sysi);
            bool trans_finished;

            ss_dassert( (tc!=NULL) && (trans!=NULL) ) ;
            if ((tc!=NULL)&&(trans!=NULL)){
                tb_database_t* tdb = tb_gettdb( tc );

                ss_dassert( tdb != NULL );
                if (tdb != NULL){
                    bool succ;
                    bool isdense;
                    char* authid;
                    char* catalog;
                    long seqid;
                    rs_atype_t* atype = rs_atype_initlong(sysi);
                    rs_aval_t* aval = rs_aval_create(sysi, atype);
                    bool trans_begun = tb_trans_beginif( sysi, trans );

                    succ = tb_seq_find( sysi,
                                        trans,
                                        (char *)RS_SEQ_MSGID,
                                        (char *)RS_AVAL_SYSNAME,
                                        RS_AVAL_DEFCATALOG,
                                        &authid,
                                        &catalog,
                                        &seqid,
                                        &isdense,
                                        NULL);

                    ss_assert( succ );

                    if (trans_begun){
                        tb_trans_commit(sysi, trans, &trans_finished, NULL);
                    }
                    /* set uniquemsgid sequence value */
                    if (succ){
                        tdb->tdb_uniquemsgid_seq_id = seqid;
                    }

                    rs_aval_free(sysi, atype, aval);
                    rs_atype_free(sysi, atype);
                    if (authid != NULL) {
                        SsMemFree(authid);
                    }
                    if (catalog != NULL) {
                        SsMemFree(catalog);
                    }
                }
            }
            tb_trans_done(sysi, trans);
        }
}
#endif /* !SS_MYSQL */

/*#***********************************************************************\
 *
 *              tb_getnewuniquemsgid
 *
 *      Returns the next value in the system sequencer pointed by
 *      tdb_uniquemsgid_seq_id field in tb_database_st struct.
 *
 * Parameters :
 *      sysi - in, use
 *          Client data.
 *
 * Return value :
 *      Sequencer value (>0), 0 if something fails.
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool tb_getnewuniquemsgid(
        rs_sysi_t* sysi,
        ss_int8_t* p_result,
        rs_err_t** p_errh)
{
        bool succ ;

        tb_trans_t* trans;
        tb_connect_t* tc;
        tb_database_t* tdb;
        long seqid;
        rs_auth_t* auth;
        bool finished;
        rs_atype_t* atype;
        rs_aval_t* aval;

        ss_assert(sysi != NULL);
        /* get connection handle and transaction handle from sysi */
        trans = (tb_trans_t*)rs_sysi_getistransactive_ctx(sysi);
        tc = (tb_connect_t*)rs_sysi_tbcon(sysi);
        ss_assert( (trans!=NULL) && (tc!=NULL) );
        tdb = tb_gettdb( tc );/* get db handle */
        ss_assert(tdb != NULL);

        seqid = tdb->tdb_uniquemsgid_seq_id;
        ss_assert( seqid > 0 );

        auth = rs_sysi_auth( sysi );
        ss_assert( auth != NULL);


        /* set system privileges on during seq_next */
        rs_auth_setsystempriv(sysi, auth, TRUE);

        atype = rs_atype_initbysqldt(sysi, RSSQLDT_BIGINT, -1L, -1L);
        aval = rs_aval_create(sysi, atype);

        tb_trans_beginif(sysi, trans);

        succ = tb_seq_next(
                sysi,
                trans,
                seqid,
                FALSE, /*sequencer is not of type DENSE*/
                atype,
                aval,
                &finished,
                p_errh);
        ss_dassert(finished); /* should be finished always */

        if (succ) {
            ss_assert(finished); /* should be finished always */
            *p_result = rs_aval_getint8(sysi, atype, aval);
        }
        rs_aval_free(sysi, atype, aval);
        rs_atype_free(sysi, atype);

        /* turn system privileges off */
        rs_auth_setsystempriv(sysi, auth, FALSE);
        return (succ);
}

#ifndef SS_NOSQL

/*#***********************************************************************\
 *
 *              tb_createsysrelfun
 *
 * Function that is called from the relation buffer iterator function.
 * When a new database is created, all relations that are in the relation
 * buffer are created. This is done so that all relations in the buffer
 * are iterated calling this function for each relation. This function
 * creates the relation to the database. The system relation initialization
 * function creates memory images of system relations and adds them to
 * the relation buffer.
 *
 * Parameters :
 *
 *      userinfo - in, use
 *          User defined parameter given to the relation buffer iterator
 *          function and passed to this function. Contains for example
 *          transaction and authorization handles.
 *
 *      is_rel - in
 *          If TRUE, the relh_or_viewh is a relation handle, otherwise
 *          it is view handle.
 *
 *      relh_or_viewh - in, use
 *          Pointer to relation or view handle. The value is specified
 *          by parameter is_rel.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tb_createsysrelfun(
        void* userinfo,
        bool is_rel,
        void* relh_or_viewh,
        long id __attribute__ ((unused)),
        void* cardin __attribute__ ((unused)))
{
        su_ret_t rc;
        rs_relh_t* relh;
        rs_viewh_t* viewh;
        tb_dd_sysrel_userinfo_t* ui;

        ui = userinfo;
        if (is_rel) {
            relh = relh_or_viewh;

            ss_dprintf_3(("tb_createsysrelfun: rel %s\n", rs_relh_name(ui->ui_cd, relh)));

            rc = tb_dd_createrel(
                    ui->ui_cd,
                    ui->ui_trans,
                    relh,
                    ui->ui_auth,
                    0,
                    TB_DD_CREATEREL_SYSTEM,
                    NULL);
            su_rc_assert(rc == SU_SUCCESS, rc);

        } else {
            viewh = relh_or_viewh;

            ss_dprintf_3(("tb_createsysrelfun: view %s\n", rs_viewh_name(ui->ui_cd, viewh)));

            rc = tb_dd_createsysview(
                    ui->ui_cd,
                    ui->ui_trans,
                    viewh,
                    ui->ui_auth,
                    NULL);
            su_rc_assert(rc == SU_SUCCESS, rc);

        }
}

/*#***********************************************************************\
 *
 *              tb_createsysrel
 *
 * Creates system relation from the relation buffer. Used to add system
 * relations to the database when the database is created.
 *
 * Parameters :
 *
 *      cd - in, use
 *          Client data.
 *
 *      trans - in, use
 *          Transaction handle.
 *
 *      rbuf - in, use
 *
 *
 *      auth - in, use
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tb_createsysrel(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        rs_auth_t* auth
#ifdef COLLATION_UPDATE
        ,su_chcollation_t chcollation
#endif
        )
{
        bool finished;
        bool succp;
        tb_dd_sysrel_userinfo_t ui;
        sqlsystem_t* sqlsystem;
        rs_err_t* err = NULL;

        ss_dprintf_3(("tb_createsysrel\n"));

        ui.ui_cd = cd;
        ui.ui_trans = trans;
        ui.ui_auth = auth;

        tb_trans_beginif(cd, trans);

        rs_rbuf_iterate(cd, rbuf, &ui, tb_createsysrelfun);

#ifdef SS_MYSQL
        sqlsystem = NULL;
#else
        sqlsystem = tb_sqls_init(cd);
#endif

        succp = tb_dd_runstartupsqlstmts(
                    cd,
                    trans,
                    sqlsystem
#ifdef COLLATION_UPDATE
                    ,chcollation
#endif
                    );
        ss_assert(succp);
        do {
            succp = tb_trans_commit(cd, trans, &finished, NULL);
        } while (!finished);
        if (!succp) {
            ss_dprintf_1(("tb_createsysrel:%s\n", rs_error_geterrstr(cd, err)));
            SsLogErrorMessage(rs_error_geterrstr(cd, err));
        }
        ss_assert(succp);

#ifndef SS_MYSQL
        tb_sqls_done(cd, sqlsystem);
#endif /* !SS_MYSQL */
}

#define TB_CONVERT_MAX_SYSID (RS_USER_ID_START - 1)

static void tb_fetch_used_ids(
        TliConnectT* tcon,
        char *rel,
        char *col,
        bool *used)
{
        TliRetT trc;
        TliCursorT* tcur;
        ulong id;

        ss_dprintf_3(("tb_fetch_used_ids, table = %s\n", rel));

        tcur = TliCursorCreate(tcon, RS_AVAL_DEFCATALOG, (char *)RS_AVAL_SYSNAME, rel);
        if (tcur == NULL) {
            return;
        }

        trc = TliCursorColLong(tcur, col, (long *)&id);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        trc = TliCursorOpen(tcur);
        ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            if (id < TB_CONVERT_MAX_SYSID) {
                used[id] = TRUE;
            }
        }

        TliCursorFree(tcur);
}

static void tb_convert_init(rs_sysi_t* cd)
{
        bool *attrid_used, *keyid_used;
        TliConnectT* tcon;
        long i;
        const char *tables[][2] = {
            { RS_RELNAME_TABLES, RS_ANAME_TABLES_ID },
            { RS_RELNAME_PUBL, RS_ANAME_PUBL_ID },
            { RS_RELNAME_FORKEYPARTS, RS_ANAME_FORKEYPARTS_ID },
            { RS_RELNAME_KEYPARTS, RS_ANAME_KEYPARTS_ID },
            { RS_RELNAME_EVENTS, RS_ANAME_EVENTS_ID },
            { RS_RELNAME_VIEWS, RS_ANAME_VIEWS_V_ID },
            { RS_RELNAME_CATALOGS, RS_ANAME_CATALOGS_ID },
            { RS_RELNAME_SEQUENCES, RS_ANAME_SEQUENCES_ID },
            { RS_RELNAME_TRIGGERS, RS_ANAME_TRIGGERS_ID },
            { RS_RELNAME_PROCEDURES, RS_ANAME_PROCEDURES_ID },
            { NULL, NULL },
        };

        ss_dprintf_3(("tb_convert_init\n"));

        attrid_used = SsMemAlloc(sizeof(bool) * (TB_CONVERT_MAX_SYSID + 1));
        keyid_used = SsMemAlloc(sizeof(bool) * (TB_CONVERT_MAX_SYSID + 1));

        for (i = 0; i < TB_CONVERT_MAX_SYSID + 1; i++) {
                keyid_used[i] = FALSE;
                attrid_used[i] = FALSE;
        }

        tcon = TliConnectInit(cd);

        tb_fetch_used_ids(
                tcon,
                (char *)RS_RELNAME_COLUMNS,
                (char *)RS_ANAME_COLUMNS_ID,
                attrid_used);

        for (i = 0; tables[i][0] != NULL; i++) {
            tb_fetch_used_ids(tcon, (char *)tables[i][0], (char *)tables[i][1], keyid_used);
        }

        TliConnectDone(tcon);

        dbe_db_convert_init(rs_sysi_db(cd), attrid_used, keyid_used);
        dbe_db_convert_set(rs_sysi_db(cd), TRUE);
}

static void tb_convert_done(rs_sysi_t* cd)
{
        dbe_db_convert_done(rs_sysi_db(cd));
}

/*#***********************************************************************\
 *
 *              tb_updatesysrel
 *
 * Updates system relations by creating those relations that do not yet
 * exist.
 *
 * Parameters :
 *
 *      tcon - in
 *
 *      cd - in
 *
 *
 *      trans - use
 *
 *
 *      rbuf - in
 *
 *
 *      p_newrel_rbt - out, give
 *              If there were new relations created, the names are
 *              stored into this rbt. The rbt is created if it is NULL.
 *
 *      insert_collation - in
 *          TRUE if collation line needs to be added (very old database)
 *
 *      chcollation - in
 *          if (insert_collation), the value for collation
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool tb_updatesysrel(
        rs_sysinfo_t* cd,
        tb_trans_t* trans,
        rs_rbuf_t* rbuf,
        su_rbt_t** p_newrel_rbt,
        bool insert_collation,
        su_chcollation_t chcollation)
{
        bool finished;
        bool succp;
        bool changes = FALSE;
        sqlsystem_t* sqlsystem;

        tb_trans_beginif(cd, trans);

#ifdef SS_MYSQL
        sqlsystem = NULL;
#else
        sqlsystem = tb_sqls_init(cd);
#endif

        if (ss_convertdb) {
            tb_convert_init(cd);
        }

        if (tb_dd_createsyskeysschemakey(cd, sqlsystem)) {
            changes = TRUE;
        }
        if (tb_dd_createsyskeyscatalogkey(cd, sqlsystem)) {
            changes = TRUE;
        }
        if (tb_dd_createsystablescatalogkey(cd, sqlsystem)) {
            changes = TRUE;
        }

        if (tb_dd_updatestartupsqlstmts(
                    cd,
                    trans,
                    sqlsystem,
                    rbuf,
                    p_newrel_rbt)) {
            changes = TRUE;
        }
#ifdef COLLATION_UPDATE
        if (insert_collation) {
            bool succp;

            succp = tb_dd_insertsysinfo_collation(
                        cd,
                        trans,
                        sqlsystem,
                        chcollation);
            ss_assert(succp);
        }
#endif /* COLLATION_UPDATE */

        do {
            succp = tb_trans_commit(cd, trans, &finished, NULL);
            ss_assert(succp);
        } while (!finished);

        if (tb_dd_updatesysrelschemakeys(cd, sqlsystem)) {
            changes = TRUE;
        }
        if (tb_dd_updatecatalogkeys(
                    cd,
                    trans,
                    sqlsystem,
                    rbuf,
                    p_newrel_rbt)) {
            changes = TRUE;
        }

#ifndef SS_MYSQL
        if (tb_dd_updatesynchistorykeys(cd, sqlsystem)) {
            changes = TRUE;
        }

        if (tb_dd_convert_sync_trxid_int2bin(cd, trans, sqlsystem)) {
            changes = TRUE;
        }
#endif /* !SS_MYSQL */


        /* JPA Jun 29, 2001
         * Commit here to avoid concurrency conflict
         * because trxid_int2bin uses different transaction.
         */
        finished = FALSE;
        do {
            succp = tb_trans_commit(cd, trans, &finished, NULL);
            ss_assert(succp);
        } while (!finished);

#ifndef SS_MYSQL
        tb_sqls_done(cd, sqlsystem);
#endif /* !SS_MYSQL */

        if (ss_convertdb) {
            tb_convert_done(cd);
        }

        return(changes);
}

#ifdef SS_UNICODE_SQL

/*#***********************************************************************\
 *
 *              tb_migratetounicode
 *
 * Migrates the data dictionary to UNICODE
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      trans - use
 *              table level transaction object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
#ifndef SS_MYSQL
static void tb_migratetounicode(
        rs_sysinfo_t* cd,
        tb_trans_t* trans)
{
        sqlsystem_t* sqlsystem;

        ss_dprintf_1(("tb_migratetounicode:start\n"));

        tb_trans_beginif(cd, trans);

        sqlsystem = tb_sqls_init(cd);
        tb_upd_ddtounicode(cd, trans, sqlsystem);
        tb_sqls_done(cd, sqlsystem);

        ss_dprintf_1(("tb_migratetounicode:done\n"));
}
#endif /* !SS_MYSQL */
#endif /* SS_UNICODE_SQL */

#endif /* SS_NOSQL */

/*#***********************************************************************\
 *
 *              tb_updatecardinalfun
 *
 *
 *
 * Parameters :
 *
 *      userinfo -
 *
 *
 *      is_rel -
 *
 *
 *      relh_or_viewh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tb_updatecardinalfun(
        void* userinfo,
        bool is_rel __attribute__ ((unused)),
        void* relh_or_viewh __attribute__ ((unused)),
        long id,
        void* cardin)
{
        tb_dd_sysrel_userinfo_t* ui;

        ui = userinfo;

        if (cardin != NULL && rs_cardin_ischanged(ui->ui_cd, cardin)) {
            ss_trigger("tab_updatecardinal");

            ss_dprintf_3(("tb_updatecardinalfun: rel %ld\n", id));
            tb_dd_updatecardinal(ui->ui_tcon, id, cardin);
            rs_cardin_clearchanged(ui->ui_cd, cardin);

            ss_trigger("tab_updatecardinal");
        }
}

/*#***********************************************************************\
 *
 *              tb_updatecardinal
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tb_updatecardinal(
        rs_sysinfo_t* cd,
        rs_rbuf_t* rbuf)
{
        tb_dd_sysrel_userinfo_t ui;
        TliConnectT* tcon;
        TliRetT trc;
        dbe_db_t* db;
        su_list_t* list;

        tcon = TliConnectInit(cd);
        ss_debug(TliSetFailOnlyInCommit(tcon, FALSE));

        ui.ui_cd = cd;
        ui.ui_tcon = tcon;

        /* Delete cardinal infos for dropped tables.
         */
        db = rs_sysi_db(cd);
        ss_dassert(db != NULL);
        list = dbe_db_givedropcardinallist(db);
        if (list != NULL) {
            su_list_node_t* n;
            void* relid;
            su_list_do_get(list, n, relid) {
                tb_dd_dropcardinal(tcon, (long)relid);
            }
            su_list_done(list);
        }

        trc = TliCommit(tcon);

        /* Update cardinal infos.
         */
        rs_rbuf_iterate(cd, rbuf, &ui, tb_updatecardinalfun);

        ss_trigger("tab_updatecardinal");

        trc = TliCommit(tcon);

        ss_trigger("tab_updatecardinal");

        ss_rc_dassert(trc == TLI_RC_SUCC || dbe_db_isreadonly(rs_sysi_db(cd)), TliErrorCode(tcon));


        TliConnectDone(tcon);
}

#ifdef COLLATION_UPDATE
/*#***********************************************************************\
 *
 *              tb_get_collation_from_inifile
 *
 *
 *
 * Parameters :
 *
 *      inifile -
 *
 *
 *      p_chcollation -
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
static bool tb_get_collation_from_inifile(
        su_inifile_t* inifile,
        su_chcollation_t* p_chcollation)
{
        bool found;
        char* value;
        uint scanidx = 0;

        found = su_inifile_scanstring(
                    inifile,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_COLLATION_SEQ,
                    "",
                    &scanidx,
                    &value);
        if (!found) {
            *p_chcollation = SU_CHCOLLATION_ISO8859_1;
            return (FALSE);
        }
        ss_dassert(value != NULL);
        *p_chcollation = su_chcollation_byname(value);
        if (*p_chcollation == SU_CHCOLLATION_NOTVALID) {
            su_rc_fatal_error(
                E_FATAL_DEFFILE_SSSS,
                SU_DBE_GENERALSECTION,
                SU_DBE_COLLATION_SEQ,
                value,
                su_inifile_getname(inifile));
            ss_derror;
        }
        SsMemFree(value);
        return (TRUE);
}

#endif /* COLLATION_UPDATE */

tb_recovctx_t* tb_recovctx_init(
        tb_database_t* tdb)
{
        tb_recovctx_t* recov_ctx;

        recov_ctx = SSMEM_NEW(tb_recovctx_t);

        recov_ctx->rc_tdb = tdb;
        recov_ctx->rc_tlicon = TliConnectInitByTabDb(tdb);
        recov_ctx->rc_cd = TliGetCd(recov_ctx->rc_tlicon);
        recov_ctx->rc_trans = TliGetTrans(recov_ctx->rc_tlicon);

        return(recov_ctx);
}


tb_recovctx_t* tb_recovctx_initbycd(
        tb_database_t* tdb,
        rs_sysi_t* cd)
{
        tb_recovctx_t* recov_ctx;

        recov_ctx = SSMEM_NEW(tb_recovctx_t);

        recov_ctx->rc_tdb = tdb;
        recov_ctx->rc_tlicon = TliConnectInit(cd);
        recov_ctx->rc_cd = TliGetCd(recov_ctx->rc_tlicon);
        recov_ctx->rc_trans = TliGetTrans(recov_ctx->rc_tlicon);

        return(recov_ctx);
}


static void tb_recovctx_commit(
        tb_recovctx_t* recov_ctx)
{
        TliCommit(recov_ctx->rc_tlicon);
}

void tb_recovctx_done(
        tb_recovctx_t* recov_ctx)
{
        TliConnectDone(recov_ctx->rc_tlicon);
        SsMemFree(recov_ctx);
}

static void tb_recovctx_rollback(tb_recovctx_t* recov_ctx)
{
        tb_trans_rollback_onestep(recov_ctx->rc_cd, recov_ctx->rc_trans,
                                  FALSE, NULL);
}

/*##**********************************************************************\
 *
 *              tb_recovctx_getrelh
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      en -
 *
 *
 *      priv -
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
rs_relh_t* tb_recovctx_getrelh(
        void* ctx,
        rs_entname_t* en,
        void* priv)
{
        tb_recovctx_t* recov_ctx = (tb_recovctx_t*)ctx;
        rs_relh_t* relh;

        tb_trans_beginif(recov_ctx->rc_cd, recov_ctx->rc_trans);

        relh = tb_dd_getrelh(recov_ctx->rc_cd, recov_ctx->rc_trans, en, priv, NULL);
        tb_recovctx_rollback(recov_ctx);

        return(relh);
}

/*##**********************************************************************\
 *
 *              tb_recovctx_getrelh_trxinfo
 *
 * Reads relation handle in given trxinfo context.
 *
 * Parameters :
 *
 *      recov_ctx -
 *
 *
 *      relid -
 *
 *
 *      trxinfo -
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
rs_relh_t* tb_recovctx_getrelh_trxinfo(
        tb_recovctx_t* recov_ctx,
        ulong relid,
        dbe_trxinfo_t* trxinfo,
        dbe_trxid_t readtrxid)
{
        rs_relh_t* relh;

        ss_dprintf_1(("tb_recovctx_getrelh_trxinfo\n"));

        tb_trans_beginwithtrxinfo(
            recov_ctx->rc_cd,
            recov_ctx->rc_trans,
            trxinfo,
            readtrxid);

        relh = tb_dd_readrelh_norbufupdate(
                recov_ctx->rc_cd,
                recov_ctx->rc_trans,
                relid);

        tb_recovctx_rollback(recov_ctx);

        ss_dprintf_2(("tb_recovctx_getrelh_trxinfo:relh=%ld\n", (long)relh));

        return(relh);
}

rs_relh_t* tb_recovctx_getrelh_byrelid(
        void *ctx,
        ulong relid)
{
        tb_recovctx_t* recov_ctx = (tb_recovctx_t*)ctx;
        rs_relh_t* relh;

        ss_dprintf_1(("tb_recovctx_getrelh_byrelid:relid=%ld\n", relid));

        tb_trans_beginif(recov_ctx->rc_cd, recov_ctx->rc_trans);

        relh = tb_dd_readrelh_norbufupdate(
                recov_ctx->rc_cd,
                recov_ctx->rc_trans,
                relid);

        tb_recovctx_rollback(recov_ctx);

        ss_dprintf_2(("tb_recovctx_getrelh_trxinfo:relh=%ld\n", (long)relh));

        return(relh);
}

#if 0 /* Pete removed 2004-08-26, not needed! */
static rs_relh_t* recovctx_getrelh_trxinfo_mme(
        tb_recovctx_t* recov_ctx,
        ulong relid,
        dbe_trxinfo_t* trxinfo)
{
        return(tb_recovctx_getrelh_trxinfo(
                    recov_ctx,
                    relid,
                    trxinfo,
                    DBE_TRXID_NULL));
}
#endif /* 0, Pete removed */

/*##**********************************************************************\
 *
 *              tb_recovctx_getviewh
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      en -
 *
 *
 *      priv -
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
rs_viewh_t* tb_recovctx_getviewh(
        tb_recovctx_t* recov_ctx,
        rs_entname_t* en,
        void* priv)
{
        rs_viewh_t* viewh;

        tb_trans_beginif(recov_ctx->rc_cd, recov_ctx->rc_trans);

        viewh = tb_dd_getviewh(recov_ctx->rc_cd, recov_ctx->rc_trans, en, priv, NULL);

        tb_recovctx_rollback(recov_ctx);

        return(viewh);
}

tb_database_t* tb_recovctx_gettabdb(
        tb_recovctx_t* recov_ctx)
{
        return(recov_ctx->rc_tdb);
}

void tb_recovctx_refreshrbuf(
        void *ctx)
{
        tb_recovctx_t* recov_ctx = (tb_recovctx_t*)ctx;
        rs_sysi_t* cd;
        rs_rbuf_t* rbuf;
        su_profile_timer;

        cd = recov_ctx->rc_cd;
        rbuf = rs_sysi_rbuf(cd);

        su_profile_start;

        tb_dd_loadrbuf(cd, rbuf, FALSE, TRUE);

        su_profile_stop("tb_recovctx_refreshrbuf");
}

void tb_recovctx_refreshrbuf_keepcardinal(
        void *ctx)
{
        tb_recovctx_t* recov_ctx = (tb_recovctx_t*)ctx;
        rs_sysi_t* cd;
        rs_rbuf_t* rbuf;
        su_profile_timer;

        cd = recov_ctx->rc_cd;
        rbuf = rs_sysi_rbuf(cd);

        su_profile_start;

        tb_dd_loadrbuf(cd, rbuf, FALSE, TRUE);

        su_profile_stop("tb_recovctx_refreshrbuf_keepcardinal");
}

static void recovctx_fullrefreshrbuf(
        tb_recovctx_t* recov_ctx,
        bool keep_cardinal)
{
        rs_sysi_t* cd;
        rs_rbuf_t* rbuf;
        su_profile_timer;

        cd = recov_ctx->rc_cd;
        rbuf = rs_sysi_rbuf(cd);

        su_profile_start;

        tb_dd_loadrbuf(cd, rbuf, TRUE, keep_cardinal);
        tb_schema_reload(cd, recov_ctx->rc_tdb->tdb_schema);

        su_profile_stop("tb_recovctx_fullrefreshrbuf");
}

void tb_recovctx_fullrefreshrbuf(
        void* ctx)
{
        recovctx_fullrefreshrbuf(ctx, TRUE);
}

void tb_reload_rbuf(
        void* ctx)
{
        tb_database_t* tdb = (tb_database_t*)ctx;
        tb_recovctx_t* recov_ctx;

        recov_ctx = tb_recovctx_init(tdb);
        recovctx_fullrefreshrbuf(recov_ctx, TRUE);
        tb_recovctx_done(recov_ctx);
}

void tb_reload_rbuf_keepcardinal(
        tb_database_t* tdb,
        rs_sysi_t* gate_cd)
{
        tb_recovctx_t* recov_ctx;

        recov_ctx = tb_recovctx_initbycd(tdb, gate_cd);
        recovctx_fullrefreshrbuf(recov_ctx, TRUE);
        tb_recovctx_done(recov_ctx);
}

#ifdef SS_UNICODE_SQL
static void tb_completemerge(
        void* cd,
        tb_database_t* tdb)
{
        dbe_db_mergestart(cd, tdb->tdb_db);
        while (dbe_db_mergeadvance(tdb->tdb_db, cd, 2)) {
            /* all is done in the test */
        }
        dbe_db_mergestop(tdb->tdb_db);
}

#endif /* SS_UNICODE_SQL */

/*#***********************************************************************\
 *
 *              tb_sysconnect_initbycd_ex
 *
 * Inits a system connection using previously allocated cd (client data)
 * if cd is not NULL.
 *
 * Parameters :
 *
 *              tdb - in
 *
 *
 *              cd - in, hold
 *                      If not NULL, this cd is used for the table connection.
 *          Otherwise a new cd is created.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
tb_connect_t* tb_sysconnect_initbycd_ex(tb_database_t* tdb, rs_sysi_t* cd, char* file, int line)
{
        tb_connect_t* tc;
        dbe_user_t* user;
        rs_auth_t* auth;

        SS_PUSHNAME("tb_sysconnect_initbycd_ex");

        tc = SSMEM_NEW(tb_connect_t);

        if (cd == NULL) {
            cd = rs_sysi_init();
        } else {
            rs_sysi_link(cd);
        }

        dbe_db_initcd(tdb->tdb_db, cd);

        if (su_usrid_usertaskqueue) {
            rs_sysi_setuserid(cd, -1);
        }

        ss_dprintf_1(("tb_sysconnect_init:cd=%ld\n", (long)cd));

        auth = rs_auth_init(cd, (char *)RS_AVAL_SYSNAME, -1L, TRUE);
        rs_sysi_insertauth(cd, auth);
        rs_auth_setsyncadmin(cd, auth);

        user = dbe_user_init(tdb->tdb_db, cd, (char *)RS_AVAL_SYSNAME, file, line);
        ss_dassert(user != NULL);
        rs_sysi_insertuser(cd, user);

        rs_sysi_inserttabdb(cd, tdb);

        rs_sysi_insertsqlinfo(cd, tdb->tdb_sqli);
        rs_sysi_setxsmgr(cd, tdb->tdb_xsmgr);
        rs_sysi_insertrslinksemarray(cd, tdb->tdb_rslinksemarray);

        ss_debug(tc->tc_chk = TBCHK_TCON);
        tc->tc_sysinfo = cd;
        tc->tc_sqltrans = NULL;
        tc->tc_sqls = NULL;
        tc->tc_db = tdb->tdb_db;
        tc->tc_nlink = 1;
        tc->tc_allowlogfailure = FALSE;
        tc->tc_sysconnect = TRUE;
        tc->tc_tdb = tdb;
        tc->tc_userid = -1;

        tc->tc_sqls_maintenance = NULL;
        tc->tc_sqls_use_maintenance = 0;

        rs_sysi_inserttbcon(cd, tc);
        rs_sysi_setconnecttype(tc->tc_sysinfo, RS_SYSI_CONNECT_SYSTEM);

        if (rs_sysi_geteventnotifiers(cd) == NULL) {
            rs_sysi_seteventnotifiers(cd, tdb->tdb_events);
        }

        rs_sysi_set_uniquemsgid_fun(cd, tb_getnewuniquemsgid);

        SS_POPNAME;

        return(tc);
}

/*##**********************************************************************\
 *
 *              tb_sysconnect_init_ex
 *
 * Initializes a system connection.
 *
 * Parameters :
 *
 *      tdb - in, hold
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
tb_connect_t* tb_sysconnect_init_ex(tb_database_t* tdb, char* file, int line)
{
        tb_connect_t* tc;

        SS_PUSHNAME("tb_sysconnect_init");

        tc = tb_sysconnect_initbycd_ex(tdb, NULL, file, line);

        SS_POPNAME;

        return(tc);
}

/*##**********************************************************************\
 *
 *              tb_sysconnect_done
 *
 * Releases a system connection.
 *
 * Parameters :
 *
 *      tc - in, take
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
void tb_sysconnect_done(tb_connect_t* tc)
{
        ss_dprintf_1(("tb_sysconnect_done:sysi=%p\n", tc->tc_sysinfo));
        ss_dassert(tc->tc_chk == TBCHK_TCON);
        ss_dassert(tc->tc_nlink == 1);

        if (tc->tc_sqltrans != NULL) {
            tb_trans_done(tc->tc_sysinfo, tc->tc_sqltrans);
        }
        dbe_user_done(rs_sysi_user(tc->tc_sysinfo));
        dbe_db_donecd(tc->tc_db, tc->tc_sysinfo);
        rs_sysi_done(tc->tc_sysinfo);
        SsMemFree(tc);
}

/*##**********************************************************************\
 *
 *              tb_sysconnect_transinit
 *
 * Adds transcation object to system connect. By default there is no
 * transaction object. Note that the transaction is not started.
 *
 * Parameters :
 *
 *      tc -
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
void tb_sysconnect_transinit(tb_connect_t* tc)
{
        ss_dprintf_1(("tb_sysconnect_transinit\n"));
        ss_dassert(tc->tc_chk == TBCHK_TCON);

        if (tc->tc_sqltrans == NULL) {
            tc->tc_sqltrans = tb_trans_init(tc->tc_sysinfo);
        }
}

#ifndef SS_MYSQL
/*##**********************************************************************\
 *
 *              tb_hsbconnect_initbycd_ex
 *
 * Ints HSB connection using previously allocated cd.
 *
 * Parameters :
 *
 *              tdb -
 *
 *
 *              cd -
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
tb_connect_t* tb_hsbconnect_initbycd_ex(
        tb_database_t* tdb,
        rs_sysi_t* cd,
        char* file,
        int line)
{
        tb_connect_t* tc;

        tc = tb_sysconnect_initbycd_ex(tdb, cd, file, line);

        tc->tc_sqls = tb_sqls_init(tc->tc_sysinfo);
        tb_sync_initcatalog_force(tc->tc_sysinfo);
        rs_sysi_setconnecttype(tc->tc_sysinfo, RS_SYSI_CONNECT_HSB);

        return(tc);
}

/*##**********************************************************************\
 *
 *              tb_hsbconnect_init_ex
 *
 * Creates tb_connect object for hsb operations.
 *
 * Parameters :
 *
 *      tdb -
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
tb_connect_t* tb_hsbconnect_init_ex(
        tb_database_t* tdb,
        char* file,
        int line)
{
        tb_connect_t* tc;

        tc = tb_hsbconnect_initbycd_ex(tdb, NULL, file, line);

        return(tc);
}

/*##**********************************************************************\
 *
 *              tb_hsbconnect_done
 *
 * Deletes special tb_connect for hsb operations.
 *
 * Parameters :
 *
 *      tc -
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
void tb_hsbconnect_done(
        tb_connect_t* tc)
{
        tb_sqls_done(tc->tc_sysinfo, tc->tc_sqls);
        tb_sysconnect_done(tc);
}
#endif /* !SS_MYSQL */


#ifdef SS_HSBG2

/*##**********************************************************************\
 *
 *              tb_hsbg2_connect_initbycd_ex
 *
 * Ints HSB connection using previously allocated cd.
 *
 * Parameters :
 *
 *              tdb -
 *
 *
 *              cd -
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
tb_connect_t* tb_hsbg2_connect_initbycd_ex(
        tb_database_t* tdb,
        rs_sysi_t* cd,
        char* file,
        int line)
{
        tb_connect_t* tc;

        tc = tb_sysconnect_initbycd_ex(tdb, cd, file, line);

        ss_dassert(tc->tc_sqls == NULL);
        rs_sysi_setconnecttype(tc->tc_sysinfo, RS_SYSI_CONNECT_HSB);

        return(tc);
}

/*##**********************************************************************\
 *
 *              tb_hsbg2_connect_init_ex
 *
 * Creates tb_connect object for hsb operations.
 *
 * Parameters :
 *
 *      tdb -
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
tb_connect_t* tb_hsbg2_connect_init_ex(
        tb_database_t* tdb,
        char* file,
        int line)
{
        tb_connect_t* tc;

        tc = tb_hsbg2_connect_initbycd_ex(tdb, NULL, file, line);

        return(tc);
}

/*##**********************************************************************\
 *
 *              tb_hsbg2_connect_done
 *
 * Deletes special tb_connect for hsb operations.
 *
 * Parameters :
 *
 *      tc -
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
void tb_hsbg2_connect_done(
        tb_connect_t* tc)
{
        ss_dassert(tc->tc_sqls == NULL);
        tb_sysconnect_done(tc);
}


/*##**********************************************************************\
 *
 *              tb_database_set_sysproperties
 *
 * Sets system properties object for database
 *
 * Parameters :
 *
 *      tdb - in, use
 *              Table level database object.
 *
 *      sysproperties - in, hold
 *              The system properties object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_database_set_sysproperties(
        tb_database_t* tbd,
        tb_sysproperties_t* sysproperties)
{
        ss_dassert(tbd != NULL);
        ss_dassert(tbd->tdb_chk == TBCHK_TDB);

        tbd->tdb_sysproperties = sysproperties;
}

/*##**********************************************************************\
 *
 *              tb_database_get_sysproperties
 *
 * Gets system properties object for database
 *
 * Parameters :
 *
 *      tdb - in, use
 *              Table level database object.
 *
 *      sysproperties - in, hold
 *              The system properties object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_sysproperties_t* tb_database_get_sysproperties(
        tb_database_t* tbd)
{
        ss_dassert(tbd != NULL);
        ss_dassert(tbd->tdb_chk == TBCHK_TDB);

        return (tbd->tdb_sysproperties);
}

#endif /*  SS_HSBG2 */

/*#***********************************************************************\
 *
 *              tb_createcheckpoint_callback
 *
 * Callback function that is called at the start of atomic section of
 * checkpoint create.
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
static su_ret_t tb_createcheckpoint_callback(rs_sysi_t* cd)
{
        su_ret_t rc = SU_SUCCESS;
        rs_sysi_t* caller_cd = cd;

        ss_dprintf_3(("tb_createcheckpoint_callback\n"));
        if (cd != NULL) {
            tb_connect_t* tbcon;

            /* We need to create a new connection to ensure we have a new
             * cd object that is not shared by any other task or thread.
             */
            tbcon = tb_sysconnect_init(rs_sysi_tabdb(cd));
            cd = tb_getclientdata(tbcon);

            rs_sysi_copydbactioncounter(cd, caller_cd);
            ss_debug(rs_sysi_copydbactionshared(cd, caller_cd));

            rs_sysi_setinsidedbeatomicsection(cd, TRUE);
            tb_blobg2mgr_flushallwblobs(cd,
                                        tb_connect_getblobg2mgr(tbcon),
                                        NULL);

            if (dbe_db_isreadonly(rs_sysi_db(cd))) {
                rc = DBE_ERR_DBREADONLY;
            }

            if (rc == SU_SUCCESS) {
                if (tb_createcheckpoint_hsbcopycallback != NULL) {
                    (*tb_createcheckpoint_hsbcopycallback)();
                }
                tb_updatecardinal(cd, dbe_db_getrbuf(rs_sysi_db(cd)));

                if (dbe_db_isreadonly(rs_sysi_db(cd))) {
                    rc = DBE_ERR_DBREADONLY;
                }
            }
#ifdef SS_HSBG2
            if(rc == SU_SUCCESS) {
                tb_database_t* tdb;
                tb_sysproperties_t* sp;

                tdb = rs_sysi_tabdb(cd);
                ss_assert(tdb != NULL);


                sp = tb_database_get_sysproperties(tdb);
                if(sp != NULL) {
                    dbe_db_t* db;
                    db = tb_tabdb_getdb(tdb);
                    ss_dassert(db != NULL);

                    if (dbe_db_ishsbcopy(db)) {
                        dbe_db_reset_logpos(db);
                    }

                    tb_sysproperties_checkpoint(sp, cd);
                }
#ifdef DBE_REPLICATION

#ifndef SS_MYSQL
                if (rc == SU_SUCCESS) {
                    tb_hsb_updatehsbreptrxidmax(cd);
                }
#endif /* !SS_MYSQL */

                if (dbe_db_isreadonly(rs_sysi_db(cd))) {
                    rc = DBE_ERR_DBREADONLY;
                }
#endif
                if (dbe_db_isreadonly(rs_sysi_db(cd))) {
                    rc = DBE_ERR_DBREADONLY;
                }
            }
#endif /* SS_HSBG2 */
            rs_sysi_setinsidedbeatomicsection(cd, FALSE);

            tb_sysconnect_done(tbcon);
        }
        return(rc);
}

static rs_sysi_t* tb_connect_initcd(void* ctx)
{
        tb_database_t* tdb = ctx;
        tb_connect_t* tc;
        rs_sysi_t* cd;

        tc = tb_sysconnect_init(tdb);
        cd = tb_getclientdata(tc);
        ss_dassert(rs_sysi_tbcon(cd) == tc);

        return(cd);
}

static void tb_connect_donecd(rs_sysi_t* cd)
{
        tb_connect_t* tc;

        tc = rs_sysi_tbcon(cd);

        tb_sysconnect_done(tc);
}

/*##**********************************************************************\
 *
 *              tb_init_server
 *
 * Initializes table level, server version.
 *
 * Parameters :
 *
 *      inifile - in, hold
 *              Inifile object.
 *
 *      allow_create_db - in
 *              If TRUE, a new database is created if one does not exist.
 *
 *      recover_anyway - in
 *          If TRUE, recovery is forced to closed database, too
 *
 *      createonly - in
 *          Create database if it does not exist, otherwise return NULL.
 *
 *      p_username - out, give
 *          If non-NULL and creating a new database, database creator
 *          username is returned in *p_username
 *
 *      p_password - out, give
 *          If non-NULL and creating a new database, database creator
 *          password is returned in *p_password
 *
 *      check_existence_of_all_db_files - in
 *          If TRUE all db files defined in the ini file must exist without holes
 *
 * Return value :
 *
 *      TRUE    - initializing succeeded
 *      FALSE   - initializing failed
 *
 * Limitations  :
 *
 * Globals used :
 */
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
        dbe_cryptoparams_t* cp)
{
        tb_database_t* tdb;
        rs_sysi_t* cd;
        long sortarraysize;
        dbe_dbstate_t dbstate = DBSTATE_CLOSED;
        dbe_ret_t rc;
        char username[SU_MAXUSERNAMELEN*2+1];
        char password[SU_MAXPASSWORDLEN*2+1];
        char defcatalog[RS_MAX_NAMELEN*2 + 1];
        bool migrating_to_catalogsupp = FALSE;
        bool migrating_to_blobg2_support = FALSE;
        bool migrating_generic = FALSE;
        bool dbexist;
        bool dbexistall;
        bool reload_rbuf = FALSE;
        bool refresh_schema = FALSE;
        bool changes_in_sysrel = FALSE;
        TliConnectT* tcon;
        rs_sysi_t* tcon_cd;
        TliRetT trc;
        int i;
        su_rbt_t* newrel_rbt = NULL;
#ifdef COLLATION_UPDATE
        bool collation_found;
        bool collation_found_from_db;
        su_chcollation_t chcollation;
#endif
#ifdef SS_HSBG2
        bool sysproperties_started = FALSE;
#endif /* SS_HSBG2 */

        ss_dprintf_1(("tb_init_server\n"));
        SS_PUSHNAME("tb_init_server");
        tb_dd_migratingtounicode = FALSE;

        if (p_username != NULL) {
            *p_username = NULL;
        }
        if (p_password != NULL) {
            *p_password = NULL;
        }

        tb_error_init();
        dbexist = dbe_db_dbexist(inifile); /* TRUE if at least on of the db files exists */
        if (check_existence_of_all_db_files) {
            dbexistall = dbe_db_dbexistall(inifile); /* TRUE if there missing db files only in the end of filespecs */
            if (dbexist && !dbexistall) { /* return error */
                SS_POPNAME;
                return(TB_INIT_FILESPECPROBLEM);
            }
        }

        if (dbexist && createonly) {
            *p_tdb = NULL;
            SS_POPNAME;
            return(TB_INIT_DBEXIST);
        }
        if (!dbexist) {
            bool create_new_db;
            bool isread_only;
            rs_err_t* errh = NULL;

            isread_only = dbe_db_iscfgreadonly(inifile);
            if (!allow_create_db || isread_only) {
                /* If process is not in the foreground, cannot create
                 * a new database.
                 */
                *p_tdb = NULL;
                SS_POPNAME;
                return(TB_INIT_DBNOTEXIST);
            }

            /* The database does not exist. Ask from user, if (s)he
             * want's to create a new database. Ask also username and
             * password for dba.
             */
            create_new_db = ui_msg_getdba(
                                username, sizeof(username),
                                password, sizeof(password)
                                ,defcatalog, sizeof(defcatalog)
                );
            if (!create_new_db) {
                /* User did not want to create a new database.
                 */
                *p_tdb = NULL;
                SS_POPNAME;
                return(TB_INIT_NOTCREATE);
            }
            username[sizeof(username) - 1] = '\0';
            password[sizeof(password) - 1] = '\0';
            if (!tb_priv_checkusernamepassword(username, password, &errh)) {
                rs_sysi_t* cd;
                cd = rs_sysi_init();
                ui_msg_error(0, rs_error_geterrstr(cd, errh));
                rs_error_free(cd, errh);
                rs_sysi_done(cd);
                *p_tdb = NULL;
                SS_POPNAME;
                return(TB_INIT_NOTCREATE);
            }

            if (p_username != NULL) {
                *p_username = SsMemStrdup(username);
            }
            if (p_password != NULL) {
                *p_password = SsMemStrdup(password);
            }
            defcatalog[sizeof(defcatalog) - 1] = '\0';
            rs_sdefs_setnewdefcatalog(defcatalog);
        }

        tdb = SSMEM_NEW(tb_database_t);
        ss_debug(tdb->tdb_chk = TBCHK_TDB);
        tdb->tdb_admincommandfp = NULL;
#ifdef SS_HSBG2
        tdb->tdb_sysproperties = tb_sysproperties_init();
#endif /* SS_HSBG2 */

        ss_svc_notify_init();

        tdb->tdb_maxcmplen = TB_DEFAULT_MAXCMPLEN;
        tdb->tdb_defaultistransient = FALSE;
        tdb->tdb_defaultisglobaltemporary = FALSE;

        {
            bool foundp;
            long tb_maxcmplen;
            bool defaultistransient;
            bool defaultisglobaltemporary;

            foundp = su_inifile_getlong(
                        inifile,
                        SU_SRV_SECTION,
                        SU_SRV_MAXCMPLEN,
                        &tb_maxcmplen);
            if (foundp) {
                tdb->tdb_maxcmplen = tb_maxcmplen;
            }
            ss_dprintf_1(("tb_init_server:maxcmplen %d\n", tdb->tdb_maxcmplen));
            foundp = su_inifile_getbool(
                        inifile,
                        SU_DBE_GENERALSECTION,
                        SU_DBE_DEFAULTISTRANSIENT,
                        &defaultistransient);
            if (foundp) {
                tdb->tdb_defaultistransient = defaultistransient;
            }
            foundp = su_inifile_getbool(
                        inifile,
                        SU_DBE_GENERALSECTION,
                        SU_DBE_DEFAULTISGLOBALTEMPORARY,
                        &defaultisglobaltemporary);
            if (foundp) {
                tdb->tdb_defaultisglobaltemporary = defaultisglobaltemporary;
            }
        }

        if (recover_anyway) {
            dbstate = DBSTATE_CRASHED;
        }

        cd = rs_sysi_init();
        rs_sysi_setcryptopar(cd, cp);

        for (i = 0; i < RS_SYSI_MAXLINKSEM; i++) {
            tdb->tdb_rslinksemarray[i] = SsQsemCreateLocal(SS_SEMNUM_RES_RSLINK);
        }
        rs_sysi_insertrslinksemarray(cd, tdb->tdb_rslinksemarray);

        dbe_db_openstate = DBE_DB_OPEN_DBFILE;

        tdb->tdb_db = dbe_db_init(cd,
                                  inifile,
                                  &dbstate,
                                  tb_dd_migratetounicode_done,
                                  force_inmemory_freelist
#ifdef SS_HSBG2
                                  , (void *) tdb->tdb_sysproperties
#endif /* SS_HSBG2 */
                                );

        dbe_db_openstate = DBE_DB_OPEN_NONE;

        if (dbstate == DBSTATE_BROKENNETCOPY) {
            dbe_db_done(tdb->tdb_db);
            for (i = 0; i < RS_SYSI_MAXLINKSEM; i++) {
                SsQsemFree(tdb->tdb_rslinksemarray[i]);
            }
            SsMemFree(tdb);
            *p_tdb = NULL;
            SS_POPNAME;
            return(TB_INIT_BROKENNETCOPY);
        }

        dbe_db_setreloadrbuffun(tdb->tdb_db, tb_reload_rbuf, tdb);
        dbe_db_setinitconnectcdfun(tdb->tdb_db, tdb, tb_connect_initcd, tb_connect_donecd);
        tdb->tdb_blobg2mgr = tb_blobg2mgr_init(tdb->tdb_db);
        dbe_db_setcheckpointcallback(
            tdb->tdb_db,
            tb_createcheckpoint_callback);

        ss_dassert(tdb->tdb_db != NULL);
        if (RS_AVAL_DEFCATALOG_NEW != NULL &&
            RS_AVAL_DEFCATALOG == NULL)
        {
            migrating_to_catalogsupp = TRUE;
        }

#ifdef COLLATION_UPDATE
        collation_found = tb_get_collation_from_inifile(
                                inifile,
                                &tdb->tdb_chcollation);
#endif
        tdb->tdb_sqli = rs_sqli_init(inifile);

#ifndef SS_MYSQL
        rs_sqli_setsqlversion(tdb->tdb_sqli, (char*)sql_ver());
#endif

        tdb->tdb_schema = NULL;
#ifdef SS_MYSQL
        tdb->tdb_events = NULL;
#else
        tdb->tdb_events = rs_eventnotifiers_init();
#endif

        sortarraysize = rs_sqli_getsortarraysize(tdb->tdb_sqli);
        if (sortarraysize < 100) {
            sortarraysize = 100;
        }
        tdb->tdb_xsmgr = xs_mgr_init(tdb->tdb_db, inifile, sortarraysize);

        tdb->tdb_cpactive = FALSE;

# ifdef COLLATION_UPDATE
        rs_aval_avfunglobalinit(tdb->tdb_chcollation);
# else   /* COLLATION_UPDATE */
        rs_aval_avfunglobalinit(SU_CHCOLLATION_FIN);
# endif  /* COLLATION_UPDATE */

        dbe_db_openstate = DBE_DB_OPEN_DBFILE;

        tdb->tdb_sysconnect = tb_sysconnect_initbycd_ex(tdb, cd, (char *)__FILE__, __LINE__);
        ss_dassert(tdb->tdb_sysconnect->tc_sysinfo == cd);

        rs_sysi_done(cd);   /* Remove rs_sysi_link done in above call. */

        if (dbstate == DBSTATE_NEW) {
            bool succp;
            long userid;
            char* srvchset_username;    /* Username converted to server chset. */
            char* srvchset_password;    /* Password converted to server chset. */

            ss_dassert(!dbexist);

            ui_msg_message(DBE_MSG_CREATING_NEW_DB);

            tcon = TliConnectInit(cd);

            tb_createsysrel(
                cd,
                TliGetTrans(tcon),
                rs_sysi_rbuf(cd),
                rs_sysi_auth(cd)
#ifdef COLLATION_UPDATE
                ,tdb->tdb_chcollation
#endif
                );

            /* Convert username and password to uppercase server
             * character set.
             */
#ifdef COLLATION_UPDATE
            collation_found_from_db = TRUE;
            srvchset_username = su_chcvt_strcvtuprdup(username, tdb->tdb_chcollation);
            srvchset_password = su_chcvt_strcvtuprdup(password, tdb->tdb_chcollation);
#else
            srvchset_username = su_chcvt_strcvtuprdup(username, SU_CHCOLLATION_FIN);
            srvchset_password = su_chcvt_strcvtuprdup(password, SU_CHCOLLATION_FIN);
#endif

            succp = tb_priv_usercreate(
                        tcon,
                        srvchset_username,
                        srvchset_password,
                        TB_UPRIV_ADMIN,
                        &userid,
                        NULL);
            ss_assert(succp);

            memset(password, '\0', sizeof(password));
            memset(srvchset_password, '\0', strlen(srvchset_password));
            SsMemFree(srvchset_username);
            SsMemFree(srvchset_password);

            if (tb_updatesysrel(cd, TliGetTrans(tcon), rs_sysi_rbuf(cd),
                                &newrel_rbt,
                                FALSE,
                                SU_CHCOLLATION_FIN)) /* Not used here! */

            {
                /* There were changes in system tables. */
                tb_priv_updatesysrel(cd, newrel_rbt);
            }

            tb_priv_initsysrel(cd);
            trc = TliCommit(tcon);
            ss_dassert(trc == TLI_RC_SUCC);

            TliConnectDone(tcon);

            tb_dd_loadrbuf(cd, rs_sysi_rbuf(cd), FALSE, FALSE);

#ifdef DBE_REPLICATION
#ifndef SS_MYSQL
            dbe_db_sethsbmode(tdb->tdb_db, cd, tb_hsb_gethsbmode(cd));
            tb_hsb_sethsbreptrxidmaxtodb(cd);
#endif /* !SS_MYSQL */
#endif /* DBE_REPLICATION */

#ifdef SS_HSBG2

#ifndef SS_MYSQL
            dbe_db_hsbg2_mme_newdb(tdb->tdb_db);
#endif /* !SS_MYSQL */

            tb_sysproperties_start(tdb->tdb_sysproperties, cd);
            sysproperties_started = TRUE;
#endif /* SS_HSBG2 */

        } else { /* Not a new database */
            ss_dassert(dbexist);

#ifdef SS_LICENSEINFO_V3
            {
                su_li3_ret_t li_rc;

                li_rc = su_li3_checkdbage(
                            dbe_db_getcreatime(tdb->tdb_db));
                if (li_rc != SU_LI_OK) {
                    char* errortext;
                    char* e = (char *)", exiting from ";

                    errortext = su_rc_givetext(li_rc);
                    ss_dassert(errortext != NULL);
                    errortext = SsMemRealloc(errortext,
                                    strlen(errortext) +
                                    strlen(e) +
                                    strlen(SS_SERVER_NAME) +
                                    1);
                    strcat(errortext, e);
                    strcat(errortext, SS_SERVER_NAME);
                    ui_msg_error(0, errortext);
                    SsMemFree(errortext);
                    SsExit(1);
                }
            }
#else /* SS_LICENSEINFO_V3 */
            {
                su_li2_ret_t li_rc;

                li_rc = su_li2_checkdbage(
                            dbe_db_getcreatime(tdb->tdb_db));
                if (li_rc != SU_LI_OK) {
                    char* errortext;
                    char* e = ", exiting from ";

                    errortext = su_rc_givetext(li_rc);
                    ss_dassert(errortext != NULL);
                    errortext = SsMemRealloc(errortext,
                                    strlen(errortext) +
                                    strlen(e) +
                                    strlen(SS_SERVER_NAME) +
                                    1);
                    strcat(errortext, e);
                    strcat(errortext, SS_SERVER_NAME);
                    ui_msg_error(0, errortext);
                    SsMemFree(errortext);
                    SsExit(1);
                }
            }
#endif /* SS_LICENSEINFO_V3 */

            tb_dd_loadrbuf(cd, rs_sysi_rbuf(cd), FALSE, FALSE);

#ifdef DBE_REPLICATION
            /* JarmoR moved to here Jun 17, 2000. Need to be after
               tb_dd_loadrbuf. */
#ifndef SS_MYSQL
            dbe_db_sethsbmode(tdb->tdb_db, cd, tb_hsb_gethsbmode(cd));
            tb_hsb_sethsbreptrxidmaxtodb(cd);
#endif /* !SS_MYSQL */

#endif /* DBE_REPLICATION */

#ifndef SS_NOCOLLATION
# ifdef COLLATION_UPDATE

            collation_found_from_db = tb_dd_getcollation(cd, &chcollation);
            if (chcollation != tdb->tdb_chcollation && collation_found) {
                su_rc_fatal_error(
                        E_FATAL_PARAM_SSSSS,
                        SU_DBE_GENERALSECTION,
                        SU_DBE_COLLATION_SEQ,
                        su_chcollation_name(tdb->tdb_chcollation),
                        su_inifile_getname(inifile),
                        su_chcollation_name(chcollation));
                ss_derror;
            }
            tdb->tdb_chcollation = chcollation;
# endif /* COLLATION_UPDATE */

#else /* SS_NOCOLLATION */
            tdb->tdb_chcollation = SU_CHCOLLATION_ISO8859_1;
#endif /* SS_NOCOLLATION */

#ifdef SS_HSBG2
        tdb->tdb_schema = tb_schema_globalinit(cd);
#endif /* SS_HSBG2 */

			dbe_db_startupforcemergeif(cd, tdb->tdb_db);

#ifdef SS_MME
#ifndef SS_MYSQL
            {
                tb_recovctx_t* recovctx;

                recovctx = tb_recovctx_init(tdb);

                rc = dbe_db_loadmme(cd,
                                 tdb->tdb_db,
                                 tb_recovctx_getrelh,
                                 tb_recovctx_getrelh_trxinfo,
                                 recovctx);
                tb_recovctx_commit(recovctx);
                tb_recovctx_done(recovctx);
            }
#endif /* !SS_MYSQL */
#endif /* SS_MME */

            tb_sysproperties_start(tdb->tdb_sysproperties, cd);
            sysproperties_started = TRUE;
            dbe_db_setstarthsbstate(tdb->tdb_db, tb_sysproperties_gethsbstate(tdb->tdb_sysproperties));

            /* Fastfoot: generic migrate branch added */
            migrating_generic = dbe_db_migrateneeded(tdb->tdb_db);
            if (migrating_generic &&
                convert_type == TB_CONVERT_NO)
            {
                dbe_db_setreadonly(tdb->tdb_db, TRUE);
                tb_done_server(tdb);
                *p_tdb = NULL;
                dbe_db_openstate = DBE_DB_OPEN_NONE;
                SS_POPNAME;
                return(TB_INIT_NOCONVERT);
            }

            if (dbstate == DBSTATE_CRASHED) {
                ulong ncommits;
                tb_recovctx_t* recovctx;
                dbe_db_recovcallback_t recovcallback;

                recovctx = tb_recovctx_init(tdb);

                recovcallback.rc_getrelhfun = tb_recovctx_getrelh;
                recovcallback.rc_getrelhfun_byrelid = tb_recovctx_getrelh_byrelid;
                recovcallback.rc_refreshrbuffun = tb_recovctx_refreshrbuf;
                recovcallback.rc_reloadrbuffun = tb_recovctx_fullrefreshrbuf;
                recovcallback.rc_rbufctx = recovctx;

                dbe_db_openstate = DBE_DB_OPEN_LOGFILE;

                ui_msg_warning(DBE_MSG_STARTING_RECOVERY);

                rc = dbe_db_recover(
                        tdb->tdb_db,
                        rs_sysi_user(cd),
                        &recovcallback,
                        &ncommits);

                su_rc_assert(rc == DBE_RC_SUCC ||
                             rc == DBE_RC_END ||
                             rc == DBE_RC_LOGFILE_TAIL_CORRUPT, rc);
                if (rc == DBE_RC_LOGFILE_TAIL_CORRUPT) {
                    ui_msg_warning(LOG_MSG_IGNORE_CORRUPTED_PART);
                }
                if (ncommits != 0) {
                    ui_msg_warning(DBE_MSG_RECOVERY_OF_TRXS_COMPLETED_U, ncommits);
                } else {
                    ui_msg_warning(DBE_MSG_RECOVERY_COMPLETED);
                }
                dbe_db_openstate = DBE_DB_OPEN_DBFILE;
                reload_rbuf = TRUE;
                refresh_schema = TRUE;

                tb_recovctx_commit(recovctx);
                tb_recovctx_done(recovctx);
            } else { /* cannot be crashed (verified in dbe0db.c) */
                migrating_to_blobg2_support =
                    dbe_db_migratetoblobg2(tdb->tdb_db);
                if (migrating_to_blobg2_support &&
                    convert_type == TB_CONVERT_NO)
                {
                    tb_done_server(tdb);
                    *p_tdb = NULL;
                    dbe_db_openstate = DBE_DB_OPEN_NONE;
                    SS_POPNAME;
                    return(TB_INIT_NOCONVERT);
                }
#ifdef SS_UNICODE_SQL
                tb_dd_migratingtounicode =
                    dbe_db_migratetounicode(tdb->tdb_db);
                if (tb_dd_migratingtounicode) {
                    if (convert_type == TB_CONVERT_NO) {
                        tb_done_server(tdb);
                        *p_tdb = NULL;
                        dbe_db_openstate = DBE_DB_OPEN_NONE;
                        SS_POPNAME;
                        return(TB_INIT_NOCONVERT);
                    }
                }
#endif /* SS_UNICODE_SQL */
                if (migrating_to_catalogsupp
                && convert_type == TB_CONVERT_NO)
                {
                    tb_done_server(tdb);
                    *p_tdb = NULL;
                    dbe_db_openstate = DBE_DB_OPEN_NONE;
                    SS_POPNAME;
                    return (TB_INIT_NOCONVERT);
                }
            }
        }

#ifndef SS_MYSQL
        {
            bool oldp = FALSE;
            bool foundp;

            foundp = su_inifile_getbool(
                        inifile,
                        SU_SQL_SECTION,
                        SU_SQL_EMULATEOLDTIMESTAMPDIFF,
                        &oldp);
            if (foundp) {
                rs_aval_settsdiffmode(oldp);
            }
        }
#endif /* !SS_MYSQL */

        rs_aval_setloadblobcallbackfun(
                cd,
                tb_blobg2_loadblobtoaval_limit);
        {
            long blobsizelimit;
            bool foundp;

            foundp = su_inifile_getlong(
                        inifile,
                        SU_SQL_SECTION,
                        SU_SQL_BLOBEXPRLIMIT,
                        &blobsizelimit);
            if (foundp) {
#ifdef SS_UNICODE_SQL
                size_t old_blobsizelimit = rs_aval_getloadblobsizelimit(cd);

                if (!(tb_dd_migratingtounicode &&
                      (long)old_blobsizelimit > blobsizelimit))
#endif /* SS_UNICODE_SQL */
                {
                    if ((ulong)blobsizelimit > SS_MAXALLOCSIZE) {
                        blobsizelimit = SS_MAXALLOCSIZE;
                    } else if (blobsizelimit < 1024L) {
                        blobsizelimit = 1024L;
                    }
                    rs_aval_setloadblobsizelimit(cd, (size_t)blobsizelimit);
                }
            }
        }

        ss_svc_notify_init();

        rc = dbe_db_open(tdb->tdb_db);
        su_rc_assert(rc == DBE_RC_SUCC, rc);
#ifdef SS_UNICODE_SQL
        if (!tb_dd_migratingtounicode)
#endif /* SS_UNICODE_SQL */
        {
            if (migrating_generic) {
                rs_err_t* errh = NULL;
                bool succp = dbe_db_setchanged(tdb->tdb_db, &errh);
                if (!succp) {
                    ss_dassert(errh != NULL);
                    su_rc_fatal_error(E_FATAL_GENERIC_S, rs_error_geterrstr(cd, errh));
                }
            }
            rc = tb_createcheckpoint(tdb->tdb_sysconnect, FALSE);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
        }

#ifdef SS_HSBG2
        if (tdb->tdb_schema != NULL) {
            tb_schema_globaldone(tdb->tdb_schema);
            tdb->tdb_schema = NULL;
        }
#endif /* SS_HSBG2 */

        tdb->tdb_schema = tb_schema_globalinit(cd);

#ifndef SS_NOSQL

        tcon = TliConnectInit(cd);
        tcon_cd = TliGetCd(tcon);

        /* Check if any changes are needed in the system relations.
         */
        if (!dbe_db_iscfgreadonly(inifile)) {

            bool call_updatesysrel;

#ifdef SS_HSBG2
            tb_trans_settransoption(tcon_cd, TliGetTrans(tcon), TB_TRANSOPT_NOLOGGING);
            dbe_db_hsbenable_syschanges(tdb->tdb_db, TRUE);
#endif

#if defined(SS_NEW_ST) 
            /* Do system relation update check only in database
             * conversion case.
             */
            call_updatesysrel = convert_type != TB_CONVERT_NO;
#else
            call_updatesysrel = TRUE;
#endif

            if (call_updatesysrel
                && tb_updatesysrel(
                    tcon_cd,
                    TliGetTrans(tcon),
                    rs_sysi_rbuf(tcon_cd),
                    &newrel_rbt,
#ifdef COLLATION_UPDATE
                    !collation_found_from_db,
                    tdb->tdb_chcollation

#else  /* COLLATION_UPDATE */
                    FALSE,
                    SU_CHCOLLATION_FIN
#endif /* COLLATION_UPDATE */
                ))
            {
                /* There were changes in system tables. */
                tb_priv_updatesysrel(cd, newrel_rbt);
                refresh_schema = TRUE;
                reload_rbuf = TRUE;
                changes_in_sysrel = TRUE;
            }

#ifdef SS_HSBG2
            dbe_db_hsbenable_syschanges(tdb->tdb_db, FALSE);
#endif
        }
#ifdef SS_HSBG2
        if(!sysproperties_started) {
            tb_sysproperties_start(tdb->tdb_sysproperties, cd);
            sysproperties_started = TRUE;
        }
#endif /* SS_HSBG2 */

#ifdef SS_UNICODE_SQL
        if (tb_dd_migratingtounicode) {
            /* Note this must be the last DD operation,
             * because rbuf and system table are not
             * synchronized!
             */
            ui_msg_warning(DBE_MSG_CONVERTING_DB);
            rs_rbuf_removeallnames(cd, rs_sysi_rbuf(cd));
            tb_dd_loadrbuf(cd, rs_sysi_rbuf(cd), FALSE, FALSE);

#ifndef SS_MYSQL
            tb_migratetounicode(cd, TliGetTrans(tcon));
#endif /* !SS_MYSQL */

            changes_in_sysrel = TRUE;
        }
        else {
            if (reload_rbuf) {
                tb_dd_loadrbuf(cd, rs_sysi_rbuf(cd), TRUE, TRUE);
            }
#ifdef SS_MYSQL
            if (refresh_schema) {
                tb_resetschema(tdb, cd);
            }
#else
            if (tb_dd_updatedefcatalog(cd, TliGetTrans(tcon))
            || refresh_schema) {
                tb_resetschema(tdb, cd);
            }
#endif

        }
#endif /* SS_UNICODE_SQL */

        trc = TliCommit(tcon);
        ss_dassert(trc == TLI_RC_SUCC);
        TliConnectDone(tcon);

#ifdef SS_UNICODE_SQL
        if (tb_dd_migratingtounicode) {
            tb_completemerge(cd, tdb);
        }
#endif /* SS_UNICODE_SQL */

        if (changes_in_sysrel) {
#ifdef SS_UNICODE_SQL
            if (tb_dd_migratingtounicode) {
                /* mark headers */
                dbe_db_migratetounicodemarkheader(tdb->tdb_db);
                /* start logging */
                dbe_db_migratetounicodecompleted(tdb->tdb_db);
            }
#endif /* SS_UNICODE_SQL */
            tb_completemerge(cd, tdb);
            rc = tb_createcheckpoint(tdb->tdb_sysconnect, FALSE);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
        }

        if (reload_rbuf
#ifdef SS_UNICODE_SQL
        &&  !tb_dd_migratingtounicode
#endif /* SS_UNICODE_SQL */
        ) {
            /* This must be done outside the update transaction. */
            tb_dd_loadrbuf(cd, rs_sysi_rbuf(cd), TRUE, TRUE);
        }
#endif /* SS_NOSQL */

        SS_POPNAME;

#ifdef SS_UNICODE_SQL
        if (tb_dd_migratingtounicode) {
            /* Recreate the database server object by using recursion.
             * This is needed for refreshing the rbuf in memory
             */
            tb_init_ret_t ret;

            tb_done_server_ex(tdb, TRUE, TRUE);
            tb_dd_migratetounicode_done = TRUE;
            if (convert_type == TB_CONVERT_AUTO) {
                ret = tb_init_server(
                        inifile,
                        FALSE,
                        FALSE,
                        FALSE,
                        TB_CONVERT_AUTO,
                        &tdb,
                        p_username,
                        p_password,
                        FALSE,
                        FALSE,
                        cp);
                *p_tdb = tdb;
                dbe_db_openstate = DBE_DB_OPEN_NONE;
                return(ret);
            } else {
                ss_dassert(convert_type == TB_CONVERT_YES);
                if (migrating_to_catalogsupp) {
                    ret = tb_init_server(
                            inifile,
                            FALSE,
                            FALSE,
                            FALSE,
                            TB_CONVERT_AUTO,
                            &tdb,
                            p_username,
                            p_password,
                            FALSE,
                            FALSE,
                            cp);
                    tb_done_server(tdb);
                    *p_tdb = NULL;
                    dbe_db_openstate = DBE_DB_OPEN_NONE;
                    if (ret == TB_INIT_SUCC) {
                        return(TB_INIT_CONVERT);
                    } else {
                        return (ret);
                    }
                }
                *p_tdb = NULL;
                dbe_db_openstate = DBE_DB_OPEN_NONE;
                return(TB_INIT_CONVERT);
            }
        } else if (convert_type == TB_CONVERT_YES) {
            tb_done_server(tdb);
            *p_tdb = NULL;
            dbe_db_openstate = DBE_DB_OPEN_NONE;
            return(TB_INIT_CONVERT);
        }
#endif /* SS_UNICODE_SQL */

#ifdef SS_SYNC
#ifndef SS_MYSQL
        tb_sync_getsyncinfo(cd);
#endif /* !SS_MYSQL */
#endif /* SS_SYNC */

        tdb->tdb_uniquemsgid_seq_id = 0;
#ifndef SS_MYSQL
        tb_setuniquemsgid_sequence(cd);
#endif /* !SS_MYSQL */

        *p_tdb = tdb;

#ifdef SS_HSBG2
        ss_dassert(sysproperties_started);
#endif /* SS_HSBG2 */

        dbe_db_openstate = DBE_DB_OPEN_NONE;

        return(TB_INIT_SUCC);
}

#if defined(DBE_MTFLUSH)

/*##**********************************************************************\
 *
 *              tb_setflushwakeupcallback
 *
 * Sets the multithread flush ended signal callback functions and
 * context for them
 *
 * Parameters :
 *
 *      tdb - use
 *          table-level database object
 *
 *      flushwakeupfp - in, hold
 *          wake up function pointer
 *
 *      flushwakeupctx - in out, hold
 *          parameter for wake up function
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
void tb_setflushwakeupcallback(
        tb_database_t* tdb,
        void (*flushwakeupfp)(void*),
        void* flushwakeupctx,
        void  (*flusheventresetfp)(void*),
        void* flusheventresetctx,
        void (*flushbatchwakeupfp)(void*),
        void* flushbatchwakeupctx)
{
        dbe_db_setflushwakeupcallback(
            tdb->tdb_db,
            flushwakeupfp,
            flushwakeupctx,
            flusheventresetfp,
            flusheventresetctx,
            flushbatchwakeupfp,
            flushbatchwakeupctx);
}

#endif /* DBE_MTFLUSH */

/*#***********************************************************************\
 *
 *              tb_done_server_ex
 *
 * Releases resources allocated in tb_init_server.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tb_done_server_ex(tb_database_t* tdb, bool convertp, bool docheckpoint)
{
        rs_sysinfo_t* cd;
        dbe_ret_t rc;
        int i;

        CHK_TDB(tdb);

        cd = tdb->tdb_sysconnect->tc_sysinfo;

        if (!dbe_db_isreadonly(tdb->tdb_db) && docheckpoint) {
            /* 11-Nov-2002 Moved to tab-level by tommi.
            MME needs cd so we have to do the checkpoint earlier. */
            dbe_db_setfinalcheckpoint(tdb->tdb_db);
            rc = dbe_db_createcheckpoint(cd, tdb->tdb_db, TRUE, FALSE);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
        }
#ifdef SS_MME
        dbe_db_clearmme(cd, tdb->tdb_db);
#endif /* SS_MME */

        ss_svc_notify_done();

#ifdef SS_HSBG2
        tb_sysproperties_done(tdb->tdb_sysproperties);
#endif /* SS_HSBG2 */

        rs_sqli_done(tdb->tdb_sqli);
        tb_sysconnect_done(tdb->tdb_sysconnect);
        tdb->tdb_sysconnect = NULL;

        if (tdb->tdb_xsmgr != NULL) {
            xs_mgr_done(tdb->tdb_xsmgr);
        }
        tb_blobg2mgr_done(tdb->tdb_blobg2mgr);
        dbe_db_done(tdb->tdb_db);

        rs_aval_avfunglobaldone();
        rs_aval_arithglobaldone();

        if (tdb->tdb_schema != NULL) {
            tb_schema_globaldone(tdb->tdb_schema);
            tdb->tdb_schema = NULL;
        }
        if (!convertp) {
            rs_sdefs_globaldone();
        }
#ifndef SS_MYSQL
        rs_eventnotifiers_done(tdb->tdb_events);
#endif

        for (i = 0; i < RS_SYSI_MAXLINKSEM; i++) {
            SsQsemFree(tdb->tdb_rslinksemarray[i]);
        }

        SsMemFree(tdb);
}

/*##**********************************************************************\
 *
 *              tb_done_server
 *
 * Releases resources allocated in tb_init_server.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_done_server(tb_database_t* tdb)
{
        tb_done_server_ex(tdb, FALSE, TRUE);
}

void tb_done_server_nocheckpoint(tb_database_t* tdb)
{
        tb_done_server_ex(tdb, FALSE, FALSE);
}

void tb_globalinit(void)
{
        SS_INIT_DEBUG;
}

void tb_globaldone(void)
{
        SsTimerGlobalDone();
        ss_trap_globaldone();
        SsThrGlobalDone();
}

/*##**********************************************************************\
 *
 *              tb_init
 *
 * Initializes table level.
 *
 * Parameters :
 *
 *      inifile - in, hold
 *              Inifile object.
 *
 * Return value :
 *
 *      TRUE    - initializing succeeded
 *      FALSE   - initializing failed
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_init(su_inifile_t* inifile,
             dbe_cryptoparams_t* cp)
{
        ss_dassert(tb_init_counter >= 0);

        if (tb_init_counter == 0) {
            tb_init_ret_t ret;

            if (tb_init_inifile != NULL) {
                su_inifile_done(inifile);
                inifile = tb_init_inifile;
                tb_init_inifile = NULL;
            }

            su_param_manager_global_init(inifile);

#ifndef SS_MYSQL
            tb_sql_globalinit();
#endif

            ret = tb_init_server(
                    inifile,
                    TRUE,
                    FALSE,
                    FALSE,
                    TB_CONVERT_AUTO,
                    &tb_local_tdb,
                    NULL,
                    NULL,
                    FALSE,
                    FALSE,
                    cp);
        }
        if (tb_local_tdb != NULL) {
            tb_init_counter++;
            return(TRUE);
        } else {
#ifndef SS_MYSQL
            tb_sql_globaldone();
#endif
            return(FALSE);
        }
}

#ifdef SS_LOCALSERVER

/*##**********************************************************************\
 *
 *              tb_setlocal
 *
 * Sets local database point to the given one.
 *
 * Parameters :
 *
 *      tdb - in, hold
 *              Table level database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_setlocal(tb_database_t* tbd)
{
        ss_dassert(tbd);
        ss_dassert(tbd->tdb_chk == TBCHK_TDB);

        tb_init_counter++;
        tb_local_tdb = tbd;
}

/*##**********************************************************************\
 *
 *              tb_getlocal
 *
 * Returns pointer to the local database
 *
 * Parameters :
 *
 *      tdb - in, hold
 *              Table level database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_database_t* tb_getlocal(void)
{
        return tb_local_tdb;
}
#endif

/*##**********************************************************************\
 *
 *              tb_done
 *
 * Releases resources allocated in tb_init.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_done(void)
{
        ss_dassert(tb_init_counter > 0);
        ss_dassert(tb_local_tdb != NULL);

        tb_init_counter--;

        if (tb_init_counter == 0) {
            tb_done_server(tb_local_tdb);
#ifndef SS_MYSQL
            tb_sql_globaldone();
#endif

            su_param_manager_global_done();
            tb_local_tdb = NULL;
        }
}

/*#**********************************************************************\
 *
 *              tb_connect_type
 *
 * Connects client to the table level.
 *
 * Parameters :
 *
 *      loginid -
 *              login id
 *
 *      username -
 *              user name
 *
 *      password -
 *              password
 *
 *      userid -
 *              user id, in case of TB_CONNECT_REPLICA_BYUSERID
 *
 *      connecttype - in
 *              Connection type.
 *
 * Return value - give :
 *
 *      connection pointer, or
 *      NULL if connection failed
 *
 * Limitations  :
 *
 * Globals used :
 */
static tb_connect_t* tb_connect_type(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        long userid,
        tb_connecttype_t connecttype,
        char* file,
        int line)
{
        tb_connect_t* tc;
        dbe_user_t*   user;
        rs_auth_t*    auth;
        TliConnectT*  tcon;
        TliRetT       trc;
        bool          succp;
        bool          isadmin;
        bool          isconsole;

        ss_dprintf_1(("tb_connect_type:connecttype = %d\n", (int)connecttype));
        CHK_TDB(tdb);
        SS_PUSHNAME("tb_connect_type");

        if (connecttype != TB_CONNECT_REPLICA_BYUSERID) {
            userid = -1L;
        }

        if (connecttype != TB_CONNECT_REPLICA_BYUSERID &&
            username == NULL && password == NULL) {

            ss_dprintf_2(("tb_connect_type:system connect\n"));
            SS_POPNAME;
            return (tdb->tdb_sysconnect);

        } else {
            tb_upriv_t upriv;

            ss_dprintf_2(("tb_connect_type:user connect\n"));

            tcon = TliConnectInit(tdb->tdb_sysconnect->tc_sysinfo);

            SS_PUSHNAME("tb_connect_type:A");
            if (connecttype == TB_CONNECT_REPLICA_BYUSERID) {
                succp = tb_priv_usercheck_byid(
                            tcon,
                            userid,
                            &username,
                            &password,
                            &upriv);
            } else {
                succp = tb_priv_usercheck(
                            tcon,
                            username,
                            password,
                            connecttype == TB_CONNECT_HSB,
                            &userid,
                            &upriv,
                            NULL);
            }

            FAKE_IF(FAKE_DBE_WRITECFGEXCEEDED) {
                succp = FALSE;
                dbe_db_setfatalerror(tdb->tdb_db,
                  SU_ERR_FILE_WRITE_CFG_EXCEEDED);
            }

            SS_POPNAME;
            trc = TliCommit(tcon);
            ss_dassert(trc == TLI_RC_SUCC);

            TliConnectDone(tcon);

            if (!succp) {
                ss_dprintf_2(("tb_connect_type:tb_priv_usercheck failed\n"));
                SS_POPNAME;
                return(NULL);
            }

            isadmin = (upriv & TB_UPRIV_ADMIN) != 0;
            isconsole = (upriv & TB_UPRIV_CONSOLE) != 0;

            switch (connecttype) {
                case TB_CONNECT_NORMAL:
                    if (upriv & TB_UPRIV_REPLICAONLY) {
                        ss_dprintf_2(("tb_connect_type:normal connect and replica only user\n"));
                        SS_POPNAME;
                        return(NULL);
                    }
                    break;
                case TB_CONNECT_REPLICA:
                case TB_CONNECT_REPLICA_BYUSERID:
                    if (upriv & TB_UPRIV_REPLICAONLY) {
                        ss_dprintf_2(("tb_connect_type:normal connect and replica only user\n"));
                        SS_POPNAME;
                        return(NULL);
                    }
                    break;
                case TB_CONNECT_ADMIN:
                    if (!isadmin) {
                        ss_dprintf_2(("tb_connect_type:admin connect and not admin\n"));
                        SS_POPNAME;
                        return(NULL);
                    }
                    break;
                case TB_CONNECT_CONSOLE:
                    if (!(isadmin || isconsole)) {
                        ss_dprintf_2(("tb_connect_type:console connect and not admin or console\n"));
                        SS_POPNAME;
                        return(NULL);
                    }
                    break;
                case TB_CONNECT_HSB:
                    ss_dassert(isadmin);
                    break;
                default:
                    ss_error;
            }

            SS_PUSHNAME("tb_connect_type:B");
            SS_PUSHNAME("tb_connect_type:B-init");
            tc = SSMEM_NEW(tb_connect_t);
            SS_POPNAME;
            ss_debug(tc->tc_chk = TBCHK_TCON);
            tc->tc_db = tdb->tdb_db;
            tc->tc_nlink = 1;
            tc->tc_allowlogfailure = FALSE;
            tc->tc_sysconnect = FALSE;
            tc->tc_userid = userid;

            tc->tc_sqls_maintenance = NULL;
            tc->tc_sqls_use_maintenance = 0;

            SS_PUSHNAME("tb_connect_type:cd and memctx");
            tc->tc_sysinfo = rs_sysi_init();
            ss_dprintf_1(("tb_connect_type:rs_sysi_init:%p\n", tc->tc_sysinfo));

            if (connecttype == TB_CONNECT_HSB) {
                rs_sysi_setconnecttype(tc->tc_sysinfo, RS_SYSI_CONNECT_HSB);
            }

            dbe_db_initcd(tc->tc_db, tc->tc_sysinfo);

            rs_sysi_setuserid(tc->tc_sysinfo, loginid);
            rs_sysi_setdbuserid(tc->tc_sysinfo, userid);
#ifdef SS_PMEM
            rs_sysi_enablememctx(tc->tc_sysinfo);
#endif

            rs_sysi_seteventnotifiers(tc->tc_sysinfo, tdb->tdb_events);

            ss_dassert(tc->tc_sysinfo != NULL);
            SS_POPNAME;

            SS_PUSHNAME("tb_connect_type:sysc");
            rs_sysi_inserttbcon(tc->tc_sysinfo, tc);
            SS_POPNAME;

            SS_PUSHNAME("tb_connect_type:auth");
            auth = rs_auth_init(tc->tc_sysinfo, username, userid, isadmin);
            rs_sysi_insertauth(tc->tc_sysinfo, auth);
            SS_POPNAME;

            SS_PUSHNAME("tb_connect_type:X");
            if (isconsole) {
                rs_auth_setconsole(tc->tc_sysinfo, auth);
            }
            if (upriv & TB_UPRIV_SYNC_ADMIN) {
                rs_auth_setsyncadmin(tc->tc_sysinfo, auth);
            }
            if (upriv & TB_UPRIV_SYNC_REGISTER) {
                rs_auth_setsyncregister(tc->tc_sysinfo, auth);
            }
            SS_POPNAME;

            SS_PUSHNAME("tb_connect_type:C");
            rs_sysi_inserttabdb(tc->tc_sysinfo, tdb);
            rs_sysi_setxsmgr(tc->tc_sysinfo, tdb->tdb_xsmgr);

            rs_sysi_insertrslinksemarray(tc->tc_sysinfo, tdb->tdb_rslinksemarray);

            user = dbe_user_init(tdb->tdb_db, tc->tc_sysinfo, username, file, line);
            ss_dassert(user != NULL);

            SS_PUSHNAME("tb_connect_type:D");
            rs_sysi_insertuser(tc->tc_sysinfo, user);
            rs_sysi_insertsqlinfo(tc->tc_sysinfo, tdb->tdb_sqli);

            tb_priv_initauth(tc->tc_sysinfo, auth);

            tc->tc_sqltrans = tb_trans_init(tc->tc_sysinfo);

#ifdef SS_MYSQL
            tc->tc_sqls = NULL;
#else
            tc->tc_sqls = tb_sqls_init(tc->tc_sysinfo);
#endif /* SS_MYSQL */

            tc->tc_tdb = tdb;

            if (tb_server_sysi_init != NULL) {
                (*tb_server_sysi_init)(tc->tc_sysinfo);
            }

#ifndef SS_MYSQL
            rs_sysi_setpassword(tc->tc_sysinfo, password);
            tb_sync_initcatalog_force(tc->tc_sysinfo);
            tb_priv_getsyncuserids(tc->tc_sysinfo, auth, username, password, NULL, -1L);
#endif /* !SS_MYSQL */

#ifndef SS_MYSQL
            rs_sysi_set_uniquemsgid_fun( tc->tc_sysinfo, tb_getnewuniquemsgid );
#endif /* !SS_MYSQL */

            ss_dprintf_2(("tb_connect_type:connect ok\n"));

            if (connecttype == TB_CONNECT_REPLICA_BYUSERID) {
                SsMemFree(username);
                ss_dassert(password != NULL);
                dynva_free(&password);
            }

            SS_POPNAME;
            SS_POPNAME;
            SS_POPNAME;
            SS_POPNAME;
            return (tc);
        }
}

/*##**********************************************************************\
 *
 *              tb_connect_ex
 *
 * Connects client to the table level.
 *
 * Parameters :
 *
 *      loginid -
 *              login id
 *
 *      username -
 *              user name
 *
 *      password -
 *              password
 *
 * Return value - give :
 *
 *      connection pointer, or
 *      NULL if connection failed
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_connect_t* tb_connect_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line)
{
        CHK_TDB(tdb);

        return(tb_connect_type(tdb, loginid, username, password, -1L, TB_CONNECT_NORMAL, file, line));
}

/*##**********************************************************************\
 *
 *              tb_connect_hsb_ex
 *
 * Connects client to the table level.
 *
 * Parameters :
 *
 *      loginid -
 *              login id
 *
 *      username -
 *              user name
 *
 *      password -
 *              password
 *
 * Return value - give :
 *
 *      connection pointer, or
 *      NULL if connection failed
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_connect_t* tb_connect_hsb_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line)
{
        CHK_TDB(tdb);

        return(tb_connect_type(tdb, loginid, username, password, -1L, TB_CONNECT_HSB, file, line));
}

/*##**********************************************************************\
 *
 *              tb_connect_admin_ex
 *
 * Connects admin client to the table level.
 *
 * Parameters :
 *
 *      loginid -
 *              login id
 *
 *      username -
 *              user name
 *
 *      password -
 *              password
 *
 * Return value - give :
 *
 *      connection pointer, or
 *      NULL if connection failed
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_connect_t* tb_connect_admin_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line)
{
        CHK_TDB(tdb);

        return(tb_connect_type(tdb, loginid, username, password, -1L, TB_CONNECT_ADMIN, file, line));
}

/*##**********************************************************************\
 *
 *              tb_connect_console_ex
 *
 * Connects console client to the table level.
 *
 * Parameters :
 *
 *      loginid -
 *              login id
 *
 *      username -
 *              user name
 *
 *      password -
 *              password
 *
 * Return value - give :
 *
 *      connection pointer, or
 *      NULL if connection failed
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_connect_t* tb_connect_console_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line)
{
        CHK_TDB(tdb);

        return(tb_connect_type(tdb, loginid, username, password, -1L, TB_CONNECT_CONSOLE, file, line));
}

/*#***********************************************************************\
 *
 *              tb_connect_buildusernamepassword
 *
 * Builds username and crypted password to be used inside the server.
 *
 * Parameters :
 *
 *      tdb -
 *
 *
 *      username -
 *
 *
 *      password -
 *
 *
 *      p_serverusername -
 *
 *
 *      p_crypt_passw -
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
void tb_connect_buildusernamepassword(
        tb_database_t* tdb,
        char* username,
        char* password,
        dstr_t* p_serverusername,
        dynva_t* p_crypt_passw)
{
        char* srvchset_username;    /* Username converted to server chset. */
        char* srvchset_password;    /* Password converted to server chset. */

        /* Convert username and password to uppercase server character set.
         */
#ifdef COLLATION_UPDATE
        srvchset_username = su_chcvt_strcvtuprdup(
                                username, tdb->tdb_chcollation);
        srvchset_password = su_chcvt_strcvtuprdup(
                                password, tdb->tdb_chcollation);
#else
        srvchset_username = su_chcvt_strcvtuprdup(
                                username, SU_CHCOLLATION_FIN);
        srvchset_password = su_chcvt_strcvtuprdup(
                                password, SU_CHCOLLATION_FIN);
#endif

        dstr_set(p_serverusername, srvchset_username);
        tb_priv_cryptpassword(srvchset_password, p_crypt_passw);

        SsMemFree(srvchset_username);
        SsMemFree(srvchset_password);
}

/*##**********************************************************************\
 *
 *              tb_connect_server_ex
 *
 * Connects to the database using non-crypted password.
 *
 * Parameters :
 *
 *      tdb -
 *
 *
 *      loginid -
 *
 *
 *      username -
 *
 *
 *      password -
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
tb_connect_t* tb_connect_server_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        char* password,
        char* file,
        int line)
{
        tb_connect_t* tbcon;
        dstr_t server_username = NULL;
        dynva_t crypt_passw = NULL;

        ss_dassert(tdb != NULL);

        tb_connect_buildusernamepassword(
            tdb,
            username,
            password,
            &server_username,
            &crypt_passw);

        tbcon = tb_connect_type(tdb, loginid, server_username, crypt_passw, -1L, TB_CONNECT_NORMAL, file, line);

        dstr_free(&server_username);
        dynva_free(&crypt_passw);

        return(tbcon);
}

/*##**********************************************************************\
 *
 *              tb_connect_replica_ex
 *
 * Connects replica client to the table level.
 *
 * Parameters :
 *
 *      loginid -
 *              login id
 *
 *      username -
 *              user name
 *
 *      password -
 *              password
 *
 * Return value - give :
 *
 *      connection pointer, or
 *      NULL if connection failed
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_connect_t* tb_connect_replica_ex(
        tb_database_t* tdb,
        int loginid,
        char* username,
        va_t* password,
        char* file,
        int line)
{
        CHK_TDB(tdb);

        return(tb_connect_type(tdb, loginid, username, password, -1L, TB_CONNECT_REPLICA, file, line));
}

/*##**********************************************************************\
 *
 *              tb_connect_replica_byuserid_ex
 *
 * Connects replica client to database using user id.
 *
 * Parameters :
 *
 *      tdb -
 *
 *
 *      loginid -
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
tb_connect_t* tb_connect_replica_byuserid_ex(
        tb_database_t* tdb,
        int loginid,
        long userid,
        char* file,
        int line)
{
        return(tb_connect_type(tdb, loginid, NULL, NULL, userid, TB_CONNECT_REPLICA_BYUSERID, file, line));
}

/*##**********************************************************************\
 *
 *              tb_connect_local_ex
 *
 * Connects client to the table level and crypts the password.
 * Also characters sets are converted from server character set to
 * database character set.
 *
 * Parameters :
 *
 *      loginid -
 *              login id
 *
 *      username -
 *              user name
 *
 *      password -
 *              password
 *
 * Return value - give :
 *
 *      connection pointer, or
 *      NULL if connection failed
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_connect_t* tb_connect_local_ex(
        int loginid,
        char* username,
        char* password,
        char* file,
        int line)
{
        tb_connect_t* tbcon;

        ss_dassert(tb_local_tdb != NULL);

        tbcon = tb_connect_server_ex(tb_local_tdb, loginid, username, password, file, line);

        return(tbcon);
}

/*##**********************************************************************\
 *
 *              tb_disconnect_task
 *
 * Disconnects client from the table level.
 *
 * Parameters :
 *
 *      tc - in, take
 *              table connection pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static int tb_disconnect_task(void* t __attribute__ ((unused)), void* td)
{
        tb_connect_t* tc = (tb_connect_t *)td;
        su_ret_t rc;
        bool finished;
        rs_err_t *errh;

        CHK_TCON(tc);

        ss_dprintf_1(("tb_disconnect_task\n"));

        if (*tb_server_shutdown_coming && tb_trans_hsbcommitsent(tc->tc_sysinfo, tc->tc_sqltrans)) {
            tb_trans_enduncertain(tc->tc_sysinfo, tc->tc_sqltrans);
        } else {
            rc = tb_trans_commit(tc->tc_sysinfo, tc->tc_sqltrans, &finished, &errh);
            if (!finished) {
                ss_dprintf_1(("tb_disconnect_task: return SRV_TASK_YIELD\n"));
                return SRV_TASK_YIELD;
            }
        }

        if (tb_server_sysi_done != NULL) {
            (*tb_server_sysi_done)(tc->tc_sysinfo);
        } else {
            ss_dprintf_1(("tb_disconnect_task:tb_server_sysi_done==NULL\n"));
        }

#ifndef SS_MYSQL
        tb_sqls_done(tc->tc_sysinfo, tc->tc_sqls);
#endif /* !SS_MYSQL */

        tb_trans_done(tc->tc_sysinfo, tc->tc_sqltrans);

        ss_dprintf_1(("tb_disconnect_task: calling dbe_user_done()\n"));
        dbe_user_done(rs_sysi_user(tc->tc_sysinfo));

        ss_dprintf_1(("tb_disconnect_task:sysi=%p\n", tc->tc_sysinfo));

        dbe_db_donecd(tc->tc_db, tc->tc_sysinfo);
        rs_sysi_done(tc->tc_sysinfo);

        SsMemFree(tc);
        ss_dprintf_1(("tb_disconnect_task: return SRV_TASK_STOP\n"));
        return SRV_TASK_STOP;
}

/*##**********************************************************************\
 *
 *              tb_rollback_at_disconnect
 *
 * Does rollback before disconnect. Can be used if for some reasons (e.g.
 * mutex ordering) rollback should be done before disconnect. Function
 * takes case also HSB specific issues at transaction rollback.
 *
 * Parameters :
 *
 *              tc -
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
bool tb_rollback_at_disconnect(tb_connect_t* tc)
{
        if (tb_trans_hsbcommitsent(tc->tc_sysinfo, tc->tc_sqltrans)
            && tb_server_task_start != NULL)
        {
            ss_dprintf_1(("tb_rollback_at_disconnect:FALSE\n"));
            return(FALSE);
        } else {
            ss_dprintf_1(("tb_rollback_at_disconnect:TRUE\n"));
            tb_trans_rollback_onestep(tc->tc_sysinfo, tc->tc_sqltrans, TRUE, NULL);
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              tb_disconnect
 *
 * Disconnects client from the table level.
 *
 * Parameters :
 *
 *      tc - in, take
 *              table connection pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_disconnect(tb_connect_t* tc)
{
        CHK_TCON(tc);

        ss_dprintf_1(("tb_disconnect\n"));

        if (tc != tc->tc_tdb->tdb_sysconnect) {
            int nlink;

            rs_sysi_rslinksem_enter(tc->tc_sysinfo);

            tc->tc_nlink--;
            ss_dassert(tc->tc_nlink >= 0);

            ss_dprintf_1(("tb_disconnect:link:%d\n", tc->tc_nlink));

            nlink = tc->tc_nlink;

            rs_sysi_rslinksem_exit(tc->tc_sysinfo);

            if (nlink == 0) {
                ss_dprintf_1(("tb_disconnect: dbe_user_done() returned\n"));

                if (!tb_rollback_at_disconnect(tc)) {
                    /* start a task to wait for the commit is done */
                    ss_dprintf_2(("tb_disconnect: start tb_disconnect_task\n"));
                    (*tb_server_task_start)(
                        tc->tc_sysinfo,
                        SU_TASK_HOTSTANDBY,
                        (char *)"tb_disconnect_task",
                        tb_disconnect_task,
                        tc);
                    return;
                }

                tb_schema_catalog_clearmode(tc->tc_sysinfo);

                if (tb_server_sysi_done != NULL) {
                    (*tb_server_sysi_done)(tc->tc_sysinfo);
                } else {
                    ss_dprintf_1(("tb_disconnect:tb_server_sysi_done==NULL\n"));
                }

#ifndef SS_MYSQL
                tb_sqls_done(tc->tc_sysinfo, tc->tc_sqls);

                if (tc->tc_sqls_maintenance != NULL) {
                    tb_sqls_done(tc->tc_sysinfo, tc->tc_sqls_maintenance);
                }
#endif /* !SS_MYSQL */

                tb_trans_done(tc->tc_sysinfo, tc->tc_sqltrans);

                ss_dprintf_1(("tb_disconnect: calling dbe_user_done()\n"));
                dbe_user_done(rs_sysi_user(tc->tc_sysinfo));

                ss_dprintf_1(("tb_disconnect:sysi=%p\n", tc->tc_sysinfo));

                dbe_db_donecd(tc->tc_db, tc->tc_sysinfo);
                rs_sysi_done(tc->tc_sysinfo);
                SsMemFree(tc);
            }
        }
}

/*##**********************************************************************\
 *
 *              tb_connect_link
 *
 * Link new reference to the table connection.
 *
 * Parameters :
 *
 *      tc - use
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
void tb_connect_link(tb_connect_t* tc)
{
        CHK_TCON(tc);

        rs_sysi_rslinksem_enter(tc->tc_sysinfo);

        tc->tc_nlink++;
        ss_dprintf_1(("tb_connect_link:%d\n", tc->tc_nlink));

        rs_sysi_rslinksem_exit(tc->tc_sysinfo);
}

bool tb_getcryptpwd_by_username(
        tb_connect_t* tc,
        char* username,
        dynva_t* p_cryptpwd_give)
{
        bool succp;
        TliRetT trc;
        TliConnectT* tcon;

        ss_dassert(username != NULL);

        if (!su_sdefs_isvalidusername(username)) {
            return(FALSE);
        }
        tcon = TliConnectInit(tc->tc_tdb->tdb_sysconnect->tc_sysinfo);
        succp = tb_priv_usercheck(
                    tcon,
                    username,
                    NULL,
                    FALSE,
                    NULL,
                    NULL,
                    p_cryptpwd_give);
        trc = TliCommit(tcon);
        ss_dassert(trc == TLI_RC_SUCC);

        TliConnectDone(tcon);
        return(succp);
}

bool tb_checkuser_cryptpwd(
        tb_connect_t* tc,
        char* username,
        va_t* crypt_passw,
        bool* p_isadmin,
        bool* p_isconsole)
{
        bool succp;
        TliRetT trc;
        TliConnectT* tcon;
        tb_upriv_t upriv;

        ss_dassert(username != NULL);

        if (!su_sdefs_isvalidusername(username)) {
            return(FALSE);
        }
        tcon = TliConnectInit(tc->tc_tdb->tdb_sysconnect->tc_sysinfo);
        succp = tb_priv_usercheck(
                    tcon,
                    username,
                    crypt_passw,
                    FALSE,
                    NULL,
                    &upriv,
                    NULL);

        if (p_isadmin != NULL) {
            *p_isadmin = (upriv & TB_UPRIV_ADMIN) != 0;
        }
        if (p_isconsole != NULL) {
            *p_isconsole = (upriv & TB_UPRIV_CONSOLE) != 0;
        }

        trc = TliCommit(tcon);
        ss_dassert(trc == TLI_RC_SUCC);

        TliConnectDone(tcon);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_checkuser
 *
 * Checks username and password.
 *
 * Parameters :
 *
 *      tc -
 *
 *
 *      username -
 *
 *
 *      password -
 *
 *
 *      p_isadmin -
 *
 *
 *      p_isconsole -
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
bool tb_checkuser(
        tb_connect_t* tc,
        char* username,
        char* password,
        bool* p_isadmin,
        bool* p_isconsole)
{
        bool succp;
        dstr_t server_username = NULL;
        dynva_t crypt_passw = NULL;

        ss_dassert(username != NULL);
        ss_dassert(password != NULL);

        if (!su_sdefs_isvalidusername(username) ||
            !su_sdefs_isvalidpassword(password)) {
            return(FALSE);
        }

        tb_connect_buildusernamepassword(
            tc->tc_tdb,
            username,
            password,
            &server_username,
            &crypt_passw);

        succp = tb_checkuser_cryptpwd(
                    tc,
                    server_username,
                    crypt_passw,
                    p_isadmin,
                    p_isconsole);

        dstr_free(&server_username);
        dynva_free(&crypt_passw);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_isvalidsetschemaname
 *
 * Checks if parameter username is a valid name to be used in SET SCHEMA
 * statement.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
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
bool tb_isvalidsetschemaname(
        void* cd,
        tb_trans_t* trans,
        char* username)
{
        return(tb_schema_isvalidsetschemaname(cd, trans, username));
}

/*##**********************************************************************\
 *
 *              tb_createcheckpoint_sethsbcopycallback
 *
 * Sets callback funtion to be called in atomic section during checkpoint
 * create.
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_createcheckpoint_sethsbcopycallback(void (*fun)(void))
{
        tb_createcheckpoint_hsbcopycallback = fun;
}

/*##**********************************************************************\
 *
 *              tb_createcheckpoint
 *
 *
 *
 * Parameters :
 *
 *      tc -
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
su_ret_t tb_createcheckpoint(
        tb_connect_t* tc,
        bool splitlog)

{
        su_ret_t rc;

        CHK_TCON(tc);

        SsSemEnter(tc->tc_tdb->tdb_rslinksemarray[0]);
        if (tc->tc_tdb->tdb_cpactive) {
            rc = DBE_ERR_CPACT;
        } else {
            rc = SU_SUCCESS;
            tc->tc_tdb->tdb_cpactive = TRUE;
        }
        SsSemExit(tc->tc_tdb->tdb_rslinksemarray[0]);

        if (rc != SU_SUCCESS) {
            return(rc);
        }

        rc = dbe_db_checkcreatecheckpoint(tc->tc_db);
        if (rc == DBE_RC_SUCC) {
            rc = dbe_db_createcheckpoint(tc->tc_sysinfo, tc->tc_db, FALSE, splitlog);

        }

        tc->tc_tdb->tdb_cpactive = FALSE;

        return(rc);
}

su_ret_t tb_createcheckpoint_start_splitlogif(
        tb_connect_t* tc,
        bool splitlog)
{
        su_ret_t rc;

        ss_dprintf_1(("tb_createcheckpoint_start_splitlogif\n"));
        CHK_TCON(tc);

        SsSemEnter(tc->tc_tdb->tdb_rslinksemarray[0]);
        if (tc->tc_tdb->tdb_cpactive) {
            rc = DBE_ERR_CPACT;
        } else {
            rc = SU_SUCCESS;
            tc->tc_tdb->tdb_cpactive = TRUE;
        }
        SsSemExit(tc->tc_tdb->tdb_rslinksemarray[0]);

        if (rc != SU_SUCCESS) {
            return(rc);
        }

        rc = dbe_db_checkcreatecheckpoint(tc->tc_db);
        if (rc == DBE_RC_SUCC) {
            rc = dbe_db_createcheckpoint_start(
                    tc->tc_sysinfo,
                    tc->tc_db,
                    splitlog);
        }

        tc->tc_tdb->tdb_cpactive = FALSE;

        return (rc);
}

bool tb_connect_last_checkpoint_split_log(
        tb_connect_t* tc)
{
        CHK_TCON(tc);

        return (dbe_db_last_checkpoint_split_log(tc->tc_sysinfo,
                                                 tc->tc_db));
}

/*##**********************************************************************\
 *
 *              tb_createcheckpoint_start
 *
 * Starts concurrent checkpoint creation
 *
 * Parameters :
 *
 *      tc - in, use
 *              connection pointer
 *
 * Return value :
 *      SU_SUCCESS
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_ret_t tb_createcheckpoint_start(tb_connect_t* tc)
{
        su_ret_t rc =
            tb_createcheckpoint_start_splitlogif(tc, FALSE);
        return (rc);
}

#if 0 /* Removed by Pete 1996-07-05 */
/*##**********************************************************************\
 *
 *              tb_createsnapshot_start
 *
 * Starts concurrent snapshot creation
 *
 * Parameters :
 *
 *      tc - in, use
 *          connection pointer
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_ret_t tb_createsnapshot_start(tb_connect_t* tc)
{
        su_ret_t rc;

        CHK_TCON(tc);

        SsSemEnter(tc->tc_tdb->tdb_rslinksemarray[0]);
        if (tc->tc_tdb->tdb_cpactive) {
            rc = DBE_ERR_CPACT;
        } else {
            rc = SU_SUCCESS;
            tc->tc_tdb->tdb_cpactive = TRUE;
        }
        SsSemExit(tc->tc_tdb->tdb_rslinksemarray[0]);

        if (rc != SU_SUCCESS) {
            return(rc);
        }

        rc = dbe_db_checkcreatecheckpoint(tc->tc_db);
        if (rc == DBE_RC_SUCC) {
            rc = dbe_db_createsnapshot_start(tc->tc_sysinfo, tc->tc_db);
        }

        tc->tc_tdb->tdb_cpactive = FALSE;

        return (rc);
}
#endif /* 0 */

/*##**********************************************************************\
 *
 *              tb_createcp_step
 *
 * Runs a step of concurrent checkpoint/snapshot creation
 *
 * Parameters :
 *
 *      tc - in, use
 *              connection pointer
 *
 * Return value :
 *      DBE_RC_CONT when the task is be continued with this function
 *      DBE_RC_END when the task is to be ended with tb_createcp_end() or
 *      error code when failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_ret_t tb_createcp_step(tb_connect_t* tc)
{
        su_ret_t rc;

        CHK_TCON(tc);

        ss_dprintf_1(("tb_createcp_step\n"));
        rc = dbe_db_createcp_step(tc->tc_sysinfo, tc->tc_db, FALSE);
        return (rc);
}

/*##**********************************************************************\
 *
 *              tb_createcp_end
 *
 * End concurrent chackpoint/snapshot creation
 *
 * Parameters :
 *
 *      tc - in, use
 *              connection pointer
 *
 * Return value :
 *      SU_SUCCESS when ok or
 *      error code when failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_ret_t tb_createcp_end(tb_connect_t* tc)
{
        su_ret_t rc;

        CHK_TCON(tc);

        rc = dbe_db_createcp_end(tc->tc_sysinfo, tc->tc_db);
        return (rc);
}

/*##**********************************************************************\
 *
 *              tb_getconnection
 *
 * Returns table connection from the table.
 *
 * Parameters :
 *
 * tdb - database pointer
 *
 * Return value :
 *
 *      table connection pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_connect_t *tb_getconnection(tb_database_t *tdb)
{
        CHK_TDB(tdb);
        return(tdb->tdb_sysconnect);
}

/*##**********************************************************************\
 *
 *              tb_getclientdata
 *
 * Returns client data pointer from the table connection.
 *
 * Parameters :
 *
 *      tc -
 *              table connection pointer
 *
 * Return value :
 *
 *      client data pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_sysinfo_t* tb_getclientdata(tb_connect_t* tc)
{
        CHK_TCON(tc);

        return(tc->tc_sysinfo);
}

/*##**********************************************************************\
 *
 *              tb_getsqltrans
 *
 * Returns SQL transaction pointer from the table connection.
 *
 * Parameters :
 *
 *      tc -
 *          table connection pointer
 *
 * Return value :
 *
 *      SQL transaction pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_trans_t* tb_getsqltrans(tb_connect_t* tc)
{
        CHK_TCON(tc);

        return(tc->tc_sqltrans);
}

/*##**********************************************************************\
 *
 *              tb_getsqls
 *
 *
 *
 * Parameters :
 *
 *      tc -
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

sqlsystem_t* tb_getsqls(tb_connect_t* tc)
{
        CHK_TCON(tc);

#ifndef SS_MYSQL

        if (tc->tc_sqls_use_maintenance > 0) {
            ss_dassert(tc->tc_sqls_maintenance != NULL);
            ss_dprintf_1(("%s, %d: return sqlsystem hurc\n", __FILE__, __LINE__));
            return(tc->tc_sqls_maintenance);
        } else {
            if (tc->tc_sqls_maintenance != NULL) {
                tb_sqls_done(tc->tc_sysinfo, tc->tc_sqls_maintenance);
                tc->tc_sqls_maintenance = NULL;
            }
        }
        ss_dprintf_1(("%s, %d: return sqlsystem normal\n", __FILE__, __LINE__));

#endif /* !SS_MYSQL */

        return(tc->tc_sqls);
}

/*##**********************************************************************\
 *
 *              tb_conn_use_hurc
 *
 * Use special history cursor for this connection
 *
 * Parameters :
 *
 *      tc -
 *
 *
 *      usep -
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
#ifndef SS_MYSQL
void tb_conn_use_hurc(
        tb_connect_t* tc,
        bool usep)
{
        CHK_TCON(tc);

        rs_sysi_setusehurc_force(tc->tc_sysinfo, usep);

        if (usep) {
            if (tc->tc_sqls_maintenance == NULL) {
                ss_dassert(tc->tc_sqls_use_maintenance == 0);
                tc->tc_sqls_maintenance = tb_sqls_init_hurc(tc->tc_sysinfo);
            }
            tc->tc_sqls_use_maintenance++;
        } else {
            ss_dassert(tc->tc_sqls_use_maintenance > 0);
            ss_dassert(tc->tc_sqls_maintenance != NULL);
            tc->tc_sqls_use_maintenance--;
        }
}
#endif /* !SS_MYSQL */


/*##**********************************************************************\
 *
 *              tb_getdb
 *
 * Returns database object pointer from the table connection.
 *
 * Parameters :
 *
 *      tc -
 *          table connection pointer
 *
 * Return value :
 *
 *      database object pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_db_t* tb_getdb(tb_connect_t* tc)
{
        CHK_TCON(tc);

        return(tc->tc_db);
}

dbe_db_t* tb_tabdb_getdb(tb_database_t* tdb)
{
        CHK_TDB(tdb);

        return(tdb->tdb_db);
}

/*##**********************************************************************\
 *
 *              tb_gettdb
 *
 * Returns table level database object pointer from the table connection.
 *
 * Parameters :
 *
 *      tc -
 *          table connection pointer
 *
 * Return value :
 *
 *      database object pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_database_t* tb_gettdb(tb_connect_t* tc)
{
        CHK_TCON(tc);

        return(tc->tc_tdb);
}

/*##**********************************************************************\
 *
 *              tb_getuserid
 *
 *
 *
 * Parameters :
 *
 *      tc -
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
long tb_getuserid(tb_connect_t* tc)
{
        CHK_TCON(tc);

        return(tc->tc_userid);
}

/*##**********************************************************************\
 *
 *              tb_setappinfo
 *
 *
 *
 * Parameters :
 *
 *      tc -
 *
 *
 *      appinfo -
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
void tb_setappinfo(
        tb_connect_t* tc,
        char* appinfo)
{
        dbe_user_t* user;

        ss_dassert(tc != NULL);

        user = rs_sysi_user(tc->tc_sysinfo);
        ss_dassert(user != NULL);

        dbe_user_setappinfo(user, appinfo);
}

/*##**********************************************************************\
 *
 *              tb_getappinfo
 *
 *
 *
 * Parameters :
 *
 *      tc -
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
char* tb_getappinfo(
        tb_connect_t* tc)
{
        dbe_user_t* user;

        if (tc == NULL) {
            /* tc may be NULL, if user has just connected. */
            return((char *)"");
        }

        user = rs_sysi_user(tc->tc_sysinfo);

        return(dbe_user_getappinfo(user));
}

/*##**********************************************************************\
 *
 *              tb_getmaxcmplen
 *
 * Returns max. cmplen for cursor constraints.
 *
 * Parameters :
 *
 *      tc -
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
int tb_getmaxcmplen(
        tb_database_t* tdb)
{
        CHK_TDB(tdb);

        return(tdb->tdb_maxcmplen);
}


/*##**********************************************************************\
 *
 *              tb_setmaxcmplen
 *
 * Sets max. cmplen for cursor constraints.
 *
 * Parameters :
 *
 *      tdb -
 *
 *
 *      maxcmplen -
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
void tb_setmaxcmplen(
        tb_database_t* tdb,
        int maxcmplen)
{
        CHK_TDB(tdb);

        ss_dassert(maxcmplen >= 0);
        ss_dassert(maxcmplen <= 128*1024);
        tdb->tdb_maxcmplen = maxcmplen;
}

bool tb_getdefaultistransient(
        tb_database_t*  tdb)
{
        CHK_TDB(tdb);

        return tdb->tdb_defaultistransient;
}

bool tb_getdefaultisglobaltemporary(
        tb_database_t*  tdb)
{
        CHK_TDB(tdb);

        return tdb->tdb_defaultisglobaltemporary;
}

/*##**********************************************************************\
 *
 *              tb_resetnamebuffers
 *
 * Resets buffered names in in-memory structures.
 *
 * Parameters :
 *
 *      tc -
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
void tb_resetnamebuffers(tb_connect_t* tc)
{
        su_profile_timer;

        ss_dprintf_1(("tb_resetnamebuffers\n"));
        ss_dassert(tc != NULL);

        su_profile_start;

        tb_dd_loadrbuf(tc->tc_sysinfo, rs_sysi_rbuf(tc->tc_sysinfo), TRUE, TRUE);

        su_profile_stop("tb_resetnamebuffers");
}

su_rbt_t* tb_getkeynameidrbt(tb_connect_t* tc)
{
        ss_dprintf_1(("tb_getkeynameidrbt\n"));
        ss_dassert(tc != NULL);

        return(tb_dd_readallkeysintorbt(tc->tc_sysinfo));

}

tb_schema_t* tb_getschema(
        tb_connect_t* tc)
{
        ss_dassert(tc != NULL);

        return(tc->tc_tdb->tdb_schema);
}

static void tb_resetschema(
        tb_database_t* tdb,
        rs_sysi_t *cd)
{
        CHK_TDB(tdb);

        if (tdb->tdb_schema != NULL) {
            tb_schema_globaldone(tdb->tdb_schema);
        }
        tdb->tdb_schema = tb_schema_globalinit(cd);
        ss_dassert(tdb->tdb_schema != NULL);
}


#ifndef SS_LIGHT
/*##**********************************************************************\
 *
 *              tb_printinfo
 *
 *
 *
 * Parameters :
 *
 *      fp -
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_printinfo(
        tb_database_t* tdb,
        void* fp)
{
        CHK_TDB(tdb);

        dbe_db_printinfo(fp, tdb->tdb_db);
}
#endif /* SS_LIGHT */

/*##**********************************************************************\
 *
 *              tb_allowsysconnectlogfailure
 *
 * Makes system connect to allow log write failures
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_allowsysconnectlogfailure(
        tb_database_t* tdb)
{
        CHK_TDB(tdb);

        if (tdb->tdb_sysconnect != NULL) {
            tdb->tdb_sysconnect->tc_allowlogfailure = TRUE;
        }
}

/*##**********************************************************************\
 *
 *              tb_connect_logfailureallowed
 *
 * Checks whether log write failure is allowed for this connection.
 * (That is: the connection is the system connection and log write has
 * failed)
 *
 * Parameters :
 *
 *      tc -
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
bool tb_connect_logfailureallowed(tb_connect_t* tc)
{
        CHK_TCON(tc);

        return (tc->tc_allowlogfailure);
}

#ifndef SS_NOSQL

/*#***********************************************************************\
 *
 *              acmd_listdelete
 *
 *
 *
 * Parameters :
 *
 *      data -
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
static void acmd_listdelete(void* data)
{
        tb_acmd_t* ac = data;

        if (!ac->ac_isadmevent) {
            SsMemFree(ac->ac_str);
        }
        else {
            rs_tval_free(ac->ac_cd, ac->ac_ttype, ac->ac_tval);
            rs_ttype_free(ac->ac_cd, ac->ac_ttype);
        }
        SsMemFree(ac);
}

/*##**********************************************************************\
 *
 *              tb_acmd_listinit
 *
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_list_t* tb_acmd_listinit(void)
{
        return(su_list_init(acmd_listdelete));
}

/*##**********************************************************************\
 *
 *              tb_admincommand
 *
 * Executes admin command.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tc -
 *
 *
 *      cmd -
 *
 *
 *      p_finishedp -
 *
 *
 *      p_ctx -
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
su_list_t* tb_admincommand(
        rs_sysi_t* cd,
        tb_connect_t* tc,
        char* cmd,
        int* acmdstatus,
        bool* p_finishedp,
        tb_admin_cmd_t* tb_admincmd,
        void** p_ctx)
{
        su_list_t* list;

        list = tb_acmd_listinit();

        if (tc->tc_tdb->tdb_admincommandfp != NULL) {
            (*tc->tc_tdb->tdb_admincommandfp)(
                cd,
                list,
                cmd,
                acmdstatus,
                p_finishedp,
                tb_admincmd,
                p_ctx);
        }
        return(list);
}

/*##**********************************************************************\
 *
 *              tb_adminevent
 *
 * Executes admin event commend.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tc -
 *
 *
 *      cmd -
 *
 *
 *      p_isadmevent -
 *
 *
 *      p_finishedp -
 *
 *
 *      p_succp -
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
su_list_t* tb_adminevent(
        rs_sysi_t* cd,
        tb_connect_t* tc,
        char* cmd,
        bool* p_isadmevent,
        bool* p_finishedp,
        bool* p_succp)
{
        su_list_t* list = NULL;

        if (p_isadmevent == NULL) {
            list = tb_acmd_listinit();
        }
        if (tc->tc_tdb->tdb_admineventfp != NULL) {
            (*tc->tc_tdb->tdb_admineventfp)(
                cd,
                list,
                cmd,
                p_isadmevent,
                p_finishedp,
                p_succp);
        }
        return(list);
}

/*##**********************************************************************\
 *
 *              tb_setadmincommandfp
 *
 *
 *
 * Parameters :
 *
 *      tc -
 *
 *
 *      admincommandfp -
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
void tb_setadmincommandfp(
        tb_connect_t* tc,
        void (*admincommandfp)(
            rs_sysi_t* cd,
            su_list_t* list,
            char* cmd,
            int* acmdstatus,
            bool* p_finishedp,
            tb_admin_cmd_t* tb_admincmd,
            void** p_ctx))
{
        tc->tc_tdb->tdb_admincommandfp = admincommandfp;
}

/*##**********************************************************************\
 *
 *              tb_setadmineventfp
 *
 *
 *
 * Parameters :
 *
 *      tc -
 *
 *
 *      admineventfp -
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
void tb_setadmineventfp(
        tb_connect_t* tc,
        void (*admineventfp)(
            rs_sysi_t* cd,
            su_list_t* list,
            char* cmd,
            bool* p_isadmevent,
            bool* p_finishedp,
            bool* p_succp))
{
        tc->tc_tdb->tdb_admineventfp = admineventfp;
}

#endif /* SS_NOSQL */

#ifdef COLLATION_UPDATE

/*##**********************************************************************\
 *
 *              tb_getcollation
 *
 * Gets the database collation sequence
 *
 * Parameters :
 *
 *      tdb - in, use
 *              table level database object
 *
 * Return value :
 *      character collation of the database as enum value
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_chcollation_t tb_getcollation(tb_database_t* tdb)
{
        CHK_TDB(tdb);
        return (tdb->tdb_chcollation);
}

/*##**********************************************************************\
 *
 *              tb_connect_getcollation
 *
 *
 *
 * Parameters :
 *
 *      tc -
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
su_chcollation_t tb_connect_getcollation(
        tb_connect_t* tc)
{
        CHK_TCON(tc);
        CHK_TDB(tc->tc_tdb);
        return(tc->tc_tdb->tdb_chcollation);
}

#endif /* COLLATION_UPDATE */

/*##**********************************************************************\
 *
 *              tb_server_setsysifuns
 *
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_server_setsysifuns(
        void (*server_sysi_init)(rs_sysi_t* sysi),
        void (*server_sysi_done)(rs_sysi_t* sysi),
        void (*server_sysi_init_functions)(rs_sysi_t* sysi))
{
        tb_server_sysi_init = server_sysi_init;
        tb_server_sysi_done = server_sysi_done;
    dbe_server_setsysifuns(
            server_sysi_init,
            server_sysi_done,
            server_sysi_init_functions);
}

void tb_server_setsrvtaskfun(
        void (*server_task_start)(
                 rs_sysinfo_t* cd,
                 SuTaskClassT taskclass,
                 char* task_name,
                 su_task_fun_t task_fun,
                 void* td),
        bool* p_sqlsrv_shutdown_coming)
{
        tb_server_task_start = server_task_start;
        tb_server_shutdown_coming = p_sqlsrv_shutdown_coming;
}

tb_blobg2mgr_t* tb_connect_getblobg2mgr(
        tb_connect_t* tc)
{
        CHK_TCON(tc);
        CHK_TDB(tc->tc_tdb);
        ss_dassert(tc->tc_tdb->tdb_blobg2mgr != NULL);
        return (tc->tc_tdb->tdb_blobg2mgr);
}

tb_blobg2mgr_t* tb_database_getblobg2mgr(
        tb_database_t* tdb)
{
        CHK_TDB(tdb);
        ss_dassert(tdb->tdb_blobg2mgr != NULL);
        return (tdb->tdb_blobg2mgr);
}

/*#***********************************************************************\
 *
 *              tb_database_pmonupdate_nomutex
 *
 * Updates pmon values from system counters. Caller must be inside
 * sqlsrv_sem.
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void tb_database_pmonupdate_nomutex(
        tb_database_t* tdb)
{
        dbe_db_t*  db;
        SsQmemStatT qms;
        dbe_dbstat_t dbs;

        db = tdb->tdb_db;
        dbe_db_getstat(db, &dbs);
        dbe_db_getmeminfo(db, &qms);

        /* File size info. */
        SS_PMON_SET(SS_PMON_DBSIZE, dbs.dbst_ind_filesize);
        SS_PMON_SET(SS_PMON_DBFREESIZE, dbs.dbst_ind_freesize);
        SS_PMON_SET(SS_PMON_LOGSIZE, dbs.dbst_log_filesize);

        /* Memory info. */
        SS_PMON_SET(SS_PMON_MEMSIZE,
            (qms.qms_sysbytecount + qms.qms_slotbytecount) / 1024);

        /* Transaction info. */
        SS_PMON_SET(SS_PMON_TRANSCOMMIT, dbs.dbst_trx_commitcnt);
        SS_PMON_SET(SS_PMON_TRANSABORT, dbs.dbst_trx_abortcnt);
        SS_PMON_SET(SS_PMON_TRANSROLLBACK, dbs.dbst_trx_rollbackcnt);
        SS_PMON_SET(SS_PMON_TRANSRDONLY, dbs.dbst_trx_readonlycnt);
        SS_PMON_SET(SS_PMON_TRANSBUFCNT, dbs.dbst_trx_bufcnt);
        SS_PMON_SET(SS_PMON_TRANSVLDCNT, dbs.dbst_trx_validatecnt);
        SS_PMON_SET(SS_PMON_TRANSACTCNT, dbs.dbst_trx_activecnt);

        /* Cache info. */
        SS_PMON_SET(SS_PMON_CACHEFIND, dbs.dbst_cac_findcnt);
        SS_PMON_SET(SS_PMON_CACHEFILEREAD, dbs.dbst_cac_readcnt);
        SS_PMON_SET(SS_PMON_CACHEFILEWRITE, dbs.dbst_cac_writecnt);
        SS_PMON_SET(SS_PMON_CACHEPREFETCH, dbs.dbst_cac_prefetchcnt);
        SS_PMON_SET(SS_PMON_CACHEPREFLUSH, dbs.dbst_cac_preflushcnt);

        /* Index info. */
        SS_PMON_SET(SS_PMON_INDWRITE, dbs.dbst_ind_writecnt);
        SS_PMON_SET(SS_PMON_INDWRITESAFTERMERGE, dbs.dbst_ind_writecntsincemerge);
        SS_PMON_SET(SS_PMON_MERGEWRITES, dbs.dbst_ind_mergewrites);

        /* Log info. */
        SS_PMON_SET(SS_PMON_LOGWRITES, dbs.dbst_log_writecnt);
        SS_PMON_SET(SS_PMON_LOGWRITESAFTERCP, dbs.dbst_log_writecntsincecp);
        SS_PMON_SET(SS_PMON_LOGFILEWRITE, dbs.dbst_log_filewritecnt);

        /* Search info. */
        SS_PMON_SET(SS_PMON_SRCHNACTIVE, dbs.dbst_sea_activecnt);
}

