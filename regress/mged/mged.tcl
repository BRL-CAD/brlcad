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
set CMD_NAME ""
set top_srcdir [lindex $argv 0]
set top_bindir [lindex $argv 1]

if {[string match $top_srcdir ""]} {
   global top_srcdir
   puts "Warning:  No source directory supplied. Assuming '../../'"
   set top_srcdir ../../
}

if {![string match $top_bindir ""]} {
   # We've been given a $top_bindir, try to validate it.  There are two legal
   # possibilities - the correct top level directory that contains the compiled
   # src/$test_binary_name/$test_binary_name, or the full path to a working
   # binary with the correct name.  (The latter isn't really a "top_bindir"
   # value, but it will allow the correct assignment of a command name which
   # is the end goal here.
   global CMD_NAME test_binary_name top_bindir

   # Try legal directory and file options
   if {[file isdirectory $top_bindir]} {
      global CMD_NAME test_binary_name top_bindir
      set candidate_name $top_bindir/src/$test_binary_name/$test_binary_name
      if {[file executable $candidate_name] && ![file isdirectory $candidate_name]} {
         global CMD_NAME candidate_name
	 set CMD_NAME $candidate_name      
      }
   } else {
      global CMD_NAME test_binary_name top_bindir
      if {[string match $test_binary_name [file tail $top_bindir]] &&     
          [file executable $top_bindir]} {   
          global CMD_NAME top_bindir
          set CMD_NAME $top_bindir
      } 
   }
  
   # If we don't have a CMD_NAME, wipeout
   if {[string match $CMD_NAME ""]} {
      global top_bindir test_binary_name
      puts "Error: $test_binary_name, is not in the expected place or is not a working binary."
      puts "Tried $top_bindir/src/$test_binary_name/$test_binary_name and $top_bindir"
      return 0
   }
} else {
   # OK, top_bindir is empty and we're on our own for a binary directory.  
   # Check ../../src/$binaryname/$binaryname first, since both in and out of source 
   # builds should have (for example) mged located there.  If that fails, check 
   # $top_srcdir/src/$binaryname/$binaryname, which could be different if top_srcdir 
   # is not equivalent to ../../ - if THAT fails, stop
   #
   global CMD_NAME top_srcdir test_binary_name
   set candidate_name ../../src/$test_binary_name/$test_binary_name
   if {[file executable $candidate_name] && ![file isdirectory $candidate_name]} {
      global CMD_NAME candidate_name
      set CMD_NAME $candidate_name
   } else {
      global candidate_name CMD_NAME top_srcdir test_binary_name
      set candidate_name $top_srcdir/src/$test_binary_name/$test_binary_name
      if {[file executable $candidate_name] && ![file isdirectory $candidate_name]} {
            global CMD_NAME candidate_name
            set CMD_NAME $candidate_name
      }
  } 
   # If we don't have a CMD_NAME, wipeout
   if {[string match $CMD_NAME ""]} {
      global top_srcdir test_binary_name
      puts "Error: $test_binary_name, is not in the expected place or is not a working binary."
      puts "Tried ../../src/$test_binary_name/$test_binary_name, $top_srcdir/src/$test_binary_name/$test_binary_name"
      return 0
   }
}

file delete mged.g mged.log mged.mged

proc add_test {cmdname {testfilerootname ""}} {
     global top_srcdir
     global test_binary_name
     set testfilename ""
     if {[string match $testfilerootname ""]} {
        set testfilename [format ./%s_test.%s $cmdname $test_binary_name]
     } else {
        set testfilename [format ./%s_test.%s $testfilerootname $test_binary_name]
     }
     set testfile [open $testfilename a]
     puts $testfile "source [format %s/regress/%s/regression_resources.tcl $top_srcdir $test_binary_name]"
     set inputtestfile [open [format %s/regress/%s/%s.%s $top_srcdir $test_binary_name $cmdname $test_binary_name] r]
     while {[gets $inputtestfile line] >= 0} {
        puts $testfile $line
     }
     close $testfile
     close $inputtestfile
}

proc run_test {cmdname {testfilename ""}} {
     global CMD_NAME
     global test_binary_name
     if {[string match $testfilename ""]} {set testfilename [format %s_test.%s $cmdname $test_binary_name]}
     if {[file exists $testfilename]} {
        exec $CMD_NAME -c [format %s.g $cmdname] < $testfilename >>& [format %s.log $cmdname]
     } else {
        add_test $cmdname 
        exec $CMD_NAME -c [format %s.g $cmdname] < $testfilename >>& [format %s.log $cmdname]
        file delete [format %s_test.mged $cmdname]
     }
}

#
#	GEOMETRIC INPUT COMMANDS
#
add_test in mged
add_test make mged
add_test 3ptarb mged
add_test arb mged
add_test comb mged
add_test g mged
add_test r mged
add_test make_bb mged
add_test cp mged
add_test cpi mged
add_test mv mged
add_test mvall mged
add_test build_region mged
add_test clone mged
add_test prefix mged
add_test mirror mged

#
#	DISPLAYING GEOMETRY - COMMANDS
#

run_test mged
