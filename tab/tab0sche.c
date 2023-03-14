/*************************************************************************\
**  source       * tab0sche.c
**  directory    * tab
**  description  * Schema support functions.
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
#include <ssmem.h>
#include <sssem.h>

#include <dbe0trx.h>

#include <dt0date.h>

#include <rs0sysi.h>
#include <rs0error.h>
#include <rs0sdefs.h>
#include <rs0auth.h>

#include "tab1defs.h"
#include "tab1dd.h"
#include "tab1priv.h"
#include "tab0tli.h"
#include "tab0tran.h"
#include "tab0cata.h"
#include "tab0proc.h"
#include "tab0admi.h"
#include "tab0seq.h"
#include "tab0evnt.h"
#include "tab0relh.h"

#ifndef SS_MYSQL
#include "tab0sync.h"
#endif /* !SS_MYSQL */

#include "tab0sche.h"

#define CHK_SC(sc)  ss_dassert(SS_CHKPTR(sc) && sc->sc_chk == TBCHK_SCHEMA)
#define CHK_SI(si)  ss_dassert(SS_CHKPTR(si) && si->si_chk == TBCHK_SCHEMAINFO)

struct tb_schema_st {
        ss_debug(tb_check_t sc_chk;)            /* check field */
        SsSemT*             sc_mutex;           /* protects the schema and
                                                   from race conditions
                                                   between two concurrent
                                                   threads creating same
                                                   schema. more concurrency
                                                   could probably achieved
                                                   by higher-grained mutexing,
                                                   but what's the point --
                                                   it is not happening very
                                                   often anyway */
        SsSemT*             sc_rbt_mutex;       /* protects the rbt */
        su_rbt_t*           sc_rbt;             /* the rbt containing
                                                   the schema entries */
};

typedef struct {
        ss_debug(tb_check_t si_chk;)
        char*               si_schema;
        char*               si_catalog;
        char*               si_owner;
        long                si_userid;
        bool                si_stmtrollback;
        dbe_trxid_t         si_trxid;
        bool                si_commitp;         /* schema has been committed
                                                   and is visible to others */
        long                si_nlink;           /* link count */
        tb_schema_t*        si_sc;              /* back pointer to schema
                                                   (needed to access mutex) */
        long                si_id;
        void*               si_modeowner;
        bool                si_ismaster;
        bool                si_isreplica;
} tb_schemainfo_t;

static bool schema_allowdrop_nomutex(
        tb_schema_t*     sc,
        void*            cd,
        tb_trans_t*      trans,
        tb_schema_drop_t droptype,
        char*            schema,
        char*            catalog,
        su_err_t**       p_errh);

static bool schema_insert_nomutex(
        tb_schema_t* sc,
        rs_sysi_t* cd,
        char* name,
        char* catalog,
        char* owner,
        long uid,
        long id,
        bool transp,
        bool commitp);


static bool schema_remove_nomutex(
        tb_schema_t* sc,
        rs_sysi_t* cd,
        char* name,
        char* catalog,
        bool transp);

static dbe_trxid_t schema_gettrxid(rs_sysi_t *cd);

static int schemainfo_rbt_cmp(void* key1, void* key2)
{
        int cmp;
        tb_schemainfo_t *si1 = key1;
        tb_schemainfo_t *si2 = key2;

        CHK_SI(si1);
        CHK_SI(si2);

        cmp = strcmp(si1->si_catalog, si2->si_catalog);
        if (cmp != 0) {
            return(cmp);
        }
        return(strcmp(si1->si_schema, si2->si_schema));
}

static void schemainfo_done(tb_schemainfo_t *si)
{
        bool freep = FALSE;

        CHK_SI(si);

        /*
         * mutexing is required, because schemainfo_done may be called
         * outside of sc->sc_mutex in transaction end callbacks
         *
         */
        SsSemEnter(ss_lib_sem);
        si->si_nlink--;

        if(si->si_nlink == 0) {
            freep = TRUE;
        }
        SsSemExit(ss_lib_sem);

        if(freep) {
            SsMemFree(si->si_catalog);
            SsMemFree(si->si_schema);
            SsMemFree(si->si_owner);
            SsMemFree(si);
        }
}

static void schemainfo_rbt_delete(void* key)
{
        tb_schemainfo_t *si = key;

        CHK_SI(si);

        schemainfo_done(si);
}

static rs_trfunret_t schemainfo_insert_trend(
        rs_sysi_t* cd,
        sqltrans_t* trans __attribute__ ((unused)),
        rs_trop_t trop,
        void* ctx)
{
        bool succp;
        tb_schemainfo_t *si = ctx;
        rs_trfunret_t rc;
        tb_schema_t *sc;

        ss_dprintf_3(("schemainfo_insert_trend\n"));

        CHK_SI(si);
        sc = si->si_sc;
        CHK_SC(sc);

        /*
         * We don't need to enter the sc->sc_mutex here because
         * we are just updating information on the rbt and not
         * system tables.
         *
         */

        switch (trop) {
            case RS_TROP_AFTERSTMTROLLBACK:
                si->si_stmtrollback = TRUE;
                succp = schema_remove_nomutex(
                            sc,
                            cd,
                            si->si_schema,
                            si->si_catalog,
                            FALSE);
                ss_dassert(succp);
                /* FALLTHROUGH */

            case RS_TROP_AFTERSTMTCOMMIT:
                rc = RS_TRFUNRET_REMOVE;
                break;

            case RS_TROP_AFTERROLLBACK:
                if (si->si_stmtrollback == FALSE) {
                    succp = schema_remove_nomutex(
                            sc,
                            cd,
                            si->si_schema,
                            si->si_catalog,
                            FALSE);
                    ss_dassert(succp);
                }
                /* FALLTHROUGH */

            case RS_TROP_AFTERCOMMIT:
                si->si_commitp = TRUE;
                schemainfo_done(si);
                rc = RS_TRFUNRET_REMOVE;
                break;

            default:
                rc = RS_TRFUNRET_KEEP;
                break;
        }

        ss_dprintf_3(("schemainfo_insert_trend:rc=%d\n", rc));

        return (rc);
}

static rs_trfunret_t schemainfo_remove_trend(
        rs_sysi_t* cd,
        sqltrans_t* trans __attribute__ ((unused)),
        rs_trop_t trop,
        void* ctx)
{
        bool succp;
        tb_schemainfo_t *si = ctx;
        rs_trfunret_t rc;
        tb_schema_t *sc;

        ss_dprintf_3(("schemainfo_remove_trend\n"));

        CHK_SI(si);

        sc = si->si_sc;

        CHK_SC(sc);

        /*
         * We don't need to enter the sc->sc_mutex here because
         * we are just updating information on the rbt and not
         * system tables.
         *
         */

        switch (trop) {
            case RS_TROP_AFTERSTMTROLLBACK:
                si->si_stmtrollback = TRUE;
                succp = schema_insert_nomutex(
                            sc,
                            cd,
                            si->si_schema,
                            si->si_catalog,
                            si->si_owner,
                            si->si_userid,
                            si->si_id,
                            FALSE,
                            TRUE);
                ss_dassert(succp);
                /* FALLTHROUGH */

            case RS_TROP_AFTERSTMTCOMMIT:
                rc = RS_TRFUNRET_REMOVE;
                break;

            case RS_TROP_AFTERROLLBACK:
                if (si->si_stmtrollback == FALSE) {
                    succp = schema_insert_nomutex(
                            sc,
                            cd,
                            si->si_schema,
                            si->si_catalog,
                            si->si_owner,
                            si->si_userid,
                            si->si_id,
                            FALSE,
                            TRUE);
                    ss_dassert(succp);
                }
                /* FALLTHROUGH */

            case RS_TROP_AFTERCOMMIT:
                si->si_commitp = TRUE;
                schemainfo_done(si);
                rc = RS_TRFUNRET_REMOVE;
                break;

            default:
                rc = RS_TRFUNRET_KEEP;
                break;
        }

        ss_dprintf_3(("schemainfo_remove_trend:rc=%d\n", rc));

        return (rc);
}

