/*************************************************************************\
**  source       * dbe1trdd.c
**  directory    * dbe
**  description  * Transaction data dictionary operations.
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

#include <ssenv.h>

#include <ssstdio.h>
#include <ssstddef.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssint8.h>

#include <su0list.h>

#include <rs0types.h>
#include <rs0error.h>
#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0relh.h>
#include <rs0viewh.h>
#include <rs0key.h>
#include <rs0rbuf.h>
#include <rs0auth.h>

#include "dbe0type.h"
#include "dbe9type.h"
#include "dbe6srk.h"
#include "dbe6log.h"
#include "dbe5isea.h"
#include "dbe5imrg.h"
#include "dbe4srch.h"
#include "dbe4tupl.h"
#include "dbe4srli.h"
#include "dbe0erro.h"
#include "dbe1seq.h"
#include "dbe0seq.h"
#include "dbe0trx.h"
#include "dbe0db.h"
#include "dbe0user.h"
#include "dbe1trdd.h"

#ifndef SS_NODDUPDATE

#define CHK_TRDD(t) ss_dassert(SS_CHKPTR(t) && (t)->trdd_chk == DBE_CHK_TRDD)

#define TRDD_UPDIDXWRITESCTR 1000

/* Enum for different data dictionary operations.
 */
typedef enum {
        TRDD_INSREL,
        TRDD_DELREL,
        TRDD_TRUNCATEREL,
        TRDD_ALTERREL,
        TRDD_RENAMEREL,
        TRDD_INSKEY,
        TRDD_DELKEY,
        TRDD_INSVIEW,
        TRDD_DELVIEW,
        TRDD_CHANGEREL,
        TRDD_INSSEQ,
        TRDD_DELSEQ,
        TRDD_INSNAME,
        TRDD_DELNAME,
        TRDD_INSEVENT,
        TRDD_DELEVENT
} trdd_ddtype_t;

/* Structure used to store info from data dictionary operations.
 * When transaction commits, these modifications are made visible.
 */
typedef struct {
        trdd_ddtype_t tdd_type;
        bool tdd_logonly; /* TRUE = this record is only relevent for logging */
        dbe_trxid_t   tdd_stmttrxid;
        bool          tdd_logstmtcommit;
        bool          tdd_physdelete;   /* for truncate */
        union {
            struct {
                rs_relh_t* relh;
            } insrel;
            struct {
                rs_relh_t* relh;
            } delrel;
            struct {
                rs_relh_t* relh;
                int ncolumns;
            } alterrel;
            struct {
                rs_relh_t* relh;
                rs_entname_t* newname;
            } renamerel;
            struct {
                rs_relh_t* relh;
                rs_key_t*  key;
                bool createkey;
            } inskey;
            struct {
                rs_relh_t* relh;
                rs_key_t*  key;
            } delkey;
            struct {
                rs_viewh_t* viewh;
            } insview;
            struct {
                rs_viewh_t* viewh;
            } delview;
            struct {
                rs_relh_t* relh;
            } changerel;
            struct {
                long seqid;
                rs_entname_t* seqname;
                bool densep;
            } insseq;
            struct {
                long seqid;
                rs_entname_t* seqname;
                bool densep;
            } delseq;
            struct {
                rs_entname_t* name;
            } insname;
            struct {
                rs_entname_t* name;
            } delname;
            struct {
                rs_event_t* event;
            } insevent;
            struct {
                rs_event_t* event;
            } delevent;
        } tdd_;
} trdd_dd_t;

typedef struct {
        bool                     vi_initp;
        dbe_tuple_createindex_t* vi_ci;
        dbe_tuple_dropindex_t*   vi_di;
        su_list_node_t*          vi_node;
        su_list_t*               vi_keylist;
        su_list_node_t*          vi_klnode;
} trdd_vldinfo_t;

struct dbe_trdd_st {
        ss_debug(dbe_chk_t trdd_chk;)
        rs_sysi_t*      trdd_cd;
        dbe_db_t*       trdd_db;
        dbe_trx_t*      trdd_trx;
        rs_rbuf_t*      trdd_rbuf;
        su_list_t*      trdd_ddlist;
        trdd_vldinfo_t  trdd_vldinfo;
        long            trdd_nindexwrites;
        long            trdd_nlogwrites;
        bool            trdd_initcommit;
        dbe_trxid_t     trdd_usertrxid;
        dbe_trxid_t     trdd_stmttrxid;
        su_list_node_t* trdd_node;
        dbe_log_t*      trdd_log;
        bool            trdd_ddchanged;
        bool            trdd_cleanupdone;
        su_list_t*      trdd_deferredblobunlinklist;
        bool            trdd_mergedisabled;
};

static su_list_node_t* trdd_ddfind_ex(
        dbe_trdd_t* trdd,
        rs_entname_t* name,
        trdd_ddtype_t ddtype,
        su_list_node_t* stop_node);

static dbe_ret_t trdd_truncaterel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        bool physdelete,
        bool drop);

/*##**********************************************************************\
 *
 *		dbe_trdd_init
 *
 * Initializes a data dictionary transaction object. Trdd object is used
 * to hold data dictionary operations of unfinished transaction.
 *
 * Parameters :
 *
 *	cd - in, hold
 *		System info object.
 *
 *	db - in, hold
 *		Database object.
 *
 *	trx - in, hold
 *		Transaction object.
 *
 *	stmttrxid - in
 *		Statement transaction id.
 *
 *	log - in, hold
 *		Database logical log object.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_trdd_t* dbe_trdd_init(
        void* cd,
        dbe_db_t* db,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        dbe_trxid_t stmttrxid,
        dbe_log_t* log)
{
        dbe_trdd_t* trdd;

        ss_dprintf_1(("dbe_trdd_init\n"));

        trdd = SSMEM_NEW(dbe_trdd_t);

        ss_debug(trdd->trdd_chk = DBE_CHK_TRDD;)
        trdd->trdd_cd = cd;
        trdd->trdd_db = db;
        trdd->trdd_trx = trx;
        trdd->trdd_rbuf = dbe_db_getrbuf(db);
        trdd->trdd_ddlist = su_list_init(NULL);
        trdd->trdd_vldinfo.vi_initp = FALSE;
        trdd->trdd_nindexwrites = 0;
        trdd->trdd_nlogwrites = 0;
        trdd->trdd_initcommit = FALSE;
        trdd->trdd_usertrxid = usertrxid;
        trdd->trdd_stmttrxid = stmttrxid;
        trdd->trdd_log = log;
        trdd->trdd_ddchanged = FALSE;
        trdd->trdd_cleanupdone = FALSE;
        trdd->trdd_deferredblobunlinklist = NULL;
        trdd->trdd_mergedisabled = FALSE;

        return(trdd);
}


/*#***********************************************************************\
 *
 *      trdd_inittdd
 *
 * Allocates and partially initializes a tdd object.
 *
 * Parameters:
 *      ddtype - in
 *          data dictionary change type
 *
 * Return value - give:
 *      pointer to newly allocated tdd object
 *
 * Limitations:
 *
 * Globals used:
 */
static trdd_dd_t* trdd_inittdd(trdd_ddtype_t ddtype)
{
        trdd_dd_t* tdd = SSMEM_NEW(trdd_dd_t);
        tdd->tdd_type = ddtype;
        tdd->tdd_logonly = FALSE;
        tdd->tdd_logstmtcommit = FALSE;
        return (tdd);
}

