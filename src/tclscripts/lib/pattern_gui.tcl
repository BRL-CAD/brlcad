#                 P A T T E R N _ G U I . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
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
#
# Description - GUI for Anderson's pattern replicator tools.
#
##

# ---------------------------------------------------------------------------
# Helper procs migrated from pattern.tcl (which has been eliminated).
# These were pattern.tcl's only public consumers.
# ---------------------------------------------------------------------------

namespace eval cadwidgets {
    if {![info exists ged]} {
	set ged db
    }

    if {![info exists mgedFlag]} {
	set mgedFlag 1
    }
}

if {![info exists local2base]} {
    set local2base 1.0
}

# _clone_invoke: dispatch 'clone' to the right command based on context
proc _clone_invoke { args } {
    if {$::cadwidgets::mgedFlag} {
	return [eval clone $args]
    } else {
	return [eval $::cadwidgets::ged clone $args]
    }
}

# _clone_progress_update: universal per-step progress handler.
# Called by MGED's BU_CLBK_DURING mechanism with the current clone count
# and total expected count supplied by the C clone implementation.
# On the first step (current == 1) the feedback widget is reset and
# configured with the correct total; every call advances it by one step.
#   widget  - iwidgets::feedback widget path
#   current - 1-based index of the clone just created
#   total   - total number of clones expected for the whole operation
proc _clone_progress_update { widget current total } {
    if { $current == 1 } {
	$widget reset
	$widget configure -steps $total
    }
    $widget step
}

proc exists_wrapper {args} {
    if {$::cadwidgets::mgedFlag} {
	eval exists $args
    } else {
	eval $::cadwidgets::ged exists $args
    }
}

proc regdef_wrapper {args} {
    if {$::cadwidgets::mgedFlag} {
	eval regdef $args
    } else {
	eval $::cadwidgets::ged regdef $args
    }
}

# pattern_rect: build a rectangular grid of clones.
proc pattern_rect { args } {
    set usage "Usage:\n\tpattern_rect \[-top|-regions|-primitives\] \[-g group_name\] \
		 \[-xdir { x y z }\] \[-ydir { x y z }\] \[-zdir { x y z }\] \
		\[-nx num_x -dx delta_x | -lx list_of_x_values\]\n\t\t \
		\[-ny num_y -dy delta_y | -ly list_of_y_values\] \[-nz num_z -dz delta_z | -lz list_of_z_values\] \
		\[-s source_string replacement_string\] \[-i increment\]  object1 \[object2 object3 ...\]"

    set opt_str ""
    set group_name ""
    set index 0
    set depth top
    set xdir {1 0 0}
    set ydir {0 1 0}
    set zdir {0 0 1}
    set num_x 0
    set num_y 0
    set num_z 0
    set delta_x 0
    set delta_y 0
    set delta_z 0
    set list_x {}
    set list_y {}
    set list_z {}
    set sstr ""
    set rstr ""
    set increment 0
    set objs {}
    set feed_name ""
    set argc [llength $args]
    while { $index < $argc } {
	set opt [lindex $args $index]
	switch -- $opt {
	    "-top"        { set depth top;        incr index }
	    "-regions"    { set depth regions;    incr index }
	    "-primitives" { set depth primitives; incr index }
	    "-g" {
		incr index; set group_name [lindex $args $index]; incr index
	    }
	    "-xdir" { incr index; set xdir [lindex $args $index]; incr index }
	    "-ydir" { incr index; set ydir [lindex $args $index]; incr index }
	    "-zdir" { incr index; set zdir [lindex $args $index]; incr index }
	    "-nx"   { incr index; set num_x   [lindex $args $index]; incr index }
	    "-ny"   { incr index; set num_y   [lindex $args $index]; incr index }
	    "-nz"   { incr index; set num_z   [lindex $args $index]; incr index }
	    "-dx"   { incr index; set delta_x [lindex $args $index]; incr index }
	    "-dy"   { incr index; set delta_y [lindex $args $index]; incr index }
	    "-dz"   { incr index; set delta_z [lindex $args $index]; incr index }
	    "-lx"   { incr index; set list_x  [lindex $args $index]; incr index }
	    "-ly"   { incr index; set list_y  [lindex $args $index]; incr index }
	    "-lz"   { incr index; set list_z  [lindex $args $index]; incr index }
	    "-s" {
		incr index
		set sstr [lindex $args $index]
		incr index
		set rstr [lindex $args $index]
		incr index
	    }
	    "-i" { incr index; set increment [lindex $args $index]; incr index }
	    "-feed_name" { incr index; set feed_name [lindex $args $index]; incr index }
	    default { set objs [lrange $args $index end]; set index $argc }
	}
    }

    if { [llength $objs] < 1 } { error "no objects specified!!!\n$usage" }
    if { [llength $list_x] == 0 && $num_x == 0 &&
	 [llength $list_y] == 0 && $num_y == 0 &&
	 [llength $list_z] == 0 && $num_z == 0 } {
	error "no X, Y, or Z values provided!!!!\n$usage"
    }

    set clone_cmd [list rect --depth $depth]
    if { $group_name ne "" } { lappend clone_cmd -g $group_name }
    if { $increment != 0 }   { lappend clone_cmd -i $increment }
    if { [string length $sstr] > 0 && [string length $rstr] > 0 } {
	lappend clone_cmd -s $sstr $rstr
    }
    lappend clone_cmd --dir x $xdir --dir y $ydir --dir z $zdir
    if { [llength $list_x] > 0 } {
	lappend clone_cmd --list "x=[join $list_x { }]"
    } elseif { $num_x > 0 } {
	lappend clone_cmd -n x=$num_x -d x=$delta_x
    }
    if { [llength $list_y] > 0 } {
	lappend clone_cmd --list "y=[join $list_y { }]"
    } elseif { $num_y > 0 } {
	lappend clone_cmd -n y=$num_y -d y=$delta_y
    }
    if { [llength $list_z] > 0 } {
	lappend clone_cmd --list "z=[join $list_z { }]"
    } elseif { $num_z > 0 } {
	lappend clone_cmd -n z=$num_z -d z=$delta_z
    }
    foreach obj $objs { lappend clone_cmd $obj }

    set result [eval _clone_invoke $clone_cmd]

    if { $group_name ne "" } {
	if { $::cadwidgets::mgedFlag } {
	    draw $group_name
	    autoview
	} else {
	    $::cadwidgets::ged draw $group_name
	    $::cadwidgets::ged autoview
	    $::cadwidgets::ged freezeGUI 0
	}
    }
    return $result
}

# pattern_sph: build a spherical pattern of clones.
proc pattern_sph { args } {
    set usage "pattern_sph \[-top | -regions | -primitives\] \[-g group_name\] \[-s source_string replacement_string\] \
		\[-i tag_number_increment\] \[-center_pat {x y z}\] \[-center_obj {x y z}\] \[-rotaz\] \[-rotel\] \
		\[-naz num_az -daz delta_az | -laz list_of_azimuths\] \
		\[-nel num_el -del delta_el | -lel list_of_elevations\] \
		\[-nr num_r -dr delta_r | -lr list_of_radii\] \
		\[-start_az starting_azimuth \] \[-start_el starting_elevation\] \[-start_r starting_radius\] \
		object1 \[object2 object3 ...\]"

    set objs {}
    set start_az_deg 0
    set start_el_deg -90.0
    set start_r 0
    set rot_az 0
    set rot_el 0
    set depth "top"
    set center_pat {0 0 0}
    set center_obj {0 0 0}
    set group_name ""
    set sstr ""
    set rstr ""
    set increment 0
    set num_az 0
    set num_el 0
    set num_r 0
    set delta_az_deg 0
    set delta_el_deg 0
    set delta_r 0
    set list_az_deg {}
    set list_el_deg {}
    set list_r {}
    set feed_name ""
    set argc [llength $args]
    set index 0
    while { $index < $argc } {
	set opt [lindex $args $index]
	switch -- $opt {
	    "-start_r"    { incr index; set start_r       [lindex $args $index]; incr index }
	    "-start_az"   { incr index; set start_az_deg  [lindex $args $index]; incr index }
	    "-start_el"   { incr index; set start_el_deg  [lindex $args $index]; incr index }
	    "-top"        { set depth top;        incr index }
	    "-regions"    { set depth regions;    incr index }
	    "-primitives" { set depth primitives; incr index }
	    "-g" {
		incr index; set group_name [lindex $args $index]; incr index
	    }
	    "-s" {
		incr index; set sstr [lindex $args $index]
		incr index; set rstr [lindex $args $index]; incr index
	    }
	    "-i"     { incr index; set increment     [lindex $args $index]; incr index }
	    "-rotaz" { set rot_az 1; incr index }
	    "-rotel" { set rot_el 1; incr index }
	    "-center_pat" { incr index; set center_pat [lindex $args $index]; incr index }
	    "-center_obj" { incr index; set center_obj [lindex $args $index]; incr index }
	    "-naz"  { incr index; set num_az       [lindex $args $index]; incr index }
	    "-daz"  { incr index; set delta_az_deg [lindex $args $index]; incr index }
	    "-nel"  { incr index; set num_el       [lindex $args $index]; incr index }
	    "-del"  { incr index; set delta_el_deg [lindex $args $index]; incr index }
	    "-nr"   { incr index; set num_r        [lindex $args $index]; incr index }
	    "-dr"   { incr index; set delta_r      [lindex $args $index]; incr index }
	    "-laz"  { incr index; set list_az_deg  [lindex $args $index]; incr index }
	    "-lel"  { incr index; set list_el_deg  [lindex $args $index]; incr index }
	    "-lr"   { incr index; set list_r       [lindex $args $index]; incr index }
	    "-feed_name" { incr index; set feed_name [lindex $args $index]; incr index }
	    default { set objs [lrange $args $index end]; set index $argc }
	}
    }

    if { [llength $objs] < 1 } { error "no objects specified\n$usage" }
    if { [llength $list_az_deg] == 0 && $num_az == 0 &&
	 [llength $list_el_deg] == 0 && $num_el == 0 &&
	 [llength $list_r] == 0 && $num_r == 0 } {
	error "No azimuth, elevation, or radii provided!!!\n$usage"
    }

    set clone_cmd [list sph --depth $depth]
    if { $group_name ne "" } { lappend clone_cmd -g $group_name }
    if { $increment != 0 }   { lappend clone_cmd -i $increment }
    if { [string length $sstr] > 0 && [string length $rstr] > 0 } {
	lappend clone_cmd -s $sstr $rstr
    }
    lappend clone_cmd -O $center_pat -p $center_obj
    set align_keys {}
    if { $rot_az } { lappend align_keys "az" }
    if { $rot_el } { lappend align_keys "el" }
    if { [llength $align_keys] > 0 } {
	lappend clone_cmd --align [join $align_keys ","]
    }
    lappend clone_cmd --start az=$start_az_deg --start el=$start_el_deg --start r=$start_r
    if { [llength $list_az_deg] > 0 } {
	lappend clone_cmd --list "az=[join $list_az_deg { }]"
    } elseif { $num_az > 0 } {
	lappend clone_cmd -n az=$num_az -d az=$delta_az_deg
    }
    if { [llength $list_el_deg] > 0 } {
	lappend clone_cmd --list "el=[join $list_el_deg { }]"
    } elseif { $num_el > 0 } {
	lappend clone_cmd -n el=$num_el -d el=$delta_el_deg
    }
    if { [llength $list_r] > 0 } {
	lappend clone_cmd --list "r=[join $list_r { }]"
    } elseif { $num_r > 0 } {
	lappend clone_cmd -n r=$num_r -d r=$delta_r
    }
    foreach obj $objs { lappend clone_cmd $obj }

    set result [eval _clone_invoke $clone_cmd]

    if { $group_name ne "" } {
	if { $::cadwidgets::mgedFlag } {
	    draw $group_name
	    autoview
	} else {
	    $::cadwidgets::ged draw $group_name
	    $::cadwidgets::ged autoview
	    $::cadwidgets::ged freezeGUI 0
	}
    }
    return $result
}

