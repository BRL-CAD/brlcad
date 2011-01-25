# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-2000 by Ajuba Solutions
# All rights reserved.
# 
# RCS: @(#) $Id$

package require tcltest
namespace import -force ::tcltest::*

# Look for the -exedir flag and find a suitable tclsh executable.

if {(![info exists argv]) || ([llength $argv] < 1)} {
    set flagArray {}
} else {
    set flagArray $argv
}

array set flag $flagArray
if {[info exists flag(-exedir)]} {
    set shell [lindex \
	    [glob -nocomplain \
	    [file join $flag(-exedir) wish*.bin] \
	    [file join $flag(-exedir) wish*]] 0]
} else {
    set shell $::tcltest::tcltest
}

set ::tcltest::testSingleFile false

# use [pwd] trick to expand relative file paths to absolute paths - MMc
set cwd [pwd]
cd [file dirname [info script]]
set ::tcltest::testsDirectory [pwd]
cd $cwd

set logfile [file join $::tcltest::temporaryDirectory Log.txt]

puts stdout "Using interp: $shell"
puts stdout "Running tests in working dir: $::tcltest::testsDirectory"
if {[llength $::tcltest::skip] > 0} {
    puts stdout "Skipping tests that match:  $::tcltest::skip"
}
if {[llength $::tcltest::match] > 0} {
    puts stdout "Only running tests that match:  $::tcltest::match"
}

if {[llength $::tcltest::skipFiles] > 0} {
    puts stdout "Skipping test files that match:  $::tcltest::skipFiles"
}
if {[llength $::tcltest::matchFiles] > 0} {
    puts stdout "Only sourcing test files that match:  $::tcltest::matchFiles"
}

set timeCmd {clock format [clock seconds]}
puts stdout "Tests began at [eval $timeCmd]"

# source each of the specified tests
foreach file [lsort [::tcltest::getMatchingFiles]] {
    set tail [file tail $file]
    puts stdout $tail

    # Change to the tests directory so the value of the following
    # variable is set correctly when we spawn the child test processes

    cd $::tcltest::testsDirectory
    set cmd [concat [list | $shell $file] [split $argv] \
	    [list -outfile $logfile]]
    if {[catch {
	set pipeFd [open $cmd "r"] 
	while {[gets $pipeFd line] >= 0} {
	    puts $::tcltest::outputChannel $line
	}
	close $pipeFd
    } msg]} {
	# Print results to ::tcltest::outputChannel.
	puts $::tcltest::outputChannel $msg
    }

    # Now concatenate the temporary log file to
    # ::tcltest::outputChannel 
    if {[catch {
	set fd [open $logfile "r"]
	while {![eof $fd]} {
	    gets $fd line
	    if {![eof $fd]} {
		if {[regexp {^([^:]+):\tTotal\t([0-9]+)\tPassed\t([0-9]+)\tSkipped\t([0-9]+)\tFailed\t([0-9]+)} $line null testFile Total Passed Skipped Failed]} {
		    foreach index [list "Total" "Passed" "Skipped" \
			    "Failed"] {
			incr ::tcltest::numTests($index) [set $index]
		    }
		    incr ::tcltest::numTestFiles
		    if {$Failed > 0} {
			lappend ::tcltest::failFiles $testFile
		    }
		}
		puts $::tcltest::outputChannel $line
	    }
	}
	close $fd
    } msg]} {
	puts $::tcltest::outputChannel $msg
    }
}

set numFailures [llength $::tcltest::failFiles]

# cleanup
puts stdout "\nTests ended at [eval $timeCmd]"
::tcltest::cleanupTests 1

if {$numFailures > 0} {
    return -code error -errorcode $numFailures \
	    -errorinfo "Found $numFailures test file failures"
} else {
    return
}
exit
