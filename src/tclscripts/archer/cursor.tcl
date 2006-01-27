# TYPE: tcltk
##############################################################
#
# cursor.tcl
#
##############################################################
#****c* Cursor/Cursor
# NAME
#    Cursor
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
    package require blt
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

