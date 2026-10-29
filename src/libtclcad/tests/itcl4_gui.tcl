#                    I T C L 4 _ G U I . T C L
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
# You should have received a copy of the GNU Lesser General Public License
# along with this file; see the file named COPYING for more information.
#
###
#
# Exercise the Itcl/Itk/Iwidgets paths that differ between Itcl 3 and 4.
# The test is run by both btclsh and bwish so libtclcad is checked with
# Itcl-first and Tk-first initialization orders.
#

proc fail {message} {
    puts stderr "FAIL: $message"
    exit 1
}

proc assert_equal {description actual expected} {
    if {$actual ne $expected} {
	fail "$description: got {$actual}, expected {$expected}"
    }
}

proc assert_coordinates {description actual expected} {
    if {[llength $actual] != [llength $expected]} {
	fail "$description: got {$actual}, expected {$expected}"
    }
    foreach a $actual e $expected {
	if {abs($a - $e) > 1.0e-9} {
	    fail "$description: got {$actual}, expected {$expected}"
	}
    }
}

proc assert_point_lists {description actual expected} {
    if {[llength $actual] != [llength $expected]} {
	fail "$description: got {$actual}, expected {$expected}"
    }
    foreach actual_point $actual expected_point $expected {
	assert_coordinates $description $actual_point $expected_point
    }
}

proc exercise_object_lifecycle {class object args} {
    set is_widget [string match .* $object]
    foreach cycle {first second} {
	if {[catch {$class $object {*}$args} message options]} {
	    catch {::itcl::delete object $object}
	    catch {destroy $object}
	    if {[dict exists $options -errorinfo]} {
		set message "[dict get $options -errorinfo]\n$message"
	    }
	    fail "$class $cycle construction failed: $message"
	}
	update
	if {[llength [info commands $object]] != 1} {
	    fail "$class $cycle construction omitted its object command"
	}
	if {$is_widget && ![winfo exists $object]} {
	    fail "$class $cycle construction omitted its Tk window"
	}
	if {[catch {::itcl::delete object $object} message]} {
	    fail "$class $cycle destruction failed: $message"
	}
	update
	if {[llength [info commands $object]] != 0} {
	    fail "$class $cycle destruction retained its object command"
	}
	if {$is_widget && [winfo exists $object]} {
	    fail "$class $cycle destruction retained its Tk window"
	}
    }
}

proc exercise_panedwindow_bindings {} {
    set pane_size 200
    set drag_distance 20
    set native_motion_binding [bind Panedwindow <Motion>]

    toplevel .panes
    foreach orientation {horizontal vertical} {
	set pane .panes.$orientation
	iwidgets::panedwindow $pane -orient $orientation \
	    -width $pane_size -height $pane_size -showhandle 0
	$pane add first -minimum 0 -margin 0
	$pane add second -minimum 0 -margin 0
	pack $pane -side left -fill both -expand yes
    }

    panedwindow .panes.native -width $pane_size -height $pane_size
    foreach name {first second} {
	frame .panes.native.$name
	.panes.native add .panes.native.$name
    }
    pack .panes.native -side left -fill both -expand yes
    update

    # Hull events must not invoke native Tk handlers on Iwidgets objects.
    foreach pane {.panes.horizontal .panes.vertical .panes.native} {
	foreach event {
	    <Motion> <Leave>
	    <ButtonPress-1> <B1-Motion> <ButtonRelease-1>
	    <ButtonPress-2> <B2-Motion> <ButtonRelease-2>
	} {
	    event generate $pane $event -x 1 -y 1
	}
	update
	assert_equal "$pane pointer events" $::background_errors {}
    }
    assert_equal "native panedwindow motion binding" \
	[bind Panedwindow <Motion>] $native_motion_binding

    # Removing the hull's class tag must retain Iwidgets' sash dragging.
    foreach orientation {horizontal vertical} {
	set pane .panes.$orientation
	set before [$pane fraction]
	set sash [$pane component sash1]
	set x 0
	set y 0
	if {$orientation eq "horizontal"} {
	    set y $drag_distance
	} else {
	    set x $drag_distance
	}
	event generate $sash <ButtonPress-1> -x 0 -y 0
	event generate $sash <B1-Motion> -x $x -y $y
	event generate $sash <B1-ButtonRelease-1> -x $x -y $y
	update
	if {[lindex [$pane fraction] 0] <= [lindex $before 0]} {
	    fail "$orientation panedwindow sash did not move"
	}
	assert_equal "$orientation sash events" $::background_errors {}
    }
    destroy .panes
    update
}

