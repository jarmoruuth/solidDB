/*************************************************************************\
**  source       * dbe0tref.c
**  directory    * dbe
**  description  * Tuple reference data structure.
**               * Tuple reference is used to
**               * - search clustering key values
**               * - create ROWID column values
**               * - generate lock name for lock manager
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

This module implements a tuple reference object. The tuple reference is
used to uniquely identify a tuple in the database. Relation cursor
functions return a tuple reference for every tuple they return. Tuple
reference is used in update and delete operations to identify the updated
or deleted tuple.

Tuple reference consists of the reference attributes of the tuple to the
clustering key and the tuple transaction id.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

v-attribute     uti0va.c
v-tuple         uti0vtpl.c

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define DBE0TREF_C

#include <ssstddef.h>
#include <ssdebug.h>
#include <ssc.h>
#include <ssmem.h>

#include <su0crc32.h>

#include <rs0types.h>
#include <rs0pla.h>
#include <rs0relh.h>
#include <rs0key.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0sysi.h>

#include <uti0va.h>
#include <uti0vtpl.h>

#include "dbe0type.h"
#include "dbe0tref.h"

/*##**********************************************************************\
 * 
 *		dbe_tref_initrecov
 * 
 * 
 * 
 * Parameters : 
 * 
 *	tref - use
 *		
 *		
 *	recovvtpl - in, take
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
void dbe_tref_initrecov(dbe_tref_t* tref, vtpl_t* recovvtpl)
{
        va_t* va;
        ss_dassert(recovvtpl != NULL);
        ss_dassert(vtpl_vacount(recovvtpl) == 2);

        tref->tr_recovvtpl = recovvtpl;

        va = VTPL_GETVA_AT0(recovvtpl);
        tref->tr_trxid = DBE_TRXID_INIT(va_getlong(va));
        va = VTPL_SKIPVA(va);
        tref->tr_vtpl = (vtpl_t*)va;
        tref->tr_readlevel = DBE_TRXNUM_NULL;
        tref->tr_flags = 0;
}

/*##**********************************************************************\
 * 
 *		dbe_tref_done
 * 
 * Releases a tuple reference. After this call the tuple reference
 * is invalid.
 * 
 * Parameters : 
 * 
 *	tref - in, take
 *		Tuple reference.
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_tref_done(
        rs_sysi_t*  cd,
        dbe_tref_t* tref)
{
        ss_dassert(cd != NULL);
        ss_dassert(tref != NULL);

        dbe_tref_freedata(cd, tref);
        SsMemFree(tref);
}

/*##**********************************************************************\
 * 
 *		dbe_tref_copy
 * 
 * Makes a copy of tuple reference.
 * 
 * Parameters : 
 * 
 *	tref - 
 *		
 *		
 * Return value - give : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_tref_t* dbe_tref_copy(
        rs_sysi_t*      cd __attribute__ ((unused)),
        dbe_tref_t*     tref)
{
        dbe_tref_t* copy_tref;

#ifndef SS_MYSQL
        if (SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_MME)) {
            copy_tref = dbe_tref_init();
            dbe_tref_setmme(copy_tref);
            if (tref->tr_rval == (mme_rval_t *) tref->tr_rval_.buf) {
                memcpy(copy_tref->tr_rval_.buf,
                       tref->tr_rval_.buf,
                       DBE_TREF_BUFSIZE);
                copy_tref->tr_rval = (mme_rval_t *) copy_tref->tr_rval_.buf;
            } else {
                copy_tref->tr_rval = mme_rval_init_from_rval(
                        cd, NULL, NULL, tref->tr_rval, NULL, NULL,
                        NULL, FALSE, FALSE,
                        MME_RVAL_KEYREF);
            }

            return copy_tref;
        }
#endif

        ss_dassert(tref != NULL);
        ss_dassert(tref->tr_vtpl != NULL);

        copy_tref = SsMemAlloc(sizeof(dbe_tref_t));

        copy_tref->tr_trxid = tref->tr_trxid;
        copy_tref->tr_readlevel = tref->tr_readlevel;
        copy_tref->tr_recovvtpl = NULL;
        copy_tref->tr_vtpl = NULL;
        copy_tref->tr_flags = tref->tr_flags;

#ifndef SS_MYSQL
        copy_tref->tr_rval = NULL;
#endif
        copy_tref->tr_stmtid = DBE_TRXID_NULL;

        if (SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_ISLOCKNAME)) {
            copy_tref->tr_lockname = tref->tr_lockname;
        }
        if (tref->tr_recovvtpl == NULL) {
            dynvtpl_setvtpl(&copy_tref->tr_vtpl, tref->tr_vtpl);
        } else {
            dynvtpl_setvtpl(&copy_tref->tr_recovvtpl, tref->tr_recovvtpl);
            copy_tref->tr_vtpl = (vtpl_t*)vtpl_getva_at(copy_tref->tr_recovvtpl, 1);
            ss_dassert(
                DBE_TRXID_EQUAL(
                    copy_tref->tr_trxid,
                    DBE_TRXID_INIT(
                        va_getlong(
                            vtpl_getva_at(
                                tref->tr_recovvtpl,
                                0
                            )
                        )
                    )
                ));
        }

        return(copy_tref);
}

#ifndef SS_NOLOGGING
/*##**********************************************************************\
 * 
 *		dbe_tref_getrecovvtpl
 * 
 * 
 * 
 * Parameters : 
 * 
 *	tref - 
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
vtpl_t* dbe_tref_getrecovvtpl(
        dbe_tref_t* tref)
{
        ss_dassert(tref != NULL);
        ss_dassert(tref->tr_vtpl != NULL);

        if (tref->tr_recovvtpl == NULL) {
            va_t trxid_va;

            va_setlong(&trxid_va, DBE_TRXID_GETLONG(tref->tr_trxid));
            dynvtpl_setvtpl(&tref->tr_recovvtpl, VTPL_EMPTY);
            dynvtpl_appva(&tref->tr_recovvtpl, &trxid_va);
            dynvtpl_appva(&tref->tr_recovvtpl, (va_t*)tref->tr_vtpl);
            dynvtpl_free(&tref->tr_vtpl);
            tref->tr_vtpl = (vtpl_t*)vtpl_getva_at(tref->tr_recovvtpl, 1);
        }

        return(tref->tr_recovvtpl);
}
#endif /* SS_NOLOGGING */

