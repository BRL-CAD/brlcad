##
#				C O L O R _ S C H E M E . T C L
#
# Author -
#	Robert G. Parker
#
# Source -
#       The U. S. Army Research Laboratory
#       Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#       Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description -
#	GUI for setting the color scheme for display manager windows.
#

proc color_scheme_init {} {
    global mged_color_scheme

    set mged_color_scheme(winter) { "255 255 255" "220 220 220" "0 0 200" "0 0 200"\
	    "0 0 0" "0 0 0" "255 0 0" "255 0 0" "100 255 100" "100 255 100"\
	    "100 100 255" "100 100 255" "0 0 0" "0 0 0" "0 0 0" "0 0 0"\
	    "100 100 255" "100 100 255" "255 0 0" "255 0 0" "0 0 0" "0 0 0" }
    set mged_color_scheme(storm) { "50 50 50" "30 30 30" "255 150 150" "255 150 150"\
	    "255 255 255" "255 255 255" "255 0 0" "255 0 0" "100 255 100" "100 255 100"\
	    "100 100 255" "100 100 255" "0 0 0" "0 0 0" "0 0 0" "0 0 0"\
	    "255 255 150" "255 255 150" "255 75 75" "255 75 75" "255 255 255" "255 255 255" }
    set mged_color_scheme(desert) { "255 200 150" "235 180 130" "139 69 19" "139 69 19"\
	    "0 0 0" "0 0 0" "255 0 0" "255 0 0" "50 200 50" "50 200 50"\
	    "100 100 255" "100 100 255" "0 0 0" "0 0 0" "0 0 0" "0 0 0"\
	    "100 100 0" "100 100 0" "100 0 0" "100 0 0" "0 0 0" "0 0 0" }
    set mged_color_scheme(blues) { "0 0 70" "0 0 50" "175 175 255" "175 175 255"\
	    "255 255 255" "255 255 255" "255 0 0" "255 0 0" "100 255 100" "100 255 100"\
	    "150 150 255" "150 150 255" "255 255 255" "255 255 255" "255 255 255" "255 255 255"\
	    "255 255 150" "255 255 150" "255 50 50" "255 50 50" "255 255 255" "255 255 255" }
    set mged_color_scheme(marine) { "0 70 70" "0 50 50" "0 200 200" "0 200 200"\
	    "255 255 255" "255 255 255" "255 0 0" "255 0 0" "100 255 100" "100 255 100"\
	    "150 150 255" "150 150 255" "255 255 255" "255 255 255" "255 255 255" "255 255 255"\
	    "255 255 100" "255 255 100" "255 0 0" "255 0 0" "255 255 255" "255 255 255" }
    set mged_color_scheme(iceBlue) { "255 255 255" "0 0 70" "0 0 200" "175 175 255"\
	    "0 0 0" "255 255 255" "255 0 0" "255 0 0" "100 255 100" "100 255 100"\
	    "100 100 255" "150 150 255" "0 0 0" "255 255 255" "0 0 0" "255 255 255"\
	    "100 100 255" "255 255 150" "255 0 0" "255 0 0" "0 0 0" "255 255 255" }
#    set mged_color_scheme() { "" "" "" ""\
#	    "" "" "" "" "" ""\
#	    "" "" "" "" "" ""\
#	    "" "" "" "" "" "" }

    set mged_color_scheme(primary_map) {\
	    { bg "Background" } \
	    { adc_line "ADC Lines" } \
	    { adc_tick "ADC Tick" } \
	    { geo_def "Geometry Default" } \
	    { geo_hl "Geometry Highlight" } \
	    { geo_label "Geometry Label" } \
	    { model_axes "Model Axes" } \
	    { model_axes_label "Model Axes Label" } \
	    { view_axes "View Axes" } \
	    { view_axes_label "View Axes Label" } \
	    { edit_axes1 "Edit Axes (Primary)" } \
	    { edit_axes_label1 "Edit Axes Label (Primary)" } \
	    { edit_axes2 "Edit Axes (Secondary)" } \
	    { edit_axes_label2 "Edit Axes Label (Secondary)" } \
	    { rubber_band "Rubber Band" } \
	    { grid "Grid" } \
	    { predictor "Predictor" } \
	}

    set mged_color_scheme(secondary_map) {\
	    { menu_line "Menu Lines" } \
	    { menu_title "Menu Title" } \
	    { menu_text2 "Menu Text" } \
	    { menu_text1 "Menu Text (Highlight)" } \
	    { state_text1 "Menu State Text 1" } \
	    { state_text2 "Menu State Text 2" } \
	    { menu_arrow "Menu Arrow" } \
	    { slider_line "Slider Lines" } \
	    { slider_text1 "Slider Text 1" } \
	    { slider_text2 "Slider Text 2" } \
	    { status_text1 "Status Text 1" } \
	    { status_text2 "Status Text 2" } \
	    { edit_info "Edit Info" } \
	    { center_dot "Center Dot" } \
	    { other_line "Other Lines" } \
	}
}

