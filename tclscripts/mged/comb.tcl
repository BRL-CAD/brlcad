#
#			C O M B . T C L
#
#	Widget for editing a combination.
#
#	Author - Robert G. Parker
#

check_externs "get_comb put_comb"

proc init_comb { id } {
    global player_screen
    global mged_active_dm
    global comb_name
    global comb_isRegion
    global comb_id
    global comb_air
    global comb_gift
    global comb_los
    global comb_color
    global comb_shader
    global comb_inherit
    global comb_comb

    set top .comb$id

    if [winfo exists $top] {
	raise $top
	return
    }

    set comb_name($id) ""
    set comb_isRegion($id) "Yes"
    set comb_id($id) ""
    set comb_air($id) ""
    set comb_gift($id) ""
    set comb_los($id) ""
    set comb_color($id) ""
    set comb_shader($id) ""
    set comb_inherit($id) ""
    set comb_comb($id) ""

    toplevel $top -screen $player_screen($id)

    frame $top.gridF
    frame $top.gridF2
    frame $top.gridF3
    frame $top.gridF4
    frame $top.nameF
    frame $top.nameFF -relief sunken -bd 2
    frame $top.idF
    frame $top.idFF -relief sunken -bd 2
    frame $top.airF
    frame $top.airFF -relief sunken -bd 2
    frame $top.giftF
    frame $top.giftFF -relief sunken -bd 2
    frame $top.losF
    frame $top.losFF -relief sunken -bd 2
    frame $top.colorF
    frame $top.colorFF -relief sunken -bd 2
    frame $top.shaderF
    frame $top.shaderFF -relief sunken -bd 2
    frame $top.combF
    frame $top.combFF -relief sunken -bd 2

    label $top.nameL -text "Name" -anchor w
    entry $top.nameE -relief flat -width 12 -textvar comb_name($id)
    menubutton $top.nameMB -relief raised -bd 2\
	    -menu $top.nameMB.m -indicatoron 1
    menu $top.nameMB.m -tearoff 0
    $top.nameMB.m add command -label "Select from all displayed"\
	    -command "winset \$mged_active_dm($id); build_comb_menu_all"
    $top.nameMB.m add command -label "Select along ray"\
	    -command "winset \$mged_active_dm($id); set mouse_behavior c"

    label $top.idL -text "Region Id" -anchor w
    entry $top.idE -relief flat -width 12 -textvar comb_id($id)

    label $top.airL -text "Air Code" -anchor w
    entry $top.airE -relief flat -width 12 -textvar comb_air($id)

    label $top.giftL -text "Gift Material" -anchor w
    entry $top.giftE -relief flat -width 12 -textvar comb_gift($id)

    label $top.losL -text "LOS" -anchor w
    entry $top.losE -relief flat -width 12 -textvar comb_los($id)

    label $top.colorL -text "Color" -anchor w
    entry $top.colorE -relief flat -width 12 -textvar comb_color($id)
    menubutton $top.colorMB -relief raised -bd 2\
	    -menu $top.colorMB.m -indicatoron 1
    menu $top.colorMB.m -tearoff 0
    $top.colorMB.m add command -label black\
	     -command "set comb_color($id) \"0 0 0\"; comb_set_colorMB $id"
    $top.colorMB.m add command -label white\
	     -command "set comb_color($id) \"220 220 220\"; comb_set_colorMB $id"
    $top.colorMB.m add command -label red\
	     -command "set comb_color($id) \"220 0 0\"; comb_set_colorMB $id"
    $top.colorMB.m add command -label green\
	     -command "set comb_color($id) \"0 220 0\"; comb_set_colorMB $id"
    $top.colorMB.m add command -label blue\
	     -command "set comb_color($id) \"0 0 220\"; comb_set_colorMB $id"
    $top.colorMB.m add command -label yellow\
	     -command "set comb_color($id) \"220 220 0\"; comb_set_colorMB $id"
    $top.colorMB.m add command -label cyan\
	    -command "set comb_color($id) \"0 220 220\"; comb_set_colorMB $id"
    $top.colorMB.m add command -label magenta\
	    -command "set comb_color($id) \"220 0 220\"; comb_set_colorMB $id"
    $top.colorMB.m add separator
    $top.colorMB.m add command -label "Color Tool..."\
	    -command "comb_choose_color $id"

    label $top.shaderL -text "Shader" -anchor w
    entry $top.shaderE -relief flat -width 12 -textvar comb_shader($id)
    menubutton $top.shaderMB -relief raised -bd 2\
	    -menu $top.shaderMB.m -indicatoron 1
    menu $top.shaderMB.m -tearoff 0
    $top.shaderMB.m add command -label default\
	    -command "set comb_shader($id) default"
    $top.shaderMB.m add command -label plastic\
	    -command "set comb_shader($id) plastic"
    $top.shaderMB.m add command -label mirror\
	    -command "set comb_shader($id) mirror"
    $top.shaderMB.m add command -label glass\
	    -command "set comb_shader($id) glass"

    label $top.combL -text "Boolean Expression:" -anchor w
    text $top.combT -relief sunken -bd 2 -width 40 -height 10\
	    -yscrollcommand "$top.gridF3.s set" -setgrid true
    scrollbar $top.gridF3.s -relief flat -command "$top.combT yview"

#    button $top.selectGiftB -relief raised -text "Select Gift Material"\
#	    -command "comb_select_gift $id"

    checkbutton $top.isRegionCB -relief raised -text "Is Region"\
	    -offvalue No -onvalue Yes -variable comb_isRegion($id)\
	    -command "comb_toggle_isRegion $id"
    checkbutton $top.inheritCB -relief raised -text "Inherit"\
	    -offvalue No -onvalue Yes -variable comb_inherit($id)

    button $top.applyB -relief raised -text "Apply"\
	    -command "comb_apply $id"
    button $top.loadDefB -relief raised -text "Reset"\
	    -command "comb_load_defaults $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.nameL -sticky "ew" -in $top.nameF
    grid $top.nameE $top.nameMB -sticky "ew" -in $top.nameFF
    grid $top.nameFF -sticky "ew" -in $top.nameF
    grid $top.idL -sticky "ew" -in $top.idF
    grid $top.idE -sticky "ew" -in $top.idFF
    grid $top.idFF -sticky "ew" -in $top.idF
    grid $top.nameF x $top.idF -sticky "ew" -in $top.gridF -pady 8
    grid columnconfigure $top.nameF 0 -weight 1
    grid columnconfigure $top.nameFF 0 -weight 1
    grid columnconfigure $top.idF 0 -weight 1
    grid columnconfigure $top.idFF 0 -weight 1

    grid $top.colorL -sticky "ew" -in $top.colorF
    grid $top.colorE $top.colorMB -sticky "ew" -in $top.colorFF
    grid $top.colorFF -sticky "ew" -in $top.colorF
    grid $top.airL -sticky "ew" -in $top.airF
    grid $top.airE -sticky "ew" -in $top.airFF
    grid $top.airFF -sticky "ew" -in $top.airF
    grid $top.colorF x $top.airF -sticky "ew" -in $top.gridF -pady 8
    grid columnconfigure $top.colorF 0 -weight 1
    grid columnconfigure $top.colorFF 0 -weight 1
    grid columnconfigure $top.airF 0 -weight 1
    grid columnconfigure $top.airFF 0 -weight 1

    grid $top.shaderL -sticky "ew" -in $top.shaderF
    grid $top.shaderE -sticky "ew" -in $top.shaderFF
    grid $top.shaderFF -sticky "ew" -in $top.shaderF
    grid $top.losL -sticky "ew" -in $top.losF
    grid $top.losE -sticky "ew" -in $top.losFF
    grid $top.losFF -sticky "ew" -in $top.losF
    grid $top.shaderF x $top.losF -sticky "ew" -in $top.gridF -pady 8
    grid columnconfigure $top.shaderF 0 -weight 1
    grid columnconfigure $top.shaderFF 0 -weight 1
    grid columnconfigure $top.losF 0 -weight 1
    grid columnconfigure $top.losFF 0 -weight 1

    grid $top.giftL -sticky "ew" -in $top.giftF
    grid $top.giftE -sticky "ew" -in $top.giftFF
    grid $top.giftFF -sticky "ew" -in $top.giftF
    grid $top.giftF x x -sticky "ew" -in $top.gridF -pady 8
#    grid $top.selectGiftB -row 3 -column 2 -sticky "sw" -in $top.gridF -pady 8
    grid columnconfigure $top.giftF 0 -weight 1
    grid columnconfigure $top.giftFF 0 -weight 1

    grid $top.isRegionCB $top.inheritCB x -sticky "ew" -in $top.gridF2\
	    -ipadx 4 -ipady 4

    grid columnconfigure $top.gridF 0 -weight 1
    grid columnconfigure $top.gridF 1 -minsize 20
    grid columnconfigure $top.gridF 2 -weight 1
    grid columnconfigure $top.gridF2 2 -weight 1

    grid $top.combL x -sticky "w" -in $top.gridF3
    grid $top.combT $top.gridF3.s -sticky "nsew" -in $top.gridF3
    grid rowconfigure $top.gridF3 1 -weight 1
    grid columnconfigure $top.gridF3 0 -weight 1

    grid $top.applyB $top.loadDefB x $top.dismissB -sticky "ew"\
	    -in $top.gridF4 -pady 8
    grid columnconfigure $top.gridF4 2 -weight 1

    grid $top.gridF -sticky "ew" -padx 8 -pady 8
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8
    grid $top.gridF3 -sticky "nsew" -padx 8 -pady 8
    grid $top.gridF4 -sticky "ew" -padx 8 -pady 8
    grid rowconfigure $top 2 -weight 1
    grid columnconfigure $top 0 -weight 1

    bind $top.colorE <Return> "comb_set_colorMB $id"
    comb_set_colorMB $id

    bind $top.nameE <Return> "comb_load_defaults $id"

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "Combination Editor"
}

