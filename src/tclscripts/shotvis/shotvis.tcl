#                     S H O T V I S . T C L
# BRL-CAD
#
# Copyright (c) 2019-2021 United States Government as represented by
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
# Description -
#
# A GUI for visual interactive shotline editing.
#
package require Itcl
package require Itk

init_vmath

set last_time 0

proc init {} {
    # load global
}
init

proc elapsed {str} {
    global ::last_time

    if {$last_time == 0} {
	set elapsed 0.0
    } else {
	set elapsed [expr [clock milliseconds] - $last_time]
    }
    puts "$elapsed\t$str"
    set last_time [clock milliseconds]
}

proc random_color_val {} {
    return [expr round(rand() * 128) + 64]
}

proc random_color {} {
    set red [random_color_val]
    set grn [random_color_val]
    set blu [random_color_val]

    return "$red/$grn/$blu"
}

proc randomize_color {obj} {
    set obj [file tail $obj]
    set rcolor [random_color]
    attr set $obj color $rcolor
    attr set $obj rgb $rcolor
}

proc randomize_region_colors {} {
    search | -type r -exec randomize_color "{}" ";"
}

proc name_exists {name} {
    return [expr ![catch {get $name}]]
}

proc rand_char {} {
    set charset {a b c d e f g h i j k l m n o p q r s t u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z 0 1 2 3 4 5 6 7 8 9} 
    set max_index 61
    set choice [expr round(rand() * $max_index)]
    return [lindex $charset $choice]
}

# generate an object name virtually guaranteed to be unique in the
# database
proc rand_name {base suf} {
    set randlist [list]
    for {set i 0} {$i < 7} {incr i} {
	lappend randlist [rand_char]
    }

    return "${base}[join $randlist ""]$suf"
}

proc unique_name {base suf} {
    set new_name "${base}${suf}"
    set cnt 1

    while {[name_exists $new_name]} {
	set new_name "${base}_${cnt}$suf"
	incr cnt
    }
    return $new_name
}

proc make_std_arrow {basename start_pt end_pt} {
    set height [vsub2 $end_pt $start_pt]
    set dir [vunitize $height]
    set len [magnitude $height]
    return [ShotVis::make_arrow $basename $start_pt $dir $len]
}

proc save_nirt {args} {
    if {[llength $args] != 1} {
	puts "Usage: save_nirt <basename>"
	return
    }
    set basename [lindex $args 0]

    if {[name_exists $basename]} {
	puts "Error: $basename already exists."
	return
    }

    set az 0
    set el 0
    set x 0
    set y 0
    set z 0
    set ray_origin {}
    set ray_dir {}
    set ray_enter {}
    set ray_exit {}
    set hit_regions {}

    set region_count 0
    set in_regions false
    set last_region_hit {}

    # run nirt
    set nirt_output [nirt -b]

    # echo nirt output
    puts $nirt_output
    update

    # parse nirt output
    foreach line [split $nirt_output "\n"] {
	# extract ray info
	if {[regexp {Origin \(x y z\) = \(([^ ]+) *([^ ]+) *([^ ]+)\)} $line match_str x y z]} {
	    set ray_origin "$x $y $z"
	}
	if {[regexp {Direction \(x y z\) = \(([^ ]+) *([^ ]+) *([^ ]+)\) *\(az el\) = \(([^ ]+) *([^ ]+)\)} $line match_str x y z az el]} {
	    set ray_dir "$x $y $z"
	}

	# extract hit regions
	if {$in_regions && [regexp {^([^ ]+) *\( *([^ ]+) *([^ ]+) *([^ ]+)\) *[0-9.]+ *[0-9.]+} $line match_str rname x y z]} {
	    lappend hit_regions $rname
	    set last_region_hit $rname

	    # extract first/last region entry point
	    if {$region_count == 0} {
		set ray_enter "$x $y $z"
		set ray_exit $ray_enter
	    } else {
		set ray_exit "$x $y $z"
	    }
	    incr region_count
	}

	if {[regexp {Region Name} $line]} {
	    set in_regions true
	}
    }

    # estimate actual exit point as last entry point + the max
    # dimension of the corresponding object's bbox
    if {![catch {bb -q -d $last_region_hit} bb_res]} {
	set dim 0
	set max_dim 0
	foreach line [split $bb_res "\n"] {
	    if {[regexp {Length: ([0-9.]+) mm} $line match_str dim]} {
		if {$dim > $max_dim} {
		    set max_dim $dim
		}
	    }
	}
	set dir [ShotVis::direction_vector {*}[lrange [view aet] 0 1]]
	set new_exit [vcomb2 1 $ray_exit $max_dim $dir]
	set ray_exit $new_exit
    }

    # keep only unique region list
    set all_reg $hit_regions
    set hit_regions [lsort -unique $all_reg]

    # create result comb
    set shotline [make_std_arrow $basename $ray_enter $ray_exit]
    g $basename $shotline {*}$hit_regions

    # set attributes
    attr set $basename "view_center" [view center]
    attr set $basename "view_aet" [view aet]
    attr set $basename "ray_origin" $ray_origin
    attr set $basename "ray_dir" $ray_dir
    attr set $basename "ray_az_el" "$az $el"
}

