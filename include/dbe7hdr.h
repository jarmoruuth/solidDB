/*************************************************************************\
**  source       * dbe7hdr.h
**  directory    * dbe
**  description  * Database header record management
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


#ifndef DBE7HDR_H
#define DBE7HDR_H

#include <su0svfil.h>
#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe8srec.h"
#include "dbe0type.h"

#define DBE_HEADER_1_ADDR 0L
#define DBE_HEADER_2_ADDR 1L

#define HEADER_FLAG_MYSQL               1
#define HEADER_FLAG_BNODE_MISMATCHARRAY 2
#define HEADER_FLAG_MERGECLEANUP        4

typedef struct dbe_header_st dbe_header_t;

dbe_header_t* dbe_header_init(
        size_t blocksize);

bool dbe_header_readblocksize(
        char* filename,
        size_t* p_blocksize);

bool dbe_header_read(
        dbe_header_t* dbe_header,
        dbe_cache_t* cache,
        su_daddr_t daddr);

void dbe_header_done(
        dbe_header_t* dbe_header);

dbe_header_t* dbe_header_makecopyof(
        dbe_header_t* dbe_header);

bool dbe_header_save(
        dbe_header_t* dbe_header,
        dbe_cache_t* cache,
        su_daddr_t daddr);

dbe_cpnum_t dbe_header_getcpnum(
        dbe_header_t* dbe_header);

void dbe_header_setcpnum(
        dbe_header_t* dbe_header,
        dbe_cpnum_t cpnum);

dbe_hdr_chknum_t dbe_header_getchknum(
        dbe_header_t* dbe_header);

void dbe_header_setchknum(
        dbe_header_t* dbe_header,
        dbe_hdr_chknum_t chknum);

void dbe_header_incchknum(
        dbe_header_t* dbe_header);

dbe_dbstate_t dbe_header_getdbstate(
        dbe_header_t* dbe_header);

void dbe_header_setdbstate(
        dbe_header_t* dbe_header,
        dbe_dbstate_t dbstate);

size_t dbe_header_getblocksize(
        dbe_header_t* dbe_header);

void dbe_header_setblocksize(
        dbe_header_t* dbe_header,
        size_t blocksize);

size_t dbe_header_getblocksizefrombuf(
        char *dbe_header_copy);

dbe_startrec_t* dbe_header_getstartrec(
        dbe_header_t* dbe_header);

void dbe_header_setstartrec(
        dbe_header_t* dbe_header,
        dbe_startrec_t* startrec);

su_daddr_t dbe_header_getbonsairoot(
        dbe_header_t* dbe_header);

void dbe_header_setbonsairoot(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);

su_daddr_t dbe_header_getpermroot(
        dbe_header_t* dbe_header);

void dbe_header_setpermroot(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);

su_daddr_t dbe_header_getmmiroot(
        dbe_header_t* dbe_header);

void dbe_header_setmmiroot(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);

su_daddr_t dbe_header_getfreelistaddr(
        dbe_header_t* dbe_header);


void dbe_header_setfreelistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);


su_daddr_t dbe_header_getchlistaddr(
        dbe_header_t* dbe_header);


void dbe_header_setchlistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);


su_daddr_t dbe_header_getcplistaddr(
        dbe_header_t* dbe_header);


void dbe_header_setcplistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);


su_daddr_t dbe_header_gettrxlistaddr(
        dbe_header_t* dbe_header);


void dbe_header_settrxlistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);

su_daddr_t dbe_header_getstmttrxlistaddr(
        dbe_header_t* dbe_header);

void dbe_header_setstmttrxlistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);

su_daddr_t dbe_header_getsequencelistaddr(
        dbe_header_t* dbe_header);

void dbe_header_setsequencelistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);

su_daddr_t dbe_header_getrtrxlistaddr(
        dbe_header_t* dbe_header);

void dbe_header_setrtrxlistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);

void dbe_header_setsequencelistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr);

su_daddr_t dbe_header_getfilesize(
        dbe_header_t* dbe_header);


void dbe_header_setfilesize(
        dbe_header_t* dbe_header,
        su_daddr_t filesize);

ss_uint2_t dbe_header_getheadervers(
        dbe_header_t* dbe_header);

ss_uint4_t dbe_header_getheaderflags(
        dbe_header_t* dbe_header);

void dbe_header_setheaderflags(
        dbe_header_t* dbe_header);

void dbe_header_setheadervers(
        dbe_header_t* dbe_header,
        ss_uint2_t headervers);

ss_uint2_t dbe_header_getsolidvers(
        dbe_header_t* dbe_header);

ss_uint2_t dbe_header_getdbvers(
        dbe_header_t* dbe_header);


void dbe_header_setdbvers(
        dbe_header_t* dbe_header,
        ss_uint2_t dbvers);

ss_uint4_t dbe_header_getcreatime(
        dbe_header_t* dbe_header);

void dbe_header_setcreatime(
        dbe_header_t* dbe_header,
        ss_uint4_t creatime);

ss_uint4_t dbe_header_gethsbtime(
        dbe_header_t* dbe_header);

void dbe_header_sethsbtime(
        dbe_header_t* dbe_header,
        ss_uint4_t hsbtime);

ss_uint4_t dbe_header_getctc(
        dbe_header_t* dbe_header);

void dbe_header_setctc(
        dbe_header_t* dbe_header,
        ss_uint4_t ctc);

ss_uint4_t dbe_header_calcstartreccrc(
        dbe_header_t* dbe_header);

char* dbe_header_getdefcatalog(
        dbe_header_t* dbe_header);
    
void dbe_header_setdefcatalog(
        dbe_header_t* dbe_header,
        char* defcatalog);
        
bool dbe_header_ishsbcopy(
        dbe_header_t* dbe_header);

bool dbe_header_isbrokenhsbcopy(
        dbe_header_t* dbe_header);

void dbe_header_sethsbcopy(
        char *dbe_header_copy,
        bool complete);

int dbe_header_gethsbcopy(
        char *dbe_header_copy);

void dbe_header_clearhsbcopy(
        dbe_header_t* dbe_header);

void dbe_header_clearhsbcopybrokenstatus(
        dbe_header_t* dbe_header);

#ifdef SS_MME
su_daddr_t dbe_header_getfirstmmeaddrpage(
        dbe_header_t* dbe_header);

void dbe_header_setfirstmmeaddrpage(
        dbe_header_t* dbe_header,
        su_daddr_t firstmmepageaddr);
#endif /* SS_MME */

void dbe_header_setcryptokey(
        dbe_header_t* dbe_header,
        su_cryptoalg_t alg,
        ss_uint1_t *key);

su_cryptoalg_t dbe_header_getcryptokey(
        dbe_header_t* dbe_header,
        ss_uint1_t *key);

su_cryptoalg_t dbe_header_getcryptoalg(
        dbe_header_t* dbe_header);

bool dbe_header_checkkey(
        dbe_header_t* dbe_header,
        dbe_cache_t* cache,
        su_daddr_t daddr);

#endif /* DBE7HDR_H */