static char* schemainfo_checknullstr(char* s)
{
        if (s == NULL) {
            return((char *)"");
        } else {
            return(s);
        }
}

static char* schemainfo_allocstr(char* s)
{
        return(SsMemStrdup(schemainfo_checknullstr(s)));
}

static void schemainfo_link(
        tb_schemainfo_t *si)
{
        CHK_SI(si);

        /*
         * mutexing is required, because schemainfo_done may be called
         * outside of sc->sc_mutex in transaction end callbacks
         *
         */
        SsSemEnter(ss_lib_sem);
        si->si_nlink++;
        SsSemExit(ss_lib_sem);

        return;
}

static bool schema_insert_nomutex(
        tb_schema_t* sc,
        rs_sysi_t* cd,
        char* name,
        char* catalog,
        char* owner,
        long uid,
        long id,
        bool transp,
        bool commitp)
{
        bool succp = TRUE;
        tb_schemainfo_t *si;

        ss_dprintf_3(("schema_insert:catalog: %s. name:%s committed:%d\n",
          catalog, name != NULL ? name : "NULL", commitp));

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_rbt_mutex));

        si = SSMEM_NEW(tb_schemainfo_t);

        ss_debug(si->si_chk = TBCHK_SCHEMAINFO;)
        si->si_schema = schemainfo_allocstr(name);
        si->si_catalog = schemainfo_allocstr(catalog);
        si->si_owner = schemainfo_allocstr(owner);
        si->si_userid = uid;
        si->si_nlink = 1;
        si->si_commitp = FALSE;
        si->si_sc = sc;
        si->si_trxid = schema_gettrxid(cd);
        si->si_id = id;
        si->si_modeowner = NULL;
        si->si_ismaster = FALSE;
        si->si_isreplica = FALSE;

        CHK_SI(si);

        SsSemEnter(sc->sc_rbt_mutex);

        if (!su_rbt_insert(sc->sc_rbt, si)) {
            succp = FALSE;

        } else {

            if (transp) {

                schemainfo_link(si);
                si->si_stmtrollback = FALSE;

                rs_trend_addstmtfun(
                    rs_sysi_getstmttrend(cd),
                    NULL, /* callback does not need transaction */
                    schemainfo_insert_trend,
                    si);

                rs_trend_addfun_first(
                    rs_sysi_gettrend(cd),
                    NULL, /* callback does not need transaction */
                    schemainfo_insert_trend,
                    si);

            }
            si->si_commitp = commitp;

            succp = TRUE;
        }

        SsSemExit(sc->sc_rbt_mutex);

        return (succp);
}

static bool schema_remove_nomutex(
        tb_schema_t* sc,
        rs_sysi_t* cd,
        char* name,
        char* catalog,
        bool transp)
{
        su_rbt_node_t* n;
        tb_schemainfo_t sibuf;
        bool succp = TRUE;

        ss_dprintf_3(("schema_remove:%s.%s\n", catalog, name));

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_rbt_mutex));

        ss_debug(sibuf.si_chk = TBCHK_SCHEMAINFO;)
        sibuf.si_schema = schemainfo_checknullstr(name);
        sibuf.si_catalog = schemainfo_checknullstr(catalog);

        SsSemEnter(sc->sc_rbt_mutex);

        n = su_rbt_search(sc->sc_rbt, &sibuf);

        if (n != NULL) {
            if (transp) {
                tb_schemainfo_t *si;

                si = su_rbtnode_getkey(n);

                CHK_SI(si);

                schemainfo_link(si);
                si->si_stmtrollback = FALSE;

                rs_trend_addstmtfun(
                    rs_sysi_getstmttrend(cd),
                    NULL, /* callback does not need transaction */
                    schemainfo_remove_trend,
                    si);

                rs_trend_addfun_first(
                    rs_sysi_gettrend(cd),
                    NULL, /* callback does not need transaction */
                    schemainfo_remove_trend,
                    si);
            }
            su_rbt_delete(sc->sc_rbt, n);
            succp = TRUE;
        } else {
            succp = FALSE;
        }

        SsSemExit(sc->sc_rbt_mutex);

        return (succp);
}

/*#***********************************************************************\
 *
 *              schemainfo_find_ext
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      name -
 *
 *
 *      catalog -
 *
 *
 *      p_username -
 *
 *
 *      p_uid -
 *
 *
 *      p_id -
 *
 *
 *      p_maintenance_owner -
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
static bool schema_find_ext_nomutex(
        tb_schema_t* sc,
        rs_sysi_t* cd __attribute__ ((unused)),
        char* name,
        char* catalog,
        dbe_trxid_t trxid,     /* uncommitted entries for this trxid only */
        char** p_username,
        long* p_uid,
        long* p_id,
        rs_sysi_t** p_maintenance_owner,
        bool *p_ismaster,
        bool *p_isreplica)
{
        su_rbt_node_t* n;
        tb_schemainfo_t sibuf;
        tb_schemainfo_t *si;
        bool succp = TRUE;

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_rbt_mutex));

        ss_debug(sibuf.si_chk = TBCHK_SCHEMAINFO;)
        sibuf.si_schema = schemainfo_checknullstr(name);
        sibuf.si_catalog = schemainfo_checknullstr(catalog);

        SsSemEnter(sc->sc_rbt_mutex);

        n = su_rbt_search(sc->sc_rbt, &sibuf);

        if (n != NULL) {
            si = su_rbtnode_getkey(n);

            CHK_SI(si);

            /*
             * return also uncommitted values if trxid == DBE_TRXID_NULL
             * or schemainfo was created by the same trxid
             *
             */

            if(!si->si_commitp
            && !DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL)
            && !DBE_TRXID_EQUAL(si->si_trxid, trxid)) {
                succp = FALSE;
            } else {

                if (p_username != NULL) {
                    *p_username = SsMemStrdup(si->si_owner);
                }
                if (p_uid != NULL) {
                    *p_uid = si->si_userid;
                }
                succp = TRUE;
            }
            if (p_uid != NULL) {
                *p_uid = si->si_userid;
            }
            if (p_id != NULL) {
                *p_id = si->si_id;
            }

            if (p_maintenance_owner != NULL) {
                *p_maintenance_owner = si->si_modeowner;
            }
            if (p_ismaster != NULL) {
                *p_ismaster = si->si_ismaster;
            }
            if (p_isreplica != NULL) {
                *p_isreplica = si->si_isreplica;
            }
        } else {
            succp = FALSE;
        }

        SsSemExit(sc->sc_rbt_mutex);

        return (succp);
}

static bool schema_find_nomutex(
        tb_schema_t* sc,
        rs_sysi_t* cd,
        char* name,
        char* catalog,
        dbe_trxid_t trxid,     /* uncommitted entries for this trxid only */
        char** p_username,
        long* p_uid,
        long* p_id)
{
        return(schema_find_ext_nomutex(
                    sc,
                    cd,
                    name,
                    catalog,
                    trxid,
                    p_username,
                    p_uid,
                    p_id,
                    NULL,NULL,NULL));
}

bool tb_schema_insert_catalog(
        rs_sysi_t* cd,
        char* catalog,
        long id)
{
        bool rc;
        tb_schema_t *sc;

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);
        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));

        /*
         * no need to acquire sc->sc_mutex because we only update the rbt
         *
         */

        rc = schema_insert_nomutex(
                sc,
                cd,
                NULL,
                catalog,
                NULL,
                -2L,
                id,
                TRUE,
                FALSE);

        return (rc);
}

bool tb_schema_remove_catalog(
        rs_sysi_t* cd,
        char* catalog)
{
        bool rc;

        tb_schema_t *sc;

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));

        /*
         * no need to acquire sc->sc_mutex because we only update the rbt
         *
         */

        rc = schema_remove_nomutex(
                sc,
                cd,
                NULL,
                catalog,
                TRUE);

        return (rc);
}