proc comb_apply { id } {
    global player_screen
    global comb_name
    global comb_isRegion
    global comb_id
    global comb_air
    global comb_gift
    global comb_los
    global comb_color
    global comb_shader
    global comb_inherit
    global comb_comb

    set top .comb$id
    set comb_comb($id) [$top.combT get 0.0 end]

    if {$comb_isRegion($id)} {
	if {$comb_id($id) < 0} {
	    mged_dialog .$id.combDialog $player_screen($id)\
		    "Bad region id!"\
		    "Region id must be >= 0"\
		    "" 0 OK
	    return
	}

	if {$comb_air($id) < 0} {
	    mged_dialog .$id.combDialog $player_screen($id)\
		    "Bad air code!"\
		    "Air code must be >= 0"\
		    "" 0 OK
	    return
	}

	if {$comb_id($id) == 0 && $comb_air($id) == 0} {
	    mged_dialog .$id.combDialog $player_screen($id)\
		    "Warning: both region id and air code are 0"\
		    "Warning: both region id and air code are 0"\
		    "" 0 OK
	}

	set result [catch {put_comb $comb_name($id) $comb_isRegion($id)\
		$comb_id($id) $comb_air($id) $comb_gift($id) $comb_los($id)\
		$comb_color($id) $comb_shader($id) $comb_inherit($id)\
		$comb_comb($id)} comb_error]

	if {$result} {
	    return $comb_error
	}

	return
    }

    set result [catch {put_comb $comb_name($id) $comb_isRegion($id)\
	    $comb_color($id) $comb_shader($id) $comb_inherit($id)\
	    $comb_comb($id)} comb_error]

    if {$result} {
	return $comb_error
    }
}

