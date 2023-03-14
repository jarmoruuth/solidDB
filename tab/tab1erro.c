/*************************************************************************\
**  source       * tab0erro.c
**  directory    * tab
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

#include <su0error.h>

#ifndef SS_NOERRORTEXT

static su_rc_text_t tb_rc_texts[] = {
{ E_ILLCHARCONST_S,          SU_RCTYPE_ERROR,   "E_ILLCHARCONST_S",
  "Illegal character constant %.80s" },

{ E_NOCHARARITH,             SU_RCTYPE_ERROR,   "E_NOCHARARITH",
  "Type CHAR not allowed for arithmetics" },

{ E_AGGRNOTORD_S,            SU_RCTYPE_ERROR,   "E_AGGRNOTORD_S",
  "Aggregate function %.80s not available for ordinary call" },

{ E_ILLAGGRPRM_S,            SU_RCTYPE_ERROR,   "E_ILLAGGRPRM_S",
  "Illegal aggregate function %.80s parameter" },

{ E_NOSUMAVGCHAR,            SU_RCTYPE_ERROR,   "E_NOSUMAVGCHAR",
  "SUM and AVG not supported for CHAR type" },

{ E_NOSUMAVGDATE,            SU_RCTYPE_ERROR,   "E_NOSUMAVGDATE",
  "SUM or AVG not supported for DATE type" },

{ E_FUNCNODEF_S,             SU_RCTYPE_ERROR,   "E_FUNCNODEF_S",
  "Function %.80s is not defined" },

{ E_ILLADDPRM_S,             SU_RCTYPE_ERROR,   "E_ILLADDPRM_S",
  "Illegal parameter %.80s to ADD function" },

{ E_DIVBYZERO,               SU_RCTYPE_ERROR,   "E_DIVBYZERO",
  "Division by zero" },

{ __NOTUSED_E_DBERROR,       SU_RCTYPE_RETCODE, "",
  "" },

{ E_RELNOTEXIST_S,           SU_RCTYPE_ERROR,   "E_RELNOTEXIST_S",
  "Table %.80s does not exist" },

{ __NOTUSED_E_RELCANNOTOPEN, SU_RCTYPE_RETCODE, "",
  "" },

{ E_RELEXIST_S,              SU_RCTYPE_ERROR,   "E_RELEXIST_S",
  "Table name %.80s conflicts with an existing entity" },

{ E_KEYNOTEXIST_S,           SU_RCTYPE_ERROR,   "E_KEYNOTEXIST_S",
  "Index %.80s does not exist" },

{ E_ATTRNOTEXISTONREL_SS,    SU_RCTYPE_ERROR,   "E_ATTRNOTEXISTONREL_SS",
  "Column %.80s does not exist on table %.80s" },

{ __NOTUSED_E_USERNOTEXIST, SU_RCTYPE_RETCODE,  "",
  "" },

{ __NOTUSED_E_X_Y_FAILED_RC,SU_RCTYPE_RETCODE,  "",
  "" },

{ E_JOINRELNOSUP,            SU_RCTYPE_ERROR,   "E_JOINRELNOSUP",
  "Join table is not supported" },

{ E_TRXSPNOSUP,              SU_RCTYPE_ERROR,   "E_TRXSPNOSUP",
  "Transaction savepoints are not supported" },

{ E_DEFNOSUP,                SU_RCTYPE_ERROR,   "E_DEFNOSUP",
  "Default values are not supported" },

{ __NOTUSED_E_FORKEYNOSUP,   SU_RCTYPE_ERROR,   "",
  "" },

{ E_DESCKEYNOSUP,            SU_RCTYPE_ERROR,   "E_DESCKEYNOSUP",
  "Descending keys are not supported" },

{ E_SCHEMANOSUP,             SU_RCTYPE_ERROR,   "E_SCHEMANOSUP",
  "Schema is not supported" },

{ __NOTUSED_E_GRANTNOSUP,    SU_RCTYPE_RETCODE, "",
  "" },

{ E_UPDNOCUR,                SU_RCTYPE_ERROR,   "E_UPDNOCUR",
  "Update through a cursor with no current row" },

{ E_DELNOCUR,                SU_RCTYPE_ERROR,   "E_DELNOCUR",
  "Delete through a cursor with no current row" },

{ __NOTUSED_E_NOTRX,         SU_RCTYPE_RETCODE, "",
  "" },

{ E_VIEWNOTEXIST_S,          SU_RCTYPE_ERROR,   "E_VIEWNOTEXIST_S",
  "View %.80s does not exist" },

{ E_VIEWEXIST_S,             SU_RCTYPE_ERROR,   "E_VIEWEXIST_S",
  "View name %.80s conflicts with an existing entity" },

{ E_INSNOTVAL_S,             SU_RCTYPE_ERROR,   "E_INSNOTVAL_S",
  "No value specified for NOT NULL column %.80s" },

{ E_DDOP,                    SU_RCTYPE_ERROR,   "E_DDOP",
  "Data dictionary operation is active for accessed table or index" },

{ E_ILLTYPE_S,               SU_RCTYPE_ERROR,   "E_ILLTYPE_S",
  "Illegal type %.80s" },

{ E_ILLTYPEPARAM_SS,         SU_RCTYPE_ERROR,   "E_ILLTYPEPARAM_SS",
  "Illegal parameter %.80s for type %.80s" },

{ E_ILLCONST_S,              SU_RCTYPE_ERROR,   "E_ILLCONST_S",
  "Illegal constant %.80s" },

{ E_ILLINTCONST_S,           SU_RCTYPE_ERROR,   "E_ILLINTCONST_S",
  "Illegal INTEGER constant %.80s" },

{ E_ILLDECCONST_S,           SU_RCTYPE_ERROR,   "E_ILLDECCONST_S",
  "Illegal DECIMAL constant %.80s" },

{ E_ILLDBLCONST_S,           SU_RCTYPE_ERROR,   "E_ILLDBLCONST_S",
  "Illegal DOUBLE PREC constant %.80s" },

{ E_ILLREALCONST_S,          SU_RCTYPE_ERROR,   "E_ILLREALCONST_S",
  "Illegal REAL constant %.80s" },

{ E_ILLASSIGN_SS,            SU_RCTYPE_ERROR,   "E_ILLASSIGN_SS",
  "Illegal assignment from type %.80s to type %.80s" },

{ E_AGGRNODEF_S,             SU_RCTYPE_ERROR,   "E_AGGRNODEF_S",
  "Aggregate function %.80s is not defined" },

{ E_NODATEARITH,             SU_RCTYPE_ERROR,   "E_NODATEARITH",
  "Type DATE not allowed for arithmetics" },

{ E_NODFLPOWARITH,           SU_RCTYPE_ERROR,   "E_NODFLPOWARITH",
  "Power arithmetic not allowed for NUMERIC and DECIMAL data type" },

{ E_ILLDATECONST_S,          SU_RCTYPE_ERROR,   "E_ILLDATECONST_S",
  "Illegal date/time/timestamp constant '%.80s'"  },

{ __NOTUSED_E_CASCADEOPNOSUP,SU_RCTYPE_RETCODE, "",
  "" },

{ __NOTUSED_E_PRIVREFNOSUP,  SU_RCTYPE_ERROR,   "",             /* v.2.00 */
  "" },

