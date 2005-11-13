#                       M U V E S . T C L
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
##
#				M U V E S . T C L
#
# Authors -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Description - routines for looking at MUVES final results files.
#

if ![info exists mged_default(tearoff_menus)] {
    set mged_default(tearoff_menus) 0
}

if ![info exists tk_version] {
    loadtk
}

proc muves_gui_init { id dpy }  {
    global muves_states
    global muves_active
    global muves_color_ramp
    global mged_default

    set top .$id\_muves

    if [winfo exists $top] {
	raise $top
	return
    }

    if ![info exists muves_states] {
	return -code error "muves_gui_init: muves_states has not been initialized\n\
see muves_states_init or muves_states_initp"
    }

    if ![info exists muves_color_ramp($id,num)] {
	set ramp -1

	incr ramp
	set muves_color_ramp($id,$ramp,label) "Spectrum 0"
	set muves_color_ramp($id,$ramp,range) {{0.0 0.0} {0.0 0.1} {0.1 0.2} \
		{0.2 0.3} {0.3 0.4} {0.4 0.5} {0.5 0.6} {0.6 0.7} {0.7 0.8} \
		{0.8 0.9} {0.9 1.0}}
	set muves_color_ramp($id,$ramp,rgb) {{255 255 255} {100 100 140} {0 0 255} \
		{0 120 255} {100 200 140} {0 150 0} {0 225 0} {255 255 0} \
		{255 160 0} {255 100 100} {255 0 0}}
	set muves_color_ramp($id,$ramp,num) [llength $muves_color_ramp($id,$ramp,range)]

	incr ramp
	set muves_color_ramp($id,$ramp,label) "Spectrum 1"
	set muves_color_ramp($id,$ramp,range) {{0.0 0.0} {0.0 1.6} {1.6 3.2} \
		{3.2 4.8} {4.8 6.4} {6.4 8.0} {8.0 9.6} {9.6 11.2} {11.2 12.8} \
		{12.8 14.4} {14.4 16.0}}
	set muves_color_ramp($id,$ramp,rgb) {{255 255 255} {100 100 140} {0 0 255} \
		{0 120 255} {100 200 140} {0 150 0} {0 225 0} {255 255 0} \
		{255 160 0} {255 100 100} {255 0 0}}
	set muves_color_ramp($id,$ramp,num) [llength $muves_color_ramp($id,$ramp,range)]

	incr ramp
	set muves_color_ramp($id,$ramp,label) "Red 0"
	set muves_color_ramp($id,$ramp,range) {{0.0 0.0} {0.0 0.1} {0.1 0.2} \
		{0.2 0.3} {0.3 0.4} {0.4 0.5} {0.5 0.6} {0.6 0.7} {0.7 0.8} \
		{0.8 0.9} {0.9 1.0}}
	set muves_color_ramp($id,$ramp,rgb) {{255 255 255} {255 229 229} {255 204 204} \
		{255 178 178} {255 153 153} {255 127 127} {255 102 102} {255 77 77} \
		{255 51 51} {255 26 26} {255 0 0}}
	set muves_color_ramp($id,$ramp,num) [llength $muves_color_ramp($id,$ramp,range)]

	incr ramp
	set muves_color_ramp($id,$ramp,label) "Red 1"
	set muves_color_ramp($id,$ramp,range) {{0.0 0.0} {0.0 0.0001} {0.0001 0.0002} \
		{0.0002 0.0003} {0.0003 0.0004} {0.0004 0.0005} {0.0005 0.0006} \
		{0.0006 0.0007} {0.0007 0.0008} {0.0008 0.0009} {0.0009 0.1} \
	        {0.1 0.2} {0.2 0.3} {0.3 0.4} {0.4 0.5} {0.5 0.6} {0.6 0.7} {0.7 0.8} \
	        {0.8 0.9} {0.9 1.0}}
	set muves_color_ramp($id,$ramp,rgb) {{255 255 255} {255 230 230} {255 220 220} \
		{255 210 210} {255 190 190} {255 170 170} {255 150 150} {255 130 130} \
		{255 110 110} {255 90 90} {255 80 80} {255 70 70} {255 60 60} {255 50 50} {255 50 40} \
	        {255 30 30} {255 20 20} {255 15 15} {255 10 10} {255 0 0}}
	set muves_color_ramp($id,$ramp,num) [llength $muves_color_ramp($id,$ramp,range)]

	incr ramp
	set muves_color_ramp($id,num) $ramp
	set muves_color_ramp($id,ramp) 0
    }

    if ![info exists muves_active($id,sindex)] {
	set muves_active($id,sindex) 0
	set muves_active($id,clabel) ""
	set muves_active($id,pvt) mean
	set muves_active($id,op1) "<"
	set muves_active($id,op2) "<="
	set muves_active($id,cval1) 0.0
	set muves_active($id,cval2) 1.0
	set muves_active($id,pass_color) "255 0 0"
	set muves_active($id,fail_color) "0 255 0"
	set muves_active($id,mode) "pass_fail"

    }

    toplevel $top -screen $dpy -menu $top.menubar
    frame $top.pass_failF
    frame $top.color_rampF

    set row -1

    incr row
    frame $top.gridF
    label $top.stateL -text "State Vectors"
    listbox $top.stateLB -width 50 -height 8 -selectmode single \
	    -yscrollcommand "$top.stateSB set"
    scrollbar $top.stateSB -command "$top.stateLB yview"
    foreach state $muves_states(names) {
	$top.stateLB insert end $state
    }
    grid $top.stateL - -sticky "nsew" -in $top.gridF -pady 4
    grid $top.stateLB $top.stateSB -sticky "nsew" -in $top.gridF
    grid columnconfigure $top.gridF 0 -weight 1
    grid rowconfigure $top.gridF 1 -weight 1
    grid $top.gridF -sticky "nsew" -padx 4 -pady 4
    grid rowconfigure $top $row -weight 1

    incr row
    menu $top.menubar -tearoff $mged_default(tearoff_menus)
    $top.menubar add cascade -label "Modes" -underline 0 -menu $top.menubar.mode

    menu $top.menubar.mode -title "Modes" -tearoff $mged_default(tearoff_menus)
    $top.menubar.mode add radiobutton -value "pass_fail" -variable muves_active($id,mode) \
	    -label "Pass/Fail" -underline 0\
	    -command "muves_toggle_mode $id $top $row"
    $top.menubar.mode add radiobutton -value "color_ramp" -variable muves_active($id,mode) \
	    -label "Color Ramp" -underline 0\
	    -command "muves_toggle_mode $id $top $row"

    muves_create_modes $id $top $top.pass_failF pass_fail
    muves_create_modes $id $top $top.color_rampF color_ramp
    muves_toggle_mode $id $top $row

    grid columnconfigure $top 0 -weight 1

    bind $top.stateLB <ButtonPress-1> "set muves_active($id,sindex) \[%W nearest %y\]; \
	    muves_update_components $id $top"

    place_near_mouse $top

    # force behavior change to favor user specified size
    update
    wm geometry $top [wm geometry $top]
}

