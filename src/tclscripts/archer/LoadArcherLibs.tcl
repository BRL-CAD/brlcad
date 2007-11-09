#              L O A D A R C H E R L I B S . T C L
# BRL-CAD
#
# Copyright (c) 2006-2007 United States Government as represented by
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
#    Bob Parker (SURVICE Engineering Company)
#
# Description:
#    Code to load Archer's shared libraries.
###

proc LoadArcherLibs {dir} {
    global tcl_platform

    # load tkimg
    if {$tcl_platform(platform) == "windows"} {
	set ext "dll"
	set tkimgdir [bu_brlcad_root "bin"]
    } else {
	set ext "so"
	set tkimgdir [bu_brlcad_root "lib"]
	if {![file exists $tkimgdir]} {
	    set tkimgdir [file join [bu_brlcad_data "src"] other tkimg .libs]
	}
    }

    # can't use sharedlibextension without changing tkimg build
    if {![file exists [file join $tkimgdir tkimg.$ext]]} {
	puts "ERROR: Unable to initialize Archer imagery"
	exit 1
    }

    load [file join $tkimgdir tkimg.$ext]

    # Try to load Sdb
    if {[catch {package require Sdb 1.1}]} {
	set Archer::haveSdb 0
    } else {
	set Archer::haveSdb 1
    }

    if { [catch {package require Swidgets} _initialized] } {
	puts "$_initialized"
	puts ""
	puts "ERROR: Unable to load Archer Scripting"
	exit 1
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