/*##**********************************************************************\
 * 
 *		dbe_tref_getvtpl
 * 
 * 
 * 
 * Parameters : 
 * 
 *	tref - 
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
vtpl_t* dbe_tref_getvtpl(
        dbe_tref_t* tref)
{
        ss_dassert(tref != NULL);
        ss_dassert(tref->tr_vtpl != NULL);

        return(tref->tr_vtpl);
}

void* dbe_tref_getrefrvaldata(
        dbe_tref_t*     tref,
        size_t*         data_len)
{
#ifdef SS_MYSQL_AC
        void* refdata;

        ss_dassert(tref != NULL);
        ss_dassert(SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_MME));

        refdata = mme_rval_getdata(tref->tr_rval, data_len);

        return(refdata);
#else /* SS_MYSQL_AC */
        return(NULL);
#endif /* SS_MYSQL_AC */
}

void dbe_tref_projectrvaltotval(
        rs_sysi_t*      cd,
        ss_byte_t*      rvalrefdata,
        size_t          rvalrefdata_len,
        rs_ttype_t*     ttype,
        rs_tval_t*      tval,
        rs_key_t*       key)
{
#ifdef SS_MYSQL_AC
        mme_rval_t* rval;

        rval = mme_rval_init_from_diskbuf(
                    cd, 
                    (ss_byte_t*)rvalrefdata, 
                    rvalrefdata_len, 
                    NULL, 
                    NULL, 
                    MME_RVAL_KEYREF);

        mme_rval_projecttotval(
            cd,
            ttype,
            tval,
            key,
            NULL,   /* sellist */
            rval,
            MME_RVAL_NORMAL);

        mme_rval_done(cd, rval, MME_RVAL_KEYREF);
#endif /* SS_MYSQL_AC */
}

