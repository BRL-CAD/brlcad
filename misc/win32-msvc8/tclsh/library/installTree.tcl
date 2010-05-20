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


puts "\[01 of 14] SETTING UP AUTO_PATH FOR TCLSH"
catch {
  lappend auto_path [file join $rootDir src other tcl library]
  lappend auto_path [file join $rootDir misc win32-msvc8 tclsh library]
  file copy -force [file join $rootDir src other incrTcl itcl library itcl.tcl] [file join $rootDir misc win32-msvc8 tclsh library]
  file copy -force [file join $rootDir src other incrTcl itcl library pkgIndex.tcl] [file join $rootDir misc win32-msvc8 tclsh library]
  set argv ""
  source "[file join $rootDir src tclscripts ami.tcl]"
  package require Itcl
}


puts "\[02 of 14] CREATING INSTALL DIRECTORIES"
catch {
  if {$verbose} { puts "Creating [file join $installDir]" }
  file mkdir [file join $installDir]
  if {$verbose} { puts "Creating [file join $installDir bin]" }
  file mkdir [file join $installDir bin]
  if {$verbose} { puts "Creating [file join $installDir bin Tkhtml3.0]" }
  file mkdir [file join $installDir bin Tkhtml3.0]
  if {$verbose} { puts "Creating [file join $installDir lib]" }
  file mkdir [file join $installDir lib]
  if {$verbose} { puts "Creating [file join $installDir lib iwidgets$iwidgetsVersion]" }
  file mkdir [file join $installDir lib iwidgets$iwidgetsVersion]
  if {$verbose} { puts "Creating [file join $installDir share]" }
  file mkdir [file join $installDir share]
  if {$verbose} { puts "Creating [file join $installDir share brlcad]" }
  file mkdir [file join $installDir share brlcad]
  if {$verbose} { puts "Creating [file join $shareDir]" }
  file mkdir [file join $shareDir]
  if {$verbose} { puts "Creating [file join $shareDir plugins]" }
  file mkdir [file join $shareDir plugins]
  if {$verbose} { puts "Creating [file join $shareDir plugins archer]" }
  file mkdir [file join $shareDir plugins archer]
  if {$verbose} { puts "Creating [file join $shareDir plugins archer Utility]" }
  file mkdir [file join $shareDir plugins archer Utility]
  if {$verbose} { puts "Creating [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]" }
  file mkdir [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]
  if {$verbose} { puts "Creating [file join $shareDir plugins archer Wizards]" }
  file mkdir [file join $shareDir plugins archer Wizards]
  if {$verbose} { puts "Creating [file join $shareDir plugins archer Wizards tankwizard]" }
  file mkdir [file join $shareDir plugins archer Wizards tankwizard]
  if {$verbose} { puts "Creating [file join $shareDir plugins archer Wizards tirewizard]" }
  file mkdir [file join $shareDir plugins archer Wizards tirewizard]
  if {$verbose} { puts "Creating [file join $shareDir db]" }
  file mkdir [file join $shareDir db]
  if {$verbose} { puts "Creating [file join $shareDir doc]" }
  file mkdir [file join $shareDir doc]
  #if {$verbose} { puts "Creating [file join $shareDir pix]" }
  #file mkdir [file join $shareDir pix]
  if {$verbose} { puts "Creating [file join $shareDir sample_applications]" }
  file mkdir [file join $shareDir sample_applications]
}


puts "\[03 of 14] COPYING ICONS TO INSTALL DIRECTORY"
catch {
  if {$verbose} { puts "copy -force [file join $rootDir doc html manuals archer archer.ico] [file join $installDir]" }
  file copy -force [file join $rootDir doc html manuals archer archer.ico] [file join $installDir]
  if {$verbose} { puts "copy -force [file join $rootDir misc nsis brlcad.ico] [file join $installDir]" }
  file copy -force [file join $rootDir misc nsis brlcad.ico] [file join $installDir]
}


puts "\[04 of 14] COPYING APPS TO BIN DIRECTORY"
catch {
  if {$verbose} { puts "copy -force [file join $rootDir src archer archer] [file join $installDir bin]" }
  file copy -force [file join $rootDir src archer archer] [file join $installDir bin]
  if {$verbose} { puts "copy -force [file join $rootDir src archer archer.bat] [file join $installDir bin]" }
  file copy -force [file join $rootDir src archer archer.bat] [file join $installDir bin]
  if {$verbose} { puts "copy -force [file join $rootDir src mged mged.bat] [file join $installDir bin]" }
  file copy -force [file join $rootDir src mged mged.bat] [file join $installDir bin]
  if {$verbose} { puts "copy -force [file join $rootDir src tclscripts rtwizard rtwizard.bat] [file join $installDir bin]" }
  file copy -force [file join $rootDir src tclscripts rtwizard rtwizard.bat] [file join $installDir bin]
}


puts "\[05 of 14] COPYING HEADER FILES TO INCLUDE DIRECTORY"
catch {
  file copy -force [file join $rootDir include] $installDir
}


