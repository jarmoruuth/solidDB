/*************************************************************************\
**  source       * tab0sysproperties.c
**  directory    * tab
**  description  * Persistent system properties
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

This module implements persistent system properties. The system properties
are simple string key (globally unique property identifier) to a
string value mappings. The system properties are automatically made
persistent at each checkpoint and they will contain the latest value after
the recovery is complete.

System properties will not automatically update the records in
recovery, this must be done explicitly. All the explicit updates will
be stored in the first checkpoint (which is done immediately after
recovery).

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

The user of this module must guarantee that the values are not updated
during checkpoint if you want consistent results.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */


#include <ssc.h>
#include <ssdebug.h>
#include <sssprint.h>
#include <ssscan.h>

#ifdef SS_HSBG2

#include <dt0date.h>

#include <su0error.h>

#include <rs0types.h>
#include <rs0sdefs.h>
#include <rs0auth.h>
#include <rs0sysi.h>
#include <rs0cons.h>

#include <dbe0type.h>
#include <dbe0db.h>

#include "tab1dd.h"
#include "tab1defs.h"
#include "tab0tran.h"
#include "tab0tli.h"

#define PROP_HSBSTATE   "HSB_STATE"

typedef enum {
        PR_TYPE_STRING,
        PR_TYPE_HSBLPID
} tb_sysproperty_type_t;


typedef struct {
        ss_debug(tb_check_t             pr_check;)
        tb_sysproperty_type_t           pr_type;
        char *                          pr_key;
        char *                          pr_value;
        bool                            pr_dirty;
        tb_sysproperties_callback_fun_t pr_callback_fun;
        void *                          pr_callback_ctx;
} tb_sysproperty_t;

struct tb_sysproperties_st {
        ss_debug(tb_check_t sp_check;)
        su_list_t*          sp_data; /* yes, there are better data structures
                                        to handle this kind of data --
                                        for example hash. the problem is that
                                        our hash implementations does not
                                        support iterating through the
                                        nodes, which is needed when
                                        we are storing data. furthermore,
                                        there is currently only a few
                                        properties so list works just fine */
        bool                sp_initialized;
        SsSemT*             sp_mutex;
        tb_database_t*      sp_tdb;
};

#define CHK_SYSPROPERTY(x) \
    ss_dassert(SS_CHKPTR(x) && (pr)->pr_check == TBCHK_SYSPROPERTY)

#define CHK_SYSPROPERTIES(x) \
    ss_dassert(SS_CHKPTR(x) && (sp)->sp_check == TBCHK_SYSPROPERTIES)

static void tb_sysproperty_free_value(
        tb_sysproperty_t* pr);

static void tb_sysproperty_set_value(
        tb_sysproperty_t* pr,
        tb_sysproperty_type_t pr_type,
        char* value);


static tb_sysproperty_t* tb_sysproperty_init(
        tb_sysproperty_type_t pr_type,
        char *key,
        char *value,
        tb_sysproperties_callback_fun_t callback_fun,
        void *callback_ctx,
        bool dirty)
{
        tb_sysproperty_t* pr;

        ss_dassert(key != NULL);
        ss_dassert(value == NULL || (callback_fun == NULL && callback_ctx == NULL));

        pr = SSMEM_NEW(tb_sysproperty_t);
        ss_debug(pr->pr_check = TBCHK_SYSPROPERTY;)
        pr->pr_type = pr_type;
        pr->pr_key = SsMemStrdup(key);
        pr->pr_value = NULL;
        pr->pr_dirty = dirty;
        pr->pr_callback_fun = callback_fun;
        pr->pr_callback_ctx = callback_ctx;

        tb_sysproperty_set_value(pr, pr_type, value);

        CHK_SYSPROPERTY(pr);

        return (pr);
}

static void tb_sysproperty_done(tb_sysproperty_t* pr)
{
        CHK_SYSPROPERTY(pr);
        SsMemFree(pr->pr_key);

        tb_sysproperty_free_value(pr);

        SsMemFree(pr);
}

static void tb_sysproperty_free_value(
        tb_sysproperty_t* pr)
{
        CHK_SYSPROPERTY(pr);
        switch (pr->pr_type) {
            case PR_TYPE_STRING:
                if(pr->pr_value != NULL) {
                    SsMemFree(pr->pr_value);
                    pr->pr_value = NULL;
                }
                break;
            case PR_TYPE_HSBLPID:
                if(pr->pr_value != NULL) {
                    SsMemFree(pr->pr_value);
                    pr->pr_value = NULL;
                }
                break;
            default:
                ss_rc_derror(pr->pr_type);
                break;
        }
}

