#                          P S . T C L
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
#	Tool for producing PostScript files of MGED's current view.
#

check_externs "::brlcad::mged"

namespace eval ::brlcad::mged::postscript {

namespace export init

variable control
variable font_families {
    {courier Courier {
	{Normal Courier}
	{Oblique Courier-Oblique}
	{Bold Courier-Bold}
	{BoldOblique Courier-BoldOblique}
    }}
    {helvetica Helvetica {
	{Normal Helvetica}
	{Oblique Helvetica-Oblique}
	{Bold Helvetica-Bold}
	{BoldOblique Helvetica-BoldOblique}
    }}
    {times Times {
	{Roman Times-Roman}
	{Italic Times-Italic}
	{Bold Times-Bold}
	{BoldItalic Times-BoldItalic}
    }}
}

proc populate_font_menu {menu id} {
    variable font_families

    foreach family $font_families {
	lassign $family name label variants
	set family_menu [format "%s.%sM" $menu $name]
	$menu add cascade -label $label -menu $family_menu
	menu $family_menu -tearoff 0

	foreach variant $variants {
	    lassign $variant variant_label font
	    $family_menu add command -label $variant_label \
		-command [list set \
		    ::brlcad::mged::postscript::control($id,font) $font]
	}
    }
}

proc init { id } {
    global mged_gui
    variable control
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

    if ![info exists control($id,file)] {
	regsub \.g$ [::brlcad::mged opendb] .ps default_file
	set control($id,file) $default_file
    }

    if ![info exists control($id,title)] {
	set control($id,title) "No Title"
    }

    if ![info exists control($id,creator)] {
	set control($id,creator) "$id"
    }

    if ![info exists control($id,font)] {
	set control($id,font) "Courier"
    }

    if ![info exists control($id,size)] {
	set control($id,size) 4.5
    }

    if ![info exists control($id,linewidth)] {
	set control($id,linewidth) 1
    }

    if ![info exists control($id,zclip)] {
	set control($id,zclip) 1
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
description of the current view."} {see_also "postscript"}}
    label $top.fileL -text "File Name" -anchor w
    hoc_register_data $top.fileL "File Name" $tmp_hoc_data
    entry $top.fileE -relief flat -width 10 \
	-textvariable ::brlcad::mged::postscript::control($id,file)
    hoc_register_data $top.fileE "File Name" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter a title for the postscript file."} {see_also "postscript"}}
    label $top.titleL -text "Title" -anchor w
    hoc_register_data $top.titleL "Title" $tmp_hoc_data
    entry $top.titleE -relief flat -width 10 \
	-textvariable ::brlcad::mged::postscript::control($id,title)
    hoc_register_data $top.titleE "Title" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter the creator of the postscript file."} {see_also "postscript"}}
    label $top.creatorL -text "Creator" -anchor w
    hoc_register_data $top.creatorL "Creator" $tmp_hoc_data
    entry $top.creatorE -relief flat -width 10 \
	-textvariable ::brlcad::mged::postscript::control($id,creator)
    hoc_register_data $top.creatorE "Creator" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter the desired text font."} {see_also "postscript"}}
    label $top.fontL -text "Font" -anchor w
    hoc_register_data $top.fontL "Font" $tmp_hoc_data
    entry $top.fontE -relief flat -width 17 \
	-textvariable ::brlcad::mged::postscript::control($id,font)
    hoc_register_data $top.fontE "Font" $tmp_hoc_data
    menubutton $top.fontMB -relief raised -bd 2\
	    -menu $top.fontMB.fontM -indicatoron 1
    hoc_register_data $top.fontMB "Font"\
	    {{summary "Pops up a menu of known
postscript fonts."}}
    menu $top.fontMB.fontM -tearoff 0
    populate_font_menu $top.fontMB.fontM $id

    set tmp_hoc_data {{summary "Enter the image size."} {see_also "postscript"}}
    label $top.sizeL -text "Size" -anchor w
    hoc_register_data $top.sizeL "Size" $tmp_hoc_data
    entry $top.sizeE -relief flat -width 10 \
	-textvariable ::brlcad::mged::postscript::control($id,size)
    hoc_register_data $top.sizeE "Size" $tmp_hoc_data

    set tmp_hoc_data {{summary "Enter the line width used when
drawing lines."} {see_also "postscript"}}
    label $top.linewidthL -text "Line Width" -anchor w
    hoc_register_data $top.linewidthL "Line Width" $tmp_hoc_data
    entry $top.linewidthE -relief flat -width 10 \
	-textvariable ::brlcad::mged::postscript::control($id,linewidth)
    hoc_register_data $top.linewidthE "Line Width" $tmp_hoc_data

    checkbutton $top.zclipCB -relief raised -text "Z Clipping"\
	    -variable ::brlcad::mged::postscript::control($id,zclip)
    hoc_register_data $top.zclipCB "Z Clipping"\
	    {{summary "If checked, clip to the viewing cube."}
	    {see_also "postscript"}}

    button $top.okB -relief raised -text "OK"\
	    -command [list ::brlcad::mged::postscript::ok $id $top]
    hoc_register_data $top.okB "Create"\
	    {{summary "Create the postscript file. The
postscript dialog is then dismissed."} {see_also "postscript"}}
    button $top.createB -relief raised -text "Create"\
	    -command [list ::brlcad::mged::postscript::create $id]
    hoc_register_data $top.createB "Create"\
	    {{summary "Create the postscript file."} {see_also "postscript"}}
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command [list catch [list destroy $top]]
    hoc_register_data $top.dismissB "Dismiss"\
	    {{summary "Dismiss the postscript tool."} {see_also "postscript"}}

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

proc ok {id top} {
    create $id
    catch {destroy $top}
}

proc create { id } {
    global mged_gui
    variable control
    global ::tk::Priv

    cmd_win set $id
    set ps_cmd [list ::brlcad::mged postscript]

    if {$control($id,file) ne ""} {
	if {[file exists $control($id,file)]} {
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

    if {$control($id,title) ne ""} {
	lappend ps_cmd -t $control($id,title)
    }

    if {$control($id,creator) ne ""} {
	lappend ps_cmd -c $control($id,creator)
    }

    if {$control($id,font) ne ""} {
	lappend ps_cmd -f $control($id,font)
    }

    if {$control($id,size) ne ""} {
	lappend ps_cmd -s $control($id,size)
    }

    if {$control($id,linewidth) ne ""} {
	lappend ps_cmd -l $control($id,linewidth)
    }

    if {$control($id,zclip)} {
	lappend ps_cmd -z
    }

    lappend ps_cmd $control($id,file)
    catch {{*}$ps_cmd}
}

}

proc init_psTool {id} {
    tailcall ::brlcad::mged::postscript::init $id
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
