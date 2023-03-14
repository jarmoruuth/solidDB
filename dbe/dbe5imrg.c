/*************************************************************************\
**  source       * dbe5imrg.c
**  directory    * dbe
**  description  * Index merge routines.
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

Merge routines are used to merge Bonsai-tree to the permanent tree.
The transaction level of the merge operation is decided by the caller
of merge routines.

Limitations:
-----------

TODO
- dbe_db_t merge sem to gate
- merge gate exclusive behaviour



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

#include <sslimits.h>

#include <ssc.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssdebug.h>
#include <sssprint.h>
#include <ssthread.h>
#include <sspmon.h>

#include <uti0vtpl.h>
#include <uti0vcmp.h>

#include <su0list.h>

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe6gobj.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe6bsea.h"
#include "dbe6btre.h"
#include "dbe7trxi.h"
#include "dbe7trxb.h"
#include "dbe7cfg.h"
#include "dbe5inde.h"
#include "dbe5isea.h"
#include "dbe5imrg.h"
#include "dbe4rfwd.h"
#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe0db.h"
#include "dbe0blobg2.h"

#define CHK_INDMERGE(im)  ss_dassert(SS_CHKPTR(im) && (im)->im_chk == DBE_CHK_INDMERGE)
#define CHK_MERGEPART(mp) ss_dassert(SS_CHKPTR(mp) && (mp)->mp_chk == DBE_CHK_MERGEPART)

typedef enum {
        MERGE_RC_CONT,
        MERGE_RC_DONE,
        MERGE_RC_ERROR
} merge_ret_t;

typedef struct {
        ss_debug(dbe_chk_t      mp_chk;)
        dbe_indmerge_t*         mp_indmerge;
        dbe_btrsea_t            mp_mergesea;    /* search used during merge */
        dbe_btree_t*            mp_bonsaitree;
        dbe_btree_t*            mp_permtree;
        dbe_bkey_t*             mp_mergetmpkey; /* Temporary key buffer used
                                                   during merge. */
        dbe_btrsea_keycons_t    mp_kc;          /* Key range constraints. */
        dbe_btrsea_timecons_t   mp_tc;          /* Time range constraints. */
        dbe_dynbkey_t           mp_beginkey;    /* Search lower limit. */
        dbe_dynbkey_t           mp_endkey;      /* Search upper limit. */
        su_list_t*              mp_deferredblobunlinklist;
        long                    mp_nindexwrites;
        long                    mp_nmergewrites;
        long                    mp_mergeupdatectr;
        bool                    mp_gateentered;
        long                    mp_keyid;
        dbe_info_t              mp_bonsaiinfo;
        dbe_info_t              mp_perminfo;
        dbe_btree_lockinfo_t    mp_permlockinfo;
        bool                    mp_initp;
        ulong                   mp_maxpoolblocks;
} mergepart_t;

struct dbe_indmerge_st {
        ss_debug(dbe_chk_t im_chk;)
        dbe_index_t*            im_ind;         /* index system */
        dbe_trxnum_t            im_patchtrxnum; /* max trx num the status
                                                   of which is ensured to
                                                   be patched during merge */
        dbe_trxnum_t            im_mergetrxnum; /* merge level, for debugging */
        dbe_trxid_t             im_aborttrxid;  /* trxbuf aborttrxid when
                                                   merge started */
        dbe_opentrxinfo_t*      im_opentrxinfo;
        bool                    im_ended;
        bool                    im_full_merge_complete;
        long                    im_nindexwrites;
        long                    im_nmergewrites;
        long                    im_startupmergewrites;
        bool                    im_quickmerge;
        dbe_gobj_t*             im_go;
        su_list_t*              im_mergepartlist;
        su_list_t*              im_mergepart_donelist;
        int                     im_nmergepart;
        int                     im_nmergetask;
        int                     im_nactivemergetask;
        int                     im_nactiveusertask;
        SsSemT*                 im_sem;
};

extern bool dbe_index_test_version_on;

