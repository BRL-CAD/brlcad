##                 M G E D . T C L
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
#	The Mged class inherits from Database and MultiDisplay.
#
class Mged {
    inherit MultiDisplay Database

    constructor {args} {
	Database::constructor
	eval MultiDisplay::constructor $args
    } {
    }

    destructor {
    }

    private method add {geo}
    private method remove {geo}
    private method contents {}
    private method open {}
    public method opendb {args}
    public method draw {args}
}

body Mged::add {geo} {
    MultiDisplay::$ul configure -geolist $dg
    MultiDisplay::$ur configure -geolist $dg
    MultiDisplay::$ll configure -geolist $dg
    MultiDisplay::$lr configure -geolist $dg
}

body Mged::opendb {args} {
    set len [llength $args]
    set result [catch {eval Database::open $args} msg]

    if {$len} {
	add $dg
    }

    return -code $result $msg
}

body Mged::draw {args} {
    set geo [$dg who]
    eval $dg draw $args

    if {$geo == ""} {
	MultiDisplay::$ul autoview
	MultiDisplay::$ur autoview
	MultiDisplay::$ll autoview
	MultiDisplay::$lr autoview
    }
}
