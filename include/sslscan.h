/*************************************************************************\
**  source       * sslscan.h
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


#ifndef SSLSCAN_H
#define SSLSCAN_H

#include "ssc.h"

/*##**********************************************************************\
 * 
 *		SsLcsScanLong
 * 
 * Converts a wide-char string to long integer. 
 * Leading whitespaces are skipped.
 * 
 *  [whitespace][{+|-}][digits]
 * 
 * Parameters : 
 * 
 *      s - in, use
 *          source string
 *
 *      p_l - out
 *          pointer to long integer
 *
 *      p_mismatch - out
 *          pointer to the first position in src that
 *          does not belong to the long integer returned
 *
 * Return value : 
 * 
 *      TRUE, if succesful
 *      FALSE, if failed.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsLcsScanLong(ss_lchar_t* s, long* p_l, ss_lchar_t** p_mismatch);

#endif /* SSLSCAN_H */


