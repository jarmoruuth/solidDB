/*************************************************************************\
**  source       * tab0admi.c
**  directory    * tab
**  description  * Administration functions
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

- REVOKE is always done in RESTRICT mode, there is no support for CASCADE.
Note that even if CASCADE is specified, it is ignored. This is needed
because it seems that the CASCADE is the default, at least in this interface,
if either CASCADE or RESTRICT is specified.

- TUPLE_ID attribute is not added to a table, if primary key is defined.
Maybe it should be added?


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
#include <ssmem.h>
#include <ssstring.h>
#include <sssprint.h>
#include <ssdebug.h>

#include <su0sdefs.h>
#include <su0vers.h>
#include <su0pars.h>
#include <su0error.h>
#include <su0li3.h>

#include <ui0msg.h>

#include <rs0types.h>
#include <rs0ttype.h>
#include <rs0key.h>
#include <rs0relh.h>
#include <rs0viewh.h>
#include <rs0sysi.h>
#include <rs0auth.h>
#include <rs0sdefs.h>
#include <rs0entna.h>

#include <dbe0type.h>
#include <dbe0rel.h>
#include <dbe0erro.h>
#include <dbe0db.h>

#include "tab1defs.h"
#include "tab1priv.h"
#include "tab1dd.h"
#ifndef SS_MYSQL
#include "tab1atab.h"
#endif
#include "tab0cata.h"
#include "tab0tli.h"
#include "tab0conn.h"
#include "tab0tran.h"
#include "tab0view.h"
#include "tab0relh.h"
#include "tab0info.h"
#include "tab0proc.h"
#include "tab0evnt.h"
#include "tab0seq.h"
#include "tab0sche.h"
#include "tab0admi.h"
#ifndef SS_MYSQL
#include "tab0sync.h"
#include "tab1set.h"
#include "tab0sql.h"
#endif

#ifndef SS_NODDUPDATE

FAKE_CODE(static long tabletogglecount = 0;)

static void tb_keyinsertref(
        void*     cd,
        rs_key_t* sec_key,
        rs_key_t* clust_key,
        rs_ano_t* p_nparts
);

typedef struct {
        rs_relh_t *relh;
        rs_relh_t *ref_relh;
        rs_key_t *ref_key;
        rs_key_t *foreign_key;
        rs_ttype_t* ttype;

        TliConnectT *tcon;
        rs_key_t* primkref;
        rs_key_t* forkref;
        tb_relh_t* tbrelh;

        TliCursorT* tcur;
        dbe_trxid_t stmtid;
} tab_addforkey_context_t;

static su_ret_t tb_checkrefkey (
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        tab_addforkey_context_t *context
);

static bool tb_checkindex (
        void*        cd,
        rs_relh_t*   rel,
        rs_key_t*    unique_key
);

/*#***********************************************************************\
 *
 *              tb_issysindexname
 *
 *
 *
 * Parameters :
 *
 *      indexname -
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
static bool tb_issysindexname(rs_sysi_t* cd, char* indexname)
{
        return(indexname[0] == RS_SYSKEY_PREFIXCHAR &&
               rs_sysi_getconnecttype(cd) != RS_SYSI_CONNECT_SYSTEM);
}

#ifdef REFERENTIAL_INTEGRITY

/*#***********************************************************************\
 *
 *              tb_matchrefkey
 *
 * Try to match the foreign key references to the given primary or
 * unique key.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      ref_key -
 *
 *
 *      ref_ttype -
 *
 *
 *      fk -
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
static bool tb_matchrefkey(
        void* cd,
        rs_key_t* ref_key,
        rs_ttype_t* ref_ttype,
        tb_sqlforkey_t* fk)
{
        int nparts;
        int ref_kpno;
        uint j;
        rs_attrtype_t kptype;

        ss_dassert(rs_key_isunique(cd, ref_key));

        /* Find first user defined attribute on referenced table reference
         * key.
         */
        nparts = rs_key_nparts(cd, ref_key);
        for (ref_kpno = 0; ref_kpno < nparts; ref_kpno++) {
            if (!rs_keyp_isconstvalue(cd, ref_key, ref_kpno) &&
                rs_keyp_parttype(cd, ref_key, ref_kpno) != RSAT_KEY_ID) {
                /* Not constant key part and not key id. Key id is checked
                 * separately here, because in self reference case it is
                 * not yet set as a constant key part.
                 */
                break;
            }
        }
        ss_dassert(ref_kpno < nparts);
        kptype = rs_keyp_parttype(cd, ref_key, ref_kpno);
        if (kptype != RSAT_USER_DEFINED
#ifdef SS_COLLATION
            &&  kptype != RSAT_COLLATION_KEY
#endif /* SS_COLLATION */
        ) {
            /* The first non-const attribute in reference key is not user
             * defined, the key cannot be used for referencing.
             */
            ss_derror;
            return(FALSE);
        }

        for (j = 0; j < fk->len; j++, ref_kpno++) {
            rs_ano_t ref_ano;
            rs_ano_t ref_physano;
            char* ref_aname;
            rs_atype_t* atype;

            ref_aname = fk->reffields[j];
            ref_ano = rs_ttype_sql_anobyname(cd, ref_ttype, ref_aname);
            ref_physano = rs_ttype_sqlanotophys(cd, ref_ttype, ref_ano);
            if (rs_keyp_ano(cd, ref_key, ref_kpno) != ref_physano) {
                /* Foreign key does not match to the reference key prefix
                 * of referenced table.
                 */
                return(FALSE);
            }
            atype = rs_ttype_atype(cd, ref_ttype, ref_physano);

#if !defined(SS_MYSQL) && !defined(SS_MYSQL_AC)
            if (rs_atype_nullallowed(cd, atype)) {
                /* Referenced columns must be NOT NULL.
                 */
                return(FALSE);
            }
#endif /* !SS_MYSQL && !SS_MYSQL_AC */
        }

        if (ref_kpno != (rs_ano_t)(rs_key_lastordering(cd, ref_key) + 1)) {
            /* Foreign key and the referenced key dos not have the
             * same number of columns.
             */
            return(FALSE);
        }
        return(TRUE);
}

/*#***********************************************************************\
 *
 *              tb_initforkey
 *
 * Initializes foreign key. Does checks that foreign key definition
 * is valid and creates indexes for the foreign key.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trans - in
 *
 *
 *      relname - in
 *
 *
 *      authid - in
 *
 *
 *      relh - use
 *
 *
 *      ttype - in
 *
 *
 *      fk - in
 *
 *
 *  keyname - in, use
 *
 *
 *      ref_relh_p - out
 *
 *
 *      refkey_p - out
 *
 *
 *      forkey_p - out
 *
 *
 *      p_errh - out, give
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
static bool tb_initforkey(
    void*        cd,
    tb_trans_t*  trans,
    char*        relname,
    char*        authid,
    char*        catalog,
    rs_relh_t*   relh,
    rs_ttype_t*  ttype,
    tb_sqlforkey_t* fk,
    char*        keyname,
    rs_relh_t**  ref_relh_p,
    rs_key_t**   ref_key_p,
    rs_key_t**   foreign_key_p,
    rs_err_t**   p_errh)
{
        uint j;
        tb_relh_t* ref_tbrelh = NULL;
        rs_relh_t* ref_relh;
        rs_ttype_t* ref_ttype;
        rs_key_t* cluster_key;
        rs_key_t* ref_key;
        rs_key_t* foreign_key;
        rs_ano_t phys_kpindex;
        rs_ano_t tupleversion_ano;
        bool succp;
        bool self_ref;
        rs_auth_t* auth;
        uint fklen;

        SS_PUSHNAME("tb_initforkey()");

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        if (fk->unresolved) {
            return TRUE;
        }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

        auth = rs_sysi_auth(cd);

        self_ref = FALSE;

        /* Check that referenced table exists.
         */
        if (strcmp(fk->reftable, relname) == 0) {
            /* Check if reference to itself.
             */
            bool same_authid;
            char* refschema;

            if (RS_ENTNAME_ISSCHEMA(fk->refschema)) {
                refschema = fk->refschema;
            } else {
                refschema = rs_auth_schema(cd, auth);
            }

            if (authid == NULL) {
                rs_auth_t *auth = rs_sysi_auth(cd);
                authid = rs_auth_username(cd, auth);
            }

            same_authid = strcmp(authid, refschema) == 0;
            if (same_authid) {
                char* refcatalog;
                if (fk->refcatalog != NULL) {
                    refcatalog = fk->refcatalog;
                } else {
                    refcatalog = rs_auth_catalog(cd, auth);
                }
                if (strcmp(catalog, refcatalog) == 0) {
                    self_ref = TRUE;
                    ref_relh = relh;
                    rs_relh_link(cd, ref_relh);
                    SS_MEM_SETLINK(ref_relh);
                    ref_ttype = rs_relh_ttype(cd, ref_relh);
                }
            } else {
                self_ref = FALSE;
            }
        }
        if (!self_ref) {
            /* Reference to another table.
             */
            ref_tbrelh = tb_relh_create(
                            cd,
                            trans,
                            fk->reftable,
                            fk->refschema,
                            fk->refcatalog,
                            p_errh);

            if (ref_tbrelh == NULL) {
                SS_POPNAME;
                return(FALSE);
            }

            ref_relh = tb_relh_rsrelh(cd, ref_tbrelh);
#ifndef SS_MME
            if (rs_relh_reltype(cd, ref_relh) == RS_RELTYPE_MAINMEMORY &&
                su_pa_nelems(rs_relh_refkeys(cd, ref_relh)) != 0) {
                /* Foreign keys are not supported with main memory
                 * tables.
                 */
                tb_relh_free(cd, ref_tbrelh);
                rs_error_create(p_errh, E_MMINOFORKEY);
                SS_POPNAME;
                return(FALSE);
            }
#else
            /* XXX - check that foreign keys are to other mainmem
                tables only. */
#endif
            if (!rs_relh_isbasetable(cd, relh)) {
                tb_relh_free(cd, ref_tbrelh);
                rs_error_create(p_errh, E_NOTBASETABLE);
                SS_POPNAME;
                return(FALSE);
            }
            rs_relh_link(cd, ref_relh);
            SS_MEM_SETLINK(ref_relh);
            ref_ttype = rs_relh_ttype(cd, ref_relh);
        }

        ref_key = rs_relh_primkey(cd, ref_relh);

        if (ref_key == NULL && fk->reffields == NULL) {
            if (!self_ref) {
                tb_relh_free(cd, ref_tbrelh);
            }
            SS_MEM_SETUNLINK(ref_relh);
            rs_relh_done(cd, ref_relh);
            rs_error_create(p_errh, E_PRIMKEY_NOTDEF_S, fk->reftable);
            SS_POPNAME;
            return(FALSE);
        }

        if (fk->reffields == NULL) {
            uint nparts = rs_key_nparts(cd, ref_key);
            uint ref_kpno;
            uint pno;
            fk->reffields = SsMemAlloc (sizeof(char*) * fk->len);
            for (ref_kpno = 0, pno = 0;
                 pno<fk->len && ref_kpno < nparts; ref_kpno++)
            {
                int ano = rs_keyp_ano(cd, ref_key, ref_kpno);
                if (ano == -1) {
                    continue;
                }
                fk->reffields[pno] = SsMemStrdup(
                                    rs_ttype_sql_aname(cd, ref_ttype, ano)
                );
                pno++;
            }
            ss_dassert(fk->len==pno);
        }

        /* Check that columns exist and data types are comparable.
         */
        for (j = 0; j < fk->len; j++) {
            rs_ano_t ref_ano;
            char* ref_aname;
            rs_atype_t* ref_atype;
            rs_atype_t* atype;
            rs_datatype_t ref_dt;
            rs_datatype_t dt;

            ref_aname = fk->reffields[j];
            ref_ano = rs_ttype_sql_anobyname(cd, ref_ttype, ref_aname);

            if (ref_ano == RS_ANO_NULL) {
                /* Referenced field not found.
                 */
                SS_MEM_SETUNLINK(ref_relh);
                rs_relh_done(cd, ref_relh);
                if (!self_ref) {
                    tb_relh_free(cd, ref_tbrelh);
                    ref_tbrelh = 0;
                }
                rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, ref_aname, fk->reftable);
                ss_dassert(ref_tbrelh == 0);
                SS_POPNAME;
                return(FALSE);
            }

            ref_atype = rs_ttype_sql_atype(cd, ref_ttype, ref_ano);
            ss_dassert((int)fk->fields[j] >= 0);
            atype = rs_ttype_sql_atype(cd, ttype, fk->fields[j]);
            ref_dt = rs_atype_datatype(cd, ref_atype);
            ss_dassert(atype != 0);
            dt = rs_atype_datatype(cd, atype);

            if (ref_dt != dt) {
                /* Data types do not match.
                 */
                SS_MEM_SETUNLINK(ref_relh);
                rs_relh_done(cd, ref_relh);
                if (!self_ref) {
                    tb_relh_free(cd, ref_tbrelh);
                    ref_tbrelh = 0;
                }
                rs_error_create(p_errh, E_FORKINCOMPATDTYPE_S, rs_ttype_sql_aname(cd, ttype, fk->fields[j]));
                ss_dassert(ref_tbrelh == 0);
                SS_POPNAME;
                return(FALSE);
            }
#ifdef SS_MYSQL

            /* Referential action SET NULL is not possible if referencing columns are not NOT NULL */
            if (fk->updrefact == SQL_REFACT_SETNULL || fk->delrefact == SQL_REFACT_SETNULL) {
                if (!(rs_atype_nullallowed(cd, atype))) {
                    rs_error_create(p_errh, E_NULLNOTALLOWED_S,
                                    rs_ttype_sql_aname(cd, ttype, fk->fields[j]));


                    SS_MEM_SETUNLINK(ref_relh);
                    rs_relh_done(cd, ref_relh);

                    if (!self_ref) {
                        tb_relh_free(cd, ref_tbrelh);
                        ref_tbrelh = 0;
                    }

                    ss_dassert(ref_tbrelh == 0);
                    SS_POPNAME;
                    return (FALSE);
                }
            }
#endif
        }

        if (!self_ref) {
            /* Check access rights to the referenced table.
             */
            if (!tb_relh_ispriv(cd, ref_tbrelh, TB_PRIV_REFERENCES)) {
                /* No global priviles to the table. Check if there are
                 * reference privileges to all referenced columns.
                 */
                tb_relpriv_t* relpriv;

                succp = TRUE;
                relpriv = tb_relh_priv(cd, ref_tbrelh);
                if (!tb_priv_issomeattrpriv(cd, relpriv)) {
                    /* No column privileges specified. */
                    succp = FALSE;
                }
                for (j = 0; j < fk->len && succp; j++) {
                    rs_ano_t ref_ano;
                    rs_ano_t ref_physano;
                    char* ref_aname;

                    ref_aname = fk->reffields[j];
                    ref_ano = rs_ttype_sql_anobyname(
                                cd,
                                ref_ttype,
                                ref_aname);
                    ref_physano = rs_ttype_sqlanotophys(
                                    cd,
                                    ref_ttype,
                                    ref_ano);

                    succp = tb_priv_isattrpriv(
                                cd,
                                relpriv,
                                ref_physano,
                                TB_PRIV_REFERENCES);
                }
                if (!succp) {
                    SS_MEM_SETUNLINK(ref_relh);
                    rs_relh_done(cd, ref_relh);
                    tb_relh_free(cd, ref_tbrelh);
                    rs_error_create(p_errh, E_NOREFERENCESPRIV_S, fk->reftable);
                    SS_POPNAME;
                    return(FALSE);
                }
            }
            tb_relh_free(cd, ref_tbrelh);
            ref_tbrelh=0;
        }

        ss_dassert(ref_tbrelh == 0);

        /* Try to find a matching primary or unique key from the
         * referenced table. Try first the primary key.
         */

        succp = FALSE;
        if (ref_key != NULL) {
            succp = tb_matchrefkey(cd, ref_key, ref_ttype, fk);
        }
        if (!succp) {
            su_pa_t* keys;
            keys = rs_relh_keys(cd, ref_relh);
            su_pa_do_get(keys, j, ref_key) {
                if (rs_key_isunique(cd, ref_key)) {
                    succp = tb_matchrefkey(cd, ref_key, ref_ttype, fk);
                    if (succp) {
                        break;
                    }
                }
            }
        }

        if (!succp) {
            rs_error_create(p_errh, E_FORKNOTUNQK);
            SS_MEM_SETUNLINK(ref_relh);
            rs_relh_done(cd, ref_relh);
            SS_POPNAME;
            return(FALSE);
        }

        fklen = fk->len;

        /* Create the foreign key to this table.
         */

        foreign_key = rs_key_init(
                        cd,
                        keyname,
                        0L,                     /* key_id (undefined) */
                        FALSE,                  /* unique */
                        FALSE,                  /* clustering */
                        FALSE,                  /* primary */
                        FALSE,                  /* prejoined */
                        fklen + 1,            /* nordering */
                        auth);
        phys_kpindex = 0;

        /* Add KEY_ID system attribute */
        rs_key_addpart(
            cd,
            foreign_key,
            phys_kpindex++,
            RSAT_KEY_ID,
            TRUE,
            RS_ANO_NULL,
            NULL);

        /* Add all foreign key attributes */
        for (j = 0; j < fk->len; j++) {
            rs_attrtype_t kptype = RSAT_USER_DEFINED;
            rs_ano_t phys_ano;
            void* collation = NULL;
            phys_ano = rs_ttype_sqlanotophys(cd, ttype, fk->fields[j]);
#ifdef SS_COLLATION
            {
                rs_atype_t* atype;

                /* TODO!! actually collation should be taken
                   from primary key key part!!!!
                */
                atype = rs_ttype_atype(cd, ttype, phys_ano);
                collation = rs_atype_collation(cd, atype);
                if (collation != NULL) {
                    kptype = RSAT_COLLATION_KEY;
                }
            }
#endif /* SS_COLLATION */

            rs_key_addpart(
                cd,
                foreign_key,
                phys_kpindex,
                kptype,
                TRUE,
                phys_ano,
                collation);

            phys_kpindex++;
        }
        ss_dassert((int)fk->updrefact>=0 && (int)fk->updrefact<256);
        ss_dassert((int)fk->delrefact>=0 && (int)fk->delrefact<256);

        cluster_key = rs_relh_clusterkey(cd, relh);
        tb_keyinsertref(cd, foreign_key, cluster_key, &phys_kpindex);

        tupleversion_ano = rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_TUPLE_VERSION);
        ss_dassert(tupleversion_ano != RS_ANO_NULL);

#ifndef DBE_UPDATE_OPTIMIZATION
        /* Add TUPLE_VERSION system attribute. */
        rs_key_addpart(
            cd,
            foreign_key,
            phys_kpindex++,
            RSAT_TUPLE_VERSION,
            TRUE,
            tupleversion_ano,
            NULL);
#endif

        succp = rs_relh_insertkey(cd, relh, foreign_key);
        ss_dassert(succp);
        *ref_relh_p = ref_relh;
        *ref_key_p = ref_key;
        *foreign_key_p = foreign_key;
        ss_dassert(ref_tbrelh == 0);
        SS_POPNAME;
        return TRUE;
}

static bool tb_is_updating_action(int upd, int del)
{
        return ((upd != SQL_REFACT_NOACTION && upd != SQL_REFACT_RESTRICT) ||
                (del != SQL_REFACT_NOACTION && del != SQL_REFACT_RESTRICT &&
                 del != SQL_REFACT_CASCADE));
}

/*#***********************************************************************\
 *
 *              tb_initforkeys
 *
 * Initializes foreign keys. Does checks that foreign key definition
 * is valid and creates indexes for the foreign key.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trans - in
 *
 *
 *      relname - in
 *
 *
 *      authid - in
 *
 *
 *      relh - use
 *
 *
 *      ttype - in
 *
 *
 *      forkey_c - in
 *
 *
 *      forkeys - in
 *
 *
 *      reftable_pa - use
 *
 *
 *      refkey_pa - use
 *
 *
 *      forkey_pa - use
 *
 *
 *      p_errh - out, give
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
static bool tb_initforkeys(
    void*        cd,
    tb_trans_t*  trans,
    char*        relname,
    char*        authid,
    char*        catalog,
    rs_relh_t*   relh,
    rs_ttype_t*  ttype,
    uint         forkey_c,
    tb_sqlforkey_t* forkeys,
    su_pa_t*     reftable_pa,
    su_pa_t*     refkey_pa,
    su_pa_t*     forkey_pa,
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
    ulong        relid,
#endif
    rs_err_t**   p_errh)
{
        uint i;

        for (i = 0; i < forkey_c; i++) {
            tb_sqlforkey_t* fk;
            char*           keyname;
            rs_relh_t*      ref_relh = NULL;
            rs_key_t*       ref_key = NULL;
            rs_key_t*       foreign_key = NULL;
            bool res;
            
            fk      = &forkeys[i];
            keyname = fk->name;
            
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
            if (fk->unresolved) {
                continue;
            }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

#if defined(SS_MYSQL_AC)
            fk->mysqlname = NULL;
#endif

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
            if (fk->mysqlname != NULL && keyname == NULL) {
                keyname = (char *)SsMemAlloc(strlen(fk->mysqlname) + 64);

                SsSprintf(keyname, "%s_%lu", fk->mysqlname, relid);
            } else {
                char *fmt = (char *)RSK_FORKEYSTR "_%lu";
                keyname = SsMemAlloc(strlen(relname) + strlen(fmt) + 64);

                SsSprintf(keyname, fmt, relname, i, relid);
            }
#endif

            if (keyname == NULL) {
                char* fmt = (char *)RSK_FORKEYSTR;
                keyname = SsMemAlloc(strlen(relname) +
                                     strlen(fmt) +
                                     (10 - 2) + /* max print width of %u  - strlen("%u") */
                                     1);
                SsSprintf(keyname, fmt, relname, i);
            }

            res = tb_initforkey(
                    cd, trans, relname, authid, catalog, relh, ttype,
                    fk, keyname, &ref_relh, &ref_key, &foreign_key, p_errh
            );
            if (keyname != fk->name) {
                SsMemFree(keyname);
            }

            if (!res) {
                return FALSE;
            }

            /* For MySQL/Soliddb: It should be removed to avoid an error where many 
               FKs are created to the same table
            */
