/*************************************************************************\
**  source       * su0vfil.c
**  directory    * su
**  description  * This file implements a virtual file handle.
**               * The real open file handles are a limited resource.
**               * That's why here is an LRU cache of open handles.
**               * Whenever access to virtual file handle is requested
**               * the virtual file is associated to a physical file
**               * handle.
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

This module implements a 'virtual file handle' system.
A virtual file handle looks like one open random access file
handle to the client. The implementation may, however,
close the file when it is not recently accessed and other files
are accessed. This implementation is needed because open file
handles are a limited resource in many operating systems.
The physically open file handles are cached with LRU algorithm.
the currently unaccessed open file handles are kept in a file handle
pool, which contains the LRU queue and bookkeeping about currently
accessed file handles and the persistent file handles so that the
configured limit of the physically open files is never exceeded.
A persistent file is a file that is physically open all the time.
A virtual file handle can be created as persistent at create time.
All stream type files (SS_FILE*) are persistent. Persistent files should
be used with caution, because they decrease the number of cacheable
file handles thus deteriorating performance if the open limit is low.

Limitations:
-----------

Between su_vfh_beginaccess() and su_vfh_endaccess() the same thread
MUST NOT call su_vfh_flush() or su_vfh_close() for the same virtual
file handle, because that would make thread sleep forever waiting
for itself to do the su_vfh_endaccess().

The static functions dealing with file handle pool MUST be
run in fhpool->fhp_mutex:ed sequence to support concurrency. Some of
them even exit and re-enter the fhp_mutex to wait for a new handle
to appear into the pool.

The code is not re-entrant, because the 'fhpool' is a static variable.
But this is not a real problem, because open file handles have
per-process or global limits, so other instances of file handle
pool would not make sense.

Error handling:
--------------

Possible error situations are returned to the client in the return value.

Objects used:
------------

Portable semaphore functions <sssem.h>
Portable file system functions <ssfile.h>
Doubly linked list <su0list.h>

Preconditions:
-------------

The global initialization for virtual file handles must be done.

Example:
-------

#include <su0vfil.h>

char str1[] = "huuhaa1\n";
char str2[] = "huuhaa2\n";

main()
{
        SsBFileT *bfilep;
        su_vfilh_t *p_vfh1, *p_vfh2; /* virtual file handles */
        void *p_pfh1, *p_pfh2;

        su_vfh_globalinit(1);      /* init. virt. file handles maxopen=1 */
        
        /* second parameter indicates persistency of handle */
        p_vfh1 = su_vfh_init("huuhaa1.tmp", FALSE, FALSE, 512);
        p_vfh2 = su_vfh_init("huuhaa2.tmp", FALSE, FALSE, 512);

        bfilep = su_vfh_beginaccess(p_vfh1, (void*)&p_pfh1);
            SsBWrite(bfilep, 0L, str1, sizeof(str1) - 1);
            SsBFlush(bfilep);
        su_vfh_endaccess(p_vfh1, p_pfh1);

        bfilep = su_vfh_beginaccess(p_vfh2, (void*)&p_pfh2);
            SsBWrite(bfilep, 0L, str2, sizeof(str2) - 1);
            SsBFlush(bfilep);
        su_vfh_endaccess(p_vfh2, p_pfh2);

        su_vfh_done(p_vfh1);
        su_vfh_done(p_vfh2);

        su_vfh_globaldone();
}

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssmem.h>
#include <sssem.h>
#include <ssdebug.h>
#include <ssthread.h>

#include "su0vfil.h"
#include "su0list.h"
#include "su0gate.h"
#include "su0mesl.h"

/* pool that serves as cache of open file handles */
struct file_handle_pool_st {
        uint        fhp_max;        /* max # of handles */
        uint        fhp_pers;       /* # of persistent handles */
        uint        fhp_accessed;   /* # of handles under access */
        su_list_t  *fhp_LRUpool;    /* handle LRU pool */
        SsSemT     *fhp_mutex;      /* mutex semaphore for fhpool*/
        su_meslist_t*     fhp_meslist;
        su_meslist_t      fhp_meslist_buf;
        su_meswaitlist_t* fhp_meswaitlist;
        int               fhp_nwait;
        su_pfilh_t *fhp_freepfhlist;/* list of free phys. file handles */
};
typedef struct file_handle_pool_st file_handle_pool_t;

/* virtual file handle */
struct su_vfilh_st {
        su_list_t  *vfh_used;       /* list of file handles in use */
        su_list_t  *vfh_unused;     /* list of file handles not in use */
        char       *vfh_pathname;   /* path needed for possible reopen */
        bool        vfh_persistent; /* TRUE if always >= 1 open */
        uint        vfh_flags;
        su_gate_t  *vfh_flushgate;  /* mutual/shared gate for flush/close/access */
        su_meslist_t*     vfh_meslist;
        su_meslist_t      vfh_meslist_buf;
        su_meswaitlist_t* vfh_meswaitlist;
        int               vfh_nwait;
        /* vfh_meslist and vfh_meswaitlist are for signaling waiting threads
        ** if handle is non-persistent it's always NULL
        */                            
        size_t      vfh_blocksize;
};

/* physical file handle structure */
struct su_pfilh_st {
        su_list_t      *pfh_LRU_list;       /* LRU queue pointer */
        su_list_node_t *pfh_LRU_lnode;      /* LRU queue node pointer */
        su_list_t      *pfh_share_list;     /* shared access list */
        su_list_node_t *pfh_share_lnode;    /* shared access list node */
        union {
            su_vfilh_t     *dad;            /* Dad vfh pointer */
            su_pfilh_t     *nextfree;       /* Next freed pfh pointer */
        } pfh_;
        SsBFileT       *pfh_fhandle;        /* pointer to binary file handle */
        bool            pfh_persistent;     /* TRUE if always open */
};

static file_handle_pool_t *fhpool = 0;
static uint latest_maxfiles = SS_MAXFILES_DEFAULT;