{ E_ILLUSERNAME_S,           SU_RCTYPE_ERROR,   "E_ILLUSERNAME_S",
  "Illegal user name %.80s" },

{ E_NOPRIV,                  SU_RCTYPE_ERROR,   "E_NOPRIV",
  "No privileges for operation" },

{ E_NOGRANTOPTONTAB_S,       SU_RCTYPE_ERROR,   "E_NOGRANTOPTONTAB_S",
  "No grant option privilege for entity %.80s" },

{ E_COLGRANTOPTNOSUP,        SU_RCTYPE_ERROR,   "E_COLGRANTOPTNOSUP",
  "Column privileges cannot be granted with grant option" },

{ E_TOOLONGCONSTR,          SU_RCTYPE_ERROR,   "E_TOOLONGCONSTR_S",
  "Too long constraint value" },

{ E_ILLCOLNAME_S,            SU_RCTYPE_ERROR,   "E_ILLCOLNAME_S",
  "Illegal column name %.80s" },

{ E_ILLPSEUDOCOLRELOP,      SU_RCTYPE_ERROR,   "E_ILLPSEUDOCOLRELOP",
  "Illegal comparison operator for a pseudo column" },

{ E_ILLPSEUDOCOLDATATYPE,   SU_RCTYPE_ERROR,   "E_ILLPSEUDOCOLDATATYPE",
  "Illegal data type for a pseudo column" },

{ E_ILLPSEUDOCOLDATA,       SU_RCTYPE_ERROR,   "E_ILLPSEUDOCOLDATA",
  "Illegal pseudo column data, maybe data is not received using pseudo column" },

{ E_NOUPDPSEUDOCOL,         SU_RCTYPE_ERROR,   "E_NOUPDPSEUDOCOL",
  "Update not allowed on pseudo column" },

{ E_NOINSPSEUDOCOL,         SU_RCTYPE_ERROR,   "E_NOINSPSEUDOCOL",
  "Insert not allowed on pseudo column" }, 

{ E_KEYNAMEEXIST_S,          SU_RCTYPE_ERROR,   "E_KEYNAMEEXIST_S",
  "Index %.80s already exists" },   

