/*************************************************************************\
**  source       * xs1cfg.h
**  directory    * xs
**  description  * Configuration object for eXternal Sorter
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


#ifndef XS1CFG_H
#define XS1CFG_H

#include <ssc.h>
#include <su0inifi.h>
#include <su0cfgl.h>

typedef struct xs_cfg_st xs_cfg_t;

xs_cfg_t* xs_cfg_init(
        su_inifile_t* inifile);

void xs_cfg_done(
        xs_cfg_t* cfg);

bool xs_cfg_poolpercenttotal(
        xs_cfg_t* cfg,
        uint* p_percent);

bool xs_cfg_poolsizeper1sort(
        xs_cfg_t* cfg,
        ulong* p_size);

bool xs_cfg_maxfilesper1sort(
        xs_cfg_t* cfg,
        uint* p_maxfiles);

bool xs_cfg_maxfilestotal(
        xs_cfg_t* cfg,
        ulong* p_maxfiles);

bool xs_cfg_blocksize(
        xs_cfg_t* cfg,
        size_t* p_blocksize);

bool xs_cfg_tmpdir(
        xs_cfg_t* cfg,
        uint dnum,
        char** p_dname_give,
        ulong* p_maxblocks);

bool xs_cfg_maxbytesperstep(
        xs_cfg_t* cfg,
        ulong* p_maxbytes);

bool xs_cfg_maxrowsperstep(
        xs_cfg_t* cfg,
        uint* p_maxrows);

bool xs_cfg_getwriteflushmode(
        xs_cfg_t* cfg,
        int* p_writeflushmode);

bool xs_cfg_getfilebuffering(
        xs_cfg_t* cfg,
        bool* p_filebuffering);

void xs_cfg_addtocfgl(
        xs_cfg_t* xs_cfg,
        su_cfgl_t* cfgl);

void xs_cfg_register_su_params(void);

#endif /* XS1CFG_H */