proc comb_load_defaults { id } {
    global player_screen
    global comb_name
    global comb_isRegion
    global comb_id
    global comb_air
    global comb_gift
    global comb_los
    global comb_color
    global comb_shader
    global comb_inherit
    global comb_comb

    set top .comb$id

    if {$comb_name($id) == ""} {
	mged_dialog .$id.combDialog $player_screen($id)\
		"You must specify a region/combination name!"\
		"You must specify a region/combination name!"\
		"" 0 OK
	return
    }

    set save_isRegion $comb_isRegion($id)
    set result [catch {get_comb $comb_name($id)} comb_defs]
    if {$result == 1} {
	mged_dialog .$id.combDialog $player_screen($id)\
		"comb_load_defaults: Error"\
		$comb_defs\
		"" 0 OK
	return
    }

    set comb_isRegion($id) [lindex $comb_defs 1]

    if {$comb_isRegion($id) == "Yes"} {
	set comb_id($id) [lindex $comb_defs 2]
	set comb_air($id) [lindex $comb_defs 3]
	set comb_gift($id) [lindex $comb_defs 4]
	set comb_los($id) [lindex $comb_defs 5]
	set comb_color($id) [lindex $comb_defs 6]
	set comb_shader($id) [lindex $comb_defs 7]
	set comb_inherit($id) [lindex $comb_defs 8]
	set comb_comb($id) [lindex $comb_defs 9]
    } else {
	set comb_color($id) [lindex $comb_defs 2]
	set comb_shader($id) [lindex $comb_defs 3]
	set comb_inherit($id) [lindex $comb_defs 4]
	set comb_comb($id) [lindex $comb_defs 5]
    }

    set_WidgetRGBColor $top.colorMB $comb_color($id)
    $top.combT delete 0.0 end
    $top.combT insert end $comb_comb($id)

    if {$save_isRegion != $comb_isRegion($id)} {
	comb_toggle_isRegion $id
    }
}

