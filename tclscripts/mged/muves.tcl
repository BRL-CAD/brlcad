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
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description - routines for looking at MUVES final results files.
#

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

    if ![info exists muves_color_ramp($id,num)] {
	set ramp -1

	incr ramp
	set muves_color_ramp($id,$ramp,label) "Ramp 0"
	set muves_color_ramp($id,$ramp,range) {{0.0 0.0} {0.0 0.1} {0.1 0.2} \
		{0.2 0.3} {0.3 0.4} {0.4 0.5} {0.5 0.6} {0.6 0.7} {0.7 0.8} \
		{0.8 0.9} {0.9 1.0}}
	set muves_color_ramp($id,$ramp,rgb) {{255 255 255} {100 100 140} {0 0 255} \
		{0 120 255} {100 200 140} {0 150 0} {0 225 0} {255 255 0} \
		{255 160 0} {255 100 100} {255 0 0}}
	set muves_color_ramp($id,$ramp,num) [llength $muves_color_ramp($id,$ramp,range)]

	incr ramp
	set muves_color_ramp($id,$ramp,label) "Red Ramp"
	set muves_color_ramp($id,$ramp,range) {{0.0 0.0} {0.0 0.1} {0.1 0.2} \
		{0.2 0.3} {0.3 0.4} {0.4 0.5} {0.5 0.6} {0.6 0.7} {0.7 0.8} \
		{0.8 0.9} {0.9 1.0}}
	set muves_color_ramp($id,$ramp,rgb) {{255 255 255} {255 229 229} {255 204 204} \
		{255 178 178} {255 153 153} {255 127 127} {255 102 102} {255 77 77} \
		{255 51 51} {255 26 26} {255 0 0}}
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
	set muves_active($id,pass_color) "0 255 0"
	set muves_active($id,fail_color) "255 0 0"
	set muves_active($id,mode) "pass_fail"
    }

    toplevel $top -screen $dpy -menu $top.menubar

    set n -1
    incr n
    frame $top.gridF$n
    label $top.stateL -text "State Vectors"
    listbox $top.stateLB -width 50 -height 8 -selectmode single \
	    -yscrollcommand "$top.stateSB set"
    scrollbar $top.stateSB -command "$top.stateLB yview"
    foreach state $muves_states(names) {
	$top.stateLB insert end $state
    }
    grid $top.stateL - -sticky "nsew" -in $top.gridF$n -pady 4
    grid $top.stateLB $top.stateSB -sticky "nsew" -in $top.gridF$n
    grid columnconfigure $top.gridF$n 0 -weight 1
    grid columnconfigure $top.gridF$n 1 -weight 0
    grid rowconfigure $top.gridF$n 1 -weight 1
    grid $top.gridF$n -sticky "nsew" -padx 4 -pady 4
    grid rowconfigure $top $n -weight 1

    incr n
    menu $top.menubar -tearoff $mged_default(tearoff_menus)
    $top.menubar add cascade -label "Mode" -underline 0 -menu $top.menubar.mode

    menu $top.menubar.mode -title "Mode" -tearoff $mged_default(tearoff_menus)
    $top.menubar.mode add radiobutton -value "pass_fail" -variable muves_active($id,mode) \
	    -label "Pass/Fail" -underline 0\
	    -command "muves_toggle_mode $id $top $n"
    $top.menubar.mode add radiobutton -value "color_ramp" -variable muves_active($id,mode) \
	    -label "Color Ramp" -underline 0\
	    -command "muves_toggle_mode $id $top $n"

    muves_toggle_mode $id $top $n
    muves_update_components $id $top

    grid columnconfigure $top 0 -weight 1

    bind $top.stateLB <ButtonPress-1> "set muves_active($id,sindex) \[%W nearest %y\]; \
	    muves_update_components $id $top"

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]
    wm geometry $top +$x+$y
    wm title $top "MGED MUVES Tool"
}