static void         su_vfh_puttousedlist(su_vfilh_t *p_vfh, su_pfilh_t *p_pfh);
static void         su_vfh_puttounusedlist(su_vfilh_t *p_vfh, su_pfilh_t *p_pfh);

static su_pfilh_t  *su_pfh_init(su_vfilh_t *p_vfh, su_pfilh_t *p_pfh);
static void         su_pfh_done(su_pfilh_t *p_pfh, bool freeflag);

static file_handle_pool_t 
                   *fhp_init(uint max);
static void         fhp_done(file_handle_pool_t *fhp);
static void         fhp_setmax(file_handle_pool_t *fhp, uint max);
static su_pfilh_t  *fhp_incpers(file_handle_pool_t *fhp);
static void         fhp_decpers(file_handle_pool_t *fhp);
static su_pfilh_t  *fhp_extractLRU(file_handle_pool_t *fhp);
static void         fhp_remove(file_handle_pool_t *fhp, su_pfilh_t *p_pfh);
static void         fhp_addtoLRUqueue(file_handle_pool_t *fhp, 
                                      su_pfilh_t *p_pfh);

#define VFH_GLOBALINIT_MAYBE() \
{\
        if (fhpool == NULL) {\
            su_vfh_globalinit(latest_maxfiles);\
        }\
}

/*##**********************************************************************\
 * 
 *      su_vfp_init_txt
 * 
 * Creates a virtual file pointer for text file.
 * (remains open until done).
 * 
 * Parameters : 
 * 
 *  pathname - in, use
 *		file name
 *
 *  flags - in, use
 *		string specifying open mode 't' is unnecessary,
 *          'b' is forbidden
 *
 * 
 * Return value - give :
 *          'virtual file pointer' or
 *          NULL on error
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
su_vfile_t *su_vfp_init_txt(pathname, flags)
        char *pathname;
        char *flags;
{
        SS_FILE *fp;
        su_pfilh_t *p_pfh;

        VFH_GLOBALINIT_MAYBE();
        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        p_pfh = fhp_incpers(fhpool);
        if (p_pfh != NULL) {
            su_pfh_done(p_pfh, FALSE);
        }
        fp = SsFOpenT(pathname, flags);
        if (fp == NULL) {
            fhp_decpers(fhpool);
        }
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        return (su_vfile_t *)fp;
}

/*##**********************************************************************\
 * 
 *      su_vfp_init_bin
 * 
 * Creates a virtual file pointer for binary file.
 * (remains open until done).
 * 
 * Parameters : 
 * 
 *  pathname - in, use
 *		file name
 *
 *  flags - in, use
 *		string specifying open mode 'b' is unnecessary,
 *          't' is forbidden
 *
 * 
 * Return value - give :
 *          'virtual file pointer' or
 *          NULL on error
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
su_vfile_t *su_vfp_init_bin(pathname, flags)
        char *pathname;
        char *flags;
{
        SS_FILE *fp;
        su_pfilh_t *p_pfh;

        VFH_GLOBALINIT_MAYBE();
        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        p_pfh = fhp_incpers(fhpool);
        if (p_pfh != NULL) {
            su_pfh_done(p_pfh, FALSE);
        }
        fp = SsFOpenB(pathname, flags);
        if (fp == NULL) {
            fhp_decpers(fhpool);
        }
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        return (su_vfile_t *)fp;
}

/*##**********************************************************************\
 * 
 *      su_vfp_done
 * 
 * Deletes a virtual file pointer and closes file.
 * 
 * Parameters : 
 * 
 *  vfp - in, take
 *		virtual file pointer
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
void su_vfp_done(vfp)
        su_vfile_t *vfp;
{
        ss_dassert(vfp != NULL);
        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        fhp_decpers(fhpool);
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        SsFClose((SS_FILE*)vfp);
}

/*##**********************************************************************\
 * 
 *      su_vfp_access
 * 
 * Grants FILE * mode access to virtual file pointer
 * 
 * Parameters : 
 * 
 *      vfp - in, use
 *		virtual file pointer
 * 
 * Return value - ref :
 *          SS_FILE* (=pointer to open file)
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
SS_FILE *su_vfp_access(vfp)
        su_vfile_t *vfp;
{
        ss_dassert(vfp != NULL);
        return (SS_FILE*)vfp;
}

/*##**********************************************************************\
 * 
 *      su_vfh_init
 * 
 * Creates virtual file handle
 * 
 * Parameters : 
 * 
 *      pathname - in, use
 *		file name
 *
 *      persistent - in
 *		TRUE implies the file remains physically open
 *          FALSE allows cacheing of handle.
 *
 *      flags - in
 *          file flags, SS_BF_*
 *
 *      blocksize - in
 *          file block size (for I/O optimization)
 *
 * Return value - give :
 *          pointer to virtual file handle or
 *          NULL on error
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
su_vfilh_t *su_vfh_init(
        char *pathname,
        bool persistent,
        uint flags,
        size_t blocksize)
{
        su_vfilh_t *p_vfh;
        su_pfilh_t *p_pfh;

        FAKE_RETURN(FAKE_SU_VFHINITFAILS,NULL);

        VFH_GLOBALINIT_MAYBE();
        p_vfh = SsMemAlloc(sizeof(su_vfilh_t));
        p_vfh->vfh_used = su_list_init(NULL);
        p_vfh->vfh_unused = su_list_init(NULL);
        p_vfh->vfh_pathname = SsMemStrdup(pathname);
        p_vfh->vfh_persistent = persistent;
        p_vfh->vfh_flags = flags;
        p_vfh->vfh_flushgate = su_gate_init(SS_SEMNUM_SU_FLUSHGATE, TRUE);
        p_vfh->vfh_blocksize = blocksize;

        if (persistent) {
            p_vfh->vfh_meslist = su_meslist_init_nomutex(&p_vfh->vfh_meslist_buf);
            p_vfh->vfh_meswaitlist = su_meswaitlist_init();
            p_vfh->vfh_nwait = 0;
            /***** MUTEXBEGIN *****/
            SsSemEnter(fhpool->fhp_mutex);
            p_pfh = fhp_incpers(fhpool);
            p_pfh = su_pfh_init(p_vfh, p_pfh);
            SsSemExit(fhpool->fhp_mutex);
            /***** MUTEXEND *******/
            if (p_pfh == NULL) {
                su_vfh_done(p_vfh);
                p_vfh = NULL;
            } else {
                su_vfh_puttounusedlist(p_vfh, p_pfh);
            }
        } else {
            p_vfh->vfh_meslist = NULL;
            p_vfh->vfh_meswaitlist = NULL;
            p_vfh->vfh_nwait = 0;
        }
        return p_vfh;
}