{ E_CONSTRCHECKFAIL_S,       SU_RCTYPE_ERROR,   "E_CONSTRCHECKFAIL_S",
  "Constraint checks were not satisfied on column %.80s" },  

{ E_SYSNAME_S,               SU_RCTYPE_ERROR,   "E_SYSNAME_S",
  "Reserved system name %.80s" },    

{ E_USERNOTFOUND_S,          SU_RCTYPE_ERROR,   "E_USERNOTFOUND_S",
  "User name %.80s not found" }, 

{ E_ROLENOTFOUND_S,          SU_RCTYPE_ERROR,   "E_ROLENOTFOUND_S",
  "Role name %.80s not found" }, 

{ E_ADMINOPTNOSUP,           SU_RCTYPE_ERROR,   "E_ADMINOPTNOSUP",
  "Admin option is not supported" },

{ E_NAMEEXISTS_S,            SU_RCTYPE_ERROR,   "E_NAMEEXISTS_S",
  "Name %.80s already exists" },

{ E_NOTUSER_S,               SU_RCTYPE_ERROR,   "E_NOTUSER_S",
  "Not a valid user name %.80s" },   

{ E_NOTROLE_S,               SU_RCTYPE_ERROR,   "E_NOTROLE_S",
  "Not a valid role name %.80s" },   

{ E_USERNOTFOUNDINROLE_SS,   SU_RCTYPE_ERROR,   "E_USERNOTFOUNDINROLE_SS",
  "User %.80s not found in role %.80s" },   

{ E_TOOSHORTPASSWORD,        SU_RCTYPE_ERROR,   "E_TOOSHORTPASSWORD",
  "Too short password" },

{ E_SHUTDOWNINPROGRESS,      SU_RCTYPE_ERROR,   "E_SHUTDOWNINPROGRESS",
  "Shutdown is in progress" },

{ __NOTUSED_E_INCRNOSUP,     SU_RCTYPE_RETCODE, "",
  "" },

{ E_NUMERICOVERFLOW,         SU_RCTYPE_ERROR,   "E_NUMERICOVERFLOW",
  "Numerical overflow" },

{ E_NUMERICUNDERFLOW,        SU_RCTYPE_ERROR,   "E_NUMERICUNDERFLOW",
  "Numerical underflow" },

{ E_NUMERICOUTOFRANGE,       SU_RCTYPE_ERROR,   "E_NUMERICOUTOFRANGE",
  "Numerical value out of range" },

{ E_MATHERR,                 SU_RCTYPE_ERROR,   "E_MATHERR",
  "Math error" },

{ E_ILLPASSWORD,             SU_RCTYPE_ERROR,   "E_ILLPASSWORD",
  "Illegal password" },

{ E_ILLROLENAME_S,           SU_RCTYPE_ERROR,   "E_ILLROLENAME_S",
  "Illegal role name %.80s" },   

#if 0
{ E_ILLNULLALLOWED_S,        SU_RCTYPE_ERROR,   "E_ILLNULLALLOWED_S",
  "NOT NULL must not be specified for added column %.80s" },
#endif

{ E_LASTCOLUMN,             SU_RCTYPE_ERROR,   "E_LASTCOLUMN",
  "Column can not be dropped, at least one column must remain in the table" }, 

{ E_ATTREXISTONREL_SS,       SU_RCTYPE_ERROR,   "E_ATTREXISTONREL_SS",
  "Column %.80s already exists on table %.80s" },    

{ E_ILLCONSTR,              SU_RCTYPE_ERROR,    "E_ILLCONSTR",
  "Illegal search constraint" },

{ E_INCOMPATMODIFY_SSS,     SU_RCTYPE_ERROR,    "E_INCOMPATMODIFY_SSS",
  "Incompatible types, can not modify column %.80s from type %.80s to type %.80s" },

{ E_DESCBINARYNOSUP,        SU_RCTYPE_ERROR,    "E_DESCBINARYNOSUP",
  "Descending keys are not supported for binary columns" },     /* v.1.21 */

{ E_FUNPARAMASTERISKNOSUP_S,SU_RCTYPE_ERROR,    "E_FUNPARAMASTERISKNOSUP_S",
  "Function %.80s: parameter * not supported" },                   /* v.1.21 */

{ E_FUNPARAMTOOFEW_S,   SU_RCTYPE_ERROR,        "E_FUNPARAMTOOFEW_S",
  "Function %.80s: Too few parameters" },                          /* v.1.21 */

{ E_FUNPARAMTOOMANY_S,      SU_RCTYPE_ERROR,    "E_FUNPARAMTOOMANY_S",
  "Function %.80s: Too many parameters" },                         /* v.1.21 */

