/*************************************************************************\
**  source       * ssservic.c
**  directory    * ss
**  description  * Service function interface.
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

#if defined(SS_LINUX) || defined(SS_FREEBSD)
/* prototype for fork() */
#include <unistd.h>
#endif /* SS_LINUX */

#include "sswindow.h"
#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssc.h"
#include "ssdebug.h"
#include "ssthread.h"
#include "ssutf.h"

#include "ssservic.h"
#include "ssmem.h"

#if defined(SS_SCO)

#include <locale.h>

/*#***********************************************************************\
 * 
 *		server_init
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
static void server_init(void)
{
        setlocale(LC_ALL, "C");
}

#else /* SS_SCO */

/*#***********************************************************************\
 * 
 *		server_init
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
static void server_init(void)
{
        /* empty */
}

#endif /* SS_SCO */

/*#***********************************************************************\
 * 
 *		svc_local_main
 * 
 * Calls init, process and done functions.
 * 
 * Parameters :
 * 
 *      init_fp - in, hold
 *          Init function.
 *          
 *      process_fp - in, hold
 *          Process function.
 *          
 *      done_fp - in, hold
 *          Done function.
 *          
 *      stop_fp - in, hold
 *          Stop function.
 *          
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void svc_local_main(
        bool (*init_fp)(void),
        bool (*process_fp)(void),
        bool (*done_fp)(void))
{
        if ((*init_fp)()) {
            (*process_fp)();
            (*done_fp)();
        }
}

/*########################################################################*/
#if defined(SS_NT) && defined(SS_NTSERVICE)
/*########################################################################*/

#include <process.h>

/* This event is signalled when the
 * worker thread ends.
 */
static HANDLE                  svc_doneevent = NULL;
static SERVICE_STATUS          svc_status;    /* current status of the service */
static SERVICE_STATUS_HANDLE   svc_statushandle = 0;
static HANDLE                  svc_threadhandle = NULL;
static DWORD                   svc_curstate = SERVICE_START_PENDING;

static char*    svc_name;

static bool     (*svc_init_fp)(void);
static bool     (*svc_process_fp)(void);
static bool     (*svc_done_fp)(void);
static bool     (*svc_stop_fp)(void);

static bool     svc_isservice = FALSE;
static bool     svc_started = FALSE;
static bool     svc_stopping = FALSE;

static int      svc_argc;
static char**   svc_argv;

static DWORD    svc_checkpoint;

/*#***********************************************************************\
 * 
 *		svc_reportstatustosccmgr
 * 
 * This function is called by the ServMainFunc() and
 * ServCtrlHandler() functions to update the service's status
 * to the service control manager.
 * 
 * Parameters : 
 * 
 *	current_state - 
 *		
 *		
 *	win32_exit_code - 
 *		
 *		
 *	checkpoint - 
 *		
 *		
 *	wait_hint - 
 *		
 *		
 *	 - 
 *		
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
static BOOL svc_reportstatustosccmgr(
        DWORD current_state,
        DWORD win32_exit_code,
        DWORD checkpoint,
        DWORD wait_hint)
{
        BOOL retcode;

        ss_pprintf_3(("svc_reportstatustosccmgr:status = %ld\n", current_state));

        /* Disable control requests until the service is started.
         */
        if (current_state == SERVICE_RUNNING) {
            svc_status.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                                            SERVICE_ACCEPT_SHUTDOWN;
        } else {
            svc_status.dwControlsAccepted = 0;
        }

        /* These SERVICE_STATUS members are set from parameters.
         */
        svc_status.dwCurrentState = current_state;
        svc_status.dwWin32ExitCode = win32_exit_code;
        svc_status.dwCheckPoint = checkpoint;
        svc_status.dwWaitHint = wait_hint;

        /* Report the status of the service to the service control manager.
         */
        retcode = SetServiceStatus(
                    svc_statushandle,   /* service reference handle */
                    &svc_status);       /* SERVICE_STATUS structure */

        if (!retcode) {
            /* An error occured.
             */
            ss_pprintf_4(("svc_reportstatustosccmgr:SetServiceStatus failed\n"));
            ss_svc_logmessage(SS_SVC_ERROR, "SetServiceStatus");
        } else {
            svc_curstate = current_state;
        }
        return(retcode);
}

