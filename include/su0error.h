/*************************************************************************\
**  source       * su0error.h
**  directory    * su
**  description  * Error return codes for SOLID
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


#ifndef SU0ERROR_H
#define SU0ERROR_H

#include <ssstddef.h>
#include <ssstdarg.h>

#include <ssc.h>

#include "su0list.h"
#include "su0msgs.h"

typedef enum {
        SU_SUCCESS = 0,
#define DBE_RC_SUCC SU_SUCCESS

        SQL_ERR_LAST = 999,
        
        DBE_RC_FOUND = 1000,
        DBE_RC_NOTFOUND,                /* 1001 */
        DBE_RC_END,                     /* 1002 */
        DBE_RC_NODESPLIT,               /* 1003 */
        DBE_WARN_HEADERSINCONSISTENT,   /* 1004 */
        DBE_WARN_DATABASECRASHED,       /* 1005 */
        DBE_RC_NODEEMPTY,               /* 1006 */
        DBE_RC_NODERELOCATE,            /* 1007 */
        DBE_RC_RESET,                   /* 1008 */
        DBE_RC_CONT,                    /* 1009 */
        DBE_RC_LOGFILE_TAIL_CORRUPT,    /* 1010 */
        DBE_RC_CHANGEDIRECTION,         /* 1011 */
        DBE_WARN_BLOBSIZE_OVERFLOW,     /* 1012 */
        DBE_WARN_BLOBSIZE_UNDERFLOW,    /* 1013 */
        DBE_RC_RETRY,                   /* 1014 */
        DBE_RC_WAITLOCK,                /* 1015 */
        DBE_RC_LOCKTUPLE,               /* 1016 */
        DBE_RC_WAITFLUSH,               /* 1017 */
        DB_RC_NOHSB,                    /* 1018 */
        DBE_RC_CANCEL,                  /* 1019 */
        DBE_RC_WAITFLUSHBATCH,          /* 1020 Flush batch buffer has space */
        DBE_RC_FIRSTLEAFKEY,            /* 1021 */
        DBE_WARN_WRONGBLOCKSIZE_SSD,    /* 1022 */
        DBE_RC_EXISTS,                  /* 1023 */

        MME_RC_FOUND_EXACT = 5000,      /* 5000 */
        MME_RC_FOUND_GREATER,           /* 5001 */
        MME_RC_FOUND_SMALLER,           /* 5002 */

        MME_RC_DUPLICATE_DELETE = 5100, /* 5100 */

        DBE_ERRORBEGIN = 10000,
        DBE_ERR_NOTFOUND,               /* 10001 */
        DBE_ERR_FAILED,                 /* 10002 */
        __NOTUSED_DBE_ERR_EXISTS,       /* 10003 */
        DBE_ERR_REDEFINITION,           /* 10004 */
        DBE_ERR_UNIQUE_S,               /* 10005 */
        DBE_ERR_LOSTUPDATE,             /* 10006 */
        DBE_ERR_NOTSERIALIZABLE,        /* 10007 */
        DBE_ERR_SNAPSHOTNOTEXISTS,      /* 10008 */
        DBE_ERR_SNAPSHOTISNEWEST,       /* 10009 */
        DBE_ERR_NOCHECKPOINT,           /* 10010 */
        DBE_ERR_HEADERSCORRUPT,         /* 10011 */
        DBE_ERR_NODESPLITFAILED,        /* 10012 */
        DBE_ERR_TRXREADONLY,            /* 10013 */
        DBE_ERR_LOCKED,                 /* 10014 */
        __NOTUSED_DBE_ERR_ALREADYCP,    /* 10015 */
        DBE_ERR_LOGFILE_CORRUPT,        /* 10016 */
        DBE_ERR_TOOLONGKEY,             /* 10017 Too long key value. */
        __NOTUSED_DBE_ERR_NOBACKUPDIR,  /* 10018 */
        DBE_ERR_BACKUPACT,              /* 10019 Backup process is active. */
        DBE_ERR_CPACT,                  /* 10020 Checkpoint process is active. */
        DBE_ERR_LOGDELFAILED_S,         /* 10021 Log file delete failed after backup. */
        __NOTUSED_DBE_ERR_TRXFAILED,    /* 10022 */
        DBE_ERR_WRONG_LOGFILE_S,        /* 10023 Logfile is from another database! */
        DBE_ERR_ILLBACKUPDIR_S,         /* 10024 */
        __NOTUSED_DBE_ERR_WBLOBBROKEN,  /* 10025 */
        DBE_ERR_TRXTIMEOUT,             /* 10026 Transaction timed out. */
        DBE_ERR_NOACTSEARCH,            /* 10027 No active search for update or delete. */
        DBE_ERR_CHILDEXIST_S,           /* 10028 */
        DBE_ERR_PARENTNOTEXIST_S,       /* 10029 */
        DBE_ERR_BACKUPDIRNOTEXIST_S,    /* 10030 Backup directory does not exist */
        DBE_ERR_DEADLOCK,               /* 10031 */
        DBE_ERR_WRONGBLOCKSIZE,         /* 10032 Wrong block size specified */
        DBE_ERR_PRIMUNIQUE_S,           /* 10033 */
        DBE_ERR_SEQEXIST,               /* 10034 */
        DBE_ERR_SEQNOTEXIST,            /* 10035 */
        DBE_ERR_SEQDDOP,                /* 10036 */
        DBE_ERR_SEQILLDATATYPE,         /* 10037 */
        DBE_ERR_ILLDESCVAL,             /* 10038 */
        DBE_ERR_ASSERT,                 /* 10039 */
        DBE_ERR_LOGWRITEFAILURE,        /* 10040 */
        DBE_ERR_DBREADONLY,             /* 10041 */
        DBE_ERR_INDEXCHECKFAILED,       /* 10042 */
        DBE_ERR_PRIMKEYBLOB,            /* 10043 */
        DBE_ERR_FREELISTDUPLICATE,      /* 10044 */
        DBE_ERR_HSBSECONDARY,               /* 10045 */
        DBE_ERR_DDOPACT,                /* 10046 Data dictionary operation is active. */
        DBE_ERR_HSBABORTED,             /* 10047 */
        DBE_ERR_HSBSQL,                 /* 10048 */
        DBE_ERR_HSBNOTSECONDARY,        /* 10049 */
        DBE_ERR_HSBBLOB,                /* 10050 */
        DBE_ERR_LOGFILE_CORRUPT_S,      /* 10051 */
        DBE_ERR_CRASHEDDBNOMIGRATEPOS,  /* 10052 Migrate to UNICODE failed if crashed db */
        DBE_ERR_RELREADONLY,            /* 10053 */
        DBE_ERR_DBALREADYINUSE,         /* 10054 */
        DBE_ERR_TOOSMALLCACHE_SSUU,     /* 10055 */
        DBE_ERR_CANNOTOPENDB_SSD,       /* 10056 */
        DBE_ERR_DBCORRUPTED,            /* 10057 */
        DBE_ERR_WRONGHEADERVERS_D,      /* 10058 */
        DBE_ERR_DATABASEFILEFORMAT_D,   /* 10059 */
        DBE_ERR_CANTRECOVERREADONLY_SS, /* 10060 */
        DBE_ERR_OUTOFCACHEBLOCKS_SS,    /* 10061 */
        DBE_ERR_LOGFILEWRITEFAILURE_SU, /* 10062 */
        DBE_ERR_LOGFILEALREADYEXISTS_S, /* 10063 */
        DBE_ERR_ILLLOGFILETEMPLATE_SSSDD, /* 10064 */
        DBE_ERR_ILLLOGCONF_SSD,           /* 10065 */
        DBE_ERR_CANTOPENLOG_SSSSS,        /* 10066 */
        DBE_ERR_OLDLOGFILE_S,             /* 10067 */
        DBE_ERR_ILLBLOCKSIZE_UUSSSU,     /* 10068 */
        DBE_ERR_RELIDNOTFOUND_D,        /* 10069 */
        DBE_ERR_RELNAMENOTFOUND_S,      /* 10070 */
        DBE_ERR_DBCORRUPTED_S,          /* 10071 */
        DBE_ERR_FILEIOPROBLEM_S,        /* 10072 */
        DBE_ERR_ILLINDEXBLOCK_DLSD,     /* 10073 */
        DBE_ERR_ROLLFWDFAILED,          /* 10074 */
        DBE_ERR_WRONGBLOCKSIZE_SSD,     /* 10075 */
        DBE_ERR_REDEFINITION_SSS,       /* 10076 */
        DBE_ERR_NOBASECATALOGGIVEN,     /* 10077 */
        DBE_ERR_USERROLLBACK,           /* 10078 Internal code */
        DBE_ERR_CANNOTREMOVEFILESPEC,   /* 10079 */
        DBE_ERR_HSBSECSERVERNOTINSYNC_D,/* 10080 */
        DBE_ERR_WRONGSIZE,              /* 10081 */
        DBE_ERR_BACKUP_ABORTED,         /* 10082 */
        DBE_ERR_ABORTHSBTRXFAILED,      /* 10083 */
        DBE_ERR_NOTLOCKED_S,            /* 10084 */
        DBE_ERR_CPDISABLED,             /* 10085 Checkpointing is disabled. */
        DBE_ERR_DELETEROWNOTFOUND,      /* 10086 */
        DBE_ERR_HSBMAINMEMORY,          /* 10087 HSB replication not allowed 
                                                 for main memory tables */
        DBE_ERR_LOCKTIMEOUTTOOLARGE_DD, /* 10088 */
        DBE_ERR_HSBPRIMARYUNCERTAIN,    /* 10089 */
        DBE_ERR_DDOPNEWERTRX,           /* 10090 */
        DBE_ERR_WRONGLOGBLOCKSIZEATBACKUP,/* 10091 */
        DBE_ERR_NOLOGGINGWITHHSB,       /* 10092 */
        DBE_ERR_MIGRATEHSB_WITHNOHSB,   /* 10093 */
        DBE_ERR_TOOFEWMMEPAGES_DD,      /* 10094 */
        DBE_ERR_CURSORISOLATIONCHANGE,  /* 10095 */
        DBE_ERR_NOTENOUGHMMEMEM_DD,     /* 10096 */
        DBE_ERR_HSBRECOV_PENDING,       /* 10097 */
        DBE_ERR_SEQINCFAILED_DD,        /* 10098 */
        DBE_ERR_NODBPASSWORD,           /* 10099 */
        DBE_ERR_WRONGPASSWORD,          /* 10100 */
        DBE_ERR_UNKNOWN_CRYPTOALG,      /* 10101 */
        DBE_ERR_OUTOFMEMORY,            /* 10102 */
        DBE_ERR_DISKERROR,              /* 10103 */
        DBE_ERR_NOTMYSQLDATABASEFILE,   /* 10104 */
        
        DBE_ERR_ERROREND = 10999,

        SU_ERR_FILE_OPEN_FAILURE = 11000,
        SU_ERR_FILE_WRITE_FAILURE,      /* 11001 */
        SU_ERR_FILE_WRITE_DISK_FULL,    /* 11002 */
        SU_ERR_FILE_WRITE_CFG_EXCEEDED, /* 11003 */
        SU_ERR_FILE_READ_FAILURE,       /* 11004 */
        SU_ERR_FILE_READ_EOF,           /* 11005 */
        SU_ERR_FILE_READ_ILLEGAL_ADDR,  /* 11006 */
        SU_ERR_FILE_LOCK_FAILURE,       /* 11007 */
        SU_ERR_FILE_UNLOCK_FAILURE,     /* 11008 */
        SU_ERR_FILE_FREELIST_CORRUPT,   /* 11009 */
        SU_ERR_TOO_LONG_FILENAME,       /* 11010 */
        SU_ERR_DUPLICATE_FILENAME,      /* 11011 */
        SU_LI_NONE,                     /* 11012 */
        SU_LI_CORRUPT,                  /* 11013 */
        SU_LI_DBTIMELIMITEXPIRED,       /* 11014 */
        SU_LI_EXETIMELIMITEXPIRED,      /* 11015 */
        SU_LI_CPUMISMATCH,              /* 11016 */
        SU_LI_OSMISMATCH,               /* 11017 */
        SU_LI_OSVERSMISMATCH,           /* 11018 */
        SU_LI_SOLVERSMISMATCH,          /* 11019 */
        SU_LI_CHKMISMATCH,              /* 11020 */
        SU_LI_LICENSEVIOLATION,         /* 11021 */
        SU_LI_NONETLICENSE_SS,          /* 11022 */
        SU_ERR_BSTREAM_BROKEN,          /* 11023 */
        SU_LI_NONETLICENSE_S,           /* 11024 */
        SU_LI_NOTCOMPAT_S,              /* 11025 */
        SU_ERR_BACKUPFILENOTREMOVABLE,  /* 11026 */
        SU_ERR_INVALID_SECTION,         /* 11027 */
        SU_ERR_INVALID_PARAMETER,       /* 11028 */
        SU_ERR_UNSUFF_PRIV,             /* 11029 */
        SU_ERR_MULTI_SET,               /* 11030 */
        SU_ERR_ILL_VALUE_TYPE,          /* 11031 */
        SU_ERR_CB_ERROR,                /* 11032 */
        SU_ERR_READONLY,                /* 11033 */
        SU_ERR_FILE_REMOVE_FAILURE,     /* 11034 */
        SU_ERR_PARAM_VALUE_TOOSMALL,    /* 11035 */
        SU_ERR_PARAM_VALUE_TOOBIG,      /* 11036 */
        SU_ERR_PARAM_VALUE_INVALID,     /* 11037 */
        SU_ERR_FILE_DELETE_FAILED,      /* 11038 */
        SU_ERR_FILE_ADDRESS_SPACE_EXCEEDED, /* 11039 */
        SU_ERR_PWDF_CANNOT_OPEN_S,      /* 11040 */
        SU_ERR_PWDF_NOT_FOUND_S,        /* 11041 */
