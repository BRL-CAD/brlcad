#                         A D C . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
#			A D C . T C L
#
# Author -
#	Robert G. Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Description -
#	Control Panel for the Angle/Distance Cursor
#

proc init_adc_control { id } {
    global mged_gui
    global mged_adc_control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    winset $mged_gui($id,active_dm)
    set top .$id.adc_control

    if [winfo exists $top] {
	raise $top
	set mged_adc_control($id) 1

	return
    }

    set padx 4
    set pady 4

    if ![info exists mged_adc_control($id,coords)] {
	set mged_adc_control($id,coords) model
	adc reset
    }

    set mged_adc_control($id,last_coords) $mged_adc_control($id,coords)

    if ![info exists mged_adc_control($id,interpval)] {
	set mged_adc_control($id,interpval) abs
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF1
    menubutton $top.coordsMB -textvariable mged_adc_control($id,coords_text)\
	    -menu $top.coordsMB.m -indicatoron 1
    menu $top.coordsMB.m -title "Coordinates" -tearoff 0
    $top.coordsMB.m add radiobutton -value model -variable mged_adc_control($id,coords)\
	    -label "Model" -command "adc_adjust_coords $id"
    hoc_register_menu_data "Coordinates" "Model" "Model Coordinates"\
	    { { summary "Set coordinate system type to model.
The model coordinate system is the
coordinate system that the MGED
database lives in." } }
    $top.coordsMB.m add radiobutton -value grid -variable mged_adc_control($id,coords)\
	    -label "Grid" -command "adc_adjust_coords $id"
    hoc_register_menu_data "Coordinates" "Grid" "Grid Coordinates"\
	    { { summary "Set coordinate system type to grid.
The grid coordinate system is 2D and
lives in the view plane. The origin
of this system is located by projecting
the model origin onto the view plane." } }
    menubutton $top.interpvalMB -textvariable mged_adc_control($id,interpval_text)\
	    -menu $top.interpvalMB.m -indicatoron 1
    hoc_register_data $top.interpvalMB "Value Interpretation Type"\
	    { { summary "This is a menu of the interpretation types.
There are two interpretation types - absolute
and relative. With absolute, the value is used
as is. However, with relative the value is treated
as an offset." } }
    menu $top.interpvalMB.m -title "Interpretation" -tearoff 0
    $top.interpvalMB.m add radiobutton -value abs -variable mged_adc_control($id,interpval)\
	    -label "Absolute" -command "adc_interpval $id"
    hoc_register_menu_data "Interpretation" "Absolute" "Interpret as absolute."\
	    { { summary "Interpret values at face value." } }
    $top.interpvalMB.m add radiobutton -value rel -variable mged_adc_control($id,interpval)\
	    -label "Relative" -command "adc_interpval $id"
    hoc_register_menu_data "Interpretation" "Relative" "Interpret as relative."\
	    { { summary "Interpret values as relative to current ADC values." } }
    grid $top.coordsMB x $top.interpvalMB x -sticky "nw" -in $top.gridF1 -padx $padx
    grid columnconfigure $top.gridF1 3 -weight 1

    frame $top.gridF2 -relief groove -bd 2
    frame $top.gridFF2
    label $top.posL -text "Position" -anchor e
    entry $top.posE -relief sunken -textvar mged_adc_control($id,pos)

    set hoc_data { { summary "The tick distance indicates the distance (local units)
from the ADC position to one of its ticks." } }
    label $top.tickL -text "Tick Distance" -anchor e
    hoc_register_data $top.tickL "Tick Distance" $hoc_data
    entry $top.tickE -relief sunken -width 15 -textvar mged_adc_control($id,dst)
    hoc_register_data $top.tickE "Tick Distance" $hoc_data

    set hoc_data { { summary "Angle 1 is one of two axes used to measure angles." } }
    label $top.a1L -text "Angle 1" -anchor e
    hoc_register_data $top.a1L "Angle 1" $hoc_data
    entry $top.a1E -relief sunken -width 15 -textvar mged_adc_control($id,a1)
    hoc_register_data $top.a1E "Angle 1" $hoc_data

    set hoc_data { { summary "Angle 2 is one of two axes used to measure angles." } }
    label $top.a2L -text "Angle 2" -anchor e
    hoc_register_data $top.a2L "Angle 2" $hoc_data
    entry $top.a2E -relief sunken -width 15 -textvar mged_adc_control($id,a2)
    hoc_register_data $top.a2E "Angle 2" $hoc_data

    grid $top.posL $top.posE -sticky "nsew" -in $top.gridFF2
    grid $top.tickL $top.tickE -sticky "nsew" -in $top.gridFF2
    grid $top.a1L $top.a1E -sticky "nsew" -in $top.gridFF2
    grid $top.a2L $top.a2E -sticky "nsew" -in $top.gridFF2
    grid columnconfigure $top.gridFF2 1 -weight 1
    grid rowconfigure $top.gridFF2 0 -weight 1
    grid rowconfigure $top.gridFF2 1 -weight 1
    grid rowconfigure $top.gridFF2 2 -weight 1
    grid rowconfigure $top.gridFF2 3 -weight 1
    grid $top.gridFF2 -sticky "nsew" -in $top.gridF2 -padx $padx -pady $pady
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 0 -weight 1

    frame $top.gridF3 -relief groove -bd 2
    frame $top.gridFF3
    label $top.anchorL -text "Anchor Points"
    label $top.enableL -text "Enable"
    hoc_register_data $top.enableL "Enable"\
	    { { summary "The \"Enable\" checkbuttons are used
to toggle anchoring for the participating
ADC attributes." } }
    label $top.anchor_xyzL -text "Position" -anchor e
    entry $top.anchor_xyzE -relief sunken -textvar mged_adc_control($id,pos)
    checkbutton $top.anchor_xyzCB -relief flat\
	    -offvalue 0 -onvalue 1 -variable mged_adc_control($id,anchor_pos)
    hoc_register_data $top.anchor_xyzCB "Anchor Position"\
	    { { summary "Toggle anchoring of the ADC position.
If anchoring is enabled, the ADC will remain
positioned at the anchor point. So if the view
changes while the ADC position is anchored, the
ADC will move with respect to the view." } }
    label $top.anchor_tickL -text "Tick Distance" -anchor e
    entry $top.anchor_tickE -relief sunken -textvar mged_adc_control($id,anchor_pt_dst)
    checkbutton $top.anchor_tickCB -relief flat\
	    -offvalue 0 -onvalue 1 -variable mged_adc_control($id,anchor_dst)
    hoc_register_data $top.anchor_tickCB "Tick Distance Anchor Point"\
	    { { summary "Toggle anchoring of the tick distance.
If anchoring is enabled, the tick is drawn at
a distance from the ADC center position that is
equal to the distance between the ADC center
position and the anchor point." } }
    label $top.anchor_a1L -text "Angle 1" -anchor e
    entry $top.anchor_a1E -relief sunken -textvar mged_adc_control($id,anchor_pt_a1)
    checkbutton $top.anchor_a1CB -relief flat\
	    -offvalue 0 -onvalue 1 -variable mged_adc_control($id,anchor_a1)
    hoc_register_data $top.anchor_a1CB "Angle 1 Anchor Point"\
	    { { summary "Toggle anchoring of angle 1. If anchoring
is enabled, angle 1 is always drawn through
its anchor point." } }
    label $top.anchor_a2L -text "Angle 2" -anchor e
    entry $top.anchor_a2E -relief sunken -textvar mged_adc_control($id,anchor_pt_a2)
    checkbutton $top.anchor_a2CB -relief flat\
	    -offvalue 0 -onvalue 1 -variable mged_adc_control($id,anchor_a2)
    hoc_register_data $top.anchor_a2CB "Angle 2 Anchor Point"\
	    { { summary "Toggle anchoring of angle 2. If anchoring
is enabled, angle 2 is always drawn through
its anchor point." } }
    grid $top.anchorL - $top.enableL -sticky "ew" -in $top.gridFF3
    grid $top.anchor_xyzL $top.anchor_xyzE $top.anchor_xyzCB -sticky "nsew" -in $top.gridFF3
    grid $top.anchor_tickL $top.anchor_tickE $top.anchor_tickCB -sticky "nsew" -in $top.gridFF3
    grid $top.anchor_a1L $top.anchor_a1E $top.anchor_a1CB -sticky "nsew" -in $top.gridFF3
    grid $top.anchor_a2L $top.anchor_a2E $top.anchor_a2CB -sticky "nsew" -in $top.gridFF3
    grid columnconfigure $top.gridFF3 1 -weight 1
    grid rowconfigure $top.gridFF3 1 -weight 1
    grid rowconfigure $top.gridFF3 2 -weight 1
    grid rowconfigure $top.gridFF3 3 -weight 1
    grid rowconfigure $top.gridFF3 4 -weight 1
    grid $top.gridFF3 -sticky "nsew" -in $top.gridF3 -padx $padx -pady $pady
    grid columnconfigure $top.gridF3 0 -weight 1
    grid rowconfigure $top.gridF3 0 -weight 1

    frame $top.gridF4
    checkbutton $top.drawB -relief flat -text "Draw"\
	    -offvalue 0 -onvalue 1 -variable mged_adc_control($id,draw)
    hoc_register_data $top.drawB "Draw"\
	    { { summary "Toggle drawing of the angle distance cursor." } }
    grid $top.drawB -in $top.gridF4

    frame $top.gridF5
    button $top.okB -relief raised -text "OK"\
	    -command "adc_ok $id $top"
    hoc_register_data $top.okB "OK"\
	    { { summary "Apply the values in the ADC control panel
to the angle distance cursor then close
the control panel."} }
    button $top.applyB -relief raised -text "Apply"\
	    -command "mged_apply $id \"adc_apply $id\";\
	    if {\$mged_adc_control($id,interpval) == \"abs\"} {
	        adc_load $id
            }"
    hoc_register_data $top.applyB "Apply"\
	    { { summary "Apply the values in the ADC control panel
to the angle distance cursor." } }
    button $top.resetB -relief raised -text "Reset"\
	    -command "mged_apply $id \"adc reset\";\
	    if {\$mged_adc_control($id,interpval) == \"abs\"} {
	        adc_load $id
            }"
    hoc_register_data $top.resetB "Reset"\
	    { { summary "Reset the angle distance cursor to its
default values." } }
    button $top.loadB -relief raised
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top; set mged_adc_control($id) 0 }"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss/close the ADC control panel." } }
    grid $top.okB $top.applyB x $top.resetB $top.loadB x $top.dismissB -sticky "ew" -in $top.gridF5
    grid columnconfigure $top.gridF5 2 -weight 1
    grid columnconfigure $top.gridF5 5 -weight 1

    grid $top.gridF1 -sticky "ew" -padx $padx -pady $pady
    grid $top.gridF2 -sticky "nsew" -padx $padx -pady $pady
    grid $top.gridF3 -sticky "nsew" -padx $padx -pady $pady
    grid $top.gridF4 -sticky "ew" -padx $padx -pady $pady
    grid $top.gridF5 -sticky "ew" -padx $padx -pady $pady
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 1 -weight 1
    grid rowconfigure $top 2 -weight 1

    adc_interpval $id
    adc_adjust_coords $id

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top; set mged_adc_control($id) 0 }"
    wm title $top "ADC Control Panel ($id)"
}

proc adc_ok { id top } {
    global mged_adc_control

    mged_apply $id "adc_apply $id"
    catch { destroy $top }

    set mged_adc_control($id) 0
}

proc adc_apply { id } {
    global mged_adc_control

    adc anchor_pos 0
    adc anchor_a1 0
    adc anchor_a2 0
    adc anchor_dst 0

    switch $mged_adc_control($id,interpval) {
	abs {
	    adc a1 $mged_adc_control($id,a1)
	    adc a2 $mged_adc_control($id,a2)
	    adc dst $mged_adc_control($id,dst)

	    adc_apply_abs $id
	}
	rel {
	    adc -i a1 $mged_adc_control($id,a1)
	    adc -i a2 $mged_adc_control($id,a2)
	    adc -i dst $mged_adc_control($id,dst)

	    adc_apply_rel $id
	}
    }

    switch $mged_adc_control($id,coords) {
	model {
	    if {$mged_adc_control($id,anchor_pos)} {
		adc anchor_pos 1
	    }
	}
	grid {
	    if {$mged_adc_control($id,anchor_pos)} {
		adc anchor_pos 2
	    }
	}
    }

    adc anchor_a1 $mged_adc_control($id,anchor_a1)
    adc anchor_a2 $mged_adc_control($id,anchor_a2)
    adc anchor_dst $mged_adc_control($id,anchor_dst)
    adc draw $mged_adc_control($id,draw)
}

proc adc_apply_abs { id } {
    global mged_adc_control

    switch $mged_adc_control($id,coords) {
	model {
	    eval adc xyz $mged_adc_control($id,pos)
	    eval adc anchorpoint_dst $mged_adc_control($id,anchor_pt_dst)
	    eval adc anchorpoint_a1 $mged_adc_control($id,anchor_pt_a1)
	    eval adc anchorpoint_a2 $mged_adc_control($id,anchor_pt_a2)
	}
	grid {
	    eval adc hv $mged_adc_control($id,pos)
	    eval adc anchorpoint_dst [eval grid2model_lu $mged_adc_control($id,anchor_pt_dst)]
	    eval adc anchorpoint_a1 [eval grid2model_lu $mged_adc_control($id,anchor_pt_a1)]
	    eval adc anchorpoint_a2 [eval grid2model_lu $mged_adc_control($id,anchor_pt_a2)]
	}
    }
}

proc adc_apply_rel { id } {
    global mged_gui
    global mged_adc_control

    switch $mged_adc_control($id,coords) {
	model {
	    eval adc -i xyz $mged_adc_control($id,pos)
	    eval adc -i anchorpoint_dst $mged_adc_control($id,anchor_pt_dst)
	    eval adc -i anchorpoint_a1 $mged_adc_control($id,anchor_pt_a1)
	    eval adc -i anchorpoint_a2 $mged_adc_control($id,anchor_pt_a2)
	}
	grid {
	    eval adc -i xyz [eval view2model_vec $mged_adc_control($id,pos) 0.0]
	    eval adc -i anchorpoint_dst [eval view2model_vec $mged_adc_control($id,anchor_pt_dst) 0.0]
	    eval adc -i anchorpoint_a1 [eval view2model_vec $mged_adc_control($id,anchor_pt_a1) 0.0]
	    eval adc -i anchorpoint_a2 [eval view2model_vec $mged_adc_control($id,anchor_pt_a2) 0.0]
	}
    }
}

proc adc_load { id } {
    global mged_gui
    global mged_adc_control

    if ![winfo exists .$id.adc_control] {
	return
    }

    winset $mged_gui($id,active_dm)

    set mged_adc_control($id,draw) [adc draw]
    set mged_adc_control($id,dst) [format "%.4f" [adc dst]]
    set mged_adc_control($id,a1) [format "%.4f" [adc a1]]
    set mged_adc_control($id,a2) [format "%.4f" [adc a2]]
    set mged_adc_control($id,anchor_dst) [adc anchor_dst]
    set mged_adc_control($id,anchor_a1) [adc anchor_a1]
    set mged_adc_control($id,anchor_a2) [adc anchor_a2]
    set mged_adc_control($id,anchor_pos) [adc anchor_pos]

    if {$mged_adc_control($id,anchor_pos) > 1} {
	set mged_adc_control($id,anchor_pos) 1
    }

    switch $mged_adc_control($id,coords) {
	model {
	    set mged_adc_control($id,pos) [eval format \"%.4f %.4f %.4f\" [adc xyz]]
	    set mged_adc_control($id,anchor_pt_dst) [eval format \"%.4f %.4f %.4f\" [adc anchorpoint_dst]]
	    set mged_adc_control($id,anchor_pt_a1) [eval format \"%.4f %.4f %.4f\" [adc anchorpoint_a1]]
	    set mged_adc_control($id,anchor_pt_a2) [eval format \"%.4f %.4f %.4f\" [adc anchorpoint_a2]]
	}
	grid {
	    set mged_adc_control($id,pos) [eval format \"%.4f %.4f\" [adc hv]]
	    set mged_adc_control($id,anchor_pt_dst) [eval format \"%.4f %.4f\" [eval model2grid_lu [adc anchorpoint_dst]]]
	    set mged_adc_control($id,anchor_pt_a1) [eval format \"%.4f %.4f\" [eval model2grid_lu [adc anchorpoint_a1]]]
	    set mged_adc_control($id,anchor_pt_a2) [eval format \"%.4f %.4f\" [eval model2grid_lu [adc anchorpoint_a2]]]
	}
    }
}

proc convert_coords { id } {
    global mged_gui
    global mged_adc_control

    if {$mged_adc_control($id,coords) == $mged_adc_control($id,last_coords)} {
	return
    }

    winset $mged_gui($id,active_dm)

    switch $mged_adc_control($id,coords) {
	model {
	    if { $mged_adc_control($id,last_coords) == "grid" } {
		set mged_adc_control($id,pos) [eval format \"%.4f %.4f %.4f\"\
			[eval grid2model_lu $mged_adc_control($id,pos)]]
		set mged_adc_control($id,anchor_pt_dst) [eval format \"%.4f %.4f %.4f\"\
			[eval grid2model_lu $mged_adc_control($id,anchor_pt_dst)]]
		set mged_adc_control($id,anchor_pt_a1) [eval format \"%.4f %.4f %.4f\"\
			[eval grid2model_lu $mged_adc_control($id,anchor_pt_a1)]]
		set mged_adc_control($id,anchor_pt_a2) [eval format \"%.4f %.4f %.4f\"\
			[eval grid2model_lu $mged_adc_control($id,anchor_pt_a2)]]
	    }
	}
	grid {
	    if { $mged_adc_control($id,last_coords) == "model" } {
		set mged_adc_control($id,pos) [eval format \"%.4f %.4f\"\
			[eval model2grid_lu $mged_adc_control($id,pos)]]
		set mged_adc_control($id,anchor_pt_dst) [eval format \"%.4f %.4f\"\
			[eval model2grid_lu $mged_adc_control($id,anchor_pt_dst)]]
		set mged_adc_control($id,anchor_pt_a1) [eval format \"%.4f %.4f\"\
			[eval model2grid_lu $mged_adc_control($id,anchor_pt_a1)]]
		set mged_adc_control($id,anchor_pt_a2) [eval format \"%.4f %.4f\"\
			[eval model2grid_lu $mged_adc_control($id,anchor_pt_a2)]]
	    }
	}
    }

    set mged_adc_control($id,last_coords) $mged_adc_control($id,coords)
}

proc adc_adjust_coords { id } {
    global mged_adc_control

    set top .$id.adc_control

    switch $mged_adc_control($id,coords) {
	model {
	    set mged_adc_control($id,coords_text) "Model Coords"
	    hoc_register_data $top.coordsMB "Coordinate System Type"\
		    { { summary "This is a menu of the two coordinate system
types - model and grid. The current coordinate
system type is model. This is the coordinate system
type that the MGED database lives in." } }

            set hoc_data { { summary "Indicates the angle distance cursor's position
in model coordinates (local units)." } }
            hoc_register_data $top.posL "ADC Position" $hoc_data
            hoc_register_data $top.posE "ADC Position" $hoc_data

            hoc_register_data $top.anchorL "Anchor Points"\
		    { { summary "Anchor points are currently used to \"anchor\"
certain ADC attributes to a point in model space.
For example, if the ADC position is anchored to the
model origin, the angle distance cursor will always be
drawn with its center at the model origin. The following
four attributes can be anchored: position, tick distance,
angle1 and angle2." } }

            set hoc_data { { summary "Indicates the angle distance cursor's position
in model coordinates (local units). If the
ADC position is anchored, the ADC will remain
positioned at the anchor point. So if the view
changes while the ADC position is anchored, the
ADC will move with respect to the view." } }
            hoc_register_data $top.anchor_xyzL "Position" $hoc_data
            hoc_register_data $top.anchor_xyzE "Position" $hoc_data

            set hoc_data { { summary "If anchoring, the tick is drawn at
a distance from the ADC center position
that is equal to the distance between the
ADC center position and the anchor point.
Note - the anchor point is currently specified
in model coordinates (local units)." } }
            hoc_register_data $top.anchor_tickL "Tick Distance Anchor Point" $hoc_data
            hoc_register_data $top.anchor_tickE "Tick Distance Ancor Point" $hoc_data

            set hoc_data { { summary "If anchoring is enabled, then angle 1 is always
drawn through its anchor point. Note - the
anchor point is currently specified in model
coordinates (local units)." } }
            hoc_register_data $top.anchor_a1L "Angle 1 Anchor Point" $hoc_data
            hoc_register_data $top.anchor_a1E "Angle 1 Anchor Point" $hoc_data

            set hoc_data { { summary "If anchoring is enabled, then angle 2 is always
drawn through its anchor point. Note - the
anchor point is currently specified in model
coordinates (local units)." } }
            hoc_register_data $top.anchor_a2L "Angle 2 Anchor Point" $hoc_data
            hoc_register_data $top.anchor_a2E "Angle 2 Anchor Point" $hoc_data
	}
	grid {
	    set mged_adc_control($id,coords_text) "Grid Coords"
	    hoc_register_data $top.coordsMB "Coordinate System Type"\
		    { { summary "This is a menu of the two coordinate system
types - model and grid. The current coordinate
system type is grid. This coordinate system is
2D and lives in the view plane. The origin of
this system is located by projecting the model
origin onto the view plane." } }

            set hoc_data { { summary "Indicates the angle distance cursor's position
in grid coordinates (local units)." } }
            hoc_register_data $top.posL "ADC Position" $hoc_data
            hoc_register_data $top.posE "ADC Position" $hoc_data

            hoc_register_data $top.anchorL "Anchor Points"\
		    { { summary "Anchor points are currently used to \"anchor\"
certain ADC attributes to a point in grid space.
For example, if the ADC position is anchored to the
grid origin, the angle distance cursor will always be
drawn with its center at the grid origin. The following
four attributes can be anchored: position, tick distance,
angle1 and angle2." } }

            set hoc_data { { summary "Indicates the angle distance cursor's position
in grid coordinates (local units)." } }
            hoc_register_data $top.anchor_xyzL "Position" $hoc_data
            hoc_register_data $top.anchor_xyzE "Position" $hoc_data

            set hoc_data { { summary "If anchoring is enabled, the tick is always
drawn at a distance from the ADC center
position that is equal to the distance between the
the ADC center position and the anchor point.
Note - the anchor point is currently specified
in grid coordinates (local units)." } }
            hoc_register_data $top.anchor_tickL "Tick Distance Anchor Point" $hoc_data
	    hoc_register_data $top.anchor_tickE "Tick Distance Ancor Point" $hoc_data

            set hoc_data { { summary "If anchoring is enabled, then angle 1 is always
drawn through its anchor point. Note - the
anchor point is currently specified in grid
coordinates (local units)." } }
            hoc_register_data $top.anchor_a1L "Angle 1 Anchor Point" $hoc_data
            hoc_register_data $top.anchor_a1E "Angle 1 Anchor Point" $hoc_data

            set hoc_data { { summary "If anchoring is enabled, then angle 2 is always
drawn through its anchor point. Note - the
anchor point is currently specified in grid
coordinates (local units)." } }
            hoc_register_data $top.anchor_a2L "Angle 2 Anchor Point" $hoc_data
            hoc_register_data $top.anchor_a2E "Angle 2 Anchor Point" $hoc_data
	}
    }

    if {$mged_adc_control($id,interpval) == "abs"} {
	convert_coords $id
    } else {
	adc_clear $id
    }
}

proc adc_interpval { id } {
    global mged_adc_control

    set top .$id.adc_control

    switch $mged_adc_control($id,interpval) {
	abs {
	    $top.loadB configure -text "Load"\
		    -command "adc_load $id"
	    set mged_adc_control($id,interpval_text) "Absolute"
	    adc_load $id

	    hoc_register_data $top.loadB "Load"\
		    { { summary "Load the ADC control panel with
values from the angle distance cursor." } }
	}
	rel {
	    $top.loadB configure -text "Clear"\
		    -command "adc_clear $id"
	    set mged_adc_control($id,interpval_text) "Relative"
	    adc_clear $id

	    hoc_register_data $top.loadB "Clear"\
		    { { summary "Clear all relative values to zero." } }
	}
    }
}

proc adc_clear { id } {
    global mged_adc_control

    if {$mged_adc_control($id,coords) == "grid"} {
	set mged_adc_control($id,pos) "0.0 0.0"
	set mged_adc_control($id,anchor_pt_dst) "0.0 0.0"
	set mged_adc_control($id,anchor_pt_a1) "0.0 0.0"
	set mged_adc_control($id,anchor_pt_a2) "0.0 0.0"
    } else {
	set mged_adc_control($id,pos) "0.0 0.0 0.0"
	set mged_adc_control($id,anchor_pt_dst) "0.0 0.0 0.0"
	set mged_adc_control($id,anchor_pt_a1) "0.0 0.0 0.0"
	set mged_adc_control($id,anchor_pt_a2) "0.0 0.0 0.0"
    }

    set mged_adc_control($id,a1) "0.0"
    set mged_adc_control($id,a2) "0.0"
    set mged_adc_control($id,dst) "0.0"

    set mged_adc_control($id,anchor_pos) 0
    set mged_adc_control($id,anchor_a1) 0
    set mged_adc_control($id,anchor_a2) 0
    set mged_adc_control($id,anchor_dst) 0
}

proc adc_CBHandler { id } {
    global mged_gui
    global ::tk::Priv

    if {[opendb] == ""} {
	set mged_gui($id,adc_draw) 0
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    mged_apply $id "adc draw $mged_gui($id,adc_draw)"
}
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
