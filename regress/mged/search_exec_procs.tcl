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

# Exercise MGED's public alias command in a headless session.  It must be
# available without auto-loading Tkcon or Tk.

# Alias to a Tcl proc copied into the search interpreter, with both a fixed
# prefix argument and a dynamic argument supplied by search -exec.
proc search_exec_alias_target {key path} {
    attr set [file tail $path] $key {alias to copied proc}
    return 1
}
alias search_exec_proc_alias search_exec_alias_target \
    search_exec_proc_alias

# Namespaced alias directly to a GED command.  The target is resolved by the
# search interpreter's GED bridge and the fixed arguments must be preserved.
alias ::search_exec_test::nested::ged_alias attr set r7 \
    search_exec_namespaced_alias {namespaced alias to GED}

# Global alias directly to a GED command, including Tcl-special characters in
# its fixed argument list.
alias search_exec_alias attr set r5 search_exec_alias \
    {alias value {with braces} \path}

# Run inside the search interpreter and use the copied public helper to query
# an alias which was replayed from MGED's main interpreter.
proc search_exec_alias_query {path} {
    set expected [list search_exec_alias_target search_exec_proc_alias]
    set actual [alias search_exec_proc_alias]
    if {$actual ne $expected} {
	error "FAIL replayed alias: expected '$expected', got '$actual'"
    }
    attr set [file tail $path] search_exec_alias_query $actual
    return 1
}

proc search_exec_verify {} {
    if {$::search_exec_precheck_error ne ""} {
	error "FAIL $::search_exec_precheck_error"
    }

    foreach {obj key expected} {
	r1 search_exec_global {global default {with braces} \path}
	r2 search_exec_namespace {namespace default {with braces} \path}
	r6 search_exec_proc_alias {alias to copied proc}
	r7 search_exec_namespaced_alias {namespaced alias to GED}
	r8 search_exec_alias_query {search_exec_alias_target search_exec_proc_alias}
	r5 search_exec_alias {alias value {with braces} \path}
    } {
	set actual [attr get $obj $key]
	if {$actual ne $expected} {
	    error "FAIL $obj $key: expected '$expected', got '$actual'"
	}
    }

    # The successful -exec lifecycle must also be fully torn down before an
    # ordinary search takes the no-interpreter path.  Keep this check inside
    # the verifier so PASS cannot be emitted if Tcl reports an error.
    if {[catch {
	search /all.g/component/power.train/r1 -maxdepth 0
    } plain_search_error]} {
	error "FAIL plain search after successful -exec: $plain_search_error"
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
