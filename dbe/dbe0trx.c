/*************************************************************************\
**  source       * dbe0trx.c
**  directory    * dbe
**  description  * Core transaction functions.
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

This module implements the database transaction object. There are
functions for starting and stopping a transaction. The transaction
object is bound to a database user.

The transaction object can be run in three different modes:

        Read-only mode.

        Read check mode, which ensures serializable transactions.
        Also phantom inserts and deletes are noticed.

        Write check mode, which checks only write operations. This
        mode prevents lost updates, but does not ensure serializable
        transactions.

Default transaction mode is write check mode.

The transaction object contains three different lists for validation
checks:

        key check list
        read check list
        write check list

The key check list contains a v-tuple from every key value insert to
a unique, primary or foreign key. The v-tuple contains those key
parts that must be checked. At validation time the transaction system
checks that there is only one key value with the same unique parts,
that for a primary key there are no childs, or that for a foreign key
there is a parent key value.  Key check list is maintained for
transactions the mode of which is either read check mode or write
check mode.

The read check list is maintained for transaction the mode of which is
read check. It contains the read operations done in the transaction.
At commit time the read operations are repeated to see if there are
any changes in the transaction read set.

The write check list is maintained for transaction the mode of which is
write check. It contains the write operations done in the transaction.
The unique tuple reference for every clustering key is stored to the
list. At commit time the write operations are checked to see that
noone elese has changed the same tuples.

In read-only transaction nothing is done during validation, there
is no need for special validation, because the read operations
always work in a consistent view of the data.

If no writes are done in the transactions, the transaction validation
is done as in a read-only transaction.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

user object     dbe0user.c

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------

void main(void)
{
        dbe_db_t* db;
        dbe_user_t* user;
        dbe_trx_t* trx;
        rs_cd_t* cd;

        /* open the database */
        db = dbe_db_init(...);

        /* create user */
        user = dbe_user_init(...);

        /* begin transaction */
        trx = dbe_trx_begin(user);

        /* do database operations */
        ...

        /* commit transaction */
        dbe_trx_commit(trx);

        /* free memory */
        dbe_trx_done(trx);

        /* kill the user */
        dbe_user_done(user);

        /* close the database */
        dbe_db_done(db);
}

**************************************************************************
#endif /* DOCUMENTATION */

#define DBE0TRX_C

#include <ssenv.h>

#ifdef SS_MYSQL_PERFCOUNT
#include <sswindow.h>
#endif /* SS_MYSQL_PERFCOUNT */

#include <ssthread.h>
#include <sspmon.h>

#include <rs0sysi.h>
#include <su0bflag.h>

#ifdef SS_COLLATION
#include "su0collation.h"
#endif /* SS_COLLATION */

#define DBE_TRX_INTERNAL
#include "dbe7cfg.h"
#include "dbe6btre.h"
#include "dbe6log.h"
#include "dbe7gtrs.h"
#include "dbe6gobj.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe6bsea.h"
#include "dbe6log.h"
#include "dbe6bmgr.h"
#include "dbe6lmgr.h"
#include "dbe5ivld.h"
#include "dbe4tupl.h"
#include "dbe4srch.h"
#include "dbe4svld.h"
#include "dbe1trdd.h"
#include "dbe0erro.h"
#include "dbe0trx.h"
#include "dbe0db.h"
#include "dbe0user.h"

#ifdef SS_MME
#ifndef SS_MYSQL
#ifdef SS_MMEG2
#include "../mmeg2/mme0rval.h"
#else
#include <../mme/mme0rval.h>
#endif
#endif
#endif /* SS_MME */

#define TRX_KCHK_FULLCHECK  SU_BFLAG_BIT(0)
#define TRX_KCHK_EARLYVLD   SU_BFLAG_BIT(1)
#define TRX_KCHK_ESCALATED  SU_BFLAG_BIT(2)
#define TRX_KCHK_MAINMEMORY SU_BFLAG_BIT(3)

/* Structure used to store checks needed for key validation.
 * The same strucutre is used to store unique, primary and
 * foreign key validations.
 */
typedef struct {
        dbe_keyvld_t    kchk_type;      /* Type of key check. */
        dbe_trxid_t     kchk_stmttrxid; /* Statement trx id. */
        dynvtpl_t       kchk_rangemin;  /* Key range minimum. */
        dynvtpl_t       kchk_rangemax;  /* Key range maximum. */
        su_bflag_t      kchk_flags;
        rs_sysi_t*      kchk_cd;
        rs_key_t*       kchk_key;
        rs_relh_t*      kchk_relh;
} trx_keycheck_t;

#ifndef SS_NOTRXREADCHECK
/* Structure used for transaction read set validation. The read set
 * validation is done only when the transaction mode is TRX_CHECKREADS.
 */
typedef struct {
        dbe_trxid_t     rchk_stmttrxid; /* Statement trx id. */
        void*           rchk_cd;
        rs_pla_t*       rchk_plan;      /* Search plan. */
        dynvtpl_t       rchk_lastkey;   /* Last key value returned in
                                           the search. Used to limit the
                                           validation range if the actual
                                           set is smaller than the whole
                                           search range. */
        dbe_trxid_t     rchk_lasttrxid; /* Trx id of the lastkey. */
} trx_readcheck_t;
#endif /* SS_NOTRXREADCHECK */

/* Structure used for transaction write set validation. The write set
 * validation is done when the transaction mode is TRX_CHECKWRITES.
 */
typedef struct {
        dbe_trxid_t     wchk_stmttrxid; /* Statement trx id. */
        dynvtpl_t       wchk_minkey;    /* Key range min. */
        dynvtpl_t       wchk_maxkey;    /* Key range max. */
        dbe_trxnum_t    wchk_maxtrxnum; /* Read level of the write. */
        rs_sysi_t*      wchk_cd;
        rs_key_t*       wchk_key;       /* Clustering key. */
        bool            wchk_escalated;
} trx_writecheck_t;

typedef struct {
        dbe_trxid_t gs_stmttrxid;
        bool        gs_delaystmtcommit;
} trx_groupstmt_t;

#define TRX_READESCALATE_CHECKLIMIT     500

/* Structure stored into rbt of key ids.
 */
typedef struct {
        long        kin_keyid;
        su_rbt_t*   kin_rbt;
} keyid_node_t;

static dbe_ret_t trx_stmt_commit_step(
        dbe_trx_t* trx,
        bool groupstmtp,
        rs_err_t** p_errh);

static dbe_ret_t trx_stmt_localrollback(
        dbe_trx_t* trx,
        bool enteractionp,
        bool groupstmtp,
        bool forcep,
        rs_err_t** p_errh);

static void trx_addreadcheck_nomutex(
        dbe_trx_t* trx,
        rs_pla_t* plan,
        dynvtpl_t lastkey,
        dbe_trxid_t lasttrxid);

/*#***********************************************************************\
 *
 *              keychk_done
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
static void keychk_done(void* data)
{
        trx_keycheck_t* kchk = data;

        SS_MEMOBJ_DEC(SS_MEMOBJ_TRXKCHK);

        if (kchk->kchk_key != NULL) {
            rs_key_done(kchk->kchk_cd, kchk->kchk_key);
        }
        if (kchk->kchk_relh != NULL) {
            rs_relh_done(kchk->kchk_cd, kchk->kchk_relh);
        }
        dynvtpl_free(&kchk->kchk_rangemin);
        dynvtpl_free(&kchk->kchk_rangemax);
        SsMemFree(kchk);
}

#ifndef SS_NOTRXREADCHECK
/*#***********************************************************************\
 *
 *              readchk_done
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
static void readchk_done(void* data)
{
        trx_readcheck_t* rchk = data;

        SS_MEMOBJ_DEC(SS_MEMOBJ_TRXRCHK);

        rs_pla_done(rchk->rchk_cd, rchk->rchk_plan);
        dynvtpl_free(&rchk->rchk_lastkey);
        SsMemFree(rchk);
}
#endif /* SS_NOTRXREADCHECK */

/*#***********************************************************************\
 *
 *              writechk_done
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
static void writechk_done(void* data)
{
        trx_writecheck_t* wchk = data;

        SS_MEMOBJ_DEC(SS_MEMOBJ_TRXWCHK);

        if (wchk->wchk_key != NULL) {
            rs_key_done(wchk->wchk_cd, wchk->wchk_key);
        }
        dynvtpl_free(&wchk->wchk_minkey);
        dynvtpl_free(&wchk->wchk_maxkey);
        SsMemFree(wchk);
}

/*#***********************************************************************\
 *
 *              trx_freemem
 *
 * Releases memory allocated for the transaction object.
 *
 * Parameters :
 *
 *      trx - in, take
 *              Transaction handle.
 *
 *      istrxbuf - in
 *              If TRUE, transaction object memory is not released.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void trx_freemem(dbe_trx_t* trx, bool istrxbuf, bool issoft)
{
        dbe_ret_t rc;
        SsSemT* trxbufsem;

        ss_dprintf_3(("trx_freemem:begin\n"));
        ss_dassert(!trx->trx_rp.rp_activep);
        ss_dassert(dbe_trx_checkcardininfo(trx));

        if (trx->trx_commitst != TRX_COMMITST_DONE) {
            if (rs_sysi_gettrxcardininfo(trx->trx_cd) != NULL) {
                dbe_trx_cardintrans_mutexif(trx->trx_cd, FALSE, FALSE, FALSE);
            }
            /* Mark the trx as ended. trx_info state is updated in
             * dbe_gtrs_endtrx.
             */
            dbe_db_enteraction(trx->trx_db, trx->trx_cd);
            dbe_gtrs_endtrx(
                trx->trx_gtrs,
                trx->trx_info,
                trx->trx_cd,
                DBE_RC_SUCC,
                TRUE,
                trx->trx_nmergewrites,
                FALSE,
                TRUE);
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            dbe_lockmgr_unlockall(
                trx->trx_lockmgr,
                trx->trx_locktran);
        } else {
            ss_dassert(trx->trx_tropst == TRX_TROPST_DONE);
        }

        if (!issoft && trx->trx_mmll != NULL) {
            dbe_mmlocklst_replicafree(trx->trx_mmll);
            trx->trx_mmll = NULL;
        }

        su_list_donebuf(&trx->trx_writechklist);
        su_list_donebuf(&trx->trx_keychklist);
#ifndef SS_NOTRXREADCHECK
        su_list_donebuf(&trx->trx_readchklist);
#endif /* SS_NOTRXREADCHECK */

        if (trx->trx_chksearch != NULL) {
            dbe_indvld_donebuf(trx->trx_chksearch);
        }
        if (trx->trx_ddopact) {
            /* Mark dd operation stopped. We should go top this branch
             * only when transaction was aborted abnormally.
             */
            dbe_db_setddopactive(trx->trx_db, FALSE);
        }
#ifndef SS_NODDUPDATE
        if (trx->trx_trdd != NULL) {
            rc = dbe_trdd_done(
                    trx->trx_trdd,
                    dbe_trxinfo_iscommitted(trx->trx_info));
            su_rc_assert(rc == DBE_RC_SUCC, rc);
        }
#endif /* SS_NODDUPDATE */

        dbe_user_settrx(trx->trx_user, NULL);

        dbe_user_invalidatesearches(
            trx->trx_user,
            trx->trx_info->ti_usertrxid,
            DBE_SEARCH_INVALIDATE_COMMIT);

        dbe_trx_freecardininfo(trx->trx_cd);

        ss_dassert(rs_sysi_gettrxcardininfo(trx->trx_cd) == NULL);
        ss_debug(trx->trx_cardininfo_in_cd = NULL);

        trxbufsem = dbe_trxbuf_getsembytrxid(
                trx->trx_trxbuf,
                trx->trx_info->ti_usertrxid);
        SsSemEnter(trxbufsem);
        if (!issoft || dbe_trxinfo_nlinks(trx->trx_info) > 1) {
            dbe_trxinfo_done_nomutex(trx->trx_info, trx->trx_cd);
        } else {
            ss_dassert(trx->trx_infocache == NULL);
            ss_dassert(dbe_trxinfo_nlinks(trx->trx_info) == 1);
            trx->trx_infocache = trx->trx_info;
            dbe_trxinfo_donebuf_nomutex(trx->trx_info, trx->trx_cd);
        }
        trx->trx_info = NULL;
        SsSemExit(trxbufsem);

        if (!issoft && trx->trx_infocache != NULL) {
            ss_dassert(dbe_trxinfo_nlinks(trx->trx_infocache) == 1);
            dbe_trxinfo_done(
                    trx->trx_infocache,
                    trx->trx_cd,
                    dbe_trxbuf_getsembytrxid(
                            trx->trx_trxbuf,
                            trx->trx_infocache->ti_usertrxid));
        }

#ifndef SS_NOLOCKING
        if (trx->trx_locktran_localinitp) {
            dbe_locktran_done(trx->trx_locktran);
            trx->trx_locktran = NULL;
        }
#endif /* SS_NOLOCKING */

#ifndef SS_NOSEQUENCE
        if (trx->trx_seqrbt != NULL) {
            su_rbt_done(trx->trx_seqrbt);
        }
#endif /* SS_NOSEQUENCE */

        if (trx->trx_hsbctx != NULL) {
            (*trx->trx_hsbctxfuns->hsbctx_done)(trx->trx_hsbctx);
        }
        if (trx->trx_hsbsqlctx != NULL) {
            (*trx->trx_hsbctxfuns->hsbsqlctx_done)(trx->trx_hsbsqlctx);
        }

        if (trx->trx_repsqllist != NULL) {
            su_list_done(trx->trx_repsqllist);
        }

#ifdef DBE_HSB_REPLICATION
        if (trx->trx_tuplestate != NULL) {
            dbe_tuplestate_done(trx->trx_tuplestate);
        }
#endif /* DBE_HSB_REPLICATION */

        dstr_free(&trx->trx_uniquerrorkeyvalue);

        ss_debug(trx->trx_chk = -1);

        if (!istrxbuf) {
            SsMemFree(trx);
        }

        ss_dprintf_3(("trx_freemem:end\n"));
}

void dbe_trx_builduniquerrorkeyvalue(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key,
        vtpl_t* vtpl,
        dbe_btrsea_timecons_t* tc)
{
        rs_ttype_t* ttype;
        rs_atype_t* atype;
        rs_aval_t* aval;
        int firstpart;
        int nunique;
        int i;
        int ano;
        va_t* va;
        rs_sysi_t* cd;
        char* p;

        struct kp_unique_info
        {
#ifdef SS_COLLATION
            su_collation_t* kp_collation;
            size_t          kp_prefixlen;
#endif /* SS_COLLATION */
            int             kp_ano;
        };

        struct kp_unique_info* kp_unique_infos = NULL;
        int n_info_count;
        rs_ano_t kp_index;

        rs_key_t*      clkey;
        su_list_t*     refattrs;
        dbe_tref_t*    tref;
        vtpl_vamap_t*  vamap;
        dbe_index_t*   index;
        dbe_ret_t      datasea_rc;
        dbe_datasea_t* datasea;
        dbe_srk_t*     srk;
        bool           key_contain_real_data;
        dynvtpl_t      dvtpl = NULL;

        ss_dprintf_1(("dbe_trx_builduniquerrorkeyvalue\n"));
        SS_PUSHNAME("dbe_trx_builduniquerrorkeyvalue");

        if (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX) {
            SS_POPNAME;
            return;
        }

        CHK_TRX(trx);
        if (vtpl == NULL) {
            dstr_free(&trx->trx_uniquerrorkeyvalue);
            SS_POPNAME;
            return;
        }

        cd         = trx->trx_cd;
        firstpart  = rs_key_first_datapart(cd, key);
        nunique    = rs_key_lastordering(cd, key); /* we have to know how many unique parts the original key has */

        kp_unique_infos = (struct kp_unique_info*)SsMemCalloc(nunique - firstpart + 1, sizeof(struct kp_unique_info));
        for (i = firstpart, n_info_count = 0; i <= nunique; ++i, ++n_info_count) {
            kp_unique_infos[n_info_count].kp_ano       = rs_keyp_ano(cd, key, i);
#ifdef SS_COLLATION
            kp_unique_infos[n_info_count].kp_collation = rs_keyp_collation(cd, key, i);
            kp_unique_infos[n_info_count].kp_prefixlen = rs_keyp_getprefixlength(cd, key, i);
#endif /* SS_COLLATION */
        }
        
        key_contain_real_data = rs_key_isprimary(cd, key);

        /* only primary keys contains real data */
        if (!key_contain_real_data && tc != NULL) {
            su_list_t refattrs_buf;

            /*
            Here we are making search in clustering key using vtuple passed
            */
            ss_dprintf_2(("dbe_trx_builduniquerrorkeyvalue:search clustering key\n"));

            clkey = rs_relh_clusterkey(cd, relh);

            su_list_initbuf(&refattrs_buf, NULL);
            refattrs = rs_pla_form_tuple_reference(cd, clkey, &refattrs_buf, key);

            vamap = vtpl_vamap_alloc(vtpl_vacount(vtpl) + 1);
            vamap = vtpl_vamap_refill(vamap, vtpl);

            tref = dbe_tref_init();

            dbe_tref_buildsearchtref_ex(
                cd,
                tref,
                clkey,
                relh,
                refattrs,
                vamap,
                tc->tc_maxtrxid);

            index = dbe_user_getindex(trx->trx_user);

            datasea = dbe_datasea_init(
                        cd,
                        index,
                        clkey,
                        tc,
                        NULL,
                        FALSE,
                        "dbe_trx_builduniquerrorkeyvalue");

            datasea_rc = dbe_datasea_search(
                            datasea,
                            dbe_tref_getvtpl(tref),
                            tc->tc_maxtrxid,
                            &srk);

            if (datasea_rc == DBE_RC_FOUND) {
                ss_dprintf_2(("dbe_trx_builduniquerrorkeyvalue:found clustering key row\n"));
                vtpl = dbe_srk_getvtpl(srk);
                dynvtpl_setvtpl(&dvtpl, vtpl);
                vtpl = dvtpl;
                key  = clkey;
                key_contain_real_data = TRUE;
            }

            dbe_tref_done(cd, tref);
            dbe_datasea_done(datasea);
            rs_pla_clear_tuple_reference_list(cd, refattrs);
            vtpl_vamap_done(vamap);
        }

        dstr_free(&trx->trx_uniquerrorkeyvalue);

        ttype = rs_relh_ttype(cd, relh);

        if (key_contain_real_data) {
            rs_ano_t keyp_index;

            ss_dprintf_2(("dbe_trx_builduniquerrorkeyvalue:get column values from clustering key row\n"));

            for (i = 0; i < n_info_count; ++i) {
                ano = kp_unique_infos[i].kp_ano;
                kp_index = (rs_ano_t)su_pa_getdata(key->k_part_by_ano, ano) - 1;

                ss_dassert(rs_keyp_parttype(cd, key, kp_index) == RSAT_USER_DEFINED);

                atype = rs_ttype_atype(cd, ttype, ano);
                va    = vtpl_getva_at(vtpl, kp_index);
                aval  = rs_aval_create(cd, atype);

#ifdef SS_COLLATION
                if (kp_unique_infos[i].kp_collation && kp_unique_infos[i].kp_prefixlen) {
                    ss_byte_t* data;
                    va_index_t datalen;
                    size_t kp_prefixlen_bytes;

                    data = va_getdata(va, &datalen);

                    kp_prefixlen_bytes = su_collation_get_bytes_in_chars(kp_unique_infos[i].kp_collation,
                                                                         (void*)data,
                                                                         datalen,
                                                                         kp_unique_infos[i].kp_prefixlen);

                    if (kp_prefixlen_bytes < datalen) {
                        data[kp_prefixlen_bytes++] = 0x00;
                        datalen = kp_prefixlen_bytes;
                    }

                    rs_aval_setdata_raw(cd, atype, aval, data, datalen);
                } else
#endif /* SS_COLLATION */
                {
                    rs_aval_setva(cd, atype, aval, va);
                }

                if (i > 0) {
                    dstr_app(&trx->trx_uniquerrorkeyvalue, "-");
                }
                p = rs_aval_print_ex(cd, atype, aval, FALSE);
                dstr_app(&trx->trx_uniquerrorkeyvalue, p);

                SsMemFree(p);
                rs_aval_free(cd, atype, aval);
            }
        } else {
            firstpart = rs_key_first_datapart(cd, key);
            nunique   = rs_key_lastordering(cd, key);

            /*
            Unfortunately we do not have key with real data or
            we just don't need it.
            */
            ss_dprintf_2(("dbe_trx_builduniquerrorkeyvalue:get column values from secondary key\n"));
            for (i = firstpart; i <= nunique; ++i) {
                ano = rs_keyp_ano(cd, key, i);
                atype = rs_ttype_atype(cd, ttype, ano);
                va = vtpl_getva_at(vtpl, i);

                aval = rs_aval_create(cd, atype);
                rs_aval_setva(cd, atype, aval, va);
                if (i > firstpart) {
                    dstr_app(&trx->trx_uniquerrorkeyvalue, "-");
                }
                p = rs_aval_print_ex(cd, atype, aval, FALSE);
                dstr_app(&trx->trx_uniquerrorkeyvalue, p);

                SsMemFree(p);
                rs_aval_free(cd, atype, aval);
            }
        }

        SsMemFreeIfNotNULL(kp_unique_infos);

        dynvtpl_free(&dvtpl);
        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              trx_keycheck
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      kchk -
 *
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
static dbe_ret_t trx_keycheck(
        dbe_trx_t*              trx,
        trx_keycheck_t*         kchk,
        dbe_btrsea_timecons_t*  tc)
{
        dbe_indsea_t* indsea;
        dbe_searchrange_t sr;
        dbe_srk_t* srk;
        dbe_ret_t indsea_rc;
        void* cd;
        dbe_index_t* index;
        dbe_ret_t rc = DBE_RC_SUCC;
        dynvtpl_t rangemax = NULL;
        bool free_rangemax = FALSE;

        ss_trigger("trx_keycheck");

        ss_dprintf_3(("trx_keycheck: %s\n",
            kchk->kchk_type == DBE_KEYVLD_UNIQUE
                ? "Unique"
                : (kchk->kchk_type == DBE_KEYVLD_PRIMARY ? "primkey" : "forkey")));
        ss_dassert(dbe_trx_semisentered(trx));

        cd = trx->trx_cd;

        if (kchk->kchk_type == DBE_KEYVLD_PRIMARY
            || kchk->kchk_type == DBE_KEYVLD_FOREIGN) {
#ifdef SS_MIXED_REFERENTIAL_INTEGRITY
            if (rs_key_isrefmme(cd, kchk->kchk_key)) {
                rc = dbe_mme_refkeycheck(
                        cd,
                        kchk->kchk_key,
                        kchk->kchk_relh,
                        trx,
                        DBE_TRXID_MAX,
                        kchk->kchk_rangemin);
                return rc;
            }
#else
            ss_dassert(!rs_key_isrefmme(cd, kchk->kchk_key));
#endif
        }

        index = dbe_user_getindex(trx->trx_user);

        if (kchk->kchk_rangemax == NULL) {
            dynvtpl_setvtplwithincrement(&rangemax, kchk->kchk_rangemin);
            free_rangemax = TRUE;
        } else {
            rangemax = kchk->kchk_rangemax;
        }

        ss_dprintf_4(("begin full key check: key id = %ld, usertrxid = %ld\n",
            va_getlong(vtpl_getva_at(kchk->kchk_rangemin, 0)),
            DBE_TRXID_GETLONG(trx->trx_usertrxid)));

        sr.sr_minvtpl = kchk->kchk_rangemin;
        sr.sr_minvtpl_closed = TRUE;
        sr.sr_maxvtpl = rangemax;
        sr.sr_maxvtpl_closed = TRUE;

        ss_dprintf_4(("range begin:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_minvtpl));
        ss_dprintf_4(("range end:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_maxvtpl));

#ifdef SS_MYSQL
        if (kchk->kchk_type == DBE_KEYVLD_FOREIGN && rs_key_isrefmme(cd, kchk->kchk_key)) {
            /* MME Validation rules for foreign keys*/
            if (trx->trx_earlyvld) {
                /* Validate foreign keys only against current trx. */
                ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));
                tc->tc_maxtrxnum = trx->trx_info->ti_maxtrxnum;
            } else {
                /* Validate other against all committed transactions. */
                tc->tc_maxtrxnum = DBE_TRXNUM_MAX;
            }
        } else {
            /* Validate other against all committed transactions. */
            tc->tc_maxtrxnum = DBE_TRXNUM_MAX;
        }
#else /* ! SS_MYSQL */
        if (kchk->kchk_type == DBE_KEYVLD_FOREIGN && trx->trx_earlyvld) {
            /* Validate foreign keys only against current trx. */
            ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));
            tc->tc_maxtrxnum = trx->trx_info->ti_maxtrxnum;
        } else {
            /* Validate other against all committed transactions. */
            tc->tc_maxtrxnum = DBE_TRXNUM_MAX;
        }
#endif /* SS_MYSQL */

        indsea = dbe_indsea_init(
                    cd,
                    index,
                    kchk->kchk_key,
                    tc,
                    &sr,
                    NULL,
                    LOCK_FREE,
                    "trx_keycheck 1");

        dbe_indsea_setvalidate(
            indsea,
            kchk->kchk_type,
            SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_EARLYVLD));

        do {
            indsea_rc = dbe_indsea_next(
                            indsea,
                            DBE_TRXID_NULL,
                            &srk);
        } while (indsea_rc == DBE_RC_NOTFOUND);

        switch (kchk->kchk_type) {
            case DBE_KEYVLD_UNIQUE:
                if (indsea_rc == DBE_RC_FOUND) {
                    /* There is a key value.
                     */
                    ss_dprintf_4(("  found key, key id = %ld, trxid = %ld, indsea_rc = %d\n",
                        va_getlong(vtpl_getva_at(dbe_srk_getvtpl(srk), 0)),
                        DBE_TRXID_GETLONG(dbe_srk_gettrxid(srk)),
                        indsea_rc));
                    ss_output_4(dbe_bkey_dprint(4, dbe_srk_getbkey(srk)));
                    if (!DBE_TRXID_EQUAL(dbe_srk_gettrxid(srk), trx->trx_usertrxid)) {
                        /* Key value not inserted by current transaction,
                         * it is unique error.
                         */
                        ss_dprintf_4(("  NOT CURRENT TRX\n"));
                        ss_output_4(dbe_bkey_dprint(4, dbe_srk_getbkey(srk)));
                        dbe_trx_seterrkey(trx, kchk->kchk_key);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                        dbe_trx_builduniquerrorkeyvalue(
                            trx, 
                            kchk->kchk_relh, 
                            kchk->kchk_key, 
                            dbe_srk_getvtpl(srk),
                            tc);
#endif
                        ss_dassert(kchk->kchk_key != NULL);
                        if (rs_key_isprimary(kchk->kchk_cd, kchk->kchk_key)) {
                            rc = DBE_ERR_PRIMUNIQUE_S;
                        } else {
                            rc = DBE_ERR_UNIQUE_S;
                        }
                    } else {
                        do {
                            indsea_rc = dbe_indsea_next(
                                            indsea,
                                            DBE_TRXID_NULL,
                                            &srk);
                        } while (indsea_rc == DBE_RC_NOTFOUND);
                        if (indsea_rc == DBE_RC_FOUND) {
                            /* There is more than one key value,
                            * it is unique error.
                            */
                            ss_dprintf_4(("  MORE THAN ONE KEY (indsea_rc = %d)\n", indsea_rc));
                            ss_output_4(dbe_bkey_dprint(4, dbe_srk_getbkey(srk)));
                            dbe_trx_seterrkey(trx, kchk->kchk_key);
                            ss_dassert(kchk->kchk_key != NULL);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                            dbe_trx_builduniquerrorkeyvalue(
                                trx, 
                                kchk->kchk_relh, 
                                kchk->kchk_key, 
                                dbe_srk_getvtpl(srk),
                                tc);
#endif
                            if (rs_key_isprimary(kchk->kchk_cd, kchk->kchk_key)) {
                                rc = DBE_ERR_PRIMUNIQUE_S;
                            } else {
                                rc = DBE_ERR_UNIQUE_S;
                            }
                        } else if (indsea_rc != DBE_RC_END) {
                            /* There was an error. */
                            rc = indsea_rc;
                        }
                    }
                } else if (indsea_rc != DBE_RC_END) {
                    /* There was an error. */
                    rc = indsea_rc;
                }
                break;
            case DBE_KEYVLD_PRIMARY:
                /* Check that child key value does not exist. */
                if (indsea_rc == DBE_RC_FOUND) {
                    /* There is a key value.
                     */
                    va_t *va;
                    rs_key_t *mkey = NULL;
                    uint i;
                    su_pa_t *keys;
                    dynvtpl_t pk_rangemin = NULL;
                    va_t va_keyid;
                    int vacount;
                    int nparts;

                    ss_dprintf_4(("  found key, key id = %ld, trxid = %ld, indsea_rc = %d\n",
                        va_getlong(vtpl_getva_at(dbe_srk_getvtpl(srk), 0)),
                        DBE_TRXID_GETLONG(dbe_srk_gettrxid(srk)),
                        indsea_rc));
                    ss_dprintf_4(("  CHILD KEY VALUE EXISTS\n"));

                    /* Check new value in the primary index. */
                    dbe_indsea_done(indsea);

                    /* Doing additional check in to make sure key has
                     * not been deleted in the same transaction
                     */

                    /* Find matching unique index */
                    keys = rs_relh_keys(cd, kchk->kchk_relh);
                    nparts = rs_key_nparts(cd, kchk->kchk_key);
                    su_pa_do_get(keys, i, mkey) {
                        if (rs_key_isunique(cd, mkey) &&
                            rs_key_nparts(cd, mkey) >= nparts)
                        {
                            rs_ano_t j;
                            for (j = 0; j < nparts; j++) {
                                if (rs_keyp_ano(cd, mkey, j)
                                        != rs_keyp_ano(cd, kchk->kchk_key, j))
                                {
                                    mkey = NULL;
                                    break;
                                }
                            }
                            if (mkey != NULL) {
                                break;
                            }
                        } else {
                            mkey = NULL;
                        }
                    }
                    ss_assert(mkey != NULL);

                    /* Do the search with the index found */
                    vacount = vtpl_vacount(kchk->kchk_rangemin);
                    va = VTPL_GETVA_AT0(kchk->kchk_rangemin);
                    dynvtpl_setvtpl(&pk_rangemin, VTPL_EMPTY);
                    va_setlong(&va_keyid, rs_key_id(cd, mkey));
                    dynvtpl_appva(&pk_rangemin, &va_keyid);
                    va = vtpl_skipva(va);
                    for (i = 1; i < vacount; i++, va = VTPL_SKIPVA(va)) {
                        dynvtpl_appva(&pk_rangemin, va);
                    }
                    sr.sr_minvtpl = pk_rangemin;
                    sr.sr_minvtpl_closed = TRUE; 
                    if (free_rangemax) {
                        dynvtpl_free(&rangemax);
                    } else {
                        rangemax = NULL;
                    }
                    dynvtpl_setvtplwithincrement(&rangemax, sr.sr_minvtpl);
                    free_rangemax = TRUE;
                    sr.sr_maxvtpl = rangemax;
                    sr.sr_maxvtpl_closed = TRUE;

                    indsea = dbe_indsea_init(
                        cd,
                        index, 
                        kchk->kchk_key,
                        tc,
                        &sr,
                        NULL,
                        LOCK_FREE,
                        "trx_keycheck 2");

                    dbe_indsea_setvalidate(
                        indsea,
                        kchk->kchk_type,
                        SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_EARLYVLD));

                    do {
                        indsea_rc = dbe_indsea_next(
                            indsea,
                            DBE_TRXID_NULL,
                            &srk);
                    } while (indsea_rc == DBE_RC_NOTFOUND);

                    if (indsea_rc != DBE_RC_FOUND) {
                        rc = DBE_ERR_CHILDEXIST_S;
                        dbe_trx_seterrkey(trx, kchk->kchk_key);
                    }
                    dynvtpl_free(&pk_rangemin);
                } else if (indsea_rc != DBE_RC_END) {
                    /* There was an error. */
                    rc = indsea_rc;
                }
                break;
            case DBE_KEYVLD_FOREIGN:
                /* Check that parent key value does exists. */
                if (indsea_rc == DBE_RC_FOUND) {
                    /* Success, there is a key value.
                     */
                    ss_dprintf_4(("  found key, key id = %ld, trxid = %ld, indsea_rc = %d\n",
                        va_getlong(vtpl_getva_at(dbe_srk_getvtpl(srk), 0)),
                        DBE_TRXID_GETLONG(dbe_srk_gettrxid(srk)),
                        indsea_rc));
                } else if (indsea_rc == DBE_RC_END) {
                    ss_dprintf_4(("  PARENT KEY VALUE DOES NOT EXISTS\n"));
                    rc = DBE_ERR_PARENTNOTEXIST_S;
                    dbe_trx_seterrkey(trx, kchk->kchk_key);
                } else {
                    /* There was an error. */
                    rc = indsea_rc;
                }
                break;
            default:
                ss_error;
        }

        if (kchk->kchk_type == DBE_KEYVLD_PRIMARY
            || kchk->kchk_type == DBE_KEYVLD_FOREIGN)
        {
            rs_key_done(cd, kchk->kchk_key);
            kchk->kchk_key = NULL;
        }

        dbe_indsea_done(indsea);
        if (free_rangemax) {
            dynvtpl_free(&rangemax);
        }

        ss_trigger("trx_keycheck");

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_bonsaikeycheck
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      kchk -
 *
 *
 *      tc -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trx_bonsaikeycheck(
        dbe_trx_t* trx,
        trx_keycheck_t* kchk,
        dbe_btrsea_timecons_t* tc)
{
        dbe_indvld_t indvld;
        dbe_searchrange_t sr;
        dbe_srk_t* srk;
        dbe_ret_t indvld_rc;
        void* cd;
        dbe_index_t* index;
        dbe_ret_t rc = DBE_RC_SUCC;
        dynvtpl_t rangemax = NULL;

        ss_dprintf_3(("trx_bonsaikeycheck\n"));
        ss_dassert(kchk->kchk_type == DBE_KEYVLD_UNIQUE);
        ss_dassert(!SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_MAINMEMORY));
        ss_dassert(dbe_trx_semisentered(trx));
        ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));
        ss_debug(memset(&indvld, '\0', sizeof(indvld)));

        cd = trx->trx_cd;
        index = trx->trx_index;

        ss_dprintf_4(("begin bonsai unq check: key id = %ld, usertrxid = %ld\n",
            va_getlong(vtpl_getva_at(kchk->kchk_rangemin, 0)),
            DBE_TRXID_GETLONG(trx->trx_usertrxid)));

        if (kchk->kchk_rangemax == NULL) {
            dynvtpl_setvtplwithincrement(&rangemax, kchk->kchk_rangemin);
        } else {
            rangemax = kchk->kchk_rangemax;
        }

        sr.sr_minvtpl = kchk->kchk_rangemin;
        sr.sr_minvtpl_closed = TRUE;
        sr.sr_maxvtpl = rangemax;
        sr.sr_maxvtpl_closed = TRUE;

        ss_dprintf_4(("range begin:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_minvtpl));
        ss_dprintf_4(("range end:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_maxvtpl));

        dbe_indvld_initbuf(
            &indvld,
            cd,
            index,
            tc->tc_usertrxid,
            trx->trx_info->ti_committrxnum,
            DBE_TRXNUM_SUM(trx->trx_info->ti_maxtrxnum, 1),
            &sr,
            NULL,
            kchk->kchk_key,
            DBE_KEYVLD_NONE,
            SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_EARLYVLD),
            FALSE);

        do {
            indvld_rc = dbe_indvld_next(&indvld, &srk);
        } while (indvld_rc == DBE_RC_NOTFOUND);

        if (indvld_rc == DBE_RC_FOUND) {
            /* There is a key value.
             */
            ss_dprintf_4(("  found key, key id = %ld, trxid = %ld, indvld_rc = %d\n",
                va_getlong(vtpl_getva_at(dbe_srk_getvtpl(srk), 0)),
                DBE_TRXID_GETLONG(dbe_srk_gettrxid(srk)),
                indvld_rc));
            ss_output_4(dbe_bkey_dprint(4, dbe_srk_getbkey(srk)));
            if (!DBE_TRXID_EQUAL(dbe_srk_gettrxid(srk), trx->trx_usertrxid)) {
                /* Key value not inserted by current transaction,
                 * it may be unique error.
                 */
                ss_dprintf_4(("  NOT CURRENT TRX\n"));
                ss_output_4(dbe_bkey_dprint(4, dbe_srk_getbkey(srk)));
                dbe_trx_seterrkey(trx, kchk->kchk_key);
                ss_dassert(kchk->kchk_key != NULL);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                dbe_trx_builduniquerrorkeyvalue(
                    trx, 
                    kchk->kchk_relh, 
                    kchk->kchk_key, 
                    dbe_srk_getvtpl(srk),
                    tc);
#endif
                if (rs_key_isprimary(kchk->kchk_cd, kchk->kchk_key)) {
                    rc = DBE_ERR_PRIMUNIQUE_S;
                } else {
                    rc = DBE_ERR_UNIQUE_S;
                }
            } else {
                do {
                    indvld_rc = dbe_indvld_next(&indvld, &srk);
                } while (indvld_rc == DBE_RC_NOTFOUND);
                if (indvld_rc == DBE_RC_FOUND) {
                    /* There is more than one key value,
                     * it may be unique error.
                     */
                    ss_dprintf_4(("  MORE THAN ONE KEY (indvld_rc = %d)\n", indvld_rc));
                    ss_output_4(dbe_bkey_dprint(4, dbe_srk_getbkey(srk)));
                    dbe_trx_seterrkey(trx, kchk->kchk_key);
                    ss_dassert(kchk->kchk_key != NULL);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                    dbe_trx_builduniquerrorkeyvalue(
                        trx, 
                        kchk->kchk_relh, 
                        kchk->kchk_key, 
                        dbe_srk_getvtpl(srk),
                        tc);
#endif
                    if (rs_key_isprimary(kchk->kchk_cd, kchk->kchk_key)) {
                        rc = DBE_ERR_PRIMUNIQUE_S;
                    } else {
                        rc = DBE_ERR_UNIQUE_S;
                    }
                } else if (indvld_rc != DBE_RC_END) {
                    /* There was an error. */
                    rc = indvld_rc;
                }
            }
        } else if (indvld_rc != DBE_RC_END) {
            /* There was an error. */
            rc = indvld_rc;
        }
        dbe_indvld_donebuf(&indvld);
        if (kchk->kchk_rangemax == NULL) {
            dynvtpl_free(&rangemax);
        }

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_keycheck_builduniquecheck
 *
 * Builds unique key check v-tuple from key v-tuple.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      key -
 *
 *
 *      key_vtpl -
 *
 *
 *      p_rangemin_dvtpl -
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
static void trx_keycheck_builduniquecheck(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_key_t* key,
        vtpl_t* key_vtpl,
        dynvtpl_t* p_rangemin_dvtpl)
{
        int nordering;
        int i;
        va_t* va;

        ss_dassert(rs_key_isunique(cd, key));

        nordering = rs_key_lastordering(cd, key) + 1;

        dynvtpl_setvtpl(p_rangemin_dvtpl, VTPL_EMPTY);
        va = VTPL_GETVA_AT0(key_vtpl);
        for (i = 0; i < nordering; i++, va = VTPL_SKIPVA(va)) {
            dynvtpl_appva(p_rangemin_dvtpl, va);
        }
}

/*#***********************************************************************\
 *
 *              trx_keycheck_escalated
 *
 * Does key check for escalated key check range.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      kchk -
 *
 *
 *      vi -
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
static dbe_ret_t trx_keycheck_escalated(
        dbe_trx_t* trx,
        trx_keycheck_t* kchk,
        trx_vldinfo_t* vi)
{
        dbe_ret_t rc;
        dbe_srk_t* srk;

        ss_dprintf_3(("trx_keycheck_escalated\n"));
        ss_dassert(kchk->kchk_key != NULL);
        ss_dassert(!SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_MAINMEMORY));
        ss_dassert(dbe_trx_semisentered(trx));

        if (trx->trx_chksearch == NULL) {
            /* Start search from escalated range.
             */
            dbe_searchrange_t sr;

            ss_dassert(kchk->kchk_rangemax != NULL);

            sr.sr_minvtpl = kchk->kchk_rangemin;
            sr.sr_minvtpl_closed = TRUE;
            sr.sr_maxvtpl = kchk->kchk_rangemax;
            sr.sr_maxvtpl_closed = TRUE;

            trx->trx_chksearch = dbe_indvld_initbuf(
                                    &trx->trx_chksearchbuf,
                                    trx->trx_cd,
                                    trx->trx_index,
                                    trx->trx_usertrxid,
                                    DBE_TRXNUM_MAX,
                                    DBE_TRXNUM_MAX,
                                    &sr,
                                    NULL,
                                    kchk->kchk_key,
                                    DBE_KEYVLD_NONE,
                                    trx->trx_earlyvld,
                                    FALSE);
        }

        rc = dbe_indvld_next(trx->trx_chksearch, &srk);
        ss_dprintf_4(("trx_keycheck_escalated:dbe_indvld_next rc = %s\n", su_rc_nameof(rc)));

        switch (rc) {
            case DBE_RC_FOUND:
                if (!dbe_srk_isdeletemark(srk) &&
                    DBE_TRXID_EQUAL(dbe_srk_getkeytrxid(srk), kchk->kchk_stmttrxid)) {
                    trx_keycheck_t cur_kchk;
                    dbe_btrsea_timecons_t tc;

                    ss_dprintf_4(("trx_keycheck_escalated:do escalated check\n"));
                    cur_kchk = *kchk;
                    cur_kchk.kchk_rangemin = NULL;
                    cur_kchk.kchk_rangemax = NULL;

                    tc = vi->vk_.tc;

                    switch (kchk->kchk_type) {
                        case DBE_KEYVLD_UNIQUE:
                            trx_keycheck_builduniquecheck(
                                trx->trx_cd,
                                kchk->kchk_key,
                                dbe_srk_getvtpl(srk),
                                &cur_kchk.kchk_rangemin);
                            break;
                        default:
                            ss_rc_error(kchk->kchk_type);
                    }
                    rc = trx_keycheck(trx, &cur_kchk, &tc);
                    dynvtpl_free(&cur_kchk.kchk_rangemin);
                    if (rc == DBE_RC_SUCC) {
                        rc = DBE_RC_CONT;
                    }
                } else {
                    rc = DBE_RC_CONT;
                }
                break;
            case DBE_RC_NOTFOUND:
                rc = DBE_RC_CONT;
                break;
            case DBE_RC_END:
                dbe_indvld_donebuf(trx->trx_chksearch);
                trx->trx_chksearch = NULL;
                vi->vk_.node = su_list_next(
                                    &trx->trx_keychklist,
                                    vi->vk_.node);
                rc = DBE_RC_CONT;
                break;
            default:
                su_rc_dassert(rc != DBE_RC_CONT && rc != DBE_RC_SUCC, rc);
                dbe_indvld_donebuf(trx->trx_chksearch);
                trx->trx_chksearch = NULL;
                break;
        }
        ss_dprintf_4(("trx_keycheck_escalated:return rc = %s\n", su_rc_nameof(rc)));

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_validate_key_init
 *
 * Initializes the key validation of a transaction.
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
static void trx_validate_key_init(
        dbe_trx_t* trx)
{
        trx_vldinfo_t* vi;

        ss_dprintf_3(("trx_validate_key_init:begin\n"));
        ss_dassert(trx->trx_vldst == TRX_VLDST_KEYS);
        ss_dassert(dbe_trx_semisentered(trx));

        vi = &trx->trx_vldinfo;

        vi->vk_.tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        vi->vk_.tc.tc_maxtrxnum = trx->trx_info->ti_committrxnum;
        vi->vk_.tc.tc_usertrxid = trx->trx_info->ti_usertrxid;
        vi->vk_.tc.tc_maxtrxid = DBE_TRXID_MAX;
        vi->vk_.tc.tc_trxbuf = NULL;

        vi->vk_.node = su_list_first(&trx->trx_keychklist);

        ss_dprintf_3(("trx_validate_key_init:end\n"));
}

/*#***********************************************************************\
 *
 *              trx_validate_key_advance
 *
 * Advances the key validation of a transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_UNIQUE
 *      DBE_ERR_CHILDEXIST
 *      DBE_ERR_PARENTNOTEXIST
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trx_validate_key_advance(
        dbe_trx_t* trx)
{
        dbe_ret_t rc;
        trx_vldinfo_t* vi;

        ss_dprintf_3(("trx_validate_key_advance:begin\n"));
        ss_dassert(trx->trx_vldst == TRX_VLDST_KEYS);
        ss_dassert(dbe_trx_semisentered(trx));

        vi = &trx->trx_vldinfo;

        if (vi->vk_.node == NULL) {

            /* End of key validation. */
            rc = DBE_RC_SUCC;

        } else {

            trx_keycheck_t* kchk;

            kchk = su_listnode_getdata(vi->vk_.node);

            if (SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_FULLCHECK)) {
                ss_dprintf_4(("trx_validate_key:do full check\n"));
                if (SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_ESCALATED)) {
                    rc = trx_keycheck_escalated(trx, kchk, vi);
                } else {
                    rc = trx_keycheck(trx, kchk, &vi->vk_.tc);
                    vi->vk_.node = su_list_next(
                                        &trx->trx_keychklist,
                                        vi->vk_.node);
                }
            } else {
                ss_dassert(kchk->kchk_type == DBE_KEYVLD_UNIQUE);
                ss_dprintf_4(("trx_validate_key:check from bonsai\n"));
                rc = trx_bonsaikeycheck(trx, kchk, &vi->vk_.tc);
                if (rc != DBE_RC_SUCC) {
                    /* There might be a conflict, do a full check.
                     */
                    ss_dprintf_4(("trx_validate_key:check from bonsai failed, do full check\n"));
                    if (SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_ESCALATED)) {
                        rc = trx_keycheck_escalated(trx, kchk, vi);
                    } else {
                        rc = trx_keycheck(trx, kchk, &vi->vk_.tc);
                        vi->vk_.node = su_list_next(
                                            &trx->trx_keychklist,
                                            vi->vk_.node);
                    }
                } else {
                    vi->vk_.node = su_list_next(
                                        &trx->trx_keychklist,
                                        vi->vk_.node);
                }
            }

            if (rc == DBE_RC_SUCC) {
                rc = DBE_RC_CONT;
            }
        }
        ss_dprintf_3(("trx_validate_key_advance:end, rc = %d\n", rc));

        return(rc);
}

