/*************************************************************************\
**  source       * dbe4nomme.c
**  directory    * dbe
**  description  * Dummy function for main memory transaction processing.
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

#define DBE_NOMME

#include <ssc.h>
#include <ssdebug.h>

#ifndef SS_MYSQL
#include <mme0stor.h>
#include <mme0page.h>
#include <mme0rval.h>
#endif

#include "dbe4mme.h"

#ifdef SS_MMEG2
#define dbe_mme_locklist_t  mme_locklist_t
#endif

dbe_mme_t* dbe_mme_init(
        dbe_db_t*       db __attribute__ ((unused)),
        dbe_cfg_t*      cfg __attribute__ ((unused)),
        dbe_cpnum_t     cpnum __attribute__ ((unused)),
        void*           memctx __attribute__ ((unused)))
{
        return(NULL);
}

void dbe_mme_done(
        rs_sysi_t*      cd __attribute__ ((unused)),
        dbe_mme_t*      mme __attribute__ ((unused)))
{
}

void dbe_mme_setreadonly(
        dbe_mme_t*      mme __attribute__ ((unused)))
{
}

void dbe_mme_startservice(
        dbe_mme_t* mme __attribute__ ((unused)))
{
}

mme_storage_t* dbe_mme_getstorage(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_mme_t* mme __attribute__ ((unused)))
{
        return(NULL);
}

void mme_setmemctxtocd(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_mme_t* mme __attribute__ ((unused)))
{
}

void dbe_mme_removeuser(
        rs_sysi_t*  cd __attribute__ ((unused)),
        dbe_mme_t*  mme __attribute__ ((unused)))
{
}

void dbe_mme_gettemporarytablecardin(
        rs_sysi_t*      cd __attribute__ ((unused)),
        dbe_mme_t*      mme __attribute__ ((unused)),
        rs_relh_t*      relh __attribute__ ((unused)),
        ss_int8_t*      p_nrows __attribute__ ((unused)),
        ss_int8_t*      p_nbytes __attribute__ ((unused)))
{
}

/* TRANSACTIONAL SERVICES ------------------------------------------------- */
dbe_ret_t dbe_mme_insert(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_relh_t* relh __attribute__ ((unused)),
        rs_tval_t* tval __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_ret_t dbe_mme_delete(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_relh_t* relh __attribute__ ((unused)),
        dbe_tref_t* tref __attribute__ ((unused)),
        dbe_mme_search_t* search __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_ret_t dbe_mme_update(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_relh_t* relh __attribute__ ((unused)),
        dbe_tref_t* old_tref __attribute__ ((unused)),
        bool* new_attrs __attribute__ ((unused)),
        rs_tval_t* new_tval __attribute__ ((unused)),
        dbe_tref_t* new_tref __attribute__ ((unused)),
        dbe_mme_search_t* search __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_mme_search_t* dbe_mme_search_init(
        rs_sysi_t*          cd __attribute__ ((unused)),
        dbe_user_t*         user __attribute__ ((unused)),
        dbe_trx_t*          trx __attribute__ ((unused)),
        dbe_trxid_t         usertrxid __attribute__ ((unused)),
        rs_ano_t*           sellist __attribute__ ((unused)),
        rs_pla_t*           plan __attribute__ ((unused)),
        rs_relh_t*          relh __attribute__ ((unused)),
        rs_key_t*           key __attribute__ ((unused)),
        dbe_cursor_type_t   cursor_type __attribute__ ((unused)),
        bool*               p_newplan __attribute__ ((unused)))
{
        return(NULL);
}

void dbe_mme_search_done(
        dbe_mme_search_t*   search __attribute__ ((unused)))
{
}

void dbe_mme_search_reset(
        dbe_mme_search_t*   search __attribute__ ((unused)),
        dbe_trx_t*          trx __attribute__ ((unused)),
        rs_pla_t*           plan __attribute__ ((unused)))
{
}

rs_aval_t* dbe_mme_search_getaval(
        dbe_mme_search_t*   search __attribute__ ((unused)),
        rs_tval_t*          tval __attribute__ ((unused)),
        rs_atype_t*         atype __attribute__ ((unused)),
        uint                kpno __attribute__ ((unused)))
{
        return(NULL);
}

bool dbe_mme_search_getsearchinfo(
        dbe_mme_search_t*   search __attribute__ ((unused)),
        rs_pla_t**          p_plan __attribute__ ((unused)))
{
        return(FALSE);
}

void dbe_mme_search_invalidate(
        dbe_mme_search_t* search __attribute__ ((unused)),
        dbe_trxid_t usertrxid __attribute__ ((unused)))
{
}

void dbe_mme_search_restart(
        dbe_mme_search_t* search __attribute__ ((unused)),
        dbe_trx_t* trx __attribute__ ((unused)))
{
}

void dbe_mme_search_abortrelid(
        dbe_mme_search_t* search __attribute__ ((unused)),
        ulong relid __attribute__ ((unused)))
{
}

void dbe_mme_search_abortkeyid(
        dbe_mme_search_t* search __attribute__ ((unused)),
        ulong keyid __attribute__ ((unused)))
{
}

bool dbe_mme_search_isactive(
        dbe_mme_search_t* search __attribute__ ((unused)))
{
        return(FALSE);
}

dbe_ret_t dbe_mme_search_relock(
        dbe_mme_search_t*   search __attribute__ ((unused)),
        dbe_trx_t*          trx __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_ret_t dbe_mme_search_nextorprev_n(
        dbe_mme_search_t*   search __attribute__ ((unused)),
        dbe_trx_t*          trx __attribute__ ((unused)),
        bool                nextp __attribute__ ((unused)),
        rs_vbuf_t*          vb __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_ret_t dbe_mme_search_next(
        dbe_mme_search_t* search __attribute__ ((unused)),
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_tval_t** p_tval __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_ret_t dbe_mme_search_prev(
        dbe_mme_search_t* search __attribute__ ((unused)),
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_tval_t** p_tval __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_ret_t dbe_mme_search_gotoend(
        dbe_mme_search_t* search __attribute__ ((unused)),
        dbe_trx_t* trx __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_ret_t dbe_mme_search_setposition(
        dbe_mme_search_t* search __attribute__ ((unused)),
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_tval_t* tval __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_tref_t* dbe_mme_search_gettref(
        dbe_mme_search_t*   search __attribute__ ((unused)),
        rs_tval_t*          tval __attribute__ ((unused)))
{
        return(NULL);
}

void dbe_mme_search_setoptinfo(
        dbe_mme_search_t* search __attribute__ ((unused)),
        ulong tuplecount __attribute__ ((unused)))
{
}

void dbe_mme_search_newplan(
        dbe_mme_search_t*   search __attribute__ ((unused)),
        ulong               relid __attribute__ ((unused)))
{
}

void dbe_mme_search_printinfo(
        void* fp __attribute__ ((unused)),
        dbe_mme_search_t* search __attribute__ ((unused)))
{
}

dbe_mme_locklist_t* dbe_mme_locklist_init(
        rs_sysi_t*          cd __attribute__ ((unused)),
        dbe_mme_t*          mme __attribute__ ((unused)))
{
        return(NULL);
}

void dbe_mme_locklist_done(
        dbe_mme_locklist_t* mmll __attribute__ ((unused)))
{
}

void dbe_mme_locklist_replicafree(
        dbe_mme_locklist_t* mmll __attribute__ ((unused)))
{
}

void dbe_mme_locklist_rollback_searches(
        dbe_mme_locklist_t* mmll __attribute__ ((unused)))
{
}

void dbe_mme_locklist_commit(
        dbe_mme_locklist_t* mmll __attribute__ ((unused)))
{
}

void dbe_mme_locklist_rollback(
        dbe_mme_locklist_t* mmll __attribute__ ((unused)))
{
}

dbe_ret_t dbe_mme_locklist_stmt_commit(
        dbe_mme_locklist_t* mmll __attribute__ ((unused)),
        dbe_trxid_t         stmttrxid __attribute__ ((unused)),
        bool                groupstmtp __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

void dbe_mme_locklist_stmt_rollback(
        dbe_mme_locklist_t* mmll __attribute__ ((unused)),
        dbe_trxid_t         stmttrxid __attribute__ ((unused)))
{
}

#ifdef SS_MIXED_REFERENTIAL_INTEGRITY
dbe_ret_t dbe_mme_refkeycheck(
        rs_sysi_t*          cd __attribute__ ((unused)),
        rs_key_t*           refkey __attribute__ ((unused)),
        rs_relh_t*          refrelh __attribute__ ((unused)),
        dbe_trx_t*          trx __attribute__ ((unused)),
        dbe_trxid_t         stmtid __attribute__ ((unused)),
        dynvtpl_t           value __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}
#endif

/* CREATE/DROP INDEX ------------------------------------------------------- */
dbe_mme_createindex_t* dbe_mme_createindex_init(
        rs_sysi_t*              cd __attribute__ ((unused)),
        dbe_trx_t*              trx __attribute__ ((unused)),
        rs_relh_t*              relh __attribute__ ((unused)),
        rs_key_t*               key __attribute__ ((unused)),
        dbe_trxid_t             usertrxid __attribute__ ((unused)),
        dbe_trxnum_t            committrxnum __attribute__ ((unused)),
        bool                    commitp __attribute__ ((unused)))
{
        return(NULL);
}

void dbe_mme_createindex_done(
        dbe_mme_createindex_t*  ci __attribute__ ((unused)))
{
}

dbe_ret_t dbe_mme_createindex_advance(
        dbe_mme_createindex_t*  ci __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_mme_dropindex_t* dbe_mme_dropindex_init(
        rs_sysi_t*              cd __attribute__ ((unused)),
        dbe_trx_t*              trx __attribute__ ((unused)),
        rs_relh_t*              relh __attribute__ ((unused)),
        rs_key_t*               key __attribute__ ((unused)),
        dbe_trxid_t             usertrxid __attribute__ ((unused)),
        dbe_trxnum_t            committrxnum __attribute__ ((unused)),
        bool                    commitp __attribute__ ((unused)),
        bool                    isclearing __attribute__ ((unused)),
        bool                    allusers __attribute__ ((unused)))
{
        return(NULL);
}

void dbe_mme_dropindex_done(
        dbe_mme_dropindex_t*  di __attribute__ ((unused)))
{
}

dbe_ret_t dbe_mme_dropindex_advance(
        dbe_mme_dropindex_t*  di __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

/* RECOVERY SERVICES ------------------------------------------------------- */
void dbe_mme_beginrecov(
        dbe_mme_t*          mme __attribute__ ((unused)))
{
}

void dbe_mme_recovinsert(
        rs_sysi_t*          cd __attribute__ ((unused)),
        dbe_mme_t*          mme __attribute__ ((unused)),
        dbe_trxbuf_t*       tb __attribute__ ((unused)),
        rs_relh_t*          relh __attribute__ ((unused)),
        mme_page_t*         page __attribute__ ((unused)),
        mme_rval_t*         rval __attribute__ ((unused)),
        dbe_trxid_t         trxid __attribute__ ((unused)),
        dbe_trxid_t         stmtid __attribute__ ((unused)))
{
}

void dbe_mme_recovplacepage(
        rs_sysi_t*              cd __attribute__ ((unused)),
        dbe_mme_t*              mme __attribute__ ((unused)),
        rs_relh_t*              relh __attribute__ ((unused)),
        mme_page_t*             page __attribute__ ((unused)),
        mme_rval_t*             rval __attribute__ ((unused)),
        dbe_trxid_t             trxid __attribute__ ((unused)),
        dbe_trxid_t             stmtid __attribute__ ((unused)))
{
}

void dbe_mme_recovstmtcommit(
        dbe_mme_t*          mme __attribute__ ((unused)),
        dbe_trxid_t         trxid __attribute__ ((unused)),
        dbe_trxid_t         stmtid __attribute__ ((unused)))
{
}

void dbe_mme_recovstmtrollback(
        dbe_mme_t*          mme __attribute__ ((unused)),
        dbe_trxid_t         trxid __attribute__ ((unused)),
        dbe_trxid_t         stmtid __attribute__ ((unused)))
{
}

void dbe_mme_recovcommit(
        dbe_mme_t*          mme __attribute__ ((unused)),
        dbe_trxid_t         trxid __attribute__ ((unused)))
{
}

void dbe_mme_recovrollback(
        dbe_mme_t*          mme __attribute__ ((unused)),
        dbe_trxid_t         trxid __attribute__ ((unused)))
{
}

int dbe_mme_abortall(
        dbe_mme_t*              mme __attribute__ ((unused)))
{
        return(0);
}

void dbe_mme_endrecov(
        dbe_mme_t*              mme __attribute__ ((unused)))
{
}

mme_rval_t* dbe_mme_rval_init_from_diskbuf(
        rs_sysi_t* cd __attribute__ ((unused)),
        ss_byte_t* diskbuf __attribute__ ((unused)),
        size_t diskbuf_size __attribute__ ((unused)),
        mme_rval_t* prev __attribute__ ((unused)),
        void* owner __attribute__ ((unused)),
        mme_rval_type_t type __attribute__ ((unused)))
{
        return(NULL);
}

void dbe_mme_rval_done(
        rs_sysi_t* cd __attribute__ ((unused)),
        mme_rval_t* rval __attribute__ ((unused)),
        mme_rval_type_t type __attribute__ ((unused)))
{
}

#if !defined(SS_PURIFY) && defined(SS_FFMEM)
void* dbe_mme_getmemctx(dbe_mme_t* mme __attribute__ ((unused)))
{
        return(NULL);
}
#endif /* !SS_PURIFY && SS_FFMEM */

void dbe_mme_gettvalsamples_nomutex(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_mme_t* mme __attribute__ ((unused)),
        rs_relh_t* relh __attribute__ ((unused)),
        rs_tval_t** sample_tvalarr __attribute__ ((unused)),
        size_t sample_size __attribute__ ((unused)))
{
}

void dbe_mme_lock_mutex(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_mme_t* mme __attribute__ ((unused)))
{
}

void dbe_mme_unlock_mutex(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_mme_t* mme __attribute__ ((unused)))
{
}

#ifdef SS_DEBUG
void dbe_mme_check_integrity(
        dbe_mme_t*      mme __attribute__ ((unused)))
{
}
#endif

su_ret_t mme_storage_startcheckpoint(
        rs_sysi_t* cd __attribute__ ((unused)),
        mme_storage_t* storage __attribute__ ((unused)),
        dbe_cpnum_t cpnum __attribute__ ((unused)),
        su_daddr_t* p_pagedirstartaddr __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

su_ret_t mme_storage_getpageforcheckpoint(
        rs_sysi_t* cd __attribute__ ((unused)),    
        mme_storage_t* storage __attribute__ ((unused)),    /* in  - tuple storage */
        size_t         blocksize __attribute__ ((unused)),  /* in  - page size */
        char*          data __attribute__ ((unused)),       /* out - page data */
        su_daddr_t*    p_daddr __attribute__ ((unused)),      /* out - page disk address */
        char*          pageaddrs __attribute__ ((unused)),  /* out - addresses of pages */
        su_daddr_t*    p_pageaddrwhenfull __attribute__ ((unused)))   /* out - If not SU_DADDR_NULL, */
{
        return(SU_SUCCESS);
}

su_ret_t mme_storage_endcheckpoint(
        rs_sysi_t* cd __attribute__ ((unused)),
        mme_storage_t* storage __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

su_ret_t mme_storage_getaddressesfromdirpage(
        rs_sysi_t*   cd __attribute__ ((unused)),
        mme_storage_t* storage __attribute__ ((unused)),
        su_daddr_t   daddr __attribute__ ((unused)),
        ss_byte_t*   pagedata __attribute__ ((unused)),
        size_t       pagedata_size __attribute__ ((unused)),
        su_daddr_t*  daddr_array __attribute__ ((unused)),
        size_t       array_size __attribute__ ((unused)),
        size_t*      p_nread __attribute__ ((unused)),
        size_t*      p_npos __attribute__ ((unused)),
        su_daddr_t*  p_daddrpage __attribute__ ((unused)))
{
        return(SU_SUCCESS);
}

dbe_cpnum_t mme_storage_getcpnumfromdirpage(
        ss_byte_t* dirpage __attribute__ ((unused)))
{
        return(0);
}

mme_page_t* mme_storage_initreadpage(
        rs_sysi_t* cd __attribute__ ((unused)),
        mme_storage_t* storage __attribute__ ((unused)),
        su_daddr_t daddr __attribute__ ((unused)),
        ss_byte_t* page_image __attribute__ ((unused)),
        size_t page_size __attribute__ ((unused)),
        mme_pagescan_t* pagescan __attribute__ ((unused)))
{
        return(NULL);
}

void mme_storage_remove_page(
        rs_sysi_t* cd __attribute__ ((unused)),
        mme_storage_t* storage __attribute__ ((unused)),
        mme_page_t* page __attribute__ ((unused)))
{
}

bool mme_storage_move_page(
        rs_sysi_t* cd __attribute__ ((unused)),
        mme_storage_t* storage __attribute__ ((unused)),
        su_daddr_t old_address __attribute__ ((unused)))
{
        return(TRUE);
}

mme_rval_t* mme_page_scanrval(
        rs_sysi_t* cd __attribute__ ((unused)),
        mme_page_t* page __attribute__ ((unused)),
        mme_pagescan_t* pagescan __attribute__ ((unused)),
        bool* p_tentative __attribute__ ((unused)),
        dbe_trxid_t* p_trxid __attribute__ ((unused)),
        dbe_trxid_t* p_stmtid __attribute__ ((unused)))
{
        return(NULL);
}

#if defined(SS_DEBUG) || defined(SS_COVER)

ss_uint4_t mme_page_getrelid(
        mme_page_t* page __attribute__ ((unused)))
{
        return(0);
}

su_daddr_t mme_page_getdiskaddr(
        mme_page_t* page __attribute__ ((unused)))
{
        return(0);
}

#endif

mme_rval_t* mme_rval_init_from_tval(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_ttype_t*     ttype __attribute__ ((unused)),
        rs_tval_t*      tval __attribute__ ((unused)),
        rs_key_t*       clustkey __attribute__ ((unused)),
        mme_rval_t*     prev __attribute__ ((unused)),
        void*           owner __attribute__ ((unused)),
        bool            tentative __attribute__ ((unused)),
        mme_rval_type_t type __attribute__ ((unused)),
        ss_byte_t*      buf __attribute__ ((unused)),
        size_t          bufsize __attribute__ ((unused)),
        su_ret_t*       p_rc __attribute__ ((unused)))
{
        return(NULL);
}

mme_rval_t* mme_rval_init_from_rval(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_ttype_t*     ttype __attribute__ ((unused)),
        rs_key_t*       clustkey __attribute__ ((unused)),
        mme_rval_t*     src_rval __attribute__ ((unused)),
        rs_ano_t*       sellist __attribute__ ((unused)),
        mme_rval_t*     prev __attribute__ ((unused)),
        void*           owner __attribute__ ((unused)),
        bool            tentative __attribute__ ((unused)),
        bool            deletemark __attribute__ ((unused)),
        mme_rval_type_t type __attribute__ ((unused)))
{
        return(NULL);
}

void mme_rval_done(
        rs_sysi_t* cd __attribute__ ((unused)),
        mme_rval_t* rval __attribute__ ((unused)),
        mme_rval_type_t type __attribute__ ((unused)))
{
}


rs_tval_t* mme_rval_projecttotval(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_ttype_t*     ttype __attribute__ ((unused)),
        rs_tval_t*      tval_or_NULL __attribute__ ((unused)),
        rs_key_t*       clustkey __attribute__ ((unused)),
        rs_ano_t*       sellist __attribute__ ((unused)),
        mme_rval_t*     rval __attribute__ ((unused)),
        mme_rval_type_t type __attribute__ ((unused)))
{
        return(NULL);
}

ss_byte_t* mme_rval_getdata(
        mme_rval_t* rval __attribute__ ((unused)),
        size_t* p_datasize __attribute__ ((unused)))
{
        return(NULL);
        return(NULL);
}

mme_rval_t* mme_rval_init_from_diskbuf(
        rs_sysi_t* cd,
        ss_byte_t* diskbuf,
        size_t diskbuf_size,
        mme_rval_t* prev,
        void* owner,
        mme_rval_type_t type)
{
        return(NULL);
}

bool print_vtpl(void* fp, vtpl_t* vtpl)
{
        return(FALSE);
}

su_ret_t mme_ipos_setposition_exact(
        rs_sysi_t*          cd,
        mme_ipos_t*         ipos,
        vtpl_t*             keyvalue,
        ulong*              p_nretries,
        bool*               p_index_accessed)
{
        return(SU_SUCCESS);
}

void mme_bcur_setkey(
        rs_sysi_t*      cd,
        mme_bcur_t*     bc,
        vtpl_t*         keyvalue)
{
}

vtpl_t* mme_bcur_getkey(
        rs_sysi_t*      cd,
        mme_bcur_t*     bc)
{
        return(NULL);
}

#if defined(SS_DEBUG) || defined(SS_COVER)

bool mme_rval_getdeletemarkflag(
        mme_rval_t* rval __attribute__ ((unused)))
{
        return(FALSE);
}

#endif
