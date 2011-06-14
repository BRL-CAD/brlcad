# tipstack.tcl --
#
#	Based on 'tooltip', provides a dynamic stack of tip texts per
#	widget. This allows dynamic transient changes to the tips, for
#	example to temporarily replace a standard epxlanation with an
#	error message.
#
# Copyright (c) 2003 ActiveState Corporation.
#
# See the file "license.terms" for information on usage and
# redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id$
#

# ### ######### ###########################
# Requisites

package require tooltip
namespace eval ::tipstack {}

# ### ######### ###########################
# Public API
#
## Basic syntax for all commands having a widget reference:
#
## tipstack::command .w ...
## tipstack::command .m -index foo ...

# ### ######### ###########################
## Push new text for a widget (or menu)

proc ::tipstack::push {args} {
    if {([llength $args] != 2) && (([llength $args] != 4))} {
	return -code error "wrong#args: expected w ?-index index? text"
    }

    # Extract valueable parts.

    set text [lindex $args end]
    set wref [lrange $args 0 end-1]

    # Remember new data (setup/extend db)

    variable db
    if {![info exists db($wref)]} {
	set db($wref) [list $text]
    } else {
	lappend db($wref) $text
    }

    # Forward to standard tooltip package.

    eval [linsert [linsert $wref end $text] 0 tooltip::tooltip]
    return
}

# ### ######### ###########################
## Pop text from stack of tip for widget.
## ! Keeps the bottom-most entry.

proc ::tipstack::pop {args} {
    if {([llength $args] != 1) && (([llength $args] != 3))} {
	return -code error "wrong#args: expected w ?-index index?"
    }
    # args == wref (see 'push').
    set wref $args

    # Pop top information form the database. Except if the
    # text is the last in the stack. Then we will keep it, it
    # is the baseline for the widget.

    variable db
    if {![info exists db($wref)]} {
	set text ""
    } else {
	set data $db($wref)

	if {[llength $data] == 1} {
	    set text [lindex $data 0]
	} else {
	    set data [lrange $data 0 end-1]
	    set text [lindex $data end]

	    set db($wref) $data
	}
    }

    # Forward to standard tooltip package.

    eval [linsert [linsert $wref end $text] 0 tooltip::tooltip]
    return
}

# ### ######### ###########################
## Clears out all data about a widget (or menu).

proc ::tipstack::clear {args} {

    if {([llength $args] != 1) && (([llength $args] != 3))} {
	return -code error "wrong#args: expected w ?-index index?"
    }
    # args == wref (see 'push').
    set wref $args

    # Remove data about widget.

    variable db
    catch {unset db($wref)}

    eval [linsert [linsert $wref end ""] 0 tooltip::tooltip]
    return
}

# ### ######### ###########################
## Convenient definition of tooltips for multiple
## independent widgets. No menus possible

proc ::tipstack::def {defs} {
    foreach {path text} $defs {
	push $path $text
    }
    return
}

# ### ######### ###########################
## Convenient definition of tooltips for multiple
## widgets in a containing widget. No menus possible.
## This is for megawidgets.

proc ::tipstack::defsub {base defs} {
    foreach {subpath text} $defs {
	push $base$subpath $text
    }
    return
}

# ### ######### ###########################
## Convenient clearage of tooltips for multiple
## widgets in a containing widget. No menus possible.
## This is for megawidgets.

proc ::tipstack::clearsub {base} {
    variable db

    foreach k [array names db ${base}*] {
	# Danger. Will fail if 'base' matches a menu reference.
	clear $k
    }
    return
}

# ### ######### ###########################
# Internal commands -- None

# ### ######### ###########################
## Data structures

namespace eval ::tipstack {
    # Map from widget references to stack of tooltips.

    variable  db
    array set db {}
}

# ### ######### ###########################
# Ready

package provide tipstack 1.0.1