#if !defined(SS_MYSQL) && !defined(SS_MYSQL_AC)
            if (tb_is_updating_action(fk->updrefact, fk->delrefact)) {
                uint j;

                for (j = 0; j < i; j++) {
                    tb_sqlforkey_t* fkp = &forkeys[j];
                    if (su_pa_getdata(reftable_pa, j) == ref_relh &&
                        tb_is_updating_action(fkp->updrefact, fkp->delrefact))
                    {
                        SS_MEM_SETUNLINK(ref_relh);
                        rs_relh_done(cd, ref_relh);
                        rs_error_create(p_errh, E_FORKEY_LOOPDEP);
                        return FALSE;
                    }
                }
            }
#endif /* !SS_MYSQL && !SS_MYSQL_AC */

            ss_dassert(ref_relh != NULL && ref_key != NULL && foreign_key != NULL);
            su_pa_insertat(reftable_pa, i, ref_relh);
            su_pa_insertat(refkey_pa, i, ref_key);
            su_pa_insertat(forkey_pa, i, foreign_key);
        }

        return(TRUE);
}

#ifdef SS_MME
/*#***********************************************************************\
 *
 *      tb_check_mme_integrity
 *
 * Checks if it is possible to create foreign key refering from one
 * table to another.
 *
 * Parameters :
 *
 *  cd - in
 *
 *
 *  relh - in
 *      referencing table
 *
 *
 *  ref_relh - in
 *      referenced table
 *
 *
 *  storetype - in
 *
 *
 *  persistencytype - in
 *
 *
 *  p_errh - out, give
 *
 *
 * Return value :
 *      TRUE if foreign key does not break MME restrictions.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool tb_check_mme_integrity (
        void*        cd,
        rs_relh_t*   relh,
        rs_relh_t*   ref_relh,
        tb_dd_store_t       storetype,
        tb_dd_persistency_t persistencytype,
        rs_err_t**   p_errh)
{
        bool ismainmem = FALSE;

        /* Temporary foreign key error processing.
         * Currently there cannot be a foreign key reference from a M-table
         * table to a D-table or vice versa.
         *
         * This additional checking is done to user tables only (not system
         * tables, since system tables are always D-tables and thus need not
         * be checked. However, if user creates a foreign key reference to a
         * system table, foreign key reference check is being made.
         */
        if ( (rs_relh_issysrel(cd, relh) == FALSE) && (ref_relh != relh) )
        {

            bool sux = TRUE;
            ss_dprintf_3(("check_mme_integrity: rs_relh_issysrel(cd, ref_relh) == FALSE\n"));

            /* It is being assumed, that at this point of relation creation
             * the reltype of the relh is not set (even if the default relation
             * type is RS_RELTYPE_MAINMEMORY, the reltype of the relh handle
             * should be RS_RELTYPE_OPTIMISTIC which is the default value in
             * rs_relh_init.)
             */
            /* ss_dassert( rs_relh_reltype(cd, relh) != RS_RELTYPE_MAINMEMORY ); */


            switch (storetype){
                case TB_DD_STORE_DEFAULT:
                    ss_dprintf_3(("check_mme_integrity: storetype == TB_DD_STORE_DEFAULT\n"));
                    if (dbe_db_getdefaultstoreismemory(rs_sysi_db(cd))){
                        ss_dprintf_3(("check_mme_integrity: defaultstore == MEMORY\n"));
                        ismainmem = TRUE;
                        if (rs_relh_reltype(cd, ref_relh) != RS_RELTYPE_MAINMEMORY){
                            sux = FALSE;
                            ss_dprintf_3(("check_mme_integrity: reltype != RS_RELTYPE_MAINMEMORY -> error\n"));
                        }
                    } else {
                        ss_dprintf_3(("check_mme_integrity: defaultstore != MEMORY\n"));
                        if (rs_relh_reltype(cd, ref_relh) == RS_RELTYPE_MAINMEMORY){
                            sux = FALSE;
                            ss_dprintf_3(("check_mme_integrity: reltype == RS_RELTYPE_MAINMEMORY -> error\n"));
                        }
                    }
                    break;
                case TB_DD_STORE_DISK:
                    ss_dprintf_3(("check_mme_integrity: storetype == TB_DD_STORE_DISK\n"));
                    if (rs_relh_reltype(cd, ref_relh) == RS_RELTYPE_MAINMEMORY){
                        sux = FALSE;
                        ss_dprintf_3(("check_mme_integrity: reltype == RS_RELTYPE_MAINMEMORY -> error\n"));
                    }
                    break;
                case TB_DD_STORE_MEMORY:
                    ss_dprintf_3(("check_mme_integrity: storetype == TB_DD_STORE_MEMORY\n"));
                    ismainmem = TRUE;
                    if (rs_relh_reltype(cd, ref_relh) != RS_RELTYPE_MAINMEMORY){
                        sux = FALSE;
                        ss_dprintf_3(("check_mme_integrity: reltype != RS_RELTYPE_MAINMEMORY -> error\n"));
                    }
                    break;
                default:
                    ss_derror;
                    break;
            }

#ifndef SS_MIXED_REFERENTIAL_INTEGRITY
            if (sux == FALSE){
                rs_error_create(p_errh, E_MMEILLFORKEY);
                return(FALSE);
            }
#endif /* !defined(SS_MIXED_REFERENTIAL_INTEGRITY) */

            /* Deny bad references between transient, temporary, and
               regular tables.
               THIS HAS TO BE DONE, EVEN IF REFERENCES BETWEEN M- AND
               D-TABLES ARE ALLOWED! */
            {
                tb_database_t*  tdb = rs_sysi_tabdb(cd);

                if (ismainmem
                    && tb_getdefaultistransient(tdb)) {
                    persistencytype = TB_DD_PERSISTENCY_TRANSIENT;
                }
                if (ismainmem
                    && tb_getdefaultisglobaltemporary(tdb)) {
                    persistencytype = TB_DD_PERSISTENCY_GLOBAL_TEMPORARY;
                }

                if ((rs_relh_isglobaltemporary(cd, ref_relh)
                     && (persistencytype
                         != TB_DD_PERSISTENCY_GLOBAL_TEMPORARY))
                    || ((persistencytype
                         == TB_DD_PERSISTENCY_GLOBAL_TEMPORARY)
                        && !rs_relh_isglobaltemporary(cd, ref_relh))) {
                    rs_error_create(
                            p_errh,
                            E_REFERENCETEMPNONTEMP);
                    return FALSE;
                }
                if (rs_relh_istransient(cd, ref_relh)
                    && persistencytype == TB_DD_PERSISTENCY_PERMANENT) {
                    rs_error_create(
                            p_errh,
                            E_REGULARREFERENCESTRANSIENT);
                    return FALSE;
                }
                if (rs_relh_isglobaltemporary(cd, ref_relh)) {
                    if (persistencytype
                        == TB_DD_PERSISTENCY_PERMANENT) {
                        ss_derror;
                        rs_error_create(
                                p_errh,
                                E_REGULARREFERENCESTEMPORARY);
                        return FALSE;
                    } else if (persistencytype
                               == TB_DD_PERSISTENCY_TRANSIENT) {
                        ss_derror;
                        rs_error_create(
                                p_errh,
                                E_TRANSIENTREFERENCESTEMPORARY);
                        return FALSE;
                    }
                }
            }
        } else {
            ss_dprintf_3(("check_mme_integrity: rs_relh_issysrel(cd, ref_relh) == TRUE\n"));
        }
        return TRUE;
}
#endif

/*#***********************************************************************\
 *
 *              admi_relh_inserted_and_empty
 *
 * Checks that relh is created in current transaction and it is empty.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              trans -
 *
 *
 *              relh -
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
static bool admi_relh_inserted_and_empty(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh)
{
        bool b;
        ss_int8_t ntuples;
        ss_int8_t nbytes;

        b = dbe_trx_relinserted(
                tb_trans_dbtrx(cd, trans),
                rs_relh_entname(cd, relh),
                NULL);
        if (!b) {
            ss_dprintf_3(("admi_relh_inserted_and_empty:%.80s, NOT inserted\n", rs_relh_name(cd, relh)));
            return(FALSE);
        }
        dbe_trx_getrelhcardin(
            tb_trans_dbtrx(cd, trans),
            relh,
            &ntuples,
            &nbytes);

        if (SsInt8Is0(ntuples) && SsInt8Is0(nbytes)) {
            if (tb_dd_checkifrelempty(cd, trans, rs_relh_entname(cd, relh))) {
                ss_dprintf_3(("admi_relh_inserted_and_empty:%.80s, inserted and empty\n", rs_relh_name(cd, relh)));
                return(TRUE);
            }
        }
        ss_dprintf_3(("admi_relh_inserted_and_empty:%.80s, NOT empty\n", rs_relh_name(cd, relh)));
        return(FALSE);
}

/*#***********************************************************************\
 *
 *      tb_createforkey
 *
 * Creates the reference key needed for foreign key checks.
 *
 * Parameters :
 *
 *  cd - in
 *
 *
 *  trans - use
 *
 *
 *  authid - in
 *
 *
 *  relh - use
 *
 *
 *  ttype - in
 *
 *
 *  fk - in
 *
 *
 *  ref_relh - in
 *
 *
 *  ref_key - in
 *
 *
 *  foreign_key - in
 *
 *
 *  p_errh - out, give
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
static bool tb_createforkey(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        tb_sqlforkey_t* fk,
        rs_relh_t*   check_relh,
        tab_addforkey_context_t **context,
        rs_err_t**   p_errh)
{
        su_ret_t rc;

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        if (fk->unresolved) {
            return TRUE;
        }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

        if ((*context)->tcon == NULL) {
            rs_auth_t* auth;
            uint j;
            rs_ttype_t* ref_ttype;
            rs_key_t* primkref;
            rs_key_t* forkref;
            rs_key_t* foreign_key = (*context)->foreign_key;
            rs_key_t* ref_key = (*context)->ref_key;
            rs_ano_t phys_kpindex;
            int nkeyparts;
            int nconstattrs;
            int action = ((fk->updrefact+1)<<8) | (fk->delrefact+1);
            uint fklen;

            auth = rs_sysi_auth(cd);
            ref_ttype = rs_relh_ttype(cd, (*context)->ref_relh);

            /*---------------------------------------------------------------
             * STEP 1: Create key object that is used to reference from
             * the primary key to the foreign key.
             */

            /* Count the number of constant attributes.
             */
            nkeyparts = rs_key_nparts(cd, foreign_key);
            for (nconstattrs = 0; nconstattrs < nkeyparts; nconstattrs++) {
                if (!rs_keyp_isconstvalue(cd, foreign_key, nconstattrs)) {
                    break;
                }
            }

            fklen = fk->len;

            /* Create the key object.
             */
            primkref = rs_key_init(
                        cd,
                        rs_key_name(cd, foreign_key),
                        0L,                     /* key_id (unused) */
                        FALSE,                  /* unique */
                        FALSE,                  /* clustering */
                        FALSE,                  /* primary */
                        FALSE,                  /* prejoined */
                        nconstattrs + fklen,    /* nordering */
                        auth);
            rs_key_settype(cd, primkref, RS_KEY_PRIMKEYCHK);
            phys_kpindex = 0;

            /* First add all constant key parts from the foreign key.
             */
            while (phys_kpindex < nkeyparts) {
                if (!rs_keyp_isconstvalue(cd, foreign_key, phys_kpindex)) {
                    break;
                }
                rs_key_addpart(
                        cd,
                        primkref,
                        phys_kpindex,
                        rs_keyp_parttype(cd, foreign_key, phys_kpindex),
                        TRUE,
                        RS_ANO_NULL,
                        rs_keyp_constvalue(cd, foreign_key, phys_kpindex));
                phys_kpindex++;
            }
            /* Add all attributes that are used to reference the foreign
             * key.
             */
            for (j = 0; j < fk->len; j++) {
                rs_attrtype_t kptype;
                void* collation = NULL;

                kptype = rs_keyp_parttype(cd, foreign_key, phys_kpindex);
#ifdef SS_COLLATION
                collation = rs_keyp_collation(cd, foreign_key, phys_kpindex);

                ss_dassert(
                    (collation == NULL && kptype == RSAT_USER_DEFINED) ||
                    (collation != NULL && kptype == RSAT_COLLATION_KEY));
#else /* SS_COLLATION */
                ss_dassert(kptype == RSAT_USER_DEFINED);
#endif /* SS_COLLATION */

                rs_key_addpart(
                        cd,
                        primkref,
                        phys_kpindex,
                        kptype,
                        TRUE,
                        rs_ttype_anobyname(cd, ref_ttype, fk->reffields[j]),
                        collation);
                phys_kpindex++;
            }

            rs_key_setaction(cd, primkref, action);

            /*---------------------------------------------------------------
             * STEP 2: Create key object that is used to reference from
             * the foreign key to the primary key.
             */

            /* Count the number of constant attributes.
             */
            nkeyparts = rs_key_nparts(cd, ref_key);
            for (nconstattrs = 0; nconstattrs < nkeyparts; nconstattrs++) {
                if (!rs_keyp_isconstvalue(cd, ref_key, nconstattrs)) {
                    break;
                }
            }
            /* Create the key object.
             */
            forkref = rs_key_init(
                        cd,
                        rs_key_name(cd, foreign_key),
                        0L,                     /* key_id (unused) */
                        FALSE,                  /* unique */
                        FALSE,                  /* clustering */
                        FALSE,                  /* primary */
                        FALSE,                  /* prejoined */
                        nconstattrs + fklen,    /* nordering */
                        auth);
            rs_key_settype(cd, forkref, RS_KEY_FORKEYCHK);
            phys_kpindex = 0;

            /* First add all constant key parts from the primary key.
             */
            while (phys_kpindex < nkeyparts) {
                if (!rs_keyp_isconstvalue(cd, ref_key, phys_kpindex)) {
                    break;
                }
                rs_key_addpart(
                        cd,
                        forkref,
                        phys_kpindex,
                        rs_keyp_parttype(cd, ref_key, phys_kpindex),
                        TRUE,
                        RS_ANO_NULL,
                        rs_keyp_constvalue(cd, ref_key, phys_kpindex));
                phys_kpindex++;
            }
            /* Add all attributes that are used to reference the foreign
             * key.
             */
            for (j = 0; j < fk->len; j++) {
                rs_attrtype_t kptype;
                void* collation = NULL;

                kptype = rs_keyp_parttype(cd, ref_key, phys_kpindex);
#ifdef SS_COLLATION
                collation = rs_keyp_collation(cd, ref_key, phys_kpindex);
                ss_dassert(
                    (collation == NULL && kptype == RSAT_USER_DEFINED) ||
                    (collation != NULL && kptype == RSAT_COLLATION_KEY));
#else /* SS_COLLATION */
                ss_dassert(kptype == RSAT_USER_DEFINED);
#endif /* SS_COLLATION */

                rs_key_addpart(
                        cd,
                        forkref,
                        phys_kpindex,
                        kptype,
                        TRUE,
                        rs_ttype_sqlanotophys(
                                cd,
                                (*context)->ttype,
                                fk->fields[j]),
                        collation);
                phys_kpindex++;
            }

            rs_key_setaction(cd, forkref, action);

            if ((*context)->relh==(*context)->ref_relh
             && rs_key_issamekey(cd, forkref, primkref, FALSE))
            {
                rs_key_done(cd, primkref);
                rs_key_done(cd, forkref);
                SsMemFree(*context);
                *context = NULL;
                rs_error_create(p_errh, E_FORKEY_SELFREF);
                return(FALSE);
            }

            (*context)->forkref = forkref;
            (*context)->primkref = primkref;
        }

        if (check_relh != NULL &&
            dbe_trx_check_refintegrity(tb_trans_dbtrx(cd, trans)))
        {
            if ((*context)->tcon == NULL) {
                (*context)->tcon = TliConnectInitByTrans(cd, trans);
            }

            rc = tb_checkrefkey(
                    cd,
                    trans,
                    *context);
            if ((*context)->tcur) {
                return TRUE;
            }

            TliConnectDone((*context)->tcon);
            (*context)->tcon = NULL;

            if (rc != SU_SUCCESS) {
                ss_dassert(rc == DBE_ERR_PARENTNOTEXIST_S);
                rs_error_create_key(p_errh, rc, (*context)->forkref);
                rs_key_done(cd, (*context)->primkref);
                rs_key_done(cd, (*context)->forkref);

                SsMemFree(*context);
                *context = NULL;
                return(FALSE);
            }
        }

        {
            rs_key_t*   primkref = (*context)->primkref;
            rs_key_t*   forkref = (*context)->forkref;
            rs_relh_t*  relh = (*context)->relh;
            rs_relh_t*  ref_relh = (*context)->ref_relh;
            rs_key_t*   foreign_key = (*context)->foreign_key;
            rs_key_t*   ref_key = (*context)->ref_key;
            su_pa_t*    dep_relh_pa;
            su_pa_t*    dep_new_pa;
            void*       relnop;
            uint        i;
            TliConnectT* tcon;

            SsMemFree(*context);
            *context = NULL;


            if (tb_is_updating_action(fk->updrefact, fk->delrefact)) {

            /* For MySQL/Soliddb: It should be removed to avoid an error where many 
               FKs are created to the same table
            */
#if !defined(SS_MYSQL) && !defined(SS_MYSQL_AC)
                if (rs_relh_relid(cd, relh) == rs_relh_relid(cd, ref_relh)) {
                    rs_error_create(p_errh, E_FORKEY_LOOPDEP);
                    rs_key_done(cd, primkref);
                    rs_key_done(cd, forkref);

                    return FALSE;
                }
#endif
                /* Detection of update/delete dependency loops.
                 * Maning of the list:
                 * - dep_relh_pa = all the relations where update to
                 *   relh cascades to (including relh).
                 * - dep_new_pa = all the relations from where upades
                 *   cascade to ref_relh (including ref_relh).
                 * - dep_deps_pa = all the dependancies where updates to
                 *   relpno, taken from dep_new_pa cascade to.
                 * Idea of the algorithm: make sure that there is no table
                 * for all dep_new_pa members dep_deps_pa does not intersect
                 * with dep_relh_pa. => Updates to these tables, that cascade
                 * to relh do not casecade to the same table twice. (Once via
                 * relh, and once bypassing relh.
                 */

                dep_relh_pa = su_pa_init();
                dep_new_pa = su_pa_init();
                tcon = TliConnectInitByTrans(cd, trans);

                ss_dprintf_1(("tb_createforkey: dependency checking starts: "
                              "relh=%d, ref_relh=%d\n",
                              rs_relh_relid(cd, relh),
                              rs_relh_relid(cd, ref_relh)));

                su_pa_insert(dep_relh_pa, (void*)rs_relh_relid(cd, relh));

                if (!tb_dd_find_depending_rels(
                        tcon, rs_relh_relid(cd, relh), dep_relh_pa, p_errh
                    ) ||
                    !tb_dd_find_dependent_rels(
                        tcon, rs_relh_relid(cd, ref_relh), dep_new_pa, p_errh
                    ))
                {
                    su_pa_done(dep_relh_pa);
                    su_pa_done(dep_new_pa);
                    rs_key_done(cd, primkref);
                    rs_key_done(cd, forkref);
                    TliConnectDone(tcon);

                    return FALSE;
                }

                su_pa_insert(dep_new_pa, (void*)rs_relh_relid(cd, ref_relh));

                su_pa_do_get(dep_new_pa, i, relnop) {
                    long        relno = (long) relnop;

                    if (!tb_dd_find_depending_rels(tcon, relno, dep_new_pa, p_errh)) {
                        su_pa_done(dep_relh_pa);
                        su_pa_done(dep_new_pa);
                        rs_key_done(cd, primkref);
                        rs_key_done(cd, forkref);
                        TliConnectDone(tcon);

                        return FALSE;
                    }
                }

            /* For MySQL/Soliddb: It should be removed to avoid an error where many 
               FKs are created to the same table
            */
#if !defined(SS_MYSQL) && !defined(SS_MYSQL_AC)
                su_pa_do_get (dep_relh_pa, i, relnop) {
                    /* Check for dependency loops */
                    void        *prelid1;
                    void        *prelid2;
                    uint        j;
                    uint        k;
                    long        relno = (long) relnop;
                    su_pa_t*    dep_deps_pa;

                    dep_deps_pa = su_pa_init();

                    ss_dprintf_1(("tb_createforkey: dep loop=%d\n", relno));

                    if (!tb_dd_find_dependent_rels(
                            tcon, relno, dep_deps_pa, p_errh
                    ))
                    {
                        su_pa_done(dep_deps_pa);
                        su_pa_done(dep_relh_pa);
                        su_pa_done(dep_new_pa);
                        rs_key_done(cd, primkref);
                        rs_key_done(cd, forkref);
                        TliConnectDone(tcon);

                        return FALSE;
                    }

                    if ((ulong)relno == rs_relh_relid(cd, relh)) {
                        su_pa_t* rkeys = rs_relh_refkeys(cd, relh);
                        rs_key_t* key;

                        su_pa_do_get (rkeys, j, key) {
                            int uact = rs_key_update_action(cd, key);
                            int dact = rs_key_delete_action(cd, key);

                            if (tb_is_updating_action(uact, dact)) {
                                ulong id = rs_key_refrelid(cd, key);
                                ss_dprintf_1(("tb_createforkey: addition %d\n", id));
                                su_pa_insert(dep_deps_pa, (void*)id);
                            }
                        }
                    }

                    ss_dprintf_1(("tb_createforkey: inserting %d\n", relno));

                    su_pa_insert(dep_deps_pa, (void*)relno);

                    su_pa_do_get(dep_deps_pa, j, prelid1) {
                        su_pa_do_get(dep_new_pa, k, prelid2) {
                            ss_dprintf_1(("tb_createforkey: in loop "
                                          "relno=%d prelid1=%d prelid2=%d\n",
                                          relno, prelid1, prelid2));

                            if (prelid1 == prelid2) {
                                rs_error_create(p_errh, E_FORKEY_LOOPDEP);
                                su_pa_done(dep_deps_pa);
                                su_pa_done(dep_relh_pa);
                                su_pa_done(dep_new_pa);
                                rs_key_done(cd, primkref);
                                rs_key_done(cd, forkref);
                                TliConnectDone(tcon);

                                return FALSE;
                            }
                        }
                    }

                    su_pa_done(dep_deps_pa);
                }
#endif /* !SS_MYSQL && !SS_MYSQL_AC */

                su_pa_done(dep_relh_pa);
                su_pa_done(dep_new_pa);
                TliConnectDone(tcon);
            }

            rs_key_setrefrelid(cd, forkref, rs_relh_relid(cd, ref_relh));
            rs_key_setrefrelid(cd, primkref, rs_relh_relid(cd, ref_relh));

            /*---------------------------------------------------------------
             * STEP 3: Add reference keys to the relation handle and to the
             * system relations.
             */

            if (!admi_relh_inserted_and_empty(cd, trans, ref_relh)) {
                if (!tb_trans_setrelhchanged(cd, trans, ref_relh, p_errh)) {
                    rs_key_done(cd, primkref);
                    rs_key_done(cd, forkref);

                    return(FALSE);
                }
            }
            if (!admi_relh_inserted_and_empty(cd, trans, relh)) {
                rs_relh_setddopactive(cd, relh);
            }

            ss_dassert(strcmp(rs_key_name(cd, primkref), rs_key_name(cd, forkref)) == 0);
            rs_relh_insertrefkey(cd, ref_relh, primkref);
            tb_dd_setmmerefkeyinfo(cd, primkref, relh);
            rs_relh_insertrefkey(cd, relh, forkref);
            tb_dd_setmmerefkeyinfo(cd, forkref, ref_relh);

            rc = tb_dd_createrefkey(
                    cd,
                    trans,
                    relh,
                    primkref,
                    rs_relh_ttype(cd, ref_relh),
                    rs_relh_relid(cd, ref_relh),
                    rs_key_id(cd, foreign_key),
                    rs_relh_relid(cd, relh),
                    rs_sysi_auth(cd),
                    p_errh);
            if (rc != SU_SUCCESS) {
                return(FALSE);
            }

            rc = dbe_trx_insertindex (
                    tb_trans_dbtrx(cd, trans), relh, foreign_key
            );
            if (rc != SU_SUCCESS) {
                if (rc == E_KEYNAMEEXIST_S) {
                    rs_error_create(p_errh, rc, rs_key_name(cd, foreign_key));
                } else {
                    rs_error_create(p_errh, rc);
                }
                return(FALSE);
            }

            rc = tb_dd_createrefkey(
                    cd,
                    trans,
                    relh,
                    forkref,
                    rs_relh_ttype(cd, relh),
                    rs_relh_relid(cd, relh),
                    rs_key_id(cd, ref_key),
                    rs_relh_relid(cd, relh),
                    rs_sysi_auth(cd),
                    p_errh);
            if (rc != SU_SUCCESS) {
                return(FALSE);
            }
        }
        return(TRUE);
}

