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
# Author(s):
#    Bob Parker
#
# Description:
#    Script for building the install tree on Windows.
#

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

# Set BRL-CAD's version
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

if {![string is int $major] ||
    ![string is int $minor] ||
    ![string is int $patch]} {
    puts "Failed to acquire BRL-CAD's version."
    puts "Bad value for one or more of the following:"
    puts "major - $major, minor - $minor, patch - $patch"
    return
}

set cadVersion "$major.$minor.$patch"
# End Set BRL-CAD's version

set shareDir [file join $installDir share brlcad $cadVersion]

# More initialization for tclsh
# Show tclsh where to find required tcl files
lappend auto_path [file join $rootDir src other tcl library]
lappend auto_path [file join $rootDir misc win32-msvc8 tclsh library]
file copy -force [file join $rootDir src other incrTcl itcl library itcl.tcl] [file join $rootDir misc win32-msvc8 tclsh library]
file copy -force [file join $rootDir src other incrTcl itcl library pkgIndex.tcl] [file join $rootDir misc win32-msvc8 tclsh library]
set argv ""
source "[file join $rootDir src tclscripts ami.tcl]"
package require Itcl

# End More initialization for tclsh


# Create install directories
puts "Creating [file join $installDir]"
file mkdir [file join $installDir]
puts "Creating [file join $installDir bin]"
file mkdir [file join $installDir bin]
puts "Creating [file join $installDir bin Tkhtml3.0]"
file mkdir [file join $installDir bin Tkhtml3.0]
puts "Creating [file join $installDir lib]"
file mkdir [file join $installDir lib]
puts "Creating [file join $installDir lib iwidgets$iwidgetsVersion]"
file mkdir [file join $installDir lib iwidgets$iwidgetsVersion]
puts "Creating [file join $installDir share]"
file mkdir [file join $installDir share]
puts "Creating [file join $installDir share brlcad]"
file mkdir [file join $installDir share brlcad]
puts "Creating [file join $shareDir]"
file mkdir [file join $shareDir]
puts "Creating [file join $shareDir plugins]"
file mkdir [file join $shareDir plugins]
puts "Creating [file join $shareDir plugins archer]"
file mkdir [file join $shareDir plugins archer]
puts "Creating [file join $shareDir plugins archer Utility]"
file mkdir [file join $shareDir plugins archer Utility]
puts "Creating [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]"
file mkdir [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]
puts "Creating [file join $shareDir plugins archer Wizards]"
file mkdir [file join $shareDir plugins archer Wizards]
puts "Creating [file join $shareDir plugins archer Wizards tankwizard]"
file mkdir [file join $shareDir plugins archer Wizards tankwizard]
puts "Creating [file join $shareDir plugins archer Wizards tirewizard]"
file mkdir [file join $shareDir plugins archer Wizards tirewizard]
puts "Creating [file join $shareDir db]"
file mkdir [file join $shareDir db]
puts "Creating [file join $shareDir doc]"
file mkdir [file join $shareDir doc]
#puts "Creating [file join $shareDir pix]"
#file mkdir [file join $shareDir pix]
puts "Creating [file join $shareDir sample_applications]"
file mkdir [file join $shareDir sample_applications]
# End Create install directories


# Copy files to install directory
puts "copy [file join $rootDir doc html manuals archer archer.ico] [file join $installDir]"
file copy [file join $rootDir doc html manuals archer archer.ico] [file join $installDir]
puts "copy [file join $rootDir misc nsis brlcad.ico] [file join $installDir]"
file copy [file join $rootDir misc nsis brlcad.ico] [file join $installDir]


# Copy files to the bin directory
puts "copy [file join $rootDir src archer archer] [file join $installDir bin]"
file copy [file join $rootDir src archer archer] [file join $installDir bin]
puts "copy [file join $rootDir src archer archer.bat] [file join $installDir bin]"
file copy [file join $rootDir src archer archer.bat] [file join $installDir bin]
puts "copy [file join $rootDir src mged mged.bat] [file join $installDir bin]"
file copy [file join $rootDir src mged mged.bat] [file join $installDir bin]
puts "copy [file join $rootDir src tclscripts rtwizard rtwizard.bat] [file join $installDir bin]"
file copy [file join $rootDir src tclscripts rtwizard rtwizard.bat] [file join $installDir bin]
# End Copy files to the bin directory


# Copy files to the include directory
file copy [file join $rootDir include] $installDir
# End Copy files to the include directory


