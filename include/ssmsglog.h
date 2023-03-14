/*************************************************************************\
**  source       * ssmsglog.h
**  directory    * ss
**  description  * Log file routines for messages logging.
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


#ifndef SSMSGLOG_H
#define SSMSGLOG_H

#include "ssc.h"
#include "ssstdarg.h"

#define SS_MSGLOG_BUFSIZE   32000

typedef struct SsMsgLogStruct SsMsgLogT;

SsMsgLogT* SsMsgLogInitDefaultTrace(void);
SsMsgLogT* SsMsgLogGiveDefaultTrace(void);
void SsMsgLogSetDefaultTraceSize(long size);
void SsMsgLogDisable(void);
void SsMsgLogSetTraceSecDecimals(long tracesecdecimals);

SsMsgLogT* SsMsgLogInit(char* filename, long loglimit);
SsMsgLogT* SsMsgLogInitForce(char* filename, long loglimit, bool forcep);
SsMsgLogT* SsMsgLogLink(SsMsgLogT* ml);
void SsMsgLogDone(SsMsgLogT* ml);
void SsMsgLogFlush(SsMsgLogT* ml);
void SsMsgLogPutStr(SsMsgLogT* ml, char* message);
void SsMsgLogVPrintf(SsMsgLogT* ml, char *message, va_list argptr);
void SsMsgLogVPrintfWithTime(SsMsgLogT* ml, char *message, va_list argptr);
void SS_CDECL SsMsgLogPrintf(SsMsgLogT* ml, char* format, ...);
void SS_CDECL SsMsgLogPrintfWithTime(SsMsgLogT* ml, char* format, ...);
char* SsMsgLogGetFileName(SsMsgLogT* ml);
void SsMsgLogSetLimit(SsMsgLogT* ml, long loglimit);
void SsMsgLogGlobalInit(void);
void SsMsgLogSetForceSplitOnce(void);

#endif /* SSMSGLOG_H */
