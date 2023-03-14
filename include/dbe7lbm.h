/*************************************************************************\
**  source       * dbe7lbm.h
**  directory    * dbe
**  description  * log-buffer manager
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


#ifndef DBE7LBM_H
#define DBE7LBM_H

#include <ssc.h>
#include <ssdebug.h>

#include "dbe0hsbbuf.h"

typedef struct dbe_lbm_st dbe_lbm_t;

dbe_lbm_t* dbe_lbm_init(
        void);

void dbe_lbm_done(
        dbe_lbm_t* lbm);

dbe_hsbbuf_t* dbe_lbm_getnext_hsbbuffer(
        dbe_lbm_t* lbm,
        dbe_hsbbuf_t* prevbuffer,
        size_t bufsize);

#endif /* DBE7LBM_H */