{ E_FUNCFAILED_S,           SU_RCTYPE_ERROR,    "E_FUNCFAILED_S",
  "Function %.80s: Run-time failure" },                            /* v.1.21 */

{ E_FUNPARAMTYPEMISMATCH_SD,SU_RCTYPE_ERROR,    "E_FUNPARAMTYPEMISMATCH_SD",
  "Function %.80s: type mismatch in parameter #%d" },              /* v.1.21 */

{ E_FUNPARAMILLVALUE_SD,    SU_RCTYPE_ERROR,    "E_FUNPARAMILLVALUE_SD",
  "Function %.80s: illegal value in parameter #%d" },              /* v.1.21 */

{ E_NOPRIMKEY_S,            SU_RCTYPE_ERROR,    "E_NOPRIMKEY_S",
  "No primary key for table '%.80s'" },                                                            /* v.1.21 */

{ __NOTUSED_E_INVALIDPRIMKEY_S,SU_RCTYPE_ERROR, "",
  "" },                                                            /* v.1.21 */

{ E_FORKINCOMPATDTYPE_S,    SU_RCTYPE_ERROR,    "E_FORKINCOMPATDTYPE_S",
  "Foreign key column %.80s data type not compatible with referenced column data type" }, /* v.1.21 */

{ E_FORKNOTUNQK,            SU_RCTYPE_ERROR,    "E_FORKNOTUNQK",
  "Foreign key does not match to the primary key or unique constraint of the referenced table" }, /* v.1.21 */

{ E_EVENTEXISTS_S,          SU_RCTYPE_ERROR,    "E_EVENTEXISTS_S",
  "Event name %.80s conflicts with an existing entity" },

{ E_EVENTNOTEXIST_S,        SU_RCTYPE_ERROR,    "E_EVENTNOTEXIST_S",
  "Event %.80s does not exist" },                                  /* v.1.21 */

{ E_PRIMKDUPCOL_S,          SU_RCTYPE_ERROR,    "E_PRIMKDUPCOL_S",
  "Duplicate column %.80s in primary key definition" },            /* v.1.21 */

{ E_UNQKDUPCOL_S,           SU_RCTYPE_ERROR,    "E_UNQKDUPCOL_S",
  "Duplicate column %.80s in unique constraint definition" },                 /* v.1.21 */

{ E_INDEXDUPCOL_S,          SU_RCTYPE_ERROR,    "E_INDEXDUPCOL_S",
  "Duplicate column %.80s in index definition" },                  /* v.1.21 */

{ E_DUPLICATEINDEX,          SU_RCTYPE_ERROR,    "E_DUPLICATEINDEX",
  "Duplicate index definition" },                  /* v.1.21 */

  { E_PRIMKCOLNOTNULL,        SU_RCTYPE_ERROR,    "E_PRIMKCOLNOTNULL",
  "Primary key columns must be NOT NULL" },                        /* v.1.21 */

{ E_UNQKCOLNOTNULL,         SU_RCTYPE_ERROR,    "E_UNQKCOLNOTNULL",
  "Unique constraint columns must be NOT NULL" },                  /* v.1.21 */

{ E_NOREFERENCESPRIV_S,     SU_RCTYPE_ERROR,    "E_NOREFERENCESPRIV_S",
  "No REFERENCES privileges to referenced columns in table %.80s" }, /* v.1.21 */

{ E_ILLTABMODECOMB,         SU_RCTYPE_ERROR,    "E_ILLTABMODECOMB",
  "Illegal table mode combination" },                                /* v.1.21 */

{ E_PROCONLYEXECUTE,        SU_RCTYPE_ERROR,    "E_PROCONLYEXECUTE",
  "Only execute privileges can be used with procedures" },           /* v.2.00 */

{ E_EXECUTEONLYPROC,        SU_RCTYPE_ERROR,    "E_EXECUTEONLYPROC",
  "Execute privileges can be used only with procedures" },           /* v.2.00 */

{ E_ILLGRANTORREVOKE,       SU_RCTYPE_ERROR,    "E_ILLGRANTORREVOKE",
  "Illegal grant or revoke operation" },                             /* v.2.00 */

{ E_SEQEXISTS_S,            SU_RCTYPE_ERROR,    "E_SEQEXISTS_S",
  "Sequence name %.80s conflicts with an existing entity" },

{ E_SEQNOTEXIST_S,          SU_RCTYPE_ERROR,    "E_SEQNOTEXIST_S",
  "Sequence %.80s does not exist" },                                 /* v.2.00 */

{ E_FORKEYREFEXIST_S,       SU_RCTYPE_ERROR,    "E_FORKEYREFEXIST_S",
  "Foreign key reference exists to table '%.80s'" },                 /* v.2.00 */

