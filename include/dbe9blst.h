/*************************************************************************\
**  source       * dbe9blst.h
**  directory    * dbe
**  description  * Block list template data structure
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


#ifndef DBE9BLST_H
#define DBE9BLST_H

#include "dbe9bhdr.h"

/* block list header */
struct dbe_blheader_st {
        dbe_blocktype_t     bl_type;    /* block type field */
        dbe_cpnum_t         bl_cpnum;   /* checkpoint number */
        dbe_bl_nblocks_t    bl_nblocks; /* number of data items in block */
        su_daddr_t          bl_next;    /* disk pointer to next block */
};

typedef struct dbe_blheader_st dbe_blheader_t;

#ifndef NO_ANSI

dbe_blheader_t *dbe_blh_init(
        dbe_blheader_t *p_hdr,
        dbe_blocktype_t type,
        dbe_cpnum_t cpnum);

void dbe_blh_get(
        dbe_blheader_t *p_hdr,
        void *diskbuf);

void dbe_blh_put(
        dbe_blheader_t *p_hdr,
        void *diskbuf);

#else /* NO_ANSI */

dbe_blheader_t *dbe_blh_init();
void  dbe_blh_get();
void dbe_blh_put();

#endif /* NO_ANSI */

#define DBE_BLIST_NBLOCKSOFFSET    6
#define DBE_BLIST_NEXTOFFSET       8
#define DBE_BLIST_DATAOFFSET       12

#endif /* DBE9BLST_H */



