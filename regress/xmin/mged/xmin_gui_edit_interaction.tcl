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
    variable menu_index 0
    variable click_base_x 60
    variable click_base_y 40
    variable click_step 10
    variable solid_menu_modes {
	Rotate Translate Scale {Set A} {Set B} {Set C} {Set A,B,C}
    }
    variable object_menu_modes {
	Scale {X move} {Y move} {XY move} Rotate
	{Scale X} {Scale Y} {Scale Z}
    }
    variable modifier_cases {
	{scale {shift ctrl} 1}
	{x_translate {alt shift} 1}
	{y_translate {alt shift} 2}
	{z_translate {alt shift} 3}
	{x_rotate {alt ctrl} 1}
	{y_rotate {alt ctrl} 2}
	{z_rotate {alt ctrl} 3}
	{x_scale {alt shift ctrl} 1}
	{y_scale {alt shift ctrl} 2}
	{z_scale {alt shift ctrl} 3}
    }
    variable modifier_index 0
    variable modifier_context ""
    variable modifier_next ""
    variable mouse_behavior_cases {
	{{Default} d}
	{{Pick Edit-Primitive} s}
	{{Pick Edit-Matrix} m}
	{{Pick Edit-Combination} c}
	{{Sweep Raytrace-Rectangle} r}
	{{Pick Raytrace-Object(s)} o}
	{{Query Ray} q}
	{{Sweep Paint-Rectangle} p}
	{{Sweep Zoom-Rectangle} z}
    }
    variable mouse_behavior_index 0
    variable pick_attempts 0
    variable pick_limit 50
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

