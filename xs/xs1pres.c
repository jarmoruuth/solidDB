/*************************************************************************\
**  source       * xs1pres.c
**  directory    * xs
**  description  * Presorter for solid sort utility
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

#include <ssc.h>
#include <ssdebug.h>
#include <sslimits.h>

#include <su0error.h>
#include <su0parr.h>
#include "xs2tupl.h"
#include "xs0qsort.h"
#include "xs2cmp.h"
#include "xs1pres.h"
#include "xs0type.h"

#if 0

#ifdef SS_DEBUG
#undef SS_MAXALLOCSIZE
#define SS_MAXALLOCSIZE (1024 * 10)
#endif

#elif INT_BIT == 16

#undef SS_MAXALLOCSIZE
#define SS_MAXALLOCSIZE (1024 * 31)

#endif /* 0 */

struct xs_presorter_st {
        xs_streamarr_t*     ps_streamarr;
        char**              ps_bufarr;
        xs_hmem_t**         ps_hmemarr;
        size_t              ps_nbuf;
        size_t              ps_bufidx;

        char*               ps_buf;
        size_t              ps_bufsize;
        char*               ps_bufend;
        char*               ps_bufpos;
        rs_sysinfo_t*       ps_cd;
        rs_ttype_t*         ps_ttype;
        rs_ano_t*           ps_anomap;
        uint*               ps_cmpcondarr;
        rs_ano_t            ps_nattrs;
        uint                ps_ntuples;
        su_pa_t*            ps_tuplearray;
        xs_mem_t*           ps_memmgr;
        xs_qcomparefp_t     ps_comp_fp;
};


