#
# Description -
#	The routines herein are used to implement traditional MGED
#	menus in Tcl/Tk.
#
# Modifications -
#	Bob Parker:
#		*- Generalized the code to accommodate multiple instances of the
#		   classic MGED "Button Menu" interface.
#		*- Added procs to implement edit menus
#

proc mmenu_set { id i } {
    global mmenu

    set w .mmenu$id

    if {![winfo exists $w]} {
	return
    }

    set result [catch {mmenu_get $i} menu]
    if {$result != 0} {
	return
    }

    set mmenu($id,$i) $menu

    if { [llength $menu]<=0 } {
	grid forget $w.f$i
	return
    }

    $w.f$i.l delete 0 end
    foreach item $menu {
	$w.f$i.l insert end $item
    }
    $w.f$i.l configure -height [llength $menu]

    set row [expr $i + 1]
    grid $w.f$i -row $row -sticky nsew
}

proc mmenu_init { id } {
    global mmenu
    global mged_gui
    global mged_display

    cmd_win set $id
    set w .mmenu$id
    catch { destroy $w }
    toplevel $w -screen $mged_gui($id,screen)

    label $w.state -textvariable mged_display(state)
    grid $w.state -row 0
    
    set mmenu($id,num) 3

    for { set i 0 } { $i < $mmenu($id,num) } { incr i } {
	frame $w.f$i -relief raised -bd 1
	listbox $w.f$i.l -bd 2 -exportselection false
        grid $w.f$i.l -sticky nsew -row 0 -column 0
	grid columnconfigure $w.f$i 0 -weight 1
	grid rowconfigure $w.f$i 0 -weight 1

	bind $w.f$i.l <Button-1> "handle_select %W %y; mged_press $id %W; break"
	bind $w.f$i.l <Button-2> "handle_select %W %y; mged_press $id %W; break"

	mmenu_set $id $i
    }

    grid columnconfigure $w 0 -weight 1
    grid rowconfigure $w 1 -weight 1
    grid rowconfigure $w 2 -weight 1
    grid rowconfigure $w 3 -weight 1

    wm title $w "MGED Button Menu ($id)"
    wm protocol $w WM_DELETE_WINDOW "toggle_button_menu $id"
    wm resizable $w 0 0

    return
}

proc mged_press { id w } {
    cmd_win set $id
    press [$w get [$w curselection]]
}

proc reconfig_mmenu { id } {
    global mmenu

    if {![winfo exists .mmenu$id]} {
	return
    }

    set w .mmenu$id

    for { set i 0 } { $i < $mmenu($id,num) } { incr i } {
	mmenu_set $id $i
    }
}

proc do_arb_edit_menu { type menu1 menu2 menu3 } {
    global mged_players
    global mged_gui
    global mged_default
    global edit_type

    if ![info exists mged_players] {
	return
    }

    set edit_type "none of above"
    foreach id $mged_players {
	.$id.menubar.settings.transform entryconfigure 2 -state normal
	set mged_gui($id,transform) "e"
	set_transform $id

	.$id.menubar.settings.coord entryconfigure 2 -state normal
	set mged_gui($id,coords) "o"
	mged_apply $id "set coords $mged_gui($id,coords)"

	.$id.menubar.settings.origin entryconfigure 3 -state normal
	set mged_gui($id,rotate_about) "k"
	mged_apply $id "set rotate_about $mged_gui($id,rotate_about)"

	.$id.menubar.edit entryconfigure 0 -state disabled
	.$id.menubar.edit entryconfigure 1 -state disabled
#	.$id.menubar.edit entryconfigure 2 -state disabled
#	.$id.menubar.edit entryconfigure 3 -state disabled
#	.$id.menubar.edit entryconfigure 4 -state disabled

	.$id.menubar.edit insert 0 cascade -label "move edges" \
		-menu .$id.menubar.edit.mvedges
	.$id.menubar.edit insert 1 cascade -label "move faces" \
		-menu .$id.menubar.edit.mvfaces
	.$id.menubar.edit insert 2 cascade -label "rotate faces" \
		-menu .$id.menubar.edit.rotfaces
	.$id.menubar.edit insert 3 separator
	.$id.menubar.edit insert 4 radiobutton -variable edit_type \
		-label "Rotate" -underline 0 -command "press srot; \
		set mged_gui($id,transform) e; set_transform $id"
	.$id.menubar.edit insert 5 radiobutton -variable edit_type \
		-label "Translate" -underline 0 -command "press sxy; \
		set mged_gui($id,transform) e; set_transform $id"
	.$id.menubar.edit insert 6 radiobutton -variable edit_type \
		-label "Scale" -underline 0 -command "press sscale; \
		set mged_gui($id,transform) e; set_transform $id"
	.$id.menubar.edit insert 7 radiobutton -variable edit_type \
		 -label "none of above" -command "press \"edit menu\"; \
		 set mged_gui($id,transform) e; set_transform $id"
	.$id.menubar.edit insert 8 separator
	.$id.menubar.edit insert 9 command -label "Reject" -underline 0 \
		-command "press reject"
	.$id.menubar.edit insert 10 command -label "Accept" -underline 0 \
		-command "press accept"
	.$id.menubar.edit insert 11 separator

	menu .$id.menubar.edit.mvedges -tearoff $mged_default(tearoff_menus)
	foreach item $menu1 {
	    if {$item != "RETURN"} {
		.$id.menubar.edit.mvedges add radiobutton -variable edit_type -label $item \
			-command "press \"edit menu\"; press \"move edges\"; \
			press \"$item\"; set mged_gui($id,transform) e; set_transform $id"
	    }
	}

	menu .$id.menubar.edit.mvfaces -tearoff $mged_default(tearoff_menus)
	foreach item $menu2 {
	    if {$item != "RETURN"} {
		.$id.menubar.edit.mvfaces add radiobutton -variable edit_type -label $item \
			-command "press \"edit menu\"; press \"move faces\"; \
			press \"$item\"; set mged_gui($id,transform) e; set_transform $id"
	    }
	}
    
	menu .$id.menubar.edit.rotfaces -tearoff $mged_default(tearoff_menus)
	foreach item $menu3 {
	    if {$item != "RETURN"} {
		.$id.menubar.edit.rotfaces add radiobutton -variable edit_type -label $item \
			-command "press \"edit menu\"; press \"rotate faces\"; \
			press \"$item\"; set mged_gui($id,transform) e; set_transform $id"
	    }
	}
    }
}

