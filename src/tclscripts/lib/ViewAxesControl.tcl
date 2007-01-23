#             V I E W A X E S C O N T R O L . T C L
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
# Author -
#	 Bob Parker
#
# Source -
#	 Survice Engineering Co. (www.survice.com)
#
# Description -
#        This is a control panel for setting an Mged widget's
#        view axes attributes.
#

::itk::usual ViewAxesControl {
}

::itcl::class ViewAxesControl {
    inherit ::itk::Toplevel

    itk_option define -mged mged Mged ""

    constructor {args} {}
    destructor {}

    protected variable axesSize ""
    protected variable axesPosition ""
    protected variable axesLineWidth 1
    protected variable axesColor "White"
    protected variable axesLabelColor "Yellow"

    protected method packIt {}
    protected method buildSize {}
    protected method buildPosition {}
    protected method buildLineWidth {}
    protected method buildAxesColor {}
    protected method buildAxesLabelColor {}
    protected method buildButtons {}

    protected method setSizeCB {size}
    protected method setPositionCB {pos}
    protected method setLineWidthCB {lw}
    protected method setAxesColorCB {color}
    protected method setAxesLabelColorCB {color}
    protected method updateControlPanel {}

    public method hide {}
    public method show {}
}

::itcl::body ViewAxesControl::constructor {args} {
    buildSize
    buildPosition
    buildLineWidth
    buildAxesColor
    buildAxesLabelColor
    buildButtons

    itk_component add sep {
	::frame $itk_interior.sep -height 2 -relief sunken -bd 1
    } {
	usual
    }

    # initially hide
    wm withdraw [namespace tail $this]

    packIt

    # process options
    eval itk_initialize $args
}

::itcl::configbody ViewAxesControl::mged {
    if {$itk_option(-mged) == ""} {
	return
    }

    if {[catch {$itk_option(-mged) isa Mged} result]} {
	error "The view axes control panel, $this, is not associated with an Mged object"
    }

    updateControlPanel
}

::itcl::body ViewAxesControl::packIt {} {
    grid $itk_component(sizeL) $itk_component(sizeCB) -sticky nsew \
	    -padx 2 -pady 1
    grid $itk_component(positionL) $itk_component(positionCB) -sticky nsew \
	    -padx 2 -pady 1
    grid $itk_component(lineWidthL) $itk_component(lineWidthCB) -sticky nsew \
	    -padx 2 -pady 1
    grid $itk_component(axesColorL) $itk_component(axesColorCB) -sticky nsew \
	    -padx 2 -pady 1
    grid $itk_component(axesLabelColorL) $itk_component(axesLabelColorCB) -sticky nsew \
	    -padx 2 -pady 1
    grid $itk_component(sep) -columnspan 2 -sticky nsew \
	    -padx 2 -pady 2
    grid $itk_component(dismissB) -columnspan 2 -sticky ns \
	    -padx 2 -pady 1

    grid columnconfigure $itk_component(hull) 1 -weight 1

}

::itcl::body ViewAxesControl::buildSize {} {
    itk_component add sizeL {
	::label $itk_interior.sizeL -text "Size:" -anchor e
    } {
	usual
    }

    itk_component add sizeCB {
	cadwidgets::ComboBox $itk_interior.sizeCB -entryvariable [::itcl::scope axesSize]
    } {
	usual
    }

    $itk_component(sizeCB) entryConfigure -state disabled

    $itk_component(sizeCB) add command -label "Small" \
	    -command [::itcl::code $this setSizeCB "Small"]
    $itk_component(sizeCB) add command -label "Medium" \
	    -command [::itcl::code $this setSizeCB "Medium"]
    $itk_component(sizeCB) add command -label "Large" \
	    -command [::itcl::code $this setSizeCB "Large"]
    $itk_component(sizeCB) add command -label "X-Large" \
	    -command [::itcl::code $this setSizeCB "X-Large"]
}

::itcl::body ViewAxesControl::buildPosition {} {
    itk_component add positionL {
	::label $itk_interior.positionL -text "Position:" -anchor e
    } {
	usual
    }

    itk_component add positionCB {
	cadwidgets::ComboBox $itk_interior.positionCB -entryvariable [::itcl::scope axesPosition]
    } {
	usual
    }

    $itk_component(positionCB) entryConfigure -state disabled

    $itk_component(positionCB) add command -label "Center" \
	    -command [::itcl::code $this setPositionCB "Center"]
    $itk_component(positionCB) add command -label "Upper Left" \
	    -command [::itcl::code $this setPositionCB "Upper Left"]
    $itk_component(positionCB) add command -label "Upper Right" \
	    -command [::itcl::code $this setPositionCB "Upper Right"]
    $itk_component(positionCB) add command -label "Lower Left" \
	    -command [::itcl::code $this setPositionCB "Lower Left"]
    $itk_component(positionCB) add command -label "Lower Right" \
	    -command [::itcl::code $this setPositionCB "Lower Right"]
}

