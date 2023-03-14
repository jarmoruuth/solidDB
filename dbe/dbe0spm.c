/*************************************************************************\
**  source       * dbe0spm.c
**  directory    * dbe
**  description  * Space maneger for HSB.
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

#include <ssc.h>
#include <ssdebug.h>
#include <sssem.h>
#include <ssmem.h>
#include <sspmon.h>

#include <su0mesl.h>
#include <su0prof.h>

#include "dbe9type.h"
#include "dbe0spm.h"

#define CHK_SPM(s)  ss_dassert((s)->spm_chk == DBE_CHK_SPM)

struct dbe_spm_st {
        ss_debug(dbe_chk_t  spm_chk;)
        int                 spm_maxsize;
        int                 spm_freesize;
        su_meslist_t        spm_meslist;
        su_meswaitlist_t*   spm_meswaitlist;
        SsSemT*             spm_sem;
        bool                spm_active;
        int                 spm_freespacerate;
        bool                spm_catchup;
};

dbe_spm_t* dbe_spm_init(int maxsize, bool catchup)
{
        dbe_spm_t* spm;

        spm = SSMEM_NEW(dbe_spm_t);

        ss_debug(spm->spm_chk = DBE_CHK_SPM);
        spm->spm_maxsize = maxsize;
        spm->spm_freesize = maxsize;
        spm->spm_sem = SsSemCreateLocal(SS_SEMNUM_DBE_SPM);
        spm->spm_active = FALSE;
        spm->spm_freespacerate = 0;
        spm->spm_catchup = catchup;

        su_meslist_init_nomutex(&spm->spm_meslist);
        spm->spm_meswaitlist = su_meswaitlist_init();

        ss_dprintf_1(("dbe_spm_init:%ld:maxsize=%d\n", (long)spm, maxsize));

#ifndef SS_MYSQL
        if (spm->spm_catchup) {
            SS_PMON_SET(SS_PMON_HSB_CATCHUPSPM_FREESPACE, spm->spm_freesize);
        } else {
            SS_PMON_ADD(SS_PMON_HSB_LOGSPM_REQCOUNT);
        }
#endif /* !SS_MYSQL */
        return(spm);
}

void dbe_spm_done(dbe_spm_t* spm)
{
        ss_dprintf_1(("dbe_spm_done:%ld\n", (long)spm));
        CHK_SPM(spm);

        su_meswaitlist_done(spm->spm_meswaitlist);
        su_meslist_done(&spm->spm_meslist);
        SsSemFree(spm->spm_sem);
        SsMemFree(spm);
}

int dbe_spm_getmaxsize(dbe_spm_t* spm)
{
        CHK_SPM(spm);

        return(spm->spm_maxsize);
}

/*##**********************************************************************\
 * 
 *		dbe_spm_setactive
 * 
 * Enables/disables space manager. 
 * Spacemanages need to be disabled separately for some cases 
 * where we must wait transactions to end before actual state change. Then 
 * we disable space manager to ensure that transaction can continue to the
 * end.
 * 
 * Parameters : 
 * 
 *		spm - 
 *		active - 
 *			
 *			
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_spm_setactive(
        dbe_spm_t* spm,
        bool active)
{
        CHK_SPM(spm);

        SsSemEnter(spm->spm_sem);

        ss_dprintf_1(("dbe_spm_setactive:%ld:active=%d\n", (long)spm, active));

        if (spm->spm_active && active) {
            ss_dprintf_2(("dbe_spm_setactive:%ld:already active, do nothing\n", (long)spm));
            ss_dassert(!spm->spm_catchup);
            SsSemExit(spm->spm_sem);
            return;
        }

        spm->spm_active = active;
        spm->spm_freesize = spm->spm_maxsize;
        if (!active) {
            /* disable */
            su_meswaitlist_wakeupall(spm->spm_meswaitlist);
        } else {
            /* enable */
            ss_dassert(su_meswaitlist_isempty(spm->spm_meswaitlist));
        }

#ifndef SS_MYSQL
        if (spm->spm_catchup) {
            SS_PMON_SET(SS_PMON_HSB_CATCHUPSPM_FREESPACE, spm->spm_freesize);
        } else {
            SS_PMON_ADD(SS_PMON_HSB_LOGSPM_REQCOUNT);
        }
#endif /* !SS_MYSQL */

        SsSemExit(spm->spm_sem);
}

void dbe_spm_setfreespacerate(
        dbe_spm_t* spm,
        int ratepercentage)
{
        CHK_SPM(spm);

        ss_dprintf_1(("dbe_spm_setfreespacerate:%ld:ratepercentage=%d\n", (long)spm, ratepercentage));

        spm->spm_freespacerate = ratepercentage;
}