{ E_ILLSETOPER,             SU_RCTYPE_ERROR,    "E_ILLSETOPER",
  "Illegal set operation" },                                         /* v.2.00 */

{ E_CMPTYPECLASH_SS,        SU_RCTYPE_ERROR,    "E_CMPTYPECLASH_SS",
  "Comparison between incompatible types %.80s and %.80s" },

{ E_SCHEMAOBJECTS,          SU_RCTYPE_ERROR,    "E_SCHEMAOBJECTS",
  "There are schema objects for this user, drop failed" },

{ E_NULLNOTALLOWED_S,       SU_RCTYPE_ERROR,    "E_NULLNOTALLOWED_S",
  "NULL value given for NOT NULL column %.80s" },

{ E_AMBIGUOUS_S,            SU_RCTYPE_ERROR,    "E_AMBIGUOUS_S",
  "Ambiguous entity name %.80s" },

{ E_MMINOFORKEY,            SU_RCTYPE_ERROR,    "E_MMINOFORKEY",
  "Foreign keys are not supported with main memory tables" },

{ E_ILLEGALARITH_SS,        SU_RCTYPE_ERROR,    "E_ILLEGAL_ARITH_SSS",
  "Illegal arithmetics between types %.80s and %.80s" },
                            
{ E_NOBLOBARITH,            SU_RCTYPE_ERROR,    "E_NOBLOBARITH",
  "String operations are not allowed on values stored as BLObs or CLObs" },

#ifdef SS_UNICODE_DATA
{ E_FUNPARAM_NOBLOBSUPP_SD,SU_RCTYPE_ERROR,   "E_FUNPARAM_NOBLOBSUPP_SD",
  "Function %.80s: Too long value (stored as CLOb) in parameter #%d" },
#endif /* SS_UNICODE_DATA */

{ E_DUPCOL_S,               SU_RCTYPE_ERROR,    "E_DUPCOL_S",
  "Column %.80s specified more than once" },

{ E_WRONGNUMOFPARAMS,       SU_RCTYPE_ERROR,    "E_WRONGNUMOFPARAMS",
  "Wrong number of parameters" },

{ E_COLPRIVONLYFORTAB,      SU_RCTYPE_ERROR,    "E_COLPRIVONLYFORTAB",
  "Column privileges are supported only for base tables" },

{ E_TYPESNOTUNIONCOMPAT_SS, SU_RCTYPE_ERROR,    "E_TYPESNOTUNIONCOMPAT_SS",
  "Types %s and %s are not union compatible" },

{ E_TOOLONGNAME_S,          SU_RCTYPE_ERROR,    "E_TOOLONGNAME_S",
  "Too long entity name '%.80s'" },

{ E_TOOMANYCOLS_D,          SU_RCTYPE_ERROR,    "E_TOOMANYCOLS_D",
  "Too many columns, max number of columns is %d" },

{ E_SYNCHISTREL,            SU_RCTYPE_ERROR,    "E_SYNCHISTREL",
  "Operation is not supported for a table with sync history" },

{ E_RELNOTEMPTY_S,          SU_RCTYPE_ERROR,    "E_RELNOTEMPTY_S",
  "Table '%.80s' is not empty" },

{ E_USERIDNOTFOUND_D,       SU_RCTYPE_ERROR,    "E_USERIDNOTFOUND_D",
  "User id %ld not found" },

{ E_ILLEGALLIKEPAT_S,       SU_RCTYPE_ERROR,    "E_ILLEGALLIKEPAT_S",
  "Illegal LIKE pattern '%.80s'" },

{ E_ILLEGALLIKETYPE_S,      SU_RCTYPE_ERROR,    "E_ILLEGALLIKETYPE_S",
  "Illegal type (%.80s) for LIKE pattern" },

{ E_CMPFAILEDDUETOBLOB,     SU_RCTYPE_ERROR,    "E_CMPFAILEDDUETOBLOB",
  "Comparison failed because at least one of values was too long" },

{ E_LIKEFAILEDDUETOBLOBVAL, SU_RCTYPE_ERROR,    "E_LIKEFAILEDDUETOBLOBVAL",
  "LIKE predicate failed because value is too long" },

{ E_LIKEFAILEDDUETOBLOBPAT, SU_RCTYPE_ERROR,    "E_LIKEFAILEDDUETOBLOBPAT",
  "LIKE predicate failed because pattern is too long" },

{ E_ILLEGALLIKEESCTYPE_S,   SU_RCTYPE_ERROR,    "E_ILLEGALLIKEESCTYPE_S",
  "Illegal type '%.80s' for LIKE ESCAPE character" },

{ E_TOOMANYNESTEDTRIG,      SU_RCTYPE_ERROR,    "E_TOOMANYNESTEDTRIG",
  "Too many nested triggers" },

