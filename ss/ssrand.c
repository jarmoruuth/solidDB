/*************************************************************************\
**  source       * ssrand.c
**  directory    * ss
**  description  * simple, fast re-entrant pseudorandom generator
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


#include <ssc.h>
#include <ssdebug.h>

#include "ssrand.h"

#define A            16807
#define M       2147483647L
#define Q           127773L      /* M div A */
#define R             2836      /* M mod A */

/*#***********************************************************************\
 * 
 *		ss_rand_init
 * 
 * Initializes a ss_rand_t object by seed value
 * 
 * Parameters : 
 * 
 *	r - use
 *		pointer to ss_rand_t object
 *		
 *	seed - in
 *		seed value integer
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void ss_rand_init(ss_rand_t* r, ss_int4_t seed)
{
        ss_dassert(r != NULL);
        if (seed < 0) {
            seed = -seed;
        }
        *r = (ss_rand_t)seed;
}

/*##**********************************************************************\
 * 
 *		ss_rand_int4
 * 
 * gives next random value from the ss_rand_t object
 * 
 * Parameters : 
 * 
 *	r - use
 *		pointer to ss_rand_t object
 *		
 * Return value :
 *      pseudorandom positive 4-byte integer
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_int4_t ss_rand_int4(ss_rand_t* r)
{
        ss_int4_t t = (ss_int4_t)*r;

        ss_dassert(r != NULL);
        t = A * (t % Q) - R * (t / Q);
        if (t < 0) {
            t += M;
        }
        *r = (ss_rand_t)t;
        return (t);
}