#ifndef SS_NOTRXREADCHECK

/*#***********************************************************************\
 *
 *              trx_validate_read_init
 *
 * Initializes read set validation.
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
static void trx_validate_read_init(
        dbe_trx_t* trx)
{
        trx_vldinfo_t* vi;

        ss_dprintf_3(("trx_validate_read:begin, time range = [%ld, %ld]\n",
            DBE_TRXNUM_GETLONG(trx->trx_info->ti_maxtrxnum) + 1, DBE_TRXNUM_GETLONG(trx->trx_info->ti_committrxnum) - 1));
        ss_dassert(trx->trx_mode == TRX_CHECKREADS);
        ss_dassert(trx->trx_vldst == TRX_VLDST_READS);
        ss_dassert(dbe_trx_semisentered(trx));
        ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));

        vi = &trx->trx_vldinfo;

        if (DBE_TRXNUM_CMP_EX(
                DBE_TRXNUM_SUM(trx->trx_info->ti_maxtrxnum, 1),
                DBE_TRXNUM_SUM(trx->trx_info->ti_committrxnum, -1)) <= 0)
        {
            /* There is a time range to validate.
             */
            ss_dprintf_4(("trx_validate_read_init:there is a time range\n"));
            vi->vr_.node = su_list_first(&trx->trx_readchklist);

        } else {
            /* The time range for validation is empty. There is no
             * need for read set validation.
             * WARNING! NOTE! FUTURE! It is expected that the
             *      transaction is internally consistent. Is this
             *      actually a correct asumption.
             */
            ss_dprintf_4(("trx_validate_read_init:empty time range, NO VALIDATE NEEDED\n"));
            vi->vr_.node = NULL;
        }

        ss_dprintf_3(("trx_validate_read_init:end\n"));
}

/*#***********************************************************************\
 *
 *              trx_validate_read_advance
 *
 * Advances the read set validation of the transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value :
 *
 *      DBE_RC_CONT
 *      DBE_RC_SUCC
 *      DBE_ERR_NOTSERIALIZABLE
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trx_validate_read_advance(
        dbe_trx_t* trx)
{
        trx_readcheck_t* rchk;
        void* cd;
        dbe_ret_t rc;
        trx_vldinfo_t* vi;

        ss_dprintf_3(("trx_validate_read_advance:begin, time range = [%ld, %ld]\n",
            DBE_TRXNUM_GETLONG(trx->trx_info->ti_maxtrxnum) + 1,
            DBE_TRXNUM_GETLONG(trx->trx_info->ti_committrxnum) - 1));
        ss_dassert(trx->trx_mode == TRX_CHECKREADS);
        ss_dassert(trx->trx_vldst == TRX_VLDST_READS);
        ss_dassert(dbe_trx_semisentered(trx));
        ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));

        vi = &trx->trx_vldinfo;

        if (vi->vr_.node == NULL) {

            /* End of read set validation. */
            rc = DBE_RC_SUCC;

        } else {

            dbe_seavld_t* seavld;
            dbe_searchrange_t sr;
            dbe_ret_t sea_rc;
            dbe_trxid_t seatrxid;

            rc = DBE_RC_CONT;
            rchk = su_listnode_getdata(vi->vr_.node);
            cd = trx->trx_cd;

            /* Validate the read set by repeating the read operation in
               a time range between trx->trx_info->ti_maxtrxnum + 1 and
               trx->trx_info->ti_committrxnum - 1. The read set is accepted
               only if there are no key values in the time range.
            */

            rs_pla_get_range_start(
                cd,
                rchk->rchk_plan,
                &sr.sr_minvtpl,
                &sr.sr_minvtpl_closed);

            sr.sr_maxvtpl = rchk->rchk_lastkey;
            sr.sr_maxvtpl_closed = TRUE;

            ss_dprintf_4(("begin read check: key id = %ld, usertrxid = %ld\n",
                va_getlong(vtpl_getva_at(sr.sr_minvtpl, 0)),
                DBE_TRXID_GETLONG(trx->trx_usertrxid)));
            ss_dprintf_4(("range begin:\n"));
            ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_minvtpl));
            ss_dprintf_4(("range end:\n"));
            ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_maxvtpl));

            seavld = dbe_seavld_init(
                        trx->trx_user,
                        trx,
                        rchk->rchk_plan,
                        &sr,
                        DBE_TRXNUM_SUM(trx->trx_info->ti_committrxnum, -1),
                        DBE_TRXNUM_SUM(trx->trx_info->ti_maxtrxnum, 1),
                        FALSE);

            do {
                sea_rc = dbe_seavld_next(seavld, &seatrxid);
            } while (sea_rc == DBE_RC_NOTFOUND);

            dbe_seavld_done(seavld);

            if (sea_rc == DBE_RC_FOUND) {
                ss_dprintf_4(("  FOUND KEY VALUE, key value trxid = %ld\n", DBE_TRXID_GETLONG(seatrxid)));
                rc = DBE_ERR_NOTSERIALIZABLE;
            } else if (sea_rc != DBE_RC_END) {
                /* There was an error. */
                rc = sea_rc;
            }
            vi->vr_.node = su_list_next(&trx->trx_readchklist, vi->vr_.node);
        }
        ss_dprintf_3(("trx_validate_read:end, rc = %d\n", rc));

        return(rc);
}

#endif /* SS_NOTRXREADCHECK */

/*#***********************************************************************\
 *
 *              trx_validate_write_init
 *
 * Initializes write set validation.
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
static void trx_validate_write_init(
        dbe_trx_t* trx)
{
        trx_vldinfo_t* vi;

        ss_trigger("trx_validate_write");
        ss_dprintf_3(("trx_validate_write_init:begin, time range = [%ld, %ld]\n",
            DBE_TRXNUM_GETLONG(trx->trx_info->ti_maxtrxnum) + 1,
            DBE_TRXNUM_GETLONG(trx->trx_info->ti_committrxnum)));
        ss_dassert(trx->trx_mode == TRX_CHECKWRITES);
        ss_dassert(trx->trx_vldst == TRX_VLDST_WRITES);
        ss_dassert(dbe_trx_semisentered(trx));
        ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));

        vi = &trx->trx_vldinfo;

#ifdef REMOVED_BY_JARMOR /* Dec 8, 1995 */
        if (trx->trx_earlyvld ||
            trx->trx_tmpmaxtrxnum != DBE_TRXNUM_NULL ||
            (trx->trx_info->ti_maxtrxnum + 1 <=
             trx->trx_info->ti_committrxnum - 1)) {

            /* There is a time range that must be validated.
             */
            ss_dprintf_4(("trx_validate_write_init:there is a time range or early validate is used\n"));
            vi->vw_.node = su_list_first(&trx->trx_writechklist);

        } else {
            /* The time range for validation is empty, no need for
             * any validation.
             */
            ss_dprintf_4(("trx_validate_write_init:empty time range, NO VALIDATE NEEDED\n"));
            vi->vw_.node = NULL;
        }
#else
        /* Above has a bad optimization, validate is always needed. Found
         * a bug when same transaction deletes the same row twice, e.g.
         * using CURRENT OF cursor. Happened only with late validate.
         */
        vi->vw_.node = su_list_first(&trx->trx_writechklist);
#endif

        ss_dprintf_3(("trx_validate_write_init:end\n"));
        ss_trigger("trx_validate_write");
}

/*#***********************************************************************\
 *
 *              trx_validate_one_write
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      wchk -
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
static dbe_ret_t trx_validate_one_write(
        dbe_trx_t* trx,
        trx_writecheck_t* wchk,
        dbe_keyvld_t keyvldtype,
        bool pessimistic)
{
        dbe_ret_t rc;
        dbe_searchrange_t sr;
        dbe_ret_t sea_rc;
        dbe_index_t* index;
        dbe_srk_t* srk;
        dbe_indvld_t indvld;
        dynvtpl_t maxkey = NULL;

        ss_trigger("trx_validate_write");
        ss_dassert(dbe_trx_semisentered(trx));
        ss_dassert(!rs_sysi_testflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY));
        ss_debug(memset(&indvld, '\0', sizeof(indvld)));

        index = trx->trx_index;
        rc = DBE_RC_SUCC;
        if (wchk->wchk_maxkey == NULL) {
            dynvtpl_setvtplwithincrement(&maxkey, wchk->wchk_minkey);
        } else {
            maxkey = wchk->wchk_maxkey;
        }

        sr.sr_minvtpl = wchk->wchk_minkey;
        sr.sr_minvtpl_closed = TRUE;
        sr.sr_maxvtpl = maxkey;
        sr.sr_maxvtpl_closed = TRUE;

        ss_pprintf_4(("trx_validate_one_write: key id = %ld, usertrxid = %ld\n",
            va_getlong(vtpl_getva_at(sr.sr_minvtpl, 0)),
            DBE_TRXID_GETLONG(trx->trx_usertrxid)));
        ss_dprintf_4(("range begin:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_minvtpl));
        ss_dprintf_4(("range end:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_maxvtpl));

        dbe_indvld_initbuf(
            &indvld,
            trx->trx_cd,
            index,
            trx->trx_usertrxid,
            trx->trx_earlyvld
                ? DBE_TRXNUM_MAX
                : trx->trx_info->ti_committrxnum,
            pessimistic
                ? DBE_TRXNUM_MIN
                : DBE_TRXNUM_SUM(wchk->wchk_maxtrxnum, 1),
            &sr,
            NULL,
            wchk->wchk_key,
            keyvldtype,
            trx->trx_earlyvld,
            pessimistic);

        do {
            sea_rc = dbe_indvld_next(&indvld, &srk);
        } while (sea_rc == DBE_RC_NOTFOUND);

        if (pessimistic) {
            /* This is used only for debugging */
            ss_pprintf_4(("trx_validate_one_write:pessimistic test\n"));
            if (sea_rc != DBE_RC_FOUND) {
                /* Key must be in permanent tree. */
                su_rc_dassert(sea_rc = DBE_RC_END, sea_rc);
            } else if (dbe_srk_isdeletemark(srk)) {
                ss_pprintf_4(("trx_validate_one_write:FOUND DELETE MARK, DBE_ERR_LOSTUPDATE, key value trxid = %ld\n", DBE_TRXID_GETLONG(dbe_srk_gettrxid(srk))));
                rc = DBE_ERR_LOSTUPDATE;
            } else {
                do {
                    sea_rc = dbe_indvld_next(&indvld, &srk);
                } while (sea_rc == DBE_RC_NOTFOUND);
                if (sea_rc == DBE_RC_FOUND) {
                    ss_pprintf_4(("trx_validate_one_write:FOUND ANOTHER ROW, DBE_ERR_LOSTUPDATE, key value trxid = %ld\n", DBE_TRXID_GETLONG(dbe_srk_gettrxid(srk))));
                    rc = DBE_ERR_LOSTUPDATE;
                }
            }
            sea_rc = DBE_RC_END;
        }
        if (sea_rc == DBE_RC_FOUND) {
            /* The delete and possible insert from user's transaction
             * are also returned. Those key values should be skipped.
             */
            bool cur_isdeletemark;
            bool prev_isdeletemark = FALSE;
            while (sea_rc == DBE_RC_FOUND) {
                ss_dprintf_4(("trx_validate_one_write:earlyvalidate\n"));
                if (!DBE_TRXID_EQUAL(dbe_srk_gettrxid(srk), trx->trx_usertrxid)) {
                    /* Not from user's transaction.
                     */
                    ss_pprintf_4(("trx_validate_one_write:Not from user's transaction\n"));
                    break;
                }
                cur_isdeletemark = dbe_srk_isdeletemark(srk);
                if (prev_isdeletemark && cur_isdeletemark) {
                    /* More than one delete to same key in user's transaction.
                     */
                    ss_pprintf_4(("trx_validate_one_write:More than one delete in user's transaction\n"));
                    break;
                }
                prev_isdeletemark = cur_isdeletemark;
                do {
                    sea_rc = dbe_indvld_next(&indvld, &srk);
                } while (sea_rc == DBE_RC_NOTFOUND);
            }
        }

        if (sea_rc == DBE_RC_FOUND) {
            ss_pprintf_4(("trx_validate_one_write:FOUND KEY VALUE, key value trxid = %ld\n", DBE_TRXID_GETLONG(dbe_srk_gettrxid(srk))));
            rc = DBE_ERR_LOSTUPDATE;
        } else if (sea_rc != DBE_RC_END) {
            /* There was an error. */
            ss_pprintf_4(("trx_validate_one_write:There was an error\n"));
            rc = sea_rc;
        }
        dbe_indvld_donebuf(&indvld);
        if (wchk->wchk_maxkey == NULL) {
            dynvtpl_free(&maxkey);
        }

        ss_trigger("trx_validate_write");

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_validate_write_escalated
 *
 * Validates escalated write check. Delete marks are searched from
 * escalated range and write check is done separately for those delete
 * marks.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      wchk -
 *
 *
 *      vi -
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
static dbe_ret_t trx_validate_write_escalated(
        dbe_trx_t* trx,
        trx_writecheck_t* wchk,
        trx_vldinfo_t* vi)
{
        dbe_ret_t rc;
        dbe_srk_t* srk;

        ss_dprintf_3(("trx_validate_write_escalated\n"));
        ss_dassert(wchk->wchk_key != NULL);
        ss_dassert(dbe_trx_semisentered(trx));

        if (trx->trx_chksearch == NULL) {
            /* Start search from escalated range.
             */
            dbe_searchrange_t sr;

            ss_dassert(wchk->wchk_maxkey != NULL);

            sr.sr_minvtpl = wchk->wchk_minkey;
            sr.sr_minvtpl_closed = TRUE;
            sr.sr_maxvtpl = wchk->wchk_maxkey;
            sr.sr_maxvtpl_closed = TRUE;

            trx->trx_chksearch = dbe_indvld_initbuf(
                                    &trx->trx_chksearchbuf,
                                    trx->trx_cd,
                                    trx->trx_index,
                                    trx->trx_usertrxid,
                                    DBE_TRXNUM_MAX,
                                    DBE_TRXNUM_MAX,
                                    &sr,
                                    NULL,
                                    wchk->wchk_key,
                                    DBE_KEYVLD_NONE,
                                    trx->trx_earlyvld,
                                    FALSE);
        }

        rc = dbe_indvld_next(trx->trx_chksearch, &srk);
        ss_dprintf_4(("trx_validate_write_escalated:dbe_indvld_next rc = %s\n", su_rc_nameof(rc)));

        switch (rc) {
            case DBE_RC_FOUND:
                if (dbe_srk_isdeletemark(srk)
                &&  DBE_TRXID_EQUAL(dbe_srk_getkeytrxid(srk), wchk->wchk_stmttrxid))
                {
                    /* Write check is done for delete marks. */
                    trx_writecheck_t cur_wchk;
                    dbe_tref_t* tref;

                    ss_dprintf_4(("trx_validate_write_escalated:do escalated check\n"));
                    tref = dbe_tref_init();
                    dbe_tref_buildclustkeytref(
                        trx->trx_cd,
                        tref,
                        wchk->wchk_key,
                        dbe_srk_getvtpl(srk),
                        dbe_srk_gettrxid(srk));

                    cur_wchk = *wchk;
                    cur_wchk.wchk_minkey = dbe_tref_getvtpl(tref);
                    cur_wchk.wchk_maxkey = NULL;

                    rc = trx_validate_one_write(trx, &cur_wchk, DBE_KEYVLD_NONE, FALSE);
                    if (rc == DBE_RC_SUCC) {
                        rc = DBE_RC_CONT;
                    }
                    dbe_tref_done(trx->trx_cd, tref);
                } else {
                    rc = DBE_RC_CONT;
                }
                break;
            case DBE_RC_NOTFOUND:
                rc = DBE_RC_CONT;
                break;
            case DBE_RC_END:
                dbe_indvld_donebuf(trx->trx_chksearch);
                trx->trx_chksearch = NULL;
                vi->vw_.node = su_list_next(
                                    &trx->trx_writechklist,
                                    vi->vw_.node);
                rc = DBE_RC_CONT;
                break;
            default:
                dbe_indvld_donebuf(trx->trx_chksearch);
                trx->trx_chksearch = NULL;
                break;
        }
        ss_dprintf_4(("trx_validate_write_escalated:return rc = %s\n", su_rc_nameof(rc)));

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_validate_write_advance
 *
 * Advances the write set validation of the transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 * Return value :
 *
 *      DBE_RC_CONT
 *      DBE_RC_SUCC
 *      DBE_ERR_LOSTUPDATE
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trx_validate_write_advance(
        dbe_trx_t* trx)
{
        trx_writecheck_t* wchk;
        dbe_ret_t rc;
        trx_vldinfo_t* vi;

        ss_trigger("trx_validate_write");
        ss_dprintf_3(("trx_validate_write_advance:begin, time range = [%ld, %ld]\n",
            DBE_TRXNUM_GETLONG(trx->trx_info->ti_maxtrxnum) + 1,
            DBE_TRXNUM_GETLONG(trx->trx_info->ti_committrxnum)));
        ss_dassert(trx->trx_mode == TRX_CHECKWRITES);
        ss_dassert(trx->trx_vldst == TRX_VLDST_WRITES);
        ss_dassert(dbe_trx_semisentered(trx));
        ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));

        vi = &trx->trx_vldinfo;

        if (vi->vw_.node == NULL) {

            /* End of write set validation. */
            rc = DBE_RC_SUCC;

        } else {

            wchk = su_listnode_getdata(vi->vw_.node);

            if (wchk->wchk_escalated) {
                /* Escalated write check.
                 */
                rc = trx_validate_write_escalated(trx, wchk, vi);
            } else {
                /* Single write check.
                 */
                rc = trx_validate_one_write(trx, wchk, DBE_KEYVLD_NONE, FALSE);
                if (rc == DBE_RC_SUCC) {
                    rc = DBE_RC_CONT;
                }
                vi->vw_.node = su_list_next(
                                    &trx->trx_writechklist,
                                    vi->vw_.node);
            }

        }
        ss_dprintf_3(("trx_validate_write_advance:end, rc = %s (%d)\n", su_rc_nameof(rc), rc));
        ss_trigger("trx_validate_write");

        return(rc);
}

