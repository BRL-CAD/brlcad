# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	Tcl/Tk interface to MGED's "make" command.

if ![info exists mged_default(solid_name_fmt)] {
    set mged_default(solid_name_fmt) "s@"
}

proc make_dsp { id top } {
    global mged_gui
    global tkPriv

	if { ![info exists mged_gui($id,solid_name)] } {
		cad_dialog $tkPriv(cad_dialog) $mged_gui($id,screen) "ERROR creating DSP" "No solid name provided!!!!" "" 0 OK
		return
	}

	if { [string length $mged_gui($id,solid_name)] < 1 } {
		cad_dialog $tkPriv(cad_dialog) $mged_gui($id,screen) "ERROR creating DSP" "No solid name provided!!!!" "" 0 OK
		return
	}

	set ret [catch {_mged_in $mged_gui($id,solid_name) dsp $mged_gui($id,dsp_file_name) $mged_gui($id,dsp_file_width) $mged_gui($id,dsp_file_length) $mged_gui($id,dsp_smooth) $mged_gui($id,dsp_cell_size) $mged_gui($id,dsp_elev_size)} result]
	if { $ret != 0 } {
		cad_dialog $tkPriv(cad_dialog) $mged_gui($id,screen) "ERROR creaing DSP" $reslult "" 0 OK
	}

	catch {_mged_sed $mged_gui($id,solid_name)}
	catch "destroy $top"
    }

proc dsp_create { id } {
	global mged_gui

	set top .$id.make_dsp

	if [winfo exists $top] {
		raise $top
		return
	}

	toplevel $top -screen $mged_gui($id,screen)

	set dsp_dscr "The DSP solid is a set of displacements on a regularly spaced grid. These displacements\n\
		are often used to represent terrain, but may be used for any surface where such displacement data\n\
		is available. The displacements are expected to be unsigned short integers (2 bytes each), stored\n\
		in binary format in a file."

	label $top.nameL -text "Primitive name:"
	entry $top.nameE -relief sunken -bd 2 -textvar mged_gui($id,solid_name)
	set tmp_hoc [list [list summary $dsp_dscr] [list description "This is the BRL-CAD name for this solid (it must be unique)" ]]
	hoc_register_data $top.nameL "Primitive Name" $tmp_hoc
	hoc_register_data $top.nameE "Primitive Name" $tmp_hoc
	label $top.fileL -text "File name:"
	entry $top.fileE -relief sunken -bd 2 -textvar mged_gui($id,dsp_file_name)
	set tmp_hoc [list [list summary $dsp_dscr] [list description "This is the name of the file containing the displacements" ]]
	hoc_register_data $top.fileL "File Name" $tmp_hoc
	hoc_register_data $top.fileE "File Name" $tmp_hoc
	label $top.widthL -text "File width:"
	entry $top.widthE -relief sunken -bd 2 -textvar mged_gui($id,dsp_file_width)
	set tmp_hoc [list [list summary $dsp_dscr] [list description "This is the width of the displacement map (number of values per row)" ]]
	hoc_register_data $top.widthL "File width" $tmp_hoc
	hoc_register_data $top.widthE "File width" $tmp_hoc
	label $top.heightL -text "File length:"
	entry $top.heightE -relief sunken -bd 2 -textvar mged_gui($id,dsp_file_length)
	set tmp_hoc [list [list summary $dsp_dscr] [list description "This is the length of the displacement map (number of rows)" ]]
	hoc_register_data $top.heightL "File width" $tmp_hoc
	hoc_register_data $top.heightE "File width" $tmp_hoc
	set mged_gui($id,dsp_smooth) 0
	label $top.smoothL -text "Use smoothing"
	checkbutton $top.smoothC -bd 2 -variable mged_gui($id,dsp_smooth)	
	set tmp_hoc [list [list summary $dsp_dscr] [list description "If this is checked, the displacememt surface will be smoothed" ]]
	hoc_register_data $top.smoothL "Smoothing" $tmp_hoc
	hoc_register_data $top.smoothC "Smoothing" $tmp_hoc

	label $top.celldimL -text "Cell size:"
	entry $top.celldimE -relief sunken -bd 2 -textvar mged_gui($id,dsp_cell_size)
	set tmp_hoc [list [list summary $dsp_dscr] [list description "This is the size of a cell in the X and Y dimension"]]
	hoc_register_data $top.celldimL "Cell Size" $tmp_hoc
	hoc_register_data $top.celldimE "Cell Size" $tmp_hoc

	label $top.cellelevL -text "Unit Elevation:"
	entry $top.cellelevE -relief sunken -bd 2 -textvar mged_gui($id,dsp_elev_size)
	set tmp_hoc [list [list summary $dsp_dscr] [list description "This is the distance represented by each step in elevation"]]

	hoc_register_data $top.cellelevL "Unit Elevation" $tmp_hoc
	hoc_register_data $top.cellelevE "Unit Elevation" $tmp_hoc


	button $top.applyB -text Apply -command "make_dsp $id $top"
	button $top.autonameB -text "Autoname" -command "solid_auto_name $id"
	button $top.dismissB -text Dismiss -command "catch {destroy $top}"
	set tmp_hoc [list [list summary $dsp_dscr] [list description "Pressing this button will create the DSP solid and dismiss this window" ]]
	hoc_register_data $top.applyB "Apply" $tmp_hoc
	set tmp_hoc [list [list summary $dsp_dscr] [list description "Pressing this button will create a unique name for the DSP solid" ]]
	hoc_register_data $top.autonameB "Autoname" $tmp_hoc
	set tmp_hoc [list [list summary $dsp_dscr] [list description "Pressing this button will dismiss this window without creating a DSP solid" ]]
	hoc_register_data $top.dismissB "Dismiss" $tmp_hoc

	grid $top.nameL -sticky "w" -row 0 -column 0
	grid $top.nameE -sticky "ew" -row 0 -column 1
	grid $top.fileL -sticky "w" -row 1 -column 0
	grid $top.fileE -sticky "ew" -row 1 -column 1
	grid $top.widthL -sticky "w" -row 2 -column 0
	grid $top.widthE -sticky "ew" -row 2 -column 1
	grid $top.heightL -sticky "w" -row 3 -column 0
	grid $top.heightE -sticky "ew" -row 3 -column 1
	grid $top.smoothC -sticky "e" -row 4 -column 0
	grid $top.smoothL -sticky "w" -row 4 -column 1
	grid $top.celldimL -sticky "w" -row 5 -column 0
	grid $top.celldimE -sticky "ew" -row 5 -column 1
	grid $top.cellelevL -sticky "w" -row 6 -column 0
	grid $top.cellelevE -sticky "ew" -row 6 -column 1
	grid $top.applyB $top.autonameB $top.dismissB -sticky "ew" -padx 8 -pady 8

    bind $top <Return> "make_dsp $id $top; break"

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "Enter Dsp Primitive Name"
}

