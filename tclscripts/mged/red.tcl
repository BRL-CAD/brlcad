#
#			R E D . T C L
#
#	Widget for editing a region.
#
#	Author - Robert G. Parker
#

check_externs "get_comb put_comb"

proc init_red { id } {
    global player_screen
    global red_name
    global red_isRegion
    global red_id
    global red_air
    global red_material
    global red_los
    global red_color
    global red_shader
    global red_inherit
    global red_comb

    set top .$id.do_red

    if [winfo exists $top] {
	raise $top
	return
    }

    set red_name($id) ""
    set red_isRegion($id) "Yes"
    set red_id($id) ""
    set red_air($id) ""
    set red_material($id) ""
    set red_los($id) ""
    set red_color($id) ""
    set red_shader($id) ""
    set red_inherit($id) ""
    set red_comb($id) ""

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
    frame $top.materialF
    frame $top.materialFF -relief sunken -bd 2
    frame $top.losF
    frame $top.losFF -relief sunken -bd 2
    frame $top.colorF
    frame $top.colorFF -relief sunken -bd 2
    frame $top.shaderF
    frame $top.shaderFF -relief sunken -bd 2
    frame $top.combF
    frame $top.combFF -relief sunken -bd 2

    label $top.nameL -text "Name" -anchor w
    entry $top.nameE -relief flat -width 12 -textvar red_name($id)

    label $top.idL -text "Region Id" -anchor w
    entry $top.idE -relief flat -width 12 -textvar red_id($id)

    label $top.airL -text "Air Code" -anchor w
    entry $top.airE -relief flat -width 12 -textvar red_air($id)

    label $top.materialL -text "Material" -anchor w
    entry $top.materialE -relief flat -width 12 -textvar red_material($id)

    label $top.losL -text "LOS" -anchor w
    entry $top.losE -relief flat -width 12 -textvar red_los($id)

    label $top.colorL -text "Color" -anchor w
    entry $top.colorE -relief flat -width 12 -textvar red_color($id)
    menubutton $top.colorMB -relief raised -bd 2\
	    -menu $top.colorMB.m -indicatoron 1
    menu $top.colorMB.m -tearoff 0
    $top.colorMB.m add command -label black\
	     -command "set red_color($id) \"0 0 0\"; red_set_colorMB $id"
    $top.colorMB.m add command -label white\
	     -command "set red_color($id) \"220 220 220\"; red_set_colorMB $id"
    $top.colorMB.m add command -label red\
	     -command "set red_color($id) \"220 0 0\"; red_set_colorMB $id"
    $top.colorMB.m add command -label green\
	     -command "set red_color($id) \"0 220 0\"; red_set_colorMB $id"
    $top.colorMB.m add command -label blue\
	     -command "set red_color($id) \"0 0 220\"; red_set_colorMB $id"
    $top.colorMB.m add command -label yellow\
	     -command "set red_color($id) \"220 220 0\"; red_set_colorMB $id"
    $top.colorMB.m add command -label cyan\
	    -command "set red_color($id) \"0 220 220\"; red_set_colorMB $id"
    $top.colorMB.m add command -label magenta\
	    -command "set red_color($id) \"220 0 220\"; red_set_colorMB $id"
    $top.colorMB.m add separator
    $top.colorMB.m add command -label "Color Tool..."\
	    -command "red_choose_color $id"

    label $top.shaderL -text "Shader" -anchor w
    entry $top.shaderE -relief flat -width 12 -textvar red_shader($id)
    menubutton $top.shaderMB -relief raised -bd 2\
	    -menu $top.shaderMB.m -indicatoron 1
    menu $top.shaderMB.m -tearoff 0
    $top.shaderMB.m add command -label default\
	    -command "set red_shader($id) default"
    $top.shaderMB.m add command -label plastic\
	    -command "set red_shader($id) plastic"
    $top.shaderMB.m add command -label mirror\
	    -command "set red_shader($id) mirror"
    $top.shaderMB.m add command -label glass\
	    -command "set red_shader($id) glass"

    label $top.combL -text "Boolean Expression:" -anchor w
    text $top.combT -relief sunken -bd 2 -width 40 -height 10\
	    -yscrollcommand "$top.gridF3.s set" -setgrid true
    scrollbar $top.gridF3.s -relief flat -command "$top.combT yview"

#    button $top.selectMaterialB -relief raised -text "Select Material"\
#	    -command "red_select_material $id"

    checkbutton $top.isRegionCB -relief raised -text "Is Region"\
	    -offvalue No -onvalue Yes -variable red_isRegion($id)\
	    -command "red_toggle_isRegion $id"
    checkbutton $top.inheritCB -relief raised -text "Inherit"\
	    -offvalue No -onvalue Yes -variable red_inherit($id)

    button $top.applyB -relief raised -text "Apply"\
	    -command "red_apply $id"
    button $top.resetB -relief raised -text "Reset"\
	    -command "red_reset $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.nameL -sticky "ew" -in $top.nameF
    grid $top.nameE -sticky "ew" -in $top.nameFF
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

    grid $top.materialL -sticky "ew" -in $top.materialF
    grid $top.materialE -sticky "ew" -in $top.materialFF
    grid $top.materialFF -sticky "ew" -in $top.materialF
    grid $top.materialF x x -sticky "ew" -in $top.gridF -pady 8
#    grid $top.selectMaterialB -row 3 -column 2 -sticky "sw" -in $top.gridF -pady 8
    grid columnconfigure $top.materialF 0 -weight 1
    grid columnconfigure $top.materialFF 0 -weight 1

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

    grid $top.applyB $top.resetB x $top.dismissB -sticky "ew"\
	    -in $top.gridF4 -pady 8
    grid columnconfigure $top.gridF4 2 -weight 1

    grid $top.gridF -sticky "ew" -padx 8 -pady 8
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8
    grid $top.gridF3 -sticky "nsew" -padx 8 -pady 8
    grid $top.gridF4 -sticky "ew" -padx 8 -pady 8
    grid rowconfigure $top 2 -weight 1
    grid columnconfigure $top 0 -weight 1

    bind $top.colorE <Return> "red_set_colorMB $id; break"
    red_set_colorMB $id

    bind $top.nameE <Return> "red_reset $id; break"

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "Region Editor"
}

