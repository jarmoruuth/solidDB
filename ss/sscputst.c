/*************************************************************************\
**  source       * sscputst.c
**  directory    * ss
**  description  * Test if the compiler options match this CPU
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


#include "ssenv.h"

#include "ssstdio.h"
#include "ssstring.h"
#include "ssc.h"
#include "ssdebug.h"
#include "sslimits.h"
#include "ssfloat.h"

/*#***********************************************************************\
 * 
 *		get_byteorder
 * 
 * Returns a 4-byte double word value indicating double word byte order.
 * 
 * Parameters : 	 - none
 * 
 * Return value :
 *      0x00010203 when LSB 1ST a'la Intel x86 & pentium (SS_LSB1ST should be #defined)
 *      0x03020100 when MSB 1ST a'la Motorola 680x0 or
 *      some other value when the byte order is weird
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static ss_uint4_t get_byteorder(void)
{
        ss_uint4_t ul = 0x03020100L;
        uchar* p = (uchar*)&ul;
#if 0
        ss_uint4_t ul2  = ((((((ss_uint4_t)p[0] << CHAR_BIT) | p[1])<< CHAR_BIT) | p[2]) << CHAR_BIT) | p[3] ;
        printf("0x%04lX, 0x%04lX, %d, %d\n", ul, ul2, CHAR_BIT, sizeof(ulong));
#endif
        return (((((((ss_uint4_t)p[0] << CHAR_BIT) | p[1])
                << CHAR_BIT) | p[2]) << CHAR_BIT) | p[3]);
        
}

/*#***********************************************************************\
 * 
 *		test_byteorder
 * 
 * Checks that the byte order is as expected
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void test_byteorder(void)
{
        ss_uint4_t t;
        ss_uint4_t expected_value; 

        t = get_byteorder();
#if defined(SS_LSB1ST) || defined(SS_CROSSENDIAN)
        expected_value = 0x00010203L;
#else  /* MSB 1ST */
        expected_value = 0x03020100L;
#endif
        if (t != expected_value) {
            SsPrintf(
                "expected byte order 0x%08lX but test gives 0x%08lX\n",
                expected_value, t);
            ss_error; 
        }
}

/*#***********************************************************************\
 * 
 *		test_unaligned_load
 * 
 * If the UNALIGNED_LOAD is #define'd this checks that it also is possible.
 * when the #define UNALIGNED_LOAD is defined and the CPU architecture
 * does not support it this function causes a trap! When the UNALIGNED_LOAD
 * is not defined this function checks that the #define SS_ALIGNMENT
 * is at least big enough. Too big values are not detected
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void test_unaligned_load(void)
{
#ifdef UNALIGNED_LOAD
        uchar buf[sizeof(ulong)*2 - 1];
        uint i;

        for (i = 0; i < sizeof(ulong); i++) {
            *(ulong*)(buf + i) = 1234567890L;
        }
#else
        union {
            uchar bytebuf[sizeof(double) + SS_ALIGNMENT];
            double dblbuf[2];
        } b;
        uchar* p = (uchar*)&b.dblbuf[0];

        *(double*)p = 0.0;  /* Always OK */
        p += SS_ALIGNMENT;
        *(double*)p = 0.0;  /* OK if SS_ALIGNMENT is big enough */
#endif
}

/*#***********************************************************************\
 * 
 *		test_intsize
 * 
 * Test that size & sign of integer types are as expected
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void test_intsize(void)
{
        ss_uint1_t ui1;
        ss_int1_t i1;
        ss_uint2_t ui2;
        ss_int2_t i2;
        ss_uint4_t ui4;
        ss_int4_t i4;
        size_t s;

        ss_assert(sizeof(ui1) == 1);
        ss_assert(sizeof(i1) == 1);
        ss_assert(sizeof(ui2) == 2);
        ss_assert(sizeof(i2) == 2);
        ss_assert(sizeof(ui4) == 4);
        ss_assert(sizeof(i4) == 4);

        ss_assert(~0 == -1);

        ui1 = (ss_uint1_t)~0;
        i1 = (ss_int1_t)~0;
        ss_assert(ui1 > 0);
        ss_assert(i1 < 0);

        ui2 = (ss_uint2_t)~0;
        i2 = (ss_int2_t)~0;
        ss_assert(ui2 > 0);
        ss_assert(i2 < 0);

        ui4 = (ss_uint4_t)~0L;
        i4 = (ss_int4_t)~0L;
        ss_assert(ui4 > 0);
        ss_assert(i4 < 0);

        s = 0;
        s--;
        ss_assert(s > 0);

        if (sizeof(int) * CHAR_BIT != INT_BIT) {
            ss_error;
        }
        /* check that ss_size_t and ss_ssize_t have correct sizes */
        ss_ct_assert(sizeof(ss_ssize_t) == sizeof(void*));
        ss_ct_assert(sizeof(ss_ssize_t) == sizeof(ss_size_t));
        /* chack that ss_ssize_t really is signed */
        ss_ct_assert((ss_ssize_t)((ss_ssize_t)1 <<
                                  (sizeof(ss_ssize_t) * SS_CHAR_BIT - 1)) < 0);
        /* chack that ss_size_t really is unsigned */
        ss_ct_assert((ss_size_t)((ss_size_t)1 <<
                              (sizeof(ss_size_t) * SS_CHAR_BIT - 1)) > 0);
}

