/*************************************************************************\
**  source       * dbe5isea.c
**  directory    * dbe
**  description  * Index search routines.
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

For search operations the search functions return a stream of v-tuples
as the search result. The search is buffered. Delete marks and the
index system division into two physical trees is handled by the search
functions.


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

#define DBE5ISEA_C

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <sssprint.h>
#include <ssthread.h>
#include <sspmon.h>

#include <uti0vtpl.h>

#include <su0list.h>
#include <su0bflag.h>
#include <su0time.h>
#include <su0prof.h>

#include <rs0sysi.h>

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe6finf.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe6bsea.h"
#include "dbe6btre.h"
#include "dbe7trxi.h"
#include "dbe7trxb.h"
#include "dbe5inde.h"
#include "dbe5isea.h"
#include "dbe0type.h"
#include "dbe0erro.h"

#define CHK_INDSEA(is) ss_dassert(SS_CHKPTR(is) && (is)->is_chk == DBE_CHK_INDSEA)

#define ISEA_SEARCHBEGINACTIVECTR_LIMIT 10

#define ISEA_MUSTINIT    SU_BFLAG_BIT(0) /* The search must be initialized. */
#define ISEA_CHGDIR      SU_BFLAG_BIT(1) /* Change search direction. */
#define ISEA_RETRY       SU_BFLAG_BIT(2) /* Retry the last key value. */
#define ISEA_RESET       SU_BFLAG_BIT(3) /* The search should be reset before
                                            the next key value is returned */
#define ISEA_BTRSEARESET SU_BFLAG_BIT(4) /* Reset B-tree searches. */
#define ISEA_VALUERESET  SU_BFLAG_BIT(5) /* Reset to a new value. */
#define ISEA_INSIDEMUTEX SU_BFLAG_BIT(6) /* Mutex already entered. */

/* Bonsai flags. */
#define ISEA_USEBONSAI   SU_BFLAG_BIT(0) /* Read only from storage tree. */
#define ISEA_ISBONSAISEA SU_BFLAG_BIT(1) /* Do we have Bonsai-tree search. */
#define ISEA_BONSAINOTSET SU_BFLAG_BIT(2) /* Debug flag to check that flags are set. */

/* Possible search states.
 */
typedef enum {
        DBE_IS_BONSAI,     /* read only from Bonsai-tree */
        DBE_IS_PERM,       /* read only from permanent tree */
        DBE_IS_COMBINE,    /* combine Bonsai and permanent trees */
        DBE_IS_BEGIN,      /* beginning of search state */
        DBE_IS_END         /* end of search state */
} indsea_state_t;

/* Structure that holds the current state of a B-tree search.
 */
typedef struct {
        dbe_btrsea_t    bst_search;     /* B-tree serch. */
        dbe_ret_t       bst_rc;         /* Return code from last
                                           dbe_btrsea_(next|prev) call. */
        dbe_srk_t*      bst_srk;        /* Search result. */
        bool            bst_keyreturned;/* TRUE, if the key is already
                                           returned. */
} btrsea_state_t;

/* Index search.
 */
struct dbe_indsea_st {
        ss_debug(dbe_chk_t is_chk;)
        char*                  is_caller;
        su_bflag_t             is_flags;        /* Special flags, normally
                                                   all bits should be zero
                                                   to avoid special case
                                                   checks. */
        su_bflag_t             is_bonsaiflags;  /* Special flags for Bonsai-tree use. */
        dbe_index_t*           is_index;        /* index system used */
        dbe_dynbkey_t          is_beginkey;     /* Search lower limit. */
        dbe_dynbkey_t          is_endkey;       /* Search upper limit. */
        dbe_btrsea_keycons_t   is_kc;           /* Key constraints. */
        dbe_btrsea_timecons_t  is_tc;           /* Time constraints. */
        indsea_state_t         is_state;        /* Search state */
        btrsea_state_t         is_bonsai;       /* Bonsai search state */
        btrsea_state_t         is_perm;         /* Permanent tree search state */
        btrsea_state_t*        is_retsea;       /* Pointer to search from
                                                   where the returned key is
                                                   taken */
        bool                   is_btrseainitcalled;
        bool                   is_deletenext;   /* Delete next found key
                                                   value, if TRUE. */
        bool                   is_checkifdeleted; /* Check if this key is
                                                   deleted, used during prev
                                                   search. */
        dbe_srk_t*             is_prevkeysrk;   /* Previous key buffer. */
        dbe_srk_t              is_prevkeysrk_buf; /* Buffer for is_prevkeysrk. */
        bool                   is_isprevkey;    /* If TRUE, the next key for
                                                   the prev call is found
                                                   from is_retsea. */
        rs_sysi_t*             is_cd;           /* Client data. */
        bool                   is_mergeactive;  /* TRUE if index merge is
                                                   active. */
        dbe_index_sealnode_t   is_sealistnode;  /* Position of this search
                                                   in index searches list. */
        dbe_index_sealnode_t   is_sealrunode;   /* Position of this search
                                                   in index searches LRU. */
        bool                   is_ended;        /* Flag: is the search ended. */
        bool                   is_forwardp;     /* Flag: was the last call to
                                                   forward direction. */
        bool                   is_validate;     /* If TRUE, this is a validate
                                                   search. */
        dbe_keyvld_t           is_keyvldtype;   /* Key validate type, used if
                                                   is_validate is TRUE. */
        bool                   is_earlyvld;     /* If TRUE, early validate
                                                   is used in validate search. */
        bool                   is_pessimistic;
        bool                   is_versionedpessimistic;
        long                   is_seaid;        /* Unique search id. */
        bool                   is_longseqsea;   /* If TRUE, this is a long
                                                   sequential search. */
        bool                   is_datasea;      /* If TRUE, this is data
                                                   search.  */
        bool                   is_mergegateentered; /* If TRUE, the search has
                                                   entered to the merge gate. */
        bool                   is_idle;         /* If TRUE, the search is
                                                   in idle state.
                                                   That is, it is reseted by
                                                   the index ssytem because
                                                   there are too many open
                                                   index searches. In idle
                                                   state the search does not
                                                   keep any cache buffers
                                                   reserved. */
        bool                   is_rowavailable;
        SsSemT*                is_activesem;    /* Semaphore used to protect
                                                   this object during the
                                                   search steps. */
        int                    is_searchbeginactivectr;
        ulong                  is_maxpoolblocks;
        long                   is_keyid;
        int                    is_maxrefkeypartno;
        dbe_bkeyinfo_t*        is_bkeyinfo;
#ifdef SS_QUICKSEARCH
        dbe_btrsea_t*          is_quicksea;
        dbe_btrsea_t           is_quickseabuf;
#endif
        ss_autotest_or_debug(dbe_bkey_t* is_lastkey;)
        ss_autotest_or_debug(dbe_bkey_t* is_lastdeletemark;)
        ss_debug(bool          is_prevcallreset;)
};

static bool indsea_error_occured = FALSE;

/*#***********************************************************************\
 *
 *		indsea_errorprint
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void indsea_errorprint(dbe_indsea_t* indsea)
{
        char debugstring[100];

        SsSemEnter(ss_lib_sem);

        if (indsea_error_occured) {
            SsSemExit(ss_lib_sem);
            return;
        }

        indsea_error_occured = TRUE;

        SsSemExit(ss_lib_sem);

        if (!indsea->is_mergegateentered) {
            dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
        }

        SsDbgFlush();

#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
        SsSprintf(debugstring, "/LEV:4/FIL:dbe5isea/LOG/UNL/NOD/TID:%u", SsThrGetid());
#else
        SsSprintf(debugstring, "/LEV:4/FIL:dbe5isea/LOG/LIM:100000000/NOD/TID:%u", SsThrGetid());
#endif
        SsDbgSet(debugstring);

        SsDbgPrintf("indsea_errorprint:\n");
        SsDbgPrintf("beginkey:\n");
        dbe_bkey_dprint(1, indsea->is_kc.kc_beginkey);
        SsDbgPrintf("endkey:\n");
        dbe_bkey_dprint(1, indsea->is_kc.kc_endkey);
#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
        SsDbgPrintf("lastkey:\n");
        dbe_bkey_dprint(1, indsea->is_lastkey);
        SsDbgPrintf("lastdeletemark:\n");
        dbe_bkey_dprint(1, indsea->is_lastdeletemark);
#endif

        SsDbgPrintf("Bonsai-tree search:\n");
        ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
        if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
            dbe_btrsea_errorprint(&indsea->is_bonsai.bst_search);
        } else {
            SsDbgPrintf("Bonsai tree not used\n");
        }
        SsDbgPrintf("Storage-tree search:\n");
        dbe_btrsea_errorprint(&indsea->is_perm.bst_search);

        dbe_indsea_print(indsea);

        SsDbgPrintf("%-6s %-4s %-4s %-4s %-3s %-3s %-3s %-3s %-3s %-3s %-3s %-3s %-3s %-3s %s\n",
            "Id",
            "Dlnx",
            "Chdl",
            "Ispk",
            "Rst",
            "Mrg",
            "Ini",
            "Fwd",
            "Vld",
            "Evl",
            "Lss",
            "Dat",
            "Pes",
            "Vep",
            "MPBl");
        SsDbgPrintf("%-6ld %-4d %-4d %-4d %-3d %-3d %-3d %-3d %-3d %-3d %-3d %-3d %-3d %-3d %ld\n",
            indsea->is_seaid,
            indsea->is_deletenext,
            indsea->is_checkifdeleted,
            indsea->is_isprevkey,
            SU_BFLAG_TEST(indsea->is_flags, ISEA_RESET) != 0,
            indsea->is_mergeactive,
            SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT) == 0,
            indsea->is_forwardp,
            indsea->is_validate,
            indsea->is_earlyvld,
            indsea->is_longseqsea,
            indsea->is_datasea,
            indsea->is_pessimistic,
            indsea->is_versionedpessimistic,
            indsea->is_maxpoolblocks);
        SsDbgPrintf("caller:%s\n", indsea->is_caller);

#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
        if (indsea->is_tc.tc_trxbuf != NULL) {
            dbe_trxbuf_print(indsea->is_tc.tc_trxbuf);
        }
#endif

        SsDbgFlush();

        if (!indsea->is_mergegateentered) {
            dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
        }
        SsDbgSet("/NOL");
}

/*#***********************************************************************\
 *
 *		indsea_check_overlap
 *
 * Checks if the bonsai tree and permanent tree search buffers overlap.
 * This is used to optimize the search in case when they do not overlap,
 * because then there is no need to combine the two searches.
 *
 * Parameters :
 *
 *	search - in, use
 *		index search
 *
 *      nextp - in
 *
 * Return value :
 *
 *      overlapping state
 *
 * Limitations  :
 *
 * Globals used :
 */
static indsea_state_t indsea_check_overlap(
        dbe_indsea_t* indsea,
        bool nextp)
{
        bool endbonsai;
        bool endperm;
        dbe_bkey_t* first_bonsai;
        dbe_bkey_t* last_bonsai;
        dbe_bkey_t* first_perm;
        dbe_bkey_t* last_perm;
        int cmp;

        CHK_INDSEA(indsea);
        ss_dprintf_3(("indsea_check_overlap:seaid=%ld\n", indsea->is_seaid));

        if (nextp) {
            ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
            endperm = !dbe_btrsea_getcurrange_next(
                            &indsea->is_perm.bst_search,
                            &first_perm,
                            &last_perm);
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
                ss_aassert(SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA));
                endbonsai = !dbe_btrsea_getcurrange_next(
                                &indsea->is_bonsai.bst_search,
                                &first_bonsai,
                                &last_bonsai);
            } else {
                endbonsai = TRUE;
            }
        } else {
            ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
            endperm = !dbe_btrsea_getcurrange_prev(
                            &indsea->is_perm.bst_search,
                            &first_perm,
                            &last_perm);
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
                ss_aassert(SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA));
                endbonsai = !dbe_btrsea_getcurrange_prev(
                                &indsea->is_bonsai.bst_search,
                                &first_bonsai,
                                &last_bonsai);
            } else {
                endbonsai = TRUE;
            }

        }

        if (endbonsai && endperm) {

            indsea->is_retsea = NULL;

            if (nextp) {
                ss_dprintf_4(("end of both search, state is DBE_IS_END\n"));
                return(DBE_IS_END);
            } else {
                ss_dprintf_4(("beginning of both search, state is DBE_IS_BEGIN\n"));
                return(DBE_IS_BEGIN);
            }

        } else if (endbonsai) {

            ss_dprintf_4(("end of bonsai, state is DBE_IS_PERM\n"));
            indsea->is_retsea = &indsea->is_perm;
            return(DBE_IS_PERM);

        } else if (endperm) {

            ss_dprintf_4(("end of perm, state is DBE_IS_BONSAI\n"));
            indsea->is_retsea = &indsea->is_bonsai;
            return(DBE_IS_BONSAI);
        }
        ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
        ss_aassert(SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI));

        ss_dprintf_4(("first bonsai:\n"));
        ss_output_4(dbe_bkey_dprint(4, first_bonsai));
        ss_dprintf_4(("last perm:\n"));
        ss_output_4(dbe_bkey_dprint(4, last_perm));

        DBE_BKEY_COMPARE(last_perm, first_bonsai, cmp);
        if (cmp < 0) {
            if (nextp) {
                ss_dprintf_4(("state is DBE_IS_PERM\n"));
                indsea->is_retsea = &indsea->is_perm;
                return(DBE_IS_PERM);
            } else {
                ss_dprintf_4(("state is DBE_IS_BONSAI\n"));
                indsea->is_retsea = &indsea->is_bonsai;
                return(DBE_IS_BONSAI);
            }
        }

        ss_dprintf_4(("first perm:\n"));
        ss_output_4(dbe_bkey_dprint(4, first_perm));
        ss_dprintf_4(("last bonsai:\n"));
        ss_output_4(dbe_bkey_dprint(4, last_bonsai));

        DBE_BKEY_COMPARE(last_bonsai, first_perm, cmp);
        if (cmp < 0) {
            if (nextp) {
                ss_dprintf_4(("state is DBE_IS_BONSAI\n"));
                indsea->is_retsea = &indsea->is_bonsai;
                return(DBE_IS_BONSAI);
            } else {
                ss_dprintf_4(("state is DBE_IS_PERM\n"));
                indsea->is_retsea = &indsea->is_perm;
                return(DBE_IS_PERM);
            }
        }

        ss_dprintf_4(("state is DBE_IS_COMBINE\n"));
        ss_debug(indsea->is_retsea = NULL);
        return(DBE_IS_COMBINE);
}

