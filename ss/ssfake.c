/*************************************************************************\
**  source       * ssfake.c
**  directory    * ss
**  description  * Fake definitions
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


#include "ssdebug.h"
#include "ssthread.h"
#include "ssfake.h"
#include "ssrtcov.h"


uint ss_fake_cmd(
        void* cd __attribute__ ((unused)), 
        char** argv __attribute__ ((unused)), 
        char* err_text __attribute__ ((unused)), 
        size_t err_text_size __attribute__ ((unused)))
{
        return(0);
}

#ifdef SS_FAKE

bool ss_fake_pause(int fakenum)
{
        bool paused = FALSE;
        FAKE_CODE_BLOCK(fakenum, {
                int val = 1;
                ss_dprintf_1(("ss_fake_pause:fakenum %d paused\n", fakenum));
                SS_RTCOVERAGE_INC(SS_RTCOV_FAKE_PAUSE);
                while (val > 0) {
                    paused = TRUE;
                    SsThrSleep(200);
                    FAKE_GET(fakenum, val);
                }
                ss_dprintf_1(("ss_fake_pause:fakenum %d resumed\n", fakenum));
        });

        return(paused);
}


#endif

