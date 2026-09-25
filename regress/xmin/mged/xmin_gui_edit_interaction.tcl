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
    variable oracle_units {mm in}
    variable oracle_index 0
    variable oracle_view_size 40
    variable oracle_drag_pixels 64
    variable oracle_scale 1
    variable oracle_solid ""
    variable oracle_comb ""
    variable oracle_expected {}
    variable oracle_model2view {}
    variable picker_attempts 0
    variable picker_limit 50
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
    } "$operation did not return the mouse to view manipulation (state [status state], transform [rset var transform], initial $initial_view, current [view_state])"
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
    later ::mged::xmin::edit_interaction::setup_oracle_unit
}

proc ::mged::xmin::edit_interaction::require_near {actual expected label {tolerance 0.01}} {
    if {[llength $actual] != [llength $expected]} {
	error "$label expected $expected, got $actual"
    }
    foreach value $actual target $expected {
	if {![string is double -strict $value] ||
	    abs($value - $target) > $tolerance} {
	    error "$label expected $expected, got $actual"
	}
    }
}

proc ::mged::xmin::edit_interaction::solid_edit_vertex {} {
    set serialization [get_sed]
    set index [lsearch -exact $serialization V]
    if {$index < 0} {
	error "get_sed has no vertex: $serialization"
    }
    return [lindex $serialization [expr {$index + 1}]]
}

proc ::mged::xmin::edit_interaction::status_matrix {name} {
    set lines [lrange [split [status $name] "\n"] 1 4]
    set matrix {}
    foreach line $lines {
	foreach value [regexp -all -inline {\S+} $line] {
	    lappend matrix $value
	}
    }
    if {[llength $matrix] != 16} {
	error "status $name did not return a 4x4 matrix: $matrix"
    }
    return $matrix
}

proc ::mged::xmin::edit_interaction::leaf_matrix {comb member} {
    set tree [db get $comb tree]
    if {[lrange $tree 0 1] ne [list l $member]} {
	error "$comb has an unexpected tree: $tree"
    }
    if {[llength $tree] == 2} {
	return {1 0 0 0  0 1 0 0  0 0 1 0  0 0 0 1}
    }
    if {[llength $tree] != 3 || [llength [lindex $tree 2]] != 16} {
	error "$comb has no 4x4 leaf matrix: $tree"
    }
    return [lindex $tree 2]
}

proc ::mged::xmin::edit_interaction::translation_matrix {dx dy} {
    return [list 1 0 0 $dx  0 1 0 $dy  0 0 1 0  0 0 0 1]
}

proc ::mged::xmin::edit_interaction::view_translation_matrix {matrix dx dy} {
    set expected $matrix
    for {set row 0} {$row < 4} {incr row} {
	set base [expr {$row * 4}]
	lset expected [expr {$base + 3}] [expr {
	    [lindex $matrix [expr {$base + 3}]] +
	    [lindex $matrix $base] * $dx +
	    [lindex $matrix [expr {$base + 1}]] * $dy}]
    }
    return $expected
}

proc ::mged::xmin::edit_interaction::setup_oracle_unit {} {
    variable oracle_units
    variable oracle_index
    variable oracle_scale
    variable oracle_solid
    variable oracle_comb
    variable oracle_view_size
    variable oracle_drag_pixels
    variable initial_view
    variable drag_pixels
    set unit [lindex $oracle_units $oracle_index]
    set oracle_scale [expr {$unit eq "in" ? 25.4 : 1.0}]
    set oracle_solid [format "oracle_%s.s" $unit]
    set oracle_comb [format "oracle_%s.c" $unit]
    units $unit
    put $oracle_solid ell V {0 0 0} \
	A [list [expr {4 * $oracle_scale}] 0 0] \
	B [list 0 [expr {3 * $oracle_scale}] 0] \
	C [list 0 0 [expr {2 * $oracle_scale}]]
    comb $oracle_comb u $oracle_solid
    Z
    e $oracle_solid
    center 0 0 0
    size $oracle_view_size
    setview 0 0 0
    sed $oracle_solid
    rset var transform e
    set drag_pixels $oracle_drag_pixels
    set initial_view [view_state]
    later ::mged::xmin::edit_interaction::oracle_solid_drag
}

