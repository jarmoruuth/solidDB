/*************************************************************************\
**  source       * rs0key.h
**  directory    * res
**  description  * Key services for table level
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


#ifndef RS0KEY_H
#define RS0KEY_H

#include <ssenv.h>
#include <ssc.h>
#include <sssprint.h>

#include <uti0va.h>

#include <su0parr.h>
#include <su0bflag.h>

#include "rs0types.h"
#include "rs0atype.h"
#include "rs0ttype.h"

#define RS_KEY_MAXCMPLEN    255

/* Check macros */
#define KEY_CHECK(key) ss_dassert(SS_CHKPTR(key) && (key)->k_check == RSCHK_KEYTYPE)
#define KP_CHECK(kp)   ss_dassert(SS_CHKPTR(kp) && (kp)->kp_check == RSCHK_KEYPARTTYPE)

typedef enum {
        RS_KEY_NORMAL       = 0,
        RS_KEY_FORKEYCHK    = 1,
        RS_KEY_PRIMKEYCHK   = 2
} rs_keytype_t;

/* Structure for keypart type
 */
typedef struct tbkeypartstruct {

        ss_debug(int    kp_check;)     /* check field */
        rs_attrtype_t   kp_type;       /* key part type */
        bool            kp_ascending;  /* TRUE, if ascending order */
        rs_ano_t        kp_ano;        /* attribute index, has meaning only
                                          if kp_constaval is not NULL */
        rs_atype_t*     kp_constatype;
        rs_aval_t*      kp_constaval;  /* constant value, or NULL if none */
        va_t*           kp_constva;
        rs_ano_t        kp_index;      /* keypart index */
#ifdef SS_COLLATION
        int             kp_prefixlength; /* 0 means unlimited */
        su_collation_t* kp_collation;
#endif /* SS_COLLATION */
} rs_keypart_t;

/* Enumerated type for possible key type properties.
   Properties can coexist, they are therefore defined as bit flags.
*/
typedef enum {

        RSKF_CLUSTERING         = (int)SU_BFLAG_BIT(0),
        RSKF_UNIQUE             = (int)SU_BFLAG_BIT(1),
        RSKF_PRIMARY            = (int)SU_BFLAG_BIT(2),
        RSKF_PREJOINED          = (int)SU_BFLAG_BIT(3),
        RSKF_ABORTED            = (int)SU_BFLAG_BIT(4),
        RSKF_READONLY           = (int)SU_BFLAG_BIT(5),
        RSKF_REFMME             = (int)SU_BFLAG_BIT(6),
        RSKF_REFTEMPORARY       = (int)SU_BFLAG_BIT(7),
        RSKF_ISTUPLEVERSION     = (int)SU_BFLAG_BIT(8),
        RSKF_PHYSICALLY_UNIQUE  = (int)SU_BFLAG_BIT(9)

} rs_keyflags_t;  /* KF = Key Flags */

/* struct for key type */
struct tbkeystruct {

        ss_debug(int    k_check;)       /* check field */
        int             k_nlink;        /* Link counter */
        char*           k_name;         /* key name */
        ulong           k_id;           /* key id */
        su_bflag_t      k_flags;        /* bit arr containing or'ed key flags */
        uint            k_nordering;    /* number of key parts used for
                                           ordering in a key  */
        rs_keytype_t    k_type;         /* key type */
        int             k_nparts;
        rs_keypart_t*   k_parts;
        long            k_maxstoragelen;
        su_pa_t*        k_part_by_ano;  /* array of (key part index + 1)
                                           values indexed by ano to find
                                           key part from k_parts. Value
                                           (index + 1) so that zero (NULL)
                                           values are not added to supa. */
        double          k_costfactor;   /* the cost for accessing this key,
                                           only applies to MME indexes. */
        void*           k_mmeindex;     /* pointer to the MME index if this
                                           is an MME index. */
        int             k_action;       /* used for foreign keys only,
                                           to specify action on update and on
                                           delete */

        bool            k_index_ready;  /* Flag to signal if key can be used
                                           in the estimator. */
        ulong           k_refrelid;     /* For foreign keys - referencing
                                         * relh id. */
        su_pa_t*        k_refkeys;      /* The refkeys of the same relh that
                                           refer to this key. */
        int             k_maxrefkeypartno; /* Max key part number used in building
                                              tuple reference. */
        SsSemT*         k_sem;
}; /* rs_key_t */

