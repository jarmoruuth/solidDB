/*************************************************************************\
**  source       * tab0tli.c
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

CREATE TABLE TEST(
        INTF1 INTEGER,
        INTF2 INTEGER,
        CHARF CHAR(10));

CREATE INDEX TEST_INDEX ON TEST (INTF2);

void TliExample(void)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        long l;
        char* str;

        tcon = TliConnect("server", "user", "password");

        /* Insert. */
        tcur = TliCursorCreate(tcon, "SCHEMA", "TEST");
        TliCursorColLong(tcur, "INTF1", &l);
        TliCursorColStr(tcur, "CHARF", &str);
        printf("Delete\n");
        l = 1L;
        str = "ONE";
        TliCursorInsert(tcur);
        TliCursorFree(tcur);

        /* Search. */
        tcur = TliCursorCreate(tcon, "TEST");
        TliCursorColLong(tcur, "INTF1", &l);
        TliCursorColStr(tcur, "CHARF", &str);
        TliCursorOrderby(tcur, "INTF2");
        TliCursorConstrStr(tcur, "CHARF", TLI_RELOP_GT, "A");
        TliCursorOpen(tcur);
        while (TliCursorNext(tcur) == TLI_RC_SUCC) {
            printf("Search: %ld %s\n", l, str);
        }
        TliCursorFree(tcur);

        /* Delete. */
        tcur = TliCursorCreate(tcon, "TEST");
        TliCursorConstrStr(tcur, "CHARF", TLI_RELOP_GT, "A");
        TliCursorOpen(tcur);
        while (TliCursorNext(tcur) == TLI_RC_SUCC) {
            printf("Delete\n");
            TliCursorDelete(tcur);
        }
        TliCursorFree(tcur);

        TliCommit(tcon);

        TliDisconnect(tcon);
}

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstdlib.h>
#include <ssstring.h>
#include <sslimits.h>
#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <uti0dyn.h>

#include <dt0dfloa.h>

#include <su0parr.h>
#include <su0inifi.h>
#include <su0cfgst.h>

#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0relh.h>

#include "tab1defs.h"
#include "tab0conn.h"
#include "tab0relh.h"
#include "tab0relc.h"
#include "tab0tran.h"
#include "tab0blobg2.h"
#include "tab0sqls.h"
#include "tab0tli.h"
#include "tab0tint.h"

#define CHK_TCON(tcon)  ss_dassert(SS_CHKPTR(tcon) && tcon->tcon_chk == TBCHK_TLICON)
#define CHK_TCUR(tcon)  ss_dassert(SS_CHKPTR(tcur) && tcur->tcur_chk == TBCHK_TLICUR)

#define TLI_MAXBLOBSIZE (SS_MAXALLOCSIZE < 65535L ? SS_MAXALLOCSIZE : 65535L)

typedef enum {
        TCOL_NULLOP_IS,
        TCOL_NULLOP_SET,
        TCOL_NULLOP_CLEAR
} tcol_nullop_t;

typedef enum {
        TCOL_DT_INT,
        TCOL_DT_LONG,
        TCOL_DT_SIZET,
        TCOL_DT_INT4T,
        TCOL_DT_INT8T,
        TCOL_DT_FLOAT,
        TCOL_DT_DOUBLE,
        TCOL_DT_DFLOAT,
        TCOL_DT_STR,
        TCOL_DT_DATE,
        TCOL_DT_DATA,
        TCOL_DT_VA,
        TCOL_DT_AVAL, 
        TCOL_DT_UTF8
} tcol_dt_t;

typedef struct {
        rs_atype_t* ta_atype;
        rs_aval_t*  ta_aval;
} tcol_aval_t;

typedef struct {
        int         tcol_ano;
        tcol_dt_t   tcol_dt;
        bool        tcol_isnull;
        union {
            int*            p_int;
            long*           p_long;
            size_t*         p_sizet;
            ss_int4_t*      p_int4t;
            ss_int8_t*      p_int8t;
            float*          p_float;
            double*         p_double;
            dt_dfl_t*       p_dfloat;
            char**          p_str;
            dt_date_t*      p_date;
            va_t**          p_va;
            tcol_aval_t*    p_ta;
            ss_char1_t** p_UTF8str;
            struct {
                char**  p_ptr;
                size_t* p_len;
            } data;
        } tcol_;
        void* tcol_tmpbuf;
} tcol_t;

struct TliConnectSt {
        int             tcon_chk;       /* Really tb_check_t */
        tb_connect_t*   tcon_tbcon;
        rs_sysinfo_t*   tcon_usercd;
        rs_sysinfo_t*   tcon_tbconcd;
        tb_trans_t*     tcon_trans;
        bool            tcon_failonlyincommit;
        bool            tcon_localtrans;
        rs_err_t*       tcon_errh;
        char*           tcon_errstr;
        uint            tcon_errcode;
};

struct TliCursorSt {
        int             tcur_chk;
        TliConnectT*    tcur_tcon;
        rs_sysinfo_t*   tcur_cd;
        su_pa_t*        tcur_cols;
        tb_trans_t*     tcur_trans;
        tb_relh_t*      tcur_tbrelh;
        rs_relh_t*      tcur_relh;
        rs_ttype_t*     tcur_relhttype;
        rs_atype_t*     tcur_esctype;
        rs_aval_t*      tcur_escinst;
        tb_relcur_t*    tcur_tbcur;
        rs_ttype_t*     tcur_ttype;
        rs_err_t*       tcur_errh;
        bool            tcur_opened;
        int             tcur_norderby;
        bool            tcur_failonlyincommit;
        char*           tcur_errstr;
        uint            tcur_errcode;
        long            tcur_maxblobsize;
        tb_tint_t*      tcur_funblock;
        bool            tcur_free_relh;
};

static ulong         nconnect;
static su_inifile_t* inifile;

static TliRetT tcur_setconstr(
        TliCursorT* tcur,
        char* colname,
        TliRelopT relop,
        rs_atype_t* atype,
        rs_aval_t* aval);

static TliRetT tcur_setcol_ano(
        TliCursorT* tcur,
        int ano,
        tcol_dt_t dt,
        void* ptr,
        size_t* p_len);

