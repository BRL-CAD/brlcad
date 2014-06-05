#                           L C . T C L
# BRL-CAD
#
# Copyright (c) 2005-2014 United States Government as represented by
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
# The 'lc' command lists a set of codes (i.e. attributes) of regions
# within a group/combination. Listed in columns are the following
# 'region_id', 'material_id', 'los', 'region name', 'region parent'.
# If a value is unset, it is reported as '--'.
#
# Option '-d' specifies to list only regions with duplicate 'region_id'.
#
# Option '-s' is the same as '-d' except some duplicates will not be
# reported (i.e. skipped). Skipped duplicates will be those within the
# specified group, that have the same parent, 'material_id' and 'los'.
#
# Option '-r' remove regions from the list in which their parent is
# a region and the region is subtracted within the parent. The '-r'
# option can be combined with any other option.
#
# Options '-2', '-3', '-4', '-5' specify to sort by column 2, 3, 4 or 5.
#
# Option '-z' specifies descending (z-a) sort order. By default, the
# list is sorted in ascending (a-z) order by 'region_id'.
#
# Option '-f FileName' identifies output should be to a file and
# specifies the file name. If the path is not included as part of the
# 'FileName' the path will be the 'current directory'.
#

set lc_done_flush 0

proc lc {args} {
    global lc_done_flush
    set name_cnt 0
    set error_cnt 0
    set find_duplicates_flag_cnt 0
    set skip_special_duplicates_flag_cnt 0
    set skip_subtracted_regions_flag_cnt 0
    set descending_sort_flag_cnt 0
    set file_name_flag_cnt 0
    set sort_column_flag_cnt 0

    if { [llength $args] == 0 } {
	puts stdout "Usage: \[-d|-s|-r\] \[-z\] \[-2|-3|-4|-5\] \[-f {FileName}\] {GroupName}"
	return
    }

    # count number of each arg type
    foreach arg $args {
	if { $arg == "-f" } {
	    incr file_name_flag_cnt
	} elseif { [string is integer $arg] && $arg <= -2 && $arg >= -5 } {
	    incr sort_column_flag_cnt
	} elseif { $arg != "-d" && $arg != "-z" && $arg != "-s" && $arg != "-r"} {
	    incr name_cnt
	}
    }

    # test arg type counts
    if { $file_name_flag_cnt > 1 } {
	puts stdout "Error: '-f' used more than once."
	incr error_cnt
    }
    if { $sort_column_flag_cnt > 1 } {
	puts stdout "Error: Sort column defined more than once."
	incr error_cnt
    }
    if { $name_cnt > 2 } {
	puts stdout "Error: More than one group name and/or file name was specified."
	incr error_cnt
    } elseif { $name_cnt == 0 && $file_name_flag_cnt == 0 } {
	puts stdout "Error: Group name not specified."
	incr error_cnt
    } elseif { $name_cnt == 2 && $file_name_flag_cnt == 0 } {
	puts stdout "Error: More than one group name was specified."
	incr error_cnt
    } elseif { $name_cnt == 0 && $file_name_flag_cnt == 1 } {
	puts stdout "Error: Group name and file name not specified."
	incr error_cnt
    }
    if { $error_cnt > 0 } {
	return
    }

    # reset counts
    set find_duplicates_flag_cnt 0
    set skip_special_duplicates_flag_cnt 0
    set skip_subtracted_regions_flag_cnt 0
    set descending_sort_flag_cnt 0
    set file_name_flag_cnt 0
    set sort_column_flag_cnt 0
    set error_cnt 0
    set sort_column 1
    set group_name_set 0
    set file_name_set 0
    set file_name ""
    set group_name ""

    foreach arg $args {
	if { $file_name_flag_cnt == 1 && $file_name_set == 0 } {
	    set file_name_set 1
	    set file_name $arg
	    continue
	}
	if { $arg == "-f" } {
	    set file_name_flag_cnt 1
	    continue
	}
	if { $arg == "-d" } {
	    set find_duplicates_flag_cnt 1
	    continue
	}
	if { $arg == "-s" } {
	    set find_duplicates_flag_cnt 1
	    set skip_special_duplicates_flag_cnt 1
	    continue
	}
	if { $arg == "-r" } {
	    set skip_subtracted_regions_flag_cnt 1
	    continue
	}
	if { $arg == "-z" } {
	    set descending_sort_flag_cnt 1
	    continue
	}
	if { $arg <= -2 && $arg >= -5 } {
	    set sort_column [expr abs($arg)]
	    set sort_column_flag_cnt 1
	    continue
	}
	set group_name_set 1
	set group_name $arg
    }

    set error_cnt 0
    set chan ""
    if { $file_name_flag_cnt == 1 && $file_name_set == 0 } {
	puts stdout "Error: File name not specified."
	incr error_cnt
    }
    if { $group_name_set == 0 } {
	puts stdout "Error: Group name not specified."
	incr error_cnt
    }
    if { $file_name_set == 1 } {
	if { [string index $file_name 0] == "-" } {
	    puts stdout "Error: File name can not start with '-'."
	    incr error_cnt
	} elseif { [file exists $file_name] == 1 } {
	    set norm_name [file normalize $file_name]
	    puts stdout "Error: File '$norm_name' already exists."
	    incr error_cnt
	} else {
	    if { [catch {set chan [open $file_name a]} tmp_msg] } {
		puts stdout "Error: $tmp_msg"
		incr error_cnt
	    }
	    set norm_name [file normalize $file_name]
	    puts stdout "Output filename: '$norm_name'"
	}
    }

    if { $error_cnt > 0 } {
	return
    }

    if { [search -name $group_name] == "" } {
	puts stdout "Error: Group '$group_name' does not exist."
	return
    }

    set objs {}

    set group_name "/$group_name"
    set objs [search $group_name -type region]

    set lines {}
    set line ""
    foreach obj $objs {
        set obj_name [file tail $obj]

	if { $obj != $group_name } {

            set obj_parent [file tail [file dirname $obj]]

	    if { $obj_parent == "" } {
		set obj_parent "--"
	    }
	    if { [catch {set Xregion_id [lindex [attr show $obj_name region_id] 3]} tmp_msg] } {
		set Xregion_id "--"
	    }
	    if { [catch {set Xmaterial_id [lindex [attr show $obj_name material_id] 3]} tmp_msg] } {
		set Xmaterial_id "--"
	    }
	    if { [catch {set Xlos [lindex [attr show $obj_name los] 3]} tmp_msg] } {
		set Xlos "--"
	    }

	    set line "$Xregion_id $Xmaterial_id $Xlos $obj_name $obj_parent"
	    lappend lines $line
	}
    }
    unset objs

    if { $find_duplicates_flag_cnt == 1 } {
	set lines [lsort -dictionary -index 0 $lines]
	set lines2 {}
	set idx1 0
	set idx2 1

	set list_len [llength $lines]

	while {$idx1 < $list_len} {

	    set prob_found 0

	    while { ($idx2 < $list_len) && ([lindex [lindex $lines $idx1] 0] == [lindex [lindex $lines $idx2] 0])} {

		if { $skip_special_duplicates_flag_cnt == 1 } {

		    # Test if there are inconsistencies in the current list of regions (i.e. regions with same region_id).
		    # All of the regions with the same region_id must have the same parent and the material_id for each of
		    # these regions must be the same.

		    set idx3 $idx2

		    while { ($idx3 < $list_len) && ([lindex [lindex $lines $idx1] 0] == [lindex [lindex $lines $idx3] 0])} {

			if { ([lindex [lindex $lines $idx1] 4] != [lindex [lindex $lines $idx3] 4]) || ([lindex [lindex $lines $idx1] 1] != [lindex [lindex $lines $idx3] 1]) || ([lindex [lindex $lines $idx1] 2] != [lindex [lindex $lines $idx3] 2])} {
			    set prob_found 1
			}
			incr idx3
		    }
		}

		if { !(($skip_special_duplicates_flag_cnt == 1) && ($prob_found == 0)) }  {

		    set first_found 0
		    set idx4 $idx2

		    while { ($idx4 < $list_len) && ([lindex [lindex $lines $idx1] 0] == [lindex [lindex $lines $idx4] 0])} {

			if { $first_found == 0 } {
			    set first_found 1
			    lappend lines2 [lindex $lines $idx1]
			    lappend lines2 [lindex $lines $idx4]
			} else {
			    lappend lines2 [lindex $lines $idx4]
			}

			incr idx4
		    }
		}

		if { $skip_special_duplicates_flag_cnt == 1 } {
		    set idx2 $idx3
		} else {
		    set idx2 $idx4
		}

	    }
	    set idx1 $idx2
	    incr idx2
	}

	if { [llength $lines2] == 0 } {
	    if { $file_name_set == 1 } {
		puts $chan "Command args: '$args'"
		puts $chan "No duplicate region_id"
		puts stdout "No duplicate region_id"
		close $chan
	    } else {
		puts stdout "No duplicate region_id"
	    }
	    puts stdout "Done."
	    return
	}
	set lines $lines2
	unset lines2
    }

    # Remove from list subtracted regions inside regions
    if { ([llength $lines] != 0) && ($skip_subtracted_regions_flag_cnt == 1) } {

	set lines2 {}

	foreach line $lines {

	    set obj [lindex $line 3]
	    set obj_parent [lindex $line 4]
	    set obj_parent_desc [split [l $obj_parent]]

	    if { [lindex $obj_parent_desc 2] == "REGION" } {

		set obj_parent_desc_len [llength $obj_parent_desc]
		set idx1 0
		set found_cnt 0
		set found_skip 0

		while { ($idx1 < $obj_parent_desc_len) } {

		    if { [lindex $obj_parent_desc $idx1] == $obj } {
			incr found_cnt

			if { [lindex $obj_parent_desc [expr ($idx1 - 1)]] == "-" } {
			    incr found_skip
			}
		    }
		    incr idx1
		}

		if { $found_skip == 0 } {
		    lappend lines2 $line
		}

	    } else {

		lappend lines2 $line
	    }
	}

	set lines $lines2
	unset lines2
    }


    # minimum column width to accommodate column headers
    set region_id_len_max 2
    set material_id_len_max 3
    set los_len_max 3
    set obj_len_max 6

    # determine column widths
    foreach line $lines {
	set region_id [lindex $line 0]
	set material_id [lindex $line 1]
	set los [lindex $line 2]
	set obj [lindex $line 3]
	set region_id_len [string length $region_id]
	set material_id_len [string length $material_id]
	set los_len [string length $los]
	set obj_len [string length $obj]

	if { $region_id_len > $region_id_len_max } {
	    set region_id_len_max $region_id_len
	}
	if { $material_id_len > $material_id_len_max } {
	    set material_id_len_max $material_id_len
	}
	if { $los_len > $los_len_max } {
	    set los_len_max $los_len
	}
	if { $obj_len > $obj_len_max } {
	    set obj_len_max $obj_len
	}
    }
    set w1 [expr ($region_id_len_max + 1)]
    set w2 [expr ($material_id_len_max + 1)]
    set w3 [expr ($los_len_max + 1)]
    set w4 [expr ($obj_len_max + 1)]

    set lines2 {}
    foreach line $lines {
	set region_id [lindex $line 0]
	set material_id [lindex $line 1]
	set los [lindex $line 2]
	set obj [lindex $line 3]
	set obj_parent [lindex $line 4]
	set line2 [format "%-*s %-*s %-*s %-*s %s" $w1 $region_id $w2 $material_id $w3 $los $w4 $obj $obj_parent]
	lappend lines2 $line2
    }
    unset lines

    # convert columns from 1-5 to 0-4
    set sort_column [expr ($sort_column - 1)]

    if { $descending_sort_flag_cnt == 1 } {
	set lines2 [lsort -dictionary -decreasing -index $sort_column $lines2]
    } else {
	set lines2 [lsort -dictionary -index $sort_column $lines2]
    }

    set list_len [llength $lines2]
    if { $file_name_set == 1 } {
	puts $chan "Command args: '$args'"
	puts $chan "List length: $list_len\n"
	puts $chan [format "%-*s %-*s %-*s %-*s %s" $w1 "ID" $w2 "MAT" $w3 "LOS" $w4 "REGION" "PARENT"]
	foreach line2 $lines2 {
	    puts $chan "$line2"
	}
	close $chan
    } else {
	puts stdout "List length: $list_len"
	puts stdout [format "%-*s %-*s %-*s %-*s %s" $w1 "ID" $w2 "MAT" $w3 "LOS" $w4 "REGION" "PARENT"]
	foreach line2 $lines2 {
	    puts stdout "$line2"
	    after 1 { set lc_done_flush [flush stdout] }
	    vwait lc_done_flush
	}
    }

    puts stdout "Done."
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
