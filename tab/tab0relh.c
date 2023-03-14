/*************************************************************************\
**  source       * tab0relh.c
**  directory    * tab
**  description  * Relation level operations for rs_relh
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
When relation handle is created the information is fetched from relinfo
buffer (rbuf). The info is not copied, only direct link into the rbuf
is established. This means that all relation handles to the same relation
share exectly the same data. 


Limitations:
-----------
Join tables are not supported

Error handling:
--------------
rs_err_t objects are created.

Objects used:
------------
All res objects
dbe-cursor for inserts


Preconditions:
-------------
rbuf should be created, initialized and stored in cd

Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define TAB0RELH_C
    
#include <ssc.h>
#include <ssstring.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <uti0va.h>

#include <su0error.h>
#include <su0li3.h>

#include <dbe0type.h>
#include <dbe0curs.h>
#include <dbe0rel.h>
#include <dbe0trx.h>
#include <dbe0user.h>
#include <dbe0erro.h>

#include <rs0types.h>
#include <rs0atype.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0aval.h>
#include <rs0cons.h>
#include <rs0relh.h>
#include <rs0viewh.h>
#include <rs0rbuf.h>
#include <rs0sysi.h>
#include <rs0auth.h>
#include <rs0entna.h>
#include <rs0sdefs.h>

#ifdef SS_SYNC
#ifndef SS_MYSQL
#include "tab0sync.h"
#endif /* !SS_MYSQL */
#endif

#include "tab1defs.h"
#include "tab1priv.h"
#include "tab1dd.h"
#include "tab0trig.h"
#include "tab0tran.h"
#include "tab0seq.h"
#include "tab0relc.h"
#include "tab0cata.h"
#include "tab0relh.h"

static uint relh_insertlocal_free(
        void*       cd,
        tb_relh_t*  tbrelh);


bool tb_relh_syncinfo(
        void*       cd,
        rs_relh_t*  relh,
        bool *p_ismaster,
        bool *p_isreplica)
{
        char* catalog;
        bool foundp;
        long id;

        catalog = rs_relh_catalog(cd, relh);

        /* this should be used instead of rs_sysi_issyncxxx */
        foundp = tb_schema_find_catalog_mode(cd, catalog, &id, NULL, p_ismaster, p_isreplica);
        ss_dassert(foundp);

#ifdef SS_DEBUG_FAILS_WITH_SET_SYNC_REPLICA_NO
        if (foundp && rs_relh_issync(cd, relh)) {
            /* if rel has history table then catalog mush be master or replica */
            ss_dassert(*p_ismaster || *p_isreplica);
        }
#endif

        return(foundp);
}


/*#***********************************************************************\
 * 
 *              relh_create
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      trans - in
 *              handle to the current transaction
 *              
 *      relname - 
 *              
 *              
 *      authid - 
 *              
 *              
 *      extrainfo - 
 *              
 *              
 *      throughview - 
 *              
 *              
 *      viewname - 
 *              
 *              
 *      viewauthid - 
 *              
 *              
 *      sqlp - 
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
static tb_relh_t* relh_create(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        char*       relname,
        char*       authid,
        char*       catalog,
        char*       extrainfo,
        uint        throughview,
        char*       viewname,
        char*       viewauthid,
        char*       viewcatalog,
        bool        sqlp,
        rs_err_t**  p_errh)
{
        tb_relh_t* tbrelh;
        rs_relh_t* relh;
        tb_relpriv_t* priv;
        bool foundp = TRUE;
        rs_entname_t ren;
        bool sysview = FALSE;

        SS_NOTUSED(extrainfo);

        if (!tb_dd_checkobjectname(relname)) {
            rs_error_create(p_errh, E_RELNOTEXIST_S, "");
            return(NULL);
        }

        catalog = tb_catalog_resolve_withschema(cd, catalog, authid);

        ss_dprintf_3(("relh_create:%s.%s.%s\n",
            catalog, authid != NULL ? authid : "NULL", relname));

        rs_entname_initbuf(&ren,
                           catalog,
                           authid,
                           relname);

        /* Get the relation handle. Note that the returned handle is
         * linked, so we must call rs_relh_done when the handle is no
         * longer needed.
         */
        if (throughview == 2) {
#ifndef SS_NOVIEW
            /* This call is done through a view, get the privileges 
             * as a combination of privileges from the view and the
             * creator of the view.
             */
            rs_viewh_t* viewh;
            rs_entname_t ven;
            viewcatalog = tb_catalog_resolve_withschema(cd, viewcatalog, viewauthid);
            ss_dprintf_2(("relh_create:throughview:%s.%s\n",
                viewcatalog, viewauthid != NULL ? viewauthid : "NULL", viewname));
            rs_entname_initbuf(&ven,
                               viewcatalog,
                               viewauthid,
                               viewname);

            viewh = tb_dd_getviewh(cd, trans, &ven, NULL, NULL);
            if (viewh != NULL) {
                ss_dprintf_3(("relh_create:%s.%s\n",
                            rs_viewh_schema(cd, viewh),
                            rs_viewh_name(cd, viewh)));
                if (strcmp(rs_viewh_schema(cd, viewh), RS_AVAL_SYSNAME)==0) {
                    sysview = TRUE;
                }
                rs_viewh_done(cd, viewh);
            }


            relh = tb_dd_getrelhfromview(cd, trans, &ren, &ven, &priv, p_errh);
#endif /* SS_NOVIEW */

        } else {
            relh = tb_dd_getrelh(cd, trans, &ren, &priv, p_errh);
        }
        if (relh == NULL) {
            /* Error object already created. */
            return(NULL);
        }
        
        if (!sysview && rs_relh_issysrel(cd, relh)) {
            if (!tb_priv_checkadminaccess(cd, relname, &foundp)) {
                foundp = tb_priv_issomerelpriv(cd, priv);
            }
        } else {
            foundp = tb_priv_issomerelpriv(cd, priv);
        }
        if (!foundp) {
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);
            rs_error_create(p_errh, E_RELNOTEXIST_S, relname);
            return(NULL);
        }

        if (sqlp) {
            rs_sysi_addstmttabnameinfo(cd, rs_relh_entname(cd, relh));
        }

#ifdef SS_SYNC
        if (rs_relh_issync(cd, relh) &&
            rs_relh_getsyncrelh(cd, relh) == NULL) {
            rs_relh_t* sync_relh;
            char *histtabname;
            rs_entname_t sen;

            histtabname = rs_sdefs_buildsynchistorytablename(rs_relh_name(cd, relh));
            ss_dprintf_3(("relh_create:%s.%s (CHECK HISTORYTABLE %s)\n", authid != NULL ? authid : "NULL", relname, histtabname));
            rs_entname_initbuf(&sen,
                               catalog,
                               authid,
                               histtabname);
            sync_relh = tb_dd_getrelh(cd, trans, &sen, NULL, NULL);
            if (sync_relh != NULL) {
                bool succp;
                succp = rs_relh_insertsyncrelh(cd, relh, sync_relh);
                if (!succp) {
                    SS_MEM_SETUNLINK(sync_relh);
                    rs_relh_done(cd, sync_relh);
                }
            } else {
                /* Concurrency problem? */
                SS_MEM_SETUNLINK(relh);
                rs_relh_done(cd, relh);
                SsMemFree(histtabname);
                rs_error_create(p_errh, E_DDOP);
                return(NULL);
            }
            ss_dprintf_3(("relh_create:%s.%s (HAS HISTORYTABLE %s)\n", authid != NULL ? authid : "NULL", relname, rs_entname_getname(&sen)));
            SsMemFree(histtabname);
        }
#endif /* SS_SYNC */

        tbrelh = tb_relh_new(cd, relh, priv);

        if (rs_relh_reltype(cd, tbrelh->rh_relh) == RS_RELTYPE_MAINMEMORY) {
            if (!dbe_db_ismme(rs_sysi_db(cd))) {
                su_err_init(p_errh, E_MMENOSUP);
                tb_relh_free(cd, tbrelh);
                tbrelh = NULL;
            } else if (!su_li3_ismainmemsupp()) {
                su_err_init(p_errh, E_MMENOLICENSE);
                tb_relh_free(cd, tbrelh);
                tbrelh = NULL;
            }
        }

        return(tbrelh);
}