/*#***********************************************************************\
 *
 *              tcon_errorfree
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tcon_errorfree(TliConnectT* tcon)
{
        ss_dassert(tcon->tcon_errh != NULL);

        rs_error_free(tcon->tcon_usercd, tcon->tcon_errh);
        tcon->tcon_errh = NULL;
        if (tcon->tcon_errstr != NULL) {
            SsMemFree(tcon->tcon_errstr);
            tcon->tcon_errstr = NULL;
        }
        tcon->tcon_errcode = 0;
}

/*#***********************************************************************\
 *
 *              connect_init
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      tbcon -
 *
 *
 *      failonlyincommit -
 *
 *
 *      trans -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void connect_init(
        TliConnectT* tcon,
        rs_sysi_t* usercd,
        tb_connect_t* tbcon,
        bool failonlyincommit,
        tb_trans_t* trans,
        bool localtrans)
{
        tcon->tcon_chk = TBCHK_TLICON;
        tcon->tcon_tbcon = tbcon;
        tcon->tcon_tbconcd = tb_getclientdata(tbcon);
        if (usercd == NULL) {
            tcon->tcon_usercd = tcon->tcon_tbconcd;
        } else {
            tcon->tcon_usercd = usercd;
        }
        tcon->tcon_trans = trans;
        tcon->tcon_localtrans = localtrans;
        tcon->tcon_failonlyincommit = failonlyincommit;
        tcon->tcon_errh = NULL;
        tcon->tcon_errstr = NULL;
        tcon->tcon_errcode = 0;

        CHK_TCON(tcon);
}

/*##**********************************************************************\
 *
 *              TliConnectInitEx
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      tbcon -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliConnectT* TliConnectInitEx(
        rs_sysi_t* cd,
        char* file, 
        int line)
{
        tb_connect_t* tbcon;
        TliConnectT* tcon;
        tb_trans_t* tbtrans;
        rs_sysi_t* new_cd;

        tbcon = tb_sysconnect_init_ex(rs_sysi_tabdb(cd), file, line);
        tb_sysconnect_transinit(tbcon);
        tbtrans = tb_getsqltrans(tbcon);
        new_cd = tb_getclientdata(tbcon);

        tcon = SSMEM_NEW(TliConnectT);

        if (rs_sysi_isinsidedbeatomicsection(cd)) {
            rs_sysi_setinsidedbeatomicsection(new_cd, TRUE);
            tb_trans_setsystrans(new_cd, tbtrans);
        }
        if (tb_connect_logfailureallowed(tbcon)) {
            tb_trans_allowlogfailure(new_cd, tbtrans);
        }
        rs_sysi_copydbactioncounter(new_cd, cd);
        ss_debug(rs_sysi_copydbactionshared(new_cd, cd));


        connect_init(
            tcon,
            NULL,
            tbcon,
            TRUE,
            tbtrans,
            TRUE);

        CHK_TCON(tcon);

        return(tcon);
}

/*##**********************************************************************\
 *
 *              TliConnectInitByTabDbEx
 *
 * Inits Tli connect using tb_database_t.
 *
 * Parameters :
 *
 *      tdb -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliConnectT* TliConnectInitByTabDbEx(
        tb_database_t* tdb, 
        char* file, 
        int line)
{
        tb_connect_t* tbcon;
        TliConnectT* tcon;
        tb_trans_t* tbtrans;
        rs_sysi_t* cd;

        tbcon = tb_sysconnect_init_ex(tdb, file, line);
        tb_sysconnect_transinit(tbcon);
        tbtrans = tb_getsqltrans(tbcon);
        cd = tb_getclientdata(tbcon);

        tcon = SSMEM_NEW(TliConnectT);

        if (tb_connect_logfailureallowed(tbcon)) {
            tb_trans_allowlogfailure(cd, tbtrans);
        }
        connect_init(
            tcon,
            NULL,
            tbcon,
            TRUE,
            tbtrans,
            TRUE);

        CHK_TCON(tcon);

        return(tcon);
}

/*##**********************************************************************\
 *
 *              TliConnectInitByTrans
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliConnectT* TliConnectInitByTrans(
        rs_sysi_t* cd,
        tb_trans_t* trans)
{
        tb_connect_t* tbcon;
        TliConnectT* tcon;
        rs_sysi_t* new_cd;

        tbcon = tb_sysconnect_init(rs_sysi_tabdb(cd));
        new_cd = tb_getclientdata(tbcon);

        rs_sysi_copydbactioncounter(new_cd, cd);
        ss_debug(rs_sysi_copydbactionshared(new_cd, cd));

        tcon = SSMEM_NEW(TliConnectT);

        connect_init(
            tcon,
            cd,
            tbcon,
            TRUE,
            trans,
            FALSE);

        CHK_TCON(tcon);

        return(tcon);
}

/*##**********************************************************************\
 *
 *              TliConnectInitByReadlevelEx
 *
 * Creates a connection with a new transaction but uses read level from
 * parameter trans.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              trans -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliConnectT* TliConnectInitByReadlevelEx(
        rs_sysi_t* cd,
        tb_trans_t* trans, 
        char* file, 
        int line)
{

        TliConnectT* tcon;

        tcon = TliConnectInitEx(cd, file, line);

        tb_trans_beginif(cd, trans);
        tb_trans_beginif(tcon->tcon_usercd, tcon->tcon_trans);

        tb_trans_inheritreadlevel(
            tcon->tcon_usercd,
            tcon->tcon_trans,
            trans);

        return(tcon);
}

/*##**********************************************************************\
 *
 *              TliConnectDone
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void TliConnectDone(
        TliConnectT* tcon)
{
        CHK_TCON(tcon);

        tb_sysconnect_done(tcon->tcon_tbcon);
        if (tcon->tcon_errh != NULL) {
            tcon_errorfree(tcon);
        }
        tcon->tcon_chk = TBCHK_FREED;

        SsMemFree(tcon);
}

/*##**********************************************************************\
 *
 *              TliConnect
 *
 *
 *
 * Parameters :
 *
 *      servername -
 *
 *
 *      username -
 *
 *
 *      password -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliConnectT* TliConnect(
        char* servername __attribute__ ((unused)),
        char* username,
        char* password)
{
        TliConnectT* tcon;
        tb_connect_t* tbcon;

        if (username == NULL || password == NULL) {
            return(NULL);
        }

        if (nconnect++ == 0) {
            inifile = su_inifile_init(SU_SOLINI_FILENAME, NULL);
            tb_init(inifile, NULL);
        }

        tbcon = tb_connect_local(-1, username, password);

        if (tbcon == NULL) {
            if (--nconnect == 0) {
                tb_done();
            }
            return(NULL);
        }

        tcon = SSMEM_NEW(TliConnectT);

        connect_init(
            tcon,
            NULL,
            tbcon,
            FALSE,
            tb_getsqltrans(tbcon),
            FALSE);

        return(tcon);
}

/*##**********************************************************************\
 *
 *              TliDisconnect
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void TliDisconnect(TliConnectT* tcon)
{
        CHK_TCON(tcon);
        ss_dassert(!tcon->tcon_localtrans);

        if (tcon->tcon_errh != NULL) {
            tcon_errorfree(tcon);
        }

        tb_disconnect(tcon->tcon_tbcon);

        ss_dassert(nconnect > 0);
        if (--nconnect == 0) {
            tb_done();
            su_inifile_done(inifile);
        }
        tcon->tcon_chk = TBCHK_FREED;

        SsMemFree(tcon);
}

/*##**********************************************************************\
 *
 *              TliConnectSetAppinfo
 *
 * Sets appinfo string.
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      appinfo -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void TliConnectSetAppinfo(TliConnectT* tcon, char* appinfo)
{
        CHK_TCON(tcon);

        tb_setappinfo(tcon->tcon_tbcon, appinfo);
}

/*##**********************************************************************\
 *
 *              TliBeginTransact
 *
 * Begins a transaction explicitly if it has not been started already
 *
 * Parameters :
 *
 *      tcon - in, use
 *              connection
 *
 * Return value :
 *      TLI_RC_SUCC when successful
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliBeginTransact(TliConnectT* tcon)
{
        CHK_TCON(tcon);
        tb_trans_beginif(tcon->tcon_usercd, tcon->tcon_trans);
        return (TLI_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              TliCommit
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCommit(TliConnectT* tcon)
{
        bool finished;
        bool succp;

        CHK_TCON(tcon);
#ifdef DBE_HSB_REPLICATION
        /* ss_dassert(!tb_trans_iswrites(tcon->tcon_usercd, tcon->tcon_trans)); */
#endif

        if (tcon->tcon_errh != NULL) {
            tcon_errorfree(tcon);
        }

        do {
            succp = tb_trans_commit(
                        tcon->tcon_usercd,
                        tcon->tcon_trans,
                        &finished,
                        &tcon->tcon_errh);
        } while (!finished);

        if (succp) {
            return(TLI_RC_SUCC);
        } else {
            return(TLI_ERR_FAILED);
        }
}

/*##**********************************************************************\
 *
 *              TliRollback
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliRollback(TliConnectT* tcon)
{
        bool finished;
        bool succp;

        CHK_TCON(tcon);

        if (tcon->tcon_errh != NULL) {
            tcon_errorfree(tcon);
        }

        do {
            succp = tb_trans_rollback(
                        tcon->tcon_usercd,
                        tcon->tcon_trans,
                        NULL,
                        &finished,
                        TRUE,
                        &tcon->tcon_errh);
        } while (!finished);

        if (succp) {
            return(TLI_RC_SUCC);
        } else {
            return(TLI_ERR_FAILED);
        }
}

/*#***********************************************************************\
 *
 *              TliCommitStmt
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static TliRetT TliCommitStmt(TliConnectT* tcon)
{
        bool finished;
        bool succp;

        CHK_TCON(tcon);

        if (tcon->tcon_errh != NULL) {
            tcon_errorfree(tcon);
        }

        do {
            succp = tb_trans_stmt_commit(
                        tcon->tcon_usercd,
                        tcon->tcon_trans,
                        &finished,
                        &tcon->tcon_errh);
        } while (!finished);

        if (succp) {
            return(TLI_RC_SUCC);
        } else {
            return(TLI_ERR_FAILED);
        }
}

/*#***********************************************************************\
 *
 *              TliRollbackStmt
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static TliRetT TliRollbackStmt(TliConnectT* tcon)
{
        bool finished;
        bool succp;

        CHK_TCON(tcon);

        do {
            succp = tb_trans_stmt_rollback(
                        tcon->tcon_usercd,
                        tcon->tcon_trans,
                        &finished,
                        NULL);
        } while (!finished);

        if (succp) {
            return(TLI_RC_SUCC);
        } else {
            return(TLI_ERR_FAILED);
        }
}

/*##**********************************************************************\
 *
 *              TliExecSQL
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      sqlstr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
#ifndef SS_MYSQL
TliRetT TliExecSQL(TliConnectT* tcon, char* sqlstr)
{
        bool succp;
        sqlsystem_t* sqls;

        CHK_TCON(tcon);

        if (tcon->tcon_errh != NULL) {
            tcon_errorfree(tcon);
        }

        sqls = tb_sqls_init(tcon->tcon_tbconcd);

        tb_trans_settransoption(
            tcon->tcon_tbconcd,
            tcon->tcon_trans,
            TB_TRANSOPT_HSBFLUSH_NO);

        succp = sql_execdirect(tcon->tcon_tbconcd, sqls, tcon->tcon_trans, sqlstr);

        tb_trans_settransoption(
            tcon->tcon_tbconcd,
            tcon->tcon_trans,
            TB_TRANSOPT_HSBFLUSH_YES);

        if (!succp) {
            tb_sqls_builderrh(tcon->tcon_tbconcd, sqls, &tcon->tcon_errh);
        }

        tb_sqls_done(tcon->tcon_tbconcd, sqls);

        return(succp ? TLI_RC_SUCC : TLI_ERR_FAILED);
}
#endif /* !SS_MYSQL */

