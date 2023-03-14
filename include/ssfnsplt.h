/*************************************************************************\
**  source       * ssfnsplt.h
**  directory    * ss
**  description  * File name split/merge routines.
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


#ifndef SSFNSPLT_H
#define SSFNSPLT_H

#ifdef NO_ANSI

int SsFnSplitPath();
int SsFnMakePath();

#else

int SsFnSplitPath(
        char *pathname,
        char *dbuf,
        int dbuf_s, 
        char *fbuf,
        int fbuf_s);

bool SsFnIsPath(
        char* pathname);

int SsFnMakePath(
        char *dirname,
        char *filename,
        char *pbuf,
        int pbuf_s);

int SsFnSplitPathExt(
        char*   pathname,
        char*   dbuf,
        int     dbuf_s, 
        char*   fbuf,
        int     fbuf_s,
        char*   ebuf,
        int     ebuf_s);

int SsFnMakePathExt(
        char*   dirname,
        char*   basename,
        char*   extension,
        char*   pbuf,
        int     pbuf_s);

bool SsFnPathIsAbsolute(
        char* pathname);

bool SsFnMakePath3Way(
        char* basedirname,
        char* dirname,
        char* fname,
        char*   pbuf,
        int     pbuf_s);


#endif

#endif /* SSFNSPLT_H */