tb_relh_t* tb_relh_new (
        rs_sysi_t*    cd,
        rs_relh_t*    relh,
        tb_relpriv_t* priv)
{
        tb_relh_t* tbrelh;

        tbrelh = SSMEM_NEW(tb_relh_t);
        tbrelh->rh_relh = relh;
        tbrelh->rh_relpriv = priv;
        tbrelh->rh_state = TBRELH_INIT;
        tbrelh->rh_tvalue_copied = FALSE;
        tbrelh->rh_trigstr = NULL;
        tbrelh->rh_trigname = NULL;
        tbrelh->rh_trigctx = NULL;
        tbrelh->rh_isstmtgroup = FALSE;

#ifndef SS_MYSQL
        tbrelh->rh_tabfunb = tb_tint_init();
#endif /* SS_MYSQL */

        ss_debug(tbrelh->rh_chk = TBCHK_RELHTYPE);

#ifdef SS_SYNC
        tbrelh->rh_synctuplevers = NULL;
#endif /* SS_SYNC */

        ss_debug(rs_relh_check(cd, tbrelh->rh_relh));
        return(tbrelh);
}

/*##**********************************************************************\
 * 
 *              tb_relh_create
 * 
 * Local version for tb_relh_create.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      trans - in
 *              handle to the current transaction
 *              
 *      relname - 
 *              
 *              
 *      authid - 
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
tb_relh_t* tb_relh_create(
        void*       cd,
        tb_trans_t* trans,
        char*       relname,
        char*       authid,
        char*       catalog,
        rs_err_t**  p_errh)
{
        tb_relh_t* tbrelh;

        SS_PUSHNAME("tb_relh_create");

        tbrelh = relh_create(
                    cd,
                    trans,
                    relname,
                    authid,
                    catalog,
                    NULL,
                    FALSE,
                    NULL,
                    NULL,
                    NULL,
                    FALSE,
                    p_errh);

        SS_POPNAME;

        return(tbrelh);
}

/*##**********************************************************************\
 * 
 *              tb_relh_sql_create
 * 
 * Member of the SQL function block.
 * Creates a handle for an existing relation in database
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      trans - in
 *              handle to the current transaction
 *              
 *      relname - in, use
 *              name of the relation
 *
 *      authid - in, use
 *              if non-NULL, the authorization id ("user name")
 *          qualifying the relation name  
 *
 *      extrainfo - in, use
 *              if non-NULL, a character string containing
 *          extra information for the relation handle
 *
 *      throughview - in
 *          3 if the table is a result table in a CREATE
 *            PUBLICATION result set
 *          2 if the table is referred through a view,
 *          1 if the table is referred directly when
 *            testing a CREATE VIEW definition
 *          0 if the table is directly referred in the
 *            SQL query being executed
 * 
 *      viewname - in
 *          if throughview = 2, the name of the view in
 *          original query that results into the use
 *          of the table
 * 
 *      viewauthid - in
 *          if throughview = 2, the authid of the view in
 *          original query that results into the use
 *          of the table (NULL if no authid)
 * 
 *      parentviewh - in, use
 *          if throughview = 2, the handle of the view in
 *          original query that results into the use
 *          of the table
 * 
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL. NULL is stored into
 *          *p_errh if the table is not found.
 *
 * Return value - give : 
 * 
 *      !NULL, a pointer into a newly allocated relation handle 
 *       NULL, in case of an error
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
tb_relh_t* tb_relh_sql_create(
        void*       cd,
        tb_trans_t* trans,
        char*       relname,
        char*       authid,
        char*       catalog,
        char*       extrainfo,
        uint        throughview,
        char*       viewname,
        char*       viewauthid,
        char*       viewcatalog,
        tb_viewh_t* parentviewh __attribute__ ((unused)),
        rs_err_t**  p_errh)
{
        tb_relh_t* tbrelh;

        ss_dprintf_1(("tb_relh_sql_create:%s.%s.%s, throughview=%d\n",
            catalog != NULL ? catalog : "NULL",
            authid != NULL ? authid : "NULL",
            relname,
            throughview));
        SS_PUSHNAME("tb_relh_sql_create");

        tbrelh = relh_create(
                    cd,
                    trans,
                    relname,
                    authid,
                    catalog,
                    extrainfo,
                    throughview,
                    viewname,
                    viewauthid,
                    viewcatalog,
                    TRUE,
                    p_errh);

        SS_POPNAME;
        
        return(tbrelh);
}

#if 1 /* JarmoR Mar 3, 1999 */

/*##**********************************************************************\
 * 
 *              tb_relh_create_synchist
 * 
 * Creates sync history tbrelh based on base tbrelh.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      trans - 
 *              
 *              
 *      base_tbrelh - 
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
tb_relh_t* tb_relh_create_synchist(
        void*       cd,
        tb_trans_t* trans __attribute__ ((unused)),
        tb_relh_t*  base_tbrelh)
{
        rs_relh_t* synchist_relh;

        ss_dprintf_3(("tb_relh_create_synchist\n"));

        if (!rs_relh_issync(cd, base_tbrelh->rh_relh)) {
                return(NULL);
        }
        synchist_relh = rs_relh_getsyncrelh(cd, base_tbrelh->rh_relh);
        if (synchist_relh == NULL) {
                /* table does not contain history */
                return(NULL);
        }

        rs_relh_link(cd, synchist_relh);

        return tb_relh_new(cd, synchist_relh, base_tbrelh->rh_relpriv);
}

#endif /* JarmoR Mar 3, 1999 */

/*##**********************************************************************\
 * 
 *              tb_relh_free
 * 
 * Member of the SQL function block.
 * Releases a relation handle
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void tb_relh_free(
        void*      cd,
        tb_relh_t* tbrelh)
{
        CHK_TBRELH(tbrelh);
        ss_dprintf_2(("tb_relh_free\n"));
        
        if (tbrelh->rh_trigctx != NULL) {
            rs_sysi_trigdone(cd, tbrelh->rh_trigctx);
            tbrelh->rh_trigctx = NULL;
        }
        relh_insertlocal_free(cd, tbrelh);
        SS_MEM_SETUNLINK(tbrelh->rh_relh);
        rs_relh_done(cd, tbrelh->rh_relh);

#ifndef SS_MYSQL
        tb_tint_done(tbrelh->rh_tabfunb);
#endif /* !SS_MYSQL */

        SsMemFree(tbrelh);
}