proc require_at_least {package minimum} {
    set version [package require $package $minimum]
    if {[package vcompare $version $minimum] < 0} {
	fail "$package $version does not satisfy minimum $minimum"
    }
    return $version
}

proc assert_compatibility_command {canonical legacy} {
    if {![llength [info commands $canonical]] && ![auto_load $canonical]} {
	fail "$canonical is not autoloadable"
    }
    if {[llength [info commands $legacy]] != 1} {
	fail "$legacy compatibility command is missing"
    }
}

set background_errors {}
proc bgerror {message} {
    lappend ::background_errors [list $message $::errorInfo]
}

set tk_version [require_at_least Tk 8.6]
set itcl_version [require_at_least Itcl 4.3.0]
set itk_version [require_at_least Itk 4.2.3]
set iwidgets_version [require_at_least Iwidgets 4.1.1]
exercise_panedwindow_bindings
foreach command {
    body class code configbody delete delete_helper ensemble filter find
    forward local mixin scope
} {
    if {[llength [info commands ::$command]]} {
	fail "Itcl command was imported globally: ::$command"
    }
}
foreach command {
    ::itk::Archetype ::itk::Toplevel ::itk::Widget ::itk::usual
    ::iwidgets::Hierarchy ::iwidgets::dialog ::iwidgets::scrolledlistbox
    ::iwidgets::tabnotebook
} {
    if {![llength [info commands $command]] && ![auto_load $command]} {
	fail "$command is not autoloadable"
    }
}
foreach command {
    Archetype Hierarchy Toplevel Widget dialog remove_destroy_hook
    scrolledlistbox tabnotebook usual
} {
    if {[llength [info commands ::$command]]} {
	fail "Itk/Iwidgets command was imported globally: ::$command"
    }
}

wm geometry . 900x700+0+0
wm title . "Itcl 4 GUI compatibility baseline"

# These classes contain every common-array reference corrected for Itcl 4.
iwidgets::checkbox .checkbox
.checkbox add alpha -text Alpha
.checkbox insert 0 beta -text Beta
.checkbox select alpha
assert_equal "checkbox selection" [.checkbox get alpha] 1

iwidgets::radiobox .radiobox
.radiobox add alpha -text Alpha
.radiobox insert 0 beta -text Beta
.radiobox select beta
assert_equal "radiobox selection" [.radiobox get] beta

iwidgets::combobox .combobox
.combobox insert list end alpha {two words}
assert_equal "combobox list insertion" [.combobox get 1] {two words}

iwidgets::spinner .spinner -labeltext Spinner
iwidgets::spinint .spinint -labeltext Integer
::iwidgets::Labeledwidget::alignlabels .spinner .spinint

set match_result {}
proc record_match {marker match_point} {
    set ::match_result [list $marker $match_point]
}
text .searchtext
.searchtext insert end "alpha two words omega"
iwidgets::finddialog .finddialog \
    -textwidget .searchtext \
    -matchcommand [list record_match {list prefix}]
set pattern [.finddialog component pattern]
$pattern insert 0 {two words}
set match_point [.finddialog find]
assert_equal "finddialog command prefix" $match_result \
    [list {list prefix} $match_point]

iwidgets::mainwindow .mainwindow
iwidgets::canvasprintbox .canvasprintbox
iwidgets::watch .watch

# Shell activation runs a nested Tk wait, which requires a functioning X
# event path and caught several failures that package-only probes could not.
iwidgets::dialogshell .dialogshell -modality application
update idletasks
.dialogshell deactivate
after 25 [list .dialogshell deactivate activated]
assert_equal "dialog activation" [.dialogshell activate] activated

# Loading the application packages checks class declarations and delayed Itk
# usual-option bodies without requiring an MGED database or rendering process.
# These commands are normally supplied by the embedding MGED session.
proc tops {args} {
    return {}
}

proc who {} {
    return {}
}

proc graph {args} {
    return {}
}

foreach package {
    Archer
    ArcherCore
    GeometryChecker
    OverlapFileTool
    GeometryBrowser
    GraphEditor
    cadwidgets::Accordion
    cadwidgets::Ged
    cadwidgets::GeometryIO
    cadwidgets::RtImage
    RtWizard::Wizard
    Sdialogs
    Swidgets
    DbPage
    ExamplePage
    FbPage
    FeedbackDialog
    FullColorPage
    GhostPage
    HelpPage
    HighlightedPage
    IntroPage
    LinePage
    MGEDpage
    PictureTypeA
    PictureTypeB
    PictureTypeBase
    PictureTypeC
    PictureTypeD
    PictureTypeE
    PictureTypeF
} {
    package require $package
}