/*#***********************************************************************\
 *
 *		trdd_donetdd
 *
 * releases memory of one tdd object and updated rbuf.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	tdd -
 *
 *
 *	rbuf -
 *
 *
 *	trxsuccp -
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
static bool trdd_donetdd(
        void* cd,
        dbe_trdd_t* trdd,
        trdd_dd_t* tdd,
        rs_rbuf_t* rbuf,
        bool trxsuccp)
{
        long relid;
        bool succp = TRUE;

        ss_dprintf_3(("trdd_donetdd:trxsuccp = %d\n", trxsuccp));

        if (trxsuccp) {
            trdd->trdd_ddchanged = TRUE;
            switch (tdd->tdd_type) {
                case TRDD_INSREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSREL\n"));
                    dbe_trx_abortrelh(
                            trdd->trdd_trx,
                            rs_relh_relid(cd, tdd->tdd_.insrel.relh));
                    rs_relh_setaborted(cd, tdd->tdd_.insrel.relh);
                    succp = rs_rbuf_updaterelname(
                                cd,
                                rbuf,
                                rs_relh_entname(cd, tdd->tdd_.insrel.relh),
                                rs_relh_relid(cd, tdd->tdd_.insrel.relh),
                                rs_relh_cardin(cd, tdd->tdd_.insrel.relh));
                    SS_MEM_SETUNLINK(tdd->tdd_.insrel.relh);
                    rs_relh_done(cd, tdd->tdd_.insrel.relh);
                    break;
                case TRDD_DELREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELREL\n"));
                    if (!tdd->tdd_logonly) {
                        dbe_trx_abortrelh(
                                trdd->trdd_trx,
                                rs_relh_relid(cd, tdd->tdd_.delrel.relh));
                        rs_relh_setaborted(cd, tdd->tdd_.delrel.relh);
                        dbe_db_adddropcardinal(
                                trdd->trdd_db,
                                rs_relh_relid(cd, tdd->tdd_.delrel.relh));
                        rs_rbuf_removerelh(
                                cd,
                                rbuf,
                                rs_relh_entname(cd, tdd->tdd_.delrel.relh));
                        rs_rbuf_removename(
                                cd,
                                rbuf,
                                rs_relh_entname(cd, tdd->tdd_.delrel.relh),
                                RSRBUF_NAME_RELATION);
                    }
                    SS_MEM_SETUNLINK(tdd->tdd_.delrel.relh);
                    rs_relh_done(cd, tdd->tdd_.delrel.relh);
                    break;
                case TRDD_TRUNCATEREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_TRUNCATEREL\n"));
                    ss_dassert(!tdd->tdd_logonly);
                    if (!tdd->tdd_physdelete) {
                        ss_dprintf_4(("trdd_donetdd:TRDD_TRUNCATEREL, abort relh\n"));
                        dbe_trx_abortrelh(
                                trdd->trdd_trx,
                                rs_relh_relid(cd, tdd->tdd_.delrel.relh));
                        rs_relh_setaborted(cd, tdd->tdd_.delrel.relh);
                        rs_rbuf_relhunbuffer_dropcardin(
                            cd,
                            rbuf,
                            rs_relh_entname(cd, tdd->tdd_.delrel.relh));
                    }
                    SS_MEM_SETUNLINK(tdd->tdd_.delrel.relh);
                    rs_relh_done(cd, tdd->tdd_.delrel.relh);
                    break;
                case TRDD_ALTERREL:
                {
                    ulong relid = rs_relh_relid(cd, tdd->tdd_.alterrel.relh);
                    ss_dprintf_4(("trdd_donetdd:TRDD_ALTERREL\n"));
                    if (relid != RS_RELID_KEYS && relid != RS_RELID_CARDINAL) {
                        /* Not during db conversion. */
                        dbe_trx_abortrelh(
                                trdd->trdd_trx,
                                rs_relh_relid(cd, tdd->tdd_.alterrel.relh));
                        rs_relh_setaborted(cd, tdd->tdd_.alterrel.relh);
                        rs_rbuf_relhunbuffer(
                            cd,
                            rbuf,
                            rs_relh_entname(cd, tdd->tdd_.alterrel.relh));
                    }
                    SS_MEM_SETUNLINK(tdd->tdd_.alterrel.relh);
                    rs_relh_done(cd, tdd->tdd_.alterrel.relh);
                    break;
                }
                case TRDD_RENAMEREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_RENAMEREL\n"));
                    dbe_trx_abortrelh(
                            trdd->trdd_trx,
                            rs_relh_relid(cd, tdd->tdd_.renamerel.relh));
                    rs_relh_setaborted(cd, tdd->tdd_.renamerel.relh);
                    rs_rbuf_renamerel(
                        cd,
                        rbuf,
                        rs_relh_entname(cd, tdd->tdd_.renamerel.relh),
                        tdd->tdd_.renamerel.newname);
                    SS_MEM_SETUNLINK(tdd->tdd_.renamerel.relh);
                    rs_relh_done(cd, tdd->tdd_.renamerel.relh);
                    rs_entname_done(tdd->tdd_.renamerel.newname);
                    break;
                case TRDD_INSKEY:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSKEY\n"));
                    relid = rs_relh_relid(cd, tdd->tdd_.inskey.relh);
                    if (relid != RS_RELID_TABLES 
                        && relid != RS_RELID_KEYS
                        && tdd->tdd_.inskey.createkey) 
                    {
                        /* Not during db conversion. */
                        dbe_trx_abortrelh(
                                trdd->trdd_trx,
                                rs_relh_relid(cd, tdd->tdd_.inskey.relh));
                        rs_relh_setaborted(cd, tdd->tdd_.inskey.relh);
                        rs_rbuf_relhunbuffer(
                                    cd,
                                    rbuf,
                                    rs_relh_entname(cd, tdd->tdd_.inskey.relh));
                    }
                    rs_key_done(cd, tdd->tdd_.inskey.key);
                    SS_MEM_SETUNLINK(tdd->tdd_.inskey.relh);
                    rs_relh_done(cd, tdd->tdd_.inskey.relh);
                    break;
                case TRDD_DELKEY:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELKEY\n"));
                    dbe_trx_abortrelh(
                            trdd->trdd_trx,
                            rs_relh_relid(cd, tdd->tdd_.delkey.relh));
                    rs_relh_setaborted(cd, tdd->tdd_.delkey.relh);
                    rs_rbuf_relhunbuffer(
                                cd,
                                rbuf,
                                rs_relh_entname(cd, tdd->tdd_.delkey.relh));
                    rs_key_done(cd, tdd->tdd_.delkey.key);
                    SS_MEM_SETUNLINK(tdd->tdd_.delkey.relh);
                    rs_relh_done(cd, tdd->tdd_.delkey.relh);
                    break;
                case TRDD_INSVIEW:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSVIEW\n"));
                    succp = rs_rbuf_updateviewname(
                                cd,
                                rbuf,
                                rs_viewh_entname(cd, tdd->tdd_.insview.viewh),
                                rs_viewh_viewid(cd, tdd->tdd_.insview.viewh));
                    rs_viewh_done(cd, tdd->tdd_.insview.viewh);
                    break;
                case TRDD_DELVIEW:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELVIEW\n"));
                    rs_rbuf_removeviewh(
                        cd,
                        rbuf,
                        rs_viewh_entname(cd, tdd->tdd_.delview.viewh));
                    rs_rbuf_removename(
                        cd,
                        rbuf,
                        rs_viewh_entname(cd, tdd->tdd_.delview.viewh),
                        RSRBUF_NAME_VIEW);
                    rs_viewh_done(cd, tdd->tdd_.delview.viewh);
                    break;
                case TRDD_CHANGEREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_CHANGEREL\n"));
                    dbe_trx_abortrelh(
                            trdd->trdd_trx,
                            rs_relh_relid(cd, tdd->tdd_.changerel.relh));
                    rs_relh_setaborted(cd, tdd->tdd_.changerel.relh);
                    rs_rbuf_relhunbuffer(
                                cd,
                                rbuf,
                                rs_relh_entname(cd, tdd->tdd_.changerel.relh));
                    SS_MEM_SETUNLINK(tdd->tdd_.changerel.relh);
                    rs_relh_done(cd, tdd->tdd_.changerel.relh);
                    break;
#ifndef SS_NOSEQUENCE
                case TRDD_INSSEQ:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSSEQ\n"));
                    rs_entname_done(tdd->tdd_.insseq.seqname);
                    break;
                case TRDD_DELSEQ:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELSEQ\n"));
                    rs_rbuf_removename(cd, rbuf, tdd->tdd_.delseq.seqname, RSRBUF_NAME_SEQUENCE);
                    rs_entname_done(tdd->tdd_.delseq.seqname);
                    break;
#endif /* SS_NOSEQUENCE */
                case TRDD_INSNAME:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSNAME\n"));
                    rs_entname_done(tdd->tdd_.insname.name);
                    break;
                case TRDD_DELNAME:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELNAME\n"));
                    rs_rbuf_removename(cd, rbuf, tdd->tdd_.delname.name, RSRBUF_NAME_GENERIC);
                    rs_entname_done(tdd->tdd_.delname.name);
                    break;
                case TRDD_INSEVENT:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSEVENT\n"));
                    rs_event_done(cd, tdd->tdd_.insevent.event);
                    break;
                case TRDD_DELEVENT:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELEVENT\n"));
                    rs_event_done(cd, tdd->tdd_.delevent.event);
                    break;
                default:
                    ss_derror;
            }
        } else {
            switch (tdd->tdd_type) {
                case TRDD_INSREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSREL\n"));
                    rs_rbuf_removename(cd, rbuf, rs_relh_entname(cd, tdd->tdd_.insrel.relh), RSRBUF_NAME_RELATION);
                    SS_MEM_SETUNLINK(tdd->tdd_.insrel.relh);
                    rs_relh_done(cd, tdd->tdd_.insrel.relh);
                    break;
                case TRDD_DELREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELREL\n"));
                    rs_rbuf_relhunbuffer(
                        cd,
                        rbuf,
                        rs_relh_entname(cd, tdd->tdd_.delrel.relh));
                    SS_MEM_SETUNLINK(tdd->tdd_.delrel.relh);
                    rs_relh_done(cd, tdd->tdd_.delrel.relh);
                    break;
                case TRDD_TRUNCATEREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_TRUNCATEREL\n"));
                    if (!tdd->tdd_physdelete) {
                        rs_rbuf_relhunbuffer(
                            cd,
                            rbuf,
                            rs_relh_entname(cd, tdd->tdd_.delrel.relh));
                    }
                    SS_MEM_SETUNLINK(tdd->tdd_.delrel.relh);
                    rs_relh_done(cd, tdd->tdd_.delrel.relh);
                    break;
                case TRDD_ALTERREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_ALTERREL\n"));
                    rs_relh_setaborted(cd, tdd->tdd_.alterrel.relh);
                    rs_rbuf_relhunbuffer(
                        cd,
                        rbuf,
                        rs_relh_entname(cd, tdd->tdd_.alterrel.relh));
                    SS_MEM_SETUNLINK(tdd->tdd_.alterrel.relh);
                    rs_relh_done(cd, tdd->tdd_.alterrel.relh);
                    break;
                case TRDD_RENAMEREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_RENAMEREL\n"));
                    rs_relh_setaborted(cd, tdd->tdd_.renamerel.relh);
                    rs_rbuf_removename(
                        cd,
                        rbuf,
                        tdd->tdd_.renamerel.newname,
                        RSRBUF_NAME_RELATION);
                    SS_MEM_SETUNLINK(tdd->tdd_.renamerel.relh);
                    rs_relh_done(cd, tdd->tdd_.renamerel.relh);
                    rs_entname_done(tdd->tdd_.renamerel.newname);
                    break;
                case TRDD_INSKEY:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSKEY\n"));
                    if (tdd->tdd_.inskey.createkey) {
                        rs_relh_setaborted(cd, tdd->tdd_.inskey.relh);
                        rs_rbuf_relhunbuffer(
                            cd,
                            rbuf,
                            rs_relh_entname(cd, tdd->tdd_.inskey.relh));
                    }
                    rs_key_done(cd, tdd->tdd_.inskey.key);
                    SS_MEM_SETUNLINK(tdd->tdd_.inskey.relh);
                    rs_relh_done(cd, tdd->tdd_.inskey.relh);
                    break;
                case TRDD_DELKEY:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELKEY\n"));
                    rs_relh_setaborted(cd, tdd->tdd_.delkey.relh);
                    rs_rbuf_relhunbuffer(
                        cd,
                        rbuf,
                        rs_relh_entname(cd, tdd->tdd_.delkey.relh));
                    rs_key_done(cd, tdd->tdd_.delkey.key);
                    SS_MEM_SETUNLINK(tdd->tdd_.delkey.relh);
                    rs_relh_done(cd, tdd->tdd_.delkey.relh);
                    break;
                case TRDD_INSVIEW:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSVIEW\n"));
                    rs_rbuf_removename(cd, rbuf, rs_viewh_entname(cd, tdd->tdd_.insview.viewh), RSRBUF_NAME_VIEW);
                    rs_viewh_done(cd, tdd->tdd_.insview.viewh);
                    break;
                case TRDD_DELVIEW:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELVIEW\n"));
                    rs_viewh_done(cd, tdd->tdd_.delview.viewh);
                    break;
                case TRDD_CHANGEREL:
                    ss_dprintf_4(("trdd_donetdd:TRDD_CHANGEREL\n"));
                    rs_relh_setaborted(cd, tdd->tdd_.changerel.relh);
                    rs_rbuf_relhunbuffer(
                        cd,
                        rbuf,
                        rs_relh_entname(cd, tdd->tdd_.changerel.relh));
                    SS_MEM_SETUNLINK(tdd->tdd_.changerel.relh);
                    rs_relh_done(cd, tdd->tdd_.changerel.relh);
                    break;
#ifndef SS_NOSEQUENCE
                case TRDD_INSSEQ:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSSEQ\n"));
                    rs_rbuf_removename(cd, rbuf, tdd->tdd_.insseq.seqname, RSRBUF_NAME_SEQUENCE);
                    rs_entname_done(tdd->tdd_.insseq.seqname);
                    break;
                case TRDD_DELSEQ:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELSEQ\n"));
                    dbe_seq_unmarkdropped(
                        dbe_db_getseq(trdd->trdd_db),
                        tdd->tdd_.delseq.seqid);
                    rs_entname_done(tdd->tdd_.delseq.seqname);
                    break;
#endif /* SS_NOSEQUENCE */
                case TRDD_INSNAME:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSNAME\n"));
                    rs_rbuf_removename(cd, rbuf, tdd->tdd_.insname.name, RSRBUF_NAME_GENERIC);
                    rs_entname_done(tdd->tdd_.insname.name);
                    break;
                case TRDD_DELNAME:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELNAME\n"));
                    rs_entname_done(tdd->tdd_.delname.name);
                    break;
                case TRDD_INSEVENT:
                    ss_dprintf_4(("trdd_donetdd:TRDD_INSEVENT\n"));
                    rs_rbuf_event_remove(
                        cd,
                        rbuf,
                        rs_event_entname(cd, tdd->tdd_.insevent.event));
                    rs_event_done(cd, tdd->tdd_.insevent.event);
                    break;
                case TRDD_DELEVENT:
                    ss_dprintf_4(("trdd_donetdd:TRDD_DELEVENT\n"));
                    rs_rbuf_event_add(cd, rbuf, tdd->tdd_.delevent.event);
                    rs_event_done(cd, tdd->tdd_.delevent.event);
                    break;
                default:
                    ss_error;
            }
        }
        ss_dassert(succp);

        SsMemFree(tdd);

        return(succp);
}

