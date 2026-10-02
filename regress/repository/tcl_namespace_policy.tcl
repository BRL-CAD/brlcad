#             T C L _ N A M E S P A C E _ P O L I C Y . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#   3. The name of the author may not be used to endorse or promote
#      products derived from this software without specific prior written
#      permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# Compare the root commands found by Tcl's namespace-aware auto_mkindex
# parser with a reviewed policy manifest.  The manifest is a ratchet: new
# globals and accidental removals both require an explicit policy update.

namespace eval ::brlcad::namespace_policy {
    variable actual_entries {}
    variable all_commands {}
    variable data_root {}
    variable excluded_sources {}
}

proc ::brlcad::namespace_policy::normalize_path {path} {
    return [string map {\\ /} [file normalize $path]]
}

proc ::brlcad::namespace_policy::normalize_command {command} {
    if {[string match ::* $command]} {
	return $command
    }
    return ::$command
}

proc ::brlcad::namespace_policy::is_global_command {command} {
    set name [string range [normalize_command $command] 2 end]
    return [expr {[string first :: $name] < 0}]
}

proc ::brlcad::namespace_policy::source_is_excluded {source} {
    variable excluded_sources

    foreach excluded $excluded_sources {
	if {$source eq $excluded || [string match "$excluded/*" $source]} {
	    return 1
	}
    }
    return 0
}

proc ::brlcad::namespace_policy::source_from_index {path} {
    variable data_root

    set normalized [normalize_path $path]
    set root [normalize_path $data_root]
    set root_length [string length $root]
    if {![string equal -nocase [string range $normalized 0 \
	[expr {$root_length - 1}]] $root] ||
	[string index $normalized $root_length] ne "/"} {
	error "indexed Tcl source is outside the data tree: $normalized"
    }

    set relative [string range $normalized [expr {$root_length + 1}] end]
    return src/tclscripts/$relative
}

proc ::brlcad::namespace_policy::record_index_entry {name1 name2 op} {
    variable actual_entries
    variable all_commands

    upvar #0 $name1 index
    set command [normalize_command $name2]
    dict set all_commands $command 1
    if {![is_global_command $command]} {
	return
    }

    set source [source_from_index [lindex $index($name2) end]]
    if {[source_is_excluded $source]} {
	return
    }
    dict set actual_entries [list $source $command] 1
}

proc ::brlcad::namespace_policy::find_index_files {directory} {
    set result {}
    foreach path [glob -nocomplain -directory $directory *] {
	if {[file isdirectory $path]} {
	    lappend result {*}[find_index_files $path]
	} elseif {[file tail $path] eq "tclIndex"} {
	    lappend result $path
	}
    }
    return $result
}

proc ::brlcad::namespace_policy::read_manifest {path} {
    set channel [open $path r]
    try {
	set contents [read $channel]
    } finally {
	close $channel
    }

    set policy [dict create exclusions {} lint_shadows {} scripts {} natives {}]
    set line_number 0
    foreach line [split $contents \n] {
	incr line_number
	set line [string trim $line]
	if {$line eq "" || [string index $line 0] eq "#"} {
	    continue
	}
	if {[catch {llength $line}]} {
	    error "$path:$line_number: malformed Tcl list"
	}
	set record [lindex $line 0]
	switch -- $record {
	    exclude {
		if {[llength $line] != 2} {
		    error "$path:$line_number: exclude expects a path"
		}
		dict lappend policy exclusions [string map {\\ /} [lindex $line 1]]
	    }
	    lint-shadow {
		if {[llength $line] != 3} {
		    error "$path:$line_number: lint-shadow expects a source and command"
		}
		lassign $line unused source command
		set source [string map {\\ /} $source]
		set command [normalize_command $command]
		if {![is_global_command $command]} {
		    error "$path:$line_number: lint-shadow command must be global"
		}
		set key [list $source $command]
		if {[dict exists $policy lint_shadows $key]} {
		    error "$path:$line_number: duplicate lint-shadow for $command in $source"
		}
		dict set policy lint_shadows $key 1
	    }
	    script {
		if {[llength $line] != 5} {
		    error "$path:$line_number: script expects source, command, kind, and target"
		}
		lassign $line unused source command kind target
		set source [string map {\\ /} $source]
		set command [normalize_command $command]
		if {$kind ni {compatibility legacy public}} {
		    error "$path:$line_number: invalid script command kind \"$kind\""
		}
		if {$kind eq "compatibility" && [llength $target] == 0} {
		    error "$path:$line_number: compatibility command has no canonical target"
		}
		if {$kind ne "compatibility" && [llength $target] != 0} {
		    error "$path:$line_number: only compatibility commands may have targets"
		}
		set key [list $source $command]
		if {[dict exists $policy scripts $key]} {
		    error "$path:$line_number: duplicate script command $command in $source"
		}
		dict set policy scripts $key [dict create kind $kind target $target]
	    }
	    native {
		if {[llength $line] != 5} {
		    error "$path:$line_number: native expects component, command, kind, and target"
		}
		lassign $line unused component command kind target
		set command [normalize_command $command]
		if {$kind ni {compatibility legacy public}} {
		    error "$path:$line_number: invalid native command kind \"$kind\""
		}
		if {$kind eq "compatibility" && [llength $target] == 0} {
		    error "$path:$line_number: native compatibility command has no canonical target"
		}
		if {$kind ne "compatibility" && [llength $target] != 0} {
		    error "$path:$line_number: only compatibility commands may have targets"
		}
		set key [list $component $command]
		if {[dict exists $policy natives $key]} {
		    error "$path:$line_number: duplicate native command $command for $component"
		}
		dict set policy natives $key [dict create kind $kind target $target]
	    }
	    default {
		error "$path:$line_number: unknown record type \"$record\""
	    }
	}
    }

    set lintable_commands {}
    foreach section {scripts natives} {
	dict for {key metadata} [dict get $policy $section] {
	    set command [lindex $key end]
	    set kind [dict get $metadata kind]
	    if {$kind eq "compatibility" ||
		($kind eq "legacy" && [string match ::_mged_* $command])} {
		dict set lintable_commands $command 1
	    }
	}
    }
    foreach key [dict keys [dict get $policy lint_shadows]] {
	lassign $key source command
	if {![dict exists $lintable_commands $command]} {
	    error "$path: lint-shadow for non-compatibility command $command in $source"
	}
    }
    return $policy
}