# Copy files to the lib directory
puts "copy [file join $rootDir src other tcl library] [file join $installDir lib tcl$tclVersion]"
file copy [file join $rootDir src other tcl library] [file join $installDir lib tcl$tclVersion]
puts "copy [file join $rootDir src other tk library] [file join $installDir lib tk$tclVersion]"
file copy [file join $rootDir src other tk library] [file join $installDir lib tk$tclVersion]
puts "copy [file join $rootDir src other incrTcl itcl library] [file join $installDir lib itcl$itclVersion]"
file copy [file join $rootDir src other incrTcl itcl library] [file join $installDir lib itcl$itclVersion]
puts "copy [file join $rootDir src other incrTcl itk library] [file join $installDir lib itk$itclVersion]"
file copy [file join $rootDir src other incrTcl itk library] [file join $installDir lib itk$itclVersion]
puts "copy [file join $rootDir src other iwidgets generic] [file join $installDir lib iwidgets$iwidgetsVersion]"
file copy [file join $rootDir src other iwidgets generic] [file join $installDir lib iwidgets$iwidgetsVersion scripts]
# End Copy files to the lib directory


# Copy files to the share directories
puts "copy [file join $rootDir AUTHORS] [file join $shareDir]"
file copy [file join $rootDir AUTHORS] [file join $shareDir]
puts "copy [file join $rootDir COPYING] [file join $shareDir]"
file copy [file join $rootDir COPYING] [file join $shareDir]
#puts "copy [file join $rootDir doc] [file join $shareDir]"
#file copy [file join $rootDir doc] [file join $shareDir]
puts "copy [file join $rootDir doc archer_ack.txt] [file join $shareDir doc]"
file copy [file join $rootDir doc archer_ack.txt] [file join $shareDir doc]
puts "copy [file join $rootDir doc html] [file join $shareDir]"
file copy [file join $rootDir doc html] [file join $shareDir]
puts "copy [file join $rootDir HACKING] [file join $shareDir]"
file copy [file join $rootDir HACKING] [file join $shareDir]
puts "copy [file join $rootDir INSTALL] [file join $shareDir]"
file copy [file join $rootDir INSTALL] [file join $shareDir]
puts "copy [file join $rootDir NEWS] [file join $shareDir]"
file copy [file join $rootDir NEWS] [file join $shareDir]
puts "copy [file join $rootDir README] [file join $shareDir]"
file copy [file join $rootDir README] [file join $shareDir]
puts "copy [file join $rootDir misc fortran_example.f] [file join $shareDir sample_applications]"
file copy [file join $rootDir misc fortran_example.f] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src vfont] [file join $shareDir]"
file copy [file join $rootDir src vfont] [file join $shareDir]
puts "copy [file join $rootDir src tclscripts] [file join $shareDir]"
file copy [file join $rootDir src tclscripts] [file join $shareDir]
puts "copy [file join $rootDir src archer plugins utility.tcl] [file join $shareDir plugins archer]"
file copy [file join $rootDir src archer plugins utility.tcl] \
    [file join $shareDir plugins archer]
puts "copy [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP.tcl] [file join $shareDir plugins archer Utility]"
file copy [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP.tcl] \
    [file join $shareDir plugins archer Utility]
puts "copy [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP AttrGroupsDisplayUtilityP.tcl] [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]"
file copy [file join $rootDir src archer plugins Utility attrGroupsDisplayUtilityP AttrGroupsDisplayUtilityP.tcl] \
    [file join $shareDir plugins archer Utility attrGroupsDisplayUtilityP]
puts "copy [file join $rootDir src archer plugins wizards.tcl] [file join $shareDir plugins archer]"
file copy [file join $rootDir src archer plugins wizards.tcl] \
    [file join $shareDir plugins archer]
puts "copy [file join $rootDir src archer plugins Wizards tankwizard.tcl] [file join $shareDir plugins archer Wizards]"
file copy [file join $rootDir src archer plugins Wizards tankwizard.tcl] \
    [file join $shareDir plugins archer Wizards]
puts "copy [file join $rootDir src archer plugins Wizards tankwizard TankWizard.tcl] [file join $shareDir plugins archer Wizards tankwizard]"
file copy [file join $rootDir src archer plugins Wizards tankwizard TankWizard.tcl] \
    [file join $shareDir plugins archer Wizards tankwizard]
puts "copy [file join $rootDir src archer plugins Wizards tirewizard.tcl] [file join $shareDir plugins archer Wizards]"
file copy [file join $rootDir src archer plugins Wizards tirewizard.tcl] \
    [file join $shareDir plugins archer Wizards]
puts "copy [file join $rootDir src archer plugins Wizards tirewizard TireWizard.tcl] [file join $shareDir plugins archer Wizards tirewizard]"
file copy [file join $rootDir src archer plugins Wizards tirewizard TireWizard.tcl] \
    [file join $shareDir plugins archer Wizards tirewizard]
