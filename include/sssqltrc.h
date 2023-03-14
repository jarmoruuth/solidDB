/*************************************************************************\
**  source       * sssqltrc.h
**  directory    * ss
**  description  * SQL string tracing, for error reports.
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


#ifndef SSSQLTRC_H
#define SSSQLTRC_H

typedef struct SsSQLTrcInfoStruct SsSQLTrcInfoT;

void SsSQLTrcSetStr(
        char* sqlstr);

char* SsSQLTrcGetStr(
        void);

#ifdef SS_DEBUG

SsSQLTrcInfoT* SsSQLTrcInfoCopy(
        void);

void SsSQLTrcInfoFree(
        SsSQLTrcInfoT* sqltrcinfo);

char* SsSQLTrcInfoGetStr(
        SsSQLTrcInfoT* sqltrcinfo);

void SsSQLTrcInfoPrint(
        SsSQLTrcInfoT* sqltrcinfo);

#endif /* SS_DEBUG */

#endif /* SSSQLTRC_H */
