# rtsync.tcl
# A prototype GUI for rtsync
# Depends on rtsync defining $dbip and $rtip.
# Uses various RTSYNC built-in commands, as well as LIBRT's Tcl commands.
#  -Mike Muuss, ARL, March 97.

global red grn blu
set red 0
set grn 0
set blu 0

global sun_region_name
set sun_region_name "light.r"

puts "running rtsync.tcl"
option add *background #ffffff
. configure -background #ffffff

# Create main interaction widget
frame .mbar -relief raised -bd 2 ; pack .mbar -side top
frame .logo_fr ; pack .logo_fr -side top
frame .title_fr ; pack .title_fr -side top
frame .sunangle_fr -relief ridge ; pack .sunangle_fr -side top
frame .suncolor_fr -relief ridge ; pack .suncolor_fr -side top
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
button .apply_angle -text "Apply" -command apply_angle
pack .apply_angle -side top -in .sunangle_fr

proc apply_angle {} {
	global red grn blu
	global sun_region_name
	# send new stuff to servers
	node_send rt_wdb_inmem_rgb {$dbip} $sun_region_name $red $grn $blu
	# indicate re-prep required.
	reprep
	# Send this to MGED to get pov message sent back.
	vrmgr_send refresh
}

# The sun color
label .suncolor_title -text "Sun Color"
scale .suncolor -label "Yellow to Blue" -from 0 -to 100 -orient horizontal \
	-command new_suncolor
frame .suncolor_swatch -height 1c -width 6c
pack .suncolor_title .suncolor .suncolor_swatch -in .suncolor_fr
proc new_suncolor {percent} {
	global red grn blu
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

button .apply_color -text "Apply" -command apply_color
pack .apply_color -side top -in .suncolor_fr

entry .suncolor_region -width 16 -relief sunken -bd 2 -textvariable sun_region_name
pack .suncolor_region -side left -in .suncolor_fr

proc apply_color {} {
	global red grn blu
	global sun_region_name
	# send new stuff to servers.  No reprep needed for this.
	node_send sh_directchange_rgb {$rtip} $sun_region_name $red $grn $blu
	# Send this to MGED to get pov message sent back.
	vrmgr_send refresh
}

puts "done rtsync.tcl"
