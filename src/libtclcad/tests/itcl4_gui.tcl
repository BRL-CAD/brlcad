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
set itcl_version [require_at_least Itcl 4.1.1]
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

foreach package {
    Archer
    ArcherCore
    GeometryChecker
    OverlapFileTool
    GeometryBrowser
    GraphEditor
    cadwidgets::Accordion
    cadwidgets::Ged
    Wizard
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

GeometryChecker .geometry_checker
OverlapFileTool .overlap_file_tool

pack .checkbox .radiobox .combobox .spinner .spinint .searchtext .watch \
    -side top -fill x
update idletasks
update

foreach widget {
    .geometry_checker
    .overlap_file_tool
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