# replace existing class
catch {delete class ShotVis} error

::itcl::class ShotVis {
    inherit ::itk::Toplevel

    itk_option define -basename basename Basename ""

    # import vmath constants
    global M_PI_2
    common M_PI_2 $M_PI_2

    global RAD2DEG
    common RAD2DEG $RAD2DEG

    global DEG2RAD
    common DEG2RAD $DEG2RAD

    common epsilon 1.0e-20

    private {
	variable basename

	variable default_text_color
	variable invalid_text_color "red"
	variable unicode_degree "\u00B0"
	variable unicode_identical_to "\u2261"
	variable threat_draw_len 1000
	variable threat_start
	variable threat_dir
	variable max_ke_angle 0

	variable threat_az 0
	variable threat_el 90
	variable az_str
	variable el_str
	variable dir_x_str
	variable dir_y_str
	variable dir_z_str
	variable start_x_str
	variable start_y_str
	variable start_z_str
	variable threat_len_str
	variable matedit_disabled_widgets {}

	variable garbage_comb {}
	variable root_comb {}
	variable nirt_comb {}
	variable threat_comb {}
	variable threat_cyl {}
	variable threat_cone {}
	variable max_ke_comb {}
	variable max_ke_cyl {}
	variable max_ke_cone {}
    }

    constructor {args} {}
    destructor {}

    proc near_zero {val} {}
    proc isFloat {val} {}
    proc isInClosedRange {val min max} {}
    proc angleIsValid {val min max} {}
    proc atan2 {y x} {}
    proc vec2ae {v} {}
    proc direction_vector {az el} {}
    proc rcc_len {rcc_name} {}
    proc rcc_center {rcc_name} {}
    proc rcc_direction {rcc_name} {}
    proc make_arrow {basename start_pt dir len} {}

    method load_vis {} {}
    method update_shot {} {}
    method redraw {} {}

    private {
	method make_matedit_frame {} {}
	method make_azel_frame {} {}
	method make_direction_frame {} {}
	method make_start_frame {} {}
	method make_nirt_frame {} {}

	method new_shotline {} {}

	method set_start_point {x y z} {}
	method set_dir {x y z} {}
	method set_ae {az el} {}
	method update_ae {} {}
	method update_dir {} {}
	method nirt_threat {} {}
	method nirt_view {} {}

	method validate_start_point {coord_idx val} {}
	method validate_dir_val {coord_idx val} {}
	method validate_ae_val {coord_idx val} {}
	method validate_draw_len {val} {}

	method start_matedit {} {}
	method accept_matedit {} {}
	method reject_matedit {} {}

	method trash {obj} {}
	method replace_obj {old new} {}
	method our_who {} {}
	method our_members {comb} {}
	method kill_old_shot {shot_comb} {}
    }
}

::itcl::body ShotVis::make_matedit_frame {} {
    itk_component add matedit_frame {
	ttk::labelframe $itk_component(threat_tab).matedit_frame \
	-text "Matrix Edit" \
	-padding 4
    } {}

    itk_component add matedit {
	ttk::button $itk_component(matedit_frame).matrix_edit \
	-text "Start" \
	-command [::itcl::code $this start_matedit]
    } {}

    lappend matedit_disabled_widgets matedit

    itk_component add accept_edit {
	ttk::button $itk_component(matedit_frame).edit_accept \
	-text "Accept" \
	-command [::itcl::code $this accept_matedit]
    } {}

    itk_component add reject_edit {
	ttk::button $itk_component(matedit_frame).edit_reject \
	-text "Reject" \
	-command [::itcl::code $this reject_matedit]
    } {}

    $itk_component(accept_edit) state disabled
    $itk_component(reject_edit) state disabled

    pack $itk_component(matedit) -side left
    pack $itk_component(accept_edit) -side left
    pack $itk_component(reject_edit) -side left
}

::itcl::body ShotVis::make_azel_frame {} {
    itk_component add threat_azel {
	ttk::frame $itk_component(threat_direction).azel_frame \
	-padding 2
    } {}

    itk_component add az_label {
	ttk::label $itk_component(threat_azel).az_label \
	-text "Az$unicode_degree" \
	-padding 2
    } {}

    itk_component add az_box {
	ttk::entry $itk_component(threat_azel).az_entry \
	-textvariable [::itcl::scope az_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_ae_val 0 %P]
    } {}

    itk_component add el_label {
	ttk::label $itk_component(threat_azel).el_label \
	-text "El/AoF$unicode_degree" \
	-padding 2
    } {}

    itk_component add el_box {
	ttk::entry $itk_component(threat_azel).el_entry \
	-textvariable [::itcl::scope el_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_ae_val 1 %P]
    } {}

    lappend matedit_disabled_widgets az_box
    lappend matedit_disabled_widgets el_box

    grid $itk_component(az_label) $itk_component(az_box) -sticky nsew -pady {0 2}
    grid $itk_component(el_label) $itk_component(el_box) -sticky nsew

    grid configure $itk_component(az_label) $itk_component(el_label) -sticky nse
    grid columnconfigure $itk_component(threat_azel) 1 -weight 1
}

