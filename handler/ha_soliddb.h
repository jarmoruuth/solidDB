/* Copyright (C) 2006-2007 MySQL AB & Solid Information Technology Ltd


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

/*
  Please read ha_exmple.cc before reading this file.
  Please keep in mind that the example storage engine implements all methods
  that are required to be implemented. handler.h has a full list of methods
  that you can implement.
*/

#ifdef INSIDE_HA_SOLIDDB_CC
/* We cannot use bool defined in Solid headers because it
* conflicts with the bool defined in Linux.
*/

#define bool_defined
#define solid_bool  int
#if defined (SS_LINUX) || defined (SS_FREEBSD)
#define bool        int
#endif

#ifdef __cplusplus
extern "C" {                    /* Assume C declarations for C++   */
#endif  /* __cplusplus */

#include <sswindow.h>
#include <ssstdio.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssprint.h>
#include <ssfnsplt.h>
#include <sstime.h>
#include <ssthread.h>
#include <ssmsglog.h>
#include <sspmon.h>
#include <ssutf.h>

#include <uti0vtpl.h>

#include <ui0msg.h>

#include <su0list.h>
#include <su0rbtr.h>
#include <su0err.h>
#include <su0error.h>
#include <su0vers.h>
#include <su0time.h>
#include <su0inifi.h>
#include <su0cfgst.h>
#include <su0param.h>
#include <su0pars.h>
#include <su0usrid.h>

#include <rs0types.h>
#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0ttype.h>
#include <rs0sysi.h>
#include <rs0cons.h>
#include <rs0key.h>
#include <rs0pla.h>
#include <rs0relh.h>
#include <rs0aval.h>
#include <rs0vbuf.h>

#include <dbe0type.h>
#include <dbe0curs.h>
#include <dbe0rel.h>
#include <dbe0db.h>
#include <dbe7cfg.h>
#include <dbe6bkey.h>

#include <est1est.h>
#include <est1pla.h>

#include <tab0tran.h>
#include <tab0conn.h>
#include <tab0admi.h>
#include <tab0relh.h>
#include <tab0relc.h>
#include <tab0sche.h>
#include <tab1dd.h>
#include <tab0seq.h>
#include <tab0srv.h>
#include <tab1refact.h>
#include <tab0minisql.h>
#include <su0vfil.h>

#include <ctype.h>
#include <my_dir.h>
#include <my_getopt.h>
#ifdef __WIN__
#include <direct.h>
#endif

#ifdef SS_MYSQL_AC

#include <tab0sql.h>

/* we should have separate include for HA */
char* hsb_sys_get_safenessstr(void);
#endif /* SS_MYSQL_AC */

#ifdef __cplusplus
}                                    /* End of extern "C" { */
#endif  /* __cplusplus */

#if defined (SS_LINUX) || defined (SS_FREEBSD)
#undef bool
#endif

#endif /* INSIDE_HA_SOLIDDB_CC */

#define ENGINENAME_SOLIDDB        "solidDB"
#define ENGINENAME_SOLIDDB_ALIAS  "solid"

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

/*
  EXAMPLE_SHARE is a structure that will be shared amoung all open handlers
  The example implements the minimum of what you will probably need.
*/

typedef struct st_soliddb_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length;
  uint use_count;
} SOLIDDB_SHARE;

#if MYSQL_VERSION_ID >= 50100
#define SS_MYSQL_ROW uchar
#else
#define SS_MYSQL_ROW byte
#endif

#ifndef MYSQL_THD
#define MYSQL_THD THD*
#endif

/*
  Class definition for the storage engine
*/

class ha_soliddb: public handler
{
  THR_LOCK_DATA lock;                     /* MySQL lock */
  SOLIDDB_SHARE *share;                   /* Shared lock info */

  MYSQL_THD user_thd;                     /* User THD set in ::external_lock() */
  bool rnd_mustinit;                      /* rnd_init called */
  bool for_update;                        /* write lock on read */
  bool extra_keyread;                     /* HA_EXTRA_KEYREAD */
  bool extra_retrieve_primary_key;        /* HA_EXTRA_RETRIEVE_PRIMARY_KEY */
  bool extra_retrieve_all_cols;           /* HA_EXTRA_RETRIEVE_ALL_COLS */
  bool mainmemory;

