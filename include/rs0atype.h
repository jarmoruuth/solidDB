/*************************************************************************\
**  source       * rs0atype.h
**  directory    * res
**  description  * Attribute type functions
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


#ifndef RS0ATYPE_H
#define RS0ATYPE_H

#include <ssenv.h>
#include <ssdebug.h>

#include <su0bflag.h>

#ifdef SS_COLLATION
#include <su0collation.h>
#endif /* SS_COLLATION */

#include "rs0types.h"

#define RS_INTERNAL

/* Internal data types supported.
 * WARNING! When changing these, check routine rs_atype_checktypes.
 */
typedef enum rsdatatypeenum {

        RSDT_CHAR    = 0,
        RSDT_INTEGER = 1,
        RSDT_FLOAT   = 2,
        RSDT_DOUBLE  = 3,
        RSDT_DATE    = 4,
        RSDT_DFLOAT  = 5,
        RSDT_BINARY  = 6,
        RSDT_UNICODE = 7,
        RSDT_BIGINT  = 8 /* Added 21.1.2002 tommiv */
} rs_datatype_t;

#define RSDT_DIMENSION ((size_t)RSDT_BIGINT + 1)

/* Types declared in SQL CLI, values MUST match the ones in
 * cli0cli.h and cli0ext1.h !
 * WARNING! When changing these, check routine rs_atype_checktypes.
 */
typedef enum rssqldatatypeenum {
        RSSQLDT_WLONGVARCHAR    = -10,
        RSSQLDT_WVARCHAR        = -9,
        RSSQLDT_WCHAR           = -8,

        RSSQLDT_BIT             = -7,
        RSSQLDT_TINYINT         = -6,
        RSSQLDT_BIGINT          = -5,
        RSSQLDT_LONGVARBINARY   = -4,
        RSSQLDT_VARBINARY       = -3,
        RSSQLDT_BINARY          = -2,
        RSSQLDT_LONGVARCHAR     = -1,

        RSSQLDT_CHAR            = 1, 
        RSSQLDT_NUMERIC         = 2,
        RSSQLDT_DECIMAL         = 3,
        RSSQLDT_INTEGER         = 4, 
        RSSQLDT_SMALLINT        = 5,
        RSSQLDT_FLOAT           = 6, 
        RSSQLDT_REAL            = 7,
        RSSQLDT_DOUBLE          = 8,
        RSSQLDT_DATE            = 9,
        RSSQLDT_TIME            = 10,
        RSSQLDT_TIMESTAMP       = 11,
        RSSQLDT_VARCHAR         = 12

} rs_sqldatatype_t;

typedef enum {
        RS_MYSQLTYPE_NONE = 0,
        RS_MYSQLTYPE_VAR_STRING,
        RS_MYSQLTYPE_VARCHAR,
        RS_MYSQLTYPE_STRING,
        RS_MYSQLTYPE_NEWDECIMAL,
        RS_MYSQLTYPE_LONG,
        RS_MYSQLTYPE_LONGLONG,
        RS_MYSQLTYPE_DATE,
        RS_MYSQLTYPE_YEAR,
        RS_MYSQLTYPE_NEWDATE,
        RS_MYSQLTYPE_TIME,
        RS_MYSQLTYPE_TIMESTAMP,
        RS_MYSQLTYPE_DATETIME,
        RS_MYSQLTYPE_TINY,
        RS_MYSQLTYPE_SHORT,
        RS_MYSQLTYPE_INT24,
        RS_MYSQLTYPE_FLOAT,
        RS_MYSQLTYPE_DOUBLE,
        RS_MYSQLTYPE_DECIMAL,
        RS_MYSQLTYPE_GEOMETRY,
        RS_MYSQLTYPE_TINY_BLOB,
        RS_MYSQLTYPE_MEDIUM_BLOB,
        RS_MYSQLTYPE_BLOB,
        RS_MYSQLTYPE_LONG_BLOB,
        RS_MYSQLTYPE_BIT
} rs_mysqldatatype_t;

/* Note: if you add a new sqldatatype, you must also update
 * this value:
 */
#define RS_SQLDATATYPES_COUNT  22

/* The reference attribute types supported.
 * WARNING! When changing these, check routine rs_atype_checktypes.
 */
