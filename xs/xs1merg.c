/*************************************************************************\
**  source       * xs1merg.c
**  directory    * xs
**  description  * Polyphase merge
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

#include <ssstring.h>
#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>

#include <su0bsrch.h>
#include <su0parr.h>
#include <su0error.h>

#include <uti0vtpl.h>

#include "xs0qsort.h"
#include "xs2stre.h"
#include "xs1merg.h"

/* Local return codes */

typedef enum {
        MGS_INIT,
        MGS_RUN,
        MGS_OK,     /* No error */
        MGS_EOS,    /* End of stream */
        MGS_BOS,    /* Beginning of stream */
        MGS_EOR,    /* End of run */
        MGS_EOM,    /* End of merge  */
        MGS_NOP,    /* No operation */
        MGS_ERROR   /* Error during merge */
} mergestatus_t;

typedef struct mgitemstruct {
        int     mgi_stream_index;
        void*   mgi_data;
        size_t  mgi_datalen;
} mgitem_t;

typedef struct {
        mgitem_t** mgtl_array;
        uint mgtl_size;
        uint mgtl_1st;
} mgtuplelist_t;

struct xs_merge_st {
        su_pa_t*          mg_readstream_pa;
        xs_stream_t*      mg_writestream;
	xs_qcomparefp_t   mg_comp_fp;
        void*             mg_comp_context;
        mgtuplelist_t*    mg_tuplelist;
        mergestatus_t     mg_status;
        ulong             mg_stepsizebytes;
        uint              mg_stepsizerows;
};

static int  mgi_cmp(void* key, void* mgi, void* ctx);

static int  merge_nextrun(xs_merge_t* mg);

static int  mgtl_initialize(xs_merge_t* mg);

static int  mgtl_addfromstream(xs_merge_t* mg, xs_stream_t* readstream, int stream_index);
static void mgtl_add(xs_merge_t* mg, void* data, size_t datalen, int stream_index);
static int  mgtl_select(xs_merge_t* mg, void** p_data, size_t* p_datalen);

static mgtuplelist_t* mgtl_init(uint nstreams);
static void mgtl_done(mgtuplelist_t* tuplelist);


/*##**********************************************************************\
 * 
 *		xs_merge_init
 * 
 * Polyphase mergesort for presorted blocks
 * 
 * Parameters : 
 * 
 *	readstream_pa - in, hold/take
 *	    Pointer array of the streams containing the presorted	
 *		data blocks.
 *		The su_pa object is in take mode, the stream elements
 *          in hold mode
 *		
 *	writestream - in, take
 *		Empty stream that can be used for merging.
 *		
 *	comp_fp - in, hold
 *		Compare function
 *		
 *	comp_context - in. hold
 *		Special context for compare function (contains ORDER BY list)
 *      
 *      step_size_bytes - in
 *          maximum number of bytes to merge at one step
 *
 *      step_size_rows - in
 *          maximum number of rows to merge at one step
 *      
 *		
 * Return value - give : 
 *      pointer to created merge object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_merge_t* xs_merge_init(
        su_pa_t* readstream_pa,
        xs_stream_t* writestream,
	xs_qcomparefp_t comp_fp,
        void* comp_context,
        ulong step_size_bytes,
        uint step_size_rows)
{

        int i;
        xs_stream_t* readstream;
        xs_merge_t* mg;
        xs_streamstatus_t streamstat = SSTA_RUN;

        su_pa_do_get(readstream_pa, i, readstream) {
            streamstat = xs_stream_rewind(readstream);
            if (streamstat == SSTA_ERROR) {
                break;
            }
        }

        mg = SSMEM_NEW(xs_merge_t);
        mg->mg_readstream_pa = readstream_pa;
        mg->mg_writestream = writestream;
        mg->mg_comp_fp = comp_fp;
        mg->mg_comp_context = comp_context;
        mg->mg_tuplelist = mgtl_init(su_pa_nelems(mg->mg_readstream_pa));
        mg->mg_status = MGS_INIT;
        mg->mg_stepsizebytes = step_size_bytes; 
        mg->mg_stepsizerows = step_size_rows;
        if (streamstat == SSTA_ERROR) {
            xs_merge_done(mg);
            return (NULL);
        }
        return (mg);
}



/*##**********************************************************************\
 * 
 *		xs_merge_done
 * 
 * Polyphase mergesort for presorted blocks
 * 
 * Parameters : 
 * 
 *	mg - in, take
 *	    merge object
 *		
 * Return value : 
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_merge_done(xs_merge_t* mg)
{
        su_pa_done(mg->mg_readstream_pa);
        mgtl_done(mg->mg_tuplelist);
        SsMemFree(mg);
}



#if 0 /* def SS_DEBUG */
static char* status_print(xs_streamstatus_t stat) {
        switch (stat) {
            case SSTA_EOR:
                return("SSTA_EOR");
            case SSTA_EOS:
                return("SSTA_EOS");
            case SSTA_RUN:
                return("SSTA_RUN");
            case SSTA_HOLD:
                return("SSTA_HOLD");
            default:
                ss_rc_error(stat);
        }
        return(NULL);
}
#endif

