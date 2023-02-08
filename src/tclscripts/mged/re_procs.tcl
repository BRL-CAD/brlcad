#                       R E _ P R O C S . T C L
# BRL-CAD
#
# Copyright (c) 2005-2023 United States Government as represented by
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
# suite of re* procs to quickly change attributes (air, los, mat, id)
# values are cascaded to all regions of a comb
#

namespace eval re_procs {
    proc chk_cmds {} {
        # make sure the mged commands we need actually exist
        set extern_commands [list db get_regions attr]
        foreach cmd $extern_commands {
	    catch {auto_load $cmd} val
	    if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	        puts stderr "[info script]: Application fails to provide command '$cmd'"
	        return -code 1
	    }
        }
    }
    proc usage { num_args type } {
        if { $num_args != 2 } {
            set upper [string toupper $type]
	    puts "Usage: re$type assembly $upper"
	    return -code 1
        }
    }
    proc driver { args type } {
        set name [lindex $args 0]
        set changeid [lindex $args 1]

        if {[catch {set objData [db get $name]} err]} { 
            puts -nonewline $err
            return -code 1
        }
        if { [lindex $objData 0] != "comb" } {
	    puts "Not a combination: $name"
	    return -code 1
        }

        set regions [get_regions $name]
        foreach region $regions {
	    attr set $region $type $changeid
        }
    }
}

# assign AIR to all regions under some assembly to some given
# AIR value
proc reair { args } {
    if { [catch {re_procs::chk_cmds}] } { return }
    if { [catch {re_procs::usage [llength $args] "air"}] } { return }
    if { [catch {re_procs::driver $args "aircode"}] } { return }
}

# assign region IDs to all regions under some assembly starting at
# some given region ID number.
proc reid { args } {
    if { [catch {re_procs::chk_cmds}] } { return }

    set argc [llength $args]
    if { ($argc != 2 && $argc != 4) || ($argc == 4 && [lindex $args 0] != "-n") } {
	puts "Usage: reid \[-n <num>\] assembly regionID"
	return
    }

    if { $argc == 4} {
	set incrval [lindex $args 1]
	set name [lindex $args 2]
	set regionid [lindex $args 3]
    } else {
	set incrval 1
	set name [lindex $args 0]
	set regionid [lindex $args 1]
    }

    set objData [db get $name]
    if { [lindex $objData 0] != "comb" } {
	puts "Not a combination: $name"
	return
    }

    set regions [get_regions $name]
    foreach region $regions {
	attr set $region region_id $regionid
	incr regionid $incrval
    }
    set highest [expr $regionid - $incrval]
    puts "Highest region_id set here: $highest"
    return
}

# assign LOS to all regions under some assembly to some given
# LOS value.
proc relos { args } {
    if { [catch {re_procs::chk_cmds}] } { return }
    if { [catch {re_procs::usage [llength $args] "los"}] } { return }
    if { [catch {re_procs::driver $args "los"}] } { return }
}

# assign material IDs to all regions under some assembly to some given
# material ID number.
proc remat { args } {
    if { [catch {re_procs::chk_cmds}] } { return }
    if { [catch {re_procs::usage [llength $args] "mat"}] } { return }
    if { [catch {re_procs::driver $args "material_id"}] } { return }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
