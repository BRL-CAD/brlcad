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

    if {[opendb] == ""} {
	cad_dialog .$id.uncool $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.do_extract

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists ex_control($id,file)] {
	regsub \.g$ [_mged_opendb] _keep.g default_file
	set ex_control($id,file) $default_file
    }

    set ex_control($id,objects) [_mged_who]

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF
    frame $top.gridF2

    set tmp_hoc_data {{summary "Enter a filename specifying where to put
the extracted objects."} {see_also keep}}
    label $top.fileL -text "File Name" -anchor w
    hoc_register_data $top.fileL "File Name" $tmp_hoc_data
    entry $top.fileE -width 24 -textvar ex_control($id,file)
    hoc_register_data $top.fileE "File Name" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter the objects to extract."}
	    {see_also keep}}
    label $top.objectsL -text "Objects" -anchor w
    hoc_register_data $top.objectsL "Objects" $tmp_hoc_data
    entry $top.objectsE -width 24 -textvar ex_control($id,objects)
    hoc_register_data $top.objectsE "Objects" $tmp_hoc_data

    button $top.okB -relief raised -text "OK"\
	    -command "do_extract $id; catch {destroy $top}"
    hoc_register_data $top.okB "Extract" {{summary "
Extract the listed objects from the current database
and put them into the specified file. The extract dialog
is then dismissed. Note - these objects are not removed
from the current database."} {see_also keep}}
    button $top.extractB -relief raised -text "Extract"\
	    -command "do_extract $id"
    hoc_register_data $top.extractB "Extract" {{summary "
Extract the listed objects from the current database
and put them into the specified file. Note - these
objects are not removed from the current database."} {see_also keep}}
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss" {{summary "Dismiss the entry dialog without
extracting database objects."}}

    grid $top.fileE $top.fileL -sticky "ew" -in $top.gridF -pady 4
    grid $top.objectsE $top.objectsL -sticky "ew" -in $top.gridF -pady 4
    grid columnconfigure $top.gridF 0 -weight 1

    grid  $top.okB $top.extractB x $top.dismissB -in $top.gridF2
    grid columnconfigure $top.gridF2 2 -weight 1

    pack $top.gridF $top.gridF2 -side top -expand 1 -fill both\
	    -padx 8 -pady 8

    place_near_mouse $top
    wm title $top "Extract Objects"
}

proc do_extract { id } {
    global mged_gui
    global ex_control

    cmd_win set $id
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