#
#                       Q R A Y . T C L
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
#	Control Panel for MGED's Query Ray behavior.
#

proc init_qray_control { id } {
    global player_screen
    global mged_qray_control
    global mged_qray_effects
    global qray_active_mouse
    global qray_use_air
    global qray_cmd_echo

    set top .$id.qray_control

    if [winfo exists $top] {
	raise $top
	set mged_qray_control($id) 1

	return
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF1 -relief groove -bd 2
    label $top.colorL -text "Query Ray Colors"
    frame $top.oddColorF
    frame $top.oddColorFF -relief sunken -bd 2
    label $top.oddColorL -text "odd" -anchor w
    entry $top.oddColorE -relief flat -width 12 -textvar qray_oddColor($id)
    menubutton $top.oddColorMB -relief raised -bd 2\
	    -menu $top.oddColorMB.m -indicatoron 1
    menu $top.oddColorMB.m -tearoff 0
    $top.oddColorMB.m add command -label black\
	    -command "set qray_oddColor($id) \"0 0 0\";\
	    set_WidgetRGBColor $top.oddColorMB \$qray_oddColor($id)"
    $top.oddColorMB.m add command -label white\
	    -command "set qray_oddColor($id) \"255 255 255\";\
	    set_WidgetRGBColor $top.oddColorMB \$qray_oddColor($id)"
    $top.oddColorMB.m add command -label red\
	    -command "set qray_oddColor($id) \"255 0 0\";\
	    set_WidgetRGBColor $top.oddColorMB \$qray_oddColor($id)"
    $top.oddColorMB.m add command -label green\
	    -command "set qray_oddColor($id) \"0 255 0\";\
	    set_WidgetRGBColor $top.oddColorMB \$qray_oddColor($id)"
    $top.oddColorMB.m add command -label blue\
	    -command "set qray_oddColor($id) \"0 0 255\";\
	    set_WidgetRGBColor $top.oddColorMB \$qray_oddColor($id)"
    $top.oddColorMB.m add command -label yellow\
	    -command "set qray_oddColor($id) \"255 255 0\";\
	    set_WidgetRGBColor $top.oddColorMB \$qray_oddColor($id)"
    $top.oddColorMB.m add command -label cyan\
	    -command "set qray_oddColor($id) \"0 255 255\";\
	    set_WidgetRGBColor $top.oddColorMB \$qray_oddColor($id)"
    $top.oddColorMB.m add command -label magenta\
	    -command "set qray_oddColor($id) \"255 0 255\";\
	    set_WidgetRGBColor $top.oddColorMB \$qray_oddColor($id)"
    $top.oddColorMB.m add separator
    $top.oddColorMB.m add command -label "Color Tool..."\
	    -command "qray_choose_color $id odd"
    grid $top.oddColorL -sticky "ew" -in $top.oddColorF
    grid $top.oddColorE $top.oddColorMB -sticky "ew" -in $top.oddColorFF
    grid columnconfigure $top.oddColorFF 0 -weight 1
    grid $top.oddColorFF -sticky "ew" -in $top.oddColorF
    grid columnconfigure $top.oddColorF 0 -weight 1

    frame $top.evenColorF
    frame $top.evenColorFF -relief sunken -bd 2
    label $top.evenColorL -text "even" -anchor w
    entry $top.evenColorE -relief flat -width 12 -textvar qray_evenColor($id)
    menubutton $top.evenColorMB -relief raised -bd 2\
	    -menu $top.evenColorMB.m -indicatoron 1
    menu $top.evenColorMB.m -tearoff 0
    $top.evenColorMB.m add command -label black\
	    -command "set qray_evenColor($id) \"0 0 0\";\
	    set_WidgetRGBColor $top.evenColorMB \$qray_evenColor($id)"
    $top.evenColorMB.m add command -label white\
	    -command "set qray_evenColor($id) \"255 255 255\";\
	    set_WidgetRGBColor $top.evenColorMB \$qray_evenColor($id)"
    $top.evenColorMB.m add command -label red\
	    -command "set qray_evenColor($id) \"255 0 0\";\
	    set_WidgetRGBColor $top.evenColorMB \$qray_evenColor($id)"
    $top.evenColorMB.m add command -label green\
	    -command "set qray_evenColor($id) \"0 255 0\";\
	    set_WidgetRGBColor $top.evenColorMB \$qray_evenColor($id)"
    $top.evenColorMB.m add command -label blue\
	    -command "set qray_evenColor($id) \"0 0 255\";\
	    set_WidgetRGBColor $top.evenColorMB \$qray_evenColor($id)"
    $top.evenColorMB.m add command -label yellow\
	    -command "set qray_evenColor($id) \"255 255 0\";\
	    set_WidgetRGBColor $top.evenColorMB \$qray_evenColor($id)"
    $top.evenColorMB.m add command -label cyan\
	    -command "set qray_evenColor($id) \"0 255 255\";\
	    set_WidgetRGBColor $top.evenColorMB \$qray_evenColor($id)"
    $top.evenColorMB.m add command -label magenta\
	    -command "set qray_evenColor($id) \"255 0 255\";\
	    set_WidgetRGBColor $top.evenColorMB \$qray_evenColor($id)"
    $top.evenColorMB.m add separator
    $top.evenColorMB.m add command -label "Color Tool..."\
	    -command "qray_choose_color $id even"
    grid $top.evenColorL -sticky "ew" -in $top.evenColorF
    grid $top.evenColorE $top.evenColorMB -sticky "ew" -in $top.evenColorFF
    grid columnconfigure $top.evenColorFF 0 -weight 1
    grid $top.evenColorFF -sticky "ew" -in $top.evenColorF
    grid columnconfigure $top.evenColorF 0 -weight 1

    frame $top.voidColorF
    frame $top.voidColorFF -relief sunken -bd 2
    label $top.voidColorL -text "void" -anchor w
    entry $top.voidColorE -relief flat -width 12 -textvar qray_voidColor($id)
    menubutton $top.voidColorMB -relief raised -bd 2\
	    -menu $top.voidColorMB.m -indicatoron 1
    menu $top.voidColorMB.m -tearoff 0
    $top.voidColorMB.m add command -label black\
	    -command "set qray_voidColor($id) \"0 0 0\";\
	    set_WidgetRGBColor $top.voidColorMB \$qray_voidColor($id)"
    $top.voidColorMB.m add command -label white\
	    -command "set qray_voidColor($id) \"255 255 255\";\
	    set_WidgetRGBColor $top.voidColorMB \$qray_voidColor($id)"
    $top.voidColorMB.m add command -label red\
	    -command "set qray_voidColor($id) \"255 0 0\";\
	    set_WidgetRGBColor $top.voidColorMB \$qray_voidColor($id)"
    $top.voidColorMB.m add command -label green\
	    -command "set qray_voidColor($id) \"0 255 0\";\
	    set_WidgetRGBColor $top.voidColorMB \$qray_voidColor($id)"
    $top.voidColorMB.m add command -label blue\
	    -command "set qray_voidColor($id) \"0 0 255\";\
	    set_WidgetRGBColor $top.voidColorMB \$qray_voidColor($id)"
    $top.voidColorMB.m add command -label yellow\
	    -command "set qray_voidColor($id) \"255 255 0\";\
	    set_WidgetRGBColor $top.voidColorMB \$qray_voidColor($id)"
    $top.voidColorMB.m add command -label cyan\
	    -command "set qray_voidColor($id) \"0 255 255\";\
	    set_WidgetRGBColor $top.voidColorMB \$qray_voidColor($id)"
    $top.voidColorMB.m add command -label magenta\
	    -command "set qray_voidColor($id) \"255 0 255\";\
	    set_WidgetRGBColor $top.voidColorMB \$qray_voidColor($id)"
    $top.voidColorMB.m add separator
    $top.voidColorMB.m add command -label "Color Tool..."\
	    -command "qray_choose_color $id void"
    grid $top.voidColorL -sticky "ew" -in $top.voidColorF
    grid $top.voidColorE $top.voidColorMB -sticky "ew" -in $top.voidColorFF
    grid columnconfigure $top.voidColorFF 0 -weight 1
    grid $top.voidColorFF -sticky "ew" -in $top.voidColorF
    grid columnconfigure $top.voidColorF 0 -weight 1

    frame $top.overlapColorF
    frame $top.overlapColorFF -relief sunken -bd 2
    label $top.overlapColorL -text "overlap" -anchor w
    entry $top.overlapColorE -relief flat -width 12 -textvar qray_overlapColor($id)
    menubutton $top.overlapColorMB -relief raised -bd 2\
	    -menu $top.overlapColorMB.m -indicatoron 1
    menu $top.overlapColorMB.m -tearoff 0
    $top.overlapColorMB.m add command -label black\
	    -command "set qray_overlapColor($id) \"0 0 0\";\
	    set_WidgetRGBColor $top.overlapColorMB \$qray_overlapColor($id)"
    $top.overlapColorMB.m add command -label white\
	    -command "set qray_overlapColor($id) \"255 255 255\";\
	    set_WidgetRGBColor $top.overlapColorMB \$qray_overlapColor($id)"
    $top.overlapColorMB.m add command -label red\
	    -command "set qray_overlapColor($id) \"255 0 0\";\
	    set_WidgetRGBColor $top.overlapColorMB \$qray_overlapColor($id)"
    $top.overlapColorMB.m add command -label green\
	    -command "set qray_overlapColor($id) \"0 255 0\";\
	    set_WidgetRGBColor $top.overlapColorMB \$qray_overlapColor($id)"
    $top.overlapColorMB.m add command -label blue\
	    -command "set qray_overlapColor($id) \"0 0 255\";\
	    set_WidgetRGBColor $top.overlapColorMB \$qray_overlapColor($id)"
    $top.overlapColorMB.m add command -label yellow\
	    -command "set qray_overlapColor($id) \"255 255 0\";\
	    set_WidgetRGBColor $top.overlapColorMB \$qray_overlapColor($id)"
    $top.overlapColorMB.m add command -label cyan\
	    -command "set qray_overlapColor($id) \"0 255 255\";\
	    set_WidgetRGBColor $top.overlapColorMB \$qray_overlapColor($id)"
    $top.overlapColorMB.m add command -label magenta\
	    -command "set qray_overlapColor($id) \"255 0 255\";\
	    set_WidgetRGBColor $top.overlapColorMB \$qray_overlapColor($id)"
    $top.overlapColorMB.m add separator
    $top.overlapColorMB.m add command -label "Color Tool..."\
	    -command "qray_choose_color $id overlap"
    grid $top.overlapColorL -sticky "ew" -in $top.overlapColorF
    grid $top.overlapColorE $top.overlapColorMB -sticky "ew" -in $top.overlapColorFF
    grid columnconfigure $top.overlapColorFF 0 -weight 1
    grid $top.overlapColorFF -sticky "ew" -in $top.overlapColorF
    grid columnconfigure $top.overlapColorF 0 -weight 1

    grid $top.colorL - -sticky "n" -in $top.gridF1 -padx 8 -pady 8
    grid $top.oddColorF $top.evenColorF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    grid $top.voidColorF $top.overlapColorF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    grid columnconfigure $top.gridF1 0 -weight 1
    grid columnconfigure $top.gridF1 1 -weight 1
    grid $top.gridF1 -sticky "ew" -padx 8 -pady 8

    frame $top.gridF2 -relief groove -bd 2
    frame $top.bnameF
    label $top.bnameL -text "Base Name" -anchor w
    entry $top.bnameE -relief sunken -bd 2 -textvar qray_basename($id)
    grid $top.bnameL -sticky "ew" -in $top.bnameF
    grid $top.bnameE -sticky "ew" -in $top.bnameF
    grid columnconfigure $top.bnameF 0 -weight 1
    grid $top.bnameF -sticky "ew" -in $top.gridF2 -padx 8 -pady 8
    grid columnconfigure $top.gridF2 0 -weight 1
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8

    frame $top.gridF3 -relief groove -bd 2
    label $top.effectsL -text "Effects" -anchor w
    checkbutton $top.cmd_echoCB -relief flat -text "Echo Cmd"\
	    -offvalue 0 -onvalue 1 -variable qray_cmd_echo($id)
    menubutton $top.effectsMB -textvariable qray_effects_text($id)\
	    -menu $top.effectsMB.m -indicatoron 1
    menu $top.effectsMB.m -tearoff 0
    $top.effectsMB.m add radiobutton -value t -variable qray_effects($id)\
	    -label "Text" -command "qray_effects $id"
    $top.effectsMB.m add radiobutton -value g -variable qray_effects($id)\
	    -label "Graphics" -command "qray_effects $id"
    $top.effectsMB.m add radiobutton -value b -variable qray_effects($id)\
	    -label "both" -command "qray_effects $id"
    grid $top.effectsL x $top.cmd_echoCB x $top.effectsMB\
	    -sticky "ew" -in $top.gridF3 -padx 8 -pady 8
    grid columnconfigure $top.gridF3 1 -weight 1
    grid columnconfigure $top.gridF3 3 -weight 1
    grid $top.gridF3 -sticky "ew" -padx 8 -pady 8

    frame $top.gridF4
    checkbutton $top.activeCB -relief flat -text "Active Mouse"\
	    -offvalue 0 -onvalue 1 -variable qray_active_mouse($id)
    checkbutton $top.use_airCB -relief flat -text "Use Air"\
	    -offvalue 0 -onvalue 1 -variable qray_use_air($id)
    button $top.advB -relief raised -text "Advanced..."\
	    -command "init_qray_adv $id"
    grid $top.activeCB $top.use_airCB x $top.advB -in $top.gridF4 -padx 8
    grid $top.gridF4 -sticky "ew" -padx 8 -pady 8

    frame $top.gridF5
    button $top.applyB -relief raised -text "Apply"\
	    -command "qray_apply $id"
    button $top.loadB -relief raised -text "Load"\
	    -command "qray_load $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top; set mged_qray_control($id) 0 }"
    grid $top.applyB x $top.loadB x $top.dismissB -sticky "ew" -in $top.gridF5
    grid columnconfigure $top.gridF5 1 -weight 1
    grid columnconfigure $top.gridF5 3 -weight 1
    grid $top.gridF5 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1

    qray_load $id
    set qray_effects($id) [qray effects]
    qray_effects $id

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top; set mged_qray_control($id) 0 }"
    wm geometry $top +$x+$y
    wm title $top "Query Ray Control Panel"
}

