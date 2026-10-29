#                       A P P L Y . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
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
#	Procedures to apply commands to one or more display managers.
#

set mged_apply_retry_interval_ms 25

proc mged_apply_when_available {retry_command script} {
    global mged_apply_retry_interval_ms

    set status [catch {uplevel 1 $script} message options]
    if {$status && [mged_command_busy $options]} {
	# Long-running GED commands pump Tk events.  A display-manager menu
	# callback serviced during that window must wait for the shared state
	# instead of dropping the requested change.
	after $mged_apply_retry_interval_ms $retry_command
    }

    return $message
}

proc mged_apply { id cmd } {
    global mged_gui

    if {![info exists mged_gui($id,apply_to)]} {
	set mged_gui($id,apply_to) 0
    }

    if {$mged_gui($id,apply_to) == 1} {
	mged_apply_local $id $cmd
    } elseif {$mged_gui($id,apply_to) == 2} {
	mged_apply_using_list $id $cmd
    } elseif {$mged_gui($id,apply_to) == 3} {
	mged_apply_all $mged_gui($id,active_dm) $cmd
    } else {
	mged_apply_active $id $cmd
    }
}

proc mged_apply_active { id cmd } {
    global mged_gui

    return [mged_apply_when_available [list mged_apply_active $id $cmd] {
	winset $mged_gui($id,active_dm)
	uplevel \#0 $cmd
    }]
}

proc mged_apply_local { id cmd } {
    global mged_gui

    return [mged_apply_when_available [list mged_apply_local $id $cmd] {
	winset $mged_gui($id,top).ul
	uplevel \#0 $cmd

	winset $mged_gui($id,top).ur
	uplevel \#0 $cmd

	winset $mged_gui($id,top).ll
	uplevel \#0 $cmd

	winset $mged_gui($id,top).lr
	uplevel \#0 $cmd

	winset $mged_gui($id,active_dm)
    }]
}

proc mged_apply_using_list { id cmd } {
    global mged_gui

    return [mged_apply_when_available [list mged_apply_using_list $id $cmd] {
	foreach dm $mged_gui($id,apply_list) {
	    winset $dm
	    uplevel \#0 $cmd
	}

	winset $mged_gui($id,active_dm)
    }]
}

proc mged_apply_all { win cmd } {
    return [mged_apply_when_available [list mged_apply_all $win $cmd] {
	foreach dm [get_dm_list] {
	    winset $dm
	    uplevel \#0 $cmd
	}

	winset $win
    }]
}

## - mged_shaded_mode_helper
#
# Automatically set GUI state for shaded_mode.
#
proc mged_shaded_mode_helper {val} {
    global mged_gui
    global mged_players

    if ![info exists mged_players] {
	return
    }

    if {$val < 0 || 2 < $val} {
	# do nothing
	return
    }

    # the gui variables must be either 0 or 1
    if {$val} {
	set val 1
    } else {
	set val 0
    }

    foreach id $mged_players {
	set mged_gui($id,zbuffer) $val
	set mged_gui($id,zclip) $val
	set mged_gui($id,lighting) $val

	mged_apply_local $id "dm set zbuffer $val; dm set zclip $val; dm set lighting $val"
    }

    return ""
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
