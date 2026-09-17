#               X M I N _ G U I _ H E L P . T C L
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
# Exercise MGED command manuals, searches, and help dialogs.

source $::env(MGED_GUI_TEST_LIBRARY)

namespace eval ::mged::xmin::help {
    variable exec_arguments {}
}

proc ::mged::xmin::help::record_exec {args} {
    set ::mged::xmin::help::exec_arguments $args
    return ""
}

proc ::mged::xmin::help::exercise_command_manual {top} {
    ::mged::gui::test::invoke $top {Help {Command Manual Pages}}
    set browser .mgedMan
    ::gui::test::require {
	[llength [info commands $browser]] == 1 &&
	[winfo exists $browser] && [winfo ismapped $browser]
    } "Command Manual Pages did not open the internal browser"

    set toc [$browser component manpagelistbox]
    ::gui::test::require {
	[winfo exists $toc] && [$toc size] > 100 &&
	[lsearch -exact [$toc get 0 end] rt] >= 0
    } "manual browser did not populate the program-page table of contents"

    man rt
    set selection [$toc curselection]
    ::gui::test::require {
	[llength $selection] == 1 && [$toc get $selection] eq "rt"
    } "man rt did not select and load the rt manual page"
    ::gui::test::require {
	[catch {man xmin_page_that_does_not_exist} message] &&
	[string first "couldn't find manual page" $message] >= 0
    } "missing manual page lookup did not report a useful error"

    man -k ray
    ::gui::test::require {[$toc size] > 0} \
	"short manual search returned no results"
    $browser search "ray tracing" full
    ::gui::test::require {[$toc size] > 0} \
	"full-text manual search returned no results"
    set search_entry [$browser component search_frame].entry
    $search_entry delete 0 end
    $search_entry insert 0 geometry
    event generate $search_entry <KeyRelease>
    ::mged::gui::test::settle
    ::gui::test::require {[$toc size] > 0} \
	"manual browser search widget did not refresh results"
    [$browser component search_frame].clear invoke
    ::gui::test::require {
	[$toc size] > 100 && [lsearch -exact [$toc get 0 end] rt] >= 0
    } "manual browser Clear did not restore the table of contents"
    $browser deactivate
}

proc ::mged::xmin::help::exercise_manual_search {top} {
    ::mged::gui::test::with_dialog_answer $top.mansearch {Manual Search} \
	$top.mansearch.mid.ent {ray tracing} $top.mansearch.bot.button0 \
	[list ::mged::gui::test::invoke $top {Help {Manual Search}}]
    set browser .mgedMan
    set toc [$browser component manpagelistbox]
    ::gui::test::require {
	[winfo ismapped $browser] && [$toc size] > 0
    } "Manual Search did not display ranked internal-browser results"
    $browser deactivate
}

proc ::mged::xmin::help::exercise_apropos {top} {
    ::mged::gui::test::with_dialog_answer $top.apropos Apropos \
	$top.apropos.mid.ent view $top.apropos.bot.button0 \
	[list ::mged::gui::test::invoke $top {Help Apropos}]
    set help_window $top.help
    ::gui::test::require {
	[winfo exists $help_window] && [winfo ismapped $help_window] &&
	[$help_window.l size] > 0
    } "Apropos did not display matching MGED commands"
    $help_window.l selection set 0
    focus $help_window.l
    ::mged::gui::test::with_dialog_answer .mged_dialog Usage "" "" \
	.mged_dialog.bot.button0 \
	[list event generate $help_window.l <KeyPress> -keysym Return]
    $help_window.cancel invoke
    ::gui::test::require {![winfo exists $help_window]} \
	"Apropos results did not dismiss"
}

proc ::mged::xmin::help::exercise_static_help {id top} {
    ::mged::gui::test::with_dialog_answer .mged_dialog {Shift Grips} \
	"" "" .mged_dialog.bot.button0 \
	[list ::mged::gui::test::invoke $top {Help {Shift Grips}}]

    ::mged::gui::test::invoke $top {Help Dedication}
    set dedication .$id\_mike
    ::gui::test::require {
	[winfo exists $dedication] && [winfo ismapped $dedication] &&
	[wm title $dedication] eq "Dedication" &&
	[string first "Michael John Muuss" [$dedication.dates cget -text]] >= 0
    } "Dedication did not display its expected content"
    $dedication.dismiss invoke
    ::gui::test::require {![winfo exists $dedication]} \
	"Dedication did not dismiss"
}

proc ::mged::xmin::help::exercise_html_manual {top} {
    global mged_browser mged_html_dir
    variable exec_arguments

    set manual_path [file normalize [file join $mged_html_dir index.html]]
    ::gui::test::require {[file readable $manual_path]} \
	"built MGED HTML manual is unavailable"

    set browser [auto_execok true]
    set mged_browser $browser
    set exec_arguments {}
    rename ::exec ::mged::xmin::help::saved_exec
    rename ::mged::xmin::help::record_exec ::exec
    set status [catch {
	::mged::gui::test::invoke $top {Help Manual}
    } message options]
    rename ::exec ::mged::xmin::help::record_exec
    rename ::mged::xmin::help::saved_exec ::exec
    if {$status} {
	return -options $options $message
    }
    ::gui::test::require {
	$exec_arguments eq [list -- {*}$browser $manual_path &]
    } "Manual did not dispatch the configured browser with its index page"

    set mged_browser [file join $::env(GUI_TEST_DIR) missing-browser]
    ::mged::gui::test::invoke $top {Help Manual}
    set fallback $top.man
    ::gui::test::require {
	[winfo exists $fallback] && [winfo ismapped $fallback] &&
	[string length [string trim [$fallback.text get 1.0 end]]] > 100
    } "Manual did not load MGED's internal HTML viewer"
    destroy $fallback
}

proc ::mged::xmin::help::run {id top} {
    exercise_command_manual $top
    exercise_manual_search $top
    exercise_apropos $top
    exercise_static_help $id $top
    exercise_html_manual $top
}

::mged::gui::test::start ::mged::xmin::help::run \
    {MGED manual and help lookup} {MGED help regression}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