# pattern_cyl: build a cylindrical pattern of clones.
proc pattern_cyl { args } {
    set usage "pattern_cyl \[-top | -regions | -primitives\] \[-g group_name\] \[-s source_string replacement_string\] \
		\[-i tag_number_increment\] \[-rot\] \[-center_obj {x y z}\] \[-center_base {x y z}\] \[-height_dir {x y z}\] \
		\[-start_az_dir {x y z}\] \
		\[-naz num_az -daz delta_az | -laz list_of_azimuths\] \
		\[-sr start_r\] \
		\[-nr num_r -dr delta_r | -lr list_of_radii\] \
		\[-sh start_h\] \
		\[-nh num_h -dh delta_h | -lh list_of_heights\] \
		object1 \[object2 object3 ...\]"

    set objs {}
    set do_rot 0
    set depth "top"
    set group_name ""
    set sstr ""
    set rstr ""
    set increment 0
    set start_az_deg 0
    set start_az_dir {1 0 0}
    set start_r 0
    set start_h 0
    set num_az 0
    set num_r 0
    set num_h 0
    set delta_az_deg 0
    set delta_r 0
    set delta_h 0
    set list_az_deg {}
    set list_r {}
    set list_h {}
    set center_base {0 0 0}
    set center_obj {0 0 0}
    set height_dir {0 0 1}
    set feed_name ""
    set argc [llength $args]
    set index 0
    while { $index < $argc } {
	set opt [lindex $args $index]
	switch -- $opt {
	    "-top"        { set depth top;        incr index }
	    "-regions"    { set depth regions;    incr index }
	    "-primitives" { set depth primitives; incr index }
	    "-g"    { incr index; set group_name  [lindex $args $index]; incr index }
	    "-s" {
		incr index; set sstr [lindex $args $index]
		incr index; set rstr [lindex $args $index]; incr index
	    }
	    "-i"    { incr index; set increment     [lindex $args $index]; incr index }
	    "-rot"  { set do_rot 1; incr index }
	    "-start_az"     { incr index; set start_az_deg  [lindex $args $index]; incr index }
	    "-start_az_dir" { incr index; set start_az_dir  [lindex $args $index]; incr index }
	    "-center_obj"   { incr index; set center_obj    [lindex $args $index]; incr index }
	    "-center_base"  { incr index; set center_base   [lindex $args $index]; incr index }
	    "-height_dir"   { incr index; set height_dir    [lindex $args $index]; incr index }
	    "-naz"  { incr index; set num_az       [lindex $args $index]; incr index }
	    "-daz"  { incr index; set delta_az_deg [lindex $args $index]; incr index }
	    "-laz"  { incr index; set list_az_deg  [lindex $args $index]; incr index }
	    "-nr"   { incr index; set num_r        [lindex $args $index]; incr index }
	    "-dr"   { incr index; set delta_r      [lindex $args $index]; incr index }
	    "-lr"   { incr index; set list_r       [lindex $args $index]; incr index }
	    "-nh"   { incr index; set num_h        [lindex $args $index]; incr index }
	    "-dh"   { incr index; set delta_h      [lindex $args $index]; incr index }
	    "-lh"   { incr index; set list_h       [lindex $args $index]; incr index }
	    "-sr"   { incr index; set start_r      [lindex $args $index]; incr index }
	    "-sh"   { incr index; set start_h      [lindex $args $index]; incr index }
	    "-feed_name" { incr index; set feed_name [lindex $args $index]; incr index }
	    default { set objs [lrange $args $index end]; set index $argc }
	}
    }

    if { [llength $objs] < 1 } { error "no objects specified\n$usage" }
    if { [llength $list_az_deg] == 0 && $num_az == 0 &&
	 [llength $list_r] == 0 && $num_r == 0 &&
	 [llength $list_h] == 0 && $num_h == 0 } {
	error "No azimuth, radii, or heights provided!!!!\n$usage"
    }

    set clone_cmd [list cyl --depth $depth]
    if { $group_name ne "" } { lappend clone_cmd -g $group_name }
    if { $increment != 0 }   { lappend clone_cmd -i $increment }
    if { [string length $sstr] > 0 && [string length $rstr] > 0 } {
	lappend clone_cmd -s $sstr $rstr
    }
    if { $do_rot }           { lappend clone_cmd --align az }
    lappend clone_cmd -p $center_obj -O $center_base
    lappend clone_cmd --dir h $height_dir --dir az $start_az_dir
    lappend clone_cmd --start az=$start_az_deg --start r=$start_r --start h=$start_h
    if { [llength $list_az_deg] > 0 } {
	lappend clone_cmd --list "az=[join $list_az_deg { }]"
    } elseif { $num_az > 0 } {
	lappend clone_cmd -n az=$num_az -d az=$delta_az_deg
    }
    if { [llength $list_r] > 0 } {
	lappend clone_cmd --list "r=[join $list_r { }]"
    } elseif { $num_r > 0 } {
	lappend clone_cmd -n r=$num_r -d r=$delta_r
    }
    if { [llength $list_h] > 0 } {
	lappend clone_cmd --list "h=[join $list_h { }]"
    } elseif { $num_h > 0 } {
	lappend clone_cmd -n h=$num_h -d h=$delta_h
    }
    foreach obj $objs { lappend clone_cmd $obj }

    set result [eval _clone_invoke $clone_cmd]

    if { $group_name ne "" } {
	if { $::cadwidgets::mgedFlag } {
	    draw $group_name
	    autoview
	} else {
	    $::cadwidgets::ged draw $group_name
	    $::cadwidgets::ged autoview
	    $::cadwidgets::ged freezeGUI 0
	}
    }
    return $result
}

# ---------------------------------------------------------------------------
# End of migrated procs
# ---------------------------------------------------------------------------

class pattern_control {
    inherit itk::Toplevel

    itk_option define -ged ged Ged "db"

    constructor {} {}
    destructor {}

    public {

    }

    private {
	variable combovar_r "top"
	variable group_r
	variable xdir_r {1 0 0}
	variable ydir_r {0 1 0}
	variable zdir_r {0 0 1}
	variable dirtype_r 1
	variable nxdir_r 1
	variable nydir_r 1
	variable nzdir_r 1
	variable dxdir_r 0
	variable dydir_r 0
	variable dzdir_r 0
	variable xlist_r
	variable ylist_r
	variable zlist_r
	variable source_string_r
	variable rep_string_r
	variable increment_r
	variable obj_r

	variable combovar_s "top"  ;# depth combo box
	variable group_s           ;# group name
	variable cpatt_s           ;# patt center
	variable cobj_s            ;# obj center

	variable azel_s 1          ;# az/el rb
	variable radii_s 1         ;# radii rb
	variable rotaz_s           ;# rotate az cb
	variable rotel_s           ;# rotate el cb
	variable numaz_s 1         ;# number of az
	variable numel_s 1         ;# number of el
	variable delaz_s 0         ;# delta az
	variable delel_s 0         ;# delta el
	variable lsaz_s            ;# list of az's
	variable lsel_s            ;# list of el's
	variable radnum_s 1        ;# number of radii
	variable raddel_s 0        ;# delta of radii
	variable radlist_s         ;# list of radii
	variable startaz_s 0       ;# starting az
	variable startel_s -90     ;# starting el
	variable startr_s  0       ;# starting radius
	variable obj_s             ;# object list
	variable source_string_s   ;# source string
	variable rep_string_s      ;# replacement string
	variable increment_s       ;# increment

	variable combovar_c "top"  ;# depth combo box
	variable group_c           ;# group name
	variable cbase_c           ;# patt center
	variable cobj_c            ;# obj center

	variable azel_c 1
	variable radii_c 1
	variable rotaz_c
	variable rotel_c
	variable numaz_c 1
	variable numel_c 1
	variable delaz_c 0
	variable delel_c 0
	variable lsaz_c
	variable lsel_c
	variable radnum_c 1
	variable raddel_c 0
	variable radlist_c
	variable startaz_c {1 0 0}
	variable startel_c
	variable obj_c
	variable source_string_c
	variable rep_string_c
	variable increment_c
	variable heightdir_c {0 0 1}
	variable starth_c
	variable e_startr_c
	variable height_c 1
	variable hnum_c
	variable dnum_c
	variable lnum_c
	variable rot_c

	variable helpvar ""

	method update_depth { box level } {}
	method apply_rect {} {}
	method apply_sph {} {}
	method apply_cyl {} {}
	method frame_disable { frame_name } {}
	method frame_enable { frame_name } {}
	method switch_states { frame_on frame_off } {}


    }
}