proc ::brlcad::namespace_policy::scan_indices {root exclusions} {
    variable actual_entries
    variable all_commands
    variable data_root
    variable excluded_sources

    set actual_entries {}
    set all_commands {}
    set data_root $root
    set excluded_sources $exclusions

    set index_files [lsort [find_index_files $root]]
    if {![llength $index_files]} {
	error "no tclIndex files found below $root"
    }

    foreach index_file $index_files {
	set ::dir [file dirname $index_file]
	unset -nocomplain ::auto_index
	set channel [open $index_file r]
	try {
	    set contents [read $channel]
	} finally {
	    close $channel
	}
	trace add variable ::auto_index \
	    write ::brlcad::namespace_policy::record_index_entry
	try {
	    uplevel #0 $contents
	} finally {
	    trace remove variable ::auto_index write \
		::brlcad::namespace_policy::record_index_entry
	}
    }

    unset -nocomplain ::auto_index ::dir
    return $actual_entries
}

proc ::brlcad::namespace_policy::validate_compatibility_targets {scripts} {
    set errors {}
    dict for {key metadata} $scripts {
	if {[dict get $metadata kind] ne "compatibility"} {
	    continue
	}
	set target [dict get $metadata target]
	set command [normalize_command [lindex $target 0]]
	if {[is_global_command $command]} {
	    lassign $key source alias
	    lappend errors "$source: compatibility command $alias has global target $target"
	}
    }
    return $errors
}

proc ::brlcad::namespace_policy::check_native {manifest inventory} {
    set policy [read_manifest $manifest]
    set expected [dict get $policy natives]
    set components [dict keys $inventory]
    set actual {}
    set errors {}

    dict for {component commands} $inventory {
	foreach command $commands {
	    set command [normalize_command $command]
	    dict set actual [list $component $command] 1
	}
    }

    foreach key [lsort [dict keys $actual]] {
	if {![dict exists $expected $key]} {
	    lassign $key component command
	    lappend errors "$component: unapproved global native Tcl command $command"
	}
    }
    foreach key [lsort [dict keys $expected]] {
	lassign $key component command
	if {$component ni $components} {
	    continue
	}
	if {![dict exists $actual $key]} {
	    lappend errors "$component: policy entry has no native command $command"
	}
	set metadata [dict get $expected $key]
	if {[dict get $metadata kind] eq "compatibility"} {
	    set target [dict get $metadata target]
	    if {[is_global_command [lindex $target 0]]} {
		lappend errors "$component: compatibility command $command has global target $target"
	    }
	}
    }

    if {[llength $errors]} {
	return -code error [join $errors \n]
    }
    return [dict size $actual]
}

