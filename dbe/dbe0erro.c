/*************************************************************************\
**  source       * dbe0erro.c
**  directory    * dbe
**  description  * Error definitions.
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

#include <ssdebug.h>
#include <sssprint.h>

#include <su0error.h>
#include <su0err.h>

#include <rs0types.h>

#include "dbe0erro.h"

#ifndef SS_NOERRORTEXT

static su_rc_text_t dbe_rc_texts[] = {
{ DBE_RC_FOUND,                 SU_RCTYPE_RETCODE,  "DBE_RC_FOUND",
  "" },

{ DBE_RC_NOTFOUND,              SU_RCTYPE_RETCODE,  "DBE_RC_NOTFOUND",
  "" },

{ DBE_RC_END,                   SU_RCTYPE_RETCODE,  "DBE_RC_END",
  "" },

{ DBE_RC_NODESPLIT,             SU_RCTYPE_RETCODE,  "DBE_RC_NODESPLIT",
  "" },

{ DBE_WARN_HEADERSINCONSISTENT, SU_RCTYPE_WARNING,  "DBE_WARN_HEADERSINCONSISTENT",
  "Database headers are inconsistent" },

{ DBE_WARN_DATABASECRASHED,     SU_RCTYPE_WARNING,  "DBE_WARN_DATABASECRASHED",
  "Database is crashed" },

{ DBE_RC_NODEEMPTY,             SU_RCTYPE_RETCODE,  "DBE_RC_NODEEMPTY",
  "" },

{ DBE_RC_NODERELOCATE,          SU_RCTYPE_RETCODE,  "DBE_RC_NODERELOCATE",
  "" },

{ DBE_RC_RESET,                 SU_RCTYPE_RETCODE,  "DBE_RC_RESET",
  "" },

{ DBE_RC_CONT,                  SU_RCTYPE_RETCODE,  "DBE_RC_CONT",
  "" },

{ DBE_RC_LOGFILE_TAIL_CORRUPT,  SU_RCTYPE_RETCODE,  "DBE_RC_LOGFILE_TAIL_CORRUPT",
  "" },

{ DBE_RC_CHANGEDIRECTION,       SU_RCTYPE_RETCODE,  "DBE_RC_CHANGEDIRECTION",
  "" },

{ DBE_WARN_BLOBSIZE_OVERFLOW,   SU_RCTYPE_WARNING,  "DBE_WARN_BLOBSIZE_OVERFLOW",
  "BLOB size overflow" },

{ DBE_WARN_BLOBSIZE_UNDERFLOW,  SU_RCTYPE_WARNING,  "DBE_WARN_BLOBSIZE_UNDERFLOW",
  "BLOB size underflow" },

{ DBE_RC_RETRY,                 SU_RCTYPE_RETCODE,  "DBE_RC_RETRY",
  "" },

{ DBE_RC_WAITLOCK,              SU_RCTYPE_RETCODE,  "DBE_RC_WAITLOCK",
  "" },

{ DBE_RC_LOCKTUPLE,             SU_RCTYPE_RETCODE,  "DBE_RC_LOCKTUPLE",
  "" },

{ DBE_RC_WAITFLUSH,             SU_RCTYPE_RETCODE,  "DBE_RC_WAITFLUSH",
  "" },

{ DB_RC_NOHSB,                 SU_RCTYPE_RETCODE,  "DB_RC_NOHSB",
  "" },

{ DBE_RC_CANCEL,               SU_RCTYPE_RETCODE,  "DBE_RC_CANCEL",
  "Operation canceled" },

{ DBE_ERR_NOTFOUND,             SU_RCTYPE_ERROR,    "DBE_ERR_NOTFOUND",
  "Key value is not found" },

{ DBE_ERR_FAILED,               SU_RCTYPE_ERROR,    "DBE_ERR_FAILED",
  "Operation failed" },

{ __NOTUSED_DBE_ERR_EXISTS,     SU_RCTYPE_RETCODE,  "",
  "" },

{ DBE_ERR_REDEFINITION,   SU_RCTYPE_ERROR,    "DBE_ERR_REDEFINITION",
"Redefinition"
},

{ DBE_ERR_UNIQUE_S,             SU_RCTYPE_ERROR,    "DBE_ERR_UNIQUE",
  "Unique constraint %sviolation." },

{ DBE_ERR_LOSTUPDATE,           SU_RCTYPE_ERROR,    "DBE_ERR_LOSTUPDATE",
  "Concurrency conflict, two transactions updated or deleted the same row" },

{ DBE_ERR_NOTSERIALIZABLE,      SU_RCTYPE_ERROR,    "DBE_ERR_NOTSERIALIZABLE",
  "Transaction is not serializable" },

{ DBE_ERR_SNAPSHOTNOTEXISTS,    SU_RCTYPE_ERROR,    "DBE_ERR_SNAPSHOTNOTEXISTS",
  "Snapshot does not exist" },

{ DBE_ERR_SNAPSHOTISNEWEST,     SU_RCTYPE_ERROR,    "DBE_ERR_SNAPSHOTISNEWEST",
  "Snapshot is newest" },

{ DBE_ERR_NOCHECKPOINT,         SU_RCTYPE_FATAL,    "DBE_ERR_NOCHECKPOINT",
"No checkpoint exists in database. Possible causes for this error include:\n\
- Most likely the creation of a new database had failed.\n\
  Please recreate the database.\n\
- Otherwise, the database has been irrevocably corrupted.\n\
  Please revert to the latest backup.\n\
"
},

{ DBE_ERR_HEADERSCORRUPT,       SU_RCTYPE_FATAL,    "DBE_ERR_HEADERSCORRUPT",
  "Database headers are corrupted" },

{ DBE_ERR_NODESPLITFAILED,      SU_RCTYPE_FATAL,    "DBE_ERR_NODESPLITFAILED",
  "Node split failed" },

{ DBE_ERR_TRXREADONLY,          SU_RCTYPE_ERROR,    "DBE_ERR_TRXREADONLY",
  "Transaction is read-only" },

{ DBE_ERR_LOCKED,               SU_RCTYPE_ERROR,    "DBE_ERR_LOCKED",
  "Resource is locked" },

{ DBE_ERR_NOTLOCKED_S,          SU_RCTYPE_ERROR,    "DBE_ERR_NOTLOCKED_S",
  "Table '%s' is not locked" },

{ __NOTUSED_DBE_ERR_ALREADYCP,  SU_RCTYPE_RETCODE,  "",
  "" },

{ DBE_ERR_LOGFILE_CORRUPT,      SU_RCTYPE_ERROR,    "DBE_ERR_LOGFILE_CORRUPT",
  "Log file is corrupted" },

{ DBE_ERR_LOGFILE_CORRUPT_S,    SU_RCTYPE_ERROR,    "DBE_ERR_LOGFILE_CORRUPT_S",
  "Log file %.80s is corrupted" },

{ DBE_ERR_TOOLONGKEY,           SU_RCTYPE_ERROR,    "DBE_ERR_TOOLONGKEY",
  "Too long key value" },

{ __NOTUSED_DBE_ERR_NOBACKUPDIR,SU_RCTYPE_RETCODE,  "",
  "" },

{ DBE_ERR_BACKUPACT,            SU_RCTYPE_ERROR,    "DBE_ERR_BACKUPACT",
  "Backup is already active" },

{ DBE_ERR_CPACT,                SU_RCTYPE_ERROR,    "DBE_ERR_CPACT",
  "Checkpoint creation is active" },

{ DBE_ERR_LOGDELFAILED_S,        SU_RCTYPE_ERROR,    "DBE_ERR_LOGDELFAILED_S",
  "Failed to delete log file '.80%s'" },

{ __NOTUSED_DBE_ERR_TRXFAILED,  SU_RCTYPE_RETCODE,  "",
  "" },

{ DBE_ERR_WRONG_LOGFILE_S,     SU_RCTYPE_FATAL,    "DBE_ERR_WRONG_LOGFILE_S",
"Roll-forward recovery failed. There is a wrong log file %s in\n\
log file directory.\n\
\n\
Possible causes for this error include:\n\
- log file directory contains log files from different databases\n\
- some old log files are present in the log file directory\n\
\n\
SOLID process cannot use this invalid log file to recover.\n\
"
},

{ DBE_ERR_ILLBACKUPDIR_S,       SU_RCTYPE_ERROR,    "DBE_ERR_ILLBACKUPDIR_S",
  "Illegal backup directory '%.80s'" },

{ __NOTUSED_DBE_ERR_WBLOBBROKEN,SU_RCTYPE_RETCODE,  "",
  "" },

{ DBE_ERR_TRXTIMEOUT,           SU_RCTYPE_ERROR,    "DBE_ERR_TRXTIMEOUT",
  "Transaction is timed out" },

{ DBE_ERR_NOACTSEARCH,          SU_RCTYPE_ERROR,    "DBE_ERR_NOACTSEARCH",
  "No active search" },

{ DBE_ERR_CHILDEXIST_S,         SU_RCTYPE_ERROR,    "DBE_ERR_CHILDEXIST",
  "Foreign key constraint %sviolation, foreign key values exist." },

{ DBE_ERR_PARENTNOTEXIST_S,     SU_RCTYPE_ERROR,    "DBE_ERR_PARENTNOTEXIST",
  "Foreign key constraint %sviolation, referenced column values do not exist." },

{ DBE_ERR_BACKUPDIRNOTEXIST_S,  SU_RCTYPE_ERROR,    "DBE_ERR_BACKUPDIRNOTEXIST_S",
  "Backup directory '%.80s' does not exist" },

{ DBE_ERR_DEADLOCK,             SU_RCTYPE_ERROR,    "DBE_ERR_DEADLOCK",
  "Transaction detected a deadlock, transaction is rolled back" },

{ DBE_ERR_WRONGLOGBLOCKSIZEATBACKUP, SU_RCTYPE_ERROR,
  "DBE_ERR_WRONGLOGBLOCKSIZEATBACKUP",
  "Backup detected a log file with wrong block size, backup aborted" },
    
{ DBE_ERR_WRONGBLOCKSIZE,       SU_RCTYPE_FATAL,    "DBE_ERR_WRONGBLOCKSIZE",
"The database you are trying to use has been originally created\n\
with a different database block size settings than your current settings."
},

{ DBE_ERR_WRONGSIZE,            SU_RCTYPE_ERROR,    "DBE_ERR_WRONGSIZE",
"The database file is incomplete or corrupt.  If the file is on a hot \n\
standby secondary server, use the 'hotstandby copy' or 'hotstandby netcopy'\n\
command to send the file from the primary server again. If you use the \n\
'hotstandby' command, make sure that you don't start the secondary server \n\
before the copy is complete."},

{ DBE_ERR_PRIMUNIQUE_S,         SU_RCTYPE_ERROR,    "DBE_ERR_PRIMUNIQUE",
  "Primary key %sunique constraint violation." },

{ DBE_ERR_SEQEXIST,             SU_RCTYPE_ERROR,    "DBE_ERR_SEQEXIST",
  "Sequence already exists" },

{ DBE_ERR_SEQNOTEXIST,          SU_RCTYPE_ERROR,    "DBE_ERR_SEQNOTEXIST",
  "Sequence does not exist" },

{ DBE_ERR_SEQINCFAILED_DD,          SU_RCTYPE_ERROR,    "DBE_ERR_SEQINCFAILED_DD",
  "Incrementing sequence (id=%d) failed (rc=%d)" },

{ DBE_ERR_SEQDDOP,              SU_RCTYPE_ERROR,    "DBE_ERR_SEQDDOP",
  "Data dictionary operation is active for accessed sequence" },

{ DBE_ERR_SEQILLDATATYPE,       SU_RCTYPE_ERROR,    "DBE_ERR_SEQILLDATATYPE",
  "Can not store sequence value, the target data type is illegal" },

{ DBE_ERR_ILLDESCVAL,           SU_RCTYPE_ERROR,    "DBE_ERR_ILLDESCVAL",
  "Illecal column value for descending index" },

{ DBE_ERR_ASSERT,               SU_RCTYPE_ERROR,    "DBE_ERR_ASSERT",
  "INTERNAL: Assertion failure" },

{ DBE_ERR_LOGWRITEFAILURE,      SU_RCTYPE_ERROR,    "DBE_ERR_LOGWRITEFAILURE",
  "Log file write failure, probably the disk containing the log files is full" },

{ DBE_ERR_DBREADONLY,           SU_RCTYPE_ERROR,    "DBE_ERR_DBREADONLY",
  "Database is read-only" },

{ DBE_ERR_INDEXCHECKFAILED,     SU_RCTYPE_ERROR,    "DBE_ERR_INDEXCHECKFAILED",
  "Database index check failed, the database file is corrupted" },

{ DBE_ERR_FREELISTDUPLICATE,    SU_RCTYPE_ERROR,    "DBE_ERR_FREELISTDUPLICATE",
  "Database free block list corrupted, same block twice in free list" },

{ DBE_ERR_PRIMKEYBLOB,          SU_RCTYPE_ERROR,    "DBE_ERR_PRIMKEYBLOB",
  "Primary key can not contain blob attributes" },

{ DBE_ERR_HSBSECONDARY,         SU_RCTYPE_ERROR,    "DBE_ERR_HSBSECONDARY",
  "This database is a HotStandby secondary server, the database is read only" },

{ DBE_ERR_HSBMAINMEMORY,        SU_RCTYPE_ERROR,    "DBE_ERR_HSBMAINMEMORY",
  "HotStandby not allowed for main memory tables" },

{ DBE_ERR_DDOPACT,              SU_RCTYPE_ERROR,    "DBE_ERR_DDOPACT",
  "Operation failed, data dictionary operation is active" },

{ DBE_ERR_HSBABORTED,           SU_RCTYPE_ERROR,    "DBE_ERR_HSBABORTED",
  "Replicated transaction is aborted" },

{ DBE_ERR_HSBSQL,               SU_RCTYPE_ERROR,    "DBE_ERR_HSBSQL",
  "Data manipulation and data definition operations \n\
can not be used in the same transaction with HotStandby, operation failed" },

{ DBE_ERR_HSBNOTSECONDARY,      SU_RCTYPE_ERROR,    "DBE_ERR_HSBNOTSECONDARY",
  "Secondary server not available any more, transaction aborted" },

{ DBE_ERR_HSBBLOB,              SU_RCTYPE_ERROR,    "DBE_ERR_HSBBLOB",
  "Row contains BLOB columns that cannot be replicated with HotStandby" },

{ DBE_ERR_CRASHEDDBNOMIGRATEPOS,SU_RCTYPE_FATAL,    "DBE_ERR_CRASHEDDBNOMIGRATEPOS",
"Cannot convert an abnormally closed database. Please use \n\
the old SOLID version to recover the database first."
},

{ DBE_ERR_RELREADONLY,          SU_RCTYPE_ERROR,    "DBE_ERR_RELREADONLY",
  "Table is read only" },

{ DBE_ERR_DBALREADYINUSE,       SU_RCTYPE_FATAL,    "DBE_ERR_DBALREADYINUSE",
"Opening the database file failed.\n\
Probably another SOLID process is already running in the same directory." },

{ DBE_ERR_TOOSMALLCACHE_SSUU,   SU_RCTYPE_FATAL,    "DBE_ERR_TOOSMALLCACHE_SSUU",
"Too little cache memory has been specified for the SOLID process:\n\n\t\
[%s]\n\t%s=%lu\n\n\
Please edit the solid.ini file to increase this parameter value at least\n\
to %lu bytes and restart the SOLID process."
},

{ DBE_ERR_CANNOTOPENDB_SSD,     SU_RCTYPE_FATAL,    "DBE_ERR_CANNOTOPENDB_SSD",
"Cannot open database file: %s error: %s (%d)\n\
\n\
Possible causes for this error include:\n\
- the SOLID process does not have correct access rights to this file."
},

{ DBE_ERR_DBCORRUPTED,          SU_RCTYPE_FATAL,    "DBE_ERR_DBCORRUPTED",
"The database is irrevocably corrupted. Please revert to the latest backup."
},

{ DBE_ERR_WRONGHEADERVERS_D,      SU_RCTYPE_FATAL,    "DBE_ERR_WRONGHEADERVERS",
"Database version (%d) does not match with SOLID version.\n\
\n\
Possible causes for this error include:\n\
- too old version of SOLID is used with this database\n\
- the database has been corrupted"
},

{ DBE_ERR_DATABASEFILEFORMAT_D,   SU_RCTYPE_FATAL,    "DBE_ERR_DATABASEFILEFORMAT",
"Database version (%d) does not match with SOLID version.\n\
\n\
Possible causes for this error include:\n\
- too old version of SOLID is used with this database\n\
- the database has been corrupted"
},

{ DBE_ERR_CANTRECOVERREADONLY_SS, SU_RCTYPE_FATAL, "DBE_ERR_CANTRECOVERREADONLY_SS",
"Can't perform roll-forward recovery in read-only mode.\n\
\n\
Read-only mode can be specified in 3 different ways.\n\
To restart SOLID in normal mode, please verify that:\n\
- SOLID process is not started with command-line option -x read-only\n\
- solid.ini doesn't contain the following parameter setting:\n\
\n\t[%s]%s=yes\n\n\
- license file does not have read-only limitation"
},

{ DBE_ERR_OUTOFCACHEBLOCKS_SS,    SU_RCTYPE_FATAL, "DBE_ERR_OUTOFCACHEBLOCKS_SS",
"Out of database cache memory blocks. SOLID process cannot continue\n\
because there is too little cache memory allocated for the SOLID\n\
process.\n\
\n\
Typical cause for this problem is heavy load from several\n\
concurrent users.\n\
\n\
To allocate more cache memory, set solid.ini parameter:\n\
\n\t[%s]\n\t%s=<cache size in bytes>\n\n\
to a higher value.\n\
\n\
NOTE: Allocated cache memory size should not exceed the \n\
amount of physical memory."
},

{ DBE_ERR_LOGFILEWRITEFAILURE_SU, SU_RCTYPE_FATAL, "DBE_ERR_LOGFILEWRITEFAILURE_SU",
"Failed to write to log file '%s' at offset: %lu.\n\
\n\
Please verify that the disk containing the log files is not full\n\
and is functioning properly.  Also, log files should not be stored\n\
on shared disks over the network."
},

{ DBE_ERR_LOGFILEALREADYEXISTS_S, SU_RCTYPE_FATAL, "DBE_ERR_LOGFILEALREADYEXISTS_S",
"Cannot create new logfile '%s' because such a file already exists in the\n\
log file directory.\n\
\n\
Probably your log file directory contains also logs from some other database.\n\
SOLID process cannot continue until invalid logfiles are removed from the\n\
log file directory."
},

{ DBE_ERR_ILLLOGFILETEMPLATE_SSSDD, SU_RCTYPE_FATAL, "DBE_ERR_ILLLOGFILETEMPLATE_SSSDD",
"Error: Illegal log file name template. Most likely the log file name template\n\
specified in solid.ini:\n\
\n\t[%s]%s=%s\n\n\
contains too few or too many sequence number digit positions.\n\
There should be at least %d and at most %d digit positions."
},

{ DBE_ERR_ILLLOGCONF_SSD, SU_RCTYPE_FATAL, "DBE_ERR_ILLLOGCONF_SSD",
"Unknown log write mode. Please, re-check configuration parameter\n\
\n\t[%s]%s=%d"
},

{ DBE_ERR_CANTOPENLOG_SSSSS, SU_RCTYPE_FATAL, "DBE_ERR_CANTOPENLOG_SSS",
"Cannot open log file: %s. Please check the log files directory and \n\
log file name template in solid.ini:\n\
\n\t[%s]%s\n\
\t[%s]%s\n\n\
and verify that:\n\
- log files directory exists\n\
- log file name template can be expanded into a valid file name in this\n\
  environment\n\
- the server process has appropriate privileges to the log files directory."
},

{ DBE_ERR_OLDLOGFILE_S, SU_RCTYPE_FATAL, "DBE_ERR_OLDLOGFILE_S",
"Cannot create database because old log file %s exists in the\n\
log files directory.\n\
\n\
Possibly the database has been deleted without deleting the log files\n\
or there are log files from some other database in the log files directory\n\
of the database to be created."
},

{ DBE_ERR_ILLBLOCKSIZE_UUSSSU, SU_RCTYPE_FATAL, "DBE_ERR_ILLBLOCKSIZE_UUSSSU",
"Roll-forward recovery cannot be performed because the configured log\n\
file block size: %lu does not match with block size: %lu\n\
of existing log file: %s.\n\
\n\
To enable recovery, please edit solid.ini to include parameter setting:\n\
\n\t[%s]%s=%lu\n\n\
and restart the SOLID process.\n\
\n\
After successfull recovery, the log file block size can be changed by:\n\
- shutting down SOLID process, then\n\
- removing old log files, then\n\
- editing new block size into solid.ini, and\n\
- restarting SOLID"
},

{ DBE_ERR_RELIDNOTFOUND_D, SU_RCTYPE_FATAL, "DBE_ERR_RELIDNOTFOUND_D",
"Roll-forward recovery failed because relation id = %ld was not found.\n\
Database has been irrevocably corrupted. Please restore the database\n\
from the latest backup."
},

{ DBE_ERR_RELNAMENOTFOUND_S, SU_RCTYPE_FATAL, "DBE_ERR_RELNAMENOTFOUND_S",
"Roll-forward recovery failed because relation name '%s' was not found.\n\
Database has been irrevocably corrupted. Please restore the database\n\
from the latest backup."
},

{ DBE_ERR_DBCORRUPTED_S, SU_RCTYPE_FATAL, "DBE_ERR_DBCORRUPTED_S",
"%s\nPlease restore the database from the latest backup."
},

{ DBE_ERR_FILEIOPROBLEM_S, SU_RCTYPE_FATAL, "DBE_ERR_FILEIOPROBLEM_S",
"%s\nDatabase operation failed because of the file I/O problem."
},

{ DBE_ERR_ILLINDEXBLOCK_DLSD, SU_RCTYPE_FATAL, "DBE_ERR_ILLINDEXBLOCK_DLSD",
"Database is inconsistent.\n\
Illegal index block type %d, addr %ld, routine %s, reachmode %d\n\
\n\
Please restore the database from the latest backup."
},

{ DBE_ERR_ROLLFWDFAILED, SU_RCTYPE_FATAL, "DBE_ERR_ROLLFWDFAILED",
"Roll-forward recovery failed.\n\
\n\
Please revert to the latest backup."
},

{ DBE_ERR_WRONGBLOCKSIZE_SSD,   SU_RCTYPE_FATAL, "DBE_ERR_WRONGBLOCKSIZE_SSD",
"The database you are trying to use has been originally created\n\
with a different database block size settings than your current settings.\n\
\n\
Please edit the solid.ini file to contain the following parameter setting:\n\
\n\t[%s]%s=%ld"
},

#ifdef SS_MYSQL
{ DBE_WARN_WRONGBLOCKSIZE_SSD,   SU_RCTYPE_WARNING, "DBE_WARN_WRONGBLOCKSIZE_SSD",
"The database you are using has been originally\n\
created with a different database block size setting than your current\n\
setting.\n\
To suppress this warning edit the option (configuration) file to contain\n\
the following parameter setting:\n\
\n\t[%s]%s=%ld"
},
#else
{ DBE_WARN_WRONGBLOCKSIZE_SSD,   SU_RCTYPE_WARNING, "DBE_WARN_WRONGBLOCKSIZE_SSD",
"The database you are using has been originally\n\
created with a different database block size setting than your current\n\
setting.\n\
To suppress this warning edit the solid.ini file to contain\n\
the following parameter setting:\n\
\n\t[%s]%s=%ld"
},
#endif

{ DBE_ERR_REDEFINITION_SSS,   SU_RCTYPE_FATAL,    "DBE_ERR_REDEFINITION_SSS",
"Roll-forward recovery failed, because %s '%s' is \n\
redefined in log file '%s'. Possible causes for this error include:\n\
- another SOLID process is using the same log file directory\n\
- old log file are present in the log file directory\n\
\n\
SOLID process cannot use corrupted log file to recover."
},
{ DBE_ERR_NOBASECATALOGGIVEN, SU_RCTYPE_FATAL, "DBE_ERR_NOBASECATALOGGIVEN",
  "No system catalog given for database conversion (use -C catalogname)"
},

{ DBE_ERR_USERROLLBACK,         SU_RCTYPE_ERROR, "DBE_ERR_USERROLLBACK",
  "User rolled back the transaction"    /* Internal code. */
},

