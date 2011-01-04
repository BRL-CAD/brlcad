#include <Types.r>
#include <SysTypes.r>

#include "version.h"

resource 'vers' (1) {
	TBL_MAJOR_VERSION, TBL_MINOR_VERSION,
	final, 0x00, verUS,
	PACKAGE_VERSION,
	"tkTable " PACKAGE_VERSION " by Jeffrey Hobbs\n"
	"Macintosh Port by Chuck Houpt"
};

resource 'vers' (2) {
	TBL_MAJOR_VERSION, TBL_MINOR_VERSION,
	final, 0x00, verUS,
	PACKAGE_VERSION,
	"tkTable " PACKAGE_VERSION " © 1997-2008"
};

/*
 * The -16397 string will be displayed by Finder when a user
 * tries to open the shared library. The string should
 * give the user a little detail about the library's capabilities
 * and enough information to install the library in the correct location.  
 * A similar string should be placed in all shared libraries.
 */
resource 'STR ' (-16397, purgeable) {
	"tkTable Library\n\n"
	"This library provides the ability to create tables "
	"from Tcl/Tk programs.  To work properly, it "
	"should be placed in the ŒTool Command Language¹ folder "
	"within the Extensions folder."
};

read 'TEXT' (3000, "tkTable", purgeable, preload) "tkTable.tcl";

/* 
 * We now load the Tk library into the resource fork of the library.
 */

data 'TEXT' (4000, "pkgIndex", purgeable, preload) {
	"if {[catch {package require Tcl 8.2}]} return\n"
	"package ifneeded Tktable " PACKAGE_VERSION " "
	"\"package require Tk 8.2; "
	"[list load [file join $dir Tktable[info sharedlibextension]] Tktable]\""
};
