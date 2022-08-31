# all.tcl --
#
# This file contains a top-level script to run all of the ttk
# tests.  Execute it by invoking "source all.tcl" when running tktest
# in this directory.
#
# Copyright © 2007 by the Tk developers.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Tk ;# This is the Tk test suite; fail early if no Tk!
package require tcltest 2.2
tcltest::configure {*}$argv
tcltest::configure -testdir [file normalize [file dirname [info script]]]
tcltest::configure -loadfile \
    [file join [file dirname [tcltest::testsDirectory]] constraints.tcl]
tcltest::configure -singleproc 1
set ErrorOnFailures [info exists env(ERROR_ON_FAILURES)]
encoding system utf-8
if {[tcltest::runAllTests] && $ErrorOnFailures} {exit 1}