/*#***********************************************************************\
 * 
 *		indmerge_errorprint
 * 
 * 
 * 
 * Parameters : 
 * 
 *	merge - 
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
static void indmerge_errorprint(
        mergepart_t* mp,
        dbe_bkey_t* bkey,
        int rc,
        char* file,
        int line)
{
        char debugstring[128];

        SsSprintf(
            debugstring,
            "/LOG/UNL/TID:%u/NOD/FLU",
            SsThrGetid());
        SsDbgSet(debugstring);

        SsDbgFlush();

        SsDbgPrintf("indmerge_errorprint:\n");
        SsDbgPrintf("File %s, line %d, rc = %s (%d)\n", file, line, su_rc_nameof(rc), rc);
        
        SsDbgPrintf("Current merge key:\n");
        dbe_bkey_dprint(0, bkey);

        dbe_btrsea_errorprint(&mp->mp_mergesea);
        SsDbgFlush();

        if (!dbe_debug && !dbe_cfg_startupforcemerge) {
            SsDbgSet("/NOL");
        }
}

/*#***********************************************************************\
 * 
 *		dbe_imrg_checkwriterc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	merge - 
 *		
 *		
 *	key - 
 *		
 *		
 *	rc - 
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
static bool dbe_imrg_checkwriterc(
        mergepart_t* mp,
        dbe_bkey_t* key,
        dbe_ret_t rc,
        char* file,
        int line)
{
        switch (rc) {
            case DBE_RC_SUCC:
                return(TRUE);
            case SU_ERR_FILE_WRITE_FAILURE:
            case SU_ERR_FILE_WRITE_DISK_FULL:
            case SU_ERR_FILE_WRITE_CFG_EXCEEDED:
                dbe_fileio_error(file, line, rc); 
                return(FALSE);
            default:
                indmerge_errorprint(mp, key, rc, file, line);
                if (!dbe_debug && !dbe_cfg_startupforcemerge) {
                    dbe_fatal_error(file, line, rc);
                }
                return(TRUE);
        }
}

static mergepart_t* mergepart_init(
        dbe_indmerge_t* merge,
        dbe_db_t* db,
        rs_sysi_t* cd,
        dbe_index_t* index __attribute__ ((unused)),
        vtpl_t* range_begin,
        vtpl_t* range_end,
        dbe_trxnum_t mergetrxnum,
        dbe_trxnum_t patchtrxnum __attribute__ ((unused)),
        ulong maxpoolblocks)
{
        mergepart_t* mp;
        rs_sysi_t* local_cd;

        ss_trigger("mergepart_init");
        SS_PUSHNAME("mergepart_init");

        ss_dprintf_1(("mergepart_init(mergetrxnum = %ld, patchtrxnum = %ld)\n",
            DBE_TRXNUM_GETLONG(mergetrxnum), DBE_TRXNUM_GETLONG(patchtrxnum)));

        if (db == NULL) {
            local_cd = NULL;
        } else {
            local_cd = dbe_db_inittbconcd(db);
        }
        if (local_cd != NULL) {
            cd = local_cd;
        }

        mp = SSMEM_NEW(mergepart_t);

        ss_debug(mp->mp_chk = DBE_CHK_MERGEPART);
        mp->mp_indmerge = merge;
        mp->mp_beginkey = NULL;
        mp->mp_endkey = NULL;
        ss_pprintf_3(("Range begin:\n"));
        if (range_begin == NULL) {
            mp->mp_kc.kc_beginkey = NULL;
            ss_pprintf_3(("NULL\n"));
        } else {
            dbe_dynbkey_setleaf(
                &mp->mp_beginkey,
                DBE_TRXNUM_NULL,
                DBE_TRXID_NULL,
                range_begin);
            dbe_bkey_setdeletemark(mp->mp_beginkey);
            mp->mp_kc.kc_beginkey = mp->mp_beginkey;
            ss_poutput_3(dbe_bkey_dprint(3, mp->mp_beginkey));
        }
        ss_pprintf_3(("Range end:\n"));
        if (range_end == NULL) {
            mp->mp_kc.kc_endkey = NULL;
            ss_pprintf_3(("NULL\n"));
        } else {
            dbe_dynbkey_setleaf(
                &mp->mp_endkey,
                DBE_TRXNUM_NULL,
                DBE_TRXID_NULL,
                range_end);
            dbe_bkey_setdeletemark(mp->mp_endkey);
            mp->mp_kc.kc_endkey = mp->mp_endkey;
            ss_poutput_3(dbe_bkey_dprint(3, mp->mp_endkey));
        }
        ss_assert(mp->mp_kc.kc_beginkey == NULL
                  || mp->mp_kc.kc_endkey == NULL
                  || dbe_bkey_compare(mp->mp_kc.kc_beginkey, mp->mp_kc.kc_endkey) < 0);
        mp->mp_kc.kc_conslist = NULL;
        mp->mp_kc.kc_cd = cd;
        mp->mp_kc.kc_key = NULL;
        if (dbe_cfg_startupforcemerge) {
            mp->mp_tc.tc_mintrxnum = DBE_TRXNUM_NULL;
            mp->mp_tc.tc_maxtrxnum = DBE_TRXNUM_NULL;
        } else {
            mp->mp_tc.tc_mintrxnum = DBE_TRXNUM_MIN;
            mp->mp_tc.tc_maxtrxnum = mergetrxnum;
        }
        mp->mp_tc.tc_usertrxid = DBE_TRXID_NULL;
        mp->mp_tc.tc_maxtrxid = DBE_TRXID_MAX;
        mp->mp_tc.tc_trxbuf = dbe_index_gettrxbuf(merge->im_ind);

        mp->mp_deferredblobunlinklist = su_list_init(NULL);
        rs_sysi_setdeferredblobunlinklist(
                mp->mp_kc.kc_cd,
                mp->mp_deferredblobunlinklist);
        mp->mp_nindexwrites = 0;
        mp->mp_nmergewrites = 0;
        mp->mp_mergeupdatectr = 0;
        mp->mp_gateentered = FALSE;
        mp->mp_maxpoolblocks = maxpoolblocks;
        mp->mp_initp = FALSE;

        mp->mp_bonsaitree = dbe_index_getbonsaitree(merge->im_ind);
        mp->mp_permtree = dbe_index_getpermtree(merge->im_ind);

        dbe_info_init(mp->mp_bonsaiinfo, DBE_INFO_DISKALLOCNOFAILURE|DBE_INFO_OPENRANGEEND|DBE_INFO_MERGE);
        dbe_info_init(mp->mp_perminfo, DBE_INFO_DISKALLOCNOFAILURE|DBE_INFO_OPENRANGEEND|DBE_INFO_MERGE);
        mp->mp_perminfo.i_btreelockinfo = &mp->mp_permlockinfo;

        mp->mp_mergetmpkey = dbe_bkey_init(dbe_index_getbkeyinfo(merge->im_ind));
        dbe_bkey_removetrxinfo(mp->mp_mergetmpkey);
        dbe_bkey_setcommitted(mp->mp_mergetmpkey);

        ss_dassert(dbe_bkey_isleaf(mp->mp_mergetmpkey));
        ss_dassert(!dbe_bkey_istrxid(mp->mp_mergetmpkey));
        ss_dassert(!dbe_bkey_istrxnum(mp->mp_mergetmpkey));

        ss_trigger("mergepart_init");
        rs_sysi_setdeferredblobunlinklist(
                mp->mp_kc.kc_cd,
                NULL);

        ss_autotest_or_debug(if (local_cd != NULL) rs_sysi_clearthrid(local_cd));
        SS_POPNAME;

        return(mp);
}

static void mergepart_reset(
        mergepart_t* mp)
{
        ss_trigger("mergepart_reset");
        
        CHK_MERGEPART(mp);
        ss_dassert(!mp->mp_gateentered);
        SS_PUSHNAME("mergepart_reset");

        if (mp->mp_initp) {
            long nindexwrites;
            long nmergewrites;
            nindexwrites = mp->mp_nindexwrites +
                           dbe_btrsea_getnkeyremoved(&mp->mp_mergesea);
            nmergewrites = mp->mp_nmergewrites + 
                           dbe_btrsea_getnmergeremoved(&mp->mp_mergesea);
            dbe_btrsea_donebuf(&mp->mp_mergesea);
            dbe_gobj_mergeupdate(
                mp->mp_indmerge->im_go, 
                nindexwrites,
                nmergewrites);
            mp->mp_indmerge->im_nindexwrites += nindexwrites;
            mp->mp_indmerge->im_nmergewrites += nmergewrites;
            mp->mp_initp = FALSE;
            mp->mp_nindexwrites = 0;
            mp->mp_nmergewrites = 0;
        } else {
            ss_dassert(!mp->mp_nindexwrites);
        }

        ss_trigger("mergepart_reset");
        SS_POPNAME;
}

static void mergepart_done(
        mergepart_t* mp)
{
        ss_trigger("mergepart_done");
        
        CHK_MERGEPART(mp);
        ss_dassert(!mp->mp_gateentered);
        SS_PUSHNAME("mergepart_done");

        mergepart_reset(mp);

        dbe_bkey_done(mp->mp_mergetmpkey);
        dbe_dynbkey_free(&mp->mp_beginkey);
        dbe_dynbkey_free(&mp->mp_endkey);
        su_list_done(mp->mp_deferredblobunlinklist);
        if (mp->mp_kc.kc_cd != NULL && rs_sysi_db(mp->mp_kc.kc_cd) != NULL) {
            dbe_db_donetbconcd(rs_sysi_db(mp->mp_kc.kc_cd), mp->mp_kc.kc_cd);
        }
        SsMemFree(mp);

        ss_trigger("mergepart_done");
        SS_POPNAME;
}

static void mergepart_updateindexwrites(
        mergepart_t* mp)
{
        CHK_MERGEPART(mp);
        ss_dassert(!mp->mp_gateentered);

        if (mp->mp_initp) {
            dbe_gobj_mergeupdate(mp->mp_indmerge->im_go, mp->mp_nindexwrites, mp->mp_nmergewrites);
            mp->mp_indmerge->im_nindexwrites += mp->mp_nindexwrites;
            mp->mp_indmerge->im_nmergewrites += mp->mp_nmergewrites;
            mp->mp_nindexwrites = 0;
            mp->mp_nmergewrites = 0;
        }
}

static void mergepart_startif(
        mergepart_t* mp)
{
        uint readaheadsize;

        if (!mp->mp_initp) {

            mp->mp_initp = TRUE;

            dbe_btrsea_initbufmerge(
                &mp->mp_mergesea,
                mp->mp_bonsaitree,
                &mp->mp_kc,
                &mp->mp_tc,
                &mp->mp_bonsaiinfo);
            /* Set the merge search to long sequential search state. This affects
             * to read operations (read ahead is used) but not write operations
             * (blocks are released with normal priority) because the merge
             * search is handled specially in the dbe_btrsea module.
             */
            dbe_btrsea_setlongseqsea(&mp->mp_mergesea);

            if (dbe_cfg_mergecleanup) {
                readaheadsize = 1;
            } else {
                readaheadsize = (uint)dbe_index_getreadaheadsize(mp->mp_indmerge->im_ind);

                if (readaheadsize > (uint)mp->mp_maxpoolblocks) {
                    readaheadsize = (uint)mp->mp_maxpoolblocks;
                }
            }

            dbe_btrsea_setreadaheadsize(
                &mp->mp_mergesea,
                readaheadsize);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_indmerge_init_ex
 * 
 * Initializes the index merge process. Merge process moves key values
 * from the Bonsai-tree to the Permanent tree. The merge process is
 * bound to the index object.
 * 
 * Parameters : 
 * 
 *      cd - in, hold
 *          client data
 * 
 *	index - in out, hold
 *		index system
 *
 *	mergetrxnum - in
 *		Trx number used during merge. Only key values
 *          with trx number less than or equal to mergetrxnum
 *          are moved from the Bonsai-tree to the permanent
 *          tree. All history information is removed below
 *          mergetrxnum.
 *
 *      patchtrxnum - in
 *          Patch transaction number. During merge all transactions
 *          with serialization number less than or equal to patchtrxnum
 *          are either marked as committed or removed from the index.
 *
 *      maxpoolblocks - 
 *
 *
 *      quickmerge - 
 *
 *
 * Return value - give : 
 * 
 *      Merge object.
 * 
 * Limitations  : 
 * 
 *      Only one merge process can be active at one time for the
 *      same index.
 * 
 * Globals used : 
 */
