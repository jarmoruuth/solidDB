/*************************************************************************\
**  source       * dbe0ld.c
**  directory    * dbe
**  description  * logdata object
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

None.

Error handling:
--------------

Asserts.


Objects used:
------------

None.


Preconditions:
-------------

None.

Multithread considerations:
--------------------------

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <stdio.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <su0list.h>

#include "dbe7logf.h"
#include "dbe7rfl.h"
#include "dbe9type.h"
#include "dbe0ld.h"
#include "dbe0catchup.h"
#include "dbe0logi.h"

#ifndef HSB_LPID
        uups
#endif

#ifdef IO_OPT
#define CHK_LOGDATA(p) \
        ss_dassert((SS_CHKPTR(p) && (p)->ld_check == DBE_CHK_LOGDATA) \
        && ((ld->beginabuf != ld->endabuf) || (ld->endpos >= ld->beginpos)));
#else
#define CHK_LOGDATA(p) \
        ss_dassert((SS_CHKPTR(p) && (p)->ld_check == DBE_CHK_LOGDATA) \
        && ((ld->beginbuf != ld->endbuf) || (ld->endpos >= ld->beginpos)));
#endif


/* ---------------------------------
 * logdata buffer enumeration states
 */
typedef enum {

        LD_BUFSTATE_BEGIN,
        LD_BUFSTATE_RAW_BEGIN,
        LD_BUFSTATE_RAW_NEXT,
        LD_BUFSTATE_RAW_END,
        LD_BUFSTATE_END

} dbe_logdata_bufstate_t;

/* Structure for primary propagator.
 * Not size in debug compilation in 108 bytes.
 * in product it is 3*4 less ==  (108-16 = 92)
 */
struct dbe_logdata_st {
        ss_debug(dbe_chk_t      ld_check;)       /* 4 check field */
        ss_debug(dbe_trxid_t    ld_trxid;)       /* 4 for debugging/trace */
        ss_debug(int            ld_nbuffers_chk;)/* 4 */

        dbe_hsbbuf_t*           hsb_beginbuf;    /* 4 */
        dbe_hsbbuf_t*           hsb_endbuf;      /* 4 */
#ifdef IO_OPT
        dbe_alogbuf_t*          beginabuf;        /* 4 */
#else
        dbe_logbuf_t*           beginbuf;        /* 4 */
#endif
        size_t                  beginpos;        /* 4 */
#ifdef IO_OPT
        dbe_alogbuf_t*   endabuf;          /* 4 */
#else
        dbe_logbuf_t*           endbuf;          /* 4 */
#endif
        size_t                  endpos;          /* 4 */
        su_list_t*              buffers;         /* 4 */
        long                    nbytes;          /* 4 */
        dbe_logdata_bufstate_t  ld_bufstate;     /* 4 */
        dbe_catchup_logpos_t    ld_logpos;       /* 3*4 */
        dbe_catchup_logpos_t*   ld_local_logpos; /* 4 */
        dbe_catchup_logpos_t    ld_remote_logpos;/* 3*4 */
        su_list_node_t*         listnode;        /* 4 */
        int                     ld_nlink;        /* 4 */
        dbe_logrectype_t        logrectype;      /* 4 */
};

/*##**********************************************************************\
 *
 *              dbe_logdata_init
 *
 * Creates logdata 'recording' object
 *
 * Parameters :
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
dbe_logdata_t* dbe_logdata_init(
        dbe_trxid_t trxid __attribute__ ((unused)),
        dbe_logrectype_t logrectype,
        bool  split_queue_force __attribute__ ((unused)),
        dbe_hsbbuf_t* hsbbuf,
        size_t bufpos,
        dbe_catchup_logpos_t logpos)
{
        dbe_logdata_t* ld;

        /* split_queue_force:this is not needed.
         * To reduce ld-size get rid of this (we may get <64byte size).
         */
        ld = SsMemAlloc(sizeof(dbe_logdata_t));

        ss_bprintf_3(("dbe_logdata_init:logrectype %.255s, trxid %ld, logpos(%d,%s,%d,%d,%d), ld=%x, size %d\n",
                       dbe_logi_getrectypename(logrectype),
                       DBE_TRXID_GETLONG(trxid), LOGPOS_DSDDD(logpos), ld, sizeof(dbe_logdata_t)));

        ss_debug(ld->ld_check = DBE_CHK_LOGDATA;)
        ld->logrectype = logrectype;
        ld->hsb_beginbuf = hsbbuf;
#ifdef IO_OPT
        ld->beginabuf = dbe_hsbbuf_get_alogbuf(hsbbuf);
        ss_dassert(DBE_LB_ALIGNMENT((ld->beginabuf->alb_buffer),
                   SS_DIRECTIO_ALIGNMENT));
#else
        ld->beginbuf = dbe_hsbbuf_get_logbuf(hsbbuf);
#endif
        ld->beginpos = bufpos;
        ld->hsb_endbuf = NULL;
#ifdef IO_OPT
        ld->endabuf = NULL;
#else
        ld->endbuf = NULL;
