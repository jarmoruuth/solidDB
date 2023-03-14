/*************************************************************************\
**  source       * su0vmem.c
**  directory    * su
**  description  * Virtual memory system.
**               * 
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


#include <ssstdio.h>
#include <ssstddef.h>
#include <ssstring.h>
#include <sslimits.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssfile.h>
#include <sscacmem.h>

#include "su0parr.h"
#include "su0vfil.h"
#include "su0svfil.h"
#include "su0vmem.h"

#define VMEM_CHECK  8675

typedef struct slot_st slot_t;

/* Virtual memory block control structure.
*/

struct slot_st {
        vmem_addr_t slot_addr;     /* Address of data in this slot. */
        char*       slot_data;     /* Pointer to the data area. */
        bool        slot_dirty;    /* Flag: is this block modified. */
        int         slot_nreach;   /* Number of reaches for this block. */
        slot_t*     slot_next;     /* Next slot in LRU chain. */
        slot_t*     slot_prev;     /* Previous slot in LRU chain. */
        slot_t*     slot_hashnext; /* Next slot in hash chain. */
};

/* Hash structure used to search blocks.
*/

typedef struct {
        uint     h_tablesize;   /* Hash table size. */
        slot_t** h_table;       /* Hash table. */
} hash_t;


/* Virtual memory structure.
*/

struct su_vmem_st {
        int             vmem_check;     /* Check value. */
        uint            vmem_nslot;     /* Number of virtual memory slots. */
        uint            vmem_blocksize; /* Memory block size. */
        uint            vmem_openflags; /* su_svf_init flags */
        su_pa_t*        vmem_fnamearr;
        su_svfil_t*     vmem_file;      /* may consist of several phys. files */
        vmem_addr_t     vmem_logicalsize;  /* Current vmem size. */
        vmem_addr_t     vmem_physicalsize; /* Current vmem size. */
        slot_t*         vmem_lruhead;   /* Head (and tail) of LRU chain. */
        hash_t          vmem_hash;      /* Hash table. */
        su_vmem_info_t* vmem_info;      /* Pointer to the info storage if
                                           non-NULL. */
        SsCacMemT*      vmem_cacmem;
        getnewfname_callback_t* vmem_getnewfname_callback;
        releasefname_callback_t* vmem_releasefname_callback;
        void*           vmem_callback_ctx; /* callback context */
};

#define HASH_INDEX(hash, addr) \
            ((size_t)((addr) % (hash)->h_tablesize))

/*#**********************************************************************\
 * 
 *		hash_init
 * 
 * Initializes a hash search structure.
 * 
 * Parameters : 
 * 
 *	hash - in, use
 *		hash table
 *
 *	tablesize - in
 *		hash table size
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void hash_init(hash_t* hash, uint tablesize)
{
        ss_dassert(tablesize > 0);

        hash->h_tablesize = tablesize;
        hash->h_table = SsMemCalloc(tablesize, sizeof(slot_t*));
}

/*#***********************************************************************\
 * 
 *		hash_resize
 * 
 * Resizes the table of a hash object
 * 
 * Parameters : 
 * 
 *	hash - in out, use
 *		hash object
 *		
 *	newtablesize - in
 *		new hash table size
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void hash_resize(hash_t* hash, uint newtablesize)
{
        SsMemFree(hash->h_table);
        hash->h_table = SsMemCalloc(newtablesize, sizeof(slot_t*));
        hash->h_tablesize = newtablesize;
}

/*#**********************************************************************\
 * 
 *		hash_done
 * 
 * Releases a hash table.
 * 
 * Parameters : 
 * 
 *	hash - in, take
 *		hash table
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void hash_done(hash_t* hash)
{
        SsMemFree(hash->h_table);
}

/*#**********************************************************************\
 * 
 *		hash_insert
 * 
 * Inserts slot into hash table.
 * 
 * Parameters : 
 * 
 *	hash - in out, use
 *		hash table
 *
 *	s - in, hold
 *		new slot
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void hash_insert(hash_t* hash, slot_t* s)
{
        uint slot_index;

        slot_index = HASH_INDEX(hash, s->slot_addr);
        
        /* put new slot to the head of the chain */
        s->slot_hashnext = hash->h_table[slot_index];
        hash->h_table[slot_index] = s;
}