/*#***********************************************************************\
 *
 *      tb_dropconstraint
 *
 *  Drops named constraint.
 *
 * Parameters :
 *
 *  cd - in
 *
 *
 *  trans - use
 *
 *
 *  relh - in
 *
 *
 *  name - in
 *
 *
 *  p_errh - out, give
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Limitations:
 *      Only named foreign keys and check constraints are supported.
 *
 * Globals used :
 *
 * See also :
 */
bool tb_dropconstraint(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        char*        schema,
        char*        catalog,
        char*        name,
        void**       cont,
        rs_err_t**   p_errh)
{
        su_ret_t rc;
        rs_key_t *key;
        rs_entname_t ename;
        char *kname;
        char *keyfmt = RSK_NEW_UNQKEYCONSTRSTR;

        rc = dbe_trx_alterrel(
                tb_trans_dbtrx(cd, trans),
                relh,
                0);
        if (rc != SU_SUCCESS) {
            rs_error_create(p_errh, rc);
            return FALSE;
        }
        if (schema==NULL) {
             schema = rs_auth_schema(cd, rs_sysi_auth(cd));
        }

        kname = SsMemAlloc(strlen(name)+strlen(keyfmt)+1);
        SsSprintf(kname, keyfmt, name);
        rs_entname_initbuf(&ename, catalog, schema, kname);
        key = rs_relh_keybyname(cd, relh, &ename);

        if (key && rs_key_isunique(cd, key) && !rs_key_isclustering(cd,key)) {
            bool ret;
            ret = tb_dropindex(
                cd,
                trans,
                kname,
                schema,
                catalog,
                NULL,
                cont,
                p_errh);

            SsMemFree(kname);
            return ret;
        }
        SsMemFree(kname);

        if (rs_relh_hasrefkey(cd, relh, name)) {
            rs_relh_t*      prelh;
            tb_relpriv_t*   priv;

            *cont = NULL;
            key = rs_relh_refkeybyname(cd, relh, name);
            if (key == NULL) {
                rs_error_create(p_errh, E_CONSTRAINT_NOT_FOUND_S, name);
                return FALSE;
            }
            prelh = tb_dd_getrelhbyid(
                    cd,
                    trans,
                    rs_key_refrelid(cd, key),
                    &priv,
                    p_errh);
            if (prelh == NULL) {
                return FALSE;
            }
            rc = dbe_trx_alterrel(
                    tb_trans_dbtrx(cd, trans),
                    prelh,
                    0);
            SS_MEM_SETUNLINK(prelh);
            rs_relh_done(cd, prelh);
            if (rc != SU_SUCCESS) {
                rs_error_create(p_errh, rc);
                return FALSE;
            }

            rc = tb_dd_droprefkey(
                    cd,
                    trans,
                    relh,
                    name,
                    rs_sysi_auth(cd),
                    p_errh);

            if (rc == SU_SUCCESS){
                ss_debug (
                bool retval = )
                rs_relh_deleterefkey(cd, relh, name);
                ss_dassert( retval == TRUE );
            }

            return rc == SU_SUCCESS;
        } else if (tb_dd_hasnamedcheck(cd, trans, relh, name)) {
            *cont = NULL;
            return tb_dd_dropnamedcheck(cd, trans, relh, name);
        } else {
            *cont = NULL;
            rs_error_create(p_errh, E_CONSTRAINT_NOT_FOUND_S, name);
            return FALSE;
        }

        ss_error;
        return TRUE;
}

/*#***********************************************************************\
 *
 *      tb_checkrefkey
 *
 *  Check if the existing table data matches given foreign key.
 *
 * Parameters :
 *
 *  cd - in
 *
 *
 *  trans - use
 *
 *
 *  tbrelh - in
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Limitations:
 *
 * Globals used :
 *
 * See also :
 */
static su_ret_t tb_checkrefkey (
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        tab_addforkey_context_t* context)
{
        dbe_trx_t *trx = tb_trans_dbtrx(cd, trans);
        su_ret_t rc = SU_SUCCESS;
        rs_tval_t* tval;
        TliRetT trc;
        TliCursorT* tcur = context->tcur;
        rs_relh_t*  relh = context->relh;
        rs_relh_t*  ref_relh = context->ref_relh;
        rs_ttype_t* ttype = context->ttype;
        rs_key_t*   foreign_key = context->foreign_key;
        rs_key_t*   fork_ref = context->forkref;
        int i;

        if (!tcur) {
            int nparts = rs_key_nparts(cd, foreign_key);
	    /* Test line added */
            ss_dassert (nparts > 0);
	    /* ----- */
            ss_dassert (context->tbrelh != NULL);

            tcur = TliCursorCreateRelh(context->tcon, context->tbrelh);
            ss_assert(tcur != NULL);  /* The table certainly exists */

            /* Do fake binding, otherwise TliCursorNext does not return
             * any tvals.
             */

            for (i=rs_key_first_datapart(cd,foreign_key);
                 i<=rs_key_lastordering(cd,foreign_key); ++i) {

                rs_ano_t ano =
                    rs_ttype_physanotosql(cd,ttype,
                                          rs_keyp_ano(cd,foreign_key,i));
                trc = TliCursorColByNo(tcur, ano);
            }

            trc = TliCursorOpen(tcur);
            ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));

            context->stmtid = dbe_trx_getstmttrxid(trx);
            context->tcur = tcur;
        }

        for (i=0; (tval=TliCursorNextTval(tcur)) != NULL; i++) {
            if (rs_relh_reltype(cd, ref_relh) == RS_RELTYPE_MAINMEMORY) {
                bool was_null = FALSE;
                rs_ano_t last_part = rs_key_lastordering(cd, fork_ref);
                rs_ano_t part;
                dynvtpl_t value = NULL;

                /* Build a value that represents the reference key. */
                dynvtpl_setvtpl(&value, VTPL_EMPTY);
                /* Dummy keyid. */
                dynvtpl_appva(&value, VA_NULL);

                for (part = rs_key_first_datapart(cd, fork_ref);
                     part <= last_part; part++)
                {
                    rs_ano_t    ano = rs_keyp_ano(cd, fork_ref, part);
                    rs_atype_t *atype = rs_ttype_atype(cd, ttype, ano);
                    rs_aval_t  *aval = rs_tval_aval(cd, ttype, tval, ano);
                    va_t*       va = rs_aval_va(cd, atype, aval);

                    if (rs_aval_isnull(cd, atype, aval)) {
                        was_null = TRUE;
                        break;
                    }
                    dynvtpl_appva(&value, va);
                }
                if (was_null) {
                    rc = SU_SUCCESS;
                } else {
                    rc = dbe_trx_mme_refkeycheck(
                        cd,
                        trx,
                        fork_ref,
                        relh,
                        context->stmtid,
                        value
                    );
                }
                dynvtpl_free(&value);
            } else {
                rc = dbe_trx_refkeycheck(
                        cd,
                        trx,
                        NULL,
                        fork_ref,
                        ttype,
                        tval);
            }
            if (rc != SU_SUCCESS) {
                break;
            }
            if (i>200) {
                return SU_SUCCESS;
            }
        }

        TliCursorFree(context->tcur);
        context->tcur = NULL;

        return rc;
}


#ifndef SS_MYSQL
/*#***********************************************************************\
 *
 *      tb_checkheckconstraint
 *
 *  Check if the existing table data matches given check constraint.
 *
 * Parameters :
 *
 *  cd - in
 *
 *
 *  trans - use
 *
 *
 *  tbrelh - in
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Limitations:
 *
 * Globals used :
 *
 * See also :
 */
static bool tb_checkcheckconstraint (
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        char*        schema,
        char*        catalog,
        char*        tabname,
        char*        constraint,
        void         **cont,
        rs_err_t**   errhandle)
{
        struct check_context {
            void*        cd;
            tb_sql_t*    sc;
            sqlsystem_t* sqls;
            char*        sqlstr;
        };
        tb_sql_ret_t rc;
        struct check_context *context = *cont;
        rs_tval_t    *tval=NULL;

        if (context == NULL) {
            sqlsystem_t* sqls = tb_sqls_init(cd);
            tb_sql_t*    sc;
            bool succp;
            static char sqlstr_fmt[] = "SELECT * FROM \"%s\".\"%s\".\"%s\" WHERE NOT (%s)";
            size_t len = sizeof(sqlstr_fmt)+strlen(tabname)+strlen(catalog)
                        +strlen(schema)+strlen(constraint)+1;
            char *sqlstr = SsMemAlloc(len);

            SsSprintf(sqlstr, sqlstr_fmt, schema, catalog, tabname, constraint);
            sc = tb_sql_init(cd, sqls, trans, sqlstr);
            ss_bassert(sc != NULL);
            succp = tb_sql_prepare(sc, errhandle);
            if (!succp) {
                /* For example if the user does not have suficient priviligies */
                SsMemFree(sqlstr);
                tb_sql_done(sc);
                tb_sqls_done(cd, sqls);
                return FALSE;
            }
            ss_dassert(succp);
            succp = tb_sql_execute(sc, errhandle);
            ss_dassert(succp);

            context = SsMemAlloc(sizeof(*context));
            context->cd = cd;
            context->sc = sc;
            context->sqls = sqls;
            context->sqlstr = sqlstr;
            *cont = context;
        }
        rc = tb_sql_fetch_cont(context->sc, TRUE, &tval, NULL);
        if (rc == TB_SQL_CONT) {
            return TRUE;
        }

        SsMemFree(context->sqlstr);
        tb_sql_done(context->sc);
        tb_sqls_done(context->cd, context->sqls);
        SsMemFree(context);
        *cont = NULL;

        if (tval != NULL) {
            rs_error_create(errhandle, E_CONSTRAINT_CHECK_FAIL);
            return FALSE;
        }
        return TRUE;
}
#endif /* !SS_MYSQL */


/*#***********************************************************************\
 *
 *      tb_addforkey
 *
 * Adds new foreign key as named constraint.
 *
 * Parameters :
 *
 *  cd - in
 *
 *
 *  trans - use
 *
 *
 *  tbrelh - in
 *
 *
 *  schema - in
 *
 *
 *  catalog - in
 *
 *
 *  tablename - in
 *
 *
 *  forkey - in
 *
 *
 *  name - in
 *
 *
 *  p_errh - out, give
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
bool tb_addforkey(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        tb_relh_t*   tbrelh,
        char*        schema,
        char*        catalog,
        char*        tablename,
        tb_sqlforkey_t* forkey,
        void**       cont,
        rs_err_t**   errhandle)
{
        tab_addforkey_context_t *context = *cont;
        bool        succp;
        rs_relh_t  *relh;
        rs_relh_t  *ref_relh = NULL;
        rs_ttype_t* ttype;

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        if (forkey->unresolved) {
            return TRUE;
        }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

        if (context == NULL) {
            rs_key_t *ref_key = NULL;
            rs_key_t *foreign_key = NULL;
            rs_auth_t *authid = rs_sysi_auth(cd);
            TliConnectT *tcon;
            su_ret_t    rc;
            char*       given_name;

            *errhandle = NULL;
            if (!tb_relh_ispriv(cd, tbrelh, TB_PRIV_CREATOR)) {
                rs_error_create(errhandle, E_NOPRIV);
                return(FALSE);
            }

            catalog = tb_catalog_resolve(cd, catalog);

            relh =  tb_relh_rsrelh(cd, tbrelh);

            if (forkey->name == NULL) {
                char* fmt = (char *)RSK_FORKEYSTR;
                int i=0;
                given_name = SsMemAlloc(strlen(tablename) +
                                     strlen(fmt) +
                                     (10 - 2) +
                                     1);
                i = 0;
                do {
                    SsSprintf(given_name, fmt, tablename, i++);
                } while (rs_relh_hasrefkey(cd, relh, given_name));
            } else if (!tb_dd_hasnamedcheck(cd, trans, relh, forkey->name)) {
                given_name = forkey->name;
            } else {
                rs_error_create(errhandle, E_CONSTRAINT_NAME_CONFLICT_S, forkey->name);
                return FALSE;
            }

            rc = dbe_trx_alterrel(
                    tb_trans_dbtrx(cd, trans),
                    relh,
                    0);
            if (rc != SU_SUCCESS) {
                if (forkey->name != given_name) {
                    SsMemFree(given_name);
                }
                rs_error_create(errhandle, rc);
                return FALSE;
            }

            ttype = tb_relh_ttype(cd, tbrelh);

            succp = tb_initforkey(
                    cd,
                    trans,
                    tablename,
                    schema,
                    catalog,
                    relh,
                    ttype,
                    forkey,
                    given_name,
                    &ref_relh,
                    &ref_key,
                    &foreign_key,
                    errhandle);

            if (forkey->name != given_name) {
                SsMemFree(given_name);
            }

            if (!succp) {
                return FALSE;
            }

#ifdef SS_MME
            {
                tb_dd_persistency_t persistencytype =
                    rs_relh_isglobaltemporary(cd, relh) ?
                        TB_DD_PERSISTENCY_GLOBAL_TEMPORARY :
                    rs_relh_istransient(cd, relh) ?
                        TB_DD_PERSISTENCY_TRANSIENT :
                        TB_DD_PERSISTENCY_PERMANENT;

                tb_dd_store_t storetype =
                    rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY ?
                        TB_DD_STORE_MEMORY: TB_DD_STORE_DISK;

                bool res = tb_check_mme_integrity (
                    cd, relh, ref_relh, storetype, persistencytype, errhandle
                );
                if (!res) {
                    succp = rs_relh_deletekey(cd, relh, foreign_key);
                    ss_dassert(succp);
                    rs_key_done(cd, foreign_key);
                    SS_MEM_SETUNLINK(ref_relh);
                    rs_relh_done(cd, ref_relh);
                    return FALSE;
                }
            }
#endif /* SS_MME */

            tcon = TliConnectInitByTrans(cd, trans);
            td_dd_createonekey(
                    tcon,
                    relh,
                    ttype,
                    foreign_key,
                    authid,
                    TB_DD_CREATEREL_USER
            );
            TliConnectDone(tcon);

            context = SsMemAlloc(sizeof(tab_addforkey_context_t));
            context->relh = relh;
            context->ref_relh = ref_relh;
            context->ref_key = ref_key;
            context->foreign_key = foreign_key;
            context->ttype = ttype;
            context->tbrelh = tbrelh;

            context->tcon = NULL;
            context->primkref = NULL;
            context->forkref = NULL;
            context->tcur = NULL;

            *cont = context;
        } else {
            relh = context->relh;
            ref_relh = context->ref_relh;
            ttype = context->ttype;
        }

        succp = tb_createforkey (
                    cd,
                    trans,
                    forkey,
                    ref_relh,
                    &context,
                    errhandle);

        if (context != NULL) {
            return TRUE;
        }
        SS_MEM_SETUNLINK(ref_relh);
        rs_relh_done(cd, ref_relh);

        *cont = NULL;
        return succp;
}

/*#***********************************************************************\
 *
 *      tb_addunique
 *
 * Adds named unique constraint.
 *
 * Parameters :
 *
 *  cd - in
 *
 *
 *  trans - use
 *
 *
 *  tbrelh - in
 *
 *
 *  schema - in
 *
 *
 *  catalog - in
 *
 *
 *  tablename - in
 *
 *
 *  forkey - in
 *
 *
 *  name - in
 *
 *
 *  p_errh - out, give
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
static bool tb_addunique(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        tb_relh_t*   tbrelh,
        char*        schema,
        char*        catalog,
        char*        tablename,
        tb_sqlunique_t* unique,
        void**       cont,
        rs_err_t**   errhandle)
{
        rs_relh_t*      relh;
        bool*           desc;
        char**          attrs;
        rs_ttype_t*     ttype;
        uint             i;
        bool            ret;
        char*           name;
        uint            uniquelen;

        *cont = NULL;
        *errhandle = NULL;

        uniquelen = unique->len;

        relh = tb_relh_rsrelh(cd, tbrelh);
        attrs = SsMemAlloc(sizeof(char*) * uniquelen);
        desc = SsMemAlloc(sizeof(bool) * uniquelen);
        ttype = tb_relh_ttype(cd, tbrelh);

        for (i=0; i<unique->len; i++) {
            attrs[i] = rs_ttype_sql_aname(cd, ttype, unique->fields[i]);
            desc[i] = FALSE;
        }

        if (unique->name) {
            char *keyfmt = (char *)RSK_NEW_UNQKEYCONSTRSTR;
            name = SsMemAlloc(strlen(unique->name)+strlen(keyfmt)+1);
            SsSprintf(name, keyfmt, unique->name);
        } else {
            char *keyfmt = (char *)RSK_NEW_UNQKEYSTR;
            rs_entname_t ename;
            int key_c = 0;
            ulong relid = rs_relh_relid (cd, relh);

            name = SsMemAlloc(strlen(tablename) +
                              strlen(keyfmt) + (RSK_RELID_DISPLAYSIZE + 1) +
                              (10 - 2) + /* max print width for %u - strlen("%u") */
                              +1);
            do {
                key_c++;
                SsSprintf(name, keyfmt, tablename, relid, key_c);
                rs_entname_initbuf(&ename, catalog, schema, name);
            } while (rs_relh_keybyname(cd, relh, &ename));
        }

        ret = tb_createindex_ext(
                cd,
                trans,
                name,
                schema,
                catalog,
                relh,
                ttype,
                TRUE,  /* unique */
                uniquelen,
                attrs,
                desc,
#ifdef SS_COLLATION
                NULL,
#endif /* SS_COLLATION */
                TB_DD_CREATEREL_USER,
                errhandle);

        SsMemFree(name);
        SsMemFree(attrs);
        SsMemFree(desc);
        return ret;
}