static dbe_trxid_t schema_gettrxid(rs_sysi_t *cd)
{
        tb_trans_t* trans;
        dbe_trx_t* trx;
        dbe_trxid_t trxid;

        trans = tb_getsqltrans(rs_sysi_tbcon(cd));
        if(trans == NULL) {
            return (DBE_TRXID_NULL);
        }
        trx = tb_trans_dbtrx(cd, trans);
        if(trx == NULL) {
            return (DBE_TRXID_NULL);
        }
        trxid = dbe_trx_getusertrxid(trx);

        return (trxid);
}

bool tb_schema_find_catalog(
        rs_sysi_t* cd,
        char* catalog,
        long* p_id)
{
        tb_schema_t *sc;
        bool rc;

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);

        /*
         * no need to acquire sc->sc_mutex because we only update the rbt
         *
         */

        rc = schema_find_nomutex(
                sc,
                cd,
                NULL,
                catalog,
                schema_gettrxid(cd),
                NULL,
                NULL,
                p_id);

        return (rc);
}

void tb_schema_globaldone(tb_schema_t *sc)
{
        CHK_SC(sc);

        if(sc->sc_mutex != NULL) {
            SsSemFree(sc->sc_mutex);
        }

        if(sc->sc_rbt_mutex != NULL) {
            SsSemFree(sc->sc_rbt_mutex);
        }

        if(sc->sc_rbt != NULL) {
            su_rbt_done(sc->sc_rbt);
        }

        SsMemFree(sc);
}

/*##**********************************************************************\
 *
 *              tb_schema_find_catalog_mode
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      catalog -
 *
 *
 *      p_id -
 *
 *
 *      p_cd -
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
bool tb_schema_find_catalog_mode(
        rs_sysi_t* cd,
        char* catalog,
        long* p_id,
        rs_sysi_t** p_cd,
        bool *p_ismaster,
        bool *p_isreplica)
{
        tb_schema_t *sc;
        bool rc;

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);

        /*
         * no need to acquire sc->sc_mutex because we only update the rbt
         *
         */

        rc = schema_find_ext_nomutex(
                sc,
                cd,
                NULL,
                catalog,
                schema_gettrxid(cd),
                NULL,
                NULL,
                p_id,
                p_cd,
                p_ismaster,
                p_isreplica);

        return (rc);
}


/*##**********************************************************************\
 *
 *              schema_catalog_setmaster_nomutex
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      sc -
 *
 *
 *      catalog -
 *
 *
 *      ismaster -
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
static bool schema_catalog_setmaster_nomutex(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_schema_t *sc,
        char* catalog,
        bool ismaster)
{
        su_rbt_node_t* n;
        tb_schemainfo_t sibuf;
        tb_schemainfo_t* si;
        bool rc = FALSE;

        SS_PUSHNAME("schema_catalog_setmaster_nomutex");
        ss_dprintf_3(("schema_catalog_setmaster_nomutex:cd=%ld, catalog=%s, mode=%d\n",
            (long)cd, schemainfo_checknullstr(catalog), ismaster));

        ss_debug(sibuf.si_chk = TBCHK_SCHEMAINFO;)
        sibuf.si_schema = schemainfo_checknullstr(NULL);
        sibuf.si_catalog = schemainfo_checknullstr(catalog);

        n = su_rbt_search(sc->sc_rbt, &sibuf);

        if (n != NULL) {
            si = su_rbtnode_getkey(n);

            CHK_SI(si);

            si->si_ismaster = ismaster;

            rc = TRUE;
        } else {
            rc = FALSE;
        }

        SS_POPNAME;
        return (rc);
}


/*##**********************************************************************\
 *
 *              schema_catalog_setreplica_nomutex
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *      sc -
 *
 *      catalog -
 *
 *
 *      isreplica -
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
static bool schema_catalog_setreplica_nomutex(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_schema_t *sc,
        char* catalog,
        bool isreplica)
{
        su_rbt_node_t* n;
        tb_schemainfo_t sibuf;
        tb_schemainfo_t* si;
        bool rc = FALSE;

        SS_PUSHNAME("schema_catalog_setreplica_nomutex");
        ss_dprintf_3(("schema_catalog_setreplica_nomutex:cd=%ld, catalog=%s, mode=%d\n",
            (long)cd, schemainfo_checknullstr(catalog), isreplica));

        ss_debug(sibuf.si_chk = TBCHK_SCHEMAINFO;)
        sibuf.si_schema = schemainfo_checknullstr(NULL);
        sibuf.si_catalog = schemainfo_checknullstr(catalog);

        n = su_rbt_search(sc->sc_rbt, &sibuf);

        if (n != NULL) {
            si = su_rbtnode_getkey(n);

            CHK_SI(si);

            si->si_isreplica = isreplica;

            rc = TRUE;
        } else {
            rc = FALSE;
        }

        SS_POPNAME;
        return (rc);
}


/*##**********************************************************************\
 *
 *              tb_schema_catalog_setmaster
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      catalog -
 *
 *
 *      ismaster -
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
bool tb_schema_catalog_setmaster(
        rs_sysi_t* cd,
        char* catalog,
        bool ismaster)
{
        tb_schema_t *sc;
        bool rc = TRUE;

        SS_PUSHNAME("tb_schema_catalog_setmaster");
        ss_dprintf_3(("tb_schema_catalog_setmaster:cd=%ld, catalog=%s, mode=%d\n",
            (long)cd, schemainfo_checknullstr(catalog), ismaster));

        sc = tb_getschema(rs_sysi_tbcon(cd));

        if (sc != NULL) {
            /* sc can be NULL when migrating from old server version */
            ss_dassert(SsSemThreadIsNotEntered(sc->sc_rbt_mutex));
            SsSemEnter(sc->sc_rbt_mutex);
            rc = schema_catalog_setmaster_nomutex(cd, sc, catalog, ismaster);
            SsSemExit(sc->sc_rbt_mutex);
        }
        SS_POPNAME;
        return (rc);
}


/*##**********************************************************************\
 *
 *              tb_schema_catalog_setreplica
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      catalog -
 *
 *
 *      isreplica -
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
bool tb_schema_catalog_setreplica(
        rs_sysi_t* cd,
        char* catalog,
        bool isreplica)
{
        tb_schema_t *sc;
        bool rc = TRUE;

        SS_PUSHNAME("tb_schema_catalog_setreplica");
        ss_dprintf_3(("tb_schema_catalog_setreplica:cd=%ld, catalog=%s, mode=%d\n",
            (long)cd, schemainfo_checknullstr(catalog), isreplica));

        sc = tb_getschema(rs_sysi_tbcon(cd));

        if (sc != NULL) {
            /* sc can be NULL when migrating from old server version */
            ss_dassert(SsSemThreadIsNotEntered(sc->sc_rbt_mutex));
            SsSemEnter(sc->sc_rbt_mutex);
            rc = schema_catalog_setreplica_nomutex(cd, sc, catalog, isreplica);
            SsSemExit(sc->sc_rbt_mutex);
        }
        SS_POPNAME;
        return (rc);
}



bool tb_schema_catalog_setmode(
        rs_sysi_t* cd,
        char* catalog,
        bool mode)
{
        tb_schema_t *sc;
        su_rbt_node_t* n;
        tb_schemainfo_t sibuf;
        tb_schemainfo_t* si;
        bool rc = FALSE;

        SS_PUSHNAME("tb_schema_catalog_setmode");
        ss_dprintf_3(("tb_schema_catalog_setmode:cd=%ld, catalog=%s, mode=%d\n",
            (long)cd, schemainfo_checknullstr(catalog), mode));

        sc = tb_getschema(rs_sysi_tbcon(cd));

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_rbt_mutex));

        SsSemEnter(sc->sc_rbt_mutex);

        ss_debug(sibuf.si_chk = TBCHK_SCHEMAINFO;)
        sibuf.si_schema = schemainfo_checknullstr(NULL);
        sibuf.si_catalog = schemainfo_checknullstr(catalog);

        n = su_rbt_search(sc->sc_rbt, &sibuf);

        if (n != NULL) {
            si = su_rbtnode_getkey(n);

            CHK_SI(si);

            if (mode) {
                ss_dassert(si->si_modeowner == NULL);
                si->si_modeowner = cd;
            } else {
                ss_dassert(si->si_modeowner != NULL);
                si->si_modeowner = NULL;
            }

            rc = TRUE;
        } else {
            rc = FALSE;
        }

        SsSemExit(sc->sc_rbt_mutex);
        SS_POPNAME;
        return (rc);
}

