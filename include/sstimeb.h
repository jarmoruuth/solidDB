/*************************************************************************\
**  source       * sstimeb.h
**  directory    * ss
**  description  * Not all platforms have timeb.h, this is substitute
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

 Defined structure returned by ftime syscall.

Limitations:
-----------

 Should be used only with platforms which do not have timeb structures defined

Error handling:
--------------

N/A

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

None.

Example:
-------

**************************************************************************
#endif /* DOCUMENTATION */

#ifndef _SYS_TIMEB_H
#define _SYS_TIMEB_H    1
#endif

 /* #include <features.h> */


/* Structure returned by ftime syscall */

struct timeb
{
    time_t time;                /* Seconds since epoch. */
    unsigned short int millitm; /* Additional accuracy, 1000 msec.  */
    short int timezone;         /* Timezone, minutes west of GMT.  */
    short int dstflag;          /* Nonzero if DST is used.  */
};

extern int ftime (struct timeb*);




