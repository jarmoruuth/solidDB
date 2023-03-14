/*************************************************************************\
**  source       * su0chcvt.h
**  directory    * su
**  description  * Character translation tables for solid
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


#ifndef SU0CHCVT_H
#define SU0CHCVT_H

#include <ssc.h>
#include <su0bflag.h>

typedef enum {
        SU_CHSET_DEFAULT    = 0,
        SU_CHSET_NOCNV      = 1,
        SU_CHSET_ANSI       = 2,
        SU_CHSET_PCOEM      = 3,
        SU_CHSET_7BITSCAND  = 4
} su_chset_t;

typedef enum {
        SU_CHCOLLATION_NOTVALID     = -1,
        SU_CHCOLLATION_ISO8859_1    = 0,
        SU_CHCOLLATION_FIN          = 1
} su_chcollation_t;

/* flag values for su_chcvt_sqlstruprquotif() */
#define SU_CHCVT_UPCASE_DEFAULT     0
#define SU_CHCVT_UPCASE_QUOTED      SU_BFLAG_BIT(0)
#define SU_CHCVT_CONVERT_ONLY       SU_BFLAG_BIT(1)

#define SU_SQLISLOWER(c)  ((uint)((int)(c) - (int)'a') <= (uint)((int)'z' - (int)'a'))

extern bool su_chcvt_upcase_quoted;

su_chcollation_t su_chcollation_byname(char* chcollation_name);
char* su_chcollation_name(su_chcollation_t chcollation);

uchar* su_chcvt_clienttoserver_init(
        su_chset_t chset,
        su_chcollation_t chcollation);
uchar* su_chcvt_servertoclient_init(
        su_chset_t chset,
        su_chcollation_t chcollation);
uchar* su_chcvt_servertoupper_init(
        su_chcollation_t chcollation);
uchar* su_chcvt_servertolower_init(
        su_chcollation_t chcollation);

void su_chcvt_done(
        uchar* table);

char* su_chcvt_strcvtuprdup(
        char* s,
        su_chcollation_t chcollation);

bool su_chcvt_sqlstrupr(
        char* s,
        uchar* client2server,
        uchar* server2client,
        char state_init_char,
        su_chcollation_t chcollation);

bool su_chcvt_sqlstruprquotif(
        char* s,
        uchar* client2server,
        uchar* server2client,
        char state_init_char,
        su_chcollation_t chcollation,
        su_bflag_t flags);

su_chset_t su_chcvt_inifilechset(void);

void su_chcvt_bin2hex(
        ss_char1_t* dest,
        ss_byte_t* src,
        size_t nbytes_src);

bool su_chcvt_hex2bin(
        ss_byte_t* dest,
        ss_char1_t* src,
        size_t nbytes_dest);

void su_chcvt_bin2hexchar2(
        ss_char2_t* dest,
        ss_byte_t* src,
        size_t nbytes);

bool su_chcvt_hex2binchar2(
        ss_byte_t* dest,
        ss_char2_t* src,
        size_t nbytes_dest);

void su_chcvt_bin2hexchar2_va(
        ss_char2_t* dest,
        ss_byte_t* src,
        size_t nbytes);

bool su_chcvt_hex2binchar2_va(
        ss_byte_t* dest,
        ss_char2_t* src,
        size_t nbytes);

#endif /* SU0CHCVT_H */
