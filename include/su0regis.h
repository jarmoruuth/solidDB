/*************************************************************************\
**  source       * su0regis.h
**  directory    * su
**  description  * Header for a quick-and-dirty main register structure.
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


#ifndef SU0REGIS_H
#define SU0REGIS_H

struct su_regis_deprecated_st {
        const char*                dep_name;
        const char*                dep_official;
};

typedef struct su_regis_deprecated_st su_regis_deprecated_t;

extern const char*           su_regis_simple_register[];
extern su_regis_deprecated_t su_regis_simple_replaced_register[];
extern su_regis_deprecated_t su_regis_simple_discontinued_register[];

#endif /* SU0REGIS_H */

/*  EOF  */
