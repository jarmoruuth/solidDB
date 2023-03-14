/*************************************************************************\
**  source       * ssrtcov.h
**  directory    * ss
**  description  * Runtime coverage monitoring routines.
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


#ifndef SSRTCOV_H
#define SSRTCOV_H

#include "ssc.h"
#include "ssfake.h"

#ifdef SS_FAKE
#define SS_RTCOVERAGE
#endif

/* Index values for different coverage entities.
 *
 * WARNING! When adding new values, all files referencing to ss_rtcoverage
 *          variable must be recompiled. This is because all data is
 *          in static tables, and the table positions change when
 *          SS_PMON_MAXVALUES changes.
 *
 * ADD CORRECT MAKEFILE DEPENDENCIES TO THIS HEADER FROM FILES USING RTCOV.
 */
typedef enum {
      SS_RTCOV_TEST                             = 0,
      SS_RTCOV_HSBG2_1SAFE,                     /* 1. basic in dbe0trx that 1-safe is enabled/covered */
      SS_RTCOV_FLOW_FORKEY_ISDISABLED,          /* 2. in dbe-level that for-key-check is disabled     */
      SS_RTCOV_RPROC_TRANSACTIVE,               /* 3. transaction is active during remote procedure call */
      SS_RTCOV_RPROC_TRANSNOTACTIVE,            /* 4. transaction is active during remote procedure call */
      SS_RTCOV_RPROC_TRANSEND,                  /* 5. remote proc call ends transaction  */
      
      SS_RTCOV_TRX_NOFLUSH,                     /* 6. Relaxed trx:no flush to log */
      SS_RTCOV_TRX_PRIPHASE1_NOFLUSH,           /* 7. HSBG2 primary phase1 commit:no flush to log */ 
      SS_RTCOV_TRX_PRIPHASE2_FLUSH,             /* 8. HSBG2 primary phase2 commit:flush to log    */ 
      SS_RTCOV_TRX_AFTERCOMMIT_CONT,            /* 9. trx aftercommit function returns cont       */ 

      SS_RTCOV_TRUNCATE_BLOBS,                  /* 10. BLOBs during truncate */ 
      SS_RTCOV_TRUNCATE_RECOVBLOBS,             /* 11. BLOBs during truncate recovery */ 
      SS_RTCOV_TRUNCATE_DROPINDEX_STEP,         /* 12. Drop index step called with truncate. */ 

      SS_RTCOV_FAKE_PAUSE,                      /* 13. To check if ss_fake_pause has paused */

      SS_RTCOV_MERGE_STEP,                      /* 14. Merge step executed. */ 
      SS_RTCOV_INDEX_PHYSDEL_BONSAI,            /* 15. Physical delete from Bonsai-tree. */
      SS_RTCOV_INDEX_PHYSDEL_PERM,              /* 16. Physical delete from permanent tree. */
      SS_RTCOV_DB_MERGE_DISABLED,               /* 17. Merge disabled. */

      SS_RTCOV_LAST_FAKE_CODE,                  /* 18. Merge disabled. */

      SS_RTCOV_DBE_DB_INIT,                     /* 19. number of dbe_init calls without dbe_done call */
      
      SS_RTCOV_FLOW_REMOVEREADLEVEL,            /* 20. Flow has released a read level */
      SS_RTCOV_DBE_REMOVEREADLEVEL,             /* 21. DBE has released a read level */
      
      SS_RTCOV_TC_USE_PRIMARY,                  /* 22. stmt prepare tells that execute this on primary */

      SS_RTCOV_MAXVALUES
} ss_rtcoverage_val_t;

#ifdef SS_RTCOVERAGE

#define SS_RTCOVERAGE_INC(v)         ss_rtcoverage.cov_values[v]++
#define SS_RTCOVERAGE_INCIF(v, b)    {if (b) {ss_rtcoverage.cov_values[v]++;}}
#define SS_RTCOVERAGE_DEC(v)         ss_rtcoverage.cov_values[v]--
#define SS_RTCOVERAGE_GET(v)         ss_rtcoverage.cov_values[v]
#define SS_RTCOVERAGE_SET(v,value)   ss_rtcoverage.cov_values[v] = value

typedef struct {
        ulong cov_values[SS_RTCOV_MAXVALUES];
        void* to_use_later;
} ss_rtcoverage_t;

extern ss_rtcoverage_t ss_rtcoverage;

#else /* SS_RTCOVERAGE */

#define SS_RTCOVERAGE_INC(v)
#define SS_RTCOVERAGE_INCIF(v, b)
#define SS_RTCOVERAGE_DEC(v)
#define SS_RTCOVERAGE_GET(v)
#define SS_RTCOVERAGE_SET(v,value)

#endif /* SS_RTCOVERAGE */

void SsRtCovInit(void);
void SsRtCovClear(void);

#endif /* SSRTCOV_H */
