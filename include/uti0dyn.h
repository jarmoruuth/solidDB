/*************************************************************************\
**  source       * uti0dyn.h
**  directory    * uti
**  description  * Dynamic strings
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

#ifndef UTI0DYN_H
#define UTI0DYN_H


#include <ssstddef.h>
#include <ssstring.h>


/* types ***************************************************/

typedef char* dstr_t;


/* global functions ****************************************/

#ifndef NO_ANSI
void   dstr_free(dstr_t* p_ds);
dstr_t dstr_set(dstr_t* p_ds, const char* str);
dstr_t dstr_app(dstr_t* p_ds, const char* str);
dstr_t dstr_setdata(dstr_t* p_ds, void* data, size_t datalen);
dstr_t dstr_appdata(dstr_t* p_ds, void* data, size_t datalen);
#else
void   dstr_free();
dstr_t dstr_set();
dstr_t dstr_app();
dstr_t dstr_setdata();
dstr_t dstr_appdata();
#endif


#endif /* UTI0DYN_H */