body pattern_control::constructor {} {
    $this configure -title "Pattern Control"

    set pad(x) 2
    set pad(y) 2
    set rb_font {-family helvetica -size 12 -weight bold -slant roman -underline 0 -overstrike 0}

    #Tabnotebook

    itk_component add tn {
	tabnotebook $itk_interior.tn -tabpos n -gap 3 -raiseselect true -bevelamount 3 -borderwidth 3  -width 890 -height 532 ;#-width 490 -height 1000
    }
    $itk_component(tn) add -label "Rectangular"
    $itk_component(tn) add -label "Spherical"
    $itk_component(tn) add -label "Cylindrical"
    set tab0 [$itk_component(tn) childsite 0]
    set tab1 [$itk_component(tn) childsite 1]
    set tab2 [$itk_component(tn) childsite 2]
    $itk_component(tn) select 0


    # R E C T _ T A B

    #Combo Label
    itk_component add l_combo_r {
	label $itk_interior.l_combo_r -text "Depth of Duplication:"
    }

    #Combo Box
    itk_component add f_combo_r {
	frame $itk_interior.f_combo_r -relief sunken -bd 2
    }

    itk_component add e_combo_r {
	entry $itk_interior.e_combo_r -relief flat -width 20 -textvariable [scope combovar_r]
    }

    itk_component add b_combo_r {
	menubutton $itk_interior.b_combo_r -relief raised -indicatoron 1
    }

    itk_component add m_combo_r {
	menu $itk_component(b_combo_r).m_combo_r -tearoff 0
    }

    set windowtag [code $this]

    $itk_component(m_combo_r) add command -label Top -command "set combovar_r Top ; $windowtag update_depth r top"
    $itk_component(m_combo_r) add command -label Regions -command "set combovar_r Regions ; $windowtag update_depth r regions"
    $itk_component(m_combo_r) add command -label Primitives -command "set combovar_r Primitives ; $windowtag update_depth r primitives"

    $itk_component(b_combo_r) configure -menu $itk_component(m_combo_r)

    grid $itk_component(e_combo_r) -in $itk_component(f_combo_r) -row 0 -column 0 -sticky nsew
    grid $itk_component(b_combo_r) -in $itk_component(f_combo_r) -row 0 -column 1 -sticky nsew


    #Group Name

    itk_component add l_group_r {
	label $itk_interior.l_group_r -text "Group Name:"
    }

    itk_component add e_group_r {
	entry $itk_interior.e_group_r -relief sunken -textvariable [scope group_r]
    }


    #Directions
    itk_component add l_xdir_r {
	label $itk_interior.l_xdir_r -text "X Direction:"
    }

    itk_component add l_ydir_r {
	label $itk_interior.l_ydir_r -text "Y Direction:"
    }

    itk_component add l_zdir_r {
	label $itk_interior.l_zdir_r -text "Z Direction:"
    }

    itk_component add e_xdir_r {
	entry $itk_interior.e_xdir_r -relief sunken -textvariable [scope xdir_r]
    }

    itk_component add e_ydir_r {
	entry $itk_interior.e_ydir_r -relief sunken -textvariable [scope ydir_r]
    }

    itk_component add e_zdir_r {
	entry $itk_interior.e_zdir_r -relief sunken -textvariable [scope zdir_r]
    }


    #Directions
    itk_component add f_dir_r {
	frame $itk_interior.f_dir_r -relief groove -bd 2
    }

    itk_component add rb_dir_r {
	radiobutton $itk_interior.rb_dir_r -text "Use Directions" -variable [scope dirtype_r] -value 1 -font $rb_font -command [code $this switch_states f_dir_r f_list_r]
    }

    itk_component add l_nxdir_r {
	label $itk_interior.l_nxdir_r -text "No. in X Direction:"
    }

    itk_component add l_nydir_r {
	label $itk_interior.l_nydir_r -text "No. in Y Direction:"
    }

    itk_component add l_nzdir_r {
	label $itk_interior.l_nzdir_r -text "No. in Z Direction:"
    }

    itk_component add e_nxdir_r {
	entry $itk_interior.e_nxdir_r -relief sunken -bd 2 -textvariable [scope nxdir_r]
    }

    itk_component add e_nydir_r {
	entry $itk_interior.e_nydir_r -relief sunken -bd 2 -textvariable [scope nydir_r]
    }

    itk_component add e_nzdir_r {
	entry $itk_interior.e_nzdir_r -relief sunken -bd 2 -textvariable [scope nzdir_r]
    }

    itk_component add l_dxdir_r {
	label $itk_interior.l_dxdir_r -text "Delta in X Direction:"
    }

    itk_component add l_dydir_r {
	label $itk_interior.l_dydir_r -text "Delta in Y Direction:"
    }

    itk_component add l_dzdir_r {
	label $itk_interior.l_dzdir_r -text "Delta in Z Direction:"
    }

    itk_component add e_dxdir_r {
	entry $itk_interior.e_dxdir_r -relief sunken -bd 2 -textvariable [scope dxdir_r]
    }

    itk_component add e_dydir_r {
	entry $itk_interior.e_dydir_r -relief sunken -bd 2 -textvariable [scope dydir_r]
    }

    itk_component add e_dzdir_r {
	entry $itk_interior.e_dzdir_r -relief sunken -bd 2 -textvariable [scope dzdir_r]
    }

    grid $itk_component(rb_dir_r)  -in $itk_component(f_dir_r) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_nxdir_r) -in $itk_component(f_dir_r) -row 1 -column 0 -sticky nsw
    grid $itk_component(l_nydir_r) -in $itk_component(f_dir_r) -row 2 -column 0 -sticky nsw
    grid $itk_component(l_nzdir_r) -in $itk_component(f_dir_r) -row 3 -column 0 -sticky nsw
    grid $itk_component(e_nxdir_r) -in $itk_component(f_dir_r) -row 1 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_nydir_r) -in $itk_component(f_dir_r) -row 2 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_nzdir_r) -in $itk_component(f_dir_r) -row 3 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(l_dxdir_r) -in $itk_component(f_dir_r) -row 4 -column 0 -sticky nsw
    grid $itk_component(l_dydir_r) -in $itk_component(f_dir_r) -row 5 -column 0 -sticky nsw
    grid $itk_component(l_dzdir_r) -in $itk_component(f_dir_r) -row 6 -column 0 -sticky nsw
    grid $itk_component(e_dxdir_r) -in $itk_component(f_dir_r) -row 4 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_dydir_r) -in $itk_component(f_dir_r) -row 5 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_dzdir_r) -in $itk_component(f_dir_r) -row 6 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew


    # Lists
    itk_component add f_list_r {
	frame $itk_interior.f_list_r -relief groove -bd 2
    }

    itk_component add rb_list_r {
	radiobutton $itk_interior.rb_list_r -text "Use Lists" -variable [scope dirtype_r] -value 0 -font $rb_font -command [code $this switch_states f_list_r f_dir_r]
    }

    itk_component add l_xlist_r {
	label $itk_interior.l_xlist_r -text "List of X Values:"
    }

    itk_component add l_ylist_r {
	label $itk_interior.l_ylist_r -text "List of Y Values:"
    }

    itk_component add l_zlist_r {
	label $itk_interior.l_zlist_r -text "List of Z Values:"
    }

    itk_component add e_xlist_r {
	entry $itk_interior.e_xlist_r -relief sunken -textvariable [scope xlist_r]
    }

    itk_component add e_ylist_r {
	entry $itk_interior.e_ylist_r -relief sunken -textvariable [scope ylist_r]
    }

    itk_component add e_zlist_r {
	entry $itk_interior.e_zlist_r -relief sunken -textvariable [scope zlist_r]
    }

    grid $itk_component(rb_list_r) -in $itk_component(f_list_r) -row 0  -column 0 -sticky nsw
    grid $itk_component(l_xlist_r) -in $itk_component(f_list_r) -row 1  -column 0 -sticky nsw
    grid $itk_component(l_ylist_r) -in $itk_component(f_list_r) -row 2 -column 0 -sticky nsw
    grid $itk_component(l_zlist_r) -in $itk_component(f_list_r) -row 3 -column 0 -sticky nsw
    grid $itk_component(e_xlist_r) -in $itk_component(f_list_r) -row 1  -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_ylist_r) -in $itk_component(f_list_r) -row 2 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_zlist_r) -in $itk_component(f_list_r) -row 3 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew


    #String Replacement
    itk_component add l_sstring_r {
	label $itk_interior.l_sstring_r -text "Source String:"
    }

    itk_component add l_rstring_r {
	label $itk_interior.l_rstring_r -text "Replacement String:"
    }

    itk_component add e_sstring_r {
	entry $itk_interior.e_sstring_r -relief sunken -textvariable [scope source_string_r]
    }

    itk_component add e_rstring_r {
	entry $itk_interior.e_rstring_r -relief sunken -textvariable [scope rep_string_r]
    }


    #Increment
    itk_component add l_incr_r {
	label $itk_interior.l_incr_r -text "Increment:"
    }

    itk_component add e_incr_r {
	entry $itk_interior.e_incr_r -relief sunken -textvariable [scope increment_r]
    }


    #Objects
    itk_component add l_obj_r {
	label $itk_interior.l_obj_r -text "Objects:"
    }

    itk_component add e_obj_r {
	entry $itk_interior.e_obj_r -relief sunken -textvariable [scope obj_r]
    }


    #Buttons
    itk_component add b_ok_r {
	button $itk_interior.b_ok_r -text "OK" -command [code $this apply_rect]
    }

    itk_component add b_dismiss_r {
	button $itk_interior.b_dismiss_r -text "Dismiss" -command "destroy $itk_interior"
    }


    #Grids
    grid $itk_component(tn) -sticky nsew

    grid $itk_component(l_combo_r)   -in $tab0 -row 0 -column 0 -sticky nsw
    grid $itk_component(l_group_r)   -in $tab0 -row 1 -column 0 -sticky nsw
    grid $itk_component(l_xdir_r)    -in $tab0 -row 2 -column 0 -sticky nsw
    grid $itk_component(l_ydir_r)    -in $tab0 -row 3 -column 0 -sticky nsw
    grid $itk_component(l_zdir_r)    -in $tab0 -row 4 -column 0 -sticky nsw

    grid $itk_component(l_sstring_r) -in $tab0 -row 5 -column 0 -sticky nsw
    grid $itk_component(l_rstring_r) -in $tab0 -row 6 -column 0 -sticky nsw
    grid $itk_component(l_incr_r)    -in $tab0 -row 7 -column 0 -sticky nsw
    grid $itk_component(l_obj_r)     -in $tab0 -row 8 -column 0 -sticky nsw
    grid $itk_component(b_ok_r)      -in $tab0 -row 9 -column 0 -columnspan 2

    grid $itk_component(f_combo_r)   -in $tab0 -row 0 -column 1 -sticky new
    grid $itk_component(e_group_r)   -in $tab0 -row 1 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_xdir_r)    -in $tab0 -row 2 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_ydir_r)    -in $tab0 -row 3 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_zdir_r)    -in $tab0 -row 4 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_sstring_r) -in $tab0 -row 5 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_rstring_r) -in $tab0 -row 6 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_incr_r)    -in $tab0 -row 7 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(e_obj_r)     -in $tab0 -row 8 -column 1 -padx $pad(x) -pady $pad(y) -sticky nsew
    grid $itk_component(b_dismiss_r) -in $tab0 -row 9 -column 2 -columnspan 2

    grid $itk_component(f_dir_r)     -in $tab0 -row 0 -column 2 -columnspan 2 -sticky nsew -padx $pad(x) -pady $pad(y) -rowspan 5
    grid $itk_component(f_list_r)    -in $tab0 -row 5 -column 2 -columnspan 2 -sticky nsew -padx $pad(x) -pady $pad(y) -rowspan 4

    #Row Configure
    grid rowconfigure $itk_interior 0 -weight 1
    grid rowconfigure $itk_component(f_combo_r) 0 -weight 1

    set i 1
    while {$i <= 8} {
	grid rowconfigure $tab0 $i -weight 1
	incr i
    }

    set i 0
    while {$i < 7} {
	grid rowconfigure $itk_component(f_dir_r) $i -weight 1
	incr i
    }

    set i 0
    while {$i < 4} {
	grid rowconfigure $itk_component(f_list_r) $i -weight 1
	incr i
    }


    #Column Configure
    grid columnconfigure $itk_interior 0 -weight 1

    grid columnconfigure $itk_component(f_combo_r) 0 -weight 1
    grid columnconfigure $itk_component(f_combo_r) 1 -weight 1

    grid columnconfigure $tab0 0 -weight 1
    grid columnconfigure $tab0 1 -weight 1
    grid columnconfigure $tab0 2 -weight 1
    grid columnconfigure $tab0 3 -weight 1


    grid columnconfigure $itk_component(f_dir_r) 0 -weight 1
    grid columnconfigure $itk_component(f_dir_r) 1 -weight 1


    grid columnconfigure $itk_component(f_list_r) 0 -weight 1
    grid columnconfigure $itk_component(f_list_r) 1 -weight 1


    #bind $itk_component() <Enter> " set [list [scope helpvar]] {}"
    bind $itk_component(l_combo_r) <Enter> " set [list [scope helpvar]]   {Depth of duplication of objects. ``top'' - only build top level objects.\n\t\t``regions'' - duplicate down to and including regions.\n\t\t``primitives'' - duplicate down to and including primitives} "
    bind $itk_component(l_group_r) <Enter> " set [list [scope helpvar]]   {Enter the name for the created group} "
    bind $itk_component(l_xdir_r) <Enter>  " set [list [scope helpvar]]   {Enter the X Direction Vector for the pattern}"
    bind $itk_component(l_ydir_r) <Enter>  " set [list [scope helpvar]]   {Enter the Y Direction Vector for the pattern}"
    bind $itk_component(l_zdir_r) <Enter>  " set [list [scope helpvar]]   {Enter the Z Direction Vector for the pattern}"
    bind $itk_component(rb_dir_r) <Enter> " set [list [scope helpvar]]    {Select to generate creation points based on number of points and distances between them}"
    bind $itk_component(l_nxdir_r) <Enter> " set [list [scope helpvar]]   {Enter the number of objects to create in X direction}"
    bind $itk_component(l_nydir_r) <Enter> " set [list [scope helpvar]]   {Enter the number of objects to create in Y direction}"
    bind $itk_component(l_nzdir_r) <Enter> " set [list [scope helpvar]]   {Enter the number of objects to create in Z direction}"
    bind $itk_component(l_dxdir_r) <Enter> " set [list [scope helpvar]]   {Enter the distance between objects in X direction}"
    bind $itk_component(l_dydir_r) <Enter> " set [list [scope helpvar]]   {Enter the distance between objects in Y direction}"
    bind $itk_component(l_dzdir_r) <Enter> " set [list [scope helpvar]]   {Enter the distance between objects in Z direction}"
    bind $itk_component(l_incr_r) <Enter> " set [list [scope helpvar]]    {Enter the amount to increment tag numbers (0 is OK)}"
    bind $itk_component(l_sstring_r) <Enter> " set [list [scope helpvar]] {Enter a string appearing in the objects to duplicate that you want to change (empty is OK)}"
    bind $itk_component(l_rstring_r) <Enter> " set [list [scope helpvar]] {Enter the string you want to replace the above string with (empty is OK)}"
    bind $itk_component(l_obj_r) <Enter> " set [list [scope helpvar]]     {Enter the list of objects to duplicate}"
    bind $itk_component(rb_list_r) <Enter> " set [list [scope helpvar]]   {Select to generate creation points using lists of coordinate values}"
    bind $itk_component(l_xlist_r) <Enter> " set [list [scope helpvar]]   {Enter a list of X values for creation points}"
    bind $itk_component(l_ylist_r) <Enter> " set [list [scope helpvar]]   {Enter a list of Y values for creation points}"
    bind $itk_component(l_zlist_r) <Enter> " set [list [scope helpvar]]   {Enter a list of Z values for creation points}"


    foreach obj { l_combo_r \
		      l_group_r \
		      l_xdir_r  \
		      l_ydir_r  \
		      l_zdir_r  \
		      rb_dir_r  \
		      l_nxdir_r \
		      l_nydir_r \
		      l_nzdir_r \
		      l_dxdir_r \
		      l_dydir_r \
		      l_dzdir_r \
		      l_incr_r  \
		      l_sstring_r \
		      l_rstring_r \
		      l_obj_r \
		      rb_list_r \
		      l_xlist_r \
		      l_ylist_r \
		      l_zlist_r} {

	bind $itk_component($obj) <Leave> " set [list [scope helpvar]] {} "
    }


    code $this update_depth top
    update


    # S P H _ T A B
    itk_component add l_combo_s {
	label $itk_interior.l_combo_s -text "Depth of Duplication:"
    }

    #Combo Box
    itk_component add f_combo_s {
	frame $itk_interior.f_combo_s -relief sunken -bd 2
    }

    itk_component add e_combo_s {
	entry $itk_interior.e_combo_s -relief flat -width 20 -textvariable [scope combovar_s]
    }

    itk_component add b_combo_s {
	menubutton $itk_interior.b_combo_s -relief raised -indicatoron 1
    }

    itk_component add m_combo_s {
	menu $itk_component(b_combo_s).m_combo_s -tearoff 0
    }

    set windowtag [code $this]

    $itk_component(m_combo_s) add command -label Top -command "set combovar_s Top ; $windowtag update_depth s top"
    $itk_component(m_combo_s) add command -label Regions -command "set combovar_s Regions ; $windowtag update_depth s regions"
    $itk_component(m_combo_s) add command -label Primitives -command "set combovar_s Primitives ; $windowtag update_depth s primitives"

    $itk_component(b_combo_s) configure -menu $itk_component(m_combo_s)

    grid $itk_component(e_combo_s) -in $itk_component(f_combo_s) -row 0 -column 0 -sticky nsew
    grid $itk_component(b_combo_s) -in $itk_component(f_combo_s) -row 0 -column 1 -sticky nsew


    #Group Name

    itk_component add l_group_s {
	label $itk_interior.l_group_s -text "Group Name:"
    }

    itk_component add e_group_s {
	entry $itk_interior.e_group_s -relief sunken -textvariable [scope group_s]
    }

    #Center pattern
    itk_component add l_cpatt_s {
	label $itk_interior.l_cpatt_s -text "Pattern Center:"
    }

    itk_component add e_cpatt_s {
	entry $itk_interior.e_cpatt_s -textvariable [scope cpatt_s]
    }

    #Center object
    itk_component add l_cobj_s {
	label $itk_interior.l_cobj_s -text "Object Center:"
    }

    itk_component add e_cobj_s {
	entry $itk_interior.e_cobj_s -textvariable [scope cobj_s]
    }

    itk_component add cb_rotaz_s {
	checkbutton $itk_interior.cb_rotaz_s -text "Rotate Azimuth" -font $rb_font -variable [scope rotaz_s]
    }

    itk_component add cb_rotel_s {
	checkbutton $itk_interior.cb_rotel_s -text "Rotate Elevation" -font $rb_font -variable [scope rotel_s]
    }

    itk_component add f_num_s {
	frame $itk_interior.f_num_s -relief groove -bd 2
    }

    itk_component add rb_num_s {
	radiobutton $itk_interior.rb_num_s -text "Create Az/El" -font $rb_font -variable [scope azel_s] -value 1 -command [code $this switch_states f_num_s f_list_s]
    }

    itk_component add l_numaz_s {
	label $itk_interior.l_numaz_s -text "Number of Azimuths:"
    }

    itk_component add l_numel_s {
	label $itk_interior.l_numel_s -text "Number of Elevations:"
    }

    itk_component add e_numaz_s {
	entry $itk_interior.e_numaz_s -relief sunken -textvariable [scope numaz_s]
    }

    itk_component add e_numel_s {
	entry $itk_interior.e_numel_s -relief sunken -textvariable [scope numel_s]
    }

    itk_component add l_delaz_s {
	label $itk_interior.l_delaz_s -text "Delta in Azimuth:"
    }

    itk_component add l_delel_s {
	label $itk_interior.l_delel_s -text "Delta in Elevation:"
    }

    itk_component add e_delaz_s {
	entry $itk_interior.e_delaz_s -relief sunken -textvariable [scope delaz_s]
    }

    itk_component add e_delel_s {
	entry $itk_interior.e_delel_s -relief sunken -textvariable [scope delel_s]
    }

    grid $itk_component(rb_num_s)  -in $itk_component(f_num_s) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_numaz_s) -in $itk_component(f_num_s) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_numaz_s) -in $itk_component(f_num_s) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_numel_s) -in $itk_component(f_num_s) -row 2 -column 0 -sticky nsw
    grid $itk_component(e_numel_s) -in $itk_component(f_num_s) -row 2 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_delaz_s) -in $itk_component(f_num_s) -row 3 -column 0 -sticky nsw
    grid $itk_component(e_delaz_s) -in $itk_component(f_num_s) -row 3 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_delel_s) -in $itk_component(f_num_s) -row 4 -column 0 -sticky nsw
    grid $itk_component(e_delel_s) -in $itk_component(f_num_s) -row 4 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)


    itk_component add f_list_s {
	frame $itk_interior.f_list_s -relief groove -bd 2
    }

    itk_component add cb_list_s {
	radiobutton $itk_interior.cb_list_s -text "Use Lists" -font $rb_font -variable [scope azel_s] -value 0 -command [code $this switch_states f_list_s f_num_s]
    }

    itk_component add l_listaz_s {
	label $itk_interior.l_listaz_s -text "List of Azimuths:"
    }

    itk_component add e_listaz_s {
	entry $itk_interior.e_listaz_s -relief sunken -textvariable [scope lsaz_s]
    }

    itk_component add l_listel_s {
	label $itk_interior.l_listel_s -text "List of Elevations:"
    }

    itk_component add e_listel_s {
	entry $itk_interior.e_listel_s -relief sunken -textvariable [scope lsel_s]
    }

    itk_component add f_radius_s {
	frame $itk_interior.f_radius_s -relief groove -bd 2
    }

    itk_component add rb_radius_s {
	radiobutton $itk_interior.rb_radius_s -text "Create Radii" -font $rb_font -variable [scope radii_s] -value 1 -command [code $this switch_states f_radius_s f_radlist_s]
    }

    itk_component add l_radius_s {
	label $itk_interior.l_radius_s -text "Number of Radii:"
    }

    itk_component add e_radius_s {
	entry $itk_interior.e_radius_s -relief sunken -textvariable [scope radnum_s]
    }

    itk_component add l_delta_s {
	label $itk_interior.l_delta_s -text "Radius Delta:"
    }

    itk_component add e_delta_s {
	entry $itk_interior.e_delta_s -relief sunken -textvariable [scope raddel_s]
    }

    itk_component add f_radlist_s {
	frame $itk_interior.f_radlist_s -relief groove -bd 2
    }

    itk_component add rb_radlist_s {
	radiobutton $itk_interior.rb_radlist_s -text "Use Radii List" -font $rb_font -variable [scope radii_s] -value 0 -command [code $this switch_states f_radlist_s f_radius_s]
    }

    itk_component add l_radlist_s {
	label $itk_interior.l_radlist_s -text "Radii List:"
    }

    itk_component add e_radlist_s {
	entry $itk_interior.e_radlist_s -relief sunken -textvariable [scope radlist_s]
    }

    itk_component add l_startaz_s {
	label $itk_interior.l_startaz_s -text "Starting Azimuth:"
    }

    itk_component add e_startaz_s {
	entry $itk_interior.e_startaz_s -relief sunken -textvariable [scope startaz_s]
    }

    itk_component add l_startel_s {
	label $itk_interior.l_startel_s -text "Starting Elevation:"
    }

    itk_component add e_startel_s {
	entry $itk_interior.e_startel_s -relief sunken -textvariable [scope startel_s]
    }

    itk_component add l_startr_s {
	label $itk_interior.l_startr_s -text "Starting Radius:"
    }

    itk_component add e_startr_s {
	entry $itk_interior.e_startr_s -relief sunken -textvariable [scope startr_s]
    }

    itk_component add l_obj_s {
	label $itk_interior.l_obj_s -text "Object List:"
    }

    itk_component add e_obj_s {
	entry $itk_interior.e_obj_s -relief sunken -textvariable [scope obj_s]
    }

    #String Replacement
    itk_component add l_sstring_s {
	label $itk_interior.l_sstring_s -text "Source String:"
    }

    itk_component add l_rstring_s {
	label $itk_interior.l_rstring_s -text "Replacement String:"
    }

    itk_component add e_sstring_s {
	entry $itk_interior.e_sstring_s -relief sunken -textvariable [scope source_string_s]
    }

    itk_component add e_rstring_s {
	entry $itk_interior.e_rstring_s -relief sunken -textvariable [scope rep_string_s]
    }


    #Increment
    itk_component add l_incr_s {
	label $itk_interior.l_incr_s -text "Increment:"
    }

    itk_component add e_incr_s {
	entry $itk_interior.e_incr_s -relief sunken -textvariable [scope increment_s]
    }


    itk_component add b_ok_s {
	button $itk_interior.b_ok_s -text "OK" -command [code $this apply_sph]
    }

    itk_component add b_dismiss_s {
	button $itk_interior.b_dismiss_s -text "Dismiss" -command "destroy $itk_interior"
    }


    grid $itk_component(cb_list_s)     -in $itk_component(f_list_s) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_listaz_s)    -in $itk_component(f_list_s) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_listaz_s)    -in $itk_component(f_list_s) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_listel_s)    -in $itk_component(f_list_s) -row 2 -column 0 -sticky nsw
    grid $itk_component(e_listel_s)    -in $itk_component(f_list_s) -row 2 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(rb_radius_s)  -in $itk_component(f_radius_s) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_radius_s)   -in $itk_component(f_radius_s) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_radius_s)   -in $itk_component(f_radius_s) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_delta_s)    -in $itk_component(f_radius_s) -row 2 -column 0 -sticky nsw
    grid $itk_component(e_delta_s)    -in $itk_component(f_radius_s) -row 2 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(rb_radlist_s)  -in $itk_component(f_radlist_s) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_radlist_s)   -in $itk_component(f_radlist_s) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_radlist_s)   -in $itk_component(f_radlist_s) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(f_list_s)    -in $itk_component(f_num_s)    -row 5 -column 0 -columnspan 2  -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(f_radlist_s) -in $itk_component(f_radius_s) -row 3 -column 0 -columnspan 2  -sticky nsew -padx $pad(x) -pady $pad(y)

    #Grids
    grid $itk_component(l_combo_s)   -in $tab1 -row 0 -column 0 -sticky nsw
    grid $itk_component(f_combo_s)   -in $tab1 -row 0 -column 1 -sticky new
    grid $itk_component(l_group_s)   -in $tab1 -row 1 -column 0 -sticky nsw
    grid $itk_component(e_group_s)   -in $tab1 -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_cpatt_s)   -in $tab1 -row 2 -column 0 -sticky nsw
    grid $itk_component(e_cpatt_s)   -in $tab1 -row 2 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_cobj_s)    -in $tab1 -row 3 -column 0 -sticky nsw
    grid $itk_component(e_cobj_s)    -in $tab1 -row 3 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(cb_rotaz_s)  -in $tab1 -row 4 -column 0 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(cb_rotel_s)  -in $tab1 -row 4 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(f_num_s)     -in $tab1 -row 0 -column 2 -columnspan 2  -sticky nsew -padx $pad(x) -pady $pad(y) -rowspan 7
    grid $itk_component(f_radius_s)  -in $tab1 -row 7 -column 2 -columnspan 2  -sticky nsew -padx $pad(x) -pady $pad(y) -rowspan 4

    grid $itk_component(l_startaz_s) -in $tab1 -row 5 -column 0 -sticky nsw
    grid $itk_component(e_startaz_s) -in $tab1 -row 5 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_startel_s) -in $tab1 -row 6 -column 0 -sticky nsw
    grid $itk_component(e_startel_s) -in $tab1 -row 6 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_startr_s)  -in $tab1 -row 7 -column 0 -sticky nsw
    grid $itk_component(e_startr_s)  -in $tab1 -row 7 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(l_sstring_s) -in $tab1 -row 8 -column 0 -sticky nsw
    grid $itk_component(e_sstring_s) -in $tab1 -row 8 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_rstring_s) -in $tab1 -row 9 -column 0 -sticky nsw
    grid $itk_component(e_rstring_s) -in $tab1 -row 9 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_incr_s)    -in $tab1 -row 10 -column 0 -sticky nsw
    grid $itk_component(e_incr_s)    -in $tab1 -row 10 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(l_obj_s)     -in $tab1 -row 11 -column 0 -sticky nsw
    grid $itk_component(e_obj_s)     -in $tab1 -row 11 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(b_ok_s)      -in $tab1 -row 13 -column 0 -columnspan 2
    grid $itk_component(b_dismiss_s) -in $tab1 -row 13 -column 2 -columnspan 2
    code $this update_depth top
    update

    #Row Configures
    grid rowconfigure $itk_component(f_combo_s) 0 -weight 1

    set i 0
    while {$i <= 10} {
	grid rowconfigure $tab1 $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 5} {
	grid rowconfigure $itk_component(f_num_s) $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 2} {
	grid rowconfigure $itk_component(f_list_s) $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 3} {
	grid rowconfigure $itk_component(f_radius_s) $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 1} {
	grid rowconfigure $itk_component(f_radlist_s) $i -weight 1
	incr i
    }

    #Column Configures
    grid columnconfigure $itk_component(f_combo_s) 0 -weight 1
    grid columnconfigure $tab1 0 -weight 1
    grid columnconfigure $tab1 1 -weight 1
    grid columnconfigure $itk_component(f_num_s) 0 -weight 1
    grid columnconfigure $itk_component(f_num_s) 1 -weight 1
    grid columnconfigure $itk_component(f_list_s) 0 -weight 1
    grid columnconfigure $itk_component(f_list_s) 1 -weight 1
    grid columnconfigure $itk_component(f_radius_s) 0 -weight 1
    grid columnconfigure $itk_component(f_radius_s) 1 -weight 1
    grid columnconfigure $itk_component(f_radlist_s) 0 -weight 1
    grid columnconfigure $itk_component(f_radlist_s) 1 -weight 1


    #bind $itk_component() <Enter> " set [list [scope helpvar]] {}"
    bind $itk_component(l_combo_s) <Enter> " set [list [scope helpvar]] {Depth of duplication of objects. ``top'' - only build top level objects.\n\t\t``regions'' - duplicate down to and including regions.\n\t\t``primitives'' - duplicate down to and including primitives} "
    bind $itk_component(l_group_s) <Enter> " set [list [scope helpvar]] {Enter the name for the created group} "
    bind $itk_component(l_cpatt_s) <Enter> " set [list [scope helpvar]] {Enter the coordinates of center of pattern}"
    bind $itk_component(l_cobj_s) <Enter> " set [list [scope helpvar]] {Enter the coordinates of center of object to be duplicated}"
    bind $itk_component(cb_rotaz_s) <Enter> " set [list [scope helpvar]] {Select to rotate the duplicates in Azimuth}"
    bind $itk_component(cb_rotel_s) <Enter> " set [list [scope helpvar]] {Select to rotate the duplicates in Elevation}"
    bind $itk_component(rb_num_s) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on number of azimuth and elevation angles and delta angles}"
    bind $itk_component(l_numaz_s) <Enter> " set [list [scope helpvar]] {Enter the number of azimuth angles to be used}"
    bind $itk_component(l_numel_s) <Enter> " set [list [scope helpvar]] {Enter the number of elevation angles to be used}"
    bind $itk_component(l_delaz_s) <Enter> " set [list [scope helpvar]] {Enter the azimuth delta angle (degrees)}"
    bind $itk_component(l_delel_s) <Enter> " set [list [scope helpvar]] {Enter the elevation angle delta (degrees)}"
    bind $itk_component(cb_list_s) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on lists of azimuth and elevation angles}"
    bind $itk_component(l_listaz_s) <Enter> " set [list [scope helpvar]] {Enter a list of azimuth angles in degrees (0-360)}"
    bind $itk_component(l_listel_s) <Enter> " set [list [scope helpvar]] {Enter a list of elevation angles in degrees (-90 - 90)}"
    bind $itk_component(rb_radius_s) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on number of radii and radius delta}"
    bind $itk_component(l_radius_s) <Enter> " set [list [scope helpvar]] {Enter the number of radii to be used}"
    bind $itk_component(l_delta_s) <Enter> " set [list [scope helpvar]] {Enter the radius delta to be used}"
    bind $itk_component(rb_radlist_s) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on a list of radii}"
    bind $itk_component(l_radlist_s) <Enter> " set [list [scope helpvar]] {Enter a list of radii to be used}"
    bind $itk_component(l_startaz_s) <Enter> " set [list [scope helpvar]] {Enter starting azimuth angle in degrees (0-360)}"
    bind $itk_component(l_startel_s) <Enter> " set [list [scope helpvar]] {Enter starting elevation angle in degrees (-90 - 90)}"
    bind $itk_component(l_startr_s) <Enter> " set [list [scope helpvar]] {Enter starting radius}"
    bind $itk_component(l_obj_s) <Enter> " set [list [scope helpvar]] {Enter the list of objects to duplicate}"
    bind $itk_component(l_sstring_s) <Enter> " set [list [scope helpvar]] {Enter a string appearing in the objects to duplicate that you want to change (empty is OK)}"
    bind $itk_component(l_rstring_s) <Enter> " set [list [scope helpvar]] {Enter the string you want to replace the above string with (empty is OK)}"
    bind $itk_component(l_incr_s) <Enter> " set [list [scope helpvar]] {Enter value to use in incrementing primitive/region numbers (0 is OK)}"

    foreach obj {l_combo_s l_group_s l_cpatt_s l_cobj_s cb_rotaz_s cb_rotel_s rb_num_s l_numaz_s l_numel_s l_delaz_s l_delel_s cb_list_s l_listaz_s l_listel_s rb_radius_s l_radius_s l_delta_s rb_radlist_s l_radlist_s l_startaz_s l_startel_s l_obj_s l_sstring_s l_rstring_s l_incr_s } {

	bind $itk_component($obj) <Leave> " set [list [scope helpvar]] {} "
    }

    #CYL TAB

    itk_component add l_combo_c {
	label $itk_interior.l_combo_c -text "Depth of Duplication:"
    }

    #Combo Box
    itk_component add f_combo_c {
	frame $itk_interior.f_combo_c -relief sunken -bd 2
    }

    itk_component add e_combo_c {
	entry $itk_interior.e_combo_c -relief flat -width 20 -textvariable [scope combovar_c]
    }

    itk_component add b_combo_c {
	menubutton $itk_interior.b_combo_c -relief raised -indicatoron 1
    }

    itk_component add m_combo_c {
	menu $itk_component(b_combo_c).m_combo_c -tearoff 0
    }

    set windowtag [code $this]

    $itk_component(m_combo_c) add command -label Top -command "set combovar_c Top ; $windowtag update_depth c top"
    $itk_component(m_combo_c) add command -label Regions -command "set combovar_c Regions ; $windowtag update_depth c regions"
    $itk_component(m_combo_c) add command -label Primitives -command "set combovar_c Primitives ; $windowtag update_depth c primitives"

    $itk_component(b_combo_c) configure -menu $itk_component(m_combo_c)

    grid $itk_component(e_combo_c) -in $itk_component(f_combo_c) -row 0 -column 0 -sticky nsew
    grid $itk_component(b_combo_c) -in $itk_component(f_combo_c) -row 0 -column 1 -sticky nsew


    #Group Name

    itk_component add l_group_c {
	label $itk_interior.l_group_c -text "Group Name:"
    }

    itk_component add e_group_c {
	entry $itk_interior.e_group_c -relief sunken -textvariable [scope group_c]
    }

    #Center base
    itk_component add l_cbase_c {
	label $itk_interior.l_cbase_c -text "Base Center:"
    }

    itk_component add e_cbase_c {
	entry $itk_interior.e_cbase_c -textvariable [scope cbase_c]
    }

    #Center object
    itk_component add l_cobj_c {
	label $itk_interior.l_cobj_c -text "Object Center:"
    }

    itk_component add e_cobj_c {
	entry $itk_interior.e_cobj_c -textvariable [scope cobj_c]
    }

    itk_component add cb_rot_c {
	checkbutton $itk_interior.cb_rot_c -text "Rotate" -font $rb_font -variable [scope rot_c]
    }


    itk_component add f_num_c {
	frame $itk_interior.f_num_c -relief groove -bd 2
    }

    itk_component add rb_num_c {
	radiobutton $itk_interior.rb_num_c -text "Create Az" -font $rb_font -variable [scope azel_c] -value 1 -command [code $this switch_states f_num_c f_list_c]
    }

    # Number of azimuths??
    itk_component add l_numaz_c {
	label $itk_interior.l_numaz_c -text "Number of Azimuths:"
    }


    itk_component add e_numaz_c {
	entry $itk_interior.e_numaz_c -relief sunken -textvariable [scope numaz_c]
    }


    itk_component add l_delaz_c {
	label $itk_interior.l_delaz_c -text "Delta in Azimuth:"
    }


    itk_component add e_delaz_c {
	entry $itk_interior.e_delaz_c -relief sunken -textvariable [scope delaz_c]
    }


    grid $itk_component(rb_num_c)  -in $itk_component(f_num_c) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_numaz_c) -in $itk_component(f_num_c) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_numaz_c) -in $itk_component(f_num_c) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(l_delaz_c) -in $itk_component(f_num_c) -row 3 -column 0 -sticky nsw
    grid $itk_component(e_delaz_c) -in $itk_component(f_num_c) -row 3 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)


    itk_component add f_list_c {
	frame $itk_interior.f_list_c -relief groove -bd 2
    }

    itk_component add cb_list_c {
	radiobutton $itk_interior.cb_list_c -text "Use Lists" -font $rb_font -variable [scope azel_c] -value 0 -command [code $this switch_states f_list_c f_num_c]
    }

    itk_component add l_listaz_c {
	label $itk_interior.l_listaz_c -text "List of Azimuths:"
    }

    itk_component add e_listaz_c {
	entry $itk_interior.e_listaz_c -relief sunken -textvariable [scope lsaz_c]
    }


    itk_component add l_startr_c {
	label $itk_interior.l_startr_c -text "Starting Radius:"
    }

    itk_component add e_startr_c {
	entry $itk_interior.e_startr_c -relief sunken -textvariable [scope e_startr_c]
    }


    itk_component add f_radius_c {
	frame $itk_interior.f_radius_c -relief groove -bd 2
    }

    itk_component add rb_radius_c {
	radiobutton $itk_interior.rb_radius_c -text "Create Radii" -font $rb_font -variable [scope radii_c] -value 1 -command [code $this switch_states f_radius_c f_radlist_c]
    }

    itk_component add l_radius_c {
	label $itk_interior.l_radius_c -text "Number of Radii:"
    }

    itk_component add e_radius_c {
	entry $itk_interior.e_radius_c -relief sunken -textvariable [scope radnum_c]
    }

    itk_component add l_delta_c {
	label $itk_interior.l_delta_c -text "Radius Delta:"
    }

    itk_component add e_delta_c {
	entry $itk_interior.e_delta_c -relief sunken -textvariable [scope raddel_c]
    }


    itk_component add f_radlist_c {
	frame $itk_interior.f_radlist_c -relief groove -bd 2
    }

    itk_component add rb_radlist_c {
	radiobutton $itk_interior.rb_radlist_c -text "Use Radii List" -font $rb_font -variable [scope radii_c] -value 0 -command [code $this switch_states f_radlist_c f_radius_c]
    }

    itk_component add l_radlist_c {
	label $itk_interior.l_radlist_c -text "Radii List:"
    }

    itk_component add e_radlist_c {
	entry $itk_interior.e_radlist_c -relief sunken -textvariable [scope radlist_c]
    }

    itk_component add l_startaz_c {
	label $itk_interior.l_startaz_c -text "Starting Azimuth Direction:"
    }

    itk_component add e_startaz_c {
	entry $itk_interior.e_startaz_c -relief sunken -textvariable [scope startaz_c]
    }

    itk_component add l_heightdir_c {
	label $itk_interior.l_heightdir_c -text "Height Direction:"
    }

    itk_component add e_heightdir_c {
	entry $itk_interior.e_heightdir_c -relief sunken -textvariable [scope heightdir_c]
    }


    itk_component add l_starth_c {
	label $itk_interior.l_starth_c -text "Starting Height:"
    }

    itk_component add e_starth_c {
	entry $itk_interior.e_starth_c -relief sunken -textvariable [scope starth_c]
    }


    itk_component add l_obj_c {
	label $itk_interior.l_obj_c -text "Object List:"
    }

    itk_component add e_obj_c {
	entry $itk_interior.e_obj_c -relief sunken -textvariable [scope obj_c]
    }


    itk_component add f_height_c {
	frame $itk_interior.f_height_c -relief groove -bd 2
    }

    itk_component add rb_height_c {
	radiobutton $itk_interior.rb_height_c -text "Create Heights" -variable [scope height_c] -font $rb_font -value 1 -command [code $this switch_states f_height_c f_lnum_c]
    }

    itk_component add l_hnum_c {
	label $itk_interior.l_hnum_c -text "No. of Heights:"
    }

    itk_component add e_hnum_c {
	entry $itk_interior.e_hnum_c -relief sunken -textvariable [scope hnum_c]
    }

    itk_component add l_dnum_c {
	label $itk_interior.l_dnum_c -text "Delta Height:"
    }

    itk_component add e_dnum_c {
	entry $itk_interior.e_dnum_c -relief sunken -textvariable [scope dnum_c]
    }

    itk_component add f_lnum_c {
	frame $itk_interior.f_lnum_c -relief groove -bd 2
    }

    itk_component add rb_lnum_c {
	radiobutton $itk_interior.rb_lnum_c -text "Use Lists" -variable [scope height_c] -font $rb_font -value 0 -command [code $this switch_states f_lnum_c f_height_c]
    }

    itk_component add l_lnum_c {
	label $itk_interior.l_lnum_c -text "List of Heights:"
    }

    itk_component add e_lnum_c {
	entry $itk_interior.e_lnum_c -relief sunken -textvariable [scope lnum_c]
    }

    grid $itk_component(rb_height_c) -in $itk_component(f_height_c) -row 0 -column 0 -sticky nsw -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_hnum_c)    -in $itk_component(f_height_c) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_hnum_c)    -in $itk_component(f_height_c) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_dnum_c)    -in $itk_component(f_height_c) -row 2 -column 0 -sticky nsw
    grid $itk_component(e_dnum_c)    -in $itk_component(f_height_c) -row 2 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(rb_lnum_c)   -in $itk_component(f_lnum_c)   -row 0 -column 0 -sticky nsw -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_lnum_c)    -in $itk_component(f_lnum_c)   -row 1 -column 0 -sticky nsw
    grid $itk_component(e_lnum_c)    -in $itk_component(f_lnum_c)   -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(f_lnum_c)    -in $itk_component(f_height_c) -row 3 -column 0 -columnspan 2 -sticky nsew -padx $pad(x) -pady $pad(y)

    #String Replacement
    itk_component add l_sstring_c {
	label $itk_interior.l_sstring_c -text "Source String:"
    }

    itk_component add l_rstring_c {
	label $itk_interior.l_rstring_c -text "Replacement String:"
    }

    itk_component add e_sstring_c {
	entry $itk_interior.e_sstring_c -relief sunken -textvariable [scope source_string_c]
    }

    itk_component add e_rstring_c {
	entry $itk_interior.e_rstring_c -relief sunken -textvariable [scope rep_string_c]
    }


    #Increment
    itk_component add l_incr_c {
	label $itk_interior.l_incr_c -text "Increment:"
    }

    itk_component add e_incr_c {
	entry $itk_interior.e_incr_c -relief sunken -textvariable [scope increment_c]
    }


    itk_component add b_ok_c {
	button $itk_interior.b_ok_c -text "OK" -command [code $this apply_cyl]
    }

    itk_component add b_dismiss_c {
	button $itk_interior.b_dismiss_c -text "Dismiss" -command "destroy $itk_interior"
    }

    grid $itk_component(f_list_c)      -in $itk_component(f_num_c)    -row 5 -column 0 -columnspan 2  -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(f_radlist_c)   -in $itk_component(f_radius_c) -row 3 -column 0 -columnspan 2  -sticky nsew -padx $pad(x) -pady $pad(y)


    grid $itk_component(cb_list_c)     -in $itk_component(f_list_c) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_listaz_c)    -in $itk_component(f_list_c) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_listaz_c)    -in $itk_component(f_list_c) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(rb_radius_c)  -in $itk_component(f_radius_c) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_radius_c)   -in $itk_component(f_radius_c) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_radius_c)   -in $itk_component(f_radius_c) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_delta_c)    -in $itk_component(f_radius_c) -row 2 -column 0 -sticky nsw
    grid $itk_component(e_delta_c)    -in $itk_component(f_radius_c) -row 2 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(rb_radlist_c)  -in $itk_component(f_radlist_c) -row 0 -column 0 -sticky nsw
    grid $itk_component(l_radlist_c)   -in $itk_component(f_radlist_c) -row 1 -column 0 -sticky nsw
    grid $itk_component(e_radlist_c)   -in $itk_component(f_radlist_c) -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    #Grids

    grid $itk_component(l_combo_c)     -in $tab2 -row 0 -column 0 -sticky nsw
    grid $itk_component(f_combo_c)     -in $tab2 -row 0 -column 1 -sticky new
    grid $itk_component(l_group_c)     -in $tab2 -row 1 -column 0 -sticky nsw
    grid $itk_component(e_group_c)     -in $tab2 -row 1 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_cbase_c)     -in $tab2 -row 2 -column 0 -sticky nsw
    grid $itk_component(e_cbase_c)     -in $tab2 -row 2 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_cobj_c)      -in $tab2 -row 3 -column 0 -sticky nsw
    grid $itk_component(e_cobj_c)      -in $tab2 -row 3 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(cb_rot_c)      -in $tab2 -row 4 -column 0 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(l_startaz_c)   -in $tab2 -row 5 -column 0 -sticky nsw
    grid $itk_component(e_startaz_c)   -in $tab2 -row 5 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(f_num_c)       -in $tab2 -row 6 -column 0 -columnspan 2   -sticky nsew -padx $pad(x) -pady $pad(y) -rowspan 2

    grid $itk_component(l_startr_c)    -in $tab2 -row 6 -column 2 -sticky nsw
    grid $itk_component(e_startr_c)    -in $tab2 -row 6 -column 3 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(f_radius_c)    -in $tab2 -row 7 -column 2 -columnspan 2  -sticky nsew -padx $pad(x) -pady $pad(y)


    grid $itk_component(l_starth_c)    -in $tab2 -row 0 -column 2 -sticky nsw
    grid $itk_component(e_starth_c)    -in $tab2 -row 0 -column 3 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(l_heightdir_c) -in $tab2 -row 1 -column 2 -sticky nsw
    grid $itk_component(e_heightdir_c) -in $tab2 -row 1 -column 3 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(f_height_c)    -in $tab2 -row 2 -column 2 -sticky nsew -padx $pad(x) -pady $pad(y) -columnspan 2 -rowspan 4

    grid $itk_component(l_sstring_c)   -in $tab2 -row 12 -column 0 -sticky nsw
    grid $itk_component(e_sstring_c)   -in $tab2 -row 12 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_rstring_c)   -in $tab2 -row 13 -column 0 -sticky nsw
    grid $itk_component(e_rstring_c)   -in $tab2 -row 13 -column 1 -sticky nsew -padx $pad(x) -pady $pad(y)
    grid $itk_component(l_incr_c)      -in $tab2 -row 12 -column 2 -sticky nsw
    grid $itk_component(e_incr_c)      -in $tab2 -row 12 -column 3 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(l_obj_c)       -in $tab2 -row 13 -column 2 -sticky nsw
    grid $itk_component(e_obj_c)       -in $tab2 -row 13 -column 3 -sticky nsew -padx $pad(x) -pady $pad(y)

    grid $itk_component(b_ok_c)        -in $tab2 -row 16 -column 0 -columnspan 2
    grid $itk_component(b_dismiss_c)   -in $tab2 -row 16 -column 2 -columnspan 2
    code $this update_depth top
    update

    #Row Configures
    grid rowconfigure $itk_component(f_combo_c) 0 -weight 1

    set i 0
    while {$i <= 15} {
	grid rowconfigure $tab2 $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 5} {
	grid rowconfigure $itk_component(f_num_c) $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 2} {
	grid rowconfigure $itk_component(f_list_c) $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 3} {
	grid rowconfigure $itk_component(f_radius_c) $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 1} {
	grid rowconfigure $itk_component(f_radlist_c) $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 3} {
	grid rowconfigure $itk_component(f_height_c) $i -weight 1
	incr i
    }

    set i 0
    while {$i <= 2} {
	grid rowconfigure $itk_component(f_lnum_c) $i -weight 1
	incr i
    }


    #Column Configures
    grid columnconfigure $itk_component(f_combo_c) 0 -weight 1
    grid columnconfigure $tab2 0 -weight 1
    grid columnconfigure $tab2 1 -weight 1
    grid columnconfigure $tab2 2 -weight 1
    grid columnconfigure $tab2 3 -weight 1

    grid columnconfigure $itk_component(f_num_c) 0 -weight 1
    grid columnconfigure $itk_component(f_num_c) 1 -weight 1
    grid columnconfigure $itk_component(f_list_c) 0 -weight 1
    grid columnconfigure $itk_component(f_list_c) 1 -weight 1
    grid columnconfigure $itk_component(f_radius_c) 0 -weight 1
    grid columnconfigure $itk_component(f_radius_c) 1 -weight 1
    grid columnconfigure $itk_component(f_radlist_c) 0 -weight 1
    grid columnconfigure $itk_component(f_radlist_c) 1 -weight 1
    grid columnconfigure $itk_component(f_height_c) 0 -weight 1
    grid columnconfigure $itk_component(f_height_c) 1 -weight 1
    grid columnconfigure $itk_component(f_lnum_c) 0 -weight 1
    grid columnconfigure $itk_component(f_lnum_c) 1 -weight 1


    itk_component add f_status {
	frame $itk_interior.f_status -relief sunken -bd 2
    }

    itk_component add l_help {
	label $itk_interior.l_help -relief flat -textvariable [scope helpvar] -justify left -height 3 -width 99
    }

    itk_component add fb_progress {
	feedback $itk_interior.fb_progress
    }

    #bind $itk_component() <Enter> " set [list [scope helpvar]] {}"
    bind $itk_component(rb_num_c) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on number and delta of azimuths}"
    bind $itk_component(l_combo_c) <Enter> " set [list [scope helpvar]] {Depth of duplication of objects. ``top'' - only build top level objects.\n\t\t``regions'' - duplicate down to and including regions.\n\t\t``primitives'' - duplicate down to and including primitives}"
    bind $itk_component(l_group_c) <Enter> " set [list [scope helpvar]] {Enter the name for the created group}"
    bind $itk_component(l_cbase_c) <Enter> " set [list [scope helpvar]] {Enter the base of the created cylindrical pattern}"
    bind $itk_component(l_cobj_c) <Enter> " set [list [scope helpvar]] {Enter the coordinates of center of object to be duplicated}"
    bind $itk_component(cb_rot_c) <Enter> " set [list [scope helpvar]] {Select to rotate the duplicates}"
    bind $itk_component(l_numaz_c) <Enter> " set [list [scope helpvar]] {Enter the number of azimuth angles to be used}"
    bind $itk_component(l_delaz_c) <Enter> " set [list [scope helpvar]] {Enter the azimuth delta angle (degrees)}"
    bind $itk_component(cb_list_c) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on a list of azimuths}"
    bind $itk_component(l_listaz_c) <Enter> " set [list [scope helpvar]] {Enter a list of azimuths to be used}"
    bind $itk_component(l_startr_c) <Enter> " set [list [scope helpvar]] {Enter starting radius}"
    bind $itk_component(rb_radius_c) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on number and delta of radii}"
    bind $itk_component(l_radius_c) <Enter> " set [list [scope helpvar]] {Enter the number of radii to be used}"
    bind $itk_component(l_delta_c) <Enter> " set [list [scope helpvar]] {Enter the delta angle (degrees)}"
    bind $itk_component(rb_radlist_c) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on a list of radii}"
    bind $itk_component(l_radlist_c) <Enter> " set [list [scope helpvar]] {Enter a list of radii to be used}"
    bind $itk_component(l_startaz_c) <Enter> " set [list [scope helpvar]] {Enter starting azimuth direction vector}"
    bind $itk_component(l_heightdir_c) <Enter> " set [list [scope helpvar]] {Enter direction vector for cylinder height}"
    bind $itk_component(l_starth_c) <Enter> " set [list [scope helpvar]] {Enter starting height of the cylinder}"
    bind $itk_component(l_obj_c) <Enter> " set [list [scope helpvar]] {Enter the list of objects to duplicate}"
    bind $itk_component(rb_height_c) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on number and delta of heights}"
    bind $itk_component(l_hnum_c) <Enter> " set [list [scope helpvar]] {Enter the number of heights to be used}"
    bind $itk_component(l_dnum_c) <Enter> " set [list [scope helpvar]] {Enter the delta of heights to be used}"
    bind $itk_component(rb_lnum_c) <Enter> " set [list [scope helpvar]] {Select to generate creation points based on a list of heights}"
    bind $itk_component(l_lnum_c) <Enter> " set [list [scope helpvar]] {Enter a list of heights}"
    bind $itk_component(l_sstring_c) <Enter> " set [list [scope helpvar]] {Enter a string appearing in the objects to duplicate that you want to change (empty is OK)}"
    bind $itk_component(l_rstring_c) <Enter> " set [list [scope helpvar]] {Enter the string you want to replace the above string with (empty is OK)}"
    bind $itk_component(l_incr_c) <Enter> " set [list [scope helpvar]] {Enter value to use in incrementing primitive/region numbers (0 is OK)}"

    foreach obj { l_combo_c \
		      l_group_c \
		      l_cbase_c \
		      l_cobj_c \
		      cb_rot_c \
		      l_numaz_c \
		      l_delaz_c \
		      cb_list_c \
		      l_listaz_c \
		      l_startr_c \
		      rb_radius_c \
		      l_radius_c \
		      l_delta_c \
		      rb_radlist_c \
		      l_radlist_c  \
		      l_startaz_c \
		      l_heightdir_c \
		      l_starth_c \
		      l_obj_c \
		      rb_height_c \
		      l_hnum_c \
		      l_dnum_c \
		      rb_lnum_c \
		      l_lnum_c \
		      l_sstring_c \
		      l_rstring_c \
		      l_incr_c \
		      rb_num_c} {

	bind $itk_component($obj) <Leave> " set [list [scope helpvar]] {} "
    }


    grid $itk_component(l_help) -in $itk_component(f_status) -sticky w -row 0 -column 0
    grid $itk_component(fb_progress) -in $itk_component(f_status) -sticky e -row 0 -column 1
    grid $itk_component(f_status) -sticky ew
    grid rowconfigure $itk_component(f_status) 0 -weight 1
    grid columnconfigure $itk_component(f_status) 0 -weight 1

    update
    update idletasks
    [code $this switch_states f_dir_r f_list_r]
    [code $this switch_states f_num_s f_list_s]
    [code $this switch_states f_radius_s f_radlist_s]
    [code $this switch_states f_num_c f_list_c]
    [code $this switch_states f_radius_c f_radlist_c]
    [code $this switch_states f_height_c f_lnum_c]
}