dbe_indmerge_t* dbe_indmerge_init_ex(
        void* cd,
        dbe_db_t* db,
        dbe_index_t* index,
        dbe_trxnum_t mergetrxnum,
        dbe_trxnum_t patchtrxnum,
        ulong maxpoolblocks,
        bool quickmerge)
{
        dbe_indmerge_t* merge;
        mergepart_t* mp;
        dbe_trxbuf_t* trxbuf;
        dbe_gobj_t* go;

        ss_trigger("dbe_indmerge");
        SS_PUSHNAME("dbe_indmerge_init");

        ss_dprintf_1(("dbe_indmerge_init_ex(mergetrxnum=%ld, patchtrxnum=%ld, maxmergeparts=%d, quickmerge=%d)\n",
            DBE_TRXNUM_GETLONG(mergetrxnum), DBE_TRXNUM_GETLONG(patchtrxnum), dbe_cfg_maxmergeparts, quickmerge));

       go = dbe_index_getgobj(index);

       dbe_counter_setactivemergetrxnum(go->go_ctr, mergetrxnum);

        merge = SSMEM_NEW(dbe_indmerge_t);

        ss_debug(merge->im_chk = DBE_CHK_INDMERGE);
        merge->im_ind = index;
        merge->im_go = go;
        merge->im_mergetrxnum = mergetrxnum;
        merge->im_patchtrxnum = patchtrxnum;
        trxbuf = dbe_index_gettrxbuf(index);
        if (dbe_trxbuf_usevisiblealltrxid(trxbuf)) {
            dbe_gtrs_entertrxgate(merge->im_go->go_gtrs);
            merge->im_aborttrxid = dbe_counter_gettrxid(merge->im_go->go_ctr);
            merge->im_opentrxinfo = dbe_gtrs_getopentrxinfo_nomutex(merge->im_go->go_gtrs);
            dbe_gtrs_exittrxgate(merge->im_go->go_gtrs);
        } else {
            merge->im_aborttrxid = dbe_trxbuf_getaborttrxid(trxbuf);
            merge->im_opentrxinfo = NULL;
        }
        ss_dprintf_2(("dbe_indmerge_init_ex:aborttrxid=%ld, usevisiblealltrxid=%d\n",
            DBE_TRXID_GETLONG(merge->im_aborttrxid), dbe_trxbuf_usevisiblealltrxid(trxbuf)));
        merge->im_ended = FALSE;
        merge->im_full_merge_complete = FALSE;
        merge->im_nindexwrites = 0;
        merge->im_nmergewrites = 0;
        merge->im_startupmergewrites = merge->im_go->go_nmergewrites;
        merge->im_quickmerge = quickmerge;
        merge->im_mergepartlist = su_list_init(NULL);
        merge->im_mergepart_donelist = su_list_init(NULL);
        merge->im_sem = SsSemCreateLocal(SS_SEMNUM_DBE_INDMERGE);
        merge->im_nmergetask = 0;
        merge->im_nactivemergetask = 0;
        merge->im_nactiveusertask = 0;

        /* Mark to all searches that merge has been started.
         */
        dbe_index_indsealist_reach(index);
        dbe_index_setmergeactive(index, TRUE);
        dbe_index_indsealist_mergeactiveiter_nomutex(index, TRUE, dbe_indsea_setmergestart);
        dbe_index_indsealist_release(index);

# ifdef SS_DEBUG
        if (dbe_cfg_mergecleanup) {
            ss_dassert(dbe_cfg_maxmergeparts <= 1);
        } else {
            ss_dassert(db == NULL || dbe_cfg_maxmergeparts > 1);
        }
# endif

        if (dbe_cfg_maxmergeparts <= 1 || db == NULL) {
            /* db can be NULL in some test */

            mp = mergepart_init(
                    merge,
                    db,
                    cd,
                    index,
                    NULL,
                    NULL,
                    mergetrxnum,
                    patchtrxnum,
                    maxpoolblocks);

            su_list_insertlast(merge->im_mergepartlist, mp);

        } else {
            /* Full merge.
             */
            dynvtpl_t* sample_vtpl;
            int alloc_size;
            int sample_size;
            int i;

            ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

            alloc_size = dbe_cfg_maxmergeparts;

            sample_vtpl = SsMemCalloc(sizeof(sample_vtpl[0]), alloc_size + 2);

            dbe_db_getmergekeysamples(
                    db,
                    sample_vtpl + 1,
                    alloc_size);

            /* The returned sample_vtpl may contain holes, compress them
             * out and count the real sample_size.
             */
            for (i = 0, sample_size = 1; i < alloc_size; i++) {
                if (sample_vtpl[i + 1] != NULL) {
                    sample_vtpl[sample_size++] = sample_vtpl[i + 1];
                }
            }
            sample_vtpl[0] = NULL;
            sample_vtpl[sample_size] = NULL;

            if (sample_size < 3) {
                mp = mergepart_init(
                        merge,
                        db,
                        cd,
                        index,
                        NULL,
                        NULL,
                        mergetrxnum,
                        patchtrxnum,
                        maxpoolblocks);
                su_list_insertlast(merge->im_mergepartlist, mp);

            } else {
                vtpl_t* range_begin;
                vtpl_t* range_end;

                for (i = 0; i < sample_size; i++) {

                    range_begin = sample_vtpl[i];
                    range_end = sample_vtpl[i + 1];
                
                    if (range_begin != NULL && range_end != NULL) {
                        int cmp;
                        cmp = vtpl_compare(range_begin, range_end);
                        if (cmp >= 0) {
                            /* Skip this range. */
                            ss_dprintf_2(("dbe_indmerge_init:skip range, i=%d\n", i));
                            ss_dassert(cmp == 0);
                            continue;
                        }
                    }

                    mp = mergepart_init(
                            merge,
                            db,
                            cd,
                            index,
                            range_begin,
                            range_end,
                            mergetrxnum,
                            patchtrxnum,
                            maxpoolblocks);

                    su_list_insertlast(merge->im_mergepartlist, mp);
                }
            }
            for (i = 0; i < sample_size; i++) {
                dynvtpl_free(&sample_vtpl[i]);
            }
            SsMemFree(sample_vtpl);
#ifdef SS_DEBUG
            {
                su_list_node_t* n;
                mergepart_t* prev_mp = NULL;
                mergepart_t* mp;
                int ncmp = 0;

                su_list_do_get(merge->im_mergepartlist, n, mp) {
                    if (mp->mp_kc.kc_beginkey != NULL
                        && mp->mp_kc.kc_endkey != NULL
                        && prev_mp != NULL
                        && prev_mp->mp_kc.kc_endkey != NULL)
                    {
                        /* Ensure that we have non-overlapping key ranges.
                         */
                        ss_dassert(dbe_bkey_compare(mp->mp_kc.kc_beginkey, mp->mp_kc.kc_endkey) < 0);
                        ss_dassert(dbe_bkey_compare(prev_mp->mp_kc.kc_endkey, mp->mp_kc.kc_beginkey) <= 0);
                        ncmp++;
                    }
                    prev_mp = mp;
                }
                ss_rc_dassert(ncmp > (int)su_list_length(merge->im_mergepartlist) - 4, ncmp);
            }
#endif /* SS_DEBUG */
        }

        merge->im_nmergepart = su_list_length(merge->im_mergepartlist);
        
        ss_trigger("dbe_indmerge");

        SS_POPNAME;

        return(merge);
}