/*#**********************************************************************\
 * 
 *		hash_remove
 * 
 * Removes a slot from a hash table.
 * 
 * Parameters : 
 * 
 *	hash - in out, use
 *		hash table
 *
 *	slot - in, use
 *		slot which is removed
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void hash_remove(hash_t* hash, slot_t* slot)
{
        uint    slot_index;
        slot_t* cur_slot;
        slot_t* prev_slot;

        ss_dassert(slot != NULL);

        slot_index = HASH_INDEX(hash, slot->slot_addr);

        cur_slot = hash->h_table[slot_index];
        ss_dassert(cur_slot != NULL);

        if (slot == cur_slot) {
            /* the first entry in the chain */
            hash->h_table[slot_index] = slot->slot_hashnext;
        } else {
            /* search slot from the chain */
            do {
                prev_slot = cur_slot;
                cur_slot = cur_slot->slot_hashnext;
                ss_dassert(cur_slot != NULL);
            } while (cur_slot != slot);
            /* remove slot from the chain */
            prev_slot->slot_hashnext = cur_slot->slot_hashnext;
        }
}

/*#**********************************************************************\
 * 
 *		hash_search
 * 
 * Searched a slot with address addr from the hash table.
 * 
 * Parameters : 
 * 
 *	hash - in, use
 *		hash table
 *
 *	addr - in
 *		searched address
 *
 * Return value : 
 *
 *      NULL, if addr not found
 *      Poiter to the slot with addr, of addr found
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static slot_t* hash_search(hash_t* hash, vmem_addr_t addr)
{
        slot_t* slot;

        slot = hash->h_table[HASH_INDEX(hash, addr)];
        while (slot != NULL && slot->slot_addr != addr) {
            slot = slot->slot_hashnext;
        }
        return(slot);
}

/*#**********************************************************************\
 * 
 *		lru_insert
 * 
 * Inserts a slot into LRU chain.
 * 
 * Parameters : 
 * 
 *	prev_s - in, use
 *		previous slot
 *
 *	s - in, use
 *		new slot
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void lru_insert(slot_t* prev_s, slot_t* s)
{
        s->slot_next = prev_s->slot_next;
        prev_s->slot_next->slot_prev = s;
        prev_s->slot_next = s;
        s->slot_prev = prev_s;
}

/*#**********************************************************************\
 * 
 *		lru_remove
 * 
 * Removes a slot from the LRU chain.
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		sloct which is removed
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void lru_remove(slot_t* s)
{
        s->slot_prev->slot_next = s->slot_next;
        s->slot_next->slot_prev = s->slot_prev;
}

/*#**********************************************************************\
 * 
 *		slot_write
 * 
 * Writes slot data into vmem file. This write function is unconditional,
 * it does not check the slot_dirty flag.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 *	s - in, use
 *		slot the data of which is written
 *
 * Return value : 
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool slot_write(su_vmem_t* vmem, slot_t* s)
{
        bool retry;
        bool succp = TRUE;
        su_ret_t rc;
        
        ss_dprintf_3(("slot_write:addr = %ld, vmem size = %ld, file size = %ld\n",
                      s->slot_addr,
                      vmem->vmem_logicalsize,
                      su_svf_getsize(vmem->vmem_file)));

        ss_dassert(s->slot_dirty);
        ss_dassert(s->slot_addr < vmem->vmem_logicalsize);

        if (vmem->vmem_info != NULL) {
            vmem->vmem_info->vmemi_nwrite++;
        }
        for (retry = FALSE; ; retry = TRUE) {
            uint num;
            char* fname;
            su_ret_t rc2;
            long maxsize;
            
            rc = su_svf_write(vmem->vmem_file,
                              s->slot_addr,
                              s->slot_data,
                              vmem->vmem_blocksize);
            if (rc != SU_ERR_FILE_WRITE_CFG_EXCEEDED || retry) {
                break;
            } /* else CFG exceeded; try to add new file */
            ss_dprintf_1(("vmem slot_write: CFG exceeded, try to add new file\n"));
            num = su_pa_nelems(vmem->vmem_fnamearr);
            fname =
                vmem->vmem_getnewfname_callback(vmem->vmem_callback_ctx,
                                                num);
            if (fname == NULL) {
                ss_dprintf_1(("vmem slot_write: new file not gotten\n"));
                succp = FALSE;
                break;
            }
            if (SsFExist(fname)) {
                SsFRemove(fname);
            }
            ss_dprintf_1(("vmem slot_write: new file: %s\n", fname));
            {
                ss_int8_t maxsize_i8;
                
                maxsize = SU_VFIL_SIZE_MAX / vmem->vmem_blocksize;
                maxsize *= vmem->vmem_blocksize;
                FAKE_CODE_BLOCK(FAKE_SU_VMEMMAXPHYSFILESIZESMALL,
                                maxsize = 20 * vmem->vmem_blocksize;);
                SsInt8SetUint4(&maxsize_i8, (ss_uint4_t)maxsize);
                rc2 = su_svf_addfile(vmem->vmem_file,
                                     fname,
                                     maxsize_i8,
                                     FALSE);
            }
            if (rc2 != SU_SUCCESS) {
                vmem->vmem_releasefname_callback(vmem->vmem_callback_ctx,
                                                 num,
                                                 fname);
                rc = rc2;
                break;
            }
            ss_dassert(!su_pa_indexinuse(vmem->vmem_fnamearr, num));
            su_pa_insertat(vmem->vmem_fnamearr, num, fname);
        } 
        if (rc != SU_SUCCESS) {
            ss_dprintf_1(("vmem slot_write failure rc = %d\n", (int)rc));
            succp = FALSE;
        } else if (succp) {
            if (s->slot_addr >= vmem->vmem_physicalsize) {
                vmem->vmem_physicalsize = s->slot_addr + 1;
            }
            s->slot_dirty = 0;
        }
        return (succp);
}

