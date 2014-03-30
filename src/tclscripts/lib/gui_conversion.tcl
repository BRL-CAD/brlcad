#              G U I _ C O N V E R S I O N . T C L
# BRL-CAD
#
# Copyright (c) 2014 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
#  These routines provide a pop-up Tk dialog that streams the ouput
#  from geometry conversions such as step-g and stl-g.
#

proc do_output { channel logID } {
    # The channel is readable; try to read it.
    set status [catch { gets $channel line } result]
    if { $status != 0 } {
         # Error on the channel
         puts "error reading $channel: $result"
         set ::conversion_complete 2
    } elseif { $result >= 0 } {
         # Successfully read the channel
         puts "$line"
	 puts $logID $line
	 flush $logID
    } elseif { [eof $channel] } {
         # End of file on the channel
         set ::conversion_complete 1
    } elseif { [fblocked $channel] } {
         # Not right now, come back later...
    } else {
         # Something unexpected just happened...
         set ::conversion_complete 3
    }
}

proc run_conversion { cmd logfile } {

set conv_log_id [open $logfile "w"]

# Open a pipe
set conversion_process [open "|$cmd"]

# Set up to deliver file events on the pipe
fconfigure $conversion_process -blocking false
fileevent $conversion_process readable [list do_output $conversion_process $conv_log_id]

# Launch the event loop and wait for the file events to finish
vwait ::conversion_complete

# Close the pipe
close $conversion_process
close $conv_log_id

puts "Conversion process complete.  See ??? for a log of this output."

after 7000
::tkcon::Destroy
}

proc gui_conversion { cmd logfile } {
   puts $cmd
   package require tkcon
   ::tkcon::Init
   tkcon exec_cmd run_conversion "$cmd" "$logfile"
}

set argv1 [lindex [lindex $argv 0] 0]
set argv2 [lindex [lindex $argv 1] 0]
gui_conversion $argv1 $argv2

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
