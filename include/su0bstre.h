/*************************************************************************\
**  source       * su0bstre.h
**  directory    * su
**  description  * Byte stream object
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


#ifndef SU0BSTRE_H
#define SU0BSTRE_H

#include "su0types.h"
#include "su0error.h"
#include "su0err.h"

typedef size_t (*su_bstream_iofp_t)(
        void*   param,
        char*   buf,
        size_t  bufsize
);

typedef char* (*su_bstream_reachfp_t)(
        void*   param,
        size_t* p_nbytes
);

typedef void (*su_bstream_releasefp_t)(
        void*   param,
        size_t  n_bytes
);

typedef bool (*su_bstream_ioendfp_t)(
        void*   param
);

typedef void (*su_bstream_closefp_t)(
        void*   param
);

typedef void (*su_bstream_abortfp_t)(
        void*   param
);

typedef su_err_t* (*su_bstream_suerrfp_t)(
        void*   param
);

su_bstream_t* su_bstream_initwrite(
        su_bstream_iofp_t       writefp,
        su_bstream_reachfp_t    reachfp,
        su_bstream_releasefp_t  releasefp,
        su_bstream_closefp_t    closefp,
        su_bstream_abortfp_t    abortfp,
        su_bstream_suerrfp_t    suerrfp,
        void*                   param
);

su_bstream_t* su_bstream_initread(
        su_bstream_iofp_t       readfp,
        su_bstream_reachfp_t    reachfp,
        su_bstream_releasefp_t  releasefp,
        su_bstream_closefp_t    closefp,
        su_bstream_abortfp_t    abortfp,
        su_bstream_suerrfp_t    suerrfp,
        void*                   param
);

su_bstream_t* su_bstream_initreadwrite(
        su_bstream_iofp_t       readfp,
        su_bstream_iofp_t       writefp,
        su_bstream_ioendfp_t    writeendfp,
        su_bstream_closefp_t    closefp,
        su_bstream_abortfp_t    abortfp,
        su_bstream_suerrfp_t    suerrfp,
        void*                   param
);

su_bstream_t* su_bstream_link(
        su_bstream_t* bstream);

void su_bstream_done(        
        su_bstream_t* bstream
);

void su_bstream_abort(
        su_bstream_t* bstream
);

void* su_bstream_getparam(
        su_bstream_t* bstream);

su_err_t* su_bstream_givesuerr(
        su_bstream_t* bstream);

/*** Inbound (READ) stream ***/

su_ret_t su_bstream_read(
        su_bstream_t* bstream,
        char*         buf,
        size_t        bufsize,
        size_t*       p_read
);

char* su_bstream_reachforread(
        su_bstream_t* bstream,
        size_t*       p_avail
);

void su_bstream_releaseread(
        su_bstream_t* bstream,
        size_t        n_read
);


/*** Outbound (WRITE) stream ***/

su_ret_t su_bstream_write(
        su_bstream_t* bstream,
        char*         buf,
        size_t        bufsize,
        size_t*       p_written
);

char* su_bstream_reachforwrite(
        su_bstream_t* bstream,
        size_t*       p_avail
);

void su_bstream_releasewrite(
        su_bstream_t* bstream,
        size_t        n_written
);

bool su_bstream_writeend(
        su_bstream_t* bstream
);

/*** two streams ***/

su_ret_t su_bstream_copy(
        su_bstream_t* trg_bstream,
        su_bstream_t* src_bstream,
        size_t*       p_ncopied
); 

#endif /* SU0BSTRE_H */