  bool extra_ignore_duplicate;
  bool extra_replace_duplicate;
  bool extra_update_duplicate;

#if MYSQL_VERSION_ID >= 50100
  Table_flags int_table_flags;
#else
  ulong int_table_flags;
#endif

  ulong num_write_rows;                   /* Number of ::write_row() calls */

  struct st_solid_connection* solid_conn; /* Reference to Solid connection. */
  struct solid_table_st*  solid_table;    /* Solid table handle. */
  struct solid_relcur_st* solid_relcur;   /* Solid table cursor handle. */

  struct st_solid_connection* get_solid_connection(THD* thd, const char * table_name = NULL);

  bool solid_ignore_duplicate(THD* thd);
  bool solid_ignore_duplicate_update(THD* thd);
  bool is_alter_table(THD* thd);

#if MYSQL_VERSION_ID < 50100
  void solid_update_duplicates(MYSQL_THD thd);
#endif

  int solid_fetch(SS_MYSQL_ROW *buf, bool nextp, THD* thd, uint);

  int solid_index_read_idx(
          SS_MYSQL_ROW* buf,
          uint index,
          const SS_MYSQL_ROW* key,
          uint key_len,
          enum ha_rkey_function find_flag,
          bool firstp,
          THD* thd);

  struct solid_relcur_st* solid_relcur_create(
          struct st_solid_connection* con,
          struct solid_relcur_st* old_relcur,
          struct rsrelhandlestruct* rsrelh,
          TABLE* table,
          THD* thd,
          int index,
          KEY* key_info);

public:

#if MYSQL_VERSION_ID >= 50100
  ha_soliddb(handlerton *hton, TABLE_SHARE *table_arg);
#else
  ha_soliddb(TABLE *table_arg);
#endif

  ~ha_soliddb();

  /* Return solidDB specific error message */
  bool get_error_message(int error, String *buf);

  /* The name that will be used for display purposes */
  const char *table_type() const { return(ENGINENAME_SOLIDDB); }
  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
   */
  const char *index_type(uint inx) { return "BTREE"; }
  const char **bas_ext() const;
  /*
    This is a list of flags that says what the storage engine
    implements. The current table flags are documented in
    handler.h
  */
#if MYSQL_VERSION_ID >= 50100
  Table_flags table_flags() const { return int_table_flags; }
#else
  ulong table_flags() const { return int_table_flags; }
#endif

  /*
    This is a bitmap of flags that says how the storage engine
    implements indexes. The current index flags are documented in
    handler.h. If you do not implement indexes, just return zero
    here.

    part is the key part to check. First key part is 0
    If all_parts it's set, MySQL want to know the flags for the combined
    index up to and including 'part'.
  */
  ulong index_flags(uint inx, uint part, bool all_parts) const
  {
      if (mainmemory) {
        /* gotoend does not yet work */
        return (HA_READ_NEXT |
                HA_READ_ORDER |
                HA_READ_RANGE |
                HA_KEYREAD_ONLY);
      } else {
          return (HA_READ_NEXT |
                  HA_READ_PREV |
                  HA_READ_ORDER |
                  HA_READ_RANGE |
                  HA_KEYREAD_ONLY);
      }
  }
  /*
    unireg.cc will call the following to make sure that the storage engine can
    handle the data it is about to send.

    Return *real* limits of your storage engine here. MySQL will do
    min(your_limits, MySQL_limits) automatically

    There is no need to implement ..._key_... methods if you don't suport
    indexes.
  */
  // TODO: we can get correct values for these.
  // for now we return something usefull
  uint max_supported_record_length() const { return HA_MAX_REC_LENGTH; }
  uint max_supported_keys()          const { return MAX_KEY; }
  uint max_supported_key_length() const { return HA_MAX_REC_LENGTH; }
  uint max_supported_key_part_length() const { return HA_MAX_REC_LENGTH; }
  const key_map *keys_to_use_for_scanning() { return &key_map_full; }

  bool has_transactions()  { return 1;}

  bool primary_key_is_clustered() { return TRUE; }

  /*
    Called in test_quick_select to determine if indexes should be used.
  */
  double scan_time();
  /*
    The next method will never be called if you do not implement indexes.
  */
  double read_time(uint index, uint ranges, ha_rows rows);

