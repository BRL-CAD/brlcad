##                 C O M B O B O X . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998-2004 by the United States Army
#       in all countries except the USA.  All rights reserved.
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

    # menu stuff
    public method add {args}
    public method delete {args}
    public method menuConfigure {args}
    public method menuEntryConfigure {args}
    public method index {args}
    public method insert {args}
    public method type {index}

    # entry stuff
    public method entryConfigure {args}
    public method getText {}
    public method setText {val}

    private variable entrytext ""
}

::itcl::configbody cadwidgets::ComboBox::text {
    $itk_component(label) configure -text $itk_option(-text)
}

::itcl::body cadwidgets::ComboBox::constructor {args} {
    itk_component add frame {
	::frame $itk_interior.frame -relief sunken -bd 2
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
	::menubutton $itk_interior.menubutton -relief raised -bd 2 \
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
