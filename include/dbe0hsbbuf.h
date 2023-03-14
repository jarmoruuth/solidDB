/*************************************************************************\
**  source       * dbe0hsbbuf.h
**  directory    * dbe
**  description  * HSB buffer interface
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

Limitations:
-----------

Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#ifndef DBE0HSBBUF_H
#define DBE0HSBBUF_H

#include <ssc.h>
#include <sssem.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssstring.h>
#include <sssprint.h>
#include <sslimits.h>
#include <ssfile.h>
#include <ssltoa.h>
#include <sspmon.h>

#include "dbe0lb.h"

typedef struct dbe_hsbbuf_st dbe_hsbbuf_t;

dbe_hsbbuf_t* dbe_hsbbuf_init(
#ifdef IO_OPT
        dbe_alogbuf_t* alogbuf,
#else
        dbe_logbuf_t* logbuf,
#endif
        size_t bufsize);

void dbe_hsbbuf_done(
        dbe_hsbbuf_t* hsbbuf);

void dbe_hsbbuf_link(
        dbe_hsbbuf_t* hsbbuf);

#ifdef IO_OPT
dbe_alogbuf_t* dbe_hsbbuf_get_alogbuf(
        dbe_hsbbuf_t* hsbbuf);
#else
dbe_logbuf_t* dbe_hsbbuf_get_logbuf(
        dbe_hsbbuf_t* hsbbuf);
#endif

size_t dbe_hsbbuf_get_bufsize(
        dbe_hsbbuf_t* hb);

#endif /* DBE0HSBBUF_H */