#endif
        ld->endpos = 0;
        ld->nbytes = 0;
        ss_debug(ld->ld_trxid = trxid;)
        ld->ld_logpos = logpos;
        ld->ld_nlink = 1;

        ld->buffers = NULL;
        ss_debug(ld->ld_nbuffers_chk = 1;)

        ld->ld_bufstate = LD_BUFSTATE_BEGIN;

        ld->listnode = NULL;

        ld->ld_local_logpos = NULL;
        DBE_CATCHUP_LOGPOS_SET_NULL(ld->ld_remote_logpos);

        dbe_hsbbuf_link(hsbbuf);

        CHK_LOGDATA(ld);
        return(ld);
}

/*##**********************************************************************\
 *
 *              dbe_logdata_link
 *
 * This should be avoided/replaced with non mutexing version
 *
 * Parameters :
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
void dbe_logdata_link(
        dbe_logdata_t* ld)
{
        SsSemEnter(ss_lib_sem);

        ld->ld_nlink++;

        SsSemExit(ss_lib_sem);

        ss_dprintf_3(("dbe_logdata_link:logrectype %.255s, trxid %ld, nlink %d, ld=%x\n",
                       dbe_logi_getrectypename(ld->logrectype),
                       DBE_TRXID_GETLONG(ld->ld_trxid),
                       ld->ld_nlink, ld));
}

/*#**********************************************************************\
 *
 *              dbe_logdata_free
 *
 * Physical free
 *
 * Parameters :
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
static void dbe_logdata_free(
        dbe_logdata_t* ld)
{
        su_list_node_t* n;
        dbe_hsbbuf_t* buffer;

        CHK_LOGDATA(ld);
        if (ld->hsb_beginbuf != NULL) {
            dbe_hsbbuf_done(ld->hsb_beginbuf);
        }
        if (ld->hsb_endbuf != ld->hsb_beginbuf) {
            dbe_hsbbuf_done(ld->hsb_endbuf);
        }

        if (ld->buffers != NULL) {
            su_list_do_get(ld->buffers, n, buffer) {
                dbe_hsbbuf_done(buffer);
            }
            su_list_done(ld->buffers);
        }

        if (ld->ld_local_logpos != NULL) {
            SsMemFree(ld->ld_local_logpos);
        }
        ss_dprintf_4(("dbe_logdata_free:ld=%x\n", ld));
        SsMemFree(ld);
}

/*#**********************************************************************\
 *
 *              dbe_logdata_done
 *
 * Release logdata recording reference
 *
 * Parameters :
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
void dbe_logdata_done(
        dbe_logdata_t* ld)
{
        bool free = FALSE;
        CHK_LOGDATA(ld);

        ss_dprintf_4(("dbe_logdata_done:logrectype %.255s, trxid %ld, nlink %d, ld=%x\n",
                       dbe_logi_getrectypename(ld->logrectype),
                       DBE_TRXID_GETLONG(ld->ld_trxid),
                       ld->ld_nlink, ld));

        SsSemEnter(ss_lib_sem);

        ld->ld_nlink--;
        if(ld->ld_nlink == 0) {
            free = TRUE;
        }

        SsSemExit(ss_lib_sem);

        if(free) {
            dbe_logdata_free(ld);
        }
}

#ifdef SS_SAANA_CLUSTER

/*#**********************************************************************\
 *
 *              dbe_logdata_copy
 *
 * Copyes logdata object.
 * Actual data is not copyed : link count in incremented
 *
 * Parameters :
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
dbe_logdata_t* dbe_logdata_copy(
        dbe_logdata_t* ld)
{
        dbe_logdata_t* cpld;
        su_list_node_t* n;
        dbe_hsbbuf_t* buffer;

        CHK_LOGDATA(ld);
        SS_PUSHNAME("dbe_logdata_copy");

        ss_dprintf_4(("dbe_logdata_copy:logrectype %.255s, trxid %ld, nlink %d, ld=%x\n",
                       dbe_logi_getrectypename(ld->logrectype),
                       DBE_TRXID_GETLONG(ld->ld_trxid),
                       ld->ld_nlink, ld));

        cpld = SsMemAlloc(sizeof(dbe_logdata_t));
        memcpy(cpld, ld, sizeof(dbe_logdata_t));

        dbe_hsbbuf_link(ld->hsb_beginbuf);
        if (ld->hsb_endbuf != ld->hsb_beginbuf) {
            dbe_hsbbuf_link(ld->hsb_endbuf);
        }

        if (ld->buffers != NULL) {
            cpld->buffers = su_list_init(NULL);
            su_list_do_get(ld->buffers, n, buffer) {
                dbe_hsbbuf_link(buffer);
                su_list_insertlast(cpld->buffers, buffer);
            }
        }

        SS_POPNAME;
        return(cpld);
}
#endif /* SS_SAANA_CLUSTER */


#ifdef SS_BETA

