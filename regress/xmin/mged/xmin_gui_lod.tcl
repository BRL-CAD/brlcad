#                X M I N _ G U I _ L O D . T C L
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
# Exercise the MGED LOD Configuration dialog and its CSG/VDS render paths.

source $::env(MGED_GUI_TEST_LIBRARY)

namespace eval ::mged::xmin::lod {
    variable dense_point_scale 1.5
    variable grid_divisions 30
    variable sparse_point_scale 0.1
    variable tolerance 0.01
}

proc ::mged::xmin::lod::require_near {actual expected description} {
    variable tolerance
    if {![string is double -strict $actual] ||
	abs($actual - $expected) > $tolerance} {
	::mged::gui::test::fail \
	    "$description is '$actual', expected '$expected'"
    }
}

proc ::mged::xmin::lod::create_adaptive_bot {name} {
    variable grid_divisions
    set amplitude 400.0
    set extent 2000.0
    set full_turn [expr {2.0 * acos(-1.0)}]
    set vertices {}
    set faces {}

    for {set y 0} {$y <= $grid_divisions} {incr y} {
	for {set x 0} {$x <= $grid_divisions} {incr x} {
	    set px [expr {-$extent / 2.0 + $extent * $x / $grid_divisions}]
	    set py [expr {-$extent / 2.0 + $extent * $y / $grid_divisions}]
	    set pz [expr {$amplitude *
		sin($full_turn * $x / $grid_divisions) *
		cos($full_turn * $y / $grid_divisions)}]
	    lappend vertices [list $px $py $pz]
	    if {$x < $grid_divisions && $y < $grid_divisions} {
		set lower [expr {$y * ($grid_divisions + 1) + $x}]
		set upper [expr {($y + 1) * ($grid_divisions + 1) + $x}]
		lappend faces [list $lower [expr {$lower + 1}] $upper]
		lappend faces \
		    [list [expr {$lower + 1}] [expr {$upper + 1}] $upper]
	    }
	}
    }

    put $name bot mode surface orient no flags {} V $vertices F $faces
    return [llength $faces]
}

proc ::mged::xmin::lod::plot_segment_count {tag} {
    set plot_path [file join $::env(GUI_TEST_DIR) "$tag.plot3"]
    file delete -force $plot_path
    _mged_plot $plot_path
    ::gui::test::require {
	[file exists $plot_path] && [file size $plot_path] > 0
    } "LOD $tag plot was not produced"

    set plot_text [exec $::env(PLOT3_ASC_BIN) $plot_path]
    return [regexp -all -line {^L[ 	]} $plot_text]
}

proc ::mged::xmin::lod::exercise_dialog {top} {
    lod off
    lod scale points 0.6
    lod scale curves 7

    ::mged::gui::test::invoke $top {Tools {LOD Configuration}}
    set dialog .loddialog
    set contents $dialog.contents
    set frame $contents.lodFrame
    ::gui::test::require {
	[winfo exists $dialog] && [winfo ismapped $dialog]
    } "LOD Configuration did not open"
    ::gui::test::require {[wm title $dialog] eq "LOD Configuration"} \
	"LOD Configuration has an unexpected title"

    require_near [$frame.pointsScale get] 0.6 \
	"LOD point scale initial widget value"
    require_near [$frame.curvesScale get] 7 \
	"LOD curve scale initial widget value"
    ::gui::test::require {
	[lod enabled] == 0 &&
	[$frame.pointsScale instate disabled] &&
	[$frame.pointsValueLabel instate disabled] &&
	[$frame.curvesScale instate disabled] &&
	[$frame.curvesValueLabel instate disabled] &&
	[$frame.updateButton instate disabled]
    } "disabled LOD state was not reflected by every dependent widget"

    $frame.lodonCheckbutton invoke
    ::mged::gui::test::settle
    ::gui::test::require {
	[lod enabled] == 1 &&
	![$frame.pointsScale instate disabled] &&
	![$frame.curvesScale instate disabled] &&
	![$frame.updateButton instate disabled]
    } "Use LOD Wireframes did not enable LOD and its controls"

    $frame.pointsScale set 1.3
    $frame.curvesScale set 11
    ::mged::gui::test::settle
    require_near [lod scale points] 1.3 "LOD point scale command value"
    require_near [lod scale curves] 11 "LOD curve scale command value"

    $frame.liveUpdateCheckbutton invoke
    ::gui::test::require {
	[$frame.liveUpdateCheckbutton instate selected]
    } "LOD Live Update control did not select"
    $frame.pointsScale set 0.8
    ::mged::gui::test::settle
    require_near [lod scale points] 0.8 "live LOD point scale value"

    $frame.updateButton invoke
    ::mged::gui::test::settle
    destroy $dialog
    ::gui::test::require {![winfo exists $dialog]} \
	"LOD Configuration did not close"
}

proc ::mged::xmin::lod::exercise_render_paths {} {
    variable dense_point_scale
    variable sparse_point_scale

    make xmin_lod_ell.s ell
    set face_count [create_adaptive_bot xmin_lod_bot.s]
    ::gui::test::require {
	[exists xmin_lod_bot.s] && [lindex [get xmin_lod_bot.s] 0] eq "bot"
    } "LOD fixture did not produce a BoT"

    lod off
    draw xmin_lod_bot.s
    autoview
    Z
    refresh
    set background_segments [plot_segment_count background]
    draw xmin_lod_bot.s
    refresh
    set full_segments [expr {[plot_segment_count full-detail] - $background_segments}]

    lod on
    lod scale points $sparse_point_scale
    draw xmin_lod_bot.s
    refresh
    set sparse_segments [expr {[plot_segment_count sparse-detail] - $background_segments}]

    lod scale points $dense_point_scale
    draw xmin_lod_bot.s
    refresh
    set dense_segments [expr {[plot_segment_count dense-detail] - $background_segments}]

    ::gui::test::require {
	$full_segments == $face_count * 3 &&
	$sparse_segments > 0 &&
	$sparse_segments < $dense_segments &&
	$dense_segments <= $full_segments
    } "VDS BoT LOD did not vary wireframe density ($sparse_segments, $dense_segments, $full_segments)"

    draw xmin_lod_ell.s
    refresh
    ::gui::test::require {
	[lod enabled] == 1 &&
	[lsearch -exact [who] xmin_lod_ell.s] >= 0
    } "adaptive CSG drawing did not remain active"

    lod off
    draw xmin_lod_ell.s xmin_lod_bot.s
    refresh
}

proc ::mged::xmin::lod::run {id top} {
    set database [file join $::env(GUI_TEST_DIR) lod.g]
    cd $::env(GUI_TEST_DIR)
    file delete -force $database
    opendb $database y
    title {Xmin MGED LOD regression}

    exercise_render_paths
    exercise_dialog $top
}

::mged::gui::test::start ::mged::xmin::lod::run \
    {MGED LOD dialog and adaptive rendering} \
    {MGED LOD regression}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
