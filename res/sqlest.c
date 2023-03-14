/*************************************************************************\
**  source       * sqlest.c
**  directory    * res
**  description  * estimation aid functions
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


#include <ssc.h>
#include <ssdebug.h>

#include "tsamplea.h"

/* The function intp is used to interpolate a double value.

   Parameters:

    x1, x2                  the two x values
    y1, y2                  the two y values
    x                       the x value for which the y value should be
                            found
*/

static double intp(
    double x1,
    double x2,
    double y1,
    double y2,
    double x
) {
    return(y1 + (y2 - y1) * (x - x1) / (x2 - x1));
}

ulong sql_tupleest(
    ulong relsize,
    uint diff
) {
    uint i = 0;
    ss_assert(relsize >= SAMPLE_S && diff && diff <= SAMPLE_S);

    /* check for oversize relation */
    if (relsize > relsizes[relsize_n - 1]) {
        relsize = relsizes[relsize_n - 1];
    }

    while (i + 1 < relsize_n && relsize > relsizes[i + 1]) {
        i++;
    }

    {
        /* find the location of the relation size in the "x" scale */
        double r1 = relsizes[i];
        double r2 = relsizes[i + 1];
        double relsize_d = (double)relsize;
        double ii = (relsize_d - r1) / (r2 - r1);

        uint j = 0;

        /* interpolate the corresponding diff value in the "1 to be found"
           line for the relation size
        */
        double diffint1 = intp(
            0.0, 1.0, diffarray[i], diffarray[i + 1], ii
        ), diffint2 = 0, dd = (double)diff;

        /* check if so many different found that 1 should be returned */
        if (diffint1 <= dd) {
            return(1);
        }

        /* loop to find the correct "y" slot */
        while (j + 1 < est_n) {
            diffint2 = intp(
                0.0,
                1.0,
                diffarray[i + (j + 1) * relsize_n],
                diffarray[i + 1 + (j + 1) * relsize_n],
                ii
            );
            if (diffint2 <= dd) {
                break;
            }
            j++;
            diffint1 = diffint2;
        }

        /* interpolate the final "y" value */
        return(
            (ulong)intp(
                diffint1,
                diffint2,
                (double)ests[j],
                (double)ests[j + 1],
                dd
            )
        );
    }
}