::itcl::body ShotVis::make_direction_frame {} {
    itk_component add threat_direction {
	ttk::labelframe $itk_component(threat_tab).threat_direction \
	-text "Direction"
    } {}

    make_azel_frame

    itk_component add dir_xbox {
	ttk::entry $itk_component(threat_direction).dir_xentry \
	-textvariable [::itcl::scope dir_x_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_dir_val 0 %P]
    } {}

    itk_component add dir_ybox {
	ttk::entry $itk_component(threat_direction).dir_yentry \
	-textvariable [::itcl::scope dir_y_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_dir_val 1 %P]
    } {}

    itk_component add dir_zbox {
	ttk::entry $itk_component(threat_direction).dir_zentry \
	-textvariable [::itcl::scope dir_z_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_dir_val 2 %P]
    } {}

    itk_component add dir_xlabel {
	ttk::label $itk_component(threat_direction).dir_xlabel \
	-text "X" \
	-padding 2
    } {}

    itk_component add dir_ylabel {
	ttk::label $itk_component(threat_direction).dir_ylabel \
	-text "Y" \
	-padding 2
    } {}

    itk_component add dir_zlabel {
	ttk::label $itk_component(threat_direction).dir_zlabel \
	-text "Z" \
	-padding 2
    } {}

    itk_component add eq_label {
	ttk::label $itk_component(threat_direction).equal_label \
	-text "$unicode_identical_to" \
	-padding 2
    } {}

    itk_component add draw_len {
	ttk::frame $itk_component(threat_direction).draw_length_frame \
	-padding 2
    } {}

    itk_component add draw_len_box {
	ttk::entry $itk_component(draw_len).entry \
	-textvariable [::itcl::scope threat_len_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_draw_len %P]
    } {}

    itk_component add draw_len_label {
	ttk::label $itk_component(draw_len).label \
	-text "Draw Length (mm)" \
	-padding 2
    } {}

    lappend matedit_disabled_widgets dir_xbox
    lappend matedit_disabled_widgets dir_ybox
    lappend matedit_disabled_widgets dir_zbox
    lappend matedit_disabled_widgets draw_len_box

    pack $itk_component(draw_len_label) -side left
    pack $itk_component(draw_len_box) -side left

    grid $itk_component(threat_azel) x                         $itk_component(dir_xlabel) $itk_component(dir_xbox) -sticky ew
    grid ^                           $itk_component(eq_label)  $itk_component(dir_ylabel) $itk_component(dir_ybox) -sticky ew
    grid ^                           x                         $itk_component(dir_zlabel) $itk_component(dir_zbox) -sticky ew
    grid $itk_component(draw_len)    -                         -                          -                        -sticky w

    grid columnconfigure $itk_component(threat_direction) 0 -weight 1
    grid columnconfigure $itk_component(threat_direction) 3 -weight 1
}

::itcl::body ShotVis::make_start_frame {} {
    itk_component add start_edit_frame {
	ttk::frame $itk_component(threat_start).edit_frame
    } {}

    itk_component add start_xyz {
	ttk::frame $itk_component(threat_start).start_xyz \
	-padding {2 16 2 4}
    } {}

    itk_component add start_xlabel {
	ttk::label $itk_component(start_xyz).xlabel \
	-text "X" \
	-padding 2
    } {}

    itk_component add start_xbox {
	ttk::entry $itk_component(start_xyz).xentry \
	-textvariable [::itcl::scope start_x_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_start_point 0 %P]
    } {}

    itk_component add start_ylabel {
	ttk::label $itk_component(start_xyz).ylabel \
	-text "Y" \
	-padding 2
    } {}

    itk_component add start_ybox {
	ttk::entry $itk_component(start_xyz).yentry \
	-textvariable [::itcl::scope start_y_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_start_point 1 %P]
    } {}

    itk_component add start_zlabel {
	ttk::label $itk_component(start_xyz).zlabel \
	-text "Z" \
	-padding 2
    } {}

    itk_component add start_zbox {
	ttk::entry $itk_component(start_xyz).zentry \
	-textvariable [::itcl::scope start_z_str] \
	-validate key \
	-validatecommand [::itcl::code $this validate_start_point 2 %P]
    } {}

    lappend matedit_disabled_widgets start_xbox
    lappend matedit_disabled_widgets start_ybox
    lappend matedit_disabled_widgets start_zbox

    pack $itk_component(start_xyz) -fill x -anchor n

    grid $itk_component(start_xlabel) $itk_component(start_xbox) $itk_component(start_ylabel) $itk_component(start_ybox) $itk_component(start_zlabel) $itk_component(start_zbox) -sticky ew
    grid columnconfigure $itk_component(start_xyz) all -weight 0
    grid columnconfigure $itk_component(start_xyz) 1 -weight 1
    grid columnconfigure $itk_component(start_xyz) 3 -weight 1
    grid columnconfigure $itk_component(start_xyz) 5 -weight 1
}

