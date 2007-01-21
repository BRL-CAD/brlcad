#!/bin/sh
#                    R A Y D E B U G . T C L
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
# demo.tcl
#	setenv LD_LIBRARY_PATH /usr/lib/X11:/usr/X11/lib
#
# A GUI to drive MGED through a series of plot files.
# -Mike Muuss, ARL, July 97.
####

# The next line restarts the shell script using BRL-CAD's WISH \
exec bwish "$0" "$@"

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
