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
