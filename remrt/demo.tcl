#!/m/cad/.tk.6d/wish demo.tcl
#!/bin/sh
# The next line restarts the shell script using best WISH in $PATH
# exec wish "$0" "$@"
# demo.tcl
#	setenv LD_LIBRARY_PATH /usr/lib/X11:/usr/X11/lib
# A GUI for starting up a demonstration of the real-time ray-tracer.
# To be run from the shell, on the machine to run MGED on.
#  starts rtsync, mged, and various rtnode's.
#  -Mike Muuss, ARL, May 97.

puts "running demo.tcl"
##option add *background #ffffff
##. configure -background #ffffff

set hostname [exec hostname]
set mged_is_alive	0

# Create main interaction widget
frame .mbar -relief raised -bd 2 ; pack .mbar -side top
frame .logo_fr ; pack .logo_fr -side top
frame .title_fr ; pack .title_fr -side top
frame .fbserv_fr -relief ridge -bd 2 ; pack .fbserv_fr -side top
frame .mged_fr  -relief ridge -bd 2 ; pack .mged_fr -side top
frame .rtsync_fr -relief ridge -bd 2 ; pack .rtsync_fr -side top
frame .rtnode_fr -relief ridge -bd 2 ; pack .rtnode_fr -side top
frame .button_fr ; pack .button_fr -side top

# Menu bar, acros very top
menubutton .mbar.file -text "File" -menu .mbar.file.menu
pack .mbar.file -side left -in .mbar -expand 1 -fill x
menu .mbar.file.menu
.mbar.file.menu add command -label "Exit" -command "exit"

# Title, across the top
frame .words_fr
image create photo .eagle -file "/m/cad/remrt/eagleCAD.gif"
label .logo -image .eagle
label .title1 -text "A Demonstration of:"
label .title2 -text "The Real-Time Ray-Tracer"
pack .title1 .title2 -side top -in .words_fr
pack .logo .words_fr -side left -in .title_fr

# Set up FBSERV parameters
if { [catch { set fbserv_host $env(FB_FILE) }] }  {
	# Environment variable not set, default to this machine
	set fbserv_host ${hostname}:0
}
set fbserv_resolution 256
frame .fb1_fr
label .fb_title -text "FBSERV host:"
entry .fb_host -width 32 -relief sunken -bd 2 -textvariable fbserv_host
pack .fb_title .fb_host -side left -in .fb1_fr

frame .fb2_fr
label .res_title -text "FBSERV resolution:"
entry .res_value -width 4 -relief sunken -bd 2 -textvariable fbserv_resolution
pack .res_title .res_value -side left -in .fb2_fr
pack .fb1_fr .fb2_fr -side top -in .fbserv_fr

# Set up MGED parameters
set database_file /m/cad/.db.6d/world.g
set mged_treetops all.g
frame .mged1_fr
label .mged1_title -text "MGED database:"
entry .mged_db -width 32 -relief sunken -bd 2 -textvariable database_file
pack .mged1_title .mged_db -side left -in .mged1_fr

frame .mged2_fr
label .mged2_title -text "MGED treetops:"
entry .mged2_treetops -width 32 -relief sunken -bd 2 -textvariable mged_treetops
button .mged2_button -text "SENSE" -command mged_sense
pack .mged2_title .mged2_treetops -side left -in .mged2_fr
pack .mged1_fr .mged2_fr .mged2_button -side top -in .mged_fr

proc mged_sense {} {
	global database_file
	global mged_treetops
	global mged_is_alive

	# Assumes MGED is running on local display, opened with "openw" cmd.
	puts "mged_sense"
	if { [catch { send mged echo NIL } status] } {
		puts "send to MGED failed, status=$status"
		puts "MGED's window needs to be opened with 'openw' command."
		return
	}
	set database_file [send mged "opendb"]
	set mged_treetops [send mged "who"]
	set mged_is_alive 1
}