proc red_apply { id } {
    global player_screen
    global red_name
    global red_isRegion
    global red_id
    global red_air
    global red_material
    global red_los
    global red_color
    global red_shader
    global red_inherit
    global red_comb

    set top .$id.do_red
    set red_comb($id) [$top.combT get 0.0 end]

    if {$red_isRegion($id)} {
	if {$red_id($id) < 0} {
	    mged_dialog .$id.redDialog $player_screen($id)\
		    "Bad region id!"\
		    "Region id must be >= 0"\
		    "" 0 OK
	    return
	}

	if {$red_air($id) < 0} {
	    mged_dialog .$id.redDialog $player_screen($id)\
		    "Bad air code!"\
		    "Air code must be >= 0"\
		    "" 0 OK
	    return
	}

	if {$red_id($id) == 0 && $red_air($id) == 0} {
	    mged_dialog .$id.redDialog $player_screen($id)\
		    "Warning: both region id and air code are 0"\
		    "Warning: both region id and air code are 0"\
		    "" 0 OK
	}

	set result [catch {put_comb $red_name($id) $red_isRegion($id)\
		$red_id($id) $red_air($id) $red_material($id) $red_los($id)\
		$red_color($id) $red_shader($id) $red_inherit($id)\
		$red_comb($id)} comb_error]

	if {$result} {
	    return $comb_error
	}

	return
    }

    set result [catch {put_comb $red_name($id) $red_isRegion($id)\
	    $red_color($id) $red_shader($id) $red_inherit($id)\
	    $red_comb($id)} comb_error]

    if {$result} {
	return $comb_error
    }
}

proc red_reset { id } {
    global player_screen
    global red_name
    global red_isRegion
    global red_id
    global red_air
    global red_material
    global red_los
    global red_color
    global red_shader
    global red_inherit
    global red_comb

    set top .$id.do_red

    if {$red_name($id) == ""} {
	mged_dialog .$id.redDialog $player_screen($id)\
		"You must specify a region/combination name!"\
		"You must specify a region/combination name!"\
		"" 0 OK
	return
    }

    set save_isRegion $red_isRegion($id)
    set result [catch {get_comb $red_name($id)} comb_defs]
    if {$result == 1} {
	mged_dialog .$id.redDialog $player_screen($id)\
		"red_reset: Error"\
		$comb_defs\
		"" 0 OK
	return
    }

    set red_isRegion($id) [lindex $comb_defs 1]

    if {$red_isRegion($id) == "Yes"} {
	set red_id($id) [lindex $comb_defs 2]
	set red_air($id) [lindex $comb_defs 3]
	set red_material($id) [lindex $comb_defs 4]
	set red_los($id) [lindex $comb_defs 5]
	set red_color($id) [lindex $comb_defs 6]
	set red_shader($id) [lindex $comb_defs 7]
	set red_inherit($id) [lindex $comb_defs 8]
	set red_comb($id) [lindex $comb_defs 9]
    } else {
	set red_color($id) [lindex $comb_defs 2]
	set red_shader($id) [lindex $comb_defs 3]
	set red_inherit($id) [lindex $comb_defs 4]
	set red_comb($id) [lindex $comb_defs 5]
    }

    set_WidgetRGBColor $top.colorMB $red_color($id)
    $top.combT delete 0.0 end
    $top.combT insert end $red_comb($id)

    if {$save_isRegion != $red_isRegion($id)} {
	red_toggle_isRegion $id
    }
}

