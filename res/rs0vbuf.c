/*************************************************************************\
**  source       * rs0vbuf.c
**  directory    * res
**  description  * Row value buffer.
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define RS0VBUF_C
#define RS_INTERNAL

#include "rs0ttype.h"
#include "rs0vbuf.h"

rs_vbuf_t* rs_vbuf_init(
        rs_sysi_t*      cd,
        rs_ttype_t*     ttype,
        ulong           nslots)
{
        rs_vbuf_t*      vb;
        ulong           i;

        vb = SsMemAlloc(sizeof(rs_vbuf_t)
                        + (nslots - 1) * sizeof(rs_vbuf_slot_t));

        vb->vb_nslots = nslots;
        vb->vb_nitems = 0;
        vb->vb_rpos = 0;
        vb->vb_wpos = 0;
        vb->vb_lastread = NULL;
        vb->vb_ttype = rs_ttype_copy(cd, ttype);
        for (i = 0; i < nslots; i++) {
            vb->vb_slot[i].vbs_status = RS_VB_FREE;
            vb->vb_slot[i].vbs_tval = rs_tval_create(cd, ttype);
        }
        ss_debug(vb->vb_check = RSCHK_VBUF);

        CHK_VBUF(vb);

        return vb;
}

void rs_vbuf_done(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb)
{
        ulong           i;

        CHK_VBUF(vb);
        
        for (i = 0; i < vb->vb_nslots; i++) {
            rs_tval_free(cd, vb->vb_ttype, vb->vb_slot[i].vbs_tval);
        }
        rs_ttype_free(cd, vb->vb_ttype);

        SsMemFree(vb);
}