/*#***********************************************************************\
 *
 *		trdd_cleanup
 *
 * Clears in-memory stuff from trdd.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	trxsuccp -
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
static dbe_ret_t trdd_cleanup(
        dbe_trdd_t* trdd,
        bool trxsuccp)
{
        dbe_ret_t rc;
        trdd_dd_t* tdd;
        su_list_node_t* n;
        rs_sysi_t* cd;
        rs_rbuf_t* rbuf;
        bool succp;

        ss_dprintf_3(("trdd_cleanup\n"));
        CHK_TRDD(trdd);

        if (trdd->trdd_cleanupdone) {
            return(DBE_RC_SUCC);
        }

        trdd->trdd_cleanupdone = TRUE;

        rc = DBE_RC_SUCC;
        cd = trdd->trdd_cd;
        rbuf = trdd->trdd_rbuf;
        succp = TRUE;

        su_list_do_get(trdd->trdd_ddlist, n, tdd) {
            succp = trdd_donetdd(cd, trdd, tdd, rbuf, trxsuccp);
            if (!succp && rc == DBE_RC_SUCC) {
                rc = DBE_ERR_FAILED;
            }
        }

        dbe_db_addlogwrites(trdd->trdd_db, trdd->trdd_nlogwrites);

        su_list_clear(trdd->trdd_ddlist);

        ss_dprintf_4(("trdd_cleanup:end, rc = %d (%s)\n", rc, su_rc_nameof(rc)));

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_done
 *
 * Releases memory allocated for dd-list.
 *
 * Parameters :
 *
 *	trdd - in, take
 *		Transaction dd handle.
 *
 *      trxsuccp - in
 *
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
dbe_ret_t dbe_trdd_done(
        dbe_trdd_t* trdd,
        bool trxsuccp)
{
        dbe_ret_t rc;
        rs_sysi_t* cd;

        ss_dprintf_1(("dbe_trdd_done\n"));
        CHK_TRDD(trdd);

        cd = trdd->trdd_cd;

        if (trdd->trdd_mergedisabled) {
            dbe_db_setmergedisabled(trdd->trdd_db, FALSE);
        }

        ss_dassert(trdd->trdd_cleanupdone);
        rc = trdd_cleanup(trdd, trxsuccp);

        if (trdd->trdd_ddchanged) {
            ss_dprintf_2(("dbe_trdd_done:RS_SIGNAL_DDCHANGE\n"));
            rs_sysi_signal(cd, RS_SIGNAL_DDCHANGE);
        } else {
            ss_dprintf_2(("dbe_trdd_done:RS_SIGNAL_FLUSHSQLCACHE\n"));
            rs_sysi_signal(cd, RS_SIGNAL_FLUSHSQLCACHE);
        }

        su_list_done(trdd->trdd_ddlist);

        if (trdd->trdd_deferredblobunlinklist != NULL) {
            ss_dassert(su_list_length(trdd->trdd_deferredblobunlinklist) == 0);
            su_list_done(trdd->trdd_deferredblobunlinklist);
        }

        SsMemFree(trdd);

        ss_dprintf_2(("dbe_trdd_done:end, rc = %d (%s)\n", rc, su_rc_nameof(rc)));

        return(rc);
}

/*##**********************************************************************\
 *
 *      dbe_trdd_cleanup
 *
 * Cleans up the trdd's operation list, but does not release the trdd itself
 * yet.  Any changed relhs are marked as aborted, and this function must be
 * called before the corresponding relh locks are released.
 *
 * Parameters:
 *      trdd - in out, use
 *          The trdd.
 *
 *      trxsuccp - use
 *          Tells if the transaction was succesfully completed.
 *
 * Return value:
 *       Operation status code.
 *
 * Limitations:
 *
 * Globals used:
 */
dbe_ret_t dbe_trdd_cleanup(
        dbe_trdd_t*         trdd,
        bool                trxsuccp)
{
        ss_dprintf_1(("dbe_trdd_cleanup\n"));
        CHK_TRDD(trdd);

        return trdd_cleanup(trdd, trxsuccp);
}

/*##**********************************************************************\
 * 
 *		dbe_trdd_unlinkblobs
 * 
 * Unlinks blobs added to unlinklist.
 * 
 * Parameters : 
 * 
 *		trdd - 
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
void dbe_trdd_unlinkblobs(
        dbe_trdd_t* trdd)
{
        rs_sysi_t* cd;

        ss_dprintf_1(("dbe_trdd_unlinkblobs\n"));
        CHK_TRDD(trdd);

        if (trdd->trdd_deferredblobunlinklist == NULL) {
            return;
        }
        if (su_list_length(trdd->trdd_deferredblobunlinklist) == 0) {
            return;
        }

        cd = trdd->trdd_cd;

        dbe_indmerge_unlinkblobs(cd, trdd->trdd_deferredblobunlinklist);
        rs_sysi_setdeferredblobunlinklist(cd, NULL);

        /* Create a new list because dbe_indmerge_unlinkblobs releases it. */
        trdd->trdd_deferredblobunlinklist = su_list_init(NULL);

        SS_RTCOVERAGE_INC(SS_RTCOV_TRUNCATE_BLOBS);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_startcommit
 *
 * Starting to commit dd operations. Called outside trx mutex and action
 * gate.
 *
 * Parameters :
 *
 *	trdd -
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
void dbe_trdd_startcommit(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trdd_t* trdd)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;

        ss_dprintf_1(("dbe_trdd_startcommit\n"));
        CHK_TRDD(trdd);

        su_list_do_get(trdd->trdd_ddlist, n, tdd) {
            SS_PMON_ADD(SS_PMON_DBEDDOP);
            switch (tdd->tdd_type) {
                case TRDD_TRUNCATEREL:
                    if (tdd->tdd_physdelete && !trdd->trdd_mergedisabled) {
                        /* Need to stop merge during truncate. */
                        dbe_db_setmergedisabled(trdd->trdd_db, TRUE);
                        trdd->trdd_mergedisabled = TRUE;
                    }
                    break;
                case TRDD_DELREL:
                    if (dbe_tuple_isnocheck(cd, trx, tdd->tdd_.delrel.relh) 
                         && !trdd->trdd_mergedisabled) 
                    {
                        /* Need to stop merge during nocheck drop. */
                        dbe_db_setmergedisabled(trdd->trdd_db, TRUE);
                        trdd->trdd_mergedisabled = TRUE;
                    }
                    
                    break;
                case TRDD_DELKEY:
                    if (dbe_tuple_isnocheck(cd, trx, tdd->tdd_.delkey.relh) 
                        && !trdd->trdd_mergedisabled) 
                    {
                        /* Need to stop merge during nocheck drop. */
                        dbe_db_setmergedisabled(trdd->trdd_db, TRUE);
                        trdd->trdd_mergedisabled = TRUE;
                    }
                    break;    
                default:
                    break;
            }
        }
}

/*##**********************************************************************\
 *
 *		dbe_trdd_getnindexwrites
 *
 *
 *
 * Parameters :
 *
 *	trdd -
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
long dbe_trdd_getnindexwrites(
        dbe_trdd_t* trdd)
{
        ss_dprintf_1(("dbe_trdd_getnindexwrites\n"));
        CHK_TRDD(trdd);

        return(trdd->trdd_nindexwrites);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_stmt_begin
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	stmttrxid -
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
void dbe_trdd_stmt_begin(
        dbe_trdd_t* trdd,
        dbe_trxid_t stmttrxid)
{
        CHK_TRDD(trdd);
        ss_dprintf_1(("dbe_trdd_stmt_begin\n"));

        trdd->trdd_stmttrxid = stmttrxid;
}

/*##**********************************************************************\
 *
 *		dbe_trdd_stmt_commit
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	stmttrxid -
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
void dbe_trdd_stmt_commit(
        dbe_trdd_t* trdd,
        dbe_trxid_t stmttrxid)
{
        CHK_TRDD(trdd);
        ss_dassert(!DBE_TRXID_EQUAL(trdd->trdd_stmttrxid, DBE_TRXID_NULL));
        SS_NOTUSED(stmttrxid);
        ss_dprintf_1(("dbe_trdd_stmt_commit\n"));

        trdd->trdd_stmttrxid = DBE_TRXID_NULL;
}

/*##**********************************************************************\
 *
 *		dbe_trdd_stmt_rollback
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	stmttrxid -
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
void dbe_trdd_stmt_rollback(
        dbe_trdd_t* trdd,
        dbe_trxid_t stmttrxid)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;
        void* cd;
        rs_rbuf_t* rbuf;
        bool succp;

        CHK_TRDD(trdd);
        /* ss_dassert(trdd->trdd_stmttrxid != DBE_TRXID_NULL); */
        ss_dprintf_1(("dbe_trdd_stmt_rollback\n"));

        cd = trdd->trdd_cd;
        rbuf = trdd->trdd_rbuf;
        succp = TRUE;

        n = su_list_first(trdd->trdd_ddlist);
        while (n != NULL) {
            su_list_node_t* nextn;

            nextn = su_list_next(trdd->trdd_ddlist, n);
            tdd = su_listnode_getdata(n);
            if (DBE_TRXID_EQUAL(tdd->tdd_stmttrxid, stmttrxid)) {
                succp = trdd_donetdd(cd, trdd, tdd, rbuf, FALSE);
                ss_dassert(succp);
                su_list_remove(trdd->trdd_ddlist, n);
            }
            n = nextn;
        }

        trdd->trdd_stmttrxid = DBE_TRXID_NULL;
}

