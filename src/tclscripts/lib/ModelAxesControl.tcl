#            M O D E L A X E S C O N T R O L . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
#        model axes attributes.
#

::itk::usual ModelAxesControl {
}

::itcl::class ModelAxesControl {
    inherit ::itk::Toplevel

    itk_option define -mged mged Mged ""

    constructor {args} {}
    destructor {}

    protected variable axesSize ""
    protected variable axesPosition {0 0 0}
    protected variable axesLineWidth 1
    protected variable axesColor "White"
    protected variable axesLabelColor "Yellow"

    protected variable tickEnable 1
    protected variable tickInterval 100
    protected variable ticksPerMajor 10
    protected variable tickThreshold 5
    protected variable tickLength 4
    protected variable majorTickLength 8
    protected variable tickColor "Yellow"
    protected variable majorTickColor "Red"

    protected method buildTabs {parent}
    protected method buildAxesTab {}
    protected method buildTicksTab {}
    protected method buildSize {parent}
    protected method buildPosition {parent}
    protected method buildLineWidth {parent}
    protected method buildColor {parent nameL nameCB text var option toption}

    protected method buildTickInterval {parent}
    protected method buildTicksPerMajor {parent}
    protected method buildTickThreshold {parent}
    protected method buildTickLength {parent}
    protected method buildMajorTickLength {parent}
    protected method buildButtons {parent}

    protected method handleTickEnable {}
    protected method setSizeCB {size}
    protected method validatePosition {pos}
    protected method setLineWidthCB {lw}
    protected method setColorCB {var option toption color}

    protected method validateTickInterval {ti}
    protected method setTicksPerMajorCB {tpm}
    protected method setTickThresholdCB {t}
    protected method setTickLengthCB {len}
    protected method setMajorTickLengthCB {len}

    public method hide {}
    public method show {}
    public method updateControlPanel {units}
}

::itcl::body ModelAxesControl::constructor {args} {
    buildTabs $itk_interior
    buildAxesTab
    buildTicksTab
    buildButtons $itk_interior

    itk_component add sep {
	::frame $itk_interior.sep -height 2 -relief sunken -bd 1
    } {
	usual
    }

    # initially hide
    wm withdraw [namespace tail $this]
    wm geometry [namespace tail $this] "250x280"

    grid $itk_component(tabs) -sticky nsew \
	-padx 2 -pady 1
    grid $itk_component(sep) -columnspan 2 -sticky nsew \
	-padx 2 -pady 2
    grid $itk_component(dismissB) -sticky ns \
	-padx 2 -pady 1

    grid rowconfigure $itk_component(hull) 0 -weight 1
    grid columnconfigure $itk_component(hull) 0 -weight 1

    # process options
    eval itk_initialize $args
}

::itcl::configbody ModelAxesControl::mged {
    if {$itk_option(-mged) == ""} {
	return
    }

    if {[catch {$itk_option(-mged) isa Mged} result] ||
	[catch {$itk_option(-mged) isa cadwidgets::Ged} result]} {
	error "The model axes control panel, $this, is not associated with an Mged object"
    }

    updateControlPanel [$itk_option(-mged) units]
}

::itcl::body ModelAxesControl::buildTabs {parent} {
    itk_component add tabs {
	::iwidgets::tabnotebook $itk_interior.tabs -tabpos n -equaltabs false
    } {
	usual
    }

    $itk_component(tabs) add -label Axes
    $itk_component(tabs) add -label Ticks
    $itk_component(tabs) select Axes
}

