#	T M G E D . T C L
#
#	Tcl/Tk GUI (graphical user interface) for mged-tcl.
#
#	This implementation makes extensive use of the Tk toolkit.
#	It also requires special hooks in mged's cmd.c, which are compiled in
#		when the -DMGED_TCL option is present.
#
#	Author -
#		Glenn Durfee
#
#	Source -
#		The U. S. Army Ballistic Research Laboratory
#		Aberdeen Proving Ground, Maryland 21005
#
#	Copyright Notice -
#		This software is Copyright (C) 1995 by the United States Army.
#		All rights reserved.

set RCSid { "@(#)$Header$ (BRL)" }

mset sgi_win_size=600


#=============================================================================
# PHASE 0: Tcl Variable defaults
#=============================================================================

# "printend" is the last printed character, before whatever the user types.
# Input to MGED is defined as everything between printend and the end of line.
set printend insert

# Some slider defaults.
set sliders(exist) 0
set sliders(fov) 0

# size of dead spot on sliders
set sliders(NOISE) 128


#=============================================================================
# PHASE 1: The button menu
#-----------------------------------------------------------------------------
# The button menu has three parts -- edit options, view options, and 
#   miscellaneous options.  Ideally, we would disable those buttons that are
#   not valid at the time.
#=============================================================================

frame .edit -borderwidth 1 -relief sunken
button .edit.reject -text "Reject Edit" -command "press reject"
button .edit.accept -text "Accept Edit" -command "press accept"

frame .view -borderwidth 1 -relief sunken
button .view.top -text "Top" -command "press top"
button .view.bottom -text "Bottom" -command "press bottom"
button .view.right -text "Right" -command "press right"
button .view.left -text "Left" -command "press left"
button .view.front -text "Front" -command "press front"
button .view.rear -text "Rear" -command "press rear"
button .view.4545 -text "45, 45" -command "press 45,45"
button .view.3525 -text "35, 25" -command "press 35,25"
button .view.restore -text "Restore View" -command "press restore"
button .view.save -text "Save View" -command "press save"
button .view.reset -text "Reset View" -command "press reset"
button .view.sliders -text "Sliders" -command "sliders_create"
button .view.calibrate -text "Calibrate Sliders" -command { knob calibrate; foreach knob { aX aY aZ } { .sliders.f.k$knob set 0 } }
button .view.zero -text "Zero Sliders" -command "sliders_zero"

frame .misc -borderwidth 1 -relief sunken
button .misc.tcolor -text "Color Edit" -command "tcolor"

pack .edit .view .misc -padx 1m -pady 1m -fill x

pack .edit.reject .edit.accept -padx 1m -pady 1 -fill x
pack .view.top .view.bottom .view.right .view.left .view.front .view.rear \
	.view.4545 .view.3525 .view.restore .view.save .view.reset \
	.view.sliders .view.zero .view.calibrate -padx 1m -pady 1 -fill x
pack .misc.tcolor -padx 1m -pady 1 -fill x


#=============================================================================
# PHASE 2: Slider behavior
#-----------------------------------------------------------------------------
# When the sliders exist, pressing the "sliders" button makes them go away.
# When they don't exist, pressing the "sliders" button makes them appear.
# They are modeled after the 4D knobs (dials), right down to the -2048 to 2047
#   range established in dm-4d.c.
# Only the field-of-view slider has its value shown (0 to 120); it might be
#   confusing to see -2048 to 2047 on the others (besides, it would take up
#   more space.)
#=============================================================================