{ E_TOOMANYNESTEDPROC,      SU_RCTYPE_ERROR,    "E_TOOMANYNESTEDPROC",
  "Too many nested procedures or functions" },

{ E_INVSYNCLIC,             SU_RCTYPE_ERROR,    "E_INVSYNCLIC",
  "Not a valid license for this product" },

{ E_NOTBASETABLE,           SU_RCTYPE_ERROR,    "E_NOTBASETABLE",
  "Operation is allowed only for base tables" },

{ E_ESTARITHERROR,          SU_RCTYPE_ERROR,    "E_ESTARITHERROR",
  "Internal error, arithmetic error in estimator" },

{ E_TRANSNOTACT,            SU_RCTYPE_ERROR,    "E_TRANSNOTACT",
  "Internal error, transaction is not active" },

{ E_ILLGRANTMODE,           SU_RCTYPE_ERROR,    "E_ILLGRANTMODE",
  "Illegal grant/revoke mode" },

{ E_HINTKEYNOTFOUND_S,      SU_RCTYPE_ERROR,    "E_HINTKEYNOTFOUND_S",
  "Index %.80s given in index hint does not exist" },

{ E_CATNOTEXIST_S,          SU_RCTYPE_ERROR,    "E_CATNOTEXIST_S",
  "Catalog %.80s does not exist" },

{ E_CATEXIST_S,             SU_RCTYPE_ERROR,    "E_CATEXIST_S",
  "Catalog %.80s already exists" },

{ E_SCHEMANOTEXIST_S,       SU_RCTYPE_ERROR,    "E_SCHEMANOTEXIST_S",
  "Schema %.80s does not exist" },

{ E_SCHEMAEXIST_S,          SU_RCTYPE_ERROR,    "E_SCHEMAEXIST_S",
  "Schema %.80s already exists" },

{ E_SCHEMAISUSER_S,         SU_RCTYPE_ERROR,    "E_SCHEMAISUSER_S",
  "Schema %.80s is an existing user" },

{ E_TRIGILLCOMMITROLLBACK,  SU_RCTYPE_ERROR,    "E_TRIGILLCOMMITROLLBACK",
  "Commit and rollback are not allowed inside trigger" },

{ E_SYNCPARAMNOTFOUND,      SU_RCTYPE_ERROR,    "E_SYNCPARAMNOTFOUND",
  "Sync parameter not found" },

{ E_CATALOGOBJECTS,         SU_RCTYPE_ERROR,    "E_CATALOGOBJECTS",
  "There are schema objects for this catalog, drop failed" },

{ E_NOCURCATDROP,           SU_RCTYPE_ERROR,    "E_NOCURCATDROP",
  "Current catalog can not be dropped" },

{ E_SCHEMAOBJECTS_S,        SU_RCTYPE_ERROR,    "E_SCHEMAOBJECTS_S",
  "There are %.80s for this schema, drop failed" },

{ E_CATALOGOBJECTS_S,       SU_RCTYPE_ERROR,    "E_CATALOGOBJECTS_S",
  "There are %.80s for this catalog, drop failed" },

{ E_CREIDXNOSAMECATSCH,     SU_RCTYPE_ERROR,    "E_CATALOGOBJECTS_S",
  "Index can be created only into same catalog and schema as the base table" },

{ E_CANNOTDROPUNQCOL_S,     SU_RCTYPE_ERROR,    "E_CANNOTDROPUNQCOL_S",
  "Can not drop a column that is part of primary or unique key %s" },

{ E_USEROBJECTS_S,          SU_RCTYPE_ERROR,    "E_USEROBJECTS_S",
  "There are %.80s for this user, drop failed" },

{ E_LASTADMIN,              SU_RCTYPE_ERROR,    "E_LASTADMIN",
  "Can not remove last administrator" },

{ E_BLANKNAME,              SU_RCTYPE_ERROR,    "E_BLANKNAME",
  "Name cannot be an empty string" },

{ E_MMIONLYFORDISKLESS,     SU_RCTYPE_ERROR,    "E_MMIONLYFORDISKLESS",
  "Main memory tables are supported only in Diskless configuration" },


{ E_ATTREXISTONVIEW_SS,       SU_RCTYPE_ERROR,   "E_ATTREXISTONVIEW_SS",
  "Column %.80s already exists on view %.80s" },     

{ E_NOCURSCHEDROP,          SU_RCTYPE_ERROR,     "E_NOCURSCHEDROP",
  "Current schema cannot be dropped" },

{ E_NOCURUSERROP,           SU_RCTYPE_ERROR,  "E_NOCURUSERROP",
  "Current user cannot be dropped" },

{ E_TABLEHASTRIGREF_D,   SU_RCTYPE_ERROR,  "E_E_TABLEHASTRIGREF_D",
  "Cannot alter table name because it is referenced in %d trigger(s)" },