/*#***********************************************************************\
 * 
 *		service_ctrl
 * 
 * This function is called by the Service Controller whenever
 * someone calls ControlService in reference to our service.
 * 
 * Parameters : 
 * 
 *	ctrl_code - 
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
static VOID WINAPI service_ctrl(DWORD ctrl_code)
{
        ss_pprintf_3(("service_ctrl\n"));

        /* Handle the requested control code.
         */
        switch (ctrl_code) {

            /* Shutdown the system.
             */
            case SERVICE_CONTROL_SHUTDOWN:
                ss_pprintf_4(("service_ctrl:SERVICE_CONTROL_SHUTDOWN\n"));
                /* FALLTHROUGH */

            /* Stop the service.
             */
            case SERVICE_CONTROL_STOP:

                ss_pprintf_4(("service_ctrl:SERVICE_CONTROL_STOP\n"));

                /* Report the status, specifying the checkpoint and waithint,
                 * before setting the termination event.
                 */
                svc_reportstatustosccmgr(
                        SERVICE_STOP_PENDING, /* current state */
                        NO_ERROR,             /* exit code */
                        svc_checkpoint++,     /* checkpoint */
                        10000);               /* waithint */

                /* Call the user given stop function
                 */
                if (!svc_stopping) {
                    ss_pprintf_4(("service_ctrl:Call svc_stop_fp\n"));
                    svc_stopping = TRUE;
                    (*svc_stop_fp)();
                }
                return;

            /* Update the service status.
             */
            case SERVICE_CONTROL_INTERROGATE:
                ss_pprintf_4(("service_ctrl:SERVICE_CONTROL_INTERROGATE\n"));
                break;

            /* Invalid control code
             */
            default:
                ss_pprintf_4(("service_ctrl:Invalid control code %d\n", ctrl_code));
                break;
        }

        /* Send a status response.
         */
        svc_reportstatustosccmgr(svc_curstate, NO_ERROR, 0, 0);
}