/*##**********************************************************************\
 * 
 *		dbe_tref_gettrxid
 * 
 * 
 * 
 * Parameters : 
 * 
 *	tref - 
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
dbe_trxid_t dbe_tref_gettrxid(
        dbe_tref_t* tref)
{
        ss_dassert(tref != NULL);
        ss_dassert(tref->tr_vtpl != NULL);

        return(tref->tr_trxid);
}

/*##**********************************************************************\
 * 
 *		dbe_tref_getreadlevel
 * 
 * 
 * 
 * Parameters : 
 * 
 *	tref - 
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
dbe_trxnum_t dbe_tref_getreadlevel(
        dbe_tref_t* tref)
{
        ss_dassert(tref != NULL);
        ss_dassert(tref->tr_vtpl != NULL);

        return(tref->tr_readlevel);
}

/*##**********************************************************************\
 * 
 *		dbe_tref_setreadlevel
 * 
 * 
 * 
 * Parameters : 
 * 
 *	tref - 
 *		
 *		
 *	readlevel - 
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
void dbe_tref_setreadlevel(
        dbe_tref_t* tref,
        dbe_trxnum_t readlevel)
{
        ss_dassert(tref != NULL);
        ss_dassert(tref->tr_vtpl != NULL);

        tref->tr_readlevel = readlevel;
}

/*#***********************************************************************\
 * 
 *		tref_buildclustkeydynvtpl
 * 
 * Builds tuple refernce v-tuple from a clustering key.
 * 
 * Parameters : 
 * 
 *	cd - in
 *		Client data.
 *		
 *	clustkey - in
 *		Clustering key.
 *		
 *	clustkey_vtpl - in
 *		Clustering key v-tuple
 *		
 *	rowidp - in
 *		If TRUE, builds a rowid v-tuple (it does not contain tuple
 *          version).
 *		
 *	p_dvtpl - out, give
 *		V-tuple is returned here.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void tref_buildclustkeydynvtpl(
        void* cd,
        rs_key_t* clustkey,
        vtpl_t* clustkey_vtpl,
        bool rowidp,
        dynvtpl_t* p_dvtpl)
{
        uint i;
        uint nrefparts;
        va_t* va;

        ss_assert(rs_key_isclustering(cd, clustkey));

        nrefparts = rs_key_nrefparts(cd, clustkey);

        dynvtpl_setvtpl(p_dvtpl, VTPL_EMPTY);

        /* Add all reference key parts. */
        va = VTPL_GETVA_AT0(clustkey_vtpl);
        for (i = 0; i < nrefparts; i++, va = VTPL_SKIPVA(va)) {
            dynvtpl_appva(p_dvtpl, va);
        }

        if (!rowidp && rs_keyp_parttype(cd, clustkey, i) == RSAT_TUPLE_VERSION) {
            /* Add tuple version.
             */
            dynvtpl_appva(p_dvtpl, va);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_tref_buildclustkeytref
 * 
 * Builds a tuple reference from the clustering key.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *          Client data.
 * 
 *	tref - out, use
 *          The new tuple reference is stored into here.
 * 
 *	clustkey - in, use
 *          Clustering key.
 * 
 *	clustkey_vtpl - in, use
 *          Clustering key v-tuple.
 * 
 *	trxid - in, use
 *          Tuple transaction id.
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_tref_buildclustkeytref(
        void* cd,
        dbe_tref_t* tref,
        rs_key_t* clustkey,
        vtpl_t* clustkey_vtpl,
        dbe_trxid_t trxid)
{
        dynvtpl_t dvtpl = NULL;

        ss_dprintf_1(("dbe_tref_buildclustkeytref\n"));
        ss_assert(rs_key_isclustering(cd, clustkey));

        dbe_tref_freedata(cd, tref);

        tref_buildclustkeydynvtpl(cd, clustkey, clustkey_vtpl, FALSE, &dvtpl);
        
        tref->tr_recovvtpl = NULL;
        tref->tr_vtpl = dvtpl;
        tref->tr_trxid = trxid;
        tref->tr_readlevel = DBE_TRXNUM_NULL;
}

/*##**********************************************************************\
 * 
 *		dbe_tref_buildsearchtref
 * 
 * Builds a tuple reference from a v-tuple and trxid and stores it into
 * tref.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		Client data.
 *		
 *	tref - out, use
 *		Tuple reference that is created and returned by this
 *		function.
 *		
 *	plan - in, use
 *		Search plan, contains e.g. list of reference attributes.
 *		
 *	vamap - in, use
 *		Va-map of the v-tuple from where the tuple reference is build.
 *		
 *	trxid - in, use
 *		Transaction id of vtpl. Needed to build the tuple
 *		reference.
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_tref_buildsearchtref(
        void* cd,
        dbe_tref_t* tref,
        rs_pla_t* plan,
        vtpl_vamap_t* vamap,
        dbe_trxid_t trxid)
{
        rs_key_t* key;
        su_list_t* refattrs;

        ss_dprintf_1(("dbe_tref_buildsearchtref\n"));

        key = rs_pla_getkey(cd, plan);
        refattrs = rs_pla_get_tuple_reference(cd, plan);
        ss_dassert(su_list_length(refattrs) > 0);

        dbe_tref_buildsearchtref_ex(cd, tref, key, rs_pla_getrelh(cd, plan), refattrs, vamap, trxid);
}

/*##**********************************************************************\
 * 
 *		dbe_tref_buildsearchtref_ex
 * 
 * Builds a tuple reference from a v-tuple and trxid and stores it into
 * tref.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		Client data.
 *		
 *	tref - out, use
 *		Tuple reference that is created and returned by this
 *		function.
 *		
 *  key - in, use
 *		Search key
 *		
 *  refattrs - in, use
 *		List of reference attributes
 *		
 *	vamap - in, use
 *		Va-map of the v-tuple from where the tuple reference is build.
 *		
 *	trxid - in, use
 *		Transaction id of vtpl. Needed to build the tuple
 *		reference.
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_tref_buildsearchtref_ex(
        void* cd,
        dbe_tref_t* tref,
        rs_key_t* key,
        rs_relh_t* relh,
        su_list_t* refattrs,
        vtpl_vamap_t* vamap,
        dbe_trxid_t trxid)
{
        su_list_node_t* lnode;
        rs_pla_ref_t* ref;
        rs_ttype_t* ttype = NULL;

        ss_dprintf_1(("dbe_tref_buildsearchtref_ex\n"));

        ss_dassert(su_list_length(refattrs) > 0);

        dbe_tref_freedata(cd, tref);
        
        dynvtpl_setvtpl(&tref->tr_vtpl, VTPL_EMPTY);

        su_list_do_get(refattrs, lnode, ref) {
            va_t* va;

            if (rs_pla_ref_isconstant(cd, ref)) {
                ss_dprintf_4(("dbe_tref_buildsearchtref_ex:kpindex=%d, const\n", rs_pla_ref_kpindex(cd, ref)));
                dynvtpl_appva(&tref->tr_vtpl, rs_pla_ref_value(cd, ref));
            } else {
                int kpindex;
                kpindex = rs_pla_ref_kpindex(cd, ref);
                va = vtpl_vamap_getva_at(vamap, kpindex);
                if (rs_keyp_isascending(cd, key, kpindex)) {
                    ss_dprintf_4(("dbe_tref_buildsearchtref_ex:kpindex=%d\n", kpindex));
                    dynvtpl_appva(&tref->tr_vtpl, va);
                } else {
                    /* Convert descending attribute to ascending format
                     * before adding to tuple reference.
                     */
                    int ano;
                    rs_atype_t* atype;
                    rs_aval_t* desc_aval;
                    ss_dprintf_4(("dbe_tref_buildsearchtref_ex:kpindex=%d desc\n", kpindex));
                    if (ttype == NULL) {
                        ttype = rs_relh_ttype(cd, relh);
                    }
                    ano = rs_keyp_ano(cd, key, kpindex);
                    atype = rs_ttype_atype(cd, ttype, ano);
                    desc_aval = rs_aval_create(cd, atype);
                    rs_aval_setva(cd, atype, desc_aval, va);
                    rs_aval_setdesc(cd, atype, desc_aval);
                    rs_aval_desctoasc(cd, atype, desc_aval);
                    va = rs_aval_va(cd, atype, desc_aval);
                    dynvtpl_appva(&tref->tr_vtpl, va);
                    rs_aval_free(cd, atype, desc_aval);
                }
            }
        }

        tref->tr_recovvtpl = NULL;
        tref->tr_trxid = trxid;
        tref->tr_readlevel = DBE_TRXNUM_NULL;
}