proc muves_create_modes { id top parent mode } {
    global muves_active
    global muves_color_ramp
    global mged_default

    set n -1

    switch $mode {
	pass_fail {
	    incr n
	    frame $parent.gridF$n -relief groove -bd 2
	    label $parent.constraintL -text "Constraints"
	    frame $parent.vtypeF -relief flat -bd 2
	    label $parent.vtypeL -text "Val Type: "
	    radiobutton $parent.minRB -text "min" -anchor w \
		    -value min -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    radiobutton $parent.meanRB -text "mean" -anchor w \
		    -value mean -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    radiobutton $parent.maxRB -text "max" -anchor w \
		    -value max -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    entry $parent.val1E -relief sunken -bd 2 -width 12 -textvar muves_active($id,cval1)
	    menubutton $parent.op1MB -relief flat -bd 2 -textvar muves_active($id,op1) -menu $parent.op1MB.m
	    menu $parent.op1MB.m -tearoff 0
	    $parent.op1MB.m add command -label "<" \
		    -command "set muves_active($id,op1) <; muves_update_components $id $top"
	    $parent.op1MB.m add command -label "<=" \
		    -command "set muves_active($id,op1) <=; muves_update_components $id $top"
	    $parent.op1MB.m add command -label ">" \
		    -command "set muves_active($id,op1) >; muves_update_components $id $top"
	    $parent.op1MB.m add command -label ">=" \
		    -command "set muves_active($id,op1) >=; muves_update_components $id $top"
	    label $parent.valL -text "val"
	    menubutton $parent.op2MB -relief flat -bd 2 -textvar muves_active($id,op2) -menu $parent.op2MB.m
	    menu $parent.op2MB.m -tearoff 0
	    $parent.op2MB.m add command -label "<" \
		    -command "set muves_active($id,op2) <; muves_update_components $id $top"
	    $parent.op2MB.m add command -label "<=" \
		    -command "set muves_active($id,op2) <=; muves_update_components $id $top"
	    $parent.op2MB.m add command -label ">" \
		    -command "set muves_active($id,op2) >; muves_update_components $id $top"
	    $parent.op2MB.m add command -label ">=" \
		    -command "set muves_active($id,op2) >=; muves_update_components $id $top"
	    entry $parent.val2E -relief sunken -bd 2 -width 12 -textvar muves_active($id,cval2)
	    grid $parent.constraintL - - - - -sticky "nsew" -in $parent.gridF$n -pady 4
	    grid $parent.vtypeL $parent.minRB $parent.meanRB $parent.maxRB -sticky "w" -in $parent.vtypeF
	    grid $parent.vtypeF - - - - -sticky w -in $parent.gridF$n -padx 4 -pady 4
	    grid $parent.val1E $parent.op1MB $parent.valL $parent.op2MB $parent.val2E \
		    -sticky "nsew" -in $parent.gridF$n -padx 4 -pady 4
	    grid columnconfigure $parent.gridF$n 0 -weight 1
	    grid columnconfigure $parent.gridF$n 4 -weight 1
	    grid $parent.gridF$n -sticky "nsew" -padx 4 -pady 4

	    incr n
	    frame $parent.gridF$n -relief groove -bd 2
	    label $parent.componentL -textvar muves_active($id,clabel)
	    # Widgets for components that "passed"
	    frame $parent.pcomponentF
	    label $parent.pcomponentL -text "Pass"
	    listbox $parent.pcomponentLB -selectmode single -yscrollcommand "$parent.pcomponentSB set"
	    scrollbar $parent.pcomponentSB -command "$parent.pcomponentLB yview"
	    # Widgets for components that "failed"
	    frame $parent.fcomponentF
	    label $parent.fcomponentL -text "Fail"
	    listbox $parent.fcomponentLB -selectmode single -yscrollcommand "$parent.fcomponentSB set"
	    scrollbar $parent.fcomponentSB -command "$parent.fcomponentLB yview"
	    grid $parent.componentL - -sticky "nsew" -in $parent.gridF$n
	    grid $parent.pcomponentL - -sticky "nsew" -in $parent.pcomponentF
	    grid $parent.pcomponentLB $parent.pcomponentSB -sticky "nsew" -in $parent.pcomponentF
	    grid columnconfigure $parent.pcomponentF 0 -weight 1
	    grid rowconfigure $parent.pcomponentF 1 -weight 1
	    grid $parent.fcomponentL - -sticky "nsew" -in $parent.fcomponentF
	    grid $parent.fcomponentLB $parent.fcomponentSB -sticky "nsew" -in $parent.fcomponentF
	    grid columnconfigure $parent.fcomponentF 0 -weight 1
	    grid rowconfigure $parent.fcomponentF 1 -weight 1
	    grid $parent.pcomponentF $parent.fcomponentF -sticky "nsew" -in $parent.gridF$n -padx 4 -pady 4
	    grid columnconfigure $parent.gridF$n 0 -weight 1
	    grid columnconfigure $parent.gridF$n 1 -weight 1
	    grid rowconfigure $parent.gridF$n 1 -weight 1
	    grid $parent.gridF$n -sticky "nsew" -padx 4 -pady 4
	    grid rowconfigure $parent $n -weight 1

	    incr n
	    frame $parent.gridF$n -relief groove -bd 2
	    #Colors
	    label $parent.colorL -text "Draw Colors"
	    #Pass Color
	    frame $parent.pcolorF
	    frame $parent.pcolorFF -relief sunken -bd 2
	    label $parent.pcolorL -text "Pass" -anchor w
	    entry $parent.pcolorE -relief flat -width 12 -textvar muves_active($id,pass_color)
	    menubutton $parent.pcolorMB -relief raised -bd 2 \
		    -menu $parent.pcolorMB.m -indicatoron 1
	    # Set menubutton color
	    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    $muves_active($id,pass_color)
	    menu $parent.pcolorMB.m -tearoff 0
	    $parent.pcolorMB.m add command -label black\
		    -command "set  muves_active($id,pass_color) \"0 0 0\"; \
		    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    \$muves_active($id,pass_color)"
	    $parent.pcolorMB.m add command -label white\
		    -command "set  muves_active($id,pass_color) \"255 255 255\"; \
		    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    \$muves_active($id,pass_color)"
	    $parent.pcolorMB.m add command -label red\
		    -command "set  muves_active($id,pass_color) \"255 0 0\"; \
		    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    \$muves_active($id,pass_color)"
	    $parent.pcolorMB.m add command -label green\
		    -command "set  muves_active($id,pass_color) \"0 255 0\"; \
		    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    \$muves_active($id,pass_color)"
	    $parent.pcolorMB.m add command -label blue\
		    -command "set  muves_active($id,pass_color) \"0 0 255\"; \
		    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    \$muves_active($id,pass_color)"
	    $parent.pcolorMB.m add command -label yellow\
		    -command "set  muves_active($id,pass_color) \"255 255 0\"; \
		    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    \$muves_active($id,pass_color)"
	    $parent.pcolorMB.m add command -label cyan\
		    -command "set  muves_active($id,pass_color) \"0 255 255\"; \
		    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    \$muves_active($id,pass_color)"
	    $parent.pcolorMB.m add command -label magenta\
		    -command "set  muves_active($id,pass_color) \"255 0 255\"; \
		    setWidgetRGBColor $parent.pcolorMB muves_active($id,pass_color) \
		    \$muves_active($id,pass_color)"
	    $parent.pcolorMB.m add separator
	    $parent.pcolorMB.m add command -label "Color Tool..."\
		    -command "muves_choose_pcolor $id $top"
	    #Fail Color
	    frame $parent.fcolorF
	    frame $parent.fcolorFF -relief sunken -bd 2
	    label $parent.fcolorL -text "Fail" -anchor w
	    entry $parent.fcolorE -relief flat -width 12 -textvar muves_active($id,fail_color)
	    menubutton $parent.fcolorMB -relief raised -bd 2 \
		    -menu $parent.fcolorMB.m -indicatoron 1
	    # Set menubutton color
	    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    $muves_active($id,fail_color)
	    menu $parent.fcolorMB.m -tearoff 0
	    $parent.fcolorMB.m add command -label black\
		    -command "set  muves_active($id,fail_color) \"0 0 0\"; \
		    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    \$muves_active($id,fail_color)"
	    $parent.fcolorMB.m add command -label white\
		    -command "set  muves_active($id,fail_color) \"255 255 255\"; \
		    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    \$muves_active($id,fail_color)"
	    $parent.fcolorMB.m add command -label red\
		    -command "set  muves_active($id,fail_color) \"255 0 0\"; \
		    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    \$muves_active($id,fail_color)"
	    $parent.fcolorMB.m add command -label green\
		    -command "set  muves_active($id,fail_color) \"0 255 0\"; \
		    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    \$muves_active($id,fail_color)"
	    $parent.fcolorMB.m add command -label blue\
		    -command "set  muves_active($id,fail_color) \"0 0 255\"; \
		    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    \$muves_active($id,fail_color)"
	    $parent.fcolorMB.m add command -label yellow\
		    -command "set  muves_active($id,fail_color) \"255 255 0\"; \
		    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    \$muves_active($id,fail_color)"
	    $parent.fcolorMB.m add command -label cyan\
		    -command "set  muves_active($id,fail_color) \"0 255 255\"; \
		    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    \$muves_active($id,fail_color)"
	    $parent.fcolorMB.m add command -label magenta\
		    -command "set  muves_active($id,fail_color) \"255 0 255\"; \
		    setWidgetRGBColor $parent.fcolorMB muves_active($id,fail_color) \
		    \$muves_active($id,fail_color)"
	    $parent.fcolorMB.m add separator
	    $parent.fcolorMB.m add command -label "Color Tool..."\
		    -command "muves_choose_fcolor $id $top"
	    grid $parent.pcolorL -sticky "ew" -in $parent.pcolorF
	    grid $parent.pcolorE $parent.pcolorMB -sticky "ew" -in $parent.pcolorFF
	    grid columnconfigure $parent.pcolorFF 0 -weight 1
	    grid $parent.pcolorFF -sticky "ew" -in $parent.pcolorF
	    grid columnconfigure $parent.pcolorF 0 -weight 1
	    grid $parent.fcolorL -sticky "ew" -in $parent.fcolorF
	    grid $parent.fcolorE $parent.fcolorMB -sticky "ew" -in $parent.fcolorFF
	    grid columnconfigure $parent.fcolorFF 0 -weight 1
	    grid $parent.fcolorFF -sticky "ew" -in $parent.fcolorF
	    grid columnconfigure $parent.fcolorF 0 -weight 1
	    grid $parent.colorL - - -sticky "ew" -in $parent.gridF$n -padx 4 -pady 4
	    grid $parent.pcolorF x $parent.fcolorF -sticky "ew" -in $parent.gridF$n -padx 4 -pady 4
	    grid columnconfigure $parent.gridF$n 0 -weight 1
	    grid columnconfigure $parent.gridF$n 2 -weight 1
	    grid $parent.gridF$n -sticky "nsew" -padx 4 -pady 4

	    #Buttons
	    incr n
	    frame $parent.gridF$n
	    button $parent.pdrawB -relief raised -text "Draw Pass" \
		    -command "eval muves_cdraw \\\"\$muves_active($id,pass_color)\\\" \
		    \[$parent.pcomponentLB get 0 end\]"
	    button $parent.peraseB -relief raised -text "Erase Pass" \
		    -command "eval muves_erase \[$parent.pcomponentLB get 0 end\]"
	    button $parent.fdrawB -relief raised -text "Draw Fail" \
		    -command "eval muves_cdraw \\\"\$muves_active($id,fail_color)\\\" \
		    \[$parent.fcomponentLB get 0 end\]"
	    button $parent.feraseB -relief raised -text "Erase Fail" \
		    -command "eval muves_erase \[$parent.fcomponentLB get 0 end\]"
	    button $parent.drawshotB -relief raised -text "Draw Shot" \
		    -command "muves_draw_shot 0"
	    button $parent.eraseshotB -relief raised -text "Erase Shot" \
		    -command "muves_erase_shot 0"
	    button $parent.zapB -relief raised -text "Zap" \
		    -command "_mged_Z"
	    button $parent.dismissB -relief raised -text "Dismiss" \
		    -command "destroy $top"
	    grid $parent.pdrawB $parent.peraseB x $parent.fdrawB $parent.feraseB \
		    -sticky "ew" -in $parent.gridF$n
	    grid $parent.drawshotB $parent.eraseshotB x $parent.zapB $parent.dismissB \
		    -sticky "ew" -in $parent.gridF$n
	    grid columnconfigure $parent.gridF$n 2 -weight 1
	    grid $parent.gridF$n -sticky "nsew" -padx 4 -pady 4

	    grid columnconfigure $parent 0 -weight 1

	    bind $parent.val1E <Return> "muves_update_components $id $top"
	    bind $parent.val2E <Return> "muves_update_components $id $top"
	}
	color_ramp {
	    incr n
	    frame $parent.gridF$n -relief groove -bd 2
	    label $parent.constraintL -text "Constraints"
	    frame $parent.vtypeF -relief flat -bd 2
	    label $parent.vtypeL -text "Val Type: "
	    radiobutton $parent.minRB -text "min" -anchor w \
		    -value min -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    radiobutton $parent.meanRB -text "mean" -anchor w \
		    -value mean -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    radiobutton $parent.maxRB -text "max" -anchor w \
		    -value max -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    menubutton $parent.color_rampMB -textvariable muves_color_ramp($id,label) \
		    -relief raised -bd 2 -indicatoron 1\
		    -menu $parent.color_rampMB.m
	    menu $parent.color_rampMB.m -tearoff $mged_default(tearoff_menus)
	    $parent.color_rampMB.m add command -label $muves_color_ramp($id,0,label) \
		    -command "toggle_ramp $id $top 0"
	    $parent.color_rampMB.m add command -label $muves_color_ramp($id,1,label) \
		    -command "toggle_ramp $id $top 1"
	    $parent.color_rampMB.m add command -label $muves_color_ramp($id,2,label) \
		    -command "toggle_ramp $id $top 2"
	    $parent.color_rampMB.m add command -label $muves_color_ramp($id,3,label) \
		    -command "toggle_ramp $id $top 3"

	    grid $parent.constraintL -in $parent.gridF$n -pady 4
	    grid $parent.vtypeL $parent.minRB $parent.meanRB $parent.maxRB x -in $parent.vtypeF -sticky nsew
	    grid $parent.vtypeF -in $parent.gridF$n -sticky nsew -padx 4 -pady 4
	    grid columnconfigure $parent.vtypeF 4 -weight 1
	    grid $parent.color_rampMB -in $parent.gridF$n -padx 4 -pady 4
	    grid columnconfigure $parent.gridF$n 0 -weight 1
	    grid $parent.gridF$n -sticky "nsew" -padx 4 -pady 4

	    #Buttons
	    incr n
	    frame $parent.gridF$n
	    button $parent.drawB -relief raised -text "Draw Components" \
		    -command "muves_ramp_draw $id"
	    button $parent.eraseB -relief raised -text "Erase Components" \
		    -command "muves_ramp_erase $id"
	    button $parent.drawshotB -relief raised -text "Draw Shot" \
		    -command "muves_draw_shot 0"
	    button $parent.eraseshotB -relief raised -text "Erase Shot" \
		    -command "muves_erase_shot 0"
	    button $parent.zapB -relief raised -text "Zap" \
		    -command "_mged_Z"
	    button $parent.dismissB -relief raised -text "Dismiss" \
		    -command "destroy $top"
	    grid $parent.drawB $parent.eraseB x x x -sticky "ew" -in $parent.gridF$n
	    grid $parent.drawshotB $parent.eraseshotB x $parent.zapB $parent.dismissB \
		    -sticky "ew" -in $parent.gridF$n
	    grid columnconfigure $parent.gridF$n 2 -weight 1
	    grid $parent.gridF$n -sticky "nsew" -padx 4 -pady 4

	    grid columnconfigure $parent 0 -weight 1

	    toggle_ramp $id $top $muves_color_ramp($id,ramp)
	}
    }
}