/*##**********************************************************************\
 * 
 *      su_vfh_done
 * 
 * Deletes virtual file handle
 * 
 * Parameters : 
 * 
 *      p_vfh - in, take
 *		pointer to virtual file handle
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
void su_vfh_done(p_vfh)
        su_vfilh_t *p_vfh;
{
        su_pfilh_t *p_pfh;

        ss_dassert(p_vfh != NULL);
        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        ss_dassert(su_list_length(p_vfh->vfh_used) == 0);
        su_gate_done(p_vfh->vfh_flushgate);
        if (p_vfh->vfh_persistent) {
            ss_dassert(p_vfh->vfh_nwait == 0);
            ss_dassert(su_meswaitlist_isempty(p_vfh->vfh_meswaitlist));
            su_meswaitlist_done(p_vfh->vfh_meswaitlist);
            su_meslist_done(p_vfh->vfh_meslist);
        }
        while (su_list_length(p_vfh->vfh_unused) != 0) {
            p_pfh = su_listnode_getdata(
                        su_list_first(p_vfh->vfh_unused));
            su_pfh_done(p_pfh, FALSE);
        }
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        su_list_done(p_vfh->vfh_used);
        su_list_done(p_vfh->vfh_unused);
        SsMemFree(p_vfh->vfh_pathname);
        SsMemFree(p_vfh);
}

/*##**********************************************************************\
 * 
 *      su_vfh_beginaccess
 * 
 * Begins access of virtual file handle. Opens the file physically
 * if it was closed.
 * 
 * Parameters : 
 * 
 *      p_vfh - in out, use
 *		pointer to virtual file handle. Release is legal after
 *          su_vfh_endaccess()
 * 
 *      pp_pfh - out, ref
 *		pointer to pointer of physical file handle
 *
 *
 * Return value - give :
 *          Pointer to SsBFileT structure (from ssfile.?) or
 *          NULL on error
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
SsBFileT *su_vfh_beginaccess(p_vfh, pp_pfh)
        su_vfilh_t *p_vfh;
        su_pfilh_t **pp_pfh;
{
        su_pfilh_t *p_pfh = NULL;
        bool signalled = FALSE;
        
        ss_dprintf_1(("su_vfh_beginaccess\n"));
        ss_dassert(p_vfh != NULL);

        FAKE_RETURN(FAKE_SU_VFHBEGINACCESS,NULL);

        /* wait for possible flush/close to complete */
        su_gate_enter_shared(p_vfh->vfh_flushgate);

        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        if (su_list_length(p_vfh->vfh_unused) != 0) {
            ss_dprintf_2(("su_vfh_beginaccess:found handle from unused list\n"));
            p_pfh = su_listnode_getdata(su_list_first(p_vfh->vfh_unused));
            if (!p_pfh->pfh_persistent) {
                fhp_remove(fhpool, p_pfh);
            }
        } else {
            /* while number of used handles is (bigger than or)
            ** equal to maximum number of open handles
            */
            ss_dprintf_2(("su_vfh_beginaccess:wait for a handle\n"));
            while (fhpool->fhp_pers + fhpool->fhp_accessed +
                   su_list_length(fhpool->fhp_LRUpool) >= fhpool->fhp_max)
            {
                if (fhpool->fhp_nwait == 0 || signalled) {
                    /* Search for a file handle only when waitlist is empty or we
                     * have received a signal.
                     */
                    if (su_list_length(p_vfh->vfh_unused) != 0) {
                        p_pfh = su_listnode_getdata(
                                    su_list_first(p_vfh->vfh_unused));
                        if (!p_pfh->pfh_persistent) {
                            fhp_remove(fhpool, p_pfh);
                        }
                        ss_dprintf_2(("su_vfh_beginaccess:found a handle\n"));
                        break;
                    }
                }
                if (su_list_length(fhpool->fhp_LRUpool) == 0) {
                    su_mes_t* mes;
                    mes = su_meslist_mesinit(fhpool->fhp_meslist);
                    su_meswaitlist_add(fhpool->fhp_meswaitlist, mes);
                    fhpool->fhp_nwait++;
                    SsSemExit(fhpool->fhp_mutex);
                    /***** MUTEXEND *******/
                    ss_dprintf_2(("su_vfh_beginaccess:wait message\n"));
                    su_mes_wait(mes);    /* wait message */
                    signalled = TRUE;
                    /***** MUTEXBEGIN *****/
                    SsSemEnter(fhpool->fhp_mutex);
                    fhpool->fhp_nwait--;
                    su_meslist_mesdone(fhpool->fhp_meslist, mes);
                } else {
                    p_pfh = fhp_extractLRU(fhpool);
                    p_pfh = su_pfh_init(p_vfh, p_pfh);
                    break;
                }
            }
            if (p_pfh == NULL) {
                p_pfh = su_pfh_init(p_vfh, NULL);
            }
        }
        if (p_pfh == NULL) { /* Error ! */
            *pp_pfh = NULL;
            SsSemExit(fhpool->fhp_mutex);
            /***** MUTEXEND *******/
            su_gate_exit(p_vfh->vfh_flushgate);
            return NULL;
        }
        su_vfh_puttousedlist(p_vfh, p_pfh);  /* links p_pfh to p_vfh->vfh_used list */
        *pp_pfh = p_pfh;
        if (!p_pfh->pfh_persistent) {
            fhpool->fhp_accessed++;
        }
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        ss_dassert(p_pfh->pfh_fhandle != NULL);
        return p_pfh->pfh_fhandle;
}

