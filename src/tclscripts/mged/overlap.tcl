#                     O V E R L A P . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#	O V E R L A P _ T O O L
#
#	Author: John Anderson
#
#	This provide a user interface for eliminating overlaps in the model.
#	This does a very simple-minded fix (subtracting one region from
#	another), but cyclic references are avoided and transformation
#	matrices are considered. The user also has the option of manually
#	editing one of the offending regions by pressing the appropriate
#	button in this gui.
#	This script is sensitive to the output format of "g_lint -s"
#	It expects the output to be in millimeters and in the form:
# overlap path1 path2 overlap_length overlap_start_point overlap_end_point
#	(10 elements per line) with the output sorted so that pairs of regions
#	are kept together, and the "worst" overlap pairs come first.

#	return the index into the 'g_lint' output that is the start point
#	of the last consectutive line that refers to the next region overlap pair
#	The input is:
#		glout - the entire output from g_lint
#		counter - index into $glout that is he start of the current region pair
#		length - the length of $glout
proc get_next_overlap { id glout counter length } {
	global over_cont

	set prev_obj1 [lindex $glout [incr counter]]
	set prev_obj2 [lindex $glout [incr counter]]
	set prev_depth [lindex $glout [incr counter]]
	set over_cont($id,max_depth) $prev_depth
	set counter [expr $counter + 7]
	while { $counter < $length } {
		set obj1 [lindex $glout [incr counter]]
		set obj2 [lindex $glout [incr counter]]
		set depth [lindex $glout [incr counter]]
		if { [string compare $obj1 $prev_obj1] || [string compare $obj2 $prev_obj2] } {
			return [expr $counter - 13]
		}
		if { $depth > $over_cont($id,max_depth) } {
			set over_cont($id,max_depth) $depth
		}
		set counter [expr $counter + 7]
		# this loop may be slow for large output, so do some updates
		mged_update 1
	}
	return [expr $counter - 10]
}

proc count_overlaps { id } {
	global over_cont

	set index 0
	if { $over_cont($id,length) < 3 } {
		set over_cont($id,overlap_count) 0
		return
	}
	set obj1 [lindex $over_cont($id,glint_ret) [incr index]]
	set obj2 [lindex $over_cont($id,glint_ret) [incr index]]
	incr index 8
	set over_cont($id,overlap_count) 1
	while { $index < $over_cont($id,length) } {
		set tmp1 [lindex $over_cont($id,glint_ret) [incr index]]
		set tmp2 [lindex $over_cont($id,glint_ret) [incr index]]

		if { [string compar $tmp1 $obj1] || [string compare $tmp2 $obj2] } {
			incr over_cont($id,overlap_count)
			set obj1 $tmp1
			set obj2 $tmp2
		}
		incr index 8
		# this loop may be slow for large output, so do some updates
		mged_update 1
	}
}

# plot the current pair of regions and the overlaps in the MGED display
proc plot_overlaps { id } {
	global over_cont

	set index $over_cont($id,start)
	set obj1 [lindex $over_cont($id,glint_ret) [incr index]]
	set obj2 [lindex $over_cont($id,glint_ret) [incr index]]
	incr index
	set done 0
	vdraw delete all
	while { 1 } {
		vdraw write next 0 [lindex $over_cont($id,glint_ret) [incr index]] [lindex $over_cont($id,glint_ret) [incr index]] [lindex $over_cont($id,glint_ret) [incr index]]
		vdraw write next 1 [lindex $over_cont($id,glint_ret) [incr index]] [lindex $over_cont($id,glint_ret) [incr index]] [lindex $over_cont($id,glint_ret) [incr index]]
		incr index
		if { $index >= $over_cont($id,length) } break
		if { [string compare $obj1 [lindex $over_cont($id,glint_ret) [incr index]]] } break
		if { [string compare $obj2 [lindex $over_cont($id,glint_ret) [incr index]]] } break
		incr index
		# this loop may be slow for large output, so do some updates
		mged_update 1
	}
	Z
	draw -C 0/255/0 $obj1
	draw -C 0/255/255 $obj2
	vdraw send
	mged_update 1
}

