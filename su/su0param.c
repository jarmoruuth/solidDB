/*************************************************************************\
**  source       * su0param.c
**  directory    * su
**  description  * Parameter handling routines.
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

#include <ssenv.h>

#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sschcvt.h>
#include <sssem.h>
#include <ssscan.h>
#include <sssprint.h>

#include <su0types.h>
#include <su0param.h>
#include <su0list.h>
#include <su0vfil.h>

#include <su0cfgst.h>

#ifndef SU_PARAM_CLI
#define SU_PARAM_SRV
#endif

#define SU_PARAM_SEM_ENTER /* SsSemEnter(param_manager->param_manager_sem) */
#define SU_PARAM_SEM_EXIT  /* SsSemExit(param_manager->param_manager_sem)  */


ss_debug(static bool param_readonly = FALSE;)

#ifdef P_ATTR
    error error
#endif
#define P_ATTR(x)   ((x) & SU_PARAM_AM_ATTRIBUTES_MASK)
#define P_NOATTR(x) ((x) & SU_PARAM_AM_NOATTRIBUTES_MASK)
    
#ifdef SU_PARAM_SRV

typedef struct su_param_manager_st {
        SsSemT*         param_manager_sem;
        su_inifile_t*   param_manager_inifile;
        su_list_t*      param_manager_list;
        bool            param_manager_save;
} su_param_manager_t;

/* some fields needed for extended features,
 * su_param uses these
 */
struct su_param_st {
        char*          param_section_name;
        char*          param_parameter_name;
        char*          param_default_value;
        char*          param_current_value;
        char*          param_factory_value;
        char*          param_description_text;
        bool           param_save;
        bool           param_delete;
        bool           param_tmp;
        bool           param_found_in_inifile;
        su_param_type_t        param_type;
        su_param_access_mode_t param_access_mode;
        su_param_set_cb_t      param_set_cb;
        su_param_set_cb_ctx_t  param_set_cb_ctx;
        su_param_get_cb_t      param_get_cb;
        void*                  param_ctx;
};

static su_param_manager_t* param_manager = NULL;
static char* su_param_manager_ifname = NULL;
static int nlinks = 0;

/*
 * structure to return access more in string
 */
static const char* su_param_access_mode_strings[] =
            {"N/A", "RW", "RW/STARTUP", "RW/CREATE", "RO"};

/*
 * structure to return parameter type in string
 */
static const char* su_param_type_strings[] =
            {"N/A", "LONG", "DOUBLE", "STR", "BOOL", "LISTEN", "BIGINT" };

/* prototypes */
static su_ret_t su_param_set_values_ex(
        const char* section_name,
        const char* parameter_name,
        const char* value,
        bool tmp_value,
        bool reset_to_default,
        bool reset_to_factory,
        su_err_t** err,
        bool* changed,
        bool do_callback);

#endif /* SU_PARAM_SRV */

static bool su_param_getvalue_ex(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_give);

static char* su_param_initfactory_to_string(su_initparam_t* param, char* buf);

#ifdef SU_PARAM_SRV
/*##**********************************************************************\
 *
 *      su_param_gettype
 *
 * Return the type of the parameter.
 *
 * Parameters:
 *      param - in
 *          parameter of interest
 *
 * Return value:
 *      the type of the parameter.
 *
 * Limitations:
 *
 * Globals used:
 */
su_param_type_t su_param_gettype(su_param_t* param)
{
        return param->param_type;
}
#endif /* SU_PARAM_SRV */

#ifdef SU_PARAM_SRV
/*#***********************************************************************\
 *
 *		su_param_check_value_type
 *
 *
 *
 * Parameters :
 *
 *	type -
 *
 *
 *	value -
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
static bool su_param_check_value_type(su_param_type_t type, char* value) {
/*          char* ptr = value; */

        ss_dprintf_4(("su_param_check_value_type: type = %s, value = %s\n",
                      su_param_type_strings[type], value));

        switch(type) {
            case SU_PARAM_TYPE_NA:
            case SU_PARAM_TYPE_STR:
                return TRUE;
            case SU_PARAM_TYPE_DOUBLE: {
                double t;
                char* tmp;
                if (!SsStrScanDouble(value, &t, &tmp)) {
                    return FALSE;
                }
                return TRUE;
            }
            case SU_PARAM_TYPE_LONG: {
                long t;
                char* tmp;
                if (!SsStrScanLong(value, &t, &tmp)) {
                    return FALSE;
                }
                return TRUE;
            }
            case SU_PARAM_TYPE_INT8: {
                ss_int8_t t;
                char* tmp;
                if (!SsStrScanInt8(value, &t, &tmp)) {
                    return (FALSE);
                }
                return (TRUE);
            }
            case SU_PARAM_TYPE_BOOL:
                if (SsStricmp(value, "yes") && SsStricmp(value, "no")) {
                    return FALSE;
                }
                return TRUE;
            case SU_PARAM_TYPE_LISTEN:
                return TRUE;
            default:
                ss_rc_error(type);
        }
        return TRUE;
}


/*#***********************************************************************\
 *
 *		su_param_init
 *
 *
 *
 * Parameters :
 *
 *	section_name -
 *
 *
 *	parameter_name -
 *
 *
 *	found_in_inifile -
 *
 *
 *	default_value -
 *
 *
 *	current_value -
 *
 *
 *	factory_value -
 *
 *
 *	description_text -
 *
 *
 *	type -
 *
 *
 *	access_mode -
 *
 *
 *	set_cb -
 *
 *
 *	get_cb -
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
static su_param_t* su_param_init(
        char* section_name,
        char* parameter_name,
        bool found_in_inifile,
        char* default_value,
        char* current_value,
        char* factory_value,
        char* description_text,
        su_param_type_t type,
        su_param_access_mode_t access_mode,
        su_param_set_cb_t set_cb,
        su_param_set_cb_ctx_t set_cb_ctx,
        su_param_get_cb_t get_cb,
        void* ctx)
{
        su_param_t* param;
        int len;

        ss_dassert(section_name != NULL);
        ss_dassert(parameter_name != NULL);
        ss_info_dassert((int)P_NOATTR(access_mode) >= 0
                        && (P_NOATTR(access_mode)
                            < (sizeof(su_param_access_mode_strings)
                               / sizeof(su_param_access_mode_strings[0]))),
                        ("%s.%s: access_mode = %02x", section_name,
                         parameter_name, access_mode));
        
        ss_dassert((int)type >= 0 &&
                   type < (sizeof(su_param_type_strings) / sizeof(su_param_type_strings[0])));

        param = SsMemAlloc(sizeof(su_param_t));

        param->param_found_in_inifile = found_in_inifile;
        param->param_section_name   = SsMemStrdup(section_name);
        param->param_parameter_name = SsMemStrdup(parameter_name);
        param->param_default_value  = default_value ? SsMemStrdup(default_value) : NULL;
        if (param->param_default_value) {
            len = strlen(param->param_default_value);
            if (len > 0 && param->param_default_value[len-1] == '\n') {
                param->param_default_value[len-1] = '\0';
            }
        }
        param->param_current_value = current_value ? SsMemStrdup(current_value) : NULL;

        ss_dassert(su_param_check_value_type(type, factory_value));
        param->param_factory_value = factory_value ? SsMemStrdup(factory_value) : NULL;
        param->param_description_text = description_text ? SsMemStrdup(description_text) : NULL;
        param->param_type = type;
        if (access_mode & (SU_PARAM_AM_UNPUBLISHED | SU_PARAM_AM_SYSTEM)) {
            /* if parameter is unpublished or system then it is invisible */
            access_mode |= SU_PARAM_AM_INVISIBLE;
        }
        /* both can't be on */
        ss_dassert( !((access_mode & SU_PARAM_AM_UNPUBLISHED)
                    && (access_mode & SU_PARAM_AM_SYSTEM)) );
        param->param_access_mode = access_mode;
        param->param_set_cb = set_cb;
        param->param_set_cb_ctx = set_cb_ctx;
        param->param_get_cb = get_cb;
        param->param_ctx = ctx;

        param->param_tmp = FALSE;
        param->param_save = FALSE;
        param->param_delete = FALSE;

        return(param);
}
#endif /* SU_PARAM_SRV */

#ifdef SU_PARAM_SRV
/*#***********************************************************************\
 *
 *		su_param_done
 *
 *
 *
 * Parameters :
 *
 *	param -
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
static void su_param_done(
        su_param_t* param)
{
        ss_dassert(param != NULL);

        if (param->param_section_name) {
            SsMemFree(param->param_section_name);
            param->param_section_name = NULL;
        }
        if (param->param_parameter_name) {
            SsMemFree(param->param_parameter_name);
            param->param_parameter_name = NULL;
        }
        if (param->param_default_value) {
            SsMemFree(param->param_default_value);
            param->param_default_value = NULL;
        }
        if (param->param_current_value) {
            SsMemFree(param->param_current_value);
            param->param_current_value = NULL;
        }
        if (param->param_factory_value) {
            SsMemFree(param->param_factory_value);
            param->param_factory_value = NULL;
        }
        if (param->param_description_text) {
            SsMemFree(param->param_description_text);
            param->param_description_text = NULL;
        }
        SsMemFree(param);
}
#endif /* SU_PARAM_SRV */

#ifdef SU_PARAM_SRV
/*#***********************************************************************\
 *
 *		param_find
 *
 *
 *
 * Parameters :
 *
 *	section_name -
 *
 *
 *	parameter_name -
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
static su_param_t* param_find(
        char* section_name,
        char* parameter_name)
{
        su_param_t* param = NULL;
        su_param_t* found_param = NULL;
        su_list_node_t* n;

        ss_assert(section_name != NULL);
        ss_assert(parameter_name != NULL);

        su_list_do_get(param_manager->param_manager_list, n, param) {
            if ((SsStricmp(parameter_name, param->param_parameter_name) == 0) &&
                (SsStricmp(section_name, param->param_section_name) == 0)) {
                ss_dassert(found_param == NULL);
                found_param = param;
                break;
            }
        }

        return(found_param);
}
#endif /* SU_PARAM_SRV */



