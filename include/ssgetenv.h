/*************************************************************************\
**  source       * ssgetenv.h
**  directory    * ss
**  description  * Environment handling
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


#ifndef SSGETENV_H
#define SSGETENV_H

#include "ssenv.h"
#include "ssstdlib.h"

#if defined(WINDOWS)

char* SsGetEnv(const char* varname);

#else

#	define SsGetEnv getenv

#endif


#endif /* SSGETENV_H */
