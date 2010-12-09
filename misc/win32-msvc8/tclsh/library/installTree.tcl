#                      I N S T A L L T R E E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2010 United States Government as represented by
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
#  Script for building the install tree on Windows.
#

# boolean to control whether to print additional debugging information
set verbose 0


set rootDir [file normalize ../../..]
if {![file exists $rootDir]} {
  puts "$rootDir must exist and must be the root of the BRL-CAD source tree. "
  return
}


set platform [lindex $argv 0]
if {$platform == "x64"} {
  set brlcadInstall "brlcadInstallx64"
} else {
  set brlcadInstall brlcadInstall
}


set installDir [file join $rootDir $brlcadInstall]
if {[file exists $installDir]} {
  puts "$installDir already exists."
}


# Look for required source files
set missingFile 0
if {![file exists [file join $rootDir src other iwidgets iwidgets.tcl.in]]} {
  puts "ERROR: Missing [file join $rootDir src other iwidgets iwidgets.tcl.in]"
  set missingFile 1
}


if {![file exists [file join $rootDir src other iwidgets pkgIndex.tcl.in]]} {
  puts "ERROR: Missing [file join $rootDir src other iwidgets pkgIndex.tcl.in]"
  set missingFile 1
}


if {![file exists [file join $rootDir src other tk win wish.exe.manifest].in]} {
  puts "ERROR: Missing [file join $rootDir src other tk win wish.exe.manifest].in]"
  set missingFile 1
}


if {![file exists [file join $rootDir include conf MAJOR]]} {
  puts "ERROR: Missing [file join $rootDir include conf MAJOR]]"
  set missingFile 1
}


if {![file exists [file join $rootDir include conf MINOR]]} {
  puts "ERROR: Missing [file join $rootDir include conf MINOR]]"
  set missingFile 1
}


if {![file exists [file join $rootDir include conf PATCH]]} {
  puts "ERROR: Missing [file join $rootDir include conf PATCH]]"
  set missingFile 1
}


if {$missingFile} {
  return
}


# Set Tcl related versions
set tclVersion "8.5"
set itclVersion "3.4"
set iwidgetsVersion "4.0.2"

# Setup BRL-CAD's version
set fd [open [file join $rootDir include conf MAJOR] "r"]
set major [read $fd]
close $fd

set fd [open [file join $rootDir include conf MINOR] "r"]
set minor [read $fd]
close $fd

set fd [open [file join $rootDir include conf PATCH] "r"]
set patch [read $fd]
close $fd

set major [string trim $major]
set minor [string trim $minor]
set patch [string trim $patch]

if {![string is int $major]
    || ![string is int $minor]
    || ![string is int $patch]} {
  puts "Failed to acquire BRL-CAD's version."
  puts "Bad value for one or more of the following:"
  puts "major - $major, minor - $minor, patch - $patch"
  return
}


set cadVersion "$major.$minor.$patch"
set shareDir [file join $installDir share brlcad $cadVersion]
# End BRL-CAD's version setup

proc copy_stuff {from_arg to_arg} {
  global verbose
  if {$verbose} {puts "file copy -force $from_arg $to_arg"}
  file copy -force $from_arg $to_arg
  if {![file exists $to_arg]} { puts "ERROR: copying $from_arg failed"}
}

proc stub_dir {dir_arg} {
  global verbose
  if {$verbose} { puts "Creating $dir_arg" }
  file mkdir $dir_arg
}

puts "\[01 of 15] SETTING UP AUTO_PATH FOR TCLSH"
catch {
  lappend auto_path [file join $rootDir src other tcl library]
  lappend auto_path [file join $rootDir misc win32-msvc8 tclsh library]
  copy_stuff [file join $rootDir src other incrTcl itcl library itcl.tcl] [file join $rootDir misc win32-msvc8 tclsh library]
  copy_stuff [file join $rootDir src other incrTcl itcl library pkgIndex.tcl] [file join $rootDir misc win32-msvc8 tclsh library]
  set argv ""
  source "[file join $rootDir src tclscripts ami.tcl]"
  package require Itcl
}