/*##**********************************************************************\
 *
 *		su_param_manager_isinitialized
 *
 *
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool su_param_manager_isinitialized(void)
{
#ifdef SU_PARAM_SRV
        if (param_manager == NULL) {
            return FALSE;
        } else {
            return TRUE;
        }
#endif /* SU_PARAM_SRV */
#ifdef SU_PARAM_CLI
        return(FALSE);
#endif /* SU_PARAM_SRV */
}

/*##**********************************************************************\
 *
 *		su_param_manager_global_init()
 *
 * Inititalizes parameter manager object which is inifile-object
 *
 * Parameters :
 *
 * su_inifile_t* inifile or NULL
 *
 *
 *
 *
 * Return value :
 *
 * true on success, false otherwise
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */

bool su_param_manager_global_init(
    su_inifile_t* inifile)
{
#ifdef SU_PARAM_SRV
        bool succp;

        ss_dprintf_3(("su_param_manager_global_init\n"));

        nlinks++;

        ss_dprintf_3(("... su_param_manager_global_init: nlinks -> %d\n",
                      nlinks));
        
        ss_debug(param_readonly = FALSE;)

        if (param_manager) {
            return FALSE;
        }
        param_manager = SsMemAlloc(sizeof(su_param_manager_t));
        param_manager->param_manager_sem = SsSemCreateLocal(SS_SEMNUM_SU_PARAM);

        if (!su_param_manager_ifname) {
            su_param_manager_ifname = SU_SOLINI_FILENAME;
        }

        if (inifile == NULL) {
            param_manager->param_manager_inifile =
                    su_inifile_init(su_param_manager_ifname, &succp);
        } else {
            param_manager->param_manager_inifile = inifile;
            succp = TRUE;
        }
        
        su_inifile_ordersections(param_manager->param_manager_inifile, TRUE);
        param_manager->param_manager_list =
                    su_list_init((void(*)(void*))su_param_done);

        param_manager->param_manager_save = FALSE;

        return succp;

#endif /* SU_PARAM_SRV */
#ifdef SU_PARAM_CLI
        return(TRUE);
#endif /* SU_PARAM_SRV */
}

#ifdef SU_PARAM_SRV
/*##**********************************************************************\
 *
 *		su_param_manager_set_ifname
 *
 *
 *
 * Parameters :
 *
 *	ifname -
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
void su_param_manager_set_ifname(
        char* ifname)
{
        if (su_param_manager_isinitialized()) {
            ss_info_dassert(0, (("Parameter manager is already initialized--too late to change the inifile's name.\n")));
        }
        su_param_manager_ifname = ifname;

        ss_dprintf_2(("su_param_manager_set_ifname: now ifname = '%s'\n",
                      ifname));
}
#endif /* SU_PARAM_SRV */


/*##**********************************************************************\
 *
 *		su_param_manager_global_done_force
 *
 * Unconditionally closes param manager even if there are still links
 * left.
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 *      Number of active users to param manager, zero when memory released.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void su_param_manager_global_done_force(void)
{
#ifdef SU_PARAM_SRV

        if (param_manager == NULL) {
            return;
        }

#if 1 /* mr 20040603: removes duplication */
        ss_dprintf_3(("su_param_manager_global_done_force\n"));

        su_param_manager_save();
        
#else /* old code */
        
        bool needsave = FALSE;
        su_param_t* param;
        su_list_node_t* n;
        bool b;

        ss_dprintf_3(("su_param_manager_global_done_force\n"));
        SU_PARAM_SEM_ENTER;
        /* loop params */

        su_list_do_get(param_manager->param_manager_list, n, param) {
            if (!param->param_tmp) {
                if (param->param_delete) {
                    if (param->param_found_in_inifile) {
                        ss_dprintf_3(("deletekeyline:[%s].%s\n",
                                param->param_section_name,
                                param->param_parameter_name));
                        b = su_inifile_deletekeyline(
                                        param_manager->param_manager_inifile,
                                        param->param_section_name,
                                        param->param_parameter_name);
                        ss_dassert(b);
                        if (b) {
                            needsave = TRUE;
                        }
                    }
                } else if (param->param_save) {
                    char* value = param->param_current_value;
                    if (value == NULL) {
                        value = "";
                    }
                    ss_dprintf_3(("savekeyline:[%s].%s\n",
                            param->param_section_name,
                            param->param_parameter_name));
                    su_inifile_putstring(
                                    param_manager->param_manager_inifile,
                                    param->param_section_name,
                                    param->param_parameter_name,
                                    value);
                    needsave = TRUE;

#if 0 /* mr 20040603: no we won't */
                    /* set default value to match that what is saved into the
                     * inifile */
                    if (param->param_default_value != NULL) {
                        SsMemFree(param->param_default_value);
                    }
                    param->param_default_value = SsMemStrdup(value);
#endif
                }
            }
        }

        if (needsave || param_manager->param_manager_save) {
            su_inifile_save(param_manager->param_manager_inifile);
            needsave = FALSE;
        }
        SU_PARAM_SEM_EXIT;
#endif /* old code */
        
        su_inifile_done(param_manager->param_manager_inifile);
        su_list_done(param_manager->param_manager_list);

        SsSemFree(param_manager->param_manager_sem);
        SsMemFree(param_manager);
        param_manager = NULL;

#endif /* SU_PARAM_SRV */
}

/*##**********************************************************************\
 *
 *		su_param_manager_global_done
 *
 * Conditionally closes param manager if there are no links left.
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 *      Number of active users to param manager, zero when memory released.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int su_param_manager_global_done(void)
{
#ifdef SU_PARAM_SRV
        ss_dprintf_3(("su_param_manager_global_done\n"));
        SU_PARAM_SEM_ENTER;
        /* loop params */

        nlinks--;

        ss_dprintf_3(("... su_param_manager_global_done: nlinks -> %d\n",
                      nlinks));
        
        ss_dassert(nlinks >= 0);
        if (nlinks > 0) {
            SU_PARAM_SEM_EXIT;
            return(nlinks);
        }
        SU_PARAM_SEM_EXIT;

        su_param_manager_global_done_force();

        return(0);
#endif /* SU_PARAM_SRV */
#ifdef SU_PARAM_CLI
        return(0);
#endif /* SU_PARAM_SRV */
}

#if defined(SS_DEBUG)

void su_param_manager_setreadonly(void)
{
        ss_dprintf_3(("su_param_manager_setreadonly\n"));

        param_readonly = TRUE;
}

#endif /* SS_DEBUG */

#ifdef SU_PARAM_SRV
/*#***********************************************************************\
 *
 *		param_callback
 *
 *
 *
 * Parameters :
 *
 *	param -
 *
 *
 *	value -
 *
 *
 *	tmpval -
 *
 *
 *	err -
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
static su_ret_t param_callback(
        su_param_t* param,
        char* value,
        bool tmpval,
        su_err_t** err)
{
        su_ret_t rc = SU_SUCCESS;

        ss_dassert(su_vfh_isinitialized());

        ss_dprintf_4(("su0param: param_callback: %.255s.%.255s, callback = %.255s, value = '%.255s'\n", param->param_section_name, param->param_parameter_name,
                      (param->param_set_cb != NULL || param->param_set_cb_ctx) ? "yes": "(null)", value));

        if (param->param_set_cb != NULL || param->param_set_cb_ctx) {
            if (param->param_set_cb != NULL) {
                ss_dassert(param->param_set_cb_ctx == NULL);
                ss_dassert(param->param_ctx == NULL);
                rc = (*param->param_set_cb)(value,
                           tmpval ? NULL : &param->param_default_value,
                           value ? value : param->param_factory_value,
                           (P_NOATTR(param->param_access_mode)
                            == SU_PARAM_AM_RWSTARTUP)
                           ? NULL : &param->param_current_value,
                           &param->param_factory_value);
            } else {
                ss_dassert(param->param_set_cb == NULL);
                rc = (*param->param_set_cb_ctx)(
                            param->param_ctx,
                            param->param_section_name,
                            param->param_parameter_name,
                            value,
                            tmpval ? NULL : &param->param_default_value,
                            value ? value : param->param_factory_value,
                            (P_NOATTR(param->param_access_mode)
                            == SU_PARAM_AM_RWSTARTUP)
                                ? NULL 
                                : &param->param_current_value,
                            &param->param_factory_value);
            }

            if (rc != SU_SUCCESS) {
                if (rc == SU_ERR_PARAM_VALUE_INVALID
                    || rc == SU_ERR_PARAM_VALUE_TOOSMALL
                    || rc == SU_ERR_PARAM_VALUE_TOOBIG) {

                    /* all as INVALID because we don't have all the
                     * parameters for TOOBIG and TOOSMALL */
                    su_err_init(err, SU_ERR_PARAM_VALUE_INVALID,
                                value,
                                param->param_section_name,
                                param->param_parameter_name);
                } else if (rc == SU_ERR_CB_ERROR) {
                    su_err_init(err, SU_ERR_CB_ERROR,
                                param->param_section_name,
                                param->param_parameter_name);
                } else if ( (   rc == SU_ERR_READONLY)
                            || (rc == SU_ERR_ILL_VALUE_TYPE)) {

                    su_err_init(err, rc); /* RO and ILL are parameterless */
                } else {
                    ss_info_dassert(0,("rc = %d, which param_callback is not prepared for\n", (int)rc));
                    su_err_init(err, SU_ERR_PARAM_VALUE_INVALID,
                                value,
                                param->param_section_name,
                                param->param_parameter_name);
                }
            }
        }

        ss_dprintf_4(("  param_callback: rc = %ld\n", (long) rc));

        return(rc);
}

