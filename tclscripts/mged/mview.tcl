# Author - Bob Parker

check_externs "_mged_attach"

proc openmv { id w wc dpy dtype S } {
    global win_to_id

    frame $wc.u
    frame $wc.l
    frame $wc.u.l -relief sunken -borderwidth 2
    frame $wc.u.r -relief sunken -borderwidth 2
    frame $wc.l.l -relief sunken -borderwidth 2
    frame $wc.l.r -relief sunken -borderwidth 2

    attach -d $dpy -t 0 -S $S -n $w.ul $dtype
    attach -d $dpy -t 0 -S $S -n $w.ur $dtype
    attach -d $dpy -t 0 -S $S -n $w.ll $dtype
    attach -d $dpy -t 0 -S $S -n $w.lr $dtype

    set win_to_id($w.ul) $id
    set win_to_id($w.ur) $id
    set win_to_id($w.ll) $id
    set win_to_id($w.lr) $id

    pack $w.ul -in $wc.u.l -expand 1 -fill both
    pack $w.ur -in $wc.u.r -expand 1 -fill both
    pack $w.ll -in $wc.l.l -expand 1 -fill both
    pack $w.lr -in $wc.l.r -expand 1 -fill both
    pack $wc.u.l $wc.u.r -side left -anchor w -expand 1 -fill both
    pack $wc.l.l $wc.l.r -side left -anchor w -expand 1 -fill both
}


proc packmv { id } {
    global mged_dmc

    pack $mged_dmc($id).u $mged_dmc($id).l -expand 1 -fill both
}


proc unpackmv { id } {
    global mged_dmc

    pack forget $mged_dmc($id).u $mged_dmc($id).l
}


proc releasemv { id } {
    global mged_top

    catch  { release $mged_top($id).ul }
    catch  { release $mged_top($id).ur }
    catch  { release $mged_top($id).ll }
    catch  { release $mged_top($id).lr }
}


proc closemv { id } {
    global mged_dmc

    releasemv $id
    catch { destroy $mged_dmc($id) }
}


proc setupmv { id } {
    global mged_top
    global faceplate
    global mged_faceplate

    winset $mged_top($id).ul
    press top
    set faceplate 0

    winset $mged_top($id).ur
    press 35,25
    set faceplate 0

    winset $mged_top($id).ll
    press front
    set faceplate 0

    winset $mged_top($id).lr
    press right
    set faceplate 0

    bind $mged_top($id).ul m "togglemv $id"
    bind $mged_top($id).ur m "togglemv $id"
    bind $mged_top($id).ll m "togglemv $id"
    bind $mged_top($id).lr m "togglemv $id"
}


proc setmv { id } {
    global mged_top
    global mged_dmc
    global win_size
    global multi_view
    global mged_active_dm
    global mged_small_dmc

    if $multi_view($id) {
	unpackmv $id

	set mv_size [expr $win_size($id) / 2 - 4]

# In case of resize/reconfiguration --- resize everybody
	winset $mged_top($id).ul
	dm size $mv_size $mv_size
	winset $mged_top($id).ur
	dm size $mv_size $mv_size
	winset $mged_top($id).ll
	dm size $mv_size $mv_size
	winset $mged_top($id).lr
	dm size $mv_size $mv_size

	pack $mged_active_dm($id) -in $mged_small_dmc($id) -expand 1 -fill both

	packmv $id
    } else {
	winset $mged_active_dm($id)
	unpackmv $id
	dm size $win_size($id) $win_size($id)
	pack $mged_active_dm($id) -in $mged_dmc($id)
    }
}


proc togglemv { id } {
    global multi_view

    if $multi_view($id) {
	set multi_view($id) 0
    } else {
	set multi_view($id) 1
    }

    setmv $id
}
