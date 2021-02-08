#!/usr/bin/env tclsh
#                T E S T _ C H E C K E R . T C L
# BRL-CAD
#
# Copyright (c) 2017-2021 United States Government as represented by
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
###

# configuration
if {![info exists ::env(CHECKER_DIR)]} {
    puts stderr "Set CHECKER_DIR to directory containing check.sh and check.tcl."
    exit 1
}

set check_dir $::env(CHECKER_DIR)

set check_sh [file join [pwd] $check_dir check.sh]
set check_tcl [file join [pwd] $check_dir check.tcl]

set mged "mged"
set mged_opts "-a nu -c"

# generates a random string like "U6w"
proc rstring {} {
  set chars abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
  set nchars [string length $chars]

  set s ""
  for {set i 0} {$i < 3} {incr i} {
    set rchar [expr round(rand() * ($nchars - 1))]
    append s [string index $chars $rchar]
  }

  return $s
}

# makes a unique directory named something like "test_check_U6w"
# returns the directory name
proc tmp_dir {} {
  set dir ""
  for {set i 0} {$i < 10} {incr i} {
    set dir "test_check_[rstring]"

    if {![file exists $dir] && ![catch {file mkdir $dir}]} {
      return $dir
    }
  }
  return -code error "failed to create temporary directory"
}

proc write_file {file_name contents} {
  set sf [open $file_name w]
  puts $sf $contents
  close $sf
}

# Write a tcl script to a file and make an mged command to source it.
#
# returns a string like "mged -a nu -c test.g source <script_name>"
proc mged_cmd {script_name script} {
  upvar #0 mged mged
  upvar #0 mged_opts mged_opts
  upvar #0 testdb testdb

  write_file $script_name $script
  return "$mged $mged_opts $testdb source $script_name"
}

proc write_db {db_name commands} {
  upvar #0 mged mged
  upvar #0 mged_opts mged_opts

  write_file [set make_db_script "makedb.tcl"] $commands
  file delete -force $db_name
  exec $mged {*}$mged_opts $db_name source $make_db_script
}

# Return true if db_a and db_b differ according to gdiff, false
# otherwise.
proc dbs_differ {db_a db_b} {
  if {$db_a != $db_b && [catch {exec gdiff -v 0 $db_a $db_b} result]} {
    return true
  }
  return false
}

# Run <mged_script> in mged and ensure that the script succeeds
# (unless <expect_fail> is true), and that the new database state is
# the same as that of <expected_result_db>.
# 
# Writes to stdout "(PASS|FAIL): <description>".
# If the check fails, exits the script with an error code.
proc check_mged_result \
  {description mged_script expected_result_db {expect_fail false}} \
{
  upvar #0 testdb testdb

  set script_name "test_check.tcl"
  set cmd [mged_cmd $script_name $mged_script]

  set script_error [catch {exec {*}$cmd 2>@1} result]

  if {[expr $script_error && !$expect_fail] || \
      [dbs_differ $testdb $expected_result_db]} \
  {
    puts "FAIL: $description"
    puts "$result"
    file delete $script_name
    exit 1
  }

  puts "PASS: $description"
}

proc try_in_mged {description mged_script {expect_fail false}} {
  upvar #0 testdb expected_result_db

  check_mged_result $description $mged_script $expected_result_db $expect_fail
}

# Run <check_cmd> (defined in <check_tcl>):
# 1. without an overlap file
# 2. with a valid overlap file
# 3. with an invalid overlap file
# Expecting success or failure as specified by fail[123].
proc test_olfile_search {check_tcl check_cmd olfile description fail1 fail2 fail3} {
  try_in_mged "try $check_cmd without overlap file ($description)" \
    "source $check_tcl
     $check_cmd" \
     $fail1

  try_in_mged "try $check_cmd with valid overlap file ($description)" \
    "source $check_tcl
     $check_cmd $olfile" \
     $fail2

  try_in_mged "try $check_cmd with invalid overlap file ($description)" \
    "source $check_tcl
     $check_cmd xxx" \
     $fail3
}

# Make a copy of the tcl checker <script> with name <pub_name>,
# replacing "private" with "public" in the class interface, and having
# the check command write out the name of the instantiated checker
# object.
proc write_public_version {script pub_name} {
  set private [open $script r]
  set public [open $pub_name w]
  set subs 0
  set in_check false

  while {[gets $private line] != -1} {
    if {[set start [string first "private \{" $line]] != -1} {
      set end [expr $start + [string length "private \{"] - 1]
      set line [string replace $line $start $end "public \{"]
      incr subs
    }
    if {[string first "proc check" $line] != -1} {
      set in_check true
    }
    if {$in_check && [string first \} $line] == 0} {
      puts $public {    return $checker}
      incr subs
      set in_check false
    }
    puts $public $line
  }
  close $private
  close $public

  if {$subs != 2} {
    return -error
  }
  return $pub_name
}

