/*************************************************************************\
**  source       * rs1aval.h
**  directory    * res
**  description  * Internal structure definitions for rs_aval_t.
**               * Used in rs0aval.c and rs1avnu.c.
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


#ifndef RS1AVAL_H
#define RS1AVAL_H

#include <ssint8.h>

#include <uti0va.h>

#include <su0bflag.h>

#include <dt0date.h>

#define RS_INTERNAL

#include "rs0aval.h"
// #include "rs0avacc.h"

typedef enum {
        RS_AGGR_SUM = 100,
        RS_AGGR_AVG,
        RS_AGGR_MAX,
        RS_AGGR_MIN,
        RS_AGGR_COUNT,
        RS_AGGR_ILLEGAL
} rs_aggrtype_t;

#define RS_AVAL_DEFAULTMAXLOADBLOBLIMIT SS_MIN(1024UL*1024UL, SS_MAXALLOCSIZE)

#define CHECK_AVAL(av) \
        ss_dassert(SS_CHKPTR(av) && \
                   (av)->ra_check == RSCHK_ATTRVALUE && \
                   (av)->ra_check_end == RSCHK_ATTRVALUE_END)

/* also was: ss_dassert(SU_BFLAG_TEST((av)->ra_flags, \
                                   RA_NULL|RA_ONLYCONVERTED) ? \
                                   !SU_BFLAG_TEST((av)->ra_flags, RA_BLOB) :\
                                   (!SU_BFLAG_TEST((av)->ra_flags, RA_BLOB) ==\
                                   !va_testblob(((av)->RA_RDVA)))) */

#define _RS_AVAL_UNLINKBLOB_(cd, atype, aval) \
{\
        ss_debug(su_ret_t rc;)\
        ss_dprintf_1(("%s %d: RS_AVAL_UNLINKBLOB(aval=0x%08lx)\n",\
                     __FILE__, __LINE__, (ulong)(aval)));\
        ss_debug(rc = )(*rs_aval_blobrefcount_dec)(cd, aval, NULL);\
        ss_rc_dassert(rc == SU_SUCCESS, rc);\
        ss_dassert(!SU_BFLAG_TEST((aval)->ra_flags, RA_NULL));\
        ss_dassert(va_testblob((aval)->RA_RDVA));\
        SU_BFLAG_CLEAR((aval)->ra_flags, RA_BLOB);\
}

#define _RS_AVAL_UNLINKBLOBIF_(cd, atype, aval) \
        if (SU_BFLAG_TEST((aval)->ra_flags, RA_BLOB)) {\
            RS_AVAL_UNLINKBLOB(cd, atype, aval);\
        } else {\
            ss_dassert(SU_BFLAG_TEST((aval)->ra_flags,\
                                     RA_NULL|RA_ONLYCONVERTED|RA_FLATVA) || \
                       ((aval)->ra_flags == 0 && (aval)->RA_RDVA == NULL) || \
                       !va_testblob((aval)->RA_RDVA));\
        }



#define _RS_AVAL_LINKBLOB_(cd, atype, aval) \
{\
        ss_debug(su_ret_t rc;)\
        ss_dprintf_1(("%s %d: RS_AVAL_LINKBLOB(aval=0x%08lx)\n",\
                     __FILE__, __LINE__, (ulong)(aval)));\
        ss_debug(rc = )(*rs_aval_blobrefcount_inc)(cd, aval, NULL);\
        ss_rc_dassert(rc == SU_SUCCESS, rc);\
        ss_dassert(!SU_BFLAG_TEST((aval)->ra_flags, RA_NULL));\
        ss_dassert(va_testblob((aval)->RA_RDVA));\
}

#define _RS_AVAL_LINKBLOBIF_(cd, atype, aval) \
        if (SU_BFLAG_TEST((aval)->ra_flags, RA_BLOB)) {\
            RS_AVAL_LINKBLOB(cd, atype, aval);\
        } else {\
            ss_dassert(SU_BFLAG_TEST((aval)->ra_flags, \
                                     RA_NULL | RA_ONLYCONVERTED | RA_FLATVA) ||\
            !va_testblob((aval)->RA_RDVA));\
        }\

            

