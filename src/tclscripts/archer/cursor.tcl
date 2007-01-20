#                      C U R S O R . T C L
# BRL-CAD
#
# Copyright (c) 2006-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# DESCRIPTION
#    Cursor setting utilities
#
# AUTHOR
#    Doug Howard
#    Bob Parker
#
#***
##############################################################

if {$tcl_platform(os) == "Windows NT"} {
    package require BLT
} else {
    # For the moment, leave it this way for
    # all other platforms.
    #package require blt
    package require BLT
}

# avoid a pkg_index error about ::blt:: being unknown
namespace eval blt {}

if {![info exists ::blt::cursorWaitCount]} {
    set ::blt::cursorWaitCount 0
}

# PROCEDURE: SetWaitCursor
#
# Changes the cursor for all of the GUI's widgets to the wait cursor.
#
# Arguments:
#       None
#
# Results:
#       None
#
proc SetWaitCursor {} {
    incr ::blt::cursorWaitCount

    if {1 < $::blt::cursorWaitCount} {
	# Already in cursor wait mode
	return
    }

    update idletasks
    set children [winfo children .]
    foreach kid $children {
        if {![catch {$kid isa "::itk::Toplevel"} result]} {
            switch -- $result {
                "1" {catch {blt::busy $kid}}
            }
        }
    }
    blt::busy .
    update
}

# PROCEDURE: SetNormalCursor
#
# Changes the cursor for all of the GUI's widgets to the normal cursor.
#
# Arguments:
#       None
#
# Results:
#       None
#
proc SetNormalCursor {} {
    incr ::blt::cursorWaitCount -1
    if {$::blt::cursorWaitCount < 0} {
	# Already in cursor normal mode
	set ::blt::cursorWaitCount 0
	return
    }

    if {$::blt::cursorWaitCount != 0} {
	return
    }

    update idletasks
    set children [winfo children .]
    foreach kid $children {
        if {![catch {$kid isa "::itk::Toplevel"} result]} {
            switch -- $result {
                "1" {catch {blt::busy release $kid}}
            }
        }
    }
    blt::busy release .
    update
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
