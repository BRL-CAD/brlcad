##                 H E L P . T C L
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
#
class cadwidgets::Help {
    constructor {args} {}
    destructor {}

    public method ? {cwidth ncol}
    public method apropos {key}
    public method add {name desc}
    public method delete {name}
    public method get {args}
    public method getCmds {}

    private variable data
}

body cadwidgets::Help::constructor {args} {
    # process options
    eval configure $args
}

body cadwidgets::Help::? {cwidth ncol} {
    set i 1
    foreach cmd [lsort [array names data]] {
	append info [format "%-[subst $cwidth]s" $cmd]
	if {![expr $i % [subst $ncol]]} {
	    append info "\n"
	}
	incr i
    }

    return $info
}

body cadwidgets::Help::add {name desc} {
    set data($name) $desc
}

body cadwidgets::Help::apropos {key} {
    set info ""
    foreach cmd [lsort [array names data]] {
	if {[string first $key $cmd] != -1} {
	    append info "$cmd "
	} elseif {[string first $key $data($cmd)] != -1} {
	    append info "$cmd "
	}
    }

    return $info
}

body cadwidgets::Help::delete {name} {
    unset data($name)
}

body cadwidgets::Help::get {args} {
    if {[llength $args] && [lindex $args 0] != {}} {
	set cmd [lindex $args 0]
	if [info exists data($cmd)] {
	    return "Usage: $cmd [lindex $data($cmd) 0]\n\t([lindex $data($cmd) 1])"
	} else {
	    error "No help found for $cmd"
	}
    } else {
	foreach cmd [lsort [array names data]] {
	    append info "$cmd [lindex $data($cmd) 0]\n\t[lindex $data($cmd) 1]\n"
	}

	return $info
    }
}

body cadwidgets::Help::getCmds {} {
    return [lsort [array names data]]
}
