/*************************************************************************\
**  source       * su1check.h
**  directory    * su
**  description  * Internal check fields for su_ structures
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


#ifndef SU1CHECK_H
#define SU1CHECK_H

#include <ssdebug.h>

typedef enum {
        SUCHK_BSTREAM = SS_CHKBASE_SU,
        SUCHK_INIFILE,
        SUCHK_PA,
        SUCHK_LIST,
        SUCHK_LISTNODE,
        SUCHK_RBT,
        SUCHK_RBTNODE,
        SUCHK_ERR,
        SUCHK_PROPS,
        SUCHK_PROPSECT,
        SUCHK_PROPERTY,
        SUCHK_PROLI,
        SUCHK_PROP,
        SUCHK_RBT2,
        SUCHK_RBT2NODE,
        SUCHK_WPROLI,
        SUCHK_WPROP,
        SUCHK_STACK,
        SUCHK_STACKELEMENT,
        SUCHK_BACKUPBUFPOOL,
        SUCHK_BACKUPWRITER,
        SUCHK_BACKUPREADER,
        SUCHK_BACKUPREADJOB,
        SUCHK_BACKUPREADSPEC,
        SUCHK_NM,
        SUCHK_GATE,
        SUCHK_GATEENTERER,
        SUCHK_MBSVFIL,
        
        SUCHK_FREEDBACKUPBUFPOOL = SS_CHKBASE_SU + SS_CHKBASE_FREED_INCR,
        SUCHK_FREEDBACKUPWRITER,
        SUCHK_FREEDBACKUPREADER,
        SUCHK_FREEDBACKUPREADJOB,
        SUCHK_FREEDBACKUPREADSPEC,
        SUCHK_FREEDGATE,
        SUCHK_FREEDGATEENTERER,
        SUCHK_FREEDMBSVFIL,

        SUCHK_TRIE,
        SUCHK_TRIE_NODE,
        SUCHK_FREED_TRIE,
        SUCHK_FREED_TRIE_NODE,
        SUCHK_VTRIE,
        SUCHK_VTRIE_NODE,
        SUCHK_VTRIE_NODE_PA,
        SUCHK_SERVICE,
        SUCHK_SERVICE_JOB,
        SUCHK_SERVICE_EVENT,
        SUCHK_SNDX,

        SUCHK_INTSPAN,
        SUCHK_INTSPAN_ELEM,

        SUCHK_TST,
        SUCHK_TSTNODE,
        SUCHK_TSTKEY,
        SUCHK_CIPHER
} su_check_t;

#endif /* SU1CHECK_H */
