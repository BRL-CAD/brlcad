#             G E O M E T R Y C H E C K E R . T C L
# BRL-CAD
#
# Copyright (c) 2016 United States Government as represented by
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
# Description -
#
# This is the GeometryChecker object script.  It's the actual guts and
# magic that "is" the overlap resolution GUI.  It generates a list of
# geometry objects that overlap or have other problems.
#

package require Itcl
package require Itk
package require Iwidgets

# go ahead and blow away the class if we are reloading
catch {delete class GeometryChecker} error

package provide GeometryChecker 1.0

class GeometryChecker {
    inherit itk::Toplevel

    constructor {} {}
    destructor {}

    public {
    }

    protected {
    }

    private {
	variable _debug
    }
}


###########
# begin constructor/destructor
###########

body GeometryChecker::constructor {} {
    # set to 1/0 to turn call path debug messages on/off
    set _debug 0
}


body GeometryChecker::destructor {} {
    if { $_debug } {
	puts "destructor"
    }
}

###########
# end constructor/destructor
###########


###########
# begin public methods
###########


##########
# end public methods
##########


##########
# begin protected methods
##########


##########
# end protected methods
##########

##########
# begin private methods
##########


##########
# end private methods
##########


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
