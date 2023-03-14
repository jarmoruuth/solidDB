/*************************************************************************\
**  source       * su0prof.h
**  directory    * su
**  description  * Execution time profiling.
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


#ifndef SU0PROF_H
#define SU0PROF_H

#include <ssc.h>
#include <ssdebug.h>

#include <su0time.h>

#define su_profile_timer        su_timer_t prof_timer

#define su_profile_start        if (ss_profile_active) { su_timer_start(&prof_timer); }
#define su_profile_stop(func)   if (ss_profile_active) { su_timer_stop(&prof_timer); su_profile_stopfunc(func, &prof_timer); }

#define su_profile_start_buf(tb)      if (ss_profile_active) { su_timer_start(tb); }
#define su_profile_stop_buf(tb, func) if (ss_profile_active) { su_timer_stop(tb); su_profile_stopfunc(func, tb); }

void su_profile_stopfunc(const char* func, su_timer_t* timer);

#endif /* SU0PROF_H */
