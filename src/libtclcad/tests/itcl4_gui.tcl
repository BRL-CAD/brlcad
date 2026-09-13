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

proc require_at_least {package minimum} {
    set version [package require $package $minimum]
    if {[package vcompare $version $minimum] < 0} {
	fail "$package $version does not satisfy minimum $minimum"
    }
    return $version
}

set background_errors {}
proc bgerror {message} {
    lappend ::background_errors [list $message $::errorInfo]
}

set tk_version [require_at_least Tk 8.6]
set itcl_version [require_at_least Itcl 4.3.0]
set itk_version [require_at_least Itk 4.2.3]
set iwidgets_version [require_at_least Iwidgets 4.1.1]
interp alias {} Hierarchy {} ::iwidgets::Hierarchy
interp alias {} scrolledlistbox {} ::iwidgets::scrolledlistbox

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
    {swidgets::Tooltip .lifecycle_tooltip}
    {swidgets::Tree .lifecycle_tree}
    {swidgets::tkgetdir .lifecycle_directory_chooser}
    {::Wizard .lifecycle_archer_wizard}
    {::RtWizard::Wizard .lifecycle_rtwizard_wizard}
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

set source_database [file join [bu_dir data] db m35.g]
set test_database [file join [pwd] itcl4-cadwidgets-[pid].g]
set copied_database [file join [pwd] itcl4-cadwidgets-copy-[pid].g]
file delete -force $test_database $copied_database
file copy $source_database $test_database

cadwidgets::Ged .functional_ged $test_database
pack .functional_ged -fill both -expand yes
update
assert_equal "Ged database query" [.functional_ged exists all.g] 1
.functional_ged draw all.g
.functional_ged autoview
update

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
