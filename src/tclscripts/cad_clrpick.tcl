# cad_clrpick.tcl --
#
#	cad_clrpick.tcl is a modification of clrpick.tcl which was
#	written by Sun Microsystems, Inc.
#
# SCCS: @(#) clrpick.tcl 1.3 96/09/05 09:59:24
#
# Copyright (c) 1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# ToDo:
#	(1): Find out how many free colors are left in the colormap and
#	     don't allocate too many colors.
#	(2): Provide better data hiding.
#
# Modified by:
#	Robert G. Parker
#               *- help on context
#		*- no tkwait
#		*- no grab
#		*- dynamic update
#		*- resize correctly
#		*- supports both RGB and HSV, color modes menu
#		*- new options: -ok and -cancel
#		*- more than one color widget allowed
#		*- original names changed to protect the innocent
#		*- use center of color segment to determine color
#

# cadColorWidget --
#
# Create a color widget and let the user choose a color.
# The caller is responsible for calling "unset data"
#
proc cadColorWidget { mode parent child args } {
    global ::tk::Priv

    if ![winfo exists $parent] {
	cad_dialog $::tk::Priv(cad_dialog) [winfo screen .]\
		"cadColorWidget: parent does not exist"\
		"cadColorWidget: parent does not exist - $parent"\
		"" 0 OK

	return {}
    }

    # Allow more than one color tool
    set w $parent.$child
    if [winfo exists $w] {
	raise $w
	return
    }

    upvar #0 $w data

    # The lines variables track the start and end indices of the line
    # elements in the colorBar canvases.
    set data(lines,colorBar1,start)   0
    set data(lines,colorBar1,last)   -1
    set data(lines,colorBar2,start) 0
    set data(lines,colorBar2,last) -1
    set data(lines,colorBar3,start)  0
    set data(lines,colorBar3,last)  -1

    # This is the actual number of lines that are drawn in each color strip.
    # Note that the colorBars may be of any width.
    # However, NUM_COLOR_SEGMENTS must be a number that evenly divides 256.
    # Such as 256, 128, 64, etc.
#    set data(NUM_COLOR_SEGMENTS) 8
    set data(NUM_COLOR_SEGMENTS) 16

    # COLORBARS_WIDTH is the number of pixels wide the color bar portion of the
    # canvas is. This number must be a multiple of NUM_COLOR_SEGMENTS
    set data(COLORBARS_WIDTH) 128

    # PLGN_WIDTH is the number of pixels wide of the triangular selection
    # polygon. This also results in the definition of the padding on the
    # left and right sides which is half of PLGN_WIDTH. Make this number even.
    set data(PLGN_WIDTH) 10

    # PLGN_HEIGHT is the height of the selection polygon and the height of the
    # selection rectangle at the bottom of the color bar. No restrictions.
    set data(PLGN_HEIGHT) 10

    cadColorWidget_Config $w $args
    cadColorWidget_InitValues $w 1

    set data(xoffset) [expr 3 + $data(indent)]

    # Create toplevel on parent's screen
    toplevel $w -class ::tk::dialog::color:: -screen [winfo screen $parent]
    cadColorWidget_Build $w $mode

#    wm transient $w $parent

    # 5. Withdraw the window, then update all the geometry information
    # so we know how big it wants to be.
    wm withdraw $w
    update idletasks

    set minsize [winfo reqheight $w]
    wm minsize $w $minsize $minsize

    # If possible, center color tool under the pointer.
    set pxy [winfo pointerxy $w]
    set x [expr [lindex $pxy 0] - [winfo reqwidth $w]/2]
    set y [expr [lindex $pxy 1] - [winfo reqheight $w]/2]
    wm geom $w +$x+$y

    wm deiconify $w
    wm title $w $data(-title)

    set data(colorModel) RGB
    update

    cadColorWidget_setColorModel $w
    set data(selection) $data(-initialcolor)
}

# cadColorWidget_InitValues --
#
#	Get called during initialization or when user resets NUM_COLOR_SEGMENTS
#
proc cadColorWidget_InitValues { w doColor } {
    upvar #0 $w data

    # rgbIncr is the difference in color intensity between a color segment
    # and its neighbors.
    set data(rgbIncr) [expr 256 / $data(NUM_COLOR_SEGMENTS)]
    set data(hueIncr) [expr 361.0 / $data(NUM_COLOR_SEGMENTS)]
    set data(normalIncr) [expr 1.0 / $data(NUM_COLOR_SEGMENTS)]

    set data(colorSegmentWidth) \
	[expr $data(COLORBARS_WIDTH) / $data(NUM_COLOR_SEGMENTS).0]

    set rgbIncr [expr 256.0 / $data(NUM_COLOR_SEGMENTS)]
    set data(rgbToX) [expr $data(colorSegmentWidth) / $rgbIncr]
    set data(xToRgb) [expr $rgbIncr / $data(colorSegmentWidth)]
    set data(hueToX) [expr $data(colorSegmentWidth) / $data(hueIncr)]
    set data(xToHue) [expr $data(hueIncr) / $data(colorSegmentWidth)]
    set data(normalToX) [expr $data(colorSegmentWidth) / $data(normalIncr)]
    set data(xToNormal) [expr $data(normalIncr) / $data(colorSegmentWidth)]

    # Indent is the width of the space at the left and right side of the
    # colorBar. It is always half the selector polygon width, because the
    # polygon extends into the space.
    set data(indent) [expr $data(PLGN_WIDTH) / 2]

    set data(colorPad) 2
    set data(selPad)   [expr $data(PLGN_WIDTH) / 2]

    #
    # minX is the x coordinate of the first colorBar
    #
    set data(minX) 3

    #
    # maxX is the x coordinate of the last colorBar
    #
    set data(maxX) [expr $data(COLORBARS_WIDTH) + $data(minX) - 1]

    #
    # canvasWidth is the width of the entire canvas, including the indents
    #
    set data(canvasWidth) [expr $data(COLORBARS_WIDTH) + \
	$data(PLGN_WIDTH)]

    # Set the initial color, specified by -initialcolor, or the
    # color chosen by the user the last time.
    if { $doColor } {
	set data(selection) $data(-initialcolor)
	set data(finalColor)  $data(-initialcolor)
	set rgb [winfo rgb . $data(selection)]

	set data(red) [expr [lindex $rgb 0]/0x100]
	set data(green) [expr [lindex $rgb 1]/0x100]
	set data(blue) [expr [lindex $rgb 2]/0x100]

	set hsv [bu_rgb_to_hsv $data(red) $data(green) $data(blue)]
	set data(hue) [lindex $hsv 0]
	set data(saturation) [lindex $hsv 1]
	set data(value) [lindex $hsv 2]

	if { $data(hue) < 0 } {
	    set data(hue) 0
	}
    }
}