{ DBE_ERR_CANNOTREMOVEFILESPEC, SU_RCTYPE_ERROR, "DBE_ERR_CANNOTREMOVEFILESPEC",
  "Cannot remove filespec. File is already in use."
},

{ DBE_ERR_HSBSECSERVERNOTINSYNC_D, SU_RCTYPE_ERROR, "DBE_ERR_HSBSECSERVERNOTINSYNC_D",
  "HotStandby Secondary server can not execute operation received \n\
from Primary server (error %d).\n\
\n\
Possible causes for this error include:\n\
- database did not originate from the Primary server using HotStandby \n\
  copy or netcopy command; \n\
- both servers have been Primary servers simultaneously;\n\
- the database has been corrupted;\n\
\n\
To continue, reinitialize the database from Primary server using HotStandby \n\
copy or netcopy command"
},

{ DBE_ERR_BACKUP_ABORTED,       SU_RCTYPE_ERROR,    "DBE_ERR_BACKUP_ABORTED",
  "Backup aborted." },


{ DBE_ERR_CPDISABLED, SU_RCTYPE_ERROR, "DBE_ERR_CPDISABLED",
  "Checkpointing is disabled." },

{ DBE_ERR_ABORTHSBTRXFAILED,       SU_RCTYPE_ERROR,    "DBE_ERR_ABORTHSBTRXFAILED",
  "Failed to abort hsb trx because commit is already sent to secondary." },

