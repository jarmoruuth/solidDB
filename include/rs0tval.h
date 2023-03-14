/*************************************************************************\
**  source       * rs0tval.h
**  directory    * res
**  description  * Tuple value functions
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


#ifndef RS0TVAL_H
#define RS0TVAL_H

#include <ssenv.h>
#include <ssc.h>
#include <ssdebug.h>
#include <sslimits.h>

#include <uti0vtpl.h>

#include <su0parr.h>

#define RS_INTERNAL

#include "rs0types.h"
#include "rs0sdefs.h"
#include "rs0ttype.h"
#include "rs0aval.h"

#if defined(RS_USE_MACROS) || defined(RS_INTERNAL)

struct rstuplevaluestruct {

        ss_debug(int tv_check;)     /* rs_check_t check field */
        ss_debug(char* tv_name;)
        vtpl_t*     tv_vtpl;        /* NULL or ref-counted vtuple */
        void*       tv_vtplalloc;   /* memory area pointer for tv_vtpl, could
                                       be different pointer e.g. if tv_vtpl
                                       points to dbe_bkey_t. */
        int         tv_nattrs;      /* Number of attributes in tv_attr_arr */
        int         tv_nlink;       /* Link count */
        rs_aval_rowflag_t tv_rowflags; /* Row flags read from database row. */
        rs_aval_t   tv_attr_arr[1]; /* Array of rs_aval_t objects, size
                                       allocated dynamically. Must be the
                                       last field. */
};  /* rs_tval_t */

#define CHECK_TVAL(tval)  {\
                            ss_dassert(SS_CHKPTR(tval));\
                            ss_dassert(tval->tv_check == RSCHK_TUPLEVALUE);\
                          }

#define _RS_TVAL_AVAL_(cd,ttype,tval,ano) \
        (&(tval)->tv_attr_arr[ano])

#ifdef SS_DEBUG

# define RS_TVAL_AVAL(cd, ttype, tval, ano) \
         rs_tval_aval(cd, ttype, tval, ano)

#else /* SS_DEBUG */

# define RS_TVAL_AVAL(cd, ttype, tval, ano) \
         _RS_TVAL_AVAL_(cd, ttype, tval, ano)

#endif /* SS_DEBUG */

#endif /* defined(RS_USE_MACROS) || defined(RS_INTERNAL) */

#ifdef RS_USE_MACROS

#define rs_tval_aval(cd, ttype, tval, ano) \
        _RS_TVAL_AVAL_(cd, ttype, tval, ano)

#else

rs_aval_t* rs_tval_aval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n);

#endif /* RS_USE_MACROS */


rs_tval_t* rs_tval_create(
        void*       cd,
        rs_ttype_t* ttype
);

void rs_tval_free(
        rs_sysi_t*  cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
);

void rs_tval_link(
        void*       cd,
        rs_tval_t*  tval
);

void rs_tval_reset(
        rs_sysi_t*  cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
);

void rs_tval_updateusecount(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        bool        inc
);

#ifdef RS_USE_MACROS

#define rs_tval_usecount(cd, ttype, tval)  ((tval)->tv_nlink)

#else /* RS_USE_MACROS */

#define rs_tval_usecount rs_tval_sql_usecount

#endif /* RS_USE_MACROS */

uint rs_tval_sql_usecount(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
);

void rs_tval_sql_setaval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n,
        rs_aval_t*  aval
);

rs_aval_t* rs_tval_sql_aval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n
);

rs_tval_t* rs_tval_copy(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
);

rs_tval_t* rs_tval_physcopy(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval);

bool rs_tval_convert(
        void*       cd,
        rs_ttype_t* dst_ttype,
        rs_tval_t** dst_tval,
        rs_ttype_t* src_ttype,
        rs_tval_t*  src_tval,
        rs_err_t**  p_errh);

uint rs_tval_sql_assignaval(
        void*       cd,
        rs_ttype_t* dst_ttype,
        rs_tval_t*  dst_tval,
        uint col_n,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh);

void rs_tval_project(
        void* cd,
        rs_ttype_t* src_ttype,
        rs_ttype_t* dst_ttype,
        rs_tval_t* src_tval,
        rs_tval_t* dst_tval,
        uint attr_cnt,
        rs_ano_t ano_array[/*attr_cnt*/]);

int rs_tval_cmp(
        void* cd,
        rs_ttype_t* ttype1,
        rs_ttype_t* ttype2,
        rs_tval_t* tval1,
        rs_tval_t* tval2,
        uint ncols,
        uint col_array[/* ncols */],
        bool asc_arr[/* ncols */]);

void rs_tval_set(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  trgtval,
        rs_tval_t*  srctval
);

void rs_tval_setaval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        rs_aval_t*  aval
);