::itcl::body ShotVis::make_nirt_frame {} {
    itk_component add nirt_threat {
	ttk::button $itk_component(nirt_tab).nirt_threat \
	-text "Nirt Along Threat" \
	-command [::itcl::code $this nirt_threat]
    } {}

    itk_component add nirt_view {
	ttk::button $itk_component(nirt_tab).nirt_view \
	-text "Nirt Current View" \
	-command [::itcl::code $this nirt_view]
    } {}

    pack $itk_component(nirt_threat) -anchor nw
    pack $itk_component(nirt_view) -anchor nw
}

::itcl::body ShotVis::destructor {} {
    if {$root_comb != $basename} {
	kill_old_shot $garbage_comb
	mv $root_comb $basename
    } else {
	kill $garbage_comb
    }
}

::itcl::body ShotVis::constructor {args} {
    itk_component add main {
	ttk::frame $itk_interior.shotvis_frame \
	-padding 4
    } {}

    itk_component add notebook {
	ttk::notebook $itk_component(main).notebook
    } {}

    itk_component add threat_tab {
	ttk::frame $itk_component(notebook).threat \
	-padding 4
    } {}

    lappend matedit_disabled_widgets notebook

    itk_component add nirt_tab {
	ttk::frame $itk_component(notebook).nirt \
	-padding 4
    } {}

    make_nirt_frame

    itk_component add threat_start {
	ttk::labelframe $itk_component(threat_tab).threat_start_labelframe \
	-text "Start Point"
    } {}

    itk_component add threat_max_ke {
	ttk::labelframe $itk_component(threat_tab).threat_maxke \
	-text "Max KE"
    } {}

    make_matedit_frame
    make_direction_frame
    make_start_frame

    pack $itk_component(main) -expand yes -fill both
    pack $itk_component(notebook) -expand yes -fill both

    $itk_component(notebook) add $itk_component(threat_tab) -text "Threat"
    $itk_component(notebook) add $itk_component(nirt_tab) -text "Nirt"

    pack $itk_component(matedit_frame) -fill x -anchor n -pady {8 16}
    pack $itk_component(threat_direction) -fill x -anchor n
    pack $itk_component(threat_start) -fill x -anchor n -pady 16

    itk_initialize {*}$args
    set basename $itk_option(-basename)

    $this configure -title "Edit Shotline Visualization - $basename"

    set default_text_color [ttk::style lookup [ttk::style theme use] -foreground]

    if {[llength [who]] == 0} {
	view aet 35 25 0
    }

    set garbage_comb [rand_name "${basename}_garbage" ""]
    put $garbage_comb comb region no tree {}
    #hide $garbage_comb

    if {[name_exists $basename]} {
	set root_comb $basename
	load_vis
    } else {
	# make default visualization
	set_start_point "0" "0" "0"
	set_dir "0" "0" "-1"
	set_ae "0" "90"
	set threat_len_str $threat_draw_len

	update_shot
    }

    autoview

    bind $itk_component(notebook) <<NotebookTabChanged>> [::itcl::code $this redraw]
}

::itcl::body ShotVis::our_members {comb} {
    # Find all comb members except references to nirted objects, i.e.
    # members we "own".
    return [search $comb -not \( -below -attr view_center -and -not -path \*.shotline\* \)]
}

# Delete members of an outdated shot combination. Faster than
# "killtree", because we can make educated assumptions about which
# members are referenced elsewhere without having to search the
# database.
::itcl::body ShotVis::kill_old_shot {shot_comb} {
    set shot_members [our_members $shot_comb]
    set cur_members [lsort [our_members $root_comb]]

    set unreferenced {}
    foreach obj $shot_members {
	if {[lsearch -exact -sorted $cur_members $obj] == -1} {
	    lappend unreferenced $obj
	}
    }
    kill {*}$unreferenced
}

::itcl::body ShotVis::near_zero {val} {
    return [expr $val > -$epsilon && $val < $epsilon]
}

::itcl::body ShotVis::atan2 {y x} {
    # bn_atan2
    if {[near_zero $x]} {
	if {y < -$epsilon} {
	    return -$M_PI_2
	}
	if {y > $epsilon} {
	    return $M_PI_2
	}
	return 0.0
    }
    return [expr atan2($y, $x)]
}

::itcl::body ShotVis::vec2ae {v} {
    # bn_ae_vec
    set x [lindex $v 0]
    set y [lindex $v 1]
    set z [lindex $v 2]

    set az [expr [atan2 $y $x] * $RAD2DEG]

    if {$az < 0} {
	set az [expr $az + 360]
    } elseif {$az >= 360} {
	set az [expr $az - 360]
    }
    set el [expr [atan2 $z [expr hypot($x, $y)]] * $RAD2DEG]

    if {[near_zero $az]} {
	set az 0.0
    }
    if {[near_zero $el]} {
	set el 0.0
    }

    return [list $az $el]
}