static void trx_beginvalidate(dbe_trx_t* trx)
{
        ss_dprintf_3(("trx_beginvalidate\n"));

        if (DBE_TRXNUM_EQUAL(trx->trx_committrxnum, DBE_TRXNUM_NULL)) {

            trx->trx_committrxnum = dbe_counter_getnewcommittrxnum(
                                                    trx->trx_counter);
            ss_assert(!DBE_TRXNUM_EQUAL(trx->trx_committrxnum, DBE_TRXNUM_NULL));
            ss_assert(!DBE_TRXNUM_EQUAL(trx->trx_committrxnum, DBE_TRXNUM_MAX));
            trx->trx_info->ti_committrxnum = trx->trx_committrxnum;
            ss_dprintf_4(("trx_beginvalidate:dbe_trxinfo_setvalidate, trxid=%ld\n", DBE_TRXID_GETLONG(trx->trx_info->ti_usertrxid)));
            (void)dbe_trxinfo_setvalidate(trx->trx_info);
            CHK_TRXINFO(trx->trx_info);

            if (SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_USEREADLEVEL)) {
                dbe_gtrs_begintrxvalidate(trx->trx_gtrs, trx->trx_info);
            } else {
                ss_dassert(trx->trx_mode == TRX_NOCHECK || trx->trx_mode == TRX_READONLY);
            }

            ss_dprintf_4(("trx_beginvalidate:committrxnum = %ld\n", DBE_TRXNUM_GETLONG(trx->trx_info->ti_committrxnum)));

        } else {
            ss_dprintf_4(("trx_beginvalidate:validate already started\n"));
        }
}

/*#***********************************************************************\
 *
 *              trx_validatereadwrite_init
 *
 * Initializes a read write transaction validation.
 *
 * Parameters :
 *
 *      trx - in out, use
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void trx_validatereadwrite_init(
        dbe_trx_t* trx)
{
        su_list_node_t* n;
        su_list_t* searches;
        dbe_search_t* sea;

        ss_dprintf_3(("trx_validatereadwrite_init:begin\n"));

        ss_dassert(trx != NULL);
        ss_dassert(trx->trx_mode != TRX_NOWRITES);
        ss_dassert(trx->trx_mode != TRX_READONLY);
        ss_dassert(dbe_trxinfo_isbegin(trx->trx_info)||dbe_trxinfo_istobeaborted(trx->trx_info));
        ss_dassert(dbe_trx_semisentered(trx));

        if (!trx->trx_trxaddedtotrxbuf) {
            trx->trx_trxaddedtotrxbuf = TRUE;
            dbe_trxbuf_add(trx->trx_trxbuf, trx->trx_info);
        }

        ss_dassert(DBE_TRXNUM_EQUAL(trx->trx_committrxnum, DBE_TRXNUM_NULL));

        if (dbe_cfg_newtrxwaitreadlevel) {
            if (!trx->trx_earlyvld 
                || su_list_length(&trx->trx_writechklist) > 0
                || trx->trx_mode == TRX_CHECKREADS) 
            {
                /* We do not have early validate or some validate checks
                 * are left for commit time or we are checking also reads
                 * (serializable isolation level). We need to allocate 
                 * committrxnumat the start of validate phase.
                 */
                dbe_gtrs_entertrxgate(trx->trx_gtrs);
                trx_beginvalidate(trx);
                dbe_gtrs_exittrxgate(trx->trx_gtrs);
            } else {
                SU_BFLAG_SET(trx->trx_flags, TRX_FLAG_DELAYEDBEGINVALIDATE);
            }
        } else {
            dbe_gtrs_entertrxgate(trx->trx_gtrs);
            trx_beginvalidate(trx);
            dbe_gtrs_exittrxgate(trx->trx_gtrs);
        }

        switch (trx->trx_mode) {

            case TRX_NOCHECK:
            case TRX_REPLICASLAVE:
                trx->trx_vldst = TRX_VLDST_END;
                break;

#ifndef SS_NOTRXREADCHECK
            case TRX_CHECKREADS:
                ss_dassert(dbe_trx_needtoaddreadcheck(trx));
                
                searches = dbe_user_checkoutsearches(trx->trx_user);

                /* Add all unfinished searches to the check list.
                 */
                su_list_do_get(searches, n, sea) {

                    bool search_advanced;
                    rs_pla_t* plan;
                    dynvtpl_t lastkey;
                    dbe_trxid_t lasttrxid;

                    lastkey = NULL;
                    search_advanced = dbe_search_getsearchinfo(
                                            sea,
                                            &plan,
                                            &lastkey,
                                            &lasttrxid);
                    if (search_advanced) {
                        trx_addreadcheck_nomutex(trx, plan, lastkey, lasttrxid);
                    }
                }
                dbe_user_checkinsearches(trx->trx_user);

                trx->trx_vldst = TRX_VLDST_READS;
                trx_validate_read_init(trx);
                break;
#endif /* SS_NOTRXREADCHECK */

            case TRX_CHECKWRITES:
                ss_dassert(su_list_length(&trx->trx_readchklist) == 0);
                trx->trx_vldst = TRX_VLDST_WRITES;
                trx_validate_write_init(trx);
                break;

            default:
                ss_error;
        }

        ss_dprintf_3(("trx_validatereadwrite_init:end\n"));
}

/*#***********************************************************************\
 *
 *              trx_validatereadwrite_advance
 *
 * Advances one step read/write transaction validation.
 *
 * Parameters :
 *
 *      trx - in out, use
 *
 *      p_errh - out, give
 *
 * Return value :
 *
 *      DBE_RC_CONT
 *      DBE_RC_SUCC
 *      DBE_ERR_UNIQUE
 *      DBE_ERR_NOTSERIALIZABLE
 *      DBE_ERR_LOSTUPDATE
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trx_validatereadwrite_advance(
        dbe_trx_t* trx)
{
        dbe_ret_t rc = 0;
        FAKE_CODE(static bool paused=FALSE);

        ss_dprintf_3(("trx_validatereadwrite_advance:begin\n"));

        ss_dassert(trx != NULL);
        ss_dassert(dbe_trxinfo_isvalidate(trx->trx_info));
        ss_dassert(dbe_trx_semisentered(trx));
        ss_dassert(!rs_sysi_testflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY));

        FAKE_CODE_RESET(FAKE_DBE_PAUSEVALIDATE, {
                trx->trx_fakepausep = TRUE;
                paused = TRUE;
                ss_dprintf_1(("FAKE_DBE_PAUSEVALIDATE\n"));
                return(DBE_RC_CONT);
        });

        FAKE_CODE_RESET(FAKE_DBE_RESUMEVALIDATE, {
                paused = FALSE;
                trx->trx_fakepausep = FALSE;
                ss_dprintf_1(("FAKE_DBE_RESUMEVALIDATE\n"));
                return(DBE_RC_CONT);
        });

        FAKE_CODE(if (trx->trx_fakepausep) { 
                        if (paused) {
                            SsThrSleep(100L); return(DBE_RC_CONT); 
                        }
                        trx->trx_fakepausep = FALSE;
                  });


        FAKE_CODE_BLOCK(
            FAKE_DBE_RANDVALIDATEWAIT,
            {
                if (rand() % 5 == 0) {
                    SsThrSleep(900L);
                }
            }
        );
        FAKE_CODE_RESET(
            FAKE_DBE_STARTVALIDATEWAIT,
            {
                trx->trx_fakewaitp = TRUE;
                SET_FAKE(FAKE_DBE_NOCOMMITREADLEVELWAIT, 1);
            }
        );
        FAKE_CODE_RESET(
            FAKE_DBE_STOPVALIDATE,
            {
                trx->trx_fakewaitp = FALSE;
                SET_FAKE(FAKE_DBE_NOCOMMITREADLEVELWAIT, 0);
            }
        );
        FAKE_CODE(if (trx->trx_fakewaitp) { SsThrSleep(100L); return(DBE_RC_CONT); });

        switch (trx->trx_vldst) {

#ifndef SS_NOTRXREADCHECK
            case TRX_VLDST_READS:
                rc = trx_validate_read_advance(trx);
                if (rc == DBE_RC_SUCC) {
                    rc = DBE_RC_CONT;
                    trx->trx_vldst = TRX_VLDST_KEYS;
                    trx_validate_key_init(trx);
                }
                break;
#endif /* SS_NOTRXREADCHECK */

            case TRX_VLDST_WRITES:
                rc = trx_validate_write_advance(trx);
                if (rc == DBE_RC_SUCC) {
                    rc = DBE_RC_CONT;
                    trx->trx_vldst = TRX_VLDST_KEYS;
                    trx_validate_key_init(trx);
                }
                break;
            case TRX_VLDST_KEYS:
                rc = trx_validate_key_advance(trx);
                if (rc == DBE_RC_SUCC) {
                    trx->trx_vldst = TRX_VLDST_END;
                }
                break;
            case TRX_VLDST_END:
                rc = DBE_RC_SUCC;
                break;
            default:
                ss_rc_error(trx->trx_vldst);
        }

        ss_dprintf_3(("trx_validatereadwrite_advance:end, rc = %d\n", rc));

        return(rc);
}

static bool trx_resolve_flushif_policy(
        dbe_trx_t* trx)
{
        if (trx->trx_flush_policy == TRX_FLUSH_UNKNOWN) {

            switch (dbe_db_getdurabilitylevel(trx->trx_db)) {
                case DBE_DURABILITY_RELAXED:
                    trx->trx_flush_policy = TRX_FLUSH_NO;
                    break;
                case DBE_DURABILITY_STRICT:
                    trx->trx_flush_policy = TRX_FLUSH_YES;
                    break;
                case DBE_DURABILITY_ADAPTIVE:
                default:
                    ss_error;
            }

        }

        ss_dassert(trx->trx_flush_policy != TRX_FLUSH_UNKNOWN);
        return(trx->trx_flush_policy == TRX_FLUSH_YES);
}


/*#***********************************************************************\
 *
 *              trx_init
 *
 * Creates and initializes transaction object.
 *
 * Parameters :
 *
 *      trxbuf - use
 *          If not NULL, buffer area for transaction object.
 *
 *      user -
 *
 *
 *      trxinfo -
 *
 *      reptrx - in
 *
 *      usemaxreadlevel - in
 *          uses DBE_TRXNUM_MAX instead of currently available read level
 *          and thus makes the read level to "read committed"
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_trx_t* trx_init(
        dbe_trx_t* trxbuf,
        dbe_user_t* user,
        dbe_trxinfo_t* trxinfo,
        bool reptrx,
        bool usemaxreadlevel __attribute__ ((unused)),
        bool issoft)
{
        dbe_trx_t* trx;
        dbe_gobj_t* go;
        dbe_db_t* db;
        rs_sysi_t* cd;
        rs_sqlinfo_t* sqli;
        dbe_mmlocklst_t* mmll;
        dbe_trxinfo_t*  new_trxinfo;
        dbe_trxinfo_t*  cached_trxinfo;

        ss_dprintf_3(("trx_init, userid = %d\n", dbe_user_getid(user)));
        ss_dassert(user != NULL);
        /* ss_dassert(dbe_user_gettrx(user) == NULL); */
        SS_PUSHNAME("trx_init");

        db = dbe_user_getdb(user);
        cd = dbe_user_getcd(user);
        mmll = NULL;

        rs_sysi_clearflag(cd, RS_SYSI_FLAG_STORAGETREEONLY);

        if (issoft) {
            ss_dassert(trxbuf != NULL);
#ifdef SS_MMEG2
            mmll = trxbuf->trx_mmll;
#else
            mmll = NULL;
#endif
            ss_dassert(trxbuf->trx_info == NULL);
            if (trxinfo == NULL) {
                new_trxinfo = trxbuf->trx_infocache;
            } else {
                if (trxbuf->trx_infocache != NULL) {
                    ss_dassert(dbe_trxinfo_nlinks(trxbuf->trx_infocache) == 1);
                    dbe_trxinfo_done(
                            trxbuf->trx_infocache,
                            cd,
                            dbe_trxbuf_getsembytrxid(
                                    trxbuf->trx_trxbuf,
                                    trxbuf->trx_infocache->ti_usertrxid));
                    trxbuf->trx_infocache = NULL;
                }
                new_trxinfo = trxinfo;
            }
            cached_trxinfo = trxbuf->trx_infocache;
        } else {
            cached_trxinfo = NULL;
            new_trxinfo = trxinfo;
        }
        if (trxbuf != NULL) {
            trx = trxbuf;
            memset(trx, '\0', sizeof(dbe_trx_t));
        } else {
            trx = SsMemCalloc(sizeof(dbe_trx_t), 1);
        }

        /* NOTE! Trx structure is allocated using SsMemCalloc, so by
         * default all fields are initialized to zero.
         */

        ss_debug(trx->trx_chk = DBE_CHK_TRX);
        if (dbe_db_isreadonly(db)
        ||  (!reptrx && dbe_db_gethsbmode(db) == DBE_HSB_SECONDARY &&
             rs_sysi_getconnecttype(cd) == RS_SYSI_CONNECT_USER &&
#ifdef SS_HSBG2
             !(ss_migratehsbg2 && ss_convertdb)
#endif /* SS_HSBG2 */
             ))
        {
            trx->trx_mode = TRX_READONLY;
        } else {
            trx->trx_mode = TRX_NOWRITES;
        }
        trx->trx_commitst = TRX_COMMITST_INIT;
        trx->trx_tropst = TRX_TROPST_BEFORE;
        trx->trx_db = db;
        trx->trx_user = user;
        trx->trx_index = dbe_user_getindex(user);
        trx->trx_cd = cd;
        trx->trx_counter = dbe_db_getcounter(db);
        trx->trx_gtrs = dbe_db_getgtrs(db);
        trx->trx_log = dbe_db_getlog(db);
        su_list_initbuf(&trx->trx_keychklist, keychk_done);
#ifndef SS_NOTRXREADCHECK
        su_list_initbuf(&trx->trx_readchklist, readchk_done);
        ss_autotest(su_list_setmaxlen(&trx->trx_readchklist, 1000000));
#endif /* SS_NOTRXREADCHECK */
        su_list_initbuf(&trx->trx_writechklist, writechk_done);
        ss_autotest(su_list_setmaxlen(&trx->trx_writechklist, 1000000));

        trx->trx_errcode = dbe_db_getfatalerrcode(db);
        trx->trx_err_key = NULL;
        trx->trx_tmpmaxtrxnum = DBE_TRXNUM_NULL;
        trx->trx_earlyvld = dbe_db_isearlyvld(db);
        trx->trx_escalatelimits = dbe_db_getescalatelimits(db);
        trx->trx_stmtcnt = 1;
        trx->trx_hsbflushallowed = TRUE;  /* Depends on operation. */

        /* The following explict initializations are not necessary. */
        /* trx->trx_trdd = NULL;                Done by SsMemCalloc */
        /* trx->trx_ddopact = FALSE;            Done by SsMemCalloc */
        /* trx->trx_isddop = FALSE;             Done by SsMemCalloc */
        /* trx->trx_nindexwrites = 0;           Done by SsMemCalloc */
        /* trx->trx_nmergewrites = 0;           Done by SsMemCalloc */
        /* trx->trx_nlogwrites = 0;             Done by SsMemCalloc */
        trx->trx_committrxnum = DBE_TRXNUM_NULL;
        /* trx->trx_rollbackdone = FALSE;       Done by SsMemCalloc */
        /* trx->trx_nonrepeatableread = FALSE;  Done by SsMemCalloc */
        /* trx->trx_trxaddedtotrxbuf = FALSE;   Done by SsMemCalloc */
        /* trx->trx_stmtaddedtotrxbuf = FALSE;  Done by SsMemCalloc */
        /* trx->trx_nointegrity = FALSE;        Done by SsMemCalloc */
        /* trx->trx_norefintegrity = FALSE;     Done by SsMemCalloc */
        /* trx->trx_chksearch = NULL;           Done by SsMemCalloc */
        /* trx->trx_stoplogging = FALSE;        Done by SsMemCalloc */
        /* trx->trx_delaystmtcommit = FALSE;    Done by SsMemCalloc */

        sqli = rs_sysi_sqlinfo(cd);

        switch (dbe_trx_mapisolation(trx, rs_sqli_getisolationlevel(sqli))) {
            case TRX_NONREPEATABLEREAD:
                trx->trx_defaultwritemode = TRX_CHECKWRITES;
                trx->trx_nonrepeatableread = TRUE;
                break;
            case TRX_CHECKWRITES:
                trx->trx_defaultwritemode = TRX_CHECKWRITES;
                break;
            case TRX_CHECKREADS:
                trx->trx_defaultwritemode = TRX_CHECKREADS;
                break;
            case TRX_NOWRITES:
            case TRX_READONLY:
            case TRX_NOCHECK:
            case TRX_REPLICASLAVE:
                break;
        }

        trx->trx_flush_policy = TRX_FLUSH_UNKNOWN;

#ifndef SS_NOLOCKING
        trx->trx_lockmgr = dbe_db_getlockmgr(db);

        /* trx->trx_locktran = dbe_locktran_init(trx->trx_cd); */
        trx->trx_locktran = rs_sysi_getlocktran(trx->trx_cd);

        trx->trx_locktran_localinitp = FALSE;

        if (trx->trx_locktran == NULL) {
            trx->trx_locktran = dbe_locktran_init(trx->trx_cd);
            trx->trx_locktran_localinitp = TRUE;
        }

        dbe_db_getlocktimeout(
            db,
            &trx->trx_pessimistic_lock_to,
            &trx->trx_optimistic_lock_to);
        /* trx->trx_lastlockrelid = 0;          Done by SsMemCalloc */
        /* trx->trx_rellockmode = LOCK_FREE;    Done by SsMemCalloc */
#endif /* SS_NOLOCKING */

        trx->trx_table_lock_to = dbe_db_gettablelocktimeout(db);


#ifndef SS_NOSEQUENCE
        trx->trx_seq = dbe_db_getseq(db);
        /* trx->trx_seqrbt = NULL;          Done by SsMemCalloc */
#endif /* SS_NOSEQUENCE */

        trx->trx_mmll = mmll;

#ifdef DBE_REPLICATION
        /* trx->trx_replication = FALSE;    Done by SsMemCalloc */
        /* trx->trx_replicaslave = FALSE;   Done by SsMemCalloc */
        /* trx->trx_stmterrcode = DBE_RC_SUCC; Done by SsMemCalloc */
        /* trx->trx_repsqllist = NULL;      Done by SsMemCalloc */
        /* trx->trx_usersafeness = FALSE;   Done by SsMemCalloc */ 
        trx->trx_committrx_hsb = reptrx;
        trx->trx_migratehsbg2 = ss_migratehsbg2;
        /* trx->trx_tuplestate = NULL;      Done by SsMemCalloc */
        ss_dassert(DBE_RC_SUCC == 0);
#endif /* DBE_REPLICATION */

#ifdef SS_SYNC
        trx->trx_savestmtctr = 10000;
#endif /* SS_SYNC */

        trx->trx_sem = dbe_user_gettrxsem(user);

        go = dbe_db_getgobj(db);
        trx->trx_trxbuf = go->go_trxbuf;
        trx->trx_infocache = cached_trxinfo;

        if (new_trxinfo == NULL) {
            trx->trx_info = dbe_trxinfo_init(cd);
        } else {
            trx->trx_info = new_trxinfo;
            if (new_trxinfo == trxinfo) {
                trx->trx_trxaddedtotrxbuf = TRUE;
                dbe_trxinfo_link(new_trxinfo, dbe_trxbuf_getsembytrxid(trx->trx_trxbuf, trx->trx_info->ti_usertrxid));
            } else {
                ss_dassert(new_trxinfo == trx->trx_infocache);
                dbe_trxinfo_initbuf(cd, new_trxinfo);
                trx->trx_infocache = NULL;
            }
        }

        trx->trx_is2safe = dbe_db_is2safe(db);
        trx->trx_hsbg2mode = dbe_db_gethsbg2mode(db);
        trx->trx_updatetrxinfo = TRUE;

        if (trxinfo == NULL) {
#ifdef SS_HSBG2
            if (ss_migratehsbg2) {
                trx->trx_usertrxid = dbe_counter_getnewtrxid(trx->trx_counter);
                trx->trx_readtrxid = trx->trx_usertrxid;
            } else {
                ss_dprintf_4(("trx_init:create new trxinfo\n"));
                if ((trx->trx_hsbg2mode == DBE_HSB_SECONDARY && !ss_convertdb)
                ||  trx->trx_hsbg2mode == DBE_HSB_PRIMARY_UNCERTAIN)
                {
                    trx->trx_mode = TRX_READONLY;
                    trx->trx_usertrxid = dbe_counter_gettrxid(trx->trx_counter);
                    trx->trx_usertrxid = DBE_TRXID_SUM(
                                            trx->trx_usertrxid,
                                            SS_INT4_MAX / 2);
                    if (DBE_TRXID_EQUAL(trx->trx_usertrxid, DBE_TRXID_NULL)) {
                        trx->trx_usertrxid = DBE_TRXID_INIT(1);
                    }
                    trx->trx_readtrxid = DBE_TRXID_NULL;
                } else {
                    trx->trx_usertrxid = dbe_counter_getnewtrxid(trx->trx_counter);
                    trx->trx_readtrxid = trx->trx_usertrxid;
                }
            }
#else /* SS_HSBG2 */
            trx->trx_usertrxid = dbe_counter_getnewtrxid(trx->trx_counter);
            trx->trx_readtrxid = trx->trx_usertrxid;
#endif /* SS_HSBG2 */
        } else {
#ifdef SS_HSBG2
            ss_dassert(!ss_convertdb);
            if (!ss_migratehsbg2) {
                trx->trx_mode = TRX_READONLY;
                trx->trx_updatetrxinfo = FALSE;
            }
#endif /* SS_HSBG2 */
            ss_dprintf_4(("trx_init:use old trxinfo\n"));
            trx->trx_usertrxid = trx->trx_info->ti_usertrxid;
            trx->trx_readtrxid = DBE_TRXID_MAX;
        }
        ss_dprintf_4(("trx_init:trxid=%ld\n", DBE_TRXID_GETLONG(trx->trx_info->ti_usertrxid)));

        (void)dbe_trxinfo_setbegin(trx->trx_info);

        trx->trx_info->ti_usertrxid = trx->trx_usertrxid;
        trx->trx_info->ti_maxtrxnum = DBE_TRXNUM_NULL;
        trx->trx_searchtrxnum = DBE_TRXNUM_NULL;
        trx->trx_stmtsearchtrxnum = DBE_TRXNUM_NULL;
        dbe_gtrs_begintrx(trx->trx_gtrs, trx->trx_info, trx->trx_cd);
        if (!dbe_cfg_relaxedreadlevel) {
            dbe_trx_ensurereadlevel(trx, FALSE);
        }

        trx->trx_stmttrxid = trx->trx_usertrxid;

        ss_autotest_or_debug(trx->trx_info->ti_isolation = rs_sqli_getisolationlevel(sqli));
        ss_autotest_or_debug(trx->trx_info->ti_trxmode = trx->trx_mode);

        dbe_user_settrx(user, trx);

        ss_debug(trx->trx_thrid = SsThrGetid());

        SS_POPNAME;

        return(trx);
}


void dbe_trx_locktran_init(
        rs_sysi_t* cd)
{
        dbe_locktran_t* locktran;

        if (rs_sysi_getconnecttype(cd) != RS_SYSI_CONNECT_HSB) {
            locktran = rs_sysi_getlocktran(cd);
            if (locktran != NULL) {
                rs_sysi_locktran_link(cd, locktran);
            } else {
                locktran = dbe_locktran_init(cd);
                rs_sysi_setlocktran(cd, locktran);
            }
        }
}

void dbe_trx_locktran_done(
        rs_sysi_t* cd)
{
        dbe_locktran_t* locktran;

        if (rs_sysi_getconnecttype(cd) != RS_SYSI_CONNECT_HSB) {
            locktran = rs_sysi_getlocktran(cd);

            ss_dassert(locktran != NULL);
            rs_sysi_locktran_unlink(cd, locktran);
            if (rs_sysi_getlocktran(cd) == NULL) {
                dbe_locktran_done(locktran);
            }
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_begin
 *
 * Begins a new transaction.
 *
 * Parameters :
 *
 *      user - in, hold
 *              User object.
 *
 * Return value - give :
 *
 *      Transaction handle.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_trx_t* dbe_trx_begin(
        dbe_user_t* user)
{
        ss_dprintf_1(("dbe_trx_begin, userid = %d\n", dbe_user_getid(user)));
        ss_dassert(user != NULL);

        return(trx_init(NULL, user, NULL, FALSE, FALSE, FALSE));
}

/*##**********************************************************************\
 *
 *              dbe_trx_beginbuf
 *
 * Begins a new transaction.
 * Uses pre-allocated transaction buffer.
 *
 * Parameters :
 *
 *      trxbuf - in, out
 *              Transaction object buffer.
 *
 *      user - in, hold
 *              User object.
 *
 * Return value - give :
 *
 *      Transaction handle.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_trx_t* dbe_trx_beginbuf(
        dbe_trx_t* trxbuf,
        dbe_user_t* user)
{
        ss_dprintf_1(("dbe_trx_beginbuf, userid = %d\n", dbe_user_getid(user)));
        ss_dassert(user != NULL);

        return(trx_init(trxbuf, user, NULL, FALSE, FALSE, TRUE));
}

void dbe_trx_initbuf(
        dbe_trx_t*      trxbuf,
        dbe_user_t*     user __attribute__ ((unused)))
{
        memset(trxbuf, '\0', sizeof(*trxbuf));
}

/*##**********************************************************************\
 *
 *              dbe_trx_beginwithmaxreadlevel
 *
 * Begins a new transaction with DBE_TRXNUM_MAX as its read level.
 *
 * Parameters :
 *
 *      user - in, hold
 *              User object.
 *
 * Return value - give :
 *
 *      Transaction handle.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_trx_t* dbe_trx_beginwithmaxreadlevel(
        dbe_user_t* user)
{
        dbe_trx_t* trx;

        ss_dprintf_1(("dbe_trx_begin, userid = %d\n", dbe_user_getid(user)));
        ss_dassert(user != NULL);

        trx = trx_init(NULL, user, NULL, FALSE, TRUE, FALSE);
        return (trx);
}

#ifdef DBE_REPLICATION
/*##**********************************************************************\
 *
 *              dbe_trx_beginreplicarecovery
 *
 * Begins a new transaction after replication recovery. Unfinished
 * replica transactions are restarted after recovery.
 *
 * Parameters :
 *
 *      user - in, hold
 *              User object.
 *
 *      trxinfo - in, take
 *              Transaction info object.
 *
 * Return value - give :
 *
 *      Transaction handle.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_trx_t* dbe_trx_beginreplicarecovery(
        dbe_user_t* user,
        dbe_trxinfo_t* trxinfo)
{
        dbe_trx_t* trx;

        ss_dprintf_1(("dbe_trx_beginreplicarecovery, userid = %d\n", dbe_user_getid(user)));
        ss_dassert(user != NULL);

        trx = trx_init(NULL, user, trxinfo, TRUE, FALSE, FALSE);

        /* We need to set mode so that we have writes. Otherwise commit may
         * think this is read only transaction and not set the trxinfo to
         * the trxbuf. Also commit may not be logged.
         */
#ifdef SS_HSBG2
        if (ss_migratehsbg2) {
            trx->trx_mode = trx->trx_defaultwritemode;
        }
#else /* SS_HSBG2 */
        trx->trx_mode = trx->trx_defaultwritemode;
#endif /* SS_HSBG2 */

        ss_autotest_or_debug(trx->trx_info->ti_trxmode = trx->trx_mode);

        ss_dassert(trx->trx_mmll == NULL);

        return(trx);
}

/*##**********************************************************************\
 *
 *              dbe_trx_beginreplica
 *
 *
 *
 * Parameters :
 *
 *      user -
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
dbe_trx_t* dbe_trx_beginreplica(
        dbe_user_t* user)
{
        ss_dprintf_1(("dbe_trx_beginreplica, userid = %d\n", dbe_user_getid(user)));
        ss_dassert(user != NULL);

        return(trx_init(NULL, user, NULL, TRUE, FALSE, FALSE));
}

#endif /* DBE_REPLICATION */

/*##**********************************************************************\
 * 
 *      dbe_trx_restart
 * 
 * Restarts the transaction when read level is released by gtrs.
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
void dbe_trx_restart(
        dbe_trx_t* trx)
{
        bool enteraction;
        SsSemT* sem;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_restart\n"));

        ss_dassert(dbe_trxinfo_canremovereadlevel(trx->trx_info));
        ss_dassert(!trx->trx_usemaxreadlevel)

        if (trx->trx_commitst == TRX_COMMITST_DONE) {
            ss_dprintf_1(("dbe_trx_restart:TRX_COMMITST_DONE\n"));
            return;
        }

        enteraction = trx->trx_hsbtrxmarkwrittentolog && trx->trx_hsbg2mode != DBE_HSB_STANDALONE;

        if (enteraction) {
            dbe_db_enteraction(trx->trx_db, trx->trx_cd);
        }

        sem = dbe_trxbuf_getsembytrxid(trx->trx_trxbuf, trx->trx_info->ti_usertrxid);

        SsSemEnter(sem);

        dbe_trxinfo_clearcanremovereadlevel(trx->trx_info);

        if (DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum)) {
            /* Transaction read level is lost, we need to restart
             * old searches.
             */
            ss_dprintf_2(("dbe_trx_restart:restart searches\n"));

            SsSemExit(sem);

            if (enteraction) {
                dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            }

            dbe_user_restartsearches(
                trx->trx_user,
                trx,
                trx->trx_info->ti_usertrxid);
        } else {
            SsSemExit(sem);
            if (enteraction) {
                dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            }
        }
        ss_dassert(!dbe_trxinfo_canremovereadlevel(trx->trx_info));
}

