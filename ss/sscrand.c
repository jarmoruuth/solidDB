/*************************************************************************\
**  source       * sscrand.c
**  directory    * ss
**  description  * Random number generator suitable for cryptographic
**               * applications.
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

Random numnber and other numeric utilities.

Limitations:
-----------

None.

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

#include "ssenv.h"

#ifdef SS_NT
#define _WIN32_WINNT 0x0400
#include "sswindow.h"
#include <wincrypt.h>
#endif

#include "ssstdlib.h"
#include "ssdebug.h"

#include "sscrand.h"

#if defined(SS_UNIX)
# define SS_DEVRANDOM "/dev/random"
#endif

/* functions ***********************************************/


/*##**********************************************************************\
 * 
 *		ss_get_random_key
 * 
 * Generates a random number bytes secuence of given size.
 * 
 * Parameters :
 * 
 * Return value : 
 * 
 *      the size of sequence generated.
 * 
 * Limitations  : 
 * 
 *      very much system specific.
 * 
 * Globals used : 
 * 
 */
int ss_get_random_key(ss_int1_t* key, int size)
{
        int rc;

#if defined(SS_FREEBSD)
        int i;
        ss_dassert(size % sizeof(long) == 0);
        srandomdev();
        for (i=0; i<size; i+=sizeof(long)) {
            *(long*)(key+i) = random();
        }
        rc = size;
#elif defined(SS_DEVRANDOM)
        SS_FILE* f = SsFOpenB((char *)SS_DEVRANDOM, (char *)"r");
        if (f == NULL) {
            rc = -1;
            return rc;
        }

        rc = SsFRead(key, 1, size, f);
        SsFClose(f);
#elif defined(SS_NT)
        HCRYPTPROV hProvider = 0;
        rc = CryptAcquireContext(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
        ss_assert(rc);
        rc = CryptGenRandom(hProvider, size, key);
        ss_assert(rc);
        CryptReleaseContext(hProvider, 0);
        rc = size;
#else
        rc = -1;
#endif
        return rc;
}

