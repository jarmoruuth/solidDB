/*************************************************************************\
**  source       * xs0error.c
**  directory    * xs
**  description  * Error codes for eXternal Sorter
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
#include <su0error.h>

#include <rs0types.h>

#include "xs0error.h"

static su_rc_text_t xs_rc_texts[] = {
{ XS_OUTOFCFGDISKSPACE,     SU_RCTYPE_ERROR,    "XS_OUTOFCFGDISKSPACE",
  "Sort failed due to insufficient configured TmpDir space" },
{ XS_OUTOFPHYSDISKSPACE,    SU_RCTYPE_ERROR,    "XS_OUTOFPHYSDISKSPACE",
  "Sort failed due to insufficient physical TmpDir space" },
{ XS_OUTOFBUFFERS,          SU_RCTYPE_ERROR,    "XS_OUTOFBUFFERS",
  "Sort failed due to insufficient sort buffer space" },
{ XS_ERR_TOOLONGROW,        SU_RCTYPE_ERROR,    "XS_ERR_TOOLONGROW",
  "Sort failed due to too long row (internal failure)" },
{ XS_ERR_SORTFAILED,        SU_RCTYPE_ERROR,    "XS_ERR_SORTFAILED",
  "Sort failed due to I/O error" },
{ XS_FATAL_PARAM_SSUUU,     SU_RCTYPE_FATAL,    "XS_FATAL_PARAM_SSUUU",
  "Illegal value specified for parameter:\n[%s]\n%s=%u\n(legal range is %u - %u)\n" },
{ XS_FATAL_DIR_S,           SU_RCTYPE_FATAL,    "XS_FATAL_DIR_S",
  "Sorter temporary directory: %s does not exist\n" }

};

/*##**********************************************************************\
 * 
 *		xs_error_init
 * 
 * Adds external sorter texts to the global error text system
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
void xs_error_init(void)
{
        su_rc_addsubsys(
            SU_RC_SUBSYS_XS,
            xs_rc_texts,
            sizeof(xs_rc_texts) / sizeof(xs_rc_texts[0]));
}