#if defined(RS_USE_MACROS) || defined(RS_INTERNAL)

#define _RS_KEY_ISCLUSTERING_(cd, key) \
        SU_BFLAG_TEST((key)->k_flags, RSKF_CLUSTERING)

#define _RS_KEY_ISUNIQUE_(cd, key) \
        SU_BFLAG_TEST((key)->k_flags, RSKF_UNIQUE)

#define _RS_KEY_ISPRIMARY_(cd, key) \
        SU_BFLAG_TEST((key)->k_flags, RSKF_PRIMARY)

#define _RS_KEY_LASTORDERING_(cd, key) \
        ((key)->k_nordering - 1)

#define _RS_KEY_FIRST_DATAPART_(cd, key) \
        (1)

#define _RS_KEY_ISREFMME_(cd, key) \
        SU_BFLAG_TEST((key)->k_flags, RSKF_REFMME)

#define _RS_KEY_ISREFTEMPORARY_(cd, key) \
        SU_BFLAG_TEST((key)->k_flags, RSKF_REFTEMPORARY)

#define _RS_KEY_REFKEYS_(cd, key) \
        ((key)->k_refkeys)

#endif /* defined(RS_USE_MACROS) || defined(RS_INTERNAL) */

#ifdef RS_USE_MACROS

#define rs_key_isclustering(cd, key) \
        _RS_KEY_ISCLUSTERING_(cd, key)

#define rs_key_isunique(cd, key) \
        _RS_KEY_ISUNIQUE_(cd, key)

#define rs_key_isprimary(cd, key) \
        _RS_KEY_ISPRIMARY_(cd, key)

#define rs_key_lastordering(cd, key) \
        _RS_KEY_LASTORDERING_(cd, key)

#define rs_key_first_datapart(cd, key) \
        _RS_KEY_FIRST_DATAPART_(cd, key)

#define rs_key_isrefmme(cd, key) \
        _RS_KEY_ISREFMME_(cd, key)

#define rs_key_isreftemporary(cd, key) \
        _RS_KEY_ISREFTEMPORARY_(cd, key)

#define rs_key_refkeys(cd, key) \
        _RS_KEY_REFKEYS_(cd, key)

#else

bool rs_key_isclustering(
        void*     cd,
        rs_key_t* key);

bool rs_key_isunique(
        void*     cd,
        rs_key_t* key);

bool rs_key_isprimary(
        void*     cd,
        rs_key_t* key);

rs_ano_t rs_key_lastordering(
        void*     cd,
        rs_key_t* key);

rs_ano_t rs_key_first_datapart(
        void*       cd,
        rs_key_t*   key);

bool rs_key_isrefmme(
        void*       cd,
        rs_key_t*   key);

bool rs_key_isreftemporary(
        void*       cd,
        rs_key_t*   key);

su_pa_t* rs_key_refkeys(
        void*       cd,
        rs_key_t*   key);

#endif /* RS_USE_MACROS */

#define rs_key_istupleversion(cd, key) SU_BFLAG_TEST((key)->k_flags, RSKF_ISTUPLEVERSION)

/* Key building services and properties */

rs_key_t* rs_key_init(
        void*       cd,
        char*       keyname,
        ulong       key_id,
        bool        unique,
        bool        clustering,
        bool        primary,
        bool        prejoined,
        uint        nordering,
        rs_auth_t*  auth
);

void rs_key_done(
        void*     cd,
        rs_key_t* key
);

void rs_key_link(
        void*     cd,
        rs_key_t* key
);

void rs_key_addpart(
        void*       cd,
        rs_key_t*   key,
        rs_ano_t    kpindex,
        rs_attrtype_t kptype,
        bool        asc,
        rs_ano_t    ano,
        void*       constantvalue_or_collation
);

SS_INLINE rs_ano_t rs_key_nparts(
        void*     cd,
        rs_key_t* key
);

SS_INLINE rs_ano_t rs_key_searchkpno_data(
        void *cd,
        rs_key_t *key,
        rs_ano_t ano);

rs_ano_t rs_key_searchkpno_ordering(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t ano);

rs_ano_t rs_key_searchkpno_anytype(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t ano);

SS_INLINE ulong rs_key_id(
        void*     cd,
        rs_key_t* key);

void rs_key_setid(
        void*     cd,
        rs_key_t* key,
        ulong     keyid
);

char* rs_key_name(
        void*     cd,
        rs_key_t* key
);