static void tb_sysproperty_set_value(
        tb_sysproperty_t* pr,
        tb_sysproperty_type_t pr_type,
        char* value)
{
        dbe_catchup_logpos_t* p_lpid;

        CHK_SYSPROPERTY(pr);

        tb_sysproperty_free_value(pr);
        pr->pr_type = pr_type;
        if (value == NULL) {
            return;
        }

        switch (pr->pr_type) {
            case PR_TYPE_STRING:
                pr->pr_value = SsMemStrdup(value);
                break;
            case PR_TYPE_HSBLPID:
                p_lpid = SsMemAlloc(sizeof(dbe_catchup_logpos_t));
                *p_lpid = *(dbe_catchup_logpos_t*)value;
                pr->pr_value = (char*)p_lpid;
                ss_dprintf_1(("tb_sysproperty_set_value:value=(%d,%s,%d,%d,%d)\n", LOGPOS_DSDDD(*p_lpid)));
                break;
            default:
                ss_rc_derror(pr->pr_type);
                break;
        }
}

#ifndef SS_MYSQL
static char* tb_sysproperty_get_valuestr_copy(
        tb_sysproperty_t* pr)
{
        dbe_catchup_logpos_t* p_lpid;
        char value[255];

        CHK_SYSPROPERTY(pr);

        if (pr->pr_value != NULL) {

            switch (pr->pr_type) {
                case PR_TYPE_STRING:
                    return(SsMemStrdup(pr->pr_value));
                    break;
                case PR_TYPE_HSBLPID:

                    p_lpid = (dbe_catchup_logpos_t*)pr->pr_value;
                    dbe_catchup_logpos_to_string(*p_lpid, value);
                    return(SsMemStrdup(value));
                    break;
                default:
                    ss_rc_derror(pr->pr_type);
                    break;
            }
        }
        return(NULL);
}


static void tb_sysproperties_update(
        TliConnectT* tcon,
        char *key,
        char *new_value)
{
        TliCursorT* tcur;
        TliRetT trc;
        dt_date_t date;
        char *value;

        ss_dprintf_3(("tb_sysproperties_update:key=%s, new_value=%s\n",
            key,
            new_value));

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SYSPROPERTIES);
        if (tcur == NULL) {
            ss_dprintf_4(("tb_sysproperties_update:table %s does not exist\n", RS_RELNAME_SYSPROPERTIES));
            return;
        }

        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SYSPROPERTY_KEY, &key);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SYSPROPERTY_VALUE, &value);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColDate(tcur, (char *)RS_ANAME_SYSPROPERTY_MODTIME, &date);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorConstrStr(
                tcur,
                (char *)RS_ANAME_SYSPROPERTY_KEY,
                TLI_RELOP_EQUAL,
                key);

        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorNext(tcur);

        date = tb_dd_curdate();

        if (trc == TLI_RC_SUCC) {
            if (new_value == NULL) {
                trc = TliCursorDelete(tcur);
                ss_dassert(trc == TLI_RC_SUCC);
            } else if (SsStrcmp(value, new_value) != 0) {
                ss_dprintf_4(("tb_sysproperties_update:update old row\n"));
                value = new_value;
                trc = TliCursorUpdate(tcur);
                ss_dassert(trc == TLI_RC_SUCC);
            }
        } else if(new_value != NULL) {
            ss_dprintf_4(("tb_sysproperties_update:insert new row\n"));
            value = new_value;
            trc = TliCursorInsert(tcur);
            ss_dassert(trc == TLI_RC_SUCC);
        }

        TliCursorFree(tcur);
}
#endif

void tb_sysproperties_done(tb_sysproperties_t *sp)
{
        su_list_node_t* node;
        tb_sysproperty_t* pr;

        CHK_SYSPROPERTIES(sp);
        ss_dassert(sp->sp_data != NULL);

        node = NULL;
        su_list_do_get(sp->sp_data, node, pr) {
            CHK_SYSPROPERTY(pr);
            tb_sysproperty_done(pr);
        }
        su_list_done(sp->sp_data);
        SsSemFree(sp->sp_mutex);
        SsMemFree(sp);
}