/*##**********************************************************************\
 * 
 *      su_vfh_beginaccesspers
 * 
 * Begins access of virtual file handle. This functions claims file
 * persistency and always gets the persistent physical file handle.
 * 
 * Parameters : 
 * 
 *      p_vfh - in out, use
 *		pointer to virtual file handle. Release is legal
 *          after su_vfh_endaccess()
 *
 *      pp_pfh - out, ref
 *		pointer to pointer of physical file handle
 *
 * Return value - ref :
 *          Pointer to SsBFileT structure (from ssfile.?) or
 *          NULL on error
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
SsBFileT *su_vfh_beginaccesspers(p_vfh, pp_pfh)
        su_vfilh_t *p_vfh;
        su_pfilh_t **pp_pfh;
{
        su_pfilh_t *p_pfh;
        int nloop;
        su_mes_t* mes;

        ss_pprintf_1(("su_vfh_beginaccesspers\n"));
        ss_dassert(p_vfh != NULL);
        ss_dassert(p_vfh->vfh_persistent);
        
        FAKE_RETURN(FAKE_SU_VFHBEGINACCPERS,NULL);

        /* wait for possible flush/close to complete */
        su_gate_enter_shared(p_vfh->vfh_flushgate);

        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        for (nloop = 0; ; nloop++) {
            su_list_node_t *lnode;

            if (p_vfh->vfh_nwait == 0 || nloop > 0) {
                /* Search for a handle only when wait list is empty or
                 * we have received a message.
                 */
                lnode = su_list_first(p_vfh->vfh_unused);
                while (lnode != NULL) {
                    p_pfh = su_listnode_getdata(lnode);
                    if (p_pfh->pfh_persistent) {
                        break;
                    }
                    lnode = su_list_next(p_vfh->vfh_unused, lnode);
                }
                if (lnode != NULL) {
                    ss_pprintf_2(("su_vfh_beginaccesspers:found file handle\n"));
                    break;
                }
            }
            mes = su_meslist_mesinit(p_vfh->vfh_meslist);
            su_meswaitlist_add(p_vfh->vfh_meswaitlist, mes);
            p_vfh->vfh_nwait++;
            SsSemExit(fhpool->fhp_mutex);
            /***** MUTEXEND *******/
            ss_pprintf_2(("su_vfh_beginaccesspers:wait message, nloop=%d\n", nloop));
            su_mes_wait(mes);    /* wait message */
            /***** MUTEXBEGIN *****/
            SsSemEnter(fhpool->fhp_mutex);
            p_vfh->vfh_nwait--;
            su_meslist_mesdone(p_vfh->vfh_meslist, mes);
        }
        su_vfh_puttousedlist(p_vfh, p_pfh);  /* links p_pfh to p_vfh->vfh_used list */
        *pp_pfh = p_pfh;
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        ss_dassert(p_pfh->pfh_fhandle != NULL);
        return p_pfh->pfh_fhandle;
}


/*##**********************************************************************\
 * 
 *      su_vfh_endaccess
 * 
 * Ends access of virtual file handle. Puts nonpersistent files to
 * open file handle cache called 'fhpool'.
 * 
 * Parameters : 
 * 
 *      p_vfh - in out, use
 *		pointer to virtual file handle
 *
 *      p_pfh - in, use
 *		pointer to physical file handle received from
 *          su_vfh_beginaccess()
 *
 * Return value :
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
void su_vfh_endaccess(p_vfh, p_pfh)
        su_vfilh_t *p_vfh;
        su_pfilh_t *p_pfh;
{
        ss_dprintf_1(("su_vfh_endaccess\n"));
        ss_dassert(p_pfh != NULL);
        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        su_vfh_puttounusedlist(p_vfh, p_pfh);
        if (!p_pfh->pfh_persistent) {
            fhpool->fhp_accessed--;
            fhp_addtoLRUqueue(fhpool, p_pfh);
        } else {
            /* Maybe someone is waiting for a handle for this file:
            ** signal that it is now available
            */
            if (p_vfh->vfh_nwait > 0) {
                su_meswaitlist_wakeup1st(p_vfh->vfh_meswaitlist);
            }
            if (fhpool->fhp_nwait > 0) {
                su_meswaitlist_wakeup1st(fhpool->fhp_meswaitlist);/* for other handle requests */
            }
        }
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        su_gate_exit(p_vfh->vfh_flushgate);
}

            
/*##**********************************************************************\
 * 
 *      su_vfh_flush
 * 
 * Flushes open file to disk. If any physical handles are currently
 * accessed, falls asleep till the p_vfh->vfh_used list becomes empty
 * 
 * Parameters : 
 * 
 *      p_vfh - in out, use
 *		pointer to virtual file handle
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
void su_vfh_flush(p_vfh)
        su_vfilh_t *p_vfh;
{
        su_list_node_t *lnode;
        su_pfilh_t *p_pfh;

        /* wait for possible accesses to complete */
        su_gate_enter_exclusive(p_vfh->vfh_flushgate);
        
        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        /* loop through all members of vfh_unused list
        ** removing the physical file handles from LRU queue
        ** to disable reuse by concurrent threads.
        */
        for (lnode = su_list_first(p_vfh->vfh_unused);
             lnode != NULL;
             lnode = su_list_next(p_vfh->vfh_unused, lnode))
        {
            p_pfh = su_listnode_getdata(lnode);
            if (!p_pfh->pfh_persistent) {
                fhp_remove(fhpool, p_pfh);  /* Remove handles from LRU queue */
                fhpool->fhp_accessed++;
            }
        }
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        /* Do the actual flush (may be time-consuming)
        ** concurrently with other file accesses ie. outside
        ** file handle pool mutex
        */
        for (lnode = su_list_first(p_vfh->vfh_unused);
             lnode != NULL;
             lnode = su_list_next(p_vfh->vfh_unused, lnode))
        {
            p_pfh = su_listnode_getdata(lnode);
            ss_dassert(p_pfh->pfh_.dad == p_vfh);
            SsBFlush(p_pfh->pfh_fhandle);
        }
        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        /* Link the physical file handles back to LRU queue */
        lnode = su_list_first(p_vfh->vfh_unused);
        if (lnode != NULL) {
            do {
                p_pfh = su_listnode_getdata(lnode);
                if (!p_pfh->pfh_persistent) {
                    fhp_addtoLRUqueue(fhpool, p_pfh);
                    fhpool->fhp_accessed--;
                } else {
                    if (p_vfh->vfh_nwait > 0) {
                        /* for persistent handle requests
                         */
                        su_meswaitlist_wakeup1st(p_vfh->vfh_meswaitlist);
                    }
                }
                lnode = su_list_next(p_vfh->vfh_unused, lnode);
            } while (lnode != NULL);
        }
        if (fhpool->fhp_nwait > 0) {
            su_meswaitlist_wakeup1st(fhpool->fhp_meswaitlist);/* for other handle requests */
        }
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        su_gate_exit(p_vfh->vfh_flushgate);
}

