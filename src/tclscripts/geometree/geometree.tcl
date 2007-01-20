#!/bin/tclsh
#                   G E O M E T R E E . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# This is the GeometryBrowser main script.  It manages the loading and
# unloading of a GeometryBrowser object into mged automatically.
#
# The development version codename is "geometree"
# The final version will be "geometree" :-)
#
# Author -
# 	Christopher Sean Morrison
#
# Source -
#		The U.S. Army Research Laboratory
#		Aberdeen Proving Ground, Maryland  21005
#

#
# Extend Autopath
#
foreach i $auto_path {
    set geometreeDir [file join $i {..} tclscripts geometree ]
    if {  [file exists $geometreeDir] } {
	lappend auto_path $geometreeDir
    }
}


package require GeometryBrowser

# All GeometryBrowser stuff is in the GeometryBrowser namespace
proc geometree { } {
	global mged_gui
	global ::tk::Priv
	global mged_players

	# determine the framebuffer window id
	if { [ catch { set mged_players } _mgedFramebufferId ] } {
		puts $_mgedFramebufferId
		puts "assuming default mged framebuffer id: id_0"
		set _mgedFramebufferId "id_0"
	}
	# just in case there are more than one returned
	set _mgedFramebufferId [ lindex $_mgedFramebufferId 0 ]

	set gt .$_mgedFramebufferId.geometree

	# see if the window is already open.  If so, just raise it up.
	if [ winfo exists $gt ] {
		raise $gt
		return
	}

	# just to quell the tk name returned and report fatal errors
	if [ catch { GeometryBrowser $gt } gbName ] {
		puts $gbName
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