typedef enum rsattrtypeenum {

        RSAT_USER_DEFINED,
        RSAT_TUPLE_ID,
        RSAT_TUPLE_VERSION,
        RSAT_TRX_ID,
        RSAT_FREELY_DEFINED,
        RSAT_CLUSTER_ID,
        RSAT_RELATION_ID,
        RSAT_KEY_ID,
        RSAT_REMOVED,           /* Removed by ALTER TABLE */
        RSAT_SYNC,              /* Sync tuple version or ispubl */
        RSAT_UNDEFINED,
#ifdef SS_COLLATION
        RSAT_COLLATION_KEY      /* generated collation weight, only used as
                                 * key part, never in atype
                                 */
#endif /* SS_COLLATION */

} rs_attrtype_t;

typedef enum {
        RSPM_IN,
        RSPM_OUT,
        RSPM_INOUT
} rs_attrparammode_t;

#if defined(RS_USE_MACROS) || defined(RS_INTERNAL)

typedef struct {
        uint        st_sqltype;
        const char* st_sqlname;
        int         st_rstype;
        ulong       st_defaultlen;
        ulong       st_defaultscale;
        bool        st_copyconvert;
} rs_atypeinfo_t;

extern const rs_atypeinfo_t rs_atype_types[];

#define ATYPE_OFFSET    10

#define ATYPE_TYPES(index)  ((rs_atype_types+ATYPE_OFFSET)[(int)(index)])

/* A bit of a chicken and egg problem here: atype now contains two avals.
   Must know the abstract rs_aval_t* type here. */
#ifndef SQLFINST_T_DEFINED
typedef struct sqlfinststruct rs_aval_t;
#endif

/* type for attribute type.
 * Note: for space efficiency most components have been put
 * to 1-byte fields, even if the conceptual type may be an
 * enumerated type. This is because some compilers treat enums
 * as ints regardless of the value range of the enum
 */
struct rsattrtypestruct {

        ss_debug(int     at_check;)      /* rs_check_t check field */

        ss_int4_t        at_len;         /* length or precision of the field */

        ss_int1_t        at_scale;       /* number of digits after decimal point */
        /* rs_sqldatatype_t */
        ss_int1_t        at_sqldatatype; /* RSSQLDT_    */
        /* rs_attrtype_t */
        ss_uint1_t       at_attrtype;    /* RSAT_       */
        ss_uint1_t       at_flags;       /* AT_NULLALLOWED,AT_PSEUDO,AT_SYNC */

        ss_uint4_t       at_autoincseqid;/* identification of auto_increment sequence
                                            if defined for this attribute */

        void*           at_originaldefault;
        void*           at_currentdefault;
#ifdef SS_COLLATION
        su_collation_t* at_collation; 
#endif /* SS_COLLATION */
        /* rs_mysqldatatype_t */
        ss_uint1_t      at_mysqldatatype;
        ss_int4_t       at_extlen;      /* external length or precision of the field */
        ss_int4_t       at_extscale;    /* external length or precision of the field */
}; /* rs_atype_t */

/* Boolean flags for atype */
#define AT_NULLALLOWED      (1 << 0)
#define AT_PSEUDO           (1 << 1)
#define AT_SYNC             (1 << 2)
#define AT_PARAM_IN         (1 << 3)
#define AT_PARAM_OUT        (1 << 4)
#define AT_AUTO_INC         (1 << 5)

#define AT_PARAM_INOUT      (AT_PARAM_IN|AT_PARAM_OUT)

#define _RS_ATYPE_ISLEGALSQLDT_(cd, sqldt) \
        (   (sqldt) >= -(ATYPE_OFFSET) \
         && (sqldt) <= RSSQLDT_VARCHAR \
         && ATYPE_TYPES(sqldt).st_rstype >= 0)

#define _RS_ATYPE_ATTRTYPE_(cd, atype) \
        ((rs_attrtype_t) (atype)->at_attrtype)

#define _RS_ATYPE_DATATYPE_(cd, at) \
        ((rs_datatype_t)(int)ATYPE_TYPES((at)->at_sqldatatype).st_rstype)

#define _RS_ATYPE_SQLDATATYPE_(cd, at) \
        ((rs_sqldatatype_t)(int)(at)->at_sqldatatype)

#define _RS_ATYPE_ISUSERDEFINED_(cd,at) \
        ((at)->at_attrtype == RSAT_USER_DEFINED)