proc ::mged::xmin::edit_interaction::oracle_solid_drag {} {
    variable oracle_solid
    variable oracle_expected
    variable oracle_scale
    variable oracle_view_size
    variable oracle_drag_pixels
    variable dm
    set move [expr {$oracle_view_size * $oracle_drag_pixels /
	double([winfo width $dm]) * $oracle_scale}]
    set oracle_expected [list $move $move 0]
    ::gui::test::require {[db get $oracle_solid V] eq {0 0 0}} \
	"solid oracle did not start at the origin"
    capture_before_edit
    drag shift
    later ::mged::xmin::edit_interaction::check_oracle_solid_drag
}

proc ::mged::xmin::edit_interaction::check_oracle_solid_drag {} {
    variable oracle_solid
    variable oracle_expected
    variable oracle_scale
    require_view_unchanged "solid unit-oracle drag"
    if {![image_changed "solid unit-oracle drag" \
	::mged::xmin::edit_interaction::check_oracle_solid_drag]} {
	return
    }
    require_near [solid_edit_vertex] $oracle_expected \
	"live solid edit" [expr {0.01 * $oracle_scale}]
    require_near [db get $oracle_solid V] {0 0 0} \
	"unaccepted solid" [expr {0.01 * $oracle_scale}]
    press accept
    require_near [db get $oracle_solid V] $oracle_expected \
	"accepted solid" [expr {0.01 * $oracle_scale}]
    sed $oracle_solid
    capture_before_edit
    drag shift
    later ::mged::xmin::edit_interaction::check_oracle_solid_reject
}

proc ::mged::xmin::edit_interaction::check_oracle_solid_reject {} {
    variable oracle_solid
    variable oracle_expected
    variable oracle_scale
    require_view_unchanged "solid rejected drag"
    if {![image_changed "solid rejected drag" \
	::mged::xmin::edit_interaction::check_oracle_solid_reject]} {
	return
    }
    require_near [db get $oracle_solid V] $oracle_expected \
	"solid before reject" [expr {0.01 * $oracle_scale}]
    press reject
    require_near [db get $oracle_solid V] $oracle_expected \
	"rejected solid" [expr {0.01 * $oracle_scale}]
    later ::mged::xmin::edit_interaction::setup_oracle_object
}

proc ::mged::xmin::edit_interaction::setup_oracle_object {} {
    variable oracle_solid
    variable oracle_comb
    variable oracle_view_size
    variable oracle_model2view
    variable initial_view
    Z
    draw $oracle_comb
    center 0 0 0
    size $oracle_view_size
    setview 0 0 0
    _mged_press oill
    _mged_ill -e -i 1 /$oracle_comb/$oracle_solid
    _mged_matpick 1
    ::gui::test::require {[status state] eq "OBJ EDIT"} \
	"could not enter object edit for unit oracle"
    rset var transform e
    set oracle_model2view [status_matrix model2view]
    set initial_view [view_state]
    later ::mged::xmin::edit_interaction::oracle_object_drag
}

proc ::mged::xmin::edit_interaction::oracle_object_drag {} {
    variable oracle_solid
    variable oracle_comb
    require_near [leaf_matrix $oracle_comb $oracle_solid] \
	[translation_matrix 0 0] "unaccepted combination" 0.000001
    capture_before_edit
    drag shift
    later ::mged::xmin::edit_interaction::check_oracle_object_drag
}

proc ::mged::xmin::edit_interaction::check_oracle_object_drag {} {
    variable oracle_solid
    variable oracle_comb
    variable oracle_expected
    variable oracle_model2view
    require_view_unchanged "object unit-oracle drag"
    if {![image_changed "object unit-oracle drag" \
	::mged::xmin::edit_interaction::check_oracle_object_drag]} {
	return
    }
    lassign $oracle_expected dx dy unused
    require_near [status_matrix model2objview] \
	[view_translation_matrix $oracle_model2view $dx $dy] \
	"live object edit" 0.01
    require_near [leaf_matrix $oracle_comb $oracle_solid] \
	[translation_matrix 0 0] "unaccepted object edit" 0.000001
    press accept
    require_near [leaf_matrix $oracle_comb $oracle_solid] \
	[translation_matrix $dx $dy] "accepted object edit" 0.01
    _mged_press oill
    _mged_ill -e -i 1 /$oracle_comb/$oracle_solid
    _mged_matpick 1
    capture_before_edit
    drag shift
    later ::mged::xmin::edit_interaction::check_oracle_object_reject
}