/*##**********************************************************************\
 * 
 *      dbe_trx_setcanremovereadlevel
 * 
 * Sets a flag in read only transaction that allows removing a read level. 
 * Removing the read level helps merge to proceed even if a transaction is 
 * open and idle for a long time.
 * 
 * Parameters : 
 * 
 *      trx - 
 *          
 *          
 *      canremove - 
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
bool dbe_trx_setcanremovereadlevel(dbe_trx_t* trx, bool release_with_writes)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setcanremovereadlevel\n"));

        if (!SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_USEREADLEVEL)) {
            ss_dprintf_2(("dbe_trx_setcanremovereadlevel:no read level\n"));
            return(FALSE);
        }

        FAKE_CODE_BLOCK(FAKE_DBE_REMOVEREADLEVEL_FLOWONLY,
        {
            ss_dprintf_1(("dbe_trx_setcanremovereadlevel:FAKE_DBE_REMOVEREADLEVEL_FLOWONLY, release_with_writes=%d\n", release_with_writes));
            if (!release_with_writes) {
                return(FALSE);
            }
        });

        if (trx->trx_mode == TRX_NOWRITES 
            || trx->trx_mode == TRX_READONLY
            || release_with_writes) 
        {
            ss_dprintf_2(("dbe_trx_setcanremovereadlevel:no writes, setcanremovereadlevel\n"));
            dbe_trxinfo_setcanremovereadlevel(trx->trx_info);
            return(TRUE);
        } else {
            return(FALSE);
        }
}

void dbe_trx_setdelayedstmterror(dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setdelayedstmterror\n"));

        SU_BFLAG_SET(trx->trx_flags, TRX_FLAG_DELAYEDSTMTERROR);
}

#ifdef DBE_HSB_REPLICATION

/*#***********************************************************************\
 *
 *              trx_repchecktrx
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
static dbe_ret_t trx_repchecktrx(dbe_trx_t* trx)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        bool isreadonly;

        isreadonly = trx->trx_mode == TRX_NOWRITES ||
                     trx->trx_mode == TRX_READONLY;

        if (trx->trx_replication && !isreadonly) {
            ss_dassert(!trx->trx_replicaslave);

            dbe_trx_initrepparams(trx, REP_COMMIT_CHECK);

            rc = dbe_trx_replicate(trx, REP_COMMIT_CHECK);
            if (rc == DB_RC_NOHSB) {
                rc = DBE_RC_SUCC;
            }
        }
        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_repcommittype
 *
 *
 *
 * Parameters :
 *
 *      type -
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
static bool trx_repcommittype(rep_type_t type)
{
        switch (type) {
            case REP_COMMIT:
            case REP_COMMIT_NOFLUSH:
            case REP_ABORT:
                return(TRUE);
            default:
                return(FALSE);
        }
}

/*#***********************************************************************\
 *
 *              trx_repcommitstmttype
 *
 *
 *
 * Parameters :
 *
 *      type -
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
static bool trx_repcommitstmttype(rep_type_t type)
{
        switch (type) {
            case REP_STMTCOMMIT:
            case REP_STMTCOMMIT_GROUP:
            case REP_STMTABORT:
            case REP_STMTABORT_GROUP:
                return(TRUE);
            default:
                return(FALSE);
        }
}

/*#***********************************************************************\
 *
 *              trx_rependtrx
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      commitp -
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
static dbe_ret_t trx_rependtrx(dbe_trx_t* trx, bool commitp)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        bool isreadonly;
        bool replicatep;

        ss_dprintf_3(("trx_rependtrx\n"));

        if (trx->trx_rp.rp_activep
        &&  trx->trx_rp.rp_donep
        &&  !trx_repcommittype(trx->trx_rp.rp_type))
        {
            /* Operation is active but it is not trx commit or rollback.
             */
            ss_dassert(!commitp);
            trx->trx_rp.rp_activep = FALSE;
        }
        ss_dassert(!trx->trx_rp.rp_activep ||
                   trx_repcommittype(trx->trx_rp.rp_type));

        isreadonly = trx->trx_mode == TRX_NOWRITES ||
                     trx->trx_mode == TRX_READONLY;

        replicatep = !isreadonly ||
                     trx->trx_repsqllist != NULL ||
                     trx->trx_rp.rp_activep;

#ifdef SS_HSBG2
        if (commitp && !isreadonly && trx->trx_log != NULL && !trx->trx_replicaslave) {
            bool dummy;
            dbe_logi_commitinfo_t info;

            switch (trx->trx_hsbg2mode) {

                case DBE_HSB_STANDALONE:
                    ss_dprintf_4(("trx_rependtrx:DBE_HSB_STANDALONE\n"));
                    break;

                case DBE_HSB_PRIMARY:
                    ss_dassert(!ss_migratehsbg2);

                    if (!trx->trx_usersafeness && !trx->trx_is2safe) {
                        trx->trx_is2safe = dbe_db_is2safe(trx->trx_db);
                    }
                    ss_dprintf_4(("trx_rependtrx:DBE_HSB_PRIMARY:trx_is2safe %d\n", trx->trx_is2safe));
                    if (trx->trx_is2safe) {
                        info = DBE_LOGI_COMMIT_HSBPRIPHASE1|DBE_LOGI_COMMIT_LOCAL|DBE_LOGI_COMMIT_2SAFE;
                    } else {
                        info = DBE_LOGI_COMMIT_HSBPRIPHASE1|DBE_LOGI_COMMIT_LOCAL;
                        SS_RTCOVERAGE_INC(SS_RTCOV_HSBG2_1SAFE);
                    }

                    /* If hsb safeness is adaptive and logging is relaxed or adaptive (or trx is relaxed 
                     * then do not flush.
                     */
                    if (!trx->trx_usersafeness && dbe_db_hsbg2safenesslevel_adaptive(trx->trx_db)) {
                        if (dbe_db_getdurabilitylevel_raw(trx->trx_db) != DBE_DURABILITY_STRICT
                        ||  trx->trx_flush_policy == TRX_FLUSH_NO)
                        {
                            SU_BFLAG_SET(info, DBE_LOGI_COMMIT_NOFLUSH);
                        }
                    }
                    if (trx->trx_trdd != NULL || trx->trx_isddop) {
                        SU_BFLAG_SET(info, DBE_LOGI_COMMIT_DDRELOAD);
                    }
                    trx->trx_committrx_hsb = TRUE;
                    rc = dbe_log_puthsbcommitmark(
                                trx->trx_log,
                                trx->trx_cd,
                                info,
                                trx->trx_usertrxid,
                                NULL,
                                &dummy);
                    ss_rc_assert(rc == DBE_RC_SUCC || rc == SRV_ERR_HSBCONNBROKEN, rc);
                    trx->trx_nlogwrites++;
                    break;

                case DBE_HSB_SECONDARY:
                    ss_dprintf_4(("trx_rependtrx:DBE_HSB_SECONDARY\n"));
                    if (!ss_convertdb) {
                        rc = DBE_ERR_HSBSECONDARY;
                    }
                    break;

                case DBE_HSB_PRIMARY_UNCERTAIN:
                    ss_dprintf_4(("trx_rependtrx:DBE_HSB_PRIMARY_UNCERTAIN\n"));
                    ss_dassert(!ss_convertdb);
                    rc = DBE_ERR_HSBPRIMARYUNCERTAIN;
                    break;

                default:
                    ss_error;
            }
        }
#endif /* SS_HSBG2 */

        FAKE_CODE_RESET(FAKE_DBE_COMMIT_FAIL, { rc = DBE_ERR_FAILED; });

        if (trx->trx_replication && replicatep) {
            rep_type_t type;

            ss_dassert(!trx->trx_replicaslave);

            dbe_trx_endrepsql(trx, commitp);

            if (commitp) {
                if (trx_resolve_flushif_policy(trx)) {
                    type = REP_COMMIT;
                } else {
                    type = REP_COMMIT_NOFLUSH;
                }
                trx->trx_committrx_hsb = TRUE;
            } else {
                type = REP_ABORT;
            }

            if (dbe_trx_initrepparams(trx, type)) {
                trx->trx_rp.rp_flushallowed = (type == REP_COMMIT ||
                                               type == REP_COMMIT_NOFLUSH);
            }

            if (commitp) {
                bool replicated;

                ss_derror;

                trx->trx_rp.rp_hsbflush = FALSE;

#ifdef DBE_LOGORDERING_FIX
                rc = dbe_log_puthsbcommitmark(
                         trx->trx_log,
                         trx->trx_cd,
                         DBE_LOGI_COMMIT_HSBPRIPHASE1,
                         trx->trx_usertrxid,
                         &trx->trx_rp,
                         &replicated);
                ss_dassert(rc != DBE_RC_CONT);
                if (rc == DB_RC_NOHSB) {
                    rc = DBE_RC_SUCC;
                }
#else /* DBE_LOGORDERING_FIX */
                rc = dbe_trx_replicate(trx, type);
                if (rc == DB_RC_NOHSB) {
                    rc = DBE_RC_SUCC;
                }
                if (rc == DBE_RC_SUCC) {
                    rc = dbe_log_puthsbcommitmark(
                             trx->trx_log,
                             trx->trx_cd,
                             trx->trx_usertrxid);
                }
#endif /* DBE_LOGORDERING_FIX */

                if (rc == DBE_RC_SUCC) {
                    trx->trx_rp.rp_activep = TRUE;
                    trx->trx_rp.rp_type = type;
                    trx->trx_rp.rp_hsbflush = TRUE;
                }
            } else {
                /* for abort */
                rc = dbe_trx_replicate(trx, type);
                if (rc == DB_RC_NOHSB) {
                    rc = DBE_RC_SUCC;
                }
            }
        }
        ss_dprintf_4(("trx_rependtrx:rc=%d\n", rc));
        return(rc);
}

static dbe_ret_t trx_repflushcommit(dbe_trx_t* trx)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        rep_type_t type;

        ss_dprintf_3(("trx_repflushcommit\n"));

        if (trx->trx_replication) {
            if (trx_resolve_flushif_policy(trx)) {
                type = REP_COMMIT;
            } else {
                type = REP_COMMIT_NOFLUSH;
            }
            ss_dassert(type == trx->trx_rp.rp_type);
            ss_dassert(trx->trx_rp.rp_hsbflush);
            rc = dbe_trx_replicate(trx, type);
            if (rc == DB_RC_NOHSB) {
                rc = DBE_RC_SUCC;
            }
        }
        return rc;
}

/*#***********************************************************************\
 *
 *              trx_rependstmt
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      commitp -
 *
 *
 *      stmttrxid -
 *
 *
 *      groupstmtp -
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
static dbe_ret_t trx_rependstmt(
        dbe_trx_t* trx,
        bool commitp,
        dbe_trxid_t stmttrxid,
        bool groupstmtp)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        bool isreadonly;
        bool replicatep;

        ss_dassert(dbe_trx_semisentered(trx));

        if (trx->trx_rp.rp_activep
        &&  trx->trx_rp.rp_donep
        &&  !trx_repcommitstmttype(trx->trx_rp.rp_type))
        {
            /* Operation is active but it is not stmt commit or rollback.
             */
            ss_dassert(!commitp);
            trx->trx_rp.rp_activep = FALSE;
        }
        ss_dassert(!trx->trx_rp.rp_activep ||
                   trx_repcommitstmttype(trx->trx_rp.rp_type));


        isreadonly = trx->trx_mode == TRX_NOWRITES ||
                     trx->trx_mode == TRX_READONLY;

        replicatep = !isreadonly ||
                     trx->trx_repsqllist != NULL ||
                     trx->trx_rp.rp_activep;

        if (trx->trx_replication && replicatep) {
            rep_type_t type;

            ss_dassert(!trx->trx_replicaslave);
            /*  JarmoR Jul 19, 2000 Removed, should be sent only
                in real commit (test 44 in sse/test/trep.c).
                dbe_trx_endrepsql(trx, commitp); */

            if (!commitp) {
                dbe_trx_stmtrollbackrepsql(trx, stmttrxid);
            }

            if (commitp) {
                if (groupstmtp) {
                    type = REP_STMTCOMMIT_GROUP;
                } else {
                    type = REP_STMTCOMMIT;
                }
            } else {
                if (groupstmtp) {
                    type = REP_STMTABORT_GROUP;
                } else {
                    type = REP_STMTABORT;
                }
            }

            dbe_trx_initrepparams(trx, type);
            trx->trx_rp.rp_stmttrxid = stmttrxid;

            rc = dbe_trx_replicate(trx, type);
            ss_dassert(rc != DBE_RC_CONT);
            if (rc == DB_RC_NOHSB) {
                rc = DBE_RC_SUCC;
            }
        }
        return(rc);
}

#endif /* DBE_HSB_REPLICATION */

void dbe_trx_error_create(dbe_trx_t* trx, dbe_ret_t rc,  rs_err_t** p_errh)
{
        if (rc == DBE_ERR_CHILDEXIST_S || rc == DBE_ERR_PARENTNOTEXIST_S ||
            rc == DBE_ERR_UNIQUE_S || rc == DBE_ERR_PRIMUNIQUE_S)
        {
            ss_dassert(trx->trx_err_key);
            rs_error_create_key(p_errh, rc, trx->trx_err_key);
        } else if (rc != DBE_RC_SUCC) {
            rs_error_create(p_errh, rc);
        } else {
            ss_error;
        }
}

/*#***********************************************************************\
 *
 *              trx_end
 *
 * Ends transaction, writes log file entry, updates trxbuf etc.
 *
 * Parameters :
 *
 *      trx - use
 *
 *
 *      rc - in
 *
 *
 *      puttolog - in
 *          If TRUE, transaction end mark is added to the log file.
 *
 *      abort_no_validate - in
 *          If TRUE, the transaction has been aborted without validating it.
 *
 *      rollback - in
 *
 *
 *      enteractionp - in
 *
 *
 *      p_errh - in
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
static dbe_ret_t trx_end(
        dbe_trx_t* trx,
        dbe_ret_t rc,
        bool puttolog,
        bool abort_no_validate,
        bool rollback,
        bool enteractionp,
        bool count_trx,
        rs_err_t** p_errh)
{
        bool iswrites;
        dbe_db_trxtype_t trxtype;
        bool entersemp = enteractionp;
        dbe_ret_t origrc = rc;
        bool gtrs_gate_entered;
        bool check_logwaitrc = FALSE;
        dbe_ret_t logwaitrc;

#ifdef SS_MYSQL_PERFCOUNT
        __int64 startcount;
        __int64 endcount;

        if (mysql_enable_perfcount > 1) {
            QueryPerformanceCounter((LARGE_INTEGER*)&startcount);
        }
#endif /* SS_MYSQL_PERFCOUNT */

        ss_dprintf_3(("trx_end:begin, rc=%s, trx->trx_mode=%d, trxid=%ld, puttolog=%d, trx->trx_nlogwrites=%d\n",
            su_rc_nameof(rc), trx->trx_mode, DBE_TRXID_GETLONG(trx->trx_usertrxid),
            puttolog, trx->trx_nlogwrites));
        ss_dassert(rc != DBE_RC_CONT);
        ss_dassert(puttolog == FALSE || puttolog == TRUE);
        ss_dassert(abort_no_validate == FALSE || abort_no_validate == TRUE);
        ss_dassert(enteractionp || dbe_trx_semisentered(trx));
        /* ss_dassert(trx->trx_cd == dbe_user_getcd(trx->trx_user)); */
        /* ss_dassert(trx == dbe_user_gettrx(trx->trx_user)); */

#ifdef SS_HSBG2

#ifdef SS_FAKE
        dbe_trx_semexit_fake(trx, enteractionp);
#endif

        iswrites = (trx->trx_mode != TRX_NOWRITES &&
                    trx->trx_mode != TRX_READONLY);
        if (!iswrites) {
            puttolog = FALSE;
        }
        ss_dprintf_4(("trx_end:iswrites=%d, trx->trx_hsbg2mode=%d\n",
            iswrites, trx->trx_hsbg2mode));
        if (!iswrites &&
            (trx->trx_hsbg2mode == DBE_HSB_SECONDARY ||
             trx->trx_hsbg2mode == DBE_HSB_PRIMARY_UNCERTAIN))
        {
            trx->trx_updatetrxinfo = FALSE;
        }
        ss_dassert(trx->trx_updatetrxinfo || dbe_trxinfo_isbegin(trx->trx_info));
#else /* SS_HSBG2 */
        iswrites = (trx->trx_mode != TRX_NOWRITES);
#endif /* SS_HSBG2 */

        if (enteractionp && !iswrites && !puttolog) {
            enteractionp = FALSE;
        }

        if (enteractionp) {
            dbe_db_enteraction(trx->trx_db, trx->trx_cd);
        }
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE) ||
                   (!iswrites && !puttolog));

        if (entersemp) {
            dbe_trx_sementer(trx);
        }
        ss_dassert(dbe_trx_semisentered(trx));

        if (trx->trx_commitst == TRX_COMMITST_DONE) {
            if (entersemp) {
                dbe_trx_semexit(trx);
            }
            if (enteractionp) {
                dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            }
            dbe_trx_error_create(trx, rc, p_errh);
            ss_dprintf_4(("trx_end:trx->trx_commitst == TRX_COMMITST_DONE, rc=%d\n", rc));
#ifdef SS_MYSQL_PERFCOUNT
            if (mysql_enable_perfcount > 1) {
                QueryPerformanceCounter((LARGE_INTEGER*)&endcount);
                trx_end_perfcount += endcount - startcount;
                trx_end_callcount++;
            }
#endif /* SS_MYSQL_PERFCOUNT */ 
            return(rc);
        }

        if (rc != DBE_RC_SUCC) {
            ss_dassert(trx->trx_hsbcommitsent == FALSE);
            trx_rependtrx(trx, FALSE);
        }

        ss_dprintf_4(("trx_end:trx->trx_nlogwrites=%d, trx->trx_hsbcommitsent=%d\n", trx->trx_nlogwrites, trx->trx_hsbcommitsent));

        if (trx->trx_nlogwrites > 0) {
            if (puttolog) {
                trx->trx_nlogwrites++;
            }

            ss_dprintf_4(("trx_end:puttolog=%d, trx->trx_log=%d\n", puttolog, trx->trx_log));

            if (puttolog && trx->trx_log != NULL) {
                dbe_ret_t logrc;
                dbe_logi_commitinfo_t info;

                ss_rc_dassert(trx->trx_mode != TRX_NOWRITES || rc != DBE_RC_SUCC, rc);

                info = DBE_LOGI_COMMIT_LOCAL;

                if (trx->trx_trdd != NULL || trx->trx_isddop) {
                    SU_BFLAG_SET(info, DBE_LOGI_COMMIT_DDRELOAD);
                }

                if (rc == DBE_RC_SUCC) {
                    if (!trx_resolve_flushif_policy(trx)) {
                        SU_BFLAG_SET(info, DBE_LOGI_COMMIT_NOFLUSH);
                    }
                    if (trx->trx_committrx_hsb) {
                        SU_BFLAG_SET(info, DBE_LOGI_COMMIT_HSBPRIPHASE2);
                        FAKE_CODE_BLOCK(FAKE_HSB_PRI_CRASH_BEFORECOMMITPHASE2,
                        {
                            ss_dprintf_1(("FAKE_HSB_PRI_CRASH_BEFORECOMMITPHASE2:trxid=%ld\n", DBE_TRXID_GETLONG(trx->trx_usertrxid)));
                            ss_skipatexit = TRUE;
                            SsExit(0);
                        });
                    }
                    rs_sysi_setlogwaitrc(trx->trx_cd, &logwaitrc);
                    ss_dprintf_4(("trx_end:dbe_log_puttrxmark:DBE_LOGREC_COMMITTRX_INFO\n"));
                    logrc = dbe_log_puttrxmark(
                                trx->trx_log,
                                trx->trx_cd,
                                DBE_LOGREC_COMMITTRX_INFO,
                                info,
                                trx->trx_usertrxid,
                                trx->trx_hsbg2mode);
                    check_logwaitrc = TRUE;
                } else {
                    ss_dprintf_4(("trx_end:dbe_log_puttrxmark:DBE_LOGREC_ABORTTRX_INFO\n"));
                    logrc = dbe_log_puttrxmark(
                                trx->trx_log,
                                trx->trx_cd,
                                DBE_LOGREC_ABORTTRX_INFO,
                                info,
                                trx->trx_usertrxid,
                                trx->trx_hsbg2mode);
                }

                if (rc == DBE_RC_SUCC && logrc != DBE_RC_SUCC) {
                    if (trx->trx_hsbcommitsent) {
                        /* Here commit phase1 is executed, allow only HSB
                         * connect failures. In such case, report success
                         * for the transaction because phase1 is already sent
                         * to secondary server.
                         */
                        su_rc_assert(logrc == SRV_ERR_HSBCONNBROKEN, logrc);
                    } else {
                        rc = logrc;
                    }
                }
            } else {
                ss_dassert(!trx->trx_hsbcommitsent);
            }
        } else {
            ss_dassert(!trx->trx_hsbcommitsent);
            ss_dassert(!trx->trx_hsbtrxmarkwrittentolog);
        }

        if (trx->trx_committrx_hsb) {
            dbe_db_commitdone(trx->trx_db, trx->trx_usertrxid,
                              (origrc == DBE_RC_SUCC) ? TRUE : FALSE);
        }

        if (rc != DBE_RC_SUCC) {
            dbe_trx_error_create(trx, rc, p_errh);
        }

        if (trx->trx_trdd != NULL) {
            trx->trx_nindexwrites += dbe_trdd_getnindexwrites(trx->trx_trdd);
            trx->trx_nmergewrites += dbe_trdd_getnindexwrites(trx->trx_trdd);
        }
        if (trx->trx_trdd != NULL && rc != DBE_RC_SUCC) {
            dbe_ret_t trddrc;
            trddrc = dbe_trdd_rollback(trx->trx_trdd);
            su_rc_assert(trddrc == DBE_RC_SUCC, trddrc);
        }

        dbe_trx_seqtransend_nomutex(trx, rc == DBE_RC_SUCC);

        if (dbe_cfg_newtrxwaitreadlevel 
            && SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_DELAYEDBEGINVALIDATE)) 
        {
            dbe_gtrs_entertrxgate(trx->trx_gtrs);
            trx_beginvalidate(trx);
            gtrs_gate_entered = TRUE;
        } else {
            gtrs_gate_entered = FALSE;
        }

        /* Mark the trx as ended. trx_info state is updated in
         * dbe_gtrs_endtrx.
         */
        dbe_gtrs_endtrx(
            trx->trx_gtrs,
            trx->trx_info,
            trx->trx_cd,
            rc == DBE_RC_SUCC,
            iswrites,
            trx->trx_nmergewrites,
            trx->trx_updatetrxinfo,
            !gtrs_gate_entered);

        if (abort_no_validate) {
            ss_dassert(!dbe_trxinfo_iscommitted(trx->trx_info));
            if (SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_USEREADLEVEL)) {
                dbe_gtrs_abortnovalidate(trx->trx_gtrs, trx->trx_info);
            } else {
                ss_dassert(trx->trx_mode == TRX_NOCHECK || !SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_DTABLE));
            }
            if (!trx->trx_trxaddedtotrxbuf) {
                trx->trx_trxaddedtotrxbuf = TRUE;
                dbe_trxbuf_add(trx->trx_trxbuf, trx->trx_info);
            }
        }

        trx->trx_errcode = rc;

        if (rs_sysi_gettrxcardininfo(trx->trx_cd) != NULL) {
            ss_dassert(dbe_trx_checkcardininfo(trx));

            dbe_trx_cardintrans_mutexif(trx->trx_cd, rc == DBE_RC_SUCC, FALSE, FALSE);

            ss_dassert(dbe_trx_checkcardininfo(trx));
        }

        /* Jarmo changed if-statement, Apr 15, 1995
           if (dbe_trxinfo_iscommitted(trx->trx_info) && trx->trx_mode != TRX_NOCHECK) {
        */
        if (dbe_trxinfo_iscommitted(trx->trx_info) && SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_DTABLE)) {
            ss_dassert(!trx->trx_trxaddedtotrxbuf ||
                       !DBE_TRXNUM_ISNULL(trx->trx_info->ti_committrxnum));
            ss_dassert(!trx->trx_trxaddedtotrxbuf ||
                       !DBE_TRXNUM_EQUAL(trx->trx_info->ti_committrxnum, DBE_TRXNUM_MAX));
        }
        if ((trx->trx_flags & (TRX_FLAG_DTABLE|TRX_FLAG_MTABLE)) == 0) {
            ss_dassert(su_list_length(&trx->trx_keychklist) == 0);
            ss_dassert(su_list_length(&trx->trx_readchklist) == 0);
            ss_dassert(su_list_length(&trx->trx_writechklist) == 0);
        }

        if (rollback) {
            ss_dassert(!dbe_trxinfo_iscommitted(trx->trx_info));
            ss_dassert(rc != DBE_RC_SUCC);
            trxtype = DBE_DB_TRXTYPE_ROLLBACK;
        } else if (rc == DBE_RC_SUCC) {
            ss_dassert(dbe_trxinfo_iscommitted(trx->trx_info) || !trx->trx_updatetrxinfo ||
                       (trx->trx_info->ti_actlistnode == NULL 
                        && (trx->trx_mode == TRX_NOWRITES || trx->trx_mode == TRX_READONLY)));
            trxtype = DBE_DB_TRXTYPE_COMMIT;
        } else {
            ss_dassert(!dbe_trxinfo_iscommitted(trx->trx_info));
            trxtype = DBE_DB_TRXTYPE_ABORT;
        }

        ss_dassert(trx->trx_updatetrxinfo || dbe_trxinfo_isbegin(trx->trx_info));

        dbe_gobj_addtrxstat(
            dbe_db_getgobj(trx->trx_db),
            trx->trx_cd,
            trxtype,
            count_trx,
            trx->trx_mode == TRX_NOWRITES || trx->trx_mode == TRX_READONLY,
            trx->trx_stmtcnt,
            trx->trx_nindexwrites,
            dbe_trxinfo_iscommitted(trx->trx_info) ? trx->trx_nlogwrites : 0);

#if defined(SS_MME) || !defined(SS_NOMMINDEX)
        if (trx->trx_mmll != NULL) {
            if (rc == DBE_RC_SUCC) {
                dbe_mmlocklst_commit(trx->trx_mmll);
            } else {
                dbe_mmlocklst_rollback(trx->trx_mmll);
            }
            /* MME locklist is no longer freed by commit/abort in MMEg2. */
#ifndef SS_MMEG2
            trx->trx_mmll = NULL;
#endif
        }
#endif /* SS_NOMMINDEX */

#ifndef SS_NODDUPDATE
        if (trx->trx_trdd != NULL) {
            dbe_ret_t   rc2;
            rc2 = dbe_trdd_cleanup(
                    trx->trx_trdd,
                    dbe_trxinfo_iscommitted(trx->trx_info));
            su_rc_assert(rc2 == DBE_RC_SUCC, rc2);
        }
#endif /* SS_NODDUPDATE */

        dbe_lockmgr_unlockall(
            trx->trx_lockmgr,
            trx->trx_locktran);

        trx->trx_commitst = TRX_COMMITST_DONE;

        if (check_logwaitrc) {
            logwaitrc = dbe_log_waitflushmes(
                            trx->trx_log,
                            trx->trx_cd);
            if (logwaitrc != DBE_RC_SUCC) {
                dbe_db_setoutofdiskspace(trx->trx_db, logwaitrc);
            }
        }

        if (entersemp) {
            dbe_trx_semexit(trx);
        }
        if (enteractionp) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        }

        ss_dprintf_3(("trx_end:end, rc = %d\n", rc));

#ifdef SS_MYSQL_PERFCOUNT
        if (mysql_enable_perfcount > 1) {
            QueryPerformanceCounter((LARGE_INTEGER*)&endcount);
            trx_end_perfcount += endcount - startcount;
            trx_end_callcount++;
        }
#endif /* SS_MYSQL_PERFCOUNT */ 

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_localrollback
 *
 * Rollbacks (aborts) a transaction.
 *
 * Parameters :
 *
 *      trx - in, take
 *              Transaction handle.
 *
 *      enteractionp - in
 *
 *
 *      p_errh - out, give
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_localrollback(
        dbe_trx_t* trx,
        bool enteractionp,
        bool count_trx,
        rs_err_t** p_errh)
{
        bool isreadonly;
        su_profile_timer;

        SS_NOTUSED(p_errh);

        ss_dprintf_3(("dbe_trx_localrollback:begin, trx->trx_mode=%d\n", trx->trx_mode));
        CHK_TRX(trx);
        ss_dassert(enteractionp || dbe_trx_semisentered(trx));

        su_profile_start;

        /* Make sure rollback is done only once.
         */
        if (enteractionp) {
            dbe_trx_sementer(trx);
        }
        if (trx->trx_rollbackdone) {
            if (enteractionp) {
                dbe_trx_semexit(trx);
            }
            return(DBE_RC_SUCC);
        } else {
            trx->trx_rollbackdone = TRUE;
        }
        if (enteractionp) {
            dbe_trx_semexit(trx);
        }

        if (!DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid)) {
            trx_stmt_localrollback(trx, enteractionp, FALSE, TRUE, NULL);
            ss_dassert(DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid));
        }

        isreadonly = trx->trx_mode == TRX_NOWRITES ||
                     trx->trx_mode == TRX_READONLY;

        ss_dassert(trx->trx_errcode != DBE_RC_SUCC);

#ifdef DBE_HSB_REPLICATION
        trx_rependtrx(trx, FALSE);
#endif /* DBE_HSB_REPLICATION */

        if (!SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_DTABLE)) {
            dbe_trxinfo_setmtabletrx(trx->trx_info);
        }

        trx_end(
            trx,
            trx->trx_errcode,
            trx->trx_log != NULL && !isreadonly,
            !isreadonly,
            TRUE,
            enteractionp,
            count_trx,
            NULL);

        su_profile_stop("dbe_trx_localrollback");

        ss_dprintf_4(("dbe_trx_localrollback:end\n"));

        return(DBE_RC_SUCC);
}

