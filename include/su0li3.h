/*************************************************************************\
**  source       * su0li3.h
**  directory    * su
**  description  * License info routines common declarations
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


#ifndef SU0LI3_H
#define SU0LI3_H

#include <ssc.h>
#include <ssstddef.h>

#include <sstime.h>
#include "su0error.h"

#define SU_LI3_FNAME solid_licensefilename

typedef su_ret_t su_li3_ret_t;

#define SU_LI3_SYNC_UNLIMITED (-1)


typedef enum {
        SU_LI3F_NOTPRESENT           = 0,
        SU_LI3F_PRESENT              = 1,
        SU_LI3F_CORRUPT              = 2
} su_li3_presence_t;

#define SU_LI3_FILEIDSIZE        256

#define SU_LI3_OFS_CHECK         0       /* 4 bytes */
#define SU_LI3_OFS_FLAGS_0_31    4       /* 4 bytes; bits are: */
#       define SU_LI3_FLG_MATCHSOLVERSMINOR  (1L << 0)
#       define SU_LI3_FLG_MATCHSOLVERSMAJOR  (1L << 1)
#       define SU_LI3_FLG_JDBC               (1L << 2)
#       define SU_LI3_FLG_LCLI               (1L << 3)
#       define SU_LI3_FLG_ODBC               (1L << 4)
#       define SU_LI3_FLG_DESKTOP            (1L << 5)
#       define SU_LI3_FLG_SYNCPROPAGATE      (1L << 6)
#       define SU_LI3_FLG_SYNCSUBSCRIBE      (1L << 7)
#       define SU_LI3_FLG_READONLY           (1L << 8)
#       define SU_LI3_FLG_HOTSTANDBY         (1L << 9)
#       define SU_LI3_FLG_RPCSECURITY        (1L << 10)
#       define SU_LI3_FLG_FILESECURITY       (1L << 11)
#       define SU_LI3_FLG_BLOBCOMPRESS       (1L << 12)
#       define SU_LI3_FLG_INDEXCOMPRESS      (1L << 13)
#       define SU_LI3_FLG_RPCCOMPRESS        (1L << 14)
#       define SU_LI3_FLG_DISTRIBUTEDQUERY   (1L << 15)
#       define SU_LI3_FLG_DISTRIBUTEDUPDATE  (1L << 16)
#       define SU_LI3_FLG_XA                 (1L << 17)
#       define SU_LI3_FLG_JAVAPROCS          (1L << 18)
#       define SU_LI3_FLG_EXTERNALPROC       (1L << 19)
#       define SU_LI3_FLG_EXTERNALRPC        (1L << 20)
#       define SU_LI3_FLG_EXTERNALEXEC       (1L << 21)
#       define SU_LI3_FLG_EXTERNALBACKUP     (1L << 22)
#       define SU_LI3_FLG_BACKUPSERVER       (1L << 23)
#       define SU_LI3_FLG_PARALLEL           (1L << 24)
#       define SU_LI3_FLG_GENERIC            (1L << 25)
#       define SU_LI3_FLG_DISKLESS           (1L << 26)
#       define SU_LI3_FLG_ACCELERATOR        (1L << 27)
#       define SU_LI3_FLG_MAINMEMORY         (1L << 28)
#       define SU_LI3_FLG_DBE                (1L << 29)

#define SU_LI3_OFS_FLAGS_32_63   8       /* 4 bytes */

#define SU_LI3_OFS_CPUBITS       12      /* 4 bytes */
#define SU_LI3_OFS_OSBITS_0_31   16      /* 4 byte */
#define SU_LI3_OFS_OSBITS_32_63  20      /* 4 byte */
#define SU_LI3_OFS_SNUM          24      /* 4 bytes */
#define SU_LI3_OFS_WKSTALIMIT    28      /* 2 bytes */
#define SU_LI3_OFS_CONNECTLIMIT  30      /* 2 bytes */
#define SU_LI3_OFS_USRLIMIT      32      /* 2 bytes */
#define SU_LI3_OFS_SOLVERSMINOR  34      /* 1 byte */
#define SU_LI3_OFS_SOLVERSMAJOR  35      /* 1 byte */
#define SU_LI3_OFS_TIMELIMIT     36      /* 4 bytes */
#define SU_LI3_OFS_EXPDATE       40      /* 4 bytes */
#define SU_LI3_OFS_THRLIMIT      44      /* 1 byte */
#define SU_LI3_OFS_CPULIMIT      45      /* 1 byte */
#define SU_LI3_OFS_MASTERLIMIT   46      /* 2 bytes */
#define SU_LI3_OFS_REPLICALIMIT  48      /* 2 bytes */
#define SU_LI3_OFS_RESERVED      50      /* 14 bytes */

#define SU_LI3_DATASIZE          64

#define SU_LI3_OFS_PRODNAME_OLD      64      /* 64 bytes */
#define SU_LI3_OFS_LICENSETEXT_OLD   128     /* 256 bytes */
#define SU_LI3_OFS_LICENSEE_OLD      384     /* 128 bytes */
#define SU_LI3_PRODNAME_MAX_OLD \
        (SU_LI3_OFS_LICENSETEXT_OLD - SU_LI3_OFS_PRODNAME_OLD - 1)
