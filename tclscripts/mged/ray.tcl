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

    eval .inmem rt_gettrees ray -i -u [_mged_who]
    ray prep 1
    ray no_bool 1
    set xx [expr $x / 2048.0]
    set yy [expr $y / 2048.0]
    set target [view2model $xx $yy 0]
    if { $perspective_mode } {
	set start [view2model 0 0 1]
    } else {
	set start [view2model $xx $yy 1]
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
