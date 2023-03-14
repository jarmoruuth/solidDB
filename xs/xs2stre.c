/*************************************************************************\
**  source       * xs2stre.c
**  directory    * xs
**  description  * Stream utility for sort
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

#include <ssstdio.h>
#include <ssstdlib.h>

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssfile.h>

#include <uti0va.h>
#include <uti0vcmp.h>

#include <su0parr.h>
#include <su0error.h>

#include "xs0acnd.h"
#include "xs2stre.h"

#define CHKVAL_STREAMARR 7896
#define CHKVAL_STREAM    9784

#define CHK_STREAMARR(sa) ss_dassert(SS_CHKPTR(sa) && sa->sa_chk == CHKVAL_STREAMARR)
#define CHK_STREAM(st)    ss_dassert(SS_CHKPTR(st) && st->st_chk == CHKVAL_STREAM)

typedef enum {
        SCSTA_NOCUR,
        SCSTA_INIT,
        SCSTA_BEGIN,
        SCSTA_FWD,
        SCSTA_BKW,
        SCSTA_END
} xs_cursorstatus_t;

struct sortstreamarr_st {
        int             sa_chk;
        int             sa_maxfiles;
        int*            sa_maxruns;
        int*            sa_actruns;
        xs_stream_t**   sa_streams;
        int             sa_totruns;
};

struct sortstream_st {
        int                 st_chk;
        int                 st_nemptyruns;
        int                 st_nruns;
        xs_streamstatus_t   st_status;
        xs_cursorstatus_t   st_cursstat;
        xs_tf_t*            st_tf;
        int                 st_nlinks;
};

/*##**********************************************************************\
 * 
 *		xs_stream_init
 * 
 * Creates a new stream object
 * 
 * Parameters : 
 * 
 *	tfmgr - in, hold
 *		pointer temporary file manager object
 *		
 * Return value - give :
 *      stream object or NULL when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_stream_t* xs_stream_init(xs_tfmgr_t* tfmgr)
{
        xs_stream_t* stream;

        stream = SSMEM_NEW(xs_stream_t);

        stream->st_chk = CHKVAL_STREAM;
        stream->st_nemptyruns = 0;
        stream->st_nruns = 0;
        stream->st_status = SSTA_RUN;
        stream->st_cursstat = SCSTA_NOCUR;
        stream->st_nlinks = 1;
        stream->st_tf = xs_tfmgr_tfinit(tfmgr);
        if (stream->st_tf == NULL) {
            xs_stream_done(stream);
            stream = NULL;
        }
        return (stream);
}

/*##**********************************************************************\
 * 
 *		xs_stream_done
 * 
 * Frees the resources allocated for the stream instance.
 * The object is not physically freed before the link count has decreased 
 * to 0.
 * 
 * Parameters : 
 * 
 *	stream - use
 *	    Stream object	
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_stream_done(xs_stream_t* stream)
{
        CHK_STREAM(stream);

        stream->st_nlinks--;
        ss_dassert(stream->st_nlinks >= 0);
        if (stream->st_nlinks == 0) {
            if (stream->st_tf != NULL) {
                xs_tf_done(stream->st_tf);
            }
            SsMemFree(stream);
        }
}

/*##**********************************************************************\
 * 
 *		xs_stream_link
 * 
 * Increments link usage count for a stream.
 * 
 * Parameters : 
 * 
 *	stream - use
 *		Stream object.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_stream_link(xs_stream_t* stream)
{
        CHK_STREAM(stream);

        stream->st_nlinks++;
}

/*##**********************************************************************\
 * 
 *		xs_stream_append
 * 
 * Appends data in the end of the stream.
 * 
 * Parameters : 
 * 
 *	stream - use
 *	    Stream to write into	
 *		
 *	data - use
 *		Pointer to the data
 *		
 *	sz - use
 *		Size of the data.
 *
 *      p_errh - out, give
 *          in case of error a pointer to a newly allocated error handle
 *          will be returned if p_errh != NULL
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
bool xs_stream_append(
        xs_stream_t* stream,
        void* data,
        size_t sz,
        rs_err_t** p_errh)
{
        bool succp;

        CHK_STREAM(stream);

        succp = xs_tf_append(stream->st_tf, data, sz, p_errh);

        return(succp);
}

/*#***********************************************************************\
 * 
 *		stream_readstatus
 * 
 * 
 * 
 * Parameters : 
 * 
 *	stream - 
 *		
 *		
 *	p_ch - 
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
static xs_streamstatus_t stream_readstatus(
        xs_stream_t* stream)
{
        char* pch;

        CHK_STREAM(stream);


        pch = xs_tf_peek(stream->st_tf, 1);
        if (pch == NULL) {
            if (!xs_tf_moveposrel(stream->st_tf, 1)) {
                return (SSTA_EOS);
            }
            return (SSTA_ERROR);
        }
        if (*pch == '\0') {
            return(SSTA_EOR);
        }
        return(SSTA_RUN);
}


/*#***********************************************************************\
 * 
 *		stream_readandsetstatus
 * 
 * Reads and possibly set the stream status. When
 * st_cursstat != SCSTA_NOCUR, it skips all EOR marks
 * and possibly changes st_cursstat.
 * This method can only be used when scanning to
 * forward direction.
 * 
 * Parameters : 
 * 
 *	stream - use
 *		
 *		
 * Return value : 
 *      stream status
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static xs_streamstatus_t stream_readandsetstatus(
        xs_stream_t* stream)
{
        stream->st_status = stream_readstatus(stream);
        if (stream->st_cursstat != SCSTA_NOCUR) {
            while (stream->st_status == SSTA_EOR
            ||     stream->st_status == SSTA_HOLD)
            {
                xs_stream_skipeor(stream);
            }
            switch (stream->st_status) {
                case SSTA_RUN:
                    stream->st_cursstat = SCSTA_FWD;
                    break;
                case SSTA_EOS:
                    stream->st_cursstat = SCSTA_END;
                    break;
                case SSTA_ERROR:
                    ss_derror;
                    break;
                default:
                    ss_rc_error(stream->st_status);
                    break;
            }
        } 
        return (stream->st_status);
}

/*##**********************************************************************\
 * 
 *		xs_stream_getnext
 * 
 * Returns a reference to the next tuple in the stream.
 * The data is guaranteed to remain in the given address until getnext()
 * is called for next time.
 * 
 * Parameters : 
 * 
 *	stream - use
 *		
 *		
 *	p_data - out, ref
 *	    A pointer to the data is stored into *p_data, if return value	
 *		is SSTA_RUN
 *		
 *	p_sz - out
 *		Size of data returned in *p_data
 *		
 * Return value : 
 *
 *       One of
 *
 *       SSTA_RUN
 *       SSTA_HOLD
 *       SSTA_EOS
 *       SSTA_EOR 
 *
 *       The return value is the same as if we would have called
 *       xs_stream_getstatus() before xs_stream_getnext()
 *
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamstatus_t xs_stream_getnext(
        xs_stream_t* stream,
        void** p_data,
        size_t* p_sz)
{
        char* p_ch;
        va_index_t len;
        va_index_t lenlen;
        va_index_t grosslen;

        bool succp = TRUE;

        ss_dprintf_1(("xs_stream_getnext\n"));
        CHK_STREAM(stream);

        if (stream->st_cursstat == SCSTA_BKW) {
            /* When scroll direction changes, we recurse
             * one step to skip the previosuly read value
             */
            size_t dummy_sz;
            void* dummy_p;

            ss_dprintf_2(("xs_stream_getnext:stream->st_cursstat == SCSTA_BKW\n"));

            stream->st_cursstat = SCSTA_FWD;
            if (xs_stream_getnext(stream, &dummy_p, &dummy_sz) != SSTA_RUN) {
                return (stream->st_status);
            }
        }
        stream_readandsetstatus(stream);
        if (stream->st_status == SSTA_RUN) {
            p_ch = xs_tf_peek(stream->st_tf, 1);
            if (p_ch == NULL) {
                succp = FALSE;
            }
            if (succp) {
                lenlen = VA_LENLEN((va_t*)p_ch);
                if (lenlen > 1) {
                    p_ch = xs_tf_peekextend(stream->st_tf, 1, lenlen);
                    if (p_ch == NULL) {
                        succp = FALSE;
                    }
                }
            }
            if (succp) {
                len = VA_NETLEN((va_t*)p_ch);
                grosslen = lenlen * 2 + len;
                ss_dprintf_2(("xs_stream_getnext:len=%d, grosslen=%d\n", len, grosslen));
                p_ch = xs_tf_peekextend(stream->st_tf, lenlen, grosslen);
                if (p_ch == NULL) {
                    succp = FALSE;
                }
                ss_dassert(va_invnetlen(p_ch + grosslen - 1) == len);
            }
            if (succp) {
                succp = xs_tf_moveposrel(stream->st_tf, grosslen);
            }

            if (succp) {
                *p_data = p_ch;
                *p_sz = grosslen;
            }
        }
        if (!succp) {
            ss_derror;
            stream->st_status = SSTA_ERROR;
        } else if (stream->st_status == SSTA_EOS
               &&  stream->st_cursstat != SCSTA_NOCUR)
        {
            stream->st_cursstat = SCSTA_END;
        }
        return (stream->st_status);
}

