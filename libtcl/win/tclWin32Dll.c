/* 
 * tclWin32Dll.c --
 *
 *	This file contains the DLL entry point which sets up the 32-to-16-bit
 *	thunking code for SynchSpawn if the library is running under Win32s.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWin32Dll.c 1.21 97/08/05 11:47:10
 */

#include "tclWinInt.h"

typedef DWORD (WINAPI * UT32PROC)(LPVOID lpBuff, DWORD dwUserDefined,
	LPVOID *lpTranslationList);

typedef BOOL (WINAPI * PUTREGISTER)(HANDLE hModule, LPCSTR SixteenBitDLL,
	LPCSTR InitName, LPCSTR ProcName, UT32PROC* ThirtyTwoBitThunk,
	FARPROC UT32Callback, LPVOID Buff);

typedef VOID (WINAPI * PUTUNREGISTER)(HANDLE hModule);

static PUTUNREGISTER UTUnRegister = NULL;
static int           tclProcessesAttached = 0;

/*
 * The following data structure is used to keep track of all of the DLL's
 * opened by Tcl so that they can be freed with the Tcl.dll is unloaded.
 */

typedef struct LibraryList {
    HINSTANCE handle;
    struct LibraryList *nextPtr;
} LibraryList;

static LibraryList *libraryList = NULL;	/* List of currently loaded DLL's.  */

static HINSTANCE tclInstance;	/* Global library instance handle. */
static int tclPlatformId;	/* Running under NT, 95, or Win32s? */

/*
 * Declarations for functions that are only used in this file.
 */

static void 		UnloadLibraries _ANSI_ARGS_((void));

/*
 * The following declaration is for the VC++ DLL entry point.
 */

BOOL APIENTRY		DllMain _ANSI_ARGS_((HINSTANCE hInst,
			    DWORD reason, LPVOID reserved));

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Borland to invoke the
 *	initialization code for Tcl.  It simply calls the DllMain
 *	routine.
 *
 * Results:
 *	See DllMain.
 *
 * Side effects:
 *	See DllMain.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HINSTANCE hInst;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    return DllMain(hInst, reason, reserved);
}

/*
 *----------------------------------------------------------------------
 *
 * DllMain --
 *
 *	This routine is called by the VC++ C run time library init
 *	code, or the DllEntryPoint routine.  It is responsible for
 *	initializing various dynamically loaded libraries.
 *
 * Results:
 *	TRUE on sucess, FALSE on failure.
 *
 * Side effects:
 *	Establishes 32-to-16 bit thunk and initializes sockets library.
 *
 *----------------------------------------------------------------------
 */
BOOL APIENTRY
DllMain(hInst, reason, reserved)
    HINSTANCE hInst;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    OSVERSIONINFO os;

    switch (reason) {
    case DLL_PROCESS_ATTACH:

	/*
	 * Registration of UT need to be done only once for first
	 * attaching process.  At that time set the tclWin32s flag
	 * to indicate if the DLL is executing under Win32s or not.
	 */

	if (tclProcessesAttached++) {
	    return FALSE;         /* Not the first initialization. */
	}

	tclInstance = hInst;
	os.dwOSVersionInfoSize = sizeof(os);
	GetVersionEx(&os);
	tclPlatformId = os.dwPlatformId;

	/*
	 * The following code stops Windows 3.x from automatically putting 
	 * up Sharing Violation dialogs, e.g, when someone tries to
	 * access a file that is locked or a drive with no disk in it.
	 * Tcl already returns the appropriate error to the caller, and they 
	 * can decide to put up their own dialog in response to that failure.  
	 *
	 * Under 95 and NT, the system doesn't automatically put up dialogs 
	 * when the above operations fail.
	 */

	if (tclPlatformId == VER_PLATFORM_WIN32s) {
	    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);
	}

	return TRUE;

    case DLL_PROCESS_DETACH:

	tclProcessesAttached--;
	if (tclProcessesAttached == 0) {

	    /*
	     * Unregister the Tcl thunk.
	     */

	    if (UTUnRegister != NULL) {
		UTUnRegister(hInst);
	    }

	    /*
	     * Cleanup any dynamically loaded libraries.
	     */

	    UnloadLibraries();

            /*
             * And finally finalize our use of Tcl.
             */

            Tcl_Finalize();
	}
	break;
    }

    return TRUE; 
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinLoadLibrary --
 *
 *	This function is a wrapper for the system LoadLibrary.  It is
 *	responsible for adding library handles to the library list so
 *	the libraries can be freed when tcl.dll is unloaded.
 *
 * Results:
 *	Returns the handle of the newly loaded library, or NULL on
 *	failure.
 *
 * Side effects:
 *	Loads the specified library into the process.
 *
 *----------------------------------------------------------------------
 */