/*#***********************************************************************\
 *
 *		param_reset_to_factory
 *
 *
 *
 * Parameters :
 *
 *	param -
 *
 *
 *	tmp_value -
 *
 *
 *	changed -
 *
 *
 *	err -
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
static su_ret_t param_reset_to_factory(
        su_param_t* param,
        bool tmp_value,
        bool* changed,
        su_err_t** err)
{
        su_ret_t rc;

        ss_dassert(param != NULL);

        rc = param_callback(param, param->param_factory_value, tmp_value, err);
        if (rc != SU_SUCCESS) {
            return(rc);
        }

        param->param_tmp = tmp_value;
        *changed = FALSE;
        if (param->param_current_value != NULL) {
            SsMemFree(param->param_current_value);
            param->param_current_value = NULL;
            *changed = TRUE;
        }
        if (param->param_default_value != NULL) {
            SsMemFree(param->param_default_value);
            param->param_default_value = NULL;
            *changed = TRUE;
        }
        if (!tmp_value && *changed) {
            ss_dassert(!param_readonly);
            param->param_delete = TRUE;
        }
        return(rc);
}

/*#***********************************************************************\
 *
 *		param_reset_to_default
 *
 *
 *
 * Parameters :
 *
 *	param -
 *
 *
 *	tmp_value -
 *
 *
 *	changed -
 *
 *
 *	err -
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
static su_ret_t param_reset_to_default(
        su_param_t* param,
        bool tmp_value,
        bool* changed,
        su_err_t** err)
{
        su_ret_t rc;

        ss_dassert(param != NULL);

        rc = param_callback(param, param->param_default_value, tmp_value, err);
        if (rc != SU_SUCCESS) {
            return(rc);
        }

        param->param_tmp = tmp_value;
        *changed = FALSE;
        if (param->param_current_value != NULL) {
            SsMemFree(param->param_current_value);
            param->param_current_value = NULL;
            *changed = TRUE;
        }
        param->param_save = FALSE;
        param->param_delete = FALSE;
        return(rc);
}

/*#***********************************************************************\
 *
 *		param_setvalue
 *
 *
 *
 * Parameters :
 *
 *	param -
 *
 *
 *	value -
 *
 *
 *	tmp_value -
 *
 *
 *	changed -
 *
 *
 *	err -
 *
 *
 *  do_callback - in
 *      if to call the set-callback function.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static su_ret_t param_setvalue(
        su_param_t* param,
        char* value,
        bool tmp_value,
        bool* changed,
        su_err_t** err,
        bool do_callback)
{
        su_ret_t rc = SU_SUCCESS;
        ss_dassert(param != NULL);

        if (do_callback) {
            rc = param_callback(param, value, tmp_value, err);
            if (rc != SU_SUCCESS) {
                return(rc);
            }
        } else {
            ss_dassert(su_vfh_isinitialized()); /* checked in
                                                 * param_callback too */
        }

        param->param_tmp = tmp_value;
        if (param->param_current_value != NULL) {
            SsMemFree(param->param_current_value);
            param->param_current_value = NULL;
            *changed = TRUE;
        }
        if (value != NULL) {
            if (param->param_type == SU_PARAM_TYPE_BOOL) {
                /* fix bool outlook */
                bool b, r;
                r = su_param_str_to_bool(value, &b);
                ss_dassert(r);

                param->param_current_value = SsMemStrdup((b) ? (char *)"Yes": (char *)"No");
            } else {
                param->param_current_value = SsMemStrdup(value);
            }
            *changed = TRUE;
        }
        param->param_delete = FALSE;
        if (tmp_value) {
            param->param_save = FALSE;
        } else {
            ss_dassert(!param_readonly);
            param->param_save = TRUE;
        }
        return(rc);
}


/*##**********************************************************************\
 *
 *		su_param_str_to_bool
 *
 *
 *
 * Parameters :
 *
 *	value_str -
 *
 *
 *	value -
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
bool su_param_str_to_bool(
        char* value_str,
        bool* value)
{
        bool succp = FALSE;

        ss_dassert(value_str != NULL);
        ss_dassert(value != NULL);
        
        if (value_str != NULL) {
            char first = value_str[0];
            if (first == 'Y' || first == 'y') {
                *value = TRUE;
                succp = TRUE;
            } else if (first == 'N' || first == 'n') {
                *value = FALSE;
                succp = TRUE;
            }
        }
        
        return(succp);
}

/*##**********************************************************************\
 *
 *		su_param_str_to_long
 *
 *
 *
 * Parameters :
 *
 *	value_str -
 *
 *
 *	value -
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
bool su_param_str_to_long(
        char* value_str,
        long* value)
{
        char* tmp;
        bool succp;

        ss_dassert(value_str != NULL);
        ss_dassert(value     != NULL);
        if (value_str == NULL) {
            return FALSE;
        }

        succp = SsStrScanLong(value_str, value, &tmp);
        if (succp) {
            /* Check if there is kilo or mega specification.
             */
            char* p;
            long mult = 1;
            p = SsStrTrimLeft(tmp);
            switch (*p) {
                case 'k':
                case 'K':
                    /* Value is kilobytes. */
                    mult = 1024L;
                    break;
                case 'm':
                case 'M':
                    /* Value is megabytes. */
                    mult = 1024L * 1024L;
                    break;
                default:
                    break;
            }
            *value *= mult;
        }
        return(succp);
}


/*##**********************************************************************\
 *
 *		su_param_str_to_double
 *
 *
 *
 * Parameters :
 *
 *	value_str -
 *
 *
 *	value -
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
bool su_param_str_to_double(
        char* value_str,
        double* value)
{
        bool succp = FALSE;

        ss_dassert(value_str != NULL);
        ss_dassert(value     != NULL);
        if (value_str != NULL) {
            char* tmp;
            succp = SsStrScanDouble(value_str, value, &tmp);
        }
        return(succp);
}
#endif /* SU_PARAM_SRV */

/*##**********************************************************************\
 *
 *		su_param_str_to_int8
 *
 *
 *
 * Parameters :
 *
 *	value_str - in
 *
 *
 *	value - in, use
 *
 *
 * Return value :
 *    TRUE, if success
 *    FALSE, if not           
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool su_param_str_to_int8(
        char* value_str,
        ss_int8_t* value)
{
        char* tmp;
        bool succp;

        ss_dassert(value_str != NULL);
        ss_dassert(value     != NULL);

        if (value_str == NULL) {
            return FALSE;
        }

        succp = SsStrScanInt8(value_str, value, &tmp);
        if (succp) {
            /* Check if there is kilo or mega specification.
             */
            char* p;
            ss_int8_t mult;

            p = SsStrTrimLeft(tmp);
            switch (*p) {
                case 'k':
                case 'K':
                    /* Value is kilobytes. */
                    SsInt8SetUint4(&mult, (ss_uint4_t)1024);
                    break;
                case 'm':
                case 'M':
                    /* Value is megabytes. */
                    SsInt8SetUint4(&mult, (ss_uint4_t)(1024L * 1024L));
                    break;
                case 'g':
                case 'G':
                    /* Value is gigabytes. */
                    SsInt8SetUint4(&mult, (ss_uint4_t)(1024L * 1024L * 1024L));
                    break;
                default:
                    goto void_multiply;
            }
            SsInt8MultiplyByInt8(value, *value, mult);
 void_multiply:;
        }
        
        return(succp);
}

static bool param_register_ex(
        char* section,
        char* name,
        char* default_value,
        char* current_value,
        char* factory_value,
        char* description_text,
        su_param_set_cb_t set_cb,
        su_param_set_cb_ctx_t set_cb_ctx,
        su_param_get_cb_t get_cb,
        su_param_type_t type,
        su_param_access_mode_t access_mode,
        void* ctx)
{
#ifdef SU_PARAM_SRV
        su_param_t* param = NULL;
        char* cp;
        bool found;

        ss_dassert(param_manager);
        ss_dassert(default_value == NULL);
        ss_info_dassert(factory_value != NULL,
                        ("Param %s.%s has NULL factory value", section, name));
        
        param = param_find(section, name);
        if (param != NULL) {
            return(TRUE);
        }

        found = su_inifile_getvalue(param_manager->param_manager_inifile,
                                    section,
                                    name,
                                    &default_value);

        if (found) {
            ss_dassert(default_value != NULL);

            /* remove comment from line end if present */
            cp = strchr(default_value, INIFILE_COMMENTCHAR);
            if (cp != NULL) {
                *cp = '\0';
            }            
        }

        if (current_value != NULL) {
            /* we did not allocate current_value so we don't need to
             * keep exact track of it (and so we can use SsStrTrim).
             */
            current_value = SsStrTrim(current_value);
        }

        param = su_param_init(
                    section,
                    name,
                    found,
                    (found) ? SsStrTrim(default_value) : default_value,
                    current_value,
                    factory_value,
                    description_text,
                    type,
                    access_mode,
                    set_cb,
                    set_cb_ctx,
                    get_cb,
                    ctx);

        if (found) {
            SsMemFree(default_value);
        }

        su_list_insertlast(param_manager->param_manager_list, param);
#endif /* SU_PARAM_SRV */
        return TRUE;
}

