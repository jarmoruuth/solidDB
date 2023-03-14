/*************************************************************************\
**  source       * xs1sort.c
**  directory    * xs
**  description  * Sorter object for solid external sort utility
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

#include <uti0vcmp.h>

#include <su0error.h>
#include <ui0msg.h>

#include "xs2stre.h"
#include "xs2tupl.h"
#include "xs2cmp.h"
#include "xs1pres.h"
#include "xs1merg.h"
#include "xs1sort.h"

typedef enum {
        SORTSTAT_INIT,
        SORTSTAT_ADD,
        SORTSTAT_MERGE,
        SORTSTAT_CURSOR,
        SORTSTAT_ERROR,
        SORTSTAT_INIT_0ORDERBY,
        SORTSTAT_ADD_0ORDERBY
} xs_sortstat_t;

struct xs_sorter_st {
        xs_sortstat_t       s_status;
        rs_sysinfo_t*       s_cd;
        rs_ttype_t*         s_ttype;
        rs_ano_t            s_nattrs;
        rs_tval_t*          s_tval;
        rs_ano_t*           s_anomap;
        xs_stream_t*        s_resultstream;
        xs_mem_t*           s_memmgr;
        xs_tfmgr_t*         s_tfmgr;
        uint                s_maxnbuf;
        uint                s_maxfiles;

        su_list_t*          s_orderby_list;
        xs_streamarr_t*     s_streamarr;
        xs_presorter_t*     s_presorter;
        xs_merge_t*         s_merge;
        uint*               s_cmpcondarr;
        ulong               s_stepsizebytes;
        uint                s_stepsizerows;
	xs_qcomparefp_t     s_comp_fp;
        ulong               s_nlines;
};

/*##**********************************************************************\
 * 
 *		xs_sorter_init
 * 
 * Creates a tuple sorter object
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *		
 *	ttype - in, hold
 *		tuple type
 *
 *      memmgr - in out, hold
 *          memory manager
 *
 *      tfmgr - in out, hold
 *          temp file manager
 *
 *	orderby_list - in, take
 *		list of xs_acond_t*'s
 *
 *      maxnbuf - in
 *          max. number of buffers to be used by the sort
 *
 *	maxfiles - in
 *		maximum number of sort stream files to be used
 *
 *      stepsizebytes - in
 *          max task step size in handled bytes during merge
 *
 *      stepsizerows - in
 *          max task step size in handled rows
 *
 *	comp_fp - in, hold
 *		Compare function
 *		
 * Return value - give :
 *      pointer to created sorter or
 *      NULL when not enough resources were available
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_sorter_t* xs_sorter_init(
        rs_sysinfo_t* cd,
        xs_mem_t* memmgr,
        xs_tfmgr_t* tfmgr,
        rs_ttype_t* ttype,
        su_list_t* orderby_list,
        uint maxnbuf,
        uint maxfiles,
        ulong stepsizebytes,
        uint stepsizerows,
        xs_qcomparefp_t comp_fp)
{
        xs_sorter_t* sorter;
        bool succp = TRUE;
        rs_ano_t* sellist;
        size_t i;

        ss_dassert(cd != NULL);
        ss_dassert(ttype != NULL);

        if (orderby_list == NULL) {
            maxnbuf = 2;
            maxfiles = 1;
        } else {
            ss_dassert(su_list_length(orderby_list) != 0);

            ss_dassert(maxnbuf >= 3);
            ss_dassert(maxfiles >= 3);
        }

        succp = xs_mem_reserve(memmgr, maxnbuf);
        if (!succp) {
            char* sqlstr = SsSQLTrcGetStr();
            
            if(sqlstr != NULL) {
                ui_msg_warning(XS_MSG_UNABLE_TO_RESERVE_MEM_INFO_DDS, 
                            maxnbuf, xs_mem_getnblocksavail(memmgr), sqlstr);
            } else {
                ui_msg_warning(XS_MSG_UNABLE_TO_RESERVE_MEM_DD, 
                            maxnbuf, xs_mem_getnblocksavail(memmgr));
            }
            
            if (orderby_list != NULL) {
                su_list_done(orderby_list);
            }
            return (NULL);
        }

        sorter = SSMEM_NEW(xs_sorter_t);
        sorter->s_cd = cd;
        sorter->s_ttype = ttype;
        sorter->s_nattrs = rs_ttype_nattrs(cd, ttype);
        sorter->s_memmgr = memmgr;
        sorter->s_tfmgr = tfmgr;
        if (maxnbuf < maxfiles) {
            maxfiles = maxnbuf;
        }
        sorter->s_maxnbuf = maxnbuf;
        sorter->s_maxfiles = maxfiles;

        sellist = SsMemAlloc(sizeof(rs_ano_t) * sorter->s_nattrs);
        for (i = 0; i < (size_t)sorter->s_nattrs; i++) {
            sellist[i] = (rs_ano_t)i;
        }
        sorter->s_anomap = 
            xs_tuple_anomap_init(
                cd,
                sorter->s_nattrs,
                sellist,
                orderby_list);
        SsMemFree(sellist);
        sorter->s_tval = NULL;

        sorter->s_orderby_list = orderby_list;
        sorter->s_merge = NULL;
        sorter->s_presorter = NULL;
        sorter->s_stepsizebytes = stepsizebytes;
        sorter->s_stepsizerows = stepsizerows;
        sorter->s_resultstream = NULL;
        sorter->s_comp_fp = comp_fp;
        sorter->s_nlines = 0UL;

        if (orderby_list == NULL) {
            sorter->s_status = SORTSTAT_INIT_0ORDERBY;
            sorter->s_cmpcondarr = NULL;
            sorter->s_streamarr = NULL;
            sorter->s_presorter = NULL;
        } else {
            sorter->s_status = SORTSTAT_INIT;

            sorter->s_cmpcondarr =
                xs_tuple_cmpcondarr_init(sorter->s_orderby_list);

            sorter->s_streamarr = xs_streamarr_init((int)maxfiles, tfmgr);
            if (sorter->s_streamarr == NULL) {
                succp = FALSE;
            }
            if (succp) {
                sorter->s_presorter =
                    xs_presorter_init(
                        cd,
                        ttype,
                        sorter->s_streamarr,
                        sorter->s_anomap,
                        comp_fp,
                        sorter->s_cmpcondarr,
                        maxnbuf - 1,    /* 1 has been taken by streamarr */
                        memmgr);
                if (sorter->s_presorter == NULL) {
                    succp = FALSE;
                }
            }
            if (!succp) {
                xs_sorter_done(sorter);
                sorter = NULL;
            }
        }
        return (sorter);
}

