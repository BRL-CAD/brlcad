#!/m/cad/.tk.6d/wish dg.tcl
#!/bin/sh
# The next line restarts the shell script using best WISH in $PATH
# exec wish "$0" "$@"
#
# A "Dyanamic Geometry" controller for the real-time ray-tracer.
# Uses "send rtsync" directives to exert control.
#  -Mike Muuss, ARL, December 1997.

puts "running dg.tcl"
##option add *background #ffffff
##. configure -background #ffffff

# Set the application name by which other applications will be able to
# SEND commands to us with "send dg _stuff_".
# It is not yet clear why this might be beneficial.
tk appname dg
wm title . dg

# Global variables
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

global treetop_name
global treetop_tree
set treetop_name all.g
# XXX Hack from Moss-World, should be obtained with
# XXX set treetop_tree [list [send rtsync vrmgr_send .inmem get all.g]]
set treetop_tree "{u {u {u {u {u {l platform.r} {l box.r {1 0 0 -23.6989  0 1 0 13.41  0 0 1 8.02399  0 0 0 1}}} {l cone.r {1 0 0 22.0492  0 1 0 12.2349  0 0 1 2.11125e-07  0 0 0 1}}} {l ellipse.r {1 0 0 14.6793  0 1 0 -41.6077  0 0 1 38.7988  0 0 0 1}}} {l tor.r}} {l light.r}}"

# Create main interaction widget
frame .logo_fr ; pack .logo_fr -side top
frame .title_fr ; pack .title_fr -side top
frame .sunangle_fr -relief ridge -bd 2 ; pack .sunangle_fr -side top -expand 1 -fill x
frame .suncolor_fr -relief ridge -bd 2 ; pack .suncolor_fr -side top -expand 1 -fill x
frame .air_fr  -relief ridge -bd 2 ; pack .air_fr -side top -expand 1 -fill x
frame .hole_fr ; pack .hole_fr -side top

# Title, across the top
frame .words_fr
image create photo .eagle -file "/m/cad/remrt/eagleCAD.gif"
label .logo -image .eagle
label .title1 -text "Dynamic Geometry Controller"
label .title2 -text "for"
label .title3 -text "BRL-CAD's SWISS"
label .title4 -text "Real-Time Ray-Tracer"
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
	# send new stuff to servers and to MGED
	# indicate LIBRT re-prep required.
	# Use new POV if one receieved, else repeat last POV.
	send rtsync \
		node_send "{" \
			.inmem adjust $sun_solid_name \
				V "{" $sunx $suny $sunz "}" \
		"}" ";" \
		vrmgr_send "{" \
			.inmem adjust $sun_solid_name \
				V "{" $sunx $suny $sunz "}" ";" \
			redraw_vlist $sun_solid_name \
		"}" ";" reprep ";" refresh
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
	# Use new POV if one receieved, else repeat last POV.
	send rtsync \
		node_send "{" \
		  sh_directchange_rgb {[get_rtip]} $sun_region_name $red $grn $blu ";" \
		  .inmem adjust $sun_region_name rgb "{" $red $grn $blu "}" \
		"}" ";" \
		vrmgr_send "{" \
		  .inmem adjust $sun_region_name rgb "{" $red $grn $blu "}" ";" \
		  redraw_vlist $sun_region_name \
		"}" ";" refresh
}

# The air shader
label .air_title -text "Air Shader"
entry .air_string1 -width 32 -relief sunken -bd 2 -textvariable air_shader1
entry .air_string2 -width 42 -relief sunken -bd 2 -textvariable air_shader2

frame .air_apply_fr
button .air_demo -text "Cloud" -command {\
	set air_shader1 "255 255 255";\
	set air_shader2 "scloud s=10/10/10 m=.05 d=10/21.25/10 o=3"}
entry .air_region -width 16 -relief sunken -bd 2 -textvariable air_region_name
button .air_apply -text "Apply" -command apply_air
pack .air_demo .air_region .air_apply -side left -in .air_apply_fr
pack .air_title .air_string1 .air_string2 .air_apply_fr -side top -in .air_fr

proc apply_air {} {
	global air_shader1
	global air_shader2
	global air_region_name

	# send new stuff to servers
	# indicate LIBRT re-prep required.
	# XXX Need short (1 sec) delay here.
	# Use new POV if one receieved, else repeat last POV.
	send rtsync \
		node_send .inmem adjust $air_region_name \
			rgb "{" $air_shader1 "}" \
			shader "{" $air_shader2 "}" ";" \
		vrmgr_send .inmem adjust $air_region_name \
			rgb "{" $air_shader1 "}" \
			shader "{" $air_shader2 "}" ";" \
		reprep ";" refresh
}

# The dynamic hole-maker
button .hole_init -text "Hole Init" -command hole_init
entry .hole_treetop -width 16 -relief sunken -bd 2 -textvariable treetop_name
button .hole_size1 -text "Size1" -command "hole_size 20"
button .hole_size2 -text "Size2" -command "hole_size 40"
button .hole_size3 -text "Size3" -command "hole_size 60"
pack .hole_init .hole_treetop .hole_size1 .hole_size2 .hole_size3 -side left -in .hole_fr

proc hole_init {} {
	global treetop_name
	global treetop_tree

	set newtree [concat "{-" $treetop_tree "{l _hole.s}}"]
	puts "newtree = $newtree"

	# .inmem form tgc
	# XXX Maybe this should be _mged_killall ?
	# Or better still, an rm $treetop_name _hole.s
	send rtsync \
		vrmgr_send \
			_mged_kill _hole.s

	send rtsync \
		node_send "{" \
			.inmem put _hole.s tgc \
				V "{" 0 0   0 "}" \
				H "{" 0 0 -50 "}" \
				A "{" 1 0 0 "}" \
				B "{" 0 1 0 "}" \
				C "{" 1 0 0 "}" \
				D "{" 0 1 0 "}" \
			";" \
			.inmem adjust $treetop_name \
				tree $newtree \
		"}" ";" \
		vrmgr_send "{" \
			.inmem put _hole.s tgc \
				V "{" 0 0   0 "}" \
				H "{" 0 0 -50 "}" \
				A "{" 1 0 0 "}" \
				B "{" 0 1 0 "}" \
				C "{" 1 0 0 "}" \
				D "{" 0 1 0 "}" \
			";" \
			.inmem adjust $treetop_name \
				tree $newtree ";" \
			_mged_e _hole.s ";" \
		"}" ";" \
		reprep ";" refresh
}

proc hole_size { radius } {
	send rtsync \
		node_send "{" \
			.inmem adjust _hole.s \
			A "{" $radius 0 0 "}" \
			B "{" 0 $radius 0 "}" \
			C "{" $radius 0 0 "}" \
			D "{" 0 $radius 0 "}" ";" \
		"}" ";" \
		vrmgr_send "{" \
			.inmem adjust _hole.s \
			A "{" $radius 0 0 "}" \
			B "{" 0 $radius 0 "}" \
			C "{" $radius 0 0 "}" \
			D "{" 0 $radius 0 "}" ";" \
			redraw_vlist _hole.s \
		"}" ";" \
		reprep ";" refresh
}

puts "done dg.tcl"

# node_send sh_opt -P1 -x1 -X1
# node_send .inmem get LIGHT
# node_send {.inmem adjust LIGHT V {20 -13 5}}
# node_send {.inmem adjust LIGHT V {50 -13 5}} ; reprep; refresh
