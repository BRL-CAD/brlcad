# rtsync.tcl
# A prototype GUI for rtsync.
# This file is executed by the rtsync program, not directly from the shell.
# Depends on rtnode defining [get_dbip] and [get_rtip].
# Depends on rtnode & mged having executed {wdb_open .inmem inmem [get_dbip]}
# Uses various RTSYNC built-in commands, as well as LIBRT's Tcl commands.
#  -Mike Muuss, ARL, March 97.

global red grn blu
set red 0
set grn 0
set blu 0

global sun_region_name
set sun_region_name "light.r"

global sun_solid_name
set sun_solid_name "LIGHT"
global sunx suny sunz
set sunx 0
set suny 0
set sunz 0

global air_shader1
set air_shader1 "200 200 255"
set air_shader2 "air dpm=.01"
set air_region_name "air.r"

puts "running rtsync.tcl"
##option add *background #ffffff
##. configure -background #ffffff

# Create main interaction widget
frame .mbar -relief raised -bd 2 ; pack .mbar -side top
frame .logo_fr ; pack .logo_fr -side top
frame .title_fr ; pack .title_fr -side top
frame .sunangle_fr -relief ridge -bd 2 ; pack .sunangle_fr -side top -expand 1 -fill x
frame .suncolor_fr -relief ridge -bd 2 ; pack .suncolor_fr -side top -expand 1 -fill x
frame .air_fr  -relief ridge -bd 2 ; pack .air_fr -side top -expand 1 -fill x
frame .button_fr ; pack .button_fr -side top

# Menu bar, acros very top
menubutton .mbar.file -text "File" -menu .mbar.file.menu
menubutton .mbar.debug -text "Debug" -menu .mbar.debug.menu
menubutton .mbar.spacepart -text "SpacePart" -menu .mbar.spacepart.menu
menubutton .mbar.help -text "Help" -menu .mbar.help.menu
pack .mbar.file -side left -in .mbar -expand 1 -fill x
pack .mbar.debug -side left -in .mbar -expand 1 -fill x
pack .mbar.spacepart -side left -in .mbar -expand 1 -fill x
pack .mbar.help -side right -in .mbar -expand 1 -fill x
menu .mbar.file.menu
.mbar.file.menu add command -label "Exit" -command "exit"
menu .mbar.help.menu
.mbar.help.menu add command -label "Exit" -command "exit"
menu .mbar.debug.menu
.mbar.debug.menu add command -label "Net Speed Test ON" -command "net_speed_test 1"
.mbar.debug.menu add command -label "Net Speed Test OFF" -command "net_speed_test 0"
.mbar.debug.menu add command -label "Remote bu_log ON" -command "node_send set print_on 1"
.mbar.debug.menu add command -label "Remote bu_log OFF" -command "node_send set print_on 0"
menu .mbar.spacepart.menu
.mbar.spacepart.menu add command -label "Muuss NUBSP" -command "space_partitioning 1"
.mbar.spacepart.menu add command -label "Gigante NUgrid" -command "space_partitioning 0"

# Title, across the top
frame .words_fr
frame .title3
image create photo .eagle -file "/m/cad/remrt/eagleCAD.gif"
label .logo -image .eagle
label .title1 -text "BRL-CAD's SWISS"
label .title2 -text "Real-Time Ray-Tracer"
label .title3a -textvariable cpu_count
label .title3b -text "CPUs active"
label .title4 -textvariable database
pack .title3a .title3b -side left -in .title3
pack .title1 .title2 .title3 .title4 -side top -in .words_fr
pack .logo .words_fr -side left -in .title_fr

# The sun angle
label .sunangle_title -text "Sun Angle, West to East"
scale .sunangle -from 180 -to 0 -orient horizontal -command new_sunangle
pack .sunangle_title .sunangle -side top -in .sunangle_fr -fill x
proc new_sunangle {degrees} {
	global sunx suny sunz

	set sunx [expr ( $degrees - 90) ]
	set suny -13
	set sunz 5
}

frame .sunangle_apply_fr
entry .sunangle_region -width 16 -relief sunken -bd 2 -textvariable sun_solid_name
button .sunangle_apply -text "Apply" -command apply_angle
pack .sunangle_region .sunangle_apply -side left -in .sunangle_apply_fr
pack .sunangle_apply_fr -side top -in .sunangle_fr

proc apply_angle {} {
	global sunx suny sunz
	global sun_solid_name

	puts "Sun pos = $sunx $suny $sunz"
	# send new stuff to servers
	node_send .inmem adjust $sun_solid_name V "{" $sunx $suny $sunz "}"

	# Have MGED update it's position too.
	vrmgr_send .inmem adjust $sun_solid_name V "{" $sunx $suny $sunz "}" ";" \
		redraw_vlist $sun_solid_name

	# indicate LIBRT re-prep required.
	reprep
	# Use new POV if one receieved, else repeat last POV.
	refresh
}

