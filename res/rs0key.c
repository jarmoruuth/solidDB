/*************************************************************************\
**  source       * rs0key.c
**  directory    * res
**  description  * Key handling services
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
This module implements the general key object.

Limitations:
-----------


Error handling:
--------------
None

Objects used:
------------
Attribute type  <rs0atype.h>
Attribute value <rs0aval.h>

Preconditions:
-------------


Multithread considerations:
--------------------------
Code is fully re-entrant.
The same key object can not be used simultaneously from many threads.


Example:
-------
See tkey.c


**************************************************************************
#endif /* DOCUMENTATION */

#define RS0KEY_C
#define RS_INTERNAL

#include <ssstdio.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <su0parr.h>
#include <su0bflag.h>

#include <uti0va.h>

#include "rs0types.h"
#include "rs0atype.h"
#include "rs0aval.h"
#include "rs0auth.h"
#include "rs0key.h"
#include "rs0sdefs.h"
#include "rs0sysi.h"

#define KEY_COST_UNKNOWN  (-1.0)

#ifdef NO_ANSI

static void rs_keypart_done();

#else /* NO_ANSI */

static void rs_keypart_done(void* cd, rs_keypart_t* kp);

#endif /* NO_ANSI */

/*##**********************************************************************\
 * 
 *		rs_key_init
 * 
 * Allocates and initializes a key object.
 * The parts of the key can be defined by using rs_key_addpart.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	keyname - in, use
 *		name of the key
 *
 *      key_id - in
 *		key id
 *
 *	unique - in
 *		TRUE, if key is unique
 *
 *	clustering - in
 *		TRUE, if key is clustering
 *
 *	primary - in
 *		TRUE, if key is primary key
 *
 *	prejoined - in
 *		TRUE, if key is prejoined key
 *
 *      nordering - in
 *		number of key parts that are used for ordering
 *          in a key
 *
 *	auth - in
 *		authorization id for the key, or NULL
 * 
 * Return value - give : 
 * 
 *      pointer to a newly allocated key
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_key_t* rs_key_init(cd, keyname, key_id, unique, clustering, primary,
                      prejoined, nordering, auth)
        void*       cd;
        char*       keyname;
        ulong       key_id;
        bool        unique;
        bool        clustering;
        bool        primary;
        bool        prejoined;
        uint        nordering;
        rs_auth_t*  auth;
{
        rs_key_t* key;

        SS_NOTUSED(cd);
        SS_NOTUSED(auth);
        ss_dassert(keyname != NULL);
        ss_dassert(nordering > 0);
        ss_dassert(primary ? unique : TRUE); /* Primary key must be unique. */

        key = SSMEM_NEW(rs_key_t);

        ss_debug(key->k_check = RSCHK_KEYTYPE;)
        key->k_nlink = 1;
        key->k_name = SsMemStrdup(keyname);
        key->k_id = key_id;
        key->k_nordering = nordering;
        key->k_type = RS_KEY_NORMAL;
        key->k_maxstoragelen = SS_INT4_MAX;
        key->k_action = 0;
        key->k_index_ready = FALSE;
        key->k_refrelid = 0;

        key->k_flags = 0;
        if (unique) {
            SU_BFLAG_SET(key->k_flags, RSKF_UNIQUE | RSKF_PHYSICALLY_UNIQUE);
        }
        if (clustering) {
            SU_BFLAG_SET(key->k_flags, RSKF_CLUSTERING | RSKF_PHYSICALLY_UNIQUE);
        }
        if (primary) {
            /* Set also unique flag, this fixes a bug in system relations. */
            SU_BFLAG_SET(key->k_flags,
                         RSKF_PRIMARY | RSKF_UNIQUE | RSKF_PHYSICALLY_UNIQUE);
        }
        if (prejoined) {
            SU_BFLAG_SET(key->k_flags, RSKF_PREJOINED);
        }

        key->k_nparts = 0;
        key->k_parts = SsMemAlloc(sizeof(key->k_parts[0]));
        key->k_part_by_ano = su_pa_init();
        key->k_costfactor = KEY_COST_UNKNOWN;
        key->k_mmeindex = NULL;
        key->k_refkeys = NULL;
        key->k_maxrefkeypartno = -1;
        key->k_sem = rs_sysi_getrslinksem(cd);
        
        return(key);
}

