/*************************************************************************\
**  source       * su0mbsvf.h
**  directory    * su
**  description  * multi-blocksize split virtual file
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


#ifndef SU0MBSVF_H
#define SU0MBSVF_H

#include <ssenv.h>
#include <ssstddef.h>
#include <ssint8.h>
#include <su0error.h>
#include <su0types.h>

typedef struct su_mbsvfil_st su_mbsvfil_t;

su_mbsvfil_t* su_mbsvf_init(void);

void su_mbsvf_done(su_mbsvfil_t* mbsvfil);

su_ret_t su_mbsvf_addfile(
        su_mbsvfil_t** p_mbsvfil,
        char* fname,
        ss_int8_t maxsize,
        size_t blocksize);

size_t su_mbsvf_getblocksize_at_addr(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t daddr);

su_ret_t su_mbsvf_read(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t daddr,
        void* buf,
        size_t bufsize);

su_ret_t su_mbsvf_write(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t daddr,
        void* buf,
        size_t bufsize);

su_ret_t su_mbsvf_decreasesize(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t newsize);

su_daddr_t su_mbsvf_getsize(su_mbsvfil_t* mbsvfil);

size_t su_mbsvf_getminblocksize(su_mbsvfil_t* mbsvfil);

size_t su_mbsvf_getmaxblocksize(su_mbsvfil_t* mbsvfil);

bool su_mbsvf_getfilespecno_and_physdaddr(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t daddr,
        int* filespecno,
        su_daddr_t* physdaddr);

char* su_mbsvf_getphysfilename(
        su_mbsvfil_t* mbsvfil,
        su_daddr_t daddr);

#endif /* SU0MBSVF_H */