/*#***********************************************************************\
 *
 *		indsea_start_searches
 *
 * Starts searches from both trees.
 *
 * Parameters :
 *
 *	indsea - in out, use
 *		index search
 *
 *	nextp - in
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void indsea_start_searches(dbe_indsea_t* indsea, bool nextp)
{
        CHK_INDSEA(indsea);
        ss_dprintf_3(("indsea_start_searches:seaid=%ld\n", indsea->is_seaid));

        SU_BFLAG_CLEAR(indsea->is_bonsaiflags, ISEA_BONSAINOTSET);

        if (indsea->is_cd != NULL 
            && rs_sysi_testflag(indsea->is_cd, RS_SYSI_FLAG_STORAGETREEONLY)
            && !indsea->is_pessimistic) 
        {
            ss_dprintf_3(("indsea_start_searches:ONLY STORAGE TREE. RS_SYSI_FLAG_STORAGETREEONLY\n"));
            ss_dassert(!indsea->is_validate);
            ss_aassert(indsea->is_keyvldtype == DBE_KEYVLD_NONE);
            SU_BFLAG_CLEAR(indsea->is_bonsaiflags, ISEA_USEBONSAI);
            SS_PMON_ADD(SS_PMON_INDEX_SEARCH_STORAGE);
        } else {
            ss_dprintf_3(("indsea_start_searches:use both trees\n"));
            SU_BFLAG_SET(indsea->is_bonsaiflags, ISEA_USEBONSAI);
            SS_PMON_ADD(SS_PMON_INDEX_SEARCH_BOTH);
        }

        ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));

        if (SU_BFLAG_TEST(indsea->is_flags, ISEA_BTRSEARESET)) {
            /* reset permanent tree search */
            dbe_btrsea_reset(
                &indsea->is_perm.bst_search,
                &indsea->is_kc,
                &indsea->is_tc,
                indsea->is_mergeactive);
        } else {
            /* start permanent tree search */
            dbe_btrsea_initbufvalidate_ex(
                &indsea->is_perm.bst_search,
                dbe_index_getpermtree(indsea->is_index),
                &indsea->is_kc,
                &indsea->is_tc,
                FALSE,
                indsea->is_validate,
                indsea->is_keyvldtype,
                indsea->is_earlyvld,
                indsea->is_mergeactive,
                indsea->is_pessimistic);
        }

        /* Advance storage tree search. */
        if (nextp) {
            indsea->is_perm.bst_rc = dbe_btrsea_getnext(
                                            &indsea->is_perm.bst_search,
                                            &indsea->is_perm.bst_srk);
        } else {
            indsea->is_perm.bst_rc = dbe_btrsea_getprev(
                                            &indsea->is_perm.bst_search,
                                            &indsea->is_perm.bst_srk);
        }

        if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_BTRSEARESET)
                && SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA)) 
            {
                /* reset Bonsai-tree search */
                dbe_btrsea_reset(
                    &indsea->is_bonsai.bst_search,
                    &indsea->is_kc,
                    &indsea->is_tc,
                    FALSE);
            } else {
                /* start Bonsai-tree search */
                dbe_btrsea_initbufvalidate_ex(
                    &indsea->is_bonsai.bst_search,
                    dbe_index_getbonsaitree(indsea->is_index),
                    &indsea->is_kc,
                    &indsea->is_tc,
                    FALSE,
                    indsea->is_validate,
                    indsea->is_keyvldtype,
                    indsea->is_earlyvld,
                    FALSE,
                    indsea->is_pessimistic);
                ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
                SU_BFLAG_SET(indsea->is_bonsaiflags, ISEA_ISBONSAISEA);
            }
        }

        /* Advance Bonsai tree search. */
        if (nextp) {
            ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
                ss_aassert(SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA));
                indsea->is_bonsai.bst_rc = dbe_btrsea_getnext(
                                                &indsea->is_bonsai.bst_search,
                                                &indsea->is_bonsai.bst_srk);
            } else {
                indsea->is_bonsai.bst_rc = DBE_RC_END;
                indsea->is_bonsai.bst_srk = NULL;
            }
        } else {
            ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
                ss_aassert(SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA));
                indsea->is_bonsai.bst_rc = dbe_btrsea_getprev(
                                                &indsea->is_bonsai.bst_search,
                                                &indsea->is_bonsai.bst_srk);
            } else {
                indsea->is_bonsai.bst_rc = DBE_RC_END;
                indsea->is_bonsai.bst_srk = NULL;
            }
        }

        indsea->is_btrseainitcalled = TRUE;
        indsea->is_perm.bst_keyreturned = FALSE;
        indsea->is_bonsai.bst_keyreturned = FALSE;

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_BTRSEARESET)) {
            if (!indsea->is_datasea) {
                uint readaheadsize;
                readaheadsize = dbe_index_getreadaheadsize(indsea->is_index);

                if (readaheadsize > (uint)(indsea->is_maxpoolblocks / 2)) {
                    readaheadsize = (uint)indsea->is_maxpoolblocks / 2;
                }

                ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
                if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
                    dbe_btrsea_setreadaheadsize(
                        &indsea->is_bonsai.bst_search,
                        readaheadsize);
                }

                dbe_btrsea_setreadaheadsize(
                    &indsea->is_perm.bst_search,
                    readaheadsize);
            }
        }

        if (indsea->is_longseqsea) {
            dbe_btrsea_setlongseqsea(&indsea->is_perm.bst_search);
        }

        indsea->is_forwardp = nextp;
}

/*##**********************************************************************\
 *
 *		dbe_indsea_init_ex
 *
 * Initializes index tree search.
 *
 * Parameters :
 *
 *	indsea - use
 *
 *
 *	cd - in, hold
 *		client data
 *
 *	index - in, hold
 *		index system
 *
 *	tc - in, hold
 *		time range constraints
 *
 *      sr - in, use
 *		search range
 *
 *      conslist - in, hold
 *		search constraints, NULL if none
 *
 * Return value - give :
 *
 *      pointer to the index search
 *
 * Comments  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 */
dbe_indsea_t* dbe_indsea_init_ex(
        void* cd,
        dbe_index_t* index,
        rs_key_t* key,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        dbe_lock_mode_t lockmode,
        bool pessimistic,
        SsSemT* dataseasem,
        char* caller)
{
        dbe_indsea_t* indsea;

        SS_NOTUSED(lockmode);
        ss_dprintf_1(("dbe_indsea_init:trxnum=%ld, pessimistic=%d\n", DBE_TRXNUM_GETLONG(tc->tc_maxtrxnum), pessimistic));

        indsea = SSMEM_NEW(dbe_indsea_t);

        ss_debug(indsea->is_chk = DBE_CHK_INDSEA);
        indsea->is_caller = caller;
        indsea->is_flags = ISEA_MUSTINIT;
        indsea->is_bonsaiflags = ISEA_BONSAINOTSET;
        indsea->is_index = index;
        indsea->is_bkeyinfo = dbe_index_getbkeyinfo(index);

        indsea->is_tc = *tc;
        indsea->is_tc.tc_trxbuf = dbe_index_gettrxbuf(index);

        indsea->is_beginkey = NULL;
        indsea->is_endkey = NULL;

        if (sr->sr_minvtpl == NULL) {
            indsea->is_kc.kc_beginkey = NULL;
        } else {
            dbe_dynbkey_setleaf(
                &indsea->is_beginkey,
                DBE_TRXNUM_NULL,
                DBE_TRXID_NULL,
                sr->sr_minvtpl);
            dbe_bkey_setdeletemark(indsea->is_beginkey);
            indsea->is_kc.kc_beginkey = indsea->is_beginkey;
        }
        if (sr->sr_maxvtpl == NULL) {
            indsea->is_kc.kc_endkey = NULL;
        } else {
            dbe_dynbkey_setleaf(
                &indsea->is_endkey,
                DBE_TRXNUM_NULL,
                DBE_TRXID_MAX,
                sr->sr_maxvtpl);
            indsea->is_kc.kc_endkey = indsea->is_endkey;
        }
        indsea->is_kc.kc_conslist = conslist;
        indsea->is_kc.kc_cd = cd;
        indsea->is_kc.kc_key = key;

        indsea->is_btrseainitcalled = FALSE;
        indsea->is_deletenext = FALSE;
        indsea->is_rowavailable = FALSE;
        indsea->is_checkifdeleted = FALSE;
        indsea->is_prevkeysrk = NULL;
        indsea->is_isprevkey = FALSE;
        indsea->is_cd = cd;
        indsea->is_retsea = NULL;
        indsea->is_forwardp = TRUE;
        indsea->is_validate = FALSE;
        indsea->is_keyvldtype = DBE_KEYVLD_NONE;
        indsea->is_earlyvld = dbe_index_isearlyvld(index);
        indsea->is_pessimistic = pessimistic;
        indsea->is_longseqsea = FALSE;
        indsea->is_datasea = dataseasem != NULL;
        indsea->is_mergegateentered = FALSE;
        indsea->is_idle = FALSE;
        indsea->is_seaid = (long)indsea; /* Avoid mutex at dbe_index_getnewseaid(index) */
        if (dataseasem != NULL) {
            indsea->is_activesem = dataseasem;
        } else {
            indsea->is_activesem = rs_sysi_givesearchsem(cd);
        }
        indsea->is_maxpoolblocks = (ulong)8;
        indsea->is_ended = FALSE;
        if (dbe_index_test_version_on) {
            indsea->is_keyid = 0;
            indsea->is_maxrefkeypartno = -1;
        } else {
            indsea->is_keyid = rs_key_id(cd, key);
            indsea->is_maxrefkeypartno = rs_key_maxrefkeypartno(cd, key);
        }
#ifdef SS_QUICKSEARCH
        indsea->is_quicksea = NULL;
#endif
        ss_autotest_or_debug(indsea->is_lastkey = dbe_bkey_init(dbe_index_getbkeyinfo(index));)
        ss_autotest_or_debug(dbe_bkey_setsearchminvtpl(indsea->is_lastkey));
        ss_autotest_or_debug(indsea->is_lastdeletemark = dbe_bkey_init(dbe_index_getbkeyinfo(index));)
        ss_debug(indsea->is_prevcallreset = FALSE);
        if (!indsea->is_datasea) {
            dbe_index_searchadd(
                index,
                indsea,
                &indsea->is_sealistnode,
                &indsea->is_sealrunode,
                NULL);
        }
        indsea->is_searchbeginactivectr = 0;

        indsea->is_mergeactive = dbe_index_ismergeactive(index);

        ss_dprintf_2(("dbe_indsea_init:seaid=%ld\n", indsea->is_seaid));

        return(indsea);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_done
 *
 * Releases resources from index search.
 *
 * Parameters :
 *
 *	indsea - in, take
 *		index search
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_indsea_done(
        dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_done:seaid=%ld\n", indsea->is_seaid));

        if (!indsea->is_ended) {
            if (!indsea->is_datasea) {
                dbe_index_searchremove(
                    indsea->is_index,
                    &indsea->is_sealistnode,
                    &indsea->is_sealrunode,
                    &indsea->is_idle);
            }
        }

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)
        ||  SU_BFLAG_TEST(indsea->is_flags, ISEA_BTRSEARESET))
        {
            ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA)) {
                dbe_btrsea_donebuf(&indsea->is_bonsai.bst_search);
            }
            dbe_btrsea_donebuf(&indsea->is_perm.bst_search);
        }
#ifdef SS_QUICKSEARCH
        if (indsea->is_quicksea != NULL) {
            dbe_btrsea_donebuf(indsea->is_quicksea);
        }
#endif /* SS_QUICKSEARCH */

        dbe_dynbkey_free(&indsea->is_beginkey);
        dbe_dynbkey_free(&indsea->is_endkey);
        if (indsea->is_prevkeysrk != NULL) {
            dbe_srk_donebuf(&indsea->is_prevkeysrk_buf, indsea->is_cd);
        }
        if (!indsea->is_datasea) {
            rs_sysi_insertsearchsem(indsea->is_cd, indsea->is_activesem);
        }
        ss_autotest_or_debug(dbe_bkey_done(indsea->is_lastkey);)
        ss_autotest_or_debug(dbe_bkey_done(indsea->is_lastdeletemark);)

        SsMemFree(indsea);
}

