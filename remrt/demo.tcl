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

tk appname demo

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
label .title3 -text "(A SIMTECH Project)"
pack .title1 .title2 .title3 -side top -in .words_fr
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
set database_file ../.db.6d/world.g
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
		/m/cad/.remrt.6d/rtsync \
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


proc find_db {} {
	global nodes
	global	fds
	global db_path
	global status

	# loop through list of nodes selected, starting each one.
	set j [array startsearch nodes]
	while { [array anymore nodes $j] } {
		set host [array nextelement nodes $j]
		if { $fds($host) == "dead" } continue
		puts "Finding $host"
		set code "!error?"
		if { [ catch {
			puts $fds($host) "find .db.6d/moss.g"
			flush $fds($host)
		} code ] } {
			# error condition on write
			puts "$host error $code"

			close $fds($host)
			set fds($host) "dead"
			set status($host) "$host -died-"
			continue
		}
		if { [gets $fds($host) reply] <= 0 } {
			puts "EOF from $host"
			close $fds($host)
			set fds($host) "dead"
			set status($host) "$host -died-"
			set code "eof"
			continue
		}
		# First word should be OK or FAIL, 2nd word is path.
		if { [lindex $reply 0] != "OK" }  {
			continue
		}
		set db_path($host) [lindex $reply 1]
	}
	array donesearch nodes $j
}

proc server_sense {host} {
	global nodes
	global	fds
	global db_path
	global status

	if { $fds($host) == "dead" } continue
	puts "Sensing $host"
	set code "!error?"
	if { [ catch {
		puts $fds($host) "status"
		flush $fds($host)
	} code ] } {
		# error condition on write
		puts "$host error $code"

		close $fds($host)
		set fds($host) "dead"
		set status($host) "$host -died-"
		return
	}
	if { [gets $fds($host) status($host)] <= 0 } {
		puts "EOF from $host"
		close $fds($host)
		set fds($host) "dead"
		set status($host) "$host -died-"
		set code "eof"
		return
	}
}

proc sense_servers {} {
	global nodes
	global	fds
	global db_path
	global status

	# loop through list of nodes selected, starting each one.
	set j [array startsearch nodes]
	while { [array anymore nodes $j] } {
		set host [array nextelement nodes $j]
		if { $fds($host) == "dead" } continue
		server_sense $host
		##.title_$shost configure -text "$status($host)"
	}
	array donesearch nodes $j
}

# Only attempt reconnection to nodes which are presently not connected.
proc reconnect {} {
	global nodes
	global	fds
	global db_path
	global status

	set j [array startsearch nodes]
	while { [array anymore nodes $j] } {
		set host [array nextelement nodes $j]
		if { $fds($host) != "dead" }  continue

		puts "Connecting to $host"
		set code "!error?"
		if { [ catch { set fds($host) [socket $host 5353] } code ] }  {
			set fds($host) dead
			set status($host) "$host not-responding"
			puts "  $host : $code, skipping"
			continue
		}
		# fds($host) is now set non-dead, to the actual fd.
		# establish per-connection state
		set db_path($host) "unknown"

		server_sense $host
	}
	array donesearch nodes $j
}

proc register {informal_name formalname} {
	global nodes
	global	fds
	global db_path
	global status

	# Establish connection-invarient state
	set nodes($formalname) 0
	set fds($formalname) "dead"
	set status($formalname) "$formalname : pre-natal"

	frame .fr_$informal_name
	checkbutton .button_$informal_name -variable nodes($formalname)
	label .title_$informal_name -textvariable status($formalname)
	entry .entry_$informal_name -width 20 -relief sunken -bd 2 -textvariable db_path($formalname)
	pack .button_$informal_name .title_$informal_name .entry_$informal_name -side left -in .fr_$informal_name
	pack .fr_$informal_name -side top -in .rtnode_fr
}

register "vapor" "vapor-uni0.arl.mil"
register "wax" "wax-uni0.arl.mil"
register wilson "wilson-uni0.arl.mil"
register jewel "jewel-atm.arl.mil"
register cosm0 "cosm0-atm.arl.hpc.mil"
register cosm1 "cosm1-atm.arl.hpc.mil"
register cosm2 "cosm2-atm.arl.hpc.mil"
register cosm3 "cosm3-atm.arl.hpc.mil"
register cosm4 "cosm4-atm.arl.hpc.mil"
register cosm5 "cosm5-atm.arl.hpc.mil"
register cosm6 "cosm6-atm.arl.hpc.mil"
register cosm7 "cosm7-atm.arl.hpc.mil"
register eckert "eckert-atm.arl.hpc.mil"
register toltec "toltec.nvl.army.mil"
#register olmec "olmec.nvl.army.mil"		# Only 1 200Mhz cpu
register octopus "octopus.nvl.army.mil"

frame .button1_fr
frame .button2_fr
button .sense_button -text "SENSE" -command sense_servers
button .find_button -text "FIND_DB" -command find_db
button .rtnode_button -text "Start NODES" -command start_nodes
pack .sense_button .find_button .rtnode_button -side left -in .button1_fr
button .reconnect_button -text "RECONNECT" -command reconnect
button .restart_button -text "(Restart RTMON)" -command restart_rtmon
pack .reconnect_button .restart_button -side left -in .button2_fr
pack .button1_fr .button2_fr -side top -in .button_fr

proc restart_rtmon {} {
	global nodes
	global	fds
	global status

	puts "restart_rtmon -- all RTSYNC processes had better be dead first."
	# loop through list of nodes selected, starting each one selected.
	set j [array startsearch nodes]
	while { [array anymore nodes $j] } {
		set host [array nextelement nodes $j]
		if { $fds($host) == "dead" }  continue

		puts "restarting $host"
		catch { puts $fds($host) "restart"; flush $fds($host); close $fds($host) }

		set fds($host) "dead"
		set status($host) "$host -restarted-"
		set nodes($host) 0
	}
	array donesearch nodes $j
	puts "restart_rtmon finished"
}

proc start_nodes {} {
	global rtsync_host
	global rtsync_port
	global nodes
	global	fds
	global db_path
	global status

	puts "start_nodes"
	# loop through list of nodes selected, starting each one selected.
	set j [array startsearch nodes]
	while { [array anymore nodes $j] } {
		set host [array nextelement nodes $j]
		if { $fds($host) == "dead" }  continue
		if { $nodes($host) == 0 }  continue
		puts "Starting rtnode on $host"
		set code "!error?"
		if { [ catch {
			puts $fds($host) "rtnode $rtsync_host $rtsync_port"
			flush $fds($host)
		} code ] } {
			# error condition on write
			puts "$host error $code"

			close $fds($host)
			set fds($host) "dead"
			set status($host) "$host -died-"
			set nodes($host) 0
			continue
		}
		if { [gets $fds($host) reply] <= 0 } {
			puts "EOF from $host"
			close $fds($host)
			set fds($host) "dead"
			set status($host) "$host -died-"
			set nodes($host) 0
			continue
		}
		# First word should be OK or FAIL, 2nd word is path.
		if { [lindex $reply 0] != "OK" }  {
			puts "$host $reply"
			set status($host) "$host -can't-exec-"
			set nodes($host) 0
			continue
		}
		set nodes($host) 0
	}
	array donesearch nodes $j
	puts "start_nodes finished"
}

# main()
# Automatically try to contact all the registered servers
reconnect

puts "demo.tcl done"


## Todo:  send change directory command.
##  Maybe give a sequence of 'em.
# how about a "re-read 'register' list cmd, by grepping through tcl src.
