#
# Modifications -
#        (Bob Parker):
#             Generalized the code to accommodate multiple instances of the
#             user interface.
#

proc mmenu_set { i menu } {
    global mmenu
    global player_count
    global player_ids

    set mmenu($i) $menu
    for { set j 0} { $j < $player_count } { incr j } {
	set w .mmenu$player_ids($j)

	if {![winfo exists $w]} {
	    return
	}

	if { [llength $menu]<=0 } then {
	    pack forget $w.f$i
	    continue
	}

	$w.f$i.l delete 0 end
	foreach item $menu {
	    $w.f$i.l insert end $item
	}
	$w.f$i.l configure -height [llength $menu]

	if { [winfo ismapped $w.f$i]==0 } then {
	    set packcmd "pack $w.f$i -side top -fill both -expand yes"
	    for { set scan [expr $i-1] } { $scan >= 0 } { incr scan -1 } {
		if { [llength $mmenu($scan)]>0 } then {
		    lappend packcmd -after $w.f$scan
		    break
		}
	    }
	    for { set scan [expr $i+1] } { $scan < $mmenu(num) } { incr scan } {
		if { [llength $mmenu($scan)]>0 } then {
		    lappend packcmd -before $w.f$scan
		    break
		}
	    }
	    eval $packcmd
	}
    }
}

proc mmenu_init { id } {
    global mmenu
    global player_ids
    global player_count
    global player_screen

    set w .mmenu$id
    catch { destroy $w }
    toplevel $w -screen $player_screen($id)
    wm title $w "MGED Button Menu"

    label $w.state -textvariable mged_display(state)
    pack $w.state -side top
    
    set i 0
    set mmenu(num) 0

    foreach menu [mmenu_get] {
	incr mmenu(num)
	
	frame $w.f$i -relief raised -bd 1
	listbox $w.f$i.l -bd 2 -exportselection false
        pack $w.f$i.l -side left -fill both -expand yes

	bind $w.f$i.l <Double-Button-1> "doit_press $id %W"
	bind $w.f$i.l <Button-2> "handle_select %W %y; doit_press $id %W"

	mmenu_set $i $menu
	incr i
    }

    return
}

proc doit_press { id w } {
    cmd_set $id
    press [$w get [$w curselection]]
}