body pattern_control::destructor {} {
    unset combovar_r
}

::itcl::configbody pattern_control::ged {
    set ::cadwidgets::ged $itk_option(-ged)
}

body pattern_control::update_depth { box level } {
    switch -- $box {
	"r" {
	    set combovar_r $level
	}
	"s" {
	    set combovar_s $level
	}
	"c" {
	    set combovar_c $level
	}
    }
}

body pattern_control::apply_rect {} {
    # The C clone implementation supplies current count and total to the
    # callback, so no Tcl-side total calculation is needed.
    $itk_component(fb_progress) reset
    set ::clone_progress_callback \
	[list _clone_progress_update $itk_component(fb_progress)]

    set cmd [list pattern_rect -$combovar_r]

    lappend cmd -g [string trim $group_r]

    lappend cmd -xdir $xdir_r -ydir $ydir_r -zdir $zdir_r

    if { $dirtype_r == 1 } {
	lappend cmd -nx [string trim $nxdir_r] -dx [string trim $dxdir_r] -ny [string trim $nydir_r] -dy [string trim $dydir_r] -nz [string trim $nzdir_r] -dz [string trim $dzdir_r]
    } else {
	lappend cmd -lx $xlist_r -ly $ylist_r -lz $zlist_r
    }

    if { [string length [string trim $source_string_r]] != 0 } {
	lappend cmd -s [string trim $source_string_r] [string trim $rep_string_r]
    }

    if { [string length [string trim $increment_r]] != 0 } {
	lappend cmd -i [string trim $increment_r]
    }

    foreach obj $obj_r {
	lappend cmd $obj
    }

    eval $cmd
    unset -nocomplain ::clone_progress_callback
}

