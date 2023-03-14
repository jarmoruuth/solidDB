/*************************************************************************\
**  source       * su0vers.h
**  directory    * su
**  description  * Solid Global version constants
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


#ifndef SU0VERS_H
#define SU0VERS_H

#include <ssenv.h>  /* Version info is now found here */

#define SU_SERVER_VERSION   SS_SERVER_VERSION
#define SU_SERVER_NAME      SS_SERVER_NAME
#define SU_SERVER_VERSNUM   SS_SERVER_VERSNUM
#define SU_SOLIDVERS        SS_SOLIDVERS
#define SU_COPYRIGHT        SS_COPYRIGHT

/* Version history:

        FOR DETAILS, SEE FILE HISTORY.TXT

        ---------------------------------------------------------------------
        01.00.0004  Fixes for SA level transactions, wrong lost update
                    detection.
                    Communication long message fix.
        01.00.0005  Out of cache buffers problem with many cursors fixed.
                    I/O optimizations for VAX/VMS.
        01.00.0006  DECnet for OS/2, 16-bit DLL.
                    ROWID attribute added also to system relations.
        01.00.0007  SYS_ADMIN_ROLE implemented.
                    Sd command from tty rc waits until server is shut down.
        01.00.0008  SA multithread problem fixed. Abnormal disconnect
                    referenced already released variables.
        01.00.0009  Memory usage fix in active trx list in dbe7gtrs.c.
        ---------------------------------------------------------------------
        01.10.0000  18.05.94
        01.10.0001  20.05.94
        01.10.0002  27.05.94
        01.10.0003  30.05.94
        01.10.0004  01.06.94
        01.10.0005  02.06.94
        01.10.0006  07.06.94
        01.10.0007  14.06.94
        01.10.0008  15.06.94
        01.10.0009  21.06.94
        01.10.0010  10.08.94

        ---------------------------------------------------------------------
        01.11.0000  17.08.94
        01.11.0001  31.08.94
        ---------------------------------------------------------------------
        01.21.0000  09.09.94
*/

/* Database header record version # */
#define SU_DBHEADER_VERSNUM 0x0332

/* database file format version #
 * database file format # must be incremented when
 * database format is changed.
 * Note that this number has nothing to do with
 * server version number
 */
#define SU_DBFILE_VERSNUM   0x0600 /* Compatible with the server version 06.00 */

/* First version with UNICODE data dictionary */
#define SU_DBFILE_VERSNUM_UNICODE_START   0x021E

/* First version with CATALOG support */
#define SU_DBFILE_VERSNUM_CATALOGSUPP_START 0x0332

/* first version where new G2 BLOBs were used */
#define SU_DBFILE_VERSNUM_BLOBG2_START 0x0401

/* first (GA) version where M-tables supported */
#define SU_DBFILE_VERSNUM_MME_START 0x0402

/* first (GA) version where HSBG2 is enabled */
#define SU_DBFILE_VERSNUM_HSBG2_START 0x0403

/* first (GA) version where B-tree bnode mismatch index is enabled */
#define SU_DBFILE_VERSNUM_BNODEMISMATCHHINDEX_START 0x0600

#endif /* SU0VERS_H */
