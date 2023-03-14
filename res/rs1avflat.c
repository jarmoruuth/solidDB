/*************************************************************************\
**  source       * rs1avflat.c
**  directory    * res
**  description  * flattening optimizations for rs_aval_t data structure
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

#define RS_INTERNAL

#include <ssenv.h>
#include <ssc.h>
#include <ssdebug.h>
#include <ssstddef.h>
#include <ssmem.h>

#include "rs0atype.h"
#include "rs0aval.h"

va_t* rs_aval_deconvert(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        rs_datatype_t dt;
        CHECK_AVAL(aval);
        ss_dassert(SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED) &&
                   SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_VTPLREF | RA_BLOB));
        switch (dt = RS_ATYPE_DATATYPE(cd, atype)) {
            case RSDT_INTEGER:
                va_setlong(&aval->ra_vabuf.va, aval->ra_.l);
                break;
            case RSDT_BIGINT:
                va_setint8(&aval->ra_vabuf.va, aval->ra_.i8);
                break;
            case RSDT_DFLOAT:
                dt_dfl_dfltova(aval->ra_.dfl, &aval->ra_vabuf.va);
                break;
            case RSDT_DOUBLE:
                va_setdouble(&aval->ra_vabuf.va, aval->ra_.d);
                break;
            case RSDT_FLOAT:
                va_setfloat(&aval->ra_vabuf.va, aval->ra_.f);
                break;
            default:
                ss_rc_error(dt);
                break;
        }
        SU_BFLAG_SET(aval->ra_flags, RA_FLATVA);
        SU_BFLAG_CLEAR(aval->ra_flags, RA_ONLYCONVERTED);
        aval->ra_va = &aval->ra_vabuf.va;
        return (&aval->ra_vabuf.va);
}
