#!/m/cad/.tk.6d/wish mon.tcl
#!/bin/sh
# The next line restarts the shell script using best WISH in $PATH
# exec wish "$0" "$@"
# demo.tcl
#	setenv LD_LIBRARY_PATH /usr/lib/X11:/usr/X11/lib
#
# A GUI to monitor the status of available servers.
# -Mike Muuss, ARL, July 97.

puts "running mon.tcl"

# Create main interaction widget
frame .cmd_fr; pack .cmd_fr -side top
frame .rtnode_fr; pack .rtnode_fr -side top

# "Sense" button
button .sense_button -text "SENSE" -command sense_servers
button .reconnect_button -text "RECONNECT" -command reconnect
button .find_button -text "FIND_DB" -command find_db
pack .sense_button .reconnect_button .find_button -side left -in .cmd_fr

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
		puts "First word is [lindex $reply 0]"
		if { [lindex $reply 0] != "OK" }  {
			continue
		}
		set db_path($host) [lindex $reply 1]
	}
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
}

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
		set db_path($formalname) "unknown"

		server_sense $host
	}
}

proc register {informal_name formalname _cmd_ dir} {
	global nodes
	global	fds
	global db_path
	global status

	# Establish connection-invarient state
	set nodes($formalname) $informal_name
	set fds($formalname) "dead"
	set status($formalname) "$formalname : pre-natal"

	frame .fr_$informal_name
	checkbutton .button_$informal_name -variable nodes($formalname)
	label .title_$informal_name -textvariable status($formalname)
	entry .entry_$informal_name -width 20 -relief sunken -bd 2 -textvariable db_path($formalname)
	pack .button_$informal_name .title_$informal_name .entry_$informal_name -side left -in .fr_$informal_name
	pack .fr_$informal_name -side top -in .rtnode_fr
}

register "vapor" "vapor.arl.mil" ssh /m/cad/remrt
register "wax" "wax.arl.mil" ssh /n/vapor/m/cad/remrt
register "eckert" "eckert.arl.hpc.mil" ssh /n/vapor/m/cad/remrt
reconnect
