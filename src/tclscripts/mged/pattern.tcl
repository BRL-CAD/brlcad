#                     P A T T E R N . T C L
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
#
#		P A T T E R N . T C L
#
#	procedures to build duplicates of objects in a specified pattern
#
# Author - John R. Anderson
#
#
#	C R E A T E _ N E W _ N A M E
#
# procedure to create a new unique name for an MGED database object.
#
# Arguments:
#	leaf - the input base name
#	sstr - string to be substituted for in the base name
#	rstr - the replacement string (substituted for "sstr" in base name)
#	increment - amount to increment the number in a name of the form xxx.s15.yyy or xxx.r2.yyy
#
# The increment is performed after the string substitution.
#
# If after the above substitutions a unique name is not produced, an "_x" is suffixed to the
# resulting name (where x is an integer)
#
# Returns a new unique object name

proc create_new_name { leaf sstr rstr increment } {
	if { [string length $sstr] != 0 } {
		set prefix ""
		set suffix ""
		set sstr_index [string first $sstr $leaf]
		if { $sstr_index >= 0 } {
			set prefix [string range $leaf 0 [expr $sstr_index - 1]]
			set suffix [string range $leaf [expr $sstr_index + [string length $sstr]] end]
			set new_name ${prefix}${rstr}${suffix}
		} else {
			set new_name $leaf
		}
	} else {
		set new_name $leaf
	}
	if { $increment > 0 } {
		if { [regexp {([^\.]*)(\.)([sr])([0-9]*)(.*)} $new_name the_match tag a_dot obj_type num tail] } {
			set new_name "${tag}.${obj_type}[expr $num + $increment]${tail}"
		}
	}

	# make sure this name doesn't already exist
	set dummy 0
	set base_name $new_name
	while { [obj_exists $new_name] == 1 } {
		incr dummy
		set max_len [expr 16 - [string length $dummy] - 1]
		set new_name "[string range $base_name 0 $max_len]_$dummy"
	}

	return $new_name
}

proc copy_tree { args } {
	set usage "Usage:\n\t copy_tree  \[-s source_string replacement_string | -i increment\] \[-primitives\] tree_to_be_copied"
	set sstr ""
	set rstr ""
	set increment 0
	set depth "regions"
	set tree ""
	set opt_str ""
	set argc [llength $args]
	if { $argc < 1 || $argc > 7 } {
		error $usage
	}
	incr argc -1
	set index 0
	while { $index < $argc } {
		set opt [lindex $args $index]
		switch -- $opt {
			"-s" {
				incr index
				set sstr [lindex $args $index]
				incr index
				set rstr [lindex $args $index]
				if { [string length $sstr] && [string length $rstr] } {
					set opt_str "$opt_str -s $sstr $rstr"
				}
			}
			"-i" {
				incr index
				set increment [lindex $args $index]
				set opt_str "$opt_str -i $increment"
			}
			"-primitives" {
				set depth "primitives"
				set opt_str "$opt_str -primitives"
			}
			"-regions" {
				set depth "regions"
			}
			default {
				error "copy_tree Unrecognized option: $opt"
			}
		}
		incr index
	}

	set tree [lindex $args $index]

	set op [lindex $tree 0]
	switch -- $op {
		"l" {
			set leaf [lindex $tree 1]
			if { [catch {db get $leaf} leaf_db] } {
				puts "WARNING: $leaf does not actually exist"
				return [list "l" $leaf]
			}
			set type [lindex $leaf_db 0]
			if { $type != "comb" && $depth != "primitives" } {
				# we have reached a leaf primitive, but we don't want to copy the primitives
				return $tree
			}
			if { [llength $tree] == 3 } {
				set old_mat [lindex $tree 2]
			} else {
				set old_mat [mat_idn]
			}

			# create new name for this object
			set new_name [create_new_name $leaf $sstr $rstr $increment]

			if { $type != "comb" } {
				# this is a primitive
				if { [catch {eval db put $new_name $leaf_db} ret] } {
					error "Cannot create copy of primitive $leaf as $new_name\n\t$ret"
				}
			} else {
				#this is a combination
				set index [lsearch -exact $leaf_db "region"]
				incr index
				set region 0
				if { [lindex $leaf_db $index] == "yes" } {
					# this is a region
					set region 1
					if { $depth == "regions" } {
						# just copy the region to the new name
						if { [catch {eval db put $new_name $leaf_db} ret] } {
							error "Cannot create copy of region $leaf as $new_name\n\t$ret"
						}
						# adjust region id
						set regdef [regdef]
						set id [lindex $regdef 1]
						if { [catch {db adjust $new_name id $id} ret] } {
							error "Cannot adjust region ident for $new_name!!!\n\t$ret"
						}
						incr id
						regdef $id
						return [list "l" $new_name $old_mat]
					}
				}
				set index [lsearch -exact $leaf_db "tree"]
				if { $index < 0 } {
					error "Combination $leaf has no Boolean tree!!!"
				}
				incr index
				set old_tree [lindex $leaf_db $index]
				set new_tree [eval copy_tree $opt_str [list $old_tree]]
				if { [catch {eval db put $new_name $leaf_db} ret] } {
					error "Cannot create copy of combination $leaf as $new_name\n\t$ret"
				}
				if { [catch {db adjust $new_name tree $new_tree} ret] } {
					error "Cannot adjust tree in new combination named $new_name\n\t$ret"
				}
				if { $region } {
					# set region ident according to regdef
					set regdef [regdef]
					set id [lindex $regdef 1]
				    if { [catch {db adjust $new_name id $id} ret] } {
						error "Cannot adjust ident number for region ($new_name)!!!!\n\t$ret"
					}
					incr id
					regdef $id
				}
			}
			return [list "l" $new_name $old_mat]
		}
		"-" -
		"+" -
		"n" -
		"u" {
			set left [lindex $tree 1]
			set right [lindex $tree 2]
			set new_left [eval copy_tree $opt_str [list $left]]
			set new_right [eval copy_tree $opt_str [list $right]]
			return [list $op $new_left $new_right]
		}
	}
}