/*#***********************************************************************\
 *
 *              trx_validate
 *
 * Does transaction validation.
 *
 * Parameters :
 *
 *      trx - use
 *
 *      p_puttolog - out
 *          If TRUE, transaction end mark is added to the log file.
 *
 *      p_abort_no_validate - out
 *          If TRUE, the transaction has been aborted without validating it.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t trx_validate(
        dbe_trx_t* trx,
        bool* p_puttolog,
        bool* p_abort_no_validate)
{
        dbe_ret_t rc = 0;

        ss_dprintf_3(("trx_validate:begin\n"));
        ss_dassert(DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid));
        ss_dassert(dbe_trx_semisentered(trx));

        switch (dbe_trxinfo_getstate(trx->trx_info)) {

            case DBE_TRXST_BEGIN:
#ifdef DBE_REPLICATION
            case DBE_TRXST_TOBEABORTED:
#endif /* DBE_REPLICATION */
                if (trx->trx_mode == TRX_NOWRITES ||
                    trx->trx_mode == TRX_READONLY) {
                    /* JarmoR Nov 21, 2000
                     * Moved this check before trx->trx_errcode != DBE_RC_SUCC
                     * so that read-only transactions do not abort
                     * unnecessarily.
                     */
                    ss_dprintf_4(("trx_validate:no writes and trx is readonly\n"));
                    rc = DBE_RC_SUCC;
                    *p_puttolog = FALSE;
                    *p_abort_no_validate = FALSE;

                } else if (trx->trx_errcode != DBE_RC_SUCC) {

                    ss_dprintf_4(("trx_validate:error, trx is failed\n"));
                    rc = trx->trx_errcode;
                    *p_puttolog = trx->trx_log != NULL;
                    *p_abort_no_validate = TRUE;

                } else if (dbe_db_isreadonly(trx->trx_db)) {
                    ss_dprintf_4(("trx_validate:error, writes and trx is readonly\n"));
                    rc = DBE_ERR_TRXREADONLY;
                    trx->trx_errcode = rc;
                    *p_puttolog = trx->trx_log != NULL;
                    *p_abort_no_validate = TRUE;

                } else if (!SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_DTABLE)) {
                    /* Only M-tables in transaction or empty transaction. Separate validate 
                     * is not needed for M-tables.
                     */
                    ss_dassert(su_list_length(&trx->trx_keychklist) == 0);
                    ss_dassert(su_list_length(&trx->trx_readchklist) == 0);
                    ss_dassert(su_list_length(&trx->trx_writechklist) == 0);
                    if (DBE_TRXNUM_EQUAL(trx->trx_committrxnum, DBE_TRXNUM_NULL)) {
                        /* We always need to have committrxnum. */
                        trx->trx_committrxnum = dbe_counter_getnewcommittrxnum(trx->trx_counter);
                        ss_assert(!DBE_TRXNUM_EQUAL(trx->trx_committrxnum, DBE_TRXNUM_NULL));
                        ss_assert(!DBE_TRXNUM_EQUAL(trx->trx_committrxnum, DBE_TRXNUM_MAX));
                        trx->trx_info->ti_committrxnum = trx->trx_committrxnum;
                    }
                    dbe_trxinfo_setmtabletrx(trx->trx_info);
                    *p_puttolog = trx->trx_log != NULL;
                    *p_abort_no_validate = FALSE;
                    rc = DBE_RC_SUCC;

                } else {

                    ss_dprintf_4(("trx_validate:trx is readwrite\n"));
                    ss_dassert(SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_DTABLE));
                    trx_validatereadwrite_init(trx);
                    if (dbe_trxinfo_getstate(trx->trx_info) == DBE_TRXST_VALIDATE) {
                        /* trx->trx_info->ti_state is set to DBE_TRXST_VALIDATE
                           in trx_validatereadwrite_init */
                        rc = DBE_RC_CONT;
                    } else {
                        *p_puttolog = trx->trx_log != NULL;
                        *p_abort_no_validate = FALSE;
                        rc = DBE_RC_SUCC;
                    }
                }
                break;

            case DBE_TRXST_VALIDATE:
                rc = trx_validatereadwrite_advance(trx);
                if (rc != DBE_RC_CONT) {
                    *p_puttolog = trx->trx_log != NULL;
                    *p_abort_no_validate = FALSE;
                }
                break;

            case DBE_TRXST_COMMIT:
                ss_error;

            case DBE_TRXST_ABORT:
                /* JarmoR Nov 22, 2000
                 * Added handler also ABORT case, previously here was ss_error.
                 * In hsb transaction may get aborted during validate e.g.
                 * if there is a role switch.
                 */
                ss_dprintf_4(("trx_validate:error, trx is failed\n"));
                ss_assert(trx->trx_errcode != DBE_RC_SUCC);
                rc = trx->trx_errcode;
                *p_puttolog = trx->trx_log != NULL;
                *p_abort_no_validate = TRUE;
                break;

            default:
                ss_rc_error(dbe_trxinfo_getstate(trx->trx_info));

        }

        if (rc != DBE_RC_CONT) {
            trx->trx_errcode = rc;
        }

        ss_dprintf_3(("trx_validate:end, rc = %s (%d)\n", su_rc_nameof(rc), rc));

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_commit_step_nomutex
 *
 * Advances commit one atomic step.
 *
 * Parameters :
 *
 *      trx - in, take
 *              Transaction handle.
 *
 *      p_errh - out, give
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_UNIQUE
 *      DBE_ERR_CHILDEXIST
 *      DBE_ERR_PARENTNOTEXIST
 *      DBE_ERR_LOSTUPDATE
 *      DBE_ERR_NOTSERIALIZABLE
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trx_commit_step_nomutex(
        dbe_trx_t* trx,
        bool* p_contp,
        rs_err_t** p_errh)
{
        dbe_ret_t rc = 0;
        bool isreadonly;

        CHK_TRX(trx);
        ss_dassert(dbe_trx_semisentered(trx));
        ss_dprintf_3(("trx_commit_step_nomutex:begin, trx->trx_commitst=%d\n", (int)trx->trx_commitst));

        *p_contp = TRUE;

        switch (trx->trx_commitst) {

            case TRX_COMMITST_INIT:
                trx->trx_commitst = TRX_COMMITST_RUNDDOP;
                /* FALLTHROUGH */

            case TRX_COMMITST_RUNDDOP:
                ss_dprintf_4(("trx_commit_step_nomutex:TRX_COMMITST_RUNDDOP\n"));
                isreadonly = trx->trx_mode == TRX_NOWRITES ||
                            trx->trx_mode == TRX_READONLY;
                if (isreadonly) {
                    ss_dassert(dbe_trdd_listlen(trx->trx_trdd) == 0);
                    trx->trx_commitst = TRX_COMMITST_VALIDATE;
                    rc = DBE_RC_SUCC;
                } else {
                    if (trx->trx_trdd != NULL
                        && trx->trx_errcode == DBE_RC_SUCC) {
                        /* There are data dictionary operations.
                         */
                        ss_dassert(!rs_sysi_testflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY));
                        trx->trx_puttolog = TRUE;
                        rc = dbe_trdd_commit_advance(
                                trx->trx_trdd, &trx->trx_ddopact);
                    } else {
                        rc = DBE_RC_SUCC;
                    }
                    switch (rc) {
                        case DBE_RC_CONT:
                            break;
                        case DBE_RC_SUCC:
                            trx->trx_commitst = TRX_COMMITST_VALIDATE;
                            rc = DBE_RC_CONT;
                            break;
                        default:
                            break;
                    }
                }
                FAKE_CODE_BLOCK_GT(FAKE_DBE_SEQUENCE_RECOVERY_BUG, 0,
                {
                    *p_contp = FALSE;
                }
                );
                break;

            case TRX_COMMITST_VALIDATE:
                ss_dprintf_4(("trx_commit_step_nomutex:TRX_COMMITST_VALIDATE\n"));
                rc = trx_validate(
                        trx,
                        &trx->trx_puttolog,
                        &trx->trx_abort_no_validate);
                switch (rc) {
                    case DBE_RC_CONT:
                        break;
                    case DBE_RC_SUCC:
                        ss_dassert(trx->trx_errcode == DBE_RC_SUCC);

#ifdef SS_HSBG2
                        FAKE_CODE_BLOCK(FAKE_HSB_WAITAFTERCOMMIT, {
                            rc = DBE_RC_CONT;
                            *p_contp = FALSE;
                            break;
                        });
#endif /* SS_HSBG2 */

#ifdef DBE_HSB_REPLICATION
                        trx->trx_commitst = TRX_COMMITST_REPINIT;
                        rc = DBE_RC_CONT;
#endif /* DBE_HSB_REPLICATION */
                        break;
                    default:
                        ss_dassert(trx->trx_errcode == rc);
                        break;
                }
                break;

#ifdef DBE_HSB_REPLICATION
            case TRX_COMMITST_REPINIT:
                ss_dprintf_4(("trx_commit_step_nomutex:TRX_COMMITST_REPINIT\n"));
                if (trx->trx_errcode == DBE_RC_SUCC) {
                    rc = trx_repchecktrx(trx);
                    if (rc == DBE_RC_SUCC) {
                        rc = dbe_trx_seqcommit_nomutex(trx);
                    }
                } else {
                    rc = trx->trx_errcode;
                }
                if (rc == DBE_RC_SUCC) {
                    rc = DBE_RC_CONT;
                    trx->trx_commitst = TRX_COMMITST_REPEXEC;
                }
                break;

            case TRX_COMMITST_REPEXEC:
                ss_dprintf_4(("trx_commit_step_nomutex:TRX_COMMITST_REPEXEC\n"));
                rc = trx_rependtrx(trx, trx->trx_errcode == DBE_RC_SUCC);
                *p_contp = FALSE;
                if (rc == DBE_RC_SUCC &&
                         trx->trx_committrx_hsb &&
                         trx->trx_errcode == DBE_RC_SUCC) {
                    rc = DBE_RC_CONT;
                    trx->trx_commitst = TRX_COMMITST_REPFLUSH;
                    /* Even though we only add the commit to the oplist,
                     * we treat it the same as we already sent the commit.
                     * because we don't know when the group commit will send
                     * the commit to secondary.
                     */
                    dbe_gtrs_settrxuncertain(trx->trx_gtrs, trx->trx_info);
                    trx->trx_hsbcommitsent = TRUE;
                } else if (rc != DBE_RC_SUCC && rc != DBE_RC_CONT) {
                    /* this only happens when commit is add to oplist but
                     * failed to sent the commit to secondary.
                     */
                    trx->trx_hsbcommitsent = FALSE;
                }
                break;

            case TRX_COMMITST_REPFLUSH:
                ss_dprintf_4(("trx_commit_step_nomutex:TRX_COMMITST_REPFLUSH\n"));
                FAKE_CODE_BLOCK(FAKE_HSB_WAITAFTERCOMMIT,
                {
                    rc = DBE_RC_CONT;
                    *p_contp = FALSE;
                    break;
                });

                rc = trx_repflushcommit(trx);
                *p_contp = FALSE;
                if (rc == DBE_RC_SUCC &&
                        trx->trx_committrx_hsb &&
                        trx->trx_errcode == DBE_RC_SUCC) {
                    rc = DBE_RC_CONT;
                    trx->trx_commitst = TRX_COMMITST_REPREADY;
                } else if (rc != DBE_RC_SUCC && rc != DBE_RC_CONT) {
                    /* this only happens when commit is add to oplist but
                     * failed to sent the commit to secondary.
                     */
                    trx->trx_hsbcommitsent = FALSE;
                }
                break;

            case TRX_COMMITST_REPREADY:
                ss_dprintf_4(("trx_commit_step_nomutex:TRX_COMMITST_REPREADY\n"));
                *p_contp = FALSE;
                rc = dbe_db_commitready(trx->trx_db, trx->trx_usertrxid);
                break;
#endif /* DBE_HSB_REPLICATION */

            default:
                ss_error;
        }

        if (rc != DBE_RC_CONT) {
            rc = trx_end(
                    trx,
                    rc,
                    trx->trx_puttolog,
                    trx->trx_abort_no_validate,
                    FALSE,
                    FALSE,
                    TRUE,
                    p_errh);
        }

        ss_dprintf_4(("trx_commit_step_nomutex:end, rc = %s (%d)\n", su_rc_nameof(rc), rc));

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_waitreadlevel
 *
 * Check that current read level containts the current transaction.
 * If not, wait until read level is high enough.
 *
 * Parameters :
 *
 *      trx - use
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
static dbe_ret_t trx_waitreadlevel(dbe_trx_t* trx)
{
        dbe_ret_t rc;
        bool waitreadlevel;
        ss_debug(static bool dbg_set = FALSE;)

        ss_dprintf_3(("trx_waitreadlevel:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(trx->trx_commitst == TRX_COMMITST_WAITREADLEVEL || trx->trx_commitst == TRX_COMMITST_DONE);

        if (DBE_TRXNUM_ISNULL(trx->trx_committrxnum)
            || !SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_DTABLE)) 
        {
            /* Read-only transaction or only M-tables.
             */
            waitreadlevel = FALSE;

        } else {
            dbe_trxnum_t maxtrxnum;

            maxtrxnum = dbe_counter_getmaxtrxnum(trx->trx_counter);

            FAKE_CODE_BLOCK(FAKE_DBE_NOCOMMITREADLEVELWAIT, { maxtrxnum = trx->trx_committrxnum; });

            ss_dprintf_4(("trx_waitreadlevel:maxtrxnum = %ld (readlevel), committrxnum = %ld\n",
                DBE_TRXNUM_GETLONG(maxtrxnum), DBE_TRXNUM_GETLONG(trx->trx_committrxnum)));

            waitreadlevel = (DBE_TRXNUM_CMP_EX(maxtrxnum, trx->trx_committrxnum) < 0);
        }

        if (waitreadlevel) {

#ifdef DBE_REPLICATION
            SsSemT* hsb_mutex;
            bool eventwait = FALSE;
#endif /* DBE_REPLICATION */

            /* Read level does not contain this transaction,
             * wait until read level is high enough.
             *
             * NOTE! WARNING! FUTURE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
             * THE CURRENT WAIT IS A BUSY LOOP WAIT, IT SHOULD BE REPLACED
             * WITH A PROPER WAITING MECHANISM.
             */
            ss_dprintf_4(("trx_waitreadlevel:start to wait\n"));

#ifdef DBE_REPLICATION
            /*
             * If there are transactions with unresolved status waiting,
             * their state may not be resolved until operator has
             * changed the HSB state.
             *
             */

            hsb_mutex = dbe_db_hsbsem_get(trx->trx_db);

            SsSemEnter(hsb_mutex);

            if (dbe_db_gethsbunresolvedtrxs(trx->trx_db)) {

                /* initialize back function pointers for event wait */
                dbe_server_sysi_init_functions(trx->trx_cd);

                rs_sysi_eventwaitwithtimeout(
                    trx->trx_cd,
                    SSE_EVENT_HSBPRIMARYSTATUS,
                    0L,         /* timeout */
                    NULL,       /* timeout function pointer */
                    NULL        /* timeout function context */
                );

                eventwait = TRUE;
            }

            SsSemExit(hsb_mutex);

#endif /* DBE_REPLICATION */

#if defined(SS_DEBUG)
#ifdef DBE_REPLICATION
            if(eventwait) {
                trx->trx_waitreadlevelstarttime = 0;
            } else {
#endif /* DBE_REPLICATION */
                /*
                 * Wait max 60 seconds for the read level.
                 *
                 * Now enabled also for HSB, disable again if causes problems (such as this asserts before hsb timeout).
                 *
                 */
                if (trx->trx_waitreadlevelstarttime == 0) {
                    trx->trx_waitreadlevelstarttime = SsTime(NULL);
                }
                if (!dbg_set && SsTime(NULL) > (unsigned long)(trx->trx_waitreadlevelstarttime + 10)) {
                    SsDbgSet("/LEV:4/FIL:dbe0trx,dbe7gtrs,su0err/LOG/NOD/THR/TIM/SQL/TASK/LIM:100000000");
                    dbg_set = TRUE;
                }
                if (SsTime(NULL) > (unsigned long)(trx->trx_waitreadlevelstarttime + 60)) {
                    SsDbgSet("/LEV:0/NOL");
                    ss_error;
                }
#ifdef DBE_REPLICATION
            }
#endif /* DBE_REPLICATION */

#endif /* SS_DEBUG */

             SS_PMON_ADD(SS_PMON_WAITREADLEVEL_COUNT);
            trx->trx_commitst = TRX_COMMITST_WAITREADLEVEL;

#if defined(DBE_REPLICATION)
            if(!eventwait) {
                if (trx->trx_waitreadlevelloopcnt < DBE_BUSYPOLL_MAXLOOP) {
                    ss_pprintf_4(("trx_waitreadlevel:SsThrSwitch, loop=%d\n", trx->trx_waitreadlevelloopcnt));
                    SsThrSwitch();
                } else {
                    ss_pprintf_4(("trx_waitreadlevel:SsThrSleep, loop=%d, sleep=%d\n", 
                        trx->trx_waitreadlevelloopcnt, 
                        trx->trx_waitreadlevelloopcnt < DBE_BUSYPOLL_MAXSLEEP ? trx->trx_waitreadlevelloopcnt : DBE_BUSYPOLL_MAXSLEEP));
                    SsThrSleep(trx->trx_waitreadlevelloopcnt < DBE_BUSYPOLL_MAXSLEEP ? trx->trx_waitreadlevelloopcnt : DBE_BUSYPOLL_MAXSLEEP);
                }
                trx->trx_waitreadlevelloopcnt++;
            }
#endif /* DBE_REPLICATION */

            rc = DBE_RC_CONT;

        } else {
            /* This transaction is inside the current read level,
             * we are done. Note that in read-only transactions the
             * trx_committrxnum is zero (it is never updated),
             * so they came directly to this branch and never wait.
             */

            trx->trx_commitst = TRX_COMMITST_DONE;

            ss_dprintf_4(("trx_waitreadlevel:end of wait\n"));
            rc = DBE_RC_SUCC;
#ifdef SS_DEBUG
            if (dbg_set) {
                SsDbgSet("/LEV:0/NOL");
                ss_debug(dbg_set = FALSE;)
            }
#endif /* SS_DEBUG */
        }
        return(rc);
}


/*#***********************************************************************\
 *
 *              trx_stmt_cleanup
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx -
 *
 *
 *      trop -
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
static void trx_stmt_cleanup(
        void* cd,
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_trop_t trop)
{
        ss_dprintf_2(("trx_stmt_cleanup:userid = %d, trop = %d\n", dbe_user_getid(trx->trx_user), (int)trop));
        rs_trend_stmttransend(rs_sysi_getstmttrend(cd), cd, trop);
}

/*#***********************************************************************\
 *
 *              trx_cleanup
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx -
 *
 *
 *      trop -
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
static dbe_ret_t trx_cleanup(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_trop_t trop)
{

        su_ret_t rc;

        ss_dprintf_2(("trx_cleanup:userid = %d, trop = %d\n", dbe_user_getid(trx->trx_user), (int)trop));
        ss_dassert(trx->trx_tropst != TRX_TROPST_DONE);

        rc = rs_trend_transend(rs_sysi_gettrend(cd), cd, trop);

        switch (trop) {
            case RS_TROP_BEFORECOMMIT:
            case RS_TROP_BEFOREROLLBACK:
                ss_dassert(trx->trx_tropst == TRX_TROPST_BEFORE);
                ss_dassert(rc == DBE_RC_SUCC);
                trx->trx_tropst = TRX_TROPST_MIDDLE;
                break;
            case RS_TROP_AFTERROLLBACK:
                ss_dassert(trx->trx_tropst == TRX_TROPST_MIDDLE);
                ss_dassert(rc == DBE_RC_SUCC);
                /* FALLTHROUGH */
            case RS_TROP_AFTERCOMMIT:
                ss_dassert(trx->trx_tropst == TRX_TROPST_MIDDLE || trx->trx_tropst == TRX_TROPST_AFTER);
                ss_dassert(rc == DBE_RC_SUCC || rc == DBE_RC_CONT);
                trx->trx_tropst = TRX_TROPST_AFTER;
                if (rc != DBE_RC_CONT) {
                    trx->trx_tropst = TRX_TROPST_DONE;
                } else {
                    SS_RTCOVERAGE_INC(SS_RTCOV_TRX_AFTERCOMMIT_CONT);
                }
                break;
            default:
                break;
        }

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_commit
 *
 * Tries to commit a transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 *      waitp - in
 *              If TRUE, wait until current read level contains the
 *              committed transaction.
 *
 *      p_errh - out, give
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_UNIQUE
 *      DBE_ERR_CHILDEXIST
 *      DBE_ERR_PARENTNOTEXIST
 *      DBE_ERR_LOSTUPDATE
 *      DBE_ERR_NOTSERIALIZABLE
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trx_commit(
        dbe_trx_t* trx,
        bool waitp,
        rs_err_t** p_errh)
{
        int nloop;
        bool contp = TRUE;
        dbe_ret_t rc;
        bool enterp = FALSE;
        su_profile_timer;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_commit:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));
        SS_PUSHNAME("dbe_trx_commit");
        ss_dassert(!trx->trx_rp.rp_activep ||
                   trx->trx_rp.rp_type == REP_COMMIT ||
                   trx->trx_rp.rp_type == REP_COMMIT_NOFLUSH);

        if (trx->trx_commitst == TRX_COMMITST_WAITREADLEVEL) {
            /* Wait until read level contains this transaction.
             */
            ss_dassert(waitp);
            rc = trx_waitreadlevel(trx);
            ss_dprintf_2(("dbe_trx_commit:end of wait read level, rc = %s (%d)\n", su_rc_nameof(rc), rc));
            ss_rc_dassert(rc == DBE_RC_CONT ? trx->trx_commitst == TRX_COMMITST_WAITREADLEVEL : trx->trx_commitst == TRX_COMMITST_DONE, trx->trx_commitst);
            SS_POPNAME;
            return(rc);
        }

        if (trx->trx_trdd != NULL
            && trx->trx_commitst == TRX_COMMITST_INIT) 
        {
            dbe_trdd_startcommit(trx->trx_cd, trx, trx->trx_trdd);
        }

        if (trx->trx_mode != TRX_NOWRITES && trx->trx_mode != TRX_READONLY) {
            dbe_db_enteraction(trx->trx_db, trx->trx_cd);
            enterp = TRUE;
        }
        dbe_trx_sementer(trx);

        if (trx->trx_rollbackdone) {
            ss_dassert(trx->trx_errcode != DBE_RC_SUCC);
            rc = trx->trx_errcode;
            dbe_trx_semexit(trx);
            if (enterp) {
                dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            }
            dbe_trx_error_create(trx, rc, p_errh);
            dbe_trx_rollback(trx, TRUE, NULL);
            ss_dprintf_2(("dbe_trx_commit:end, rc = %s (%d) (rollback)\n", su_rc_nameof(rc), rc));
            SS_POPNAME;
            return(rc);
        }

        su_profile_start;

        for (nloop = 0; nloop < DBE_MAXLOOP * 10 && contp; nloop++) {
            if (!DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid)) {
                ss_dassert(trx->trx_groupstmtlist == NULL);
                for (;;) {
                    rc = trx_stmt_commit_step(trx, FALSE, p_errh);
                    if (rc != DBE_RC_CONT) {
                        break;
                    }
                }
                trx_stmt_cleanup(trx->trx_cd, trx, rc == DBE_RC_SUCC ? RS_TROP_AFTERSTMTCOMMIT : RS_TROP_AFTERSTMTROLLBACK);
                if (enterp) {
                    dbe_trx_abortif_nomutex(trx);
                }
                if (rc == DBE_RC_SUCC) {
                    ss_dassert(DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid));
                    rc = DBE_RC_CONT;
                } else if (rc != DBE_RC_CONT) {
                    rc = trx_end(trx, rc, TRUE, TRUE, FALSE, FALSE, TRUE, NULL);
                }
            } else {
                rc = trx_commit_step_nomutex(trx, &contp, p_errh);
            }
            if (rc != DBE_RC_CONT) {
                break;
            }
        }

        if (trx->trx_commitst == TRX_COMMITST_DONE) {
            ss_dassert(rc != DBE_RC_CONT);
            if (trx->trx_openflag != NULL) {
                *trx->trx_openflag = FALSE;
            }
        }

        dbe_trx_semexit(trx);

        if (trx->trx_trdd != NULL && rc != DBE_RC_CONT) {
            dbe_trdd_unlinkblobs(trx->trx_trdd);
        }

        if (enterp) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        }

        if (rc == DBE_RC_SUCC && waitp) {
            /* Wait until read level contains this transaction.
             */
            ss_dassert(trx->trx_commitst == TRX_COMMITST_DONE);
            rc = trx_waitreadlevel(trx);
            ss_rc_dassert(rc == DBE_RC_CONT ? trx->trx_commitst == TRX_COMMITST_WAITREADLEVEL : trx->trx_commitst == TRX_COMMITST_DONE, trx->trx_commitst);
        }

        su_profile_stop("dbe_trx_commit");
        ss_dprintf_2(("dbe_trx_commit:end, rc = %s (%d)\n", su_rc_nameof(rc), rc));
        SS_POPNAME;
        return(rc);
}


/*##**********************************************************************\
 *
 *              dbe_trx_commit
 *
 * Tries to commit a transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 *      waitp - in
 *              If TRUE, wait until current read level contains the
 *              committed transaction.
 *
 *      p_errh - out, give
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_UNIQUE
 *      DBE_ERR_CHILDEXIST
 *      DBE_ERR_PARENTNOTEXIST
 *      DBE_ERR_LOSTUPDATE
 *      DBE_ERR_NOTSERIALIZABLE
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_commit(
        dbe_trx_t* trx,
        bool waitp,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;

        CHK_TRX(trx);

        if ((trx->trx_mode == TRX_NOWRITES || trx->trx_mode == TRX_READONLY)
            && trx->trx_trdd == NULL
            && !rs_trend_isfun(rs_sysi_gettrend(trx->trx_cd))
            && !trx->trx_rollbackdone
            && trx->trx_errcode == DBE_RC_SUCC) 
        {
            ss_dassert(!trx->trx_hsbcommitsent);
            ss_dassert(trx->trx_commitst != TRX_COMMITST_WAITREADLEVEL);

            rc = trx_end(
                    trx,
                    SU_SUCCESS,
                    FALSE,      /* puttolog */
                    FALSE,      /* abort_no_validate */
                    FALSE,      /* rollback */
                    TRUE,       /* enteractionp */
                    TRUE,       /* count_trx */
                    p_errh);

            su_rc_dassert(rc == SU_SUCCESS, rc);

            trx->trx_tropst = TRX_TROPST_DONE;

            dbe_trx_sementer(trx);
            if (trx->trx_openflag != NULL) {
                *trx->trx_openflag = FALSE;
            }
            dbe_trx_semexit(trx);

            return(rc);
        }

        if (trx->trx_tropst == TRX_TROPST_BEFORE) {
            trx_cleanup(trx->trx_cd, trx, RS_TROP_BEFORECOMMIT);
        }
        if (trx->trx_tropst == TRX_TROPST_AFTER) {
            ss_dprintf_2(("dbe_trx_commit:trx->trx_tropst == TRX_TROPST_AFTER\n"));
            ss_dassert(trx->trx_commitst == TRX_COMMITST_DONE);
            ss_dassert(trx->trx_errcode == DBE_RC_SUCC);
            rc = trx_cleanup(trx->trx_cd, trx, RS_TROP_AFTERCOMMIT);
            ss_dassert(rc == DBE_RC_SUCC || rc == DBE_RC_CONT);
            return(rc);
        }

        ss_dassert(!trx->trx_opactivep);
        ss_debug(trx->trx_opactivep = TRUE);

        rc = trx_commit(trx, waitp, p_errh);
        ss_dprintf_2(("dbe_trx_commit:rc=%d\n", rc));

        ss_debug(trx->trx_opactivep = FALSE);

        if (rc != DBE_RC_CONT) {
            ss_dprintf_2(("dbe_trx_commit:NOT DBE_RC_CONT:rc=%d\n", rc));
            ss_dassert(trx->trx_commitst == TRX_COMMITST_DONE);
            ss_dassert(rc == DBE_RC_SUCC ? trx->trx_errcode == DBE_RC_SUCC : trx->trx_errcode != DBE_RC_SUCC);
            if (rc == DBE_RC_SUCC) {
                rc = trx_cleanup(trx->trx_cd, trx, RS_TROP_AFTERCOMMIT);
                ss_dassert(rc == DBE_RC_SUCC || rc == DBE_RC_CONT);
            } else if (trx->trx_tropst != TRX_TROPST_DONE) {
                trx_cleanup(trx->trx_cd, trx, RS_TROP_AFTERROLLBACK);
            }
        }

        return(rc);
}


/*##**********************************************************************\
 *
 *              dbe_trx_rollback
 *
 * Rollbacks (aborts) a transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 *      p_errh - out, give
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_rollback(
        dbe_trx_t* trx,
        bool count_trx,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_rollback:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));
        SS_NOTUSED(p_errh);
        SS_PUSHNAME("dbe_trx_rollback");

        if (trx->trx_hsbcommitsent) {
            ss_dprintf_2(("dbe_trx_rollback:DBE_ERR_ABORTHSBTRXFAILED\n"));
            SS_POPNAME;
            return DBE_ERR_ABORTHSBTRXFAILED;
        }

        if (trx->trx_commitst == TRX_COMMITST_WAITREADLEVEL) {
            ss_dprintf_2(("dbe_trx_rollback:end, trx->trx_commitst == TRX_COMMITST_WAITREADLEVEL\n"));
            trx->trx_commitst = TRX_COMMITST_DONE;
            return(DBE_RC_SUCC);
        }

        if (trx->trx_rp.rp_activep
        &&  trx->trx_rp.rp_donep
        &&  trx->trx_rp.rp_type != REP_ABORT)
        {
            /* Operation is active but it is not trx rollback.
             */
            trx->trx_rp.rp_activep = FALSE;
        }
        ss_dassert(!trx->trx_rp.rp_activep
                    || trx->trx_rp.rp_type == REP_ABORT
                    || trx->trx_rp.rp_type == REP_COMMIT
                    || trx->trx_rp.rp_type == REP_COMMIT_NOFLUSH);

        if (trx->trx_tropst == TRX_TROPST_BEFORE) {
            trx_cleanup(trx->trx_cd, trx, RS_TROP_BEFOREROLLBACK);
        }

        if (trx->trx_errcode == SU_SUCCESS) {
            trx->trx_errcode = DBE_ERR_USERROLLBACK;
        }

        if (!trx->trx_rollbackdone) {
            rc = dbe_trx_localrollback(trx, TRUE, count_trx, p_errh);
        } else {
            rc = DBE_RC_SUCC;
        }

        dbe_trx_sementer(trx);
        if (trx->trx_openflag != NULL) {
            *trx->trx_openflag = FALSE;
        }
        dbe_trx_semexit(trx);

        trx_cleanup(trx->trx_cd, trx, RS_TROP_AFTERROLLBACK);
        ss_dprintf_2(("dbe_trx_rollback:end, rc=%d\n", rc));
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_done
 *
 * Releases transaction handle after commit or rollback.
 *
 * Parameters :
 *
 *      trx - in, take
 *              Transaction handle.
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
void dbe_trx_done(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_done:userid = %d\n", dbe_user_getid(trx->trx_user)));
        dbe_trx_seterrkey(trx, NULL);

        trx_freemem(trx, FALSE, FALSE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_donebuf
 *
 * Releases transaction handle after commit or rollback.
 * Uses pre-allocated transaction buffer.
 *
 * Parameters :
 *
 *      trx - in, take
 *              Transaction handle.
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
void dbe_trx_donebuf(
        dbe_trx_t*  trx,
        bool        issoft,
        bool        isactive)
{
        if (isactive) {
            CHK_TRX(trx);
        }
        ss_dprintf_1(("dbe_trx_donebuf:userid = %d\n", isactive ? dbe_user_getid(trx->trx_user) : -1));

        if (!isactive) {
            if (trx->trx_mmll != NULL) {
                dbe_mmlocklst_replicafree(trx->trx_mmll);
            }
            trx->trx_mmll = NULL;
            
            if (trx->trx_infocache != NULL) {
                /* Some kludges for debugging purposes. */
                ss_dassert(trx->trx_infocache->ti_nlinks == 1);
                ss_debug(trx->trx_infocache->ti_chk = DBE_CHK_TRXINFO);
                dbe_trxinfo_done(
                        trx->trx_infocache,
                        trx->trx_cd,
                        dbe_trxbuf_getsembytrxid(
                                trx->trx_trxbuf,
                                trx->trx_infocache->ti_usertrxid));
            }
            trx->trx_infocache = NULL;
        } else {
            trx_freemem(trx, TRUE, issoft);
        }
}

#ifdef DBE_REPLICATION
/*##**********************************************************************\
 *
 *              dbe_trx_replicaend
 *
 * Ends a replication transaction, the transaction is ended as an
 * uncertain transaction.
 *
 * Parameters :
 *
 *      trx - in, take
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
void dbe_trx_replicaend(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_replicaend:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));

        ss_dassert(trx->trx_commitst != TRX_COMMITST_WAITREADLEVEL);
        ss_dassert(!trx->trx_rollbackdone);
        ss_dassert(trx->trx_commitst != TRX_COMMITST_DONE);
        ss_dassert(trx->trx_replicaslave);

        ss_error;

        /* dbe_gtrs_quitreplicatrx(trx->trx_gtrs, trx->trx_info); */
        if (!trx->trx_trxaddedtotrxbuf) {
            trx->trx_trxaddedtotrxbuf = TRUE;
            dbe_trxbuf_add(trx->trx_trxbuf, trx->trx_info);
        }

#ifndef SS_NODDUPDATE
        if (trx->trx_trdd != NULL) {
            dbe_ret_t rc;

            rc = dbe_trdd_cleanup(
                    trx->trx_trdd,
                    dbe_trxinfo_iscommitted(trx->trx_info));
            su_rc_assert(rc == DBE_RC_SUCC, rc);
        }
#endif /* SS_NODDUPDATE */

#ifndef SS_NOLOCKING
        dbe_lockmgr_unlockall(
            trx->trx_lockmgr,
            trx->trx_locktran);
#endif /* SS_NOLOCKING */

        ss_dassert(dbe_trx_checkcardininfo(trx));

        dbe_trx_cardinstmttrans_mutexif(trx->trx_cd, FALSE, FALSE, FALSE);

        ss_dassert(dbe_trx_checkcardininfo(trx));

        if (rs_sysi_gettrxcardininfo(trx->trx_cd) != NULL) {
            ss_dassert(dbe_trx_checkcardininfo(trx));

            dbe_trx_cardintrans_mutexif(trx->trx_cd, FALSE, FALSE, FALSE);
            
            ss_dassert(dbe_trx_checkcardininfo(trx));
        }
        trx_freemem(trx, FALSE, FALSE);

        ss_dprintf_2(("dbe_trx_replicaend:end\n"));
}

#endif /* DBE_REPLICATION */

/*#***********************************************************************\
 *
 *              trx_stmt_removeinfo
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      stmt_succp -
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
static void trx_stmt_removeinfo(dbe_trx_t* trx, bool stmt_succp)
{
        su_list_node_t* n;
        su_list_node_t* next_n;
        trx_writecheck_t* wchk;
        trx_keycheck_t* kchk;

        ss_dprintf_3(("trx_stmt_removeinfo:stmt_succp = %d\n", stmt_succp));
        ss_dassert(dbe_trx_semisentered(trx));

#ifndef SS_NOTRXREADCHECK
        if (!stmt_succp) {
            /* Remove read check info.
             */
            trx_readcheck_t* rchk;
            n = su_list_first(&trx->trx_readchklist);
            while (n != NULL) {
                rchk = su_listnode_getdata(n);
                if (!DBE_TRXID_EQUAL(rchk->rchk_stmttrxid, trx->trx_stmttrxid)) {
                    /* No more checks for this statement. */
                    break;
                }
                su_list_removefirst(&trx->trx_readchklist);
                n = su_list_first(&trx->trx_readchklist);
            }
        }