#define _RS_ATYPE_COPYCONVERT_(cd, at) \
        (ATYPE_TYPES((at)->at_sqldatatype).st_copyconvert)

#define _RS_ATYPE_SQLDTTODT_(cd, sqldt) \
        (ATYPE_TYPES(sqldt).st_rstype)

#define _RS_ATYPE_LENGTH_(cd,at) \
        ((at)->at_len)

#define RS_ATYPE_INITBUFTOUNDEF(cd,atype) \
        do { \
            (atype)->at_attrtype = RSAT_UNDEFINED; \
            (atype)->at_originaldefault = NULL; \
            (atype)->at_currentdefault = NULL; \
        } while (FALSE)

#define RS_ATYPE_BUFISUNDEF(cd,atype) \
        ((atype)->at_attrtype == RSAT_UNDEFINED)

#ifdef SS_DEBUG

/* In debug version use the function. */
#define RS_ATYPE_DATATYPE   rs_atype_datatype
#define RS_ATYPE_SQLDATATYPE rs_atype_sqldatatype
#define RS_ATYPE_ISUSERDEFINED rs_atype_isuserdefined
#define RS_ATYPE_LENGTH rs_atype_length
#define RS_ATYPE_SQLDTTODT rs_atype_sqldttodt

#else /* SS_DEBUG */

/* In no-debug version use the macro. */
#define RS_ATYPE_DATATYPE   _RS_ATYPE_DATATYPE_
#define RS_ATYPE_SQLDATATYPE _RS_ATYPE_SQLDATATYPE_
#define RS_ATYPE_ISUSERDEFINED _RS_ATYPE_ISUSERDEFINED_
#define RS_ATYPE_LENGTH _RS_ATYPE_LENGTH_
#define RS_ATYPE_SQLDTTODT _RS_ATYPE_SQLDTTODT_

#endif /* SS_DEBUG */

#define RS_ATYPE_ISLEGALSQLDT _RS_ATYPE_ISLEGALSQLDT_
#define dt_isnumber(dt) ((dt) == RSDT_INTEGER || (dt) == RSDT_FLOAT || \
                         (dt) == RSDT_DOUBLE  || (dt) == RSDT_DFLOAT || \
                         (dt) == RSDT_BIGINT)


#define dt_isstring(dt) ((dt) == RSDT_CHAR || (dt) == RSDT_UNICODE)


#endif /* defined(RS_USE_MACROS) || defined(RS_INTERNAL) */

#ifdef RS_USE_MACROS

#define rs_atype_attrtype(cd, atype) \
        _RS_ATYPE_ATTRTYPE_(cd, atype)

#define rs_atype_datatype(cd, atype) \
        _RS_ATYPE_DATATYPE_(cd, atype)

#else

rs_attrtype_t rs_atype_attrtype(
        void*       cd,
        rs_atype_t* atype);

rs_datatype_t rs_atype_datatype(
        void*       cd,
        rs_atype_t* atype);

#endif /* RS_USE_MACROS */


rs_atype_t* rs_atype_create(
        void*       cd,
        char*       type_name,
        char*       pars,
        bool        nullallowed,
        rs_err_t**  p_errh
);

void rs_atype_free(
        void*       cd,
        rs_atype_t* atype
);

rs_atype_t* rs_atype_copy(
        void*       cd,
        rs_atype_t* atype
);

void rs_atype_copybuf(
        rs_sysi_t*      cd,
        rs_atype_t*     dst_atype,
        rs_atype_t*     src_atype);

rs_atype_t* rs_atype_copymax(
        void* cd,
        rs_atype_t* atype);

rs_atype_t* rs_atype_createconst(
        void*       cd,
        char*       sqlvalstr,
        rs_err_t**  errhandle
);

#ifdef SS_COLLATION

SS_INLINE su_collation_t* rs_atype_collation(
        void* cd,
        rs_atype_t* atype);

SS_INLINE void rs_atype_setcollation(
        void* cd,
        rs_atype_t* atype,
        su_collation_t* collation);

SS_INLINE su_charset_t rs_atype_charset(
        void* cd __attribute__((unused)),
        rs_atype_t* atype);

#endif /* SS_COLLATION */

SS_INLINE ss_int1_t rs_atype_sqldatatype(
        void*       cd,
        rs_atype_t* atype
);