/*#***********************************************************************\
 *
 *      tb_addcheckconstraint
 *
 * Adds new check string as named constraint.
 *
 * Parameters :
 *
 *  cd - in
 *
 *
 *  trans - use
 *
 *
 *  tbrelh - in
 *
 *
 *  schema - in
 *
 *
 *  catalog - in
 *
 *
 *  tablename - in
 *
 *
 *  checkstr - in
 *
 *
 *  name - in
 *
 *
 *  p_errh - out, give
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
static bool tb_addcheckconstraint_relh(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        char*        schema __attribute__ ((unused)),
        char*        catalog __attribute__ ((unused)),
        char*        tablename,
        char*        checkstr,
        int          start_index,
        char*        name,
        rs_err_t**   errhandle)
{
        rs_auth_t *authid = rs_sysi_auth(cd);
        char *given_name;
        su_ret_t    rc;

        *errhandle = NULL;

        if (name) {
            given_name = name;
            if (rs_relh_hasrefkey(cd, relh, name)) {
                rs_error_create(errhandle, E_CONSTRAINT_NAME_CONFLICT_S, name);
                return FALSE;
            }
        } else {
            char* fmt = (char *)RSK_CHECKSTR;
            size_t i;
            given_name = SsMemAlloc(strlen(tablename) +
                            strlen(fmt) +
                            8+1);
            i = start_index;
            do {
                SsSprintf(given_name, fmt, tablename, i++);
            } while (tb_dd_hasnamedcheck(cd, trans, relh, given_name));
        }

        rc = tb_dd_createnamedcheck(
                    cd,
                    trans,
                    relh,
                    given_name,
                    checkstr,
                    authid,
                    errhandle);
        if (name == 0) {
            SsMemFree(given_name);
        }

        if (rc != SU_SUCCESS) {
            return FALSE;
        }

        rs_relh_addcheckstring(cd, relh, checkstr, name);
        return TRUE;
}

#ifndef SS_MYSQL
static bool tb_addcheckconstraint(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        tb_relh_t*   tbrelh,
        char*        checkstr,
        char*        name,
        void**       cont,
        rs_err_t**   errhandle)
{
        su_ret_t   rc;
        rs_relh_t *relh = tb_relh_rsrelh(cd, tbrelh);
        char*      schema = rs_relh_schema(cd, relh);
        char*      catalog = rs_relh_catalog(cd, relh);
        char*      tablename = rs_relh_name(cd, relh);

        if (!tb_relh_ispriv(cd, tbrelh, TB_PRIV_CREATOR)) {
            rs_error_create(errhandle, E_NOPRIV);
            return(FALSE);
        }

        if (!tb_checkcheckconstraint(cd, trans, catalog, schema, tablename,
                                     checkstr, cont, errhandle))
        {
            return FALSE;
        }
        if (*cont) {
            return TRUE;
        }

        rc = dbe_trx_alterrel(
                    tb_trans_dbtrx(cd, trans),
                    relh,
                    0);
        if (rc != SU_SUCCESS) {
            rs_error_create(errhandle, rc);
            return FALSE;
        }

        return tb_addcheckconstraint_relh (
                    cd,
                    trans,
                    relh,
                    schema,
                    catalog,
                    tablename,
                    checkstr,
                    1,
                    name,
                    errhandle
        );
}
#endif /* !SS_MYSQL */


/*#***********************************************************************\
 *
 *              tb_createforkeys
 *
 * Creates the reference keys needed for foreign key checks.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trans - use
 *
 *
 *      authid - in
 *
 *
 *      relh - use
 *
 *
 *      ttype - in
 *
 *
 *      forkey_c - in
 *
 *
 *      forkeys - in
 *
 *
 *      reftable_pa - in
 *
 *
 *      refkey_pa - in
 *
 *
 *      forkey_pa - in
 *
 *
 *      p_errh - out, give
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
static bool tb_createforkeys(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        rs_ttype_t*  ttype,
        uint         forkey_c,
        tb_sqlforkey_t* forkeys,
        su_pa_t*     reftable_pa,
        su_pa_t*     refkey_pa,
        su_pa_t*     forkey_pa,
#ifdef SS_MME
        tb_dd_store_t       storetype,
        tb_dd_persistency_t persistencytype,
#endif /* SS_MME */
        rs_err_t**   p_errh)
{
        uint i;

        for (i = 0; i < forkey_c; i++) {
        
            tb_sqlforkey_t*          fk;
            rs_relh_t*               ref_relh;
            bool                     res;
            tab_addforkey_context_t* context;
            
            fk = &forkeys[i];
            
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
            if (fk->unresolved) {
                continue;
            }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

            ref_relh = su_pa_getdata(reftable_pa, i);
            
#ifdef SS_MME
            res = tb_check_mme_integrity (
                    cd, relh, ref_relh, storetype, persistencytype, p_errh
            );
            if (!res) {
                return FALSE;
            }
#endif /* SS_MME */

            context = SsMemAlloc(sizeof(tab_addforkey_context_t));

            context->relh = relh;
            context->tbrelh = NULL;
            context->ref_relh = su_pa_getdata(reftable_pa, i);
            context->ref_key = su_pa_getdata(refkey_pa, i);
            context->foreign_key = su_pa_getdata(forkey_pa, i);
            context->ttype = ttype;

            context->tcon = NULL;
            context->primkref = NULL;
            context->forkref = NULL;
            context->tcur = NULL;

            res = tb_createforkey(
                    cd,
                    trans,
                    fk,
                    NULL,
                    &context,
                    p_errh);

            if (!res) {
                return FALSE;
            } else {
                su_ret_t rc;
                rc = tb_dd_resolverefkeys(cd, su_pa_getdata(reftable_pa, i));
                if (rc != SU_SUCCESS) {
                    su_err_init(p_errh, rc);
                    return(FALSE);
                }
            }
        }
        return TRUE;
}

#endif /* REFERENTIAL_INTEGRITY */

/*#***********************************************************************\
 *
 *              forkey_pa_done
 *
 * Releases supas used when creating foreign keys.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      reftable_pa - in, take
 *
 *
 *      refkey_pa - in, take
 *
 *
 *      forkey_pa - in, take
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
static void forkey_pa_done(
        rs_sysi_t* cd,
        su_pa_t* reftable_pa,
        su_pa_t* refkey_pa,
        su_pa_t* forkey_pa)
{
        size_t i;
        rs_relh_t* relh;

        su_pa_do_get(reftable_pa, i, relh) {
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);
        }
        su_pa_done(reftable_pa);
        su_pa_done(refkey_pa);
        su_pa_done(forkey_pa);
}


/*##**********************************************************************\
 *
 *      del_key
 *
 * Utility function to delete rel in case of error.
 * To be used only in tb_createrelation_ext.
 */

static bool del_rel(
        void*        cd,
        rs_relh_t*   rel,
        rs_ttype_t*  ttype,
        rs_key_t*    key,
        char*        keyname,
        rs_entname_t* en)
{
        rs_ttype_free(cd, ttype);
        SS_MEM_SETUNLINK(rel);
        rs_relh_done(cd, rel);
        rs_key_done(cd, key);
        if (keyname) {
            SsMemFree(keyname);
        }
        rs_rbuf_removename(cd, rs_sysi_rbuf(cd), en, RSRBUF_NAME_RELATION);
        SS_POPNAME;
        return(FALSE);
}

/*##**********************************************************************\
 *
 *              tb_createrelation_ext
 *
 * Creates a new relation into the database. Optionally returns the
 * relation handle of the new relation.
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
 *      relname - in, use
 *              name of the table to be created
 *
 *      authid - in, use
 *              authorization id for the table (NULL if none)
 *
 *      ttype - in, use
 *              row type object describing the column types
 *          and names
 *
 *      primkey - in, use
 *              if non-NULL, description of a primary key
 *
 *      unique_c - in, use
 *              the number of UNIQUE constraints
 *
 *      unique - in, use
 *              an array of UNIQUE constraints (used only
 *          if unique_c > 0)
 *
 *      forkey_c - in, use
 *              the number of foreign keys that are defined
 *          in the forkeys
 *
 *      forkeys - in, use
 *              the array of foreign key definitions (used only
 *          if forkey_c > 0)
 *
 *      def - in, use
 *              if non-NULL, an integer array containing
 *          1 for all the columns for which a default
 *          value is provided and 2 for all the columns
 *          where "DEFAULT USER" is set. If NULL, no
 *          defaults are set
 *
 *      defvalue - in, use
 *              used only if def is non-NULL. Row instance
 *          containing the default values marked with
 *          1's in the def array
 *
 *      check_c - in
 *          the number of check constraints that are
 *          defined in the checks array
 *
 *      checks - in
 *          an array of SQL strings containing the CHECK
 *          constraints on the rows in the table. The
 *          individual non-empty SQL constraints can be
 *          combined by surrounding them with parenthesis
 *          and inserting ANDs between the components
 *
 *      checknames - in
 *          array containing the names of the names of
 *          the check constraints (empty string as an
 *          entry means no name)
 *
 *      p_relh - out, give
 *          if non-NULL, rs_relh_t* of new relation is returned here
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
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
bool tb_createrelation_ext(
        void*               cd,
        tb_trans_t*         trans,
        char*               relname,
        char*               authid,
        char*               catalog,
        char*               extrainfo,
        rs_ttype_t*         usr_ttype,
        tb_sqlunique_t*     primkey,
        uint                unique_c,
        tb_sqlunique_t*     unique,
        uint                forkey_c,
        tb_sqlforkey_t*     forkeys,
        uint*               def,
        rs_tval_t*          defvalue,
        uint                check_c,
        char**              checks,
        char**              checknames,
        tb_dd_createrel_t   createtype,
        tb_dd_persistency_t persistencytype,
        tb_dd_store_t       storetype,
        tb_dd_durability_t  durability __attribute__ ((unused)),
        tb_relmode_t        add_relmode,
        rs_relh_t**         p_relh,
        rs_err_t**          p_errh)
{
        rs_relh_t*   rel;
        rs_ttype_t*  ttype; /* Temporary ttype for new relation */
        rs_ano_t     user_nattrs;
        su_ret_t     rc;
        bool         b;
        long         uid_array[2];
        rs_ano_t     tupleid_ano = RS_ANO_NULL;
        rs_ano_t     tupleversion_ano = RS_ANO_NULL;
        rs_ano_t     sync_ispubltuple_ano = RS_ANO_NULL;
        rs_ano_t     sync_tupleversion_ano = RS_ANO_NULL;
        TliConnectT* tcon;
        long         current_userid;
        su_pa_t*     reftable_pa;
        su_pa_t*     refkey_pa;
        su_pa_t*     forkey_pa;
        rs_key_t*    cluster_key = NULL;
        rs_auth_t*   auth;
        dbe_db_t*    db;
        rs_entname_t en;
        long         relid=-1;
        uint          i;
        tb_relmodemap_t relmodes = (tb_relmodemap_t)0;
        uint pklen;

        ss_dprintf_1(("tb_createrelation_ex\n"));
        ss_dprintf_2(("persistencytype=%d, storetype=%d, add_relmode=%d\n", persistencytype, storetype, add_relmode));
        SS_PUSHNAME("tb_createrelation_ex");
        ss_dassert(relname);

        db = rs_sysi_db(cd);

        if (tb_trans_geterrcode(cd, trans, p_errh) != SU_SUCCESS) {
            SS_POPNAME;
            return(FALSE);
        }
        auth = rs_sysi_auth(cd);
        if (!RS_ENTNAME_ISSCHEMA(authid)) {
            authid = rs_auth_schema(cd, auth);
        }
        catalog = tb_catalog_resolve(cd, catalog);

        rs_entname_initbuf(&en,
                           catalog,
                           authid,
                           relname);

        if (storetype == TB_DD_STORE_DEFAULT
            && add_relmode == TB_RELMODE_MAINMEMORY) 
        {
            ss_dprintf_2(("set storetype=TB_DD_STORE_MEMORY\n"));
            storetype = TB_DD_STORE_MEMORY;
        }

#ifdef SS_FAKE

            /*
             * Toggle between M-tables, optimistic D-tables and pessimistic
             * D-tables when FAKE is set and storetype == TB_DD_STORE_DEFAULT.
             */

            FAKE_IF(FAKE_TAB_TOGGLETABLETYPES) {

                if (storetype == TB_DD_STORE_DEFAULT
                    && add_relmode == TB_RELMODE_SYSDEFAULT
                    && (SsStrcmp(authid, (char *)"_SYSTEM") != 0))
                {
                    tabletogglecount++;
                    switch (tabletogglecount % 3) {
                        case 0:
                            ui_msg_message(TAB_MSG_FAKERANDOMTABLE_SS, relname, "M-TABLE");
                            storetype = TB_DD_STORE_MEMORY;
                            break;
                        case 1:
                            ui_msg_message(TAB_MSG_FAKERANDOMTABLE_SS, relname, "D-TABLE optimistic");
                            storetype = TB_DD_STORE_DISK;
                            add_relmode |= TB_RELMODE_OPTIMISTIC;
                            break;    
                        case 2:
                            ui_msg_message(TAB_MSG_FAKERANDOMTABLE_SS, relname, "D-TABLE pessimistic");
                            storetype = TB_DD_STORE_DISK;
                            add_relmode |= TB_RELMODE_PESSIMISTIC;
                            break;
                        default:
                            ss_error;
                    }
                }
            }

            /*
             * Force D-tables only
             * 
             */
            FAKE_IF(FAKE_TAB_FORCEDTABLES) {
                ui_msg_message(TAB_MSG_FAKERANDOMTABLE_SS, relname, "D-TABLE");
                storetype = TB_DD_STORE_DISK;
            }
#endif

        if (persistencytype == TB_DD_PERSISTENCY_TEMPORARY) {
            /*keyword GLOBAL is optional, accept this*/
            persistencytype = TB_DD_PERSISTENCY_GLOBAL_TEMPORARY;
        }
        if (persistencytype == TB_DD_PERSISTENCY_GLOBAL_TEMPORARY
            && storetype == TB_DD_STORE_DEFAULT) {
            /*force temporary tables to memory if STORE not specified*/
            storetype = TB_DD_STORE_MEMORY;
        }

        if (persistencytype == TB_DD_PERSISTENCY_TRANSIENT
            && storetype == TB_DD_STORE_DEFAULT) {
            /*force tramsient tables to memory if STORE not specified*/
            storetype = TB_DD_STORE_MEMORY;
        }

#if 0
        if (def) {
            rs_error_create(p_errh, E_DEFNOSUP);
            SS_POPNAME;
            return(FALSE);
        }
#endif
        if (strlen(relname) > RS_MAX_NAMELEN) {
            rs_error_create(p_errh, E_TOOLONGNAME_S, relname);
            SS_POPNAME;
            return(FALSE);
        }
        /* (Devendra) fix for tpr 390142*/
        if (relname[0] == '\0') {
            rs_error_create(p_errh, E_BLANKNAME);
            SS_POPNAME;
            return(FALSE);
        }
        /*------------------------------*/


#ifndef REFERENTIAL_INTEGRITY
        if (forkey_c) {
            rs_error_create(p_errh, E_FORKEYNOSUP);
            SS_POPNAME;
            return(FALSE);
        }
