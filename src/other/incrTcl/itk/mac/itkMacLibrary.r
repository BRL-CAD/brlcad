/*
 * tkMacLibrary.r --
 *
 *	This file creates resources for use in most Tk applications.
 *	This is designed to be an example of using the Tcl/Tk 
 *	libraries in a Macintosh Application.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacLibrary.r 1.5 96/10/03 17:54:21
 */

#include <Types.r>
#include <SysTypes.r>
#include <AEUserTermTypes.r>

/*
 * The folowing include and defines help construct
 * the version string for Tcl.
 */

#define RESOURCE_INCLUDED
#include <tcl.h>
#include <tk.h>
#include "itcl.h"
#include "itk.h"

#if (TK_RELEASE_LEVEL == 0)
#   define RELEASE_LEVEL alpha
#elif (TK_RELEASE_LEVEL == 1)
#   define RELEASE_LEVEL beta
#elif (TK_RELEASE_LEVEL == 2)
#   define RELEASE_LEVEL final
#endif

#if (TK_RELEASE_LEVEL == 2)
#   define MINOR_VERSION (ITCL_MINOR_VERSION * 16) + TK_RELEASE_SERIAL
#else
#   define MINOR_VERSION ITCL_MINOR_VERSION * 16
#endif

#define RELEASE_CODE 0x00

#define ITCL_LIBRARY_RESOURCES 3000
#define ITK_LIBRARY_RESOURCES 3500

resource 'vers' (1) {
	ITCL_MAJOR_VERSION, MINOR_VERSION,
	RELEASE_LEVEL, 0x00, verUS,
	ITK_PATCH_LEVEL,
	ITK_PATCH_LEVEL ", by Michael McLennan © 1993-1998" "\n" "Lucent Technologies, Inc."
};

resource 'vers' (2) {
	ITCL_MAJOR_VERSION, MINOR_VERSION,
	RELEASE_LEVEL, 0x00, verUS,
	ITK_PATCH_LEVEL,
	"ItkWish " ITK_PATCH_LEVEL " © 1993-1998"
};


#define ITCL_LIBRARY_RESOURCES 2000
#define ITK_LIBRARY_RESOURCES 3500
/*
 * The -16397 string will be displayed by Finder when a user
 * tries to open the shared library. The string should
 * give the user a little detail about the library's capabilities
 * and enough information to install the library in the correct location.  
 * A similar string should be placed in all shared libraries.
 */
resource 'STR ' (-16397, purgeable) {
	"Itk Library\n\n"
	"This is the library needed to run add Itcl to the Tcl/Tk shell. "
	"To work properly, it should be placed in the ŒTool Command Language¹ folder "
	"within the Extensions folder."
};


/* 
 * We now load the Tk library into the resource fork of the library.
 */

#include "itkMacTclCode.r"

read 'TEXT' (ITK_LIBRARY_RESOURCES+12, "itk:tclIndex", purgeable) 
	"::mac:tclIndex";
data 'TEXT' (ITK_LIBRARY_RESOURCES+13,"pkgIndex",purgeable, preload) {
	"# Tcl package index file, version 1.0\n"
	"package ifneeded Itk 3.1 [list package require Itcl 3.1 \; load [file join $dir itk31[info sharedlibextension]] Itk \; source -rsrc itk:tclIndex]\n"
};

