#                       A P P L Y . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
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
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
#			A P P L Y . T C L
#
#	Procedures to apply commands to one or more display managers.
#
#	Author - Robert G. Parker
#

proc mged_apply { id cmd } {
    global mged_gui

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

    winset $mged_gui($id,active_dm)
    catch { uplevel #0 $cmd } msg

    return $msg
}

proc mged_apply_local { id cmd } {
    global mged_gui

    winset $mged_gui($id,top).ul
    catch { uplevel #0 $cmd } msg

    winset $mged_gui($id,top).ur
    catch { uplevel #0 $cmd } msg

    winset $mged_gui($id,top).ll
    catch { uplevel #0 $cmd } msg

    winset $mged_gui($id,top).lr
    catch { uplevel #0 $cmd } msg

    winset $mged_gui($id,active_dm)

    return $msg
}

proc mged_apply_using_list { id cmd } {
    global mged_gui

    set msg ""
    foreach dm $mged_gui($id,apply_list) {
	winset $dm
	catch { uplevel #0 $cmd } msg
    }

    winset $mged_gui($id,active_dm)

    return $msg
}

proc mged_apply_all { win cmd } {
    set msg ""
    foreach dm [get_dm_list] {
	winset $dm
	catch { uplevel #0 $cmd } msg
    }

    winset $win

    return $msg
}

## - mged_shaded_mode_helper
#
# Automatically set GUI state for shaded_mode.
#
proc mged_shaded_mode_helper {val} {
    global mged_gui
    global mged_players

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
