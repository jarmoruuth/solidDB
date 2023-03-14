/*************************************************************************\
**  source       * xs2tupl.c
**  directory    * xs
**  description  * Tuple building routines for sorting
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


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <uti0vtpl.h>
#include <uti0va.h>
#include <uti0vcmp.h>
#include <su0bmap.h>

#include <rs0types.h>
#include <rs0atype.h>
#include <rs0ttype.h>

#include "xs0acnd.h"
#include "xs2tupl.h"


/*##**********************************************************************\
 * 
 *		xs_tuple_anomap_init
 * 
 * Creates a vtuple-ano to tval-sql-ano coversion table
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *      nattrs - in
 *          # of selected columns
 *
 *	sellist - in, use
 *		list of selected columns
 *		
 *	orderby_list - in, use
 *		orderby list of xs_acond_t*'s
 *		
 * Return value - give :
 *      created conversion table
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
rs_ano_t* xs_tuple_anomap_init(
        rs_sysinfo_t* cd __attribute__ ((unused)),
        uint nattrs,
        rs_ano_t sellist[/*nattrs*/],
        su_list_t* orderby_list)
{
        su_list_node_t* lnode;
        xs_acond_t* acond;
        uint i;
        uint j;
        rs_ano_t* anomap;

        anomap = SsMemAlloc(sizeof(rs_ano_t) * nattrs);

        /* Columns in the order by list are stored at the beginning
         * of v-tuple. RS_ANO_NULL is used to mark those columns
         * that are already added to anomap.
         */
        i = 0;
        if (orderby_list != NULL) {
            su_list_do_get(orderby_list, lnode, acond) {
                ss_dassert((uint)xs_acond_ano(acond) < nattrs);
                for (j = 0; j < nattrs; j++) {
                    /* Find and mark ano as already added to anomap. */
                    if (sellist[j] == xs_acond_ano(acond)) {
                        /* Ano found. */
                        sellist[j] = RS_ANO_NULL;
                        break;
                    }
                }
                ss_dassert(j < nattrs);
                if (j < nattrs) {
                    anomap[i] = xs_acond_ano(acond);
                }
                i++;
            }
        }

        /* Add columns with no ordering criterium to the anomap.
         */
        for (j = 0; j < nattrs; j++) {
            if (sellist[j] != RS_ANO_NULL) {
                anomap[i] = sellist[j];
                i++;
            }
        }
        ss_dassert(i == nattrs);
        return (anomap);
}

/*##**********************************************************************\
 * 
 *		xs_tuple_anomap_done
 * 
 * Deletes a vtuple-ano to tval-sql-ano coversion table
 * 
 * Parameters : 
 * 
 *	anomap - in, take
 *		pointer to conversion map
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_tuple_anomap_done(
        rs_ano_t* anomap)
{
        ss_dassert(anomap != NULL);
        SsMemFree(anomap);
}

/*##**********************************************************************\
 * 
 *		xs_tuple_cmpcondarr_init
 * 
 * Creates a comparison condition array needed for comparison
 * routine used by quicksort and polyphase merge sort
 * 
 * Parameters : 
 * 
 *	orderby_list - in, use
 *		ORDER BY list
 *		
 * Return value - give :
 *      created comparison condition array
 * 
 * Comments :
 * 
 * Globals used : 
 *
 * See also :
 *      vtpl_condcompare() in uti0vcmp.c
 */
uint* xs_tuple_cmpcondarr_init(su_list_t* orderby_list)
{
        uint n;
        uint* cmpcondarr;
        su_list_node_t* node;
        xs_acond_t* acond;

        n = su_list_length(orderby_list);
        cmpcondarr = SsMemAlloc(sizeof(uint) * (n + 1));
        cmpcondarr[0] = n;
        n = 1;
        su_list_do_get(orderby_list, node, acond) {
            if (xs_acond_asc(acond)) {
                cmpcondarr[n] = VTPL_CMP_ASC;
            } else {
                cmpcondarr[n] = VTPL_CMP_DESC;
            }
            n++;
        }
        return (cmpcondarr);
}