/*#***********************************************************************\
 * 
 *		merge_select_writestream
 * 
 * 
 * 
 * Parameters : 
 * 
 *	mg - 
 *		
 *		
 * Return value : 
 * 
 *     MGS_OK
 *     MGS_NOP, if there were no empty read streams. Happens in the
 *              first call.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static mergestatus_t merge_select_writestream(
        xs_merge_t* mg
) {
        xs_stream_t* old_writestream = NULL;
        xs_stream_t* readstream;
        int i;

#if 0 /* Looks like this is a false test */
        int count = 0;

        su_pa_do_get(mg->mg_readstream_pa, i, readstream) {

            if (xs_stream_getstatus(readstream) == SSTA_EOS) {
                count++;
            }
        }
        if (count == 0
        ||  count == 1
        ||  count == su_pa_nelems(mg->mg_readstream_pa))
        {

                /* OK */
        } else {


            su_pa_do_get(mg->mg_readstream_pa, i, readstream) {

                printf("Stream %d, status='%s', n_runs=%d\n",
                        i,
                        status_print(xs_stream_getstatus(readstream)),
                        xs_stream_nruns(readstream)
                      );

            }
            ss_error;
        }   

#endif

        su_pa_do_get(mg->mg_readstream_pa, i, readstream) {

            if (xs_stream_getstatus(readstream) == SSTA_EOS) {
                xs_streamstatus_t streamstat;

                /* Swap the old writestream with the readstream that
                 * has met the EOS
                 */
                old_writestream = mg->mg_writestream;
                streamstat = xs_stream_rewind(old_writestream);
                if (streamstat == SSTA_ERROR) {
                    return (MGS_ERROR);
                }

                mg->mg_writestream = su_pa_remove(mg->mg_readstream_pa, i);
                su_pa_insertat(mg->mg_readstream_pa, i, old_writestream);

                xs_stream_rewrite(mg->mg_writestream);
                break;
            }
        }
        if (old_writestream == NULL) {
            return(MGS_NOP);
        } else {
            return(MGS_OK);
        }
}

/*#***********************************************************************\
 * 
 *		merge_nextrun
 * 
 * Skip EOR marker in all of the read streams
 * 
 * Parameters : 
 * 
 *	mg - 
 *		
 *		
 * Return value : 
 * 
 *      MGS_EOM, if all the read streams went into EOS -> MERGE FINISHED
 *      MGS_EOS, if at least one but not all went into EOS
 *      MGS_OK,  if all the streams have run available
 *      MGS_ERROR, if failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static int merge_nextrun(
        xs_merge_t* mg
) {
        xs_stream_t* readstream;
        int i;
        int n = 0;
        int n_eos = 0;
        int n_eor = 0;
        int n_hold = 0;
        int n_run = 0;

        xs_streamstatus_t stat;

        su_pa_do_get(mg->mg_readstream_pa, i, readstream) {

            stat = xs_stream_getstatus(readstream); 
            ss_rc_dassert(stat == SSTA_EOR ||
                         stat == SSTA_EOS ||
                         stat == SSTA_HOLD ||
                         stat == SSTA_ERROR,
                         stat
                        );
            if (stat != SSTA_ERROR) {
                xs_stream_skipeor(readstream);
                stat = xs_stream_getstatus(readstream);
            }

            switch (stat) {

                case SSTA_EOR:
                    ss_dprintf_1(("merge_nextrun, stream %d to SSTA_EOR\n", i));
                    n_eor++;
                    break;
                case SSTA_EOS:
                    ss_dprintf_1(("merge_nextrun, stream %d to SSTA_EOS\n", i));
                    n_eos++;
                    break;
                case SSTA_RUN:
                    ss_dprintf_1(("merge_nextrun, stream %d to SSTA_RUN\n", i));
                    n_run++;
                    break;
                case SSTA_HOLD:
                    ss_dprintf_1(("merge_nextrun, stream %d to SSTA_HOLD\n", i));
                    n_hold++;
                    break;
                case SSTA_ERROR:
                    ss_dprintf_1(("merge_nextrun, stream %d to SSTA_ERROR\n", i));
                    return (MGS_ERROR);
                default:
                    ss_rc_error(stat);
                    break;  /* NOTREACHED */
            }
            n++;
        }
        ss_dprintf_1(("n=%d, n_eor=%d, n_eos=%d, n_run=%d, n_hold=%d\n",
                       n, n_eor, n_eos, n_run, n_hold
                    ));

        if (n_eos == n) {
            mg->mg_status = MGS_EOM;
            return(MGS_EOM);
        } else if (n_eos > 0) {
            mg->mg_status = MGS_EOS;
            return(MGS_EOS);
        } else {
            ss_dassert(mg->mg_status == MGS_EOR);
            return(MGS_OK);
        }
}

