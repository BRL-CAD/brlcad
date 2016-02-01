#                         R A Y . T C L
# BRL-CAD
#
# Copyright (c) 2004-2016 United States Government as represented by
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
#			R A Y . T C L
#
#  Authors -
#	Mike J. Muuss
#	Robert G. Parker
#
#  Description -
#	Tcl routines that use LIBRT's Tcl interface.
#

proc mouse_shoot_ray { x y } {
    global perspective_mode

    # get zclip setting
    set zclip [dm set zclip]

    if {[catch {eval .inmem rt_gettrees ray -i -u [_mged_who]} msg]} {
	error $msg
    }

    ray prep 1
    ray no_bool 1
    set xx [expr $x / 2048.0]
    set yy [expr $y / 2048.0]
    set target [v2m_point $xx $yy 0]
    if {$perspective_mode} {
	set start [v2m_point 0 0 1]
    } elseif {!$zclip} {
	# calculate a view Z that's in front of all geometry

	# get center and size of displayed geometry
	set center_size [get_autoview]

	# extract view Z from center of geometry, then back the rest of the way out of the geometry
	set Z [expr [lindex [eval m2v_point [lindex $center_size 1]] 2] + [expr [lindex $center_size 3] / [size]]]
	if {$Z < 1} {
	    set Z 1
	}

	set start [v2m_point $xx $yy $Z]
    } else {
	set start [v2m_point $xx $yy 1]
    }

    return [ray shootray $start at $target]
}

proc ray_get_info { ray key1 key2 } {
    set items {}

    foreach partition $ray {
	set item [bu_get_value_by_keyword $key1 $partition]

	if {$key2 == ""} {
	    lappend items $item
	} else {
	    lappend items [bu_get_value_by_keyword $key2 $item]
	}
    }

    return $items
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