proc sliders_create { } {
	global sliders

	if { $sliders(exist) } then {
		catch { destroy .sliders }
		set sliders(exist) 0
	} else {
		catch { destroy .sliders }
		toplevel .sliders -class Dialog

		frame .sliders.f -borderwidth 3
		label .sliders.f.ratelabel -text "Rate Based Sliders" -anchor c
		scale .sliders.f.kX -label "X Translate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command "sliders_change X"
		scale .sliders.f.kY -label "Y Translate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command "sliders_change Y"
		scale .sliders.f.kZ -label "Z Translate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command "sliders_change Z"
		scale .sliders.f.kS -label "Zoom" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command "sliders_change S"
		scale .sliders.f.kx -label "X Rotate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command "sliders_change x"
		scale .sliders.f.ky -label "Y Rotate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command "sliders_change y"
		scale .sliders.f.kz -label "Z Rotate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command "sliders_change z"
		
		label .sliders.f.abslabel -text "Absolute Sliders" -anchor c
		scale .sliders.f.kaX -label "X Translate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command "sliders_change aX"
		scale .sliders.f.kaY -label "Y Translate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command "sliders_change aY"
		scale .sliders.f.kaZ -label "Z Translate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal \
			-length 400 -command "sliders_change aZ"
		scale .sliders.f.kaS -label "Zoom" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command "sliders_change aS"
		scale .sliders.f.kax -label "X Rotate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command "sliders_change ax"
		scale .sliders.f.kay -label "Y Rotate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command "sliders_change ay"
		scale .sliders.f.kaz -label "Z Rotate" -showvalue no \
			-from -2048 -to 2047 -orient horizontal -length 400 \
			-command "sliders_change az"
		scale .sliders.f.fov -label "Field of view" -showvalue yes \
			-from 0 -to 120 -orient horizontal -length 400 \
			-command sliders_fov

		pack .sliders.f -padx 1m -pady 1m
		pack .sliders.f.ratelabel -pady 4
		pack .sliders.f.kX .sliders.f.kY .sliders.f.kZ .sliders.f.kS \
		     .sliders.f.kx .sliders.f.ky .sliders.f.kz
		pack .sliders.f.abslabel -pady 4
		pack .sliders.f.kaX .sliders.f.kaY .sliders.f.kaZ \
		     .sliders.f.kaS .sliders.f.kax .sliders.f.kay \
		     .sliders.f.kaz .sliders.f.fov

		foreach knob { X Y Z S x y z aX aY aZ aS ax ay az } {
		    .sliders.f.k$knob set [expr round(2048.0*[getknob $knob])]
		}

		.sliders.f.fov set $sliders(fov)

		set sliders(exist) 1
	}
}


## sliders_irlimit
##   Because the sliders may seem too sensitive, setting them exactly to zero
##   may be hard.  This function can be used to extend the location of "zero" 
##   into "the dead zone".

proc sliders_irlimit { val } {
	global sliders

	if { [expr $val > $sliders(NOISE)] } then {
	    return [expr ($val-$sliders(NOISE))*2048/(2048-$sliders(NOISE))]
	}

	if { [expr $val < -$sliders(NOISE)] } then {
	    return [expr ($val+$sliders(NOISE))*2048/(2048-$sliders(NOISE))]
	}

	return 0
}


## sliders_change
##   Generic slider-changing callback.

proc sliders_change { which val } {
	knob $which [expr [sliders_irlimit $val] / 2048.0]
}


## sliders_fov
##   Callback for field-of-view slider.

proc sliders_fov { val } {
	global sliders

	set sliders(fov) $val
	if { [expr $val==0] } then {
		mset perspective=-1
	} else {
		mset perspective=$val
	}
}


## sliders_zero
##   Zeroes the sliders.

proc sliders_zero { } {
	global sliders

	if { [expr $sliders(exist)!=0] } then {
		foreach knob { X Y Z S x y z aX aY aZ aS ax ay az } {
			knob $knob 0
			.sliders.f.k$knob set 0
		}
	}
}


## sliders_togglerate
##   Callback for toggling the "Rate Based" togglebutton.

proc sliders_togglerate { } {
	global sliders

	if { [expr $sliders(rate_based)==0] } then {
		mset rateknobs=0
	} else {
		mset rateknobs=1
	}
}