void tb_schema_catalog_clearmode(
        rs_sysi_t* cd)
{
        tb_schema_t *sc;
        su_rbt_node_t* n;
        tb_schemainfo_t* si;

        SS_PUSHNAME("tb_schema_catalog_clearmode");
        ss_dprintf_3(("tb_schema_catalog_clearmode:cd=%ld\n", (long)cd));

        sc = tb_getschema(rs_sysi_tbcon(cd));

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_rbt_mutex));

        SsSemEnter(sc->sc_rbt_mutex);

        n = su_rbt_min(sc->sc_rbt, NULL);
        while (n != NULL) {
            si = su_rbtnode_getkey(n);
            if (si->si_modeowner == cd) {
                si->si_modeowner = NULL;
            }
            n = su_rbt_succ(sc->sc_rbt, n);
        }

        SsSemExit(sc->sc_rbt_mutex);
        SS_POPNAME;
}

bool tb_schema_reload(rs_sysi_t* cd, tb_schema_t* sc)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        long uid;
        long id;
        char* name;
        char* catalog;
        char* owner;
        su_profile_timer;

        ss_dprintf_1(("tb_schema_reload\n"));
        CHK_SC(sc);

        su_profile_start;

        SsSemEnter(sc->sc_rbt_mutex);

        su_rbt_deleteall(sc->sc_rbt);

        SsSemExit(sc->sc_rbt_mutex);

        tcon = TliConnectInit(cd);
        TliConnectSetAppinfo(tcon, (char *)"tb_schema_reload");

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SCHEMAS);
        if (tcur == NULL) {
            char *errstr;
            int errcode;
            TliErrorInfo(tcon, &errstr, (uint *)&errcode);
            ss_dprintf_1(("tb_schema_reload:TliCursorCreate (rc=%d) %s:skipping schema reload\n",
              errcode, errstr));
            TliConnectDone(tcon);
            su_profile_stop("tb_schema_reload");
            return(FALSE);
        }

        /* Add default system schema.
         */
        schema_insert_nomutex(
            sc,
            cd,
            (char *)RS_AVAL_SYSNAME,
            RS_AVAL_DEFCATALOG,
            (char *)RS_AVAL_SYSNAME,
            -1,
            1,  /* SYNC_MODES */
            FALSE,
            TRUE);

        /* Add default system schema.
         */
        schema_insert_nomutex(
            sc,
            cd,
            NULL,
            RS_AVAL_DEFCATALOG,
            NULL,
            -2,
            1, /* SYNC_MODES */
            FALSE,
            TRUE);

        trc = TliCursorColLong(tcur, (char *)RS_ANAME_SCHEMAS_ID, &id);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_NAME, &name);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_OWNER, &owner);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_CATALOG, &catalog);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            bool succp;
            uid = tb_priv_getuid(tcon, owner);
            ss_dassert(uid != -1L);
            succp = schema_insert_nomutex(sc, cd, name, catalog, owner, uid, id, FALSE, TRUE);
            ss_dassert(succp);
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_CATALOGS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, (char *)RS_ANAME_CATALOGS_ID, &id);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_CATALOGS_NAME, &catalog);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            bool ismaster __attribute__ ((unused));
            bool isreplica __attribute__ ((unused));
            bool succp;

            succp = schema_insert_nomutex(sc, cd, NULL, catalog, NULL, -2L, id, FALSE, TRUE);

#ifndef SS_MYSQL
            tb_sync_getcatalogsyncinfo(cd, catalog, &ismaster, &isreplica);
            succp = schema_catalog_setmaster_nomutex(cd, sc, catalog, ismaster);
            ss_dassert(succp);
            succp = schema_catalog_setreplica_nomutex(cd, sc, catalog, isreplica);
            ss_dassert(succp);
#endif /* !SS_MYSQL */

        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);

        TliConnectDone(tcon);

        su_profile_stop("tb_schema_reload");

        return(TRUE);
}

tb_schema_t *tb_schema_globalinit(rs_sysi_t* cd)
{
        tb_schema_t *sc;
        bool succp;

        ss_dprintf_1(("tb_schema_globalinit\n"));

        sc = SSMEM_NEW(tb_schema_t);
        ss_debug(sc->sc_chk = TBCHK_SCHEMA;)
        sc->sc_rbt = su_rbt_init(schemainfo_rbt_cmp, schemainfo_rbt_delete);
        sc->sc_mutex = SsSemCreateLocal(SS_SEMNUM_TAB_SCHEMA);
        sc->sc_rbt_mutex = SsSemCreateLocal(SS_SEMNUM_TAB_SCHEMA_RBT);

        CHK_SC(sc);

        succp = tb_schema_reload(cd, sc);

        if (succp) {
            CHK_SC(sc);
            return(sc);
        } else {
            tb_schema_globaldone(sc);
            return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              tb_schema_create_ex
 *
 * Member of the SQL function block.
 * Creates a data base schema
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle of the transaction to which this operation
 *          belongs to (NULL if transactions are not in use)
 *
 *      schema - in
 *          name for the schema
 *
 *      catalog - in
 *          catalog name for the schema (NULL if none)
 *
 *      authid - in, use
 *              authorization id for the schema
 *
 *      usercreate - in
 *              TRUE if this is called from CREATE USER
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *      FALSE, if failed
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_schema_create_ex(
        void*       cd,
        tb_trans_t* trans,
        char*       schema,
        char*       catalog,
        char*       authid __attribute__ ((unused)),
        bool        usercreate,
        long        uid,
        rs_err_t**  p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        long id;
        dt_date_t date;
        dbe_db_t* db;
        rs_auth_t* auth;
        char* authname;
        bool rc = TRUE;
        tb_schema_t *sc;

        ss_dprintf_1(("tb_schema_create_ex\n"));

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);

        auth = rs_sysi_auth(cd);

        if (usercreate) {
            catalog = RS_AVAL_DEFCATALOG;
        } else {
            catalog = tb_catalog_resolve(cd, catalog);
        }

        if (strlen(schema) > RS_MAX_NAMELEN) {
            rs_error_create(p_errh, E_TOOLONGNAME_S, schema);
            return (FALSE);
        }
        if (RS_SDEFS_ISSYSSCHEMA(schema)) {
            rs_error_create(p_errh, E_NOPRIV);
            return (FALSE);
        }

        if (!tb_catalog_find(cd, trans, catalog, NULL)) {
            rs_error_create(p_errh, E_CATNOTEXIST_S, catalog);
            return (FALSE);
        }

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));

        catalog = tb_catalog_resolve(cd, catalog);

        SsSemEnter(sc->sc_mutex);

        /*
         * Uncommitted schemainfos created by anybody should make this
         * fail
         *
         * mikko / 2001-01-15
         *
         */

        if (schema_find_nomutex(
                sc,
                cd,
                schema,
                catalog,
                DBE_TRXID_NULL,
                NULL,
                NULL,
                NULL)) {

            rs_error_create(p_errh, E_SCHEMAEXIST_S, schema);
            rc = FALSE;
            goto return_code;
        }

        db = rs_sysi_db(cd);
        id = dbe_db_getnewuserid_log(db);

        tcon = TliConnectInitByTrans(cd, trans);
        TliConnectSetAppinfo(tcon, (char *)"tb_schema_create_ex");

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SCHEMAS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, (char *)RS_ANAME_SCHEMAS_ID, &id);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_NAME, &schema);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_OWNER, &authname);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColDate(tcur, (char *)RS_ANAME_SCHEMAS_CREATIME, &date);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_CATALOG, &catalog);
        ss_dassert(trc == TLI_RC_SUCC);

        date = tb_dd_curdate();
        if (usercreate) {
            authname = schema;
        } else {
            authname = rs_auth_username(cd, auth);
        }
        ss_dassert(authname != NULL);

        trc = TliCursorInsert(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        TliCursorFree(tcur);

        if (!usercreate) {
            uid = tb_priv_getuid(tcon, authname);
        }

        TliConnectDone(tcon);

        if (tb_trans_isfailed(cd, trans)) {
            tb_trans_geterrcode(cd, trans, p_errh);
            rc = FALSE;
            goto return_code;
        }

        ss_dassert(uid >= 0);

        if (!schema_insert_nomutex(sc, cd, schema, catalog, authname, uid, id, TRUE, FALSE)) {
            rs_error_create(p_errh, E_SCHEMAEXIST_S, schema);
            rc = FALSE;
        } else {
            tb_trans_setddop(cd, trans);
        }

return_code:;

        SsSemExit(sc->sc_mutex);

        ss_dprintf_1(("tb_schema_create_ex:rc=%d\n", rc));

        return(rc);
}