/*##**********************************************************************\
 * 
 *		xs_stream_getprev
 * 
 * Returns a reference to the previous tuple in the stream.
 * The data is guaranteed to remain in the given address until getnext()
 * is called for next time.
 * 
 * Parameters : 
 * 
 *	stream - use
 *		
 *		
 *	p_data - out, ref
 *	    A pointer to the data is stored into *p_data, if return value	
 *		is SSTA_RUN
 *		
 *	p_sz - out
 *		Size of data returned in *p_data
 *		
 * Return value : 
 *
 *       One of
 *
 *       SSTA_RUN
 *       SSTA_HOLD
 *       SSTA_EOS
 *       SSTA_EOR 
 *
 *       The return value is the same as if we would have called
 *       xs_stream_getstatus() before xs_stream_getnext()
 *
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamstatus_t xs_stream_getprev(
        xs_stream_t* stream,
        void** p_data,
        size_t* p_sz)
{
        va_index_t lenlen;
        va_index_t len;
        va_index_t grosslen;
        char* p_ch;
        bool succp = TRUE;

        CHK_STREAM(stream);

        ss_dassert(stream->st_cursstat != SCSTA_NOCUR);

        if (stream->st_cursstat == SCSTA_FWD) {
            /* When scroll direction changes, we recurse
             * one step to skip the previosuly read value
             */
            void* dummy_p;
            size_t dummy_sz;

            stream->st_cursstat = SCSTA_BKW;
            if (xs_stream_getprev(stream, &dummy_p, &dummy_sz) != SSTA_RUN) {
                return (stream->st_status);
            }
        }
        do {
            if (!xs_tf_moveposrel(stream->st_tf, -1)) {
                stream->st_cursstat = SCSTA_BEGIN;
                stream->st_status = SSTA_BOS;
                return (stream->st_status);
            }
            p_ch = xs_tf_peek(stream->st_tf, 1);
            if (p_ch == NULL) {
                ss_derror;
                succp = FALSE;
            }
        } while (succp && *p_ch == '\0');
        if (succp) {
            lenlen = VA_LENLEN((va_t*)p_ch);
            if (lenlen > 1) {
                succp = xs_tf_moveposrel(stream->st_tf, 1L - (long)lenlen);
                if (succp) {
                    p_ch = xs_tf_peek(stream->st_tf, lenlen);
                    if (p_ch == NULL) {
                        succp = FALSE;
                    }
                }
            }
        }
        if (succp) {
            len = va_invnetlen(p_ch + lenlen - 1);
            grosslen = len + lenlen;
            succp = xs_tf_moveposrel(
                        stream->st_tf,
                        -(long)grosslen);
            if (succp) {
                grosslen += lenlen;
                p_ch = xs_tf_peek(stream->st_tf, (size_t)grosslen);
                if (p_ch == NULL) {
                    succp = FALSE;
                }
            }
        }
        if (succp) {
            *p_data = p_ch;
            *p_sz = grosslen;
            stream->st_status = SSTA_RUN;
            stream->st_cursstat = SCSTA_BKW;
        } else {
            ss_derror;
            stream->st_status = SSTA_ERROR;
        }

        return(stream->st_status);
}