# cadColorWidget_Config  --
#
#	Parses the command line arguments to tk_chooseColor
#
proc cadColorWidget_Config { w argList } {
    upvar #0 $w data

    # 1: the configuration specs
    #
    set specs {
	{-initialcolor "" "" ""}
	{-title "" "" "Color"}
	{-ok "" "" ""}
	{-cancel "" "" ""}
    }

    # 2: parse the arguments
    #
    tclParseConfigSpec $w $specs "" $argList

    if ![string compare $data(-title) ""] {
	set data(-title) " "
    }
    if ![string compare $data(-initialcolor) ""] {
	if {[info exists data(selectColor)] && \
		[string compare $data(selectColor) ""]} {
	    set data(-initialcolor) $data(selectColor)
	} else {
	    set data(-initialcolor) [. cget -background]
	}
    } else {
	if [catch {winfo rgb . $data(-initialcolor)} err] {
	    error $err
	}
    }
}

# cadColorWidget_Build --
#
#	Build the color tool.
#
proc cadColorWidget_Build { w mode } {
    global hoc_data

    upvar #0 $w data

    set topFrame [frame $w.top -relief groove -bd 2]

    frame $topFrame.cmF
    label $topFrame.cmL -text "Color Model:"
    hoc_register_data $topFrame.cmL "Color Model"\
	    { { summary "Select a color model from the menu. Currently, there
are two choices: RGB and HSV." } }
    menubutton $topFrame.cmMB -relief raised -bd 2\
	    -menu $topFrame.cmMB.m -indicatoron 1 -textvariable $w\(colorModel\)
    hoc_register_data $topFrame.cmMB "Color Model"\
	    { { summary "Select a color model from the menu. Currently, there
are two choices: RGB and HSV." } }
    menu $topFrame.cmMB.m -title "Color Model" -tearoff 0
    $topFrame.cmMB.m add radiobutton -value RGB -variable $w\(colorModel\) \
	    -label "RGB" -command  "cadColorWidget_setColorModel $w"
    hoc_register_menu_data "Color Model" "RGB" "Color Model - RGB"\
	    { { summary "Change the color model to RGB." } }
    $topFrame.cmMB.m add radiobutton -value HSV -variable $w\(colorModel\) \
	    -label "HSV" -command  "cadColorWidget_setColorModel $w"
    hoc_register_menu_data "Color Model" "HSV" "Color Model - HSV"\
	    { { summary "Change the color model to HSV." } }
    grid $topFrame.cmL $topFrame.cmMB -in $topFrame.cmF -padx 2

    # StripsFrame contains the colorstrips and the individual RGB entries
    set stripsFrame [frame $topFrame.colorStrip]

    set row 0
    foreach colorBar { colorBar1 colorBar2 colorBar3 } {
	# each f frame contains an [R|G|B] entry and the equiv. color strip.
	set f [frame $stripsFrame.$colorBar]

	# The box frame contains the label and entry widget for an [R|G|B]
	set box [frame $f.box]

	label $box.label -text $colorBar: -width 9 -under 0 -anchor ne
	entry $box.entry -textvariable $w\($colorBar\) -width 7
	hoc_register_data $box.entry "Color Entry"\
		{ { summary "Enter the color component indicated by
the label." } }
	grid $box.label $box.entry -sticky w
	grid columnconfigure $box 0 -weight 0
	grid columnconfigure $box 1 -weight 0

	set height [expr \
	    [winfo reqheight $box.entry] - \
	    2*([$box.entry cget -highlightthickness] + [$box.entry cget -bd])]

	canvas $f.color -height $height\
	    -width $data(COLORBARS_WIDTH) -relief sunken -bd 2
	hoc_register_data $f.color "Color Ramp"\
		{ { summary "This is a color ramp that indicates
the choice of colors." } }
	canvas $f.sel -height $data(PLGN_HEIGHT) \
	    -width $data(canvasWidth) -highlightthickness 0
	hoc_register_data $f.sel "Color Selector"\
		{ { summary "This is a color selector which is used
to select colors from the color ramp." } }
	grid $box -row $row -column 0 -sticky w
	grid $f.color -row $row -column 1 -sticky ew -padx $data(indent)
	grid $f -sticky ew -row $row
	incr row 1
	grid $f.sel -row $row -column 1 -sticky ew
	grid columnconfigure $f 1 -weight 1

	set data($colorBar,entry) $box.entry
	set data($colorBar,col) $f.color
	set data($colorBar,sel) $f.sel

	if { $colorBar == "colorBar1" } {
	    bind $data($colorBar,col) <Configure> "cadColorWidget_ResizeColorBars $w"
	} else {
	    bind $data($colorBar,col) <Configure> "cadColorWidget_DrawColorScale $w $colorBar 1"
	}

	bind $data($colorBar,col) <Enter> \
	    "cadColorWidget_EnterColorBar $w $colorBar"
	bind $data($colorBar,col) <Leave> \
	    "cadColorWidget_LeaveColorBar $w $colorBar"

	bind $data($colorBar,sel) <Enter> \
	    "cadColorWidget_EnterColorBar $w $colorBar"
	bind $data($colorBar,sel) <Leave> \
	    "cadColorWidget_LeaveColorBar $w $colorBar"

	bind $box.entry <Return> "cadColorWidget_HandleColorEntry $w $colorBar"

	incr row 1
    }

    grid columnconfigure $stripsFrame 0 -weight 1
    grid rowconfigure $stripsFrame 1 -weight 1
    grid rowconfigure $stripsFrame 3 -weight 1
    set f1 [frame $topFrame.f1 -relief sunken -bd 2]
    set data(finalCanvas) [frame $f1.demo -bd 0 -width 100 -height 70]
    hoc_register_data $f1.demo "Color Demo"\
	    { { summary "This area is used to demonstrate
what the specified color looks like." } }

    grid $data(finalCanvas) -sticky nsew -in $f1
    grid columnconfigure $f1 0 -weight 1
    grid rowconfigure $f1 0 -weight 1

    grid $topFrame.cmF -sticky w -row 0 -in $topFrame -padx 8 -pady 8
    grid $stripsFrame $f1 -sticky nsew -row 1 -in $topFrame -padx 8 -pady 8
    grid columnconfigure $topFrame 0 -weight 1
    grid columnconfigure $topFrame 1 -weight 1
    grid rowconfigure $topFrame 0 -weight 0
    grid rowconfigure $topFrame 1 -weight 1

    # The midFrame contains a the selection string
    #
    set midFrame [frame $w.mid -relief groove -bd 2]

    frame $midFrame.selF
    label $midFrame.selF.lab -text "Name or Hex String:" -underline 0 -anchor sw
    entry $midFrame.selF.ent -textvariable $w\(selection\)
    hoc_register_data $midFrame.selF.ent "Color Entry"\
		{ { summary "
Enter the composite color specification in either
hex format or using a string from the X color
database (i.e. /usr/X11/lib/X11/rgb.txt). For
example, black or #000000 would specify an
equivalent RGB of 0 0 0. In another example,
wheat or #efca8c would specify an RGB value of
239 202 140." } }
    bind $midFrame.selF.ent <Return> "cadColorWidget_HandleSelEntry $w"
    grid $midFrame.selF.lab $midFrame.selF.ent -sticky ew -in $midFrame.selF
    grid columnconfigure $midFrame.selF 0 -weight 0
    grid columnconfigure $midFrame.selF 1 -weight 1

    grid $midFrame.selF x x -sticky ew -in $midFrame -padx 8 -pady 8
    grid columnconfigure $midFrame 0 -weight 1
    grid columnconfigure $midFrame 1 -weight 1
    grid columnconfigure $midFrame 2 -weight 1

    # the botFrame frame contains the buttons
    #
    set botFrame [frame $w.bot -relief flat -bd 2]
    switch $mode {
	tool {
	    cadColorWidget_SelectDismissButtons $w $botFrame
	}
	dialog {
	    cadColorWidget_OkCancelButtons $w $botFrame
	}
    }

    grid $topFrame -sticky nsew -row 0 -column 0 -in $w -padx 8 -pady 8
    grid $midFrame -sticky nsew -row 1 -column 0 -in $w -padx 8 -pady 8
    grid $botFrame -sticky ew -row 2 -column 0 -in $w -padx 8 -pady 8
    grid columnconfigure $w 0 -weight 1
    grid rowconfigure $w 0 -weight 1
    grid rowconfigure $w 1 -weight 0
    grid rowconfigure $w 2 -weight 0

    # Accelerator bindings
    #
    bind $w <Alt-r> "focus $data(colorBar1,entry)"
    bind $w <Alt-g> "focus $data(colorBar2,entry)"
    bind $w <Alt-b> "focus $data(colorBar3,entry)"
    bind $w <Alt-s> "focus $midFrame.selF.ent"

    wm protocol $w WM_DELETE_WINDOW "cadColorWidget_DismissCmd $w"
}

proc cadColorWidget_SelectDismissButtons { w botFrame } {
    upvar #0 $w data

    button $botFrame.apply -text Select -width 8 -under 0 \
	    -command "cadColorWidget_ApplyCmd $w"
    hoc_register_data $botFrame.apply "Select Color"\
	    { { summary "Select the color from the composite color entry.
Once selected, the color can be pasted to any
window that implements the X selection protocol." } }
    button $botFrame.dismiss -text Dismiss -width 8 -under 0 \
	    -command "cadColorWidget_DismissCmd $w"
    hoc_register_data $botFrame.dismiss "Dismiss Editor"\
	    { { summary "Dismiss the color editor." } }

    set data(applyBtn)      $botFrame.apply
    set data(dismissBtn)  $botFrame.dismiss

    grid x $botFrame.apply x $botFrame.dismiss x -sticky ew -in $botFrame
    grid columnconfigure $botFrame 0 -weight 1
    grid columnconfigure $botFrame 2 -weight 1
    grid columnconfigure $botFrame 4 -weight 1

    # Button specific accelerator bindings
    #
    bind $w <KeyPress-Escape> "::tk::ButtonInvoke $data(dismissBtn)"
    bind $w <Alt-c> "::tk::ButtonInvoke $data(dismissBtn)"
    bind $w <Alt-o> "::tk::ButtonInvoke $data(applyBtn)"
}

proc cadColorWidget_OkCancelButtons { w botFrame } {
    upvar #0 $w data

    button $botFrame.ok -text OK -width 8 -under 0 \
	    -command "cadColorWidget_OkCmd $w"
    hoc_register_data $botFrame.ok "Color Editor - Ok"\
	    { { summary "Dismiss the color editor and apply
the color." } }
    button $botFrame.cancel -text Cancel -width 8 -under 0 \
	    -command "cadColorWidget_CancelCmd $w"
    hoc_register_data $botFrame.cancel "Color Editor - Cancel"\
	    { { summary "Dismiss the color editor, but do not
apply the color." } }

    set data(okBtn)      $botFrame.ok
    set data(cancelBtn)  $botFrame.cancel

    grid x $botFrame.ok x $botFrame.cancel x -sticky ew -in $botFrame
    grid columnconfigure $botFrame 0 -weight 1
    grid columnconfigure $botFrame 2 -weight 1
    grid columnconfigure $botFrame 4 -weight 1

    # Button specific accelerator bindings
    #
    bind $w <KeyPress-Escape> "::tk::ButtonInvoke $data(cancelBtn)"
    bind $w <Alt-c> "::tk::ButtonInvoke $data(cancelBtn)"
    bind $w <Alt-o> "::tk::ButtonInvoke $data(okBtn)"
}

# cadColorWidget_SetRGBValue --
#
#	Sets the current selection of the dialog box
#
proc cadColorWidget_SetRGBValue { w rgb } {
    upvar #0 $w data

    set data(red) [lindex $rgb 0]
    set data(green) [lindex $rgb 1]
    set data(blue) [lindex $rgb 2]

    set hsv [bu_rgb_to_hsv $data(red) $data(green) $data(blue)]
    set data(hue) [lindex $hsv 0]
    set data(saturation) [lindex $hsv 1]
    set data(value) [lindex $hsv 2]

    if { $data(hue) < 0 } {
	set data(hue) 0
    }

    cadColorWidget_RedrawColorBars $w all

    # Now compute the new x value of each colorBars pointer polygon
    foreach colorBar { colorBar1 colorBar2 colorBar3 } {
	switch $colorBar {
	    colorBar1 {
		set x [cadColorWidget_RgbToX $w $data(red)]
	    }
	    colorBar2 {
		set x [cadColorWidget_RgbToX $w $data(green)]
	    }
	    colorBar3 {
		set x [cadColorWidget_RgbToX $w $data(blue)]
	    }
	}
	cadColorWidget_MoveSelector $w $data($colorBar,sel) $colorBar $x 0
    }
}

# cadColorWidget_XToRgb --
#
#	Converts a screen coordinate to intensity
#
proc cadColorWidget_XToRgb {w x} {
    upvar #0 $w data

    set color [cadColorWidget_XToColor $w $x $data(xToRgb)]
    return [expr int($color)]
}

# cadColorWidget_RgbToX
#
#	Converts an intensity to screen coordinate.
#
proc cadColorWidget_RgbToX {w color} {
    upvar #0 $w data

    return [cadColorWidget_ColorToX $w $color $data(rgbToX)]
}

proc cadColorWidget_XToColor { w x cf } {
    upvar #0 $w data

    if { $x <= $data(minX) } {
	return 0
    }

    set x [expr $x - $data(minX)]
    return [expr $x * $cf]
}

proc cadColorWidget_ColorToX { w color cf } {
    upvar #0 $w data

    set x [expr $color * $cf]

    return [expr int($x + $data(minX))]
}



# cadColorWidget_DrawColorScale --
#
proc cadColorWidget_DrawColorScale {w colorBar {create 0}} {
    global lines
    upvar #0 $w data

    set l 0

    # col: color bar canvas
    # sel: selector canvas
    set col $data($colorBar,col)
    set sel $data($colorBar,sel)

    # First handle the case that we are creating everything for the first time.
    if $create {
	# First remove all the lines that already exist.
	if { $data(lines,$colorBar,last) > $data(lines,$colorBar,start)} {
	    for {set i $data(lines,$colorBar,start)} \
		{$i <= $data(lines,$colorBar,last)} { incr i} {
		$sel delete $i
	    }
	}
	# Delete the selector if it exists
	if [info exists data($colorBar,index)] {
	    $sel delete $data($colorBar,index)
	}

	# Draw the selection polygons
	cadColorWidget_CreateSelector $w $sel $colorBar

	if { 0 } {
	    $sel bind $data($colorBar,index) <ButtonPress-1> \
		    "cadColorWidget_StartMove $w $sel $colorBar %x $data(selPad) 1"
	    $sel bind $data($colorBar,index) <B1-Motion> \
		    "cadColorWidget_MoveSelector $w $sel $colorBar %x $data(selPad)"
	} else {
	    $sel bind $data($colorBar,index) <ButtonPress-1> \
		    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(selPad)"
	    $sel bind $data($colorBar,index) <B1-Motion> \
		    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(selPad)"
	}
	$sel bind $data($colorBar,index) <ButtonRelease-1> \
	    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(selPad)"

	set height [winfo height $col]
	# Create an invisible region under the colorstrip to catch mouse clicks
	# that aren't on the selector.
	set data($colorBar,clickRegion) [$sel create rectangle 0 0 \
	    $data(canvasWidth) $height -fill {} -outline {}]

	if { 0 } {
	    bind $col <ButtonPress-1> \
		    "cadColorWidget_StartMove $w $sel $colorBar %x $data(colorPad)"
	    bind $col <B1-Motion> \
		    "cadColorWidget_MoveSelector $w $sel $colorBar %x $data(colorPad)"
	} else {
	    bind $col <ButtonPress-1> \
		    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(colorPad)"
	    bind $col <B1-Motion> \
		    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(colorPad)"
	}
	bind $col <ButtonRelease-1> \
	    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(colorPad)"

	if { 0 } {
	    $sel bind $data($colorBar,clickRegion) <ButtonPress-1> \
		    "cadColorWidget_StartMove $w $sel $colorBar %x $data(selPad)"
	    $sel bind $data($colorBar,clickRegion) <B1-Motion> \
		    "cadColorWidget_MoveSelector $w $sel $colorBar %x $data(selPad)"
	} else {
	    $sel bind $data($colorBar,clickRegion) <ButtonPress-1> \
		    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(selPad)"
	    $sel bind $data($colorBar,clickRegion) <B1-Motion> \
		    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(selPad)"
	}
	$sel bind $data($colorBar,clickRegion) <ButtonRelease-1> \
	    "cadColorWidget_ReleaseMouse $w $sel $colorBar %x $data(selPad)"
    } else {
	# l is the canvas index of the first colorBar.
	set l $data(lines,$colorBar,start)
    }

    # Draw the color bars.
    switch $data(colorModel) {
	RGB {
	    cadColorWidget_DrawRGBColorBars $w $colorBar $create $l
	}
	HSV {
	    cadColorWidget_DrawHSVColorBars $w $colorBar $create $l
	}
    }

    cadColorWidget_RedrawFinalColor $w
}

proc cadColorWidget_DrawHSVColorBars { w colorBar create l } {
    global debug_highlightW
    upvar #0 $w data

    # col: color bar canvas
    # sel: selector canvas
    set col $data($colorBar,col)
    set sel $data($colorBar,sel)

    set highlightW [expr [$col cget -highlightthickness] + [$col cget -bd]]
    set debug_highlightW $highlightW
    for { set i 0 } { $i < $data(NUM_COLOR_SEGMENTS) } { incr i } {
	set startx [expr round($i * $data(colorSegmentWidth) + $highlightW)]

	if { $colorBar == "colorBar1" } {
	    # hue
	    set intensity [expr $i.5 * $data(hueIncr)]
	    set rgb [bu_hsv_to_rgb  $intensity $data(saturation) $data(value)]
	} elseif { $colorBar == "colorBar2" } {
	    # saturation
	    set intensity [expr $i.5 * $data(normalIncr)]
	    if { $intensity == 0 } {
		set intensity 0.0001
	    }
	    set rgb [bu_hsv_to_rgb  $data(hue) $intensity $data(value)]
	} else {
	    # value
	    set intensity [expr $i.5 * $data(normalIncr)]
	    set rgb [bu_hsv_to_rgb  $data(hue) $data(saturation) $intensity]
	}

	set color [format "#%02x%02x%02x" \
		[lindex $rgb 0] \
		[lindex $rgb 1] \
		[lindex $rgb 2]]

	if $create {
	    set index [$col create rect $startx $highlightW \
		    [expr round(($i + 1) * $data(colorSegmentWidth) + $highlightW)] \
		    [expr [winfo height $col] + $highlightW]\
		    -fill $color -outline $color]
	} else {
	    $col itemconf $l -fill $color -outline $color
	    incr l
	}
    }

    $sel raise $data($colorBar,index)

    if $create {
	set data(lines,$colorBar,last) $index
	set data(lines,$colorBar,start) [expr $index - $data(NUM_COLOR_SEGMENTS) + 1 ]
    }
}

proc cadColorWidget_DrawRGBColorBars { w colorBar create l } {
    upvar #0 $w data

    # col: color bar canvas
    # sel: selector canvas
    set col $data($colorBar,col)
    set sel $data($colorBar,sel)

    set highlightW [expr [$col cget -highlightthickness] + [$col cget -bd]]
    for {set i 0} { $i < $data(NUM_COLOR_SEGMENTS)} { incr i} {
	set startx [expr round($i * $data(colorSegmentWidth) + $highlightW)]

	set intensity [expr int($i.5 * $data(rgbIncr))]
	if { $colorBar == "colorBar1" } {
	    # red
	    set color [format "#%02x%02x%02x" $intensity $data(green) $data(blue)]
	} elseif { $colorBar == "colorBar2" } {
	    # green
	    set color [format "#%02x%02x%02x" $data(red) $intensity $data(blue)]
	} else {
	    # blue
	    set color [format "#%02x%02x%02x" $data(red) $data(green) $intensity]
	}

	if $create {
	    set index [$col create rect $startx $highlightW \
		    [expr round(($i + 1) * $data(colorSegmentWidth) + $highlightW)] \
		    [expr [winfo height $col] + $highlightW]\
		    -fill $color -outline $color]
	} else {
	    $col itemconf $l -fill $color -outline $color
	    incr l
	}
    }

    $sel raise $data($colorBar,index)

    if $create {
	set data(lines,$colorBar,last) $index
	set data(lines,$colorBar,start) [expr $index - $data(NUM_COLOR_SEGMENTS) + 1 ]
    }
}

# cadColorWidget_CreateSelector --
#
#	Creates and draws the selector polygon at position $data($colorBar,x).
#
proc cadColorWidget_CreateSelector { w sel colorBar } {
    upvar #0 $w data

    set data($colorBar,index) [$sel create polygon \
	0 $data(PLGN_HEIGHT) \
	$data(PLGN_WIDTH) $data(PLGN_HEIGHT) \
	$data(indent) 0]

    switch $data(colorModel) {
	RGB {
	    switch $colorBar {
		colorBar1 {
		    set data($colorBar,x) [cadColorWidget_ColorToX $w $data(red) $data(rgbToX)]
		}
		colorBar2 {
		    set data($colorBar,x) [cadColorWidget_ColorToX $w $data(green) $data(rgbToX)]
		}
		colorBar3 {
		    set data($colorBar,x) [cadColorWidget_ColorToX $w $data(blue) $data(rgbToX)]
		}
	    }
	}
	HSV {
	    switch $colorBar {
		colorBar1 {
		    set data($colorBar,x) [cadColorWidget_ColorToX $w $data(hue) $data(hueToX)]
		}
		colorBar2 {
		    set data($colorBar,x) [cadColorWidget_ColorToX $w $data(saturation) $data(normalToX)]
		}
		colorBar3 {
		    set data($colorBar,x) [cadColorWidget_ColorToX $w $data(value) $data(normalToX)]
		}
	    }
	}
    }

    $sel move $data($colorBar,index) $data($colorBar,x) 0
}

# cadColorWidget_RedrawFinalColor --
#
#	Combines the intensities of the three colors into the final color
#
proc cadColorWidget_RedrawFinalColor {w} {
    upvar #0 $w data

    set color [format "#%02x%02x%02x" $data(red) $data(green) $data(blue)]

    $data(finalCanvas) configure -bg $color
    set data(finalColor) $color
    set data(selection) $color
    set data(finalRGB) [list $data(red) $data(green) $data(blue)]
    set data(finalHSV) [list $data(hue) $data(saturation) $data(value)]
}

# cadColorWidget_RedrawColorBars --
#
# Only redraws the colors on the color strips that were not manipulated.
# Params: color of colorstrip that changed. If color is not [red|green|blue]
#         Then all colorstrips will be updated
#
proc cadColorWidget_RedrawColorBars {w colorBar} {
    upvar #0 $w data

    switch $colorBar {
	colorBar1 {
	    cadColorWidget_DrawColorScale $w colorBar2
	    cadColorWidget_DrawColorScale $w colorBar3
	}
	colorBar2 {
	    cadColorWidget_DrawColorScale $w colorBar1
	    cadColorWidget_DrawColorScale $w colorBar3
	}
	colorBar3 {
	    cadColorWidget_DrawColorScale $w colorBar1
	    cadColorWidget_DrawColorScale $w colorBar2
	}
	default {
	    cadColorWidget_DrawColorScale $w colorBar1
	    cadColorWidget_DrawColorScale $w colorBar2
	    cadColorWidget_DrawColorScale $w colorBar3
	}
    }
    cadColorWidget_RedrawFinalColor $w
}

#----------------------------------------------------------------------
#			Event handlers
#----------------------------------------------------------------------

# cadColorWidget_StartMove --
#
#	Handles a mousedown button event over the selector polygon.
#	Adds the bindings for moving the mouse while the button is
#	pressed.  Sets the binding for the button-release event.
#
# Params: sel is the selector canvas window, color is the color of the strip.
#
proc cadColorWidget_StartMove {w sel color x delta {dontMove 0}} {
    upvar #0 $w data

    if !$dontMove {
	cadColorWidget_MoveSelector $w $sel $color $x $delta
    }
}

# cadColorWidget_MoveSelector --
#
# Moves the polygon selector so that its middle point has the same
# x value as the specified x. If x is outside the bounds [0,255],
# the selector is set to the closest endpoint.
#
# Params: sel is the selector canvas, c is [red|green|blue]
#         x is a x-coordinate.
#
proc cadColorWidget_MoveSelector {w sel color x delta} {
    upvar #0 $w data

    incr x -$delta

    if { $x < $data(minX) } {
	set x $data(minX)
    } elseif { $x > $data(maxX)} {
	set x $data(maxX)
    }

    set diff [expr  $x - $data($color,x)]
    $sel move $data($color,index) $diff 0
    set data($color,x) [expr $data($color,x) + $diff]

    # Return the x value that it was actually set at
    return $x
}

# cadColorWidget_ReleaseMouse
#
# Removes mouse tracking bindings, updates the colorBars.
#
# Params: sel is the selector canvas, color is the color of the strip,
#         x is the x-coord of the mouse.
#
proc cadColorWidget_ReleaseMouse {w sel colorBar x delta} {
    upvar #0 $w data

    set x [cadColorWidget_MoveSelector $w $sel $colorBar $x $delta]

    # Calculate the color value.
    switch $data(colorModel) {
	RGB {
	    set color [cadColorWidget_XToColor $w $x $data(xToRgb)]
	    set color [expr round($color)]
	    switch $colorBar {
		colorBar1 {
		    set data(red) $color
		}
		colorBar2 {
		    set data(green) $color
		}
		colorBar3 {
		    set data(blue) $color
		}
	    }
	}
	HSV {
	    switch $colorBar {
		colorBar1 {
		    set data(hue) [cadColorWidget_XToColor $w $x $data(xToHue)]

		    if { $data(hue) < 0 } {
			set data(hue) 0
		    }
		}
		colorBar2 {
		    set data(saturation) [cadColorWidget_XToColor $w $x $data(xToNormal)]
		}
		colorBar3 {
		    set data(value) [cadColorWidget_XToColor $w $x $data(xToNormal)]
		}
	    }
	}
    }

    cadColorWidget_AdjustColors $w
    cadColorWidget_RedrawColorBars $w $colorBar

    update
}

# cadColorWidget_ResizeColorbars --
#
#	Completely redraws the colorBars, including resizing the
#	colorstrips
#
proc cadColorWidget_ResizeColorBars {w} {
    upvar #0 $w data

    update
    set width [winfo width $data(colorBar1,col)]
    set data(COLORBARS_WIDTH) [expr $width - 6]

    cadColorWidget_InitValues $w 0

    foreach colorBar { colorBar1 colorBar2 colorBar3 } {
	cadColorWidget_DrawColorScale $w $colorBar 1
    }
}

# cadColorWidget_HandleSelEntry --
#
#	Handles the return keypress event in the "Selection:" entry
#
# This should eventually handle:
#              r g b
#              h s v
#              c m y?????
#              others
#
# For now, only handles:
#		color name
#		#XXXXXX		2 hex values for each of r, g, and b
#
proc cadColorWidget_HandleSelEntry {w} {
    upvar #0 $w data

    set color [string trim $data(selection)]
    set rgb [cadColorWidget_getRGB . $color]

    # Check to make sure the color is valid
    if {[llength $rgb] != 3} {
	set data(selection) $data(finalColor)
	return
    }

    cadColorWidget_SetRGBValue $w $rgb
    set data(selection) $color
}

proc cadColorWidget_getRGB { w color } {
    if [catch {winfo rgb $w $color} rgb] {
	return ""
    }

    # scale from (0, 65535) to (0,255)
    set R [expr [lindex $rgb 0]/0x100]
    set G [expr [lindex $rgb 1]/0x100]
    set B [expr [lindex $rgb 2]/0x100]

    return "$R $G $B"
}

# cadColorWidget_HandleColorEntry --
#
#	Handles the return keypress event in the colorBar[123] entry.
#
proc cadColorWidget_HandleColorEntry { w colorBar } {
    upvar #0 $w data

    if [cadColorWidget_CheckColorRange $w $colorBar] {
	return
    }

    cadColorWidget_AdjustColors $w
    cadColorWidget_RedrawColorBars $w $colorBar

    # Now compute the new x value of each colorBar's pointer polygon
    foreach colorBar { colorBar1 colorBar2 colorBar3 } {
	switch $data(colorModel) {
	    RGB {
		switch $colorBar {
		    colorBar1 {
			set x [cadColorWidget_ColorToX $w $data(red) $data(rgbToX)]
		    }
		    colorBar2 {
			set x [cadColorWidget_ColorToX $w $data(green) $data(rgbToX)]
		    }
		    colorBar3 {
			set x [cadColorWidget_ColorToX $w $data(blue) $data(rgbToX)]
		    }
		}
	    }
	    HSV {
		switch $colorBar {
		    colorBar1 {
			set x [cadColorWidget_ColorToX $w $data(hue) $data(hueToX)]
		    }
		    colorBar2 {
			set x [cadColorWidget_ColorToX $w $data(saturation) $data(normalToX)]
		    }
		    colorBar3 {
			set x [cadColorWidget_ColorToX $w $data(value) $data(normalToX)]
		    }
		}
	    }
	}
	set x [expr int($x)]
	cadColorWidget_MoveSelector $w $data($colorBar,sel) $colorBar $x 0
    }
}

# mouse cursor enters a color bar
#
proc cadColorWidget_EnterColorBar {w colorBar} {
    upvar #0 $w data

    if [info exists data($colorBar,index)] {
	$data($colorBar,sel) itemconfig $data($colorBar,index) -fill red
    }
}

# mouse leaves a color bar
#
proc cadColorWidget_LeaveColorBar {w colorBar} {
    upvar #0 $w data

    if [info exists data($colorBar,index)] {
	$data($colorBar,sel) itemconfig $data($colorBar,index) -fill black
    }
}

# user hits OK button
#
proc cadColorWidget_OkCmd { w } {
    upvar #0 $w data

    catch { eval $data(-ok) }
}

# user hits Cancel button
#
proc cadColorWidget_CancelCmd { w } {
    upvar #0 $w data

    catch { eval $data(-cancel) }
}

# user hits Apply button
#
proc cadColorWidget_ApplyCmd { w } {
    upvar #0 $w data

    set ent $w.mid.selF.ent
    $ent selection range 0 end
}

# user hits Dismiss button
#
proc cadColorWidget_DismissCmd { w } {
    upvar #0 $w data

    catch { destroy $w }
    unset data
}

# cadColorWidget_AdjustColors --
#
#	Given a colorBar that's been modified, adjust the
#	other color variables.
#
proc cadColorWidget_AdjustColors { w } {
    upvar #0 $w data

    switch $data(colorModel) {
	RGB {
	    set hsv [bu_rgb_to_hsv $data(red) $data(green) $data(blue)]
	    set data(hue) [lindex $hsv 0]
	    set data(saturation) [lindex $hsv 1]
	    set data(value) [lindex $hsv 2]

	    if { $data(hue) < 0 } {
		set data(hue) 0
	    }
	}
	HSV {
	    set rgb [bu_hsv_to_rgb $data(hue) $data(saturation) $data(value)]
	    set data(red) [lindex $rgb 0]
	    set data(green) [lindex $rgb 1]
	    set data(blue) [lindex $rgb 2]
	}
    }
}

# cadColorWidget_CheckColorRange --
#
#	Check to insure that the value is in range.
#
proc cadColorWidget_CheckColorRange { w colorBar } {
    upvar #0 $w data

    switch $data(colorModel) {
	RGB {
	    switch $colorBar {
		colorBar1 {
		    if { ($data(red) < 0) || (255 < $data(red)) } {
			set data(red) [lindex $data(finalRGB) 0]
			return 1
		    }
		}
		colorBar2 {
		    if { ($data(green) < 0) || (255 < $data(green)) } {
			set data(green) [lindex $data(finalRGB) 1]
			return 1
		    }
		}
		colorBar3 {
		    if { ($data(blue) < 0) || (255 < $data(blue)) } {
			set data(blue) [lindex $data(finalRGB) 2]
			return 1
		    }
		}
	    }
	}
	HSV {
	    switch $colorBar {
		colorBar1 {
		    if { ($data(hue) < 0) || 360 < $data(hue) } {
			set data(hue) [lindex $data(finalHSV) 0]
			return 1
		    }
		}
		colorBar2 {
		    if { ($data(saturation) < 0.0) || (1.0 < $data(saturation)) } {
			set data(saturation) [lindex $data(finalHSV) 1]
			return 1
		    }
		}
		colorBar3 {
		    if { ($data(value) < 0.0) || (1.0 < $data(value))} {
			set data(value) [lindex $data(finalHSV) 2]
			return 1
		    }
		}
	    }
	}
    }

    # value is in range
    return 0
}

proc cadColorWidget_setColorModel { w } {
    upvar #0 $w data

    set top $w.top.colorStrip

    switch $data(colorModel) {
	RGB {
	    set x1 [cadColorWidget_ColorToX $w $data(red) $data(rgbToX)]
	    set x2 [cadColorWidget_ColorToX $w $data(green) $data(rgbToX)]
	    set x3 [cadColorWidget_ColorToX $w $data(blue) $data(rgbToX)]

	    $top.colorBar1.box.label configure -text Red:
	    $top.colorBar1.box.entry configure -textvariable $w\(red\)
	    $top.colorBar2.box.label configure -text Green:
	    $top.colorBar2.box.entry configure -textvariable $w\(green\)
	    $top.colorBar3.box.label configure -text Blue:
	    $top.colorBar3.box.entry configure -textvariable $w\(blue\)
	}
	HSV {
	    set x1 [cadColorWidget_ColorToX $w $data(hue) $data(hueToX)]
	    set x2 [cadColorWidget_ColorToX $w $data(saturation) $data(normalToX)]
	    set x3 [cadColorWidget_ColorToX $w $data(value) $data(normalToX)]

	    $top.colorBar1.box.label configure -text Hue:
	    $top.colorBar1.box.entry configure -textvariable $w\(hue\)
	    $top.colorBar2.box.label configure -text Saturation:
	    $top.colorBar2.box.entry configure -textvariable $w\(saturation\)
	    $top.colorBar3.box.label configure -text Value:
	    $top.colorBar3.box.entry configure -textvariable $w\(value\)
	}
    }

    set x1 [expr int($x1)]
    set x2 [expr int($x2)]
    set x3 [expr int($x3)]

    cadColorWidget_MoveSelector $w $w.top.colorStrip.colorBar1.sel colorBar1 $x1 0
    cadColorWidget_MoveSelector $w $w.top.colorStrip.colorBar2.sel colorBar2 $x2 0
    cadColorWidget_MoveSelector $w $w.top.colorStrip.colorBar3.sel colorBar3 $x3 0

    cadColorWidget_RedrawColorBars $w all
}

proc cadColorWidget_destroy { w } {
    upvar #0 $w data

    destroy $w
    unset data
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
