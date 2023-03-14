/*************************************************************************\
**  source       * tab0tli.h
**  directory    * tab
**  description  * Table Level Interface.
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


#ifndef TAB0TLI_H
#define TAB0TLI_H

#include <ssc.h>

#include <ssint8.h>

#include <dt0date.h>
#include <dt0type.h>

#include <uti0va.h>

#include <rs0types.h>
#include <rs0sysi.h>
#include <rs0entna.h>

#include <dbe0db.h>

#include "tab0conn.h"
#include "tab0tran.h"

#define TliConnectInit(cd)                      TliConnectInitEx(cd, (char *)__FILE__, __LINE__)
#define TliConnectInitByTabDb(tdb)              TliConnectInitByTabDbEx(tdb, (char *)__FILE__, __LINE__)
#define TliConnectInitByReadlevel(cd, trans)    TliConnectInitByReadlevelEx(cd, trans, (char *)__FILE__, __LINE__)

typedef enum {
        TLI_RC_SUCC,
        TLI_RC_END,
        TLI_RC_NULL_YES, /* internal: answer to ISNULL */
        TLI_RC_NULL_NO,  /* -"- */
        TLI_RC_COLNOTBOUND,
        TLI_ERR_FAILED = 100,
        TLI_ERR_CURNOTOPENED,
        TLI_ERR_CUROPENED,
        TLI_ERR_ORDERBYILL,
        TLI_ERR_COLNAMEILL,
        TLI_ERR_CONSTRILL,
        TLI_ERR_TYPECONVILL
} TliRetT;

typedef enum {
        TLI_RELOP_EQUAL,
        TLI_RELOP_NOTEQUAL,
        TLI_RELOP_LT,
        TLI_RELOP_GT,
        TLI_RELOP_LE,
        TLI_RELOP_GE,
        TLI_RELOP_LIKE,
        TLI_RELOP_ISNULL,
        TLI_RELOP_ISNOTNULL,
        TLI_RELOP_EQUAL_OR_ISNULL   /* Only for TliCursorConstrStr. */
} TliRelopT;

typedef struct TliConnectSt TliConnectT;
typedef struct TliCursorSt  TliCursorT;

TliConnectT* TliConnectInitEx(
        rs_sysi_t* cd, 
        char* file, 
        int line);

TliConnectT* TliConnectInitByTabDbEx(
        tb_database_t* tdb,
        char* file, 
        int line);

TliConnectT* TliConnectInitByTrans(
        rs_sysi_t* cd,
        tb_trans_t* trans);

TliConnectT* TliConnectInitByReadlevelEx(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* file, 
        int line);

void TliConnectDone(
        TliConnectT* tcon);

TliConnectT* TliConnect(
        char* servername,
        char* username,
        char* password);

void TliDisconnect(
        TliConnectT* tcon);

void TliConnectSetAppinfo(
        TliConnectT* tcon,
        char* appinfo);

TliRetT TliBeginTransact(
        TliConnectT* tcon);

TliRetT TliCommit(
        TliConnectT* tcon);

TliRetT TliRollback(
        TliConnectT* tcon);

TliRetT TliExecSQL(
        TliConnectT* tcon,
        char* sqlstr);

bool TliErrorInfo(
        TliConnectT* tcon,
        char** errstr,
        uint* errcode);

uint TliErrorCode(
        TliConnectT* tcon);

bool TliConnectCopySuErr(
        TliConnectT* tcon,
        su_err_t** p_target_errh);

void TliSetFailOnlyInCommit(
        TliConnectT* tcon,
        bool value);

rs_sysinfo_t* TliGetCd(
        TliConnectT* tcon);

dbe_db_t* TliGetDb(
        TliConnectT* tcon);

tb_database_t* TliGetTdb(
        TliConnectT* tcon);

tb_trans_t* TliGetTrans(
        TliConnectT* tcon);

TliCursorT* TliCursorCreate(
        TliConnectT* tcon,
        const char* catalog,
        const char* schema,
        const char* relname);

TliCursorT* TliCursorCreateRelh(
        TliConnectT* tcon,
        tb_relh_t* tbrelh);

TliCursorT* TliCursorCreateEn(
        TliConnectT* tcon,
        rs_entname_t* en);

void TliCursorFree(
        TliCursorT* tcur);

void TliCursorSetIsolationTransparent(
        TliCursorT* tcur,
        bool transparent);