proc red_toggle_isRegion { id } {
    global red_isRegion

    set top .$id.do_red
    grid remove $top.gridF

    if {$red_isRegion($id) == "Yes"} {
	grid forget $top.nameF $top.colorF $top.shaderF

	frame $top.idF
	frame $top.idFF -relief sunken -bd 2
	frame $top.airF
	frame $top.airFF -relief sunken -bd 2
	frame $top.materialF
	frame $top.materialFF -relief sunken -bd 2
	frame $top.losF
	frame $top.losFF -relief sunken -bd 2

	label $top.idL -text "Region Id" -anchor w
	entry $top.idE -relief flat -width 12 -textvar red_id($id)

	label $top.airL -text "Air Code" -anchor w
	entry $top.airE -relief flat -width 12 -textvar red_air($id)

	label $top.materialL -text "Material" -anchor w
	entry $top.materialE -relief flat -width 12 -textvar red_material($id)

	label $top.losL -text "LOS" -anchor w
	entry $top.losE -relief flat -width 12 -textvar red_los($id)

#	button $top.selectMaterialB -relief raised -text "Select Material"\
#		-command "red_select_material $id"

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

	grid $top.materialL -sticky "ew" -in $top.materialF
	grid $top.materialE -sticky "ew" -in $top.materialFF
	grid $top.materialFF -sticky "ew" -in $top.materialF
	grid $top.materialF x x -sticky "ew" -in $top.gridF -pady 8
#	grid $top.selectMaterialB -row 3 -column 2 -sticky "sw" -in $top.gridF -pady 8
	grid columnconfigure $top.materialF 0 -weight 1
	grid columnconfigure $top.materialFF 0 -weight 1
    } else {
	grid forget $top.nameF $top.idF $top.airF $top.materialF $top.losF\
		$top.colorF $top.shaderF
#	grid forget $top.nameF $top.idF $top.airF $top.materialF $top.losF\
#		$top.colorF $top.shaderF $top.selectMaterialB

	destroy $top.idL $top.idE
	destroy $top.airL $top.airE
	destroy $top.materialL $top.materialE
	destroy $top.losL $top.losE
#	destroy $top.selectMaterialB
	destroy $top.idF $top.idFF
	destroy $top.airF $top.airFF
	destroy $top.materialF $top.materialFF
	destroy $top.losF $top.losFF

	grid $top.nameF x x -sticky "ew" -in $top.gridF -pady 8
	grid $top.colorF x x -sticky "ew" -in $top.gridF -pady 8
	grid $top.shaderF x x -sticky "ew" -in $top.gridF -pady 8
    }

    grid $top.gridF
}

proc red_choose_color { id } {
    global player_screen
    global red_color

    set top .$id.do_red
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
    set red_color($id) [lindex $colors 1]
}

proc red_set_colorMB { id } {
    global red_color

    set top .$id.do_red
    set_WidgetRGBColor $top.colorMB $red_color($id)
}

#proc red_select_material { id } {
#    global player_screen
#    global red_material
#    global red_material_list
#
#    set top .$id.sel_material
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
#    listbox $top.materialLB -selectmode single -yscrollcommand "$top.materialS set"
#    scrollbar $top.materialS -relief flat -command "$top.materialLB yview"
#
#    label $top.materialL -text "Material List:" -anchor w
#    entry $top.materialE -width 12 -textvar red_material_list($id)
#
#    button $top.resetB -relief raised -text "Reset"\
#	    -command "load_material $id"
#    button $top.dismissB -relief raised -text "Dismiss"\
#	    -command "catch { destroy $top }"
#
#    grid $top.materialLB $top.materialS -sticky "nsew" -in $top.gridF
#    grid rowconfigure $top.gridF 0 -weight 1
#    grid columnconfigure $top.gridF 0 -weight 1
#
#    grid $top.materialL x -sticky "ew" -in $top.gridF2
#    grid $top.materialE $top.resetB -sticky "nsew" -in $top.gridF2
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
#    wm title $top "Select Material"
#}
#
#proc load_material { id } {
#    global red_material
#    global red_material_list
#}