{ DBE_ERR_DELETEROWNOTFOUND,       SU_RCTYPE_ERROR,    "DBE_ERR_DELETEROWNOTFOUND",
      "Deleted row not found." },

{ DBE_ERR_LOCKTIMEOUTTOOLARGE_DD,      SU_RCTYPE_ERROR,    "DBE_ERR_LOCKTIMEOUTTOOLARGE_DD",
      "Specified lock timeout %ld is too large, maximum is %ld." },

{ DBE_ERR_HSBPRIMARYUNCERTAIN,      SU_RCTYPE_ERROR,    "DBE_ERR_HSBPRIMARYUNCERTAIN",
      "Operation failed, server is in HSB primary uncertain mode." },

{ DBE_ERR_DDOPNEWERTRX,         SU_RCTYPE_ERROR,    "DBE_ERR_DDOPNEWERTRX",
      "Data dictionary operation in a newer transaction." },

{ DBE_ERR_NOLOGGINGWITHHSB,      SU_RCTYPE_FATAL,   "DBE_ERR_NOLOGGINGWITHHSB",
      "HotStandby cannot operate when logging is disabled." },

{ DBE_ERR_MIGRATEHSB_WITHNOHSB,      SU_RCTYPE_FATAL,   "DBE_ERR_MIGRATEHSB_WITHNOHSB",
      "HotStandby migration is not possible if Hotstandby is not configured." },
