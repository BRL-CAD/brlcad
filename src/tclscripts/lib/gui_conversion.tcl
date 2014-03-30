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

puts "Conversion process complete.  See $logfile for a log of this output."

after 7000
::tkcon::Destroy
}

proc stl_options {} {
    set w [frame .[clock seconds]]
    wm resizable . 600 600
    wm title . "STeroLithography (STL) File Importer options"

    label $w.ofl -text "Output file"
    entry $w.ofe -textvar $w -bg white
    grid $w.ofl  -column 0  -row 0  -sticky e
    grid $w.ofe  -column 1 -columnspan 2  -row 0  -sticky news

    label $w.debugl      -text "Print Debugging Info"
    checkbutton $w.debuge -variable print_debug_info
    grid $w.debugl  -column 0  -row 1  -sticky e
    grid $w.debuge  -column 1 -row 1  -sticky news

    label $w.binaryl     -text "Binary STL File"
    checkbutton $w.binarye -variable stl_binary_file
    grid $w.binaryl  -column 0  -row 2  -sticky e
    grid $w.binarye  -column 1 -row 2  -sticky news

    label $w.mindisttoll -text "Minimum Distance Between Vertices"
    entry $w.mindisttole -textvar $w -bg white
    grid $w.mindisttoll  -column 0  -row 3  -sticky e
    grid $w.mindisttole  -column 1 -columnspan 2  -row 3  -sticky news

    label $w.objnamel    -text "Object Name"
    entry $w.objnamee -textvar $w -bg white
    grid $w.objnamel  -column 0  -row 4  -sticky e
    grid $w.objnamee  -column 1 -columnspan 2  -row 4  -sticky news

    label $w.initregidl  -text "Initial Region Number"
    entry $w.initregide -textvar $w -bg white
    grid $w.initregidl  -column 0  -row 5  -sticky e
    grid $w.initregide  -column 1 -columnspan 2  -row 5  -sticky news

    label $w.globalidl   -text "Uniform Region Number"
    entry $w.globalide -textvar $w -bg white
    grid $w.globalidl  -column 0  -row 6  -sticky e
    grid $w.globalide  -column 1 -columnspan 2  -row 6  -sticky news

    label $w.gmatcodel   -text "Uniform Material Code"
    entry $w.gmatcodee -textvar $w -bg white
    grid $w.gmatcodel  -column 0  -row 7  -sticky e
    grid $w.gmatcodee  -column 1 -columnspan 2  -row 7  -sticky news

    label $w.stlunitsl   -text "STL Units"
    entry $w.stlunitse -textvar $w -bg white
    grid $w.stlunitsl  -column 0  -row 8  -sticky e
    grid $w.stlunitse  -column 1 -columnspan 2  -row 8  -sticky news

    # Application buttons
    button $w.ok     -text OK     -command {set done 1}
    button $w.c      -text Clear  -command "set $w {}"
    button $w.cancel -text Cancel -command "set $w {}; set done 1"
    grid $w.ok -column 0 -row 9 -sticky e
    grid $w.c -column 1 -row 9
    grid $w.cancel -column 2 -row 9 -sticky w
    pack $w -expand true -fill both
    vwait done
    destroy $w
    set ::$w
}

proc gui_conversion { cmd logfile } {
   package require tkcon
   ::tkcon::Init
   tkcon exec_cmd run_conversion "$cmd" "$logfile"
}

#::stl_options
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
