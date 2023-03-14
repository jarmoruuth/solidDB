/*************************************************************************\
**  source       * su0svfil.h
**  directory    * su
**  description  * virtual splittable file declarations
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


#ifndef SU0SVFIL_H
#define SU0SVFIL_H

#include <ssstddef.h>

#include <ssc.h>
#include <sslimits.h>
#include <ssint8.h>

#include "su0error.h"
#include "su0cipher.h"

#ifndef SU_DADDR_T_DEFINED
#define SU_DADDR_T_DEFINED
typedef FOUR_BYTE_T su_daddr_t;
#endif /* SU_DADDR_T_DEFINED */

#ifndef SU_DADDR_NULL
#define SU_DADDR_NULL   ((su_daddr_t)-1L)
#define SU_DADDR_MAX    (SU_DADDR_NULL - 1U)
#endif /* SU_DADDR_NULL */

/* Maximum portable size for an individual file */
#define SU_VFIL_SIZE_MAX SS_INT4_MAX

typedef struct su_svfil_st su_svfil_t;

su_svfil_t *su_svf_init(
        size_t blocksize,
        uint flags);

void su_svf_done(
        su_svfil_t *svfp);

su_ret_t su_svf_addfile(
        su_svfil_t *svfp,
        char *pathname,
        ss_int8_t maxsize,
        bool persistent);

su_ret_t su_svf_addfile2(
        su_svfil_t *svfp,
        char *pathname,
        ss_int8_t maxsize,
        bool persistent,
        uint diskno);

su_ret_t su_svf_read(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        size_t *sizeread);

su_ret_t su_svf_read_raw(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        size_t *sizeread);

su_ret_t su_svf_write(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size);

su_ret_t su_svf_readlocked(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        size_t *sizeread);

su_ret_t su_svf_readlocked_raw(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size,
        size_t *sizeread);

su_ret_t su_svf_writelocked(
        su_svfil_t *svfp,
        su_daddr_t loc,
        void *data,
        size_t size);

su_ret_t su_svf_append(
        su_svfil_t *svfp,
        void *data,
        size_t size);

su_ret_t su_svf_lockrange(
        su_svfil_t *svfp,
        su_daddr_t start,
        su_daddr_t length);

su_ret_t su_svf_unlockrange(
        su_svfil_t *svfp,
        su_daddr_t start,
        su_daddr_t length);

su_ret_t su_svf_extendsize(
        su_svfil_t *svfp,
        su_daddr_t sz);

su_ret_t su_svf_decreasesizeby1(
        su_svfil_t* svfp);

su_ret_t su_svf_decreasesize(
        su_svfil_t* svfp,
        su_daddr_t daddr);

void su_svf_flush(
        su_svfil_t *svfp);

su_daddr_t su_svf_getsize(
        su_svfil_t *svfp);

size_t su_svf_getblocksize(
        su_svfil_t *svfp);

char* su_svf_getphysfilenamewithrange(
        su_svfil_t* svfp,
        su_daddr_t daddr,
        int* p_filespecno,
        su_daddr_t* p_blocks_left);

char* su_svf_getphysfilename(
        su_svfil_t* svfp,
        su_daddr_t daddr);

int su_svf_getdiskno(
        su_svfil_t* svfp,
        su_daddr_t daddr);

su_svfil_t *su_svf_initcopy(
        char* dir,
        su_svfil_t *svfp,
        su_daddr_t* p_copysize,
        uint flags,
        su_ret_t* p_rc);

su_svfil_t* su_svf_init_fixed(
        size_t blocksize,
        uint flags);

int su_svf_getfilespecno(
        su_svfil_t* svfp,
        su_daddr_t daddr);

bool su_svf_getfilespecno_and_physdaddr(
        su_svfil_t* svfp,
        su_daddr_t daddr,
        int *filespecno,
        su_daddr_t *physdaddr);

bool su_svf_isreadonly(
        su_svfil_t* svfp);

su_daddr_t su_svf_getmaxsize(
        su_svfil_t *svfp);

su_ret_t su_svf_removelastfile(
        su_svfil_t* svfp);

float su_svf_phfilefullness(
        su_svfil_t* svfp,
        uint nth);

void su_svf_fileusageinfo(
        su_svfil_t* svfp,
        double* maxsize,
        double* currsize,
        float* totalperc,
        uint nth,
        float* vfilperc);

su_ret_t su_svf_filenameinuse(
        su_svfil_t* svfp,
        char* pathname);

ss_int8_t su_svf_getnbyteswritten(
        su_svfil_t*     svfp);

void su_svf_zeronbyteswritten(
        su_svfil_t*     svfp);

#ifdef MME_CP_FIX

typedef struct {
        SsLIOReqTypeT lr_reqtype;
        su_daddr_t lr_daddr;
        void* lr_data;
        size_t lr_size;
        su_ret_t lr_rc;
} su_svf_lioreq_t;

su_ret_t su_svf_listio(
        su_svfil_t* svf,
        su_svf_lioreq_t req_array[],
        size_t nreq);

#endif /* MME_CP_FIX */

void su_svf_setcipher(
        su_svfil_t* svf,
        void* cipher,
        char *(SS_CALLBACK *encrypt)(void *cipher, su_daddr_t daddr, char *page,
                         int npages, size_t pagesize),
        bool  (SS_CALLBACK *decrypt)(void *cipher, su_daddr_t daddr, char *page,
                         int npages, size_t pagesize));

void *su_svf_getcipher(su_svfil_t* svf);

su_ret_t su_svf_encryptall(
        su_svfil_t*  svfp);

su_ret_t su_svf_decryptall(
        su_svfil_t*  svfp,
        su_cipher_t* old_cipher);

#endif /* SU0SVFIL_H */