/*##**********************************************************************\
 *
 *		su_param_register
 *
 * Creates one parameter. The default_value is derived from the
 * inifile object and therefore must be NULL. It is possible to set the
 * current_value, for instance to fill it with value from command line
 * options.
 *
 * Parameters :
 *
 *	    section - name of the section where parameter is intended to belong
 *
 *      name - in, use
 *              name of the parameter (must be unique within section)
 *
 *      default_value - in, use
 *              current value for the parameter
 *
 *      current_value - in, use
 *              factory value for this parameter, may be NULL
 *
 *      factory_value - in, use
 *              factory value for this parameter, may be NULL
 *
 *      set_cb - pointer to callback function which is called when parameter value will be set
 *
 *      get_cb - pointer to callback function which is called when parameter value is queried
 *
 *      type - type of parameter
 *
 *      access_mode - parameters access mode
 *
 *
 * Return value :
 *
 * TRUE on success and when trying to insert same parameter twice
 * FALSE otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool su_param_register(
        const char* section,
        const char* name,
        const char* default_value,
        const char* current_value,
        const char* factory_value,
        const char* description_text,
        su_param_set_cb_t set_cb,
        su_param_get_cb_t get_cb,
        su_param_type_t type,
        su_param_access_mode_t access_mode)
{
        bool b;

        b = param_register_ex(
                (char *)section,
                (char *)name,
                (char *)default_value,
                (char *)current_value,
                (char *)factory_value,
                (char *)description_text,
                set_cb,
                NULL,
                get_cb,
                type,
                access_mode,
                NULL);
        return(b);
}

/*##**********************************************************************\
 *
 *      su_param_register_dynfactory
 *
 * Register a parameter and give it a factory value to use instead of the
 * one (if any) set in the init struct.
 *
 * Set only the value type is relevant and set other to NULL, 0, 0.0 or 0,
 * according to their types. Other types will be just ignored.
 *
 * Parameters:
 *      arr - in
 *          array containing the parameter's info
 *
 *      section - in
 *          parameter's section name
 *
 *      keyname - in
 *          parameter's keyname
 *
 *      str - in
 *          string type factory value
 *
 *      l - in
 *          long type factory value
 *
 *      d - in
 *          double type factory value
 *
 *      b - in
 *          bool type factory value
 *
 * Return value:
 *      TRUE, if found and registered.
 *      FALSE, if not.
 *
 * Limitations:
 *
 * Globals used:
 */
bool su_param_register_dynfactory(su_initparam_t arr[], char* section,
                                  char* keyname, char* str, long l,
                                  double d, bool b)
{
        su_initparam_t* param = NULL;
        su_initparam_t tmp;
        char* f;
        bool succ = FALSE;
        
        for (param = arr; param->p_section != NULL; param++) {
            if ((   SsStricmp(param->p_section, section) == 0)
                && (SsStricmp(param->p_keyname, keyname) == 0)) {
                
                tmp.p_paramtype     = param->p_paramtype;
                tmp.p_defaultstr    = str;
                tmp.p_defaultlong   = l;
                tmp.p_defaultdouble = d;
                tmp.p_defaultbool   = b;
                f = su_param_initfactory_to_string(&tmp, NULL);
                
                succ = su_param_register(
                        param->p_section,
                        param->p_keyname,
                        NULL,
                        NULL,
                        f,
                        param->p_description,
                        param->p_setfun,
                        param->p_getfun,
                        param->p_paramtype,
                        param->p_accessmode);
                ss_dassert(succ);
                
                SsMemFree(f);
                
                break;
            }
        }

        return succ;
}
                
/*##**********************************************************************\
 *
 *		su_param_isregistered
 *
 * Check if parameter is registered
 *
 * Parameters :
 *
 *	section -
 *
 *	keyname -
 *
 * Return value :
 *
 *   TRUE
 *   FALSE
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool su_param_isregistered(char* section, char* keyname)
{
#ifdef SU_PARAM_SRV

        su_param_t* param;

        param = param_find(section, keyname);
        if (param != NULL) {
            return TRUE;
        }
#endif
        return FALSE;
}

/*##**********************************************************************\
 *
 *		su_param_switchtoreadonly
 *
 * Switch parameter to read-only
 *
 * Parameters :
 *
 *	section -
 *
 *	keyname -
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void su_param_switchtoreadonly(char* section, char* keyname)
{
#ifdef SU_PARAM_SRV
        su_param_t* param;

        param = param_find(section, keyname);
        if (param != NULL) {
            param->param_access_mode = SU_PARAM_AM_RONLY
                | P_ATTR(param->param_access_mode);
        }
#endif
}

/*##**********************************************************************\
 *
 *      su_param_switchtotemporary
 *
 * Switches parameter to be temporary and not saved.
 *
 * Parameters:
 *      section - in
 *          section name
 *
 *      keyname - in
 *          parameter name
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void su_param_switchtotemporary(char* section, char* keyname)
{
#ifdef SU_PARAM_SRV
        su_param_t* param;

        ss_error; /* this probably does not work after the value has
                   * been set once.  this is because the temporary status
                   * is changed when value is set. */

        param = param_find(section, keyname);
        if (param != NULL) {
            param->param_tmp  = TRUE;
            param->param_save = FALSE;
        }
#endif
}

/*##**********************************************************************\
 *
 *		su_param_remove
 *
 * Remove parameter
 *
 * Parameters :
 *
 *	section -
 *
 *	keyname -
 *
 * Return value :
 *
 *    TRUE -  parameter removed
 *    FALSE - parameter not found
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool su_param_remove(char* section, char* keyname)
{
#ifdef SU_PARAM_SRV
        su_list_node_t* n;
        su_param_t* param;
        bool found;

        ss_dassert(!param_readonly);

        found = FALSE;
        su_list_do_get(param_manager->param_manager_list, n, param) {
            if ((SsStricmp(keyname, param->param_parameter_name) == 0) &&
                (SsStricmp(section, param->param_section_name) == 0)) {
                found = TRUE;
                break;
            }
        }
        if (!found) {
            return FALSE;
        }
        su_inifile_deletekeyline(
                param_manager->param_manager_inifile,
                param->param_section_name,
                param->param_parameter_name
        );
        su_list_remove(param_manager->param_manager_list, n);
        param_manager->param_manager_save = TRUE;
        return TRUE;

#else
        return FALSE;
#endif
}

/*#***********************************************************************\
 *
 *      su_param_initfactory_to_string
 *
 * Converts the factory value in an initstruct to a string.
 *
 * Parameters:
 *      param - in
 *          param of interest
 *
 *      buf - use, out
 *          if non-NULL, string will be put into this variable.
 *
 * Return value - give:
 *      if buf == NULL, returns a pointer to the new value string
 *      otherwise returns NULL.
 *
 * Limitations:
 *
 * Globals used:
 */
static char* su_param_initfactory_to_string(su_initparam_t* param, char* buf)
{
        char local_buf[128];
        char* factory_default_str;

        if (buf == NULL) {
            factory_default_str = local_buf;
        } else {
            factory_default_str = buf;
        }
        
        switch (param->p_paramtype) {
            
            case SU_PARAM_TYPE_NA:
            case SU_PARAM_TYPE_STR:
            case SU_PARAM_TYPE_LISTEN:
            case SU_PARAM_TYPE_INT8:
                ss_dassert(param->p_defaultstr != NULL);
                SsSprintf(factory_default_str, "%s",
                            param->p_defaultstr);
                break;
                
            case SU_PARAM_TYPE_LONG:
                SsSprintf(factory_default_str, "%ld",
                          param->p_defaultlong);
                break;
                
            case SU_PARAM_TYPE_DOUBLE:
                SsSprintf(factory_default_str, "%f",
                          param->p_defaultdouble);
                break;
                
            case SU_PARAM_TYPE_BOOL:
                SsSprintf(factory_default_str, "%s",
                          (param->p_defaultbool) ? "Yes" : "No");
                break;
                
            default:
                ss_rc_error(param->p_paramtype);
                break;
        }

        return (buf == NULL) ? SsMemStrdup(factory_default_str): NULL;
}

/*##**********************************************************************\
 *
 *		su_param_register_array
 *
 * Registers array of parameter values.
 *
 * Parameters :
 *
 *	params - in, use
 *	   Array of parameters.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool su_param_register_array(
        su_initparam_t params[])
{
#ifdef SU_PARAM_SRV
        char factory_default_str[128];
        int i;
        bool b;

        i = 0;
        while (params[i].p_section != NULL) {
#if 1
            su_param_initfactory_to_string(&(params[i]), factory_default_str);
#else
            switch (params[i].p_paramtype) {

                case SU_PARAM_TYPE_NA:
                case SU_PARAM_TYPE_STR:
                case SU_PARAM_TYPE_LISTEN:
                case SU_PARAM_TYPE_INT8:
                    ss_dassert(params[i].p_defaultstr != NULL);
                    SsSprintf(factory_default_str, "%s",
                            params[i].p_defaultstr);
                    break;

                case SU_PARAM_TYPE_LONG:
                    SsSprintf(factory_default_str, "%ld",
                            params[i].p_defaultlong);
                    break;

                case SU_PARAM_TYPE_DOUBLE:
                    SsSprintf(factory_default_str, "%f",
                            params[i].p_defaultdouble);
                    break;

                case SU_PARAM_TYPE_BOOL:
                    SsSprintf(factory_default_str, "%s",
                        (params[i].p_defaultbool) ? "Yes" : "No");
                    break;

                default:
                    ss_rc_error(params[i].p_paramtype);
                    break;
            }
#endif
            
            b = su_param_register(
                    params[i].p_section,
                    params[i].p_keyname,
                    NULL,
                    NULL,
                    factory_default_str,
                    params[i].p_description,
                    params[i].p_setfun,
                    params[i].p_getfun,
                    params[i].p_paramtype,
                    params[i].p_accessmode);

            ss_dprintf_2(("Registering: %s.%s=%s\nDesc:%s\nType:%s, mode:%s\n",
                    params[i].p_section,
                    params[i].p_keyname,
                    factory_default_str,
                    params[i].p_description,
                    su_param_type_strings[params[i].p_paramtype],
                    su_param_access_mode_strings[
                            P_NOATTR(params[i].p_accessmode)
                        ]));

            ss_dassert(b);
            if (!b) {
                return FALSE;
            }
            i++;
        }
#endif /* SU_PARAM_SRV */
        return(TRUE);
}

