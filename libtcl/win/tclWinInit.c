/* 
 * tclWinInit.c --
 *
 *	Contains the Windows-specific interpreter initialization functions.
 *
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinInit.c 1.32 97/06/24 17:28:26
 */

#include "tclInt.h"
#include "tclPort.h"
#include <winreg.h>
#include <winnt.h>
#include <winbase.h>

/*
 * The following declaration is a workaround for some Microsoft brain damage.
 * The SYSTEM_INFO structure is different in various releases, even though the
 * layout is the same.  So we overlay our own structure on top of it so we
 * can access the interesting slots in a uniform way.
 */

typedef struct {
    WORD wProcessorArchitecture;
    WORD wReserved;
} OemId;

/*
 * The following macros are missing from some versions of winnt.h.
 */

#ifndef PROCESSOR_ARCHITECTURE_INTEL
#define PROCESSOR_ARCHITECTURE_INTEL 0
#endif
#ifndef PROCESSOR_ARCHITECTURE_MIPS
#define PROCESSOR_ARCHITECTURE_MIPS  1
#endif
#ifndef PROCESSOR_ARCHITECTURE_ALPHA
#define PROCESSOR_ARCHITECTURE_ALPHA 2
#endif
#ifndef PROCESSOR_ARCHITECTURE_PPC
#define PROCESSOR_ARCHITECTURE_PPC   3
#endif
#ifndef PROCESSOR_ARCHITECTURE_UNKNOWN
#define PROCESSOR_ARCHITECTURE_UNKNOWN 0xFFFF
#endif

/*
 * The following arrays contain the human readable strings for the Windows
 * platform and processor values.
 */


#define NUMPLATFORMS 3
static char* platforms[NUMPLATFORMS] = {
    "Win32s", "Windows 95", "Windows NT"
};

#define NUMPROCESSORS 4
static char* processors[NUMPROCESSORS] = {
    "intel", "mips", "alpha", "ppc"
};

/*
 * The following string is the startup script executed in new
 * interpreters.  It looks on disk in several different directories
 * for a script "init.tcl" that is compatible with this version
 * of Tcl.  The init.tcl script does all of the real work of
 * initialization.
 */

static char *initScript =
"proc init {} {\n\
    global tcl_library tcl_platform tcl_version tcl_patchLevel env errorInfo\n\
    global tcl_pkgPath\n\
    rename init {}\n\
    set errors {}\n\
    proc tcl_envTraceProc {lo n1 n2 op} {\n\
	global env\n\
	set x $env($n2)\n\
	set env($lo) $x\n\
	set env([string toupper $lo]) $x\n\
    }\n\
    foreach p [array names env] {\n\
	set u [string toupper $p]\n\
	if {$u != $p} {\n\
	    switch -- $u {\n\
		COMSPEC -\n\
		PATH {\n\
		    if {![info exists env($u)]} {\n\
			set env($u) $env($p)\n\
		    }\n\
		    trace variable env($p) w [list tcl_envTraceProc $p]\n\
		    trace variable env($u) w [list tcl_envTraceProc $p]\n\
		}\n\
	    }\n\
	}\n\
    }\n\
    if {![info exists env(COMSPEC)]} {\n\
	if {$tcl_platform(os) == {Windows NT}} {\n\
	    set env(COMSPEC) cmd.exe\n\
	} else {\n\
	    set env(COMSPEC) command.com\n\
	}\n\
    }	\n\
    set dirs {}\n\
    if {[info exists env(TCL_LIBRARY)]} {\n\
	lappend dirs $env(TCL_LIBRARY)\n\
    }\n\
    lappend dirs $tcl_library\n\
    lappend dirs [file join [file dirname [file dirname [info nameofexecutable]]] lib/tcl$tcl_version]\n\
    if [string match {*[ab]*} $tcl_patchLevel] {\n\
	set lib tcl$tcl_patchLevel\n\
    } else {\n\
	set lib tcl$tcl_version\n\
    }\n\
    lappend dirs [file join [file dirname [file dirname [pwd]]] $lib/library]\n\
    lappend dirs [file join [file dirname [pwd]] library]\n\
    foreach i $dirs {\n\
	set tcl_library $i\n\
	set tclfile [file join $i init.tcl]\n\
	if {[file exists $tclfile]} {\n\
            lappend tcl_pkgPath [file dirname $i]\n\
	    if ![catch {uplevel #0 [list source $tclfile]} msg] {\n\
	        return\n\
	    } else {\n\
		append errors \"$tclfile: $msg\n$errorInfo\n\"\n\
	    }\n\
	}\n\
    }\n\
    set msg \"Can't find a usable init.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\n\"\n\
    append msg \"$errors\n\n\"\n\
    append msg \"This probably means that Tcl wasn't installed properly.\n\"\n\
    error $msg\n\
}\n\
init\n";