proc muves_toggle_mode { id top n } {
    global muves_active
    global muves_color_ramp
    global mged_default

    switch $muves_active($id,mode) {
	pass_fail {
	    frame $top.gridF$n -relief groove -bd 2
	    label $top.constraintL -text "Constraints"
	    frame $top.vtypeF -relief flat -bd 2
	    label $top.vtypeL -text "Val Type: "
	    radiobutton $top.minRB -text "min" -anchor w \
		    -value min -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    radiobutton $top.meanRB -text "mean" -anchor w \
		    -value mean -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    radiobutton $top.maxRB -text "max" -anchor w \
		    -value max -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    entry $top.val1E -relief sunken -bd 2 -width 12 -textvar muves_active($id,cval1)
	    menubutton $top.op1MB -relief flat -bd 2 -textvar muves_active($id,op1) -menu $top.op1MB.m
	    menu $top.op1MB.m -tearoff 0
	    $top.op1MB.m add command -label "<" \
		    -command "set muves_active($id,op1) <; muves_update_components $id $top"
	    $top.op1MB.m add command -label "<=" \
		    -command "set muves_active($id,op1) <=; muves_update_components $id $top"
	    $top.op1MB.m add command -label ">" \
		    -command "set muves_active($id,op1) >; muves_update_components $id $top"
	    $top.op1MB.m add command -label ">=" \
		    -command "set muves_active($id,op1) >=; muves_update_components $id $top"
	    label $top.valL -text "val"
	    menubutton $top.op2MB -relief flat -bd 2 -textvar muves_active($id,op2) -menu $top.op2MB.m
	    menu $top.op2MB.m -tearoff 0
	    $top.op2MB.m add command -label "<" \
		    -command "set muves_active($id,op2) <; muves_update_components $id $top"
	    $top.op2MB.m add command -label "<=" \
		    -command "set muves_active($id,op2) <=; muves_update_components $id $top"
	    $top.op2MB.m add command -label ">" \
		    -command "set muves_active($id,op2) >; muves_update_components $id $top"
	    $top.op2MB.m add command -label ">=" \
		    -command "set muves_active($id,op2) >=; muves_update_components $id $top"
	    entry $top.val2E -relief sunken -bd 2 -width 12 -textvar muves_active($id,cval2)
	    grid $top.constraintL - - - - -sticky "nsew" -in $top.gridF$n -pady 4
	    grid $top.vtypeL $top.minRB $top.meanRB $top.maxRB -sticky "w" -in $top.vtypeF
	    grid $top.vtypeF - - - - -sticky w -in $top.gridF$n -padx 4 -pady 4
	    grid $top.val1E $top.op1MB $top.valL $top.op2MB $top.val2E \
		    -sticky "nsew" -in $top.gridF$n -padx 4 -pady 4
	    grid columnconfigure $top.gridF$n 0 -weight 1
	    grid columnconfigure $top.gridF$n 4 -weight 1
	    grid $top.gridF$n -sticky "nsew" -padx 4 -pady 4

	    incr n
	    frame $top.gridF$n -relief groove -bd 2
	    label $top.componentL -textvar muves_active($id,clabel)
	    # Widgets for components that "passed"
	    frame $top.pcomponentF
	    label $top.pcomponentL -text "Pass"
	    listbox $top.pcomponentLB -selectmode single -yscrollcommand "$top.pcomponentSB set"
	    scrollbar $top.pcomponentSB -command "$top.pcomponentLB yview"
	    # Widgets for components that "failed"
	    frame $top.fcomponentF
	    label $top.fcomponentL -text "Fail"
	    listbox $top.fcomponentLB -selectmode single -yscrollcommand "$top.fcomponentSB set"
	    scrollbar $top.fcomponentSB -command "$top.fcomponentLB yview"
	    grid $top.componentL - -sticky "nsew" -in $top.gridF$n
	    grid $top.pcomponentL - -sticky "nsew" -in $top.pcomponentF
	    grid $top.pcomponentLB $top.pcomponentSB -sticky "nsew" -in $top.pcomponentF
	    grid columnconfigure $top.pcomponentF 0 -weight 1
	    grid rowconfigure $top.pcomponentF 1 -weight 1
	    grid $top.fcomponentL - -sticky "nsew" -in $top.fcomponentF
	    grid $top.fcomponentLB $top.fcomponentSB -sticky "nsew" -in $top.fcomponentF
	    grid columnconfigure $top.fcomponentF 0 -weight 1
	    grid rowconfigure $top.fcomponentF 1 -weight 1
	    grid $top.pcomponentF $top.fcomponentF -sticky "nsew" -in $top.gridF$n -padx 4 -pady 4
	    grid columnconfigure $top.gridF$n 0 -weight 1
	    grid columnconfigure $top.gridF$n 1 -weight 1
	    grid rowconfigure $top.gridF$n 1 -weight 1
	    grid $top.gridF$n -sticky "nsew" -padx 4 -pady 4
	    grid rowconfigure $top $n -weight 1

	    incr n
	    frame $top.gridF$n -relief groove -bd 2
	    #Colors
	    label $top.colorL -text "Draw Colors"
	    #Pass Color
	    frame $top.pcolorF
	    frame $top.pcolorFF -relief sunken -bd 2
	    label $top.pcolorL -text "Pass" -anchor w
	    entry $top.pcolorE -relief flat -width 12 -textvar muves_active($id,pass_color)
	    menubutton $top.pcolorMB -relief raised -bd 2 \
		    -menu $top.pcolorMB.m -indicatoron 1
	    # Set menubutton color
	    set_WidgetRGBColor $top.pcolorMB $muves_active($id,pass_color)
	    menu $top.pcolorMB.m -tearoff 0
	    $top.pcolorMB.m add command -label black\
		    -command "set  muves_active($id,pass_color) \"0 0 0\"; \
		    set_WidgetRGBColor $top.pcolorMB \$muves_active($id,pass_color)"
	    $top.pcolorMB.m add command -label white\
		    -command "set  muves_active($id,pass_color) \"255 255 255\"; \
		    set_WidgetRGBColor $top.pcolorMB \$muves_active($id,pass_color)"
	    $top.pcolorMB.m add command -label red\
		    -command "set  muves_active($id,pass_color) \"255 0 0\"; \
		    set_WidgetRGBColor $top.pcolorMB \$muves_active($id,pass_color)"
	    $top.pcolorMB.m add command -label green\
		    -command "set  muves_active($id,pass_color) \"0 255 0\"; \
		    set_WidgetRGBColor $top.pcolorMB \$muves_active($id,pass_color)"
	    $top.pcolorMB.m add command -label blue\
		    -command "set  muves_active($id,pass_color) \"0 0 255\"; \
		    set_WidgetRGBColor $top.pcolorMB \$muves_active($id,pass_color)"
	    $top.pcolorMB.m add command -label yellow\
		    -command "set  muves_active($id,pass_color) \"255 255 0\"; \
		    set_WidgetRGBColor $top.pcolorMB \$muves_active($id,pass_color)"
	    $top.pcolorMB.m add command -label cyan\
		    -command "set  muves_active($id,pass_color) \"0 255 255\"; \
		    set_WidgetRGBColor $top.pcolorMB \$muves_active($id,pass_color)"
	    $top.pcolorMB.m add command -label magenta\
		    -command "set  muves_active($id,pass_color) \"255 0 255\"; \
		    set_WidgetRGBColor $top.pcolorMB \$muves_active($id,pass_color)"
	    $top.pcolorMB.m add separator
	    $top.pcolorMB.m add command -label "Color Tool..."\
		    -command "muves_choose_pcolor $id $top"
	    #Fail Color
	    frame $top.fcolorF
	    frame $top.fcolorFF -relief sunken -bd 2
	    label $top.fcolorL -text "Fail" -anchor w
	    entry $top.fcolorE -relief flat -width 12 -textvar muves_active($id,fail_color)
	    menubutton $top.fcolorMB -relief raised -bd 2 \
		    -menu $top.fcolorMB.m -indicatoron 1
	    # Set menubutton color
	    set_WidgetRGBColor $top.fcolorMB $muves_active($id,fail_color)
	    menu $top.fcolorMB.m -tearoff 0
	    $top.fcolorMB.m add command -label black\
		    -command "set  muves_active($id,fail_color) \"0 0 0\"; \
		    set_WidgetRGBColor $top.fcolorMB \$muves_active($id,fail_color)"
	    $top.fcolorMB.m add command -label white\
		    -command "set  muves_active($id,fail_color) \"255 255 255\"; \
		    set_WidgetRGBColor $top.fcolorMB \$muves_active($id,fail_color)"
	    $top.fcolorMB.m add command -label red\
		    -command "set  muves_active($id,fail_color) \"255 0 0\"; \
		    set_WidgetRGBColor $top.fcolorMB \$muves_active($id,fail_color)"
	    $top.fcolorMB.m add command -label green\
		    -command "set  muves_active($id,fail_color) \"0 255 0\"; \
		    set_WidgetRGBColor $top.fcolorMB \$muves_active($id,fail_color)"
	    $top.fcolorMB.m add command -label blue\
		    -command "set  muves_active($id,fail_color) \"0 0 255\"; \
		    set_WidgetRGBColor $top.fcolorMB \$muves_active($id,fail_color)"
	    $top.fcolorMB.m add command -label yellow\
		    -command "set  muves_active($id,fail_color) \"255 255 0\"; \
		    set_WidgetRGBColor $top.fcolorMB \$muves_active($id,fail_color)"
	    $top.fcolorMB.m add command -label cyan\
		    -command "set  muves_active($id,fail_color) \"0 255 255\"; \
		    set_WidgetRGBColor $top.fcolorMB \$muves_active($id,fail_color)"
	    $top.fcolorMB.m add command -label magenta\
		    -command "set  muves_active($id,fail_color) \"255 0 255\"; \
		    set_WidgetRGBColor $top.fcolorMB \$muves_active($id,fail_color)"
	    $top.fcolorMB.m add separator
	    $top.fcolorMB.m add command -label "Color Tool..."\
		    -command "muves_choose_fcolor $id $top"
	    grid $top.pcolorL -sticky "ew" -in $top.pcolorF
	    grid $top.pcolorE $top.pcolorMB -sticky "ew" -in $top.pcolorFF
	    grid columnconfigure $top.pcolorFF 0 -weight 1
	    grid $top.pcolorFF -sticky "ew" -in $top.pcolorF
	    grid columnconfigure $top.pcolorF 0 -weight 1
	    grid $top.fcolorL -sticky "ew" -in $top.fcolorF
	    grid $top.fcolorE $top.fcolorMB -sticky "ew" -in $top.fcolorFF
	    grid columnconfigure $top.fcolorFF 0 -weight 1
	    grid $top.fcolorFF -sticky "ew" -in $top.fcolorF
	    grid columnconfigure $top.fcolorF 0 -weight 1
	    grid $top.colorL - - -sticky "ew" -in $top.gridF$n -padx 4 -pady 4
	    grid $top.pcolorF x $top.fcolorF -sticky "ew" -in $top.gridF$n -padx 4 -pady 4
	    grid columnconfigure $top.gridF$n 0 -weight 1
	    grid columnconfigure $top.gridF$n 2 -weight 1
	    grid $top.gridF$n -sticky "nsew" -padx 4 -pady 4

	    #Buttons
	    incr n
	    frame $top.gridF$n
	    button $top.pdrawB -relief raised -text "Draw Pass" \
		    -command "eval muves_cdraw \\\"\$muves_active($id,pass_color)\\\" \
		    \[$top.pcomponentLB get 0 end\]"
	    button $top.peraseB -relief raised -text "Erase Pass" \
		    -command "eval muves_erase \[$top.pcomponentLB get 0 end\]"
	    button $top.fdrawB -relief raised -text "Draw Fail" \
		    -command "eval muves_cdraw \\\"\$muves_active($id,fail_color)\\\" \
		    \[$top.fcomponentLB get 0 end\]"
	    button $top.feraseB -relief raised -text "Erase Fail" \
		    -command "eval muves_erase \[$top.fcomponentLB get 0 end\]"
	    button $top.dismissB -relief raised -text "Dismiss" \
		    -command "destroy $top"
	    grid $top.pdrawB $top.peraseB x $top.fdrawB $top.feraseB $top.dismissB \
		    -sticky "ew" -in $top.gridF$n
	    grid columnconfigure $top.gridF$n 2 -weight 1
	    grid $top.gridF$n -sticky "nsew" -padx 4 -pady 4

	    bind $top.val1E <Return> "muves_update_components $id $top"
	    bind $top.val2E <Return> "muves_update_components $id $top"
	}
	color_ramp {
	    frame $top.gridF$n -relief groove -bd 2
	    label $top.constraintL -text "Constraints"
	    frame $top.vtypeF -relief flat -bd 2
	    label $top.vtypeL -text "Val Type: "
	    radiobutton $top.minRB -text "min" -anchor w \
		    -value min -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    radiobutton $top.meanRB -text "mean" -anchor w \
		    -value mean -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    radiobutton $top.maxRB -text "max" -anchor w \
		    -value max -variable muves_active($id,pvt) \
		    -command "muves_update_components $id $top"
	    menubutton $top.color_rampMB -textvariable muves_color_ramp($id,label) \
		    -relief raised -bd 2 -indicatoron 1\
		    -menu $top.color_rampMB.m
	    menu $top.color_rampMB.m -tearoff $mged_default(tearoff_menus)
	    $top.color_rampMB.m add command -label $muves_color_ramp($id,0,label) \
		    -command "toggle_ramp $id $top 0"
	    $top.color_rampMB.m add command -label $muves_color_ramp($id,1,label) \
		    -command "toggle_ramp $id $top 1"

	    grid $top.constraintL -in $top.gridF$n -pady 4
	    grid $top.vtypeL $top.minRB $top.meanRB $top.maxRB x -in $top.vtypeF -sticky nsew
	    grid $top.vtypeF -in $top.gridF$n -sticky nsew -padx 4 -pady 4
	    grid columnconfigure $top.vtypeF 4 -weight 1
	    grid $top.color_rampMB -in $top.gridF$n -padx 4 -pady 4
	    grid columnconfigure $top.gridF$n 0 -weight 1
	    grid $top.gridF$n -sticky "nsew" -padx 4 -pady 4

	    #Buttons
	    incr n
	    frame $top.gridF$n
	    button $top.drawB -relief raised -text "Draw" \
		    -command "muves_ramp_draw $id"
	    button $top.eraseB -relief raised -text "Erase" \
		    -command "muves_ramp_erase $id"
	    button $top.zapB -relief raised -text "Zap" \
		    -command "_mged_Z"
	    button $top.dismissB -relief raised -text "Dismiss" \
		    -command "destroy $top"
	    grid $top.drawB $top.eraseB x $top.zapB $top.dismissB -sticky "ew" -in $top.gridF$n
	    grid columnconfigure $top.gridF$n 2 -weight 1
	    grid $top.gridF$n -sticky "nsew" -padx 4 -pady 4

	    toggle_ramp $id $top $muves_color_ramp($id,ramp)
	}
    }
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

    switch $muves_active($id,mode) {
	pass_fail {
	    # first delete any existing members
	    $top.pcomponentLB delete 0 end
	    $top.fcomponentLB delete 0 end

	    set components [muves_get_components_in_range 0 $muves_active($id,sindex) \
		    $muves_active($id,pvt) $muves_active($id,op1) $muves_active($id,op2) \
		    $muves_active($id,cval1) $muves_active($id,cval2)]

	    #load components that "passed"
	    eval $top.pcomponentLB insert end [lindex $components 0]

	    #load components that "failed"
	    eval $top.fcomponentLB insert end [lindex $components 1]
	}
	color_ramp {
	    # get the number of values in state vector "state"
	    set nval [lindex $muves_states(nval) $muves_active($id,sindex)]

	    # get the processed vector of type "type" for "shot_record" and "state"
	    set type $muves_active($id,pvt)
	    set svec [lindex $muves_states(SR:0,$type) $muves_active($id,sindex)]

	    # get the list of components for "state"
	    set sdef [lindex $muves_states(defs) $muves_active($id,sindex)]

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

		lappend muves_color_ramp($id,$ramp,components) \
			[muves_get_components_in_range2 $nval $svec $sdef \
			<= <= $lval $uval]
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

proc muves_data_init { rm_file fr_file } {
    muves_read_region_map $rm_file
    muves_read_final_results $fr_file
    muves_process_shot_records
}

proc muves_read_region_map { rm_file } {
    global muves_regions
    global muves_to_rid

    set muves_regions {}
    set rm_file_cid [open $rm_file]

    while ![eof $rm_file_cid] {
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
    while { ![eof $fr_file_cid] } {
	gets $fr_file_cid line

	if {$line == "Q:threat"} {
	    break
	}
    }

    ##################### process threats #####################
    set muves_threats {}
    while { ![eof $fr_file_cid] } {
	gets $fr_file_cid line

	if {$line == "Q:modkeys"} {
	    break
	}

	lappend muves_threats [lrange $line 0 end]
    }

    ##################### skip to views #######################
    while { ![eof $fr_file_cid] } {
	gets $fr_file_cid line

	if {$line == "Q:view-v2"} {
	    break
	}
    }

    ##################### process views ########################
    set muves_views {}
    while { ![eof $fr_file_cid] } {
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
    while { ![eof $fr_file_cid] } {
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

	for { set j 1 } { ![eof $fr_file_cid] } { incr j } {
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
    eval whichid -s [eval muves_to_rid $args]
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
# contains components that passed while the second contains components
# that failed.
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
proc muves_get_components_in_range { shot_record state type op1 op2 val1 val2 } {
    global muves_states

    # get the number of values in state vector "state"
    set nval [lindex $muves_states(nval) $state]

    # get the processed vector of type "type" for "shot_record" and "state"
    set svec [lindex $muves_states(SR:$shot_record,$type) $state]

    # get the list of components for "state"
    set sdef [lindex $muves_states(defs) $state]

    set pass {}
    set fail {}

    # For each value "i" in state vector "state"
    for { set i 0 } { $i < $nval } { incr i } {
	set tval [lindex $svec $i]
	if [expr $val1 $op1 $tval] {
	    if [expr $tval $op2 $val2] {
		# this components value is in range so add to list
		lappend pass [lindex $sdef $i]
	    } else {
		lappend fail [lindex $sdef $i]
	    }
	} else {
	    lappend fail [lindex $sdef $i]
	}
    }

    set components [list $pass $fail]

    return $components
}

proc muves_get_components_in_range2 { nval svec sdef op1 op2 val1 val2 } {
    global muves_states

    set pass {}

    # For each value "i" in state vector "state"
    for { set i 0 } { $i < $nval } { incr i } {
	set tval [lindex $svec $i]
	if [expr $val1 $op1 $tval] {
	    if [expr $tval $op2 $val2] {
		# this components value is in range so add to list
		lappend pass [lindex $sdef $i]
	    }
	}
    }

    return $pass
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
