/*************************************************************************\
**  source       * ssutiwnt.h
**  directory    * ss
**  description  * Some utilities for Windows NT version
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


#ifndef SSUTIWNT_H
#define SSUTIWNT_H

#include "ssenv.h"

#if defined (SS_NT)

#include "sswindow.h"

LPSECURITY_ATTRIBUTES SsWntACLInit(void);
void SsWntACLDone(LPSECURITY_ATTRIBUTES lpsa);

#endif /* SS_NT */

#endif /* SSUTIWNT_H */