char* rs_atype_name(
        void*       cd,
        rs_atype_t* atype
);

ss_int4_t rs_atype_length(
        void*       cd,
        rs_atype_t* atype
);

ss_int1_t rs_atype_scale(
        void*       cd,
        rs_atype_t* atype
);

bool rs_atype_nullallowed(
        void*       cd,
        rs_atype_t* atype
);

void rs_atype_setnullallowed(
        void*       cd,
        rs_atype_t* atype,
        bool        nullallowed
);

SS_INLINE bool rs_atype_pseudo(
        void*       cd,
        rs_atype_t* atype
);

bool rs_atype_autoinc(
        void*       cd,
        rs_atype_t* atype
);

bool rs_atype_sql_pseudo(
        void*       cd,
        rs_atype_t* atype
);
       
bool rs_atype_syncpublinsert_pseudo(
        void*       cd,
        rs_atype_t* atype
);

void rs_atype_setpseudo(
        void*       cd,
        rs_atype_t* atype,
        bool        ispseudo
);

void rs_atype_setautoinc(
        void*       cd,
        rs_atype_t* atype,
        bool        isautoinc,
        long        seq_id
);

void rs_atype_setmysqldatatype(
        rs_sysi_t*  cd,
        rs_atype_t* atype,
        rs_mysqldatatype_t type
);

rs_mysqldatatype_t rs_atype_mysqldatatype(
        rs_sysi_t*  cd,
        rs_atype_t* atype
);

bool rs_atype_issync(
        void*       cd,
        rs_atype_t* atype
);

void rs_atype_setsync(
        void*       cd,
        rs_atype_t* atype,
        bool        issync
);

rs_attrparammode_t rs_atype_getparammode(
        void*       cd,
        rs_atype_t* atype
);

void rs_atype_setparammode(
        void*              cd,
        rs_atype_t*        atype,
        rs_attrparammode_t mode
);

char* rs_atype_pars(
        void*       cd,
        rs_atype_t* atype
);

bool rs_atype_comppos(
        void*       cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2
);

bool rs_atype_issame(
        void*       cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2
);

#ifdef REMOVED
bool rs_atype_arpos(
        void*       cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2
);
#endif

bool rs_atype_likepos(
        void*       cd,
        rs_atype_t* atype
);

/* NOTE: Following services are not members of SQL-funblock */

rs_atype_t* rs_atype_init(
        void*            cd,
        rs_attrtype_t    attrtype,
        rs_datatype_t    rsdatatype,
        rs_sqldatatype_t sqldatatype,
        long             len,
        long             scale,
        bool             nullallowed
);

rs_atype_t* rs_atype_init_sqldt(
        void* cd,
        rs_sqldatatype_t sqldatatype);

rs_atype_t* rs_atype_init_rsdt(
        void* cd,
        rs_datatype_t datatype);


rs_atype_t* rs_atype_initbysqldt(
        void* cd,
        rs_sqldatatype_t sqldt,
        long length,
        long scale);

bool rs_atype_checktypes(
	void*            cd,
	rs_attrtype_t    attrtype,
        rs_datatype_t    datatype,
        rs_sqldatatype_t sqldatatype
);

rs_atype_t* rs_atype_initchar(
                void* cd
);

rs_atype_t* rs_atype_initlongvarchar(
                void* cd
);

rs_atype_t* rs_atype_initbinary(
                void* cd
);
rs_atype_t* rs_atype_initlongvarbinary(
        void* cd
);
rs_atype_t* rs_atype_initlong(
                void* cd
);

rs_atype_t* rs_atype_initbigint(
                void* cd
);

rs_atype_t* rs_atype_initsmallint(
                void* cd
);

rs_atype_t* rs_atype_inittinyint(
                void* cd
);

rs_atype_t* rs_atype_initfloat(
                void* cd
);

rs_atype_t* rs_atype_initdouble(
                void* cd
);

rs_atype_t* rs_atype_initdfloat(
                void* cd
);

rs_atype_t* rs_atype_initdate(
                void* cd
);

rs_atype_t* rs_atype_inittime(
                void* cd
);

rs_atype_t* rs_atype_inittimestamp(
                void* cd
);

bool rs_atype_isuserdefined(
        void*       cd,
        rs_atype_t* atype
);

rs_datatype_t rs_atype_sqldttodt(
        void*       cd,
        rs_sqldatatype_t sqldatatype
);

