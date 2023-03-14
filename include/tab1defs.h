/*************************************************************************\
**  source       * tab1defs.h
**  directory    * tab
**  description  * General type and other definitions.
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


#ifndef TAB1DEFS_H
#define TAB1DEFS_H

#include "rs0sdefs.h"

typedef enum {
        TBCHK_RELHTYPE = SS_CHKBASE_TAB,
        TBCHK_KEYTYPE,
        TBCHK_RELCUR,
        TBCHK_TRANS,
        TBCHK_VIEWHTYPE,
        TBCHK_TLICON,
        TBCHK_TLICUR,
        TBCHK_SACON,
        TBCHK_SACUR,
        TBCHK_TDB,
        TBCHK_TCON,
        TBCHK_HURC,
        TBCHK_TBSQL,
        TBCHK_SEQ,
        TBCHK_PATCH,
        TBCHK_SCHEMA,
        TBCHK_SCHEMAINFO,
        TBCHK_BLOBG2MGR,
        TBCHK_BLOBG2WRITESTREAM,
        TBCHK_BLOBG2READSTREAM,
        TBCHK_BLOBREADSTREAMWRAPPER,
        TBCHK_INMEMORYBLOBG2REF,
        TBCHK_FREED = SS_CHKBASE_TAB + SS_CHKBASE_FREED_INCR,
        TBCHK_FREEDBLOBG2MGR,
        TBCHK_FREEDBLOBG2WRITESTREAM,
        TBCHK_FREEDBLOBG2READSTREAM,
        TBCHK_FREEDINMEMORYBLOBG2REF,
        TBCHK_FREEDBLOBREADSTREAMWRAPPER,
        TBCHK_SYSPROPERTIES,
        TBCHK_SYSPROPERTY,
        TBCHK_EST,
        TBCHK_SRVLOCKWAIT
} tb_check_t;

#define TB_INFO_PROPERTY_SYNCMASTER     RS_BBOARD_PAR_SYNCMASTER
#define TB_INFO_PROPERTY_SYNCREPLICA    RS_BBOARD_PAR_SYNCREPLICA
#define TB_INFO_PROPERTY_SYNCNODE       RS_BBOARD_PAR_NODE_NAME

#endif /* TAB1DEFS_H */