::itcl::body ViewAxesControl::buildLineWidth {} {
    itk_component add lineWidthL {
	::label $itk_interior.lineWidthL -text "Line Width:" -anchor e
    } {
	usual
    }

    itk_component add lineWidthCB {
	cadwidgets::ComboBox $itk_interior.lineWidthCB -entryvariable [::itcl::scope axesLineWidth]
    } {
	usual
    }

    $itk_component(lineWidthCB) entryConfigure -state disabled

    $itk_component(lineWidthCB) add command -label "1" \
	    -command [::itcl::code $this setLineWidthCB "1"]
    $itk_component(lineWidthCB) add command -label "2" \
	    -command [::itcl::code $this setLineWidthCB "2"]
    $itk_component(lineWidthCB) add command -label "3" \
	    -command [::itcl::code $this setLineWidthCB "3"]
}

::itcl::body ViewAxesControl::buildAxesColor {} {
    itk_component add axesColorL {
	::label $itk_interior.axesColorL -text "Axes Color:" -anchor e
    } {
	usual
    }

    itk_component add axesColorCB {
	cadwidgets::ComboBox $itk_interior.axesColorCB -entryvariable [::itcl::scope axesColor]
    } {
	usual
    }

    $itk_component(axesColorCB) entryConfigure -state disabled
    $itk_component(axesColorCB) add command -label "Black" \
	    -command [::itcl::code $this setAxesColorCB "Black"]
    $itk_component(axesColorCB) add command -label "Blue" \
	    -command [::itcl::code $this setAxesColorCB "Blue"]
    $itk_component(axesColorCB) add command -label "Cyan" \
	    -command [::itcl::code $this setAxesColorCB "Cyan"]
    $itk_component(axesColorCB) add command -label "Green" \
	    -command [::itcl::code $this setAxesColorCB "Green"]
    $itk_component(axesColorCB) add command -label "Magenta" \
	    -command [::itcl::code $this setAxesColorCB "Magenta"]
    $itk_component(axesColorCB) add command -label "Red" \
	    -command [::itcl::code $this setAxesColorCB "Red"]
    $itk_component(axesColorCB) add command -label "White" \
	    -command [::itcl::code $this setAxesColorCB "White"]
    $itk_component(axesColorCB) add command -label "Yellow" \
	    -command [::itcl::code $this setAxesColorCB "Yellow"]
    $itk_component(axesColorCB) add command -label "Triple" \
	    -command [::itcl::code $this setAxesColorCB "Triple"]
}

::itcl::body ViewAxesControl::buildAxesLabelColor {} {
    itk_component add axesLabelColorL {
	::label $itk_interior.axesLabelColorL -text "Axes Label Color:" -anchor e
    } {
	usual
    }

    itk_component add axesLabelColorCB {
	cadwidgets::ComboBox $itk_interior.axesLabelColorCB -entryvariable [::itcl::scope axesLabelColor]
    } {
	usual
    }

    $itk_component(axesLabelColorCB) entryConfigure -state disabled
    $itk_component(axesLabelColorCB) add command -label "Black" \
	    -command [::itcl::code $this setAxesLabelColorCB "Black"]
    $itk_component(axesLabelColorCB) add command -label "Blue" \
	    -command [::itcl::code $this setAxesLabelColorCB "Blue"]
    $itk_component(axesLabelColorCB) add command -label "Cyan" \
	    -command [::itcl::code $this setAxesLabelColorCB "Cyan"]
    $itk_component(axesLabelColorCB) add command -label "Green" \
	    -command [::itcl::code $this setAxesLabelColorCB "Green"]
    $itk_component(axesLabelColorCB) add command -label "Magenta" \
	    -command [::itcl::code $this setAxesLabelColorCB "Magenta"]
    $itk_component(axesLabelColorCB) add command -label "Red" \
	    -command [::itcl::code $this setAxesLabelColorCB "Red"]
    $itk_component(axesLabelColorCB) add command -label "White" \
	    -command [::itcl::code $this setAxesLabelColorCB "White"]
    $itk_component(axesLabelColorCB) add command -label "Yellow" \
	    -command [::itcl::code $this setAxesLabelColorCB "Yellow"]
}

::itcl::body ViewAxesControl::buildButtons {} {
    itk_component add dismissB {
	::button $itk_interior.dismissB -relief raised -text "Dismiss" \
		-command [::itcl::code $this hide]
    } {
	usual
    }
}

::itcl::body ViewAxesControl::setSizeCB {size} {
    set axesSize $size

    if {$itk_option(-mged) == ""} {
	return
    }

    switch -- $axesSize {
	"Small" {
	    $itk_option(-mged) configure -viewAxesSize 0.2
	}
	"Medium" {
	    $itk_option(-mged) configure -viewAxesSize 0.4
	}
	"Large" {
	    $itk_option(-mged) configure -viewAxesSize 0.8
	}
	"X-Large" {
	    $itk_option(-mged) configure -viewAxesSize 1.6
	}
    }

    setPositionCB $axesPosition
}