proc muves_toggle_mode { id top row } {
    global muves_active

    switch $muves_active($id,mode) {
	pass_fail {
	    catch { grid forget $top.color_rampF }
	    grid $top.pass_failF -row 1 -sticky nsew
	    grid rowconfigure $top $row -weight 1
	}
	color_ramp {
	    catch { grid forget $top.pass_failF }
	    grid $top.color_rampF -row 1 -sticky nsew
	    grid rowconfigure $top $row -weight 0
	}
    }

    muves_update_components $id $top
}

proc toggle_ramp { id top ramp } {
    global muves_color_ramp

    set muves_color_ramp($id,ramp) $ramp
    set muves_color_ramp($id,label) $muves_color_ramp($id,$ramp,label)
    muves_update_components $id $top
}

proc muves_update_components { id top } {
    global muves_states
    global muves_active
    global muves_color_ramp

    # update active state name
    set muves_active($id,clabel) \
	    "Components for [lindex $muves_states(names) $muves_active($id,sindex)]"

    wm title $top [lindex $muves_states(names) $muves_active($id,sindex)]

    # get the number of values in state vector "state"
    set nval [lindex $muves_states(nval) $muves_active($id,sindex)]

    # get the processed vector of type "type" for "shot_record" and "state"
    set type $muves_active($id,pvt)
    set svec [lindex $muves_states(SR:0,$type) $muves_active($id,sindex)]

    # get the list of components for "state"
    set sdef [lindex $muves_states(defs) $muves_active($id,sindex)]

    switch $muves_active($id,mode) {
	pass_fail {
	    # first delete any existing members
	    $top.pass_failF.pcomponentLB delete 0 end
	    $top.pass_failF.fcomponentLB delete 0 end

	    set pf [muves_get_components_in_range $nval $svec $sdef \
		    $muves_active($id,op1) $muves_active($id,op2) \
		    $muves_active($id,cval1) $muves_active($id,cval2)]

	    #load components that "passed"
	    eval $top.pass_failF.pcomponentLB insert end [lindex [lindex $pf 0] 1]

	    #load components that "failed"
	    eval $top.pass_failF.fcomponentLB insert end [lindex [lindex $pf 1] 1]
	}
	color_ramp {
	    set ramp $muves_color_ramp($id,ramp)
	    set n $muves_color_ramp($id,$ramp,num)
	    if [info exists muves_color_ramp($id,$ramp,components)] {
		unset muves_color_ramp($id,$ramp,components)
	    }
	    set muves_color_ramp($id,$ramp,components) {}
	    for { set ri 0 } { $ri < $n } { incr ri } {
		set rval [lindex $muves_color_ramp($id,$ramp,range) $ri]
		set lval [lindex $rval 0]
		set uval [lindex $rval 1]

		if {$nval > 0} {
# pf is a pass/fail list with the following form:
# { { {pv1 pv2 ... pvn} {pc1 pc2 ... pcn} } { {fv1 fv2 ... fvn} {fc1 fc2 ... fcn} } }
# pv - pass value           pc - pass component
# fv - fail value           fc - fail component
		    set pf [muves_get_components_in_range $nval $svec $sdef <= <= $lval $uval]
		    lappend muves_color_ramp($id,$ramp,components) [lindex [lindex $pf 0] 1]

		    # assign leftovers to svec and sdef
		    set svec [lindex [lindex $pf 1] 0]
		    set sdef [lindex [lindex $pf 1] 1]
		    set nval [llength $svec]
		} else {
		    lappend muves_color_ramp($id,$ramp,components) {}
		}
	    }
	}
    }
}

