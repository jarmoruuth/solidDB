/*************************************************************************\
**  source       * dbe0spm.h
**  directory    * dbe
**  description  * Space maneger for HSB.
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


#ifndef DBE0SPM_H
#define DBE0SPM_H

#include <sssem.h>

typedef struct dbe_spm_st dbe_spm_t;

dbe_spm_t* dbe_spm_init(
        int maxsize, 
        bool catchup);

void dbe_spm_done(
        dbe_spm_t* spm);

int dbe_spm_getmaxsize(
        dbe_spm_t* spm);

void dbe_spm_setactive(
        dbe_spm_t* spm,
        bool active);

void dbe_spm_setfreespacerate(
        dbe_spm_t* spm,
        int rate);

void dbe_spm_addspace(
        dbe_spm_t* spm,
        int size);

void dbe_spm_getspace(
        dbe_spm_t* spm,
        int size);

#endif /* DBE0SPM_H */

