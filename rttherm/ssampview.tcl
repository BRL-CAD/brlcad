#			D I S P . T C L
#
# GUI for "disp" program
#  -Mike Muuss, 7-Mar-96
#
#  $Header$

puts "disp.tcl: start"

set wavel 0
global wavel

set line_num	32
global line_num

set pixel_num 32
global pixel_num

set cursor_on 1

# Indicate whether Min/Max sliders apply to this plot.  1=yes.
set rescale_scanline 1
set rescale_spectral 0

set initial_maxval $maxval

# This should be imported from the C code.
set nwave 100
global nwave

#frame .buttons
frame .scale
frame .entry_line1
frame .entry_line2
frame .plot
frame .spatial_v
frame .spatial 	-borderwidth 2 -relief ridge
frame .spectral 	-borderwidth 2 -relief groove
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
label .min2 -textvariable minval
label .max1 -text "Max:"
label .max2 -textvariable maxval
checkbutton .cursor_on -text "FB cursor" -variable cursor_on -command {update}
checkbutton .atmosphere_on -text "Atmosphere" -variable use_atmosphere -command {update}
checkbutton .cie_on -text "CIE" -variable use_cie_xyz -command {update}
pack .min1 .min2 .max1 .max2 .cursor_on .atmosphere_on .cie_on \
	-side left -in .entry_line1

label .wl1 -text "Wavelength of Framebuffer = "
label .wl3 -textvariable lambda -width 8
label .wl4 -text "microns"
pack .wl1 .wl3 .wl4 -side left -in .entry_line2

proc update { {foo 0} } {
	global wavel
	global pixel_num
	global line_num
	global cursor_on

	if { [catch "doit1 $wavel" status ] } {
		puts "update: doit1 error= $status"
		return
	}
	scanline
	pixelplot

	# Points to lower left corner of selected pixel, bump up one.
	fb_cursor -42 $cursor_on [expr $pixel_num + 1] [expr $line_num + 1]
	puts [fb_readpixel -42 $pixel_num $line_num]
}

proc scalechange {var value} {
	global $var
	global initial_maxval
	global wavel
	global line_num

	set $var [expr $initial_maxval * $value / 1000]
	update
}

label .scanline4 -text "Scanline Plot"
checkbutton .scanline5 -text "Rescale" -variable rescale_scanline -command {update}
scale .scanline3 -label Scanline -from $height -to 0 -showvalue yes -length 280 \
	-orient vertical -variable line_num -command {update}
canvas .canvas_scanline -width $width -height 280

scale .pixel1 -label Pixel -from 0 -to $width -showvalue yes \
	-orient horizontal -variable pixel_num -command {update}

pack .scanline4 .scanline5 .canvas_scanline .pixel1 -side top -in .spatial_v
pack .spatial_v .scanline3 -side left -in .spatial

label .spectral1 -text "Spectral Plot"
checkbutton .spectral2 -text "Rescale" -variable rescale_spectral -command {update}
canvas .canvas_pixel -width $nwave -height 280
scale .wl2 -label "Wavelen" -from 0 -to [expr $nwave - 1] -showvalue yes \
	-orient horizontal -variable wavel -command {update}
pack .spectral1 .spectral2 .canvas_pixel .wl2 -side top -in .spectral

pack .spatial .spectral -side left -in .plot