proc comb_toggle_isRegion { id } {
    global comb_isRegion

    set top .comb$id
    grid remove $top.gridF

    if {$comb_isRegion($id) == "Yes"} {
	grid forget $top.nameF $top.colorF $top.shaderF

	frame $top.idF
	frame $top.idFF -relief sunken -bd 2
	frame $top.airF
	frame $top.airFF -relief sunken -bd 2
	frame $top.giftF
	frame $top.giftFF -relief sunken -bd 2
	frame $top.losF
	frame $top.losFF -relief sunken -bd 2

	label $top.idL -text "Region Id" -anchor w
	entry $top.idE -relief flat -width 12 -textvar comb_id($id)

	label $top.airL -text "Air Code" -anchor w
	entry $top.airE -relief flat -width 12 -textvar comb_air($id)

	label $top.giftL -text "Gift Material" -anchor w
	entry $top.giftE -relief flat -width 12 -textvar comb_gift($id)

	label $top.losL -text "LOS" -anchor w
	entry $top.losE -relief flat -width 12 -textvar comb_los($id)

#	button $top.selectGiftB -relief raised -text "Select Gift Material"\
#		-command "comb_select_gift $id"

	grid $top.idL -sticky "ew" -in $top.idF
	grid $top.idE -sticky "ew" -in $top.idFF
	grid $top.idFF -sticky "ew" -in $top.idF
	grid $top.nameF x $top.idF -sticky "ew" -row 0 -in $top.gridF -pady 8
	grid columnconfigure $top.idF 0 -weight 1
	grid columnconfigure $top.idFF 0 -weight 1

	grid $top.airL -sticky "ew" -in $top.airF
	grid $top.airE -sticky "ew" -in $top.airFF
	grid $top.airFF -sticky "ew" -in $top.airF
	grid $top.colorF x $top.airF -sticky "ew" -in $top.gridF -pady 8
	grid columnconfigure $top.airF 0 -weight 1
	grid columnconfigure $top.airFF 0 -weight 1

	grid $top.losL -sticky "ew" -in $top.losF
	grid $top.losE -sticky "ew" -in $top.losFF
	grid $top.losFF -sticky "ew" -in $top.losF
	grid $top.shaderF x $top.losF -sticky "ew" -in $top.gridF -pady 8
	grid columnconfigure $top.losF 0 -weight 1
	grid columnconfigure $top.losFF 0 -weight 1

	grid $top.giftL -sticky "ew" -in $top.giftF
	grid $top.giftE -sticky "ew" -in $top.giftFF
	grid $top.giftFF -sticky "ew" -in $top.giftF
	grid $top.giftF x x -sticky "ew" -in $top.gridF -pady 8
#	grid $top.selectGiftB -row 3 -column 2 -sticky "sw" -in $top.gridF -pady 8
	grid columnconfigure $top.giftF 0 -weight 1
	grid columnconfigure $top.giftFF 0 -weight 1
    } else {
	grid forget $top.nameF $top.idF $top.airF $top.giftF $top.losF\
		$top.colorF $top.shaderF
#	grid forget $top.nameF $top.idF $top.airF $top.giftF $top.losF\
#		$top.colorF $top.shaderF $top.selectGiftB

	destroy $top.idL $top.idE
	destroy $top.airL $top.airE
	destroy $top.giftL $top.giftE
	destroy $top.losL $top.losE
#	destroy $top.selectGiftB
	destroy $top.idF $top.idFF
	destroy $top.airF $top.airFF
	destroy $top.giftF $top.giftFF
	destroy $top.losF $top.losFF

	grid $top.nameF x x -sticky "ew" -in $top.gridF -pady 8
	grid $top.colorF x x -sticky "ew" -in $top.gridF -pady 8
	grid $top.shaderF x x -sticky "ew" -in $top.gridF -pady 8
    }

    grid $top.gridF
}