puts "\[06 of 14] COPYING LIBRARIES TO LIB DIRECTORY"
catch {
  if {$verbose} { puts "copy -force [file join $rootDir src other tcl library] [file join $installDir lib tcl$tclVersion]" }
  file copy -force [file join $rootDir src other tcl library] [file join $installDir lib tcl$tclVersion]
  if {$verbose} { puts "copy -force [file join $rootDir src other tk library] [file join $installDir lib tk$tclVersion]" }
  file copy -force [file join $rootDir src other tk library] [file join $installDir lib tk$tclVersion]
  if {$verbose} { puts "copy -force [file join $rootDir src other incrTcl itcl library] [file join $installDir lib itcl$itclVersion]" }
  file copy -force [file join $rootDir src other incrTcl itcl library] [file join $installDir lib itcl$itclVersion]
  if {$verbose} { puts "copy -force [file join $rootDir src other incrTcl itk library] [file join $installDir lib itk$itclVersion]" }
  file copy -force [file join $rootDir src other incrTcl itk library] [file join $installDir lib itk$itclVersion]
  if {$verbose} { puts "copy -force [file join $rootDir src other iwidgets generic] [file join $installDir lib iwidgets$iwidgetsVersion]" }
  file copy -force [file join $rootDir src other iwidgets generic] [file join $installDir lib iwidgets$iwidgetsVersion scripts]
}


puts "\[07 of 14] COPYING DATA TO SHARE DIRECTORY"
catch {
  if {$verbose} { puts "copy -force [file join $rootDir AUTHORS] [file join $shareDir]" }
  file copy -force [file join $rootDir AUTHORS] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir COPYING] [file join $shareDir]" }
  file copy -force [file join $rootDir COPYING] [file join $shareDir]
  #if {$verbose} { puts "copy -force [file join $rootDir doc] [file join $shareDir]" }
  #file copy -force [file join $rootDir doc] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir doc archer_ack.txt] [file join $shareDir doc]" }
  file copy -force [file join $rootDir doc archer_ack.txt] [file join $shareDir doc]
  if {$verbose} { puts "copy -force [file join $rootDir doc html] [file join $shareDir]" }
  file copy -force [file join $rootDir doc html] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir HACKING] [file join $shareDir]" }
  file copy -force [file join $rootDir HACKING] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir INSTALL] [file join $shareDir]" }
  file copy -force [file join $rootDir INSTALL] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir NEWS] [file join $shareDir]" }
  file copy -force [file join $rootDir NEWS] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir README] [file join $shareDir]" }
  file copy -force [file join $rootDir README] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir misc fortran_example.f] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir misc fortran_example.f] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src vfont] [file join $shareDir]" }
  file copy -force [file join $rootDir src vfont] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir src tclscripts] [file join $shareDir]" }
  file copy -force [file join $rootDir src tclscripts] [file join $shareDir]
  if {$verbose} { puts "copy -force [file join $rootDir src archer plugins utility.tcl] [file join $shareDir plugins archer]" }
  file copy -force [file join $rootDir src archer plugins utility.tcl] [file join $shareDir plugins archer]
  if {$verbose} { puts "copy -force [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP.tcl] [file join $shareDir plugins archer Utility]" }
  file copy -force [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP.tcl] [file join $shareDir plugins archer Utility]
  if {$verbose} { puts "copy -force [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP AttrGroupsDisplayUtilityP.tcl] [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]" }
  file copy -force [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP AttrGroupsDisplayUtilityP.tcl] [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]
  if {$verbose} { puts "copy -force [file join $rootDir src archer plugins wizards.tcl] [file join $shareDir plugins archer]" }
  file copy -force [file join $rootDir src archer plugins wizards.tcl] [file join $shareDir plugins archer]
  if {$verbose} { puts "copy -force [file join $rootDir src archer plugins Wizards tankwizard.tcl] [file join $shareDir plugins archer Wizards]" }
  file copy -force [file join $rootDir src archer plugins Wizards tankwizard.tcl] [file join $shareDir plugins archer Wizards]
  if {$verbose} { puts "copy -force [file join $rootDir src archer plugins Wizards tankwizard TankWizard.tcl] [file join $shareDir plugins archer Wizards tankwizard]" }
  file copy -force [file join $rootDir src archer plugins Wizards tankwizard TankWizard.tcl] [file join $shareDir plugins archer Wizards tankwizard]
  if {$verbose} { puts "copy -force [file join $rootDir src archer plugins Wizards tirewizard.tcl] [file join $shareDir plugins archer Wizards]" }
  file copy -force [file join $rootDir src archer plugins Wizards tirewizard.tcl] [file join $shareDir plugins archer Wizards]
  if {$verbose} { puts "copy -force [file join $rootDir src archer plugins Wizards tirewizard TireWizard.tcl] [file join $shareDir plugins archer Wizards tirewizard]" }
  file copy -force [file join $rootDir src archer plugins Wizards tirewizard TireWizard.tcl] [file join $shareDir plugins archer Wizards tirewizard]
  if {$verbose} { puts "copy -force [file join $rootDir src conv g-xxx.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src conv g-xxx.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src conv g-xxx_facets.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src conv g-xxx_facets.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src gtools g_transfer.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src gtools g_transfer.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src libpkg tpkg.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src libpkg tpkg.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src librt primitives xxx xxx.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src librt primitives xxx xxx.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src librt raydebug.tcl] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src librt raydebug.tcl] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src librt nurb_example.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src librt nurb_example.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src rt rtexample.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src rt rtexample.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src util pl-dm.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src util pl-dm.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src util roots_example.c] [file join $shareDir sample_applications]" }
  file copy -force [file join $rootDir src util roots_example.c] [file join $shareDir sample_applications]
  if {$verbose} { puts "copy -force [file join $rootDir src nirt sfiles] [file join $shareDir nirt]" }
  file copy -force [file join $rootDir src nirt sfiles] [file join $shareDir nirt]
}


