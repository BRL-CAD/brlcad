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
    listbox $top.listbox -yscrollcommand "$top.scrollbar set"
    foreach word $items {
	$top.listbox insert end $word
    }
    scrollbar $top.scrollbar -command "$top.listbox yview"
    button $top.abortB -text "Abort $type Selection" \
	-command "$abort_cmd"

    grid $top.listbox $top.scrollbar -sticky "nsew" -in $top.frame
    grid $top.frame -sticky "nsew" -padx 8 -pady 8
    grid $top.abortB -padx 8 -pady 8
    grid columnconfigure $top.frame 0 -weight 1
    grid rowconfigure $top.frame 0 -weight 1
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm geometry $top +$x+$y
    wm title $top "$type Selection Menu"
}

proc bind_listbox { top event action } {
    bind $top.listbox $event $action
}
