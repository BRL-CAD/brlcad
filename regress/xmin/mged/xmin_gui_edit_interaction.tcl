#            X M I N _ G U I _ E D I T _ I N T E R A C T I O N . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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
# Exercise real XTEST mouse edits and verify the displayed geometry and
# the view independently.  Only setup forces a baseline refresh; edit phases
# return to MGED's refresh loop before observing pixels.

source $::env(MGED_GUI_TEST_LIBRARY)

namespace eval ::mged::xmin::edit_interaction {
    variable dm ""
    variable initial_view ""
    variable before_image ""
    variable change_deadline 0
    variable image_serial 0
    variable commands {
	{tra 4 0 0}
	{rot 0 0 20}
	{mrot 0 25 0}
	{arot 1 0 0 20}
    }
    variable command_index 0
    variable ready_attempts 0
    variable ready_limit 400
    variable phase_delay_ms 100
    variable change_timeout_ms 5000
    variable crop_inset_divisor 4
    variable drag_pixels 55
    variable drag_steps 8
    variable drag_delay_ms 15
}

proc ::mged::xmin::edit_interaction::checked {action} {
    if {[catch {uplevel #0 $action} message options]} {
	if {[dict exists $options -errorinfo]} {
	    puts stderr [dict get $options -errorinfo]
	}
	::mged::gui::test::finish 1 \
	    "FAIL: MGED interactive editing: $message"
    }
}

proc ::mged::xmin::edit_interaction::later {action} {
    variable phase_delay_ms
    after $phase_delay_ms [list ::mged::xmin::edit_interaction::checked $action]
}

proc ::mged::xmin::edit_interaction::view_state {} {
    return [list [center] [size] [view quat]]
}

proc ::mged::xmin::edit_interaction::require_view_unchanged {operation} {
    variable initial_view
    ::gui::test::require {
	[::gui::test::equivalent_values [view_state] $initial_view]
    } "$operation changed the view instead of the edited geometry"
}

proc ::mged::xmin::edit_interaction::display_image {} {
    variable dm
    variable image_serial
    variable crop_inset_divisor
    # Xmin's root capture does not include the Tk photo used by tkswrast.
    set photo [[lindex [winfo children $dm] 0] cget -image]
    if {$photo eq ""} {
	error "display image is unavailable"
    }
    # Compare the central geometry, away from faceplate and status overlays.
    set width [::image width $photo]
    set height [::image height $photo]
    set x0 [expr {$width / $crop_inset_divisor}]
    set y0 [expr {$height / $crop_inset_divisor}]
    set pixels [$photo data -format ppm -from $x0 $y0 \
	[expr {$width - $x0}] [expr {$height - $y0}]]
    set path [file join $::env(GUI_TEST_DIR) \
	[format "edit-view-%02d.ppm" [incr image_serial]]]
    set channel [open $path wb]
    puts -nonewline $channel $pixels
    close $channel
    return $pixels
}

proc ::mged::xmin::edit_interaction::capture_before_edit {} {
    variable before_image
    variable change_deadline
    variable change_timeout_ms
    set before_image [display_image]
    set change_deadline [expr {[clock milliseconds] + $change_timeout_ms}]
}

proc ::mged::xmin::edit_interaction::image_changed {operation retry} {
    variable before_image
    variable change_deadline
    if {[display_image] ne $before_image} {
	return 1
    }
    if {[clock milliseconds] >= $change_deadline} {
	error "$operation did not redraw the edited geometry"
    }
    later $retry
    return 0
}

proc ::mged::xmin::edit_interaction::drag {modifier} {
    variable dm
    variable drag_pixels
    variable drag_steps
    variable drag_delay_ms
    set width [winfo width $dm]
    set height [winfo height $dm]
    set x [expr {$width / 2}]
    set y [expr {$height / 2}]
    set ctl $::env(GUI_TEST_CTL)
    exec $ctl key-down $modifier
    set status [catch {
	exec $ctl mouse-drag --steps $drag_steps --delay $drag_delay_ms \
	    [winfo id $dm] \
	    $x $y [expr {$x + $drag_pixels}] [expr {$y - $drag_pixels}]
    } message options]
    set release_status [catch {exec $ctl key-up $modifier} release_message release_options]
    if {$status} {
	return -options $options $message
    }
    if {$release_status} {
	return -options $release_options $release_message
    }
}

proc ::mged::xmin::edit_interaction::ready {} {
    global mged_gui mged_players
    variable ready_attempts
    variable ready_limit
    if {![info exists mged_players] || [llength $mged_players] == 0 ||
	![info exists mged_gui([lindex $mged_players 0],active_dm)]} {
	if {[incr ready_attempts] >= $ready_limit} {
	    error "MGED GUI did not become ready"
	}
	later ::mged::xmin::edit_interaction::ready
	return
    }
    set id [lindex $mged_players 0]
    if {![winfo exists .$id] || ![winfo ismapped .$id]} {
	if {[incr ready_attempts] >= $ready_limit} {
	    error "MGED GUI did not finish mapping"
	}
	later ::mged::xmin::edit_interaction::ready
	return
    }
    setup_object $id
}

proc ::mged::xmin::edit_interaction::setup_object {id} {
    global mged_gui
    variable dm
    variable initial_view
    set dm $mged_gui($id,active_dm)
    winset $dm
    cd $::env(GUI_TEST_DIR)
    opendb [file join $::env(GUI_TEST_DIR) edit-interaction.g] y
    put edit.s ell V {0 0 0} A {8 0 0} B {0 4 0} C {0 0 2}
    comb edit.c u edit.s
    Z
    draw edit.c
    center 0 0 0
    size 40
    setview 0 0 0
    _mged_press oill
    _mged_ill -e -i 1 /edit.c/edit.s
    _mged_matpick 1
    ::gui::test::require {[status state] eq "OBJ EDIT"} \
	"could not enter object edit mode"
    rset var transform e
    refresh
    set initial_view [view_state]
    later ::mged::xmin::edit_interaction::object_command
}

proc ::mged::xmin::edit_interaction::object_command {} {
    variable commands
    variable command_index
    capture_before_edit
    uplevel #0 [lindex $commands $command_index]
    later ::mged::xmin::edit_interaction::check_object_command
}

proc ::mged::xmin::edit_interaction::check_object_command {} {
    variable commands
    variable command_index
    set command [lindex $commands $command_index]
    require_view_unchanged $command
    if {![image_changed $command ::mged::xmin::edit_interaction::check_object_command]} {
	return
    }
    if {[incr command_index] < [llength $commands]} {
	later ::mged::xmin::edit_interaction::object_command
    } else {
	later ::mged::xmin::edit_interaction::object_drag
    }
}

proc ::mged::xmin::edit_interaction::object_drag {} {
    capture_before_edit
    drag shift
    later ::mged::xmin::edit_interaction::check_object_drag
}

proc ::mged::xmin::edit_interaction::check_object_drag {} {
    require_view_unchanged "object mouse pan"
    if {![image_changed "object mouse pan" ::mged::xmin::edit_interaction::check_object_drag]} {
	return
    }
    later ::mged::xmin::edit_interaction::object_spin
}

proc ::mged::xmin::edit_interaction::object_spin {} {
    capture_before_edit
    drag ctrl
    later ::mged::xmin::edit_interaction::check_object_spin
}

proc ::mged::xmin::edit_interaction::check_object_spin {} {
    require_view_unchanged "object mouse spin"
    if {![image_changed "object mouse spin" ::mged::xmin::edit_interaction::check_object_spin]} {
	return
    }
    press accept
    later ::mged::xmin::edit_interaction::setup_solid
}

proc ::mged::xmin::edit_interaction::setup_solid {} {
    variable initial_view
    ::gui::test::require {[status state] eq "VIEWING"} \
	"object edit did not exit after accept"
    Z
    e edit.s
    center 0 0 0
    size 40
    setview 0 0 0
    sed edit.s
    ::gui::test::require {[status state] eq "SOL EDIT"} \
	"could not enter solid edit mode"
    rset var transform e
    set initial_view [view_state]
    later ::mged::xmin::edit_interaction::solid_pan
}

proc ::mged::xmin::edit_interaction::solid_pan {} {
    capture_before_edit
    drag shift
    later ::mged::xmin::edit_interaction::check_solid_pan
}

proc ::mged::xmin::edit_interaction::check_solid_pan {} {
    require_view_unchanged "solid mouse pan"
    if {![image_changed "solid mouse pan" ::mged::xmin::edit_interaction::check_solid_pan]} {
	return
    }
    later ::mged::xmin::edit_interaction::solid_spin
}

proc ::mged::xmin::edit_interaction::solid_spin {} {
    capture_before_edit
    drag ctrl
    later ::mged::xmin::edit_interaction::check_solid_spin
}

proc ::mged::xmin::edit_interaction::check_solid_spin {} {
    require_view_unchanged "solid mouse spin"
    if {![image_changed "solid mouse spin" ::mged::xmin::edit_interaction::check_solid_spin]} {
	return
    }
    press reject
    ::mged::gui::test::finish 0 \
	"PASS: MGED object and solid edits redraw without changing the view"
}

after 25 [list ::mged::xmin::edit_interaction::checked \
    ::mged::xmin::edit_interaction::ready]

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
