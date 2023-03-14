/*************************************************************************\
**  source       * sqlest.h
**  directory    * res
**  description  * header for estimation aid functions
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


/* The function sql_tupleest gives an integer estimation of how many entries
   will be found from a given relation with a key value, when a sample of 100
   entries of the key value reveals a specified number of different values.

   Parameters:

    relsize                 approximation of tuples in the relation
    diff                    number of different values found in the
                            sample

   Return: estimation of how many tuples will be found for a given key
           value
*/
ulong sql_tupleest(
    ulong relsize,
    uint diff
);

/* The function sql_tupleestd gives a floating-point estimation of how many
   entries will be found from a given relation with a key value, when
   a sample of 100 entries of the key value reveals a specified number of
   different values.

   Parameters:

    relsize                 approximation of tuples in the relation
    diff                    number of different values found in the
                            sample

   Return: estimation of how many tuples will be found for a given key
           value
*/
double sql_tupleestd(
    ulong relsize,
    uint diff
);