##
#
# Build the color scheme GUI.
#
proc color_scheme_build { id primary_title primary_map secondary_title secondary_map } {
    global mged_gui
    global mged_color_scheme

    set top .$id.color_scheme

    if [winfo exists $top] {
	raise $top
	return
    }

    # Initialize variables
    if ![info exists mged_color_scheme($id,smflag)] {
	set mged_color_scheme($id,smflag) 0
    }

    winset $mged_gui($id,active_dm)

    toplevel $top -screen $mged_gui($id,screen)
    set entry_width 12

    set row -1

    incr row
    set save_row $row
    set ce_row 0
    frame $top.csF$row
    label $top.activeL -text "Active Pane"
    label $top.inactiveL -text "Inactive Pane"
    grid x $top.activeL $top.inactiveL -row $ce_row -sticky "nsew" -in $top.csF$row

    # build primary entries
    color_scheme_build_entries $id $top $top.csF$row\
	    $mged_color_scheme(primary_map) ce_row $entry_width
    set ce_secondary_row $ce_row

    if $mged_color_scheme($id,smflag) {
	# build secondary entries
	color_scheme_build_entries $id $top $top.csF$row\
		$mged_color_scheme(secondary_map) ce_row $entry_width

	set height 800
    } else {
	# initialize secondary map variables
	color_scheme_reset $id $top

	set height 500
    }

    grid columnconfigure $top.csF$row 1 -weight 1
    grid columnconfigure $top.csF$row 2 -weight 1
    grid $top.csF$row -row $row -sticky "nsew" -padx 8 -pady 8
    grid rowconfigure $top $row -weight 1

    incr row
    checkbutton $top.smflagCB -relief raised -text $secondary_title\
	    -offvalue 0 -onvalue 1 -variable mged_color_scheme($id,smflag)\
	    -command "color_scheme_toggle_secondary $id $top $top.csF$save_row\
	    $ce_secondary_row $entry_width"
    grid $top.smflagCB -row $row -padx 8 -pady 8

    incr row
    frame $top.csF$row
    button $top.okB -relief raised -text "Ok"\
	    -command "color_scheme_ok $id $top"
    button $top.applyB -relief raised -text "Apply"\
	    -command "color_scheme_apply $id"
    button $top.resetB -relief raised -text "Reset"\
	    -command "color_scheme_reset $id $top"

    menubutton $top.cannedMB -relief raised -text "Canned" -bd 2\
	    -menu $top.cannedMB.m -indicatoron 1
    menu $top.cannedMB.m -tearoff 0
    $top.cannedMB.m add command -label Blues\
	    -command "color_scheme_load_canned $id $top $mged_color_scheme(blues)"
    $top.cannedMB.m add command -label Marine\
	    -command "color_scheme_load_canned $id $top $mged_color_scheme(marine)"
    $top.cannedMB.m add command -label Storm\
	    -command "color_scheme_load_canned $id $top $mged_color_scheme(storm)"
    $top.cannedMB.m add command -label Winter\
	    -command "color_scheme_load_canned $id $top $mged_color_scheme(winter)"
    $top.cannedMB.m add command -label "Ice Blue"\
	    -command "color_scheme_load_canned $id $top $mged_color_scheme(iceBlue)"
    $top.cannedMB.m add command -label Desert\
	    -command "color_scheme_load_canned $id $top $mged_color_scheme(desert)"
    $top.cannedMB.m add command -label Default\
	    -command "color_scheme_load_default $id $top"

    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.okB $top.applyB x $top.resetB $top.cannedMB x $top.dismissB\
	    -sticky "nsew" -in $top.csF$row
    grid columnconfigure $top.csF$row 2 -weight 1
    grid columnconfigure $top.csF$row 5 -weight 1
    grid $top.csF$row -row $row -sticky "nsew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1
    
    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top $primary_title
    wm geometry $top 500x$height\+$x+$y
}

