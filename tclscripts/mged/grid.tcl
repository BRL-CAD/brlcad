#
#                       G R I D . T C L
#
# Author -
#	Robert G. Parker
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
# Description -
#	Control Panel for MGED's grid.

proc do_grid_spacing { id spacing_type } {
    global player_screen
    global grid_control_h
    global grid_control_v
    global grid_control_maj_h
    global grid_control_maj_v
    global localunit

    set top .$id.grid_spacing

    if [winfo exists $top] {
	raise $top

	return
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF1 -relief groove -bd 2
    frame $top.gridF2

    label $top.tickSpacingL -text "Tick Spacing\n($localunit per tick)"
    label $top.majorSpacingL -text "Major Spacing\n(ticks per major)"

    if {$spacing_type == "h"} {
	label $top.resL -text "Horiz." -anchor w
	entry $top.resE -relief sunken -width 12 -textvar grid_control_h($id)
	entry $top.maj_resE -relief sunken -width 12 -textvar grid_control_maj_h($id)
    } elseif {$spacing_type == "v"} {
	label $top.resL -text "Vert." -anchor w
	entry $top.resE -relief sunken -width 12 -textvar grid_control_v($id)
	entry $top.maj_resE -relief sunken -width 12 -textvar grid_control_maj_v($id)
    } elseif {$spacing_type == "b"} {
	label $top.resL -text "Horiz. & Vert." -anchor w
	entry $top.resE -relief sunken -width 12 -textvar grid_control_h($id)
	entry $top.maj_resE -relief sunken -width 12 -textvar grid_control_maj_h($id)
    } else {
	catch {destroy $top}
	return
    }

    button $top.applyB -relief raised -text "Apply"\
	    -command "grid_spacing_apply $id $spacing_type"
    button $top.loadB -relief raised -text "Load"\
	    -command "grid_spacing_load $id $spacing_type"
    button $top.autosizeB -relief raised -text "Autosize"\
	    -command "grid_control_autosize $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid x $top.tickSpacingL x $top.majorSpacingL -in $top.gridF1 -padx 8 -pady 8
    grid $top.resL $top.resE x $top.maj_resE -sticky "ew" -in $top.gridF1 -padx 8 -pady 8

    grid columnconfigure $top.gridF1 1 -weight 1
    grid columnconfigure $top.gridF1 3 -weight 1

    grid $top.applyB x $top.loadB $top.autosizeB x $top.dismissB -sticky "ew" -in $top.gridF2
    grid columnconfigure $top.gridF2 1 -weight 1
    grid columnconfigure $top.gridF2 1 -minsize 10
    grid columnconfigure $top.gridF2 4 -weight 1
    grid columnconfigure $top.gridF2 4 -minsize 10

    grid $top.gridF1 -sticky "ew" -padx 8 -pady 8
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1

    grid_spacing_load $id $spacing_type

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm geometry $top +$x+$y
    wm title $top "Grid Spacing"
}

proc do_grid_anchor { id } {
    global player_screen
    global grid_control_anchor
    global grid_anchor

    set top .$id.grid_anchor

    if [winfo exists $top] {
	raise $top

	return
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF1 -relief groove -bd 2
    frame $top.gridF2

    frame $top.anchorF

    label $top.anchorL -text "Anchor Point" -anchor w
    entry $top.anchorE -relief sunken -bd 2 -width 12 -textvar grid_control_anchor($id)

    button $top.applyB -relief raised -text "Apply"\
	    -command "doit $id \"set grid_anchor \\\$grid_control_anchor($id)\""
    button $top.loadB -relief raised -text "Load"\
	    -command "winset \$mged_active_dm($id); set grid_control_anchor($id) \$grid_anchor"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.anchorL -sticky "ew" -in $top.anchorF
    grid $top.anchorE -sticky "ew" -in $top.anchorF
    grid columnconfigure $top.anchorF 0 -weight 1

    grid $top.anchorF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    grid columnconfigure $top.gridF1 0 -weight 1

    grid $top.applyB x $top.loadB x $top.dismissB -sticky "ew" -in $top.gridF2
    grid columnconfigure $top.gridF2 1 -weight 1
    grid columnconfigure $top.gridF2 1 -weight 3

    grid $top.gridF1 -sticky "ew" -padx 8 -pady 8
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm geometry $top +$x+$y
    wm title $top "Grid Anchor Point"
}

proc do_grid_color { id } {
    global player_screen
    global mged_active_dm
    global grid_control_color
    global grid_color

    set top .$id.grid_color

    if [winfo exists $top] {
	raise $top

	return
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF1 -relief groove -bd 2
    frame $top.gridF2

    frame $top.colorF
    frame $top.colorFF -relief sunken -bd 2

    label $top.colorL -text "Color" -anchor w
    entry $top.colorE -relief flat -width 12 -textvar grid_control_color($id)
    menubutton $top.colorMB -relief raised -bd 2\
	    -menu $top.colorMB.m -indicatoron 1
    menu $top.colorMB.m -tearoff 0
    $top.colorMB.m add command -label black\
	    -command "set grid_control_color($id) \"0 0 0\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label white\
	    -command "set grid_control_color($id) \"255 255 255\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label red\
	    -command "set grid_control_color($id) \"255 0 0\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label green\
	    -command "set grid_control_color($id) \"0 255 0\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label blue\
	    -command "set grid_control_color($id) \"0 0 255\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label yellow\
	    -command "set grid_control_color($id) \"255 255 0\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label cyan\
	    -command "set grid_control_color($id) \"0 255 255\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label magenta\
	    -command "set grid_control_color($id) \"255 0 255\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add separator
    $top.colorMB.m add command -label "Color Tool..."\
	    -command "grid_control_choose_color $id $top"

    button $top.applyB -relief raised -text "Apply"\
	    -command "doit $id \"set grid_color \\\$grid_control_color($id)\""
    button $top.loadB -relief raised -text "Load"\
	    -command "winset \$mged_active_dm($id);\
	    set grid_control_color($id) \$grid_color;\
	    grid_control_set_colorMB $id $top;"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }" 

    grid $top.colorL -sticky "ew" -in $top.colorF
    grid $top.colorE $top.colorMB -sticky "ew" -in $top.colorFF
    grid $top.colorFF -sticky "ew" -in $top.colorF
    grid columnconfigure $top.colorFF 0 -weight 1
    grid columnconfigure $top.colorF 0 -weight 1

    grid $top.colorF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    grid columnconfigure $top.gridF1 0 -weight 1

    grid $top.applyB x $top.loadB x $top.dismissB -sticky "ew" -in $top.gridF2
    grid columnconfigure $top.gridF2 1 -weight 1
    grid columnconfigure $top.gridF2 3 -weight 1

    grid $top.gridF1 -sticky "ew" -padx 8 -pady 8
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1

    bind $top.colorE <Return> "grid_control_set_colorMB $id $top"

    winset $mged_active_dm($id)
    set grid_control_color($id) $grid_color
    grid_control_set_colorMB $id $top

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm geometry $top +$x+$y
    wm title $top "Grid Color"
}

proc init_grid_control { id } {
    global player_screen
    global mged_grid_control
    global mged_grid_draw
    global mged_grid_snap
    global grid_control_draw
    global grid_control_snap
    global grid_control_square
    global grid_control_color
    global grid_control_anchor
    global grid_control_h
    global grid_control_v
    global grid_control_maj_h
    global grid_control_maj_v
    global grid_draw
    global grid_snap
    global grid_color
    global grid anchor
    global grid_res_h
    global grid_res_v
    global grid_res_major_h
    global grid_res_major_v
    global localunit

    set top .$id.grid_control

    if [winfo exists $top] {
	raise $top
	set mged_grid_control($id) 1

	return
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF1
    frame $top.gridFF1 -relief groove -bd 2
    frame $top.gridF2
    frame $top.gridFF2 -relief groove -bd 2
    frame $top.gridF3
    frame $top.gridFF3 -relief groove -bd 2
    frame $top.gridF4

    frame $top.anchorF
    frame $top.anchorFF -relief sunken -bd 2
    frame $top.colorF
    frame $top.colorFF -relief sunken -bd 2

    label $top.tickSpacingL -text "Tick Spacing\n($localunit per tick)"
    label $top.majorSpacingL -text "Major Spacing\n(ticks per major)"

    label $top.hL -text "Horiz." -anchor w
    entry $top.hE -relief sunken -width 12 -textvar grid_control_h($id)
    entry $top.maj_hE -relief sunken -width 12 -textvar grid_control_maj_h($id)

    label $top.vL -text "Vert." -anchor w
    entry $top.vE -relief sunken -width 12 -textvar grid_control_v($id)
    entry $top.maj_vE -relief sunken -width 12 -textvar grid_control_maj_v($id)

    checkbutton $top.squareGridCB -relief flat -text "Square Grid"\
	    -offvalue 0 -onvalue 1 -variable grid_control_square($id)\
	    -command "set_grid_square $id"

    label $top.anchorL -text "Anchor Point" -anchor w
    entry $top.anchorE -relief flat -width 12 -textvar grid_control_anchor($id)

    label $top.colorL -text "Color" -anchor w
    entry $top.colorE -relief flat -width 12 -textvar grid_control_color($id)
    menubutton $top.colorMB -relief raised -bd 2\
	    -menu $top.colorMB.m -indicatoron 1
    menu $top.colorMB.m -tearoff 0
    $top.colorMB.m add command -label black\
	    -command "set grid_control_color($id) \"0 0 0\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label white\
	    -command "set grid_control_color($id) \"255 255 255\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label red\
	    -command "set grid_control_color($id) \"255 0 0\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label green\
	    -command "set grid_control_color($id) \"0 255 0\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label blue\
	    -command "set grid_control_color($id) \"0 0 255\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label yellow\
	    -command "set grid_control_color($id) \"255 255 0\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label cyan\
	    -command "set grid_control_color($id) \"0 255 255\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add command -label magenta\
	    -command "set grid_control_color($id) \"255 0 255\"; grid_control_set_colorMB $id $top"
    $top.colorMB.m add separator
    $top.colorMB.m add command -label "Color Tool..."\
	    -command "grid_control_choose_color $id $top"

    label $top.gridEffectsL -text "Grid Effects" -anchor w

    checkbutton $top.drawCB -relief flat -text "Draw"\
	    -offvalue 0 -onvalue 1 -variable grid_control_draw($id)

    checkbutton $top.snapCB -relief flat -text "Snap"\
	    -offvalue 0 -onvalue 1 -variable grid_control_snap($id)

    button $top.applyB -relief raised -text "Apply"\
	    -command "grid_control_apply $id"
    button $top.loadB -relief raised -text "Load"\
	    -command "grid_control_load $id $top"
    button $top.autosizeB -relief raised -text "Autosize"\
	    -command "grid_control_autosize $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top; set mged_grid_control($id) 0 }"

    grid x $top.tickSpacingL x $top.majorSpacingL -in $top.gridFF1 -padx 8 -pady 8
    grid $top.hL $top.hE x $top.maj_hE -sticky "ew" -in $top.gridFF1 -padx 8 -pady 8
    grid $top.vL $top.vE x $top.maj_vE -sticky "ew" -in $top.gridFF1 -padx 8 -pady 8
    grid $top.squareGridCB - - - -in $top.gridFF1 -padx 8 -pady 8

    grid $top.anchorL -sticky "ew" -in $top.anchorF
    grid $top.anchorE -sticky "ew" -in $top.anchorFF
    grid $top.anchorFF -sticky "ew" -in $top.anchorF
    grid $top.colorL -sticky "ew" -in $top.colorF
    grid $top.colorE $top.colorMB -sticky "ew" -in $top.colorFF
    grid $top.colorFF -sticky "ew" -in $top.colorF
    grid $top.anchorF x $top.colorF -sticky "ew" -in $top.gridFF2 -padx 8 -pady 8
    grid columnconfigure $top.anchorF 0 -weight 1
    grid columnconfigure $top.anchorFF 0 -weight 1
    grid columnconfigure $top.colorF 0 -weight 1
    grid columnconfigure $top.colorFF 0 -weight 1

    grid $top.gridEffectsL x $top.drawCB x $top.snapCB x -sticky "ew" -in $top.gridFF3\
	    -padx 8 -pady 8

    grid $top.applyB x $top.loadB $top.autosizeB x $top.dismissB -sticky "ew" -in $top.gridF4

    grid columnconfigure $top.gridFF1 1 -weight 1
    grid columnconfigure $top.gridFF1 3 -weight 1
    grid columnconfigure $top.gridF1 0 -weight 1
    grid $top.gridFF1 -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    grid $top.gridF1 -sticky "ew" -padx 8 -pady 8
    

    grid columnconfigure $top.gridFF2 0 -weight 1
    grid columnconfigure $top.gridFF2 1 -minsize 20
    grid columnconfigure $top.gridFF2 2 -weight 1
    grid columnconfigure $top.gridF2 0 -weight 1
    grid $top.gridFF2 -sticky "ew" -in $top.gridF2 -padx 8
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top.gridFF3 0 -weight 0
    grid columnconfigure $top.gridFF3 1 -weight 1
    grid columnconfigure $top.gridFF3 3 -minsize 20
    grid columnconfigure $top.gridFF3 5 -weight 1
    grid columnconfigure $top.gridF3 0 -weight 1
    grid $top.gridFF3 -sticky "ew" -in $top.gridF3 -padx 8 -pady 8
    grid $top.gridF3 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top.gridF4 1 -weight 1
    grid columnconfigure $top.gridF4 4 -weight 1
    grid $top.gridF4 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1
    grid columnconfigure $top 0 -minsize 400

    bind $top.colorE <Return> "grid_control_set_colorMB $id $top"

    grid_control_load $id $top
    grid_control_set_colorMB $id $top
    set_grid_square $id

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top; set mged_grid_control($id) 0 }"
    wm geometry $top +$x+$y
    wm title $top "Grid Control Panel"
}

proc grid_control_choose_color { id top } {
    global player_screen
    global grid_control_color

    set colors [chooseColor $top]

    if {[llength $colors] == 0} {
	return
    }

    if {[llength $colors] != 2} {
	mged_dialog .$id.redDialog $player_screen($id)\
		"Error choosing a color!"\
		"Error choosing a color!"\
		"" 0 OK
	return
    }

    $top.colorMB configure -bg [lindex $colors 0]
    set grid_control_color($id) [lindex $colors 1]
}

proc grid_control_set_colorMB { id top } {
    global grid_control_color

    set_WidgetRGBColor $top.colorMB $grid_control_color($id)
}

proc grid_control_apply { id } {
    global mged_active_dm
    global grid_control_draw
    global grid_control_snap
    global grid_control_color
    global grid_control_anchor
    global grid_control_h
    global grid_control_v
    global grid_control_maj_h
    global grid_control_maj_v
    global grid_draw
    global mged_grid_draw
    global grid_snap
    global mged_grid_snap
    global grid_color
    global grid_anchor
    global grid_res_h
    global grid_res_v
    global grid_res_major_h
    global grid_res_major_v
    global grid_control_square

    winset $mged_active_dm($id)
    doit $id "set grid_draw \$grid_control_draw($id)"
    doit $id "set grid_snap \$grid_control_snap($id)"
    doit $id "set grid_color \$grid_control_color($id)"
    doit $id "set grid_anchor \$grid_control_anchor($id)"

    doit $id "set grid_res_h \$grid_control_h($id)"
    doit $id "set grid_res_major_h \$grid_control_maj_h($id)"

    if {$grid_control_square($id)} {
	doit $id "set grid_res_v \$grid_control_h($id)"
	doit $id "set grid_res_major_v \$grid_control_maj_h($id)"
	doit $id "set grid_control_v($id) \$grid_control_h($id)"
	doit $id "set grid_control_maj_v($id) \$grid_control_maj_h($id)"
    } else {
	doit $id "set grid_res_v \$grid_control_v($id)"
	doit $id "set grid_res_major_v \$grid_control_maj_v($id)"
    }

    set mged_grid_draw($id) $grid_control_draw($id)
    set mged_grid_snap($id) $grid_control_snap($id)
}

proc grid_control_load { id top } {
    global mged_active_dm
    global grid_control_draw
    global grid_control_snap
    global grid_control_color
    global grid_control_anchor
    global grid_control_h
    global grid_control_v
    global grid_control_maj_h
    global grid_control_maj_v
    global grid_draw
    global mged_grid_draw
    global grid_snap
    global mged_grid_snap
    global grid_color
    global grid_anchor
    global grid_res_h
    global grid_res_v
    global grid_res_major_h
    global grid_res_major_v
    global grid_control_square

    winset $mged_active_dm($id)
    set grid_control_draw($id) $grid_draw
    set grid_control_snap($id) $grid_snap
    set grid_control_color($id) $grid_color
    set grid_control_anchor($id) $grid_anchor

    set grid_control_h($id) $grid_res_h
    set grid_control_maj_h($id) $grid_res_major_h

    if {!$grid_control_square($id)} {
	set grid_control_v($id) $grid_res_v
	set grid_control_maj_v($id) $grid_res_major_v
    }

    set mged_grid_draw($id) $grid_control_draw($id)
    set mged_grid_snap($id) $grid_control_snap($id)

    grid_control_set_colorMB $id $top
}

proc set_grid_square { id } {
    global grid_control_h
    global grid_control_v
    global grid_control_maj_h
    global grid_control_maj_v
    global grid_control_square

    set top .$id.grid_control
    if [winfo exists $top] {
	if {$grid_control_square($id)} {
	    $top.vE configure -textvar grid_control_h($id)
	    $top.maj_vE configure -textvar grid_control_maj_h($id)
	} else {
#	    set grid_control_v($id) $grid_control_h($id)
#	    set grid_control_maj_v($id) $grid_control_maj_h($id)

	    $top.vE configure -textvar grid_control_v($id)
	    $top.maj_vE configure -textvar grid_control_maj_v($id)
	}
    }
}

proc grid_control_update { sf } {
    global mged_players
    global grid_control_h
    global grid_control_v
    global grid_control_anchor
    global localunit

    foreach id $mged_players {
	if {[info exists grid_control_anchor($id)] &&\
		[llength $grid_control_anchor($id)] == 3} {
	    set x [lindex $grid_control_anchor($id) 0]
	    set y [lindex $grid_control_anchor($id) 1]
	    set z [lindex $grid_control_anchor($id) 2]

	    set x [expr $sf * $x]
	    set y [expr $sf * $y]
	    set z [expr $sf * $z]

	    set grid_control_anchor($id) "$x $y $z"
	}

	if [info exists grid_control_h($id)] {
	    set grid_control_h($id) [expr $sf * $grid_control_h($id)]
	    set grid_control_v($id) [expr $sf * $grid_control_v($id)]
	}

	set top .$id.grid_control
	if [winfo exists $top] {
	    $top.tickSpacingL configure -text "Tick Spacing\n($localunit per tick)"
	}

	set top .$id.grid_spacing
	if [winfo exists $top] {
	    $top.tickSpacingL configure -text "Tick Spacing\n($localunit per tick)"
	}
    }
}

proc grid_control_autosize { id } {
    global mged_display
    global mged_active_dm
    global grid_control_h
    global grid_control_v
    global grid_control_maj_h
    global grid_control_maj_v

    set result [regexp "^.*=\(.*\)" $mged_display($mged_active_dm($id),size) match size]
    set val [expr $size / 64]

    set grid_control_h($id) $val
    set grid_control_v($id) $val
    set grid_control_maj_h($id) 5
    set grid_control_maj_v($id) 5
}

proc grid_spacing_apply { id spacing_type } {
    global mged_active_dm
    global grid_control_h
    global grid_control_v
    global grid_control_maj_h
    global grid_control_maj_v
    global grid_res_h
    global grid_res_v
    global grid_res_major_h
    global grid_res_major_v
    global grid_control_square

    winset $mged_active_dm($id)

    if {$spacing_type == "h"} {
	set grid_control_square($id) 0
	set_grid_square $id
	set grid_control_v($id) $grid_res_v
	set grid_control_maj_v($id) $grid_res_major_v

	set grid_res_h $grid_control_h($id)
	set grid_res_major_h $grid_control_maj_h($id)
    } elseif {$spacing_type == "v"} {
#	set save_v $grid_control_v($id)
#	set save_maj_v $grid_control_maj_v($id)

	set grid_control_square($id) 0
	set_grid_square $id
	set grid_control_h($id) $grid_res_h
	set grid_control_maj_h($id) $grid_res_major_h

#	set grid_control_v($id) $save_v
#	set grid_control_maj_v($id) $save_maj_v
	set grid_res_v $grid_control_v($id)
	set grid_res_major_v $grid_control_maj_v($id)
    } else {
	set grid_res_h $grid_control_h($id)
	set grid_res_major_h $grid_control_maj_h($id)
	set grid_res_v $grid_control_h($id)
	set grid_res_major_v $grid_control_maj_h($id)
	set grid_control_v($id) $grid_control_h($id)
	set grid_control_maj_v($id) $grid_control_maj_h($id)
    }

    catch { destroy .$id.grid_spacing }
}

proc grid_spacing_load { id spacing_type } {
    global mged_active_dm
    global grid_control_h
    global grid_control_v
    global grid_control_maj_h
    global grid_control_maj_v
    global grid_res_h
    global grid_res_v
    global grid_res_major_h
    global grid_res_major_v

    winset $mged_active_dm($id)

    if {$spacing_type == "h"} {
	set grid_control_h($id) $grid_res_h
	set grid_control_maj_h($id) $grid_res_major_h
    } elseif {$spacing_type == "v"} {
	set grid_control_v($id) $grid_res_v
	set grid_control_maj_v($id) $grid_res_major_v
    } else {
	set grid_control_h($id) $grid_res_h
	set grid_control_maj_h($id) $grid_res_major_h
    }
}
