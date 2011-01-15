#!/bin/sh
#                        A M P I . T C L
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
# This is a comment \
    /bin/echo "This is not a shell script"
# This is a comment \
    exit

# make the pkgIndex.tcl
if {![info exists argv]} {
    puts "No directory argument provided."
    return 0
}

foreach arg $argv {
    # generate a pkgIndex.tcl file in the arg dir
    puts "Generating pkgIndex.tcl in $arg"
    catch {pkg_mkIndex -verbose $arg *.tcl *.itcl *.itk *.sh}

    if {![file exists "$arg/pkgIndex.tcl"]} {
	puts "ERROR: pkgIndex.tcl does not exist in $arg"
	continue
    }

    set pkgIndex ""
    set header ""

    # sort the pkgIndex.tcl
    set fd [open "$arg/pkgIndex.tcl"]
    while {[gets $fd data] >= 0} {
	if {[string compare -length 7 $data "package"] == 0} {
	    lappend pkgIndex $data
	} else {
	    lappend header $data
	}
    }
    close $fd

    # write out the sorted pkgIndex.tcl
    set fd [open "$arg/pkgIndex.tcl" {WRONLY TRUNC CREAT}]
    foreach line $header {
	puts $fd $line
    }
    foreach line [lsort $pkgIndex] {
	puts $fd $line
    }
    close $fd
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
