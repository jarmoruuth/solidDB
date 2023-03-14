/*************************************************************************\
**  source       * ui0srvsf.c
**  directory    * ui
**  description  * Server user interface dummy functions
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

#define SS_WINNT_NEEDSGDI
#include <sswindow.h>

#include <ssstdlib.h>
#include <ssproces.h>

#include <ssc.h>
#include <ssdebug.h>
#include <sssprint.h>
#include <ssservic.h>

#include "ui0srv.h"
#include "ui0msg.h"

#if defined(SS_WIN) || defined(SS_NT)

#include <sswindow.h>		/* required for all Windows applications */
#include <sswinint.h>

extern SS_HINST_TYPE SshInst;
extern HANDLE SshPrevInst;

#endif

#include <ssstdio.h>
#include <ssstring.h>
#include <sssprint.h>
#include <ssthread.h>


#include <ssmem.h>
#include <ssmem.h>

#include <su0vers.h>
#ifdef SS_LICENSEINFO_V3
#include <su0li3.h>
#else /* SS_LICENSEINFO_V3 */
#include <su0li2.h>
#endif /* SS_LICENSEINFO_V3 */

#include "ui1msg.h"

bool ui_srv_init(
        char* srvname,
        bool hide_icon,
        bool (*normal_shutdown_fp)(void),
        bool (*quick_shutdown_fp)(void),
        uint (*usercount_fp)(void))
{
        SS_NOTUSED(srvname);
        SS_NOTUSED(normal_shutdown_fp);
        SS_NOTUSED(quick_shutdown_fp);
        SS_NOTUSED(usercount_fp);
        return(TRUE);
}

void ui_srv_done(void)
{
}

void ui_srv_startup(void)
{
}

void ui_srv_setconfirmshutdown_fp(
        bool (*confirmshutdown_fp)(void))
{
        SS_NOTUSED(confirmshutdown_fp);
}

void ui_srv_setinfo_fp(
        char* (*info_fp)(void))
{
        SS_NOTUSED(info_fp);
}

bool ui_srv_isgui(void)
{
        return(FALSE);
}
