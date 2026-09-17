#                 X M I N _ G U I _ T E S T . T C L
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
# Shared lifecycle and interaction helpers for focused MGED Xmin fixtures.

if {![info exists ::env(GUI_TEST_LIBRARY)] ||
    ![info exists ::env(GUI_TEST_DIR)]} {
    puts stderr "GUI_TEST_LIBRARY and GUI_TEST_DIR are required"
    exit 2
}
source $::env(GUI_TEST_LIBRARY)
::gui::test::capture_background_errors

namespace eval ::mged::gui::test {
    variable dialog_error ""
    variable dialog_seen 0
    variable dialog_serial 0
    variable finished 0
    variable ready_retries 0
    variable ready_retry_limit 400
    variable retry_delay_ms 25
    variable settle_delay_ms 25
    variable settle_serial 0
}

proc ::mged::gui::test::fail {message} {
    error $message
}

proc ::mged::gui::test::settle {} {
    variable settle_delay_ms
    variable settle_serial
    set serial [incr settle_serial]
    after $settle_delay_ms [list set ::mged::gui::test::settle_serial $serial]
    vwait ::mged::gui::test::settle_serial
}

proc ::mged::gui::test::invoke {root labels} {
    ::gui::test::write progress "invoke: [join $labels { > }]"
    ::gui::test::invoke_menu_entry $root $labels
    settle
}

proc ::mged::gui::test::set_entry {entry value} {
    $entry delete 0 end
    $entry insert 0 $value
}

proc ::mged::gui::test::find_descendant {root class} {
    foreach widget [::gui::test::descendants $root] {
	if {[winfo class $widget] eq $class} {
	    return $widget
	}
    }
    fail "$root has no $class descendant"
}

proc ::mged::gui::test::find_widget_by_text {root text} {
    foreach widget [::gui::test::descendants $root] {
	if {![catch {$widget cget -text} label] && $label eq $text} {
	    return $widget
	}
    }
    fail "$root has no widget labeled '$text'"
}

proc ::mged::gui::test::find_toplevel_by_title {title} {
    foreach widget [winfo children .] {
	if {![catch {wm title $widget} widget_title] &&
	    $widget_title eq $title} {
	    return $widget
	}
    }
    fail "no toplevel has title '$title'"
}

proc ::mged::gui::test::require_mapped {widget description} {
    variable ready_retry_limit

    for {set attempt 0} {$attempt < $ready_retry_limit} {incr attempt} {
	update idletasks
	if {[winfo exists $widget] && [winfo ismapped $widget]} {
	    return
	}
	settle
    }
    fail "$description is not mapped"
}

proc ::mged::gui::test::return_file_dialog_path {path args} {
    return $path
}

proc ::mged::gui::test::with_file_dialog_path {command path script} {
    set saved_command ${command}_xmin_saved
    rename $command $saved_command
    interp alias {} $command {} \
	::mged::gui::test::return_file_dialog_path $path

    set status [catch {uplevel 1 $script} result options]
    rename $command {}
    rename $saved_command $command
    if {$status} {
	return -options $options $result
    }
    return $result
}

proc ::mged::gui::test::answer_dialog {
    serial path expected_title entry value button
} {
    variable dialog_error
    variable dialog_seen
    variable dialog_serial

    if {$serial != $dialog_serial} {
	return
    }
    if {![winfo exists $path] || ![winfo ismapped $path]} {
	after 25 [list ::mged::gui::test::answer_dialog $serial $path \
	    $expected_title $entry $value $button]
	return
    }

    set dialog_seen 1
    if {[wm title $path] ne $expected_title} {
	set dialog_error "unexpected dialog '[wm title $path]', expected '$expected_title'"
    }
    if {$entry ne ""} {
	$entry delete 0 end
	$entry insert 0 $value
    }
    if {![winfo exists $button]} {
	set dialog_error "$expected_title dialog has no requested button"
	destroy $path
	return
    }
    $button invoke
}

proc ::mged::gui::test::with_dialog_answer {
    path expected_title entry value button script
} {
    variable dialog_error
    variable dialog_seen
    variable dialog_serial

    set dialog_error ""
    set dialog_seen 0
    set serial [incr dialog_serial]
    after 25 [list ::mged::gui::test::answer_dialog $serial $path \
	$expected_title $entry $value $button]
    set status [catch {uplevel 1 $script} result options]
    incr dialog_serial

    if {$status} {
	return -options $options $result
    }
    if {!$dialog_seen} {
	fail "$expected_title dialog did not open"
    }
    if {$dialog_error ne ""} {
	fail $dialog_error
    }
    settle
    return $result
}

proc ::mged::gui::test::finish {status message} {
    variable finished
    if {$finished} {
	return
    }
    set finished 1
    catch {update}
    lassign [::gui::test::check_background_errors $status $message] \
	status message
    puts $message
    ::gui::test::write result $message
    if {$status != 0} {
	puts stderr $message
    }
    _mged_quit
}

proc ::mged::gui::test::run_checked {body pass_message failure_prefix} {
    global mged_gui mged_players
    variable ready_retries
    variable ready_retry_limit
    variable retry_delay_ms

    if {![info exists mged_players] || [llength $mged_players] == 0} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    finish 1 "FAIL: $failure_prefix: MGED did not create a GUI player"
	    return
	}
	after $retry_delay_ms [list ::mged::gui::test::run_checked \
	    $body $pass_message $failure_prefix]
	return
    }

    set id [lindex $mged_players 0]
    set top .$id
    if {![winfo exists $top] || ![winfo ismapped $top] ||
	![info exists mged_gui($id,active_dm)]} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    finish 1 "FAIL: $failure_prefix: MGED did not finish mapping its GUI"
	    return
	}
	after $retry_delay_ms [list ::mged::gui::test::run_checked \
	    $body $pass_message $failure_prefix]
	return
    }

    if {[catch {uplevel #0 [list $body $id $top]} message options]} {
	puts stderr $message
	if {[dict exists $options -errorinfo]} {
	    set error_info [dict get $options -errorinfo]
	    puts stderr $error_info
	    ::gui::test::write tcl_error_debug $error_info
	}
	finish 1 "FAIL: $failure_prefix: $message"
	return
    }
    finish 0 "PASS: $pass_message"
}

proc ::mged::gui::test::start {body pass_message failure_prefix} {
    after 25 [list ::mged::gui::test::run_checked \
	$body $pass_message $failure_prefix]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