#=============================================================================
# PHASE 3: MGED Interaction
#-----------------------------------------------------------------------------
# Sets up the MGED Interaction window ".i", which has the following structure:
# At the top, a menu ".i.menu" with some Useful Functions, and a help option.
# Note that some of those functions request filenames in a rather rudimentary
#   fashion.  An honest-to-goodness file selection box would be rather nice.
# The remainder of the window is contained within a sunken frame, ".i.f".
# This frame has two parts: the text widget, ".i.f.text", which contains all
#   mged output (rt_log is hooked into outputting there, see cmd.c), as well
#   as the input from the user.
# In gui_output in cmd.c, when output is sent to ".i.f.text", the variable
#   "printend" is set to the end of the MGED output, which is also the
#   beginning of the user's input.  When the user hits return, everything 
#   from printend to the end of the line is snarfed from .i.f.text and fed 
#   to mged to process as it pleases (see gui_cmdline in cmd.c)
#=============================================================================

toplevel .i
wm title .i "MGED Interaction"

#-----------------------------------------------------------------------------
# MENUS
#-----------------------------------------------------------------------------

frame .i.menu -relief raised -borderwidth 1
menubutton .i.menu.file -text "File" -menu .i.menu.file.m -underline 0
menu .i.menu.file.m
.i.menu.file.m add command -label "Source" -command sourcefile -underline 0
.i.menu.file.m add command -label "Save History" -command savehist \
	-underline 5
.i.menu.file.m add command -label "Save Timed History" -command savethist \
	-underline 5
.i.menu.file.m add command -label "Save Full Transcript" -command savetrans \
	-underline 5
.i.menu.file.m add command -label "Quit" -command quit -underline 0

menubutton .i.menu.help -text "Help" -menu .i.menu.help.m -underline 0
menu .i.menu.help.m
.i.menu.help.m add command -label "Still under construction." -underline 0

pack .i.menu -side top -fill x
pack .i.menu.file -side left
pack .i.menu.help -side right

tk_menuBar .i.menu .i.menu.file .i.menu.help
tk_bindForTraversal .

## fsb
##   Conjures up a file "selection" box with the given properties.

proc fsb { w title caption oktext callback } {
	global filename

	catch { destroy $w }
	toplevel $w -class Dialog
	wm title $w $title

	set filename($w) ""

	frame  $w.f -relief flat
	pack   $w.f -padx 1m -pady 1m
	label  $w.f.label -text $caption
	pack   $w.f.label -side top
	entry  $w.f.entry -textvariable filename($w) -width 16 -relief sunken
	bind   $w.f.entry <Return> "$callback \$filename($w) ; destroy $w"
	pack   $w.f.entry -expand yes -fill x -side top
	button $w.f.ok -text $oktext \
		-command "$callback \$filename($w) ; destroy $w"
	button $w.f.cancel -text "Cancel" -command "destroy $w"
	pack   $w.f.ok -side left -pady 1m
	pack   $w.f.cancel -side right -pady 1m
}


## sourcefile
##   Callback for "Source File" menu item

proc sourcefile { } {
	fsb .sf "Source file" "Enter name of file to be sourced:" \
		"Source" source
}


## savetrans
##   Callback for "Save Transcript" menu item

proc savetrans { } {
	fsb .st "Save Transcript" "Enter filename to store transcript in:" \
		"Save" recordtrans
}


## recordtrans
##   Used by savetrans/fsb to record a transcript to the end of the given file

proc recordtrans { fname } {
	set f [open $fname a+]
	puts $f [.i.f.text get 1.0 end]
	close $f
}


## savehist 
##   Callback for "Save History" menu item

proc savehist { } {
	fsb .sh "Save History" "Enter filename to store history in:" \
		"Save" "history -outfile"
}


## savethist 
##   Callback for "Save Timed History" menu item

proc savethist { } {
	fsb .sh "Save Timed History" \
		"Enter filename to store timed history in:" \
		"Save" "history -delays -outfile"
}


