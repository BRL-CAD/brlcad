/* 
 * tclWin32Dll.c --
 *
 *	This file contains the DLL entry point.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclWinInt.h"

/*
 * The following data structures are used when loading the thunking 
 * library for execing child processes under Win32s.
 */

typedef DWORD (WINAPI UT32PROC)(LPVOID lpBuff, DWORD dwUserDefined,
	LPVOID *lpTranslationList);

typedef BOOL (WINAPI UTREGISTER)(HANDLE hModule, LPCSTR SixteenBitDLL,
	LPCSTR InitName, LPCSTR ProcName, UT32PROC **ThirtyTwoBitThunk,
	FARPROC UT32Callback, LPVOID Buff);

typedef VOID (WINAPI UTUNREGISTER)(HANDLE hModule);

/* 
 * The following variables keep track of information about this DLL
 * on a per-instance basis.  Each time this DLL is loaded, it gets its own 
 * new data segment with its own copy of all static and global information.
 */

static HINSTANCE hInstance;	/* HINSTANCE of this DLL. */
static int platformId;		/* Running under NT, or 95/98? */

/*
 * The following function tables are used to dispatch to either the
 * wide-character or multi-byte versions of the operating system calls,
 * depending on whether the Unicode calls are available.
 */

static TclWinProcs asciiProcs = {
    0,

    (BOOL (WINAPI *)(const TCHAR *, LPDCB)) BuildCommDCBA,
    (TCHAR *(WINAPI *)(TCHAR *)) CharLowerA,
    (BOOL (WINAPI *)(const TCHAR *, const TCHAR *, BOOL)) CopyFileA,
    (BOOL (WINAPI *)(const TCHAR *, LPSECURITY_ATTRIBUTES)) CreateDirectoryA,
    (HANDLE (WINAPI *)(const TCHAR *, DWORD, DWORD, SECURITY_ATTRIBUTES *, 
	    DWORD, DWORD, HANDLE)) CreateFileA,
    (BOOL (WINAPI *)(const TCHAR *, TCHAR *, LPSECURITY_ATTRIBUTES, 
	    LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, const TCHAR *, 
	    LPSTARTUPINFOA, LPPROCESS_INFORMATION)) CreateProcessA,
    (BOOL (WINAPI *)(const TCHAR *)) DeleteFileA,
    (HANDLE (WINAPI *)(const TCHAR *, WIN32_FIND_DATAT *)) FindFirstFileA,
    (BOOL (WINAPI *)(HANDLE, WIN32_FIND_DATAT *)) FindNextFileA,
    (BOOL (WINAPI *)(WCHAR *, LPDWORD)) GetComputerNameA,
    (DWORD (WINAPI *)(DWORD, WCHAR *)) GetCurrentDirectoryA,
    (DWORD (WINAPI *)(const TCHAR *)) GetFileAttributesA,
    (DWORD (WINAPI *)(const TCHAR *, DWORD nBufferLength, WCHAR *, 
	    TCHAR **)) GetFullPathNameA,
    (DWORD (WINAPI *)(HMODULE, WCHAR *, int)) GetModuleFileNameA,
    (DWORD (WINAPI *)(const TCHAR *, WCHAR *, DWORD)) GetShortPathNameA,
    (UINT (WINAPI *)(const TCHAR *, const TCHAR *, UINT uUnique, 
	    WCHAR *)) GetTempFileNameA,
    (DWORD (WINAPI *)(DWORD, WCHAR *)) GetTempPathA,
    (BOOL (WINAPI *)(const TCHAR *, WCHAR *, DWORD, LPDWORD, LPDWORD, LPDWORD,
	    WCHAR *, DWORD)) GetVolumeInformationA,
    (HINSTANCE (WINAPI *)(const TCHAR *)) LoadLibraryA,
    (TCHAR (WINAPI *)(WCHAR *, const TCHAR *)) lstrcpyA,
    (BOOL (WINAPI *)(const TCHAR *, const TCHAR *)) MoveFileA,
    (BOOL (WINAPI *)(const TCHAR *)) RemoveDirectoryA,
    (DWORD (WINAPI *)(const TCHAR *, const TCHAR *, const TCHAR *, DWORD, 
	    WCHAR *, TCHAR **)) SearchPathA,
    (BOOL (WINAPI *)(const TCHAR *)) SetCurrentDirectoryA,
    (BOOL (WINAPI *)(const TCHAR *, DWORD)) SetFileAttributesA,
};

