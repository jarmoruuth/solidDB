/*************************************************************************\
**  source       * dbe0erro.h
**  directory    * dbe
**  description  * Error definitions.
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


#ifndef DBE0ERRO_H
#define DBE0ERRO_H

#include <rs0types.h>
#include <rs0error.h>
#include <su0error.h>

typedef su_ret_t dbe_ret_t;

void dbe_error_init(
        void);

void dbe_fatal_error(
        char* file,
        int line,
        dbe_ret_t rc);

void dbe_fileio_error(
        char* file,
        int line,
        dbe_ret_t rc);
                          
#endif /* DBE0ERRO_H */