/*##**********************************************************************\
 * 
 *              tb_relh_issame
 * 
 * The tb_relh_issame function checks if a database table referred to
 * a table handle is the same one as specified with a set of qualifiers.
 * The function is used to find the source table in a FROM list that
 * matches a table qualifier in a column reference. The specified qualifiers
 * do not have to make an unambiguous match to identify the table, e.g.
 * "SELECT T1.A FROM S1.T1" identifies T1 to mean S1.T1, although there
 * could be also a table S2.T1. Then, "SELECT T1.A FROM S1.T1, S2.T1"
 * is ambiguous.
 * 
 * Parameters : 
 * 
 *      cd - in
 *              application state
 *              
 *      trans - in
 *              handle to the current transaction
 *              
 *      tbrelh - in
 *              the table handle, from FROM list
 *              
 *      tablename - in
 *              name of the table
 *              
 *      schema - in
 *              if non-NULL, the schema name to qualify the table name
 *              
 *      catalog - in
 *              if non-NULL, the catalog name to qualify the schema name
 *              
 *      extrainfo - in
 *              if non-NULL, a character string containing extra information 
 *          for the table handle
 *              
 *      throughview - in
 *          2 if the table is referred through a view,
 *          1 if the table is referred directly when
 *          testing a CREATE VIEW definition
 *          0 if the table is directly referred in the
 *          SQL query being executed
 *
 *      viewname - in
 *          if throughview = 2, the name of the view in
 *          original query that results into the use
 *          of the table
 *
 *      viewschema - in
 *          if throughview = 2, the schema of the view in
 *          original query that results into the use
 *          of the table (NULL if no schema)
 *
 *      viewcatalog - in
 *          if throughview = 2, the catalog of the view in
 *          original query that results into the use
 *          of the table (NULL if no catalog)
 *
 * Return value : 
 * 
 *      0 - if the qualifiers do not indicate the specified table
 *      1 - if the qualifiers unambiguosly identify the specified table
 *      2 - if there are several database tables (including the
 *          specified one) that match the qualifiers
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
uint tb_relh_issame(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        char*       tablename,
        char*       tableschema,
        char*       tablecatalog,
        char*       extrainfo,
        uint        throughview,
        char*       viewname,
        char*       viewschema,
        char*       viewcatalog)
{
        uint retval;
        tb_relh_t* tbrelh2;

        CHK_TBRELH(tbrelh);

        tablecatalog = tb_catalog_resolve_withschema(cd, tablecatalog, tableschema);

        ss_dprintf_1(("tb_relh_issame:tbrelh %s.%s.%s, param %s.%s\n",
            rs_relh_catalog(cd, tbrelh->rh_relh),
            rs_relh_schema(cd, tbrelh->rh_relh),
            rs_relh_name(cd, tbrelh->rh_relh),
            tablecatalog,
            tableschema != NULL ? tableschema : "NULL",
            tablename));

        /* First check table names.
         */
        if (strcmp(rs_relh_name(cd, tbrelh->rh_relh), tablename) != 0) {
            ss_dprintf_2(("tb_relh_issame:different table names\n"));
            return(0);
        }

        /* If schema is given, schema names must be same.
         */
        if (tableschema != NULL) {
            retval = strcmp(rs_relh_schema(cd, tbrelh->rh_relh), tableschema);
            if (retval == 0) {
                /* Equal schemas, check catalogs. */
                retval = strcmp(
                            rs_relh_catalog(cd, tbrelh->rh_relh),
                            tablecatalog);
            }
            ss_dprintf_2(("tb_relh_issame:tableschema != NULL:retval = %d\n", retval == 0));
            return(retval == 0);
        }

        tbrelh2 = relh_create(
                    cd,
                    trans,
                    tablename,
                    tableschema,
                    tablecatalog,
                    extrainfo,
                    throughview,
                    viewname,
                    viewschema,
                    viewcatalog,
                    FALSE,
                    NULL);
        if (tbrelh2 == NULL) {
            ss_dprintf_2(("tb_relh_issame:param table not found, retval=0\n"));
            return(0);
        } else {
            retval = (rs_relh_relid(cd, tbrelh->rh_relh) ==
                      rs_relh_relid(cd, tbrelh2->rh_relh));
            tb_relh_free(cd, tbrelh2);
            ss_dprintf_2(("tb_relh_issame:relid cmp retval = %d\n", retval));
            return(retval);
        }
}

/*#***********************************************************************\
 * 
 *              relh_insertlocal_init
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      tbrelh - 
 *              
 *              
 *      ttype - 
 *              
 *              
 *      tvalue - 
 *              
 *              
 *      selflags - 
 *              
 *              
 *      p_errh - 
 *              
 *              
 *      truncate - 
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
static uint relh_insertlocal_init(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        bool*       selflags,
        rs_err_t**  p_errh,
        bool        truncate)
{
        uint        sql_attr_n;
        uint        i;
        rs_relh_t*  rel;
        bool        ok_no_truncations = TRUE;
        bool        replica_synctrigger = FALSE;
        bool        ismaster;
        bool        isreplica;
        bool        foundp;

        ss_dprintf_3(("%s: Inside relh_insertlocal_init.\n", __FILE__));
        ss_dassert(tbrelh->rh_state == TBRELH_INIT);
        ss_debug(rs_relh_check(cd, tbrelh->rh_relh));
        ss_dassert(tbrelh->rh_trigctx == NULL);
        
        tbrelh->rh_tvalue_copied = FALSE;

        rel = tbrelh->rh_relh;
        ss_dassert(rs_ttype_nattrs(cd, ttype) ==
                   rs_ttype_nattrs(cd, rs_relh_ttype(cd, rel)));

        if (selflags == NULL) {
            rs_error_create(p_errh, E_NOINSPSEUDOCOL);
            return(TB_CHANGE_ERROR);
        }

        if (!tb_relh_ispriv(cd, tbrelh, TB_PRIV_INSERT)) {
            ss_dprintf_2(("%s: relh_insertlocal_init:E_NOPRIV\n", __FILE__));
            rs_error_create(p_errh, E_NOPRIV);
            return(TB_CHANGE_ERROR);
        }

        isreplica = FALSE;
        if (rs_relh_issync(cd, rel)) {
            foundp = tb_relh_syncinfo(cd, rel, &ismaster, &isreplica);
            ss_dassert(foundp);
        }
        tbrelh->rh_ins_tval = tvalue;

        sql_attr_n = rs_ttype_sql_nattrs(cd, ttype);

        ss_dprintf_2(("%s: relh_insertlocal_init checking selflags\n", __FILE__));

        for (i = 0; i < sql_attr_n; i++) {
            bool ispseudo;
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = rs_ttype_sql_atype(cd, ttype, i);
            ispseudo = rs_atype_pseudo(cd, atype);

            if (ispseudo && selflags[i]) {
#ifdef SS_SYNC
                aval = rs_tval_sql_aval(cd, ttype, tbrelh->rh_ins_tval, i);
                if (isreplica &&
                    rs_atype_issync(cd, atype) &&
                    !rs_aval_isnull(cd, atype, aval)) {
                    selflags[i] = FALSE;
                    /* use local version sequencer */
                    replica_synctrigger = TRUE;
                } else {
                    /* It is illegal to insert pseudo attributes. */
                    rs_error_create(p_errh, E_NOINSPSEUDOCOL);
                    return(TB_CHANGE_ERROR);
                }
#else /* SS_SYNC */
                /* It is illegal to insert pseudo attributes. */
                rs_error_create(p_errh, E_NOINSPSEUDOCOL);
                return(TB_CHANGE_ERROR);
#endif /* SS_SYNC */
            }

            if (!ispseudo && !selflags[i]) {
#if 0
                /* No value given, this column is set to SQL-NULL.
                 */
                aval = rs_tval_sql_aval(cd, ttype, tbrelh->rh_ins_tval, i);
                if (!rs_aval_isnull(cd, atype, aval)) {
                    /* Column not already null, set it null.
                     */
                    if (!tbrelh->rh_tvalue_copied) {
                        /* Make a copy of the row instance so the default
                         * arrangements will not change the original
                         * parameter
                         */
                        tbrelh->rh_ins_tval = rs_tval_copy(cd, ttype, tvalue);
                        tbrelh->rh_tvalue_copied = TRUE;
                        aval = rs_tval_sql_aval(cd, ttype, tbrelh->rh_ins_tval, i);
                    }
                    rs_aval_setnull(cd, atype, aval);
                }
#else
                /* No value given, this column is set to current default.
                 */
                aval = rs_tval_sql_aval(cd, ttype, tbrelh->rh_ins_tval, i);
                atype = rs_ttype_sql_atype(cd, ttype, i);
                if (!tbrelh->rh_tvalue_copied) {
                    /* Make a copy of the row instance so the default
                     * arrangements will not change the original
                     * parameter
                     */
                    tbrelh->rh_ins_tval = rs_tval_copy(cd, ttype, tvalue);
                    tvalue = tbrelh->rh_ins_tval;
                    tbrelh->rh_tvalue_copied = TRUE;
                    aval = rs_tval_sql_aval(cd, ttype, tbrelh->rh_ins_tval, i);
                }
                if (rs_atype_getcurrentdefault(cd, atype) != NULL) {
                    RS_AVALRET_T    r;
                    
                    r = rs_aval_assign_ext(
                            cd,
                            atype,
                            aval,
                            atype,
                            rs_atype_getcurrentdefault(cd, atype),
                            NULL);
                    ss_bassert(r == RSAVR_SUCCESS);
                } else {
                    rs_aval_setnull(cd, atype, aval);
                }
#endif
            }
        }

        /* Note !!!!!!!!!!!!
         * if ok_no_truncations == FALSE
         * a warning should be given to client !
         */
        ok_no_truncations = rs_tval_trimchars(cd, ttype, tvalue, truncate);

        tbrelh->rh_istrigger = rs_relh_triggerstr(cd, rel, TB_TRIG_BEFOREINSERT) ||
                               rs_relh_triggerstr(cd, rel, TB_TRIG_AFTERINSERT);

        if (tbrelh->rh_istrigger) {
            tbrelh->rh_isstmtgroup = TRUE;
            tb_trans_setstmtgroup(cd, trans, TRUE);
        } else {
            tbrelh->rh_isstmtgroup = FALSE;
        }