# az el describes approach direction
# vector points opposite
::itcl::body ShotVis::direction_vector {az el} {
    set az [expr $az * $DEG2RAD]
    set el [expr $el * $DEG2RAD]

    # bn_vec_ae
    set rtemp [expr cos($el)]
    set vx [expr $rtemp * cos($az)]
    set vy [expr $rtemp * sin($az)]
    set vz [expr sin($el)]

    set vect [list $vx $vy $vz]
    set v [vscale [vunitize $vect] -1]

    for {set i 0} {$i < 3} {incr i} {
	if {[near_zero [lindex $v $i]]} {
	    lset v $i 0.0
	}
    }

    return $v
}


::itcl::body ShotVis::isFloat {str} {
    return [regexp -- {^[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)$} $str]
}

::itcl::body ShotVis::isInClosedRange {val min max} {
    return [expr $val >= $min && $val <= $max]
}

::itcl::body ShotVis::angleIsValid {val min max} {
    return [expr [isFloat $val] && [isInClosedRange $val $min $max]]
}

::itcl::body ShotVis::validate_draw_len {val} {
    if {[isFloat $val] && $val > 0} {
	set threat_draw_len $val
	$itk_component(draw_len_box) configure -foreground $default_text_color
	update_shot
    } else {
	$itk_component(draw_len_box) configure -foreground $invalid_text_color
    }
    return true
}

::itcl::body ShotVis::start_matedit {} {
    foreach component $matedit_disabled_widgets {
	$itk_component($component) state disabled
    }
    $itk_component(accept_edit) state !disabled
    $itk_component(reject_edit) state !disabled

    set cyl_path [lindex [search /$threat_comb -name \*.cyl] 0]
    oed /$threat_comb "/[join [lrange [split $cyl_path "/"] 2 end] "/"]"
}

::itcl::body ShotVis::accept_matedit {} {
    $itk_component(accept_edit) state disabled
    $itk_component(reject_edit) state disabled

    accept
    xpush $threat_comb

    if {$nirt_comb != ""} {

    }

    foreach component $matedit_disabled_widgets {
	$itk_component($component) state !disabled
    }

    load_vis
}

::itcl::body ShotVis::reject_matedit {} {
    $itk_component(accept_edit) state disabled
    $itk_component(reject_edit) state disabled

    reject

    foreach component $matedit_disabled_widgets {
	$itk_component($component) state !disabled
    }
}

::itcl::body ShotVis::nirt_threat {} {
    set old_center [view center]
    set old_aet [view aet]

    # look down threat
    set rcc_name [lindex [search $threat_comb -name \*.cyl] 0]

    set rcc_center [rcc_center $rcc_name]
    view center {*}$rcc_center
    view aet {*}[vec2ae [vscale [rcc_direction $rcc_name] -1]] 0

    # erase threat
    erase {*}[our_members $root_comb]
    update

    # do nirt
    set old_nirt_name [lindex [search $root_comb -not -path \*$threat_comb\* -attr view_center] 0]

    set nirt_name [rand_name "${basename}_nirt_" ""]
    save_nirt "$nirt_name"

    # update nirt comb
    if {$old_nirt_name != ""} {
	rm $root_comb $old_nirt_name
	trash $old_nirt_name
    }
    comb $root_comb u $nirt_name
    set nirt_comb $nirt_name

    # restore view
    view center {*}$old_center
    view aet {*}$old_aet

    redraw
}

::itcl::body ShotVis::nirt_view {} {
    # erase threat
    erase {*}[our_members $root_comb]
    update

    # do nirt
    set old_nirt_name [lindex [search $root_comb -not -path \*$threat_comb\* -attr view_center] 0]

    set nirt_name [rand_name "${basename}_nirt_" ""]
    save_nirt "$nirt_name"

    # update nirt comb
    if {$old_nirt_name != ""} {
	rm $root_comb $old_nirt_name
	trash $old_nirt_name
    }
    comb $root_comb u $nirt_name
    set nirt_comb $nirt_name

    # replace invalidated threat shotline with nirt's shotline
    set threat_cyl [rand_name "${threat_comb}_" ".cyl"]
    set threat_cone [rand_name "${threat_comb}_" ".cone"]
    set nirt_cyl [lindex [search $nirt_comb -name \*.cyl] 0]
    set nirt_cone [lindex [search $nirt_comb -name \*.cone] 0]
    cp $nirt_cyl $threat_cyl
    cp $nirt_cone $threat_cone

    set threat_shotline [lindex [search $threat_comb -name \*.shotline\*] 0]
    rm $threat_comb $threat_shotline
    trash $threat_shotline

    set shotline_comb [rand_name "${basename}_threat" ".shotline"]
    put $shotline_comb comb region no tree [list u [list l $threat_cyl] [list l $threat_cone]]
    comb $threat_comb u $shotline_comb

    new_shotline

    load_vis
}

