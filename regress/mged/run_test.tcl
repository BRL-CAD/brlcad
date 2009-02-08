#                        R U N _ T E S T . T C L
# BRL-CAD
#
# Copyright (c) 2004-2009 United States Government as represented by
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

###########
#
#                     R U N _ T E S T . T C L
#
# This script is intended to run single tests from within the regress/mged
# directory, using local mged if compiled in-tree and if not just trying
# mged.

#
#	SETUP

proc is_mged {name} {if {[string match mged [file tail $name]] && [file executable $name] && ![file isdirectory $name]} {return 1} else {return 0}}

set MGED_CMD [pwd]/../../src/mged/mged

if {![is_mged $MGED_CMD]} {
  global MGED_CMD
  puts "Warning - using default MGED in path, probably NOT local compiled copy"
  set MGED_CMD mged
}

set top_srcdir ../../ 

if {[info exists ::env(LD_LIBRARY_PATH)]} {
   set ::env(LD_LIBRARY_PATH) ../src/other/tcl/unix:../src/other/tk/unix:$top_srcdir/src/other/tcl/unix:$top_srcdir/src/other/tk/unix:$::env(LD_LIBRARY_PATH)
} else {
   set ::env(LD_LIBRARY_PATH) ../src/other/tcl/unix:../src/other/tk/unix:$top_srcdir/src/other/tcl/unix:$top_srcdir/src/other/tk/unix
}

if {[info exists ::env(DYLD_LIBRARY_PATH)]} {
   set ::env(DYLD_LIBRARY_PATH) ../src/other/tcl/unix:../src/other/tk/unix:$top_srcdir/src/other/tcl/unix:$top_srcdir/src/other/tk/unix:$::env(DYLD_LIBRARY_PATH)
} else {
   set ::env(DYLD_LIBRARY_PATH) ../src/other/tcl/unix:../src/other/tk/unix:$top_srcdir/src/other/tcl/unix:$top_srcdir/src/other/tk/unix
}

proc add_test {cmdname} {
     global top_srcdir
     set mgedfile [open ./tmp_run_test.mged a]
     puts $mgedfile "source regression_resources.tcl"
     set testfile [open [format %s.mged $cmdname] r]
     while {[gets $testfile line] >= 0} {
        puts $mgedfile $line
     }
     close $mgedfile
     close $testfile
}

proc run_test {cmdname} {
     global MGED_CMD
     global top_srcdir
     add_test $cmdname
     exec $MGED_CMD -c [format %s.g $cmdname] < tmp_run_test.mged >>& [format %s.log $cmdname]
     file delete tmp_run_test.mged 
}

run_test [lindex $argv 0]