/*#***********************************************************************\
 *
 *              logdata_doprint
 *
 * Does actual logdata printing. Separated from external function to
 * avoid overhead if debug levels are not set at all.
 *
 * Parameters :
 *
 *              ld -
 *
 *
 *              txt -
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
static void logdata_doprint(
        dbe_logdata_t* ld,
        char* txt)
{
        int nrawbufs = 0;
        CHK_LOGDATA(ld);

        if (txt == NULL) {
            txt = (char *)"";
        }

        if (ld->buffers != NULL) {
            nrawbufs = su_list_length(ld->buffers);
        }

#ifdef IO_OPT
        ss_dprintf_1(("dbe_logdata_print:%.255s:%.255s, trxid %ld, nbytes %d, id %ld, nrawbufs %d\n    logpos(%d,%s,%d,%d,%d)\n    b(%x,%d) e(%x,%d)\n",
                       txt,
                       dbe_logi_getrectypename(ld->logrectype),
                       DBE_TRXID_GETLONG(ld->ld_trxid),
                       ld->nbytes,
                       dbe_catchup_logpos_dbg_id(ld->ld_logpos),
                       nrawbufs,
                       LOGPOS_DSDDD(ld->ld_logpos),
                       ld->beginabuf->alb_buffer,
                       ld->beginpos,
                       ld->endabuf->alb_buffer,
                       ld->endpos
                    ));
#else
        ss_dprintf_1(("dbe_logdata_print:%.255s:%.255s, trxid %ld, nbytes %d, id %ld, nrawbufs %d\n    logpos(%d,%s,%d,%d,%d)\n    b(%x,%d) e(%x,%d)\n",
                        txt,
                        dbe_logi_getrectypename(ld->logrectype),
                        DBE_TRXID_GETLONG(ld->ld_trxid),
                        ld->nbytes,
                        dbe_catchup_logpos_dbg_id(ld->ld_logpos),
                        nrawbufs,
                        LOGPOS_DSDDD(ld->ld_logpos),
                        ld->beginbuf,
                        ld->beginpos,
                        ld->endbuf,
                        ld->endpos
                    ));
#endif

        if (ld->nbytes < 200 && ld->nbytes > 0) {
            char* buf;
            int bufsize;
            char* _tmp;

            ss_dassert(ld->buffers == NULL);
#ifdef IO_OPT
            buf = (char*)DBE_LB_DATA(ld->beginabuf->alb_buffer) + ld->beginpos;
            if (ld->beginabuf == ld->endabuf) {
#else
            buf = (char*)DBE_LB_DATA(ld->beginbuf) + ld->beginpos;
            if (ld->beginbuf == ld->endbuf) {
#endif
                bufsize = ld->endpos - ld->beginpos;
                _tmp = SsHexStr(buf, bufsize);
                ss_bprintf_3(("dbe_logdata_print_hex:%.255s:%.255s\n", txt, _tmp));
                SsMemFree(_tmp);

            } else {
                int bufsize2;
                char* _tmp2;
                char* _tmp3;
#ifdef IO_OPT
                bufsize = dbe_hsbbuf_get_bufsize(ld->hsb_beginbuf)
                        - ld->beginpos
                        - 2*sizeof(ld->beginabuf->alb_buffer->lb_.chk);
                _tmp = SsHexStr(buf, bufsize);

                buf = (char *)DBE_LB_DATA(ld->endabuf->alb_buffer);
                bufsize2 = ld->endpos;
#else
                bufsize = dbe_hsbbuf_get_bufsize(ld->hsb_beginbuf) - ld->beginpos - 2*sizeof(ld->beginbuf->lb_.chk);
                _tmp = SsHexStr(buf, bufsize);

                buf = (char *)DBE_LB_DATA(ld->endbuf);
                bufsize2 = ld->endpos;
#endif

                if (bufsize2 > 0) {
                    _tmp2 = SsHexStr(buf, bufsize2);
                } else {
                    _tmp2 = SsMemStrdup((char *)"");
                }

                _tmp3 = SsMemAlloc(2*(bufsize+bufsize2)+1);
                SsSprintf(_tmp3, "%.255s%.255s", _tmp, _tmp2);
                ss_bprintf_3(("dbe_logdata_print_hex:%.255s:%.255s\n", txt, _tmp3));

                SsMemFree(_tmp);
                SsMemFree(_tmp2);
                SsMemFree(_tmp3);
            }
        }
}

/*#**********************************************************************\
 *
 *              dbe_logdata_print
 *
 * Print statistic of this logdata recorder.
 * For debugging/traceing
 *
 * Parameters :
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
void dbe_logdata_print(
        dbe_logdata_t* ld,
        const char* txt)
{
        ss_boutput_1((logdata_doprint(ld, (char *)txt)));
}

#endif /* SS_BETA */

/*#**********************************************************************\
 *
 *              dbe_logdata_addbuffer
 *
 * Add data to logdata
 *
 * Parameters :
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
void dbe_logdata_addbuffer(
        dbe_logdata_t* ld,
        dbe_hsbbuf_t* hsbbuf)
{
#ifdef IO_OPT
        dbe_alogbuf_t* alogbuf;
#else
        dbe_logbuf_t* logbuf;
#endif
        CHK_LOGDATA(ld);

        /*
         * the begin buffer and logbuf may be the same
         * during catchup, because we need to add buffers
         * that span multiple blocks (such as split log files).
         *
         * the check is implemented here instead for dbe4rfl.c
         * because of implementation simplicity, we can avoid
         * extra pointers.
         *
         */
        if (ld->hsb_beginbuf == hsbbuf) {
            return;
        }
        ss_debug(ld->ld_nbuffers_chk++;)

        ss_dassert(hsbbuf != NULL);
        ss_dassert(ld->hsb_endbuf == NULL);
