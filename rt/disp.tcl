# GUI for "disp" program
#  -Mike Muuss, 7-Mar-96

set wavel 0
global wavel

set line_num	32
global line_num

set initial_maxval $maxval

#frame .buttons
frame .scale
frame .entry_line1
frame .entry_line2
frame .plot
#pack .buttons -side top
pack .scale -side top
pack .entry_line1 -side top
pack .entry_line2 -side top
pack .plot -side top

set int_min 0
set int_max 1000

scale .minscale -label Min -from 0 -to 1000 -showvalue no -length 400 \
	-orient horizontal -variable int_min -command {scalechange minval}
scale .maxscale -label Max -from 0 -to 1000 -showvalue no -length 400 \
	-orient horizontal -variable int_max -command {scalechange maxval}
pack .minscale .maxscale -side top -in .scale

label .min1 -text "Min:"
entry .min2 -textvariable minval
label .max1 -text "Max:"
entry .max2 -textvariable maxval
pack .min1 .min2 .max1 .max2 -side left -in .entry_line1

scale .wl1 -label "Wl#" -from 0 -to 99 -showvalue yes \
	-orient horizontal -variable wavel -command {update}
##label .wl1 -text "Wl:"
##entry .wl2 -textvariable wavel
label .wl3 -textvariable lambda -width 8
label .wl4 -text "microns"
pack .wl1 .wl3 .wl4 -side left -in .entry_line2

proc update {foo} {
	global wavel

	doit1 $wavel
	scanline 0
}

proc scalechange {var value} {
	global $var
	global initial_maxval
	global wavel
	global line_num

	set $var [expr $initial_maxval * $value / 1000]
	update 0
}

scale .scanline3 -label Scanline -from $height -to 0 -showvalue yes -length 280 \
	-orient vertical -variable line_num -command {scanline}

canvas .canvas -width $width -height 280
pack .canvas .scanline3 -side left -in .plot

# Remember: 4th quadrant addressing!
proc scanline {foo} {
	global wavel
	global width
	global height
	global minval
	global maxval
	global line_num

	set ymax 256

#	destroy .canvas
#	canvas .canvas -width $width -height 280
#	pack .canvas .scanline3 -side left -in .plot
	.canvas delete T

#	.canvas create line 0 0 $width 0
#		$width $ymax
#	.canvas create line 0 0 \
#		0 $ymax \
#		$width $ymax

	set y $line_num
	set x0 0
	set y0 [expr $ymax - 1]
	set scale [expr 255 / ($maxval - $minval) ]
	for {set x1 0} {$x1 < $width} {incr x1} {
		set y1 [expr $ymax - 1 - \
			( [getspectval $x1 $y $wavel] - $minval ) * $scale]
		if {$y1 < 0} {
			set y1 0
		} elseif {$y1 > 255} {
			set y1 255
		}
		.canvas create line $x0 $y0 $x1 $y1 -tags T
		set x0 $x1
		set y0 $y1
##		puts "$x0 $y0 $x1 $y1"
	}
}
