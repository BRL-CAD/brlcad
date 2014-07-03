#               I T K _ R E D E F I N E S . T C L
# BRL-CAD
#
# Copyright (c) 2013-2014 United States Government as represented by
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
catch {itcl::delete class itk::Toplevel}

itcl::class itk::Toplevel {
    inherit itk::Archetype

    constructor {args} {
	#
	#  Create a toplevel window with the same name as this object
	#
	set itk_hull [namespace tail $this]
	set itk_interior $itk_hull

	itk_component add hull {
	    toplevel $itk_hull -class [namespace tail [info class]]
	} {
	    keep -menu -background -cursor -takefocus
	}
	bind itk-delete-$itk_hull <Destroy> [list itcl::delete object $this]

	set tags [bindtags $itk_hull]
	bindtags $itk_hull [linsert $tags 0 itk-delete-$itk_hull]

	eval itk_initialize $args
    }

    destructor {
	if {[winfo exists $itk_hull]} {
	    set tags [bindtags $itk_hull]
	    set i [lsearch $tags itk-delete-$itk_hull]
	    if {$i >= 0} {
		bindtags $itk_hull [lreplace $tags $i $i]
	    }
	    destroy $itk_hull
	}
	itk_component delete hull

	set components [component]
	foreach component $components {
	    set path($component) [component $component]
	}
	foreach component $components {
	    if {[winfo exists $path($component)]} {
		destroy $path($component)
	    }
	}
    }

    itk_option define -title title Title "" {
	wm title $itk_hull $itk_option(-title)
    }

    private variable itk_hull ""
}

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