proc qray_apply { id } {
    global mged_active_dm
    global mged_use_air
    global mged_mouse_behavior
    global mged_qray_effects
    global qray_active_mouse
    global qray_use_air
    global qray_cmd_echo
    global qray_effects
    global qray_basename
    global qray_oddColor
    global qray_evenColor
    global qray_voidColor
    global qray_overlapColor
    global use_air
    global mouse_behavior

    winset $mged_active_dm($id)

    if {$qray_active_mouse($id)} {
	set mouse_behavior q
	set mged_mouse_behavior($id) q
    } elseif {$mouse_behavior == "q"} {
	set mouse_behavior d
	set mged_mouse_behavior($id) d
    }

    set use_air $qray_use_air($id)
    set mged_use_air($id) $qray_use_air($id)

    qray echo $qray_cmd_echo($id)

    qray effects $qray_effects($id)
    set mged_qray_effects($id) $qray_effects($id)

    qray basename $qray_basename($id)

    eval qray oddcolor $qray_oddColor($id)
    eval qray evencolor $qray_evenColor($id)
    eval qray voidcolor $qray_voidColor($id)
    eval qray overlapcolor $qray_overlapColor($id)
}

proc qray_load { id } {
    global mged_active_dm
    global mged_use_air
    global qray_active_mouse
    global qray_use_air
    global qray_cmd_echo
    global qray_effects
    global qray_basename
    global qray_oddColor
    global qray_evenColor
    global qray_voidColor
    global qray_overlapColor
    global use_air
    global mouse_behavior

    winset $mged_active_dm($id)

    set top .$id.qray_control

    if {$mouse_behavior == "q"} {
	set qray_active_mouse($id) 1
    } else {
	set qray_active_mouse($id) 0
    }

    set qray_use_air($id) $use_air
    set qray_cmd_echo($id) [qray echo]
    set qray_effects($id) [qray effects]
    set qray_basename($id) [qray basename]

    set qray_oddColor($id) [qray oddcolor]
    set_WidgetRGBColor $top.oddColorMB $qray_oddColor($id)

    set qray_evenColor($id) [qray evencolor]
    set_WidgetRGBColor $top.evenColorMB $qray_evenColor($id)

    set qray_voidColor($id) [qray voidcolor]
    set_WidgetRGBColor $top.voidColorMB $qray_voidColor($id)

    set qray_overlapColor($id) [qray overlapcolor]
    set_WidgetRGBColor $top.overlapColorMB $qray_overlapColor($id)
}