#ifdef IO_OPT
        ss_dassert(ld->endabuf == NULL);
#else
        ss_dassert(ld->endbuf == NULL);
#endif

        if (ld->buffers == NULL) {
            ld->buffers = su_list_init(NULL);
        }
        su_list_insertlast(ld->buffers, hsbbuf);
        dbe_hsbbuf_link(hsbbuf);
#ifdef IO_OPT
        alogbuf = dbe_hsbbuf_get_alogbuf(hsbbuf);
        ss_dassert(DBE_LB_ALIGNMENT((alogbuf->alb_buffer), 
                   SS_DIRECTIO_ALIGNMENT));
        ld->nbytes = ld->nbytes
                + dbe_hsbbuf_get_bufsize(hsbbuf)
                - 2*sizeof(alogbuf->alb_buffer->lb_.chk);;
#else
        logbuf = dbe_hsbbuf_get_logbuf(hsbbuf);
        ld->nbytes = ld->nbytes
                + dbe_hsbbuf_get_bufsize(hsbbuf)
                - 2*sizeof(logbuf->lb_.chk);;
#endif
        dbe_logdata_print(ld, "dbe_logdata_addbuffer");
}


/*#**********************************************************************\
 *
 *              dbe_logdata_close
 *
 * Stop recording
 *
 * Parameters :
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
void dbe_logdata_close(
        dbe_logdata_t* ld,
        dbe_hsbbuf_t* hsbbuf,
        size_t bufpos,
        int nbuffers_chk __attribute__ ((unused)))
{
        long nbytes;

        CHK_LOGDATA(ld);
#ifdef IO_OPT
        ss_dassert(ld->endabuf == NULL);
#else
        ss_dassert(ld->endbuf == NULL);
#endif

        /*
         * this is very stupid way to do this, but it is much simpler to use
         * this module from the roll-forward log when dbe_logdata_addbuf() can
         * be called immediately each time we allocate a buffer rather than
         * when we know that no more buffers will be read.
         *
         * this hides that "complexity" behind the logdata interface
         *
         * !!!! must be replaced with a smarter implementation
         *
         */

        if (ld->buffers != NULL) {
            su_list_node_t* n;

            n = su_list_last(ld->buffers);
            ss_assert((dbe_hsbbuf_t*) su_listnode_getdata(n) != hsbbuf);
        }

#ifdef SS_DEBUG
        /*
         * Debug-Check that endbuf does not already exists
         */
        if (ld->buffers != NULL) {
            su_list_node_t* n;
            dbe_hsbbuf_t* buffer;

            su_list_do_get(ld->buffers, n, buffer) {
                ss_dassert(buffer != hsbbuf);
            }
        }
#endif
        ld->hsb_endbuf = hsbbuf;
#ifdef IO_OPT
        ld->endabuf = dbe_hsbbuf_get_alogbuf(hsbbuf);
        ss_dassert(DBE_LB_ALIGNMENT((ld->endabuf->alb_buffer), 
                   SS_DIRECTIO_ALIGNMENT));
#else
        ld->endbuf = dbe_hsbbuf_get_logbuf(hsbbuf);
#endif
        ld->endpos = bufpos;

        if (ld->hsb_endbuf != ld->hsb_beginbuf) {
            dbe_hsbbuf_link(ld->hsb_endbuf);
        }

#ifdef IO_OPT
        if (ld->beginabuf == ld->endabuf) {
            ss_dassert(ld->endpos > ld->beginpos);
            nbytes = ld->endpos - ld->beginpos;
            ss_dassert(nbuffers_chk == 1);
        } else {
            ss_debug(ld->ld_nbuffers_chk++;)
            nbytes = dbe_hsbbuf_get_bufsize(ld->hsb_beginbuf)
                    - ld->beginpos
                    - 2*sizeof(ld->beginabuf->alb_buffer->lb_.chk);
#else
        if (ld->beginbuf == ld->endbuf) {
            ss_dassert(ld->endpos > ld->beginpos);
            nbytes = ld->endpos - ld->beginpos;
            ss_dassert(nbuffers_chk == 1);
        } else {
            ss_debug(ld->ld_nbuffers_chk++;)
            nbytes = dbe_hsbbuf_get_bufsize(ld->hsb_beginbuf)
                    - ld->beginpos
                    - 2*sizeof(ld->beginbuf->lb_.chk);
#endif
            nbytes = nbytes + ld->endpos;
        }

        ld->nbytes = ld->nbytes + nbytes;

        ss_dassert(nbuffers_chk == ld->ld_nbuffers_chk);
}