# Use the public checker script <check_tcl> to capture the standard
# name of the instantiated checker object.
#
# Returns the name of the checker object.
proc checker_name {check_tcl} {
  upvar #0 mged mged
  upvar #0 mged_opts mged_opts
  upvar #0 testdb testdb

  # write checker object name to text file
  set name_tcl "checker_name.tcl"
  set name_txt "checker_name.txt"

  set write_name [mged_cmd $name_tcl \
    "source $check_tcl
    set glob_compat_mode 0
    set f \[open $name_txt w\]
    puts \$f \[check\]
    close \$f"]

  set err [catch {exec {*}$write_name} res]
  file delete $name_tcl

  if {$err} {
    return -code error "$res"
  }

  # read name from text file
  set f [open $name_txt r]
  set checker ""
  while {[gets $f line] != -1} {
    if {[string first "checker" $line] != -1} {
      set checker $line
      break
    }
  }
  close $f
  file delete $name_txt

  if {$checker == ""} {
    return -code error "couldn't get the name of the checker object"
  }
  return $checker
}

# Return an mged script that runs check with the given args, then
# selects the given overlap list item, and does subLeft or SubRight as
# specified.
proc subSelection {check_args selection_id sub_direction} {
  upvar #0 check_tcl check_tcl
  upvar #0 checker checker

  return "source $check_tcl
  set glob_compat_mode 0
  check $check_args
  set ck \[$checker component checkList\]
  \$ck selection set $selection_id
  $checker $sub_direction
  "
}

### MAIN

# check for required inputs
if {![file exists $check_tcl]} {
  puts stderr "check script \"$check_tcl\" doesn't exist"
  exit 1
}

if {![file exists $check_sh]} {
  puts stderr "check script \"$check_sh\" doesn't exist"
  exit 1
}

# operate in test directory on test databse with public version of
# check script
cd [set testdir [file join [pwd] [tmp_dir]]]

set pub_name [file join $testdir "public_[file tail $check_tcl]"]

if {[catch {write_public_version $check_tcl $pub_name} pub_check]} {
  puts stderr "couldn't rewrite \"$check_tcl\" with a public interface for testing"
  puts stderr $pub_check
  exit 1
}
set check_tcl $pub_check

## create initial test database
set testdb "test.g"

file mkdir [set oldir [file join $testdir ${testdb}.ck]]
set olfile [file join $oldir ck.${testdb}.overlaps]

set testdb [file join $testdir $testdb]

# definitions for overlapping spheres
set sph_a {in a.s sph -.155	-.426	.331	.5}
set sph_b {in b.s sph .383	-.305	.34	.5}
set sph_c {in c.s sph .59	-.354	.248	.5}

# definitions for non-overlapping spheres
set sph_d {in d.s sph -1.51	-1.23	0	.5}
set sph_e {in e.s sph 1.4	1.54	-1	.5}
set sph_f {in f.s sph 1.53	-1.64	1.22	.5}
set sph_g {in g.s sph -1.46	1.4	1.67	.5}

# instructions for creating initial test database
set make_solids "
  $sph_a
  $sph_b
  $sph_c
  $sph_d
  $sph_e
  $sph_f
  $sph_g"

set make_r1 "
  comb c1 u c.s - d.s + g.s - f.s - e.s u b.s
  comb c2 u c1
  r r1 u c2
"

set make_r2 "
  comb c3 u a.s
  comb c4 u c3
  comb c5 u c4
  r r2 u c5
"

set make_regions "
  $make_r1
  $make_r2
  comb all u r1 u r2
"

write_file [set make_db_script "makedb.tcl"] "
  $make_solids
  $make_regions
"

# instructions to create test overlap directory
write_file $olfile "/all/r1 /all/r2 183670.8684"

# try to create the db (need correct mged)
if {[catch {exec $mged {*}$mged_opts $testdb source $make_db_script}]} {
  puts stderr "can't run \"$mged $mged_opts\""
  exit 1
}


# need checker object name for tests
if {[catch {checker_name $check_tcl} checker]} {
  puts stderr $checker
  exit 1
}

# run tests
try_in_mged "source check.tcl in mged" \
    "source $check_tcl"

puts "== Check Command Argument Behavior =="
foreach flag {"" " -F"} {
  try_in_mged "check$flag doesn't run without a database" \
    "closedb
     source $check_tcl
     check$flag" \
     [set expect_fail true]
}

foreach flag {"" " -F"} {
  test_olfile_search $check_tcl "check$flag" $olfile "working dir" \
    false false true

  set tmpdir "tmp"
  file mkdir $tmpdir
  cd $tmpdir
  test_olfile_search $check_tcl "check$flag" $olfile "wrong dir" \
    true false true
  cd $testdir
  file delete -force $tmpdir
}

file copy $testdb [set testdb_bak "$testdb.bak"]

# Check behavior in simple random databases with a single overlap.
# Same solids in each, in different organizations such that r1
# overlaps r2.

# Random 1
puts "== Subtraction Behavior, DB 1 =="
write_db [set testdb_sub_r2 "test_sub_r2.g"] "
  $make_solids
  comb c1 u c.s - a.s - d.s + g.s - f.s - e.s u b.s - a.s
  comb c2 u c1
  r r1 u c2
  $make_r2
  comb all u r1 u r2
"

write_db [set testdb_sub_r2_F "test_sub_r2_F.g"] "
  $make_solids
  comb c1 u c.s - a.s - d.s + g.s - f.s - e.s u b.s
  comb c2 u c1
  r r1 u c2
  $make_r2
  comb all u r1 u r2
"

write_db [set testdb_sub_r1 "test_sub_r1.g"] "
  $make_solids
  $make_r1

  comb c3 u a.s - c.s
  comb c4 u c3
  comb c5 u c4
  r r2 u c5

  comb all u r1 u r2
"

# Should be able to subtract right r2 (simplifies to a.s) from r1 with
# or without -F.
check_mged_result "subtract one solid without -F" \
  [subSelection "" 1 subRight] $testdb_sub_r2

file copy -force $testdb_bak $testdb

check_mged_result "subtract one solid with -F" \
  [subSelection "-F" 1 subRight] $testdb_sub_r2_F

file copy -force $testdb_bak $testdb

# Should not be able to subtract r1 (recipe) from r2 without -F.
check_mged_result "don't subtract recipe without -F" \
  [subSelection "" 1 subLeft] $testdb [set expect_fail true]

check_mged_result "subtract first solid in recipe with -F" \
  [subSelection "-F" 1 subLeft] $testdb_sub_r1

# Random 2
puts "== Subtraction Behavior, DB 2 =="
write_db $testdb "
  $make_solids
  comb c1 u a.s
  r r1 u c1 - d.s
  r r2 u c.s
  comb all u r1 u r2
"
file copy -force $testdb $testdb_bak

write_file $olfile "/all/r1 /all/r2 103.8843"

write_db $testdb_sub_r2 "
  $make_solids
  comb c1 u a.s - c.s
  r r1 u c1 - d.s
  r r2 u c.s
  comb all u r1 u r2
"
write_db [set testdb_sub_r1_F "test_sub_r1_F.g"] "
  $make_solids
  comb c1 u a.s
  r r1 u c1 - d.s
  r r2 u c.s - a.s
  comb all u r1 u r2
"

# Should be able to subtract r2 (simplifies to c.s) with or without
# -F.
check_mged_result "subtract one solid without -F" \
  [subSelection "" 1 subRight] $testdb_sub_r2

file copy -force $testdb_bak $testdb

check_mged_result "subtract one solid with -F" \
  [subSelection "-F" 1 subRight] $testdb_sub_r2

file copy -force $testdb_bak $testdb

# Should not be able to subtract r1 (recipe) without -F.
check_mged_result "don't subtract recipe without -F" \
  [subSelection "" 1 subLeft] $testdb [set expect_fail true]

file copy -force $testdb_bak $testdb

check_mged_result "subtract first solid in recipe with -F" \
  [subSelection "-F" 1 subLeft] $testdb_sub_r1_F

# Random 3
puts "== Subtraction Behavior, DB 3 =="
write_db $testdb "
  $make_solids
  comb c1 u a.s u d.s
  comb c2 u c1
  r r1 u c1
  r r2 u e.s u b.s
  comb all u r1 u r2
"
file copy -force $testdb $testdb_bak

write_file $olfile "/all/r2 /all/r1 212.1077"

write_db $testdb_sub_r1_F "
  $make_solids
  comb c1 u a.s u d.s
  comb c2 u c1
  r r1 u c1
  r r2 u e.s - a.s u b.s
  comb all u r1 u r2
"

write_db $testdb_sub_r2_F "
  $make_solids
  comb c1 u a.s - e.s u d.s
  comb c2 u c1
  r r1 u c1
  r r2 u e.s u b.s
  comb all u r1 u r2
"

# shouldn't be able to subtract either side without -F
check_mged_result "dont subtract recipe without -F" \
  [subSelection "" 1 subLeft] $testdb [set expect_fail true]

file copy -force $testdb_bak $testdb

check_mged_result "dont subtract recipe without -F" \
  [subSelection "" 1 subRight] $testdb [set expect_fail true]

file copy -force $testdb_bak $testdb

# should be able to subtract either side with -F
check_mged_result "subtract first solid with -F" \
  [subSelection "-F" 1 subLeft] $testdb_sub_r1_F

file copy -force $testdb_bak $testdb

check_mged_result "subtract first solid with -F" \
  [subSelection "-F" 1 subRight] $testdb_sub_r2_F

file copy -force $testdb_bak $testdb

# cleanup
file delete -force $testdir
