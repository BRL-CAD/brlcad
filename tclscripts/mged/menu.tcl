#
# Modifications -
#        (Bob Parker):
#             Generalized the code to accommodate multiple instances of the
#             user interface.
#

proc mmenu_set { w id i menu } {
    global mmenu

#    do_edit_pulldown $id $i $menu

    if {![winfo exists $w]} {
	return
    }

    set mmenu($id,$i) $menu

    if { [llength $menu]<=0 } {
	pack forget $w.f$i
	return
    }

    $w.f$i.l delete 0 end
    foreach item $menu {
	$w.f$i.l insert end $item
    }
    $w.f$i.l configure -height [llength $menu]

    if { [winfo ismapped $w.f$i]==0 } {
	set packcmd "pack $w.f$i -side top -fill both -expand yes"
	for { set scan [expr $i-1] } { $scan >= 0 } { incr scan -1 } {
	    if { [llength $mmenu($id,$scan)]>0 } {
		lappend packcmd -after $w.f$scan
		break
	    }
	}
	for { set scan [expr $i+1] } { $scan < $mmenu($id,num) } { incr scan } {
	    if { [llength $mmenu($id,$scan)]>0 } then {
		lappend packcmd -before $w.f$scan
		break
	    }
	}
	eval $packcmd
    }
}


proc mmenu_init { id } {
    global mmenu
    global player_screen
    global mged_display

    cmd_set $id
    set w .mmenu$id
    catch { destroy $w }
    toplevel $w -screen $player_screen($id)

    label $w.state -textvariable mged_display(state)
    pack $w.state -side top
    
    set i 0
    set mmenu($id,num) 0

    foreach menu [mmenu_get] {
	incr mmenu($id,num)
	
	frame $w.f$i -relief raised -bd 1
	listbox $w.f$i.l -bd 2 -exportselection false
        pack $w.f$i.l -side left -fill both -expand yes

	bind $w.f$i.l <Button-1> "handle_select %W %y; doit_press $id %W"
	bind $w.f$i.l <Button-2> "handle_select %W %y; doit_press $id %W"

	mmenu_set $w $id $i $menu

	incr i
    }

    wm title $w "MGED Button Menu ($id)"
    wm protocol $w WM_DELETE_WINDOW "toggle_button_menu $id"
    wm resizable $w 0 0

    return
}

proc doit_press { id w } {
    cmd_set $id
    press [$w get [$w curselection]]
}

proc reconfig_mmenu { id } {
    global mmenu

    if {![winfo exists .mmenu$id]} {
	return
    }

    set w .mmenu$id
    for {set i 0} {$i < 3} {incr i} {
	catch { destroy $w.f$i } 
    }

    set i 0
    set mmenu($id,num) 0
    foreach menu [mmenu_get] {
	incr mmenu($id,num)
	
	frame $w.f$i -relief raised -bd 1
	listbox $w.f$i.l -bd 2 -exportselection false
        pack $w.f$i.l -side left -fill both -expand yes

	bind $w.f$i.l <Button-1> "handle_select %W %y; doit_press $id %W"
	bind $w.f$i.l <Button-2> "handle_select %W %y; doit_press $id %W"

	mmenu_set $w $id $i $menu

	incr i
    }
}