set swidgets_exports [list \
    Selectlists Togglearrow Tooltip Tree Treenode selectlists tkgetdir \
    togglearrow tooltip tree treenode]
set sdialogs_exports [list \
    Entrydialog Listdialog Stddlgs entrydialog listdialog]
assert_equal "Swidgets exports" \
    [lsort [namespace eval ::swidgets {namespace export}]] \
    $swidgets_exports
assert_equal "Sdialogs exports" \
    [lsort [namespace eval ::sdialogs {namespace export}]] \
    $sdialogs_exports
namespace eval ::widget_export_probe {
    namespace import ::swidgets::*
    namespace import ::sdialogs::*
}
assert_equal "widget package imports" \
    [lsort [namespace eval ::widget_export_probe {namespace import}]] \
    [lsort [concat $swidgets_exports $sdialogs_exports]]
namespace delete ::widget_export_probe

foreach {canonical legacy} {
    ::cadwidgets::run_conversion_config ::run_conversion_config
    ::cadwidgets::pid_wait ::pid_wait
    ::cadwidgets::SetWaitCursor ::SetWaitCursor
    ::cadwidgets::SetNormalCursor ::SetNormalCursor
    ::cadwidgets::_clone_invoke ::_clone_invoke
    ::cadwidgets::_clone_progress_update ::_clone_progress_update
    ::cadwidgets::exists_wrapper ::exists_wrapper
    ::cadwidgets::regdef_wrapper ::regdef_wrapper
    ::cadwidgets::pattern_rect ::pattern_rect
    ::cadwidgets::pattern_sph ::pattern_sph
    ::cadwidgets::pattern_cyl ::pattern_cyl
    ::cadwidgets::pattern_control ::pattern_control
} {
    assert_compatibility_command $canonical $legacy
}
assert_equal "cursor wait counter" $::cadwidgets::cursorWaitCount 0
assert_equal "obsolete cursor counter" \
    [info exists ::cadwidgets::cursorWaitcount] 0

frame .cursor_namespace_probe
::cadwidgets::SetWaitCursor .cursor_namespace_probe
::SetWaitCursor .cursor_namespace_probe
assert_equal "nested cursor wait count" $::cadwidgets::cursorWaitCount 2
::SetNormalCursor .cursor_namespace_probe
::cadwidgets::SetNormalCursor .cursor_namespace_probe
assert_equal "balanced cursor wait count" $::cadwidgets::cursorWaitCount 0
assert_equal "restored cursor" [.cursor_namespace_probe cget -cursor] {}
destroy .cursor_namespace_probe

namespace eval ::pattern_namespace_probe {
    variable feedback_calls {}

    proc ged {marker subcommand args} {
	return [list $marker $subcommand {*}$args]
    }

    proc feedback {subcommand args} {
	variable feedback_calls
	lappend feedback_calls [list $subcommand {*}$args]
    }
}
set saved_pattern_ged $::cadwidgets::ged
set saved_pattern_mged_flag $::cadwidgets::mgedFlag
set ::cadwidgets::ged [list ::pattern_namespace_probe::ged probe]
set ::cadwidgets::mgedFlag 0
set expected_pattern_call [list probe clone rect --depth top \
    --dir x {1 0 0} --dir y {0 1 0} --dir z {0 0 1} \
    -n x=2 -d x=3 source.s]
assert_equal "canonical pattern dispatch" \
    [::cadwidgets::pattern_rect -nx 2 -dx 3 source.s] \
    $expected_pattern_call
assert_equal "legacy pattern dispatch" \
    [::pattern_rect -nx 2 -dx 3 source.s] \
    $expected_pattern_call
assert_equal "canonical exists dispatch" \
    [::cadwidgets::exists_wrapper source.s] \
    [list probe exists source.s]
assert_equal "legacy regdef dispatch" \
    [::regdef_wrapper region 1000 0 1] \
    [list probe regdef region 1000 0 1]
::cadwidgets::_clone_progress_update \
    ::pattern_namespace_probe::feedback 1 2
::_clone_progress_update ::pattern_namespace_probe::feedback 2 2
assert_equal "qualified clone progress callback" \
    $::pattern_namespace_probe::feedback_calls \
    [list reset [list configure -steps 2] step step]
set ::cadwidgets::ged $saved_pattern_ged
set ::cadwidgets::mgedFlag $saved_pattern_mged_flag
namespace delete ::pattern_namespace_probe

foreach class {::DataUtils ::sdialogs::Stddlgs ::swidgets::Togglearrow} {
    if {![llength [info commands $class]] && ![auto_load $class]} {
	fail "$class is not autoloadable"
    }
}