void dbe_spm_addspace(
        dbe_spm_t* spm,
        int size)
{
        CHK_SPM(spm);

        SsSemEnter(spm->spm_sem);

        ss_dprintf_1(("dbe_spm_addspace:%ld:size=%d, freesize=%d, catchup %d\n", (long)spm, size, spm->spm_freesize, spm->spm_catchup));

        if (size <= spm->spm_maxsize) {
            spm->spm_freesize = spm->spm_freesize + size;

            if (spm->spm_freesize >= spm->spm_maxsize) {
                spm->spm_freesize = spm->spm_maxsize;
            }

            if (spm->spm_freespacerate > 0) {
                ss_dprintf_2(("dbe_spm_addspace:%ld:spm->spm_freespacerate=%d, original spm->spm_freesize=%d\n", (long)spm, spm->spm_freespacerate, spm->spm_freesize));
                spm->spm_freesize = spm->spm_freesize * spm->spm_freespacerate / 100;
                if (spm->spm_freesize == 0) {
                    spm->spm_freesize = 1;
                }
            }
            ss_dprintf_2(("dbe_spm_addspace:%ld:new spm->spm_freesize=%d\n", (long)spm, spm->spm_freesize));
        }
        while (!su_meswaitlist_isempty(spm->spm_meswaitlist) && spm->spm_freesize > 0) {
            ss_dprintf_2(("dbe_spm_addspace:%ld:wakeup1st\n", (long)spm));
            su_meswaitlist_wakeup1st(spm->spm_meswaitlist);
            if (spm->spm_active) {
                spm->spm_freesize--;
            }
        }

#ifndef SS_MYSQL
        if (spm->spm_catchup) {
            SS_PMON_SET(SS_PMON_HSB_CATCHUPSPM_FREESPACE, spm->spm_freesize);
        } else {
            SS_PMON_ADD(SS_PMON_HSB_LOGSPM_REQCOUNT);
        }
#endif /* !SS_MYSQL */

        SsSemExit(spm->spm_sem);
}

void dbe_spm_getspace(
        dbe_spm_t* spm,
        int size)
{
        bool waitp;
        su_profile_timer;

        CHK_SPM(spm);

        if (!spm->spm_active) {
            return;
        }

        SsSemEnter(spm->spm_sem);

        ss_dprintf_1(("dbe_spm_getspace:%ld:size=%d, freesize=%d, catchup %d\n", (long)spm, size, spm->spm_freesize, spm->spm_catchup));
        su_profile_start;

        if (spm->spm_freesize >= size) {
            /* There is enough space for this request. */
            spm->spm_freesize -= size;
            ss_dprintf_2(("dbe_spm_getspace:%ld:there is free space, new freesize=%d\n", (long)spm, spm->spm_freesize));
            waitp = FALSE;

        } else if (size > 1 && spm->spm_freesize > 0) {
            /* There is some space but not enough. Use all space and continue. */
            ss_dprintf_2(("dbe_spm_getspace:%ld:some free space but not enough, take all, size=%d\n", (long)spm, size));
            spm->spm_freesize = 0;
            waitp = FALSE;

        } else {
            waitp = TRUE;
        }

#ifndef SS_MYSQL
        if (spm->spm_catchup) {
            SS_PMON_ADD(SS_PMON_HSB_CATCHUPSPM_REQCOUNT);
            SS_PMON_SET(SS_PMON_HSB_CATCHUPSPM_FREESPACE, spm->spm_freesize);
        } else {
            SS_PMON_ADD(SS_PMON_HSB_LOGSPM_REQCOUNT);
            SS_PMON_SET(SS_PMON_HSB_LOGSPM_FREESPACE, spm->spm_freesize);
        }
#endif /* !SS_MYSQL */

        if (waitp) {
            su_mes_t* mes;
            ss_dprintf_2(("dbe_spm_getspace:freesize %ld:wait...\n", (long)spm, spm->spm_freesize));
            mes = su_meslist_mesinit(&spm->spm_meslist);
            su_meswaitlist_add(spm->spm_meswaitlist, mes);

#ifndef SS_MYSQL
            if (spm->spm_catchup) {
                SS_PMON_ADD(SS_PMON_HSB_CATCHUPSPM_WAITCOUNT);
            } else {
                SS_PMON_ADD(SS_PMON_HSB_LOGSPM_WAITCOUNT);
            }
#endif /* !SS_MYSQL */

            SsSemExit(spm->spm_sem);

            su_mes_wait(mes);

            SsSemEnter(spm->spm_sem);

            ss_dprintf_2(("dbe_spm_getspace:%ld:wakeup\n", (long)spm));

            su_meslist_mesdone(&spm->spm_meslist, mes);
        }
        su_profile_stop("dbe_spm_getspace");

        SsSemExit(spm->spm_sem);
}

