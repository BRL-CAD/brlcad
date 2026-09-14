#           R T W I Z A R D _ X M I N _ G U I . T C L
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
if {![info exists ::env(XMIN_GUI_LIBRARY)] ||
    ![info exists ::env(RTWIZARD_TEST_DATABASE)]} {
    puts stderr "XMIN_GUI_LIBRARY and RTWIZARD_TEST_DATABASE are required"
    exit 2
}
source $::env(XMIN_GUI_LIBRARY)

namespace eval ::rtwizard::xmin {
    variable dialog_seen ""
    variable ready_retries 0
    variable dialog_poll_delay_ms 25
    variable ready_retry_limit 400
    variable retry_delay_ms 25
    variable startup_delay_ms 1500
    variable forced_port_probe false
}

proc ::rtwizard::xmin::finish {status message} {
    catch {destroy .}
    catch {update}
    lassign [::xmin::test::check_background_errors $status $message] \
	status message
    puts $message
    if {$status != 0} {
	puts stderr $message
    }
    exit $status
}

proc ::rtwizard::xmin::compare_manifest {actual} {
    if {![info exists ::env(RTWIZARD_MENU_MANIFEST)]} {
	return
    }
    set channel [open $::env(RTWIZARD_MENU_MANIFEST) r]
    set expected [string trim [read $channel]]
    close $channel
    if {$actual ne $expected} {
	::xmin::test::write menu_inventory_actual $actual
	error "live rtwizard menu inventory differs from its manifest"
    }
}

proc ::rtwizard::xmin::toplevels {} {
    set toplevels {}
    foreach widget [linsert [::xmin::test::descendants .] 0 .] {
	if {[winfo toplevel $widget] eq $widget} {
	    lappend toplevels $widget
	}
    }
    return [lsort -unique $toplevels]
}

proc ::rtwizard::xmin::dismiss_dialog {} {
    variable dialog_seen
    foreach widget [toplevels] {
	if {$widget eq "." || ![winfo ismapped $widget]} {
	    continue
	}
	set dialog_seen [wm title $widget]
	destroy $widget
	return
    }
    variable dialog_poll_delay_ms
    after $dialog_poll_delay_ms ::rtwizard::xmin::dismiss_dialog
}

proc ::rtwizard::xmin::exercise_dialog {labels expected_title} {
    variable dialog_seen
    variable dialog_poll_delay_ms
    set dialog_seen ""
    after $dialog_poll_delay_ms ::rtwizard::xmin::dismiss_dialog
    ::xmin::test::invoke_menu_entry . $labels
    ::xmin::test::require {$dialog_seen ne ""} \
	"menu entry did not display a dialog: [join $labels { > }]"
    if {$expected_title ne ""} {
	::xmin::test::require {$dialog_seen eq $expected_title} \
	    "unexpected dialog title '$dialog_seen', expected '$expected_title'"
    }
}

proc ::rtwizard::xmin::step_labels {} {
    set labels {}
    foreach entry [split [::xmin::test::menu_inventory .] "\n"] {
	if {[lindex $entry 0] eq "Steps" &&
	    [lindex $entry end-1] ne "separator"} {
	    lappend labels [lindex $entry 1]
	}
    }
    return [lsort -dictionary $labels]
}

proc ::rtwizard::xmin::set_framebuffer_entry {component variable value} {
    set control [$::fbp component $component]
    ::xmin::test::require {[winfo exists $control]} \
	"missing framebuffer-page component: $component"
    $control clear
    $control insert 0 $value
    update idletasks
    ::xmin::test::require {
	$::RtWizard::wizard_state($variable) eq $value
    } "framebuffer control $component did not update $variable"
}

