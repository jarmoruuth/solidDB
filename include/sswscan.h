/*************************************************************************\
**  source       * sswscan.h
**  directory    * ss
**  description  * Wide char string scan routines
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


#ifndef SSWSCAN_H
#define SSWSCAN_H

#include "ssc.h"

bool SsWcsScanDouble(
	ss_char2_t* src,
	double* p_d,
        ss_char2_t** p_mismatch);

bool SsWcsScanLong(
        ss_char2_t* s,
        long* p_l,
        ss_char2_t** p_mismatch);

bool SsWcsScanInt(
	ss_char2_t* src,
	int* p_i,
        ss_char2_t** p_mismatch);




#endif /* SSWSCAN_H */
