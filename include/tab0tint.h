/*************************************************************************\
**  source       * tab0tint.h
**  directory    * tab
**  description  * Table level funblock interface for relcur and relh
**               * functions.
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

#ifndef TAB0TINT_H
#define TAB0TINT_H


#include <rs0error.h>
#include <rs0atype.h>
#include <rs0aval.h>

#ifndef TB_TINT_T_DEFINED
#define TB_TINT_T_DEFINED

typedef struct tb_tint_st {
/*  tb_relcur_ functions */
        tb_relcur_t* (*cursor_create)( void*,
                                       tb_trans_t*,
                                       tb_relh_t*,
                                       uint,
                                       bool );        /* tb_relcur_create */
        void (*cursor_disableinfo)( void*,
                                    tb_relcur_t*);    /* tb_relcur_disableinfo */
        void (*cursor_free)( void*,
                             tb_relcur_t* );          /* tb_relcur_free */
        void (*cursor_orderby)(void*,
                               tb_relcur_t*,
                               uint,
                               bool );                /* tb_relcur_orderby */
        void (*cursor_constr)( void*,
                               tb_relcur_t*,
                               uint,
                               uint,
                               rs_atype_t*,
                               rs_aval_t*,
                               rs_atype_t*,
                               rs_aval_t* );          /* tb_relcur_constr */
        void (*cursor_project)( void*,
                                tb_relcur_t*,
                                int* );               /* tb_relcur_project */
        bool (*cursor_endofconstr)( void*,
                                    tb_relcur_t*,
                                    rs_err_t** );     /* tb_relcur_endofconstr */
        uint (*cursor_ordered)( void*,
                                tb_relcur_t*,
                                uint* );              /* tb_relcur_ordered */
        void (*cursor_open)( void*,
                             tb_relcur_t* );          /* tb_relcur_open */
        rs_ttype_t* (*cursor_ttype)( void*,
                                     tb_relcur_t* );  /* tb_relcur_ttype */
        rs_tval_t* (*cursor_next)( void*,
                                   tb_relcur_t*,
                                   uint*,
                                   rs_err_t** );      /* tb_relcur_next */
        rs_tval_t* (*cursor_prev)( void*,
                                   tb_relcur_t*,
                                   uint*,
                                   rs_err_t** );      /* tb_relcur_prev */
        uint (*cursor_update)( void*,
                               tb_relcur_t*,
                               rs_tval_t*,
                               bool*,
                               bool*,
                               uint,
                               uint*,
                               uint*,
                               rs_atype_t**,
                               rs_aval_t**,
                               rs_err_t** );          /* tb_relcur_update */
        uint (*cursor_delete)( void*,
                               tb_relcur_t*,
                               rs_err_t** );          /* tb_relcur_delete */
        bool (*cursor_begin)( void*,
                              tb_relcur_t* );         /* tb_relcur_begin */
        bool (*cursor_end)( void*,
                            tb_relcur_t* );           /* tb_relcur_end */
        void* (*cursor_copytref)( void*,
                                  tb_relcur_t* );     /* tb_relcur_copytref */
        uint (*cursor_saupdate)( void*,
                                 tb_relcur_t*,
                                 tb_trans_t*,
                                 rs_tval_t*,
                                 bool*,
                                 void*,
                                 bool,
                                 rs_err_t** );        /* tb_relcur_saupdate */
        uint (*cursor_sadelete)( void*,
                                 tb_relcur_t*,
                                 tb_trans_t*,
                                 void*,
                                 rs_err_t** );        /* tb_relcur_sadelete */
        bool (*cursor_setposition)( void*,
                                    tb_relcur_t*,
                                    rs_tval_t*,
                                    rs_err_t**);     /* tb_relcur_setposition */
/* tb_relh_ functions */    
        uint (*relhandle_sainsert)( void*,
                                    tb_trans_t*,
                                    tb_relh_t*,
                                    rs_ttype_t*,
                                    rs_tval_t*,
                                    bool*,
                                    bool,
                                    rs_err_t** );      /* tb_relh_sainsert */
} tb_tint_t;

#endif /* TB_TINT_T_DEFINED */

tb_tint_t* tb_tint_init(void);

void tb_tint_done(tb_tint_t* xinterface);

#endif /* TAB0TINT_H */