SS_INLINE bool rs_key_issyskey(
        void*     cd,
        rs_key_t* key
);

rs_auth_t* rs_key_auth(
        void*     cd,
        rs_key_t* key
);

SS_INLINE rs_ano_t rs_key_nrefparts(
        void*     cd,
        rs_key_t* key
);

bool rs_key_isprejoined(
        void*     cd,
        rs_key_t* key
);

void rs_key_setaborted(
        void*     cd,
        rs_key_t* key
);

SS_INLINE bool rs_key_isaborted(
        void*     cd,
        rs_key_t* key
);

void rs_key_settype(
        void*     cd,
        rs_key_t* key,
        rs_keytype_t type
);

SS_INLINE rs_keytype_t rs_key_type(
        void*     cd,
        rs_key_t* key
);

void rs_key_setmaxstoragelen(
        void*     cd,
        rs_key_t* key,
        long      maxlen
);

SS_INLINE long rs_key_maxstoragelen(
        void*     cd,
        rs_key_t* key
);

void rs_key_setmaxrefkeypartno(
        void*     cd,
        rs_key_t* key,
        int       maxkpno
);

SS_INLINE int rs_key_maxrefkeypartno(
        void*     cd,
        rs_key_t* key
);

bool rs_key_issamekey (
        void*     cd,
        rs_key_t* key1,
        rs_key_t* key2,
        bool issubkey
);

/* Key part properties */

rs_ano_t rs_keyp_ano(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex
);

#ifdef SS_COLLATION

SS_INLINE su_collation_t* rs_keyp_collation(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t kpindex);

SS_INLINE void rs_keyp_setprefixlength(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t kpindex,
        int prefix_length);

SS_INLINE int rs_keyp_getprefixlength(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t kpindex);
#endif /* SS_COLLATION */


SS_INLINE bool rs_keyp_isascending(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex
);

SS_INLINE void rs_key_set_physically_nonunique(
        rs_sysi_t* cd,
        rs_key_t* key);

SS_INLINE bool rs_key_is_physically_unique(
        rs_sysi_t* cd,
        rs_key_t* key);


#define _RS_KEYP_ISCONSTVALUE_(k, ki)   ((k)->k_parts[ki].kp_constaval != NULL)
#define _RS_KEYP_CONSTVALUE_(k, ki)     ((k)->k_parts[ki].kp_constva)
#define _RS_KEYP_PARTTYPE_(k, ki)       ((k)->k_parts[ki].kp_type)

#if defined(RS_USE_MACROS)

#define rs_keyp_isconstvalue(cd, k, ki) _RS_KEYP_ISCONSTVALUE_(k, ki)
#define rs_keyp_constvalue(cd, k, ki)   _RS_KEYP_CONSTVALUE_(k, ki)
#define rs_keyp_parttype(cd, k, ki)     _RS_KEYP_PARTTYPE_(k, ki)

#else

bool rs_keyp_isconstvalue(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex
);

va_t* rs_keyp_constvalue(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex
);

rs_attrtype_t rs_keyp_parttype(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex
);

#endif

void rs_keyp_setconstvalue(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex,
        va_t*     va
);

SS_INLINE rs_atype_t* rs_keyp_constatype(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex
);

SS_INLINE rs_aval_t* rs_keyp_constaval(
        void*     cd,
        rs_key_t* key,
        rs_ano_t  kpindex
);

void rs_key_setrefmme(
        void*       cd,
        rs_key_t*   key,
        bool        flag);

SS_INLINE void rs_key_setreftemporary(
        void*       cd,
        rs_key_t*   key,
        bool        flag);

void rs_key_setaction(
        rs_sysi_t*  cd,
        rs_key_t*   key,
        int         action);

SS_INLINE int rs_key_action(
        rs_sysi_t*  cd,
        rs_key_t*   key);

SS_INLINE int rs_key_update_action(
        rs_sysi_t*  cd,
        rs_key_t*   key);

SS_INLINE int rs_key_delete_action(
        rs_sysi_t*  cd,
        rs_key_t*   key);

bool rs_key_index_ready (
        rs_sysi_t*  cd,
        rs_key_t*   key);

void rs_key_setindex_ready (
        rs_sysi_t*  cd,
        rs_key_t*   key);

void rs_key_setrefrelid(
        rs_sysi_t*  cd,
        rs_key_t*   key,
        ulong       relid);