#define SU_LI_OK                        SU_SUCCESS
        /* non-error SU return codes */
        SU_RC_OUTOFBUFFERS = 11500,
        SU_RC_END,                      /* 11501 */

        RC_SU_BSTREAM_SUCC = 12000,
        ERR_SU_BSTREAM_EOS,             /* 12001 */
        ERR_SU_BSTREAM_ERROR,           /* 12002 */
        ERR_SU_BSTREAM_BROKEN,          /* 12003 */
        ERR_SU_BSTREAM_TIMEOUT,         /* 12004 */

        RES_ERRORBEGIN = 13000,
        E_ILLCHARCONST_S,               /* 13001 Illegal CHAR constant '%s' */
        E_NOCHARARITH,                  /* 13002 Type CHAR not allowed for arithmetics */
        E_AGGRNOTORD_S,                 /* 13003 Aggregate function %s not available for 
                                                 ordinary call */
        E_ILLAGGRPRM_S,                 /* 13004 Ill parameter to aggregate fun %s */
        E_NOSUMAVGCHAR,                 /* 13005 SUM and AVG not available for CHAR type */
        E_NOSUMAVGDATE,                 /* 13006 SUM and AVG not available for DATE type */
        E_FUNCNODEF_S,                  /* 13007 Function not defined */
        E_ILLADDPRM_S,                  /* 13008 Illegal parameter to function ADD */
        E_DIVBYZERO,                    /* 13009 Divide by zero */
        __NOTUSED_E_DBERROR,            /* 13010 NOT USED */
        E_RELNOTEXIST_S,                /* 13011 Relation does not exist. */
        __NOTUSED_E_RELCANNOTOPEN,      /* 13012 */
        E_RELEXIST_S,                   /* 13013 Already exists */
        E_KEYNOTEXIST_S,                /* 13014 Key does not exist. */
        E_ATTRNOTEXISTONREL_SS,         /* 13015 Attribute does not exist on relation. */
        __NOTUSED_E_USERNOTEXIST,       /* 13016 */
        __NOTUSED_E_X_Y_FAILED_RC,      /* 13017 */
        E_JOINRELNOSUP,                 /* 13018 Join relation is not supported. */
        E_TRXSPNOSUP,                   /* 13019 TRX Savepoints nosup */
        E_DEFNOSUP,                     /* 13020 Defaults are not supported */
        __NOTUSED_E_FORKEYNOSUP,        /* 13021 Foreign keys not supported  */
        E_DESCKEYNOSUP,                 /* 13022 Descending keys not supported  */
        E_SCHEMANOSUP,                  /* 13023 */
        __NOTUSED_E_GRANTNOSUP,         /* 13024 */
        E_UPDNOCUR,                     /* 13025 Update/Delete through a cursor with */
        E_DELNOCUR,                     /* 13026 no current row */
        __NOTUSED_E_NOTRX,              /* 13027 */
        E_VIEWNOTEXIST_S,               /* 13028 View does not exists. */
        E_VIEWEXIST_S,                  /* 13029 View already exists. */
        E_INSNOTVAL_S,                  /* 13030 no value specified for NOT NULL col */
        E_DDOP,                         /* 13031 data dictionary operation is active */
        E_ILLTYPE_S,                    /* 13032 Illegal type '%s'  */
        E_ILLTYPEPARAM_SS,              /* 13033 Illegal parameter '%s' to type '%s'  */
        E_ILLCONST_S,                   /* 13034 Illegal constant '%s'  */
        E_ILLINTCONST_S,                /* 13035 Illegal INTEGER constant '%s'  */
        E_ILLDECCONST_S,                /* 13036 Illegal DECIMAL constant '%s'  */
        E_ILLDBLCONST_S,                /* 13037 Illegal DOUBLE PREC constant '%s'  */
        E_ILLREALCONST_S,               /* 13038 Illegal REAL constant '%s'  */
        E_ILLASSIGN_SS,                 /* 13039 Invalid assign from type '%s' to type '%s' */
        E_AGGRNODEF_S,                  /* 13040 Aggregate function '%s' not defined */
        E_NODATEARITH,                  /* 13041 Type CHAR not allowed for arithmetics */
        E_NODFLPOWARITH,                /* 13042 POW() not supported for dfloat arithmetics */
        E_ILLDATECONST_S,               /* 13043 Illegal DATE constant '%s'  */
        __NOTUSED_E_CASCADEOPNOSUP,     /* 13044 */
        __NOTUSED_E_PRIVREFNOSUP,       /* 13045 */
        E_ILLUSERNAME_S,                /* 13046 Illegal user name. */
        E_NOPRIV,                       /* 13047 No privilege for operation */
        E_NOGRANTOPTONTAB_S,            /* 13048 No grant option for table. */
        E_COLGRANTOPTNOSUP,             /* 13049 */
        E_TOOLONGCONSTR,                /* 13050 Too long constraint value, max constraint
                                                 is RS_KEY_MAXCMPLEN. */
        E_ILLCOLNAME_S,                 /* 13051 Illegal column name, e.g. a reserved name */
        E_ILLPSEUDOCOLRELOP,            /* 13052 Illegal relop for pseudo column */
        E_ILLPSEUDOCOLDATATYPE,         /* 13053 Illegal data type for a pseudo column */
        E_ILLPSEUDOCOLDATA,             /* 13054 Illegal pseudo column data, maybe data not
                                                 received using pseudo column */
        E_NOUPDPSEUDOCOL,               /* 13055 Cannot update pseudo attributes. */
        E_NOINSPSEUDOCOL,               /* 13056 Cannot insert pseudo attributes. */
        E_KEYNAMEEXIST_S,               /* 13057 Key name already exists. */
        E_CONSTRCHECKFAIL_S,            /* 13058 Constraint checks were not satisfaid. */
        E_SYSNAME_S,                    /* 13059 Reserved name for the system. */
        E_USERNOTFOUND_S,               /* 13060 */
        E_ROLENOTFOUND_S,               /* 13061 */
        E_ADMINOPTNOSUP,                /* 13062 */
        E_NAMEEXISTS_S,                 /* 13063 */
        E_NOTUSER_S,                    /* 13064 */
        E_NOTROLE_S,                    /* 13065 */
        E_USERNOTFOUNDINROLE_SS,        /* 13066 */
        E_TOOSHORTPASSWORD,             /* 13067 */
        E_SHUTDOWNINPROGRESS,           /* 13068 */
        __NOTUSED_E_INCRNOSUP,          /* 13069 */
        E_NUMERICOVERFLOW,              /* 13070 */
        E_NUMERICUNDERFLOW,             /* 13071 */
        E_NUMERICOUTOFRANGE,            /* 13072 */
        E_MATHERR,                      /* 13073 */
        E_ILLPASSWORD,                  /* 13074 Illegal passwored. */
        E_ILLROLENAME_S,                /* 13075 Illegal role name */
        __NOTUSED_E_ILLNULLALLOWED_S,   /* 13076 Null allowed is illegal. */
        E_LASTCOLUMN,                   /* 13077 */
        E_ATTREXISTONREL_SS,            /* 13078 */
        E_ILLCONSTR,                    /* 13079 Illegal search constraint */
        E_INCOMPATMODIFY_SSS,           /* 13080 Incompatible types in alter table. */
        E_DESCBINARYNOSUP,              /* 13081 v.1.21 */

        /* v.1.21  additions begin */
        E_FUNPARAMASTERISKNOSUP_S,      /* 13082 Function %s: parameter * not supported */
        E_FUNPARAMTOOFEW_S,             /* 13083 Function %s: Too few parameters */
        E_FUNPARAMTOOMANY_S,            /* 13084 Function %s: Too many parameters */
        E_FUNCFAILED_S,                 /* 13085 Function %s: Run-time failure */
        E_FUNPARAMTYPEMISMATCH_SD,      /* 13086 Function %s: type mismatch in parameter #%d */
        E_FUNPARAMILLVALUE_SD,          /* 13087 Function %s: illegal value in parameter #%d */
        E_NOPRIMKEY_S,                  /* 13088 */
        __NOTUSED_E_INVALIDPRIMKEY_S,   /* 13089 */
        E_FORKINCOMPATDTYPE_S,          /* 13090 */
        E_FORKNOTUNQK,                  /* 13091 */
        E_EVENTEXISTS_S,                /* 13092 */
        E_EVENTNOTEXIST_S,              /* 13093 */
        E_PRIMKDUPCOL_S,                /* 13094 */
        E_UNQKDUPCOL_S,                 /* 13095 */
        E_INDEXDUPCOL_S,                /* 13096 */
        E_PRIMKCOLNOTNULL,              /* 13097 */
        E_UNQKCOLNOTNULL,               /* 13098 */
        E_NOREFERENCESPRIV_S,           /* 13099 */
        E_ILLTABMODECOMB,               /* 13100 */
        /* v.1.21  additions end */

        /* v.2.00  additions begin */
        E_PROCONLYEXECUTE,              /* 13101 */
        E_EXECUTEONLYPROC,              /* 13102 */
        E_ILLGRANTORREVOKE,             /* 13103 */
        E_SEQEXISTS_S,                  /* 13104 */
        E_SEQNOTEXIST_S,                /* 13105 */
        E_FORKEYREFEXIST_S,             /* 13106 */
        E_ILLSETOPER,                   /* 13107 */
        /* v.2.00  additions end */

        /* v.2.10  additions begin */
        E_CMPTYPECLASH_SS,              /* 13108 */
        /* v.2.10  additions end */
        
        /* v.2.20  additions begin */
        E_SCHEMAOBJECTS,                /* 13109 */
        E_NULLNOTALLOWED_S,             /* 13110 */
        E_AMBIGUOUS_S,                  /* 13111 */
        E_MMINOFORKEY,                  /* 13112 */
        /* v.2.20  additions end */
        
        /* New additions */
        E_ILLEGALARITH_SS,              /* 13113 */
        E_NOBLOBARITH,                  /* 13114 */
        E_FUNPARAM_NOBLOBSUPP_SD,       /* 13115 */
        E_DUPCOL_S,                     /* 13116 */
        E_WRONGNUMOFPARAMS,             /* 13117 */
        E_COLPRIVONLYFORTAB,            /* 13118 */
        E_TYPESNOTUNIONCOMPAT_SS,       /* 13119 */
        E_TOOLONGNAME_S,                /* 13120 */
        E_TOOMANYCOLS_D,                /* 13121 */
        E_SYNCHISTREL,                  /* 13122 */
        E_RELNOTEMPTY_S,                /* 13123 */
        E_USERIDNOTFOUND_D,             /* 13124 */
        E_ILLEGALLIKEPAT_S,             /* 13125 */
        E_ILLEGALLIKETYPE_S,            /* 13126 */
        E_CMPFAILEDDUETOBLOB,           /* 13127 */
        E_LIKEFAILEDDUETOBLOBVAL,       /* 13128 */
        E_LIKEFAILEDDUETOBLOBPAT,       /* 13129 */
        E_ILLEGALLIKEESCTYPE_S,         /* 13130 */
        E_TOOMANYNESTEDTRIG,            /* 13131 */
        E_TOOMANYNESTEDPROC,            /* 13132 */
        E_INVSYNCLIC,                   /* 13133 */ /* JPA SS_SYNC */
        E_NOTBASETABLE,                 /* 13134 */ /* JPA SS_SYNC */
        E_ESTARITHERROR,                /* 13135 */
        E_TRANSNOTACT,                  /* 13136 */
        E_ILLGRANTMODE,                 /* 13137 */
        E_HINTKEYNOTFOUND_S,            /* 13138 */
        E_CATNOTEXIST_S,                /* 13139 */
        E_CATEXIST_S,                   /* 13140 */
        E_SCHEMANOTEXIST_S,             /* 13141 */
        E_SCHEMAEXIST_S,                /* 13142 */
        E_SCHEMAISUSER_S,               /* 13143 */
        E_TRIGILLCOMMITROLLBACK,        /* 13144 */
        E_SYNCPARAMNOTFOUND,            /* 13145 */
        E_CATALOGOBJECTS,               /* 13146 */
        E_NOCURCATDROP,                 /* 13147 */
        E_SCHEMAOBJECTS_S,              /* 13148 */
        E_CATALOGOBJECTS_S,             /* 13149 */
        E_CREIDXNOSAMECATSCH,           /* 13150 */
        E_CANNOTDROPUNQCOL_S,           /* 13151 */
        E_USEROBJECTS_S,                /* 13152 */
        E_LASTADMIN,                    /* 13153 */
        E_BLANKNAME,                    /* 13154 */
        E_MMIONLYFORDISKLESS,           /* 13155 */
        E_ATTREXISTONVIEW_SS,           /* 13156 */ /* Risto for Sputnik */ 
        E_NOCURSCHEDROP,                /* 13157 */
        E_NOCURUSERROP,                 /* 13158 */
        E_TRUNCATETABLENOSUP,           /* 13159 */
        E_TABLEHASTRIGREF_D,            /* 13160 */
        E_MMEUPDNEEDSFORUPDATE,         /* 13161 */
        E_MMEDELNEEDSFORUPDATE,         /* 13162 */
        E_DESCBIGINTNOSUP,              /* 13163 */ /* 23-jan-2003 */
        E_TRANSACTIVE,                  /* 13164 */
        E_MMENOPREV,                    /* 13165 */
        E_MMENOLICENSE,                 /* 13166 */
        E_TRANSIENTONLYFORMME,          /* 13167 */
        E_TRANSIENTNOTEMPORARY,         /* 13168 */
        E_TEMPORARYNOTRANSIENT,         /* 13169 */
        E_TEMPORARYONLYFORMME,          /* 13170 */
        E_MMEILLFORKEY,                 /* 13171 */
        E_REGULARREFERENCESTRANSIENT,   /* 13172 */
        E_REGULARREFERENCESTEMPORARY,   /* 13173 */
        E_TRANSIENTREFERENCESTEMPORARY, /* 13174 */
        E_REFERENCETEMPNONTEMP,         /* 13175 */
        E_CANNOTCHANGESTOREIFSYNCHIST,  /* 13176 */
        E_UNQKDUP_COND,                 /* 13177 */
        E_CONSTRAINT_NOT_FOUND_S,       /* 13178 */
        E_REF_ACTION_NOT_SUPPORTED,     /* 13179 */
        E_CONSTRAINT_NAME_CONFLICT_S,   /* 13180 */
        E_CONSTRAINT_CHECK_FAIL,        /* 13181 */
        E_NOTNULLWITHOUTDEFAULT,        /* 13182 */
        E_INDEX_IS_USED_S,              /* 13183 */
        E_PRIMKEY_NOTDEF_S,             /* 13184 */
        E_NULL_EXISTS_S,                /* 13185 */
        E_NULL_CANNOTDROP_S,            /* 13186 */
        E_MMENOTOVERCOMMIT,             /* 13187 */
        E_FORKEY_SELFREF,               /* 13188 */
        E_MMENOPOSITION,                /* 13189 */
        E_FATAL_DEFFILE_SSSS,           /* 13190 */
        E_FATAL_PARAM_SSSSS,            /* 13191 */
        E_FATAL_GENERIC_S,              /* 13192 */
        E_FORKEY_LOOPDEP,               /* 13193 */
        E_CANNOTDROPFORKEYCOL_S,        /* 13194 */
        E_READCOMMITTEDUPDNEEDSFORUPDATE,/* 13195 */
        E_READCOMMITTEDDELNEEDSFORUPDATE,/* 13196 */
        E_MMENOSUP,                      /* 13197 */
        E_FUNCILLCOMMITROLLBACK,        /* 13198 */
        E_DUPLICATEINDEX,               /* 13199 */
        E_UPDNEEDSFORUPDATE,            /* 13200 */
        E_DELNEEDSFORUPDATE,            /* 13201 */
        E_TC_ISOLATION,                 /* 13202 */
        E_DBENOLICENSE,                 /* 13203 */
        E_TC_ONLY,                      /* 13204 */
        E_UNRESFKEYS_S,                 /* 13205 */

        RS_RCBEGIN = 13500,
        RS_WARN_STRINGTRUNC_SS,         /* 13501 */
        RS_WARN_NUMERICTRUNC_SS,        /* 13502 */ 

        SRV_RCBEGIN = 14000,
        SRV_RC_MORE,                    /* 14001 */
        SRV_RC_END,                     /* 14002 */
        SRV_RC_CONT,                    /* 14003 */
        SRV_RC_EXTCONNECT1,             /* 14004 */
        SRV_RC_EXTREPLY,                /* 14005 */
        SRV_RC_EXTREPLY_FLUSHCACHE,     /* 14006 */
        SRV_RC_HSBCONNECTING,           /* 14007 */
        SRV_RC_HSBCATCHUP,              /* 14008 */
        SRV_RC_HSBSWITCHBEGIN,          /* 14009 */
        SRV_RC_HSBDISCONNECTING,        /* 14010 */

        SRV_ERRORBEGIN = 14500,
        SRV_ERR_FAILED,                 /* 14501 */
        SRV_ERR_RPCPARAM,               /* 14502 */
        SRV_ERR_COMERROR,               /* 14503 */
        SRV_ERR_DUPLICATECURSORNAME_S,  /* 14504 */
        SRV_ERR_ILLUSERORPASS,          /* 14505 */
        SRV_ERR_CLOSED,                 /* 14506 */
        SRV_ERR_TOOMANYUSERS,           /* 14507 */
        SRV_ERR_CONNTIMEOUT,            /* 14508 */
        SRV_ERR_VERSMISMATCH,           /* 14509 */
        SRV_ERR_RPCWRITEFAILED,         /* 14510 */
        SRV_ERR_RPCREADFAILED,          /* 14511 */
        SRV_ERR_USERSLOGGED,            /* 14512 */
        SRV_ERR_BACKUPACTIVE,           /* 14513 */
        SRV_ERR_MAKECPACTIVE,           /* 14514 */
        SRV_ERR_ILLTOUSERID,            /* 14515 */
        SRV_ERR_ILLTOUSERNAME,          /* 14516 */
        SRV_ERR_ATLOSTUPDATE,           /* 14517 */
        SRV_ERR_BROKENCONN,             /* 14518 */
        SRV_ERR_THROWOUT,               /* 14519 */
        SRV_ERR_SECONDARYCLOSED,        /* 14520 */
        SRV_ERR_THRCREATEFAILED,        /* 14521 */
        SRV_ERR_HSBNOSYNCDIR,           /* 14522 */
        SRV_ERR_HSBSWITCHACTIVE,        /* 14523 */
        SRV_ERR_HSBNOTSAMEDB,           /* 14524 */
        SRV_ERR_HSBNOTSYNC,             /* 14525 */
        SRV_ERR_BADARG,                 /* 14526 */
        SRV_ERR_HSBSTANDALONE,          /* 14527 */
        SRV_ERR_BOTHPRIMARIES,          /* 14528 */
        
        /* v.2.20  additions begin */
        SRV_ERR_OPERTIMEOUT,            /* 14529 */
        /* v.2.20  additions end */
        
        /* UNICODE version additions begin */
        SRV_ERR_CLINOUNICODESUPP,       /* 14530 */
        /* UNICODE version additions end */

        SRV_ERR_TOOMANYOPENCURSORS_D,   /* 14531 */
        SRV_ERR_CURSORSYNCFAILED,       /* 14532 */
        SRV_ERR_STMTCANCEL,             /* 14533 */
        SRV_ERR_ADMINONLY,              /* 14534 */

        SRV_ERR_ALREADYPRIMARY,         /* 14535 */
        SRV_ERR_ALREADYSECONDARY,       /* 14536 */
        SRV_ERR_HSBCONNBROKEN,          /* 14537 */
        SRV_ERR_HSBNOTPRIMARY,          /* 14538 */

        SRV_ERR_OPERATIONREFUSED,       /* 14539 */
        SRV_ERR_ALREADYSTANDALONE,      /* 14540 */
        SRV_ERR_CANNOTSETSTANDALONE,    /* 14541 */
        SRV_ERR_NOTSUPINBACKUPMODE,     /* 14542 */
        SRV_ERR_HSBROLEERROR2,          /* 14543 added by Devendra */
        SRV_ERR_DISKLESSNOTSUPP,        /* 14544 */
        SRV_ERR_HSBSETALONE,            /* 14545 */
        SRV_ERR_HSBSWITCHALONE,         /* 14546 */
        SRV_ERR_READTIMEOUTPARAM,       /* 14547 */
        SRV_ERR_HSBSWITCHNOHSBLOG,      /* 14548 */
        SRV_ERR_HSBACTIVE,              /* 14549 */
        SRV_ERR_HSBNOTBROKEN,           /* 14550 */
        SRV_ERR_MAXSTARTSTMTS_D,        /* 14551 */
        SRV_ERR_BACKUPSERVERMODE,       /* 14552 */
        SRV_ERR_BACKUPNOTACTIVE,        /* 14553 */
        SRV_ERR_TF_LEVEL_MISMATCH,      /* 14554 */
        SRV_ERR_NETBACKUPCOLLISION_S,   /* 14555 */
        SRV_ERR_NONETBACKUPCONNECTSTR,  /* 14556 */
        SRV_ERR_NETBACKUPSRVTOHSBG2,    /* 14557 */

        SRV_ERR_TC_AMBIQUOUS_CMD = 14600, /* 14600 */

        HSBG2_ERR_BOTH_ARE_PRIMARY = 14700, /* 14700 */
        HSBG2_ERR_BOTH_ARE_SECONDARY,   /* 14701 */
        HSBG2_ERR_CATCHUP_ACTIVE,       /* 14702 */
        HSBG2_ERR_COPY_ACTIVE,          /* 14703 */
        HSB_ERR_COPY_FAILED_NOT_PRIMARY_ALONE, /* 14704 */
        HSBG2_ERR_STANDALONE_ONLY_WHEN_ALONE,  /* 14705 */
        SRV_ERR_HSBINVALIDREADTHREADMODE,      /* 14706 */
        HSBG2_ERR_STANDALONE,                  /* 14707 */
        HSBG2_ERR_CATCHUPPOSNOTFOUND,          /* 14708 */
        HSBG2_ERR_NO_CONNECTSTRING,            /* 14709 */
        HSBG2_ERR_RESET,                       /* 14710 */
        HSBG2_ERR_SHUTDOWN,                    /* 14711 */
        HSB_ERR_NOTSUPP_IN_SECONDARY,          /* 14712 */


        SAP_ERRORBEGIN = 15000,
        SAP_ERR_SYNTAXERROR_SD,         /* 15001 */
        SAP_ERR_ILLCOLNAME_S,           /* 15002 */
        SAP_ERR_TOOMANYPARAMS,          /* 15003 */
        SAP_ERR_TOOFEWPARAMS,           /* 15004 */

        /* Main Memory Engine (MME) return codes */
        MME_RC_SUCCESS_WITH_INFO         = 16000,
        MME_RC_CONT,                    /* 16001 */
        MME_RC_END,                     /* 16002 */
        MME_RC_DELETE,                  /* 16003 */
        MME_RC_MEMORY_BACKTONORMAL,     /* 16004 */
        MME_RC_MEMORY_BACKTOLOW,        /* 16005 */
        
        /* Main Memory Engine (MME) error codes */
        MME_ERR_PERSISTENT_ROWS_MUST_BE_SHADOWED = 16500,
        MME_ERR_VALUE_TOO_LARGE,        /* 16501 */
        MME_ERR_NO_BLOB_SUPPORT,        /* 16502 */
        MME_ERR_NO_SERIALIZABLE,        /* 16503 */
        MME_ERR_MEMORY_LOW,             /* 16504 */
        MME_ERR_MEMORY_BOTTOM,          /* 16505 */
        MME_ERR_OUTOFMEMINSTARTUP,      /* 16506 */
        
        /* Err codes 20000-20999 reserved to DisKit session level errors */

        SES_ERRORBEGIN = 20000,         /* General ses errors begin */
        SES_ERR_ILLSESCLASS,            /* 20001 Illegal (unknown or nosup) session class */
        SES_ERR_DLLNOTFOUND,            /* 20002 Protocol specific DLL not found */
        SES_ERR_WRONGDLLVERSION,        /* 20003 Wrong DLL version */
        SES_ERR_ILLADDRINFO,            /* 20004 Illegal address info string */
        SES_ERR_LISADDRINUSE,           /* 20005 Listening address is already in use */
        SES_ERR_SERVERNOTFOUND,         /* 20006 No processs listening with given name */
        SES_ERR_ILLCONTROL,             /* 20007 Illegal control parameter */
        SES_ERR_ILLSIZEPRM,             /* 20008 Illegal byte count for read/write */
        SES_ERR_WRITEFAILED,            /* 20009 Write operation failed */
        SES_ERR_READFAILED,             /* 20010 Read operation failed */
        SES_ERR_ACCEPTFAILED,           /* 20011 Accept failed, out of resources */
        SES_ERR_NONET,                  /* 20012 Network card of software missing */
        SES_ERR_NOREC,                  /* 20013 Out of resources */
        SES_ERR_UNSPECIFIED,            /* 20014 Unspecified protocol error */
        SES_ERR_LISTENSTOPPED,          /* 20015 Listen stopped, out of resources */
        SES_ERR_THUNKNOSUP,             /* 20016 Thunking is not supported */
        SES_ERR_ILLADAPTER,             /* 20017 Illegal network adapter spefified */
        SES_ERR_HOSTMACHINENOTFOUND,    /* 20018 Host machine not found */
        SES_ERR_SRVCOMNOSUP,            /* 20019 Server end comm. not supported */
        SES_ERR_NOMBXPRIV,              /* 20020 No privilege to create a mailbox */
        SES_ERR_NOUPIPEACCESS,          /* 20021 No permission to access the upipe */
        SES_ERR_LISTHREADFAILED,        /* 20022 Internal listening thread failed to start */
        SES_ERR_TOOMANYRESOLVERREQUESTS, /* 20023 Too many DNS resolver requests already in progress */
        SES_ERR_RESOLVETIMEOUT,         /* 20024 DNS resolve timed out */
        SES_ERR_CONNECTTIMEOUT,         /* 20025 Connecting to a remote host timed out */
                                        
        SES_ERROREND = 20999,           /* General ses errors end*/
                                        
        COM_ERRORBEGIN = 21000,         /* Communication */

        COM_RC_SELECTERROR,             /* 21001 */
        COM_RC_NEWSES,                  /* 21002 */
        COM_RC_OLDSES,                  /* 21003 */
        COM_RC_BRKSES,                  /* 21004 */
        COM_RC_NOSES,                   /* 21005 */

        COM_WARN_INVCFGPARAMVALUE_SS = 21100,
        COM_WARN_INVINIFILEPROTOCOL_S,  /* 21101 */

        COM_ERR_PROTOCOLNOSUP_S = 21300,
        COM_ERR_DLLNOTFOUND_SS,         /* 21301 */
        COM_ERR_WRONGDLLVERSION_S,      /* 21302 */
        COM_ERR_ENVIRONFAULT_S,         /* 21303 */
        COM_ERR_NOREC_S,                /* 21304 */
        COM_ERR_EMPTYADDRINFO,          /* 21305 */
        COM_ERR_SERVERNOTFOUND_S,       /* 21306 */
        COM_ERR_INVCONNECTINFO_S,       /* 21307 */
        COM_ERR_BROKENCONN_SSD,         /* 21308 */
        COM_ERR_ACCEPTFAILED_S,         /* 21309 */
        COM_ERR_LISTENSTOPPED_S,        /* 21310 */
        COM_ERR_SELTHREADFAILED_S,      /* 21311 */
        COM_ERR_DUPLICATELISTENINFO_S,  /* 21312 */
        COM_ERR_ALREADYLISTENING_S,     /* 21313 */
        COM_ERR_LISADDRINUSE_S,         /* 21314 */
        COM_ERR_INVLISTENINFO_S,        /* 21315 */
        COM_ERR_CLOSEFAILED_S,          /* 21316 */
        COM_ERR_LISTENCFGSAVEFAILED,    /* 21317 */
        COM_ERR_UNSPECIFIED_SD,         /* 21318 */
        COM_ERR_RPCVERSMISMATCH,        /* 21319 */
        COM_ERR_RPCNOSUP,               /* 21320 */
        COM_ERR_ILLADAPTER_SD,          /* 21321 */
        COM_ERR_HOSTMACHINENOTFOUND_S,  /* 21322 */
        COM_ERR_SRVCOMNOSUP_S,          /* 21323 */
        COM_ERR_NOMBXPRIV,              /* 21324 */
        COM_ERR_ONLYONELISTENSUPP,      /* 21325 */
        COM_ERR_LISTHREADFAILED_SD,     /* 21326 */
        COM_ERR_TOOMANYRESOLVERREQUESTS, /* 21327 Too many DNS resolver requests already in progress */
        COM_ERR_RESOLVETIMEOUT_S,       /* 21328 DNS resolve timed out */
        COM_ERR_CONNECTTIMEOUT_S,       /* 21329 Connecting to a remote host timed out */
                                        