/*##**********************************************************************\
 * 
 *		dbe_indmerge_init
 * 
 * Initializes the index merge process. Merge process moves key values
 * from the Bonsai-tree to the Permanent tree. The merge process is
 * bound to the index object.
 * 
 * Parameters : 
 * 
 *      cd - in, hold
 *          client data
 * 
 *	index - in out, hold
 *		index system
 *
 *	mergetrxnum - in
 *		Trx number used during merge. Only key values
 *          with trx number less than or equal to mergetrxnum
 *          are moved from the Bonsai-tree to the permanent
 *          tree. All history information is removed below
 *          mergetrxnum.
 *
 *      patchtrxnum - in
 *          Patch transaction number. During merge all transactions
 *          with serialization number less than or equal to patchtrxnum
 *          are either marked as committed or removed from the index.
 *
 *      maxpoolblocks - 
 *
 *
 * Return value - give : 
 * 
 *      Merge object.
 * 
 * Limitations  : 
 * 
 *      Only one merge process can be active at one time for the
 *      same index.
 * 
 * Globals used : 
 */
dbe_indmerge_t* dbe_indmerge_init(
        void* cd,
        dbe_db_t* db,
        dbe_index_t* index,
        dbe_trxnum_t mergetrxnum,
        dbe_trxnum_t patchtrxnum,
        ulong maxpoolblocks)
{
        return(dbe_indmerge_init_ex(
                cd,
                db,
                index,
                mergetrxnum,
                patchtrxnum,
                maxpoolblocks,
                DBE_TRXNUM_ISNULL(mergetrxnum)));
}

/*##**********************************************************************\
 * 
 *		dbe_indmerge_done_ex
 * 
 * Stops the index merge process.
 * 
 * Parameters : 
 * 
 *	merge - in, take
 *		merge object
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_indmerge_done_ex(
        dbe_indmerge_t* merge,
        long* p_nindexwrites)
{
        su_list_node_t* n;
        mergepart_t* mp;

        ss_trigger("dbe_indmerge");
        
        CHK_INDMERGE(merge);
        ss_dassert(dbe_index_ismergeactive(merge->im_ind));
        SS_PUSHNAME("dbe_indmerge_done");
        ss_dprintf_1(("dbe_indmerge_done_ex:ended=%d, full_merge_complete=%d\n", merge->im_ended, merge->im_full_merge_complete));

        dbe_index_indsealist_reach(merge->im_ind);

        /* Mark to all searches that merge has been stopped.
         */
        dbe_index_indsealist_mergeactiveiter_nomutex(merge->im_ind, FALSE, dbe_indsea_setmergestop);

        if (merge->im_ended) {
            /* Remove obsolete transaction info from trxbuf.
             */
            ss_dprintf_2(("dbe_indmerge_done_ex:call dbe_trxbuf_clean, aborttrxid=%ld, patchtrxnum=%ld\n",
                 DBE_TRXID_GETLONG(merge->im_aborttrxid), DBE_TRXNUM_GETLONG(merge->im_patchtrxnum)));
            dbe_trxbuf_clean(
                dbe_index_gettrxbuf(merge->im_ind),
                merge->im_patchtrxnum,
                merge->im_aborttrxid,
                merge->im_opentrxinfo);

            if (merge->im_full_merge_complete) {
                ss_dprintf_2(("dbe_indmerge_done_ex:set storagetrxnum=%ld\n", DBE_TRXNUM_GETLONG(merge->im_mergetrxnum)));
                dbe_counter_setstoragetrxnum(merge->im_go->go_ctr, merge->im_mergetrxnum);
            }
        }

        if (merge->im_opentrxinfo != NULL) {
            dbe_gtrs_freeopentrxinfo(merge->im_opentrxinfo);
        }

        dbe_index_setmergeactive(merge->im_ind, FALSE);

        dbe_index_indsealist_release(merge->im_ind);

        su_list_do_get(merge->im_mergepartlist, n, mp) {
            mergepart_done(mp);
        }
        su_list_done(merge->im_mergepartlist);
        su_list_do_get(merge->im_mergepart_donelist, n, mp) {
            mergepart_done(mp);
        }
        su_list_done(merge->im_mergepart_donelist);

        if (p_nindexwrites != NULL) {
            if (dbe_btree_getheight(dbe_index_getbonsaitree(merge->im_ind)) == 1) {
                /* If Bonsai-tree is shrinked to a single leaf, assume
                 * that everything has been removed.
                 */
                *p_nindexwrites = LONG_MAX;
                dbe_gobj_mergeupdate(merge->im_go, LONG_MAX, LONG_MAX);
            } else {
                *p_nindexwrites = merge->im_nindexwrites;
                if (merge->im_ended
                    && merge->im_nmergewrites < merge->im_startupmergewrites)
                {
                    long diff;
                    ss_dassert(dbe_bkey_getmergekeycount(NULL) == 1);
                    diff = merge->im_startupmergewrites - merge->im_nmergewrites;
                    dbe_gobj_mergeupdate(merge->im_go, diff, diff);
                }
            }
        }

        dbe_counter_setactivemergetrxnum(merge->im_go->go_ctr, DBE_TRXNUM_NULL);

        SsSemFree(merge->im_sem);
        SsMemFree(merge);

        ss_trigger("dbe_indmerge");
        SS_POPNAME;
}