puts "\[08 of 14] CREATING iwidgets.tcl"
catch {
  set fd1 [open [file join $rootDir src other iwidgets iwidgets.tcl.in] r]
  set lines [read $fd1]
  close $fd1
  if {$verbose} { puts "Creating [file join $installDir lib iwidgets$iwidgetsVersion iwidgets.tcl]" }
  set fd2 [open [file join $installDir lib iwidgets$iwidgetsVersion iwidgets.tcl] w]
  set lines [regsub -all {@ITCL_VERSION@} $lines $itclVersion]
  set lines [regsub -all {@IWIDGETS_VERSION@} $lines $iwidgetsVersion]
  if {$verbose} { puts $fd2 $lines }
  close $fd2
}


puts "\[09 of 14] CREATING pkgIndex.tcl FOR iwidgets"
catch {
  set fd1 [open [file join $rootDir src other iwidgets pkgIndex.tcl.in] r]
  set lines [read $fd1]
  close $fd1
  if {$verbose} { puts "Creating [file join $installDir lib iwidgets$iwidgetsVersion pkgIndex.tcl]" }
  set fd2 [open [file join $installDir lib iwidgets$iwidgetsVersion pkgIndex.tcl] w]
  set lines [regsub -all {@IWIDGETS_VERSION@} $lines $iwidgetsVersion]
  if {$verbose} { puts $fd2 $lines }
  close $fd2
}


puts "\[10 of 14] CREATING wish.exe.manifest"
catch {
  set fd1 [open [file join $rootDir src other tk win wish.exe.manifest].in r]
  set lines [read $fd1]
  close $fd1
  if {$verbose} { puts "Creating [file join $rootDir src other tk win wish.exe.manifest]" }
  set fd2 [open [file join $rootDir src other tk win wish.exe.manifest] w]
  set lines [regsub -all {@TK_WIN_VERSION@} $lines $tclVersion]
  set lines [regsub -all {@MACHINE@} $lines "x86"]
  if {$verbose} { puts $fd2 $lines }
  close $fd2
}


puts "\[11 of 14] CREATING tclIndex FILES"
catch {
  make_tclIndex [list [file join $shareDir tclscripts]]
  make_tclIndex [list [file join $shareDir tclscripts lib]]
  make_tclIndex [list [file join $shareDir tclscripts archer]]
  make_tclIndex [list [file join $shareDir tclscripts mged]]
  make_tclIndex [list [file join $shareDir tclscripts rtwizard]]
  make_tclIndex [list [file join $shareDir tclscripts util]]
}


# FIXME: MUCH MORE NEEDED


puts "\[12 of 14] COPYING REDIST FILES"
catch {
  file copy -force "C:/Program Files/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.CRT" [file join $installDir bin]
  file copy -force "C:/Program Files/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.MFC" [file join $installDir bin]
}


puts "\[13 of 14] CREATING AND COPYING tkhtml SOURCE FILES"
catch {
  set savepwd [pwd]
  cd [file join $rootDir src other tkhtml3 src]
  if {$verbose} { puts "... creating htmltokens files" }
  source tokenlist.txt
  if {$verbose} { puts "... creating cssprop files" }
  source cssprop.tcl
  if {$verbose} { puts "... creating htmldefaultstyle.c" }
  exec tclsh mkdefaultstyle.tcl > htmldefaultstyle.c
  if {$verbose} { puts "... creating pkgIndex.tcl" }
  set fd [open pkgIndex.tcl "w"]
  if {$verbose} { puts $fd {package ifneeded Tkhtml 3.0 [list load [file join $dir tkhtml.dll]]} }
  close $fd
  cd $savepwd
  if {$verbose} { puts "copy -force [file join $rootDir src other tkhtml3 src pkgIndex.tcl] [file join $installDir bin Tkhtml3.0]" }
  file copy -force [file join $rootDir src other tkhtml3 src pkgIndex.tcl] [file join $installDir bin Tkhtml3.0]
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
puts "\[14 of 14] REMOVING UNWANTED FILES FROM $installDir"
removeUnwanted $installDir
