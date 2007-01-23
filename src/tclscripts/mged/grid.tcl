#                        G R I D . T C L
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
##
#                       G R I D . T C L
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
#	Control Panel for MGED's grid.
#

proc do_grid_spacing { id spacing_type } {
    global mged_gui
    global grid_control_spacing
    global localunit
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.grid_spacing

    if [winfo exists $top] {
	raise $top
	return
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF1 -relief groove -bd 2
    frame $top.gridF2

    if {$spacing_type == "h"} {
	label $top.resL -text "Horiz." -anchor w
	entry $top.resE -relief sunken -width 12 -textvar grid_control_spacing($id,tick)
	entry $top.maj_resE -relief sunken -width 12 -textvar grid_control_spacing($id,ticksPerMajor)
    } elseif {$spacing_type == "v"} {
	label $top.resL -text "Vert." -anchor w
	entry $top.resE -relief sunken -width 12 -textvar grid_control_spacing($id,tick)
	entry $top.maj_resE -relief sunken -width 12 -textvar grid_control_spacing($id,ticksPerMajor)
    } elseif {$spacing_type == "b"} {
	label $top.resL -text "Horiz. & Vert." -anchor w
	hoc_register_data $top.resL "Horiz. & Vert."\
		{ { summary "The tick spacing and major spacing are set
for both horizontal and vertical directions." } }

	set hoc_data { { summary "Tick spacing, here, is the distance between each
tick in both the horizontal and vertical directions." } }
	label $top.tickSpacingL -text "Tick Spacing\n($localunit/tick)"
	hoc_register_data $top.tickSpacingL "Tick Spacing" $hoc_data
	entry $top.resE -relief sunken -width 12 -textvar grid_control_spacing($id,tick)
	hoc_register_data $top.resE "Tick Spacing" $hoc_data

        set hoc_data { { summary "Major spacing is measured in ticks
and determines how often lines of
ticks are drawn." } }
        label $top.majorSpacingL -text "Major Spacing\n(ticks/major)"
        hoc_register_data $top.majorSpacingL "Major Spacing" $hoc_data
	entry $top.maj_resE -relief sunken -width 12 -textvar grid_control_spacing($id,ticksPerMajor)
	hoc_register_data $top.maj_resE "Major Spacing" $hoc_data
    } else {
	catch {destroy $top}
	return
    }

    button $top.okB -relief raised -text "OK"\
	    -command "grid_spacing_ok $id $spacing_type $top"
    hoc_register_data $top.okB "OK"\
	    { { summary "Apply the grid spacing settings
to the grid, then close the grid
spacing control panel." } }
    button $top.applyB -relief raised -text "Apply"\
	    -command "grid_spacing_apply $id $spacing_type"
    hoc_register_data $top.applyB "Apply"\
	    { { summary "Apply the grid spacing settings
to the grid." } }
    button $top.resetB -relief raised -text "Reset"\
	    -command "grid_spacing_reset $id $spacing_type"
    hoc_register_data $top.resetB "Reset"\
	    { { summary "Reset the control panel from the grid." } }
    button $top.autosizeB -relief raised -text "Autosize"\
	    -command "grid_spacing_autosize $id"
    hoc_register_data $top.autosizeB "Autosize"\
	    { { summary "Set the grid spacing according to the view
size. The number of ticks will be between 20
and 200. The tick spacing will be a power of
10 in local units." } }
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss/close the grid spacing control panel." } }

    grid x $top.tickSpacingL x $top.majorSpacingL -in $top.gridF1 -padx 8 -pady 8
    grid $top.resL $top.resE x $top.maj_resE -sticky "ew" -in $top.gridF1 -padx 8 -pady 8

    grid columnconfigure $top.gridF1 1 -weight 1
    grid columnconfigure $top.gridF1 3 -weight 1

    grid $top.okB $top.applyB x $top.resetB $top.autosizeB x $top.dismissB -sticky "ew" -in $top.gridF2
    grid columnconfigure $top.gridF2 2 -weight 1
    grid columnconfigure $top.gridF2 2 -minsize 10
    grid columnconfigure $top.gridF2 5 -weight 1
    grid columnconfigure $top.gridF2 5 -minsize 10

    grid $top.gridF1 -sticky "ew" -padx 8 -pady 8
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1

    grid_spacing_reset $id $spacing_type

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "Grid Spacing ($id)"
}

proc do_grid_anchor { id } {
    global mged_gui
    global grid_control_anchor
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.grid_anchor

    if [winfo exists $top] {
	raise $top

	return
    }

    # Initialize variables
    winset $mged_gui($id,active_dm)
    set grid_control_anchor($id) [_mged_rset grid anchor]

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF1 -relief groove -bd 2
    frame $top.gridF2

    frame $top.anchorF

    set hoc_data { { summary "The grid anchor point is a point such that
when the grid is drawn, one of its points
must be located exactly at the anchor point.
The anchor point is specified using model
coordinates and local units. The anchor point
and tick spacings work together to give the
user accurate information about where things
are in the view as well as a high degree of
accuracy when snapping." } }
    label $top.anchorL -text "Anchor Point" -anchor e
    hoc_register_data $top.anchorL "Anchor Point" $hoc_data
    entry $top.anchorE -relief sunken -bd 2 -width 12 -textvar grid_control_anchor($id)
    hoc_register_data $top.anchorE "Anchor Point" $hoc_data

    button $top.okB -relief raised -text "OK"\
	    -command "mged_apply $id \"rset grid anchor \\\$grid_control_anchor($id)\";
                      catch { destroy $top }"
    hoc_register_data $top.okB "OK"\
	    { { summary "Apply the grid anchor control panel
settings to the grid, then close the
control panel." } }
    button $top.applyB -relief raised -text "Apply"\
	    -command "mged_apply $id \"rset grid anchor \\\$grid_control_anchor($id)\""
    hoc_register_data $top.applyB "Apply"\
	    { { summary "Apply the grid anchor control panel
settings to the grid." } }
    button $top.resetB -relief raised -text "Reset"\
	    -command "winset \$mged_gui($id,active_dm);\
	    set grid_control_anchor($id) \[rset grid anchor\]"
    hoc_register_data $top.resetB "Reset"\
	    { { summary "Reset the control panel from the grid." } }
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss/close the grid anchor control panel." } }

    grid $top.anchorL $top.anchorE -sticky "ew" -in $top.anchorF
    grid columnconfigure $top.anchorF 1 -weight 1

    grid $top.anchorF -sticky "ew" -in $top.gridF1 -padx 8 -pady 8
    grid columnconfigure $top.gridF1 0 -weight 1

    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky "ew" -in $top.gridF2
    grid columnconfigure $top.gridF2 2 -weight 1
    grid columnconfigure $top.gridF2 4 -weight 3

    grid $top.gridF1 -sticky "ew" -padx 8 -pady 8
    grid $top.gridF2 -sticky "ew" -padx 8 -pady 8

    grid columnconfigure $top 0 -weight 1

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "Grid Anchor Point ($id)"
}

proc init_grid_control { id } {
    global mged_gui
    global mged_default
    global grid_control
    global localunit
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set top .$id.grid_control

    if [winfo exists $top] {
	raise $top

	return
    }

    if ![info exists grid_control($id,rh)] {
	set init_grid_vars 1
    } else {
	set init_grid_vars 0
    }

    set grid_control($id,padx) 4
    set grid_control($id,pady) 4

    toplevel $top -screen $mged_gui($id,screen) -menu $top.menubar

    menu $top.menubar -tearoff $mged_default(tearoff_menus)
    $top.menubar add cascade -label "Apply To" -underline 0\
	    -menu $top.menubar.applyTo
    menu $top.menubar.applyTo -title "Apply To"\
	    -tearoff $mged_default(tearoff_menus)
    # The help on context for the applyTo menu was already defined in openw.tcl
    $top.menubar.applyTo add radiobutton -value 0\
	    -variable mged_gui($id,apply_to)\
	    -label "Active Pane" -underline 0
    $top.menubar.applyTo add radiobutton -value 1\
	    -variable mged_gui($id,apply_to)\
	    -label "Local Panes" -underline 0
    $top.menubar.applyTo add radiobutton -value 2\
	    -variable mged_gui($id,apply_to)\
	    -label "Listed Panes" -underline 1
    $top.menubar.applyTo add radiobutton -value 3\
	    -variable mged_gui($id,apply_to)\
	    -label "All Panes" -underline 4

    frame $top.gridF1
    frame $top.gridFF1 -relief groove -bd 2
    frame $top.gridF2
    frame $top.gridFF2 -relief groove -bd 2
    frame $top.gridF3
    frame $top.gridFF3 -relief groove -bd 2
    frame $top.gridF4

    frame $top.hF -relief sunken -bd 2
    frame $top.maj_hF -relief sunken -bd 2
    frame $top.vF -relief sunken -bd 2
    frame $top.maj_vF -relief sunken -bd 2

    frame $top.anchorF

    label $top.tickSpacingL -text "Tick Spacing\n($localunit/tick)"
    hoc_register_data $top.tickSpacingL "Tick Spacing"\
	    { { summary "Tick spacing is the distance between each tick
in either the horizontal or vertical direction." } }
    label $top.majorSpacingL -text "Major Spacing\n(ticks/major)"
    hoc_register_data $top.majorSpacingL "Major Spacing"\
	    { { summary "Major spacing is measured in ticks and determines
how often lines of ticks are drawn." } }

    set hoc_data { { summary "This row is for horizontal tick spacing
and horizontal major spacing. These two
attributes help determine how the grid is
drawn and how snapping is performed." } }
    label $top.hL -text "Horiz." -anchor w
    hoc_register_data $top.hL "Horizontal Spacing" $hoc_data
    entry $top.hE -relief flat -width 12 -textvar grid_control($id,rh)
    hoc_register_data $top.hE "Horizontal Tick Spacing" $hoc_data
    menubutton $top.hMB -relief raised -bd 2\
	    -menu $top.hMB.spacing -indicatoron 1
    hoc_register_data $top.hMB "Horizontal Tick Spacings"\
	    { { summary "Pops up a menu of distances to choose
from for horizontal tick spacing." } }
    menu $top.hMB.spacing -title "Grid Spacing" -tearoff 0
    $top.hMB.spacing add command -label "Micrometer" -underline 4\
	    -command "set_grid_spacing_htick $id micrometer"
    $top.hMB.spacing add command -label "Millimeter" -underline 2\
	    -command "set_grid_spacing_htick $id millimeter"
    $top.hMB.spacing add command -label "Centimeter" -underline 0\
	    -command "set_grid_spacing_htick $id centimeter"
    $top.hMB.spacing add command -label "Decimeter" -underline 0\
	    -command "set_grid_spacing_htick $id decimeter"
    $top.hMB.spacing add command -label "Meter" -underline 0\
	    -command "set_grid_spacing_htick $id meter"
    $top.hMB.spacing add command -label "Kilometer" -underline 0\
	    -command "set_grid_spacing_htick $id kilometer"
    $top.hMB.spacing add separator
    $top.hMB.spacing add command -label "1/10 Inch" -underline 0\
	    -command "set_grid_spacing_htick $id \"1/10 inch\""
    $top.hMB.spacing add command -label "1/4 Inch" -underline 2\
	    -command "set_grid_spacing_htick $id \"1/4 inch\""
    $top.hMB.spacing add command -label "1/2 Inch" -underline 2\
	    -command "set_grid_spacing_htick $id \"1/2 inch\""
    $top.hMB.spacing add command -label "Inch" -underline 0\
	    -command "set_grid_spacing_htick $id inch"
    $top.hMB.spacing add command -label "Foot" -underline 0\
	    -command "set_grid_spacing_htick $id foot"
    $top.hMB.spacing add command -label "Yard" -underline 0\
	    -command "set_grid_spacing_htick $id yard"
    $top.hMB.spacing add command -label "Mile" -underline 3\
	    -command "set_grid_spacing_htick $id mile"
    entry $top.maj_hE -relief flat -width 12 -textvar grid_control($id,mrh)
    hoc_register_data $top.maj_hE "Horizontal Major Spacing"\
	    { { summary "Enter horizontal major spacing here." } }

    set hoc_data { { summary "This row is for vertical tick spacing
and vertical major spacing. These two
attributes help determine how the grid
is drawn and how snapping is performed." } }
    label $top.vL -text "Vert." -anchor w
    hoc_register_data $top.vL "Vertical Spacing" $hoc_data
    entry $top.vE -relief flat -width 12 -textvar grid_control($id,rv)
    hoc_register_data $top.vE "Vertical Tick Spacing" $hoc_data
    menubutton $top.vMB -relief raised -bd 2\
	    -menu $top.vMB.spacing -indicatoron 1
    hoc_register_data $top.vMB "Vertical Tick Spacings"\
	    { { summary "Pops up a menu of distances to choose from for
vertical tick spacing." } }
    menu $top.vMB.spacing -title "Grid Spacing" -tearoff 0
    $top.vMB.spacing add command -label "Micrometer" -underline 4\
	    -command "set_grid_spacing_vtick $id micrometer"
    $top.vMB.spacing add command -label "Millimeter" -underline 2\
	    -command "set_grid_spacing_vtick $id millimeter"
    $top.vMB.spacing add command -label "Centimeter" -underline 0\
	    -command "set_grid_spacing_vtick $id centimeter"
    $top.vMB.spacing add command -label "Decimeter" -underline 0\
	    -command "set_grid_spacing_vtick $id decimeter"
    $top.vMB.spacing add command -label "Meter" -underline 0\
	    -command "set_grid_spacing_vtick $id meter"
    $top.vMB.spacing add command -label "Kilometer" -underline 0\
	    -command "set_grid_spacing_vtick $id kilometer"
    $top.vMB.spacing add separator
    $top.vMB.spacing add command -label "1/10 Inch" -underline 0\
	    -command "set_grid_spacing_vtick $id \"1/10 inch\""
    $top.vMB.spacing add command -label "1/4 Inch" -underline 2\
	    -command "set_grid_spacing_vtick $id \"1/4 inch\""
    $top.vMB.spacing add command -label "1/2 Inch" -underline 2\
	    -command "set_grid_spacing_vtick $id \"1/2 inch\""
    $top.vMB.spacing add command -label "Inch" -underline 0\
	    -command "set_grid_spacing_vtick $id inch"
    $top.vMB.spacing add command -label "Foot" -underline 0\
	    -command "set_grid_spacing_vtick $id foot"
    $top.vMB.spacing add command -label "Yard" -underline 0\
	    -command "set_grid_spacing_vtick $id yard"
    $top.vMB.spacing add command -label "Mile" -underline 3\
	    -command "set_grid_spacing_vtick $id mile"
    entry $top.maj_vE -relief flat -width 12 -textvar grid_control($id,mrv)
    hoc_register_data $top.maj_vE "Vertical Major Spacing"\
	    { { summary "Enter vertical major spacing here." } }

    checkbutton $top.squareGridCB -relief flat -text "Square Grid"\
	    -offvalue 0 -onvalue 1 -variable grid_control($id,square)\
	    -command "set_grid_square $id"
    hoc_register_data $top.squareGridCB "Square Grid"\
	    { { synopsis "Toggle square grid mode." }
              { description "In square grid mode the horizontal and
vertical attributes are the same. For
example, if the horizontal tick spacing
is 12 inches, then the vertical tick spacing
is 12 inches. And if the horizontal major
spacing is 10 ticks, then the vertical major
spacing is 10 ticks." } }

    set hoc_data { { summary "The grid anchor point is a point such that
when the grid is drawn, one of its points
must be located exactly at the anchor point.
The anchor point is specified using model
coordinates and local units. The anchor point
and tick spacings work together to give the
user accurate information about where things
are in the view as well as a high degree of
accuracy when snapping." } }
    label $top.anchorL -text "Anchor Point" -anchor e
    hoc_register_data $top.anchorL "Grid Anchor Point" $hoc_data
    entry $top.anchorE -relief sunken -bd 2 -width 12 -textvar grid_control($id,anchor)
    hoc_register_data $top.anchorE "Grid Anchor Point" $hoc_data

    label $top.gridEffectsL -text "Grid Effects" -anchor w
    hoc_register_data $top.gridEffectsL "Grid Effects"\
	    { { summary "The grid can be drawn on the screen and
it can be used for snapping. Note - the
grid exists whether it is drawn or not." } }

    checkbutton $top.drawCB -relief flat -text "Draw"\
	    -offvalue 0 -onvalue 1 -variable grid_control($id,draw)
    hoc_register_data $top.drawCB "Draw Grid"\
	    { { synopsis "Toggle drawing the grid." }
              { description "The grid is a lattice of points over the pane
(geometry window). The regular spacing between
the points gives the user accurate visual cues
regarding dimension. This spacing can be set by
the user." }
            { see_also "rset" } }

    checkbutton $top.snapCB -relief flat -text "Snap"\
	    -offvalue 0 -onvalue 1 -variable grid_control($id,snap)
    hoc_register_data $top.snapCB "Snap To Grid"\
	    { { synopsis "Toggle grid snapping." }
              { description "When snapping to grid, the internal routines
that use the mouse pointer location, move/snap
that location to the nearest grid point. This
gives the user high accuracy with the mouse for
transforming the view or editing solids/matrices." }
            { see_also "rset" } }

    button $top.okB -relief raised -text "OK"\
	    -command "grid_control_ok $id $top"
    hoc_register_data $top.okB "OK"\
	    { { summary "Apply grid control panel settings to
the grid, then close the control panel." } }
    button $top.applyB -relief raised -text "Apply"\
	    -command "grid_control_apply $id"
    hoc_register_data $top.applyB "Apply"\
	    { { summary "Apply grid control panel settings to the grid." } }
    button $top.resetB -relief raised -text "Reset"\
	    -command "grid_control_reset $id"
    hoc_register_data $top.resetB "Reset"\
	    { { summary "Reset the control panel from the grid." } }
    button $top.autosizeB -relief raised -text "Autosize"\
	    -command "grid_control_autosize $id"
    hoc_register_data $top.autosizeB "Autosize"\
	    { { summary "Set the grid spacing according to the view
size. The number of ticks will be between 20 and 200.
The tick spacing will be a power of 10 in local units." } }
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Dismiss/close the grid control panel." } }

    grid x $top.tickSpacingL x $top.majorSpacingL -in $top.gridFF1 \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid $top.hE $top.hMB -sticky nsew -in $top.hF
    grid columnconfigure $top.hF 0 -weight 1
    grid rowconfigure $top.hF 0 -weight 1
    grid $top.maj_hE -sticky nsew -in $top.maj_hF
    grid columnconfigure $top.maj_hF 0 -weight 1
    grid rowconfigure $top.maj_hF 0 -weight 1
    grid $top.hL $top.hF x $top.maj_hF -sticky nsew -in $top.gridFF1 \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid $top.vE $top.vMB -sticky nsew -in $top.vF
    grid columnconfigure $top.vF 0 -weight 1
    grid rowconfigure $top.vF 0 -weight 1
    grid $top.maj_vE -sticky nsew -in $top.maj_vF
    grid columnconfigure $top.maj_vF 0 -weight 1
    grid rowconfigure $top.maj_vF 0 -weight 1
    grid $top.vL $top.vF x $top.maj_vF -sticky nsew -in $top.gridFF1 \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid $top.squareGridCB - - - -in $top.gridFF1 \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid columnconfigure $top.gridFF1 1 -weight 1
    grid columnconfigure $top.gridFF1 3 -weight 1
    grid rowconfigure $top.gridFF1 1 -weight 1
    grid rowconfigure $top.gridFF1 2 -weight 1
    grid $top.gridFF1 -sticky nsew -in $top.gridF1 \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid columnconfigure $top.gridF1 0 -weight 1
    grid rowconfigure $top.gridF1 0 -weight 1

    grid $top.anchorL $top.anchorE -sticky nsew -in $top.anchorF
    grid columnconfigure $top.anchorF 1 -weight 1
    grid rowconfigure $top.anchorF 0 -weight 1
    grid $top.anchorF -sticky nsew -in $top.gridFF2 \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid columnconfigure $top.gridFF2 0 -weight 1
    grid rowconfigure $top.gridFF2 0 -weight 1
    grid $top.gridFF2 -sticky nsew -in $top.gridF2 -padx $grid_control($id,padx)
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 0 -weight 1

    grid $top.gridEffectsL x $top.drawCB x $top.snapCB x -sticky "ew" -in $top.gridFF3\
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid columnconfigure $top.gridFF3 0 -weight 0
    grid columnconfigure $top.gridFF3 1 -weight 1
    grid columnconfigure $top.gridFF3 3 -minsize 20
    grid columnconfigure $top.gridFF3 5 -weight 1
    grid $top.gridFF3 -sticky "ew" -in $top.gridF3 \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid columnconfigure $top.gridF3 0 -weight 1

    grid $top.okB $top.applyB x $top.resetB $top.autosizeB x $top.dismissB -sticky "ew" -in $top.gridF4
    grid columnconfigure $top.gridF4 2 -weight 1
    grid columnconfigure $top.gridF4 5 -weight 1

    grid $top.gridF1 -sticky nsew \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid $top.gridF2 -sticky nsew \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid $top.gridF3 -sticky "ew" \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid $top.gridF4 -sticky "ew" \
	    -padx $grid_control($id,padx) -pady $grid_control($id,pady)
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 2
    grid rowconfigure $top 1 -weight 1

    if {$init_grid_vars} {
	grid_control_reset $id
	grid_control_autosize $id
	set grid_control($id,square) 1
    } else {
	grid_control_reset $id
    }
    set_grid_square $id

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm title $top "Grid Control Panel ($id)"
}

proc grid_control_ok { id top } {
    grid_control_apply $id
    catch { destroy $top }
}

proc grid_control_apply { id } {
    global grid_control
    global mged_gui

    if {$grid_control($id,square)} {
	mged_apply $id "rset grid anchor $grid_control($id,anchor);\
		rset grid rh $grid_control($id,rh);\
		rset grid mrh $grid_control($id,mrh);\
		rset grid rv $grid_control($id,rh);\
		rset grid mrv $grid_control($id,mrh);\
		rset grid snap $grid_control($id,snap);\
		rset grid draw $grid_control($id,draw)"

	set grid_control($id,rv) $grid_control($id,rh)
	set grid_control($id,mrv) $grid_control($id,mrh)
    } else {
	mged_apply $id "rset grid anchor $grid_control($id,anchor);\
		rset grid rh $grid_control($id,rh);\
		rset grid mrh $grid_control($id,mrh);\
		rset grid rv $grid_control($id,rv);\
		rset grid mrv $grid_control($id,mrv);\
		rset grid snap $grid_control($id,snap);\
		rset grid draw $grid_control($id,draw)"
    }

    # update the main GUI
    set mged_gui($id,grid_draw) $grid_control($id,draw)
    set mged_gui($id,grid_snap) $grid_control($id,snap)
}

proc grid_control_reset { id } {
    global mged_gui
    global grid_control

    if ![winfo exists .$id.grid_control] {
	return
    }

    winset $mged_gui($id,active_dm)

    set grid_control($id,draw) [rset grid draw]
    set grid_control($id,snap) [rset grid snap]
    set grid_control($id,anchor) [rset grid anchor]
    set grid_control($id,rh) [eval format "%.5f" [rset grid rh]]
    set grid_control($id,mrh) [rset grid mrh]
    set grid_control($id,rv) [eval format "%.5f" [rset grid rv]]
    set grid_control($id,mrv) [rset grid mrv]

    if {$grid_control($id,rh) != $grid_control($id,rv) ||\
	$grid_control($id,mrh) != $grid_control($id,mrv)} {
	set grid_control($id,square) 0
	set_grid_square $id
    }

    set mged_gui($id,grid_draw) $grid_control($id,draw)
    set mged_gui($id,grid_snap) $grid_control($id,snap)
}

proc set_grid_square { id } {
    global grid_control

    set top .$id.grid_control
    if [winfo exists $top] {
	if {$grid_control($id,square)} {
	    $top.vE configure -textvar grid_control($id,rh)
	    $top.maj_vE configure -textvar grid_control($id,mrh)
	} else {
	    $top.vE configure -textvar grid_control($id,rv)
	    $top.maj_vE configure -textvar grid_control($id,mrv)
	}
    }
}

proc grid_control_update { sf } {
    global mged_players
    global grid_control
    global localunit

    foreach id $mged_players {
	if {[info exists grid_control($id,anchor)] &&\
		[llength $grid_control($id,anchor)] == 3} {
	    set x [lindex $grid_control($id,anchor) 0]
	    set y [lindex $grid_control($id,anchor) 1]
	    set z [lindex $grid_control($id,anchor) 2]

	    set x [expr $sf * $x]
	    set y [expr $sf * $y]
	    set z [expr $sf * $z]

	    set grid_control($id,anchor) "$x $y $z"
	}

	if [info exists grid_control($id,rh)] {
	    set grid_control($id,rh) [expr $sf * $grid_control($id,rh)]
	    set grid_control($id,rv) [expr $sf * $grid_control($id,rv)]
	}

	set top .$id.grid_control
	if [winfo exists $top] {
	    $top.tickSpacingL configure -text "Tick Spacing\n($localunit/tick)"
	}

	set top .$id.grid_spacing
	if [winfo exists $top] {
	    $top.tickSpacingL configure -text "Tick Spacing\n($localunit/tick)"
	}
    }
}

proc grid_autosize {} {
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

# Gives between 20 and 200 ticks in user units
    set lower [expr log10(20)]
    set upper [expr $lower+1]
    set s [expr log10([_mged_view size])]

    if {$s < $lower} {
	set val [expr pow(10, floor($s - $lower))]
    } elseif {$upper < $s} {
	set val [expr pow(10, ceil($s - $upper))]
    } else {
	set val 1.0
    }

    return $val
}

proc grid_spacing_autosize { id } {
    global mged_gui
    global grid_control_spacing
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    winset $mged_gui($id,active_dm)
    set val [grid_autosize]

    set grid_control_spacing($id,tick) $val
    set grid_control_spacing($id,ticksPerMajor) 10
}

proc grid_control_autosize { id } {
    global mged_gui
    global grid_control

    winset $mged_gui($id,active_dm)
    set val [grid_autosize]

    set grid_control($id,rh) $val
    set grid_control($id,rv) $val
    set grid_control($id,mrh) 10
    set grid_control($id,mrv) 10
}

proc grid_spacing_ok { id spacing_type top } {
    grid_spacing_apply $id $spacing_type
    catch { destroy $top }
}

proc grid_spacing_apply { id spacing_type } {
    global mged_gui
    global grid_control_spacing

    if {[opendb] == ""} {
	return
    }

    if {$spacing_type == "h"} {
	mged_apply $id "rset grid rh $grid_control_spacing($id,tick);\
		rset grid mrh $grid_control_spacing($id,ticksPerMajor)"
    } elseif {$spacing_type == "v"} {
	mged_apply $id "rset grid rv $grid_control_spacing($id,tick);\
		rset grid mrv $grid_control_spacing($id,ticksPerMajor)"
    } else {
	mged_apply $id "rset grid rh $grid_control_spacing($id,tick);\
		rset grid mrh $grid_control_spacing($id,ticksPerMajor);\
		rset grid rv $grid_control_spacing($id,tick);\
		rset grid mrv $grid_control_spacing($id,ticksPerMajor)"
    }
}

proc grid_spacing_reset { id spacing_type } {
    global mged_gui
    global grid_control_spacing

    winset $mged_gui($id,active_dm)

    if {$spacing_type == "v"} {
	set grid_control_spacing($id,tick) [eval format "%.5f" [rset grid rv]]
	set grid_control_spacing($id,ticksPerMajor) [eval format "%.5f" [rset grid mrv]]
    } else {
	set grid_control_spacing($id,tick) [eval format "%.5f" [rset grid rh]]
	set grid_control_spacing($id,ticksPerMajor) [eval format "%.5f" [rset grid mrh]]
    }
}

proc set_grid_spacing { id grid_unit apply } {
    global mged_gui
    global grid_control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    set_grid_res res res_major $grid_unit

    if {$apply} {
	mged_apply $id "rset grid rh $res; rset grid rv $res;\
		rset grid mrh $res_major; rset grid mrv $res_major"
    } else {
	set grid_control($id,rh) $res
	set grid_control($id,rv) $res
	set grid_control($id,mrh) $res_major
	set grid_control($id,mrv) $res_major
    }
}

proc set_grid_spacing_htick { id grid_unit } {
    global mged_gui
    global grid_control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    # set res according to grid_unit
    set_grid_res res res_major $grid_unit

    # set the horizontal tick resolution
    set grid_control($id,rh) $res

    if {$grid_control($id,square)} {
	set grid_control($id,rv) $res
    }
}

proc set_grid_spacing_vtick { id grid_unit } {
    global mged_gui
    global grid_control
    global ::tk::Priv

    if {[opendb] == ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
		"No database has been opened!" info 0 OK
	return
    }

    # set res according to grid_unit
    set_grid_res res res_major $grid_unit

    # set the vertical tick resolution
    set grid_control($id,rv) $res

    if {$grid_control($id,square)} {
	set grid_control($id,rh) $res
    }
}

proc set_grid_res { r rm grid_unit } {
    global base2local

    upvar $r res $rm res_major

    switch $grid_unit {
	micrometer {
	    set res [expr 0.001 * $base2local]
	    set res_major 10
	}
	millimeter {
	    set res $base2local
	    set res_major 10
	}
	centimeter {
	    set res [expr 10 * $base2local]
	    set res_major 10
	}
	decimeter {
	    set res [expr 100 * $base2local]
	    set res_major 10
	}
	meter {
	    set res [expr 1000 * $base2local]
	    set res_major 10
	}
	kilometer {
	    set res [expr 1000000 * $base2local]
	    set res_major 10
	}
	"1/10 inch" {
	    set res [expr 2.54 * $base2local]
	    set res_major 10
	}
	"1/4 inch" {
	    set res [expr 6.35 * $base2local]
	    set res_major 4
	}
	"1/2 inch" {
	    set res [expr 12.7 * $base2local]
	    set res_major 2
	}
	inch {
	    set res [expr 25.4 * $base2local]
	    set res_major 12
	}
	foot {
	    set res [expr 304.8 * $base2local]
	    set res_major 10
	}
	yard {
	    set res [expr 914.4 * $base2local]
	    set res_major 10
	}
	mile {
	    set res [expr 1609344 * $base2local]
	    set res_major 10
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