/*##**********************************************************************\
 *
 *              TliGetCd
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_sysinfo_t* TliGetCd(TliConnectT* tcon)
{
        CHK_TCON(tcon);

        return(tcon->tcon_usercd);
}

/*##**********************************************************************\
 *
 *              TliGetTrans
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
tb_trans_t* TliGetTrans(TliConnectT* tcon)
{
        CHK_TCON(tcon);

        return(tcon->tcon_trans);
}

/*##**********************************************************************\
 *
 *              TliTransIsFailed
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool TliTransIsFailed(TliConnectT* tcon)
{
        CHK_TCON(tcon);

        return(tb_trans_isfailed(tcon->tcon_usercd, tcon->tcon_trans));
}

/*##**********************************************************************\
 *
 *              TliSetFailOnlyInCommit
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      value -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void TliSetFailOnlyInCommit(TliConnectT* tcon, bool value)
{
        CHK_TCON(tcon);

        tcon->tcon_failonlyincommit = value;
}

/*##**********************************************************************\
 *
 *              TliGetDb
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_db_t* TliGetDb(TliConnectT* tcon)
{
        CHK_TCON(tcon);

        return(tb_getdb(tcon->tcon_tbcon));
}

/*##**********************************************************************\
 *
 *              TliGetTdb
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_database_t* TliGetTdb(TliConnectT* tcon)
{
        CHK_TCON(tcon);

        return(tb_gettdb(tcon->tcon_tbcon));
}


/*##**********************************************************************\
 *
 *              TliErrorInfo
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      errstr -
 *
 *
 *      errcode -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool TliErrorInfo(
        TliConnectT* tcon,
        char** errstr,
        uint* errcode)
{
        CHK_TCON(tcon);

        if (tcon->tcon_errh == NULL) {
            if (errstr != NULL) {
                *errstr = NULL;
            }
            if (errcode != NULL) {
                *errcode = 0;
            }
            return(FALSE);
        } else {
            if (tcon->tcon_errstr == NULL) {
                rs_error_printinfo(
                    tcon->tcon_usercd,
                    tcon->tcon_errh,
                    &tcon->tcon_errcode,
                    &tcon->tcon_errstr);
            }
            if (errstr != NULL) {
                *errstr = tcon->tcon_errstr;
            }
            if (errcode != NULL) {
                *errcode = tcon->tcon_errcode;
            }
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              TliErrorCode
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
uint TliErrorCode(
        TliConnectT* tcon)
{
        CHK_TCON(tcon);

        if (tcon->tcon_errh == NULL) {
            return(SU_SUCCESS);
        } else {
            if (tcon->tcon_errstr == NULL) {
                rs_error_printinfo(
                    tcon->tcon_usercd,
                    tcon->tcon_errh,
                    &tcon->tcon_errcode,
                    &tcon->tcon_errstr);
            }
            return(tcon->tcon_errcode);
        }
}

/*##**********************************************************************\
 *
 *              TliConnectCopySuErr
 *
 * Gets possible error information as an error handle.
 *
 * Parameters :
 *
 *      tcon - in, use
 *          connection
 *
 *      p_target_errh - out, give
 *          if p_target_errh != NULL and
 *          error state is avaliable a newly allocated
 *          error handle is put to *p_target_errh.
 *
 *
 * Return value :
 *      TRUE - error information was available
 *      FALSE - no error status in tcon
 *
 * Limitations  :
 *
 * Globals used :
 */
bool TliConnectCopySuErr(
        TliConnectT* tcon,
        su_err_t** p_target_errh)
{
        CHK_TCON(tcon);

        if (tcon->tcon_errh == NULL) {
            if (tb_trans_isfailed(tcon->tcon_usercd, tcon->tcon_trans)) {
                tb_trans_geterrcode(tcon->tcon_usercd, tcon->tcon_trans, p_target_errh);
                return(*p_target_errh != NULL);
            }
            return(FALSE);
        } else {
            su_err_copyerrh(p_target_errh, tcon->tcon_errh);
            return(TRUE);
        }
}

/*#***********************************************************************\
 *
 *              tcur_errorfree
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void tcur_errorfree(TliCursorT* tcur)
{
        ss_dassert(tcur->tcur_errh != NULL);

        rs_error_free(tcur->tcur_cd, tcur->tcur_errh);
        tcur->tcur_errh = NULL;
        if (tcur->tcur_errstr != NULL) {
            SsMemFree(tcur->tcur_errstr);
            tcur->tcur_errstr = NULL;
        }
        tcur->tcur_errcode = 0;
}

/*#***********************************************************************\
 *
 *              tcur_settransfailed
 *
 * Sets current transaction as failed.
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void tcur_settransfailed(TliCursorT* tcur)
{
        rs_sysinfo_t* cd;
        uint errcode;

        cd = tcur->tcur_cd;

        if (tcur->tcur_errh != NULL) {
            rs_error_printinfo(cd, tcur->tcur_errh, &errcode, NULL);

            if (errcode < DBE_ERRORBEGIN || errcode > DBE_ERR_ERROREND) {
                errcode = DBE_ERR_FAILED;
            }

        } else {
            errcode = DBE_ERR_FAILED;
        }
        tb_trans_setfailed(cd, tcur->tcur_trans, errcode);
}

/*##**********************************************************************\
 *
 *              TliCursorCreate
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 *      schema -
 *
 *
 *      relname -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliCursorT* TliCursorCreate(
        TliConnectT* tcon,
        const char* catalog,
        const char* schema,
        const char* relname)
{
        tb_relh_t* tbrelh;
        rs_sysinfo_t* cd;
        TliCursorT* tcur;

        CHK_TCON(tcon);
        ss_dassert(relname != NULL);

        if (tcon->tcon_errh != NULL) {
            tcon_errorfree(tcon);
        }

        cd = tcon->tcon_tbconcd;

        tbrelh = tb_relh_create(cd, tcon->tcon_trans, (char *)relname, (char *)schema, (char *)catalog, &tcon->tcon_errh);
        if (tbrelh == NULL) {
            return(NULL);
        }

        tcur = TliCursorCreateRelh(tcon, tbrelh);
        tcur->tcur_free_relh = TRUE;
        return tcur;
}

TliCursorT* TliCursorCreateRelh(
        TliConnectT* tcon,
        tb_relh_t* tbrelh)
{
        rs_relh_t* relh;
        TliCursorT* tcur = SSMEM_NEW(TliCursorT);
        rs_sysinfo_t* cd;

        cd = tcon->tcon_tbconcd;
        relh = tb_relh_rsrelh(cd, tbrelh);

        tcur->tcur_chk = TBCHK_TLICUR;
        tcur->tcur_tcon = tcon;
        tcur->tcur_cd = cd;
        tcur->tcur_cols = su_pa_init();
        tcur->tcur_trans = tcon->tcon_trans;
        tcur->tcur_tbrelh = tbrelh;
        tcur->tcur_relh = relh;
        tcur->tcur_relhttype = rs_relh_ttype(cd, relh);
        tcur->tcur_esctype = rs_atype_initchar(cd);
        tcur->tcur_escinst = rs_aval_create(cd, tcur->tcur_esctype);
        tcur->tcur_free_relh = FALSE;
        {
            ss_debug(bool succp =)
            rs_aval_set8bitstr_ext(cd, tcur->tcur_esctype, tcur->tcur_escinst, (char *)"\\", NULL);
            ss_dassert(succp);
        }
        tcur->tcur_funblock = tb_tint_init();

        tcur->tcur_tbcur = tcur->tcur_funblock->cursor_create(
                                cd,
                                tcur->tcur_trans,
                                tbrelh,
                                1000, /* 1000 makes an SASELECT that lives over
                                         commit for M-tables. */
                                FALSE);
        tcur->tcur_ttype = NULL;
        tcur->tcur_opened = FALSE;
        tcur->tcur_norderby = 0;
        tcur->tcur_failonlyincommit = tcon->tcon_failonlyincommit;
        tcur->tcur_errh = NULL;
        tcur->tcur_errstr = NULL;
        tcur->tcur_errcode = 0;
        tcur->tcur_maxblobsize = TLI_MAXBLOBSIZE;

        tcur->tcur_funblock->cursor_disableinfo(cd, tcur->tcur_tbcur);

        return(tcur);
}

