# GUI for "disp" program
#  -Mike Muuss, 7-Mar-96

set wavel 0
global wavel

set line_num	32
global line_num

set pixel_num 32
global pixel_num

set initial_maxval $maxval

# This should be imported from the C code.
set nwave 100
global nwave

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
set int_max 200

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

label .wl1 -text "Wavelen#"
scale .wl2 -from 0 -to [expr $nwave - 1] -showvalue yes \
	-orient horizontal -variable wavel -command {update}
label .wl3 -textvariable lambda -width 8
label .wl4 -text "microns"
pack .wl1 .wl2 .wl3 .wl4 -side left -in .entry_line2

proc update {foo} {
	global wavel
	global pixel_num
	global line_num

	doit1 $wavel
	scanline 0
	pixelplot 0

	# Points to lower left corner of selected pixel, bump up one.
	fb_cursor -42 1 [expr $pixel_num + 1] [expr $line_num + 1]
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
	-orient vertical -variable line_num -command {update}
canvas .canvas_scanline -width $width -height 280

scale .pixel1 -label Pixel -from 0 -to $width -showvalue yes \
	-length 280 \
	-orient vertical -variable pixel_num -command {update}
canvas .canvas_pixel -width $nwave -height 280

pack .canvas_scanline .scanline3 .pixel1 .canvas_pixel -side left -in .plot

# Remember: 4th quadrant addressing!
proc scanline {foo} {
	global wavel
	global width
	global height
	global minval
	global maxval
	global line_num

	set ymax 256

	.canvas_scanline delete T

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
		.canvas_scanline create line $x0 $y0 $x1 $y1 -tags T
		set x0 $x1
		set y0 $y1
##		puts "$x0 $y0 $x1 $y1"
	}
}


# Draw spectral curve for one pixel.
# Remember: 4th quadrant addressing!
proc pixelplot {foo} {
	global wavel
	global width
	global height
	global minval
	global maxval
	global initial_maxval
	global line_num
	global pixel_num
	global nwave

	set ymax 256

	.canvas_pixel delete T

	set x $pixel_num
	set y $line_num
	set x0 0
	set y0 [expr $ymax - 1]
	set scale [expr 255 / ($initial_maxval - $minval) ]
	for {set x1 0} {$x1 < $nwave} {incr x1} {
		set y1 [expr $ymax - 1 - \
			( [getspectval $x $y $x1] - $minval ) * $scale]
		if {$y1 < 0} {
			set y1 0
		} elseif {$y1 > 255} {
			set y1 255
		}
		.canvas_pixel create line $x0 $y0 $x1 $y1 -tags T
		set x0 $x1
		set y0 $y1
##		puts "$x0 $y0 $x1 $y1"
	}
}


### XXX Hack:  Last thing:
doit1 42
