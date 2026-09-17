#          X M I N _ G U I _ W O R K F L O W S . T C L
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
# Exercise MGED creation, browsing, and collaboration workflows through live
# Tk controls and verify their database or display-state results.

if {![info exists ::env(GUI_TEST_LIBRARY)] ||
    ![info exists ::env(GUI_TEST_DIR)]} {
    puts stderr "GUI_TEST_LIBRARY and GUI_TEST_DIR are required"
    exit 2
}
source $::env(GUI_TEST_LIBRARY)
::gui::test::capture_background_errors

namespace eval ::mged::xmin::workflows {
    variable dialog_seen 0
    variable dialog_serial 0
    variable dialog_error ""
    variable finished 0
    variable ready_retries 0
    variable ready_retry_limit 400
    variable retry_delay_ms 25
    variable settle_delay_ms 25
    variable settle_serial 0
    variable tolerance 0.001
    variable watching_dialog 0
}

proc ::mged::xmin::workflows::fail {message} {
    error $message
}

proc ::mged::xmin::workflows::settle {} {
    variable settle_delay_ms
    variable settle_serial
    set serial [expr {$settle_serial + 1}]
    after $settle_delay_ms [list set ::mged::xmin::workflows::settle_serial $serial]
    vwait ::mged::xmin::workflows::settle_serial
}

proc ::mged::xmin::workflows::require_near {actual expected description} {
    variable tolerance
    if {[llength $actual] != [llength $expected]} {
	fail "$description has [llength $actual] values, expected [llength $expected]: $actual"
    }
    foreach observed $actual requested $expected {
	if {![string is double -strict $observed] ||
	    abs($observed - $requested) > $tolerance} {
	    fail "$description is '$actual', expected '$expected'"
	}
    }
}

proc ::mged::xmin::workflows::invoke {root labels} {
    ::gui::test::write progress "invoke: [join $labels { > }]"
    ::gui::test::invoke_menu_entry $root $labels
    settle
}

proc ::mged::xmin::workflows::return_file_dialog_path {path args} {
    return $path
}

proc ::mged::xmin::workflows::with_file_dialog_path {command path script} {
    set saved_command ${command}_xmin_saved
    rename $command $saved_command
    interp alias {} $command {} \
	::mged::xmin::workflows::return_file_dialog_path $path

    set status [catch {uplevel 1 $script} result options]
    rename $command {}
    rename $saved_command $command
    if {$status} {
	return -options $options $result
    }
    return $result
}