#define AVAL_SETDATAANDNULL(cd, atype, aval, data, dlen) \
do {\
        size_t nlen = (dlen) + 1;\
        size_t glen = VA_GROSSLENBYNETLEN(nlen);\
        ss_dassert(!SU_BFLAG_TEST((aval)->ra_flags, RA_FLATVA));\
        if (glen <= sizeof((aval)->ra_vabuf)) {\
            refdva_free(&(aval)->ra_va);\
            (aval)->ra_va = va_setdataandnull(&(aval)->ra_vabuf.va,\
                                              (data), (dlen));\
            SU_BFLAG_SET((aval)->ra_flags, RA_FLATVA);\
        } else {\
            refdva_setdataandnull(&(aval)->ra_va, (data), (dlen));\
        }\
        CHECK_AVAL(aval);\
} while (FALSE)

#define AVAL_SETDATA(cd, atype, aval, data, dlen) \
do {\
        size_t glen = VA_GROSSLENBYNETLEN(dlen);\
        ss_dassert(!SU_BFLAG_TEST((aval)->ra_flags, RA_FLATVA));\
        if (glen <= sizeof((aval)->ra_vabuf)) {\
            refdva_free(&(aval)->ra_va);\
            (aval)->ra_va = va_setdata(&(aval)->ra_vabuf.va,\
                                       (data), (dlen));\
            SU_BFLAG_SET((aval)->ra_flags, RA_FLATVA);\
        } else {\
            refdva_setdata(&(aval)->ra_va, (data), (dlen));\
        }\
        CHECK_AVAL(aval);\
} while (FALSE)

#define AVAL_SETDATACHAR1TO2(cd, atype, aval, data, dlen) \
do {\
        size_t glen = ((dlen) * 2) + 1;\
        glen = VA_GROSSLENBYNETLEN(glen);\
        ss_dassert(!SU_BFLAG_TEST((aval)->ra_flags, RA_FLATVA));\
        if (glen <= sizeof((aval)->ra_vabuf)) {\
            refdva_free(&(aval)->ra_va);\
            (aval)->ra_va =\
                va_setdatachar1to2(\
                    &(aval)->ra_vabuf.va,\
                    (data), (dlen));\
            SU_BFLAG_SET((aval)->ra_flags, RA_FLATVA);\
        } else {\
            refdva_setdatachar1to2(&(aval)->ra_va, (data), (dlen));\
        }\
        CHECK_AVAL(aval);\
} while (FALSE)

#define AVAL_SETDATACHAR2(cd, atype, aval, data, dlen) \
do {\
        size_t glen = ((dlen) * 2) + 1;\
        glen = VA_GROSSLENBYNETLEN(glen);\
        ss_dassert(!SU_BFLAG_TEST((aval)->ra_flags, RA_FLATVA));\
        if (glen <= sizeof((aval)->ra_vabuf)) {\
            refdva_free(&(aval)->ra_va);\
            (aval)->ra_va =\
                va_setdatachar2(&(aval)->ra_vabuf.va,\
                                (data), (dlen));\
            SU_BFLAG_SET((aval)->ra_flags, RA_FLATVA);\
        } else {\
            refdva_setdatachar2(&(aval)->ra_va, (data), (dlen));\
        }\
        CHECK_AVAL(aval);\
} while (FALSE)


#define AVAL_SETVADATACHAR2TO1(cd, atype, aval, data, dlen) \
        rs_aval_putvadatachar2to1(cd, atype, aval, data, dlen)

#define AVAL_SETDATACHAR2TO1(cd, atype, aval, data, dlen) \
        rs_aval_putdatachar2to1(cd, atype, aval, data, dlen)


