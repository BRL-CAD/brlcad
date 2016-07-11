#
#                    B R L M A N . T C L
# BRL-CAD
#
# Copyright (c) 2006-2016 United States Government as represented by
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
    set w [ ManBrowser .browser -useToC 1 -listDir $::section_number ]
} else {
    set w [ ManBrowser .browser -useToC 1 -listDir 1 ]
}
$w buttonconfigure 0 -text Close -command ::exit

if {[info exists ::man_file]} {
    wm title .browser "BRL-CAD Man Page ($::section_number) - $::man_name"
} else {
    wm title .browser "BRL-CAD Man Pages"
}
wm protocol .browser WM_DELETE_WINDOW ::exit
bind $w <Escape> ::exit
bind $w <q> ::exit

if {[info exists ::man_file]} {
    $w loadPage $man_file
} else {
    set intro_file [file join [bu_brlcad_data $::data_dir] mann/Introduction.html]
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
