#                 E X P A N D _ C O M B . T C L
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
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
#  Expand the tree from a combination record into 
#  indivudually named combinations
#
#  Typically called from expand_comb
#
proc expand_tree {prefix tree regflag} {
    # are we at a leaf in the tree?
    puts "expand_tree $prefix $regflag"

    set op [lindex $tree 0]
    
    if { $op == "l" } {
	set leaf_obj [lindex $tree 1]

	if {[catch {db get_type $leaf_obj} dbtype]} {
	    puts "could not get type for $leaf_obj"
	    return
	}
	if {$dbtype == "comb"} {
	    set leaf_obj [lindex $tree 1]
	    expand_comb ${prefix}_${leaf_obj} $leaf_obj

	    set cmd [list put $prefix comb region $regflag tree]
	    if {[llength $tree] == 2} {
		lappend cmd [list l ${prefix}_${leaf_obj} ]
	    } else {
		lappend cmd [list l ${prefix}_${leaf_obj} [lindex $tree 2] ]
	    }
	    puts $cmd
	    eval $cmd
	} else {
	    set cmd [list put $prefix comb region 0 tree $tree]
	    puts $cmd
	    eval $cmd
	}

	return
    }

    set left [lindex $tree 1]
    expand_tree ${prefix}l $left no

    set right [lindex $tree 2]
    expand_tree ${prefix}r $right no

    set cmd [list put $prefix comb region $regflag tree [list $op [list l ${prefix}l] [list l ${prefix}r]]]

    eval $cmd
}


#
# Expand a combination (region/combination/group) so that all nodes
# in the Boolean tree are named.  The primitives will not be duplicated
# but the rest of the DAG will be expanded and named.  Sub-elements will be
# named things like:
#	${new_comb}lrlll
#
# When sub-regions or combinations are encountered, their name will be appended
# to the name of the tree objects.  This helps identify where these sub-combinations
# were located in the original tree.
#
# Usage:
#	expand_comb new_comb old_comb
#
proc expand_comb {args} {
    if {[llength $args] != 2} {
	puts "Usage: expand_comb new_comb old_comb"
	return
    }
    set prefix [lindex $args 0]
    set comb [lindex $args 1]

    if {[catch {db get_type $comb} dbtype]} {
	puts " cant get type for $comb"
	return
    }
    if {$dbtype != "comb"} {
	puts "$comb not a comb"
	return
    }

    if {[catch {get $comb tree} tree]} {
	puts "get tree $r failed"
	return
    }
    
    expand_tree $prefix $tree [get $comb region]

    return
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