proc ::mged::xmin::workflows::answer_dialog {
    serial path expected_title entry value
} {
    variable dialog_serial
    variable dialog_error
    variable dialog_seen

    if {$serial != $dialog_serial} {
	return
    }
    if {![winfo exists $path] || ![winfo ismapped $path]} {
	after 25 [list ::mged::xmin::workflows::answer_dialog \
	    $serial $path $expected_title $entry $value]
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
    if {![winfo exists $path.bot.button0]} {
	set dialog_error "$expected_title dialog has no acceptance button"
	destroy $path
	return
    }
    $path.bot.button0 invoke
}

proc ::mged::xmin::workflows::with_dialog_answer {
    path expected_title entry value script
} {
    variable dialog_error
    variable dialog_seen
    variable dialog_serial

    set dialog_error ""
    set dialog_seen 0
    set serial [incr dialog_serial]
    after 25 [list ::mged::xmin::workflows::answer_dialog \
	$serial $path $expected_title $entry $value]
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

proc ::mged::xmin::workflows::dismiss_unexpected_dialog {context} {
    variable dialog_error
    variable watching_dialog
    if {!$watching_dialog} {
	return
    }
    if {![winfo exists .mged_dialog] || ![winfo ismapped .mged_dialog]} {
	after 25 [list ::mged::xmin::workflows::dismiss_unexpected_dialog $context]
	return
    }

    set dialog_error "$context displayed unexpected dialog '[wm title .mged_dialog]'"
    set watching_dialog 0
    if {[winfo exists .mged_dialog.bot.button0]} {
	.mged_dialog.bot.button0 invoke
    } else {
	destroy .mged_dialog
    }
}

proc ::mged::xmin::workflows::invoke_apply {button context} {
    variable dialog_error
    variable watching_dialog
    set dialog_error ""
    set watching_dialog 1
    after 25 [list ::mged::xmin::workflows::dismiss_unexpected_dialog $context]
    $button invoke
    set watching_dialog 0
    if {$dialog_error ne ""} {
	fail $dialog_error
    }
    settle
}

proc ::mged::xmin::workflows::reject_edit {} {
    catch {_mged_reject}
    settle
}

proc ::mged::xmin::workflows::assert_created {name panel context} {
    ::gui::test::require {[exists $name]} "$context did not create $name"
    ::gui::test::require {![winfo exists $panel]} \
	"$context did not dismiss its creation dialog"
    ::gui::test::require {[llength [get $name]] > 1} \
	"$context created an unreadable database object"
    reject_edit
}

proc ::mged::xmin::workflows::create_generic_primitive {id top type menu_path} {
    global mged_gui

    set name xmin_$type.s
    invoke $top [concat $menu_path [list [format "%s..." $type]]]
    set panel .$id.make_solid
    ::gui::test::require {[winfo exists $panel] && [winfo ismapped $panel]} \
	"Create $type did not open the generic primitive dialog"
    ::gui::test::require {
	[string first $type [$panel.nameL cget -text]] >= 0
    } "Create $type opened a dialog for the wrong primitive type"

    set mged_gui($id,solid_name) $name
    invoke_apply $panel.applyB "Create $type"
    assert_created $name $panel "Create $type"
}

proc ::mged::xmin::workflows::exercise_generic_primitives {id top} {
    foreach specification {
	{arb8 {Create Arbs}}
	{arb7 {Create Arbs}}
	{arb6 {Create Arbs}}
	{arb5 {Create Arbs}}
	{arb4 {Create Arbs}}
	{rpp {Create Arbs}}
	{arbn {Create Arbs}}
	{rcc {Create {Cones & Cylinders}}}
	{rec {Create {Cones & Cylinders}}}
	{rhc {Create {Cones & Cylinders}}}
	{rpc {Create {Cones & Cylinders}}}
	{tec {Create {Cones & Cylinders}}}
	{tgc {Create {Cones & Cylinders}}}
	{trc {Create {Cones & Cylinders}}}
	{ehy {Create Ellipsoids}}
	{ell {Create Ellipsoids}}
	{ell1 {Create Ellipsoids}}
	{epa {Create Ellipsoids}}
	{sph {Create Ellipsoids}}
	{ars Create}
	{eto Create}
	{extrude Create}
	{half Create}
	{metaball Create}
	{part Create}
	{pipe Create}
	{sketch Create}
	{tor Create}
	{bot Create}
	{nmg Create}
    } {
	lassign $specification type menu_path
	create_generic_primitive $id $top $type $menu_path
    }
}

proc ::mged::xmin::workflows::write_binary_file {path data} {
    set channel [open $path wb]
    fconfigure $channel -encoding binary -translation binary
    puts -nonewline $channel $data
    close $channel
}

proc ::mged::xmin::workflows::write_text_file {path contents} {
    set channel [open $path w]
    puts -nonewline $channel $contents
    close $channel
}

proc ::mged::xmin::workflows::exercise_file_operations {id top database} {
    global ex_control

    set extracted_database [file join $::env(GUI_TEST_DIR) extracted.g]
    invoke $top {File Export {Database Objects}}
    set extract_panel .$id.do_extract
    ::gui::test::require {
	[winfo exists $extract_panel] && [winfo ismapped $extract_panel]
    } "Database Objects export did not open the Extract Objects dialog"
    set ex_control($id,file) $extracted_database
    set ex_control($id,objects) xmin_arb8.s
    $extract_panel.okB invoke
    settle
    ::gui::test::require {
	[file exists $extracted_database] && [file size $extracted_database] > 0
    } "Database Objects export did not write a database"

    set ascii_database [file join $::env(GUI_TEST_DIR) workflows.asc]
    with_file_dialog_path ::tk_getSaveFile $ascii_database \
	[list invoke $top {File Export {Ascii Database}}]
    ::gui::test::require {
	[file exists $ascii_database] && [file size $ascii_database] > 0
    } "Ascii Database export did not write a file"

    set script [file join $::env(GUI_TEST_DIR) loaded-script.tcl]
    write_text_file $script \
	{set ::mged::xmin::workflows::script_loaded 1}
    set ::mged::xmin::workflows::script_loaded 0
    with_file_dialog_path ::tk_getOpenFile $script \
	[list with_dialog_answer .mged_dialog {Script loaded} "" "" \
	    [list invoke $top {File {Load Script...}}]]
    ::gui::test::require {$::mged::xmin::workflows::script_loaded} \
	"Load Script did not evaluate the selected Tcl file"

    set new_database [file join $::env(GUI_TEST_DIR) new-database.g]
    with_file_dialog_path ::tk_getSaveFile $new_database \
	[list with_dialog_answer .mged_dialog {File created} "" "" \
	    [list invoke $top {File {New...}}]]
    ::gui::test::require {
	[file exists $new_database] &&
	[file normalize [_mged_opendb]] eq [file normalize $new_database]
    } "New did not create and open the selected database"

    with_file_dialog_path ::tk_getOpenFile $database \
	[list with_dialog_answer .mged_dialog {File loaded} "" "" \
	    [list invoke $top {File {Open...}}]]
    ::gui::test::require {
	[file normalize [_mged_opendb]] eq [file normalize $database] &&
	[exists xmin_arb8.s]
    } "Open did not restore the selected database"

    with_file_dialog_path ::tk_getOpenFile $extracted_database \
	[list with_dialog_answer .$id.prefix Prefix .$id.prefix.mid.ent bin_ \
	    [list invoke $top {File Import {Binary Database}}]]
    ::gui::test::require {[exists bin_xmin_arb8.s]} \
	"Binary Database import did not apply its prefix"

    with_file_dialog_path ::tk_getOpenFile $ascii_database \
	[list with_dialog_answer .$id.prefix Prefix .$id.prefix.mid.ent asc_ \
	    [list invoke $top {File Import {Ascii Database}}]]
    ::gui::test::require {[exists asc_xmin_arb8.s]} \
	"Ascii Database import did not apply its prefix"
}

proc ::mged::xmin::workflows::exercise_dsp_creation {id top} {
    global mged_gui

    set data_file [file join $::env(GUI_TEST_DIR) dsp-data.bw]
    write_binary_file $data_file [binary format S* {0 1 2 3}]

    invoke $top {Create dsp...}
    set panel .$id.make_dsp
    ::gui::test::require {[winfo exists $panel] && [winfo ismapped $panel]} \
	"Create dsp did not open its specialized dialog"

    set name xmin_dsp.s
    set mged_gui($id,solid_name) $name
    set mged_gui($id,dsp_file_name) $data_file
    set mged_gui($id,dsp_file_width) 2
    set mged_gui($id,dsp_file_length) 2
    set mged_gui($id,dsp_smooth) 0
    set mged_gui($id,dsp_cell_size) 1
    set mged_gui($id,dsp_elev_size) 1
    invoke_apply $panel.applyB "Create dsp"
    assert_created $name $panel "Create dsp"
}

proc ::mged::xmin::workflows::exercise_binunif_creation {id top} {
    set data_file [file join $::env(GUI_TEST_DIR) binunif-data.bin]
    write_binary_file $data_file [binary format c* {1 2 3 4}]

    invoke $top {Create binunif...}
    set panel .$id.make_binunif
    ::gui::test::require {[winfo exists $panel] && [winfo ismapped $panel]} \
	"Create binunif did not open its specialized dialog"

    set name xmin_binunif
    $panel.nameE delete 0 end
    $panel.nameE insert 0 $name
    $panel.typeCB set {8-bit ints}
    $panel.fileE delete 0 end
    $panel.fileE insert 0 $data_file
    invoke_apply $panel.applyB "Create binunif"
    assert_created $name $panel "Create binunif"
}

proc ::mged::xmin::workflows::exercise_geometry_browser {id top} {
    invoke $top {Tools {Geometry Browser}}
    set browser .$id.geometree
    ::gui::test::require {
	[llength [info commands $browser]] == 1 &&
	[winfo exists $browser] && [winfo ismapped $browser]
    } "Geometry Browser did not open"
    ::gui::test::require {[wm title $browser] eq "Geometry Browser"} \
	"Geometry Browser has an unexpected title"

    set children [$browser getNodeChildren "" yes]
    set roots {}
    foreach child $children {
	lappend roots [lindex $child 0]
    }
    ::gui::test::require {
	[lsearch -exact $roots /xmin_arb8.s] >= 0 &&
	[lsearch -exact $roots /xmin_binunif] >= 0
    } "Geometry Browser did not populate the isolated database"

    ::gui::test::write progress "browser: display"
    $browser displayNode /xmin_arb8.s alone
    settle
    ::gui::test::require {[lsearch -exact [who] xmin_arb8.s] >= 0} \
	"Geometry Browser did not display the selected primitive"
    ::gui::test::write progress "browser: autosize"
    $browser autosizeDisplay
    ::gui::test::write progress "browser: zoom"
    $browser zoomDisplay in
    $browser zoomDisplay out
    ::gui::test::write progress "browser: clear"
    $browser clearDisplay
    settle
    ::gui::test::require {[who] eq ""} \
	"Geometry Browser did not clear the display"

    ::gui::test::write progress "browser: close"
    set file_menu [$browser.menubar entrycget 0 -menu]
    ::gui::test::require {[$file_menu entrycget 0 -label] eq "Close"} \
	"Geometry Browser does not expose its Close command"

    $file_menu invoke 0
    settle
    ::gui::test::require {![winfo exists $browser]} "Geometry Browser did not close"
}

proc ::mged::xmin::workflows::exercise_collaboration {id} {
    global mged_collaborators mged_gui mged_players

    ::gui::test::write progress "collaboration: join primary"
    if {[lsearch -exact $mged_collaborators $id] < 0} {
	collaborate join $id
    }

    set peer xmin_peer
    ::gui::test::write progress "collaboration: create peer"
    set result [gui -config b -dt tkswrast -id $peer -join]
    if {$result ne ""} {
	fail "could not create collaborating MGED player: $result"
    }
    settle
    ::gui::test::write progress "collaboration: peer created"

    ::gui::test::require {
	[lsearch -exact $mged_players $peer] >= 0 &&
	[lsearch -exact $mged_collaborators $peer] >= 0 &&
	$mged_gui($peer,collaborate)
    } "second MGED player did not join the collaborative session"

    winset $mged_gui($id,active_dm)
    _mged_center 11 22 33
    _mged_size 123
    settle
    winset $mged_gui($peer,active_dm)
    require_near [center] {11 22 33} "collaborative view center"
    require_near [size] {123} "collaborative view size"

    collaborate quit $peer
    ::gui::test::require {
	[lsearch -exact $mged_collaborators $peer] < 0 &&
	!$mged_gui($peer,collaborate)
    } "second MGED player did not leave the collaborative session"

    set mged_gui($peer,show_cmd) 0
    set mged_gui($peer,show_dm) 0
    gui_destroy $peer
    ::gui::test::require {[lsearch -exact $mged_players $peer] < 0} \
	"second MGED player did not shut down cleanly"

    collaborate quit $id
    winset $mged_gui($id,active_dm)
}

proc ::mged::xmin::workflows::finish {status message} {
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

proc ::mged::xmin::workflows::run {} {
    global mged_gui mged_players
    variable ready_retries
    variable ready_retry_limit
    variable retry_delay_ms

    if {![info exists mged_players] || [llength $mged_players] == 0} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    fail "MGED did not create a GUI player"
	}
	after $retry_delay_ms ::mged::xmin::workflows::run_checked
	return
    }
    set id [lindex $mged_players 0]
    set top .$id
    if {![winfo exists $top] || ![winfo ismapped $top] ||
	![info exists mged_gui($id,active_dm)]} {
	if {[incr ready_retries] > $ready_retry_limit} {
	    fail "MGED did not finish mapping its GUI"
	}
	after $retry_delay_ms ::mged::xmin::workflows::run_checked
	return
    }

    set database [file join $::env(GUI_TEST_DIR) workflows.g]
    cd $::env(GUI_TEST_DIR)
    file delete -force $database
    opendb $database y
    title {Xmin MGED workflow regression}

    exercise_generic_primitives $id $top
    exercise_dsp_creation $id $top
    exercise_binunif_creation $id $top
    exercise_file_operations $id $top $database
    exercise_geometry_browser $id $top
    exercise_collaboration $id
    finish 0 "PASS: MGED creation, file, browser, and collaboration workflows"
}

proc ::mged::xmin::workflows::run_checked {} {
    if {[catch {run} message options]} {
	puts stderr $message
	if {[dict exists $options -errorinfo]} {
	    set error_info [dict get $options -errorinfo]
	    puts stderr $error_info
	    ::gui::test::write tcl_error_debug $error_info
	}
	finish 1 "FAIL: MGED workflow regression: $message"
    }
}

after 25 ::mged::xmin::workflows::run_checked

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
