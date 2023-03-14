/*************************************************************************\
**  source       * dt0type.h
**  directory    * dt
**  description  * Common definitions of data types.
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


#ifndef DT0TYPE_H
#define DT0TYPE_H

/* NOTE! Dfloat structure size is hard coded to sa header file. You should
 *       not change the dfloat size without considering the effects in
 *       sa interface.
 */
#define DT_DFL_DATASIZE 9

#ifndef dfloat 
#  define dfloat struct dfloatstruct
   struct dfloatstruct {
#ifdef SS_PURIFY
        union {
            /* Internal dfloat data structure. */
            char dfl_data[DT_DFL_DATASIZE];
            /* Field for proper size alignment for Purify. */
            char dfl_sizealign[4 * (DT_DFL_DATASIZE / 4) + 4];
            /* Field for proper byte alignment for Purify. */
            double dfl_bytealign;
        } _;
#else /* SS_PURIFY */
        /* Internal dfloat data structure. */
        char dfl_data[DT_DFL_DATASIZE];
#endif /* SS_PURIFY */
   };
#endif

#define dt_dfl_t dfloat

/* NOTE! Date structure size is hard coded to sa header file. You should
 *       not change the date size without considering the effects in
 *       sa interface.
 */
#define DT_DATE_DATASIZE 11

typedef struct dtdatestruct {

        char date_data[DT_DATE_DATASIZE];

} dt_date_t;

#endif /* DT0TYPE_H */
