/*************************************************************************\
**  source       * dbe4tupl.c
**  directory    * dbe
**  description  * Tuple insert, update and delete operations.
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

This module implements the tuple level operations. There are functions
to insert, delete and update tuples in a relation. Delete and update
routines use a tuple reference to specify the tuple that is deleted
or updated.

No security checks are done at this level. The caller of this level
must implement them.

BLOB:

In delete, where to get the blob info if key values can be blobs (too
long v-tuple).

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

attribute type      rs0atype.h
attribute value     rs0aval.c
tuple type          rs0ttype.c
tuple value         rs0tval.c
relation handle     rs0relh.c
key definition      rs0key.c

index system        dbe0inde.c
transaction object  dbe0trx.c
tuple reference     dbe0tref.c


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstddef.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssservic.h>
#include <ssint8.h>

#include <su0parr.h>
#include <su0prof.h>

#include <uti0va.h>
#include <uti0vtpl.h>

#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0relh.h>
#include <rs0key.h>
#include <rs0tnum.h>

#include "dbe9type.h"
#include "dbe7ctr.h"
#include "dbe7cfg.h"
#include "dbe7binf.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe6log.h"
#include "dbe6bmgr.h"
#include "dbe6lmgr.h"
#ifdef SS_MME
#include "dbe4mme.h"
#endif
#include "dbe5dsea.h"
#include "dbe5isea.h"
#include "dbe5imrg.h"
#include "dbe5inde.h"
#include "dbe4tupl.h"
#include "dbe0erro.h"
#include "dbe0trx.h"
#include "dbe1trx.h"
#include "dbe0tref.h"
#include "dbe0db.h"
#include "dbe0blobg2.h"

#ifndef SS_LIGHT
# define DBE_XS
#endif /* SS_LIGHT */

#ifdef DBE_XS
#ifdef SS_MYSQL
# include <xs0sqli.h>
#else
# include "../xs/xs0sqli.h"
#endif
#endif

#ifndef SS_NODDUPDATE

#ifdef DBE_HSB_REPLICATION

typedef enum {
        TUPLE_LOCKRELH,
        TUPLE_UPDATE_CHECK_COLS,
        TUPLE_LOCKROW,
        TUPLE_INSERT_INIT,
        TUPLE_INSERT,
        TUPLE_DELETE,
        TUPLE_UPDATE,
        TUPLE_HOTSTANDBY_INS,
        TUPLE_HOTSTANDBY_DEL
} tuple_oper_t;

typedef enum {
        OPER_INS,
        OPER_DEL,
        OPER_UPD
} oper_type_t;

typedef struct {
        long ci_nrows;
        long ci_nbytes;
} tuple_upd_cardininfo_t;

struct dbe_tuplestate_st {
        ss_debug(dbe_chk_t ts_chk;)
        tuple_oper_t    ts_oper;
        rs_key_t*       ts_clustkey;
        dbe_tref_t*     ts_tref;
        bool            ts_insdynvtplp;
        dynvtpl_t       ts_insdvtpl;
        bool            ts_deldynvtplp;
        dynvtpl_t       ts_deldvtpl;
        rs_relh_t*      ts_relh;
        bool            ts_isblobattrs;
        dbe_datasea_t*  ts_datasea;
};

#define CHK_TS(ts)  ss_dassert(SS_CHKPTR(ts) && (ts)->ts_chk == DBE_CHK_TUPLESTATE)

dbe_tuplestate_t* dbe_tuplestate_init(void)
{
        dbe_tuplestate_t* ts;

        ts = SsMemAlloc(sizeof(dbe_tuplestate_t));
        ss_debug(ts->ts_chk = DBE_CHK_TUPLESTATE);
        ts->ts_oper = TUPLE_LOCKRELH;
        ts->ts_insdynvtplp = FALSE;
        ts->ts_insdvtpl = NULL;
        ts->ts_deldynvtplp = FALSE;
        ts->ts_deldvtpl = NULL;
        ts->ts_tref = NULL;
        ts->ts_isblobattrs = FALSE;
        ts->ts_datasea = NULL;

        CHK_TS(ts);
        return(ts);
}

void dbe_tuplestate_done(dbe_tuplestate_t *ts)
{
        CHK_TS(ts);

        if (ts->ts_insdynvtplp) {
            dynvtpl_free(&ts->ts_insdvtpl);
        }
        if (ts->ts_deldynvtplp) {
            dynvtpl_free(&ts->ts_deldvtpl);
        }
        if (ts->ts_datasea != NULL) {
            dbe_datasea_done(ts->ts_datasea);
        }
        CHK_TS(ts);
        SsMemFree(ts);
}

#endif /* DBE_HSB_REPLICATION */

typedef enum {
        CI_XSSTATE_FETCH,
        CI_XSSTATE_SORT,
        CI_XSSTATE_INSERT
} ci_xsstate_t;

struct dbe_tuple_createindex_st {
#ifdef SS_MME
        dbe_createindex_header_t    ci_hdr;
#endif
        rs_key_t*                   ci_key;
        rs_key_t*                   ci_clustkey;
        rs_sysi_t*                  ci_cd;
        dbe_trxid_t                 ci_usertrxid;
        dbe_trxnum_t                ci_committrxnum;
        dbe_btrsea_timecons_t       ci_tc;
        dbe_index_t*                ci_index;
        dbe_indsea_t*               ci_indsea;
        bool                        ci_commitp;
        bool                        ci_isunique;
        rs_ttype_t*                 ci_ttype;
        bool                        ci_nocheck;
#ifdef DBE_XS
        xs_sorter_t*                ci_sorter;
        rs_ttype_t*                 ci_xsttype;
        rs_tval_t*                  ci_xstval;
        rs_atype_t*                 ci_xsatype;
        rs_aval_t*                  ci_xsaval[2];
        ci_xsstate_t                ci_xsstate;
#endif /* DBE_XS */
};

struct dbe_tuple_dropindex_st {
#ifdef SS_MME
        dbe_dropindex_header_t  di_hdr;
#endif
        dbe_trxid_t             di_usertrxid;
        dbe_trxnum_t            di_committrxnum;
        dbe_btrsea_timecons_t   di_tc;
        dbe_index_t*            di_index;
        dbe_indsea_t*           di_indsea;
        bool                    di_isclustkey;
        bool                    di_commitp;
        bool                    di_nocheck;
        long                    di_keyid;
        rs_sysi_t*              di_cd;
        su_list_t*              di_deferredblobunlinklist;
        bool                    di_truncate;
        bool                    di_localdeferredblobunlinklist;
        dbe_btrsea_t            di_btrsea;       /* B-tree search state for nocheck mode. */
        dbe_btrsea_keycons_t    di_kc;           /* Key constraints. */
        bool                    di_bonsaitreesearch;
        rs_relh_t*              di_relh;
};

#endif /* SS_NODDUPDATE */

#ifndef SS_LIGHT

/*##**********************************************************************\
 *
 *		dbe_tuple_printvtpl
 *
 * Debug function to print v-tuple contents.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	relh -
 *
 *
 *	vtpl -
 *
 *
 *	trefp -
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
int dbe_tuple_printvtpl(
        void* cd,
        rs_relh_t* relh,
        vtpl_t* vtpl,
        bool trefp,
        bool oldhsb)
{
        uint kpno;
        rs_ttype_t* ttype;
        rs_key_t* key;
        uint nparts;
        va_t* va;

        ttype = rs_relh_ttype(cd, relh);
        key = rs_relh_clusterkey(cd, relh);
        if (trefp) {
            nparts = rs_key_lastordering(cd, key) + 1;
        } else {
            nparts = rs_key_nparts(cd, key);
        }
        va = VTPL_GETVA_AT0(vtpl);

        for (kpno = 0; kpno < nparts; kpno++, va = VTPL_SKIPVA(va)) {
            rs_ano_t ano;
            char buf[100];
            uint j;
            va_index_t len;
            uchar* p;
            rs_atype_t* atype;
            bool cutp;

            if (rs_keyp_isconstvalue(cd, key, kpno)) {
                continue;
            }
            ano = rs_keyp_ano(cd, key, kpno);
            if (va_testnull(va)) {
                SsDbgPrintf("%s = NULL\n", rs_ttype_aname(cd, ttype, ano));
                continue;
            }
            atype = rs_ttype_atype(cd, ttype, ano);
            switch (rs_atype_datatype(cd, atype)) {
#ifdef SS_UNICODE_DATA
                case RSDT_BINARY:
                case RSDT_UNICODE:
                    p = va_getdata(va, &len);
                    cutp = len > 25;
                    if (cutp) {
                        len = 25;
                    }
                    for (j = 0, len *= 2; j < len; j += 2, p++) {
                        static char hex[16] = "0123456789ABCDEF";
                        buf[j] = hex[*p >> 4];
                        buf[j + 1] = hex[*p & 0xf];
                    }
                    buf[j] = '\0';
                    if (cutp) {
                        strcat(buf, "...");
                    }
                    SsDbgPrintf("%.25s = 0x%s\n",
                        rs_ttype_aname(cd, ttype, ano),
                        buf);
                    break;
                case RSDT_CHAR:
                    SsDbgPrintf("%.25s = '%.50s'\n",
                        rs_ttype_aname(cd, ttype, ano),
                        va_getasciiz(va));
                    break;
#else /* SS_UNICODE_DATA */
                case RSDT_CHAR:
                    switch (rs_atype_sqldatatype(cd, atype)) {
                        case RSSQLDT_LONGVARBINARY:
                        case RSSQLDT_VARBINARY:
                        case RSSQLDT_BINARY:
                            p = va_getdata(va, &len);
                            cutp = len > 25;
                            if (cutp) {
                                len = 25;
                            }
                            for (j = 0, len *= 2; j < len; j += 2, p++) {
                                static char hex[16] = "0123456789ABCDEF";
                                buf[j] = hex[*p >> 4];
                                buf[j + 1] = hex[*p & 0xf];
                            }
                            buf[j] = '\0';
                            if (cutp) {
                                strcat(buf, "...");
                            }
                            SsDbgPrintf("%.25s = 0x%s\n",
                                rs_ttype_aname(cd, ttype, ano),
                                buf);
                            break;
                        default:
                            SsDbgPrintf("%.25s = '%.50s'\n",
                                rs_ttype_aname(cd, ttype, ano),
                                va_getasciiz(va));
                            break;
                    }
                    break;
#endif /* SS_UNICODE_DATA */
                case RSDT_INTEGER:
                    SsDbgPrintf("%s = %ld\n",
                        rs_ttype_aname(cd, ttype, ano),
                        va_getlong(va));
                    break;
                case RSDT_BIGINT:
                    if (oldhsb) {
                        SsDbgPrintf("%s = BIGINT not printed with old HSB\n",rs_ttype_aname(cd, ttype, ano));
                    } else {
                        ss_char_t buf[32];
                        SsInt8ToAscii(va_getint8(va), buf, RS_BIGINT_RADIX, 0, '0', TRUE);
                        SsDbgPrintf("%s = %s\n",
                            rs_ttype_aname(cd, ttype, ano), buf);
                    }
                    break;
                default:
                    SsDbgPrintf("%s = ?\n", rs_ttype_aname(cd, ttype, ano));
                    break;
            }
        }
        return(1);
}

#endif /* SS_LIGHT */

/*#***********************************************************************\
 *
 *		tuple_getmergekeycount
 *
 * Returns the number of merge keys from a single v-tuple. Merge limit
 * is calculated using a fixed key value length. If the key value length
 * is greater that the fixed length, it is counted as multiple key values
 * in merge key counters.
 *
 * NOTE! Started to use actual number of key values for merge.
 *
 * Parameters :
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
static uint tuple_getmergekeycount(vtpl_t* vtpl __attribute__ ((unused)))
{
#if 0
        uint len;

        len = BKEY_MAXHEADERLEN + VTPL_GROSSLEN(vtpl);

        return(len / DBE_CFG_MERGEKEYLEN + 1);
#else
        return(1);
#endif
}

#ifndef SS_NOBLOB

/*#***********************************************************************\
 *
 *		dbe_tuple_copyblobva
 *
 * Makes a "logical" copy of blob data. Actually with the new G2 Blobs
 * it only increment the reference count for the BLOB.
 * There is a need to
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data context
 *
 *      va - in, use
 *          v-attribute that has a BLOB reference
 *
 * Return value :
 *      SU_SUCCESS when successful, error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_tuple_copyblobva(
        rs_sysi_t* cd,
        va_t* va)
{
        dbe_ret_t rc;

        SS_PUSHNAME("dbe_tuple_copyblobva");
        ss_dprintf_3(("dbe_tuple_copyblobva\n"));

        ss_dassert(va_testblob(va));
        if (!dbe_brefg2_isblobg2check_from_va(va)) {
            ss_error; /* old format compatibility not coded yet!!! */
        }
        rc = (*dbe_blobg2callback_incrementpersistentrefcount_byva)(
                cd, va, NULL);
        SS_POPNAME;
        return(rc);
}

/*#***********************************************************************\
 *
 *		dbe_tuple_copyblobaval
 *
 * Makes a "logical" copy of blob data. Actually with the new G2 Blobs
 * it only increment the reference count for the BLOB.
 * There is a need to
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	atype -
 *
 *
 *	aval -
 *
 *
 * Return value :
 *      SU_SUCCESS when successful, error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_tuple_copyblobaval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        dbe_ret_t rc;
        va_t* va;

        SS_PUSHNAME("dbe_tuple_copyblobaval");
        ss_dassert(rs_aval_isblob(cd, atype, aval));
        ss_dprintf_3(("dbe_tuple_copyblobaval\n"));
        va = rs_aval_va(cd, atype, aval);
        ss_dassert(va_testblob(va));
        if (!dbe_brefg2_isblobg2check_from_va(va)) {
            rc = (*dbe_blobg2callback_copy_old_blob_to_g2)(
                    cd, atype, aval, NULL);
            if (rc == SU_SUCCESS) {
                va = rs_aval_va(cd, atype, aval);
            } else {
                goto exitcode;
            }
        }
        rc = dbe_tuple_copyblobva(cd, va);
 exitcode:;
        SS_POPNAME;
        return(rc);
}

#endif /* SS_NOBLOB */

/*#***************************************************************\
*                                                               
*                  trx_uniquecheck_isnull                       
*                                                               
* Check is any of unique fields is NULL.
*
* Parameters:
*
*     cd -
* 
*     key -
*
*     key_vtpl - 
*
*
* Return values : 
*                TRUE if a value of an unique field is NULL, otherwise FALSE. 
*
* Comments :
*
* Globals used : 
*
*/
bool trx_uniquecheck_isnull(
        rs_sysi_t* cd,
        rs_key_t* key,
        vtpl_t* key_vtpl)
{
        int nordering;
        int i;
        va_t* va;

        ss_dassert(rs_key_isunique(cd, key));

        nordering = rs_key_lastordering(cd, key) + 1;

        va = VTPL_GETVA_AT0(key_vtpl);
        for (i = 0; i < nordering; i++, va = VTPL_SKIPVA(va)) {
	  if (va_testnull(va)) {
	    return TRUE;
	  }
        }
	
     return FALSE;
}

/*#***********************************************************************\
 *
 *		tuple_buildkey_tval
 *
 * Builds a key value from a tuple value.
 *
 * Parameters :
 *
 *	cd - in, use
 *		Client data.
 *
 *	key - in, use
 *		Key definition where the corresponding key value
 *		is build.
 *
 *	ttype - in, use
 *		Tuple type of the tuple value from where the key value
 *		is build.
 *
 *	tval - in, use
 *		Tuple type from where the key value is build.
 *
 *	copyblobp - in
 *		If TRUE, make copies of uncopied blob attributes.
 *      actually, this is now just a reference counter increment
 *      for G2 blobs
 *
 *	p_dvtpl - out, give
 *		Pointer to the dynamic v-tuple into where the new
 *		key value is allocated.
 *
 *	p_isblob - out
 *		TRUE is set to *p_isblob if there are blob attributes
 *		in the resulting v-tuple
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t tuple_buildkey_tval(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        bool copyblobp,
        dbe_trxid_t trxid __attribute__ ((unused)),
        bool* upd_attrs __attribute__ ((unused)),
        dynvtpl_t* p_dvtpl,
        bool* p_isblob)
{
        uint i;
        uint nparts;
        dynva_t blobinfo = NULL;
        dbe_ret_t rc = DBE_RC_SUCC;
#ifdef SS_COLLATION
        union {
                void* dummy_for_alignment;
                ss_byte_t _buf[BUFVA_MAXBUFSIZE];
        } keyva;
        bufva_init(keyva._buf, sizeof(keyva._buf));
#endif /* SS_COLLATION */
        ss_dprintf_3(("tuple_buildkey_tval\n"));

#ifdef DBE_UPDATE_OPTIMIZATION
        if (!rs_key_isclustering(cd, key)
            && !rs_key_istupleversion(cd, key)
            && !dbe_trx_keypartsupdated(cd, key, -1, upd_attrs))
        {
            ss_dprintf_3(("tuple_buildkey_tval:key %d not changed\n", rs_key_id(cd, key)));
            rc = DBE_RC_NOTFOUND;
            goto cleanup_ret_rc;
        }
