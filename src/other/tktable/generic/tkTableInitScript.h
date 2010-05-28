/* 
 * tkTableInitScript.h --
 *
 *	This file contains common init script for tkTable
 *
 * Copyright (c) 1998 Jeffrey Hobbs
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * The following string is the startup script executed when the table is
 * loaded.  It looks on disk in several different directories for a script
 * "TBL_RUNTIME" (as defined in Makefile) that is compatible with this
 * version of tkTable.  The sourced script has all key bindings defined.
 */

static char tkTableInitScript[] = "if {[info proc tkTableInit]==\"\"} {\n\
  proc tkTableInit {} {\n\
    global tk_library tcl_pkgPath errorInfo env\n\
    rename tkTableInit {}\n\
    set errors {}\n\
    if {![info exists env(TK_TABLE_LIBRARY_FILE)]} {\n\
	set env(TK_TABLE_LIBRARY_FILE) " TBL_RUNTIME "\n\
    }\n\
    if {[info exists env(TK_TABLE_LIBRARY)]} {\n\
	lappend dirs $env(TK_TABLE_LIBRARY)\n\
    }\n\
    lappend dirs " TBL_RUNTIME_DIR "\n\
    if {[info exists tcl_pkgPath]} {\n\
	foreach i $tcl_pkgPath {\n\
	    lappend dirs [file join $i Tktable" PACKAGE_VERSION "] \\\n\
		[file join $i Tktable] $i\n\
	}\n\
    }\n\
    lappend dirs $tk_library [pwd]\n\
    foreach i $dirs {\n\
	set try [file join $i $env(TK_TABLE_LIBRARY_FILE)]\n\
	if {[file exists $try]} {\n\
	    if {![catch {uplevel #0 [list source $try]} msg]} {\n\
		set env(TK_TABLE_LIBRARY) $i\n\
		return\n\
	    } else {\n\
		append errors \"$try: $msg\n$errorInfo\n\"\n\
	    }\n\
	}\n\
    }\n"
#ifdef NO_EMBEDDED_RUNTIME
"    set msg \"Can't find a $env(TK_TABLE_LIBRARY_FILE) in the following directories: \n\"\n\
    append msg \"    $dirs\n\n$errors\n\n\"\n\
    append msg \"This probably means that TkTable wasn't installed properly.\"\n\
    return -code error $msg\n"
#else
"    set env(TK_TABLE_LIBRARY) EMBEDDED_RUNTIME\n"
#   ifdef MAC_TCL
"    source -rsrc tkTable"
#   else
"    uplevel #0 {"
#	include "tkTable.tcl.h"
"    }"
#   endif
#endif
"  }\n\
}\n\
tkTableInit";

/*
 * The init script can't make certain calls in a safe interpreter,
 * so we always have to use the embedded runtime for it
 */
static char tkTableSafeInitScript[] = "if {[info proc tkTableInit]==\"\"} {\n\
  proc tkTableInit {} {\n\
    set env(TK_TABLE_LIBRARY) EMBEDDED_RUNTIME\n"
#ifdef NO_EMBEDDED_RUNTIME
"    append msg \"tkTable requires embedded runtime to be compiled for\"\n\
    append msg \" use in safe interpreters\"\n\
    return -code error $msg\n"
#endif
#   ifdef MAC_TCL
"    source -rsrc tkTable"
#   else
"    uplevel #0 {"
#	include "tkTable.tcl.h"
"    }"
#   endif
"  }\n\
}\n\
tkTableInit";

