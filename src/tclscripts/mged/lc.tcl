#                           L C . T C L
# BRL-CAD
#
# Copyright (c) 2005-2012 United States Government as represented by
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
# The 'lc' command lists a set of codes (ie attributes) of regions
# within a group/combination. The attributes listed are 'region_id',
# 'material_id' and 'los' followed by 'region name'. If an attribute
# is unset, it is reported as '--'. By default, the list is sorted in
# ascending (a-z) order by 'region_id'. Sorting can be performed on
# any column in ascending or descending (z-a) order. Duplicate
# 'region_id' can be listed within a group/combination. Output can also
# be saved to a file. Option '-d' specifies duplicate 'region_id'
# should be listed. Options '-2', '-3', '-4' specify sort columns 2,
# 3 or 4. Option '-z' specifies descending sort order. Option
# '-f FileName' identifies output should be to a file and specifies the
# file name. If the path is not included as part of the 'FileName' the
# path will be the 'current directory'.
#


proc lc {args} {
    set name_cnt 0
    set error_cnt 0
    set find_duplicates_flag_cnt 0
    set descending_sort_flag_cnt 0
    set file_name_flag_cnt 0
    set sort_column_flag_cnt 0

    if { [llength $args] == 0 } {
	puts stdout "Usage: \[-d\] \[-z\] \[-2|-3|-4\] \[-f {FileName}\] {GroupName}" 
	return
    }

    # count number of each arg type
    foreach arg $args {
	if { $arg == "-f" } {
	    incr file_name_flag_cnt
	} elseif { [string is integer $arg] && $arg <= -2 && $arg >= -4 } {
	    incr sort_column_flag_cnt
	} elseif { $arg != "-d" && $arg != "-z" } {
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
	if { $arg == "-z" } { 
	    set descending_sort_flag_cnt 1
	    continue
	}
	if { $arg <= -2 && $arg >= -4 } {
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
    set objs [search $group_name -type region]

    set lines {}
    set line ""
    foreach obj $objs {
	if { [catch {set Xregion_id [lindex [attr show $obj region_id] 3]} tmp_msg] } {
	    set Xregion_id "--"
	}
	if { [catch {set Xmaterial_id [lindex [attr show $obj material_id] 3]} tmp_msg] } {
	    set Xmaterial_id "--"
	}
	if { [catch {set Xlos [lindex [attr show $obj los] 3]} tmp_msg] } {
	    set Xlos "--"
	}
	set line "$Xregion_id $Xmaterial_id $Xlos $obj"
	lappend lines $line
    }
    unset objs

    if { $find_duplicates_flag_cnt == 1 } {
	set lines [lsort -dictionary -index 0 $lines]
	set lines2 {}
	set prev_id ""
	set prev_line ""
	set proc_seq 0
	foreach line $lines {
	    set cur_id [lindex $line 0]
	    if { $cur_id == $prev_id } {
		lappend lines2 $prev_line
		set proc_seq 1
	    } else {
		if { $proc_seq == 1 } {
		    lappend lines2 $prev_line
		    set proc_seq 0
		}
	    }
	    set prev_line $line
	    set prev_id $cur_id
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

    # minimum column width to accomidate column headers
    set region_id_len_max 2
    set material_id_len_max 3
    set los_len_max 3

    # determine column widths
    foreach line $lines {
	set region_id [lindex $line 0]
	set material_id [lindex $line 1]
	set los [lindex $line 2]
	set region_id_len [string length $region_id]
	set material_id_len [string length $material_id]
	set los_len [string length $los]
	if { $region_id_len > $region_id_len_max } {
	    set region_id_len_max $region_id_len
	}
	if { $material_id_len > $material_id_len_max } {
	    set material_id_len_max $material_id_len
	}
	if { $los_len > $los_len_max } {
	    set los_len_max $los_len
	}
    }
    set w1 [expr ($region_id_len_max + 1)]
    set w2 [expr ($material_id_len_max + 1)]    
    set w3 [expr ($los_len_max + 1)]

    set lines2 {}
    foreach line $lines {
	set region_id [lindex $line 0]
	set material_id [lindex $line 1]
	set los [lindex $line 2]
	set obj [lindex $line 3]
	set line2 [format "%-*s %-*s %-*s %s" $w1 $region_id $w2 $material_id $w3 $los $obj]
	lappend lines2 $line2
    }
    unset lines

    # convert columns from 1-4 to 0-3
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
	puts $chan [format "%-*s %-*s %-*s %s" $w1 "ID" $w2 "MAT" $w3 "LOS" "NAME"]
	foreach line2 $lines2 {
	    puts $chan "$line2"
	}
	close $chan
    } else {
	puts stdout "List length: $list_len"
	puts stdout [format "%-*s %-*s %-*s %s" $w1 "ID" $w2 "MAT" $w3 "LOS" "NAME"]
	foreach line2 $lines2 {
	    puts stdout "$line2"
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

