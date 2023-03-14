/*************************************************************************\
**  source       * dbe0tref.h
**  directory    * dbe
**  description  * Tuple reference data structure.
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


#ifndef DBE0TREF_H
#define DBE0TREF_H

#include <uti0vtpl.h>

#include <su0list.h>
#include <su0bflag.h>

#include <rs0types.h>
#include <rs0pla.h>

#ifndef SS_MYSQL
#include <mme0rval.h>
#endif

#include "dbe0type.h"

#define DBE_TREF_BUFSIZE  64

#define DBE_TREF_LOCKNAME_CRCINIT 0x7F33B697L

#define DBE_TREF_ISLOCKNAME     SU_BFLAG_BIT(1)
#define DBE_TREF_VALIDLOCKNAME  SU_BFLAG_BIT(0)
#ifdef SS_MME
#define DBE_TREF_MME            SU_BFLAG_BIT(2)
#endif

typedef struct {
        dbe_trxid_t    tr_trxid;     /* Tuple transaction id. */
        dynvtpl_t      tr_vtpl;      /* Tuple reference v-tuple, ordering key
                                        parts from clutering key. Contains
                                        key id and primary key attributes. */
        dynvtpl_t      tr_recovvtpl; /* Whole tuple reference as a v-tuple.
                                        Contains trxid and reference v-tuple.
                                        These entries are stored into log. */
        dbe_trxnum_t   tr_readlevel; /* Read level used to read this tuple.
                                        Concurrency check is done based on
                                        this read level. */
        dbe_lockname_t tr_lockname;  /* Lock name for this tuple. */
        su_bflag_t     tr_flags;     /* Flagss */
#ifdef SS_MME
        mme_rval_t*     tr_rval;
        dbe_trxid_t     tr_stmtid;
        union {
                ss_byte_t buf[DBE_TREF_BUFSIZE];
                SS_ALIGN_T dummy4alignment; /* name tells it all :) */
        } tr_rval_;
#endif
} dbe_tref_t;

SS_INLINE void dbe_tref_initbuf(
        dbe_tref_t* tref);

SS_INLINE dbe_tref_t* dbe_tref_init(
        void);

void dbe_tref_initrecov(
        dbe_tref_t* tref,
        vtpl_t* recovvtpl);

void dbe_tref_done(
        rs_sysi_t*  cd,
        dbe_tref_t* tref);

SS_INLINE void dbe_tref_freedata(
        rs_sysi_t*  cd,
        dbe_tref_t* tref);

dbe_tref_t* dbe_tref_copy(
        rs_sysi_t*  cd,
        dbe_tref_t* tref);

vtpl_t* dbe_tref_getrecovvtpl(
        dbe_tref_t* tref);

vtpl_t* dbe_tref_getvtpl(
        dbe_tref_t* tref);

dbe_trxid_t dbe_tref_gettrxid(
        dbe_tref_t* tref);

dbe_trxnum_t dbe_tref_getreadlevel(
        dbe_tref_t* tref);

void dbe_tref_setreadlevel(
        dbe_tref_t* tref,
        dbe_trxnum_t readlevel);

void dbe_tref_buildclustkeytref(
        void* cd,
        dbe_tref_t* tref,
        rs_key_t* clustkey,
        vtpl_t* clustkey_vtpl,
        dbe_trxid_t trxid);

void dbe_tref_buildsearchtref(
        void* cd,
        dbe_tref_t* tref,
        rs_pla_t* plan,
        vtpl_vamap_t* vamap,
        dbe_trxid_t trxid);

void dbe_tref_buildsearchtref_ex(
        void* cd,
        dbe_tref_t* tref,
        rs_key_t* key,
        rs_relh_t* relh,
        su_list_t* refattrs,
        vtpl_vamap_t* vamap,
        dbe_trxid_t trxid);

void dbe_tref_buildrepdeletetref(
        void* cd,
        dbe_tref_t* tref,
        rs_relh_t* relh,
        vtpl_t* del_vtpl);

void dbe_tref_setrowiddata(
        void*           cd,
        dbe_tref_t*     tref,
        rs_ttype_t*     ttype,
        rs_atype_t*     atype,
        rs_aval_t*      aval,
        rs_key_t*       clustkey);

dbe_lockname_t dbe_tref_getlockname(
        void* cd,
        dbe_tref_t* tref,
        rs_key_t* clustkey);

bool dbe_tref_isvalidlockname(
        dbe_tref_t* tref);

void dbe_tref_removevalidlockname(
        dbe_tref_t* tref);

dbe_lockname_t dbe_tref_getcurrentlockname(
        dbe_tref_t* tref);

void* dbe_tref_getrefrvaldata(
        dbe_tref_t*     tref,
        size_t*         data_len);