body pattern_control::apply_sph {} {
    # The C clone implementation supplies current count and total to the
    # callback, so no Tcl-side total calculation is needed.
    $itk_component(fb_progress) reset
    set ::clone_progress_callback \
	[list _clone_progress_update $itk_component(fb_progress)]

    set cmd [list pattern_sph -$combovar_s]
    lappend cmd -g [string trim $group_s]

    if { [string length [string trim $source_string_s]] != 0 } {
	lappend cmd -s [string trim $source_string_s] [string trim $rep_string_s]
    }

    if { [string length [string trim $increment_s]] != 0 } {
	lappend cmd -i [string trim $increment_s]
    }

    lappend cmd -center_pat $cpatt_s
    lappend cmd -center_obj $cobj_s

    if { $rotaz_s == 1 } {
	lappend cmd -rotaz
    }

    if { $rotel_s == 1 } {
	lappend cmd -rotel
    }

    if { $azel_s == 1 } {
	lappend cmd -naz [string trim $numaz_s] -daz [string trim $delaz_s] -nel [string trim $numel_s] -del [string trim $delel_s]
    } else {
	lappend cmd -laz [string trim $lsaz_s] -lel [string trim $lsel_s]
    }

    if { $radii_s == 1 } {
	lappend cmd -nr [string trim $radnum_s] -dr [string trim $raddel_s]
    } else {
	lappend cmd -lr [string trim $radlist_s]
    }

    lappend cmd -start_az [string trim $startaz_s] -start_el [string trim $startel_s] -start_r [string trim $startr_s]
    foreach obj $obj_s {
	lappend cmd $obj
    }

    eval $cmd
    unset -nocomplain ::clone_progress_callback
}

