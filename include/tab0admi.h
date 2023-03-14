/*************************************************************************\
**  source       * tab0admi.h
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


#ifndef TAB0ADMI_H
#define TAB0ADMI_H

#include <rs0types.h>
#include <rs0error.h>
#include <rs0sysi.h>
#include <rs0ttype.h>
#include <rs0entna.h>

#include "tab0type.h"
#include "tab0tran.h"
#include "tab0tli.h"

typedef struct {

    /* name of the foreign key (NULL if none) */
    char *name;

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
    /* constraint name on MySQL */
    char *mysqlname;
#endif

    /* number of fields in the foreign key */
    uint len;

    /* array (of length len) of indexes of fields in the foreign key */
    uint *fields;

    /* name of the referenced table */
    char *reftable;

    /* schema of the referenced table (NULL if none) */
    char *refschema;

    /* catalog of the referenced table (NULL if none) */
    char *refcatalog;

    /* names of the fields in the referenced table (NULL if referring to
       the primary key)
    */
    char **reffields;

    /* referential action in case of delete */
    sqlrefact_t delrefact;

    /* referential action in case of update */
    sqlrefact_t updrefact;

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED    
    /* is this forkey unresolved in time of table is being created */
    bool unresolved;
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */    
} tb_sqlforkey_t;

typedef struct {

    /* constraint name (NULL if none) */
    char *name;

    /* number of fields */
    uint len;

    /* array (of length len) of indexes of fields */
    uint *fields;

    /* array of index prefixes of fields */
    uint *prefixes;

} tb_sqlunique_t;

#ifndef SS_MYSQL
#include "tab0admibe.h"
#endif /* !SS_MYSQL */

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
                        sqlrefact_t updrefact);

void tb_forkey_done_buf(tb_sqlforkey_t* forkey);

#if defined(SS_DEBUG)
    void tb_forkey_print(void* cd,
                         const char* tablename,
                         rs_ttype_t* table_type,
                         const tb_sqlforkey_t* forkey);
#endif /*SS_DEBUG*/

bool tb_createrelation_ext(
        void*               cd,
        tb_trans_t*         trans,
        char*               relname,
        char*               authid,
        char*               catalog,
        char*               extrainfo,
        rs_ttype_t*         ttype,
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
        tb_dd_durability_t  durability,
        tb_relmode_t        add_relmode,
        rs_relh_t**         p_relh,
        rs_err_t**          err
);

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
        rs_ttype_t*     ttype,
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
        rs_err_t**      err
);

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
        rs_err_t**  errhandle
);

bool tb_admi_droprelation(
    void*       cd,
    tb_trans_t* trans,
    char*       relname,
    char*       authid,
        char*       catalog,
        char*       extrainfo,
        bool        cascade,
        bool        checkforkeys,
    rs_err_t**  p_errh);

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
    rs_err_t**  p_errh);

bool tb_droprelation(
        void*       cd,
        tb_trans_t* trans,
        char*       relname,
        char*       authid,
        char*       catalog,
        char*       extrainfo,
        bool        cascade,
        uint constraint_c,
        char** tablenames,
        char** schemas,
        char** catalogs,
        char** extrainfos,
        char** constraints,
        void**      cont,
        rs_err_t**  errhandle
);

bool tb_truncaterelation(
    void*       cd,
    tb_trans_t* trans,
        char*       tablename,
        char*       schema,
        char*       catalog,
        char*       extrainfo,
        void**      cont,
        rs_err_t**  p_errh
);

bool tb_createindex_ext(
    void*               cd,
    tb_trans_t*         trans,
    char*               indexname,
    char*               authid,
        char*               catalog,
    rs_relh_t*          relh,
    rs_ttype_t*         ttype,
    bool                unique,
    uint                attr_c,
    char**              attrs,
    bool*               desc,
#ifdef SS_COLLATION
    size_t*             prefixlengths,
#endif
    tb_dd_createrel_t   type,
    rs_err_t**          p_errh
);

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
#endif
        void**      cont,
        rs_err_t**  p_errh);

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
        uint        col_c,
        char**      columns,
        bool*       desc,
        void**      cont,
        rs_err_t**  err
);

bool tb_dropindex(
        void*       cd,
        tb_trans_t* trans,
        char*       indexname,
        char*       authid,
        char*       catalog,
        char*       extrainfo,
        void**      cont,
        rs_err_t**  errhandle
);

bool tb_dropindex_relh(
        void*       cd,
        tb_trans_t* trans,
        rs_relh_t*  relh,
        char*       indexname,
        char*       authid,
        char*       catalog,
        char*       extrainfo,
        void**      cont,
    rs_err_t**  p_errh);

bool tb_dropview(
        void*       cd,
        tb_trans_t* trans,
        char*       viewname,
        char*       authid,
        char*       catalog,
        char*       extrainfo,
        bool        cascade,
        void**      cont,
        rs_err_t**  errhandle
);

char* tb_authid(
        void*       cd,
        tb_trans_t* trans
);

bool tb_admi_grantcreatorpriv(
        rs_sysi_t* cd,
        TliConnectT* tcon,
        long id,
        long userid,
        tb_priv_t tbpriv,
        rs_err_t** p_errh);


bool tb_admi_checkschemaforcreateobj(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_entname_t* en,
        long* p_userid,
        rs_err_t** p_errh);

bool tb_admi_checkpriv(
        rs_sysi_t* cd,
        long id,
        char* objschema,
        tb_priv_t checkpriv,
        rs_err_t** p_errh);

bool tb_admi_droppriv(
        TliConnectT* tcon,
        long relid,
    rs_err_t** p_errh);


typedef enum {
        TB_ADMIN_CMD_OP_RESET   = 1,
        TB_ADMIN_CMD_OP_LAST
} tb_admin_cmd_op_t;


tb_admin_cmd_t* tb_admin_cmd_init(
        rs_sysi_t* cd);

void tb_admin_cmd_done(
        tb_admin_cmd_t* acmd);

void tb_admin_cmd_setcallback(
        tb_admin_cmd_t* acmd,
        void            (*cmd_callback)(tb_admin_cmd_op_t op, rs_sysi_t* cd, tb_admin_cmd_t* acmd, void* ctx1, void* ctx2),
        void* ctx1,
        void* ctx2);

void tb_admin_cmd_reset(
        rs_sysi_t* cd,
        tb_admin_cmd_t* acmd);

bool tb_addforkey(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        tb_relh_t*   tbrelh,
        char*        schema,
        char*        catalog,
        char*        tablename,
        tb_sqlforkey_t* forkey,
        void**       cont,
        rs_err_t**   errhandle);

bool tb_dropconstraint(
        rs_sysi_t*   cd,
        tb_trans_t*  trans,
        rs_relh_t*   relh,
        char*        schema,
        char*        catalog,
        char*        name,
        void**       cont,
        rs_err_t**   p_errh);

#endif /* TAB0ADMI_H */