proc do_arb_edit_menu { menu1 menu2 menu3 } {
    global mged_players
    global mged_top
    global edit_type
    global transform_what
    global erotate_about
    global rotate_about_what
    global ecoords
    global coord_type

    if ![info exists mged_players] {
	return
    }

    set edit_type "none of above"
    foreach id $mged_players {
	.$id.m.options.m.cm_transform entryconfigure 3 -state normal
	set transform_what($id) "e"
	set_transform $id

	.$id.m.options.m.cm_origin entryconfigure 0 -command "set_erotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 1 -command "set_erotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 2 -command "set_erotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 3 -command "set_erotate_about $id"\
		-state normal
	.$id.m.options.m.cm_coord entryconfigure 0 -command "set_ecoords $id"
	.$id.m.options.m.cm_coord entryconfigure 1 -command "set_ecoords $id"
	.$id.m.options.m.cm_coord entryconfigure 2 -command "set_ecoords $id"\
		-state normal

	winset $mged_top($id).ul
	set rotate_about_what($id) $erotate_about
	set coord_type($id) $ecoords
	set_erotate_about $id
	set_ecoords $id

	.$id.m.edit.m entryconfigure 0 -state disabled
	.$id.m.edit.m entryconfigure 1 -state disabled
	.$id.m.edit.m entryconfigure 2 -state disabled

	.$id.m.edit.m insert 0 cascade -label "move edges" \
		-menu .$id.m.edit.m.cm_mvedges
	.$id.m.edit.m insert 1 cascade -label "move faces" \
		-menu .$id.m.edit.m.cm_mvfaces
	.$id.m.edit.m insert 2 cascade -label "rotate faces" \
		-menu .$id.m.edit.m.cm_rotfaces
	.$id.m.edit.m insert 3 separator
	.$id.m.edit.m insert 4 radiobutton -variable edit_type \
		-label "Rotate" -underline 0 -command "press srot"
	.$id.m.edit.m insert 5 radiobutton -variable edit_type \
		-label "Translate" -underline 0 -command "press sxy"
	.$id.m.edit.m insert 6 radiobutton -variable edit_type \
		-label "Scale" -underline 0 -command "press sscale"
	.$id.m.edit.m insert 7 radiobutton -variable edit_type \
		 -label "none of above" -command "press \"edit menu\""
	.$id.m.edit.m insert 8 separator
	.$id.m.edit.m insert 9 command -label "Reject" -underline 0 \
		-command "press reject"
	.$id.m.edit.m insert 10 command -label "Accept" -underline 0 \
		-command "press accept"
	.$id.m.edit.m insert 11 separator

	menu .$id.m.edit.m.cm_mvedges
	foreach item $menu1 {
	    if {$item != "RETURN"} {
		.$id.m.edit.m.cm_mvedges add radiobutton -variable edit_type -label $item \
			-command "press \"edit menu\"; press \"move edges\"; \
			press \"$item\""
	    }
	}

	menu .$id.m.edit.m.cm_mvfaces
	foreach item $menu2 {
	    if {$item != "RETURN"} {
		.$id.m.edit.m.cm_mvfaces add radiobutton -variable edit_type -label $item \
			-command "press \"edit menu\"; press \"move faces\"; \
			press \"$item\""
	    }
	}
    
	menu .$id.m.edit.m.cm_rotfaces
	foreach item $menu3 {
	    if {$item != "RETURN"} {
		.$id.m.edit.m.cm_rotfaces add radiobutton -variable edit_type -label $item \
			-command "press \"edit menu\"; press \"rotate faces\"; \
			press \"$item\""
	    }
	}
    }
}