/*#***********************************************************************\
 *
 *		indsea_freebnode
 *
 *
 *
 * Parameters :
 *
 *	indsea -
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
static void indsea_freebnode(
        dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_3(("indsea_freebnode:seaid=%ld\n", indsea->is_seaid));

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {

            ss_dprintf_1(("indsea_freebnode:seaid=%ld, set ISEA_RESET\n", indsea->is_seaid));
            SU_BFLAG_SET(indsea->is_flags, ISEA_RESET);
            indsea->is_isprevkey = FALSE;

            ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA)) {
                dbe_btrsea_freebnode(&indsea->is_bonsai.bst_search);
            }
            dbe_btrsea_freebnode(&indsea->is_perm.bst_search);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_indsea_reset_fetch
 * 
 * 
 * 
 * Parameters : 
 * 
 *		indsea - 
 *			
 *			
 *		tc - 
 *			
 *			
 *		sr - 
 *			
 *			
 *		conslist - 
 *			
 *			
 *		stmttrxid - 
 *			
 *			
 *		p_srk - 
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
dbe_ret_t dbe_indsea_reset_fetch(
        dbe_indsea_t* indsea,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        dbe_trxid_t stmttrxid,
        dbe_srk_t** p_srk)
{
        dbe_ret_t rc;

        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_reset_fetch:seaid=%ld, trxnum=%ld\n", indsea->is_seaid, tc->tc_maxtrxnum));
        ss_dassert(sr != NULL);
        ss_dassert(!indsea->is_datasea);

        ss_dprintf_1(("dbe_indsea_reset:full reset, seabufused=%ld\n",
            dbe_index_getseabufused(indsea->is_index)));

        if (indsea->is_ended) {
            indsea->is_idle = FALSE;
            dbe_index_searchadd(
                indsea->is_index,
                indsea,
                &indsea->is_sealistnode,
                &indsea->is_sealrunode,
                &indsea->is_idle);
        } else {
            if (indsea->is_idle || indsea->is_searchbeginactivectr++ == ISEA_SEARCHBEGINACTIVECTR_LIMIT) {
                dbe_index_searchbeginactive(
                    indsea->is_index,
                    &indsea->is_sealrunode,
                    &indsea->is_idle);
                indsea->is_searchbeginactivectr = 0;
            }
        }

        SsSemEnter(indsea->is_activesem);

        if (SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
            /* B-tree searches not started yet. */
            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_BTRSEARESET)) {
                /* No activity since previous reset. */
                indsea->is_flags = ISEA_MUSTINIT|ISEA_BTRSEARESET;
            } else {
                indsea->is_flags = ISEA_MUSTINIT;
            }
        } else {
            /* B-tree searches started, they must be reseted. */
            indsea->is_flags = ISEA_MUSTINIT|ISEA_BTRSEARESET;
        }

        indsea->is_tc = *tc;
        indsea->is_tc.tc_trxbuf = dbe_index_gettrxbuf(indsea->is_index);

        if (sr->sr_minvtpl == NULL) {
            dbe_dynbkey_free(&indsea->is_kc.kc_beginkey);
        } else {
            dbe_dynbkey_setleaf(
                &indsea->is_beginkey,
                DBE_TRXNUM_NULL,
                DBE_TRXID_NULL,
                sr->sr_minvtpl);
            dbe_bkey_setdeletemark(indsea->is_beginkey);
            indsea->is_kc.kc_beginkey = indsea->is_beginkey;
        }
        if (sr->sr_maxvtpl == NULL) {
            dbe_dynbkey_free(&indsea->is_kc.kc_endkey);
        } else {
            dbe_dynbkey_setleaf(
                &indsea->is_endkey,
                DBE_TRXNUM_NULL,
                DBE_TRXID_MAX,
                sr->sr_maxvtpl);
            indsea->is_kc.kc_endkey = indsea->is_endkey;
        }

        indsea->is_kc.kc_conslist = conslist;

        if (indsea->is_btrseainitcalled) {
            dbe_btrsea_resetkeycons(&indsea->is_perm.bst_search, &indsea->is_kc);
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI|ISEA_ISBONSAISEA)) {
                dbe_btrsea_resetkeycons(&indsea->is_bonsai.bst_search, &indsea->is_kc);
            }
        }

        /* indsea->is_deletenext = FALSE; Common case below */
        indsea->is_checkifdeleted = FALSE;
        if (indsea->is_prevkeysrk != NULL) {
            dbe_srk_donebuf(&indsea->is_prevkeysrk_buf, indsea->is_cd);
            indsea->is_prevkeysrk = NULL;
        }
        /* indsea->is_isprevkey = FALSE; Common case below */
        indsea->is_retsea = NULL;
        indsea->is_forwardp = TRUE;
        /* indsea->is_longseqsea = FALSE; Common case below */
        indsea->is_ended = FALSE;

        ss_autotest_or_debug(dbe_bkey_done(indsea->is_lastkey);)
        ss_autotest_or_debug(indsea->is_lastkey = dbe_bkey_init(dbe_index_getbkeyinfo(indsea->is_index));)
        ss_autotest_or_debug(dbe_bkey_setsearchminvtpl(indsea->is_lastkey));

        indsea->is_mergeactive = dbe_index_ismergeactive(indsea->is_index);

        indsea->is_longseqsea = FALSE;
        indsea->is_isprevkey = FALSE;
        indsea->is_deletenext = FALSE;
        indsea->is_rowavailable = FALSE;
        ss_debug(indsea->is_prevcallreset = FALSE);
#ifndef SS_NOLOCKING
        SU_BFLAG_CLEAR(indsea->is_flags, ISEA_RETRY);
#endif /* SS_NOLOCKING */

        SU_BFLAG_SET(indsea->is_flags, ISEA_INSIDEMUTEX);

        rc = dbe_indsea_next(indsea, stmttrxid, p_srk);

        SU_BFLAG_CLEAR(indsea->is_flags, ISEA_INSIDEMUTEX);

        SsSemExit(indsea->is_activesem);

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_reset_ex
 *
 * Resets the search. After reset the search is in the same state as after
 * init.
 *
 * If search range (sr) is NULL then only restarts the index search with new
 * time constraints. Typically used when cursor is not closed in transaction
 * commit and new transaction starts.
 *
 * Note that btree search needs no special restart because it has a pointer
 * to the index search time contraint structure. In reset case (sr != NULL)
 * also B-trees are reset.
 *
 * Parameters :
 *
 *	indsea - in, use
 *		Index search object.
 *
 *	tc - in
 *		New time constraints.
 *
 *	sr - in
 *		New search ranbge. If pointer is NULL search is only
 *		restarted with new time constraints in tc.
 *
 *	conslist - in
 *		New constraintrs. Used only when sr != NULL.
 *
 * Return value :
 *
 * Comments  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 *
 * See also :
 */
void dbe_indsea_reset_ex(
        dbe_indsea_t* indsea,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        bool valuereset)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_reset:seaid=%ld, trxnum=%ld\n", indsea->is_seaid, DBE_TRXNUM_GETLONG(tc->tc_maxtrxnum)));

        if (sr != NULL) {

            ss_dprintf_1(("dbe_indsea_reset:full reset, seabufused=%ld\n",
                dbe_index_getseabufused(indsea->is_index)));

            if (!indsea->is_datasea) {
                if (indsea->is_ended) {
                    indsea->is_idle = FALSE;
                    dbe_index_searchadd(
                        indsea->is_index,
                        indsea,
                        &indsea->is_sealistnode,
                        &indsea->is_sealrunode,
                        &indsea->is_idle);
                } else {
                    if (indsea->is_idle || indsea->is_searchbeginactivectr++ == ISEA_SEARCHBEGINACTIVECTR_LIMIT) {
                        dbe_index_searchbeginactive(
                            indsea->is_index,
                            &indsea->is_sealrunode,
                            &indsea->is_idle);
                        indsea->is_searchbeginactivectr = 0;
                    }
                }
            }

            SsSemEnter(indsea->is_activesem);

            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
                /* B-tree searches not started yet. */
                if (SU_BFLAG_TEST(indsea->is_flags, ISEA_BTRSEARESET)) {
                    /* No activity since previous reset. */
                    indsea->is_flags = ISEA_MUSTINIT|ISEA_BTRSEARESET;
                } else {
                    indsea->is_flags = ISEA_MUSTINIT;
                }
            } else {
                /* B-tree searches started, they must be reseted. */
                indsea->is_flags = ISEA_MUSTINIT|ISEA_BTRSEARESET;
            }

            indsea->is_tc = *tc;
            indsea->is_tc.tc_trxbuf = dbe_index_gettrxbuf(indsea->is_index);

            if (sr->sr_minvtpl == NULL) {
                dbe_dynbkey_free(&indsea->is_kc.kc_beginkey);
            } else {
                dbe_dynbkey_setleaf(
                    &indsea->is_beginkey,
                    DBE_TRXNUM_NULL,
                    DBE_TRXID_NULL,
                    sr->sr_minvtpl);
                dbe_bkey_setdeletemark(indsea->is_beginkey);
                indsea->is_kc.kc_beginkey = indsea->is_beginkey;
            }
            if (sr->sr_maxvtpl == NULL) {
                dbe_dynbkey_free(&indsea->is_kc.kc_endkey);
            } else {
                dbe_dynbkey_setleaf(
                    &indsea->is_endkey,
                    DBE_TRXNUM_NULL,
                    DBE_TRXID_MAX,
                    sr->sr_maxvtpl);
                indsea->is_kc.kc_endkey = indsea->is_endkey;
            }

            indsea->is_kc.kc_conslist = conslist;

            if (indsea->is_btrseainitcalled) {
                dbe_btrsea_resetkeycons(&indsea->is_perm.bst_search, &indsea->is_kc);
                if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI|ISEA_ISBONSAISEA)) {
                    dbe_btrsea_resetkeycons(&indsea->is_bonsai.bst_search, &indsea->is_kc);
                }
            }

            /* indsea->is_deletenext = FALSE; Common case below */
            indsea->is_checkifdeleted = FALSE;
            if (indsea->is_prevkeysrk != NULL) {
                dbe_srk_donebuf(&indsea->is_prevkeysrk_buf, indsea->is_cd);
                indsea->is_prevkeysrk = NULL;
            }
            /* indsea->is_isprevkey = FALSE; Common case below */
            indsea->is_retsea = NULL;
            indsea->is_forwardp = TRUE;
            /* indsea->is_longseqsea = FALSE; Common case below */
            indsea->is_ended = FALSE;

            ss_autotest_or_debug(dbe_bkey_done(indsea->is_lastkey);)
            ss_autotest_or_debug(indsea->is_lastkey = dbe_bkey_init(dbe_index_getbkeyinfo(indsea->is_index));)
            ss_autotest_or_debug(dbe_bkey_setsearchminvtpl(indsea->is_lastkey));

            indsea->is_mergeactive = dbe_index_ismergeactive(indsea->is_index);

        } else {
            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
                return;
            }

            ss_aassert(!indsea->is_ended);

            if (indsea->is_idle || indsea->is_searchbeginactivectr++ == ISEA_SEARCHBEGINACTIVECTR_LIMIT) {
                if (!indsea->is_datasea) {
                    dbe_index_searchbeginactive(
                        indsea->is_index,
                        &indsea->is_sealrunode,
                        &indsea->is_idle);
                }
                indsea->is_searchbeginactivectr = 0;
            }

            SsSemEnter(indsea->is_activesem);

            indsea->is_tc = *tc;
            indsea->is_tc.tc_trxbuf = dbe_index_gettrxbuf(indsea->is_index);

            if (indsea->is_kc.kc_endkey != NULL) {
                dbe_bkey_settrxid(indsea->is_kc.kc_endkey, DBE_TRXID_MAX);
            }
            if (valuereset) {
                SU_BFLAG_SET(indsea->is_flags, ISEA_VALUERESET);
                ss_aassert(indsea->is_maxrefkeypartno != -1);
            }
        }

        indsea->is_longseqsea = FALSE;
        indsea->is_isprevkey = FALSE;
        indsea->is_deletenext = FALSE;
        if (valuereset) {
            indsea->is_rowavailable = FALSE;
        }
        ss_debug(indsea->is_prevcallreset = FALSE);
#ifndef SS_NOLOCKING
        SU_BFLAG_CLEAR(indsea->is_flags, ISEA_RETRY);
#endif /* SS_NOLOCKING */

        if (sr == NULL) {
            indsea_freebnode(indsea);
        }

        SsSemExit(indsea->is_activesem);
}

void dbe_indsea_reset(
        dbe_indsea_t* indsea,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist)
{
        dbe_indsea_reset_ex(
                indsea,
                tc,
                sr,
                conslist,
                TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setvalidate
 *
 * Marks the index search as a transaction validation search.
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 *	earlyvld -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_indsea_setvalidate(
        dbe_indsea_t* indsea,
        dbe_keyvld_t keyvldtype,
        bool earlyvld)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setvalidate:seaid=%ld\n", indsea->is_seaid));
        ss_aassert(SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT));

        indsea->is_validate = TRUE;
        indsea->is_keyvldtype = keyvldtype;
        indsea->is_earlyvld = earlyvld;
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setversionedpessimistic
 *
 * Marks the index search as a versioned pessimistic search.
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 *	earlyvld -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_indsea_setversionedpessimistic(dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setversionedpessimistic:seaid=%ld\n", indsea->is_seaid));
        ss_aassert(SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT));

        indsea->is_versionedpessimistic = TRUE;
}