#-----------------------------------------------------------------------------
# MGED INTERACTION WIDGET
#-----------------------------------------------------------------------------

frame .i.f -relief sunken -borderwidth 2
pack .i.f -padx 3 -pady 3

text .i.f.text -relief raised -bd 2 -yscrollcommand ".i.f.scroll set" \
	-width 80 -height 10 -wrap char
scrollbar .i.f.scroll -command ".i.f.text yview"
pack .i.f.scroll -side right -fill y
pack .i.f.text -side left

# give it some rudimentary tcsh/jove/emacs-like bindings

bind .i.f.text <Return> +execute
bind .i.f.text <Control-a> ".i.f.text mark set insert \$printend"
bind .i.f.text <Control-e> ".i.f.text mark set insert end"
bind .i.f.text <Control-d> ".i.f.text delete insert"
bind .i.f.text <Control-k> "set yankbuffer \[.i.f.text get insert \
	\"insert lineend\"\] ; .i.f.text delete insert \"insert lineend\""
bind .i.f.text <Control-y> ".i.f.text insert insert \$yankbuffer ; \
	.i.f.text yview -pickplace insert"
bind .i.f.text <Control-b> ".i.f.text mark set insert \"insert - 1 chars\""
bind .i.f.text <Control-f> ".i.f.text mark set insert \"insert + 1 chars\""
set yankbuffer ""

## execute 
##   Callback for a carriage-return in the MGED Interaction window.
##   Note that if the end of the line on which the carriage return was pressed
##     is before the end of the last prompt, the ".i.f.text get" returns
##     nothing and no command is executed.

proc execute { } {
	global printend sliders

	set commandend [.i.f.text index "insert lineend"]

	.i.f.text mark set insert end
	.i.f.text insert insert \n
	cmdline "[format "%s\n" [.i.f.text get $printend $commandend]]"
	.i.f.text yview -pickplace insert
}





#=============================================================================
# PHASE 4: tcolor -- the color editor
#-----------------------------------------------------------------------------
# Standard Tcl/Tk demo "/usr/{brl,local,contrib}/lib/tk/demos/tcolor".
# It has been assimilated (with minor changes).
# When the color of your choice has been selected, it is spat into the
#   .i.f.text MGED Interaction widget as if you had typed it there.
#=============================================================================

# colorSpace -			Color space currently being used for
#				editing.  Must be "rgb", "cmy", or "hsb".
# label1, label2, label3 -	Labels for the scales.
# red, green, blue -		Current color intensities in decimal
#				on a scale of 0-65535.
# color -			A string giving the current color value
#				in the proper form for x:
#				#RRRRGGGGBBBB
# updating -			Non-zero means that we're in the middle of
#				updating the scales to load a new color,so
#				information shouldn't be propagating back
#				from the scales to other elements of the
#				program:  this would make an infinite loop.
# command -			Holds the command that has been typed
#				into the "Command" entry.
# autoUpdate -			1 means execute the update command
#				automatically whenever the color changes.
# name -			Name for new color, typed into entry.

set colorSpace hsb
set red 65535
set green 0
set blue 0
set color #ffff00000000
set updating 0
set autoUpdate 1
set name ""
		