#define RPC_RC_SUCC                     SU_SUCCESS

        RPC_ERR_PINGILLSEQUENCE = 21500,
        RPC_ERR_PINGDATACORRUPT,        /* 21501 */
        RPC_ERR_PINGDATALOST,           /* 21502 */
        RPC_ERR_PINGEXTRADATA,          /* 21503 */
        RPC_ERR_PINGNOTALLOWED_D,       /* 21504 */
        RPC_ERR_PINGILLBUFSIZE,         /* 21505 */
        RPC_ERR_PINGBRKSES,             /* 21506 */
        RPC_RC_PINGOK_DS,               /* 21507 */
        RPC_ERR_PINGNOSUP,              /* 21508 */
        RPC_ERR_FILEWRITEFAILURE_S,     /* 21509 */
        RPC_ERR_FILEREADFAILURE_S,      /* 21510 */

        RPC_RC_SMTPLOGINFAILED = 21550, /* 21550 srv not found or or no account */
        RPC_RC_SMTPRCPTUNKNOWN,         /* 21551 unknown recipient */
        RPC_RC_SMTPHEADERFAILED,        /* 21552 sending message hdr failed */
        RPC_RC_SMTPDATAFAILED,          /* 21553 sending message data failed */

        /* DANGER:  If you change the following COM_ERROREND, you should
         *          check that cli1util.c and sa0usql.c are re-compiled
         */
        COM_ERROREND = 21999,           /* Communication errors end */

        RPC_ERRORBEGIN = 22000,         /* RPC NOT USED */

        RPC_ERROREND = 22999,

        SP_ERRORBEGIN = 23000,
        SP_ERR_UNDEFSYMB_S,                /* 23001 */
        SP_ERR_UNDEFCURS_S,                /* 23002 */
        SP_ERR_ILLSQLOP_S,                 /* 23003 */
        SP_ERR_SYNTAXERR_SD,               /* 23004 */
        SP_ERR_PROCNOTFOUND_S,             /* 23005 */
        SP_ERR_WRONGNUMOFPARAMS_S,         /* 23006 */
        SP_ERR_PROCEXIST_S,                /* 23007 */
        __NOTUSED_SP_ERR_CANNOTDROP_S,     /* 23008 */
        __NOTUSED_SP_ERR_EVENTNOTEXIST_SD, /* 23009 */
        SP_ERR_EVENTINCOMPATPARTYPE_SD,    /* 23010 */
        SP_ERR_EVENTWRONGPARCOUNT_SD,      /* 23011 */
        SP_ERR_EVENTDUPWAIT_SD,            /* 23012 */
        SP_ERR_UNDEFSEQUENCE_S,            /* 23013 */
        SP_ERR_DUPSEQNAME_S,               /* 23014 */
        SP_ERR_SEQNAMENOTFOUND_S,          /* 23015 */
        SP_ERR_SEQINCOMPATDATATYPE_SD,     /* 23016 */
        SP_ERR_DUPSYMB_S,                  /* 23017 */
        SP_ERR_PROCOWNERNOTFOUND_S,        /* 23018 */
        SP_ERR_DUPCURNAME_S,               /* 23019 */
        SP_ERR_ILLEGALWHENEVEROP_S,        /* 23020 */
        SP_ERR_ILLEGALRETURNROW_D,         /* 23021 */
        SP_ERR_SQLVARMUSTBECHAR_SD,        /* 23022 */
        SP_ERR_CALLSYNTAXERR_SD,           /* 23023 */
        SP_ERR_TRIGNOTFOUND_S,             /* 23024 */
        SP_ERR_TRIGEXIST_S,                /* 23025 */
        SP_ERR_NOTCHARTYPE_SD,             /* 23026 */
        SP_ERR_TRIGDUPCOLREF_S,            /* 23027 */
        SP_TRIGILLCOMMITROLLBACK,          /* 23028 */
        SP_FUNCILLCOMMITROLLBACK,          /* 23029 */
        SP_ERR_FUNCNOTFOUND_S,             /* 23030 */

        SP_RUNTIMEERRORBEGIN = 23500,
        SP_ERR_CURNOTOPEN_S,               /* 23501 */
        SP_ERR_ILLCOLNUM_SS,               /* 23502 */
        SP_ERR_PREVOPFAIL_SS,              /* 23503 */
        SP_ERR_CURNOEXEC_S,                /* 23504 */
        SP_ERR_NOTSELECT_S,                /* 23505 */
        SP_ERR_ENDOFTABLE_S,               /* 23506 */
        __NOTUSED_SP_ERR_ILLTYPECONV_SSS,  /* 23507 */
        SP_ERR_ILLASSIGN_D,                /* 23508 */
        SP_ERR_NOSQLERROR_SDS,             /* 23509 */
        SP_ERR_TRXNOTREADONLY_SD,          /* 23510 */
        SP_ERR_NOUSING_SDS,                /* 23511 */
        SP_ERR_TOOFEWUSING_SDS,            /* 23512 */
        SP_ERR_CMPTYPECLASH_SDSS,          /* 23513 */
        SP_ERR_LOGOPTYPECLASH_SDS,         /* 23514 */
        SP_ERR_LISTASSIGNFAILED_SDDS,      /* 23515 */
        SP_ERR_CALLPARAMASSIGNFAILED_SD,   /* 23516 */
        SP_ERR_ILLOPCODE_D,                /* 23517 */
        SP_ERR_USERERROR_S,                /* 23518 */
        SP_ERR_PROCNOFETCHPREV,            /* 23519 */
        SP_ERR_REMPROC_INVLINK_S,          /* 23520 */
        SP_ERR_REMPROC_NOLINK_GIVEN,       /* 23521 */
        SP_ERR_REMPROC_DYNPARAMS_ERROR,    /* 23522 */
        SP_ERR_DEFNODE_NOT_DEFINED,        /* 23523 */
        SP_ERR_COULDNOTLOAD_APP_S,         /* 23524 */
        SP_ERR_APPFUNCNOTFOUND_S,          /* 23525 */
        SP_ERR_DEFAULTPARAMASSIGNFAILED_SD,/* 23526 */
        SP_ERR_CALLPARAMASSIGNEDTWICE_SD,  /* 23527 */
        SP_ERR_APP_ALREADY_RUNNING_S,      /* 23528 */
        SP_ERR_APP_NOT_RUNNING_S,          /* 23529 */

        SP_ERROREND = 23999,

        XS_ERRORBEGIN = 24000,
        XS_OUTOFCFGDISKSPACE,              /* 24001 */
        XS_OUTOFPHYSDISKSPACE,             /* 24002 */
        XS_OUTOFBUFFERS,                   /* 24003 */
        XS_ERR_TOOLONGROW,                 /* 24004 */
        XS_ERR_SORTFAILED,                 /* 24005 */
        XS_ERROREND = 24999,

        SNC_ERRORBEGIN = 25000,
        SNC_ERR_STMTSAVEFAILED,            /* 25001 */
        SNC_ERR_STMTSAVENODDOP,            /* 25002 */
        SNC_ERR_STMTSAVENOSAVE,            /* 25003 */
        SNC_ERR_DYNPARAMNOSUPP,            /* 25004 */
        SNC_ERR_MSGACTIVE_S,               /* 25005 */
        SNC_ERR_MSGNOTACTIVE_S,            /* 25006 */
        SNC_ERR_MASTERNOTFOUND_S,          /* 25007 */
        __NOTUSED_SNC_ERR_MASTERNOTFOUNDES_SS,       /* 25008 */ /* Explicit Schema */
        SNC_ERR_REPLICANOTFOUND_S,         /* 25009 */
        SNC_ERR_PUBLNOTFOUND_S,            /* 25010 */
        SNC_ERR_PUBLWRONGNUMPARAMS_S,      /* 25011 */
        SNC_ERR_REPLYTIMEOUT,              /* 25012 */
        SNC_ERR_MSGNAMENOTFOUND_S,         /* 25013 */
        SNC_ERR_MORETHANONEMASTER,         /* 25014 */
        SNC_ERR_SYNTAXERR_SD,              /* 25015 */
        SNC_ERR_MSGNOTFOUND_DD,            /* 25016 */
        SNC_ERR_NOUNIQUEKEY_S,             /* 25017 */
        SNC_ERR_MSGILLSEQUENCE,            /* 25018 */
        SNC_ERR_NOTREPLICADB,              /* 25019 */
        SNC_ERR_NOTMASTERDB,               /* 25020 */
        SNC_ERR_NOTSYNCDB,                 /* 25021 */
        SNC_ERR_USERERR_S,                 /* 25022 */
        SNC_ERR_REPLICAREGISTERFAILED,     /* 25023 */
        SNC_ERR_UNDEFINED_MASTER,          /* 25024 */
        SNC_ERR_NODE_NOT_DEFINED,          /* 25025 */
        SNC_ERR_NOT_SYNC_UID,              /* 25026 */
        SNC_ERR_TOOLONGVALUE_LD,           /* 25027 */
        SNC_ERR_MSGSYSSUBSC_S,             /* 25028 */
        __NOTUSED_SNC_ERR_REPLICANOTFOUNDES_SS,      /* 25029 */ /* Explicit Schema */
        SNC_ERR_REPLICAALREADYEXIST_S,     /* 25030 */
        SNC_ERR_TRANSACTIVE,               /* 25031 */
        SNC_ERR_PUBLSQLMUSTRETROWS,        /* 25032 */
        SNC_ERR_PUBLEXIST_S,               /* 25033 */
        SNC_ERR_MSGNAMEEXIST_S,            /* 25034 */
        SNC_ERR_MSGLOCKED_S,               /* 25035 */
        SNC_ERR_PUBLVERS_S,                /* 25036 */
        SNC_ERR_PUBLCOLCOUNTMISMATCH_S,    /* 25037 */
        SNC_ERR_TABLEREFINPUBL_S,          /* 25038 */
        SNC_ERR_TABLEREFINSUBSC_S,         /* 25039 */
        SNC_ERR_USERIDNOTFOUND_D,          /* 25040 */
        SNC_ERR_SUBSCNOTFOUND_S,           /* 25041 */
        SNC_ERR_TOOLONG_MSGFORWARD_DD,     /* 25042 */
        SNC_ERR_TOOLONG_MSGREPLY_DD,       /* 25043 */
        SNC_ERR_CONFIGNOTCHARPARAM,        /* 25044 */
        SNC_ERR_MLEVELDISABLED,            /* 25045 */
        SNC_ERR_ILLCOMMITROLLBACK,         /* 25046 */
        SNC_ERR_DROPSUBSCNOPARAMINFO_S,    /* 25047 */
        SNC_ERR_PUBLREQUESTNOTFOUND_S,     /* 25048 */
        SNC_ERR_REFTABLENOTFOUNDINPUBL_S,  /* 25049 */
        SNC_ERR_TABLENOTHISTORIZED_S,      /* 25050 */
        SNC_ERR_MSGSEXIST,                 /* 25051 */
        SNC_ERR_SETNODENAMEFAILED_S,       /* 25052 */
        SNC_ERR_REPLICANOTREGISTERED_S,    /* 25053 */
        SNC_ERR_SYNCSCHEMAMISMATCH_S,      /* 25054 */
        SNC_ERR_CONNECTINFO_NOTALLOWED,    /* 25055 */
        SNC_ERR_TRANSAUTOCOMMIT,           /* 25056 */
        SNC_ERR_ALREADYREGISTERED_S,       /* 25057 */
        SNC_ERR_NOCONNECTINFO,             /* 25058 */
        SNC_ERR_NODERENAME,                /* 25059 */
        SNC_ERR_ATTRNOTEXISTONPUBL_SSS,    /* 25060 */
        SNC_ERR_WHERENOTABLEREF_S,         /* 25061 */
        SNC_ERR_USERMAP_NOTEXIST_SS,       /* 25062 */
        SNC_ERR_USERMAP_EXIST_SS,          /* 25063 */
        SNC_ERR_REPLICAMSGSEXIST_SS,       /* 25064 */
        SNC_ERR_MASTERMSGSEXIST_SS,        /* 25065 */
        SNC_ERR_BOOKMARKALREADYEXIST_S,    /* 25066 */
        SNC_ERR_BOOKMARKNOTFOUND_S,        /* 25067 */
        SNC_ERR_EXPFILE_OPEN_FAILURE_S,    /* 25068 */
        SNC_ERR_IMPFILE_OPEN_FAILURE_S,    /* 25069 */
        SNC_ERR_ITX_MORETHANONEMASTER,     /* 25070 */
        SNC_ERR_NOTREGISTEREDTOPUBL_S,     /* 25071 */
        SNC_ERR_ALREADYREGISTEREDTOPUBL_S, /* 25072 */
        SNC_ERR_EXPFILEWRONGMASTER_S,      /* 25073 */
        SNC_ERR_USERDEFNOTALLOWED,         /* 25074 */
        SNC_ERR_TRXNOTFOUND,               /* 25075 */
        SNC_ERR_MSGREGISTERREPLICAONLY_S,  /* 25076 */
        SNC_ERR_INVNODENAME_S,             /* 25077 */
        SNC_ERR_NODEALREADYEXIST_S,        /* 25078 */
        SNC_ERR_MASTEROBJECTS_S,           /* 25079 */
        SNC_ERR_REPLICAOBJECTS_S,          /* 25080 */
        SNC_ERR_PUBLNOSUBQUERY,            /* 25081 */
        SNC_ERR_NODENAMEREMOVEFAIL,        /* 25082 */
        SNC_ERR_NOCOMMITBLOCKWITHHSB,      /* 25083 */
        SNC_ERR_SAVESTMTNOADMIN,           /* 25084 */
        SNC_ERR_BLOBREADFAILED,            /* 25085 */
        SNC_ERR_SAVESTMTNOSTART,           /* 25086 */
        SNC_ERR_NOCONNECTINFO_S,           /* 25087 */

        SNC_ERR_ALREADYINMAINTENANCEMODE,  /* 25088 */
        SNC_ERR_NOTMAINTENANCEMODEOWNER,   /* 25089 */
        SNC_ERR_INMAINTENANCEMODE,         /* 25090 */
        SNC_ERR_NOTINMAINTENANCEMODE,      /* 25091 */
        
        SNC_ERR_APP_VERSION_STRING_MISMATCH, /* 25092 */
        SNC_ERR_MASTEREXISTS,              /* 25093 */
        SNC_ERR_CORRUPTEDMESSAGE_DDD,      /* 25094 */
        SNC_ERR_MESSAGE_ABORTED_S,         /* 25095 */

        E_SSAC_HY003 = 25200,
        E_SSAC_HY009,                      /* 25201 */
        E_SSAC_HY010,                      /* 25202 */
        E_SSAC_HY012,                      /* 25203 */
        E_SSAC_HY090,                      /* 25204 */
        E_SSAC_HY092,                      /* 25205 */
        E_SSAC_HYT01,                      /* 25206 */
        E_SSAC_24000,                      /* 25207 */
        E_SSAC_22001,                      /* 25208 */
        E_SSAC_22008,                      /* 25209 */
        E_SSAC_07002,                      /* 25210 */
        E_SSAC_07009,                      /* 25211 */
        E_SSAC_CONNECT_FAILED,             /* 25212 */
        E_SSAC_ALREADY_CONNECTED,          /* 25213 */
        E_SSAC_CONNECT_NOT_OPEN,           /* 25214 */
        E_SSAC_CONNECT_REQUEST_REJECTED,   /* 25215 */
        E_SSAC_CONNECT_EXPECT_ROLLBACK,    /* 25216 */
        E_SSAC_TF_CONNECT_FAILED,          /* 25217 */
        E_SSAC_CONNECTED_STANDALONE,       /* 25218 */
        E_SSAC_CLUSTERING_DISABLED,        /* 25219 */
        
        SNC_ERROREND = 25499,
        
        
        ERR_LAST_NOT_USED = 29999          /* Last possible error code, 
                                              bigger numbers are messages. */
} su_ret_t;

