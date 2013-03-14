#                   L O D D I A L O G . T C L
# BRL-CAD
#
# Copyright (c) 2013 United States Government as represented by
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

package require Tk
package require Itcl
package require Itk

::itcl::class LODDialog {
    inherit ::itk::Widget

    constructor {_lodon args} {}

    public {
	variable lodon 0
	variable pointsScale 1.0
	variable curvesScale 1.0

	method disableLODWidgets {}
	method updatePointsValue {newVal}
	method updateCurvesValue {newVal}
    }
}

::itcl::body LODDialog::constructor {_lodon args} {
    eval itk_initialize $args

    set lodon $_lodon

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
	    -from 0.0 \
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
	    -from 1.0 \
	    -to 10.0 \
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
	    -text "Redraw Wireframes"
    } {}

    itk_component add zoomUpdateCheckbutton {
	ttk::checkbutton $itk_component(lodFrame).zoomUpdateCheckbutton \
	    -text "Redraw Wireframes After Every Zoom"
    } {}

    disableLODWidgets

# DISPLAY WIDGETS
    pack $itk_component(lodFrame) -expand true -fill both

    grid $itk_component(lodFrame).lodonCheckbutton - \
	-sticky nw -padx 3 -pady 3
    grid $itk_component(lodFrame).separator1        - \
	-sticky ew -padx 6 -pady 6
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
    grid $itk_component(lodFrame).updateButton     - \
	-sticky sw -padx 3 -pady 3
    grid $itk_component(lodFrame).zoomUpdateCheckbutton \
	-sticky w -padx 3 -pady 3

    grid configure $itk_component(lodFrame).pointsScale -sticky ew
    grid configure $itk_component(lodFrame).curvesScale -sticky ew
    grid columnconfigure $itk_component(lodFrame) 0 -weight 1
    grid rowconfigure $itk_component(lodFrame) 7 -weight 1
}

::itcl::body LODDialog::disableLODWidgets {} {
    if {$lodon} {
	$itk_component(lodFrame).pointsLabel state !disabled
	$itk_component(lodFrame).pointsScale state !disabled
	$itk_component(lodFrame).pointsLabel state !disabled
	$itk_component(lodFrame).curvesLabel state !disabled
	$itk_component(lodFrame).curvesScale state !disabled
	$itk_component(lodFrame).curvesLabel state !disabled
	$itk_component(lodFrame).updateButton state !disabled
	$itk_component(lodFrame).zoomUpdateCheckbutton state !disabled
    } else {
	$itk_component(lodFrame).pointsLabel state disabled
	$itk_component(lodFrame).pointsScale state disabled
	$itk_component(lodFrame).pointsLabel state disabled
	$itk_component(lodFrame).curvesLabel state disabled
	$itk_component(lodFrame).curvesScale state disabled
	$itk_component(lodFrame).curvesLabel state disabled
	$itk_component(lodFrame).updateButton state disabled
	$itk_component(lodFrame).zoomUpdateCheckbutton state disabled
    }
}

::itcl::body LODDialog::updatePointsValue {newVal} {
    set pointsScale [format %.1f $newVal]
}

::itcl::body LODDialog::updateCurvesValue {newVal} {
    set curvesScale [tcl::mathfunc::round $newVal]
}