puts "copy [file join $rootDir src conv g-xxx.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src conv g-xxx.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src conv g-xxx_facets.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src conv g-xxx_facets.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src gtools g_transfer.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src gtools g_transfer.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src libpkg tpkg.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src libpkg tpkg.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src librt primitives xxx xxx.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src librt primitives xxx xxx.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src librt raydebug.tcl] [file join $shareDir sample_applications]"
file copy [file join $rootDir src librt raydebug.tcl] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src librt nurb_example.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src librt nurb_example.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src rt rtexample.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src rt rtexample.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src util pl-dm.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src util pl-dm.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src util roots_example.c] [file join $shareDir sample_applications]"
file copy [file join $rootDir src util roots_example.c] [file join $shareDir sample_applications]
puts "copy [file join $rootDir src nirt sfiles] [file join $shareDir nirt]"
file copy [file join $rootDir src nirt sfiles] [file join $shareDir nirt]
# End Copy files to the share directories


# Create iwidgets.tcl
set fd1 [open [file join $rootDir src other iwidgets iwidgets.tcl.in] r]
set lines [read $fd1]
close $fd1
puts "Creating [file join $installDir lib iwidgets$iwidgetsVersion iwidgets.tcl]"
set fd2 [open [file join $installDir lib iwidgets$iwidgetsVersion iwidgets.tcl] w]
set lines [regsub -all {@ITCL_VERSION@} $lines $itclVersion]
set lines [regsub -all {@IWIDGETS_VERSION@} $lines $iwidgetsVersion]
puts $fd2 $lines
close $fd2
# End Create iwidgets.tcl


# Create pkgIndex.tcl for iwidgets
set fd1 [open [file join $rootDir src other iwidgets pkgIndex.tcl.in] r]
set lines [read $fd1]
close $fd1
puts "Creating [file join $installDir lib iwidgets$iwidgetsVersion pkgIndex.tcl]"
set fd2 [open [file join $installDir lib iwidgets$iwidgetsVersion pkgIndex.tcl] w]
set lines [regsub -all {@IWIDGETS_VERSION@} $lines $iwidgetsVersion]
puts $fd2 $lines
close $fd2
# End Create pkgIndex.tcl for iwidgets


# Create wish.exe.manifest
set fd1 [open [file join $rootDir src other tk win wish.exe.manifest].in r]
set lines [read $fd1]
close $fd1
puts "Creating [file join $rootDir src other tk win wish.exe.manifest]"
set fd2 [open [file join $rootDir src other tk win wish.exe.manifest] w]
set lines [regsub -all {@TK_WIN_VERSION@} $lines $tclVersion]
set lines [regsub -all {@MACHINE@} $lines "x86"]
puts $fd2 $lines
close $fd2
# End Create wish.exe.manifest


# Create the tclIndex files
#lappend auto_path 
make_tclIndex [list [file join $shareDir tclscripts]]
make_tclIndex [list [file join $shareDir tclscripts lib]]
make_tclIndex [list [file join $shareDir tclscripts archer]]
make_tclIndex [list [file join $shareDir tclscripts mged]]
make_tclIndex [list [file join $shareDir tclscripts rtwizard]]
make_tclIndex [list [file join $shareDir tclscripts util]]

#XXX More needed

# End Create the tclIndex files


# Copy redist files
catch {
    file copy "C:/Program Files/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.CRT" [file join $installDir bin]
    file copy "C:/Program Files/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.MFC" [file join $installDir bin]
}

# Create source files for tkhtml
puts "Create source files in tkhtml3 ..."
set savepwd [pwd]
cd [file join $rootDir src other tkhtml3 src]
puts "... creating htmltokens files"
source tokenlist.txt
puts "... creating cssprop files"
source cssprop.tcl
puts "... creating htmldefaultstyle.c"
exec tclsh mkdefaultstyle.tcl > htmldefaultstyle.c
puts "... creating pkgIndex.tcl"
set fd [open pkgIndex.tcl "w"]
puts $fd {package ifneeded Tkhtml 3.0 [list load [file join $dir tkhtml.dll]]}
close $fd
cd $savepwd
puts "copy [file join $rootDir src other tkhtml3 src pkgIndex.tcl] [file join $installDir bin Tkhtml3.0]"
file copy [file join $rootDir src other tkhtml3 src pkgIndex.tcl] [file join $installDir bin Tkhtml3.0]
# End Create source files for tkhtml

proc removeUnwanted {_startDir} {
    set savepwd [pwd]
    cd $_startDir

    puts "removing unwanted files in $_startDir ..."

    set files [glob -nocomplain *]
    set hfiles [glob -nocomplain -type hidden .svn]
    eval lappend files $hfiles
    set files [lsort -unique $files]

    foreach file $files {
	if {$file == ".svn" || $file == "Makefile.am" || [regexp {~$} $file]} {
	    puts "... deleting $file"
	    file delete -force $file
	} elseif {[file isdirectory $file]} {
	    removeUnwanted $file
	}
    }

    cd $savepwd
}

# Remove unwanted directories/files as a result of wholesale copies
removeUnwanted $installDir