# Remember: 4th quadrant addressing!
proc scanline { {foo 0} } {
	global wavel
	global width
	global height
	global minval
	global maxval
	global line_num
	global pixel_num
	global rescale_scanline
	global initial_maxval

	set ymax 256

	.canvas_scanline delete T
	.canvas_scanline create line $pixel_num 0 $pixel_num $ymax -tags T -fill grey

	set y $line_num
	set x0 0
	set y0 [expr $ymax - 1]
	if {$rescale_scanline == 0}  {
		set scale [expr 255 / ($initial_maxval - $minval) ]
	} else {
		set scale [expr 255 / ($maxval - $minval) ]
	}
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

puts "disp.tcl: about to define proc pixelplot"


# Draw spectral curve for one pixel.
# Remember: 4th quadrant addressing!
proc pixelplot { {foo 0} } {
	global wavel
	global width
	global height
	global minval
	global maxval
	global initial_maxval
	global line_num
	global pixel_num
	global nwave
	global wavel
	global rescale_spectral

	set ymax 256

	.canvas_pixel delete T
	# put a vertical rule at current wavelength.
	.canvas_pixel create line $wavel 0 $wavel $ymax -tags T -fill grey

	set x $pixel_num
	set y $line_num
	set x0 0
	set y0 [expr $ymax - 1]
	if {$rescale_spectral == 0}  {
		set scale [expr 255 / ($initial_maxval - $minval) ]
	} else {
		set scale [expr 255 / ($maxval - $minval) ]
	}
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


puts "disp.tcl: about to define proc plot_tabdata"

#			P L O T _ T A B D A T A
#
# Draw spectral curve from one Tcl-ified rt_tabdata structure.  Form is:
#	x {...} y {...} nx # ymin # ymax #
#
# Remember: 4th quadrant addressing!
#
proc plot_tabdata { canvas data {y_minval -1} {y_maxval -1} {screen_xmax 255} {screen_ymax 255} } {
	#puts "plot_tabdata starting"
	#puts "canvas = $canvas"
	#puts "data   = $data"
	#puts "llength= [llength $data]"
	# Sets key_x, key_y, key_nx, key_ymin, key_ymax
	bu_get_all_keyword_values $data
	#puts "ret    = $ret"
	#puts "nx     = $key_nx"
	#puts "key_x  = $key_x"
	#puts "key_y  = $key_y"

	# If y_minval and y_maxval not set (arg defaults used),
	# scale data using min and max values provided with the data.
	if {$y_minval == $y_maxval} {
		set y_minval $key_ymin
		set y_maxval $key_ymax
	}
	#puts "y_minval = $y_minval"
	#puts "y_maxval = $y_maxval"
	set y_scale [expr ($screen_ymax - 1) * 1.0 / ($y_maxval - $y_minval) ]
	#puts "y_scale  = $y_scale"

	# Scale in X dimension
	set x_minval [lindex $key_x 0]
	set x_maxval [lindex $key_x end]
	#puts "x_minval = $x_minval"
	#puts "x_maxval = $x_maxval"
	set x_scale [expr ($screen_xmax - 1) * 1.0 / ($x_maxval - $x_minval) ]
	#puts "x_scale  = $x_scale"

	# Draw a vertical line to control screen-size auto-scale of widget.
	set x0 0
	set y0 [expr $screen_ymax - 1]

	$canvas delete T
	$canvas create line $x0 0 $x0 $screen_ymax -tags T -fill grey

	for {set j 0} {$j < $key_nx} {incr j} {
		set x1 [expr int(([lindex $key_x $j] - $x_minval ) * $x_scale)]
		set y1 [expr int($screen_ymax - \
			( [lindex $key_y $j] - $y_minval ) * $y_scale)]
		if {$y1 < 0} {
			set y1 0
		} elseif {$y1 > $screen_ymax} {
			set y1 $screen_ymax
		}
		$canvas create line $x0 $y0 $x1 $y1 -tags T
		#puts "$x0 $y0 $x1 $y1"
		set x0 $x1
		set y0 $y1
	}
	$canvas create text 0 $screen_ymax -text $x_minval -anchor sw -tags T
	$canvas create text $x1 $screen_ymax -text $x_maxval -anchor se -tags T
	$canvas create text 0 0 -text $y_maxval -anchor nw -tags T
	#puts "plot_tabdata $canvas ending"
}

puts "disp.tcl: about to define proc popup_plot_tabdata"

#			P O P U P _ P L O T _ T A B D A T A
#
#  Create a one-time throwaway pop-up window with a tabdata plot in it.
#
set popup_counter 0
proc popup_plot_tabdata { title data {minval -1} {maxval -1} {screen_xmax 255} {screen_ymax 255} }  {
	global	popup_counter

	incr popup_counter
	set popup .popup$popup_counter

	toplevel $popup
	wm title $popup $title

	puts "About to run canvas ${popup}.canvas"

	canvas ${popup}.canvas -width [expr $screen_xmax + 1] -height [expr $screen_ymax + 1]
	button ${popup}.dismiss -text "Dismiss" -command "destroy $popup"
	pack ${popup}.canvas ${popup}.dismiss -side top -in $popup

puts "about to call plot_tabdata"
	plot_tabdata ${popup}.canvas $data $minval $maxval $screen_xmax $screen_ymax
	return $popup
}

puts "disp.tcl: about to define proc do_testing"

proc do_testing {} {
	# sets ntsc_r, ntsc_g, ntsc_b
	getntsccurves
	puts "do_testing: ntsc_r = $ntsc_r"
	popup_plot_tabdata "NTSC Red, Sampled" $ntsc_r 0 1
	popup_plot_tabdata "NTSC Green, Sampled" $ntsc_g 0 1
	popup_plot_tabdata "NTSC Blue, Sampled" $ntsc_b 0 1

	popup_plot_tabdata "NTSC Red Orig" $ntsc_r_orig 0 1
	popup_plot_tabdata "NTSC Green Orig" $ntsc_g_orig 0 1
	popup_plot_tabdata "NTSC Blue Orig" $ntsc_b_orig 0 1
}

puts "disp.tcl: About to run first_command= $first_command"

### XXX Hack:  Last thing:
##doit1 42
# This variable is set by C startoff.
eval $first_command

puts "disp.tcl: finished"
