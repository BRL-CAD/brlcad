#        X M I N _ G U I _ S E A R C H _ E X E C . T C L
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
# Exercise asynchronous search -exec drawing, serialization, and interruption.

if {![info exists ::env(XMIN_GUI_LIBRARY)] ||
    ![info exists ::env(XMIN_TEST_DIR)]} {
    puts stderr "XMIN_GUI_LIBRARY and XMIN_TEST_DIR are required"
    exit 2
}
source $::env(XMIN_GUI_LIBRARY)
::xmin::test::capture_background_errors

namespace eval ::mged::xmin::search_exec {
    variable command_widget ""
    variable covered_draw_time_limit_ms 5000
    variable draw_time_limit_ms 15000
    variable display_widget ""
    variable finished 0
    variable interrupt_requested 0
    variable minimum_tgc_paths 1000
    variable nested_done 0
    variable nested_error_code {}
    variable nested_message ""
    variable nested_status 0
    variable ready_retries 0
    variable ready_retry_limit 800
    variable retry_delay_ms 25
}

proc ::mged::xmin::search_exec::fail {message} {
    error $message
}

proc ::mged::xmin::search_exec::nested_search {} {
    variable display_widget
    variable nested_done
    variable nested_error_code
    variable nested_message
    variable nested_status

    event generate $display_widget <Enter>
    set nested_status [catch {
	_mged_search / -type tgc -exec ls "{}" ";"
    } nested_message nested_options]
    if {[dict exists $nested_options -errorcode]} {
	set nested_error_code [dict get $nested_options -errorcode]
    }
    set nested_done 1
}

proc ::mged::xmin::search_exec::request_interrupt {} {
    variable command_widget
    variable interrupt_requested

    event generate $command_widget <Control-c>
    set interrupt_requested 1
}

