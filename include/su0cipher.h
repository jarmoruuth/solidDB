/*************************************************************************\
**  source       * su0cipher.h
**  directory    * su
**  description  * Database file ecryption 
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


#ifndef SU0CIPHER_H
#define SU0CIPHER_H

enum su_cryptoalg_e {
        SU_CRYPTOALG_NONE = 0,
        SU_CRYPTOALG_APP = 1,
        SU_CRYPTOALG_DES = 2,
        SU_CRYPTOALG_MAX = SU_CRYPTOALG_DES
};

typedef enum su_cryptoalg_e su_cryptoalg_t;

#define SU_CRYPTOKEY_SIZE 32

typedef struct su_cipher_s su_cipher_t;

void su_cipher_generate(ss_uint1_t *key);

su_cipher_t* su_cipher_init(
        su_cryptoalg_t alg,
        ss_uint1_t* key,
        ss_char_t *password);

void su_cipher_done(su_cipher_t* cipher);

void su_cipher_change_password(
        ss_uint1_t* key,
        ss_char_t *password,
        ss_char_t *old_password);

void su_cipher_encrypt_page(su_cipher_t* cipher, void* page, ss_uint4_t size);
void su_cipher_decrypt_page(su_cipher_t* cipher, void* page, ss_uint4_t size);
ss_uint1_t* su_cipher_getkey(su_cipher_t* cipher);

#endif /* SU0CIPHER_H */
