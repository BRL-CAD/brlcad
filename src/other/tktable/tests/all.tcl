# all.tcl --
#
# This file contains a top-level script to run all of the Tk table
# tests.  Execute it by invoking "source all.tcl" when running tktest
# in this directory.
#
# Copyright (c) 1998-2000 Ajuba Solutions
# Copyright (c) 2000-2002 Jeffrey Hobbs
#
# See the file "license.txt" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id$

package require tcltest
namespace import -force ::tcltest::*

set ::tcltest::testSingleFile false
set ::tcltest::testsDirectory [file dir [info script]]

# We need to ensure that the testsDirectory is absolute
catch {::tcltest::normalizePath ::tcltest::testsDirectory}

set chan $::tcltest::outputChannel

puts $chan "Tk $tk_patchLevel tests running in interp: [info nameofexecutable]"
puts $chan "Tests running with pwd:           [pwd]"
puts $chan "Tests running in working dir:     $::tcltest::testsDirectory"
if {[llength $::tcltest::skip] > 0} {
    puts $chan "Skipping tests that match:            $::tcltest::skip"
}
if {[llength $::tcltest::match] > 0} {
    puts $chan "Only running tests that match:        $::tcltest::match"
}

if {[llength $::tcltest::skipFiles] > 0} {
    puts $chan "Skipping test files that match:       $::tcltest::skipFiles"
}
if {[llength $::tcltest::matchFiles] > 0} {
    puts $chan "Only sourcing test files that match:  $::tcltest::matchFiles"
}

set timeCmd {clock format [clock seconds]}
puts $chan "Tests began at [eval $timeCmd]"

# source each of the specified tests
foreach file [lsort [::tcltest::getMatchingFiles]] {
    set tail [file tail $file]
    puts $chan $tail
    if {[catch {source $file} msg]} {
	puts $chan $msg
    }
}

# cleanup
puts $chan "\nTests ended at [eval $timeCmd]"
::tcltest::cleanupTests 1
exit