# go to the next overlap and display the available options for fixing it
proc next_overlap { id } {
	global over_cont

	if { $over_cont($id,cur_ovr_index) >= $over_cont($id,length) } {
		$over_cont($id,top).status configure -text "Done"
		$over_cont($id,top).go_quit.go configure -state normal
		grid forget $over_cont($id,work_frame)
		update
		return
	}
	set over_cont($id,start) $over_cont($id,cur_ovr_index)
	set over_cont($id,cur_ovr_index) [get_next_overlap $id $over_cont($id,glint_ret)\
		$over_cont($id,cur_ovr_index) $over_cont($id,length)]
	set obj1 [lindex $over_cont($id,glint_ret) [incr over_cont($id,cur_ovr_index)]]
	set obj2 [lindex $over_cont($id,glint_ret) [incr over_cont($id,cur_ovr_index)]]
	$over_cont($id,work_frame).l0 configure -text "Overlap #$over_cont($id,overlap_no) of $over_cont($id,overlap_count)"
	$over_cont($id,work_frame).l1 configure -text "Object1: $obj1"
	$over_cont($id,work_frame).l3 configure -text "Object2: $obj2"
	incr over_cont($id,cur_ovr_index)
	set overlap_length [expr $over_cont($id,max_depth) / $over_cont($id,local2base)]
	set overlap_text [format "by as much as %g $over_cont($id,localunit)" $overlap_length]
	$over_cont($id,work_frame).l4 configure -text $overlap_text

	set index [string last "/" $obj1]
	if { $index != -1 } {
		set over_cont($id,r1_name) [string range $obj1 [incr index] end]
	} else {
		set over_cont($id,r1_name) $obj1
	}

	set index [string last "/" $obj2]
	if { $index != -1 } {
		set over_cont($id,r2_name) [string range $obj2 [incr index] end]
	} else {
		set over_cont($id,r2_name) $obj2
	}

	set type1 [lindex [db get $over_cont($id,r1_name)] 0]
	set type2 [lindex [db get $over_cont($id,r2_name)] 0]

	if { $type1 != "comb" || $type2 != "comb" } {
	    tk_messageBox -icon error -type ok -title "Primitive Overlap" -message \
	    "This overlap involves primitives, not regions. This tool does not handle such overlaps"
	    $over_cont($id,work_frame).b1 configure -state disabled
	    set b1_disabled 1
	    $over_cont($id,work_frame).b2 configure -state disabled
	    set b2_disabled 1
	    $over_cont($id,work_frame).b3 configure -state disabled
	    $over_cont($id,work_frame).b4 configure -state disabled

	} else {

	    set b1_disabled 0
	    set b2_disabled 0
	    $over_cont($id,work_frame).b3 configure -state normal
	    $over_cont($id,work_frame).b4 configure -state normal
	    if { [check_cycle $over_cont($id,r1_name) $over_cont($id,r2_name)] } {
		$over_cont($id,work_frame).b1 configure -state disabled
		set b1_disabled 1
	    } else {
		$over_cont($id,work_frame).b1 configure -state normal
	    }

	    if { [check_cycle $over_cont($id,r2_name) $over_cont($id,r1_name)] } {
		$over_cont($id,work_frame).b2 configure -state disabled
		set b2_disabled 1
	    } else {
		$over_cont($id,work_frame).b2 configure -state normal
	    }

	    if { $b1_disabled && $b2_disabled } {
		tk_messageBox -message "Cannot simply subtract one region from another here\n\
			without creating a reference cycle" -icon warning -type ok
	    }
	}
	set over_cont($id,cur_ovr_index) [expr $over_cont($id,cur_ovr_index) + 7]
	if { $over_cont($id,cur_ovr_index) >= $over_cont($id,length) } {
		$over_cont($id,top).status configure -text "Done"
		$over_cont($id,top).go_quit.go configure -state normal
		$over_cont($id,work_frame).b5 configure -state disabled
		update
	}
	incr over_cont($id,overlap_no)
	update
}

