# Author - Bob Parker

check_externs "_mged_attach"

if ![info exists mged_default(bd)] {
    set mged_default(bd) 2
}

proc openmv { id w wc dpy dtype S } {
    global win_to_id
    global mged_default

    frame $wc.u
    frame $wc.l
    frame $wc.u.l -relief sunken -borderwidth $mged_default(bd)
    frame $wc.u.r -relief sunken -borderwidth $mged_default(bd)
    frame $wc.l.l -relief sunken -borderwidth $mged_default(bd)
    frame $wc.l.r -relief sunken -borderwidth $mged_default(bd)

    attach -d $dpy -t 0 -S $S -n $w.ul $dtype
    attach -d $dpy -t 0 -S $S -n $w.ur $dtype
    attach -d $dpy -t 0 -S $S -n $w.ll $dtype
    attach -d $dpy -t 0 -S $S -n $w.lr $dtype

    set win_to_id($w.ul) $id
    set win_to_id($w.ur) $id
    set win_to_id($w.ll) $id
    set win_to_id($w.lr) $id

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
}

proc mview_build_menubar { id } {
    global mged_gui

    set w $mged_gui($id,top)

    if {$mged_gui($id,top) == $mged_gui($id,dmc)} {
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
    bind $w <Alt-ButtonPress-1> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-ButtonPress-2> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-F> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#file %X %Y; break"
    bind $w <Alt-f> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#file %X %Y; break"
    bind $w <Alt-E> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#edit %X %Y; break"
    bind $w <Alt-e> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#edit %X %Y; break"
    bind $w <Alt-C> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#create %X %Y; break"
    bind $w <Alt-c> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#create %X %Y; break"
    bind $w <Alt-V> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#view %X %Y; break"
    bind $w <Alt-v> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#view %X %Y; break"
    bind $w <Alt-R> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#viewring %X %Y; break"
    bind $w <Alt-r> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#viewring %X %Y; break"
    bind $w <Alt-S> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-s> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-M> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-m> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-T> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#tools %X %Y; break"
    bind $w <Alt-t> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#tools %X %Y; break"
    bind $w <Alt-H> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#help %X %Y; break"
    bind $w <Alt-h> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#help %X %Y; break"
}

proc menu_accelerator_bindings { id w pos } {
    bind $w <Alt-ButtonPress-1> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-ButtonPress-2> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-F> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.file %X %Y; break"
    bind $w <Alt-f> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.file %X %Y; break"
    bind $w <Alt-E> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.edit %X %Y; break"
    bind $w <Alt-e> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.edit %X %Y; break"
    bind $w <Alt-C> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.create %X %Y; break"
    bind $w <Alt-c> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.create %X %Y; break"
    bind $w <Alt-V> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.view %X %Y; break"
    bind $w <Alt-v> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.view %X %Y; break"
    bind $w <Alt-R> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.viewring %X %Y; break"
    bind $w <Alt-r> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.viewring %X %Y; break"
    bind $w <Alt-S> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-s> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-M> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-m> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-T> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.tools %X %Y; break"
    bind $w <Alt-t> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.tools %X %Y; break"
    bind $w <Alt-H> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.help %X %Y; break"
    bind $w <Alt-h> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.help %X %Y; break"
}

proc packmv { id } {
    global mged_gui

    grid $mged_gui($id,dmc).u -sticky "nsew" -row 0 -column 0
    grid $mged_gui($id,dmc).l -sticky "nsew" -row 1 -column 0
    grid columnconfigure $mged_gui($id,dmc) 0 -weight 1
    grid rowconfigure $mged_gui($id,dmc) 0 -weight 1
    grid rowconfigure $mged_gui($id,dmc) 1 -weight 1
}


proc unpackmv { id } {
    global mged_gui

    grid forget $mged_gui($id,dmc).u $mged_gui($id,dmc).l
}

proc releasemv { id } {
    global mged_gui

    catch  { release $mged_gui($id,top).ul }
    catch  { release $mged_gui($id,top).ur }
    catch  { release $mged_gui($id,top).ll }
    catch  { release $mged_gui($id,top).lr }
}

proc closemv { id } {
    global mged_gui

    releasemv $id
    catch { destroy $mged_gui($id,dmc) }
}

proc setupmv { id } {
    global mged_gui
    global faceplate

    set_default_views $id
    mged_apply_local $id "set faceplate 0"
    set height [expr [winfo screenheight $mged_gui($id,top)] - 50]
    set width $height
    wm geometry $mged_gui($id,top) $width\x$height
#    setmv $id

#    bind $mged_gui($id,top).ul m "togglemv $id; break"
#    bind $mged_gui($id,top).ur m "togglemv $id; break"
#    bind $mged_gui($id,top).ll m "togglemv $id; break"
#    bind $mged_gui($id,top).lr m "togglemv $id; break"
}

proc set_default_views { id } {
    global mged_gui

    winset $mged_gui($id,top).ul
    _mged_press reset
    ae 0 90

    winset $mged_gui($id,top).ur
    _mged_press reset
    press 35,25

    winset $mged_gui($id,top).ll
    _mged_press reset
    press front

    winset $mged_gui($id,top).lr
    _mged_press reset
    press left
}

proc setmv { id } {
    global mged_gui
    global mged_default

    set border_offset [expr $mged_default(bd) * 2]

    if { $mged_gui($id,multi_view) } {
	set width [expr ([winfo width $mged_gui($id,top)] - $border_offset) / 2]
	set height [expr ([winfo height $mged_gui($id,top)] - $border_offset) / 2]

	if {$mged_gui($id,comb)} {
	    if {$mged_gui($id,show_cmd)} {
		set height [expr $height - [get_cmd_win_height $id]]
#		set height [expr $height - [winfo height .$id.tf]]
	    }

	    if {$mged_gui($id,show_status)} {
		set height [expr $height - [winfo height .$id.status]]
	    }
	}

	# In case of resize/reconfiguration --- resize everybody
	winset $mged_gui($id,top).ul
	dm size $width $height
	winset $mged_gui($id,top).ur
	dm size $width $height
	winset $mged_gui($id,top).ll
	dm size $width $height
	winset $mged_gui($id,top).lr
	dm size $width $height

	grid $mged_gui($id,active_dm) -in $mged_gui($id,small_dmc) -sticky "nsew" -row 0 -column 0

	packmv $id
    } else {
	winset $mged_gui($id,active_dm)
	catch { unpackmv $id }

	set width [expr [winfo width $mged_gui($id,top)] - $border_offset]
	set height [expr [winfo height $mged_gui($id,top)] - $border_offset]

	if {$mged_gui($id,comb)} {
	    if {$mged_gui($id,show_cmd)} {
		set height [expr $height - [get_cmd_win_height $id]]
#		set height [expr $height - [winfo height .$id.tf]]
	    }

	    if {$mged_gui($id,show_status)} {
		set height [expr $height - [winfo height .$id.status]]
	    }
	}

	dm size $width $height

	grid $mged_gui($id,active_dm) -in $mged_gui($id,dmc) -sticky "nsew" -row 0 -column 0
	grid columnconfigure $mged_gui($id,dmc) 0 -weight 1
	grid rowconfigure $mged_gui($id,dmc) 0 -weight 1
    }
}

proc togglemv { id } {
    global mged_gui

    if $mged_gui($id,multi_view) {
	set mged_gui($id,multi_view) 0
    } else {
	set mged_gui($id,multi_view) 1
    }

    setmv $id
}