## color_scheme_build_entries --
#
# Build the color entry rows. Each row contains
# the following:	name active_color_entry inactive_color_entry
#
proc color_scheme_build_entries { id top container map row entry_width } {
    global mged_color_scheme

    upvar $row ce_row

    foreach key_name_pair $map {
	set key [lindex $key_name_pair 0]
	set name [lindex $key_name_pair 1]
	set key_a $key\_a
	set key_ia $key\_ia
	set colorvar_a mged_color_scheme($id,$key_a)
	set colorvar_ia mged_color_scheme($id,$key_ia)

	incr ce_row
	label $top.$key\L -text $name
	set ce_a [color_entry_build $top $key_a $colorvar_a\
		"color_entry_chooser $id $top $key_a \"Active $name Color\" \
		mged_color_scheme $id,$key_a"\
		$entry_width [rset cs $key_a]]

	set ce_ia [color_entry_build $top $key_ia $colorvar_ia\
		"color_entry_chooser $id $top $key_ia \"Inactive $name Color\" \
		mged_color_scheme $id,$key_ia"\
		$entry_width [rset cs $key_ia]]

	grid $top.$key\L -row $ce_row -column 0 -sticky "e" -in $container -padx 2 -pady 2
	grid $ce_a -row $ce_row -column 1 -sticky "nsew" -in $container -padx 2 -pady 2
	grid $ce_ia -row $ce_row -column 2 -sticky "nsew" -in $container -padx 2 -pady 2
	grid rowconfigure $container $ce_row -weight 1
    }
}

proc color_scheme_toggle_secondary { id top container row entry_width } {
    global mged_color_scheme

    # save grid info for container, then remove
    set grid_info [grid info $container]
    grid forget $container

    set ce_row $row
    if $mged_color_scheme($id,smflag) {
	color_scheme_build_entries $id $top $container $mged_color_scheme(secondary_map) ce_row $entry_width
	set height 800
    } else {
	foreach key_name_pair $mged_color_scheme(secondary_map) {
	    set key [lindex $key_name_pair 0]
	    set key_a $key\_a
	    set key_ia $key\_ia

	    grid forget $top.$key\L
	    destroy $top.$key\L

	    color_entry_destroy $top $key_a
	    color_entry_destroy $top $key_ia
	}
	set height 500
    }

    # resize the toplevel window then update to prevent extra processing
    wm geometry $top 500x$height
    update

    # put back container
    eval grid $container $grid_info
}

proc color_scheme_ok { id top } {
    color_scheme_apply $id
    catch { destroy $top }
}

proc color_scheme_apply { id } {
    global mged_color_scheme

    foreach key_name_pair $mged_color_scheme(primary_map) {
	set key [lindex $key_name_pair 0]
	set key_a $key\_a
	set key_ia $key\_ia
	set color_a $mged_color_scheme($id,$key_a)
	set color_ia $mged_color_scheme($id,$key_ia)

	mged_apply_local $id "rset cs $key_a $color_a; rset cs $key_ia $color_ia"
    }

    foreach key_name_pair $mged_color_scheme(secondary_map) {
	set key [lindex $key_name_pair 0]
	set key_a $key\_a
	set key_ia $key\_ia
	set color_a $mged_color_scheme($id,$key_a)
	set color_ia $mged_color_scheme($id,$key_ia)

	mged_apply_local $id "rset cs $key_a $color_a; rset cs $key_ia $color_ia"
    }

    # force display manager windows to update their respective color schemes
    mged_apply_local $id "rset cs mode \[rset cs mode\]"
}

