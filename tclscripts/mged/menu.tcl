#
# Modifications -
#        (Bob Parker):
#             Generalized the code to accommodate multiple instances of the
#             user interface.
#

proc mmenu_set { w id i menu } {
    global mmenu

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

    wm title $w "$id\'s MGED Button Menu"
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
