/*
 * tclWinInt.h --
 *
 *	Declarations of Windows-specific shared variables and procedures.
 *
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TCLWININT
#define _TCLWININT

#include "tclInt.h"

/*
 * The following specifies how much stack space TclpCheckStackSpace()
 * ensures is available.  TclpCheckStackSpace() is called by Tcl_EvalObj()
 * to help avoid overflowing the stack in the case of infinite recursion.
 */

#define TCL_WIN_STACK_THRESHOLD 0x8000

/*
 * Some versions of Borland C have a define for the OSVERSIONINFO for
 * Win32s and for NT, but not for Windows 95.
 * Define VER_PLATFORM_WIN32_CE for those without newer headers.
 */

#ifndef VER_PLATFORM_WIN32_WINDOWS
#define VER_PLATFORM_WIN32_WINDOWS 1
#endif
#ifndef VER_PLATFORM_WIN32_CE
#define VER_PLATFORM_WIN32_CE 3
#endif

/*
 * The following structure keeps track of whether we are using the
 * multi-byte or the wide-character interfaces to the operating system.
 * System calls should be made through the following function table.
 */

typedef union {
    WIN32_FIND_DATAA a;
    WIN32_FIND_DATAW w;
} WIN32_FIND_DATAT;

typedef struct TclWinProcs {
    int useWide;

    BOOL (WINAPI *buildCommDCBProc)(const TCHAR *, LPDCB);
    TCHAR *(WINAPI *charLowerProc)(TCHAR *);
    BOOL (WINAPI *copyFileProc)(const TCHAR *, const TCHAR *, BOOL);
    BOOL (WINAPI *createDirectoryProc)(const TCHAR *, LPSECURITY_ATTRIBUTES);
    HANDLE (WINAPI *createFileProc)(const TCHAR *, DWORD, DWORD,
	    LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    BOOL (WINAPI *createProcessProc)(const TCHAR *, TCHAR *,
	    LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
	    LPVOID, const TCHAR *, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
    BOOL (WINAPI *deleteFileProc)(const TCHAR *);
    HANDLE (WINAPI *findFirstFileProc)(const TCHAR *, WIN32_FIND_DATAT *);
    BOOL (WINAPI *findNextFileProc)(HANDLE, WIN32_FIND_DATAT *);
    BOOL (WINAPI *getComputerNameProc)(WCHAR *, LPDWORD);
    DWORD (WINAPI *getCurrentDirectoryProc)(DWORD, WCHAR *);
    DWORD (WINAPI *getFileAttributesProc)(const TCHAR *);
    DWORD (WINAPI *getFullPathNameProc)(const TCHAR *, DWORD nBufferLength,
	    WCHAR *, TCHAR **);
    DWORD (WINAPI *getModuleFileNameProc)(HMODULE, WCHAR *, int);
    DWORD (WINAPI *getShortPathNameProc)(const TCHAR *, WCHAR *, DWORD);
    UINT (WINAPI *getTempFileNameProc)(const TCHAR *, const TCHAR *, UINT,
	    WCHAR *);
    DWORD (WINAPI *getTempPathProc)(DWORD, WCHAR *);
    BOOL (WINAPI *getVolumeInformationProc)(const TCHAR *, WCHAR *, DWORD,
	    LPDWORD, LPDWORD, LPDWORD, WCHAR *, DWORD);
    HINSTANCE (WINAPI *loadLibraryProc)(const TCHAR *);
    TCHAR (WINAPI *lstrcpyProc)(WCHAR *, const TCHAR *);
    BOOL (WINAPI *moveFileProc)(const TCHAR *, const TCHAR *);
    BOOL (WINAPI *removeDirectoryProc)(const TCHAR *);
    DWORD (WINAPI *searchPathProc)(const TCHAR *, const TCHAR *,
	    const TCHAR *, DWORD, WCHAR *, TCHAR **);
    BOOL (WINAPI *setCurrentDirectoryProc)(const TCHAR *);
    BOOL (WINAPI *setFileAttributesProc)(const TCHAR *, DWORD);
    /*
     * These two function pointers will only be set when
     * Tcl_FindExecutable is called.  If you don't ever call that
     * function, the application will crash whenever WinTcl tries to call
     * functions through these null pointers.  That is not a bug in Tcl
     * -- Tcl_FindExecutable is obligatory in recent Tcl releases.
     */
    BOOL (WINAPI *getFileAttributesExProc)(const TCHAR *,
	    GET_FILEEX_INFO_LEVELS, LPVOID);
    BOOL (WINAPI *createHardLinkProc)(const TCHAR*, const TCHAR*,
				      LPSECURITY_ATTRIBUTES);

    /* deleted INT (__cdecl *utimeProc)(const TCHAR*, struct _utimbuf *); */
    /* These two are also NULL at start; see comment above */
    HANDLE (WINAPI *findFirstFileExProc)(const TCHAR*, UINT,
					 LPVOID, UINT,
					 LPVOID, DWORD);
    BOOL (WINAPI *getVolumeNameForVMPProc)(const TCHAR*, TCHAR*, DWORD);
    DWORD (WINAPI *getLongPathNameProc)(const TCHAR*, TCHAR*, DWORD);
    /*
     * These six are for the security sdk to get correct file
     * permissions on NT, 2000, XP, etc.  On 95,98,ME they are
     * always null.
     */
    BOOL (WINAPI *getFileSecurityProc)(LPCTSTR lpFileName,
		     SECURITY_INFORMATION RequestedInformation,
		     PSECURITY_DESCRIPTOR pSecurityDescriptor,
		     DWORD nLength,
		     LPDWORD lpnLengthNeeded);
    BOOL (WINAPI *impersonateSelfProc) (SECURITY_IMPERSONATION_LEVEL
		      ImpersonationLevel);
    BOOL (WINAPI *openThreadTokenProc) (HANDLE ThreadHandle,
		      DWORD DesiredAccess, BOOL OpenAsSelf,
		      PHANDLE TokenHandle);
    BOOL (WINAPI *revertToSelfProc) (void);
    void (WINAPI *mapGenericMaskProc) (PDWORD AccessMask,
		      PGENERIC_MAPPING GenericMapping);
    BOOL (WINAPI *accessCheckProc)(PSECURITY_DESCRIPTOR pSecurityDescriptor,
		    HANDLE ClientToken, DWORD DesiredAccess,
		    PGENERIC_MAPPING GenericMapping,
		    PPRIVILEGE_SET PrivilegeSet,
		    LPDWORD PrivilegeSetLength,
		    LPDWORD GrantedAccess,
		    LPBOOL AccessStatus);
    /*
     * Unicode console support. WriteConsole and ReadConsole
     */
    BOOL (WINAPI *readConsoleProc)(
      HANDLE hConsoleInput,
      LPVOID lpBuffer,
      DWORD nNumberOfCharsToRead,
      LPDWORD lpNumberOfCharsRead,
      LPVOID lpReserved
    );
    BOOL (WINAPI *writeConsoleProc)(
      HANDLE hConsoleOutput,
      const void* lpBuffer,
      DWORD nNumberOfCharsToWrite,
      LPDWORD lpNumberOfCharsWritten,
      LPVOID lpReserved
    );
    BOOL (WINAPI *getUserName)(LPTSTR lpBuffer, LPDWORD lpnSize);
} TclWinProcs;

MODULE_SCOPE TclWinProcs *tclWinProcs;

/*
 * Declarations of functions that are not accessible by way of the
 * stubs table.
 */

MODULE_SCOPE char	TclWinDriveLetterForVolMountPoint(
			    const WCHAR *mountPoint);
MODULE_SCOPE void	TclWinEncodingsCleanup();
MODULE_SCOPE void	TclWinInit(HINSTANCE hInst);
MODULE_SCOPE TclFile	TclWinMakeFile(HANDLE handle);
MODULE_SCOPE Tcl_Channel TclWinOpenConsoleChannel(HANDLE handle,
			    char *channelName, int permissions);
MODULE_SCOPE Tcl_Channel TclWinOpenFileChannel(HANDLE handle, char *channelName,
			    int permissions, int appendMode);
MODULE_SCOPE Tcl_Channel TclWinOpenSerialChannel(HANDLE handle,
			    char *channelName, int permissions);
MODULE_SCOPE HANDLE	TclWinSerialReopen(HANDLE handle, const TCHAR *name,
			    DWORD access);
MODULE_SCOPE int	TclWinSymLinkCopyDirectory(const TCHAR* LinkOriginal,
			    const TCHAR* LinkCopy);
MODULE_SCOPE int	TclWinSymLinkDelete(const TCHAR* LinkOriginal,
			    int linkOnly);
#if defined(TCL_THREADS) && defined(USE_THREAD_ALLOC)
MODULE_SCOPE void	TclWinFreeAllocCache(void);
MODULE_SCOPE void	TclFreeAllocCache(void *);
MODULE_SCOPE Tcl_Mutex *TclpNewAllocMutex(void);
MODULE_SCOPE void *	TclpGetAllocCache(void);
MODULE_SCOPE void	TclpSetAllocCache(void *);
#endif /* TCL_THREADS */

/* Needed by tclWinFile.c and tclWinFCmd.c */
#ifndef FILE_ATTRIBUTE_REPARSE_POINT
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400
#endif

#endif	/* _TCLWININT */