/*#***********************************************************************\
 * 
 *		service_worker_thread
 * 
 * this function does the actual nuts and bolts work that
 * the service requires.
 * 
 * Parameters : 
 * 
 *	not_used - 
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
static VOID _CRTAPI1 service_worker_thread(VOID* not_used)
{
        ss_pprintf_3(("service_worker_thread:begin\n"));

        ss_pprintf_4(("service_worker_thread:svc_process_fp\n"));
        (*svc_process_fp)();
        
        svc_stopping = TRUE;

        ss_pprintf_4(("service_worker_thread:svc_done_fp\n"));
        (*svc_done_fp)();

        ss_pprintf_4(("service_worker_thread:set event svc_doneevent\n"));
        SetEvent(svc_doneevent);

        ss_pprintf_3(("service_worker_thread:end\n"));
}

/*#***********************************************************************\
 * 
 *		service_main
 * 
 * This function takes care of actually starting the service,
 * informing the service controller at each step along the way.
 * After launching the worker thread, it waits on the event
 * that the worker thread will signal at its termination.
 * 
 * Parameters : 
 * 
 *	dwArgc - 
 *		
 *		
 *	lpszArgv - 
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
static VOID WINAPI service_main(DWORD argc, LPTSTR* argv)
{
        DWORD wait_ret;

        ss_pprintf_3(("service_main\n"));

        svc_checkpoint = 1;
        svc_curstate = SERVICE_START_PENDING;
        svc_name = argv[0];
        svc_started = FALSE;
        svc_stopping = FALSE;
        svc_argc = argc;
        svc_argv = argv;
        svc_name = SS_SERVER_NAME;

        /* Register our service control handler:
         */
        svc_statushandle = RegisterServiceCtrlHandler(
                                TEXT(svc_name),
                                service_ctrl);
        if (!svc_statushandle) {
            ss_pprintf_4(("service_main:RegisterServiceCtrlHandler failed\n"));
            goto cleanup;
        }

        /* SERVICE_STATUS members that don't change.
         */
        svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        svc_status.dwServiceSpecificExitCode = 0;

        /* Report the status to Service Control Manager.
         */
        if (!svc_reportstatustosccmgr(
                SERVICE_START_PENDING, /* service state */
                NO_ERROR,              /* exit code */
                svc_checkpoint++,      /* checkpoint */
                3000)) {               /* wait hint */
            ss_pprintf_4(("service_main:svc_reportstatustosccmgr 1 failed\n"));
            goto cleanup;
        }

        /* Create the event object. The control handler function signals
         * this event when it receives the "stop" control code.
         */
        svc_doneevent = CreateEvent(
                            NULL,    /* no security attributes */
                            TRUE,    /* manual reset event */
                            FALSE,   /* not-signalled */
                            NULL);   /* no name */

        if (svc_doneevent == (HANDLE)NULL) {
            ss_pprintf_4(("service_main:CreateEvent failed\n"));
            goto cleanup;
        }

        /* Report the status to the service control manager.
         */
        if (!svc_reportstatustosccmgr(
                SERVICE_START_PENDING, /* service state */
                NO_ERROR,              /* exit code */
                svc_checkpoint++,      /* checkpoint */
                10000)) {              /* wait hint */
            ss_pprintf_4(("service_main:svc_reportstatustosccmgr 2 failed\n"));
            goto cleanup;
        }

        ss_pprintf_4(("service_main:call svc_init_fp\n"));
        if (!(*svc_init_fp)()) {
            ss_pprintf_4(("service_main:svc_init_fp failed\n"));
            goto cleanup;
        }

        /* Start the thread that performs the work of the service.
         */
        svc_threadhandle = (HANDLE)_beginthread(
                                    service_worker_thread,
                                    16 * 1024,      /* stack size */
                                    NULL);          /* argument to thread */
        if (!svc_threadhandle) {
            ss_pprintf_4(("service_main:_beginthread failed\n"));
            goto cleanup;
        }

        /* Report the status to the service control manager.
         */
        if (!svc_reportstatustosccmgr(
                SERVICE_RUNNING, /* service state */
                NO_ERROR,        /* exit code */
                0,               /* checkpoint */
                0)) {            /* wait hint */
            ss_pprintf_4(("service_main:svc_reportstatustosccmgr 3 failed\n"));
            if (!svc_stopping) {
                svc_stopping = TRUE;
                (*svc_stop_fp)();
            }
            goto cleanup;
        }

        svc_curstate = SERVICE_RUNNING;
        svc_checkpoint = 1;
        svc_started = TRUE;

        /* Wait indefinitely until svc_doneevent is signaled.
         */
        ss_pprintf_4(("service_main:wait for event svc_doneevent\n"));
        wait_ret = WaitForSingleObject(
                    svc_doneevent,  /* event object */
                    INFINITE);       /* wait indefinitely */

        ss_pprintf_4(("service_main:got event svc_doneevent\n"));

        svc_reportstatustosccmgr(
                SERVICE_STOP_PENDING, /* current state */
                NO_ERROR,             /* exit code */
                svc_checkpoint++,     /* checkpoint */
                10000);               /* waithint */

cleanup:

        ss_pprintf_4(("service_main:cleanup\n"));

        if (svc_doneevent != NULL) {
            CloseHandle(svc_doneevent);
        }

        /* Try to report the stopped status to the service control manager.
         */
        if (svc_statushandle != 0) {
            svc_reportstatustosccmgr(
                SERVICE_STOPPED,
                NO_ERROR,
                0,
                0);
        }

        /* When SERVICE MAIN FUNCTION returns in a single service
         * process, the StartServiceCtrlDispatcher function in
         * the main thread returns, terminating the process.
         */
        return;
}

