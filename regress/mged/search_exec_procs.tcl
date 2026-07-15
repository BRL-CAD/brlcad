#           S E A R C H _ E X E C _ P R O C S . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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
# Procedures and aliases in this file must be available to search -exec after
# it is sourced into MGED's main Tcl interpreter.

proc search_exec_global {path {value {global default {with braces} \path}}} {
    attr set [file tail $path] search_exec_global $value
    return 1
}

namespace eval ::search_exec_test {
    namespace eval nested {
	variable settings
	array set settings {value {namespace default {with braces} \path}}

	proc mark {path {key search_exec_namespace}} {
	    variable settings
	    attr set [file tail $path] $key $settings(value)
	    return 1
	}
    }
}

interp alias {} search_exec_alias {} attr set r5 search_exec_alias \
    {alias value {with braces} \path}

proc search_exec_verify {} {
    foreach {obj key expected} {
	r1 search_exec_global {global default {with braces} \path}
	r2 search_exec_namespace {namespace default {with braces} \path}
	r5 search_exec_alias {alias value {with braces} \path}
    } {
	set actual [attr get $obj $key]
	if {$actual ne $expected} {
	    error "FAIL $obj $key: expected '$expected', got '$actual'"
	}
    }
    puts "PASS search -exec sourced Tcl state"
}

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