::itcl::body ViewAxesControl::setPositionCB {pos} {
    set axesPosition $pos

    if {$itk_option(-mged) == ""} {
	return
    }

    switch -- $axesSize {
	"Small" {
	    set offset 0.85
	}
	"Medium" {
	    set offset 0.75
	}
	"Large" {
	    set offset 0.55
	}
	default -
	"X-Large" {
	    set offset 0.0
	}
    }

    switch -- $pos {
	default -
	"Center" {
	    $itk_option(-mged) setViewAxesPosition {0 0 0}
	}
	"Upper Left" {
	    $itk_option(-mged) setViewAxesPosition "-$offset $offset 0"
	}
	"Upper Right" {
	    $itk_option(-mged) setViewAxesPosition "$offset $offset 0"
	}
	"Lower Left" {
	    $itk_option(-mged) setViewAxesPosition "-$offset -$offset 0"
	}
	"Lower Right" {
	    $itk_option(-mged) setViewAxesPosition "$offset -$offset 0"
	}
    }
}

::itcl::body ViewAxesControl::setLineWidthCB {lw} {
    set axesLineWidth $lw

    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) configure -viewAxesLineWidth $lw
}

::itcl::body ViewAxesControl::setAxesColorCB {color} {
    set axesColor $color

    if {$itk_option(-mged) == ""} {
	return
    }

    # disable axes triple color
    $itk_option(-mged) configure -viewAxesTripleColor 0

    switch -- $color {
	"Black" {
	    $itk_option(-mged) configure -viewAxesColor {0 0 0}
	}
	"Blue" {
	    $itk_option(-mged) configure -viewAxesColor {100 100 255}
	}
	"Cyan" {
	    $itk_option(-mged) configure -viewAxesColor {0 255 255}
	}
	"Green" {
	    $itk_option(-mged) configure -viewAxesColor {100 255 100}
	}
	"Magenta" {
	    $itk_option(-mged) configure -viewAxesColor {255 0 255}
	}
	"Red" {
	    $itk_option(-mged) configure -viewAxesColor {255 100 100}
	}
	default -
	"White" {
	    $itk_option(-mged) configure -viewAxesColor {255 255 255}
	}
	"Yellow" {
	    $itk_option(-mged) configure -viewAxesColor {255 255 0}
	}
	"Triple" {
	    $itk_option(-mged) configure -viewAxesTripleColor 1
	}
    }
}

::itcl::body ViewAxesControl::setAxesLabelColorCB {color} {
    set axesLabelColor $color

    if {$itk_option(-mged) == ""} {
	return
    }

    switch -- $color {
	"Black" {
	    $itk_option(-mged) configure -viewAxesLabelColor {0 0 0}
	}
	"Blue" {
	    $itk_option(-mged) configure -viewAxesLabelColor {100 100 255}
	}
	"Cyan" {
	    $itk_option(-mged) configure -viewAxesLabelColor {0 255 255}
	}
	"Green" {
	    $itk_option(-mged) configure -viewAxesLabelColor {100 255 100}
	}
	"Magenta" {
	    $itk_option(-mged) configure -viewAxesLabelColor {255 0 255}
	}
	"Red" {
	    $itk_option(-mged) configure -viewAxesLabelColor {255 100 100}
	}
	default -
	"White" {
	    $itk_option(-mged) configure -viewAxesLabelColor {255 255 255}
	}
	"Yellow" {
	    $itk_option(-mged) configure -viewAxesLabelColor {255 255 0}
	}
    }
}

::itcl::body ViewAxesControl::updateControlPanel {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    set size [lindex [$itk_option(-mged) configure -viewAxesSize] 4]
    if {$size <= 0.2} {
	set axesSize "Small"
    } elseif {$size <= 0.4} {
	set axesSize "Medium"
    } elseif {$size <= 0.8} {
	set axesSize "Large"
    } else {
	set axesSize "X-Large"
    }

    set axesPosition "Lower Left"
    set axesLineWidth [lindex [$itk_option(-mged) configure -viewAxesLineWidth] 4]
    if {$axesLineWidth == 0} {
	set axesLineWidth 1
    }

    #set axesColor "White"
    #set axesLabelColor "Yellow"
    set tripleColor [lindex [$itk_option(-mged) configure -viewAxesTripleColor] 4]
    if {$tripleColor} {
	set axesColor "Triple"
    }
}

::itcl::body ViewAxesControl::hide {} {
    wm withdraw [namespace tail $this]
}

::itcl::body ViewAxesControl::show {} {
    wm deiconify [namespace tail $this]
    raise [namespace tail $this]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