SS_INLINE ulong rs_key_refrelid(
        rs_sysi_t*  cd,
        rs_key_t*   key);

#ifdef SS_MME
SS_INLINE void* rs_key_getmmeindex(
        void*           cd,
        rs_key_t*       key);

void rs_key_setmmeindex(
        void*           cd,
        rs_key_t*       key,
        void*           index);

double rs_key_costfactor(
        void*           cd,
        rs_key_t*       key,
        rs_ttype_t*     ttype);
#endif

void rs_key_addrefkey(
        void*           cd,
        rs_key_t*       key,
        rs_key_t*       refkey);

#ifdef SS_DEBUG

void rs_key_print(
        void*     cd,
	rs_key_t* key);

void rs_key_print_ex(
        void* cd, 
    rs_ttype_t* table_type, 
    rs_key_t* key);

void rs_key_setreadonly(
        void*     cd,
	rs_key_t* key);

#endif /* SS_DEBUG */

/* Macros for internal use.
 */
#define _RS_KEYP_ISASCENDING_(cd, key, kpindex) ((key)->k_parts[kpindex].kp_ascending)
#define _RS_KEYP_ANO_(cd, key, kpindex)         ((key)->k_parts[kpindex].kp_ano)

/* Fast macros for external use.
 */
#ifdef SS_DEBUG
#define RS_KEYP_ISASCENDING rs_keyp_isascending
#else /* SS_DEBUG */
#define RS_KEYP_ISASCENDING _RS_KEYP_ISASCENDING_
#define rs_keyp_ano         _RS_KEYP_ANO_
#endif /* SS_DEBUG */

#if defined(RS0KEY_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		rs_key_id
 * 
 * Returns the key id of the key.
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
 *      key id
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE ulong rs_key_id(
        void*     cd,
        rs_key_t* key)
{
        KEY_CHECK(key);

        return(key->k_id);
}

/*##**********************************************************************\
 * 
 *		rs_key_nparts
 * 
 * Returns the number of parts in the key, including
 * system attributes like tuple id, and constant attributes.
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
 *      number of keyparts 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE rs_ano_t rs_key_nparts(
        void* cd,
	rs_key_t* key)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return(key->k_nparts);
}

/*##**********************************************************************\
 * 
 *		rs_key_searchkpno_data
 * 
 * Searches a keypart number for a given attribute number from key.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	key - in, use
 *		key object
 *
 *	ano - in, use
 *		attribute number
 *
 * Return value : 
 * 
 *      keypart number or                
 *      RS_ANO_NULL if not found
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE rs_ano_t rs_key_searchkpno_data(
        void *cd,
        rs_key_t *key,
        rs_ano_t ano)
{
        rs_keypart_t *kp;

        SS_NOTUSED(cd);
        ss_dassert(ano != RS_ANO_NULL);
        ss_dassert(key->k_nparts > 0);

        if (su_pa_indexinuse(key->k_part_by_ano, (uint)ano)) {
            int kpindex;
            /* double cast because of GCC 4 giving an error with just cast
             * to int. "cast from 'void*' to 'int' loses precision".  Unclear
             * if requiring this can be considered a bug in GCC. */
            kpindex = (int)(ss_ptr_as_scalar_t)su_pa_getdata(key->k_part_by_ano, (uint)ano);
            kp = &key->k_parts[kpindex - 1];
            KP_CHECK(kp);
            return (kp->kp_index);
        }
        return (RS_ANO_NULL);
}