/*##**********************************************************************\
 *
 *		su_param_register_array_ctx
 *
 * Registers array of parameter values using context.
 *
 * Parameters :
 *
 *	params - in, use
 *	   Array of parameters.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool su_param_register_array_ctx(
        su_initparam_ctx_t params[],
        void* ctx)
{
#ifdef SU_PARAM_SRV
        char factory_default_str[128];
        int i;
        bool b;

        i = 0;
        while (params[i].p_section != NULL) {
#if 1
            ss_dassert(sizeof(su_initparam_t) == sizeof(su_initparam_ctx_t));
            su_param_initfactory_to_string((su_initparam_t*)&(params[i]), factory_default_str);
#else
            switch (params[i].p_paramtype) {

                case SU_PARAM_TYPE_NA:
                case SU_PARAM_TYPE_STR:
                case SU_PARAM_TYPE_LISTEN:
                case SU_PARAM_TYPE_INT8:
                    ss_dassert(params[i].p_defaultstr != NULL);
                    SsSprintf(factory_default_str, "%s",
                            params[i].p_defaultstr);
                    break;

                case SU_PARAM_TYPE_LONG:
                    SsSprintf(factory_default_str, "%ld",
                            params[i].p_defaultlong);
                    break;

                case SU_PARAM_TYPE_DOUBLE:
                    SsSprintf(factory_default_str, "%f",
                            params[i].p_defaultdouble);
                    break;

                case SU_PARAM_TYPE_BOOL:
                    SsSprintf(factory_default_str, "%s",
                        (params[i].p_defaultbool) ? "Yes" : "No");
                    break;

                default:
                    ss_rc_error(params[i].p_paramtype);
                    break;
            }
#endif
            
            b = param_register_ex(
                    (char *)params[i].p_section,
                    (char *)params[i].p_keyname,
                    NULL,
                    NULL,
                    factory_default_str,
                    (char *)params[i].p_description,
                    NULL,
                    params[i].p_setfun,
                    params[i].p_getfun,
                    params[i].p_paramtype,
                    params[i].p_accessmode,
                    ctx);

            ss_dprintf_2(("Registering: %s.%s=%s\nDesc:%s\nType:%s, mode:%s\n",
                    params[i].p_section,
                    params[i].p_keyname,
                    factory_default_str,
                    params[i].p_description,
                    su_param_type_strings[params[i].p_paramtype],
                    su_param_access_mode_strings[
                            P_NOATTR(params[i].p_accessmode)
                        ]));

            ss_dassert(b);
            if (!b) {
                return FALSE;
            }
            i++;
        }
#endif /* SU_PARAM_SRV */
        return(TRUE);
}

#ifdef SU_PARAM_SRV_DEBUG
/*##**********************************************************************\
 *
 *		su_param_print
 *
 *
 *
 * Parameters :
 *
 *	param -
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
void su_param_print(
        su_param_t* param)
{
        ss_dassert(param != NULL);

        SsDbgPrintf("section : %s, parameter : %s, default value : %s, current value : %s, type : %s, access mode : %s\n",
                    param->param_section_name,
                    param->param_parameter_name,
                    param->param_default_value,
                    param->param_current_value,
                    su_param_type_strings[param->param_type],
                    /* su_param_get_type_string(param->param_type), */
                    su_param_access_mode_strings[
                            P_NOATTR(param->param_access_mode)]);
                    /* su_param_get_access_mode_string(param->param_access_mode)); */
}
#endif /* SU_PARAM_SRV_DEBUG */


/*##**********************************************************************\
 *
 *		su_param_manager_save
 *
 *
 *
 * Parameters : 	 - none
 *
 * Return value :
 *      1+, if ok.   (TRUE)
 *      0, on error. (FALSE)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int su_param_manager_save()
{
#ifdef SU_PARAM_SRV
        return su_param_manager_saveas_ex(NULL, TRUE);
#endif
        return 0;
}

/*##**********************************************************************\
 *
 *      su_param_manager_saveas_ex
 *
 * Save param manager's parameters into a given file.
 *
 * Parameters:
 *      fname - in
 *          file to save into, if NULL, then use default (solid.ini usually) 
 *
 *      update - in
 *          if FALSE, doesn't update manager's save flag so that when save is
 *          called next time, the values will be saved then too even if there
 *          are no changes.
 *
 * Return value:
 *      1+, if ok.   (TRUE)
 *         1, if saved
 *         2, if no need to save
 *      0, on error. (FALSE)
 *
 * Limitations:
 *
 * Globals used:
 */
int su_param_manager_saveas_ex(char* fname, bool update)
{
        int rv = 2;

#ifdef SU_PARAM_SRV
        bool needsave = FALSE;
        su_param_t* param;
        su_list_node_t* n;
        bool b;

        ss_dprintf_3(("su_param_manager_save\n"));
        SU_PARAM_SEM_ENTER;
        /* loop params */

        su_list_do_get(param_manager->param_manager_list, n, param) {
            if (!param->param_tmp) { /* don't save temps */
                if (param->param_delete) {
                    if (param->param_found_in_inifile) {
                        ss_dprintf_3(("deletekeyline:[%s].%s\n",
                                param->param_section_name,
                                param->param_parameter_name));
                        b = su_inifile_deletekeyline(
                                        param_manager->param_manager_inifile,
                                        param->param_section_name,
                                        param->param_parameter_name);
                        ss_dassert(b);
                        if (b) {
                            needsave = TRUE;
                        }
                    }
                    param->param_delete = FALSE;

                } else if (param->param_save) {
                    char* value = param->param_current_value;
                    if (value == NULL) {
                        value = (char *)"";
                    }
                    ss_dprintf_3(("savekeyline:[%s].%s = %.255s\n",
                                  param->param_section_name,
                                  param->param_parameter_name,
                                  value));
                    su_inifile_putstring(
                                    param_manager->param_manager_inifile,
                                    param->param_section_name,
                                    param->param_parameter_name,
                                    value);
                    needsave = TRUE;
                    param->param_save = FALSE;

#if 0
                    /* set default value to match that what is saved into the
                     * inifile */
                    if (param->param_default_value != NULL) {
                        SsMemFree(param->param_default_value);
                    }
                    param->param_default_value = SsMemStrdup(value);
#endif
                }
            }
        }

        if (needsave || param_manager->param_manager_save) {
            bool b;
            if (fname == NULL) {
                b = su_inifile_save(param_manager->param_manager_inifile);
            } else { /* filename was given */
                b = su_inifile_saveas(param_manager->param_manager_inifile,
                                      fname);
            }
            rv = (b) ? 1 : 0;
            
            if (update) {
                param_manager->param_manager_save = FALSE;
            } else {
                /* ensure save on next time */
                param_manager->param_manager_save = TRUE;
            }                
        }
        SU_PARAM_SEM_EXIT;

#endif /* SU_PARAM_SRV */

        return rv;
}

#ifdef SU_PARAM_SRV
/*##**********************************************************************\
 *
 *		su_param_manager_saveas
 *
 *
 *
 * Parameters :
 *
 *	ifname - in
 *      name of the file to save to.
 *
 *
 * Return value : out
 *      TRUE, if ok.
 *      FALSE, on error.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int su_param_manager_saveas(
        char* ifname)
{
#if 0
        return su_param_manager_saveas_ex(ifname, FALSE);
#else
        return su_inifile_saveas(param_manager->param_manager_inifile, ifname);
#endif
}
#endif /* SU_PARAM_SRV */


