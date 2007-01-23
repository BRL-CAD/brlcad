#                        H E L P . T C L
# BRL-CAD
#
# Copyright (c) 1998-2007 United States Government as represented by
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
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#
::itcl::class cadwidgets::Help {
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

::itcl::body cadwidgets::Help::constructor {args} {
    # process options
    eval configure $args
}

::itcl::body cadwidgets::Help::? {cwidth ncol} {
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

::itcl::body cadwidgets::Help::add {name desc} {
    set data($name) $desc
}

::itcl::body cadwidgets::Help::apropos {key} {
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

::itcl::body cadwidgets::Help::delete {name} {
    unset data($name)
}

::itcl::body cadwidgets::Help::get {args} {
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

::itcl::body cadwidgets::Help::getCmds {} {
    return [lsort [array names data]]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
