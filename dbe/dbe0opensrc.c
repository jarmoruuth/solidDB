/*************************************************************************\
**  source       * dbe0opensrc.h
**  directory    * dbe
**  description  * DBE-level stubs for solidDB opensource
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
#include <ssmem.h>
#include <ssdebug.h>

#include "dbe0crypt.h"
#include "dbe0hsbstate.h"

struct dbe_cryptoparams_st {
        char *dummy;
};

dbe_cryptoparams_t* dbe_crypt_init(void)
{
        return(NULL);
}

void dbe_crypt_done(
        dbe_cryptoparams_t* cp __attribute__ ((unused)))
{
}

bool dbe_crypt_setmode(
        dbe_cryptoparams_t* cp __attribute__ ((unused)),
        char *password __attribute__ ((unused)),
        char *old_password __attribute__ ((unused)),
        bool encrypt_db __attribute__ ((unused)),
        bool decrypt_db __attribute__ ((unused)))
{
        return TRUE;
}

cpar_mode_t dbe_crypt_getmode(
        dbe_cryptoparams_t* cp __attribute__ ((unused)))
{
        return(CPAR_NONE);
}

void dbe_crypt_setcipher(
        dbe_cryptoparams_t* cryptopars __attribute__ ((unused)),
        su_cipher_t* cipher __attribute__ ((unused)))
{
}

void dbe_crypt_setdecrypt(
        dbe_cryptoparams_t* cryptopars __attribute__ ((unused)),
        dbe_decrypt_t decrypt __attribute__ ((unused)))
{
}

void dbe_crypt_setencrypt(
        dbe_cryptoparams_t* cryptopars __attribute__ ((unused)),
        dbe_encrypt_t encrypt __attribute__ ((unused)))
{
}

dbe_encrypt_t dbe_crypt_getencrypt(
        dbe_cryptoparams_t* cryptopars __attribute__ ((unused)))
{
        return(NULL);
}

dbe_decrypt_t dbe_crypt_getdecrypt(
        dbe_cryptoparams_t* cryptopars __attribute__ ((unused)))
{
        return(NULL);
}

su_cipher_t* dbe_crypt_getcipher(
        dbe_cryptoparams_t* cryptopars __attribute__ ((unused)))
{
        return(NULL);
}

char *dbe_crypt_getpasswd(
        dbe_cryptoparams_t* cryptopars __attribute__ ((unused)))
{
        return(NULL);
}

char *dbe_crypt_getoldpasswd(
        dbe_cryptoparams_t* cryptopars __attribute__ ((unused)))
{
        return(NULL);
}

dbe_hsbstate_t *dbe_hsbstate_init(
        dbe_hsbstatelabel_t label __attribute__ ((unused)), 
        dbe_db_t* db __attribute__ ((unused)))
{
        return(NULL);
}

void dbe_hsbstate_done(dbe_hsbstate_t* state __attribute__ ((unused)))
{
}

dbe_hsbmode_t dbe_hsbstate_getdbehsbmode(dbe_hsbstate_t* state __attribute__ ((unused)))
{
        return(DBE_HSB_STANDALONE);
}

bool dbe_hsbstate_is2safe(
        dbe_hsbstate_t* state __attribute__ ((unused)))
{
        return(FALSE);
}

void dbe_hsbstate_entermutex(dbe_hsbstate_t* state __attribute__ ((unused)))
{
}

void dbe_hsbstate_exitmutex(dbe_hsbstate_t* state __attribute__ ((unused)))
{
}

hsb_role_t dbe_hsbstate_getrole(dbe_hsbstate_t* state __attribute__ ((unused)))
{
        return(HSB_ROLE_STANDALONE);
}

bool dbe_hsbstate_isstandaloneloggingstate(
        dbe_hsbstatelabel_t new_state __attribute__ ((unused)))
{
        return(TRUE);
}

char* dbe_hsbstate_getstatestring(
        dbe_hsbstatelabel_t state_label __attribute__ ((unused)))
{
        return((char *)"HSB_STATE_STANDALONE");
}

char* dbe_hsbstate_getrolestring(hsb_role_t role __attribute__ ((unused)))
{
        return((char *)"HSB_ROLE_STANDALONE");
}

char* dbe_hsbstate_getrolestring_user(hsb_role_t role __attribute__ ((unused)))
{
        return((char *)"STANDALONE");
}

