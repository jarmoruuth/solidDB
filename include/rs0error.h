/*************************************************************************\
**  source       * rs0error.h
**  directory    * res
**  description  * Error generation and handling
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


#ifndef RS0ERROR_H
#define RS0ERROR_H

#include <ssc.h>

#include <su0err.h>

#include "rs0types.h"

void SS_CDECL rs_error_create(
        rs_err_t** p_errh,
        uint       code,
        ...
);

void SS_CDECL rs_error_create_key(
        rs_err_t** p_errh,
        uint       code,
        rs_key_t*  key
);

void rs_error_create_text(
        rs_err_t** p_errh,
        uint       code,
        char*      text
);

void rs_error_free(
        void*     cd,
        rs_err_t* errh
);

void rs_error_printinfo(
        void*     cd,
        rs_err_t* errh,
        uint*     errcode,
        char**    errstr
);

char* rs_error_geterrstr(
        void*     cd,
        rs_err_t* errh
);

int rs_error_geterrcode(
        void*     cd,
        rs_err_t* errh
);

void rs_error_copyerrh(
        rs_err_t** p_errh,
        rs_err_t* errh);

#endif /* RS0ERROR_H */