proc qray_choose_color { id ray } {
    global player_screen
    global qray_oddColor
    global qray_evenColor
    global qray_voidColor
    global qray_overlapColor

    set top .$id.qray_control

    set colors [chooseColor $top]

    if {[llength $colors] == 0} {
	return
    }

    if {[llength $colors] != 2} {
	mged_dialog .$id.gridDialog $player_screen($id)\
		"Error choosing a color!"\
		"Error choosing a color!"\
		"" 0 OK
	return
    }

    switch $ray {
	odd {
	    $top.oddColorMB configure -bg [lindex $colors 0]
	    set qray_oddColor($id) [lindex $colors 1]
	}
	even {
	    $top.evenColorMB configure -bg [lindex $colors 0]
	    set qray_evenColor($id) [lindex $colors 1]
	}
	void {
	    $top.voidColorMB configure -bg [lindex $colors 0]
	    set qray_voidColor($id) [lindex $colors 1]
	}
	overlap {
	    $top.overlapColorMB configure -bg [lindex $colors 0]
	    set qray_overlapColor($id) [lindex $colors 1]
	}
    }
}

proc qray_effects { id } {
    global mged_qray_effects
    global qray_effects_text
    global qray_effects

    set top .$id.qray_control

    switch $qray_effects($id) {
	t {
	    set qray_effects_text($id) "Text"
	}
	g {
	    set qray_effects_text($id) "Graphics"
	}
	b {
	    set qray_effects_text($id) "both"
	}
    }
}