/*#***********************************************************************\
 * 
 *		rs_keypart_done
 * 
 * Releases resources allocated for a key part.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	kp - in, take
 *		key part
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void rs_keypart_done(cd, kp)
        void*         cd;
        rs_keypart_t* kp;
{
        SS_NOTUSED(cd);
        KP_CHECK(kp);

        if (kp->kp_constaval != NULL) {
            rs_aval_free(cd, kp->kp_constatype, kp->kp_constaval);
            rs_atype_free(cd, kp->kp_constatype);
        }
}

/*##**********************************************************************\
 * 
 *		rs_key_done
 * 
 * Frees a key object.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, take
 *		pointer to key
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_key_done(cd, key)
        void* cd;
	    rs_key_t* key;
{
        uint i;

        KEY_CHECK(key);

        SsSemEnter(key->k_sem);

        ss_dassert(key->k_nlink > 0);
        key->k_nlink--;

        if (key->k_nlink == 0) {

            SsSemExit(key->k_sem);
            
            for (i = 0; i < (uint)key->k_nparts; i++) {
                rs_keypart_done(cd, &key->k_parts[i]);
            }
            SsMemFree(key->k_parts);
            su_pa_done(key->k_part_by_ano);
            if (key->k_refkeys != NULL) {
                rs_key_t*   refkey;
                
                su_pa_do_get(key->k_refkeys, i, refkey) {
                    rs_key_done(cd, refkey);
                }
                su_pa_done(key->k_refkeys);
            }
            SsMemFree(key->k_name);
            SsMemFree(key);

        } else {

            SsSemExit(key->k_sem);
        }
}

/*##**********************************************************************\
 * 
 *		rs_key_link
 * 
 * Adds reference to the key object.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in out, use
 *		pointer to key
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_key_link(cd, key)
        void* cd;
	rs_key_t* key;
{
        SS_NOTUSED(cd);

        KEY_CHECK(key);

        SsSemEnter(key->k_sem);

        ss_dassert(key->k_nlink > 0);

        key->k_nlink++;

        SsSemExit(key->k_sem);
}

/*##**********************************************************************\
 * 
 *		rs_key_addpart
 * 
 * Adds a new part to the key at the specified index
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in out, use
 *		pointer to key
 *
 *	kpindex - in, use
 *		index of the new key part to be, starting from 0
 *
 *	kptype - in, use
 *		type of the key part
 *
 *	asc - in, use
 *		TRUE, if key is ascending according to this part
 *
 *	ano - in, use
 *		attribute index in ttype of the relation
 *
 *	constvalue - in, use
 *		constant va, NULL if this key part is not
 *          constant
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_key_addpart(
        void*           cd,
        rs_key_t*       key,
        rs_ano_t        kpindex,
        rs_attrtype_t   kptype,
        bool            asc,
        rs_ano_t        ano,
        void*           constvalue_or_collation)
{
        rs_keypart_t* kp;

        SS_NOTUSED(cd);
        KEY_CHECK(key);

        ss_assert(kpindex == key->k_nparts);
        ss_dassert(!SU_BFLAG_TEST(key->k_flags, RSKF_READONLY));

        key->k_nparts++;
        key->k_parts = SsMemRealloc(
                            key->k_parts,
                            key->k_nparts * sizeof(key->k_parts[0]));

        kp = &key->k_parts[kpindex];

        ss_debug(kp->kp_check = RSCHK_KEYPARTTYPE;)
        kp->kp_type = kptype;
        kp->kp_ascending = asc;
        kp->kp_ano = ano;
        kp->kp_constaval = NULL;
        kp->kp_constva = NULL;
        kp->kp_index = kpindex;
#ifdef SS_COLLATION
        kp->kp_collation = NULL;
        kp->kp_prefixlength = 0;
#endif /* SS_COLLATION */

        KP_CHECK(kp);

        if (kptype == RSAT_TUPLE_VERSION) {
            SU_BFLAG_SET(key->k_flags, RSKF_ISTUPLEVERSION);
        }
#ifdef SS_COLLATION
        FAKE_CODE_BLOCK(FAKE_RES_ELBONIAN_COLLATION,
            if (kptype == RSAT_COLLATION_KEY
            &&  constvalue_or_collation == NULL)
            {
                constvalue_or_collation = &su_collation_fake_elbonian;
            });
#endif /* SS_COLLATION */

        if (constvalue_or_collation != NULL) {
#ifdef SS_COLLATION
            if (kptype == RSAT_COLLATION_KEY) {
                ss_dassert(constvalue_or_collation != NULL);
                kp->kp_collation = constvalue_or_collation;
            } else
#endif /* SS_COLLATION */
            {
                rs_keyp_setconstvalue(cd, key, kpindex, constvalue_or_collation);
            }
        } else {
#ifdef SS_COLLATION
            ss_dassert(kptype != RSAT_COLLATION_KEY);
#endif /* SS_COLLATION */
            if (ano != RS_ANO_NULL) {
                su_pa_insertat(key->k_part_by_ano, (uint)ano,
                               (void*)(kpindex + 1));
            }
        }
}

