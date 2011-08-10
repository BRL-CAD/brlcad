#                      S K T _ E D . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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


proc dot { v1_in v2_in } {
    # args are names of arrays
    upvar $v1_in v1
    upvar $v2_in v2

    return [expr { $v1(0) * $v2(0) + $v1(1) * $v2(1) } ]
}

proc dist { x1 y1 x2 y2 } {
    return [expr { sqrt( ($x1 - $x2) * ($x1 - $x2) + ($y1 - $y2) * ($y1 - $y2))}]
}

proc cross2d { v1_in v2_in } {
    # args are names of arrays
    # only the value of the 'z' component is returned
    upvar $v1_in v1
    upvar $v2_in v2
    return [expr {$v1(0) * $v2(1) - $v1(1) * $v2(0)}]
}

proc diff { v_out v1_in v2_in } {
    # args are the names of arrays
    # returns difference array in v_out
    upvar $v1_in v1
    upvar $v2_in v2
    upvar $v_out v
    set v(0) [expr {$v1(0) - $v2(0)}]
    set v(1) [expr {$v1(1) - $v2(1)}]
}

proc find_arc_center { sx sy ex ey radius center_is_left } {
    # The center must lie on the perpendicular bisector of the line connecting the two points

    # vector from start point to end point
    set s2e(0) [expr {$ex - $sx}]
    set s2e(1) [expr {$ey - $sy}]

    # mid point
    set midx [expr {($sx + $ex) / 2.0}]
    set midy [expr {($sy + $ey) / 2.0}]

    # perpendicular direction
    set dirx [expr {-($midy - $sy)}]
    set diry [expr {$midx - $sx}]

    # unitize direction vector
    set len1sq [expr {$dirx * $dirx + $diry * $diry}]
    set dir_len [expr {sqrt( $len1sq ) }]
    set dirx [expr {$dirx / $dir_len}]
    set diry [expr {$diry / $dir_len}]

    # calculate distance from mid point to center
    set lensq [expr {$radius * $radius - $len1sq}]
    if { $lensq <= 0. } {
	set tmp_len 0.0
    } else {
	set tmp_len [expr {sqrt( $lensq )}]
    }

    # calculate center as distance from mid point along the bisector direction
    set cx [expr { $midx + $dirx * $tmp_len } ]
    set cy [expr { $midy + $diry * $tmp_len } ]

    # There are two possible centers, make sure we have the right one
    set s2c(0) [expr {$cx - $sx}]
    set s2c(1) [expr {$cy - $sy}]

    if { $center_is_left == 0 } {
	if { [cross2d s2e s2c] < 0.0 } {
	    # wrong center
	    set cx [expr { $midx - $dirx * $tmp_len } ]
	    set cy [expr { $midy - $diry * $tmp_len } ]
	}
    } else {
	if { [cross2d s2e s2c] > 0.0 } {
	    # wrong center
	    set cx [expr { $midx - $dirx * $tmp_len } ]
	    set cy [expr { $midy - $diry * $tmp_len } ]
	}
    }

    return "$cx $cy"
}


class Sketch_editor {
    inherit itk::Toplevel

    private variable sketch_name
    private variable sketch_path
    private variable edit_frame
    private variable V
    private variable A
    private variable B
    private variable VL {}
    private variable SL {}
    private variable myscale 1.0
    private variable segments {}
    private variable tolocal 1.0
    private variable tobase 1.0
    private variable units ""
    private variable vert_radius 3
    private variable x_coord 0.0
    private variable y_coord 0.0
    private variable radius 0.0
    private variable index1 -1
    private variable index2 -1
    private variable needs_saving 0
    private variable move_start_x
    private variable move_start_y
    private variable curr_seg ""
    private variable save_entry
    private variable angle
    private variable bezier_indices ""
    private variable selection_mode ""
    common pi2 [expr {4.0 * asin( 1.0 )}]
    common rad2deg  [expr {360.0 / $pi2}]

    constructor { args } {
	set num_args [llength $args]
	if { $num_args == 0 } {
	    set sketch_name ""
	    set sketch_info {}
	    set SL {}
	    set V(0) 0.0
	    set V(1) 0.0
	    set V(2) 0.0
	    set A(0) 1.0
	    set A(1) 0.0
	    set A(2) 0.0
	    set B(0) 0.0
	    set B(1) 1.0
	    set B(2) 0.0
	    $this configure -title "Sketch Editor: Untitled"
	} elseif { $num_args == 1 } {
	    # get the tcl version of the sketch
	    set sketch_path $args
	    set path [split [string trim $args] "/"]
	    set sketch_name [lindex $path [expr [llength $path] - 1]]

	    # check if we already have an editor working on this sketch
	    verify_unique_editor

	    if { [catch {db get $sketch_name} sketch_info] } {
		tk_messageBox -icon error -title "Cannot Get Sketch" -type ok \
		    -message $sketch_info
		destroy $itk_component(hull)
		return
	    }
	    if { [lindex $sketch_info 0] != "sketch" } {
		tk_messageBox -icon error -type ok -title "$name is not a sketch" \
		    -message "$name is not a sketch"
		destroy $itk_component(hull)
		return
	    }
	} elseif { $num_args == 2 } {
	    set sketch_name [lindex $args 0]

	    # check if we already have an editor working on this sketch
	    verify_unique_editor

	    set sketch_path [lindex $args 1]
	    if { [catch {db get $sketch_name} sketch_info] } {
		tk_messageBox -icon error -title "Cannot Get Sketch" -type ok \
		    -message $sketch_info
		destroy $itk_component(hull)
		return
	    }
	    if { [lindex $sketch_info 0] != "sketch" } {
		tk_messageBox -icon error -type ok -title "$name is not a sketch" \
		    -message "$name is not a sketch"
		destroy $itk_component(hull)
		return
	    }
	} else {
	    tk_messageBox -icon error -type ok -title "Error in constructor args" \
		-message "Wrong number of args to Sketch_editor constructor (should be 1 or none)\n\
					Args were: $args"
	    destroy $itk_component(hull)
	    return
	}
	if { [catch units units] } {
	    set units ""
	}
	if { [string index $units end] == "\n" } {
	    set units [string range $units 0 "end-1"]
	}
	if { [catch {status base2local} tolocal] } {
	    set tolocal 1.0
	    set tobase 1.0
	} else {
	    set tobase [expr {1.0 / $tolocal}]
	}

	if { [llength $sketch_name] > 0 } {
	    $this configure -title "Sketch Editor: $sketch_name   ($units)"
	}
	set sketch_info [lrange $sketch_info 1 end]

	set segments {}
	itk_initialize
	itk_component add canvas {
	    canvas $itk_interior.canv -width 600 -height 600 \
		-scrollregion {0 0 300 300} \
		-xscrollcommand [code $itk_interior.xscr set] \
		-yscrollcommand [code $itk_interior.yscr set]
	}
	itk_component add controls {
	    frame $itk_interior.controls -relief groove -bd 3
	}
	itk_component add notebook {
	    tabnotebook $itk_component(controls).notebook -tabpos w -gap 3 \
		-raiseselect true -bevelamount 3 -borderwidth 3 \
		-foreground \#ff0000
	}
	itk_component add frame {
	    frame $itk_component(controls).frame -relief groove -bd 3
	}

	set create_frame [$itk_component(controls).notebook add -label "Create"]

	button $create_frame.create_line -text "Create Line" -command [code $this create_line]
	button $create_frame.create_circle -text "Create Circle" -command [code $this create_circle]
	button $create_frame.create_arc -text "Create Arc" -command [code $this create_arc]
	button $create_frame.create_bezier -text "Create Bezier" -command [code $this create_bezier]
	grid $create_frame.create_line -row 0 -column 0 -sticky n
	grid $create_frame.create_circle -row 1 -column 0 -sticky n
	grid $create_frame.create_arc -row 3 -column 0 -sticky n
	grid $create_frame.create_bezier -row 4 -column 0 -sticky n

	set debug_frame [$itk_component(controls).notebook add -label "Debug"]

	button $debug_frame.describe -text "Describe All Segments" -command [code $this describe]
	grid $debug_frame.describe -row 0 -column 0 -sticky n

	set save_frame [$itk_component(controls).notebook add -label "Save"]

	label $save_frame.lab -text "Save as:"
	set save_entry [entry $save_frame.ent -textvariable [scope sketch_name] -width 19]
	bind $save_frame.ent <Return> [code $this do_save]
	button $save_frame.ok -text "Save" -command [code $this do_save]
	grid $save_frame.lab -row 0 -column 0 -sticky e
	grid $save_frame.ent -row 0 -column 1 -sticky w
	grid $save_frame.ok -row 1 -column 0 -columnspan 2

	button $itk_component(frame).redraw -text "Redraw" -command [code $this draw_segs]
	button $itk_component(frame).zoomin -text "Zoom In" -command [code $this do_scale 2.0]
	button $itk_component(frame).zoomout -text "Zoom Out" -command [code $this do_scale 0.5]
	button $itk_component(frame).dismiss -text "Dismiss" -command [code $this dismiss]
	button $itk_component(frame).reset -text "Reset Sketch" -command [code $this reset]
	grid $itk_component(frame).redraw -row 0 -column 0 -sticky nw
	grid $itk_component(frame).reset -row 0 -column 1 -sticky ne
	grid $itk_component(frame).zoomin -row 1 -column 0 -sticky w
	grid $itk_component(frame).zoomout -row 1 -column 1 -sticky e
	grid $itk_component(frame).dismiss -row 2 -column 0 -columnspan 2 -sticky s

	set edit_frame [$itk_component(controls).notebook add -label "Edit"]

	radiobutton $edit_frame.pick_seg -text "Select Segments" -command [code $this start_seg_pick] -variable selection_mode -value "segs" -indicatoron false -state normal
	radiobutton $edit_frame.pick_vert -text "Select Vertices" -command [code $this start_vert_pick] -variable selection_mode -value "verts" -indicatoron false -state normal
	button $edit_frame.delete -text "Delete Selected" -command [code $this delete_selection]
	button $edit_frame.move -text "Move Selected" -command [code $this setup_move]
	button $edit_frame.cancel -text "Cancel" -command [code $this do_cancel 1]

	grid $edit_frame.pick_seg -row 0 -column 0 -sticky n
	grid $edit_frame.pick_vert -row 1 -column 0 -sticky n
	grid $edit_frame.delete -row 2 -column 0 -sticky n
	grid $edit_frame.move -row 3 -column 0 -sticky n
	grid $edit_frame.cancel -row 4 -column 0 -sticky n

	set arc_edit_frame [$itk_component(controls).notebook add -label "Edit Arc"]

	button $arc_edit_frame.pick_arc -text "Select Arc" -command [code $this pick_arc]
	button $arc_edit_frame.other_half -text "Use Arc Complement" -command [code $this reverse_curr_seg]
	button $arc_edit_frame.radius -text "Set Radius" -command [code $this start_adjust_radius]
	button $arc_edit_frame.cancel -text "Cancel" -command [code $this do_cancel 1]
	set angle 0.0
	entry $arc_edit_frame.angle -textvariable [scope angle]
	label $arc_edit_frame.angle_label -text "Tangency Angle:"
	button $arc_edit_frame.set_tangency -text "Set Tangency" -command [code $this start_set_tangency]
	label $arc_edit_frame.blank_row -text ""
	grid $arc_edit_frame.pick_arc -row 0 -column 0 -sticky n -columnspan 2
	grid $arc_edit_frame.other_half -row 1 -column 0 -sticky n -columnspan 2
	grid $arc_edit_frame.radius -row 2 -column 0 -sticky n -columnspan 2
	grid $arc_edit_frame.cancel -row 3 -column 0 -sticky n -columnspan 2
	grid $arc_edit_frame.blank_row -row 4 -column 0
	grid $arc_edit_frame.angle_label -row 5 -column 0 -sticky e
	grid $arc_edit_frame.angle -row 5 -column 1 -sticky w
	grid $arc_edit_frame.set_tangency -row 6 -column 0 -sticky n -columnspan 2

	grid $itk_component(notebook) -row 1 -column 0 -sticky nsew

	grid $itk_component(frame) -row 2 -column 0 -sticky nsew -pady 3 -padx 3

	$itk_component(notebook) select "Create"

	itk_component add coords {
	    frame $itk_component(controls).fr_coords
	}
	label $itk_component(coords).x_lab -width 7 -text "X:" -anchor e
	entry $itk_component(coords).x -width 10 -relief sunken -textvariable [scope x_coord]
	label $itk_component(coords).y_lab -text "Y:" -anchor e
	entry $itk_component(coords).y -width 10 -relief sunken -textvariable [scope y_coord]
	label $itk_component(coords).rad_lab -text "Radius:"
	entry $itk_component(coords).radius -width 10 -textvariable [scope radius]
	grid $itk_component(coords).x_lab -row 0 -column 0 -sticky e
	grid $itk_component(coords).x -row 0 -column 1 -sticky w
	grid $itk_component(coords).y_lab -row 0 -column 2 -sticky e
	grid $itk_component(coords).y -row 0 -column 3 -sticky w
	grid $itk_component(coords) -row 0 -column 0 -sticky n
	grid $itk_component(coords).rad_lab -row 1 -column 0 -sticky e
	grid $itk_component(coords).radius -row 1 -column 1 -sticky w

	itk_component add status_line {
	    label $itk_interior.stat -height 4 -text ""
	}
	bind $itk_component(canvas) <Configure> [code $this draw_segs]
	bind $itk_component(canvas) <Motion> [code $this show_coords %x %y]
	itk_component add xscr {
	    scrollbar $itk_interior.xscr -orient horizontal -command [code $itk_component(canvas) xview]
	}
	itk_component add yscr {
	    scrollbar $itk_interior.yscr -orient vertical -command [code $itk_component(canvas) yview]
	}

	grid $itk_component(controls) -row 0 -column 0 -rowspan 2 -sticky nsew -padx 3 -pady 3
	grid rowconfigure $itk_component(controls) 1 -weight 1
	grid $itk_component(canvas) -row 0 -column 1 -sticky nsew
	grid $itk_component(yscr) -row 0 -column 2 -sticky ns
	grid $itk_component(xscr) -row 1 -column 1 -sticky ew
	grid $itk_component(status_line) -row 2 -column 0 -columnspan 2 -sticky ew
	grid rowconfigure $itk_interior 0 -weight 1
	grid columnconfigure $itk_interior 1 -weight 1

	get_sketch_values $sketch_info

	create_segments

	draw_segs
	update idletasks
	set canv_height [winfo height $itk_component(canvas)]
	set canv_width [winfo width $itk_component(canvas)]
	set min_max [$itk_component(canvas) bbox all]
	set tmp_scale1 [expr double($canv_width) / ([lindex $min_max 2] - [lindex $min_max 0] + 2.0 * $vert_radius)]
	if { $tmp_scale1 < 0.0 } {set tmp_scale1 [expr -$tmp_scale1] }
	set tmp_scale2 [expr double($canv_height) / ([lindex $min_max 3] - [lindex $min_max 1] + 2.0 * $vert_radius)]
	if { $tmp_scale2 < 0.0 } {set tmp_scale2 [expr -$tmp_scale2] }
	if { $tmp_scale1 < $tmp_scale2 } {
	    do_scale $tmp_scale1
	} else {
	    do_scale $tmp_scale2
	}

    }