/*##**********************************************************************\
 * 
 *		xs_stream_rewrite
 * 
 * Deletes the whole data contents of the stream.
 * 
 * Parameters : 
 * 
 *	stream - use
 *		
 *		
 * Return value :
 *      stream status
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamstatus_t xs_stream_rewrite(xs_stream_t* stream)
{
        bool succp;

        CHK_STREAM(stream);

        stream->st_status = SSTA_RUN;
        stream->st_nruns = 0;

        succp = xs_tf_rewrite(stream->st_tf);
        if (!succp) {
            ss_derror;
            stream->st_status = SSTA_ERROR;
        }
        return (stream->st_status);
}

/*##**********************************************************************\
 * 
 *		xs_stream_rewind
 * 
 * Set read pointer to the beginning 
 * 
 * Parameters : 
 * 
 *	stream - use
 *		
 *		
 * Return value :
 *      stream status
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamstatus_t xs_stream_rewind(xs_stream_t* stream)
{
        bool succp;

        ss_dprintf_1(("xs_stream_rewind\n"));
        CHK_STREAM(stream);

        succp = xs_tf_rewind(stream->st_tf);
        if (succp) {
            stream_readandsetstatus(stream);
            if (stream->st_status == SSTA_RUN
            &&  stream->st_cursstat != SCSTA_NOCUR)
            {
                stream->st_cursstat = SCSTA_BEGIN;
            }
        }
        if (!succp) {
            stream->st_status = SSTA_ERROR;
        }
        return (stream->st_status);
}


/*##**********************************************************************\
 * 
 *		xs_stream_initfetch
 * 
 * 
 * 
 * Parameters : 
 * 
 *	stream - use
 *		
 * Return value : 
 *      stream status:
 *      SSTA_RUN when OK
 *      SSTA_EOS when empty stream
 *      SSTA_ERROR when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamstatus_t xs_stream_initfetch(
        xs_stream_t* stream)
{
        bool succp;
        xs_streamstatus_t ssta;

        CHK_STREAM(stream);
        
        ss_dassert(stream->st_cursstat == SCSTA_NOCUR);
        stream->st_cursstat = SCSTA_INIT;

        succp = xs_tf_opencursor(stream->st_tf);
        if (succp) {
            stream_readandsetstatus(stream);
            if (stream->st_status == SSTA_RUN
            &&  stream->st_cursstat != SCSTA_NOCUR)
            {
                stream->st_cursstat = SCSTA_BEGIN;
            }
        }
        if (!succp) {
            ss_derror;
            stream->st_status = SSTA_ERROR;
        }
        ssta = stream->st_status;
        ss_debug(
            if (ssta != SSTA_ERROR) {
                ss_rc_dassert(stream->st_cursstat == SCSTA_BEGIN
                    || stream->st_cursstat == SCSTA_END,
                    stream->st_cursstat);
            }
        );
        return (ssta);
}


/*##**********************************************************************\
 * 
 *		xs_stream_cursortobegin
 * 
 * Meves the cursor to beginning of the result stream
 * 
 * Parameters : 
 * 
 *	stream - in out, use
 *		stream object in cursor state
 *		
 * Return value :
 *      stream status (usually SSTA_RUN)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamstatus_t xs_stream_cursortobegin(xs_stream_t* stream)
{
        bool succp;

        CHK_STREAM(stream);
        ss_dassert(stream->st_cursstat != SCSTA_NOCUR);
        succp = xs_tf_movetobegin(stream->st_tf);
        if (succp) {
            stream_readandsetstatus(stream);
            if (stream->st_status == SSTA_RUN)
            {
                stream->st_cursstat = SCSTA_BEGIN;
            }
        }
        if (!succp) {
            ss_derror;
            stream->st_status = SSTA_ERROR;
        }
        return (stream->st_status);
}

/*##**********************************************************************\
 * 
 *		xs_stream_cursortoend
 * 
 * Moves the cursor to end of result stream
 * 
 * Parameters : 
 * 
 *	stream - in out, use
 *		stream in cursor state
 *		
 * Return value :
 *      stream status (usually SSTA_EOS)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamstatus_t xs_stream_cursortoend(xs_stream_t* stream)
{
        bool succp;

        CHK_STREAM(stream);
        ss_dassert(stream->st_cursstat != SCSTA_NOCUR);
        succp = xs_tf_movetoend(stream->st_tf);
        if (succp) {
            stream->st_status = SSTA_EOS;
            stream->st_cursstat = SCSTA_END;
        }
        if (!succp) {
            ss_derror;
            stream->st_status = SSTA_ERROR;
        }
        return (stream->st_status);
}

/*#***********************************************************************\
 * 
 *		stream_addemptyruns
 * 
 * 
 * 
 * Parameters : 
 * 
 *	stream - 
 *		
 *		
 *	nemptyruns - 
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
static void stream_addemptyruns(
        xs_stream_t* stream,
        int nemptyruns)
{
        CHK_STREAM(stream);

        stream->st_nemptyruns = nemptyruns;
        if (nemptyruns > 0) {
            stream->st_status = SSTA_HOLD;
        } else {
            stream_readandsetstatus(stream);
        }
}


/*##**********************************************************************\
 * 
 *		xs_stream_getstatus
 * 
 * Returns the status of the stream
 * 
 * Parameters : 
 * 
 *	stream - use
 *		
 *		
 * Return value : 
 * 
 *       SSTA_RUN,  if stream is in the middle of an actual run
 *       SSTA_EOR,  if stream is in the end of a run 
 *                  The status can be changed by xs_stream_skipeor()
 *       SSTA_HOLD, if stream contains a dummy run. 
 *                  Because a dummy run also contains an EOR marker, the
 *                  status can be changed by xs_stream_skipeor()
 *       SSTA_EOS   if stream is empty.
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamstatus_t xs_stream_getstatus(xs_stream_t* stream)
{
        CHK_STREAM(stream);

        return(stream->st_status);
}

/*##**********************************************************************\
 * 
 *		xs_stream_nruns
 * 
 * Returns the number of unread runs (actual + dummy) in the stream
 * 
 * Parameters : 
 * 
 *	stream - use
 *	    stream	
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
long xs_stream_nruns(xs_stream_t* stream)
{
        CHK_STREAM(stream);

        return(stream->st_nemptyruns + stream->st_nruns);
}

/*##**********************************************************************\
 * 
 *		xs_stream_seteoratend
 * 
 * Sets an End-of-run marker at the end of the stream.
 * 
 * Parameters : 
 * 
 *	stream - in out, use
 *          stream object
 *		
 *      p_errh - out, give
 *          in case of error a pointer to a newly allocated error handle
 *          will be returned if p_errh != NULL
 *		
 * Return value :
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_stream_seteoratend(
        xs_stream_t* stream,
        rs_err_t** p_errh)
{
        bool succp;
        static char c = '\0';

        CHK_STREAM(stream);
        succp = xs_tf_append(stream->st_tf, &c, 1, p_errh);
        stream->st_nruns++;
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_stream_skipeor
 * 
 * Skips over an EOR marker in the stream.
 * 
 * Parameters : 
 * 
 *	stream - 
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
xs_streamstatus_t xs_stream_skipeor(xs_stream_t* stream)
{
        bool succp = TRUE;

        CHK_STREAM(stream);

        if (stream->st_status == SSTA_EOS) {
            return(SSTA_EOS);
        }
        if (stream->st_nemptyruns > 0) {
            ss_dassert(stream->st_status == SSTA_HOLD
                || stream->st_status == SSTA_EOR);
            stream->st_nemptyruns--;
            if (stream->st_nemptyruns > 0) {
                stream->st_status = SSTA_HOLD;
                return(SSTA_HOLD);
            }
        } else {
            ss_dassert(stream->st_status == SSTA_EOR);
            stream->st_nruns--;

            succp = xs_tf_moveposrel(stream->st_tf, 1L);
            if (!succp) {
                ss_derror;
                stream->st_status = SSTA_ERROR;
            }
        }
        if (succp) {
            stream->st_status = stream_readstatus(stream);
        }
        return(stream->st_status);
}

/*##**********************************************************************\
 * 
 *		xs_streamarr_init
 * 
 * Creates an array of stream objects. This object is used to distribute
 * tuples to different streams.
 * 
 * Parameters : 
 * 
 *	maxstreams - in
 *		Max number of streams used for sorting.
 *
 *      tfmgr - in, hold
 *          temporary file manager
 *		
 * Return value - give : 
 * 
 *      Pointer to stream array object or
 *      NULL when not enough resources were available
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_streamarr_t* xs_streamarr_init(
        int maxstreams,
        xs_tfmgr_t* tfmgr)
{
        int i;
        xs_streamarr_t* sa;
        bool succp = TRUE;

        sa = SSMEM_NEW(xs_streamarr_t);

        sa->sa_chk = CHKVAL_STREAMARR;
        sa->sa_maxfiles = maxstreams;
        sa->sa_maxruns = SsMemAlloc(sizeof(int) * (maxstreams + 1));
        sa->sa_actruns = SsMemAlloc(sizeof(int) * (maxstreams + 1));
        sa->sa_streams = SsMemAlloc(sizeof(xs_stream_t*) * (maxstreams + 1));
        sa->sa_totruns = 0;

        for (i = 0; i <= sa->sa_maxfiles; i++) {
            sa->sa_maxruns[i] = sa->sa_actruns[i] = 0;
        }
        sa->sa_maxruns[0] = sa->sa_maxruns[maxstreams] = 1;

        sa->sa_streams[0] = NULL;   /* Index zero not used. */
        for (i = 1; i <= maxstreams; i++) {
            sa->sa_streams[i] = xs_stream_init(tfmgr);
            if (sa->sa_streams[i] == NULL) {
                succp = FALSE;
            }
        }
        if (!succp) {
            xs_streamarr_done(sa);
            sa = NULL;
        }
        return (sa);
}

