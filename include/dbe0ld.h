/*************************************************************************\
**  source       * dbe0ld.h
**  directory    * dbe
**  description  * Container for all buffers related to single 
**               * log operation
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


#ifndef DBE0LD_H
#define DBE0LD_H

#include <ssc.h>
#include <ssdebug.h>

#include "dbe0type.h"
#include "dbe0catchup.h"
#include "dbe0logi.h"
#include "dbe0hsbbuf.h"
#include "dbe0lb.h"

dbe_logdata_t* dbe_logdata_init(
        dbe_trxid_t trxid,
        dbe_logrectype_t logrectype,
        bool  split_queue_force,
        dbe_hsbbuf_t* hsbbuf,
        size_t bufpos,
        dbe_catchup_logpos_t logpos);

void dbe_logdata_link(
        dbe_logdata_t* ld);

void dbe_logdata_done(
        dbe_logdata_t* ld);

dbe_logdata_t* dbe_logdata_copy(
        dbe_logdata_t* ld);

#ifdef SS_BETA
void dbe_logdata_print(
        dbe_logdata_t* ld,
        const char* txt);
#else /* SS_BETA */
#define dbe_logdata_print(ld,txt)
#endif /* SS_BETA */

void dbe_logdata_addbuffer(
        dbe_logdata_t* ld,
        dbe_hsbbuf_t* hsbbuf);

void dbe_logdata_close(
        dbe_logdata_t* ld,
        dbe_hsbbuf_t* hsbbuf,
        size_t bufpos,
        int nbuffers_chk);

void dbe_logdata_setlogrectype(
        dbe_logdata_t* ld,
        dbe_logrectype_t logrectype);

dbe_logrectype_t dbe_logdata_getlogrectype(
        dbe_logdata_t* ld);

/*
void dbe_logdata_settrxid(
        dbe_logdata_t* ld,
        dbe_trxid_t trxid);

dbe_trxid_t dbe_logdata_gettrxid(
        dbe_logdata_t* ld);
*/

long dbe_logdata_nbytes(
        dbe_logdata_t* ld);

char* dbe_logdata_getbuffer(
        dbe_logdata_t* ld,
        char* logbuf,
        size_t *p_bufsize);

bool dbe_logdata_split_hsbqueue(
        dbe_logdata_t* ld);

bool dbe_logdata_mustflush_logrectype(
        dbe_logrectype_t rectype,
        ss_byte_t* logdata);

bool dbe_logdata_mustflush(
        dbe_logdata_t* ld,
        bool* p_is1safe);

bool dbe_logdata_mustflush_nowait(
        dbe_logdata_t* ld);

bool dbe_logdata_required_in_secondary(
        dbe_logdata_t* ld);

#ifdef SS_HSBG2

void dbe_logdata_setrole(
        dbe_logdata_t* ld,
        hsb_role_t role);

hsb_role_t dbe_logdata_getrole(
        dbe_logdata_t* ld);

void dbe_logdata_setlogpos(
        dbe_logdata_t* ld,
        dbe_catchup_logpos_t logpos);

dbe_catchup_logpos_t dbe_logdata_getlogpos(
        dbe_logdata_t* ld);

dbe_catchup_logpos_t dbe_logdata_get_local_logpos(
        dbe_logdata_t* ld);

dbe_catchup_logpos_t dbe_logdata_get_remote_logpos(
        dbe_logdata_t* ld);

long dbe_logdata_get_new_primary_nodeid(
        dbe_logdata_t* ld);

long dbe_logdata_get_new_primary_originator(
        dbe_logdata_t* ld);

#endif /* SS_HSBG2 */

#endif /* DBE0LD_H */