TliCursorT* TliCursorCreateEn(
        TliConnectT* tcon,
        rs_entname_t* en)
{

        TliCursorT* tcur =
            TliCursorCreate(tcon,
                            rs_entname_getcatalog(en),
                            rs_entname_getschema(en),
                            rs_entname_getname(en));
        return (tcur);
}

/*##**********************************************************************\
 *
 *              TliCursorFree
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void TliCursorFree(TliCursorT* tcur)
{
        uint i;
        tcol_t* tcol;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        su_pa_do_get(tcur->tcur_cols, i, tcol) {
            switch (tcol->tcol_dt) {
                case TCOL_DT_INT:
                case TCOL_DT_LONG:
                case TCOL_DT_SIZET:
                case TCOL_DT_INT4T:
                case TCOL_DT_INT8T:
                case TCOL_DT_FLOAT:
                case TCOL_DT_DOUBLE:
                case TCOL_DT_DFLOAT:
                case TCOL_DT_DATE:
                    break;
                case TCOL_DT_STR:
                    tcol->tcol_.p_str = NULL;
                    break;
                case TCOL_DT_DATA:
                    tcol->tcol_.data.p_ptr = NULL;
                    tcol->tcol_.data.p_len = 0;
                    break;
                case TCOL_DT_VA:
                    tcol->tcol_.p_va = NULL;
                    break;
                case TCOL_DT_AVAL:
                    SsMemFree(tcol->tcol_.p_ta);
                    break;
                case TCOL_DT_UTF8:
                    tcol->tcol_.p_UTF8str = NULL;
                    break;
                default:
                    ss_error;
            }
            if (tcol->tcol_tmpbuf != NULL) {
                SsMemFree(tcol->tcol_tmpbuf);
                tcol->tcol_tmpbuf = NULL;
            }
            SsMemFree(tcol);
        }
        su_pa_done(tcur->tcur_cols);

        tcur->tcur_funblock->cursor_free(tcur->tcur_cd, tcur->tcur_tbcur);

        if (tcur->tcur_free_relh) {
            tb_relh_free(tcur->tcur_cd, tcur->tcur_tbrelh);
        }
        rs_aval_free(tcur->tcur_cd, tcur->tcur_esctype, tcur->tcur_escinst);
        rs_atype_free(tcur->tcur_cd, tcur->tcur_esctype);

        tb_tint_done( tcur->tcur_funblock );

        tcur->tcur_chk = TBCHK_FREED;

        SsMemFree(tcur);
}

void TliCursorSetIsolationTransparent(
        TliCursorT* tcur,
        bool transparent)
{
        ss_dassert(tcur->tcur_tbcur != NULL);
        if (tcur->tcur_tbcur != NULL) {
            /* funblock ??? */
            tb_relcur_setisolation_transparent(NULL, tcur->tcur_tbcur, transparent);
        }        
}


/*##**********************************************************************\
 *
 *              TliCursorSetMaxBlobSize
 *
 * Sets maximum size of blobs that are read in memory during fetching.
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      maxblobsize -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void TliCursorSetMaxBlobSize(
        TliCursorT* tcur,
        long maxblobsize)
{
        CHK_TCON(tcur->tcur_tcon);

        tcur->tcur_maxblobsize = maxblobsize;
}

/*##**********************************************************************\
 *
 *              TliCursorErrorInfo
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      errstr -
 *
 *
 *      errcode -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool TliCursorErrorInfo(
        TliCursorT* tcur,
        char** errstr,
        uint* errcode)
{
        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh == NULL) {
            if (errstr != NULL) {
                *errstr = NULL;
            }
            if (errcode != NULL) {
                *errcode = 0;
            }
            return(FALSE);
        } else {
            if (tcur->tcur_errstr == NULL) {
                rs_error_printinfo(
                    tcur->tcur_cd,
                    tcur->tcur_errh,
                    &tcur->tcur_errcode,
                    &tcur->tcur_errstr);
            }
            if (errstr != NULL) {
                *errstr = tcur->tcur_errstr;
            }
            if (errcode != NULL) {
                *errcode = tcur->tcur_errcode;
            }
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              TliCursorErrorCode
 *
 * Return cursor error code.
 *
 * Parameters :
 *
 *              tcur -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
uint TliCursorErrorCode(
        TliCursorT* tcur)
{
        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh == NULL) {
            return(0);
        } else {
            return(rs_error_geterrcode(tcur->tcur_cd,tcur->tcur_errh));
        }
}

/*##**********************************************************************\
 *
 *              TliCursorCopySuErr
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      p_errh -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool TliCursorCopySuErr(
        TliCursorT* tcur,
        su_err_t** p_target_errh)
{
        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh == NULL) {
            if (tb_trans_isfailed(tcur->tcur_cd, tcur->tcur_trans)) {
                tb_trans_geterrcode(tcur->tcur_cd, tcur->tcur_trans, p_target_errh);
                return(*p_target_errh != NULL);
            }
            return(FALSE);
        } else {
            su_err_copyerrh(p_target_errh, tcur->tcur_errh);
            return(TRUE);
        }
}

/*#***********************************************************************\
 *
 *              tcur_setcol
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      dt -
 *
 *
 *      ptr -
 *
 *
 *      len
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static TliRetT tcur_setcol(
        TliCursorT* tcur,
        char* colname,
        tcol_dt_t dt,
        void* ptr,
        size_t* p_len)
{
        int ano;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);
        ss_dassert(colname != NULL);
        ss_dassert(ptr != NULL);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        if (tcur->tcur_opened) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_CUROPENED);
            return(TLI_ERR_CUROPENED);
        }

        ano = rs_ttype_sql_anobyname(
                tcur->tcur_cd,
                tcur->tcur_relhttype,
                colname);
        if (ano < 0) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_COLNAMEILL);
            return(TLI_ERR_COLNAMEILL);
        }
        return tcur_setcol_ano(tcur, ano, dt, ptr, p_len);
}

/*#***********************************************************************\
 *
 *      tcur_setcol
 *      
 *  Bind column specified by its number.
 *
 * Parameters :
 * 
 *  tcur - cursor object
 *  
 *
 *  ano - in
 *      column number
 *  
 *
 *  dt - in
 *      data type
 *  
 *
 *  ptr - in
 *      pointer to the value pointer buffer
 *  
 *
 *  p_len - in
 *      pointer to the length
 *
 * Return value :
 * 
 * Limitations  :
 *
 * Globals used :
 */
static TliRetT tcur_setcol_ano(
        TliCursorT* tcur,
        int ano,
        tcol_dt_t dt,
        void* ptr,
        size_t* p_len)
{
        tcol_t* tcol;
        tcol = SSMEM_NEW(tcol_t);

        tcol->tcol_ano = ano;
        tcol->tcol_dt = dt;
        tcol->tcol_isnull = FALSE;
        tcol->tcol_tmpbuf = NULL;
        switch (dt) {
            case TCOL_DT_INT:
                tcol->tcol_.p_int = ptr;
                break;
            case TCOL_DT_LONG:
                tcol->tcol_.p_long = ptr;
                break;
            case TCOL_DT_SIZET:
                tcol->tcol_.p_sizet = ptr;
                break;
            case TCOL_DT_INT4T:
                tcol->tcol_.p_int4t = ptr;
                break;
            case TCOL_DT_INT8T:
                tcol->tcol_.p_int8t = ptr;
                break;
            case TCOL_DT_FLOAT:
                tcol->tcol_.p_float = ptr;
                break;
            case TCOL_DT_DOUBLE:
                tcol->tcol_.p_double = ptr;
                break;
            case TCOL_DT_DFLOAT:
                tcol->tcol_.p_dfloat = ptr;
                break;
            case TCOL_DT_STR:
                tcol->tcol_.p_str = ptr;
                break;
            case TCOL_DT_DATE:
                tcol->tcol_.p_date = ptr;
                break;
            case TCOL_DT_DATA:
                tcol->tcol_.data.p_ptr = ptr;
                tcol->tcol_.data.p_len = p_len;
                break;
            case TCOL_DT_VA:
                tcol->tcol_.p_va = ptr;
                break;
            case TCOL_DT_AVAL:
                tcol->tcol_.p_ta = ptr;
                break;
            case TCOL_DT_UTF8:
                tcol->tcol_.p_UTF8str = ptr;
                break;
            default:
                ss_error;
        }

        su_pa_insert(tcur->tcur_cols, tcol);

        return(TLI_RC_SUCC);
}

