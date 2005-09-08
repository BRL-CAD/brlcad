# all.tcl -- -*- tcl -*-
#
# Import common functionality, then run the tests in this directory.
#
# Copyright (c) 2002         Andreas Kupries <andreas_kupries@users.sourceforge.net>
# All rights reserved.
# 
# RCS: @(#) $Id$

set _pwd  [pwd]
cd  [file dirname [file join [pwd] [info script]]]
set _here [pwd]
cd $_pwd
source [file join [file dirname [file dirname $_here]] tests all.tcl]
unset _pwd _here

set ::tcltest::testSingleFile false
set ::tcltest::testsDirectory [file dirname [info script]]

# We need to ensure that the testsDirectory is absolute
::tcltest::normalizePath ::tcltest::testsDirectory

run_tests
return
