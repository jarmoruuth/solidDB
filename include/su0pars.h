/*************************************************************************\
**  source       * su0pars.h
**  directory    * su
**  description  * Simple string parsing utilities.
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


#ifndef SU0PARS_H
#define SU0PARS_H

typedef struct {
        char* m_start;
        char* m_pos;
} su_pars_match_t;

void su_pars_match_init(
        su_pars_match_t* m,
        char* s);

void su_pars_check_comment(
        su_pars_match_t* m);

bool su_pars_match_const(
        su_pars_match_t* m,
        char* s);

bool su_pars_match_keyword(
        su_pars_match_t* m,
        char* keyword);

bool su_pars_get_pwd(
        su_pars_match_t* m,
        char* pwd_buf,
        uint pwd_size);

bool su_pars_get_id(
        su_pars_match_t* m,
        char* id_buf,
        uint id_size);

bool su_pars_get_tablename(
        su_pars_match_t* m,
        char* authid_buf,
        uint authid_size,
        char* tabname_buf,
        uint tabname_size);

bool su_pars_get_filename(
        su_pars_match_t* m,
        char* fname_buf,
        uint fname_size);

bool su_pars_get_stringliteral(
        su_pars_match_t* m,
        char* string_buf,
        uint string_size);

bool su_pars_get_stringliteral_withquotes(
        su_pars_match_t* m,
        char* string_buf,
        uint string_size);

bool su_pars_get_uint(
        su_pars_match_t* m,
        uint* p_num);

bool su_pars_get_long(
        su_pars_match_t* m,
        long* p_num);

bool su_pars_get_special(
        su_pars_match_t* m,
        char* buf,
        uint buf_size);

bool su_pars_get_numeric(
        su_pars_match_t* m,
        char* buf,
        uint buf_size);

bool su_pars_give_objname(
        su_pars_match_t* m,
        char** p_authid,
        char** p_name);

bool su_pars_give_objname3(
        su_pars_match_t* m,
        char** p_catalog,
        char** p_schema,
        char** p_name);

bool su_pars_give_hint(
        su_pars_match_t* m, 
        char** p_hint);

bool su_pars_skipto_keyword(
        su_pars_match_t* m,
        const char* const_str,
        const char* end_str);

#endif /* SU0PARS_H */