/*#**********************************************************************\
 *
 *              dbe_logdata_getlogrectype
 *
 *
 * Parameters :
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
dbe_logrectype_t dbe_logdata_getlogrectype(
        dbe_logdata_t* ld)
{
        CHK_LOGDATA(ld);
        return(ld->logrectype);
}

void dbe_logdata_setrole(
        dbe_logdata_t* ld,
        hsb_role_t role)
{
        CHK_LOGDATA(ld);
        dbe_catchup_logpos_set_role(&ld->ld_logpos, role);
}

hsb_role_t dbe_logdata_getrole(
        dbe_logdata_t* ld)
{
        CHK_LOGDATA(ld);

        return(dbe_catchup_logpos_role(&ld->ld_logpos));
}


/*#**********************************************************************\
 *
 *              dbe_logdata_setlogpos
 *
 * 'patch' the logpos or this recording.
 * This is used in disk catchup because when
 * listening the 'tape' our position starts from zero and
 * we stil can start listening from any point.
 *
 * Parameters :
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
void dbe_logdata_setlogpos(
        dbe_logdata_t* ld,
        dbe_catchup_logpos_t logpos)
{
        CHK_LOGDATA(ld);
        ld->ld_logpos = logpos;
}


/*#**********************************************************************\
 *
 *              dbe_logdata_getlogpos
 *
 *
 * Parameters :
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
dbe_catchup_logpos_t dbe_logdata_getlogpos(
        dbe_logdata_t* ld)
{
        CHK_LOGDATA(ld);

        return(ld->ld_logpos);
}

#ifdef SS_HSBG2
static void logdata_getdata(dbe_logdata_t* ld, ss_byte_t* buf, size_t len)
{
        char *tmp;
        size_t nbytes;
        size_t bytes;
        size_t ncopy;
        size_t nleft = len;

        tmp = NULL;
        bytes = 0;
        for(;;) {
            tmp = dbe_logdata_getbuffer(ld, tmp, &nbytes);
            if (tmp == NULL) {
                break;
            }
            ncopy = SS_MIN(nbytes, nleft);
            memcpy(buf+bytes, tmp, ncopy);
            bytes += ncopy;
            nleft -= ncopy;
            if (bytes >= len) {
                break;
            }
        }
        ss_dassert(bytes == len);
}

/* This function should be called only once for this logdata.
 * It is required that optimisation is done outside this.
 * Catchup?
 */
static long logdata_decode_new_primary(dbe_logdata_t *ld)
{
        long primary_nodeid;
        uchar *p;
        ss_byte_t buf[sizeof(uchar) + 2 * sizeof(ss_uint4_t)];

        CHK_LOGDATA(ld);
        ss_dassert(ld->logrectype == DBE_LOGREC_HSBG2_NEW_PRIMARY);

        logdata_getdata(ld, buf, sizeof(uchar) + 2 * sizeof(ss_uint4_t));

        p = buf;
        ss_dassert(*p == ld->logrectype);
        p = buf + sizeof(uchar); /* originator nodeid */

        p = buf + sizeof(uchar) + sizeof(ss_uint4_t);
        primary_nodeid = SS_UINT4_LOADFROMDISK(p);

        return(primary_nodeid);
}

static bool logdata_decode_logpos(dbe_logdata_t *ld)
{
        CHK_LOGDATA(ld);

        if(ld->logrectype == DBE_LOGREC_HSBG2_DURABLE
        || ld->logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE
        || ld->logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK) {
            ss_byte_t buf[sizeof(uchar) + 2 * DBE_LOGPOS_BINSIZE];
            size_t len = 0;
            uchar *p;
            dbe_logfnum_t   logfnum;
            su_daddr_t      daddr;
            size_t          bufpos;
            dbe_hsb_lpid_t  id;
            hsb_role_t      role;

            switch(ld->logrectype) {
                case DBE_LOGREC_HSBG2_DURABLE:
                    len = sizeof(uchar) + 3 * sizeof(ss_uint4_t);
                    len += sizeof(dbe_hsb_lpid_t) + sizeof(uchar);
                    break;

                case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
                case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
                    len = sizeof(uchar) + 6 * sizeof(ss_uint4_t);
                    len += (2 * (sizeof(dbe_hsb_lpid_t) + sizeof(uchar)));
                    break;

                default:
                    ss_rc_error(ld->logrectype);
                    break;

            }
            ss_dassert((size_t)ld->nbytes == len);

            logdata_getdata(ld, buf, len);

            /* load local logpos */
            p = buf;
            ss_dassert(*p == ld->logrectype);
            p = buf + sizeof(uchar);


            if (ld->ld_local_logpos == NULL) {

                ld->ld_local_logpos = SsMemAlloc(sizeof(dbe_catchup_logpos_t));

                id = LPID_LOADFROMDISK(p);
                p += sizeof(dbe_hsb_lpid_t);
                role = *p;
                p += 1;
                logfnum = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(ss_uint4_t);
                daddr = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(ss_uint4_t);
                bufpos = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(ss_uint4_t);

                if (0 == logfnum) {
                    ss_rc_dassert(0 == daddr, daddr);
                    ss_rc_dassert(0 == bufpos, bufpos);
                    DBE_CATCHUP_LOGPOS_SET_NULL(*ld->ld_local_logpos);
                } else {
                    DBE_CATCHUP_LOGPOS_SET(
                        *ld->ld_local_logpos,
                        logfnum,
                        daddr,
                        bufpos);
                    dbe_catchup_logpos_set_id(ld->ld_local_logpos, id, role);
                }
                ss_dprintf_1(("logdata_decode_logpos:local:(%d,%s,%d,%d,%d)\n", LOGPOS_DSDDD((*ld->ld_local_logpos))));
            }
            if(ld->logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE
            || ld->logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK) {
                p = buf + sizeof(uchar) + DBE_LOGPOS_BINSIZE;
                id = LPID_LOADFROMDISK(p);
                p += sizeof(dbe_hsb_lpid_t);
                role = *p;
                p += 1;
                ld->ld_remote_logpos.lp_logfnum = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(ss_uint4_t);
                ld->ld_remote_logpos.lp_daddr = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(ss_uint4_t);
                ld->ld_remote_logpos.lp_bufpos = SS_UINT4_LOADFROMDISK(p);
                dbe_catchup_logpos_set_id(&ld->ld_remote_logpos, id, role);
                ss_dprintf_1(("logdata_decode_logpos:remote:(%d,%s,%d,%d,%d)\n",LOGPOS_DSDDD(ld->ld_remote_logpos)));
            }

            return (TRUE);
        }

        return (FALSE);
}