proc init_solid_create { id type } {
    global mged_gui
    global tkPriv

    if {[opendb] == ""} {
	cad_dialog $tkPriv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.make_solid

    if [winfo exists $top] {
	raise $top
	return
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.nameF 
    label $top.nameL -text "Enter name for $type:" -anchor w
    entry $top.nameE -relief sunken -bd 2 -textvar mged_gui($id,solid_name)
    button $top.applyB -relief raised -text "Apply"\
	    -command "make_solid $id $top $type"
    button $top.autonameB -relief raised -text "Autoname"\
	    -command "solid_auto_name $id"
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"

    grid $top.nameL -sticky "w" -in $top.nameF
    grid $top.nameE -sticky "ew" -in $top.nameF
    grid $top.nameF - - - - -sticky "ew" -padx 8 -pady 8
    grid columnconfigure $top.nameF 0 -weight 1
    grid $top.applyB x $top.autonameB x $top.dismissB -sticky "ew" -padx 8 -pady 8
    grid columnconfigure $top 1 -weight 1
    grid columnconfigure $top 3 -weight 1

    bind $top <Return> "make_solid $id $top $type; break"

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "Enter Primitive Name"
}

proc solid_auto_name { id } {
    global mged_gui
    global mged_default
    global tkPriv

    set result [catch {_mged_make_name $mged_default(solid_name_fmt)} name]

    if {$result == 0} {
	set mged_gui($id,solid_name) $name
    } else {
	cad_dialog $tkPriv(cad_dialog) $mged_gui($id,screen)\
		"Failed to automatically create a solid name!"\
		$name\
		"" 0 OK
	return
    }
}

proc make_solid { id w type } {
    global mged_gui
    global tkPriv

    set result [catch {_mged_make $mged_gui($id,solid_name) $type} msg]

    if {$result == 0} {
	catch {_mged_sed $mged_gui($id,solid_name)}
	catch {destroy $w}
    } else {
	cad_dialog $tkPriv(cad_dialog) $mged_gui($id,screen)\
		"Bad solid name!"\
		$msg\
		"" 0 OK
	return
    }
}