body pattern_control::apply_cyl {} {
    # The C clone implementation supplies current count and total to the
    # callback, so no Tcl-side total calculation is needed.
    $itk_component(fb_progress) reset
    set ::clone_progress_callback \
	[list _clone_progress_update $itk_component(fb_progress)]

    set cmd [list pattern_cyl -$combovar_c]
    lappend cmd -g [string trim $group_c]

    if { [string length [string trim $source_string_c]] != 0 } {
	lappend cmd -s [string trim $source_string_c] [string trim $rep_string_c]
    }

    if { [string length [string trim $increment_c]] != 0 } {
	lappend cmd -i [string trim $increment_c]
    }

    if { $rot_c == 1 } {
	lappend cmd -rot
    }

    lappend cmd -center_obj [string trim $cobj_c]
    lappend cmd -center_base [string trim $cbase_c]
    lappend cmd -height_dir [string trim $heightdir_c]
    lappend cmd -start_az_dir [string trim $startaz_c]

    if { $azel_c == 1 } {
	lappend cmd -naz [string trim $numaz_c] -daz [string trim $delaz_c]
    } else {
	lappend cmd -laz [string trim $lsaz_c]
    }

    lappend cmd -sr [string trim $e_startr_c]

    if { $radii_c == 1 } {
	lappend cmd -nr [string trim $radnum_c] -dr [string trim $raddel_c]
    } else {
	lappend cmd -lr [string trim $radlist_c]
    }

    lappend cmd -sh [string trim $starth_c]

    if { $height_c == 1 } {
	lappend cmd -nh [string trim $hnum_c] -dh [string trim $dnum_c]
    } else {
	lappend cmd -lh [string trim $lnum_c]
    }

    foreach obj $obj_c {
	lappend cmd $obj
    }

    eval $cmd
    unset -nocomplain ::clone_progress_callback
}

