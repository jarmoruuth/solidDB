/*************************************************************************\
**  source       * xs0acnd.h
**  directory    * xs
**  description  * Attributewise sort condition interface
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


#ifndef XS0ACND_H
#define XS0ACND_H

#include <ssc.h>
#include <rs0types.h>

typedef struct {
        bool        sac_asc;
        rs_ano_t    sac_ano;
} xs_acond_t;

xs_acond_t* xs_acond_init(
        bool ascending,
        rs_ano_t ano);

void xs_acond_done(xs_acond_t* acond);

#define xs_acond_asc(acond)       ((acond)->sac_asc)
#define xs_acond_ano(acond)       ((acond)->sac_ano)

#endif /* XS0ACND_H */