typedef enum {
        TB_NULL,
        TB_SYNCHISTTABLE,
        TB_BASETABLE,
        TB_VIEW,
        TB_PROCEDURE,
        TB_EVENT,
        TB_SEQUENCE
} tb_coltype_t;

typedef struct {
        const char*   tabname;
        const char*   colname_schema;
        const char*   colname_catalog;
        const char*   colname_table;
        tb_coltype_t type;
} tb_cascadecols_t;

static tb_cascadecols_t cascadedropitems[] = {

    { RS_RELNAME_TABLES,
      RS_ANAME_TABLES_SCHEMA,
      RS_ANAME_TABLES_CATALOG,
      RS_ANAME_TABLES_NAME,
      TB_BASETABLE
    },

    { RS_RELNAME_TABLES,
      RS_ANAME_TABLES_SCHEMA,
      RS_ANAME_TABLES_CATALOG,
      RS_ANAME_TABLES_NAME,
      TB_VIEW
    },

#ifndef SS_MYSQL
    { RS_RELNAME_PROCEDURES,
      RS_ANAME_PROCEDURES_SCHEMA,
      RS_ANAME_PROCEDURES_CATALOG,
      RS_ANAME_PROCEDURES_NAME,
      TB_PROCEDURE
    },

    { RS_RELNAME_EVENTS,
      RS_ANAME_EVENTS_SCHEMA,
      RS_ANAME_EVENTS_CATALOG,
      RS_ANAME_EVENTS_NAME,
      TB_EVENT
    },
#endif /* !SS_MYSQL */

    { RS_RELNAME_SEQUENCES,
      RS_ANAME_SEQUENCES_SCHEMA,
      RS_ANAME_SEQUENCES_CATALOG,
      RS_ANAME_SEQUENCES_NAME,
      TB_SEQUENCE
    },

    { NULL,
      NULL,
      NULL,
      NULL,
      TB_NULL
    }
};

bool tb_schema_dropreferencekeys(
        void*            cd,
        tb_trans_t*      trans,
        char*            schema,
        char*            catalog,
        rs_err_t**       p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        bool succ = TRUE;
        char *tablename;
        char *schemaname;

        SS_PUSHNAME("tb_schema_dropreferencekeys");
        tcon = TliConnectInitByTrans(cd, trans);
        TliConnectSetAppinfo(tcon, (char *)"tb_schema_dropreferencekeys");

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_TABLES);
        ss_assert(tcur != NULL);

        trc = TliCursorConstrStr(
            tcur,
            (char *)RS_ANAME_TABLES_CATALOG,
            TLI_RELOP_EQUAL,
            catalog);
        ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        if (schema != NULL){
            trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_TABLES_SCHEMA,
                TLI_RELOP_EQUAL,
                schema);
            ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

            schemaname = schema;
        } else {
            trc = TliCursorColStr(tcur, (char *)RS_ANAME_TABLES_SCHEMA, &schemaname);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
        }

        trc = TliCursorColStr(tcur, (char *)RS_ANAME_TABLES_NAME, &tablename);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        trc = TliCursorOpen(tcur);
        ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

        while (TliCursorNext(tcur) == TLI_RC_SUCC) {
            tb_relh_t *tbrelh;
            rs_relh_t *rsrelh;

            tbrelh = tb_relh_create(
                    cd,
                    trans,
                    tablename,
                    schema,
                    catalog,
                    p_errh);

            if (!tbrelh) {
                rs_error_free(cd, *p_errh);
                *p_errh = NULL;
                continue;
            }

            rsrelh = tb_relh_rsrelh(cd, tbrelh );
            succ = tb_dd_droprefkeys_ext(tcon, rsrelh, p_errh);
            tb_relh_free(cd, tbrelh);
            if (!succ) {
                break;
            }
        }

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        SS_POPNAME;
        return(succ);
}

/*##**********************************************************************\
 *
 *              tb_schema_drop_ex
 *
 * Member of the SQL function block.
 * Drops a database schema
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle of the transaction to which this operation
 *          belongs to (NULL if transactions are not in use)
 *
 *      schema - in, use
 *              schema name
 *
 *      catalog - in
 *          catalog name for the schema
 *
 *      cascade - in
 *          1 = cascade, 0 = restrict
 *
 *      userdrop - in
 *              TRUE if this is called from DROP USER
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *      FALSE, if failed
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_schema_drop_int(
        void*            cd,
        tb_trans_t*      trans,
        char*            schema,
        char*            catalog,
        bool             cascade,
        tb_schema_drop_t droptype,
        rs_err_t**       p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        bool succp = TRUE;
        rs_auth_t* auth;
        long uid;
        tb_schema_t *sc;

        ss_dprintf_1(("tb_schema_drop_int\n"));

        ss_dassert( droptype == TB_SCHEMA_DROP_USER ||
                    droptype == TB_SCHEMA_DROP_SCHEMA ||
                    droptype == TB_SCHEMA_DROP_CATALOG );

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);

        auth = rs_sysi_auth(cd);

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));
        SsSemEnter(sc->sc_mutex);

        /*
         * If the schema is not yet committed, we should not be able to
         * drop it unless we created it
         *
         * mikko / 2001-01-15
         *
         */
        if (!schema_find_nomutex(sc,
                                 cd,
                                 schema,
                                 catalog,
                                 schema_gettrxid(cd),
                                 NULL,
                                 &uid,
                                 NULL)) {

            rs_error_create(p_errh, E_SCHEMANOTEXIST_S, schema);
            succp = FALSE;
            goto return_code;
        }

        if (uid != rs_auth_userid(cd, auth) && !rs_auth_isadmin(cd, auth)) {
            rs_error_create(p_errh, E_NOPRIV);
            succp = FALSE;
            goto return_code;
        }

        if (strcmp(RS_AVAL_SYSNAME, schema) == 0){
            rs_error_create(p_errh, E_NOPRIV);
            succp = FALSE;
            goto return_code;
        }

        if ((strcmp(catalog, rs_auth_catalog(cd,auth)) == 0) &&
            (strcmp(schema, rs_auth_schema(cd, auth)) == 0)) {
            rs_error_create(p_errh, E_NOCURSCHEDROP);
            succp = FALSE;
            goto return_code;
        }

        tcon = TliConnectInitByTrans(cd, trans);
        TliConnectSetAppinfo(tcon, (char *)"tb_schema_drop_ex");

#ifndef SS_MYSQL /* Following is always true in MySQL/solidDB */
        if (droptype == TB_SCHEMA_DROP_SCHEMA
            && strcmp(catalog, RS_AVAL_DEFCATALOG) == 0
            && tb_priv_isuser(tcon, schema, &uid)) {

            rs_error_create(p_errh, E_SCHEMAISUSER_S, schema);
            TliConnectDone(tcon);
            succp = FALSE;
            goto return_code;
        }
