/*************************************************************************\
**  source       * sssocket.h
**  directory    * ss
**  description  * a poke at making a portable socket interface
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


#ifndef SSSOCKET_H
#define SSSOCKET_H

/* Usage:
 *
 * ss_socket_globalinit();
 *
 * int fd = ss_socket_new(...);  // is really socket() so see its docs.
 *
 * <... do stuff ...>
 *
 * ss_socket_close(fd); 
 *
 * ss_socket_globaldone();
 *
 */

#include <ssdebug.h>

/* init macro defaults:
#  define ss_socket_globalinit()
#  define ss_socket_new(a,b,c)     socket((a),(b),(c))
#  define ss_socket_close(a)       close(a)
#  define ss_socket_globaldone()
#  define ss_socket_geterror()     (errno)
#  define SS_SOCKET_INVALIDSOCKET  (-1)
*/

#if SOME_WEIRD_OS_WITH_DIFFERENT_SOCKET_H_ANDOR_LOCATION_OF_CLOSE
#elif defined(SS_W32) || defined(SS_NT) || defined(SS_WIN)
#  include <winsock.h>
#  include <io.h>             /* close */
/* init macros */
#  define ss_socket_globalinit()                                           \
        do {                                                               \
            WORD wVersionRequested;                                        \
            WSADATA wsaData;                                               \
            int err;                                                       \
            wVersionRequested = MAKEWORD(1, 1);                            \
            err = WSAStartup(wVersionRequested,                            \
                             &wsaData);                                    \
            ss_info_assert(err == 0,                                       \
                          ("Can't find WinSock DLL, error = %d", err));    \
            if (   LOBYTE(wsaData.wVersion) != 1                           \
                || HIBYTE(wsaData.wVersion) != 1) {                        \
                                                                           \
                ss_info_assert(0,                                          \
                               ("WinSock version is %d.%d; 1.1 is needed", \
                               LOBYTE(wsaData.wVersion),                   \
                               HIBYTE(wsaData.wVersion)));                 \
            }                                                              \
        } while (0);
#  define ss_socket_globaldone()   WSACleanup()
#  define ss_socket_geterror()     WSAGetLastError()
#  define SS_SOCKET_INVALIDSOCKET  INVALID_SOCKET
/* default */
#  define ss_socket_new(a,b,c)     socket((a),(b),(c))
#  define ss_socket_close(a)       close(a)

#else /* unixes and all the good guys */
#  include <sys/socket.h>     /* socket */
#  include <unistd.h>         /* close */
#  include <errno.h>
/* init macros */
#  define ss_socket_globalinit()
#  define ss_socket_new(a,b,c)     socket((a),(b),(c))
#  define ss_socket_close(a)       close(a)
#  define ss_socket_globaldone()
#  define ss_socket_geterror()     (errno)
#  define SS_SOCKET_INVALIDSOCKET  (-1)

#endif /* socket.h */

#endif /* SSSOCKET_H */

/* EOF */
