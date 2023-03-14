/*************************************************************************\
**  source       * rs0order.h
**  directory    * res
**  description  * Order by services.
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


#ifndef RS0ORDER_H
#define RS0ORDER_H

#include <ssc.h>

#include "rs0types.h"

typedef struct tborderbystruct rs_ob_t;

rs_ob_t* rs_ob_init(
        void*    cd,
        rs_ano_t ano,
        bool     asc
);

void rs_ob_done(
        void*    cd,
        rs_ob_t* ob
);

rs_ano_t rs_ob_ano(
        void*    cd,
        rs_ob_t* ob
);

bool rs_ob_asc(
        void*    cd,
        rs_ob_t* ob
);

void rs_ob_setasc(
        void*    cd,
        rs_ob_t* ob
);

void rs_ob_setdesc(
        void*    cd,
        rs_ob_t* ob
);

void rs_ob_setsolved(
        void*    cd,
        rs_ob_t* ob,
        bool solved
);

bool rs_ob_issolved(
        void*    cd,
        rs_ob_t* ob
);

#endif /* RS0ORDER_H */