/*##**********************************************************************\
 * 
 *		xs_streamarr_done
 * 
 * Releases stream array object. All streams are closed and removed.
 * 
 * Parameters : 
 * 
 *	sa - in, take
 *		Stream array object.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_streamarr_done(xs_streamarr_t* sa)
{
        int i;

        CHK_STREAMARR(sa);

        for (i = 1; i <= sa->sa_maxfiles; i++) {
            if (sa->sa_streams[i] != NULL) {
                xs_stream_done(sa->sa_streams[i]);
            }
        }
        SsMemFree(sa->sa_streams);
        SsMemFree(sa->sa_maxruns);
        SsMemFree(sa->sa_actruns);
        SsMemFree(sa);
}

/*##**********************************************************************\
 * 
 *		xs_streamarr_nextstream
 * 
 * Selects next stream for output. Used in the distribute phase of 
 * polyphase merge sort to select the next stream when one presorted
 * block becomes full.
 * 
 * Parameters : 
 * 
 *	sa - use
 *		Stream array object.
 *		
 * Return value - ref : 
 * 
 *      Pointer to a stream array object.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_stream_t* xs_streamarr_nextstream(xs_streamarr_t* sa)
{
        int i;
        int j;
        int inc;
        bool succp = TRUE;

        CHK_STREAMARR(sa);

        sa->sa_totruns++;
        sa->sa_actruns[0]++;
        for (i = 1; i <= sa->sa_maxfiles; i++) {
            succp = xs_tf_close(sa->sa_streams[i]->st_tf);
            if (!succp) {
                ss_derror;
                return (NULL);
            }
        }
        if (sa->sa_actruns[0] > sa->sa_maxruns[0]) {
            /* Find next perfect distribution. */
            inc = sa->sa_maxruns[sa->sa_maxfiles];
            sa->sa_maxruns[0] += (sa->sa_maxfiles - 2) * inc;
            for (i = sa->sa_maxfiles; i > 1; i--) {
                sa->sa_maxruns[i] = sa->sa_maxruns[i - 1] + inc;
            }
        }

        j = 2;
        /* Select file farthest from perfect. */
        for (i = 3; i <= sa->sa_maxfiles; i++) {
            if (sa->sa_maxruns[i] - sa->sa_actruns[i]  >
                sa->sa_maxruns[j] - sa->sa_actruns[j]) {
                j = i;
            }
        }
        succp = xs_tf_open(sa->sa_streams[j]->st_tf);
        if (!succp) {
            ss_derror;
            return (NULL);
        }
        sa->sa_actruns[j]++;

        return(sa->sa_streams[j]);
}