/*#***********************************************************************\
 *
 *      TliCursorColByNo
 *
 *  Creates a fake binding, it does nothing but inserts columnt by ano to
 *  tval.
 *
 * Parameters :
 *
 *  tcur -
 *      cursor object
 *
 *
 *  ano - in
 *      column number
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorColByNo(
        TliCursorT* tcur,
        int ano)
{
        static char fake_buffer[20];

        ss_dassert(sizeof(fake_buffer) > DT_DATE_DATASIZE);
        return tcur_setcol_ano(tcur, ano, TCOL_DT_INT, fake_buffer, NULL);
}

/*##**********************************************************************\
 *
 *              TliCursorColInt
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      intptr -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorColInt(
        TliCursorT* tcur,
        const char* colname,
        int* intptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_INT, intptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColLong
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      longptr -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorColLong(
        TliCursorT* tcur,
        const char* colname,
        long* longptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_LONG, longptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColSizet
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      longptr -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorColSizet(
        TliCursorT* tcur,
        const char* colname,
        size_t* sizetptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_SIZET, sizetptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColInt4t
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      sizetptr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColInt4t(
        TliCursorT* tcur,
        const char* colname,
        ss_int4_t* int4tptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_INT4T, int4tptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColInt8t
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      sizetptr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColInt8t(
        TliCursorT* tcur,
        const char* colname,
        ss_int8_t* int8tptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_INT8T, int8tptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColFloat
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      floatptr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColFloat(
        TliCursorT* tcur,
        const char* colname,
        float* floatptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_FLOAT, floatptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColDouble
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      doubleptr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColDouble(
        TliCursorT* tcur,
        const char* colname,
        double* doubleptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_DOUBLE, doubleptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColDfloat
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      dfloatptr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColDfloat(
        TliCursorT* tcur,
        const char* colname,
        dt_dfl_t* dfloatptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_DFLOAT, dfloatptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColUTF8
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      strptr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColUTF8(
        TliCursorT* tcur,
        const char* colname,
        ss_char1_t** strptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_UTF8, strptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorConstrUTF8
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      UTF8strval -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorConstrUTF8(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        char* UTF8strval)
{
        TliRetT rc;
        rs_atype_t* atype;
        rs_aval_t* aval;
        bool succp;

        switch (relop) {
            case TLI_RELOP_ISNULL:
            case TLI_RELOP_ISNOTNULL:
                if (UTF8strval == NULL) {
                    UTF8strval = (char *)"";
                }
                break;
            case TLI_RELOP_EQUAL_OR_ISNULL:
                if (UTF8strval == NULL) {
                    relop = TLI_RELOP_ISNULL;
                    UTF8strval = (char *)"";
                } else {
                    relop = TLI_RELOP_EQUAL;
                }
                break;
            default:
                break;
        }

        atype = rs_atype_initbysqldt(tcur->tcur_cd, RSSQLDT_WVARCHAR, -1L, -1L);
        aval = rs_aval_create(tcur->tcur_cd, atype);
        succp = rs_aval_setUTF8str_raw(
                    tcur->tcur_cd,
                    atype, aval,
                    UTF8strval,
                    NULL);

        if (succp) {
            rc = tcur_setconstr(tcur, (char *)colname, relop, atype, aval);
        } else {
            rc = TLI_ERR_CONSTRILL;
        }

        rs_aval_free(tcur->tcur_cd, atype, aval);
        rs_atype_free(tcur->tcur_cd, atype);

        return(rc);
}

/*##**********************************************************************\
 *
 *              TliCursorColDate
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      dateptr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColDate(
        TliCursorT* tcur,
        const char* colname,
        dt_date_t* dateptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_DATE, dateptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColData
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      dataptr -
 *
 *
 *      lenptr -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColData(
        TliCursorT* tcur,
        const char* colname,
        char** dataptr,
        size_t* lenptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_DATA, dataptr, lenptr));
}

/*##**********************************************************************\
 *
 *              TliCursorColVa
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      vaptr -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorColVa(
        TliCursorT* tcur,
        const char* colname,
        va_t** vaptr)
{
        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_VA, vaptr, NULL));
}

/*##**********************************************************************\
 *
 *              TliCursorColAval
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      atype - in, hold
 *
 *
 *      aval - in, hold
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorColAval(
        TliCursorT* tcur,
        const char* colname,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        tcol_aval_t* ta;

        ta = SSMEM_NEW(tcol_aval_t);

        ta->ta_atype = atype;
        ta->ta_aval = aval;

        return(tcur_setcol(tcur, (char *)colname, TCOL_DT_AVAL, ta, NULL));
}

/*#***********************************************************************\
 *
 *              tcur_handleNULL
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      nullop -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static TliRetT tcur_handleNULL(
        TliCursorT* tcur,
        const char* colname,
        tcol_nullop_t nullop)
{
        int ano;
        int i;
        tcol_t* tcol;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);
        ss_dassert(colname != NULL);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        ano = rs_ttype_sql_anobyname(
                tcur->tcur_cd,
                tcur->tcur_relhttype,
                (char *)colname);
        if (ano < 0) {
            switch (nullop) {
                case TCOL_NULLOP_IS:
                case TCOL_NULLOP_SET:
                case TCOL_NULLOP_CLEAR:
                    rs_error_create(&tcur->tcur_errh,
                                    E_ATTRNOTEXISTONREL_SS,
                                    (char *)colname,
                                    rs_relh_name(tcur->tcur_cd,
                                                 tcur->tcur_relh));
                    return(TLI_ERR_COLNAMEILL);
                default:
                    ss_error;
            }
        }

        su_pa_do_get(tcur->tcur_cols, i, tcol) {
            if (tcol->tcol_ano == ano) {
                switch (nullop) {
                    case TCOL_NULLOP_IS:
                        return(tcol->tcol_isnull?
                               TLI_RC_NULL_YES : TLI_RC_NULL_NO);
                    case TCOL_NULLOP_SET:
                        tcol->tcol_isnull = TRUE;
                        return(TLI_RC_SUCC);
                    case TCOL_NULLOP_CLEAR:
                        tcol->tcol_isnull = FALSE;
                        return(TLI_RC_SUCC);
                    default:
                        ss_error;
                }
            }
        }

        if (nullop == TCOL_NULLOP_IS) {
            return(TLI_RC_NULL_NO);
        } else {
            return(TLI_RC_COLNOTBOUND);
        }
}

/*##**********************************************************************\
 *
 *              TliCursorColIsNULL
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
int TliCursorColIsNULL(
        TliCursorT* tcur,
        const char* colname)
{
        TliRetT tlirc = tcur_handleNULL(tcur, (char *)colname, TCOL_NULLOP_IS);
        switch (tlirc) {
            case TLI_RC_NULL_YES:
                return (TRUE);
            default:
                ss_rc_derror(tlirc);
            case TLI_RC_NULL_NO:
                return (FALSE);
        }
}


/*##**********************************************************************\
 *
 *              TliCursorColSetNULL
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorColSetNULL(
        TliCursorT* tcur,
        const char* colname)
{
        return(tcur_handleNULL(tcur, (char *)colname, TCOL_NULLOP_SET));
}

/*##**********************************************************************\
 *
 *              TliCursorColClearNULL
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorColClearNULL(
        TliCursorT* tcur,
        const char* colname)
{
        return(tcur_handleNULL(tcur, (char *)colname, TCOL_NULLOP_CLEAR));
}


static TliRetT tcur_orderby(
        TliCursorT* tcur,
        char* colname,
        bool isascending)
{
        int ano;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);
        ss_dassert(colname != NULL);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        if (tcur->tcur_opened) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_CUROPENED);
            return(TLI_ERR_CUROPENED);
        }

        ano = rs_ttype_sql_anobyname(
                tcur->tcur_cd,
                tcur->tcur_relhttype,
                colname);
        if (ano < 0) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_COLNAMEILL);
            return(TLI_ERR_COLNAMEILL);
        }

        tcur->tcur_funblock->cursor_orderby(
            tcur->tcur_cd,
            tcur->tcur_tbcur,
            ano,
            isascending);

        tcur->tcur_norderby++;

        return(TLI_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              TliCursorOrderby
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorOrderby(
        TliCursorT* tcur,
        const char* colname)
{
        return( tcur_orderby(
            tcur,
            (char *)colname,
            TRUE));
}

/*##**********************************************************************\
 *
 *              TliCursorDescendingOrderby
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorDescendingOrderby(
        TliCursorT* tcur,
        const char* colname)
{
        return( tcur_orderby(
            tcur,
            (char *)colname,
            FALSE));
}

/*#***********************************************************************\
 *
 *              tcur_setconstr
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      atype -
 *
 *
 *      aval -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static TliRetT tcur_setconstr(
        TliCursorT* tcur,
        char* colname,
        TliRelopT relop,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        int ano;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);
        ss_dassert(colname != NULL);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        if (tcur->tcur_opened) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_CUROPENED);
            return(TLI_ERR_CUROPENED);
        }

        ano = rs_ttype_sql_anobyname(
                tcur->tcur_cd,
                tcur->tcur_relhttype,
                colname);
        if (ano < 0) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_COLNAMEILL);
            return(TLI_ERR_COLNAMEILL);
        }

        tcur->tcur_funblock->cursor_constr(
            tcur->tcur_cd,
            tcur->tcur_tbcur,
            ano,
            relop,
            atype,
            aval,
            tcur->tcur_esctype,
            tcur->tcur_escinst);

        return(TLI_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              TliCursorConstrInt8t
 *
 * Sets a 64-bit integer constraint for a column. (Will later use
 * BIGINT data type when it comes available, now converts the value
 * to 32-bit integer and fails if the value does not fit int 32 bits)
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      int8val -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorConstrInt8t(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        ss_int8_t int8val)
{
        TliRetT rc;
        rs_atype_t* atype;
        rs_aval_t* aval;

        atype = rs_atype_initbysqldt(tcur->tcur_cd, RSSQLDT_BIGINT, -1, -1);
        aval = rs_aval_create(tcur->tcur_cd, atype);
        {
            ss_debug(bool succp =)
            rs_aval_setint8_ext(tcur->tcur_cd, atype, aval, int8val, NULL);
            ss_dassert(succp);
        }
        rc = tcur_setconstr(tcur, (char *)colname, relop, atype, aval);
        rs_aval_free(tcur->tcur_cd, atype, aval);
        rs_atype_free(tcur->tcur_cd, atype);
        return(rc);
}

/*##**********************************************************************\
 *
 *              TliCursorConstrInt
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      intval -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorConstrInt(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        int intval)
{
        return(TliCursorConstrLong(tcur, (char *)colname, relop, (long)intval));
}

/*##**********************************************************************\
 *
 *              TliCursorConstrLong
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      longval -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorConstrLong(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        long longval)
{
        TliRetT rc;
        rs_atype_t* atype;
        rs_aval_t* aval;

        ss_dassert(SS_INT4_MIN <= longval && longval <= SS_INT4_MAX);
        atype = rs_atype_initlong(tcur->tcur_cd);
        aval = rs_aval_create(tcur->tcur_cd, atype);
        {
            ss_debug(bool succp =)
            rs_aval_setlong_ext(tcur->tcur_cd,
                                atype, aval,
                                (ss_int4_t)longval, NULL);
            ss_dassert(succp);
        }

        rc = tcur_setconstr(tcur, (char *)colname, relop, atype, aval);

        rs_aval_free(tcur->tcur_cd, atype, aval);
        rs_atype_free(tcur->tcur_cd, atype);

        return(rc);
}

#if 0 /* not used, removed by Pete 1997-02-19 */