#endif /* not REFERENTIAL_INTEGRITY */

        if (!tb_priv_checkschemaforcreateobj(cd, trans, &en, &current_userid, p_errh)) {
            SS_POPNAME;
            return (FALSE);
        }

        if (!rs_rbuf_addname(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION, -1L)
            && !dbe_trx_namedeleted(tb_trans_dbtrx(cd, trans), &en))
        {
            rs_error_create(p_errh, E_RELEXIST_S, relname);
            SS_POPNAME;
            return (FALSE);
        }
        rc = dbe_trx_insertname(tb_trans_dbtrx(cd, trans), &en);
        if (rc != DBE_RC_SUCC) {
            rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
            rs_error_create(p_errh, rc);
            SS_POPNAME;
            return (FALSE);
        }
        if (createtype == TB_DD_CREATEREL_SYNC) {
            int i;
            int nattrs;
            ttype = rs_ttype_copy(cd, usr_ttype);
            nattrs = rs_ttype_sql_nattrs(cd, ttype);
            user_nattrs = 0;
            for (i = 0; i < nattrs; i++) {
                rs_atype_t* atype;
                atype = rs_ttype_sql_atype(cd, ttype, i);
                if (!rs_atype_pseudo(cd, atype)) {
                    user_nattrs++;
                }
            }
            sync_ispubltuple_ano = rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_SYNC_ISPUBLTUPLE);
            sync_tupleversion_ano = rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_SYNCTUPLEVERS);
            tupleversion_ano = rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_TUPLE_VERSION);
            tupleid_ano = rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_TUPLE_ID);
        } else {
            int i;
            int nattrs;

            nattrs = rs_ttype_nattrs(cd, usr_ttype);
            if (nattrs > RS_MAX_COLUMNS) {
                rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
                rs_error_create(p_errh, E_TOOMANYCOLS_D, RS_MAX_COLUMNS);
                SS_POPNAME;
                return(FALSE);
            }
            for (i = 0; i < nattrs; i++) {
                char* aname;
                aname = rs_ttype_aname(cd, usr_ttype, i);
                if (strlen(aname) > RS_MAX_NAMELEN) {
                    rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
                    rs_error_create(p_errh, E_TOOLONGNAME_S, aname);
                    SS_POPNAME;
                    return(FALSE);
                }
                if (rs_sdefs_sysaname(aname)) {
                    rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
                    rs_error_create(p_errh, E_SYSNAME_S, aname);
                    SS_POPNAME;
                    return (FALSE);
                }
                /* Check for internal name conflict. */
                if (rs_ttype_anobyname(cd, usr_ttype, aname) != i) {
                    rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
                    rs_error_create(p_errh, E_ATTREXISTONREL_SS, aname, relname);
                    SS_POPNAME;
                    return (FALSE);
                }
            }
            {
                /* Create here a tuple type that represents the physical
                * ttype of new relation.
                */
                rs_atype_t* atype;
                rs_ano_t sys_ano;

                ttype = rs_ttype_copy(cd, usr_ttype);
                user_nattrs = rs_ttype_nattrs(cd, ttype);

                sys_ano = user_nattrs;

#ifdef SS_ALWAYS_HAVE_TUPLE_ID
                {
#else
                if (primkey == NULL && createtype != TB_DD_CREATEREL_SYNC) {
#endif
                    /* Let's add a TUPLE_ID attribute to the tuple type */
                    atype = rs_atype_init(cd,
                                          RSAT_TUPLE_ID,
                                          RSDT_BIGINT,
                                          RSSQLDT_BIGINT,
                                          RS_BIGINT_PREC,
                                          RS_BIGINT_SCALE,
                                          FALSE);
                    ss_dassert(atype != NULL);

                    tupleid_ano = sys_ano++;
                    rs_ttype_setatype(cd, ttype, tupleid_ano, atype);
                    rs_ttype_setaname(cd, ttype, tupleid_ano, (char *)RS_ANAME_TUPLE_ID);
                    rs_atype_free(cd, atype);
                }

                /* Add a TUPLE_VERSION attribute to the tuple type */
                atype = rs_atype_initrowver(cd, FALSE);

                tupleversion_ano = sys_ano++;
                rs_ttype_setatype(cd, ttype, tupleversion_ano, atype);
                rs_ttype_setaname(cd, ttype, tupleversion_ano, (char *)RS_ANAME_TUPLE_VERSION);
                rs_atype_free(cd, atype);

                if (createtype == TB_DD_CREATEREL_SYNC) {

                    /* Add a SYNC_ISPUBLTUPLE attribute to the tuple type */
                    atype = rs_atype_initsyncispubltuple(cd, FALSE);

                    rs_atype_setsync(cd, atype, TRUE); /* _MLEVEL */
                    sync_ispubltuple_ano = sys_ano++;
                    rs_ttype_setatype(cd, ttype, sync_ispubltuple_ano, atype);
                    rs_ttype_setaname(cd, ttype, sync_ispubltuple_ano, (char *)RS_ANAME_SYNC_ISPUBLTUPLE);
                    rs_atype_free(cd, atype);

                    /* Add a SYNC_TUPLE_VERSION attribute to the tuple type */
                    atype = rs_atype_initsynctuplevers(cd, FALSE);

                    rs_atype_setsync(cd, atype, TRUE); /* _MLEVEL */
                    sync_tupleversion_ano = sys_ano++;
                    rs_ttype_setatype(cd, ttype, sync_tupleversion_ano, atype);
                    rs_ttype_setaname(cd, ttype, sync_tupleversion_ano, (char *)RS_ANAME_SYNCTUPLEVERS);
                    rs_atype_free(cd, atype);
                }

                rs_ttype_addpseudoatypes(cd, ttype);

                /* Put any default values to the corresponding atypes. */
                if (def != NULL) {
                    rs_ano_t    ano;
                    rs_aval_t*  aval;
                    
                    for (ano = 0; ano < user_nattrs; ano++) {
                        if (def[ano] == 1) {
                            atype = rs_ttype_atype(cd, ttype, ano);
                            aval = rs_tval_aval(cd, ttype, defvalue, ano);

                            if (!tb_dd_checkdefaultvalue(
                                        cd,
                                        atype,
                                        atype,
                                        aval,
                                        p_errh)) {
                                rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
                                rs_ttype_free(cd, ttype);

                                SS_POPNAME;
                                return FALSE;
                            }
                            rs_atype_insertcurrentdefault(
                                    cd,
                                    atype,
                                    rs_aval_copy(cd, atype, aval));
                        }
                    }
                }
            }
        }

        rel = rs_relh_init(cd, &en, 0L, ttype);
        ss_assert(rel != NULL);
        SS_MEM_SETLINK(rel);

#if 0
        b = rs_relh_setdefault(cd, rel, def, defvalue);
        ss_dassert(b);
#endif

        if (primkey != NULL) {

            /* Construct a primary key, it is also the clustering key.
             */
            char* prikeyname;
            rs_key_t*   primary_key = NULL;
            rs_ano_t    phys_kpindex = 0L;
            rs_ano_t    user_kpindex = 0L;
            rs_ano_t    user_ano = 0L;
            bool        b;

            if (createtype != TB_DD_CREATEREL_SYSTEM) {
                relid = dbe_db_getnewrelid_log(db);
            }

            if (primkey->name && primkey->name[0]!='\0') {
                prikeyname = SsMemStrdup(primkey->name);
            } else {
                char* fmt = (char *)RSK_USER_PRIMKEYSTR;
                prikeyname = SsMemAlloc(
                                    strlen(relname) +
                                    strlen(fmt) + (RSK_RELID_DISPLAYSIZE + 1) + 1);
                                
                if (createtype != TB_DD_CREATEREL_SYSTEM) {
                    SsSprintf(prikeyname, fmt, relname, relid);
                } else {
                    fmt = (char *)RSK_PRIMKEYSTR;
                    SsSprintf(prikeyname, fmt, relname);
                }
            }

            pklen = primkey->len;

            ss_dprintf_2(("%s: rs_createrelation constructing a primary key.\n",
                           __FILE__ ));

            primary_key = rs_key_init(
                            cd,
                            prikeyname,
                            0L,                     /* key_id (undefined) */
                            TRUE,                   /* unique */
                            TRUE,                   /* clustering */
                            TRUE,                   /* primary */
                            FALSE,                  /* prejoined */
                            createtype == TB_DD_CREATEREL_SYNC
                                ? pklen + 3
                                : pklen + 1, /* nordering */
                            auth);
            rs_key_setindex_ready(cd, primary_key);
            SsMemFree(prikeyname);

            /* Add KEY_ID system attribute.
             */
            rs_key_addpart(
                cd,
                primary_key,
                phys_kpindex++,
                RSAT_KEY_ID,
                TRUE,
                RS_ANO_NULL,
                NULL
            );

            /* Add all primary key user attributes.
             */
            for (user_kpindex = 0;
                 user_kpindex < (rs_ano_t)primkey->len;
                 user_kpindex++)
            {
                rs_attrtype_t kptype = RSAT_USER_DEFINED;
                void* collation = NULL;
                char* attrname;
                rs_atype_t* atype;
                rs_ano_t ano = primkey->fields[user_kpindex];
                rs_ano_t phys_ano = rs_ttype_sqlanotophys(cd, ttype, ano);

                atype = rs_ttype_atype(cd, ttype, phys_ano);
                rs_atype_setnullallowed(cd, atype, FALSE);

                attrname = rs_ttype_aname(cd, ttype, phys_ano);

                if (rs_key_searchkpno_anytype(cd, primary_key, phys_ano)
                    != RS_ANO_NULL)
                {
                    rs_error_create(p_errh, E_PRIMKDUPCOL_S, attrname);
                    return del_rel(cd, rel, ttype, primary_key, NULL, &en);
                }

                ss_dprintf_2(("%s: rs_createrelation, primkey aname = '%s'\n",
                                __FILE__, attrname));
#ifdef SS_COLLATION
                collation = rs_atype_collation(cd, atype);

                if (collation != NULL) {
                    kptype = RSAT_COLLATION_KEY;
                }
#endif /* SS_COLLATION */

                rs_key_addpart(
                    cd,
                    primary_key,
                    phys_kpindex,
                    kptype,
                    TRUE,
                    phys_ano,
                    collation
                );
                
#ifdef SS_COLLATION
                if (primkey->prefixes != NULL &&
                    primkey->prefixes[user_kpindex] != 0) {
                    rs_keyp_setprefixlength(cd,
                                            primary_key, phys_kpindex,
                                            primkey->prefixes[user_kpindex]);
                }
#endif /* SS_COLLATION */

                ++phys_kpindex;
            }

            if (createtype == TB_DD_CREATEREL_SYNC) {
                /* Add SYNC_ISPUBLTUPLE system attribute.
                 */
                rs_key_addpart(
                    cd,
                    primary_key,
                    phys_kpindex++,
                    RSAT_SYNC,
                    TRUE,
                    sync_ispubltuple_ano,
                    NULL
                );
                /* Add SYNC_TUPLE_VERSION system attribute.
                 */
                rs_key_addpart(
                    cd,
                    primary_key,
                    phys_kpindex++,
                    RSAT_SYNC,
                    TRUE,
                    sync_tupleversion_ano,
                    NULL
                );
            }

#ifdef SS_ALWAYS_HAVE_TUPLE_ID
            /* Add TUPLE_ID system attribute.
             */
            rs_key_addpart(
                cd,
                primary_key,
                phys_kpindex++,
                RSAT_TUPLE_ID,
                TRUE,
                tupleid_ano,
                NULL
            );
#endif

            if (extrainfo == NULL || strcmp(extrainfo, "NOTUPLEVERSION") != 0) {

                /* Add TUPLE_VERSION system attribute.
                 */
                rs_key_addpart(
                    cd,
                    primary_key,
                    phys_kpindex++,
                    RSAT_TUPLE_VERSION,
                    TRUE,
                    tupleversion_ano,
                    NULL
                );
            }

            /* Add all other user attributes.
             */
            for (user_ano = 0; user_ano < user_nattrs; user_ano++) {

                char* attrname;
                rs_ano_t phys_ano;
                bool already_added;

                /* Check if the attribute is already added.
                 */
                already_added = FALSE;
                phys_ano = rs_ttype_sqlanotophys(cd, ttype, user_ano);
                ss_dassert(phys_ano != RS_ANO_NULL);
                if (rs_key_searchkpno_data(cd, primary_key, phys_ano)
                    == RS_ANO_NULL)
                {
                    /* NOTE: keyparts of type RSAT_COLLATION_KEY return
                     * RS_ANO_NULL, that is why they will be added again.
                     * (collation key has no inverse mapping to original
                     * column value)
                     */
                    attrname = rs_ttype_aname(cd, ttype, phys_ano);
                    ss_dprintf_2(("%s: rs_createrelation, primkey aname = '%s'\n",
                                    __FILE__, attrname));
                    rs_key_addpart(
                        cd,
                        primary_key,
                        phys_kpindex++,
                        RSAT_USER_DEFINED,
                        TRUE,
                        phys_ano,
                        NULL
                    );
                }
            }

            b = rs_relh_insertkey(cd, rel, primary_key);
            ss_dassert(b);

            cluster_key = primary_key;

        } else {

            /* No primary key, construct a separate clustering key
             * using the TUPLE_ID.
             */
            char*       clustkeyname;
            rs_ano_t    phys_kpindex = 0L;
            rs_ano_t    user_ano = 0L;

            ss_dprintf_2(("%s: rs_createrelation constructing a clustering key.\n",
                           __FILE__ ));

            if (createtype == TB_DD_CREATEREL_SYNC) {
                {
                    char* fmt = (char *)RSK_USER_PRIMKEYSTR;
                    clustkeyname = SsMemAlloc(strlen(relname) + strlen(fmt) + (RSK_RELID_DISPLAYSIZE + 1) + 1);
                    relid = dbe_db_getnewrelid_log(db);
                    SsSprintf(clustkeyname, fmt, relname, relid);
                }
                cluster_key = rs_key_init(
                                cd,
                                clustkeyname,
                                0L,             /* key_id (undefined) */
                                TRUE,           /* unique */
                                TRUE,           /* clustering */
                                TRUE,           /* primary */
                                FALSE,          /* prejoined */
                                3,              /* nordering */
                                auth
                            );
            } else {
                {
                    char* fmt = (char *)RSK_NEW_CLUSTERKEYSTR;
                    relid = dbe_db_getnewrelid_log(db);
                    clustkeyname = SsMemAlloc(strlen(relname) + strlen(fmt) + (RSK_RELID_DISPLAYSIZE + 1) + 1);
                    SsSprintf(clustkeyname, fmt, relname, relid);
                }
                cluster_key = rs_key_init(
                                cd,
                                clustkeyname,
                                0L,             /* key_id (undefined) */
                                FALSE,          /* unique */
                                TRUE,           /* clustering */
                                FALSE,          /* primary */
                                FALSE,          /* prejoined */
                                2,              /* nordering */
                                auth
                            );
            }
            rs_key_setindex_ready(cd, cluster_key);

            SsMemFree(clustkeyname);

            /* Add KEY_ID system attribute.
             */
            rs_key_addpart(
                cd,
                cluster_key,
                phys_kpindex++,
                RSAT_KEY_ID,
                TRUE,
                RS_ANO_NULL,
                NULL
            );

            if (createtype == TB_DD_CREATEREL_SYNC) {
                /* Add SYNC_ISPUBLTUPLE system attribute.
                 */
                rs_key_addpart(
                    cd,
                    cluster_key,
                    phys_kpindex++,
                    RSAT_SYNC,
                    TRUE,
                    sync_ispubltuple_ano,
                    NULL
                );
                /* Add SYNC_TUPLE_VERSION system attribute.
                 */
                rs_key_addpart(
                    cd,
                    cluster_key,
                    phys_kpindex++,
                    RSAT_SYNC,
                    TRUE,
                    sync_tupleversion_ano,
                    NULL
                );
            } else {
                /* Add TUPLE_ID system attribute, its attribute number is
                 * the first after last user defined.
                 */
                ss_dassert(tupleid_ano != RS_ANO_NULL);
                rs_key_addpart(
                    cd,
                    cluster_key,
                    phys_kpindex++,
                    RSAT_TUPLE_ID,
                    TRUE,
                    tupleid_ano,
                    NULL
                );

            }

            /* Add TUPLE_VERSION system attribute.
             */
            rs_key_addpart(
                cd,
                cluster_key,
                phys_kpindex++,
                RSAT_TUPLE_VERSION,
                TRUE,
                tupleversion_ano,
                NULL
            );

            /* Add all user attributes */
            for (user_ano = 0; user_ano < user_nattrs; user_ano++) {

                void* collation = NULL;
                rs_atype_t* atype;
                char* attrname;
                rs_ano_t phys_ano = rs_ttype_sqlanotophys(cd, ttype, user_ano);

                /* Note! With syncrel anos may not match because there might
                   be deleted columns. */
                ss_dassert(phys_ano == user_ano || createtype == TB_DD_CREATEREL_SYNC);
                attrname = rs_ttype_aname(cd, ttype, phys_ano);
                ss_dprintf_2(("%s: rs_createrelation, primkey aname = '%s'\n",
                                __FILE__, attrname));
#ifdef SS_COLLATION
                /* Add collation keypart for strings */
                atype = rs_ttype_atype(cd, ttype, phys_ano);
                collation = rs_atype_collation(cd, atype);

                // add collation and than add data
                if (collation != NULL) {
                    rs_key_addpart(
                        cd,
                        cluster_key,
                        phys_kpindex++,
                        RSAT_COLLATION_KEY,
                        TRUE,
                        phys_ano,
                        collation
                    );
                }
#endif /* SS_COLLATION */

                rs_key_addpart(
                    cd,
                    cluster_key,
                    phys_kpindex++,
                    RSAT_USER_DEFINED,
                    TRUE,
                    phys_ano,
                    NULL
                );
            }

            b = rs_relh_insertkey(cd, rel, cluster_key);
            ss_dassert(b);
        }

        if (unique_c > 0) {

            uint      key_c;
            char*     keyname;
            char* keyfmt = (char *)RSK_NEW_UNQKEYSTR;

            ss_assert(unique != NULL);

            for (key_c = 0; key_c < unique_c; key_c++) {

                rs_key_t*   unique_key = NULL;
                uint        uniquelen;
                rs_ano_t    phys_kpindex = 0L;
                rs_ano_t    user_kpindex = 0L;
                if (unique[key_c].name != NULL) {
                    char *keyfmt = (char *)RSK_NEW_UNQKEYCONSTRSTR;
                    keyname = SsMemAlloc(strlen(unique[key_c].name)+strlen(keyfmt)+1);
                    SsSprintf(keyname, keyfmt, unique[key_c].name);
                } else {
                    keyname = SsMemAlloc(strlen(relname) +
                                 strlen(keyfmt) + (RSK_RELID_DISPLAYSIZE + 1) +
                                 (10 - 2) + /* max print width for %u - strlen("%u") */
                                 1);
                    SsSprintf(keyname, keyfmt, relname, relid, key_c);
                }

                ss_dassert(key_c < 1000);

                uniquelen = unique[key_c].len;

                ss_dprintf_2(("%s: rs_createrelation constructing a unqkey.\n",
                               __FILE__ ));

                unique_key = rs_key_init(
                                cd,
                                keyname,
                                0L,                     /* key_id (undefined) */
                                TRUE,                   /* unique */
                                FALSE,                  /* clustering */
                                FALSE,                  /* primary */
                                FALSE,                  /* prejoined */
                                uniquelen + 1,  /* nordering */
                                auth
                             );

                /* Add KEY_ID system attribute */
                rs_key_addpart(
                    cd,
                    unique_key,
                    phys_kpindex++,
                    RSAT_KEY_ID,
                    TRUE,
                    RS_ANO_NULL,
                    NULL
                );

                /* Add all user attributes */
                for (user_kpindex = 0;
                     user_kpindex < (rs_ano_t)unique[key_c].len;
                     user_kpindex++)
                {
                    rs_attrtype_t kptype = RSAT_USER_DEFINED;
                    void* collation = NULL;
                    char* attrname;
                    rs_atype_t* atype;
                    rs_ano_t ano = unique[key_c].fields[user_kpindex];
                    rs_ano_t phys_ano = rs_ttype_sqlanotophys(cd, ttype, ano);

                    atype = rs_ttype_atype(cd, ttype, phys_ano);
                    if (rs_atype_nullallowed(cd, atype)) {
                        /* ANSI says unique key columns must be NOT NULL. */
                        rs_error_create(p_errh, E_UNQKCOLNOTNULL);
                        return del_rel(cd, rel, ttype, unique_key, keyname, &en);
                    }

                    attrname = rs_ttype_aname(cd, ttype, phys_ano);

                    if (rs_key_searchkpno_anytype(cd, unique_key, phys_ano)
                        != RS_ANO_NULL)
                    {
                        rs_error_create(p_errh, E_UNQKDUPCOL_S, attrname);
                        return del_rel(cd, rel, ttype, unique_key, keyname, &en);
                    }

                    ss_dprintf_2(("%s: rs_createrelation, unqkey aname = '%s'\n",
                                    __FILE__, attrname));
#ifdef SS_COLLATION
                    collation = rs_atype_collation(cd, atype);

                    if (collation != NULL) {
                        kptype = RSAT_COLLATION_KEY;
                    }
#endif /* SS_COLLATION */

                    rs_key_addpart(
                        cd,
                        unique_key,
                        phys_kpindex++,
                        kptype,
                        TRUE,
                        phys_ano,
                        collation
                    );
                }

#ifndef DBE_UPDATE_OPTIMIZATION
                /* Add TUPLE_VERSION system attribute.
                 */
                rs_key_addpart(
                    cd,
                    unique_key,
                    phys_kpindex++,
                    RSAT_TUPLE_VERSION,
                    TRUE,
                    tupleversion_ano,
                    NULL
                );
#endif

                if (!tb_checkindex(cd, rel, unique_key)) {
                    rs_error_create(p_errh, E_UNQKDUP_COND);
                    return del_rel(cd, rel, ttype, unique_key, keyname, &en);
                }

                tb_keyinsertref(cd, unique_key, cluster_key, &phys_kpindex);

                rs_key_setindex_ready(cd, unique_key);
                b = rs_relh_insertkey(cd, rel, unique_key);
                ss_dassert(b);

                SsMemFree(keyname);
            }

        }

        reftable_pa = su_pa_init();
        refkey_pa = su_pa_init();
        forkey_pa = su_pa_init();

#ifdef REFERENTIAL_INTEGRITY
        b = tb_initforkeys(
                cd,
                trans,
                relname,
                authid,
                catalog,
                rel,
                ttype,
                forkey_c,
                forkeys,
                reftable_pa,
                refkey_pa,
                forkey_pa,
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                relid,
#endif
                p_errh);

        if (!b) {
            forkey_pa_done(cd, reftable_pa, refkey_pa, forkey_pa);
            rs_ttype_free(cd, ttype);
            SS_MEM_SETUNLINK(rel);
            rs_relh_done(cd, rel);
            rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
            SS_POPNAME;
            return(FALSE);
        }
#endif

        rc = tb_dd_createrel(
                cd,
                trans,
                rel,
                auth,
                createtype,
                relid,
                p_errh);
        if (rc != SU_SUCCESS) {
            rs_rbuf_removename(cd, rs_sysi_rbuf(cd), &en, RSRBUF_NAME_RELATION);
        }

        ss_debug_1(rs_relh_print(cd, rel));

        if (rc != SU_SUCCESS) {
            forkey_pa_done(cd, reftable_pa, refkey_pa, forkey_pa);
            rs_ttype_free(cd, ttype);
            SS_MEM_SETUNLINK(rel);
            rs_relh_done(cd, rel);
            SS_POPNAME;
            return(FALSE);
        }

        uid_array[0] = current_userid;
        uid_array[1] = -1L;

        tcon = TliConnectInitByTrans(cd, trans);

        b = tb_priv_setrelpriv(
                tcon,
                FALSE,
                current_userid,
                createtype != TB_DD_CREATEREL_SYNC,   /* grant option */
                rs_relh_relid(cd, rel),
                createtype == TB_DD_CREATEREL_SYNC
                    ? TB_PRIV_SELECT
                    : (TB_PRIV_SELECT|TB_PRIV_INSERT|TB_PRIV_DELETE|
                       TB_PRIV_UPDATE|TB_PRIV_REFERENCES|TB_PRIV_CREATOR),
                uid_array,
                p_errh);

        TliConnectDone(tcon);

        if (b) {
            if (storetype == TB_DD_STORE_MEMORY
            ||  (storetype == TB_DD_STORE_DEFAULT
                 && dbe_db_getdefaultstoreismemory(db)
                 && !rs_relh_issysrel(cd, rel)))
            {
                if (!dbe_db_ismme(db)) {
                    rs_error_create(p_errh, E_MMENOSUP);
                    b = FALSE;
                }
                if (b && !su_li3_ismainmemsupp()) {
                    rs_error_create(p_errh, E_MMENOLICENSE);
                    b = FALSE;
                }
                if (b && dbe_db_ishsb(db)) {
                    ss_dprintf_2(("%s: unholy matrimony: old HSB & MME\n", __FILE__));
                    rs_error_create(p_errh, DBE_ERR_HSBMAINMEMORY);
                    b = FALSE;
                }
                relmodes |= TB_RELMODE_MAINMEMORY;
                {
                    tb_database_t*  tdb = rs_sysi_tabdb(cd);
                    bool            ismainmem;

                    ismainmem = (storetype == TB_DD_STORE_MEMORY
                                 || (storetype == TB_DD_STORE_DEFAULT
                                     && dbe_db_getdefaultstoreismemory(db)));

                    if (ismainmem && tb_getdefaultistransient(tdb)) {
                        persistencytype = TB_DD_PERSISTENCY_TRANSIENT;
                    }

                    if (ismainmem && tb_getdefaultisglobaltemporary(tdb)) {
                        persistencytype = TB_DD_PERSISTENCY_GLOBAL_TEMPORARY;
                    }
                }
            }
            if (b && !rs_relh_issysrel(cd, rel) &&
                (storetype == TB_DD_STORE_DISK ||
                 (storetype == TB_DD_STORE_DEFAULT
                  && !dbe_db_getdefaultstoreismemory(db))))
            {
                ss_dassert(b);
                if (!su_li3_isdbesupp()) {
                    rs_error_create(p_errh, E_DBENOLICENSE);
                    b = FALSE;
                }
            }

            if (b && persistencytype == TB_DD_PERSISTENCY_GLOBAL_TEMPORARY) {
                relmodes |= TB_RELMODE_GLOBALTEMPORARY;
            }

            if (b && persistencytype == TB_DD_PERSISTENCY_TRANSIENT) {
                relmodes |= TB_RELMODE_TRANSIENT;
            }
            if (add_relmode != TB_RELMODE_SYSDEFAULT) {
                ss_dprintf_2(("add relmode = %d\n", add_relmode));
                relmodes |= add_relmode;
            }
            if (b && relmodes != TB_RELMODE_SYSDEFAULT) {
                b = tb_dd_setrelmodes(cd,
                                      trans,
                                      rel,
                                      relmodes,
                                      TRUE,
                                      p_errh);
            }
        }

        if (!b) {
            forkey_pa_done(cd, reftable_pa, refkey_pa, forkey_pa);
            rs_ttype_free(cd, ttype);
            SS_MEM_SETUNLINK(rel);
            rs_relh_done(cd, rel);
            SS_POPNAME;
            return(FALSE);
        }

        tb_priv_createrelorview(
            cd,
            rs_relh_relid(cd, rel),
            createtype == TB_DD_CREATEREL_SYNC
                ? TB_PRIV_SELECT
                : (TB_PRIV_SELECT|TB_PRIV_INSERT|TB_PRIV_DELETE|
                   TB_PRIV_UPDATE|TB_PRIV_REFERENCES|TB_PRIV_CREATOR),
            createtype != TB_DD_CREATEREL_SYNC);      /* grant option */

#ifdef REFERENTIAL_INTEGRITY
        b = tb_createforkeys(
                cd,
                trans,
                rel,
                ttype,
                forkey_c,
                forkeys,
                reftable_pa,
                refkey_pa,
                forkey_pa,
#ifdef SS_MME
                storetype,
                persistencytype,
#endif /* SS_MME */
                p_errh);

        if (!b) {
            forkey_pa_done(cd, reftable_pa, refkey_pa, forkey_pa);
            rs_ttype_free(cd, ttype);
            SS_MEM_SETUNLINK(rel);
            rs_relh_done(cd, rel);
            SS_POPNAME;
            return(FALSE);
        }
#endif
        forkey_pa_done(cd, reftable_pa, refkey_pa, forkey_pa);

        for (i=0; i<check_c; i++) {
            bool b;
            b = tb_addcheckconstraint_relh(
                    cd,
                    trans,
                    rel,
                    authid,
                    catalog,
                    relname,
                    checks[i],
                    i,
                    *checknames[i] ? checknames[i] : NULL,
                    p_errh);
            if (!b) {
                SS_POPNAME;
                return (FALSE);
            }
        }

        rs_ttype_free(cd, ttype);

        FAKE_CODE_BLOCK(FAKE_TAB_MMETABLESONLY,
            if ((storetype == TB_DD_STORE_DISK
                && !rs_relh_issysrel(cd, rel))
                ||
                (storetype == TB_DD_STORE_DEFAULT
                && !dbe_db_getdefaultstoreismemory(db)
                && !rs_relh_issysrel(cd, rel)))
            {
                SsPrintf("---> Rejecting relation: '%s'\n", relname);
                SS_MEM_SETUNLINK(rel);
                rs_relh_done(cd, rel);
                SS_POPNAME;
                ss_info_assert(FALSE, ("FAKE #%d enabled: Disk tables not allowed.", FAKE_TAB_MMETABLESONLY));
                rs_error_create(p_errh, DBE_ERR_FAILED);
                return(FALSE);
            }
        );

        if (b) {
            su_ret_t rc;
            rc = tb_dd_resolverefkeys(cd, rel);
            if (rc != SU_SUCCESS) {
                b = FALSE;
                su_err_init(p_errh, rc);
            }
        }

        if (!b) {
            SS_MEM_SETUNLINK(rel);
            rs_relh_done(cd, rel);
            SS_POPNAME;
            return(FALSE);
        }

        if (p_relh != NULL) {
            *p_relh = rel;
        } else {
            SS_MEM_SETUNLINK(rel);
            rs_relh_done(cd, rel);
        }

        SS_POPNAME;
        return(TRUE);
}