proc ::mged::xmin::edit_interaction::require_view_changed {operation} {
    variable initial_view
    ::gui::test::require {
	![::gui::test::equivalent_values [view_state] $initial_view]
    } "$operation did not return the mouse to view manipulation"
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

proc ::mged::xmin::edit_interaction::drag {modifiers {button 1}} {
    variable dm
    variable drag_pixels
    variable drag_steps
    variable drag_delay_ms
    set width [winfo width $dm]
    set height [winfo height $dm]
    set x [expr {$width / 2}]
    set y [expr {$height / 2}]
    set ctl $::env(GUI_TEST_CTL)
    set pressed {}
    set status [catch {
	foreach modifier $modifiers {
	    exec $ctl key-down $modifier
	    lappend pressed $modifier
	}
	exec $ctl mouse-drag --button $button --steps $drag_steps \
	    --delay $drag_delay_ms \
	    [winfo id $dm] \
	    $x $y [expr {$x + $drag_pixels}] [expr {$y - $drag_pixels}]
    } message options]
    foreach modifier [lreverse $pressed] {
	set release_status [catch {
	    exec $ctl key-up $modifier
	} release_message release_options]
	if {$release_status && !$status} {
	    set status $release_status
	    set message $release_message
	    set options $release_options
	}
    }
    if {$status} {
	return -options $options $message
    }
}

proc ::mged::xmin::edit_interaction::start_modifier_matrix {context next} {
    variable initial_view
    variable modifier_context
    variable modifier_index
    variable modifier_next
    setview 35 25 0
    set initial_view [view_state]
    set modifier_context $context
    set modifier_next $next
    set modifier_index 0
    later ::mged::xmin::edit_interaction::modifier_step
}

proc ::mged::xmin::edit_interaction::modifier_step {} {
    variable modifier_cases
    variable modifier_index
    lassign [lindex $modifier_cases $modifier_index] label modifiers button
    capture_before_edit
    drag $modifiers $button
    later ::mged::xmin::edit_interaction::check_modifier_step
}

proc ::mged::xmin::edit_interaction::check_modifier_step {} {
    variable modifier_cases
    variable modifier_context
    variable modifier_index
    variable modifier_next
    set label [lindex [lindex $modifier_cases $modifier_index] 0]
    set operation "$modifier_context mouse $label"
    require_view_unchanged $operation
    if {![image_changed $operation \
	::mged::xmin::edit_interaction::check_modifier_step]} {
	return
    }
    if {[incr modifier_index] < [llength $modifier_cases]} {
	later ::mged::xmin::edit_interaction::modifier_step
	return
    }
    later $modifier_next
}

proc ::mged::xmin::edit_interaction::menu_click {} {
    variable dm
    variable menu_index
    variable click_base_x
    variable click_base_y
    variable click_step
    # Vary targets so consecutive X, Y, and XY moves cannot be no-ops.
    set x [expr {[winfo width $dm] / 2 + $click_base_x +
	($menu_index % 3) * $click_step}]
    set y [expr {[winfo height $dm] / 2 - $click_base_y -
	($menu_index % 2) * $click_step}]
    exec $::env(GUI_TEST_CTL) click [winfo id $dm] $x $y 2
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
    start_modifier_matrix object \
	::mged::xmin::edit_interaction::finish_object_modifiers
}

proc ::mged::xmin::edit_interaction::finish_object_modifiers {} {
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
    start_modifier_matrix solid \
	::mged::xmin::edit_interaction::solid_grid_snap
}

proc ::mged::xmin::edit_interaction::solid_grid_snap {} {
    variable initial_view
    rset grid snap 1
    set initial_view [view_state]
    capture_before_edit
    drag shift
    later ::mged::xmin::edit_interaction::check_solid_grid_snap
}

proc ::mged::xmin::edit_interaction::check_solid_grid_snap {} {
    require_view_unchanged "solid grid-snapped mouse pan"
    if {![image_changed "solid grid-snapped mouse pan" \
	::mged::xmin::edit_interaction::check_solid_grid_snap]} {
	return
    }
    rset grid snap 0
    later ::mged::xmin::edit_interaction::setup_solid_menu
}

proc ::mged::xmin::edit_interaction::setup_solid_menu {} {
    variable initial_view
    setview 35 25 0
    set initial_view [view_state]
    later ::mged::xmin::edit_interaction::solid_menu_step
}

proc ::mged::xmin::edit_interaction::solid_menu_step {} {
    global mged_players
    variable solid_menu_modes
    variable menu_index
    set menu .[lindex $mged_players 0].menubar.edit
    set mode [lindex $solid_menu_modes $menu_index]
    $menu invoke [$menu index $mode]
    ::gui::test::require {$::edit_solid_flag > 0} \
	"solid $mode menu did not select an edit mode"
    capture_before_edit
    menu_click
    later ::mged::xmin::edit_interaction::check_solid_menu
}

proc ::mged::xmin::edit_interaction::check_solid_menu {} {
    variable solid_menu_modes
    variable menu_index
    set mode [lindex $solid_menu_modes $menu_index]
    require_view_unchanged "solid menu $mode mouse click"
    if {![image_changed "solid menu $mode mouse click" \
	::mged::xmin::edit_interaction::check_solid_menu]} {
	return
    }
    if {[incr menu_index] < [llength $solid_menu_modes]} {
	later ::mged::xmin::edit_interaction::solid_menu_step
	return
    }
    later ::mged::xmin::edit_interaction::solid_menu_none
}

proc ::mged::xmin::edit_interaction::solid_menu_none {} {
    global mged_players
    variable initial_view
    set menu .[lindex $mged_players 0].menubar.edit
    $menu invoke [$menu index "None Of Above"]
    ::gui::test::require {$::edit_solid_flag == 0} \
	"solid None Of Above menu did not clear the edit flag"
    set initial_view [view_state]
    menu_click
    later ::mged::xmin::edit_interaction::check_solid_menu_none
}

proc ::mged::xmin::edit_interaction::check_solid_menu_none {} {
    require_view_changed "solid None Of Above"
    later ::mged::xmin::edit_interaction::setup_object_menu
}

proc ::mged::xmin::edit_interaction::setup_object_menu {} {
    variable initial_view
    variable menu_index
    press reject
    Z
    draw edit.c
    center 0 0 0
    size 40
    setview 35 25 0
    _mged_press oill
    _mged_ill -e -i 1 /edit.c/edit.s
    _mged_matpick 1
    ::gui::test::require {[status state] eq "OBJ EDIT"} \
	"could not enter object edit mode for menu checks"
    rset var transform e
    set initial_view [view_state]
    set menu_index 0
    later ::mged::xmin::edit_interaction::object_menu_step
}

proc ::mged::xmin::edit_interaction::object_menu_step {} {
    global mged_players
    variable object_menu_modes
    variable menu_index
    set menu .[lindex $mged_players 0].menubar.edit
    set mode [lindex $object_menu_modes $menu_index]
    $menu invoke [$menu index $mode]
    ::gui::test::require {$::edit_object_flag > 0} \
	"object $mode menu did not select an edit mode"
    capture_before_edit
    menu_click
    later ::mged::xmin::edit_interaction::check_object_menu
}

proc ::mged::xmin::edit_interaction::check_object_menu {} {
    variable object_menu_modes
    variable menu_index
    set mode [lindex $object_menu_modes $menu_index]
    require_view_unchanged "object menu $mode mouse click"
    if {![image_changed "object menu $mode mouse click" \
	::mged::xmin::edit_interaction::check_object_menu]} {
	return
    }
    if {[incr menu_index] < [llength $object_menu_modes]} {
	later ::mged::xmin::edit_interaction::object_menu_step
	return
    }
    later ::mged::xmin::edit_interaction::object_menu_none
}

proc ::mged::xmin::edit_interaction::object_menu_none {} {
    global mged_players
    variable initial_view
    set menu .[lindex $mged_players 0].menubar.edit
    $menu invoke [$menu index "none of above"]
    ::gui::test::require {$::edit_object_flag == 0} \
	"object none of above menu did not clear the edit flag"
    set initial_view [view_state]
    menu_click
    later ::mged::xmin::edit_interaction::check_object_menu_none
}

proc ::mged::xmin::edit_interaction::check_object_menu_none {} {
    require_view_changed "object none of above"
    press reject
    later ::mged::xmin::edit_interaction::mouse_behavior_step
}

proc ::mged::xmin::edit_interaction::mouse_behavior_step {} {
    global mged_gui mged_players
    variable mouse_behavior_cases
    variable mouse_behavior_index
    set id [lindex $mged_players 0]
    set menu .$id.menubar.settings.mouse_behavior
    lassign [lindex $mouse_behavior_cases $mouse_behavior_index] label code
    $menu invoke [$menu index $label]
    ::gui::test::require {
	$mged_gui($id,mouse_behavior) eq $code &&
	[rset var mouse_behavior] eq $code
    } "Settings Mouse Behavior '$label' did not select '$code'"
    if {[incr mouse_behavior_index] < [llength $mouse_behavior_cases]} {
	later ::mged::xmin::edit_interaction::mouse_behavior_step
	return
    }
    later ::mged::xmin::edit_interaction::pick_primitive
}

proc ::mged::xmin::edit_interaction::pick_primitive {} {
    global mged_players
    variable dm
    set id [lindex $mged_players 0]
    set menu .$id.menubar.settings.mouse_behavior
    $menu invoke [$menu index Default]
    Z
    draw edit.s
    center 0 0 0
    size 40
    setview 0 0 0
    $menu invoke [$menu index "Pick Edit-Primitive"]
    exec $::env(GUI_TEST_CTL) click [winfo id $dm] \
	[expr {[winfo width $dm] / 2}] [expr {[winfo height $dm] / 2}] 2
    later ::mged::xmin::edit_interaction::check_primitive_pick
}

proc ::mged::xmin::edit_interaction::check_primitive_pick {} {
    variable pick_attempts
    variable pick_limit
    if {[status state] ne "SOL EDIT"} {
	if {[incr pick_attempts] >= $pick_limit} {
	    error "Pick Edit-Primitive mouse click did not enter solid edit"
	}
	later ::mged::xmin::edit_interaction::check_primitive_pick
	return
    }
    ::gui::test::require {[rset var mouse_behavior] eq "d"} \
	"Pick Edit-Primitive did not restore default mouse behavior"
    press reject
    ::mged::gui::test::finish 0 \
	"PASS: MGED mouse edit routing and redraw"
}

after 25 [list ::mged::xmin::edit_interaction::checked \
    ::mged::xmin::edit_interaction::ready]

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
