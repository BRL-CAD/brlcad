/* 
 * tclMacApplication.r --
 *
 *	This file creates resources for use Tcl Shell application.
 *	It should be viewed as an example of how to create a new
 *	Tcl application using the shared Tcl libraries.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacApplication.r 1.1 96/09/11 21:12:54
 */

#include <Types.r>
#include <SysTypes.r>

/*
 * The folowing include and defines help construct
 * the version string for Tcl.
 */

#define RESOURCE_INCLUDED
#include "tcl.h"
#include "itcl.h"
#include "itclPatch.h"

/* Should really have one of these in itcl too, but for now... */

#if (TCL_RELEASE_LEVEL == 0)
#   define RELEASE_LEVEL alpha
#elif (TCL_RELEASE_LEVEL == 1)
#   define RELEASE_LEVEL beta
#elif (TCL_RELEASE_LEVEL == 2)
#   define RELEASE_LEVEL final
#endif

#if (TCL_RELEASE_LEVEL == 2)
#   define MINOR_VERSION (ITCL_MINOR_VERSION * 16) + TCL_RELEASE_SERIAL
#else
#   define MINOR_VERSION ITCL_MINOR_VERSION * 16
#endif

resource 'vers' (1) {
	ITCL_MAJOR_VERSION, MINOR_VERSION,
	RELEASE_LEVEL, 0x00, verUS,
	ITCL_PATCH_LEVEL,
	ITCL_PATCH_LEVEL ", by Michael McLennan © Lucent Technologies, Inc."
};

resource 'vers' (2) {
	ITCL_MAJOR_VERSION, MINOR_VERSION,
	RELEASE_LEVEL, 0x00, verUS,
	ITCL_PATCH_LEVEL,
	"Itcl Shell " ITCL_PATCH_LEVEL " © 1993-1998"
};

#define ITCL_APP_CREATOR 'ITcL'

type ITCL_APP_CREATOR as 'STR ';
resource ITCL_APP_CREATOR (0, purgeable) {
	"Itcl Shell " ITCL_PATCH_LEVEL " © 1993-1998"
};

/*
 * The 'kind' resource works with a 'BNDL' in Macintosh Easy Open
 * to affect the text the Finder displays in the "kind" column and
 * file info dialog.  This information will be applied to all files
 * with the listed creator and type.
 */

resource 'kind' (128, "Itcl kind", purgeable) {
	ITCL_APP_CREATOR,
	0, /* region = USA */
	{
		'APPL', "Itcl Shell",
	}
};

/*
 * The following resource is used when creating the 'env' variable in
 * the Macintosh environment.  The creation mechanisim looks for the
 * 'STR#' resource named "Tcl Environment Variables" rather than a
 * specific resource number.  (In other words, feel free to change the
 * resource id if it conflicts with your application.)  Each string in
 * the resource must be of the form "KEYWORD=SOME STRING".  See Tcl
 * documentation for futher information about the env variable.
 *
 * A good example of something you may want to set is: "TCL_LIBRARY=My
 * disk:etc."
 */
 
resource 'STR#' (128, "Tcl Environment Variables") {
	{	"SCHEDULE_NAME=Agent Controller Schedule",
		"SCHEDULE_PATH=Lozoya:System Folder:Tcl Lib:Tcl-Scheduler"
	};
};