  /*
    Everything below are methods that we implement

    Most of these methods are not obligatory, skip them and
    MySQL will treat them as not implemented
  */
  int open(const char *name, int mode, uint test_if_locked);    // required
  int close(void);                                              // required

  int write_row(SS_MYSQL_ROW * buf);
  int update_row(const SS_MYSQL_ROW* old_data, SS_MYSQL_ROW* new_data);
  int delete_row(const SS_MYSQL_ROW* buf);

  int index_read(SS_MYSQL_ROW* buf, const SS_MYSQL_ROW* key,
                 uint key_len, enum ha_rkey_function find_flag);

  int index_read_last(SS_MYSQL_ROW* buf, const SS_MYSQL_ROW* key,
                 uint key_len);

#if MYSQL_VERSION_ID < 50100
  int index_read_idx(SS_MYSQL_ROW* buf, uint idx, const SS_MYSQL_ROW* key,
                     uint key_len, enum ha_rkey_function find_flag);
#endif

  int index_next(SS_MYSQL_ROW* buf);
  int index_next_same(SS_MYSQL_ROW* buf, const SS_MYSQL_ROW* key, uint keylen);
  int index_prev(SS_MYSQL_ROW* buf);
  int index_first(SS_MYSQL_ROW* buf);
  int index_last(SS_MYSQL_ROW* buf);
  /*
    unlike index_init(), rnd_init() can be called two times
    without rnd_end() in between (it only makes sense if scan=1).
    then the second call should prepare for the new table scan
    (e.g if rnd_init allocates the cursor, second call should
    position it to the start of the table, no need to deallocate
    and allocate it again
  */
  int rnd_init(bool scan);                                      //required
  int rnd_end();
  int rnd_next(SS_MYSQL_ROW* buf);                              //required
  int rnd_pos(SS_MYSQL_ROW* buf, SS_MYSQL_ROW* pos);            //required
  void position(const SS_MYSQL_ROW* record);                    //required

#if MYSQL_VERSION_ID >= 50030
  int info(uint);
#else
  void info(uint);                                              //required
#endif

  void start_bulk_insert(ha_rows rows);

  int start_stmt(THD *thd, thr_lock_type lock_type);

  int extra(enum ha_extra_function operation);
  int external_lock(THD *thd, int lock_type);                   //required
/*   int delete_all_rows(void); */
  int truncate(void);
  ha_rows records_in_range(uint inx, key_range *min_key,
                           key_range *max_key);
  int delete_table(const char *from);
  int rename_table(const char * from, const char * to);
  int create(const char *name, TABLE *form,
             HA_CREATE_INFO *create_info);                      //required

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);     //required

  uint8 table_cache_type();

  int check(THD* thd, HA_CHECK_OPT* check_opt);

  char* get_foreign_key_create_info();

  int get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list);

  /* Used in REPLACE, return 1 if a table is referenced by a foreign key */
  uint referenced_by_foreign_key();

  /* Auto increment routines */

  ulonglong get_auto_increment();
  int reset_auto_increment(ulonglong value);

  void free_foreign_key_create_info(char* str);

  int index_init(uint keynr
#if MYSQL_VERSION_ID >= 50100
                 , bool sorted
#endif
      );

#if MYSQL_VERSION_ID >= 50100
  void get_auto_increment(ulonglong offset, ulonglong increment,
                          ulonglong nb_desired_values,
                          ulonglong *first_value,
                          ulonglong *nb_reserved_values);

  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint table_changes);

  int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys);

  int prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys);

  int final_drop_index(TABLE *table_arg);

  int reset();
#endif /* MYSQL_VERSION_ID >= 50100 */

#ifdef SS_MULTI_RANGE_READ_SUPPORTED
  int read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
                             KEY_MULTI_RANGE *ranges, uint range_count,
                             bool sorted, HANDLER_BUFFER *buffer)
      { return HA_ERR_WRONG_COMMAND; }

  int read_multi_range_next(KEY_MULTI_RANGE **found_range_p)
      { return HA_ERR_WRONG_COMMAND; }


  int read_range_first(const key_range *start_key,
                       const key_range *end_key,
                       bool eq_range, bool sorted)
      { return HA_ERR_WRONG_COMMAND; }
#endif /* SS_MULTI_RANGE_READ_SUPPORTED */
  