#ifndef rs_key_first_datapart
/*##**********************************************************************\
 *
 *      rs_key_first_datapart
 *
 * Returns the index of the first user defined and user visible data part
 * in the key.
 *
 * Parameters:
 *      cd - in, use
 *          client data
 *
 *      key - in, use
 *          the key
 *
 * Return value:
 *      Index of the first user visible part.
 *
 * Limitations:
 *
 * Globals used:
 */
rs_ano_t rs_key_first_datapart(
        void*       cd __attribute__ ((unused)),
        rs_key_t*   key)
{
        KEY_CHECK(key);

        ss_dassert(key->k_parts[0].kp_type == RSAT_KEY_ID);

        return _RS_KEY_FIRST_DATAPART_(cd, key);
}
#endif /* !defined(rs_key_first_datapart) */

/*##**********************************************************************\
 * 
 *		rs_key_setid
 * 
 * Sets the key id of a key.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in out, use
 *		pointer to key
 *
 *	keyid - in
 *		key id
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_key_setid(
        void*     cd,
        rs_key_t* key,
        ulong     keyid)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        key->k_id = keyid;
}

/*##**********************************************************************\
 * 
 *		rs_key_name
 * 
 * Returns the name of the key.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 *
 * Return value - ref :    
 * 
 *      key name
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* rs_key_name(cd, key)
        void*     cd;
        rs_key_t* key;
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return(key->k_name);
}

#ifndef rs_key_lastordering
/*##**********************************************************************\
 * 
 *		rs_key_lastordering
 * 
 * Returns the key part index of the last key part which is relevant
 * to the ordering in a key. This function is used e.g. to test if
 * the constraints determine a unique row in a query:
 * 
 *         UNIQUE KEY ARI (FAMILYNAME, FIRSTNAME, CODE)
 *         
 * here the key part index of CODE is the answer.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 * 
 * Return value : 
 * 
 *      key part index of the last ordering key part
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ano_t rs_key_lastordering(cd, key)
        void*     cd;
        rs_key_t* key;
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return _RS_KEY_LASTORDERING_(cd, key);
}
#endif /* !defined(rs_key_lastordering) */

#ifndef rs_key_isclustering
/*##**********************************************************************\
 * 
 *		rs_key_isclustering
 * 
 * Checks if the given key is a clustering key.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 *
 * Return value : 
 * 
 *      TRUE, if clustering key
 *      FALSE, if not
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_key_isclustering(cd, key)
        void* cd;
	rs_key_t* key;
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return _RS_KEY_ISCLUSTERING_(cd, key);
}
#endif /* !defined(rs_key_isclustering) */

/*##**********************************************************************\
 * 
 *		rs_key_isprejoined
 * 
 * Checks if the given key is a prejoined key.
 * 
 * key_isprejoined() is TRUE if the key is prejoined to another key.
 * This means that the key is mixed in the index leaves with another
 * key in another table. This can be found out by looking at the first
 * entry of the key definition. If it is the key id, it is not prejoined,
 * else it is a user-defined cluster id, and the key is probably
 * prejoined.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 *
 * Return value : 
 * 
 *      TRUE, if prejoined key
 *      FALSE, if not
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_key_isprejoined(cd, key)
        void* cd;
	rs_key_t* key;
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return(SU_BFLAG_TEST(key->k_flags, RSKF_PREJOINED));
}

#ifndef rs_key_isunique
/*##**********************************************************************\
 * 
 *		rs_key_isunique
 * 
 * Checks if the given key is unique.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 * 
 * Return value : 
 * 
 *      TRUE, if unique key
 *      FALSE, if not
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_key_isunique(cd, key)
        void* cd;
	rs_key_t* key;
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return _RS_KEY_ISUNIQUE_(cd, key);
}
#endif /* !defined(rs_key_isunique) */

#ifndef rs_key_isprimary
/*##**********************************************************************\
 * 
 *		rs_key_isprimary
 * 
 * Checks if the given key is primary.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 * 
 * Return value : 
 * 
 *      TRUE, if primary key
 *      FALSE, if not
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_key_isprimary(cd, key)
        void* cd;
        rs_key_t* key;
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return _RS_KEY_ISPRIMARY_(cd, key);
}
#endif /* !defined(rs_key_isprimary) */

