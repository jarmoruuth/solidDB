/*************************************************************************\
**  source       * sswinnt.h
**  directory    * ss
**  description  * 
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


#ifndef SSWINNT_H
#define SSWINNT_H

#if defined(SS_NT) 

#include "ssenv.h"

/*  If defined, the following flags inhibit definition
 *     of the indicated items.
 *  NOGDICAPMASKS     - CC_*, LC_*, PC_*, CP_*, TC_*, RC_
 *  NOVIRTUALKEYCODES - VK_*
 *  NOWINMESSAGES     - WM_*, EM_*, LB_*, CB_*
 *  NOWINSTYLES       - WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
 *  NOSYSMETRICS      - SM_*
 *  NOMENUS           - MF_*
 *  NOICONS           - IDI_*
 *  NOKEYSTATES       - MK_*
 *  NOSYSCOMMANDS     - SC_*
 *  NORASTEROPS       - Binary and Tertiary raster ops
 *  NOSHOWWINDOW      - SW_*
 *  OEMRESOURCE       - OEM Resource values
 *  NOATOM            - Atom Manager routines
 *  NOCLIPBOARD       - Clipboard routines
 *  NOCOLOR           - Screen colors
 *  NOCTLMGR          - Control and Dialog routines
 *  NODRAWTEXT        - DrawText() and DT_*
 *  NOGDI             - All GDI defines and routines
 *  NOKERNEL          - All KERNEL defines and routines
 *  NOUSER            - All USER defines and routines
 *  NONLS             - All NLS defines and routines
 *  NOMB              - MB_* and MessageBox()
 *  NOMEMMGR          - GMEM_*, LMEM_*, GHND, LHND, associated routines
 *  NOMETAFILE        - typedef METAFILEPICT
 *  NOMINMAX          - Macros min(a,b) and max(a,b)
 *  NOMSG             - typedef MSG and associated routines
 *  NOOPENFILE        - OpenFile(), OemToAnsi, AnsiToOem, and OF_*
 *  NOSCROLL          - SB_* and scrolling routines
 *  NOSERVICE         - All Service Controller routines, SERVICE_ equates, etc.
 *  NOSOUND           - Sound driver routines
 *  NOTEXTMETRIC      - typedef TEXTMETRIC and associated routines
 *  NOWH              - SetWindowsHook and WH_*
 *  NOWINOFFSETS      - GWL_*, GCL_*, associated routines
 *  NOCOMM            - COMM driver routines
 *  NOKANJI           - Kanji support stuff.
 *  NOHELP            - Help engine interface.
 *  NOPROFILER        - Profiler interface.
 *  NODEFERWINDOWPOS  - DeferWindowPos routines
 */
#if !defined(SS_WINNT_NEEDSGDI) && (_MSC_VER < 1000)
#define NOGDI
#endif

#if _MSC_VER >= 1000
#define NOIME
#endif

#if !defined(SS_WINNT_NOT_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
#endif 

#define MMNODRV         /* Installable driver support */
#define MMNOSOUND       /* Sound support */
#define MMNOWAVE        /* Waveform support */
#define MMNOMIDI        /* MIDI support */
#define MMNOAUX         /* Auxiliary audio support */
#define MMNOMIXER       /* Mixer support */
#define MMNOTIMER       /* Timer support */
#define MMNOJOY         /* Joystick support */
#define MMNOMCI         /* MCI support */
#define MMNOMMIO        /* Multimedia file I/O support */
#define MMNOMMSYSTEM    /* General MMSYSTEM functions */

#define _WIN32_WINNT 0x0403 /* enable TryEnterCriticalSection etc. */

#include <windows.h>

#endif

#endif /* SSWINNT_H */
