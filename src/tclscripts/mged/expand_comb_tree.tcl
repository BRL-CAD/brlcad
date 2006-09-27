#
#  Expand a single boolean node in a tree
#  Attempt to facetize the result
#
proc expand_bool_node {name tree s} {

    set left [lindex $tree 1]
    set right [lindex $tree 2]

    if {[lindex $left 0] != "l"} {
	# need to expand left
	set newname ${name}l
	expand_bool_node $newname $left "$s  "
	set left "l $newname"
    }

    if {[lindex $right 0] != "l"} {
	# need to expand right
	set newname ${name}r
	expand_bool_node $newname $right "$s  "
	set right "l $newname"
    }


    set tree [list [lindex $tree 0] $left $right]
    set cmd "db put $name comb tree [list $tree]"
    # puts "$cmd"
    eval $cmd
}

#
#  Create database objects for all boolean nodes in a combination tree
#
proc expand_comb_tree {args} {

    if {[llength $args] == 0} {
	puts "usage: expand_comb_tree \[-c\] regname ..."
    }

    if {[string compare [lindex $args 0] "-c"] == 0} {
	set duplicate 1
    } else {
	set duplicate 0
    }

    foreach r [lrange $args $duplicate end] {

	if {[catch {get $r tree} tree]} {
	    puts "Error: could not get tree for $r.  Skipping"
	    continue
	}

	set left [lindex $tree 1]
	set right [lindex $tree 2]

	# check to see if we are already a single boolean op
	if {[lindex $left 0] == "l" && [lindex $right 0] == "l"} {
	    return
	}
	
	# at lease one branch is deaper than a leaf

	if {[lindex $left 0] != "l"} {
	    expand_bool_node ${r}_xpand_l $left "  "
	    set left "l ${r}_xpand_l"
	}
	if {[lindex $right 0] != "l"} {
	    expand_bool_node  ${r}_xpand_r $right "  "
	    set right "l ${r}_xpand_r"
	}

	set tree "[lindex $tree 0] [list $left] [list $right]"
	if {$duplicate} {
	    # create new region
	    cp $r ${r}_xpand
	    db adjust ${r}_xpand tree $tree
	} else {
	    db adjust $r tree $tree
	}
    }
}