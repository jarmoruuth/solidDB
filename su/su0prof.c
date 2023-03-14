/*************************************************************************\
**  source       * su0prof.c
**  directory    * su
**  description  * Execution time profiling.
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

#include <ssstdio.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssprint.h>

#include "su0time.h"
#include "su0prof.h"

/*##**********************************************************************\
 * 
 *		su_profile_stopfunc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	func - 
 *		
 *		
 *	timer - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_profile_stopfunc(const char* func, su_timer_t* timer)
{
        double d;

        d = su_timer_readf(timer);

        if (d > ss_profile_limit) {
            int i;
            double div;
            char buf[10];
            for (i = 0, div = 2.0; i < 8 && d / div > ss_profile_limit; i++, div *= 2.0) {
                buf[i] = '*';
            }
            if (i > 0) {
                buf[i++] = ' ';
            }
            buf[i] = '\0';

            SsDbgPrintf("PROF:%s %.3lf secs:%.512s %s\n", buf, d, (char *)func, buf);
        }
}

