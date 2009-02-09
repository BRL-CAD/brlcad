#                         M G E D . T C L
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
#                     M G E D . T C L
#
# This script is the master top level script used to run a comprehensive
# regression test on all MGED commands.  It runs individual scripts with
# mged for each command and is responsible for managing outputs in such
# a fashion that they can be systematically compared to a standard
#

#
#	SETUP

set test_binary_name mged

proc is_test_binary {name} {if {[file executable $name] && ![file isdirectory $name]} {return 1} else {return 0}}

set CMD_NAME [pwd]/../../src/mged/$test_binary_name

if {![is_test_binary $CMD_NAME]} {
  global CMD_NAME
  puts "Error: $CMD_NAME is not present or is not a working binary."
  return 0
}

set top_srcdir [lindex $argv 0]

if {[info exists ::env(LD_LIBRARY_PATH)]} {
   set ::env(LD_LIBRARY_PATH) ../../src/other/tcl/unix:../../src/other/tk/unix:$top_srcdir/src/other/tcl/unix:$top_srcdir/src/other/tk/unix:$::env(LD_LIBRARY_PATH)
} else {
   set ::env(LD_LIBRARY_PATH) ../../src/other/tcl/unix:../../src/other/tk/unix:$top_srcdir/src/other/tcl/unix:$top_srcdir/src/other/tk/unix
}

if {[info exists ::env(DYLD_LIBRARY_PATH)]} {
   set ::env(DYLD_LIBRARY_PATH) ../../src/other/tcl/unix:../../src/other/tk/unix:$top_srcdir/src/other/tcl/unix:$top_srcdir/src/other/tk/unix:$::env(DYLD_LIBRARY_PATH)
} else {
   set ::env(DYLD_LIBRARY_PATH) ../../src/other/tcl/unix:../../src/other/tk/unix:$top_srcdir/src/other/tcl/unix:$top_srcdir/src/other/tk/unix
}


file delete mged.g mged.log mged.mged

proc add_test {cmdname {testfilename ""}} {
     global top_srcdir
     global test_binary_name
     if {[string match $testfilename ""]} {set testfilename $test_binary_name}
     set testfile [open [format ./%s.%s $testfilename $testfilename] a]
     puts $testfile "source [format %s/regress/%s/regression_resources.tcl $top_srcdir $testfilename]"
     set inputtestfile [open [format %s/regress/%s/%s.%s $top_srcdir $testfilename $cmdname $testfilename] r]
     while {[gets $inputtestfile line] >= 0} {
        puts $testfile $line
     }
     close $testfile
     close $inputtestfile
}

proc run_test {cmdname} {
     global CMD_NAME
     global top_srcdir
     if {[file exists [format %s.mged $cmdname]]} {
        exec $CMD_NAME -c [format %s.g $cmdname] < [format %s.mged $cmdname] >>& [format %s.log $cmdname]
     } else {
        add_test $cmdname [format %s_test $cmdname]
        exec $CMD_NAME -c [format %s.g $cmdname] < [format %s_test.mged $cmdname] >>& [format %s.log $cmdname]
        file delete [format %s_test.mged $cmdname]
     }
}

#
#	GEOMETRIC INPUT COMMANDS
#
add_test in
add_test make
add_test 3ptarb
add_test arb
add_test comb
add_test g
add_test r
add_test make_bb
add_test cp
add_test cpi
add_test mv
add_test mvall
add_test build_region
add_test clone
add_test prefix
add_test mirror

#
#	DISPLAYING GEOMETRY - COMMANDS
#





run_test mged