/*#**********************************************************************\
 * 
 *		slot_reach
 * 
 * Reaches and optionally reads slot data from the given address into memory.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 *	s - in out, use
 *		slot into which the data is read
 *
 *	addr - in
 *		data address
 *
 *	readp - in
 *		if true the data is read from the disk
 *
 * Return value :
 *      TRUE when successful
 *      FALSE when failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool slot_reach(su_vmem_t* vmem, slot_t* s, vmem_addr_t addr, bool readp)
{
        bool succp = TRUE;

        if (vmem->vmem_info != NULL) {
            vmem->vmem_info->vmemi_nread++;
        }
        if (s->slot_dirty) {
            succp = slot_write(vmem, s);
            if (!succp) {
                return (FALSE);
            }
        }
        if (s->slot_addr != SU_DADDR_NULL) {
            hash_remove(&vmem->vmem_hash, s);
        }

        if (readp && addr < vmem->vmem_physicalsize) {
            su_ret_t rc;
            size_t sizeread;

            rc = su_svf_read(
                    vmem->vmem_file,
                    addr,
                    s->slot_data,
                    vmem->vmem_blocksize,
                    &sizeread);
            if (rc != SU_SUCCESS || sizeread != vmem->vmem_blocksize) {
                succp = FALSE;
            }
        }
        if (succp) {  
            s->slot_addr = addr;
            s->slot_nreach++;

            lru_remove(s);
            lru_insert(vmem->vmem_lruhead, s);
        } else {
            s->slot_addr = SU_DADDR_NULL;
        }
        if (s->slot_addr != SU_DADDR_NULL) {
            hash_insert(&vmem->vmem_hash, s);
        }
        return (succp);
}

/*#***********************************************************************\
 * 
 *		vmem_fileopen
 * 
 * 
 * 
 * Parameters : 
 * 
 *	fname - 
 *		
 *		
 *	blocksize - 
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
static su_svfil_t* vmem_fileopen(char* fname, size_t blocksize, int openflags)
{
        su_ret_t rc;
        long maxsize;
        ss_int8_t maxsize_i8;
        
        su_svfil_t* svfil =
            su_svf_init(blocksize, openflags);
        maxsize = SU_VFIL_SIZE_MAX / blocksize;
        maxsize *= blocksize;
        FAKE_CODE_BLOCK(FAKE_SU_VMEMMAXPHYSFILESIZESMALL,
                        maxsize = 20 * blocksize;);
        SsInt8SetUint4(&maxsize_i8, (ss_uint4_t)maxsize);
        rc = su_svf_addfile(svfil,
                            fname,
                            maxsize_i8,
                            FALSE);
        if (rc != SU_SUCCESS) {
            ss_rc_derror(rc);
            su_svf_done(svfil);
            svfil = NULL;
        }
        return (svfil);
}

/*##**********************************************************************\
 * 
 *		su_vmem_open
 * 
 * Opens an existing virtual memory or creates a new one.
 * 
 * Parameters : 
 * 
 *	fname - in, use
 *		file name for the virtual memory storage. If fname
 *          is NULL, then the system generates a unique temporary
 *          file name.
 *
 *	nblock - in
 *		number of virtual memory blocks
 *
 *      blocks - in, hold
 *          array of pointers to virtual memory blocks
 *
 *	blocksize - in
 *		memory block size
 *
 *      openflags - in
 *          file open flags ored together from SS_BF_XXX
 *
 *      getnewfname_callback - in
 *          pointer to callback function that gets a new file
 *          name for extra file needed to extentd the address space
 *          beyond 2 GB
 *
 *      releasefname_callback - in
 *          pointer to callback function for releasing file name
 *          gotten through call to getnewfname_callback
 *
 *      ctx - in, hold
 *          context to be given as the first parameter to the above
 *          callback functions
 *
 * Return value - give : 
 * 
 *      vmem pointer, or NULL failed to open file
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_vmem_t* su_vmem_open(
        char* fname,
        uint nblock,
        void* blocks, /* void* blocks[nblocks], actually*/
        size_t blocksize,
        int openflags,
        getnewfname_callback_t* getnewfname_callback,
        releasefname_callback_t* releasefname_callback,
        void* ctx)
{
        int     i;
        slot_t* s;
        su_vmem_t* vmem;

        ss_dprintf_1(("su_vmem_open:fname=%s\n", fname != NULL ? fname : "NULL"));

        vmem = SsMemAlloc(sizeof(su_vmem_t));
        ss_assert(vmem != NULL);

        vmem->vmem_check = VMEM_CHECK;
        vmem->vmem_nslot = nblock;
        vmem->vmem_blocksize = blocksize;
        vmem->vmem_openflags = openflags;
        vmem->vmem_fnamearr = su_pa_init();
        ss_dassert(fname != NULL);
        fname = SsMemStrdup(fname);
        su_pa_insert(vmem->vmem_fnamearr, fname);
        vmem->vmem_getnewfname_callback = getnewfname_callback;
        vmem->vmem_releasefname_callback = releasefname_callback;
        vmem->vmem_callback_ctx = ctx;
        vmem->vmem_file = vmem_fileopen(fname, blocksize, openflags);
        if (vmem->vmem_file == NULL) {
            su_pa_done(vmem->vmem_fnamearr);
            SsMemFree(vmem);
            SsMemFree(fname);
            return(NULL);
        }
        vmem->vmem_logicalsize = vmem->vmem_physicalsize
            = su_svf_getsize(vmem->vmem_file);

        vmem->vmem_info = NULL;

        vmem->vmem_lruhead = SsMemAlloc(sizeof(slot_t));
        ss_assert(vmem->vmem_lruhead != NULL);
        vmem->vmem_lruhead->slot_addr = SU_DADDR_NULL;
        vmem->vmem_lruhead->slot_data = NULL;
        vmem->vmem_lruhead->slot_next = vmem->vmem_lruhead;
        vmem->vmem_lruhead->slot_prev = vmem->vmem_lruhead;
        hash_init(&vmem->vmem_hash, nblock);

        if (blocks == NULL) {
            vmem->vmem_cacmem = SsCacMemInit(blocksize, nblock);
        } else {
            vmem->vmem_cacmem = NULL;
        }

        for (i = 0; i < (int)nblock; i++) {
            s = SsMemAlloc(sizeof(slot_t));
            s->slot_addr = SU_DADDR_NULL;
            if (blocks == NULL) {
                s->slot_data = SsCacMemAlloc(vmem->vmem_cacmem);
            } else {
                s->slot_data = ((void**)blocks)[i];
            }
            s->slot_dirty = FALSE;
            s->slot_nreach = 0;
            lru_insert(vmem->vmem_lruhead, s);
        }

        ss_dprintf_2(("su_vmem_open:%d\n", (int)vmem));

        return(vmem);
}

