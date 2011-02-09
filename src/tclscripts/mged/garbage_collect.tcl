#             G A R B A G E _ C O L L E C T . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
# This routine uses the MGED "keep" command to create a copy of the
# current database in a temporary file (thus eliminating free blocks),
# then replacing the current database with the new copy.
#
###

proc garbage_collect { args } {
    #	We need to turn off autosize, so this lets us access it
    global autosize

    if { $args == "-h" || $args == "-help" } {
	puts "garbage_collect reclaims any available free space in the currently"
	puts "open geometry database file.  As objects are deleted and created,"
	puts "BRL-CAD geometry files can become fragmented and require cleanup."
	puts ""
	puts "This command will automatically reclaim any available space by"
	puts "keeping all existing top-level objects to a new file and, after"
	puts "verifying that all objects were successfully saved, will replace"
	puts "the currently open geometry database with the new file."
	puts ""
	puts "DUE TO THE POTENTIAL FOR DATA CORRUPTION, PLEASE MANUALLY BACK UP"
	puts "YOUR GEOMETRY FILE BEFORE RUNNING 'garbage_collect'."
	return
    }

    # needs to report usage if the action was not confirmed
    if { $args != "-c" && $args != "-confirm" } {
	puts "Usage: garbage_collect \[-c|-confirm\] \[-h|-help\]"
	return
    }

    # get the name of the current database
    if { [opendb] == "" } {
	puts "ERROR: Database not open!"
	puts "Aborting garbage collect."
	return
    }
    set filename [opendb]
    set old_dir [file dirname $filename]
    set old_tail [file tail $filename]
    set old_size [file size $filename]
    puts "Reclaiming available free space in $filename"

    # save the current setting of autosize
    set old_autosize $autosize

    # save the current view
    set view_size [view size]
    set view_quat [view quat]
    set view_center [view center]

    # get a list of the currently displayed objects
    set obj_list [concat [who]]

    # get a list of all top level objects in the database
    set top_list [concat [tops -n]]
    set old_list_len [llength $top_list]

    # save the database title (keep changes it)
    set tmp_title [title]

    # eliminate the excess newline that the "title" command appends
    set old_title [string range $tmp_title 0 [expr [string length $tmp_title] - 1]]

    # create a temporary file
    set tmp_file ${old_dir}/[pid]_gc_$old_tail
    file delete -force $tmp_file
    if [file exists $tmp_file] {
	puts "ERROR: $tmp_file is in the way, cannot proceed."
	puts "Aborting garbage collect, database unchanged."
	return;
    }

    # use "keep" to create a clean database
    puts "Keeping clean database to $tmp_file"
    if { [llength $top_list] == 0 } {
	puts "WARNING: no objects to keep found"
    } else {
	for { set index 0 } { $index < $old_list_len } { incr index } {
	    set obj "[lindex $top_list $index]"
	    puts "Keeping $obj"
	    eval keep $tmp_file "$obj"
	}
	if ![file exists $tmp_file] {
	    puts "ERROR: Unable to save objects to $tmp_file."
	    puts "Aborting garbage collect, database unchanged."
	    return;
	}
    }

    # close the current db just to be safe, but keep a backup
    puts "Closing the current geometry database."
    closedb
    set old_file_backup ${old_dir}/[pid]_gc_backup_$old_tail
    if [file exists $old_file_backup] {
	puts "ERROR: Unable to use $old_file_backup as backup, already exists."
	puts "Remove $old_file_backup manually (e.g., via file delete)."
	puts "Aborting garbage_collect, database closed."
	return
    }
    puts "Saving $filename to $old_file_backup"
    file rename $filename $old_file_backup
    if ![file exists $old_file_backup] {
	puts "ERROR: Unable to rename $filename to $old_file_backup for safety."
	puts "Aborting garbage_collect, database closed."
	return
    }

    # replace the old .g with the new file (if there were objects to save)
    if [file exists $tmp_file] {
	puts "Replacing $filename with $tmp_file"
	file rename $tmp_file $filename
	if ![file exists $filename] {
	    puts "ERROR: Unable to rename $tmp_file to $filename."
	    puts "Aborting garbage_collect, database closed."
	    return
	}
    }

    # open the new database, keep backup until verified
    opendb $filename y

    # repair the title
    if [catch {title $old_title} title_error] {
	puts "WARNING: Unable to set the title"
	puts "$title_error"
    }

    # restore the view
    if [catch {eval view size $view_size} view_error] {
	puts "WARNING: Unable to restore view"
	puts "view size failed:\n$view_error"
    }
    if [catch {eval view quat $view_quat} view_error] {
	puts "WARNING: Unable to restore view"
	puts "view quat failed:\n$view_error"
    }
    if [catch {eval view center $view_center} view_error] {
	puts "WARNING: Unable to restore view"
	puts "view center failed:\n$view_error"
    }

    # turn off autosize while we restore the previous view
    set autosize 0

    # put back up anything that was being displayed
    if { [llength $obj_list] > 0 } {
	eval e $obj_list
    }

    # restore the autosize setting
    set autosize $old_autosize

    # VERIFY THE NEW FILE

    set verify_failures 0

    puts "Verifying new $filename"

    set new_file [opendb]
    set new_dir [file dirname $new_file]
    set new_tail [file tail $new_file]
    set new_size [file size $new_file]

    # make sure the new copy is correct (first make sure we have the same number
    # of top level ovjects)
    set new_top_list [tops -n]
    set new_list_len [llength $new_top_list]
    if { $new_list_len != $old_list_len } {
	puts "ERROR: Top-level object lists do NOT match ($new_list_len != $old_list_len)"
	puts "Result of 'tops' was:\n$new_top_list"
	puts "\n\n'tops' should have produced:\n$top_list"
	puts "Aborting garbage collect, restoring from backup."
	if [file exists $old_file_backup] {
	    file delete -force $filename
	    file rename $old_file_backup $filename
	} else {
	    puts "ERROR: Unexpected failure encountered while restoring from backup!!!"
	}
	return
    }
    # now compare the name of each top level object
    for { set index 0 } { $index < $new_list_len } { incr index } {
	if { [string compare [lindex $new_top_list $index] [lindex $top_list $index] ] } {
	    puts "ERROR: Failed to find [lindex $new_top_list $index]"
	    puts "Result of 'tops' was:\n$new_top_list"
	    puts "\n\n'tops' should have produced:\n$top_list"
	    puts "Aborting garbage collect, restoring from backup."
	    if [file exists $old_file_backup] {
		file delete $filename
		file rename $old_file_backup $filename
	    } else {
		puts "ERROR: Unexpected failure encountered while restoring from backup!"
	    }
	    return
	}
    }

    # make sure title matches
    set tmp_title [title]
    set new_title [string range $tmp_title 0 [expr [string length $tmp_title] - 1]]
    if { $new_title != $old_title } {
	puts "WARNING: Database title does not match"
	puts "Old title: $old_title"
	puts "New title: $new_title"
	puts "Verify $filename and compare to $old_file_backup manually (e.g., via g_diff)."
	incr verify_failures
    }

    # TODO: should really verify every object in the database, not just

    puts "old size: $old_size bytes, new size: $new_size bytes"
    set percentage [format "%.1f" [expr \( $old_size - $new_size \) / \( $old_size / 100.0 \)]]
    if { $new_size < $old_size } {
	puts "Reduced by [expr $old_size - $new_size] bytes ($percentage% savings)"
	if { $percentage > 50.0 && $old_size > 512 } {
	    puts "WARNING: Database size decreased substantially (more than 50%)"
	    incr verify_failures
	}
    } elseif { $new_size == $old_size } {
	puts "Database size did NOT change."
    } else {
	puts "Increased by [expr $new_size - $old_size] bytes ($percentage% savings)"
	if { $old_size > 512 } {
	    puts "Database got bigger!  This should generally not happen."
	    incr verify_failures
	}
    }

    if { $new_file != $filename || $new_dir != $old_dir || $new_tail != $old_tail } {
	puts "WARNING: Unexpected changes encountered."
	puts "Original .g file is saved as $old_file_backup"
	incr verify_failures
    }

    if { $verify_failures > 0 } {
	puts "\nOne or more verification failures encountered."
	puts "Verify $filename and compare to $old_file_backup manually (e.g., via g_diff)."
	return
    }

    # delete the original file
    file delete -force $old_file_backup
    puts "\ngarbage_collect completed successfully."
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