#define _RS_AVAL_COPYBUF2_(cd,atype,res_aval,aval) \
{\
        ss_dassert(res_aval!=aval);\
        *(res_aval) = *(aval);\
        RS_AVAL_LINKBLOBIF(cd, atype, res_aval);\
        if (SU_BFLAG_TEST((res_aval)->ra_flags, RA_VTPLREF)) {\
            ss_dassert(!SU_BFLAG_TEST((res_aval)->ra_flags, RA_NULL));\
            rs_aval_removevtplref(cd, atype, res_aval);\
        } else if (SU_BFLAG_TEST((res_aval)->ra_flags, RA_FLATVA)) {\
            ss_dassert((res_aval)->ra_va == &(aval)->ra_vabuf.va);\
            (res_aval)->ra_va = &(res_aval)->ra_vabuf.va;\
        } else if (!SU_BFLAG_TEST((res_aval)->ra_flags, \
                                  RA_NULL | RA_ONLYCONVERTED)) {\
            refdva_link((aval)->ra_va);\
        }\
        if (_RS_ATYPE_COPYCONVERT_(cd, atype)) {\
            SU_BFLAG_CLEAR((res_aval)->ra_flags, RA_CONVERTED);\
        }\
        (res_aval)->ra_accinfo = NULL;\
}

#define _RS_AVAL_FIX_RAWCOPY_(cd,atype,res_aval) \
{\
        RS_AVAL_LINKBLOBIF(cd, atype, res_aval);\
        if (SU_BFLAG_TEST((res_aval)->ra_flags, RA_FLATVA)) {\
            (res_aval)->ra_va = &(res_aval)->ra_vabuf.va;\
        } else if (!SU_BFLAG_TEST((res_aval)->ra_flags, RA_NULL | RA_VTPLREF | RA_ONLYCONVERTED)) {\
            refdva_link((res_aval)->ra_va);\
        }\
        if (_RS_ATYPE_COPYCONVERT_(cd, atype)) {\
            SU_BFLAG_CLEAR((res_aval)->ra_flags, RA_CONVERTED);\
        }\
        (res_aval)->ra_accinfo = NULL;\
}

#define _RS_AVAL_COPYBUF4_(cd,atype,res_aval,aval) \
{\
        ss_dassert(res_aval!=aval);\
        RS_AVAL_UNLINKBLOBIF(cd, atype, res_aval);\
        if (!SU_BFLAG_TEST((res_aval)->ra_flags, RA_NULL | RA_VTPLREF | RA_ONLYCONVERTED | RA_FLATVA)) {\
            refdva_free(&(res_aval)->ra_va);\
        }\
        if ((res_aval)->ra_accinfo != NULL) {\
            rs_aval_accinfo_free(((res_aval)->ra_accinfo));\
        }\
        *(res_aval) = *(aval);\
        RS_AVAL_FIX_RAWCOPY(cd,atype,res_aval);\
}

#define _RS_AVAL_ASSIGNBUF_(cd, atype, dst_aval, src_aval) \
{\
        ss_dassert(dst_aval!=src_aval);\
        RS_AVAL_UNLINKBLOBIF(cd, atype, dst_aval);\
        if (!SU_BFLAG_TEST((dst_aval)->ra_flags, RA_VTPLREF | RA_NULL | RA_FLATVA | RA_ONLYCONVERTED)) {\
            refdva_done(&((dst_aval)->ra_va));\
        }\
        if ((dst_aval)->ra_accinfo != NULL) {\
            rs_aval_accinfo_free(((dst_aval)->ra_accinfo));\
        }\
        *(dst_aval) = *(src_aval);\
        RS_AVAL_LINKBLOBIF(cd, atype, dst_aval);\
        if (SU_BFLAG_TEST((dst_aval)->ra_flags, RA_VTPLREF)) {\
            ss_dassert(!SU_BFLAG_TEST((dst_aval)->ra_flags, RA_NULL));\
            rs_aval_removevtplref(cd, atype, dst_aval);\
        } else if (SU_BFLAG_TEST((dst_aval)->ra_flags, RA_FLATVA)) {\
            ss_dassert((dst_aval)->ra_va == &(src_aval)->ra_vabuf.va);\
            (dst_aval)->ra_va = &(dst_aval)->ra_vabuf.va;\
        } else if (!SU_BFLAG_TEST((dst_aval)->ra_flags, RA_NULL | RA_ONLYCONVERTED)) {\
            refdva_link((dst_aval)->ra_va);\
        }\
        if (_RS_ATYPE_COPYCONVERT_(cd, atype)) {\
            SU_BFLAG_CLEAR((dst_aval)->ra_flags, RA_CONVERTED);\
        }\
        (dst_aval)->ra_accinfo = NULL;\
}