proc color_scheme_reset { id top } {
    global mged_gui
    global mged_color_scheme

    winset $mged_gui($id,active_dm)

    foreach key_name_pair $mged_color_scheme(primary_map) {
	set key [lindex $key_name_pair 0]
	set key_a $key\_a
	set key_ia $key\_ia
	set color_a [rset cs $key_a]
	set color_ia [rset cs $key_ia]

	set mged_color_scheme($id,$key_a) $color_a
	set mged_color_scheme($id,$key_ia) $color_ia
	color_entry_update $top $key_a $color_a
	color_entry_update $top $key_ia $color_ia
    }

    foreach key_name_pair $mged_color_scheme(secondary_map) {
	set key [lindex $key_name_pair 0]
	set key_a $key\_a
	set key_ia $key\_ia
	set color_a [rset cs $key_a]
	set color_ia [rset cs $key_ia]

	set mged_color_scheme($id,$key_a) $color_a
	set mged_color_scheme($id,$key_ia) $color_ia

	if $mged_color_scheme($id,smflag) {
	    color_entry_update $top $key_a $color_a
	    color_entry_update $top $key_ia $color_ia
	}
    }
}

proc color_scheme_load_default { id top } {
    global mged_color_scheme

    foreach key_name_pair $mged_color_scheme(primary_map) {
	set key [lindex $key_name_pair 0]
	set key_a $key\_a
	set key_ia $key\_ia
	set color_a [cs_def $key_a]
	set color_ia [cs_def $key_ia]

	set mged_color_scheme($id,$key_a) $color_a
	set mged_color_scheme($id,$key_ia) $color_ia
	color_entry_update $top $key_a $color_a
	color_entry_update $top $key_ia $color_ia
    }

    foreach key_name_pair $mged_color_scheme(secondary_map) {
	set key [lindex $key_name_pair 0]
	set key_a $key\_a
	set key_ia $key\_ia
	set color_a [cs_def $key_a]
	set color_ia [cs_def $key_ia]

	set mged_color_scheme($id,$key_a) $color_a
	set mged_color_scheme($id,$key_ia) $color_ia

	if $mged_color_scheme($id,smflag) {
	    color_entry_update $top $key_a $color_a
	    color_entry_update $top $key_ia $color_ia
	}
    }
}

