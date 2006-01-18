#                   R T _ S C R I P T . T C L
# BRL-CAD
#
# Copyright (c) 2004-2006 United States Government as represented by
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
#			R T _ S C R I P T . T C L
#
#	Widget for producing RT script files of MGED's current view.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_saveview"

proc init_rtScriptTool { id } {
    global mged_gui
    global rts_control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.do_rtScript

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists rts_control($id,file)] {
	regsub \.g$ [_mged_opendb] .sh default_file
	set rts_control($id,file) $default_file
    }

    if ![info exists rts_control($id,args)] {
	set rts_control($id,args) ""
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF
    frame $top.gridF2

    set tmp_hoc_data {{summary "Enter a filename specifying where
to put the RT script."} {see_also "saveview, rt"}}
    label $top.fileL -text "File Name" -anchor w
    hoc_register_data $top.fileL "File Name" $tmp_hoc_data
    entry $top.fileE -width 12 -textvar rts_control($id,file)
    hoc_register_data $top.fileE "File Name" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter other rt options."}
                      {see_also "saveview, rt"}}
    label $top.argsL -text "Other args" -anchor w
    hoc_register_data $top.argsL "Other args" $tmp_hoc_data
    entry $top.argsE -width 12 -textvar rts_control($id,args)
    hoc_register_data $top.argsE "Other args" $tmp_hoc_data

    button $top.okB -relief raised -text "OK"\
	    -command "do_rtScript $id; catch {destroy $top}"
    hoc_register_data $top.okB "Create"\
	    {{summary "Create the RT script. The rt_script
dialog is then dismissed."}}
    button $top.createB -relief raised -text "Create"\
	    -command "do_rtScript $id"
    hoc_register_data $top.createB "Create"\
	    {{summary "Create the RT script."}}
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    {{summary "Dismiss the entry dialog without creating
the RT script."}}

    grid $top.fileE $top.fileL -sticky "ew" -in $top.gridF -pady 4
    grid $top.argsE $top.argsL -sticky "ew" -in $top.gridF -pady 4
    grid columnconfigure $top.gridF 0 -weight 1

    grid $top.okB $top.createB x $top.dismissB -in $top.gridF2
    grid columnconfigure $top.gridF2 2 -weight 1

    pack $top.gridF $top.gridF2 -side top -expand 1 -fill both\
	    -padx 8 -pady 8

    place_near_mouse $top
    wm title $top "RT Script Tool"
}

proc do_rtScript { id } {
    global mged_gui
    global rts_control
    global ::tk::Priv

    cmd_win set $id
    set rts_cmd "_mged_saveview"

    if {$rts_control($id,file) != ""} {
	if [file exists $rts_control($id,file)] {
	    set result [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		    "Append $rts_control($id,file)?"\
		    "Append $rts_control($id,file)?"\
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

    append rts_cmd " $rts_control($id,file)"

    if {$rts_control($id,args) != ""} {
	append rts_cmd " $rts_control($id,args)"
    }

    catch {eval $rts_cmd}
}
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
