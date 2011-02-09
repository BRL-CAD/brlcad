#              L O A D A R C H E R L I B S . T C L
# BRL-CAD
#
# Copyright (c) 2006-2011 United States Government as represented by
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

proc LoadArcherCoreLibs {} {
    global tcl_platform

    # load tkpng
    if { [catch {package require tkpng} _initialized] } {
	puts "$_initialized"
	puts ""
	puts "ERROR: Unable to load tkpng"
	exit 1
    }


    if { [catch {package require Swidgets} _initialized] } {
	puts "$_initialized"
	puts ""
	puts "ERROR: Unable to load ArcherCore Scripting"
	exit 1
    }

    # load Tkhtml
    catch {package require hv3 0.1} hv3
}

proc LoadArcherLibs {} {
    # Nothing for now
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