/*##**********************************************************************\
 *
 *              tb_createrelation
 *
 * Member of the SQL function block.
 * Creates a new relation into the database
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
 *      relname - in, use
 *              name of the table to be created
 *
 *      authid - in, use
 *              authorization id for the table (NULL if none)
 *
 *      catalog - in, use
 *          catalog name for the table (NULL if none)
 *
 *      extrainfo - in, use
 *              extra information for the table (NULL if none)
 *
 *      type - in
 *          0 = permanent table, 1 = temporary table,
 *          2 = global temporary table
 *
 *      storetype - in
 *          0 = default, 1 = local memory, 2 = locale disk
 *
 *      ttype - in, use
 *              row type object describing the column types
 *          and names
 *
 *      primkey - in, use
 *              if non-NULL, description of a primary key
 *
 *      unique_c - in, use
 *              the number of UNIQUE constraints
 *
 *      unique - in, use
 *              an array of UNIQUE constraints (used only
 *          if unique_c > 0)
 *
 *      forkey_c - in, use
 *              the number of foreign keys that are defined
 *          in the forkeys
 *
 *      forkeys - in, use
 *              the array of foreign key definitions (used only
 *          if forkey_c > 0)
 *
 *      def - in, use
 *              if non-NULL, an integer array containing
 *          1 for all the columns for which a default
 *          value is provided and 2 for all the columns
 *          where "DEFAULT USER" is set. If NULL, no
 *          defaults are set
 *
 *      defvalue - in, use
 *              used only if def is non-NULL. Row instance
 *          containing the default values marked with
 *          1's in the def array
 *
 *      check_c - in
 *          the number of check constraints that are
 *          defined in the checks array
 *
 *      checks - in
 *          an array of SQL strings containing the CHECK
 *          constraints on the rows in the table. The
 *          individual non-empty SQL constraints can be
 *          combined by surrounding them with parenthesis
 *          and inserting ANDs between the components
 *
 *      checknames - in
 *          array containing the names of the names of
 *          the check constraints (empty string as an
 *          entry means no name)
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
bool tb_createrelation(
        void*           cd,
        tb_trans_t*     trans,
        char*           relname,
        char*           authid,
        char*           catalog,
        char*           extrainfo,
        uint            type,
        uint            storetype,
        uint            durability,
        rs_ttype_t*     usr_ttype,
        tb_sqlunique_t* primkey,
        uint            unique_c,
        tb_sqlunique_t* unique,
        uint            forkey_c,
        tb_sqlforkey_t* forkeys,
        uint*           def,
        rs_tval_t*      defvalue,
        uint            check_c,
        char**          checks,
        char**          checknames,
        void**          cont,
        rs_err_t**      p_errh)
{
        ss_dprintf_1(("tb_createrelation:persistencytype=%d, storetype=%d, extrainfo='%s'\n",
            type, storetype, extrainfo != NULL ? extrainfo : "NULL"));
        *cont = NULL;
        return(tb_createrelation_ext(
                    cd,
                    trans,
                    relname,
                    authid,
                    catalog,
                    extrainfo,
                    usr_ttype,
                    primkey,
                    unique_c,
                    unique,
                    forkey_c,
                    forkeys,
                    def,
                    defvalue,
                    check_c,
                    checks,
                    checknames,
                    TB_DD_CREATEREL_USER,
                    type,
                    storetype,
                    durability,
                    TB_RELMODE_SYSDEFAULT,
                    NULL,
                    p_errh));
}

#ifndef SS_MYSQL

/*##**********************************************************************\
 *
 *              tb_altertable
 *
 * This function modifies a table in the database.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      tablename -
 *              name of the table to be created
 *
 *      schema -
 *              schema name for the table (NULL if none)
 *
 *      catalog -
 *              catalog name for the table (NULL if none)
 *
 * action
 *      ALTER TABLE action
 * strpar
 *      column name if action is ADD COLUMN, MODIFY
 *      COLUMN, RENAME COLUMN or DROP COLUMN,
 *      schema name if action is MODIFY SCHEMA,
 *      constraint name if action is ADD CONSTRAINT or
 *      or DROP CONSTRAINT, parameters string to SET
 *      if action is SET
 *
 * type
 *      column type (used only if action is ADD COLUMN
 *      or MODIFY COLUMN)
 *
 * typepars
 *      column type parameters (used only if action is
 *      ADD COLUMN or MODIFY COLUMN, NULL if none)
 *
 *      newname -
 *              new column name or new constraint string (used
 *          only if action = 2 or 5)
 *
 *  notnull
 *      1 if NOT NULL is specified (used only if action
 *      is ADD COLUMN or MODIFY COLUMN)
 *
 *  def
 *      0 in case of no default, 1 in case of specified
 *      default, 2 in case of DEFAULT USER (used only
 *      in case of ADD/MODIFY/ALTER COLUMN)
 *
 *  deftype
 *      default type (used only in case of ADD/MODIFY/
 *      ALTER COLUMNand def = 1)
 *
 *  defval
 *      default value (used only in case of ADD/MODIFY/
 *      COLUMN and def = 1)
 *
 *  newname
 *      new column name or new constraint string (used
 *      only if action is RENAME COLUMN or ADD CONSTRAINT)
 *
 *      cascade - in
 *          1 = cascade, 0 = restrict (used only if action
 *          is DROP COLUMNS or DROP CONSTRAINT)
 *
 *      unique - in
 *          the unique or primary key definition (used only
 *          if action is ADD UNIQUE or ADD PRIMARY KEY)
 *
 *      forkey - in
 *          the foreign key definition (used only if
 *          action is ADD FOREIGN KEY )
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
 *      errhandle -
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_altertable(
        void*       cd,
        tb_trans_t* trans,
        char*       tablename,
        char*       schema,
        char*       catalog,
        char*       extrainfo,
        uint        action,
        char*       strpar,
        char*       type,
        char*       typepars,
        bool        notnull,
        uint        def,
        sqlftype_t* deftype,
        sqlfinst_t* defval,
        char*       newname,
        bool        cascade,
        tb_sqlunique_t* unique,
        tb_sqlforkey_t* forkey,
        void**      cont,
        rs_err_t**  errhandle)
{
        rs_atype_t* atype;
        bool succp = TRUE;
        tb_relh_t* tbrelh;

        ss_dprintf_1(("tb_altertable:tablename=%s,action=%d,strpar=%s,type=%s,typepars=%s,newname=%s",
            tablename, action, strpar,
            type != NULL ? type : "NULL",
            typepars != NULL ? typepars : "NULL",
            newname != NULL ? newname : "NULL"));
        ss_dprintf_1(("tb_altertable:notnull=%d,def=%u,deftype=%p,defval=%p,forkey=%p\n",
            notnull, def, deftype, defval, forkey));
        SS_PUSHNAME("tb_altertable");
        ss_debug(*errhandle = NULL);

        catalog = tb_catalog_resolve(cd, catalog);

        if (tb_trans_geterrcode(cd, trans, errhandle) != SU_SUCCESS) {
            SS_POPNAME;
            return(FALSE);
        }

        if (action < 2) {
            ss_dassert(type != NULL);
            atype = rs_atype_create(cd, type, typepars, !notnull, errhandle);
            if (atype == NULL) {
                SS_POPNAME;
                return(FALSE);
            }
            if (notnull
                && (def != 1
                    || rs_aval_isnull(cd, deftype, defval))) {
                rs_error_create(errhandle, E_NOTNULLWITHOUTDEFAULT);
                rs_atype_free(cd, atype);
                SS_POPNAME;
                return FALSE;
            }
        } else {
            atype = NULL;
        }

        tbrelh = tb_relh_create(cd, trans, tablename, schema, catalog, errhandle);
        if (tbrelh == NULL) {
            if (atype != NULL) {
                rs_atype_free(cd, atype);
            }
            SS_POPNAME;
            return(FALSE);
        }

        if (!rs_relh_isbasetable(cd, tb_relh_rsrelh(cd, tbrelh))) {
            if (atype != NULL) {
                rs_atype_free(cd, atype);
            }
            tb_relh_free(cd, tbrelh);
            su_err_init(errhandle, E_NOTBASETABLE);
            SS_POPNAME;
            return(FALSE);
        }

        switch (action) {
            case SQL_ALTERACT_ADDCOLUMN:
                /* add column */
                *cont = NULL;
                succp = tb_atab_altertable(
                            cd,
                            trans,
                            tablename,
                            schema,
                            catalog,
                            TB_ALTERMODE_ADD,
                            strpar,
                            newname,
                            atype,
                            def,
                            deftype,
                            defval,
                            errhandle);
                break;
            case SQL_ALTERACT_MODIFYCOLUMN:
                /* modify column */
                *cont = NULL;
                succp = tb_atab_altertable(
                            cd,
                            trans,
                            tablename,
                            schema,
                            catalog,
                            TB_ALTERMODE_MODIFY,
                            strpar,
                            newname,
                            atype,
                            def,
                            deftype,
                            defval,
                            errhandle);
                break;
            case SQL_ALTERACT_RENAMECOLUMN:
                /* rename column */
                *cont = NULL;
                succp = tb_atab_altertable(
                            cd,
                            trans,
                            tablename,
                            schema,
                            catalog,
                            TB_ALTERMODE_RENAME,
                            strpar,
                            newname,
                            atype,
                            def,
                            deftype,
                            defval,
                            errhandle);
                break;
            case SQL_ALTERACT_DROPCOLUMN:
                /* drop column */
                *cont = NULL;
                succp = tb_atab_altertable(
                            cd,
                            trans,
                            tablename,
                            schema,
                            catalog,
                            TB_ALTERMODE_REMOVE,
                            strpar,
                            newname,
                            atype,
                            def,
                            deftype,
                            defval,
                            errhandle);
                break;

            case SQL_ALTERACT_MODIFYSCHEMA:
                succp = tb_atab_altertable(
                            cd,
                            trans,
                            tablename,
                            schema,
                            catalog,
                            TB_ALTERMODE_CHANGESCHEMA,
                            strpar,
                            newname,
                            atype,
                            def,
                            deftype,
                            defval,
                            errhandle);
                break;

            case SQL_ALTERACT_ALTERCOLUMN:
                /* modify schema */
                *cont = NULL;
                succp = tb_atab_altertable(
                            cd,
                            trans,
                            tablename,
                            schema,
                            catalog,
                            TB_ALTERMODE_MODIFY,
                            strpar,
                            newname,
                            atype,
                            def,
                            deftype,
                            defval,
                            errhandle);
                break;

            case SQL_ALTERACT_ADDFOREIGNKEY:
                succp = tb_addforkey(
                            cd,
                            trans,
                            tbrelh,
                            schema,
                            catalog,
                            tablename,
                            forkey,
                            cont,
                            errhandle);
                break;

            case SQL_ALTERACT_ADDUNIQUE:
                succp = tb_addunique(
                            cd,
                            trans,
                            tbrelh,
                            schema,
                            catalog,
                            tablename,
                            unique,
                            cont,
                            errhandle);
                break;

            case SQL_ALTERACT_DROPCONSTRAINT:
                SS_PUSHNAME("tb_altertable:SQL_ALTERACT_DROPCONSTRAINT");
                succp = tb_dropconstraint(
                            cd,
                            trans,
                            tb_relh_rsrelh(cd, tbrelh),
                            schema,
                            catalog,
                            strpar,
                            cont,
                            errhandle);
                SS_POPNAME;
                break;

            case SQL_ALTERACT_ADDCONSTRAINT:
                succp = tb_addcheckconstraint(
                            cd,
                            trans,
                            tbrelh,
                            newname,  /* The constraint is in newname */
                            strpar,   /* Constraint name is in strpar */
                            cont,
                            errhandle);
                break;

            case SQL_ALTERACT_SET:
                /* set ... */
                *cont = NULL;
                succp = tb_atab_set(
                            cd,
                            trans,
                            tbrelh,
                            strpar,
                            errhandle);
                break;

            case SQL_ALTERACT_ALTERADDNOTNULL:
                *cont = NULL;
                succp = tb_atab_altertable(
                            cd,
                            trans,
                            tablename,
                            schema,
                            catalog,
                            TB_ALTERMODE_ADDNOTNULL,
                            strpar,
                            newname,
                            atype,
                            def,
                            deftype,
                            defval,
                            errhandle);
                break;

            case SQL_ALTERACT_ALTERDROPNOTNULL:
                *cont = NULL;
                succp = tb_atab_altertable(
                            cd,
                            trans,
                            tablename,
                            schema,
                            catalog,
                            TB_ALTERMODE_DROPNOTNULL,
                            strpar,
                            newname,
                            atype,
                            def,
                            deftype,
                            defval,
                            errhandle);
                break;

            default:
                /* ss_derror; */
                *cont = NULL;
                *errhandle = NULL;  /* Generate SQL error. */
                succp = FALSE;
                break;
        }

        if (atype != NULL) {
            rs_atype_free(cd, atype);
        }
        tb_relh_free(cd, tbrelh);

        ss_dassert(succp == TRUE || succp == FALSE);
        SS_POPNAME;

        return(succp);
}

#endif /* !SS_MYSQL */

/*##**********************************************************************\
 *
 *              tb_droprelation_ext
 *
 * Drops a relation from the database
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
 *      relname - in, use
 *              name of the relation to be dropped
 *
 *      authid - in, use
 *              authorization id for the relation (NULL if none)
 *
 *      catalog - in
 *          catalog name for the table (NULL if none)
 *
 *      cascade - in
 *          1 = cascade, 0 = restrict
 *
 *      issyncrel - in
 *
 *
 *      p_issyncrel - out
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
bool tb_droprelation_ext(
        void*       cd,
        tb_trans_t* trans,
        bool        usercall,
        char*       relname,
        char*       authid,
        char*       catalog,
        bool        cascade,
        bool        issyncrel,
        bool*       p_issyncrel,
        bool        checkforkeys,
        rs_err_t**  p_errh)
{
        bool succp;
        su_ret_t rc;
        TliConnectT* tcon;
        long relid;
        rs_entname_t en;
        rs_entname_t* p_en = NULL;

        ss_dprintf_1(("%s: Inside tb_droprelation_ext.\n", __FILE__));
        SS_PUSHNAME("tb_droprelation_ext");
        ss_trigger("droptable");

        catalog = tb_catalog_resolve(cd, catalog);

        if (tb_trans_geterrcode(cd, trans, p_errh) != SU_SUCCESS) {
            SS_POPNAME;
            return(FALSE);
        }

        /* (Devendra) fix for tpr 390142*/
        if (relname[0] == '\0') {
            rs_error_create(p_errh, E_BLANKNAME);
            SS_POPNAME;
            return(FALSE);
        }
        /*-----------------------------*/

        rs_entname_initbuf(&en,
                           catalog,
                           authid,
                           relname);

        ss_dprintf_2(("%s: tb_droprelation_ext:tb_dd_droprel.\n", __FILE__));
        succp = tb_dd_droprel(
                    cd,
                    trans,
                    usercall,
                    &en,
                    issyncrel,
                    &relid,
                    &p_en,
                    p_issyncrel,
                    checkforkeys,
                    cascade,
                    p_errh);
        if (!succp) {
            ss_trigger("droptable");
            SS_POPNAME;
            return(FALSE);
        }

#ifndef SS_MYSQL
#ifdef SS_SYNC
        if (!cascade){
            succp = tb_sync_allowsynctablechange(cd, trans, p_en, p_errh);
            if (!succp) {
                rs_entname_done(p_en);
                ss_trigger("droptable");
                SS_POPNAME;
                return(FALSE);
            }
        }
#endif /* SS_SYNC */
#endif /* !SS_MYSQL */

        tcon = TliConnectInitByTrans(cd, trans);

        ss_dprintf_2(("%s: tb_droprelation_ext:tb_priv_droprelpriv.\n", __FILE__));
        succp = tb_priv_droprelpriv(tcon, relid, p_errh);

        TliConnectDone(tcon);

        SS_POPNAME;
        ss_trigger("droptable");

        if (!succp) {
            rs_entname_done(p_en);
            return(FALSE);
        }

        rc = dbe_trx_deletename(tb_trans_dbtrx(cd, trans), p_en);
        rs_entname_done(p_en);

        if (rc != DBE_RC_SUCC) {
            rs_error_create(p_errh, rc);
            return(FALSE);
        }

        return(TRUE);
}

