#                          P S . T C L
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
#			P S . T C L
#
#	Tool for producing PostScript files of MGED's current view.
#
#	Author - Robert G. Parker
#

check_externs "_mged_opendb _mged_ps"

proc init_psTool { id } {
    global mged_gui
    global ps_control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.do_ps

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists ps_control($id,file)] {
	regsub \.g$ [_mged_opendb] .ps default_file
	set ps_control($id,file) $default_file
    }

    if ![info exists ps_control($id,title)] {
	set ps_control($id,title) "No Title"
    }

    if ![info exists ps_control($id,creator)] {
	set ps_control($id,creator) "$id"
    }

    if ![info exists ps_control($id,font)] {
	set ps_control($id,font) "Courier"
    }

    if ![info exists ps_control($id,size)] {
	set ps_control($id,size) 4.5
    }

    if ![info exists ps_control($id,linewidth)] {
	set ps_control($id,linewidth) 1
    }

    if ![info exists ps_control($id,zclip)] {
	set ps_control($id,zclip) 1
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.elF
    frame $top.fileF -relief sunken -bd 2
    frame $top.titleF -relief sunken -bd 2
    frame $top.creatorF -relief sunken -bd 2
    frame $top.fontF -relief sunken -bd 2
    frame $top.sizeF -relief sunken -bd 2
    frame $top.linewidthF -relief sunken -bd 2
    frame $top.buttonF
    frame $top.buttonF2

    set tmp_hoc_data {{summary "Enter a filename specifying where
to put the generated postscript
description of the current view."} {see_also "ps"}}
    label $top.fileL -text "File Name" -anchor w
    hoc_register_data $top.fileL "File Name" $tmp_hoc_data
    entry $top.fileE -relief flat -width 10 -textvar ps_control($id,file)
    hoc_register_data $top.fileE "File Name" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter a title for the postscript file."} {see_also "ps"}}
    label $top.titleL -text "Title" -anchor w
    hoc_register_data $top.titleL "Title" $tmp_hoc_data
    entry $top.titleE -relief flat -width 10 -textvar ps_control($id,title)
    hoc_register_data $top.titleE "Title" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter the creator of the postscript file."} {see_also "ps"}}
    label $top.creatorL -text "Creator" -anchor w
    hoc_register_data $top.creatorL "Creator" $tmp_hoc_data
    entry $top.creatorE -relief flat -width 10 -textvar ps_control($id,creator)
    hoc_register_data $top.creatorE "Creator" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter the desired text font."} {see_also "ps"}}
    label $top.fontL -text "Font" -anchor w
    hoc_register_data $top.fontL "Font" $tmp_hoc_data
    entry $top.fontE -relief flat -width 17 -textvar ps_control($id,font)
    hoc_register_data $top.fontE "Font" $tmp_hoc_data
    menubutton $top.fontMB -relief raised -bd 2\
	    -menu $top.fontMB.fontM -indicatoron 1
    hoc_register_data $top.fontMB "Font"\
	    {{summary "Pops up a menu of known
postscript fonts."}}
    menu $top.fontMB.fontM -tearoff 0
    $top.fontMB.fontM add cascade -label "Courier"\
	    -menu $top.fontMB.fontM.courierM
    $top.fontMB.fontM add cascade -label "Helvetica"\
	    -menu $top.fontMB.fontM.helveticaM
    $top.fontMB.fontM add cascade -label "Times"\
	    -menu $top.fontMB.fontM.timesM

    menu $top.fontMB.fontM.courierM -tearoff 0
    $top.fontMB.fontM.courierM add command -label "Normal"\
	    -command "set ps_control($id,font) Courier"
    $top.fontMB.fontM.courierM add command -label "Oblique"\
	    -command "set ps_control($id,font) Courier-Oblique"
    $top.fontMB.fontM.courierM add command -label "Bold"\
	    -command "set ps_control($id,font) Courier-Bold"
    $top.fontMB.fontM.courierM add command -label "BoldOblique"\
	    -command "set ps_control($id,font) Courier-BoldOblique"

    menu $top.fontMB.fontM.helveticaM -tearoff 0
    $top.fontMB.fontM.helveticaM add command -label "Normal"\
	    -command "set ps_control($id,font) Helvetica"
    $top.fontMB.fontM.helveticaM add command -label "Oblique"\
	    -command "set ps_control($id,font) Helvetica-Oblique"
    $top.fontMB.fontM.helveticaM add command -label "Bold"\
	    -command "set ps_control($id,font) Helvetica-Bold"
    $top.fontMB.fontM.helveticaM add command -label "BoldOblique"\
	    -command "set ps_control($id,font) Helvetica-BoldOblique"

    menu $top.fontMB.fontM.timesM -tearoff 0
    $top.fontMB.fontM.timesM add command -label "Roman"\
	    -command "set ps_control($id,font) Times-Roman"
    $top.fontMB.fontM.timesM add command -label "Italic"\
	    -command "set ps_control($id,font) Times-Italic"
    $top.fontMB.fontM.timesM add command -label "Bold"\
	    -command "set ps_control($id,font) Times-Bold"
    $top.fontMB.fontM.timesM add command -label "BoldItalic"\
	    -command "set ps_control($id,font) Times-BoldItalic"

    set tmp_hoc_data {{summary "Enter the image size."} {see_also "ps"}}
    label $top.sizeL -text "Size" -anchor w
    hoc_register_data $top.sizeL "Size" $tmp_hoc_data
    entry $top.sizeE -relief flat -width 10 -textvar ps_control($id,size)
    hoc_register_data $top.sizeE "Size" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter the line width used when
drawing lines."} {see_also "ps"}}
    label $top.linewidthL -text "Line Width" -anchor w
    hoc_register_data $top.linewidthL "Line Width" $tmp_hoc_data
    entry $top.linewidthE -relief flat -width 10 -textvar ps_control($id,linewidth)
    hoc_register_data $top.linewidthE "Line Width" $tmp_hoc_data

    checkbutton $top.zclipCB -relief raised -text "Z Clipping"\
	    -variable ps_control($id,zclip)
    hoc_register_data $top.zclipCB "Z Clipping"\
	    {{summary "If checked, clip to the viewing cube."}
            {see_also "ps"}}

    button $top.okB -relief raised -text "OK"\
	    -command "do_ps $id; catch {destroy $top}"
    hoc_register_data $top.okB "Create"\
	    {{summary "Create the postscript file. The
postscript dialog is then dismissed."} {see_also "ps"}}
    button $top.createB -relief raised -text "Create"\
	    -command "do_ps $id"
    hoc_register_data $top.createB "Create"\
	    {{summary "Create the postscript file."} {see_also "ps"}}
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    {{summary "Dismiss the postscript tool."} {see_also "ps"}}

    grid $top.fileE -sticky "ew" -in $top.fileF
    grid $top.fileF  $top.fileL -sticky "ew" -in $top.elF -pady 4
    grid $top.titleE -sticky "ew" -in $top.titleF
    grid $top.titleF $top.titleL -sticky "ew" -in $top.elF -pady 4
    grid $top.creatorE -sticky "ew" -in $top.creatorF
    grid $top.creatorF $top.creatorL -sticky "ew" -in $top.elF -pady 4
    grid $top.fontE $top.fontMB -sticky "ew" -in $top.fontF
    grid $top.fontF $top.fontL -sticky "ew" -in $top.elF -pady 4
    grid $top.sizeE -sticky "ew" -in $top.sizeF
    grid $top.sizeF $top.sizeL -sticky "ew" -in $top.elF -pady 4
    grid $top.linewidthE -sticky "ew" -in $top.linewidthF
    grid $top.linewidthF $top.linewidthL -sticky "ew" -in $top.elF -pady 4
    grid columnconfigure $top.fileF 0 -weight 1
    grid columnconfigure $top.titleF 0 -weight 1
    grid columnconfigure $top.creatorF 0 -weight 1
    grid columnconfigure $top.fontF 0 -weight 1
    grid columnconfigure $top.sizeF 0 -weight 1
    grid columnconfigure $top.linewidthF 0 -weight 1
    grid columnconfigure $top.elF 0 -weight 1

    grid $top.zclipCB x -sticky "ew" -in $top.buttonF -ipadx 4 -ipady 4
    grid columnconfigure $top.buttonF 1 -weight 1

    grid $top.okB $top.createB x $top.dismissB -sticky "ew" -in $top.buttonF2
    grid columnconfigure $top.buttonF2 2 -weight 1 -minsize 40

    pack $top.elF $top.buttonF $top.buttonF2 -expand 1 -fill both -padx 8 -pady 8

    place_near_mouse $top
    wm title $top "PostScript Tool ($id)"
}

proc do_ps { id } {
    global mged_gui
    global ps_control
    global ::tk::Priv

    cmd_win set $id
    set ps_cmd "_mged_ps"

    if {$ps_control($id,file) != ""} {
	if {[file exists $ps_control($id,file)]} {
	    set result [cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen)\
		    "Overwrite $ps_control($id,file)?"\
		    "Overwrite $ps_control($id,file)?"\
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

    if {$ps_control($id,title) != ""} {
	append ps_cmd " -t \"$ps_control($id,title)\""
    }

    if {$ps_control($id,creator) != ""} {
	 append ps_cmd " -c \"$ps_control($id,creator)\""
    }

    if {$ps_control($id,font) != ""} {
	 append ps_cmd " -f $ps_control($id,font)"
    }

    if {$ps_control($id,size) != ""} {
	 append ps_cmd " -s $ps_control($id,size)"
    }

    if {$ps_control($id,linewidth) != ""} {
	 append ps_cmd " -l $ps_control($id,linewidth)"
    }

    if {$ps_control($id,zclip) != 0} {
	append ps_cmd " -z"
    }

    append ps_cmd " $ps_control($id,file)"
    catch {eval $ps_cmd}
}
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
