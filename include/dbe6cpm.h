/*************************************************************************\
**  source       * dbe6cpm.h
**  directory    * dbe
**  description  * checkpoint manager
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


#ifndef DBE6CPM_H
#define DBE6CPM_H

#include <ssdebug.h>
#include <sstime.h>

typedef struct dbe_cprec_st dbe_cprec_t;
typedef struct dbe_cpmgr_st dbe_cpmgr_t;

#include "dbe7trxb.h"
#include "dbe7rtrx.h"
#include "dbe0type.h"

dbe_cpmgr_t* dbe_cpmgr_init(
        dbe_file_t* dbfile);

void dbe_cpmgr_done(
        dbe_cpmgr_t* cpmgr);

dbe_ret_t dbe_cpmgr_prepare(
        dbe_cpmgr_t* cpmgr,
        dbe_cpnum_t cpnum,
        dbe_blocktype_t type,
        SsTimeT ts);

void dbe_cpmgr_createcp(
        dbe_cpmgr_t* cpmgr,
        dbe_trxbuf_t* trxbuf,
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_seq_t* seq,
        su_daddr_t bonsairoot,
        su_daddr_t permroot,
        su_daddr_t mmiroot);

bool dbe_cpmgr_flushstep(
        dbe_cpmgr_t* cpmgr);

void dbe_cpmgr_updateheaders(
        dbe_cpmgr_t* cpmgr);

void dbe_cpmgr_inheritchlist(
        dbe_cpmgr_t* cpmgr);

void dbe_cpmgr_deletecp(
        dbe_cpmgr_t* cpmgr,
        dbe_cpnum_t cpnum);

dbe_cpnum_t dbe_cpmgr_prevcheckpoint(
        dbe_cpmgr_t* cpmgr,
        dbe_cpnum_t from);

dbe_cpnum_t dbe_cpmgr_newest(
        dbe_cpmgr_t* cpmgr);

bool dbe_cpmgr_isalive(
        dbe_cpmgr_t* cpmgr,
        dbe_cpnum_t cpnum);

dbe_ret_t dbe_cpmgr_restorestartrec(
        dbe_cpmgr_t* cpmgr);

SsTimeT dbe_cpmgr_gettimestamp(
        dbe_cpmgr_t* cpmgr);

dbe_startrec_t* dbe_cprec_getstartrec(
        dbe_cprec_t* cprec);

void dbe_cprec_setstartrec(
        dbe_cprec_t* cprec,
        dbe_startrec_t* startrec);

void dbe_cpmgr_deldeadcheckpoints(
        dbe_cpmgr_t* cpmgr);

void dbe_cpmgr_getsrec(
        char* pagedata,
        dbe_startrec_t* startrec);

#ifdef SS_DEBUG

#include <ssstdlib.h>

extern int dbe_cpmgr_dbgcrashpoint;

#define DBE_CPMGR_CRASHPOINT(n) \
do { if (dbe_cpmgr_dbgcrashpoint && (n)==dbe_cpmgr_dbgcrashpoint){SsExit(0);}\
} while (0)

#else

#define DBE_CPMGR_CRASHPOINT(n)

#endif

#ifdef SS_MME
void dbe_cpmgr_setfirstmmeaddr(
        dbe_cpmgr_t* cpmgr,
        su_daddr_t firstmmepageaddr);
#endif

#endif /* DBE6CPM_H */