#endif /* SS_NOTRXREADCHECK */

        /* Remove write check info.
         */
        n = su_list_first(&trx->trx_writechklist);
        while (n != NULL) {
            wchk = su_listnode_getdata(n);
            if (!DBE_TRXID_EQUAL(wchk->wchk_stmttrxid, trx->trx_stmttrxid)) {
                /* No more checks for this statement. */
                break;
            }
            su_list_removefirst(&trx->trx_writechklist);
            n = su_list_first(&trx->trx_writechklist);
        }
        /* Remove key check info.
         */
        n = su_list_first(&trx->trx_keychklist);
        while (n != NULL) {
            next_n = su_list_next(&trx->trx_keychklist, n);
            kchk = su_listnode_getdata(n);
            if (DBE_TRXID_EQUAL(kchk->kchk_stmttrxid, trx->trx_stmttrxid)) {
                /* Remove this check. */
                su_list_remove(&trx->trx_keychklist, n);
            }
            n = next_n;
        }
}

/*#***********************************************************************\
 *
 *              trx_stmt_begin
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
static void trx_stmt_begin(dbe_trx_t* trx, bool replicap)
{
        CHK_TRX(trx);
        ss_dprintf_3(("trx_stmt_begin:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(dbe_trx_semisentered(trx));

        dbe_trx_ensureusertrxid(trx);

        if (replicap && trx->trx_replicarecovery) {
            trx->trx_replicarecovery = FALSE;
            trx->trx_stmttrxid = trx->trx_usertrxid;
        }

        if (!trx->trx_rollbackdone &&
            DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid))
        {
            ss_dassert(DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid));

            trx->trx_stmtcnt++;

#ifdef SS_HSBG2
            if (((trx->trx_hsbg2mode == DBE_HSB_SECONDARY && !ss_convertdb)
                 || trx->trx_hsbg2mode == DBE_HSB_PRIMARY_UNCERTAIN)
                && !trx->trx_migratehsbg2)
            {
                trx->trx_stmttrxid = DBE_TRXID_SUM(trx->trx_stmttrxid, 1);
                if (DBE_TRXID_EQUAL(trx->trx_stmttrxid, DBE_TRXID_NULL)) {
                    trx->trx_stmttrxid = DBE_TRXID_INIT(1);
                }
            } else {
                trx->trx_stmttrxid = dbe_counter_getnewtrxid(trx->trx_counter);
            }
#else /* SS_HSBG2 */
            trx->trx_stmttrxid = dbe_counter_getnewtrxid(trx->trx_counter);
#endif /* SS_HSBG2 */
            ss_dassert(DBE_TRXID_CMP_EX(trx->trx_usertrxid, trx->trx_stmttrxid) <= 0);
            trx->trx_hsbstmtmarkwrittentolog = FALSE;
            trx->trx_stmtiswrites = FALSE;
#ifndef SS_NODDUPDATE
            if (trx->trx_trdd != NULL) {
                dbe_trdd_stmt_begin(trx->trx_trdd, trx->trx_stmttrxid);
            }
#endif /* SS_NODDUPDATE */
            trx->trx_vldst = TRX_VLDST_INIT;
            if (replicap) {
                trx->trx_stmtaddedtotrxbuf = TRUE;
                dbe_trxbuf_addstmt(
                    trx->trx_trxbuf,
                    trx->trx_stmttrxid,
                    trx->trx_info);
            } else {
                trx->trx_stmtaddedtotrxbuf = FALSE;
            }
#ifdef DBE_REPLICATION
            trx->trx_stmterrcode = DBE_RC_SUCC;
#endif /* DBE_REPLICATION */

            if (trx->trx_migratehsbg2 && !ss_migratehsbg2) {
                dbe_trx_setfailed_nomutex(trx, DBE_ERR_FAILED);
            }
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_stmt_begin
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
void dbe_trx_stmt_begin(dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_stmt_begin:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);

        ss_dprintf_1(("dbe_trx_stmt_begin: inside mutex.\n"));
        trx_stmt_begin(trx, FALSE);

        dbe_trx_semexit(trx);
}

#ifdef DBE_REPLICATION

/*##**********************************************************************\
 *
 *              dbe_trx_stmt_beginreplica
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
void dbe_trx_stmt_beginreplica(dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_stmt_beginreplica:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);

        trx_stmt_begin(trx, TRUE);

        dbe_trx_semexit(trx);
}

/*##**********************************************************************\
 *
 *              dbe_trx_stmt_beginreplicarecovery
 *
 * Begins a new statement after replication recovery. Unfinished
 * replica statements are restarted after recovery.
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
void dbe_trx_stmt_beginreplicarecovery(dbe_trx_t* trx, dbe_trxid_t stmttrxid)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_stmt_beginreplicarecovery:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);

        trx->trx_stmttrxid = stmttrxid;
#ifndef SS_NODDUPDATE
        if (trx->trx_trdd != NULL) {
            dbe_trdd_stmt_begin(trx->trx_trdd, trx->trx_stmttrxid);
        }
#endif /* SS_NODDUPDATE */
        trx->trx_vldst = TRX_VLDST_INIT;
        trx->trx_stmtaddedtotrxbuf = TRUE;
        trx->trx_replicarecovery = TRUE;

        dbe_trx_semexit(trx);
}

#endif /* DBE_REPLICATION */

#ifdef SS_DEBUG
/*#***********************************************************************\
 *
 *              trx_stmt_checktrxbuf
 *
 * Debug function to ensure that statement info is put correctly to the
 * trxbuf.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      stmt_commitp -
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
static void trx_stmt_checktrxbuf(dbe_trx_t* trx, bool stmt_commitp)
{
        dbe_trxstate_t ts;
        dbe_trxid_t trxid;

        ss_dprintf_3(("trx_stmt_checktrxbuf\n"));
        ss_dassert(dbe_trx_semisentered(trx));

#ifdef SS_HSBG2
        if (trx->trx_mode == TRX_NOCHECK) {
            /* In nocheck mode trx is not added to trxbuf. */
            return;
        }
#endif /* SS_HSBG2 */

        ts = dbe_trxbuf_gettrxstate(
                trx->trx_trxbuf,
                trx->trx_stmttrxid,
                NULL,
                &trxid);

        if (stmt_commitp) {
            if (trx->trx_errcode == DBE_RC_SUCC) {
                ss_dassert(ts == DBE_TRXST_BEGIN || ts == DBE_TRXST_TOBEABORTED);
            } else {
                ss_dassert(ts == DBE_TRXST_BEGIN || ts == DBE_TRXST_TOBEABORTED || ts == DBE_TRXST_ABORT);
            }
            ss_dassert(DBE_TRXID_EQUAL(trxid, trx->trx_usertrxid));
        } else {
            ss_dassert(ts == DBE_TRXST_ABORT);
            ss_dassert(DBE_TRXID_EQUAL(trxid, trx->trx_stmttrxid));
        }
}
#endif /* SS_DEBUG */

static void trx_groupstmtlist_done(void* data)
{
        SsMemFree(data);
}

/*#***********************************************************************\
 *
 *              trx_groupstmt_endstmt
 *
 * Ends statement in a statement group, if one is active.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      groupstmtp -
 *
 *
 *      succp -
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
static dbe_ret_t trx_groupstmt_endstmt(
        dbe_trx_t* trx,
        dbe_trxid_t stmttrxid,
        bool groupstmtp,
        dbe_ret_t rc)
{
        bool succp;
        trx_groupstmt_t* gs;

        CHK_TRX(trx);
        ss_dprintf_3(("trx_groupstmt_endstmt:stmttrxid=%ld, groupstmtp=%d\n", DBE_TRXID_GETLONG(stmttrxid), groupstmtp));
        ss_dassert(dbe_trx_semisentered(trx));

        succp = (rc == DBE_RC_SUCC);

        if (groupstmtp) {
            /* Accumulating the group.
             */
            if (succp) {
                /* Add the statement to the stmt group list.
                 */
                ss_dassert(!DBE_TRXID_EQUAL(stmttrxid, trx->trx_usertrxid));
                if (trx->trx_groupstmtlist == NULL) {
                    trx->trx_groupstmtlist = su_list_init(trx_groupstmtlist_done);
                }
                gs = SSMEM_NEW(trx_groupstmt_t);
                gs->gs_stmttrxid = stmttrxid;
                gs->gs_delaystmtcommit = trx->trx_delaystmtcommit;
                ss_dprintf_4(("trx_groupstmt_endstmt:add to group\n"));
                su_list_insertlast(trx->trx_groupstmtlist, gs);
                trx->trx_delaystmtcommit = FALSE;
            }
        } else if (trx->trx_groupstmtlist != NULL) {
            su_list_node_t* n;
            su_list_do_get(trx->trx_groupstmtlist, n, gs) {
                /* Set writes flag to TRUE, we need to record group
                 * commit to log.
                 */
                if (succp) {
                    ss_dprintf_3(("trx_groupstmt_endstmt:commit stmttrxid=%ld\n", DBE_TRXID_GETLONG(gs->gs_stmttrxid)));
                    if (trx->trx_log != NULL && trx->trx_nlogwrites > 0 && !gs->gs_delaystmtcommit) {
                        rc = dbe_log_putstmtmark(
                                    trx->trx_log,
                                    trx->trx_cd,
                                    DBE_LOGREC_COMMITSTMT,
                                    trx->trx_usertrxid,
                                    gs->gs_stmttrxid);
                        if (rc != DBE_RC_SUCC) {
                            dbe_trx_setfailurecode_nomutex(trx, rc);
                            break;
                        }
                        trx->trx_nlogwrites++;
                    }
                } else {
                    ss_dprintf_3(("trx_groupstmt_endstmt:abort stmttrxid=%ld\n", DBE_TRXID_GETLONG(gs->gs_stmttrxid)));
                    if (trx->trx_trdd != NULL && trx->trx_earlyvld) {
                        dbe_trdd_stmt_rollback(
                            trx->trx_trdd,
                            gs->gs_stmttrxid);
                    }
                    if (trx->trx_mmll != NULL) {
                        dbe_mmlocklst_stmt_rollback(
                            trx->trx_mmll,
                            gs->gs_stmttrxid);
                    }
                    dbe_trx_stmtrollbackrepsql(trx, gs->gs_stmttrxid);
                    dbe_gtrs_entertrxgate(trx->trx_gtrs);
                    dbe_trxbuf_abortstmt(
                        trx->trx_trxbuf, 
                        dbe_counter_getcommittrxnum(trx->trx_counter),
                        gs->gs_stmttrxid);
                    dbe_gtrs_exittrxgate(trx->trx_gtrs);
                    if (trx->trx_log != NULL) {
                        dbe_ret_t logrc;
                        logrc = dbe_log_putstmtmark(
                                    trx->trx_log,
                                    trx->trx_cd,
                                    DBE_LOGREC_ABORTSTMT,
                                    trx->trx_usertrxid,
                                    gs->gs_stmttrxid);
                        if (logrc != DBE_RC_SUCC) {
                            dbe_trx_setfailurecode_nomutex(trx, logrc);
                            break;
                        }
                        trx->trx_delaystmtcommit = FALSE;
                        trx->trx_nlogwrites++;
                    }
                }
            }
            su_list_done(trx->trx_groupstmtlist);
            trx->trx_groupstmtlist = NULL;
        }
        return(rc);
}

#ifdef SS_DEBUG
/*#***********************************************************************\
 *
 *              trx_stmt_hsblogstmtstartcheck
 *
 * Checks is hsb statement start is written to log.
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
static bool trx_stmt_hsblogstmtstartcheck(dbe_trx_t* trx)
{
        return(trx->trx_hsbstmtmarkwrittentolog ||
               dbe_db_gethsbmode(trx->trx_db) != DBE_HSB_PRIMARY ||
               trx->trx_log == NULL ||
               !dbe_db_ishsb(trx->trx_db));
}
#endif /* SS_DEBUG */

/*#***********************************************************************\
 *
 *              trx_stmt_commit_step
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      groupstmtp -
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
static dbe_ret_t trx_stmt_commit_step(
        dbe_trx_t* trx,
        bool groupstmtp,
        rs_err_t** p_errh)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        trx_writecheck_t* wchk;
        trx_keycheck_t* kchk;
        trx_vldinfo_t* vi;

        CHK_TRX(trx);
        ss_dassert(dbe_trx_semisentered(trx));

        if (trx->trx_commitst == TRX_COMMITST_DONE) {
            if (trx->trx_errcode != SU_SUCCESS) {
                dbe_trx_error_create(trx, trx->trx_errcode, p_errh);
            }
            return(trx->trx_errcode);
        }
        ss_dprintf_3(("trx_stmt_commit_step: trx->trx_stmttrxid=%d, trx->trx_usertrxid=%d\n",trx->trx_stmttrxid.id, trx->trx_usertrxid.id));
        ss_dassert(!DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid));

        vi = &trx->trx_vldinfo;

        switch (trx->trx_vldst) {

            case TRX_VLDST_INIT:
                if (!trx->trx_stmtaddedtotrxbuf) {
                    dbe_trx_addstmttotrxbuf(trx);
                }

                if (trx->trx_earlyvld) {
                    /* Start write set validate.
                     */
                    vi->vw_.node = su_list_first(&trx->trx_writechklist);
                    if (vi->vw_.node != NULL && dbe_db_isreadonly(trx->trx_db)) {
                        rc = DBE_ERR_TRXREADONLY;
                    } else {
                        trx->trx_vldst = TRX_VLDST_WRITES;
                        rc = DBE_RC_CONT;
                    }
                } else {
                    rc = DBE_RC_SUCC;
                }
                if (!(trx->trx_vldst == TRX_VLDST_WRITES && rc == DBE_RC_CONT)) {
                    break;
                }
                /* FALLTHROUGH */

            case TRX_VLDST_WRITES:
                if (vi->vw_.node == NULL) {
                    /* End of write set validate, start key check
                     * validate.
                     */
                    trx->trx_vldst = TRX_VLDST_KEYS;
                    vi->vk_.node = su_list_first(&trx->trx_keychklist);
                    vi->vk_.tc.tc_mintrxnum = DBE_TRXNUM_MIN;
                    vi->vk_.tc.tc_maxtrxnum = DBE_TRXNUM_MAX;
                    vi->vk_.tc.tc_usertrxid = trx->trx_usertrxid;
                    vi->vk_.tc.tc_maxtrxid = DBE_TRXID_MAX;
                    vi->vk_.tc.tc_trxbuf = NULL;
                    rc = DBE_RC_CONT;
                } else {
                    ss_dassert(trx->trx_stmtiswrites);
                    ss_dassert(trx_stmt_hsblogstmtstartcheck(trx));

                    wchk = su_listnode_getdata(vi->vw_.node);
                    if (!DBE_TRXID_EQUAL(wchk->wchk_stmttrxid, trx->trx_stmttrxid)) {
                        /* No more write checks for this statement.
                         */
                        vi->vw_.node = NULL;
                        rc = DBE_RC_CONT;
                    } else {
                        /* Validate the write operation.
                         */
                        if (wchk->wchk_escalated) {
                            /* Escalated write check.
                             */
                            rc = trx_validate_write_escalated(trx, wchk, vi);
                        } else {
                            /* Single write check.
                             */
                            rc = trx_validate_one_write(trx, wchk, DBE_KEYVLD_NONE, FALSE);
                            if (rc == DBE_RC_SUCC) {
                                /* Move to next write operation.
                                 */
                                vi->vw_.node = su_list_next(
                                                    &trx->trx_writechklist,
                                                    vi->vw_.node);
                                rc = DBE_RC_CONT;
                            }
                        }
                    }
                }
                if (!(trx->trx_vldst == TRX_VLDST_KEYS && rc == DBE_RC_CONT)) {
                    break;
                }
                /* FALLTHROUGH */

            case TRX_VLDST_KEYS:
                if (vi->vk_.node == NULL) {
                    /* End of key checks. The statement is validated
                     * succesfully.
                     */
                    trx->trx_vldst = TRX_VLDST_END;
                    rc = DBE_RC_SUCC;
                } else {
                    ss_dassert(trx_stmt_hsblogstmtstartcheck(trx));
                    ss_dassert(trx->trx_stmtiswrites);

                    kchk = su_listnode_getdata(vi->vk_.node);
                    if (!DBE_TRXID_EQUAL(kchk->kchk_stmttrxid, trx->trx_stmttrxid)) {
                        /* Move to next key operation.
                         */
                        vi->vk_.node = su_list_next(
                                            &trx->trx_keychklist,
                                            vi->vk_.node);
                        rc = DBE_RC_CONT;
                    } else {
                        /* Validate the key write.
                         */
                        if (SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_ESCALATED)) {
                            /* Escalated key check.
                             */
                            rc = trx_keycheck_escalated(trx, kchk, vi);
                        } else {
                            /* Single key check.
                             */
                            rc = trx_keycheck(trx, kchk, &vi->vk_.tc);
                            if (rc == DBE_RC_SUCC) {
                                /* Move to next key operation.
                                 */
                                vi->vk_.node = su_list_next(
                                                    &trx->trx_keychklist,
                                                    vi->vk_.node);
                                rc = DBE_RC_CONT;
                            }
                        }
                    }
                }
                break;

            default:
                ss_error;
        }

        if (rc != DBE_RC_CONT) {
            /* End of validate.
             */
            bool readonlyp;
            readonlyp = (rc == DBE_RC_SUCC &&
                         (trx->trx_mode == TRX_NOWRITES ||
                          trx->trx_mode == TRX_READONLY));

            if (trx->trx_earlyvld) {
                /* Remove checks for this statement.
                 */
                trx_stmt_removeinfo(trx, rc == DBE_RC_SUCC);
            }

            if (!(groupstmtp && readonlyp)) {
                rc = trx_groupstmt_endstmt(trx, trx->trx_stmttrxid, groupstmtp, rc);
            }

#ifdef DBE_HSB_REPLICATION
            if (rc == DBE_RC_SUCC) {
                rc = trx_repchecktrx(trx);
                if (rc != DBE_RC_SUCC) {
                    dbe_trx_setfailurecode_nomutex(trx, rc);
                }
            }
#endif /* DBE_HSB_REPLICATION */

            if (rc == DBE_RC_SUCC) {

                if (trx->trx_trdd != NULL && trx->trx_earlyvld) {
                    dbe_trdd_stmt_commit(trx->trx_trdd, trx->trx_stmttrxid);
                }

                if (trx->trx_log != NULL
                    &&  !readonlyp
                    &&  !groupstmtp
                    &&  trx->trx_stmtiswrites)
                {
                    ss_dassert(trx_stmt_hsblogstmtstartcheck(trx));
                    if (!trx->trx_delaystmtcommit) {
                        rc = dbe_log_putstmtmark(
                                    trx->trx_log,
                                    trx->trx_cd,
                                    DBE_LOGREC_COMMITSTMT,
                                    trx->trx_usertrxid,
                                    trx->trx_stmttrxid);
                        if (rc != DBE_RC_SUCC) {
                            dbe_trx_setfailurecode_nomutex(trx, rc);
                        } else {
                            trx->trx_nlogwrites++;
                        }
                    } else {
                        trx->trx_delaystmtcommit = FALSE;
                    }
                }

            } else {
                if (trx->trx_log != NULL
                &&  !readonlyp
                &&  trx->trx_stmtiswrites)
                {
                    dbe_ret_t logrc;
                    ss_dassert(trx_stmt_hsblogstmtstartcheck(trx));
                    logrc = dbe_log_putstmtmark(
                                trx->trx_log,
                                trx->trx_cd,
                                DBE_LOGREC_ABORTSTMT,
                                trx->trx_usertrxid,
                                trx->trx_stmttrxid);
                    if (logrc != DBE_RC_SUCC) {
                        dbe_trx_setfailurecode_nomutex(trx, logrc);
                    } else {
                        trx->trx_nlogwrites++;
                    }
                    trx->trx_delaystmtcommit = FALSE;
                }
            }

            if (rc != DBE_RC_SUCC) {
                /* Validate failed.
                 */
                if (trx->trx_stmtaddedtotrxbuf) {
#ifdef SS_HSBG2
                    if (trx->trx_mode != TRX_NOCHECK)
#endif
                    {
                        dbe_gtrs_entertrxgate(trx->trx_gtrs);
                        dbe_trxbuf_abortstmt(
                            trx->trx_trxbuf, 
                            dbe_counter_getcommittrxnum(trx->trx_counter),
                            trx->trx_stmttrxid);
                        dbe_gtrs_exittrxgate(trx->trx_gtrs);
                    }
                    trx->trx_stmtaddedtotrxbuf = FALSE;
                }

                dbe_trx_error_create(trx, rc, p_errh);

                if (trx->trx_trdd != NULL && trx->trx_earlyvld) {
                    dbe_trdd_stmt_rollback(trx->trx_trdd, trx->trx_stmttrxid);
                }

                if (trx->trx_mmll != NULL) {
                    dbe_mmlocklst_stmt_rollback(trx->trx_mmll, trx->trx_stmttrxid);
                }

            }

            ss_dassert(dbe_trx_checkcardininfo(trx));

            dbe_trx_cardinstmttrans_mutexif(trx->trx_cd, rc == DBE_RC_SUCC, groupstmtp, FALSE);

            ss_dassert(dbe_trx_checkcardininfo(trx));
            ss_debug(if (trx->trx_stmtaddedtotrxbuf) trx_stmt_checktrxbuf(trx, rc == DBE_RC_SUCC));

#ifdef DBE_HSB_REPLICATION
            {
                dbe_ret_t reprc;
                reprc = trx_rependstmt(
                            trx,
                            rc == DBE_RC_SUCC,
                            trx->trx_stmttrxid,
                            groupstmtp);
                if (reprc != DBE_RC_SUCC) {
                    dbe_trx_setfailurecode_nomutex(trx, reprc);
                    if (rc == DBE_RC_SUCC) {
                        rc = reprc;
                        ss_dassert(*p_errh == NULL);
                        rs_error_create(p_errh, rc);
                    }
                }
                trx->trx_stmterrcode = DBE_RC_SUCC;
            }
#endif /* DBE_HSB_REPLICATION */

            trx->trx_readtrxid = trx->trx_stmttrxid;
            trx->trx_stmttrxid = trx->trx_usertrxid;
            trx->trx_vldst = TRX_VLDST_INIT;
            trx->trx_hsbstmtmarkwrittentolog = FALSE;
            trx->trx_stmtiswrites = FALSE;

            ss_dassert(DBE_TRXID_CMP_EX(trx->trx_usertrxid, trx->trx_readtrxid) <= 0);
            ss_dassert(rc == DBE_RC_SUCC || p_errh == NULL || *p_errh != NULL);
        }

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_stmt_commit
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      groupstmtp -
 *              If TRUE, this statement is added to a statement group
 *              that is fully committed only when a call is made with
 *              groupstmtp as FALSE.
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
dbe_ret_t dbe_trx_stmt_commit(
        dbe_trx_t* trx,
        bool groupstmtp,
        rs_err_t** p_errh)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        su_profile_timer;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_stmt_commit:begin, userid = %d, groupstmtp=%d\n", dbe_user_getid(trx->trx_user), groupstmtp));
        ss_dassert(!trx->trx_rp.rp_activep || trx->trx_rp.rp_type == REP_STMTCOMMIT);

        if (!groupstmtp 
            && (trx->trx_mode == TRX_NOWRITES || trx->trx_mode == TRX_READONLY)
            && !rs_trend_isfun(rs_sysi_getstmttrend(trx->trx_cd))
            && !trx->trx_rollbackdone
            && trx->trx_errcode == DBE_RC_SUCC
            && trx->trx_earlyvld) 
        {
            dbe_trx_sementer(trx);

            ss_dassert(su_list_length(&trx->trx_writechklist) == 0);
            ss_dassert(su_list_length(&trx->trx_keychklist) == 0);

            if (trx->trx_mmll != NULL) {
                rc = dbe_mmlocklst_stmt_commit(
                        trx->trx_mmll,
                        trx->trx_stmttrxid,
                        FALSE);
                ss_rc_dassert(rc == DBE_RC_SUCC, rc);
            }

            trx->trx_readtrxid = trx->trx_stmttrxid;
            trx->trx_stmttrxid = trx->trx_usertrxid;
            trx->trx_vldst = TRX_VLDST_INIT;
            trx->trx_hsbstmtmarkwrittentolog = FALSE;
            trx->trx_stmtiswrites = FALSE;
            
            dbe_trx_semexit(trx);
            
            if (trx->trx_tuplestate != NULL) {
                dbe_tuplestate_done(trx->trx_tuplestate);
                trx->trx_tuplestate = NULL;
            }
            
            return(DBE_RC_SUCC);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);
        dbe_trx_sementer(trx);

        FAKE_CODE_RESET(FAKE_HSBG2_STMTCOMMITFAIL_D,
        {
            ss_dprintf_1(("FAKE_HSBG2_STMTCOMMITFAIL_D\n"));
            dbe_trx_setfailurecode_nomutex(trx, DBE_ERR_HSBABORTED);
        });

        if (trx->trx_errcode != DBE_RC_SUCC) {
            rc = trx->trx_errcode;
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            dbe_trx_semexit(trx);
            dbe_trx_error_create(trx, rc, p_errh);
            dbe_trx_stmt_rollback(trx, groupstmtp, NULL);
            if (!trx->trx_rollbackdone) {
                trx->trx_errcode = DBE_RC_SUCC;
            }
            ss_dprintf_2(("dbe_trx_stmt_commit:end, rc = %s (%d)\n", su_rc_nameof(rc), rc));
            ss_debug(trx->trx_opactivep = FALSE);

            goto exit_function;
        }

#if defined(SS_MME)
        /* This is used only with MME, not with the old mmind. */
        /* XXX - is this in the right place?  Is this the right stuff? */
        if (trx->trx_mmll != NULL) {
            rc = dbe_mmlocklst_stmt_commit(
                    trx->trx_mmll,
                    trx->trx_stmttrxid,
                    groupstmtp);
            if (rc != DBE_RC_SUCC) {
                if (!trx->trx_rollbackdone) {
                    trx->trx_errcode = DBE_RC_SUCC;
                }
                dbe_db_exitaction(trx->trx_db, trx->trx_cd);
                dbe_trx_semexit(trx);
                dbe_trx_error_create(trx, rc, p_errh);
                dbe_trx_stmt_rollback(trx, groupstmtp, NULL);
                ss_dprintf_2(("dbe_trx_stmt_commit:end, rc = %s (%d)\n", su_rc_nameof(rc), rc));
                ss_debug(trx->trx_opactivep = FALSE);

                goto exit_function;
            }
        }
#endif /* SS_MME */

        ss_dassert(!DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid));
        su_profile_start;

        ss_dassert(!trx->trx_opactivep);
        ss_debug(trx->trx_opactivep = TRUE);

        for (;;) {
            rc = trx_stmt_commit_step(trx, groupstmtp, p_errh);
            if (rc != DBE_RC_CONT) {
                break;
            }
        }

        ss_dprintf_2(("dbe_trx_stmt_commit:end, rc = %s (%d)\n", su_rc_nameof(rc), rc));
        ss_debug(trx->trx_opactivep = FALSE);

        trx_stmt_cleanup(trx->trx_cd, trx, rc == DBE_RC_SUCC ? RS_TROP_AFTERSTMTCOMMIT : RS_TROP_AFTERSTMTROLLBACK);

        dbe_trx_abortif_nomutex(trx);

        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        trx->trx_stmtsearchtrxnum = DBE_TRXNUM_NULL;

#ifdef DBE_HSB_REPLICATION
        if(trx->trx_tuplestate != NULL) {
            dbe_tuplestate_done(trx->trx_tuplestate);
            trx->trx_tuplestate = NULL;
        }
#endif /* DBE_HSB_REPLICATION */

        su_profile_stop("dbe_trx_stmt_commit");

 exit_function:
        FAKE_CODE_BLOCK(
                FAKE_DBE_SLEEP_AFTER_STMTCOMMIT,
                {
                    SsPrintf("FAKE_DBE_SLEEP_AFTER_STMTCOMMIT: sleeping...\n");
                    SsThrSleep(5000);
                });

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_stmt_localrollback
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      enteractionp -
 *
 *
 *      forcep -
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
static dbe_ret_t trx_stmt_localrollback(
        dbe_trx_t* trx,
        bool enteractionp,
        bool groupstmtp,
        bool forcep,
        rs_err_t** p_errh)
{
        bool iswrites;
        dbe_trxid_t stmttrxid;
        su_profile_timer;

        SS_NOTUSED(p_errh);
        CHK_TRX(trx);
        ss_dprintf_1(("trx_stmt_localrollback:begin, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(enteractionp || dbe_trx_semisentered(trx));

        if (enteractionp) {
            dbe_db_enteraction(trx->trx_db, trx->trx_cd);
            dbe_trx_sementer(trx);
        }
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));


        if (trx->trx_rollbackdone && !forcep) {
            if (enteractionp) {
                dbe_db_exitaction(trx->trx_db, trx->trx_cd);
                dbe_trx_semexit(trx);
            }
            return(DBE_RC_SUCC);
        }

        /* In some HSB case (shutdown during explicit COMMIT WORK statement)
         * we may not have activbe statement any more.
         */
        ss_dassert(!DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid)
                   || trx->trx_rp.rp_donep);

        if (DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid)) {
            /* No statement active. */
            if (enteractionp) {
                dbe_db_exitaction(trx->trx_db, trx->trx_cd);
                dbe_trx_semexit(trx);
            }
            return(DBE_RC_SUCC);
        }

#ifdef SS_HSBG2
        iswrites = (trx->trx_mode != TRX_NOWRITES &&
                    trx->trx_mode != TRX_READONLY);
#else /* SS_HSBG2 */
        iswrites = (trx->trx_mode != TRX_NOWRITES);
#endif /* SS_HSBG2 */

        stmttrxid = trx->trx_stmttrxid;

        /* In throwout situations the following assert may not be true.
            ss_dassert(trx->trx_vldst == TRX_VLDST_INIT);
         */

        su_profile_start;

        if (!trx->trx_stmtaddedtotrxbuf && iswrites) {
            trx->trx_stmtaddedtotrxbuf = TRUE;
            dbe_trxbuf_addstmt(
                trx->trx_trxbuf,
                trx->trx_stmttrxid,
                trx->trx_info);
        }
        if (trx->trx_stmtaddedtotrxbuf) {
#ifdef SS_HSBG2
            if (trx->trx_mode != TRX_NOCHECK)
#endif
            {
                dbe_gtrs_entertrxgate(trx->trx_gtrs);
                dbe_trxbuf_abortstmt(
                    trx->trx_trxbuf, 
                    dbe_counter_getcommittrxnum(trx->trx_counter),
                    trx->trx_stmttrxid);
                dbe_gtrs_exittrxgate(trx->trx_gtrs);
            }
            trx->trx_stmtaddedtotrxbuf = FALSE;
        }

#if defined(SS_MME) || !defined(SS_NOMMINDEX)
        if (trx->trx_mmll != NULL) {
            dbe_mmlocklst_stmt_rollback(trx->trx_mmll, trx->trx_stmttrxid);
        }
#endif /* SS_NOMMINDEX */

