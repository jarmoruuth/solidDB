/*************************************************************************\
**  source       * su0bstre.c
**  directory    * su
**  description  * Binary stream object
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

#include <ssdebug.h>
#include <ssmem.h>

#include "su1check.h"
#include "su0bstre.h"

typedef enum {
        BSTR_READ,
        BSTR_WRITE,
        BSTR_READWRITE
} bstr_type_t;

struct substreamstruct {
        su_check_t              bstr_check;     /* SUCHK_BSTREAM */
        bstr_type_t             bstr_type;      /* Read/Write stream */
        su_bstream_iofp_t       bstr_readiofp;  /* Funp for read */
        su_bstream_iofp_t       bstr_writeiofp; /* Funp for write */
        su_bstream_ioendfp_t    bstr_writeendfp;
        su_bstream_reachfp_t    bstr_reachfp;   /* Funp for reach */
        su_bstream_releasefp_t  bstr_releasefp; /* Funp for release */
        su_bstream_closefp_t    bstr_closefp;   /* Called by bstream_done */
        su_bstream_abortfp_t    bstr_abortfp;   /* Called by bstream_abort */
        su_bstream_suerrfp_t    bstr_suerrfp;   /* givesuerr */
        void*                   bstr_param;     /* Parameter for funp */
        uint                    bstr_nlink;
};

#define BSTREAM_CHECK(bs) \
        ss_dassert(SS_CHKPTR(bs) && (bs)->bstr_check == SUCHK_BSTREAM)

static su_err_t* bstr_givesuerr(
        void*   param __attribute__ ((unused)))
{
        su_err_t* err = NULL;
        char* text = su_rc_givetext_noargs(SU_ERR_BSTREAM_BROKEN);
        su_err_init_text(&err, SU_ERR_BSTREAM_BROKEN, text);
        SsMemFree(text);
        return(err);
}

static su_bstream_t* bstream_init_kind(
        su_bstream_iofp_t       readiofp,
        su_bstream_iofp_t       writeiofp,
        su_bstream_reachfp_t    reachfp,
        su_bstream_releasefp_t  releasefp,
        su_bstream_closefp_t    closefp,
        su_bstream_abortfp_t    abortfp,
        su_bstream_suerrfp_t    suerrfp,
        void*                   param,
        bstr_type_t             kind
) {
        su_bstream_t* bstream;

        ss_dassert(kind == BSTR_READ || kind == BSTR_WRITE || kind == BSTR_READWRITE);
        bstream = SSMEM_NEW(su_bstream_t);
        bstream->bstr_check     = SUCHK_BSTREAM;
        bstream->bstr_type      = kind;
        bstream->bstr_readiofp  = readiofp;   
        bstream->bstr_writeiofp = writeiofp;   
        bstream->bstr_writeendfp = NULL;   
        bstream->bstr_reachfp   = reachfp;
        bstream->bstr_releasefp = releasefp;
        bstream->bstr_closefp   = closefp;
        if (abortfp == NULL) {
            bstream->bstr_abortfp   = (su_bstream_abortfp_t)closefp;
        } else {
            bstream->bstr_abortfp   = abortfp;
        }
        if (suerrfp == NULL) {
            bstream->bstr_suerrfp   = bstr_givesuerr;
        } else {
            bstream->bstr_suerrfp   = suerrfp;
        }
        bstream->bstr_param     = param;
        bstream->bstr_nlink     = 1;
        return(bstream);
}

su_err_t* su_bstream_givesuerr(
        su_bstream_t* bstream)
{
        BSTREAM_CHECK(bstream);
        return((*bstream->bstr_suerrfp)(bstream->bstr_param));
}

/*##**********************************************************************\
 * 
 *		su_bstream_initwrite
 * 
 * Creates a new write stream.
 * 
 * Parameters : 
 * 
 *	writefn - in, hold
 *		
 *		
 *	reachfn - in, hold
 *		
 *		
 *	releasefn - in, hold
 *		
 *		
 *	param - in, hold
 *		
 *		
 * Return value - give : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_bstream_t* su_bstream_initwrite(
        su_bstream_iofp_t       writefp,
        su_bstream_reachfp_t    reachfp,
        su_bstream_releasefp_t  releasefp,
        su_bstream_closefp_t    closefp,
        su_bstream_abortfp_t    abortfp,
        su_bstream_suerrfp_t    suerrfp,
        void*                   param
) {
        return(bstream_init_kind(NULL, writefp, reachfp, releasefp, closefp, abortfp, suerrfp, param, BSTR_WRITE));        
}

su_bstream_t* su_bstream_initread(
        su_bstream_iofp_t       readfp,
        su_bstream_reachfp_t    reachfp,
        su_bstream_releasefp_t  releasefp,
        su_bstream_closefp_t    closefp,
        su_bstream_abortfp_t    abortfp,
        su_bstream_suerrfp_t    suerrfp,
        void*                   param
) {
        return(bstream_init_kind(readfp, NULL, reachfp, releasefp, closefp, abortfp, suerrfp, param, BSTR_READ));
}

su_bstream_t* su_bstream_initreadwrite(
        su_bstream_iofp_t       readfp,
        su_bstream_iofp_t       writefp,
        su_bstream_ioendfp_t    writeendfp,
        su_bstream_closefp_t    closefp,
        su_bstream_abortfp_t    abortfp,
        su_bstream_suerrfp_t    suerrfp,
        void*                   param
) {
        su_bstream_t* bstream;

        bstream = bstream_init_kind(readfp, writefp, NULL, NULL, closefp, abortfp, suerrfp, param, BSTR_READWRITE);
        bstream->bstr_writeendfp = writeendfp;
        return(bstream);
}

su_bstream_t* su_bstream_link(
        su_bstream_t* bstream)
{
        BSTREAM_CHECK(bstream);
        bstream->bstr_nlink++;
        return(bstream);
}

void su_bstream_done(
        su_bstream_t* bstream
) {
        BSTREAM_CHECK(bstream);
        ss_dassert(bstream->bstr_nlink > 0);
        bstream->bstr_nlink--;
        if (bstream->bstr_nlink > 0) {
            return;
        }
        (*bstream->bstr_closefp)(bstream->bstr_param);
        SsMemFree(bstream);
}

void su_bstream_abort(
        su_bstream_t* bstream)
{
        BSTREAM_CHECK(bstream);
        (*bstream->bstr_abortfp)(bstream->bstr_param);
        /* SsMemFree(bstream); JarmoR removed Sep 8, 1999 */
}