proc read_output { id } {
	global over_cont mged_gui

	set inn [read $over_cont($id,fd)]
	if { [eof $over_cont($id,fd)] } {
		catch "close $over_cont($id,fd)"
		if { [string length $inn] > 0 } {
			append over_cont($id,glint_ret) $inn
		}
		catch "file delete /tmp/g_lint_error"
		$over_cont($id,top).status configure -text "Processing output..."
		update
		set over_cont($id,length) [llength $over_cont($id,glint_ret)]
		set over_cont($id,cur_ovr_index) 0
		set over_cont($id,start) 0
		count_overlaps $id
		if { $over_cont($id,overlap_count) == 0 } {
			cad_dialog $over_cont($id,dialog_window) $mged_gui($id,screen) "Overlap Tool" "No overlaps found" info 0 OK
		}
		set over_cont($id,overlap_no) 1
		grid $over_cont($id,work_frame) -row 4 -column 0 -columnspan 6 -sticky ew
		next_overlap $id
	}
	append over_cont($id,glint_ret) $inn
}

# run 'g_lint' and display the frame containing the fixing options
proc fix_overlaps { id } {
	global over_cont 

	set over_cont($id,glint_ret) ""

	if { $over_cont($id,ray) == "ray" } {
		# check on validity of objects for raytracing
		set bad_objs ""
		foreach obj $over_cont($id,objs) {
			catch {set ret [t $obj]}
			if { [string first $obj $ret] != 0 } {
				lappend bad_objs $obj
			}
		}
		if { [llength $bad_objs] > 0 } {
			tk_messageBox -icon error -type ok -title "Bad Object Names" -message "Unrecognized object names:\n$bad_objs"
			return
		}

		set size_in_mm [expr $over_cont($id,local2base) * $over_cont($id,size)]
		$over_cont($id,top).status configure -text "Shooting rays at $size_in_mm mm, this may take some time ..."
		$over_cont($id,top).go_quit.go configure -state disabled
		$over_cont($id,work_frame).b5 configure -state normal
		update
		set model [opendb]
		set file_name "| g_lint -s -a $over_cont($id,az) -e $over_cont($id,el) -g $size_in_mm $model $over_cont($id,objs) 2> /tmp/g_lint_error"
		set over_cont($id,fd) [open $file_name]
		fconfigure $over_cont($id,fd) -blocking false
		
		if { $over_cont($id,fd) < 1 } {
			set fd [open /tmp/g_lint_error]
			set mess [read $fd]
			close $fd
			file delete /tmp/g_lint_error
			tk_messageBox -icon error -message $mess -title "g_lint error" -type ok
			$over_cont($id,top).status configure -text "ready"
			$over_cont($id,top).go_quit.go configure -state normal
			return
		}
		set over_cont($id,pid) [pid $over_cont($id,fd)]
		fileevent $over_cont($id,fd) readable "read_output $id"
	} else {
		# read overlap file
		if { [catch {open $over_cont($id,file) "r"} over_cont($id,fd)] } {
			tk_messageBox -icon error -type ok -title "Bad File Name" -message "Cannot open $over_cont($id,file)\n\
					$over_cont($id,fd)"
			return
		}
		$over_cont($id,top).status configure -text "Reading overlap file..."
		$over_cont($id,top).go_quit.go configure -state disabled
		$over_cont($id,work_frame).b5 configure -state normal
		update
		read_output $id
	}
}