#ifdef SU_PARAM_SRV
/*##**********************************************************************\
 *
 *      su_param_set_values
 *
 * Set parameter's values and calls existing callback function.
 *
 * Parameters:
 *      section_name - in
 *          section of the parameter
 *
 *      parameter_name - in
 *          name of the parameter
 *
 *      value - in
 *          new value of the parameter
 *
 *      tmp_value - in
 *          if new value is temporary and not to be saved
 *
 *      reset_to_default - in
 *          if value should be reset to the one found in the inifile
 *
 *      reset_to_factory - in
 *          if value should be reset to the factory setting
 *
 *      err - out
 *          possible error
 *
 *      changed - out
 *          if value was changed
 *
 * Return value:
 *      SU_SUCCESS, if ok.
 *      On error:
 *          SU_ERR_READONLY, if parameter is read-only and can't be changed.
 *          SU_ERR_ILL_VALUE_TYPE, if new value's type is unacceptable.
 *          SU_ERR_INVALID_PARAMETER, if parameter was not found.
 *          SU_ERR_PARAM_VALUE_INVALID, if new value is invalid.
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t su_param_set_values(
        const char* section_name,
        const char* parameter_name,
        const char* value,
        bool tmp_value,
        bool reset_to_default,
        bool reset_to_factory,
        su_err_t** err,
        bool* changed)
{
        return su_param_set_values_ex(section_name, parameter_name, value,
                                      tmp_value, reset_to_default,
                                      reset_to_factory, err, changed, TRUE);
}

/*##**********************************************************************\
 *
 *      su_param_set_values_nocb
 *
 * Set parameter's values but do not call an existing callback.
 *
 * This should only be used if there are parameters that refer to the same
 * value and would otherwise cause a infinite loop.
 *
 * Parameters:
 *      section_name - in
 *          section of the parameter
 *
 *      parameter_name - in
 *          name of the parameter
 *
 *      value - in
 *          new value of the parameter
 *
 *      tmp_value - in
 *          if new value is temporary and not to be saved
 *
 *      reset_to_default - in
 *          if value should be reset to the one found in the inifile
 *
 *      reset_to_factory - in
 *          if value should be reset to the factory setting
 *
 *      err - out
 *          possible error
 *
 *      changed - out
 *          if value was changed
 *
 * Return value:
 *      SU_SUCCESS, if ok.
 *      On error:
 *          SU_ERR_READONLY, if parameter is read-only and can't be changed.
 *          SU_ERR_ILL_VALUE_TYPE, if new value's type is unacceptable.
 *          SU_ERR_INVALID_PARAMETER, if parameter was not found.
 *          SU_ERR_PARAM_VALUE_INVALID, if new value is invalid.
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t su_param_set_values_nocb(
        const char* section_name,
        const char* parameter_name,
        const char* value,
        bool tmp_value,
        bool reset_to_default,
        bool reset_to_factory,
        su_err_t** err,
        bool* changed)
{
        return su_param_set_values_ex(section_name, parameter_name, value,
                                      tmp_value, reset_to_default,
                                      reset_to_factory, err, changed, FALSE);
}

static su_ret_t su_param_set_values_ex(
        const char* section_name,
        const char* parameter_name,
        const char* value,
        bool tmp_value,
        bool reset_to_default,
        bool reset_to_factory,
        su_err_t** err,
        bool* changed,
        bool do_callback)
{
        su_param_t* param = NULL;
        su_ret_t rc = SU_SUCCESS;

        *changed = FALSE;

        ss_dassert(section_name   != NULL);
        ss_dassert(parameter_name != NULL);

        ss_dprintf_2(("su_param_set_values_ex: %.255s.%.255s to '%.255s', tmp=%.1s, reset-def=%.1s, reset-fac=%.1s, do-callback=%.1s\n", section_name,
                      parameter_name,
                      (value != NULL) ? value: "<null>",
                      (tmp_value) ? "y": "n",
                      (reset_to_default) ? "y": "n",
                      (reset_to_factory) ? "y": "n",
                      (do_callback) ? "y": "n"));

        param = param_find((char *)section_name, (char *)parameter_name);
        if (param != NULL) {
            if (P_NOATTR(param->param_access_mode) == SU_PARAM_AM_RONLY) {
                su_err_init(err, SU_ERR_READONLY);
                return SU_ERR_READONLY;
            }
            if (reset_to_factory) {
                ss_dassert(!reset_to_default);
                rc = param_reset_to_factory(param, tmp_value, changed, err);
            } else if (reset_to_default){
                ss_dassert(!reset_to_factory);
                rc = param_reset_to_default(param, tmp_value, changed, err);
            } else {
                if (su_param_check_value_type(param->param_type, (char *)value)) {
                    rc = param_setvalue(param, (char *)value, tmp_value, changed, err,
                                        do_callback);
                } else {
                    su_err_init(err, SU_ERR_ILL_VALUE_TYPE);
                    rc = SU_ERR_ILL_VALUE_TYPE;
                }
            }
        } else {
            /* parameter not found */
            su_err_init(err, SU_ERR_INVALID_PARAMETER,
                        section_name,
                        parameter_name);
            rc = SU_ERR_INVALID_PARAMETER;
        }

        ss_debug(if (param != NULL) {
            ss_dassert(param->param_factory_value != NULL);
            ss_dprintf_2(("su_param_set_values_ex: %.255s.%.255s is now '%.255s', '%.255s', '%.255s' (cur/def/fac)\n",
                          param->param_section_name,
                          param->param_parameter_name,
                          (param->param_current_value) ?
                          param->param_current_value: "<null>",
                          (param->param_default_value) ?
                          param->param_default_value: "<null>",
                          param->param_factory_value)); /* factory can't be NULL */            
        });

#if 0
        /* check that return value is what we really accept */
        ss_dassert( (   rc == SU_SUCCESS)
                    || (rc == SU_ERR_READONLY)
                    || (rc == SU_ERR_ILL_VALUE_TYPE)
                    || (rc == SU_ERR_INVALID_PARAMETER)
                    || (rc == SU_ERR_PARAM_VALUE_INVALID) );
#endif
        
        return(rc);
}
#else
su_ret_t su_param_set_values(
        const char* section_name,
        const char* parameter_name,
        const char* value,
        bool tmp_value,
        bool reset_to_default,
        bool reset_to_factory,
        su_err_t** err,
        bool* changed)
{
        return SU_ERR_INVALID_PARAMETER;
}
#endif /* SU_PARAM_SRV */

/*##**********************************************************************\
 *
 *      su_param_getparam
 *
 * Return a parameter object.
 *
 * Parameters:
 *      section - in, use
 *          section of the parameter
 *
 *      param_name - in, use
 *          name of the parameter
 *
 * Return value - ref :
 *      NULL, if failed
 *      else a pointer to the parameter object.
 *
 * Limitations:
 *
 * Globals used:
 */
su_param_t* su_param_getparam(const char* section, const char* param_name)
{
#ifdef SU_PARAM_SRV
        su_param_t* param;

        ss_dassert(section != NULL);
        ss_dassert(param_name != NULL);

        param = param_find((char *)section, (char *)param_name);

        return param;
#else
        return NULL;
#endif /* SU_PARAM_SRV */
}

#ifdef SU_PARAM_SRV

/*##**********************************************************************\
 *
 *		su_param_fill_paramlist
 *
 *
 *
 * Parameters :
 *
 *	sectionmask -
 *
 *
 *	namemask -
 *
 *
 *	list -
 *
 *
 *	err -
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
bool su_param_fill_paramlist(
        const char* sectionmask,
        const char* namemask,
        su_list_t* list,
        su_err_t** err)
{
        return su_param_fill_paramlist_ex(sectionmask, namemask, list, err, 0);
}

/*#**********************************************************************\
 *
 *		su_param_fill_paramlist_ex
 *
 *
 *
 * Parameters :
 *
 *	sectionmask -
 *
 *
 *	namemask -
 *
 *
 *	list -
 *
 *
 *	err -
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
bool su_param_fill_paramlist_ex(
        const char* sectionmask,
        const char* namemask,
        su_list_t* list,
        su_err_t** err,
        su_param_access_mode_t attribs)
{
        su_param_t* param = NULL;
        su_list_node_t* n;
        int count = 0;
        su_param_access_mode_t attrmask = 0;
        
        ss_dassert(list != NULL);
        ss_dassert(P_NOATTR(attribs) == 0);

        attribs = attribs | SU_PARAM_AM_UNPUBLISHED;

        /* unpub is set on so that it will always be off in attrmask */
        
        /* so this might not be entirely clear..
         * WANTED| MASK        As we don't want those without a wanted 
         *  S I  |  S I        flag, we AND the mask with attribs and
         *-------+--------     return those that result in 0.
         *  0 0  |  1 1
         *  0 1  |  1 0        As MASK is 1s-complement of WANTED, we toggle
         *  1 0  |  0 1        with XOR.  Extra bits being turned to 1 is not
         *  1 1  |  0 0        a problem as they are masked out later.
         *
         **/
        attrmask = (attribs ^ ~(su_param_access_mode_t)0);            

        ss_dprintf_3(("su_param_fill_paramlist_ex: attribs = %02x, attrmask = %02x\n", attribs, attrmask));
        
        su_list_do_get(param_manager->param_manager_list, n, param) {
            bool accept = TRUE;

            ss_dprintf_3(("Try:[%s].%s\n",
                        param->param_section_name,
                        param->param_parameter_name));

            if (sectionmask != NULL) {
                accept = (SsStricmp(sectionmask, param->param_section_name) == 0);
                if (accept && namemask != NULL) {
#if 0
                    char* tmpname;
                    char* tmpmask;

                    tmpname = SsMemStrdup(param->param_parameter_name);
                    tmpmask = SsMemStrdup(namemask);

                    SsStrupr(tmpname);
                    SsStrupr(tmpmask);

                    accept = (strstr(tmpname, tmpmask) != NULL);

                    SsMemFree(tmpmask);
                    SsMemFree(tmpname);
#else
                    accept = (SsStricmp(namemask,
                                        param->param_parameter_name) == 0);
#endif
                }
            }
            accept = accept && ( (P_ATTR(param->param_access_mode)
                                 & attrmask) == 0 );
            if (accept) {
                ss_dprintf_3(("Accept: %s.%s, access mode value = %02x\n",
                              param->param_section_name,
                              param->param_parameter_name,
                              (uint) param->param_access_mode));
                su_list_insertlast(list, param);
                count++;
            }
        }

        if (count == 0) {
            su_err_init(err, SU_ERR_INVALID_PARAMETER,
                        sectionmask ? sectionmask : "",
                        namemask ? namemask : "");
            return(FALSE);
        }
        return(TRUE);
}
#endif /* SU_PARAM_SRV */


#ifdef SU_PARAM_SRV
/*#***********************************************************************\
 *
 *		param_getfactory_forprint
 *
 *
 *
 * Parameters :
 *
 *	param -
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
static char* param_getfactory_forprint(
        su_param_t* param)
{
        if (param->param_factory_value != NULL) {
            return(param->param_factory_value);
        }
        return((char *)"");
}

/*#***********************************************************************\
 *
 *		param_getdefault_forprint
 *
 *
 *
 * Parameters :
 *
 *	param -
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
static char* param_getdefault_forprint(
        su_param_t* param)
{
        if (param->param_default_value != NULL) {
            return(param->param_default_value);
        }
        return(param_getfactory_forprint(param));
}

/*#***********************************************************************\
 *
 *		param_getcurrent_forprint
 *
 *
 *
 * Parameters :
 *
 *	param -
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
static char* param_getcurrent_forprint(
        su_param_t* param)
{
        if (param->param_get_cb != NULL) {
            return ((param->param_get_cb)());
        }

        if (param->param_current_value != NULL) {
            return(param->param_current_value);
        }
        return(param_getdefault_forprint(param));
}
#endif /* SU_PARAM_SRV */


