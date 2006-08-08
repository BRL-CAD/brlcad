#!/bin/sh
# The next line restarts the shell script using best WISH in $PATH
# exec bwish "$0" "$@"
# demo.tcl
#	setenv LD_LIBRARY_PATH /usr/lib/X11:/usr/X11/lib
#
# A GUI to drive MGED through a series of plot files.
# -Mike Muuss, ARL, July 97.

puts "running raydebug.tcl"

# Create main interaction widget
frame .cmd_fr; pack .cmd_fr -side top
frame .number_fr; pack .number_fr -side top

# "Advance" button
button .advance_button -text "Advance" -command do_advance
button .reverse_button -text "Reverse" -command do_reverse
pack .advance_button .reverse_button -side left -in .cmd_fr

set num -1
label .number -textvariable num
button .reset_button -text "Reset" -command {set num -1; do_advance}
pack .number .reset_button -side left -in .number_fr

puts "mged_sense"
if { [catch { send mged echo NIL } status] } {
	puts "send to MGED failed, status=$status"
	puts "MGED's window needs to be opened with 'openw' command."
	puts "or issue 'tk appname mged' to MGED."
	puts "Also, check your DISPLAY = $env(DISPLAY)"
	exit
}

proc do_advance {} {
	global num

	incr num
	send mged "Z; overlay cell$num.pl"
}

proc do_reverse {} {
	global num

	incr num -1
	send mged "Z; overlay cell$num.pl"
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
