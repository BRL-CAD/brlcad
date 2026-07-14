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
