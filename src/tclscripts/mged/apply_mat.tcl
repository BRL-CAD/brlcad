#                   A P P L Y _ M A T . T C L
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
#		A P P L Y _ M A T . T C L
#
# Procedures to apply matrices to objects in MGED
#
# Author - John R. Anderson
#

#
#	A P P L Y _ M A T _ T O _ R E G I O N S
#
# procedure to apply the provided matrix to the provided tree
# recurses until a region is encountered, where the matrix is applied at each leaf arc of the region.
# The matrix is not applied to any primitive encountered above the region level.
#
# Returns a new tree with the matrix applied

proc apply_mat_to_regions { tree mat } {
	set op [lindex $tree 0]
	switch -- $op {
		l
		{
			set leaf [lindex $tree 1]
			set new_leaf [db get $leaf]
			set type [lindex $new_leaf 0]
			if { $type != "comb" } {
				puts "WARNING: encountered primitive ($leaf) above region level!!!, ignoring"
				return $tree
			}
			if { [llength $tree] == 3 } {
				set old_mat [lindex $tree 2]
				set new_mat [mat_mul $mat $old_mat]
			} else {
				set new_mat $mat
			}
			set index [lsearch -exact $new_leaf "region"]
			if { $index < 0 } {
				error "ERROR: 'region' attribute missing from combination $leaf"
			}
			incr index
			set region [lindex $new_leaf $index]
			set index [lsearch -exact $new_leaf "tree"]
			if { $index < 0 } {
				error "ERROR: no tree for combination $leaf"
			}
			incr index
			set sub_tree [lindex $new_leaf $index]
			if { $region == "yes" } {
				set new_tree [apply_mat_comb $sub_tree $new_mat]
			} else {
				set new_tree [apply_mat_to_regions $sub_tree $new_mat]
			}
			set new_leaf [lreplace $new_leaf $index $index $new_tree]
			set command [concat db adjust $leaf [lrange $new_leaf 1 end]]
			if { [catch $command ret] } {
				error "ERROR: 'db adjust' failed for combination $leaf\n\t$ret"
			}
			return [list l $leaf]
		}
		- -
		+ -
		u -
		n
		{
			set left [lindex $tree 1]
			set right [lindex $tree 2]
			set new_left [apply_mat_to_regions $left $mat]
			set new_right [apply_mat_to_regions $right $mat]
			return [list $op $new_left $new_right]
		}
	}
}

#
#	A P P L Y _ M A T _ T O _ C O M B
#
# Apply provided matrix to the provided tree. This routine does not descend beyond the specified tree.
# The matrix is applied at each leaf arc in the tree.
#
# Returns the modified tree

proc apply_mat_comb { tree mat } {
	set op [lindex $tree 0]
	switch -- $op {
		l
		{
			set leaf [lindex $tree 1]
			if { [llength $tree] == 3 } {
				set old_mat [lindex $tree 2]
				set new_mat [mat_mul $mat $old_mat]
			} else {
				set new_mat $mat
			}
			return [list "l" $leaf $new_mat]
		}
		- -
		+ -
		u -
		n
		{
			set left [lindex $tree 1]
			set right [lindex $tree 2]
			set new_left [apply_mat_comb $left $mat]
			set new_right [apply_mat_comb $right $mat]
			return [list $op $new_left $new_right]
		}
	}
}

#
#	A P P L Y _ M A T
#
# This procedure applies the provided matrix to the list of objects specified.
# One of the "-primitives", "-regions", or "-top" options may be provided to specify
# where the matrix is to be applied (the default is "-top").
#
# If an object specified is a primitive, the matrix is pushed into the primitive.
#
# If "-primitives" is specified, then the matrix is applied at the top level object
# and "xpush" is used to push the changes to the primitive level.
#
# If "-regions" is specified, then the matrix is applied to each leaf in the first regions
# encountered as the tree is descended (matrices along the way are incorporated).
#
# If "-top" is specified (or no options are specified), then the matrix is applied to each
# leaf arc of the spcified objects.
#
# Returns TCL_ERROR or TCL_OK

proc apply_mat { args } {
	set usage "Usage:\n\tapply_mat \[-primitives | -regions | -top\] matrix object1 \[object2 object3 ...\]"
	set argc [llength $args]
	if { $argc < 1 } {
		error $usage
	}
	set mat {}
	set objs {}
	set depth "top"
	set index 0
	while { $index < $argc } {
		set opt [lindex $args $index]
		switch -- $opt {
			"-primitives" {
				set depth "primitives"
				incr index
			}
			"-regions" {
				set depth "regions"
				incr index
			}
			"-top" {
				set depth "top"
				incr index
			}
			default {
				break
			}
		}
	}
	if { $index >= $argc } {
		error "No matrix or objects specified!!!\n$usage"
	}
	set mat [lindex $args $index]
	incr index
	if { $index >= $argc } {
		error "No objects specified!!!!\n$usage"
	}

	foreach obj [lrange $args $index end] {
		if { [catch "db get $obj" obj_db] } {
			puts $obj_db
			puts "WARNING: $obj does not exist, ignoring!!"
			continue
		}

		set type [lindex $obj_db 0]
		if { $type != "comb" } {
			# this is a primitive, so just apply the matrix
			# using a temporary combination and a "push"
			set tmp [make_name ____]
			db put $tmp comb region no tree [list l $obj $mat]
			push $tmp
			kill $tmp
			continue
		}

		# The object is a combination
		set indx [lsearch -exact $obj_db tree]
		if { $indx < 0 } {
			error "$obj does not have a Boolean tree!!!!"
		}
		incr indx
		set obj_tree [lindex $obj_db $indx]

		switch $depth {
			"primitives" {
				set new_tree [apply_mat_comb $obj_tree $mat]
				db adjust $obj tree $new_tree
				xpush $obj
			}
			"regions" {
				set new_tree [apply_mat_to_regions $obj_tree $mat]
				db adjust $obj tree $new_tree
			}
			"top" {
				set new_tree [apply_mat_comb $obj_tree $mat]
				db adjust $obj tree $new_tree
			}
		}
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