void TliCursorSetMaxBlobSize(
        TliCursorT* tcur,
        long maxblobsize);

bool TliCursorErrorInfo(
        TliCursorT* tcur,
        char** errstr,
        uint* errcode);

uint TliCursorErrorCode(
        TliCursorT* tcur);

bool TliCursorCopySuErr(
        TliCursorT* tcur,
        su_err_t** p_target_errh);

TliRetT TliCursorColByNo(
        TliCursorT* tcur,
        int ano);

TliRetT TliCursorColInt(
        TliCursorT* tcur,
        const char* colname,
        int* intptr);

TliRetT TliCursorColLong(
        TliCursorT* tcur,
        const char* colname,
        long* longptr);

TliRetT TliCursorColInt4t(
        TliCursorT* tcur,
        const char* colname,
        ss_int4_t* int4tptr);

TliRetT TliCursorColInt8t(
        TliCursorT* tcur,
        const char* colname,
        ss_int8_t* int8tptr);

TliRetT TliCursorColSizet(
        TliCursorT* tcur,
        const char* colname,
        size_t* sizetptr);

TliRetT TliCursorColFloat(
        TliCursorT* tcur,
        const char* colname,
        float* floatptr);

TliRetT TliCursorColDouble(
        TliCursorT* tcur,
        const char* colname,
        double* doubleptr);

TliRetT TliCursorColDfloat(
        TliCursorT* tcur,
        const char* colname,
        dt_dfl_t* dfloatptr);

TliRetT TliCursorColStr(
        TliCursorT* tcur,
        const char* colname,
        char** strptr);

TliRetT TliCursorColDate(
        TliCursorT* tcur,
        const char* colname,
        dt_date_t* dateptr);

TliRetT TliCursorColData(
        TliCursorT* tcur,
        const char* colname,
        char** dataptr,
        size_t* lenptr);

TliRetT TliCursorColVa(
        TliCursorT* tcur,
        const char* colname,
        va_t** vaptr);

TliRetT TliCursorColAval(
        TliCursorT* tcur,
        const char* colname,
        rs_atype_t* atype,
        rs_aval_t* aval);

int TliCursorColIsNULL(
        TliCursorT* tcur,
        const char* colname);

TliRetT TliCursorColSetNULL(
        TliCursorT* tcur,
        const char* colname);

TliRetT TliCursorColClearNULL(
        TliCursorT* tcur,
        const char* colname);

TliRetT TliCursorOrderby(
        TliCursorT* tcur,
        const char* colname);

TliRetT TliCursorDescendingOrderby(
        TliCursorT* tcur,
        const char* colname);

TliRetT TliCursorConstrInt(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        int intval);

TliRetT TliCursorConstrInt8t(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        ss_int8_t int8val);

TliRetT TliCursorConstrLong(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        long longval);

TliRetT TliCursorConstrStr(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        char* strval);

TliRetT TliCursorConstrDate(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        dt_date_t* dateval);

TliRetT TliCursorConstrData(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        char* dataval,
        size_t datalen);

TliRetT TliCursorConstrVa(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        va_t* vaval);

TliRetT TliCursorConstrAval(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        rs_atype_t* atype,
        rs_aval_t* aval);

TliRetT TliCursorConstrIsNull(
        TliCursorT* tcur,
        rs_atype_t* atype,
        char *colname);

TliRetT TliCursorOpen(
        TliCursorT* tcur);

TliRetT TliCursorNext(
        TliCursorT* tcur);

rs_tval_t* TliCursorNextTval (
        TliCursorT *tcur);

TliRetT TliCursorPrev(
        TliCursorT* tcur);

TliRetT TliCursorInsert(
        TliCursorT* tcur);

TliRetT TliCursorDelete(
        TliCursorT* tcur);

TliRetT TliCursorUpdate(
        TliCursorT* tcur);

su_chcollation_t TliGetCollation(
        TliConnectT* tcon);

TliRetT TliCursorColUTF8(
        TliCursorT* tcur,
        const char* colname,
        ss_char1_t** strptr);

TliRetT TliCursorConstrUTF8(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        char* UTF8strval);

bool TliTransIsFailed(
        TliConnectT* tcon);

#define TliCursorColStr     TliCursorColUTF8
#define TliCursorConstrStr  TliCursorConstrUTF8

#endif /* TAB0TLI_H */