proc do_edit_menu { type menu1 } {
    global mged_display
    global mged_players
    global mged_gui
    global edit_type

    if ![info exists mged_players] {
	return
    }

    set edit_type "none of above"
    foreach id $mged_players {
	.$id.menubar.settings.transform entryconfigure 2 -state normal
	set mged_gui($id,transform) "e"
	set_transform $id

	.$id.menubar.settings.coord entryconfigure 2 -state normal
	set mged_gui($id,coords) "o"
	mged_apply $id "set coords $mged_gui($id,coords)"

	.$id.menubar.settings.origin entryconfigure 3 -state normal
	set mged_gui($id,rotate_about) "k"
	mged_apply $id "set rotate_about $mged_gui($id,rotate_about)"

	.$id.menubar.edit entryconfigure 0 -state disabled
	.$id.menubar.edit entryconfigure 1 -state disabled
#	.$id.menubar.edit entryconfigure 2 -state disabled
#	.$id.menubar.edit entryconfigure 3 -state disabled
#	.$id.menubar.edit entryconfigure 4 -state disabled

	set i 0
	foreach item $menu1 {
	    if {$item != "RETURN"} {
		.$id.menubar.edit insert $i radiobutton -variable edit_type \
			-label $item -command "press \"edit menu\"; press \"$item\"; \
			set mged_gui($id,transform) e; set_transform $id"
		incr i
	    }
	}

	if {[llength $menu1]} {
	    .$id.menubar.edit insert $i separator
	    incr i
	}

	if {$mged_display(state) == "SOL EDIT"} {
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Rotate" -underline 0 -command "press srot; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Translate" -underline 0 -command "press sxy; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Scale" -underline 0 -command "press sscale; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "none of above" -command "set edit_solid_flag 0; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i separator
	    incr i
	} else {
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Scale" -command "press \"Scale\"; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "X move" -command "press \"X move\"; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Y move" -command "press \"Y move\"; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "XY move" -command "press \"XY move\"; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Rotate" -command "press \"Rotate\"; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Scale X" -command "press \"Scale X\"; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Scale Y" -command "press \"Scale Y\"; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "Scale Z" -command "press \"Scale Z\"; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i radiobutton -variable edit_type \
		    -label "none of above" -command "set edit_object_flag 0; \
		    set mged_gui($id,transform) e; set_transform $id"
	    incr i
	    .$id.menubar.edit insert $i separator
	    incr i
	}

	.$id.menubar.edit insert $i command -label "Reject" -underline 0 \
		-command "press reject"

	incr i
	.$id.menubar.edit insert $i command -label "Accept" -underline 0 \
		-command "press accept"

	incr i
	.$id.menubar.edit insert $i separator
    }
}

proc undo_edit_menu {} {
    global mged_players
    global mged_gui

    if ![info exists mged_players] {
	return
    }

    foreach id $mged_players {
	destroy_edit_info $id

	while {1} {
	    if {[.$id.menubar.edit type 0] == "separator"} {
		.$id.menubar.edit delete 0
		continue
	    }

	    if {[.$id.menubar.edit entrycget 0 -label] != "Solid Selection..."} {
		.$id.menubar.edit delete 0
	    } else {
		break
	    }
	}

	if {[winfo exists .$id.menubar.edit.mvedges]} {
	    destroy .$id.menubar.edit.mvedges
	    destroy .$id.menubar.edit.mvfaces
	    destroy .$id.menubar.edit.rotfaces
	}

	.$id.menubar.edit entryconfigure 0 -state normal
	.$id.menubar.edit entryconfigure 1 -state normal
#	.$id.menubar.edit entryconfigure 2 -state normal
#	.$id.menubar.edit entryconfigure 3 -state normal
#	.$id.menubar.edit entryconfigure 4 -state normal

	.$id.menubar.settings.transform entryconfigure 2 -state disabled
	if {$mged_gui($id,transform) == "e"} {
	    set mged_gui($id,transform) "v"
	    set_transform $id
	}

	.$id.menubar.settings.coord entryconfigure 2 -state disabled
	if {$mged_gui($id,coords) == "o"} {
	    set mged_gui($id,coords) "v"
	    mged_apply $id "set coords $mged_gui($id,coords)"
	}

	.$id.menubar.settings.origin entryconfigure 3 -state disabled
	if {$mged_gui($id,rotate_about) == "k"} {
	    set mged_gui($id,rotate_about) "v"
	    mged_apply $id "set rotate_about $mged_gui($id,rotate_about)"
	}
    }
}