## color_scheme_load_canned --
#
# Utility for loading the GUI with a canned color scheme.
#
proc color_scheme_load_canned { id top\
	bg_a bg_ia line_a line_ia\
	line_hl_a line_hl_ia geo_def_a geo_def_ia m_axes_a m_axes_ia\
	v_axes_a v_axes_ia e_axes1_a e_axes1_ia e_axes2_a e_axes2_ia\
	text1_a text1_ia text2_a text2_ia text3_a text3_ia } {
    global mged_color_scheme

    set mged_color_scheme($id,bg_a) $bg_a
    set mged_color_scheme($id,bg_ia) $bg_ia
    set mged_color_scheme($id,adc_line_a) $line_a
    set mged_color_scheme($id,adc_line_ia) $line_ia
    set mged_color_scheme($id,adc_tick_a) $line_hl_a
    set mged_color_scheme($id,adc_tick_ia) $line_hl_ia
    set mged_color_scheme($id,geo_def_a) $geo_def_a
    set mged_color_scheme($id,geo_def_ia) $geo_def_ia
    set mged_color_scheme($id,geo_hl_a) $line_hl_a
    set mged_color_scheme($id,geo_hl_ia) $line_hl_ia
    set mged_color_scheme($id,geo_label_a) $text1_a
    set mged_color_scheme($id,geo_label_ia) $text1_ia
    set mged_color_scheme($id,model_axes_a) $m_axes_a
    set mged_color_scheme($id,model_axes_ia) $m_axes_ia
    set mged_color_scheme($id,model_axes_label_a) $text1_a
    set mged_color_scheme($id,model_axes_label_ia) $text1_ia
    set mged_color_scheme($id,view_axes_a) $v_axes_a
    set mged_color_scheme($id,view_axes_ia) $v_axes_ia
    set mged_color_scheme($id,view_axes_label_a) $text1_a
    set mged_color_scheme($id,view_axes_label_ia) $text1_ia
    set mged_color_scheme($id,edit_axes1_a) $e_axes1_a
    set mged_color_scheme($id,edit_axes1_ia) $e_axes1_ia
    set mged_color_scheme($id,edit_axes2_a) $e_axes2_a
    set mged_color_scheme($id,edit_axes2_ia) $e_axes2_ia
    set mged_color_scheme($id,edit_axes_label1_a) $text1_a
    set mged_color_scheme($id,edit_axes_label1_ia) $text1_ia
    set mged_color_scheme($id,edit_axes_label2_a) $text1_a
    set mged_color_scheme($id,edit_axes_label2_ia) $text1_ia
    set mged_color_scheme($id,rubber_band_a) $line_hl_a
    set mged_color_scheme($id,rubber_band_ia) $line_hl_ia
    set mged_color_scheme($id,grid_a) $line_hl_a
    set mged_color_scheme($id,grid_ia) $line_hl_ia
    set mged_color_scheme($id,predictor_a) $line_hl_a
    set mged_color_scheme($id,predictor_ia) $line_hl_ia
    set mged_color_scheme($id,menu_line_a) $line_a
    set mged_color_scheme($id,menu_line_ia) $line_ia
    set mged_color_scheme($id,slider_line_a) $line_a
    set mged_color_scheme($id,slider_line_ia) $line_ia
    set mged_color_scheme($id,other_line_a) $line_a
    set mged_color_scheme($id,other_line_ia) $line_ia
    set mged_color_scheme($id,status_text1_a) $text3_a
    set mged_color_scheme($id,status_text1_ia) $text3_ia
    set mged_color_scheme($id,status_text2_a) $text1_a
    set mged_color_scheme($id,status_text2_ia) $text1_ia
    set mged_color_scheme($id,slider_text1_a) $text3_a
    set mged_color_scheme($id,slider_text1_ia) $text3_ia
    set mged_color_scheme($id,slider_text2_a) $text2_a
    set mged_color_scheme($id,slider_text2_ia) $text2_ia
    set mged_color_scheme($id,menu_text1_a) $text3_a
    set mged_color_scheme($id,menu_text1_ia) $text3_ia
    set mged_color_scheme($id,menu_text2_a) $text1_a
    set mged_color_scheme($id,menu_text2_ia) $text1_ia
    set mged_color_scheme($id,menu_title_a) $text2_a
    set mged_color_scheme($id,menu_title_ia) $text2_ia
    set mged_color_scheme($id,menu_arrow_a) $text3_a
    set mged_color_scheme($id,menu_arrow_ia) $text3_ia
    set mged_color_scheme($id,state_text1_a) $text3_a
    set mged_color_scheme($id,state_text1_ia) $text3_ia
    set mged_color_scheme($id,state_text2_a) $text1_a
    set mged_color_scheme($id,state_text2_ia) $text1_ia
    set mged_color_scheme($id,edit_info_a) $text1_a
    set mged_color_scheme($id,edit_info_ia) $text1_ia
    set mged_color_scheme($id,center_dot_a) $text1_a
    set mged_color_scheme($id,center_dot_ia) $text1_ia

    foreach key_name_pair $mged_color_scheme(primary_map) {
	set key [lindex $key_name_pair 0]
	set key_a $key\_a
	set key_ia $key\_ia

	color_entry_update $top $key_a $mged_color_scheme($id,$key_a)
	color_entry_update $top $key_ia $mged_color_scheme($id,$key_ia)
    }

    if $mged_color_scheme($id,smflag) {
	foreach key_name_pair $mged_color_scheme(secondary_map) {
	    set key [lindex $key_name_pair 0]
	    set key_a $key\_a
	    set key_ia $key\_ia

	    color_entry_update $top $key_a $mged_color_scheme($id,$key_a)
	    color_entry_update $top $key_ia $mged_color_scheme($id,$key_ia)
	}
    }
}
