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
button .sense_button -text "SENSE" -command server_sense
pack .sense_button -side top -in .cmd_fr

proc server_sense {} {
	global nodes
	global	fds
	global dirs
	global status

	puts "start server_sense"
	# loop through list of nodes selected, starting each one.
	set j [array startsearch nodes]
	while { [array anymore nodes $j] } {
		set host [array nextelement nodes $j]
		puts "Sensing $host"
		set code "!error?"
		set status($host) "unknown"
		if { [ catch {
			puts $fds($host) "status"
			flush $fds($host)
			gets $fds($host) status($host)
		} code ] } {
			# error condition
			puts "$host error $code"
			unset nodes($host)
			continue
		}
		##puts "status=$status($host)"
		##set shost $nodes($host)
		##puts "cget=[.title_$shost configure]"
		##.title_$shost configure -text "$status($host)"
	}
	puts "done"
}

proc register {host formalname _cmd_ dir} {
	global nodes
	global	fds
	global dirs
	global status

	puts "Connecting to $host"
	set code "!error?"
	if { [ catch { set fds($formalname) [socket $formalname 5353] } code ] }  {
		puts "  $formalname : $code, skipping"
		return
	}

	set dirs($formalname) $dir
	set nodes($formalname) $host
	set status($formalname) "???"
	frame .fr_$host
	checkbutton .button_$host -variable nodes($formalname)
	label .title_$host -textvariable status($formalname)
	entry .entry_$host -width 20 -relief sunken -bd 2 -textvariable dirs($formalname)
	pack .button_$host .title_$host .entry_$host -side left -in .fr_$host
	pack .fr_$host -side top -in .rtnode_fr
}

register "vapor" "vapor.arl.mil" ssh /m/cad/remrt
register "wax" "wax.arl.mil" ssh /n/vapor/m/cad/remrt
server_sense