int rs_atype_datatyperadix(
        void*         cd,
        rs_datatype_t datatype
);

char* rs_atype_sqldatatypename(
        void* cd,
        rs_atype_t* atype);

char* rs_atype_sqldatatypenamebydt(
        void* cd,
        rs_sqldatatype_t sqldt);

bool rs_atype_isnum(
        void* cd,
        rs_atype_t* atype);

char* rs_atype_getdefaultdtformat(
        void* cd,
        rs_atype_t* atype);

void rs_atype_sqltypelength(
        void*       cd,
        rs_atype_t* atype,
        ulong* out_bytelen,
        ulong* out_sqllen

);

long rs_atype_maxstoragelength(
        void*       cd,
        rs_atype_t* atype

);

rs_atype_t* rs_atype_union(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh);

char* rs_atype_givefullname(
        void* cd,
        rs_atype_t* atype);

void rs_atype_outputfullname(
        void* cd,
        rs_atype_t* atype,
        void (*outputfun)(void*, void*),
        void *outputpar);

rs_atype_t* rs_atype_defpar(
        void* cd);

#ifdef SS_DEBUG

char* rs_atype_attrtypename(
        void*         cd,
        rs_attrtype_t attrtype
);

void rs_atype_print(
        void*       cd,
        rs_atype_t* atype
);

#endif /* SS_DEBUG */

extern const ss_char_t RS_TN_BIGINT[];            
extern const ss_char_t RS_TN_BINARY[];            
extern const ss_char_t RS_TN_BIT[];               
extern const ss_char_t RS_TN_CHAR[];              
extern const ss_char_t RS_TN_CHARACTER[];         
extern const ss_char_t RS_TN_CHARACTER_VARYING[]; 
extern const ss_char_t RS_TN_CHAR_VARYING[]; 
extern const ss_char_t RS_TN_DATE[];              
extern const ss_char_t RS_TN_DEC[];               
extern const ss_char_t RS_TN_DECIMAL[];           
extern const ss_char_t RS_TN_DOUBLE_PRECISION[];  
extern const ss_char_t RS_TN_FLOAT[];             
extern const ss_char_t RS_TN_INT[];               
extern const ss_char_t RS_TN_INTEGER[];           
extern const ss_char_t RS_TN_LONG_VARBINARY[];    
extern const ss_char_t RS_TN_LONG_VARCHAR[];      
extern const ss_char_t RS_TN_NULL[];              
extern const ss_char_t RS_TN_NUMERIC[];           
extern const ss_char_t RS_TN_REAL[];              
extern const ss_char_t RS_TN_SMALLINT[];          
extern const ss_char_t RS_TN_TIME[];              
extern const ss_char_t RS_TN_TIMESTAMP[];         
extern const ss_char_t RS_TN_TINYINT[];           
extern const ss_char_t RS_TN_VARBINARY[];         
extern const ss_char_t RS_TN_VARCHAR[];

extern const ss_char_t RS_TN_WCHAR[];           
extern const ss_char_t RS_TN_WVARCHAR[];        
extern const ss_char_t RS_TN_LONG_WVARCHAR[];
extern const ss_char_t RS_TN_VARWCHAR[];
extern const ss_char_t RS_TN_LONG_VARWCHAR[];

rs_atype_t* rs_atype_chartouni(void* cd, rs_atype_t* atype);
rs_atype_t* rs_atype_unitochar(void* cd, rs_atype_t* atype);

#define rs_atype_comppos rs_atype_comppos_ext


bool rs_atype_comppos_ext(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2);

bool rs_atype_assignpos(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_atype_t* src_atype);

bool rs_atype_convertpos(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_atype_t* src_atype);

bool rs_atype_sqldtcanbeconvertedto(
        void* cd, 
        rs_sqldatatype_t sqldt_from,
        size_t* p_ntypes,
        rs_sqldatatype_t p_sqldt_to[RS_SQLDATATYPES_COUNT]);

#define RS_TN_SYSVARCHAR RS_TN_WVARCHAR
#define RSSQLDT_SYSVARCHAR RSSQLDT_WVARCHAR

rs_atype_t* rs_atype_initrowid(
        rs_sysi_t* cd);

rs_atype_t* rs_atype_initrowver(
        rs_sysi_t* cd,
        bool pseudop);

