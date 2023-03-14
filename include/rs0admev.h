/*************************************************************************\
**  source       * rs0admev.h
**  directory    * res
**  description  * Admin event definitions
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


#ifndef RS0ADMEV_H
#define RS0ADMEV_H

#include <ssc.h>
#include <sstime.h>

#include <rs0types.h>

typedef enum rs_admevent_id_en {

        RS_ADMEVENT_NOTIFY        = 0,
        RS_ADMEVENT_USERS,
        RS_ADMEVENT_MESSAGES,
        RS_ADMEVENT_STATE_OPEN,
        RS_ADMEVENT_STATE_SHUTDOWN,
        RS_ADMEVENT_STATE_MONITOR,
        RS_ADMEVENT_STATE_TRACE,
        RS_ADMEVENT_PARAMETER,
        RS_ADMEVENT_TMCMD,
        RS_ADMEVENT_TRX_TIMEOUT,
        RS_ADMEVENT_CHECKPOINT,
        RS_ADMEVENT_BACKUP,
        RS_ADMEVENT_ILL_LOGIN,
        RS_ADMEVENT_ERROR,
        RS_ADMEVENT_MERGE,
#ifdef SS_MME
        RS_ADMEVENT_MEMORY_LOW,
        RS_ADMEVENT_MEMORY_BACKTONORMAL,
#endif /* SS_MME */
        RS_ADMEVENT_ROWS2MERGE,
        RS_ADMEVENT_SHUTDOWNREQ,
        RS_ADMEVENT_BACKUPREQ,
        RS_ADMEVENT_MERGEREQ,
        RS_ADMEVENT_CHECKPOINTREQ,
        RS_ADMEVENT_IDLE,
        RS_ADMEVENT_HSBROLESWITCH,
        RS_ADMEVENT_HSBCONNECTSTATUS,
        RS_ADMEVENT_HSBLOGSIZE,   
        RS_ADMEVENT_NETCOPYREQ,   
        RS_ADMEVENT_NETCOPYEND,   
        RS_ADMEVENT_SACFAILED,
        RS_ADMEVENT_HSBSTATESWITCH,
        
        RS_ADMEVENT_ENDOFENUM
        
} rs_admevid_t;


/* some reserved userid values */

#define SYS_UID_SERVER      -1
#define SYS_UID_TMCMD       -2
#define SYS_UID_EXTERNAL    -3
#define SYS_UID_TERMINATOR  -4

rs_admevid_t rs_admev_ename2eid(char* ename);

char* rs_admev_eid2ename(rs_admevid_t eid);

#endif /* RS0ADMEV_H */
