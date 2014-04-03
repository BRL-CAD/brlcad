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
#  These routines provide a pop-up Tk dialog that streams the output
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

# Echo the conversion command to be run
puts $cmd
puts $conv_log_id $cmd
flush $conv_log_id

# Open a pipe
set conversion_process [open "|$cmd 2>@1"]

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

#################################################################
#
#   Rhinoceros 3D (a.k.a Rhino) / OpenNURBS  3dm specific logic
#
#################################################################

proc rhino_options {} {
    set w [frame .[clock seconds]]
    wm resizable . 800 600
    wm title . "Rhinoceros 3D (3dm) File Importer options"

    label $w.ofl -text "Output file"
    entry $w.ofe -textvariable ::output_file -width 40 -bg white
    grid $w.ofl  -column 0  -row 0  -sticky e
    grid $w.ofe  -column 1 -columnspan 2  -row 0  -sticky news

    label $w.debugl      -text "Print Debugging Info"
    checkbutton $w.debuge -variable ::print_debug_info
    grid $w.debugl  -column 0  -row 1  -sticky e
    grid $w.debuge  -column 1 -row 1  -sticky news

    label $w.verbosityl     -text "Printing Verbosity"
    checkbutton $w.verbositye -variable ::printing_verbosity
    grid $w.verbosityl  -column 0  -row 2  -sticky e
    grid $w.verbositye  -column 1 -row 2  -sticky news

    label $w.scalefactorl -text "Scaling Factor"
    entry $w.scalefactore -textvariable ::scale_factor -bg white
    grid $w.scalefactorl  -column 0  -row 3  -sticky e
    grid $w.scalefactore  -column 1 -columnspan 2  -row 3  -sticky news

    label $w.tolerancel    -text "Tolerance"
    entry $w.tolerancee -textvariable ::tolerance -bg white
    grid $w.tolerancel  -column 0  -row 4  -sticky e
    grid $w.tolerancee  -column 1 -columnspan 2  -row 4  -sticky news

    label $w.randomcolorl  -text "Randomize Colors of Imported Objects"
    checkbutton $w.randomcolore -variable ::randomize_color
    grid $w.randomcolorl  -column 0  -row 5  -sticky e
    grid $w.randomcolore  -column 1 -columnspan 2  -row 5  -sticky news

    label $w.uuidl   -text "Use Universally Unique IDentifiers\n(UUIDs) for Object Names"
    checkbutton $w.uuide -variable ::use_uuids
    grid $w.uuidl  -column 0  -row 6  -sticky e
    grid $w.uuide  -column 1 -columnspan 2  -row 6  -sticky news

    label $w.brlcadcompl   -text "Generate BRL-CAD Compliant Names"
    checkbutton $w.brlcadcompe -variable ::brlcad_names
    grid $w.brlcadcompl  -column 0  -row 7  -sticky e
    grid $w.brlcadcompe  -column 1 -columnspan 2  -row 7  -sticky news

    label $w.oll -text "Conversion Log file"
    entry $w.ole -textvariable ::log_file -bg white
    grid $w.oll  -column 0  -row 8  -sticky e
    grid $w.ole  -column 1 -columnspan 2  -row 8  -sticky news

    # Application buttons
    button $w.ok     -text OK     -command {set done 1}
    button $w.c      -text Clear  -command "set $w {}"
    button $w.cancel -text Cancel -command "set $w {}; set ::cancel_process 1; set done 1"
    grid $w.ok -column 0 -row 9 -sticky es
    grid $w.c -column 1 -row 9 -sticky s
    grid $w.cancel -column 2 -row 9 -sticky w

    grid columnconfigure $w 1 -weight 1

    pack $w -expand true -fill x
    vwait done

    destroy $w
}

# For 3dm-g, it's options first, then output file, then input file
proc ::rhino_build_cmd {} {
    set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] 3dm-g$::exe_ext]]]

    if {$::print_debug_info == 1} {
       append cmd " -d" { }
    }
    if {[llength "$::printing_verbosity"] > 0} {
       append cmd " -v $::printing_verbosity" { }
    }
    if {[llength "$::scale_factor"] > 0} {
       append cmd " -s $::scale_factor" { }
    }
    if {[llength "$::tolerance"] > 0} {
       append cmd " -t $::tolerance" { }
    }
    if {$::randomize_color == 1} {
       append cmd " -r" { }
    }
    if {$::use_uuids == 1} {
       append cmd " -u" { }
    }
    if {$::brlcad_names == 1} {
       append cmd " -c" { }
    }

    append cmd " -o $::output_file" { }

    append cmd " $::input_file" { }

    set ::rhino_cmd $cmd
}


#################################################################
#
#            STerolithography (STL) specific logic
#
#################################################################