/* FIXME^: typo: Hotstandby -> HotStandby */

{ DBE_ERR_TOOFEWMMEPAGES_DD,    SU_RCTYPE_FATAL,    "DBE_ERR_TOOFEWMMEPAGES",
      "Only %d cache pages configured for M-table usage, at least %d needed." },

{ DBE_ERR_NOTENOUGHMMEMEM_DD,    SU_RCTYPE_FATAL,    "DBE_ERR_NOTENOUGHMMEMEM",
      "Only %d kilobytes configured for M-table checkpointing, at least %dKB is needed." },

{ DBE_ERR_NODBPASSWORD,         SU_RCTYPE_FATAL,    "DBE_ERR_NODBPASSWORD",
      "Encryption password has not been given for encrypted database." },
{ DBE_ERR_WRONGPASSWORD,        SU_RCTYPE_FATAL,    "DBE_ERR_WRONGPASSWORD",
      "Incorrect password has been given for encrypted database." },
{ DBE_ERR_UNKNOWN_CRYPTOALG,    SU_RCTYPE_FATAL,    "DBE_ERR_UNKNOWN_CRYPTOALG",
      "Unknown encryption algorithm." },
{ DBE_ERR_NOTMYSQLDATABASEFILE, SU_RCTYPE_FATAL,    "DBE_ERR_NOTMYSQLDATABASEFILE",
    "Database is not created using solidDB for MySQL. Cannot open database." },