static int mgi_cmp(void* key, void* mgi, void* ctx)
{
        int cmp;

        cmp = (((xs_merge_t*)ctx)->mg_comp_fp)(
                    &key,
                    &(*(mgitem_t**)mgi)->mgi_data,
                    ((xs_merge_t*)ctx)->mg_comp_context);
        return (cmp);
}

static mgtuplelist_t* mgtl_init(uint nstreams)
{
        mgtuplelist_t* tuplelist;

        tuplelist = SSMEM_NEW(mgtuplelist_t);
        tuplelist->mgtl_array = SsMemAlloc(sizeof(mgitem_t*) * nstreams);
        tuplelist->mgtl_1st = 
            tuplelist->mgtl_size =
                nstreams;
        return (tuplelist);
}

static void mgtl_done(mgtuplelist_t* tuplelist)
{
        SsMemFree(tuplelist->mgtl_array);
        SsMemFree(tuplelist);
}

/*#***********************************************************************\
 * 
 *		mgtl_initialize
 * 
 * Initializes the merge tuplelist in mg by taking the first
 * tuple from each output stream and adding them into mgtl.
 * 
 * Parameters : 
 * 
 *	mg - 
 *		
 * Return value :
 *      MGS_OK
 *      MGS_ERROR
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static int mgtl_initialize(
        xs_merge_t* mg
) {
        int rc;
        int i;
        xs_stream_t* readstream;

        ss_dprintf_1(("mgtl_initialize\n"));
        ss_dassert(mg->mg_tuplelist->mgtl_1st == mg->mg_tuplelist->mgtl_size);

        su_pa_do_get(mg->mg_readstream_pa, i, readstream) {

            rc = mgtl_addfromstream(mg, readstream, i);
            if (rc == MGS_ERROR) {
                return (rc);
            }
        }
        return (MGS_OK);
}


/*#***********************************************************************\
 * 
 *		mgtl_addfromstream
 * 
 * Takes the first tuple (if available) from stream and adds it into the 
 * tuplelist of mg
 * 
 * Parameters : 
 * 
 *	mg - 
 *		
 *		
 *	readstream - 
 *		
 *	stream_index -	
 *		
 * Return value : 
 * 
 *      MGS_OK
 *      MGS_EOR
 *      MGS_EOS
 *      MGS_ERROR
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static int mgtl_addfromstream(
        xs_merge_t* mg,
        xs_stream_t* readstream,
        int stream_index
) {
        void* data;
        size_t datalen;
        xs_streamstatus_t stat;

        stat = xs_stream_getnext(readstream, &data, &datalen);
        switch (stat) {

            case SSTA_EOS:
                ss_dprintf_2(("mgtl_addfromstream %d, SSTA_EOS.\n", stream_index));
                return(MGS_EOS);

            case SSTA_EOR:
                ss_dprintf_2(("mgtl_addfromstream %d, SSTA_EOR.\n", stream_index));
                return(MGS_EOR);

            case SSTA_HOLD:
                ss_dprintf_2(("mgtl_addfromstream %d, SSTA_HOLD.\n", stream_index));
                return(MGS_EOR);

            case SSTA_RUN:
                ss_dprintf_2(("mgtl_addfromstream %d, SSTA_RUN.\n", stream_index));
                mgtl_add(mg, data, datalen, stream_index);
                return(MGS_OK);

            case SSTA_ERROR:
                ss_dprintf_2(("mgtl_addfromstream %d, SSTA_ERROR.\n", stream_index));
                return (MGS_ERROR);
            default:
                ss_rc_error(stat);
                return (MGS_ERROR); /* NOTREACHED */
        }
        ss_derror;
        return(MGS_ERROR);    /* NOTREACHED */
}