/*##**********************************************************************\
 * 
 *		rs_key_setaborted
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cd - 
 *		
 *		
 *	key - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_key_setaborted(cd, key)
        void* cd;
	rs_key_t* key;
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        SU_BFLAG_SET(key->k_flags, RSKF_ABORTED);
}

/*##**********************************************************************\
 * 
 *		rs_key_settype
 * 
 * Sets the key type.
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	key - use
 *		
 *		
 *	type - in
 *          Key type, one of RS_KEY_NORMAL, RS_KEY_FORKEYCHK or
 *          RS_KEY_PRIMKEYCHK.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_key_settype(
        void*     cd,
        rs_key_t* key,
        rs_keytype_t type)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        ss_dassert(type == RS_KEY_FORKEYCHK || type == RS_KEY_PRIMKEYCHK);

        key->k_type = type;
}

/*##**********************************************************************\
 * 
 *		rs_key_setmaxstoragelen
 * 
 * Sets max storage length for a key.
 * 
 * Parameters : 
 * 
 *	cd - 
 *		
 *		
 *	key - 
 *		
 *		
 *	maxlen - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_key_setmaxstoragelen(
        void*     cd,
        rs_key_t* key,
        long      maxlen)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        key->k_maxstoragelen = maxlen;
}

void rs_key_setmaxrefkeypartno(
        void*     cd,
        rs_key_t* key,
        int       maxkpno)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);
        ss_dassert(key->k_maxrefkeypartno == -1 || key->k_maxrefkeypartno == maxkpno);

        key->k_maxrefkeypartno = maxkpno;
}

/*##**********************************************************************\
 *      rs_key_issubkey
 *
 * Check if all the components of key1 are also componets of key2.
 *
 *  cd -
 *
 *  key1 - key to check
 *
 *  key2 - key to check with
 *
 * Comments : This is useful to detect duplicated and "implied" unique keys.
 *
 * Globals used :
 *
 * See also :
 */

bool rs_key_issamekey (
        void*     cd,
        rs_key_t* key1,
        rs_key_t* key2,
        bool issubkey)
{
        int i;

        SS_NOTUSED(cd);
        KEY_CHECK(key1);
        KEY_CHECK(key2);

        for (i=0; i<key1->k_nparts && i<key2->k_nparts; i++) {
            if (key1->k_parts[i].kp_type == RSAT_KEY_ID &&
                key2->k_parts[i].kp_type == RSAT_KEY_ID)
            {
                /* Move on, do nothing */
            } else if (key1->k_parts[i].kp_type == RSAT_USER_DEFINED &&
                       key2->k_parts[i].kp_type == RSAT_USER_DEFINED &&
                       key1->k_parts[i].kp_ano == key2->k_parts[i].kp_ano && 
                       key1->k_parts[i].kp_ascending == key2->k_parts[i].kp_ascending)
            {
                /* Move on, do nothing */
            } else if (key1->k_parts[i].kp_type == RSAT_TUPLE_VERSION &&
                       key2->k_parts[i].kp_type == RSAT_TUPLE_VERSION)
            {
                /* End of user-defined part reached. */
                return TRUE;
            } else if(issubkey && key1->k_parts[i].kp_type == RSAT_TUPLE_VERSION)
            {
                /* End of user-defined part reached for key1. */
                return TRUE;
#ifdef DBE_UPDATE_OPTIMIZATION
            } else if (key1->k_nordering == i &&
                       key2->k_nordering == i)
            {
                /* End of user-defined part reached. */
                return TRUE;
            } else if(issubkey && key1->k_nordering == i)
            {
                /* End of user-defined part reached for key1. */
                return TRUE;
#endif /* DBE_UPDATE_OPTIMIZATION */
            } else 
            {
                return FALSE;
            }
        }
        return key1->k_nparts == key2->k_nparts;
}

#if !defined(RS_USE_MACROS)

/*##**********************************************************************\
 * 
 *		rs_keyp_parttype
 * 
 * Returns the type of kpindex'th part in the key.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 *
 *	kpindex - in, use
 *		key part index. The order number of the key part in
 *          question. Numbering starts from 0.
 * 
 * Return value : 
 * 
 *      Key part type. One of enumerated rs_attrtype_t items.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_attrtype_t rs_keyp_parttype(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex)
{
        rs_keypart_t* kp;

        SS_NOTUSED(cd);
        KEY_CHECK(key);

        ss_dassert((uint)kpindex < (uint)key->k_nparts);

        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);

        return(kp->kp_type);
}

#endif /* !defined(RS_USE_MACROS) */

