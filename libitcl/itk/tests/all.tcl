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

set ::tcltest::testSingleFile false
set ::tcltest::testsDirectory [file dirname [info script]]

# We need to ensure that the testsDirectory is absolute
::tcltest::normalizePath ::tcltest::testsDirectory

puts stdout "Tests running in interp:       [info nameofexecutable]"
puts stdout "Tests running with pwd:        [pwd]"
puts stdout "Tests running in working dir:  $::tcltest::testsDirectory"
if {[llength $::tcltest::skip] > 0} {
    puts stdout "Skipping tests that match:            $::tcltest::skip"
}
if {[llength $::tcltest::match] > 0} {
    puts stdout "Only running tests that match:        $::tcltest::match"
}

if {[llength $::tcltest::skipFiles] > 0} {
    puts stdout "Skipping test files that match:       $::tcltest::skipFiles"
}
if {[llength $::tcltest::matchFiles] > 0} {
    puts stdout "Only sourcing test files that match:  $::tcltest::matchFiles"
}

set timeCmd {clock format [clock seconds]}
puts stdout "Tests began at [eval $timeCmd]"

# protect against exit calls in the tests
rename exit real_exit
proc exit args {
    #puts stderr "[info script]: called [info level 0]"
}

# source each of the specified tests
foreach file [lsort [::tcltest::getMatchingFiles]] {
    set tail [file tail $file]
    puts stdout $tail
    if {[catch {source $file} msg]} {
	puts stdout $msg
    }
}

# cleanup
puts stdout "\nTests ended at [eval $timeCmd]"
::tcltest::cleanupTests 1

real_exit