#	This routine takes the name of a region and a tree (as returned by
#	"db get"), and returns the first instance of that region in the tree
#	in the form of a "leaf" node from the tree.
proc get_leaf { region tree } {
	set l1 [lindex $tree 1]
	set l2 [lindex $tree 2]

	set op [lindex $l1 0]
	if { [string compare $op "l"] == 0 } {
		set leaf [lindex $l1 1]
		if { [string compare $leaf $region] == 0 } {
			return $l1
		} else { set left "" }
	} else {
		set left [get_leaf $region $l1]
	}
	set op [lindex $l2 0]
	if { [string compare $op "l"] == 0 } {
		set leaf [lindex $l2 1]
		if { [string compare $leaf $region] == 0 } {
			return $l2
		} else { set right "" }
	} else {
		set right [get_leaf $region $l2]
	}

	if { [lsearch -exact $left $region] > -1 } {
		return $left
	}
	if { [lsearch -exact $right $region] > -1 } {
		return $right
	}
}

#	This routine takes a path and returns a two element list.
#	The first list element is the path to the combination containing
#	the final element in the path. The second element in the returned list
#	is the last element in the path. If the path consists of a single
#	element, then the first element in the returned list will be "/".
proc get_comb_leaf { path } {
	set index [string last "/" $path]
        if { $index == 0 } {
	    set ret_list [list "/" [string range $path 1 end]]
	} elseif { $index > 0 } {
		set comb [string range $path 0 [expr $index - 1]]
		set region [string range $path [incr index] end]
		set ret_list [list $comb $region]
	} else {
		set ret_list [list "/" $path]
	}
	return $ret_list
}

#	This routine does the actual "db adjust" to include the subtraction
#	chosen by the user. The argument "which" must be "12" or "21",
#	indicating the subtraction should be "obj1 - obj2" or 
#	"obj2 - obj1" respectively.
#	Note that transformations in the path must be taken into acount
proc do_subtract { id which } {
	global over_cont

	if { [string compare $which "12"] == 0 } {
		set name1 [lindex $over_cont($id,glint_ret) [incr over_cont($id,start)]]
		set name2 [lindex $over_cont($id,glint_ret) [incr over_cont($id,start)]]
	} elseif { [string compare $which "21"] == 0 } {
		set name2 [lindex $over_cont($id,glint_ret) [incr over_cont($id,start)]]
		set name1 [lindex $over_cont($id,glint_ret) [incr over_cont($id,start)]]
	}

	set comb_leaf2 [get_comb_leaf $name2]
	set comb2 [lindex $comb_leaf2 0 ]
	set region2 [lindex $comb_leaf2 1]

	set mat2 [list 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1]
	if { [string compare $comb2 "/"] == 0 } {
		set leaf2 [list "l" $region2]
	} else {
		set leaf2 [get_leaf $region2 [db get $comb2 tree]]
		if { [llength $leaf2] > 2 } {
			set mat2 [lindex $leaf2 2]
		}
	}

	set comb_leaf1 [get_comb_leaf $name1]
	set comb1 [lindex $comb_leaf1 0]
	set region1 [lindex $comb_leaf1 1]
	set mat1 [list 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1]
	if { [string compare $comb1 "/"] != 0 } {
		set leaf1 [get_leaf $region1 [ db get $comb1 tree]]
		if { [llength $leaf1] > 2 } {
			set mat1 [lindex $leaf1 2]
		}
	}
	set inv [mat_inv $mat1]
	set final_mat [mat_mul $inv $mat2]

	db adjust $region1 tree [list "-" [db get $region1 tree] [list "l" $region2 $final_mat ]]

	next_overlap $id
}

# pop up the combination editor for object 1
proc edit_object1 { id } {
	global comb_control over_cont

	init_comb $id
	set comb_control($id,name) $over_cont($id,r1_name)
	comb_reset $id
}

# pop up the combination editor for object 2
proc edit_object2 { id } {
	global comb_control over_cont

	init_comb $id
	set comb_control($id,name) $over_cont($id,r2_name)
	comb_reset $id
}

