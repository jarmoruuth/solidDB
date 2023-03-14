/*************************************************************************\
**  source       * ssmemtrc.h
**  directory    * ss
**  description  * Call stack tracing for memory debugging
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


#ifndef SSMEMTRC_H
#define SSMEMTRC_H

#include "ssenv.h"

extern bool ss_memtrc_disablecallstack;

int SsMemTrcEnterFunction(
        char *filename,
        char *function_name);

int SsMemTrcExitFunction(
        char *filename,
        int pending_pop);

char** SsMemTrcCopyCallStk(
        void);

void SsMemTrcFreeCallStk(
        char** callstack);

void SsMemTrcGetFunctionStk(
        char** callstk, 
        int* p_len);

void* SsMemTrcGetCallStk(void);

void SsMemTrcFprintCallStk(
        void* fp,
        char** callstk,
        void* pcallstack);

void SsMemTrcPrintCallStk(
        char** callstk);

uint SsMemTrcGetCallStackHeight(
        char** callstk);

char* SsMemTrcGetCallStackNth(
        char** callstk,
        uint nth);

void SsMemTrcAddAppinfo(
        char* appinfo);

int ss_memtrc_hashpjw(
        char* s);

#endif /* SSMEMTRC_H */