#endif
        
        if (cascade) {
            int i;
            char *name = NULL;
            char *table_type __attribute__ ((unused));
            void *dummy;

            table_type = NULL;
            
            succp = tb_schema_dropreferencekeys(
                    cd,
                    trans,
                    schema,
                    catalog,
                    p_errh);

            for (i = 0; succp && cascadedropitems[i].tabname != NULL; i++) {
                /* for each schema find rows from tables and delete
                 * - tables
                 * - procedures
                 * - events
                 * - sequences
                 */

                tcur = TliCursorCreate(tcon,
                                       RS_AVAL_DEFCATALOG,
                                       RS_AVAL_SYSNAME,
                                       (char *)cascadedropitems[i].tabname);
                ss_assert(tcur != NULL);

                trc = TliCursorConstrStr(
                    tcur,
                    (char *)cascadedropitems[i].colname_catalog,
                    TLI_RELOP_EQUAL_OR_ISNULL,
                    catalog);
                ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                if (schema != NULL) {
                    trc = TliCursorConstrStr(
                        tcur,
                        (char *)cascadedropitems[i].colname_schema,
                        TLI_RELOP_EQUAL,
                        schema);
                    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
                }

                if (cascadedropitems[i].type == TB_BASETABLE){
                    trc = TliCursorConstrStr(
                        tcur,
                        (char *)RS_ANAME_TABLES_TYPE,
                        TLI_RELOP_EQUAL,
                        (char *)RS_AVAL_BASE_TABLE);
                    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
                }

                if (cascadedropitems[i].type == TB_VIEW){
                    trc = TliCursorConstrStr(
                        tcur,
                        (char *)RS_ANAME_TABLES_TYPE,
                        TLI_RELOP_EQUAL,
                        (char *)RS_AVAL_VIEW);
                    ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
                }

                if (cascadedropitems[i].colname_table != NULL){
                    trc = TliCursorColStr(tcur, (char *)cascadedropitems[i].colname_table, &name);
                    ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
                }

                trc = TliCursorOpen(tcur);
                ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                while (TliCursorNext(tcur) == TLI_RC_SUCC) {
                    switch( cascadedropitems[i].type ){
                        case TB_BASETABLE :
                            succp = tb_admi_droprelation(
                                cd,
                                trans,
                                name,
                                schema,
                                catalog,
                                NULL,
                                TRUE,
                                TRUE,
                                p_errh);
                            break;
                        case TB_VIEW:
                            succp = tb_dropview(
                                cd,
                                trans,
                                name,
                                schema,
                                catalog,
                                NULL,
                                cascade,
                                &dummy,
                                p_errh );
                            break;
                        case TB_PROCEDURE :
                            succp = tb_proc_drop(
                                cd,
                                trans,
                                name,
                                schema,
                                catalog,
                                p_errh );
                            break;
                        case TB_EVENT :
                            succp = tb_event_drop(
                                cd,
                                trans,
                                name,
                                schema,
                                catalog,
                                p_errh );
                            break;
                        case TB_SEQUENCE :
                            succp = tb_dropseq(
                                cd,
                                trans,
                                name,
                                schema,
                                catalog,
                                NULL,
                                cascade,
                                &dummy,
                                p_errh );
                            break;
                        default:
                            ss_derror;
                            break;
                    }
                }
                name = NULL;
                TliCursorFree(tcur);
            }
        } else {
            if (droptype != TB_SCHEMA_DROP_CATALOG
            && !schema_allowdrop_nomutex(sc,
                                         cd,
                                         trans,
                                         droptype,
                                         schema,
                                         catalog,
                                         p_errh)) {
                TliConnectDone(tcon);
                succp = FALSE;
                goto return_code;
            }
        }

        if (!succp){
            TliConnectDone(tcon);
            succp = FALSE;
            goto return_code;
        }

        if (!schema_remove_nomutex(sc, cd, schema, catalog, TRUE)) {
            succp = FALSE;
            rs_error_create(p_errh, E_SCHEMANOTEXIST_S, schema);
            /* "fallthrough" */
        } else {
            tb_trans_setddop(cd, trans);
        }

        TliConnectDone(tcon);

 return_code:
        ss_dprintf_1(("tb_schema_drop_int:succ=%d\n", succp));
        SsSemExit(sc->sc_mutex);
        return succp;
}

bool tb_schema_drop_ex(
        void*            cd,
        tb_trans_t*      trans,
        char*            schema,
        char*            catalog,
        bool             cascade,
        tb_schema_drop_t droptype,
        rs_err_t**       p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        long id;
        bool succp = TRUE;

        SS_PUSHNAME("tb_schema_drop_ex");

        ss_dprintf_1(("tb_schema_drop_ex\n", succp));

        if (droptype == TB_SCHEMA_DROP_USER) {
            catalog = RS_AVAL_DEFCATALOG;
        } else {
            catalog = tb_catalog_resolve(cd, catalog);
        }

        succp = tb_schema_drop_int(
            cd, trans, schema, catalog, cascade, droptype, p_errh
        );

        if (!succp) {
            goto return_code;
        }

        tcon = TliConnectInitByTrans(cd, trans);
        TliConnectSetAppinfo(tcon, (char *)"tb_schema_drop_ex");

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SCHEMAS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColLong(tcur, (char *)RS_ANAME_SCHEMAS_ID, &id);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SCHEMAS_NAME,
                TLI_RELOP_EQUAL,
                schema);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SCHEMAS_CATALOG,
                TLI_RELOP_EQUAL_OR_ISNULL,
                catalog);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);

        if (trc != TLI_RC_SUCC && trc != TLI_RC_END) {
            TliCursorCopySuErr(tcur, p_errh);
            TliCursorFree(tcur);
            TliConnectDone(tcon);
            succp = FALSE;
            goto return_code;
        }

        if (trc == TLI_RC_SUCC) {
            trc = TliCursorDelete(tcur);
            ss_dassert(trc == TLI_RC_SUCC);
            succp = (trc == TLI_RC_SUCC);
        } else {
            /* Not found */
            ss_dassert(trc == TLI_RC_END);
            succp = FALSE;
            rs_error_create(p_errh, E_SCHEMANOTEXIST_S, schema);
        }

        TliCursorFree(tcur);
        TliConnectDone(tcon);

return_code:;

        ss_dprintf_1(("tb_schema_drop_ex:rc=%d\n", succp));

        SS_POPNAME;

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_schema_dropcatalog
 *
 * Drops all schema objects from a catalog. When a user creates a table to
 * a catalog, an implicit schema is created for that user. These are dropped
 * when a catalog is dropped.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      catalog -
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
void tb_schema_dropcatalog(
        void*       cd,
        tb_trans_t* trans,
        char*       catalog)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char* name;
        tb_schema_t *sc;

        SS_PUSHNAME("tb_schema_dropcatalog");
        ss_dprintf_1(("tb_schema_dropcatalog\n"));

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);
        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));

        SsSemEnter(sc->sc_mutex);

        tcon = TliConnectInitByTrans(cd, trans);
        TliConnectSetAppinfo(tcon, (char *)"tb_schema_dropcatalog");

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SCHEMAS);
        ss_dassert(tcur != NULL);

        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_NAME, &name);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SCHEMAS_CATALOG,
                TLI_RELOP_EQUAL_OR_ISNULL,
                catalog);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            schema_remove_nomutex(sc, cd, name, catalog, TRUE);
            trc = TliCursorDelete(tcur);
            ss_dassert(trc == TLI_RC_SUCC);
        }
        ss_dassert(trc == TLI_RC_END);

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        SsSemExit(sc->sc_mutex);

        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *              tb_schema_maptouser
 *
 * Maps schema name to user id and user name.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trans - in, use
 *
 *
 *      schema - in
 *
 *
 *      catalog - in
 *
 *
 *      p_userid - out
 *              If non-NULL, user id is stored into *p_userid.
 *
 *      p_username - out, give
 *              If non-NULL, user name is stored into *p_username. Returned
 *          user name is allocated using SsMemAlloc.
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
bool tb_schema_maptouser(
        void*       cd,
        tb_trans_t* trans __attribute__ ((unused)),
        char*       schema,
        char*       catalog,
        long*       p_userid,
        char**      p_username)
{
        bool rc;
        tb_schema_t *sc;
        rs_auth_t* auth;

        auth = rs_sysi_auth(cd);

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);

        catalog = tb_catalog_resolve(cd, catalog);

        ss_dprintf_1(("tb_schema_maptouser:%s.%s\n", catalog != NULL ? catalog : "NULL", schema));

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));

        /*
         * no need to acquire sc->sc_mutex because we only update the rbt
         *
         */

        rc = schema_find_nomutex(
                sc,
                cd,
                schema,
                catalog,
                schema_gettrxid(cd),
                p_username,
                p_userid,
                NULL);

        return (rc);
}