    method get_tobase {} {
	return $tobase
    }

    method verify_unique_editor {} {
	set editors [find objects -class Sketch_editor]
	foreach editor $editors {
	    if { [string compare $this $editor] == 0 } {
		continue
	    }

	    if { [string compare [$editor get_sketch_name] $sketch_name] == 0 } {
		# another editor for this sketch already exists, destroy me
		$editor raise_me
		destroy $itk_component(hull)
		return
	    }
	}
    }

    method get_sketch_name {} {
	return $sketch_name
    }

    method raise_me {} {
	wm deiconify $itk_component(hull)
	raise $itk_component(hull)
    }

    method do_set_tangency { sx sy } {
	if { $curr_seg == "" } {
	    tk_messageBox -icon error -type ok -title "No Arc Selected" \
		-message "Please select an arc to edit"
	    return
	}

	if { [$curr_seg is_full_circle] } {
	    tk_messageBox -icon error -type ok -title "Full CIrcle Selected" \
		-message "Please select an arc (Not a full circle)"
	    return
	}
	set item [pick_segment $sx $sy]
	if { $item == -1 } return
	if { $item == $curr_seg } return

	set verts0 [$curr_seg get_verts]
	set verts1 [$item get_verts]

	set common_vertex -1
	foreach vertex $verts0 {
	    set common [lsearch -exact $verts1 $vertex]
	    if { $common != -1 } {
		set common_vertex $vertex
		break
	    }
	}
	if { $common_vertex == -1 } {
	    tk_messageBox -icon error -type ok -title "Selected segment is not adjacent to arc being edited"
	    $item unhighlight
	    return
	}

	set dir_list [$item get_tangent_at_vertex $common_vertex]
	set dir(0) [lindex $dir_list 0]
	set dir(1) [lindex $dir_list 1]

	if { $angle != 0.0 } {
	    # rotate to desired angle
	    set cosa [expr { cos( $angle / $rad2deg ) } ]
	    set sina [expr { sin ( $angle / $rad2deg ) } ]
	    set tmp1(0) [expr { $dir(0) * $cosa - $dir(1) * $sina } ]
	    set tmp1(1) [expr { $dir(0) * $sina + $dir(1) * $cosa } ]
	    set dir(0) $tmp1(0)
	    set dir(1) $tmp1(1)
	    set dir_list "$dir(0) $dir(1)"
	}

	# use normal to this direction (a radius)
	set tmp $dir(0)
	set dir(0) [expr { -$dir(1) } ]
	set dir(1) $tmp

	set v1 [lindex $VL [lindex $verts0 0]]

	set v2 [lindex $VL [lindex $verts0 1]]

	set diff(0) [expr {[lindex $v1 0] - [lindex $v2 0] } ]
	set diff(1) [expr {[lindex $v1 1] - [lindex $v2 1] } ]
	set dotval [dot diff dir]
	if { [expr { abs( $dotval ) } ] < 1.0e-10 } {
	    tk_messageBox -type ok -icon error -title "Impossible Tangency" \
		-message "Cannot create such an arc (you are asking for a straight line)"
	    return

	}
	set new_radius [expr { [dot diff diff] / (-2.0 * $dotval) } ]
	if { $new_radius < 0.0 } {
	    set new_radius [expr { -$new_radius } ]
	    set dir(0) [expr { -$dir(0) } ]
	    set dir(1) [expr { -$dir(1) } ]
	}
	set vcom [lindex $VL $common_vertex]

	# calculate arc centers in both directions
	set centera(0) [expr { [lindex $vcom 0] + $new_radius * $dir(0) } ]
	set centera(1) [expr { [lindex $vcom 1] + $new_radius * $dir(1) } ]
	set centerb(0) [expr { [lindex $vcom 0] - $new_radius * $dir(0) } ]
	set centerb(1) [expr { [lindex $vcom 1] - $new_radius * $dir(1) } ]

	# choose the one that is the correct distance from the other vertex
	if { $vcom == $v1 } {
	    set vother $v2
	} else {
	    set vother $v1
	}
	set diff2(0) [expr { $centera(0) - [lindex $vother 0] } ]
	set diff2(1) [expr { $centera(1) - [lindex $vother 1] } ]
	set dista [expr { abs( sqrt( $diff2(0) * $diff2(0) + $diff2(1) * $diff2(1)) - $new_radius)}]

	set diff2(0) [expr { $centerb(0) - [lindex $vother 0] } ]
	set diff2(1) [expr { $centerb(1) - [lindex $vother 1] } ]
	set distb [expr { abs( sqrt( $diff2(0) * $diff2(0) + $diff2(1) * $diff2(1)) - $new_radius)}]

	if { $dista < $distb } {
	    set center(0) $centera(0)
	    set center(1) $centera(1)
	} else {
	    set center(0) $centerb(0)
	    set center(1) $centerb(1)
	}

	set diff2(0) [expr { $center(0) - [lindex $v2 0] } ]
	set diff2(1) [expr { $center(1) - [lindex $v2 1] } ]
	set cross [cross2d diff2 diff]
	if { $cross > 0.0 } {
	    set center_is_left 1
	} else {
	    set center_is_left 0
	}
	set diff1(0) [expr { [lindex $v1 0] - $center(0) } ]
	set diff1(1) [expr { [lindex $v1 1] - $center(1) } ]
	set tmp1(0) [expr { [lindex $v1 0] + [lindex $dir_list 0] } ]
	set tmp1(1) [expr { [lindex $v1 1] + [lindex $dir_list 1] } ]
	set diff2(0) [expr { $tmp1(0) - $center(0) } ]
	set diff2(1) [expr { $tmp1(1) - $center(1) } ]
	set cross [cross2d diff1 diff2]
	if { $cross > 0.0 } {
	    set orientation 0
	} else {
	    set orientation 1
	}
	$curr_seg set_vars R $new_radius L $center_is_left O $orientation
	redraw_segs
	set needs_saving 1
	do_cancel 1
    }

