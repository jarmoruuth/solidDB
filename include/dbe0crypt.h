/*************************************************************************\
**  source       * dbe0crypt.h
**  directory    * dbe
**  description  * DBE-level encryption.
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


#ifndef DBE0CRYPT_H
#define DBE0CRYPT_H

#include <su0types.h>
#include <su0cipher.h>
#include <dbe0type.h>

typedef enum { CPAR_NONE, CPAR_ENCRYPT, CPAR_ENCRYPTED,
               CPAR_CHANGEPASSWORD, CPAR_DECRYPT } cpar_mode_t;

typedef struct dbe_cryptoparams_st dbe_cryptoparams_t;

dbe_cryptoparams_t* dbe_crypt_getcryptopars(void);
void dbe_crypt_setcryptopars(dbe_cryptoparams_t* cp);

dbe_cryptoparams_t* dbe_crypt_init(void);

void dbe_crypt_done(dbe_cryptoparams_t* cp);

bool dbe_crypt_setmode(
        dbe_cryptoparams_t* cryptopars,
        char *password,
        char *old_password,
        bool encrypt_db,
        bool decrypt_db);

cpar_mode_t dbe_crypt_getmode(
        dbe_cryptoparams_t* cryptopars);

void dbe_crypt_setencrypt(
        dbe_cryptoparams_t* cryptopars,
        dbe_encrypt_t cpar_encrypt);

void dbe_crypt_setdecrypt(
        dbe_cryptoparams_t* cryptopars,
        dbe_decrypt_t decrypt);

void dbe_crypt_setcipher(
        dbe_cryptoparams_t* cryptopars,
        su_cipher_t* cipher);

dbe_encrypt_t dbe_crypt_getencrypt(
        dbe_cryptoparams_t* cryptopars);

dbe_decrypt_t dbe_crypt_getdecrypt(
        dbe_cryptoparams_t* cryptopars);

su_cipher_t* dbe_crypt_getcipher(
        dbe_cryptoparams_t* cryptopars);

char *dbe_crypt_getpasswd(
        dbe_cryptoparams_t* cryptopars);

char *dbe_crypt_getoldpasswd(
        dbe_cryptoparams_t* cryptopars);

#endif /* DBE0CRYPT_H */
