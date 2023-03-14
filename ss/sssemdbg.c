/*************************************************************************\
**  source       * sssemdbg.c
**  directory    * ss
**  description  * Semaphore debug functions.
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

#include "ssenv.h"
#include "ssc.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "sssem.h"

#ifdef SS_SEM_DBG

bool ss_semdbg_uselibsem = FALSE;

static SsSemDbgT** sd_list = NULL;

SsSemDbgT* SsSemDbgAddVoidChecksIf(
        ss_semnum_t semnum,
        char* file,
        int line,
        bool gatep,
        bool void_checks)
{
        SsSemDbgT* sd;

        if (ss_semdebug) {
            if (semnum < 0) {
                semnum = 0;
            }

            if (ss_semdbg_uselibsem) {
                SsSemEnter(ss_lib_sem);
            }
            if (sd_list == NULL) {
                sd_list = calloc(SS_SEMNUM_LAST, sizeof(SsSemDbgT*));
                ss_assert(sd_list != NULL);
            }
            sd = sd_list[semnum];
            if (sd == NULL) {
                sd = malloc(sizeof(SsSemDbgT));
                ss_assert(sd != NULL);

                sd->sd_callcnt = 0;
                sd->sd_waitcnt = 0;
                sd->sd_file = file;
                sd->sd_line = line;
                sd->sd_gatep = gatep;

                sd_list[semnum] = sd;
            }
            if (ss_semdbg_uselibsem) {
                SsSemExit(ss_lib_sem);
            }

            return(sd);
        } else {
            return(NULL);
        }
}

SsSemDbgT* SsSemDbgAdd(
        ss_semnum_t semnum,
        char* file,
        int line,
        bool gatep)
{
        SsSemDbgT* semdbg;

        semdbg = SsSemDbgAddVoidChecksIf(semnum, file, line, gatep, FALSE);
        return (semdbg);
}

static void SemDbgPrintList(
        SS_FILE* fp,
        bool freep,
        bool formatted)
{
        SsSemDbgT* sd;
        char* stars[5];

        stars[0] = "";
        stars[1] = "*";
        stars[2] = "**";
        stars[3] = "***";
        stars[4] = "****";

        if (sd_list != NULL) {
            int i;
            char* fmt;
            if (formatted) {
                fmt = "Id,File,Line,CallCnt,WaitCnt,WaitPrc,Type,Critical\n";
            } else {
                fmt = "Id:   File:         Line: CallCnt: WaitCnt: WaitPrc:\n";
            }
            SsFprintf(fp, fmt);
            for (i = 0; i < SS_SEMNUM_LAST; i++) {
                sd = sd_list[i];
                if (sd != NULL) {
                    long waitprc_long;
                    waitprc_long = sd->sd_callcnt == 0
                                ? 0L
                                : (long)(sd->sd_waitcnt * 100.0 / sd->sd_callcnt);
                    if (formatted) {
                        fmt = "%d,%s,%d,%lu,%lu,%lu,%s,%s\n";
                    } else {
                        fmt = "%5d %-13s %5d %8lu %8lu %7lu %s %s\n";
                    }
                    SsFprintf(fp, fmt,
                        i,
                        sd->sd_file,
                        sd->sd_line,
                        sd->sd_callcnt,
                        sd->sd_waitcnt,
                        waitprc_long,
                        sd->sd_gatep ? "gate" : "",
                        stars[SS_MIN(4, waitprc_long / 10)]);
                    if (freep) {
                        free(sd);
                    }
                }
            }
            if (freep) {
                free(sd_list);
                sd_list = NULL;
            }
        }
}

void SsSemDbgPrintList(
        SS_FILE* fp,
        bool freep)
{
        SemDbgPrintList(fp, freep, FALSE);
}

void SsSemDbgPrintFormattedList(
        SS_FILE* fp,
        bool freep)
{
        SemDbgPrintList(fp, freep, TRUE);
}

SsSemDbgT** SsGetSemList(void)
{
        return (sd_list);
}

#endif /* SS_SEM_DBG */
