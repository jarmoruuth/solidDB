/*************************************************************************\
**  source       * su0gate.h
**  directory    * su
**  description  * Gate semaphores.
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


#ifndef SU0GATE_H
#define SU0GATE_H

#include <ssc.h>
#include <sssem.h>

typedef struct su_gate_st su_gate_t;

#ifdef SS_MT

su_gate_t* su_gate_init(ss_semnum_t num, bool checkonlyp);
void       su_gate_done(su_gate_t* gate);

void su_gate_enter_shared(su_gate_t* gate);
void su_gate_enter_exclusive(su_gate_t* gate);
void su_gate_exit(su_gate_t* gate);
uint su_gate_ninqueue(su_gate_t* gate);
void su_gate_setmaxexclusive(su_gate_t* gate, int maxexclusive);

#ifdef SS_SEM_DBG
# define su_gate_init(n, c) \
        su_gate_init_dbg(n, c, (char *)__FILE__, __LINE__)

su_gate_t* su_gate_init_dbg(
        ss_semnum_t num,
        bool checkonlyp,
        char* file,
        int line);
#endif /* SS_SEM_DBG */

#ifdef SS_DEBUG

bool su_gate_thread_is_shared(su_gate_t* gate);
bool su_gate_thread_is_exclusive(su_gate_t* gate);
void su_gate_set_thread_tracking(su_gate_t* gate, bool flag);

#endif

#else /* SS_MT */

#define su_gate_init(c,n)                           (NULL)
#define su_gate_done(gate)
#define su_gate_enter_shared(gate)
#define su_gate_enter_exclusive(gate)
#define su_gate_exit(gate)
#define su_gate_ninqueue(gate)                      (0)
#define su_gate_init_dbg(c,n, file, line)           (NULL)

#endif /* SS_MT */

#endif /* SU0GATE_H */