/*##**********************************************************************\
 *
 *		dbe_indsea_getseaid
 *
 *
 *
 * Parameters :
 *
 *	indsea -
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
long dbe_indsea_getseaid(
        dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_getseaid:seaid=%ld\n", indsea->is_seaid));

        return(indsea->is_seaid);
}

/*#***********************************************************************\
 *
 *		indsea_bkey_compare
 *
 *
 *
 * Parameters :
 *
 *	bonsaisrk - in
 *
 *
 *	permsrk - in
 *
 *
 *	p_retkeyfrombonsai - out
 *
 *
 *	nextp - in
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_RC_END
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t indsea_bkey_compare(
        dbe_srk_t* bonsaisrk,
        dbe_srk_t* permsrk,
        bool* p_retkeyfrombonsai,
        bool nextp)
{
        int cmp;
        dbe_bkey_t* bonsaikey;
        dbe_bkey_t* permkey;

        ss_dprintf_3(("indsea_bkey_compare\n"));

        if (bonsaisrk == NULL) {
            bonsaikey = NULL;
        } else {
            bonsaikey = dbe_srk_getbkey(bonsaisrk);
        }
        if (permsrk == NULL) {
            permkey = NULL;
        } else {
            permkey = dbe_srk_getbkey(permsrk);
        }

        if (bonsaikey == NULL || permkey == NULL) {

            if (bonsaikey == NULL && permkey == NULL) {
                return(DBE_RC_END);
            } else if (bonsaikey == NULL) {
                /* Take the key from the Permanent tree. */
                *p_retkeyfrombonsai = FALSE;
            } else {
                /* Take the key from the Bonsai-tree. */
                *p_retkeyfrombonsai = TRUE;
            }

        } else {

            DBE_BKEY_COMPARE(bonsaikey, permkey, cmp);
            if (!nextp) {
                cmp = -cmp;
            }
            if (cmp < 0) {
                /* Take the key from the Bonsai-tree. */
                *p_retkeyfrombonsai = TRUE;
            } else if (cmp > 0) {
                /* Take the key from the Permanent tree. */
                *p_retkeyfrombonsai = FALSE;
            } else {
                ss_error;
            }
        }
        return(DBE_RC_SUCC);
}

/*#***********************************************************************\
 *
 *		indsea_bkey_incvtpl
 *
 * Increments a v-tuple in a key value k.
 *
 * Parameters :
 *
 *	k - in out, use
 *		key buffer containing the key which is incremeneted
 *
 * Return value :
 *
 * Comments     :
 *
 * Comment  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 */
static void indsea_bkey_incvtpl(dbe_bkey_t* k, int maxrefkeypartno)
{
        dynvtpl_t dvtpl = NULL;

        ss_dprintf_3(("indsea_bkey_incvtpl\n"));

        if (maxrefkeypartno != -1) {
            /* Generate a v-tuple past the current logical
               value. */
            dynvtpl_setvtplwithincrement_lastvano(&dvtpl, dbe_bkey_getvtpl(k), maxrefkeypartno);
        } else {
            /* Generate a v-tuple with a smallest possible
               increment. */
            dynvtpl_setvtplwithincrement(&dvtpl, dbe_bkey_getvtpl(k));
        }
        dbe_bkey_setvtpl(k, dvtpl);
        dynvtpl_free(&dvtpl);
}

/*#***********************************************************************\
 *
 *		indsea_reset
 *
 * Reset searches from both trees.
 *
 * During merge the search is reset each time the buffer is ended.
 * Also after merge the search may be reset if the search may have
 * in the buffer key values that are not marked as committed but the
 * merge has marked as committed. The search must be then reset
 * because the transactions are removed from transaction buffer and
 * the status is not longer known.
 *
 * WARNING: When we get here, the merge process cannot
 *          advance until this function has ended. Also
 *          this branch cannot be executed when the
 *          merge process is in 'unsafe' state. We must
 *          wait until merge is in safe state, stop the
 *          merge and then continue.
 *
 * Parameters :
 *
 *	indsea - in out, use
 *		Index search.
 *
 *	nextp - in
 *
 *	last_ret_key - in, use
 *
 *	absolute_reset_key - in
 *
 * Return value :
 *
 * Comments  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 */
static void indsea_reset(
        dbe_indsea_t* indsea,
        bool nextp,
        dbe_bkey_t* last_ret_key,
        bool absolute_reset_key)
{
        dbe_bkey_t* reset_key;

        CHK_INDSEA(indsea);
        ss_dprintf_1(("indsea_reset:seaid=%ld, nextp=%d, absrk=%d\n",
            indsea->is_seaid, nextp, absolute_reset_key));

        SU_BFLAG_CLEAR(indsea->is_flags, ISEA_RESET);

        if (last_ret_key == NULL) {
            ss_dprintf_4(("indsea_reset:last_ret_key == NULL, indsea->is_rowavailable=%d\n", indsea->is_rowavailable));
            if (indsea->is_retsea == NULL ||
                indsea->is_retsea->bst_srk == NULL) {
                su_rc_dassert(indsea->is_state == DBE_IS_END, indsea->is_state);
                return;
            }
            last_ret_key = dbe_srk_getbkey(indsea->is_retsea->bst_srk);
            absolute_reset_key = indsea->is_rowavailable;
        }

        ss_dprintf_4(("generate reseting key\n"));
        reset_key = dbe_bkey_init_ex(
                        indsea->is_cd, 
                        dbe_index_getbkeyinfo(indsea->is_index));

        if (absolute_reset_key) {

            ss_dprintf_4(("indsea_reset:absolute_reset_key\n"));
            dbe_bkey_copy(reset_key, last_ret_key);

        } else if (nextp) {

            dbe_bkey_setbkey(reset_key, last_ret_key);

            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_VALUERESET) 
                && indsea->is_maxrefkeypartno != -1) 
            {
                /* Start from the next key value.
                 */
                ss_dprintf_4(("indsea_reset:nextp, valuereset, start from the next key value\n"));
                indsea_bkey_incvtpl(reset_key, indsea->is_maxrefkeypartno);
                dbe_bkey_settrxid(reset_key, DBE_TRXID_NULL);
                dbe_bkey_setdeletemark(reset_key);
                indsea->is_deletenext = FALSE;
            } else if (indsea->is_deletenext) {
                /* Start from the next v-tuple.
                 */
                ss_dprintf_4(("indsea_reset:nextp, indsea->is_deletenext, start from the next v-tuple\n"));
                indsea_bkey_incvtpl(reset_key, -1);
                dbe_bkey_settrxid(reset_key, DBE_TRXID_NULL);
                dbe_bkey_setdeletemark(reset_key);
                indsea->is_deletenext = FALSE;
            } else {
                /* Create key value that is immediate successor of reset_key.
                 */
                ss_dprintf_4(("indsea_reset:nextp, else, create key value that is immediate successor of reset_key\n"));
                dbe_bkey_settrxid(reset_key, DBE_TRXID_SUM(dbe_bkey_gettrxid(reset_key), 1));
            }

        } else {

            /* Create key value that is immediate predecessor of reset_key.
             */
            dbe_bkey_setbkey(reset_key, last_ret_key);
            if (dbe_bkey_isdeletemark(reset_key)) {
                dbe_bkey_settrxid(reset_key, DBE_TRXID_SUM(dbe_bkey_gettrxid(reset_key), -1));
            } else {
                dbe_bkey_setdeletemark(reset_key);
                dbe_bkey_settrxid(reset_key, DBE_TRXID_MAX);
            }
            indsea->is_isprevkey = FALSE;
        }

        ss_dprintf_4(("reseting key:\n"));
        ss_output_4(dbe_bkey_dprint(4, reset_key));
        ss_autotest_or_debug(dbe_bkey_copy(indsea->is_lastkey, reset_key));

        SU_BFLAG_CLEAR(indsea->is_flags, ISEA_VALUERESET);

        if (!indsea->is_mergegateentered) {
            dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
        }

        indsea->is_rowavailable = FALSE;

        /* Reset both searches.
         */
        dbe_btrsea_setresetkey(&indsea->is_perm.bst_search, reset_key, indsea->is_mergeactive);

        /* Advance storage tree search. */
        if (nextp) {
            indsea->is_perm.bst_rc = dbe_btrsea_getnext(
                                            &indsea->is_perm.bst_search,
                                            &indsea->is_perm.bst_srk);
        } else {
            indsea->is_perm.bst_rc = dbe_btrsea_getprev(
                                            &indsea->is_perm.bst_search,
                                            &indsea->is_perm.bst_srk);
        }

        ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
        if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA)) {
            dbe_btrsea_setresetkey(&indsea->is_bonsai.bst_search, reset_key, FALSE);
        }

        /* Advance Bonsai tree search. */
        if (nextp) {
            ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
                indsea->is_bonsai.bst_rc = dbe_btrsea_getnext(
                                                &indsea->is_bonsai.bst_search,
                                                &indsea->is_bonsai.bst_srk);
            } else {
                indsea->is_bonsai.bst_rc = DBE_RC_END;
                indsea->is_bonsai.bst_srk = NULL;
            }
        } else {
            ss_aassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
            if (SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)) {
                indsea->is_bonsai.bst_rc = dbe_btrsea_getprev(
                                                &indsea->is_bonsai.bst_search,
                                                &indsea->is_bonsai.bst_srk);
            } else {
                indsea->is_bonsai.bst_rc = DBE_RC_END;
                indsea->is_bonsai.bst_srk = NULL;
            }
        }

        indsea->is_bonsai.bst_keyreturned = FALSE;
        indsea->is_perm.bst_keyreturned = FALSE;

        if (!indsea->is_mergegateentered) {
            /* Gate entered in this function, exit also in this function. */
            dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
        }

        indsea->is_state = indsea_check_overlap(indsea, nextp);

        ss_output_4(dbe_indsea_print(indsea));

        if (nextp) {
            ss_aassert(indsea->is_bonsai.bst_srk == NULL ||
                dbe_bkey_compare(dbe_srk_getbkey(indsea->is_bonsai.bst_srk), reset_key) >= 0);
            ss_aassert(indsea->is_perm.bst_srk == NULL ||
                dbe_bkey_compare(dbe_srk_getbkey(indsea->is_perm.bst_srk), reset_key) >= 0);
        } else {
            ss_aassert(indsea->is_bonsai.bst_srk == NULL ||
               dbe_bkey_compare(dbe_srk_getbkey(indsea->is_bonsai.bst_srk), reset_key) <= 0);
            ss_aassert(indsea->is_perm.bst_srk == NULL ||
               dbe_bkey_compare(dbe_srk_getbkey(indsea->is_perm.bst_srk), reset_key) <= 0);
        }

        ss_debug(indsea->is_prevcallreset = FALSE);

        dbe_bkey_done_ex(indsea->is_cd, reset_key);
}

/*#***********************************************************************\
 *
 *		indsea_chgdir_prevtonext
 *
 *
 *
 * Parameters :
 *
 *	indsea -
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
static dbe_ret_t indsea_chgdir_prevtonext(dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_3(("indsea_chgdir_prevtonext:seaid=%ld\n", indsea->is_seaid));
        ss_aassert(!indsea->is_forwardp);

        indsea->is_forwardp = TRUE;
        indsea->is_isprevkey = FALSE;
        indsea->is_checkifdeleted = FALSE;
        indsea->is_deletenext = FALSE;
        indsea->is_rowavailable = FALSE;
        ss_debug(indsea->is_prevcallreset = FALSE);
        ss_autotest_or_debug(dbe_bkey_setsearchminvtpl(indsea->is_lastkey));

        switch (indsea->is_state) {
            case DBE_IS_END:
                return(DBE_RC_END);
            case DBE_IS_BEGIN:
                if (indsea->is_prevkeysrk == NULL) {
                    indsea_reset(
                        indsea,
                        TRUE,
                        indsea->is_kc.kc_beginkey,
                        TRUE);
                } else {
                    indsea_reset(
                        indsea,
                        TRUE,
                        dbe_srk_getbkey(indsea->is_prevkeysrk),
                        FALSE);
                }
                break;
            case DBE_IS_BONSAI:
            case DBE_IS_PERM:
            case DBE_IS_COMBINE:
                ss_dassert(indsea->is_prevkeysrk != NULL);
                indsea_reset(
                    indsea,
                    TRUE,
                    dbe_srk_getbkey(indsea->is_prevkeysrk),
                    FALSE);
                break;
            default:
                ss_error;
        }
        return(DBE_RC_SUCC);
}

/*#***********************************************************************\
 *
 *		indsea_chgdir_nexttoprev
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 *	nextp -
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
static dbe_ret_t indsea_chgdir_nexttoprev(dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_3(("indsea_chgdir_nexttoprev:seaid=%ld\n", indsea->is_seaid));
        ss_aassert(indsea->is_forwardp);

        indsea->is_forwardp = FALSE;
        indsea->is_isprevkey = FALSE;
        indsea->is_deletenext = FALSE;
        indsea->is_rowavailable = FALSE;
        ss_debug(indsea->is_prevcallreset = FALSE);
        ss_autotest_or_debug(dbe_bkey_setsearchminvtpl(indsea->is_lastkey));

        indsea->is_checkifdeleted = FALSE;
        if (indsea->is_prevkeysrk != NULL) {
            dbe_srk_donebuf(&indsea->is_prevkeysrk_buf, indsea->is_cd);
            indsea->is_prevkeysrk = NULL;
        }

        switch (indsea->is_state) {
            case DBE_IS_BEGIN:
                return(DBE_RC_END);
            case DBE_IS_END:
                indsea_reset(
                    indsea,
                    FALSE,
                    indsea->is_kc.kc_endkey,
                    TRUE);
                break;
            case DBE_IS_BONSAI:
            case DBE_IS_PERM:
            case DBE_IS_COMBINE:
                ss_aassert(indsea->is_retsea != NULL);
                ss_aassert(indsea->is_retsea->bst_srk != NULL);
                indsea_reset(
                    indsea,
                    FALSE,
                    NULL,
                    FALSE);
                break;
            default:
                ss_error;
        }
        return(DBE_RC_SUCC);
}

/*#***********************************************************************\
 *
 *		indsea_btrsea_getnextorprevif
 *
 * Gets the next key from a B-tree search. Does nothing if there already
 * is a pending key that is not yet used in the indsea.
 *
 * Parameters :
 *
 *	bst - in out, use
 *		B-tree search state structure
 *
 *	nextp - in
 *
 * Return value :
 *
 *      DBE_RC_FOUND
 *      DBE_RC_NOTFOUND
 *      DBE_RC_END
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t indsea_btrsea_getnextorprevif(
        btrsea_state_t* bst,
        bool nextp)
{
        su_profile_timer;

        ss_dprintf_3(("indsea_btrsea_getnextorprevif\n"));
        su_profile_start;

        if (bst->bst_keyreturned) {
            /* There is not a pending key, get the next key
             * from the search.
             */
            bst->bst_keyreturned = FALSE;
            if (nextp) {
                bst->bst_rc = dbe_btrsea_getnext(
                                    &bst->bst_search,
                                    &bst->bst_srk);
            } else {
                bst->bst_rc = dbe_btrsea_getprev(
                                    &bst->bst_search,
                                    &bst->bst_srk);
            }
        }
        su_profile_stop("indsea_btrsea_getnextorprevif");
        return(bst->bst_rc);
}

