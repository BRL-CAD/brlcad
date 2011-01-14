#                 R E M A P _ M A T E R . T C L
# BRL-CAD
#
# Copyright (c) 2005-2011 United States Government as represented by
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
# this script goes through a model and changes material codes for all
# regions in the currently open geometry database, remapping them
# according to an input file.
#
# The input file allows blank lines and comments (denoted with a '#'
# character until end-of-line).  By default the command will warn you
# if it encounters any material codes in the .g file that you didn't
# mention in the spec file.  If you don't want to see these warnings,
# you can add the argument 0 after the file name.
#
# As the changes are immediately performed, a backup of the input .g
# file should be made beforehand.
#

######################
# Example Input File #
######################
# # Specification of the mapping to be used by the proc remap_mater
# #
# # Each line is a pair "old new" that means "Every region whose
# # material code is "old" should have it replaced with material
# # code "new."
#
# #  COVART-to-GIFT
# 4  2  # RHA
# 2  8  # Pb
# 8  9  # Ti
# 17 41
#
# # Note that the 2nd line below
#
# 19 666
# 19 81
#
# # replaces the first one, so that all 19's will be replaced with
# # 81's, not 666's.
#
# # Note also that
#
# 5 10
# 10 20
#
# # will change 10's to 20's and also change 5's to 10's.
# # It will NOT change 5's to 10's and then to 20's
###


# make sure the mged commands we need actually exist
set extern_commands [list db]
foreach cmd $extern_commands {
    catch {auto_load $cmd} val
    if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	puts stderr "[info script]: Application fails to provide command '$cmd'"
	return
    }
}


#
# An ugly little kludge to allow modern versions of Tcl to use the
# "string is digit" construct, while providing older versions a
# workaround.
#
proc string_is_digit_strict {s} {
    global tcl_version
    if {$tcl_version >= 8.2} {
	return [string is digit -strict $s]
    } else {
	set l [string length $s]
	if {$l == 0} { return 0 }
	for {set i 0} {$i < $l} {incr i} {
	    if {[lsearch {0 1 2 3 4 5 6 7 8 9} \
		     [string range $s $i $i]] == -1} {
		return 0
	    }
	}
	return 1
    }
}


proc remap_mater {file_name {silent 0}} {

    #
    # Read in the spec file and build the mapping
    #
    set chan [open $file_name r]
    set line_count 0
    while {[gets $chan line] >= 0} {
	incr line_count
	#
	# Strip comments and leading white space
	#
	if {[set p [string first "\#" $line]] >= 0} {
	    set line [string range $line 0 [expr $p - 1]]
	}
	set line [string trimleft $line " \t"]
	if {[string length $line] == 0} {
	    continue
	}
	#
	# Grab a pair of digits and store them
	#
	if {[string_is_digit_strict $line]} {
	    error "Bad input on line $line_count: $line"
	}
	switch -- [scan $line "%d %d%s" old new junk] {
	    2{set new_mater($old) $new}
	    3{error "Extra stuff: '$junk' on line $line_count: $line"}
	    default{error "Bad input on line $line_count: $line"}
	}
    }
    close $chan

    #
    # Apply the mapping to every region in the database
    #
    set old_maters [array names new_mater]
    foreach region [ls -r] {
	set old_mater [db get $region GIFTmater]
	if {[lsearch $old_maters $old_mater] == -1} {
	    if {! $silent} {
		set msg [format\
			     "Warning: Region %s retaining mater %s not in mapping" \
			     $region $old_mater]
		puts stderr $msg
	    }
	} else {
	    db adjust $region GIFTmater $new_mater($old_mater)
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
