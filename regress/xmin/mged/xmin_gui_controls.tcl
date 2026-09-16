#           X M I N _ G U I _ C O N T R O L S . T C L
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
# Exercise stateful MGED control panels through their live Tk widgets.

if {![info exists ::env(XMIN_GUI_LIBRARY)] ||
    ![info exists ::env(XMIN_TEST_DIR)]} {
    puts stderr "XMIN_GUI_LIBRARY and XMIN_TEST_DIR are required"
    exit 2
}
source $::env(XMIN_GUI_LIBRARY)
::xmin::test::capture_background_errors

namespace eval ::mged::xmin {
    variable adc_distance_tolerance 0.1
    variable about_seen ""
    variable finished 0
    variable ready_retries 0
    variable ready_retry_limit 400
    variable retry_delay_ms 25
    variable settle_delay_ms 25
    variable settle_serial 0
    variable tolerance 0.001
}

proc ::mged::xmin::fail {message} {
    error $message
}

proc ::mged::xmin::require_near {actual expected description {allowed_delta ""}} {
    variable tolerance
    if {$allowed_delta eq ""} {
	set allowed_delta $tolerance
    }
    if {[llength $actual] != [llength $expected]} {
	fail "$description has [llength $actual] values, expected [llength $expected]: $actual"
    }
    foreach observed $actual requested $expected {
	if {![string is double -strict $observed] ||
	    abs($observed - $requested) > $allowed_delta} {
	    fail "$description is '$actual', expected '$expected'"
	}
    }
}

proc ::mged::xmin::settle {} {
    variable settle_delay_ms
    variable settle_serial
    set serial [expr {$settle_serial + 1}]
    after $settle_delay_ms [list set ::mged::xmin::settle_serial $serial]
    vwait ::mged::xmin::settle_serial
}

proc ::mged::xmin::view_state {} {
    set center_point [center]
    set view_size [size]
    set quaternion [view quat]
    return [concat $center_point $view_size $quaternion]
}

proc ::mged::xmin::set_view {azimuth elevation center_point view_size} {
    _mged_ae $azimuth $elevation
    _mged_center {*}$center_point
    _mged_size $view_size
    settle
    return [view_state]
}

proc ::mged::xmin::invoke {root labels} {
    ::xmin::test::write progress "invoke: [join $labels { > }]"
    ::xmin::test::invoke_menu_entry $root $labels
    settle
    ::xmin::test::write progress "invoked: [join $labels { > }]"
}

proc ::mged::xmin::exercise_view_ring {id top} {
    global mged_gui view_ring

    winset $mged_gui($id,active_dm)
    set first [set_view 10 20 {1 2 3} 100]
    invoke $top {ViewRing {Add View}}
    set second [set_view 40 30 {-4 5 6} 200]
    invoke $top {ViewRing {Add View}}
    set second_id $view_ring($id)
    ::xmin::test::require {[llength $mged_gui($id,views)] == 2} \
	"ViewRing did not store two views"

    set_view 0 0 {0 0 0} 50
    invoke $top {ViewRing {Prev View}}
    require_near [view_state] $first "ViewRing previous view"
    invoke $top {ViewRing {Next View}}
    require_near [view_state] $second "ViewRing next view"
    invoke $top {ViewRing {Last View}}
    require_near [view_state] $first "ViewRing last-view toggle"

    set select_menu $top.menubar.viewring.select
    $select_menu post 0 0
    $select_menu invoke end
    $select_menu unpost
    require_near [view_state] $second "ViewRing selected view"

    set delete_menu $top.menubar.viewring.delete
    $delete_menu invoke 0
    ::xmin::test::require {[llength $mged_gui($id,views)] == 1} \
	"ViewRing did not delete the selected stored view"
    ::xmin::test::require {$view_ring($id) == $second_id} \
	"ViewRing deletion left its selection on a deleted view"
    invoke $top {ViewRing {Next View}}
    require_near [view_state] $second "ViewRing traversal after deletion"
}

