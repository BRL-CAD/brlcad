# Author - Bob Parker

check_externs "_mged_attach"

if ![info exists use_grid_gm] {
    set use_grid_gm 0
}

if ![info exists debug_setmv] {
    set debug_setmv 0
}

set mged_default_bd 2

proc openmv { id w wc dpy dtype S } {
    global win_to_id
    global use_grid_gm
    global mged_default_bd

    frame $wc.u
    frame $wc.l
    frame $wc.u.l -relief sunken -borderwidth $mged_default_bd
    frame $wc.u.r -relief sunken -borderwidth $mged_default_bd
    frame $wc.l.l -relief sunken -borderwidth $mged_default_bd
    frame $wc.l.r -relief sunken -borderwidth $mged_default_bd

    attach -d $dpy -t 0 -S $S -n $w.ul $dtype
    attach -d $dpy -t 0 -S $S -n $w.ur $dtype
    attach -d $dpy -t 0 -S $S -n $w.ll $dtype
    attach -d $dpy -t 0 -S $S -n $w.lr $dtype

    set win_to_id($w.ul) $id
    set win_to_id($w.ur) $id
    set win_to_id($w.ll) $id
    set win_to_id($w.lr) $id

    if { $use_grid_gm } {
	grid $w.ul -in $wc.u.l -sticky "nsew" -row 0 -column 0
	grid $w.ur -in $wc.u.r -sticky "nsew" -row 0 -column 0
	grid $w.ll -in $wc.l.l -sticky "nsew" -row 0 -column 0
	grid $w.lr -in $wc.l.r -sticky "nsew" -row 0 -column 0
	grid $wc.u.l -sticky "nsew" -row 0 -column 0
	grid $wc.u.r -sticky "nsew" -row 0 -column 1
	grid $wc.l.l -sticky "nsew" -row 0 -column 0
	grid $wc.l.r -sticky "nsew" -row 0 -column 1

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
}

proc mview_build_menubar { id } {
    global mged_top
    global mged_dmc

    set w $mged_top($id)

    if {$mged_top($id) == $mged_dmc($id)} {
	.$id.menubar clone $w.menubar menubar
	$w configure -menu $w.menubar

	menu_accelerator_bindings_for_clone $id $w $w.ul ul
	menu_accelerator_bindings_for_clone $id $w $w.ur ur
	menu_accelerator_bindings_for_clone $id $w $w.ll ll
	menu_accelerator_bindings_for_clone $id $w $w.lr lr
    } else {
	menu_accelerator_bindings $id $w.ul ul
	menu_accelerator_bindings $id $w.ur ur
	menu_accelerator_bindings $id $w.ll ll
	menu_accelerator_bindings $id $w.lr lr
    }
}

proc menu_accelerator_bindings_for_clone { id parent w pos } {
    bind $w <Alt-ButtonPress-1> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-ButtonPress-2> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-F> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#file %X %Y; break"
    bind $w <Alt-f> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#file %X %Y; break"
    bind $w <Alt-E> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#edit %X %Y; break"
    bind $w <Alt-e> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#edit %X %Y; break"
    bind $w <Alt-C> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#create %X %Y; break"
    bind $w <Alt-c> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#create %X %Y; break"
    bind $w <Alt-V> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#view %X %Y; break"
    bind $w <Alt-v> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#view %X %Y; break"
    bind $w <Alt-R> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#viewring %X %Y; break"
    bind $w <Alt-r> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#viewring %X %Y; break"
    bind $w <Alt-S> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-s> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-M> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-m> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-T> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#tools %X %Y; break"
    bind $w <Alt-t> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#tools %X %Y; break"
    bind $w <Alt-H> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#help %X %Y; break"
    bind $w <Alt-h> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#help %X %Y; break"
}

proc menu_accelerator_bindings { id w pos } {
    bind $w <Alt-ButtonPress-1> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-ButtonPress-2> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
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
    bind $w <Alt-S> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-s> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-M> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-m> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-T> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.tools %X %Y; break"
    bind $w <Alt-t> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.tools %X %Y; break"
    bind $w <Alt-H> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.help %X %Y; break"
    bind $w <Alt-h> "set mged_dm_loc($id) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.help %X %Y; break"
}

proc packmv { id } {
    global mged_dmc
    global use_grid_gm

    if { $use_grid_gm } {
	grid $mged_dmc($id).u -sticky "nsew" -row 0 -column 0
	grid $mged_dmc($id).l -sticky "nsew" -row 1 -column 0
	grid columnconfigure $mged_dmc($id) 0 -weight 1
	grid rowconfigure $mged_dmc($id) 0 -weight 1
	grid rowconfigure $mged_dmc($id) 1 -weight 1
    } else {
	pack $mged_dmc($id).u $mged_dmc($id).l -expand 1 -fill both
    }
}


proc unpackmv { id } {
    global mged_dmc
    global use_grid_gm

    if { $use_grid_gm } {
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
    set height [expr [winfo screenheight $mged_top($id)] - 50]
    set width $height
    wm geometry $mged_top($id) $width\x$height
#    setmv $id

#    bind $mged_top($id).ul m "togglemv $id"
#    bind $mged_top($id).ur m "togglemv $id"
#    bind $mged_top($id).ll m "togglemv $id"
#    bind $mged_top($id).lr m "togglemv $id"
}

proc set_default_views { id } {
    global mged_top

    winset $mged_top($id).ul
    _mged_press reset
    ae 0 90

    winset $mged_top($id).ur
    _mged_press reset
    press 35,25

    winset $mged_top($id).ll
    _mged_press reset
    press front

    winset $mged_top($id).lr
    _mged_press reset
    press left
}

proc setmv { id } {
    global mged_top
    global mged_dmc
    global mged_comb
    global mged_show_cmd
    global mged_show_status
    global win_size
    global mged_multi_view
    global mged_active_dm
    global mged_small_dmc
    global mged_default_bd
    global use_grid_gm
    global debug_setmv

    incr debug_setmv

    set border_offset [expr $mged_default_bd * 2]

    if { $mged_multi_view($id) } {
	if { $use_grid_gm } {
	    set width [expr ([winfo width $mged_top($id)] - $border_offset) / 2]
	    set height [expr ([winfo height $mged_top($id)] - $border_offset) / 2]

	    if {$mged_comb($id)} {
		if {$mged_show_cmd($id)} {
		    set height [expr $height - [get_cmd_win_height $id]]
#		    set height [expr $height - [winfo height .$id.tf]]
		}

		if {$mged_show_status($id)} {
		    set height [expr $height - [winfo height .$id.status]]
		}
	    }

	    # In case of resize/reconfiguration --- resize everybody
	    winset $mged_top($id).ul
	    dm size $width $height
	    winset $mged_top($id).ur
	    dm size $width $height
	    winset $mged_top($id).ll
	    dm size $width $height
	    winset $mged_top($id).lr
	    dm size $width $height

	    grid $mged_active_dm($id) -in $mged_small_dmc($id) -sticky "nsew" -row 0 -column 0
	} else {
	    unpackmv $id

	    set mv_size [expr $win_size($id) / 2 - $border_offset]

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
	}

	packmv $id
    } else {
	winset $mged_active_dm($id)
	catch { unpackmv $id }

	if { $use_grid_gm } {
	    set width [expr [winfo width $mged_top($id)] - $border_offset]
	    set height [expr [winfo height $mged_top($id)] - $border_offset]

	    if {$mged_comb($id)} {
		if {$mged_show_cmd($id)} {
		    set height [expr $height - [get_cmd_win_height $id]]
#		    set height [expr $height - [winfo height .$id.tf]]
		}

		if {$mged_show_status($id)} {
		    set height [expr $height - [winfo height .$id.status]]
		}
	    }

	    dm size $width $height

	    grid $mged_active_dm($id) -in $mged_dmc($id) -sticky "nsew" -row 0 -column 0
	    grid columnconfigure $mged_dmc($id) 0 -weight 1
	    grid rowconfigure $mged_dmc($id) 0 -weight 1
	} else {
	    dm size $win_size($id) $win_size($id)
	    pack $mged_active_dm($id) -in $mged_dmc($id)
	}
    }
}

proc togglemv { id } {
    global mged_multi_view

    if $mged_multi_view($id) {
	set mged_multi_view($id) 0
    } else {
	set mged_multi_view($id) 1
    }

    setmv $id
}

