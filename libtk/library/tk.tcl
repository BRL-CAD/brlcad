# tk.tcl --
#
# Initialization script normally executed in the interpreter for each
# Tk-based application.  Arranges class bindings for widgets.
#
# SCCS: @(#) tk.tcl 1.98 97/10/28 15:21:04
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# Insist on running with compatible versions of Tcl and Tk.

package require -exact Tk 8.0
package require -exact Tcl 8.0

# Add Tk's directory to the end of the auto-load search path, if it
# isn't already on the path:

if {[info exists auto_path]} {
    if {[lsearch -exact $auto_path $tk_library] < 0} {
	lappend auto_path $tk_library
    }
}

# Turn off strict Motif look and feel as a default.

set tk_strictMotif 0

# tkScreenChanged --
# This procedure is invoked by the binding mechanism whenever the
# "current" screen is changing.  The procedure does two things.
# First, it uses "upvar" to make global variable "tkPriv" point at an
# array variable that holds state for the current display.  Second,
# it initializes the array if it didn't already exist.
#
# Arguments:
# screen -		The name of the new screen.

proc tkScreenChanged screen {
    set x [string last . $screen]
    if {$x > 0} {
	set disp [string range $screen 0 [expr $x - 1]]
    } else {
	set disp $screen
    }

    uplevel #0 upvar #0 tkPriv.$disp tkPriv
    global tkPriv
    global tcl_platform

    if [info exists tkPriv] {
	set tkPriv(screen) $screen
	return
    }
    set tkPriv(activeMenu) {}
    set tkPriv(activeItem) {}
    set tkPriv(afterId) {}
    set tkPriv(buttons) 0
    set tkPriv(buttonWindow) {}
    set tkPriv(dragging) 0
    set tkPriv(focus) {}
    set tkPriv(grab) {}
    set tkPriv(initPos) {}
    set tkPriv(inMenubutton) {}
    set tkPriv(listboxPrev) {}
    set tkPriv(menuBar) {}
    set tkPriv(mouseMoved) 0
    set tkPriv(oldGrab) {}
    set tkPriv(popup) {}
    set tkPriv(postedMb) {}
    set tkPriv(pressX) 0
    set tkPriv(pressY) 0
    set tkPriv(prevPos) 0
    set tkPriv(screen) $screen
    set tkPriv(selectMode) char
    if {[string compare $tcl_platform(platform) "unix"] == 0} {
	set tkPriv(tearoff) 1
    } else {
	set tkPriv(tearoff) 0
    }
    set tkPriv(window) {}
}

# Do initial setup for tkPriv, so that it is always bound to something
# (otherwise, if someone references it, it may get set to a non-upvar-ed
# value, which will cause trouble later).

tkScreenChanged [winfo screen .]

# tkEventMotifBindings --
# This procedure is invoked as a trace whenever tk_strictMotif is
# changed.  It is used to turn on or turn off the motif virtual
# bindings.
#
# Arguments:
# n1 - the name of the variable being changed ("tk_strictMotif").

proc tkEventMotifBindings {n1 dummy dummy} {
    upvar $n1 name
    
    if $name {
	set op delete
    } else {
	set op add
    }

    event $op <<Cut>> <Control-Key-w>
    event $op <<Copy>> <Meta-Key-w> 
    event $op <<Paste>> <Control-Key-y>
}

#----------------------------------------------------------------------
# Define the set of common virtual events.
#----------------------------------------------------------------------

switch $tcl_platform(platform) {
    "unix" {
	event add <<Cut>> <Control-Key-x> <Key-F20> 
	event add <<Copy>> <Control-Key-c> <Key-F16>
	event add <<Paste>> <Control-Key-v> <Key-F18>
	trace variable tk_strictMotif w tkEventMotifBindings
	set tk_strictMotif $tk_strictMotif
    }
    "windows" {
	event add <<Cut>> <Control-Key-x> <Shift-Key-Delete>
	event add <<Copy>> <Control-Key-c> <Control-Key-Insert>
	event add <<Paste>> <Control-Key-v> <Shift-Key-Insert>
    }
    "macintosh" {
	event add <<Cut>> <Control-Key-x> <Key-F2> 
	event add <<Copy>> <Control-Key-c> <Key-F3>
	event add <<Paste>> <Control-Key-v> <Key-F4>
	event add <<Clear>> <Clear>
    }
}

# ----------------------------------------------------------------------
# Read in files that define all of the class bindings.
# ----------------------------------------------------------------------

if {$tcl_platform(platform) != "macintosh"} {
    source $tk_library/button.tcl
    source $tk_library/entry.tcl
    source $tk_library/listbox.tcl
    source $tk_library/menu.tcl
    source $tk_library/scale.tcl
    source $tk_library/scrlbar.tcl
    source $tk_library/text.tcl
}

# ----------------------------------------------------------------------
# Default bindings for keyboard traversal.
# ----------------------------------------------------------------------

bind all <Tab> {tkTabToWindow [tk_focusNext %W]}
bind all <Shift-Tab> {tkTabToWindow [tk_focusPrev %W]}

# tkCancelRepeat --
# This procedure is invoked to cancel an auto-repeat action described
# by tkPriv(afterId).  It's used by several widgets to auto-scroll
# the widget when the mouse is dragged out of the widget with a
# button pressed.
#
# Arguments:
# None.

proc tkCancelRepeat {} {
    global tkPriv
    after cancel $tkPriv(afterId)
    set tkPriv(afterId) {}
}

# tkTabToWindow --
# This procedure moves the focus to the given widget.  If the widget
# is an entry, it selects the entire contents of the widget.
#
# Arguments:
# w - Window to which focus should be set.

proc tkTabToWindow {w} {
    if {"[winfo class $w]" == "Entry"} {
	$w select range 0 end
	$w icur end
    }
    focus $w
}