proc muves_choose_pcolor { id parent } {
    set child pcolor

    cadColorWidget dialog $parent $child\
	    -title "Muves Pass Color"\
	    -initialcolor [$parent.pcolorMB cget -background]\
	    -ok "muves_pcolor_ok $id $parent $parent.$child"\
	    -cancel "cadColorWidget_destroy $parent.$child"
}

proc muves_choose_fcolor { id parent } {
    set child fcolor

    cadColorWidget dialog $parent $child\
	    -title "Muves Fail Color"\
	    -initialcolor [$parent.fcolorMB cget -background]\
	    -ok "muves_fcolor_ok $id $parent $parent.$child"\
	    -cancel "cadColorWidget_destroy $parent.$child"
}

proc muves_pcolor_ok { id parent w } {
    global muves_active

    upvar #0 $w data

    $parent.pcolorMB configure -bg $data(finalColor)
    set muves_active($id,pass_color) "$data(red) $data(green) $data(blue)"

    destroy $w
    unset data
}

proc muves_fcolor_ok { id parent w } {
    global muves_active

    upvar #0 $w data

    $parent.fcolorMB configure -bg $data(finalColor)
    set muves_active($id,fail_color) "$data(red) $data(green) $data(blue)"

    destroy $w
    unset data
}

proc muves_states_init { rm_file fr_file } {
    muves_read_region_map $rm_file
    muves_read_final_results $fr_file
    muves_process_shot_records
}