#ifdef SU_PARAM_SRV
/*#***********************************************************************\
 *
 *		param_get_valuestring
 *
 *
 *
 * Parameters :
 *
 *	section_name -
 *
 *
 *	parameter_name -
 *
 *
 *	default_value -
 *
 *
 *	current_value -
 *
 *
 *	factory_value -
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
#ifndef SS_MYSQL
static bool param_get_valuestring(
        char* section_name,
        char* parameter_name,
        char** default_value,
        char** current_value,
        char** factory_value)
{
        su_param_t* param;
        bool foundp = FALSE;
        ss_dassert(section_name);
        ss_dassert(parameter_name);

        param = param_find(section_name, parameter_name);
        if (param == NULL) {
            return(FALSE);
        }

        if (current_value != NULL) {
            *current_value = param_getcurrent_forprint(param);
            foundp = *current_value != NULL;
        }
        if (default_value != NULL) {
            *default_value = param->param_default_value;
            foundp = foundp || *default_value != NULL;
        }
        if (factory_value != NULL) {
            *factory_value = param->param_factory_value;
            foundp = foundp || *factory_value != NULL;
        }
        return(foundp);

}
#endif /* !SS_MYSQL */

#endif /* SU_PARAM_SRV */

/*##**********************************************************************\
 *
 *		su_param_getvalue
 *
 * Retrieves the value AND removes quotes from around it! If you don't want
 * this, use su_param_getvalue_ex().
 *
 * Parameters :
 *
 *	inifile -
 *
 *
 *	section -
 *
 *
 *	keyname -
 *
 *
 *	value_give -
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
bool su_param_getvalue(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_give)
{
        bool found;
        char* s;

        found = su_param_getvalue_ex(inifile, section, keyname, value_give);
        if (found) {        
            s = SsStrUnquote(*value_give);
            if (s != *value_give) { /* was changed */
                char* tmp = *value_give;
                *value_give = SsMemStrdup(s);
                SsMemFree(tmp);
            }        
        }
        
        return found; 
}

/*#**********************************************************************\
 *
 *		su_param_getvalue_ex
 *
 * Retrieves the value as is.
 *
 * Parameters :
 *
 *	inifile -
 *
 *
 *	section -
 *
 *
 *	keyname -
 *
 *
 *	value_give -
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
static bool su_param_getvalue_ex(
        su_inifile_t* inifile __attribute__ ((unused)),
        const char* section,
        const char* keyname,
        char** value_give)
{
        char* value = NULL;
#ifdef SU_PARAM_SRV
        su_param_t* param;

        ss_dassert(section);
        ss_dassert(keyname);
        ss_dassert(value_give != NULL);

        param = param_find((char *)section, (char *)keyname);
        if (param == NULL) {
            return(FALSE);
        }

        if (P_NOATTR(param->param_access_mode) == SU_PARAM_AM_RW
         || P_NOATTR(param->param_access_mode) == SU_PARAM_AM_RONLY) {
            value = param->param_current_value;
        }

        if (value == NULL) {
            value = param->param_default_value;
        }

        if (value != NULL) {
            value = SsMemStrdup(SsStrTrim(value));
        }

        *value_give = value;

        return(value != NULL);
#endif
#ifdef SU_PARAM_CLI
        bool foundp;
        
        foundp = su_inifile_getvalue(inifile, section, keyname, &value);
        if (foundp) {
            char* tmp = value;
            value = SsMemStrdup(SsStrTrim(value));
            SsMemFree(tmp);
            
            *value_give = value;
        }
        return(foundp);
#endif
}


/*##**********************************************************************\
 *
 *		su_param_getlong
 *
 *
 *
 * Parameters :
 *
 *	inifile -
 *
 *
 *	section -
 *
 *
 *	keyname -
 *
 *
 *	p_value -
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
bool su_param_getlong(
        su_inifile_t* inifile __attribute__ ((unused)),
        const char* section,
        const char* keyname,
        long* p_value)
{
#ifdef SU_PARAM_SRV
        su_param_t* param;
        char* value = NULL;

        ss_dassert(section);
        ss_dassert(keyname);
        ss_dassert(p_value != NULL);

        param = param_find((char *)section, (char *)keyname);
        if (param == NULL) {
            return(FALSE);
        }

        if (P_NOATTR(param->param_access_mode) == SU_PARAM_AM_RW) {
            value = param->param_current_value;
        }

        if (value == NULL) {
            value = param->param_default_value;
        }

        if (value != NULL) {
            su_param_str_to_long(value, p_value);
        }
        return(value != NULL);

#endif
#ifdef SU_PARAM_CLI
        return(su_inifile_getlong(inifile, section, keyname, p_value));
#endif
}

/*##**********************************************************************\
 *
 *		su_param_getint8
 *
 *
 *
 * Parameters :
 *
 *	inifile -
 *
 *
 *	section -
 *
 *
 *	keyname -
 *
 *
 *	p_value -
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
bool su_param_getint8(
        su_inifile_t* inifile __attribute__ ((unused)),
        const char* section,
        const char* keyname,
        ss_int8_t* p_value)
{
#ifdef SU_PARAM_SRV
        su_param_t* param;
        char* value = NULL;

        ss_dassert(section);
        ss_dassert(keyname);
        ss_dassert(p_value != NULL);

        param = param_find((char *)section, (char *)keyname);
        if (param == NULL) {
            return(FALSE);
        }

        if (P_NOATTR(param->param_access_mode) == SU_PARAM_AM_RW) {
            value = param->param_current_value;
        }

        if (value == NULL) {
            value = param->param_default_value;
        }

        if (value != NULL) {
            su_param_str_to_int8(value, p_value);
        }
        return(value != NULL);

#endif
#ifdef SU_PARAM_CLI
        return(su_inifile_getint8(inifile, section, keyname, p_value));
#endif
}

/*##**********************************************************************\
 *
 *		su_param_getdouble
 *
 *
 *
 * Parameters :
 *
 *	inifile -
 *
 *
 *	section -
 *
 *
 *	keyname -
 *
 *
 *	p_value -
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
bool su_param_getdouble(
        su_inifile_t* inifile __attribute__ ((unused)),
        const char* section,
        const char* keyname,
        double* p_value)
{
#ifdef SU_PARAM_SRV
        su_param_t* param;
        char* value = NULL;

        ss_dassert(section);
        ss_dassert(keyname);
        ss_dassert(p_value != NULL);

        param = param_find((char *)section, (char *)keyname);
        if (param == NULL) {
            return(FALSE);
        }

        if (P_NOATTR(param->param_access_mode) == SU_PARAM_AM_RW) {
            value = param->param_current_value;
        }

        if (value == NULL) {
            value = param->param_default_value;
        }

        if (value != NULL) {
            su_param_str_to_double(value, p_value);
        }
        return(value != NULL);

#endif
#ifdef SU_PARAM_CLI
        return(su_inifile_getdouble(inifile, section, keyname, p_value));
#endif
}

/*##**********************************************************************\
 *
 *		su_param_getbool
 *
 *
 *
 * Parameters :
 *
 *	inifile -
 *
 *
 *	section -
 *
 *
 *	keyname -
 *
 *
 *	p_value -
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
bool su_param_getbool(
        su_inifile_t* inifile __attribute__ ((unused)),
        const char* section,
        const char* keyname,
        bool* p_value)
{
#ifdef SU_PARAM_SRV
        su_param_t* param;
        char* value = NULL;

        ss_dassert(section);
        ss_dassert(keyname);
        ss_dassert(p_value != NULL);

        param = param_find((char *)section, (char *)keyname);

        if (param == NULL) {
            return(FALSE);
        }

        if (P_NOATTR(param->param_access_mode) == SU_PARAM_AM_RW) {
            value = param->param_current_value;
        }

        if (value == NULL) {
            value = param->param_default_value;
        }

        if (value != NULL) {
            su_param_str_to_bool(value, p_value);
        }
        return(value != NULL);

#endif /* SU_PARAM_SRV */
#ifdef SU_PARAM_CLI
        return(su_inifile_getbool(inifile, section, keyname, p_value));
#endif
}


#ifdef SU_PARAM_SRV
/*##**********************************************************************\
 *
 *		su_param_get_values_forprint
 *
 *
 *
 * Parameters :
 *
 *	param -
 *
 *
 *	section_name -
 *
 *
 *	parameter_name -
 *
 *
 *	default_value -
 *
 *
 *	current_value -
 *
 *
 *	factory_value -
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
void su_param_get_values_forprint(
        su_param_t* param,
        char** section_name,
        char** parameter_name,
        char** default_value,
        char** current_value,
        char** factory_value)
{
        ss_dassert(param != NULL);

        *section_name = param->param_section_name;
        *parameter_name = param->param_parameter_name;
        *current_value = param_getcurrent_forprint(param);
        *default_value = param_getdefault_forprint(param);
        *factory_value = param_getfactory_forprint(param);
}
#endif /* SU_PARAM_SRV */