/*##**********************************************************************\
 *
 *              tb_schema_create
 *
 * Member of the SQL function block.
 * Creates a data base schema
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle of the transaction to which this operation
 *          belongs to (NULL if transactions are not in use)
 *
 *      schema - in
 *          name for the schema
 *
 *      catalog - in
 *          catalog name for the schema (NULL if none)
 *
 *      authid - in, use
 *              authorization id for the schema
 *
 *      cont - in/out, give
 *          *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *      FALSE, if failed
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_schema_create(
        void*       cd,
        tb_trans_t* trans,
        char*       schema,
        char*       catalog,
        char*       authid,
        void**      cont,
        rs_err_t**  p_errh)
{
        *cont = NULL;
        return(tb_schema_create_ex(
                cd,
                trans,
                schema,
                catalog,
                authid,
                FALSE,
                0,
                p_errh));
}

/*##**********************************************************************\
 *
 *              tb_schema_drop
 *
 * Member of the SQL function block.
 * Drops a database schema
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle of the transaction to which this operation
 *          belongs to (NULL if transactions are not in use)
 *
 *      schema - in, use
 *              schema name
 *
 *      catalog - in
 *          catalog name for the schema
 *
 *      cascade - in
 *          1 = cascade, 0 = restrict
 *
 *      cont - in/out, give
 *          *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *      FALSE, if failed
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_schema_drop(
        void*       cd,
        tb_trans_t* trans,
        char*       schema,
        char*       catalog,
        bool        cascade,
        void**      cont,
        rs_err_t**  p_errh)
{
        *cont = NULL;
        SS_PUSHNAME("tb_schema_drop");
        SS_POPNAME;
        return(tb_schema_drop_ex(
                    cd,
                    trans,
                    schema,
                    catalog,
                    cascade,
                    TB_SCHEMA_DROP_SCHEMA,
                    p_errh));
}

bool tb_schema_find(
        void*       cd,
        tb_trans_t* trans __attribute__ ((unused)),
        char*       schema,
        char*       catalog)
{
        bool succp;
        tb_schema_t *sc;

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);

        catalog = rs_auth_catalog(cd, rs_sysi_auth(cd));

        catalog = tb_catalog_resolve(cd, catalog);


        succp = schema_find_nomutex(
                    sc,
                    cd,
                    schema,
                    catalog,
                    schema_gettrxid(cd),
                    NULL,
                    NULL,
                    NULL);

        return(succp);
}

typedef struct {
        const char*   tabname;
        const char*   colname_schema;
        const char*   colname_catalog;
        const char*   errname;
} tb_checkcols_t;

static tb_checkcols_t schemacheks[] = {
    { RS_RELNAME_TABLES,     RS_ANAME_TABLES_SCHEMA,    RS_ANAME_TABLES_CATALOG,         "tables" },
    { RS_RELNAME_KEYS,       RS_ANAME_KEYS_SCHEMA,      RS_ANAME_KEYS_CATALOG,           "keys"},
    { RS_RELNAME_PROCEDURES, RS_ANAME_PROCEDURES_SCHEMA,RS_ANAME_PROCEDURES_CATALOG,     "procedures"},
    { RS_RELNAME_TRIGGERS,   RS_ANAME_TRIGGERS_SCHEMA,  RS_ANAME_TRIGGERS_CATALOG,       "triggers"},
    { RS_RELNAME_EVENTS,     RS_ANAME_EVENTS_SCHEMA,    RS_ANAME_EVENTS_CATALOG,         "events"},
    { RS_RELNAME_FORKEYS,    RS_ANAME_FORKEYS_SCHEMA,   RS_ANAME_FORKEYS_CATALOG,        "foreign keys"},
    { RS_RELNAME_SEQUENCES,  RS_ANAME_SEQUENCES_SCHEMA, RS_ANAME_SEQUENCES_CATALOG,      "sequences"},
    { RS_RELNAME_PUBL,       RS_ANAME_PUBL_CREATOR,     RS_ANAME_PUBL_CATALOG,           "publications"},
    { RS_RELNAME_PUBL_STMTS, RS_ANAME_PUBL_STMT_MASTERSCHEMA,RS_ANAME_PUBL_STMT_MASTERCATALOG,"publication tables"},
    { NULL,                  NULL,                      NULL,                             NULL }
};

static tb_checkcols_t catalogcheks[] = {
    { RS_RELNAME_TABLES,         NULL, RS_ANAME_TABLES_CATALOG,            "tables" },
    { RS_RELNAME_KEYS,           NULL, RS_ANAME_KEYS_CATALOG,              "keys" },
    { RS_RELNAME_PROCEDURES,     NULL, RS_ANAME_PROCEDURES_CATALOG,        "procedures" },
    { RS_RELNAME_TRIGGERS,       NULL, RS_ANAME_TRIGGERS_CATALOG,          "triggers" },
    { RS_RELNAME_EVENTS,         NULL, RS_ANAME_EVENTS_CATALOG,            "events" },
    { RS_RELNAME_FORKEYS,        NULL, RS_ANAME_FORKEYS_CATALOG,           "foreign keys" },
    { RS_RELNAME_SEQUENCES,      NULL, RS_ANAME_SEQUENCES_CATALOG,         "sequences" },
    { RS_RELNAME_PUBL,           NULL, RS_ANAME_PUBL_CATALOG,              "publications" },
    { RS_RELNAME_PUBL_STMTS,     NULL, RS_ANAME_PUBL_STMT_MASTERCATALOG,   "publication tables" },
    { RS_RELNAME_SYNC_MASTERS,   NULL, RS_ANAME_SYNC_MASTER_REPLICACATALOG,"masters" },
    { RS_RELNAME_SYNC_REPLICAS,  NULL, RS_ANAME_SYNC_REPLICA_MASTERCATALOG,"replicas" },
    { RS_RELNAME_SYNC_BOOKMARKS, NULL, RS_ANAME_SYNC_BOOKMARKS_CATALOG,    "bookmarks" },
/*     { RS_RELNAME_SYNCINFO,    NULL, RS_ANAME_SYNCINFO_NODECATALOG,      "sync nodes" }, */
    { NULL,                      NULL, NULL,                                NULL }
};

static bool schema_findrows_nomutex(
        tb_schema_t*    sc __attribute__ ((unused)),
        void*           cd,
        tb_trans_t*     trans,
        tb_checkcols_t* checkcols,
        char*           colvalue_schema,
        char*           colvalue_catalog,
        su_ret_t        err_rc,
        su_err_t**      p_errh)
{
        int i;
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        bool foundrows = FALSE;

        CHK_SC(sc);

        ss_dassert(SsSemThreadIsEntered(sc->sc_mutex));

        ss_dprintf_4(("schema_findrows:%s.%s\n", colvalue_catalog, colvalue_schema != NULL ? colvalue_schema : "NULL"));

        tcon = TliConnectInitByTrans(cd, trans);
        TliConnectSetAppinfo(tcon, (char *)"schema_findrows_nomutex");

        for (i = 0; checkcols[i].tabname != NULL; i++) {

            ss_dprintf_4(("schema_findrows:%s\n", checkcols[i].tabname));

            tcur = TliCursorCreate(tcon,
                                   RS_AVAL_DEFCATALOG,
                                   (char *)RS_AVAL_SYSNAME,
                                   (char *)checkcols[i].tabname);
            ss_assert(tcur != NULL);

            if (colvalue_schema != NULL) {
                trc = TliCursorConstrStr(
                        tcur,
                        checkcols[i].colname_schema,
                        TLI_RELOP_EQUAL,
                        (char *)colvalue_schema);
                ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
            }
            trc = TliCursorConstrStr(
                    tcur,
                    checkcols[i].colname_catalog,
                    TLI_RELOP_EQUAL_OR_ISNULL,
                    (char *)colvalue_catalog);
            ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

            trc = TliCursorOpen(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

            trc = TliCursorNext(tcur);
            ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END, TliErrorCode(tcon));

            TliCursorFree(tcur);

            if (trc == TLI_RC_SUCC) {
                ss_dprintf_4(("    schema_findrows:found a row\n"));
                foundrows = TRUE;
                su_err_init(p_errh, err_rc, checkcols[i].errname);
                break;
            }
        }

        TliConnectDone(tcon);

        return(foundrows);
}