::itcl::body ShotVis::validate_start_point {coord_idx val} {
    if {[isFloat $val]} {
	lset threat_start $coord_idx $val
	switch -- $coord_idx {
	    0 {
		$itk_component(start_xbox) configure -foreground $default_text_color
	    }
	    1 {
		$itk_component(start_ybox) configure -foreground $default_text_color
	    }
	    2 {
		$itk_component(start_zbox) configure -foreground $default_text_color
	    }
	}
	update_shot
    } else {
	switch -- $coord_idx {
	    0 {
		$itk_component(start_xbox) configure -foreground $invalid_text_color
	    }
	    1 {
		$itk_component(start_ybox) configure -foreground $invalid_text_color
	    }
	    2 {
		$itk_component(start_zbox) configure -foreground $invalid_text_color
	    }
	}
    }
    return true
}

::itcl::body ShotVis::update_ae {} {
    set ae [vec2ae $threat_dir]
    set_ae [lindex $ae 0] [lindex $ae 1]
}

::itcl::body ShotVis::validate_ae_val {coord_idx val} {
    if {[isFloat $val] && $val >= -360.0 && $val <= 360.0} {
	if {$coord_idx == 0} {
	    set threat_az $val
	    $itk_component(az_box) configure -foreground $default_text_color
	} else {
	    set threat_el $val
	    $itk_component(el_box) configure -foreground $default_text_color
	}
	update_dir
	update_shot
    } else {
	if {$coord_idx == 0} {
	    $itk_component(az_box) configure -foreground $invalid_text_color
	} else {
	    $itk_component(el_box) configure -foreground $invalid_text_color
	}
    }
    return true
}

::itcl::body ShotVis::validate_dir_val {coord_idx val} {
    if {[isFloat $val]} {
	lset threat_dir $coord_idx $val

	if {![near_zero [magnitude $threat_dir]]} {
	    update_ae
	    update_shot
	}

	switch -- $coord_idx {
	    0 {
		$itk_component(dir_xbox) configure -foreground $default_text_color
	    }
	    1 {
		$itk_component(dir_ybox) configure -foreground $default_text_color
	    }
	    2 {
		$itk_component(dir_zbox) configure -foreground $default_text_color
	    }
	}
    } else {
	switch -- $coord_idx {
	    0 {
		$itk_component(dir_xbox) configure -foreground $invalid_text_color
	    }
	    1 {
		$itk_component(dir_ybox) configure -foreground $invalid_text_color
	    }
	    2 {
		$itk_component(dir_zbox) configure -foreground $invalid_text_color
	    }
	}
    }
    return true
}

::itcl::body ShotVis::new_shotline {} {
    set old_shotline $root_comb

    if {$old_shotline != ""} {
	# erase members in old tree
	set drawn [our_who]
	if {[llength $drawn] > 0} {
	    erase {*}$drawn
	}

	trash $old_shotline

	set new_shotline_comb [rand_name $basename ""]
    } else {
	set new_shotline_comb $basename
    }

    put $new_shotline_comb comb region no tree [list u [list l $threat_comb] [list l $nirt_comb]]
    set root_comb $new_shotline_comb

    attr set $threat_comb shotvis_threat 1
    if {$max_ke_angle > 0.0} {
	attr set $max_ke_comb shotvis_max_ke 1
    }

    $this configure -title "Edit Shotline Visualization - $root_comb"
}

::itcl::body ShotVis::set_start_point {x y z} {
    set start_x_str $x
    set start_y_str $y
    set start_z_str $z

    set threat_start [list $x $y $z]
}

::itcl::body ShotVis::set_dir {x y z} {
    set dir_x_str $x
    set dir_y_str $y
    set dir_z_str $z

    set threat_dir [list $x $y $z]
    set threat_dir [vunitize $threat_dir]
}

::itcl::body ShotVis::set_ae {az el} {
    set az_str $az
    set threat_az $az

    set el_str $el
    set threat_el $el
}

::itcl::body ShotVis::update_dir {} {
    set_dir {*}[direction_vector $threat_az $threat_el]
}

::itcl::body ShotVis::make_arrow {basename start_pt dir len} {
    set height [vscale $dir $len]
    set end_pt [vadd2 $start_pt $height]
    set thickness 5

    set cyl [rand_name "${basename}_" .cyl]
    in $cyl rcc {*}$start_pt {*}$height [expr $thickness / 2.0]

    set width [expr $thickness * 2]
    set height [vscale $dir [expr $width * 2.88]]

    set cone [rand_name "${basename}_" .cone]
    in $cone trc {*}$end_pt {*}$height [expr $thickness * 2] .01

    erase $cyl $cone

    # group
    set group [rand_name "${basename}_" .shotline]
    put $group comb region no tree [list u [list l $cyl] [list l $cone]]
    attr set $group color {255/0/0}
    attr set $group rgb {255/0/0}

    return $group
}

::itcl::body ShotVis::trash {obj} {
    comb $garbage_comb u $obj
}

::itcl::body ShotVis::replace_obj {old new} {
    if {[name_exists $old]} {
	killtree -f {*}$old
    }
    mvall $new $old
}

::itcl::body ShotVis::rcc_len {rcc_name} {
    return [magnitude [list {*}[get $rcc_name H]]]
}

::itcl::body ShotVis::rcc_center {rcc_name} {
    return [list {*}[get $rcc_name V]]
}

