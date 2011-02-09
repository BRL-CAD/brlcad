#                B U I L D _ R E G I O N . T C L
# BRL-CAD
#
# Copyright (c) 2000-2011 United States Government as represented by
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
#	This procedure builds regions in MGED based on primitive solid names
#
#	Usage:
#		build_region [-a region_number] tag start_num end_num
#
#	The region is built from all primitive solids matching the RE "tag.s*"
#	The region is a union of all the solids of the form "tag.s#", with optional
#	intersections and subtractions. Solids of the form "tag.s#-#" are subtracted from
#	the base solid, and solids of the form "tag.s#+#" are intersected. For example,
#	given solids "abc.s1, abc.s2, abc.s2-1, abc.s2+1", the command "build_region abc 1 2"
#	will produce a region "abc.r1" (assuming "abc.r1" does not already exist):
#		u abc.s1
#		u abc.s2
#		+ abc.s2+1
#		- abc.s2-1
#
#	Only solids with numbers from "start_num" through "end_num" will be used in building the region
#	A new region will be created, unless the "-a" option is used. The "-a region_number" option
#	results in the new construction being unioned with an existing region named "tag.r#" where #
#	is the specified "region_number". No checking is done to determine if the existing region already
#	contains the unioned objects.
#	When a new region is created, its properties are determined by the current "regdef" setings, and
#	the regdef ident number is incremented.
#
#	Author:
#		John R. Anderson
#

proc build_region { args } {
    set usage "Usage:\n\tbuild_region \[-a region_num\] tag start_num end_num"

    # get command line arguments
    set argc [llength $args]
    if { $argc != 3 && $argc != 5 } {
	error "Too few arguments\n$usage"
    }
    set append 0
    set index 0
    if { $argc == 5 } {
	set append 1
	set regnum [lindex $args 1]
	incr index 2
    }
    set tag [lindex $args $index]
    incr index
    set start [lindex $args $index]
    incr index
    set end [lindex $args $index]

    # get regdef values
    set defs [regdef]
    set id [lindex $defs 1]
    set air [lindex $defs 3]
    set los [lindex $defs 5]
    set mater [lindex $defs 7]

    # get list of solids mathing tag form
    set taglen [expr [string length $tag] + 2]
    set alist [expand ${tag}.s*]
    if { $alist == "${tag}.s*" } {
	error "No solids found with tag ($tag)"
    }

    # use dictionary order sorting to get solids in handy order
    set alist [lsort -dictionary $alist]

    # loop through each solid in the sorted list
    set tree ""
    set curtree ""
    set cursol -1
    foreach solid $alist {
	# the default operator is union
	set op u

	# check for a minus sign beyond the tag
	set opind [string first "-" $solid $taglen]
	if { $opind > -1 } {
	    set op -
	} else {
	    # check for an intersection
	    set opind [string first "+" $solid $taglen]
	    if { $opind > -1 } {
		set op n
	    }
	}

	# get the base solid number
	if { $opind > -1 } {
	    # this is a subtracted or intersected solid
	    set solnum [string range $solid $taglen [expr $opind - 1]]
	} else {
	    # this is a base solid
	    set solnum [string range $solid $taglen end]
	}

	# if the solid number is outside the specified range, skip it
	if { $solnum < $start || $solnum > $end } continue

	# if the base solid number has changed, we start a new union
	if { $solnum != $cursol } {
	    if { $op != "u" } {
		error "Missing base solid (${tag}.s${solnum})"
	    }
	    # start new union
	    if { [llength $curtree] > 0 } {
		if { [llength $tree] > 0 } {
		    set tree [list u $tree $curtree]
		} else {
		    set tree $curtree
		}
	    }
	    set curtree [list l $solid]
	    set cursol $solnum
	} else {
	    # add to current tree
	    set curtree [list $op $curtree [list l $solid]]
	}
    }

    # add last bit of the region currently being built
    if { [llength $curtree] > 0 } {
	if { [llength $tree] > 0 } {
	    set tree [list u $tree $curtree]
	} else {
	    set tree $curtree
	}
    }

    if { [llength $tree] == 0 } {
	error "no solids found for tree ($args)"
    }

    # if we are in append mode, union the current tree with an existing region
    if { $append } {
	set regname ${tag}.r$regnum
	if { [catch {db get $regname tree} oldtree] == 0 } {
	    set tree [list u $oldtree $tree]
	    if { [catch {db adjust $regname tree $tree} ret ] } {
		error "failed to update existing region ($regname)"
	    } else {
		puts "Appended to region $regname"
	    }
	} else {
	    # specified region does not exist, so create it
	    if { [catch {db put $regname comb region yes air $air id $id los $los GIFTmater $mater tree $tree} ret] } {
		error "failed to create region!!!\n$ret"
	    } else {
		puts "Created region $regname"
	    }
	    # increment regdef ident number
	    if { $id > 0 } {
		incr id
		regdef $id
	    }
	}
    } else {
	# find next region name that doesn't already exist
	set regnum 0
	set reg_exists 1
	while { $reg_exists } {
	    incr regnum
	    set regname ${tag}.r$regnum
	    if { [catch {db get $regname} ret] } {
		set reg_exists 0
	    }
	}

	# create the new region
	if { [catch {db put $regname comb region yes air $air id $id los $los GIFTmater $mater tree $tree} ret] } {
	    error "failed to create region!!!\n$ret"
	} else {
	    puts "Created region $regname"
	}

	# increment regdef ident number
	if { $id > 0 } {
	    incr id
	    regdef $id
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