#ifdef DBE_REPLICATION

/*##**********************************************************************\
 * 
 *		dbe_tref_buildrepdeletetref
 * 
 * Builds a tref object from v-tuple format tuple reference, used in
 * dbe delete routine.
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	tref - use
 *		
 *		
 *	relh - in
 *		
 *		
 *	del_vtpl - in
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
void dbe_tref_buildrepdeletetref(
        void* cd,
        dbe_tref_t* tref,
        rs_relh_t* relh,
        vtpl_t* del_vtpl)
{
        rs_key_t* key;
        uint i;
        uint nrefparts;
        va_t* va;
        va_t* add_va;

        ss_dprintf_1(("dbe_tref_buildrepdeletetref\n"));

        key = rs_relh_clusterkey(cd, relh);
        nrefparts = rs_key_nrefparts(cd, key);

        dbe_tref_freedata(cd, tref);
        
        dynvtpl_setvtpl(&tref->tr_vtpl, VTPL_EMPTY);

        va = VTPL_GETVA_AT0(del_vtpl);
        for (i = 0; i < nrefparts; i++, va = VTPL_SKIPVA(va)) {
            ss_dassert(rs_keyp_parttype(cd, key, i) != RSAT_TUPLE_VERSION);
            if (rs_keyp_isconstvalue(cd, key, i)) {
                add_va = rs_keyp_constvalue(cd, key, i);
            } else {
                add_va = va;
            }
            dynvtpl_appva(&tref->tr_vtpl, add_va);
        }
}

#endif /* DBE_REPLICATION */