/*##**********************************************************************\
 * 
 *		xs_sorter_done
 * 
 * Deletes a sorter object
 * 
 * Parameters : 
 * 
 *	sorter - in, take
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
void xs_sorter_done(
        xs_sorter_t* sorter)
{
        if (sorter->s_tval != NULL) {
            rs_tval_free(sorter->s_cd, sorter->s_ttype, sorter->s_tval);
        }
        if (sorter->s_presorter != NULL) {
            xs_presorter_done(sorter->s_presorter);
        }
        if (sorter->s_streamarr != NULL) {
            xs_streamarr_done(sorter->s_streamarr);
        }
        if (sorter->s_resultstream != NULL) {
            xs_stream_done(sorter->s_resultstream);
        }
        if (sorter->s_merge != NULL) {
            xs_merge_done(sorter->s_merge);
        }
        if (sorter->s_anomap != NULL) {
            xs_tuple_anomap_done(sorter->s_anomap);
        }
        if (sorter->s_cmpcondarr != NULL) {
            xs_tuple_cmpcondarr_done(sorter->s_cmpcondarr);
        }
        if (sorter->s_orderby_list != NULL) {
            su_list_done(sorter->s_orderby_list);
        }
        xs_mem_unreserve(sorter->s_memmgr, sorter->s_maxnbuf);
        SsMemFree(sorter);
}

/*##**********************************************************************\
 * 
 *		xs_sorter_addtuple
 * 
 * Adds a tuple
 * 
 * Parameters : 
 * 
 *	sorter - in out, use
 *		pointer to sorter object
 *		
 *	tval - in, use
 *		pointer to tuple value object
 *
 *      p_errh - out, give
 *          when error occurs a newly allocated error handle
 *          is given if p_errh != NULL
 *		
 * Return value :
 *      TRUE when successful or
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_sorter_addtuple(
        xs_sorter_t* sorter,
        rs_tval_t* tval,
        rs_err_t** p_errh)
{
        bool succp = TRUE;

        ss_dassert(sorter != NULL);
        ss_dassert(tval != NULL);

        switch (sorter->s_status) {
            case SORTSTAT_INIT:
                sorter->s_status = SORTSTAT_ADD;
                /* FALLTHROUGH */
            case SORTSTAT_ADD:
                ss_dassert(sorter->s_presorter != NULL);
                succp = xs_presorter_addtuple(sorter->s_presorter, tval, p_errh);
                if (!succp) {
                    sorter->s_status = SORTSTAT_ERROR;
                }
                break;
            case SORTSTAT_ERROR:
                succp = FALSE;
                break;
            case SORTSTAT_INIT_0ORDERBY:
                {
                    xs_streamstatus_t ssta = 0;
                    sorter->s_status = SORTSTAT_ADD_0ORDERBY;
                    ss_dassert(sorter->s_resultstream == NULL);
                    sorter->s_resultstream = xs_stream_init(sorter->s_tfmgr);
                    if (sorter->s_resultstream != NULL) {
                        ssta = xs_stream_rewrite(sorter->s_resultstream);
                    }
                    if (sorter->s_resultstream == NULL || ssta == SSTA_ERROR) {
                        rs_error_create(p_errh, XS_OUTOFPHYSDISKSPACE);
                        sorter->s_status = SORTSTAT_ERROR;
                        succp = FALSE;
                        break;
                    }
                }
                /* FALLTHROUGH */
            case SORTSTAT_ADD_0ORDERBY:
                ss_dassert(sorter->s_resultstream != NULL);
                succp = xs_tuple_makevtpl2stream(
                            sorter->s_cd,
                            sorter->s_ttype,
                            tval,
                            sorter->s_nattrs,
                            sorter->s_resultstream,
                            p_errh);
                break;
            default:
                ss_rc_error(sorter->s_status);
        }
        sorter->s_nlines++;
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_sorter_merge
 * 
 * Runs merge sort to the tuple stream fed into the sorter
 * 
 * Parameters : 
 * 
 *	sorter - in out, use
 *		pointer to sorter object
 *		
 *	p_emptyset - out
 *		If set is empty, FALSE is set into *p_emptyset. Otherwise
 *		TRUE is set into *p_emptyset.
 *
 *      p_errh - out, give
 *          when error occurs a newly allocated error handle
 *          is given if p_errh != NULL
 *		
 * Return value :
 *      XS_RC_CONT when job is still incomplete
 *      XS_RC_SUCC when job is finished
 *      XS_RC_ERROR when error occured
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_ret_t xs_sorter_merge(
        xs_sorter_t* sorter,
        bool* p_emptyset,
        rs_err_t** p_errh)
{
        xs_ret_t rc = 0;
        bool succp;
        bool onerun = FALSE;
        xs_stream_t* writestream;
        su_pa_t* readstream_pa;

        ss_dassert(sorter != NULL);
        ss_dassert(p_emptyset != NULL);

        *p_emptyset = TRUE;
        switch (sorter->s_status) {
            case SORTSTAT_INIT:
                ss_bprintf_1(("xs_sorter_merge: 0 lines\n"));
                ss_dassert(sorter->s_nlines == 0);
                sorter->s_status = SORTSTAT_CURSOR;
                sorter->s_resultstream = NULL;
                return (XS_RC_SUCC);
            case SORTSTAT_ADD:
                ss_bprintf_1(("xs_sorter_merge: starting to sort %lu lines\n",
                    sorter->s_nlines));
                ss_dassert(sorter->s_presorter != NULL);
                *p_emptyset = FALSE;
                succp = xs_presorter_flush(sorter->s_presorter, p_errh);
                if (!succp) {
                    sorter->s_status = SORTSTAT_ERROR;
                    return (XS_RC_ERROR);
                }
                xs_presorter_done(sorter->s_presorter);
                sorter->s_presorter = NULL;
                onerun = xs_streamarr_endofdistribute(
                            sorter->s_streamarr,
                            &writestream,
                            &readstream_pa);

                if (onerun) {
                    rc = XS_RC_SUCC;
                    xs_stream_link(writestream);
                    sorter->s_resultstream = writestream;
                } else {
                    rc = XS_RC_CONT;
                    sorter->s_status = SORTSTAT_MERGE;
                    if (sorter->s_maxnbuf > sorter->s_maxfiles) {
                        xs_mem_unreserve(
                            sorter->s_memmgr,
                            sorter->s_maxnbuf - sorter->s_maxfiles);
                        sorter->s_maxnbuf = sorter->s_maxfiles;
                    }
                    sorter->s_merge =
                        xs_merge_init(
                            readstream_pa,
                            writestream,
                            sorter->s_comp_fp,
                            sorter->s_cmpcondarr,
                            sorter->s_stepsizebytes,
                            sorter->s_stepsizerows);
                    if (sorter->s_merge == NULL) {
                        rs_error_create(p_errh, XS_ERR_SORTFAILED);
                        sorter->s_status = SORTSTAT_ERROR;
                        return (XS_RC_ERROR);
                    }
                }
                break;
            case SORTSTAT_MERGE:
                *p_emptyset = FALSE;
                rc = xs_merge_step(
                        sorter->s_merge,
                        &sorter->s_resultstream,
                        p_errh);
                break;
            case SORTSTAT_CURSOR:
                ss_error;
            case SORTSTAT_ERROR:
                rs_error_create(p_errh, XS_ERR_SORTFAILED);
                return (XS_RC_ERROR);
            case SORTSTAT_INIT_0ORDERBY:
                ss_bprintf_1(("xs_sorter_merge: stored 0 lines\n"));
                sorter->s_status = SORTSTAT_CURSOR;
                ss_dassert(sorter->s_resultstream == NULL);
                return (XS_RC_SUCC);
            case SORTSTAT_ADD_0ORDERBY:
                {
                    xs_streamstatus_t ssta;

                    ss_rc_dassert(sorter->s_maxnbuf == 2, sorter->s_maxnbuf);
                    ss_bprintf_1(("xs_sorter_merge: stored %lu lines\n",
                        sorter->s_nlines));
                    sorter->s_tval = rs_tval_create(sorter->s_cd, sorter->s_ttype);
                    ssta = xs_stream_initfetch(
                                    sorter->s_resultstream);
                    switch (ssta) {
                        case SSTA_RUN:
                            *p_emptyset = FALSE;
                            break;
                        default:
                            ss_rc_derror(ssta);
                            /* the above fired once at a customer.
                             * Changed from
                             * ss_rc_error to ss_rc_derror
                             * by Pete 1999-02-17
                             */
                            /* FALLTHROUGH */
                        case SSTA_ERROR:
                            rs_error_create(p_errh, XS_ERR_SORTFAILED);
                            return (XS_RC_ERROR);
                    }
                    sorter->s_status = SORTSTAT_CURSOR;
                    return (XS_RC_SUCC);
                }
            default:
                ss_rc_error(sorter->s_status);
        }

        if (rc == XS_RC_SUCC) {
            xs_streamstatus_t ssta;
            xs_streamarr_done(sorter->s_streamarr);
            sorter->s_streamarr = NULL;
            ssta = xs_stream_initfetch(
                            sorter->s_resultstream);
#if 0 /* Changed by pete 1996-10-21 to decrease resource usage */
      /* If You change back, remember to change xs_tf_opencursor, too */
            xs_mem_unreserve(
                sorter->s_memmgr,
                sorter->s_maxnbuf - 2);
            sorter->s_maxnbuf = 2;
#else /* 0 */
            xs_mem_unreserve(
                sorter->s_memmgr,
                sorter->s_maxnbuf - 1);
            sorter->s_maxnbuf = 1;
#endif /* 0 */
            if (sorter->s_cmpcondarr != NULL) {
                xs_tuple_cmpcondarr_done(sorter->s_cmpcondarr);
                sorter->s_cmpcondarr = NULL;
            }
            switch (ssta) {
                case SSTA_RUN:
                    *p_emptyset = FALSE;
                    break;
                case SSTA_ERROR:
                    rs_error_create(p_errh, XS_ERR_SORTFAILED);
                    return (XS_RC_ERROR);
                default:
                    ss_rc_error(ssta);
                    break;
            }
            sorter->s_tval = rs_tval_create(sorter->s_cd, sorter->s_ttype);
            sorter->s_status = SORTSTAT_CURSOR;
        } else {
            sorter->s_resultstream = NULL;
        }
        return (rc);
}

