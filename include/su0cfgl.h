/*************************************************************************\
**  source       * su0cfgl.h
**  directory    * su
**  description  * Configuration list function.
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


#ifndef SU0CFGL_H
#define SU0CFGL_H

#include <ssc.h>

#include "su0list.h"
#include "su0inifi.h"

/* Possible flags. */
#define SU_CFGL_LONGPARAM   1
#define SU_CFGL_STRPARAM    2
#define SU_CFGL_BOOLPARAM   4
#define SU_CFGL_ISDEFAULT   8
#define SU_CFGL_ISADVANCED  16
#define SU_CFGL_ISCONST     32

typedef su_list_t su_cfgl_t;

su_cfgl_t* su_cfgl_init(void);

void su_cfgl_done(
        su_cfgl_t* cfgl);

void su_cfgl_addstrparam(
        su_cfgl_t* cfgl,
        char* section,
        char* name,
        char* value,
        char* defaultval,
        int flags);

void su_cfgl_addlongparam(
        su_cfgl_t* cfgl,
        char* section,
        char* name,
        long value,
        long defaultval,
        int flags);

void su_cfgl_addboolparam(
        su_cfgl_t* cfgl,
        char* section,
        char* name,
        bool value,
        bool defaultval,
        int flags);

void su_cfgl_addlong(
        su_cfgl_t* cfgl,
        su_inifile_t* inifile,
        const char* section,
        const char* param,
        long defaultval,
        int flags);

void su_cfgl_addbool(
        su_cfgl_t* cfgl,
        su_inifile_t* inifile,
        const char* section,
        const char* param,
        bool defaultval,
        int flags);

void su_cfgl_addstr(
        su_cfgl_t* cfgl,
        su_inifile_t* inifile,
        const char* section,
        const char* param,
        const char* defaultval,
        int flags);

void su_cfgl_getparam(
        su_list_node_t* cfgl_node,
        char** p_section,
        char** p_name,
        char** p_value,
        char** p_defaultval,
        int* p_flags);

#endif /* SU0CFGL_H */