proc ::brlcad::namespace_policy::root_native_commands {} {
    set baseline [interp create]
    try {
	set core_commands [$baseline eval {info commands ::*}]
    } finally {
	interp delete $baseline
    }

    set script_commands [info procs ::*]
    set result {}
    foreach command [info commands ::*] {
	set command [normalize_command $command]
	if {$command in $core_commands || $command in $script_commands ||
	    [string match ::.* $command]} {
	    continue
	}
	lappend result $command
    }
    return [lsort -unique $result]
}

proc ::brlcad::namespace_policy::check_native_set {
    manifest components commands
} {
    set policy [read_manifest $manifest]
    set expected {}
    set errors {}

    dict for {key metadata} [dict get $policy natives] {
	lassign $key component command
	if {$component in $components} {
	    dict set expected $command [list $component $metadata]
	}
    }

    set actual {}
    foreach command $commands {
	dict set actual [normalize_command $command] 1
    }
    foreach command [lsort [dict keys $actual]] {
	if {![dict exists $expected $command]} {
	    lappend errors "unapproved global native Tcl command $command"
	}
    }
    foreach command [lsort [dict keys $expected]] {
	if {![dict exists $actual $command]} {
	    lassign [dict get $expected $command] component metadata
	    lappend errors "$component: policy entry has no native command $command"
	}
    }

    if {[llength $errors]} {
	return -code error [join $errors \n]
    }
    return [dict size $actual]
}

proc ::brlcad::namespace_policy::check {manifest data_directory} {
    set policy [read_manifest $manifest]
    set actual [scan_indices $data_directory [dict get $policy exclusions]]
    set expected [dict get $policy scripts]
    set errors [validate_compatibility_targets $expected]

    foreach key [lsort [dict keys $actual]] {
	if {![dict exists $expected $key]} {
	    lassign $key source command
	    lappend errors "$source: unapproved global Tcl command $command"
	}
    }
    foreach key [lsort [dict keys $expected]] {
	if {![dict exists $actual $key]} {
	    lassign $key source command
	    lappend errors "$source: policy entry has no indexed command $command"
	}
    }

    if {[llength $errors]} {
	puts stderr "Tcl namespace policy failed:"
	foreach message $errors {
	    puts stderr "  $message"
	}
	return 1
    }

    puts "Tcl namespace policy passed ([dict size $actual] reviewed global commands)"
    return 0
}

proc ::brlcad::namespace_policy::write_baseline {manifest data_directory} {
    set policy [read_manifest $manifest]
    set actual [scan_indices $data_directory [dict get $policy exclusions]]
    set previous [dict get $policy scripts]

    set channel [open $manifest w]
    try {
	puts $channel "# Tcl namespace policy manifest."
	puts $channel "#"
	puts $channel "# Records are Tcl lists with one of these forms:"
	puts $channel "#   exclude path-prefix"
	puts $channel "#   lint-shadow source command"
	puts $channel "#   script source command compatibility|legacy|public canonical-prefix"
	puts $channel "#   native component command compatibility|legacy|public canonical-prefix"
	puts $channel "#"
	puts $channel "# Update deliberately with:"
	puts $channel "#   btclsh tcl_namespace_policy.tcl --write-baseline manifest data-root"
	puts $channel ""
	foreach excluded [lsort [dict get $policy exclusions]] {
	    puts $channel [list exclude $excluded]
	}
	puts $channel ""
	foreach key [lsort [dict keys [dict get $policy lint_shadows]]] {
	    lassign $key source command
	    puts $channel [list lint-shadow $source $command]
	}
	puts $channel ""
	foreach key [lsort [dict keys $actual]] {
	    lassign $key source command
	    set metadata [dict create kind legacy target {}]
	    if {[dict exists $previous $key]} {
		set metadata [dict get $previous $key]
	    }
	    puts $channel [list script $source $command \
		[dict get $metadata kind] [dict get $metadata target]]
	}
	foreach key [lsort [dict keys [dict get $policy natives]]] {
	    lassign $key component command
	    set metadata [dict get $policy natives $key]
	    puts $channel [list native $component $command \
		[dict get $metadata kind] [dict get $metadata target]]
	}
    } finally {
	close $channel
    }
    puts "Wrote [dict size $actual] script command entries to $manifest"
}

if {[info exists argv0]} {
    if {[file normalize [info script]] eq [file normalize $argv0]} {
	if {$argc == 2} {
	    lassign $argv manifest data_directory
	    exit [::brlcad::namespace_policy::check $manifest $data_directory]
	}
	if {$argc == 3 && [lindex $argv 0] eq "--write-baseline"} {
	    lassign [lrange $argv 1 end] manifest data_directory
	    ::brlcad::namespace_policy::write_baseline $manifest $data_directory
	    exit 0
	}

	puts stderr \
	    "Usage: [file tail [info script]] ?--write-baseline? manifest data-root"
	exit 2
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
