/* 
 * tclMacLibrary.r --
 *
 *	This file creates resources used by the Tcl shared library.
 *	Many thanks go to "Jay Lieske, Jr." <lieske@princeton.edu> who
 *	wrote the initial version of this file.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacLibrary.r 1.3 96/09/12 17:40:07
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


/*
 * Currently the creator for all Tcl/Tk libraries and extensions
 * should be 'TclL'.  This will allow those extension and libraries
 * to use the common icon for Tcl extensions.  However, this signature
 * still needs to be approved by the signature police at Apple and may
 * change.
 */
#define ITCL_CREATOR 'ITcL'
#define TCL_LIBRARY_RESOURCES 2000
#define ITCL_LIBRARY_RESOURCES 2000

/*
 * The 'BNDL' resource is the primary link between a file's
 * creator/type and its icon.  This resource acts for all Tcl shared
 * libraries; other libraries will not need one and ought to use
 * custom icons rather than new file types for a different appearance.
 */

resource 'BNDL' (TCL_LIBRARY_RESOURCES, "Tcl bundle", purgeable) 
{
	ITCL_CREATOR,
	0,
	{	/* array TypeArray: 2 elements */
		/* [1] */
		'FREF',
		{	/* array IDArray: 1 elements */
			/* [1] */
			0, TCL_LIBRARY_RESOURCES
		},
		/* [2] */
		'ICN#',
		{	/* array IDArray: 1 elements */
			/* [1] */
			0, TCL_LIBRARY_RESOURCES
		}
	}
};

resource 'FREF' (TCL_LIBRARY_RESOURCES, purgeable) 
{
	'shlb', 0, ""
};

type ITCL_CREATOR as 'STR ';
resource ITCL_CREATOR (0, purgeable) {
	"Itcl Library " ITCL_PATCH_LEVEL " © 1993-1998"
};

/*
 * The 'kind' resource works with a 'BNDL' in Macintosh Easy Open
 * to affect the text the Finder displays in the "kind" column and
 * file info dialog.  This information will be applied to all files
 * with the listed creator and type.
 */

resource 'kind' (TCL_LIBRARY_RESOURCES, "Itcl kind", purgeable) {
	ITCL_CREATOR,
	0, /* region = USA */
	{
		'shlb', "Itcl Library"
	}
};


/*
 * The -16397 string will be displayed by Finder when a user
 * tries to open the shared library. The string should
 * give the user a little detail about the library's capabilities
 * and enough information to install the library in the correct location.  
 * A similar string should be placed in all shared libraries.
 */
resource 'STR ' (-16397, purgeable) {
	"Itcl Library\n\n"
	"This is one of the libraries needed to run the Itcl flavor of the Tool Command Language programs. "
	"To work properly, it should be placed in the ŒTool Command Language¹ folder "
	"within the Extensions folder."
};

/* 
 * The mechanisim below loads Tcl source into the resource fork of the
 * application.  The example below creates a TEXT resource named
 * "Init" from the file "init.tcl".  This allows applications to use
 * Tcl to define the behavior of the application without having to
 * require some predetermined file structure - all needed Tcl "files"
 * are located within the application.  To source a file for the
 * resource fork the source command has been modified to support
 * sourcing from resources.  In the below case "source -rsrc {Init}"
 * will load the TEXT resource named "Init".
 */

#include "itclMacTclCode.r"

data 'TEXT' (ITCL_LIBRARY_RESOURCES+1,"pkgIndex",purgeable, preload) {
	"# Tcl package index file, version 1.0\n"
	"package ifneeded Itcl 3.1 [list load [file join $dir itcl31[info sharedlibextension]] Itcl]\n"
};