/*#***********************************************************************\
 *
 *		indsea_combine_nextorprev
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 *	nextp -
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
static dbe_ret_t indsea_combine_nextorprev(
        dbe_indsea_t* indsea,
        bool nextp)
{
        dbe_ret_t rc = 0;
        bool bonsai_retsea = FALSE;

        CHK_INDSEA(indsea);
        ss_dprintf_3(("indsea_combine_nextorprev:seaid=%ld, indsea->is_state = %d\n", indsea->is_seaid, indsea->is_state));

        indsea->is_rowavailable = FALSE;

        switch (indsea->is_state) {

            case DBE_IS_BEGIN:
                /* Beginning of search. */
                ss_dprintf_4(("indsea_combine_nextorprev: DBE_IS_BEGIN, seaid=%ld, indsea->is_state = %d\n", indsea->is_seaid, indsea->is_state));
                ss_aassert(!nextp);
                rc = DBE_RC_END;
                break;

            case DBE_IS_END:
                /* End of search. */
                ss_dprintf_4(("indsea_combine_nextorprev: DBE_IS_END, seaid=%ld, indsea->is_state = %d\n", indsea->is_seaid, indsea->is_state));
                ss_aassert(nextp);
                rc = DBE_RC_END;
                break;

            case DBE_IS_BONSAI:
                /* Use only Bonsai-tree. */
                ss_dprintf_4(("indsea_combine_nextorprev: DBE_IS_BONSAI, seaid=%ld, indsea->is_state = %d\n", indsea->is_seaid, indsea->is_state));
                rc = indsea_btrsea_getnextorprevif(&indsea->is_bonsai, nextp);
                break;

            case DBE_IS_PERM:
                /* Use only permanent tree. */
                ss_dprintf_4(("indsea_combine_nextorprev: DBE_IS_PERM, seaid=%ld, indsea->is_state = %d\n", indsea->is_seaid, indsea->is_state));
                rc = indsea_btrsea_getnextorprevif(&indsea->is_perm, nextp);
                break;

            case DBE_IS_COMBINE:
                /* Combine Bonsai and permanent trees. */
                ss_dprintf_4(("indsea_combine_nextorprev: DBE_IS_COMBINE, seaid=%ld, indsea->is_state = %d\n", indsea->is_seaid, indsea->is_state));
                indsea_btrsea_getnextorprevif(&indsea->is_bonsai, nextp);
                indsea_btrsea_getnextorprevif(&indsea->is_perm, nextp);
                rc = indsea_bkey_compare(
                        indsea->is_bonsai.bst_srk,
                        indsea->is_perm.bst_srk,
                        &bonsai_retsea,
                        nextp);
                if (rc == DBE_RC_END) {
                    indsea->is_retsea = NULL;
                } else if (bonsai_retsea) {
                    indsea->is_retsea = &indsea->is_bonsai;
                } else {
                    indsea->is_retsea = &indsea->is_perm;
                }
                break;

            default:
                ss_rc_error(indsea->is_state);
        }

        if (rc == DBE_RC_END) {
            /* Beginning or end of search. */
            ss_output_4(dbe_indsea_print(indsea));
            ss_aassert(indsea->is_bonsai.bst_rc == DBE_RC_END);
            ss_aassert(indsea->is_bonsai.bst_srk == NULL);
            ss_aassert(indsea->is_perm.bst_rc == DBE_RC_END);
            ss_aassert(indsea->is_perm.bst_srk == NULL);
            if (nextp) {
                ss_dprintf_4(("indsea_combine_nextorprev: set state to DBE_IS_END\n"));
                indsea->is_state = DBE_IS_END;
            } else {
                ss_dprintf_4(("indsea_combine_nextorprev: set state to DBE_IS_BEGIN\n"));
                indsea->is_state = DBE_IS_BEGIN;
            }
        } else {
            ss_aassert(indsea->is_retsea != NULL);
            ss_aassert(!indsea->is_retsea->bst_keyreturned);
            rc = indsea->is_retsea->bst_rc;
            indsea->is_retsea->bst_keyreturned = TRUE;
        }
        dbe_btrsea_setnodereadonly(&indsea->is_perm.bst_search);

        ss_dprintf_4(("indsea_combine_nextorprev %d: rc = %s\n", __LINE__, su_rc_nameof(rc)));
        return(rc);
}

/*#***********************************************************************\
 *
 *		indsea_init
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 *	nextp -
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
static dbe_ret_t indsea_init(dbe_indsea_t* indsea, bool nextp)
{
        dbe_ret_t rc;
        su_profile_timer;

        ss_dprintf_3(("indsea_init:seaid=%ld\n", indsea->is_seaid));
        su_profile_start;

        SU_BFLAG_CLEAR(indsea->is_flags, ISEA_MUSTINIT);
        indsea_start_searches(indsea, nextp);
        indsea->is_state = indsea_check_overlap(indsea, nextp);
        rc = indsea_combine_nextorprev(indsea, nextp);

        SU_BFLAG_CLEAR(indsea->is_flags, ISEA_BTRSEARESET);
        su_profile_stop("indsea_init");

        return(rc);
}

/*#***********************************************************************\
 *
 *		indsea_getnextorprev
 *
 * Returns the next key value from the index search. Searches
 * from Bonsai-tree and Permanent tree are combined here, but no
 * checks for delete marks are done.
 *
 * State transitions in structure indsea:
 *
 *      rc              indsea->
 *      --------------- -----------
 *      DBE_RC_FOUND    is_state != DBE_IS_BEGIN or DBE_IS_END
 *                      is_retkey = current search key
 *
 *      DBE_RC_NOTFOUND is_state != DBE_IS_BEGIN or DBE_IS_END
 *                      is_retkey = current search key
 *
 *      DBE_RC_RESET  is_state != DBE_IS_BEGIN or DBE_IS_END
 *                      is_retkey = current search key
 *
 *      DBE_RC_END      is_state = DBE_IS_BEGIN or DBE_IS_END
 *                      is_retkey = undefined
 *
 * Parameters :
 *
 *	indsea - in out, use
 *		index search
 *
 *	reset_at_buf_edge - in
 *		if TRUE, the search is reset when either of the
 *		searches is at the edge of the node, the checked
 *		edge depends on search direction
 *
 *	nextp - in
 *
 * Return value :
 *
 *      DBE_RC_FOUND
 *      DBE_RC_NOTFOUND
 *      DBE_RC_RESET
 *      DBE_RC_END
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t indsea_getnextorprev(
        dbe_indsea_t* indsea,
        bool reset_at_buf_edge,
        bool nextp)
{
        dbe_ret_t rc;
        bool bufedge;
        btrsea_state_t* retsea;

        CHK_INDSEA(indsea);
        ss_dprintf_3(("indsea_getnextorprev:seaid=%ld\n", indsea->is_seaid));

        if (indsea->is_flags != 0) {

            /* Check special case flags.
             */
            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
                if (reset_at_buf_edge && !indsea->is_mergegateentered) {
                    dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
                    indsea->is_mergegateentered = TRUE;
                }
                rc = indsea_init(indsea, nextp);
                return(rc);
            }

#ifndef SS_NOLOCKING
            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_RETRY)) {
                ss_dprintf_4(("indsea_getnextorprev %d: rc = DBE_RC_RETRY (%d)\n", __LINE__, DBE_RC_RETRY));
                return(DBE_RC_RETRY);
            }
