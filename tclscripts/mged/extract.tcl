#
#			E X T R A C T . T C L
#
#	Widget for extracting objects out of the current MGED database.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_keep db_glob"

proc init_extractTool { id } {
    global player_screen
    global ex_file
    global ex_objects

    set top .$id.do_extract

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists ex_file($id)] {
	regsub \.g$ [_mged_opendb] .keep default_file
	set ex_file($id) $default_file
    }

    if ![info exists ex_objects($id)] {
	set ex_objects($id) ""
    }

    toplevel $top -screen $player_screen($id)

    frame $top.gridF
    frame $top.gridF2

    label $top.fileL -text "File Name" -anchor w
    entry $top.fileE -width 24 -textvar ex_file($id)

    label $top.objectsL -text "Objects..." -anchor w
    entry $top.objectsE -width 24 -textvar ex_objects($id)

    button $top.extractB -relief raised -text "Extract"\
	    -command "do_extract $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.fileE $top.fileL -sticky "ew" -in $top.gridF -pady 4
    grid $top.objectsE $top.objectsL -sticky "ew" -in $top.gridF -pady 4
    grid columnconfigure $top.gridF 0 -weight 1

    grid $top.extractB x $top.dismissB -in $top.gridF2
    grid columnconfigure $top.gridF2 1 -weight 1

    pack $top.gridF $top.gridF2 -side top -expand 1 -fill both\
	    -padx 8 -pady 8

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "Extract Objects..."
}

proc do_extract { id } {
    global player_screen
    global ex_file
    global ex_objects

    cmd_set $id
    set ex_cmd "_mged_keep"

    if {$ex_file($id) != ""} {
	if [file exists $ex_file($id)] {
	    set result [mged_dialog .$id.exDialog $player_screen($id)\
		    "Append to $ex_file($id)?"\
		    "Append to $ex_file($id)?"\
		    "" 0 OK CANCEL]

	    if {$result} {
		return
	    }
	}
    } else {
	mged_dialog .$id.exDialog $player_screen($id)\
		"No file name specified!"\
		"No file name specified!"\
		"" 0 OK

	return
    }

    append ex_cmd " $ex_file($id)"

    if {$ex_objects($id) != ""} {
	set globbed_str [db_glob $ex_objects($id)]
	append ex_cmd " $globbed_str"
    } else {
	mged_dialog .$id.exDialog $player_screen($id)\
		"No objects specified!"\
		"No objects specified!"\
		"" 0 OK

	return
    }

    set result [catch {eval $ex_cmd}]
    return $result
}