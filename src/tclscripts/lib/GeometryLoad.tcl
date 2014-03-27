#               G E O M E T R Y L O A D . T C L
# BRL-CAD
#
# Copyright (c) 2014 United States Government as represented by
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
# Description -
#	This calls converters for various types of Geometry.
#

package provide cadwidgets::GeometryLoad 1.0

namespace eval cadwidgets {
proc geom_load {input_file} {

    set binpath [bu_brlcad_root [bu_brlcad_dir "bin"] ]

    set input_ext [file extension $input_file]
    set input_root [file rootname [file tail $input_file]]
    set input_dir [file dirname $input_file]
    set output_file [file join [file dirname $input_file] "$input_root.g"]

    # Don't do anything further if we're give a file that's already a .g file
    if {[string compare $input_ext ".g"] == 0} {return $input_file}

    # Don't overwrite anything
    if {[file exists $output_file]} {
	return -code error "A file named $input_root.g already exists in $input_dir - rename or remove this file before importing $input_file"
    }

    switch -- $input_ext {
	".stp" {
	    set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] step-g]] \
	    	    -o $output_file \
		    $input_file]
            catch {eval exec $cmd _conv_log}
	}
	".step" {
	    set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] step-g]] \
	    	    -o $output_file \
		    $input_file]
            catch {eval exec $cmd _conv_log}
	}
    }

    if {[file exists $output_file]} {
	return $output_file
    } else {
	return -code error "Converting $input_file to a .g file failed: $_conv_log"
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
