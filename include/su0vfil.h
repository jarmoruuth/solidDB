/*************************************************************************\
**  source       * su0vfil.h
**  directory    * su
**  description  * Declares file interface with 'virtual file handle'.
**               * Virtual file handle is a trick which keeps
**               * a virtually open file, which may actually be closed.
**               * Each file is opened whenever it is accessed.
**               * This was done because open file handles are a limited
**               * resource in many OS's. This package is also safe in
**               * multithreaded applications.
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

#ifndef SU0VFIL_H
#define SU0VFIL_H

#include <ssstdio.h>
#include <ssfile.h>

typedef FILE su_vfile_t;
typedef struct su_vfilh_st su_vfilh_t;  /* virtual file handle */
typedef struct su_pfilh_st su_pfilh_t;  /* psysical file handle */

su_vfile_t *su_vfp_init_txt(char *pathname, char *flags);
su_vfile_t *su_vfp_init_bin(char *pathname, char *flags);
void su_vfp_done(su_vfile_t *vfpp);
SS_FILE *su_vfp_access(su_vfile_t *vfpp);

su_vfilh_t *su_vfh_init(
        char *pathname,
        bool persistent,
        uint flags,
        size_t blocksize);

void su_vfh_done(su_vfilh_t *vfhp);
SsBFileT *su_vfh_beginaccess(su_vfilh_t *vfhp, su_pfilh_t **pp_pfh);
SsBFileT *su_vfh_beginaccesspers(su_vfilh_t *vfhp, su_pfilh_t **pp_pfh);
void su_vfh_endaccess(su_vfilh_t *vfhp, su_pfilh_t *pfhp);
void su_vfh_flush(su_vfilh_t *vfhp);
void su_vfh_close(su_vfilh_t *vfhp);

bool su_vfh_globalinit(uint max);
void su_vfh_globaldone(void);
bool su_vfh_isinitialized(void);
void su_vfh_setmaxopen(uint max);
uint su_vfh_getmaxopen(void);
uint su_vfh_getnrpersistent(void);
uint su_vfh_getnraccessed(void);
uint su_vfh_getnrcached(void);
uint su_vfh_getnropen(void);
char *su_vfh_getfilename(su_vfilh_t *vfhp);
bool su_vfh_ispersistent(su_vfilh_t *vfhp);

#endif /* SU0VFIL_H */

