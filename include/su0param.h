/*************************************************************************\
**  source       * su0param.h
**  directory    * su
**  description  * Structures for parameter objects.
**               * 
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


#ifndef SU0PARAM_H
#define SU0PARAM_H

#include <ssc.h>
#include <ssdebug.h>
#include <su0list.h>
#include <su0err.h>
#include <ssint8.h>

#include "su0inifi.h"

       
typedef struct su_param_st su_param_t;

typedef su_ret_t (*su_param_set_cb_t)(
        char* default_value,
        char** default_value_loc,
        char* current_value,
        char** current_value_loc,
        char** factory_value_loc);

typedef su_ret_t (*su_param_set_cb_ctx_t)(
        void* ctx,
        char* section,
        char* name,
        char* default_value,
        char** default_value_loc,
        char* current_value,
        char** current_value_loc,
        char** factory_value_loc);

typedef char* (*su_param_get_cb_t)(void);

/* access modes */
/* mr: 20040504: hack: changed to bitfield defines to allow hiding of
 *               parameters so that they are invisible but still usable. */
/* real modes are sequential and restricted to first byte */
#define SU_PARAM_AM_NA          0x00
#define SU_PARAM_AM_RW          0x01
#define SU_PARAM_AM_RWSTARTUP   0x02
#define SU_PARAM_AM_RWCREATE    0x03
#define SU_PARAM_AM_RONLY       0x04
/* attribute flags for accessing */
#define SU_PARAM_AM_UNPUBLISHED 0x10 /* may be visible, depends on INVISIBLE */
#define SU_PARAM_AM_INVISIBLE   0x20 /* this is a toggle attribute */
#define SU_PARAM_AM_SYSTEM      0x40 /* never visible */

#define SU_PARAM_AM_NOATTRIBUTES_MASK 0x0f
#define SU_PARAM_AM_ATTRIBUTES_MASK   0x70  /* update this if you add
                                             * attributes.  Note that
                                             * su_param_fill_paramlist is very
                                             * hard coded in these */
#define SU_PARAM_AM_ALL    SU_PARAM_AM_ATTRIBUTES_MASK
#define SU_PARAM_AM_NORMAL 0x00

/* parameter types */
enum su_param_type_enum {
        SU_PARAM_TYPE_NA,
        SU_PARAM_TYPE_LONG,
        SU_PARAM_TYPE_DOUBLE,
        SU_PARAM_TYPE_STR,
        SU_PARAM_TYPE_BOOL,
        SU_PARAM_TYPE_LISTEN,
        SU_PARAM_TYPE_INT8
};

typedef unsigned int su_param_access_mode_t;
typedef enum su_param_type_enum su_param_type_t;

su_ret_t su_param_set_values(
        const char* section,
        const char* parameter,
        const char* value,
        bool tmp_value,
        bool reset_to_default,
        bool reset_to_factory,
        su_err_t** err,
        bool* changed);

su_ret_t su_param_set_values_nocb(
        const char* section_name,
        const char* parameter_name,
        const char* value,
        bool tmp_value,
        bool reset_to_default,
        bool reset_to_factory,
        su_err_t** err,
        bool* changed);

su_param_t* su_param_getparam(const char* section, const char* param_name);

bool su_param_fill_paramlist(
        const char* sectionmask,
        const char* namemask,
        su_list_t* pist,
        su_err_t** err);

bool su_param_fill_paramlist_ex(
        const char* sectionmask,
        const char* namemask,
        su_list_t* list,
        su_err_t** err,
        su_param_access_mode_t attribs);

bool su_param_getvalue(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_give);

bool su_param_getlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        long* value);

bool su_param_getint8(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        ss_int8_t* p_value);

bool su_param_getdouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        double* value);

bool su_param_getbool(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        bool* p_value);

void su_param_get_values_forprint(
        su_param_t* param, 
        char** section_name, 
        char** parameter_name, 
        char** default_value, 
        char** current_value, 
        char** factory_value);

bool su_param_scanlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        long* l);

bool su_param_scanint8(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        ss_int8_t* i8);

bool su_param_scandouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        double* d);

bool su_param_scanstring(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        char** str);



bool su_param_manager_isinitialized(void);
bool su_param_manager_global_init(su_inifile_t* inifile);
int  su_param_manager_global_done(void);
void su_param_manager_global_done_force(void);
bool su_param_manager_save(void);
bool su_param_manager_saveas(char* fname);
bool su_param_manager_saveas_ex(char* fname, bool update);
ss_debug(void su_param_manager_setreadonly(void);)

void su_param_manager_set_ifname(char* ifname);

/* register one parameter, ie. add one parameter to inifile-object */
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
        su_param_access_mode_t access_mode);

typedef struct {
        const char*   p_section;
        const char*   p_keyname;
        const char*   p_defaultstr; 
        long    p_defaultlong;
        double  p_defaultdouble;
        bool    p_defaultbool;
        su_param_set_cb_t   p_setfun;
        su_param_get_cb_t   p_getfun;
        su_param_type_t     p_paramtype;
        su_param_access_mode_t  p_accessmode;
        const char*   p_description;
} su_initparam_t;

typedef struct {
        const char*   p_section;
        const char*   p_keyname;
        const char*   p_defaultstr; 
        long    p_defaultlong;
        double  p_defaultdouble;
        bool    p_defaultbool;
        su_param_set_cb_ctx_t p_setfun;
        su_param_get_cb_t   p_getfun;
        su_param_type_t     p_paramtype;
        su_param_access_mode_t  p_accessmode;
        const char*   p_description;
} su_initparam_ctx_t;

bool su_param_isregistered(char* section, char* keyname);
void su_param_switchtoreadonly(char* section, char* keyname);
void su_param_switchtotemporary(char* section, char* keyname);
bool su_param_remove(char* section, char* keyname);

bool su_param_register_dynfactory(su_initparam_t arr[], char* section,
                                  char* keyname, char* str, long l,
                                  double d, bool b);

bool su_param_register_array(
        su_initparam_t params[]);

bool su_param_register_array_ctx(
        su_initparam_ctx_t params[],
        void* ctx);

void su_param_get_paraminfo_str(su_param_t* param, char** info);

void su_param_get_description(
        su_param_t* param,
        char** section_name,
        char** parameter_name,
        char** description_text,
        char** access_mode,
        char** type,
        char** default_value,
        char** factory_value,
        char** current_value);

bool su_param_str_to_bool(
        char* value_str,
        bool* value);

bool su_param_str_to_long(
        char* value_str,
        long* value);

bool su_param_str_to_int8(
        char* value_str,
        ss_int8_t* value);

bool su_param_str_to_double(
        char* value_str,
        double* value);

char* su_param_getsectionname(su_param_t* param);
char* su_param_getparamname(su_param_t* param);
su_param_type_t su_param_gettype(su_param_t* param);

void su_param_setvisibility(su_param_t* param, bool vis);

#endif /* SU0PARAM_H */

