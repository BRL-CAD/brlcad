#
#			R T _ S C R I P T . T C L
#
#	Widget for producing RT script files of MGED's current view.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_saveview"

proc init_rtScriptTool { id } {
    global player_screen
    global rts_file
    global rts_args

    set top .$id.do_rtScript

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists rts_file($id)] {
	regsub \.g$ [_mged_opendb] .sh default_file
	set rts_file($id) $default_file
    }

    if ![info exists rts_args($id)] {
	set rts_args($id) ""
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF
    frame $top.gridF2

    label $top.fileL -text "File Name" -anchor w
    entry $top.fileE -width 12 -textvar rts_file($id)

    label $top.argsL -text "Other args..." -anchor w
    entry $top.argsE -width 12 -textvar rts_args($id)

    button $top.createB -relief raised -text "Create"\
	    -command "do_rtScript $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.fileE $top.fileL -sticky "ew" -in $top.gridF -pady 4
    grid $top.argsE $top.argsL -sticky "ew" -in $top.gridF -pady 4
    grid columnconfigure $top.gridF 0 -weight 1

    grid $top.createB x $top.dismissB -in $top.gridF2
    grid columnconfigure $top.gridF2 1 -weight 1

    pack $top.gridF $top.gridF2 -side top -expand 1 -fill both\
	    -padx 8 -pady 8

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "RT Script Tool..."
}

proc do_rtScript { id } {
    global player_screen
    global rts_file
    global rts_args

    cmd_set $id
    set rts_cmd "_mged_saveview"

    if {$rts_file($id) != ""} {
	if [file exists $rts_file($id)] {
	    set result [mged_dialog .$id.rtsDialog $player_screen($id)\
		    "Overwrite $rts_file($id)?"\
		    "Overwrite $rts_file($id)?"\
		    "" 0 OK CANCEL]

	    if {$result} {
		return
	    }
	}
    } else {
	mged_dialog .$id.rtsDialog $player_screen($id)\
		"No file name specified!"\
		"No file name specified!"\
		"" 0 OK

	return
    }

    append rts_cmd " $rts_file($id)"

    if {$rts_args($id) != ""} {
	append rts_cmd " $rts_args($id)"
    }

    catch {eval $rts_cmd}
}