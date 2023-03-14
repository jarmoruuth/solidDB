/*************************************************************************\
**  source       * rs0vbuf.h
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


#ifndef RS0VBUF_H
#define RS0VBUF_H

#include "rs0types.h"
#include "rs0tval.h"

typedef struct rs_vbuf_st rs_vbuf_t;

#define CHK_VBUF(vb)  ss_dassert(SS_CHKPTR(vb) && (vb)->vb_check == RSCHK_VBUF)

typedef enum {
        RS_VB_FREE,
        RS_VB_INUSE,
        RS_VB_EOS
} rs_vbuf_slotstatus_t;

typedef struct {
        rs_vbuf_slotstatus_t    vbs_status;
        rs_tval_t*              vbs_tval;
} rs_vbuf_slot_t;

struct rs_vbuf_st {
        ss_debug(rs_check_t     vb_check;)
        ulong                   vb_nslots;
        ulong                   vb_nitems;
        ulong                   vb_rpos;
        ulong                   vb_wpos;
        rs_tval_t*              vb_lastread;
        rs_ttype_t*             vb_ttype;
        rs_vbuf_slot_t          vb_slot[1];
}; /* rs_vbuf_t */


rs_vbuf_t* rs_vbuf_init(
        rs_sysi_t*      cd,
        rs_ttype_t*     ttype,
        ulong           nslots);

void rs_vbuf_done(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE void rs_vbuf_reset(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE bool rs_vbuf_hasroom(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE bool rs_vbuf_hasdata(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE rs_tval_t* rs_vbuf_getwritable(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE void rs_vbuf_writeeos(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE void rs_vbuf_writedone(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE void rs_vbuf_abortwrite(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE rs_tval_t* rs_vbuf_readtval(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE rs_tval_t* rs_vbuf_lastread(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

SS_INLINE void rs_vbuf_rewind(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb);

#if defined(RS0VBUF_C) || defined(SS_USE_INLINE)

SS_INLINE void rs_vbuf_reset(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        ulong           i;
        
        CHK_VBUF(vb);

        for (i = 0; i < vb->vb_nslots; i++) {
            vb->vb_slot[i].vbs_status = RS_VB_FREE;
        }
        vb->vb_nitems = vb->vb_rpos = vb->vb_wpos = 0;
        vb->vb_lastread = NULL;
}

SS_INLINE bool rs_vbuf_hasroom(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        CHK_VBUF(vb);

        ss_dprintf_1(("rs_vbuf_hasroom: vb %08x rpos %d wpos %d nitems %d\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems));
        
        return (vb->vb_nitems + 1) < vb->vb_nslots;
}

SS_INLINE bool rs_vbuf_hasdata(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        CHK_VBUF(vb);
        
        ss_dprintf_1(("rs_vbuf_hasdata: vb %08x rpos %d wpos %d nitems %d\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems));
        
        return vb->vb_nitems > 0;
}

SS_INLINE rs_tval_t* rs_vbuf_getwritable(
        rs_sysi_t*      cd,
        rs_vbuf_t*      vb)
{
        rs_tval_t*      tval;
        
        CHK_VBUF(vb);
        
        ss_dprintf_1(("rs_vbuf_getwritable: vb %08x rpos %d wpos %d nitems %d\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems));
        
        if (rs_vbuf_hasroom(cd, vb)) {
            tval = vb->vb_slot[vb->vb_wpos].vbs_tval;
            ss_dassert(vb->vb_slot[vb->vb_wpos].vbs_status == RS_VB_FREE);
            vb->vb_slot[vb->vb_wpos].vbs_status = RS_VB_INUSE;
        } else {
            tval = NULL;
        }

        return tval;
}

SS_INLINE void rs_vbuf_writeeos(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        CHK_VBUF(vb);
        
        ss_dprintf_1(("rs_vbuf_writeeos: vb %08x rpos %d wpos %d nitems %d\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems));
        
        ss_dassert(vb->vb_slot[vb->vb_wpos].vbs_status == RS_VB_INUSE);
        
        vb->vb_slot[vb->vb_wpos].vbs_status = RS_VB_EOS;
        vb->vb_nitems++;
        vb->vb_wpos = (vb->vb_wpos + 1) % vb->vb_nslots;
}

SS_INLINE void rs_vbuf_writedone(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        CHK_VBUF(vb);

        ss_dprintf_1(("rs_vbuf_writedone: vb %08x rpos %d wpos %d nitems %d\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems));
        
        vb->vb_nitems++;
        vb->vb_wpos = (vb->vb_wpos + 1) % vb->vb_nslots;
}

SS_INLINE void rs_vbuf_abortwrite(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        CHK_VBUF(vb);

        ss_dprintf_1(("rs_vbuf_abortwrite: vb %08x rpos %d wpos %d nitems %d\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems));
        
        vb->vb_slot[vb->vb_wpos].vbs_status = RS_VB_FREE;
}

SS_INLINE rs_tval_t* rs_vbuf_readtval(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        rs_tval_t*      tval;

        CHK_VBUF(vb);

        ss_dprintf_1(("rs_vbuf_readtval: vb %08x rpos %d wpos %d nitems %d\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems));
        
        ss_dassert(rs_vbuf_hasdata(cd, vb));

        if (vb->vb_slot[vb->vb_rpos].vbs_status == RS_VB_EOS) {
            tval = NULL;
        } else {
            ss_dassert(vb->vb_slot[vb->vb_rpos].vbs_status == RS_VB_INUSE);
            tval = vb->vb_slot[vb->vb_rpos].vbs_tval;
        }
        vb->vb_slot[vb->vb_rpos].vbs_status = RS_VB_FREE;

        vb->vb_nitems--;
        vb->vb_rpos = (vb->vb_rpos + 1) % vb->vb_nslots;

        vb->vb_lastread = tval;
        
        return tval;
}

SS_INLINE rs_tval_t* rs_vbuf_lastread(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        
        CHK_VBUF(vb);

        ss_dprintf_1(("rs_vbuf_lastread: vb %08x rpos %d wpos %d nitems %d lastread %08x\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems, vb->vb_lastread));
        
        return vb->vb_lastread;
}

SS_INLINE void rs_vbuf_rewind(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_vbuf_t*      vb)
{
        ulong           i;
        
        CHK_VBUF(vb);

        ss_dprintf_1(("rs_vbuf_rewind: vb %08x rpos %d wpos %d nitems %d\n",
                      vb, vb->vb_rpos, vb->vb_wpos, vb->vb_nitems));
        
        for (i = vb->vb_rpos; i != vb->vb_wpos; i = (i + 1) % vb->vb_nslots) {
            ss_dprintf_2(("rs_vbuf_rewind: rewinding slot %d\n", i));
            ss_dassert(vb->vb_slot[i].vbs_status != RS_VB_FREE);
            vb->vb_slot[i].vbs_status = RS_VB_FREE;
        }
        vb->vb_wpos = vb->vb_rpos;
        vb->vb_nitems = 0;
}

#endif /* defined(RS0VBUF_C) || defined(SS_USE_INLINE) */

#endif /* RS0VBUF_H */
