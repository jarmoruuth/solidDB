/*************************************************************************\
**  source       * ssmemlimit.c
**  directory    * ss
**  description  * generic memory limit handling service
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

#include "ssmemlimit.h"
#include "sscheck.h"

#define MEMLIMIT_CHECK(ml) \
        ss_dassert(SS_CHKPTR(ml) && (ml)->ml_check == SS_CHK_MEMLIMIT)

void SsMemLimitInitBuf(SsMemLimitT* limitbuf,
                       int id,
                       size_t limit_value,
                       void* ctx,
                       SsMemLimitCallBackFunT* exceedcallback,
                       SsMemLimitCallBackFunT* fallbelowcallback)
{
        ss_debug(limitbuf->ml_check = SS_CHK_MEMLIMIT);
        limitbuf->ml_ctx = ctx;
        limitbuf->ml_limit = limit_value;
        limitbuf->ml_id = id;
        limitbuf->ml_exceedcallback = exceedcallback;
        limitbuf->ml_fallbelowcallback = fallbelowcallback;
        MEMLIMIT_CHECK(limitbuf);
}

void SsMemLimitDoneBuf(SsMemLimitT* limit)
{
        MEMLIMIT_CHECK(limit);
        limit->ml_limit = 0;
        limit->ml_exceedcallback = NULL;
        limit->ml_fallbelowcallback = NULL;
        ss_debug(limit->ml_check = SS_CHK_MEMLIMIT_FREED);
}

void SsMemLimitCheck(SsMemLimitT* limit,
                     size_t old_memusage,
                     size_t new_memusage)
{
        MEMLIMIT_CHECK(limit);
        if (limit->ml_limit == 0) {
            return;
        }
        if (old_memusage <= limit->ml_limit) {
            if (new_memusage > limit->ml_limit) {
                (*limit->ml_exceedcallback)(
                        limit->ml_ctx,
                        limit->ml_id,
                        limit->ml_limit,
                        old_memusage,
                        new_memusage);
            }
        } else if (old_memusage > limit->ml_limit) {
            if (new_memusage <= limit->ml_limit) {
                (*limit->ml_fallbelowcallback)(
                        limit->ml_ctx,
                        limit->ml_id,
                        limit->ml_limit,
                        old_memusage,
                        new_memusage);
            }
        }
}

bool SsMemLimitAdjust(SsMemLimitT* limit,
                      size_t new_limitvalue,
                      size_t current_memusage)
{
        MEMLIMIT_CHECK(limit);
        if (new_limitvalue != 0) {
            if ((new_limitvalue < limit->ml_limit || limit->ml_limit == 0) &&
                new_limitvalue < current_memusage)
            {
                return (FALSE);
            }
        }
        if (limit->ml_limit != 0 &&
            current_memusage > limit->ml_limit &&
            (current_memusage <= new_limitvalue ||
             new_limitvalue == 0))
        {
            (*limit->ml_fallbelowcallback)(
                    limit->ml_ctx,
                    limit->ml_id,
                    limit->ml_limit,
                    current_memusage,
                    current_memusage);
        }
        limit->ml_limit = new_limitvalue;
        return (TRUE);
}

size_t SsMemLimitGetValue(SsMemLimitT* limit)
{
        MEMLIMIT_CHECK(limit);
        return (limit->ml_limit);
}
