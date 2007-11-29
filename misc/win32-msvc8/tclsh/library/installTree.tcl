#                      I N S T A L L T R E E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2007 United States Government as represented by
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


set usage "Usage: init.sh root_src_dir"


if {$argc != 0} {
    puts $usage
    return
}

set rootDir [file normalize ../../..]
if {![file exists $rootDir]} {
    puts "$rootDir must exist and must be the root of the BRL-CAD source tree. "
    return
}

set installDir [file join $rootDir brlcadInstall]
if {[file exists $installDir]} {
    puts "$installDir already exists."
    return
}

# Look for required source files
set missingFile 0
if {![file exists [file join $rootDir src other iwidgets iwidgets.tcl.in]]} {
    puts "Missing [file join $rootDir src other iwidgets iwidgets.tcl.in]"
    set missingFile 1
}

if {![file exists [file join $rootDir src other iwidgets pkgIndex.tcl.in]]} {
    puts "Missing [file join $rootDir src other iwidgets pkgIndex.tcl.in]"
    set missingFile 1
}

if {![file exists [file join $rootDir src other tk win wish.exe.manifest].in]} {
    puts "Missing [file join $rootDir src other tk win wish.exe.manifest].in]"
    set missingFile 1
}

if {![file exists [file join $rootDir include conf MAJOR]]} {
    puts "Missing [file join $rootDir include conf MAJOR]]"
    set missingFile 1
}

if {![file exists [file join $rootDir include conf MINOR]]} {
    puts "Missing [file join $rootDir include conf MINOR]]"
    set missingFile 1
}

if {![file exists [file join $rootDir include conf PATCH]]} {
    puts "Missing [file join $rootDir include conf PATCH]]"
    set missingFile 1
}


if {$missingFile} {
    return
}


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


# Show tclsh where to find required tcl files
lappend auto_path [file join $rootDir src other tcl library]


# Create missing include/conf files
if {![catch {open [file join $rootDir include conf COUNT] w} fd]} {
    puts "Creating [file join $rootDir include conf COUNT]"
    puts $fd "0"
    close $fd
} else {
    puts "Unable to create [file join $rootDir include conf COUNT]\n$fd"
    return
}

if {![catch {open [file join $rootDir include conf HOST] w} fd]} {
    if {[info exists env(HOSTNAME)] && $env(HOSTNAME) != ""} {
	set host $env(HOSTNAME)
    } else {
	set host unknown
    }

    puts "Creating [file join $rootDir include conf HOST]"
    puts $fd \"$host\"
    close $fd
} else {
    puts "Unable to create [file join $rootDir include conf DATE]\n$fd"
    return
}

if {![catch {open [file join $rootDir include conf PATH] w} fd]} {
    puts "Creating [file join $rootDir include conf PATH]"
    puts $fd \"brlcad\"
    close $fd
} else {
    puts "Unable to create [file join $rootDir include conf PATH]\n$fd"
    return
}

if {![catch {open [file join $rootDir include conf USER] w} fd]} {
    if {[info exists env(USER)] && $env(USER) != ""} {
	set user $env(USER)
    } else {
	set user unknown
    }

    puts "Creating [file join $rootDir include conf USER]"
    puts $fd \"$user\"
    close $fd
} else {
    puts "Unable to create [file join $rootDir include conf USER]\n$fd"
    return
}

if {![catch {open [file join $rootDir include conf DATE] w} fd]} {
    puts "Creating [file join $rootDir include conf DATE]"
    puts $fd \"[clock format [clock seconds] -format "%B %d, %Y"]\"
    close $fd
} else {
    puts "Unable to create [file join $rootDir include conf DATE]\n$fd"
    return
}
# End Create missing include/conf files


