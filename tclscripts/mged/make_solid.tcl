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
    set mged_default(solid_name_fmt) "default_name"
}

proc init_solid_create { id type } {
    global mged_gui

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
    wm title $top "Enter Solid Name"
}

proc solid_auto_name { id } {
    global mged_gui
    global mged_default

    set result [catch {_mged_make_name $mged_default(solid_name_fmt)} name]

    if {$result == 0} {
	set mged_gui($id,solid_name) $name
    } else {
	cad_dialog .$id.solidDialog $mged_gui($id,screen)\
		"Failed to automatically create a solid name!"\
		$name\
		"" 0 OK
	return
    }
}

proc make_solid { id w type } {
    global mged_gui

    set result [catch {_mged_make $mged_gui($id,solid_name) $type} msg]

    if {$result == 0} {
	catch {_mged_sed $mged_gui($id,solid_name)}
	catch {destroy $w}
    } else {
	cad_dialog .$id.solidDialog $mged_gui($id,screen)\
		"Bad solid name!"\
		$msg\
		"" 0 OK
	return
    }
}
