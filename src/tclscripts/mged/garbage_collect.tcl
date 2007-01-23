#             G A R B A G E _ C O L L E C T . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
#	This routine uses the MGED "keep" command to create a copy of the current
#	database in a temporary file (thus eliminating free blocks), then overwriting
#	the current database with the copy.

proc garbage_collect {} {
	#	We need to turn off autosize, so this lets us access it
	global autosize

	# get the name of the current database
	set cur_file [opendb]
        set cur_dir [file dirname $cur_file]
        set cur_tail [file tail $cur_file]
        set old_size [file size $cur_file]

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
	set tmp_file /usr/tmp/[pid]_$cur_tail
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
	catch {[exec mged -c $tmp_file tops]} new_tops
	if { [regexp "\(NOTICE:.*is BRL-CAD v5 format.\)\(.*\)" $new_tops matchvar notice real_tops] } {
	    set new_tops $real_tops
	}
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

	set new_size [file size $cur_file]

	if { $new_size < $old_size } {
	    puts "Database was $old_size bytes, is now $new_size bytes (saved [expr $old_size - $new_size] bytes) "
	} elseif { $new_size == $old_size } {
	    puts "Database size did not change ($old_size bytes)"
	} else {
	    puts "Database got bigger!!! this should not happen"
	    puts "was $old_size bytes, is now $new_size bytes (difference is [expr $new_size - $old_size] bytes)"
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