/*##**********************************************************************\
 * 
 *		ss_svc_main
 * 
 * Main service function. Start and possibly registers the service and
 * call the function pointers that do actual service work.
 * 
 * Parameters :
 * 
 *      name - in, hold
 *          Server name.
 *          
 *      init_fp - in, hold
 *          Init function. Initializes the application. If this function
 *          returns TRUE, process_fp and done_fp functions are called.
 *          If this function returns FALSE, nothing is done (done_fp is
 *          not called).
 *          
 *      process_fp - in, hold
 *          Process function. When this function returns, the service
 *          is stopped after calling done_fp.
 *          
 *      done_fp - in, hold
 *          Done function.
 *          
 *      stop_fp - in, hold
 *          Stop function. Used to stop the service by external request.
 *          Normally service should be stopped by returning from the
 *          process_fp function.
 *          
 *      quickstop_fp - in, hold
 *          Quick stop function. Used to stop the service by external request.
 *          Used in cases when the service should be stopped immediately.
 *          
 *      p_foreground - in, out
 *          If *p_foreground==SS_SVC_FOREGROUND_YES, process is run in
 *          the foreground, otherwise default is set by this function.
 *          The availibility of background execution depends on the
 *          operating system.
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void ss_svc_main(
        char* name,
        bool (*init_fp)(void),
        bool (*process_fp)(void),
        bool (*done_fp)(void),
        bool (*stop_fp)(void),
        bool (*quickstop_fp)(void),
        ss_svc_foreground_t* p_foreground)
{
        SERVICE_TABLE_ENTRY dispatch_table[] = {
            { TEXT(name), (LPSERVICE_MAIN_FUNCTION)service_main },
            { NULL, NULL }
        };

        ss_pprintf_1(("ss_svc_main:begin %s\n", name));

        server_init();

        if (*p_foreground != SS_SVC_FOREGROUND_NO || SsEnvOs() != SS_OS_WNT) {
            
            ss_pprintf_2(("ss_svc_main:run in the foreground\n"));
            *p_foreground = SS_SVC_FOREGROUND_YES;
            svc_isservice = FALSE;
            svc_local_main(init_fp, process_fp, done_fp);

        } else {
            
            ss_pprintf_2(("ss_svc_main:run in the background\n"));
            *p_foreground = SS_SVC_FOREGROUND_NO;
            svc_isservice = TRUE;
            svc_name = name;
            svc_init_fp = init_fp;
            svc_process_fp = process_fp;
            svc_done_fp = done_fp;
            svc_stop_fp = stop_fp;

            ss_pprintf_2(("ss_svc_main:StartServiceCtrlDispatcher\n"));

            if (!StartServiceCtrlDispatcher(dispatch_table)) {
                ss_pprintf_2(("ss_svc_main:StartServiceCtrlDispatcher failed\n"));
                ss_svc_logmessage(SS_SVC_ERROR, "StartServiceCtrlDispatcher failed.");
                /* ss_svc_stop(FALSE); */
            }
            ss_pprintf_2(("ss_svc_main:done\n"));
        }
}

/*##**********************************************************************\
 * 
 *		ss_svc_stop
 * 
 * This function can be used by any thread to report an
 * error, or stop the service.
 * 
 * Parameters : 
 * 
 *	internal_errorp - in
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
void ss_svc_stop(bool internal_errorp)
{
        if (!svc_started) {
            return;
        }

        ss_pprintf_1(("ss_svc_stop:begin, internal_errorp = %d\n", internal_errorp));

        if (internal_errorp) {
            /* Report the status, specifying the checkpoint and waithint,
             * before setting the termination event.
             */
            svc_reportstatustosccmgr(
                SERVICE_STOPPED,
                ERROR_SERVICE_SPECIFIC_ERROR,
                0,
                0);
            ss_pprintf_2(("ss_svc_stop:set event svc_doneevent\n"));
            SetEvent(svc_doneevent);

        } else if (!svc_stopping) {
            svc_stopping = TRUE;
            svc_reportstatustosccmgr(
                SERVICE_STOP_PENDING,   /* current state */
                NO_ERROR,               /* exit code */
                svc_checkpoint++,       /* checkpoint */
                10000);                 /* waithint */
            ss_pprintf_2(("ss_svc_stop:%d:svc_stop_fp\n", __LINE__));
            (*svc_stop_fp)();
        }

        ss_pprintf_2(("ss_svc_stop:end\n"));
}

/*##**********************************************************************\
 * 
 *		ss_svc_notify_init
 * 
 * Notifies the service system that initialization is still in
 * progress.
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
void ss_svc_notify_init(void)
{
        if (svc_started) {
            ss_dassert(!svc_stopping);
            svc_reportstatustosccmgr(
                SERVICE_START_PENDING, /* service state */
                NO_ERROR,              /* exit code */
                svc_checkpoint++,      /* checkpoint */
                10000);                /* wait hint */
        }
}

/*##**********************************************************************\
 * 
 *		ss_svc_notify_done
 * 
 * 
 * Notifies the service system that shutdown is still in
 * progress.
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
void ss_svc_notify_done(void)
{
        if (svc_stopping) {
            svc_reportstatustosccmgr(
                SERVICE_STOP_PENDING,   /* current state */
                NO_ERROR,               /* exit code */
                svc_checkpoint++,       /* checkpoint */
                10000);                 /* waithint */
        }
}

