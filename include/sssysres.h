/*************************************************************************\
**  source       * sssysres.h
**  directory    * ss
**  description  * System resource pool object. This object is used
**               * to save system resources that must be released
**               * before the executable exists.
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


#ifndef SSSYSRES_H
#define SSSYSRES_H

void SsSysResGlobalInit(
        void);

void SsSysResGlobalDone(
        void);

void* SsSysResAdd(
        void (*freefun)(void* data),
        void* data);

void* SsSysResAddLast(
        void (*freefun)(void* data),
        void* data);

void SsSysResRemove(
        void* resid);

#endif /* SSSYSRES_H */