{ E_MMEUPDNEEDSFORUPDATE,       SU_RCTYPE_ERROR,  "E_MMEUPDNEEDSFORUPDATE",
  "Update failed, M-tables require FOR UPDATE" },

{ E_MMEDELNEEDSFORUPDATE,       SU_RCTYPE_ERROR,  "E_MMEDELNEEDSFORUPDATE",
  "Delete failed, M-tables require FOR UPDATE" },

{ E_READCOMMITTEDUPDNEEDSFORUPDATE,       SU_RCTYPE_ERROR,  "E_READCOMMITTEDUPDNEEDSFORUPDATE",
  "Update failed, READ COMMITTED isolation requires FOR UPDATE" },

{ E_READCOMMITTEDDELNEEDSFORUPDATE,       SU_RCTYPE_ERROR,  "E_READCOMMITTEDDELNEEDSFORUPDATE",
  "Delete failed, READ COMMITTED isolation requires FOR UPDATE" },

{ E_DESCBIGINTNOSUP,        SU_RCTYPE_ERROR,    "E_DESCBIGINTNOSUP",
  "Descending keys are not supported for bigint columns" },

{ E_TRANSACTIVE,            SU_RCTYPE_ERROR,    "E_TRANSACTIVE",
  "Transaction is active, operation failed" },

{ E_MMENOPREV,            SU_RCTYPE_ERROR,    "E_MMENOPREV",
  "Fetch previous is not supported for M-tables" },

{ E_MMENOLICENSE,         SU_RCTYPE_ERROR,    "E_MMENOLICENSE",
  "License does not allow accessing M-tables" },

{ E_DBENOLICENSE,         SU_RCTYPE_ERROR,    "E_DBENOLICENSE",
  "License does not allow creating D-tables" },

{ E_TRANSIENTONLYFORMME,    SU_RCTYPE_ERROR,    "E_TRANSIENTONLYFORMME",
  "Only M-tables can be transient" },

{ E_TRANSIENTNOTEMPORARY,   SU_RCTYPE_ERROR,    "E_TRANSIENTNOTEMPORARY",
  "Transient tables can not be set temporary" },

{ E_TEMPORARYNOTRANSIENT,   SU_RCTYPE_ERROR,    "E_TEMPORARYNOTRANSIENT",
  "Temporary tables can not be set transient" },

{ E_TEMPORARYONLYFORMME,    SU_RCTYPE_ERROR,    "E_TEMPORARYONLYFORMME",
  "Only M-tables can be temporary" },

{ E_MMEILLFORKEY,         SU_RCTYPE_ERROR,    "E_MMEILLFORKEY",
  "Foreign key constraints between D- and M-tables are not supported" },

{ E_REGULARREFERENCESTRANSIENT, SU_RCTYPE_ERROR, "E_REGULARREFERENCESTRANSIENT",
  "A persistent table can not reference a transient table" },

{ E_REGULARREFERENCESTEMPORARY, SU_RCTYPE_ERROR, "E_REGULARREFERENCESTEMPORARY",
  "A persistent table can not reference a temporary table" },

{ E_TRANSIENTREFERENCESTEMPORARY, SU_RCTYPE_ERROR, "E_TRANSIENTREFERENCESTEMPORARY",
  "A transient table can not reference a temporary table" },

{ E_REFERENCETEMPNONTEMP,   SU_RCTYPE_ERROR,    "E_REFERENCETEMPNONTEMP",
  "A reference between temporary and non-temporary table is not allowed" },

{ E_CANNOTCHANGESTOREIFSYNCHIST, SU_RCTYPE_ERROR, "E_CANNOTCHANGESTOREIFSYNCHIST",
  "Cannot change STORE for a table with sync history." },

{ E_UNQKDUP_COND, SU_RCTYPE_ERROR, "E_UNQKDUP_COND",
  "Cannot define UNIQUE constraint with duplicated or implied restriction." },

{ E_CONSTRAINT_NOT_FOUND_S, SU_RCTYPE_ERROR, "E_CONSTRAINT_NOT_FOUND_S",
  "Constraint by name '%.80s' not found." },

{ E_REF_ACTION_NOT_SUPPORTED, SU_RCTYPE_ERROR, "E_REF_ACTION_NOT_SUPPORTED",
  "Foreign key actions other than restrict are not supported." },

{ E_CONSTRAINT_NAME_CONFLICT_S, SU_RCTYPE_ERROR, "E_CONSTRAINT_NAME_CONFLICT_S",
  "Constraint name '%.80s' already exists." },

{ E_CONSTRAINT_CHECK_FAIL, SU_RCTYPE_ERROR, "E_CONSTRAINT_CHECK_FAIL",
  "Constraint check fails on existing data." },

