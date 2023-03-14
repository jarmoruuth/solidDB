/*************************************************************************\
**  source       * sswfile.h
**  directory    * ss
**  description  * Wide character file support
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


#ifndef SSWFILE_H
#define SSWFILE_H

#include "ssc.h"
#include "ssstdio.h"

typedef enum {
        SSTFT_ASCII,
        SSTFT_UNICODE_LSB1ST,
        SSTFT_UNICODE_MSB1ST,
        SSTFT_ERROR
} SsTextFileTypeT;

bool SsWchdir(ss_char2_t* dirname);
SS_FILE* SsWfopen(ss_char2_t* fname, ss_char2_t* flags);
SsTextFileTypeT SsFTypeGet(SS_FILE* fp);
int SsFputwc(ss_char2_t c, SS_FILE* fp);
int SsFPutWBuf(ss_char2_t* s, size_t n, SS_FILE* fp);

int SsFputws(ss_char2_t* s, SS_FILE* fp);
int SsFgetwc(SS_FILE* fp, SsTextFileTypeT ftype);
ss_char2_t* SsFgetws(
        ss_char2_t* buf,
        size_t n,
        SS_FILE* fp,
        SsTextFileTypeT ftype);

/* UTF8 routines */
SS_FILE* SsUTF8fopen(
        ss_char1_t* fname, 
        ss_char1_t* flags);

bool SsUTF8chdir(
        ss_char1_t* dirname);

#endif /* SSWFILE_H */