proc ::mged::xmin::exercise_grid {id top} {
    global grid_control mged_gui

    invoke $top {Tools {Grid Control Panel}}
    set panel $top.grid_control
    ::xmin::test::require {[winfo exists $panel] && [winfo ismapped $panel]} \
	"Grid Control Panel did not open"
    ::xmin::test::require {[wm title $panel] eq "Grid Control Panel ($id)"} \
	"Grid Control Panel has an unexpected title"

    foreach setting {anchor rh rv mrh mrv draw snap} {
	set original($setting) [rset grid $setting]
    }
    set grid_control($id,square) 0
    set grid_control($id,anchor) {1 2 3}
    set grid_control($id,rh) 2.5
    set grid_control($id,rv) 3.5
    set grid_control($id,mrh) 7
    set grid_control($id,mrv) 9
    set grid_control($id,draw) 1
    set grid_control($id,snap) 1
    $panel.applyB invoke

    require_near [rset grid anchor] {1 2 3} "grid anchor"
    require_near [rset grid rh] {2.5} "horizontal grid spacing"
    require_near [rset grid rv] {3.5} "vertical grid spacing"
    require_near [rset grid mrh] {7} "horizontal major-grid spacing"
    require_near [rset grid mrv] {9} "vertical major-grid spacing"
    ::xmin::test::require {
	[rset grid draw] == 1 && [rset grid snap] == 1 &&
	$mged_gui($id,grid_draw) == 1 && $mged_gui($id,grid_snap) == 1
    } "Grid Control Panel did not synchronize its render and GUI state"

    set grid_control($id,anchor) $original(anchor)
    set grid_control($id,rh) $original(rh)
    set grid_control($id,rv) $original(rv)
    set grid_control($id,mrh) $original(mrh)
    set grid_control($id,mrv) $original(mrv)
    set grid_control($id,draw) $original(draw)
    set grid_control($id,snap) $original(snap)
    $panel.applyB invoke
    $panel.resetB invoke
    $panel.dismissB invoke
    ::xmin::test::require {![winfo exists $panel]} \
	"Grid Control Panel did not dismiss"
}

proc ::mged::xmin::exercise_adc {id top} {
    global mged_adc_control
    variable adc_distance_tolerance

    invoke $top {Tools {ADC Control Panel}}
    set panel $top.adc_control
    ::xmin::test::require {[winfo exists $panel] && [winfo ismapped $panel]} \
	"ADC Control Panel did not open"
    ::xmin::test::require {[wm title $panel] eq "ADC Control Panel ($id)"} \
	"ADC Control Panel has an unexpected title"
    set reset_position [adc xyz]

    set mged_adc_control($id,coords) model
    set mged_adc_control($id,interpval) abs
    adc_adjust_coords $id
    adc_interpval $id
    set mged_adc_control($id,pos) {1 2 3}
    set mged_adc_control($id,dst) 4.5
    set mged_adc_control($id,a1) 15
    set mged_adc_control($id,a2) 75
    set mged_adc_control($id,anchor_pos) 0
    set mged_adc_control($id,anchor_dst) 0
    set mged_adc_control($id,anchor_a1) 0
    set mged_adc_control($id,anchor_a2) 0
    set mged_adc_control($id,draw) 1
    $panel.applyB invoke

    require_near [adc xyz] {1 2 3} "ADC model position"
    require_near [adc dst] {4.5} "ADC tick distance" \
	$adc_distance_tolerance
    require_near [adc a1] {15} "ADC first angle"
    require_near [adc a2] {75} "ADC second angle"
    ::xmin::test::require {[adc draw] == 1} \
	"ADC Control Panel did not enable the cursor"

    $panel.resetB invoke
    require_near [adc xyz] $reset_position "reset ADC position"
    ::xmin::test::require {
	[adc draw] == 1 && $mged_adc_control($id,draw) == 1
    } "ADC Control Panel reset did not preserve and reload the draw setting"
    set mged_adc_control($id,draw) 0
    $panel.applyB invoke
    $panel.dismissB invoke
    ::xmin::test::require {![winfo exists $panel]} \
	"ADC Control Panel did not dismiss"
}

