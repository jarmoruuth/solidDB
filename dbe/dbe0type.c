/*************************************************************************\
**  source       * dbe0type.c
**  directory    * dbe
**  description  * Functions for common types.
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

#define DBE0TYPE_C

#include "dbe0type.h"
#include "dbe9type.h"
#include "dbe7ctr.h"

#define DBE_TYPE_GAP    ((ss_int4_t)((1024L * 1024L * 1024L) - 1024L))   /* 1G - 1024 */

#ifdef SS_DEBUG

dbe_trxnum_t dbe_trxnum_null = { DBE_CHK_TRXNUM, 0 };
dbe_trxnum_t dbe_trxnum_min =  { DBE_CHK_TRXNUM, 1 };
dbe_trxnum_t dbe_trxnum_max =  { DBE_CHK_TRXNUM, DBE_TYPE_GAP };

dbe_trxid_t  dbe_trxid_null =    { DBE_CHK_TRXID, 0 };
dbe_trxid_t  dbe_trxid_illegal = { DBE_CHK_TRXID, -1 };
dbe_trxid_t  dbe_trxid_min =     { DBE_CHK_TRXID, 1 };
dbe_trxid_t  dbe_trxid_max =     { DBE_CHK_TRXID, DBE_TYPE_GAP };

#else /* SS_DEBUG */

dbe_trxnum_t dbe_trxnum_null = 0;
dbe_trxnum_t dbe_trxnum_min =  1;
dbe_trxnum_t dbe_trxnum_max =  DBE_TYPE_GAP;

dbe_trxid_t  dbe_trxid_null =    0;
dbe_trxid_t  dbe_trxid_illegal = -1;
dbe_trxid_t  dbe_trxid_min =     1;
dbe_trxid_t  dbe_trxid_max =     DBE_TYPE_GAP;

#endif /* SS_DEBUG */

static ss_uint4_t dbe_type_skipgap(ss_uint4_t curval, int direction)
{
        if ((ss_uint4_t)(curval - 1) >= (ss_uint4_t)(DBE_CTR_MAXLIMIT - 1)) {
            if (direction < 0) { /* decrement reches gap, jump below the gap */
                curval = (ss_uint4_t)(DBE_CTR_MAXLIMIT - 1);
            } else { /* increment reaches gap, jump above the gap */
                curval = (ss_uint4_t)1;
            }
        }
        return(curval);
}

#ifdef SS_DEBUG

dbe_trxnum_t dbe_trxnum_init(long n)
{
        dbe_trxnum_t trxnum;

        trxnum.num = n;
        ss_debug(trxnum.chk = DBE_CHK_TRXNUM);

        CHK_TRXNUM(trxnum);

        return(trxnum);
}

dbe_trxnum_t dbe_trxnum_initfromtrxid(dbe_trxid_t trxid)
{
        dbe_trxnum_t trxnum;

        CHK_TRXID(trxid);

        trxnum.num = trxid.id;
        ss_debug(trxnum.chk = DBE_CHK_TRXNUM);

        CHK_TRXNUM(trxnum);

        return(trxnum);
}

dbe_trxnum_t dbe_trxnum_sum(dbe_trxnum_t trxnum, int n)
{
        CHK_TRXNUM(trxnum);
        
        ss_rc_dassert(n != 0, n);
        trxnum.num += n;
        trxnum.num = dbe_type_skipgap(trxnum.num, n);
        return(trxnum);
}

#else /* SS_DEBUG */

dbe_trxnum_t dbe_trxnum_sum(dbe_trxnum_t trxnum, int n)
{
        CHK_TRXNUM(trxnum);
        
        ss_rc_dassert(n != 0, n);
        trxnum += n;
        trxnum = dbe_type_skipgap(trxnum, n);
        return(trxnum);
}

#endif /* SS_DEBUG */

#ifdef SS_DEBUG

long dbe_trxnum_getlong(dbe_trxnum_t trxnum)
{
        CHK_TRXNUM(trxnum);
        
        return(trxnum.num);
}

#endif /* SS_DEBUG */

#ifdef SS_DEBUG

bool dbe_trxnum_equal(dbe_trxnum_t trxnum1, dbe_trxnum_t trxnum2)
{
        CHK_TRXNUM(trxnum1);
        CHK_TRXNUM(trxnum2);
        
        return(trxnum1.num == trxnum2.num);
}

dbe_trxid_t dbe_trxid_init(long n)
{
        dbe_trxid_t trxid;

        trxid.id = (ss_int4_t)n;
        ss_debug(trxid.chk = DBE_CHK_TRXID);

        CHK_TRXID(trxid);

        return(trxid);
}

dbe_trxid_t dbe_trxid_initfromtrxnum(dbe_trxnum_t trxnum)
{
        dbe_trxid_t trxid;

        CHK_TRXNUM(trxnum);

        trxid.id = trxnum.num;
        ss_debug(trxid.chk = DBE_CHK_TRXID);

        CHK_TRXID(trxid);

        return(trxid);
}

dbe_trxid_t dbe_trxid_sum(dbe_trxid_t trxid, int n)
{
        CHK_TRXID(trxid);

        ss_rc_dassert(n != 0, n);
        trxid.id += n;
        trxid.id = (ss_int4_t)dbe_type_skipgap((ss_uint4_t)trxid.id, n);
        return(trxid);
}

#else /* SS_DEBUG */

dbe_trxid_t dbe_trxid_sum(dbe_trxid_t trxid, int n)
{
        CHK_TRXID(trxid);

        ss_rc_dassert(n != 0, n);
        trxid += n;
        trxid = (ss_int4_t)dbe_type_skipgap((ss_uint4_t)trxid, n);
        return(trxid);
}


#endif /* SS_DEBUG */

#ifdef SS_DEBUG

long dbe_trxid_getlong(dbe_trxid_t trxid)
{
        CHK_TRXID(trxid);
        
        return(trxid.id);
}

#endif /* SS_DEBUG */

#ifdef SS_DEBUG

bool dbe_trxid_equal(dbe_trxid_t trxid1, dbe_trxid_t trxid2)
{
        CHK_TRXID(trxid1);
        CHK_TRXID(trxid2);
        
        return(trxid1.id == trxid2.id);
}

#endif /* SS_DEBUG */

void dbe_type_updateconst(dbe_counter_t* ctr)
{
        dbe_trxnum_t curtrxnum;
        dbe_trxid_t  curtrxid;

        curtrxnum = dbe_counter_getcommittrxnum(ctr);
        curtrxid = dbe_counter_gettrxid(ctr);

        dbe_trxnum_min =  dbe_trxnum_init(dbe_type_skipgap(dbe_trxnum_getlong(curtrxnum) - DBE_TYPE_GAP, 1));
        dbe_trxnum_max = dbe_trxnum_init(dbe_type_skipgap(dbe_trxnum_getlong(curtrxnum) + DBE_TYPE_GAP, 1));

        dbe_trxid_min = dbe_trxid_init(dbe_type_skipgap(dbe_trxid_getlong(curtrxid) - DBE_TYPE_GAP, 1));
        dbe_trxid_max = dbe_trxid_init(dbe_type_skipgap(dbe_trxid_getlong(curtrxid) + DBE_TYPE_GAP, 1));
}