/*#**********************************************************************\
 * 
 *		vmem_done
 * 
 * Releases virtual memory. If delete is TRUE, deletes the virtual memory
 * file.
 * 
 * Parameters : 
 * 
 *	vmem - in, take
 *		vmem pointer
 *
 *	delete - in
 *		if TRUE, deletes vmem file
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SS_MYSQL
static void vmem_done(
        su_vmem_t* vmem __attribute__ ((unused)),
        bool delete __attribute__ ((unused)))
{
}
#endif /*! SS_MYSQL */

/*##**********************************************************************\
 * 
 *		su_vmem_delete
 * 
 * Closes virtual memory and deletes the virtual memory file.
 * 
 * Parameters : 
 * 
 *	vmem - in, take
 *		vmem pointer
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_vmem_delete(su_vmem_t* vmem)
{
        slot_t* s;
        uint i;
        char* fname;

        ss_dprintf_1(("su_vmem_delete:%d\n", (int)vmem));
        ss_dassert(vmem->vmem_check == VMEM_CHECK);

        su_svf_done(vmem->vmem_file);
        su_pa_do_get(vmem->vmem_fnamearr, i, fname) {
            SsFRemove(fname);
        }
        /* loop through releasing all file names given by callback interface */
        for (i = su_pa_nelems(vmem->vmem_fnamearr);
             --i > 0; )
        {
            fname = su_pa_getdata(vmem->vmem_fnamearr, i);
            vmem->vmem_releasefname_callback(vmem->vmem_callback_ctx,
                                             i,
                                             fname);
        }
        /* and also free the first file name which was
         * received at initialization
         */
        ss_rc_dassert(i == 0, i);
        fname = su_pa_getdata(vmem->vmem_fnamearr, 0);
        SsMemFree(fname);
        su_pa_done(vmem->vmem_fnamearr);

        hash_done(&vmem->vmem_hash);

        while (vmem->vmem_lruhead->slot_next != vmem->vmem_lruhead) {
            s = vmem->vmem_lruhead->slot_next;
            ss_dassert(s->slot_nreach == 0);
            lru_remove(s);
            if (vmem->vmem_cacmem != NULL) {
                SsCacMemFree(vmem->vmem_cacmem, s->slot_data);
            }
            SsMemFree(s);
        }
        ss_dassert(vmem->vmem_lruhead->slot_addr == SU_DADDR_NULL);
        SsMemFree(vmem->vmem_lruhead);
        if (vmem->vmem_cacmem != NULL) {
            SsCacMemDone(vmem->vmem_cacmem);
        }

        vmem->vmem_check = 0;

        SsMemFree(vmem);
}

