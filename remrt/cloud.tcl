#!/m/cad/.tk.6d/wish cloud.tcl
#!/bin/sh
# The next line restarts the shell script using best WISH in $PATH
# exec wish "$0" "$@"
#
# A "Dyanamic Geometry" controller for the real-time ray-tracer.
# Uses "send rtsync" directives to exert control.
#
# Causes clouds to move, etc.
#
#  -Mike Muuss, ARL, December 1997.

puts "running cloud.tcl"

# Set the application name by which other applications will be able to
# SEND commands to us with "send motion _stuff_".
# It is not yet clear why this might be beneficial.
tk appname motion
wm title . motion

global time
set time 0

set duration_sec 4.0
set duration_ms [expr int($duration_sec * 1000)]

# Create main interaction widget
frame .title_fr ; pack .title_fr -side top
frame .time_fr ; pack .time_fr -side top

# Title, across the top
frame .words_fr
image create photo .eagle -file "/m/cad/remrt/eagleCAD.gif"
label .logo -image .eagle
label .title1 -text "Motion Controller"
label .title2 -text "for"
label .title3 -text "BRL-CAD's SWISS"
label .title4 -text "Real-Time Ray-Tracer"
pack .title1 .title2 .title3 .title4 -side top -in .words_fr
pack .logo .words_fr -side left -in .title_fr

# The timer
label .time_value -textvariable time
label .time_title -text "Simulation Time (sec)"
pack .time_value .time_title -in .time_fr

for {set time 0} {1} {set time [expr $time + $duration_sec]} {

	# Change displacement in texture space of the clouds.
	set cloud_region_name cloud.r
	set cloud_shader \
"scloud s=5000/5000/5000 m=1 d=[expr $time/100+10]/21.25/10] o=3"


	send rtsync \
		node_send "{" \
		  .inmem adjust $cloud_region_name rgb "{" 255 255 255 "}" shader "{" $cloud_shader "}" ";" \
		  sh_directchange_shader {[get_rtip]} $cloud_region_name \
			"{" $cloud_shader "}" ";" \
		"}" ";" \
		vrmgr_send "{" \
		  .inmem adjust $cloud_region_name rgb "{" 255 255 255 "}" shader "{" $cloud_shader "}" ";" \
		  redraw_vlist $cloud_region_name \
		"}" ";" refresh

	# Delay, updating screen
	update
	after $duration_ms
	update
}
