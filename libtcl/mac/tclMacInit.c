/*
 * tclMacInit.c --
 *
 *	Contains the Mac-specific interpreter initialization functions.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacInit.c 1.39 97/09/23 13:17:30
 */

#include <Files.h>
#include <Gestalt.h>
#include <TextUtils.h>
#include <Resources.h>
#include <Strings.h>
#include "tclInt.h"
#include "tclMacInt.h"

/*
 *----------------------------------------------------------------------
 *
 * TclPlatformInit --
 *
 *	Performs Mac-specific interpreter initialization related to the
 *      tcl_platform and tcl_library variables.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets "tcl_library" & "tcl_platfrom" Tcl variable
 *
 *----------------------------------------------------------------------
 */

void
TclPlatformInit(
    Tcl_Interp *interp)		/* Tcl interpreter to initialize. */
{
    char *libDir;
    Tcl_DString path, libPath;
    long int gestaltResult;
    int minor, major;
    char versStr[10];

    /*
     * Set runtime C variable that tells cross platform C functions
     * what platform they are running on.  This can change at
     * runtime for testing purposes.
     */
    tclPlatform = TCL_PLATFORM_MAC;
    
    /*
     * Define the tcl_platfrom variable.
     */
    Tcl_SetVar2(interp, "tcl_platform", "platform", "macintosh",
	    TCL_GLOBAL_ONLY);
    Tcl_SetVar2(interp, "tcl_platform", "os", "MacOS", TCL_GLOBAL_ONLY);
    Gestalt(gestaltSystemVersion, &gestaltResult);
    major = (gestaltResult & 0x0000FF00) >> 8;
    minor = (gestaltResult & 0x000000F0) >> 4;
    sprintf(versStr, "%d.%d", major, minor);
    Tcl_SetVar2(interp, "tcl_platform", "osVersion", versStr, TCL_GLOBAL_ONLY);
#if GENERATINGPOWERPC
    Tcl_SetVar2(interp, "tcl_platform", "machine", "ppc", TCL_GLOBAL_ONLY);
#else
    Tcl_SetVar2(interp, "tcl_platform", "machine", "68k", TCL_GLOBAL_ONLY);
#endif

    /*
     * The tcl_library path can be found in one of two places.  As an element
     * in the env array.  Or the default which is to a folder in side the
     * Extensions folder of your system.
     */
     
    Tcl_DStringInit(&path);
    libDir = Tcl_GetVar2(interp, "env", "TCL_LIBRARY", TCL_GLOBAL_ONLY);
    if (libDir != NULL) {
	Tcl_SetVar(interp, "tcl_library", libDir, TCL_GLOBAL_ONLY);
    } else {
	libDir = Tcl_GetVar2(interp, "env", "EXT_FOLDER", TCL_GLOBAL_ONLY);
	if (libDir != NULL) {
	    Tcl_JoinPath(1, &libDir, &path);
	    
	    Tcl_DStringInit(&libPath);
	    Tcl_DStringAppend(&libPath, ":Tool Command Language:tcl", -1);
	    Tcl_DStringAppend(&libPath, TCL_VERSION, -1);
	    Tcl_JoinPath(1, &libPath.string, &path);
	    Tcl_DStringFree(&libPath);
	    Tcl_SetVar(interp, "tcl_library", path.string, TCL_GLOBAL_ONLY);
	} else {
	    Tcl_SetVar(interp, "tcl_library", "no library", TCL_GLOBAL_ONLY);
	}
    }
    
    /*
     * Now create the tcl_pkgPath variable.
     */
    Tcl_DStringSetLength(&path, 0);
    libDir = Tcl_GetVar2(interp, "env", "EXT_FOLDER", TCL_GLOBAL_ONLY);
    if (libDir != NULL) {
	Tcl_JoinPath(1, &libDir, &path);
	libDir = ":Tool Command Language:";
	Tcl_JoinPath(1, &libDir, &path);
	Tcl_SetVar(interp, "tcl_pkgPath", path.string,
		TCL_GLOBAL_ONLY|TCL_LIST_ELEMENT);
    } else {
	Tcl_SetVar(interp, "tcl_pkgPath", "no extension folder",
		TCL_GLOBAL_ONLY|TCL_LIST_ELEMENT);
    }
    Tcl_DStringFree(&path);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCheckStackSpace --
 *
 *	On a 68K Mac, we can detect if we are about to blow the stack.
 *	Called before an evaluation can happen when nesting depth is
 *	checked.
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
    return StackSpace() > TCL_MAC_STACK_THRESHOLD;
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
Tcl_Init(
    Tcl_Interp *interp)		/* Interpreter to initialize. */
{
    static char initCmd[] =
	"if {[catch {source -rsrc Init}] != 0} {\n\
	if [file exists [info library]:init.tcl] {\n\
	    source [info library]:init.tcl\n\
	} else {\n\
	    set msg \"can't find Init resource or [info library]:init.tcl;\"\n\
	    append msg \" perhaps you need to\\ninstall Tcl or set your \"\n\
	    append msg \"TCL_LIBRARY environment variable?\"\n\
	    error $msg\n\
	}\n}\n\
        if {[catch {source -rsrc History}] != 0} {\n\
	if [file exists [info library]:history.tcl] {\n\
	    source [info library]:history.tcl\n\
	} else {\n\
	    set msg \"can't find History resource or [info library]:history.tcl;\"\n\
	    append msg \" perhaps you need to\\ninstall Tcl or set your \"\n\
	    append msg \"TCL_LIBRARY environment variable?\"\n\
	    error $msg\n\
	}\n}\n\
        if {[catch {source -rsrc Word}] != 0} {\n\
	if [file exists [info library]:word.tcl] {\n\
	    source [info library]:word.tcl\n\
	} else {\n\
	    set msg \"can't find Word resource or [info library]:word.tcl;\"\n\
	    append msg \" perhaps you need to\\ninstall Tcl or set your \"\n\
	    append msg \"TCL_LIBRARY environment variable?\"\n\
	    error $msg\n\
	}\n}";

    /*
     * For Macintosh applications the Init function may be contained in
     * the application resources.  If it exists we use it - otherwise we
     * look in the tcl_library directory.  Ditto for the history command.
     */
    
    return Tcl_Eval(interp, initCmd);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SourceRCFile --
 *
 *	This procedure is typically invoked by Tcl_Main or Tk_Main
 *	procedure to source an application specific rc file into the
 *	interpreter at startup time.  This will either source a file
 *	in the "tcl_rcFileName" variable or a TEXT resource in the
 *	"tcl_rcRsrcName" variable.
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
Tcl_SourceRCFile(
    Tcl_Interp *interp)		/* Interpreter to source rc file into. */
{
    Tcl_DString temp;
    char *fileName;
    Tcl_Channel errChannel;
    Handle h;

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

    fileName = Tcl_GetVar(interp, "tcl_rcRsrcName", TCL_GLOBAL_ONLY);

    if (fileName != NULL) {
	c2pstr(fileName);
	h = GetNamedResource('TEXT', (StringPtr) fileName);
	p2cstr((StringPtr) fileName);
	if (h != NULL) {
	    if (Tcl_MacEvalResource(interp, fileName, 0, NULL) != TCL_OK) {
		errChannel = Tcl_GetStdChannel(TCL_STDERR);
		if (errChannel) {
		    Tcl_Write(errChannel, interp->result, -1);
		    Tcl_Write(errChannel, "\n", 1);
		}
	    }
	    Tcl_ResetResult(interp);
	    ReleaseResource(h);
	}
    }
}
