/* 
 * tkInitScript.h --
 *
 *	This file contains Unix & Windows common init script
 *      It is not used on the Mac. (the mac init script is in tkMacInit.c)
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkInitScript.h 1.3 97/08/11 19:12:28
 */



/*
 * The following string is the startup script executed in new
 * interpreters.  It looks in several different directories
 * for a script "tk.tcl" that is compatible with this version
 * of Tk.  The tk.tcl script does all of the real work of
 * initialization.
 * When called from a safe interpreter, it does not use file exists.
 * we don't use pwd either because of safe interpreters.
 */

static char initScript[] =
"proc tkInit {} {\n\
    global tk_library tk_version tk_patchLevel env errorInfo\n\
    rename tkInit {}\n\
    set errors \"\"\n\
    if {![info exists tk_library]} {\n\
	set tk_library .\n\
    }\n\
    set dirs {}\n\
    if {[info exists env(TK_LIBRARY)]} {\n\
	lappend dirs $env(TK_LIBRARY)\n\
    }\n\
    lappend dirs $tk_library\n\
    lappend dirs [file join [file dirname [info library]] tk$tk_version]\n\
    set parentDir [file dirname [file dirname [info nameofexecutable]]]\n\
    lappend dirs [file join $parentDir tk$tk_version]\n\
    lappend dirs [file join $parentDir lib tk$tk_version]\n\
    lappend dirs [file join $parentDir library]\n\
    set parentParentDir [file dirname $parentDir]\n\
    if [string match {*[ab]*} $tk_patchLevel] {\n\
        set dirSuffix $tk_patchLevel\n\
    } else {\n\
        set dirSuffix $tk_version\n\
    }\n\
    lappend dirs [file join $parentParentDir tk$dirSuffix library]\n\
    lappend dirs [file join $parentParentDir library]\n\
    lappend dirs [file join [file dirname \
	[file dirname [info library]]] tk$dirSuffix library]\n\
    foreach i $dirs {\n\
	set tk_library $i\n\
	set tkfile [file join $i tk.tcl]\n\
        if {[interp issafe] || [file exists $tkfile]} {\n\
	    if {![catch {uplevel #0 [list source $tkfile]} msg]} {\n\
		return\n\
	    } else {\n\
		append errors \"$tkfile: $msg\n$errorInfo\n\"\n\
	    }\n\
	}\n\
    }\n\
    set msg \"Can't find a usable tk.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\n\"\n\
    append msg \"$errors\n\n\"\n\
    append msg \"This probably means that Tk wasn't installed properly.\n\"\n\
    error $msg\n\
}\n\
tkInit";