/*##**********************************************************************\
 * 
 *		xs_sorter_fetchnext
 * 
 * Fetches next tval from sorted tuple stream
 * 
 * Parameters : 
 * 
 *	sorter - in out, use
 *		pointer to sorter object
 *		
 *	p_tval - out, ref
 *		pointer to storage where the tval pointer is to be
 *          stored
 *		
 * Return value : 
 *      TRUE when successful,
 *      FALSE when failed (probably at end of stream)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_sorter_fetchnext(
        xs_sorter_t* sorter,
        rs_tval_t** p_tval)
{
        xs_streamstatus_t rc;
        bool succp;
        vtpl_t* vtpl;
        size_t sz;

        ss_rc_dassert(sorter->s_status == SORTSTAT_CURSOR, sorter->s_status);
        if (sorter->s_resultstream == NULL) {
            rc = SSTA_EOS;
        } else {
            rc = xs_stream_getnext(
                    sorter->s_resultstream,
                    (void**)&vtpl,
                    &sz);
        }
        if (rc != SSTA_RUN) {
            *p_tval = NULL;
            return (FALSE);
        }
        succp = xs_tuple_filltval(
                    sorter->s_cd,
                    sorter->s_ttype,
                    sorter->s_anomap,
                    vtpl,
                    sorter->s_tval);
        if (!succp) {
            *p_tval = NULL;
        } else {
            *p_tval = sorter->s_tval;
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_sorter_fetchprev
 * 
 * Fetches prev (alreadey fetched) tval from sorted tuple stream
 * 
 * Parameters : 
 * 
 *	sorter - in out, use
 *		pointer to sorter object
 *		
 *	p_tval - out, ref
 *		pointer to storage where the tval pointer is to be
 *          stored
 *		
 * Return value : 
 *      TRUE when successful,
 *      FALSE when failed (propably at beginning of stream)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_sorter_fetchprev(
        xs_sorter_t* sorter,
        rs_tval_t** p_tval)
{
        xs_streamstatus_t rc;
        bool succp;
        vtpl_t* vtpl;
        size_t sz;

        ss_rc_dassert(sorter->s_status == SORTSTAT_CURSOR, sorter->s_status);
        if (sorter->s_resultstream == NULL) {
            rc = SSTA_BOS;
        } else {
            rc = xs_stream_getprev(
                    sorter->s_resultstream,
                    (void**)&vtpl,
                    &sz);
        }
        if (rc != SSTA_RUN) {
            *p_tval = NULL;
            return (FALSE);
        }
        succp = xs_tuple_filltval(
                    sorter->s_cd,
                    sorter->s_ttype,
                    sorter->s_anomap,
                    vtpl,
                    sorter->s_tval);
        if (!succp) {
            *p_tval = NULL;
        } else {
            *p_tval = sorter->s_tval;
        }
        return (succp);
}


/*##**********************************************************************\
 * 
 *		xs_sorter_cursortobegin
 * 
 * Moves the cursor to beginning of a sorted result set
 * 
 * Parameters : 
 * 
 *	sorter - in out, use
 *		sorter object
 *		
 * Return value :
 *      TRUE when OK
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_sorter_cursortobegin(
        xs_sorter_t* sorter)
{
        xs_streamstatus_t rc;

        ss_rc_dassert(sorter->s_status == SORTSTAT_CURSOR, sorter->s_status);
        if (sorter->s_resultstream == NULL) {
            rc = SSTA_EOS;
        } else {
            rc = xs_stream_cursortobegin(sorter->s_resultstream);
        }
        if (rc == SSTA_ERROR) {
            sorter->s_status = SORTSTAT_ERROR;
            return (FALSE);
        }
        return TRUE;
}


/*##**********************************************************************\
 * 
 *		xs_sorter_cursortoend
 * 
 * Moves cursor to end of sorter result set
 * 
 * Parameters : 
 * 
 *	sorter - in out, use
 *		sorter object
 *		
 * Return value :
 *      TRUE when OK
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_sorter_cursortoend(
        xs_sorter_t* sorter)
{
        xs_streamstatus_t rc;

        ss_rc_dassert(sorter->s_status == SORTSTAT_CURSOR, sorter->s_status);
        if (sorter->s_resultstream == NULL) {
            rc = SSTA_BOS;
        } else {
            rc = xs_stream_cursortoend(sorter->s_resultstream);
        }
        if (rc == SSTA_ERROR) {
            sorter->s_status = SORTSTAT_ERROR;
            return (FALSE);
        }
        return (TRUE);
}