/*#**********************************************************************\
 * 
 *		mgtl_add
 * 
 * Adds a new item into merge tuplelist.
 * The stream index is stored because it is the information returned
 * when the first item is selected by mgtl_select.
 * 
 */
static void mgtl_add(
            xs_merge_t* mg,
            void* data,
            size_t datalen,
            int stream_index
) {

        mgitem_t* p_newmgitem;
        mgitem_t** pp_mgitem;
        size_t pos;

        ss_dassert(data != NULL);
        ss_dassert(datalen != 0);
        ss_dassert(stream_index >= 0);

        p_newmgitem = SSMEM_NEW(mgitem_t);
        p_newmgitem->mgi_data = data;
        p_newmgitem->mgi_datalen = datalen;
        p_newmgitem->mgi_stream_index = stream_index;

        ss_dassert(mg->mg_tuplelist->mgtl_1st > 0);
        if (mg->mg_tuplelist->mgtl_1st >= mg->mg_tuplelist->mgtl_size) {
            ss_dassert(mg->mg_tuplelist->mgtl_1st == mg->mg_tuplelist->mgtl_size);
            mg->mg_tuplelist->mgtl_1st--;
            mg->mg_tuplelist->mgtl_array[mg->mg_tuplelist->mgtl_1st] =
                p_newmgitem;
        } else {
            (void)su_bsearch_ctx(
                    data,
                    mg->mg_tuplelist->mgtl_array + mg->mg_tuplelist->mgtl_1st,
                    mg->mg_tuplelist->mgtl_size - mg->mg_tuplelist->mgtl_1st,
                    sizeof(mgitem_t*),
                    (su_bsearch_ctxcmpfuncptr_t)mgi_cmp,
                    mg,
                    (void**)&pp_mgitem);
            pos = pp_mgitem - mg->mg_tuplelist->mgtl_array;
            if (pos > mg->mg_tuplelist->mgtl_1st) {
                memmove(
                    mg->mg_tuplelist->mgtl_array + mg->mg_tuplelist->mgtl_1st - 1,
                    mg->mg_tuplelist->mgtl_array + mg->mg_tuplelist->mgtl_1st,
                    sizeof(mgitem_t*) * (pos - mg->mg_tuplelist->mgtl_1st));
            }
            mg->mg_tuplelist->mgtl_1st--;
            pp_mgitem--;
            *pp_mgitem = p_newmgitem;
        }
}