/*##**********************************************************************\
 * 
 *		ss_svc_logmessage
 * 
 * 
 * 
 * Parameters : 
 * 
 *	type - 
 *		
 *		
 *	msg - 
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
void ss_svc_logmessage(ss_svc_msgtype_t type, char* msg)
{
        HANDLE  event_source;
        CHAR    buf[80];
        LPTSTR  msgs[2];
        int     num_msgs = 1;
        int     msgtype;

        if (!svc_isservice) {
            return;
        }

        switch (type) {
            case SS_SVC_INFO:
                msgtype = EVENTLOG_INFORMATION_TYPE;
                break;
            case SS_SVC_WARNING:
                msgtype = EVENTLOG_WARNING_TYPE;
                break;
            case SS_SVC_ERROR:
            default:
                msgtype = EVENTLOG_ERROR_TYPE;
                /* Get last system error code.
                 */
                sprintf(buf, "GetLastError() = %d", GetLastError());
                num_msgs = 2;
                break;
        }

        msgs[0] = msg;
        msgs[1] = buf;

        event_source = RegisterEventSource(NULL, TEXT(svc_name));

        if (event_source != NULL) {
            ReportEvent(
                event_source,         /* handle of event source */
                (WORD)msgtype,        /* event type */
                0,                    /* event category */
                0,                    /* event ID */
                NULL,                 /* current user's SID */
                (WORD)num_msgs,       /* count in msgs */
                0,                    /* no bytes of raw data */
                msgs,                 /* array of messages */
                NULL);                /* no raw data */

            DeregisterEventSource(event_source);
        }
}