#ifdef SS_SYNC
        if (replica_synctrigger ||
            (rs_relh_issync(cd, rel))) {
            tbrelh->rh_state = TBRELH_SYNCHISTORY;
        } else
#endif /* SS_SYNC */
        {
            tbrelh->rh_trigstr = rs_relh_triggerstr(cd, rel, TB_TRIG_BEFOREINSERT);
            tbrelh->rh_trigname = rs_relh_triggername(cd, rel, TB_TRIG_BEFOREINSERT);
            if (tbrelh->rh_trigstr != NULL) {
                tbrelh->rh_state = TBRELH_BEFORETRIGGER;
            } else {
                tbrelh->rh_state = TBRELH_CHECK;
            }
        }

        return(TB_CHANGE_CONT);
}

/*#***********************************************************************\
 * 
 *              relh_insertlocal_trigger
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
 *      tbrelh - 
 *              
 *              
 *      ttype - 
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
static uint relh_insertlocal_trigger(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        rs_ttype_t* ttype,
        rs_err_t**  p_errh)
{
        int rc;
        bool issubscribe = FALSE;

        ss_dprintf_3(("%s: Inside relh_insertlocal_trigger.\n", __FILE__));
        ss_dassert(tbrelh->rh_state == TBRELH_BEFORETRIGGER || tbrelh->rh_state == TBRELH_AFTERTRIGGER);
        ss_dassert(tbrelh->rh_isstmtgroup);
        
        issubscribe = rs_sysi_subscribe_write(cd);
        rs_sysi_setsubscribe_write(cd, FALSE);

        rc = rs_sysi_trigexec(
                cd,
                trans,
                tbrelh->rh_trigname,
                tbrelh->rh_trigstr,
                ttype,
                NULL,
                tbrelh->rh_ins_tval,
                &tbrelh->rh_trigctx,
                p_errh);

        rs_sysi_setsubscribe_write(cd, issubscribe);

        switch (rc) {
            case 0:
                return(TB_CHANGE_ERROR);
            case 1:
                break;
            case 2:
                if (tbrelh->rh_state == TBRELH_BEFORETRIGGER) {
                    tbrelh->rh_state = TBRELH_CHECK;
                } else {
                    tbrelh->rh_state = TBRELH_FREE;
                }
                break;
            default:
                ss_error;
        }

        return(TB_CHANGE_CONT);
}


/*#***********************************************************************\
 * 
 *              relh_insertlocal_synchistory
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
 *      tbrelh - 
 *              
 *              
 *      ttype - 
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
#ifdef SS_SYNC
#ifndef SS_MYSQL
static uint relh_insertlocal_synchistory(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        rs_ttype_t* ttype,
        rs_err_t**  p_errh)
{
        rs_relh_t*  rel;
        bool succp;
        bool ispubltuple;

        ss_dprintf_3(("%s: Inside relh_insertlocal_synchistory.\n", __FILE__));
        SS_PUSHNAME("relh_insertlocal_synchistory");

        ss_dassert(tbrelh->rh_state == TBRELH_SYNCHISTORY);

        rel = tbrelh->rh_relh;

        ispubltuple = (tb_trans_getsyncstate(cd, trans, 0) == TB_TRANS_SYNCST_SUBSCRIBEWRITE);

        if (!ispubltuple) {
            /* check if tables catalog is in maintenance mode and
               mode is set by current user
             */
            long catalogid;
            char* catalog;
            rs_sysi_t*   modeowner_cd;

            catalog = rs_relh_catalog(cd, rel);
            ss_dassert(catalog != NULL);

            succp = tb_schema_find_catalog_mode(cd, catalog, &catalogid, &modeowner_cd, NULL, NULL);
            ss_dassert(succp);
            if (cd == modeowner_cd) {
                ispubltuple = TRUE;
            }

        }
       
        succp = tb_synchist_ispubltuple_to_tval(cd, trans, ttype, tbrelh->rh_ins_tval, ispubltuple, FALSE, p_errh);
        if (!succp) {
            /* ss_error; JPA 230998 */
            ss_dprintf_3(("relcur_update_synchistory: error: %s\n", rs_error_geterrstr(cd, *p_errh)));
            SS_POPNAME;
            return(TB_CHANGE_ERROR);
        }
        succp = tb_synchist_nextver_to_tval(
                    cd,
                    trans,
                    ttype,
                    tbrelh->rh_ins_tval,
                    NULL,
                    p_errh);
        if (!succp) {
            ss_dprintf_3(("relcur_update_synchistory: error: %s\n", rs_error_geterrstr(cd, *p_errh)));
            SS_POPNAME;
            return(TB_CHANGE_ERROR);
        }

        tbrelh->rh_trigstr = rs_relh_triggerstr(cd, rel, TB_TRIG_BEFOREINSERT);
        tbrelh->rh_trigname = rs_relh_triggername(cd, rel, TB_TRIG_BEFOREINSERT);
        if (tbrelh->rh_trigstr != NULL) {
            tbrelh->rh_state = TBRELH_BEFORETRIGGER;
        } else {
            tbrelh->rh_state = TBRELH_CHECK;
        }

        SS_POPNAME;
        return(TB_CHANGE_CONT);
}
#endif /* !SS_MYSQL */
#endif /* SS_SYNC */

/*#***********************************************************************\
 * 
 *              relh_insertlocal_check
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      tbrelh - 
 *              
 *              
 *      ttype - 
 *              
 *              
 *      selflags - 
 *              
 *              
 *      p_errh - 
 *              
 *              
 *      truncate - 
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
static uint relh_insertlocal_check(
        void*       cd,
        tb_relh_t*  tbrelh,
        rs_ttype_t* ttype,
        bool*       selflags,
        rs_err_t**  p_errh,
        bool        truncate __attribute__ ((unused)))
{
        uint        sql_attr_n __attribute__ ((unused));
        uint        i;
        rs_relh_t*  rel;

        ss_dprintf_3(("%s: Inside relh_insertlocal_check.\n", __FILE__));
        ss_dassert(tbrelh->rh_state == TBRELH_CHECK);
        ss_debug(rs_relh_check(cd, tbrelh->rh_relh));
        
        rel = tbrelh->rh_relh;
        ss_dassert(rs_ttype_nattrs(cd, ttype) ==
                   rs_ttype_nattrs(cd, rs_relh_ttype(cd, rel)));

        sql_attr_n = rs_ttype_sql_nattrs(cd, ttype);

        ss_dprintf_2(("%s: relh_insertlocal_check checking NOT NULLs\n", __FILE__));

        for (i = 0; i < sql_attr_n; i++) {
            bool ispseudo;
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = rs_ttype_sql_atype(cd, ttype, i);
            ispseudo = rs_atype_pseudo(cd, atype);

            if (!ispseudo && !rs_atype_nullallowed(cd, atype)) {
                /* Check for not NULL.
                 */
                aval = rs_tval_sql_aval(
                            cd,
                            ttype,
                            tbrelh->rh_ins_tval,
                            i);
                if (rs_aval_isnull(cd, atype, aval)) {
                    /* Null not allowed. */
                    rs_error_create(
                        p_errh,
                        !selflags[i]
                            ? E_INSNOTVAL_S
                            : E_NULLNOTALLOWED_S,
                        rs_ttype_sql_aname(cd, ttype, i));
                    return(TB_CHANGE_ERROR);
                }
            }
        }

        tbrelh->rh_state = TBRELH_INSERT;

        return(TB_CHANGE_CONT);
}

