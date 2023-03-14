/*************************************************************************\
**  source       * rs0relh.h
**  directory    * res
**  description  * Relation handle functions
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


#ifndef RS0RELH_H
#define RS0RELH_H

#include <ssc.h>
#include <su0parr.h>
#include <su0gate.h>

#include <uti0va.h>
#include <uti0vtpl.h>

#include "rs0types.h"
#include "rs0cardi.h"
#include "rs0entna.h"

#ifndef SS_MYSQL
#include "rs0hcol.h"
#endif

#define RS_INTERNAL

#define CHECK_RELH(rel) {\
                            ss_dassert(SS_CHKPTR(rel));\
                            ss_dassert(rel->rh_check == RSCHK_RELHTYPE);\
                        }

typedef enum {
        RS_RELTYPE_OPTIMISTIC,
        RS_RELTYPE_PESSIMISTIC,
        RS_RELTYPE_MAINMEMORY
} rs_reltype_t;

#if defined(RS_USE_MACROS) || defined(RS_INTERNAL)

rs_key_t* rs_relh_search_clusterkey(rs_sysi_t* cd, rs_relh_t* relh);

typedef struct {
        rs_entname_t*   t_name;
        char*           t_str;
} relh_trig_t;

struct rsrelhandlestruct {

        rs_check_t      rh_check;       /* check field */

        int             rh_nlink;       /* number of links to this relh,
                                           free is not done until the link
                                           count is zero */
        rs_entname_t*   rh_name;        /* relation name */
        ulong           rh_relid;       /* relation id */
        bool            rh_aborted;
        int             rh_ddopactive;
        int             rh_readonly;

        rs_ttype_t*     rh_ttype;
        su_pa_t*        rh_key_pa;      /* array of rs_key_t* objects */
        su_pa_t*        rh_refkey_pa;   /* array of rs_key_t* objects */
        rs_key_t*       rh_clusterkey;  /* the clustering key */
        char**          rh_sqlcheckstrs; /* SQL check strings */
        char**          rh_sqlchecknames; /* SQL check string names */
        uint*           rh_defarr;      /* if non-NULL, an integer array
                                           containing 1 for all the attrs for
                                           which a default value is provided  */
        rs_tval_t*      rh_defvalue;    /* default values for attributes */
        rs_reltype_t    rh_reltype;     /* Relation type. */
        bool            rh_istransient; /* Is this relation transient. */
        bool            rh_isglobaltemporary; /* Is this relation a global
                                                 temporary. */
        su_gate_t*      rh_pessgate;    /* gate for pessimistic transactions. */
        long            rh_pesschangecount;
        bool            rh_nocheck;     /* If TRUE, no checks are needed for table */
        su_pa_t*        rh_triggers;
        bool            rh_basetablep;
        bool            rh_historytablep;
        bool            rh_rowlevelhsbp;
        bool            rh_syncp;
#ifdef SS_SYNC
        rs_relh_t*      rh_syncrelh;

        rs_hcol_t*      rh_hcol;

#endif /* SS_SYNC */
#ifndef SS_NOESTSAMPLES
        su_pa_t*        rh_attrsamples; /* Attribute selectivity samples,
                                           indexed by attribute number. */
        su_pa_t*        rh_samplesizes; /* Number of samples for an attribute. */
        su_pa_t*        rh_samplediffs; /* Number of different samples for an attribute. */
        su_pa_t*        rh_eqrowestimate; /* Estimated number of rows for one range. */
        su_pa_t*        rh_samplenulls; /* Number of NULL samples for an attribute. */
        su_pa_t*        rh_maxdiffrows; /* Max number of different column values. */
        SsSemT*         rh_samplesem;   /* Smple mutex. */
        bool            rh_samplefailed;    /* TRUE if sample failed. */
        long            rh_samplechange;    /* Count of sample changes. */
        ulong           rh_samplechangectr;
        ss_int8_t       rh_samplentuples;   /* Number of tuples when sample takes. */
        bool            rh_issamples;       /* TRUE if we have samples. */
        bool            rh_mustrefreshsamples;
        ulong           rh_replanchange;
        ulong           rh_replanchangectr;
        ss_int8_t       rh_replanntuples;
#endif /* SS_NOESTSAMPLES */
        rs_cardin_t*    rh_cardin;      /* Cardinality object, shared with
                                           rs_rbuf_t. */
        char*           rh_rowcheckcolname;