proc tcolor { } {
	global colorSpace red green blue color updating autoUpdate name

	catch { destroy .c }
	toplevel .c

	wm title .c "Color Editor"
	tk_bindForTraversal .c
	focus .c

# Create the menu bar at the top of the window.

	frame .c.menu -relief raised -borderwidth 2
	pack .c.menu -side top -fill x
	menubutton .c.menu.file -text "Color space" -menu .c.menu.file.m \
		-underline 0
	menu .c.menu.file.m
	.c.menu.file.m add radio -label "RGB color space" \
		-variable colorSpace -value rgb -underline 0 \
		-command {changeColorSpace rgb}
	.c.menu.file.m add radio -label "CMY color space" \
		-variable colorSpace -value cmy -underline 0 \
		-command {changeColorSpace cmy}
	.c.menu.file.m add radio -label "HSB color space" \
		-variable colorSpace -value hsb -underline 0 \
		-command {changeColorSpace hsb}
	pack .c.menu.file -side left
	tk_menuBar .c.menu .c.menu.file

# Create the command entry window at the bottom of the window, along
# with the update button.

	frame .c.bot -relief raised -borderwidth 2
	pack .c.bot -side bottom -fill x
	button .c.cancel -text "Cancel" -command "destroy .c"
	button .c.ok -text "Ok" -command ".i.f.text insert \
		insert \"\$red \$green \$blue\"; destroy .c"
	pack .c.cancel .c.ok -in .c.bot -side left -pady .1c -padx .25c \
		-expand yes -fill x -ipadx 0.25c
	
# Create the listbox that holds all of the color names in rgb.txt,
# if an rgb.txt file can be found.

	frame .c.middle -relief raised -borderwidth 2
	pack .c.middle -side top -fill both
	foreach i {/usr/local/lib/X11/rgb.txt /usr/lib/X11/rgb.txt
		/X11/R5/lib/X11/rgb.txt /X11/R4/lib/rgb/rgb.txt
		/usr/X11R6/lib/X11/rgb.txt } {
		if ![file readable $i] {
			continue;
		}
		set f [open $i]
		frame .c.middle.left
		pack .c.middle.left -side left -padx .25c -pady .25c
		listbox .c.names -geometry 20x12 \
			-yscrollcommand ".c.scroll set" -relief sunken \
			-borderwidth 2 -exportselection false
		tk_listboxSingleSelect .c.names
		bind .c.names <Double-1> {
		    tc_loadNamedColor [.c.names get [.c.names curselection]]
		}
		scrollbar .c.scroll -orient vertical -command \
			".c.names yview" -relief sunken -borderwidth 2
		pack .c.names -in .c.middle.left -side left
		pack .c.scroll -in .c.middle.left -side right -fill y
		while {[gets $f line] >= 0} {
			if {[llength $line] == 4} {
				.c.names insert end [lindex $line 3]
			}
		}
		close $f
		break;
	}

# Create the three scales for editing the color, and the entry for
# typing in a color value.

	frame .c.middle.middle
	pack .c.middle.middle -side left -expand yes -fill y
	frame .c.middle.middle.1
	frame .c.middle.middle.2
	frame .c.middle.middle.3
	frame .c.middle.middle.4
	pack .c.middle.middle.1 .c.middle.middle.2 .c.middle.middle.3 \
		-side top -expand yes
	pack .c.middle.middle.4 -side top -expand yes -fill x
	foreach i {1 2 3} {
		label .c.label$i -textvariable label$i
		scale .c.scale$i -from 0 -to 1000 -length 10c \
			-orient horizontal -command tc_scaleChanged
		button .c.up$i -width 2 -text + -command "tc_inc $i 1"
		button .c.down$i -width 2 -text - -command "tc_inc $i -1"
		pack .c.label$i -in .c.middle.middle.$i -side top -anchor w
		pack .c.down$i -in .c.middle.middle.$i -side left -padx .25c
		pack .c.scale$i -in .c.middle.middle.$i -side left
		pack .c.up$i -in .c.middle.middle.$i -side left -padx .25c
	}
	label .c.nameLabel -text "Name of new color:"
	entry .c.name -relief sunken -borderwidth 2 -textvariable name \
		-width 30 -font -Adobe-Courier-Medium-R-Normal-*-120-*
	pack .c.nameLabel .c.name -in .c.middle.middle.4 -side left
	bind .c.name <Return> {tc_loadNamedColor $name}

# Create the color display swatch on the right side of the window.

	frame .c.middle.right
	pack .c.middle.right -side left -pady .25c -padx .25c -anchor s
	frame .c.swatch -width 2c -height 5c -background $color
	label .c.value -textvariable color -width 13 \
		-font -Adobe-Courier-Medium-R-Normal-*-120-*
	pack .c.swatch -in .c.middle.right -side top -expand yes -fill both
	pack .c.value -in .c.middle.right -side bottom -pady .25c

	changeColorSpace hsb
}

