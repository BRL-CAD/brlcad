#
#			E X T R A C T . T C L
#
#	Tool for extracting objects out of the current MGED database.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_keep db_glob"

proc init_extractTool { id } {
    global mged_gui
    global ex_control

    set top .$id.do_extract

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists ex_control($id,file)] {
	regsub \.g$ [_mged_opendb] .keep default_file
	set ex_control($id,file) $default_file
    }

    if { 1 } {
	set ex_control($id,objects) [_mged_who]
    } else {
	if ![info exists ex_control($id,objects)] {
	    set ex_control($id,objects) ""
	}
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF
    frame $top.gridF2

    label $top.fileL -text "File Name" -anchor w
    entry $top.fileE -width 24 -textvar ex_control($id,file)

    label $top.objectsL -text "Objects" -anchor w
    entry $top.objectsE -width 24 -textvar ex_control($id,objects)

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
    global mged_gui
    global ex_control

    cmd_set $id
    set ex_cmd "_mged_keep"

    if {$ex_control($id,file) != ""} {
	if [file exists $ex_control($id,file)] {
	    set result [cad_dialog .$id.exDialog $mged_gui($id,screen)\
		    "Append to $ex_control($id,file)?"\
		    "Append to $ex_control($id,file)?"\
		    "" 0 OK CANCEL]

	    if {$result} {
		return
	    }
	}
    } else {
	cad_dialog .$id.exDialog $mged_gui($id,screen)\
		"No file name specified!"\
		"No file name specified!"\
		"" 0 OK

	return
    }

    append ex_cmd " $ex_control($id,file)"

    if {$ex_control($id,objects) != ""} {
	set globbed_str [db_glob $ex_control($id,objects)]
	append ex_cmd " $globbed_str"
    } else {
	cad_dialog .$id.exDialog $mged_gui($id,screen)\
		"No objects specified!"\
		"No objects specified!"\
		"" 0 OK

	return
    }

    set result [catch {eval $ex_cmd}]
    return $result
}