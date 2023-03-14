/*************************************************************************\
**  source       * dbe7binf.c
**  directory    * dbe
**  description  * Blob info
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

#include <ssdebug.h>
#include <ssmem.h>
#include <sslimits.h>

#include "dbe7binf.h"

/*##**********************************************************************\
 * 
 *		dbe_blobinfo_init
 * 
 * Initializes a blob v-tuple info array. Blob info is stored to the
 * key value in packed format. The first info in the array tells the
 * number of attributes in the v-tuple followed by blob attribute
 * indices in the v-tuple. Each v-attribute index is stored in two
 * bytes (limiting the number of attributes to 32k).
 * 
 * This functions initializes the blob info. Blob v-attributes are
 * added by function dbe_blobinfo_append.
 * 
 * Parameters : 
 * 
 *	p_blobinfo - out, give
 *		Dynamic v-attribute where the blob info is initialized.
 *		
 *	nparts - in
 *		Number of v-attributes in the v-tuple.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_blobinfo_init(dynva_t* p_blobinfo, uint nparts)
{
        uchar data[2];

        data[0] = (char)nparts;
        data[1] = (char)(nparts >> 8);

        dynva_setdata(p_blobinfo, data, sizeof(data));
}

/*##**********************************************************************\
 * 
 *		dbe_blobinfo_append
 * 
 * Appends blob v-attribute index (partno) to the blob info. The blob info
 * v-attribute must be first created by function dbe_blobinfo_init.
 * 
 * Parameters : 
 * 
 *	p_blobinfo - use
 *		Dynamic blob info v-attribute created by dbe_blobinfo_init.
 *		
 *	partno - in
 *		Blob v-attribute index in v-tuple.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_blobinfo_append(dynva_t* p_blobinfo, uint partno)
{
        uchar data[2];

        data[0] = (char)partno;
        data[1] = (char)(partno >> 8);

        dynva_appdata(p_blobinfo, data, sizeof(data));
}

/*##**********************************************************************\
 * 
 *		dbe_blobinfo_getattrs
 * 
 * Gets blob v-attribute info from a v-tuple. It is expected that the
 * blob info is stored to the last attribute of the v-tuple.
 *
 * A boolean array is allocated and returned to the user. The array
 * contains TRUE in those indices that contain blob v-attribute.
 * 
 * Parameters : 
 * 
 *	vtpl - in
 *		V-tuple containing v-attributes.
 *		
 *	tuple_nattrs - in
 *		Number of parts in tuple ttype. This is needed because
 *		after alter table the actual tuple may have more columns
 *		than in the v-tuple and the caller may index out of
 *		allocated and returned boolean array.
 *		
 *	p_nattrs - out
 *		Number of attributes in returned blob info boolean
 *		array.
 *		
 * Return value - give : 
 * 
 *      Boolean array containing TRUE in tsode indices where there is
 *      blob v-atttribute in the v-tuple. The array is allocated using
 *      SsMemAlloc, and the user must release the array using SsMemfree.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool* dbe_blobinfo_getattrs(
        vtpl_t* vtpl,
        int tuple_nattrs,
        int* p_nattrs)
{
        bool* blobattrs;
        int vacount;
        va_t* blobinfo;
        uchar* data;
        va_index_t datalen;
        int nparts;
        int partno;

        vacount = vtpl_vacount(vtpl);
        blobinfo = vtpl_getva_at(vtpl, vacount - 1);
        data = va_getdata(blobinfo, &datalen);
        ss_dassert(datalen >= 4);
        ss_dassert(datalen % 2 == 0);

        nparts = data[1] << 8;
        nparts |= data[0];
        ss_dassert(nparts == vacount - 1);
        data += 2;
        datalen -= 2;

        nparts = SS_MAX(tuple_nattrs, nparts);

        blobattrs = SsMemCalloc(sizeof(blobattrs[0]), nparts);

        while (datalen > 0) {
            partno = data[1] << 8;
            partno |= data[0];
            ss_dassert(partno < nparts);
            /* The following assert may fire because possible key compression 
             * removes special blob length information.
             * ss_dassert(va_testblob(vtpl_getva_at(vtpl, partno)));
             */
            blobattrs[partno] = TRUE;
            data += 2;
            datalen -= 2;
        }

        if (p_nattrs != NULL) {
            *p_nattrs = nparts;
        }

        return(blobattrs);
}