# The sun color
label .suncolor_title -text "Sun Color, Yellow to Blue"
scale .suncolor -from 0 -to 100 -orient horizontal -command new_suncolor
frame .suncolor_swatch -height 1c -width 6c
pack .suncolor_title .suncolor .suncolor_swatch -in .suncolor_fr -fill x
proc new_suncolor {percent} {
	global red grn blu

	if { $percent == 100 } {
		set red 255
		set grn 255
		set blu 255
	} else {
		# alpha is how much yellow, beta is how much blue.
		set beta [expr $percent / 100.0 ]
		set alpha [expr 1.0 - $beta ]
		# yellow is 255,255,0 and SkyBlue2 is 126 192 238
		set red [expr int(255 * $alpha + 126 * $beta)]
		set grn [expr int(255 * $alpha + 192 * $beta)]
		set blu [expr int(  0 * $alpha + 238 * $beta)]
	}
	set hexcolor [format #%02x%02x%02x $red $grn $blu]
	.suncolor_swatch config -background $hexcolor
	#puts "Sun color $percent percent of yellow = $red $grn $blu"
}

frame .suncolor_apply_fr
entry .suncolor_region -width 16 -relief sunken -bd 2 -textvariable sun_region_name
button .apply_color -text "Apply" -command apply_color
pack .suncolor_region .apply_color -side left -in .suncolor_apply_fr
pack .suncolor_apply_fr -side top -in .suncolor_fr

proc apply_color {} {
	global red grn blu
	global sun_region_name

	# send new stuff to servers.  No reprep needed for this.
	# However, change the inmem database too, for consistency.
	node_send \
	  sh_directchange_rgb {[get_rtip]} $sun_region_name $red $grn $blu ";" \
	    .inmem adjust $sun_region_name rgb "{" $red $grn $blu "}"

	# Have MGED update it's color too.  This doesn't work.
	#vrmgr_send .inmem adjust $sun_region_name rgb $red $grn $blu ";" \
	#	redraw_vlist $sun_region_name

	# Use new POV if one receieved, else repeat last POV.
	refresh
}

# The air shader
label .air_title -text "Air Shader"
entry .air_string1 -width 32 -relief sunken -bd 2 -textvariable air_shader1
entry .air_string2 -width 32 -relief sunken -bd 2 -textvariable air_shader2

frame .air_apply_fr
entry .air_region -width 16 -relief sunken -bd 2 -textvariable air_region_name
button .air_apply -text "Apply" -command apply_air
pack .air_region .air_apply -side left -in .air_apply_fr
pack .air_title .air_string1 .air_string2 .air_apply_fr -side top -in .air_fr

proc apply_air {} {
	global air_shader1
	global air_shader2
	global air_region_name

	# send new stuff to servers
	node_send .inmem adjust $air_region_name rgb "{" $air_shader1 "}" ";" \
		  .inmem adjust $air_region_name shader "{" $air_shader2 "}"
	vrmgr_send .inmem adjust $air_region_name rgb "{" $air_shader1 "}" ";" \
		   .inmem adjust $air_region_name shader "{" $air_shader2 "}"

	# indicate LIBRT re-prep required.
	reprep
	# XXX Need short (1 sec) delay here.
	# Use new POV if one receieved, else repeat last POV.
	refresh
}

# The Update CPU Status button, in it's own window
toplevel .status
frame .status.incr_fr
button .status.button -text "Update CPU Status" -command update_cpu_status
button .status.incr -text "NCPU++" -command \
	{cur_node_send {global npsw; incr npsw 1; set npsw}}
button .status.decr -text "NCPU--" -command \
	{cur_node_send {global npsw; incr npsw -1; set npsw}}
listbox .status.list -height 1 -width 60
pack .status.incr .status.decr -side left -in .status.incr_fr
pack .status.button .status.incr_fr .status.list -side top -in .status


proc update_cpu_status {} {
	set nodes [list_rtnodes]

	.status.list delete 0 end
	.status.list configure -height [expr [llength $nodes] + 1]
	.status.list insert end [get_rtnode -1];# generate title
	foreach node $nodes {
		.status.list insert end [get_rtnode $node]
	}
}

proc cur_node_send {remote_cmd} {
	set node_num [lindex [selection get] 0]
	one_node_send $node_num $remote_cmd
}

proc net_speed_test {val} {
	node_send \
		set test_fb_speed $val ";" \
		set curframe 0
}

proc space_partitioning {val} {
	node_send \
		sh_opt -,$val ";" \
		set curframe 0
	reprep
}

# Create independent status window.  All further creates happen here.
frame .statusbut_fr; pack .statusbut_fr -side top

# Allow "send rtsync _stuff_" directives to reach us.
tk appname rtsync

puts "done rtsync.tcl"

# node_send sh_opt -P1 -x1 -X1
# node_send .inmem get LIGHT
# node_send {.inmem adjust LIGHT V {20 -13 5}}
# node_send {.inmem adjust LIGHT V {50 -13 5}} ; reprep; refresh
