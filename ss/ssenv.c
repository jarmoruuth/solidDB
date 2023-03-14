/*************************************************************************\
**  source       * ssenv.c
**  directory    * ss
**  description  * environment description functions
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

#ifdef SS_UNIX
#include <unistd.h>
#include <errno.h>
#endif

#include "sswindow.h"
#include "sssprint.h"
#include "ssstring.h"
#include "ssdebug.h"
#include "sschcvt.h"

#include "ssenv.h"

#ifdef SS_NT

static SsOsIdT ssenv_getw32version(int* p_major, int* p_minor)
{
        BOOL b;
        OSVERSIONINFO osinfo;

        osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        b = GetVersionEx(&osinfo);
        ss_dassert(b);
        ss_dprintf_1(("maj:%d, min:%d, build:%d, id=%d, CSD=%s.\n",
                      (int)osinfo.dwMajorVersion,
                      (int)osinfo.dwMinorVersion,
                      (int)osinfo.dwBuildNumber,
                      (int)osinfo.dwPlatformId,
                      osinfo.szCSDVersion));

        if (p_major != NULL) {
            *p_major = (int)osinfo.dwMajorVersion;
        }
        if (p_minor != NULL) {
            *p_minor = (int)osinfo.dwMinorVersion;
        }

        switch (osinfo.dwPlatformId) {

            case VER_PLATFORM_WIN32s: /* 0 */
                ss_error;
                return(SS_OS_NULL); /* Dummy return to keep compiler happy */

            case 1: /* VER_PLATFORM_WIN32_WINDOWS ==  1 ?  */
                /* Here we could detect W98/W95 by checking maj.min.
                   W95 seems to have 4.0, W98 4.10 */
                return(SS_OS_W95);

            case VER_PLATFORM_WIN32_NT: /* 2 */
                return(SS_OS_WNT);

            default:
                ss_rc_error(osinfo.dwPlatformId);
                return(SS_OS_NULL); /* Dummy return to keep compiler happy */
        }       
}


/*##**********************************************************************\
 * 
 *		SsEnvOs
 * 
 * Returns the OS-identifier of current W32 operating system
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 *      One of SS_OS_WNT or SS_OS_W95 enum constants
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsOsIdT SsEnvOs(void)
{
        return(ssenv_getw32version(NULL, NULL));
}

int SsEnvOsVersion(void)
{
        SsOsIdT os;
        int maj=0, min=0;
        os = ssenv_getw32version(&maj, &min);
        if (os == SS_OS_WNT) {
            /* Windows NT */
            return(maj);
        } else {
            /* Windows 95/98/.. */
            if (maj == 4) {
                /* this returns 0 for w95, 10 for w98, ..? */
                return(min);
            } else {
                /* i don't know what this is called, but maj > 4 anyway */
                return(maj);
            }
        }
}

