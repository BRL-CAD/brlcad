#
#			L I S T . T C L
#
#  Author -
#	Robert G. Parker
#
#  Description -
#	Tcl routines for utilizing Tcl's listbox.
#

proc create_listbox { top screen type items abort_cmd } {
    toplevel $top -screen $screen
    frame $top.frame
    listbox $top.listbox -xscrollcommand "$top.hscrollbar set" -yscrollcommand "$top.vscrollbar set"
    foreach word $items {
	$top.listbox insert end $word
    }
    # right justify
    $top.listbox xview 1000
    scrollbar $top.hscrollbar -orient horizontal -command "$top.listbox xview"
    scrollbar $top.vscrollbar -command "$top.listbox yview"
    button $top.abortB -text "Abort $type Selection" \
	-command "$abort_cmd"

    grid $top.listbox $top.vscrollbar -sticky "nsew" -in $top.frame
    grid $top.hscrollbar x -sticky "nsew" -in $top.frame
    grid $top.frame -sticky "nsew" -padx 8 -pady 8
    grid $top.abortB -padx 8 -pady 8
    grid columnconfigure $top.frame 0 -weight 1
    grid rowconfigure $top.frame 0 -weight 1
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "$type Selection Menu"
}

proc bind_listbox { top event action } {
    bind $top.listbox $event "$action"
}

proc get_listbox_entry { w x y } {
    if ![winfo exists $w] {
	return
    }

    $w selection clear 0 end
    $w selection set @$x,$y

    return [$w get @$x,$y]
}


## - lbdcHack
#
# Listbox double click hack.
#
proc lbdcHack {w x y t id type path} {
    global mged_gui
    global mged_default

    switch $type {
	s -
	c {
	    set item [get_listbox_entry $w $x $y]
	}
	m {
	    set item [$w index @$x,$y]
	}
    }

    if {$mged_gui($id,lastButtonPress) == 0 ||
        $mged_gui($id,lastItem) != $item ||
        [expr {abs($mged_gui($id,lastButtonPress) - $t) > $mged_default(doubleClickTol)}]} {

	switch $type {
	    s {
		solid_illum $item
	    }
	    c {
		set spath [comb_get_solid_path $item]
		set path_pos [comb_get_path_pos $spath $item]
		matrix_illum $spath $path_pos
	    }
	    m {
		matrix_illum $path $item
	    }
	}
    } else {
	switch $type {
	    s {
		set mged_gui($id,mgs_path) $item
		bind $w <ButtonRelease-1> \
			"destroy [winfo parent $w]; break"
	    }
	    c {
		set mged_gui($id,mgc_comb) $item
		bind $w <ButtonRelease-1> \
			"destroy [winfo parent $w]; break"
	    }
	    m {
		set mged_gui($id,mgs_pos) $item
		bind $w <ButtonRelease-1> \
			"destroy [winfo parent $w]; break"
	    }
	}
    }

    set mged_gui($id,lastButtonPress) $t
    set mged_gui($id,lastItem) $item
}
