##                 S P L A S H . T C L
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
#       The Splash class implements a splash screen as described
#       in the "Effective Tcl/Tk Programming" book.
#

option add *Splash.background blue widgetDefault
option add *Splash.foreground yellow widgetDefault
option add *Splash.borderWidth 2 widgetDefault
option add *Splash.relief raised widgetDefault
option add *Splash.cursor watch widgetDefault

class Splash {
    inherit itk::Toplevel

    constructor {args} {}
    destructor {}

    public method center {} {}

    private variable xmax 0
    private variable ymax 0
}

body Splash::constructor {args} {
    wm withdraw $itk_component(hull)
    set xmax [winfo screenwidth $itk_component(hull)]
    set ymax [winfo screenheight $itk_component(hull)]

    # revive ignored options from Toplevel
    itk_option add hull.borderwidth hull.relief

    itk_component add message {
	label $itk_interior.message
    } {
	usual
	keep -height -width
	rename -text -message message Text
    }

    # process options
    eval itk_initialize $args

    pack $itk_component(message) -expand yes -fill both

    wm overrideredirect $itk_component(hull) 1
    update idletasks
    center
    wm deiconify $itk_component(hull)
}

body Splash::center {} {
    set x [expr ($xmax - [winfo reqwidth $itk_component(hull)]) / 2]
    set y [expr ($ymax - [winfo reqheight $itk_component(hull)]) / 2]
    wm geometry $itk_component(hull) "+$x+$y"
}