#ifdef SS_MME
        rs_ano_t        rh_differentiatingano;  /* The attribute used as an
                                                   implicit key part in MME
                                                   tables to differentiate
                                                   rows in a non-unique
                                                   index. */
#endif
        SsSemT*         rh_sem;

#ifndef NOT_NOTNULL_ANOARRAY
        rs_ano_t        *rh_notnull_anoarray;
#endif /* NOT_NOTNULL_ANOARRAY */

        bool            rh_isautoincinited; /* TRUE if autoincrement value
                                             has been initialized. */
        long            rh_autoincseqid;  /* autoinc sequence id */

}; /* rs_relh_t */

#define _RS_RELH_CLUSTERKEY_(cd, relh) \
        ((relh)->rh_clusterkey == NULL \
         ? (relh)->rh_clusterkey = rs_relh_search_clusterkey((cd), (relh)) \
         : (relh)->rh_clusterkey)

#define _RS_RELH_TTYPE_(cd, relh) \
        ((relh)->rh_ttype)

#define _RS_RELH_RELID_(cd, relh) \
        ((relh)->rh_relid)

#define _RS_RELH_ISTRANSIENT_(cd, relh) \
        ((relh)->rh_istransient)

#define _RS_RELH_ISGLOBALTEMPORARY_(cd, relh) \
        ((relh)->rh_isglobaltemporary)

#define _RS_RELH_ISAUTOINCINITED_(cd, relh) \
        ((relh)->rh_isautoincinited)

#define _RS_RELH_KEYS_(cd, relh) \
        ((relh)->rh_key_pa)

#endif /* defined(RS_INTERNAL) || defined (RS_USE_MACROS) */

#ifdef RS_USE_MACROS

#define rs_relh_clusterkey(cd, relh)  _RS_RELH_CLUSTERKEY_(cd, relh)

#define rs_relh_ttype(cd, relh)       _RS_RELH_TTYPE_(cd, relh)

#define rs_relh_relid(cd, relh)       _RS_RELH_RELID_(cd, relh)

#define rs_relh_istransient(cd, relh)   _RS_RELH_ISTRANSIENT_(cd, relh)

#define rs_relh_isglobaltemporary(cd, relh) _RS_RELH_ISGLOBALTEMPORARY_(cd, relh)

#define rs_relh_isautoincinited(cd, relh) _RS_RELH_ISAUTOINCINITED_(cd, relh)

#define rs_relh_keys(cd, relh)            _RS_RELH_KEYS_(cd, relh)

#else

rs_key_t *rs_relh_clusterkey(
        void *cd,
        rs_relh_t *relh);

rs_ttype_t* rs_relh_ttype( /* used to be describe */
        void*      cd,
        rs_relh_t* rel);

ulong rs_relh_relid(
        void*      cd,
        rs_relh_t* relh);

bool rs_relh_istransient(
        void*       cd,
        rs_relh_t*  relh);

bool rs_relh_isglobaltemporary(
        void*       cd,
        rs_relh_t*  relh);

bool rs_relh_isautoincinited(
        void *      cd,
        rs_relh_t*  relh);

su_pa_t* rs_relh_keys(
        void*      cd,
        rs_relh_t* rel);

#endif /* RS_USE_MACROS */


#define rs_relh_islogged(cd, relh) (!rs_relh_istransient(cd, relh) \
                                    && !rs_relh_isglobaltemporary(cd, relh))

uint rs_relh_canreverse(
        void*      cd,
        rs_relh_t* rel
);

char* rs_relh_checkstring(
        void*      cd,
        rs_relh_t* rel
);

char** rs_relh_checkstrings(
        void*      cd,
        rs_relh_t* rel,
        char***    pnamearray
);

SS_INLINE rs_entname_t* rs_relh_entname(
        void*      cd,
        rs_relh_t* rel
);

SS_INLINE char* rs_relh_name(
        void*      cd,
        rs_relh_t* rel
);

SS_INLINE char* rs_relh_schema(
	void*      cd,
	rs_relh_t* relh
);

