/*************************************************************************\
**  source       * su0opensrc.c
**  directory    * su
**  description  * SU-level stubs for solidDB opensource
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


#include <ssc.h>
#include <ssdebug.h>
#include <ssstring.h>
#include <ssmem.h>

#include <su0cipher.h>

struct su_cipher_s {
        char* dummy;
};

su_cipher_t* su_cipher_init(
        su_cryptoalg_t alg __attribute__ ((unused)),
        ss_uint1_t* key __attribute__ ((unused)),
        ss_char_t *password __attribute__ ((unused)))
{
        return(NULL);
}

void su_cipher_done(su_cipher_t* cipher __attribute__ ((unused)))
{
}

void su_cipher_change_password(
        ss_uint1_t* key __attribute__ ((unused)),
        ss_char_t *password __attribute__ ((unused)),
        ss_char_t *old_password __attribute__ ((unused)))
{
}

void su_cipher_encrypt_page(
        su_cipher_t* cipher __attribute__ ((unused)),
        void* page __attribute__ ((unused)),
        ss_uint4_t size __attribute__ ((unused)))
{
}

void su_cipher_decrypt_page(
        su_cipher_t* cipher __attribute__ ((unused)),
        void* page __attribute__ ((unused)),
        ss_uint4_t size __attribute__ ((unused)))
{
}

void su_cipher_generate(ss_uint1_t *key __attribute__ ((unused)))
{
}

ss_uint1_t* su_cipher_getkey(
        su_cipher_t* cipher __attribute__ ((unused)))
{
        return(NULL);
}