static TclWinProcs unicodeProcs = {
    1,

    (BOOL (WINAPI *)(const TCHAR *, LPDCB)) BuildCommDCBW,
    (TCHAR *(WINAPI *)(TCHAR *)) CharLowerW,
    (BOOL (WINAPI *)(const TCHAR *, const TCHAR *, BOOL)) CopyFileW,
    (BOOL (WINAPI *)(const TCHAR *, LPSECURITY_ATTRIBUTES)) CreateDirectoryW,
    (HANDLE (WINAPI *)(const TCHAR *, DWORD, DWORD, SECURITY_ATTRIBUTES *, 
	    DWORD, DWORD, HANDLE)) CreateFileW,
    (BOOL (WINAPI *)(const TCHAR *, TCHAR *, LPSECURITY_ATTRIBUTES, 
	    LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, const TCHAR *, 
	    LPSTARTUPINFOA, LPPROCESS_INFORMATION)) CreateProcessW,
    (BOOL (WINAPI *)(const TCHAR *)) DeleteFileW,
    (HANDLE (WINAPI *)(const TCHAR *, WIN32_FIND_DATAT *)) FindFirstFileW,
    (BOOL (WINAPI *)(HANDLE, WIN32_FIND_DATAT *)) FindNextFileW,
    (BOOL (WINAPI *)(WCHAR *, LPDWORD)) GetComputerNameW,
    (DWORD (WINAPI *)(DWORD, WCHAR *)) GetCurrentDirectoryW,
    (DWORD (WINAPI *)(const TCHAR *)) GetFileAttributesW,
    (DWORD (WINAPI *)(const TCHAR *, DWORD nBufferLength, WCHAR *, 
	    TCHAR **)) GetFullPathNameW,
    (DWORD (WINAPI *)(HMODULE, WCHAR *, int)) GetModuleFileNameW,
    (DWORD (WINAPI *)(const TCHAR *, WCHAR *, DWORD)) GetShortPathNameW,
    (UINT (WINAPI *)(const TCHAR *, const TCHAR *, UINT uUnique, 
	    WCHAR *)) GetTempFileNameW,
    (DWORD (WINAPI *)(DWORD, WCHAR *)) GetTempPathW,
    (BOOL (WINAPI *)(const TCHAR *, WCHAR *, DWORD, LPDWORD, LPDWORD, LPDWORD, 
	    WCHAR *, DWORD)) GetVolumeInformationW,
    (HINSTANCE (WINAPI *)(const TCHAR *)) LoadLibraryW,
    (TCHAR (WINAPI *)(WCHAR *, const TCHAR *)) lstrcpyW,
    (BOOL (WINAPI *)(const TCHAR *, const TCHAR *)) MoveFileW,
    (BOOL (WINAPI *)(const TCHAR *)) RemoveDirectoryW,
    (DWORD (WINAPI *)(const TCHAR *, const TCHAR *, const TCHAR *, DWORD, 
	    WCHAR *, TCHAR **)) SearchPathW,
    (BOOL (WINAPI *)(const TCHAR *)) SetCurrentDirectoryW,
    (BOOL (WINAPI *)(const TCHAR *, DWORD)) SetFileAttributesW,
};

TclWinProcs *tclWinProcs;
static Tcl_Encoding tclWinTCharEncoding;

/*
 * The following declaration is for the VC++ DLL entry point.
 */

BOOL APIENTRY		DllMain(HINSTANCE hInst, DWORD reason, 
				LPVOID reserved);


#ifdef __WIN32__
#ifndef STATIC_BUILD


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
    switch (reason) {
    case DLL_PROCESS_ATTACH:
	TclWinInit(hInst);
	return TRUE;

    case DLL_PROCESS_DETACH:
	if (hInst == hInstance) {
	    Tcl_Finalize();
	}
	break;
    }

    return TRUE; 
}

#endif /* !STATIC_BUILD */
#endif /* __WIN32__ */

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
    return hInstance;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinInit --
 *
 *	This function initializes the internal state of the tcl library.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes the tclPlatformId variable.
 *
 *----------------------------------------------------------------------
 */

void
TclWinInit(hInst)
    HINSTANCE hInst;		/* Library instance handle. */
{
    OSVERSIONINFO os;

    hInstance = hInst;
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os);
    platformId = os.dwPlatformId;

    /*
     * We no longer support Win32s, so just in case someone manages to
     * get a runtime there, make sure they know that.
     */

    if (platformId == VER_PLATFORM_WIN32s) {
	panic("Win32s is not a supported platform");	
    }

    tclWinProcs = &asciiProcs;
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
 *	    VER_PLATFORM_WIN32s		Win32s on Windows 3.1. (not supported)
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
    return platformId;
}