#endif

        *p_isblob = FALSE;
        nparts = rs_key_nparts(cd, key);
        dynvtpl_setvtpl(p_dvtpl, VTPL_EMPTY);

        for (i = 0; i < nparts && rc == DBE_RC_SUCC; i++) {
            va_t* va;
            uint ano = 0;
            rs_atype_t* atype = NULL;
            rs_aval_t* aval;
            rs_aval_t* desc_aval;
            int prefixlen;

            desc_aval = NULL;

            if (rs_keyp_isconstvalue(cd, key, i)) {
                /* A constant attribute in a key, get the constant
                 * value.
                 */
                va = rs_keyp_constvalue(cd, key, i);

            } else {
                /* Get the attribute value from the tval.
                 */
                ano = rs_keyp_ano(cd, key, i);
#ifdef SS_COLLATION
                atype = rs_ttype_atype(cd, ttype, ano);
                aval = rs_tval_aval(cd, ttype, tval, ano);
                prefixlen = rs_keyp_getprefixlength(cd, key, i);

                va = rs_aval_getkeyva(cd,
                                      atype, aval,
                                      rs_keyp_collation(cd, key, i),
                                      rs_keyp_parttype(cd, key, i),
                                      rs_keyp_isascending(cd, key, i),
                                      keyva._buf,
                                      sizeof(keyva._buf),
                                      prefixlen);

                if (va == NULL) {
                    rc = DBE_ERR_FAILED;
                    va = VA_NULL;
                }
#else /* SS_COLLATION */
                if (rs_keyp_isascending(cd, key, i)) {
                    va = rs_tval_va(cd, ttype, tval, ano);
                } else {
                    ss_dassert(!copyblobp);
                    atype = rs_ttype_atype(cd, ttype, ano);
                    aval = rs_tval_aval(cd, ttype, tval, ano);
                    desc_aval = rs_aval_copy(cd, atype, aval);
                    if (rs_aval_asctodesc(cd, atype, desc_aval)) {
                        va = rs_aval_va(cd, atype, desc_aval);
                    } else {
                        va = VA_NULL;
                        rc = DBE_ERR_ILLDESCVAL;
                    }
                }
#endif /* SS_COLLATION */
            }
            ss_dassert(va != NULL);

            if (rc == DBE_RC_SUCC && va_testblob(va)) {
#ifndef SS_NOBLOB
                if (blobinfo == NULL) {
                    dbe_blobinfo_init(&blobinfo, nparts);
                }
                dbe_blobinfo_append(&blobinfo, i);
                if (copyblobp) {
                    atype = rs_ttype_atype(cd, ttype, ano);
                    aval = rs_tval_aval(cd, ttype, tval, ano);
                    ss_dassert(!rs_keyp_isconstvalue(cd, key, i));

                    rc = dbe_tuple_copyblobaval(
                            cd,
                            atype,
                            aval);
                    /* to refresh the va value! */
                    va = rs_aval_va(cd, atype, aval);
                }
#else /* SS_NOBLOB */
                rc = DBE_ERR_FAILED;
#endif /* SS_NOBLOB */
            }
            dynvtpl_appva(p_dvtpl, va);
            if (desc_aval != NULL) {
                rs_aval_free(cd, atype, desc_aval);
            }
        }
#ifndef SS_NOBLOB
        if (blobinfo != NULL) {
            /* There were blob attributes, append blobinfo to the v-tuple.
             */
            *p_isblob = TRUE;
            dynvtpl_appva(p_dvtpl, blobinfo);
            dynva_free(&blobinfo);
        }
#endif /* SS_NOBLOB */
cleanup_ret_rc:;
#ifdef SS_COLLATION
        bufva_done(keyva._buf, sizeof(keyva._buf));
#endif /* SS_COLLATION */
        return(rc);
}

/*#***********************************************************************\
 *
 *		tuple_buildkey_vtpl
 *
 * Builds a key value from a v-tuple. V-tuple must be a clustering key.
 *
 * Parameters :
 *
 *	cd - in, use
 *		Client data.
 *
 *	key - in, use
 *		Key definition used to build the v-tuple.
 *
 *	clustkey - in, use
 *		Key definition for the clustering key.
 *
 *	ttype - in, use
 *		Tuple type of the tuple value from where the key value
 *		is build.
 *
 *	clust_vamap - in, use
 *		Vmap for the clustering key.
 *
 *	blobattrs - in
 *		Bool array where true for every blob attribute.
 *
 *	p_dvtpl - out, give
 *		Pointer to the dynamic v-tuple into where the new
 *		key value is allocated.
 *
 *	p_isblob - out
 *		TRUE is set to *p_isblob if there are blob attributes
 *		in the resulting v-tuple
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t tuple_buildkey_vtpl(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_key_t* clustkey,
        rs_ttype_t* ttype,
        vtpl_vamap_t* clust_vamap,
        bool* blobattrs,
        bool* upd_attrs __attribute__ ((unused)),
        dynvtpl_t* p_dvtpl,
        bool* p_isblob)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        uint i;
        uint nparts;
        dynva_t blobinfo = NULL;
        bool succp;
#ifdef SS_COLLATION
        union {
                void* dummy_for_alignment;
                ss_byte_t _buf[BUFVA_MAXBUFSIZE];
        } keyva;
        bufva_init(keyva._buf, sizeof(keyva._buf));
#endif /* SS_COLLATION */

        succp = TRUE;
        *p_isblob = FALSE;
        nparts = rs_key_nparts(cd, key);
        dynvtpl_setvtpl(p_dvtpl, VTPL_EMPTY);

#ifdef DBE_UPDATE_OPTIMIZATION
        if (!rs_key_isclustering(cd, key)
            && !rs_key_istupleversion(cd, key)
            && !dbe_trx_keypartsupdated(cd, key, -1, upd_attrs)) 
        {
            ss_dprintf_3(("tuple_buildkey_vtpl:key %d not changed\n", rs_key_id(cd, key)));
            rc = DBE_RC_NOTFOUND;
            goto cleanup_ret_rc;
        }
#endif

        for (i = 0; i < nparts && succp; i++) {
            va_t* va;
            rs_atype_t*     atype = NULL;
            rs_aval_t*      aval;
#ifdef SS_COLLATION
            rs_aval_t       avalbuf;
#else /* SS_COLLATION */
            rs_atype_t* desc_atype = NULL;
            rs_aval_t* desc_aval;
            desc_aval = NULL;
#endif /* SS_COLLATION */
            
            aval = NULL;

            if (rs_keyp_isconstvalue(cd, key, i)) {

                va = rs_keyp_constvalue(cd, key, i);

            } else {
                uint ano;   /* Attribute number in ttype. */
                uint kpno;  /* Key part number in clustering key. */
                int prefixlen;

                /* Get attribute number of this key part. */
                ano = rs_keyp_ano(cd, key, i);
                ss_dassert((int)ano >= 0);

                /* Get key part number in the clustering key. */
                kpno = rs_key_searchkpno_data(cd, clustkey, ano);
                ss_dassert(kpno != (uint)RS_ANO_NULL);

                /* Get v-attribute value from the clustering key. */
                va = vtpl_vamap_getva_at(clust_vamap, kpno);
#ifdef SS_COLLATION
                if (rs_keyp_parttype(cd, key, i) == RSAT_COLLATION_KEY
                ||  !rs_keyp_isascending(cd, key, i)
                ||  va == VA_DEFAULT)
                {
                    atype = rs_ttype_atype(cd, ttype, ano);
                    rs_aval_createbuf(cd, atype, &avalbuf);
                    aval = &avalbuf;
                    rs_aval_setva(cd, atype, &avalbuf, va);
                    prefixlen = rs_keyp_getprefixlength(cd, key, i);

                    va = rs_aval_getkeyva(cd,
                                          atype, &avalbuf, 
                                          rs_keyp_collation(cd, key, i),
                                          rs_keyp_parttype(cd, key, i),
                                          rs_keyp_isascending(cd, key, i),
                                          keyva._buf,
                                          sizeof(keyva._buf),
                                          prefixlen);

                    if (va == NULL) {
                        rc = DBE_ERR_FAILED;
                        va = VA_NULL;
                    }
                }
#else /* SS_COLLATION */
                if (!rs_keyp_isascending(cd, key, i)) {
                    /* Create descending va. */
                    desc_atype = rs_ttype_atype(cd, ttype, ano);
                    desc_aval = rs_aval_create(cd, desc_atype);
                    rs_aval_setva(cd, desc_atype, desc_aval, va);
                    succp = rs_aval_asctodesc(cd, desc_atype, desc_aval);
                    if (succp) {
                        va = rs_aval_va(cd, desc_atype, desc_aval);
                    } else {
                        rc = DBE_ERR_ILLDESCVAL;
                        va = VA_NULL;
                    }
                } else if (va == VA_DEFAULT) {
                    /* Get the original default, if any, right. */
                    atype = rs_ttype_atype(cd, ttype, ano);
                    aval = rs_aval_create(cd, atype);
                    rs_aval_setva(cd, atype, aval, va);
                    va = rs_aval_va(cd, atype, aval);
                }
#endif /* SS_COLLATION */
#ifndef SS_NOBLOB
                if (blobattrs != NULL && blobattrs[kpno]) {
                    /* This is a blob attribute. */
                    ss_dassert(va_testblob(va));
                    if (blobinfo == NULL) {
                        dbe_blobinfo_init(&blobinfo, nparts);
                    }
                    dbe_blobinfo_append(&blobinfo, i);
                } else {
                    ss_dassert(!va_testblob(va));
                }
#endif /* SS_NOBLOB */
            }

            dynvtpl_appva(p_dvtpl, va);

#ifdef SS_COLLATION
            if (aval != NULL) {
                ss_dassert(aval == &avalbuf);
                rs_aval_freebuf(cd, atype, aval);
            }
#else /* SS_COLLATION */
            if (desc_aval != NULL) {
                rs_aval_free(cd, desc_atype, desc_aval);
            }
            if (aval != NULL) {
                rs_aval_free(cd, atype, aval);
            }
#endif /* SS_COLLATION */
        }

#ifndef SS_NOBLOB
        if (blobinfo != NULL) {
            /* There were blob attributes, append blobinfo to the v-tuple.
             */
            *p_isblob = TRUE;
            dynvtpl_appva(p_dvtpl, blobinfo);
            dynva_free(&blobinfo);
        }
#endif /* SS_NOBLOB */
cleanup_ret_rc:;
#ifdef SS_COLLATION
        bufva_done(keyva._buf, sizeof(keyva._buf));
#endif /* SS_COLLATION */

        return (rc);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_isnocheck
 *
 * Checks if tuple operation should be run in nocheck mode.
 *
 * Parameters :
 *
 *		cd -
 *
 *
 *		trx -
 *
 *
 *		relh -
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
bool dbe_tuple_isnocheck(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh)
{
        bool isnocheck;

        if (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX) {
            isnocheck = rs_relh_isnocheck(cd, relh);
        } else {
            isnocheck = rs_relh_isnocheck(cd, relh) || dbe_trx_isnocheck(trx);
        }
        ss_dprintf_4(("dbe_tuple_isnocheck:isnocheck=%d\n", isnocheck));
        return(isnocheck);
}

/*#***********************************************************************\
 *
 *		tuple_insordel_vtpl
 *
 * Inserts or deletes a v-tuple from the database according to a value
 * of flag insertp. The v-tuple is the clustering key v-tuple. Other key
 * values are constructed from the clustering key v-tuple and also inserted
 * or deleted.
 *
 * Parameters :
 *
 *      type - in
 *          Operation type: ins, del or upd. Both del and upd deletes rows.
 *
 *	cd - in, use
 *		Client data.
 *
 *	trx - in out, use
 *		Transaction handle. Can be DBE_TRX_NOTRX, if transactions
 *		are not used.
 *
 *      trxnum - in
 *          Transaction number when trx == DBE_TRX_NOTRX or
 *          DBE_TRXNUM_NULL otherwise
 *
 *	usertrxid - in
 *		Transaction id for the deleted tuple.
 *
 *	keytrxid - in
 *		Key trx id for the deleted tuple, used if physical delete.
 *
 *	relh - in, use
 *		Relation handle for the deleted tuple.
 *
 *	clustkey_vtpl - in, use
 *		V-tuple of the clustering key of the relation.
 *
 *	tref - in, use
 *		Tuple reference in case of a delete.
 *
 *	clustkey_isblob - in
 *		If TRUE, the clustering key contains blob attributes.
 *
 *	upd_attrs - in, use
 *          If not NULL, a boolean array containg TRUE for those attributes
 *          that has been updated.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_EXISTS
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t tuple_insordel_vtpl(
        oper_type_t opertype,
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxnum_t trxnum,
        dbe_trxid_t usertrxid __attribute__ ((unused)),
        dbe_trxid_t keytrxid __attribute__ ((unused)),
        dbe_trxid_t stmttrxid,
        rs_relh_t* relh,
        vtpl_t* clustkey_vtpl,
        dbe_tref_t* tref,
#ifdef DBE_HSB_REPLICATION
        dbe_tuplestate_t* ts,
#endif /* DBE_HSB_REPLICATION */
        bool clustkey_isblob,
        bool isnocheck,
        bool* upd_attrs,
        tuple_upd_cardininfo_t* ci,
        bool updatep,
        bool addtolog,
        bool prelocked)
{
        uint nattrs = 0;
        dynvtpl_t dvtpl = NULL;
        vtpl_t* key_vtpl;
        su_pa_t* keys;
        uint i;
        rs_key_t* key = NULL;
        dbe_index_t* index = 0;
        rs_key_t* clustkey;
        vtpl_vamap_t* clustkey_vamap;
        dbe_ret_t rc;
        bool* blobattrs = NULL;
        int keypos = 0;
        bool isonlydelemark = FALSE;
        rs_reltype_t reltype;
        rs_ttype_t* ttype;
        bool clustkey_changed = FALSE;
        dbe_indexop_mode_t mode = 0;
	bool is_unique = FALSE; 

        ss_dassert(trx != NULL);

        reltype = rs_relh_reltype(cd, relh);
        switch (reltype) {
            case RS_RELTYPE_OPTIMISTIC:
            case RS_RELTYPE_PESSIMISTIC:
                index = dbe_db_getindex(rs_sysi_db(cd));
                break;
            default:
                ss_error;
        }

        /*
            Insert or delete all key values.
        */

        clustkey = rs_relh_clusterkey(cd, relh);
        ss_dassert(clustkey != NULL);

        keys = rs_relh_keys(cd, relh);
        rc = DBE_RC_SUCC;
        ttype = rs_relh_ttype(cd, relh);

        if (clustkey_isblob) {
            nattrs = rs_ttype_nattrs(cd, ttype);
            blobattrs = dbe_blobinfo_getattrs(
                            clustkey_vtpl,
                            nattrs,
                            NULL);
        }

        clustkey_vamap = vtpl_vamap_init(clustkey_vtpl);
        if (clustkey_isblob) {
            vtpl_vamap_removelast(clustkey_vamap);
        }

#ifdef DBE_UPDATE_OPTIMIZATION
        if (opertype == OPER_UPD && dbe_trx_keypartsupdated(cd, clustkey, -1, upd_attrs)) {
            /* Reference key parts updated, update all key parts. */
            clustkey_changed = TRUE;
        }
#endif

        /* Build all key values.
         */
        su_pa_do_get(keys, i, key) {
            bool isblob;

            if (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX) {
                if (!DBE_TRXNUM_EQUAL(trxnum, DBE_TRXNUM_NULL)) {
                    mode = DBE_INDEXOP_COMMITTED;
                } else {
                    mode = 0;
                }
            } else if (dbe_trx_forcecommit(trx)) {
                mode = DBE_INDEXOP_COMMITTED;
                trxnum = dbe_trx_getcommittrxnum(trx);
            } else {
                mode = 0;
            }

            if (rs_key_isclustering(cd, key)) {
                ulong bytecount;
                key_vtpl = clustkey_vtpl;
                isblob = clustkey_isblob;
                /* Update table cardinal */
                bytecount = (ulong)vtpl_grosslen(clustkey_vtpl);
                if (opertype == OPER_INS) {
                    if (isblob) {
                        /* inserted BLOB columns must be correctly linked! */
                        uint ano;
                        for (ano = 0; ano < nattrs; ano++) {
                            if (blobattrs[ano]) {
                                va_t* blobva =
                                    vtpl_vamap_getva_at(clustkey_vamap, ano);
                                rc = dbe_tuple_copyblobva(cd, blobva);
                                ss_rc_dassert(rc == DBE_RC_SUCC, rc);
                            }
                        }
                    }
                    ss_dassert(ci == NULL);
                    if (trx == DBE_TRX_NOTRX || isnocheck) {
                        rs_relh_insertbytes(cd, relh, bytecount, 1);
                    } else {
                        ss_dassert(cd != NULL);
                        dbe_trx_insertbytes(cd, relh, bytecount, 1);
                    }
                } else {
                    if (isnocheck && opertype == OPER_UPD && ci != NULL) {
                        ci->ci_nrows--;
                        ci->ci_nbytes -= bytecount;
                    } else if (trx == DBE_TRX_NOTRX || isnocheck) {
                        rs_relh_deletebytes(cd, relh, bytecount,
                                            updatep ? 0 : 1);
                    } else {
                        ss_dassert(cd != NULL);
                        dbe_trx_deletebytes(cd, relh, bytecount,
                                            updatep ? 0 : 1);
                    }
                }
                mode |= DBE_INDEXOP_CLUSTERING;
            } else {
                rc = tuple_buildkey_vtpl(
                        cd,
                        key,
                        clustkey,
                        ttype,
                        clustkey_vamap,
                        blobattrs,
                        clustkey_changed ? NULL : upd_attrs,
                        &dvtpl,
                        &isblob);
#ifdef DBE_UPDATE_OPTIMIZATION
                if (rc == DBE_RC_NOTFOUND) {
                    ss_dassert(opertype == OPER_UPD);
                    continue;
                }
#endif
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                key_vtpl = dvtpl;
            }
            if (isblob) {
                mode |= DBE_INDEXOP_BLOB;
            }
            if (isnocheck) {
                mode |= DBE_INDEXOP_NOCHECK;
            }
            if (opertype == OPER_UPD) {
#ifdef DBE_UPDATE_OPTIMIZATION
                ss_dassert(clustkey_changed || 
                           (mode & DBE_INDEXOP_CLUSTERING) || 
                           rs_key_isclustering(cd, key) || 
                           rs_key_istupleversion(cd, key) ||
                           dbe_trx_keypartsupdated(cd, key, -1, upd_attrs));
                mode |= DBE_INDEXOP_UPDATE;
#else
                if (!dbe_trx_keypartsupdated(cd, key, -1, upd_attrs)) {
                    mode |= DBE_INDEXOP_UPDATE;
                }
#endif
                if (isnocheck && prelocked) {
                    mode |= DBE_INDEXOP_PRELOCKED;
                }
            }
            if (opertype == OPER_INS) {
                ss_dassert(tref == NULL);
                rc = dbe_index_insert(
                        index,
                        rs_key_id(cd, key),
                        key_vtpl,
                        trxnum,
                        stmttrxid,
                        mode,
                        cd);
                ss_dassert(rc != DBE_ERR_LOSTUPDATE);

            } else {
                ss_dassert(tref != NULL);
                rc = dbe_index_delete(
                        index,
                        rs_key_id(cd, key),
                        key_vtpl,
                        trxnum,
                        stmttrxid,
                        mode,
                        &isonlydelemark,
                        cd);
            }
            if (rc != DBE_RC_SUCC) {
                if (rc == DBE_ERR_UNIQUE_S && trx != DBE_TRX_NOTRX && trx != DBE_TRX_HSBTRX) {
                    dbe_trx_seterrkey(trx, key);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                    dbe_trx_builduniquerrorkeyvalue(
                        trx, 
                        relh, 
                        key, 
                        key_vtpl,
                        0);
#endif
                }
                break;
            }

            if (trx != DBE_TRX_NOTRX && trx != DBE_TRX_HSBTRX  && !isnocheck) {
                rc = dbe_trx_addwrite(
                        trx,
                        opertype == OPER_INS,
                        key,
                        tref,
                        tuple_getmergekeycount(key_vtpl),
                        isonlydelemark,
                        relh,
                        reltype);
                if (rc != DBE_RC_SUCC) {
                    break;
                }
            }

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
 	    /* To check if the key is unique and nullable then  */
 	    /* below referential integrity check should be omitted*/
 	    if (rs_key_isunique(cd, key) && key->k_refkeys != 0
 		&& !rs_key_isprimary(cd, key)
 		&& (opertype == OPER_UPD || opertype == OPER_DEL)) {
 	      is_unique = trx_uniquecheck_isnull(cd,key,dvtpl);
 	    }
#endif
            keypos++;
        }
        dynvtpl_free(&dvtpl);

#ifdef DBE_UPDATE_OPTIMIZATION
        if (rc == DBE_RC_NOTFOUND) {
            ss_dassert(opertype == OPER_UPD);
            rc = DBE_RC_SUCC;
        }
#endif

#ifdef REFERENTIAL_INTEGRITY
        if (rc == DBE_RC_SUCC && trx != DBE_TRX_NOTRX && trx != DBE_TRX_HSBTRX && !isnocheck && !is_unique) {
            /* Add key reference checks.
             */
            su_pa_t* refkeys;
            refkeys = rs_relh_refkeys(cd, relh);
            su_pa_do_get(refkeys, i, key) {
                bool add_check = FALSE;
                if (opertype == OPER_INS) {
                    add_check = (rs_key_type(cd, key) == RS_KEY_FORKEYCHK);
                } else if (rs_key_type(cd, key) == RS_KEY_PRIMKEYCHK) {
                    int action;
                    if (opertype==OPER_UPD) {
                        action = rs_key_update_action(cd, key);
#ifdef SS_MYSQL
                        /* This need to be changed when we support other
                         * foreign key actions than RESTRICT.
                         */
                        add_check = action == SQL_REFACT_SETDEFAULT ||
                                    action == SQL_REFACT_NOACTION ||
                                    action == SQL_REFACT_RESTRICT;
#else
                        add_check = action == SQL_REFACT_SETDEFAULT ||
                                    action == SQL_REFACT_NOACTION;
#endif
                    } else {
                        action = rs_key_delete_action(cd, key);
                        add_check = action != SQL_REFACT_CASCADE &&
                                    action != SQL_REFACT_SETNULL;
                    }
                }
                if (add_check) {
                    rc = dbe_trx_addrefkeycheck(
                            cd,
                            trx,
                            relh,
                            clustkey,
                            key,
                            relh,
                            clustkey_vamap,
                            upd_attrs,
                            reltype);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                }
            }
        }
#endif /* REFERENTIAL_INTEGRITY */

        if (rc == DBE_RC_SUCC && trx != DBE_TRX_NOTRX && trx != DBE_TRX_HSBTRX) {
            if (addtolog) {
                rc = dbe_trx_addtolog(
                        trx,
                        opertype == OPER_INS,
                        clustkey,
                        tref,
                        clustkey_vtpl,
                        relh,
                        clustkey_isblob);
            }
#ifdef DBE_HSB_REPLICATION
            if (rc == DBE_RC_SUCC) {
                ss_dassert(tref != NULL);
                CHK_TS(ts);
                ts->ts_tref = tref;
                if (opertype == OPER_INS) {
                    ts->ts_insdvtpl = clustkey_vtpl;
                    ts->ts_insdynvtplp = FALSE;
                } else {
                    ts->ts_deldvtpl = clustkey_vtpl;
                    ts->ts_deldynvtplp = FALSE;
                }
                ts->ts_relh = relh;
                ts->ts_isblobattrs = clustkey_isblob;
                CHK_TS(ts);
            }
#endif /* DBE_HSB_REPLICATION */
        }

        if (blobattrs != NULL) {
            SsMemFree(blobattrs);
        }
        vtpl_vamap_done(clustkey_vamap);

        if (rc != DBE_RC_SUCC && rs_sysi_testflag(cd, RS_SYSI_FLAG_MYSQL)) {
            if (opertype == OPER_DEL
                && rc == DBE_ERR_UNIQUE_S) 
            {
                /* Trying to delete the sam row twice. It can
                 * happen in multi-table delete.
                 */
                rc = DBE_RC_SUCC;
            }
        }

        ss_dprintf_4(("tuple_insordel_vtpl, rc = %s\n", su_rc_nameof(rc)));

        return(rc);
}

