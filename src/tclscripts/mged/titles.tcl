#                      T I T L E S . T C L
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
catch { destroy .titles }

toplevel .titles
wm title .titles "MGED Display Variables"

frame .titles.l
frame .titles.r

pack .titles.l .titles.r -side left -fill both -expand yes

foreach name [array names mged_display] {
    label .titles.l.$name -text mged_display($name) -anchor w
    label .titles.r.$name -textvar mged_display($name) -anchor w

    pack .titles.l.$name -side top -fill y -expand yes -anchor w
    pack .titles.r.$name -side top -fill x -expand yes -anchor w
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