void dbe_indmerge_done(
        dbe_indmerge_t* merge)
{
        dbe_indmerge_done_ex(merge, NULL);
}

dbe_trxnum_t dbe_indmerge_getmergelevel(dbe_indmerge_t* merge)
{
        CHK_INDMERGE(merge);

        return(merge->im_mergetrxnum);
}

/*##**********************************************************************\
 * 
 *		dbe_indmerge_setnmergetasks
 * 
 * 
 * 
 * Parameters : 
 * 
 *	merge - in, take
 *		merge object
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_indmerge_setnmergetasks(
        dbe_indmerge_t* merge,
        int nmergetasks)
{
        ss_dprintf_1(("dbe_indmerge_setnmergetasks:nmergetasks=%d\n", nmergetasks));
        ss_dassert(nmergetasks > 0);

        merge->im_nmergetask = nmergetasks;
}

/*##**********************************************************************\
 * 
 *		dbe_indmerge_getnmerged
 * 
 * Returns the number of merged key values.
 * 
 * Parameters : 
 * 
 *	merge - in
 *		merge object
 *
 *      p_nindexwrites - out
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
void dbe_indmerge_getnmerged(
        dbe_indmerge_t* merge,
        long* p_nindexwrites)
{
        su_list_node_t* n;
        mergepart_t* mp;

        CHK_INDMERGE(merge);

        if (dbe_btree_getheight(dbe_index_getbonsaitree(merge->im_ind)) == 1) {
            /* If Bonsai-tree is shrinked to a single leaf, assume
             * that everything has been removed.
             */
            *p_nindexwrites = LONG_MAX;
            dbe_gobj_mergeupdate(merge->im_go, LONG_MAX, LONG_MAX);
        } else {
            SsSemEnter(merge->im_sem);
            su_list_do_get(merge->im_mergepartlist, n, mp) {
                mergepart_updateindexwrites(mp);
            }
            SsSemExit(merge->im_sem);
            *p_nindexwrites = merge->im_nindexwrites;
        }
}

static su_ret_t merge_delete_vtplandblob(
        mergepart_t* mp,
        dbe_btree_t* t,
        dbe_bkey_t* k,
        dbe_info_t* info)
{
        su_ret_t rc;
        /* pass the deferred BLOB unlink list to lower level routines
         * through cd
         */
        rc = dbe_btree_delete_vtplandblob(t, k, mp->mp_kc.kc_cd, info);
        return (rc);
}

static su_ret_t merge_delete_keyandblob(
        mergepart_t* mp,
        dbe_btree_t* t,
        dbe_bkey_t* k,
        dbe_info_t* info)
{
        su_ret_t rc;
        /* pass the deferred BLOB unlink list to lower level routines
         * through cd
         */
        rc = dbe_btree_delete_keyandblob(t, k, mp->mp_kc.kc_cd, info);
        return (rc);
}

static void indmerge_readpathforwrite(dbe_btree_t* b, dbe_bkey_t* k, dbe_info_t* info)
{        
        dbe_ret_t rc;
        su_list_t* nodepath;

        ss_dprintf_3(("indmerge_readpathforwrite\n"));
        
        rc = dbe_btree_readpathforwrite(b, k);
        if (rc == DBE_RC_NODERELOCATE) {
            nodepath = dbe_btree_nodepath_init(b, k, TRUE, info, DBE_NODEPATH_RELOCATE);
            rc = dbe_btree_nodepath_relocate(nodepath, b, info);
            ss_rc_dassert(rc == DBE_RC_SUCC, rc);
            dbe_btree_nodepath_done(nodepath);
            dbe_btree_unlock(b);
        }
        ss_rc_dassert(rc == DBE_RC_SUCC, rc);
}
        
/*#***********************************************************************\
 * 
 *		indmerge_advance_normal
 * 
 * 
 * 
 * Parameters : 
 * 
 *	merge - 
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
static merge_ret_t indmerge_advance_normal(
        dbe_indmerge_t* merge,
        mergepart_t* mp,
        bool mergetask)
{
        int skip;
        dbe_srk_t* srk;
        dbe_ret_t rc;
        dbe_bkey_t* curkey;
        merge_ret_t retcode;
        
        ss_trigger("dbe_indmerge");
        SS_PUSHNAME("indmerge_advance_normal");
            
        ss_dprintf_3(("indmerge_advance_normal\n"));
        CHK_INDMERGE(merge);
        CHK_MERGEPART(mp);

        ss_dassert(!merge->im_ended);
        ss_dassert(!mp->mp_gateentered);

        SS_PMON_ADD(SS_PMON_MERGESTEP);
        if (!mergetask) {
            SS_PMON_ADD(SS_PMON_MERGEUSERSTEP);
        }
        
        for (skip = 0; skip < 10; skip++) {
            rc = dbe_btrsea_getnext(&mp->mp_mergesea, &srk);
            if (rc != DBE_RC_NOTFOUND) {
                break;
            }
        }
        switch (rc) {
            case DBE_RC_END:
                /* End of this merge part. */
                ss_trigger("dbe_indmerge");
                retcode = MERGE_RC_DONE;
                goto ret_cleanup;
            case DBE_RC_NOTFOUND:
#if 1 /* Jarmo changed, Jun 1, 1996
         Maybe this is because otherwise we cannot guarantee the key state
         (delete from bonsai or permanent tree). */
                ss_berror;