proc stl_options {} {
    set w [frame .[clock seconds]]
    wm resizable . 800 600
    wm title . "STeroLithography (STL) File Importer options"

    label $w.ofl -text "Output file"
    entry $w.ofe -textvariable ::output_file -width 40 -bg white
    grid $w.ofl  -column 0  -row 0  -sticky e
    grid $w.ofe  -column 1 -columnspan 2  -row 0  -sticky news

    label $w.debugl      -text "Print Debugging Info"
    checkbutton $w.debuge -variable ::print_debug_info
    grid $w.debugl  -column 0  -row 1  -sticky e
    grid $w.debuge  -column 1 -row 1  -sticky news

    label $w.binaryl     -text "Binary STL File"
    checkbutton $w.binarye -variable ::stl_binary_file
    grid $w.binaryl  -column 0  -row 2  -sticky e
    grid $w.binarye  -column 1 -row 2  -sticky news

    label $w.mindisttoll -text "Minimum Distance Between Vertices"
    entry $w.mindisttole -textvariable ::min_dist_tol -bg white
    grid $w.mindisttoll  -column 0  -row 3  -sticky e
    grid $w.mindisttole  -column 1 -columnspan 2  -row 3  -sticky news

    label $w.objnamel    -text "Object Name"
    entry $w.objnamee -textvariable ::object_name -bg white
    grid $w.objnamel  -column 0  -row 4  -sticky e
    grid $w.objnamee  -column 1 -columnspan 2  -row 4  -sticky news

    label $w.initregidl  -text "Initial Region Number"
    entry $w.initregide -textvariable ::initial_region_number -bg white
    grid $w.initregidl  -column 0  -row 5  -sticky e
    grid $w.initregide  -column 1 -columnspan 2  -row 5  -sticky news

    label $w.globalidl   -text "Uniform Region Number"
    entry $w.globalide -textvariable ::uniform_region_number -bg white
    grid $w.globalidl  -column 0  -row 6  -sticky e
    grid $w.globalide  -column 1 -columnspan 2  -row 6  -sticky news

    label $w.gmatcodel   -text "Uniform Material Code"
    entry $w.gmatcodee -textvariable ::uniform_material_code -bg white
    grid $w.gmatcodel  -column 0  -row 7  -sticky e
    grid $w.gmatcodee  -column 1 -columnspan 2  -row 7  -sticky news

    label $w.stlunitsl   -text "STL Units"
    entry $w.stlunitse -textvariable ::input_file_units -bg white
    grid $w.stlunitsl  -column 0  -row 8  -sticky e
    grid $w.stlunitse  -column 1 -columnspan 2  -row 8  -sticky news

    label $w.oll -text "Conversion Log file"
    entry $w.ole -textvariable ::log_file -bg white
    grid $w.oll  -column 0  -row 9  -sticky e
    grid $w.ole  -column 1 -columnspan 2  -row 9  -sticky news


    # Application buttons
    button $w.ok     -text OK     -command {set done 1}
    button $w.c      -text Clear  -command "set $w {}"
    button $w.cancel -text Cancel -command "set $w {}; set ::cancel_process 1; set done 1"
    grid $w.ok -column 0 -row 10 -sticky es
    grid $w.c -column 1 -row 10 -sticky s
    grid $w.cancel -column 2 -row 10 -sticky w

    grid columnconfigure $w 1 -weight 1

    pack $w -expand true -fill x
    vwait done

    destroy $w
}

# For stl-g, it's options first, then input file, then output file
proc ::stl_build_cmd {} {
    set cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] stl-g$::exe_ext]]]

    if {$::print_debug_info == 1} {
       append cmd " -d" { }
    }
    if {$::stl_binary_file == 1} {
       append cmd " -b" { }
    }
    if {[llength "$::min_dist_tol"] > 0} {
       append cmd " -t $::min_dist_tol" { }
    }
    if {[llength "$::object_name"] > 0} {
       append cmd " -N $::object_name" { }
    }
    if {[llength "$::initial_region_number"] > 0} {
       append cmd " -i $::initial_region_number" { }
    }
    if {[llength "$::uniform_region_number"] > 0} {
       append cmd " -I $::uniform_region_number" { }
    }
    if {[llength "$::uniform_material_code"] > 0} {
       append cmd " -m $::uniform_material_code" { }
    }
    if {[llength "$::input_file_units"] > 0} {
       append cmd " -c $::input_file_units" { }
    }

    append cmd " $::input_file" { }

    append cmd " $::output_file" { }

    set ::stl_cmd $cmd
}


proc gui_conversion { cmd logfile } {
#close $::info_id
   package require tkcon
   ::tkcon::Init
   tkcon exec_cmd run_conversion "$cmd" "$logfile"
}

#set ::info_id [open /home/cyapp/convdebug.log "w"]
set ::exe_ext ""
if {$tcl_platform(platform) == "windows"} {
    set ::exe_ext ".exe"
}
set ::input_file  [lindex $argv 0]
set ::log_file [lindex $argv 1]
set ::input_ext [file extension $::input_file]
set ::input_root [file rootname [file tail $::input_file]]
set ::input_dir [file dirname $::input_file]
set ::output_file [file join [file dirname $::input_file] "$input_root.g"]
if {[llength $::log_file] == 0} {
   set ::log_file [file join [file dirname $::input_file] "$input_root.log"]
}
set ::cancel_process 0

switch -nocase "$::input_ext" {
    ".3dm" {
	::rhino_options
	::rhino_build_cmd

        if {$::cancel_process == 1} {exit 0}
        gui_conversion $rhino_cmd $::log_file
    }
    ".stl" {
	::stl_options
        ::stl_build_cmd
        if {$::cancel_process == 1} {exit 0}
        gui_conversion $::stl_cmd $::log_file
    }
    ".stp" {
	set step_cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] step-g$::exe_ext]] \
	                    -v -o $::output_file \
			    $::input_file]
        gui_conversion $step_cmd $::log_file
    }
    ".step" {
	set step_cmd [list [bu_brlcad_root [file join [bu_brlcad_dir bin] step-g$::exe_ext]] \
	                    -v -o $::output_file \
			    $::input_file]
        gui_conversion $step_cmd $::log_file
    }
    default {
	exit 1
    }
}

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