{ MME_ERR_VALUE_TOO_LARGE,   SU_RCTYPE_ERROR,  "MME_ERR_VALUE_TOO_LARGE",
      "New row value too large for M-table." },

{ MME_ERR_NO_BLOB_SUPPORT,      SU_RCTYPE_ERROR,    "MME_ERR_NO_BLOB_SUPPORT",
      "BLObs are not supported in M-tables." },

{ MME_ERR_NO_SERIALIZABLE,      SU_RCTYPE_ERROR,    "MME_ERR_NO_SERIALIZABLE",
      "Serializable isolation level is not supported in M-tables." },

{ MME_ERR_OUTOFMEMINSTARTUP,    SU_RCTYPE_FATAL, "MME_ERR_OUTOFMEMINSTARTUP",
      "Too small configured [MME]ImdbMemoryLimit to start server." },

{ MME_ERR_MEMORY_LOW,           SU_RCTYPE_ERROR,  "MME_ERR_MEMORY_LOW",
"Memory for M-tables is running low, inserts to M-tables disallowed." },
{ MME_ERR_MEMORY_BOTTOM,        SU_RCTYPE_ERROR,  "MME_ERR_MEMORY_BOTTOM",
"Ran out of memory for M-tables,\
 updates and inserts to M-tables disallowed." },
{ MME_RC_MEMORY_BACKTONORMAL,   SU_RCTYPE_MSG,    "MME_RC_MEMORY_BACKTONORMAL",
"M-table operations now have enough memory for normal service."},
{ MME_RC_MEMORY_BACKTOLOW,      SU_RCTYPE_MSG, "MME_RC_MEMORY_BACKTOLOW",
"M-table operations now have enough memory for updates,\
 inserts still disallowed." },