/*
 *----------------------------------------------------------------------
 *
 * TclPlatformInit --
 *
 *	Performs Windows-specific interpreter initialization related to the
 *	tcl_library variable.  Also sets up the HOME environment variable
 *	if it is not already set.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets "tcl_library" and "env(HOME)" Tcl variables
 *
 *----------------------------------------------------------------------
 */

void
TclPlatformInit(interp)
    Tcl_Interp *interp;
{
    char *ptr;
    char buffer[13];
    Tcl_DString ds;
    OSVERSIONINFO osInfo;
    SYSTEM_INFO sysInfo;
    int isWin32s;		/* True if we are running under Win32s. */
    OemId *oemId;
    HKEY key;
    DWORD size;

    tclPlatform = TCL_PLATFORM_WINDOWS;

    Tcl_DStringInit(&ds);

    /*
     * Find out what kind of system we are running on.
     */

    osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osInfo);

    isWin32s = (osInfo.dwPlatformId == VER_PLATFORM_WIN32s);

    /*
     * Since Win32s doesn't support GetSystemInfo, we use a default value.
     */

    oemId = (OemId *) &sysInfo;
    if (!isWin32s) {
	GetSystemInfo(&sysInfo);
    } else {
	oemId->wProcessorArchitecture = PROCESSOR_ARCHITECTURE_INTEL;
    }

    /*
     * Initialize the tcl_library variable from the registry.
     */

    if (!isWin32s) {
	if ((RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"Software\\Sun\\Tcl\\" TCL_VERSION, 0, KEY_READ, &key)
		== ERROR_SUCCESS)
		&& (RegQueryValueEx(key, "Root", NULL, NULL, NULL, &size)
		    == ERROR_SUCCESS)) {
	    Tcl_DStringSetLength(&ds, size);
	    RegQueryValueEx(key, "Root", NULL, NULL,
		    (LPBYTE)Tcl_DStringValue(&ds), &size);
	}
    } else {
	if ((RegOpenKeyEx(HKEY_CLASSES_ROOT,
		"Software\\Sun\\Tcl\\" TCL_VERSION, 0, KEY_READ, &key)
		== ERROR_SUCCESS)
		&& (RegQueryValueEx(key, "", NULL, NULL, NULL, &size)
		    == ERROR_SUCCESS)) {
	    Tcl_DStringSetLength(&ds, size);
	    RegQueryValueEx(key, "", NULL, NULL,
		    (LPBYTE) Tcl_DStringValue(&ds), &size);
	}
    }
    Tcl_SetVar(interp, "tcl_library", Tcl_DStringValue(&ds), TCL_GLOBAL_ONLY);
    if (Tcl_DStringLength(&ds) > 0) {
	char *argv[3];
	argv[0] = Tcl_GetVar(interp, "tcl_library", TCL_GLOBAL_ONLY);
	argv[1] = "lib";
	argv[2] = NULL;
	Tcl_DStringSetLength(&ds, 0);
	Tcl_SetVar(interp, "tcl_pkgPath", Tcl_JoinPath(2, argv, &ds),
		TCL_GLOBAL_ONLY|TCL_LIST_ELEMENT);
	argv[1] = "lib/tcl" TCL_VERSION;
	Tcl_DStringSetLength(&ds, 0);
	Tcl_SetVar(interp, "tcl_library", Tcl_JoinPath(2, argv, &ds), 
		TCL_GLOBAL_ONLY);
    }

    /*
     * Define the tcl_platform array.
     */

    Tcl_SetVar2(interp, "tcl_platform", "platform", "windows",
	    TCL_GLOBAL_ONLY);
    if (osInfo.dwPlatformId < NUMPLATFORMS) {
	Tcl_SetVar2(interp, "tcl_platform", "os",
		platforms[osInfo.dwPlatformId], TCL_GLOBAL_ONLY);
    }
    sprintf(buffer, "%d.%d", osInfo.dwMajorVersion, osInfo.dwMinorVersion);
    Tcl_SetVar2(interp, "tcl_platform", "osVersion", buffer, TCL_GLOBAL_ONLY);
    if (oemId->wProcessorArchitecture < NUMPROCESSORS) {
	Tcl_SetVar2(interp, "tcl_platform", "machine",
		processors[oemId->wProcessorArchitecture],
		TCL_GLOBAL_ONLY);
    }

    /*
     * Set up the HOME environment variable from the HOMEDRIVE & HOMEPATH
     * environment variables, if necessary.
     */

    ptr = Tcl_GetVar2(interp, "env", "HOME", TCL_GLOBAL_ONLY);
    if (ptr == NULL) {
	Tcl_DStringSetLength(&ds, 0);
	ptr = Tcl_GetVar2(interp, "env", "HOMEDRIVE", TCL_GLOBAL_ONLY);
	if (ptr != NULL) {
	    Tcl_DStringAppend(&ds, ptr, -1);
	}
	ptr = Tcl_GetVar2(interp, "env", "HOMEPATH", TCL_GLOBAL_ONLY);
	if (ptr != NULL) {
	    Tcl_DStringAppend(&ds, ptr, -1);
	}
	if (Tcl_DStringLength(&ds) > 0) {
	    Tcl_SetVar2(interp, "env", "HOME", Tcl_DStringValue(&ds),
		    TCL_GLOBAL_ONLY);
	} else {
	    Tcl_SetVar2(interp, "env", "HOME", "c:\\", TCL_GLOBAL_ONLY);
	}
    }

    Tcl_DStringFree(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Init --
 *
 *	This procedure is typically invoked by Tcl_AppInit procedures
 *	to perform additional initialization for a Tcl interpreter,
 *	such as sourcing the "init.tcl" script.
 *
 * Results:
 *	Returns a standard Tcl completion code and sets interp->result
 *	if there is an error.
 *
 * Side effects:
 *	Depends on what's in the init.tcl script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Init(interp)
    Tcl_Interp *interp;		/* Interpreter to initialize. */
{
    return Tcl_Eval(interp, initScript);

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
 *----------------------------------------------------------------------
 *
 * Tcl_SourceRCFile --
 *
 *	This procedure is typically invoked by Tcl_Main of Tk_Main
 *	procedure to source an application specific rc file into the
 *	interpreter at startup time.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what's in the rc script.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SourceRCFile(interp)
    Tcl_Interp *interp;		/* Interpreter to source rc file into. */
{
    Tcl_DString temp;
    char *fileName;
    Tcl_Channel errChannel;

    fileName = Tcl_GetVar(interp, "tcl_rcFileName", TCL_GLOBAL_ONLY);

    if (fileName != NULL) {
        Tcl_Channel c;
	char *fullName;

        Tcl_DStringInit(&temp);
	fullName = Tcl_TranslateFileName(interp, fileName, &temp);
	if (fullName == NULL) {
	    /*
	     * Couldn't translate the file name (e.g. it referred to a
	     * bogus user or there was no HOME environment variable).
	     * Just do nothing.
	     */
	} else {

	    /*
	     * Test for the existence of the rc file before trying to read it.
	     */

            c = Tcl_OpenFileChannel(NULL, fullName, "r", 0);
            if (c != (Tcl_Channel) NULL) {
                Tcl_Close(NULL, c);
		if (Tcl_EvalFile(interp, fullName) != TCL_OK) {
		    errChannel = Tcl_GetStdChannel(TCL_STDERR);
		    if (errChannel) {
			Tcl_Write(errChannel, interp->result, -1);
			Tcl_Write(errChannel, "\n", 1);
		    }
		}
	    }
	}
        Tcl_DStringFree(&temp);
    }
}
