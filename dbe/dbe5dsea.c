/*************************************************************************\
**  source       * dbe5dsea.c
**  directory    * dbe
**  description  * Data search routines. Used to search the data tuples
**               * using a tuple reference.
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

Special data search (datasea) routines are used to search the key
value that contains all attributes (called clustering key, or data tuple)
given the tuple reference.


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

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>

#include <uti0va.h>
#include <uti0vtpl.h>
#include <uti0vcmp.h>

#include <su0list.h>
#include <su0slike.h>

#include <rs0aval.h>
#include <rs0atype.h>
#include <rs0cons.h>
#include <rs0pla.h>

#include "dbe9type.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe5inde.h"
#include "dbe5isea.h"
#include "dbe5dsea.h"
#include "dbe0type.h"
#include "dbe0erro.h"

#define CHK_DATASEA(ds) ss_dassert(SS_CHKPTR(ds) && (ds)->ds_chk == DBE_CHK_DATASEA)

/* Data search data structure.
 */
struct dbe_datasea_st {
        ss_debug(dbe_chk_t ds_chk;)
        char*                   ds_caller;
        dbe_indsea_t*           ds_indsea;
        dbe_btrsea_timecons_t*  ds_tc;
        dbe_index_t*            ds_index;
        void*                   ds_cd;
        su_list_t*              ds_conslist;
        vtpl_t*                 ds_maxvtpl;
        int                     ds_maxvtpl_len;
        bool                    ds_pessimistic;
        bool                    ds_longseqsea;
        rs_key_t*               ds_key;
        SsSemT*                 ds_activesem;
};