rename ::tk_messageBox ::itcl4_tk_messageBox
proc ::tk_messageBox {args} {
    set ::message_box_arguments $args
    return xmin-answer
}
set dialog_status [catch {
    sdialogs::Stddlgs::questiondlg {Xmin title} {Xmin message} yesno -parent .
} dialog_result dialog_options]
rename ::tk_messageBox {}
rename ::itcl4_tk_messageBox ::tk_messageBox
if {$dialog_status} {
    return -options $dialog_options $dialog_result
}
assert_equal "Stddlgs result forwarding" $dialog_result xmin-answer
foreach {option expected} {
    -title {Xmin title}
    -icon question
    -type yesno
    -message {Xmin message}
    -parent .
} {
    assert_equal "Stddlgs $option forwarding" \
	[dict get $::message_box_arguments $option] $expected
}
unset ::message_box_arguments

source [file join [bu_dir data] tclscripts archer SketchEditFrame.tcl]
if {$SketchEditFrame::rad2deg <= 0.0} {
    fail "Archer SketchEditFrame common variable is not publicly accessible"
}

# Archer and RtWizard deliberately have separate framework classes.  Loading
# and constructing both in one interpreter protects that namespace boundary.
if {![llength [info commands ::Wizard]] &&
    ![auto_load ::Wizard]} {
    fail "Archer Wizard class is not autoloadable"
}
foreach class {::Wizard ::RtWizard::Wizard} {
    if {[llength [info commands $class]] != 1} {
	fail "missing distinct wizard class $class"
    }
}

foreach lifecycle {
    {cadwidgets::Accordion .lifecycle_accordion}
    {cadwidgets::CellPlot .lifecycle_cell_plot}
    {cadwidgets::ColorEntry .lifecycle_color_entry}
    {cadwidgets::ComboBox .lifecycle_combo_box}
    {cadwidgets::Help lifecycle_help}
    {cadwidgets::Legend .lifecycle_legend}
    {cadwidgets::pattern_control .lifecycle_pattern_control}
    {Command .lifecycle_command}
    {GeometryChecker .geometry_checker}
    {GraphEditor .lifecycle_graph_editor}
    {OverlapFileTool .overlap_file_tool}
    {Splash .lifecycle_splash -message Lifecycle}
    {TabWindow .lifecycle_tab_window}
    {Table lifecycle_table}
    {TableView .lifecycle_table_view {{Column A} {Column B}}}
    {sdialogs::Entrydialog .lifecycle_entry_dialog}
    {sdialogs::Listdialog .lifecycle_list_dialog}
    {swidgets::Selectlists .lifecycle_select_lists}
    {swidgets::Togglearrow .lifecycle_toggle_arrow}
    {swidgets::Tooltip .lifecycle_tooltip}
    {swidgets::Tree .lifecycle_tree}
    {swidgets::tkgetdir .lifecycle_directory_chooser}
    {::Wizard .lifecycle_archer_wizard}
    {::RtWizard::Wizard .lifecycle_rtwizard_wizard}
    {::pattern_control .lifecycle_legacy_pattern_control}
} {
    exercise_object_lifecycle {*}$lifecycle
}

# Exercise the retained cadwidgets classes as functional components, not just
# as parse/load probes.
cadwidgets::CellPlot .functional_cell_plot \
    -range {0 10} -plotWidth 100 -plotHeight 50
set cell [.functional_cell_plot createCell 1 2 3 4 -fill red]
assert_coordinates "CellPlot rectangular coordinate transform" \
    [.functional_cell_plot coords $cell] {10 30 30 40}
::itcl::delete object .functional_cell_plot

array set ::cadwidget_table_data {}
cadwidgets::TkTable .functional_tk_table ::cadwidget_table_data \
    {{Name} {Value}} -rows 3
.functional_tk_table setTableVal 1,1 alpha
assert_equal "TkTable data update" $::cadwidget_table_data(1,1) alpha
.functional_tk_table selectSingleRow 1
assert_equal "TkTable row selection" \
    [.functional_tk_table getSelectedRows] 1
::itcl::delete object .functional_tk_table
unset ::cadwidget_table_data

sdialogs::Listdialog .functional_list_dialog -label Selection
.functional_list_dialog insert end alpha
.functional_list_dialog insert end beta
set listbox [.functional_list_dialog component listbox]
$listbox selection set 1
assert_equal "Listdialog selection" [.functional_list_dialog get] beta
::itcl::delete object .functional_list_dialog

swidgets::Selectlists .functional_select_lists -unique true
.functional_select_lists insert end {beta alpha beta}
.functional_select_lists select alpha
assert_equal "Selectlists transfer" [.functional_select_lists get] alpha
::itcl::delete object .functional_select_lists