# The procedure below handles the "+" and "-" buttons next to
# the adjustor scales.  They just increment or decrement the
# appropriate scale value.

proc tc_inc {scale inc} {
	.c.scale$scale set [expr [.c.scale$scale get]+$inc]
}

# The procedure below is invoked when one of the scales is adjusted.
# It propagates color information from the current scale readings
# to everywhere else that it is used.

proc tc_scaleChanged args {
    global red green blue colorSpace color updating autoUpdate
    if $updating {
	return
    }
    if {$colorSpace == "rgb"} {
	set red   [format %.0f [expr [.c.scale1 get]*65.535]]
	set green [format %.0f [expr [.c.scale2 get]*65.535]]
	set blue  [format %.0f [expr [.c.scale3 get]*65.535]]
    } else {
	if {$colorSpace == "cmy"} {
	    set red   [format %.0f [expr {65535 - [.c.scale1 get]*65.535}]]
	    set green [format %.0f [expr {65535 - [.c.scale2 get]*65.535}]]
	    set blue  [format %.0f [expr {65535 - [.c.scale3 get]*65.535}]]
	} else {
	    set list [hsbToRgb [expr {[.c.scale1 get]/1000.0}] \
		    [expr {[.c.scale2 get]/1000.0}] \
		    [expr {[.c.scale3 get]/1000.0}]]
	    set red [lindex $list 0]
	    set green [lindex $list 1]
	    set blue [lindex $list 2]
	}
    }
    set color [format "#%04x%04x%04x" $red $green $blue]
    .c.swatch config -bg $color
    update idletasks
}

# The procedure below is invoked to update the scales from the
# current red, green, and blue intensities.  It's invoked after
# a change in the color space and after a named color value has
# been loaded.

proc tc_setScales {} {
    global red green blue colorSpace updating
    set updating 1
    if {$colorSpace == "rgb"} {
	.c.scale1 set [format %.0f [expr $red/65.535]]
	.c.scale2 set [format %.0f [expr $green/65.535]]
	.c.scale3 set [format %.0f [expr $blue/65.535]]
    } else {
	if {$colorSpace == "cmy"} {
	    .c.scale1 set [format %.0f [expr (65535-$red)/65.535]]
	    .c.scale2 set [format %.0f [expr (65535-$green)/65.535]]
	    .c.scale3 set [format %.0f [expr (65535-$blue)/65.535]]
	} else {
	    set list [rgbToHsv $red $green $blue]
	    .c.scale1 set [format %.0f [expr {[lindex $list 0] * 1000.0}]]
	    .c.scale2 set [format %.0f [expr {[lindex $list 1] * 1000.0}]]
	    .c.scale3 set [format %.0f [expr {[lindex $list 2] * 1000.0}]]
	}
    }
    set updating 0
}

# The procedure below is invoked when a named color has been
# selected from the listbox or typed into the entry.  It loads
# the color into the editor.

proc tc_loadNamedColor name {
    global red green blue color autoUpdate

    if {[string index $name 0] != "#"} {
	set list [winfo rgb .c.swatch $name]
	set red [lindex $list 0]
	set green [lindex $list 1]
	set blue [lindex $list 2]
    } else {
	case [string length $name] {
	    4 {set format "#%1x%1x%1x"; set shift 12}
	    7 {set format "#%2x%2x%2x"; set shift 8}
	    10 {set format "#%3x%3x%3x"; set shift 4}
	    13 {set format "#%4x%4x%4x"; set shift 0}
	    default {error "syntax error in color name \"$name\""}
	}
	if {[scan $name $format red green blue] != 3} {
	    error "syntax error in color name \"$name\""
	}
	set red [expr $red<<$shift]
	set green [expr $green<<$shift]
	set blue [expr $blue<<$shift]
    }
    tc_setScales
    set color [format "#%04x%04x%04x" $red $green $blue]
    .c.swatch config -bg $color
}