/*##**********************************************************************\
 * 
 *		ss_svc_isservice
 * 
 * Checks if the process is run as a service. Service processes are
 * normally run in the background, they do not have own terminal for
 * output. When this function is called before ss_svc_main, the default
 * service status is returned. After ss_svc_main the actual service
 * status is returned.
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 *      This function is not allowed to produce any output.
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool ss_svc_isservice(void)
{
        return(svc_isservice);
}

/*#***********************************************************************\
 * 
 *		svc_installservice
 * 
 * Installs a service to service manager.
 * 
 * Parameters : 
 * 
 *	service_manager - 
 *		
 *		
 *	service_name - 
 *		
 *		
 *	service_exe - 
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
static bool svc_installservice(
        SC_HANDLE service_manager,
        LPCTSTR service_name,
        LPCTSTR service_exe,
        bool autostart)
{
        int rc;
        SC_HANDLE service_handle;

        service_handle = CreateService(
                            service_manager,            // SCManager database
                            service_name,               // name of service
                            service_name,               // name to display
                            STANDARD_RIGHTS_REQUIRED|
                                SERVICE_QUERY_CONFIG|
                                SERVICE_QUERY_STATUS|
                                SERVICE_START|
                                SERVICE_STOP,           // desired access
                            SERVICE_WIN32_OWN_PROCESS,  // service type
                            autostart
                                ? SERVICE_AUTO_START
                                : SERVICE_DEMAND_START, // start type
                            SERVICE_ERROR_NORMAL,       // error control type
                            service_exe,                // services binary
                            NULL,                       // no load ordering group
                            NULL,                       // no tag identifier
                            NULL,                       // no dependencies
                            NULL,                       // LocalSystem account
                            NULL);                      // no password

        if (service_handle == NULL) {
            rc = GetLastError();
            ss_pprintf_2(("svc_installservice(%s, %s) failure: CreateService (0x%02x)\n",
                service_name, service_exe, rc));
            return(rc);
        }

        CloseServiceHandle(service_handle);

        return(0);
}

/*#***********************************************************************\
 * 
 *		svc_removeservice
 * 
 * Removes service from service manager.
 * 
 * Parameters : 
 * 
 *	service_manager - 
 *		
 *		
 *	service_name - 
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
static int svc_removeservice(SC_HANDLE service_manager, LPCTSTR service_name)
{
        SC_HANDLE service_handle;
        BOOL ret;
        int rc;

        service_handle = OpenService(service_manager, service_name, DELETE);

        if (service_handle == NULL) {
            rc = GetLastError();
            ss_pprintf_2(("svc_removeservice(%s) failure: OpenService (0x%02x)\n",
                service_name, rc));
            return(rc);
        }

        ret = DeleteService(service_handle);

        if (!ret) {
            rc = GetLastError();
            ss_pprintf_2(("svc_removeservice(%s) failure: DeleteService (0x%02x)\n",
                service_name, rc));
            return(rc);
        } else {
            return(0);
        }
}

/*##**********************************************************************\
 * 
 *		ss_svc_install
 * 
 * Install a new servive with given name and exe path.
 * 
 * Parameters : 
 * 
 *	name - 
 *		
 *		
 *	exe_location - 
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
int ss_svc_install(char* name, char* exe_location, bool autostart)
{
        int ret;
        SC_HANDLE service_manager;
        char* name_ASCII;
        char* exe_location_ASCII;

        ss_pprintf_1(("ss_svc_install(%s, %s)\n", name, exe_location));

        service_manager = OpenSCManager(
                            NULL,                       // machine (NULL == local)
                            NULL,                       // database (NULL == default)
                            SC_MANAGER_CREATE_SERVICE); // access required
        if (service_manager == NULL) {
            ret = GetLastError();
            ss_pprintf_2(("ss_svc_install failure: OpenSCManager (0x%02x)\n", ret));
        } else {
            name_ASCII = SsUTF8toASCII8Strdup(name);
            exe_location_ASCII = SsUTF8toASCII8Strdup(exe_location);
            ret = svc_installservice(service_manager, name_ASCII, exe_location_ASCII, autostart);
            CloseServiceHandle(service_manager);
            SsMemFree(name_ASCII);
            SsMemFree(exe_location_ASCII);
        }

        return(ret);
}

/*##**********************************************************************\
 * 
 *		ss_svc_remove
 * 
 * Removes a service with a given name.
 * 
 * Parameters : 
 * 
 *	name - 
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
int ss_svc_remove(char* name)
{
        int ret;
        SC_HANDLE service_manager;
        char* name_ASCII;
        
        ss_pprintf_1(("ss_svc_remove(%s)\n", name));

        service_manager = OpenSCManager(
                            NULL,                   // machine (NULL == local)
                            NULL,                   // database (NULL == default)
                            SC_MANAGER_CONNECT);    // access required

        if (service_manager == NULL) {
            ret = GetLastError();
            ss_pprintf_2(("ss_svc_remove failure: OpenSCManager (0x%02x)\n", ret));
        } else {
            name_ASCII = SsUTF8toASCII8Strdup(name);
            ret = svc_removeservice(service_manager, name_ASCII);
            CloseServiceHandle(service_manager);
            SsMemFree(name_ASCII);
        }

        return(ret);
}

/*##**********************************************************************\
 * 
 *		ss_svc_errorcodetotext
 * 
 * Returns text for an error code.
 * 
 * Parameters : 
 * 
 *	rc - 
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
char* ss_svc_errorcodetotext(int rc)
{
        switch (rc) {
            case ERROR_ACCESS_DENIED:
                return("ERROR_ACCESS_DENIED");
            case ERROR_DUP_NAME:
                return("ERROR_DUP_NAME");
            case ERROR_INVALID_HANDLE:
                return("ERROR_INVALID_HANDLE");
            case ERROR_INVALID_NAME:
                return("ERROR_INVALID_NAME");
            case ERROR_INVALID_PARAMETER:
                return("ERROR_INVALID_PARAMETER");
            case ERROR_INVALID_SERVICE_ACCOUNT:
                return("ERROR_INVALID_SERVICE_ACCOUNT");
            case ERROR_INVALID_DATA:
                return("ERROR_INVALID_DATA");
            case ERROR_SERVICE_EXISTS:
                return("ERROR_SERVICE_EXISTS");
            case ERROR_SERVICE_MARKED_FOR_DELETE:
                return("ERROR_SERVICE_MARKED_FOR_DELETE");
            case ERROR_DATABASE_DOES_NOT_EXIST:
                return("ERROR_DATABASE_DOES_NOT_EXIST");
            default:
                return("Unknown code");
        }
}

/*########################################################################*/
#elif defined(SS_UNIX) /* SS_NT */
/*########################################################################*/

static bool svc_isservice = TRUE;