/*##**********************************************************************\
 * 
 *      su_vfh_close
 * 
 * Closes open file handle. If any physical handles are currently
 * accessed, falls asleep till the p_vfh->vfh_used list becomes empty
 * 
 * Parameters : 
 * 
 *      p_vfh - in out, use
 *		pointer to virtual file handle
 * 
 * Return value : none
 * 
 * Limitations  : MUST NOT be called for persistent handles
 * 
 * Globals used : fhpool
 */
void su_vfh_close(p_vfh)
        su_vfilh_t *p_vfh;
{
        uint length;
        su_pfilh_t *p_pfh;
        su_list_node_t* lnode;

        ss_assert(!p_vfh->vfh_persistent);

        /* wait for possible accesses to complete */
        su_gate_enter_exclusive(p_vfh->vfh_flushgate);

        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        /* loop through all members of vfh_unused list
        ** removing the physical file handles from LRU queue
        ** to disable reuse by concurrent threads.
        */
        for (lnode = su_list_first(p_vfh->vfh_unused);
             lnode != NULL;
             lnode = su_list_next(p_vfh->vfh_unused, lnode))
        {
            p_pfh = su_listnode_getdata(lnode);
            fhp_remove(fhpool, p_pfh);  /* Remove handles from LRU queue */
            fhpool->fhp_accessed++;
        }
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        /* Delete (=close) all physical file handles
        ** that are open for this file (This is done
        ** concurrently with accesses of other files)
        */
        for (;;) {
            length = su_list_length(p_vfh->vfh_unused);
            if (length == 0) {
                break;
            }
            lnode = su_list_first(p_vfh->vfh_unused);
            p_pfh = su_listnode_getdata(lnode);
            if (p_pfh->pfh_fhandle != NULL) {
                SsBClose(p_pfh->pfh_fhandle);
                p_pfh->pfh_fhandle = NULL;
            }
            /***** MUTEXBEGIN *****/
            SsSemEnter(fhpool->fhp_mutex);
            fhpool->fhp_accessed--;
            su_pfh_done(p_pfh, FALSE); /* also unlinks the *p_pfh from lists */
            if (fhpool->fhp_nwait > 0) {
                su_meswaitlist_wakeup1st(fhpool->fhp_meswaitlist);/* for other handle requests */
            }
            SsSemExit(fhpool->fhp_mutex);
            /***** MUTEXEND *******/
        }
        su_gate_exit(p_vfh->vfh_flushgate);
}


/*##**********************************************************************\
 * 
 *      su_vfh_globalinit
 * 
 * Makes global initialization of file handle pool. This routine must
 * be called before any other function defined in this file.
 * 
 * Parameters : 
 * 
 *      max - in
 *          max number of physically open files
 *
 * Return value :
 *      TRUE    max files set
 *      FALSE   failed, using default SS_MAXFILES_DEFAULT
 * 
 * Limitations  : Must only be called once
 *                (or after su_vfh_globaldone()).
 * 
 * Globals used : fhpool
 */
bool su_vfh_globalinit(max)
        uint max;
{
        bool succp;

        latest_maxfiles = max;
        if (fhpool == NULL) {
            succp = SsFSetMaxOpenRel((int)max, NULL);
            if (!succp) {
                max = SS_MAXFILES_DEFAULT;
                succp = SsFSetMaxOpenRel((int)max, NULL);
                ss_assert(succp);
            }
            fhpool = fhp_init(max);
        } else {
            succp = TRUE;
        }
        return(succp);
}