{ E_NOTNULLWITHOUTDEFAULT, SU_RCTYPE_ERROR, "E_NOTNULLWITHOUTDEFAULT",
  "Added column with NOT NULL must have a non-NULL default." },

{ E_INDEX_IS_USED_S, SU_RCTYPE_ERROR, "E_INDEX_IS_USED_S",
  "Index is referenced by foreign key %s, it cannot be dropped." },

{ E_PRIMKEY_NOTDEF_S, SU_RCTYPE_ERROR, "E_PRIMKEY_NOTDEF_S",
  "Primary key not found for table '%s'. Cannot define foreign key." },

{ E_NULL_EXISTS_S, SU_RCTYPE_ERROR, "E_NULL_EXISTS_S",
  "Cannot set NOT NULL on column '%s' that already has NULL value." },

{ E_NULL_CANNOTDROP_S, SU_RCTYPE_ERROR, "E_NULL_CANNOTDROP_S",
  "Cannot drop NOT NULL on column '%s' that is used as part of unique key." },

{ E_MMENOTOVERCOMMIT,   SU_RCTYPE_ERROR,    "E_MMENOTOVERCOMMIT",
  "The cursor cannot continue accessing M-tables after the transaction has committed or aborted.\
 The statement must be re-executed." },

{ E_FORKEY_SELFREF, SU_RCTYPE_ERROR, "E_FORKEY_SELFREF",
  "Foreign key refers to itself." },

{ E_FORKEY_LOOPDEP, SU_RCTYPE_ERROR, "E_FORKEY_LOOPDEP",
  "Foreign key creates update dependency loop." },

{ E_MMENOPOSITION,  SU_RCTYPE_ERROR,    "E_MMENOPOSITION",
  "Positioning is not supported for M-tables." },

{ E_FATAL_DEFFILE_SSSS, SU_RCTYPE_FATAL,  "E_FATAL_DEFFILE_SSSS",
  "Definition\n[%s]\n%s=%s\nin file %s is not valid.\n" },

{ E_FATAL_PARAM_SSSSS, SU_RCTYPE_FATAL,  "E_FATAL_DEFFILE_SSSSS",
  "Parameter setting\n[%s]\n%s=%s\nin file %s conflicts\nwith the setting in database: %s.\n" },

{ E_FATAL_GENERIC_S,   SU_RCTYPE_FATAL,  "E_FATAL_GENERIC_S",
  "%s\n" },

{ E_CANNOTDROPFORKEYCOL_S, SU_RCTYPE_ERROR, "E_CANNOTDROPFORKEYCOL_S",
  "Can not drop a column that is part of a foreign key %s" },

{ E_MMENOSUP, SU_RCTYPE_ERROR, "E_MMENOSUP",
  "M-tables are not supported" },

{ E_FUNCILLCOMMITROLLBACK,  SU_RCTYPE_ERROR,    "E_FUNCILLCOMMITROLLBACK",
  "Commit and rollback are not allowed inside function" },

{ E_UPDNEEDSFORUPDATE,       SU_RCTYPE_ERROR,  "E_UPDNEEDSFORUPDATE",
  "Update failed, used isolation level requires FOR UPDATE" },

{ E_DELNEEDSFORUPDATE,       SU_RCTYPE_ERROR,  "E_DELNEEDSFORUPDATE",
  "Delete failed, used isolation level requires FOR UPDATE" },

{ E_TC_ISOLATION,            SU_RCTYPE_ERROR,  "E_TC_ISOLATION",
  "Cluster connection does not support isolation levels higher than READ COMMITED" },

{ E_TC_ONLY,                 SU_RCTYPE_ERROR,  "E_TC_ONLY",
  "SET WRITE command makes sense only for TC connection" },

/*
 * Warnings
 */

{ RS_WARN_STRINGTRUNC_SS,   SU_RCTYPE_WARNING,  "RS_WARN_STRINGTRUNC_SS",
  "String data truncation in assignment from %.80s to %.80s" },

{ RS_WARN_NUMERICTRUNC_SS,  SU_RCTYPE_WARNING,  "RS_WARN_NUMERICTRUNC_SS",
  "Numeric value right truncation in assignment from %.80s to %.80s" }

};


#endif /* SS_NOERRORTEXT */

/*##**********************************************************************\
 * 
 *		tb_error_init
 * 
 * Adds tab and res error texts to the global error text system.
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
void tb_error_init(void)
{
#ifndef SS_NOERRORTEXT

        su_rc_addsubsys(
            SU_RC_SUBSYS_TAB,
            tb_rc_texts,
            sizeof(tb_rc_texts) / sizeof(tb_rc_texts[0]));

#endif /* SS_NOERRORTEXT */
}