#endif /* SS_NOLOCKING */
            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_CHGDIR)) {
                /* Changedirection also resets the search.
                 */
                ss_dprintf_4(("indsea_getnextorprev %d: rc = DBE_RC_CHANGEDIRECTION (%d)\n", __LINE__, DBE_RC_CHANGEDIRECTION));
                ss_dassert(nextp != indsea->is_forwardp);
                SU_BFLAG_CLEAR(indsea->is_flags, ISEA_CHGDIR);
                return(DBE_RC_CHANGEDIRECTION);
            } else {
                ss_dassert(nextp == indsea->is_forwardp);
            }

            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_RESET)) {
                ss_dprintf_1(("indsea_getnextorprev %d: rc = DBE_RC_RESET (%d)\n", __LINE__, DBE_RC_RESET));
                return(DBE_RC_RESET);
            }
        } else {
            ss_dassert(nextp == indsea->is_forwardp);
        }

        retsea = indsea->is_retsea;

        if (retsea != NULL && retsea->bst_srk != NULL) {
            if (nextp) {
                bufedge = dbe_srk_getkeypos(retsea->bst_srk) & DBE_KEYPOS_LAST;
            } else {
                bufedge = dbe_srk_getkeypos(retsea->bst_srk) & DBE_KEYPOS_FIRST;
            }
        } else {
            bufedge = FALSE;
        }

        if (bufedge) {

            if (reset_at_buf_edge && !indsea->is_rowavailable) {
                /* When we have merge active and one of the searches is
                 * at the end of buffer state, both searches must be
                 * reset. Reset is needed, because merge process
                 * could have moved keys from the Bonsai-tree to the
                 * Permanent tree, so the search buffer contents may not
                 * be synchronized.
                 */
                ss_dprintf_4(("indsea_getnextorprev %d: rc = DBE_RC_RESET (%d)\n", __LINE__, DBE_RC_RESET));
                return(DBE_RC_RESET);
            }

            if (retsea == &indsea->is_bonsai) {
                indsea_btrsea_getnextorprevif(&indsea->is_bonsai, nextp);
            } else {
                ss_dassert(retsea == &indsea->is_perm);
                indsea_btrsea_getnextorprevif(&indsea->is_perm, nextp);
            }
            /* Check if Bonsai-tree and Permanent tree searches
             * overlap. Also end of search situations are
             * handled by the check_overlap function.
             */
            indsea->is_state = indsea_check_overlap(indsea, nextp);
        }

        rc = indsea_combine_nextorprev(indsea, nextp);

        ss_dprintf_4(("indsea_getnextorprev %d: rc = %s\n", __LINE__, su_rc_nameof(rc)));
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_next
 *
 * Returns the next v-tuple in the search. If the next v-tuple is not
 * found, returns NULL. NULL return value means end of search.
 *
 * Parameters :
 *
 *	indsea - in out, use
 *		index search
 *
 *      p_srk - out, ref
 *          Search results is returned in *p_srk.
 *
 * Return value :
 *
 *      DBE_RC_END      - End of search.
 *      DBE_RC_NOTFOUND - Next key value not found in this step, it may
 *                        be found in the next step. Parameter p_srk
 *                        is not updated.
 *      DBE_RC_FOUND    - Next tuple found, parameter p_srk is updated.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_indsea_next(
        dbe_indsea_t* indsea,
        dbe_trxid_t stmttrxid,
        dbe_srk_t** p_srk)
{
        bool reset_at_buf_edge;
        dbe_ret_t rc;
        bool isdeletemark;
        bool key_status_known = TRUE;
        bool loop_once;
        bool retry_done = FALSE;
        dbe_bkey_t* k;
        ss_autotest_or_debug(int nloop = 0;)
        ss_autotest_or_debug(dbe_ret_t prev_rc;)
        ss_debug(bool resetp = FALSE;)
        su_profile_timer;

        SS_NOTUSED(stmttrxid);

        su_profile_start;

        ss_autotest_or_debug(rc = DBE_RC_SUCC);
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_next:seaid=%ld\n", indsea->is_seaid));
        ss_aassert(!indsea->is_ended);
        ss_debug(indsea->is_prevcallreset = FALSE);
        ss_debug(if (indsea_error_occured) return(DBE_RC_NOTFOUND);)
        SU_GENERIC_TIMER_START(SU_GENERIC_TIMER_DBE_INDSEAFETCH);

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_INSIDEMUTEX)) {

            if (indsea->is_idle || indsea->is_searchbeginactivectr++ == ISEA_SEARCHBEGINACTIVECTR_LIMIT) {
                if (!indsea->is_datasea) {
                    dbe_index_searchbeginactive(
                        indsea->is_index,
                        &indsea->is_sealrunode,
                        &indsea->is_idle);
                }
                indsea->is_searchbeginactivectr = 0;
            }

            SsSemEnter(indsea->is_activesem);
        }

        if (!indsea->is_forwardp) {
            /* Change search direction.
             */
            SU_BFLAG_SET(indsea->is_flags, ISEA_CHGDIR);
        }
        reset_at_buf_edge = indsea->is_mergeactive;

        /* Read the next key value. The read is redone when the
         * search is reset. The reset should happen only once
         * at one call of this function.
         */
        do {
            ss_aassert(nloop <= 10000); /* Not a real limit, just a debug
                                           check to detect possible endless
                                           loop. */
            ss_autotest_or_debug(nloop++);
            ss_autotest_or_debug(prev_rc = rc);

            loop_once = FALSE;

            /* Get the next key in ascending order.
             */
            rc = indsea_getnextorprev(indsea, reset_at_buf_edge, TRUE);

            ss_output_3(dbe_indsea_print(indsea));
            switch (rc) {
                case DBE_RC_FOUND:
                    ss_aassert(indsea->is_retsea != NULL);
                    ss_aassert(indsea->is_retsea->bst_srk != NULL);
                    key_status_known = TRUE;
                    isdeletemark = dbe_srk_isdeletemark(indsea->is_retsea->bst_srk);
                    if (indsea->is_deletenext) {
                        /* This key value is deleted by a preceding
                         * delete mark.
                         */
#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
                        if (!dbe_bkey_equal_vtpl(
                                indsea->is_lastdeletemark,
                                dbe_srk_getbkey(indsea->is_retsea->bst_srk))
                            && indsea->is_keyvldtype != DBE_KEYVLD_FOREIGN)
                        {
                            indsea_errorprint(indsea);
                            SsDbgPrintf("dbe_indsea_next:cur rc = %s (%d), prev_rc = %s (%d), nloop = %d\n",
                                su_rc_nameof(rc),
                                (int)rc,
                                su_rc_nameof(prev_rc),
                                (int)prev_rc,
                                nloop);
                            ss_error;
                        }
#endif /* defined(AUTOTEST_RUN) || defined(SS_DEBUG) */

                        if (isdeletemark && indsea->is_validate) {
                            /* During validate search we may see
                             * consecutive delete marks.
                             */
                            rc = DBE_RC_NOTFOUND;
                            key_status_known = FALSE;
                        } else {
                            if (isdeletemark) {
                                indsea_errorprint(indsea);
                                ss_dprintf_3(("dbe_indsea_next:prev_rc = %s (%d)\nnloop = %d\n",
                                    su_rc_nameof(prev_rc),
                                    (int)prev_rc,
                                    nloop));
                                ss_error;
                            }
                            indsea->is_deletenext = FALSE;
                            rc = DBE_RC_NOTFOUND;
                        }
                    } else if (isdeletemark) {
                        /* When the next key value is found, it should not be
                         * returned.
                         */
                        indsea->is_deletenext = TRUE;
                        key_status_known = FALSE;
                        rc = DBE_RC_NOTFOUND;
                        ss_autotest_or_debug(dbe_bkey_copy(
                                                indsea->is_lastdeletemark,
                                                dbe_srk_getbkey(indsea->is_retsea->bst_srk)));
                    }
#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
                    if (dbe_bkey_compare(
                                indsea->is_lastkey,
                                dbe_srk_getbkey(indsea->is_retsea->bst_srk))
                            > 0)
                    {
                        indsea_errorprint(indsea);
                        ss_error;
                    }
#endif
                    ss_autotest_or_debug(dbe_bkey_copy(indsea->is_lastkey, dbe_srk_getbkey(indsea->is_retsea->bst_srk)));
                    break;
                case DBE_RC_NOTFOUND:
                    ss_aassert(indsea->is_retsea != NULL);
                    ss_aassert(indsea->is_retsea->bst_srk != NULL);
                    break;
                case DBE_RC_LOCKTUPLE:
                    ss_aassert(indsea->is_retsea != NULL);
                    ss_aassert(indsea->is_retsea->bst_srk != NULL);
                    break;
#ifndef SS_NOLOCKING
                case DBE_RC_RETRY:
                    /* Retry the search from the current position.
                     */
                    ss_dprintf_2(("dbe_indsea_next:retry search\n"));
                    ss_aassert(SU_BFLAG_TEST(indsea->is_flags, ISEA_RETRY));
                    ss_aassert(indsea->is_retsea != NULL);
                    ss_debug(resetp = TRUE);
                    reset_at_buf_edge = FALSE;
                    loop_once = TRUE;
                    retry_done = TRUE;
                    if (!indsea->is_mergegateentered) {
                        dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
                        indsea->is_mergegateentered = TRUE;
                    }
                    k = dbe_srk_getbkey(indsea->is_retsea->bst_srk);
                    if (dbe_bkey_istrxid(k)) {
                        dbe_bkey_settrxid(k, DBE_TRXID_MIN);
                    }
                    dbe_bkey_setdeletemark(k);
                    indsea_reset(indsea, TRUE, k, TRUE);
                    SU_BFLAG_CLEAR(indsea->is_flags, ISEA_RETRY);
                    break;
#endif /* SS_NOLOCKING */

                case DBE_RC_RESET:
                    /* Reset the search.
                     */
                    ss_dprintf_2(("dbe_indsea_next:reset search\n"));
                    ss_aassert(reset_at_buf_edge || SU_BFLAG_TEST(indsea->is_flags, ISEA_RESET));
                    ss_aassert(!resetp);
                    ss_debug(resetp = TRUE);
                    reset_at_buf_edge = FALSE;
                    loop_once = TRUE;
                    SS_PMON_ADD_BETA(SS_PMON_INDSEARESET);
                    if (!indsea->is_mergegateentered) {
                        dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
                        indsea->is_mergegateentered = TRUE;
                    }
                    indsea_reset(indsea, TRUE, NULL, FALSE);
                    break;

                case DBE_RC_CHANGEDIRECTION:
                    /* Change search direction.
                     */
                    ss_dprintf_2(("dbe_indsea_next:change search direction.\n"));
                    reset_at_buf_edge = FALSE;
                    key_status_known = FALSE;
                    if (!indsea->is_mergegateentered) {
                        dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
                        indsea->is_mergegateentered = TRUE;
                    }
                    indsea_chgdir_prevtonext(indsea);
                    break;

                case DBE_RC_END:
                    ss_output_begin(indsea->is_deletenext &&
                                    indsea->is_keyvldtype != DBE_KEYVLD_FOREIGN)
                        ss_debug(indsea_errorprint(indsea));
                    ss_output_end
                    ss_aassert(!indsea->is_deletenext ||
                               indsea->is_keyvldtype == DBE_KEYVLD_FOREIGN);
                    if (indsea->is_mergegateentered) {
                        dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
                        indsea->is_mergegateentered = FALSE;
                    }

                    if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_INSIDEMUTEX)) {
                        SsSemExit(indsea->is_activesem);
                    }

                    ss_dprintf_2(("dbe_indsea_next:end of search\n"));
                    SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_INDSEAFETCH);
                    su_profile_stop("dbe_indsea_next");

                    return(DBE_RC_END);

                case DBE_ERR_ASSERT:
                    if (indsea->is_mergegateentered) {
                        dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
                        indsea->is_mergegateentered = FALSE;
                    }

                    if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_INSIDEMUTEX)) {
                        SsSemExit(indsea->is_activesem);
                    }
                    SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_INDSEAFETCH);
                    su_profile_stop("dbe_indsea_next");

                    return(DBE_ERR_ASSERT);

                default:
                    su_rc_error(rc);
            }
        } while (!key_status_known || loop_once);

        if (indsea->is_mergegateentered) {
            dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
            indsea->is_mergegateentered = FALSE;
        }
        ss_dprintf_2(("dbe_indsea_next:rc = %s\n", su_rc_nameof(rc)));
        ss_output_begin(rc == DBE_RC_FOUND)
            ss_dprintf_2(("dbe_indsea_next indsea->is_retsea->bst_srk key:\n"));
            ss_output_2(dbe_bkey_dprint(2, dbe_srk_getbkey(indsea->is_retsea->bst_srk)));
        ss_output_end
        ss_aassert(!indsea->is_deletenext);

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_INSIDEMUTEX)) {
            SsSemExit(indsea->is_activesem);
        }
        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_INDSEAFETCH);

        switch (rc) {
            case DBE_RC_LOCKTUPLE:
                if (!retry_done) {
                    SU_BFLAG_SET(indsea->is_flags, ISEA_RETRY);
                }
                /* FALLTHROUGH */
            case DBE_RC_FOUND:
                /* Return the found key value.
                 */
                ss_aassert(indsea->is_retsea->bst_srk != NULL);
                if (p_srk != NULL) {
                    *p_srk = indsea->is_retsea->bst_srk;
                }
                FAKE_CODE_BLOCK(FAKE_DBE_INDSEA_RESETFETCHNEXT,
                                dbe_indsea_reset(indsea, &indsea->is_tc, NULL, NULL);)
                su_profile_stop("dbe_indsea_next");
                return(rc);
            case DBE_RC_NOTFOUND:
                su_profile_stop("dbe_indsea_next");
                return(rc);
            default:
                su_rc_error(rc);
        }
        su_profile_stop("dbe_indsea_next");
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_prev
 *
 * Returns the prev v-tuple in the search. If the prev v-tuple is not
 * found, returns NULL. NULL return value means end of search.
 *
 * Parameters :
 *
 *	indsea - in out, use
 *		index search
 *
 *      p_srk - out, ref
 *          Search results is returned in *p_isr.
 *
 * Return value :
 *
 *      DBE_RC_END      - End of search.
 *      DBE_RC_NOTFOUND - Prev key value not found in this step, it may
 *                        be found in the prev step. Parameter p_srk
 *                        is not updated.
 *      DBE_RC_FOUND    - Prev tuple found, parameter p_srk is updated.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_indsea_prev(
        dbe_indsea_t* indsea,
        dbe_trxid_t stmttrxid,
        dbe_srk_t** p_srk)
{
        bool reset_at_buf_edge;
        dbe_ret_t rc;
        bool isdeletemark;
        bool key_status_known = TRUE;
        bool retry_done = FALSE;
        bool loop_once;
        dbe_bkey_t* k;
        ss_debug(int nloop = 0;)
        ss_autotest_or_debug(dbe_ret_t prev_rc = -1;)
        FAKE_CODE(bool fake_reset = FALSE;)

        ss_autotest_or_debug(rc = DBE_RC_SUCC;)

        SS_NOTUSED(stmttrxid);

        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_prev:seaid=%ld\n", indsea->is_seaid));
        ss_dassert(!indsea->is_ended);
        ss_dassert(!indsea->is_checkifdeleted);
        ss_debug(if (indsea_error_occured) return(DBE_RC_NOTFOUND);)

        if (indsea->is_idle || indsea->is_searchbeginactivectr++ == ISEA_SEARCHBEGINACTIVECTR_LIMIT) {
            if (!indsea->is_datasea) {
                dbe_index_searchbeginactive(
                    indsea->is_index,
                    &indsea->is_sealrunode,
                    &indsea->is_idle);
            }
            indsea->is_searchbeginactivectr = 0;
        }

        SsSemEnter(indsea->is_activesem);

        if (indsea->is_forwardp) {
            /* Change search direction.
             */
            SU_BFLAG_SET(indsea->is_flags, ISEA_CHGDIR);
        }
        reset_at_buf_edge = indsea->is_mergeactive;

        /* Read the prev key value. The read is redone when the
         * search is reset. The reset should happen only once
         * at one call of this function.
         */
        do {
            /*ss_dassert(nloop <= 10000); */ /* Not a real limit, just a debug
                                   check to detect possible endless
                                   loop. */
            ss_debug(nloop++;)
            ss_autotest_or_debug(prev_rc = rc);

            loop_once = FALSE;

            /* Get the prev key in ascending order.
             */
            if (SU_BFLAG_TEST(indsea->is_flags, ISEA_RETRY) &&
                !SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
                rc = DBE_RC_RETRY;
            } else if (indsea->is_isprevkey) {
                indsea->is_isprevkey = FALSE;
                rc = DBE_RC_FOUND;
            } else {
                rc = indsea_getnextorprev(indsea, reset_at_buf_edge, FALSE);
            }

            ss_output_3(dbe_indsea_print(indsea));
            switch (rc) {
                case DBE_RC_FOUND:
                    ss_dassert(indsea->is_retsea->bst_srk != NULL);
                    isdeletemark = dbe_srk_isdeletemark(indsea->is_retsea->bst_srk);
                    if (isdeletemark) {
                        /* Key is a delete mark.
                         */
                        if (indsea->is_checkifdeleted) {
                            /* We were checking if the prev key was deleted.
                             * Now we know it is deleted.
                             */
                            indsea->is_checkifdeleted = FALSE;
                            rc = DBE_RC_NOTFOUND;
                            key_status_known = TRUE;
                        } else {
                            /* After reset we may find delete marks, altough
                             * indsea->is_checkifdeleted flag is not set.
                             * Continue to the previous key value.
                             */
                            ss_output_begin(!indsea->is_prevcallreset)
                                ss_debug(indsea_errorprint(indsea));
                                ss_dprintf_3(("prev_rc = %s (%d)\nnloop = %d\n",
                                    su_rc_nameof(prev_rc),
                                    (int)prev_rc,
                                    nloop));
                            ss_output_end
                            ss_dassert(indsea->is_prevcallreset);
                            rc = DBE_RC_NOTFOUND;
                            key_status_known = FALSE;
                        }
                    } else if (indsea->is_checkifdeleted) {
                        /* We were checking if the prev key was deleted.
                         * Now we know that is is not deleted. Save the
                         * current key for the next call of this function.
                         */
                        indsea->is_checkifdeleted = FALSE;
                        indsea->is_isprevkey = TRUE;
                        key_status_known = TRUE;
                    } else {
                        /* Start to check if this key value is deleted by a
                         * preceding delete mark.
                         */
                        indsea->is_checkifdeleted = TRUE;
                        if (indsea->is_prevkeysrk == NULL) {
                            indsea->is_prevkeysrk = dbe_srk_initbuf(
                                                        &indsea->is_prevkeysrk_buf,
                                                        indsea->is_cd,
                                                        indsea->is_bkeyinfo);
                        }
                        dbe_srk_copy(
                            indsea->is_prevkeysrk,
                            indsea->is_retsea->bst_srk);
                        rc = DBE_RC_NOTFOUND;
                        key_status_known = FALSE;
                    }
                    ss_autotest_or_debug(dbe_bkey_copy(indsea->is_lastkey, dbe_srk_getbkey(indsea->is_retsea->bst_srk)));
                    ss_debug(indsea->is_prevcallreset = FALSE);
                    break;
                case DBE_RC_NOTFOUND:
                    ss_dassert(indsea->is_retsea->bst_srk != NULL);
                    ss_dassert(!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_BONSAINOTSET));
                    if (indsea->is_checkifdeleted
                        && (!SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA)
                            || dbe_btrsea_isbegin(&indsea->is_bonsai.bst_search)))
                    {
                        /* We were checking if the prev key was deleted.
                         * Now we know that is is not deleted.
                         */
                        indsea->is_checkifdeleted = FALSE;
                        key_status_known = TRUE;
                        rc = DBE_RC_FOUND;
                    }
                    break;
                case DBE_RC_LOCKTUPLE:
                    ss_dassert(indsea->is_retsea->bst_srk != NULL);
                    if (indsea->is_prevkeysrk == NULL) {
                        indsea->is_prevkeysrk = dbe_srk_initbuf(
                                                    &indsea->is_prevkeysrk_buf,
                                                    indsea->is_cd,
                                                    indsea->is_bkeyinfo);
                    }
                    dbe_srk_copy(
                        indsea->is_prevkeysrk,
                        indsea->is_retsea->bst_srk);
                    break;
#ifndef SS_NOLOCKING
                case DBE_RC_RETRY:
                    /* Retry the search from the current position.
                     */
                    ss_dprintf_2(("dbe_indsea_prev:retry the search\n"));
                    ss_dassert(SU_BFLAG_TEST(indsea->is_flags, ISEA_RETRY));
                    ss_dassert(indsea->is_retsea != NULL);
                    reset_at_buf_edge = FALSE;
                    indsea->is_isprevkey = FALSE;
                    loop_once = TRUE;
                    retry_done = TRUE;
                    if (!indsea->is_mergegateentered) {
                        dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
                        indsea->is_mergegateentered = TRUE;
                    }
                    k = dbe_srk_getbkey(indsea->is_prevkeysrk);
                    ss_dassert(!dbe_bkey_isdeletemark(k));
                    indsea_reset(indsea, FALSE, k, TRUE);
                    SU_BFLAG_CLEAR(indsea->is_flags, ISEA_RETRY);
                    indsea->is_checkifdeleted = FALSE;
                    ss_debug(indsea->is_prevcallreset = TRUE);
                    break;
#endif /* SS_NOLOCKING */

                case DBE_RC_RESET:
                    /* Reset the search.
                     */
                    ss_dassert(reset_at_buf_edge || SU_BFLAG_TEST(indsea->is_flags, ISEA_RESET));
#if 0
                    ss_dassert(!indsea->is_prevcallreset);
#endif
                    SS_PMON_ADD_BETA(SS_PMON_INDSEARESET);
                    reset_at_buf_edge = FALSE;
                    indsea->is_isprevkey = FALSE;
                    if (!indsea->is_mergegateentered) {
                        dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
                        indsea->is_mergegateentered = TRUE;
                    }
                    if (indsea->is_checkifdeleted) {
                        /* We were checking if key at indsea->is_prevkeysrk
                         * is deleted, start again from that key.
                         */
                        ss_dprintf_2(("dbe_indsea_prev:reset search %d\n", __LINE__));
                        indsea->is_checkifdeleted = FALSE;
                        ss_dassert(indsea->is_prevkeysrk != NULL);
                        ss_dassert(!dbe_srk_isdeletemark(indsea->is_prevkeysrk));
                        indsea_reset(
                            indsea,
                            FALSE,
                            dbe_srk_getbkey(indsea->is_prevkeysrk),
                            TRUE);
                    } else if (SU_BFLAG_TEST(indsea->is_flags, ISEA_RESET) &&
                               indsea->is_prevkeysrk != NULL) {
                        /* The reset request was external (by calling
                         * dbe_indsea_reset, dbe_indsea_setmergeend or
                         * dbe_indsea_setidle). In that case if is_prevkeysrk
                         * is not NULL, it contains the last returned key
                         * value. Start the search before that key value.
                         */
                        ss_dprintf_2(("dbe_indsea_prev:reset search %d\n", __LINE__));
                        ss_dassert(!dbe_srk_isdeletemark(indsea->is_prevkeysrk));
                        ss_dassert(nloop == 1);
                        indsea_reset(
                            indsea,
                            FALSE,
                            dbe_srk_getbkey(indsea->is_prevkeysrk),
                            FALSE);
                    } else {
                        /* We are not at any accepted key value, start from
                         * the key before current key.
                         */
                        ss_dprintf_2(("dbe_indsea_prev:reset search %d\n", __LINE__));
                        indsea_reset(indsea, FALSE, NULL, FALSE);
                    }
                    loop_once = TRUE;
                    ss_debug(indsea->is_prevcallreset = TRUE);
                    break;

                case DBE_RC_CHANGEDIRECTION:
                    /* Change search direction.
                     */
                    ss_dprintf_2(("dbe_indsea_prev:change search direction.\n"));
                    ss_dassert(nloop <= 1);
                    if (!indsea->is_mergegateentered) {
                        dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
                        indsea->is_mergegateentered = TRUE;
                    }
                    loop_once = TRUE;
                    reset_at_buf_edge = FALSE;
                    indsea_chgdir_nexttoprev(indsea);
                    FAKE_CODE(fake_reset = TRUE;)
                    ss_debug(indsea->is_prevcallreset = TRUE);
                    break;

                case DBE_RC_END:
                    ss_debug(indsea->is_prevcallreset = FALSE);
                    if (indsea->is_checkifdeleted) {
                        /* We were checking if the prev key was deleted.
                         * Now we know that is is not deleted.
                         */
                        indsea->is_checkifdeleted = FALSE;
                        rc = DBE_RC_FOUND;
                        key_status_known = TRUE;
                    } else {
                        if (indsea->is_prevkeysrk != NULL) {
                            dbe_srk_donebuf(&indsea->is_prevkeysrk_buf, indsea->is_cd);
                            indsea->is_prevkeysrk = NULL;
                        }
                        if (indsea->is_mergegateentered) {
                            dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
                            indsea->is_mergegateentered = FALSE;
                        }

                        ss_dprintf_2(("dbe_indsea_prev:end of search\n"));

                        SsSemExit(indsea->is_activesem);

                        return(DBE_RC_END);
                    }
                    break;
                default:
                    su_rc_error(rc);
            }

        } while ((!key_status_known &&
                  !rs_sysi_iscancelled((rs_sysi_t*)indsea->is_cd)) ||
                 loop_once);

        FAKE_CODE_BLOCK(
            FAKE_DBE_RESET_CHGDIR_PREV,
            {
                if (fake_reset) {
                    SU_BFLAG_SET(indsea->is_flags, ISEA_RESET);
                }
            }
        );

        if(rs_sysi_iscancelled(indsea->is_cd)){
            rs_sysi_setcancelled((rs_sysi_t*)indsea->is_cd, FALSE);
            if (indsea->is_checkifdeleted) {
                indsea->is_checkifdeleted = FALSE;
                key_status_known = TRUE;
            } else {
                if (indsea->is_prevkeysrk != NULL) {
                    dbe_srk_donebuf(&indsea->is_prevkeysrk_buf, indsea->is_cd);
                    indsea->is_prevkeysrk = NULL;
                }
                ss_dprintf_2(("dbe_indsea_prev: search cancelled\n"));
            }
            rc = DBE_RC_CANCEL;
        }

        if (indsea->is_mergegateentered) {
            dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
            indsea->is_mergegateentered = FALSE;
        }
        ss_dprintf_2(("dbe_indsea_prev:rc = %s\n", su_rc_nameof(rc)));
        ss_output_begin(rc == DBE_RC_FOUND)
            ss_dprintf_2(("dbe_indsea_prev indsea->is_prevkeysrk key:\n"));
            ss_output_2(dbe_bkey_dprint(2, dbe_srk_getbkey(indsea->is_prevkeysrk)));
        ss_output_end

        ss_dassert(!indsea->is_checkifdeleted);

        SsSemExit(indsea->is_activesem);

        switch (rc) {
            case DBE_RC_LOCKTUPLE:
                if (!retry_done) {
                    SU_BFLAG_SET(indsea->is_flags, ISEA_RETRY);
                }
                /* FALLTHROUGH */
            case DBE_RC_FOUND:
                /* Return the found key value.
                 */
                ss_dassert(indsea->is_prevkeysrk != NULL);
                if (p_srk != NULL) {
                    *p_srk = indsea->is_prevkeysrk;
                }
                return(rc);
            case DBE_RC_CANCEL:
            case DBE_RC_NOTFOUND:
                break;
            default:
                su_rc_error(rc);
        }
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_gotoend
 *
 * Go to end of search. The following prev call returns the last row
 * in the result set,
 *
 * Parameters :
 *
 *	indsea -
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
void dbe_indsea_gotoend(
        dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_gotoend\n"));
        ss_dassert(!indsea->is_ended);

        if (indsea->is_idle || indsea->is_searchbeginactivectr++ == ISEA_SEARCHBEGINACTIVECTR_LIMIT) {
            if (!indsea->is_datasea) {
                dbe_index_searchbeginactive(
                    indsea->is_index,
                    &indsea->is_sealrunode,
                    &indsea->is_idle);
            }
            indsea->is_searchbeginactivectr = 0;
        }

        SsSemEnter(indsea->is_activesem);

        dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
        indsea->is_mergegateentered = TRUE;

        if (SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
            indsea_init(indsea, TRUE);
        }

        indsea->is_longseqsea = FALSE;
        indsea->is_isprevkey = FALSE;
        indsea->is_deletenext = FALSE;
        indsea->is_rowavailable = FALSE;
        indsea->is_forwardp = TRUE;
        indsea->is_state = DBE_IS_END;
#ifndef SS_NOLOCKING
        SU_BFLAG_CLEAR(indsea->is_flags, ISEA_RETRY);
#endif /* SS_NOLOCKING */
        dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
        indsea->is_mergegateentered = FALSE;

        SsSemExit(indsea->is_activesem);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setposition
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 *	vtpl -
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
dbe_ret_t dbe_indsea_setposition(
        dbe_indsea_t* indsea,
        vtpl_t* vtpl)
{
        dbe_ret_t rc;
        dbe_bkey_t* bkey;

        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setposition\n"));
        ss_dassert(!indsea->is_ended);

        if (indsea->is_idle || indsea->is_searchbeginactivectr++ == ISEA_SEARCHBEGINACTIVECTR_LIMIT) {
            if (!indsea->is_datasea) {
                dbe_index_searchbeginactive(
                    indsea->is_index,
                    &indsea->is_sealrunode,
                    &indsea->is_idle);
            }
            indsea->is_searchbeginactivectr = 0;
        }

        SsSemEnter(indsea->is_activesem);

        dbe_index_mergegate_enter_shared(indsea->is_index, indsea->is_keyid);
        indsea->is_mergegateentered = TRUE;

        if (SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
            indsea_init(indsea, TRUE);
        }

        bkey = dbe_bkey_init_ex(indsea->is_cd, dbe_index_getbkeyinfo(indsea->is_index));
        dbe_bkey_setdeletemark(bkey);
        dbe_bkey_setvtpl(bkey, vtpl);
        if (dbe_bkey_compare(bkey, indsea->is_beginkey) < 0) {
            /* Smaller than search range begin key, set key to begin key. */
            dbe_bkey_copy(bkey, indsea->is_beginkey);
        } else if (dbe_bkey_compare(bkey, indsea->is_endkey) > 0) {
            /* Greater than search range end key, set key to end key. */
            dbe_bkey_copy(bkey, indsea->is_endkey);
        }

        indsea->is_deletenext = FALSE;
        indsea->is_rowavailable = FALSE;
        indsea->is_isprevkey = FALSE;
        indsea->is_checkifdeleted = FALSE;
        ss_autotest_or_debug(dbe_bkey_setsearchminvtpl(indsea->is_lastkey));

        indsea_reset(indsea, TRUE, bkey, TRUE);

        dbe_bkey_done_ex(indsea->is_cd, bkey);

        rc = indsea_combine_nextorprev(indsea, TRUE);

        if (rc != DBE_RC_END) {
            ss_dassert(indsea->is_retsea != NULL);
            indsea->is_retsea->bst_keyreturned = FALSE;
            indsea->is_rowavailable = TRUE;
        }

        dbe_index_mergegate_exit(indsea->is_index, indsea->is_keyid);
        indsea->is_mergegateentered = FALSE;

        SsSemExit(indsea->is_activesem);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_getlastkey
 *
 * Returns the last key in the search. If the search end has been
 * reched, returns the range end of the search, otherwise returns
 * the greatest key value returned by the search.
 *
 * Parameters :
 *
 *	indsea - in, use
 *
 *
 *	p_lastkey - out, give
 *
 *
 *	p_lasttrxid - out
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_indsea_getlastkey(
        dbe_indsea_t* indsea,
        dynvtpl_t* p_lastkey,
        dbe_trxid_t* p_lasttrxid)
{
        dbe_bkey_t* lastkey;

        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_getlastkey:seaid=%ld\n", indsea->is_seaid));
        ss_dassert(p_lastkey != NULL);
        ss_dassert(p_lasttrxid != NULL);

        SsSemEnter(indsea->is_activesem);

        /* WARNING! Change this when previous-call is
                    supported.
        */
        if (indsea->is_retsea == NULL ||
            indsea->is_retsea->bst_srk == NULL ||
            indsea->is_prevkeysrk != NULL) {
            /* return range end */
            lastkey = indsea->is_kc.kc_endkey;
        } else {
            /* return current last key */
            lastkey = dbe_srk_getbkey(indsea->is_retsea->bst_srk);
        }

        if (dbe_bkey_istrxid(lastkey)) {
            *p_lasttrxid = dbe_bkey_gettrxid(lastkey);
        } else {
            *p_lasttrxid = DBE_TRXID_NULL;
        }
        dynvtpl_setvtpl(p_lastkey, dbe_bkey_getvtpl(lastkey));

        SsSemExit(indsea->is_activesem);

        return(TRUE);
}

bool dbe_indsea_ischanged(
        dbe_indsea_t* indsea)
{
        bool changed;

        CHK_INDSEA(indsea);

        SsSemEnter(indsea->is_activesem);
        if (SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
            changed = FALSE;
        } else {
            changed = dbe_btrsea_ischanged(&indsea->is_perm.bst_search);
            if (!changed
                && SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_USEBONSAI)
                && SU_BFLAG_TEST(indsea->is_bonsaiflags, ISEA_ISBONSAISEA)) 
            {
                changed = dbe_btrsea_ischanged(&indsea->is_bonsai.bst_search);
            }
        }
        SsSemExit(indsea->is_activesem);

        ss_dprintf_1(("dbe_indsea_ischanged:seaid=%ld, changed=%d\n", indsea->is_seaid, changed));

        return(changed);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setmergestart
 *
 * Sets the merge active flag on for the search. During the search
 * the merge status is checked directly from the index system object.
 *
 * Parameters :
 *
 *	indsea - in out, use
 *		Index search.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_indsea_setmergestart(
        dbe_indsea_t* indsea,
        bool mergeactive)
{
        CHK_INDSEA(indsea);
        ss_dassert(mergeactive);
        ss_dassert(mergeactive == dbe_index_ismergeactive(indsea->is_index));

        SsSemEnter(indsea->is_activesem);

        ss_dprintf_1(("dbe_indsea_setmergestart:seaid=%ld, mergeactive=%d\n", indsea->is_seaid, indsea->is_mergeactive));

        indsea->is_mergeactive = TRUE;

        SsSemExit(indsea->is_activesem);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setmergestop
 *
 * Clears the merge active flag on for the search.
 *
 * Parameters :
 *
 *	indsea - in out, use
 *		Index search.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_indsea_setmergestop(
        dbe_indsea_t* indsea,
        bool mergeactive)
{
        CHK_INDSEA(indsea);

        SsSemEnter(indsea->is_activesem);

        ss_dprintf_1(("dbe_indsea_setmergestop:seaid=%ld, mergeactive=%d\n", indsea->is_seaid, indsea->is_mergeactive));
        ss_dassert(indsea->is_mergeactive);

        indsea_freebnode(indsea);

        indsea->is_mergeactive = mergeactive;

        SsSemExit(indsea->is_activesem);
}

#ifndef SS_NOLOCKING
/*##**********************************************************************\
 *
 *		dbe_indsea_setretry
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 *	retryp -
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
void dbe_indsea_setretry(
        dbe_indsea_t* indsea,
        bool retryp)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setretry:seaid=%ld, retryp = %d\n", indsea->is_seaid, retryp));

        SsSemEnter(indsea->is_activesem);

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
            if (retryp) {
                SU_BFLAG_SET(indsea->is_flags, ISEA_RETRY);
            } else {
                SU_BFLAG_CLEAR(indsea->is_flags, ISEA_RETRY);
            }
            ss_autotest_or_debug(dbe_bkey_setsearchminvtpl(indsea->is_lastkey));
        }

        SsSemExit(indsea->is_activesem);
}
#endif /* SS_NOLOCKING */

/*##**********************************************************************\
 *
 *		dbe_indsea_setidle
 *
 *
 *
 * Parameters :
 *
 *	indsea -
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
bool dbe_indsea_setidle(
        dbe_indsea_t* indsea)
{
        bool succp;

        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setidle:seaid=%ld\n", indsea->is_seaid));
        ss_dassert(!indsea->is_ended);

        SsSemEnter(indsea->is_activesem);

        if (!indsea->is_idle) {
            indsea_freebnode(indsea);
            indsea->is_idle = TRUE;
            succp = TRUE;
        } else {
            succp = FALSE;
        }

        SsSemExit(indsea->is_activesem);

        return(succp);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setended
 *
 * Marks the search as ended. Releases some resources from the search
 * but the current search result is still accessible. Used e.g. when
 * the search is used to get only one data tuple from the primary key
 * during secondary key search. After the data tuple is found, the search
 * is not accessed but the data tuple value must still be accessible.
 *
 * Parameters :
 *
 *	indsea -
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
void dbe_indsea_setended(
        dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setended:seaid=%ld\n", indsea->is_seaid));
        ss_dassert(!indsea->is_ended);

        /* Remove from the search list. */
        if (!indsea->is_datasea) {
            dbe_index_searchremove(
                indsea->is_index,
                &indsea->is_sealistnode,
                &indsea->is_sealrunode,
                &indsea->is_idle);
        }

        indsea->is_ended = TRUE;

        SsSemEnter(indsea->is_activesem);

        /* Release cache buffers. */
        indsea_freebnode(indsea);

        SsSemExit(indsea->is_activesem);
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setlongseqsea
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_indsea_setlongseqsea(dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setlongseqsea:seaid=%ld\n", indsea->is_seaid));

        indsea->is_longseqsea = TRUE;

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
            dbe_btrsea_setlongseqsea(&indsea->is_perm.bst_search);
        }
}

/*##**********************************************************************\
 *
 *		dbe_indsea_clearlongseqsea
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_indsea_clearlongseqsea(dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_clearlongseqsea:seaid=%ld\n", indsea->is_seaid));

        indsea->is_longseqsea = FALSE;

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
            dbe_btrsea_clearlongseqsea(&indsea->is_perm.bst_search);
        }
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setdatasea
 *
 *
 *
 * Parameters :
 *
 *	indsea -
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
void dbe_indsea_setdatasea(dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setdatasea:seaid=%ld\n", indsea->is_seaid));
        ss_assert(indsea->is_datasea);

        indsea->is_datasea = TRUE;

        if (!SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT)) {
            dbe_btrsea_setreadaheadsize(&indsea->is_perm.bst_search, 0);
        }
}

/*##**********************************************************************\
 *
 *		dbe_indsea_setmaxpoolblocks
 *
 *
 *
 * Parameters :
 *
 *	indsea -
 *
 *
 *	maxpoolblocks -
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
void dbe_indsea_setmaxpoolblocks(
        dbe_indsea_t* indsea,
        ulong maxpoolblocks)
{
        CHK_INDSEA(indsea);
        ss_dprintf_1(("dbe_indsea_setmaxpoolblocks:seaid=%ld: blocks = %ld\n", indsea->is_seaid, maxpoolblocks));

        indsea->is_maxpoolblocks = maxpoolblocks;
}

#ifdef SS_QUICKSEARCH

void* dbe_indsea_getquicksearch(
        dbe_indsea_t* indsea,
        bool longsearch)
{
        CHK_INDSEA(indsea);
        ss_dassert(SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT));

        ss_error;

        indsea->is_quicksea = &indsea->is_quickseabuf;

        dbe_btrsea_initbufvalidate_ex(
            indsea->is_quicksea,
            dbe_index_getpermtree(indsea->is_index),
            &indsea->is_kc,
            &indsea->is_tc,
            FALSE,
            indsea->is_validate,
            indsea->is_keyvldtype,
            indsea->is_earlyvld,
            FALSE,
            FALSE,
            FALSE);
        if (longsearch) {
            dbe_btrsea_setlongseqsea(indsea->is_quicksea);
            dbe_btrsea_setreadaheadsize(
                indsea->is_quicksea,
                (uint)dbe_index_getreadaheadsize(indsea->is_index));
        }

        return(indsea->is_quicksea);
}

#endif /* SS_QUICKSEARCH */

/*##**********************************************************************\
 *
 *		dbe_indsea_print
 *
 * Prints the index search buffers to stdout.
 *
 * Parameters :
 *
 *	indsea - in, use
 *		index search
 *
 * Return value :
 *
 *      TRUE always
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_indsea_print(dbe_indsea_t* indsea)
{
        CHK_INDSEA(indsea);

        SsDbgPrintf("Search state:\n");
        switch (indsea->is_state) {
            case DBE_IS_BEGIN:
                SsDbgPrintf("begin\n");
                return(TRUE);
            case DBE_IS_END:
                SsDbgPrintf("end\n");
                return(TRUE);
            case DBE_IS_BONSAI:
                SsDbgPrintf("bonsai\n");
                break;
            case DBE_IS_PERM:
                SsDbgPrintf("perm\n");
                break;
            case DBE_IS_COMBINE:
                SsDbgPrintf("combine\n");
                break;
            default:
                ss_rc_error(indsea->is_state);
        }

        SsDbgPrintf("Bonsai tree search key: (%s)\n",
            indsea->is_retsea == &indsea->is_bonsai ? "retsea" : "");
        if (indsea->is_bonsai.bst_srk != NULL) {
            dbe_bkey_dprint(1, dbe_srk_getbkey(indsea->is_bonsai.bst_srk));
        } else {
            SsDbgPrintf("NULL\n");
        }

        SsDbgPrintf("Permanent tree search key: (%s)\n",
            indsea->is_retsea == &indsea->is_perm ? "retsea" : "");
        if (indsea->is_perm.bst_srk != NULL) {
            dbe_bkey_dprint(1, dbe_srk_getbkey(indsea->is_perm.bst_srk));
        } else {
            SsDbgPrintf("NULL\n");
        }
        SsDbgPrintf("Search mintrxnum=%ld, maxtrxnum=%ld, usertrxid=%ld\n",
            DBE_TRXNUM_GETLONG(indsea->is_tc.tc_mintrxnum),
            DBE_TRXNUM_GETLONG(indsea->is_tc.tc_maxtrxnum),
            DBE_TRXID_GETLONG(indsea->is_tc.tc_usertrxid));

        return(TRUE);
}

#ifndef SS_LIGHT

/*##**********************************************************************\
 *
 *		dbe_indsea_printinfoheader
 *
 *
 *
 * Parameters :
 *
 *      fp -
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
void dbe_indsea_printinfoheader(void* fp)
{
        SsFprintf(fp, "      %-6s %-4s %-4s %-4s %-3s %-3s %-3s %-3s %-3s %-3s %-3s %s\n",
            "Id",
            "Dlnx",
            "Chdl",
            "Ispk",
            "Rst",
            "Mrg",
            "Ini",
            "Fwd",
            "Vld",
            "Evl",
            "Lss",
            "MPBl");
}

/*##**********************************************************************\
 *
 *		dbe_indsea_printinfo
 *
 *
 *
 * Parameters :
 *
 *	fp -
 *
 *
 *	indsea -
 *
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
void dbe_indsea_printinfo(
        void* fp,
        dbe_indsea_t* indsea)
{
        SsFprintf(fp, "      %-6ld %-4d %-4d %-4d %-3d %-3d %-3d %-3d %-3d %-3d %-3d %ld\n",
            indsea->is_seaid,
            indsea->is_deletenext,
            indsea->is_checkifdeleted,
            indsea->is_isprevkey,
            SU_BFLAG_TEST(indsea->is_flags, ISEA_RESET) != 0,
            indsea->is_mergeactive,
            SU_BFLAG_TEST(indsea->is_flags, ISEA_MUSTINIT) == 0,
            indsea->is_forwardp,
            indsea->is_validate,
            indsea->is_earlyvld,
            indsea->is_longseqsea,
            indsea->is_maxpoolblocks);
}

#endif /* SS_LIGHT */
