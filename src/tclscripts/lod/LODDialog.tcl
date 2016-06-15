#                   L O D D I A L O G . T C L
# BRL-CAD
#
# Copyright (c) 2013-2014 United States Government as represented by
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
# Group of widgets for configuring Level of Detail drawing.
#
# Usage: LODDialog <instance name> <lodon> [-cmdprefix <Archer instance name>]
#
# If lodon is true, the "use lod" checkbutton will be pre selected.
#
# The -cmdprefix option is used when this class is instanced in Archer,
# where the ged commands are methods of the Archer mega-widget instance.
#

package require Tk
package require Itcl
package require Itk

::itcl::class LODDialog {
    inherit ::itk::Widget

    constructor {args} {}

    public {
	variable lodon 0
	variable redrawOnZoom 0
	variable liveUpdate 0
	variable pointsScale 1.0
	variable curvesScale 1

	method disableLODWidgets {}
	method updatePointsValue {newVal}
	method updateCurvesValue {newVal}
	method redrawOnZoom {}
	method redraw {}
    }

    private {
	variable gedcmd ""
	method lodcmd {args}
    }

    itk_option define -cmdprefix cmdprefix CmdPrefix "" {}
}

::itcl::body LODDialog::constructor {args} {
    eval itk_initialize $args

    if {$itk_option(-cmdprefix) != ""} {
	set gedcmd "$itk_option(-cmdprefix) gedCmd"
    }

    set lodon [lodcmd enabled]

    set pointsScale [format "%.1f" [lodcmd scale points]]
    set curvesScale [format "%.0f" [lodcmd scale curves]]

    set pointsScale 1.0
    set curvesScale 1

    if {[lodcmd redraw] == "onzoom"} {
	set redrawOnZoom 1
    }

    # CREATE WIDGETS
    itk_component add lodFrame {
	ttk::frame $itk_interior.lodFrame \
	    -padding 5
    } {}

    itk_component add lodonCheckbutton {
	ttk::checkbutton $itk_component(lodFrame).lodonCheckbutton \
	    -text "Use LOD Wireframes" \
	    -command "$this disableLODWidgets" \
	    -variable [::itcl::scope lodon]
    } {}

    itk_component add separator1 {
	ttk::separator $itk_component(lodFrame).separator1
    } {}

    itk_component add pointsLabel {
	ttk::label $itk_component(lodFrame).pointsLabel \
	    -text "Scale Number of Points"
    } {}

    itk_component add pointsScale {
	ttk::scale $itk_component(lodFrame).pointsScale \
	    -from 0.1 \
	    -to 1.5 \
	    -value $pointsScale \
	    -command "$this updatePointsValue"
    } {}

    itk_component add pointsValueLabel {
	ttk::label $itk_component(lodFrame).pointsValueLabel \
	    -textvariable [::itcl::scope pointsScale] \
	    -width 5
    } {}

    itk_component add spacerFrame1 {
	ttk::frame $itk_component(lodFrame).spacerFrame1 \
	-height 10
    } {}

    itk_component add curvesLabel {
	ttk::label $itk_component(lodFrame).curvesLabel \
	    -text "Scale Number of Curves"
    } {}

    itk_component add curvesScale {
	ttk::scale $itk_component(lodFrame).curvesScale \
	    -from 1 \
	    -to 50 \
	    -value $curvesScale \
	    -command "$this updateCurvesValue"
    } {}

    itk_component add curvesValueLabel {
	ttk::label $itk_component(lodFrame).curvesValueLabel \
	    -textvariable [::itcl::scope curvesScale] \
	    -width 5
    } {}

    itk_component add spacerFrame2 {
	ttk::frame $itk_component(lodFrame).spacerFrame2 \
	    -height 30
    } {}

    itk_component add updateButton {
	ttk::button $itk_component(lodFrame).updateButton \
	    -text "Update Wireframes" \
	    -command "$this redraw"
    } {}

    itk_component add liveUpdateCheckbutton {
	ttk::checkbutton $itk_component(lodFrame).liveUpdateCheckbutton \
	    -text "Live Update" \
	    -variable [::itcl::scope liveUpdate]
    } {}

    itk_component add zoomUpdateCheckbutton {
	ttk::checkbutton $itk_component(lodFrame).zoomUpdateCheckbutton \
	    -text "Redraw Wireframes After Every Zoom" \
	    -variable [::itcl::scope redrawOnZoom] \
	    -command "$this redrawOnZoom"
    } {}

    disableLODWidgets

# DISPLAY WIDGETS
    pack $itk_component(lodFrame) -expand true -fill both

    grid $itk_component(lodFrame).lodonCheckbutton - \
	-sticky nw -padx 3 -pady 3
    grid $itk_component(lodFrame).separator1        - \
	-sticky ew -padx 6 -pady 6
    grid $itk_component(lodFrame).updateButton     - \
	-sticky sw -padx 3 -pady 3
    grid $itk_component(lodFrame).pointsLabel      - \
	-sticky w -padx 3 -pady 3
    grid $itk_component(lodFrame).pointsScale      $itk_component(lodFrame).pointsValueLabel \
	-sticky w -padx 3 -pady 3
    grid $itk_component(lodFrame).spacerFrame1     - \
	-stick nesw
    grid $itk_component(lodFrame).curvesLabel      - \
	-sticky w -padx 3 -pady 3
    grid $itk_component(lodFrame).curvesScale      $itk_component(lodFrame).curvesValueLabel \
	-sticky e -padx 3 -pady 3
    grid $itk_component(lodFrame).spacerFrame2     - \
	-stick nesw
    grid $itk_component(lodFrame).liveUpdateCheckbutton \
	-sticky w -padx 3 -pady 3
    grid $itk_component(lodFrame).zoomUpdateCheckbutton \
	-sticky w -padx 3 -pady 3

    grid configure $itk_component(lodFrame).pointsScale -sticky ew
    grid configure $itk_component(lodFrame).curvesScale -sticky ew
    grid columnconfigure $itk_component(lodFrame) 0 -weight 1
    grid rowconfigure $itk_component(lodFrame) 7 -weight 1
}