proc copy_obj { args } {
	set usage "Usage:\n\tcopy_obj \[-s source_string replacement_string | -i increment\] \[-primitives\] object_to_be_copied"
	set sstr ""
	set rstr ""
	set increment 0
	set depth "regions"
	set obj ""
	set argc [llength $args]
	if { $argc < 1 || $argc > 7 } {
		puts "Error in command: copy_obj $args\nwrongh number of arguments ($argc)"
		error $usage
	}
	incr argc -1
	set opt_str ""
	set index 0
	while { $index < $argc } {
		set opt [lindex $args $index]
		switch -- $opt {
			"-s" {
				incr index
				set sstr [lindex $args $index]
				incr index
				set rstr [lindex $args $index]
				if { [string length $sstr] && [string length $rstr] } {
					set opt_str "$opt_str -s $sstr $rstr"
				}
			}
			"-i" {
				incr index
				set increment [lindex $args $index]
				set opt_str "$opt_str -i $increment"
			}
			"-primitives" {
				set depth "primitives"
				set opt_str "$opt_str -primitives"
			}
			"-regions" {
				set depth "regions"
				set opt_str "$opt_str -regions"
			}
			default {
				error "Copy_obj: Unrecognized option: $opt"
			}
		}
		incr index
	}

	set obj [lindex $args $index]
	if { [catch {db get $obj} obj_db] } {
		error "Cannot retrieve object $obj\n\t$obj_db"
	}

	set type [lindex $obj_db 0]
	if { $type != "comb" } {
		# object is a primitive
		if { $depth != "primitives" } {
			error "Trying to copy a primitive ($obj) with depth at regions!!!!"
		}
		# just copy the primitive to a new name
		set new_name [create_new_name $obj $sstr $rstr $increment]
		if { [catch {eval db put $new_name $obj_db} ret] } {
			error "cannot copy $obj to $new_name!!!\n\t$ret"
		}
		return $new_name
	}

	# this is a combination
	set region 0
	set region_idx [lsearch -exact $obj_db "region"]
	if { $region_idx < 0 } {
		error "Combination ($obj) does not have a region attribute!!!"
	}
	incr region_idx
	if { [lindex $obj_db $region_idx] == "yes" } {
		# this is a region
		set region 1
		if { $depth == "regions" } {
			# just copy this region to a new name
			set new_name [create_new_name $obj $sstr $rstr $increment]
			if { [catch {eval db put $new_name $obj_db} ret] } {
				error "Cannot copy $obj to $new_name!!!\n\t$ret"
			}
			set regdef [regdef]
			set id [lindex $regdef 1]
			if { [catch {db adjust $new_name id $id} ret] } {
				error "Cannot adjust ident for region ($new_name)!!!!\n\t$ret"
			}
			incr id
			regdef $id
			return $new_name
		}

	}

	# copy the tree, copy the combination, then adjust the tree
	set tree_idx [lsearch -exact $obj_db "tree"]
	if { $tree_idx < 0 } {
		error "Region ($obj) has no tree!!!!"
	}
	incr tree_idx
	set tree [lindex $obj_db $tree_idx]
	set new_tree [eval copy_tree $opt_str [list $tree]]
	set new_name [create_new_name $obj $sstr $rstr $increment]
	if { [catch {eval db put $new_name $obj_db} ret] } {
		error "Cannot copy $obj to $new_name!!!\n\t$ret"
	}
	if { [catch {db adjust $new_name tree $new_tree} ret] } {
		error "Cannot adjust tree on new combination ($new_name)!!!!\n\t$ret"
	}

	if { $region } {
		# set region ident according to regdef
		set regdef [regdef]
		set id [lindex $regdef 1]
		if { [catch {db adjust $new_name id $id} ret] } {
			error "Cannot adjust ident number for region ($new_name)!!!!\n\t$ret"
		}
		incr id
		regdef $id
	}
	return $new_name
}

