#	This routine uses the MGED "keep" command to create a copy of the current
#	database in a temporary file (thus eliminating free blocks), then overwriting
#	the current database with the copy.

proc garbage_collect {} {
	#	We need to turn off autosize, so this lets us access it
	global autosize

	# get the name of the current database
	set cur_file [opendb]

	# save the current setting of autosize
	set save_autosize $autosize

	# get a list of the currently displayed objects
	set obj_list [concat [who]]

	# get a list of all top level objects in the database
	set top_list [tops]
	set old_list_len [llength $top_list]

	# save the database title (keep changes it)
	set tmp_title [title]

	# eliminate the excess newline that the "title" command appends
	set save_title [string range $tmp_title 0 [expr [string length $tmp_title] - 2]]

	# create a temporary file
	set tmp_file /usr/tmp/[pid]_$cur_file
	exec /bin/rm -f $tmp_file

	# massage list of top level objects to eliminate suffixes ("/" and "/R")
	set fixed_tops {}
	foreach name $top_list {
		set len [string length $name]
		if {[string index $name [expr $len - 1]] == "/" } {
			lappend fixed_tops [string range $name 0 [expr $len - 2]]
		} elseif {[string compare [string range $name [expr $len - 2] end] "/R"] == 0 } {
			lappend fixed_tops [string range $name 0 [expr $len - 3]]
		} else {
			lappend fixed_tops $name
		}
	}

	# use "keep" to create a clean database
	puts "Keeping clean database to $tmp_file"
	eval keep $tmp_file $fixed_tops

	# make sure the new copy is correct (first make sure we have the same number
	# of top level ovjects
	puts "verifying $tmp_file"
	catch {[exec /m/cad/.mged.6d/mged -c $tmp_file tops]} new_tops
	set new_list_len [llength $new_tops]
	if { $new_list_len != $old_list_len } {
		puts "$tmp_file is corrupt, garbage collection aborted!!!!!"
		puts "Result of 'tops' was:\n$new_tops"
		puts "\n\n'tops' should have produced:\n$top_list"
		exec /bin/rm -f $tmp_file
		return
	}

	# now compare the name of each top level object
	for { set index 0 } { $index < $new_list_len } { incr index } {
		if { [string compare [lindex $new_tops $index] [lindex $top_list $index] ] } {
			puts "$tmp_file is corrupt, garbage collection aborted!!!!"
			puts "Result of 'tops' was:\n$new_tops"
			puts "\n\n'tops' should have produced:\n$top_list"
			exec /bin/rm -f $tmp_file
			return
		}
	}

	# turn off autosize so that we can duplicate the current view later
	set autosize 0

	# looks like a good copy, overwrite the current database
	puts "copying $tmp_file to $cur_file"
	exec cp $tmp_file  $cur_file

	# delete the temporary file
	exec /bin/rm -f $tmp_file

	# reopen the overwritten database
	opendb $cur_file

	# put back up anything that was being displayed
	if { [llength $obj_list] > 0 } {
		eval e $obj_list
	}

	# repair the title
	title $save_title

	# restore the autosize setting
	set autosize $save_autosize
}