SS_INLINE char* rs_relh_catalog(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setcatalog(
        void*      cd,
        rs_relh_t* relh,
        char* newcatalog);

SS_INLINE bool rs_relh_issysrel(
	void*      cd,
	rs_relh_t* relh
);

bool rs_relh_isdlsysrel(
	void*      cd,
	rs_relh_t* relh
);

SS_INLINE bool rs_relh_isbasetable(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setbasetable(
	void*      cd,
	rs_relh_t* relh,
        bool       basetablep
);

SS_INLINE bool rs_relh_ishistorytable(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_sethistorytable(
	void*      cd,
	rs_relh_t* relh,
        bool       historytablep
);

bool rs_relh_isrowlevelhsb(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setrowlevelhsb(
	void*      cd,
	rs_relh_t* relh,
        bool       rowlevelhsbp
);

SS_INLINE bool rs_relh_issync(
	void*      cd,
	rs_relh_t* relh
);

#ifdef SS_SYNC

void rs_relh_setsync(
	void*      cd,
	rs_relh_t* relh,
        bool       syncp
);

bool rs_relh_insertsyncrelh(
	void*      cd,
	rs_relh_t* relh,
	rs_relh_t* sync_relh
);

rs_relh_t* rs_relh_getsyncrelh(
	void*      cd,
	rs_relh_t* relh
);

#ifndef SS_MYSQL
rs_hcol_t* rs_relh_gethcol(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_inserthcol(
	void*      cd,
	rs_relh_t* relh,
        rs_hcol_t* hcol
);
#endif /* SS_MYSQL */

int rs_relh_sync_getsubscribe_count(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_sync_addsubscribe_count(
	void*      cd,
	rs_relh_t* relh,
        bool       addp
);

#endif /* SS_SYNC */

/* Relh services for key selection */

SS_INLINE su_pa_t* rs_relh_refkeys(
        void*      cd,
        rs_relh_t* rel
);

rs_key_t *rs_relh_primkey(
        void *cd,
        rs_relh_t *relh);

rs_key_t *rs_relh_keybyname(
        void *cd,
        rs_relh_t *relh,
        rs_entname_t* keyname);

rs_key_t *rs_relh_keybyid(
	void*      cd,
	rs_relh_t* relh,
        ulong      keyid);

rs_ano_t rs_relh_keyno(
    void* cd,
    rs_relh_t* relh,
    ulong keyid);

uint rs_relh_nattrs(
        void*      cd,
        rs_relh_t* rel
);

void rs_relh_setaborted(
	void*      cd,
	rs_relh_t* relh
);

SS_INLINE bool rs_relh_isaborted(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setddopactive(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setnoddopactive(
	void*      cd,
	rs_relh_t* relh
);

SS_INLINE bool rs_relh_isddopactive(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setreadonlyttype(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setreadonly(
	void*      cd,
	rs_relh_t* relh
);

SS_INLINE bool rs_relh_isreadonly(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setnocheck(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setcheck(
	void*      cd,
	rs_relh_t* relh
);

SS_INLINE bool rs_relh_isnocheck(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_setreltype(
	void*      cd,
	rs_relh_t* relh,
        rs_reltype_t reltype
);

SS_INLINE rs_reltype_t rs_relh_reltype(
	void*      cd,
	rs_relh_t* relh
);

void rs_relh_settransient(
        void*       cd,
        rs_relh_t*  relh,
        bool        istransient);

void rs_relh_setglobaltemporary(
        void*       cd,
        rs_relh_t*  relh,
        bool        isglobaltemporary);

long rs_relh_pessgate_enter_shared(
	void*      cd,
	rs_relh_t* relh);

void rs_relh_pessgate_enter_exclusive(
	void*      cd,
	rs_relh_t* relh);

void rs_relh_pessgate_exit(
	void*      cd,
	rs_relh_t* relh);

/* For administration */

rs_relh_t* rs_relh_init(
	void*           cd,
	rs_entname_t*   relname,
        ulong           relid,
        rs_ttype_t*     ttype
);

void rs_relh_done(
        void*      cd,
        rs_relh_t* rel
);

void rs_relh_link(
	void*      cd,
	rs_relh_t* relh
);

rs_relh_t* rs_relh_copy(
	void*      cd,
	rs_relh_t* relh
);

bool rs_relh_setcheckstring(
        rs_sysi_t* cd,
        rs_relh_t* rel,
        char*      checkstr
);

bool rs_relh_addcheckstring(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        char*      checkstr,
        char*      constrname
);

bool rs_relh_setdefault(
	void*       cd,
        rs_relh_t*  relh,
        uint*       defarr,
        rs_tval_t*  defvalue
);

void rs_relh_setrelid(
        void*      cd,
        rs_relh_t* relh,
        ulong      relid
);

bool rs_relh_default(
	void*       cd,
        rs_relh_t*  rel,
        uint**      p_defarr,
        rs_tval_t** p_defvalue
);

bool rs_relh_insertkey(
        void*      cd,
        rs_relh_t* rel,
        rs_key_t*  key
);

bool rs_relh_deletekey(
        void*      cd,
        rs_relh_t* rel,
        rs_key_t*  key
);

bool rs_relh_insertrefkey(
        void*      cd,
        rs_relh_t* rel,
        rs_key_t*  key
);

bool rs_relh_deleterefkey (
        void*      cd,
        rs_relh_t* rel,
        char*      name
);

bool rs_relh_hasrefkey (
        void*      cd,
        rs_relh_t* relh,
        char*      name
);

rs_key_t* rs_relh_refkeybyname(
        void*      cd,
        rs_relh_t* relh,
        char*      name);

rs_cardin_t* rs_relh_cardin(
	void*      cd,
        rs_relh_t* relh
);

rs_cardin_t* rs_relh_replacecardin(
	void*      cd,
        rs_relh_t* relh,
        rs_cardin_t* cr
);

void rs_relh_setcardin(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        rs_cardin_t* cr
);

bool rs_relh_cardinalchanged(
	void*      cd,
        rs_relh_t* relh
);

void rs_relh_clearcardinalchanged(
	void*      cd,
        rs_relh_t* relh
);

void rs_relh_setcardinal(
	void*      cd,
        rs_relh_t* relh,
        ss_int8_t  ntuples,
        ss_int8_t  nbytes
);

bool rs_relh_updatecardinal(
        void*       cd,
        rs_relh_t*  relh,
        long        ntuples,
        long        nbytes,
        ulong       nchanges);

void rs_relh_cardininfo(
	void*      cd,
        rs_relh_t* relh,
        ss_int8_t* p_ntuples,
        ss_int8_t* p_nbytes
);

SS_INLINE ss_int8_t rs_relh_ntuples(
	void*      cd,
        rs_relh_t* rel
);

ss_int8_t rs_relh_nbytes(
	void*      cd,
        rs_relh_t* rel
);

bool rs_relh_insertbytes(
        void*       cd,
        rs_relh_t*  relh,
        ulong       nbytes,
        ulong       nchanges);

bool rs_relh_deletebytes(
        void*       cd,
        rs_relh_t*  relh,
        ulong       nbytes,
        ulong       nchanges);

bool rs_relh_applydelta(
        void*       cd,
        rs_relh_t*  relh,
        long        delta_rows,
        long        delta_bytes);

bool rs_relh_applydelta_nomutex(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        long        delta_rows,
        long        delta_bytes);

void rs_relh_samplemutex_enter(
	void*      cd,
        rs_relh_t* relh
);

void rs_relh_samplemutex_exit(
	void*      cd,
        rs_relh_t* relh
);

void rs_relh_initattrsamples_nomutex(
	    void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        vtpl_t**   keysamples,
        int*       sample_estimates,
        int        sample_size
);

void rs_relh_clearsamples_nomutex(
        void* cd,
        rs_relh_t* relh);

void rs_relh_initattrsamplesbytval_nomutex(
        void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        rs_tval_t** tval_arr,
        int        sample_size);

bool rs_relh_issamples_nomutex(
	void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano
);

bool rs_relh_issamplefailed_nomutex(
	void*      cd,
        rs_relh_t* relh
);

void rs_relh_setsamplefailed_nomutex(
	void*      cd,
        rs_relh_t* relh
);

bool rs_relh_mustrefreshsamples(
	void*      cd,
        rs_relh_t* relh
);

bool rs_relh_getrelopselectivity(
	void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        int        relop,
        va_t*      value,
        int        escchar,
        ss_int8_t* p_nvalues);

bool rs_relh_getrangeselectivity(
	void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        va_t*      range_start,
        bool       range_start_closed,
        va_t*      range_end,
        bool       range_end_closed,
        bool       pointlike,
        ss_int8_t* p_nvalues);

bool rs_relh_getequalvalues(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        rs_ano_t    ano,
        long*       p_nvalues);

long rs_relh_getdiffrowcount(
	void*      cd,
        rs_relh_t* relh,
        rs_ano_t   ano,
        long       ano_rowcount);

void rs_relh_settrigger(
        void* cd,
        rs_relh_t* relh,
        int index,
        rs_entname_t* name,
        char* str);

SS_INLINE char* rs_relh_triggerstr(
        void* cd,
        rs_relh_t* relh,
        int index);

SS_INLINE rs_entname_t* rs_relh_triggername(
        void* cd,
        rs_relh_t* relh,
        int index);

int rs_relh_triggercount(
        rs_relh_t* relh);

#ifdef SS_DEBUG

void rs_relh_print(
	void*       cd,
	rs_relh_t*  relh
);

void rs_relh_check(
	void*      cd,
	rs_relh_t* relh);

#endif /* SS_DEBUG */

/* The following is only for system table bug fixes.
 */
rs_key_t *rs_relh_takekeybyname(
	void*           cd,
	rs_relh_t*      relh,
    rs_entname_t*   keyname);

rs_key_t* rs_relh_refkeybyid(
    void*      cd,
    rs_relh_t* relh,
    ulong      keyid);

void rs_relh_printsamples(
	void*      cd,
    rs_relh_t* relh,
    int        level);

void rs_relh_modifyttypeifsystable(
    void* cd,
    rs_relh_t* relh);

SS_INLINE char* rs_relh_rowcheckcolname(
	void*      cd,
	rs_relh_t* relh);

#ifdef SS_MME
rs_ano_t rs_relh_getdifferentiatingano(
        rs_sysi_t*      cd,
        rs_relh_t*      relh);
#endif

#ifndef NOT_NOTNULL_ANOARRAY
SS_INLINE rs_ano_t *rs_relh_notnull_anoarray(
       rs_sysi_t* cd, 
       rs_relh_t* relh);

void rs_relh_notnull_anoarray_create(
       rs_sysi_t* cd, 
       rs_relh_t* relh);
#endif /* NOT_NOTNULL_ANOARRAY */

void rs_relh_initautoincrement(
        void*      cd,
        rs_relh_t* relh);

long rs_relh_readautoincrement_seqid(
        void*      cd,
        rs_relh_t* relh);

void rs_relh_setautoincrement_seqid(
        void*      cd,
        rs_relh_t* relh,
        long       seq_id);

#if defined(RS0RELH_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *              rs_relh_ntuples
 * 
 * Returns the number of tuples in a relation.
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
 *      Number of tuples in the relation
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE ss_int8_t rs_relh_ntuples(
        void*      cd,
        rs_relh_t* relh
) {
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(rs_cardin_ntuples(cd, relh->rh_cardin));
}

/*##**********************************************************************\
 * 
 *              rs_relh_triggerstr
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      index - 
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
SS_INLINE char* rs_relh_triggerstr(
        void* cd __attribute__ ((unused)),
        rs_relh_t* relh,
        int index)
{
        ss_dprintf_1(("%s:rs_relh_triggerstr:%d\n", __FILE__, index));
        CHECK_RELH(relh);

        if (!su_pa_indexinuse(relh->rh_triggers, index)) {
            return(NULL);
        } else {
            relh_trig_t* trig;
            trig = (relh_trig_t*)su_pa_getdata(relh->rh_triggers, index);
            return(trig->t_str);
        }
}

/*##**********************************************************************\
 * 
 *              rs_relh_triggername
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 *      index - 
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
SS_INLINE rs_entname_t* rs_relh_triggername(
        void* cd __attribute__ ((unused)),
        rs_relh_t* relh,
        int index)
{
        ss_dprintf_1(("%s:rs_relh_triggername:%d\n", __FILE__, index));
        CHECK_RELH(relh);

        if (!su_pa_indexinuse(relh->rh_triggers, index)) {
            return(NULL);
        } else {
            relh_trig_t* trig;
            trig = (relh_trig_t*)su_pa_getdata(relh->rh_triggers, index);
            return(trig->t_name);
        }
}

/*##**********************************************************************\
 * 
 *              rs_relh_reltype
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
SS_INLINE rs_reltype_t rs_relh_reltype(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_reltype);
}

/*##**********************************************************************\
 * 
 *              rs_relh_isnocheck
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
SS_INLINE bool rs_relh_isnocheck(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_nocheck);
}

/*##**********************************************************************\
 * 
 *              rs_relh_isreadonly
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE bool rs_relh_isreadonly(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_readonly > 0);
}

/*##**********************************************************************\
 * 
 *              rs_relh_name
 * 
 * Member of the SQL function block.
 * Returns a pointer to the relation name string
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
 *      Pointer into a relation name
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE char* rs_relh_name(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_name\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_name);
        return(rs_entname_getname(relh->rh_name));
}

/*##**********************************************************************\
 * 
 *              rs_relh_entname
 * 
 * Returns entity name object of the relation.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
SS_INLINE rs_entname_t* rs_relh_entname(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_entname\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_name);
        return(relh->rh_name);
}

/*##**********************************************************************\
 * 
 *              rs_relh_schema
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      relh - in
 *              
 *              
 * Return value  - ref : 
 * 
 *      Schema of relh, or NULL if no schema.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE char* rs_relh_schema(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_schema\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        return(rs_entname_getschema(relh->rh_name));
}

/*##**********************************************************************\
 * 
 *              rs_relh_catalog
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
SS_INLINE char* rs_relh_catalog(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        return (rs_entname_getcatalog(relh->rh_name));
}

/*##**********************************************************************\
 * 
 *              rs_relh_issysrel
 * 
 * Checks if the relation handle is a system relation handle.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
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
SS_INLINE bool rs_relh_issysrel(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_issysrel\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        if (RS_SDEFS_ISSYSID(relh->rh_relid)) {
            return(TRUE);
        }
        return(RS_SDEFS_ISSYSSCHEMA(rs_entname_getschema(relh->rh_name)));
}

SS_INLINE bool rs_relh_isbasetable(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_isbasetable\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_basetablep);
}

SS_INLINE bool rs_relh_ishistorytable(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_ishistorytable\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_historytablep);
}

SS_INLINE bool rs_relh_issync(
        void*      cd,
        rs_relh_t* relh)
{
        ss_dprintf_1(("%s: rs_relh_issync\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_syncp);
}

/*##**********************************************************************\
 * 
 *              rs_relh_refkeys
 * 
 * Returns the reference keys of a relation. These are used for referential
 * integrity.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value - ref : 
 * 
 *      Pointer to a su_pa_t array containing the reference keys
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE su_pa_t* rs_relh_refkeys(
        void*       cd,
        rs_relh_t*  relh)
{
        ss_dprintf_1(("%s: rs_relh_refkeys\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_RELH(relh);
        ss_dassert(relh->rh_refkey_pa);
        return(relh->rh_refkey_pa);
}

/*##**********************************************************************\
 * 
 *              rs_relh_isaborted
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE bool rs_relh_isaborted(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_aborted);
}

/*##**********************************************************************\
 * 
 *              rs_relh_isddopactive
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      relh - 
 *              
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE bool rs_relh_isddopactive(
        void*      cd,
        rs_relh_t* relh)
{
        SS_NOTUSED(cd);
        CHECK_RELH(relh);

        return(relh->rh_ddopactive > 0);
}

SS_INLINE char* rs_relh_rowcheckcolname(
        void*      cd __attribute__ ((unused)),
        rs_relh_t* relh)
{
        CHECK_RELH(relh);

        return(relh->rh_rowcheckcolname);
}

#ifndef NOT_NOTNULL_ANOARRAY
/*##**********************************************************************\
 * 
 *              rs_relh_notnull_anoarray
 * 
 * Returns array of attribute numbers (=rs_ano_t) in relh 
 * which has NOT NULL property.
 * Array is terminated with RS_ANO_NULL. 
 * Array is never NULL but first element can be RS_ANO_NULL.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      relh - in, use
 *              relation handle
 *
 * Return value - rs_ano_t*
 * 
 *      Array of attribute # which has NOT NULL property (pseudo attributes are not counted).
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE rs_ano_t *rs_relh_notnull_anoarray(
        rs_sysi_t*  cd,
        rs_relh_t*  relh)
{
        CHECK_RELH(relh);

        /* If already created NOT NULL property array of ano's then return it */
        if (relh->rh_notnull_anoarray == NULL) {
            rs_relh_notnull_anoarray_create(cd, relh);
        }

        return relh->rh_notnull_anoarray;
}
#endif /* NOT_NOTNULL_ANOARRAY */

#endif /* defined(RS0RELH_C) || defined(SS_USE_INLINE) */

#endif /* RS0RELH_H */