/*##**********************************************************************\
 *
 *              tb_admi_droprelation
 *
 * Drops a relation from the database
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
 *      relname - in, use
 *              name of the relation to be dropped
 *
 *      authid - in, use
 *              authorization id for the relation (NULL if none)
 *
 *      catalog - in
 *          catalog name for the table (NULL if none)
 *
 *      cascade - in
 *          1 = cascade, 0 = restrict
 *
 *      checkforkeys - in
 *          FALSE in case of drop schema/catalog cascade
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
bool tb_admi_droprelation(
        void*       cd,
        tb_trans_t* trans,
        char*       relname,
        char*       authid,
        char*       catalog,
        char*       extrainfo __attribute__ ((unused)),
        bool        cascade,
        bool        checkforkeys,
        rs_err_t**  p_errh)
{
        bool succp = TRUE;
        bool issyncrel = FALSE;

        rs_entname_t  en;
        rs_relh_t*    relh;
        tb_relpriv_t* priv;
        char*         hrelname __attribute__ ((unused));

        SS_PUSHNAME("tb_admi_droprelation");
        catalog = tb_catalog_resolve(cd, catalog);

        rs_entname_initbuf(&en,
                           catalog,
                           authid,
                           relname);

        relh = tb_dd_getrelh(cd, trans, &en, &priv, p_errh);
        if (relh == NULL) {
            SS_POPNAME;
            return(FALSE);
        }

#ifndef SS_MYSQL
        issyncrel = rs_relh_issync(cd, relh);

        if (issyncrel && !cascade) {
            rs_entname_t* p_rname;

            p_rname = rs_entname_copy(rs_relh_entname(cd, relh));
            succp = tb_sync_allowsynctablechange(cd, trans, p_rname, p_errh);
            rs_entname_done(p_rname);
        } else {
            succp = TRUE;
        }

        if (issyncrel && succp) {
            /* drop history first */
            hrelname = rs_sdefs_buildsynchistorytablename(relname);
            succp = tb_droprelation_ext(
                    cd,
                    trans,
                    FALSE, /* simulate non usercall */
                    hrelname,
                    authid,
                    catalog,
                    cascade,
                    TRUE,
                    NULL,
                    checkforkeys,
                    p_errh);
            SsMemFree(hrelname);
        }
#endif /* !SS_MYSQL */

        SS_MEM_SETUNLINK(relh);
        rs_relh_done(cd, relh);

        if (succp) {
            succp = tb_droprelation_ext(
                    cd,
                    trans,
                    TRUE,
                    relname,
                    authid,
                    catalog,
                    cascade,
                    FALSE,
                    &issyncrel,
                    checkforkeys,
                    p_errh);
        }
        SS_POPNAME;
        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_droprelation
 *
 * Member of the SQL function block.
 * Drops a relation from the database
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
 *      relname - in, use
 *              name of the relation to be dropped
 *
 *      authid - in, use
 *              authorization id for the relation (NULL if none)
 *
 *      catalog - in
 *          catalog name for the table (NULL if none)
 *
 *      cascade - in
 *          0 = restrict, 1 = cascade, 2 = cascade constraints
 *
 *      constraint_c
 *          number of constraints to be cascaded (used
 *          only if cascade = 2)
 *
 *      tablenames
 *          array of table names of the constraints (used
 *          only if cascade = 2)
 *
 *      schemas
 *          array of schema names of the constraints (used
 *          only if cascade = 2)
 *
 *      catalogs
 *          array of catalog names of the constraints (used
 *          only if cascade = 2)
 *
 *      extrainfos
 *          array of extra informations for the tables of
 *          the costraints (used only if cascade = 2)
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
bool tb_droprelation(
        void*       cd,
        tb_trans_t* trans,
        char*       relname,
        char*       authid,
        char*       catalog,
        char*       extrainfo,
        bool        cascade,
        uint constraint_c __attribute__ ((unused)),
        char** tablenames __attribute__ ((unused)),
        char** schemas __attribute__ ((unused)),
        char** catalogs __attribute__ ((unused)),
        char** extrainfos __attribute__ ((unused)),
        char** constraints,
        void**      cont,
        rs_err_t**  p_errh)
{
        *cont = NULL;

        if (cascade == 2) {
            if (constraints != NULL) {
                *p_errh = NULL;
                return(FALSE);
            }
            cascade = 1;
        }

        return( tb_admi_droprelation( cd,
                                      trans,
                                      relname,
                                      authid,
                                      catalog,
                                      extrainfo,
                                      cascade,
                                      TRUE,
                                      p_errh) );
}

/*#***********************************************************************\
 * 
 *              tb_truncaterelation_ext
 * 
 * 
 * 
 * Parameters : 
 * 
 *              cd - 
 *                      
 *                      
 *              trans - 
 *                      
 *                      
 *              tablename - 
 *                      
 *                      
 *              schema - 
 *                      
 *                      
 *              catalog - 
 *                      
 *                      
 *              extrainfo - 
 *                      
 *                      
 *              issyncrel - 
 *                      
 *                      
 *              cont - 
 *                      
 *                      
 *              p_errh - 
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
static bool tb_truncaterelation_ext(
        void*       cd,
        tb_trans_t* trans,
        char*       tablename,
        char*       schema,
        char*       catalog,
        char*       extrainfo __attribute__ ((unused)),
        bool        issyncrel,
        void**      cont __attribute__ ((unused)),
        rs_err_t**  p_errh)
{
        bool succp;
        rs_entname_t en;

        ss_dprintf_1(("%s: Inside tb_truncaterelation_ext.\n", __FILE__));
        SS_PUSHNAME("tb_truncaterelation_ext");

        if (tb_trans_geterrcode(cd, trans, p_errh) != SU_SUCCESS) {
            SS_POPNAME;
            return(FALSE);
        }

        if (tablename[0] == '\0') {
            rs_error_create(p_errh, E_BLANKNAME);
            SS_POPNAME;
            return(FALSE);
        }

        rs_entname_initbuf(&en,
                           catalog,
                           schema,
                           tablename);

        succp = tb_dd_truncaterel(
                    cd,
                    trans,
                    &en,
                    issyncrel,
                    p_errh);

        SS_POPNAME;

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_truncaterelation
 *
 *
 *
 * Parameters :
 *
 *      cd - in, use
 *              Client Data.
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 *      tablename - in, use
 *              name of the table to be truncated
 *
 *      schema - in, use
 *              schema name for the table (NULL if none)
 *
 *      catalog - in, use
 *              catalog name for the table (NULL if none)
 *
 *      extrainfo - in, use
 *              extra information for the table (NULL if none)
 *
 *      cont - out, give
 *              *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *err
 *          if err is non-NULL
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_truncaterelation(
        void*       cd,
        tb_trans_t* trans,
        char*       tablename,
        char*       schema,
        char*       catalog,
        char*       extrainfo,
        void**      cont,
        rs_err_t**  p_errh)
{
        rs_entname_t  en;
        rs_relh_t*    relh;
        tb_relpriv_t* priv;
        bool succp = TRUE;
        bool issyncrel;
        char*         hrelname __attribute__ ((unused));

        SS_PUSHNAME("tb_truncaterelation");

        *cont = NULL;
        issyncrel= FALSE;

        catalog = tb_catalog_resolve(cd, catalog);

        rs_entname_initbuf(&en,
                           catalog,
                           schema,
                           tablename);

        relh = tb_dd_getrelh(cd, trans, &en, &priv, p_errh);
        if (relh == NULL) {
            SS_POPNAME;
            return(FALSE);
        }

        /* It is not allowed to truncate Flow history table */
        if (rs_relh_ishistorytable(cd, relh)) {
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);
            rs_error_create(p_errh, E_NOTBASETABLE);
            SS_POPNAME;
            return(FALSE);
        }

#ifndef SS_MYSQL
        issyncrel = rs_relh_issync(cd, relh);

        if (issyncrel) {
            rs_entname_t* p_rname;

            p_rname = rs_entname_copy(rs_relh_entname(cd, relh));
            succp = tb_sync_allowsynctablechange(cd, trans, p_rname, p_errh);
            rs_entname_done(p_rname);
        } else {
            succp = TRUE;
        }

        if (issyncrel && succp) {
            /* truncate history first */
            hrelname = rs_sdefs_buildsynchistorytablename(tablename);
            succp = tb_truncaterelation_ext(
                        cd,
                        trans,
                        hrelname,
                        schema,
                        catalog,
                        extrainfo,
                        TRUE,
                        cont,
                        p_errh);
            SsMemFree(hrelname);
        }
#endif /* !SS_MYSQL */

        SS_MEM_SETUNLINK(relh);
        rs_relh_done(cd, relh);

        if (succp) {
            succp = tb_truncaterelation_ext(
                        cd,
                        trans,
                        tablename,
                        schema,
                        catalog,
                        extrainfo,
                        FALSE,
                        cont,
                        p_errh);
        }
        SS_POPNAME;
        return(succp);
}

static bool tb_checkindex (
        void*        cd,
        rs_relh_t*   rel,
        rs_key_t*    unique_key)
{
/* AKARYAKIN 2007-02-05: MySQL allows several identical UNIQUE constraints
 * per a table. Fix EBUGZILLA58.  
 */
#ifndef SS_MYSQL

        int j;
        su_pa_t* rel_keys = rs_relh_keys(cd, rel);

        su_pa_do(rel_keys, j) {
            rs_key_t* key = su_pa_getdata(rel_keys, j);
            if ((rs_key_isunique(cd, key) || rs_key_isprimary(cd, key))
              && rs_key_issamekey(cd, unique_key, key, FALSE))
            {
                return FALSE;
            }
        }

#endif

        return TRUE;
}

static bool tb_isduplicateindex (
        void*        cd,
        rs_relh_t*   rel,
        rs_key_t*    unique_key)
{
        int j;
        su_pa_t* rel_keys = rs_relh_keys(cd, rel);

        su_pa_do(rel_keys, j) {
            rs_key_t* key = su_pa_getdata(rel_keys, j);
            if (rs_key_issamekey(cd, unique_key, key, TRUE))
            {
                return TRUE;
            }
        }

        return FALSE;
}

/*##**********************************************************************\
 *
 *              tb_createindex_ext
 *
 * Creates an index into the database using given relh.
 *
 * NOTE: In the X/Open specification CREATE UNIQUE INDEX does not
 * require NOT NULL for index columns, altough in the CREATE
 * TABLE unique definition NOT NULL is required. ANSI does
 * not specify CREATE INDEX statement.
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
 *      indexname - in, use
 *              name of the index to be created
 *
 *      authid - in, use
 *              authorization id for the index (NULL if none)
 *
 *      relh - in, use
 *              relation handle
 *
 *      unique - in, use
 *              TRUE if the index is supposed to be unique
 *
 *      attr_c - in, use
 *              number of attributes in the index
 *
 *      attrs - in, use
 *              array of attribute names
 *
 *      desc - in, use
 *              array of boolean values for every attribute.
 *          Value TRUE means DESC (descending attribute)
 *
 *      prefixlengths - in, use
 *          array of prefix lengths, or NULL if none of keyparts is a prefix.
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
bool tb_createindex_ext(
    void*               cd,
    tb_trans_t*         trans,
    char*               indexname,
    char*               authid __attribute__ ((unused)),
    char*               catalog,
    rs_relh_t*          relh,
    rs_ttype_t*         ttype,
    bool                unique,
    uint                attr_c,
    char**              attrs,
    bool*               desc,
#ifdef SS_COLLATION
    size_t*             prefix_lengths,
#endif /* SS_COLLATION */
    tb_dd_createrel_t   type,
    rs_err_t**          p_errh)
{
        rs_key_t*       key;
        rs_key_t*       cluster_key;
        uint            kpno;
        int             phys_kpindex = 0;
        su_ret_t        rc;
        rs_auth_t*      auth;
        int             i;
        rs_ano_t        tupleversion_ano;
        rs_entname_t    en;
        bool            physically_non_unique = !unique;
        
        ss_dprintf_1(("%s: Inside tb_createindex_ext.\n", __FILE__));
        SS_PUSHNAME("tb_createindex_ext");
        SS_NOTUSED(trans);

        catalog = tb_catalog_resolve(cd, catalog);

        if (tb_trans_geterrcode(cd, trans, p_errh) != SU_SUCCESS) {
            SS_POPNAME;
            return(FALSE);
        }

        if (strlen(indexname) > RS_MAX_NAMELEN) {
            rs_error_create(p_errh, E_TOOLONGNAME_S, indexname);
            SS_POPNAME;
            return(FALSE);
        }

        auth = rs_sysi_auth(cd);

        rs_entname_initbuf(
            &en,
            rs_relh_catalog(cd, relh),
            rs_relh_schema(cd, relh),
            indexname);

        key = rs_relh_keybyname(cd, relh, &en);
        if (key != NULL) {
            rs_error_create(p_errh, E_KEYNAMEEXIST_S, indexname);
            SS_POPNAME;
            return (FALSE);
        }

        if (type == TB_DD_CREATEREL_USER) {
            for (i = 0; i < (int)attr_c; i++) {
                if (rs_sdefs_sysaname(attrs[i])) {
                    rs_error_create(p_errh, E_ILLCOLNAME_S, attrs[i]);
                    SS_POPNAME;
                    return (FALSE);
                }
            }
        }

        cluster_key = rs_relh_clusterkey(cd, relh);
        ss_dassert(cluster_key != NULL);

        key = rs_key_init(
                cd,
                indexname,
                0L,         /* key_id (undefined) */
                unique,
                FALSE,      /* clustering */
                FALSE,      /* primary */
                FALSE,      /* prejoined */
                attr_c + 1, /* nordering */
                auth);
        ss_dassert(key != NULL);

        /* Add KEY_ID system attribute */
        rs_key_addpart(
            cd,
            key,
            phys_kpindex++,
            RSAT_KEY_ID,
            TRUE,
            RS_ANO_NULL,
            NULL
        );

        for (kpno = 0; kpno < attr_c; kpno++, phys_kpindex++) {
            char*    attrname = attrs[kpno];
            rs_ano_t phys_ano = rs_ttype_anobyname(cd, ttype, attrname);
            rs_atype_t* atype;
            rs_attrtype_t kptype;
            void* collation = NULL;

            ss_dassert(phys_kpindex == kpno + 1);
            if ((int)phys_ano < 0) {
                /* There is no such attribute name. */
                rs_key_done(cd, key);
                rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, attrname, rs_relh_name(cd, relh));
                SS_POPNAME;
                return(FALSE);
            }

            ss_dassert(attrs[kpno] != NULL);

            if (rs_key_searchkpno_anytype(cd, key, phys_ano) != RS_ANO_NULL) {
                rs_error_create(p_errh, E_INDEXDUPCOL_S, attrname);
                rs_key_done(cd, key);
                SS_POPNAME;
                return(FALSE);
            }

            atype = rs_ttype_atype(cd, ttype, phys_ano);

            if (desc[kpno]) {
                switch (rs_atype_sqldatatype(cd, atype)) {
                    case RSSQLDT_LONGVARBINARY:
                    case RSSQLDT_VARBINARY:
                    case RSSQLDT_BINARY:
                        rs_key_done(cd, key);
                        rs_error_create(p_errh, E_DESCBINARYNOSUP);
                        SS_POPNAME;
                        return(FALSE);
                    case RSSQLDT_BIGINT:
                        rs_key_done(cd, key);
                        rs_error_create(p_errh, E_DESCBIGINTNOSUP);
                        SS_POPNAME;
                        return(FALSE);
                    default:
                        break;
                }
            }

#ifdef SS_SYNC
            ss_dassert(rs_atype_attrtype(cd, atype) == RSAT_USER_DEFINED ||
                       rs_atype_attrtype(cd, atype) == RSAT_SYNC);
#else
            ss_dassert(rs_atype_attrtype(cd, atype) == RSAT_USER_DEFINED);
#endif
            kptype = rs_atype_attrtype(cd, atype);
            physically_non_unique |= rs_atype_nullallowed(cd, atype);
#ifdef SS_COLLATION
            if (kptype == RSAT_USER_DEFINED) {
                collation = rs_atype_collation(cd, atype);

                if (collation != NULL) {
                    kptype = RSAT_COLLATION_KEY;
                }
            }
#endif /* SS_COLLATION */

            rs_key_addpart(
                cd,
                key,
                phys_kpindex,
                kptype,
                !desc[kpno],
                phys_ano,
                collation);
            
#ifdef SS_COLLATION
            if (prefix_lengths != NULL &&
                prefix_lengths[kpno] != 0) {
                rs_keyp_setprefixlength(cd,
                                        key, phys_kpindex,
                                        prefix_lengths[kpno]);
            }
#endif /* SS_COLLATION*/
            
        }
        if (physically_non_unique) {
            rs_key_set_physically_nonunique(cd, key);
        }
#ifndef DBE_UPDATE_OPTIMIZATION
        /* Add TUPLE_VERSION system attribute.
         */
        tupleversion_ano = rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_TUPLE_VERSION);
        ss_dassert(tupleversion_ano != RS_ANO_NULL);
        rs_key_addpart(
            cd,
            key,
            phys_kpindex++,
            RSAT_TUPLE_VERSION,
            TRUE,
            tupleversion_ano,
            NULL
        );