body pattern_control::frame_disable { frame_name } {
    switch -- $frame_name {
	"f_list_r" {
	    foreach obj { l_xlist_r \
			      l_ylist_r \
			      l_zlist_r \
			      e_xlist_r \
			      e_ylist_r \
			      e_zlist_r  } {

		$itk_component($obj) configure -state disabled
	    }

	}
	"f_dir_r" {
	    foreach obj { l_nxdir_r \
			      l_nydir_r \
			      l_nzdir_r \
			      e_nxdir_r \
			      e_nydir_r \
			      e_nzdir_r \
			      l_dxdir_r \
			      l_dydir_r \
			      l_dzdir_r \
			      e_dxdir_r \
			      e_dydir_r \
			      e_dzdir_r } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_num_s" {
	    foreach obj { l_numaz_s \
			      e_numaz_s \
			      l_numel_s \
			      e_numel_s \
			      l_delaz_s \
			      e_delaz_s \
			      l_delel_s \
			      e_delel_s } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_list_s" {
	    foreach obj { l_listaz_s \
			      e_listaz_s \
			      l_listel_s \
			      e_listel_s } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_radius_s" {
	    foreach obj { l_radius_s \
			      e_radius_s \
			      l_delta_s  \
			      e_delta_s  } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_radlist_s" {
	    foreach obj { l_radlist_s \
			      e_radlist_s } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_num_c" {
	    foreach obj { l_numaz_c \
			      e_numaz_c \
			      l_delaz_c \
			      e_delaz_c } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_list_c" {
	    foreach obj { l_listaz_c \
			      e_listaz_c } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_height_c" {
	    foreach obj { l_hnum_c \
			      e_hnum_c \
			      l_dnum_c \
			      e_dnum_c } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_radius_c" {
	    foreach obj { l_radius_c \
			      e_radius_c \
			      l_delta_c  \
			      e_delta_c } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_radlist_c" {
	    foreach obj { l_radlist_c \
			      e_radlist_c } {

		$itk_component($obj) configure -state disabled
	    }
	}
	"f_lnum_c" {
	    foreach obj { l_lnum_c \
			      e_lnum_c } {

		$itk_component($obj) configure -state disabled
	    }
	}
	default {
	    error "$frame_name not found."
	}
    }
}


