/*************************************************************************\
**  source       * ssutiwnt.c
**  directory    * ss
**  description  * Utilities for Windows NT
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

#if defined(SS_NT)

#include "sswindow.h"
#include "ssmem.h"
#include "ssdebug.h"
#include "ssutiwnt.h"

/*##**********************************************************************\
 * 
 *		SsWntACLInit
 * 
 * Creates security descriptor whose disclosure ACL (Acces Control List)
 * is empy.
 * 
 * This kind of security attributes are used in SOLID NT Service.
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
LPSECURITY_ATTRIBUTES SsWntACLInit(void)
{
        if (SS_ENV_OS == SS_OS_W95) {

            return(NULL);

        } else {

            LPSECURITY_ATTRIBUTES   lpsa;
            PSECURITY_DESCRIPTOR    pSD;

            lpsa = SSMEM_NEW(SECURITY_ATTRIBUTES);

            pSD = (PSECURITY_DESCRIPTOR)SsMemAlloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
            ss_assert(pSD);
            memset(pSD, '\0', SECURITY_DESCRIPTOR_MIN_LENGTH);

            if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)) {
                SsMemFree(pSD);
                ss_error;
                /* return(NULL); */
            }

            // add a NULL disc. ACL to the security descriptor.
            //
            if (!SetSecurityDescriptorDacl(pSD, TRUE, (PACL) NULL, FALSE)) {
                SsMemFree((HLOCAL)pSD);
                ss_error;
                /* return(NULL); */
            }

            lpsa->nLength = sizeof(SECURITY_ATTRIBUTES);
            lpsa->lpSecurityDescriptor = pSD;
            lpsa->bInheritHandle = TRUE;       
            return(lpsa);
        }
}


/*##**********************************************************************\
 * 
 *		SsWntACLDone
 * 
 * Frees a security descriptor
 * 
 * Parameters : 
 * 
 *	acl - 
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
void SsWntACLDone(LPSECURITY_ATTRIBUTES lpsa)
{
        if (SS_ENV_OS == SS_OS_W95) {
            return;
        } else {
            SsMemFree(lpsa->lpSecurityDescriptor);
            SsMemFree(lpsa);
        }
}

#endif /* SS_NT */