::itcl::body ModelAxesControl::buildAxesTab {} {
    set parent [$itk_component(tabs) childsite Axes]

    buildSize $parent
    buildPosition $parent
    buildLineWidth $parent
    buildColor $parent axesColorL axesColorCB "Axes Color" \
	axesColor -modelAxesColor -modelAxesTripleColor
    buildColor $parent axesLabelColorL axesLabelColorCB "Axes Label Color" \
	axesLabelColor -modelAxesLabelColor ""

    if {0} {
	itk_component add tickEnableCB {
	    ::checkbutton $parent.tickEnableCB -text "Ticks Enabled" -variable tickEnable \
		-command [::itcl::code $this handleTickEnable]
	} {
	    usual
	}

	# pack everything in this tab
	grid $itk_component(tickEnableCB) -sticky nsew \
	    -padx 2 -pady 8
    }

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

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body ModelAxesControl::buildTicksTab {} {
    set parent [$itk_component(tabs) childsite Ticks]

    buildTickInterval $parent
    buildTicksPerMajor $parent
    buildTickThreshold $parent
    buildTickLength $parent
    buildMajorTickLength $parent
    buildColor $parent tickColorL tickColorCB "Tick Color" \
	tickColor -modelAxesTickColor ""
    buildColor $parent majorTickColorL majorTickColorCB "Major Tick Color" \
	majorTickColor -modelAxesTickMajorColor ""

    # pack everything in this tab
    grid $itk_component(tickIntervalL) $itk_component(tickIntervalCB) -sticky nsew \
	-padx 2 -pady 1
    grid $itk_component(ticksPerMajorL) $itk_component(ticksPerMajorCB) -sticky nsew \
	-padx 2 -pady 1
    grid $itk_component(tickThresholdL) $itk_component(tickThresholdCB) -sticky nsew \
	-padx 2 -pady 1
    grid $itk_component(tickLenL) $itk_component(tickLenCB) -sticky nsew \
	-padx 2 -pady 1
    grid $itk_component(majorTickLenL) $itk_component(majorTickLenCB) -sticky nsew \
	-padx 2 -pady 1
    grid $itk_component(tickColorL) $itk_component(tickColorCB) -sticky nsew \
	-padx 2 -pady 1
    grid $itk_component(majorTickColorL) $itk_component(majorTickColorCB) -sticky nsew \
	-padx 2 -pady 1

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body ModelAxesControl::buildSize {parent} {
    itk_component add sizeL {
	::label $parent.sizeL -text "Size:" -anchor e
    } {
	usual
    }

    itk_component add sizeCB {
	cadwidgets::ComboBox $parent.sizeCB -entryvariable [::itcl::scope axesSize]
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
    $itk_component(sizeCB) add command -label "View (1x)" \
	-command [::itcl::code $this setSizeCB "View (1x)"]
    $itk_component(sizeCB) add command -label "View (2x)" \
	-command [::itcl::code $this setSizeCB "View (2x)"]
    $itk_component(sizeCB) add command -label "View (4x)" \
	-command [::itcl::code $this setSizeCB "View (4x)"]
    $itk_component(sizeCB) add command -label "View (8x)" \
	-command [::itcl::code $this setSizeCB "View (8x)"]
}

::itcl::body ModelAxesControl::buildPosition {parent} {
    itk_component add positionL {
	::label $parent.positionL -text "Position:" -anchor e
    } {
	usual
    }

    itk_component add positionCB {
	cadwidgets::ComboBox $parent.positionCB -entryvariable [::itcl::scope axesPosition]
    } {
	usual
    }

    # remove menubutton
    grid forget [$itk_component(positionCB) component menubutton]

    $itk_component(positionCB) entryConfigure -validate key \
	-validatecommand [::itcl::code $this validatePosition %P]
}

::itcl::body ModelAxesControl::buildLineWidth {parent} {
    itk_component add lineWidthL {
	::label $parent.lineWidthL -text "Line Width:" -anchor e
    } {
	usual
    }

    itk_component add lineWidthCB {
	cadwidgets::ComboBox $parent.lineWidthCB -entryvariable [::itcl::scope axesLineWidth]
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

## - buildColor
#
# This method already suffers from feature creep (i.e. toption).
#
::itcl::body ModelAxesControl::buildColor {parent nameL nameCB text var option toption} {
    itk_component add $nameL {
	::label $parent.$nameL -text $text -anchor e
    } {
	usual
    }

    itk_component add $nameCB {
	cadwidgets::ComboBox $parent.$nameCB -entryvariable [::itcl::scope $var]
    } {
	usual
    }

    $itk_component($nameCB) entryConfigure -state disabled
    $itk_component($nameCB) add command -label "Black" \
	-command [::itcl::code $this setColorCB $var $option $toption "Black"]
    $itk_component($nameCB) add command -label "Blue" \
	-command [::itcl::code $this setColorCB $var $option $toption "Blue"]
    $itk_component($nameCB) add command -label "Cyan" \
	-command [::itcl::code $this setColorCB $var $option $toption "Cyan"]
    $itk_component($nameCB) add command -label "Green" \
	-command [::itcl::code $this setColorCB $var $option $toption "Green"]
    $itk_component($nameCB) add command -label "Magenta" \
	-command [::itcl::code $this setColorCB $var $option $toption "Magenta"]
    $itk_component($nameCB) add command -label "Red" \
	-command [::itcl::code $this setColorCB $var $option $toption "Red"]
    $itk_component($nameCB) add command -label "White" \
	-command [::itcl::code $this setColorCB $var $option $toption "White"]
    $itk_component($nameCB) add command -label "Yellow" \
	-command [::itcl::code $this setColorCB $var $option $toption "Yellow"]

    if {$toption != ""} {
	$itk_component($nameCB) add command -label "Triple" \
	    -command [::itcl::code $this setColorCB $var $toption $toption "Triple"]
    }
}

::itcl::body ModelAxesControl::buildTickInterval {parent} {
    itk_component add tickIntervalL {
	::label $parent.tickIntervalL -text "Tick Interval:" -anchor e
    } {
	usual
    }

    itk_component add tickIntervalCB {
	cadwidgets::ComboBox $parent.tickIntervalCB -entryvariable [::itcl::scope tickInterval]
    } {
	usual
    }

    # remove menubutton
    grid forget [$itk_component(tickIntervalCB) component menubutton]

    $itk_component(tickIntervalCB) entryConfigure -validate key \
	-validatecommand [::itcl::code $this validateTickInterval %P]
}

::itcl::body ModelAxesControl::buildTicksPerMajor {parent} {
    itk_component add ticksPerMajorL {
	::label $parent.ticksPerMajorL -text "Ticks Per Major:" -anchor e
    } {
	usual
    }

    itk_component add ticksPerMajorCB {
	cadwidgets::ComboBox $parent.ticksPerMajorCB -entryvariable [::itcl::scope ticksPerMajor]
    } {
	usual
    }

    $itk_component(ticksPerMajorCB) entryConfigure -state disabled

    $itk_component(ticksPerMajorCB) add command -label "2" \
	-command [::itcl::code $this setTicksPerMajorCB "2"]
    $itk_component(ticksPerMajorCB) add command -label "3" \
	-command [::itcl::code $this setTicksPerMajorCB "3"]
    $itk_component(ticksPerMajorCB) add command -label "4" \
	-command [::itcl::code $this setTicksPerMajorCB "4"]
    $itk_component(ticksPerMajorCB) add command -label "5" \
	-command [::itcl::code $this setTicksPerMajorCB "5"]
    $itk_component(ticksPerMajorCB) add command -label "6" \
	-command [::itcl::code $this setTicksPerMajorCB "6"]
    $itk_component(ticksPerMajorCB) add command -label "8" \
	-command [::itcl::code $this setTicksPerMajorCB "8"]
    $itk_component(ticksPerMajorCB) add command -label "10" \
	-command [::itcl::code $this setTicksPerMajorCB "10"]
    $itk_component(ticksPerMajorCB) add command -label "12" \
	-command [::itcl::code $this setTicksPerMajorCB "12"]
}

::itcl::body ModelAxesControl::buildTickThreshold {parent} {
    itk_component add tickThresholdL {
	::label $parent.tickThresholdL -text "Tick Threshold:" -anchor e
    } {
	usual
    }

    itk_component add tickThresholdCB {
	cadwidgets::ComboBox $parent.tickThresholdCB -entryvariable [::itcl::scope tickThreshold]
    } {
	usual
    }

    $itk_component(tickThresholdCB) entryConfigure -state disabled

    $itk_component(tickThresholdCB) add command -label "4" \
	-command [::itcl::code $this setTickThresholdCB "4"]
    $itk_component(tickThresholdCB) add command -label "8" \
	-command [::itcl::code $this setTickThresholdCB "8"]
    $itk_component(tickThresholdCB) add command -label "16" \
	-command [::itcl::code $this setTickThresholdCB "16"]
    $itk_component(tickThresholdCB) add command -label "32" \
	-command [::itcl::code $this setTickThresholdCB "32"]
    $itk_component(tickThresholdCB) add command -label "64" \
	-command [::itcl::code $this setTickThresholdCB "64"]
}

::itcl::body ModelAxesControl::buildTickLength {parent} {
    itk_component add tickLenL {
	::label $parent.tickLenL -text "Tick Length:" -anchor e
    } {
	usual
    }

    itk_component add tickLenCB {
	cadwidgets::ComboBox $parent.tickLenCB -entryvariable [::itcl::scope tickLength]
    } {
	usual
    }

    $itk_component(tickLenCB) entryConfigure -state disabled

    $itk_component(tickLenCB) add command -label "2" \
	-command [::itcl::code $this setTickLengthCB "2"]
    $itk_component(tickLenCB) add command -label "4" \
	-command [::itcl::code $this setTickLengthCB "4"]
    $itk_component(tickLenCB) add command -label "8" \
	-command [::itcl::code $this setTickLengthCB "8"]
    $itk_component(tickLenCB) add command -label "16" \
	-command [::itcl::code $this setTickLengthCB "16"]
}

::itcl::body ModelAxesControl::buildMajorTickLength {parent} {
    itk_component add majorTickLenL {
	::label $parent.majorTickLenL -text "Major Tick Length:" -anchor e
    } {
	usual
    }

    itk_component add majorTickLenCB {
	cadwidgets::ComboBox $parent.majorTickLenCB -entryvariable [::itcl::scope majorTickLength]
    } {
	usual
    }

    $itk_component(majorTickLenCB) entryConfigure -state disabled

    $itk_component(majorTickLenCB) add command -label "2" \
	-command [::itcl::code $this setMajorTickLengthCB "2"]
    $itk_component(majorTickLenCB) add command -label "4" \
	-command [::itcl::code $this setMajorTickLengthCB "4"]
    $itk_component(majorTickLenCB) add command -label "8" \
	-command [::itcl::code $this setMajorTickLengthCB "8"]
    $itk_component(majorTickLenCB) add command -label "16" \
	-command [::itcl::code $this setMajorTickLengthCB "16"]
}

::itcl::body ModelAxesControl::buildButtons {parent} {
    itk_component add dismissB {
	::button $parent.dismissB -relief raised -text "Dismiss" \
	    -command [::itcl::code $this hide]
    } {
	usual
    }
}

::itcl::body ModelAxesControl::handleTickEnable {} {
    if {$itk_option(-mged) == ""} {
	return
    }

    if {$tickEnable} {
	$itk_component(tabs) pageconfigure Ticks -state normal
    } else {
	$itk_component(tabs) pageconfigure Ticks -state disabled
    }

    $itk_option(-mged) configure -modelAxesTickEnable
#    $itk_option(-mged) setModelAxesTickEnable $tickEnable
}

::itcl::body ModelAxesControl::setSizeCB {size} {
    set axesSize $size

    if {$itk_option(-mged) == ""} {
	return
    }

    switch -- $axesSize {
	"Small" {
	    $itk_option(-mged) configure -modelAxesSize 0.2
	}
	"Medium" {
	    $itk_option(-mged) configure -modelAxesSize 0.4
	}
	"Large" {
	    $itk_option(-mged) configure -modelAxesSize 0.8
	}
	"X-Large" {
	    $itk_option(-mged) configure -modelAxesSize 1.6
	}
	"View (1x)" {
	    $itk_option(-mged) configure -modelAxesSize 2.0
	}
	"View (2x)" {
	    $itk_option(-mged) configure -modelAxesSize 4.0
	}
	"View (4x)" {
	    $itk_option(-mged) configure -modelAxesSize 8.0
	}
	"View (8x)" {
	    $itk_option(-mged) configure -modelAxesSize 16.0
	}
    }
}

::itcl::body ModelAxesControl::validatePosition {pos} {
    if {$itk_option(-mged) == ""} {
	return true
    }

    set len [llength $pos]
    switch -- $len {
	0 {
	    return true
	}
	1 {
	    set x [lindex $pos 0]
	    if {[string is double $x]} {
		return true
	    } else {
		if {[string length $x] == 1 && $x == "-"} {
		    return true
		}

		return false
	    }
	}
	2 {
	    set x [lindex $pos 0]
	    set y [lindex $pos 1]

	    if {[string is double $x] &&
		[string is double $y]} {
		return true
	    } else {
		if {![string is double $x] && \
			[string length $x] == 1 && \
			$x == "-"} {
		    return true
		}

		if {![string is double $y] && \
			[string length $y] == 1 && \
			$y == "-"} {
		    return true
		}

		return false
	    }
	}
	3 {
	    set x [lindex $pos 0]
	    set y [lindex $pos 1]
	    set z [lindex $pos 2]

	    if {[string is double $x] &&
		[string is double $y] &&
		[string is double $z]} {

		$itk_option(-mged) configure -modelAxesPosition $pos
#		$itk_option(-mged) setModelAxesPosition $pos
		return true
	    } else {
		if {![string is double $x] && \
			[string length $x] == 1 && \
			$x == "-"} {
		    return true
		}

		if {![string is double $y] && \
			[string length $y] == 1 && \
			$y == "-"} {
		    return true
		}

		if {![string is double $z] && \
			[string length $z] == 1 && \
			$z == "-"} {
		    return true
		}

		return false
	    }
	}
    }

    return false
}

::itcl::body ModelAxesControl::setLineWidthCB {lw} {
    set axesLineWidth $lw

    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) configure -modelAxesLineWidth $lw
}

::itcl::body ModelAxesControl::setColorCB {var option toption color} {
    set $var $color

    if {$itk_option(-mged) == ""} {
	return
    }

    if {$toption != ""} {
	$itk_option(-mged) configure $toption 0
    }

    switch -- $color {
	"Black" {
	    $itk_option(-mged) configure $option {0 0 0}
	}
	"Blue" {
	    $itk_option(-mged) configure $option {100 100 255}
	}
	"Cyan" {
	    $itk_option(-mged) configure $option {0 255 255}
	}
	"Green" {
	    $itk_option(-mged) configure $option {100 255 100}
	}
	"Magenta" {
	    $itk_option(-mged) configure $option {255 0 255}
	}
	"Red" {
	    $itk_option(-mged) configure $option {255 100 100}
	}
	default -
	"White" {
	    $itk_option(-mged) configure $option {255 255 255}
	}
	"Yellow" {
	    $itk_option(-mged) configure $option {255 255 0}
	}
	"Triple" {
	    $itk_option(-mged) configure $option 1
	}
    }
}

::itcl::body ModelAxesControl::validateTickInterval {ti} {
    if {$itk_option(-mged) == ""} {
	return true
    }

    set len [llength $ti]
    switch -- $len {
	0 {
	    return true
	}
	1 {
	    if {[string is double $ti]} {
		if {0 < $ti} {
		    $itk_option(-mged) configure -modelAxesTickInterval $ti
#		    $itk_option(-mged) setModelAxesTickInterval $ti
		    return true
		}

		if {$ti == 0} {
		    return true
		}

		# must be greater than 0
		return false
	    } else {
		return false
	    }
	}
	default {
	    return false
	}
    }
}

::itcl::body ModelAxesControl::setTicksPerMajorCB {tpm} {
    set ticksPerMajor $tpm

    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) configure -modelAxesTicksPerMajor $ticksPerMajor
#    $itk_option(-mged) setModelAxesTicksPerMajor $ticksPerMajor
}

::itcl::body ModelAxesControl::setTickThresholdCB {threshold} {
    set tickThreshold $threshold

    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) configure -modelAxesTickThreshold $tickThreshold
}

::itcl::body ModelAxesControl::setTickLengthCB {length} {
    set tickLength $length

    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) configure -modelAxesTickLength $tickLength
}

::itcl::body ModelAxesControl::setMajorTickLengthCB {length} {
    set majorTickLength $length

    if {$itk_option(-mged) == ""} {
	return
    }

    $itk_option(-mged) configure -modelAxesTickMajorLength $majorTickLength
}

::itcl::body ModelAxesControl::updateControlPanel {units} {
    if {$itk_option(-mged) == ""} {
	return
    }

    set size [lindex [$itk_option(-mged) configure -modelAxesSize] 4]
    if {$size <= 0.2} {
	set axesSize "Small"
    } elseif {$size <= 0.4} {
	set axesSize "Medium"
    } elseif {$size <= 0.8} {
	set axesSize "Large"
    } elseif {$size <= 1.6} {
	set axesSize "X-Large"
    } elseif {$size <= 2.0} {
	set axesSize "View (1x)"
    } elseif {$size <= 4.0} {
	set axesSize "View (2x)"
    } elseif {$size <= 8.0} {
	set axesSize "View (4x)"
    } elseif {$size <= 16.0} {
	set axesSize "View (8x)"
    }

    #eval set axesPosition [$itk_option(-mged) center]
#    set axesPosition [$itk_option(-mged) setModelAxesPosition]
#    set axesLineWidth [lindex [$itk_option(-mged) configure -modelAxesLineWidth] 4]
    set axesPosition [$itk_option(-mged) cget -modelAxesPosition]
    set axesLineWidth [$itk_option(-mged) cget -modelAxesLineWidth]
    if {$axesLineWidth == 0} {
	set axesLineWidth 1
    }

    #set axesColor "White"
    #set axesLabelColor "Yellow"
    #set tickColor "Yellow"
    #set majorTickColor "Red"

#    set tickEnable [$itk_option(-mged) setModelAxesTickEnable]
#    set tickInterval [$itk_option(-mged) setModelAxesTickInterval]
#    set ticksPerMajor [$itk_option(-mged) setModelAxesTicksPerMajor]
#    set tickThreshold [lindex [$itk_option(-mged) configure -modelAxesTickThreshold] 4]
#    set tickLength [lindex [$itk_option(-mged) configure -modelAxesTickLength] 4]
#    set majorTickLength [lindex [$itk_option(-mged) configure -modelAxesTickMajorLength] 4]

    set tickEnable [$itk_option(-mged) cget -modelAxesTickEnable]
    set tickInterval [$itk_option(-mged) cget -modelAxesTickInterval]
    set ticksPerMajor [$itk_option(-mged) cget -modelAxesTicksPerMajor]
    set tickThreshold [$itk_option(-mged) cget -modelAxesTickThreshold]
    set tickLength [$itk_option(-mged) cget -modelAxesTickLength]
    set majorTickLength [$itk_option(-mged) cget -modelAxesTickMajorLength]

    $itk_component(tickIntervalL) configure -text "Tick Interval ($units):"
}

::itcl::body ModelAxesControl::hide {} {
    wm withdraw [namespace tail $this]
}

::itcl::body ModelAxesControl::show {} {
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