rs_atype_t* rs_atype_initrowflags(
        rs_sysi_t* cd);

rs_atype_t* rs_atype_initsynctuplevers(
        rs_sysi_t* cd,
        bool pseudop);

rs_atype_t* rs_atype_initsyncispubltuple(
        rs_sysi_t* cd,
        bool pseudop);

char* rs_atype_givecoltypename(
        rs_sysi_t* cd,
        rs_atype_t* atype);

void rs_atype_insertoriginaldefault(
        rs_sysi_t*      cd,
        rs_atype_t*     atype,
        void*           defval);

rs_aval_t* rs_atype_getoriginaldefault(
        rs_sysi_t*      cd,
        rs_atype_t*     atype);

long rs_atype_getautoincseqid(
        rs_sysi_t*      cd,
        rs_atype_t*     atype);

void rs_atype_insertcurrentdefault(
        rs_sysi_t*      cd,
        rs_atype_t*     atype,
        void*           defval);

rs_aval_t* rs_atype_getcurrentdefault(
        rs_sysi_t*      cd,
        rs_atype_t*     atype);

void rs_atype_releasedefaults(
        rs_sysi_t*      cd,
        rs_atype_t*     atype);

SS_INLINE void rs_atype_getextlenscale(
        void*       cd,
        rs_atype_t* atype,
        uint*       len,
        uint*       scale);

SS_INLINE void rs_atype_setextlenscale(
        void*       cd,
        rs_atype_t* atype,
        uint        len,
        uint        scale);

#define CHECK_ATYPE(at) {\
                            ss_dassert(SS_CHKPTR(at));\
                            ss_dassert((at)->at_check == RSCHK_ATYPE);\
                        }

#if defined(RS0ATYPE_C) || defined(SS_USE_INLINE)

#ifdef SS_COLLATION

SS_INLINE su_collation_t* rs_atype_collation(
        void* cd,
        rs_atype_t* atype)
{
        CHECK_ATYPE(atype);
        return (atype->at_collation);
}

SS_INLINE void rs_atype_setcollation(
        void* cd,
        rs_atype_t* atype,
        su_collation_t* collation)
{
        CHECK_ATYPE(atype);
        atype->at_collation = collation;
}

SS_INLINE su_charset_t rs_atype_charset(
        void* cd __attribute__((unused)),
        rs_atype_t* atype)
{
        su_charset_t cs = SUC_DEFAULT;

        CHECK_ATYPE(atype);
        if (!atype->at_collation) {
            return cs;
        }

        cs = su_collation_get_charset(atype->at_collation);
        return cs;
}

#endif /* SS_COLLATION */

/*##**********************************************************************\
 *
 *              rs_atype_sqldatatype
 *
 * Member of the SQL function block.
 * Returns an integer (one of RSSQLDT_) identifying the main type of an
 * attibute type object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 *      an integer identifying the SQL main type
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE ss_int1_t rs_atype_sqldatatype(
        void*       cd,
        rs_atype_t* atype)
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_sqldatatype\n", __FILE__));
        CHECK_ATYPE(atype);
        return (_RS_ATYPE_SQLDATATYPE_(cd, atype));
}

/*##**********************************************************************\
 *
 *              rs_atype_pseudo
 *
 * Checks if the atype is a pseudo atype.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool rs_atype_pseudo(
        void*       cd,
        rs_atype_t* atype)
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_pseudo\n", __FILE__));
        CHECK_ATYPE(atype);

        return (SU_BFLAG_TEST(atype->at_flags, AT_PSEUDO) != 0);
}


SS_INLINE void rs_atype_getextlenscale(
        void*       cd,
        rs_atype_t* atype,
        uint*       len,
        uint*       scale)
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_getextlenscale\n", __FILE__));
        CHECK_ATYPE(atype);

        *len = atype->at_extlen;
        *scale = atype->at_extscale;
}

SS_INLINE void rs_atype_setextlenscale(
        void*       cd,
        rs_atype_t* atype,
        uint        len,
        uint        scale)
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_getextlenscale\n", __FILE__));
        CHECK_ATYPE(atype);

        atype->at_extlen = len;
        atype->at_extscale = scale;
}

#endif /* defined(RS0ATYPE_C) || defined(SS_USE_INLINE) */

#endif /* RS0ATYPE_H */
