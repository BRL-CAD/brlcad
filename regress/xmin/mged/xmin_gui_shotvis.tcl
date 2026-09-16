#            X M I N _ G U I _ S H O T V I S . T C L
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
# Exercise ShotVis construction, interactive editing, persistence, and reload.

source $::env(MGED_XMIN_GUI_LIBRARY)

namespace eval ::mged::xmin::shotvis {
    variable basename xmin_shot
    variable tolerance 0.001
}

proc ::mged::xmin::shotvis::require_near {actual expected description} {
    variable tolerance
    if {![string is double -strict $actual] ||
	abs($actual - $expected) > $tolerance} {
	::mged::xmin::test::fail \
	    "$description is '$actual', expected '$expected'"
    }
}

proc ::mged::xmin::shotvis::root_combination {} {
    set title [wm title .shotvis]
    set prefix {Edit Shotline Visualization - }
    ::xmin::test::require {[string first $prefix $title] == 0} \
	"ShotVis window title does not identify its active root"
    set root [string range $title [string length $prefix] end]
    ::xmin::test::require {[exists $root]} \
	"ShotVis active root '$root' does not exist"
    return $root
}

proc ::mged::xmin::shotvis::threat_combination {} {
    set root [root_combination]
    set threats [search $root -attr shotvis_threat]
    ::xmin::test::require {[llength $threats] == 1} \
	"ShotVis root does not contain exactly one threat combination"
    return [lindex $threats 0]
}

proc ::mged::xmin::shotvis::threat_cylinder {} {
    set threat [threat_combination]
    set cylinders [search $threat -type tgc -name \*.cyl]
    ::xmin::test::require {[llength $cylinders] == 1} \
	"ShotVis threat does not contain exactly one cylinder"
    return [lindex $cylinders 0]
}

proc ::mged::xmin::shotvis::set_validated_entry {entry value} {
    ::xmin::test::require {[winfo exists $entry]} \
	"ShotVis entry '$entry' is unavailable"
    $entry delete 0 end
    $entry insert 0 $value
    ::mged::xmin::test::settle
    ::xmin::test::require {[$entry get] eq $value} \
	"ShotVis entry '$entry' rejected '$value'"
}

proc ::mged::xmin::shotvis::exercise_math {} {
    lassign [::ShotVis::direction_vector 0 90] x y z
    require_near $x 0.0 "ShotVis direction X"
    require_near $y 0.0 "ShotVis direction Y"
    require_near $z -1.0 "ShotVis direction Z"

    lassign [::ShotVis::vec2ae {0 1 0}] azimuth elevation
    require_near $azimuth 90.0 "ShotVis vector azimuth"
    require_near $elevation 0.0 "ShotVis vector elevation"
    require_near [::shotvis::vec_angle {1 0 0} {0 1 0}] 90.0 \
	"ShotVis vector angle"
}

proc ::mged::xmin::shotvis::exercise_editor {} {
    variable basename

    shotvis $basename
    ::mged::xmin::test::require_mapped .shotvis ShotVis
    ::xmin::test::require {
	[wm title .shotvis] eq "Edit Shotline Visualization - $basename" &&
	[exists $basename]
    } "ShotVis did not create its window and root combination"

    set threat [threat_combination]
    ::xmin::test::require {
	[attr get $threat shotvis_threat] == 1
    } "ShotVis threat combination lacks its identifying attribute"

    set main .shotvis.shotvis_frame
    set threat_tab $main.notebook.threat
    set direction $threat_tab.threat_direction
    set start $threat_tab.threat_start_labelframe.start_xyz
    set_validated_entry $start.xentry 25
    set_validated_entry $direction.azel_frame.az_entry 45
    set_validated_entry $direction.azel_frame.el_entry 30
    set_validated_entry $direction.draw_length_frame.entry 250

    set cylinder [threat_cylinder]
    lassign [get $cylinder V] start_x start_y start_z
    require_near $start_x 25.0 "ShotVis edited start X"
    require_near $start_y 0.0 "ShotVis edited start Y"
    require_near $start_z 0.0 "ShotVis edited start Z"
    require_near [magnitude [get $cylinder H]] 250.0 \
	"ShotVis edited draw length"

    set notebook $main.notebook
    $notebook select $notebook.nirt
    ::mged::xmin::test::settle
    $notebook select $notebook.threat
    ::mged::xmin::test::settle
    ::xmin::test::require {
	[lsearch -exact [who] [threat_combination]] >= 0
    } "ShotVis did not redraw its threat after changing tabs"

    ::itcl::delete object .shotvis
    ::xmin::test::require {
	![winfo exists .shotvis] && [exists $basename]
    } "ShotVis did not preserve its edited root when closed"
}

proc ::mged::xmin::shotvis::exercise_reload {} {
    variable basename

    shotvis $basename
    ::mged::xmin::test::require_mapped .shotvis {reloaded ShotVis}
    set cylinder [threat_cylinder]
    lassign [get $cylinder V] start_x start_y start_z
    require_near $start_x 25.0 "reloaded ShotVis start X"
    require_near [magnitude [get $cylinder H]] 250.0 \
	"reloaded ShotVis draw length"
    .shotvis update_shot
    ::mged::xmin::test::settle
    ::itcl::delete object .shotvis
    ::xmin::test::require {[exists $basename]} \
	"reloaded ShotVis did not preserve its updated root"
}

proc ::mged::xmin::shotvis::run {id top} {
    variable basename
    set database [file join $::env(XMIN_TEST_DIR) shotvis.g]
    cd $::env(XMIN_TEST_DIR)
    file delete -force $database
    opendb $database y
    title {Xmin MGED ShotVis regression}

    exercise_editor
    exercise_reload
    exercise_math
}

::mged::xmin::test::start ::mged::xmin::shotvis::run \
    {MGED ShotVis editing and persistence} {MGED ShotVis regression}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