proc muves_states_initp { rm_file fr_file pfr_file } {
    global muves_states

    muves_read_region_map $rm_file
    muves_read_final_results $fr_file
    source $pfr_file
}

proc muves_read_region_map { rm_file } {
    global muves_regions
    global muves_to_rid

    set muves_regions {}
    set rm_file_cid [open $rm_file]

    while {[eof $rm_file_cid] == 0} {
	gets $rm_file_cid line
	set len [llength $line]

	if { $len > 1 } {
	    set name [lindex $line 0]
	    lappend muves_regions $name
	    set muves_to_rid($name) [lrange $line 1 end]
	}
    }

    close $rm_file_cid
}

proc muves_read_final_results { fr_file } {
    global muves_threats
    global muves_views
    global muves_states
    global muves_shot_records

    set fr_file_cid [open $fr_file]

    ##################### skip to threats #####################
    while {[eof $fr_file_cid] == 0} {
	gets $fr_file_cid line

	if {$line == "Q:threat"} {
	    break
	}
    }

    ##################### process threats #####################
    set muves_threats {}
    while {[eof $fr_file_cid] == 0} {
	gets $fr_file_cid line

	if {$line == "Q:modkeys"} {
	    break
	}

	lappend muves_threats [lrange $line 0 end]
    }

    ##################### skip to views #######################
    while {[eof $fr_file_cid] == 0} {
	gets $fr_file_cid line

	if {$line == "Q:view-v2"} {
	    break
	}
    }

    ##################### process views ########################
    set muves_views {}
    while {[eof $fr_file_cid] == 0} {
	gets $fr_file_cid line

	if {$line == "Q:states"} {
	    break
	}

	lappend muves_views [lrange $line 0 end]
    }

    ##################### process state vector definitions  #######################
    if [info exists muves_states] {
	unset muves_states
    }
    set muves_states(num) 0
    while {[eof $fr_file_cid] == 0} {
	gets $fr_file_cid line

	set result [regexp "\\\(\[0-9\]+\\\)\[ \t\]+(\[^ \t\]+)\[ \t\]+(\[0-9\]+)\[ \t\]+(.+)$" $line match type n state_name]
	if {$result != 1} {
	    break
	}

	incr muves_states(num)
	lappend muves_states(types) $type
	lappend muves_states(nval) $n
	lappend muves_states(names) $state_name

	#skip first brace
	gets $fr_file_cid line

	# build state vector definition in state_def
	set state_def {}
	for { set i 0 } { $i < $n } { incr i } {
	    gets $fr_file_cid line

	    #strip off parentheses and square bracketed strings
	    regexp "\[ \t\]*\[(\]?(\[^\]\[()\]+)\[\[\]?\[^\]\[()\]*\[\]\]?\[)\]?" \
		    $line match state_def_element
	    lappend state_def $state_def_element
	}
	lappend muves_states(defs) $state_def

	#skip last brace
	gets $fr_file_cid line
    }

    #the last "gets" in the loop above puts us at the line containing "EH:"

    ##################### process shot records ######################
    if [info exists muves_shot_records] {
	unset muves_shot_records
    }
    set nthreats [llength $muves_threats]
    set nviews [llength $muves_views]
    set muves_shot_records(num) [expr $nthreats * $nviews]

    # skip first shot record number
    gets $fr_file_cid line

    for { set i 0 } { $i < $muves_shot_records(num) } { incr i } {
	if [eof $fr_file_cid] {
	    break
	}

	# record aim point
	gets $fr_file_cid line
	set muves_shot_records(AP:$i) [lrange $line 2 end]

	for { set j 1 } {[eof $fr_file_cid] == 0} { incr j } {
	    gets $fr_file_cid line

	    set key [lindex $line 0]
	    if {$key != "FP:"} {
		# start next shot record or END
		set muves_shot_records($i,nshots) [expr $j - 1]

		if {$key == "V:"} {
		    # skip next shot record number
		    gets $fr_file_cid line
		}

		break
	    }

	    # record firing point
	    set muves_shot_records(SR:$i,FP:$j) [lrange $line 2 end]

	    # record state vectors for this shot
	    for { set k 0 } { $k < $muves_states(num) } { incr k } {
		gets $fr_file_cid line
		set muves_shot_records($i,$j,$k) [lrange $line 2 end]
	    }
	}
    }

    close $fr_file_cid
    muves_create_shot 0
}

