#!/bin/sh
#                    B R L M A N . T C L
# BRL-CAD
#
# Copyright (c) 2006-2012 United States Government as represented by
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
#
# Start-up script for both the command line and graphical modes of
# brlman
#

# The trailing backslash forces tcl to skip the next line \
STARTUP_HOME=`dirname $0`/..
#\
export STARTUP_HOME
# restart using btclsh \
TCLSH="btclsh"
#\
for clsh in btclsh btclsh_d ; do
# see if we're installed \
    if test -f ${STARTUP_HOME}/bin/$clsh ; then
#\
	TCLSH="${STARTUP_HOME}/bin/$clsh"
#\
	break;
#\
    fi
# see if we're not installed yet \
    if test -f ${STARTUP_HOME}/btclsh/$clsh ; then
#\
	TCLSH="${STARTUP_HOME}/btclsh/$clsh"
#\
	break;
#\
    fi
#\
done
#\
exec "$TCLSH" "$0" "$@"

#
# Begin Tcl here!
#

# Be systematic about extensions to check for each possible man section.
set manext(0) {0}
set manext(1) {1}
set manext(2) {2}
set manext(3) {3}
set manext(4) {4}
set manext(5) {5}
set manext(6) {6}
set manext(7) {7}
set manext(8) {8}
set manext(9) {9}
set manext(n) {n nged}

# Set up the options we support with rtwizard.  We will use
# an associative array named wizard_state in the RtWizard
# namespace to hold the key information - have getopt place the
# results of its parsing directly in that array
package require GetOpt
getopt::init {
        {gui 			"g"  	{::enable_gui}}
        {no-gui 		""  	{::disable_gui}}
        {language               "L"     {::have_lang ::man_language}}
}

# Perform the actual option parsing
if {[info exists argv]} {
  set argv2 [getopt::getopt $argv]
  set argv ""
  if {! [info exists argv2]} {
    # Exit if no args are provided
    puts stderr "Error - please specify manual page"
    if {[info exists argv]} {exit}
  } else {
    if {[llength $argv2] == 0} {
       puts stderr "Error - please specify manual page"
       if {[info exists argv]} {exit}
    }
  }
} 

# Default to English
if {![info exists ::man_language]} {set ::man_language "en"}

if {[llength $argv2] > 2} {
   puts stderr "Usage - brlman \[section \#\] name"
   if {[info exists argv]} {exit}
}

# If we have a section number, grab it
if {[llength $argv2] == 2} {
   # Count how many potentially valid section numbers we have
   set numcount 0
   foreach manarg $argv2 {
     if {[string is integer -strict "$manarg"] || $manarg == "n"} {incr numcount} 
   }
   if {$numcount == 0} {
      puts stderr "Usage - brlman \[section \#\] name"
      if {[info exists argv]} {exit}
   }
   if {$numcount == 1} {
      if {[string is integer -strict [lindex $argv2 0]] || [lindex $argv2 0] == "n"} {
         set section_number [lindex $argv2 0]
         set argv2 [lreplace $argv2 0 0]
      } else {
         set section_number [lindex $argv2 1]
         set argv2 [lreplace $argv2 1 1]
      }
   }
   if {$numcount == 2} {set section_number [lindex $argv2 1]}
}

# If we have both gui and no-gui specified, use gui
if {[info exists ::use_gui] && [info exists ::disable_gui]} {
   puts stderr "Warning - both -gui and -no-gui supplied - enabling gui"
   unset ::disable_gui
}

# Locate the file specified.
if {[info exists ::enable_gui]} {
   if {[info exists section_number]} {
      if {[file exists [file join [bu_brlcad_data doc/html/man$section_number] $::man_language $argv2.html]]} {
        set man_file [file join [bu_brlcad_data doc/html/man$section_number] $::man_language $argv2.html]
      } else {
        puts stderr "No man page found for $argv2 in section $section_number"
        if {[info exists argv]} {exit}
      }
   } else {
      foreach i {1 3 5 n} {
         if {![info exists man_file] && [file exists [file join [bu_brlcad_data doc/html/man$i] $::man_language $argv2.html]]} {
            set man_file [file join [bu_brlcad_data doc/html/man$i] $::man_language $argv2.html]
         }
      }
      if {! [info exists man_file]} {
        puts stderr "No man page found for $argv2"
        if {[info exists argv]} {exit}
      }
   }
} else {
   if {[info exists section_number]} {
      foreach extension $manext($section_number) {
         if {![info exists man_file] && [file exists [file join [bu_brlcad_data man/man$section_number] $argv2.$extension]]} {
           set man_file [file join [bu_brlcad_data man/man$section_number] $argv2.$extension]
         }
      }
      if {![info exists man_file]} {
        puts stderr "No man page found for $argv2 in section $section_number"
        if {[info exists argv]} {exit}
      }
   } else {
      foreach i {1 3 5 n} {
         foreach extension $manext($i) {
            if {![info exists man_file] && [file exists [file join [bu_brlcad_data man/man$i] $argv2.$extension]]} {
               set man_file [file join [bu_brlcad_data man/man$i] $argv2.$extension]
            }
         }
      }
      if {! [info exists man_file]} {
        puts stderr "No man page found for $argv2"
        if {[info exists argv]} {exit}
      }
   }
}


if {[info exists ::enable_gui]} {
  puts "Graphical mode done."
} else {
  puts $man_file
}

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
