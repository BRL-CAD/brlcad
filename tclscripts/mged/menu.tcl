proc mmenu_set { i menu } {
    global mmenu
    set w .mmenu

    set mmenu($i) $menu
   
    if { [llength $menu]<=0 } then {
	pack forget $w.f$i
	return
    }

    .mmenu.f$i.l delete 0 end
    foreach item $menu {
	.mmenu.f$i.l insert end $item
    }
    .mmenu.f$i.l configure -height [llength $menu]

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

proc mmenu_init { } {
    global mmenu
    
    set w .mmenu
    catch { destroy $w }
    toplevel $w
    wm title $w "MGED Button Menu"

    label $w.state -textvariable mged_display(state)
    pack $w.state -side top
    
    set i 0
    set mmenu(num) 0

    foreach menu [mmenu_get] {
	incr mmenu(num)
	
	frame $w.f$i -relief raised -bd 1
#	scrollbar $w.f$i.s -command "$w.f$i.l yview"
#	listbox $w.f$i.l -bd 2 -yscroll "$w.f$i.s set"
#	pack $w.f$i.s -side right -fill y
	listbox $w.f$i.l -bd 2
        pack $w.f$i.l -side left -fill both -expand yes
	bind $w.f$i.l <Double-Button-1> {
	    press [selection get]
	}
	bind $w.f$i.l <Button-2> {
	    tkListboxBeginSelect %W [%W index @%x,%y]
	    press [selection get]
	}
	
	mmenu_set $i $menu
	incr i
    }

    set mmenu(num) $i
    return
}

mmenu_init