# Create install directories
puts "Creating [file join $installDir]"
file mkdir [file join $installDir]
puts "Creating [file join $installDir bin]"
file mkdir [file join $installDir bin]
puts "Creating [file join $installDir lib]"
file mkdir [file join $installDir lib]
puts "Creating [file join $installDir lib iwidgets4.0.2]"
file mkdir [file join $installDir lib iwidgets4.0.2]
puts "Creating [file join $installDir share]"
file mkdir [file join $installDir share]
puts "Creating [file join $installDir share brlcad]"
file mkdir [file join $installDir share brlcad]
puts "Creating [file join $installDir share brlcad $cadVersion]"
file mkdir [file join $installDir share brlcad $cadVersion]
puts "Creating [file join $installDir share brlcad $cadVersion plugins]"
file mkdir [file join $installDir share brlcad $cadVersion plugins]
puts "Creating [file join $installDir share brlcad $cadVersion plugins archer]"
file mkdir [file join $installDir share brlcad $cadVersion plugins archer]
puts "Creating [file join $installDir share brlcad $cadVersion plugins archer Utilities]"
file mkdir [file join $installDir share brlcad $cadVersion plugins archer Utilities]
puts "Creating [file join $installDir share brlcad $cadVersion plugins archer Wizards]"
file mkdir [file join $installDir share brlcad $cadVersion plugins archer Wizards]
# End Create install directories


# Copy files to the bin directory
puts "copy [file join $rootDir src archer archer] [file join $installDir bin]"
file copy [file join $rootDir src archer archer] [file join $installDir bin]
puts "copy [file join $rootDir src archer archer.bat] [file join $installDir bin]"
file copy [file join $rootDir src archer archer.bat] [file join $installDir bin]
puts "copy [file join $rootDir src mged mged.bat] [file join $installDir bin]"
file copy [file join $rootDir src mged mged.bat] [file join $installDir bin]
# End Copy files to the bin directory


# Copy files to the lib directory
puts "copy [file join $rootDir src other blt library] [file join $installDir lib blt2.4]"
file copy [file join $rootDir src other blt library] [file join $installDir lib blt2.4]
puts "copy [file join $rootDir src other tcl library] [file join $installDir lib tcl8.5]"
file copy [file join $rootDir src other tcl library] [file join $installDir lib tcl8.5]
puts "copy [file join $rootDir src other tk library] [file join $installDir lib tk8.5]"
file copy [file join $rootDir src other tk library] [file join $installDir lib tk8.5]
puts "copy [file join $rootDir src other incrTcl itcl library] [file join $installDir lib itcl3.4]"
file copy [file join $rootDir src other incrTcl itcl library] [file join $installDir lib itcl3.4]
puts "copy [file join $rootDir src other incrTcl itk library] [file join $installDir lib itk3.4]"
file copy [file join $rootDir src other incrTcl itk library] [file join $installDir lib itk3.4]
puts "copy [file join $rootDir src other iwidgets generic] [file join $installDir lib iwidgets4.0.2]"
file copy [file join $rootDir src other iwidgets generic] [file join $installDir lib iwidgets4.0.2 scripts]
# End Copy files to the lib directory


# Copy files to the share directories
puts "copy [file join $rootDir doc html] [file join $installDir share brlcad $cadVersion]"
file copy [file join $rootDir doc html] [file join $installDir share brlcad $cadVersion]

puts "copy [file join $rootDir doc] [file join $installDir share brlcad $cadVersion]"
file copy [file join $rootDir doc] [file join $installDir share brlcad $cadVersion]

puts "copy [file join $rootDir COPYING] [file join $installDir share brlcad $cadVersion]"
file copy [file join $rootDir COPYING] [file join $installDir share brlcad $cadVersion]

puts "copy [file join $rootDir src tclscripts] [file join $installDir share brlcad $cadVersion]"
file copy [file join $rootDir src tclscripts] [file join $installDir share brlcad $cadVersion]

puts "copy [file join $rootDir src archer plugins utility.tcl] [file join $installDir share brlcad $cadVersion plugins archer]"
file copy [file join $rootDir src archer plugins utility.tcl] \
    [file join $installDir share brlcad $cadVersion plugins archer]
