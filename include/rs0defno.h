/*************************************************************************\
**  source       * rs0defno.h
**  directory    * res
**  description  * Default node definition
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


#ifndef RS0DEFNO_H
#define RS0DEFNO_H

typedef enum {
        RS_DEFNODE_MASTER = 1,
        RS_DEFNODE_REPLICA
} rs_defnodetype_t;

rs_defnode_t* rs_defnode_init(
        rs_defnodetype_t    type,
        char*               name,
        char*               connectstr);

void rs_defnode_done(
        rs_defnode_t* defnode);

rs_defnodetype_t rs_defnode_type(
        rs_defnode_t* defnode);

char* rs_defnode_name(
        rs_defnode_t* defnode);

char* rs_defnode_connectstr(
        rs_defnode_t* defnode);

#endif /* RS0DEFNO_H */