/*##**********************************************************************\
 * 
 *		xs_tuple_cmpcondarr_done
 * 
 * Deletes a comparison condition array
 * 
 * Parameters : 
 * 
 *	cmpcondarr - in, take
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void xs_tuple_cmpcondarr_done(uint* cmpcondarr)
{
        ss_dassert(cmpcondarr != NULL);
        SsMemFree(cmpcondarr);
}

/*##**********************************************************************\
 * 
 *		xs_tuple_makevtpl
 * 
 * Builds a vtuple into a sort buffer
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *		
 *	ttype - in, use
 *		tuple type
 *		
 *	tval - in, use
 *		tuple value
 *
 *      nattrs - in
 *          number of attributes in tval & ttype
 *          (to avoid call to rs_ttype_nattrs)
 *          
 *	anomap - in, use
 *		vtuple-ano to tval-sql-ano conversion table
 *		
 *	buf - out
 *		pointer to memory storage where the vtuple is to be built
 *		
 *	bufsize - in
 *		size of buf in bytes
 *		
 *	p_nextpos - out
 *		pointer to pointer variable where the address of the next
 *          unused buffer position will be stored
 *		
 * Return value :
 *      TRUE when the vtuple is built OK or
 *      FALSE when gross length of vtuple is bigger than bufsize
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tuple_makevtpl(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        uint nattrs,
        rs_ano_t* anomap,
        char* buf,
        size_t bufsize,
        char** p_nextpos)
{
        va_t* va;
        va_index_t va_glen;
        size_t s;
        vtpl_t* target_vtpl;
        uint i;
        va_index_t vtpl_llen;
        va_index_t vtpl_nlen;
        va_index_t vtpl_glen;

        ss_dassert(nattrs == rs_ttype_nattrs(cd, ttype));

        if (bufsize < VA_LENGTHMINLEN) {
            *p_nextpos = buf;
            return (FALSE);
        }
        target_vtpl = (vtpl_t*)buf;
        vtpl_setvtpl(target_vtpl, VTPL_EMPTY);
        for (i = 0; i < nattrs; i++) {
            vtpl_glen = VTPL_GROSSLEN(target_vtpl);
            va = rs_tval_va(cd, ttype, tval, anomap[i]);
            va_glen = VA_GROSSLEN(va);
            s = vtpl_glen + va_glen;
            if (s + (VA_LENGTHMAXLEN * 2 - VA_LENGTHMINLEN) > bufsize) {

                vtpl_llen = VTPL_LENLEN(target_vtpl);
                vtpl_nlen = vtpl_glen - vtpl_llen;

                if (s + vtpl_llen > bufsize
                ||  (vtpl_llen == VA_LENGTHMINLEN
                  && vtpl_nlen + va_glen >= VA_LONGVALIMIT))
                {
                    *p_nextpos = buf;
                    return (FALSE);
                }
            }
            vtpl_appva(target_vtpl, va);
        }
        ss_dassert(nattrs == vtpl_vacount(target_vtpl));
        vtpl_glen = VTPL_GROSSLEN(target_vtpl);
        vtpl_llen = VTPL_LENLEN(target_vtpl);
        s = vtpl_glen + vtpl_llen;
        ss_dassert(s <= bufsize);
        va_appinvlen((va_t*)target_vtpl);
        *p_nextpos = buf + s;
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		xs_tuple_makevtpl2stream
 * 
 * Outputs a stream vtuple directly to stream object (when no
 * sorting is needed)
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *		
 *	ttype - in, use
 *		tuple type
 *		
 *	tval - in, use
 *		tuple value
 *		
 *      nattrs - in
 *          number of attributes in tval & ttype
 *          (to avoid call to rs_ttype_nattrs)
 *		
 *	stream - use
 *		output stream to use
 *		
 *	p_errh - out, give
 *		in case of failure an error handle is given through
 *          this pointer
 *		
 * Return value :
 *      TRUE = success
 *      FALSE = failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tuple_makevtpl2stream(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        uint nattrs,
        xs_stream_t* stream,
        rs_err_t** p_errh)
{
        uint i;
        va_index_t vtpl_netlength;
        va_index_t vtpl_lenlength;
        ss_byte_t vtpllen_buf[VA_LENGTHMAXLEN];
        ss_byte_t vtplinvlen_buf[VA_LENGTHMAXLEN];
        bool succp;

        for (vtpl_netlength = 0, i = 0; i < nattrs; i++) {
            va_t* va;

            va = rs_tval_va(cd, ttype, tval, i);
            vtpl_netlength += VA_GROSSLEN(va);
        }
        va_setdata((va_t*)vtpllen_buf, NULL, (va_index_t)vtpl_netlength);
        va_patchinvlen(vtplinvlen_buf, (va_t*)vtpllen_buf);
        vtpl_lenlength = VA_LENLEN((va_t*)vtpllen_buf);
        succp = xs_stream_append(stream, vtpllen_buf, vtpl_lenlength, p_errh);
        if (!succp) {
            return (FALSE);
        }
        for (i = 0; i < nattrs; i++) {
            va_t* va;

            va = rs_tval_va(cd, ttype, tval, i);
            succp = xs_stream_append(
                        stream,
                        va,
                        VA_GROSSLEN(va),
                        p_errh);
            if (!succp) {
                return (FALSE);
            }
        }
        succp = xs_stream_append(stream, vtplinvlen_buf, vtpl_lenlength, p_errh);
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_tuple_filltval
 * 
 * fills data to a tval from a vtuple
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *		
 *	ttype - in, use
 *		tuple type
 *		
 *	anomap - in, use
 *		vtuple-ano to ttype-sql-ano coversion map
 *		
 *	vtpl - in, use
 *		pointer to vtuple
 *		
 *	tval - out, use
 *		pointer to tval
 *		
 * Return value :
 *      TRUE when successful,
 *      FALSE when failed (never happens!)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_tuple_filltval(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_ano_t* anomap,
        vtpl_t* vtpl,
        rs_tval_t* tval)
{
        char* vtpl_end;
        va_t* va;
#ifdef RS_REFTVALS
        dynvtpl_t vtpl2 = NULL;

        ss_dassert(vtpl_vacount(vtpl) == rs_ttype_nattrs(cd, ttype));

        dynvtpl_setvtpl(&vtpl2, vtpl);
        vtpl = vtpl2;
        rs_tval_linktovtpl(cd, ttype, tval, vtpl, vtpl, TRUE);
#endif
        vtpl_end = (char*)vtpl + VTPL_GROSSLEN(vtpl);
        va = VTPL_GETVA_AT0(vtpl);
        for (; (char*)va < vtpl_end; anomap++) {
#ifdef RS_REFTVALS
            rs_tval_setvaref(cd, ttype, tval, *anomap, va);
#else
            rs_tval_setva(cd, ttype, tval, *anomap, va);
#endif
            va = VTPL_SKIPVA(va);
        }

        ss_dassert(vtpl_vacount(vtpl) == rs_ttype_nattrs(cd, ttype));

        return (TRUE);
}