# clean-up a bit and quit
proc over_quit { id } {
	global over_cont

	if { [string length $over_cont($id,objs)] > 0 } {
		eval draw $over_cont($id,objs)
		eval view center $over_cont($id,center)
		eval view size $over_cont($id,vsize)
		eval view aet $over_cont($id,aet)
		eval view eye $over_cont($id,eye)
	}
    catch {vdraw delete all}
	vdraw send

	if { $over_cont($id,ray) == "ray" && $over_cont($id,fd) > 0 } {
	    catch {exec kill -9 [lindex $over_cont($id,pid) 0]}
	    catch {close $over_cont($id,fd)}
	}
    catch {destroy $over_cont($id,dialog_window)}
    catch {unset over_cont($id,glint_ret)}
	destroy $over_cont($id,top)
}

proc ray_setup { id } {
	global over_cont

	set fr [grid slaves $over_cont($id,top) -row 2]
	set fr_list [split $fr "."]
	set cur_fr [lindex $fr_list end]
	if { $cur_fr == "ray_fr" } return

	grid forget $over_cont($id,top).glint_fr
	grid $over_cont($id,top).ray_fr -row 2 -column 0 -columnspan 6
}

proc glint_setup { id } {
	global over_cont

	set fr [grid slaves $over_cont($id,top) -row 2]
	set fr_list [split $fr "."]
	set cur_fr [lindex $fr_list end]
	if { $cur_fr == "glint_fr" } return

	grid forget $over_cont($id,top).ray_fr
	grid $over_cont($id,top).glint_fr -row 2 -column 0 -columnspan 6
}