    method start_set_tangency {} {
	if { $curr_seg == "" } {
	    tk_messageBox -icon error -type ok -title "No Arc Selected" \
		-message "Please select an arc to edit"
	    do_cancel 1
	    return
	}
	if { [$itk_component(canvas) type $curr_seg] != "arc" } {
	    tk_messageBox -icon error -type ok -title "No Arc Selected" \
		-message "Please select an arc to edit"
	    do_cancel 1
	    return
	}
	$itk_component(status_line) configure -text "After setting the desired angle, Use mouse button 1\n\
			to select a neighboring segment for tangency"
	bind $itk_component(canvas) <ButtonPress-1> [code $this do_set_tangency %x %y]
    }

    method start_adjust_radius {} {
	if { $curr_seg == "" } {
	    tk_messageBox -icon error -type ok -title "No Arc Selected" \
		-message "Please select an arc to edit"
	    do_cancel 1
	    return
	}
	if { [$itk_component(canvas) type $curr_seg] != "arc" } {
	    tk_messageBox -icon error -type ok -title "No Arc Selected" \
		-message "Please select an arc to edit"
	    do_cancel 1
	    return
	}
	set indices [$curr_seg get_verts]
	set index1 [lindex $indices 0]
	set index2 [lindex $indices 1]
	bind $itk_component(canvas) <ButtonPress-1> [code $this set_arc_radius_start $curr_seg %x %y]
	bind $itk_component(canvas) <B1-Motion> [code $this set_arc_radius_start $curr_seg %x %y]
	bind $itk_component(canvas) <ButtonRelease-1> [code $this set_arc_radius_end $curr_seg 1 %x %y]
	bind $itk_component(coords).radius <Return> [code $this set_arc_radius_end $curr_seg 0 0 0]
	$itk_component(status_line) configure -text "Use mouse button 1 to drag any point on the arc\n\
			or enter desired radius in radius entry box"
    }

    method reverse_curr_seg {} {
	if { $curr_seg == "" } {
	    tk_messageBox -icon error -type ok -title "No Arc Selected" \
		-message "Please select an arc to edit"
	    do_cancel 1
	    return
	}
	if { [$itk_component(canvas) type $curr_seg] != "arc" } {
	    tk_messageBox -icon error -type ok -title "No Arc Selected" \
		-message "Please select an arc to edit"
	    do_cancel 1
	    return
	}
	$curr_seg reverse_orientation
	set needs_saving 1
	redraw_segs
	set curr_seg ""
	do_cancel 1
    }

    method get_scale {} {
	return $myscale
    }

    method get_vlist {} {
	return $VL
    }

    method arc_pick_highlight { x y } {
	set item [pick_segment $x $y]
	if { $item == -1 } return
	if { [$itk_component(canvas) type $item] != "arc" } return
	$item highlight
	set curr_seg $item
    }

    method pick_arc {} {
	set curr_seg ""

	# check if an arc has already been selected
	set ids [$itk_component(canvas) find withtag selected]
	set arc_count 0
	set arc ""
	foreach id $ids {
	    set canv_type [$itk_component(canvas) type $id]
	    if { $canv_type == "arc" } {
		incr arc_count
		set arc $id
	    }
	}

	if { $arc_count == 1 } {
	    set tags [$itk_component(canvas) gettags $arc]
	    set curr_seg [lindex $tags 0]
	} else {
	    $itk_component(status_line) configure -text "Click on the arc to edit using mouse button 1"
	    unhighlight_selected
	    bind $itk_component(canvas) <ButtonRelease-1> [code $this arc_pick_highlight %x %y]
	}
    }

    method do_cancel { cancel_highlight } {
	if { $cancel_highlight } {
	    unhighlight_selected
	}
	set curr_seg ""
	set index1 -1
	set index2 -1
	set selection_mode ""
	$edit_frame.pick_seg deselect
	$edit_frame.pick_vert deselect
	bind $itk_component(canvas) <ButtonPress-1> {}
	bind $itk_component(canvas) <ButtonRelease-1> {}
	bind $itk_component(canvas) <ButtonRelease-3> {}
	bind $itk_component(canvas) <B1-Motion> {}
	bind $itk_component(coords).x <Return> {}
	bind $itk_component(coords).y <Return> {}
	bind $itk_component(coords).radius <Return> {}
	$itk_component(status_line) configure -text ""
    }

    method get_sketch_values { sketch_info } {
	foreach {key value} $sketch_info {
	    switch $key {
		V {
		    for { set index 0 } { $index < 3 } { incr index } {
			set V($index) [expr {$tolocal * [lindex $value $index]}]
		    }
		}
		A {
		    for { set index 0 } { $index < 3 } { incr index } {
			set A($index) [expr {$tolocal * [lindex $value $index]}]
		    }
		}
		B {
		    for { set index 0 } { $index < 3 } { incr index } {
			set B($index) [expr {$tolocal * [lindex $value $index]}]
		    }
		}
		VL {
		    foreach vert $value {
			set x [expr {$tolocal * [lindex $vert 0]}]
			set y [expr {$tolocal * [lindex $vert 1]}]
			lappend VL "$x $y"
		    }
		}
		SL {
		    set SL $value
		}
	    }
	}
    }