proc ::mged::xmin::search_exec::exercise {} {
    variable covered_draw_time_limit_ms
    variable draw_time_limit_ms
    variable interrupt_requested
    variable minimum_tgc_paths
    variable nested_done
    variable nested_error_code
    variable nested_message
    variable nested_status

    set paths [_mged_search / -type tgc]
    ::xmin::test::require {[llength $paths] >= $minimum_tgc_paths} \
	"m35.g supplied too few TGC paths for the scaling regression"

    set nested_done 0
    set nested_error_code {}
    set nested_message ""
    set nested_status 0
    after 25 ::mged::xmin::search_exec::nested_search
    set started [clock milliseconds]
    set draw_status [catch {
	_mged_search / -type tgc -exec draw "{}" ";"
    } draw_message]
    set draw_elapsed [expr {[clock milliseconds] - $started}]

    ::xmin::test::require {$draw_status == 0} \
	"search -exec draw failed: $draw_message"
    ::xmin::test::require {$nested_done} \
	"the event loop did not service the nested search probe"
    ::xmin::test::require {
	$nested_status == 1 &&
	$nested_message eq "another MGED command is already running" &&
	$nested_error_code eq {BRLCAD MGED COMMAND_BUSY}
    } "nested search was not rejected safely: $nested_message"
    set draw_limit_message [format \
	"search -exec draw took %dms; expected less than %dms" \
	$draw_elapsed $draw_time_limit_ms]
    ::xmin::test::require {$draw_elapsed < $draw_time_limit_ms} \
	$draw_limit_message
    ::xmin::test::require {[llength [_mged_who]] >= $minimum_tgc_paths} \
	"search -exec draw did not populate the display list"

    set redraw_started [clock milliseconds]
    set redraw_status [catch {
	_mged_search / -type tgc -exec draw "{}" ";"
    } redraw_message]
    set redraw_elapsed [expr {[clock milliseconds] - $redraw_started}]
    ::xmin::test::require {$redraw_status == 0} \
	"repeated search -exec draw failed: $redraw_message"
    set redraw_limit_message [format \
	"repeated search -exec draw took %dms; expected less than %dms" \
	$redraw_elapsed $draw_time_limit_ms]
    ::xmin::test::require {$redraw_elapsed < $draw_time_limit_ms} \
	$redraw_limit_message

    _mged_Z
    set interrupt_requested 0
    after 25 ::mged::xmin::search_exec::request_interrupt
    set interrupt_status [catch {
	_mged_search / -type tgc -exec draw "{}" ";"
    } interrupt_message]

    ::xmin::test::require {$interrupt_requested == 1} \
	"the running search did not accept an interrupt request"
    ::xmin::test::require {
	$interrupt_status == 1 && $interrupt_message eq "Command interrupted."
    } "interrupted search returned an unexpected result: $interrupt_message"

    set followup_path [lindex $paths 0]
    set path_components [split [string trimleft $followup_path /] /]
    set ancestor [lindex $path_components 0]
    set followup_status [catch {
	_mged_search $followup_path -type tgc -exec ls "{}" ";"
    } followup_message]
    ::xmin::test::require {$followup_status == 0} \
	"search -exec failed after interruption: $followup_message"

    _mged_Z
    _mged_draw $ancestor
    set ancestor_display [_mged_who]
    _mged_draw $followup_path
    ::xmin::test::require {[_mged_who] eq $ancestor_display} \
	"drawing a child path duplicated its displayed ancestor"

    _mged_Z
    _mged_draw [lindex $paths 0]
    _mged_draw [lindex $paths 1]
    _mged_draw $ancestor
    set consolidated_display [_mged_who]
    ::xmin::test::require {
	[llength $consolidated_display] == 1 &&
	[lindex $consolidated_display 0] eq $ancestor
    } "drawing an ancestor did not replace its displayed descendants"

    set component_paths [_mged_search /component -type tgc]
    ::xmin::test::require {
	[llength $component_paths] >= $minimum_tgc_paths
    } "component supplied too few TGC paths for the overlap regression"

    _mged_Z
    _mged_draw component
    set covered_started [clock milliseconds]
    set covered_status [catch {
	_mged_search /component -type tgc -exec draw "{}" ";"
    } covered_message]
    set covered_elapsed [expr {[clock milliseconds] - $covered_started}]
    ::xmin::test::require {$covered_status == 0} \
	"drawing paths under component failed: $covered_message"
    set covered_limit_message [format \
	"drawing paths under component took %dms; expected less than %dms" \
	$covered_elapsed $covered_draw_time_limit_ms]
    ::xmin::test::require {$covered_elapsed < $covered_draw_time_limit_ms} \
	$covered_limit_message
    set covered_display [_mged_who]
    ::xmin::test::require {
	[llength $covered_display] == 1 &&
	[lindex $covered_display 0] eq "component"
    } "drawing covered paths changed the component display root"

    set metrics "tgc_paths [llength $paths]\ndraw_elapsed_ms $draw_elapsed"
    append metrics "\nredraw_elapsed_ms $redraw_elapsed"
    append metrics "\ncovered_elapsed_ms $covered_elapsed"
    puts "MGED search-exec: [llength $paths] paths, first $draw_elapsed ms, \
	redraw $redraw_elapsed ms"
    ::xmin::test::write search_exec_metrics $metrics
}

proc ::mged::xmin::search_exec::finish {status message} {
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

proc ::mged::xmin::search_exec::run {} {
    global mged_gui mged_players
    variable command_widget
    variable display_widget
    variable ready_retries
    variable ready_retry_limit
    variable retry_delay_ms

    set ready 1
    if {![info exists mged_players] || [llength $mged_players] == 0} {
	set ready 0
    } else {
	set id [lindex $mged_players 0]
	set top .$id
	if {![winfo exists $top] || ![winfo ismapped $top] ||
	    ![info exists mged_gui($id,active_dm)]} {
	    set ready 0
	}
    }

    if {$ready} {
	set idle_status [catch {_mged_who} idle_message]
	if {$idle_status &&
	    $idle_message eq "another MGED command is already running"} {
	    set ready 0
	} elseif {$idle_status} {
	    fail "could not query MGED command state: $idle_message"
	}
    }

    if {!$ready} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    fail "MGED did not finish GUI and command initialization"
	}
	after $retry_delay_ms ::mged::xmin::search_exec::run_checked
	return
    }

    set command_widget $top.t
    set display_widget $mged_gui($id,active_dm)
    exercise
    finish 0 "PASS: MGED search drawing is linear, serialized, and interruptible"
}

proc ::mged::xmin::search_exec::run_checked {} {
    if {[catch {run} message options]} {
	if {[dict exists $options -errorinfo]} {
	    ::xmin::test::write tcl_error_debug [dict get $options -errorinfo]
	}
	finish 1 "FAIL: MGED search -exec regression: $message"
    }
}

after 25 ::mged::xmin::search_exec::run_checked

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
