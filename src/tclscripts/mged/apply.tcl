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

    catch {::brlcad::mged winset $mged_gui($id,active_dm)} msg
    catch {uplevel \#0 $cmd} msg

    return $msg
}

proc mged_apply_local { id cmd } {
    global mged_gui

    ::brlcad::mged winset $mged_gui($id,top).ul
    catch { uplevel \#0 $cmd } msg

    ::brlcad::mged winset $mged_gui($id,top).ur
    catch { uplevel \#0 $cmd } msg

    ::brlcad::mged winset $mged_gui($id,top).ll
    catch { uplevel \#0 $cmd } msg

    ::brlcad::mged winset $mged_gui($id,top).lr
    catch { uplevel \#0 $cmd } msg

    ::brlcad::mged winset $mged_gui($id,active_dm)

    return $msg
}

proc mged_apply_using_list { id cmd } {
    global mged_gui

    set msg ""
    foreach dm $mged_gui($id,apply_list) {
	::brlcad::mged winset $dm
	catch { uplevel \#0 $cmd } msg
    }

    ::brlcad::mged winset $mged_gui($id,active_dm)

    return $msg
}

proc mged_apply_all { win cmd } {
    set msg ""
    foreach dm [::brlcad::mged get_dm_list] {
	::brlcad::mged winset $dm
	catch { uplevel \#0 $cmd } msg
    }

    ::brlcad::mged winset $win

    return $msg
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

	set commands [list \
	    [list ::brlcad::mged dm set zbuffer $val] \
	    [list ::brlcad::mged dm set zclip $val] \
	    [list ::brlcad::mged dm set lighting $val]]
	mged_apply_local $id [join $commands {; }]
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
