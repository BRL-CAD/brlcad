#
#			P L O T . T C L
#
#	Widget for producing Unix Plot files of MGED's current view.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_pl"

proc init_plotTool { id } {
    global player_screen
    global pl_file_or_filter
    global pl_file
    global pl_filter
    global pl_zclip
    global pl_2d
    global pl_float

    set top .$id.do_plot

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists pl_file_or_filter($id)] {
	set pl_file_or_filter($id) file
    }

    if ![info exists pl_file($id)] {
	regsub \.g$ [_mged_opendb] .plot default_file
	set pl_file($id) $default_file
    }

    if ![info exists pl_filter($id)] {
	set pl_filter($id) ""
    }

    if ![info exists pl_zclip($id)] {
	set pl_zclip($id) 1
    }

    if ![info exists pl_2d($id)] {
	set pl_2d($id) 0
    }

    if ![info exists pl_float($id)] {
	set pl_float($id) 0
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF
    frame $top.gridF2
    frame $top.gridF3

    if {$pl_file_or_filter($id) == "file"} {
	set file_state normal
	set filter_state disabled
    } else {
	set filter_state normal
	set file_state disabled
    }

    entry $top.fileE -width 12 -textvar pl_file($id)\
	    -state $file_state
    radiobutton $top.fileRB -text "File Name" -anchor w\
	    -value file -variable pl_file_or_filter($id)\
	    -command "pl_set_file_state $id"

    entry $top.filterE -width 12 -textvar pl_filter($id)\
	    -state $filter_state
    radiobutton $top.filterRB -text "Filter" -anchor w\
	    -value filter -variable pl_file_or_filter($id)\
	    -command "pl_set_filter_state $id"

    checkbutton $top.zclip -relief raised -text "Z Clipping"\
	    -variable pl_zclip($id)
    checkbutton $top.twoDCB -relief raised -text "2D"\
	    -variable pl_2d($id)
    checkbutton $top.floatCB -relief raised -text "Float"\
	    -variable pl_float($id)

    button $top.createB -relief raised -text "Create"\
	    -command "do_plot $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.fileE $top.fileRB -sticky "ew" -in $top.gridF -pady 4
    grid $top.filterE $top.filterRB -sticky "ew" -in $top.gridF -pady 4
    grid columnconfigure $top.gridF 0 -weight 1

    grid $top.zclip x $top.twoDCB x $top.floatCB -sticky "ew"\
	    -in $top.gridF2 -padx 4 -pady 4
    grid columnconfigure $top.gridF2 1 -weight 1
    grid columnconfigure $top.gridF2 3 -weight 1

    grid $top.createB x $top.dismissB -sticky "ew" -in $top.gridF3 -pady 4
    grid columnconfigure $top.gridF3 1 -weight 1

    pack $top.gridF $top.gridF2 $top.gridF3 -side top -expand 1 -fill both\
	    -padx 8 -pady 8

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "Unix Plot Tool..."
}

proc do_plot { id } {
    global player_screen
    global pl_file_or_filter
    global pl_file
    global pl_filter
    global pl_zclip
    global pl_2d
    global pl_float

    cmd_set $id
    set pl_cmd "_mged_pl"

    if {$pl_zclip($id)} {
	append pl_cmd " -zclip"
    }

    if {$pl_2d($id)} {
	append pl_cmd " -2d"
    }

    if {$pl_float($id)} {
	append pl_cmd " -float"
    }

    if {$pl_file_or_filter($id) == "file"} {
	if {$pl_file($id) != ""} {
	    if [file exists $pl_file($id)] {
		set result [mged_dialog .$id.plotDialog $player_screen($id)\
			"Overwrite $pl_file($id)?"\
			"Overwrite $pl_file($id)?"\
			"" 0 OK CANCEL]

		if {$result} {
		    return
		}
	    }
	} else {
	    mged_dialog .$id.plotDialog $player_screen($id)\
		    "No file name specified!"\
		    "No file name specified!"\
		    "" 0 OK

	    return
	}

	append pl_cmd " $pl_file($id)"
    } else {
	if {$pl_filter($id) == ""} {
	    mged_dialog .$id.plotDialog $player_screen($id)\
		    "No filter specified!"\
		    "No filter specified!"\
		    "" 0 OK

	    return
	}

	append pl_cmd " |$pl_filter($id)"
    }

    catch {eval $pl_cmd}
}

proc pl_set_file_state { id } {
    set top .$id.do_plot

    $top.fileE configure -state normal
    $top.filterE configure -state disabled

    focus $top.fileE
}

proc pl_set_filter_state { id } {
    set top .$id.do_plot

    $top.filterE configure -state normal
    $top.fileE configure -state disabled

    focus $top.filterE
}