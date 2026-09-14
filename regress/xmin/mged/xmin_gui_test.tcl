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

if {![info exists ::env(XMIN_GUI_LIBRARY)] ||
    ![info exists ::env(XMIN_TEST_DIR)]} {
    puts stderr "XMIN_GUI_LIBRARY and XMIN_TEST_DIR are required"
    exit 2
}
source $::env(XMIN_GUI_LIBRARY)
::xmin::test::capture_background_errors

namespace eval ::mged::xmin::test {
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

proc ::mged::xmin::test::fail {message} {
    error $message
}

proc ::mged::xmin::test::settle {} {
    variable settle_delay_ms
    variable settle_serial
    set serial [incr settle_serial]
    after $settle_delay_ms [list set ::mged::xmin::test::settle_serial $serial]
    vwait ::mged::xmin::test::settle_serial
}

proc ::mged::xmin::test::invoke {root labels} {
    ::xmin::test::write progress "invoke: [join $labels { > }]"
    ::xmin::test::invoke_menu_entry $root $labels
    settle
}

proc ::mged::xmin::test::set_entry {entry value} {
    $entry delete 0 end
    $entry insert 0 $value
}

proc ::mged::xmin::test::find_descendant {root class} {
    foreach widget [::xmin::test::descendants $root] {
	if {[winfo class $widget] eq $class} {
	    return $widget
	}
    }
    fail "$root has no $class descendant"
}

proc ::mged::xmin::test::find_widget_by_text {root text} {
    foreach widget [::xmin::test::descendants $root] {
	if {![catch {$widget cget -text} label] && $label eq $text} {
	    return $widget
	}
    }
    fail "$root has no widget labeled '$text'"
}

proc ::mged::xmin::test::find_toplevel_by_title {title} {
    foreach widget [winfo children .] {
	if {![catch {wm title $widget} widget_title] &&
	    $widget_title eq $title} {
	    return $widget
	}
    }
    fail "no toplevel has title '$title'"
}

proc ::mged::xmin::test::require_mapped {widget description} {
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

proc ::mged::xmin::test::return_file_dialog_path {path args} {
    return $path
}

proc ::mged::xmin::test::with_file_dialog_path {command path script} {
    set saved_command ${command}_xmin_saved
    rename $command $saved_command
    interp alias {} $command {} \
	::mged::xmin::test::return_file_dialog_path $path

    set status [catch {uplevel 1 $script} result options]
    rename $command {}
    rename $saved_command $command
    if {$status} {
	return -options $options $result
    }
    return $result
}

proc ::mged::xmin::test::answer_dialog {
    serial path expected_title entry value button
} {
    variable dialog_error
    variable dialog_seen
    variable dialog_serial

    if {$serial != $dialog_serial} {
	return
    }
    if {![winfo exists $path] || ![winfo ismapped $path]} {
	after 25 [list ::mged::xmin::test::answer_dialog $serial $path \
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

proc ::mged::xmin::test::with_dialog_answer {
    path expected_title entry value button script
} {
    variable dialog_error
    variable dialog_seen
    variable dialog_serial

    set dialog_error ""
    set dialog_seen 0
    set serial [incr dialog_serial]
    after 25 [list ::mged::xmin::test::answer_dialog $serial $path \
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

proc ::mged::xmin::test::finish {status message} {
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

proc ::mged::xmin::test::run_checked {body pass_message failure_prefix} {
    global mged_gui mged_players
    variable ready_retries
    variable ready_retry_limit
    variable retry_delay_ms

    if {![info exists mged_players] || [llength $mged_players] == 0} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    finish 1 "FAIL: $failure_prefix: MGED did not create a GUI player"
	    return
	}
	after $retry_delay_ms [list ::mged::xmin::test::run_checked \
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
	after $retry_delay_ms [list ::mged::xmin::test::run_checked \
	    $body $pass_message $failure_prefix]
	return
    }

    if {[catch {uplevel #0 [list $body $id $top]} message options]} {
	puts stderr $message
	if {[dict exists $options -errorinfo]} {
	    set error_info [dict get $options -errorinfo]
	    puts stderr $error_info
	    ::xmin::test::write tcl_error_debug $error_info
	}
	finish 1 "FAIL: $failure_prefix: $message"
	return
    }
    finish 0 "PASS: $pass_message"
}

proc ::mged::xmin::test::start {body pass_message failure_prefix} {
    after 25 [list ::mged::xmin::test::run_checked \
	$body $pass_message $failure_prefix]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
