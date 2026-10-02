#                M E T A S T R E N G T H . T C L
# BRL-CAD
#
# Copyright (c) 2019-2026 United States Government as represented by
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
# Usage: metastrength combination [-options ...]
#
# This script searches for all metaballs in a given combination
# hierarchy.  It prints field strengths for all points on all
# metaballs encountered.  It can also optionally set or scale the
# metaball field strengths in that combination or on a cloned copy.
#
###

namespace eval ::brlcad::metastrength {}

proc ::brlcad::metastrength::invoke {command_prefix command args} {
    return [{*}$command_prefix $command {*}$args]
}

proc ::brlcad::metastrength::default_command_prefix {} {
    if {[info exists ::ArcherCore::application]} {
	set application $::ArcherCore::application
	if {$application ne "" && [llength [info commands $application]]} {
	    return [list $application]
	}
    }
    if {[llength [info commands ::brlcad::mged]]} {
	return ::brlcad::mged
    }
    error "metastrength requires an MGED or Archer command context"
}

proc ::brlcad::metastrength::print_strengths {command_prefix obj} {

    puts "Searching $obj for metaballs"

    set matches [invoke $command_prefix search /$obj -type metaball]
    set count [llength $matches]

    puts "Metaballs found: $count"
    if {$count == 0} {
	return
    }

    foreach mb $matches {
	puts "  $mb:"
	set count 1
	foreach mp [lindex [invoke $command_prefix get $mb] end] {
	    puts "    pt#$count field strength: [lindex $mp 3]"
	    incr count
	}
    }
}


proc ::brlcad::metastrength::print_usage {} {
    puts "Usage: metastrength comb \[-help\] \[-clone\] \[-scale factor | -set strength\]\n"
}


proc ::brlcad::metastrength::run {command_prefix comb args} {
    set argc [llength $args]
    set do_clone 0
    set do_scale 0
    set do_set 0
    set mfs 1.0

    if {$comb eq "" || $argc > 3} {
	print_usage
	return
    }

    if {[string match "-h*" "$comb"]} {
	print_usage
	return
    } elseif {[string match {-*} "$comb"]} {
	error "Expecting a combination object name before options"
    }
    
    for {set i 0} {$i < $argc} {incr i} {
	set arg [lindex $args $i]
	if {[string match {-*} $arg]} {
	    switch -glob -- $arg {
		-clone {
		    set do_clone 1
		}
		-scale {
		    set do_scale 1
		    if {$i + 1 > $argc - 1} {
			print_usage
			error "-scale option requires a factor"
		    }
		    incr i
		    set mfs [lindex $args $i]
		}
		-set {
		    set do_set 1
		    if {$i + 1 > $argc - 1} {
			print_usage
			error "-set option requires a strength value"
		    }
		    incr i
		    set mfs [lindex $args $i]
		}
		-help {
		    print_usage
		    return
		}
		* {
		    print_usage
		    error "Unrecognized subcommand \[$arg\]"
		}
	    }
	} else {
	    print_usage
	    error "Unrecognized command line argument \[$arg\]"
	}
    }

    if {![invoke $command_prefix exists $comb]} {
	error "Unable to find a combination named \[$comb\]"
    }

    print_strengths $command_prefix $comb
    if {$argc == 0} {
	return
    }
    
    if {$do_clone} {
	puts -nonewline "Cloning $comb ... "
	set cloneout [invoke $command_prefix clone $comb]
	set last_line [string range $cloneout \
	    [expr {[string last "\n" $cloneout end-1] + 1}] end]
	set obj [lindex $last_line 0]
	puts "to $obj, done."
	if {![invoke $command_prefix exists $obj]} {
	    error "Unable to successfully clone \[$comb\]"
	}
    } else {
	set obj $comb
    }

    if {$do_scale} {
	puts "SCALING $obj metaball field strengths by $mfs"
    } elseif {$do_set} {
	puts "SETTING $obj metaball field strengths to $mfs"
    }

    set newmatches [invoke $command_prefix search $obj -type metaball]
    foreach nmb $newmatches {
	set newpts {}
	set newmeta [invoke $command_prefix get $nmb]
	foreach point [lindex $newmeta end] {

	    if {$do_scale} {
		lset point end-1 [expr {$mfs * [lindex $point end-1]}]
	    } elseif {$do_set} {
		lset point end-1 $mfs
	    }

	    lappend newpts $point
	}
	invoke $command_prefix mv $nmb ${nmb}.backup
	invoke $command_prefix put $nmb {*}[lrange $newmeta 0 end-1] $newpts
	if {![catch {invoke $command_prefix lt $nmb}]} {
	    catch {invoke $command_prefix kill -f ${nmb}.backup}
	}
    }
    puts "Done.\n"

    print_strengths $command_prefix $obj
}

# Keep the historical command-line entry point in both MGED and Archer.
proc ::metastrength {comb args} {
    tailcall ::brlcad::metastrength::run \
	[::brlcad::metastrength::default_command_prefix] $comb {*}$args
}


# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
