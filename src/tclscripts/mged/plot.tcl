#                        P L O T . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
#			P L O T . T C L
#
#	Widget for producing Unix Plot files of MGED's current view.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_pl"

proc init_plotTool { id } {
    global mged_gui
    global pl_control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.do_plot

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists pl_control($id,file_or_filter)] {
	set pl_control($id,file_or_filter) file
    }

    if ![info exists pl_control($id,file)] {
	regsub \.g$ [_mged_opendb] .plot default_file
	set pl_control($id,file) $default_file
    }

    if ![info exists pl_control($id,filter)] {
	set pl_control($id,filter) ""
    }

    if ![info exists pl_control($id,zclip)] {
	set pl_control($id,zclip) 1
    }

    if ![info exists pl_control($id,2d)] {
	set pl_control($id,2d) 0
    }

    if ![info exists pl_control($id,float)] {
	set pl_control($id,float) 0
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF
    frame $top.gridF2
    frame $top.gridF3

    if {$pl_control($id,file_or_filter) == "file"} {
	set file_state normal
	set filter_state disabled
    } else {
	set filter_state normal
	set file_state disabled
    }

    entry $top.fileE -width 12 -textvar pl_control($id,file)\
	    -state $file_state
    hoc_register_data $top.fileE "File Name"\
	    {{summary "Enter a filename specifying where
to put the UNIX-plot of the displayed
geometry."} {see_also pl}}
    radiobutton $top.fileRB -text "File Name" -anchor w\
	    -value file -variable pl_control($id,file_or_filter)\
	    -command "pl_set_file_state $id"
    hoc_register_data $top.fileRB "File"\
	    {{summary "Activate the filename entry."}}

    entry $top.filterE -width 12 -textvar pl_control($id,filter)\
	    -state $filter_state
    hoc_register_data $top.filterE "Filter"\
	    {{summary "If a filter is specified, the
output is sent there."} {see_also pl}}
    radiobutton $top.filterRB -text "Filter" -anchor w\
	    -value filter -variable pl_control($id,file_or_filter)\
	    -command "pl_set_filter_state $id"
    hoc_register_data $top.filterRB "Filter"\
	    {{summary "Activate the filter entry."}}

    checkbutton $top.zclipCB -relief raised -text "Z Clipping"\
	    -variable pl_control($id,zclip)
    hoc_register_data $top.zclipCB "Z Clipping"\
	    {{summary "If checked, the plot will be
clipped to the viewing cube."} {see_also pl}}
    checkbutton $top.twoDCB -relief raised -text "2D"\
	    -variable pl_control($id,2d)
    hoc_register_data $top.twoDCB "2D"\
	    {{summary "If checked, the plot will be
two-dimensional instead of three-dimensional."} {see_also pl}}
    checkbutton $top.floatCB -relief raised -text "Float"\
	    -variable pl_control($id,float)
    hoc_register_data $top.floatCB "Float"\
	    {{summary "If checked, the plot file will use floating
point numbers instead of integers."}}

    button $top.okB -relief raised -text "OK"\
	    -command "do_plot $id; catch {destroy $top}"
    hoc_register_data $top.okB "Create"\
	    {{summary "Create a plot file of the current view.
The plot dialog is then dismissed."} {see_also pl}}
    button $top.createB -relief raised -text "Create"\
	    -command "do_plot $id"
    hoc_register_data $top.createB "Create"\
	    {{summary "Create a plot file of the current view."} {see_also pl}}
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    {{summary "Dismiss the plot tool."}}

    grid $top.fileE $top.fileRB -sticky "ew" -in $top.gridF -pady 4
    grid $top.filterE $top.filterRB -sticky "ew" -in $top.gridF -pady 4
    grid columnconfigure $top.gridF 0 -weight 1

    grid $top.zclipCB x $top.twoDCB x $top.floatCB -sticky "ew"\
	    -in $top.gridF2 -padx 4 -pady 4
    grid columnconfigure $top.gridF2 1 -weight 1
    grid columnconfigure $top.gridF2 3 -weight 1

    grid $top.okB $top.createB x $top.dismissB -sticky "ew" -in $top.gridF3 -pady 4
    grid columnconfigure $top.gridF3 2 -weight 1

    pack $top.gridF $top.gridF2 $top.gridF3 -side top -expand 1 -fill both\
	    -padx 8 -pady 8

    place_near_mouse $top
    wm title $top "Unix Plot Tool ($id)"
}

proc do_plot { id } {
    global mged_gui
    global pl_control
    global ::tk::Priv

    cmd_win set $id
    set pl_cmd "_mged_pl"

    if {$pl_control($id,zclip)} {
	append pl_cmd " -zclip"
    }

    if {$pl_control($id,2d)} {
	append pl_cmd " -2d"
    }

    if {$pl_control($id,float)} {
	append pl_cmd " -float"
    }

    if {$pl_control($id,file_or_filter) == "file"} {
	if {$pl_control($id,file) != ""} {
	    if [file exists $pl_control($id,file)] {
		set result [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
			"Overwrite $pl_control($id,file)?"\
			"Overwrite $pl_control($id,file)?"\
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

	append pl_cmd " $pl_control($id,file)"
    } else {
	if {$pl_control($id,filter) == ""} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		    "No filter specified!"\
		    "No filter specified!"\
		    "" 0 OK

	    return
	}

	append pl_cmd " |$pl_control($id,filter)"
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
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
