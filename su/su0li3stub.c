/*************************************************************************\
**  source       * su0li3stub.c
**  directory    * su
**  description  * stub license file for some module tests
**               * that do not start server but use license-
**               * enabled services.
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

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include "su0error.h"
#include "su0li3.h"

ss_char1_t* solid_licensefilename = (ss_char1_t*)"no licensefile";
char*       diskless_licfile = NULL; /* for diskless, in-memory license file */

ss_byte_t* su_li3_initflat(
        su_li3_t* licenseinfo __attribute__ ((unused)),
        size_t* p_len __attribute__ ((unused)))
{
        return (NULL);
}

bool su_li3_loadfromfile(
        su_li3_t* licenseinfo __attribute__ ((unused)),
        char* fname __attribute__ ((unused)))
{
        return (TRUE);
}

su_li3_presence_t su_li3_tryload(
        su_li3_t* licenseinfo __attribute__ ((unused)),
        char* fname __attribute__ ((unused)))
{
        return (SU_LI3F_NOTPRESENT);
}

su_li3_ret_t su_li3_check(
        char** p_licensefname,
        char* exepath __attribute__ ((unused)))
{
        *p_licensefname = NULL;
        return (SU_LI_OK);
}

su_li3_ret_t su_li3_checkdbage(
        SsTimeT dbcreatime __attribute__ ((unused)))
{
        return (SU_LI_OK);
}
char* su_li3_getprodname(void)
{
#ifdef SS_MYSQL
        return ((char *)"solidDB for MySQL Beta 1");
#else
        return ((char *)"solid");
#endif
}
        
int su_li3_getuserlimit(void)
{
        return (0);
}
int su_li3_getworkstationlimit(void)
{
        return (0);
}

int su_li3_getconnectlimit(void)
{
        return (0);
}

ss_uint4_t su_li3_gettimelimit(void)
{
        return (0);
}
SsTimeT su_li3_getexpdate(void)
{
        return (0);
}
char* su_li3_getlicensetext(void)
{
#ifdef SS_MYSQL
        return ((char *)"Prototype version");
#else
        return ((char *)"Stub version");
#endif
}
    
char* su_li3_getlicensee(void)
{
        return ((char *)"Solid");
}
ss_uint4_t su_li3_getsnum(void)
{
        return (123456);
}

int su_li3_getthrlimit(void)
{
        return (0);
}
int su_li3_getcpulimit(void)
{
        return (0);
}
int su_li3_syncreplicalimit(void)
{
        return (0);
}
int su_li3_syncmasterlimit(void)
{
        return (0);
}

bool su_li3_isjdbcsupp(void) { return (TRUE); }
bool su_li3_islclisupp(void) { return (TRUE); }
bool su_li3_isodbcsupp(void) { return (TRUE); }

#ifdef SS_MYSQL_AC
bool su_li3_ishotstandbysupp(void) { return (TRUE); }
bool su_li3_ismainmemsupp(void) { return (TRUE); }
#else

#ifdef SS_MYSQL
bool su_li3_ishotstandbysupp(void) { return (FALSE); }
bool su_li3_ismainmemsupp(void) { return (FALSE); }
#else
bool su_li3_ishotstandbysupp(void) { return (TRUE); }
bool su_li3_ismainmemsupp(void) { return (TRUE); }
#endif

#endif /* SS_MYSQL_AC */

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
bool su_li3_isdesktop(void) { return (FALSE); }
bool su_li3_synccanpropagate(void) { return (FALSE); }
bool su_li3_synccansubscribe(void) { return (FALSE); }
bool su_li3_isdistributedquerysupp(void) { return (FALSE); }
bool su_li3_isdistributedupdatesupp(void) { return (FALSE); }
bool su_li3_isxasupp(void) { return (FALSE); }
bool su_li3_isexternalprocsupp(void) { return (FALSE); }
bool su_li3_issync(void) { return (FALSE); }
#else
bool su_li3_isdesktop(void) { return (TRUE); }
bool su_li3_synccanpropagate(void) { return (TRUE); }
bool su_li3_synccansubscribe(void) { return (TRUE); }
bool su_li3_isdistributedquerysupp(void) { return (TRUE); }
bool su_li3_isdistributedupdatesupp(void) { return (TRUE); }
bool su_li3_isxasupp(void) { return (TRUE); }
bool su_li3_isexternalprocsupp(void) { return (TRUE); }
bool su_li3_issync(void) { return (TRUE); }
#endif
bool su_li3_isdbesupp(void) { return (TRUE); }
bool su_li3_isparallelsupp(void) { return (TRUE); }
bool su_li3_isdisklesssupp(void) { return (TRUE); }
bool su_li3_isrpcsecuritysupp(void) { return (TRUE); }
bool su_li3_isfilesecuritysupp(void) { return (TRUE); }
bool su_li3_isblobcompresssupp(void) { return (TRUE); }
bool su_li3_isindexcompresssupp(void) { return (TRUE); }
bool su_li3_isrpccompresssupp(void) { return (TRUE); }
bool su_li3_isjavaprocsupp(void) { return (TRUE); }
bool su_li3_isexternalrpcsupp(void) { return (TRUE); }
bool su_li3_isexternalexecsupp(void) { return (TRUE); }
bool su_li3_isexternalbackupsupp(void) { return (TRUE); }
bool su_li3_isbackupserversupp(void) { return (TRUE); }
bool su_li3_isreadonly(void) { return (FALSE); }
bool su_li3_isgeneric(void) { return (TRUE); }
bool su_li3_isaccelerator(void) { return (TRUE); }

char* su_li3_givelicensereport(void) 
{ 
#ifdef SS_MYSQL
        return (SsMemStrdup((char *)"License for prototype version")); 
#else
        return (SsMemStrdup((char *)"License granted")); 
#endif
}

char* su_li3_givelicensereport_r(su_li3_t* licenseinfo __attribute__ ((unused)))
{
        return (NULL);
}

void su_li3_donebuf(
        su_li3_t* licenseinfo __attribute__ ((unused)))
{
}

void su_li3_globaldone(
        void)
{
}
/* License eXtra Check routines (for paranoid checking against crackers) */

#if 0
ss_uint4_t su_lxc_calcctc(ss_uint4_t creatime)
{
        return (1234);
}
#endif /* 0 */

su_li3_ret_t su_lx3_para(
        SsTimeT dbcreatime __attribute__ ((unused)),
        ss_uint4_t creatime_check __attribute__ ((unused)))
{
        return (SU_LI_OK);
}

su_li3_ret_t su_lx3_tcrypt(void)
{
        return (SU_LI_OK);
}