proc init_qray_adv { id } {
    global player_screen
    global qray_fmt_ray
    global qray_fmt_head
    global qray_fmt_partition
    global qray_fmt_foot
    global qray_fmt_miss
    global qray_fmt_overlap

    set top .$id.qray_adv

    if [winfo exists $top] {
	raise $top

	return
    }

    toplevel $top -screen $player_screen($id)
    qray_load_fmt $id

    frame $top.gridF1 -relief groove -bd 2
    label $top.fmtL -text "Query Ray Formats"
    grid $top.fmtL -in $top.gridF1 -padx 8 -pady 8
    frame $top.rayF
    label $top.rayL  -text "Ray" -anchor w
    entry $top.rayE -relief sunken -bd 2 -width 135 -textvar qray_fmt_ray($id)
    grid $top.rayL -sticky "ew" -in $top.rayF
    grid $top.rayE -sticky "ew" -in $top.rayF
    grid columnconfigure $top.rayF 0 -weight 1
    grid $top.rayF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    frame $top.headF
    label $top.headL -text "Head" -anchor w
    entry $top.headE -relief sunken -bd 2 -width 135 -textvar qray_fmt_head($id)
    grid $top.headL -sticky "ew" -in $top.headF
    grid $top.headE -sticky "ew" -in $top.headF
    grid columnconfigure $top.headF 0 -weight 1
    grid $top.headF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    frame $top.partitionF
    label $top.partitionL -text "Partition" -anchor w
    entry $top.partitionE -relief sunken -bd 2 -width 140 -textvar qray_fmt_partition($id)
    grid $top.partitionL -sticky "ew" -in $top.partitionF
    grid $top.partitionE -sticky "ew" -in $top.partitionF
    grid columnconfigure $top.partitionF 0 -weight 1
    grid $top.partitionF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    frame $top.footF
    label $top.footL -text "Foot" -anchor w
    entry $top.footE -relief sunken -bd 2 -width 140 -textvar qray_fmt_foot($id)
    grid $top.footL -sticky "ew" -in $top.footF
    grid $top.footE -sticky "ew" -in $top.footF
    grid columnconfigure $top.footF 0 -weight 1
    grid $top.footF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    frame $top.missF
    label $top.missL -text "Miss" -anchor w
    entry $top.missE -relief sunken -bd 2 -width 140 -textvar qray_fmt_miss($id)
    grid $top.missL -sticky "ew" -in $top.missF
    grid $top.missE -sticky "ew" -in $top.missF
    grid columnconfigure $top.missF 0 -weight 1
    grid $top.missF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    frame $top.overlapF
    label $top.overlapL -text "Overlap" -anchor w
    entry $top.overlapE -relief sunken -bd 2 -width 140 -textvar qray_fmt_overlap($id)
    grid $top.overlapL -sticky "ew" -in $top.overlapF
    grid $top.overlapE -sticky "ew" -in $top.overlapF
    grid columnconfigure $top.overlapF 0 -weight 1
    grid $top.overlapF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    grid columnconfigure $top.gridF1 0 -weight 1
    grid $top.gridF1 -sticky "ew" -padx 8 -pady 8

    frame $top.gridF2
    button $top.applyB -relief raised -text "Apply"\
	    -command "qray_apply_fmt $id"
    button $top.loadB -relief raised -text "Load"\
	    -command "qray_load_fmt $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top; }"
    grid $top.applyB x $top.loadB x $top.dismissB -sticky "ew" -in $top.gridF2
    grid columnconfigure $top.gridF2 1 -weight 1
    grid columnconfigure $top.gridF2 3 -weight 1
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm geometry $top +$x+$y
    wm title $top "Query Ray Advanced Settings"
}

proc qray_apply_fmt { id } {
    global mged_active_dm
    global qray_fmt_ray
    global qray_fmt_head
    global qray_fmt_partition
    global qray_fmt_foot
    global qray_fmt_miss
    global qray_fmt_overlap

    winset $mged_active_dm($id)
    qray fmt r $qray_fmt_ray($id)
    qray fmt h $qray_fmt_head($id)
    qray fmt p $qray_fmt_partition($id)
    qray fmt f $qray_fmt_foot($id)
    qray fmt m $qray_fmt_miss($id)
    qray fmt o $qray_fmt_overlap($id)
}

proc qray_load_fmt { id } {
    global mged_active_dm
    global qray_fmt_ray
    global qray_fmt_head
    global qray_fmt_partition
    global qray_fmt_foot
    global qray_fmt_miss
    global qray_fmt_overlap

    winset $mged_active_dm($id)
    set qray_fmt_ray($id) [qray fmt r]
    set qray_fmt_head($id) [qray fmt h]
    set qray_fmt_partition($id) [qray fmt p]
    set qray_fmt_foot($id) [qray fmt f]
    set qray_fmt_miss($id) [qray fmt m]
    set qray_fmt_overlap($id) [qray fmt o]
}
