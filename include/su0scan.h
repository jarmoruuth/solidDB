/*************************************************************************\
**  source       * su0scan.h
**  directory    * su
**  description  * Portable sscan utilities 
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


#if 0 /* Pete removed 1996-08-06 */

#ifndef SU0SCAN_H
#define SU0SCAN_H

#include <ssc.h>

bool su_sscandouble(char* src, double* p_d, char** p_mismatch);
bool su_sscanlong(char* src, long* p_l, char** p_mismatch);
bool su_sscanint(char* src, int* p_i, char** p_mismatch);

#endif /* SU0SCAN_H */
#endif /* 0 (Pete removed 1996-08-06) */