HINSTANCE
TclWinLoadLibrary(name)
    char *name;			/* Library file to load. */
{
    HINSTANCE handle;
    LibraryList *ptr;

    handle = LoadLibrary(name);
    if (handle != NULL) {
	ptr = (LibraryList*) ckalloc(sizeof(LibraryList));
	ptr->handle = handle;
	ptr->nextPtr = libraryList;
	libraryList = ptr;
    } else {
	TclWinConvertError(GetLastError());
    }
    return handle;
}

/*
 *----------------------------------------------------------------------
 *
 * UnloadLibraries --
 *
 *	Frees any dynamically allocated libraries loaded by Tcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the libraries on the library list as well as the list.
 *
 *----------------------------------------------------------------------
 */

static void
UnloadLibraries()
{
    LibraryList *ptr;

    while (libraryList != NULL) {
	FreeLibrary(libraryList->handle);
	ptr = libraryList->nextPtr;
	ckfree((char*)libraryList);
	libraryList = ptr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinSynchSpawn --
 *
 *	32-bit entry point to the 16-bit SynchSpawn code.
 *
 * Results:
 *	1 on success, 0 on failure.
 *
 * Side effects:
 *	Spawns a command and waits for it to complete.
 *
 *----------------------------------------------------------------------
 */
int 
TclWinSynchSpawn(void *args, int type, void **trans, Tcl_Pid *pidPtr)
{
    static UT32PROC UTProc = NULL;
    static int utErrorCode;

    if (UTUnRegister == NULL) {
	/*
	 * Load the Universal Thunking routines from kernel32.dll.
	 */

	HINSTANCE hKernel;
	PUTREGISTER UTRegister;
	char buffer[] = "TCL16xx.DLL";

	hKernel = TclWinLoadLibrary("Kernel32.Dll");
	if (hKernel == NULL) {
	    return 0;
	}

	UTRegister = (PUTREGISTER) GetProcAddress(hKernel, "UTRegister");
	UTUnRegister = (PUTUNREGISTER) GetProcAddress(hKernel, "UTUnRegister");
	if (!UTRegister || !UTUnRegister) {
	    UnloadLibraries();
	    return 0;
	}

	/*
	 * Construct the complete name of tcl16xx.dll.
	 */

	buffer[5] = '0' + TCL_MAJOR_VERSION;
	buffer[6] = '0' + TCL_MINOR_VERSION;

	/*
	 * Register the Tcl thunk.
	 */

	if (UTRegister(tclInstance, buffer, NULL, "UTProc", &UTProc, NULL,
		NULL) == FALSE) {
	    utErrorCode = GetLastError();
	}
    }

    if (UTProc == NULL) {
	/*
	 * The 16-bit thunking DLL wasn't found.  Return error code that
	 * indicates this problem.
	 */

	SetLastError(utErrorCode);
	return 0;
    }

    UTProc(args, type, trans);
    *pidPtr = 0;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinGetTclInstance --
 *
 *	Retrieves the global library instance handle.
 *
 * Results:
 *	Returns the global library instance handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HINSTANCE
TclWinGetTclInstance()
{
    return tclInstance;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinGetPlatformId --
 *
 *	Determines whether running under NT, 95, or Win32s, to allow 
 *	runtime conditional code.
 *
 * Results:
 *	The return value is one of:
 *	    VER_PLATFORM_WIN32s		Win32s on Windows 3.1. 
 *	    VER_PLATFORM_WIN32_WINDOWS	Win32 on Windows 95.
 *	    VER_PLATFORM_WIN32_NT	Win32 on Windows NT
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int		
TclWinGetPlatformId()
{
    return tclPlatformId;
}
