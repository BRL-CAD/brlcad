# Author - Bob Parker

check_externs "_mged_attach"

if ![info exists bob_testing] {
    set bob_testing 0
}

proc openmv { id w wc dpy dtype S } {
    global win_to_id
    global bob_testing

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

    if { $bob_testing } {
	grid $w.ul -in $wc.u.l -sticky "nsew"
	grid $w.ur -in $wc.u.r -sticky "nsew"
	grid $w.ll -in $wc.l.l -sticky "nsew"
	grid $w.lr -in $wc.l.r -sticky "nsew"
	grid $w.u.l $wc.u.r -sticky "nsew"
	grid $w.l.l $wc.l.r -sticky "nsew"

	grid columnconfigure $w.ul 0 -weight 1
	grid rowconfigure $w.ul 0 -weight 1
	grid columnconfigure $w.ur 0 -weight 1
	grid rowconfigure $w.ur 0 -weight 1
	grid columnconfigure $w.ll 0 -weight 1
	grid rowconfigure $w.ll 0 -weight 1
	grid columnconfigure $w.lr 0 -weight 1
	grid rowconfigure $w.lr 0 -weight 1

	grid columnconfigure $wc.u.l 0 -weight 1
	grid rowconfigure $wc.u.l 0 -weight 1
	grid columnconfigure $wc.u.r 0 -weight 1
	grid rowconfigure $wc.u.r 0 -weight 1
	grid columnconfigure $wc.l.l 0 -weight 1
	grid rowconfigure $wc.l.l 0 -weight 1
	grid columnconfigure $wc.l.r 0 -weight 1
	grid rowconfigure $wc.l.r 0 -weight 1
	grid columnconfigure $wc.u 0 -weight 1
	grid columnconfigure $wc.u 1 -weight 1
	grid rowconfigure $wc.u 0 -weight 1
	grid columnconfigure $wc.l 0 -weight 1
	grid columnconfigure $wc.l 1 -weight 1
	grid rowconfigure $wc.l 0 -weight 1
    } else {
	pack $w.ul -in $wc.u.l -expand 1 -fill both
	pack $w.ur -in $wc.u.r -expand 1 -fill both
	pack $w.ll -in $wc.l.l -expand 1 -fill both
	pack $w.lr -in $wc.l.r -expand 1 -fill both
	pack $wc.u.l $wc.u.r -side left -anchor w -expand 1 -fill both
	pack $wc.l.l $wc.l.r -side left -anchor w -expand 1 -fill both
    }

    menu_accelerator_bindings $id $w.ul ul
    menu_accelerator_bindings $id $w.ur ur
    menu_accelerator_bindings $id $w.ll ll
    menu_accelerator_bindings $id $w.lr lr
}

proc menu_accelerator_bindings { id w pos } {
    bind $w <Alt-ButtonPress-1> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-ButtonPress-2> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings.mouse_behavior %X %Y; break"
    bind $w <Alt-F> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.file %X %Y; break"
    bind $w <Alt-f> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.file %X %Y; break"
    bind $w <Alt-E> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.edit %X %Y; break"
    bind $w <Alt-e> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.edit %X %Y; break"
    bind $w <Alt-C> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.create %X %Y; break"
    bind $w <Alt-c> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.create %X %Y; break"
    bind $w <Alt-V> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.view %X %Y; break"
    bind $w <Alt-v> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.view %X %Y; break"
    bind $w <Alt-R> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.viewring %X %Y; break"
    bind $w <Alt-r> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.viewring %X %Y; break"
    bind $w <Alt-M> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-m> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-S> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-s> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-T> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.tools %X %Y; break"
    bind $w <Alt-t> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.tools %X %Y; break"
    bind $w <Alt-O> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.other %X %Y; break"
    bind $w <Alt-o> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.other %X %Y; break"
    bind $w <Alt-H> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.help %X %Y; break"
    bind $w <Alt-h> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.help %X %Y; break"
}

proc packmv { id } {
    global mged_dmc
    global bob_testing

    if { $bob_testing } {
	grid $mged_dmc($id).u -sticky "nsew"
	grid $mged_dmc($id).l -sticky "nsew"
    } else {
	pack $mged_dmc($id).u $mged_dmc($id).l -expand 1 -fill both
    }
}


proc unpackmv { id } {
    global mged_dmc
    global bob_testing

    if { $bob_testing } {
	grid forget $mged_dmc($id).u $mged_dmc($id).l
    } else {
	pack forget $mged_dmc($id).u $mged_dmc($id).l
    }
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

    set_default_views $id
    mged_apply_local $id "set faceplate 0"

    bind $mged_top($id).ul m "togglemv $id"
    bind $mged_top($id).ur m "togglemv $id"
    bind $mged_top($id).ll m "togglemv $id"
    bind $mged_top($id).lr m "togglemv $id"
}

proc set_default_views { id } {
    global mged_top

    winset $mged_top($id).ul
    ae 0 90

    winset $mged_top($id).ur
    press 35,25

    winset $mged_top($id).ll
    press front

    winset $mged_top($id).lr
    press left
}

proc setmv { id } {
    global mged_top
    global mged_dmc
    global win_size
    global multi_view
    global mged_active_dm
    global mged_small_dmc
    global bob_testing

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

	if { $bob_testing } {
	    grid $mged_active_dm($id) -in $mged_small_dmc($id) -sticky "nsew"
	} else {
	    pack $mged_active_dm($id) -in $mged_small_dmc($id) -expand 1 -fill both
	}

	packmv $id
    } else {
	winset $mged_active_dm($id)
	unpackmv $id
	dm size $win_size($id) $win_size($id)

	if { $bob_testing } {
	    grid $mged_active_dm($id) -in $mged_dmc($id) -sticky "nsew"
	} else {
	    pack $mged_active_dm($id) -in $mged_dmc($id)
	}
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