body pattern_control::frame_enable { frame_name } {
    switch -- $frame_name {
	"f_list_r" {
	    foreach obj { l_xlist_r \
			      l_ylist_r \
			      l_zlist_r \
			      e_xlist_r \
			      e_ylist_r \
			      e_zlist_r  } {

		$itk_component($obj) configure -state normal
	    }

	}
	"f_dir_r" {
	    foreach obj { l_nxdir_r \
			      l_nydir_r \
			      l_nzdir_r \
			      e_nxdir_r \
			      e_nydir_r \
			      e_nzdir_r \
			      l_dxdir_r \
			      l_dydir_r \
			      l_dzdir_r \
			      e_dxdir_r \
			      e_dydir_r \
			      e_dzdir_r } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_num_s" {
	    foreach obj { l_numaz_s \
			      e_numaz_s \
			      l_numel_s \
			      e_numel_s \
			      l_delaz_s \
			      e_delaz_s \
			      l_delel_s \
			      e_delel_s } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_list_s" {
	    foreach obj { l_listaz_s \
			      e_listaz_s \
			      l_listel_s \
			      e_listel_s } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_radius_s" {
	    foreach obj { l_radius_s \
			      e_radius_s \
			      l_delta_s  \
			      e_delta_s  } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_radlist_s" {
	    foreach obj { l_radlist_s \
			      e_radlist_s } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_num_c" {
	    foreach obj { l_numaz_c \
			      e_numaz_c \
			      l_delaz_c \
			      e_delaz_c } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_list_c" {
	    foreach obj { l_listaz_c \
			      e_listaz_c } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_height_c" {
	    foreach obj { l_hnum_c \
			      e_hnum_c \
			      l_dnum_c \
			      e_dnum_c } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_radius_c" {
	    foreach obj { l_radius_c \
			      e_radius_c \
			      l_delta_c  \
			      e_delta_c } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_radlist_c" {
	    foreach obj { l_radlist_c \
			      e_radlist_c } {

		$itk_component($obj) configure -state normal
	    }
	}
	"f_lnum_c" {
	    foreach obj { l_lnum_c \
			      e_lnum_c } {

		$itk_component($obj) configure -state normal
	    }
	}
	default {
	    error "$frame_name not found."
	}
    }

}

body pattern_control::switch_states { frame_on frame_off } {
    frame_enable $frame_on
    frame_disable $frame_off
}
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