proc do_edit_menu { menu1 } {
    global mged_display
    global mged_players
    global mged_top
    global edit_type
    global transform_what
    global erotate_about
    global rotate_about_what
    global ecoords
    global coord_type

    if ![info exists mged_players] {
	return
    }

    set edit_type "none of above"
    foreach id $mged_players {
	.$id.m.options.m.cm_transform entryconfigure 3 -state normal
	set transform_what($id) "e"
	set_transform $id

	.$id.m.options.m.cm_origin entryconfigure 0 -command "set_erotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 1 -command "set_erotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 2 -command "set_erotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 3 -command "set_erotate_about $id"\
		-state normal
	.$id.m.options.m.cm_coord entryconfigure 0 -command "set_ecoords $id"
	.$id.m.options.m.cm_coord entryconfigure 1 -command "set_ecoords $id"
	.$id.m.options.m.cm_coord entryconfigure 2 -command "set_ecoords $id"\
		-state normal

	winset $mged_top($id).ul
	set rotate_about_what($id) $erotate_about
	set coord_type($id) $ecoords
	set_erotate_about $id
	set_ecoords $id

	.$id.m.edit.m entryconfigure 0 -state disabled
	.$id.m.edit.m entryconfigure 1 -state disabled
	.$id.m.edit.m entryconfigure 2 -state disabled

	set i 0
	foreach item $menu1 {
	    if {$item != "RETURN"} {
		.$id.m.edit.m insert $i radiobutton -variable edit_type \
			-label $item -command "press \"$item\""
		incr i
	    }
	}

	if {[llength $menu1]} {
	    .$id.m.edit.m insert $i separator
	    incr i
	}

	if {$mged_display(state) == "SOL EDIT"} {
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Rotate" -underline 0 -command "press srot"
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Translate" -underline 0 -command "press sxy"
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Scale" -underline 0 -command "press sscale"
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "none of above" -command "set edit_solid_flag 0"
	    incr i
	    .$id.m.edit.m insert $i separator
	    incr i
	} else {
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Scale" -command "press \"Scale\""
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "X move" -command "press \"X move\""
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Y move" -command "press \"Y move\""
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "XY move" -command "press \"XY move\""
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Rotate" -command "press \"Rotate\""
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Scale X" -command "press \"Scale X\""
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Scale Y" -command "press \"Scale Y\""
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "Scale Z" -command "press \"Scale Z\""
	    incr i
	    .$id.m.edit.m insert $i radiobutton -variable edit_type \
		    -label "none of above" -command "set edit_object_flag 0"
	    incr i
	    .$id.m.edit.m insert $i separator
	    incr i
	}

	.$id.m.edit.m insert $i command -label "Reject" -underline 0 \
		-command "press reject"

	incr i
	.$id.m.edit.m insert $i command -label "Accept" -underline 0 \
		-command "press accept"

	incr i
	.$id.m.edit.m insert $i separator
    }
}

proc undo_edit_menu {} {
    global mged_players
    global mged_top
    global transform_what
    global rotate_about
    global rotate_about_what
    global coords
    global coord_type

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	while {1} {
	    if {[.$id.m.edit.m type 0] == "separator"} {
		.$id.m.edit.m delete 0
		continue
	    }

	    if {[.$id.m.edit.m entrycget 0 -label] != "Add"} {
		.$id.m.edit.m delete 0
	    } else {
		break
	    }
	}

	if {[winfo exists .$id.m.edit.m.cm_mvedges]} {
	    destroy .$id.m.edit.m.cm_mvedges
	    destroy .$id.m.edit.m.cm_mvfaces
	    destroy .$id.m.edit.m.cm_rotfaces
	}

	.$id.m.edit.m entryconfigure 0 -state normal
	.$id.m.edit.m entryconfigure 1 -state normal
	.$id.m.edit.m entryconfigure 2 -state normal

	.$id.m.options.m.cm_transform entryconfigure 3 -state disabled
	if {$transform_what($id) == "e"} {
	    set transform_what($id) "v"
	    set_transform $id
	}

	.$id.m.options.m.cm_origin entryconfigure 0 -command "set_rotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 1 -command "set_rotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 2 -command "set_rotate_about $id"
	.$id.m.options.m.cm_origin entryconfigure 3 -command "set_rotate_about $id"\
		-state disabled
	.$id.m.options.m.cm_coord entryconfigure 0 -command "set_coords $id"
	.$id.m.options.m.cm_coord entryconfigure 1 -command "set_coords $id"
	.$id.m.options.m.cm_coord entryconfigure 2 -command "set_coords $id"\
		-state disabled

	winset $mged_top($id).ul
	set rotate_about_what($id) $rotate_about
	set coord_type($id) $coords
	set_rotate_about $id
	set_coords $id
    }
}