void tb_sysproperties_checkpoint(
        tb_sysproperties_t* sp __attribute__ ((unused)),
        rs_sysi_t* cd __attribute__ ((unused)))
{
#ifndef SS_MYSQL
        TliConnectT* tcon;
        su_list_node_t* node;
        tb_sysproperty_t* pr;
        TliRetT trc;
        tb_trans_t* trans;
        su_list_t* list_copy;
        dbe_hsbstate_t* hsbstate;

        ss_dprintf_1(("tb_sysproperties_checkpoint\n"));
        CHK_SYSPROPERTIES(sp);

        if(!sp->sp_initialized) {

            ss_dprintf_2(("tb_sysproperties_checkpoint:migration checkpoint\n"));

            return;
        }
        ss_dassert(sp->sp_initialized);

        hsbstate = dbe_db_gethsbstate(rs_sysi_db(cd));
        ss_dassert(hsbstate != NULL);
        if (hsbstate != NULL) {
            char buf[20];
            SsSprintf(buf, "%d", dbe_hsbstate_getloggingstatelabel(hsbstate));
            tb_sysproperties_set(sp, PROP_HSBSTATE, buf);
        }

        /*
         * take a snapshot of the properties
         *
         */

        list_copy = su_list_init(NULL);

        ss_dassert(SsSemThreadIsNotEntered(sp->sp_mutex));
        SsSemEnter(sp->sp_mutex);

        su_list_do_get(sp->sp_data, node, pr) {

            bool free_value;
            char *value = NULL;
            tb_sysproperty_t* pr_copy;

            CHK_SYSPROPERTY(pr);

            free_value = FALSE;

            if(pr->pr_callback_fun != NULL) {
                value = (pr->pr_callback_fun)(pr->pr_callback_ctx);
                free_value = (value != NULL);
            } else if(pr->pr_dirty) {
                value = pr->pr_value;
            } else {
                continue;
            }

            pr->pr_dirty = FALSE;

            pr_copy = tb_sysproperty_init(pr->pr_type, pr->pr_key, value, NULL, NULL, FALSE);

            if (free_value) {
                /* type must be string */
                SsMemFree(value);
            }

            su_list_insertlast(list_copy, pr_copy);
        }

        SsSemExit(sp->sp_mutex);

        /*
         * and store them into the system table (should use single cursor)
         *
         */

        tcon = TliConnectInit(cd);
        ss_dassert(tcon != NULL);

        trans = TliGetTrans(tcon);
        tb_trans_settransoption(cd, trans, TB_TRANSOPT_USEMAXREADLEVEL);
        tb_trans_settransoption(cd, trans, TB_TRANSOPT_NOLOGGING);
        tb_trans_settransoption(cd, trans, TB_TRANSOPT_NOCHECK);

        pr = NULL;

        while((pr = (tb_sysproperty_t*) su_list_removefirst(list_copy)) != NULL) {
            char* value;
            value = tb_sysproperty_get_valuestr_copy(pr);
            tb_sysproperties_update(tcon, pr->pr_key, value);
            if (value != NULL) {
                SsMemFree(value);
            }
            tb_sysproperty_done(pr);
        }

        su_list_done(list_copy);

        trc = TliCommit(tcon);
        ss_rc_dassert(trc == TLI_RC_SUCC || TliErrorCode(tcon) == DBE_ERR_DBREADONLY || TliErrorCode(tcon) == DBE_ERR_TRXREADONLY, TliErrorCode(tcon));

        TliConnectDone(tcon);
#endif /* !SS_MYSQL */
}

static char *tb_sysproperties_get_nomutex(tb_sysproperties_t* sp, char *key)
{
        su_list_node_t* node;
        char *retval = NULL;
        tb_sysproperty_t* pr;

        CHK_SYSPROPERTIES(sp);

        su_list_do_get(sp->sp_data, node, pr) {
            CHK_SYSPROPERTY(pr);
            if(0 == SsStrcmp(pr->pr_key, key)) {
                retval = pr->pr_value;
                break;
            }
        }

        return (retval);
}