::itcl::body LODDialog::lodcmd {args} {
    eval $gedcmd lod $args
}

::itcl::body LODDialog::disableLODWidgets {} {
    if {$lodon} {
	$itk_component(lodFrame).pointsLabel state !disabled
	$itk_component(lodFrame).pointsScale state !disabled
	$itk_component(lodFrame).pointsLabel state !disabled
	$itk_component(lodFrame).curvesLabel state !disabled
	$itk_component(lodFrame).curvesScale state !disabled
	$itk_component(lodFrame).curvesLabel state !disabled
	$itk_component(lodFrame).liveUpdateCheckbutton state !disabled
	$itk_component(lodFrame).zoomUpdateCheckbutton state !disabled
	lodcmd on
    } else {
	$itk_component(lodFrame).pointsLabel state disabled
	$itk_component(lodFrame).pointsScale state disabled
	$itk_component(lodFrame).pointsLabel state disabled
	$itk_component(lodFrame).curvesLabel state disabled
	$itk_component(lodFrame).curvesScale state disabled
	$itk_component(lodFrame).curvesLabel state disabled
	$itk_component(lodFrame).liveUpdateCheckbutton state disabled
	$itk_component(lodFrame).zoomUpdateCheckbutton state disabled
	lodcmd off
    }
}

::itcl::body LODDialog::updatePointsValue {newVal} {
    set pointsScale [format %.1f $newVal]
    lodcmd scale points $pointsScale

    if {$liveUpdate} {
	redraw
    }
}

::itcl::body LODDialog::updateCurvesValue {newVal} {
    set curvesScale [tcl::mathfunc::round $newVal]
    lodcmd scale curves $curvesScale

    if {$liveUpdate} {
	redraw
    }
}

::itcl::body LODDialog::redrawOnZoom {} {
    if {$redrawOnZoom} {
	lodcmd redraw onzoom
    } else {
	lodcmd redraw off
    }
}

::itcl::body LODDialog::redraw {} {
    foreach obj [eval $gedcmd who] {
	eval $gedcmd draw $obj
    }
}
