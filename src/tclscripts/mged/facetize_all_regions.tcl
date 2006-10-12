#
#  Expand a single boolean node in a tree
#  to create new object called $name
#
proc expand_bool_node {name tree s} {

    puts "$s expand_bool_node $name"

    if { [catch {get $name} dbrec] } {
	
    } else {
	# exists
	puts "$name already exists... skipping\n$dbrec"
	return
    }

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
    eval $cmd
}

#
#  Create database objects for all boolean nodes in a combination tree
#
#  Options: -c  don't modify the given combination, copy it and expand that one.
#               This allows the original combination record to remain intact.
#
# return 0 expanded all
#	1 one or more did not expand (already single boolean)
#
#
proc expand_comb_tree {args} {
    set return_val 0

    if {[llength $args] == 0} {
	puts "usage: expand_comb_tree \[-c\] regname ..."
    }

    if {[string compare [lindex $args 0] "-c"] == 0} {
	set duplicate 1
    } else {
	set duplicate 0
    }

    foreach r [lrange $args $duplicate end] {

	puts "expand $r"

	if {[catch {get $r tree} tree]} {
	    puts "Error: could not get tree for $r.  Skipping"
	    continue
	}

	if { [lindex $tree 0] == "l" } {
	    # a combination of a single leaf?
	    puts "single leaf [lindex $tree 1]"
	    continue
	}

	set left [lindex $tree 1]
	set right [lindex $tree 2]

	# check to see if we are already a single boolean op
	if {[lindex $left 0] == "l" && [lindex $right 0] == "l"} {
	    puts "just 2 leaf nodes [lindex $left 1] [lindex $right 1]"} {
	    set return_val 1
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
	    # create new region/combnation
	    cp $r ${r}_xpand
	    db adjust ${r}_xpand tree $tree
	} else {
	    db adjust $r tree $tree
	}
    }

    return $return_val
}

#
#
# return 
# 0 success
# 1 failure
# 2 previously failed
#
proc attempt_facetize {reg reg_fail fail} {

    # check to see if the bot already exists
    # or previously failed to facetize
    if { ! [catch {db get $reg.bot} regval]} {
	# already exists
	puts " ---- $reg.bot exists $regval"
	return 0
    }

    if {[lsearch $reg_fail $reg] >= 0} {
	# in list of previous failures
	puts " ---- $reg.bot previously failed to facetize"
	return 2
    }


    # not in the list of previous failures.  Facetize it

    puts "---- Attempting to facetize $reg"

    # record the region we are about to attempt as failed
    # if we dump core, it won't be attempted again
    set pos_fail [tell $fail]
    set buf " $reg"
    puts -nonewline $fail $buf
    flush $fail

    if {[catch {facetize $reg.bot $reg} res]} {
	# facetize failed
	puts "---- $reg failed to facetize"
	return 1
    } else {
	puts "---- $reg now done"

	# remove $reg from list of failed
	seek $fail $pos_fail
	set erase [format "%*s" [string length $buf] " "]
	puts -nonewline $fail $erase
	flush $fail
	seek $fail $pos_fail
    }
    
    return 0
}

#
#  Facetize a sub-portion of a combination that failed to fully facetize
#
#
proc facetize_failed_comb {reg reg_fail fail} {

    puts "\n----- facetizing_failed_comb $reg -----"
    if { [catch {db get $reg tree} tree] } {
	puts "Could not get tree from $reg"
	return
    }

    # get children
    if { [lindex [lindex $tree 1] 0] != "l" } {
	puts "left is not a leaf"
	set comb [lindex [lindex $tree 1] 1]
	expand_comb_tree $reg
	facetize_failed_comb $reg $reg_fail $fail
	return
    } else {
	set left [lindex [lindex $tree 1] 1]
    }
    if { [lindex [lindex $tree 2] 0] != "l" } {
	puts "right is not a leaf"
	set comb [lindex [lindex $tree 1] 1]
	expand_comb_tree $reg
	facetize_failed_comb $reg $reg_fail $fail
	return
    } else {
	set right [lindex [lindex $tree 2] 1]
    }

    foreach child [list $left $right ] {

	set cond [attempt_facetize $child $reg_fail $fail]
	switch -exact $cond {
	    0 { # succeeded, delete sub-tree, modify comb to contain facetized
		puts "sub-child $child facetized"
	    }
	    1 {
		# failed, try to facetize the sub-tree parts
		if {[facetize_failed_comb $child $reg_fail $fail]} {
		    puts "failed child $child"
		} else {
		    puts "succeeded child $child"
		}
	    }
	    2 { # previously failed
		puts "sub-child $child previously failed"
		break
	    }
	    default {
		puts "sub-child $child gave cond $cond"
		break
	    }
	}
    }
}

#
#  Attempt to facetize each region in a database
#  This proc is usually called from facetall.sh
#  which manages re-starting mged when we get a 
#  core dump
#
proc facetize_all_regions {filename} {
    global glob_compat_mode

    set glob_compat_mode 0
    set reg_list [ls -r]

    puts "\nStarting to facetize regions\n\n"

    # open list of files that previously failed to convert
    # get list
    if {[file exists $filename]} {
	set fail [open $filename r]
	set reg_fail [eval list [read $fail]]
	close $fail
    } else {
	set reg_fail ""
    }

    set fail [open $filename a]

    foreach reg $reg_list {
	puts "------ <$reg> ------"
	switch [attempt_facetize $reg $reg_fail $fail] {
	    0 { 
		#succeeded
		puts "$reg ok"
	    }
	    1 {
		# failed

		puts "expanding $reg"
		if { [expand_comb_tree -c $reg] } {
		    puts "sub-facetizing $reg"
#		    facetize_failed_comb ${reg} $reg_fail $fail
		} else {
		    puts "sub-facetizing ${reg}_xpand"
#		    facetize_failed_comb ${reg}_xpand $reg_fail $fail
		}
	    }
	    2 { # previous failure
		puts "$reg failed before"
	    }
	    default { puts "what?" }
	}
    }
    set glob_compat_mode 1
    return
}