#ifdef SS_DEBUG
/*##**********************************************************************\
 * 
 *		rs_keyp_ano
 * 
 * Returns the index of the attribute in the corresponding relation.
 * Should be called only when the key part in kpindex is not constant
 * (this is not checked, though).
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 *
 *	kpindex - in, use
 *		key part index. The order number of the key part in
 *          question. Numbering starts from 0.
 *
 * Return value : 
 * 
 *      Attribute index in relation, numbering starts from 0
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ano_t rs_keyp_ano(cd, key, kpindex)
        void* cd;
	rs_key_t* key;
	rs_ano_t kpindex;
{
        rs_keypart_t* kp;

        SS_NOTUSED(cd);
        KEY_CHECK(key);

        ss_dassert((uint)kpindex < (uint)key->k_nparts);

        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);

        return(kp->kp_ano);
}
#endif /* SS_DEBUG */

#if !defined(RS_USE_MACROS)
/*##**********************************************************************\
 * 
 *		rs_keyp_isconstvalue
 * 
 * Checks if given key part is a constant key part
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *	    client data	
 *		
 *	key - in, use
 *		Pointer to key
 *		
 *	kpindex - in, use
 *		key part index. The order number of the key part in
 *          question. Numbering starts from 0.
 *		
 * Return value : 
 *      TRUE, if key part is a constant 
 *      FALSE, if not
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_keyp_isconstvalue(cd, key, kpindex)
        void*     cd;
	rs_key_t* key;
	rs_ano_t  kpindex;
{
        rs_keypart_t* kp;

        SS_NOTUSED(cd);
        KEY_CHECK(key);

        ss_dassert((uint)kpindex < (uint)key->k_nparts);

        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);

        return(_RS_KEYP_ISCONSTVALUE_(key, kpindex));
}
#endif /* !defined(RS_USE_MACROS) */

/*##**********************************************************************\
 * 
 *		rs_keyp_setconstvalue
 * 
 * Sets the constant value of the key part. If constant value is
 * not defined for the key part in kpindex, va should be NULL.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in out, use
 *		pointer to key
 *
 *	kpindex - 
 *		key part index. The order number of the key part in
 *          question. Numbering starts from 0.
 *
 *      va - in, use
 *		constant value, or NULL
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_keyp_setconstvalue(cd, key, kpindex, va)
        void*     cd;
	rs_key_t* key;
	rs_ano_t  kpindex;
	va_t*     va;
{
        rs_keypart_t* kp;

        SS_NOTUSED(cd);
        KEY_CHECK(key);

        ss_dassert((uint)kpindex < (uint)key->k_nparts);

        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);

        if (kp->kp_constaval != NULL) {
            rs_aval_free(cd, kp->kp_constatype, kp->kp_constaval);
            rs_atype_free(cd, kp->kp_constatype);
        }

        if (va == NULL) {

            kp->kp_constatype = NULL;
            kp->kp_constaval = NULL;
            kp->kp_constva = NULL;

        } else {

            rs_atype_t* atype;
            rs_aval_t* aval;
            ulong len = 0;
            ulong scale = 0;
            rs_datatype_t    datatype = 0;
            rs_sqldatatype_t sqldatatype = 0;

            switch (kp->kp_type) {
                case RSAT_FREELY_DEFINED:
                case RSAT_CLUSTER_ID:
                    datatype = RSDT_CHAR;
                    sqldatatype = RSSQLDT_VARCHAR;
                    len = 0;
                    scale = 0;
                    break;
                case RSAT_RELATION_ID:
                case RSAT_KEY_ID:
                    datatype = RSDT_INTEGER;
                    sqldatatype = RSSQLDT_INTEGER;
                    len = RS_INT_PREC;
                    scale = RS_INT_SCALE;
                    break;
                default:
                    ss_error;
            }
            atype = rs_atype_init(
                        cd,
                        kp->kp_type,
                        datatype,
                        sqldatatype,
                        len,
                        scale,
                        TRUE);
            aval = rs_aval_create(cd, atype);
            rs_aval_setva(cd, atype, aval, va);

            kp->kp_constatype = atype;
            kp->kp_constaval = aval;
            if (aval == NULL) {
                kp->kp_constva = NULL;
            } else {
                kp->kp_constva = rs_aval_va(cd, atype, aval);
            }
        }
}

#if !defined(RS_USE_MACROS)
/*##**********************************************************************\
 * 
 *		rs_keyp_constvalue
 * 
 * Returns the constant value of the key part. If constant value is
 * not defined for the key part in kpindex, returns NULL.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 *
 *	kpindex - in, ues
 *		key part index. The order number of the key part in
 *          question. Numbering starts from 0.
 *
 * Return value - ref : 
 * 
 *      a va containing the constant value 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* rs_keyp_constvalue(cd, key, kpindex)
        void*     cd;
	rs_key_t* key;
	rs_ano_t  kpindex;
{
        rs_keypart_t* kp;

        SS_NOTUSED(cd);
        KEY_CHECK(key);

        ss_dassert((uint)kpindex < (uint)key->k_nparts);

        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);

        return(_RS_KEYP_CONSTVALUE_(key, kpindex));
}
#endif /* !defined(RS_USE_MACROS) */