#endif
                ss_trigger("dbe_indmerge");
                retcode = MERGE_RC_CONT;
                goto ret_cleanup;
            case DBE_RC_FOUND:
                break;
            default:
                /* Error! */
                dbe_db_setfatalerror(merge->im_go->go_db, rc);
                retcode = MERGE_RC_ERROR;
                goto ret_cleanup;
        }

        SS_PMON_ADD(SS_PMON_MERGEOPER);

        curkey = dbe_srk_getbkey(srk);

        ss_dprintf_3(("MERGE in indmerge_advance_normal\n"));
        ss_dprintf_3(("Merge key:\n"));
        ss_output_3(dbe_bkey_dprint(3, curkey));

        ss_dassert(dbe_bkey_iscommitted(curkey));
        ss_dassert(dbe_bkey_iscommitted(curkey));
        ss_dassert(mp->mp_beginkey == NULL || dbe_bkey_compare(mp->mp_beginkey, curkey) <= 0);
        ss_dassert(mp->mp_endkey == NULL || dbe_bkey_compare(curkey, mp->mp_endkey) < 0);

        if (dbe_bkey_isdeletemark(curkey)) {
            dbe_bkey_t* deletemark;

            deletemark = curkey;

            /* Peek the next key value. */
            rc = dbe_btrsea_peeknext(&mp->mp_mergesea, &srk);

            curkey = dbe_srk_getbkey(srk);

            if (rc == DBE_RC_NOTFOUND || rc == DBE_RC_END) {

                /* The deleted key value must be in the Permanent tree.
                 */
#if 1 /* Jarmo changed, Jun 1, 1996 */
                ss_dassert(rc != DBE_RC_NOTFOUND);
#endif
                su_rc_assert(rc == DBE_RC_NOTFOUND || rc == DBE_RC_END, rc);
                ss_dprintf_3(("merge:Delete from permanent tree (next key not found).\n"));

                indmerge_readpathforwrite(mp->mp_permtree, deletemark, &mp->mp_perminfo);

                if (!mp->mp_gateentered && !dbe_cfg_usenewbtreelocking) {
                    mp->mp_gateentered = TRUE;
                    if (dbe_index_test_version_on) {
                        mp->mp_keyid = 0;
                    } else {
                        mp->mp_keyid = dbe_bkey_getkeyid(deletemark);
                    }
                    dbe_index_mergegate_enter_exclusive(merge->im_ind, mp->mp_keyid);
                }
                ss_dassert(dbe_index_test_version_on || dbe_cfg_usenewbtreelocking || mp->mp_keyid == dbe_bkey_getkeyid(deletemark));

                rc = merge_delete_vtplandblob(
                        mp,
                        mp->mp_permtree,
                        deletemark,
                        &mp->mp_perminfo);
                su_rc_assert(dbe_imrg_checkwriterc(mp, deletemark, rc, (char *)__FILE__, __LINE__), rc);

                rc = dbe_btree_delete(
                        mp->mp_bonsaitree,
                        deletemark,
                        &mp->mp_bonsaiinfo);
                dbe_imrg_checkwriterc(mp, deletemark, rc, (char *)__FILE__, __LINE__);

                dbe_btree_lockinfo_unlock(&mp->mp_perminfo, mp->mp_permtree);

                mp->mp_nindexwrites++;
                mp->mp_nmergewrites += dbe_bkey_getmergekeycount(deletemark);
                
            } else if (rc == DBE_RC_FOUND) {

                ss_dprintf_3(("Peeked key:\n"));
                ss_output_3(dbe_bkey_dprint(3, curkey));

                if (!dbe_bkey_isdeletemark(curkey) &&
                    dbe_bkey_equal_vtpl(deletemark, curkey)) {

                    /* The next key is not a delete mark and the deletemark
                     * key deletes the next key (because they are equal).
                     */
                    ss_dprintf_3(("merge:Delete from Bonsai-tree.\n"));
                    if (!mp->mp_gateentered && !dbe_cfg_usenewbtreelocking) {
                        mp->mp_gateentered = TRUE;
                        if (dbe_index_test_version_on) {
                            mp->mp_keyid = 0;
                        } else {
                            mp->mp_keyid = dbe_bkey_getkeyid(deletemark);
                        }
                        dbe_index_mergegate_enter_exclusive(merge->im_ind, mp->mp_keyid);
                    }
                    ss_dassert(dbe_index_test_version_on || dbe_cfg_usenewbtreelocking || mp->mp_keyid == dbe_bkey_getkeyid(deletemark));
                    
                    rc = dbe_btree_delete(
                            mp->mp_bonsaitree,
                            deletemark,
                            &mp->mp_bonsaiinfo);
                    dbe_imrg_checkwriterc(mp, deletemark, rc, (char *)__FILE__, __LINE__);

                    rc = dbe_btrsea_getnext(&mp->mp_mergesea, &srk);
                    if (rc == DBE_RC_FOUND) {
                        curkey = dbe_srk_getbkey(srk);

                        rc = merge_delete_keyandblob(
                                mp,
                                mp->mp_bonsaitree,
                                curkey,
                                &mp->mp_bonsaiinfo);
                        su_rc_assert(dbe_imrg_checkwriterc(mp, curkey, rc, (char *)__FILE__, __LINE__), rc);
                    } else {
                        /* Error! */
                        su_rc_assert(rc != DBE_RC_NOTFOUND, rc);
                        dbe_db_setfatalerror(merge->im_go->go_db, rc);
                        retcode = MERGE_RC_ERROR;
                        goto ret_cleanup;
                    }

                    mp->mp_nindexwrites += 2;
                    mp->mp_nmergewrites +=
                        dbe_bkey_getmergekeycount(deletemark) + 
                        dbe_bkey_getmergekeycount(curkey);

                } else {

                    /* The next key is also a delete mark or the v-tuple
                     * of the next key value is not equal with the delete
                     * mark. The deletemark key must delete a key from
                     * the Permanent tree.
                     */
                    ss_dprintf_3(("merge:Delete from permanent tree (next key found)\n"));

                    indmerge_readpathforwrite(mp->mp_permtree, deletemark, &mp->mp_perminfo);

                    if (!mp->mp_gateentered && !dbe_cfg_usenewbtreelocking) {
                        mp->mp_gateentered = TRUE;
                        if (dbe_index_test_version_on) {
                            mp->mp_keyid = 0;
                        } else {
                            mp->mp_keyid = dbe_bkey_getkeyid(deletemark);
                        }
                        dbe_index_mergegate_enter_exclusive(merge->im_ind, mp->mp_keyid);
                    }
                    ss_dassert(dbe_index_test_version_on || dbe_cfg_usenewbtreelocking || mp->mp_keyid == dbe_bkey_getkeyid(deletemark));
                    
                    rc = merge_delete_vtplandblob(
                            mp,
                            mp->mp_permtree,
                            deletemark,
                            &mp->mp_perminfo);
                    su_rc_assert(dbe_imrg_checkwriterc(mp, deletemark, rc, (char *)__FILE__, __LINE__), rc);

                    rc = dbe_btree_delete(
                            mp->mp_bonsaitree,
                            deletemark,
                            &mp->mp_bonsaiinfo);
                    dbe_imrg_checkwriterc(mp, deletemark, rc, (char *)__FILE__, __LINE__);

                    dbe_btree_lockinfo_unlock(&mp->mp_perminfo, mp->mp_permtree);

                    mp->mp_nindexwrites++;
                    mp->mp_nmergewrites += dbe_bkey_getmergekeycount(deletemark);
                }
            } else {
                /* Error! */
                dbe_db_setfatalerror(merge->im_go->go_db, rc);
                retcode = MERGE_RC_ERROR;
                goto ret_cleanup;
            }

        } else {

            /* Key is not a delete mark, move it to the permanent tree.
             * Copy the vtpl part to the mergetmpkey that does not contain
             * any transaction fields at the key header. Thus, the
             * transaction info is removed when keys are moved to the
             * permanent tree.
             */
            dbe_bkey_setbkey(mp->mp_mergetmpkey, curkey);
        
            ss_dprintf_3(("merge:Move key to permanent tree.\n"));

            indmerge_readpathforwrite(mp->mp_permtree, mp->mp_mergetmpkey, &mp->mp_perminfo);

            if (!mp->mp_gateentered && !dbe_cfg_usenewbtreelocking) {
                mp->mp_gateentered = TRUE;
                if (dbe_index_test_version_on) {
                    mp->mp_keyid = 0;
                } else {
                    mp->mp_keyid = dbe_bkey_getkeyid(mp->mp_mergetmpkey);
                }
                dbe_index_mergegate_enter_exclusive(merge->im_ind, mp->mp_keyid);
            }
            ss_dassert(dbe_index_test_version_on || dbe_cfg_usenewbtreelocking || mp->mp_keyid == dbe_bkey_getkeyid(mp->mp_mergetmpkey));

            rc = dbe_btree_insert(
                    mp->mp_permtree,
                    mp->mp_mergetmpkey,
                    NULL,
                    NULL,
                    &mp->mp_perminfo);
            su_rc_assert(dbe_imrg_checkwriterc(mp, mp->mp_mergetmpkey, rc, (char *)__FILE__, __LINE__), rc);

            rc = dbe_btree_delete(mp->mp_bonsaitree, curkey, &mp->mp_bonsaiinfo);
            dbe_imrg_checkwriterc(mp, curkey, rc, (char *)__FILE__, __LINE__);

            dbe_btree_lockinfo_unlock(&mp->mp_perminfo, mp->mp_permtree);

            mp->mp_nindexwrites++;
            mp->mp_nmergewrites += dbe_bkey_getmergekeycount(curkey);
        }

        if (mp->mp_mergeupdatectr++ % 100 == 0) {
            dbe_gobj_mergeupdate(mp->mp_indmerge->im_go, mp->mp_nindexwrites, mp->mp_nmergewrites);
            SsSemEnter(merge->im_sem);
            mp->mp_indmerge->im_nindexwrites += mp->mp_nindexwrites;
            mp->mp_indmerge->im_nmergewrites += mp->mp_nmergewrites;
            SsSemExit(merge->im_sem);
            mp->mp_nindexwrites = 0;
            mp->mp_nmergewrites = 0;
        }

        ss_trigger("dbe_indmerge");

        retcode = MERGE_RC_CONT;
        if (SU_BFLAG_TEST(mp->mp_bonsaiinfo.i_flags, DBE_INFO_OUTOFDISKSPACE)
            || SU_BFLAG_TEST(mp->mp_perminfo.i_flags, DBE_INFO_OUTOFDISKSPACE)) 
        {
            /* Disk failure. */
            ss_debug(SsPrintf("merge:DBE_INFO_OUTOFDISKSPACE\n"));
            ss_dprintf_1(("indmerge_advance_normal:DBE_INFO_OUTOFDISKSPACE\n"));
            retcode = MERGE_RC_ERROR;
        }
 ret_cleanup:;
        SS_POPNAME;
        return (retcode);
}

