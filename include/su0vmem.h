/*************************************************************************\
**  source       * su0vmem.h
**  directory    * su
**  description  * Virtual memory system.
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


#ifndef SU0VMEM_H
#define SU0VMEM_H

#include <ssstdio.h>

#include <ssc.h>

#include "su0types.h"

/* Virtual memory info strcuture.
*/
typedef struct {
        int     vmemi_minchain;     /* Minimum hash chain length. */
        int     vmemi_maxchain;     /* Maximum hash chain length. */
        double  vmemi_avgchain;     /* Average hash chain length. */
        int     vmemi_nchain;       /* Number of used hash slots. */
        int     vmemi_nslot;        /* Number of hash slots. */
        int     vmemi_nitem;        /* Number of virtual memory items. */
        long    vmemi_nfind;        /* Number of finds done in vmem. */
        long    vmemi_nread;        /* Number of disk reads. */
        long    vmemi_nwrite;       /* Number of disk writes. */
        double  vmemi_readhitrate;  /* Vmem read hit rate. */
} su_vmem_info_t;

/* typedefs for callback functions for getting/releasing extra file names
 * needed for exceeding the 2 GB limit
 */
typedef char* getnewfname_callback_t(void* ctx, uint num);
typedef void releasefname_callback_t(void* ctx, uint num, char* fname);

/* Virtual memory address type. */
typedef su_daddr_t vmem_addr_t;

typedef struct su_vmem_st su_vmem_t;

su_vmem_t* su_vmem_open(
        char* fname,
        uint nblock,
        void* blocks, /* void* blocks[nblocks], actually*/
        size_t blocksize,
        int openflags,
        getnewfname_callback_t* getnewfname_callback,
        releasefname_callback_t* releasefname_callback,
        void* ctx);

void su_vmem_delete(
        su_vmem_t* vmem);

void su_vmem_rewrite(
        su_vmem_t* vmem);

void su_vmem_removebuffers(
        su_vmem_t* vmem);

void su_vmem_addbuffers(
        su_vmem_t* vmem,
        uint nblock,
        void* blocks);  /* void**, actually */

void su_vmem_setinfo(
        su_vmem_t* vmem,
        su_vmem_info_t* info);

void su_vmem_getinfo(
        su_vmem_t* vmem,
        su_vmem_info_t* info);

vmem_addr_t su_vmem_sizeinblocks(
        su_vmem_t* vmem);

uint su_vmem_blocksize(
        su_vmem_t* vmem);

char* su_vmem_reach(
        su_vmem_t* vmem,
        vmem_addr_t addr,
        uint* p_byte_c);

char* su_vmem_reachnew(
        su_vmem_t* vmem,
        vmem_addr_t* p_addr,
        uint* p_byte_c);

void su_vmem_release(
        su_vmem_t* vmem,
        vmem_addr_t addr,
        bool wrote);

bool su_vmem_syncsizeifneeded(
        su_vmem_t* vmem);

#endif /* SU0VMEM_H */