#ifndef rs_key_isrefmme
bool rs_key_isrefmme(
        void*           cd __attribute__ ((unused)),
        rs_key_t*       key)
{
        KEY_CHECK(key);
        ss_dassert(key->k_type == RS_KEY_FORKEYCHK
                   || key->k_type == RS_KEY_PRIMKEYCHK);

        return _RS_KEY_ISREFMME_(cd, key);
}
#endif /* defined(rs_key_isrefmme) */

void rs_key_setrefmme(
        void*           cd __attribute__ ((unused)),
        rs_key_t*       key,
        bool            flag)
{
        KEY_CHECK(key);
        ss_dassert(key->k_type == RS_KEY_FORKEYCHK
                   || key->k_type == RS_KEY_PRIMKEYCHK);

        if (flag) {
            SU_BFLAG_SET(key->k_flags, RSKF_REFMME);
        } else {
            SU_BFLAG_CLEAR(key->k_flags, RSKF_REFMME);
        }
}

#ifndef rs_key_isreftemporary
bool rs_key_isreftemporary(
        void*           cd __attribute__ ((unused)),
        rs_key_t*       key)
{
        KEY_CHECK(key);
        ss_dassert(key->k_type == RS_KEY_FORKEYCHK
                   || key->k_type == RS_KEY_PRIMKEYCHK);

        return _RS_KEY_ISREFTEMPORARY_(cd, key);
}
#endif /* defined(rs_key_isreftemporary) */

/*#***********************************************************************\
 *
 *      rs_key_setaction
 * 
 * Set action value associated with the key (for foreign keys).
 *
 * Parameters : 
 *
 *      cd - in, use
 *      client data
 *
 *  key - in, use
 *      key to update
 *
 *  action - in, use
 *      action value (two lower bytes are used).
 *
 * Return value : 
 *
 * Limitations  :
 *      Makes sence only for foreign keys.
 *
 * Globals used :
 */
void rs_key_setaction(
        rs_sysi_t*  cd __attribute__ ((unused)),
        rs_key_t*   key,
        int         action)
{
        KEY_CHECK(key);
        key->k_action = action;
}

/*##**********************************************************************\
 *
 *      rs_key_index_ready
 *      
 * Checks if the key can be used for index search.
 * 
 * Parameters :
 * 
 *  cd - in, use
 *      client data
 *      
 *  key - in, use
 *      key 
 *      
 * Return value :
 * 
 * Limitations  :
 *      
 * Globals used :
 * 
 * See also:
 */     
bool rs_key_index_ready (
        rs_sysi_t*  cd __attribute__ ((unused)),
        rs_key_t*   key)
{
        KEY_CHECK(key);
        return key->k_index_ready;
}

/*##**********************************************************************\
 *
 *      rs_key_setindex_ready
 *
 * Set key flag - key can be used for index search.
 * 
 * Parameters :
 *
 *  cd - in, use
 *      client data
 *  
 *  key - in, use
 *      key 
 *  
 * Return value :
 *
 * Limitations  :
 *  
 * Globals used :
 *
 * See also:
 */ 
void rs_key_setindex_ready (
        rs_sysi_t*  cd __attribute__ ((unused)),
        rs_key_t*   key)
{
        KEY_CHECK(key);
        key->k_index_ready = TRUE;
}


/*##**********************************************************************\
 *
 *      rs_key_setrefrelid
 *      
 * Set referencing relid for foreign keys.
 * 
 * Parameters :
 * 
 *  cd - in, use
 *      client data
 *      
 *  key - in, use
 *      key 
 *
 *  relid - in, use
 *      referencing table id
 *      
 * Return value :
 * 
 * Limitations  :
 *      
 * Globals used :
 * 
 * See also:
 */     
void rs_key_setrefrelid(
        rs_sysi_t*  cd __attribute__ ((unused)),
        rs_key_t*   key,
        ulong       relid)
{
        KEY_CHECK(key);
        ss_dassert(key->k_type == RS_KEY_FORKEYCHK ||
                   key->k_type == RS_KEY_PRIMKEYCHK);
        key->k_refrelid = relid;
}

