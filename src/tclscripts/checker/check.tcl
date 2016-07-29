#                     C H E C K E R . T C L
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
# This is the Geometry Checker GUI.  It presents a list of geometry
# objects that overlap or have other problems.
#

package require Tk
package require Itcl
package require Itk

# go ahead and blow away the class if we are reloading
catch {delete class GeometryChecker} error

::itcl::class GeometryChecker {
    inherit ::itk::Widget

    constructor {args} {}
    destructor {}

    public {
	method toggleDebug {} {}
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

::itcl::body GeometryChecker::constructor {args} {
    eval itk_initialize $args

    # set to 1/0 to turn call path debug messages on/off
    set _debug 0

    itk_component add checkFrame {
	ttk::frame $itk_interior.checkFrame -padding 5
    } {}

    itk_component add checkLabel {
    	ttk::label $itk_component(checkFrame).checkLabel \
    	    -text "This tool checks for geometry overlaps" 
    } {}

    itk_component add checkButton {
    	ttk::button $itk_component(checkFrame).checkButton \
    	    -text "Check!" \
    	    -command "rtcheck"
    } {}

    pack $itk_component(checkFrame) -expand true -fill both
    grid $itk_component(checkFrame).checkLabel - -sticky w -padx 5 -pady 5
    grid $itk_component(checkFrame).checkButton - -stick nesw -padx 5 -pady 5
}


::itcl::body GeometryChecker::destructor {} {
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

# toggleDebug
#
# turns debugging on/off
#
body GeometryChecker::toggleDebug { } {
    if { $_debug } {
	set _debug 0
    } else {
	set _debug 1
    }
}

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


# All GeometryChecker stuff is in the GeometryChecker namespace
proc check {{parent ""}} {
    if {[winfo exists $parent.checker]} {
	return
    }

    set checkerWindow [toplevel $parent.checker]
    set checkerContents [GeometryChecker $checkerWindow.contents]

    wm title $checkerWindow "Geometry Checker"
    pack $checkerContents -expand true -fill both
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