/*##**********************************************************************\
 * 
 *		su_vmem_rewrite
 * 
 * Truncates the vmem zero length
 * 
 * Parameters : 
 * 
 *	vmem - in out, use
 *		vmem pointer
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_vmem_rewrite(su_vmem_t* vmem)
{
        slot_t* s;
        su_vmem_info_t* info;
        uint i;
        char* fname;

        ss_dprintf_1(("su_vmem_rewrite:%d\n", (int)vmem));
        ss_dassert(vmem->vmem_check == VMEM_CHECK);

        /* Remove all slots from hash */
        s = vmem->vmem_lruhead->slot_next;
        while (s != vmem->vmem_lruhead) {
            if (s->slot_addr != SU_DADDR_NULL) {
                hash_remove(&vmem->vmem_hash, s);
                s->slot_addr = SU_DADDR_NULL;
            }
            s->slot_dirty = FALSE;
            ss_dassert(s->slot_nreach == 0);
            s = s->slot_next;
        }
        vmem->vmem_logicalsize = 0L;
        vmem->vmem_physicalsize = 0L;
        info = vmem->vmem_info;
        if (info != NULL) {
            info->vmemi_nfind = 0;
            info->vmemi_nread = 0;
            info->vmemi_nwrite = 0;
            su_vmem_getinfo(vmem, info);
        }
        su_svf_done(vmem->vmem_file);
        i = su_pa_nelems(vmem->vmem_fnamearr);
        ss_dassert(i > 0);
        if (i != 0) {
            for (;;) {
                i--;
                fname = su_pa_getdata(vmem->vmem_fnamearr, i);
                SsFRemove(fname);
                if (i != 0) {
                    su_pa_remove(vmem->vmem_fnamearr, i);
                    vmem->vmem_releasefname_callback(vmem->vmem_callback_ctx,
                                                     i,
                                                     fname);
                } else {
                    break;
                }
            }
            vmem->vmem_file =
                vmem_fileopen(fname,
                              vmem->vmem_blocksize,
                              vmem->vmem_openflags);
            ss_assert(vmem->vmem_file != NULL);
        }
}