#ifdef SS_MME

void rs_key_setmmeindex(
        void*           cd __attribute__ ((unused)),
        rs_key_t*       key,
        void*           index)
{
        KEY_CHECK(key);

        key->k_mmeindex = index;
}

double rs_key_costfactor(
        void*           cd __attribute__ ((unused)),
        rs_key_t*       key,
        rs_ttype_t*     ttype)
{
        double          cost;
        rs_ano_t        ano;
        rs_ano_t        kp;
        rs_ano_t        last_kp;
        rs_atype_t*     atype;
        
        KEY_CHECK(key);

        if (key->k_costfactor != KEY_COST_UNKNOWN) {
            return key->k_costfactor;
        }

        cost = 1.0;
        last_kp = rs_key_lastordering(cd, key); 
        for (kp = rs_key_first_datapart(cd, key);
             kp <= last_kp;
             kp++) {
            ano = rs_keyp_ano(cd, key, kp);
            atype = rs_ttype_atype(cd, ttype, ano);

            switch (rs_atype_datatype(cd, atype)) {
                case RSDT_INTEGER:
                case RSDT_BIGINT:
                    cost = cost * 1.1;
                    break;

                default:
                    cost = cost * 1.2;
                    break;
            }
        }

        if (!rs_key_isunique(cd, key)
            && !rs_key_isclustering(cd, key)
            && !rs_key_isprimary(cd, key)) {
            cost = cost * 1.1;
        }

        key->k_costfactor = cost;

        return cost;
}
#endif

#ifndef rs_key_refkeys
su_pa_t* rs_key_refkeys(
        void*       cd __attribute__ ((unused)),
        rs_key_t*   key)
{
        KEY_CHECK(key);
        ss_dassert(key->k_type == RS_KEY_NORMAL);

        return _RS_KEY_REFKEYS_(cd, key);
}
#endif