#ifdef SU_PARAM_SRV
/*##**********************************************************************\
 *
 *      su_param_get_paraminfo_str
 *
 * Returns info about a parameter in a string, imitating AC 'parameter' output
 * format.
 *
 * Parameters:
 *      param - in
 *          parameter of interest
 *
 *      info - out, give
 *          info string will be returned in this.
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void su_param_get_paraminfo_str(su_param_t* param, char** info)
{
        char buf[1024];
        
        ss_dassert(info != NULL);

        SsSprintf(buf, "%s %s %s %s %s ; AM: %s, F: %s%s%s",
                  param->param_section_name,
                  param->param_parameter_name,
                  param_getcurrent_forprint(param),
                  param_getdefault_forprint(param),
                  param_getfactory_forprint(param),
                  su_param_access_mode_strings[
                          P_NOATTR(param->param_access_mode)
                      ],
                  (param->param_access_mode & SU_PARAM_AM_SYSTEM)
                  ? "S" : "",
                  (param->param_access_mode & SU_PARAM_AM_INVISIBLE)
                  ? "I" : "",
                  (param->param_access_mode & SU_PARAM_AM_UNPUBLISHED)
                  ? "U" : "");

        *info = SsMemStrdup(buf);
}

/*##**********************************************************************\
 *
 *		su_param_get_description
 *
 *
 *
 * Parameters :
 *
 *	param -
 *
 *
 *	section_name -
 *
 *
 *	parameter_name -
 *
 *
 *	description_text -
 *
 *
 *	access_mode -
 *
 *
 *	type -
 *
 *
 *	default_value -
 *
 *
 *	factory_value -
 *
 *
 *	current_value -
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
void su_param_get_description(
        su_param_t* param,
        char** section_name,
        char** parameter_name,
        char** description_text,
        char** access_mode,
        char** type,
        char** default_value,
        char** factory_value,
        char** current_value)
{
        ss_dassert(param != NULL);
        ss_dassert(param->param_section_name);
        ss_dassert(param->param_parameter_name);

        ss_dassert(section_name    != NULL);
        ss_dassert(parameter_name  != NULL);
        ss_dassert(description_text!= NULL);
        ss_dassert(access_mode     != NULL);
        ss_dassert(type            != NULL);
        ss_dassert(default_value   != NULL);
        ss_dassert(factory_value   != NULL);
        ss_dassert(current_value   != NULL);

        *section_name   = param->param_section_name;
        *parameter_name = param->param_parameter_name;
        *default_value  = param->param_default_value;
        *description_text = param->param_description_text;
        *factory_value  = param->param_factory_value;
        *current_value  = param->param_current_value;
        *access_mode    = (char *)su_param_access_mode_strings[
                P_NOATTR(param->param_access_mode)];
        *type           = (char *)su_param_type_strings[param->param_type];
}
#endif /* SU_PARAM_SRV */


/*##**********************************************************************\
 *
 *		su_param_scanlong
 *
 * Scans a long int value from keyline=value1, value2, ... line
 *
 * Parameters :
 *
 *	inifile - in, use
 *		pointer to inifile object
 *
 *	section - in, use
 *		section name
 *
 *	keyname - in, use
 *		key name
 *
 *	separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *
 *	scanindex - in out, use
 *		pointer to index in the value string. For the first value
 *          it should be initialized to 0.
 *
 *	l - out
 *		pointer to variable where the scanned long int value is stored
 *
 * Return value :
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool su_param_scanlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        long* l)
{
#ifdef SU_PARAM_SRV
        char* valuestr;
        char* value;
        bool b;

        b = su_param_getvalue_ex(inifile, section, keyname, &valuestr);
        if (!b) {
            return FALSE;
        }
        b = SsStrScanStringWQuoting(valuestr, (char *)separators, scanindex, ';',
                                    &value);
        if (b) {
            b = su_param_str_to_long(value, l);
            SsMemFree(value);
            SsMemFree(valuestr);
            if (b) {
                return TRUE;
            } else {
                return FALSE;
            }
        } else {
            SsMemFree(valuestr);
            return FALSE;
        }
#endif
#ifdef SU_PARAM_CLI
        return(su_inifile_scanlong(inifile, section, keyname, separators, scanindex, l));
#endif
}
/*##**********************************************************************\
 *
 *		su_param_scanint8
 *
 * Scans a int8 value from keyline=value1, value2, ... line
 *
 * Parameters :
 *
 *	inifile - in, use
 *		pointer to inifile object
 *
 *	section - in, use
 *		section name
 *
 *	keyname - in, use
 *		key name
 *
 *	separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *
 *	scanindex - in out, use
 *		pointer to index in the value string. For the first value
 *          it should be initialized to 0.
 *
 *	i8 - out
 *		pointer to variable where the scanned int8 value is stored
 *
 * Return value :
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal integer or
 *      FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool su_param_scanint8(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        ss_int8_t* i8)
{
#ifdef SU_PARAM_SRV
        char* valuestr;
        char* value;
        bool b;

        b = su_param_getvalue_ex(inifile, section, keyname, &valuestr);
        if (!b) {
            return FALSE;
        }
        b = SsStrScanStringWQuoting(valuestr, (char *)separators, scanindex, ';',
                                    &value);
        if (b) {
            b = su_param_str_to_int8(value, i8);
            SsMemFree(value);
            SsMemFree(valuestr);
            if (b) {
                return TRUE;
            } else {
                return FALSE;
            }
        } else {
            SsMemFree(valuestr);
            return FALSE;
        }
#endif
#ifdef SU_PARAM_CLI
        return(su_inifile_scanint8(inifile, section, keyname, separators, scanindex, i8));
#endif
}

/*##**********************************************************************\
 *
 *		su_param_scandouble
 *
 * Scans a double value from a keyname=value1, value2, ... line
 *
 * Parameters :
 *
 *	inifile - in, use
 *		pointer to inifile object
 *
 *	section - in, use
 *		section name
 *
 *	keyname - in, use
 *		key name
 *
 *	separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *
 *	scanindex - in out, use
 *		pointer to index to value string. For the first value it
 *          should be initialized to 0
 *
 *	d - out
 *		pointer to variable where the value is stored
 *
 * Return value :
 *      TRUE if the keyname was found under the specified section and
 *      the value was a legal double or
 *      FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool su_param_scandouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        double* d)
{
#ifdef SU_PARAM_SRV
        char* valuestr;
        char* value;
        bool b;

        b = su_param_getvalue_ex(inifile, section, keyname, &valuestr);
        if (!b) {
            return FALSE;
        }
        b = SsStrScanStringWQuoting(valuestr, (char *)separators, scanindex, ';',
                                    &value);
        if (b) {
            b = su_param_str_to_double(value, d);
            SsMemFree(value);
            SsMemFree(valuestr);
            if (b) {
                return TRUE;
            } else {
                return FALSE;
            }
        } else {
            SsMemFree(valuestr);
            return FALSE;
        }
#endif
#ifdef SU_PARAM_CLI
        return(su_inifile_scandouble(inifile, section, keyname, separators, scanindex, d));
#endif

}

/*##**********************************************************************\
 *
 *		su_param_scanstring
 *
 * Scans a string value from a keyname=value1, value2, ... line
 *
 * Parameters :
 *
 *	inifile - in, use
 *		pointer to inifile object
 *
 *	section - in, use
 *		section name
 *
 *	keyname - in, use
 *		key name
 *
 *	separators - in, use
 *          string containing legal separator characters, typically " \t,"
 *
 *
 *	scanindex - in out, use
 *		pointer to index to value string. For the first value it
 *          should be initialized to 0
 *
 *	str - out, give
 *		pointer to char* where a copy of the scanned string is stored
 *
 * Return value :
 *      TRUE when a valid value was found or
 *      FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
bool su_param_scanstring(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        char** str)
{

#ifdef SU_PARAM_SRV

        char* valuestr;
        bool b;
        
        b = su_param_getvalue_ex(inifile, section, keyname, &valuestr);
        if (!b) {
            *str = NULL;
            return FALSE;
        }

        b = SsStrScanStringWQuoting(valuestr, (char *)separators, scanindex, ';', str);
        SsMemFree(valuestr);

        return b;

#endif /* SU_PARAM_SRV */
#ifdef SU_PARAM_CLI
        return(su_inifile_scanstring(inifile, section, keyname, separators, scanindex, str));
#endif

}

#ifdef SU_PARAM_SRV
/*##**********************************************************************\
 *
 *      su_param_getsectionname
 *
 * Get parameter's section name.
 *
 * Parameters:
 *      param - in
 *          parameter of interest.
 *
 * Return value - ref:
 *      pointer to the section name.
 *
 * Limitations:
 *
 * Globals used:
 */
char* su_param_getsectionname(su_param_t* param)
{
        ss_dassert(param != NULL);

        return param->param_section_name;
}

/*##**********************************************************************\
 *
 *      su_param_getparamname
 *
 * Get parameter's parameter name (ie. "keyname").
 *
 * Parameters:
 *      param - in
 *          parameter of interest.
 *
 * Return value - ref:
 *      pointer to the parameter's name.
 *
 * Limitations:
 *
 * Globals used:
 */
char* su_param_getparamname(su_param_t* param)
{
        ss_dassert(param != NULL);

        return param->param_parameter_name;
}

#endif /* SU_PARAM_SRV */

void su_param_setvisibility(su_param_t* param, bool vis)
{
#ifdef SU_PARAM_SRV
        ss_dassert(param != NULL);
        ss_dprintf_4(("su_param_setvisibility: %s.%s to %s\n",
                      param->param_section_name, param->param_parameter_name,
                      (vis) ? "TRUE" : "FALSE"));
        if (vis) {
            param->param_access_mode &= ~SU_PARAM_AM_INVISIBLE;
        } else {
            param->param_access_mode |= SU_PARAM_AM_INVISIBLE;
        }
#endif /* SU_PARAM_SRV */        
}

#undef P_ATTR
#undef P_NOATTR

/* EOF */

