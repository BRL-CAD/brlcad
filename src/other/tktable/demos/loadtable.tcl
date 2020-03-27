# loadtable.tcl
#
# Ensures that the table library extension is loaded

if {[string equal "Windows CE" $::tcl_platform(os)]} {
    if {[info proc puts] != "puts" || ![llength [info command ::tcl::puts]]} {
	# Rename puts to something innocuous on Windows CE,
	# but only if it wasn't already renamed (thus it's a proc)
	rename puts ::tcl::puts
	proc puts args {
	    set la [llength $args]
	    if {$la<1 || $la>3} {
		error "usage: puts ?-nonewline? ?channel? string"
	    }
	    set nl \n
	    if {[lindex $args 0]=="-nonewline"} {
		set nl ""
		set args [lrange $args 1 end]
	    }
	    if {[llength $args]==1} {
		set args [list stdout [join $args]] ;# (2)
	    }
	    foreach {channel s} $args break
	    if {$channel=="stdout" || $channel=="stderr"} {
		#$::putsw insert end $s$nl
	    } else {
		set cmd ::tcl::puts
		if {$nl==""} {lappend cmd -nonewline}
		lappend cmd $channel $s
		uplevel 1 $cmd
	    }
	}
    }
}

set ::VERSION 2.10
if {[string compare unix $tcl_platform(platform)]} {
    set table(library) Tktable$::VERSION[info sharedlibextension]
} else {
    set table(library) libTktable$::VERSION[info sharedlibextension]
}
if {
    [string match {} [info commands table]]
    && [catch {package require Tktable $::VERSION} err]
    && [catch {load [file join [pwd] $table(library)]} err]
    && [catch {load [file join [pwd] .. unix $table(library)]} err]
    && [catch {load [file join [pwd] .. win $table(library)]} err]
} {
    error $err
} else {
    puts "Tktable v[package provide Tktable] loaded"
}