#define RS_AVAL_CREATEBUF(cd, atype, avalbuf) \
        rs_aval_createbuf(cd, atype, avalbuf)
#define RS_AVAL_FREEBUF(cd, atype, avalbuf) \
        rs_aval_freebuf(cd, atype, avalbuf)

#ifdef SS_DEBUG

void rs_aval_assignbuf(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t*  dst_aval,
        rs_aval_t*  src_aval);

void rs_aval_fix_rawcopy(
        void*       cd,
        rs_atype_t* atype,
        rs_aval_t*  res_aval);

void rs_aval_linkblob(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_unlinkblob(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_linkblobif(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void rs_aval_unlinkblobif(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

#define RS_AVAL_COPYBUF2(cd,atype,res_aval,aval) \
        rs_aval_copybuf2(cd,atype,res_aval,aval)
#define RS_AVAL_FIX_RAWCOPY(cd,atype,res_aval) \
        rs_aval_fix_rawcopy(cd,atype,res_aval)
#define RS_AVAL_COPYBUF4(cd,atype,res_aval,aval) \
        rs_aval_copybuf4(cd,atype,res_aval,aval)
#define RS_AVAL_ISNULL(cd, atype, aval) \
        rs_aval_isnull(cd, atype, aval)
#define RS_AVAL_ASSIGNBUF(cd, atype, dst_aval, src_aval) \
        rs_aval_assignbuf(cd, atype, dst_aval, src_aval)
#define RS_AVAL_LINKBLOB(cd, atype, aval) \
        rs_aval_linkblob(cd, atype, aval)
#define RS_AVAL_LINKBLOBIF(cd, atype, aval) \
        rs_aval_linkblobif(cd, atype, aval)
#define RS_AVAL_UNLINKBLOB(cd, atype, aval) \
        rs_aval_unlinkblob(cd, atype, aval)
#define RS_AVAL_UNLINKBLOBIF(cd, atype, aval) \
        rs_aval_unlinkblobif(cd, atype, aval)
#else
#define RS_AVAL_COPYBUF2(cd,atype,res_aval,aval) \
        _RS_AVAL_COPYBUF2_(cd,atype,res_aval,aval)
#define RS_AVAL_FIX_RAWCOPY(cd,atype,res_aval) \
        _RS_AVAL_FIX_RAWCOPY_(cd,atype,res_aval)
#define RS_AVAL_COPYBUF4(cd,atype,res_aval,aval) \
        _RS_AVAL_COPYBUF4_(cd,atype,res_aval,aval)
#define RS_AVAL_ISNULL(cd, atype, aval) \
        _RS_AVAL_ISNULL_(cd, atype, aval)
#define RS_AVAL_ASSIGNBUF(cd, atype, dst_aval, src_aval) \
        _RS_AVAL_ASSIGNBUF_(cd, atype, dst_aval, src_aval)
#define RS_AVAL_LINKBLOB(cd, atype, aval) \
        _RS_AVAL_LINKBLOB_(cd, atype, aval)
#define RS_AVAL_LINKBLOBIF(cd, atype, aval) \
        _RS_AVAL_LINKBLOBIF_(cd, atype, aval)
#define RS_AVAL_UNLINKBLOB(cd, atype, aval) \
        _RS_AVAL_UNLINKBLOB_(cd, atype, aval)
#define RS_AVAL_UNLINKBLOBIF(cd, atype, aval) \
        _RS_AVAL_UNLINKBLOBIF_(cd, atype, aval)
#endif

/* replacement for system function pow() */
double rs_aval_numfun_power(double a, double e);

su_ret_t rs_aval_blobrefcount_inc(
        rs_sysi_t* cd,
        rs_aval_t* aval,
        su_err_t** p_errh);

su_ret_t rs_aval_blobrefcount_dec(
        rs_sysi_t* cd,
        rs_aval_t* aval,
        su_err_t** p_errh);

void rs_aval_accinfo_free(
        rs_aval_accinfo_t* accinfo);

#endif /* RS1AVAL_H */