proc comb_choose_color { id } {
    global player_screen
    global comb_color

    set top .comb$id
    set colors [chooseColor $top]

    if {[llength $colors] == 0} {
	return
    }

    if {[llength $colors] != 2} {
	mged_dialog .$id.combDialog $player_screen($id)\
		"Error choosing a color!"\
		"Error choosing a color!"\
		"" 0 OK
	return
    }

    $top.colorMB configure -bg [lindex $colors 0]
    set comb_color($id) [lindex $colors 1]
}

proc comb_set_colorMB { id } {
    global comb_color

    set top .comb$id
    set_WidgetRGBColor $top.colorMB $comb_color($id)
}

#proc comb_select_gift { id } {
#    global player_screen
#    global comb_gift
#    global comb_gift_list
#
#    set top .$id.sel_gift
#
#    if [winfo exists $top] {
#	raise $top
#	return
#    }
#
#    toplevel $top -screen $player_screen($id)
#
#    frame $top.gridF
#    frame $top.gridF2
#    frame $top.gridF3 -relief groove -bd 2
#
#    listbox $top.giftLB -selectmode single -yscrollcommand "$top.giftS set"
#    scrollbar $top.giftS -relief flat -command "$top.giftLB yview"
#
#    label $top.giftL -text "Gift List:" -anchor w
#    entry $top.giftE -width 12 -textvar comb_gift_list($id)
#
#    button $top.loadB -relief raised -text "Load"\
#	    -command "load_gift_material $id"
#    button $top.dismissB -relief raised -text "Dismiss"\
#	    -command "catch { destroy $top }"
#
#    grid $top.giftLB $top.giftS -sticky "nsew" -in $top.gridF
#    grid rowconfigure $top.gridF 0 -weight 1
#    grid columnconfigure $top.gridF 0 -weight 1
#
#    grid $top.giftL x -sticky "ew" -in $top.gridF2
#    grid $top.giftE $top.loadB -sticky "nsew" -in $top.gridF2
#    grid columnconfigure $top.gridF2 0 -weight 1
#
#    grid $top.dismissB -in $top.gridF3 -pady 8
#
#    grid $top.gridF -sticky "nsew" -padx 8 -pady 8
#    grid $top.gridF2 -sticky "ew" -padx 8 -pady 16
#    grid $top.gridF3 -sticky "ew"
#    grid rowconfigure $top 0 -weight 1
#    grid columnconfigure $top 0 -weight 1
#
#    wm title $top "Select Gift Material"
#}
#
#proc load_gift_material { id } {
#    global comb_gift
#    global comb_gift_list
#}