/*##**********************************************************************\
 * 
 *		su_vmem_removebuffers
 * 
 * Removes all buffers from vmem. Before the vmem object can
 * be used again, a call to su_vmem_addbuffers() must be made.
 * 
 * Parameters : 
 * 
 *	vmem - in out, use
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
void su_vmem_removebuffers(su_vmem_t* vmem)
{
        slot_t* s;

        ss_dprintf_1(("su_vmem_removebuffers:%d\n", (int)vmem));
        ss_dassert(vmem->vmem_check == VMEM_CHECK);

        /* save all dirty blocks */
        s = vmem->vmem_lruhead->slot_next;
        while (s != vmem->vmem_lruhead) {
            if (s->slot_dirty) {
                slot_write(vmem, s);
            }
            s = s->slot_next;
        }

        while (vmem->vmem_lruhead->slot_next != vmem->vmem_lruhead) {
            s = vmem->vmem_lruhead->slot_next;
            ss_dassert(s->slot_nreach == 0);
            if (s->slot_addr != SU_DADDR_NULL) {
                hash_remove(&vmem->vmem_hash, s);
            }
            lru_remove(s);
            if (vmem->vmem_cacmem != NULL) {
                SsCacMemFree(vmem->vmem_cacmem, s->slot_data);
            }
            SsMemFree(s);
        }
        ss_dassert(vmem->vmem_lruhead->slot_addr == SU_DADDR_NULL);
        if (vmem->vmem_cacmem != NULL) {
            SsCacMemDone(vmem->vmem_cacmem);
            vmem->vmem_cacmem = NULL;
        }
        vmem->vmem_nslot = 0;
}

/*##**********************************************************************\
 * 
 *		su_vmem_addbuffers
 * 
 * Adds new cache buffers to vmem
 * 
 * Parameters : 
 * 
 *	vmem - in out, use
 *		vmem pointer
 *		
 *	nblock - in
 *		# of new slots
 *		
 *	blocks - in, hold
 *		array of pointers to buffers. Reference to those
 *          pointers is held for the lifetime of the vmem object.
 *          the pointer array is not referenced after this call.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_vmem_addbuffers(
        su_vmem_t* vmem,
        uint nblocks,
        void* blocks)   /* void* blocks[nblocks], actually */
{
        slot_t* s;
        su_vmem_info_t* info;
        uint i;

        ss_dprintf_1(("su_vmem_addbuffers:%d, nblocks=%d\n", (int)vmem, nblocks));
        ss_dassert(vmem->vmem_check == VMEM_CHECK);
        ss_dassert(blocks != NULL);
        ss_dassert(nblocks > 0);

        /* Remove all slots from hash */
        s = vmem->vmem_lruhead->slot_next;
        while (s != vmem->vmem_lruhead) {
            if (s->slot_addr != SU_DADDR_NULL) {
                hash_remove(&vmem->vmem_hash, s);
            }
            s = s->slot_next;
        }
        hash_resize(&vmem->vmem_hash, vmem->vmem_hash.h_tablesize + nblocks);
        /* Put all slots with address back to hash table */
        s = vmem->vmem_lruhead->slot_next;
        while (s != vmem->vmem_lruhead) {
            if (s->slot_addr != SU_DADDR_NULL) {
                hash_insert(&vmem->vmem_hash, s);
            }
            s = s->slot_next;
        }
        for (i = 0; i < nblocks; i++) {
            s = SsMemAlloc(sizeof(slot_t));
            s->slot_addr = SU_DADDR_NULL;
            s->slot_data = ((void**)blocks)[i];
            s->slot_dirty = FALSE;
            s->slot_nreach = 0;
            lru_insert(vmem->vmem_lruhead->slot_prev, s);
        }
        vmem->vmem_nslot += nblocks;

        info = vmem->vmem_info;
        if (info != NULL) {
            info->vmemi_nfind = 0;
            info->vmemi_nread = 0;
            info->vmemi_nwrite = 0;
            su_vmem_getinfo(vmem, info);
        }
}

/*##**********************************************************************\
 * 
 *		su_vmem_sizeinblocks
 * 
 * Returns virtual memory size in blocks.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 * Return value : 
 * 
 *      vmem size in bytes
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
vmem_addr_t su_vmem_sizeinblocks(su_vmem_t* vmem)
{
        ss_dassert(vmem->vmem_check == VMEM_CHECK);

        return(vmem->vmem_logicalsize);
}

/*##**********************************************************************\
 * 
 *		su_vmem_blocksize
 * 
 * Returns the virtual memory block size.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 * Return value : 
 * 
 *      block size in bytes
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint su_vmem_blocksize(su_vmem_t* vmem)
{
        ss_dassert(vmem->vmem_check == VMEM_CHECK);

        return(vmem->vmem_blocksize);
}

/*##**********************************************************************\
 * 
 *		su_vmem_reach
 * 
 * Reaches a given virtual memory address and ensures that it stays in 
 * memory until it is released.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 *	addr - in
 *		virtual memory address
 *
 *	p_byte_c - out
 *		number of bytes accessible through returned pointer
 *
 * Return value - ref : 
 * 
 *      pointer to the virtual memory block in central memory or
 *      NULL when failure
 * 
 * Limitations  : 
 * 
 *      The address must be a multiple of virtual memory block size.
 * 
 * Globals used : 
 */