private:
  virtual int disable_indexes(uint mode);
  virtual int enable_indexes(uint mode);
  virtual int indexes_are_disabled();
  int disable_foreign_keys();
  int enable_foreign_keys();

  virtual int analyze(THD* thd, HA_CHECK_OPT* check_opt);
};

/* Configuration parameters for SolidDB storage engine */

#if MYSQL_VERSION_ID < 50100
extern struct show_var_st soliddb_status_variables[];
#endif

extern char* soliddb_logdir;
extern char* soliddb_backupdir;
#ifdef HAVE_SOLIDDB_BACKUP_NEW
extern my_bool soliddb_backup_emptydir;
extern my_bool soliddb_backup_myisam;
extern my_bool soliddb_backup_system;
extern my_bool soliddb_backup_config;
#endif
extern char* soliddb_filespec;
extern longlong soliddb_cache_size;
extern ulong soliddb_durability_level;
extern my_bool soliddb_checkpoint_deletelog;
extern ulong soliddb_lock_wait_timeout;
extern ulong soliddb_db_block_size;
extern ulong soliddb_log_block_size;
extern ulong soliddb_backup_block_size;
extern longlong soliddb_checkpoint_interval;
extern ulong soliddb_io_threads;
extern longlong soliddb_lockhash_size;
extern ulong soliddb_checkpoint_time;
extern my_bool soliddb_log_enabled;
extern ulong soliddb_maxcursors;
extern ulong soliddb_threads;
extern my_bool soliddb_pessimistic;
extern ulong soliddb_write_threads;
extern ulong soliddb_extend_increment;
extern ulong soliddb_readahead;
extern char* soliddb_admin_command;

void soliddb_add_filespec(MYSQL_THD, struct st_mysql_sys_var *var, void* save, const void *value);
int soliddb_check_filespec(MYSQL_THD thd, struct st_mysql_sys_var *var, void* save, struct st_mysql_value *value);
int soliddb_check_backupdir(MYSQL_THD thd, struct st_mysql_sys_var *var, struct st_mysql_value *value, const void *save);
int soliddb_check_admin_command(MYSQL_THD thd, struct st_mysql_sys_var* var, void *save, struct st_mysql_value *value);
void soliddb_update_durability_level(MYSQL_THD thd, struct st_mysql_sys_var *var, void* value, void* save);
void soliddb_update_checkpoint_time(MYSQL_THD thd, struct st_mysql_sys_var *var, void* value, void* save);
void soliddb_update_lock_wait_timeout(MYSQL_THD, struct st_mysql_sys_var *var, void* value, void* save);
void soliddb_update_checkpoint_interval(MYSQL_THD thd, struct st_mysql_sys_var *var, void* value, void* save);
void soliddb_update_backupdir(MYSQL_THD thd, struct st_mysql_sys_var *var, void* value, const void* save);
void soliddb_update_admin_command(MYSQL_THD, struct st_mysql_sys_var *var, void* value, const void* save);

int solid_get_error_message(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        THD*,
        TABLE **,
        int,
        String *);

void solid_drop_database(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        char* path);

bool solid_flush_logs(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton
#endif
    );

void soliddb_export_status(void);

#if MYSQL_VERSION_ID < 50100

int solid_end(void);
bool soliddb_show_status(THD* thd);
bool soliddb_show_mutex_status(THD *thd);
#endif

#ifdef MYSQL_DYNAMIC_PLUGIN

extern "C" {
struct charset_info_st *thd_charset(MYSQL_THD thd);

char **thd_query(MYSQL_THD thd);

int thd_non_transactional_update(const MYSQL_THD thd);

int thd_binlog_format(const MYSQL_THD thd);

void thd_mark_transaction_to_rollback(MYSQL_THD thd, bool all);
}
#endif /* MYSQL_DYNAMIC_PLUGIN */

#ifdef INSIDE_HA_SOLIDDB_CC

#if MYSQL_VERSION_ID >= 50100
extern handlerton *legacy_soliddb_hton;
#endif

#define MAX_REF_LENGTH      8000                        /* TODO: neeed to check this. */
#define TABLE_SCAN_INDEX    -1
#define MAX_FOREIGN_LEN     64000

#define CHKVAL_CONN         986543
#define CHKVAL_RELCUR       329874

