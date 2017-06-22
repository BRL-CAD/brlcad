# all.tcl --
#
# This file contains a top-level script to run all of the ttk
# tests.  Execute it by invoking "source all.tcl" when running tktest
# in this directory.
#
# Copyright (c) 2007 by the Tk developers.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Tcl 8.5
package require tcltest 2.2
package require Tk ;# This is the Tk test suite; fail early if no Tk!
tcltest::configure {*}$argv
tcltest::configure -testdir [file normalize [file dirname [info script]]]
tcltest::configure -loadfile \
    [file join [file dirname [tcltest::testsDirectory]] constraints.tcl]
tcltest::configure -singleproc 1
tcltest::runAllTests