/*#***********************************************************************\
 *
 *		trdd_delrel_advance
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	tdd -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trdd_delrel_advance(
        dbe_trdd_t* trdd,
        trdd_dd_t* tdd,
        bool truncate)
{
        dbe_ret_t rc;
        void* cd;
        trdd_vldinfo_t* vi;

        ss_dprintf_3(("trdd_delrel_advance:%s\n", tdd->tdd_type == TRDD_DELREL ? "DELREL" : "TRUNCATEREL"));
        ss_dassert((tdd->tdd_type == TRDD_DELREL && !truncate) 
                   || (tdd->tdd_type == TRDD_TRUNCATEREL && truncate));

        vi = &trdd->trdd_vldinfo;
        cd = trdd->trdd_cd;

        if (tdd->tdd_logonly) {
            ss_dassert(!truncate);
            goto logging;
        }
        if (!vi->vi_initp) {

            uint i;
            su_pa_t* keys;
            rs_key_t* key;

            if (dbe_trxbuf_gettrxstate(dbe_db_gettrxbuf(trdd->trdd_db), tdd->tdd_stmttrxid, NULL, NULL)
                == DBE_TRXST_ABORT)
            {
                ss_dprintf_4(("trdd_delrel_advance:stmt aborted\n"));
                return(DBE_RC_END);
            }
            vi->vi_initp = TRUE;

            dbe_db_abortsearchesrelid(
                trdd->trdd_db,
                rs_relh_relid(cd, tdd->tdd_.delrel.relh));

            vi->vi_keylist = su_list_init(NULL);
            keys = rs_relh_keys(cd, tdd->tdd_.delrel.relh);
            /* MME requires that the clustering key is dropped last.
               This behaviour does not hurt non-MME operations, so let's
               just do it like this always. */
            su_pa_do_get(keys, i, key) {

                if (!rs_key_isclustering(cd, key)
                    && !rs_key_isprimary(cd, key)) {
                    su_list_insertlast(
                            vi->vi_keylist,
                            key);
                }
            }
            key = rs_relh_clusterkey(cd, tdd->tdd_.delrel.relh);

#ifdef SS_DEBUG
            if (key == NULL) {
                ss_assert(rs_relh_isaborted(cd, tdd->tdd_.delrel.relh) == TRUE);
            }
#endif

            ss_assert(key != NULL);
            su_list_insertlast(vi->vi_keylist, key);
            vi->vi_klnode = su_list_first(vi->vi_keylist);
            vi->vi_di = NULL;

        } else if (vi->vi_klnode == NULL) {

    logging:;
            rc = DBE_RC_END;
#ifdef DBE_DDRECOV_BUGFIX
#ifndef SS_NOLOGGING
            if (trdd->trdd_log != NULL) {
                void* cd;
                cd = trdd->trdd_cd;
                if (truncate) {
                    int i;
                    int nindex;
                    nindex = su_list_length(vi->vi_keylist);
                    nindex++; /* Add relid. */
                    for (i = 0; i < nindex; i++) {
                        dbe_log_putincsysctr(trdd->trdd_log, DBE_LOGREC_INCSYSCTR, DBE_CTR_KEYID);
                    }
                }

                rc = dbe_log_putdroptable(
                        trdd->trdd_log,
                        cd,
                        truncate
                            ? DBE_LOGREC_TRUNCATETABLE
                            : DBE_LOGREC_DROPTABLE,
                        tdd->tdd_stmttrxid,
                        rs_relh_relid(cd, tdd->tdd_.delrel.relh),
                        rs_entname_getname(
                            rs_relh_entname(cd, tdd->tdd_.delrel.relh)));
                if (rc == DBE_RC_SUCC && tdd->tdd_logstmtcommit) {
                    rc = dbe_log_putstmtmark(
                                trdd->trdd_log,
                                cd,
                                DBE_LOGREC_COMMITSTMT,
                                trdd->trdd_usertrxid,
                                tdd->tdd_stmttrxid);
                }
                if (rc == DBE_RC_SUCC) {
                    rc = DBE_RC_END;
                }
            }
#endif /* SS_NOLOGGING */
#endif /* DBE_DDRECOV_BUGFIX */
            if (!tdd->tdd_logonly) {
                su_list_done(vi->vi_keylist);
                vi->vi_initp = FALSE;
            }
            return(rc);

        } else {

            if (vi->vi_di == NULL) {
                if (truncate && trdd->trdd_deferredblobunlinklist == NULL) {
                    trdd->trdd_deferredblobunlinklist = su_list_init(NULL);
                }
                vi->vi_di = dbe_tuple_dropindex_init(
                                trdd->trdd_trx,
                                tdd->tdd_.delrel.relh,
                                su_listnode_getdata(vi->vi_klnode),
                                truncate,
                                trdd->trdd_deferredblobunlinklist);
            }

            rc = dbe_tuple_dropindex_advance(vi->vi_di);

            trdd->trdd_nindexwrites++;
            trdd->trdd_nlogwrites++;

            switch (rc) {
                case DBE_RC_CONT:
                    break;
                case DBE_RC_END:
                    dbe_tuple_dropindex_done(vi->vi_di);
                    vi->vi_di = NULL;
                    vi->vi_klnode = su_list_next(
                                        vi->vi_keylist,
                                        vi->vi_klnode);
                    break;
                default:
                    su_rc_error(rc);
            }
        }
        return(DBE_RC_CONT);
}

/*#***********************************************************************\
 *
 *		trdd_inskey_advance
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	tdd -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trdd_inskey_advance(
        dbe_trdd_t* trdd,
        trdd_dd_t* tdd,
        bool* p_ddopisactive)
{
        dbe_ret_t rc;
        trdd_vldinfo_t* vi;

        ss_dprintf_3(("trdd_inskey_advance\n"));
        ss_dassert(tdd->tdd_type == TRDD_INSKEY);

        vi = &trdd->trdd_vldinfo;

        if (!vi->vi_initp) {
            if (dbe_trxbuf_gettrxstate(dbe_db_gettrxbuf(trdd->trdd_db), tdd->tdd_stmttrxid, NULL, NULL)
                == DBE_TRXST_ABORT)
            {
                ss_dprintf_4(("trdd_inskey_advance:stmt aborted\n"));
                return(DBE_RC_END);
            }

            if (!tdd->tdd_.inskey.createkey) {
                rs_key_setindex_ready(trdd->trdd_cd, tdd->tdd_.inskey.key);
                if (trdd->trdd_log != NULL && tdd->tdd_logstmtcommit) {
                    trdd->trdd_nlogwrites++;
                    rc = dbe_log_putstmtmark(
                                trdd->trdd_log,
                                trdd->trdd_cd,
                                DBE_LOGREC_COMMITSTMT,
                                trdd->trdd_usertrxid,
                                tdd->tdd_stmttrxid);
                    if (rc == DBE_RC_SUCC) {
                        rc = DBE_RC_END;
                    }
                } else {
                    rc = DBE_RC_END;
                }
                return(rc);
            }
            ss_dassert(tdd->tdd_.inskey.createkey == TRUE);

            vi->vi_initp = TRUE;

            dbe_db_setddopactive(trdd->trdd_db, TRUE);
            *p_ddopisactive = TRUE;

            vi->vi_ci = dbe_tuple_createindex_init(
                            trdd->trdd_trx,
                            tdd->tdd_.inskey.relh,
                            tdd->tdd_.inskey.key);
            rc = DBE_RC_CONT;

        } else {
            ss_dassert(tdd->tdd_.inskey.createkey == TRUE);

            rc = dbe_tuple_createindex_advance(vi->vi_ci);

            trdd->trdd_nindexwrites++;
            trdd->trdd_nlogwrites++;

            if (rc != DBE_RC_CONT) {
                void* cd;
                cd = trdd->trdd_cd;

                dbe_tuple_createindex_done(vi->vi_ci);
                rs_key_setindex_ready(cd, tdd->tdd_.inskey.key);
#ifdef DBE_DDRECOV_BUGFIX
#ifndef SS_NOLOGGING
                if (trdd->trdd_log != NULL && rc == DBE_RC_END) {
                    rc = dbe_log_putcreateordropidx(
                            trdd->trdd_log,
                            cd,
                            DBE_LOGREC_CREATEINDEX,
                            tdd->tdd_stmttrxid,
                            rs_relh_relid(cd, tdd->tdd_.inskey.relh),
                            rs_key_id(cd, tdd->tdd_.inskey.key),
                            rs_entname_getname(
                                rs_relh_entname(cd, tdd->tdd_.inskey.relh)));
                    if (rc == DBE_RC_SUCC && tdd->tdd_logstmtcommit) {
                        rc = dbe_log_putstmtmark(
                                    trdd->trdd_log,
                                    cd,
                                    DBE_LOGREC_COMMITSTMT,
                                    trdd->trdd_usertrxid,
                                    tdd->tdd_stmttrxid);
                    }
                    if (rc == DBE_RC_SUCC) {
                        rc = DBE_RC_END;
                    }
                }
#endif /* SS_NOLOGGING */
#endif /* DBE_DDRECOV_BUGFIX */
                FAKE_CODE_BLOCK(FAKE_DBE_SLEEP_WHILE_DDOPACTIVE, {
                    ss_dprintf_3(("trdd_inskey_advance:FAKE_DBE_SLEEP_WHILE_DDOPACTIVE\n"));
                    SsThrSleep(7000);
                    ss_dprintf_3(("trdd_inskey_advance:FAKE_DBE_SLEEP_WHILE_DDOPACTIVE:sleep done\n"));
                });
                dbe_db_setddopactive(trdd->trdd_db, FALSE);

                *p_ddopisactive = FALSE;
                vi->vi_initp = FALSE;
            }
        }
        return(rc);
}