#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL && iswrites && trx->trx_nlogwrites > 0) {
            dbe_ret_t logrc;
            logrc = dbe_log_putstmtmark(
                        trx->trx_log,
                        trx->trx_cd,
                        DBE_LOGREC_ABORTSTMT,
                        trx->trx_usertrxid,
                        trx->trx_stmttrxid);
            if (logrc != DBE_RC_SUCC) {
                dbe_trx_setfailurecode_nomutex(trx, logrc);
            } else {
                trx->trx_nlogwrites++;
            }
            trx->trx_delaystmtcommit = FALSE;
        }
#endif /* SS_NOLOGGING */

        /* Remove checks for this statement.
         */
        trx_stmt_removeinfo(trx, FALSE);

#ifndef SS_NODDUPDATE
        if (trx->trx_trdd != NULL) {
            dbe_trdd_stmt_rollback(trx->trx_trdd, trx->trx_stmttrxid);
        }
#endif /* SS_NODDUPDATE */

        ss_dassert(dbe_trx_checkcardininfo(trx));

        dbe_trx_cardinstmttrans_mutexif(trx->trx_cd, FALSE, groupstmtp, FALSE);

        ss_dassert(dbe_trx_checkcardininfo(trx));
        ss_debug(if (trx->trx_stmtaddedtotrxbuf) trx_stmt_checktrxbuf(trx, FALSE));

#ifdef DBE_HSB_REPLICATION
        trx_rependstmt(trx, FALSE, trx->trx_stmttrxid, groupstmtp);
        trx->trx_stmterrcode = DBE_RC_SUCC;
#endif /* DBE_HSB_REPLICATION */

        trx_groupstmt_endstmt(trx, stmttrxid, groupstmtp, DBE_ERR_FAILED);

        if (enteractionp) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        }

        trx->trx_stmttrxid = trx->trx_usertrxid;
        trx->trx_vldst = TRX_VLDST_INIT;
        trx->trx_hsbstmtmarkwrittentolog = FALSE;
        trx->trx_stmtiswrites = FALSE;

        trx_stmt_cleanup(trx->trx_cd, trx, RS_TROP_AFTERSTMTROLLBACK);

        if (enteractionp) {
            dbe_trx_semexit(trx);
        }

        su_profile_stop("trx_stmt_localrollback");
        ss_dprintf_2(("trx_stmt_localrollback:end\n"));

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              dbe_trx_stmt_rollback
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
dbe_ret_t dbe_trx_stmt_rollback(
        dbe_trx_t* trx,
        bool groupstmtp,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;
        CHK_TRX(trx);
        ss_dassert(!trx->trx_opactivep);
        ss_debug(trx->trx_opactivep = TRUE);

        if (trx->trx_rp.rp_activep
        &&  trx->trx_rp.rp_donep
        &&  trx->trx_rp.rp_type != REP_STMTCOMMIT)
        {
            /* Operation is active but it is not stmt rollback.
             */
            trx->trx_rp.rp_activep = FALSE;
        }
        ss_rc_assert((trx->trx_rp.rp_type != REP_COMMIT &&
                      trx->trx_rp.rp_type != REP_COMMIT_NOFLUSH &&
                      trx->trx_rp.rp_type != REP_STMTCOMMIT) ||
                     trx->trx_rp.rp_donep, trx->trx_rp.rp_type);

        rc = trx_stmt_localrollback(trx, TRUE, groupstmtp, FALSE, p_errh);

        ss_debug(trx->trx_opactivep = FALSE);

#ifdef DBE_HSB_REPLICATION
        if(trx->trx_tuplestate != NULL) {
            dbe_tuplestate_done(trx->trx_tuplestate);
            trx->trx_tuplestate = NULL;
        }
#endif /* DBE_HSB_REPLICATION */

        return(rc);
}

#ifndef SS_NOTRXREADCHECK

/*#***********************************************************************\
 *
 *              keyid_rbt_inscmp
 *
 *
 *
 * Parameters :
 *
 *      key1 -
 *
 *
 *      key2 -
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
static int keyid_rbt_inscmp(void* key1, void* key2)
{
        keyid_node_t* kin1 = key1;
        keyid_node_t* kin2 = key2;

        return((int)(kin1->kin_keyid - kin2->kin_keyid));
}

/*#***********************************************************************\
 *
 *              keyid_rbt_seacmp
 *
 *
 *
 * Parameters :
 *
 *      sea_key -
 *
 *
 *      rbt_key -
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
static int keyid_rbt_seacmp(void* sea_key, void* rbt_key)
{
        keyid_node_t* kin = rbt_key;

        return((int)((long)sea_key - kin->kin_keyid));
}

/*#***********************************************************************\
 *
 *              keyid_rbt_del
 *
 *
 *
 * Parameters :
 *
 *      key -
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
static void keyid_rbt_del(void* key)
{
        keyid_node_t* kin = key;

        su_rbt_done(kin->kin_rbt);
        SsMemFree(kin);
}

/*#***********************************************************************\
 *
 *              rchk_rbt_cmp
 *
 *
 *
 * Parameters :
 *
 *      key1 -
 *
 *
 *      key2 -
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
static int rchk_rbt_cmp(void* key1, void* key2)
{
        trx_readcheck_t* rchk1 = key1;
        trx_readcheck_t* rchk2 = key2;
        rs_sysi_t* cd;
        vtpl_t* start1;
        vtpl_t* start2;
        bool dummy_isclosed;
        int cmp;

        cd = rchk1->rchk_cd;

        rs_pla_get_range_start(cd, rchk1->rchk_plan, &start1, &dummy_isclosed);
        rs_pla_get_range_start(cd, rchk2->rchk_plan, &start2, &dummy_isclosed);

        cmp = vtpl_compare(start1, start2);

        if (cmp == 0) {
            return((int)((char*)key1 - (char*)key2));
        } else {
            return(cmp);
        }
}

/*#***********************************************************************\
 *
 *              trx_escalate_buildtree
 *
 * Creates a rb-tree of read checks. The returned tree contains keyid_node_t
 * entries. The tree is ordered by key id. For each key id there is a
 * rb-tree that contains all read checks for that tree. The read checks
 * are ordered the read set range start key value.
 *
 * Parameters :
 *
 *      trx - in
 *              Transaction object.
 *
 * Return value - give :
 *
 *      Rb-tree ordered by key id where each node contains a tree of read
 *      check for that key.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static su_rbt_t* trx_escalate_buildtree(dbe_trx_t* trx)
{
        su_list_node_t* n;
        trx_readcheck_t* rchk;
        su_rbt_t* keyid_rbt;
        su_rbt_t* rchk_rbt;
        rs_sysi_t* cd;
        long last_keyid = -1L;
        su_rbt_t* last_rchk_rbt = NULL;

        ss_dprintf_3(("trx_escalate_buildtree\n"));
        ss_dassert(dbe_trx_semisentered(trx));

        cd = trx->trx_cd;
        keyid_rbt = su_rbt_inittwocmp(
                        keyid_rbt_inscmp,
                        keyid_rbt_seacmp,
                        keyid_rbt_del);

        su_list_do_get(&trx->trx_readchklist, n, rchk) {
            long keyid;
            su_rbt_node_t* keyid_node;
            keyid_node_t* kin;

            if (!DBE_TRXID_EQUAL(rchk->rchk_stmttrxid, trx->trx_stmttrxid)) {
                continue;
            }

            /* Find the rchk_rbt for this keyid.
             */
            keyid = rs_pla_getkeyid(cd, rchk->rchk_plan);
            if (keyid == last_keyid) {
                ss_dassert(last_rchk_rbt != NULL);
                rchk_rbt = last_rchk_rbt;
            } else {
                keyid_node = su_rbt_search(keyid_rbt, (void*)keyid);
                if (keyid_node == NULL) {
                    /* Keyid not found, add it.
                     */
                    rchk_rbt = su_rbt_init(rchk_rbt_cmp, NULL);
                    kin = SSMEM_NEW(keyid_node_t);
                    kin->kin_keyid = keyid;
                    kin->kin_rbt = rchk_rbt;
                    su_rbt_insert(keyid_rbt, kin);
                } else {
                    /* Keyid found.
                     */
                    kin = su_rbtnode_getkey(keyid_node);
                    rchk_rbt = kin->kin_rbt;
                }
                last_keyid = keyid;
                last_rchk_rbt = rchk_rbt;
            }

            /* Add this rchk to the tree.
             */
            su_rbt_insert(rchk_rbt, rchk);
        }
        return(keyid_rbt);
}

/*#***********************************************************************\
 *
 *              trx_escalate_key
 *
 * Escalates the read checks of the given key. The read checks for the
 * key are given in the parameter rbt, where they are ordered by the
 * range start key value. The escalated read set is between minimum
 * of all range starts in the rbt and maximum of all range ends in the
 * rbt. The read set does not access the data values, the check contains
 * the whole escalated key value range. The function generates the escalated
 * read check object and replaces all read checks in the read check list
 * with it.
 *
 * If the tree does not contain enough read checks, no escalation is done.
 *
 * Parameters :
 *
 *      trx - use
 *              Transaction object.
 *
 *      rbt - in
 *              Rb-tree of read check for the key.
 *
 *      keyid - in
 *              Key id.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void trx_escalate_key(dbe_trx_t* trx, su_rbt_t* rbt, long keyid)
{
        rs_sysi_t* cd;
        vtpl_t* start;
        vtpl_t* end;
        su_rbt_node_t* node;
        trx_readcheck_t* rchk;
        bool dummy_isclosed;
        rs_pla_t* min_plan;
        trx_readcheck_t* min_rchk;
        rs_pla_t* new_plan;
        trx_readcheck_t* new_rchk;
        su_list_node_t* n;
        dynvtpl_t new_start = NULL;
        dynvtpl_t new_end = NULL;

        ss_dprintf_3(("trx_escalate_key:keyid = %ld, readcnt = %ld\n",
            keyid, (long)su_rbt_nelems(rbt)));
        ss_dassert(dbe_trx_semisentered(trx));

        if ((long)su_rbt_nelems(rbt) < trx->trx_escalatelimits->esclim_read) {
            ss_dprintf_4(("trx_escalate_key:no need to escalate\n"));
            return;
        }

        ss_dprintf_4(("trx_escalate_key:escalate this key\n"));

        cd = trx->trx_cd;
        node = su_rbt_min(rbt, NULL);
        rchk = su_rbtnode_getkey(node);
        min_rchk = rchk;
        min_plan = rchk->rchk_plan;

        /* Get the escalated range start.
         */
        rs_pla_get_range_start(cd, min_plan, &start, &dummy_isclosed);

        /* Find the escalated range end by scanning through the tree and
         * taking the maximum of range ends.
         */
        end = rchk->rchk_lastkey;
        node = su_rbt_succ(rbt, node);
        while (node != NULL) {
            rchk = su_rbtnode_getkey(node);
            if (vtpl_compare(end, rchk->rchk_lastkey) < 0) {
                end = rchk->rchk_lastkey;
            }
            node = su_rbt_succ(rbt, node);
        }

        /* Create a new plan object for the escalated range.
         */
        dynvtpl_setvtpl(&new_start, start);
        dynvtpl_setvtpl(&new_end, end);

        ss_dprintf_4(("new range begin:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, new_start));
        ss_dprintf_4(("new range end:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, new_end));

        new_plan = rs_pla_alloc(cd);

        rs_pla_initbuf(
            cd,
            new_plan,
            rs_pla_getrelh(cd, min_plan),
            rs_pla_getkey(cd, min_plan),
            TRUE,               /* isconsistent */
            new_start,          /* range start (give) */
            TRUE,               /* start closed */
            new_end,            /* range end (give) */
            TRUE,               /* end closed */
            rs_pla_get_key_constraints_buf(cd, new_plan), /* key constraint */
            rs_pla_get_data_constraints_buf(cd, new_plan),/* data constraint */
            NULL,               /* all constraints */
            NULL,               /* tuple constraint */
            NULL,               /* select list */
            FALSE,              /* dereference */
            0L,                 /* dummy, n solved range cons */
            0L,                 /* dummy, n solved key cons */
            0L,                 /* dummy, n solved data cons */
            TRUE);              /* use link counts to relh and key */


        /* Create new read check for the escalated range.
         */
        new_rchk = SSMEM_NEW(trx_readcheck_t);
        new_rchk->rchk_stmttrxid = min_rchk->rchk_stmttrxid;
        new_rchk->rchk_cd = cd;
        new_rchk->rchk_plan = new_plan;
        new_rchk->rchk_lastkey = NULL;
        dynvtpl_setvtpl(&new_rchk->rchk_lastkey, end);
        new_rchk->rchk_lasttrxid = DBE_TRXID_MAX;

        /* Remove old checks from the trx read check list.
         */
        n = su_list_first(&trx->trx_readchklist);
        while (n != NULL) {
            su_list_node_t* tmpn;
            rchk = su_listnode_getdata(n);
            tmpn = su_list_next(&trx->trx_readchklist, n);
            if (DBE_TRXID_EQUAL(rchk->rchk_stmttrxid, trx->trx_stmttrxid) &&
                rs_pla_getkeyid(cd, rchk->rchk_plan) == (ulong)keyid) {
                su_list_remove(&trx->trx_readchklist, n);
            }
            n = tmpn;
        }

        /* Add the newly created escalated rchk to the trx rchk list.
         */
        su_list_insertfirst(&trx->trx_readchklist, new_rchk);

        SS_MEMOBJ_INC(SS_MEMOBJ_TRXRCHK, trx_readcheck_t);
}

/*#***********************************************************************\
 *
 *              trx_escalate_readset
 *
 * Escalates read set by combining multiple read set into one read set.
 * First all read checks are divided by the key id, and then the read
 * sets of each key are escalated.
 *
 * Parameters :
 *
 *      trx - use
 *              Transaction object.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void trx_escalate_readset(dbe_trx_t* trx)
{
        su_rbt_t* keyid_rbt;
        su_rbt_node_t* keyid_node;

        ss_dprintf_3(("trx_escalate_readset\n"));
        ss_dassert(dbe_trx_semisentered(trx));

        keyid_rbt = trx_escalate_buildtree(trx);

        keyid_node = su_rbt_min(keyid_rbt, NULL);
        while (keyid_node != NULL) {
            keyid_node_t* kin;
            kin = su_rbtnode_getkey(keyid_node);
            trx_escalate_key(trx, kin->kin_rbt, kin->kin_keyid);
            keyid_node = su_rbt_succ(keyid_rbt, keyid_node);
        }
        su_rbt_done(keyid_rbt);
}

#else /* SS_NOTRXREADCHECK */

# define trx_escalate_readset(trx)

#endif /* SS_NOTRXREADCHECK */

#ifdef DBE_LOGORDERING_FIX

/*##**********************************************************************\
 *
 *              dbe_trx_initrepparams_tuple
 *
 * Sets the replication parameters for one tuple operation.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      insertp -
 *
 *
 *      tref -
 *
 *
 *      vtpl -
 *
 *
 *      relh -
 *
 *
 *      isblobattrs -
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
rep_params_t *dbe_trx_initrepparams_tuple(
        dbe_trx_t* trx,
        bool insertp,
        dbe_tref_t* tref,
        vtpl_t* vtpl,
        rs_relh_t* relh,
        bool isblobattrs)
{
#ifdef DBE_HSB_REPLICATION
        if (!trx->trx_replicaslave) {
            ss_dassert(insertp || tref != NULL);

            /*
             * All other insert operations
             *
             */

            if (dbe_trx_initrepparams(trx, insertp ? REP_INSERT : REP_DELETE)) {
                trx->trx_rp.rp_relh = relh;
                trx->trx_rp.rp_tupletrxid = insertp ? DBE_TRXID_NULL : tref->tr_trxid;
                trx->trx_rp.rp_vtpl = vtpl;
                trx->trx_rp.rp_isblob = isblobattrs;
                trx->trx_rp.rp_flushallowed = trx->trx_hsbflushallowed;
                return (&trx->trx_rp);
            }
        }
#endif /* DBE_HSB_REPLICATION */

        return(NULL);
}
#endif /* DBE_LOGORDERING_FIX */

/*#***********************************************************************\
 *
 *              trx_logtupleversion
 *
 * Logs tuple version increment in case of HSB primary and trx logging is
 * disabled. This is needed to keep tuple version counters up top date
 * in secondary.
 *
 * Parameters :
 *
 *              trx -
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
static dbe_ret_t trx_logtupleversion(dbe_trx_t* trx)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        dbe_log_t* log;

        ss_dassert(trx->trx_log == NULL);

        log = dbe_db_getlog(trx->trx_db);
        if (log != NULL) {
            ss_dprintf_3(("trx_logtupleversion:log tuple version inc\n"));
            rc = dbe_log_putincsysctr(log, DBE_LOGREC_INCSYSCTR, DBE_CTR_TUPLEVERSION);
        }
        if (trx->trx_mode == TRX_NOCHECK) {
            /* In NOCHECK mode return always success because those transactions
             * are typically used during checkpoint.
             */
            return(DBE_RC_SUCC);
        } else {
            return(rc);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_addtolog
 *
 * Adds entry to the log file. The entry must be the clustering key value.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      insertp -
 *
 *
 *      clustkey -
 *
 *
 *      tref -
 *
 *
 *      vtpl -
 *
 *
 *      relid -
 *
 *
 *      isblobattrs -
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
dbe_ret_t dbe_trx_addtolog(
        dbe_trx_t* trx,
        bool insertp,
        rs_key_t* clustkey __attribute__ ((unused)),
        dbe_tref_t* tref,
        vtpl_t* vtpl,
        rs_relh_t* relh,
        bool isblobattrs)
{
        rs_sysi_t* cd;
        dbe_ret_t rc;
#ifdef DBE_LOGORDERING_FIX
#ifdef DBE_REPLICATION
        rep_params_t *rp = NULL;
        bool replicated = FALSE;
#endif /* DBE_REPLICATION */
#endif /* DBE_LOGORDERING_FIX */

        CHK_TRX(trx);
        SS_PUSHNAME("dbe_trx_addtolog");

        ss_dprintf_1(("dbe_trx_addtolog, userid = %d\n", dbe_user_getid(trx->trx_user)));
        /* ss_dassert(trx->trx_mode != TRX_NOWRITES); Fails in nocheck tables. */
        ss_dassert(trx->trx_mode != TRX_READONLY);
        ss_dassert(rs_key_isclustering(trx->trx_cd, clustkey));

        if (trx->trx_errcode != DBE_RC_SUCC) {
            /* transaction will fail */
            SS_POPNAME;
            return(trx->trx_errcode);
        }

#ifdef SS_HSBG2
        ss_dassert(trx->trx_mode != TRX_NOWRITES ||
                   rs_relh_isnocheck(trx->trx_cd, relh));
        ss_dassert(trx->trx_hsbstmtmarkwrittentolog ||
                   trx->trx_log == NULL ||
                   trx->trx_mode == TRX_NOCHECK ||
                   rs_relh_isnocheck(trx->trx_cd, relh));
        ss_dassert(trx->trx_stmtiswrites ||
                   rs_relh_isnocheck(trx->trx_cd, relh));
        rc = DBE_RC_SUCC;
#else /* SS_HSBG2 */
        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            SS_POPNAME;
            return(rc);
        }
#endif /* SS_HSBG2 */

        trx->trx_nlogwrites++;

        if (trx->trx_log == NULL) {
            ss_dprintf_1(("dbe_trx_addtolog: row insert to table %s.%s.%s not logged cd = 0x%08lX\n",
                          rs_relh_catalog(trx->trx_cd, relh),
                          rs_relh_schema(trx->trx_cd, relh),
                          rs_relh_name(trx->trx_cd, relh),
                          (ulong)trx->trx_cd));
            if (insertp) {
                rc = trx_logtupleversion(trx);
            } else {
                rc = DBE_RC_SUCC;
            }
        } else {
            cd = trx->trx_cd;
            /* Add entry to the log file.
             */

#ifndef SS_NOLOGGING

#ifdef DBE_LOGORDERING_FIX
#ifdef DBE_REPLICATION
            ss_dassert(dbe_trx_semisnotentered(trx));
            dbe_trx_sementer(trx);

            rp = dbe_trx_initrepparams_tuple(
                        trx,
                        insertp,
                        tref,
                        vtpl,
                        relh,
                        isblobattrs);
#endif /* DBE_REPLICATION */
#endif /* DBE_LOGORDERING_FIX */

            if (insertp) {
                ss_dassert(tref == NULL);

                rc = dbe_log_puttuple(
                        trx->trx_log,
                        trx->trx_cd,
                        isblobattrs
                            ? DBE_LOGREC_INSTUPLEWITHBLOBS
                            : DBE_LOGREC_INSTUPLENOBLOBS,
                        trx->trx_stmttrxid,
                        vtpl,
                        rs_relh_relid(trx->trx_cd, relh)
#ifdef DBE_LOGORDERING_FIX
                        , rp,
                        &replicated);
#else /* DBE_LOGORDERING_FIX */
                        );
#endif /* DBE_LOGORDERING_FIX */
            } else {
                ss_dassert(tref != NULL);

                rc = dbe_log_puttuple(
                        trx->trx_log,
                        trx->trx_cd,
                        DBE_LOGREC_DELTUPLE,
                        trx->trx_stmttrxid,
                        dbe_tref_getrecovvtpl(tref),
                        rs_relh_relid(trx->trx_cd, relh)
#ifdef DBE_LOGORDERING_FIX
                        , rp,
                        &replicated);
#else /* DBE_LOGORDERING_FIX */
                        );
#endif /* DBE_LOGORDERING_FIX */
            }

#ifdef DBE_LOGORDERING_FIX
#ifdef DBE_REPLICATION
            dbe_trx_semexit(trx);

            if(replicated) {
                dbe_trx_markreplicate(trx);
            }
#endif /* DBE_REPLICATION */
#endif /* DBE_LOGORDERING_FIX */
#endif /* SS_NOLOGGING */
        }

        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_uselogging
 *
 * Returns TRUE if this transation uses logging.
 *
 * Parameters :
 *
 *              trx -
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
bool dbe_trx_uselogging(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_log != NULL);
}

/*##**********************************************************************\
 *
 *              dbe_trx_mme_addtolog
 *
 * Adds MME entry to the log file.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      insertp -
 *
 *
 *      clustkey -
 *
 *
 *      tref -
 *
 *
 *      vtpl -
 *
 *
 *      relid -
 *
 *
 *      isblobattrs -
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
#ifdef SS_MME
dbe_ret_t dbe_trx_mme_addtolog(
        dbe_trx_t* trx,
        bool insertp,
        rs_key_t* clustkey __attribute__ ((unused)),
        mme_rval_t* mmerval,
        rs_relh_t* relh,
        bool isblobattrs)
{
        rs_sysi_t* cd;
        dbe_ret_t rc;
        dbe_logrectype_t logrectype;

#ifdef DBE_LOGORDERING_FIX
#ifdef DBE_REPLICATION
        rep_params_t *rp = NULL;
        bool replicated = FALSE;
#endif /* DBE_REPLICATION */
#endif /* DBE_LOGORDERING_FIX */

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_mme_addtolog, userid = %d\n", dbe_user_getid(trx->trx_user)));
        /* ss_dassert(trx->trx_mode != TRX_NOWRITES); Fails in nocheck tables. */
        ss_dassert(trx->trx_mode != TRX_READONLY);

        if (trx->trx_errcode != DBE_RC_SUCC) {
            /* transaction will fail */
            return(trx->trx_errcode);
        }

        if(dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }

        ss_dassert(trx->trx_mode != TRX_NOWRITES);
/*
        ss_dassert(trx->trx_hsbstmtmarkwrittentolog ||
                   trx->trx_mode == TRX_NOCHECK);*/
        ss_dassert(trx->trx_stmtiswrites);
        rc = DBE_RC_SUCC;

        trx->trx_nlogwrites++;

        if (trx->trx_log == NULL) {
            ss_dprintf_1(("dbe_trx_mme_addtolog: row insert to table %s.%s.%s not logged cd = 0x%08lX\n",
                          rs_relh_catalog(trx->trx_cd, relh),
                          rs_relh_schema(trx->trx_cd, relh),
                          rs_relh_name(trx->trx_cd, relh),
                          (ulong)trx->trx_cd));
            if (insertp) {
                rc = trx_logtupleversion(trx);
            } else {
                rc = DBE_RC_SUCC;
            }
        } else {
            cd = trx->trx_cd;
            /* Add entry to the log file.
             */

            ss_dassert(dbe_trx_semisnotentered(trx));
            dbe_trx_sementer(trx);

            /* There are no blobs in MME at the moment.
               Remove this, when there are! tommiv 8-dec-2002 */
            ss_rc_assert(!isblobattrs, isblobattrs);

            if (insertp) {
                logrectype =  isblobattrs ?
                                DBE_LOGREC_MME_INSTUPLEWITHBLOBS
                                : DBE_LOGREC_MME_INSTUPLENOBLOBS;
            } else {
                logrectype = DBE_LOGREC_MME_DELTUPLE;
            }

            rc = dbe_log_putmmetuple(
                    trx->trx_log,
                    trx->trx_cd,
                    logrectype,
                    trx->trx_stmttrxid,
                    mmerval,
                    rs_relh_relid(trx->trx_cd, relh),
                    rp,
                    &replicated);

            dbe_trx_semexit(trx);

            if(replicated) {
                ss_error; /* XXX Not possible yet */
                dbe_trx_markreplicate(trx);
            }
        }

        return(rc);
}
#endif /* SS_MME */