# The procedure below is invoked when a new color space is selected.
# It changes the labels on the scales and re-loads the scales with
# the appropriate values for the current color in the new color space

proc changeColorSpace space {
    global label1 label2 label3
    if {$space == "rgb"} {
	set label1 Red
	set label2 Green
	set label3 Blue
	tc_setScales
	return
    }
    if {$space == "cmy"} {
	set label1 Cyan
	set label2 Magenta
	set label3 Yellow
	tc_setScales
	return
    }
    if {$space == "hsb"} {
	set label1 Hue
	set label2 Saturation
	set label3 Brightness
	tc_setScales
	return
    }
}

# The procedure below converts an RGB value to HSB.  It takes red, green,
# and blue components (0-65535) as arguments, and returns a list containing
# HSB components (floating-point, 0-1) as result.  The code here is a copy
# of the code on page 615 of "Fundamentals of Interactive Computer Graphics"
# by Foley and Van Dam.

proc rgbToHsv {red green blue} {
    if {$red > $green} {
	set max $red.0
	set min $green.0
    } else {
	set max $green.0
	set min $red.0
    }
    if {$blue > $max} {
	set max $blue.0
    } else {
	if {$blue < $min} {
	    set min $blue.0
	}
    }
    set range [expr $max-$min]
    if {$max == 0} {
	set sat 0
    } else {
	set sat [expr {($max-$min)/$max}]
    }
    if {$sat == 0} {
	set hue 0
    } else {
	set rc [expr {($max - $red)/$range}]
	set gc [expr {($max - $green)/$range}]
	set bc [expr {($max - $blue)/$range}]
	if {$red == $max} {
	    set hue [expr {.166667*($bc - $gc)}]
	} else {
	    if {$green == $max} {
		set hue [expr {.166667*(2 + $rc - $bc)}]
	    } else {
		set hue [expr {.166667*(4 + $gc - $rc)}]
	    }
	}
    }
    return [list $hue $sat [expr {$max/65535}]]
}

# The procedure below converts an HSB value to RGB.  It takes hue, saturation,
# and value components (floating-point, 0-1.0) as arguments, and returns a
# list containing RGB components (integers, 0-65535) as result.  The code
# here is a copy of the code on page 616 of "Fundamentals of Interactive
# Computer Graphics" by Foley and Van Dam.

proc hsbToRgb {hue sat value} {
    set v [format %.0f [expr 65535.0*$value]]
    if {$sat == 0} {
	return "$v $v $v"
    } else {
	set hue [expr $hue*6.0]
	if {$hue >= 6.0} {
	    set hue 0.0
	}
	scan $hue. %d i
	set f [expr $hue-$i]
	set p [format %.0f [expr {65535.0*$value*(1 - $sat)}]]
	set q [format %.0f [expr {65535.0*$value*(1 - ($sat*$f))}]]
	set t [format %.0f [expr {65535.0*$value*(1 - ($sat*(1 - $f)))}]]
	case $i \
	    0 {return "$v $t $p"} \
	    1 {return "$q $v $p"} \
	    2 {return "$p $v $t"} \
	    3 {return "$p $q $v"} \
	    4 {return "$t $p $v"} \
	    5 {return "$v $p $q"}
	error "i value $i is out of range"
    }
}



#============================================================================
# PHASE 5: Commands MGED to set things up all nice and pretty.
#============================================================================

# The usual faceplate is now superfluous
mset faceplate=0

# Tell MGED to put in necessary GUI hooks.
gui .i.f.text