/*#***********************************************************************\
 * 
 *		test_floatingpoint
 * 
 * Tests that the floating-point system is as expected.
 * Not currently implemented - is it needed???
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void test_floatingpoint(void)
{
}

/*#***********************************************************************\
 * 
 *		test_memcmp
 * 
 * Tests that our SsMemcmp() does not use C library memcmp()
 * where it makes signed comparison and that it returns 0
 * for zero length blocks.
 * The usage of C library memcmp() can be prevented by #defining
 * SS_MEMCMP_SIGNED to "sstring.h"
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void test_memcmp(void)
{
        ss_assert(SsMemcmp("\x80", "\x7F", 1) > 0);
        ss_assert(SsMemcmp("a", "b", 0) == 0);
}

static void test_stackdir1(char* addr1)
{
        char buf[100];

        memset(buf, '1', sizeof(buf)); /* dummy to disable optimization */
#if SS_STACK_GROW_DIRECTION < 0
        if (addr1 <= buf) {
            ss_debug(SsPrintf("addr1 = %08lX buf = %08lX\n", addr1, buf));
            ss_error;
        }
#else
        if (addr1 >= buf) {
            ss_debug(SsPrintf("addr1 = %08lX buf = %08lX\n", addr1, buf));
            ss_error;
        }
#endif
        memcpy(addr1, buf, sizeof(buf)); /* dummy to disable optimization */
}

/* dummy function pointer to disable function inlining */
static void (*test_stackdir1ptr)(char* addr1) = test_stackdir1;

static void test_stackdir(void)
{
        char buf[100];
        char dummybuf[100];
        
        memset(buf, '2', sizeof(buf));
        memset(dummybuf, '5', sizeof(dummybuf));
        (*test_stackdir1ptr)(buf);
        SsMemcmp(buf, dummybuf, SS_MIN(sizeof(buf), sizeof(dummybuf)));
}

#ifdef NO_ANSI_FLOAT
static void test_floatparams1(double d1, double d2)
{
        float f1 = (float)d1;
        float f2 = (float)d2;
        float f = (float)1.0;

        ss_assert(f1 == f);
        f *= 2;
        ss_assert(f2 == f);
}
#else
static void test_floatparams1(float f1, float f2)
{
        float f = (float)1.0;

        ss_assert(f1 == f);
        f *= 2;
        ss_assert(f2 == f);
}
#endif

/*#***********************************************************************\
 * 
 *		test_floatparams
 * 
 * If the test fails and NO_ANSI_FLOAT is not #defined
 * you should consider #defining NO_ANSI_FLOAT
 * in "ssfloat.h" in this environment. If the test fails
 * and NO_ANSI_FLOAT is #defined I really don't know ...
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void test_floatparams(void)
{
        float f1 = 1.0;
        float f2 = f1 * 2;
        test_floatparams1(f1, f2);
}

/*#***********************************************************************\
 * 
 *		test_limits
 * 
 * Tests that int2 and int4 limits are correctly set
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void test_limits(void)
{
        ss_assert(SS_INT2_MAX == 0x00007fffL);
        ss_assert(SS_INT2_MIN == ((ss_int2_t)0x8000L) && SS_INT2_MIN < 0);
        ss_assert(SS_UINT2_MAX == 0x0000ffffL && SS_UINT2_MAX > SS_INT2_MAX);
        ss_assert(SS_UINT2_MIN == 0);
        ss_assert(SS_INT4_MAX == 0x7fffffffL);
        ss_assert(SS_INT4_MIN == ((ss_int4_t)0x80000000L) && SS_INT4_MIN < 0);
        ss_assert(SS_UINT4_MAX == ((ss_uint4_t)0xffffffffL) 
                  && SS_UINT4_MAX > SS_INT4_MAX);
        ss_assert(SS_UINT4_MIN == 0);
}

/*##**********************************************************************\
 * 
 *		SsCPUTest
 * 
 * Test whether compilation options for CPU are correct. Errors cause
 * the program to abort - either through assertion failure or trap
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsCPUTest(void)
{
        test_byteorder();
        test_unaligned_load();
        test_intsize();
        test_floatingpoint();
        test_memcmp();
        test_stackdir();
        test_floatparams();
        test_limits();
}