{ DBE_ERR_CURSORISOLATIONCHANGE,    SU_RCTYPE_ERROR, "DBE_ERR_CURSORISOLATIONCHANGE",
 "Cursor is closed after isolation change." }

};

#endif /* SS_NOERRORTEXT */

/*##**********************************************************************\
 *
 *              dbe_error_init
 *
 * Adds database error texts to the global error text system.
 *
 * Parameters :
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_error_init(void)
{
#ifndef SS_NOERRORTEXT

        su_rc_addsubsys(
            SU_RC_SUBSYS_DBE,
            dbe_rc_texts,
            sizeof(dbe_rc_texts) / sizeof(dbe_rc_texts[0]));

#endif /* SS_NOERRORTEXT */
}

/*##**********************************************************************\
 *
 *              dbe_fatal_error
 *
 * Reports fatal database error. Prints error with 'static' header
 * text "Database file is inconsistent..." (DBE_ERR_DSCORRUPTED_S)
 * or "...file I/O problem" (DBE_ERR_FILEIOPROBLEM_S) and appends
 * optional additional info (rc) to this text.
 *
 *
 * Parameters :
 *
 *      file - in
 *              Source file name.
 *
 *      line - in
 *              Source line number.
 *
 *      rc - in
 *              Error code of additional info, or zero if no error code needed.
 *          NOTE: additional info text is not allowed to have arguments
 *                for example SU_ERR_xxxx_SS is not allowed.
 *
 * Return value :
 *
 * Comments :
 *
 *      This function never returns.
 *
 * Globals used :
 *
 * See also :
 */
