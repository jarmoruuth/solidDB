/*************************************************************************\
**  source       * sstraph2.c
**  directory    * ss
**  description  * Server addition portion of sstraph.c
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

#include "ssdebug.h"
#include "ssmem.h"
#include "ssthread.h"
#include "sstraph.h"
#include "ssstdarg.h"
#include "ssstring.h"
#include "sssignal.h"
#include "ssmath.h"

#if defined(SS_NT) 

    typedef struct _exception SsMathExceptionT;
#   define SsMatherr _matherr
#   define SsMatherrGet(p) (((SsMathExceptionT*)(p))->type)

/* ---------------------------------------------------------------------- */
#elif defined(SS_PTHREAD) || defined(SS_LINUX)

    typedef struct exception SsMathExceptionT;
# if defined(SS_LINUX) && defined(SS_DLL)
#   undef  SsMatherr
# else
#   define SsMatherr matherr
# endif
#   define SsMatherrGet(p) (((SsMathExceptionT*)(p))->type)

/* ---------------------------------------------------------------------- */
#else /* Other than SS_NT or SS_PTHREAD */

#   if defined(SS_SOLARIS)
        typedef struct exception SsMathExceptionT;
#       define SsMatherrGet(p) (((SsMathExceptionT*)(p))->type)
#       if !defined(WCC)
#           define SsMatherr _matherr
#       else
#           define SsMatherr matherr
#       endif

#   elif defined (WCC) && (defined(SS_W32) || defined(SS_W16) || defined(SS_DOS))
#       define SsMathExceptionT struct exception 
#       define SsMatherrGet(p)  (((SsMathExceptionT*)(p))->type)
#       define SsMatherr        matherr

#   elif defined(SS_WIN)
        typedef struct _exception SsMathExceptionT;
#       define SsMatherrGet(p) (((SsMathExceptionT*)(p))->type)
#       define SsMatherr matherr

#   else /* Not SS_WIN */
        typedef int SsMathExceptionT;
#       define SsMatherrGet(p) (*(SsMathExceptionT*)(p))
#       define SsMatherr matherr
#   endif

#endif /* SS_NT */

/*##**********************************************************************\
 * 
 *		SsMatherr
 * 
 * Custom math error handler
 * 
 * Parameters : 
 * 
 *	p - in
 *		exception context
 *		
 * Return value : none really
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#if !defined(SS_DOS4GW) && \
    !defined(SS_SCO) && !defined(SS_FREEBSD) && \
    !defined(SS_BSI) 

int SS_CLIBCALLBACK SsMatherr(SsMathExceptionT* p)
{
        ss_dprintf_1(("matherr\n"));
        ss_trap_raise(SS_TRAP_FPE);
        SS_NOTUSED(p);
        return (0); /* not reached */
}

#endif /* ... */

/*##**********************************************************************\
 * 
 *		SsMatherrLink
 * 
 * Makes the server to link to correct version of matherr.
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
void SsMatherrLink(void)
{

}
