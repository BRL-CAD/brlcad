# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-2000 by Ajuba Solutions
# All rights reserved.

package require Tcl 8.4
package require tcltest 2.1

tcltest::testsDirectory [file dir [info script]]
tcltest::runAllTests

return