/*#***********************************************************************\
 *
 *		trdd_delkey_advance
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	tdd -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trdd_delkey_advance(
        dbe_trdd_t* trdd,
        trdd_dd_t* tdd)
{
        dbe_ret_t rc;
        void* cd;
        trdd_vldinfo_t* vi;

        ss_dprintf_3(("trdd_delkey_advance\n"));
        ss_dassert(tdd->tdd_type == TRDD_DELKEY);

        vi = &trdd->trdd_vldinfo;
        cd = trdd->trdd_cd;

        if (!vi->vi_initp) {

            if (dbe_trxbuf_gettrxstate(dbe_db_gettrxbuf(trdd->trdd_db), tdd->tdd_stmttrxid, NULL, NULL)
                == DBE_TRXST_ABORT)
            {
                ss_dprintf_4(("trdd_delkey_advance:stmt aborted\n"));
                return(DBE_RC_END);
            }

            vi->vi_initp = TRUE;
            rs_key_setaborted(cd, tdd->tdd_.delkey.key);
            dbe_db_abortsearcheskeyid(
                trdd->trdd_db,
                rs_key_id(cd, tdd->tdd_.delkey.key));

            vi->vi_di = dbe_tuple_dropindex_init(
                            trdd->trdd_trx,
                            tdd->tdd_.delkey.relh,
                            tdd->tdd_.delkey.key,
                            FALSE,
                            NULL);
        } else {

            rc = dbe_tuple_dropindex_advance(vi->vi_di);

            trdd->trdd_nindexwrites++;
            trdd->trdd_nlogwrites++;

            switch (rc) {
                case DBE_RC_CONT:
                    break;
                case DBE_RC_END:
                    dbe_tuple_dropindex_done(vi->vi_di);
#ifdef DBE_DDRECOV_BUGFIX
#ifndef SS_NOLOGGING
                    if (trdd->trdd_log != NULL) {
                        void* cd;
                        cd = trdd->trdd_cd;
                        rc = dbe_log_putcreateordropidx(
                                trdd->trdd_log,
                                cd,
                                DBE_LOGREC_DROPINDEX,
                                tdd->tdd_stmttrxid,
                                rs_relh_relid(cd, tdd->tdd_.delkey.relh),
                                rs_key_id(cd, tdd->tdd_.delkey.key),
                                rs_entname_getname(
                                    rs_relh_entname(cd, tdd->tdd_.delkey.relh)));
                        if (rc == DBE_RC_SUCC && tdd->tdd_logstmtcommit) {
                            rc = dbe_log_putstmtmark(
                                        trdd->trdd_log,
                                        cd,
                                        DBE_LOGREC_COMMITSTMT,
                                        trdd->trdd_usertrxid,
                                        tdd->tdd_stmttrxid);
                        }
                        if (rc == DBE_RC_SUCC) {
                            rc = DBE_RC_END;
                        }
                    }
#endif /* SS_NOLOGGING */
#endif /* DBE_DDRECOV_BUGFIX */
                    vi->vi_initp = FALSE;
                    return(rc);
                default:
                    su_rc_error(rc);
            }
        }
        return(DBE_RC_CONT);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_commit_advance
 *
 * Advances data dictionary operations commit. In practice this means
 * that key values are inserted to or deleted from index.
 *
 * Parameters :
 *
 *	trdd - in, use
 *		Transaction handle.
 *
 *	p_ddopisactive - out
 *		The current data dictionary status as reported to db object
 *          is stored to *p_ddopisactive when it is changed.
 *
 * Return value :
 *
 *      DBE_RC_CONT
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trdd_commit_advance(dbe_trdd_t* trdd, bool* p_ddopisactive)
{
        dbe_ret_t rc;
        trdd_dd_t* tdd;

        ss_dprintf_1(("dbe_trdd_commit_advance:begin\n"));
        CHK_TRDD(trdd);

        if (!trdd->trdd_initcommit) {
            bool foundp;
            do {
                trdd_dd_t* tdd;
                su_list_node_t* n1;
                su_list_node_t* n2;

                foundp = FALSE;
                su_list_do_get(trdd->trdd_ddlist, n1, tdd) {
                    if (tdd->tdd_type == TRDD_INSNAME) {
                        n2 = trdd_ddfind_ex(trdd, tdd->tdd_.insname.name, TRDD_DELNAME, n1);
                        if (n2 != NULL) {
                            tdd = su_listnode_getdata(n2);
                            rs_entname_done(tdd->tdd_.delname.name);
                            SsMemFree(tdd);
                            su_list_remove(trdd->trdd_ddlist, n2);
                            foundp = TRUE;
                            break;
                        }
                    }
                    if (tdd->tdd_type == TRDD_DELNAME) {
                        n2 = trdd_ddfind_ex(trdd, tdd->tdd_.delname.name, TRDD_INSNAME, n1);
                        if (n2 != NULL) {
                            tdd = su_listnode_getdata(n2);
                            rs_entname_done(tdd->tdd_.insname.name);
                            SsMemFree(tdd);
                            su_list_remove(trdd->trdd_ddlist, n2);
                            foundp = TRUE;
                            break;
                        }
                    }
                }
            } while(foundp);

            trdd->trdd_initcommit = TRUE;
            trdd->trdd_node = su_list_first(trdd->trdd_ddlist);
        }

        if (trdd->trdd_node == NULL) {

            /* End of dd validation. */
            rc = DBE_RC_SUCC;

        } else {

            bool move_next;

            rc = DBE_RC_CONT;
            move_next = TRUE;
            tdd = su_listnode_getdata(trdd->trdd_node);

            ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

            switch (tdd->tdd_type) {
                case TRDD_INSREL:
                    break;
                case TRDD_DELREL:
                    /* Delete all key values. */
                    rc = trdd_delrel_advance(trdd, tdd, FALSE);
                    switch (rc) {
                        case DBE_RC_CONT:
                            move_next = FALSE;
                            break;
                        case DBE_RC_END:
                            move_next = TRUE;
                            rc = DBE_RC_CONT;
                            break;
                        default:
                            move_next = FALSE;
                            break;
                    }
                    break;
                case TRDD_TRUNCATEREL:
                    if (tdd->tdd_physdelete) {
                        /* Delete all key values. */
                        rc = trdd_delrel_advance(trdd, tdd, TRUE);
                        switch (rc) {
                            case DBE_RC_CONT:
                                move_next = FALSE;
                                break;
                            case DBE_RC_END:
                                move_next = TRUE;
                                rc = DBE_RC_CONT;
                                break;
                            default:
                                move_next = FALSE;
                                break;
                        }
                    } else {
                        if (trdd->trdd_log != NULL) {
                            rc = dbe_log_putdroptable(
                                    trdd->trdd_log,
                                    trdd->trdd_cd,
                                    DBE_LOGREC_TRUNCATECARDIN,
                                    tdd->tdd_stmttrxid,
                                    rs_relh_relid(trdd->trdd_cd, tdd->tdd_.delrel.relh),
                                    rs_entname_getname(
                                        rs_relh_entname(trdd->trdd_cd, tdd->tdd_.delrel.relh)));
                            if (rc == DBE_RC_SUCC && tdd->tdd_logstmtcommit) {
                                rc = dbe_log_putstmtmark(
                                        trdd->trdd_log,
                                        trdd->trdd_cd,
                                        DBE_LOGREC_COMMITSTMT,
                                        trdd->trdd_usertrxid,
                                        tdd->tdd_stmttrxid);
                            }
                            if (rc == DBE_RC_SUCC) {
                                rc = DBE_RC_CONT;
                            }
                        }
                    }
                    break;
                case TRDD_ALTERREL:
                case TRDD_RENAMEREL:
                    /* Nothing to do here, rbuf updated in freemem.
                     */
                    break;
                case TRDD_INSKEY:
                    /* Insert new key values. */
                    rc = trdd_inskey_advance(trdd, tdd, p_ddopisactive);
                    switch (rc) {
                        case DBE_RC_CONT:
                            move_next = FALSE;
                            break;
                        case DBE_RC_END:
                            move_next = TRUE;
                            rc = DBE_RC_CONT;
                            break;
                        default:
                            move_next = FALSE;
                            break;
                    }
                    break;
                case TRDD_DELKEY:
                    /* Delete key values. */
                    rc = trdd_delkey_advance(trdd, tdd);
                    switch (rc) {
                        case DBE_RC_CONT:
                            move_next = FALSE;
                            break;
                        case DBE_RC_END:
                            move_next = TRUE;
                            rc = DBE_RC_CONT;
                            break;
                        default:
                            move_next = FALSE;
                            break;
                    }
                    break;
                case TRDD_INSVIEW:
                    break;
                case TRDD_DELVIEW:
                    break;
                case TRDD_CHANGEREL:
                    /* Nothing to do here, rbuf updated in freemem.
                     */
                    break;
#ifndef SS_NOSEQUENCE
                case TRDD_INSSEQ:
                    if (trdd->trdd_log != NULL) {
                        rc = dbe_log_putcreatectrorseq(
                                trdd->trdd_log,
                                trdd->trdd_cd,
                                tdd->tdd_.insseq.densep
                                    ? DBE_LOGREC_CREATESEQ
                                    : DBE_LOGREC_CREATECTR,
                                tdd->tdd_stmttrxid,
                                tdd->tdd_.insseq.seqid,
                                rs_entname_getname(tdd->tdd_.insseq.seqname));
                        if (rc == DBE_RC_SUCC) {
                            rc = DBE_RC_CONT;
                        }
                        FAKE_CODE_BLOCK_GT(FAKE_DBE_SEQUENCE_RECOVERY_BUG, 1,
                        {
                            SsPrintf("FAKE_DBE_SEQUENCE_RECOVERY_BUG:dbe_trdd_commit_advance\n");
                            FAKE_SET(FAKE_DBE_SEQUENCE_RECOVERY_BUG, 3);
                        }
                        );
                    }
                    dbe_seq_create(
                        dbe_db_getseq(trdd->trdd_db),
                        tdd->tdd_.insseq.seqid,
                        NULL);
                    break;
                case TRDD_DELSEQ:
                    if (trdd->trdd_log != NULL) {
                        rc = dbe_log_putdropctrorseq(
                                trdd->trdd_log,
                                trdd->trdd_cd,
                                tdd->tdd_.delseq.densep
                                    ? DBE_LOGREC_DROPSEQ
                                    : DBE_LOGREC_DROPCTR,
                                tdd->tdd_stmttrxid,
                                tdd->tdd_.delseq.seqid,
                                rs_entname_getname(tdd->tdd_.delseq.seqname));
                        if (rc == DBE_RC_SUCC) {
                            rc = DBE_RC_CONT;
                        }
                    }
                    dbe_seq_drop(
                        dbe_db_getseq(trdd->trdd_db),
                        tdd->tdd_.delseq.seqid,
                        NULL);
                    break;
#endif /* SS_NOSEQUENCE */
                case TRDD_INSNAME:
                case TRDD_DELNAME:
                case TRDD_INSEVENT:
                case TRDD_DELEVENT:
                    break;
                default:
                    ss_error;
            }

            if (move_next) {
                trdd->trdd_node = su_list_next(trdd->trdd_ddlist, trdd->trdd_node);
            }
        }

        ss_dprintf_2(("dbe_trdd_commit_advance:end, rc = %d\n", rc));
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_rollback
 *
 * Does rollback to the trdd object. Function dbe_trdd_done must still be
 * called.
 *
 * Parameters :
 *
 *	trdd -
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
dbe_ret_t dbe_trdd_rollback(dbe_trdd_t* trdd)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_trdd_rollback\n"));
        CHK_TRDD(trdd);

        rc = trdd_cleanup(trdd, FALSE);

        return(rc);
}

/*#***********************************************************************\
 *
 *		trdd_clearlogstmtcommit
 *
 * Clears statement commit logging flag if there are other statements with
 * same trxid and logging flag set. This should ensure we log the statement
 * commit only once.
 *
 * Parameters :
 *
 *		trdd -
 *
 *
 *		stmttrxid -
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
static void trdd_clearlogstmtcommit(
        dbe_trdd_t* trdd,
        dbe_trxid_t stmttrxid)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;

        su_list_do_get(trdd->trdd_ddlist, n, tdd) {
            if (DBE_TRXID_EQUAL(tdd->tdd_stmttrxid, stmttrxid)) {
                tdd->tdd_logstmtcommit = FALSE;
            }
        }
}

/*#***********************************************************************\
 *
 *		trdd_ddfind_ex
 *
 *
 *
 * Parameters :
 *
 *	trdd - in, use
 *
 *
 *	name - in, use
 *
 *
 *	ddtype - in
 *
 *
 * Return value :
 *
 *      pointer to the list node with name, or
 *      NULL
 *
 * Limitations  :
 *
 * Globals used :
 */