/*##**********************************************************************\
 *
 *              TliCursorConstrDate
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      dateval -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorConstrDate(
        TliCursorT* tcur,
        char* colname,
        TliRelopT relop,
        dt_date_t* dateval)
{
        TliRetT rc;
        rs_atype_t* atype;
        rs_aval_t* aval;
        bool succp;

        ss_dassert(dateval != NULL);

        atype = rs_atype_initdate(tcur->tcur_cd);
        aval = rs_aval_create(tcur->tcur_cd, atype);
        succp = rs_aval_setdate(tcur->tcur_cd, atype, aval, dateval);

        if (succp) {
            rc = tcur_setconstr(tcur, colname, relop, atype, aval);
        } else {
            rc = TLI_ERR_CONSTRILL;
        }

        rs_aval_free(tcur->tcur_cd, atype, aval);
        rs_atype_free(tcur->tcur_cd, atype);

        return(rc);
}
#endif /* 0 */

/*##**********************************************************************\
 *
 *              TliCursorConstrData
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      dataval -
 *
 *
 *      datalen -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorConstrData(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        char* dataval,
        size_t datalen)
{
        TliRetT rc;
        rs_atype_t* atype;
        rs_aval_t* aval;
        bool succp;

        ss_dassert(dataval != NULL);

        atype = rs_atype_initbinary(tcur->tcur_cd);
        aval = rs_aval_create(tcur->tcur_cd, atype);
        succp = rs_aval_setbdata_ext(
                    tcur->tcur_cd, atype, aval, dataval, datalen, NULL);

        if (succp) {
            rc = tcur_setconstr(tcur, (char *)colname, relop, atype, aval);
        } else {
            rc = TLI_ERR_CONSTRILL;
        }

        rs_aval_free(tcur->tcur_cd, atype, aval);
        rs_atype_free(tcur->tcur_cd, atype);

        return(rc);
}

/*##**********************************************************************\
 *
 *              TliCursorConstrVa
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      vaval -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorConstrVa(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        va_t* vaval)
{
        TliRetT rc;
        rs_atype_t* atype;
        rs_aval_t* aval;

        ss_dassert(vaval != NULL);

        atype = rs_atype_initbinary(tcur->tcur_cd);
        aval = rs_aval_create(tcur->tcur_cd, atype);
        rs_aval_setva(tcur->tcur_cd, atype, aval, vaval);

        rc = tcur_setconstr(tcur, (char *)colname, relop, atype, aval);

        rs_aval_free(tcur->tcur_cd, atype, aval);
        rs_atype_free(tcur->tcur_cd, atype);

        return(rc);
}

/*##**********************************************************************\
 *
 *              TliCursorConstrAval
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      colname -
 *
 *
 *      relop -
 *
 *
 *      atype - in, use
 *
 *
 *      aval - in, use
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
TliRetT TliCursorConstrAval(
        TliCursorT* tcur,
        const char* colname,
        TliRelopT relop,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        TliRetT rc;

        rc = tcur_setconstr(tcur, (char *)colname, relop, atype, aval);

        return(rc);
}

/*##**********************************************************************\
 *
 *      TliCursorConstrIsNull
 *
 *
 * 
 * Parameters :
 *  
 *  tcur -
 *
 *  
 *  ano -
 *
 * Return value :
 * 
 * Comments :
 * 
 * Globals used :
 * 
 * See also :
 */
TliRetT TliCursorConstrIsNull(
        TliCursorT* tcur,
        rs_atype_t* atype,
        char *colname)
{
        TliRetT rc;
        rs_aval_t* aval = rs_aval_create(tcur->tcur_cd, atype);

        rs_aval_setnull(tcur->tcur_cd, atype, aval);
        rc = tcur_setconstr(tcur, colname, TLI_RELOP_ISNULL, atype, aval);
        rs_aval_free(tcur->tcur_cd, atype, aval);
        return(rc);
}