#define CHK_CONN(c)         ss_dassert((c) != NULL && (c)->sc_chk == CHKVAL_CONN)
#define CHK_RELCUR(r)       ss_dassert((r) != NULL && (r)->sr_chk == CHKVAL_RELCUR)

#define MAX_RELCURDONELIST  11

typedef enum {
        SOLID_NEW_CURSOR_CREATE,
        SOLID_NEW_CURSOR_REUSE,
        SOLID_NEW_CURSOR_USEOLD
} solid_new_cursor_t;


typedef struct st_solid_connection {
        int           sc_chk;
        tb_connect_t* sc_tbcon;        // connection
        int           sc_userid;       // userid for tb_srv_ (throwout)
        rs_sysi_t*    sc_cd;           // client data
        tb_trans_t*   sc_trans;        // transaction for the connection
        int           sc_isvalid;      // is this connection valid
        int           sc_nlink;        // number of refences to the connection
        su_err_t*     sc_errh;
        su_list_t*    sc_relcurlist;   // list of relcur objects allocated from
                                    // this connection
        su_list_t*    sc_relcurdonelist[MAX_RELCURDONELIST];  // list of relcur objects that should
                                                              // be released
        long          sc_err_tableid;  // Table id where error was found
        SsSemT*       sc_mutex;        // Own mutex for a connection
        rs_key_t*     sc_errkey;       // Key on constraint error
        rs_ttype_t*   sc_ttype;        // Value type to be inserted or updated
        rs_tval_t*    sc_tval;         // Value to be inserted or updated
        long          sc_seq_id;

        int           con_n_tables; // #-of tables in use

} SOLID_CONN;

SOLID_CONN* get_solid_ha_data_connection(
        handlerton *hton,
        MYSQL_THD thd);

typedef struct solid_table_st {
        rs_relh_t*  st_rsrelh;
        rs_ttype_t* st_ttype;
        rs_tval_t*  st_tval;
        int         st_relopsize1;
        int*        st_relops1;
        int*        st_anos1;
        rs_tval_t*  st_limtval1;
        int         st_relopsize2;
        int*        st_relops2;
        int*        st_anos2;
        rs_tval_t*  st_limtval2;
} solid_table_t;

typedef struct solid_relcur_st {
        int                   sr_chk;
        SOLID_CONN*           sr_con;
        dbe_cursor_t*         sr_relcur;
        rs_ttype_t*           sr_ttype;
        rs_tval_t*            sr_fetchtval;
        dbe_bkey_t*           sr_bkeybuf;
        uint                  sr_index;
        int                   sr_full_scan;
        rs_key_t*             sr_key;       /* Used key, or NULL for ::rnd_ routines. */
        rs_sysi_t*            sr_cd;
        tb_trans_t*           sr_trans;
        su_list_t*            sr_relcurlist;
        su_list_node_t*       sr_relcurlistnode;
        rs_relh_t*            sr_rsrelh;
        int                   sr_for_update;   // Use 'for update' in select
        dbe_cursor_type_t     sr_cursor_type;  // Cursor type
        int*                  sr_selectlist;   // Attribute number list of selected columns
        int*                  sr_plaselectlist;   // Attribute number list of selected columns
        su_list_t*            sr_constraints;
        rs_tval_t*            sr_constval;
        rs_tval_t*            sr_postval;
        int*                 sr_usedfields;   // Boolean array of used fields
        rs_pla_t*             sr_pla;
        rs_key_t*             sr_plakey;
        query_id_t            sr_query_id;      /* the latest query id with this cursor. */
        int                   sr_open_cursor;
        int                   sr_extra_keyread;                     /* HA_EXTRA_KEYREAD */
        int                   sr_extra_retrieve_primary_key;        /* HA_EXTRA_RETRIEVE_PRIMARY_KEY */
        int                   sr_extra_retrieve_all_cols;           /* HA_EXTRA_RETRIEVE_ALL_COLS */
        int                   sr_unique_row;
        int                   sr_force_dereference;
        int                   sr_mainmem;
        int                   sr_prevnextp;
        rs_vbuf_t*            sr_vbuf;
        su_pa_t*              sr_casc_states;
} solid_relcur_t;

typedef struct {
        char*   srm_mysqlname;
        char*   srm_solidname;
} solid_sysrel_map_t;

#endif /* INSIDE_HA_SOLIDDB_CC */