proc muves_ramp_draw { id } {
    global muves_color_ramp

    set ramp $muves_color_ramp($id,ramp)
    set n $muves_color_ramp($id,$ramp,num)
    for { set ri 0 } { $ri < $n } { incr ri } {
	set rgb [lindex $muves_color_ramp($id,$ramp,rgb) $ri]
	set components [lindex $muves_color_ramp($id,$ramp,components) $ri]

	catch { eval muves_to_mged $components } region_names
	if [llength $region_names] {
	    catch { eval _mged_draw -C \"$rgb\" $region_names }
	}
    }
}

proc muves_ramp_erase { id } {
    global muves_color_ramp

    set ramp $muves_color_ramp($id,ramp)
    set n $muves_color_ramp($id,$ramp,num)
    for { set ri 0 } { $ri < $n } { incr ri } {
	set components [lindex $muves_color_ramp($id,$ramp,components) $ri]

	catch { eval muves_to_mged $components } region_names
	if [llength $region_names] {
	    catch { eval _mged_erase $region_names }
	}
    }
}

proc muves_cdraw { color args } {
    if ![llength $args] {
	return
    }

    set region_names [eval muves_to_mged $args]
    if ![llength $region_names] {
	return
    }
    catch { eval _mged_draw -C \"$color\" $region_names }

    return
}

proc muves_draw { args } {
    if ![llength $args] {
	return
    }

    set region_names [eval muves_to_mged $args]
    if ![llength $region_names] {
	return
    }
    catch { eval _mged_draw $region_names }

    return
}

proc muves_erase { args } {
    if ![llength $args] {
	return
    }

    set region_names [eval muves_to_mged $args]
    catch { eval _mged_erase $region_names }

    return
}

proc muves_to_rid { args } {
    global muves_to_rid

    set rid {}
    foreach muves_name $args {
	eval lappend rid $muves_to_rid($muves_name)
    }

    return $rid
}

proc muves_to_mged { args } {
    set ids [eval muves_to_rid $args]
    set n [llength  $ids]
    set region_ids {}
    set air_ids {}

    for { set i 0 } { $i < $n } { incr i } {
	set id [lindex $ids $i]
	regexp "\(-?\[0-9\]+\)(\[:-\]\(-?\[0-9\]+\))?" $id match id1 sub1 id2

	if { $id1 > 0 } {
	    lappend region_ids $id
	} else {
	    if { $sub1 == {} } {
		lappend air_ids [expr -1 * $id1]
	    } else {
		lappend air_ids "[expr -1 * $id1]:[expr -1 * $id2]"
	    }
	}
    }

    set mged_objects {}
    if { $region_ids != {} } {
	set mged_objects [eval whichid -s $region_ids]
    }

    if { $air_ids != {} } {
	set mged_objects [concat $mged_objects [eval whichair -s $air_ids]]
    }

    return $mged_objects
}

## muves_process_shot_records
#
# Process shot records
#
proc muves_process_shot_records {} {
    global muves_states
    global muves_shot_records

    # For each shot record "i"
    for { set i 0 } { $i < $muves_shot_records(num) } { incr i } {
	# For each state vector "j"
	for { set j 0 } { $j < $muves_states(num) } { incr j } {
	    # Get the number of values in state vector "j"
	    set nval [lindex $muves_states(nval) $j]

	    # initialize a few local variables
	    for { set m 0 } { $m < $nval } { incr m } {
		set min($m) 1000.0
		set max($m) -1000.0
		set total($m) 0.0
	    }

	    set minlist {}
	    set maxlist {}
	    set meanlist {}

	    switch [lindex $muves_states(types) $j] {
		lof -
		pk {
		    # For each shot "k" in shot record "i"
		    for { set k 1 } { $k <= $muves_shot_records($i,nshots) } { incr k } {
			# For each component in state vector "j"
			for { set m 0 } { $m < $nval } { incr m } {
			    set val [lindex $muves_shot_records($i,$k,$j) $m]

			    if { $val < $min($m) } {
				set min($m) $val
			    }

			    if { $max($m) < $val } {
				set max($m) $val
			    }

			    set total($m) [expr $total($m) + $val]
			}
		    }
		}
		killed -
		notkilled -
		hit {
		    # For each shot "k" in shot record "i"
		    for { set k 1 } { $k <= $muves_shot_records($i,nshots) } { incr k } {
			# get number of hex characters in state vector "j"
			set nhex [string length $muves_shot_records($i,$k,$j)]

			# we're going to process things backwards, so
			# set n to the last index for state vector "j"
			set n [expr $nval - 1]

			# For each hex character, process up to 4 values in state vector "j".
			# Do this until we run out of hex characters or exceed the
			# length of state vector "j".
			for { set m [expr $nhex - 1] } { $n >= 0 && $m >= 0 } { incr m -1 } {
			    set hex [string index $muves_shot_records($i,$k,$j) $m]
			    set bits [get_bits_from_hex $hex]
			    for { set bitn 3 } { $n >= 0 && $bitn >= 0 } { incr n -1; incr bitn -1 } {
				set val [lindex $bits $bitn]

				if { $val < $min($n) } {
				    set min($n) $val
				}

				if { $max($n) < $val } {
				    set max($n) $val
				}

				set total($n) [expr $total($n) + $val]
			    }
			}

			# no more hex bits left, so assume values are zero
			for {} { $n >= 0 } { incr n -1 } {
			    if { 0 < $min($n) } {
				set min($n) 0
			    }

			    if { $max($n) < 0 } {
				set max($n) 0
			    }
			}
		    }
		}
	    }

	    # turn arrays into lists
	    for { set m 0 } { $m < $nval } { incr m } {
		lappend minlist $min($m)
		lappend maxlist $max($m)
		lappend meanlist [expr $total($m) / $muves_shot_records($i,nshots)]
	    }

	    lappend muves_states(SR:$i,min) $minlist
	    lappend muves_states(SR:$i,max) $maxlist
	    lappend muves_states(SR:$i,mean) $meanlist
	}
    }
}

## muves_get_components_in_range
#
# Returns a list of two sublists of muves components. The first sublist
# contains the values and components that passed while the second contains
# the values and components that failed.
# op1, op2, val1 and val2 are evaluated to determine if the value in
# question passes.
# For example, assume the parameters have the following values:
#     op1  --->  <
#     op2  --->  <=
#     val1 --->  0.0
#     val2 --->  1.0
#
# Then evaluate each VAL as follows:     0.0 < VAL <= 1.0      where VAL is a
# member of the processed vector of interest. So, in English, this passes if VAL
# is greater than 0.0 and less than or equal to 1.0.
#
proc muves_get_components_in_range { nval svec sdef op1 op2 val1 val2 } {
    global muves_states

    if { ![muves_check_val $val1] || ![muves_check_val $val2] } {
	return { {} {} }
    }

    set pass_comp {}
    set pass_val {}
    set fail_comp {}
    set fail_val {}

    # For each value "i" in state vector "state"
    for { set i 0 } { $i < $nval } { incr i } {
	set tval [lindex $svec $i]
	if [expr $val1 $op1 $tval] {
	    if [expr $tval $op2 $val2] {
		# this components value is in range so add to list
		lappend pass_comp [lindex $sdef $i]
		lappend pass_val $tval
	    } else {
		lappend fail_comp [lindex $sdef $i]
		lappend fail_val $tval
	    }
	} else {
	    lappend fail_comp [lindex $sdef $i]
	    lappend fail_val $tval
	}
    }

    return [list [list $pass_val $pass_comp] [list $fail_val $fail_comp]]
}

proc muves_check_val { val } {
    set result [regexp "\[0-9\]+(\\.\[0-9\]+)?" $val match]

    if { $result == 1 && $val == $match } {
	return 1
    }

    return 0
}

proc muves_draw_shot { shot } {
    vdraw open shot$shot
    vdraw send
}

proc muves_erase_shot { shot } {
    catch { d _VDRWshot$shot }
}

proc muves_create_shot { shot } {
    global muves_shot_records
    global M_PI_2

    set move 0
    set draw 1

    set point1 [lrange $muves_shot_records(AP:$shot) 0 2]
    set dir [lrange $muves_shot_records(AP:$shot) 3 5]
    set dist [vscale $dir 10000]
    set point2 [vadd2 $point1 $dist]

    # make arrow
    set dist [vscale $dir 400]
    set tmp_pt [vsub2 $point2 $dist]
    set perp [mat_vec_perp $dir]
    set perp_dist [vscale $perp 100]
    set point3 [vadd2 $tmp_pt $perp_dist]
    set point4 [vsub2 $tmp_pt $perp_dist]
    set mat [mat_arb_rot $point1 $dir $M_PI_2]
    set point5 [mat4x3pnt $mat $point3]
    set point6 [mat4x3pnt $mat $point4]

    vdraw open shot$shot
    eval vdraw write 0 $move $point1
    eval vdraw write next $draw $point2
    eval vdraw write next $draw $point3
    eval vdraw write next $move $point2
    eval vdraw write next $draw $point4
    eval vdraw write next $move $point2
    eval vdraw write next $draw $point5
    eval vdraw write next $move $point2
    eval vdraw write next $draw $point6
    eval vdraw write next $draw $point4
    eval vdraw write next $draw $point5
    eval vdraw write next $draw $point3
    eval vdraw write next $draw $point6
}

proc muves_destroy_shot { shot } {
    muves_erase_shot $shot

    vdraw open shot$shot
    vdraw delete all
}

## get_bits_from_hex
#
#
#
proc get_bits_from_hex { hex } {
    switch $hex {
	0 {
	    return { 0.0 0.0 0.0 0.0 }
	}
	1 {
	    return { 0.0 0.0 0.0 1.0 }
	}
	2 {
	    return { 0.0 0.0 1.0 0.0 }
	}
	3 {
	    return { 0.0 0.0 1.0 1.0 }
	}
	4 {
	    return { 0.0 1.0 0.0 0.0 }
	}
	5 {
	    return { 0.0 1.0 0.0 1.0 }
	}
	6 {
	    return { 0.0 1.0 1.0 0.0 }
	}
	7 {
	    return { 0.0 1.0 1.0 1.0 }
	}
	8 {
	    return { 1.0 0.0 0.0 0.0 }
	}
	9 {
	    return { 1.0 0.0 0.0 1.0 }
	}
	a -
	A {
	    return { 1.0 0.0 1.0 0.0 }
	}
	b -
	B {
	    return { 1.0 0.0 1.0 1.0 }
	}
	c -
	C {
	    return { 1.0 1.0 0.0 0.0 }
	}
	d -
	D {
	    return { 1.0 1.0 0.0 1.0 }
	}
	e -
	E {
	    return { 1.0 1.0 1.0 0.0 }
	}
	f -
	F {
	    return { 1.0 1.0 1.0 1.0 }
	}
	default {
	    return { 0.0 0.0 0.0 0.0 }
	}
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