puts "\[02 of 15] CREATING INSTALL DIRECTORIES"
catch {
  # stub_dir [file join $shareDir pix]
  stub_dir [file join $installDir bin]
  stub_dir [file join $installDir lib Tkhtml3.0]
  stub_dir [file join $installDir lib Tktable2.10]
  stub_dir [file join $installDir lib Tktable2.10 html]
  stub_dir [file join $installDir lib iwidgets$iwidgetsVersion]
  stub_dir [file join $installDir lib]
  stub_dir [file join $installDir share brlcad]
  stub_dir [file join $installDir share]
  stub_dir [file join $installDir]
  stub_dir [file join $shareDir db]
  stub_dir [file join $shareDir doc]
  stub_dir [file join $shareDir nirt]
  stub_dir [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]
  stub_dir [file join $shareDir plugins archer Utility]
  stub_dir [file join $shareDir plugins archer Wizards tankwizard]
  stub_dir [file join $shareDir plugins archer Wizards tirewizard]
  stub_dir [file join $shareDir plugins archer Wizards]
  stub_dir [file join $shareDir plugins archer]
  stub_dir [file join $shareDir plugins]
  stub_dir [file join $shareDir sample_applications]
  stub_dir [file join $shareDir]
}


puts "\[03 of 15] COPYING ICONS TO INSTALL DIRECTORY"
catch {
  copy_stuff [file join $rootDir doc html manuals archer archer.ico] [file join $installDir]
  copy_stuff [file join $rootDir misc nsis brlcad.ico] [file join $installDir]
}


puts "\[04 of 15] COPYING APPS TO BIN DIRECTORY"
catch {
  copy_stuff [file join $rootDir src archer archer.bat] [file join $installDir bin]
  copy_stuff [file join $rootDir src archer archer] [file join $installDir bin]
  copy_stuff [file join $rootDir src mged mged.bat] [file join $installDir bin]
  copy_stuff [file join $rootDir src tclscripts rtwizard rtwizard.bat] [file join $installDir bin]
}


puts "\[05 of 15] COPYING HEADER FILES TO INCLUDE DIRECTORY"
catch {
  copy_stuff [file join $rootDir include] $installDir
}


puts "\[06 of 15] COPYING LIBRARIES TO LIB DIRECTORY"
catch {
  copy_stuff [file join $rootDir src other incrTcl itcl library] [file join $installDir lib itcl$itclVersion]
  copy_stuff [file join $rootDir src other incrTcl itk library] [file join $installDir lib itk$itclVersion]
  copy_stuff [file join $rootDir src other iwidgets generic] [file join $installDir lib iwidgets$iwidgetsVersion scripts]
  copy_stuff [file join $rootDir src other tcl library] [file join $installDir lib tcl$tclVersion]
  copy_stuff [file join $rootDir src other tk library] [file join $installDir lib tk$tclVersion]
}


puts "\[07 of 15] COPYING DATA TO SHARE DIRECTORY"
catch {
  #copy_stuff [file join $rootDir doc] [file join $shareDir]
  copy_stuff [file join $rootDir AUTHORS] [file join $shareDir]
  copy_stuff [file join $rootDir COPYING] [file join $shareDir]
  copy_stuff [file join $rootDir HACKING] [file join $shareDir]
  copy_stuff [file join $rootDir INSTALL] [file join $shareDir]
  copy_stuff [file join $rootDir NEWS] [file join $shareDir]
  copy_stuff [file join $rootDir README] [file join $shareDir]
  copy_stuff [file join $rootDir doc archer_ack.txt] [file join $shareDir doc]
  copy_stuff [file join $rootDir doc html] [file join $shareDir]
  copy_stuff [file join $rootDir misc fortran_example.f] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP AttrGroupsDisplayUtilityP.tcl] [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]
  copy_stuff [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP.tcl] [file join $shareDir plugins archer Utility]
  copy_stuff [file join $rootDir src archer plugins Wizards tankwizard TankWizard.tcl] [file join $shareDir plugins archer Wizards tankwizard]
  copy_stuff [file join $rootDir src archer plugins Wizards tankwizard.tcl] [file join $shareDir plugins archer Wizards]
  copy_stuff [file join $rootDir src archer plugins Wizards tirewizard TireWizard.tcl] [file join $shareDir plugins archer Wizards tirewizard]
  copy_stuff [file join $rootDir src archer plugins Wizards tirewizard.tcl] [file join $shareDir plugins archer Wizards]
  copy_stuff [file join $rootDir src archer plugins utility.tcl] [file join $shareDir plugins archer]
  copy_stuff [file join $rootDir src archer plugins wizards.tcl] [file join $shareDir plugins archer]
  copy_stuff [file join $rootDir src conv g-xxx.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src conv g-xxx_facets.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src gtools g_transfer.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src libpkg tpkg.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src librt nurb_example.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src librt primitives xxx xxx.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src librt raydebug.tcl] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src nirt sfiles] [file join $shareDir nirt]
  copy_stuff [file join $rootDir src rt rtexample.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src tclscripts] [file join $shareDir]
  copy_stuff [file join $rootDir src util pl-dm.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src util roots_example.c] [file join $shareDir sample_applications]
  copy_stuff [file join $rootDir src vfont] [file join $shareDir]
}


