/*************************************************************************\
**  source       * dbe6bkey.c
**  directory    * dbe
**  description  * B+-tree key value system.
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

This module implements the database index key value systems.  There
are two different kind of key values available: index keys and leaf
keys. Leaf keys are those keys that are stored into leaf node of the
B+-tree. They contain the actual user data. Index keys are those keys
that are used in upper levels of the B+-tree. They are used only for
navigating in the tree to find the correct leaf. All functions work
for both kind of keys. The key type is specified in the info byte of
the key value.

Key values consist of a key header and a variable length v-tuple. The
key header is different for leaf and index keys. The header structure
is defined by the info bytes in the info byte.

Key format:

        +-----------------+
        | key header      |
        +-----------------+
        | variable length |
        | v-tuple         |
        +-----------------+

Leaf key value header format in Bonsai-tree:

        +-------------------------------+
        | info byte                     |
        +-------------------------------+
        | v-tuple mismatch index to the |
        | previous key value            |
        +-------------------------------+
        | transaction number            |
        +-------------------------------+
        | transaction id                |
        +-------------------------------+

Index key value header format in Bonsai-tree:

        +-------------------------------+
        | info byte                     |
        +-------------------------------+
        | v-tuple mismatch index to the |
        | previous key value            |
        +-------------------------------+
        | Address of the node below     |
        | this node.                    |
        +-------------------------------+
        | transaction id                |
        +-------------------------------+

Leaf key value header format in permanent tree:

        +-------------------------------+
        | info byte                     |
        +-------------------------------+
        | v-tuple mismatch index to the |
        | previous key value            |
        +-------------------------------+

Index key value header format in permanent tree:

        +-------------------------------+
        | info byte                     |
        +-------------------------------+
        | v-tuple mismatch index to the |
        | previous key value            |
        +-------------------------------+
        | Address of the node below     |
        | this node.                    |
        +-------------------------------+

As can be seen above, the leaf and index key value formats are otherwise
equal, but the index key value contains an address field instead of
the transaction number.

The transaction id is a unique id that identifies the transaction. It is
used for key value ordering when v-tuple parts are equal.

The transaction number is the transaction serialization number. It is
also used as a time stamp to decide which key values are visible
to searches.

Key values can be compressed. Certain functions expect that some
input key values are compressed, other do not work with compressed
key values.

A special search structure is used to search a keys value from a sequence
of compressed key values. This search is used to search a key value
(or key value position for insert) from the index tree leaf.

Key value ordering:

        Three different parts of the key value are used for ordering.

        1. The primary ordering is done using the v-tuple value.

        2. When v-tuple values are the same, tuples are ordered by the
           delemark bit. The key value with a delete mark bit set becomes
           before the key value with the delete mark bit not set.

        3. When both v-tuple value and delete mark status are equal,
           tuples are ordered by the transaction id field. The key value
           with a smaller transaction id becomes first. That is, key values
           are ordered in ascending order by the transaction id.

Delete mark logic:

        The v-tuple part in a delete mark is the same as in the key
        that the delete mark deletes.

        The deletemark bit is set in the delete mark key value.

        The transaction number is the commit number of the transaction
        that has added the delete mark.

        The transaction id is the id of the transaction that has
        added the delete mark. It cannot be from the delete mark,
        because then the index would contain equal key values
        (same v-tuple, transaction id and delete mark status).

Examples:

        Fields in the example are
        1. delete mark status, i=insert, d=delete
        2. transaction id
        3. v-tuple value (version number in parenthessi)

        begin transaction 1

            insert key 'abc':
            [i][1]'abc'()

        commit transaction 1

        begin transaction 2

            update 'abc' to 'abc':
            [i][2]'abc'(1)
            [d][2]'abc'()
            [i][1]'abc'()

            update 'abc' to 'abc' (old value removed physically):
            [i][2]'abc'(2)
            [d][2]'abc'()
            [i][1]'abc'()

            update 'abc' to 'cde' (old value removed physically):
            [d][2]'abc'()
            [i][1]'abc'()
            [i][2]'cde'(3)

            update 'cde' to 'fgh' (old value removed physically):
            [d][2]'abc'()
            [i][1]'abc'()
            [i][2]'fgh'(4)

            delete 'fgh' (old value removed physically):
            [d][2]'abc'()
            [i][1]'abc'()

        commit transaction 2

Limitations:
-----------

Key values have a fixed maximum length.

Error handling:
--------------

All errors are handled using asserts.

Objects used:
------------

v-tuples    uti0va, uti0vtpl, uti0vcmp

Preconditions:
-------------


Multithread considerations:
--------------------------

The code is fully reentrant.

Example:
-------

        bool find_key(node_t* node, dbe_trxnum_t trxnum, vtpl_t* vtpl)
        {
            dbe_bkey_t* k = dbe_bkey_initleaf(trxnum, vtpl);
            dbe_bkey_t* current_key;
            dbe_bkey_search_t* ks;
            int cmp;

            dbe_bkey_search_init(ks, k);

            current_key = get_first_key_of_the_node(node);

            while (more_key_values(node)) {
                dbe_bkey_search_step(ks, current_key, cmp);
                if (cmp <= 0) {
                    break;
                }
                current_key = get_next_key_of_the_node(node, current_key);
            }

            dbe_bkey_done(k);

            return(cmp == 0);
        }

**************************************************************************
#endif /* DOCUMENTATION */

#define DBE6BKEY_C

#include <ssstdio.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sslimits.h>
#include <sssprint.h>
#include <sschcvt.h>
#include <ssthread.h>
#include <ssmsglog.h>

#include <su0svfil.h>

#include <uti0va.h>
#include <uti0vtpl.h>
#include <uti0vcmp.h>

#include <rs0key.h>
#include <rs0sdefs.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0atype.h>
#include <rs0aval.h>

#include "dbe7cfg.h"
#include "dbe7binf.h"
#include "dbe6bkey.h"
#include "dbe6log.h"
#include "dbe6bmgr.h"

/*##**********************************************************************\
 *
 *		dbe_bkey_getbloblimit_low
 *
 * Calculates the lower blob length limit. If the key total length is
 * still too large afhetr removing blob attributes using the higher limit,
 * then this lower limit is used.
 *
 * Parameters :
 *
 *	maxkeylen -
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
va_index_t dbe_bkey_getbloblimit_low(
        uint maxkeylen)
{
        SS_NOTUSED(maxkeylen);

        return(VA_LENGTHMAXLEN + RS_KEY_MAXCMPLEN + RS_VABLOBG2REF_SIZE);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_getbloblimit_high
 *
 * Calculates the higher blob length limit. This higher limit is first
 * used when storing attributes as blobs. If the total length is still
 * larger than allowed limit, then lower blob limit is used.
 *
 * Parameters :
 *
 *	maxkeylen -
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
va_index_t dbe_bkey_getbloblimit_high(
        uint maxkeylen)
{
        /* There is no real reason for VA_LENGTHMAXLEN below, it is
         * decremented just to make sure that we are not exactly at the
         * maxlen limit. One byte decrement should be equally well.
         */
        return(maxkeylen - BKEY_MAXHEADERLEN - VA_LENGTHMAXLEN);
}

/*##**********************************************************************\
 *
 *		dbe_bkeyinfo_init
 *
 *
 *
 * Parameters :
 *
 *	bkeyinfo -
 *
 *
 *	maxkeylen -
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
void dbe_bkeyinfo_init(
        dbe_bkeyinfo_t* bkeyinfo,
        uint maxkeylen)
{
        bkeyinfo->ki_maxkeylen = maxkeylen;
        ss_debug(bkeyinfo->ki_chk = DBE_CHK_BKEYINFO);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_test
 *
 * Tests if a key value is consistent.
 *
 * Parameters :
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      TRUE - to return something so that this can be used in
 *             assert macros
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bkey_test(dbe_bkey_t* k)
{
        TWO_BYTE_T index;

        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_TYPEOK(k));
        index = BKEY_LOADINDEX(k);
        ss_dassert((vtpl_index_t)index < BKEY_MAXLEN);
        ss_dassert(vtpl_grosslen(BKEY_GETVTPLPTR(k)) < BKEY_MAXLEN);
        ss_dassert(dbe_bkey_getlength(k) < BKEY_MAXLEN);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_init
 *
 * Initializes an empty leaf key value.
 *
 * Parameters :
 *
 * Return value - give :
 *
 *      pointer to a key value
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bkey_t* dbe_bkey_init(dbe_bkeyinfo_t* ki)
{
        dbe_bkey_t* k;

        CHK_BKEYINFO(ki);
        ss_dassert(BKEY_INFOSIZE == 1);
        ss_dassert(BKEY_INDEXSIZE == 2);
        ss_dassert(BKEY_TRXNUMSIZE == 4);
        ss_dassert(BKEY_ADDRSIZE == 4);
        ss_dassert(BKEY_TRXIDSIZE == 4);
        SS_MEMOBJ_INC(SS_MEMOBJ_BKEY, dbe_bkey_t);

        k = SsMemAlloc(ki->ki_maxkeylen);

        dbe_bkey_initlongleafbuf(k);

        ss_dassert(dbe_bkey_checkheader(k));

        return(k);
}

/*#***********************************************************************\
 *
 *		bkey_initleafbuf
 *
 * Initializes key buffer as a storage tree leaf key value with initial
 * empty values.
 *
 * Parameters :
 *
 *      k - use
 *          key buffer
 *
 *      trxnum - in
 *		transaction number of the key
 *
 *      trxid - in
 *		transaction id of the key
 *
 *	vtpl - in, use
 *		initial key content
 *
 * Return value - give :
 *
 *      pointer to a leaf key value
 *
 * Limitations  :
 *
 * Globals used :
 */
