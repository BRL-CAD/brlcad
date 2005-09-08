#
# dragdrop.tcl
#
# ----------------------------------------------------------------------
# Bindings for the BLT drag&drop command
# ----------------------------------------------------------------------
#   AUTHOR:  George Howlett
#            Bell Labs Innovations for Lucent Technologies
#            gah@bell-labs.com
#            http://www.tcltk.com/blt
# ----------------------------------------------------------------------
# Copyright (c) 1998  Lucent Technologies, Inc.
# ======================================================================
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted,
# provided that the above copyright notice appear in all copies and that
# both that the copyright notice and warranty disclaimer appear in
# supporting documentation, and that the names of Lucent Technologies
# any of their entities not be used in advertising or publicity
# pertaining to distribution of the software without specific, written
# prior permission.
#
# Lucent Technologies disclaims all warranties with regard to this
# software, including all implied warranties of merchantability and
# fitness.  In no event shall Lucent be liable for any special, indirect
# or consequential damages or any damages whatsoever resulting from loss
# of use, data or profits, whether in an action of contract, negligence
# or other tortuous action, arising out of or in connection with the use
# or performance of this software.
#
# ======================================================================

if { $tcl_version >= 8.0 } {
    set cmd blt::drag&drop
} else {
    set cmd drag&drop
}
for { set i 1 } { $i <= 5 } { incr i } {
    bind BltDrag&DropButton$i <ButtonPress-$i>  [list $cmd drag %W %X %Y]
    bind BltDrag&DropButton$i <B$i-Motion>	  [list $cmd drag %W %X %Y]
    bind BltDrag&DropButton$i <ButtonRelease-$i> [list $cmd drop %W %X %Y]
}

# ----------------------------------------------------------------------
#
# Drag&DropInit --
#
#	Invoked from C whenever a new drag&drop source is created.
#	Sets up the default bindings for the drag&drop source.
#
#	<ButtonPress-?>	 Starts the drag operation.
#	<B?-Motion>	 Updates the drag.
#	<ButtonRelease-?> Drop the data on the target.
#
# Arguments:	
#	widget		source widget
#	button		Mouse button used to activate drag.
#	cmd		"dragdrop" or "blt::dragdrop"
#
# ----------------------------------------------------------------------

proc blt::Drag&DropInit { widget button } {
    set tagList {}
    if { $button > 0 } {
	lappend tagList BltDrag&DropButton$button
    }
    foreach tag [bindtags $widget] {
	if { ![string match BltDrag&DropButton* $tag] } {
	    lappend tagList $tag
	}
    }
    bindtags $widget $tagList
}