/*#**********************************************************************\
 *
 *              dbe_logdata_get_local_logpos
 *
 *
 * Parameters :
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
dbe_catchup_logpos_t dbe_logdata_get_local_logpos(
        dbe_logdata_t* ld)
{
        CHK_LOGDATA(ld);
        ss_dassert(ld->logrectype == DBE_LOGREC_HSBG2_DURABLE
                || ld->logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE
                || ld->logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK);
        logdata_decode_logpos(ld);

        return (*ld->ld_local_logpos);
}


/*#**********************************************************************\
 *
 *              dbe_logdata_get_new_primary_nodeid
 *
 *
 * Parameters :
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
long dbe_logdata_get_new_primary_nodeid(
        dbe_logdata_t* ld)
{
        long primary_nodeid;
        CHK_LOGDATA(ld);
        ss_dassert(ld->logrectype == DBE_LOGREC_HSBG2_NEW_PRIMARY);

        primary_nodeid = logdata_decode_new_primary(ld);

        return (primary_nodeid);
}

/*#**********************************************************************\
 *
 *              dbe_logdata_get_new_primary_originator
 *
 *
 * Parameters :
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

#ifdef NOT_USED_HSBG2
long dbe_logdata_get_new_primary_originator(
        dbe_logdata_t* ld)
{
        CHK_LOGDATA(ld);
        ss_dassert(ld->logrectype == DBE_LOGREC_HSBG2_NEW_PRIMARY);

        if (ld->ld_primary_nodeid == -1) {
            logdata_decode_new_primary(ld);
        }

        return (ld->ld_originator_nodeid);
}
#endif /* NOT_USED_HSBG2 */

/*##*********************************************************************\
 *
 *              dbe_logdata_get_remote_logpos
 *
 *
 * Parameters :
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
dbe_catchup_logpos_t dbe_logdata_get_remote_logpos(
        dbe_logdata_t* ld)
{
        CHK_LOGDATA(ld);
        ss_dassert(ld->logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE
                || ld->logrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK);
        logdata_decode_logpos(ld);

        return (ld->ld_remote_logpos);
}
#endif /* SS_HSBG2 */