puts "copy [file join $rootDir src archer plugins wizards.tcl] [file join $installDir share brlcad $cadVersion plugins archer]"
file copy [file join $rootDir src archer plugins wizards.tcl] \
    [file join $installDir share brlcad $cadVersion plugins archer]
# End Copy files to the share directories


# Remove undesired directories/files as a result of wholesale copies
file delete -force [file join $installDir lib blt2.4 CVS]
file delete -force [file join $installDir lib blt2.4 Makefile.am]
file delete -force [file join $installDir lib blt2.4 dd_protocols CVS]
file delete -force [file join $installDir lib blt2.4 dd_protocols Makefile.am]
file delete -force [file join $installDir lib itcl3.4 CVS]
file delete -force [file join $installDir lib itcl3.4 Makefile.am]
file delete -force [file join $installDir lib itk3.4 CVS]
file delete -force [file join $installDir lib itk3.4 Makefile.am]
file delete -force [file join $installDir lib iwidgets4.0.2 CVS]
file delete -force [file join $installDir lib iwidgets4.0.2 Makefile.am]
file delete -force [file join $installDir lib iwidgets4.0.2 scripts CVS]
file delete -force [file join $installDir lib iwidgets4.0.2 scripts Makefile.am]
file delete -force [file join $installDir lib tcl8.5 CVS]
file delete -force [file join $installDir lib tcl8.5 dde CVS]
file delete -force [file join $installDir lib tcl8.5 encoding CVS]
file delete -force [file join $installDir lib tcl8.5 http CVS]
file delete -force [file join $installDir lib tcl8.5 http1.0 CVS]
file delete -force [file join $installDir lib tcl8.5 msgcat CVS]
file delete -force [file join $installDir lib tcl8.5 msgs CVS]
file delete -force [file join $installDir lib tcl8.5 opt CVS]
file delete -force [file join $installDir lib tcl8.5 platform CVS]
file delete -force [file join $installDir lib tcl8.5 reg CVS]
file delete -force [file join $installDir lib tcl8.5 tcltest CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Africa CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata America Argentina CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata America CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata America Indiana CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata America Kentucky CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata America North_Dakota CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Antarctica CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Arctic CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Asia CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Atlantic CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Australia CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Brazil CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Canada CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Chile CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Etc CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Europe CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Indian CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Mexico CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata Pacific CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata SystemV CVS]
file delete -force [file join $installDir lib tcl8.5 tzdata US CVS]
file delete -force [file join $installDir lib tk8.5 CVS]
#file delete -force [file join $installDir lib tk8.5 Makefile.am]
file delete -force [file join $installDir lib tk8.5 demos CVS]
#file delete -force [file join $installDir lib tk8.5 demos Makefile.am]
file delete -force [file join $installDir lib tk8.5 demos images CVS]
#file delete -force [file join $installDir lib tk8.5 demos images Makefile.am]
file delete -force [file join $installDir lib tk8.5 images CVS]
#file delete -force [file join $installDir lib tk8.5 images Makefile.am]
file delete -force [file join $installDir lib tk8.5 msgs CVS]
#file delete -force [file join $installDir lib tk8.5 msgs Makefile.am]
file delete -force [file join $installDir lib tk8.5 ttk CVS]
#file delete -force [file join $installDir lib tk8.5 ttk Makefile.am]

file delete -force [file join $installDir share brlcad $cadVersion doc .cvsignore]
file delete -force [file join $installDir share brlcad $cadVersion doc book Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion doc book CVS]
file delete -force [file join $installDir share brlcad $cadVersion doc CVS]
file delete -force [file join $installDir share brlcad $cadVersion doc html]
file delete -force [file join $installDir share brlcad $cadVersion doc legal Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion doc legal CVS]
file delete -force [file join $installDir share brlcad $cadVersion doc Makefile.am]