#define SS_OSVERSFULL_DEFINED
bool SsEnvOsversFull(int* p_major, int* p_minor)
{
        ssenv_getw32version(p_major, p_minor);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		SsEnvId
 * 
 */
SsEnvIdT SsEnvId(void)
{
        int maj;
        int min;
        bool b;
       
        switch (SsEnvOs()) {
            case SS_OS_WNT:
                return(SS_ENVID_NTI);
            case SS_OS_W95:
                b = SsEnvOsversFull(&maj, &min);
                ss_dassert(b);
                if (min == 0) {
                    return(SS_ENVID_W95);
                } else {
                    return(SS_ENVID_W98);
                }
            default:
                ss_error;
                return(SS_ENVID_DOS); /* Dummy return to keep compiler happy */
        }
}

#endif /* SS_NT */

#if defined(SS_UNIX) && 0

#include <errno.h>
#include <sys/utsname.h>

#define SS_OSVERSFULL_DEFINED
bool SsEnvOsversFull(int* p_major, int* p_minor)
{
        struct utsname un;
        int rc;

        rc = uname(&un);
        if (rc < 0) {
            ss_rc_derror(errno);
        }
        ss_dprintf_1(("sysname:%s\nnodename:%s\nrelease:%s\nversion:%s\nmachine:%s\n",
                       un.sysname,un.nodename,un.release,un.version,un.machine));

        return(FALSE);
}

#endif

#if defined(SS_LINUX)

#define SS_OSVERSFULL_DEFINED

#define SS_OSRELEASE_PATH "/proc/sys/kernel/osrelease"

#include "ssfile.h"
#include "sswfile.h"
#include "ssscan.h"
bool SsEnvOsversFull(
        int* p_major __attribute__ ((unused)),
        int* p_minor __attribute__ ((unused))) {
        return(TRUE);
}
bool SsEnvOsversFullWPL(int* p_major, int* p_minor, int* p_pl)
{
        char buf[50];
        char* dot;
        char* tmp;
        SS_FILE* fp;
        char* mismatch;
        fp = SsFOpenB((char *)SS_OSRELEASE_PATH, (char *)"r");
        if (fp) {
            SsFGets(buf, 50, fp);
            dot = strstr(buf, ".");
            if (!dot) {
                SsFClose(fp);
                return(FALSE);
            }
            tmp = dot + 1;
            SsStrScanInt(buf, p_major, &mismatch);
            SsStrScanInt(tmp, p_minor, &mismatch);
            if (mismatch) {
                SsStrScanInt(mismatch+1, p_pl, &mismatch);
            } else {
                *p_pl = 0;
            }

            SsFClose(fp);

            return(TRUE);
        }
        
        return(FALSE);
}

#else
bool SsEnvOsversFullWPL(int* p_major, int* p_minor, int* pl){
        return(TRUE);
}
#endif /* SS_LINUX */



static char* env_name(
        int cpu,
        int os,
        int osvers,
        char* outbuf,
        int bufsize,
        bool licp)
{

        char buf[64];

        ss_dassert(outbuf != NULL);
        ss_dassert(bufsize > 0);
        switch (os) {
            case SS_OS_DOS:
            case SS_OS_WIN:
                strcpy(buf, SsEnvOsName(os));
                break;

            case SS_OS_W95:
                if (licp) {
                    strcpy(buf, "Windows 95/98");
                } else {
                    /* Make decision by os minor version. 0 == 95, 10 == 98 */
                    int maj = 0, min = 0;
                    if (SsEnvOsversFull(&maj, &min)) {
                        if (min == 0) {
                            strcpy(buf, "Windows 95");
                        } else {
                            strcpy(buf, "Windows 98");
                        }
                    } else {
                        strcpy(buf, "Windows 95/98");
                    }
                }
                break;

            case SS_OS_WNT:
                if (licp) {
                    /* SsSprintf(buf, "%s %s", SsEnvOsName(os), SsEnvCpuName(cpu)); */
                    SsSprintf(buf, "Windows NT/Windows 2000 %s", SsEnvOsName(os), SsEnvCpuName(cpu));
                } else {
                    int maj = 0, min = 0;
                    if (SsEnvOsversFull(&maj, &min)) {
                        if ((maj == 5) && (min == 0)) {
                            SsSprintf(buf, "Windows 2000 %s", SsEnvCpuName(cpu));
                        } else if ((maj == 5) && (min == 1)) {
                            SsSprintf(buf, "Windows XP");
                        } else if ((maj == 5) && (min == 2)) {
                            SsSprintf(buf, "Windows Server 2003");
                        } else if ((maj == 6) && (min == 0)) {
                            SsSprintf(buf, "Windows Vista");
		            } else {
                            SsSprintf(buf, "%s %d.%d %s", SsEnvOsName(os), maj, min, SsEnvCpuName(cpu));
                        }
                    } else {
                        SsSprintf(buf, "%s %d.x %s", SsEnvOsName(os), maj, SsEnvCpuName(cpu));
                    }
                }
                break;

            case SS_OS_SOLARIS:
                if (osvers == 28) {
                    SsSprintf(buf, "%s 8 %s", SsEnvOsName(os), SsEnvCpuName(cpu));
                } else if (osvers == 29) {
                    SsSprintf(buf, "%s 9 %s", SsEnvOsName(os), SsEnvCpuName(cpu));
                } else if (osvers == 210) {
                    SsSprintf(buf, "%s 10 %s", SsEnvOsName(os), SsEnvCpuName(cpu));
                } else {
                    SsSprintf(buf, "%s %s", SsEnvOsName(os), SsEnvCpuName(cpu));
                }
                break;

            case SS_OS_LINUX:
#ifdef SS_OSVERSFULL_DEFINED
            {
                int maj, min, pl;
                if (SsEnvOsversFullWPL(&maj, &min, &pl)) {
                    SsSprintf(buf, "%s %d.%d.%d %s", SsEnvOsName(os), maj, min,pl,  SsEnvCpuName(cpu));
                } else {
                    SsSprintf(buf, "%s %s", SsEnvOsName(os), SsEnvCpuName(cpu));
                }
            }
#else
                SsSprintf(buf, "%s %s", SsEnvOsName(os), SsEnvCpuName(cpu));
#endif
                break;

            case SS_OS_FREEBSD:
                if (osvers != 0) {
                    SsSprintf(buf, "%s %d.x %s", SsEnvOsName(os), osvers, SsEnvCpuName(cpu));
                } else {
                    strcpy(buf, SsEnvOsName(os));
                }
                break;

            default:
                strcpy(buf, "Unknown environment");
                break;
        }
#ifdef SS_ENV_64BIT
        if (!licp) {
            strcat(buf, " 64bit");
        }
#endif

#ifdef SS_MT
        if (!licp) {
            strcat(buf, " MT");
        }
#endif
        strncpy(outbuf, buf, bufsize);
        outbuf[bufsize - 1] = '\0';
        return (outbuf);
}

/*##**********************************************************************\
 * 
 *		SsEnvNameCurr
 * 
 * Returns name of current environment (defined at compile time)
 * 
 * Parameters : 	 - none
 * 
 * Return value - ref:
 *      pointer to static buffer containing the environment name
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* SsEnvNameCurr(void)
{
        static char buf[128];

        if (buf[0] == '\0') {
            env_name(
                SS_ENV_CPU,
                SS_ENV_OS,
                SS_ENV_OSVERS,
                buf,
                sizeof(buf),
                FALSE);
        }
        return (buf);
}

/*##**********************************************************************\
 * 
 *		SsEnvName
 * 
 * Returns envrionment name as a string
 * 
 * Parameters : 
 * 
 *	cpu - in
 *		cpu type SS_CPU_XX
 *		
 *	os - in
 *		os type SS_OS_XX
 *		
 *	osvers - in
 *		os major version number
 *          (in win98 the minor version)
 *
 *      outbuf - out
 *          buffer for output
 *
 *      bufsize - in
 *          size of outbuf
 *
 * Return value - ref:
 *      outbuf
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* SsEnvName(int cpu, int os, int osvers, char* outbuf, int bufsize)
{
        return(env_name(cpu, os, osvers, outbuf, bufsize, FALSE));
}

/*##**********************************************************************\
 * 
 *		SsEnvLicenseName
 * 
 * Returns the OS name in the "license form".  This text is displayed in 
 * license text (... license for ..)
 * 
 */
char* SsEnvLicenseName(int cpu, int os, int osvers, char* outbuf, int bufsize)
{
        return(env_name(cpu, os, osvers, outbuf, bufsize, TRUE));
}

/*##**********************************************************************\
 * 
 *		SsEnvCpuName
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cpu - 
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
const char* SsEnvCpuName(int cpu)
{
        switch (cpu) {
            case SS_CPU_IX86:
                return ("ix86");
            case SS_CPU_POWERPC:
                return ("PowerPC");
            case SS_CPU_SPARC:
                return ("SPARC");
            case SS_CPU_IA64:
                 return ("IA64");
            case SS_CPU_AMD64:
                 return ("AMD64");
	    case SS_CPU_SPARC64:
                return ("SPARC");
            case SS_CPU_IA64_32BIT:
                return ("IA64");
            default:
                return ("Unknown");
        }
}

/*##**********************************************************************\
 * 
 *		SsEnvOsName
 * 
 * 
 * 
 * Parameters : 
 * 
 *	os - 
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
const char* SsEnvOsName(int os)
{
        switch (os) {
            case SS_OS_DOS:
                return ("MS-DOS");
            case SS_OS_WIN:
                return ("Windows");
            case SS_OS_W95:
                return ("Windows 95/98");
            case SS_OS_WNT:
                return ("Windows NT");
            case SS_OS_LINUX:
                return ("Linux");
            case SS_OS_SOLARIS:
                return ("Solaris");
            case SS_OS_FREEBSD:
                return ("FreeBSD");
            default:
                return ("Unknown");
        }
}

/*##**********************************************************************\
 * 
 *		SsEnvOsToken
 * 
 * Returns the current environment name token as a string
 * 
 * Parameters : 
 * 
 *      outbuf - out
 *          buffer for output
 *
 * Return value - ref:
 *      outbuf
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#if 0
/* Ari removed Aug 25, 1998
* You should use SsEnvTokenCurr() instead
*/

char* SsEnvOsToken(char *outbuf)
{
        return(strcpy(outbuf, SsEnvTokenCurr());
}
#endif

typedef struct SsIdToTokenSt {
        SsEnvIdT it_id;
        const char*    it_token;
} SsIdToTokenT;

/* These values are based on the "final draft" from Product Management,
 * Jan 03, 1997.  Ari
 */
static SsIdToTokenT id2token [] = {
        {SS_ENVID_DOS, "dos"},      
        {SS_ENVID_D4G, "d4g"},      
        {SS_ENVID_W16, "w16"},
        {SS_ENVID_W95, "w32"}, /* xxx not unique */
        {SS_ENVID_W98, "w32"}, /* xxx not unique */
        {SS_ENVID_NTI, "w32"}, /* xxx not unique */
        {SS_ENVID_NTA, "nta"},
        {SS_ENVID_O16, "o16"},
        {SS_ENVID_O32, "o32"},
        {SS_ENVID_A3X, "a3x"},
        {SS_ENVID_A4X, "a4x"},
        {SS_ENVID_A4X64BIT, "a4x64"},
        {SS_ENVID_H9X, "h9x"},
        {SS_ENVID_H0X, "h0x"},
        {SS_ENVID_H1X, "h1x"},
        {SS_ENVID_H1X64BIT, "h1x64"},
        {SS_ENVID_HIA, "hia"},
        {SS_ENVID_HIA64BIT, "hia64"},
        {SS_ENVID_SCX, "scx"},
        {SS_ENVID_OVV, "ovv"},
        {SS_ENVID_OVA, "ova"},
        {SS_ENVID_SSX, "ssx"},
        {SS_ENVID_S8X, "s8x"},
        {SS_ENVID_S9X, "s9x"},
        {SS_ENVID_S8X64BIT, "s8x64"},
        {SS_ENVID_S0X, "s0x"},
        {SS_ENVID_S0X64BIT, "s0x64"},
        {SS_ENVID_S0XI, "s0xi"},
        {SS_ENVID_S0XI64BIT, "s0xi64"},
        {SS_ENVID_IRX, "irx"},
        {SS_ENVID_LUX, "lux"},
        {SS_ENVID_LXA, "lxa"},
        {SS_ENVID_L2X, "l2x"},
        {SS_ENVID_L2X64, "l2x64"},
        {SS_ENVID_CLX, "clx"},
        {SS_ENVID_DIX, "dix"},
        {SS_ENVID_VSSX, "vssx"},
        {SS_ENVID_FREEBSD, "fbx"},
        {SS_ENVID_FEX, "fex"},
        {SS_ENVID_FEX64, "fex64"},
        {SS_ENVID_PSP, "psp"},
        {SS_ENVID_BSI, "bsi"},
        {SS_ENVID_LPX, "lpx"},
	{SS_ENVID_OSA, "osa"},
	{SS_ENVID_LXSB, "lxsb"},
        {SS_ENVID_S9X64BIT, "s9x64"},
        {SS_ENVID_A5X, "a5x"},
	{SS_ENVID_QPX, "qpx"},
        {SS_ENVID_WNT64, "w64"},
        {-1, NULL}
};


/*##**********************************************************************\
 * 
 *		SsEnvIdToken
 * 
 * This function returns the 3-letter abbreviation of the given env id.
 * The returned name should be used for product Dll and library names etc.
 * 
 * Parameters : 
 * 
 *	envid - 
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
char* SsEnvTokenById(SsEnvIdT envid)
{
        int i;
        for (i=0; (int)id2token[i].it_id != -1; i++) {

            if (id2token[i].it_id == envid) {
                return((char *)id2token[i].it_token);
            }
        }
        ss_rc_error(envid);
        return((char *)"");
}

/*##**********************************************************************\
 * 
 *		SsEnvIdByToken
 * 
 * Return Env Id of given token.
 * WARNING:  This is not exact in "w32", because there are more
 *           than one correct answers.
 *           w32 --> W95/W98/NTI
 *
 * Parameters : 
 * 
 *	token - 
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
SsEnvIdT SsEnvIdByToken(const char* token)
{
        int i;
        for (i=0; id2token[i].it_token != NULL; i++) {

            if (SsStricmp(id2token[i].it_token, token) == 0) {
                return(id2token[i].it_id);
            }
        }
        SsPrintf("%s.\n", token != NULL ? token : "");
        ss_error;
        return((SsEnvIdT)-1);
}

/*##**********************************************************************\
 * 
 *		SsEnvTokenCurr
 * 
 * 
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
char* SsEnvTokenCurr(void)
{
        return(SsEnvTokenById(SS_ENV_ID));
}

#if !defined(SS_OSVERSFULL_DEFINED)

/*##**********************************************************************\
 * 
 *		SsEnvOsversFull
 * 
 * Default osvers function, returns only major version (approx)
 * Possible env specific implementions are earlier in this file
 * 
 * Parameters : 
 * 
 *	p_major - 
 *		
 *		
 *	p_minor - 
 *		
 *		
 * Return value : 
 *	TRUE, if both components available	
 *      always FALSE, because only major is returned
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool SsEnvOsversFull(int* p_major, int* p_minor)
{
        *p_major = SS_ENV_OSVERS;
        *p_minor = 0;
        return(FALSE);
}

#endif /* SS_OSVERSFULL_DEFINED */

#include "ssmem.h"
#include "ssgetenv.h"

/*##**********************************************************************\
 *
 *      SsHostname
 *
 * Gives the current host's name.
 *
 * Parameters:
 *
 * Return value - give:
 *      hostname
 *
 * Limitations:
 *
 * Globals used:
 */
char*    SsHostname(void)
{
#if defined(SS_UNIX)
        char* hostname;
        int rc;

        hostname = SsMemAlloc(256);
        rc = gethostname(hostname, 256);
        ss_rc_assert(rc == 0, errno);
        hostname[255] = '\0';

        return hostname;
#elif defined(SS_NT)
        return SsMemStrdup(SsGetEnv("COMPUTERNAME"));
#else
        ss_info_assert(FALSE, ("SsHostname not ported to this platform"));

        return SsMemStrdup("UNDEFINED");
#endif
}