char* su_rc_getapplication(
        void);

void su_rc_setapplication(
        char* application);

void su_setprintmsgcode(
        bool p);

void su_rc_addsubsys(
        su_rc_subsys_t subsys,
        su_rc_text_t* texts,
        uint size);

su_list_t* su_rc_getallmessages(
        void);

char* su_rc_buildtext_bycomponent(
        su_rctype_t type,
        char* application,
        char* subsysname,
        char* type_name,
        int code,
        char* text);

char* su_rc_buildtext(
        su_rc_subsys_t subsys,
        su_rctype_t type,
        int code,
        char* text);

char* su_rc_vgivetext(
        su_ret_t rc,
        va_list arg_list);

char* su_rc_vgiveerrtext(
        su_ret_t rc,
        va_list arg_list,
        int msg_type);

char* su_rc_givetext_noargs(
        su_ret_t rc);

char* SS_CDECL su_rc_givetext(
        su_ret_t rc,
        ...);

char* su_rc_nameof(
        su_ret_t rc);

char* su_rc_classof(
        su_ret_t rc);

char* su_rc_textof(
        su_ret_t rc);

char* su_rc_typeof(
        su_ret_t rc);

char* su_rc_skipheader(
        char* text);

#if defined(NO_ANSI) 

int su_rc_assertionfailure();