static uint relh_dbeinsert(
        void*       cd,
        tb_trans_t* trans,
        rs_relh_t*  relh,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        bool        sap,
        rs_err_t**  p_errh)
{
        uint        retcode;
        dbe_ret_t   rc;
        dbe_trx_t*  trx;

        ss_dprintf_3(("%s: Inside relh_dbeinsert.\n", __FILE__));
        ss_debug(rs_relh_check(cd, relh));
        
        trx = tb_trans_dbtrx(cd, trans);

        ss_output_4(rs_tval_print(cd, ttype, tval));

        ss_dprintf_2(("%s: relh_dbeinsert calling dbe\n", __FILE__));

        if (!sap) {
            tb_trans_stmt_begin(cd, trans);
        }

        ss_pprintf_2(("relh_dbeinsert:table %.255s\n", rs_relh_name(cd, relh)));
        ss_poutput_2(rs_tval_print(cd, ttype, tval));

        rc = dbe_rel_insert(
                trx,
                relh,
                tval,
                p_errh);

        switch (rc) {
            case DBE_RC_SUCC:
                if (rs_relh_isddopactive(cd, relh)) {
                    rs_error_create(p_errh, E_DDOP);
                    retcode = TB_CHANGE_ERROR;
                } else {
                    retcode = TB_CHANGE_SUCC;
                }
                break;
            case DBE_RC_WAITLOCK:
            case DBE_RC_CONT:
                retcode = TB_CHANGE_CONT;
                break;
            default:
                retcode = TB_CHANGE_ERROR;
                break;
        }

        return(retcode);
}

/*#***********************************************************************\
 * 
 *              relh_insertlocal_insert
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
 *      tbrelh - 
 *              
 *              
 *      ttype - 
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
static uint relh_insertlocal_insert(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        rs_ttype_t* ttype,
        bool        sap,
        rs_err_t**  p_errh)
{
        uint        retcode;

        ss_dprintf_3(("%s: Inside relh_insertlocal_insert.\n", __FILE__));
        ss_dassert(tbrelh->rh_state == TBRELH_INSERT);

        retcode = relh_dbeinsert(
                    cd,
                    trans,
                    tbrelh->rh_relh,
                    ttype,
                    tbrelh->rh_ins_tval,
                    sap,
                    p_errh);

        switch (retcode) {
            case TB_CHANGE_CONT:
            case TB_CHANGE_ERROR:
                return(retcode);
            case TB_CHANGE_SUCC:
                retcode = TB_CHANGE_CONT;
                break;
        }
        
        if (tbrelh->rh_isstmtgroup) {
            tbrelh->rh_state = TBRELH_STMTCOMMIT;
        } else {
            tbrelh->rh_trigstr = rs_relh_triggerstr(cd, tbrelh->rh_relh, TB_TRIG_AFTERINSERT);
            tbrelh->rh_trigname = rs_relh_triggername(cd, tbrelh->rh_relh, TB_TRIG_AFTERINSERT);
            if (tbrelh->rh_trigstr != NULL) {
                tbrelh->rh_state = TBRELH_AFTERTRIGGER;
            } else {
                tbrelh->rh_state = TBRELH_FREE;
            }
        }

        return(retcode);
}

/*#***********************************************************************\
 * 
 *              relh_insertlocal_stmtcommit
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
 *      tbrelh - 
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
static uint relh_insertlocal_stmtcommit(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        rs_err_t**  p_errh)
{
        uint    retcode;
        bool    succp;
        bool    finishedp;

        ss_dprintf_3(("%s: Inside relh_insertlocal_stmtcommit.\n", __FILE__));
        ss_dassert(tbrelh->rh_state == TBRELH_STMTCOMMIT);
        ss_dassert(tbrelh->rh_isstmtgroup);

        succp = tb_trans_stmt_commit(cd, trans, &finishedp, p_errh);
        if (!finishedp) {
            return(TB_CHANGE_CONT);
        }

        if (succp) {
            retcode = TB_CHANGE_CONT;
        } else {
            retcode = TB_CHANGE_ERROR;
        }

        tbrelh->rh_trigstr = rs_relh_triggerstr(cd, tbrelh->rh_relh, TB_TRIG_AFTERINSERT);
        tbrelh->rh_trigname = rs_relh_triggername(cd, tbrelh->rh_relh, TB_TRIG_AFTERINSERT);
        if (tbrelh->rh_trigstr != NULL) {
            tbrelh->rh_state = TBRELH_AFTERTRIGGER;
        } else {
            tbrelh->rh_state = TBRELH_FREE;
        }

        return(retcode);
}

/*#***********************************************************************\
 * 
 *              relh_insertlocal_free
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      tbrelh - 
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
static uint relh_insertlocal_free(
        void*       cd,
        tb_relh_t*  tbrelh)
{
        ss_dprintf_3(("%s: Inside relh_insertlocal_free.\n", __FILE__));
        ss_debug(rs_relh_check(cd, tbrelh->rh_relh));
        ss_dassert(tbrelh->rh_trigctx == NULL);

#ifdef SS_SYNC
        dynva_free(&tbrelh->rh_synctuplevers);
#endif      

        if (tbrelh->rh_tvalue_copied) {
            rs_ttype_t* ttype;
            ttype = rs_relh_ttype(cd, tbrelh->rh_relh);
            rs_tval_free(cd, ttype, tbrelh->rh_ins_tval);
            tbrelh->rh_tvalue_copied = FALSE;
        }

        tbrelh->rh_state = TBRELH_INIT;

        return(TB_CHANGE_SUCC);
}

/*#*******************************************************************
 * 
 *              relh_insertlocal
 * 
 * Like tb_relh_insert (see below), but this one takes an extra
 * parameter about whether to truncate character & binary attributes
 * to their maximum length.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              transaction handle
 *
 *      relh - in, use
 *              relation handle
 *
 *      ttype - in, use
 *              tuple type for the tuple to be inserted
 *
 *      tvalue - in, use
 *              attribute values (= tuple) for insert
 *
 *      selflags - in, use
 *              if non-NULL, a boolean array having TRUE for
 *          the attributes that contain data. Remaining attributes
 *          are set to (SQL)NULL
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *              
 *      truncate - in
 *          TRUE if truncation to max. length allowed FALSE when not
 *
 * Return value : 
 * 
 *      TB_CHANGE_SUCC  (1) if the operation is successful
 *      TB_CHANGE_CONT  (3) if the operation did not terminate
 *      TB_CHANGE_ERROR (0) in case of other errors
 *              
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static uint relh_insertlocal(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrelh,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        bool*       selflags,
        rs_err_t**  p_errh,
        bool        truncate,
        bool        sap)
{
        uint retcode = 0;

        ss_dprintf_3(("%s: Inside relh_insertlocal.\n", __FILE__));
        ss_debug(rs_relh_check(cd, tbrelh->rh_relh));

        switch (tbrelh->rh_state) {

            case TBRELH_INIT:
                retcode = relh_insertlocal_init(
                            cd,
                            trans,
                            tbrelh,
                            ttype,
                            tvalue,
                            selflags,
                            p_errh,
                            truncate);

#ifndef SS_MYSQL
                if (retcode == TB_CHANGE_CONT &&
                    tbrelh->rh_state == TBRELH_SYNCHISTORY) {

                    if (!tbrelh->rh_tvalue_copied) {
                        tbrelh->rh_ins_tval = rs_tval_copy(cd, ttype, tvalue);
                        tbrelh->rh_tvalue_copied = TRUE;
                    }

                    retcode = relh_insertlocal_synchistory(
                                cd,
                                trans,
                                tbrelh,
                                ttype,
                                p_errh);
                }
#endif /* !SS_MYSQL */

                if (retcode != TB_CHANGE_CONT ||
                    tbrelh->rh_state != TBRELH_CHECK) {
                    break;
                }
                /* FALLTHROUGH */

            case TBRELH_CHECK:
                retcode = relh_insertlocal_check(
                            cd,
                            tbrelh,
                            ttype,
                            selflags,
                            p_errh,
                            truncate);
                if (retcode != TB_CHANGE_CONT ||
                    tbrelh->rh_state != TBRELH_INSERT) {
                    break;
                }
                /* FALLTHROUGH */

            case TBRELH_INSERT:
                retcode = relh_insertlocal_insert(
                            cd,
                            trans,
                            tbrelh,
                            ttype,
                            sap,
                            p_errh);
                if (retcode != TB_CHANGE_CONT ||
                    tbrelh->rh_state != TBRELH_FREE) {
                    break;
                }
                /* FALLTHROUGH */

            case TBRELH_FREE:
                retcode = relh_insertlocal_free(cd, tbrelh);
                break;

            case TBRELH_STMTCOMMIT:
                ss_dassert(tbrelh->rh_isstmtgroup);
                retcode = relh_insertlocal_stmtcommit(
                            cd,
                            trans,
                            tbrelh,
                            p_errh);
                break;

            case TBRELH_BEFORETRIGGER:
            case TBRELH_AFTERTRIGGER:
                retcode = relh_insertlocal_trigger(
                            cd,
                            trans,
                            tbrelh,
                            ttype,
                            p_errh);
                break;