# This is the top level entry point
proc overlap_tool { id } {
    global over_cont comb_control localunit local2base
    global ::tk::Priv tk_version

    if { [info exists tk_version] == 0 } {
	puts "Cannot run the overlap tool without Tk loaded"
	return
    }

    set db [opendb]
    if { [string length $db] == 0 } {
	tk_messageBox -icon warning -type ok -title "No Database Open" -message \
		"Cannot run the overlap tool without a database open"
	return
    }

	set over_cont($id,top) .fovr_$id
	if [winfo exists $over_cont($id,top)] {
		if { [winfo ismapped $over_cont($id,top)] == 0 } {
			wm deiconify $over_cont($id,top)
		}
		raise $over_cont($id,top)
		return
	}

	set over_cont($id,objs) ""
	set objs [who]
	if { [string length $objs] == 0 } {
		set objs "all"
	} else {
		set over_cont($id,objs) $objs
	}

	set over_cont($id,center) [view center]
	set over_cont($id,vsize) [view size]
	set over_cont($id,aet) [view aet]
	set over_cont($id,eye) [view eye]
	set over_cont($id,objs) $objs
	set over_cont($id,az) 0
	set over_cont($id,el) 0
	set over_cont($id,local2base) $local2base
	set over_cont($id,localunit) $localunit

	# set initial cell size to 100mm
	set over_cont($id,size) [expr 100 / $local2base]

	toplevel $over_cont($id,top)
	wm title $over_cont($id,top) "Fix Overlaps"
	hoc_register_data $over_cont($id,top) "Fix Overlaps" {
		{summary "This is a tool to help the user eliminate unwanted overlaps\n\
			between regions in a BRL-CAD model." }
	}
	set over_cont($id,ray) "ray"
	radiobutton $over_cont($id,top).raytrace -variable over_cont($id,ray) -value "ray" -command "ray_setup $id"
	label $over_cont($id,top).ray_lab -text "Raytrace to find overlaps"
	hoc_register_data $over_cont($id,top).raytrace "Raytrace to Find Overlaps" {
		{summary "Select this option to do raytracing to discover overlaps" }
	}
	hoc_register_data $over_cont($id,top).ray_lab "Raytrace to Find Overlaps" {
		{summary "Select this option to do raytracing to discover overlaps" }
	}
	radiobutton $over_cont($id,top).glint -variable over_cont($id,ray) -value "glint" -command "glint_setup $id"
	label $over_cont($id,top).glint_lab -text "Read overlaps from g_lint output"
	hoc_register_data $over_cont($id,top).glint "Read Overlaps from a file" {
		{summary "Select this option to read an overlap file produced from 'g_lint -s'" }
	}
	hoc_register_data $over_cont($id,top).glint_lab "Read Overlaps from a file" {
		{summary "Select this option to read an overlap file produced from 'g_lint -s'" }
	}

	frame $over_cont($id,top).ray_fr
	label $over_cont($id,top).ray_fr.objs_l -text "Object(s)"
	entry $over_cont($id,top).ray_fr.objs_e -width 50 -textvariable over_cont($id,objs)
	hoc_register_data $over_cont($id,top).ray_fr.objs_l "Object(s)" {
		{summary "Enter a list of objects to be checked for overlapping regions\n\
			This is normally a top level group."}
	}
	hoc_register_data $over_cont($id,top).ray_fr.objs_e "Object(s)" {
		{summary "Enter a list of objects to be checked for overlapping regions\n\
			This is normally a top level group."}
	}
	label $over_cont($id,top).ray_fr.ray -text "Raytracing Parameters:"
	hoc_register_data $over_cont($id,top).ray_fr.ray "Ray Tracing Parameters" {
		{summary "This is a list of parameters that will be passed to 'g_lint'\n\
			to check the object list for overlapping regions."}
	}
	label $over_cont($id,top).ray_fr.az_l -text "Azimuth:"
	entry $over_cont($id,top).ray_fr.az_e -width 5 -textvariable over_cont($id,az)
	hoc_register_data $over_cont($id,top).ray_fr.az_l "Azimuth" {
		{summary "Enter the azimuth angle to be passed to 'g_lint' for its\n\
			check for overlapping regions."}
		{range "0 through 360 degrees"}
	}
	hoc_register_data $over_cont($id,top).ray_fr.az_e "Azimuth" {
		{summary "Enter the azimuth angle to be passed to 'g_lint' for its\n\
			check for overlapping regions."}
		{range "0 through 360 degrees"}
	}
	label $over_cont($id,top).ray_fr.el_l -text "Elevation:"
	entry $over_cont($id,top).ray_fr.el_e -width 5 -textvariable over_cont($id,el)
	hoc_register_data $over_cont($id,top).ray_fr.el_l "Elevation" {
		{summary "Enter the elevation angle to be passed to 'g_lint' for its\n\
			check for overlapping regions."}
		{range "-90 through 90 degrees"}
	}
	hoc_register_data $over_cont($id,top).ray_fr.el_e "Elevation" {
		{summary "Enter the elevation angle to be passed to 'g_lint' for its\n\
			check for overlapping regions."}
		{range "-90 through 90 degrees"}
	}
	label $over_cont($id,top).ray_fr.size_l -text "Ray Spacing ($localunit):"
	entry $over_cont($id,top).ray_fr.size_e -width 5 -textvariable over_cont($id,size)
	hoc_register_data $over_cont($id,top).ray_fr.size_l "Ray Spacing" {
		{summary "Enter the spacing between rays to be fired while\n\
			looking for overlapping regions."}
		{description "A very small spacing will result in a large number of rays\n\
			being traced. This may take some time. A very large spacing\n\
			is likely to miss some overlapping regions."}
	}
	hoc_register_data $over_cont($id,top).ray_fr.size_e "Ray Spacing" {
		{summary "Enter the spacing between rays to be fired while\n\
			looking for overlapping regions."}
		{description "A very small spacing will result in a large number of rays\n\
			being traced. This may take some time. A very large spacing\n\
			is likely to miss some overlapping regions. A good strategy is\n\
			to start with larger spacing and work towards smaller, eliminating\n\
			the worst overlaps first."}
	}

	frame $over_cont($id,top).glint_fr
	label $over_cont($id,top).glint_fr.file_l -text "Enter overlap file:"
	entry $over_cont($id,top).glint_fr.file_e -textvariable over_cont($id,file) -width 40
	hoc_register_data $over_cont($id,top).glint_fr.file_l "Overlap File" {
		{summary "Enter the name of a file containing overlap data from 'g_lint -s'" }
	}
	hoc_register_data $over_cont($id,top).glint_fr.file_e "Overlap File" {
		{summary "Enter the name of a file containing overlap data from 'g_lint -s'" }
	}

	frame $over_cont($id,top).go_quit
	button $over_cont($id,top).go_quit.go -text go -command "fix_overlaps $id"
	hoc_register_data $over_cont($id,top).go_quit.go "Go" {
		{summary "Press this button to begin the check for overlapping regions."}
	}
	button $over_cont($id,top).go_quit.quit -text "quit" -command "over_quit $id"
	hoc_register_data $over_cont($id,top).go_quit.quit "quit" {
		{summary "Press this button to quit fixing overlapping regions."}
	}
	grid $over_cont($id,top).go_quit.go -row 0 -column 0 -sticky ew
	grid $over_cont($id,top).go_quit.quit -row 0 -column 1 -sticky ew
	label $over_cont($id,top).status -text "ready" -width 50 -relief groove
	hoc_register_data $over_cont($id,top).status "Status" {
		{summary "This line provides some 'status' information while the\n\
			overlap tool is running:\n\
			'ready' - ready for the user to press 'go' and start the check\n\
			'done' - all the discovered overlaps have been processed\n\
			'Shooting rays at ...' - Rays are being traced to discover overlapping regions\n\
			'Reading overlap file ...' - Specified overlap file is beinmg scanned\n\
			'Processing output...' - Individual overlaps are being presented to the user" }
	}
	grid $over_cont($id,top).raytrace -row 0 -column 1 -sticky e
	grid $over_cont($id,top).ray_lab -row 0 -column 2 -sticky w -columnspan 3
	grid $over_cont($id,top).glint -row 1 -column 1 -sticky e
	grid $over_cont($id,top).glint_lab -row 1 -column 2 -sticky w -columnspan 3

	grid $over_cont($id,top).ray_fr.objs_l -row 2 -column 0 -sticky e
	grid $over_cont($id,top).ray_fr.objs_e -row 2 -column 1 -sticky ew -columnspan 5
	grid $over_cont($id,top).ray_fr.ray -row 3 -column 0 -columnspan 3
	grid $over_cont($id,top).ray_fr.az_l -row 4 -column 0 -sticky e
	grid $over_cont($id,top).ray_fr.az_e -row 4 -column 1 -sticky ew
	grid $over_cont($id,top).ray_fr.el_l -row 4 -column 2 -sticky e
	grid $over_cont($id,top).ray_fr.el_e -row 4 -column 3 -sticky ew
	grid $over_cont($id,top).ray_fr.size_l -row 4 -column 4 -sticky e
	grid $over_cont($id,top).ray_fr.size_e -row 4 -column 5 -sticky ew

	grid $over_cont($id,top).ray_fr -row 2 -column 0 -columnspan 6

	grid $over_cont($id,top).glint_fr.file_l -row 0 -column 0 -sticky e
	grid $over_cont($id,top).glint_fr.file_e -row 0 -column 1 -sticky w

	grid $over_cont($id,top).go_quit -row 3 -column 0 -columnspan 6
	grid $over_cont($id,top).status -row 5 -column 0 -columnspan 6 -sticky ew -padx 3 -pady 3
	update
	set over_cont($id,work_frame) [frame $over_cont($id,top).f1]
	label $over_cont($id,work_frame).l0 -text "Overlap #0 of 0"
	label $over_cont($id,work_frame).l1 -text "Object1: "
	label $over_cont($id,work_frame).l2 -text "overlaps"
	label $over_cont($id,work_frame).l3 -text "Object2: "
	label $over_cont($id,work_frame).l4 -text "by as much as 0.0 mm"
	hoc_register_data $over_cont($id,work_frame).l0 "Overlap counter" {
		{summary "Keeps track of how many overlaps have been examined." }
	}
	set reg_data [list [list summary "Object1 and Object2 are regions that appear to\n\
		occupy some of the same space. The maximum length of the overlap\n\
		as discovered by 'g_lint' using the ray trace parameters provided is displayed."]]
	hoc_register_data $over_cont($id,work_frame).l1 "Object1" $reg_data
	hoc_register_data $over_cont($id,work_frame).l2 "overlaps" $reg_data
	hoc_register_data $over_cont($id,work_frame).l3 "Object2" $reg_data
	hoc_register_data $over_cont($id,work_frame).l4 "Maximum overlap" $reg_data
	grid $over_cont($id,work_frame).l0 -row 0 -column 0 -columnspan 5 -sticky ew
	grid $over_cont($id,work_frame).l1 -row 1 -column 0 -columnspan 5 -sticky ew
	grid $over_cont($id,work_frame).l2 -row 2 -column 0 -columnspan 5 -sticky ew
	grid $over_cont($id,work_frame).l3 -row 3 -column 0 -columnspan 5 -sticky ew
	grid $over_cont($id,work_frame).l4 -row 4 -column 0 -columnspan 5 -sticky ew
	button $over_cont($id,work_frame).b1 -text "obj1 - obj2" -command "do_subtract $id 12"
	hoc_register_data $over_cont($id,work_frame).b1 "obj1 - obj2" {
		{summary "Eliminate this overlap by subtracting Object2 from Object1.\n\
			This button will be disabled if its operation would\n\
			create a cyclic reference in the model."}
	}
	button $over_cont($id,work_frame).b2 -text "obj2 - obj1" -command "do_subtract $id 21"
	hoc_register_data $over_cont($id,work_frame).b2 "obj2 - obj1" {
		{summary "Eliminate this overlap by subtracting Object1 from Object2.\n\
			This button will be disabled if its operation would\n\
			create a cyclic reference in the model."}
	}
	button $over_cont($id,work_frame).b3 -text "edit object1" -command "edit_object1 $id"
	hoc_register_data $over_cont($id,work_frame).b3 "edit object1" {
		{summary "Press this button to bring up the combination editor\n\
			and edit object1 manually to eliminate this overlap."}
	}
	button $over_cont($id,work_frame).b4 -text "edit object2" -command "edit_object2 $id"
	hoc_register_data $over_cont($id,work_frame).b4 "edit object2" {
		{summary "Press this button to bring up the combination editor\n\
			and edit object2 manually to eliminate this overlap."}
	}
	button $over_cont($id,work_frame).b5 -text "next overlap" -command "next_overlap $id"
	hoc_register_data $over_cont($id,work_frame).b5 "next overlap" {
		{summary "Press this button to contimue to the next pair of\n\
			overlapping regions without further processing of the current pair."}
	}
	button $over_cont($id,work_frame).b6 -text "plot overlaps" -command "plot_overlaps $id"
	hoc_register_data $over_cont($id,work_frame).b6 "plot overlaps" {
		{summary "Press this button to see a plot of the two overlapping regions\n\
			and the rays that 'g_lint' reported in the overlap area.\n\
			The overlap rays are drawn in red. Object 1 is drawn in green,\n\
			and Object 2 is drawn in cyan."}
	}
	grid $over_cont($id,work_frame).b1 -row 5 -column 0
	grid $over_cont($id,work_frame).b2 -row 5 -column 1
	grid $over_cont($id,work_frame).b3 -row 5 -column 2
	grid $over_cont($id,work_frame).b4 -row 5 -column 3
	grid $over_cont($id,work_frame).b6 -row 5 -column 4
	grid $over_cont($id,work_frame).b5 -row 6 -column 2
	vdraw open overlaps
	vdraw params color ff0000
	
	set over_cont($id,fd) -1
	set over_cont($id,pid) ""
	set over_cont($id,length) 0
	set over_cont($id,overlap_count) 0
	set over_cont($id,dialog_window) $::tk::Priv(cad_dialog)
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