::itcl::body ShotVis::rcc_direction {rcc_name} {
    return [vunitize [list {*}[get $rcc_name H]]]
}

::itcl::body ShotVis::load_vis {} {
    set threat_comb [lindex [search $root_comb -attr shotvis_threat] 0]
    set threat_cyl [lindex [search $threat_comb -name \*.cyl] 0]
    set threat_cone [lindex [search $threat_comb -name \*.cone] 0]

    set threat_draw_len [rcc_len $threat_cyl]
    set threat_len_str $threat_draw_len

    set_start_point {*}[rcc_center $threat_cyl]

    set_dir {*}[rcc_direction $threat_cyl]
    update_ae

    set nirt_comb [lindex [search $root_comb -not -path \*$threat_comb\* -attr view_center] 0]

    redraw
}

::itcl::body ShotVis::our_who {} {
    # Identify drawn objects we own, based on basename.
    # Much faster than "search $old_shotline".
    set drawn [list]
    foreach obj [who] {
	if {[regexp $basename $obj]} {
	    lappend drawn $obj
	}
    }
    return $drawn
}

::itcl::body ShotVis::redraw {} {
    set drawn [our_who]
    if {[llength $drawn] > 0} {
	erase {*}$drawn
    }

    set draw_objs {}

    # TODO: do a name lookup instead of matching the index directly so
    # this doesn't break if the order is changed
    set cur_tab_idx [$itk_component(notebook) index current]

    switch -- $cur_tab_idx {
	0 {
	    # threat
	    lappend draw_objs $threat_comb
	}
	1 {
    	    # nirt
	    if {$nirt_comb != ""} {
		lappend draw_objs $nirt_comb
		Z
	    } else {
		lappend draw_objs $threat_comb
	    }
	}
    }

    e -R -S -L200 {*}$draw_objs
}

::itcl::body ShotVis::update_shot {} {
    set threat_arrow [make_arrow "${basename}_threat" $threat_start $threat_dir $threat_draw_len]
    set threat_comb [rand_name "${basename}_threat_" ""]
    put $threat_comb comb region no tree [list l $threat_arrow]
    attr set $threat_comb shotvis_threat 1

    if {$max_ke_angle > 0.0} {
	# make max ke arrow, derived from threat and angle
	set max_ke_dir [shotvis::max_ke_from_threat $threat_dir $max_ke_angle]
	set max_ke_name [make_arrow "${basename}_max_ke" $threat_start $max_ke_dir $threat_draw_len]
    }

    new_shotline
    redraw
}

namespace eval shotvis {
    proc vec_angle {v1 v2} {
	global RAD2DEG

	return [expr [vdot $v1 $v2] / ([magitude $v1] * [magnitude $v2]) * $RAD2DEG]
    }

    proc mat_mul {a b} {
	# bn_mat_mul
	set o [mat_idn]

	lset o 0 [expr [lindex $a 0] * [lindex $b 0] + [lindex $a 1] * [lindex $b 4] + [lindex $a 2] * [lindex $b 8] + [lindex $a 3] * [lindex $b 12]]
	lset o 1 [expr [lindex $a 0] * [lindex $b 1] + [lindex $a 1] * [lindex $b 5] + [lindex $a 2] * [lindex $b 9] + [lindex $a 3] * [lindex $b 13]]
	lset o 2 [expr [lindex $a 0] * [lindex $b 2] + [lindex $a 1] * [lindex $b 6] + [lindex $a 2] * [lindex $b 10] + [lindex $a 3] * [lindex $b 14]]
	lset o 3 [expr [lindex $a 0] * [lindex $b 3] + [lindex $a 1] * [lindex $b 7] + [lindex $a 2] * [lindex $b 11] + [lindex $a 3] * [lindex $b 15]]

	lset o 4 [expr [lindex $a 4] * [lindex $b 0] + [lindex $a 5] * [lindex $b 4] + [lindex $a 6] * [lindex $b 8] + [lindex $a 7] * [lindex $b 12]]
	lset o 5 [expr [lindex $a 4] * [lindex $b 1] + [lindex $a 5] * [lindex $b 5] + [lindex $a 6] * [lindex $b 9] + [lindex $a 7] * [lindex $b 13]]
	lset o 6 [expr [lindex $a 4] * [lindex $b 2] + [lindex $a 5] * [lindex $b 6] + [lindex $a 6] * [lindex $b 10] + [lindex $a 7] * [lindex $b 14]]
	lset o 7 [expr [lindex $a 4] * [lindex $b 3] + [lindex $a 5] * [lindex $b 7] + [lindex $a 6] * [lindex $b 11] + [lindex $a 7] * [lindex $b 15]]

	lset o 8 [expr [lindex $a 8] * [lindex $b 0] + [lindex $a 9] * [lindex $b 4] + [lindex $a 10] * [lindex $b 8] + [lindex $a 11] * [lindex $b 12]]
	lset o 9 [expr [lindex $a 8] * [lindex $b 1] + [lindex $a 9] * [lindex $b 5] + [lindex $a 10] * [lindex $b 9] + [lindex $a 11] * [lindex $b 13]]
	lset o 10 [expr [lindex $a 8] * [lindex $b 2] + [lindex $a 9] * [lindex $b 6] + [lindex $a 10] * [lindex $b 10] + [lindex $a 11] * [lindex $b 14]]
	lset o 11 [expr [lindex $a 8] * [lindex $b 3] + [lindex $a 9] * [lindex $b 7] + [lindex $a 10] * [lindex $b 11] + [lindex $a 11] * [lindex $b 15]]

	lset o 12 [expr [lindex $a 12] * [lindex $b 0] + [lindex $a 13] * [lindex $b 4] + [lindex $a 14] * [lindex $b 8] + [lindex $a 15] * [lindex $b 12]]
	lset o 13 [expr [lindex $a 12] * [lindex $b 1] + [lindex $a 13] * [lindex $b 5] + [lindex $a 14] * [lindex $b 9] + [lindex $a 15] * [lindex $b 13]]
	lset o 14 [expr [lindex $a 12] * [lindex $b 2] + [lindex $a 13] * [lindex $b 6] + [lindex $a 14] * [lindex $b 10] + [lindex $a 15] * [lindex $b 14]]
	lset o 15 [expr [lindex $a 12] * [lindex $b 3] + [lindex $a 13] * [lindex $b 7] + [lindex $a 14] * [lindex $b 11] + [lindex $a 15] * [lindex $b 15]]

	return $o
    }