proc obj_exists { obj_name } {
	if { [catch {db get $obj_name} ret] } {
		return 0
	} else {
		return 1
	}
}



proc pattern_rect { args } {
        global local2base

    	set usage "Usage:\n\tpattern_rect \[-top|-regions|-primitives\] \[-g group_name\] \
		 \[-xdir { x y z }\] \[-ydir { x y z }\] \[-zdir { x y z }\] \
		\[-nx num_x -dx delta_x | -lx list_of_x_values\]\n\t\t \
		\[-ny num_y -dy delta_y | -ly list_of_y_values\] \[-nz num_z -dz delta_z | -lz list_of_z_values\] \
		\[-s source_string replacement_string\] \[-i increment\]  object1 \[object2 object3 ...\]"

        init_vmath

	set opt_str ""
	set group_name ""
	set group_list {}
	set index 0
	set depth top
	set got_depth 0
	set xdir { 1 0 0 }
	set ydir { 0 1 0 }
	set zdir { 0 0 1 }
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
	set inc 1
	set objs {}
	set feed_name ""
	set argc [llength $args]
	while { $index < $argc } {
		set opt [lindex $args $index]
		switch -- $opt {
			"-top" {
				set depth top
				set opt_str "$opt_str -top"
				incr index
			}
			"-regions" {
				set depth regions
				set opt_str "$opt_str -regions"
				incr index
			}
			"-primitives" {
				set depth primitives
				set opt_str "$opt_str -primitives"
				incr index
			}
			"-g" {
				incr index
				set group_name [lindex $args $index]
				incr index
			}
			"-xdir" {
			    incr index
			    set xdir [lindex $args $index]
			    incr index
			}
			"-ydir" {
			    incr index
			    set ydir [lindex $args $index]
			    incr index
			}
			"-zdir" {
			    incr index
			    set zdir [lindex $args $index]
			    incr index
			}
			"-nx" {
				incr index
				set num_x [lindex $args $index]
				incr index
			}
			"-ny" {
				incr index
				set num_y [lindex $args $index]
				incr index
			}
			"-nz" {
				incr index
				set num_z [lindex $args $index]
				incr index
			}
			"-dx" {
				incr index
				set delta_x [lindex $args $index]
				incr index
			}
			"-dy" {
				incr index
				set delta_y [lindex $args $index]
				incr index
			}
			"-dz" {
				incr index
				set delta_z [lindex $args $index]
				incr index
			}
			"-lx" {
				incr index
				set list_x [lindex $args $index]
				incr index
			}
			"-ly" {
				incr index
				set list_y [lindex $args $index]
				incr index
			}
			"-lz" {
				incr index
				set list_z [lindex $args $index]
				incr index
			}
			"-s" {
				incr index
				set sstr [lindex $args $index]
				incr index
				set rstr [lindex $args $index]
				incr index
				if { [string length $rstr] && [string length $sstr] } {
					set opt_str "$opt_str -s $sstr $rstr"
				}
			}
			"-i" {
				incr index
				set increment [lindex $args $index]
				set inc $increment
				incr index
			}
			"-feed_name" {
			    incr index
			    set feed_name [lindex $args $index]
			    incr index
			}
			default {
				set objs [lrange $args $index end]
				set index $argc
			}
		}
	}

	if { [llength $objs] < 1 } {
		error "no objects specified!!!\n$usage"
	}

	if {	[llength $list_x] == 0 && $num_x == 0 &&
		[llength $list_y] == 0 && $num_y == 0 &&
		[llength $list_z] == 0 && $num_z == 0 } {
			error "no X, Y, or Z values provided!!!!\n$usage"
	}

	if { $num_x > 1 && $delta_x == 0 } {
		error "no X delta provided\n$usage"
	}

	if { $num_y > 1 && $delta_y == 0 } {
		error "no Y delta provided\n$usage"
	}

	if { $num_z > 1 && $delta_z == 0 } {
		error "no Z delta provided\n$usage"
	}

	if { $num_x } {
		set list_x {}
		for { set index 1 } { $index <= $num_x } { incr index } {
			lappend list_x [expr $delta_x * $index]
		}
	} else {
		set num_x [llength $list_x]
	}

	if { $num_y } {
		set list_y {}
		for { set index 1 } { $index <= $num_y } { incr index } {
			lappend list_y [expr $delta_y * $index]
		}
	} else {
		set num_y [llength $list_y]
	}

	if { $num_z } {
		set list_z {}
		for { set index 1 } { $index <= $num_z } { incr index } {
			lappend list_z [expr $delta_z * $index]
		}
	} else {
		set num_z [llength $list_z]
	}

	if { $num_x == 0 } {
		set list_x { 0 }
	}

	if { $num_y == 0 } {
		set list_y { 0 }
	}

	if { $num_z == 0 } {
		set list_z { 0 }
	}

	set xlen [llength $list_x]
	set ylen [llength $list_y]
	set zlen [llength $list_z]
	$feed_name configure -steps [expr $xlen * $ylen * $zlen]

	# unitize direction vectors
	set xdir [vunitize $xdir]
	set ydir [vunitize $ydir]
	set zdir [vunitize $zdir]

        # convert to local units
        for { set i 0 } { $i < $num_x } { incr i } {
	    set list_x [lreplace $list_x $i $i [expr [lindex $list_x $i] * $local2base]]
	}
        for { set i 0 } { $i < $num_y } { incr i } {
	    set list_y [lreplace $list_y $i $i [expr [lindex $list_y $i] * $local2base]]
	}
        for { set i 0 } { $i < $num_z } { incr i } {
	    set list_z [lreplace $list_z $i $i [expr [lindex $list_z $i] * $local2base]]
	}

	set x_index 0
	foreach x $list_x {
		incr x_index
	        set x_vec [vscale $xdir $x]
		set y_index 0
		foreach y $list_y {
			incr y_index
		        set y_vec [vscale $ydir $y]
			set z_index 0
			foreach z $list_z {
				incr z_index
			        set z_vec [vscale $zdir $z]
				set mat [mat_deltas_vec [mat_idn] [vadd3 $x_vec $y_vec $z_vec]]
				foreach obj $objs {
					switch $depth {
						"top" {
							set base_new_name ${obj}_${x_index}_${y_index}_${z_index}
							set new_name [create_new_name $base_new_name $sstr $rstr $increment]
							if { [catch {db put $new_name comb region no tree [list l $obj $mat] } ret] } {
								error "Cannot create new object!!!\n$ret"
							}
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
						"regions" {
							set new_name [eval copy_obj $opt_str -i $increment $obj]
							apply_mat -$depth $mat $new_name
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
						"primitives" {
							set new_name [eval copy_obj $opt_str -i $increment $obj]
							apply_mat -$depth $mat $new_name
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
					}
					set increment [expr $increment + $inc]
				}
				$feed_name step
				update idletasks
			}
		}
	}

	if { [llength $group_list] > 0 } {
		if { [catch "g $group_name $group_list" ret] } {
			error "Cannot create group $group_name from list \{${group_list}\}!!!\n$ret"
		}
	}
	draw $group_name
}


proc pattern_sph { args } {
	global M_PI M_PI_2 local2base

	init_vmath
	set usage "pattern_sph \[-top | -regions | -primitives\] \[-g group_name\] \[-s source_string replacement_string\] \
		\[-i tag_number_increment\] \[-center_pat {x y z}\] \[-center_obj {x y z}\] \[-rotaz\] \[-rotel\] \
		\[-naz num_az -daz delta_az | -laz list_of_azimuths\] \
		\[-nel num_el -del delta_el | -lel list_of_elivations\] \
		\[-nr num_r -dr delta_r | -lr list_of_radii\] \
		\[-start_az starting_azimuth \] \[-start_el starting_elevation\] \[-start_r starting_radius\] \
		\[-raz\] \[-rel\] \
		object1 \[object2 object3 ...\]"
	set objs {}
	set start_az 0
	set start_el [expr -$M_PI_2]
	set start_r 0
	set rot_az 0
	set rot_el 0
	set depth "-top"
	set center_pat { 0 0 0 }
	set center_obj { 0 0 0 }
	set group_name ""
	set group_list {}
	set sstr ""
	set rstr ""
	set increment 0
	set inc 1
	set num_az 0
	set num_el 0
	set num_r 0
	set delta_az 0
	set delta_el 0
	set delta_r 0
	set list_az {}
	set list_el {}
	set list_r {}
	set opt_str ""
	set argc [llength $args]
	set index 0
	set feed_name ""
	while { $index < $argc } {
		set opt [lindex $args $index]
		switch -- $opt {
		        "-start_r" {
			    incr index
			    set start_r [lindex $args $index]
			    incr index
			}
			"-top" {
				set depth top
				set opt_str "$opt_str -top"
				incr index
			}
			"-regions" {
				set depth regions
				set opt_str "$opt_str -regions"
				incr index
			}
			"-primitives" {
				set depth primitives
				set opt_str "$opt_str -primitives"
				incr index
			}
			"-g" {
				incr index
				set group_name [lindex $args $index]
				incr index
			}
			"-s" {
				incr index
				set sstr [lindex $args $index]
				incr index
				set rstr [lindex $args $index]
				incr index
				if { [string length $rstr] && [string length $sstr] } {
					set opt_str "$opt_str -s $sstr $rstr"
				}
			}
			"-i" {
				incr index
				set increment [lindex $args $index]
				set inc $increment
				incr index
			}
			"-start_az" {
				incr index
				set start_az [expr [lindex $args $index] * $M_PI / 180.0]
				incr index
			}
			"-start_el" {
				incr index
				set start_el [expr [lindex $args $index] * $M_PI / 180.0]
				incr index
			}
			"-center_pat" {
				incr index
				set center_pat [lindex $args $index]
				incr index
			}
			"-center_obj" {
				incr index
				set center_obj [lindex $args $index]
				incr index
			}
			"-rotaz" {
				set rot_az 1
				incr index
			}
			"-rotel" {
				set rot_el 1
				incr index
			}
			"-naz" {
				incr index
				set num_az [lindex $args $index]
				incr index
			}
			"-nel" {
				incr index
				set num_el [lindex $args $index]
				incr index
			}
			"-nr" {
				incr index
				set num_r [lindex $args $index]
				incr index
			}
			"-daz" {
				incr index
				set delta_az [expr [lindex $args $index] * $M_PI / 180.0]
				incr index
			}

			"-del" {
				incr index
				set delta_el [expr [lindex $args $index] * $M_PI / 180.0]
				incr index
			}
			"-dr" {
				incr index
				set delta_r [lindex $args $index]
				incr index
			}
			"-laz" {
				incr index
				set tmp_list [lindex $args $index ]
			        set list_az {}
				foreach az $tmp_list {
				    lappend list_az [expr {$az * $M_PI / 180.0}]
				}
				incr index
			}
			"-lel" {
				incr index
				set tmp_list [lindex $args $index ]
			        set list_el {}
				foreach el $tmp_list {
				    lappend list_el [expr {$el * $M_PI / 180.0}]
				}
				incr index
			}
			"-lr" {
				incr index
				set list_r [lindex $args $index ]
				incr index
			}
			"-feed_name" {
			    incr index
			    set feed_name [lindex $args $index]
			    incr index
			}
			default {
				set objs [lrange $args $index end]
				set index $argc
			}
		}
	}

	if { [llength $objs] < 1 } {
		error "no objects specified\n$usage"
	}

	if {	[llength $list_az] == 0 && $num_az == 0 &&
		[llength $list_el] == 0 && $num_el == 0 &&
		[llength $list_r] == 0 && $num_r == 0 } {
			error "No azimuth, elevation, or radii provided!!!\n$usage"
	}

	if { $num_az > 1 && $delta_az == 0 } {
		error "No azimuth delta provided!!!\n$usage"
	}
	if { $num_el > 1 && $delta_el == 0 } {
		error "No elevation delta provided!!!\n$usage"
	}
	if { $num_r > 1 && $delta_r == 0 } {
		error "No radius delta provided!!!\n$usage"
	}

	if { $num_az } {
		set list_az {}
		for { set index 0 } { $index < $num_az } { incr index } {
			lappend list_az [expr $start_az + $delta_az * $index]
		}
	} else {
		set num_az [llength $list_az]
	}
	if { $num_el } {
		set list_el {}
		for { set index 0 } { $index < $num_el } { incr index } {
			lappend list_el [expr $start_el + $delta_el * $index]
		}
	} else {
		set num_el [llength $list_el]
	}
	if { $num_r } {
		set list_r {}
		for { set index 0 } { $index < $num_r } { incr index } {
			lappend list_r [expr $start_r + $delta_r * $index]
		}
	} else {
		set num_r [llength $list_r]
	}

	if { $num_az == 0 } {
		set list_az { 0 }
	}
	if { $num_el == 0 } {
		set list_el { 0 }
	}
	if { $num_r == 0 } {
		set list_r { $start_r }
	}
	set rlen [llength $list_r]
	set azlen [llength $list_az]
	set pole_count 0
	set ellen [llength $list_el]
	foreach el $list_el {
	    set abs_el $el
	    if { $abs_el < 0.0 } {
		set abs_el [expr {-$abs_el}]
	    }
	    set diff [expr {$M_PI_2 - $abs_el}]
	    if { $diff < 0.001 } {
		incr pole_count
	    }
	}

        # convert to base units
        for { set i 0 } { $i < $num_r } { incr i } {
	    set list_r [lreplace $list_r $i $i [expr [lindex $list_r $i] * $local2base]]
	}

        set center_pat [vscale $center_pat $local2base]
        set center_obj [vscale $center_obj $local2base]

	$feed_name configure -steps [expr {$rlen * ($ellen - $pole_count) * $azlen + $pole_count * $rlen}]
	set r_index 0
	foreach radius $list_r {
		incr r_index
		set el_index 0
		foreach el $list_el {
			incr el_index
			set az_index 0
			foreach az $list_az {
				incr az_index
				set mat1 [mat_deltas_vec [mat_idn] [vreverse $center_obj]]
				if { $rot_az && $rot_el } {
					set mat2 [mat_mul [mat_ae [expr $az * 180.0 / $M_PI] [expr $el * 180.0 / $M_PI]] $mat1]
				} elseif { $rot_az } {
					set mat2 [mat_mul [mat_ae [expr $az * 180.0 / $M_PI] 0] $mat1]
				} elseif { $rot_el } {
					set mat2 [mat_mul [mat_ae 0 [expr $el * 180.0 / $M_PI]] $mat1]
				} else {
					set mat2 $mat1
				}
				set r_vec "[expr $radius * cos( $az ) * cos( $el )] [expr $radius * sin( $az ) * cos( $el )] [expr $radius * sin( $el )]"
				set mat1 [mat_deltas_vec [mat_idn] [vadd2 $r_vec $center_pat]]
				set mat [mat_mul $mat1 $mat2]
				foreach obj $objs {
					switch $depth {
						"top" {
							set base_new_name ${obj}_${az_index}_${el_index}_${r_index}
							set new_name [create_new_name $base_new_name $sstr $rstr $increment]
							if { [catch {db put $new_name comb region no tree [list l $obj $mat] } ret] } {
								error "Cannot create new object!!!\n$ret"
							}
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
						"regions" {
							set new_name [eval copy_obj $opt_str -i $increment $obj]
							apply_mat -$depth $mat $new_name
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
						"primitives" {
							set new_name [eval copy_obj $opt_str -i $increment $obj]
							apply_mat -$depth $mat $new_name
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
					}
					set increment [expr $increment + $inc]
				}
				$feed_name step
				update idletasks
				set abs_el $el
				if { $abs_el < 0.0 } {
				    set abs_el [expr -$abs_el]
				}
				set diff [expr {$M_PI_2 - $abs_el}]
				if { $diff < 0.001 } break
			}
		}
	}

	if { [llength $group_list] > 0 } {
		if { [catch "g $group_name $group_list" ret] } {
			error "Cannot create group $group_name from list \{${group_list}\}!!!\n$ret"
		}
	}
	
	draw $group_name
}


proc pattern_cyl { args } {
	global M_PI M_PI_2 local2base

	init_vmath

	set usage "pattern_cyl \[-top | -region | -primitives\] \[-g group_name]\ \[-s source_string replacemrnt_string\] \
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
	set depth "-top"
	set group_name ""
	set group_list {}
	set sstr ""
	set rstr ""
	set increment 0
	set inc 1
	set start_az 0
	set start_az_dir { 1 0 0 }
	set start_r 0
	set start_h 0
	set num_az 0
	set num_r 0
	set num_h 0
	set delta_az 0
	set delta_r 0
	set delta_h 0
	set list_az {}
	set list_r {}
	set list_h {}
	set center_base { 0 0 0 }
	set center_obj { 0 0 0 }
	set height_dir { 0 0 1 }
	set depth "top"
	set opt_str ""
	set argc [llength $args]
	set index 0
	while { $index < $argc } {
		set opt [lindex $args $index]
		switch -- $opt {
			"-top" {
				set depth top
				set opt_str "$opt_str -top"
				incr index
			}
			"-regions" {
				set depth regions
				set opt_str "$opt_str -regions"
				incr index
			}
			"-primitives" {
				set depth primitives
				set opt_str "$opt_str -primitives"
				incr index
			}
			"-g" {
				incr index
				set group_name [lindex $args $index]
				incr index
			}
			"-s" {
				incr index
				set sstr [lindex $args $index]
				incr index
				set rstr [lindex $args $index]
				incr index
				if { [string length $rstr] && [string length $sstr] } {
					set opt_str "$opt_str -s $sstr $rstr"
				}
			}
			"-i" {
				incr index
				set increment [lindex $args $index]
				set inc $increment
				incr index
			}
			"-start_az" {
				incr index
				set start_az [expr [lindex $args $index] * $M_PI / 180.0]
				incr index
			}
			"-start_az_dir" {
				incr index
				set start_az_dir [lindex $args $index]
				incr index
			}
			"-rot" {
				set do_rot 1
				incr index
			}
			"-center_obj" {
				incr index
				set center_obj [lindex $args $index]
				incr index
			}
			"-center_base" {
				incr index
				set center_base [lindex $args $index]
				incr index
			}
			"-height_dir" {
				incr index
				set height_dir [lindex $args $index]
				incr index
			}
			"-naz" {
				incr index
				set num_az [lindex $args $index]
				incr index
			}
			"-daz" {
				incr index
				set delta_az [expr [lindex $args $index] * $M_PI / 180.0]
				incr index
			}
			"-laz" {
				incr index
				set tmp_list [lindex $args $index ]
			        set list_az {}
				foreach az $tmp_list {
				    lappend list_az [expr {$az * $M_PI / 180.0}]
				}
				incr index
			}
			"-nr" {
				incr index
				set num_r [lindex $args $index]
				incr index
			}
			"-dr" {
				incr index
				set delta_r [lindex $args $index]
				incr index
			}
			"-nh" {
				incr index
				set num_h [lindex $args $index]
				incr index
			}
			"-dh" {
				incr index
				set delta_h [lindex $args $index]
				incr index
			}
			"-lh" {
				incr index
				set list_h [lindex $args $index]
				incr index
			}
			"-sr" {
				incr index
				set start_r [lindex $args $index]
				incr index
			}
			"-sh" {
			    incr index
			    set start_h [lindex $args $index]
			    incr index
			}
			"-feed_name" {
			    incr index
			    set feed_name [lindex $args $index]
			    incr index
			}
			default {
				set objs [lrange $args $index end]
				set index $argc
			}
		}
	}

	if { [llength $objs] < 1 } {
		error "no objects specified\n$usage"
	}

	if { 	[llength $list_az] == 0 && $num_az == 0 &&
		[llength $list_r] == 0 && $num_r == 0 &&
		[llength $list_h] == 0 && $num_h == 0 } {

			error "No azimuth, radii, or heights provided!!!!\n$usage"
	}

	if { $num_az > 1 && $delta_az == 0 } {
		error "No azimuth delta provided!!!\n$usage"
	}
	if { $num_r > 1 && $delta_r == 0 } {
		error "No radius delta provided!!!\n$usage"
	}
	if { $num_h > 1 && $delta_h == 0 } {
		error "No height delta provided!!!\n$usage"
	}

	eval set tmp_az [magnitude $start_az_dir]
	if { [expr abs($tmp_az)] < 0.001 } {
	    error "azimuth direction vector is too small!!!\n$usage"
	} else {
	    set tmp_az [expr 1.0 / $tmp_az]
	    set start_az_dir [vscale $start_az_dir $tmp_az]
	}

	eval set tmp_ht [magnitude $height_dir]
	if { [expr abs($tmp_ht)] < 0.001 } {
	    error "height direction vector is too small!!!\n$usage"
	} else {
	    set tmp_ht [expr 1.0 / $tmp_ht]
	    set height_dir [vscale $height_dir $tmp_ht]
	}

	if { [expr abs([vdot $start_az_dir $height_dir])] > 0.001 } {
		error "azimuth and height direction must be perpendicular!!!\n$usage"
	} else {
		set az_dir2 [vcross $height_dir $start_az_dir]
	}

	if { $num_az } {
		set list_az {}
		for { set index 0 } {$index < $num_az} { incr index } {
			lappend list_az [expr $start_az + $delta_az * $index]
		}
	} else {
		set num_az [llength $list_az]
	}

	if { $num_r } {
		set list_r {}
		for { set index 0 } {$index < $num_r} { incr index } {
			lappend list_r [expr $start_r + $delta_r * $index]
		}
	} else {
		set num_r [llength $list_r]
	}

	if { $num_h } {
		set list_h {}
		for { set index 0 } {$index < $num_h} { incr index } {
			lappend list_h [expr $start_h + $delta_h * $index]
		}
	} else {
		set num_h [llength $list_h]
	}

	if { $num_az == 0 } {
		set list_az { 0 }
	}
	if { $num_h == 0 } {
		set list_h { 0 }
	}
	if { $num_r == 0 } {
		set list_r { 0 }
	}

	set rlen [llength $list_r]
	set hlen [llength $list_h]
	set azlen [llength $list_az]
	$feed_name configure -steps [expr $rlen * $hlen * $azlen]

        # convert to base units
        for { set i 0 } { $i < $num_h } { incr i } {
	    set list_h [lreplace $list_h $i $i [expr [lindex $list_h $i] * $local2base]]
	}
        for { set i 0 } { $i < $num_r } { incr i } {
	    set list_r [lreplace $list_r $i $i [expr [lindex $list_r $i] * $local2base]]
	}
        set center_obj [vscale $center_obj $local2base]
        set center_base [vscale $center_base $local2base]

	set r_index 0
	foreach radius $list_r {
		incr r_index
		set h_index 0
		foreach height $list_h {
			incr h_index
			set az_index 0
			foreach az $list_az {
				incr az_index
				set mat1 [mat_deltas_vec [mat_idn] [vreverse $center_obj]]
				if { $do_rot } {
					set mat2 [mat_mul [mat_arb_rot [vreverse $center_obj] $height_dir $az] $mat1]
				} else {
					set mat2 $mat1
				}
				set r_vec_x [expr $radius * cos( $az )]
				set r_vec_y [expr $radius * sin( $az )]
				set r_vec [vblend $r_vec_x $start_az_dir $r_vec_y $az_dir2]
				set r_vec [vjoin1 $r_vec $height $height_dir]
				set mat1 [mat_deltas_vec [mat_idn] [vadd2 $r_vec $center_base]]
				set mat [mat_mul $mat1 $mat2]

				foreach obj $objs {
					switch $depth {
						"top" {
							set base_new_name ${obj}_${r_index}_${h_index}_${az_index}
							set new_name [create_new_name $base_new_name $sstr $rstr $increment]
							if { [catch {db put $new_name comb region no tree [list l $obj $mat] } ret] } {
								error "Cannot create new object!!!\n$ret"
							}
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
						"regions" {
							set new_name [eval copy_obj $opt_str -i $increment $obj]
							apply_mat -$depth $mat $new_name
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
						"primitives" {
							set new_name [eval copy_obj $opt_str -i $increment $obj]
							apply_mat -$depth $mat $new_name
							if { $group_name != "" } {
								lappend group_list $new_name
							}
						}
					}
					set increment [expr $increment + $inc]
				}
				$feed_name step
				update idletasks
			}
		}
	}

	if { [llength $group_list] > 0 } {
		if { [catch "g $group_name $group_list" ret] } {
			error "Cannot create group $group_name from list \{${group_list}\}!!!\n$ret"
		}
	}
	
	draw $group_name
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