#ifdef SS_DEBUG
/*#***********************************************************************\
 * 
 *		streamarr_outputdistribution
 * 
 * 
 * 
 * Parameters : 
 * 
 *	sa - 
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
static void streamarr_outputdistribution(xs_streamarr_t* sa)
{
        int i;

        SsDbgPrintf("        ");
        for (i = 2; i <= sa->sa_maxfiles; i++) {
            SsDbgPrintf("%5d ", i);
        }
        SsDbgPrintf("\n");
        SsDbgPrintf("maxruns ");
        for (i = 2; i <= sa->sa_maxfiles; i++) {
            SsDbgPrintf("%5d ", sa->sa_maxruns[i]);
        }
        SsDbgPrintf("\n");
        SsDbgPrintf("actruns ");
        for (i = 2; i <= sa->sa_maxfiles; i++) {
            SsDbgPrintf("%5d ", sa->sa_actruns[i]);
        }
        SsDbgPrintf("\n");
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		xs_streamarr_endofdistribute
 * 
 * Marks end of distribution phase. 
 * 
 * Parameters : 
 * 
 *	sa - use
 *		Stream array object.
 *		
 *	p_writestream - out, ref
 *		A pointer to an empty stream object that can be used for
 *		writing is stored into *p_writestream.
 *		
 *	p_readstream_pa - out, give
 *		A pointer array of streams with data after distribute phase
 *		is stored into *p_readstream_pa. The stream objects in the
 *		pointer array are references to streams local to stream array
 *          object and should not be deleted by the caller.
 *		
 * Return value : 
 * 
 *      TRUE    There were only one run, the stream is returned
 *              in *p_writestream
 * 
 *      FALSE   There were more than one run, merge sort must be called
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_streamarr_endofdistribute(
        xs_streamarr_t* sa,
        xs_stream_t** p_writestream,
        su_pa_t** p_readstream_pa)
{
        int i;
        bool succp;

        CHK_STREAMARR(sa);
        ss_dprintf_1(("xs_streamarr_endofdistribute\n"));

        /* Check if there were only one run. */
        if (sa->sa_totruns == 1) {
            for (i = 2; i <= sa->sa_maxfiles; i++) {
                if (sa->sa_actruns[i] > 0) {
                    ss_dassert(sa->sa_actruns[i] == 1);
                    xs_stream_rewind(sa->sa_streams[i]);
                    *p_writestream = sa->sa_streams[i];
                    *p_readstream_pa = NULL;
                    return(TRUE);
                }
            }
            ss_error;
        }

        *p_writestream = sa->sa_streams[1];
        succp = xs_tf_open((*p_writestream)->st_tf);
        ss_assert(succp);
        *p_readstream_pa = su_pa_init();

        for (i = 2; i <= sa->sa_maxfiles; i++) {
            if (sa->sa_actruns[i] > 0) {
                if (sa->sa_actruns[i] < sa->sa_maxruns[i]) {
                    stream_addemptyruns(
                        sa->sa_streams[i],
                        sa->sa_maxruns[i] - sa->sa_actruns[i]);
                }
                su_pa_insert(*p_readstream_pa, sa->sa_streams[i]);
            }
        }

        ss_output_1(streamarr_outputdistribution(sa));

        return(FALSE);
}