proc ::mged::xmin::edit_interaction::check_oracle_object_reject {} {
    variable oracle_solid
    variable oracle_comb
    variable oracle_expected
    require_view_unchanged "object rejected drag"
    if {![image_changed "object rejected drag" \
	::mged::xmin::edit_interaction::check_oracle_object_reject]} {
	return
    }
    lassign $oracle_expected dx dy unused
    require_near [leaf_matrix $oracle_comb $oracle_solid] \
	[translation_matrix $dx $dy] "object before reject" 0.01
    press reject
    require_near [leaf_matrix $oracle_comb $oracle_solid] \
	[translation_matrix $dx $dy] "rejected object edit" 0.01
    variable oracle_index
    variable oracle_units
    if {[incr oracle_index] < [llength $oracle_units]} {
	later ::mged::xmin::edit_interaction::setup_oracle_unit
    } else {
	later ::mged::xmin::edit_interaction::setup_transform_switch
    }
}

proc ::mged::xmin::edit_interaction::setup_transform_switch {} {
    global mged_players
    variable oracle_solid
    variable initial_view
    set id [lindex $mged_players 0]
    Z
    e $oracle_solid
    center 0 0 0
    size 40
    setview 0 0 0
    sed $oracle_solid
    press sxy
    rset var coords o
    set menu .$id.menubar.settings.transform
    $menu invoke [$menu index View]
    ::gui::test::require {[rset var transform] eq "v"} \
	"View transform menu did not select view manipulation"
    set initial_view [view_state]
    drag ctrl
    later ::mged::xmin::edit_interaction::check_view_transform
}

proc ::mged::xmin::edit_interaction::check_view_transform {} {
    variable initial_view
    variable oracle_expected
    variable oracle_scale
    require_view_changed "View transform mouse drag"
    require_near [solid_edit_vertex] $oracle_expected \
	"View transform altered the solid" [expr {0.01 * $oracle_scale}]
    set initial_view [view_state]
    drag shift
    later ::mged::xmin::edit_interaction::check_view_pan
}

proc ::mged::xmin::edit_interaction::check_view_pan {} {
    global mged_players
    variable initial_view
    variable oracle_expected
    variable oracle_scale
    require_view_changed "View transform mouse pan"
    require_near [solid_edit_vertex] $oracle_expected \
	"View pan altered the solid" [expr {0.01 * $oracle_scale}]
    set id [lindex $mged_players 0]
    set menu .$id.menubar.settings.transform
    $menu invoke [$menu index "Model Params"]
    ::gui::test::require {[rset var transform] eq "e"} \
	"Model Params menu did not select geometry editing"
    set initial_view [view_state]
    capture_before_edit
    drag shift
    later ::mged::xmin::edit_interaction::check_edit_transform
}

proc ::mged::xmin::edit_interaction::check_edit_transform {} {
    variable oracle_solid
    variable oracle_expected
    variable oracle_scale
    require_view_unchanged "Model Params mouse drag"
    if {![image_changed "Model Params mouse drag" \
	::mged::xmin::edit_interaction::check_edit_transform]} {
	return
    }
    require_near [db get $oracle_solid V] $oracle_expected \
	"unaccepted Model Params edit" [expr {0.01 * $oracle_scale}]
    press reject
    later ::mged::xmin::edit_interaction::setup_picker_fixture
}

