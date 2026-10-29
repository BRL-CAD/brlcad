#                     E X T R A C T . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
#	Tool for extracting objects out of the current MGED database.
#

check_externs "::brlcad::ged ::brlcad::mged"

namespace eval ::brlcad::mged::extract {

namespace export init

variable control

proc init { id } {
    global mged_gui
    variable control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
	    "No database has been opened!" info 0 OK
	return
    }

    set top .$id.do_extract

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists control($id,file)] {
	regsub \.g$ [::brlcad::mged opendb] _keep.g default_file
	set control($id,file) $default_file
    }

    set control($id,objects) [::brlcad::mged who]

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF
    frame $top.gridF2

    set tmp_hoc_data {{summary "Enter a filename specifying where to put
the extracted objects."} {see_also keep}}
    label $top.fileL -text "File Name" -anchor w
    hoc_register_data $top.fileL "File Name" $tmp_hoc_data
    entry $top.fileE -width 24 \
	-textvariable ::brlcad::mged::extract::control($id,file)
    hoc_register_data $top.fileE "File Name" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter the objects to extract."}
	    {see_also keep}}
    label $top.objectsL -text "Objects" -anchor w
    hoc_register_data $top.objectsL "Objects" $tmp_hoc_data
    entry $top.objectsE -width 24 \
	-textvariable ::brlcad::mged::extract::control($id,objects)
    hoc_register_data $top.objectsE "Objects" $tmp_hoc_data

    button $top.okB -relief raised -text "OK"\
	    -command [list ::brlcad::mged::extract::ok $id $top]
    hoc_register_data $top.okB "Extract" {{summary "
Extract the listed objects from the current database
and put them into the specified file. The extract dialog
is then dismissed. Note - these objects are not removed
from the current database."} {see_also keep}}
    button $top.extractB -relief raised -text "Extract"\
	    -command [list ::brlcad::mged::extract::create $id]
    hoc_register_data $top.extractB "Extract" {{summary "
Extract the listed objects from the current database
and put them into the specified file. Note - these
objects are not removed from the current database."} {see_also keep}}
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command [list catch [list destroy $top]]
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

proc ok {id top} {
    create $id
    catch {destroy $top}
}

proc create { id } {
    global mged_gui
    variable control
    global ::tk::Priv

    cmd_win set $id
    set ex_cmd [list ::brlcad::mged keep]

    if {$control($id,file) ne ""} {
	if [file exists $control($id,file)] {
	    set result [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
			    "Append to $control($id,file)?"\
			    "Append to $control($id,file)?"\
			    "" 0 OK Cancel]

	    if {$result} {
		return
	    }
	}
    } else {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
	    "No file name specified!"\
	    "No file name specified!"\
	    "" 0 OK

	return
    }

    lappend ex_cmd $control($id,file)

    if {$control($id,objects) ne ""} {
	set globbed_objects [::brlcad::ged glob $control($id,objects)]
	lappend ex_cmd {*}$globbed_objects
    } else {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
	    "No objects specified!"\
	    "No objects specified!"\
	    "" 0 OK

	return
    }

    set result [catch {{*}$ex_cmd}]
    return $result
}

}

proc init_extractTool {id} {
    tailcall ::brlcad::mged::extract::init $id
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