/*##*********************************************************************\
 *
 *              dbe_logdata_nbytes
 *
 *
 * Parameters :
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
long dbe_logdata_nbytes(
        dbe_logdata_t* ld)
{
        CHK_LOGDATA(ld);
#ifdef IO_OPT
        ss_dassert(ld->endabuf != NULL);
#else
        ss_dassert(ld->endbuf != NULL);
#endif
        return(ld->nbytes);
}

/*##*********************************************************************\
 *
 *              dbe_logdata_mustflush_logrectype
 *
 *
 * Parameters :
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
bool dbe_logdata_mustflush_logrectype(
        dbe_logrectype_t rectype,
        ss_byte_t* logdata)
{
        switch (rectype) {
            case DBE_LOGREC_COMMITTRX_INFO:
                if (SU_BFLAG_TEST(*logdata, DBE_LOGI_COMMIT_NOFLUSH)) {
                    return(FALSE);
                }
                return(SU_BFLAG_TEST(*logdata, DBE_LOGI_COMMIT_2SAFE));
            case DBE_LOGREC_PREPARETRX:
            case DBE_LOGREC_CHECKPOINT_NEW:
                return(TRUE);
            default:
                break;
        }

        return(FALSE);
}

/*##*********************************************************************\
 *
 *              dbe_logdata_mustflush
 *
 *
 * Parameters :
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
bool dbe_logdata_mustflush(
        dbe_logdata_t* ld,
        bool* p_is1safe)
{
        CHK_LOGDATA(ld);
        if (ld->logrectype == DBE_LOGREC_COMMITTRX_INFO) {
            ss_byte_t buf[1+sizeof(ss_int4_t)+1];
            logdata_getdata(ld, buf, sizeof(buf));
            ss_rc_dassert(buf[0] == DBE_LOGREC_COMMITTRX_INFO, buf[0]);
            ss_rc_dassert(DBE_LOGI_ISVALIDCOMMITINFO(buf[5]), buf[5]);

            if (!SU_BFLAG_TEST(buf[1+sizeof(ss_int4_t)], DBE_LOGI_COMMIT_NOFLUSH)) {
                *p_is1safe = (!SU_BFLAG_TEST(buf[1+sizeof(ss_int4_t)], DBE_LOGI_COMMIT_2SAFE));
            } else {
                *p_is1safe = FALSE;
            }

            return(dbe_logdata_mustflush_logrectype(DBE_LOGREC_COMMITTRX_INFO, &buf[1+sizeof(ss_int4_t)]));
        } else {
            return(dbe_logdata_mustflush_logrectype(ld->logrectype, NULL));
        }
}

/*##*********************************************************************\
 *
 *              dbe_logdata_mustflush_nowait
 *
 *
 * Parameters :
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
bool dbe_logdata_mustflush_nowait(
        dbe_logdata_t* ld)
{
        CHK_LOGDATA(ld);

        switch (ld->logrectype) {
            case DBE_LOGREC_HSBG2_DURABLE:
            case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
            case DBE_LOGREC_HSBG2_NEW_PRIMARY:
                return(TRUE);
            default:
                break;
        }
        return(FALSE);
}


/*##**********************************************************************\
 *
 *              dbe_logdata_required_in_secondary
 *
 * Is this operation required to send to secondary.
 * If it is and system can not send it then catchup
 * from trxlog file is required.
 *
 * Parameters :
 *
 *      ld -
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
bool dbe_logdata_required_in_secondary(
        dbe_logdata_t* ld)
{
        bool b;

        CHK_LOGDATA(ld);

        switch (ld->logrectype) {
            case DBE_LOGREC_NOP:
            case DBE_LOGREC_HEADER:

            case DBE_LOGREC_CHECKPOINT_OLD:
            case DBE_LOGREC_SNAPSHOT_OLD:
            case DBE_LOGREC_DELSNAPSHOT:

            case DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT:
            case DBE_LOGREC_SNAPSHOT_NEW:
            case DBE_LOGREC_HSBG2_NEWSTATE:

                b = FALSE;
                break;

            default:
                b = TRUE;
                break;
        }

        return (b);
}

#ifdef SS_DEBUG

static void print_buf(char* mes, char* buf, int nbytes)
{
    if (nbytes > 512) {
        nbytes = 512;
    }
    ss_output_4( { char* _tmp = SsHexStr(buf, nbytes); SsDbgPrintf("Logbuf:print:%.255s:\n%.255s\n", mes, _tmp); SsMemFree(_tmp); } );
}

#endif /* SS_DEBUG */

/*##*********************************************************************\
 *
 *              dbe_logdata_getbuffer
 *
 * Iterate through the recording.
 *
 * Parameters :
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
char* dbe_logdata_getbuffer(
        dbe_logdata_t* ld,
        char* logbuf,
        size_t *p_bufsize)
{
        /*  there are four cases
            |-----b-----e----|            begin and end in same block
            |-----b----|------|----e----| begin and end in different block
            |-----b-----e----|            begin and end in same block with extra raw buffers (this should not be possible)
            |-----b----|------|----e----| begin and end in different block with extra raw buffers

         */

        char* buf;
        int nbytes_tmp;
#ifdef IO_OPT
        ss_dassert(ld->endabuf != NULL);
        CHK_LOGDATA(ld);
        if (ld->beginabuf == ld->endabuf) {
            ss_dassert(ld->endpos > ld->beginpos);
        }
#else
        ss_dassert(ld->endbuf != NULL);
        CHK_LOGDATA(ld);
        if (ld->beginbuf == ld->endbuf) {
            ss_dassert(ld->endpos > ld->beginpos);
        }