#define SU_LI3_LICENSETXT_MAX_OLD \
        (SU_LI3_OFS_LICENSEE_OLD - SU_LI3_OFS_LICENSETEXT_OLD - 1)
#define SU_LI3_LICENSEELEN_MAX_OLD \
        (SU_LI3_SIZE_OLD - SU_LI3_OFS_LICENSEE_OLD - 1)

#define SU_LI3_SIZE_OLD          512

typedef struct {
        int         li_version;
        ss_char1_t* li_prodname;
        ss_char1_t* li_licensetext;
        ss_char1_t* li_licensee;
        ss_byte_t li_data[SU_LI3_DATASIZE];
} su_li3_t;

extern su_li3_t solid_licenseinfo;
extern ss_char1_t* solid_licensefilename;

/* Server runtime routines */

ss_byte_t* su_li3_initflat(
        su_li3_t* licenseinfo,
        size_t* p_len);

bool su_li3_loadfromfile(
        su_li3_t* licenseinfo,
        char* fname);

su_li3_presence_t su_li3_tryload(
        su_li3_t* licenseinfo,
        char* fname);

su_li3_ret_t su_li3_check(
        char** p_licensefname,
        char* exepath);

su_li3_ret_t su_li3_checkdbage(
        SsTimeT dbcreatime);

char* su_li3_getprodname(void);
int su_li3_getuserlimit(void);
int su_li3_getworkstationlimit(void);
int su_li3_getconnectlimit(void);

ss_uint4_t su_li3_gettimelimit(void);
SsTimeT su_li3_getexpdate(void);
char* su_li3_expdatestring(SsTimeT expdate, char* buf);
char* su_li3_getlicensetext(void);
char* su_li3_getlicensee(void);
ss_uint4_t su_li3_getsnum(void);

int su_li3_getthrlimit(void);
int su_li3_getcpulimit(void);
int su_li3_syncreplicalimit(void);
int su_li3_syncmasterlimit(void);

bool su_li3_isjdbcsupp(void);
bool su_li3_islclisupp(void);
bool su_li3_isodbcsupp(void);
bool su_li3_isdesktop(void);
bool su_li3_synccanpropagate(void);
bool su_li3_synccansubscribe(void);
bool su_li3_isparallelsupp(void);
bool su_li3_ishotstandbysupp(void);
bool su_li3_isdisklesssupp(void);
bool su_li3_isrpcsecuritysupp(void);
bool su_li3_isfilesecuritysupp(void);
bool su_li3_isblobcompresssupp(void);
bool su_li3_isindexcompresssupp(void);
bool su_li3_isrpccompresssupp(void);
bool su_li3_isdistributedquerysupp(void);
bool su_li3_isdistributedupdatesupp(void);
bool su_li3_isxasupp(void);
bool su_li3_isjavaprocsupp(void);
bool su_li3_isexternalprocsupp(void);
bool su_li3_isexternalrpcsupp(void);
bool su_li3_isexternalexecsupp(void);
bool su_li3_isexternalbackupsupp(void);
bool su_li3_isbackupserversupp(void);
bool su_li3_isreadonly(void);
bool su_li3_issync(void);
bool su_li3_isgeneric(void);
bool su_li3_isaccelerator(void);
bool su_li3_ismainmemsupp(void);
bool su_li3_isdbesupp(void);

char* su_li3_givelicensereport(void);
char* su_li3_givelicensereport_r(su_li3_t* licenseinfo);

/* Functions needed for generation of licenses */
su_li3_t* su_li3_init(
        int version,
        ss_uint4_t snum,
        char* prodname,
        char* licensetext,
        char* licensee,
        ss_uint4_t workstationlimit,
        ss_uint4_t connectlimit,
        ss_uint4_t userlimit,
        ss_uint4_t flags_0_31,
        ss_uint4_t flags_32_63,
        ss_uint4_t cpubitmask,
        ss_uint4_t osbitmask_0_31,
        ss_uint4_t osbitmask_32_63,
        ss_uint4_t solvers_major,
        ss_uint4_t solvers_minor,
        ss_uint4_t timelimit,
        ss_uint4_t expdate,
        ss_uint4_t thrlimit,
        ss_uint4_t cpulimit,
        ss_uint4_t syncmasterlimit,
        ss_uint4_t syncreplicalimit,
        ss_uint4_t reserved32b_1,
        ss_uint4_t reserved32b_2,
        ss_uint4_t reserved32b_3,
        ss_uint4_t reserved16b_4);

void su_li3_donebuf(
        su_li3_t* licenseinfo);

void su_li3_done(
        su_li3_t* licenseinfo);

void su_li3_globaldone(
        void);

bool su_li3_genfile(
        su_li3_t* licenseinfo,
        char* fname);

/* License eXtra Check routines (for paranoid checking against crackers) */

ss_uint4_t su_lxc_calcctc(ss_uint4_t creatime);

su_li3_ret_t su_lx3_para(
        SsTimeT dbcreatime,
        ss_uint4_t creatime_check);

su_li3_ret_t su_lx3_tcrypt(void);

#endif /* SU0LI3_H */