void ss_svc_main(
        char* name,
        bool (*init_fp)(void),
        bool (*process_fp)(void),
        bool (*done_fp)(void),
        bool (*stop_fp)(void),
        bool (*quickstop_fp)(void) __attribute__ ((unused)),
        ss_svc_foreground_t* p_foreground)
{
        ss_dassert(name != NULL);
        SS_NOTUSED(name);
        SS_NOTUSED(stop_fp);
        
        server_init();
        
        if (*p_foreground == SS_SVC_FOREGROUND_UNKNOWN) {
            if (svc_isservice) {
                *p_foreground = SS_SVC_FOREGROUND_NO;
            } else {
                *p_foreground = SS_SVC_FOREGROUND_YES;
            }
        }
        
        svc_isservice = (*p_foreground == SS_SVC_FOREGROUND_NO);
        
        if (*p_foreground == SS_SVC_FOREGROUND_YES) {
            svc_local_main(init_fp, process_fp, done_fp);
        } else {
            ss_dprintf_1(("Inside ss_svc_main, forking.\n"));
            switch (fork()) {
                case -1:	
                    printf("%s Fatal Error: Failed to create a child\n", name);
                    exit(1);
                case 0:	/* child */
                    ss_dprintf_1(("Child starts\n"));
                    
#if !defined(SS_DEBUG) && !defined(SS_STDIO_ENABLED)
                    /* Closing standard i/o streams for daemon processes
                     * in PRODUCT builds, unless SS_STDIO_ENABLED is
                     * explicitly defined during compilation.
                     */
                    (void)fclose(stdin);
                    (void)fclose(stdout);
                    (void)fclose(stderr);
#endif /* !SS_DEBUG && !SS_STDIO_ENABLED */

#if defined(SS_UNIX)
                    /* Detach from session and create our own (Unixy).
                     * May return -1 (and fail) but that can be normal so
                     * we don't care. */
                    setsid();
#endif                    
                    svc_local_main(init_fp, process_fp, done_fp);
                    exit(0);
                default:		
                    ss_dprintf_1(("Terminal should be free now.\n"));
                    exit(0);    /* Parent disappears, terminal is free ? */
            }
        }
}

void ss_svc_stop(bool internal_errorp)
{
        SS_NOTUSED(internal_errorp);
}

void ss_svc_notify_init(void)
{
}

void ss_svc_notify_done(void)
{
}

void ss_svc_logmessage(
        ss_svc_msgtype_t type __attribute__ ((unused)),
        char* msg __attribute__ ((unused)))
{
}

bool ss_svc_isservice(void)
{
        return(svc_isservice);
}

int ss_svc_install(
        char* name,
        char* exe_location,
        bool autostart)
{
        SS_NOTUSED(name);
        SS_NOTUSED(exe_location);
        SS_NOTUSED(autostart);
        return(-1);
}

int ss_svc_remove(
        char* name)
{
        SS_NOTUSED(name);
        return(-1);
}

char* ss_svc_errorcodetotext(int rc)
{
        SS_NOTUSED(rc);
        return((char *)"Services not supported");
}

/*########################################################################*/
#else /* SS_NT */
/*########################################################################*/

static bool svc_isservice = FALSE;

void ss_svc_main(
        char* name,
        bool (*init_fp)(void),
        bool (*process_fp)(void),
        bool (*done_fp)(void),
        bool (*stop_fp)(void),
        bool (*quickstop_fp)(void),
        ss_svc_foreground_t* p_foreground)
{
        SS_NOTUSED(name);
        SS_NOTUSED(stop_fp);
        SS_NOTUSED(quickstop_fp);

        if (*p_foreground == SS_SVC_FOREGROUND_UNKNOWN) {
            if (svc_isservice) {
                *p_foreground = SS_SVC_FOREGROUND_NO;
            } else {
                *p_foreground = SS_SVC_FOREGROUND_YES;
            }
        }
        svc_isservice = (*p_foreground == SS_SVC_FOREGROUND_NO);

        server_init();
        svc_local_main(init_fp, process_fp, done_fp);
}

void ss_svc_stop(bool internal_errorp)
{
        SS_NOTUSED(internal_errorp);
}

void ss_svc_notify_init(void)
{
}

void ss_svc_notify_done(void)
{
}

void ss_svc_logmessage(ss_svc_msgtype_t type, char* msg)
{
}

bool ss_svc_isservice(void)
{
        return(svc_isservice);
}

int ss_svc_install(
        char* name,
        char* exe_location,
        bool autostart)
{
        SS_NOTUSED(name);
        SS_NOTUSED(exe_location);
        SS_NOTUSED(autostart);
        return(-1);
}

int ss_svc_remove(
        char* name)
{
        SS_NOTUSED(name);
        return(-1);
}

char* ss_svc_errorcodetotext(int rc)
{
        SS_NOTUSED(rc);
        return("Services not supported");
}

#endif /* SS_NT */
