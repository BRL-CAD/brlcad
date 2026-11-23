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
if {![info exists ::env(GUI_TEST_LIBRARY)] ||
    ![info exists ::env(RTWIZARD_TEST_DATABASE)]} {
    puts stderr "GUI_TEST_LIBRARY and RTWIZARD_TEST_DATABASE are required"
    exit 2
}
source $::env(GUI_TEST_LIBRARY)

namespace eval ::rtwizard::xmin {
    variable dialog_seen ""
    variable ready_retries 0
    variable dialog_poll_delay_ms 25
    variable ready_retry_limit 400
    variable retry_delay_ms 25
    variable startup_delay_ms 1500
}

proc ::rtwizard::xmin::finish {status message} {
    catch {destroy .}
    catch {update}
    lassign [::gui::test::check_background_errors $status $message] \
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
	::gui::test::write menu_inventory_actual $actual
	error "live rtwizard menu inventory differs from its manifest"
    }
}

proc ::rtwizard::xmin::toplevels {} {
    set toplevels {}
    foreach widget [linsert [::gui::test::descendants .] 0 .] {
	if {[winfo toplevel $widget] eq $widget} {
	    lappend toplevels $widget
	}
    }
    return [lsort -unique $toplevels]
}

proc ::rtwizard::xmin::dismiss_dialog {} {
    variable dialog_seen
    foreach widget [toplevels] {
	# The startup feedback window can still be mapped when an alert opens.
	if {![winfo ismapped $widget] || [winfo class $widget] ne "Dialog"} {
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
    ::gui::test::invoke_menu_entry . $labels
    ::gui::test::require {$dialog_seen ne ""} \
	"menu entry did not display a dialog: [join $labels { > }]"
    if {$expected_title ne ""} {
	::gui::test::require {$dialog_seen eq $expected_title} \
	    "unexpected dialog title '$dialog_seen', expected '$expected_title'"
    }
}

proc ::rtwizard::xmin::step_labels {} {
    set labels {}
    foreach entry [split [::gui::test::menu_inventory .] "\n"] {
	if {[lindex $entry 0] eq "Steps" &&
	    [lindex $entry end-1] ne "separator"} {
	    lappend labels [lindex $entry 1]
	}
    }
    return [lsort -dictionary $labels]
}

proc ::rtwizard::xmin::set_framebuffer_entry {component variable value} {
    set control [$::fbp component $component]
    ::gui::test::require {[winfo exists $control]} \
	"missing framebuffer-page component: $component"
    $control clear
    $control insert 0 $value
    update idletasks
    ::gui::test::require {
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
	::gui::test::require {[winfo exists $control]} \
	    "missing framebuffer-page component: $component"
	$control clear
	$control insert 0 $value
    }
    ::gui::test::require {
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
    ::gui::test::require {[winfo exists $animation]} \
	"missing framebuffer-page component: cutAnimation"
    set ::RtWizard::wizard_state(make_animation) 0
    $animation invoke
    ::gui::test::require {$::RtWizard::wizard_state(make_animation) == 1} \
	"cut-animation control did not update its state"

    set radiobox [$::fbp component radBox]
    set filename [$::fbp component fileName]
    set browse [$::fbp component browse]
    $radiobox select toScreen
    ::gui::test::require {
	[$filename cget -state] eq "disabled" &&
	[$browse cget -state] eq "disabled"
    } "screen output did not disable file controls"
    $radiobox select toFile
    ::gui::test::require {
	[$filename cget -state] eq "normal" &&
	[$browse cget -state] eq "normal"
    } "file output did not enable file controls"
}

proc ::rtwizard::xmin::check_process_timeout {} {
    rename ::rtwizard_render ::rtwizard::xmin::native_render
    proc ::rtwizard_render {type settings} {
	return [dict get $settings _process_timeout]
    }
    try {
	set timeout [::RtWizard::render A {}]
	::gui::test::require {$timeout == 7} "render did not receive the CLI process timeout"
    } finally {
	rename ::rtwizard_render {}
	rename ::rtwizard::xmin::native_render ::rtwizard_render
    }
}

proc ::rtwizard::xmin::wait_for_preview {condition message} {
    variable ready_retry_limit
    variable retry_delay_ms
    for {set i 0} {$i < $ready_retry_limit} {incr i} {
	update
	if {[uplevel 1 [list expr $condition]]} {return}
	after $retry_delay_ms
    }
    error $message
}

proc ::rtwizard::xmin::exercise_animation_preview {title dimension output} {
    set win .rtwizard_animation_preview
    upvar #0 ::RtWizard::AnimationPreview::state preview
    ::gui::test::invoke_menu_entry . {Render Preview}
    ::gui::test::require {[winfo exists $win]} "missing animation player for '$title'"
    wait_for_preview {!$preview(running)} "animation preview did not finish for '$title'"
    ::gui::test::require {
	$preview(status) eq "Preview ready (3 frames)" &&
	[image width $preview(photo)] == $dimension &&
	[image height $preview(photo)] == $dimension
    } "unexpected '$title' animation preview: $preview(status)"

    $win.controls.play invoke
    $win.frame set 1
    update idletasks
    set first [$preview(photo) data]
    $win.frame set 3
    update idletasks
    ::gui::test::require {$preview(frame) == 3 && $first ne [$preview(photo) data]} \
	"scrubbing did not display a different frame for '$title'"
    $win.controls.play invoke
    wait_for_preview {$preview(frame) != 3} "playback did not advance or loop for '$title'"
    set directory $preview(directory)
    $win.controls.close invoke
    ::gui::test::require {![winfo exists $win] && ![file exists $directory]} \
	"closing the '$title' animation preview did not clean up"
    ::gui::test::require {![file exists $output]} "preview published the final animation for '$title'"
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
    ::gui::test::require {[wm title .] eq $expected_title} \
	"unexpected rtwizard main-window title"

    check_process_timeout

    set widget_inventory [::gui::test::widget_inventory .]
    set menu_inventory [::gui::test::menu_inventory .]
    ::gui::test::write widget_inventory $widget_inventory
    ::gui::test::write menu_inventory $menu_inventory
    compare_manifest $menu_inventory

    foreach labels {
	{File {Write .rtwizardrc}}
	{Image {New Image...}}
	{Render Preview}
	{Render Full-Size}
	{Help Help...}
	{Help About...}
    } {
	::gui::test::require {
	    [::gui::test::find_menu_entry . $labels] ne ""
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
	::gui::test::invoke_menu_entry . [list Image $title]
	::gui::test::require {[$::exp getImageType] eq $title} \
	    "rtwizard did not select image type: $title"
	foreach page [concat {dbp fbp intro help exp} $pages] {
	    $::wizardInstance select $page
	    update
	}
	set expected_steps [concat $expected_steps \
	    {Greeting {Configure Framebuffer}}]
	set actual_steps [step_labels]
	::gui::test::require {
	    $actual_steps eq [lsort -dictionary $expected_steps]
	} "unexpected Steps menu for '$title': $actual_steps"
    }

    exercise_framebuffer_controls

    exercise_dialog {Help About...} "About RtWizard"

    set config_file $::env(RTWIZARD_RCFILE)
    exercise_dialog {File {Write .rtwizardrc}} ""
    ::gui::test::require {[file exists $config_file]} \
	"rtwizard did not create the requested configuration file"
    set channel [open $config_file r]
    set config [read $channel]
    close $channel
    foreach setting {wizard_width wizard_height gpane} {
	::gui::test::require {
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
    set animation [$::fbp component cutAnimation]
    set preset [$::fbp component animationPreset]
    set_framebuffer_entry cutSteps cut_steps 3
    set_framebuffer_entry animationFps animation_fps 24
    set_framebuffer_entry turntableObject turntable_object arb8_quadrants.c
    set rgb_bytes_per_pixel 3
    set expected_size [expr {$render_dimension * $render_dimension * $rgb_bytes_per_pixel}]
    set preview_dimension [expr {$render_dimension / 2}]
    ::gui::test::require {
	[info exists ::RtWizard::wizard_state(verbose)]
    } "RaytraceWizard did not initialize its optional verbose state"
    ::gui::test::require {
	[llength [info commands rtwizard_render]] == 1
    } "rtwizard did not register its native renderer"
    foreach {title basename} {
	{Simple Full-Color Image} full-color
	{Simple Line Drawing} line
	{Highlighted Image} highlighted
	{Mixed Full-Color and Edges} mixed
	{Ghost Image with Insert} ghost
	{Ghost Image with Insert and Lines} ghost-line
    } {
	::gui::test::invoke_menu_entry . [list Image $title]
	set render_file [file join $::env(GUI_TEST_DIR) rtwizard-$basename.pix]
	set old_output [open $render_file w]
	puts $old_output "previous image"
	close $old_output
	set_framebuffer_entry fileName output_filename $render_file
	::gui::test::invoke_menu_entry . {Render Full-Size}
	::gui::test::require {[file exists $render_file]} \
	    "rtwizard '$title' render did not create its output file"
	::gui::test::require {[file size $render_file] == $expected_size} \
	    "rtwizard '$title' render has an unexpected byte count"

	$animation invoke
	foreach motion {cut orbit turntable} {
	    [$preset cget -menu] invoke $motion
	    ::gui::test::require {$::RtWizard::wizard_state(animation_preset) eq $motion} \
		"animation menu did not select $motion"
	    set render_file [file join $::env(GUI_TEST_DIR) rtwizard-$basename-$motion.png]
	    set_framebuffer_entry fileName output_filename $render_file
	    exercise_animation_preview "$title / $motion" $preview_dimension $render_file
	    ::gui::test::invoke_menu_entry . {Render Full-Size}
	    set prefix [file join $::env(GUI_TEST_DIR) rtwizard-$basename-$motion-frame]
	    exec [file join [bu_dir bin] icv] anim extract $render_file $prefix
	    set files [lsort [glob -- "$prefix*.png"]]
	    ::gui::test::require {[llength $files] == 3} \
	        "rtwizard '$title' $motion did not render all three animation frames"
	    set channel [open [lindex $files 0] rb]
	    set first [read $channel]
	    close $channel
	    set channel [open [lindex $files end] rb]
	    set last [read $channel]
	    close $channel
	    ::gui::test::require {$first ne $last} "rtwizard '$title' $motion animation did not move"
	}
	$animation invoke
    }

    set expected_preview_size [expr {
	$preview_dimension * $preview_dimension * $rgb_bytes_per_pixel
    }]
    set preview_file [file join $::env(GUI_TEST_DIR) rtwizard-preview.pix]
    set ::RtWizard::wizard_state(output_filename) $preview_file
    ::gui::test::invoke_menu_entry . {Render Preview}
    ::gui::test::require {
	[file exists $preview_file] && [file size $preview_file] == $expected_preview_size
    } "rtwizard preview did not create the expected half-size image"

    $animation invoke
    set_framebuffer_entry fileName output_filename ""
    set_framebuffer_entry cutSteps cut_steps 120
    set win .rtwizard_animation_preview
    upvar #0 ::RtWizard::AnimationPreview::state preview
    ::gui::test::invoke_menu_entry . {Render Preview}
    wait_for_preview {$preview(ready) > 0} "no preview frame arrived before cancellation"
    ::gui::test::require {$preview(running)} "preview completed before cancellation could be exercised"
    $win.controls.cancel invoke
    wait_for_preview {!$preview(running)} "cancelled animation preview did not stop"
    ::gui::test::require {
	$preview(status) eq "Preview cancelled" && $preview(ready) < 120 &&
	[image width $preview(photo)] == $preview_dimension
    } "cancelling the preview did not stop rendering and retain completed frames"
    set directory $preview(directory)
    $win.controls.close invoke
    ::gui::test::require {![file exists $directory]} "cancelled preview left temporary frames"
    $animation invoke

    set ::RtWizard::wizard_state(output_filename) ""
    [$::fbp component radBox] select toScreen
    ::gui::test::invoke_menu_entry . {Render Preview}

    $::wizardInstance select exp
    update
    finish 0 "PASS: rtwizard menus, controls, preview, and stills and animations for all six picture types"
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
set ::env(RTWIZARD_RCFILE) [file join $::env(GUI_TEST_DIR) .rtwizardrc]
set ::RtWizard::wizard_state(dbFile) $::env(RTWIZARD_TEST_DATABASE)
foreach {role objects} {
    color {groundplane.r arb8_quadrants.c}
    ghost {arb8_axes.c}
    line {arb8_quadrants.c}
} {
    set ::RtWizard::wizard_state(${role}_objlist) $objects
}
set argv {}
set argc 0
::gui::test::capture_background_errors
after $::rtwizard::xmin::startup_delay_ms ::rtwizard::xmin::run_checked
if {[llength [info commands rtwizard_render]] != 1} {
    puts stderr "rtwizard did not register its native renderer"
    exit 1
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