puts "\[08 of 15] CREATING iwidgets.tcl"
catch {
  set fd1 [open [file join $rootDir src other iwidgets iwidgets.tcl.in] r]
  set lines [read $fd1]
  close $fd1
  if {$verbose} { puts "Creating [file join $installDir lib iwidgets$iwidgetsVersion iwidgets.tcl]" }
  set fd2 [open [file join $installDir lib iwidgets$iwidgetsVersion iwidgets.tcl] w]
  set lines [regsub -all {@ITCL_VERSION@} $lines $itclVersion]
  set lines [regsub -all {@IWIDGETS_VERSION@} $lines $iwidgetsVersion]
  puts $fd2 $lines
  close $fd2
}


puts "\[09 of 15] CREATING pkgIndex.tcl FOR iwidgets"
catch {
  set fd1 [open [file join $rootDir src other iwidgets pkgIndex.tcl.in] r]
  set lines [read $fd1]
  close $fd1
  if {$verbose} { puts "Creating [file join $installDir lib iwidgets$iwidgetsVersion pkgIndex.tcl]" }
  set fd2 [open [file join $installDir lib iwidgets$iwidgetsVersion pkgIndex.tcl] w]
  set lines [regsub -all {@IWIDGETS_VERSION@} $lines $iwidgetsVersion]
  puts $fd2 $lines
  close $fd2
}


puts "\[10 of 15] CREATING wish.exe.manifest"
catch {
  set fd1 [open [file join $rootDir src other tk win wish.exe.manifest].in r]
  set lines [read $fd1]
  close $fd1
  if {$verbose} { puts "Creating [file join $rootDir src other tk win wish.exe.manifest]" }
  set fd2 [open [file join $rootDir src other tk win wish.exe.manifest] w]
  set lines [regsub -all {@TK_WIN_VERSION@} $lines $tclVersion]
  set lines [regsub -all {@MACHINE@} $lines "x86"]
  puts $fd2 $lines
  close $fd2
}


puts "\[11 of 15] CREATING tclIndex FILES"
catch {
  make_tclIndex [list [file join $shareDir tclscripts]]
  make_tclIndex [list [file join $shareDir tclscripts lib]]
  make_tclIndex [list [file join $shareDir tclscripts archer]]
  make_tclIndex [list [file join $shareDir tclscripts mged]]
  make_tclIndex [list [file join $shareDir tclscripts rtwizard]]
  make_tclIndex [list [file join $shareDir tclscripts util]]
}


# FIXME: MUCH MORE NEEDED


puts "\[12 of 15] COPYING REDIST FILES"
catch {
    if {[info exists "C:/Program Files/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.CRT"]} {
	copy_stuff "C:/Program Files/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.CRT" [file join $installDir bin]
	copy_stuff "C:/Program Files/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.MFC" [file join $installDir bin]
    } else {
	copy_stuff "C:/Program Files (x86)/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.CRT" [file join $installDir bin]
	copy_stuff "C:/Program Files (x86)/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.MFC" [file join $installDir bin]
    }
}