proc ::mged::xmin::edit_interaction::setup_picker_fixture {} {
    global mged_players
    variable dm
    set id [lindex $mged_players 0]
    units mm
    put picker.s ell V {0 0 0} A {4 0 0} B {0 4 0} C {0 0 4}
    comb picker.c u picker.s
    Z
    draw picker.c
    center 0 0 0
    size 40
    setview 0 0 0
    set menu .$id.menubar.settings.mouse_behavior
    $menu invoke [$menu index "Pick Edit-Matrix"]
    exec $::env(GUI_TEST_CTL) click [winfo id $dm] \
	[expr {[winfo width $dm] / 2}] [expr {[winfo height $dm] / 2}] 2
    later ::mged::xmin::edit_interaction::wait_for_matrix_picker
}

proc ::mged::xmin::edit_interaction::wait_for_matrix_picker {} {
    global mged_players
    variable picker_attempts
    variable picker_limit
    set id [lindex $mged_players 0]
    set top .mgsp$id
    if {![winfo exists $top] || ![winfo ismapped $top]} {
	if {[incr picker_attempts] >= $picker_limit} {
	    error "Pick Edit-Matrix did not display a matrix list"
	}
	later ::mged::xmin::edit_interaction::wait_for_matrix_picker
	return
    }
    set listbox [::mged::gui::test::find_descendant $top Listbox]
    set box [$listbox bbox 1]
    if {[llength $box] != 4} {
	error "matrix picker did not offer the member transform"
    }
    lassign $box x y width height
    set x [expr {$x + $width / 2}]
    set y [expr {$y + $height / 2}]
    exec $::env(GUI_TEST_CTL) click [winfo id $listbox] $x $y 1
    after 100 [list ::mged::xmin::edit_interaction::checked \
	[list ::mged::xmin::edit_interaction::finish_matrix_picker \
	    $listbox $x $y]]
}

proc ::mged::xmin::edit_interaction::finish_matrix_picker {listbox x y} {
    exec $::env(GUI_TEST_CTL) click [winfo id $listbox] $x $y 1
    later ::mged::xmin::edit_interaction::check_matrix_picker
}

proc ::mged::xmin::edit_interaction::check_matrix_picker {} {
    global mged_players
    variable dm
    set id [lindex $mged_players 0]
    ::gui::test::require {[status state] eq "OBJ EDIT"} \
	"Pick Edit-Matrix did not enter object editing"
    ::gui::test::require {[rset var mouse_behavior] eq "d"} \
	"Pick Edit-Matrix did not restore default mouse behavior"
    press reject
    set menu .$id.menubar.settings.mouse_behavior
    $menu invoke [$menu index "Pick Edit-Combination"]
    exec $::env(GUI_TEST_CTL) click [winfo id $dm] \
	[expr {[winfo width $dm] / 2}] [expr {[winfo height $dm] / 2}] 2
    later ::mged::xmin::edit_interaction::check_combination_picker
}

proc ::mged::xmin::edit_interaction::check_combination_picker {} {
    global comb_control mged_players
    set id [lindex $mged_players 0]
    ::gui::test::require {
	[info exists comb_control($id,name)] &&
	$comb_control($id,name) eq "picker.c" &&
	[winfo exists .$id.comb]
    } "Pick Edit-Combination did not open the chosen combination"
    ::gui::test::require {[rset var mouse_behavior] eq "d"} \
	"Pick Edit-Combination did not restore default mouse behavior"
    later ::mged::xmin::edit_interaction::setup_bot_triangle_picker
}

proc ::mged::xmin::edit_interaction::setup_bot_triangle_picker {} {
    units mm
    in picker_bot.s bot 3 1 1 1 \
	0 0 0  10 0 0  0 10 0  0 1 2
    Z
    e picker_bot.s
    center 0 0 0
    size 40
    setview 0 0 0
    sed picker_bot.s
    press {Pick Triangle}
    M 1 307 307
    press {Move Triangle}
    M 1 512 512
    press accept
    set vertices [db get picker_bot.s V]
    require_near [lindex $vertices 0] {5 5 0} "BOT triangle V0"
    require_near [lindex $vertices 1] {15 5 0} "BOT triangle V1"
    require_near [lindex $vertices 2] {5 15 0} "BOT triangle V2"
    ::mged::gui::test::finish 0 \
	"PASS: MGED mouse routing, redraw, units, and picker modes"
}

after 25 [list ::mged::xmin::edit_interaction::checked \
    ::mged::xmin::edit_interaction::ready]

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
