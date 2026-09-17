#           X M I N _ G U I _ O V E R L A P S . T C L
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
# Exercise MGED's overlap-generation workflow through live widgets.

source $::env(MGED_GUI_TEST_LIBRARY)

namespace eval ::mged::xmin::overlaps {}

proc ::mged::xmin::overlaps::make_overlap_group {name offset} {
    set left "${name}_left.s"
    set right "${name}_right.s"
    set left_region "${name}_left.r"
    set right_region "${name}_right.r"
    set group "${name}.g"

    in $left sph $offset 0 0 10
    in $right sph [expr {$offset + 5}] 0 0 10
    r $left_region u $left
    r $right_region u $right
    g $group $left_region $right_region
}

proc ::mged::xmin::overlaps::exercise_overlap_tool {top database} {
    make_overlap_group first 0
    make_overlap_group second 100
    Z

    ::mged::gui::test::invoke $top {Tools {Overlaps Tool}}
    set menu .overlapmenu.overlapmenu
    ::mged::gui::test::require_mapped $menu "Overlap Menu"
    $menu.buttonsFrame.newFileFrame.buttonRunOvFileGen invoke
    ::mged::gui::test::settle

    set tool .overlapmenu.overlapfiletool.overlapfiletool
    ::mged::gui::test::require_mapped $tool "Overlap File Tool"
    $tool clearSelection
    ::mged::gui::test::set_entry $tool.ovFrame.objectsEntry \
	{first.g second.g}
    $tool.ovFrame.buttonAdd invoke
    set selected [$tool.objFrame.objectsList get 0 end]
    ::gui::test::require {
	[llength $selected] == 2 &&
	[lsearch -glob $selected */first.g] >= 0 &&
	[lsearch -glob $selected */second.g] >= 0
    } "Overlap File Tool did not retain two separately selected objects"

    $tool.ovButtonFrame.buttonGo invoke
    ::mged::gui::test::settle

    set checker .checker.ck
    ::mged::gui::test::require_mapped $checker "Geometry Checker"
    set tree $checker.checkFrame.checkList
    set rows [$tree children {}]
    ::gui::test::require {[llength $rows] == 2} \
	"generated overlap file did not contain both selected assemblies"

    $checker.headerFrame.fullPathDisplayCheckButton invoke
    set first_row [lindex $rows 0]
    ::gui::test::require {
	[string index [$tree set $first_row Left] 0] eq "/" &&
	[string index [$tree set $first_row Right] 0] eq "/"
    } "Geometry Checker did not display full paths"

    $tree selection set $first_row
    event generate $tree <<TreeviewSelect>>
    ::mged::gui::test::settle
    ::gui::test::require {[llength [who]] >= 2} \
	"selecting an overlap did not draw its objects"

    $tree.checkMenu invoke 2
    set copied [clipboard get]
    ::gui::test::require {
	[string first "/" $copied] >= 0 && [llength $copied] == 2
    } "Geometry Checker did not copy both full paths"

    $checker.checkButtonFrame.buttonNext invoke
    $checker.checkButtonFrame.buttonPrev invoke
    $tree selection set $first_row
    $tree.checkMenu invoke 0

    set mark_file [file join "${database}.ck" \
	"ck.[file tail $database].marked"]
    ::gui::test::require {
	[file exists $mark_file] && [$tree tag has marked $first_row]
    } "Geometry Checker did not persist a resolved mark"
    set mark_channel [open $mark_file r]
    set marked_paths [gets $mark_channel]
    close $mark_channel
    ::gui::test::require {
	[llength $marked_paths] == 2
    } "Geometry Checker did not write both marked overlap paths"

    $tree selection set $first_row
    $tree.checkMenu invoke 1
    ::gui::test::require {![$tree tag has marked $first_row]} \
	"Geometry Checker did not clear a resolved mark"
    destroy .checker
}

proc ::mged::xmin::overlaps::run {id top} {
    set database [file join $::env(GUI_TEST_DIR) overlaps.g]
    cd $::env(GUI_TEST_DIR)
    file delete -force $database "${database}.ck"
    opendb $database y
    title {Xmin MGED overlap regression}

    exercise_overlap_tool $top $database
}

::mged::gui::test::start ::mged::xmin::overlaps::run \
    {MGED overlap tools} {MGED overlap regression}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