# Select machine to run RTSYNC on
set rtsync_host $hostname
set rtsync_port 4446
frame .sync1_fr
label .rtsync_title -text "RTSYNC host:"
entry .rtsync_host -width 24 -relief sunken -bd 2 -textvariable rtsync_host
pack .rtsync_title .rtsync_host -side left -in .sync1_fr

frame .sync2_fr
label .sync2_title -text "RTSYNC port:"
entry .sync2_port -width 4 -relief sunken -bd 2 -textvariable rtsync_port
pack .sync2_title .sync2_port -side left -in .sync2_fr

button .rtsync_button -text "START" -command start_rtsync
button .rtsync_button2 -text "LINK TO MGED" -command start_vrmgr
pack .sync1_fr .sync2_fr .rtsync_button .rtsync_button2 -side top -in .rtsync_fr

proc start_rtsync {} {
	global rtsync_host
	global fbserv_resolution
	global fbserv_host
	global database_file
	global mged_treetops

	puts "start_rtsync"

	# Test access to framebuffer by clearing it.
	# On fail will pop-up an error window and abort this func.
	exec fbclear -F$fbserv_host 0 200 0

	set code "!error?"
	catch {
		exec ssh $rtsync_host \
		setenv cadroot /m/cad ";" \
		setenv TCL_LIBRARY {$cadroot/libtcl/library} ";" \
		setenv TK_LIBRARY {$cadroot/libtk/library} ";" \
		/m/cad/remrt/rtsync \
		-s$fbserv_resolution \
		-F$fbserv_host \
		$database_file $mged_treetops \
		&
	} code
	puts "exec RTSYNC result = $code"
}

proc start_vrmgr {} {
	global rtsync_host
	global mged_is_alive

	if { [catch { send mged vrmgr $rtsync_host master } status] } {
		puts "send to MGED failed, status=$status"
		puts "MGED's window needs to be opened with 'openw' command."
		return
	}
	set mged_is_alive 1
}

# Select machines to run RTNODE on
label .rtnode_title -text "RTNODE hosts:"
pack .rtnode_title -side top -in .rtnode_fr
proc register {host formalname cmd} {
	global nodes
	global	cmds

	frame .fr_$host
	checkbutton .button_$host -variable nodes($formalname)
	label .title_$host -text "$formalname $cmd"
	pack .button_$host .title_$host -side left -in .fr_$host
	pack .fr_$host -side top -in .rtnode_fr
	set cmds($formalname) $cmd
}
register "vapor" "vapor.arl.mil" ssh
register "wax" "wax.arl.mil" ssh
register wilson "wilson.arl.mil" ssh
register jewel "jewel.arl.mil" ssh
register cosm0 "cosm0.arl.hpc.mil" rsh
register cosm1 "cosm1.arl.hpc.mil" rsh
register cosm2 "cosm2.arl.hpc.mil" rsh
register cosm3 "cosm3.arl.hpc.mil" rsh
register cosm4 "cosm4.arl.hpc.mil" rsh
register cosm5 "cosm5.arl.hpc.mil" rsh
register cosm6 "cosm6.arl.hpc.mil" rsh
register cosm7 "cosm7.arl.hpc.mil" rsh
register eckert "eckert-ether.arl.hpc.mil" ssh


button .rtnode_button -text "Start NODES" -command start_nodes
pack .rtnode_button -side top -in .button_fr

proc start_nodes {} {
	global rtsync_host
	global rtsync_port
	global nodes
	global	cmds

	puts "start_nodes"
	# loop through list of nodes selected, starting each one.
	set j [array startsearch nodes]
	while { [array anymore nodes $j] } {
		set host [array nextelement nodes $j]
		if { $nodes($host) == 0 }  continue
		puts "Starting rtnode on $host"
		set code "!error?"
		catch {
			# ssh -f flag means detatch
			# ssh -n flag means take stdin from /dev/null.
			exec echo "no" | $cmds($host) $host \
				/m/cad/remrt/rtnode \
				$rtsync_host $rtsync_port &
		} code
		puts "$host results = $code"
	}
	array donesearch nodes $j
	puts "start_nodes finished"
}

puts "demo.tcl done"
