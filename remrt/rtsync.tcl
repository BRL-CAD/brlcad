# rtsync.tcl
# A prototype GUI for rtsync
#  -Mike Muuss, ARL, March 97.

puts "running rtsync.tcl"
option add *background #ffffff
. configure -background #ffffff

# Create main interaction widget
frame .mbar -relief raised -bd 2 ; pack .mbar -side top
frame .logo_fr ; pack .logo_fr -side top
frame .title_fr ; pack .title_fr -side top
frame .sunangle_fr ; pack .sunangle_fr -side top
frame .suncolor_fr ; pack .suncolor_fr -side top
frame .button_fr ; pack .button_fr -side top

# Menu bar, acros very top
menubutton .mbar.file -text "File" -menu .mbar.file.menu
menubutton .mbar.help -text "Help" -menu .mbar.help.menu
pack .mbar.file -side left -in .mbar -expand 1
pack .mbar.help -side right -in .mbar -expand 1
menu .mbar.file.menu
.mbar.file.menu add command -label "Exit" -command "exit"
menu .mbar.help.menu
.mbar.help.menu add command -label "Exit" -command "exit"

# Title, across the top
image create photo .eagle -file "/m/cad/remrt/eagleCAD.gif"
label .logo -image .eagle
label .title -text "RTSYNC, the real-time ray-tracer"
pack .logo .title -side left -in .title_fr

# The sun angle
label .sunangle_title -text "Sun Angle"
scale .sunangle -label "West to East" -from 180 -to 0 -orient horizontal \
	-command new_sunangle
pack .sunangle_title .sunangle -side top -in .sunangle_fr -fill x
proc new_sunangle {degrees} {
	puts "Sun angle ought to be $degrees"
}

# The sun color
label .suncolor_title -text "Sun Color"
scale .suncolor -label "Yellow to Blue" -from 0 -to 100 -orient horizontal \
	-command new_suncolor
frame .suncolor_swatch -height 1c -width 6c
pack .suncolor_title .suncolor .suncolor_swatch -in .suncolor_fr
proc new_suncolor {percent} {
	global red,grn,blu
	# alpha is how much yellow, beta is how much blue.
	set beta [expr $percent / 100.0 ]
	set alpha [expr 1.0 - $beta ]
	# yellow is 255,255,0 and SkyBlue2 is 126 192 238
	set red [expr int(255 * $alpha + 126 * $beta)]
	set grn [expr int(255 * $alpha + 192 * $beta)]
	set blu [expr int(  0 * $alpha + 238 * $beta)]
	set hexcolor [format #%02x%02x%02x $red $grn $blu]
	.suncolor_swatch config -background $hexcolor
	puts "Sun color $percent percent of yellow = $red $grn $blu"
}

# The buttons
button .apply -text "Apply" -command apply_settings
pack .apply -side top -in .button_fr

proc apply_settings {} {
	# send new stuff to servers
	# indicate re-prep required.
}

puts "done rtsync.tcl"
