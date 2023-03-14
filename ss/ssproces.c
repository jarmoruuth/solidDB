/*************************************************************************\
**  source       * ssproces.c
**  directory    * ss
**  description  * Portable process handling functions.
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

#if defined(SS_UNIX)
#  include <unistd.h>
#endif

#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssctype.h"
#include "sschcvt.h"

#include "ssc.h"
#include "ssmem.h"
#include "ssproces.h"

#if defined(SS_DOS) || defined(SS_NT) || defined(SS_UNIX) 

bool SsProcessSetPriority(int priority, int* p_prev_priority)
{
        SS_NOTUSED(priority);
        SS_NOTUSED(p_prev_priority);
        return(TRUE);
}

bool SsProcessSetWorkingSetSize(long working_set)
{
        SS_NOTUSED(working_set);
        return(TRUE);
}

void SsProcessSwitch(void)
{

}

#if defined(SS_UNIX) || defined(SS_NT)

long SsProcessId(void)
{
        return(getpid());
}

#else /* SS_UNIX || SS_NT */


long SsProcessId(void)
{
        return(0L);
}
#endif /* SS_UNIX || SS_NT */

#endif /* DOS, NT, UNIX */

/***********************************************************************\
 ***                                                                 ***
 ***                     WINDOWS                                     ***
 ***                                                                 ***
\***********************************************************************/

#if defined(SS_WIN)

#include "sswindow.h"
#include "ssdebug.h"

bool SsProcessSetPriority(int priority, int* p_prev_priority)
{
        SS_NOTUSED(priority);
        SS_NOTUSED(p_prev_priority);
        return(TRUE);
}

bool SsProcessSetWorkingSetSize(long working_set)
{
        SS_NOTUSED(working_set);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		SsProcessSwitch
 * 
 * Function for applications that do not have an own messageloop.
 * Its main goal is to yield and thus give time for other applications.
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsProcessSwitch(void)
{
        MSG msg;
        bool b = TRUE;

        ss_dprintf_1(("SsProcessSwitch.\n"));
        /* Just Yield */
        b = PeekMessage(&msg, 0, 0, 0, PM_REMOVE);
        if (b) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
#if defined(MSC) && !defined(SS_W32)
            static BOOL bSwitch = TRUE;
            if (bSwitch) {
                _asm mov ax, 0x1689
                _asm mov bl, 1; set mouse busy flag
                _asm int 0x2f
            } else {
                _asm int 0x28
            }
            bSwitch = !bSwitch;
#endif /* defined(MSC) && !defined(SS_W32) */
        }
}

long SsProcessId(void)
{
        return(0L);
}

#endif /* SS_WIN */

/***********************************************************************\
 ***                                                                 ***
 ***                     GENERAL                                     ***
 ***                                                                 ***
\***********************************************************************/

#if defined(SS_NT) || defined(SS_UNIX)

static void make_newcmd(
        char* cmd,
        char** p_cmdname,
        char** p_cmdargs)
{
        char* newcmd;
        char* newargs;
        
        newcmd = SsMemStrdup(cmd);
        newargs = SsMemStrdup(cmd);
        *p_cmdname = newcmd;

        while (*newcmd != '\0' && !ss_isspace((ss_byte_t)*newcmd)) {
            newcmd++;
            newargs++;
        }
        if (*newcmd != '\0') {
            *newcmd = '\0';
            newcmd++;
            newargs--;
            *newargs = '\0';
            newargs++;
            /*
            while (*newcmd != '\0' && !ss_isspace((ss_byte_t)*newcmd)) {
                newcmd++;
            }
            */
            *p_cmdargs = newargs;
        } else {
            *p_cmdargs = NULL;
        }
}

#endif /* SS_NT or SS_UNIX */

/*##**********************************************************************\
 * 
 *		SsSystem
 * 
 * Replacement for C-libary system() function. Executes the given
 * command line.
 * 
 * Parameters : 
 * 
 *	cmd - 
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
bool SsSystem(char* cmd)
{
#if defined(SS_NT) 

#ifdef MSVC7_NT
        uintptr_t status;
#else
        int status;
#endif
        char* cmdname;
        char* cmdargs;

        make_newcmd(cmd, &cmdname, &cmdargs);

        status = spawnlp(P_NOWAIT, cmdname, cmdargs, NULL);

        SsMemFree(cmdname);

        return(status == 0);

#elif defined(SS_UNIX)

        int pid;
        char* cmdname;
        char* cmdargs;

        make_newcmd(cmd, &cmdname, &cmdargs);

        pid = fork();

        if (pid == 0) {
            execlp(cmdname, cmdargs, NULL);
            _exit (127);
        } else {
            SsMemFree(cmdname);
            SsMemFree(cmdargs);
        }

        return(pid > 0);

#else
        SS_NOTUSED(cmd);

        return(FALSE);

#endif
}


bool SsSystemSync(char* cmd)
{
#if defined(SS_NT) 

#ifdef MSVC7_NT
        intptr_t pid;
#else
        int pid;
#endif
        int status;
        char* cmdname;
        char* cmdargs;

        make_newcmd(cmd, &cmdname, &cmdargs);

        pid = spawnlp(P_NOWAIT, cmdname, cmdargs, NULL);

        SsMemFree(cmdname);
        cwait(&status, pid, WAIT_CHILD);
        return(status == 0);

#elif defined(SS_UNIX) 

        int pid;
        char* cmdname;
        char* cmdargs;

        make_newcmd(cmd, &cmdname, &cmdargs);

        pid = fork();

        if (pid == 0) {
            execlp(cmdname, cmdargs, NULL);
            _exit (127);
        } else {
            SsMemFree(cmdname);
            SsMemFree(cmdargs);
        }

        return(pid > 0);

#else
        SS_NOTUSED(cmd);

        return(FALSE);

#endif
}
