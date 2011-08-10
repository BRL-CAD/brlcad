#                        B O T S . T C L
# BRL-CAD
#
# Copyright (c) 2009-2011 United States Government as represented by
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
# produce a report of all BoT objects in the database for a given type
#
# DEPRECATED: this command should go away once 'search' has support to
# introspect primitive data parameters (form elements)
#

proc per_line { args } {
    puts stderr "DEPRECATION WARNING: 'bots' and 'per_line' are temporary until 'search' is enhanced."
    foreach list $args {
	foreach item $list {
	    puts "$item"
	}
    }
}

proc bots { args } {

    set extern_commands [list db tops]
    foreach cmd $extern_commands {
	catch {auto_load $cmd} val
	if {[expr [string compare [info command $cmd] $cmd] != 0]} {
	    puts stderr "[info script]: Application fails to provide command '$cmd'"
	    return
	}
    }

    set argc [llength $args]
    set results [list]

    if { $argc == 0 || $argc > 2 } {
	puts "Usage: bots {surf|plate|volume|plate_nocos} \[object\]"
	return
    } elseif { $argc == 1 } {
	set t [tops -n]
	foreach object $t {
	    lset results [concat $results [bots [lindex $args 0] "$object"]]
	}
    } else {
	# puts "DEBUG: bots $args"

	set mode [lindex $args 0]
	set name [lindex $args 1]

	catch {db get $name} objData
	set type [lindex $objData 0]

	if { $type == "comb" } {
	    foreach node [lt $name] {
		set child [lindex $node 1]
		lset results [concat $results [bots $mode "$child"]]
	    }
	    return $results
	}

	if { $type == "bot" } {
	    catch {db get $name mode} botmode
	    if { $botmode == $mode } {
		lappend results "$name"
	    }
	}
    }

    set unique [list]
    foreach obj $results {
	if { [lsearch $unique $obj] == -1 } {
	    lappend unique $obj
	}
    }
    return $unique
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