static void bkey_initleafbuf(
        dbe_bkey_t* k,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        vtpl_t* vtpl)
{
        ss_dassert(BKEY_MAXHEADERLEN + vtpl_grosslen(vtpl) < BKEY_MAXLEN);

        BKEY_INITINFO(k, BKEY_LEAF|BKEY_2LONGUSED);
        BKEY_STOREINDEX(k, 0);
        BKEY_STORETRXNUM(k, trxnum);
        BKEY_STORETRXID(k, trxid);
        vtpl_setvtpl(BKEY_GETVTPLPTR(k), vtpl);

        ss_dassert(dbe_bkey_checkheader(k));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_initshortleafbuf
 *
 *
 *
 * Parameters :
 *
 *	k -
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
void dbe_bkey_initshortleafbuf(
        dbe_bkey_t* k)
{
        BKEY_INITINFO(k, BKEY_LEAF);
        BKEY_STOREINDEX(k, 0);
        vtpl_setvtpl(BKEY_GETVTPLPTR(k), VTPL_EMPTY);

        ss_dassert(dbe_bkey_checkheader(k));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_initlongleafbuf
 *
 * Initializes key buffer as a bonsai tree leaf key value with initial
 * empty values.
 *
 * Parameters :
 *
 *	k -
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
void dbe_bkey_initlongleafbuf(
        dbe_bkey_t* k)
{
        ss_dassert(k != NULL);

        BKEY_INITINFO(k, BKEY_LEAF|BKEY_2LONGUSED);
        BKEY_STOREINDEX(k, 0);
        BKEY_STORETRXNUM(k, DBE_TRXNUM_NULL);
        BKEY_STORETRXID(k, DBE_TRXID_NULL);
        vtpl_setvtpl(BKEY_GETVTPLPTR(k), VTPL_EMPTY);

        ss_dassert(dbe_bkey_checkheader(k));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_initpermleaf
 *
 *
 *
 * Parameters :
 *
 *	vtpl -
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
dbe_bkey_t* dbe_bkey_initpermleaf(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        vtpl_t* vtpl)
{
        dbe_bkey_t* k = NULL;

        CHK_BKEYINFO(ki);
        ss_dassert(BKEY_MAXHEADERLEN + vtpl_grosslen(vtpl) < ki->ki_maxkeylen);

        if (cd != NULL) {
            k = rs_sysi_getbkeybuf(cd);
        }
        if (k == NULL) {
            k = SsMemAlloc(ki->ki_maxkeylen);
        }

        BKEY_INITINFO(k, BKEY_LEAF);
        BKEY_STOREINDEX(k, 0);
        vtpl_setvtpl(BKEY_GETVTPLPTR(k), vtpl);

        ss_dassert(dbe_bkey_checkheader(k));

        return(k);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_initleaf
 *
 * Initializes a leaf key value with initial values.
 *
 * Parameters :
 *
 *      trxnum - in
 *		transaction number of the key
 *
 *      trxid - in
 *		transaction id of the key
 *
 *	vtpl - in, use
 *		initial key content
 *
 * Return value - give :
 *
 *      pointer to a leaf key value
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bkey_t* dbe_bkey_initleaf(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        vtpl_t* vtpl)
{
        dbe_bkey_t* k = NULL;

        CHK_BKEYINFO(ki);
        ss_dassert(BKEY_MAXHEADERLEN + vtpl_grosslen(vtpl) < ki->ki_maxkeylen);

        if (cd != NULL) {
            k = rs_sysi_getbkeybuf(cd);
        }
        if (k == NULL) {
            k = SsMemAlloc(ki->ki_maxkeylen);
        }

        bkey_initleafbuf(k, trxnum, trxid, vtpl);

        ss_dassert(dbe_bkey_checkheader(k));

        return(k);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_initindex
 *
 * Initializes an index key value with initial values.
 *
 * Parameters :
 *
 *      trxid - in
 *		transaction id of the key
 *
 *	addr - in
 *		node address stored into key
 *
 *	vtpl - in, use
 *		initial key content
 *
 * Return value - give :
 *
 *      pointer to a key value
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bkey_t* dbe_bkey_initindex(
        dbe_bkeyinfo_t* ki,
        dbe_trxid_t trxid,
        su_daddr_t addr,
        vtpl_t* vtpl)
{
        dbe_bkey_t* k;

        CHK_BKEYINFO(ki);
        ss_dassert(BKEY_MAXHEADERLEN + vtpl_grosslen(vtpl) < ki->ki_maxkeylen);
        SS_MEMOBJ_INC(SS_MEMOBJ_BKEY, dbe_bkey_t);

        k = SsMemAlloc(ki->ki_maxkeylen);

        BKEY_INITINFO(k, BKEY_2LONGUSED);
        BKEY_STOREINDEX(k, 0);
        BKEY_STORETRXID(k, trxid);
        BKEY_STOREADDR(k, addr);
        vtpl_setvtpl(BKEY_GETVTPLPTR(k), vtpl);

        ss_dassert(dbe_bkey_checkheader(k));

        return(k);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_done
 *
 * Releases a key value. The released key value must be the one created
 * with a function dbe_bkey_init*.
 *
 * Parameters :
 *
 *	k - in, take
 *		key value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_done(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_TYPEOK(k));
        SS_MEMOBJ_DEC(SS_MEMOBJ_BKEY);

        SsMemFree(k);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setvtpl
 *
 * Sets the key value content from a v-tuple.
 *
 * Parameters :
 *
 *	tk - in out, use
 *		target key
 *
 *	vtpl - in, use
 *		v-tuple stored into tk
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_setvtpl(dbe_bkey_t* tk, vtpl_t* vtpl)
{
        ss_dassert(tk != NULL);
        ss_dassert(dbe_bkey_checkheader(tk));
        ss_dassert(BKEY_TYPEOK(tk));

        ss_dassert(BKEY_HEADERLEN(tk) + vtpl_grosslen(vtpl) < BKEY_MAXLEN);

        BKEY_STOREINDEX(tk, 0);
        vtpl_setvtpl(BKEY_GETVTPLPTR(tk), vtpl);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_settreeminvtpl
 *
 * Create a key value that is smaller than any possible key
 * value in the index tree.
 *
 * Parameters :
 *
 *	tk -
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
void dbe_bkey_settreeminvtpl(dbe_bkey_t* tk)
{
        ss_byte_t vtpl_buf[20];
        ss_byte_t va_buf[20];
        vtpl_t* vtpl = (vtpl_t*)&vtpl_buf[0];
        va_t* va = (va_t*)&va_buf[0];

        va_setdata(va, (char *)"\0", 1);

        vtpl_setvtpl(vtpl, VTPL_EMPTY);
        vtpl_appva(vtpl, va);

        dbe_bkey_setvtpl(tk, vtpl);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setsearchminvtpl
 *
 * Create a key value that is smaller than any possible key
 * value in the index search.
 *
 * Parameters :
 *
 *	tk -
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
void dbe_bkey_setsearchminvtpl(dbe_bkey_t* tk)
{
        ss_byte_t vtpl_buf[20];
        vtpl_t* vtpl = (vtpl_t*)&vtpl_buf[0];

        vtpl_setvtpl(vtpl, VTPL_EMPTY);
        vtpl_appva(vtpl, &va_minint);

        dbe_bkey_setvtpl(tk, vtpl);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setsearchmaxvtpl
 *
 * Create a key value that is greater than any possible key
 * value in the index search.
 *
 * Parameters :
 *
 *	tk -
 *
 *
 * Return value :
 *
 * Comments :
 *
 *      WARNING! This code expects that a maximum v-attribute
 *      is known. The code should work if every key value
 *      is always preceded with a key id.
 *
 * Globals used :
 *
 * See also :
 */
void dbe_bkey_setsearchmaxvtpl(dbe_bkey_t* tk)
{
        ss_byte_t vtpl_buf[20];
        vtpl_t* vtpl = (vtpl_t*)&vtpl_buf[0];

        vtpl_setvtpl(vtpl, VTPL_EMPTY);
        vtpl_appva(vtpl, &va_maxint);

        dbe_bkey_setvtpl(tk, vtpl);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setbkey
 *
 *
 *
 * Parameters :
 *
 *	tk -
 *
 *
 *	sk -
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
void dbe_bkey_setbkey(dbe_bkey_t* tk, dbe_bkey_t* sk)
{
        uint sk_info;

        ss_dassert(tk != NULL);
        ss_dassert(sk != NULL);
        ss_dassert(dbe_bkey_checkheader(tk));
        ss_dassert(dbe_bkey_checkheader(sk));
        ss_dassert(BKEY_TYPEOK(tk));
        ss_dassert(BKEY_TYPEOK(sk));

        BKEY_STOREINDEX(tk, 0);
        vtpl_setvtpl(BKEY_GETVTPLPTR(tk), BKEY_GETVTPLPTR(sk));

        sk_info = BKEY_GETINFO(sk);

        if (dbe_bkey_istrxid(tk) && dbe_bkey_istrxid(sk)) {
            dbe_bkey_settrxid(tk, dbe_bkey_gettrxid(sk));
        }
        if (dbe_bkey_isdeletemark(sk)) {
            dbe_bkey_setdeletemark(tk);
        }

        if (dbe_bkey_isupdate(sk)) {
            dbe_bkey_setupdate(tk);
        }

        BKEY_INITINFO(tk, BKEY_GETINFO(tk) & ~(BKEY_BLOB|BKEY_CLUSTERING));
        if (sk_info & BKEY_BLOB) {
            BKEY_SETINFO(tk, BKEY_BLOB);
        }
        if (sk_info & BKEY_CLUSTERING) {
            BKEY_SETINFO(tk, BKEY_CLUSTERING);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bkey_getmismatchindex
 *
 * Returns the mismatch index of the key value.
 *
 * Parameters :
 *
 *	k - in use
 *		Key value the mismatch index of which is returned.
 *
 * Return value :
 *
 *      Mismatch index of key value.
 *
 * Limitations  :
 *
 * Globals used :
 */
vtpl_index_t dbe_bkey_getmismatchindex(dbe_bkey_t* k)
{
        TWO_BYTE_T index;

        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_TYPEOK(k));

        index = BKEY_LOADINDEX(k);

        return((vtpl_index_t)index);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_setaddr
 *
 * Sets the addres of a key value. The key value must be an index
 * key value.
 *
 * Parameters :
 *
 *	k - in out use
 *		Key value the address of which is set.
 *
 *	addr - in
 *		The new address of key value.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_setaddr(dbe_bkey_t* k, su_daddr_t addr)
{
        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert((BKEY_GETINFO(k) & BKEY_LEAF) == 0);
        ss_dassert((BKEY_GETINFO(k) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) != 0);

        BKEY_STOREADDR(k, addr);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_removetrxinfo
 *
 *
 *
 * Parameters :
 *
 *	k - in out, use
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_removetrxinfo(dbe_bkey_t* k)
{
        vtpl_t* old_vtpl;
        uint info;

        ss_dassert(k != NULL);

        old_vtpl = dbe_bkey_getvtpl(k);

        info = BKEY_GETINFO(k);

        if (BKEY_ISLEAF(k)) {
            BKEY_INITINFO(k, info & ~(BKEY_1LONGUSED|BKEY_2LONGUSED));
            ss_dassert(!dbe_bkey_istrxid(k));
            ss_dassert(!dbe_bkey_istrxnum(k));
        } else {
            BKEY_INITINFO(k, info & ~BKEY_2LONGUSED);
            BKEY_SETINFO(k, BKEY_1LONGUSED);
            ss_dassert(!dbe_bkey_istrxnum(k));
        }

        memmove(
            dbe_bkey_getvtpl(k),
            old_vtpl,
            (size_t)vtpl_grosslen(old_vtpl));
}

#ifdef DBE_MERGEDEBUG

void dbe_bkey_setmergedebuginfo(
        dbe_bkey_t* k,
        dbe_trxnum_t readlevel,
        int trxmode)
{
        char* p;

        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert((BKEY_GETINFO(k) & BKEY_2LONGUSED) != 0);

        p = BKEY_GETMERGEDEBUGINFOPTR(k);

        SS_UINT4_STORETODISK(p, DBE_TRXNUM_GETLONG(readlevel));
        p += sizeof(ss_uint4_t);
        *p = (ss_byte_t)trxmode;
}

#endif /* DBE_MERGEDEBUG */


/*##**********************************************************************\
 *
 *		dbe_bkey_removedeletemark
 *
 * Removes the deletemark info.
 *
 * Parameters :
 *
 *	k - in out, use
 *		key value
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_removedeletemark(dbe_bkey_t* k)
{
        uint info;

        ss_dassert(k != NULL);
        ss_dassert(dbe_bkey_checkheader(k));

        info = BKEY_GETINFO(k);
        info &= ~BKEY_DELETEMARK;
        BKEY_INITINFO(k, info);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_getkeyid
 *
 * Return key id at the start of bkey v-tuple.
 *
 * Parameters :
 *
 *	k -
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
long dbe_bkey_getkeyid(dbe_bkey_t* k)
{
        vtpl_t* vtpl;
        long keyid;

        vtpl = BKEY_GETVTPLPTR(k);

        if (vtpl_grosslen(vtpl) >= 7 && va_grosslen(vtpl_getva_at(vtpl, 0)) < 10) {
            keyid = va_getlong_check(VTPL_GETVA_AT0(vtpl));
        } else {
            keyid = -1L;
        }
        return(keyid);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_copy_keeptargetformat
 *
 * Copies key by keepingt targer key format. This means that target buffer
 * must contain a valid key.
 *
 * Parameters :
 *
 *	tk -
 *
 *
 *	sk -
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
void dbe_bkey_copy_keeptargetformat(dbe_bkey_t* tk, dbe_bkey_t* sk)
{
        ss_dassert(tk != NULL);
        ss_dassert(sk != NULL);
        ss_dassert(dbe_bkey_checkheader(tk));
        ss_dassert(dbe_bkey_checkheader(sk));
        ss_dassert(BKEY_TYPEOK(tk));
        ss_dassert(BKEY_TYPEOK(sk));

        ss_debug_4(dbe_bkey_test(tk));
        ss_debug_4(dbe_bkey_test(sk));

        if (BKEY_HEADERLEN(tk) == BKEY_HEADERLEN(sk)) {
            memcpy(tk, sk, dbe_bkey_getlength(sk));

        } else {
            /* Need to copy fields separately. We assume target key must
             * be a short key.
             */
            int sinfo;
            TWO_BYTE_T index;

            ss_dassert(BKEY_HEADERLEN(tk) < BKEY_HEADERLEN(sk));

            sinfo = BKEY_GETINFO(sk);
            sinfo = sinfo & ~(BKEY_1LONGUSED|BKEY_2LONGUSED);
            BKEY_SETINFO(tk, sinfo);
            index = BKEY_LOADINDEX(sk);
            BKEY_STOREINDEX(tk, index);
            vtpl_setvtpl(BKEY_GETVTPLPTR(tk), BKEY_GETVTPLPTR(sk));
        }
}

/*##**********************************************************************\
 *
 *		dbe_bkey_copyheader
 *
 * Copies target key header into source key. Compression info is not
 * copied.
 *
 * Parameters :
 *
 *	tk - out, use
 *		target key
 *
 *	sk - in, use
 *		source key
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_copyheader(dbe_bkey_t* tk, dbe_bkey_t* sk)
{
        ss_uint2_t index;

        ss_dassert(tk != NULL);
        ss_dassert(sk != NULL);
        ss_dassert(dbe_bkey_checkheader(sk));
        ss_dassert(BKEY_TYPEOK(sk));

        ss_debug_4(dbe_bkey_test(sk));

        index = BKEY_LOADINDEX(tk);

        memcpy(tk, sk, BKEY_HEADERLEN(sk));

        BKEY_STOREINDEX(tk, index);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_reexpand_delete
 *
 * Expands a compressed key value value after a deleted key. The deleted
 * key value must be given in compressed format. The result is the next
 * key that contains equal bytes from the deleted key value so that it
 * is still possible to construct the original key value.
 *
 * If the compression index of the deleted key value is smaller than
 * in the next key value, there is no need to reexpand the next key value.
 * In such case the next key value is just copied to the target key.
 *
 * This is symmetrical operation with dbe_bkey_recompress_insert.
 *
 * Example:
 *
 *      Initial keys:
 *
 *          key 1i  aabb
 *          key 3i  aacc
 *          key 2i  aacd
 *
 *      Compressed index:
 *
 *          key 1c  0 aabb
 *          key 3c  3 cc
 *          key 2c' 4 d
 *
 *      Delete key 3:
 *
 *      Reexpand key 2:
 *
 *          key 3c  3 cc
 *          key 2c' 4 d
 *          ------------
 *                  3 cd
 *          =>
 *
 *          key 2c  3 cd
 *
 *      New compressed index:
 *
 *          key 1c  0   aabb
 *          key 2c  3   cd
 *
 * Parameters :
 *
 *	tk - out, use
 *		target key
 *
 *	dk - in, use
 *		deleted key in compressed format
 *
 *	nk - in, use
 *		next key after deleted key in compressed format, this key
 *          is reexpanded into tk
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_reexpand_delete(dbe_bkey_t* tk, dbe_bkey_t* dk, dbe_bkey_t* nk)
{
        TWO_BYTE_T dk_index;
        TWO_BYTE_T nk_index;

        ss_dassert(tk != NULL);
        ss_dassert(dk != NULL);
        ss_dassert(nk != NULL);
        ss_dassert(tk != dk);
        ss_dassert(tk != nk);
        ss_dassert(dbe_bkey_checkheader(dk));
        ss_dassert(dbe_bkey_checkheader(nk));
        ss_dassert(BKEY_TYPEOK(dk));
        ss_dassert(BKEY_TYPEOK(nk));

        ss_debug_4(dbe_bkey_test(dk));
        ss_debug_4(dbe_bkey_test(nk));

        dk_index = BKEY_LOADINDEX(dk);
        nk_index = BKEY_LOADINDEX(nk);

        if (dk_index < nk_index) {
            vtpl_index_t index;

            /* copy header part directly */
            memcpy(tk, nk->k_.pval, BKEY_HEADERLEN(nk));

            index = (vtpl_index_t)(nk_index - dk_index);

            vtpl_expand(
                BKEY_GETVTPLPTR(tk),
                BKEY_GETVTPLPTR(dk),
                BKEY_GETVTPLPTR(nk),
                index);

            BKEY_STOREINDEX(tk, dk_index);

        } else {

            dbe_bkey_copy(tk, nk);
        }

        ss_debug_4(dbe_bkey_test(tk));
}

/*##**********************************************************************\
 *
 *      dbe_bkey_init_expand
 *
 * Initializes a vtpl_expand_state object
 *
 * Parameters :
 *
 *  es - out, use
 *      pointer to vtpl_expand_state structure
 *
 */
void dbe_bkey_init_expand( vtpl_expand_state* es )
{
        vtpl_init_expand( es );         /* (see uti0vcmp.c) */
}                                       /* dbe_bkey_init_expand */


/*##**********************************************************************\
 *
 *      dbe_bkey_done_expand
 *
 * Cleans up a vtpl_expand_state object.
 *
 * Parameters :
 *
 *  es - out, use
 *      pointer to vtpl_expand_state structure
 *
 */
void dbe_bkey_done_expand( vtpl_expand_state* es )
{
        vtpl_done_expand( es );
}                                       /* dbe_bkey_done_expand */


/*##**********************************************************************\
 *
 *      dbe_bkey_save_expand
 *
 * Advances a vtpl_expand_state object to the next key in sequence.
 *
 * Determines how much data from the current v-tuple (the key
 * given last time) will be needed to expand the next v-tuple
 * (the key given this time).  Saves that information in the
 * vtpl_expand_state object, along with the given v-tuple ptr.
 * Upon return, the caller can expand the v-tuple by calling
 * dbe_bkey_copy_expand().
 *
 * Parameters :
 *
 *  es - in out
 *      pointer to vtpl_expand_state structure
 *
 *  ck - in, hold
 *      compressed key (the next key in the compression sequence)
 *
 */
void dbe_bkey_save_expand( vtpl_expand_state* es
                         , dbe_bkey_t*        ck
                         )
{
        ss_dassert( ck != NULL );
        ss_dassert( dbe_bkey_checkheader( ck ) );
        ss_dassert( BKEY_TYPEOK( ck ) );

        ss_debug_4( dbe_bkey_test( ck ) );

        vtpl_save_expand( es, BKEY_LOADINDEX( ck ), BKEY_GETVTPLPTR( ck ) );
}                                       /* dbe_bkey_save_expand */


/*##**********************************************************************\
 *
 *      dbe_bkey_copy_expand
 *
 * Expands the current v-tuple and adds the given header.
 *
 * Parameters :
 *
 *  tk - out, use
 *      Pointer to destination area.  Target key is built there.
 *      Destination must not overlap with original compressed key.
 *      Ignored if l_tk_area is less than the required size.
 *
 *  es - in, use
 *      Pointer to vtpl_expand_state structure, containing
 *      the current compressed v-tuple ptr and prefix info
 *      cached by the latest call to dbe_bkey_save_expand().
 *
 *  sk_header - in, use
 *      The header from this key is copied to the target key.
 *
 *  l_tk_area - in
 *      Size of destination area in bytes.
 *
 * Return value :
 *    Size of expanded key (number of bytes).  Always > 0.
 *
 * Note:
 *    Before first use, dbe_bkey_init_expand() must be called to
 *    initialize the vtpl_expand_state structure.  When finished,
 *    dbe_bkey_done_expand() must be called to clean it up.
 */
vtpl_index_t dbe_bkey_copy_expand( dbe_bkey_t*        tk
                                 , vtpl_expand_state* es
                                 , dbe_bkey_t*        sk_header
                                 , vtpl_index_t       l_tk_area
                                 )
{
        vtpl_index_t l_header;
        vtpl_index_t l_tk;

        ss_dassert( sk_header != NULL );
        ss_dassert( dbe_bkey_checkheader( sk_header ) );
        ss_dassert( BKEY_TYPEOK( sk_header ) );

        l_header = BKEY_HEADERLEN( sk_header );

        ss_dassert( tk != NULL );
        ss_dassert( l_tk_area >= l_header );

        /* Expand the key. */
        l_tk = l_header + vtpl_copy_expand( es
                                          , (vtpl_t*)((ss_byte_t*)tk + l_header)
                                          , l_tk_area - l_header
                                          );

        /* Error if target area not big enough. */
        ss_assert( l_tk <= l_tk_area );

        /* Copy header part from sk_header. */
        memcpy( tk, sk_header, l_header );
        BKEY_STOREINDEX( tk, 0 );

        ss_debug_4( dbe_bkey_test( tk ) );

        return l_tk;
}                                       /* dbe_bkey_copy_expand */

/*#***********************************************************************\
 *
 *		bkey_compare_header_gettrxid
 *
 * Returns the trx id of a key value. If no trx id is available,
 * returns 0.
 *
 * Parameters :
 *
 *	k - in, use
 *
 *
 * Return value :
 *
 *      Transaction id.
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_trxid_t bkey_compare_header_gettrxid(dbe_bkey_t* k)
{
        ss_dassert(k != NULL);

        if (BKEY_ISLEAF(k)) {
            switch (BKEY_GETINFO(k) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) {
                case 0:
                    /* no ids */
                    return(dbe_trxid_null);
                case BKEY_1LONGUSED:
                    /* only trxnum */
                    return(dbe_trxid_initfromtrxnum(dbe_bkey_gettrxnum(k)));
                case BKEY_2LONGUSED:
                    /* trxnum and trxid */
                    return(BKEY_GETTRXID(k));
                default:
                    ss_error;
            }
        } else {
            switch (BKEY_GETINFO(k) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) {
                case 0:
                    /* none, impossible in index node */
                    ss_error;
                case BKEY_1LONGUSED:
                    /* only addr */
                    return(dbe_trxid_null);
                case BKEY_2LONGUSED:
                    /* addr and trxid */
                    return(BKEY_GETTRXID(k));
                default:
                    ss_error;
            }
        }
        return(dbe_trxid_null);
}

/*##*********************************************************************\
 *
 *		dbe_bkey_compare_deletemark
 *
 * Compares two key value headers from they deletemark status.
 *
 * If delete mark status are different, the key with a delete mark
 * becomes before a key without delete mark. That is, deleted key
 * values are ordered before inserted key values.
 *
 * Parameters :
 *
 *	k1 - in, use
 *		key value 1
 *
 *	k2 - in, use
 *		key value 2
 *
 * Return value :
 *
 *      < 0     deletemark(k1) && !deletemark(k2)
 *      = 0     deletemark states are same
 *      > 0     !deletemark(k1) && deletemark(k2)
 *
 * Comments  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 */
int dbe_bkey_compare_deletemark(dbe_bkey_t* k1, dbe_bkey_t* k2)
{
        bool k1_isdeletemark;
        bool k2_isdeletemark;

        ss_dassert(k1 != NULL);
        ss_dassert(k2 != NULL);
        ss_dassert(dbe_bkey_checkheader(k1));
        ss_dassert(dbe_bkey_checkheader(k2));
        ss_dassert(BKEY_TYPEOK(k1));
        ss_dassert(BKEY_TYPEOK(k2));

        k1_isdeletemark = BKEY_ISDELETEMARK(k1);
        k2_isdeletemark = BKEY_ISDELETEMARK(k2);

        if (k1_isdeletemark && !k2_isdeletemark) {
            return(-1);
        } else if (!k1_isdeletemark && k2_isdeletemark) {
            return(1);
        } else {
            return(0);
        }
}

/*##*********************************************************************\
 *
 *		dbe_bkey_compare_header
 *
 * Compares two key value headers.
 *
 * If delete mark status are different, the key with a delete mark
 * becomes before a key without delete mark. That is, deleted key
 * values are ordered before inserted key values.
 *
 * If delete mark statuses are different, the key with a smaller transaction
 * id becomes first.
 *
 * Parameters :
 *
 *	k1 - in, use
 *		key value 1
 *
 *	k2 - in, use
 *		key value 2
 *
 * Return value :
 *
 *      < 0     deletemark(k1) && !deletemark(k2) or
 *              deletemark(k1) == deletemark(k2) && trxid(k1) < trxid(k2)
 *      = 0     transaction ids and deletemark states are same
 *      > 0     !deletemark(k1) && deletemark(k2) or
 *              deletemark(k1) == deletemark(k2) && trxid(k1) > trxid(k2)
 *
 * Comments  :
 *
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 *
 * Globals used :
 */
int dbe_bkey_compare_header(dbe_bkey_t* k1, dbe_bkey_t* k2)
{
        int cmp;
        dbe_trxid_t k1_trxid;
        dbe_trxid_t k2_trxid;

        ss_dassert(k1 != NULL);
        ss_dassert(k2 != NULL);
        ss_dassert(dbe_bkey_checkheader(k1));
        ss_dassert(dbe_bkey_checkheader(k2));
        ss_dassert(BKEY_TYPEOK(k1));
        ss_dassert(BKEY_TYPEOK(k2));

        cmp = dbe_bkey_compare_deletemark(k1, k2);

        if (cmp != 0) {
            return(cmp);
        }

        k1_trxid = bkey_compare_header_gettrxid(k1);
        k2_trxid = bkey_compare_header_gettrxid(k2);

        return(DBE_TRXID_CMP_EX(k1_trxid, k2_trxid));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_compress
 *
 * Compresses a key value. Target key can be the same buffer as either
 * previous key or next key.
 *
 * Parameters :
 *
 *	tk - out, use
 *		target key
 *
 *	pk - in, use
 *		previous key
 *
 *	nk - in, use
 *		next key
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_compress(
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* tk,
        dbe_bkey_t* pk,
        dbe_bkey_t* nk)
{
        ss_byte_t* buf = NULL;
        dbe_bkey_t* tmpk;
        TWO_BYTE_T index;

        CHK_BKEYINFO(ki);
        ss_dassert(tk != NULL);
        ss_dassert(pk != NULL);
        ss_dassert(nk != NULL);
        ss_dassert(dbe_bkey_checkheader(pk));
        ss_dassert(dbe_bkey_checkheader(nk));
        ss_dassert(BKEY_TYPEOK(pk));
        ss_dassert(BKEY_TYPEOK(nk));
        ss_debug(index = BKEY_LOADINDEX(pk));
        ss_dassert(index == 0);
        ss_debug(index = BKEY_LOADINDEX(nk));
        ss_dassert(index == 0);

        ss_debug_4(dbe_bkey_test(pk));
        ss_debug_4(dbe_bkey_test(nk));

#ifdef TEST
        ss_dprintf_4(("dbe_bkey_compress, pk:\n"));
        ss_output_4(dbe_bkey_dprint(4, pk));
        ss_dprintf_4(("dbe_bkey_compress, nk:\n"));
        ss_output_4(dbe_bkey_dprint(4, nk));
#endif /* TEST */

        ss_dassert_4(dbe_bkey_compare(pk, nk) < 0);

        if (tk != pk && tk != nk) {
            tmpk = tk;
        } else {
            buf = SsMemAlloc(ki->ki_maxkeylen);
            tmpk = (dbe_bkey_t*)buf;
        }

        /* copy header part directly */
        memcpy(tmpk, nk->k_.pval, BKEY_HEADERLEN(nk));

        index = (TWO_BYTE_T)vtpl_compress(
                                BKEY_GETVTPLPTR(tmpk),
                                BKEY_GETVTPLPTR(pk),
                                BKEY_GETVTPLPTR(nk));
        BKEY_STOREINDEX(tmpk, index);

        if (tmpk != tk) {
            dbe_bkey_copy(tk, tmpk);
            SsMemFree(buf);
        }

        ss_debug_4(dbe_bkey_test(tk));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_recompress_insert
 *
 * Recompresses a key value against a key value that is added before it.
 * The compressed next key value is compressed against the compressed
 * previous key. The compression result is the recompressed format of
 * next key. The compression index is added to the old compression index
 * of the next key.
 *
 * This compression is done only in case where the compresssion index of
 * the previous key is smaller than or equal to the next key. Otherwise
 * the next key is just copied into target key.
 *
 * This is symmetrical operation with dbe_bkey_reexpand_delete.
 *
 * Example:
 *
 *      Initial keys:
 *
 *          key 1i  aabb
 *          key 2i  aacd
 *
 *      Compressed index:
 *
 *          key 1c  0   aabb
 *          key 2c  3   cd
 *
 *      Insert key 3:
 *
 *          key 3i  aacc
 *
 *      Compress key 3:
 *
 *          key 3c  3 cc
 *
 *      Recompress key 2:
 *
 *          key 3c  3 cc
 *          key 2c  3 cd
 *          ------------
 *                  1  d
 *          =>
 *
 *          key 2c' 4  d
 *
 *      New compressed index:
 *
 *          key 1c  0 aabb
 *          key 3c  2 cc
 *          key 2c' 4 d
 *
 * Parameters :
 *
 *	tk - out, use
 *		target key
 *
 *	pk - in, use
 *		previous key, the newly inserted key in compressed format
 *
 *	nk - in, use
 *		next key that is recompressed into tk in compressed format
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_recompress_insert(dbe_bkey_t* tk, dbe_bkey_t* pk, dbe_bkey_t* nk)
{
        TWO_BYTE_T pk_index;
        TWO_BYTE_T nk_index;
        TWO_BYTE_T index;

        ss_dassert(tk != NULL);
        ss_dassert(pk != NULL);
        ss_dassert(nk != NULL);
        ss_dassert(tk != pk);
        ss_dassert(tk != nk);
        ss_dassert(dbe_bkey_checkheader(pk));
        ss_dassert(dbe_bkey_checkheader(nk));
        ss_dassert(BKEY_TYPEOK(pk));
        ss_dassert(BKEY_TYPEOK(nk));

        ss_debug_4(dbe_bkey_test(pk));
        ss_debug_4(dbe_bkey_test(nk));

#ifdef TEST
        ss_dprintf_4(("dbe_bkey_recompress_insert, pk:\n"));
        ss_output_4(dbe_bkey_dprint(4, pk));
        ss_dprintf_4(("dbe_bkey_recompress_insert, nk:\n"));
        ss_output_4(dbe_bkey_dprint(4, nk));
#endif /* TEST */

        pk_index = BKEY_LOADINDEX(pk);
        nk_index = BKEY_LOADINDEX(nk);

        if (pk_index <= nk_index) {

            /* copy header part directly */
            memcpy(tk, nk->k_.pval, BKEY_HEADERLEN(nk));

            index = (TWO_BYTE_T)vtpl_compress(
                                    BKEY_GETVTPLPTR(tk),
                                    BKEY_GETVTPLPTR(pk),
                                    BKEY_GETVTPLPTR(nk));
            index = index + nk_index;
            BKEY_STOREINDEX(tk, index);

        } else {

            dbe_bkey_copy(tk, nk);
        }


        ss_debug_4(dbe_bkey_test(tk));
}

/*#***********************************************************************\
 *
 *		dbe_bkey_buildtext
 *
 *
 *
 * Parameters :
 *
 *	buf -
 *
 *
 *	k -
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
static bool dbe_bkey_buildtext(char* buf, int buflen, dbe_bkey_t* k)
{
        uint info;
        char header[10];
        int i;
        vtpl_t* vtpl;
        TWO_BYTE_T two_byte;
        FOUR_BYTE_T four_byte;
        FOUR_BYTE_T four_byte2;
        char localbuf[80];
        char* bufend = buf + buflen - 1;

        if (k == NULL) {
            strcpy(buf, "NULL");
            return(TRUE);
        }

        buf[0] = '\0';

        info = BKEY_GETINFO(k);
        i = 0;
        header[i++] = (char)((info & BKEY_LEAF) ? 'L': 'I');
        header[i++] = (char)((info & BKEY_DELETEMARK) ? 'd': 'i');
        header[i++] = (char)((info & BKEY_COMMITTED) ? 'c': 'u');
        header[i++] = (char)((info & BKEY_CLUSTERING) ? 'c': 's');
        header[i++] = (char)('0' + (info & (BKEY_1LONGUSED|BKEY_2LONGUSED)));
        header[i++] = (char)((info & BKEY_UPDATE) ? 'u': 'i');
        header[i++] = '\0';

        vtpl = BKEY_GETVTPLPTR(k);

        two_byte = BKEY_LOADINDEX(k);
        SsSprintf(localbuf, "%s:%u", header, (uint)two_byte);
        strcpy(buf, localbuf);
        buf += strlen(localbuf);
        if (dbe_bkey_isleaf(k)) {
            if (BKEY_GETINFO(k) & BKEY_1LONGUSED) {
                four_byte = BKEY_LOADTRXNUM(k);
                SsSprintf(localbuf, "[%ld]", (long)four_byte);
            } else if (BKEY_GETINFO(k) & BKEY_2LONGUSED) {
                four_byte = BKEY_LOADTRXNUM(k);
                four_byte2 = BKEY_LOADTRXID(k);
                SsSprintf(localbuf, "[%ld,%ld]", (long)four_byte, (long)four_byte2);
#ifdef DBE_MERGEDEBUG
                if (info & BKEY_DELETEMARK) {
                    char* p;
                    long readlevel;
                    int trxmode;

                    strcpy(buf, localbuf);
                    buf += strlen(localbuf);

                    p = BKEY_GETMERGEDEBUGINFOPTR(k);
                    readlevel = SS_UINT4_LOADFROMDISK(p);
                    p += sizeof(ss_uint4_t);
                    trxmode = *p;
                    SsSprintf(localbuf, "[%ld,%d]", readlevel, trxmode);
                }
#endif /* DBE_MERGEDEBUG */
            } else {
                SsSprintf(localbuf, "[]");
            }
            strcpy(buf, localbuf);
            buf += strlen(localbuf);
            SsSprintf(localbuf, "%ld:", dbe_bkey_getkeyid(k));
            strcpy(buf, localbuf);
            buf += strlen(localbuf);
        } else {
            if (BKEY_GETINFO(k) & BKEY_1LONGUSED) {
                four_byte = BKEY_LOADADDR(k);
                SsSprintf(localbuf, "[%ld]", (long)four_byte);
            } else if (BKEY_GETINFO(k) & BKEY_2LONGUSED) {
                four_byte = BKEY_LOADADDR(k);
                four_byte2 = BKEY_LOADTRXID(k);
                SsSprintf(localbuf, "[%ld,%ld]", (long)four_byte, (long)four_byte2);
            } else {
                SsSprintf(localbuf, "[*** ERROR ***]");
            }
            strcpy(buf, localbuf);
            buf += strlen(localbuf);
        }
        vtpl_buildvtpltext(buf, (int)(bufend - buf), vtpl);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_print_ex
 *
 * Prints a key value to stdout in a readable format.
 *
 * Parameters :
 *
 *      fp - use
 *          File pointer or NULL.
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      TRUE always
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bkey_print_ex(void* fp, char* txt, dbe_bkey_t* k)
{
        char* buf;
        int len;
        char format[20];

        if (k == NULL) {
            SsFprintf(fp, "NULL");
            return(TRUE);
        }

        SsSprintf(format, "%%s%%.%ds\n", SS_MSGLOG_BUFSIZE-128);
        len = 80 + 3 * dbe_bkey_getlength(k) + strlen(txt);
        buf = SsMemAlloc(len);

        dbe_bkey_buildtext(buf, len, k);

        SsFprintf(fp, format, txt, buf);

        SsMemFree(buf);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_print
 *
 * Prints a key value to stdout in a readable format.
 *
 * Parameters :
 *
 *      fp - use
 *          File pointer or NULL.
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      TRUE always
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bkey_print(void* fp, dbe_bkey_t* k)
{
        return(dbe_bkey_print_ex(fp, "", k));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_printvtpl
 *
 * Prints a v-tuple to stdout in an (almost) readable format.
 *
 * Parameters :
 *
 *      fp - use
 *          File pointer or NULL.
 *
 *	vtpl - in, use
 *		v-tuple
 *
 * Return value :
 *
 *      TRUE always
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_bkey_printvtpl(void* fp, vtpl_t* vtpl)
{
        return(vtpl_printvtpl(fp, vtpl));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_dprint_ex
 *
 *
 *
 * Parameters :
 *
 *	level -
 *
 *
 *	k -
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
bool dbe_bkey_dprint_ex(int level, char* txt, dbe_bkey_t* k)
{
        char* buf;
        int len;
        char format[20];

        if (k == NULL) {
            strcpy(format, "%s%s\n");
            buf = SsMemStrdup((char *)"NULL");
        } else {
            SsSprintf(format, "%%s%%.%ds\n", SS_MSGLOG_BUFSIZE-128);
            len = 80 + 3 * dbe_bkey_getlength(k) + strlen(txt);
            buf = SsMemAlloc(len);
            dbe_bkey_buildtext(buf, len, k);
        }

        switch (level) {
            case 1:
                SsDbgPrintfFun1(format, txt, buf);
                break;
            case 2:
                SsDbgPrintfFun2(format, txt, buf);
                break;
            case 3:
                SsDbgPrintfFun3(format, txt, buf);
                break;
            case 4:
                SsDbgPrintfFun4(format, txt, buf);
                break;
            default:
                SsDbgPrintf(format, txt, buf);
                break;
        }

        SsMemFree(buf);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_dprint
 *
 *
 *
 * Parameters :
 *
 *	level -
 *
 *
 *	k -
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
bool dbe_bkey_dprint(int level, dbe_bkey_t* k)
{
        return(dbe_bkey_dprint_ex(level, "", k));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_dprintvtpl
 *
 *
 *
 * Parameters :
 *
 *	level -
 *
 *
 *	vtpl -
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
bool dbe_bkey_dprintvtpl(int level, vtpl_t* vtpl)
{
        return(vtpl_dprintvtpl(level, vtpl));
}

/*##**********************************************************************\
 *
 *		dbe_bkey_search_compress
 *
 * Compresses the search key after the search ends. This is done after
 * the correct place for a key insert is searched using the function
 * dbe_bkey_search_step and it has returned a value <= 0.
 *
 * Parameters :
 *
 *	ks - in, use
 *		pointer to a key search structure
 *
 *	tk - out, use
 *		target key buffer to where the search key is compressed
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_bkey_search_compress(dbe_bkey_search_t* ks, dbe_bkey_t* tk)
{
        TWO_BYTE_T index;

        ss_dassert(ks != NULL);
        ss_dassert(tk != NULL);
        ss_dassert(dbe_bkey_checkheader(tk));
        ss_dassert(BKEY_TYPEOK(tk));

        /* copy header part directly */
        memcpy(tk, ks->ks_k->k_.pval, BKEY_HEADERLEN(ks->ks_k));

        index = (TWO_BYTE_T)vtpl_search_compress(BKEY_GETVTPLPTR(tk), &ks->ks_ss);
        BKEY_STOREINDEX(tk, index);

        ss_dassert(dbe_bkey_checkheader(tk));
        ss_dassert(BKEY_TYPEOK(tk));
        ss_debug_4(dbe_bkey_test(tk));
}

/*#**********************************************************************\
 *
 *		bkey_findsplit
 *
 * Finds the split key value between full_key and compressed_key and
 * stores it into target_split_key. The split key value is the shortest
 * possible key value between full_key and compressed_key.
 *
 * Parameters :
 *
 *	target_split_key - out, use
 *		Target buffer into where the split key value is stored.
 *
 *	full_key - in, use
 *		Full key value.
 *
 *	compressed_key - in, use
 *		Compressed key value greater or equal to full_key.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void bkey_findsplit(
        dbe_bkey_t* target_split_key,
        dbe_bkey_t* full_key,
        dbe_bkey_t* compressed_key)
{
        TWO_BYTE_T index;

        ss_dassert(target_split_key != NULL);
        ss_dassert(full_key != NULL);
        ss_dassert(compressed_key != NULL);
        ss_dassert(dbe_bkey_checkheader(full_key));
        ss_dassert(dbe_bkey_checkheader(compressed_key));
        ss_dassert(dbe_bkey_checkheader(target_split_key));
        ss_dassert(BKEY_TYPEOK(full_key));
        ss_dassert(BKEY_TYPEOK(compressed_key));
        ss_dassert(BKEY_TYPEOK(target_split_key));
        ss_debug(index = BKEY_LOADINDEX(full_key));
        ss_dassert(index == 0);

        index = 0;
        BKEY_STOREINDEX(target_split_key, index);
        index = BKEY_LOADINDEX(compressed_key);
        vtpl_find_split(
            BKEY_GETVTPLPTR(target_split_key),
            BKEY_GETVTPLPTR(full_key),
            BKEY_GETVTPLPTR(compressed_key),
            (vtpl_index_t)index);

        ss_dassert(
            BKEY_HEADERLEN(target_split_key) +
                vtpl_grosslen(BKEY_GETVTPLPTR(target_split_key)) <
            BKEY_MAXLEN);
        ss_dassert(dbe_bkey_compare(full_key, target_split_key) <= 0);

        ss_dassert(dbe_bkey_checkheader(target_split_key));
}

/*#***********************************************************************\
 *
 *		bkey_initsplit_header
 *
 *
 *
 * Parameters :
 *
 *	sk - in, use
 *
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
#ifdef NEW_SPLITKEY
static dbe_bkey_t* bkey_initsplit_header(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* sk)
{
        dbe_bkey_t* k = NULL;

        CHK_BKEYINFO(ki);
        SS_MEMOBJ_INC(SS_MEMOBJ_BKEY, dbe_bkey_t);

        if (cd != NULL) {
            k = rs_sysi_getbkeybuf(cd);
        }
        if (k == NULL) {
            k = SsMemAlloc(ki->ki_maxkeylen);
        }

        if (dbe_bkey_isleaf(sk)) {
            switch (BKEY_GETINFO(sk) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) {
                case 0:
                    BKEY_INITINFO(k, BKEY_1LONGUSED);
                    break;
                case BKEY_1LONGUSED:
                    BKEY_INITINFO(k, BKEY_2LONGUSED);
                    BKEY_STORETRXID(k, dbe_trxid_null);
                    break;
                case BKEY_2LONGUSED:
                    BKEY_INITINFO(k, BKEY_2LONGUSED);
                    BKEY_STORETRXID(k, dbe_trxid_null);
                    break;
                default:
                    ss_error;
            }
        } else {
            switch (BKEY_GETINFO(sk) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) {
                case 0:
                    ss_error;
                case BKEY_1LONGUSED:
                    BKEY_INITINFO(k, BKEY_1LONGUSED);
                    break;
                case BKEY_2LONGUSED:
                    BKEY_INITINFO(k, BKEY_2LONGUSED);
                    BKEY_STORETRXID(k, dbe_trxid_null);
                    break;
                default:
                    ss_error;
            }
        }
        BKEY_STOREINDEX(k, 0);

        dbe_bkey_setdeletemark(k);

        return(k);
}
#else /* NEW_SPLITKEY */
static dbe_bkey_t* bkey_initsplit_header(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* sk)
{
        dbe_bkey_t* k = NULL;
        dbe_trxid_t tmp_trxid;
        dbe_trxnum_t tmp_trxnum;

        CHK_BKEYINFO(ki);
        SS_MEMOBJ_INC(SS_MEMOBJ_BKEY, dbe_bkey_t);

        if (cd != NULL) {
            k = rs_sysi_getbkeybuf(cd);
        }
        if (k == NULL) {
            k = SsMemAlloc(ki->ki_maxkeylen);
        }

        if (dbe_bkey_isleaf(sk)) {
            switch (BKEY_GETINFO(sk) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) {
                case 0:
                    BKEY_INITINFO(k, BKEY_1LONGUSED);
                    break;
                case BKEY_1LONGUSED:
                    BKEY_INITINFO(k, BKEY_2LONGUSED);
                    tmp_trxnum = dbe_bkey_gettrxnum(sk);
                    tmp_trxid = dbe_trxid_initfromtrxnum(tmp_trxnum);
                    BKEY_STORETRXID(k, tmp_trxid);
                    break;
                case BKEY_2LONGUSED:
                    BKEY_INITINFO(k, BKEY_2LONGUSED);
                    BKEY_STORETRXID(k, dbe_bkey_gettrxid(sk));
                    break;
                default:
                    ss_error;
            }
        } else {
            switch (BKEY_GETINFO(sk) & (BKEY_1LONGUSED|BKEY_2LONGUSED)) {
                case 0:
                    ss_error;
                case BKEY_1LONGUSED:
                    BKEY_INITINFO(k, BKEY_1LONGUSED);
                    break;
                case BKEY_2LONGUSED:
                    BKEY_INITINFO(k, BKEY_2LONGUSED);
                    BKEY_STORETRXID(k, dbe_bkey_gettrxid(sk));
                    break;
                default:
                    ss_error;
            }
        }
        BKEY_STOREINDEX(k, 0);

        if (dbe_bkey_isdeletemark(sk)) {
            dbe_bkey_setdeletemark(k);
        }

        return(k);
}
#endif /* NEW_SPLITKEY */

/*##**********************************************************************\
 *
 *		dbe_bkey_initsplit
 *
 *
 *
 * Parameters :
 *
 *	split_key - in, use
 *
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bkey_t* dbe_bkey_initsplit(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* split_key)
{
        dbe_bkey_t* k;

        CHK_BKEYINFO(ki);
        ss_dassert(dbe_bkey_checkheader(split_key));
        ss_dassert(BKEY_TYPEOK(split_key));

        k = bkey_initsplit_header(cd, ki, split_key);

        dbe_bkey_setvtpl(k, dbe_bkey_getvtpl(split_key));

        ss_dassert(dbe_bkey_checkheader(k));
        ss_dassert(BKEY_TYPEOK(k));

        return(k);
}

/*##**********************************************************************\
 *
 *		dbe_bkey_findsplit
 *
 *
 *
 * Parameters :
 *
 *	full_key - in, use
 *
 *
 *	compressed_key - in, use
 *
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_bkey_t* dbe_bkey_findsplit(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* full_key,
        dbe_bkey_t* compressed_key)
{
        dbe_bkey_t* k;

        CHK_BKEYINFO(ki);
        ss_dassert(dbe_bkey_checkheader(full_key));
        ss_dassert(BKEY_TYPEOK(full_key));
        ss_dassert(dbe_bkey_checkheader(compressed_key));
        ss_dassert(BKEY_TYPEOK(compressed_key));

        k = bkey_initsplit_header(cd, ki, compressed_key);

        bkey_findsplit(k, full_key, compressed_key);

        return(k);
}

/*##**********************************************************************\
 * 
 *		dbe_bkey_setbkeytotval
 * 
 * Sets tval fields directly from bkey.
 * 
 * Parameters : 
 * 
 *		cd - 
 *			
 *			
 *		key - 
 *			
 *			
 *		bkey - 
 *			
 *			
 *		ttype - 
 *			
 *			
 *		tval - 
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
dbe_ret_t dbe_bkey_setbkeytotval(
        rs_sysi_t* cd,
        rs_key_t* key,
        dbe_bkey_t* bkey,
        rs_ttype_t* ttype,
        rs_tval_t* tval)
{
        dbe_ret_t rc;
        rs_ano_t kpno;
        rs_ano_t nparts;
        vtpl_t* vtpl;
        va_t* va;
        bool* blobattrs = NULL;

        ss_dprintf_3(("dbe_bkey_setbkeytotval\n"));
        ss_dassert(rs_key_isprimary(cd, key));

        rc = DBE_RC_FOUND;
        ss_dassert(tval != NULL);

        vtpl = dbe_bkey_getvtpl(bkey);

        if (dbe_bkey_isblob(bkey)) {
            blobattrs = dbe_blobinfo_getattrs(
                            vtpl,
                            rs_ttype_nattrs(cd, ttype),
                            NULL);
        }
        if (dbe_bkey_isupdate(bkey)) {
            rs_tval_setrowflags(cd, ttype, tval, RS_AVAL_ROWFLAG_UPDATE);
        }

        nparts = rs_key_nparts(cd, key);

        va = VTPL_GETVA_AT0(vtpl);

        /* Create a tuple value that is returned.
         */
        for (kpno = 0; kpno < nparts; kpno++) {
            rs_ano_t ano;

            ano = rs_keyp_ano(cd, key, kpno);
            ss_dassert(ano != RS_ANO_PSEUDO);

            if (ano != RS_ANO_NULL) {
                if (blobattrs != NULL && blobattrs[kpno]) {
                    /* This is a blob attribute.
                     * We cannot test that va is a blob va, because key
                     * compression may remove special blob va length
                     * information
                     * ss_dassert(va_testblob(va));
                     */
                    dynva_t blobva = NULL;
                    char* data;
                    va_index_t datalen;

                    /* Create a blob va. */
                    data = va_getdata(va, &datalen);
                    dynva_setblobdata(&blobva, data, datalen, NULL, 0);
                    rs_tval_setva(cd, ttype, tval, ano, blobva);
                    dynva_free(&blobva);
                } else {
                    /* Set normal attribute. */
                    ss_dassert(!va_testblob(va));

                    rs_tval_setvaref_flat(cd, ttype, tval, ano, va);
                }
            }

            va = VTPL_SKIPVA(va);
        }

        if (blobattrs != NULL) {
            SsMemFree(blobattrs);
        }

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_dynbkey_setbkey
 *
 * Sets bkey to a dynbkey variable.
 *
 * Parameters :
 *
 *	dynbkey - use
 *		dynamic bkey variable
 *
 *	bkey - in
 *		bkey the value of which is set to dynbkey
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_dynbkey_t dbe_dynbkey_setbkey(
        dbe_dynbkey_t* dynbkey,
        dbe_bkey_t* bkey)
{
        ss_dassert(dbe_bkey_checkheader(bkey));
        ss_dassert(BKEY_TYPEOK(bkey));

        if (*dynbkey != NULL) {
            SS_MEMOBJ_DEC(SS_MEMOBJ_BKEY);
        }

        *dynbkey = SsMemRealloc(*dynbkey, dbe_bkey_getlength(bkey));
        dbe_bkey_copy(*dynbkey, bkey);

        SS_MEMOBJ_INC(SS_MEMOBJ_BKEY, dbe_bkey_t);

        ss_dassert(dbe_bkey_checkheader(*dynbkey));
        ss_dassert(BKEY_TYPEOK(*dynbkey));

        return(*dynbkey);
}

/*##**********************************************************************\
 *
 *		dbe_dynbkey_setleaf
 *
 * Sets a leaf key value with initial values to a dynbkey variable.
 *
 * Parameters :
 *
 *      dynbkey - use
 *          dynamic bkey variable
 *
 *      trxnum - in
 *		transaction number of the key
 *
 *      trxid - in
 *		transaction id of the key
 *
 *	vtpl - in, use
 *		initial key content
 *
 * Return value - give :
 *
 *      pointer to a leaf key value
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_dynbkey_t dbe_dynbkey_setleaf(
        dbe_dynbkey_t* dynbkey,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        vtpl_t* vtpl)
{
        if (*dynbkey != NULL) {
            SS_MEMOBJ_DEC(SS_MEMOBJ_BKEY);
        }

        *dynbkey = SsMemRealloc(*dynbkey, BKEY_MAXHEADERLEN + vtpl_grosslen(vtpl));
        bkey_initleafbuf(*dynbkey, trxnum, trxid, vtpl);

        SS_MEMOBJ_INC(SS_MEMOBJ_BKEY, dbe_bkey_t);

        ss_dassert(dbe_bkey_checkheader(*dynbkey));
        ss_dassert(BKEY_TYPEOK(*dynbkey));

        return(*dynbkey);
}

/*##**********************************************************************\
 *
 *		dbe_dynbkey_expand
 *
 * Expands the key value in ck to dynamic bkey variable.
 *
 * Parameters :
 *
 *	dyntk - use
 *		dynamic bkey
 *
 *	fk - in
 *		full key preceding ck.
 *
 *	ck - in
 *		compressed key that is expanded into dyntk
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_dynbkey_expand(
        dbe_dynbkey_t* dyntk,
        dbe_bkey_t* fk,
        dbe_bkey_t* ck)
{
        TWO_BYTE_T index;
        size_t allocsize;
        dbe_dynbkey_t new_dyntk;

        ss_dassert(dbe_bkey_test(fk));
        ss_dassert(dbe_bkey_test(ck));

        index = BKEY_LOADINDEX(ck);

        /* Fixed memory allocation bug. If ck was expanded from
         * a short v-tuple to a long one, allocation was four bytes
         * too small. Added VA_LENGTHMAXLEN to fix the problem and
         * changed grosslen to netlen.
         *   JarmoR Mar 25, 1998
         */
        allocsize = BKEY_MAXHEADERLEN +
                    2 * index +
                    VA_LENGTHMAXLEN +
                    vtpl_netlen(BKEY_GETVTPLPTR(ck));

        new_dyntk = SsMemAlloc(allocsize);

        SS_MEMOBJ_INC(SS_MEMOBJ_BKEY, dbe_bkey_t);

        dbe_bkey_expand(new_dyntk, fk, ck);

#ifdef SS_DEBUG
        if (dbe_bkey_getlength(new_dyntk) > allocsize) {
            /* This test failed sometimes with snc/test/snctst. Fixed
             * by adding VA_LENGTHMAXLEN to allocsize calculation.
             *   JarmoR Mar 25, 1998
             */
            char debugstring[48];
            SsSprintf(
                debugstring,
                "/LOG/NOD/UNL/TID:%u/FLU",
                SsThrGetid());
            SsDbgSet(debugstring);
            SsDbgFlush();
            SsDbgPrintf("dbe_dynbkey_expand\n");
            SsDbgPrintf("dbe_bkey_getlength(new_dyntk)=%d\n", dbe_bkey_getlength(new_dyntk));
            SsDbgPrintf("allocsize=%d\n", allocsize);
            SsDbgPrintf("BKEY_MAXHEADERLEN=%d\n", BKEY_MAXHEADERLEN);
            SsDbgPrintf("index=%d\n", index);
            SsDbgPrintf("vtpl_grosslen(BKEY_GETVTPLPTR(new_dyntk))=%d\n", vtpl_grosslen(BKEY_GETVTPLPTR(new_dyntk)));
            SsDbgPrintf("vtpl_grosslen(BKEY_GETVTPLPTR(ck))=%d\n", vtpl_grosslen(BKEY_GETVTPLPTR(ck)));
            SsDbgPrintf("vtpl_grosslen(BKEY_GETVTPLPTR(fk))=%d\n", vtpl_grosslen(BKEY_GETVTPLPTR(fk)));
            SsDbgFlush();
            SsDbgPrintf("new_dyntk:\n");
            dbe_bkey_dprint(0, new_dyntk);
            SsDbgPrintf("ck:\n");
            dbe_bkey_dprint(0, ck);
            SsDbgPrintf("fk:\n");
            dbe_bkey_dprint(0, fk);
            SsDbgFlush();
            ss_error;
        }
#endif /* SS_DEBUG */

        ss_dassert(dbe_bkey_test(fk));
        ss_dassert(dbe_bkey_test(ck));
        ss_dassert(dbe_bkey_test(new_dyntk));

        if (*dyntk != NULL) {
            SS_MEMOBJ_DEC(SS_MEMOBJ_BKEY);
            SsMemFree(*dyntk);
        }
        *dyntk = new_dyntk;

        ss_dassert(dbe_bkey_test(*dyntk));
}

/*##**********************************************************************\
 *
 *		dbe_dynbkey_free
 *
 * Releases dynbkey variable. The dynbkey variable is set to NULL.
 *
 * Parameters :
 *
 *	dynbkey - use
 *		dynamic bkey
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_dynbkey_free(
        dbe_dynbkey_t* dynbkey)
{
        if (*dynbkey != NULL) {
            ss_dassert(dbe_bkey_checkheader(*dynbkey));
            ss_dassert(BKEY_TYPEOK(*dynbkey));
            SS_MEMOBJ_DEC(SS_MEMOBJ_BKEY);
            SsMemFree(*dynbkey);
            *dynbkey = NULL;
        }
}