proc ::rtwizard::xmin::exercise_framebuffer_controls {} {
    $::wizardInstance select fbp
    update

    set width 96
    set height 72
    foreach {component value} [list width $width height $height] {
	set control [$::fbp component $component]
	::xmin::test::require {[winfo exists $control]} \
	    "missing framebuffer-page component: $component"
	$control clear
	$control insert 0 $value
    }
    ::xmin::test::require {
	[$::fbp getWidth] == $width && [$::fbp getHeight] == $height
    } "framebuffer dimension controls did not retain their values"

    foreach {component variable value} {
	cutSteps cut_steps 12
	animationFps animation_fps 8
	cutDirection cut_direction {1 0 -1}
	aoSamples ao_samples 16
	aoRadius ao_radius 2.5
    } {
	set_framebuffer_entry $component $variable $value
    }

    set animation [$::fbp component cutAnimation]
    ::xmin::test::require {[winfo exists $animation]} \
	"missing framebuffer-page component: cutAnimation"
    set ::RtWizard::wizard_state(make_animation) 0
    $animation invoke
    ::xmin::test::require {$::RtWizard::wizard_state(make_animation) == 1} \
	"cut-animation control did not update its state"

    set radiobox [$::fbp component radBox]
    set filename [$::fbp component fileName]
    set browse [$::fbp component browse]
    $radiobox select toScreen
    ::xmin::test::require {
	[$filename cget -state] eq "disabled" &&
	[$browse cget -state] eq "disabled"
    } "screen output did not disable file controls"
    $radiobox select toFile
    ::xmin::test::require {
	[$filename cget -state] eq "normal" &&
	[$browse cget -state] eq "normal"
    } "file output did not enable file controls"
}

proc ::rtwizard::xmin::invoke_render_with_port_contention {} {
    variable forced_port_probe
    set forced_port_probe false
    rename ::rtwiz_port_occupied ::rtwizard::xmin::actual_port_occupied
    proc ::rtwiz_port_occupied {logical_port} {
	if {!$::rtwizard::xmin::forced_port_probe} {
	    set ::rtwizard::xmin::forced_port_probe true
	    return 1
	}
	return [::rtwizard::xmin::actual_port_occupied $logical_port]
    }
    try {
	::xmin::test::invoke_menu_entry . {Render Full-Size}
    } finally {
	rename ::rtwiz_port_occupied {}
	rename ::rtwizard::xmin::actual_port_occupied ::rtwiz_port_occupied
    }
    ::xmin::test::require {$forced_port_probe} \
	"rtwizard did not exercise its occupied framebuffer-port retry"
}

