#
#                    B R L M A N . T C L
# BRL-CAD
#
# Copyright (c) 2006-2022 United States Government as represented by
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


package require Iwidgets
package require ManBrowser 1.0
wm state . withdrawn
if {[info exists ::section_number]} {
    set w [ ManBrowser .browser -useToC 1 -defaultDir $::section_number ]
} else {
    set w [ ManBrowser .browser -useToC 1 -defaultDir 1 ]
}
$w buttonconfigure 0 -text Close -command ::exit

if {[info exists ::man_file]} {
    wm title .browser "BRL-CAD Manual Page Browser (man$::section_number)"
} else {
    wm title .browser "BRL-CAD Manual Page Browser"
}

wm protocol .browser WM_DELETE_WINDOW ::exit
bind $w <Escape> ::exit
bind $w <q> ::exit

if {[info exists ::man_file]} {
    $w select $man_file
} else {
    set intro_file [file join [bu_dir doc] html mann Introduction.html]
    $w loadPage $intro_file
}

$w center
$w activate
$w center
vwait forever
update


# Local Variables:
# mode: tcl
# tab-width: 8
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