/*#***********************************************************************\
 *
 *		tuple_insblobattrs_limit
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      relh - in, use
 *          relation handle
 *
 *      ttype -
 *
 *
 *      tval -
 *
 *
 *      bloblimit -
 *
 *
 *      trxid -
 *
 *
 *      p_glen -
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
static dbe_ret_t tuple_insblobattrs_limit(
        rs_sysi_t* cd,
        rs_relh_t* relh __attribute__ ((unused)),
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        va_index_t bloblimit,
        dbe_trxid_t trxid __attribute__ ((unused)),
        rs_key_t* clustkey,
        long* p_glen)
{
        int i;
        rs_ano_t nattrs;
        long tuple_glen;
        rs_atype_t* atype;
        rs_aval_t* aval;
        va_t* va;
        va_index_t va_glen;
        dbe_ret_t rc = DBE_RC_SUCC;
        bool isblob = FALSE;

        SS_PUSHNAME("tuple_insblobattrs_limit");

        tuple_glen = VA_LENGTHMAXLEN + BKEY_MAXHEADERLEN;
        nattrs = rs_ttype_nattrs(cd, ttype);

        for (i = 0; i < nattrs; i++) {

            atype = rs_ttype_atype(cd, ttype, i);
            aval = rs_tval_aval(cd, ttype, tval, i);
            if (rs_aval_isnull(cd, atype, aval)) {
                tuple_glen += VA_GROSSLEN(VA_NULL);
            } else {
                va = rs_aval_va(cd, atype, aval);
                va_glen = VA_GROSSLEN(va);
                switch (rs_atype_datatype(cd, atype)) {
                    case RSDT_CHAR:
#ifdef SS_UNICODE_DATA
                    case RSDT_UNICODE:
                    case RSDT_BINARY:
#endif /* SS_UNICODE_DATA */
                        if (va_glen > bloblimit) {
#ifndef SS_NOBLOB
                            rs_ano_t kpno;
                            ss_dprintf_4(("tuple_insblobattrs_limit:blob attr = %d, va_glen = %ld, bloblimit = %ld\n",
                                i, (long)va_glen, (long)bloblimit));
                            ss_dassert(!rs_aval_isblob(cd, atype, aval));
                            kpno = rs_key_searchkpno_data(cd, clustkey, i);
                            if (kpno < rs_key_nrefparts(cd, clustkey)
                                && !rs_sysi_testflag(cd, RS_SYSI_FLAG_MYSQL)) 
                            {
                                /* Blob attribute in primary key reference
                                 * part.
                                 */
                                SS_POPNAME;
                                return(DBE_ERR_PRIMKEYBLOB);
                            }
#ifdef DBE_HSB_REPLICATION
#ifndef SS_HSBG2
                            if (dbe_db_ishsb(rs_sysi_db(cd))
                                && !(rs_relh_issysrel(cd, relh) &&
                                     !rs_relh_isrowlevelhsb(cd, relh)))
                            {
                                /* BLOB attributes are currently not allowed
                                 * in HSB configurations
                                 */
                                SS_POPNAME;
                                return (DBE_ERR_HSBBLOB);

                            }
#endif /* !SS_HSBG2 */
#endif /* DBE_HSB_REPLICATION */
                            rc = dbe_blobg2_insertaval(
                                    cd,
                                    atype,
                                    aval,
                                    DBE_BLOBVAGROSSLEN(RS_KEY_MAXCMPLEN),
                                    NULL);
                            if (rc != DBE_RC_SUCC) {
                                SS_POPNAME;
                                return(rc);
                            }
                            va = rs_aval_va(cd, atype, aval);
                            ss_dassert(va_testblob(va));
                            va_glen = VA_GROSSLEN(va);
                            tuple_glen += va_glen;
                            tuple_glen += DBE_BLOBINFO_ITEMLEN;
                            isblob = TRUE;
#else
                            SS_POPNAME;
                            return(DBE_ERR_FAILED);
#endif /* SS_NOBLOB */
                        } else {
                            if (rs_aval_isblob(cd, atype, aval)
                                && !rs_sysi_testflag(cd, RS_SYSI_FLAG_MYSQL)) 
                            {
                                rs_ano_t kpno;
                                ss_dprintf_4((
                                        "tuple_insblobattrs_limit:already blob attr = %d\n",
                                        i));
                                kpno = rs_key_searchkpno_data(cd, clustkey, i);
                                if (kpno < rs_key_nrefparts(cd, clustkey)) {
                                    /* Blob attribute in primary key reference
                                     * part.
                                     */
                                    SS_POPNAME;
                                    return(DBE_ERR_PRIMKEYBLOB);
                                }
                            }
                            tuple_glen += va_glen;
                        }
                        break;
                    default:
                        /*  Assert removed so we need not pass index object
                            as a parameter. Jarmo Aug 22, 1995
                            ss_dassert(va_glen < dbe_index_getbloblimit_low(index));
                        */
                        tuple_glen += va_glen;
                        break;
                }
            }
        }
#ifndef SS_NOBLOB
        if (isblob) {
            tuple_glen += DBE_BLOBINFO_HEADERLEN;
            ss_dprintf_4(("tuple_insblobattrs_limit:tuple_glen = %ld\n", tuple_glen));
        }
#endif /* SS_NOBLOB */

        *p_glen = tuple_glen;

        SS_POPNAME;

        return(rc);
}

/*#***********************************************************************\
 *
 *		tuple_insblobattrs
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	index -
 *
 *
 *      relh - in, use
 *          relation handle
 *
 *	ttype -
 *
 *
 *	tval -
 *
 *
 *	trxid -
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
static dbe_ret_t tuple_insblobattrs(
        rs_sysi_t* cd,
        dbe_index_t* index,
        rs_relh_t* relh,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        rs_key_t* clustkey,
        dbe_trxid_t trxid)
{
        dbe_ret_t rc;
        long tuple_glen;
        va_index_t bkey_bloblimit_high;

        SS_PUSHNAME("tuple_insblobattrs");
        bkey_bloblimit_high = dbe_index_getbloblimit_high(index);

        if (rs_key_maxstoragelen(cd, clustkey) >= bkey_bloblimit_high) {
            /* First use larger value to check if any attribute must be
             * stored as a blob.
             */
            rc = tuple_insblobattrs_limit(
                    cd,
                    relh,
                    ttype,
                    tval,
                    bkey_bloblimit_high,
                    trxid,
                    clustkey,
                    &tuple_glen);

            if (rc == DBE_RC_SUCC && (ulong)tuple_glen > bkey_bloblimit_high) {
                /* The total tuple length is still too large, use lower
                 * blob limit to store long attributes as blobs.
                 */
                rc = tuple_insblobattrs_limit(
                        cd,
                        relh,
                        ttype,
                        tval,
                        dbe_index_getbloblimit_low(index),
                        trxid,
                        clustkey,
                        &tuple_glen);
            }
        } else {
            rc = DBE_RC_SUCC;
        }
        SS_POPNAME;
        return(rc);
}

/*#***********************************************************************\
 *
 *		tuple_insert_tval
 *
 * Inserts a tuple into database. The key values are constructed
 * from the tval and inserted to the database.
 *
 * Parameters :
 *
 *	type - in
 *		Operation type: ins or upd.
 *
 *	cd - in, use
 *		Client data object.
 *
 *	trx - in out, use
 *		Transaction handle. Can be DBE_TRX_NOTRX, if transactions
 *		are not used.
 *
 *	usertrxid - in
 *		User transaction id for the new tuple.
 *
 *	relh - in, use
 *		Relation handle specifying the relation into where the
 *		tuple is added. The key definitions are also found from
 *		this handle.
 *
 *	tval - in, use
 *		Tuple value of the new inserted tuple. This must be of
 *		same type as the tuple value of relation handle.
 *
 *	tref - out, use
 *          If not NULL, the tuple referemce of the newly inserted
 *		tuple is returned here.
 *
 *	upd_attrs - in, use
 *          If not NULL, a boolean array containg TRUE for those attributes
 *          that has been updated.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_EXISTS
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t tuple_insert_tval(
        oper_type_t opertype,
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_relh_t* relh,
        rs_tval_t* tval,
        dbe_tref_t* tref,
#ifdef DBE_HSB_REPLICATION
        dbe_tuplestate_t* ts,
#endif /* DBE_HSB_REPLICATION */
        bool isnocheck,
        bool* upd_attrs,
        tuple_upd_cardininfo_t* ci,
        bool addtolog,
        bool prelocked)
{
        dynvtpl_t dvtpl = NULL;
        dynvtpl_t clustkey_dvtpl = NULL;
        su_pa_t* keys;
        uint i;
        rs_key_t* clustkey;
        rs_key_t* key = NULL;
        dbe_index_t* index;
        rs_ttype_t* ttype;
        dbe_ret_t rc;
        bool clustkey_isblob = FALSE;
        dbe_trxid_t stmttrxid;
        rs_reltype_t reltype;
        long timeout;
        bool optimistic_lock;
        bool isblob;
        bool tref_built = FALSE;
        bool uselocking;
        dbe_trxnum_t trxnum  = DBE_TRXNUM_NULL;
        bool clustkey_changed = FALSE;
        dbe_indexop_mode_t mode = 0;

        SS_PUSHNAME("tuple_insert_tval_1");

        ss_assert(trx != DBE_TRX_NOTRX);
        ss_assert(trx != DBE_TRX_HSBTRX);
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));
        ss_dassert(opertype == OPER_INS || opertype == OPER_UPD);

        reltype = rs_relh_reltype(cd, relh);
        switch (reltype) {
            case RS_RELTYPE_OPTIMISTIC:
            case RS_RELTYPE_PESSIMISTIC:
                index = dbe_db_getindex(rs_sysi_db(cd));
                break;
            default:
                ss_error;
        }

        ss_dprintf_3(("tuple_insert_tval\n"));
        ss_assert(trx != NULL);

        stmttrxid = dbe_trx_getstmttrxid(trx);
        index = dbe_db_getindex(rs_sysi_db(cd));
        ttype = rs_relh_ttype(cd, relh);
        clustkey = rs_relh_clusterkey(cd, relh);
        ss_dassert(clustkey != NULL);

        rc = tuple_insblobattrs(
                cd,
                index,
                relh,
                ttype,
                tval,
                clustkey,
                stmttrxid);

        if (rc != DBE_RC_SUCC) {
            ss_dprintf_4(("tuple_insert_tval:tuple_insblobattrs failed, rc = %s (%d)\n",
                su_rc_nameof(rc), rc));
            SS_POPNAME;
            return(rc);
        }

#ifdef DBE_UPDATE_OPTIMIZATION
        if (opertype == OPER_UPD && dbe_trx_keypartsupdated(cd, clustkey, -1, upd_attrs)) {
            /* Reference key parts updated, update all key parts. */
            clustkey_changed = TRUE;
        }
#endif

        keys = rs_relh_keys(cd, relh);

        rc = tuple_buildkey_tval(
                cd,
                clustkey,
                ttype,
                tval,
                TRUE,
                stmttrxid,
                NULL,
                &clustkey_dvtpl,
                &clustkey_isblob);

        SS_POPNAME;
        SS_PUSHNAME("tuple_insert_tval_2");

#ifndef SS_NOLOCKING

        SS_PUSHNAME("dbe_trx_uselocking");
        uselocking = dbe_trx_uselocking(
                        trx,
                        relh,
                        LOCK_X,
                        &timeout,
                        &optimistic_lock);
        SS_POPNAME;
        if (uselocking && optimistic_lock) {
            /* In optimistic case inserted tuples are not locked.
             */
            uselocking = FALSE;
        }

        if (rc == DBE_RC_SUCC && uselocking) {
            dbe_lock_reply_t reply;
            dbe_tref_t* tmp_tref = NULL;
            bool newlock;

            ss_dprintf_4(("tuple_insert_tval:uselocking\n"));

            if (tref == NULL) {
                /* Generate tuple reference. */
                tmp_tref = dbe_tref_init();
                tref = tmp_tref;
            }
            dbe_tref_buildclustkeytref(
                cd,
                tref,
                clustkey,
                clustkey_dvtpl,
                usertrxid);
            tref_built = TRUE;

            reply = dbe_trx_lock(
                        trx,
                        relh,
                        tref,
                        LOCK_X,
                        timeout,
                        optimistic_lock,
                        &newlock);

            if (tmp_tref != NULL) {
                dbe_tref_done(cd, tmp_tref);
                tmp_tref = tref = NULL;
            }

            switch (reply) {
                case LOCK_OK:
                    break;
                case LOCK_WAIT:
                   rc = DBE_RC_WAITLOCK;
                   break;
                case LOCK_TIMEOUT:
                case LOCK_DEADLOCK:
                    /* We may accidentally get a deadlock if lock
                     * names conflict.
                     */
                    ss_dassert(!optimistic_lock);
                    dbe_trx_setdeadlock_noenteraction(trx);
                    rc = DBE_ERR_DEADLOCK;
                    break;
                default:
                    ss_error;
            }
        }
