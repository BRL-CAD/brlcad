#      X M I N _ G U I _ Q U E R Y _ P A T T E R N . T C L
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
# Exercise MGED's Query Ray and Build Pattern tools through live widgets.

source $::env(MGED_XMIN_GUI_LIBRARY)

namespace eval ::mged::xmin::query_pattern {}

proc ::mged::xmin::query_pattern::set_components {dialog values} {
    foreach {component value} $values {
	::mged::xmin::test::set_entry $dialog.$component $value
    }
}

proc ::mged::xmin::query_pattern::require_pattern {
    group first_clone second_clone description
} {
    ::xmin::test::require {[exists $group]} "$description group was not created"
    set definition [get $group]
    ::xmin::test::require {
	[string first $first_clone $definition] >= 0 &&
	[string first $second_clone $definition] >= 0
    } "$description group did not contain both requested clones"
}

proc ::mged::xmin::query_pattern::exercise_query_ray {id top} {
    global mouse_behavior qray_control use_air

    set mouse_behavior d
    set use_air 0
    qray echo 0
    qray effects t
    qray basename query_ray

    ::mged::xmin::test::invoke $top {Tools {Query Ray Control Panel}}
    set dialog .$id.qray_control
    ::mged::xmin::test::require_mapped $dialog "Query Ray Control Panel"

    $dialog.activeCB invoke
    $dialog.use_airCB invoke
    $dialog.cmd_echoCB invoke
    $dialog.effectsMB.m invoke 2
    ::mged::xmin::test::set_entry $dialog.bnameE xmin_qray
    set qray_control($id,oddcolor) {12 34 56}
    color_entry_update $dialog oddColor qray_control($id,oddcolor) \
	$qray_control($id,oddcolor)
    $dialog.applyB invoke

    ::xmin::test::require {
	$mouse_behavior eq "q" &&
	$use_air == 1 &&
	[_mged_qray echo] == 1 &&
	[_mged_qray effects] eq "b" &&
	[_mged_qray basename] eq "xmin_qray" &&
	[_mged_qray oddcolor] eq "12 34 56"
    } "Query Ray Apply did not persist mouse, air, effect, name, or color settings"

    $dialog.advB invoke
    ::mged::xmin::test::settle
    set advanced .$id.qray_adv
    ::mged::xmin::test::require_mapped $advanced \
	"Query Ray Advanced Settings"

    set applied_format {XMIN RAY FORMAT}
    ::mged::xmin::test::set_entry $advanced.rayE $applied_format
    $advanced.applyB invoke
    ::xmin::test::require {[_mged_qray fmt r] eq $applied_format} \
	"Query Ray advanced Apply did not persist the ray format"

    ::mged::xmin::test::set_entry $advanced.rayE {discarded format}
    $advanced.resetB invoke
    ::xmin::test::require {$qray_control($id,fmt_ray) eq $applied_format} \
	"Query Ray advanced Reset did not restore the applied format"
    $advanced.okB invoke
    ::xmin::test::require {![winfo exists $advanced]} \
	"Query Ray Advanced Settings did not close after OK"

    ::mged::xmin::test::set_entry $dialog.bnameE discarded_name
    $dialog.resetB invoke
    ::xmin::test::require {$qray_control($id,basename) eq "xmin_qray"} \
	"Query Ray Reset did not restore the applied base name"
    $dialog.okB invoke
    ::xmin::test::require {![winfo exists $dialog]} \
	"Query Ray Control Panel did not close after OK"
}

proc ::mged::xmin::query_pattern::exercise_patterns {top} {
    make rect_source.s sph
    make sph_source.s sph
    make cyl_source.s sph

    ::mged::xmin::test::invoke $top {Tools {Build Pattern Tool}}
    set dialog [::mged::xmin::test::find_toplevel_by_title "Pattern Control"]
    ::mged::xmin::test::require_mapped $dialog "Build Pattern Tool"

    set_components $dialog {
	e_group_r rect_pattern.g
	e_nxdir_r 2
	e_dxdir_r 300
	e_nydir_r 1
	e_dydir_r 0
	e_nzdir_r 1
	e_dzdir_r 0
	e_obj_r rect_source.s
    }
    $dialog.b_ok_r invoke
    require_pattern rect_pattern.g rect_source.s100 rect_source.s200 \
	"rectangular pattern"

    $dialog.tn select 1
    set_components $dialog {
	e_group_s sph_pattern.g
	e_cpatt_s {0 0 0}
	e_cobj_s {0 0 0}
	e_numaz_s 2
	e_delaz_s 90
	e_numel_s 1
	e_delel_s 0
	e_radius_s 1
	e_delta_s 0
	e_startaz_s 0
	e_startel_s 0
	e_startr_s 300
	e_obj_s sph_source.s
    }
    $dialog.b_ok_s invoke
    require_pattern sph_pattern.g sph_source.s100 sph_source.s200 \
	"spherical pattern"

    $dialog.tn select 2
    set_components $dialog {
	e_group_c cyl_pattern.g
	e_cbase_c {0 0 0}
	e_cobj_c {0 0 0}
	e_numaz_c 2
	e_delaz_c 90
	e_startr_c 300
	e_radius_c 1
	e_delta_c 0
	e_startaz_c {1 0 0}
	e_heightdir_c {0 0 1}
	e_starth_c 0
	e_hnum_c 1
	e_dnum_c 0
	e_obj_c cyl_source.s
    }
    $dialog.b_ok_c invoke
    require_pattern cyl_pattern.g cyl_source.s100 cyl_source.s200 \
	"cylindrical pattern"

    $dialog.b_dismiss_c invoke
    ::xmin::test::require {![winfo exists $dialog]} \
	"Build Pattern Tool did not dismiss"
}

proc ::mged::xmin::query_pattern::run {id top} {
    set database [file join $::env(XMIN_TEST_DIR) query-pattern.g]
    cd $::env(XMIN_TEST_DIR)
    file delete -force $database
    opendb $database y
    title {Xmin MGED specialized-tool regression}
    make qray.s sph
    r qray.r u qray.s
    attr set qray.r region yes
    draw qray.r
    autoview

    exercise_query_ray $id $top
    exercise_patterns $top
}

::mged::xmin::test::start ::mged::xmin::query_pattern::run \
    {MGED Query Ray and Build Pattern tools} \
    {MGED Query Ray/Build Pattern regression}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
