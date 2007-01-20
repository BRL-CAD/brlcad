#                      S P L A S H . T C L
# BRL-CAD
#
# Copyright (c) 1998-2007 United States Government as represented by
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
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#       The Splash class implements a splash screen.
#

option add *Splash.background blue widgetDefault
option add *Splash.foreground yellow widgetDefault
option add *Splash.borderWidth 2 widgetDefault
option add *Splash.relief raised widgetDefault
option add *Splash.cursor watch widgetDefault

::itcl::class Splash {
    inherit iwidgets::Shell

    constructor {args} {}
}

::itcl::body Splash::constructor {args} {
    wm overrideredirect $itk_component(hull) 1

    # revive a few ignored options
    itk_option add hull.borderwidth hull.relief

    itk_component add message {
	label $itk_interior.message
    } {
	usual
	keep -image -bitmap
	rename -width -labelwidth labelwidth Labelwidth
	rename -height -labelheight labelheight Labelheight
	rename -text -message message Text
    }

    # process options
    eval itk_initialize $args

    pack $itk_component(message) -expand yes -fill both
    center
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