#endif /* SS_NOLOCKING */

        SS_POPNAME;
        SS_PUSHNAME("tuple_insert_tval_3");

        /* Build all key values.
         */
        if (rc == DBE_RC_SUCC) {

            ss_dprintf_4(("tuple_insert_tval:Build all key values\n"));

            su_pa_do_get(keys, i, key) {
                bool isclustering;

                ss_dprintf_4(("tuple_insert_tval:key %s\n", rs_key_name(cd, key)));

                if (dbe_trx_forcecommit(trx)) {
                    mode = DBE_INDEXOP_COMMITTED;
                    trxnum = dbe_trx_getcommittrxnum(trx);
                } else {
                    mode = 0;
                }

                isclustering = rs_key_isclustering(cd, key);
                if (isclustering) {
                    ss_dassert(clustkey_dvtpl != NULL);
                    isblob = clustkey_isblob;
                    dynvtpl_free(&dvtpl);
                    dvtpl = clustkey_dvtpl;
                    clustkey_dvtpl = NULL;
                    mode |= DBE_INDEXOP_CLUSTERING;
                } else {
                    rc = tuple_buildkey_tval(
                            cd,
                            key,
                            ttype,
                            tval,
                            FALSE,
                            stmttrxid,
                            clustkey_changed ? NULL : upd_attrs,
                            &dvtpl,
                            &isblob);
                }
#ifdef DBE_UPDATE_OPTIMIZATION
                if (rc == DBE_RC_NOTFOUND) {
                    ss_dassert(opertype == OPER_UPD);
                    continue;
                }
#endif
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_4(("tuple_insert_tval:tuple_buildkey_tval failed, rc = %s (%d)\n",
                        su_rc_nameof(rc), rc));
                    break;
                }
                if (isblob) {
                    mode |= DBE_INDEXOP_BLOB;
                }
                if (isnocheck) {
                    mode |= DBE_INDEXOP_NOCHECK;
                }

                if (opertype == OPER_UPD) {
#ifdef DBE_UPDATE_OPTIMIZATION
                    ss_dassert(clustkey_changed || 
                               (mode & DBE_INDEXOP_CLUSTERING) ||
                               rs_key_isclustering(cd, key) ||
                               rs_key_istupleversion(cd, key) ||
                               dbe_trx_keypartsupdated(cd, key, -1, upd_attrs));
                    mode |= DBE_INDEXOP_UPDATE;
#else
                    if (!dbe_trx_keypartsupdated(cd, key, -1, upd_attrs)) {
                        mode |= DBE_INDEXOP_UPDATE;
                    }
#endif
                    if (isnocheck && prelocked) {
                        mode |= DBE_INDEXOP_PRELOCKED;
                    }
                }

                rc = dbe_index_insert(
                            index,
                            rs_key_id(cd, key),
                            dvtpl,
                            trxnum,
                            stmttrxid,
                            mode,
                            cd);

                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_4(("tuple_insert_tval:index insert failed, rc = %s (%d)\n",
                        su_rc_nameof(rc), rc));

                    if (rc == DBE_ERR_UNIQUE_S && trx != DBE_TRX_NOTRX && trx != DBE_TRX_HSBTRX) {
                        dbe_trx_seterrkey(trx, key);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                        dbe_trx_builduniquerrorkeyvalue(
                            trx, 
                            relh, 
                            key, 
                            dvtpl,
                            0);
#endif
                    }
                    break;
                }

                if (isclustering) {
                    /* Update table cardinal.
                     */
                    long nbytes;
                    nbytes = (long)vtpl_grosslen(dvtpl);
                    if (isnocheck && opertype == OPER_UPD && ci != NULL) {
                        ci->ci_nrows++;
                        ci->ci_nbytes += nbytes;
                    } else if (isnocheck) {
                        rs_relh_insertbytes(cd, relh, nbytes, 1);
                    } else {
                        ss_dassert(cd != NULL);
                        dbe_trx_insertbytes(cd, relh, nbytes, 1);
                    }
                    if (tref != NULL && !tref_built) {
                        /* Generate tuple reference. */
                        dbe_tref_buildclustkeytref(
                            cd,
                            tref,
                            key,
                            dvtpl,
                            usertrxid);
                    }
                }

                if (!isnocheck) {
                    rc = dbe_trx_addwrite(
                            trx,
                            TRUE,
                            key,
                            tref,
                            tuple_getmergekeycount(dvtpl),
                            FALSE,
                            relh,
                            reltype);
                    if (rc != DBE_RC_SUCC) {
                        ss_dprintf_4(("tuple_insert_tval:dbe_trx_addwrite failed, rc = %s (%d)\n",
                            su_rc_nameof(rc), rc));
                        break;
                    }
                    if (rs_key_isunique(cd, key)) {

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC))
		      if (!trx_uniquecheck_isnull(cd,key,dvtpl)) {
                        rc = dbe_trx_adduniquecheck(
                                cd,
                                trx,
                                relh,
                                key,
                                dvtpl,
                                upd_attrs,
                                reltype);
		      }	
#else
                      rc = dbe_trx_adduniquecheck(
                                cd,
                                trx,
                                relh,
                                key,
                                dvtpl,
                                upd_attrs,
                                reltype);
#endif /* SS_MYSQL && SS_MULTIPLE_NULLS */

                      if (rc != DBE_RC_SUCC) {
                            ss_dprintf_4(("tuple_insert_tval:dbe_trx_adduniquecheck failed, rc = %s (%d)\n",
                                su_rc_nameof(rc), rc));
                            break;
                      }
                    }
                }
                if (isclustering) {
                    ss_dassert(clustkey_dvtpl == NULL);
                    clustkey_dvtpl = dvtpl;
                    dvtpl = NULL;
                }
            } /* su_pa_do(keys) */
        } /* if (rc == DBE_RC_SUCC) */
        dynvtpl_free(&dvtpl);
        SS_POPNAME;
        SS_PUSHNAME("tuple_insert_tval_4");

#ifdef DBE_UPDATE_OPTIMIZATION
        if (rc == DBE_RC_NOTFOUND) {
            ss_dassert(opertype == OPER_UPD);
            rc = DBE_RC_SUCC;
        }
#endif

#ifdef REFERENTIAL_INTEGRITY
        if (rc == DBE_RC_SUCC && !isnocheck) {
            /* Add key reference (referential integrity) checks.
             */
            su_pa_t* refkeys;
            vtpl_vamap_t* clustkey_vamap = NULL;
            refkeys = rs_relh_refkeys(cd, relh);
            ss_dprintf_4(("tuple_insert_tval:Add referential integrity checks\n"));
            su_pa_do_get(refkeys, i, key) {
                if (rs_key_type(cd, key) == RS_KEY_FORKEYCHK) {
                    if (clustkey_vamap == NULL) {
                        clustkey_vamap = vtpl_vamap_init(clustkey_dvtpl);
                        if (clustkey_isblob) {
                            vtpl_vamap_removelast(clustkey_vamap);
                        }
                    }
                    rc = dbe_trx_addrefkeycheck(
                            cd,
                            trx,
                            relh,
                            clustkey,
                            key,
                            relh,
                            clustkey_vamap,
                            upd_attrs,
                            reltype);
                    if (rc != DBE_RC_SUCC) {
                        ss_dprintf_4(("tuple_insert_tval:dbe_trx_addrefkeycheck failed, rc = %s (%d)\n",
                            su_rc_nameof(rc), rc));
                        break;
                    }
                }
            }
            if (clustkey_vamap != NULL) {
                vtpl_vamap_done(clustkey_vamap);
            }
        }