/*#***********************************************************************\
 * 
 *		mgtl_select
 * 
 *  Returns the stream index of first tuple in mergelist.
 *  Removes the corresponding tuple from list and frees the memory. 
 * 
 * Parameters : 
 * 
 *	mg - 
 *		
 *		
 *	p_data - 
 *		
 *		
 *	p_datalen - 
 *		
 *		
 * Return value : 
 * 
 *      >= 0, Index of the stream, *p_data and *p_datalen updated
 *      -1, list empty
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static int  mgtl_select(
        xs_merge_t* mg,
        void** p_data,
        size_t* p_datalen
) {
        mgitem_t* p_mgitem;
        int       retval = -1;

        ss_dprintf_3(("mgtl_select:mgtl_1st=%d < mgtl_size=%d\n",
            mg->mg_tuplelist->mgtl_1st, mg->mg_tuplelist->mgtl_size));

        if (mg->mg_tuplelist->mgtl_1st < mg->mg_tuplelist->mgtl_size) {
            p_mgitem = mg->mg_tuplelist->mgtl_array[
                            mg->mg_tuplelist->mgtl_1st];
            mg->mg_tuplelist->mgtl_1st++;
            retval = p_mgitem->mgi_stream_index;
            *p_data = p_mgitem->mgi_data;
            *p_datalen = p_mgitem->mgi_datalen;
            SsMemFree(p_mgitem);
        }
        return(retval);
}

/*##**********************************************************************\
 * 
 *		xs_merge_step
 * 
 * Polyphase mergesort for presorted blocks
 * 
 * Parameters : 
 *
 *      mg - use
 *          merge object
 *
 *	p_resultstream - out, give
 *		Result stream when return code was XS_RC_SUCC
 *
 *      p_errh - out, give
 *          in case of an error a pointer to a newly allocated
 *          error handle will be given if p_errh != NULL
 *
 * Return value : 
 *
 *      XS_RC_CONT when merge is still incomplete
 *      XS_RC_SUCC when merge was completed
 *      XS_RC_ERROR when merge failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_ret_t xs_merge_step(
        xs_merge_t* mg,
        xs_stream_t** p_resultstream,
        rs_err_t** p_errh)
{
        xs_ret_t xs_rc;
        int rc;
        int i;
        void* data;
        size_t datalen;
        xs_stream_t* readstream;
        ulong n_bytes_merged = 0L;
        uint n_rows_merged = 0L;
        bool succp;
        mergestatus_t mstatus;

        ss_dprintf_1(("xs_merge_step\n"));

        switch (mg->mg_status) {
            case MGS_INIT:
            case MGS_EOS:
                ss_dprintf_2(("xs_merge_step:%s\n", mg->mg_status == MGS_INIT ? "MGS_INIT" : "MGS_EOS"));
                mstatus = merge_select_writestream(mg);
                if (mstatus == MGS_ERROR) {
                    mg->mg_status = MGS_ERROR;
                    goto error_case;
                }
                /* FALLTHROUGH */
            case MGS_EOR:
                ss_dprintf_2(("xs_merge_step:MGS_EOR\n"));
                mgtl_initialize(mg);
                mg->mg_status = MGS_RUN;
                break;
            case MGS_RUN:
                ss_dprintf_2(("xs_merge_step:MGS_RUN\n"));
                break;
            default:
                ss_rc_derror(mg->mg_status);
            error_case:;
                rs_error_create(p_errh, XS_ERR_SORTFAILED);
                return (XS_RC_ERROR);
        }
        ss_rc_dassert(mg->mg_status == MGS_RUN, mg->mg_status);
        for (;;) {
            i = mgtl_select(mg, &data, &datalen);
            if (i < 0) {
                rc = MGS_EOR;
                break;
            }
            ss_dassert(vtpl_vacount(data) > 0);
            readstream = su_pa_getdata(mg->mg_readstream_pa, i);
            succp = xs_stream_append(mg->mg_writestream, data, datalen, p_errh);
            if (!succp) {
                return (XS_RC_ERROR);
            }
            n_bytes_merged += datalen;
            n_rows_merged++;
            rc = mgtl_addfromstream(mg, readstream, i);
            if (rc == MGS_ERROR) {
                if (p_errh != NULL) {
                    rs_error_create(p_errh, XS_ERR_SORTFAILED);
                }
                return (XS_RC_ERROR);
            }
            if (mg->mg_stepsizebytes <= n_bytes_merged
            ||  mg->mg_stepsizerows <= n_rows_merged)
            {
                rc = MGS_RUN;
                break;
            }
        }
        if (rc == MGS_EOR) {
            ss_dprintf_1(("Adding eor into target.\n"));
            succp = xs_stream_seteoratend(mg->mg_writestream, p_errh);
            if (!succp) {
                return (XS_RC_ERROR);
            }
            mg->mg_status = MGS_EOR;
            rc = merge_nextrun(mg);
        }
        /* **** */
        switch (rc) {
            case MGS_EOM:       /* merge sort completed */
                ss_dprintf_2(("xs_merge_step:MGS_EOM\n"));
                xs_rc = XS_RC_SUCC; 
                break;
            case MGS_OK:        /* next run available */
            case MGS_EOS:       /* end of stream detected */
            case MGS_RUN:       /* in the middle of a run */
                ss_dprintf_2(("xs_merge_step:%s\n",
                    rc == MGS_OK
                        ? "MGS_OK"
                        : (rc == MGS_EOS
                            ? "MGS_EOS"
                            : "MGS_RUN"
                           )
                ));
                xs_rc = XS_RC_CONT;
                break;
            case MGS_EOR:       /* never happens! */
                ss_derror;
            case MGS_ERROR:
                ss_dprintf_2(("xs_merge_step:MGS_ERROR\n"));
                ss_derror;
                if (p_errh != NULL) {
                    rs_error_create(p_errh, XS_ERR_SORTFAILED);
                }
                xs_rc = XS_RC_ERROR;
                break;
            default:
                ss_dprintf_2(("xs_merge_step:%d\n", rc));
                ss_rc_derror(rc);
                if (p_errh != NULL) {
                    rs_error_create(p_errh, XS_ERR_SORTFAILED);
                }
                xs_rc = XS_RC_ERROR;
                break;
        }
        if (xs_rc == XS_RC_SUCC) {
            xs_streamstatus_t streamstat;

            streamstat = xs_stream_rewind(mg->mg_writestream);
            if (streamstat == SSTA_ERROR) {
                rs_error_create(p_errh, XS_ERR_SORTFAILED);
                return (XS_RC_ERROR);
            }
            *p_resultstream = mg->mg_writestream;
            xs_stream_link(*p_resultstream);
        }
        return (xs_rc);
}