    method create_segments {} {
	foreach seg $SL {
	    set type [lindex $seg 0]
	    set seg [lrange $seg 1 end]
	    switch $type {
		line {
		    lappend segments ::Sketch_editor::[Sketch_line \#auto $this $itk_component(canvas) $seg]
		}
		carc {
		    set index [lsearch -exact $seg R]
		    incr index
		    set tmp_radius [lindex $seg $index]
		    if { $tmp_radius > 0.0 } {
			set tmp_radius [expr {$tolocal * $tmp_radius}]
			set seg [lreplace $seg $index $index $tmp_radius]
		    }
		    lappend segments ::Sketch_editor::[Sketch_carc \#auto $this $itk_component(canvas) $seg]
		}
		bezier {
		    lappend segments ::Sketch_editor::[Sketch_bezier \#auto $this $itk_component(canvas) $seg]
		}
		default {
		    tk_messageBox -type ok -icon warning -title "Unrecognized segment type" \
			-message "Curve segments of type '$type' are not yet handled"
		}
	    }
	}
    }

    method dismiss {} {
	if { $needs_saving } {
	    set answer [tk_messageBox -type yesno -icon warning -title "Changes Not Saved" \
			    -message "Quit without saving changes????"]
	    if { $answer == "no" } {
		return
	    }
	}
	delete object $this
    }

    method reset {} {
	if { [catch {db get $sketch_name} sketch_info] } {
	    tk_messageBox -icon error -title "Cannot Get Sketch" -type ok \
		-message $sketch_info
	    destroy $itk_component(hull)
	    return
	}
	if { [lindex $sketch_info 0] != "sketch" } {
	    tk_messageBox -icon error -type ok -title "$name is not a sketch" -message "$name is not a sketch"
	    destroy $itk_component(hull)
	    return
	}
	set sketch_info [lrange $sketch_info 1 end]
	set segments {}
	set VL {}
	set SL {}
	set needs_saving 0
	$itk_component(canvas) delete all

	get_sketch_values $sketch_info

	create_segments

	draw_segs
    }

    # this method checks if an object name already exists in the database
    # if so, return 1, otherwise return 0
    method check_name { obj_name } {
	catch "db match $obj_name" ret
	set ret [string trimright $ret "\n"]
	if { $ret != $obj_name } {
	    return 0
	} else {
	    return 1
	}
    }

    method write_sketch_to_db { name update } {
	set out "V { [expr {$tobase * $V(0)}] [expr {$tobase * $V(1)}] [expr {$tobase * $V(2)}] }"
	set out "$out A { [expr {$tobase * $A(0)}] [expr {$tobase * $A(1)}] [expr {$tobase * $A(2)}] }"
	set out "$out B { [expr {$tobase * $B(0)}] [expr {$tobase * $B(1)}] [expr {$tobase * $B(2)}] } VL {"
	foreach vert $VL {
	    set out "$out { [expr {$tobase * [lindex $vert 0]}] [expr {$tobase * [lindex $vert 1]}] }"
	}
	set out "$out } SL {"
	foreach seg $segments {
	    set out "$out [$seg serialize $tobase] "
	}
	set out "$out }"

	if { $update } {
	    set tmp [db get $name]
	    set type [lindex $tmp 0]
	    if { $type != "sketch" || $name != $sketch_name } {
		set answer [tk_messageBox -type yesno -icon question -title "Overwrite Existing Object???" \
				-message "$name already exists, overwrite??"]
		if { $answer == "no" } {
		    return
		}
		catch "db kill $name"
		set command "put $name sketch"
	    } else {
		set command "adjust $name"
	    }
	} else {
	    set command "put $name sketch"
	}

	if { [catch "db $command $out" ret] } {
	    tk_messageBox -type ok -title "ERROR Saving Sketch!!!!" -message "$ret"
	} else {
	    set sketch_name $name
	    $this configure -title "Sketch Editor: $sketch_name   ($units)"
	    set needs_saving 0
	}
    }

    method do_save {} {
	set obj_name [$save_entry get]
	write_sketch_to_db $obj_name [check_name $obj_name]
	draw $sketch_path
    }

    method do_scale { scale_factor } {
	set myscale [expr {$myscale * $scale_factor}]
	$itk_component(canvas) scale all 0 0 $scale_factor $scale_factor
	draw_segs
	$itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
    }

    method describe {} {
	puts "Sketch: $sketch_name"
	puts "Scale Factor = $myscale"
	puts "conversion factors: to local = $tolocal to base = $tobase"
	puts "V = ($V(0), $V(1), $V(2)), A = ($A(0), $A(1), $A(2)), B = ($B(0), $B(1), $B(2))"
	puts "Vertex List:"
	set index 0
	foreach vertex $VL {
	    puts "\tvertex #$index = ($vertex)"
	    incr index
	}
	foreach seg $segments {
	    $seg describe
	    set tags [$itk_component(canvas) gettags $seg]
	    puts "	$seg tags: $tags"
	}
    }

    method show_coords { x y } {
	# x and y are screen coords, convert to model coords
	set x_coord [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	set y_coord [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
    }

    method set_radius { r } {
	set radius $r
    }

    method draw_verts {} {
	set index 0
	$itk_component(canvas) delete verts
	foreach vert $VL {
	    set xc [lindex $vert 0]
	    set yc [lindex $vert 1]
	    set x1 [expr {$myscale * $xc - $vert_radius}]
	    set y1 [expr {-$myscale * $yc - $vert_radius}]
	    set x2 [expr {$myscale * $xc + $vert_radius}]
	    set y2 [expr {-$myscale * $yc + $vert_radius}]
	    set last [$itk_component(canvas) create oval $x1 $y1 $x2 $y2 -fill black -tags "p$index verts"]
	    incr index
	}
    }

    method redraw_segs {} {
	set ids [$itk_component(canvas) find withtag segs]
	foreach id $ids {
	    set tags [$itk_component(canvas) gettags $id]
	    set seg [lindex $tags 0]
	    $itk_component(canvas) delete $id
	    $seg draw [lrange $tags 2 end]
	}
    }

    method draw_segs {} {
	$itk_component(canvas) delete  all
	draw_verts
	set first 1
	foreach seg $segments {
	    if { $first } {
		set first 0
		$seg draw first_seg
	    } else {
		$seg draw ""
	    }
	}
	$itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
    }

    method pick_vertex { sx sy } {
	set x [$itk_component(canvas) canvasx $sx]
	set y [$itk_component(canvas) canvasy $sy]
	set item [$itk_component(canvas) find closest $x $y 0 first_seg]
	if { $item == "" } {
	    return -1
	}

	set tags [$itk_component(canvas) gettags $item]
	set index [lsearch -glob $tags p*]
	if { $index == -1 } {
	    return -1
	}
	return [string range [lindex $tags $index] 1 end]
    }

    method pick_segment { sx sy } {
	set x [$itk_component(canvas) canvasx $sx]
	set y [$itk_component(canvas) canvasy $sy]
	set item [$itk_component(canvas) find closest $x $y]
	if { $item == "" } {
	    return -1
	}

	set tags [$itk_component(canvas) gettags $item]
	set index [lsearch -exact $tags segs]
	if { $index == -1 } {
	    return -1
	}
	return [lindex $tags 0]
    }

    method seg_pick_highlight { x y } {
	set item [pick_segment $x $y]
	if { $item == -1 } return
	$item highlight
	set curr_seg $item
    }

    method unhighlight_selected {} {
	set curr_selection [$itk_component(canvas) find withtag selected]
	foreach id $curr_selection {
	    set tags [$itk_component(canvas) gettags $id]
	    set item [lindex $tags 0]
	    set type [lindex $tags 1]
	    switch $type {
		segs   {
		    $item unhighlight
		}
		verts  {
		    $itk_component(canvas) itemconfigure $id -outline black -fill black
		}
	    }
	}
	$itk_component(canvas) dtag selected selected
	$itk_component(canvas) dtag first_select first_select
    }

    method start_seg_pick {} {
	unhighlight_selected
	bind $itk_component(canvas) <ButtonRelease-1> [code $this seg_pick_highlight %x %y]
    }

    method vert_pick_highlight { x y } {
	set item [pick_vertex $x $y]
	if { $item == -1 } return
	set selection [$itk_component(canvas) find withtag first_select]
	if { $selection == {} } {
	    $itk_component(canvas) addtag first_select withtag p$item
	    $itk_component(canvas) itemconfigure p$item -fill yellow -outline yellow
	} else {
	    $itk_component(canvas) itemconfigure p$item -fill red -outline red
	}
	$itk_component(canvas) addtag selected withtag p$item
    }

    method start_vert_pick {} {
	unhighlight_selected
	bind $itk_component(canvas) <ButtonRelease-1> [code $this vert_pick_highlight %x %y]
    }

    method vert_is_used { index } {
	foreach seg $segments {
	    if [$seg is_vertex_used $index] {return 1}
	}
	return 0
    }

    method fix_vertex_references { deleted_verts } {
	foreach seg $segments {
	    $seg fix_vertex_reference $deleted_verts
	}
	draw_verts
    }

    method delete_selection {} {
	set deleted_verts {}
	set ids [$itk_component(canvas) find withtag selected]
	foreach id $ids {
	    set tags [$itk_component(canvas) gettags $id]
	    set type [lindex $tags 1]
	    set item [lindex $tags 0]
	    switch $type {
		segs {
		    set index [lsearch $segments $item]
		    set segments [lreplace $segments $index $index]
		    destroy $item
		    set needs_saving 1
		}
		verts {
		    set index [string range $item 1 end]
		    if [vert_is_used $index] {
			tk_messageBox -icon error -type ok -title \
			    "Vertex Is In Use" -message \
			    "Cannot delete a vertex being used by a segment"
			$itk_component(canvas) dtag $id selected
			$itk_component(canvas) itemconfigure $id -outline black -fill black
		    } else {
			lappend deleted_verts $index
			set needs_saving 1
		    }
		}
	    }
	}
	set deleted_verts [lsort -integer -decreasing $deleted_verts]
	foreach vert $deleted_verts {
	    set VL [lreplace $VL $vert $vert]
	}
	fix_vertex_references $deleted_verts
	$itk_component(canvas) delete selected
    }

    # move vertices tagged as "moving"
    # then redraw the segments
    # state == 0 => moving by mouse drag
    # state == 1 => end of mouse movement (button release)
    # state == 2 => moving by input from entry widget
    method continue_move {state sx sy } {
	set needs_saving 1
	if { $state != 2 } {
	    $this show_coords $sx $sy
	    set x [$itk_component(canvas) canvasx $sx]
	    set y [$itk_component(canvas) canvasy $sy]
	    set dx [expr $x - $move_start_x]
	    set dy [expr $y - $move_start_y]
	} else {
	    set id [$itk_component(canvas) find withtag first_select]
	    set tags [$itk_component(canvas) gettags $id]
	    set index [string range [lindex $tags 0] 1 end]
	    set old_coords [$itk_component(canvas) coords $id]
	    set dx [expr { $x_coord * $myscale - ([lindex $old_coords 0] + [lindex $old_coords 2])/2.0}]
	    set dy [expr { -$y_coord * $myscale - ([lindex $old_coords 1] + [lindex $old_coords 3])/2.0}]
	}
	$itk_component(canvas) move moving $dx $dy

	# actually move the vertices in the vertex list
	set ids [$itk_component(canvas) find withtag moving]
	foreach id $ids {
	    set tags [$itk_component(canvas) gettags $id]
	    set index [string range [lindex $tags 0] 1 end]
	    set new_coords [$itk_component(canvas) coords $id]
	    set new_x [expr ([lindex $new_coords 0] + [lindex $new_coords 2])/(2.0 * $myscale)]
	    set new_y [expr -([lindex $new_coords 1] + [lindex $new_coords 3])/(2.0 * $myscale)]
	    set VL [lreplace $VL $index $index [concat $new_x $new_y]]
	}
	redraw_segs
	if { $state == 0 } {
	    set move_start_x $x
	    set move_start_y $y
	} else {
	    bind $itk_component(canvas) <ButtonPress-1> {}
	    bind $itk_component(canvas) <B1-Motion> {}
	    bind $itk_component(canvas) <ButtonRelease-1> {}
	    bind $itk_component(coords).x <Return> {}
	    bind $itk_component(coords).y <Return> {}
	    $itk_component(status_line) configure -text ""
	    $itk_component(canvas) dtag moving moving
	    unhighlight_selected
	}

    }

    method start_move { sx sy } {
	set move_start_x [$itk_component(canvas) canvasx $sx]
	set move_start_y [$itk_component(canvas) canvasy $sy]
	bind $itk_component(canvas) <B1-Motion> [code $this continue_move 0 %x %y]
	bind $itk_component(canvas) <ButtonRelease-1> [code $this continue_move 1 %x %y]
	set needs_saving 1
    }

    method setup_move {} {
	$itk_component(canvas) addtag moving withtag selected
	set ids [$itk_component(canvas) find withtag selected]
	foreach id $ids {
	    set tags [$itk_component(canvas) gettags $id]
	    set type [lindex $tags 1]
	    if { $type == "segs" } {
		$itk_component(canvas) dtag $id moving
		set item [lindex $tags 0]
		$item tag_verts moving
	    }
	}
	bind $itk_component(canvas) <ButtonPress-1> [code $this start_move %x %y]
	bind $itk_component(coords).x <Return> [code $this continue_move 2 0 0]
	bind $itk_component(coords).y <Return> [code $this continue_move 2 0 0]
	$itk_component(status_line) configure -text "Click and drag using mouse button 1\n\
				Or enter new coordinates for the yellow vertex in the coordinate entry windows"
    }

    method circle_3pt { x1 y1 x2 y2 x3 y3 cx_out cy_out } {
	# find the center of a circle that passes through three points
	# return the center in "cx_out cy_out"
	# returns 0 on success
	# returns 1 if no such circle exists

	upvar $cx_out cx
	upvar $cy_out cy

	# first find the midpoints of two lines connecting the three points
	set mid1(0) [expr { ($x1 + $x2)/2.0 }]
	set mid1(1) [expr { ($y1 + $y2)/2.0 }]
	set mid2(0) [expr { ($x3 + $x2)/2.0 }]
	set mid2(1) [expr { ($y3 + $y2)/2.0 }]

	# next find the slopes of the perpendicular bisectors
	set dir1(0) [expr { $y1 - $y2 }]
	set dir1(1) [expr { $x2 - $x1 }]
	set dir2(0) [expr { $y2 - $y3 }]
	set dir2(1) [expr { $x3 - $x2 }]

	# find difference between the bisector start points
	set diffs(0) 0.0
	set diffs(1) 0.0
	diff diffs mid2 mid1

	# center point is the intersection of the two perpendicular bisectors
	set cross1 [cross2d dir2 dir1]
	set cross2 [cross2d dir1 diffs]

	# if cross1 is to small, this will catch the error
	if { [catch { expr {$cross2 / $cross1} } beta] } {
	    # return an error flag
	    return 1
	}

	set cx [expr { $mid2(0) + $beta * $dir2(0)}]
	set cy [expr { $mid2(1) + $beta * $dir2(1)}]

	return 0
    }

    method create_bezier {} {
	set bezier_indices ""
	$itk_component(status_line) configure -text "Click mouse button 1 to set Bezier start point\n\
		    or click mouse button 3 to select an existing vertex as the start point\n\
		    or enter start point coordinates in entry windows"
	bind $itk_component(canvas) <ButtonRelease-1> [code $this start_bezier 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-3> [code $this start_bezier_pick %x %y]
	bind $itk_component(coords).x <Return> [code $this start_bezier 0 0 0]
	bind $itk_component(coords).y <Return> [code $this start_bezier 0 0 0]
    }

    method start_bezier { coord_type x y } {
	if { $coord_type == 1 } {
	    # screen coordinates
	    show_coords $x $y
	    set sx [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set sy [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	} elseif { $coord_type == 0 } {
	    # model coordinates
	    set sx $x_coord
	    set sy $y_coord
	}
	if { $coord_type != 2 } {
	    # use index numbers
	    set bezier_indices [llength $VL]
	    lappend VL "$sx $sy"
	    draw_verts
	}

	lappend bezier_indices $bezier_indices
	set new_seg [Sketch_bezier \#auto $this $itk_component(canvas) \
			 "D [expr [llength $bezier_indices] - 1] P [list $bezier_indices]"]
	lappend segments ::Sketch_editor::$new_seg
	set needs_saving 1
	draw_segs

	# setup to pick next bezier point
	$itk_component(status_line) configure -text "Click mouse button 1 to set next Bezier point\n\
		    or click mouse button 3 to select an existing vertex as the next point\n\
		    or enter next point coordinates in entry windows\n\
		    or click mouse button 2 to finish"
	bind $itk_component(canvas) <ButtonRelease-1> [code $this next_bezier $new_seg 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-3> [code $this next_bezier_pick $new_seg %x %y]
	bind $itk_component(canvas) <ButtonRelease-2> [code $this end_bezier $new_seg]
	bind $itk_component(coords).x <Return> [code $this next_bezier $new_seg 0 0 0]
	bind $itk_component(coords).y <Return> [code $this next_bezier $new_seg 0 0 0]
    }

    method start_bezier_pick { x y } {
	set index [pick_vertex $x $y]
	if { $index == -1 } return
	set bezier_indices $index
	start_bezier 2 0 0
    }

    method next_bezier { segment coord_type x y} {
	if { $coord_type == 1 } {
	    # screen coordinates
	    show_coords $x $y
	    set sx [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set sy [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	} elseif { $coord_type == 0 } {
	    # model coordinates
	    set sx $x_coord
	    set sy $y_coord
	}
	if { $coord_type != 2 } {
	    # use index numbers
	    if { [llength $bezier_indices] == 2 && [lindex $bezier_indices 0] == [lindex $bezier_indices 1] } {
		set bezier_indices [lrange $bezier_indices 0 0]
	    }
	    lappend bezier_indices [llength $VL]
	    lappend VL "$sx $sy"
	    draw_verts
	}
	$itk_component(canvas) delete ::Sketch_editor::$segment
	::Sketch_editor::$segment set_vars D [expr [llength $bezier_indices] - 1] P $bezier_indices
	draw_segs
    }

    method next_bezier_pick { segment x y } {
	set index [pick_vertex $x $y]
	if { $index == -1 } return
	if { [llength $bezier_indices] == 2 && [lindex $bezier_indices 0] == [lindex $bezier_indices 1] } {
	    set bezier_indices [lrange $bezier_indices 0 0]
	}
	lappend bezier_indices $index
	next_bezier $segment 2 0 0
    }

    method end_bezier { segment } {
	bind $itk_component(canvas) <ButtonRelease-1> {}
	bind $itk_component(canvas) <ButtonRelease-3> {}
	bind $itk_component(canvas) <ButtonRelease-2> {}
	bind $itk_component(coords).x <Return> {}
	bind $itk_component(coords).y <Return> {}

	if { [llength $bezier_indices] < 2 } {
	    tk_messageBox -icon error -type ok -title "Not enough points" \
		-message "A Bezier curve must have at least two points"
	    set bezier_indices ""
	    return
	}

	$itk_component(canvas) delete ::Sketch_editor::$segment
	::Sketch_editor::$segment set_vars D [expr [llength $bezier_indices] - 1] P $bezier_indices
	set needs_saving 1
	draw_segs
    }

    method create_arc {} {
	# setup to pick arc start point
	set index1 -1
	set index2 -1
	$itk_component(status_line) configure -text "Click mouse button 1 to set arc start point\n\
			or click mouse button 3 to select an existing vertex as the start point\n\
			or enter start point coordinates in entry windows"
	bind $itk_component(canvas) <ButtonRelease-1> [code $this start_arc 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-3> [code $this start_arc_pick %x %y]
	bind $itk_component(coords).x <Return> [code $this start_arc 0 0 0]
	bind $itk_component(coords).y <Return> [code $this start_arc 0 0 0]
    }

    method start_arc_pick { x y } {
	set index [pick_vertex $x $y]
	if { $index == -1 } return
	set index1 $index
	start_arc 2 0 0
    }

    method end_arc_pick { x y } {
	set index [pick_vertex $x $y]
	if { $index == -1 } return
	set index2 $index
	end_arc 2 0 0
    }

    method end_arc { coord_type x y } {
	if { $coord_type == 1 } {
	    # screen coordinates
	    show_coords $x $y
	    set ex [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set ey [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	} elseif { $coord_type == 0 } {
	    # model coordinates
	    set ex $x_coord
	    set ey $y_coord
	}
	if { $coord_type != 2 } {
	    # use index numbers
	    set index2 [llength $VL]
	    lappend VL "$ex $ey"
	    draw_verts
	}

	# calculate an initial radius
	set s [lindex $VL $index1]
	set sx [lindex $s 0]
	set sy [lindex $s 1]
	set e [lindex $VL $index2]
	set ex [lindex $e 0]
	set ey [lindex $e 1]
	set radius [dist $sx $sy $ex $ey]
	set new_seg [Sketch_carc \#auto $this $itk_component(canvas) "S $index1 E $index2 R $radius L 0 O 0"]
	lappend segments ::Sketch_editor::$new_seg
	set needs_saving 1
	draw_segs

	# setup to set radius
	bind $itk_component(canvas) <ButtonRelease-1> {}
	bind $itk_component(canvas) <ButtonRelease-3> {}
	bind $itk_component(coords).x <Return> {}
	bind $itk_component(coords).y <Return> {}

	$itk_component(status_line) configure -text "Use mouse button 1 to drag any point on the arc\n\
			or enter desired radius in radius entry box"

	bind $itk_component(canvas) <ButtonPress-1> [code $this set_arc_radius_start $new_seg %x %y]
	bind $itk_component(canvas) <B1-Motion> [code $this set_arc_radius_start $new_seg %x %y]
	bind $itk_component(canvas) <ButtonRelease-1> [code $this set_arc_radius_end $new_seg 1 %x %y]
	bind $itk_component(coords).radius <Return> [code $this set_arc_radius_end $new_seg 0 0 0]
    }

    method set_arc_radius_start { segment x y } {
	show_coords $x $y
	set sx [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	set sy [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	set cx 0.0
	set cy 0.0
	set s_list [lindex $VL $index1]
	set s(0) [lindex $s_list 0]
	set s(1) [lindex $s_list 1]
	set e_list [lindex $VL $index2]
	set e(0) [lindex $e_list 0]
	set e(1) [lindex $e_list 1]
	if { [circle_3pt $s(0) $s(1) $sx $sy $e(0) $e(1) cx cy] } {
	    return
	}

	set center(0) $cx
	set center(1) $cy
	diff diff1 e s
	diff diff2 center s

	if { [cross2d diff1 diff2] > 0.0 } {
	    set center_is_left 1
	} else {
	    set center_is_left 0
	}

	set start_ang [expr { atan2( ($s(1) - $cy), ($s(0) - $cx) ) }]
	set mid_ang [expr { atan2( ($sy - $cy), ($sx - $cx) ) }]
	while { $mid_ang < $start_ang } {
	    set mid_ang [expr $mid_ang + $pi2]
	}
	set end_ang [expr { atan2( ($e(1) - $cy), ($e(0) - $cx) ) }]
	while { $end_ang < $mid_ang } {
	    set end_ang [expr $end_ang + $pi2]
	}

	if { [expr {$end_ang - $start_ang}] > $pi2 } {
	    set orient 1
	} else {
	    set orient 0
	}

	set radius [dist $s(0) $s(1) $cx $cy]
	$segment set_vars R $radius L $center_is_left O $orient
	redraw_segs
    }

    method set_arc_radius_end { segment coord_type x y } {
	if { $coord_type == 1 } {
	    # screen coordinates
	    show_coords $x $y
	    set sx [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set sy [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	    set cx 0.0
	    set cy 0.0

	    set s_list [lindex $VL $index1]
	    set s(0) [lindex $s_list 0]
	    set s(1) [lindex $s_list 1]
	    set e_list [lindex $VL $index2]
	    set e(0) [lindex $e_list 0]
	    set e(1) [lindex $e_list 1]
	    if { [circle_3pt $s(0) $s(1) $sx $sy $e(0) $e(1) cx cy] } {
		return
	    }

	    set center(0) $cx
	    set center(1) $cy
	    diff diff1 e s
	    diff diff2 center s
	    if { [cross2d diff1 diff2] > 0.0 } {
		set center_is_left 1
	    } else {
		set center_is_left 0
	    }

	    set start_ang [expr { atan2( ($s(1) - $cy), ($s(0) - $cx) ) }]
	    set mid_ang [expr { atan2( ($sy - $cy), ($sx - $cx) ) }]
	    while { $mid_ang < $start_ang } {
		set mid_ang [expr $mid_ang + $pi2]
	    }
	    set end_ang [expr { atan2( ($e(1) - $cy), ($e(0) - $cx) ) }]
	    while { $end_ang < $mid_ang } {
		set end_ang [expr $end_ang + $pi2]
	    }

	    if { [expr {$end_ang - $start_ang}] > $pi2 } {
		set orient 1
	    } else {
		set orient 0
	    }
	    set radius [dist $s(0) $s(1) $cx $cy]
	    $segment set_vars R $radius L $center_is_left O $orient
	} else {
	    set s_list [lindex $VL $index1]
	    set s(0) [lindex $s_list 0]
	    set s(1) [lindex $s_list 1]
	    set e_list [lindex $VL $index2]
	    set e(0) [lindex $e_list 0]
	    set e(1) [lindex $e_list 1]
	    set min_radius [expr {[dist $s(0) $s(1) $e(0) $e(1)] / 2.0}]
	    if { $radius < $min_radius } {
		tk_messageBox -icon error -title "Radius Too Small" -type ok \
		    -message "Radius of $radius is too small, minimum is $min_radius"
		set radius $min_radius
	    }
	    $segment set_vars R $radius
	}
	draw_segs
	do_cancel 1
    }

    method start_arc { coord_type x y } {
	if { $coord_type == 1 } {
	    # screen coordinates
	    show_coords $x $y
	    set sx [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set sy [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	} elseif { $coord_type == 0 } {
	    # model coordinates
	    set sx $x_coord
	    set sy $y_coord
	}
	if { $coord_type != 2 } {
	    # use index numbers
	    set index1 [llength $VL]
	    lappend VL "$sx $sy"
	    draw_verts
	}

	# setup to pick arc end point
	$itk_component(status_line) configure -text "Click mouse button 1 to set arc end point\n\
			or click mouse button 3 to select an existing vertex as the end point\n\
			or enter end point coordinates in entry windows"
	bind $itk_component(canvas) <ButtonRelease-1> [code $this end_arc 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-3> [code $this end_arc_pick %x %y]
	bind $itk_component(coords).x <Return> [code $this end_arc 0 0 0]
	bind $itk_component(coords).y <Return> [code $this end_arc 0 0 0]
    }

    method create_circle {} {
	$itk_component(status_line) configure -text "Click mouse button 1 to set circle center\n\
			or click mouse button 3 to select an existing vertex as the center\n\
			or enter center coordinates in entry windows"
	bind $itk_component(canvas) <ButtonRelease-1> [code $this start_circle 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-3> [code $this start_circle_pick %x %y]
	bind $itk_component(coords).x <Return> [code $this start_circle 0 0 0]
	bind $itk_component(coords).y <Return> [code $this start_circle 0 0 0]
    }

    method start_circle_pick { x y } {
	set index [pick_vertex $x $y]
	if { $index == -1 } return
	set index1 $index
	set index2 $index
	set coords [$itk_component(canvas) coords p$index]
	set x_coord [expr {([lindex $coords 0] + [lindex $coords 2]) / (2.0 * $myscale)}]
	set y_coord [expr {-([lindex $coords 1] + [lindex $coords 3]) / (2.0 * $myscale)}]
	start_circle 2 0 0
    }

    method start_circle { coord_type x y } {
	if { $coord_type == 1 } {
	    # screen coordinates
	    show_coords $x $y
	    set sx [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set sy [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	} elseif { $coord_type == 0 } {
	    # model coordinates
	    set sx $x_coord
	    set sy $y_coord
	}

	set radius 0.0
	if { $coord_type != 2 } {
	    # use index numbers
	    set index1 [llength $VL]
	    lappend VL "$sx $sy"
	    set index2 [llength $VL]
	    lappend VL "$sx $sy"
	}
	set new_seg [Sketch_carc \#auto $this $itk_component(canvas) "S $index1 E $index2 R -1 L 0 O 0"]
	lappend segments ::Sketch_editor::$new_seg
	set needs_saving 1
	draw_segs
	$itk_component(status_line) configure -text "Click and hold mouse button 1 to adjust radius\n\
			or click mouse button 3 to select an existing vertex to set the radius\n\
			or enter the radius or coordinates of a point on the circle in the entry windows"
	bind $itk_component(canvas) <ButtonPress-1> [code $this continue_circle $new_seg 0 1 %x %y]
	bind $itk_component(canvas) <B1-Motion> [code $this continue_circle $new_seg 0 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-1> [code $this continue_circle $new_seg 1 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-3> [code $this continue_circle_pick $new_seg %x %y]
	bind $itk_component(coords).x <Return> [code $this continue_circle $new_seg 1 0 0 0]
	bind $itk_component(coords).y <Return> [code $this continue_circle $new_seg 1 0 0 0]
	bind $itk_component(coords).radius <Return> [code $this continue_circle $new_seg 1 2 0 0]
    }

    method continue_circle_pick { seg x y } {
	set index [pick_vertex $x $y]
	if { $index == -1 } return
	set index1 $index
	set coords [$itk_component(canvas) coords p$index]
	set x_coord [expr {([lindex $coords 0] + [lindex $coords 2]) / (2.0 * $myscale)}]
	set y_coord [expr {-([lindex $coords 1] + [lindex $coords 3]) / (2.0 * $myscale)}]
	continue_circle $seg 1 3 0 0
    }

    method continue_circle { segment state coord_type x y } {
	if { $coord_type == 1 } {
	    # screen coords
	    show_coords $x $y
	    set ex [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set ey [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	} elseif { $coord_type == 2 } {
	    # use radius entry widget
	    set vert [lindex $VL $index1]
	    set ex [expr {[lindex $vert 0] + $radius}]
	    set ey [lindex $vert 1]
	} elseif { $coord_type == 3 } {
	    # use index numbers
	    $segment set_vars S $index1
	} else {
	    # model coords
	    set ex $x_coord
	    set ey $y_coord
	}

	if { $coord_type != 3 } {
	    if { $index1 == $index2 } {
		# need to create a new vertex
		set index1 [llength $VL]
		lappend VL "$ex #ey"
		$segment set_vars S $index1
	    }
	    set VL [lreplace $VL $index1 $index1 "$ex $ey"]
	}
	$itk_component(canvas) delete ::Sketch_editor::$segment
	$segment draw ""
	set radius [$segment get_radius]
	$itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
	if { $state } {
	    bind $itk_component(canvas) <ButtonPress-1> {}
	    bind $itk_component(canvas) <B1-Motion> {}
	    bind $itk_component(canvas) <ButtonRelease-1> {}
	    bind $itk_component(canvas) <ButtonRelease-3> {}
	    bind $itk_component(coords).x <Return> {}
	    bind $itk_component(coords).y <Return> {}
	    $itk_component(status_line) configure -text ""
	    draw_verts
	}
    }

    method create_line {} {
	$itk_component(status_line) configure -text "Click mouse button 1 to set line start point\n\
			or click mouse button 3 to select an existing vertex as start point\n\
			or enter start coordinates in entry windows"
	bind $itk_component(canvas) <ButtonRelease-1> [code $this start_line 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-3> [code $this start_line_pick %x %y]
	bind $itk_component(coords).x <Return> [code $this start_line 0 0 0]
	bind $itk_component(coords).y <Return> [code $this start_line 0 0 0]
    }

    method start_line_pick { x y } {
	set index [pick_vertex $x $y]
	if { $index == -1 } return
	set index1 $index
	set index2 $index
	start_line 2 0 0
    }

    method start_line { coord_type x y } {
	bind $itk_component(canvas) <ButtonPress-1> {}
	if { $coord_type == 1 } {
	    # screen coords
	    show_coords $x $y
	    set sx [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set sy [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	} elseif {$coord_type == 0 } {
	    # model coords
	    set sx $x_coord
	    set sy $y_coord
	}

	if { $coord_type != 2 } {
	    set index1 [llength $VL]
	    lappend VL "$sx $sy"
	    set index2 [llength $VL]
	    lappend VL "$sx $sy"
	}
	set new_seg [Sketch_line \#auto $this $itk_component(canvas) "S $index1 E $index2"]
	lappend segments ::Sketch_editor::$new_seg
	set needs_saving 1
	draw_verts
	draw_segs
	$itk_component(status_line) configure -text "Click and hold mouse button 1 to adjust end point\n\
			or click mouse button 3 to select an existing vertex to set the endpoint\n\
			or enter end point cordinates in the entry windows"
	bind $itk_component(canvas) <B1-Motion> [code $this continue_line $new_seg 0 1 %x %y]
	bind $itk_component(canvas) <ButtonPress-1> [code $this continue_line $new_seg 2 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-1> [code $this continue_line $new_seg 1 1 %x %y]
	bind $itk_component(canvas) <ButtonRelease-3> [code $this continue_line_pick $new_seg %x %y]
	bind $itk_component(coords).x <Return> [code $this continue_line $new_seg 2 0 0 0]
	bind $itk_component(coords).y <Return> [code $this continue_line $new_seg 2 0 0 0]
    }

    method continue_line_pick { seg x y } {
	set index [pick_vertex $x $y]
	if { $index == -1 } return
	set index2 $index
	continue_line $seg 1 2 0 0
    }

    method continue_line { segment  state coord_type x y } {
	if { $coord_type == 1 } {
	    # screen coords
	    show_coords $x $y
	    set ex [expr {[$itk_component(canvas) canvasx $x] / $myscale}]
	    set ey [expr {-[$itk_component(canvas) canvasy $y] / $myscale}]
	} elseif { $coord_type == 2 } {
	    # use index numbers
	    $segment set_vars E $index2
	} else {
	    # model coords
	    set ex $x_coord
	    set ey $y_coord
	}

	if { $state == 2 } {
	    set index2 [llength $VL]
	    lappend VL "$ex $ey"
	    ::Sketch_editor::$segment set_vars E $index2
	}

	if { $coord_type != 2 } {
	    set VL [lreplace $VL $index2 $index2 "$ex $ey"]
	}
	$itk_component(canvas) delete ::Sketch_editor::$segment
	$segment draw ""
	$itk_component(canvas) configure -scrollregion [$itk_component(canvas) bbox all]
	if { $state == 1 } {
	    bind $itk_component(canvas) <ButtonPress-1> {}
	    bind $itk_component(canvas) <B1-Motion> {}
	    bind $itk_component(canvas) <ButtonRelease-1> {}
	    bind $itk_component(canvas) <ButtonRelease-3> {}
	    bind $itk_component(coords).x <Return> {}
	    bind $itk_component(coords).y <Return> {}
	    $itk_component(status_line) configure -text ""
	}
	draw_verts
    }

    method get_tangent_at_vertex { index exclude } {
	set tangent { 0 0 }
	foreach seg $segments {
	    if [lsearch -exact $exclude $seg] continue
	    if [$seg is_vertex_used $index] {
		return [$seg get_tangent_at_vertex $index]
	    }
	}
	return $tangent
    }
}


class Sketch_carc {
    private variable canv
    private variable editor
    private variable start_index -1
    private variable end_index -1
    private variable radius -1
    private variable center_is_left -1
    private variable orientation -1

    constructor { sketch_editor canvas seg } {
	set editor $sketch_editor
	set canv $canvas
	eval "set_vars $seg"
    }

    method set_vars { args } {
	foreach {key value} $args {
	    switch $key {
		S { set start_index $value }
		E { set end_index $value }
		R { set radius $value }
		L { set center_is_left $value }
		O { set orientation $value }
	    }
	}
    }

    method reverse_orientation {} {
	set orientation [expr { !$orientation }]
    }

    method tag_verts { atag } {
	$canv addtag $atag withtag p$start_index
	$canv addtag $atag withtag p$end_index
    }

    method fix_vertex_reference { deleted_verts } {
	foreach index $deleted_verts {
	    if { $start_index > $index } {incr start_index -1}
	    if { $end_index > $index } {incr end_index -1}
	}
    }

    method is_vertex_used { index } {
	if { $start_index == $index } {return 1}
	if { $end_index == $index } {return 1}
	return 0
    }

    method get_verts {} {
	return "$start_index $end_index"
    }

    method get_tangent_at_vertex { index } {
	if { $index != $start_index && $index != $end_index } {
	    return "0 0"
	}
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set sx [expr {$myscale * [lindex $start 0]}]
	set sy [expr {-$myscale * [lindex $start 1]}]
	set end [lindex $vlist $end_index]
	set ex [expr {$myscale * [lindex $end 0]}]
	set ey [expr {-$myscale * [lindex $end 1]}]
	if { $radius < 0.0 } {
	    if { $vertex == $end_index } {
		return "0 0"
	    }
	    set normalx [expr $ex - $sx]
	    set normaly [expr $ey - $sy]
	    set len [Sketch_editor::dist $sx $sy $ex $ey]
	    if { [catch {expr 1.0 / $len} one_over_len] } {
		return "0 0"
	    }
	    set tangentx [expr -$normaly * $one_over_len ]
	    set tangenty [expr $normalx * $one_over_len ]
	} else {
	    set tmp_radius [expr {$myscale * $radius}]
	    set center [Sketch_editor::find_arc_center $sx $sy $ex $ey $tmp_radius $center_is_left]
	    set cx [lindex $center 0]
	    set cy [lindex $center 1]
	    if { $vertex == $start_index } {
		set normalx [expr $sx - $cx]
		set normaly [expr $sy - $cy]
		set len [Sketch_editor::dist $sx $sy $cx $cy]
		if { [catch {expr 1.0 / $len} one_over_len] } {
		    return "0 0"
		}
		if { $orientation } {
		    set tangentx [expr $normaly * $one_over_len ]
		    set tangenty [expr -$normalx * $one_over_len ]
		} else  {
		    set tangentx [expr -$normaly * $one_over_len ]
		    set tangenty [expr $normalx * $one_over_len ]
		}
	    } else {
		set normalx [expr $ex - $cx]
		set normaly [expr $ey - $cy]
		set len [Sketch_editor::dist $ex $ey $cx $cy]
		if { [catch {expr 1.0 / $len} one_over_len] } {
		    return "0 0"
		}
		if { $orientation } {
		    set tangentx [expr -$normaly * $one_over_len ]
		    set tangenty [expr $normalx * $one_over_len ]
		} else  {
		    set tangentx [expr $normaly * $one_over_len ]
		    set tangenty [expr -$normalx * $one_over_len ]
		}
	    }
	}
	return "$tangentx $tangenty"
    }

    method describe {} {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set end [lindex $vlist $end_index]

	if { $radius < 0.0 } {
	    puts "full circle centered at vertex #$end_index ($end) with vertex #$start_index ($start)"
	} else {
	    puts "circular arc (radius = $radius) from vertex #$start_index ($start) to #$end_index ($end)"
	}
	puts "	[$this serialize [$editor get_tobase]]"
    }

    method get_radius {} {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	if { $radius >= 0.0 } {
	    return $radius
	} else {
	    set start [lindex $vlist $start_index]
	    set end [lindex $vlist $end_index]
	    set sx [expr {$myscale * [lindex $start 0]}]
	    set sy [expr {-$myscale * [lindex $start 1]}]
	    set ex [expr {$myscale * [lindex $end 0]}]
	    set ey [expr {-$myscale * [lindex $end 1]}]
	    set tmp_radius [Sketch_editor::dist $sx $sy $ex $ey]
	    return $tmp_radius
	}
    }

    method serialize { tobase } {
	if { $radius < 0.0 } {
	    return "{ carc S $start_index E $end_index R $radius L $center_is_left O $orientation } "
	} else {
	    return "{ carc S $start_index E $end_index R [expr {$tobase * $radius}] L $center_is_left O $orientation }"
	}
    }

    method highlight {} {
	$canv itemconfigure $this -outline red
	$canv addtag selected withtag $this
    }

    method unhighlight {} {
	$canv itemconfigure $this -outline black
	$canv dtag $this selected
    }

    method is_full_circle {} {
	if { $radius < 0.0 } {
	    return 1
	} else {
	    return 0
	}
    }

    method draw { atag } {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	if { $start_index > -1 } {
	    set start [lindex $vlist $start_index]
	    set sx [expr {$myscale * [lindex $start 0]}]
	    set sy [expr {-$myscale * [lindex $start 1]}]
	}
	if { $end_index > -1 } {
	    set end [lindex $vlist $end_index]
	    set ex [expr {$myscale * [lindex $end 0]}]
	    set ey [expr {-$myscale * [lindex $end 1]}]
	}

	if { $radius < 0.0 } {
	    # full circle
	    set tmp_radius [Sketch_editor::dist $sx $sy $ex $ey]
	    $editor set_radius $tmp_radius
	    set x1 [expr {$ex - $tmp_radius}]
	    set y1 [expr {$ey - $tmp_radius}]
	    set x2 [expr {$ex + $tmp_radius}]
	    set y2 [expr {$ey + $tmp_radius}]
	    if {[lsearch -exact $atag selected] != -1} {
		$canv create oval $x1 $y1 $x2 $y2 -outline red -tags [concat $this segs $atag]
	    } else {
		$canv create oval $x1 $y1 $x2 $y2 -tags [concat $this segs $atag]
	    }
	} elseif { $radius > 0.0 } {
	    # arc
	    set tmp_radius [expr {$myscale * $radius}]
	    set center [Sketch_editor::find_arc_center $sx $sy $ex $ey $tmp_radius $center_is_left]
	    set cx [lindex $center 0]
	    set cy [lindex $center 1]
	    set min_radius [::Sketch_editor::dist $sx $sy $cx $cy]
	    if { $tmp_radius < $min_radius } {
		set tmp_radius $min_radius
		set radius [expr { $tmp_radius / $myscale }]
		# show the new radius in the window
		$editor set_radius $radius
	    }
	    set x1 [expr {$cx - $tmp_radius}]
	    set y1 [expr {$cy - $tmp_radius}]
	    set x2 [expr {$cx + $tmp_radius}]
	    set y2 [expr {$cy + $tmp_radius}]
	    set start_ang [expr {-$Sketch_editor::rad2deg * atan2( ($sy - $cy), ($sx - $cx))}]
	    set end_ang [expr {-$Sketch_editor::rad2deg * atan2( ($ey - $cy), ($ex - $cx))}]
	    if { $orientation == 0 } {
		while { $end_ang < $start_ang } {
		    set end_ang [expr {$end_ang + 360.0}]
		}
	    } else {
		while { $end_ang > $start_ang } {
		    set end_ang [expr {$end_ang - 360.0}]
		}
	    }
	    if {[lsearch -exact $atag selected] != -1} {
		$canv create arc $x1 $y1 $x2 $y2 \
		    -start $start_ang \
		    -extent [expr {$end_ang - $start_ang}] \
		    -outline red \
		    -style arc -tags [concat $this segs $atag]
	    } else {
		$canv create arc $x1 $y1 $x2 $y2 \
		    -start $start_ang \
		    -extent [expr {$end_ang - $start_ang}] \
		    -style arc -tags [concat $this segs $atag]
	    }
	}
    }
}


proc bezdex { i j k num_pts } {
    return [expr $i * [expr $num_pts * 2] + $j * 2 + $k]
}

proc calc_bezier {num_pts coords t} {
    for { set j 0 } { $j < $num_pts } { incr j } {
	set vtemp([bezdex 0 $j 0 $num_pts]) [lindex $coords [expr $j * 2]]
	set vtemp([bezdex 0 $j 1 $num_pts]) [lindex $coords [expr $j * 2 + 1]]
    }

    set degree [expr $num_pts - 1]
    for { set i 1 } { $i < $num_pts } { incr i } {
	for { set j 0 } { $j <= [expr $degree - $i] } { incr j } {
	    set vtemp([bezdex $i $j 0 $num_pts]) \
		[ expr \
		      [expr [expr 1.0 - $t] * $vtemp([bezdex [expr $i - 1] $j 0 $num_pts])] + \
		      [expr $t * $vtemp([bezdex [expr $i - 1] [expr $j + 1] 0 $num_pts])]]

	    set vtemp([bezdex $i $j 1 $num_pts]) \
		[ expr \
		      [expr [expr 1.0 - $t] * $vtemp([bezdex [expr $i - 1] $j 1 $num_pts])] + \
		      [expr $t * $vtemp([bezdex [expr $i - 1] [expr $j + 1] 1 $num_pts])]]
	}
    }

    return "$vtemp([bezdex $degree 0 0 $num_pts]) $vtemp([bezdex $degree 0 1 $num_pts])"
}


class Sketch_bezier {
    private variable canv
    private variable editor
    private variable num_points
    private variable index_list

    constructor { sketch_editor canvas seg } {
	set editor $sketch_editor
	set canv $canvas
	eval "set_vars $seg"
    }

    method set_vars { args } {
	foreach { key value } $args {
	    switch $key {
		D { set num_points [expr $value + 1] }
		P { set index_list $value }
	    }
	}
    }

    method get_verts {} {
	return $index_list
    }

    method tag_verts { atag } {
	foreach index $index_list {
	    $canv addtag $atag withtag p$index
	}
    }

    method fix_vertex_reference { deleted_verts } {
	foreach del $deleted_verts {
	    for { set index 0 } { $index < $num_points } { incr index } {
		set vert [lindex $index_list $index]
		if { $vert > $del } {
		    set index_list [lreplace $index_list $index $index [expr $vert - 1]]
		}
	    }
	}
    }

    method is_vertex_used { index } {
	if { [lsearch -exact $index_list $index] != -1 } {
	    return 1
	} else {
	    return 0
	}
    }

    method get_tangent_at_vertex { index } {
	return "0 0"
    }

    method highlight {} {
	$canv itemconfigure $this -fill red
	$canv addtag selected withtag $this
    }

    method unhighlight {} {
	$canv itemconfigure $this -fill black
	$canv dtag $this selected
    }

    method draw { atag } {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	set coords ""
	set count 0

	# scale the control point coordinates
	foreach index $index_list {
	    set pt [lindex $vlist $index]
	    lappend coords [expr {$myscale * [lindex $pt 0]}] [expr {-$myscale * [lindex $pt 1]}]
	    incr count
	}

	# calculate some segments
	set segments [expr $count * 4]
	set points ""
	for { set i 0 } { $i < $segments } { incr i } {
	    set t [expr {1.0 - [expr [expr $segments - $i - 1.0] / [expr $segments - 1.0]]}]
	    lappend points [calc_bezier $count $coords $t]
	}
	set points [join $points]

	# draw line segments instead
	if {[lsearch -exact $atag selected ] != -1} {
	    $canv create line $points -fill red -tags [concat $this segs $atag]
	} else {
	    $canv create line $points -tags [concat $this segs $atag]
	}
    }

    method describe {} {
	set vlist [$editor get_vlist]
	set coords ""
	foreach index $index_list {
	    lappend coords [lindex $vlist $index]
	}
	puts "bezier with $num_points vertices: $coords"
    }

    method serialize { tobase } {
	return "{bezier D [expr $num_points - 1] P [list $index_list]}"
    }
}

class Sketch_line {
    private variable canv
    private variable editor
    private variable start_index -1
    private variable end_index -1

    constructor { sketch_editor canvas seg } {
	set editor $sketch_editor
	set canv $canvas
	eval "set_vars $seg"
    }


    method set_vars { args } {
	foreach {key value} $args {
	    switch $key {
		S { set start_index $value }
		E { set end_index $value }
	    }
	}
    }

    method get_verts {} {
	return "$start_index $end_index"
    }

    method tag_verts { atag } {
	$canv addtag $atag withtag p$start_index
	$canv addtag $atag withtag p$end_index
    }

    method fix_vertex_reference { deleted_verts } {
	foreach index $deleted_verts {
	    if { $start_index > $index } {incr start_index -1}
	    if { $end_index > $index } {incr end_index -1}
	}
    }

    method is_vertex_used { index } {
	if { $start_index == $index } {return 1}
	if { $end_index == $index } {return 1}
	return 0
    }

    method get_tangent_at_vertex { index } {
	if { $index != $start_index && $index != $end_index } {
	    return "0 0"
	}
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set end [lindex $vlist $end_index]
	set dx [expr {[lindex $end 0] - [lindex $start 0]}]
	set dy [expr {[lindex $end 1] - [lindex $start 1]}]
	set len [expr {sqrt($dx * $dx + $dy * $dy)}]
	if { [catch {expr 1.0 / $len} one_over_len] } {
	    return "0 0"
	}
	if { $index == $end_index } {
	    set tangentx [expr $dx * $one_over_len ]
	    set tangenty [expr $dy * $one_over_len ]
	} else {
	    set tangentx [expr -$dx * $one_over_len ]
	    set tangenty [expr -$dy * $one_over_len ]
	}
	return "$tangentx $tangenty"
    }

    method highlight {} {
	$canv itemconfigure $this -fill red
	$canv addtag selected withtag $this
    }

    method unhighlight {} {
	$canv itemconfigure $this -fill black
	$canv dtag $this selected
    }

    method draw { atag } {
	set myscale [$editor get_scale]
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set end [lindex $vlist $end_index]
	set sx [expr {$myscale * [lindex $start 0]}]
	set sy [expr {-$myscale * [lindex $start 1]}]
	set ex [expr {$myscale * [lindex $end 0]}]
	set ey [expr {-$myscale * [lindex $end 1]}]
	if {[lsearch -exact $atag selected ] != -1} {
	    $canv create line $sx $sy $ex $ey -fill red -tags [concat $this segs $atag]
	} else {
	    $canv create line $sx $sy $ex $ey -tags [concat $this segs $atag]
	}
    }

    method describe {} {
	set vlist [$editor get_vlist]
	set start [lindex $vlist $start_index]
	set end [lindex $vlist $end_index]
	puts "line from vertex #$start_index ($start) to #$end_index ($end)"
    }

    method serialize { tobase } {
	return "{ line S $start_index E $end_index }"
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