static su_list_node_t* trdd_ddfind_ex(
        dbe_trdd_t* trdd,
        rs_entname_t* name,
        trdd_ddtype_t ddtype,
        su_list_node_t* stop_node)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;
        rs_relh_t* relh;
        rs_viewh_t* viewh;
        rs_key_t* key = NULL;
        void* cd;
        rs_entname_t en;
        rs_entname_t en2;

        cd = trdd->trdd_cd;

        if (ddtype != TRDD_INSKEY) {
            if (rs_entname_getcatalog(name) == NULL) {
                rs_auth_t* auth;
                auth = rs_sysi_auth(cd);
                rs_entname_initbuf(&en2,
                                   rs_auth_catalog(cd, auth),
                                   rs_entname_getschema(name),
                                   rs_entname_getname(name));
                name = &en2;
            }
            if (rs_entname_getschema(name) == NULL) {
                rs_auth_t* auth;
                auth = rs_sysi_auth(cd);
                rs_entname_initbuf(
                        &en,
                        rs_entname_getcatalog(name),
                        rs_auth_schema(cd, auth),
                        rs_entname_getname(name));
                name = &en;
            }
        }

        su_list_do_get(trdd->trdd_ddlist, n, tdd) {
            if (n == stop_node) {
                break;
            }
            if (ddtype == tdd->tdd_type) {
                switch (tdd->tdd_type) {
                    case TRDD_INSREL:
                        relh = tdd->tdd_.insrel.relh;
                        if (rs_entname_compare(
                                rs_relh_entname(cd, relh), name) == 0) {
                            return(n);
                        }
                        break;
                    case TRDD_DELREL:
                    case TRDD_TRUNCATEREL:
                        relh = tdd->tdd_.delrel.relh;
                        if (rs_entname_compare(
                                rs_relh_entname(cd, relh), name) == 0) {
                            return(n);
                        }
                        break;
                    case TRDD_INSKEY:
                        key = tdd->tdd_.inskey.key;
                        {
                            rs_entname_t en;

                            rs_entname_initbuf(
                                    &en,
                                    rs_relh_catalog(cd, tdd->tdd_.inskey.relh),
                                    rs_relh_schema(cd, tdd->tdd_.inskey.relh),
                                    rs_key_name(cd, key));
                            if (rs_entname_compare(&en, name) == 0) {
                                return(n);
                            }
                        }
                        break;
                    case TRDD_DELKEY:
                        key = tdd->tdd_.delkey.key;
                        {
                            rs_entname_t en;

                            rs_entname_initbuf(
                                    &en,
                                    rs_relh_catalog(cd, tdd->tdd_.delkey.relh),
                                    rs_relh_schema(cd, tdd->tdd_.delkey.relh),
                                    rs_key_name(cd, key));
                            if (rs_entname_compare(&en, name) == 0) {
                                return(n);
                            }
                        }
                        break;
                    case TRDD_INSVIEW:
                        viewh = tdd->tdd_.insview.viewh;
                        if (rs_entname_compare(
                                rs_viewh_entname(cd, viewh), name) == 0) {
                            return(n);
                        }
                        break;
                    case TRDD_DELVIEW:
                        viewh = tdd->tdd_.delview.viewh;
                        if (rs_entname_compare(
                                rs_viewh_entname(cd, viewh), name) == 0) {
                            return(n);
                        }
                        break;
                    case TRDD_INSSEQ:
                        if (rs_entname_compare(
                                tdd->tdd_.insseq.seqname, name) == 0) {
                            return(n);
                        }
                        break;
                    case TRDD_INSNAME:
                        if (rs_entname_compare(
                                tdd->tdd_.insname.name, name) == 0) {
                            return(n);
                        }
                        break;
                    case TRDD_DELNAME:
                        if (rs_entname_compare(
                                tdd->tdd_.delname.name, name) == 0) {
                            return(n);
                        }
                        break;
                    case TRDD_INSEVENT:
                        if (rs_entname_compare(
                                rs_event_entname(cd, tdd->tdd_.insevent.event),
                                name) == 0) {
                            return(n);
                        }
                        break;
                    default:
                        ss_rc_error(tdd->tdd_type);
                }
            }
        }
        return(NULL);
}

static su_list_node_t* trdd_ddfind(
        dbe_trdd_t* trdd,
        rs_entname_t* name,
        trdd_ddtype_t ddtype)
{
        return(trdd_ddfind_ex(
                trdd,
                name,
                ddtype,
                NULL));
}

/*#***********************************************************************\
 *
 *		trdd_ddfind_relh
 *
 * Find operation based on name in relh.
 *
 * Parameters :
 *
 *	trdd - in, use
 *
 *
 *	name - in, use
 *
 *
 *	ddtype - in
 *
 *
 * Return value :
 *
 *      pointer to the list node with name, or
 *      NULL
 *
 * Limitations  :
 *
 * Globals used :
 */
