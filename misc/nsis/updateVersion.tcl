#                      U P D A T E V E R S I O N . T C L
# BRL-CAD
#
# Copyright (c) 2002-2011 United States Government as represented by
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
# Description:
#    Script for creating VERSION.txt (used by brlcad.nsi) and
#    VERSION.bat (used by asc2g.vcproj).
#

set rootDir [file normalize ../../../]
if {![file exists [file join $rootDir include]]} {
    puts "$rootDir must exist and must be the root of the BRL-CAD source tree. "
    return
}

set missingFile 0

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

set platform [lindex $argv 0]
if {$platform == "x64"} {
    set brlcadInstall "brlcadInstallx64"
    set installerSuffix " (64-bit)"
} else {
    set platform ""
    set brlcadInstall brlcadInstall
    set installerSuffix ""
}

set version "$major.$minor.$patch"

# Create VERSION.txt
set fd [open [file join $rootDir misc nsis VERSION.txt] "w"]
puts $fd "!define MAJOR '$major'"
puts $fd "!define MINOR '$minor'"
puts $fd "!define PATCH '$patch'"
puts $fd "!define VERSION '$version'"
puts $fd "!define PLATFORM '$platform'"
puts $fd "!define INSTALLERSUFFIX '$installerSuffix'"
close $fd

# Create VERSION.bat
set fd [open [file join $rootDir misc nsis VERSION.bat] "w"]
puts $fd "set BrlcadVersion=$version"
close $fd