/*##**********************************************************************\
 * 
 *      su_vfh_globaldone
 * 
 * Closes virtual file system. After this routine the su_vfh_globalinit()
 * must be called before any other functions from this file may be called.
 * 
 * Parameters : none
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
void su_vfh_globaldone()
{
        bool succp;

        if (fhpool != NULL) {
            succp = SsFSetMaxOpenRel(-(int)fhpool->fhp_max, NULL);
            ss_dassert(succp);
            fhp_done(fhpool);
            fhpool = NULL;
        }
}

/*##**********************************************************************\
 * 
 *      su_vfh_isinitialized
 * 
 * Checks if virtual file system pool is in initialized state.
 * 
 * Parameters : none
 * 
 * Return value :
 * 
 *      TRUE  - is initialized
 *      FALSE - not initialized
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
bool su_vfh_isinitialized(void)
{
        return(fhpool != NULL);
}

/*##**********************************************************************\
 * 
 *      su_vfh_setmaxopen
 * 
 * Sets maximum number of open file handles
 * 
 * Parameters : 
 * 
 *      max - in
 *		max. number of physically open files
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
void su_vfh_setmaxopen(max)
        uint max;
{
        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        fhp_setmax(fhpool, max);
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
}

/*##**********************************************************************\
 * 
 *      su_vfh_getmaxopen
 * 
 * Gets max. number of open files
 * 
 * Parameters : none
 * 
 * Return value :
 *          max. number of physically open files
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
uint su_vfh_getmaxopen()
{
        uint r;

        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        r = fhpool->fhp_max;
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        return r;
}

/*##**********************************************************************\
 * 
 *      su_vfh_getnrpersistent
 * 
 * Gets number of persistent files
 * 
 * Parameters : none
 * 
 * Return value :
 *          number of persistent files
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
uint su_vfh_getnrpersistent()
{
        uint r;

        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        r = fhpool->fhp_pers;
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        return r;
}

/*##**********************************************************************\
 * 
 *      su_vfh_getnraccessed
 * 
 * Gets number of currently accessed files.
 * 
 * Parameters : none
 * 
 * Return value :
 *          number of accesses files
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
uint su_vfh_getnraccessed()
{
        uint r;

        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        r = fhpool->fhp_accessed;
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        return r;
}

/*##**********************************************************************\
 * 
 *      su_vfh_getnrcached
 * 
 * Gets number of open files handles in cache.
 * 
 * Parameters : none
 * 
 * Return value :
 *          number of file handles in cache
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
uint su_vfh_getnrcached()
{
        uint r;

        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        r = su_list_length(fhpool->fhp_LRUpool);
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        return r;
}

/*##**********************************************************************\
 * 
 *      su_vfh_getnropen
 * 
 * Gets number of open file handles including persistent, accessed
 * and cached. This number never exceeds the max. open files limit.
 * 
 * Parameters : none
 * 
 * Return value :
 *          numbers of open file handles
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
uint su_vfh_getnropen()
{
        uint r;

        /***** MUTEXBEGIN *****/
        SsSemEnter(fhpool->fhp_mutex);
        r = fhpool->fhp_accessed + fhpool->fhp_pers +
            su_list_length(fhpool->fhp_LRUpool);
        SsSemExit(fhpool->fhp_mutex);
        /***** MUTEXEND *******/
        return r;
}

/*##**********************************************************************\
 * 
 *      su_vfh_getfilename
 * 
 * Gets filename associated with virtual file handle.
 * 
 * Parameters : 
 * 
 *      p_vfh - in, use
 *		pointer to virtual file handle
 *
 * Return value - ref :
 *          pointer to char string containing filename.
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
char *su_vfh_getfilename(p_vfh)
        su_vfilh_t *p_vfh;
{
        return p_vfh->vfh_pathname;
}

/*##**********************************************************************\
 * 
 *      su_vfh_ispersistent
 * 
 * Checks whether virtual file handle is persistent.
 * 
 * Parameters : 
 * 
 *      p_vfh - in, use
 *		pointer to virtual file handle
 *
 * Return value :
 *          TRUE if persistent FALSE otherwise
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
bool su_vfh_ispersistent(p_vfh)
        su_vfilh_t *p_vfh;
{
        return p_vfh->vfh_persistent;
}

/*#**********************************************************************\
 * 
 *      su_vfh_puttousedlist
 * 
 * Links a physical file handle to the vfh_used list of 
 * virtual file handle
 * 
 * Parameters : 
 * 
 *      p_vfh - in out, use
 *		pointer to virtual file handle
 *
 *      p_pfh - in out, use
 *		pointer to physical file handle
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void su_vfh_puttousedlist(p_vfh, p_pfh)
        su_vfilh_t *p_vfh;
        su_pfilh_t *p_pfh;
{
        ss_dassert(p_vfh != NULL);
        ss_dassert(p_pfh != NULL);

        if (p_pfh->pfh_share_lnode != NULL) {   /* unlink *p_pfh */
            ss_dassert(p_pfh->pfh_.dad == p_vfh);
            ss_dassert(p_vfh->vfh_unused == p_pfh->pfh_share_list);
            su_list_remove(p_pfh->pfh_share_list, p_pfh->pfh_share_lnode);
        }
        p_pfh->pfh_share_lnode = su_list_insertfirst(p_vfh->vfh_used, p_pfh);
        p_pfh->pfh_share_list = p_vfh->vfh_used;
}

/*#**********************************************************************\
 * 
 *      su_vfh_puttounusedlist
 * 
 * Links a physical file handle to the vfh_unused list of
 * virtual file handle
 * 
 * Parameters : 
 * 
 *      p_vfh - in out, use
 *		pointer to virtual file handle
 *
 *      p_pfh - in out, use
 *		pointer to physical file handle
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void su_vfh_puttounusedlist(p_vfh, p_pfh)
        su_vfilh_t *p_vfh;
        su_pfilh_t *p_pfh;
{
        ss_dassert(p_vfh != NULL);
        ss_dassert(p_pfh != NULL);

        if (p_pfh->pfh_share_lnode != NULL) {   /* unlink *p_pfh */
            ss_dassert(p_vfh->vfh_used == p_pfh->pfh_share_list);
            su_list_remove(p_pfh->pfh_share_list, p_pfh->pfh_share_lnode);
        }
        p_pfh->pfh_share_lnode = su_list_insertfirst(p_vfh->vfh_unused, p_pfh);
        p_pfh->pfh_share_list = p_vfh->vfh_unused;
}

