/*************************************************************************\
**  source       * ssver.c
**  directory    * ss
**  description  * Server version number and name information.
**               * NOTE! Files ssver.c and sssncver.c are equivalent.
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

/************************************************************************\
 *
 *      SERVER_VERSION
 *
 * Primary place for solidDB version number in now in version.inc
 * Major and minor version, version string etc are calculated from these
 * components passed in compiler command line.
 * For MySQL we expand the version number macros.
 */
#if defined(SS_MYSQL) && !defined(SS_SFVERS_MAJOR)
static int   vers_major = 6;
static int   vers_minor = 1;
static int   vers_release = 80;
static int   vers_dll = SS_VERS_DLL;
#else
static int   vers_major = SS_SFVERS_MAJOR;
static int   vers_minor = SS_SFVERS_MINOR;
static int   vers_release = SS_SFVERS_RELEASE;
static int   vers_dll = SS_VERS_DLL;
#endif
static char* vers_dllpostfix = (char *)"";

/* NOTE! Name is hard coded in ssver.c, sssncver.c, makefile.inc, cli1util.h */
#ifdef SS_MYSQL
#ifdef MYSQL_DYNAMIC_PLUGIN
static char* vers_name = (char *)"solidDB Storage Engine Beta for MySQL";
#else
static char* vers_name = (char *)"solidDB for MySQL";
#endif
#else
static char* vers_name = (char *)"Solid Embedded Engine with Accelerator option EE";
#endif

/*##**********************************************************************\
 * 
 *		ss_versionnumber
 * 
 * Returns solid major and minor version stored in one integer.
 * The minor version is stored in lowest byte, major in next one
 * 
 * For instance, version 02.30 --> 0x021E
 * 
 * This was changed to function to avoid the need of changing more than
 * one code location when version number changes.
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 *      This function returns the value of SS_SERVER_VERSNUM (ssenv.h)
 * 
 * Globals used : 
 * 
 * See also : 
 */
int ss_versionnumber(void)
{
        int major;
        int minor;
        int versno;

        major = vers_major;
        minor = vers_minor;

        versno = (major << 8) | (minor & 0x00FF);

        return(versno);
}

/*##**********************************************************************\
 * 
 *		ss_vers_major
 * 
 */
int ss_vers_major(void)
{
        return(vers_major);
}

/*##**********************************************************************\
 * 
 *		ss_vers_minor
 * 
 */
int ss_vers_minor(void)
{
        return(vers_minor);
}

/*##**********************************************************************\
 * 
 *		ss_vers_release
 * 
 */
int ss_vers_release(void)
{
        return(vers_release);
}

/*##**********************************************************************\
 * 
 *		ss_servername
 * 
 */
char* ss_servername(void)
{
        return(vers_name);
}

/*##**********************************************************************\
 * 
 *		ss_setservername
 * 
 */
void ss_setservername(char* name)
{
        vers_name = name;
}

/*##**********************************************************************\
 * 
 *		ss_vers_dll
 * 
 */
int ss_vers_dll(void)
{
        return(vers_dll);
}
/*##**********************************************************************\
 * 
 *		ss_vers_dllpostfix
 * 
 */
char* ss_vers_dllpostfix(void)
{
        return(vers_dllpostfix);
}

/*##**********************************************************************\
 * 
 *		ss_vers_issync
 * 
 */
int ss_vers_issync(void)
{
        return(0);
}

/*##**********************************************************************\
 * 
 *		ss_vers_isaccelerator
 * 
 */
int ss_vers_isaccelerator(void)
{
        return(1);
}

/*##**********************************************************************\
 * 
 *		ss_vers_isdiskless
 * 
 */
int ss_vers_isdiskless(void)
{
        return(0);
}