file delete -force [file join $installDir share brlcad $cadVersion html CVS]
file delete -force [file join $installDir share brlcad $cadVersion html Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals Anim_Tutorial CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals Anim_Tutorial Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals archer CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals archer Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals cadwidgets CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals cadwidgets Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals libbu CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals libbu Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals libdm CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals libdm Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals librt CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals librt Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals mged .cvsignore]
file delete -force [file join $installDir share brlcad $cadVersion html manuals mged CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals mged Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals mged animmate CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals mged animmate Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html manuals shaders CVS]
file delete -force [file join $installDir share brlcad $cadVersion html manuals shaders Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html ReleaseNotes CVS]
file delete -force [file join $installDir share brlcad $cadVersion html ReleaseNotes Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html ReleaseNotes Rel5.0 CVS]
file delete -force [file join $installDir share brlcad $cadVersion html ReleaseNotes Rel5.0 Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html ReleaseNotes Rel5.0 Summary CVS]
file delete -force [file join $installDir share brlcad $cadVersion html ReleaseNotes Rel5.0 Summary Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion html ReleaseNotes Rel6.0 CVS]
file delete -force [file join $installDir share brlcad $cadVersion html ReleaseNotes Rel6.0 Makefile.am]

file delete -force [file join $installDir share brlcad $cadVersion tclscripts CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Themes CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Themes Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Themes Crystal CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Themes Crystal Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Themes Crystal_Large CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Themes Crystal_Large Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Themes Windows CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer images Themes Windows Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts archer Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts geometree CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts geometree Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts lib CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts lib Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts mged CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts mged Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts nirt CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts nirt Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts pl-dm CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts pl-dm Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeA CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeA Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeB CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeB Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeC CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeC Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeD CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeD Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeE CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeE Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeF CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard examples PictureTypeF Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard lib CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts rtwizard lib Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts sdialogs CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts sdialogs Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts sdialogs scripts CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts sdialogs scripts Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts swidgets CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts swidgets Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts swidgets images CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts swidgets images Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts swidgets scripts CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts swidgets scripts Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts util CVS]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts util Makefile.am]
file delete -force [file join $installDir share brlcad $cadVersion tclscripts Makefile.am]
# End Remove undesired directories/files as a result of wholesale copies


# Create iwidgets.tcl
set fd1 [open [file join $rootDir src other iwidgets iwidgets.tcl.in] r]
set lines [read $fd1]
close $fd1
puts "Creating [file join $installDir lib iwidgets4.0.2 iwidgets.tcl]"
set fd2 [open [file join $installDir lib iwidgets4.0.2 iwidgets.tcl] w]
set lines [regsub -all {@ITCL_VERSION@} $lines "3.4"]
set lines [regsub -all {@IWIDGETS_VERSION@} $lines "4.0.2"]
puts $fd2 $lines
close $fd2
# End Create iwidgets.tcl


# Create pkgIndex.tcl for iwidgets
set fd1 [open [file join $rootDir src other iwidgets pkgIndex.tcl.in] r]
set lines [read $fd1]
close $fd1
puts "Creating [file join $installDir lib iwidgets4.0.2 pkgIndex.tcl]"
set fd2 [open [file join $installDir lib iwidgets4.0.2 pkgIndex.tcl] w]
set lines [regsub -all {@IWIDGETS_VERSION@} $lines "4.0.2"]
puts $fd2 $lines
close $fd2
# End Create pkgIndex.tcl for iwidgets


# Create wish.exe.manifest
set fd1 [open [file join $rootDir src other tk win wish.exe.manifest].in r]
set lines [read $fd1]
close $fd1
puts "Creating [file join $rootDir src other tk win wish.exe.manifest]"
set fd2 [open [file join $rootDir src other tk win wish.exe.manifest] w]
set lines [regsub -all {@TK_WIN_VERSION@} $lines "8.5"]
set lines [regsub -all {@MACHINE@} $lines "x86"]
puts $fd2 $lines
close $fd2
# End Create wish.exe.manifest
