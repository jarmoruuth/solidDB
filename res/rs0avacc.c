/*************************************************************************\
**  source       * rs0avacc.c
**  directory    * res
**  description  * Accelerator specific aval info.
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

#define RS0AVACC_C

#include <ssenv.h>
#include <ssc.h>
#include <ssdebug.h>

#include "rs0types.h"
#include "rs0avacc.h"

void* rs_aval_getwblob(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);

        if (aval->ra_accinfo == NULL) {
            return(NULL);
        } else {
            return(aval->ra_accinfo->ai_wblob);
        }
}

void rs_aval_attachwblob(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        void* wblob,
        void (*wblob_cancel_fun)(void* wblob))
{
        rs_aval_accinfo_t* accinfo;

        CHECK_AVAL(aval);

        accinfo = aval->ra_accinfo;

        if (accinfo == NULL) {
            accinfo = aval->ra_accinfo = SsMemCalloc(1, sizeof(rs_aval_accinfo_t));
        } else if (accinfo->ai_wblob != NULL) {
            (*accinfo->ai_wblob_cancel_fun)(accinfo->ai_wblob);
        }
        accinfo->ai_wblob = wblob;
        accinfo->ai_wblob_cancel_fun = wblob_cancel_fun;
}

void* rs_aval_getrblob(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);

        if (aval->ra_accinfo == NULL) {
            return(NULL);
        } else {
            return(aval->ra_accinfo->ai_rblob);
        }
}

void rs_aval_attachrblob(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        void* rblob,
        void (*rblob_cancel_fun)(void* rblob, bool memif))
{
        rs_aval_accinfo_t* accinfo;
        
        CHECK_AVAL(aval);

        accinfo = aval->ra_accinfo;

        if (accinfo == NULL) {
            accinfo = aval->ra_accinfo = SsMemCalloc(1, sizeof(rs_aval_accinfo_t));
        } else if (accinfo->ai_rblob != NULL) {
            (*accinfo->ai_rblob_cancel_fun)(accinfo->ai_rblob, FALSE);
        }
        accinfo->ai_rblob = rblob;
        accinfo->ai_rblob_cancel_fun = rblob_cancel_fun;
}

void rs_aval_accinfo_free(
        rs_aval_accinfo_t* accinfo)
{
        ss_dassert(accinfo != NULL);

        if (accinfo->ai_wblob != NULL) {
            (*accinfo->ai_wblob_cancel_fun)(accinfo->ai_wblob);
        }
        if (accinfo->ai_rblob != NULL) {
            (*accinfo->ai_rblob_cancel_fun)(accinfo->ai_rblob, FALSE);
        }
        SsMemFree(accinfo);
}