/*#**********************************************************************\
 * 
 *      su_pfh_init
 * 
 * Creates or reinits a physical file handle.
 * 
 * Parameters : 
 * 
 *      p_vfh - in, use
 *		pointer to the associated virtual file handle
 *
 *      p_pfh - in out, use
 *		pointer to previously allocated physical file handle
 *          (which had been reset by fhp_extractLRU()) or
 *          NULL if we need to allocate a new one
 *
 * Return value - give :
 *          pointer to created physical file handle
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_pfilh_t *su_pfh_init(p_vfh, p_pfh)
        su_vfilh_t *p_vfh;
        su_pfilh_t *p_pfh;
{
        if (p_pfh == NULL) {
            if (fhpool->fhp_freepfhlist == NULL) {
                p_pfh = SsMemAlloc(sizeof(su_pfilh_t));
            } else {
                p_pfh = fhpool->fhp_freepfhlist;
                fhpool->fhp_freepfhlist = p_pfh->pfh_.nextfree;
                p_pfh->pfh_.nextfree = NULL;
            }
        }
        p_pfh->pfh_.dad = p_vfh;
        p_pfh->pfh_LRU_lnode = NULL;
        p_pfh->pfh_LRU_list = NULL;
        p_pfh->pfh_share_lnode = NULL;
        p_pfh->pfh_share_list = NULL;
        p_pfh->pfh_fhandle = SsBOpen(
                                p_vfh->vfh_pathname,
                                p_vfh->vfh_flags,
                                p_vfh->vfh_blocksize);
        if (p_pfh->pfh_fhandle == NULL) {
            SsMemFree(p_pfh);
            return NULL;
        }
        
        if (p_vfh->vfh_persistent &&
            su_list_length(p_vfh->vfh_used) == 0 &&
            su_list_length(p_vfh->vfh_unused) == 0)
        {
            p_pfh->pfh_persistent = TRUE;
        } else {
            p_pfh->pfh_persistent = FALSE;
        }
        return (p_pfh);
}


/*#**********************************************************************\
 * 
 *      su_pfh_done
 * 
 * Deletes a physical file handle (closes the file)
 * 
 * Parameters : 
 * 
 *      p_pfh - in, take
 *		pointer to physical file handle
 *
 *      freeflag - in
 *          tells whether the memory is to be freed, too
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : fhpool
 */
static void su_pfh_done(p_pfh, freeflag)
        su_pfilh_t *p_pfh;
        bool freeflag;
{
        ss_dassert(p_pfh != NULL);
        if (p_pfh->pfh_LRU_lnode != NULL) {
            ss_dassert(p_pfh->pfh_fhandle != NULL);
            su_list_remove(p_pfh->pfh_LRU_list, p_pfh->pfh_LRU_lnode);
            p_pfh->pfh_LRU_lnode = NULL;
        }
        if (p_pfh->pfh_share_lnode != NULL) {
            su_list_remove(p_pfh->pfh_share_list, p_pfh->pfh_share_lnode);
            p_pfh->pfh_share_lnode = NULL;
        }
        if (p_pfh->pfh_persistent) {
            fhp_decpers(fhpool);
            p_pfh->pfh_persistent = FALSE;
        }
        if (p_pfh->pfh_fhandle != NULL) {
            SsBClose(p_pfh->pfh_fhandle);
            p_pfh->pfh_fhandle = NULL;
        }
        if (freeflag) {
            SsMemFree(p_pfh);
        } else {
            p_pfh->pfh_.dad = NULL;
            p_pfh->pfh_.nextfree = fhpool->fhp_freepfhlist;
            fhpool->fhp_freepfhlist = p_pfh;
        }
}

/*#**********************************************************************\
 * 
 *      fhp_init
 * 
 * Creates file handle pool
 * 
 * Parameters : 
 * 
 *      max - in
 *		max. number of open files
 *
 * Return value - give :
 *          pointer to created file handle pool
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static file_handle_pool_t *fhp_init(max)
        uint max;
{
        file_handle_pool_t *pp;
    
        pp = SsMemAlloc(sizeof(file_handle_pool_t));
        pp->fhp_max = max;
        pp->fhp_pers = 0;
        pp->fhp_accessed = 0;
        pp->fhp_LRUpool = su_list_init(NULL);
        pp->fhp_mutex = SsSemCreateLocal(SS_SEMNUM_SU_VFIL);
        pp->fhp_meslist = su_meslist_init(&pp->fhp_meslist_buf);
        pp->fhp_meswaitlist = su_meswaitlist_init();
        pp->fhp_nwait = 0;
        pp->fhp_freepfhlist = NULL;
        return pp;
}

/*#**********************************************************************\
 * 
 *      fhp_done
 * 
 * Deletes file handle pool
 * 
 * Parameters : 
 * 
 *      fhp - in, take
 *		pointer to file handle pool
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void fhp_done(fhp)
	    file_handle_pool_t *fhp;
{
        su_pfilh_t *p_pfh;

        ss_dassert(fhp != NULL);
        ss_dassert(fhp->fhp_nwait == 0);
        ss_dassert(su_meswaitlist_isempty(fhp->fhp_meswaitlist));
        su_meswaitlist_done(fhp->fhp_meswaitlist);
        su_meslist_done(fhp->fhp_meslist);
        SsSemFree(fhp->fhp_mutex);
        while (su_list_length(fhp->fhp_LRUpool) > 0) {
            p_pfh = fhp_extractLRU(fhp);
            su_pfh_done(p_pfh, TRUE);
        }
        su_list_done(fhp->fhp_LRUpool);
        while (fhp->fhp_freepfhlist != NULL) {
            p_pfh = fhp->fhp_freepfhlist;
            fhp->fhp_freepfhlist = p_pfh->pfh_.nextfree;
            su_pfh_done(p_pfh, TRUE);
        }
        SsMemFree(fhp);
}

/*#**********************************************************************\
 * 
 *		fhp_setmax
 * 
 * Sets max. number of open file handles in pool.
 * 
 * Parameters : 
 * 
 *      fhp - in out, use
 *		pointer to file handle pool
 *
 *	max - in
 *		max. number of open files
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void fhp_setmax(fhp, max)
	    file_handle_pool_t *fhp;
	    uint max;
{
        su_pfilh_t *p_pfh;

        ss_dassert(max > fhp->fhp_pers);
        if (fhp->fhp_max > max) {
            while (fhp->fhp_max > max
                   && fhp->fhp_pers  +
                      fhp->fhp_accessed + 
                      su_list_length(fhp->fhp_LRUpool) >
                   (fhp->fhp_max - 1))
            {
                if (su_list_length(fhp->fhp_LRUpool) == 0) {
                    su_mes_t* mes;
                    mes = su_meslist_mesinit(fhp->fhp_meslist);
                    su_meswaitlist_add(fhp->fhp_meswaitlist, mes);
                    fhp->fhp_nwait++;
                    SsSemExit(fhp->fhp_mutex);
                    /***** MUTEXEND *******/
                    su_mes_wait(mes);
                    /***** MUTEXBEGIN *****/
                    SsSemEnter(fhp->fhp_mutex);
                    fhp->fhp_nwait--;
                    su_meslist_mesdone(fhp->fhp_meslist, mes);
                } else {
                    p_pfh = fhp_extractLRU(fhp);
                    su_pfh_done(p_pfh, FALSE);
                    fhp->fhp_max--;
                }
            }
        }
        fhp->fhp_max = max;
}
    