proc ::rtwizard::xmin::run {} {
    variable ready_retries
    variable ready_retry_limit
    variable retry_delay_ms
    if {![info exists ::wizardInstance] || $::wizardInstance eq "" ||
	![info exists ::RtWizard::wizard_state(gui_ready)] ||
	!$::RtWizard::wizard_state(gui_ready) || ![winfo ismapped .]} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    error "rtwizard did not finish constructing its GUI"
	}
	after $retry_delay_ms ::rtwizard::xmin::run_checked
	return
    }

    set expected_title "RtWizard - [file tail $::env(RTWIZARD_TEST_DATABASE)]"
    ::xmin::test::require {[wm title .] eq $expected_title} \
	"unexpected rtwizard main-window title"

    set widget_inventory [::xmin::test::widget_inventory .]
    set menu_inventory [::xmin::test::menu_inventory .]
    ::xmin::test::write widget_inventory $widget_inventory
    ::xmin::test::write menu_inventory $menu_inventory
    compare_manifest $menu_inventory

    foreach labels {
	{File {Write .rtwizardrc}}
	{Image {New Image...}}
	{Render Preview}
	{Render Full-Size}
	{Help Help...}
	{Help About...}
    } {
	::xmin::test::require {
	    [::xmin::test::find_menu_entry . $labels] ne ""
	} "missing rtwizard menu entry: [join $labels { > }]"
    }

    foreach {title pages expected_steps} {
	{Simple Full-Color Image} {fullColor} {{Configure Full-Color Elements}}
	{Simple Line Drawing} {lines} {{Configure Line-Drawing Elements}}
	{Highlighted Image} {highlighted} {{Configure Highlighted Elements}}
	{Mixed Full-Color and Edges} {fullColor lines} {
	    {Configure Full-Color Elements} {Configure Line-Drawing Elements}}
	{Ghost Image with Insert} {ghost fullColor} {
	    {Configure Ghost Elements} {Configure Full-Color Elements}}
	{Ghost Image with Insert and Lines} {ghost fullColor lines} {
	    {Configure Ghost Elements} {Configure Full-Color Elements}
	    {Configure Line-Drawing Elements}}
    } {
	::xmin::test::invoke_menu_entry . [list Image $title]
	::xmin::test::require {[$::exp getImageType] eq $title} \
	    "rtwizard did not select image type: $title"
	foreach page [concat {dbp fbp intro help exp} $pages] {
	    $::wizardInstance select $page
	    update
	}
	set expected_steps [concat $expected_steps \
	    {Greeting {Configure Framebuffer}}]
	set actual_steps [step_labels]
	::xmin::test::require {
	    $actual_steps eq [lsort -dictionary $expected_steps]
	} "unexpected Steps menu for '$title': $actual_steps"
    }

    exercise_framebuffer_controls

    exercise_dialog {Help About...} "About RtWizard"

    set config_file $::env(RTWIZARD_RCFILE)
    exercise_dialog {File {Write .rtwizardrc}} ""
    ::xmin::test::require {[file exists $config_file]} \
	"rtwizard did not create the requested configuration file"
    set channel [open $config_file r]
    set config [read $channel]
    close $channel
    foreach setting {wizard_width wizard_height gpane} {
	::xmin::test::require {
	    [string first "set ::$setting " $config] >= 0
	} "rtwizard configuration omitted $setting"
    }

    set render_dimension 64
    foreach component {width height} {
	set control [$::fbp component $component]
	$control clear
	$control insert 0 $render_dimension
    }
    [$::fbp component radBox] select toFile
    set ::RtWizard::wizard_state(make_animation) 0
    set rgb_bytes_per_pixel 3
    set expected_size [expr {$render_dimension * $render_dimension * $rgb_bytes_per_pixel}]
    ::xmin::test::require {
	[info exists ::RtWizard::wizard_state(verbose)]
    } "RaytraceWizard did not initialize its optional verbose state"
    set force_port_contention true
    foreach {title basename} {
	{Simple Full-Color Image} full-color
	{Simple Line Drawing} line
	{Highlighted Image} highlighted
	{Mixed Full-Color and Edges} mixed
	{Ghost Image with Insert} ghost
	{Ghost Image with Insert and Lines} ghost-line
    } {
	::xmin::test::invoke_menu_entry . [list Image $title]
	set render_file [file join $::env(XMIN_TEST_DIR) rtwizard-$basename.pix]
	set ::RtWizard::wizard_state(output_filename) $render_file
	if {$force_port_contention} {
	    invoke_render_with_port_contention
	    set force_port_contention false
	} else {
	    ::xmin::test::invoke_menu_entry . {Render Full-Size}
	}
	::xmin::test::require {[file exists $render_file]} \
	    "rtwizard '$title' render did not create its output file"
	::xmin::test::require {[file size $render_file] == $expected_size} \
	    "rtwizard '$title' render has an unexpected byte count"
    }

    set preview_dimension [expr {$render_dimension / 2}]
    set expected_preview_size [expr {
	$preview_dimension * $preview_dimension * $rgb_bytes_per_pixel
    }]
    set preview_file [file join $::env(XMIN_TEST_DIR) rtwizard-preview.pix]
    set ::RtWizard::wizard_state(output_filename) $preview_file
    ::xmin::test::invoke_menu_entry . {Render Preview}
    ::xmin::test::require {
	[file exists $preview_file] && [file size $preview_file] == $expected_preview_size
    } "rtwizard preview did not create the expected half-size image"

    $::wizardInstance select exp
    update
    finish 0 "PASS: rtwizard menus, pages, controls, preview, and all renderers"
}

proc ::rtwizard::xmin::run_checked {} {
    if {[catch {run} message options]} {
	puts stderr $message
	if {[dict exists $options -errorinfo]} {
	    puts stderr [dict get $options -errorinfo]
	}
	finish 1 "FAIL: rtwizard GUI regression"
    }
}

namespace eval ::RtWizard {}
set ::env(RTWIZARD_RCFILE) [file join $::env(XMIN_TEST_DIR) .rtwizardrc]
set ::RtWizard::wizard_state(dbFile) $::env(RTWIZARD_TEST_DATABASE)
foreach object_list {color_objlist ghost_objlist line_objlist} {
    set ::RtWizard::wizard_state($object_list) {all.g}
}
set argv {}
set argc 0
::xmin::test::capture_background_errors
after $::rtwizard::xmin::startup_delay_ms ::rtwizard::xmin::run_checked
package require RaytraceWizard

# RaytraceWizard normally remains in its event loop.  Reaching here means it
# returned without the test callback explicitly completing the process.
puts stderr "rtwizard exited before the GUI regression completed"
exit 1

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
