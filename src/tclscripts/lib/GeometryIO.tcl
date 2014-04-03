#               G E O M E T R Y I O . T C L
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

package provide cadwidgets::GeometryIO 1.0

namespace eval cadwidgets {
set ::exe_ext ""
if {$tcl_platform(platform) == "windows"} {
    set ::exe_ext ".exe"
}

proc geom_load {input_file gui_feedback} {

    set binpath [bu_brlcad_root [bu_brlcad_dir "bin"] ]

    set input_ext [file extension $input_file]
    set input_root [file rootname [file tail $input_file]]
    set input_dir [file dirname $input_file]
    set output_file [file join [file dirname $input_file] "$input_root.g"]
    set log_file [file join [file dirname $input_file] "$input_root.log"]

    # Don't do anything further if we're give a file that's already a .g file
    if {[string compare $input_ext ".g"] == 0} {return $input_file}

    # Don't overwrite anything
    if {[file exists $output_file]} {
	return -code error "A file named $input_root.g already exists in $input_dir - rename or remove this file before importing $input_file"
    }

    switch -nocase -- $input_ext {
	".3dm" {
            if {[string compare $gui_feedback "1"] == 0} {
               set gui_cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] bwish$::exe_ext]] \
                   [bu_brlcad_data tclscripts/lib/gui_conversion.tcl] "$input_file" "$log_file"]
	       catch {eval exec $gui_cmd} _conv_log
            } else {
	        set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] 3dm-g$::exe_ext]] \
	            -r \
	            -c \
		    -o $output_file \
	    	    $input_file]
 		catch {eval exec $cmd} _conv_log
            }
	}
	".stl" {
            if {[string compare $gui_feedback "1"] == 0} {
               set gui_cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] bwish$::exe_ext]] \
                   [bu_brlcad_data tclscripts/lib/gui_conversion.tcl] "$input_file" "$log_file"]
	       catch {eval exec $gui_cmd} _conv_log
            } else {
	        set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] stl-g$::exe_ext]] \
	    	    $input_file \
		    $output_file]
 		catch {eval exec $cmd} _conv_log
            }
	}
	".stp" {
            if {[string compare $gui_feedback "1"] == 0} {
               set gui_cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] bwish$::exe_ext]] \
                   [bu_brlcad_data tclscripts/lib/gui_conversion.tcl] "$input_file" "$log_file"]
	       catch {eval exec $gui_cmd} _conv_log
            } else {
	        set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] step-g$::exe_ext]] \
	    	    -v -o $output_file \
		    $input_file]

 		catch {eval exec $cmd} _conv_log
            }
	}
	".step" {
            if {[string compare $gui_feedback "1"] == 0} {
               set gui_cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] bwish$::exe_ext]] \
                   [bu_brlcad_data tclscripts/lib/gui_conversion.tcl] "$input_file" "$log_file"]
	       catch {eval exec $gui_cmd} _conv_log
            } else {
	       set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] step-g$::exe_ext]] \
	    	    -v -o $output_file \
		    $input_file]

 		catch {eval exec $cmd} _conv_log
            }
	}
	default {
	    return -code error "File format $input_ext is not supported."
	}
    }

    if {[file exists $output_file]} {
	return $output_file
    } else {
	return -code error "Converting $input_file to a .g file failed: $_conv_log"
    }
}

# TODO - pass in list of object names, not ged object - caller can assemble tops list
# and may only want a subset.
proc geom_save {input_file output_file db_component} {

    set binpath [bu_brlcad_root [bu_brlcad_dir "bin"] ]

    set output_filename [file tail $output_file]
    set output_dir [file dirname $output_file]
    set output_ext [file extension $output_file]

    # Don't overwrite anything
    if {[file exists $output_file]} {
	return -code error "A file named $output_filename already exists in $output_dir - rename or remove this file before saving as $output_file"
    }

    # Don't do anything except a copy if we're give a file that's already a .g file
    if {[string compare $output_ext ".g"] == 0} {
	file copy -force $input_file $output_file
	if {[file exists $output_file]
	    return $output_file
        } else {
	    return -code error "Error saving as $output_file"
	}
    }

    switch -- $output_ext {
	".obj" {
	    set tops_list [lsort -dictionary [$db_component tops]]
	    set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] g-obj$::exe_ext]] \
		    -o $output_file \
	    	    $input_file]
            append cmd " " { }
	    for {set i 0} {$i < [llength $tops_list]} {incr i} {
		append cmd [lindex $tops_list $i] { }
	    }
            catch {eval exec $cmd} _conv_log
	}
	".stl" {
	    set tops_list [lsort -dictionary [$db_component tops]]
	    set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] g-stl$::exe_ext]] \
	            -o $output_file \
	    	    $input_file]
            append cmd " " { }
	    for {set i 0} {$i < [llength $tops_list]} {incr i} {
		append cmd [lindex $tops_list $i] { }
	    }
            catch {eval exec $cmd} _conv_log
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