/*##**********************************************************************\
 * 
 *		dbe_tref_setrowiddata
 * 
 * Builds row id data from a tuple reference.
 * 
 * Parameters : 
 * 
 *	cd - 
 *		
 *		
 *	tref - 
 *
 *      ttype - in, use
 *		
 *	atype - 
 *		
 *		
 *	aval - 
 *		
 *		
 *	clustkey - 
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
void dbe_tref_setrowiddata(
        void*           cd,
        dbe_tref_t*     tref,
        rs_ttype_t*     ttype __attribute__ ((unused)),
        rs_atype_t*     atype,
        rs_aval_t*      aval,
        rs_key_t*       clustkey)
{
        bool succp;
        dynvtpl_t dvtpl = NULL;

        ss_dprintf_1(("dbe_tref_setrowiddata\n"));

        ss_assert(rs_key_isclustering(cd, clustkey));

#ifndef SS_MYSQL
        if (SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_MME)) {
            rs_ano_t        ano;
            rs_ano_t        kp;
            rs_ano_t        last_kp;
            rs_tval_t*      tval;
            va_t*           va;

            ss_dprintf_2(("this is an MME tref.\n"));

            ss_dassert(cd != NULL);
            ss_dassert(ttype != NULL);
            ss_dassert(tref->tr_rval != NULL);
            
            tval = mme_rval_projecttotval(cd, ttype, NULL, clustkey,
                                          NULL, tref->tr_rval,
                                          MME_RVAL_KEYREF);

            dynvtpl_setvtpl(&dvtpl, VTPL_EMPTY);
            last_kp = rs_key_lastordering(cd, clustkey);
            for (kp = 0; kp <= last_kp; kp++) {
                if (!rs_keyp_isconstvalue(cd, clustkey, kp)) {
                    ano = rs_keyp_ano(cd, clustkey, kp);
                    va = rs_tval_va(cd, ttype, tval, ano);
                } else {
                    va = VA_EMPTY;
                }
                dynvtpl_appva(&dvtpl, va);
            }
            rs_tval_free(cd, ttype, tval);
        } else
#endif
        {
            tref_buildclustkeydynvtpl(cd, clustkey, tref->tr_vtpl,
                                      TRUE, &dvtpl);
        }
        
#ifdef SS_UNICODE_DATA
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_BINARY);
#else /* SS_UNICODE_DATA */
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_CHAR);
#endif /* SS_UNICODE_DATA */

        /* Insert the whole vtpl (including length bytes) as the aval data.
         */