/*##**********************************************************************\
 * 
 *		rs_key_issyskey
 * 
 * Checks if the key is a system generated key.
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
SS_INLINE bool rs_key_issyskey(
        void*     cd,
        rs_key_t* key)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return(key->k_name[0] == RS_SYSKEY_PREFIXCHAR);
}

/*##**********************************************************************\
 * 
 *		rs_key_nrefparts
 * 
 * Returns the number of reference attributes in the beginning of a
 * clustering key.
 * 
 * Parameters : 
 * 
 *      cd - in, ues
 *		client data
 *
 *	key - in, use
 *		pointer to key
 * 
 * Return value : 
 * 
 *      number of reference attributes
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE rs_ano_t rs_key_nrefparts(
        void*     cd,
        rs_key_t* key)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);
        ss_dassert(SU_BFLAG_TEST(key->k_flags, RSKF_CLUSTERING));

        return(key->k_nordering);
}

/*##**********************************************************************\
 * 
 *		rs_key_isaborted
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
SS_INLINE bool rs_key_isaborted(
        void* cd,
	rs_key_t* key)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return(SU_BFLAG_TEST(key->k_flags, RSKF_ABORTED));
}

/*##**********************************************************************\
 * 
 *		rs_key_type
 * 
 * Returns the key type.
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	key - in
 *		
 *		
 * Return value : 
 * 
 *      Key type, one of RS_KEY_NORMAL, RS_KEY_FORKEYCHK or
 *      RS_KEY_PRIMKEYCHK.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE rs_keytype_t rs_key_type(
        void*     cd,
        rs_key_t* key)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return(key->k_type);
}

/*##**********************************************************************\
 * 
 *		rs_key_maxstoragelen
 * 
 * Returns max storage length for a key.
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
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE long rs_key_maxstoragelen(
        void*     cd,
        rs_key_t* key)
{
        SS_NOTUSED(cd);
        KEY_CHECK(key);

        return(key->k_maxstoragelen);
}

SS_INLINE int rs_key_maxrefkeypartno(
        void*     cd,
        rs_key_t* key)
{
        int maxkpno;

        SS_NOTUSED(cd);
        KEY_CHECK(key);

        if (rs_key_isclustering(cd, key) || rs_key_isunique(cd, key)) {
            maxkpno = rs_key_lastordering(cd, key);
        } else {
            maxkpno = key->k_maxrefkeypartno;
        }
        return(maxkpno);
}

/*##**********************************************************************\
 * 
 *		rs_keyp_isascending
 * 
 * Returns TRUE if key part is in ascending order or FALSE is key part
 * is in descending order.
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
 *      TRUE    - ascending key part
 *      FALSE   - descending key part
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE bool rs_keyp_isascending(
        void* cd,
	rs_key_t* key,
	rs_ano_t kpindex)
{
        rs_keypart_t* kp;

        SS_NOTUSED(cd);
        KEY_CHECK(key);

        ss_dassert((uint)kpindex < (uint)key->k_nparts);

        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);

        return(_RS_KEYP_ISASCENDING_(cd, key, kpindex));
}

/*##**********************************************************************\
 * 
 *		rs_keyp_constatype
 * 
 * Returns the atype of constant key part
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
 *		
 * Return value - ref : 
 *     Pointer to atype object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE rs_atype_t* rs_keyp_constatype(
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

        return(kp->kp_constatype);
}

/*##**********************************************************************\
 * 
 *		rs_keyp_constaval
 * 
 * Returns the aval of constant key part
 * 
 * Parameters : 
 * 
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
 *      Pointer to aval object 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE rs_aval_t* rs_keyp_constaval(
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

        return(kp->kp_constaval);
}

SS_INLINE void rs_key_setreftemporary(
        void*           cd __attribute__ ((unused)),
        rs_key_t*       key,
        bool            flag)
{
        KEY_CHECK(key);
        ss_dassert(key->k_type == RS_KEY_FORKEYCHK
                   || key->k_type == RS_KEY_PRIMKEYCHK);

        if (flag) {
            SU_BFLAG_SET(key->k_flags, RSKF_REFTEMPORARY);
        } else {
            SU_BFLAG_CLEAR(key->k_flags, RSKF_REFTEMPORARY);
        }
}

/*##**********************************************************************\
 *
 *      rs_key_action
 *
 * Returns action value associated with the key as integer.
 *
 * Parameters :
 *
 *      cd - in, use
 *      client data
 *
 *  key - in, use
 *      key
 *
 * Return value :
 *
 * Limitations  :
 *      Makes sence only for foreign keys.
 *
 * Globals used :
 *
 * See also: 
 *      rs_key_update_action, rs_key_delete_action
 */
SS_INLINE int rs_key_action(
        rs_sysi_t*  cd __attribute__ ((unused)),
        rs_key_t*   key)
{
        KEY_CHECK(key);

        ss_dassert(key->k_action == 0
                || key->k_type == RS_KEY_FORKEYCHK
                || key->k_type == RS_KEY_PRIMKEYCHK);
        return key->k_action;
}

/*##**********************************************************************\
 *
 *      rs_key_update_action
 *
 * Returns action value for 'ON UPDATE' associated with the key.
 *
 * Parameters :
 *
 *      cd - in, use
 *      client data
 *
 *  key - in, use
 *      key
 *
 * Return value :
 *
 * Limitations  :
 *      Makes sence only for foreign keys.
 *
 * Globals used :
 *
 * See also:
 *      rs_key_update_action, rs_key_delete_action
 */
