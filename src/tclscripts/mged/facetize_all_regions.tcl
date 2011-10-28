#        F A C E T I Z E _ A L L _ R E G I O N S . T C L
# BRL-CAD
#
# Copyright (c) 2007-2011 United States Government as represented by
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
# return
# -1 bot already exists
# 0 success
# 1 failure
# 2 previously failed
#
proc attempt_facetize {reg reg_fail fail} {

    # check to see if the bot already exists
    # or previously failed to facetize
    if { [ exists $reg.bot ] } {
	# already exists
	puts " ---- $reg.bot already exists"
	return 3
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
    }

    puts "---- $reg now done"

    # remove $reg from list of failed
    seek $fail $pos_fail
    set erase [format "%*s" [string length $buf] " "]
    puts -nonewline $fail $erase
    flush $fail
    seek $fail $pos_fail

    return 0
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
	switch -exact [attempt_facetize $reg $reg_fail $fail] {
	    0 {
		#succeeded
		puts "$reg ok"
	    }
	    1 {
		# failed

		puts "expanding $reg"
		expand_comb ${reg}_ $reg 0
		puts "sub-facetizing $reg"
		#		facetize_failed_comb ${reg}_ $reg_fail $fail
	    }
	    2 { # previous failure
		puts "$reg failed before"
	    }
	    3 { # previously succeeded
		puts "$reg previously succeeded"
	    }
	    default { puts "what?" }
	}
    }
    set glob_compat_mode 1
    return
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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