static su_list_node_t* trdd_ddfind_relh(
        dbe_trdd_t* trdd,
        rs_entname_t* name,
        trdd_ddtype_t ddtype)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;
        rs_relh_t* relh;
        void* cd;

        cd = trdd->trdd_cd;

        ss_dassert(rs_entname_getcatalog(name) != NULL);
        ss_dassert(rs_entname_getschema(name) != NULL);

        su_list_do_get(trdd->trdd_ddlist, n, tdd) {
            if (ddtype == tdd->tdd_type) {
                switch (tdd->tdd_type) {
                    case TRDD_INSKEY:
                        relh = tdd->tdd_.inskey.relh;
                        if (rs_entname_compare(
                                rs_relh_entname(cd, relh), name) == 0) {
                            return(n);
                        }
                        break;
                    case TRDD_DELKEY:
                        relh = tdd->tdd_.delkey.relh;
                        if (rs_entname_compare(
                                rs_relh_entname(cd, relh), name) == 0) {
                            return(n);
                        }
                        break;
                    default:
                        ss_rc_error(tdd->tdd_type);
                }
            }
        }
        return(NULL);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_relinserted
 *
 *
 *
 * Parameters :
 *
 *	trdd - in, use
 *
 *
 *	relname - in, use
 *
 *
 *	p_relh - out, ref
 *
 *
 * Return value :
 *
 *      TRUE    - is inserted
 *      FALSE   - not inserted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trdd_relinserted(
        dbe_trdd_t* trdd,
        rs_entname_t* relname,
        rs_relh_t** p_relh)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;

        CHK_TRDD(trdd);
        ss_dassert(relname != NULL);
        ss_dprintf_1(("dbe_trdd_relinserted:%s\n", rs_entname_getname(relname)));

        n = trdd_ddfind(trdd, relname, TRDD_INSREL);
        if (n != NULL) {
            if (p_relh != NULL) {
                tdd = su_listnode_getdata(n);
                *p_relh = tdd->tdd_.insrel.relh;
            }
            ss_dprintf_2(("dbe_trdd_relinserted:TRUE\n"));
            return(TRUE);
        } else {
            ss_dprintf_2(("dbe_trdd_relinserted:FALSE\n"));
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *		dbe_trdd_reldeleted
 *
 *
 *
 * Parameters :
 *
 *	trdd - in, use
 *
 *
 *	relname - in, use
 *
 *
 * Return value :
 *
 *      TRUE    - is deleted
 *      FALSE   - not deleted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trdd_reldeleted(dbe_trdd_t* trdd, rs_entname_t* relname)
{
        CHK_TRDD(trdd);
        ss_dassert(relname != NULL);
        ss_dprintf_1(("dbe_trdd_reldeleted:%s\n", rs_entname_getname(relname)));

        return(trdd_ddfind(trdd, relname, TRDD_DELREL) != NULL);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_indexinserted
 *
 *
 *
 * Parameters :
 *
 *	trdd - in, use
 *
 *
 *	indexname - in, use
 *
 *
 *	p_relh - out, ref
 *
 *
 *	p_key - out, ref
 *
 *
 * Return value :
 *
 *      TRUE    - is inserted
 *      FALSE   - not inserted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trdd_indexinserted(
        dbe_trdd_t* trdd,
        rs_entname_t* indexname,
        rs_relh_t** p_relh,
        rs_key_t** p_key)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;

        CHK_TRDD(trdd);
        ss_dassert(indexname != NULL);
        ss_dprintf_1(("dbe_trdd_indexinserted:%s\n", rs_entname_getname(indexname)));

        n = trdd_ddfind(trdd, indexname, TRDD_INSKEY);
        if (n != NULL) {
            tdd = su_listnode_getdata(n);
            if (p_relh != NULL) {
                *p_relh = tdd->tdd_.inskey.relh;
            }
            if (p_key != NULL) {
                *p_key = tdd->tdd_.inskey.key;
            }
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *		dbe_trdd_viewinserted
 *
 *
 *
 * Parameters :
 *
 *	trdd - in, use
 *
 *
 *	viewname - in, use
 *
 *
 *	p_viewh - out, ref
 *
 *
 * Return value :
 *
 *      TRUE    - is inserted
 *      FALSE   - not inserted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trdd_viewinserted(
        dbe_trdd_t* trdd,
        rs_entname_t* viewname,
        rs_viewh_t** p_viewh)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;

        CHK_TRDD(trdd);
        ss_dassert(viewname != NULL);
        ss_dprintf_1(("dbe_trdd_viewinserted:%s\n", rs_entname_getname(viewname)));

        n = trdd_ddfind(trdd, viewname, TRDD_INSVIEW);
        if (n != NULL) {
            if (p_viewh != NULL) {
                tdd = su_listnode_getdata(n);
                *p_viewh = tdd->tdd_.insview.viewh;
            }
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *		dbe_trdd_viewdeleted
 *
 *
 *
 * Parameters :
 *
 *	trdd - in, use
 *
 *
 *	viewname - in, use
 *
 *
 * Return value :
 *
 *      TRUE    - is deleted
 *      FALSE   - not deleted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trdd_viewdeleted(dbe_trdd_t* trdd, rs_entname_t* viewname)
{
        CHK_TRDD(trdd);
        ss_dassert(viewname != NULL);
        ss_dprintf_1(("dbe_trdd_viewdeleted:%s\n", rs_entname_getname(viewname)));

        return(trdd_ddfind(trdd, viewname, TRDD_DELVIEW) != NULL);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_insertrel
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh - in, use
 *
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
dbe_ret_t dbe_trdd_insertrel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        trdd_dd_t* tdd;

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trdd_insertrel:%s\n", rs_relh_name(trdd->trdd_cd, relh)));

        rs_relh_link(trdd->trdd_cd, relh);
        SS_MEM_SETLINK(relh);

        tdd = trdd_inittdd(TRDD_INSREL);

        tdd->tdd_.insrel.relh = relh;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(rc);
}

/*#***********************************************************************\
 *
 *		trdd_deletekeys
 *
 * Delete all key references to the relation specified by relh.
 * This is called when the relation is deleted.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh - in, use
 *
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
static void trdd_deletekeys(
        dbe_trdd_t* trdd,
        rs_relh_t* relh)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;
        rs_relh_t* tdd_relh;
        void* cd;
        rs_entname_t* name;
        bool removenode;

        cd = trdd->trdd_cd;
        name = rs_relh_entname(cd, relh);

        n = su_list_first(trdd->trdd_ddlist);
        while (n != NULL) {
            tdd = su_listnode_getdata(n);
            removenode = FALSE;
            switch (tdd->tdd_type) {
                case TRDD_INSKEY:
                    tdd_relh = tdd->tdd_.inskey.relh;
                    if (rs_entname_compare(
                            rs_relh_entname(cd, tdd_relh), name) == 0) {
                        rs_key_done(cd, tdd->tdd_.inskey.key);
                        SS_MEM_SETUNLINK(tdd_relh);
                        rs_relh_done(cd, tdd_relh);
                        removenode = TRUE;
                    }
                    break;
                case TRDD_DELKEY:
                    tdd_relh = tdd->tdd_.delkey.relh;
                    if (rs_entname_compare(
                            rs_relh_entname(cd, tdd_relh), name) == 0) {
                        rs_relh_insertkey(
                            cd,
                            tdd_relh,
                            tdd->tdd_.delkey.key);
                        SS_MEM_SETUNLINK(tdd->tdd_.delkey.relh);
                        rs_relh_done(cd, tdd->tdd_.delkey.relh);
                        removenode = TRUE;
                    }
                    break;
                default:
                    break;
            }
            if (removenode) {
                su_list_node_t* tmpn;
                tmpn = su_list_next(trdd->trdd_ddlist, n);
                su_list_remove(trdd->trdd_ddlist, n);
                SsMemFree(tdd);
                n = tmpn;
            } else {
                n = su_list_next(trdd->trdd_ddlist, n);
            }
        }
}

/*##**********************************************************************\
 *
 *		dbe_trdd_deleterel
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh - in, use
 *
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
dbe_ret_t dbe_trdd_deleterel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh)
{
        trdd_dd_t* tdd;
        void* cd;
        su_list_node_t* n;
        bool created_in_same_trx;
        dbe_ret_t rc;

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trdd_deleterel:%s\n", rs_relh_name(trdd->trdd_cd, relh)));

        cd = trdd->trdd_cd;

        n = trdd_ddfind(trdd, rs_relh_entname(cd, relh), TRDD_INSREL);
        created_in_same_trx = (n != NULL);

        if (dbe_cfg_physicaldroptable && !created_in_same_trx) {
            rc = trdd_truncaterel(trdd, relh, TRUE, TRUE);
            if (rc != DBE_RC_SUCC) {
                return(rc);
            }
        }

        rs_relh_link(cd, relh);
        SS_MEM_SETLINK(relh);

        if (created_in_same_trx) {
            trdd_deletekeys(trdd, relh);
            tdd = su_listnode_getdata(n);
            SS_MEM_SETUNLINK(tdd->tdd_.insrel.relh);
            rs_relh_done(cd, tdd->tdd_.insrel.relh);
            SsMemFree(tdd);
            su_list_remove(trdd->trdd_ddlist, n);
        }

        rs_relh_setddopactive(cd, relh);

        trdd_clearlogstmtcommit(trdd, trdd->trdd_stmttrxid);

        tdd = trdd_inittdd(TRDD_DELREL);

        tdd->tdd_.delrel.relh = relh;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;
        tdd->tdd_logstmtcommit = TRUE;
        tdd->tdd_logonly = created_in_same_trx;

        ss_dassert(!DBE_TRXID_ISNULL(tdd->tdd_stmttrxid));

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*#**********************************************************************\
 *
 *		trdd_truncaterel
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh - in, use
 *
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
static dbe_ret_t trdd_truncaterel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        bool physdelete,
        bool drop)
{
        trdd_dd_t* tdd;
        void* cd;
        /* su_list_node_t* n; */

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("trdd_truncaterel:%s, drop=%d\n", rs_relh_name(trdd->trdd_cd, relh), drop));

        cd = trdd->trdd_cd;

        rs_relh_setddopactive(cd, relh);

        rs_relh_link(cd, relh);
        SS_MEM_SETLINK(relh);

        tdd = trdd_inittdd(TRDD_TRUNCATEREL);

        tdd->tdd_.delrel.relh = relh;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;
        tdd->tdd_logstmtcommit = TRUE;
        tdd->tdd_logonly = FALSE;
        tdd->tdd_physdelete = physdelete;

        ss_dassert(!DBE_TRXID_ISNULL(tdd->tdd_stmttrxid));

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_truncaterel
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh - in, use
 *
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
dbe_ret_t dbe_trdd_truncaterel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        bool physdelete)
{
        /* trdd_dd_t* tdd; */
        void* cd;
        su_list_node_t* n;

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trdd_truncaterel:%s\n", rs_relh_name(trdd->trdd_cd, relh)));

        cd = trdd->trdd_cd;

        if (rs_relh_issync(cd, relh)) {
            return(DBE_ERR_FAILED);
        }
        n = trdd_ddfind(trdd, rs_relh_entname(cd, relh), TRDD_INSREL);
        if (n != NULL) {
            return(DBE_ERR_FAILED);
        }
        n = trdd_ddfind_relh(trdd, rs_relh_entname(cd, relh), TRDD_INSKEY);
        if (n != NULL) {
            return(DBE_ERR_FAILED);
        }
        n = trdd_ddfind_relh(trdd, rs_relh_entname(cd, relh), TRDD_DELKEY);
        if (n != NULL) {
            return(DBE_ERR_FAILED);
        }

        rs_relh_link(cd, relh);
        SS_MEM_SETLINK(relh);

        rs_relh_setddopactive(cd, relh);

        trdd_clearlogstmtcommit(trdd, trdd->trdd_stmttrxid);

        trdd_truncaterel(trdd, relh, physdelete, FALSE);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_alterrel
 *
 * Adds info that a relation has been altered to the trdd.
 *
 * Parameters :
 *
 *	trdd - use
 *		Transaction handle.
 *
 *	relh - in
 *		Relation handle.
 *
 *	add - in
 *		If TRUE, this is add column. Ogherwise this is drop column.
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_trdd_alterrel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        int ncolumns)
{
        trdd_dd_t* tdd;
        void* cd;

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trdd_alterrel:%s, ncolumns=%d\n", rs_relh_name(trdd->trdd_cd, relh), ncolumns));

        cd = trdd->trdd_cd;

        rs_relh_link(cd, relh);
        SS_MEM_SETLINK(relh);
        rs_relh_setddopactive(cd, relh);

        tdd = trdd_inittdd(TRDD_ALTERREL);

        tdd->tdd_.alterrel.relh = relh;
        tdd->tdd_.alterrel.ncolumns = ncolumns;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

int dbe_trdd_newcolumncount(
        dbe_trdd_t* trdd,
        rs_relh_t* relh)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;
        void* cd;
        int count = 0;

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trdd_columnaddcount:%s\n", rs_relh_name(trdd->trdd_cd, relh)));

        cd = trdd->trdd_cd;

        su_list_do_get(trdd->trdd_ddlist, n, tdd) {
            if (tdd->tdd_type == TRDD_ALTERREL
                && tdd->tdd_.alterrel.relh == relh) 
            {
                count += tdd->tdd_.alterrel.ncolumns;
            }
        }
        return(count);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_renamerel
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh -
 *
 *
 *	newname -
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
dbe_ret_t dbe_trdd_renamerel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        rs_entname_t* newname)
{
        trdd_dd_t* tdd;
        void* cd;

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trdd_renamerel:%s\n", rs_relh_name(trdd->trdd_cd, relh)));

        cd = trdd->trdd_cd;

        rs_relh_link(cd, relh);
        SS_MEM_SETLINK(relh);

        tdd = trdd_inittdd(TRDD_RENAMEREL);

        tdd->tdd_.renamerel.relh = relh;
        tdd->tdd_.renamerel.newname = rs_entname_copy(newname);
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_insertindex
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh - in, use
 *
 *
 *	key - in, use
 *
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
dbe_ret_t dbe_trdd_insertindex(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        rs_key_t* key)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        trdd_dd_t* tdd;
        su_list_node_t* n;
        rs_entname_t en;
        char* schema;
        bool setddopactive;
        bool createkey;

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dassert(key != NULL);
        ss_dprintf_1(("dbe_trdd_insertindex:%s, %s\n",
            rs_relh_name(trdd->trdd_cd, relh), rs_key_name(trdd->trdd_cd, key)));

        schema = rs_relh_schema(trdd->trdd_cd, relh);
        {
            char* catalog = rs_relh_catalog(trdd->trdd_cd, relh);
            rs_entname_initbuf(&en,
                               catalog,
                               schema,
                               rs_key_name(trdd->trdd_cd, key));
        }

        n = trdd_ddfind(trdd, rs_relh_entname(trdd->trdd_cd, relh), TRDD_TRUNCATEREL);
        if (n != NULL) {
            return(DBE_ERR_FAILED);
        }
        n = trdd_ddfind(trdd, &en, TRDD_INSKEY);
        if (n != NULL) {
            return(E_KEYNAMEEXIST_S);
        }

        setddopactive = TRUE;
        createkey = TRUE;
        n = trdd_ddfind(trdd, rs_relh_entname(trdd->trdd_cd, relh), TRDD_INSREL);
        if (n != NULL) {
            /* Inserted in this trx, check if it is empty.
             */
            ss_int8_t ntuples;
            ss_int8_t nbytes;

            dbe_trx_getrelhcardin_nomutex(
                trdd->trdd_trx,
                relh,
                &ntuples,
                &nbytes);
            if (SsInt8Is0(ntuples) && SsInt8Is0(nbytes)) {
                setddopactive = FALSE;
            }
        }
        if (!setddopactive) {
            rs_key_t* found_key;
            found_key = rs_relh_keybyname(trdd->trdd_cd, relh, &en);
            if (found_key == key) {
                /* Key is already addded to relh, 
                 * no need to add key insert. 
                 */
                createkey = FALSE;
            } else {
                setddopactive = TRUE;
            }
        }

        if (setddopactive) {
            rs_relh_setddopactive(trdd->trdd_cd, relh);
        }

        rs_relh_link(trdd->trdd_cd, relh);
        SS_MEM_SETLINK(relh);
        rs_key_link(trdd->trdd_cd, key);

        trdd_clearlogstmtcommit(trdd, trdd->trdd_stmttrxid);

        tdd = trdd_inittdd(TRDD_INSKEY);

        tdd->tdd_.inskey.relh = relh;
        tdd->tdd_.inskey.key = key;
        tdd->tdd_.inskey.createkey = createkey;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;
        tdd->tdd_logstmtcommit = TRUE;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_deleteindex
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh - in, use
 *
 *
 *	key - in, use
 *
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
dbe_ret_t dbe_trdd_deleteindex(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        rs_key_t* key)
{
        trdd_dd_t* tdd;
        void* cd;
        su_list_node_t* n;
        rs_entname_t en;
        char* schema;

        CHK_TRDD(trdd);
        cd = trdd->trdd_cd;
        ss_dassert(relh != NULL);
        ss_dassert(key != NULL);
        ss_dprintf_1(("dbe_trdd_deleteindex:%s, %s\n",
            rs_relh_name(cd, relh), rs_key_name(cd, key)));

        schema = rs_relh_schema(cd, relh);
        {
            char* catalog = rs_relh_catalog(cd, relh);
            rs_entname_initbuf(&en,
                               catalog,
                               schema,
                               rs_key_name(cd, key));
        }

        n = trdd_ddfind(trdd, rs_relh_entname(cd, relh), TRDD_TRUNCATEREL);
        if (n != NULL) {
            return(DBE_ERR_FAILED);
        }
        n = trdd_ddfind(trdd, &en, TRDD_INSKEY);
        if (n != NULL) {
            tdd = su_listnode_getdata(n);
            if (tdd->tdd_.inskey.key == key) {
                rs_relh_setddopactive(cd, relh);
                SS_MEM_SETUNLINK(tdd->tdd_.inskey.relh);
                rs_relh_done(cd, tdd->tdd_.inskey.relh);
                rs_key_done(cd, tdd->tdd_.inskey.key);
                SsMemFree(tdd);
                su_list_remove(trdd->trdd_ddlist, n);
                return(DBE_RC_SUCC);
            } else {
                n = NULL;
            }
        }

        rs_relh_setddopactive(cd, relh);
        rs_relh_link(cd, relh);
        SS_MEM_SETLINK(relh);
        rs_key_link(cd, key);

        trdd_clearlogstmtcommit(trdd, trdd->trdd_stmttrxid);

        tdd = trdd_inittdd(TRDD_DELKEY);

        tdd->tdd_.delkey.relh = relh;
        tdd->tdd_.delkey.key = key;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;
        tdd->tdd_logstmtcommit = TRUE;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

#ifndef SS_NOSEQUENCE

/*##**********************************************************************\
 *
 *		dbe_trdd_insertseq
 *
 * Adds create sequence operation to the transaction dd operations.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	seq_id -
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
dbe_ret_t dbe_trdd_insertseq(
        dbe_trdd_t* trdd,
        rs_entname_t* seq_name,
        long seq_id,
        bool densep)
{
        trdd_dd_t* tdd;

        CHK_TRDD(trdd);
        ss_dprintf_1(("dbe_trdd_insertseq:%s\n", rs_entname_getname(seq_name)));

        tdd = trdd_inittdd(TRDD_INSSEQ);

        tdd->tdd_.insseq.seqid = seq_id;
        tdd->tdd_.insseq.seqname = rs_entname_copy(seq_name);
        tdd->tdd_.insseq.densep = densep;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_deleteseq
 *
 * Adds drop sequence operation to the transaction dd operations.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	seq_id -
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
dbe_ret_t dbe_trdd_deleteseq(
        dbe_trdd_t* trdd,
        rs_entname_t* seq_name,
        long seq_id,
        bool densep)
{
        dbe_ret_t rc;
        trdd_dd_t* tdd;
        su_list_node_t* n;

        CHK_TRDD(trdd);
        ss_dprintf_1(("dbe_trdd_deleteseq:%s\n", rs_entname_getname(seq_name)));

        n = trdd_ddfind(trdd, seq_name, TRDD_INSSEQ);
        if (n != NULL) {
            tdd = su_listnode_getdata(n);
            rs_rbuf_removename(trdd->trdd_cd, trdd->trdd_rbuf, tdd->tdd_.insseq.seqname, RSRBUF_NAME_SEQUENCE);
            rs_entname_done(tdd->tdd_.insseq.seqname);
            SsMemFree(tdd);
            su_list_remove(trdd->trdd_ddlist, n);
            return(DBE_RC_SUCC);
        }

        rc = dbe_seq_markdropped(dbe_db_getseq(trdd->trdd_db), seq_id);

        if (rc != DBE_RC_SUCC) {
            return(rc);
        }

        tdd = trdd_inittdd(TRDD_DELSEQ);

        tdd->tdd_.delseq.seqid = seq_id;
        tdd->tdd_.delseq.seqname = rs_entname_copy(seq_name);
        tdd->tdd_.delseq.densep = densep;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

#endif /* SS_NOSEQUENCE */

/*##**********************************************************************\
 *
 *		dbe_trdd_insertname
 *
 * Adds create <name> operation to the transaction dd operations.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	name -
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
dbe_ret_t dbe_trdd_insertname(
        dbe_trdd_t* trdd,
        rs_entname_t* name)
{
        trdd_dd_t* tdd;

        CHK_TRDD(trdd);
        ss_dprintf_1(("dbe_trdd_insertname:%s\n", rs_entname_getname(name)));

        tdd = trdd_inittdd(TRDD_INSNAME);

        tdd->tdd_.insname.name = rs_entname_copy(name);
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_deletename
 *
 * Adds drop <name> operation to the transaction dd operations.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	name -
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
dbe_ret_t dbe_trdd_deletename(
        dbe_trdd_t* trdd,
        rs_entname_t* name)
{
        trdd_dd_t* tdd;

        CHK_TRDD(trdd);
        ss_dprintf_1(("dbe_trdd_deletename:%s\n", rs_entname_getname(name)));

        tdd = trdd_inittdd(TRDD_DELNAME);

        tdd->tdd_.delname.name = rs_entname_copy(name);
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_namedeleted
 *
 *
 *
 * Parameters :
 *
 *	trdd - in, use
 *
 *
 *	name - in, use
 *
 *
 * Return value :
 *
 *      TRUE    - is deleted
 *      FALSE   - not deleted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trdd_namedeleted(dbe_trdd_t* trdd, rs_entname_t* name)
{
        CHK_TRDD(trdd);
        ss_dassert(name != NULL);
        ss_dprintf_1(("dbe_trdd_namedeleted:%s\n", rs_entname_getname(name)));

        return(trdd_ddfind(trdd, name, TRDD_DELNAME) != NULL &&
               trdd_ddfind(trdd, name, TRDD_INSNAME) == NULL);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_insertevent
 *
 * Adds create <event> operation to the transaction dd operations.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	name -
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
dbe_ret_t dbe_trdd_insertevent(
        dbe_trdd_t* trdd,
        rs_event_t* event)
{
        trdd_dd_t* tdd;

        CHK_TRDD(trdd);
        ss_dprintf_1(("dbe_trdd_insertevent:%s\n", rs_event_name(trdd->trdd_cd, event)));

        tdd = trdd_inittdd(TRDD_INSEVENT);

        tdd->tdd_.insevent.event = event;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        rs_event_link(trdd->trdd_cd, event);

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_deleteevent
 *
 * Adds drop <event> operation to the transaction dd operations.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	event -
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
dbe_ret_t dbe_trdd_deleteevent(
        dbe_trdd_t* trdd,
        rs_entname_t* name)
{
        trdd_dd_t* tdd;
        su_list_node_t* n;
        rs_event_t* event;

        CHK_TRDD(trdd);
        ss_dprintf_1(("dbe_trdd_deleteevent:%s\n", rs_entname_getname(name)));

        n = trdd_ddfind(
                trdd,
                name,
                TRDD_INSEVENT);
        if (n != NULL) {
            tdd = su_listnode_getdata(n);
            rs_event_done(trdd->trdd_cd, tdd->tdd_.insevent.event);
            SsMemFree(tdd);
            su_list_remove(trdd->trdd_ddlist, n);
            return(DBE_RC_SUCC);
        }

        if (!rs_rbuf_event_findref(
                trdd->trdd_cd,
                rs_sysi_rbuf(trdd->trdd_cd),
                name,
                &event)) {
            return(DBE_ERR_FAILED);
        }

        tdd = trdd_inittdd(TRDD_DELEVENT);

        tdd->tdd_.delevent.event = event;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_insertview
 *
 *
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	viewh - in, use
 *
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
dbe_ret_t dbe_trdd_insertview(
        dbe_trdd_t* trdd,
        rs_viewh_t* viewh)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        trdd_dd_t* tdd;

        CHK_TRDD(trdd);
        ss_dassert(viewh != NULL);
        ss_dprintf_1(("dbe_trdd_insertview:%s\n", rs_viewh_name(trdd->trdd_cd, viewh)));

        rs_viewh_link(trdd->trdd_cd, viewh);

        tdd = trdd_inittdd(TRDD_INSVIEW);

        tdd->tdd_.insview.viewh = viewh;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_deleteview
 *
 *
 *
 * Parameters :
 *
 *	trdd - in out, use
 *
 *
 *	viewh - in, use
 *
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
dbe_ret_t dbe_trdd_deleteview(
        dbe_trdd_t* trdd,
        rs_viewh_t* viewh)
{
        trdd_dd_t* tdd;
        void* cd;
        su_list_node_t* n;

        CHK_TRDD(trdd);
        ss_dassert(viewh != NULL);
        ss_dprintf_1(("dbe_trdd_deleteview:%s\n", rs_viewh_name(trdd->trdd_cd, viewh)));

        cd = trdd->trdd_cd;

        n = trdd_ddfind(trdd, rs_viewh_entname(cd, viewh), TRDD_INSVIEW);
        if (n != NULL) {
            tdd = su_listnode_getdata(n);
            rs_viewh_done(cd, tdd->tdd_.insview.viewh);
            SsMemFree(tdd);
            su_list_remove(trdd->trdd_ddlist, n);
            return(DBE_RC_SUCC);
        }

        rs_viewh_link(cd, viewh);

        tdd = trdd_inittdd(TRDD_DELVIEW);

        tdd->tdd_.delview.viewh = viewh;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_trdd_setrelhchanged
 *
 * Marks relation handle as changed.
 *
 * Parameters :
 *
 *	trdd -
 *
 *
 *	relh -
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
dbe_ret_t dbe_trdd_setrelhchanged(
        dbe_trdd_t* trdd,
        rs_relh_t* relh)
{
        trdd_dd_t* tdd;
        void* cd;

        CHK_TRDD(trdd);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trdd_setrelhchanged:%s\n", rs_relh_name(trdd->trdd_cd, relh)));

        cd = trdd->trdd_cd;

        rs_relh_link(cd, relh);
        SS_MEM_SETLINK(relh);
        rs_relh_setddopactive(cd, relh);

        tdd = trdd_inittdd(TRDD_CHANGEREL);

        tdd->tdd_.changerel.relh = relh;
        tdd->tdd_stmttrxid = trdd->trdd_stmttrxid;

        su_list_insertlast(trdd->trdd_ddlist, tdd);

        return(DBE_RC_SUCC);
}

void dbe_trdd_setlog(dbe_trdd_t* trdd, dbe_log_t* log)
{
        CHK_TRDD(trdd);

        trdd->trdd_log = log;
}

#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *		dbe_trdd_listlen
 *
 *
 *
 * Parameters :
 *
 *	trdd -
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
int dbe_trdd_listlen(
        dbe_trdd_t* trdd)
{
        if (trdd == NULL) {
            return(0);
        } else {
            CHK_TRDD(trdd);
            return(su_list_length(trdd->trdd_ddlist));
        }
}
#endif /* SS_DEBUG */

#endif /* SS_NODDUPDATE */
