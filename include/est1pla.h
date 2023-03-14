/*************************************************************************\
**  source       * tab1pla.h
**  directory    * est
**  description  * Header file for the search plan generation.
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

#ifndef TAB1PLA_H
#define TAB1PLA_H

#include <rs0pla.h>

/* The following function creates a search plan structure and
   generates the search plan. The table level has to call this
   first and then the rs-functions which extract information from
   the returned object.
*/
rs_pla_t* tb_pla_create_search_plan(
        rs_sysi_t* cd,
        rs_pla_t*   rspla,
        rs_relh_t* table,
        rs_key_t* key,
        su_list_t* constraint_list,
        su_pa_t* cons_byano,
        int* selection_list,
        bool addlinks);

bool tb_pla_form_range_constraint(
        rs_sysi_t*      cd,
        rs_ano_t        column_no,
        su_list_t*      constraint_list,
        su_pa_t*        cons_byano,
        dynvtpl_t*      range_start,
        bool*           range_start_closed,
        dynvtpl_t*      range_end,
        bool*           range_end_closed,
        bool*           p_pointlike,
        bool*           p_emptyrange);

void tb_pla_form_select_list_buf(
        rs_sysi_t* cd,
        rs_key_t* clkey,
        rs_key_t* key,
        rs_ano_t* select_list,
        rs_ano_t* key_select_list,
        bool must_dereference1,
        bool* p_must_dereference2);

rs_ano_t* tb_pla_form_select_list(
        rs_sysi_t* cd,
        rs_key_t* clkey,
        rs_key_t* key,
        rs_ano_t* select_list,
        bool must_dereference1,
        bool* p_must_dereference2);

/* Functions for testing */

void tb_pla_initialize_test(void);

rs_pla_t* tb_pla_get_plan(void);

#endif /* TAB1PLA_H */
