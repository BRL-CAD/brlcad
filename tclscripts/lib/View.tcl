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
    private variable view ""

    public variable size ""
    public variable scale ""
    public variable center ""
    public variable aet ""

    constructor {args} {
	# first create view object
	set view [v_open $this\_view]

	# now safe to process options
	eval configure $args

	# initialize public variables
	set size [$view size]
	set scale [$view scale]
	set center [$view center]
	set aet [$view aet]
    }

    destructor {
	$view close
    }

    public method aet {args}
    public method center {args}
    public method rot {args}
    public method slew {args}
    public method tra {args}
    public method size {args}
    public method scale {args}
    public method zoom {sf}
}

configbody View::size {
    size $size
}

configbody View::scale {
    scale $scale
}

configbody View::center {
    center $center
}

configbody View::aet {
    aet $aet
}

body View::aet {args} {
    set len [llength $args]

    # get aet
    if {$len == 0} {
	return $aet
    }

    # set aet
    $view aet $args
    set aet [$view aet]
}

body View::center {args} {
    set len [llength $args]

    # get center
    if {$len == 0} {
	return $center
    }

    # set center
    $view center $args
    set center [$view center]
}

body View::rot {args} {
    # rotate view
    $view rot $args
    set aet [$view aet]
}

body View::slew {args} {
    # slew the view
    $view slew $args

    set center [$view center]
}

body View::tra {args} {
    # translate view
    $view tra $args
    set center [$view center]
}

body View::size {args} {
    set len [llength $args]

    # get size
    if {$len == 0} {
	return $size
    }

    # set size
    $view size $args

    set size [$view size]
    set scale [$view scale]
}

body View::scale {args} {
    set len [llength $args]

    # get scale
    if {$len == 0} {
	return $scale
    }

    # set scale
    $view scale $args

    set size [$view size]
    set scale [$view scale]
}

body View::zoom {sf} {
    $view zoom $sf

    set size [$view size]
    set scale [$view scale]
}
