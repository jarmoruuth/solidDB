/*************************************************************************\
**  source       * su0cfgl.c
**  directory    * su
**  description  * Configuration list function.
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
#include <sssprint.h>

#include "su0cfgl.h"

typedef struct {
        char*   ci_section;
        char*   ci_name;
        char*   ci_value;
        char*   ci_defaultval;
        int     ci_flags;
} su_cfgitem_t;

/*#***********************************************************************\
 * 
 *		cfgl_delete
 * 
 * List delete function for cfg list.
 * 
 * Parameters : 
 * 
 *	data - in, take
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
static void cfgl_delete(void* data)
{
        su_cfgitem_t* ci = data;

        SsMemFree(ci->ci_name);
        SsMemFree(ci->ci_value);
        SsMemFree(ci->ci_defaultval);
        SsMemFree(ci);
}

/*##**********************************************************************\
 * 
 *		su_cfgl_init
 * 
 * 
 * 
 * Parameters :
 * 
 * Return value - give : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_cfgl_t* su_cfgl_init(void)
{
        su_cfgl_t* cfgl;

        cfgl = su_list_init(cfgl_delete);

        return(cfgl);
}

/*##**********************************************************************\
 * 
 *		su_cfgl_done
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfgl - in, take
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
void su_cfgl_done(su_cfgl_t* cfgl)
{
        su_list_done(cfgl);
}

/*##**********************************************************************\
 * 
 *		su_cfgl_addstrparam
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfgl - use
 *		
 *		
 *	section - in, hold
 *		
 *		
 *	name - in
 *		
 *		
 *	value - in
 *		
 *		
 *	defaultval - in
 *		
 *		
 *	flags - in
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
void su_cfgl_addstrparam(
        su_cfgl_t* cfgl,
        char* section,
        char* name,
        char* value,
        char* defaultval,
        int flags)
{
        su_cfgitem_t* ci;

        ss_dassert(!(flags & SU_CFGL_LONGPARAM));
        ss_dassert(!(flags & SU_CFGL_BOOLPARAM));

        ci = SSMEM_NEW(su_cfgitem_t);

        ci->ci_section = section;
        ci->ci_name = SsMemStrdup(name);
        ci->ci_value = SsMemStrdup(value);
        ci->ci_defaultval = SsMemStrdup(defaultval);
        ci->ci_flags = flags | SU_CFGL_STRPARAM;

        su_list_insertlast(cfgl, ci);
}

/*##**********************************************************************\
 * 
 *		su_cfgl_addlongparam
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfgl - use
 *		
 *		
 *	section - in, hold
 *		
 *		
 *	name - in
 *		
 *		
 *	value - in
 *		
 *		
 *	defaultval - in
 *		
 *		
 *	flags - in
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
void su_cfgl_addlongparam(
        su_cfgl_t* cfgl,
        char* section,
        char* name,
        long value,
        long defaultval,
        int flags)
{
        char buf[20];
        su_cfgitem_t* ci;

        ss_dassert(!(flags & SU_CFGL_STRPARAM));
        ss_dassert(!(flags & SU_CFGL_BOOLPARAM));

        ci = SSMEM_NEW(su_cfgitem_t);

        ci->ci_section = section;
        ci->ci_name = SsMemStrdup(name);
        SsSprintf(buf, "%ld", value);
        ci->ci_value = SsMemStrdup(buf);
        SsSprintf(buf, "%ld", defaultval);
        ci->ci_defaultval = SsMemStrdup(buf);
        ci->ci_flags = flags | SU_CFGL_LONGPARAM;

        su_list_insertlast(cfgl, ci);
}

/*##**********************************************************************\
 * 
 *		su_cfgl_addboolparam
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfgl - use
 *		
 *		
 *	section - in, hold
 *		
 *		
 *	name - in
 *		
 *		
 *	value - in
 *		
 *		
 *	defaultval - in
 *		
 *		
 *	flags - in
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
void su_cfgl_addboolparam(
        su_cfgl_t* cfgl,
        char* section,
        char* name,
        bool value,
        bool defaultval,
        int flags)
{
        su_cfgitem_t* ci;

        ss_dassert(!(flags & SU_CFGL_STRPARAM));
        ss_dassert(!(flags & SU_CFGL_LONGPARAM));

        ci = SSMEM_NEW(su_cfgitem_t);

        ci->ci_section = section;
        ci->ci_name = SsMemStrdup(name);
        ci->ci_value = SsMemStrdup(value ? (char *)"yes" : (char *)"no");
        ci->ci_defaultval = SsMemStrdup(defaultval ? (char *)"yes" : (char *)"no");
        ci->ci_flags = flags | SU_CFGL_BOOLPARAM;

        su_list_insertlast(cfgl, ci);
}

/*##**********************************************************************\
 * 
 *		su_cfgl_addlong
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfgl - use
 *		
 *		
 *	inifile - in
 *		
 *		
 *	section - in, hold
 *		
 *		
 *	param - in
 *		
 *		
 *	defaultval - in
 *		
 *		
 *	flags - in
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
void su_cfgl_addlong(
        su_cfgl_t* cfgl,
        su_inifile_t* inifile,
        const char* section,
        const char* param,
        long defaultval,
        int flags)
{
        bool found;
        long l;

        ss_dassert(!(flags & SU_CFGL_ISDEFAULT));

        found = su_inifile_getlong(inifile, section, param, &l);
        if (!found) {
            flags |= SU_CFGL_ISDEFAULT;
            l = defaultval;
        }
        su_cfgl_addlongparam(cfgl, (char *)section, (char *)param, l, defaultval, flags);
}

/*##**********************************************************************\
 * 
 *		su_cfgl_addbool
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfgl - use
 *		
 *		
 *	inifile - in
 *		
 *		
 *	section - in, hold
 *		
 *		
 *	param - in
 *		
 *		
 *	defaultval - in
 *		
 *		
 *	flags - in
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
void su_cfgl_addbool(
        su_cfgl_t* cfgl,
        su_inifile_t* inifile,
        const char* section,
        const char* param,
        bool defaultval,
        int flags)
{
        bool found;
        bool b;
        char* s = NULL;

        ss_dassert(!(flags & SU_CFGL_ISDEFAULT));

        found = su_inifile_getstring(inifile, section, param, &s);
        if (!found) {
            flags |= SU_CFGL_ISDEFAULT;
            b = defaultval;
        } else {
            b = (*s == 'y' || *s == 'Y');
        }                
        su_cfgl_addboolparam(cfgl, (char *)section, (char *)param, b, defaultval, flags);
        if (s != NULL) {
            SsMemFree(s);
        }
}

/*##**********************************************************************\
 * 
 *		su_cfgl_addstr
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfgl - use
 *		
 *		
 *	inifile - in
 *		
 *		
 *	section - in, hold
 *		
 *		
 *	param - in
 *		
 *		
 *	defaultval - in
 *		
 *		
 *	flags - in
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
void su_cfgl_addstr(
        su_cfgl_t* cfgl,
        su_inifile_t* inifile,
        const char* section,
        const char* param,
        const char* defaultval,
        int flags)
{
        bool found;
        char* s;

        ss_dassert(!(flags & SU_CFGL_ISDEFAULT));

        found = su_inifile_getstring(inifile, section, param, &s);
        if (!found) {
            flags |= SU_CFGL_ISDEFAULT;
            s = SsMemStrdup((char *)defaultval);
        }
        su_cfgl_addstrparam(cfgl, (char *)section, (char *)param, s, (char *)defaultval, flags);
        SsMemFree(s);
}

/*##**********************************************************************\
 * 
 *		su_cfgl_getparam
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cfgl_node - in
 *		
 *		
 *	p_section - out, ref
 *		
 *		
 *	p_name - out, ref
 *		
 *		
 *	p_value - out, ref
 *		
 *		
 *	p_defaultval - out, ref
 *		
 *		
 *	p_flags - out
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
void su_cfgl_getparam(
        su_list_node_t* cfgl_node,
        char** p_section,
        char** p_name,
        char** p_value,
        char** p_defaultval,
        int* p_flags)
{
        su_cfgitem_t* ci;

        ci = su_listnode_getdata(cfgl_node);

        *p_section = ci->ci_section;
        *p_name = ci->ci_name;
        *p_value = ci->ci_value;
        *p_defaultval = ci->ci_defaultval;
        *p_flags = ci->ci_flags;
}
