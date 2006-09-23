proc facetize_failed_comb {reg} {

    if {[catch {db get $reg tree} tree]} {
	puts "Could not get tree from $reg"
	return
    }

    set left [lindex [lindex $tree 1] 1]
    set right [lindex [lindex $tree 2] 1]

    if {[catch {facetize $left.bot $left} res]} {
	# facetize failed
	facetize_failed_comb $left
    } else {
	# facetize succeeded
    }

    if {[catch {facetize $right.bot $right} res]} {
	# facetize failed
	facetize_failed_comb $right
    } else {
	# facetize succeeded
    }

}


proc facetize_all_regions {} {
    set glob_compat_mode 0
    set reg_list [ls -r]

    puts "\n\n\nStarting to facetize regions\n\n\n"

    # open list of files that previously failed to convert
    # get list
    if {[file exists regions.fail]} {
	set fail [open regions.fail r]
	set reg_fail [eval list [read $fail]]
	close $fail
    } else {
	set reg_fail ""
    }

    set fail [open regions.fail a]

    foreach reg $reg_list {
	puts "------ <$reg> ------"


	# check to see if the bot already exists
	if {[catch {db get $reg.bot} regval]} {

	    # bot does not exist

	    # check to see if it previously failed
	    if { [lsearch $reg_fail $reg] < 0} {
		# not in the list of previous failures.  Facetize it

		puts "facetizing $reg"

		# record the region we are about to attempt as failed
		set pos_fail [tell $fail]
		set buf " $reg"
		puts -nonewline $fail $buf
		flush $fail

		if {[catch {facetize $reg.bot $reg} res]} {
		    # facetize failed
		    puts "$reg failed"
		} else {
		    #facetize succeeded
		    puts "facetized"

		    # remove $reg from list of failed
		    seek $fail $pos_fail
		    set erase [format "%*s" [string length $buf] " "]
		    puts -nonewline $fail $erase
		    flush $fail
		    seek $fail $pos_fail
		}
	    } else {
		puts "previously failed on $reg"
	    }
	} else {
	    puts "   exists"
	}
    }
}