#define su_rc_assert(expr, rc) \
        if (!(expr)) { su_rc_assertionfailure((char *)__FILE__, __LINE__, (char *)NULL, rc); }

#else /* NO_ANSI */

int su_rc_assertionfailure(char *file, int line, char *expr, su_ret_t rc);

#define su_rc_assert(expr, rc) \
        if (!(expr)) { su_rc_assertionfailure((char *)__FILE__, __LINE__, (char *)#expr, rc); }

#endif /* NO_ANSI */

#define su_rc_error(rc) su_rc_assertionfailure((char *)__FILE__, __LINE__, (char *)NULL, rc)

void SS_CDECL su_fatal_error(char* msg, ...);
void SS_CDECL su_rc_fatal_error(su_ret_t rc, ...);

#ifdef SS_DEBUG

#define su_rc_dassert su_rc_assert
#define su_rc_derror  su_rc_error

#else /* SS_DEBUG */

#define su_rc_dassert(exp, rc)
#define su_rc_derror(rc)

#endif /* SS_DEBUG */

#if defined(SS_BETA) || defined(SS_DEBUG)

#define su_rc_bassert su_rc_assert
#define su_rc_berror  su_rc_error

#else /* SS_BETA */

#define su_rc_bassert(exp, rc)
#define su_rc_berror(rc)

#endif /* SS_BETA */


void SS_CDECL su_rc_adderrortext(const char* buf, su_ret_t rc, ...);
void SS_CDECL su_informative_exit(const char* file, int line, su_ret_t rc, ...);
void SS_CDECL su_emergency_exit(const char* file, int line, su_ret_t rc, ...);

void SS_CDECL su_informative2rc_exit(
                const char* file,
                int line,
                su_ret_t rc,
                su_ret_t rc2,
                ...
);
void SS_CDECL su_emergency2rc_exit(
                const char* file,
                int line,
                su_ret_t rc,
                su_ret_t rc2,
                ...
);

#endif /* SU0ERROR_H */