/*##**********************************************************************\
 *
 *              TliCursorOpen
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorOpen(TliCursorT* tcur)
{
        int i;
        int j;
        tcol_t* tcol;
        int* selattrs;
        bool succp;
        uint nullcoll;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        if (tcur->tcur_opened) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_CUROPENED);
            return(TLI_ERR_CUROPENED);
        }

        selattrs = SsMemAlloc(
                       sizeof(selattrs[0]) * (su_pa_nelems(tcur->tcur_cols) + 1));
        j = 0;
        su_pa_do_get(tcur->tcur_cols, i, tcol) {
            selattrs[j++] = tcol->tcol_ano;
        }
        selattrs[j] = -1;

        tcur->tcur_funblock->cursor_project(
            tcur->tcur_cd,
            tcur->tcur_tbcur,
            selattrs);

        SsMemFree(selattrs);

        succp = tcur->tcur_funblock->cursor_endofconstr(
                    tcur->tcur_cd,
                    tcur->tcur_tbcur,
                    &tcur->tcur_errh);
        if (!succp) {
            return(TLI_ERR_CONSTRILL);
        }

        if ((int)tcur->tcur_funblock->cursor_ordered(tcur->tcur_cd, tcur->tcur_tbcur, &nullcoll) !=
            tcur->tcur_norderby) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_ORDERBYILL);
            return(TLI_ERR_ORDERBYILL);
        }

        tb_trans_beginif(tcur->tcur_cd, tcur->tcur_trans);

        tcur->tcur_funblock->cursor_open(tcur->tcur_cd, tcur->tcur_tbcur);

        tcur->tcur_ttype = tcur->tcur_funblock->cursor_ttype(tcur->tcur_cd, tcur->tcur_tbcur);
        tcur->tcur_opened = TRUE;

        return(TLI_RC_SUCC);
}


/*#***********************************************************************\
 *
 *              TliCursorNextTval
 *
 *  Fetches the next tval from open cursor.
 *
 * Parameters :
 *
 *      tcur - the cursor
 *
 *
 * Return value :
 *  tval value or NULL in case of error or end of table
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_tval_t* TliCursorNextTval (TliCursorT *tcur)
{
        rs_tval_t* tval;
        uint finished;
        tcol_t* tcol __attribute__ ((unused));
        rs_sysinfo_t* cd;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        if (!tcur->tcur_opened) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_CURNOTOPENED);
            return NULL;
        }

        tb_trans_beginif(tcur->tcur_cd, tcur->tcur_trans);

        cd = tcur->tcur_cd;

        do {
                tval = tcur->tcur_funblock->cursor_next(cd, tcur->tcur_tbcur, &finished, &tcur->tcur_errh);
        } while (finished == TB_FETCH_CONT);

        if (finished == TB_FETCH_ERROR) {
            ss_dassert(tcur->tcur_errh != NULL);
            if (tcur->tcur_failonlyincommit) {
                tcur_settransfailed(tcur);
            }
            return NULL;
        }
        return tval;
}

/*#***********************************************************************\
 *
 *              CursorNextOrPrev
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      nextp -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static TliRetT CursorNextOrPrev(TliCursorT* tcur, bool nextp)
{
        rs_tval_t* tval;
        uint finished;
        int i;
        tcol_t* tcol;
        rs_sysinfo_t* cd;
        su_ret_t rc;
        bool succp;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        if (!tcur->tcur_opened) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_CURNOTOPENED);
            return(TLI_ERR_CURNOTOPENED);
        }

        tb_trans_beginif(tcur->tcur_cd, tcur->tcur_trans);

        cd = tcur->tcur_cd;

        do {
            if (nextp) {
                tval = tcur->tcur_funblock->cursor_next(cd, tcur->tcur_tbcur, &finished, &tcur->tcur_errh);
            } else {
                tval = tcur->tcur_funblock->cursor_prev(cd, tcur->tcur_tbcur, &finished, &tcur->tcur_errh);
            }
        } while (finished == TB_FETCH_CONT);

        if (finished == TB_FETCH_ERROR) {
            ss_dassert(tcur->tcur_errh != NULL);
            if (tcur->tcur_failonlyincommit) {
                tcur_settransfailed(tcur);
                return(TLI_RC_END);
            } else {
                return(TLI_ERR_FAILED);
            }
        }

        if (tval == NULL) {
            return(TLI_RC_END);
        }
        rc = tb_blobg2_readsmallblobstotval(
                cd,
                tcur->tcur_ttype,
                tval,
                tcur->tcur_maxblobsize);
        ss_dassert(rc == SU_SUCCESS);

        su_pa_do_get(tcur->tcur_cols, i, tcol) {
            rs_atype_t* atype;
            rs_aval_t* aval;

            atype = rs_ttype_sql_atype(
                        cd,
                        tcur->tcur_ttype,
                        tcol->tcol_ano);
            aval = rs_tval_sql_aval(
                        cd,
                        tcur->tcur_ttype,
                        tval,
                        tcol->tcol_ano);

            tcol->tcol_isnull = rs_aval_isnull(cd, atype, aval);

            switch (tcol->tcol_dt) {
                case TCOL_DT_INT:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_int = 0;
                    } else {
                        *tcol->tcol_.p_int = (int)rs_aval_getlong(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_LONG:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_long = 0;
                    } else {
                        *tcol->tcol_.p_long = rs_aval_getlong(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_SIZET:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_sizet = 0;
                    } else {
                        *tcol->tcol_.p_sizet = (size_t)rs_aval_getlong(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_INT4T:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_int4t = 0;
                    } else {
                        *tcol->tcol_.p_int4t = (ss_int4_t)rs_aval_getlong(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_INT8T:
                    if (tcol->tcol_isnull) {
                        SsInt8Set0(tcol->tcol_.p_int8t);
                    } else {
                        *tcol->tcol_.p_int8t =
                            rs_aval_getint8(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_FLOAT:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_float = (float)0.0;
                    } else {
                        *tcol->tcol_.p_float = rs_aval_getfloat(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_DOUBLE:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_double = (double)0.0;
                    } else {
                        *tcol->tcol_.p_double = rs_aval_getdouble(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_DFLOAT:
                    if (!tcol->tcol_isnull) {
                        *tcol->tcol_.p_dfloat = rs_aval_getdfloat(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_STR:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_str = NULL;
                    } else {
                        *tcol->tcol_.p_str = rs_aval_getasciiz(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_UTF8:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_UTF8str = NULL;
                    } else {
                        ss_debug(RS_AVALRET_T retc;)
                        size_t totalsize;
                        size_t required_bufsize =
                            rs_aval_requiredUTF8bufsize(cd, atype, aval);
                        ss_dassert(required_bufsize != 0);
                        if (tcol->tcol_tmpbuf != NULL) {
                            tcol->tcol_tmpbuf =
                                SsMemRealloc(
                                    tcol->tcol_tmpbuf,
                                    required_bufsize);
                        } else {
                            tcol->tcol_tmpbuf =
                                SsMemAlloc(required_bufsize);
                        }
                        ss_debug(retc = )
                            rs_aval_converttoUTF8(
                                    cd,
                                    atype, aval,
                                    tcol->tcol_tmpbuf, required_bufsize,
                                    0,
                                    &totalsize,
                                    NULL);
                        ss_rc_dassert(retc == RSAVR_SUCCESS, retc);
                        *tcol->tcol_.p_UTF8str = tcol->tcol_tmpbuf;
                    }
                    break;

                case TCOL_DT_DATE:
                    if (!tcol->tcol_isnull) {
                        memcpy(
                            tcol->tcol_.p_date,
                            rs_aval_getdate(cd, atype, aval),
                            DT_DATE_DATASIZE);
                    }
                    break;
                case TCOL_DT_DATA:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.data.p_ptr = NULL;
                        *tcol->tcol_.data.p_len = 0;
                    } else {
                        ulong len;
                        *tcol->tcol_.data.p_ptr = rs_aval_getdata(
                                                        cd,
                                                        atype,
                                                        aval,
                                                        &len);
                        *tcol->tcol_.data.p_len = (size_t)len;
                    }
                    break;
                case TCOL_DT_VA:
                    if (tcol->tcol_isnull) {
                        *tcol->tcol_.p_va = NULL;
                    } else {
                        *tcol->tcol_.p_va = rs_aval_va(cd, atype, aval);
                    }
                    break;
                case TCOL_DT_AVAL:
                    {
                        RS_AVALRET_T aret;
                        aret = rs_aval_assign_ext(
                                    cd,
                                    tcol->tcol_.p_ta->ta_atype,
                                    tcol->tcol_.p_ta->ta_aval,
                                    atype,
                                    aval,
                                    &tcur->tcur_errh);
                        succp = (aret == RSAVR_SUCCESS);
                    }
                    if (!succp) {
                        ss_dassert(tcur->tcur_errh != NULL);
                        return(TLI_ERR_FAILED);
                    }
                    break;
                default:
                    ss_error;
            }
        }
        return(TLI_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              TliCursorNext
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorNext(TliCursorT* tcur)
{
        return(CursorNextOrPrev(tcur, TRUE));
}

/*##**********************************************************************\
 *
 *              TliCursorPrev
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorPrev(TliCursorT* tcur)
{
        return(CursorNextOrPrev(tcur, FALSE));
}

/*#***********************************************************************\
 *
 *              CursorInsOrUpd
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 *      insertp -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static TliRetT CursorInsOrUpd(TliCursorT* tcur, bool insertp)
{
        bool succp;
        int retcode;
        int i;
        tcol_t* tcol;
        rs_ttype_t* ttype;
        rs_tval_t* tval;
        rs_atype_t* atype;
        rs_aval_t* aval;
        rs_sysinfo_t* cd;
        TliRetT rc = TLI_RC_SUCC;
        bool* selflags;
        long tmplong;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        cd = tcur->tcur_cd;
        ttype = tcur->tcur_relhttype;

        selflags = SsMemCalloc(sizeof(selflags[0]), rs_ttype_sql_nattrs(cd, ttype));

        tval = rs_tval_create(cd, ttype);
        su_pa_do_get(tcur->tcur_cols, i, tcol) {
            atype = rs_ttype_sql_atype(cd, ttype, tcol->tcol_ano);
            aval = rs_tval_sql_aval(cd, ttype, tval, tcol->tcol_ano);
            succp = TRUE;
            if (tcol->tcol_isnull) {
                rs_aval_setnull(cd, atype, aval);
            } else {
                switch (tcol->tcol_dt) {
                    case TCOL_DT_INT:
                        succp = rs_aval_setlong_ext(cd, atype, aval, (long)*tcol->tcol_.p_int, NULL);
                        break;
                    case TCOL_DT_LONG:
                        succp = rs_aval_setlong_ext(cd, atype, aval, *tcol->tcol_.p_long, NULL);
                        break;
                    case TCOL_DT_INT4T:
                        tmplong = (long)(*tcol->tcol_.p_int4t);
                        succp = rs_aval_setlong_ext(cd, atype, aval, tmplong, NULL);
                        break;
                    case TCOL_DT_INT8T:
                        succp = rs_aval_setint8_ext(
                                cd, atype, aval, *tcol->tcol_.p_int8t, NULL);
                        break;
                    case TCOL_DT_SIZET:
                        tmplong = (long)(*tcol->tcol_.p_sizet);
                        succp = rs_aval_setlong_ext(cd, atype, aval, tmplong, NULL);
                        break;
                    case TCOL_DT_FLOAT:
                        succp = rs_aval_setfloat_ext(cd, atype, aval, *tcol->tcol_.p_float, NULL);
                        break;
                    case TCOL_DT_DOUBLE:
                        succp = rs_aval_setdouble_ext(cd, atype, aval, *tcol->tcol_.p_double, NULL);
                        break;
                    case TCOL_DT_DFLOAT:
                        succp = rs_aval_setdfloat_ext(cd, atype, aval, *tcol->tcol_.p_dfloat, NULL);
                        break;
                    case TCOL_DT_STR:
                        if (*tcol->tcol_.p_str == NULL) {
                            rs_aval_setnull(cd, atype, aval);
                        } else {
                            succp = rs_aval_set8bitstr_ext(
                                        cd,
                                        atype,
                                        aval,
                                        *tcol->tcol_.p_str,
                                        NULL);
                        }
                        break;
                    case TCOL_DT_UTF8:
                        if (*tcol->tcol_.p_UTF8str == NULL) {
                            rs_aval_setnull(cd, atype, aval);
                        } else {
                            succp = rs_aval_setUTF8str_ext(
                                        cd,
                                        atype,
                                        aval,
                                        *tcol->tcol_.p_UTF8str,
                                        NULL);
                        }
                        break;
                    case TCOL_DT_DATE:
                        succp = rs_aval_setdate_ext(
                                    cd,
                                    atype,
                                    aval,
                                    tcol->tcol_.p_date,
                                    DT_DATE_SQLTYPE_UNKNOWN,
                                    NULL);
                        break;
                    case TCOL_DT_DATA:
                        if (*tcol->tcol_.data.p_ptr == NULL) {
                            rs_aval_setnull(cd, atype, aval);
                        } else {
                            succp = rs_aval_setbdata_ext(
                                        cd,
                                        atype,
                                        aval,
                                        *tcol->tcol_.data.p_ptr,
                                        *tcol->tcol_.data.p_len,
                                        NULL);
                        }
                        break;
                    case TCOL_DT_VA:
                        if (*tcol->tcol_.p_va == NULL) {
                            rs_aval_setnull(cd, atype, aval);
                        } else {
                            rs_aval_setva(cd, atype, aval, *tcol->tcol_.p_va);
                        }
                        break;
                    case TCOL_DT_AVAL:
                        {
                            RS_AVALRET_T aret;
                            aret = rs_aval_assign_ext(
                                        cd,
                                        atype,
                                        aval,
                                        tcol->tcol_.p_ta->ta_atype,
                                        tcol->tcol_.p_ta->ta_aval,
                                        NULL);
                            succp = (aret == RSAVR_SUCCESS);
                        }
                        break;
                    default:
                        ss_error;
                }
            }
            if (!succp) {
                rs_tval_free(cd, ttype, tval);
                SsMemFree(selflags);
                rs_error_create(&tcur->tcur_errh, TLI_ERR_TYPECONVILL);
                return(TLI_ERR_FAILED);
            }
            selflags[tcol->tcol_ano] = TRUE;
        }

        tb_trans_beginif(tcur->tcur_cd, tcur->tcur_trans);

        if (tcur->tcur_tcon->tcon_localtrans) {
            tb_trans_stmt_begin(tcur->tcur_cd, tcur->tcur_trans);
        }

        tb_trans_settransoption(
            tcur->tcur_cd,
            tcur->tcur_trans,
            TB_TRANSOPT_HSBFLUSH_NO);

        tb_trans_setdelayedstmterror(
            tcur->tcur_cd,
            tcur->tcur_trans);
        do {
            if (insertp) {
                retcode = tb_relh_insert(
                            cd,
                            tcur->tcur_trans,
                            tcur->tcur_tbrelh,
                            ttype,
                            tval,
                            selflags,
                            &tcur->tcur_errh);
            } else {
                retcode = tcur->tcur_funblock->cursor_update(
                            cd,
                            tcur->tcur_tbcur,
                            tval,
                            selflags,
                            NULL,
                            0,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            &tcur->tcur_errh);
            }
        } while (retcode == TB_CHANGE_CONT);

        tb_trans_settransoption(
            tcur->tcur_cd,
            tcur->tcur_trans,
            TB_TRANSOPT_HSBFLUSH_YES);

        SsMemFree(selflags);

        rs_tval_free(cd, ttype, tval);

        if (retcode == TB_CHANGE_SUCC) {
            if (tcur->tcur_tcon->tcon_localtrans) {
                rc = TliCommitStmt(tcur->tcur_tcon);
                if (rc != TLI_RC_SUCC) {
                    bool succp;
                    ss_rc_dassert(rc == TLI_ERR_FAILED, rc);
                    succp = TliConnectCopySuErr(tcur->tcur_tcon, &tcur->tcur_errh);
                    ss_dassert(succp);
                }
            }
            return (rc);
        } else {
            if (tcur->tcur_tcon->tcon_localtrans) {
                TliRollbackStmt(tcur->tcur_tcon);
            }
            if (tcur->tcur_failonlyincommit) {
                tcur_settransfailed(tcur);
                return(TLI_RC_SUCC);
            } else if (tb_trans_isfailed(tcur->tcur_cd, tcur->tcur_trans)) {
                /* Transaction already failed so we can return success and
                 * insert or update will fail anyway.
                 */
                return(TLI_RC_SUCC);
            } else {
                return(TLI_ERR_FAILED);
            }
        }
}