void dbe_fatal_error(
        char* file,
        int line,
        dbe_ret_t rc)
{
        char buf[256];
        char addinfo[256];

        /* Check error code to give more meaningful error text.
         */
        switch (rc) {
            case SU_ERR_FILE_OPEN_FAILURE:
            case SU_ERR_FILE_WRITE_FAILURE:
            case SU_ERR_FILE_WRITE_DISK_FULL:
            case SU_ERR_FILE_WRITE_CFG_EXCEEDED:
            case SU_ERR_FILE_READ_FAILURE:
            case SU_ERR_FILE_READ_EOF:
            case SU_ERR_FILE_READ_ILLEGAL_ADDR:
            case SU_ERR_FILE_LOCK_FAILURE:
            case SU_ERR_FILE_UNLOCK_FAILURE:
            case SU_ERR_TOO_LONG_FILENAME:
            case SU_ERR_DUPLICATE_FILENAME:

                su_rc_adderrortext(buf, rc);
                SsSprintf(addinfo, "(%d) %s", rc, buf);

            su_emergency_exit(
                        file,
                        line,
                        DBE_ERR_FILEIOPROBLEM_S,
                        addinfo);

                break;

            default:
                if (rc == 0) {
                    SsSprintf(addinfo, "The database is irrevocably corrupted.");
                } else {

                    su_rc_adderrortext(buf, rc);
                    SsSprintf(addinfo, "(%d) %s", rc, buf);
                }
            su_emergency_exit(
                        file,
                        line,
                        DBE_ERR_DBCORRUPTED_S,
                        addinfo);

                break;
        }
}