char* su_vmem_reach(su_vmem_t* vmem, vmem_addr_t addr, uint* p_byte_c)
{
        bool succp;
        slot_t* s;

        ss_dprintf_1(("su_vmem_reach:%d, addr=%d\n", (int)vmem, addr));
        ss_dassert(vmem->vmem_check == VMEM_CHECK);
        ss_dassert(vmem->vmem_nslot > 0);
        ss_dassert(addr < vmem->vmem_logicalsize);

        if (vmem->vmem_info != NULL) {
            vmem->vmem_info->vmemi_nfind++;
        }

        /* search addr from slots */
        s = hash_search(&vmem->vmem_hash, addr);
        if (s != NULL) {
            ss_dassert(s->slot_addr == addr);
            s->slot_nreach++;
            *p_byte_c = vmem->vmem_blocksize;
            if (s != vmem->vmem_lruhead->slot_next) {
                lru_remove(s);
                lru_insert(vmem->vmem_lruhead, s);
            }
            ss_dprintf_2(("su_vmem_reach:%d, addr found in cache, *p_byte_c=%d\n", (int)vmem, *p_byte_c));
            return(s->slot_data);
        }
        /* Try to find any slot with zero reservations. */
        s = vmem->vmem_lruhead->slot_prev;
        while (s != vmem->vmem_lruhead) {
            if (s->slot_nreach == 0) {
                succp = slot_reach(vmem, s, addr, TRUE);
                if (!succp) {
                    return (NULL);
                }
                *p_byte_c = vmem->vmem_blocksize;
                ss_dprintf_2(("su_vmem_reach:%d, addr NOT found in cache, *p_byte_c=%d\n", (int)vmem, *p_byte_c));
                return(s->slot_data);
            }
            s = s->slot_prev;
        }
        ss_derror;
        return(NULL);
}

/*##**********************************************************************\
 * 
 *		su_vmem_reachnew
 * 
 * Reaches a new, empty virtual memory address and ensures that it stays
 * in memory until it is released. The content of returned data buffer
 * is undefined.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 *	p_addr - out
 *		pointer to new virtual memory address
 *
 *	p_byte_c - out
 *		number of bytes accessible through returned pointer
 *
 * Return value - ref : 
 * 
 *      pointer to the virtual memory block in central memory
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* su_vmem_reachnew(su_vmem_t* vmem, vmem_addr_t* p_addr, uint* p_byte_c)
{
        slot_t* s;
        vmem_addr_t addr;
        bool succp;

        ss_dprintf_1(("su_vmem_reachnew:%d\n", (int)vmem));
        ss_dassert(vmem->vmem_check == VMEM_CHECK);
        ss_dassert(vmem->vmem_nslot > 0);

        if (vmem->vmem_info != NULL) {
            vmem->vmem_info->vmemi_nfind++;
        }

        addr = vmem->vmem_logicalsize;

        *p_addr = addr;
        *p_byte_c = vmem->vmem_blocksize;

        vmem->vmem_logicalsize = addr + 1;

        /* Try to find any slot with zero reservations. */
        s = vmem->vmem_lruhead->slot_prev;
        while (s != vmem->vmem_lruhead) {
            if (s->slot_nreach == 0) {
                succp = slot_reach(vmem, s, addr, FALSE);
                if (!succp) {
                    return (NULL);
                }
                ss_dprintf_2(("su_vmem_reachnew:%d, *p_addr=%d, *p_byte_c=%d\n",
                    (int)vmem, *p_addr, *p_byte_c));
                return(s->slot_data);
            }
            s = s->slot_prev;
        }
        ss_derror;
        return(NULL);
}

