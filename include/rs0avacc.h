/*************************************************************************\
**  source       * rs0avacc.h
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


#ifndef RS0AVACC_H
#define RS0AVACC_H

#include "rs0sysi.h"
#include "rs0atype.h"
#include "rs0aval.h"

SS_INLINE bool rs_aval_isinitialized(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

SS_INLINE void rs_aval_setinitialized(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool isinit);

void* rs_aval_getwblob(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_attachwblob(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        void* rblob,
        void (*wblob_cancel_fun)(void* wblob));

void* rs_aval_getrblob(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_attachrblob(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        void* rblob,
        void (*rblob_cancel_fun)(void* rblob, bool memif));

SS_INLINE void rs_aval_clearblobs(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

struct rs_aval_accinfo_st {
        void* ai_wblob;
        void  (*ai_wblob_cancel_fun)(void* wblob);
        void* ai_rblob;
        void  (*ai_rblob_cancel_fun)(void* wblob, bool memif);
}; /* rs_aval_accinfo_t */

#if defined(RS0AVACC_C) || defined(SS_USE_INLINE)

SS_INLINE bool rs_aval_isinitialized(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval)
{
        CHECK_AVAL(aval);

        return(SU_BFLAG_TEST(aval->ra_flags, RA_ACCINIT));
}

SS_INLINE void rs_aval_setinitialized(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        bool isinit)
{
        CHECK_AVAL(aval);

        if (isinit) {
            SU_BFLAG_SET(aval->ra_flags, RA_ACCINIT);
        } else {
            SU_BFLAG_CLEAR(aval->ra_flags, RA_ACCINIT);
        }
}

SS_INLINE void rs_aval_clearblobs(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval)
{
        rs_aval_accinfo_t* accinfo;

        CHECK_AVAL(aval);
#if 0   /* accinfo can be NULL, when called. tommiv 03-jan-2003 */
        ss_dassert(aval->ra_accinfo != NULL);
#endif
        accinfo = aval->ra_accinfo;

        if (accinfo != NULL) {
            if (accinfo->ai_wblob != NULL) {
                (*accinfo->ai_wblob_cancel_fun)(accinfo->ai_wblob);
                accinfo->ai_wblob = NULL;
            }
            if (accinfo->ai_rblob != NULL) {
                (*accinfo->ai_rblob_cancel_fun)(accinfo->ai_rblob, FALSE);
                accinfo->ai_rblob = NULL;
            }
        }
}

#endif /* defined(RS0AVACC_C) || defined(SS_USE_INLINE) */

#endif /* RS0AVACC_H */