SS_INLINE int rs_key_update_action(
        rs_sysi_t*  cd __attribute__ ((unused)),
        rs_key_t*   key)
{
        KEY_CHECK(key);

        ss_dassert(key->k_action == 0
                || key->k_type == RS_KEY_FORKEYCHK
                || key->k_type == RS_KEY_PRIMKEYCHK);

        if (key->k_action == 0) {
            return SQL_REFACT_NOACTION;
        }
        ss_dassert ((key->k_action>>8)-1 >= SQL_REFACT_CASCADE &&
                    (key->k_action>>8)-1 <= SQL_REFACT_NOACTION);
        return (key->k_action>>8)-1;
}

/*##**********************************************************************\
 *
 *      rs_key_delete_action
 *
 * Returns action value for 'ON DELETE' associated with the key.
 *
 * Parameters :
 *
 *      cd - in, use
 *      client data
 *
 *  key - in, use
 *      key 
 *
 * Return value :
 *
 * Limitations  :
 *      Makes sence only for foreign keys.
 *
 * Globals used :
 *
 * See also:
 *      rs_key_update_action, rs_key_delete_action
 */
SS_INLINE int rs_key_delete_action(
        rs_sysi_t*  cd __attribute__ ((unused)),
        rs_key_t*   key)
{
        KEY_CHECK(key);

        ss_dassert(key->k_action == 0
                || key->k_type == RS_KEY_FORKEYCHK
                || key->k_type == RS_KEY_PRIMKEYCHK);
        if (key->k_action == 0) {
            return SQL_REFACT_NOACTION;
        }
        ss_dassert ((key->k_action & 0xFF)-1 >= SQL_REFACT_CASCADE &&
                    (key->k_action & 0xFF)-1 <= SQL_REFACT_NOACTION);
        return (key->k_action & 0xFF)-1;
}

/*##**********************************************************************\
 *
 *      rs_key_refrelid
 *      
 * Returns referencing table relation id.
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
SS_INLINE ulong rs_key_refrelid(
        rs_sysi_t*  cd __attribute__ ((unused)),
        rs_key_t*   key)
{
        KEY_CHECK(key);
        ss_dassert(key->k_type == RS_KEY_FORKEYCHK ||
                   key->k_type == RS_KEY_PRIMKEYCHK);
        return key->k_refrelid;
}

SS_INLINE void* rs_key_getmmeindex(
        void*           cd __attribute__ ((unused)),
        rs_key_t*       key)
{
        KEY_CHECK(key);

        return key->k_mmeindex;
}

SS_INLINE void rs_key_set_physically_nonunique(
        rs_sysi_t* cd,
        rs_key_t* key)
{
        KEY_CHECK(key);
        SU_BFLAG_CLEAR(key->k_flags, RSKF_PHYSICALLY_UNIQUE);
}

SS_INLINE bool rs_key_is_physically_unique(
        rs_sysi_t* cd,
        rs_key_t* key)
{
        KEY_CHECK(key);
        return (SU_BFLAG_TEST(key->k_flags, RSKF_PHYSICALLY_UNIQUE));
}

#ifdef SS_COLLATION

SS_INLINE su_collation_t* rs_keyp_collation(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t kpindex)
{
        rs_keypart_t* kp;

        KEY_CHECK(key);
        ss_dassert((uint)kpindex < (uint)key->k_nparts);
        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);
        return (kp->kp_collation);
}

SS_INLINE void rs_keyp_setprefixlength(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t kpindex,
        int prefix_length)
{
        rs_keypart_t* kp;

        KEY_CHECK(key);
        ss_dassert((uint)kpindex < (uint)key->k_nparts);
        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);
        kp->kp_prefixlength = prefix_length;       
}

SS_INLINE int rs_keyp_getprefixlength(
        rs_sysi_t* cd,
        rs_key_t* key,
        rs_ano_t kpindex)
{
        rs_keypart_t* kp;

        KEY_CHECK(key);
        ss_dassert((uint)kpindex < (uint)key->k_nparts);
        kp = &key->k_parts[kpindex];
        KP_CHECK(kp);
        return (kp->kp_prefixlength);
}
#endif /* SS_COLLATION */

#endif /* defined(RS0KEY_C) || defined(SS_USE_INLINE) */

#endif /* RS0KEY_H */