/*##**********************************************************************\
 * 
 *		su_vmem_syncsizeifneeded
 * 
 * Synchronizes the file size of the vmem if that is necessary
 * 
 * Parameters : 
 * 
 *	vmem - use
 *		vmem object
 *		
 * Return value :
 *      TRUE - sync successful or unnecessary
 *      FALSE - file size change failed (disk full?)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_vmem_syncsizeifneeded(su_vmem_t* vmem)
{
        ss_dprintf_1(("su_vmem_syncsizeifneeded:%d\n", (int)vmem));

        if (vmem->vmem_logicalsize > vmem->vmem_physicalsize) {
            if (vmem->vmem_nslot < vmem->vmem_logicalsize) {
                slot_t* s;

                ss_dprintf_2(("su_vmem_syncsizeifneeded:%d, do sync\n", (int)vmem));

                s = vmem->vmem_lruhead->slot_next;
                while (s != vmem->vmem_lruhead) {
                    if (s->slot_addr != SU_DADDR_NULL
                    &&  s->slot_addr + 1 >=
                        vmem->vmem_logicalsize)
                    {
                        if (s->slot_dirty) {
                            bool succp = slot_write(vmem, s);
                            if (!succp) {
                                return (FALSE);
                            }
                        }
                    }
                    s = s->slot_next;
                }
                /* Fall to the
                 * if vmem->vmem_physicalsize != vmem->vmem_logicalsize)
                 * test
                 */
            } else {
                /* otherwise the whole data fits into buffers,
                 * no need to sync to file at this stage
                 */
                return (TRUE);
            }
        }
        ss_dassert(vmem->vmem_physicalsize == vmem->vmem_logicalsize);
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		su_vmem_release
 * 
 * Releases virtual memory address.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 *	addr - in
 *		virtual memory block address
 *
 *	wrote - in
 *		TRUE is the block content is modified, orherwise FALSE
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_vmem_release(su_vmem_t* vmem, vmem_addr_t addr, bool wrote)
{
        slot_t* s;

        ss_dprintf_1(("su_vmem_release:%d, addr=%d, wrote=%d\n", (int)vmem, addr, wrote));
        ss_dassert(vmem->vmem_check == VMEM_CHECK);
        ss_dassert(vmem->vmem_nslot > 0);
        ss_dassert(addr < vmem->vmem_logicalsize);

        /* search addr from slots */
        s = hash_search(&vmem->vmem_hash, addr);
        ss_assert(s != NULL);

        ss_dassert(s->slot_addr == addr);
        ss_dassert(s->slot_nreach > 0);
        s->slot_nreach--;
        s->slot_dirty |= wrote;
}

/*##**********************************************************************\
 * 
 *		su_vmem_setinfo
 * 
 * Sets the info gathering on for a virtual memory. Returns the initial
 * vmem into in parameter info. The user given info buffer is used during
 * vmem operations for temporary storage.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 *	info - in, hold
 *		pointer to the info structure which is initialized
 *          and into where the initial info is stored
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_vmem_setinfo(su_vmem_t* vmem, su_vmem_info_t* info)
{
        vmem->vmem_info = info;

        info->vmemi_nfind = 0;
        info->vmemi_nread = 0;
        info->vmemi_nwrite = 0;

        su_vmem_getinfo(vmem, info);
}

/*##**********************************************************************\
 * 
 *		su_vmem_getinfo
 * 
 * Updates the vmem info structure. The info structure must be the same
 * given in vmem_setinfo.
 * 
 * Parameters : 
 * 
 *	vmem - in, use
 *		vmem pointer
 *
 *	info - out, use
 *		pointer to the vmem info structure into where the info
 *          is stored
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void su_vmem_getinfo(su_vmem_t* vmem, su_vmem_info_t* info)
{
        int     i;
        int     n;
        slot_t* slot;

        if (info->vmemi_nfind > 0) {
            info->vmemi_readhitrate =
                100.0 * (info->vmemi_nfind - info->vmemi_nread) /
                info->vmemi_nfind;
        } else {
            info->vmemi_readhitrate = 0.0;
        }

        info->vmemi_minchain = INT_MAX;
        info->vmemi_maxchain = 0;
        info->vmemi_avgchain = 0.0;
        info->vmemi_nslot = vmem->vmem_hash.h_tablesize;
        info->vmemi_nchain = 0;
        info->vmemi_nitem = 0;

        for (i = 0; i < (int)vmem->vmem_hash.h_tablesize; i++) {
            slot = vmem->vmem_hash.h_table[i];
            if (slot != NULL) {
                info->vmemi_nchain++;
                n = 0;
                do {
                    info->vmemi_nitem++;
                    n++;
                    slot = slot->slot_hashnext;
                } while (slot != NULL);
                info->vmemi_minchain = SS_MIN(info->vmemi_minchain, n);
                info->vmemi_maxchain = SS_MAX(info->vmemi_maxchain, n);
            }
        }

        if (info->vmemi_nchain > 0) {
            info->vmemi_avgchain = (double)info->vmemi_nitem / (double)info->vmemi_nchain;
        } else {
            info->vmemi_avgchain = 0.0;
        }
}
