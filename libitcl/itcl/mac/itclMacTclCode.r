/*
 *  itclMacTclCode.r
 *
 *  This file includes the Itcl code that is needed to startup Tcl.
 *  It is to be included either in the resource fork of the shared library, or in the
 *  resource fork of the application for a statically bound application.
 *
 *  Jim Ingham
 *  Lucent Technologies 1996
 *
 */
 
#include <Types.r>
#include <SysTypes.r>



#define ITCL_LIBRARY_RESOURCES 2500

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

read 'TEXT' (ITCL_LIBRARY_RESOURCES, "itcl", purgeable,preload) "::library:itcl.tcl";