#endif

        if (unique && !tb_checkindex(cd, relh, key)) {
            rs_error_create(p_errh, E_UNQKDUP_COND);
            rs_key_done(cd, key);
            SS_POPNAME;
            return FALSE;
        } else if(!unique && !rs_sysi_allowduplicateindex(cd) && tb_isduplicateindex(cd, relh, key)) {
            rs_error_create(p_errh, E_DUPLICATEINDEX);
            rs_key_done(cd, key);
            SS_POPNAME;
            return FALSE;
        }
        tb_keyinsertref(cd, key, cluster_key, &phys_kpindex);

        rc = tb_dd_createindex(
                cd,
                trans,
                relh,
                ttype,
                key,
                auth,
                &en,
                type,
                p_errh);

        if (rc == DBE_RC_SUCC) {
            rs_relh_insertkey(cd, relh, key);
            rs_key_link(cd, key);
        }

        SS_POPNAME;

        if (rc != DBE_RC_SUCC) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              tb_createindex_prefix
 *
 * Creates an index into the database
 *
 * NOTE: In the X/Open specification CREATE UNIQUE INDEX does not
 * require NOT NULL for index columns, altough in the CREATE
 * TABLE unique definition NOT NULL is required. ANSI does
 * not specify CREATE INDEX statement.
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
 *      indexname - in, use
 *              name of the index to be created
 *
 *      authid - in, use
 *              authorization id for the index (NULL if none)
 *
 *      relname - in, use
 *              name of the relation
 *
 *      tauthid - in, use
 *              authorization id for the relation (NULL if none)
 *
 *      unique - in, use
 *              TRUE if the index is supposed to be unique
 *
 *      attr_c - in, use
 *              number of attributes in the index
 *
 *      attrs - in, use
 *              array of attribute names
 *
 *      desc - in, use
 *              array of boolean values for every attribute.
 *          Value TRUE means DESC (descending attribute)
 *
 *      prefixlengths - in, use
 *          Array of prefix character lengths (or byte lengths for
 *          binary data types) for keypart. (prefix index)
 *          NULL means: none of key parts is a prefix index part.
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
bool tb_createindex_prefix(
        void*       cd,
        tb_trans_t* trans,
        char*       indexname,
        char*       authid,
        char*       catalog,
        char*       extrainfo __attribute__ ((unused)),
        char*       relname,
        char*       tauthid,
        char*       tcatalog,
        char*       textrainfo __attribute__ ((unused)),
        bool        unique,
        uint        attr_c,
        char**      attrs,
        bool*       desc,
#ifdef SS_COLLATION
        size_t*     prefixlengths,
#endif /* SS_COLLATION */
        void**      cont,
        rs_err_t**  p_errh)
{
        rs_relh_t*      relh;
        tb_relh_t*      tbrelh;
        rs_auth_t*      auth;
        rs_entname_t    en;
        long            userid;
        bool            succp;

        ss_dprintf_1(("%s: Inside tb_createindex.\n", __FILE__));
        SS_PUSHNAME("tb_createindex");
        SS_NOTUSED(trans);

        *cont = NULL;

        catalog = tb_catalog_resolve(cd, catalog);
        tcatalog = tb_catalog_resolve(cd, tcatalog);

        if (tb_trans_geterrcode(cd, trans, p_errh) != SU_SUCCESS) {
            SS_POPNAME;
            return(FALSE);
        }
        tbrelh = tb_relh_create(cd, trans, relname, tauthid, tcatalog, p_errh);
        if (tbrelh == NULL) {
            SS_POPNAME;
            return(FALSE);
        }

        relh = tb_relh_rsrelh(cd, tbrelh);
        auth = rs_sysi_auth(cd);

        if (!rs_relh_isbasetable(cd, relh)) {
            tb_relh_free(cd, tbrelh);
            su_err_init(p_errh, E_NOTBASETABLE);
            SS_POPNAME;
            return(FALSE);
        }

        if (authid == NULL) {
            authid = rs_relh_schema(cd, relh);
        }

        rs_entname_initbuf(
            &en,
            rs_relh_catalog(cd, relh),
            rs_relh_schema(cd, relh),
            indexname);

        if (
            (rs_entname_getcatalog(&en) != NULL &&
             catalog != NULL &&
             strcmp(rs_entname_getcatalog(&en), catalog) != 0) ||
            strcmp(rs_entname_getschema(&en), authid) != 0)
        {
            su_err_init(p_errh, E_CREIDXNOSAMECATSCH);
            tb_relh_free(cd, tbrelh);
            SS_POPNAME;
            return (FALSE);
        }

        if (!tb_priv_checkschemaforcreateobj(cd, trans, &en, &userid, p_errh)) {
            tb_relh_free(cd, tbrelh);
            SS_POPNAME;
            return (FALSE);
        }

        if (!tb_relh_ispriv(cd, tbrelh, TB_PRIV_CREATOR)) {
            tb_relh_free(cd, tbrelh);
            rs_error_create(p_errh, E_RELNOTEXIST_S, relname);
            SS_POPNAME;
            return(FALSE);
        }

        if (tb_issysindexname(cd, indexname)) {
            tb_relh_free(cd, tbrelh);
            rs_error_create(p_errh, E_SYSNAME_S, indexname);
            SS_POPNAME;
            return (FALSE);
        }

        succp = tb_createindex_ext(
                cd,
                trans,
                indexname,
                authid,
                catalog,
                relh,
                rs_relh_ttype(cd, relh),
                unique,
                attr_c,
                attrs,
                desc,
#ifdef SS_COLLATION
                prefixlengths,
#endif /* SS_COLLATION */
                TB_DD_CREATEREL_USER,
                p_errh);

        if (succp && rs_relh_issync(cd, relh)) {
            rs_relh_t* sync_relh;
            char* sync_indexname;

            sync_relh = rs_relh_getsyncrelh(cd, relh);
            ss_assert(sync_relh != NULL);
            sync_indexname = SsMemAlloc(sizeof(RSK_SYNCHIST_USERKEYNAMESTR) + strlen(indexname));
            SsSprintf(sync_indexname, RSK_SYNCHIST_USERKEYNAMESTR, indexname);

            succp = tb_createindex_ext(
                    cd,
                    trans,
                    sync_indexname,
                    authid,
                    catalog,
                    sync_relh,
                    rs_relh_ttype(cd, sync_relh),
                    FALSE, /* unique */
                    attr_c,
                    attrs,
                    desc,
#ifdef SS_COLLATION
                    NULL,
#endif /* SS_COLLATION */
                    TB_DD_CREATEREL_SYNC,
                    p_errh);
            SsMemFree(sync_indexname);
        }

        tb_relh_free(cd, tbrelh);
        SS_POPNAME;

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_createindex
 *
 * Member of the SQL function block.
 * Creates an index into the database
 *
 * NOTE: In the X/Open specification CREATE UNIQUE INDEX does not
 * require NOT NULL for index columns, altough in the CREATE
 * TABLE unique definition NOT NULL is required. ANSI does
 * not specify CREATE INDEX statement.
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
 *      indexname - in, use
 *              name of the index to be created
 *
 *      authid - in, use
 *              authorization id for the index (NULL if none)
 *
 *      relname - in, use
 *              name of the relation
 *
 *      tauthid - in, use
 *              authorization id for the relation (NULL if none)
 *
 *      unique - in, use
 *              TRUE if the index is supposed to be unique
 *
 *      attr_c - in, use
 *              number of attributes in the index
 *
 *      attrs - in, use
 *              array of attribute names
 *
 *      desc - in, use
 *              array of boolean values for every attribute.
 *          Value TRUE means DESC (descending attribute)
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
bool tb_createindex(
        void*       cd,
        tb_trans_t* trans,
        char*       indexname,
        char*       authid,
        char*       catalog,
        char*       extrainfo,
        char*       relname,
        char*       tauthid,
        char*       tcatalog,
        char*       textrainfo,
        bool        unique,
        uint        attr_c,
        char**      attrs,
        bool*       desc,
        void**      cont,
        rs_err_t**  p_errh)
{
        bool succp;

        succp = tb_createindex_prefix(cd,
                                      trans,
                                      indexname,
                                      authid,
                                      catalog,
                                      extrainfo,
                                      relname,
                                      tauthid,
                                      tcatalog,
                                      textrainfo,
                                      unique,
                                      attr_c,
                                      attrs,
                                      desc,
#ifdef SS_COLLATION
                                      NULL,
#endif /* SS_COLLATION */
                                      cont,
                                      p_errh);
        return (succp);
}

/*##**********************************************************************\
 *
 *              tb_dropindex
 *
 * Member of the SQL function block.
 * Drops an index from the database
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle of the transaction to which this operation
 *          belongs to (NULL if transactions are not is use)
 *
 *      indexname - in, use
 *              name of the index to be dropped
 *
 *      authid - in, use
 *              authorization id for the index (NULL if none)
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
bool tb_dropindex(
    void*       cd,
    tb_trans_t* trans,
    char*       indexname,
    char*       authid,
        char*       catalog,
        char*       extrainfo __attribute__ ((unused)),
        void**      cont,
    rs_err_t**  p_errh)
{
        su_ret_t   rc;
        rs_entname_t en;
        rs_auth_t*   auth;
        bool         issynctable;

        ss_dprintf_1(("%s: Inside tb_dropindex.\n", __FILE__));
        SS_PUSHNAME("tb_dropindex");

        *cont = NULL;

        catalog = tb_catalog_resolve(cd, catalog);

        if (tb_trans_geterrcode(cd, trans, p_errh) != SU_SUCCESS) {
            SS_POPNAME;
            return(FALSE);
        }

        auth = rs_sysi_auth(cd);
        if (!RS_ENTNAME_ISSCHEMA(authid)) {
            authid = rs_auth_schema(cd, auth);
        }
        rs_entname_initbuf(&en,
                           catalog,
                           authid,
                           indexname);

        if (tb_issysindexname(cd, indexname)) {
            rs_error_create(p_errh, E_SYSNAME_S, indexname);
            SS_POPNAME;
            return (FALSE);
        }

        rc = tb_dd_dropindex(cd, trans, &en, FALSE, &issynctable, p_errh);
        if (rc == SU_SUCCESS && issynctable) {
            char* sync_indexname;

            sync_indexname = SsMemAlloc(sizeof(RSK_SYNCHIST_USERKEYNAMESTR) + strlen(indexname));
            SsSprintf(sync_indexname, RSK_SYNCHIST_USERKEYNAMESTR, indexname);
            rs_entname_initbuf(&en,
                               catalog,
                               authid,
                               sync_indexname);

            rc = tb_dd_dropindex(cd, trans, &en, TRUE, NULL, p_errh);

            SsMemFree(sync_indexname);
        }

        SS_POPNAME;

        if (rc == SU_SUCCESS) {
            return(TRUE);
        } else {
            return(FALSE);
        }
}

bool tb_dropindex_relh(
        void*       cd,
        tb_trans_t* trans,
        rs_relh_t*  relh,
        char*       indexname,
        char*       authid,
        char*       catalog,
        char*       extrainfo __attribute__ ((unused)),
        void**      cont,
        rs_err_t**  p_errh)
{
        su_ret_t   rc;
        rs_entname_t en;
        rs_auth_t*   auth;
        bool         issynctable;

        ss_dprintf_1(("%s: Inside tb_dropindex.\n", __FILE__));
        SS_PUSHNAME("tb_dropindex");

        *cont = NULL;

        catalog = tb_catalog_resolve(cd, catalog);

        if (tb_trans_geterrcode(cd, trans, p_errh) != SU_SUCCESS) {
            SS_POPNAME;
            return(FALSE);
        }

        auth = rs_sysi_auth(cd);
        if (!RS_ENTNAME_ISSCHEMA(authid)) {
            authid = rs_auth_schema(cd, auth);
        }
        rs_entname_initbuf(&en,
                           catalog,
                           authid,
                           indexname);

        if (tb_issysindexname(cd, indexname)) {
            rs_error_create(p_errh, E_SYSNAME_S, indexname);
            SS_POPNAME;
            return (FALSE);
        }

        rc = tb_dd_dropindex_relh(cd, trans, relh, &en, FALSE, &issynctable, p_errh);
        if (rc == SU_SUCCESS && issynctable) {
            char* sync_indexname;

            sync_indexname = SsMemAlloc(sizeof(RSK_SYNCHIST_USERKEYNAMESTR) + strlen(indexname));
            SsSprintf(sync_indexname, RSK_SYNCHIST_USERKEYNAMESTR, indexname);
            rs_entname_initbuf(&en,
                               catalog,
                               authid,
                               sync_indexname);

            rc = tb_dd_dropindex_relh(cd, trans, relh, &en, TRUE, NULL, p_errh);

            SsMemFree(sync_indexname);
        }

        SS_POPNAME;

        if (rc == SU_SUCCESS) {
            return(TRUE);
        } else {
            return(FALSE);
        }
}


/*##**********************************************************************\
 *
 *              tb_authid
 *
 * Member of the SQL function block.
 * Returns the authorization id to be used as the default
 * qualification for relation names. Typically, the user id of the current
 * user should be returned. If the SQL statement belongs to a schema,
 * the authorization id given in the createschema call should be
 * returned.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 * Return value :
 *
 *      pointer into the authorization id
 *      NULL value means that no authorization id is in use
 *
 * Limitations  :
 *
 * Globals used :
 */
char* tb_authid(cd, trans)
    void*       cd;
    tb_trans_t* trans;
{
        rs_auth_t* auth;
        char* authname;

        SS_NOTUSED(trans);

        auth = rs_sysi_auth(cd);
        authname = rs_auth_schema(cd, auth);
        ss_dassert(authname != NULL);
        return(authname);
}

/*#***********************************************************************\
 *
 *              tb_keyinsertref
 *
 * Inserts reference part into a secondary key. The reference part
 * is taken from the ordering parts of the clustering key.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      sec_key - in out, use
 *              secondary key
 *
 *      clust_key - in, use
 *              clustering key
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tb_keyinsertref(cd, sec_key, clust_key, p_nparts)
    void*     cd;
    rs_key_t* sec_key;
    rs_key_t* clust_key;
        rs_ano_t* p_nparts;
{
        rs_ano_t kp_no;
        rs_ano_t kp_ano;
        rs_ano_t nordering;
        rs_attrtype_t kptype;

        nordering = rs_key_nrefparts(cd, clust_key);
        for (kp_no = 0; kp_no < nordering; kp_no++) {
            void* collation = NULL;
            kptype = rs_keyp_parttype(cd, clust_key, kp_no);
            switch (kptype) {
#ifdef SS_COLLATION
                case RSAT_COLLATION_KEY:
                    collation = rs_keyp_collation(cd, clust_key, kp_no);
                    ss_dassert(collation != NULL);
                    /* FALLTHROUGH */
#endif /* SS_COLLATION */
                case RSAT_USER_DEFINED:
                case RSAT_TUPLE_ID:
                case RSAT_SYNC:
                    kp_ano = rs_keyp_ano(cd, clust_key, kp_no);

                    /* check that the attribute does not exist in the key
                     */
                    if (rs_key_searchkpno_anytype(cd, sec_key, kp_ano) == RS_ANO_NULL) {
                        rs_key_addpart(
                            cd,
                            sec_key,
                            *p_nparts,
                            kptype,
                            TRUE, /* store real column value,
                                     not descending key value */
                            kp_ano,
                            collation);
                        (*p_nparts)++;
                    }
                    break;
                case RSAT_TUPLE_VERSION:
                    /* Currently, tuple version is never a part of reference */
                    ss_error;
                    break;
                default:
                    break;
            }
        }
}



#endif /* SS_NODDUPDATE */


bool tb_admi_grantcreatorpriv(
        rs_sysi_t* cd,
        TliConnectT* tcon,
        long id,
        long userid,
        tb_priv_t tbpriv,
        rs_err_t** p_errh)
{
        bool succp;
        long uid_array[2];

        uid_array[0] = userid;
        uid_array[1] = -1L;

        succp = tb_priv_setrelpriv(
                    tcon,
                    FALSE,
                    userid,
                    TRUE,
                    id,
                    tbpriv,
                    uid_array,
                    p_errh);

        if (succp) {
            tb_priv_createrelorview(
                cd,
                id,
                tbpriv,
                TRUE);
        }
        return(succp);
}

bool tb_admi_checkschemaforcreateobj(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_entname_t* en,
        long* p_userid,
        rs_err_t** p_errh)
{
        return(tb_priv_checkschemaforcreateobj(
                    cd,
                    trans,
                    en,
                    p_userid,
                    p_errh));

}

bool tb_admi_checkpriv(
        rs_sysi_t* cd,
        long id,
        char* objschema,
        tb_priv_t checkpriv,
        rs_err_t** p_errh)
{
        tb_relpriv_t* priv;
        bool sysrel;

        ss_dassert(RS_ENTNAME_ISSCHEMA(objschema));

        tb_priv_getrelpriv(cd, id, FALSE, FALSE, &priv);

        sysrel = RS_SDEFS_ISSYSSCHEMA(objschema);

        if (tb_priv_isrelpriv(cd, priv, checkpriv, sysrel)) {
            return(TRUE);
        } else {
            rs_error_create(p_errh, E_NOPRIV);
            return(FALSE);
        }
}

bool tb_admi_droppriv(
        TliConnectT* tcon,
        long relid,
        rs_err_t** p_errh)
{
        return(tb_priv_droprelpriv(
                    tcon,
                    relid,
                p_errh));
}

struct tb_admin_cmd_st {
        rs_sysi_t*      cmd_cd;
        void            (*cmd_callback)(tb_admin_cmd_op_t op, rs_sysi_t* cd, tb_admin_cmd_t* acmd, void* ctx1, void* ctx2);
        void*           cmd_ctx1;
        void*           cmd_ctx2;
};

tb_admin_cmd_t* tb_admin_cmd_init(
        rs_sysi_t* cd)
{
        tb_admin_cmd_t* acmd;

        ss_dprintf_1(("tb_admin_cmd_init\n"));

        acmd = SSMEM_NEW(tb_admin_cmd_t);

        acmd->cmd_callback = NULL;
        acmd->cmd_ctx1     = NULL;
        acmd->cmd_ctx2     = NULL;
        acmd->cmd_cd       = cd;

        ss_dprintf_1(("tb_admin_cmd_init:acmd %lx\n", acmd));  

        return(acmd);
}

void tb_admin_cmd_done(
        tb_admin_cmd_t* acmd)
{
        ss_dprintf_1(("tb_admin_cmd_done:acmd %lx\n", acmd)); 
        
        SsMemFree(acmd);
}

void tb_admin_cmd_setcallback(
        tb_admin_cmd_t* acmd,
        void            (*cmd_callback)(tb_admin_cmd_op_t op, rs_sysi_t* cd, tb_admin_cmd_t* acmd, void* ctx1, void* ctx2),
        void* ctx1,
        void* ctx2)
{
        ss_dprintf_1(("tb_admin_cmd_setcallback:acmd %lx\n", acmd)); 

        acmd->cmd_callback = cmd_callback;
        acmd->cmd_ctx1     = ctx1;
        acmd->cmd_ctx2     = ctx2;
}

void tb_admin_cmd_reset(
        rs_sysi_t* cd,
        tb_admin_cmd_t* acmd)
{
        ss_dprintf_1(("tb_admin_cmd_reset:acmd %lx\n", acmd));

        if (acmd->cmd_callback != NULL) {
            ss_dprintf_1(("tb_admin_cmd_reset:call callback %lx(%lx, %lx)\n", acmd->cmd_callback, acmd->cmd_ctx1, acmd->cmd_ctx2));
            acmd->cmd_callback(TB_ADMIN_CMD_OP_RESET, cd, acmd, acmd->cmd_ctx1, acmd->cmd_ctx2);
        }
}


/*#***********************************************************************\
 *
 *              tb_forkey_init_buf
 *
 * Initializes tb_sqlforkey_t structure in the provided pre-allocated 
 * memory buffer.
 *
 * Parameters :
 *
 *      forkey - in, used
 *              pointer to structure being initialized.  
 *      fk_name - in, used
 *              name of foreign key constraint
 *      mysql_fk_name - in, used only for SS_MYSQL SS_MYSQL_AC
 *              name of foreign key constraint for MySQL
 *      n_fields - in, used
 *              number of fields in foreign key constraint
 *      attids - in, used
 *              pointer to array of size 'n_fields' of attribute IDs of fields
 *              in the child table of the constraint
 *      refen - in, used
 *              pointer to array of size 'n_fields' of names of parent 
 *              table attributes 
 *      refattnames - in, used
 *      delrefact - in, used
 *                  delete cascading action
 *      updrefact - in, used
 *                  update cascading action
 *
 * Return value : none
 *
 * Limitations  : 
 *
 * Globals used :
 */

void tb_forkey_init_buf(tb_sqlforkey_t* forkey,
                        char* fk_name,
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                        char *mysql_fk_name,
#endif
                        uint n_fields,
                        uint* attids,
                        rs_entname_t* refen, /* referenced (parent) table name */
                        char** refattnames,
                        sqlrefact_t delrefact,
                        sqlrefact_t updrefact)
{
        uint f;

        memset(forkey,0,sizeof(tb_sqlforkey_t));
        forkey->name = (char*)SsMemStrdup(fk_name);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        forkey->mysqlname = (char*)SsMemStrdup(mysql_fk_name);
#endif
        forkey->len = n_fields;
        forkey->delrefact = delrefact;
        forkey->updrefact = updrefact;
        forkey->refcatalog = (char*)SsMemStrdup(rs_entname_getcatalog(refen));
        forkey->refschema  = (char*)SsMemStrdup(rs_entname_getschema(refen));
        forkey->reftable   = (char*)SsMemStrdup(rs_entname_getname(refen));
        forkey->fields = (uint*)SsMemAlloc(n_fields * sizeof(uint));
        forkey->reffields = (char**)SsMemAlloc(n_fields * sizeof(char*));
        for (f=0; f<n_fields; ++f) {
            forkey->fields[f] = attids[f];
            forkey->reffields[f] = (char*)SsMemStrdup(refattnames[f]);
        }
        
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        forkey->unresolved = FALSE;
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */        
}

/*#***********************************************************************\
 *
 *              tb_forkey_done_buf
 *
 * Frees tb_sqlforkey_t structure. The memory of the structure itself 
 * is not freed.
 *
 * Parameters :
 *
 *      forkey - in, used
 *              pointer to structure being freed.  
 *
 * Return value : none
 *
 * Limitations  : 
 *
 * Globals used :
 */

void tb_forkey_done_buf(tb_sqlforkey_t* forkey)
{
        uint f;

        SsMemFree(forkey->name);
        if (forkey->refcatalog) {
            SsMemFree(forkey->refcatalog);
        }
        if (forkey->refschema) {
            SsMemFree(forkey->refschema);
        }
        SsMemFree(forkey->reftable);

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        SsMemFree(forkey->mysqlname);
#endif

        SsMemFree(forkey->fields);
        for (f=0; f<forkey->len; ++f) {
            SsMemFree(forkey->reffields[f]);
        }
        SsMemFree(forkey->reffields);
}

#if defined(SS_DEBUG)
    /*#***********************************************************************\
     *
     *              tb_forkey_print
     *
     * Prints foreign key defined by tb_sqlforkey_t structure.
     *
     * Parameters :
     *
     *      cd - in, use
     *              client data
     *      tablename  - in, use
     *              string containing child table name. May be omitted (NULL). 
     *      table_type - in, use
     *              child table type (rs_ttype_t) structure pointer
     *      forkey  - in, use
     *              foreign key structure (tb_sqlforkey_t) pointer
     * Return value :
     *
     * Limitations  :
     *
     * Globals used :
     */

    void tb_forkey_print(void* cd,
                         const char* tablename,
                         rs_ttype_t* table_type,
                         const tb_sqlforkey_t* forkey)
    {
        static const char *SQL_REFACT_NAME[] = {
                                                "CASCADE",
                                                "SETNULL",
                                                "SETDEFAULT",
                                                "RESTRICT",
                                                "NOACTION"
                                                };
        uint i;

        SS_NOTUSED(cd);
        ss_dassert(table_type!=NULL);
        ss_dassert(forkey!=NULL);

        if (tablename) {
            SsDbgPrintf("TABLE %-30s\n", tablename);
        }

        SsDbgPrintf("FOREIGN KEY %-50s"
                #if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                    " %-50s"
                #endif
                    "\nreferences %-40s.%-40s.%-40s ON DEL: %-7s ON UPD: %-7s\n",
                    forkey->name,
                #if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                    forkey->mysqlname,
                #endif
                    forkey->refschema,
                    (forkey->refcatalog ? forkey->refcatalog : "<DBA>"),
                    forkey->reftable,
                    SQL_REFACT_NAME[forkey->delrefact],
                    SQL_REFACT_NAME[forkey->updrefact]
            );

        SsDbgPrintf("    %-2s %-3s %-20s %-10s\n",
                         "KP", "ANO", "ANAME", "PARANAME" );
        for (i=0; i<(uint)forkey->len; ++i) {
            rs_ano_t physano = rs_ttype_sqlanotophys(cd,table_type,forkey->fields[i]);
            SsDbgPrintf("    %-2u %-3d %-20s %-10s\n",
                i,
                physano,
                rs_ttype_aname(cd,table_type,physano),
                forkey->reffields[i] );
        }
    }
#endif /*SS_DEBUG*/
