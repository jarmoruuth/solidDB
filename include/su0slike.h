/*************************************************************************\
**  source       * su0slike.h
**  directory    * su
**  description  * SQL string "LIKE" operator
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


#ifndef SU0SLIKE_H
#define SU0SLIKE_H

#ifndef SU_SLIKE_NOESCHAR
# define SU_SLIKE_NOESCCHAR  (-1)
#endif /* SU_SLIKE_NOESCHAR */

bool su_slike(
        ss_char1_t* s,
        size_t      slen,
        ss_char1_t* patt,
        size_t      plen,
        int         esc
);

bool su_slike_legalpattern(
	ss_char1_t* patt,
        size_t      pattlen,
	int         esc);

ss_char1_t* su_slike_fixedprefix(
	ss_char1_t*   patt,
        size_t  pattlen,
	int     esc,
        size_t* p_prefixlen,
        ss_char1_t buf_or_null[/* pattlen + 1 */]);

size_t su_slike_prefixinfo(
        ss_char1_t* patt,
        size_t pattlen,
        int esc,
        size_t* p_numfixedchars,
        size_t* p_numwildcards);

#endif /* SU0SLIKE_H */
