/*************************************************************************\
**  source       * su0err.h
**  directory    * su
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


#ifndef SU0ERR_H
#define SU0ERR_H

#include <ssc.h>
#include <ssstdarg.h>
#include <su0error.h>

typedef struct suerrstruct su_err_t;
 

void SS_CDECL su_err_init(
        su_err_t** p_errh,
        su_ret_t   code,
        ...
);

void su_err_init_noargs(
        su_err_t** p_errh,
        su_ret_t code);


void SS_CDECL su_err_vinit(
        su_err_t** p_errh,
        su_ret_t   code,
        va_list    arg_ptr
);

void su_err_init_text(
        su_err_t** p_errh,
        su_ret_t   code,
        char*      text
);

void su_err_done(
        su_err_t* errh
);

void su_err_printinfo(
        su_err_t* errh,
        su_ret_t* p_errcode,
        char**    p_errstr
);

char* su_err_geterrstr(
        su_err_t* errh
);

int su_err_geterrcode(
        su_err_t* errh
);

void su_err_copyerrh(
        su_err_t** p_errh,
        su_err_t* errh);

#ifdef SS_DEBUG

bool su_err_check(
        su_err_t* err);

#endif /* SS_DEBUG */

#endif /* SU0ERR_H */