void rs_tval_insertaval(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        rs_aval_t*  aval
);

void rs_tval_setva(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va
);

void rs_tval_setva_unlink(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va
);

void rs_tval_sql_setva(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n,
        va_t*       va
);

bool rs_tval_scanblobs(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        int*        p_phys_attr_n
);

long rs_tval_vagrosslen(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval
);

long rs_tval_vagrosslen_project(
        void*       cd,
        rs_ttype_t* ttype,
        bool* attrflags,
        rs_tval_t*  tval
);

bool rs_tval_trimchars(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        bool truncate);

void rs_tval_ensureconverted(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

rs_tval_t* rs_tval_realcopy(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

void rs_tval_linktovtpl(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        vtpl_t* p_vtpl,
        void* p_vtplalloc,
        bool init);

void rs_tval_linktovtpl_nounlink(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        vtpl_t* p_vtpl,
        void* p_vtplalloc,
        bool init);

void rs_tval_unlinkfromvtpl(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

void rs_tval_setvaref(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va);

SS_INLINE void rs_tval_setvaref_flat(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va);

#ifdef SS_DEBUG

void rs_tval_resetexternalflatva(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval);

#endif /* SS_DEBUG */

void rs_tval_setvaref_unlink(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va);

void rs_tval_sql_setvaref(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n,
        va_t*       va);

va_t* rs_tval_va(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n);

va_t* rs_tval_sql_va(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        sql_attr_n);

void rs_tval_removevtplref(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval);

dynvtpl_t rs_tval_givevtpl(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

uint rs_tval_nullifyblobids(
        rs_sysi_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* orig_tval_in,
        rs_tval_t** tval_in_out);

void rs_tval_setrowflags(
        rs_sysi_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        rs_aval_rowflag_t flags);

void rs_tval_clearallrowflags(
        rs_sysi_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

rs_aval_rowflag_t rs_tval_getrowflags(
        rs_sysi_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

#ifdef SS_SYNC

rs_tval_t* rs_tval_initfromvtpl(
        void* cd,
        rs_ttype_t* ttype,
        vtpl_t* p_vtpl);

#endif /* SS_SYNC */

#if defined(SS_DEBUG) || defined(SS_BETA) || TRUE

void rs_tval_print(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

char* rs_tval_printtostring(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

#endif /* defined(SS_DEBUG) || defined(SS_BETA) */

char* rs_tval_printtostring_likesolsql(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

#ifdef RS_FASTTTYPE
#define rs_tval_sql_setaval     rs_tval_setaval
#define rs_tval_sql_aval        rs_tval_aval
#define rs_tval_sql_setva       rs_tval_setva
#define rs_tval_sql_setvaref    rs_tval_setvaref
#define rs_tval_sql_va          rs_tval_va
#endif

bool rs_tval_uni2charif(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        rs_ttype_t** p_new_ttype,
        rs_tval_t** p_new_tval,
        rs_err_t** p_errh);

void rs_tval_char2uniif(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

/* XXX - THIS IS A TEMPORARY FUNCTION. */
int rs_tval_nattrs(
        void*           cd,
        rs_ttype_t*     ttype,
        rs_tval_t*      tval);

void rs_tval_copy_over(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* dst_tval,
        rs_tval_t* src_tval);

bool rs_tval_sql_set1avalnull(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        rs_ano_t sqlano);

#if defined(RS0TVAL_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              rs_tval_setvaref_flat
 *
 * Sets the value of an attribute of tuple value object from a v-attribute.
 * Only reference is assigned in this function and the reference is marked
 * as flatva.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      ttype - in, use
 *              tuple type
 *
 *      tval - in out, use
 *              tuple value
 *
 *      phys_attr_n - in
 *              physical attibute number in tuple
 *
 *      va - in, hold
 *              the new value for the attribute
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void rs_tval_setvaref_flat(
        void*       cd,
        rs_ttype_t* ttype,
        rs_tval_t*  tval,
        uint        phys_attr_n,
        va_t*       va
) {
        ss_dprintf_3(("%s: rs_tval_setvaref_flat\n", __FILE__));
        CHECK_TVAL(tval);
        {
            rs_atype_t* atype;
            rs_aval_t*  tval_aval;

            ss_dassert(phys_attr_n < (uint)tval->tv_nattrs);

            atype = RS_TTYPE_ATYPE(cd, ttype, phys_attr_n);
            tval_aval = RS_TVAL_AVAL(cd, ttype, tval, phys_attr_n);

            rs_aval_setvaref(cd, atype, tval_aval, va);
            rs_aval_setflatvarefflag(cd, atype, tval_aval);
        }
}

#endif /* defined(RS0TVAL_C) || defined(SS_USE_INLINE) */

#endif /* RS0TVAL_H */
