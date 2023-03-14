/*************************************************************************\
**  source       * dbe6gobj.c
**  directory    * dbe
**  description  * Global objects.
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

#include <ssenv.h>
#include <ssc.h>
#include <ssdebug.h>
#include <sssem.h>
#include <ssmem.h>
#include <ssthread.h>

#include <rs0admev.h>

#include "dbe0db.h"
#include "dbe6gobj.h"

dbe_gobj_t* dbe_gobj_init(void)
{
        dbe_gobj_t* go;

        go = SsMemCalloc(sizeof(dbe_gobj_t), 1);

        go->go_mergest = DBE_GOBJ_MERGE_NONE;
        go->go_sem = SsSemCreateLocal(SS_SEMNUM_DBE_GOBJ);

        return(go);
}

void dbe_gobj_done(dbe_gobj_t* go)
{
        SsSemFree(go->go_sem);
        SsMemFree(go);
}

static void gobj_removemergewrites(
        dbe_gobj_t* go,
        long nkeyremoved,
        long nmergeremoved)
{
        if (go->go_nindexwrites < nkeyremoved) {
            go->go_nindexwrites = 0;
        } else {
            go->go_nindexwrites -= nkeyremoved;
        }
        if (go->go_nmergewrites < nmergeremoved) {
            go->go_nmergewrites = 0;
        } else {
            go->go_nmergewrites -= nmergeremoved;
        }
}

void dbe_gobj_mergestart(
        dbe_gobj_t* go,
        dbe_trxnum_t mergetrxnum,
        bool full_merge)
{

        SsSemEnter(go->go_sem);

        if (full_merge || !dbe_cfg_mergecleanup) {
            ss_dassert(go->go_mergest == DBE_GOBJ_MERGE_NONE);
            go->go_mergest = DBE_GOBJ_MERGE_STARTED;
        }

        if (full_merge) {
            go->go_mergerounds++;
        } else {
            go->go_quickmergerounds++;
        }

        if (full_merge) {
            go->go_mergetrxnum = mergetrxnum;
        }

        SsSemExit(go->go_sem);
}

void dbe_gobj_mergestop(
        dbe_gobj_t* go)
{
        SsSemEnter(go->go_sem);

        ss_dassert(go->go_mergest == DBE_GOBJ_MERGE_STARTED);

        go->go_mergest = DBE_GOBJ_MERGE_NONE;
        go->go_mergetrxnum = DBE_TRXNUM_NULL;

        SsSemExit(go->go_sem);
}

void dbe_gobj_mergeupdate(
        dbe_gobj_t* go,
        long nkeyremoved,
        long nmergeremoved)
{
        SsSemEnter(go->go_sem);

        gobj_removemergewrites(go, nkeyremoved, nmergeremoved);

        SsSemExit(go->go_sem);
}

void dbe_gobj_quickmergeupdate(
        dbe_gobj_t* go,
        long nstmtremoved)
{
        SsSemEnter(go->go_sem);

        if (nstmtremoved > go->go_trxstat.ts_quickmergelimitcnt) {
            go->go_trxstat.ts_quickmergelimitcnt = 0;
        } else {
            go->go_trxstat.ts_quickmergelimitcnt -= nstmtremoved;
        }

        SsSemExit(go->go_sem);
}

void dbe_gobj_addmergewrites(
        dbe_gobj_t* go,
        long nmergewrites)
{
        SsSemEnter(go->go_sem);

        go->go_nmergewrites += nmergewrites;

        SsSemExit(go->go_sem);
}

static void gobj_addindexwrites_nomutex(
        dbe_gobj_t* go,
        rs_sysi_t* cd __attribute__ ((unused)),
        long nindexwrites)
{
#ifndef SS_MYSQL
        if (go->go_nindexwrites <= 0 && nindexwrites > 0) {

            rs_eventnotifiers_call(
                    cd,
                    "SYS_EVENT_ROWS2MERGE",
                    NULL, FALSE,
                    go->go_nindexwrites + nindexwrites, TRUE,
                    -1, FALSE);
            /* dbe_db_notify(go->go_db, RS_ADMEVENT_ROWS2MERGE, go->go_nindexwrites + nindexwrites); */
        }
#endif /* !SS_MYSQL */
        go->go_nindexwrites += nindexwrites;
        go->go_ntotindexwrites += nindexwrites;
}

void dbe_gobj_addindexwrites(
        dbe_gobj_t* go,
        rs_sysi_t* cd,
        long nindexwrites)
{
        SsSemEnter(go->go_sem);

        gobj_addindexwrites_nomutex(go, cd, nindexwrites);

        SsSemExit(go->go_sem);
}


void dbe_gobj_addlogwrites(
        dbe_gobj_t* go,
        long nlogwrites)
{
        SsSemEnter(go->go_sem);

        go->go_nlogwrites += nlogwrites;
        go->go_ntotlogwrites += nlogwrites;

        SsSemExit(go->go_sem);
}

void dbe_gobj_addtrxstat(
        dbe_gobj_t* go,
        rs_sysi_t* cd,
        dbe_db_trxtype_t trxtype,
        bool count_it,
        bool read_only,
        long stmtcnt,
        long nindexwrites,
        long nlogwrites)
{
        SsSemEnter(go->go_sem);

        ss_dprintf_1(("dbe_gobj_addtrxstat:trxtype=%d, read_only=%d, stmtcount=%ld\n", trxtype, read_only, stmtcnt));

        if (count_it) {
            switch (trxtype) {
                case DBE_DB_TRXTYPE_COMMIT:
                    go->go_trxstat.ts_commitcnt++;
                    break;
                case DBE_DB_TRXTYPE_ABORT:
                    go->go_trxstat.ts_abortcnt++;
                    break;
                case DBE_DB_TRXTYPE_ROLLBACK:
                    go->go_trxstat.ts_rollbackcnt++;
                    break;
            }
        }

        if (read_only) {
            go->go_trxstat.ts_readonlycnt++;
        } else {
            go->go_trxstat.ts_stmtcnt += stmtcnt;
            go->go_trxstat.ts_quickmergelimitcnt += stmtcnt;
        }

        if (nindexwrites) {
            gobj_addindexwrites_nomutex(go, cd, nindexwrites);
        }

        if (nlogwrites) {
            go->go_nlogwrites += nlogwrites;
            go->go_ntotlogwrites += nlogwrites;
        }

        SsSemExit(go->go_sem);
}