/*##**********************************************************************\
 * 
 *		su_bstream_getparam
 * 
 * Gets a pointer to object that is bound to the bstream
 * 
 * Parameters : 
 * 
 *	bstream - in, use
 *		pointer to bstream object
 *		
 * Return value - ref :
 *      pointer to object that is bound to bstream
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void* su_bstream_getparam(
        su_bstream_t* bstream)
{
        BSTREAM_CHECK(bstream);
        return (bstream->bstr_param);
}

/*** Inbound (READ) stream ***/

su_ret_t su_bstream_read(
        su_bstream_t* bstream,
        char*         buf,
        size_t        bufsize,
        size_t*       p_read
) {
        size_t n_read;

        BSTREAM_CHECK(bstream);
        ss_dassert(buf != NULL);
        ss_dassert(p_read != NULL);
        ss_dassert(bstream->bstr_type == BSTR_READ || bstream->bstr_type == BSTR_READWRITE);
        n_read = (*bstream->bstr_readiofp)(bstream->bstr_param, buf, bufsize);
        if (n_read > 0) {
            *p_read = n_read;
            return(RC_SU_BSTREAM_SUCC);
        } else if (n_read == 0) {
            *p_read = n_read;
            return(ERR_SU_BSTREAM_EOS);
        } else {
            *p_read = 0;
            return(ERR_SU_BSTREAM_BROKEN);
        }
}

char* su_bstream_reachforread(
        su_bstream_t* bstream,
        size_t*       p_avail
) {
        BSTREAM_CHECK(bstream);
        ss_dassert(bstream->bstr_type == BSTR_READ);
        return((*bstream->bstr_reachfp)(bstream->bstr_param, p_avail));
}

void su_bstream_releaseread(
        su_bstream_t* bstream,
        size_t        n_read
) {
        BSTREAM_CHECK(bstream);
        ss_dassert(bstream->bstr_type == BSTR_READ);
        (*bstream->bstr_releasefp)(bstream->bstr_param, n_read);
}


/*** Outbound (WRITE) stream ***/

su_ret_t su_bstream_write(
        su_bstream_t* bstream,
        char*         buf,
        size_t        bufsize,
        size_t*       p_written
) {
        size_t n_written;

        BSTREAM_CHECK(bstream);
        ss_dassert(buf != NULL);
        ss_dassert(p_written != NULL);
        ss_dassert(bstream->bstr_type == BSTR_WRITE || bstream->bstr_type == BSTR_READWRITE);
        n_written = (*bstream->bstr_writeiofp)(bstream->bstr_param, buf, bufsize);
        if (n_written > 0) {
            *p_written = n_written;
            return(RC_SU_BSTREAM_SUCC);
        } else if (n_written == 0) {
            *p_written = n_written;
            return(ERR_SU_BSTREAM_EOS);
        } else {
            *p_written = 0;
            return(ERR_SU_BSTREAM_BROKEN);
        }
}

char* su_bstream_reachforwrite(
        su_bstream_t* bstream,
        size_t*       p_avail
) {
        BSTREAM_CHECK(bstream);
        ss_dassert(bstream->bstr_type == BSTR_WRITE);
        return((*bstream->bstr_reachfp)(bstream->bstr_param, p_avail));
}

void su_bstream_releasewrite(
        su_bstream_t* bstream,
        size_t        n_written
) {
        BSTREAM_CHECK(bstream);
        ss_dassert(bstream->bstr_type == BSTR_WRITE);
        (*bstream->bstr_releasefp)(bstream->bstr_param, n_written);
}

bool su_bstream_writeend(
        su_bstream_t* bstream
) {
        BSTREAM_CHECK(bstream);
        if (bstream->bstr_writeendfp != 0) {
            return((*bstream->bstr_writeendfp)(bstream->bstr_param));
        } else {
            return(TRUE);
        }
}


/*** two streams ***/

su_ret_t su_bstream_copy(
        su_bstream_t* trg_bstream,
        su_bstream_t* src_bstream,
        size_t*       p_ncopied
) {
        SS_NOTUSED(trg_bstream);
        SS_NOTUSED(src_bstream);
        SS_NOTUSED(p_ncopied);
        ss_error;
        return(RC_SU_BSTREAM_SUCC);
}