/*#***********************************************************************\
 * 
 *		indmerge_advance_quick
 * 
 * 
 * 
 * Parameters : 
 * 
 *	merge - 
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
static merge_ret_t indmerge_advance_quick(
        dbe_indmerge_t* merge,
        mergepart_t* mp,
        bool mergetask)
{
        merge_ret_t retcode;
        dbe_ret_t rc;
        
        ss_dprintf_3(("indmerge_advance_quick\n"));
        CHK_INDMERGE(merge);
        CHK_MERGEPART(mp);
        ss_dassert(!merge->im_ended);

        SS_PMON_ADD(SS_PMON_MERGEQUICKSTEP);
        if (!mergetask) {
            SS_PMON_ADD(SS_PMON_MERGEUSERSTEP);
        }

        rc = dbe_btrsea_getnext_quickmerge(&mp->mp_mergesea);
        switch (rc) {
            case DBE_RC_END:
                /* End of this merge process. */
                return(MERGE_RC_DONE);
            case DBE_RC_NOTFOUND:
                return(MERGE_RC_CONT);
            case DBE_RC_FOUND:
                break;
            default:
                /* Error! */
                dbe_db_setfatalerror(merge->im_go->go_db, rc);
                return(FALSE);
        }
        if (SU_BFLAG_TEST(mp->mp_bonsaiinfo.i_flags, DBE_INFO_OUTOFDISKSPACE)
            || SU_BFLAG_TEST(mp->mp_perminfo.i_flags, DBE_INFO_OUTOFDISKSPACE)) 
        {
            /* Disk failure. */
            ss_debug(SsPrintf("merge:DBE_INFO_OUTOFDISKSPACE\n"));
            ss_dprintf_1(("indmerge_advance_normal:DBE_INFO_OUTOFDISKSPACE\n"));
            retcode = MERGE_RC_ERROR;
        } else {
            retcode = MERGE_RC_CONT;
        }
        return(retcode);
}