static void tb_sysproperties_fetch_list(rs_sysi_t* cd, su_list_t* list)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc;
        char* key;
        char* value;

        tcon = TliConnectInit(cd);
        ss_dassert(tcon != NULL);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               (char *)RS_RELNAME_SYSPROPERTIES);

        if (tcur == NULL) {
            /* Table not found, maybe migrate from old database version */
            return;
        }

        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SYSPROPERTY_KEY, &key);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, (char *)RS_ANAME_SYSPROPERTY_VALUE, &value);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);

        while((trc = TliCursorNext(tcur)) == TLI_RC_SUCC) {
            tb_sysproperty_t* pr;

            pr = tb_sysproperty_init(PR_TYPE_STRING, key, value, NULL, NULL, FALSE);
            su_list_insertlast(list, pr);

        }

        TliCursorFree(tcur);
        ss_dassert(trc == TLI_RC_END);

        trc = TliCommit(tcon);
        ss_dassert(trc == TLI_RC_SUCC);

        TliConnectDone(tcon);
}

static void tb_sysproperties_merge_nomutex(
        tb_sysproperties_t* sp,
        su_list_t* list)
{
        tb_sysproperty_t* pr;

        while((pr = (tb_sysproperty_t*) su_list_removefirst(list)) != NULL) {
            if (!tb_sysproperties_get_nomutex(sp, pr->pr_key)) {
                su_list_insertlast(sp->sp_data, pr);
            } else {
                tb_sysproperty_done(pr);
            }
        }
}

void tb_sysproperties_start(
        tb_sysproperties_t* sp,
        rs_sysi_t* cd)
{
        su_list_t* list;

        ss_dprintf_1(("tb_sysproperties_start\n"));
        SS_PUSHNAME("tb_sysproperties_start");

        ss_dassert(!sp->sp_initialized);
        CHK_SYSPROPERTIES(sp);
        ss_dassert(SsSemThreadIsNotEntered(sp->sp_mutex));

        sp->sp_tdb = rs_sysi_tabdb(cd);

        list = su_list_init(NULL);
        tb_sysproperties_fetch_list(cd, list);

        SsSemEnter(sp->sp_mutex);
        tb_sysproperties_merge_nomutex(sp, list);
        sp->sp_initialized = TRUE;
        SsSemExit(sp->sp_mutex);

        su_list_done(list);

        SS_POPNAME;
}


tb_sysproperties_t* tb_sysproperties_init(void)
{
        tb_sysproperties_t* sp;

        ss_dprintf_1(("tb_sysproperties_init\n"));
        SS_PUSHNAME("tb_sysproperties_init");

        sp = SSMEM_NEW(tb_sysproperties_t);
        ss_debug(sp->sp_check = TBCHK_SYSPROPERTIES;)
        sp->sp_data = su_list_init(NULL);
        sp->sp_initialized = FALSE;
        sp->sp_mutex = SsSemCreateLocal(SS_SEMNUM_TAB_SYSPROPERTIES);
        sp->sp_tdb = NULL;

        CHK_SYSPROPERTIES(sp);

        SS_POPNAME;

        return (sp);
}

/* will return the stored value (even if callback function was used) */
char *tb_sysproperties_get(tb_sysproperties_t* sp, char *key)
{
        char *retval = NULL;

        CHK_SYSPROPERTIES(sp);
        ss_dassert(sp->sp_initialized);

        ss_dassert(SsSemThreadIsNotEntered(sp->sp_mutex));
        SsSemEnter(sp->sp_mutex);
        retval = tb_sysproperties_get_nomutex(sp, key);
        SsSemExit(sp->sp_mutex);

        return (retval);
}


dbe_catchup_logpos_t tb_sysproperties_get_lpid(tb_sysproperties_t* sp, char *key)
{
        su_list_node_t* node;
        dbe_catchup_logpos_t retval;
        tb_sysproperty_t* pr;

        CHK_SYSPROPERTIES(sp);
        ss_dassert(sp->sp_initialized);

        DBE_CATCHUP_LOGPOS_SET_NULL(retval);

        su_list_do_get(sp->sp_data, node, pr) {
            CHK_SYSPROPERTY(pr);
            if(0 == SsStrcmp(pr->pr_key, key)) {

                if (pr->pr_value != NULL) {
                    switch (pr->pr_type) {
                        case PR_TYPE_STRING:
                            retval = dbe_catchup_logpos_init_from_string(pr->pr_value);
                            break;
                        case PR_TYPE_HSBLPID:
                            retval = *(dbe_catchup_logpos_t*)pr->pr_value;
                            break;
                        default:
                            ss_rc_derror(pr->pr_type);
                            break;
                    }
                }
                break;
            }
        }

        return (retval);
}


