##                 C A D C O M B O B O X . T C L
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
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	The CadComboBox combines an entry widget with a menubutton and menu.
#

class CadComboBox {
    inherit itk::Widget

    itk_option define -text text Text ""

    constructor {args} {}
    destructor {}

    # menu stuff
    public method add {args}
    public method delete {args}
    public method insert {args}

    # entry stuff
    public method entryget {}
    public method entryset {val}

    private variable entrytext ""
}

configbody CadComboBox::text {
    $itk_component(label) configure -text $itk_option(-text)
}

body CadComboBox::constructor {args} {
    itk_component add frame {
	frame $itk_interior.frame -relief sunken -bd 2
    } {}

    itk_component add label {
	label $itk_interior.label
    } {
	usual
    }

    itk_component add entry {
	entry $itk_interior.entry -relief flat -width 12 \
		-textvariable [scope entrytext]
    } {
	usual
    }

    itk_component add menubutton {
	menubutton $itk_interior.menubutton -relief raised -bd 2 \
		-menu $itk_interior.menubutton.m -indicatoron 1
    } {}

    itk_component add menu {
	menu $itk_interior.menubutton.m -tearoff 0
    } {}

    grid $itk_component(entry) $itk_component(menubutton) \
	    -sticky "nsew" -in $itk_component(frame)
    grid rowconfigure $itk_component(frame) 0 -weight 1
    grid columnconfigure $itk_component(frame) 0 -weight 1
    grid $itk_component(label) $itk_component(frame) -sticky "nsew"
    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 1 -weight 1

    eval itk_initialize $args
}

body CadComboBox::add {args} {
    eval $itk_component(menu) add $args
}

body CadComboBox::delete {args} {
    eval $itk_component(menu) delete $args
}

body CadComboBox::insert {args} {
    eval $itk_component(menu) insert $args
}

body CadComboBox::entryget {} {
    return $entrytext
}

body CadComboBox::entryset {val} {
    set entrytext $val
}