static bool schema_allowdrop_nomutex(
        tb_schema_t*     sc,
        void*            cd,
        tb_trans_t*      trans,
        tb_schema_drop_t droptype,
        char*            schema,
        char*            catalog,
        su_err_t**       p_errh)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char* found_schema;
        char* found_owner;
        char* found_catalog;
        bool allowdrop = TRUE;
        bool foundrows;
        su_ret_t errcode = 0;

        CHK_SC(sc);

        ss_dassert(SsSemThreadIsEntered(sc->sc_mutex));

        switch (droptype) {
            case TB_SCHEMA_DROP_USER:
                errcode = E_USEROBJECTS_S;
                break;
            case TB_SCHEMA_DROP_SCHEMA:
                errcode = E_SCHEMAOBJECTS_S;
                break;
            case TB_SCHEMA_DROP_CATALOG:
                errcode = E_CATALOGOBJECTS_S;
                break;
            default:
                ss_error;
        }

        /* First check rows from current catalog. Are there any database
         * object left.
         */
        foundrows = schema_findrows_nomutex(
                        sc,
                        cd,
                        trans,
                        droptype != TB_SCHEMA_DROP_CATALOG
                            ? schemacheks
                            : catalogcheks,
                        schema,
                        catalog,
                        errcode,
                        p_errh);

        if (foundrows) {
            return(FALSE);
        }

        /* Next check schema infos.
         */
        tcon = TliConnectInitByTrans(cd, trans);
        TliConnectSetAppinfo(tcon, (char *)"schema_allowdrop_nomutex");

        switch (droptype) {
            case TB_SCHEMA_DROP_USER:
                /* Check that user does not have any schemas left in any
                 * catalog.
                 */
                tcur = TliCursorCreate(tcon,
                                    RS_AVAL_DEFCATALOG,
                                    (char *)RS_AVAL_SYSNAME,
                                    (char *)RS_RELNAME_SCHEMAS);
                ss_assert(tcur != NULL);

                trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_NAME, &found_schema);
                ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
                trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_CATALOG, &found_catalog);
                ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                trc = TliCursorConstrStr(
                        tcur,
                        (char *)RS_ANAME_SCHEMAS_OWNER,
                        TLI_RELOP_EQUAL,
                        schema);
                ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                trc = TliCursorOpen(tcur);
                ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                    if (strcmp(found_catalog, RS_AVAL_DEFCATALOG) == 0
                        && strcmp(found_schema, schema) == 0) {
                        /* Schema for the current user, skip this schema.
                         */
                        continue;
                    }
                    ss_dprintf_4(("    schema_allowdrop_nomutex:found a row, found_schema=%s\n", found_schema));
                    allowdrop = FALSE;
                    su_err_init(p_errh, E_USEROBJECTS_S, "schemas");
                    break;
                }
                ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END, TliErrorCode(tcon));
                TliCursorFree(tcur);
                break;
            case TB_SCHEMA_DROP_SCHEMA:
                /* For real schemas, not further scheks are needed from
                 * schema table because schemas can not create other
                 * schemas.
                 */
                break;
            case TB_SCHEMA_DROP_CATALOG:
                /* Check that catalog does not have any schema objects
                 * defined.
                 */
                tcur = TliCursorCreate(tcon,
                                    RS_AVAL_DEFCATALOG,
                                    (char *)RS_AVAL_SYSNAME,
                                    (char *)RS_RELNAME_SCHEMAS);
                ss_assert(tcur != NULL);

                trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_OWNER, &found_owner);
                ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));
                trc = TliCursorColStr(tcur, (char *)RS_ANAME_SCHEMAS_NAME, &found_schema);
                ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                trc = TliCursorConstrStr(
                        tcur,
                        (char *)RS_ANAME_SCHEMAS_CATALOG,
                        TLI_RELOP_EQUAL_OR_ISNULL,
                        catalog);
                ss_rc_assert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                trc = TliCursorOpen(tcur);
                ss_rc_dassert(trc == TLI_RC_SUCC, TliErrorCode(tcon));

                while ((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
                    /* Check that there are only implicit schemas from
                     * actual users (they have same schema and owner.
                     */
                    if (strcmp(found_owner, found_schema) != 0) {
                        allowdrop = FALSE;
                        su_err_init(p_errh, E_CATALOGOBJECTS_S, "schemas");
                        break;
                    }
                }
                ss_rc_dassert(trc == TLI_RC_SUCC || trc == TLI_RC_END, TliErrorCode(tcon));

                TliCursorFree(tcur);
                break;
            default:
                ss_error;
        }

        TliConnectDone(tcon);

        return(allowdrop);
}

/*##**********************************************************************\
 *
 *              tb_schema_allowuserdrop
 *
 * Checks if user can be dropped.
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
bool tb_schema_allowuserdrop(
        void*       cd,
        tb_trans_t* trans,
        char*       username,
        rs_err_t**  p_errh)
{
        tb_schema_t *sc;
        bool succp = TRUE;

        ss_dassert(username != NULL);

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);
        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));

        SsSemEnter(sc->sc_mutex);

        succp = schema_allowdrop_nomutex(sc,
                                      cd,
                                      trans,
                                      TB_SCHEMA_DROP_USER,
                                      username,
                                      NULL,
                                      p_errh);

        SsSemExit(sc->sc_mutex);

        return (succp);
}

/*##**********************************************************************\
 *
 *              tb_schema_allowcatalogdrop
 *
 * Checks if catalog can be dropped.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
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
bool tb_schema_allowcatalogdrop(
        void*       cd,
        tb_trans_t* trans,
        char*       catalog,
        rs_err_t**  p_errh)
{
        tb_schema_t *sc;
        bool succp = TRUE;

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);
        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));

        catalog = tb_catalog_resolve(cd, catalog);

        SsSemEnter(sc->sc_mutex);

        succp = schema_allowdrop_nomutex(
                                 sc,
                                 cd,
                                 trans,
                                 TB_SCHEMA_DROP_CATALOG,
                                 NULL,
                                 catalog,
                                 p_errh);

        SsSemExit(sc->sc_mutex);

        return (succp);
}


bool tb_schema_isvalidsetschemaname(
        void* cd,
        tb_trans_t* trans __attribute__ ((unused)),
        char* username)
{
        bool succp = TRUE;
        char* catalog;
        tb_schema_t *sc;
        rs_auth_t* auth;

        ss_dassert(username != NULL);

        auth = rs_sysi_auth(cd);

        sc = tb_getschema(rs_sysi_tbcon(cd));

        CHK_SC(sc);

        if (strcmp(RS_AVAL_SYSNAME, username) == 0) {
            return(TRUE);
        }
        if (username == NULL || username[0] == '\0') {
            return(FALSE);
        }

        catalog = rs_auth_catalog(cd, rs_sysi_auth(cd));

        catalog = tb_catalog_resolve(cd, catalog);

        ss_dassert(SsSemThreadIsNotEntered(sc->sc_mutex));

        /*
         * no need to acquire sc->sc_mutex because we only update the rbt
         *
         */

        succp = schema_find_nomutex(
                    sc,
                    cd,
                    username,
                    catalog,
                    schema_gettrxid(cd),
                    NULL,
                    NULL,
                    NULL);

        return(succp);
}