/*##**********************************************************************\
 * 
 *		dbe_indmerge_advance
 * 
 * Advances the merge process one atomic step.
 * 
 * Parameters : 
 * 
 *	merge - use
 *		merge object
 *
 * Return value : 
 * 
 *      dbe_mergeadvance_t 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_mergeadvance_t dbe_indmerge_advance(
        dbe_indmerge_t* merge,
        rs_sysi_t* cd __attribute__ ((unused)),
        uint nstep,
        bool mergetask,
        su_list_t** p_deferred_blob_unlink_list)
{
        merge_ret_t retcode = 0;
        mergepart_t* mp;
        dbe_mergeadvance_t mergeret;
        
        ss_pprintf_1(("dbe_indmerge_advance:mergetask=%d\n", mergetask));
        CHK_INDMERGE(merge);

        SsSemEnter(merge->im_sem);

        if (merge->im_ended) {
            SsSemExit(merge->im_sem);
            ss_pprintf_2(("dbe_indmerge_advance:merge->im_ended, return DBE_MERGEADVANCE_END\n"));
            return(DBE_MERGEADVANCE_END);
        }

        ss_dassert(dbe_index_ismergeactive(merge->im_ind));

        if (mergetask) {
            merge->im_nactivemergetask++;
        } else {
            if (merge->im_nmergetask != 0) {
                int inactive_merge_tasks;
                inactive_merge_tasks = merge->im_nmergetask - merge->im_nactivemergetask;
                ss_pprintf_2(("dbe_indmerge_advance:im_mergepartlist len=%d, inactive_merge_tasks=%d, im_nmergetask=%d, im_nactivemergetask=%d\n",
                    su_list_length(merge->im_mergepartlist), 
                    inactive_merge_tasks, 
                    merge->im_nmergetask,
                    merge->im_nactivemergetask));
                if ((int)su_list_length(merge->im_mergepartlist) <= inactive_merge_tasks) {
                    ss_pprintf_2(("dbe_indmerge_advance:su_list_length(merge->im_mergepartlist)=%d <= inactive_merge_tasks=%d, return DBE_MERGEADVANCE_PART_END\n",
                        su_list_length(merge->im_mergepartlist), inactive_merge_tasks));
                    SsSemExit(merge->im_sem);
                    return(DBE_MERGEADVANCE_PART_END);
                }
            } else {
                ss_pprintf_2(("dbe_indmerge_advance:im_mergepartlist len=%d, im_nmergetask=0 (ZERO)\n",
                    su_list_length(merge->im_mergepartlist)));
            }
            merge->im_nactiveusertask++;
        }

        mp = su_list_removefirst(merge->im_mergepartlist);
        if (mp == NULL) {
            ss_pprintf_2(("dbe_indmerge_advance:mp == NULL, return DBE_MERGEADVANCE_PART_END\n"));
            if (mergetask) {
                merge->im_nactivemergetask--;
            } else {
                merge->im_nactiveusertask--;
            }
            SsSemExit(merge->im_sem);
            return(DBE_MERGEADVANCE_PART_END);
        }

        SsSemExit(merge->im_sem);

        SS_PUSHNAME("dbe_indmerge_advance");
        ss_dassert(mp->mp_kc.kc_cd != NULL);
        ss_autotest_or_debug(rs_sysi_setthrid(mp->mp_kc.kc_cd));

        rs_sysi_setdeferredblobunlinklist(
                mp->mp_kc.kc_cd,
                mp->mp_deferredblobunlinklist);

        mergepart_startif(mp);

        ss_dassert(nstep > 0);
        for (; nstep > 0; nstep--) {
            ss_pprintf_2(("dbe_indmerge_advance:%s:mergetasks=%d, usertasks=%d\n",
                merge->im_quickmerge ? "quick" : "normal",
                merge->im_nactivemergetask, merge->im_nactiveusertask));
            if (merge->im_quickmerge) {
                retcode = indmerge_advance_quick(merge, mp, mergetask);
            } else {
                retcode = indmerge_advance_normal(merge, mp, mergetask);
            }
            SS_RTCOVERAGE_INC(SS_RTCOV_MERGE_STEP);
            if (mp->mp_gateentered) {
                dbe_index_mergegate_exit(merge->im_ind, mp->mp_keyid);
                mp->mp_gateentered = FALSE;
            }
            if (retcode != MERGE_RC_CONT) {
                /* Merge ended or error. */
                break;
            }
        }

        rs_sysi_setdeferredblobunlinklist(
                mp->mp_kc.kc_cd,
                NULL);
        SS_POPNAME;

        if (su_list_length(mp->mp_deferredblobunlinklist) != 0) {
            *p_deferred_blob_unlink_list = mp->mp_deferredblobunlinklist;
            mp->mp_deferredblobunlinklist = su_list_init(NULL);
        } else {
            *p_deferred_blob_unlink_list = NULL;
        }

        SsSemEnter(merge->im_sem);

        if (mergetask) {
            merge->im_nactivemergetask--;
        } else {
            merge->im_nactiveusertask--;
        }

        ss_dassert(merge->im_nmergepart > 0);

        if (retcode == MERGE_RC_CONT) {
            /* Continue.
             */
            ss_pprintf_2(("dbe_indmerge_advance:MERGE_RC_CONT, return DBE_MERGEADVANCE_CONT\n"));
            su_list_insertfirst(merge->im_mergepartlist, mp);
            mergeret = DBE_MERGEADVANCE_CONT;

        } else if (retcode == MERGE_RC_DONE) {
            /* Merge part ended. 
             */
            mergepart_reset(mp);
            su_list_insertlast(merge->im_mergepart_donelist, mp);
            ss_dassert(merge->im_nmergepart > 0);
            merge->im_nmergepart--;
            if (merge->im_nmergepart == 0) {
                /* All parts done. */
                merge->im_ended = TRUE;
                ss_pprintf_2(("dbe_indmerge_advance:MERGE_RC_DONE, merge->im_nmergepart == 0, return DBE_MERGEADVANCE_END\n"));
                mergeret = DBE_MERGEADVANCE_END;
                if (!merge->im_quickmerge) {
                    merge->im_full_merge_complete = TRUE;
                }
            } else if (su_list_length(merge->im_mergepartlist) == 0)  {
                /* All parts active, someone is processing them. */
                ss_pprintf_2(("dbe_indmerge_advance:MERGE_RC_DONE, su_list_length(merge->im_mergepartlist) == 0, return DBE_MERGEADVANCE_PART_END\n"));
                mergeret = DBE_MERGEADVANCE_PART_END;
            } else {
                /* There are parts that are inactive, continue normally. */
                ss_pprintf_2(("dbe_indmerge_advance:MERGE_RC_DONE, return DBE_MERGEADVANCE_CONT\n"));
                mergeret = DBE_MERGEADVANCE_CONT;
            }

        } else {
            /* Error. 
             */
            ss_dassert(retcode == MERGE_RC_ERROR);
            mergepart_reset(mp);
            su_list_insertlast(merge->im_mergepart_donelist, mp);
            merge->im_ended = TRUE;
            ss_pprintf_2(("dbe_indmerge_advance:MERGE_RC_ERROR, return DBE_MERGEADVANCE_END\n"));
            mergeret = DBE_MERGEADVANCE_END;
        }

        ss_autotest_or_debug(rs_sysi_clearthrid(mp->mp_kc.kc_cd));

        SsSemExit(merge->im_sem);

        return(mergeret);
}

void dbe_indmerge_unlinkblobs(
        rs_sysi_t* cd,
        su_list_t* deferred_blob_unlink_list)
{
        ss_dprintf_1(("dbe_indmerge_unlinkblobs\n"));

        if (deferred_blob_unlink_list != NULL) {
            ss_dassert(cd != NULL);
            /* now run the deferred BLOB unlinks when merge gate
             * has been opened again
             */
            ss_dprintf_2(("dbe_indmerge_unlinkblobs:unlink blobs\n"));
            dbe_blobg2_unlink_list_of_blobids(cd,
                                              deferred_blob_unlink_list,
                                              NULL);
            su_list_done(deferred_blob_unlink_list);
        }
}