/*##**********************************************************************\
 *
 *              dbe_trx_reptuple
 *
 * Replicates one tuple operation.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      insertp -
 *
 *
 *      tref -
 *
 *
 *      vtpl -
 *
 *
 *      relh -
 *
 *
 *      isblobattrs -
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
dbe_ret_t dbe_trx_reptuple(
        dbe_trx_t* trx __attribute__ ((unused)),
        bool insertp __attribute__ ((unused)),
        dbe_tref_t* tref __attribute__ ((unused)),
        vtpl_t* vtpl __attribute__ ((unused)),
        rs_relh_t* relh __attribute__ ((unused)),
        bool isblobattrs __attribute__ ((unused)))
{
        dbe_ret_t rc = DBE_RC_SUCC;


#if defined(DBE_HSB_REPLICATION) && !defined(DBE_LOGORDERING_FIX)
        if (!trx->trx_replicaslave) {
            ss_dassert(insertp || tref != NULL);

            if (dbe_trx_initrepparams(trx, insertp ? REP_INSERT : REP_DELETE)) {
                trx->trx_rp.rp_relh = relh;
                trx->trx_rp.rp_tupletrxid = insertp ? DBE_TRXID_NULL : tref->tr_trxid;
                trx->trx_rp.rp_vtpl = vtpl;
                trx->trx_rp.rp_isblob = isblobattrs;
                trx->trx_rp.rp_flushallowed = trx->trx_hsbflushallowed;
            }

            rc = dbe_trx_replicate(
                    trx,
                    insertp ? REP_INSERT : REP_DELETE);
            switch (rc) {
                case DBE_RC_SUCC:
                    trx->trx_replication = TRUE;
                    break;
                case DBE_RC_CONT:
                    break;
                case DB_RC_NOHSB:
                    rc = DBE_RC_SUCC;
                    break;
                default:
                    break;
            }
        }
#endif /* DBE_HSB_REPLICATION && !DBE_LOGORDERING_FIX */

        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_addwritecheck
 *
 * Adds write check info to wrote check info list. Escalates write check
 * info if necessary.
 *
 * The write check is a key range that contains only a single tuple.
 * The tuple is specified using the tuple reference.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      key -
 *
 *
 *      vtpl -
 *
 *
 *      readlevel -
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
static void trx_addwritecheck(
        dbe_trx_t* trx,
        rs_key_t* key,
        vtpl_t* tref_vtpl,
        dbe_trxnum_t readlevel)
{
        trx_writecheck_t* wchk;
        bool escalatep;
        rs_sysi_t* cd;

        ss_dprintf_3(("trx_addwritecheck\n"));
        ss_dassert(dbe_trx_semisentered(trx));

        cd = trx->trx_cd;

        if (su_list_length(&trx->trx_writechklist) >
            (uint)trx->trx_escalatelimits->esclim_check) {

            wchk = su_list_getfirst(&trx->trx_writechklist);
            ss_dassert(wchk->wchk_key != NULL);
            escalatep = DBE_TRXID_EQUAL(wchk->wchk_stmttrxid, trx->trx_stmttrxid) &&
                        DBE_TRXNUM_EQUAL(wchk->wchk_maxtrxnum, readlevel) &&
                        rs_key_id(cd, key) == rs_key_id(cd, wchk->wchk_key);
        } else {
            escalatep = FALSE;
        }

        if (escalatep) {
            /* Escalate the write check range.
             */
            ss_pprintf_4(("trx_addwritecheck: escalate write check, list len = %d\n",
                su_list_length(&trx->trx_writechklist)));
            ss_dassert(wchk->wchk_key != NULL);
            SS_PMON_ADD_BETA(SS_PMON_DBE_TRX_ESCALATEWRITECHECK);
            if (wchk->wchk_maxkey == NULL) {
                dynvtpl_setvtplwithincrement(
                    &wchk->wchk_maxkey,
                    wchk->wchk_minkey);
            }
            if (vtpl_compare(tref_vtpl, wchk->wchk_minkey) < 0) {
                dynvtpl_setvtpl(&wchk->wchk_minkey, tref_vtpl);
            } else if (vtpl_compare(tref_vtpl, wchk->wchk_maxkey) > 0) {
                dynvtpl_setvtplwithincrement(&wchk->wchk_maxkey, tref_vtpl);
            }
            wchk->wchk_escalated = TRUE;

        } else {
            /* Add a new write check.
             */
            wchk = SSMEM_NEW(trx_writecheck_t);

            wchk->wchk_minkey = NULL;
            wchk->wchk_maxkey = NULL;

            dynvtpl_setvtpl(&wchk->wchk_minkey, tref_vtpl);

            wchk->wchk_maxtrxnum = readlevel;
            wchk->wchk_stmttrxid = trx->trx_stmttrxid;
            wchk->wchk_escalated = FALSE;
            wchk->wchk_cd = cd;
            wchk->wchk_key = key;

            rs_key_link(cd, key);

            ss_pprintf_4(("trx_addwritecheck: add write check\n"));
            ss_output_4(dbe_bkey_dprintvtpl(4, wchk->wchk_minkey));

            su_list_insertfirst(&trx->trx_writechklist, wchk);

            SS_MEMOBJ_INC(SS_MEMOBJ_TRXWCHK, trx_writecheck_t);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_addwrite
 *
 * Adds a write operation to a transaction. The write operations is
 * buffered to a local buffer to be saved to the transaction log at
 * commit time. If only the write set is specified to be checked at
 * transaction commit time, also the info for write check is added
 * to a local buffer. The write operation can be insert or delete.
 *
 * Parameters :
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 *      insertp - in
 *              If TRUE, the key value is inserted, otherwise the key
 *              value is deleted.
 *
 *      key - in, use
 *              Key definition for dvtpl.
 *
 *      tref - in, use
 *              Tuple reference taken from dvtpl, if key is a clustering
 *              key. May be also NULL.
 *
 *      nmergekeys - in
 *          Number of merge keys in this index write.
 *
 *      isonlydelemark - in
 *          If it is certain, that key the inserted key value is the only
 *          delete mark in the node, isonlydelemark is TRUE.
 *
 *      reltype - in
 *              Relation type.
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_addwrite(
        dbe_trx_t* trx,
        bool insertp,
        rs_key_t* key,
        dbe_tref_t* tref,
        uint nmergekeys,
        bool isonlydeletemark,
        rs_relh_t* relh __attribute__ ((unused)),
        rs_reltype_t reltype)
{
        void* cd;
        trx_mode_t mode;
        dbe_ret_t rc;
        bool do_writecheck = FALSE;    /* If TRUE, a write check is done
                                          for the tuple. */

        CHK_TRX(trx);
        ss_pprintf_1(("dbe_trx_addwrite, userid = %d, isonlydeletemark = %d\n", dbe_user_getid(trx->trx_user), isonlydeletemark));
        ss_dassert(!trx->trx_opactivep);
        ss_debug(trx->trx_opactivep = TRUE);
        ss_dassert(dbe_trx_checkcardininfo(trx));

        trx->trx_nindexwrites++;
        trx->trx_nmergewrites += nmergekeys;

        rs_sysi_addmergewrites(trx->trx_cd, nmergekeys);

        if (trx->trx_errcode != DBE_RC_SUCC) {
            /* transaction will fail */
            ss_debug(trx->trx_opactivep = FALSE);
            return(trx->trx_errcode);
        }

#ifdef SS_HSBG2
        if (ss_migratehsbg2) {
            rc = dbe_trx_markwrite(trx, TRUE);
            if (rc != DBE_RC_SUCC) {
                ss_debug(trx->trx_opactivep = FALSE);
                return(rc);
            }
        } else {
            ss_dassert(trx->trx_mode != TRX_NOWRITES);
            ss_dassert(trx->trx_hsbstmtmarkwrittentolog ||
                       trx->trx_log == NULL ||
                       trx->trx_mode == TRX_NOCHECK);
            ss_dassert(trx->trx_stmtiswrites);
            rc = DBE_RC_SUCC;
        }
#else /* SS_HSBG2 */
        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            ss_debug(trx->trx_opactivep = FALSE);
            return(rc);
        }
#endif /* SS_HSBG2 */

        /* Relh locking is not needed here any more because it is locked
           already at dbe4tupl.c. */

        if (reltype != RS_RELTYPE_OPTIMISTIC) {
            ss_debug(trx->trx_opactivep = FALSE);
            return(DBE_RC_SUCC);
        }

        dbe_trx_sementer(trx);

        cd = trx->trx_cd;

        mode = trx->trx_mode;

#ifndef SS_NOTRXREADCHECK
        if (mode == TRX_CHECKREADS && trx->trx_earlyvld) {
            /* During early validate, do lost update checks immediately
             * also when using read set validation.
             */
            mode = TRX_CHECKWRITES;
        }
#endif /* SS_NOTRXREADCHECK */

        if (rs_key_isclustering(cd, key)) {
            if (!insertp && mode == TRX_CHECKWRITES) {
                if (trx->trx_earlyvld) {
                    /* Info about the delete mark uniqueness can be used
                     * with early validate.
                     */
                    do_writecheck = !isonlydeletemark;
                    ss_beta(if (isonlydeletemark) { SS_PMON_ADD_BETA(SS_PMON_DBE_TRX_ONLYDELETEMARK); })
                } else {
                    do_writecheck = TRUE;
                }
                ss_beta(if (do_writecheck) { SS_PMON_ADD_BETA(SS_PMON_DBE_TRX_NEEDWRITECHECK); })
            }
        }

        switch (mode) {

            case TRX_NOCHECK:
            case TRX_CHECKREADS:
            case TRX_REPLICASLAVE:
                ss_dassert(DBE_TRXNUM_EQUAL(trx->trx_tmpmaxtrxnum, DBE_TRXNUM_NULL));
                break;

            case TRX_CHECKWRITES:
                if (do_writecheck) {
                    /* Key delete and clustering key. The write check
                     * is not needed for inserts, only key delete can
                     * conflict with other transactions. Write check is
                     * done only for the clustering key.
                     */
                    ss_dassert(tref != NULL);
                    ss_debug(if (!DBE_TRXNUM_EQUAL(trx->trx_tmpmaxtrxnum, DBE_TRXNUM_NULL)) {
                        ss_dassert(DBE_TRXNUM_EQUAL(trx->trx_tmpmaxtrxnum, tref->tr_readlevel));
                    });
                    ss_dassert(trx->trx_mode == TRX_CHECKWRITES ||
                               (trx->trx_earlyvld && trx->trx_mode == TRX_CHECKREADS));
                    trx_addwritecheck(
                        trx,
                        key,
                        tref->tr_vtpl,
                        tref->tr_readlevel);
                }
                break;

            default:
                ss_rc_error(trx->trx_mode);
        }
        ss_debug(trx->trx_opactivep = FALSE);
        dbe_trx_semexit(trx);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_checklostupdate
 *
 * Does a lost update check for tuple identified by tref. Used in optimistic
 * FOR UPDATE seaches where it must ensured that there is no confict with
 * the read tuple.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      tref -
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
dbe_ret_t dbe_trx_checklostupdate(
        dbe_trx_t* trx,
        dbe_tref_t* tref,
        rs_key_t* key,
        bool pessimistic)
{
        dbe_ret_t rc;
        trx_writecheck_t cur_wchk;
        dbe_keyvld_t keyvldtype;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_checklostupdate, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_ensureusertrxid(trx);

        dbe_trx_sementer(trx);

        if (trx->trx_nonrepeatableread) {
            ss_dprintf_2(("dbe_trx_checklostupdate, DBE_KEYVLD_READCOMMITTED_FORUPDATE\n"));
            keyvldtype = DBE_KEYVLD_READCOMMITTED_FORUPDATE;
        } else {
            ss_dprintf_2(("dbe_trx_checklostupdate, DBE_KEYVLD_NONE\n"));
            keyvldtype = DBE_KEYVLD_NONE;
        }

        ss_dassert(!trx->trx_opactivep);
        ss_debug(trx->trx_opactivep = TRUE);

        cur_wchk.wchk_stmttrxid = trx->trx_stmttrxid;
        cur_wchk.wchk_minkey = dbe_tref_getvtpl(tref);
        cur_wchk.wchk_maxkey = NULL;
        cur_wchk.wchk_maxtrxnum = dbe_tref_getreadlevel(tref);
        cur_wchk.wchk_key = key;

        rc = trx_validate_one_write(trx, &cur_wchk, keyvldtype, pessimistic);

        ss_dprintf_2(("dbe_trx_checklostupdate, rc = %s\n", su_rc_nameof(rc)));

        ss_debug(trx->trx_opactivep = FALSE);

        dbe_trx_semexit(trx);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_checkoldupdate
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh -
 *
 *
 *      tref -
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
dbe_ret_t dbe_trx_checkoldupdate(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* tref)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_checkoldupdate, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (!dbe_tref_isvalidlockname(tref)) {
            /* Error, the row is not the current row any more, the row has
               been updated in the same transaction.
             */
            rc = DBE_ERR_LOSTUPDATE;
        } else {
            /* Mark all locks on the current row as old.
             */
            dbe_user_marksearchesrowold(
                trx->trx_user,
                rs_relh_relid(trx->trx_cd, relh),
                tref->tr_lockname);
        }

        ss_dprintf_2(("dbe_trx_checkoldupdate, rc = %s\n", su_rc_nameof(rc)));

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_keypartsupdated
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      key -
 *
 *
 *      nparts -
 *
 *
 *      upd_attrs -
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
bool dbe_trx_keypartsupdated(
        void* cd __attribute__ ((unused)),
        rs_key_t* key,
        uint nparts,
        bool* upd_attrs)
{
        ss_dprintf_3(("dbe_trx_keypartsupdated\n"));

        if (upd_attrs != NULL) {
            /* Check if some of the attributes has been changed.
             */
            bool updated = FALSE;
            uint i;
            if (nparts == (uint)-1) {
                nparts = rs_key_lastordering(cd, key) + 1;
            }
            for (i = 0; i < nparts; i++) {
                rs_ano_t ano;
                ano = rs_keyp_ano(cd, key, i);
                if (ano != RS_ANO_NULL && upd_attrs[ano]) {
                    updated = TRUE;
                    break;
                }
            }
            return(updated);
        } else {
            /* No info about updates, this must be insert or delete. */
            return(TRUE);
        }
}

/*#***********************************************************************\
 *
 *              keycheck_escalate
 *
 * Escalate key check if necessary.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      type -
 *
 *
 *      key -
 *
 *
 *      vtpl -
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
static bool keycheck_escalate(
        dbe_trx_t* trx,
        dbe_keyvld_t type,
        rs_key_t* key,
        vtpl_t* vtpl)
{
        su_list_node_t* n;
        trx_keycheck_t* kchk;
        bool escalatep = FALSE;
        rs_sysi_t* cd;
        long keyid;

        ss_dprintf_3(("keycheck_escalate\n"));
        ss_dassert(dbe_trx_semisentered(trx));

        if (su_list_length(&trx->trx_keychklist) <=
            (uint)trx->trx_escalatelimits->esclim_check) {
            ss_dprintf_4(("keycheck_escalate:no escalate, list len = %d\n",
                su_list_length(&trx->trx_keychklist)));
            return(FALSE);
        }

        ss_dprintf_4(("keycheck_escalate:try to escalate, list len = %d\n",
            su_list_length(&trx->trx_keychklist)));

        cd = trx->trx_cd;
        keyid = rs_key_id(cd, key);

        su_list_do_get(&trx->trx_keychklist, n, kchk) {
            if (!DBE_TRXID_EQUAL(kchk->kchk_stmttrxid, trx->trx_stmttrxid)) {
                /* Different statement, skip this key check. */
                continue;
            }
            if (kchk->kchk_type == type &&
                rs_key_id(cd, kchk->kchk_key) == (ulong)keyid &&
                !SU_BFLAG_TEST(kchk->kchk_flags, TRX_KCHK_MAINMEMORY)) {
                /* Escalate old key check.
                 */
                ss_dprintf_4(("keycheck_escalate:escalate this key check\n"));
                if (kchk->kchk_rangemax == NULL) {
                    dynvtpl_setvtplwithincrement(
                        &kchk->kchk_rangemax,
                        kchk->kchk_rangemin);
                }
                if (vtpl_compare(vtpl, kchk->kchk_rangemin) < 0) {
                    dynvtpl_setvtpl(&kchk->kchk_rangemin, vtpl);
                } else if (vtpl_compare(vtpl, kchk->kchk_rangemax) > 0) {
                    dynvtpl_setvtplwithincrement(&kchk->kchk_rangemax, vtpl);
                }
                SU_BFLAG_SET(kchk->kchk_flags, TRX_KCHK_ESCALATED);
                escalatep = TRUE;
                break;
            }
        }
        return(escalatep);
}

/*##**********************************************************************\
 *
 *              dbe_trx_adduniquecheck
 *
 * Adds a unique check to the transaction. The unique key specified
 * by this functions is validated at transaction commit time.
 *
 * Parameters :
 *
 *      cd - in, use
 *              Client data.
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 *      key - in, use
 *              Unique key definition
 *
 *      key_vtpl - in, use
 *              V-tuple containing the attribute values of the unique key.
 *
 *      upd_attrs - in, use
 *          If not NULL, a boolean array containg TRUE for those attributes
 *          that has been updated.
 *
 *      reltype - in
 *              Relation type.
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_adduniquecheck(
        void* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key,
        vtpl_t* key_vtpl,
        bool* upd_attrs,
        rs_reltype_t reltype)
{
        dynvtpl_t rangemin_dvtpl = NULL;
        int nordering;
        dbe_btrsea_timecons_t tc;
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dassert(rs_key_isunique(cd, key));
        ss_dprintf_1(("dbe_trx_adduniquecheck, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(!trx->trx_opactivep);
        ss_debug(trx->trx_opactivep = TRUE);
        ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));
        ss_aassert(cd == trx->trx_cd);

        if (trx->trx_errcode != DBE_RC_SUCC ||
            trx->trx_mode == TRX_NOCHECK ||
            trx->trx_mode == TRX_REPLICASLAVE ||
            trx->trx_nointegrity) {
            /* transaction will fail or no checks are needed */
            ss_debug(trx->trx_opactivep = FALSE);
            return(DBE_RC_SUCC);
        }

        nordering = rs_key_lastordering(cd, key) + 1;

        ss_dprintf_2(("nordering = %d\n", nordering));

        if (!dbe_trx_keypartsupdated(cd, key, nordering, upd_attrs)) {
            /* None of the attributes in ordering part has been
             * changed, there is no need for unique check.
             */
            ss_debug(trx->trx_opactivep = FALSE);
            return(DBE_RC_SUCC);
        }

        dbe_trx_sementer(trx);

        /* Create a v-tuple containing all ordering parts of the unique
         * key. Those parts must be unique, and they are used as the
         * the starting key value of the range search.
         */
        trx_keycheck_builduniquecheck(cd, key, key_vtpl, &rangemin_dvtpl);

        if (keycheck_escalate(trx, DBE_KEYVLD_UNIQUE, key, rangemin_dvtpl)) {
            dynvtpl_free(&rangemin_dvtpl);
        } else {
            /* Add a new key check.
             */
            trx_keycheck_t* kchk;

            kchk = SSMEM_NEW(trx_keycheck_t);

            kchk->kchk_type = DBE_KEYVLD_UNIQUE;
            kchk->kchk_rangemin = rangemin_dvtpl;
            kchk->kchk_rangemax = NULL;
            kchk->kchk_flags = 0;
            if (trx->trx_earlyvld || reltype != RS_RELTYPE_OPTIMISTIC) {
                SU_BFLAG_SET(kchk->kchk_flags, TRX_KCHK_EARLYVLD);
            }
            if (reltype == RS_RELTYPE_MAINMEMORY) {
                SU_BFLAG_SET(kchk->kchk_flags, TRX_KCHK_MAINMEMORY);
            }
            kchk->kchk_stmttrxid = trx->trx_stmttrxid;
            kchk->kchk_cd = cd;
            kchk->kchk_key = key;
            kchk->kchk_relh = relh;

            rs_key_link(cd, key);
            rs_relh_link(cd, relh);

            if (trx->trx_earlyvld || reltype != RS_RELTYPE_OPTIMISTIC) {
                SU_BFLAG_SET(kchk->kchk_flags, TRX_KCHK_FULLCHECK);
            } else {
                /* Check the unique insert against previously committed
                 * transactions.
                 */
                tc.tc_mintrxnum = DBE_TRXNUM_MIN;
                if (reltype != RS_RELTYPE_OPTIMISTIC) {
                    tc.tc_maxtrxnum = DBE_TRXNUM_MAX;
                } else {
                    tc.tc_maxtrxnum = trx->trx_info->ti_maxtrxnum;
                }
                tc.tc_usertrxid = trx->trx_info->ti_usertrxid;
                tc.tc_maxtrxid = DBE_TRXID_MAX;
                tc.tc_trxbuf = NULL;

                rc = trx_keycheck(trx, kchk, &tc);

                /* If the check failed, at commit time we must do a
                 * full check.
                 */
                if (rc != DBE_RC_SUCC) {
                    SU_BFLAG_SET(kchk->kchk_flags, TRX_KCHK_FULLCHECK);
                    rc = DBE_RC_SUCC;
                }
            }
            su_list_insertlast(&trx->trx_keychklist, kchk);

            SS_MEMOBJ_INC(SS_MEMOBJ_TRXKCHK, trx_keycheck_t);
        }

        ss_debug(trx->trx_opactivep = FALSE);

        dbe_trx_semexit(trx);
        ss_dassert(rc == DBE_RC_SUCC);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_seterrkey
 *
 * Sets error key.
 *
 * Parameters :
 *
 *      trx - in
 *
 *      key - in
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_trx_seterrkey(
        dbe_trx_t* trx,
        rs_key_t* key)
{
        if (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX) {
            return;
        }
        CHK_TRX(trx);
        if (trx->trx_err_key != NULL) {
            rs_key_done(trx->trx_cd, trx->trx_err_key);
        }
        trx->trx_err_key = key;
        if (key != NULL) {
            rs_key_link(trx->trx_cd, key);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_giveerrkey
 *
 * Give a error key.
 *
 * Parameters :
 *
 *      trx - in
 *
 * Return value : pointer to key where constraint error happened or NULL
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_key_t* dbe_trx_giveerrkey(
        dbe_trx_t* trx)
{
        rs_key_t* key;

        CHK_TRX(trx);

        key = trx->trx_err_key;
        trx->trx_err_key = NULL;
        return (key);
}

/*##**********************************************************************\
 *
 *              dbe_trx_giveuniquerrorkeyvalue
 *
 * Give a unique error key value.
 *
 * Parameters :
 *
 *      trx - in
 *
 * Return value : pointer to key value or NULL - give
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dstr_t dbe_trx_giveuniquerrorkeyvalue(
        dbe_trx_t* trx)
{
        dstr_t value;

        CHK_TRX(trx);

        value = trx->trx_uniquerrorkeyvalue;
        trx->trx_uniquerrorkeyvalue = NULL;
        return(value);
}

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
/*##**********************************************************************\
 *
 *              dbe_trx_set_foreign_key_check
 *
 * Sets a foreign_key_check value.
 *
 * Parameters :
 *
 *      trx - in
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_trx_set_foreign_key_check(
        dbe_trx_t* trx,
        bool value)
{
        CHK_TRX(trx);

        trx->trx_foreign_key_checks = value;
}

/*##**********************************************************************\
 *
 *              dbe_trx_get_foreign_key_check
 *
 * Give a foreign_key_check value.
 *
 * Parameters :
 *
 *      trx - in
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_trx_get_foreign_key_check(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return trx->trx_foreign_key_checks;
}
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

#ifdef REFERENTIAL_INTEGRITY

/*##**********************************************************************\
 *
 *              dbe_trx_addrefkeycheck
 *
 * Adds referential integrity check to the transaction object.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      trx - use
 *
 *
 *      clustkey - in
 *
 *
 *      refkey - in
 *
 *
 *      key_vamap - in
 *
 *
 *      upd_attrs - in
 *
 *
 *      reltype - in
 *              If TRUE, the locking is used for unique key table.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_trx_addrefkeycheck(
        void*           cd,
        dbe_trx_t*      trx,
        rs_relh_t*      relh,
        rs_key_t*       clustkey,
        rs_key_t*       refkey,
        rs_relh_t*      refrelh,
        vtpl_vamap_t*   key_vamap,
        bool*           upd_attrs,
        rs_reltype_t    reltype)
{
        trx_keycheck_t* kchk;
        dynvtpl_t rangemin_dvtpl = NULL;
        int i;
        int nrefkeyparts;
        int keytype;
#ifdef SS_COLLATION
        union {
                void* dummy_for_alignment;
                ss_byte_t _buf[BUFVA_MAXBUFSIZE];
        } keyva;
        bool free_avalbuf = FALSE;
        rs_atype_t* atype;
        rs_aval_t avalbuf;

        bufva_init(keyva._buf, sizeof(keyva._buf));
#endif /* SS_COLLATION */

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_addrefkeycheck, userid = %d\n", dbe_user_getid(trx->trx_user)));

        SS_PUSHNAME("dbe_trx_addrefkeycheck");

        if (trx->trx_errcode != DBE_RC_SUCC ||
            trx->trx_mode == TRX_NOCHECK ||
            trx->trx_mode == TRX_REPLICASLAVE ||
            trx->trx_nointegrity ||
            trx->trx_norefintegrity
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
            || !trx->trx_foreign_key_checks
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */
            ) {
            /* transaction will fail or no checks are needed */
            SS_POPNAME;
            return(DBE_RC_SUCC);
        }

        nrefkeyparts = rs_key_nparts(cd, refkey);

        ss_dprintf_2(("nrefkeyparts = %d\n", nrefkeyparts));

        if (!dbe_trx_keypartsupdated(cd, refkey, nrefkeyparts, upd_attrs)) {
            /* None of the attributes in refkey part has been
             * changed, there is no need for a check.
             */

            SS_POPNAME;
            return(DBE_RC_SUCC);
        }

        dbe_trx_sementer(trx);

        keytype = rs_key_type(cd, refkey);

        /* Create a v-tuple containing all reference key parts.
         * Those parts must mark the start of the check range.
         */
        dynvtpl_setvtpl(&rangemin_dvtpl, VTPL_EMPTY);
        for (i = 0; i < nrefkeyparts; i++) {
            va_t* va = NULL;
            if (rs_keyp_isconstvalue(cd, refkey, i)) {
                va = rs_keyp_constvalue(cd, refkey, i);
            } else if (key_vamap != NULL) {
                uint kpno;  /* Key part number in clustering key. */
                rs_ano_t ano;
                int prefixlen;

                ano = rs_keyp_ano(cd, refkey, i);
                kpno = rs_key_searchkpno_data(cd, clustkey, ano);
                ss_dassert(kpno != (uint)RS_ANO_NULL);
                va = vtpl_vamap_getva_at(key_vamap, kpno);
                if (keytype == RS_KEY_FORKEYCHK && va_testnull(va)) {
                    /* ANSI says that if any of the foreign key fields is
                     * NULL, there need not be a matching primary key value.
                     */
                    dynvtpl_free(&rangemin_dvtpl);
                    dbe_trx_semexit(trx);
#ifdef SS_COLLATION
                    bufva_done(keyva._buf, sizeof(keyva._buf));
#endif /* SS_COLLATION */
                    SS_POPNAME;
                    return(DBE_RC_SUCC);
                }
#ifdef SS_COLLATION
                if (rs_keyp_parttype(cd, refkey, i) == RSAT_COLLATION_KEY) {
                    prefixlen = rs_keyp_getprefixlength(cd, refkey, i);
                    atype = rs_ttype_atype(cd, rs_relh_ttype(cd, relh), ano);
                    rs_aval_createbuf(cd, atype, &avalbuf);
                    rs_aval_setva(cd, atype, &avalbuf, va);

                    va = rs_aval_getkeyva(
                            cd,
                            atype, 
                            &avalbuf, 
                            rs_keyp_collation(cd, refkey, i),
                            rs_keyp_parttype(cd, refkey, i),
                            rs_keyp_isascending(cd, refkey, i),
                            keyva._buf, 
                            sizeof(keyva._buf),
                            prefixlen);
                    
                    ss_assert(va != NULL);
                    free_avalbuf = TRUE;
                } else {
                    free_avalbuf = FALSE;
                }
#endif /* SS_COLLATION */
            }
            
            if (va != NULL) {
                dynvtpl_appva(&rangemin_dvtpl, va);
            }
#ifdef SS_COLLATION
            if (free_avalbuf) {
                rs_aval_freebuf(cd, atype, &avalbuf);
                free_avalbuf = FALSE;
            }
#endif /* SS_COLLATION */
        }

        kchk = SSMEM_NEW(trx_keycheck_t);

        if (keytype == RS_KEY_PRIMKEYCHK) {
            /* Check that child key value does not exist. */
            kchk->kchk_type = DBE_KEYVLD_PRIMARY;
        } else {
            /* Check that parent key value does exists. */
            ss_dassert(keytype == RS_KEY_FORKEYCHK);
            kchk->kchk_type = DBE_KEYVLD_FOREIGN;
        }
        kchk->kchk_rangemin = rangemin_dvtpl;
        kchk->kchk_rangemax = NULL;
        kchk->kchk_stmttrxid = trx->trx_stmttrxid;
        kchk->kchk_flags = 0;
        SU_BFLAG_SET(kchk->kchk_flags, TRX_KCHK_FULLCHECK);
        if (trx->trx_earlyvld || reltype != RS_RELTYPE_OPTIMISTIC) {
            SU_BFLAG_SET(kchk->kchk_flags, TRX_KCHK_EARLYVLD);
        }
        if (reltype == RS_RELTYPE_MAINMEMORY) {
            SU_BFLAG_SET(kchk->kchk_flags, TRX_KCHK_MAINMEMORY);
        }
        kchk->kchk_cd = NULL;
        kchk->kchk_key = refkey;
        rs_key_link(cd, refkey);
        kchk->kchk_relh = refrelh;
        rs_relh_link(cd, refrelh);

        su_list_insertlast(&trx->trx_keychklist, kchk);

        SS_MEMOBJ_INC(SS_MEMOBJ_TRXKCHK, trx_keycheck_t);

        dbe_trx_semexit(trx);

#ifdef SS_COLLATION
        bufva_done(keyva._buf, sizeof(keyva._buf));
#endif /* SS_COLLATION */

        SS_POPNAME;
        return(DBE_RC_SUCC);
}

#endif /* REFERENTIAL_INTEGRITY */

/*#***********************************************************************\
 *
 *              trx_addreadcheck_nomutex
 *
 * Adds a read check to the transaction. The read range specified by
 * this function is used to validate the read set of the transaction
 * at commit time.
 *
 * Parameters :
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 *      plan - in, take
 *              Search plan.
 *
 *      lastkey - in, take
 *              Last (greatest) key returned by the search.
 *
 *      lasttrxid - in
 *              Transaction id of parameter lastkey.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void trx_addreadcheck_nomutex(
        dbe_trx_t* trx,
        rs_pla_t* plan,
        dynvtpl_t lastkey,
        dbe_trxid_t lasttrxid)
{
        uint listlen;
        trx_readcheck_t* rchk;

        CHK_TRX(trx);
        ss_dprintf_1(("trx_addreadcheck_nomutex, userid = %d\n", dbe_user_getid(trx->trx_user)));

        /* Add a new read check.
         */
        ss_dprintf_2(("trx_addreadcheck_nomutex:add new read check, list len = %ld\n",
            (long)su_list_length(&trx->trx_readchklist)));

        rchk = SSMEM_NEW(trx_readcheck_t);

        rchk->rchk_cd = trx->trx_cd;
        rchk->rchk_plan = plan;
        rchk->rchk_lastkey = lastkey;
        rchk->rchk_lasttrxid = lasttrxid;
        rchk->rchk_stmttrxid = trx->trx_stmttrxid;
        su_list_insertfirst(&trx->trx_readchklist, rchk);
        SS_MEMOBJ_INC(SS_MEMOBJ_TRXRCHK, trx_readcheck_t);
        listlen = su_list_length(&trx->trx_readchklist);
        if ((listlen % TRX_READESCALATE_CHECKLIMIT) == 0) {
            trx_escalate_readset(trx);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_addreadcheck
 *
 * Adds a read check to the transaction. The read range specified by
 * this function is used to validate the read set of the transaction
 * at commit time. The read set validation is not done, if the transaction
 * is read-only or only the write set is specified to be checked at
 * commit.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      plan -
 *
 *
 *      lastkey -
 *
 *
 *      lasttrxid -
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
void dbe_trx_addreadcheck(
        dbe_trx_t* trx,
        rs_pla_t* plan,
        dynvtpl_t lastkey,
        dbe_trxid_t lasttrxid)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_addreadcheck, userid = %d\n", dbe_user_getid(trx->trx_user)));

        ss_dassert(dbe_trx_needtoaddreadcheck(trx));

        dbe_trx_sementer(trx);

        trx_addreadcheck_nomutex(
            trx,
            plan,
            lastkey,
            lasttrxid);

        dbe_trx_semexit(trx);
}

#ifdef SS_MIXED_REFERENTIAL_INTEGRITY
dbe_ret_t dbe_trx_refkeycheck(
        void*           cd,
        dbe_trx_t*      trx,
        rs_key_t*       clustkey __attribute__ ((unused)),
        rs_key_t*       refkey,
        rs_ttype_t*     ttype,
        rs_tval_t*      tval)
{
        dynvtpl_t       rangemin_dvtpl = NULL;
        dynvtpl_t       rangemax_dvtpl = NULL;
        dbe_ret_t       rc;
        rs_ano_t        ano;
        rs_atype_t*     atype;
        rs_aval_t*      aval;
        va_t*           va;
        ulong           i;
        ulong           nrefkeyparts;
        rs_keytype_t    keytype;
        dbe_index_t*    index;
        dbe_indsea_t*   indsea;
        dbe_searchrange_t   sr;
        dbe_srk_t*      srk;
        dbe_ret_t       indsea_rc;
        dbe_btrsea_timecons_t   tc;

        SS_PUSHNAME("dbe_trx_refkeycheck");

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_refkeycheck\n"));

        dbe_trx_ensurereadlevel(trx, TRUE);

        keytype = rs_key_type(cd, refkey);
        nrefkeyparts = rs_key_nparts(cd, refkey);
        rc = DBE_RC_SUCC;

        dynvtpl_setvtpl(&rangemin_dvtpl, VTPL_EMPTY);
        dynvtpl_setvtpl(&rangemax_dvtpl, VTPL_EMPTY);
        for (i = 0; i < nrefkeyparts; i++) {
            if (rs_keyp_isconstvalue(cd, refkey, i)) {
                va = rs_keyp_constvalue(cd, refkey, i);
            } else {
                ano = rs_keyp_ano(cd, refkey, i);
                atype = rs_ttype_atype(cd, ttype, ano);
                aval = rs_tval_aval(cd, ttype, tval, ano);
                va = rs_aval_va(cd, atype, aval);
                if (keytype == RS_KEY_FORKEYCHK && va_testnull(va)) {
                    /* ANSI says that if any of the foreign key fields is
                       NULL, there doesn't need to be a matching parent
                       value. */
                    rc = DBE_RC_SUCC;
                    goto exit_function;
                }
            }

            dynvtpl_appva(&rangemin_dvtpl, va);
        }

        dynvtpl_setvtplwithincrement(&rangemax_dvtpl, rangemin_dvtpl);
        tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        if (keytype == RS_KEY_FORKEYCHK) {
            tc.tc_maxtrxnum = trx->trx_info->ti_maxtrxnum;
        } else {
            tc.tc_maxtrxnum = DBE_TRXNUM_MAX;
        }
        tc.tc_usertrxid = trx->trx_info->ti_usertrxid;
        tc.tc_maxtrxid = DBE_TRXID_MAX;
        tc.tc_trxbuf = NULL;

        sr.sr_minvtpl = rangemin_dvtpl;
        sr.sr_minvtpl_closed = TRUE;
        sr.sr_maxvtpl = rangemax_dvtpl;
        sr.sr_maxvtpl_closed = TRUE;

        index = dbe_user_getindex(trx->trx_user);
        indsea = dbe_indsea_init(
                cd,
                index,
                refkey,
                &tc,
                &sr,
                NULL,
                LOCK_FREE,
                "dbe_trx_refkeycheck");
        dbe_indsea_setvalidate(
                indsea,
                (keytype == RS_KEY_FORKEYCHK
                 ? DBE_KEYVLD_FOREIGN
                 : DBE_KEYVLD_PRIMARY),
                TRUE);

        do {
            indsea_rc = dbe_indsea_next(
                    indsea,
                    DBE_TRXID_NULL,
                    &srk);
        } while (indsea_rc == DBE_RC_NOTFOUND);

        switch (keytype) {
            case RS_KEY_PRIMKEYCHK:
                /* Check that the child key value does not exist. */
                if (indsea_rc == DBE_RC_FOUND) {
                    rc = DBE_ERR_CHILDEXIST_S;
                    dbe_trx_seterrkey(trx, refkey);
                } else if (indsea_rc != DBE_RC_END) {
                    rc = indsea_rc;
                }
                break;

            case RS_KEY_FORKEYCHK:
                /* Check that parent key value exists. */
                if (indsea_rc == DBE_RC_FOUND) {
                    /* Success */
                } else if (indsea_rc == DBE_RC_END) {
                    rc = DBE_ERR_PARENTNOTEXIST_S;
                    dbe_trx_seterrkey(trx, refkey);
                } else {
                    rc = indsea_rc;
                }
                break;

            default:
                ss_error;
        }

        dbe_indsea_done(indsea);

 exit_function:
        dynvtpl_free(&rangemin_dvtpl);
        dynvtpl_free(&rangemax_dvtpl);

        SS_POPNAME;

        return rc;
}

/*##**********************************************************************\
 *
 *              dbe_trx_mme_refkeycheck
 *
 * Wrapper to call mme_refkeycheck from trx layer.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx -
 *
 *
 *      refkey -
 *
 *
 *      refrelh -
 *
 *
 *      stmtid
 *
 *
 *      value -
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
dbe_ret_t dbe_trx_mme_refkeycheck(
        rs_sysi_t*          cd,
        dbe_trx_t*          trx,
        rs_key_t*           refkey,
        rs_relh_t*          refrelh,
        dbe_trxid_t         stmtid,
        dynvtpl_t           value)
{
        return dbe_mme_refkeycheck(cd, refkey, refrelh, trx, stmtid, value);
}

#endif /* SS_MIXED_REFERENTIAL_INTEGRITY */