/*##**********************************************************************\
 *
 *              dbe_fileio_error
 *
 * Reports fatal file-io error.
 *
 *
 * Parameters :
 *
 *      file - in
 *              Source file name.
 *
 *      line - in
 *              Source line number.
 *
 *      rc - in
 *              Error code of additional info, or zero if no error code needed.
 *          NOTE: additional info text is not allowed to have arguments
 *                for example SU_ERR_xxxx_SS is not allowed.
 *
 * Return value :
 *
 * Comments :
 *
 *
 * Globals used :
 *
 * See also :
 */
void dbe_fileio_error(
        char* file,
        int line,
        dbe_ret_t rc)
{
        char buf[256];
        char addinfo[256];

        /* Check error code to give more meaningful error text.
         */
        switch (rc) {
            case SU_ERR_FILE_OPEN_FAILURE:
            case SU_ERR_FILE_WRITE_FAILURE:
            case SU_ERR_FILE_WRITE_DISK_FULL:
            case SU_ERR_FILE_WRITE_CFG_EXCEEDED:
            case SU_ERR_FILE_READ_FAILURE:
            case SU_ERR_FILE_READ_EOF:
            case SU_ERR_FILE_READ_ILLEGAL_ADDR:
            case SU_ERR_FILE_LOCK_FAILURE:
            case SU_ERR_FILE_UNLOCK_FAILURE:
            case SU_ERR_TOO_LONG_FILENAME:
            case SU_ERR_DUPLICATE_FILENAME:

                su_rc_adderrortext(buf, rc);
                SsSprintf(addinfo, "(%d) %s", rc, buf);

            su_emergency_exit(
                        file,
                        line,
                        DBE_ERR_FILEIOPROBLEM_S,
                        addinfo);

                break;

            default:
                break;
        }
}