/*#**********************************************************************\
 * 
 *		fhp_incpers
 * 
 * Increments number of persistent files.
 * 
 * Parameters : 
 * 
 *      fhp - in out, use
 *		pointer to file handle pool
 *
 * 
 * Return value - give : 
 *          pointer to physical file handle object, that was removed
 *          from LRU queue, that has to be released by the caller, or
 *          NULL
 *
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_pfilh_t *fhp_incpers(fhp)
        file_handle_pool_t *fhp;
{
        su_pfilh_t *p_pfh = NULL;

        ss_dassert(fhp->fhp_pers + 1 < fhp->fhp_max);
        /* while number of used handles is (bigger than or)
        ** equal to maximum number of open handles
        */
        while ((fhp->fhp_pers + 1) + fhp->fhp_accessed +
               su_list_length(fhp->fhp_LRUpool) >
               fhp->fhp_max)
        {
            if (su_list_length(fhp->fhp_LRUpool) == 0) {
                su_mes_t* mes;
                mes = su_meslist_mesinit(fhp->fhp_meslist);
                su_meswaitlist_add(fhp->fhp_meswaitlist, mes);
                fhp->fhp_nwait++;
                SsSemExit(fhp->fhp_mutex);
                /***** MUTEXEND *******/
                su_mes_wait(mes);           /* wait for handle to appear */
                /***** MUTEXBEGIN *****/
                SsSemEnter(fhp->fhp_mutex);
                fhp->fhp_nwait--;
                su_meslist_mesdone(fhp->fhp_meslist, mes);
            } else {
                if (p_pfh != NULL) {
                    su_pfh_done(p_pfh, FALSE);
                }
                p_pfh = fhp_extractLRU(fhp);
            }
        }
        fhp->fhp_pers++;
        return p_pfh;
}

/*#**********************************************************************\
 * 
 *		fhp_decpers
 * 
 * Decrements number of persistent files.
 * 
 * Parameters : 
 * 
 *      fhp - in out, use
 *		pointer to file handle pool
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void fhp_decpers(fhp)
	    file_handle_pool_t *fhp;
{
        ss_dassert(fhp->fhp_pers != 0);
        fhp->fhp_pers--;
}


/*#**********************************************************************\
 * 
 *		fhp_extractLRU
 * 
 * Removes the Least Recently Used file handle from the pool and closes
 * the file
 * 
 * Parameters : 
 * 
 *      fhp - in out, use
 *		pointer to file handle pool
 * 
 * Return value - give :
 *          pointer to extracted physical file handle
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_pfilh_t *fhp_extractLRU(fhp)
	    file_handle_pool_t *fhp;
{
        su_pfilh_t *p_lru_pfh;

        ss_dassert(su_list_length(fhp->fhp_LRUpool) != 0);
        p_lru_pfh = su_list_removelast(fhp->fhp_LRUpool);
        p_lru_pfh->pfh_LRU_lnode = NULL;
        p_lru_pfh->pfh_LRU_list = NULL;
        ss_dassert(p_lru_pfh->pfh_fhandle != NULL);
        ss_dassert(p_lru_pfh->pfh_share_lnode != NULL);
        su_list_remove(p_lru_pfh->pfh_share_list, p_lru_pfh->pfh_share_lnode);
        p_lru_pfh->pfh_share_list = NULL;
        p_lru_pfh->pfh_share_lnode = NULL;
        if (p_lru_pfh->pfh_fhandle != NULL) {
            SsBClose(p_lru_pfh->pfh_fhandle);
            p_lru_pfh->pfh_fhandle = NULL;
        }
        p_lru_pfh->pfh_.dad = NULL;
        return p_lru_pfh;
}

/*#**********************************************************************\
 * 
 *		fhp_remove
 * 
 * Removes a virtual file handle from the file handle pool
 * 
 * Parameters : 
 * 
 *	fhp - in out, use
 *		pointer to file handle pool
 *
 *	p_pfh - in out, use
 *		pointer to phys. file handle 
 *
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void fhp_remove(fhp, p_pfh)
	file_handle_pool_t *fhp;
        su_pfilh_t *p_pfh;
{
        if (p_pfh->pfh_LRU_lnode != NULL) {
            su_list_remove(fhp->fhp_LRUpool, p_pfh->pfh_LRU_lnode);
        } else {
            ss_dprintf_1(("fhp_remove(): already out of LRU\n"));
        }
        p_pfh->pfh_LRU_lnode = NULL;
        p_pfh->pfh_LRU_list = NULL;
}

/*#**********************************************************************\
 * 
 *		fhp_addtoLRUqueue
 * 
 * Adds a file handle to the pool
 * 
 * Parameters : 
 * 
 *	fhp - in out, use
 *		pointer to file handle pool
 *
 *	p_pfh - in, take
 *		pointer to physical file handle
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void fhp_addtoLRUqueue(fhp, p_pfh)
	file_handle_pool_t *fhp;
	su_pfilh_t *p_pfh;
{
        p_pfh->pfh_LRU_lnode = su_list_insertfirst(fhp->fhp_LRUpool, p_pfh);
        p_pfh->pfh_LRU_list = fhp->fhp_LRUpool;
        if (fhp->fhp_nwait > 0) {
            su_meswaitlist_wakeup1st(fhp->fhp_meswaitlist);
        }
}

