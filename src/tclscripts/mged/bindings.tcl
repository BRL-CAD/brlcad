#                    B I N D I N G S . T C L
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
if ![info exists mged_players] {
    set mged_players {}
}

proc mged_bind_dm { w } {
    global hot_key
    global forwarding_key
    global tcl_platform

    # KeySym for <F9> --> 0xffc6 --> 65478
    set hot_key 65478

    #make this the current display manager
    if { $::tcl_platform(platform) != "windows" && $::tcl_platform(os) != "Darwin" } {
	bind $w <Enter> "winset $w; focus $w;"
    } else {
	# some platforms should not be forced window activation (winset)
	bind $w <Enter> "winset $w;"
    }

    # Load standardized JSON bindings for mged

}

proc print_return_val str {
    if {$str != ""} {
	distribute_text "" "" $str
	stuff_str $str
    }
}

proc reset_everything { w } {
    global mged_gui

    # stop all the spinning
    knob zero

    # stop all the editing
    press reject
    
    # restore default mouse behavior
    set id [get_player_id_dm $w]
    set mged_gui($id,mouse_behavior) d
    set_mouse_behavior $id
}

proc update_gui { w vname val } {
    global mged_players
    global mged_gui

    foreach id $mged_players {
	if {$mged_gui($id,active_dm) == $w} {
	    set mged_gui($id,$vname) $val
	    return
	}
    }
}

# Dummy stubs to prevent legacy scripts from failing if they try to invoke them
proc default_key_bindings { w } {}
proc default_mouse_bindings { w } {}
proc forward_key_bindings { w } {}
proc toggle_forward_key_bindings { w } {}
proc set_forward_keys { w val } {}
proc shift_grip_hints { w hint } {}
proc scale_shift_grip_hints { w axis } {}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