#endif /* REFERENTIAL_INTEGRITY */

        if (rc == DBE_RC_SUCC) {
            ss_dprintf_4(("tuple_insert_tval:Add to log\n"));
            if (addtolog) {
                rc = dbe_trx_addtolog(
                        trx,
                        TRUE,
                        clustkey,
                        tref,
                        clustkey_dvtpl,
                        relh,
                        clustkey_isblob);
            }
#ifdef DBE_HSB_REPLICATION
            if (rc == DBE_RC_SUCC) {
                CHK_TS(ts);
                if (tref != NULL) {
                    ts->ts_tref = tref;
                }
                ts->ts_insdvtpl = clustkey_dvtpl;
                ts->ts_insdynvtplp = TRUE;
                ts->ts_relh = relh;
                ts->ts_isblobattrs = clustkey_isblob;
                CHK_TS(ts);
                clustkey_dvtpl = NULL;
            }
#endif /* DBE_HSB_REPLICATION */
        }
        dynvtpl_free(&clustkey_dvtpl);

        ss_dprintf_4(("tuple_insert_tval, rc = %s (%d)\n", su_rc_nameof(rc), rc));

        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_insert_disk
 *
 * Inserts a new tuple into relation.
 *
 * Parameters :
 *
 *	cd - in, use
 *		Client data object.
 *
 *	trx - in out, use
 *		Transaction handle. Can be DBE_TRX_NOTRX, if transactions
 *		are not used.
 *
 *	usertrxid - in
 *		User transaction id of the new tuple.
 *
 *	relh - in, use
 *		Relation handle specifying the relation into where the
 *		tuple is added. The key definitions are also found from
 *		this handle.
 *
 *	tval - in, use
 *		Tuple value of the new inserted tuple. This must be of
 *		same type as the tuple value of relation handle.
 *
 *	type - in
 *		Insert type.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_EXISTS
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_tuple_insert_disk(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_relh_t* relh,
        rs_tval_t* tval,
        dbe_tupleinsert_t type)
{
        dbe_ret_t rc = 0;
        dbe_db_t* db;
        dbe_tuplestate_t *ts;

        SS_PUSHNAME("dbe_tuple_insert");

        ss_dprintf_1(("*** dbe_tuple_insert_disk: BEGIN, userid = %d, usertrxid = %ld\n", rs_sysi_userid(cd), DBE_TRXID_GETLONG(usertrxid)));
        ss_dprintf_4(("dbe_tuple_insert_disk: relname = %s\n", rs_relh_name(cd, relh)));

        ts = dbe_trx_gettuplestate(trx);
        if(ts == NULL) {
            ts = dbe_tuplestate_init();
            dbe_trx_settuplestate(trx, ts);
        }
        CHK_TS(ts);

        db = rs_sysi_db(cd);

        switch (ts->ts_oper) {
            case TUPLE_LOCKRELH:
                if (dbe_trx_isreadonly(trx)) {
                    rc = DBE_ERR_TRXREADONLY;
                    break;
                }
                rc = dbe_trx_lockrelh(trx, relh, FALSE, -1L);
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_INSERT_INIT;
                /* FALLTHROUGH */

            case TUPLE_INSERT_INIT:
            {
                rs_ttype_t* ttype;
                rs_ano_t i;
                rs_ano_t nattrs;

                ttype = rs_relh_ttype(cd, relh);
                nattrs = rs_ttype_nattrs(cd, ttype);

                if (type != DBE_TUPLEINSERT_REPLICA) {
                    /* Set special attributes.
                     */
                    for (i = 0; i < nattrs; i++) {
                        rs_atype_t* atype;
                        rs_aval_t* aval;
                        rs_tuplenum_t tuplenum;
                        ss_int8_t i8;
                        va_t tuplenum_va;
                        rs_key_t* clustkey;

                        atype = rs_ttype_atype(cd, ttype, i);

                        switch (rs_atype_attrtype(cd, atype)) {

                            case RSAT_TUPLE_ID:
                                if (rs_atype_datatype(cd, atype) == RSDT_BINARY) {
                                    tuplenum = dbe_counter_getnewtuplenum(dbe_db_getcounter(db));
                                    rs_tuplenum_getva(&tuplenum, &tuplenum_va);
                                    rs_tval_setva(cd, ttype, tval, i, &tuplenum_va);
                                } else {
                                    ss_dassert(rs_atype_datatype(cd, atype) == RSDT_BIGINT);
                                    aval = rs_tval_aval(cd, ttype, tval, i);
                                    i8 = dbe_counter_getnewint8tuplenum(dbe_db_getcounter(db));
                                    rs_aval_setint8_raw(cd, atype, aval, i8, NULL);
                                }
                                break;
                            case RSAT_TUPLE_VERSION:
                                clustkey = rs_relh_clusterkey(cd, relh);
                                if (rs_key_isprimary(cd, clustkey)) {
                                    /* There is no TUPLE_ID, add unique tuple version.
                                    */
                                    if (rs_atype_datatype(cd, atype) == RSDT_BINARY) {
                                        rs_tuplenum_t tupleversion;
                                        va_t tupleversion_va;
                                        tupleversion = dbe_counter_getnewtupleversion(dbe_db_getcounter(db));
                                        rs_tuplenum_getva(&tupleversion, &tupleversion_va);
                                        rs_tval_setva(cd, ttype, tval, i, &tupleversion_va);
                                    } else {
                                        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_BIGINT);
                                        i8 = dbe_counter_getnewint8tupleversion(dbe_db_getcounter(db));
                                        aval = rs_tval_aval(cd, ttype, tval, i);
                                        rs_aval_setint8_raw(cd, atype, aval, i8, NULL);
                                    }
                                } else {
                                    /* There is TUPLE_ID, add NULL tuple version as the
                                    * the initial version number.
                                    */
                                    aval = rs_tval_aval(cd, ttype, tval, i);
                                    rs_aval_setnull(cd, atype, aval);
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
                ts->ts_oper = TUPLE_INSERT;
                /* FALLTHROUGH */
            }

            case TUPLE_INSERT:
            {
                bool ispessimistic;

                ispessimistic = (rs_relh_reltype(cd, relh) == RS_RELTYPE_PESSIMISTIC);
                if (ispessimistic && dbe_cfg_usepessimisticgate) {
                    SS_PUSHNAME("rs_relh_pessgate_enter_exclusive");
                    rs_relh_pessgate_enter_exclusive(cd, relh);
                    SS_POPNAME;
                }

                rc = tuple_insert_tval(
                        OPER_INS,
                        cd,
                        trx,
                        usertrxid,
                        relh,
                        tval,
                        NULL,
                        ts,
                        dbe_tuple_isnocheck(cd, trx, relh),
                        NULL,
                        NULL,
                        TRUE,
                        FALSE);

                if (ispessimistic && dbe_cfg_usepessimisticgate) {
                    SS_PUSHNAME("rs_relh_pessgate_exit");
                    rs_relh_pessgate_exit(cd, relh);
                    SS_POPNAME;
                }

                if (rc != DBE_RC_SUCC) {
                    break;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_HOTSTANDBY_INS;
                /* FALLTHROUGH */
            }
            case TUPLE_HOTSTANDBY_INS:
                CHK_TS(ts);
                FAKE_IF(FAKE_DBE_HSBREPBLOBFAILURE) {
                    if (ts->ts_isblobattrs) {
                        rc = DBE_ERR_HSBBLOB;
                        break;
                    }
                }
                rc = dbe_trx_reptuple(
                        trx,
                        TRUE,
                        ts->ts_tref,
                        ts->ts_insdvtpl,
                        ts->ts_relh,
                        ts->ts_isblobattrs);
                break;
            default:
                ss_rc_error(ts->ts_oper);
        }

        if (rc != DBE_RC_CONT && rc != DBE_RC_WAITLOCK) {
            CHK_TS(ts);
            dbe_tuplestate_done(ts);
            dbe_trx_settuplestate(trx, NULL);
        }

        ss_dprintf_1(("*** dbe_tuple_insert_disk: RETURN, rc = %s\n", su_rc_nameof(rc)));

        SS_POPNAME;
        return(rc);
}

#ifndef SS_NOLOGGING

/*##**********************************************************************\
 *
 *		dbe_tuple_recovinsert
 *
 * Inserts a new tuple into relation using a clustering key v-tuple.
 *
 * Parameters :
 *
 *	cd - in, use
 *		Client data object.
 *
 *	trx - in out, use
 *		Transaction handle. Can be DBE_TRX_NOTRX, if transactions
 *		are not used.
 *
 *	trxnum -
 *
 *	stmttrxid - in
 *		Statement transaction id of the new tuple.
 *
 *	relh - in, use
 *		Relation handle specifying the relation into where the
 *		tuple is added. The key definitions are also found from
 *		this handle.
 *
 *	clustkey_vtpl - in, use
 *		V-tuple of the clustering key of the relation.
 *
 *	clustkey_isblob - in
 *		TRUE if clustering key contains blob attributes
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_EXISTS
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_tuple_recovinsert(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxnum_t trxnum,
        dbe_trxid_t stmttrxid,
        rs_relh_t* relh,
        vtpl_t* clustkey_vtpl,
        bool clustkey_isblob)
{
        dbe_ret_t rc;
        dbe_trxid_t usertrxid;
        dbe_gobj_t* go;

        ss_dprintf_1(("dbe_tuple_recovinsert, %s, stmttrxid=%ld\n", rs_relh_name(cd, relh), DBE_TRXID_GETLONG(stmttrxid)));
        ss_dassert(!rs_sysi_testflag(cd, RS_SYSI_FLAG_STORAGETREEONLY));

        ss_dprintf_4(("\n", dbe_tuple_printvtpl(
                        cd,
                        relh,
                        clustkey_vtpl,
                        FALSE,
                        FALSE)));

        /* Get the user transaction id.
         */
        go = dbe_db_getgobj(rs_sysi_db(cd));
        dbe_trxbuf_gettrxstate(
            go->go_trxbuf,
            stmttrxid,
            NULL,
            &usertrxid);

        rc = tuple_insordel_vtpl(
                OPER_INS,
                cd,
                trx,
                trxnum,
                usertrxid,
                DBE_TRXID_NULL,
                stmttrxid,
                relh,
                clustkey_vtpl,
                NULL,
#ifdef DBE_HSB_REPLICATION
                NULL,
#endif /* DBE_HSB_REPLICATION */
                clustkey_isblob,
                dbe_tuple_isnocheck(cd, trx, relh),
                NULL,
                NULL,
                /* Doesn't really matter if we count this as an update
                   or not. */
                FALSE,
                TRUE,
                FALSE);

        return(rc);
}

#endif /* SS_NOLOGGING */

/*#***********************************************************************\
 *
 *		tuple_lock
 *
 * Locks the tuple specified by tref, if locking is used in this table.
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trx -
 *
 *
 *	relh -
 *
 *
 *	tref_vtpl -
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
static dbe_ret_t tuple_lock(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* tref)
{
#ifndef SS_NOLOCKING
        long timeout;
        bool optimistic_lock;

        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));
        ss_dassert(!rs_sysi_testflag(cd, RS_SYSI_FLAG_STORAGETREEONLY));

        if (dbe_trx_uselocking(trx, relh, LOCK_X, &timeout, &optimistic_lock)) {
            dbe_lock_reply_t reply;
            bool newlock;

            reply = dbe_trx_lock(
                        trx,
                        relh,
                        tref,
                        LOCK_X,
                        timeout,
                        optimistic_lock,
                        &newlock);

#ifdef SS_MYSQL
            ss_dassert(!newlock);
#endif
            if (newlock) {
                reply = LOCK_DEADLOCK;
            }
            switch (reply) {
                case LOCK_OK:
                    return(dbe_trx_checkoldupdate(trx, relh, tref));
                case LOCK_TIMEOUT:
                case LOCK_DEADLOCK:
                    if (optimistic_lock) {
                        ss_dprintf_4(("*** tuple_lock: RETURN, rc = DBE_ERR_LOSTUPDATE\n"));
                        return(DBE_ERR_LOSTUPDATE);
                    } else {
                        dbe_trx_setdeadlock_noenteraction(trx);
                        ss_dprintf_4(("*** tuple_lock: RETURN, rc = DBE_ERR_DEADLOCK\n"));
                        return(DBE_ERR_DEADLOCK);
                    }
                case LOCK_WAIT:
                    ss_dprintf_4(("*** tuple_lock: RETURN, rc = DBE_RC_WAITLOCK\n"));
                    return(DBE_RC_WAITLOCK);
                default:
                    ss_error;
            }
        }
#endif /* SS_NOLOCKING */

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_delete_disk
 *
 * Deletes a data tuple from the database. The deleted tuple is identified
 * by tuple reference returned from the search.
 *
 * Parameters :
 *
 *	cd - in, use
 *		Client data object.
 *
 *	trx - in out, use
 *		Transaction handle. Can be DBE_TRX_NOTRX, if transactions
 *		are not used.
 *
 *	usertrxid - in, use
 *		User transaction id.
 *
 *	relh - in, use
 *		Relation handle for the deleted tuple.
 *
 *	tref - in, use
 *		Tuple reference specifying the deleted tuple.
 *
 *	search - in, use
 *	    If non-NULL, the search from where the tuple is deleted.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_NOTFOUND
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_tuple_delete_disk(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_relh_t* relh,
        dbe_tref_t* tref,
        dbe_search_t* search)
{
        dbe_ret_t rc = 0;
        dbe_tuplestate_t* ts;
        bool isupdatable = TRUE;

        ss_dprintf_1(("*** dbe_tuple_delete_disk: BEGIN, userid = %d, usertrxid = %ld\n", rs_sysi_userid(cd), DBE_TRXID_GETLONG(usertrxid)));

        if (search != NULL && !dbe_search_isactive(search, &isupdatable)) {
            if (!isupdatable) {
                ss_dprintf_1(("*** dbe_tuple_delete_disk: RETURN at %d, rc = E_DELNEEDSFORUPDATE\n", __LINE__));
                return(E_DELNEEDSFORUPDATE);
            } else {
                ss_dprintf_1(("*** dbe_tuple_delete_disk: RETURN at %d, rc = DBE_ERR_NOACTSEARCH\n", __LINE__));
                return(DBE_ERR_NOACTSEARCH);
            }
        }
        if (!isupdatable) {
            ss_dprintf_1(("*** dbe_tuple_update_disk: RETURN, rc = E_UPDNEEDSFORUPDATE\n"));
            return(E_UPDNEEDSFORUPDATE);
        }

        ts = dbe_trx_gettuplestate(trx);
        if(ts == NULL) {
            ts = dbe_tuplestate_init();
            dbe_trx_settuplestate(trx, ts);
        }
        CHK_TS(ts);

        switch (ts->ts_oper) {
            case TUPLE_LOCKRELH:
                if (dbe_trx_isreadonly(trx)) {
                    rc = DBE_ERR_TRXREADONLY;
                    break;
                }
                rc = dbe_trx_lockrelh(trx, relh, FALSE, -1L);
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_LOCKROW;
                /* FALLTHROUGH */

            case TUPLE_LOCKROW:
            {
                dbe_ret_t lockrc;

                lockrc = tuple_lock(cd, trx, relh, tref);
                if (lockrc != DBE_RC_SUCC) {
                    rc = lockrc;
                    break;
                }

                CHK_TS(ts);
                ts->ts_oper = TUPLE_DELETE;
#ifdef SS_DEBUG
                if (rs_relh_reltype(cd, relh) == RS_RELTYPE_PESSIMISTIC) {
                    dbe_ret_t rc;
                    rc = dbe_trx_checklostupdate(trx, tref, ts->ts_clustkey, TRUE);
                    su_rc_dassert(rc == DBE_RC_SUCC || rs_sysi_getallowduplicatedelete(cd), rc);
                }
#endif /* SS_DEBUG */
                /* FALLTHROUGH */
            }

            case TUPLE_DELETE:
            {
                dbe_datasea_t* datasea = NULL;
                dbe_btrsea_timecons_t tc;
                dbe_srk_t* srk;
                rs_reltype_t reltype;

                rc = DBE_RC_FOUND;

                reltype = rs_relh_reltype(cd, relh);

                if (search == NULL || !dbe_search_getclustvtpl(search, &srk)) {
                    rs_key_t* clustkey;
                    dbe_index_t* index = NULL;
                    clustkey = rs_relh_clusterkey(cd, relh);
                    switch (reltype) {
                        case RS_RELTYPE_OPTIMISTIC:
                        case RS_RELTYPE_PESSIMISTIC:
                            index = dbe_db_getindex(rs_sysi_db(cd));
                            break;
                        default:
                            ss_error;
                    }
                    tc.tc_maxtrxnum = (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX)
                                        ? DBE_TRXNUM_MAX
                                        : tref->tr_readlevel;
                    tc.tc_mintrxnum = DBE_TRXNUM_MIN;
                    tc.tc_usertrxid = usertrxid;
                    tc.tc_maxtrxid = DBE_TRXID_MAX;
                    tc.tc_trxbuf = NULL;

                    /* Find the data tuple from the index.
                     */
                    datasea = dbe_datasea_init(
                                    cd,
                                    index,
                                    clustkey,
                                    &tc,
                                    NULL,
                                    FALSE,
                                    "dbe_tuple_delete_disk");

                    rc = dbe_datasea_search(
                                datasea,
                                tref->tr_vtpl,
                                (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX)
                                    ? DBE_TRXID_NULL
                                    : dbe_trx_getstmttrxid(trx),
                                &srk);

                    switch (rc) {
                        case DBE_RC_FOUND:
                            break;
                        case DBE_RC_END:
                            rc = DBE_ERR_NOTFOUND;
                            break;
                        case DBE_ERR_DEADLOCK:
                            break;
                        case DBE_ERR_HSBSECONDARY:
                            break;
                        default:
                            su_rc_derror(rc);
                            break;
                    }
                }

                if (rc != DBE_RC_FOUND) {
                    if (datasea != NULL) {
                        dbe_datasea_done(datasea);
                    }
                    break;
                }

                if (reltype == RS_RELTYPE_PESSIMISTIC && dbe_cfg_usepessimisticgate) {
                    rs_relh_pessgate_enter_exclusive(cd, relh);
                }

                {
                    su_list_t deferred_blob_unlink_list_buf;
                    su_list_t* deferred_blob_unlink_list = NULL;
                    bool is_nocheck = dbe_tuple_isnocheck(cd, trx, relh);
                    /* Delete the tuple.
                     */
                    if (is_nocheck) {
                        deferred_blob_unlink_list =
                            rs_sysi_getdeferredblobunlinklist(cd);
                        ss_dassert(deferred_blob_unlink_list == NULL);
                        if (deferred_blob_unlink_list == NULL) {
                            deferred_blob_unlink_list =
                                &deferred_blob_unlink_list_buf;
                            su_list_initbuf(
                                    &deferred_blob_unlink_list_buf,
                                    NULL);
                            rs_sysi_setdeferredblobunlinklist(
                                    cd,
                                    deferred_blob_unlink_list);
                        }
                    }
                    rc = tuple_insordel_vtpl(
                            OPER_DEL,
                            cd,
                            trx,
                            DBE_TRXNUM_NULL,
                            usertrxid,
                            dbe_srk_getkeytrxid(srk),
                            dbe_trx_getstmttrxid(trx),
                            relh,
                            dbe_srk_getvtpl(srk),
                            tref,
                            ts,
                            dbe_srk_isblob(srk),
                            is_nocheck,
                            NULL,
                            NULL,
                            FALSE,
                            TRUE,
                            FALSE);
                    if (is_nocheck) {
                        dbe_blobg2_unlink_list_of_blobids(
                                cd,
                                deferred_blob_unlink_list,
                                NULL);
                        if (deferred_blob_unlink_list ==
                            &deferred_blob_unlink_list_buf)
                        {
                            rs_sysi_setdeferredblobunlinklist(cd, NULL);
                            su_list_donebuf(deferred_blob_unlink_list);
                        }
                    }
                }

                if (reltype == RS_RELTYPE_PESSIMISTIC && dbe_cfg_usepessimisticgate) {
                    rs_relh_pessgate_exit(cd, relh);
                }

                if (datasea != NULL) {
                    CHK_TS(ts);
                    ts->ts_datasea = datasea;
                }
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_HOTSTANDBY_DEL;
                /* FALLTHROUGH */
            }
            case TUPLE_HOTSTANDBY_DEL:
                CHK_TS(ts);
                rc = dbe_trx_reptuple(
                        trx,
                        FALSE,
                        ts->ts_tref,
                        ts->ts_deldvtpl,
                        ts->ts_relh,
                        ts->ts_isblobattrs);
                break;
            default:
                ss_rc_error(ts->ts_oper);
        }

        if (rc != DBE_RC_CONT && rc != DBE_RC_WAITLOCK) {
            CHK_TS(ts);
            dbe_tuplestate_done(ts);
            dbe_trx_settuplestate(trx, NULL);
        }

        ss_dprintf_1(("*** dbe_tuple_delete_disk: RETURN at %d, rc = %s\n", __LINE__, su_rc_nameof(rc)));

        return(rc);
}

#ifndef SS_NOLOGGING

/*##**********************************************************************\
 *
 *		dbe_tuple_recovdelete
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trx -
 *
 *
 *	trxnum -
 *
 *
 *	stmttrxid -
 *
 *
 *	relh -
 *
 *
 *	tref_vtpl -
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
dbe_ret_t dbe_tuple_recovdelete(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxnum_t trxnum,
        dbe_trxid_t stmttrxid,
        rs_relh_t* relh,
        vtpl_t* tref_vtpl,
        bool hsbrecov __attribute__ ((unused)))
{
        dbe_index_t* index = NULL;
        dbe_ret_t rc;
        dbe_datasea_t* datasea;
        dbe_btrsea_timecons_t tc;
        dbe_srk_t* srk;
        dbe_tref_t tref;
        dbe_gobj_t* go;
        rs_key_t* clustkey;

        clustkey = rs_relh_clusterkey(cd, relh);

        switch (rs_relh_reltype(cd, relh)) {
            case RS_RELTYPE_OPTIMISTIC:
            case RS_RELTYPE_PESSIMISTIC:
                index = dbe_db_getindex(rs_sysi_db(cd));
                break;
            default:
                ss_error;
        }

        ss_dprintf_1(("dbe_tuple_recovdelete, %s, stmttrxid=%ld\n", rs_relh_name(cd, relh), DBE_TRXID_GETLONG(stmttrxid)));
        ss_dassert(!rs_sysi_testflag(cd, RS_SYSI_FLAG_STORAGETREEONLY));

        dbe_tref_initrecov(&tref, tref_vtpl);

        ss_output_2(dbe_tuple_printvtpl(
                        cd,
                        relh,
                        tref.tr_vtpl,
                        TRUE,
                        FALSE));

        /* Get the user transaction id.
         */
        go = dbe_db_getgobj(rs_sysi_db(cd));
        dbe_trxbuf_gettrxstate(
            go->go_trxbuf,
            stmttrxid,
            NULL,
            &tc.tc_usertrxid);

        tc.tc_maxtrxnum = (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX)
                            ? DBE_TRXNUM_MAX
                            : dbe_trx_getmaxtrxnum(trx);
        tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        tc.tc_maxtrxid = DBE_TRXID_MAX;
        tc.tc_trxbuf = NULL;

        /* Find the data tuple from the index.
         */
        datasea = dbe_datasea_init(
                        cd,
                        index,
                        clustkey,
                        &tc,
                        NULL,
                        FALSE,
                        "dbe_tuple_recovdelete");

        rc = dbe_datasea_search(
                    datasea,
                    tref.tr_vtpl,
                    stmttrxid,
                    &srk);

        switch (rc) {
            case DBE_RC_FOUND:
                /* Delete the tuple.
                 */
                rc = tuple_insordel_vtpl(
                        OPER_DEL,
                        cd,
                        trx,
                        trxnum,
                        tc.tc_usertrxid,
                        dbe_srk_getkeytrxid(srk),
                        stmttrxid,
                        relh,
                        dbe_srk_getvtpl(srk),
                        &tref,
#ifdef DBE_HSB_REPLICATION
                        NULL,
#endif /* DBE_HSB_REPLICATION */
                        dbe_srk_isblob(srk),
                        dbe_tuple_isnocheck(cd, trx, relh),
                        NULL,
                        NULL,
                        FALSE,
                        TRUE,
                        FALSE);
                break;
            case DBE_RC_END:
                ss_rc_dassert(hsbrecov, DBE_TRXID_GETLONG(tc.tc_usertrxid));
                rc = DBE_ERR_NOTFOUND;
                break;
            default:
                su_rc_derror(rc);
                break;
        }
        dbe_datasea_done(datasea);

        return(rc);
}

#endif /* SS_NOLOGGING */

/*#***********************************************************************\
 *
 *		tuple_updatetval_vtpl
 *
 * Updates a tval by adding those attributes that are not in it from a
 * v-tuple. The given v-tuple must the be clustering key entry.
 *
 * Parameters :
 *
 *	cd - in, use
 *		Client data.
 *
 *	relh - in, use
 *		Relation handle for the deleted tuple.
 *
 *	new_attrs - in, use
 *		Boolean array containg TRUE for those attributes
 *          that has been updated and are available in new_tval.
 *
 *	new_tval - in out, use
 *		New tuple values containing the updated attributes.
 *		Attributes that are not updated are taken from
 *		old_clustkey_vtpl and added to this tuple value.
 *
 *	trxid - in
 *		User transaction id.
 *
 *	old_clustkey_vtpl - in
 *		Clustering key value of the old version of updated
 *		tuple.
 *
 *	old_clustkey_isblob - in
 *		If TRUE, the old clustkey contains blob attributes.
 *
 *	old_clustkey_vamap - in
 *		va map of old clustering key
 *
 *	counter - use
 *		counter object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t tuple_updatetval_vtpl(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        bool* new_attrs,
        rs_tval_t* new_tval,
        dbe_trxid_t trxid __attribute__ ((unused)),
        vtpl_t* old_clustkey_vtpl __attribute__ ((unused)),
        vtpl_vamap_t* old_clustkey_vamap,
        dbe_counter_t* counter)
{
        rs_ttype_t* ttype;
        rs_key_t* clustkey;
        uint i;
        uint nparts;

        SS_PUSHNAME("tuple_updatetval_vtpl");
        ss_dassert(new_attrs != NULL);

        ttype = rs_relh_ttype(cd, relh);
        clustkey = rs_relh_clusterkey(cd, relh);
        ss_dassert(clustkey != NULL);

        nparts = rs_key_nparts(cd, clustkey);

        /* Scan through all key parts in clustering key and update
         * those attributes in new_tval that has not been changed.
         */
        for (i = 0; i < nparts; i++) {
            uint ano;
            rs_atype_t* atype;
            rs_aval_t* aval;
            rs_attrtype_t kptype;
            rs_tuplenum_t tupleversion;
            va_t* va;
            va_t* new_va;
            va_t tupleversion_va;

            va = vtpl_vamap_getva_at(old_clustkey_vamap, i);

            kptype = rs_keyp_parttype(cd, clustkey, i);
            ano = rs_keyp_ano(cd, clustkey, i);

            switch (kptype) {

                case RSAT_SYNC:
                case RSAT_USER_DEFINED:
#ifdef SS_COLLATION
                case RSAT_COLLATION_KEY:
#endif /* SS_COLLATION */
                    if (new_attrs[ano]) {
                        /* User defined attribute that has been changed */
                        if (rs_sysi_testflag(cd, RS_SYSI_FLAG_MYSQL)) {
                            /* Compare actual values to check if column is
                             * really changed.
                             */
                            new_va = rs_tval_va(cd, ttype, new_tval, ano);
                            if (va_compare(va, new_va) == 0) {
                                /* Value is not really changed, as a side effect clear 
                                 * changed flag in new_attrs array.
                                 */
                                new_attrs[ano] = FALSE;
                                ss_dprintf_4(("tuple_updatetval_vtpl:column %d not really changed\n", ano));
                            }
                        }
                        break;
                    }
                    /* FALLTHROUGH! */

                case RSAT_TUPLE_ID:
                    ss_bassert(!new_attrs[ano]);
                    atype = rs_ttype_atype(cd, ttype, ano);
                    aval = rs_tval_aval(cd, ttype, new_tval, ano);
                    rs_aval_setva(cd, atype, aval, va);
                    break;

                case RSAT_TUPLE_VERSION:
                    ss_bassert(!new_attrs[ano]);
                    atype = rs_ttype_atype(cd, ttype, ano);
                    aval = rs_tval_aval(cd, ttype, new_tval, ano);
                    if (rs_atype_datatype(cd, atype) == RSDT_BINARY) {
                        tupleversion = dbe_counter_getnewtupleversion(counter);
                        rs_tuplenum_getva(&tupleversion, &tupleversion_va);
                        rs_aval_setva(cd, atype, aval, &tupleversion_va);
                    } else {
                        ss_int8_t i8;
                        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_BIGINT);
                        i8 = dbe_counter_getnewint8tupleversion(counter);
                        rs_aval_setint8_raw(cd, atype, aval, i8, NULL);
                    }
                    break;

                case RSAT_RELATION_ID:
                case RSAT_KEY_ID:
                case RSAT_REMOVED:
                    /* skip these */
                    break;

                case RSAT_FREELY_DEFINED:
                case RSAT_CLUSTER_ID:
                case RSAT_TRX_ID:
                    ss_error;
                default:
                    ss_error;
            }
        }

        SS_POPNAME;

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_update_disk
 *
 * Updates a tuple. Old data tuple is identified by a tuple
 * reference. All attributes of the new version of the tuple are not
 * required. Those that are not available are taken from the old version
 * of the tuple.
 *
 * Parameters :
 *
 *	cd - in, use
 *          Client data object.
 *
 *	trx - in out, use
 *		Transaction handle. Can be DBE_TRX_NOTRX, if transactions
 *		are not used.
 *
 *	usertrxid - in, use
 *          User transaction id.
 *
 *	relh - in, use
 *          Relation handle for the updated tuple.
 *
 *	old_tref - in, use
 *          Tuple reference of the old version of the tuple.
 *
 *	new_attrs - in, use
 *          Boolean array containg TRUE for those attributes
 *          that has been updated and are available in new_tval.
 *
 *	new_tval - in out, use
 *          New tuple value containing the updated attributes. This
 *          must be of same type as relh. As a side effect those
 *          attributes that are not updated are assigned the old
 *          attribute value.
 *
 *      new_tref - out, use
 *          The tuple reference of the new, updated tuple is returned
 *          here.
 *
 *	search - in, use
 *	    If non-NULL, the search from where the tuple is updated.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_EXISTS
 *      DBE_ERR_NOTFOUND
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_tuple_update_disk(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_relh_t* relh,
        dbe_tref_t* old_tref,
        bool* new_attrs,
        rs_tval_t* new_tval,
        dbe_tref_t* new_tref,
        dbe_search_t* search)
{
        dbe_tuplestate_t* ts;
        dbe_ret_t rc = DBE_RC_SUCC;
        bool isupdatable = TRUE;
        bool pessgate_entered = FALSE;
        dbe_index_t* index = NULL;
        bool isnocheck = FALSE;
        su_list_t* deferred_blob_unlink_list = NULL;
        su_list_t deferred_blob_unlink_list_buf;
        rs_reltype_t reltype;
        tuple_upd_cardininfo_t ci;
        tuple_upd_cardininfo_t* pci = NULL;
        bool prelocked = FALSE;
        dbe_srk_t* srk;

        ss_dprintf_1(("*** dbe_tuple_update_disk: BEGIN, userid = %d, usertrxid = %ld\n", rs_sysi_userid(cd), DBE_TRXID_GETLONG(usertrxid)));

        if (search != NULL && !dbe_search_isactive(search, &isupdatable)) {
            if (!isupdatable) {
                ss_dprintf_1(("*** dbe_tuple_update_disk: RETURN, rc = E_UPDNEEDSFORUPDATE\n"));
                return(E_UPDNEEDSFORUPDATE);
            } else {
                ss_dprintf_1(("*** dbe_tuple_update_disk: RETURN, rc = DBE_ERR_NOACTSEARCH\n"));
                return(DBE_ERR_NOACTSEARCH);
            }
        }
        if (!isupdatable) {
            ss_dprintf_1(("*** dbe_tuple_update_disk: RETURN, rc = E_UPDNEEDSFORUPDATE\n"));
            return(E_UPDNEEDSFORUPDATE);
        }

        reltype = rs_relh_reltype(cd, relh);

        ts = dbe_trx_gettuplestate(trx);
        if(ts == NULL) {
            ts = dbe_tuplestate_init();
            dbe_trx_settuplestate(trx, ts);
        }
        CHK_TS(ts);

        switch (ts->ts_oper) {
            case TUPLE_LOCKRELH:
                if (dbe_trx_isreadonly(trx)) {
                    rc = DBE_ERR_TRXREADONLY;
                    break;
                }
                rc = dbe_trx_lockrelh(trx, relh, FALSE, -1L);
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_UPDATE_CHECK_COLS;
                /* FALLTHROUGH */

            case TUPLE_UPDATE_CHECK_COLS:
            {
                uint i;
                uint nparts;

                /* Check if new_attrs array is empty (no updated attributes).
                 */
                CHK_TS(ts);
                ts->ts_clustkey = rs_relh_clusterkey(cd, relh);
                ss_dassert(ts->ts_clustkey != NULL);
                nparts = rs_key_nparts(cd, ts->ts_clustkey);
                for (i = 0; i < nparts; i++) {
                    uint ano;
                    ano = rs_keyp_ano(cd, ts->ts_clustkey, i);
                    if (ano != RS_ANO_NULL && new_attrs[ano]) {
                        break;
                    }
                }
                if (i == nparts) {
                    /* new_attrs is empty, nothing to do, return success */
                    rc = DBE_RC_SUCC;
                    break;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_LOCKROW;
                /* FALLTHROUGH */
            }

            case TUPLE_LOCKROW:
            {
                dbe_ret_t lockrc;

                lockrc = tuple_lock(cd, trx, relh, old_tref);
                if (lockrc != DBE_RC_SUCC) {
                    rc = lockrc;
                    break;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_UPDATE;
#ifdef SS_DEBUG
                if (reltype == RS_RELTYPE_PESSIMISTIC) {
                    dbe_ret_t rc;
                    rc = dbe_trx_checklostupdate(trx, old_tref, ts->ts_clustkey, TRUE);
                    su_rc_dassert(rc == DBE_RC_SUCC || rs_sysi_getallowduplicatedelete(cd), rc);
                }
#endif /* SS_DEBUG */
                /* FALLTHROUGH */
            }

            case TUPLE_UPDATE:
            {
                dbe_datasea_t* datasea = NULL;
                dbe_btrsea_timecons_t tc;

                if (search == NULL || !dbe_search_getclustvtpl(search, &srk)) {
                    dbe_index_t* index = NULL;
                    CHK_TS(ts);
                    switch (reltype) {
                        case RS_RELTYPE_OPTIMISTIC:
                        case RS_RELTYPE_PESSIMISTIC:
                            index = dbe_db_getindex(rs_sysi_db(cd));
                            break;
                        default:
                            ss_error;
                    }
                    tc.tc_maxtrxnum = old_tref->tr_readlevel;
                    tc.tc_mintrxnum = DBE_TRXNUM_MIN;
                    tc.tc_usertrxid = usertrxid;
                    tc.tc_maxtrxid = DBE_TRXID_MAX;
                    tc.tc_trxbuf = NULL;

                    /* Find the clustering key from the index.
                     */
                    datasea = dbe_datasea_init(
                                    cd,
                                    index,
                                    ts->ts_clustkey,
                                    &tc,
                                    NULL,
                                    FALSE,
                                    "dbe_tuple_update_disk");

                    rc = dbe_datasea_search(
                                datasea,
                                old_tref->tr_vtpl,
                                (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX)
                                    ? DBE_TRXID_NULL
                                    : dbe_trx_getstmttrxid(trx),
                                &srk);

                    switch (rc) {
                        case DBE_RC_FOUND:
                            break;
                        case DBE_RC_END:
                            rc = DBE_ERR_NOTFOUND;
                            break;
                        case DBE_ERR_DEADLOCK:
                            break;
                        case DBE_ERR_HSBSECONDARY:
                            break;
                        default:
                            su_rc_derror(rc);
                            break;
                    }
                    if (rc != DBE_RC_FOUND) {
                        dbe_datasea_done(datasea);
                        break;
                    }
                }

                if (reltype == RS_RELTYPE_PESSIMISTIC && dbe_cfg_usepessimisticgate) {
                    rs_relh_pessgate_enter_exclusive(cd, relh);
                    pessgate_entered = TRUE;
                }

                /* Build the new, updated tval from old v-tuple
                 * and new attributes from new_tval.
                 */
                rc = tuple_updatetval_vtpl(
                        cd,
                        relh,
                        new_attrs,
                        new_tval,
                        usertrxid,
                        dbe_srk_getvtpl(srk),
                        dbe_srk_getvamap(srk),
                        dbe_db_getcounter(rs_sysi_db(cd)));

                if (rc == DBE_RC_SUCC) {
                    /* Delete the old tuple.
                     */
                    isnocheck = dbe_tuple_isnocheck(cd, trx, relh);
                    if (isnocheck) {
                        ss_assert(!pessgate_entered);
                        deferred_blob_unlink_list =
                            rs_sysi_getdeferredblobunlinklist(cd);
                        ss_dassert(deferred_blob_unlink_list == NULL);
                        if (deferred_blob_unlink_list == NULL) {
                            deferred_blob_unlink_list =
                                &deferred_blob_unlink_list_buf;
                            su_list_initbuf(&deferred_blob_unlink_list_buf,
                                            NULL);
                            rs_sysi_setdeferredblobunlinklist(cd, deferred_blob_unlink_list);
                        }
                        if (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX || !dbe_trx_uselogging(trx)) {
                            index = dbe_db_getindex(rs_sysi_db(cd));
                            dbe_index_locktree(index);
                            pci = &ci;
                            ci.ci_nrows = 0;
                            ci.ci_nbytes = 0;
                            prelocked = TRUE;
                        }
                    }

                    rc = tuple_insordel_vtpl(
                            OPER_UPD,
                            cd,
                            trx,
                            DBE_TRXNUM_NULL,
                            usertrxid,
                            dbe_srk_getkeytrxid(srk),
                            dbe_trx_getstmttrxid(trx),
                            relh,
                            dbe_srk_getvtpl(srk),
                            old_tref,
                            ts,
                            dbe_srk_isblob(srk),
                            isnocheck,
                            new_attrs,
                            pci,
                            TRUE,
                            index == NULL,
                            prelocked);
                }

                if (datasea != NULL) {
                    CHK_TS(ts);
                    ts->ts_datasea = datasea;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_INSERT;
                /* FALLTHROUGH */
            }

            case TUPLE_INSERT:
            {
                if (!pessgate_entered && reltype == RS_RELTYPE_PESSIMISTIC && dbe_cfg_usepessimisticgate) {
                    rs_relh_pessgate_enter_exclusive(cd, relh);
                    pessgate_entered = TRUE;
                }

                if (rc == DBE_RC_SUCC) {
                    /* Insert a new tuple.
                     */
                    rc = tuple_insert_tval(
                            OPER_UPD,
                            cd,
                            trx,
                            usertrxid,
                            relh,
                            new_tval,
                            new_tref,
                            ts,
                            isnocheck,
                            new_attrs,
                            pci,
                            index == NULL,
                            prelocked);
                }
                if (isnocheck) {
                    ss_assert(!pessgate_entered);
                    if (index != NULL) {
                        rs_key_t* clustkey;
                        bool isblob;

                        dbe_index_unlocktree(index);

                        /* Do logging outside index lock.
                         */
                        clustkey = rs_relh_clusterkey(cd, relh);
                        isblob = dbe_srk_isblob(srk);
                        if (rc == DBE_RC_SUCC && trx != DBE_TRX_NOTRX && trx != DBE_TRX_HSBTRX) {
                            /* Log delete. */
                            rc = dbe_trx_addtolog(
                                    trx,
                                    FALSE,
                                    clustkey,
                                    old_tref,
                                    dbe_srk_getvtpl(srk),
                                    relh,
                                    isblob);
                        }
                        if (rc == DBE_RC_SUCC && trx != DBE_TRX_NOTRX && trx != DBE_TRX_HSBTRX) {
                            /* Log insert. */
                            ss_dassert(ts->ts_insdvtpl != NULL);
                            rc = dbe_trx_addtolog(
                                    trx,
                                    TRUE,
                                    clustkey,
                                    new_tref,
                                    ts->ts_insdvtpl,
                                    relh,
                                    isblob);
                        }
                    }
                    if (deferred_blob_unlink_list != NULL) {
                        dbe_blobg2_unlink_list_of_blobids(
                                cd,
                                deferred_blob_unlink_list,
                                NULL);
                        if (deferred_blob_unlink_list ==
                            &deferred_blob_unlink_list_buf)
                        {
                            rs_sysi_setdeferredblobunlinklist(cd, NULL);
                            su_list_donebuf(deferred_blob_unlink_list);
                        }
                    }
                } else {
                    ss_dassert(index == NULL);
                    ss_dassert(deferred_blob_unlink_list == NULL);
                }
                if (pci != NULL) {
                    rs_relh_applydelta(cd, relh, ci.ci_nrows, ci.ci_nbytes);
                }

                if (reltype == RS_RELTYPE_PESSIMISTIC && dbe_cfg_usepessimisticgate) {
                    rs_relh_pessgate_exit(cd, relh);
                }
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                CHK_TS(ts);
                ts->ts_oper = TUPLE_HOTSTANDBY_DEL;
                /* FALLTHROUGH */
            }
            case TUPLE_HOTSTANDBY_DEL:
                CHK_TS(ts);
                rc = dbe_trx_reptuple(
                        trx,
                        FALSE,
                        ts->ts_tref,
                        ts->ts_deldvtpl,
                        ts->ts_relh,
                        ts->ts_isblobattrs);
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                ts->ts_oper = TUPLE_HOTSTANDBY_INS;
                /* FALLTHROUGH */

            case TUPLE_HOTSTANDBY_INS:
                CHK_TS(ts);
                rc = dbe_trx_reptuple(
                        trx,
                        TRUE,
                        ts->ts_tref,
                        ts->ts_insdvtpl,
                        ts->ts_relh,
                        ts->ts_isblobattrs);
                break;
            default:
                ss_rc_error(ts->ts_oper);
        }

        if (rc != DBE_RC_CONT && rc != DBE_RC_WAITLOCK) {
            CHK_TS(ts);
            dbe_tuplestate_done(ts);
            dbe_trx_settuplestate(trx, NULL);
        }

        ss_dprintf_1(("*** dbe_tuple_update_disk: RETURN, rc = %s\n", su_rc_nameof(rc)));

        return(rc);
}

#ifndef SS_NODDUPDATE

#ifdef DBE_XS

static int createindex_xscompare(
            vtpl_t** vtpl1,
            vtpl_t** vtpl2,
            uint* dummy_cmpcondarr __attribute__ ((unused)))
{
        int cmp;

        ss_dassert(vtpl_vacount(*vtpl1) == 2);
        ss_dassert(vtpl_vacount(*vtpl2) == 2);
        ss_dassert(vtpl_vacount((vtpl_t*)VTPL_GETVA_AT0(*vtpl1)) > 0);
        ss_dassert(vtpl_vacount((vtpl_t*)VTPL_GETVA_AT0(*vtpl2)) > 0);

        cmp = vtpl_compare(
                (vtpl_t*)VTPL_GETVA_AT0(*vtpl1),
                (vtpl_t*)VTPL_GETVA_AT0(*vtpl2));
        return (cmp);
}

#endif /* DBE_XS */

/*#***********************************************************************\
 *
 *		tuple_createindex_init
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	relh -
 *
 *
 *	key -
 *
 *
 *	usertrxid -
 *
 *
 *	committrxnum -
 *
 *
 *	commitp -
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
static dbe_tuple_createindex_t* tuple_createindex_init(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key,
        dbe_trxid_t usertrxid,
        dbe_trxnum_t committrxnum,
        bool commitp)
{
        dbe_tuple_createindex_t* ci;
        dbe_searchrange_t sr;
        dynvtpl_t min_dvtpl = NULL;
        dynvtpl_t max_dvtpl = NULL;
        dynva_t dva = NULL;
        rs_ttype_t* ttype;
        ss_int8_t dummy_i8;

        ci = SSMEM_NEW(dbe_tuple_createindex_t);

#ifdef SS_MME
        ci->ci_hdr.ch_type = DBE_CREATEINDEX_DBE;
#endif

        ci->ci_cd = cd;
        ci->ci_key = key;
        dbe_trx_seterrkey(trx, key);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        dbe_trx_builduniquerrorkeyvalue(
            trx, 
            NULL, 
            NULL, 
            NULL,
            0);
#endif

        ss_dprintf_1(("tuple_createindex_init:begin, rel name '%s', key name '%s', key id = %d\n",
            rs_relh_name(cd, relh), rs_key_name(cd, key), rs_key_id(cd, key)));

        ci->ci_clustkey = rs_relh_clusterkey(cd, relh);
        ss_dassert(ci->ci_clustkey != NULL);

        dynvtpl_setvtpl(&min_dvtpl, VTPL_EMPTY);
        dynva_setlong(&dva, rs_key_id(cd, ci->ci_clustkey));
        dynvtpl_appva(&min_dvtpl, dva);

        dynvtpl_setvtplwithincrement(&max_dvtpl, min_dvtpl);

        sr.sr_minvtpl = min_dvtpl;
        sr.sr_minvtpl_closed = TRUE;
        sr.sr_maxvtpl = max_dvtpl;
        sr.sr_maxvtpl_closed = TRUE;

        ci->ci_usertrxid = usertrxid;
        ci->ci_committrxnum = committrxnum;
        ci->ci_commitp = commitp;
        ci->ci_isunique = rs_key_isunique(cd, key);
        ci->ci_nocheck = dbe_tuple_isnocheck(cd, trx, relh);

        ttype = rs_relh_ttype(cd, relh);
        ci->ci_ttype = rs_ttype_copy(cd, ttype);

        ci->ci_tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        ci->ci_tc.tc_maxtrxnum =
            DBE_TRXNUM_EQUAL(ci->ci_committrxnum, DBE_TRXNUM_NULL)
                ? DBE_TRXNUM_MAX
                : DBE_TRXNUM_SUM(ci->ci_committrxnum, -1);
        ci->ci_tc.tc_usertrxid = ci->ci_usertrxid;
        ci->ci_tc.tc_maxtrxid = DBE_TRXID_MAX;
        ci->ci_tc.tc_trxbuf = NULL;

        ci->ci_index = dbe_db_getindex(rs_sysi_db(cd));
#ifdef DBE_XS
        SsInt8SetUint4(&dummy_i8, 1000);
        if (SsInt8Cmp(rs_relh_ntuples(cd, relh), dummy_i8) > 0) {
            uint order_cols[2];
            bool descarr[2];
            int i;
            rs_atype_t* atype;
            ss_int8_t ntuples_i8;
            ss_int4_t nlines;

            atype = rs_atype_initlongvarbinary(cd);
            ci->ci_xsttype = rs_ttype_create(cd);
            for (i = 0; i < 2; i++) {
                rs_ttype_setatype(cd, ci->ci_xsttype, i, atype);
            }
            rs_atype_free(cd, atype);

            order_cols[0] = 0;
            order_cols[1] = 1;
            descarr[0] = FALSE;
            descarr[1] = FALSE;

            ntuples_i8 = rs_relh_ntuples(cd, relh);
            /* Check that ntuples fits to 32-bit int */
            SsInt8SetInt4(&dummy_i8, SS_INT4_MAX);
            if (SsInt8Cmp(ntuples_i8, dummy_i8) > 0) {
                ntuples_i8 = dummy_i8;
            }
            SsInt8ConvertToInt4(&nlines, ntuples_i8);

            ci->ci_sorter = xs_sorter_cmpinit(
                                cd,
                                ci->ci_xsttype,
                                nlines,
                                TRUE,
                                2,
                                order_cols,
                                descarr,
                                (xs_qcomparefp_t)createindex_xscompare);
            if (ci->ci_sorter != NULL) {
                /* Sorter create was succesfull.
                 */
                ss_dprintf_2(("tuple_createindex_init:sorter created\n"));
                ci->ci_xstval = rs_tval_create(cd, ci->ci_xsttype);
                ci->ci_xsatype = rs_ttype_atype(cd, ci->ci_xsttype, 0);
                for (i = 0; i < 2; i++) {
                    ci->ci_xsaval[i] = rs_tval_aval(
                                            cd,
                                            ci->ci_xsttype,
                                            ci->ci_xstval,
                                            i);
                }
                ci->ci_xsstate = CI_XSSTATE_FETCH;
            } else {
                ss_dprintf_2(("tuple_createindex_init:sorter create FAILED\n"));
                rs_ttype_free(cd, ci->ci_xsttype);
            }
        } else {
            ci->ci_sorter = NULL;
        }
#endif /* DBE_XS */

        /* Search all tuples using clustering key and create the
           new key values.
        */
        ci->ci_indsea = dbe_indsea_init(
                            cd,
                            ci->ci_index,
                            ci->ci_clustkey,
                            &ci->ci_tc,
                            &sr,
                            NULL,
                            LOCK_FREE,
                            "tuple_createindex_init");

        dynvtpl_free(&min_dvtpl);
        dynvtpl_free(&max_dvtpl);
        dynva_free(&dva);

        ss_dprintf_2(("tuple_createindex_init:end\n"));

        return(ci);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_createindex_init
 *
 * Creates index specified by key.
 *
 * Parameters :
 *
 *	trx - in, use
 *		transaction handle
 *
 *	relh - in, use
 *		relh specifying the relation into which the key is created
 *
 *	key - in, use
 *		key specifying the index that is created
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
dbe_tuple_createindex_t* dbe_tuple_createindex_init(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key)
{
        rs_sysi_t* cd = dbe_trx_getcd(trx);

#ifdef SS_MME
        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            return  (dbe_tuple_createindex_t*)dbe_mme_createindex_init(
                           cd,
                           trx,
                           relh,
                           key,
                           dbe_trx_getusertrxid(trx),
                           DBE_TRXNUM_MAX,
                           FALSE);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            return tuple_createindex_init(
                           cd,
                           trx,
                           relh,
                           key,
                           dbe_trx_getusertrxid(trx),
                           DBE_TRXNUM_MAX,
                           FALSE);
        }
#else /* SS_MME */
        return(tuple_createindex_init(
                    cd,
                    trx,
                    relh,
                    key,
                    dbe_trx_getusertrxid(trx),
                    DBE_TRXNUM_MAX,
                    FALSE));
#endif
}

static void tuple_createindex_done(dbe_tuple_createindex_t* ci)
{
#ifdef DBE_XS
        if (ci->ci_sorter != NULL) {
            xs_sorter_sqldone(ci->ci_cd, ci->ci_sorter);
            rs_tval_free(ci->ci_cd, ci->ci_xsttype, ci->ci_xstval);
            rs_ttype_free(ci->ci_cd, ci->ci_xsttype);
        }
#endif /* DBE_XS */
        rs_ttype_free(ci->ci_cd, ci->ci_ttype);
        dbe_indsea_done(ci->ci_indsea);
        SsMemFree(ci);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_createindex_done
 *
 *
 *
 * Parameters :
 *
 *	ci -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_tuple_createindex_done(dbe_tuple_createindex_t* ci)
{
#ifdef SS_MME
        if (ci->ci_hdr.ch_type == DBE_CREATEINDEX_MME) {
            dbe_mme_createindex_done((dbe_mme_createindex_t*)ci);
        } else {
            ss_dassert(ci->ci_hdr.ch_type == DBE_CREATEINDEX_DBE);
            tuple_createindex_done(ci);
        }
#else
        tuple_createindex_done(ci);
#endif
}

/*#***********************************************************************\
 *
 *		tuple_uniquecheck
 *
 *
 *
 * Parameters :
 *
 *	ci -
 *
 *
 *	key_vtpl -
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
static dbe_ret_t tuple_uniquecheck(
        dbe_tuple_createindex_t* ci,
        vtpl_t* key_vtpl)
{
        dbe_indsea_t* indsea;
        dbe_searchrange_t sr;
        dbe_btrsea_timecons_t tc;
        dbe_ret_t indsea_rc;
        dbe_ret_t rc = DBE_RC_SUCC;
        dynvtpl_t rangemin_dvtpl = NULL;
        dynvtpl_t rangemax_dvtpl = NULL;
        int i;
        int nordering;
        dbe_srk_t* srk;

        ss_dassert(rs_key_isunique(ci->ci_cd, ci->ci_key));
        ss_dprintf_3(("tuple_uniquecheck\n"));

        nordering = rs_key_lastordering(ci->ci_cd, ci->ci_key) + 1;

        ss_dprintf_4(("tuple_uniquecheck:nordering = %d\n", nordering));

        /* Create a v-tuple containing all ordering parts of the unique
           key. Those parts must be unique, and they are used as the
           the starting key value of the range search.
        */
        dynvtpl_setvtpl(&rangemin_dvtpl, VTPL_EMPTY);
        {
            va_t* va;
            va = VTPL_GETVA_AT0(key_vtpl);
            for (i = 0; i < nordering; i++, va = VTPL_SKIPVA(va)) {
                dynvtpl_appva(&rangemin_dvtpl, va);
            }
        }
        /* Now generate the range maximum v-tuple. */
        dynvtpl_setvtplwithincrement(&rangemax_dvtpl, rangemin_dvtpl);

        sr.sr_minvtpl = rangemin_dvtpl;
        sr.sr_minvtpl_closed = TRUE;
        sr.sr_maxvtpl = rangemax_dvtpl;
        sr.sr_maxvtpl_closed = TRUE;

        tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        tc.tc_maxtrxnum = DBE_TRXNUM_MAX;
        tc.tc_usertrxid = ci->ci_usertrxid;
        tc.tc_maxtrxid = DBE_TRXID_MAX;
        tc.tc_trxbuf = NULL;

        ss_dprintf_4(("range begin:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_minvtpl));
        ss_dprintf_4(("range end:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, sr.sr_maxvtpl));

        indsea = dbe_indsea_init(
                    ci->ci_cd,
                    ci->ci_index,
                    ci->ci_key,
                    &tc,
                    &sr,
                    NULL,
                    LOCK_FREE,
                    "tuple_uniquecheck");

        dbe_indsea_setvalidate(
            indsea,
            DBE_KEYVLD_UNIQUE,
            TRUE);

        do {
            indsea_rc = dbe_indsea_next(
                            indsea,
                            ci->ci_usertrxid,
                            &srk);
        } while (indsea_rc == DBE_RC_NOTFOUND);

        switch (indsea_rc) {
            case DBE_RC_FOUND:
                rc = DBE_ERR_UNIQUE_S;
                break;
            case DBE_RC_END:
                rc = DBE_RC_SUCC;
                break;
            default:
                rc = indsea_rc;
                break;
        }

        dbe_indsea_done(indsea);

        dynvtpl_free(&rangemin_dvtpl);
        dynvtpl_free(&rangemax_dvtpl);

        return(rc);
}

static dbe_ret_t createindex_insertvtpl(
        dbe_tuple_createindex_t* ci,
        vtpl_t* key_vtpl,
        dbe_indexop_mode_t mode)
{
        dbe_ret_t rc;

        ss_dassert(vtpl_vacount(key_vtpl) > 0);

        rc = dbe_index_insert(
                ci->ci_index,
                rs_key_id(ci->ci_cd, ci->ci_key),
                key_vtpl,
                ci->ci_committrxnum,
                ci->ci_usertrxid,
                mode,
                ci->ci_cd);
        return(rc);
}

static dbe_ret_t createindex_uniquecheck(
        dbe_tuple_createindex_t* ci,
        vtpl_t* key_vtpl)
{
        dbe_ret_t rc;

        if (!ci->ci_commitp && ci->ci_isunique) {
            rc = tuple_uniquecheck(ci, key_vtpl);
        } else {
            rc = DBE_RC_SUCC;
        }
        return(rc);
}

#ifdef DBE_XS

static dbe_ret_t createindex_xsfetchadd(
        dbe_tuple_createindex_t* ci,
        vtpl_t* key_vtpl,
        dbe_indexop_mode_t mode)
{
        bool succp;

        ss_dprintf_3(("createindex_xsfetchadd\n"));
        ss_dassert(ci->ci_xsstate == CI_XSSTATE_FETCH);
        ss_dassert(vtpl_vacount(key_vtpl) > 0);

        rs_aval_setva(
            ci->ci_cd,
            ci->ci_xsatype,
            ci->ci_xsaval[0],
            (va_t*)key_vtpl);

#ifdef SS_UNICODE_DATA
        succp = rs_aval_setbdata_ext(
                    ci->ci_cd,
                    ci->ci_xsatype,
                    ci->ci_xsaval[1],
                    (void*)&mode,
                    sizeof(mode),
                    NULL);
#else /* SS_UNICODE_DATA */
        succp = rs_aval_setdata(
                    ci->ci_cd,
                    ci->ci_xsatype,
                    ci->ci_xsaval[1],
                    (void*)&mode,
                    sizeof(mode));
#endif /* SS_UNICODE_DATA */
        ss_dassert(succp);

        succp = xs_sorter_sqladd(
                    ci->ci_cd,
                    ci->ci_sorter,
                    ci->ci_xstval,
                    NULL);
        if (succp) {
            return(DBE_RC_CONT);
        } else {
            return(DBE_ERR_FAILED);
        }
}

static dbe_ret_t createindex_xssort(dbe_tuple_createindex_t* ci)
{
        xs_ret_t xrc;

        ss_dprintf_3(("createindex_xssort\n"));
        ss_dassert(ci->ci_xsstate == CI_XSSTATE_SORT);

        xrc = xs_sorter_sqlrunstep(ci->ci_cd, ci->ci_sorter, NULL);
        switch (xrc) {
            case XS_RC_CONT:
                return(DBE_RC_CONT);
            case XS_RC_SUCC:
                ci->ci_xsstate = CI_XSSTATE_INSERT;
                return(DBE_RC_CONT);
            default:
                ss_dassert(xrc == XS_RC_ERROR);
                return(DBE_ERR_FAILED);
        }
}

static dbe_ret_t createindex_xsinsert(dbe_tuple_createindex_t* ci)
{
        dbe_ret_t rc;
        bool finished;
        rs_tval_t* tval;
        va_t* va;
        vtpl_t* key_vtpl;
        dbe_indexop_mode_t mode;

        ss_dprintf_3(("createindex_xsinsert\n"));
        ss_dassert(ci->ci_xsstate == CI_XSSTATE_INSERT);

        tval = xs_sorter_sqlfetchnext(ci->ci_cd, ci->ci_sorter, &finished);
        if (!finished) {
            return(DBE_RC_CONT);
        }
        if (tval == NULL) {
            /* End of set. */
            return(DBE_RC_END);
        }

        /* First get mode. */
        va = rs_tval_va(ci->ci_cd, ci->ci_xsttype, tval, 1);
        memcpy(&mode, va_getasciiz(va), sizeof(mode));

        /* Then get v-tuple value. */
        key_vtpl = (vtpl_t*)rs_tval_va(ci->ci_cd, ci->ci_xsttype, tval, 0);

        rc = createindex_uniquecheck(ci, key_vtpl);
        if (rc != DBE_RC_SUCC) {
            return(rc);
        }
        rc = createindex_insertvtpl(ci, key_vtpl, mode);
        if (rc != DBE_RC_SUCC) {
            return(rc);
        }
        return(DBE_RC_CONT);
}

#endif /* DBE_XS */

/*##**********************************************************************\
 *
 *		dbe_tuple_createindex_advance
 *
 *
 *
 * Parameters :
 *
 *	ci -
 *
 *
 * Return value :
 *
 *      DBE_RC_CONT
 *      DBE_RC_END
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_tuple_createindex_advance(dbe_tuple_createindex_t* ci)
{
        dynvtpl_t key_dvtpl = NULL;
        dbe_ret_t rc;
        bool isblob;
        dbe_srk_t* srk;
        bool* blobattrs = NULL;
        vtpl_vamap_t* clustkey_vamap;
        dbe_indexop_mode_t mode;

        ss_dprintf_1(("dbe_tuple_createindex_advance:begin\n"));

#ifdef SS_MME
        if (ci->ci_hdr.ch_type == DBE_CREATEINDEX_MME) {
            return dbe_mme_createindex_advance((dbe_mme_createindex_t*)ci);
        } else {
            ss_dassert(ci->ci_hdr.ch_type == DBE_CREATEINDEX_DBE);
        }
#endif

#ifdef DBE_XS
        if (ci->ci_sorter != NULL) {
            switch (ci->ci_xsstate) {
                case CI_XSSTATE_FETCH:
                    break;
                case CI_XSSTATE_SORT:
                    return(createindex_xssort(ci));
                case CI_XSSTATE_INSERT:
                    return(createindex_xsinsert(ci));
                default:
                    ss_error;
            }
        }
#endif /* DBE_XS */

        do {
            rc = dbe_indsea_next(
                    ci->ci_indsea,
                    ci->ci_usertrxid,
                    &srk);
        } while (rc == DBE_RC_NOTFOUND);

        if (rc == DBE_RC_FOUND) {
            if (dbe_srk_isblob(srk)) {
                blobattrs = dbe_blobinfo_getattrs(
                                dbe_srk_getvtpl(srk),
                                rs_ttype_nattrs(ci->ci_cd, ci->ci_ttype),
                                NULL);
            }

            clustkey_vamap = dbe_srk_getvamap(srk);

            rc = tuple_buildkey_vtpl(
                    ci->ci_cd,
                    ci->ci_key,
                    ci->ci_clustkey,
                    ci->ci_ttype,
                    clustkey_vamap,
                    blobattrs,
                    NULL,
                    &key_dvtpl,
                    &isblob);

            if (blobattrs != NULL) {
                SsMemFree(blobattrs);
            }

            if (ci->ci_commitp) {
                mode = DBE_INDEXOP_COMMITTED;
            } else {
                mode = 0;
            }
            if (isblob) {
                mode |= DBE_INDEXOP_BLOB;
            }
            if (isblob) {
                mode |= DBE_INDEXOP_BLOB;
            }
            if (ci->ci_nocheck) {
                mode |= DBE_INDEXOP_NOCHECK;
            }

#ifdef DBE_XS
            if (ci->ci_sorter != NULL && rc == DBE_RC_SUCC) {
                rc = createindex_xsfetchadd(ci, key_dvtpl, mode);
            }
#endif /* DBE_XS */

            if (rc == DBE_RC_SUCC) {
                rc = createindex_uniquecheck(ci, key_dvtpl);
            }
            if (rc == DBE_RC_SUCC) {
                rc = createindex_insertvtpl(ci, key_dvtpl, mode);
            }

            dynvtpl_free(&key_dvtpl);

            if (rc == DBE_RC_SUCC) {
                rc = DBE_RC_CONT;
            }
        }
#ifdef DBE_XS
        if (ci->ci_sorter != NULL && rc == DBE_RC_END) {
            ci->ci_xsstate = CI_XSSTATE_SORT;
            rc = DBE_RC_CONT;
        }
#endif

        ss_dprintf_2(("dbe_tuple_createindex_advance:end, rc = %d\n", rc));

        return(rc);
}

/*#***********************************************************************\
 *
 *		tuple_dropindex_init
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	key -
 *
 *
 *	usertrxid -
 *
 *
 *	committrxnum -
 *
 *
 *	commitp -
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
static dbe_tuple_dropindex_t* tuple_dropindex_init(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key,
        dbe_trxid_t usertrxid,
        dbe_trxnum_t committrxnum,
        bool commitp,
        bool truncate,
        bool recovery,
        su_list_t* deferredblobunlinklist)
{
        dbe_tuple_dropindex_t* di;
        dbe_searchrange_t sr;
        dynvtpl_t min_dvtpl = NULL;
        dynvtpl_t max_dvtpl = NULL;
        dynva_t dva = NULL;
        ss_debug(bool isexclusive = TRUE;)

        di = SSMEM_NEW(dbe_tuple_dropindex_t);

#ifdef SS_MME
        di->di_hdr.dh_type = DBE_DROPINDEX_DBE;
#endif

        ss_dprintf_1(("tuple_dropindex_init, key name '%s', key id = %d, truncate = %d\n",
            rs_key_name(cd, key), rs_key_id(cd, key), truncate));
        ss_dassert(truncate || recovery || dbe_trx_getlockrelh(cd, trx, relh, &isexclusive, NULL));
        ss_dassert(isexclusive);

        sr.sr_minvtpl = NULL;
        sr.sr_minvtpl_closed = TRUE;
        sr.sr_maxvtpl = NULL;
        sr.sr_maxvtpl_closed = TRUE;

        dynvtpl_setvtpl(&min_dvtpl, VTPL_EMPTY);
        dynva_setlong(&dva, rs_key_id(cd, key));
        dynvtpl_appva(&min_dvtpl, dva);

        dynvtpl_setvtplwithincrement(&max_dvtpl, min_dvtpl);

        sr.sr_minvtpl = min_dvtpl;
        sr.sr_maxvtpl = max_dvtpl;

        di->di_usertrxid = usertrxid;
        di->di_committrxnum = committrxnum;
        di->di_commitp = commitp;
        di->di_keyid = rs_key_id(cd, key);
        di->di_cd = cd;
        di->di_relh = relh;
        rs_relh_link(cd, relh);
        SS_MEM_SETLINK(relh);

        di->di_tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        di->di_tc.tc_maxtrxnum =
            DBE_TRXNUM_EQUAL(di->di_committrxnum, DBE_TRXNUM_NULL)
                ? DBE_TRXNUM_MAX
                : di->di_committrxnum;
        di->di_tc.tc_usertrxid = di->di_usertrxid;
        di->di_tc.tc_maxtrxid = DBE_TRXID_MAX;
        di->di_tc.tc_trxbuf = NULL;

        di->di_index = dbe_db_getindex(rs_sysi_db(cd));

        di->di_isclustkey = rs_key_isclustering(cd, key);
        if (truncate) {
            di->di_nocheck = TRUE;
        } else {
            di->di_nocheck = dbe_tuple_isnocheck(cd, trx, relh);
        }
        if (di->di_nocheck) {
            if (deferredblobunlinklist != NULL) {
                ss_dassert(truncate);
                di->di_deferredblobunlinklist = deferredblobunlinklist;
                di->di_localdeferredblobunlinklist = FALSE;
            } else {
                di->di_deferredblobunlinklist = su_list_init(NULL);
                di->di_localdeferredblobunlinklist = TRUE;
            }
            rs_sysi_setdeferredblobunlinklist(cd, di->di_deferredblobunlinklist);
        } else {
            di->di_deferredblobunlinklist = NULL;
        }
        di->di_truncate = truncate;

        if (di->di_nocheck && di->di_hdr.dh_type == DBE_DROPINDEX_DBE) {
            dbe_dynbkey_t dynbkey = NULL;

            di->di_bonsaitreesearch = TRUE;
            dbe_dynbkey_setleaf(
                &dynbkey,
                DBE_TRXNUM_NULL,
                DBE_TRXID_NULL,
                sr.sr_minvtpl);
            dbe_bkey_setdeletemark(dynbkey);
            di->di_kc.kc_beginkey = dynbkey;

            dynbkey = NULL;
            dbe_dynbkey_setleaf(
                &dynbkey,
                DBE_TRXNUM_NULL,
                DBE_TRXID_MAX,
                sr.sr_maxvtpl);
            di->di_kc.kc_endkey = dynbkey;

            di->di_kc.kc_conslist = NULL;
            di->di_kc.kc_cd = cd;
            di->di_kc.kc_key = key;

            di->di_tc.tc_mintrxnum = DBE_TRXNUM_NULL;
            di->di_tc.tc_maxtrxnum = DBE_TRXNUM_NULL;
            di->di_tc.tc_usertrxid = DBE_TRXID_NULL;
            di->di_tc.tc_maxtrxid = DBE_TRXID_NULL;
            di->di_tc.tc_trxbuf = dbe_index_gettrxbuf(di->di_index);

            dbe_btrsea_initbufvalidate(
                &di->di_btrsea,
                dbe_index_getbonsaitree(di->di_index),
                &di->di_kc,
                &di->di_tc,
                FALSE,
                FALSE,
                DBE_KEYVLD_NONE,
                dbe_index_isearlyvld(di->di_index));
            dbe_btrsea_settimeconsacceptall(&di->di_btrsea);
        } else {
            /* Search all tuples using clustering key and delete the
               key values.
            */
            di->di_indsea = dbe_indsea_init(
                                cd,
                                di->di_index,
                                key,
                                &di->di_tc,
                                &sr,
                                NULL,
                                LOCK_FREE,
                                "tuple_dropindex_init");
        }

        dynvtpl_free(&min_dvtpl);
        dynvtpl_free(&max_dvtpl);
        dynva_free(&dva);

        return(di);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_dropindex_init
 *
 *
 *
 * Parameters :
 *
 *	trx -
 *
 *
 *	key -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_tuple_dropindex_t* dbe_tuple_dropindex_init(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key,
        bool truncate,
        su_list_t* deferredblobunlinklist)
{
        rs_sysi_t* cd = dbe_trx_getcd(trx);

#ifdef SS_MME
        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            return  (dbe_tuple_dropindex_t*)dbe_mme_dropindex_init(
                           cd,
                           trx,
                           relh,
                           key,
                           dbe_trx_getusertrxid(trx),
                           DBE_TRXNUM_MAX,
                           FALSE,
                           FALSE,
                           TRUE);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            return tuple_dropindex_init(
                           cd,
                           trx,
                           relh,
                           key,
                           dbe_trx_getusertrxid(trx),
                           DBE_TRXNUM_MAX,
                           FALSE,
                           truncate,
                           FALSE,
                           deferredblobunlinklist);
        }
#else /* SS_MME */
        return(tuple_dropindex_init(
                    cd,
                    trx,
                    relh,
                    key,
                    dbe_trx_getusertrxid(trx),
                    DBE_TRXNUM_MAX,
                    FALSE));
#endif
}

/*##**********************************************************************\
 *
 *		dbe_tuple_dropindex_done
 *
 *
 *
 * Parameters :
 *
 *	di -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_tuple_dropindex_done(dbe_tuple_dropindex_t* di)
{
#ifdef SS_MME
        if (di->di_hdr.dh_type == DBE_DROPINDEX_MME) {
            dbe_mme_dropindex_done((dbe_mme_dropindex_t*)di);
        } else {
            ss_dassert(di->di_hdr.dh_type == DBE_DROPINDEX_DBE);
            if (di->di_nocheck) {
                dbe_dynbkey_t dynbkey;
                dbe_btrsea_donebuf(&di->di_btrsea);
                dynbkey = di->di_kc.kc_beginkey;
                dbe_dynbkey_free(&dynbkey);
                dynbkey = di->di_kc.kc_endkey;
                dbe_dynbkey_free(&dynbkey);
            } else {
                dbe_indsea_done(di->di_indsea);
            }
            if (di->di_deferredblobunlinklist != NULL
                && di->di_localdeferredblobunlinklist)
            {
                rs_sysi_link(di->di_cd);
                dbe_indmerge_unlinkblobs(di->di_cd, di->di_deferredblobunlinklist);
                rs_sysi_setdeferredblobunlinklist(di->di_cd, NULL);
                SS_RTCOVERAGE_INC(SS_RTCOV_TRUNCATE_RECOVBLOBS);
            }
            SS_MEM_SETUNLINK(di->di_relh);
            rs_relh_done(di->di_cd, di->di_relh);
            SsMemFree(di);
        }
#else
        dbe_indsea_done(di->di_indsea);
        SsMemFree(di);
#endif
}

/*##**********************************************************************\
 *
 *		dbe_tuple_dropindex_advance
 *
 *
 *
 * Parameters :
 *
 *	di -
 *
 *
 * Return value :
 *
 *      DBE_RC_CONT
 *      DBE_RC_END
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_tuple_dropindex_advance(dbe_tuple_dropindex_t* di)
{
        dbe_ret_t rc;
        dbe_srk_t* srk;

#ifdef SS_MME
        if (di->di_hdr.dh_type == DBE_DROPINDEX_MME) {
            return dbe_mme_dropindex_advance((dbe_mme_dropindex_t*)di);
        } else {
            ss_dassert(di->di_hdr.dh_type == DBE_DROPINDEX_DBE);
        }
#endif

        ss_dprintf_1(("dbe_tuple_dropindex_advance\n"));

        if (di->di_nocheck) {
            do {
                rc = dbe_btrsea_getnext(&di->di_btrsea, &srk);
            } while (rc == DBE_RC_NOTFOUND);
        } else {
            do {
                rc = dbe_indsea_next(
                        di->di_indsea,
                        di->di_usertrxid,
                        &srk);
            } while (rc == DBE_RC_NOTFOUND);
        }

        if (rc == DBE_RC_FOUND) {
            dbe_indexop_mode_t mode;

            if (di->di_truncate) {
                ss_dassert(di->di_nocheck);
                SS_RTCOVERAGE_INC(SS_RTCOV_TRUNCATE_DROPINDEX_STEP);
            }

            if (di->di_commitp) {
                mode = DBE_INDEXOP_COMMITTED;
            } else {
                mode = 0;
            }
            if (di->di_isclustkey) {
                mode |= DBE_INDEXOP_CLUSTERING;
            }
            if (di->di_nocheck) {
                mode |= DBE_INDEXOP_NOCHECK;
            }
            if (dbe_srk_isblob(srk)) {
                mode |= DBE_INDEXOP_BLOB;
            }

            if (di->di_nocheck) {
                rc = dbe_index_bkey_delete_physical(
                        di->di_index, 
                        dbe_srk_getbkey(srk),
                        di->di_cd);
            } else {
                ss_dassert(!di->di_truncate);
                rc = dbe_index_delete(
                        di->di_index,
                        di->di_keyid,
                        dbe_srk_getvtpl(srk),
                        di->di_committrxnum,
                        di->di_usertrxid,
                        mode,
                        NULL,
                        di->di_cd);
            }
            if (rc == DBE_RC_SUCC) {
                rc = DBE_RC_CONT;
            }
        }

        if (rc == DBE_ERR_ASSERT) {
            rc = DBE_RC_END;
        }
        if (rc == DBE_RC_END && di->di_nocheck && di->di_bonsaitreesearch) {
            di->di_bonsaitreesearch = FALSE;
            /* Next go through permanent tree. */
            dbe_btrsea_donebuf(&di->di_btrsea);
            dbe_btrsea_initbufvalidate(
                &di->di_btrsea,
                dbe_index_getpermtree(di->di_index),
                &di->di_kc,
                &di->di_tc,
                FALSE,
                FALSE,
                DBE_KEYVLD_NONE,
                dbe_index_isearlyvld(di->di_index));
            dbe_btrsea_settimeconsacceptall(&di->di_btrsea);
            rc = DBE_RC_CONT;
        }

        ss_dprintf_2(("dbe_tuple_dropindex_advance:end, rc = %d\n", rc));

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_recovdroprel
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trxid -
 *
 *
 *	committrxnum -
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
dbe_ret_t dbe_tuple_recovdroprel(
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        dbe_trxnum_t committrxnum,
        rs_relh_t* relh,
        bool truncate)
{
        su_pa_t* keys;
        rs_key_t* key;
        uint i;
        dbe_ret_t rc;

        keys = rs_relh_keys(cd, relh);

        /* MME requires the clustering key to be freed last.  It doesn't
           hurt DBE, so let's do it always.
           apl 2002-12-23 */
        su_pa_do_get(keys, i, key) {
            if (!rs_key_isclustering(cd, key) && !rs_key_isprimary(cd, key)) {
                rc = dbe_tuple_recovdropindex(
                        cd,
                        trxid,
                        committrxnum,
                        relh,
                        rs_key_id(cd, key),
                        truncate);
                if (rc != DBE_RC_SUCC) {
                    return(rc);
                }
            }
        }
        su_pa_do_get(keys, i, key) {
            if (rs_key_isclustering(cd, key) || rs_key_isprimary(cd, key)) {
                rc = dbe_tuple_recovdropindex(
                        cd,
                        trxid,
                        committrxnum,
                        relh,
                        rs_key_id(cd, key),
                        truncate);
                if (rc != DBE_RC_SUCC) {
                    return(rc);
                }
            }
        }

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *		dbe_tuple_recovcreateindex
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trxid -
 *
 *
 *	committrxnum -
 *
 *
 *	relh -
 *
 *
 *	keyid -
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
dbe_ret_t dbe_tuple_recovcreateindex(
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        dbe_trxnum_t committrxnum,
        rs_relh_t* relh,
        ulong keyid)
{
        uint ctr = 0;
        rs_key_t* key;
        dbe_ret_t rc;
        su_profile_timer;

        su_profile_start;

        key = rs_relh_keybyid(cd, relh, keyid);
        if (key == NULL) {
            ss_derror;
            su_profile_stop("dbe_tuple_recovcreateindex");
            return(DBE_ERR_NOTFOUND);
        }

        /* First drop possible old key.
         */
        rc = dbe_tuple_recovdropindex(cd, trxid, committrxnum, relh, keyid, FALSE);
        if (rc != DBE_RC_SUCC) {
            su_profile_stop("dbe_tuple_recovcreateindex");
            return(rc);
        }

#ifdef SS_MME
        if (rs_relh_reltype(cd, relh)
                == RS_RELTYPE_MAINMEMORY) {
            dbe_mme_createindex_t* ci;

            ci = dbe_mme_createindex_init(
                    cd,
                    DBE_TRX_NOTRX,
                    relh,
                    key,
                    trxid,
                    committrxnum,
                    DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL)
                        ? FALSE : TRUE);

            do {
                rc = dbe_mme_createindex_advance(ci);
                if (ctr++ % 100 == 0) {
                    ss_svc_notify_init();
                }
            } while (rc == DBE_RC_CONT);

            dbe_mme_createindex_done(ci);

        } else {
#endif
            /* Create the new index.
             */
            dbe_tuple_createindex_t* ci;

            ci = tuple_createindex_init(
                    cd,
                    DBE_TRX_NOTRX,
                    relh,
                    key,
                    trxid,
                    committrxnum,
                    DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL)
                        ? FALSE : TRUE);

            do {
                rc = dbe_tuple_createindex_advance(ci);
                if (ctr++ % 100 == 0) {
                    ss_svc_notify_init();
                }
            } while (rc == DBE_RC_CONT);

            dbe_tuple_createindex_done(ci);
#ifdef SS_MME
        }
#endif
        su_profile_stop("dbe_tuple_recovcreateindex");
        if (rc == DBE_RC_END) {
            return(DBE_RC_SUCC);
        } else {
            return(rc);
        }
}

/*##**********************************************************************\
 *
 *		dbe_tuple_recovdropindex
 *
 *
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	trxid -
 *
 *
 *	committrxnum -
 *
 *
 *	relh -
 *
 *
 *	keyid -
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
dbe_ret_t dbe_tuple_recovdropindex(
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        dbe_trxnum_t committrxnum,
        rs_relh_t* relh,
        ulong keyid,
        bool truncate)
{
        uint ctr = 0;
        rs_key_t* key;
        dbe_ret_t rc;
        su_profile_timer;

        su_profile_start;

        key = rs_relh_keybyid(cd, relh, keyid);
        if (key == NULL) {
            ss_derror;
            su_profile_stop("dbe_tuple_recovdropindex");
            return(DBE_ERR_NOTFOUND);
        }

        /* Drop the index.
         */
#ifdef SS_MME
        if (rs_relh_reltype(cd, relh)
                == RS_RELTYPE_MAINMEMORY) {

            dbe_mme_dropindex_t* di;

            di = dbe_mme_dropindex_init(
                    cd,
                    NULL,
                    relh,
                    key,
                    trxid,
                    committrxnum,
                    DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL)
                        ? FALSE : TRUE,
                    FALSE,
                    TRUE);

            do {
                rc = dbe_mme_dropindex_advance(di);
                if (ctr++ % 100 == 0) {
                    ss_svc_notify_init();
                }
            } while (rc == DBE_RC_CONT);

            dbe_mme_dropindex_done(di);

        } else {
#endif
            dbe_tuple_dropindex_t* di;

            di = tuple_dropindex_init(
                    cd,
                    DBE_TRX_NOTRX,
                    relh,
                    key,
                    trxid,
                    committrxnum,
                    DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_NULL)
                        ? FALSE : TRUE,
                    truncate,
                    TRUE,
                    NULL);

            do {
                rc = dbe_tuple_dropindex_advance(di);
                if (ctr++ % 100 == 0) {
                    ss_svc_notify_init();
                }
            } while (rc == DBE_RC_CONT);

            dbe_tuple_dropindex_done(di);
#ifdef SS_MME
        }
#endif
        su_profile_stop("dbe_tuple_recovdropindex");
        if (rc == DBE_RC_END) {
            return(DBE_RC_SUCC);
        } else {
            return(rc);
        }
}

#endif /* SS_NODDUPDATE */
