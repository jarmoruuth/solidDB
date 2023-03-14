/*************************************************************************\
**  source       * dbe7trxi.c
**  directory    * dbe
**  description  * Transaction info structure.
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

#define DBE7TRXI_C

#include <ssmem.h>
#include <ssdebug.h>

#include <rs0sysi.h>

#include "dbe0type.h"
#include "dbe7trxi.h"

dbe_trxinfo_t* dbe_trxinfo_init(rs_sysi_t* cd)
{
        dbe_trxinfo_t* ti;

        SS_MEMOBJ_INC(SS_MEMOBJ_TRXINFO, dbe_trxinfo_t);

        ti = rs_sysi_qmemctxalloc(cd, sizeof(dbe_trxinfo_t));

        dbe_trxinfo_initbuf(cd, ti);
        
        return(ti);
}


void dbe_trxinfo_done_nomutex(dbe_trxinfo_t* ti, rs_sysi_t* cd)
{
        ss_debug(ss_int4_t dbg_nlinks;)
        ss_int4_t nlinks;

        CHK_TRXINFO(ti);

        /* MUTEX BEGIN */
        ss_debug(dbg_nlinks = ti->ti_nlinks);

        ss_dassert(ti->ti_nlinks > 0);

        nlinks = --ti->ti_nlinks;

        /* MUTEX END */

        if (nlinks == 0) {
            dbe_trxinfo_donebuf_nomutex(ti, cd);
            SS_MEMOBJ_DEC(SS_MEMOBJ_TRXINFO);
            ss_rc_dassert(ti->ti_nlinks == 0, ti->ti_nlinks);
            rs_sysi_qmemctxfree(cd, ti);
        } else {
            ss_rc_dassert(dbg_nlinks - 1 == ti->ti_nlinks, ti->ti_nlinks);
        }
}