static void tb_sysproperties_set_value_or_callback(
        tb_sysproperties_t* sp,
        tb_sysproperty_type_t pr_type,
        char *key,
        char *value,
        tb_sysproperties_callback_fun_t callback_fun,
        void *callback_ctx)
{
        su_list_node_t* node;
        tb_sysproperty_t* pr = NULL;
        bool inserted = FALSE;

        CHK_SYSPROPERTIES(sp);

        ss_dassert(SsSemThreadIsNotEntered(sp->sp_mutex));
        SsSemEnter(sp->sp_mutex);

        su_list_do_get(sp->sp_data, node, pr) {
            CHK_SYSPROPERTY(pr);
            if(0 == SsStrcmp(pr->pr_key, key)) {
                tb_sysproperty_free_value(pr);
                
                tb_sysproperty_set_value(pr, pr_type, value);
                if(value != NULL) {
                    /* pr->pr_value = SsMemStrdup(value); */
                    pr->pr_callback_fun = NULL;
                    pr->pr_callback_ctx = NULL;
                } else {
                    /* pr->pr_value = NULL; */
                    pr->pr_callback_fun = callback_fun;
                    pr->pr_callback_ctx = callback_ctx;
                }

                pr->pr_dirty = TRUE;
                inserted = TRUE;
                break;
            }
        }

        if(!inserted) {
            pr = tb_sysproperty_init(
                    pr_type,
                    key,
                    value,
                    callback_fun,
                    callback_ctx,
                    TRUE);
            su_list_insertlast(sp->sp_data, pr);
        }

        if (sp->sp_initialized) {
            dbe_db_t* db;
            int rc;

            db = tb_tabdb_getdb(sp->sp_tdb);
            rc = dbe_db_force_checkpoint(db);

            if (rc != DBE_RC_SUCC) {
                ss_dassert(rc == DBE_ERR_DBREADONLY);
                ss_dprintf_1((
                    "tb_sysproperties_set_value_or_callback:%d\n",
                    rc));
            }
        }

        SsSemExit(sp->sp_mutex);
}

void tb_sysproperties_register_callback(
        tb_sysproperties_t* sp,
        char *key,
        tb_sysproperties_callback_fun_t callback_fun,
        void *callback_ctx)
{
        ss_dprintf_1(("tb_sysproperties_register_callback\n"));
        CHK_SYSPROPERTIES(sp);
        ss_dassert(key != NULL);

        tb_sysproperties_set_value_or_callback(
            sp,
            PR_TYPE_STRING,
            key,
            NULL,
            callback_fun,
            callback_ctx);
}

void tb_sysproperties_set(
        tb_sysproperties_t* sp,
        char *key,
        char *value)
{
        ss_dprintf_1(("tb_sysproperties_set:key=%s, value=%s\n", key, value));
        CHK_SYSPROPERTIES(sp);
        ss_dassert(key != NULL);

        tb_sysproperties_set_value_or_callback(
            sp,
            PR_TYPE_STRING,
            key,
            value,
            NULL,
            NULL);
}

void tb_sysproperties_set_lpid(
        tb_sysproperties_t* sp,
        char *key,
        dbe_catchup_logpos_t* value)
{
        ss_dprintf_1(("tb_sysproperties_set_lpid:key=%s, value=(%d,%s,%d,%d,%d)\n", key, LOGPOS_DSDDD(*value)));
        CHK_SYSPROPERTIES(sp);
        ss_dassert(key != NULL);

        tb_sysproperties_set_value_or_callback(
            sp,
            PR_TYPE_HSBLPID,
            key,
            (char*)value,
            NULL,
            NULL);
}

dbe_hsbstatelabel_t tb_sysproperties_gethsbstate(tb_sysproperties_t* sp)
{
        char* prop;
        long statelabel;

        prop = tb_sysproperties_get(sp, (char *)PROP_HSBSTATE);
        if (prop != NULL) {
            bool succp;
            char *mismatch;
            succp = SsStrScanLong(prop, &statelabel, &mismatch);
            ss_dassert(succp);
            ss_dassert(*mismatch == '\0');
        } else {
            statelabel = HSB_STATE_STANDALONE;
        }
        return((dbe_hsbstatelabel_t)statelabel);
}


#endif /* SS_HSBG2 */
