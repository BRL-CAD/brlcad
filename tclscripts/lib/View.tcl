##                 V I E W . T C L
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
#	The View class wraps LIBRT's view object.
#
class View {
    public variable size 1000
    public variable center {0 0 0}
    public variable aet "0 90 0"
    public variable perspective_angle 90

    constructor {args} {}
    destructor {}

    public method aet {args}
    public method center {args}
    public method get_viewname {}
    public method model2view {}
    public method observer {args}
    public method perspective {args}
    public method pmodel2view {}
    public method rot {args}
    public method size {args}
    public method slew {x y}
    public method tra {args}
    public method zoom {sf}

    private variable view ""
}

configbody View::size {
    View::size $size
}

configbody View::center {
    eval View::center $center
}

configbody View::aet {
    eval View::aet $aet
}

configbody View::perspective_angle {
    View::perspective $perspective_angle
}

body View::constructor {args} {
    # first create view object
    set view [v_open $this\_view]

    # process options
    eval configure $args

    View::size $size
    eval View::center $center
    eval View::aet $aet
    View::perspective $perspective_angle
}

body View::destructor {} {
    $view close
}

body View::get_viewname {} {
    return $view
}

body View::observer {args} {
    eval $view observer $args
}

body View::aet {args} {
    # get aet
    if {$args == ""} {
	return $aet
    }

    # set aet
    $view aet $args
    set aet $args
}

body View::center {args} {
    # get center
    if {$args == ""} {
	return $center
    }

    # set center
    $view center $args
    set center $args
}

body View::rot {args} {
    # rotate view
    $view rot $args

    set aet [$view aet]
}

body View::slew {x y} {
    # slew the view
    $view slew [list $x $y]

    set center [$view center]
}

body View::tra {args} {
    # translate view
    $view tra $args

    set center [$view center]
}

body View::size {args} {
    # get size
    if {$args == ""} {
	return $size
    }

    # set size
    $view size $args

    set size $args
}

body View::zoom {sf} {
    $view zoom $sf

    set size [$view size]
}

body View::model2view {} {
    $view model2view
}

body View::pmodel2view {} {
    $view pmodel2view
}

body View::perspective {args} {
    # get perspective angle
    if {$args == ""} {
	return $perspective_angle
    }

    $view perspective $args
    set perspective_angle $args
}
