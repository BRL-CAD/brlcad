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
#       The Splash class implements a splash screen.
#

option add *Splash.background blue widgetDefault
option add *Splash.foreground yellow widgetDefault
option add *Splash.borderWidth 2 widgetDefault
option add *Splash.relief raised widgetDefault
option add *Splash.cursor watch widgetDefault

class Splash {
    inherit iwidgets::Shell

    constructor {args} {}
}

body Splash::constructor {args} {
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