#ifdef SS_SYNC_CHANGED
            case TBRELH_SYNCHISTORY:
                retcode = relh_insertlocal_synchistory(
                            cd,
                            trans,
                            tbrelh,
                            ttype,
                            p_errh);
                break;
#endif
            default:
                ss_error;
        }

        if (retcode == TB_CHANGE_CONT && !tbrelh->rh_tvalue_copied) {
            tbrelh->rh_ins_tval = rs_tval_copy(cd, ttype, tvalue);
            tbrelh->rh_tvalue_copied = TRUE;
        } else if (retcode == TB_CHANGE_ERROR) {
            relh_insertlocal_free(cd, tbrelh);
        }
        if (retcode != TB_CHANGE_CONT && tbrelh->rh_isstmtgroup) {
            tb_trans_stmt_begin(cd, trans);
            tb_trans_setstmtgroup(cd, trans, FALSE);
            tbrelh->rh_isstmtgroup = FALSE;
        }

        return(retcode);
}

/*##**********************************************************************\
 * 
 *              tb_relh_insert
 * 
 * Inserts a tuple into the relation 
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              transaction handle
 *
 *      relh - in, use
 *              relation handle
 *
 *      ttype - in, use
 *              tuple type for the tuple to be inserted
 *
 *      tvalue - in, use
 *              attribute values (= tuple) for insert
 *
 *      selflags - in, use
 *              if non-NULL, a boolean array having TRUE for
 *          the attributes that contain data. Remaining attributes
 *          are set to (SQL)NULL
 *      
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value : 
 * 
 *      TB_CHANGE_SUCC  (1) if the operation is successful
 *      TB_CHANGE_CONT  (3) if the operation did not terminate
 *      TB_CHANGE_ERROR (0) in case of other errors
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint tb_relh_insert(cd, trans, tbrelh, ttype, tvalue, selflags, p_errh)
        void*       cd;
        tb_trans_t* trans;
        tb_relh_t*  tbrelh;
        rs_ttype_t* ttype;
        rs_tval_t*  tvalue;
        bool*       selflags;
        rs_err_t**  p_errh;
{
        uint retcode;

        CHK_TBRELH(tbrelh);
        SS_PUSHNAME("tb_relh_insert");

        retcode = relh_insertlocal(
                    cd,
                    trans,
                    tbrelh,
                    ttype,
                    tvalue,
                    selflags,
                    p_errh,
                    TRUE,
                    FALSE);
        
        SS_POPNAME;
        
        return (retcode);
}

/*##**********************************************************************\
 * 
 *              tb_relh_insert_sql
 * 
 * Member of the SQL function block.
 * Inserts a tuple into the relation 
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              transaction handle
 *
 *      relh - in, use
 *              relation handle
 *
 *      ttype - in, use
 *              tuple type for the tuple to be inserted
 *
 *      tvalue - in, use
 *              attribute values (= tuple) for insert
 *
 *      selflags - in, use
 *              if non-NULL, a boolean array having TRUE for
 *          the attributes that contain data. Remaining attributes
 *          are set to (SQL)NULL
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
 *          TRUE if operation was successful
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool tb_relh_insert_sql(
        void*       cd,
        tb_trans_t* trans,
        tb_relh_t*  tbrel,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        bool*       selflags,
        void**      cont,
        rs_err_t**  errhandle)
{
        uint ret;
        bool succ = TRUE;

        ss_dprintf_1(("tb_relh_insert_sql:%s\n", rs_relh_name(cd, tbrel->rh_relh)));

        ret = tb_relh_insert( cd,
                              trans,
                              tbrel,
                              ttype,
                              tvalue,
                              selflags,
                              errhandle );

        switch( ret ){
            case TB_CHANGE_CONT :
                *cont = trans;
                break;
            case TB_CHANGE_SUCC :
                *cont = NULL;
                break;
            case TB_CHANGE_ERROR :
                /* fallthrough */
            default :
                *cont = NULL;
                succ = FALSE;
                break;
        };

        return( succ );
}

