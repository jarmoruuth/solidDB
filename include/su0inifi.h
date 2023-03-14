/*************************************************************************\
**  source       * su0inifi.h
**  directory    * su
**  description  * Windows .INI format configuration file utility
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


#ifndef SU0INIFI_H
#define SU0INIFI_H

#include <ssc.h>
#include <ssint8.h>
#include <ssfile.h>

#define INIFILE_COMMENTCHAR ';'

typedef struct su_inifile_st su_inifile_t;

typedef enum su_inifile_regkey_en {
        SU_REGKEY_CURRENT_USER,
        SU_REGKEY_LOCAL_MACHINE
} su_inifile_regkey_t;

char* su_inifile_search(
        char* fname);

su_inifile_t* su_inifile_init(
        char* fname,
        bool* p_found);

su_inifile_t* su_inifile_initreg(
        const char*               regname,
        su_inifile_regkey_t rootkey,
        bool*               p_found);

void su_inifile_done(
        su_inifile_t* inifile);

void su_inifile_link(
        su_inifile_t* inifile);

bool su_inifile_isfilefound(
        su_inifile_t* inifile);

char* su_inifile_getname(
        su_inifile_t* inifile);

void su_inifile_entermutex(
        su_inifile_t* inifile);

void su_inifile_exitmutex(
        su_inifile_t* inifile);

bool su_inifile_getvalue(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_give);

bool su_inifile_getlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        long* value);

bool su_inifile_getmillisec(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        long* value);

bool su_inifile_getint8(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        ss_int8_t* value);

bool su_inifile_getint(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        int* value);

bool su_inifile_getdouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        double* value);

bool su_inifile_getbool(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        bool* p_value);

bool su_inifile_getstring(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        char** value_give);

bool su_inifile_scanlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        long* value);

bool su_inifile_scanmillisec(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        long* value);

bool su_inifile_scanint8(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        ss_int8_t* value);

bool su_inifile_scandouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        double* value);

bool su_inifile_scanstring(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* separators,
        uint* scanindex,
        char** value_give);

void su_inifile_putstring(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        const char* value);

void su_inifile_putlong(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        long value);

void su_inifile_putbool(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        bool value);

void su_inifile_putdouble(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname,
        double value);

bool su_inifile_deletekeyline(
        su_inifile_t* inifile,
        const char* section,
        const char* keyname);

bool su_inifile_deletesection(
        su_inifile_t* inifile,
        const char* section);

bool su_inifile_save(
        su_inifile_t* inifile);

bool su_inifile_saveas(
        su_inifile_t* inifile,
        char* fname);

void su_inifile_savefp(
        su_inifile_t* inifile, 
        SS_FILE* fp);

void su_inifile_savecallback(
        su_inifile_t* inifile, 
        void (*callback)(void* ctx, char* line), 
        void* ctx);

char* su_inifile_getnthkeyline(
        su_inifile_t* inifile,
        const char* section_name,
        uint n);

bool su_inifile_putline(
        su_inifile_t* inifile,
        const char* section_name,
        const char* line);

char* su_inifile_getnthline(
        su_inifile_t* inifile,
        const char* section_name,
        uint n);

bool su_inifile_deletenthline(
        su_inifile_t* inifile,
        const char* section_name,
        uint n);

void su_inifile_ordersections(
        su_inifile_t* inifile,
        bool onoff);

#ifdef SS_DEBUG
void su_inifile_print(
        su_inifile_t* inifile);
#endif /* SS_DEBUG */

/* simple registering (used to be Quick'n'Dirty but it was decided that it is
 * not that dirty.
 */

void su_inifile_simplereg_setcompatibilitymode(bool mode);
bool su_inifile_simplereg_warningsdisabled(bool val);
void su_inifile_simplereg_setprintbuf(void (*sse_printbuf_fp)(char* buf));
void su_inifile_simplereg_init_and_check(void (*sse_printbuf_fp)(char* buf));
bool su_inifile_simplereg_checkforunregistered(char* inifilename,
                                               bool print);

#endif /* SU0INIFI_H */

/* EOF */
