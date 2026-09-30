#                        P L O T . T C L
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
#	Widget for producing Unix Plot files of MGED's current view.
#

check_externs "::brlcad::mged"

namespace eval ::brlcad::mged::plot {

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

    set top .$id.do_plot

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists control($id,file_or_filter)] {
	set control($id,file_or_filter) file
    }

    if ![info exists control($id,file)] {
	regsub \.g$ [::brlcad::mged opendb] .plot3 default_file
	set control($id,file) $default_file
    }

    if ![info exists control($id,filter)] {
	set control($id,filter) ""
    }

    if ![info exists control($id,zclip)] {
	set control($id,zclip) 1
    }

    if ![info exists control($id,2d)] {
	set control($id,2d) 0
    }

    if ![info exists control($id,float)] {
	set control($id,float) 0
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF
    frame $top.gridF2
    frame $top.gridF3

    if {$control($id,file_or_filter) eq "file"} {
	set file_state normal
	set filter_state disabled
    } else {
	set filter_state normal
	set file_state disabled
    }

    entry $top.fileE -width 12 \
	-textvariable ::brlcad::mged::plot::control($id,file)\
	-state $file_state
    hoc_register_data $top.fileE "File Name"\
	{{summary "Enter a filename specifying where
to put the UNIX-plot of the displayed
geometry."} {see_also pl}}
    radiobutton $top.fileRB -text "File Name" -anchor w\
	    -value file \
	    -variable ::brlcad::mged::plot::control($id,file_or_filter)\
	    -command [list ::brlcad::mged::plot::select_file $id]
    hoc_register_data $top.fileRB "File"\
	    {{summary "Activate the filename entry."}}

    entry $top.filterE -width 12 \
	-textvariable ::brlcad::mged::plot::control($id,filter)\
	    -state $filter_state
    hoc_register_data $top.filterE "Filter"\
	    {{summary "If a filter is specified, the
output is sent there."} {see_also pl}}
    radiobutton $top.filterRB -text "Filter" -anchor w\
	    -value filter \
	    -variable ::brlcad::mged::plot::control($id,file_or_filter)\
	    -command [list ::brlcad::mged::plot::select_filter $id]
    hoc_register_data $top.filterRB "Filter"\
	    {{summary "Activate the filter entry."}}

    checkbutton $top.zclipCB -relief raised -text "Z Clipping"\
	    -variable ::brlcad::mged::plot::control($id,zclip)
    hoc_register_data $top.zclipCB "Z Clipping"\
	    {{summary "If checked, the plot will be
clipped to the viewing cube."} {see_also pl}}
    checkbutton $top.twoDCB -relief raised -text "2D"\
	    -variable ::brlcad::mged::plot::control($id,2d)
    hoc_register_data $top.twoDCB "2D"\
	    {{summary "If checked, the plot will be
two-dimensional instead of three-dimensional."} {see_also pl}}
    checkbutton $top.floatCB -relief raised -text "Float"\
	    -variable ::brlcad::mged::plot::control($id,float)
    hoc_register_data $top.floatCB "Float"\
	    {{summary "If checked, the plot file will use floating
point numbers instead of integers."}}

    button $top.okB -relief raised -text "OK"\
	    -command [list ::brlcad::mged::plot::ok $id $top]
    hoc_register_data $top.okB "Create"\
	    {{summary "Create a plot file of the current view.
The plot dialog is then dismissed."} {see_also pl}}
    button $top.createB -relief raised -text "Create"\
	    -command [list ::brlcad::mged::plot::create $id]
    hoc_register_data $top.createB "Create"\
	    {{summary "Create a plot file of the current view."} {see_also pl}}
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command [list catch [list destroy $top]]
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

proc ok {id top} {
    create $id
    catch {destroy $top}
}

proc create { id } {
    global mged_gui
    variable control
    global ::tk::Priv

    cmd_win set $id
    set pl_cmd [list ::brlcad::mged plot]

    if {$control($id,zclip)} {
	lappend pl_cmd -zclip
    }

    if {$control($id,2d)} {
	lappend pl_cmd -2d
    }

    if {$control($id,float)} {
	lappend pl_cmd -float
    }

    if {$control($id,file_or_filter) eq "file"} {
	if {$control($id,file) ne ""} {
	    if [file exists $control($id,file)] {
		set result [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
				"Overwrite $control($id,file)?"\
				"Overwrite $control($id,file)?"\
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

	lappend pl_cmd $control($id,file)
    } else {
	if {$control($id,filter) eq ""} {
	    cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		"No filter specified!"\
		"No filter specified!"\
		"" 0 OK

	    return
	}

	lappend pl_cmd "|$control($id,filter)"
    }

    catch {{*}$pl_cmd}
}

proc select_file { id } {
    set top .$id.do_plot

    $top.fileE configure -state normal
    $top.filterE configure -state disabled

    focus $top.fileE
}

proc select_filter { id } {
    set top .$id.do_plot

    $top.filterE configure -state normal
    $top.fileE configure -state disabled

    focus $top.filterE
}

}

proc init_plotTool {id} {
    tailcall ::brlcad::mged::plot::init $id
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