proc ::mged::xmin::exercise_font_persistence {id top} {
    global mged_default

    invoke $top {File Preferences Fonts}
    set panel $top.font_scheme
    ::xmin::test::require {[winfo exists $panel] && [winfo ismapped $panel]} \
	"Fonts panel did not open"
    ::xmin::test::require {[wm title $panel] eq "Fonts"} \
	"Fonts panel has an unexpected title"

    set text_menu $panel._TextMB.menu
    set font_index [$text_menu index {courier 18}]
    ::xmin::test::require {$font_index ne "none"} \
	"Fonts panel does not offer courier 18"
    $text_menu invoke $font_index
    $panel.applyB invoke

    ::xmin::test::require {[font configure text_font -size] == 18} \
	"Fonts panel did not apply the text-font size"
    ::xmin::test::require {[dict get $mged_default(text_font) -size] == 18} \
	"Fonts panel did not update the persisted text-font setting"

    invoke $top {File {Create/Update .mgedrc}}
    set rcfile [file join $::env(XMIN_TEST_DIR) .mgedrc]
    ::xmin::test::require {[file exists $rcfile]} \
	"Create/Update .mgedrc did not create the isolated preferences file"
    set channel [open $rcfile r]
    set contents [read $channel]
    close $channel
    set expected [list set mged_default(text_font) $mged_default(text_font)]
    ::xmin::test::require {[lsearch -exact [split $contents \n] $expected] >= 0} \
	".mgedrc did not contain the applied text-font setting"

    $panel.dismissB invoke
    ::xmin::test::require {![winfo exists $panel]} \
	"Fonts panel did not dismiss"
}

proc ::mged::xmin::dismiss_about {expected_title} {
    variable about_seen
    foreach widget [linsert [::xmin::test::descendants .] 0 .] {
	if {[winfo toplevel $widget] ne $widget ||
	    ![winfo ismapped $widget] || [wm title $widget] ne $expected_title} {
	    continue
	}
	set about_seen [wm title $widget]
	if {![winfo exists $widget.bot.button0]} {
	    fail "About MGED dialog has no dismissal button"
	}
	$widget.bot.button0 invoke
	return
    }
    after 25 [list ::mged::xmin::dismiss_about $expected_title]
}

proc ::mged::xmin::exercise_about {top} {
    variable about_seen
    set expected_title {About MGED...}
    set about_seen ""
    after 25 [list ::mged::xmin::dismiss_about $expected_title]
    invoke $top {Help {About MGED}}
    ::xmin::test::require {$about_seen eq $expected_title} \
	"About MGED did not display its dialog"
}

proc ::mged::xmin::finish {status message} {
    variable finished
    if {$finished} {
	return
    }
    set finished 1
    catch {update}
    lassign [::xmin::test::check_background_errors $status $message] \
	status message
    puts $message
    ::xmin::test::write result $message
    if {$status != 0} {
	puts stderr $message
    }
    _mged_quit
}

proc ::mged::xmin::run {} {
    global mged_gui mged_players
    variable ready_retries
    variable ready_retry_limit
    variable retry_delay_ms

    if {![info exists mged_players] || [llength $mged_players] == 0} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    fail "MGED did not create a GUI player"
	}
	after $retry_delay_ms ::mged::xmin::run_checked
	return
    }
    set id [lindex $mged_players 0]
    set top .$id
    if {![winfo exists $top] || ![winfo ismapped $top] ||
	![info exists mged_gui($id,active_dm)]} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    fail "MGED did not finish mapping its GUI"
	}
	after $retry_delay_ms ::mged::xmin::run_checked
	return
    }

    set versions {}
    foreach {package minimum} {Itcl 4.3.0 Itk 4.2.3 Iwidgets 4.1.1} {
	set version [package require $package $minimum]
	lappend versions "$package $version"
    }
    ::xmin::test::write package_versions [join $versions \n]
    set ::env(HOME) $::env(XMIN_TEST_DIR)

    exercise_view_ring $id $top
    exercise_grid $id $top
    exercise_adc $id $top
    exercise_font_persistence $id $top
    exercise_about $top
    finish 0 "PASS: MGED ViewRing, grid, ADC, fonts, preferences, and About controls"
}

proc ::mged::xmin::run_checked {} {
    if {[catch {run} message options]} {
	puts stderr $message
	if {[dict exists $options -errorinfo]} {
	    set error_info [dict get $options -errorinfo]
	    puts stderr $error_info
	    ::xmin::test::write tcl_error_debug $error_info
	}
	finish 1 "FAIL: MGED control-panel regression: $message"
    }
}

after 25 ::mged::xmin::run_checked

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