set ::toggle_count 0
swidgets::Togglearrow .functional_toggle_arrow \
    -command {incr ::toggle_count}
pack .functional_toggle_arrow
update
set closed_arrow [.functional_toggle_arrow component closed]
event generate $closed_arrow <ButtonPress-1> -x 8 -y 8
update
assert_equal "Togglearrow open state" \
    [.functional_toggle_arrow cget -togglestate] open
assert_equal "Togglearrow callback" $::toggle_count 1
.functional_toggle_arrow configure -state disabled
set open_arrow [.functional_toggle_arrow component open]
event generate $open_arrow <ButtonPress-1> -x 8 -y 8
update
assert_equal "disabled Togglearrow state" \
    [.functional_toggle_arrow cget -togglestate] open
assert_equal "disabled Togglearrow callback" $::toggle_count 1
::itcl::delete object .functional_toggle_arrow
unset ::toggle_count

set source_database [file join [bu_dir data] db m35.g]
set test_database [file join [pwd] itcl4-cadwidgets-[pid].g]
set copied_database [file join [pwd] itcl4-cadwidgets-copy-[pid].g]
file delete -force $test_database $copied_database
file copy $source_database $test_database

cadwidgets::Ged .functional_ged $test_database
pack .functional_ged -fill both -expand yes
update
assert_equal "Ged database query" [.functional_ged exists all.g] 1
assert_coordinates "Ged unit conversion round trip" \
    [expr {[.functional_ged base2local] * [.functional_ged local2base]}] 1.0
assert_coordinates "Ged point-centered matrix wrapper" \
    [.functional_ged mat_xform_about_pnt [mat_angles 0 0 0] {1 2 3}] \
    [mat_idn]
.functional_ged draw all.g
.functional_ged autoview
update

set annotation_points {{0 0 0} {1 2 3}}
DataUtils::appendGlobalData .functional_ged .functional_ged xmin \
    Xmin_Data_Lines data_lines points $annotation_points 0 2
set stored_annotations [.functional_ged attr get _GLOBAL Xmin_Data_Lines]
assert_equal "DataUtils persisted group" \
    [lindex [lindex $stored_annotations 0] 0] xmin
assert_equal "DataUtils persisted coordinates" \
    [lrange [lindex $stored_annotations 0] 1 end] $annotation_points
assert_point_lists "DataUtils synchronized display data" \
    [.functional_ged data_lines points] $annotation_points
DataUtils::deleteGlobalData .functional_ged .functional_ged xmin Xmin_Data_Lines
assert_equal "DataUtils deleted persisted group" \
    [.functional_ged attr get _GLOBAL Xmin_Data_Lines] {}
set selected_points {{4 5 6} {7 8 9}}
.functional_ged sdata_lines points $selected_points
assert_point_lists "selected data line display" \
    [.functional_ged sdata_lines points] $selected_points

foreach lifecycle {
    {ModelAxesControl .functional_model_axes -mged .functional_ged}
    {ViewAxesControl .functional_view_axes -mged .functional_ged}
    {RtControl .functional_rt_control -mged .functional_ged}
} {
    exercise_object_lifecycle {*}$lifecycle
}

assert_equal "GeometryIO .g load" \
    [cadwidgets::geom_load $test_database 0] $test_database
assert_equal "GeometryIO .g save" \
    [cadwidgets::geom_save $test_database $copied_database .functional_ged] \
    $copied_database
assert_equal "GeometryIO .g copy size" [file size $copied_database] \
    [file size $test_database]
set unsupported_file [file join [pwd] itcl4-cadwidgets-[pid].unsupported]
if {![catch {
    cadwidgets::geom_save $test_database $unsupported_file .functional_ged
} unsupported_message] ||
    [string first "is not supported" $unsupported_message] < 0} {
    fail "GeometryIO did not reject an unsupported output format"
}

::itcl::delete object .functional_ged
file delete -force $test_database $copied_database $unsupported_file

pack .checkbox .radiobox .combobox .spinner .spinint .searchtext .watch \
    -side top -fill x
update idletasks
update

foreach widget {
    .dialogshell
    .watch
    .canvasprintbox
    .mainwindow
    .finddialog
    .spinint
    .spinner
    .combobox
    .radiobox
    .checkbox
} {
    if {[winfo exists $widget]} {
	destroy $widget
    }
}
update

if {[llength $background_errors]} {
    fail "background errors: $background_errors"
}

puts "PASS: Tcl $tk_version, Itcl $itcl_version, Itk $itk_version, Iwidgets $iwidgets_version"
exit 0

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