/*##**********************************************************************\
 *
 *      rs_key_addrefkey
 *
 * Add a reference key to a physical key.  Whenever a value is inserted
 * to or deleted from this physical key, the reference keys associated with
 * it must be checked for referential integrity constraints.
 *
 * The link count of the added reference key is increased here and decreased
 * when this physical key is freed.  This ensures that the reference keys are
 * safe to be accessed as long as the physical key object is alive, although
 * care should be taken not to use old versions of either physical or reference
 * key objects.
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      key - in out, use
 *          The physical key.
 *
 *      refkey - in out, use
 *          The reference key.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void rs_key_addrefkey(
        void*       cd,
        rs_key_t*   key,
        rs_key_t*   refkey)
{
        KEY_CHECK(key);
        ss_dassert(key->k_type == RS_KEY_NORMAL);
        KEY_CHECK(refkey);
        ss_dassert(refkey->k_type == RS_KEY_FORKEYCHK
                   || refkey->k_type == RS_KEY_PRIMKEYCHK);

        if (key->k_refkeys == NULL) {
            key->k_refkeys = su_pa_init();
        } else {
            rs_key_t*   rkey;
            uint i;

            su_pa_do_get(key->k_refkeys, i, rkey) {
                if (refkey == rkey) {
                    return;
                }
            }
        }

        rs_key_link(cd, refkey);
        su_pa_insert(key->k_refkeys, refkey);
}

rs_ano_t rs_key_searchkpno_anytype(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t ano)
{
        rs_keypart_t *kp;
        rs_ano_t i;
        
        ss_dassert(ano != RS_ANO_NULL);
        ss_dassert(key->k_nparts >= 0);
        for (i = 0; i < key->k_nparts; i++) {
            kp = &key->k_parts[i];
            if (kp->kp_ano == ano) {
                return (kp->kp_index);
            }
        }
        return (RS_ANO_NULL);
}
        
rs_ano_t rs_key_searchkpno_ordering(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t ano)
{
        rs_keypart_t *kp;
        rs_ano_t i;
        
        ss_dassert(ano != RS_ANO_NULL);
        ss_dassert(key->k_nparts >= 0);
        for (i = 0; i < key->k_nordering; i++) {
            kp = &key->k_parts[i];
            if (kp->kp_ano == ano) {
                return (kp->kp_index);
            }
        }
        return (RS_ANO_NULL);
}
        
#ifdef SS_DEBUG
/*##**********************************************************************\
 * 
 *		rs_key_print
 * 
 * Prints key using SsDbgPrintf.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_key_print(cd, key)
        void*     cd;
	rs_key_t* key;
{
        uint i;

        SsDbgPrintf("%-5s %-25s %-4s %-4s %-4s %-4s %-4s\n",
            "ID", "NAME", "CLUS", "UNIQ", "PRIM", "PREJ", "NORD");

        SsDbgPrintf("%5ld %-25s %-4s %-4s %-4s %-4s %4d\n",
            key->k_id,
            key->k_name,
            SU_BFLAG_TEST(key->k_flags, RSKF_CLUSTERING) ? "YES" : "NO",
            SU_BFLAG_TEST(key->k_flags, RSKF_UNIQUE) ? "YES" : "NO",
            SU_BFLAG_TEST(key->k_flags, RSKF_PRIMARY) ? "YES" : "NO",
            SU_BFLAG_TEST(key->k_flags, RSKF_PREJOINED) ? "YES" : "NO",
            key->k_nordering);

        SsDbgPrintf("    %-2s %-14s %-3s %-3s %s\n",
            "KP", "TYPE", "ASC", "ANO", "CONSTVALUE");
        for (i = 0; i < (uint)key->k_nparts; i++) {
            char *val;
            char valbuf[10];
            rs_keypart_t* kp;

            kp = &key->k_parts[i];

            if (kp->kp_constaval == NULL) {
                val = (char *)"NULL";
            } else if (kp->kp_type == RSAT_KEY_ID) {
                SsSprintf(valbuf, "%d",
                          va_getlong(rs_keyp_constvalue(cd, key, i)));
                val = valbuf;
            } else {
                val = va_getasciiz(rs_keyp_constvalue(cd, key, i));
            }
            SsDbgPrintf("    %2d %-14s %-3s %3d %.*s\n",
                i,
                rs_atype_attrtypename(cd, kp->kp_type),
                kp->kp_ascending ? "YES" : "NO",
                kp->kp_ano,
                kp->kp_constaval != NULL
                    ? va_netlen(rs_keyp_constvalue(cd, key, i))
                    : 4,
                val);
        }
}

/*##**********************************************************************\
 * 
 *		rs_key_print_ex
 * 
 * Prints key using SsDbgPrintf.
 * 
 * Parameters : 
 * 
 *  cd - in, use
 *		client data
 *
 *	key - in, use
 *		pointer to key
 *  table_type - in, use
 *      table type of the table this key belongs to.
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_key_print_ex(void* cd, rs_ttype_t* table_type, rs_key_t* key)
{
        uint i;

        SsDbgPrintf("%-5s %-25s %-4s %-4s %-4s %-4s %-4s\n",
            "ID", "NAME", "CLUS", "UNIQ", "PRIM", "PREJ", "NORD");

        SsDbgPrintf("%5ld %-25s %-4s %-4s %-4s %-4s %4d\n",
            key->k_id,
            key->k_name,
            SU_BFLAG_TEST(key->k_flags, RSKF_CLUSTERING) ? "YES" : "NO",
            SU_BFLAG_TEST(key->k_flags, RSKF_UNIQUE) ? "YES" : "NO",
            SU_BFLAG_TEST(key->k_flags, RSKF_PRIMARY) ? "YES" : "NO",
            SU_BFLAG_TEST(key->k_flags, RSKF_PREJOINED) ? "YES" : "NO",
            key->k_nordering);

        SsDbgPrintf("    %-2s %-14s %-3s %-3s %-30s %s\n",
                    "KP", "TYPE", "ASC", "ANO", "ANAME", "CONSTVALUE");
        for (i = 0; i < (uint)key->k_nparts; i++) {
            char *val;
            char valbuf[10];
            rs_keypart_t* kp;

            kp = &key->k_parts[i];

            if (kp->kp_constaval == NULL) {
                val = (char *)"NULL";
            } else if (kp->kp_type == RSAT_KEY_ID) {
                SsSprintf(valbuf, "%d",
                          va_getlong(rs_keyp_constvalue(cd, key, i)));
                val = valbuf;
            } else {
                val = va_getasciiz(rs_keyp_constvalue(cd, key, i));
            }
            SsDbgPrintf("    %2d %-14s %-3s %3d %-30.30s %.*s\n",
                i,
                rs_atype_attrtypename(cd, kp->kp_type),
                kp->kp_ascending ? "YES" : "NO",
                kp->kp_ano,
                rs_ttype_aname(cd,table_type,kp->kp_ano),
                kp->kp_constaval != NULL
                    ? va_netlen(rs_keyp_constvalue(cd, key, i))
                    : 4,
                val);
        }
}

void rs_key_setreadonly(
        void*     cd,
	rs_key_t* key)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        SU_BFLAG_SET(key->k_flags, RSKF_READONLY);
}

#endif /* SS_DEBUG */