#endif

        if (logbuf == NULL) {
            /* this assert may not be valid: if logdata is only partially
             * written to secondary (in case of broken) we may
             * re-start from begining.
             */
            /* ss_dassert(ld->ld_bufstate == LD_BUFSTATE_BEGIN); */

            ss_bprintf_3(("dbe_logdata_getbuffer:first:begin,end %d,%d\n",
                          ld->beginpos, ld->endpos));
#ifdef IO_OPT
            buf = (char*)DBE_LB_DATA(ld->beginabuf->alb_buffer) + ld->beginpos;
            if (ld->beginabuf == ld->endabuf) {
                *p_bufsize = ld->endpos - ld->beginpos;
            } else {
                *p_bufsize = dbe_hsbbuf_get_bufsize(
                        ld->hsb_beginbuf)
                        - ld->beginpos
                        - 2*sizeof(ld->beginabuf->alb_buffer->lb_.chk);
#else
            buf = (char*)DBE_LB_DATA(ld->beginbuf) + ld->beginpos;
            if (ld->beginbuf == ld->endbuf) {
                *p_bufsize = ld->endpos - ld->beginpos;
            } else {
                *p_bufsize = dbe_hsbbuf_get_bufsize(
                        ld->hsb_beginbuf)
                        - ld->beginpos
                        - 2*sizeof(ld->beginbuf->lb_.chk);
#endif
            }
            nbytes_tmp = *p_bufsize;
            if (nbytes_tmp > 512) {
                nbytes_tmp = 512;
            }
            ld->listnode = NULL;
            if (ld->buffers != NULL) {
                ld->listnode = su_list_first(ld->buffers);
            }
            ld->ld_bufstate = LD_BUFSTATE_RAW_BEGIN;
            ss_debug(print_buf((char *)"first", buf, *p_bufsize));
            return(buf);
        }

        if (ld->ld_bufstate == LD_BUFSTATE_RAW_BEGIN) {
            ld->listnode = NULL;
            if (ld->buffers != NULL) {
                ld->listnode = su_list_first(ld->buffers);
            }
            ld->ld_bufstate = LD_BUFSTATE_RAW_NEXT;
        }

        if (ld->ld_bufstate == LD_BUFSTATE_RAW_NEXT) {
            if (ld->listnode != NULL) {
                dbe_hsbbuf_t* hb;
#ifdef IO_OPT
                dbe_alogbuf_t* alb;
                ss_dassert(ld->buffers != NULL);
                hb = su_listnode_getdata(ld->listnode);
                alb = dbe_hsbbuf_get_alogbuf(hb);
                ss_dassert(DBE_LB_ALIGNMENT((alb->alb_buffer), 
                           SS_DIRECTIO_ALIGNMENT));
                ss_dassert(alb != NULL);
                buf = (char*)DBE_LB_DATA(alb->alb_buffer);
                *p_bufsize =
                        dbe_hsbbuf_get_bufsize(hb)
                        - 2*sizeof(alb->alb_buffer->lb_.chk);

                ld->listnode = su_list_next(ld->buffers, ld->listnode);
                ss_debug(print_buf((char *)"raw", buf, *p_bufsize));
#else
                dbe_logbuf_t* lb;
                ss_dassert(ld->buffers != NULL);
                hb = su_listnode_getdata(ld->listnode);
                lb = dbe_hsbbuf_get_logbuf(hb);
                ss_dassert(lb != NULL);
                buf = (char*)DBE_LB_DATA(lb);
                *p_bufsize = dbe_hsbbuf_get_bufsize(hb) - 2*sizeof(lb->lb_.chk);

                ld->listnode = su_list_next(ld->buffers, ld->listnode);
                ss_debug(print_buf((char *)"raw", buf, *p_bufsize));
#endif /* IO_OPT */
                return(buf);
            }
            ld->ld_bufstate = LD_BUFSTATE_RAW_END;
        }

#ifdef IO_OPT
        if (ld->ld_bufstate == LD_BUFSTATE_RAW_END) {
            if (ld->beginabuf == ld->endabuf) {
                ss_bprintf_3(("dbe_logdata_getbuffer:all in one buffer\n"));
                ld->ld_bufstate = LD_BUFSTATE_END;
            } else {
                ld->ld_bufstate = LD_BUFSTATE_END;
                ss_bprintf_3(("dbe_logdata_getbuffer:next\n"));
                ss_assert(ld->beginabuf != ld->endabuf);
                buf = (char *)DBE_LB_DATA(ld->endabuf->alb_buffer);
                *p_bufsize = ld->endpos;
                if (*p_bufsize > 0) {
                    nbytes_tmp = *p_bufsize;
                    if (nbytes_tmp > 512) {
                        nbytes_tmp = 512;
                    }
                    ss_debug(print_buf((char *)"next", buf, *p_bufsize));
                    return(buf);
                }
            }
        }
#else
        if (ld->ld_bufstate == LD_BUFSTATE_RAW_END) {
            if (ld->beginbuf == ld->endbuf) {
                ss_bprintf_3(("dbe_logdata_getbuffer:all in one buffer\n"));
                ld->ld_bufstate = LD_BUFSTATE_END;
            } else {
                ld->ld_bufstate = LD_BUFSTATE_END;
                ss_bprintf_3(("dbe_logdata_getbuffer:next\n"));
                ss_assert(ld->beginbuf != ld->endbuf);
                buf = (char *)DBE_LB_DATA(ld->endbuf);
                *p_bufsize = ld->endpos;
                if (*p_bufsize > 0) {
                    nbytes_tmp = *p_bufsize;
                    if (nbytes_tmp > 512) {
                        nbytes_tmp = 512;
                    }
                    ss_debug(print_buf((char *)"next", buf, *p_bufsize));
                    return(buf);
                }
            }
        }
#endif
        if (ld->ld_bufstate == LD_BUFSTATE_END) {
            ld->ld_bufstate = LD_BUFSTATE_BEGIN;
            return(NULL);
        }

        ss_rc_derror(ld->ld_bufstate);

        return(NULL);
}