/*##**********************************************************************\
 * 
 *              tb_relh_sainsert
 * 
 * For SA interface.
 * Inserts a tuple into the relation 
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              transaction handle
 *
 *      relh - in, use
 *              relation handle
 *
 *      ttype - in, use
 *              tuple type for the tuple to be inserted
 *
 *      tvalue - in, use
 *              attribute values (= tuple) for insert
 *
 *      selflags - in, use
 *              if non-NULL, a boolean array having TRUE for
 *          the attributes that contain data. Remaining attributes
 *          are set to (SQL)NULL
 *
 *      truncate - in
 *          TRUE if truncation to max. length allowed FALSE when not
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value : 
 * 
 *      TB_CHANGE_SUCC  (1) if the operation is successful
 *      TB_CHANGE_CONT  (3) if the operation did not terminate
 *      TB_CHANGE_ERROR (0) in case of other errors
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint tb_relh_sainsert(cd, trans, tbrelh, ttype, tvalue, selflags, truncate, p_errh)
        void*       cd;
        tb_trans_t* trans;
        tb_relh_t*  tbrelh;
        rs_ttype_t* ttype;
        rs_tval_t*  tvalue;
        bool*       selflags;
        bool        truncate;
        rs_err_t**  p_errh;
{
        uint retcode;

        CHK_TBRELH(tbrelh);
        SS_PUSHNAME("tb_relh_insert");

        retcode = relh_insertlocal(
                    cd,
                    trans,
                    tbrelh,
                    ttype,
                    tvalue,
                    selflags,
                    p_errh,
                    truncate,
                    TRUE);

        SS_POPNAME;

        return (retcode);
}

/*##**********************************************************************\
 * 
 *              tb_relh_createjoin
 * 
 * Member of the SQL function block.
 * Tries to create a relation handle that represents a cartesian product
 * of two or more relations.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relnames - in, use
 *              NULL-terminated array of relation names
 *
 *      authids - in, use
 *              Array of authorization ids (contains as many
 *          items as the relnames array, NULL item
 *          means missing authorization id)
 *
 *      extrainfos - in, use
 *              Array of extra information strings (contains
 *          as many items as the relnames array, NULL
 *          item means missing extra information string)
 *
 *      eqjoins - in, use
 *              NULL-terminated array of equality constraints
 *          to the join key. NULL value means no
 *          constraints
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 * 
 * Return value - give : 
 * 
 *      !NULL, a pointer into a newly allocated relation handle 
 *       NULL, error (e.g. the combination of relations is not available)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
tb_relh_t* tb_relh_createjoin(cd, relnames, authids, catalogs, extrainfos,
                              eqjoins, p_errh)
        void*       cd;
        sqllist_t*  relnames;
        sqllist_t*  authids;
        sqllist_t*  extrainfos;
        sqllist_t*  catalogs __attribute__ ((unused));
        sqllist_t*  eqjoins;
        rs_err_t**  p_errh;
{
        SS_NOTUSED(cd);
        SS_NOTUSED(relnames);
        SS_NOTUSED(authids);
        SS_NOTUSED(extrainfos);
        SS_NOTUSED(eqjoins);
        ss_dprintf_1(("tb_relh_createjoin\n"));

        rs_error_create(p_errh, E_JOINRELNOSUP);
        return(NULL);
}

/*##**********************************************************************\
 * 
 *              tb_relh_canreverse
 * 
 * Member of the SQL function block.
 * Checks if reversing is possible with a specified relation handle.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      tbrelh - in, use
 *              relation handle
 *
 * Return value : 
 * 
 *      SQL_REVERSE_YES
 *      SQL_REVERSE_NO
 *      SQL_REVERSE_POSSIBLY
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
sql_reverse_t tb_relh_canreverse(cd, tbrelh)
        void*      cd;
        tb_relh_t* tbrelh;
{
        CHK_TBRELH(tbrelh);
        ss_dprintf_1(("tb_relh_canreverse\n"));
        return(rs_relh_canreverse(cd, tbrelh->rh_relh));
}

/*##**********************************************************************\
 * 
 *              tb_relh_ttype
 * 
 * Member of the SQL function block.
 * Describes a relation handle by returning a tuple type that corresponds to
 * the relation.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      rel - in, use
 *              relation handle
 * 
 * Return value - ref : 
 * 
 *      Pointer to tuple type that corresponds to the relation
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ttype_t* tb_relh_ttype(cd, tbrelh)
        void*      cd __attribute__ ((unused));
        tb_relh_t* tbrelh;
{
        CHK_TBRELH(tbrelh);
        ss_dprintf_1(("tb_relh_ttype\n"));
        return(rs_relh_ttype(cd, tbrelh->rh_relh));
}

/*##**********************************************************************\
 * 
 *              tb_relh_checkstrings
 * 
 * Member of the SQL function block.
 * Returns an SQL condition string that represents the CHECK constraints
 * on the relation
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      tbrelh - in, use
 *              relation handle
 *
 * Return value - ref : 
 * 
 *      !NULL, pointer into the SQL condition string.
 *      NULL, if no constraint is specified
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char** tb_relh_checkstrings(cd, tbrelh, pnamearray)
        void*      cd;
        tb_relh_t* tbrelh;
        char***    pnamearray;     
{
        CHK_TBRELH(tbrelh);
        ss_dprintf_1(("tb_relh_checkstrings\n"));
        return(rs_relh_checkstrings(cd, tbrelh->rh_relh, pnamearray));
}

/*##**********************************************************************\
 * 
 *              tb_relh_entname
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      tbrelh - 
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
rs_entname_t* tb_relh_entname(
        void*      cd,
        tb_relh_t* tbrelh)
{
        CHK_TBRELH(tbrelh);
        return(rs_relh_entname(cd, tbrelh->rh_relh));
}

#ifdef SS_SYNC

/*##**********************************************************************\
 * 
 *              tb_synchist_nextver_to_tval
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
 *      ttype - 
 *              
 *              
 *      tvalue - 
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
#ifndef SS_MYSQL
bool tb_synchist_nextver_to_tval(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        va_t*       synctuplevers,
        rs_err_t**  p_errh)
{
        rs_atype_t* atype;
        rs_aval_t* aval;
        int index;
        bool succp = TRUE;

        ss_output_4(rs_tval_print(cd, ttype, tvalue));
        SS_PUSHNAME("tb_synchist_nextver_to_tval");

        index = rs_ttype_anobyname(cd, ttype, RS_ANAME_SYNCTUPLEVERS);
        ss_dassert(index >= 0);
        aval = rs_tval_aval(cd, ttype, tvalue, index);
        atype = rs_ttype_atype(cd, ttype, index);

        if (synctuplevers == NULL) {
            succp = tb_sync_getnewsynctupleversion(
                        cd,
                        trans,
                        atype,
                        aval,
                        p_errh);
        } else {
            rs_aval_setva(cd, atype, aval, synctuplevers);
            succp = TRUE;
        }

        /* this may fail: rs_aval_getlong
        ss_bprintf_3(("tb_synchist_nextver_to_tval: next versid=%ld\n",
                       rs_aval_getlong(cd, atype, aval)));
        */

        ss_output_4(rs_tval_print(cd, ttype, tvalue));
        SS_POPNAME;

        return (succp);
}
#endif /* !SS_MYSQL */

bool tb_synchist_ispubltuple_to_tval(
        rs_sysi_t*  cd,
        tb_trans_t* trans __attribute__ ((unused)),
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue,
        bool        ispubltuple,
        bool        isupdate,
        rs_err_t**  p_errh)
{
        rs_atype_t* atype;
        rs_aval_t* aval;
        int index;
        long lvalue;

        ss_output_4(rs_tval_print(cd, ttype, tvalue));
        SS_PUSHNAME("tb_synchist_ispubltuple_to_tval");

        if (ispubltuple) {
            lvalue = 1L; /* both Voyager and 1.1 uses 1 for TRUE */
        } else {
            /* lvalue = 0L; */
            lvalue = rs_sysi_getlocalsyncid(cd);
        }

        if (isupdate) {
            ss_dassert(2 == RS_AVAL_ROWFLAG_UPDATE);
            lvalue = lvalue | RS_AVAL_ROWFLAG_UPDATE;
        }

        index = rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_SYNC_ISPUBLTUPLE);
        ss_dassert(index >= 0);
        if (index < 0) {
            rs_error_create(p_errh, E_NOINSPSEUDOCOL);
            SS_POPNAME;
            return (FALSE);
        }
        aval = rs_tval_aval(cd, ttype, tvalue, index);
        atype = rs_ttype_atype(cd, ttype, index);

        {
            ss_debug(RS_AVALRET_T retc =)
            rs_aval_setlong_ext(cd, atype, aval, lvalue, NULL);
            ss_dassert(retc != RSAVR_FAILURE);
        }

        ss_output_4(rs_tval_print(cd, ttype, tvalue));
        SS_POPNAME;
        return (TRUE);
}

bool tb_synchist_is_tval_publtuple(
        rs_sysi_t*  cd,
        tb_trans_t* trans __attribute__ ((unused)),
        rs_ttype_t* ttype,
        rs_tval_t*  tvalue)
{
        rs_atype_t* atype;
        rs_aval_t* aval;
        rs_ano_t ano;
        long lvalue;

        ano = rs_ttype_anobyname(cd, ttype, (char *)RS_ANAME_SYNC_ISPUBLTUPLE);
        ss_dassert((int)ano >= 0);
        atype = rs_ttype_atype(cd, ttype, ano);
        aval = rs_tval_aval(cd, ttype, tvalue, ano);

        /* VOYAGER - begin */
        if (rs_aval_isnull(cd, atype, aval)) {
            /*  see. rs0pla....INCLUDENULLS...dbe6bsea and
             *  cur_pseudoattr_setaval in tab0relc
             */
            return(FALSE);
        }
        /* VOYAGER - end */

        /* VOYAGER_SYNCHIST */
        lvalue = rs_aval_getlong(cd, atype, aval);

        ss_dassert(2 == RS_AVAL_ROWFLAG_UPDATE);
        lvalue &= ~(RS_AVAL_ROWFLAG_UPDATE);

        if (lvalue == rs_sysi_getlocalsyncid(cd)) {
            return(FALSE);
        }
        /* to change this see tab0relc cur_pseudoattr_setaval
         * if (lvalue == SS_INT4_MIN) {
         *    return(FALSE);
         * }
         */

        return(TRUE);
}



