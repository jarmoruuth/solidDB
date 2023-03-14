/*************************************************************************\
**  source       * sssprint.c
**  directory    * ss
**  description  * sprintf replacement.
**               * 
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

Portable replacement for sprintf() and vsprintf(). Floating point
formats are not supported in all environments (like MS Windows).

Limitations:
-----------

Floating point formats are mot supported in all environments
(like MS Windows).

Error handling:
--------------

None.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

None.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include "sswindow.h"    /* for wvsprintf */

#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssstdarg.h"

#include "sssprint.h"

/*##**********************************************************************\
 * 
 *		SsVsprintf
 * 
 * Replacement for vsprintf function.
 * 
 *      NOTE: In Microsoft Windows DLL, floating point formats are not
 *      supported. Use SsDoubleToAscii or SsDoubleToAsciiE.
 * 
 * Parameters : 
 * 
 *      str - out, use
 *		output buffer
 *
 *	format - in, use
 *		printf style format string
 *
 *	ap - in, use
 *		pointer to the argument list
 *
 * Return value :
 * 
 *      number of bytes written
 * 
 * Limitations  : 
 * 
 *      Floating point formats are not supported in Microsoft Windows DLL.
 * 
 * Globals used : none
 */
int SsVsprintf(char* str, char* format, va_list ap)
{
#ifdef WINDOWS
        return(wvsprintf(str, format, ap));
#else
        return(vsprintf(str, format, ap));
#endif
}

/*##**********************************************************************\
 * 
 *		SsSprintf
 * 
 * Replacement for sprintf function.
 * 
 *      NOTE: In Microsoft Windows DLL, floating point formats are not
 *      supported. Use SsDoubleToAscii or SsDoubleToAsciiE.
 * 
 * Parameters :
 * 
 *      str - out, use
 *		output buffer
 *
 *      format - in, use
 *		printf style format string
 *
 *      ... - in, use
 *          variable number of arguments for the format string
 * 
 * Return value : 
 * 
 *      number of bytes written
 * 
 * Limitations  : 
 * 
 *      Floating point formats are not supported in Microsoft Windows DLL.
 * 
 * Globals used :
 */
int SS_CDECL SsSprintf(char* str, const char* format, ...)
{
        int retval;
        va_list ap;

        va_start(ap, format);
        retval = SsVsprintf(str, (char *)format, ap);
        va_end(ap);

        return(retval);
}