#ifdef SS_UNICODE_DATA
        succp = rs_aval_setbdata_ext(
                    cd,
                    atype,
                    aval,
                    (char*)dvtpl,
                    (uint)vtpl_grosslen(dvtpl),
                    NULL);
#else /* SS_UNICODE_DATA */
        succp = rs_aval_setdata(
                    cd,
                    atype,
                    aval,
                    (char*)dvtpl,
                    (uint)vtpl_grosslen(dvtpl));
#endif /* SS_UNICODE_DATA */
        ss_dassert(succp);

        dynvtpl_free(&dvtpl);
}

#ifndef SS_NOLOCKING

/*##**********************************************************************\
 * 
 *		dbe_tref_getlockname
 * 
 * Generates lock name from tuple reference.
 * 
 * Parameters : 
 * 
 *	cd - 
 *		
 *		
 *	tref - 
 *		
 *		
 *	clustkey - 
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
dbe_lockname_t dbe_tref_getlockname(
        void* cd,
        dbe_tref_t* tref,
        rs_key_t* clustkey)
{
        uint i;
        uint nrefparts;
        va_t* va;
        ss_uint4_t h;

        ss_assert(rs_key_isclustering(cd, clustkey));

        if (SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_ISLOCKNAME)) {
            return(tref->tr_lockname);
        }

        nrefparts = rs_key_nrefparts(cd, clustkey);

        /* Generate lock name from reference key parts. If physical
         * delete is not used, the tuple reference v-tuple may contain
         * tuple version, so we must scan reference key parts separately.
         * CRC-32 spreads the hash to 32 bits.
         */
        h = DBE_TREF_LOCKNAME_CRCINIT;
        va = VTPL_GETVA_AT0(tref->tr_vtpl);
        for (i = 0; i < nrefparts; i++, va = VTPL_SKIPVA(va)) {
            su_crc32((char*)va, VA_GROSSLEN(va), (void*)&h);
        }

        tref->tr_lockname = h;
        SU_BFLAG_SET(tref->tr_flags, DBE_TREF_ISLOCKNAME);
        SU_BFLAG_SET(tref->tr_flags, DBE_TREF_VALIDLOCKNAME);

        ss_dprintf_2(("dbe_tref_getlockname:name = %lu\n", h));

        return ((dbe_lockname_t)h);
}

/*##**********************************************************************\
 * 
 *		dbe_tref_isvalidlockname
 * 
 * Checks if tref has a valid lock name.
 * 
 * Parameters : 
 * 
 *	tref - 
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
bool dbe_tref_isvalidlockname(
        dbe_tref_t* tref)
{
        ss_dassert(tref != NULL);
        
        return(SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_VALIDLOCKNAME));
}

/*##**********************************************************************\
 * 
 *		dbe_tref_removevalidlockname
 * 
 * Removes a valid lock name from tref.
 * 
 * Parameters : 
 * 
 *	tref - 
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
void dbe_tref_removevalidlockname(
        dbe_tref_t* tref)
{
        ss_dassert(tref != NULL);
        
        SU_BFLAG_CLEAR(tref->tr_flags, DBE_TREF_VALIDLOCKNAME);
}

/*##**********************************************************************\
 * 
 *		dbe_tref_getcurrentlockname
 * 
 * Returns current lock name previously calculated by dbe_tref_getlockname.
 * 
 * Parameters : 
 * 
 *	tref - 
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
dbe_lockname_t dbe_tref_getcurrentlockname(
        dbe_tref_t* tref)
{
        ss_dassert(tref != NULL);
        ss_dassert(dbe_tref_isvalidlockname(tref));
        ss_dassert(SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_ISLOCKNAME));

        return(tref->tr_lockname);
}

#endif /* SS_NOLOCKING */