/*##**********************************************************************\
 * 
 *              tb_relh_syncinsert_init
 * 
 * Row insert routine for sync version. No constraints or access rights
 * are checked.
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
 *      relh - 
 *              
 *              
 *      ttype - 
 *              
 *              
 *      tval - 
 *              
 *              
 *      vers_tval - 
 *              
 *              
 *      waspubltuple - 
 *              
 *              
 *      master_addsynchistory - 
 *              If not TRUE, row is not added to synchisotry in master node.
 *              
 *      p_retcode - 
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
#ifndef SS_MYSQL
tb_syncinserthistory_t* tb_relh_syncinsert_init(
        void*       cd,
        tb_trans_t* trans,
        rs_relh_t*  relh,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        rs_tval_t*  vers_tval,
        bool        waspubltuple,
        bool        isupdate,
        bool        master_addsynchistory,
        bool        ismaster,
        bool        isreplica,
        uint*       p_retcode,
        rs_err_t**  p_errh)
{
        tb_syncinserthistory_t* sih = NULL;
        rs_ano_t ano;
        rs_ano_t ins_ano;
        bool ispubltuple;
        rs_atype_t* atype;
        rs_aval_t* aval;
        rs_ttype_t* ins_ttype;
        rs_tval_t*  ins_tval;
        int nattrs;
        bool succp;
        bool replica_addsynchistory;

        ss_dprintf_3(("tb_relh_syncinsert_init:%s\n", rs_relh_name(cd, relh)));
        SS_PUSHNAME("tb_relh_syncinsert");
        *p_retcode = TB_CHANGE_SUCC;

        /*
        ss_dprintf_4(("ins ttype:\n"));
        ss_output_4(rs_ttype_print(cd, ttype));
        ss_dprintf_4(("relh ttype:\n"));
        ss_output_4(rs_ttype_print(cd, rs_relh_ttype(cd, relh)));
        */

        ss_dassert(isreplica || ismaster);
        ss_dassert(vers_tval == NULL);

        ispubltuple = waspubltuple;
        replica_addsynchistory = isreplica && ispubltuple;
        master_addsynchistory = ismaster && master_addsynchistory;

        if (!master_addsynchistory && !replica_addsynchistory) {
            ss_dprintf_3(("tb_relh_syncinsert_init:not inserted (ispubltuple=%d)\n", (int)ispubltuple));
            SS_POPNAME;
            *p_retcode = TB_CHANGE_SUCC;
            return(NULL);
        }

        /* we can insert tentative tuples to history */
        ins_ttype = rs_relh_ttype(cd, relh);
        ins_tval = rs_tval_create(cd, ins_ttype);

        if (ismaster || waspubltuple) {
            succp = tb_synchist_nextver_to_tval(cd, trans, ins_ttype, ins_tval, NULL, p_errh);
            if (!succp) {
                *p_retcode = TB_CHANGE_ERROR;
                rs_tval_free(cd, ins_ttype, ins_tval);
                SS_POPNAME;
                return(NULL);
            }
        }

#ifdef SS_DEBUG
        /* check that sync tuple version is not null */
        ano = rs_ttype_anobyname(cd, ttype, RS_ANAME_SYNCTUPLEVERS);
        ss_dassert((int)ano >= 0);
        aval = rs_tval_aval(cd, ins_ttype, ins_tval, ano);
        ss_dassert(!rs_aval_isnull(cd, rs_ttype_atype(cd, ttype, ano), aval));

#endif /* SS_DEBUG */

        succp = tb_synchist_ispubltuple_to_tval(cd, trans, ins_ttype, ins_tval, ispubltuple, isupdate, p_errh);
        if (!succp) {
            *p_retcode = TB_CHANGE_ERROR;
            rs_tval_free(cd, ins_ttype, ins_tval);
            SS_POPNAME;
            return(NULL);
        }
        nattrs = rs_ttype_sql_nattrs(cd, ttype);
        for (ano = 0; ano < nattrs; ano++) {
            atype = rs_ttype_sql_atype(cd, ttype, ano);
            if (!rs_atype_pseudo(cd, atype)) {
                aval = rs_tval_sql_aval(cd, ttype, tval, ano);
                rs_tval_sql_setaval(cd, ins_ttype, ins_tval, ano, aval);
            }
        }

        sih = SSMEM_NEW(tb_syncinserthistory_t);
        sih->sih_cd = cd;
        sih->sih_trans = trans;
        sih->sih_relh = relh;
        sih->sih_ins_ttype = ins_ttype;
        sih->sih_ins_tval = ins_tval;

        *p_retcode = TB_CHANGE_CONT;
        SS_POPNAME;
        return(sih);
}
#endif /* !SS_MYSQL */

/*##**********************************************************************\
 * 
 *              tb_relh_syncinsert_exec
 * 
 * 
 * 
 * Parameters : 
 * 
 *      sih - 
 *              
 *              
 *      cd - 
 *              
 *              
 *      trans - 
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
#ifndef SS_MYSQL
uint tb_relh_syncinsert_exec(
        tb_syncinserthistory_t* sih,
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_err_t** p_errh)
{
        uint retcode;
        retcode = relh_dbeinsert(
                    sih->sih_cd,
                    sih->sih_trans,
                    sih->sih_relh,
                    sih->sih_ins_ttype,
                    sih->sih_ins_tval,
                    FALSE,
                    p_errh);

        ss_bassert(retcode == TB_CHANGE_SUCC ||
                   retcode == TB_CHANGE_ERROR ||
                   retcode == TB_CHANGE_CONT);

        return(retcode);
}
#endif /* !SS_MYSQL */

#ifndef SS_MYSQL
/*##**********************************************************************\
 * 
 *              tb_relh_syncinsert_done
 * 
 * 
 * 
 * Parameters : 
 * 
 *      sih - 
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
void tb_relh_syncinsert_done(
        tb_syncinserthistory_t* sih)
{
        rs_tval_free(sih->sih_cd, sih->sih_ins_ttype, sih->sih_ins_tval);
        SsMemFree(sih);
}
#endif /* !SS_MYSQL */

/*##**********************************************************************\
 * 
 *              tb_relh_getsynchistrelinfo
 * 
 * Return information about sync history table, if available
 * 
 * Parameters : 
 * 
 *      p_name - out, ref
 *              
 *              
 *      p_schema - out, ref
 *              
 *              
 *      p_catalog - out, ref
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
bool tb_relh_getsynchistrelinfo(
        void*       cd,
        tb_relh_t*  tbrelh,             /* base relhandle */
        char**      p_name,
        char**      p_schema,
        char**      p_catalog)
{
        rs_relh_t* synchist_relh; /* sync history relh */
        CHK_TBRELH(tbrelh);
        ss_dprintf_3(("%s: tb_relh_getsyncrelh.\n", __FILE__));
        if (!rs_relh_issync(cd, tbrelh->rh_relh)) {
            ss_dprintf_2(("%s: tb_relh_getsyncrelh, not sync.\n", __FILE__));
                return(FALSE);
        }
        synchist_relh = rs_relh_getsyncrelh(cd, tbrelh->rh_relh);
        if (synchist_relh == NULL) {
                /* table does not contain history */
                return(FALSE);
        }
        *p_name = rs_relh_name(cd, synchist_relh);
        *p_schema = rs_relh_schema(cd, synchist_relh);
        *p_catalog = rs_relh_catalog(cd, synchist_relh);

        return(TRUE);
}       

/*##**********************************************************************\
 * 
 *              tb_relh_tabfunblock
 * 
 *     Return the function block for a relation handle.
 * 
 * Parameters : 
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
tb_tint_t* tb_relh_tabfunblock( 
        void*       cd,
        tb_relh_t*  tbrelh )
{
        return(tbrelh->rh_tabfunb);
}
#endif /* !SS_MYSQL */

#endif /* SS_SYNC */