/*##**********************************************************************\
 * 
 *		dbe_datasea_init
 * 
 * Initializes are search to find a single key value specified by
 * reftrxid and refvtpl. Typically used to find the data tuple
 * (clustering key entry) of a relation.
 * 
 * Parameters : 
 * 
 *	cd - in, hold
 *		Client data.
 *		
 *	indsys - in, hold
 *		Index system object.
 *		
 *	tc - in, hold
 *		Time constraints.
 *		
 *	conslist - in, hold
 *		List of contraints used to specify the searched tuple.
 *		Can be NULL if no constraints.
 *		
 * Return value - give : 
 * 
 *      Pointer to the data search object.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_datasea_t* dbe_datasea_init(
        void* cd,
        dbe_index_t* index,
        rs_key_t* key,
        dbe_btrsea_timecons_t* tc,
        su_list_t* conslist,
        bool pessimistic,
        char* caller)
{
        dbe_datasea_t* datasea;

        ss_dprintf_1(("dbe_datasea_init\n"));

        datasea = SsMemAlloc(sizeof(dbe_datasea_t));

        ss_debug(datasea->ds_chk = DBE_CHK_DATASEA);

        datasea->ds_caller = caller;
        datasea->ds_tc = tc;
        datasea->ds_index = index;
        datasea->ds_indsea = NULL;
        datasea->ds_key = key;
        datasea->ds_cd = cd;
        datasea->ds_conslist = conslist;
        datasea->ds_maxvtpl = NULL;
        datasea->ds_maxvtpl_len = 0;
        datasea->ds_pessimistic = pessimistic;
        datasea->ds_longseqsea = FALSE;
        datasea->ds_activesem = rs_sysi_givesearchsem(cd);

        return(datasea);
}

/*##**********************************************************************\
 * 
 *		dbe_datasea_reset
 * 
 * Resets data search. After reset the search is in same state as after init.
 * 
 * Parameters : 
 * 
 *	datasea - in, use
 *		Data search object.
 *		
 *	conslist - in
 *		New constraints.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 *      Note that time constraints (tc) are stored as reference to upper
 *      level object and automatically change when upper level values
 *      are changed.
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_datasea_reset(
        dbe_datasea_t* datasea,
        su_list_t* conslist)
{
        ss_dprintf_1(("dbe_datasea_reset\n"));

        datasea->ds_conslist = conslist;
        datasea->ds_longseqsea = FALSE;
}

/*##**********************************************************************\
 * 
 *		dbe_datasea_search
 * 
 * Searches a single key value specified by reftrxid and refvtpl.
 * Typically used to find the data tuple (clustering key entry) of
 * a relation.
 * 
 * Parameters : 
 * 
 *	cd - in, hold
 *		Client data.
 *		
 *	datasea - in, take
 *		Data search object.
 *		
 *	refvtpl - in, use
 *		V-tuple value of the searched key value.
 *		
 *      srk - out, ref
 *          Search results is returned in *p_srk.
 *
 * Return value - give : 
 * 
 *      DBE_RC_FOUND
 *      DBE_RC_END
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_ret_t dbe_datasea_search(
        dbe_datasea_t* datasea,
        vtpl_t* refvtpl,
        dbe_trxid_t stmttrxid,
        dbe_srk_t** p_srk)
{
        dbe_searchrange_t sr;
        dbe_ret_t rc;
        int maxvtpl_len;

        CHK_DATASEA(datasea);

        ss_dprintf_4(("dbe_datasea_search:key:\n"));
        ss_output_4(dbe_bkey_dprintvtpl(4, refvtpl));
        SS_PUSHNAME("dbe_datasea_search");

        sr.sr_minvtpl = refvtpl;
        sr.sr_minvtpl_closed = TRUE;

        maxvtpl_len = vtpl_grosslen(refvtpl) + 12;

        if (maxvtpl_len > datasea->ds_maxvtpl_len) {
            datasea->ds_maxvtpl_len = maxvtpl_len;
            if (datasea->ds_maxvtpl != NULL) {
                SsMemFree(datasea->ds_maxvtpl);
            }
            datasea->ds_maxvtpl = SsMemAlloc(maxvtpl_len);
        }

        /* Make max. just one smallest possible increment bigger
        ** than minimum. Thus the resulting range is an exact match!
        */
        vtpl_setvtplwithincrement(datasea->ds_maxvtpl, sr.sr_minvtpl);

        sr.sr_maxvtpl = datasea->ds_maxvtpl;
        sr.sr_maxvtpl_closed = FALSE;

        if (datasea->ds_indsea != NULL) {
            dbe_indsea_reset(
                datasea->ds_indsea,
                datasea->ds_tc,
                &sr,
                datasea->ds_conslist);
        } else {
            datasea->ds_indsea = dbe_indsea_init_ex(
                                    datasea->ds_cd,
                                    datasea->ds_index,
                                    datasea->ds_key,
                                    datasea->ds_tc,
                                    &sr,
                                    datasea->ds_conslist,
                                    LOCK_S,
                                    datasea->ds_pessimistic,
                                    datasea->ds_activesem,
                                    datasea->ds_caller);
            dbe_indsea_setdatasea(datasea->ds_indsea);
        }

        if (datasea->ds_longseqsea) {
            dbe_indsea_setlongseqsea(datasea->ds_indsea);
        }

        do {
            rc = dbe_indsea_next(
                    datasea->ds_indsea,
                    stmttrxid,
                    p_srk);
        } while (rc == DBE_RC_NOTFOUND);

        dbe_indsea_setended(datasea->ds_indsea);

        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 * 
 *		dbe_datasea_done
 * 
 * Releases the data search object.
 * 
 * Parameters : 
 * 
 *	datasea - in, take
 *		Data search object.
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_datasea_done(dbe_datasea_t* datasea)
{
        ss_dprintf_4(("dbe_datasea_done\n"));
        CHK_DATASEA(datasea);

        if (datasea->ds_maxvtpl != NULL) {
            SsMemFree(datasea->ds_maxvtpl);
        }
        if (datasea->ds_indsea != NULL) {
            dbe_indsea_done(datasea->ds_indsea);
        }
        rs_sysi_insertsearchsem(datasea->ds_cd, datasea->ds_activesem);
        SsMemFree(datasea);
}

/*##**********************************************************************\
 * 
 *		dbe_datasea_setlongseqsea
 * 
 * 
 * 
 * Parameters : 
 * 
 *	datasea - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_datasea_setlongseqsea(dbe_datasea_t* datasea)
{
        datasea->ds_longseqsea = TRUE;
}

/*##**********************************************************************\
 * 
 *		dbe_datasea_clearlongseqsea
 * 
 * 
 * 
 * Parameters : 
 * 
 *	datasea - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_datasea_clearlongseqsea(dbe_datasea_t* datasea)
{
        datasea->ds_longseqsea = FALSE;
}