void dbe_tref_projectrvaltotval(
        rs_sysi_t*      cd,
        ss_byte_t*      rvalrefdata,
        size_t          rvalrefdata_len,
        rs_ttype_t*     ttype,
        rs_tval_t*      tval,
        rs_key_t*       key);

#ifdef SS_MME

SS_INLINE void dbe_tref_setmme(
        dbe_tref_t*     tref);

SS_INLINE void dbe_tref_setrefrval(
        dbe_tref_t*     tref,
        rs_sysi_t*      cd,
        rs_ttype_t*     ttype,
        rs_tval_t*      tval,
        rs_key_t*       clustkey,
        dbe_trxid_t     stmtid);

SS_INLINE mme_rval_t* dbe_tref_getrefrval(
        dbe_tref_t*     tref);

SS_INLINE dbe_trxid_t dbe_tref_getstmtid(
        dbe_tref_t*     tref);

#endif /* SS_MME */

#if defined(DBE0TREF_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		dbe_tref_initbuf
 * 
 * Initializes a new, empty tuple reference.
 * 
 * Parameters :
 * 
 *      tref - use
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void dbe_tref_initbuf(dbe_tref_t* tref)
{
        tref->tr_trxid = DBE_TRXID_NULL;
        tref->tr_readlevel = DBE_TRXNUM_NULL;
        tref->tr_vtpl = NULL;
        tref->tr_recovvtpl = NULL;
        tref->tr_flags = 0;
        tref->tr_rval = NULL;
        tref->tr_stmtid = DBE_TRXID_NULL;
}

/*##**********************************************************************\
 * 
 *		dbe_tref_init
 * 
 * Allocates a new, empty tuple reference.
 * 
 * Parameters :
 * 
 * Return value - give : 
 * 
 *      Pointer to the empty tuple reference.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE dbe_tref_t* dbe_tref_init(void)
{
        dbe_tref_t* tref;

        tref = (dbe_tref_t*)SsMemAlloc(sizeof(dbe_tref_t));

        dbe_tref_initbuf(tref);

        return(tref);
}

/*##**********************************************************************\
 * 
 *		dbe_tref_freedata
 * 
 * Releases data from a tuple reference.
 * 
 * Parameters : 
 * 
 *	tref - in, use
 *		Tuple reference.
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void dbe_tref_freedata(
        rs_sysi_t*  cd __attribute__ ((unused)),
        dbe_tref_t* tref)
{
        ss_dassert(tref != NULL);
        if (tref->tr_recovvtpl != NULL) {
            dynvtpl_free(&tref->tr_recovvtpl);
            tref->tr_vtpl = NULL;
        } else if (tref->tr_vtpl != NULL) {
            dynvtpl_free(&tref->tr_vtpl);
        }

#ifndef SS_MYSQL
        if (SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_MME)) {
            ss_dassert(cd != NULL);
            ss_dassert(tref->tr_rval != NULL);
            if (tref->tr_rval != (mme_rval_t *) tref->tr_rval_.buf) {
                mme_rval_done(cd, tref->tr_rval, MME_RVAL_KEYREF);
            }
        }
#endif
        
        tref->tr_flags = 0;
}



#ifdef SS_MME
SS_INLINE void dbe_tref_setmme(
        dbe_tref_t*     tref)
{
        ss_dassert(tref != NULL);

        SU_BFLAG_SET(tref->tr_flags, DBE_TREF_MME);
}

SS_INLINE void dbe_tref_setrefrval(
        dbe_tref_t*     tref,
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_ttype_t*     ttype __attribute__ ((unused)),
        rs_tval_t*      tval __attribute__ ((unused)),
        rs_key_t*       clustkey __attribute__ ((unused)),
        dbe_trxid_t     stmtid)
{
        ss_dassert(tref != NULL);
        ss_dassert(SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_MME));
        ss_dassert(tref->tr_rval == NULL);

#ifndef SS_MYSQL
        {
            su_ret_t rc __attribute__ ((unused));
            tref->tr_rval = mme_rval_init_from_tval(
                    cd, ttype, tval, clustkey, NULL, NULL, FALSE,
                    MME_RVAL_KEYREF,
                    tref->tr_rval_.buf, DBE_TREF_BUFSIZE,
                    &rc);
            ss_rc_dassert(tref->tr_rval != NULL, rc);
        }
#endif
        tref->tr_stmtid = stmtid;
}

SS_INLINE mme_rval_t* dbe_tref_getrefrval(
        dbe_tref_t*     tref)
{
        ss_dassert(tref != NULL);
        ss_dassert(SU_BFLAG_TEST(tref->tr_flags, DBE_TREF_MME));

        return tref->tr_rval;
}

SS_INLINE dbe_trxid_t dbe_tref_getstmtid(
        dbe_tref_t*     tref)
{
        return tref->tr_stmtid;
}
#endif /* SS_MME */

#endif /* defined(DBE0TREF_C) || defined(SS_USE_INLINE) */

#endif /* DBE0TREF_H */
