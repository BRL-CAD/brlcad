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

if ![info exists mged_solid_name_fmt] {
    set mged_solid_name_fmt "default_name"
}

proc init_solid_create { id type } {
    global player_screen

    set top .$id.make_solid

    if [winfo exists $top] {
	raise $top
	return
    }

    toplevel $top -screen $player_screen($id)

    frame $top.nameF 
    label $top.nameL -text "Enter name for $type:" -anchor w
    entry $top.nameE -relief sunken -bd 2 -textvar mged_solid_name($id)
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

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm geometry $top +$x+$y
    wm title $top "Enter Solid Name"
}

proc solid_auto_name { id } {
    global mged_solid_name
    global mged_solid_name_fmt

    set result [catch {_mged_make_name $mged_solid_name_fmt} name]

    if {$result == 0} {
	set mged_solid_name($id) $name
    } else {
	cad_dialog .$id.solidDialog $player_screen($id)\
		"Failed to automatically create a solid name!"\
		$name\
		"" 0 OK
	return
    }
}

proc make_solid { id w type } {
    global player_screen
    global mged_solid_name

    set result [catch {_mged_make $mged_solid_name($id) $type} msg]

    if {$result == 0} {
	catch {_mged_sed $mged_solid_name($id)}
	catch {destroy $w}
    } else {
	cad_dialog .$id.solidDialog $player_screen($id)\
		"Bad solid name!"\
		$msg\
		"" 0 OK
	return
    }
}