/*##**********************************************************************\
 * 
 *		xs_presorter_init
 * 
 * Creates a presorter object
 * 
 * Parameters : 
 * 
 *	cd - in, hold
 *		client data
 *		
 *	ttype - in, hold
 *		tuple type
 *		
 *	streamarr - in out, hold
 *		output stream array for mergesort
 *
 *      anomap - in, hold
 *          attribute number map from vtpl to sql tval/ttype
 *
 *	comp_fp - in, hold
 *		Compare function
 *		
 *      cmpcondarr - in, hold
 *          comparison condition array needed for vtpl_condcompare()
 *          (used by quick sort)
 *
 *	nbuf - in
 *		number of buffers to use in presorting
 *
 *      memmgr - in, hold
 *          buffer memory manager
 *		
 * Return value - give : 
 *      pointer to created presorter object
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_presorter_t* xs_presorter_init(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        xs_streamarr_t* streamarr,
        rs_ano_t* anomap,
        xs_qcomparefp_t comp_fp,
        uint* cmpcondarr,
        uint nbuf,
        xs_mem_t* memmgr)
{
        size_t i;
        xs_presorter_t* presorter;

        presorter = SSMEM_NEW(xs_presorter_t);
        presorter->ps_memmgr = memmgr;
        presorter->ps_streamarr = streamarr;
        presorter->ps_nbuf = nbuf;
        if (presorter->ps_nbuf == 0) {
            presorter->ps_nbuf = 1;
        }
        presorter->ps_bufsize = xs_mem_getblocksize(presorter->ps_memmgr);
        presorter->ps_bufarr =
            SsMemAlloc(sizeof(char*) * (presorter->ps_nbuf + 1));
        presorter->ps_hmemarr =
            SsMemAlloc(sizeof(xs_hmem_t*) * presorter->ps_nbuf);
        presorter->ps_tuplearray = su_pa_init();
        presorter->ps_nattrs = rs_ttype_nattrs(cd, ttype);

        presorter->ps_anomap = anomap;
        presorter->ps_cmpcondarr = cmpcondarr;
        for (i = 0; i < presorter->ps_nbuf; i++) {
            presorter->ps_hmemarr[i] =
                xs_mem_allocreserved(
                    presorter->ps_memmgr,
                    &presorter->ps_bufarr[i]);
            if (presorter->ps_hmemarr[i] == NULL) {
                presorter->ps_bufarr[i] = NULL;
                presorter->ps_nbuf = i;
                xs_presorter_done(presorter);
                return (NULL);
            }
        }
        presorter->ps_bufarr[i] = NULL;
        presorter->ps_bufidx = 0;
        presorter->ps_buf = presorter->ps_bufarr[presorter->ps_bufidx];
        presorter->ps_bufpos = presorter->ps_buf;
        presorter->ps_bufend = presorter->ps_buf + presorter->ps_bufsize;
        presorter->ps_cd = cd;
        presorter->ps_ttype = ttype;
        presorter->ps_ntuples = 0;
        presorter->ps_comp_fp = comp_fp;

        ss_dassert(presorter != NULL);
        ss_dassert(presorter->ps_bufarr != NULL);
        ss_dassert(presorter->ps_anomap != NULL);
        ss_dassert(presorter->ps_ttype != NULL);
        ss_dassert(presorter->ps_cd != NULL);
        ss_dassert(presorter->ps_streamarr != NULL);
        ss_dassert(presorter->ps_tuplearray != NULL);
        return (presorter);
}

/*##**********************************************************************\
 * 
 *		xs_presorter_done
 * 
 * Deletes a presorter object
 * 
 * Parameters : 
 * 
 *	presorter - in, take
 *		pointer to presorter
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_presorter_done(
        xs_presorter_t* presorter)
{
        size_t i;
        bool succp;

        ss_dassert(presorter != NULL);
        ss_dassert(presorter->ps_bufarr != NULL);
        ss_dassert(presorter->ps_tuplearray != NULL);

        succp = xs_mem_reserveonfree(
                    presorter->ps_memmgr,
                    presorter->ps_nbuf);
        ss_dassert(succp);
        for (i = 0; i < presorter->ps_nbuf; i++) {
            xs_mem_free(presorter->ps_memmgr, presorter->ps_hmemarr[i]);
        }
        SsMemFree(presorter->ps_hmemarr);
        SsMemFree(presorter->ps_bufarr);
        su_pa_done(presorter->ps_tuplearray);
        SsMemFree(presorter);
}

/*##**********************************************************************\
 * 
 *		xs_presorter_flush
 * 
 * Flushes (after presorting) the presort buffer
 * 
 * Parameters : 
 * 
 *	presorter - in out, use
 *		pointer to presorter
 *
 *      p_errh - out, give
 *          when error occurs a newly allocated error handle
 *          is given if p_errh != NULL
 *
 * Return value :
 *      TRUE when successful
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_presorter_flush(
        xs_presorter_t* presorter,
        rs_err_t** p_errh)
{
        uint i;
        vtpl_t* vtpl;
        bool succp = TRUE;
        xs_stream_t* stream;
        vtpl_index_t vtpl_size;

        ss_dassert(presorter != NULL);
        ss_dassert(presorter->ps_bufarr != NULL);
        ss_dassert(presorter->ps_ttype != NULL);
        ss_dassert(presorter->ps_cd != NULL);
        ss_dassert(presorter->ps_streamarr != NULL);
        ss_dassert(presorter->ps_tuplearray != NULL);

        if (presorter->ps_ntuples == 0) {
            return (TRUE);
        }
        if (presorter->ps_ntuples > 1) {
            xs_qsort(
                su_pa_datastart(presorter->ps_tuplearray),
                presorter->ps_ntuples,
                sizeof(void*),
                presorter->ps_comp_fp,
                presorter->ps_cmpcondarr);
        }
        stream = xs_streamarr_nextstream(presorter->ps_streamarr);
        if (stream == NULL) {
            succp = FALSE;
        } else {
            su_pa_do_get(presorter->ps_tuplearray, i, vtpl) {
                vtpl_size = vtpl_grosslen(vtpl) + va_lenlen((va_t*)vtpl);
                succp = xs_stream_append(
                            stream,
                            vtpl,
                            vtpl_size,
                            p_errh);
                if (!succp) {
                    break;
                }
            }
        }
        if (succp) {
            succp = xs_stream_seteoratend(stream, p_errh);
        }

        presorter->ps_ntuples = 0;
        presorter->ps_bufidx = 0;
        presorter->ps_buf = presorter->ps_bufarr[presorter->ps_bufidx];
        presorter->ps_bufpos = presorter->ps_buf;
        presorter->ps_bufend = presorter->ps_buf + presorter->ps_bufsize;
        su_pa_do(presorter->ps_tuplearray, i) {
            su_pa_remove(presorter->ps_tuplearray, i);
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_presorter_addtuple
 * 
 * Adds a tuple into presorter
 * 
 * Parameters : 
 * 
 *	presorter - in out, use
 *		pointer to presorter
 *		
 *	tval - in, use
 *		pointer to tuple value
 *
 *      p_errh - out, give
 *          when error occurs a newly allocated error handle
 *          is given if p_errh != NULL
 *		
 * Return value :
 *      TRUE when successful
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_presorter_addtuple(
        xs_presorter_t* presorter,
        rs_tval_t* tval,
        rs_err_t** p_errh)
{
        bool succp;
        char* newbufpos;

        ss_dassert(presorter != NULL);
        ss_dassert(presorter->ps_bufarr != NULL);
        ss_dassert(presorter->ps_anomap != NULL);
        ss_dassert(presorter->ps_ttype != NULL);
        ss_dassert(presorter->ps_cd != NULL);
        ss_dassert(presorter->ps_streamarr != NULL);
        ss_dassert(presorter->ps_tuplearray != NULL);
        ss_dassert(tval != NULL);
        ss_dassert(presorter->ps_bufpos < presorter->ps_bufend);

        succp = xs_tuple_makevtpl(
                presorter->ps_cd,
                presorter->ps_ttype,
                tval,
                presorter->ps_nattrs,
                presorter->ps_anomap,
                presorter->ps_bufpos,
                presorter->ps_bufend - presorter->ps_bufpos,
                &newbufpos);
        if (!succp) {
            /* Not enough space, switch to next buffer */
            if ((presorter->ps_bufidx + 1) < presorter->ps_nbuf) {
                presorter->ps_bufidx++;
                presorter->ps_buf = presorter->ps_bufarr[presorter->ps_bufidx];
                presorter->ps_bufpos = presorter->ps_buf;
                presorter->ps_bufend = presorter->ps_buf + presorter->ps_bufsize;
                succp = xs_tuple_makevtpl(
                            presorter->ps_cd,
                            presorter->ps_ttype,
                            tval,
                            presorter->ps_nattrs,
                            presorter->ps_anomap,
                            presorter->ps_bufpos,
                            presorter->ps_bufend - presorter->ps_bufpos,
                            &newbufpos);
                if (!succp) {
                    if (p_errh != NULL) {
                        rs_error_create(p_errh, XS_ERR_TOOLONGROW);
                    }
                    return (succp);
                }
            }
        }
        if (!succp) {
            /* All presort buffers are full, presort
             * and flush them all.
             */
            succp = xs_presorter_flush(presorter, p_errh);
            if (!succp) {
                return (succp);
            }
            succp = xs_tuple_makevtpl(
                    presorter->ps_cd,
                    presorter->ps_ttype,
                    tval,
                    presorter->ps_nattrs,
                    presorter->ps_anomap,
                    presorter->ps_bufpos,
                    presorter->ps_bufend - presorter->ps_bufpos,
                    &newbufpos);
            if (!succp) {
                if (p_errh != NULL) {
                    rs_error_create(p_errh, XS_ERR_TOOLONGROW);
                }
                return (succp);
            }
        }
        su_pa_insertat(
            presorter->ps_tuplearray,
            presorter->ps_ntuples,
            presorter->ps_bufpos);
        presorter->ps_ntuples++;
        presorter->ps_bufpos = newbufpos;
        if (presorter->ps_bufpos == presorter->ps_bufend) {
            if ((presorter->ps_bufidx + 1) < presorter->ps_nbuf) {
                presorter->ps_bufidx++;
                presorter->ps_buf = presorter->ps_bufarr[presorter->ps_bufidx];
                presorter->ps_bufpos = presorter->ps_buf;
                presorter->ps_bufend = presorter->ps_buf + presorter->ps_bufsize;
            } else {
                succp = xs_presorter_flush(presorter, p_errh);
            }
        }
        return (succp);
}