puts "\[13 of 15] CREATING AND COPYING tkhtml SOURCE FILES"
catch {
  set savepwd [pwd]
  cd [file join $rootDir src other tkhtml src]
  if {$verbose} { puts "... creating htmltokens files" }
  source tokenlist.txt
  if {$verbose} { puts "... creating cssprop files" }
  source cssprop.tcl
  if {$verbose} { puts "... creating htmldefaultstyle.c" }
  exec tclsh mkdefaultstyle.tcl > htmldefaultstyle.c
  if {$verbose} { puts "... creating pkgIndex.tcl" }
  set fd [open pkgIndex.tcl "w"]
  puts $fd {package ifneeded Tkhtml 3.0 [list load [file join $dir tkhtml.dll]]}
  close $fd
  cd $savepwd
  copy_stuff [file join $rootDir src other tkhtml src pkgIndex.tcl] [file join $installDir lib Tkhtml3.0]
}


puts "\[14 of 15] CREATING tkTable.tcl.h and pkgIndex.tcl"
catch {
    set tktabledir [file join $rootDir src other tktable]
    set fd1 [open [file join $tktabledir library tkTable.tcl] r]
    set lines [split [read $fd1] "\n"]
    close $fd1
    set fd2 [open [file join $tktabledir tkTable.tcl.h] w]

    foreach line $lines {
	switch -regexp -- $line "^$$" - {^#} continue
	    regsub -all {\\} $line {\\\\} line
	    regsub -all {\"} $line {\"} line
	    puts $fd2 "\"$line\\n\""
    }

    close $fd2

    if {$verbose} { puts "... creating pkgIndex.tcl" }
    set fd [open [file join $tktabledir pkgIndex.tcl] "w"]
    puts $fd {if {[catch {package require Tcl 8.2}]} return}
    puts $fd {package ifneeded Tktable 2.10 [list load [file join $dir tktable.dll] Tktable]}
    close $fd

    copy_stuff [file join $tktabledir pkgIndex.tcl] [file join $installDir lib Tktable2.10]
    copy_stuff [file join $tktabledir library tkTable.tcl] [file join $installDir lib Tktable2.10]
    copy_stuff [file join $tktabledir doc tkTable.html] [file join $installDir lib Tktable2.10 html]
    copy_stuff [file join $tktabledir license.txt] [file join $installDir lib Tktable2.10]
}


proc removeUnwanted {_startDir} {
  global verbose
  set savepwd [pwd]
  cd $_startDir
  set deleted 0

  set files [glob -nocomplain -type f *]
  set hfiles [glob -nocomplain -type {f hidden} *]
  eval lappend files $hfiles
  set files [lsort -unique $files]

  set dirs [glob -nocomplain -type d *]
  set hdirs [glob -nocomplain -type {d hidden} *]
  eval lappend dirs $hdirs
  set dirs [lsort -unique $dirs]

  foreach file $files {
    if { [regexp {Makefile.*} $file] || [regexp {~$} $file] } {
      if {$verbose} {
        puts "... deleting $_startDir/$file"
      } else {
        puts -nonewline "."
        flush stdout
        incr deleted
      }
      catch {
        file delete -force $file
      }
    }
  }

  foreach dir $dirs {
    if { $dir == "." || $dir == ".." } {
      continue
    } elseif { $dir == ".libs" || $dir == ".deps" || $dir == ".svn" } {
      if {$verbose} {
        puts "... deleting $_startDir/$dir"
      } else {
        puts -nonewline "."
        flush stdout
        incr deleted
      }
      catch {
        file delete -force $dir
      }
    } else {
      # recursively walk hierarchy
      removeUnwanted $dir
    }
  }

  if {!$verbose && $deleted} {
    puts "\n"
    flush stdout
  }

  cd $savepwd
}


# Remove unwanted directories/files as a result of wholesale copies
puts "\[15 of 15] REMOVING UNWANTED FILES FROM $installDir"
removeUnwanted $installDir