/*
 *-------------------------------------------------------------------------
 *
 * TclWinNoBackslash --
 *
 *	We're always iterating through a string in Windows, changing the
 *	backslashes to slashes for use in Tcl.
 *
 * Results:
 *	All backslashes in given string are changed to slashes.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

char *
TclWinNoBackslash(
    char *path)			/* String to change. */
{
    char *p;

    for (p = path; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }
    return path;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCheckStackSpace --
 *
 *	Detect if we are about to blow the stack.  Called before an 
 *	evaluation can happen when nesting depth is checked.
 *
 * Results:
 *	1 if there is enough stack space to continue; 0 if not.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclpCheckStackSpace()
{
    /*
     * We can recurse only if there is at least TCL_WIN_STACK_THRESHOLD
     * bytes of stack space left.  alloca() is cheap on windows; basically
     * it just subtracts from the stack pointer causing the OS to throw an
     * exception if the stack pointer is set below the bottom of the stack.
     */

    __try {
	alloca(TCL_WIN_STACK_THRESHOLD);
	return 1;
    } __except (1) {}

    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * TclWinGetPlatform --
 *
 *	This is a kludge that allows the test library to get access
 *	the internal tclPlatform variable.
 *
 * Results:
 *	Returns a pointer to the tclPlatform variable.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclPlatformType *
TclWinGetPlatform()
{
    return &tclPlatform;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclWinSetInterfaces --
 *
 *	A helper proc that allows the test library to change the
 *	tclWinProcs structure to dispatch to either the wide-character
 *	or multi-byte versions of the operating system calls, depending
 *	on whether Unicode is the system encoding.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

void
TclWinSetInterfaces(
    int wide)			/* Non-zero to use wide interfaces, 0
				 * otherwise. */
{
    Tcl_FreeEncoding(tclWinTCharEncoding);

    if (wide) {
	tclWinProcs = &unicodeProcs;
	tclWinTCharEncoding = Tcl_GetEncoding(NULL, "unicode");
    } else {
	tclWinProcs = &asciiProcs;
	tclWinTCharEncoding = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_WinUtfToTChar, Tcl_WinTCharToUtf --
 *
 *	Convert between UTF-8 and Unicode when running Windows NT or 
 *	the current ANSI code page when running Windows 95.
 *
 *	On Mac, Unix, and Windows 95, all strings exchanged between Tcl
 *	and the OS are "char" oriented.  We need only one Tcl_Encoding to
 *	convert between UTF-8 and the system's native encoding.  We use
 *	NULL to represent that encoding.
 *
 *	On NT, some strings exchanged between Tcl and the OS are "char"
 *	oriented, while others are in Unicode.  We need two Tcl_Encoding
 *	APIs depending on whether we are targeting a "char" or Unicode
 *	interface.  
 *
 *	Calling Tcl_UtfToExternal() or Tcl_ExternalToUtf() with an
 *	encoding of NULL should always used to convert between UTF-8
 *	and the system's "char" oriented encoding.  The following two
 *	functions are used in Windows-specific code to convert between
 *	UTF-8 and Unicode strings (NT) or "char" strings(95).  This saves
 *	you the trouble of writing the following type of fragment over and
 *	over:
 *
 *		if (running NT) {
 *		    encoding <- Tcl_GetEncoding("unicode");
 *		    nativeBuffer <- UtfToExternal(encoding, utfBuffer);
 *		    Tcl_FreeEncoding(encoding);
 *		} else {
 *		    nativeBuffer <- UtfToExternal(NULL, utfBuffer);
 *		}
 *
 *	By convention, in Windows a TCHAR is a character in the ANSI code
 *	page on Windows 95, a Unicode character on Windows NT.  If you
 *	plan on targeting a Unicode interfaces when running on NT and a
 *	"char" oriented interface while running on 95, these functions
 *	should be used.  If you plan on targetting the same "char"
 *	oriented function on both 95 and NT, use Tcl_UtfToExternal()
 *	with an encoding of NULL.
 *
 * Results:
 *	The result is a pointer to the string in the desired target
 *	encoding.  Storage for the result string is allocated in
 *	dsPtr; the caller must call Tcl_DStringFree() when the result
 *	is no longer needed.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

TCHAR *
Tcl_WinUtfToTChar(string, len, dsPtr)
    const char *string;		/* Source string in UTF-8. */
    int len;			/* Source string length in bytes, or < 0 for
				 * strlen(). */
    Tcl_DString *dsPtr;		/* Uninitialized or free DString in which 
				 * the converted string is stored. */
{
    return (TCHAR *) Tcl_UtfToExternalDString(tclWinTCharEncoding, 
	    string, len, dsPtr);
}

char *
Tcl_WinTCharToUtf(string, len, dsPtr)
    const TCHAR *string;	/* Source string in Unicode when running
				 * NT, ANSI when running 95. */
    int len;			/* Source string length in bytes, or < 0 for
				 * platform-specific string length. */
    Tcl_DString *dsPtr;		/* Uninitialized or free DString in which 
				 * the converted string is stored. */
{
    return Tcl_ExternalToUtfDString(tclWinTCharEncoding, 
	    (const char *) string, len, dsPtr);
}