/*##**********************************************************************\
 *
 *              TliCursorInsert
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorInsert(TliCursorT* tcur)
{
        return(CursorInsOrUpd(tcur, TRUE));
}

/*##**********************************************************************\
 *
 *              TliCursorDelete
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorDelete(TliCursorT* tcur)
{
        uint retcode;

        CHK_TCUR(tcur);
        CHK_TCON(tcur->tcur_tcon);

        if (tcur->tcur_errh != NULL) {
            tcur_errorfree(tcur);
        }

        if (!tcur->tcur_opened) {
            rs_error_create(&tcur->tcur_errh, TLI_ERR_CURNOTOPENED);
            return(TLI_ERR_CURNOTOPENED);
        }

        tb_trans_beginif(tcur->tcur_cd, tcur->tcur_trans);

        tb_trans_settransoption(
            tcur->tcur_cd,
            tcur->tcur_trans,
            TB_TRANSOPT_HSBFLUSH_NO);

        tb_trans_setdelayedstmterror(
            tcur->tcur_cd,
            tcur->tcur_trans);

        do {
            retcode = tcur->tcur_funblock->cursor_delete(tcur->tcur_cd, tcur->tcur_tbcur, &tcur->tcur_errh);
        } while (retcode == TB_CHANGE_CONT);

        tb_trans_settransoption(
            tcur->tcur_cd,
            tcur->tcur_trans,
            TB_TRANSOPT_HSBFLUSH_YES);

        if (retcode == TB_CHANGE_SUCC) {
            if (tcur->tcur_tcon->tcon_localtrans) {
                TliRetT tlirc = TliCommitStmt(tcur->tcur_tcon);
                if (tlirc != TLI_RC_SUCC) {
                    TliConnectCopySuErr(tcur->tcur_tcon, &tcur->tcur_errh);
                }
                return (tlirc);
            } else {
                return (TLI_RC_SUCC);
            }
        } else {
            ss_dassert(tcur->tcur_errh != NULL);
            if (tcur->tcur_tcon->tcon_localtrans) {
                TliRollbackStmt(tcur->tcur_tcon);
            }
            if (tcur->tcur_failonlyincommit) {
                tcur_settransfailed(tcur);
                return(TLI_RC_SUCC);
            } else if (tb_trans_isfailed(tcur->tcur_cd, tcur->tcur_trans)) {
                /* Transaction already failed so we can return success and
                 * delete will fail anyway.
                 */
                return(TLI_RC_SUCC);
            } else {
                return(TLI_ERR_FAILED);
            }
        }
}

/*##**********************************************************************\
 *
 *              TliCursorUpdate
 *
 *
 *
 * Parameters :
 *
 *      tcur -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
TliRetT TliCursorUpdate(TliCursorT* tcur)
{
        return(CursorInsOrUpd(tcur, FALSE));
}

/*##**********************************************************************\
 *
 *              TliGetCollation
 *
 *
 *
 * Parameters :
 *
 *      tcon -
 *
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_chcollation_t TliGetCollation(TliConnectT* tcon)
{
        su_chcollation_t chcollation;
#ifdef COLLATION_UPDATE
        chcollation = tb_connect_getcollation(tcon->tcon_tbcon);
#else
        chcollation = SU_CHCOLLATION_FIN;
#endif
        return (chcollation);
}