    # make a tranformation matrix that rotates deg around the axis defined by the given pt and dir
    proc mat_arb_rot {axis_pt axis_dir deg} {
	global DEG2RAD

	set rad [expr $deg * $DEG2RAD]

	# bn_mat_arb_rot
	if {$rad == 0.0} {
	    return [mat_idn]
	}

	set tran1 [mat_idn]
	set tran2 [mat_idn]

	# translation to pt
	lset tran1 3 [expr -1 * [lindex $axis_pt 0]]
	lset tran1 7 [expr -1 * [lindex $axis_pt 1]]
	lset tran1 11 [expr -1 * [lindex $axis_pt 2]]

	# translation from pt
	lset tran2 3 [lindex $axis_pt 0]
	lset tran2 7 [lindex $axis_pt 1]
	lset tran2 11 [lindex $axis_pt 2]

	# rotation matrix
	set cos_ang [expr cos($rad)]
	set sin_ang [expr sin($rad)]
	set one_m_cosang [expr 1 - $cos_ang]
	set n1_sq [expr [lindex $axis_dir 0] * [lindex $axis_dir 0]]
	set n2_sq [expr [lindex $axis_dir 1] * [lindex $axis_dir 1]]
	set n3_sq [expr [lindex $axis_dir 2] * [lindex $axis_dir 2]]
	set n1_n2 [expr [lindex $axis_dir 0] * [lindex $axis_dir 1]]
	set n1_n3 [expr [lindex $axis_dir 0] * [lindex $axis_dir 2]]
	set n2_n3 [expr [lindex $axis_dir 1] * [lindex $axis_dir 2]]

	set rot [mat_idn]
	lset rot 0 [expr $n1_sq + (1.0 - $n1_sq) * $cos_ang]
	lset rot 1 [expr $n1_n2 * $one_m_cosang - [lindex $axis_dir 2] * $sin_ang]
	lset rot 2 [expr $n1_n3 * $one_m_cosang + [lindex $axis_dir 1] * $sin_ang]

	lset rot 4 [expr $n1_n2 * $one_m_cosang + [lindex $axis_dir 2] * $sin_ang]
	lset rot 5 [expr $n2_sq + (1.0 - $n2_sq) * $cos_ang]
	lset rot 6 [expr $n2_n3 * $one_m_cosang - [lindex $axis_dir 0] * $sin_ang]

	lset rot 8 [expr $n1_n3 * $one_m_cosang - [lindex $axis_dir 1] * $sin_ang]
	lset rot 9 [expr $n2_n3 * $one_m_cosang + [lindex $axis_dir 0] * $sin_ang]
	lset rot 10 [expr $n3_sq + (1.0 - $n3_sq) * $cos_ang]

	set m [mat_mul $rot $tran1]
	set m [mat_mul $tran2 $m]

	return $m
    }

    proc rot_vect_about_axis {v axis_pt axis_dir deg} {
	set m [mat_arb_rot $axis_pt $axis_dir $deg]
	return [mat4x3pnt $m $v]
    }

    proc max_ke_from_threat {threat_dir deg} {
	set up {0 0 1}
	set axis_dir [vunitize [vcross $up $threat_dir]]

	return [rot_vect_about_axis $threat_dir {0 0 0} $axis_dir $deg]
    }

    proc arrow_rcc {arrow_comb} {
	return [search $arrow_comb -depth 1 -type tgc -name \*.cyl]
    }

}


proc shotvis {args} {
    if {[llength $args] != 1} {
	puts "Usage: shotvis <basename>"
	return
    }
    set basename [lindex $args 0]

    ::ShotVis .shotvis -basename $basename
}

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
