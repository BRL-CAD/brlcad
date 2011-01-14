#                    C O M B O B O X . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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
#	The ComboBox combines an entry widget with a menubutton and menu.
#


::itk::usual ComboBox {
    keep -tearoff
}

::itcl::class cadwidgets::ComboBox {
    inherit ::itk::Widget

    itk_option define -text text Text ""

    constructor {args} {}
    destructor {}

    public {
	# menu stuff
	method add {args}
	method delete {args}
	method menuConfigure {args}
	method menuEntryConfigure {args}
	method index {args}
	method insert {args}
	method type {index}

	# entry stuff
	method entryConfigure {args}
	method getText {}
	method setText {val}
    }

    private {
	variable entrytext ""
    }
}

::itcl::configbody cadwidgets::ComboBox::text {
    $itk_component(label) configure -text $itk_option(-text)
}

::itcl::body cadwidgets::ComboBox::constructor {args} {
    itk_component add frame {
	::frame $itk_interior.frame -relief sunken -bd 1
    } {
	usual
    }

    itk_component add label {
	::label $itk_interior.label
    } {
	usual
    }

    itk_component add entry {
	::entry $itk_interior.entry -relief flat -width 12
    } {
	usual
	keep -state
	rename -textvariable -entryvariable entryvariable Text
    }

    itk_component add menubutton {
	::menubutton $itk_interior.menubutton -relief raised -bd 1 \
	    -menu $itk_interior.menubutton.m -indicatoron 1
    } {
	usual
    }

    itk_component add menu {
	::menu $itk_interior.menubutton.m
    } {
	usual
	keep -tearoff
    }

    grid $itk_component(entry) $itk_component(menubutton) \
	-sticky "nsew" -in $itk_component(frame)
    grid rowconfigure $itk_component(frame) 0 -weight 1
    grid columnconfigure $itk_component(frame) 0 -weight 1
    grid $itk_component(label) $itk_component(frame) -sticky "nsew"
    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 1 -weight 1

    # Associate the entrytext variable with the entry component
    configure -entryvariable [::itcl::scope entrytext]
    eval itk_initialize $args
}

::itcl::body cadwidgets::ComboBox::add {args} {
    eval $itk_component(menu) add $args
}

::itcl::body cadwidgets::ComboBox::delete {args} {
    eval $itk_component(menu) delete $args
}

::itcl::body cadwidgets::ComboBox::menuConfigure {args} {
    eval $itk_component(menu) configure $args
}

::itcl::body cadwidgets::ComboBox::menuEntryConfigure {args} {
    eval $itk_component(menu) entryconfigure $args
}

::itcl::body cadwidgets::ComboBox::index {args} {
    eval $itk_component(menu) index $args
}

::itcl::body cadwidgets::ComboBox::insert {args} {
    eval $itk_component(menu) insert $args
}

::itcl::body cadwidgets::ComboBox::entryConfigure {args} {
    eval $itk_component(entry) configure $args
}

::itcl::body cadwidgets::ComboBox::getText {} {
    set ev [cget -entryvariable]

    # $-substitution should work, but doesn't
    return [set $ev]
}

::itcl::body cadwidgets::ComboBox::setText {val} {
    set ev [cget -entryvariable]
    set $ev $val
}

::itcl::body cadwidgets::ComboBox::type {index} {
    $itk_component(menu) type $index
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
