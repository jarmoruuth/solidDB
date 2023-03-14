/*************************************************************************\
**  source       * tsamplea.h
**  directory    * res
**  description  * output of tsamplea.cpp with sample size = 100, 
**               * times = 1000 (integer version)
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


#define SAMPLE_S 100

static uint relsize_n = 14;
static ulong relsizes[] = {
    100,1000,2000,5000,10000,20000,50000,100000,200000,500000,1000000,2000000,5000000,10000000
};

static uint est_n = 22;
static ulong ests[] = {
    1,2,5,10,20,50,100,200,500,1000,2000,5000,10000,20000,50000,100000,200000,500000,1000000,2000000,5000000,10000000
};

static uint diffarray[] = {
    63,95,98,99,99,100,100,100,100,100,100,100,100,100,
    43,91,95,98,99,100,100,100,100,100,100,100,100,100,
    20,79,89,95,98,99,100,100,100,100,100,100,100,100,
    10,63,79,91,95,97,99,99,100,100,100,100,100,100,
    5,43,63,83,91,95,98,99,100,100,100,100,100,100,
    2,20,37,64,79,89,95,98,99,99,100,100,100,100,
    1,10,20,43,63,79,91,95,98,99,100,100,100,100,
    0, 5,10,25,43,63,83,91,95,98,99,100,100,100,
    0, 2,4,10,20,37,63,79,89,95,98,99,100,100,
    0, 1,2,5,10,20,43,63,79,91,95,98,99,100,
    0, 0, 1,2,5,10,25,43,64,83,91,95,98,99,
    0, 0, 0, 1,2,4,10,20,37,63,79,89,95,98,
    0, 0, 0, 0, 1,2,5,10,20,43,63,79,91,95,
    0, 0, 0, 0, 0, 1,2,5,10,25,43,63,83,91,
    0, 0, 0, 0, 0, 0, 1,2,4,10,20,37,63,79,
    0, 0, 0, 0, 0, 0, 0, 1,2,5,10,20,43,63,
    0, 0, 0, 0, 0, 0, 0, 0, 1,2,5,10,25,43,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1,2,4,10,20,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,2,5,10,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,2,5,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,2,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

