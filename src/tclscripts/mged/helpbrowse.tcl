#                   M A N . T C L
# BRL-CAD
#
# Copyright (c) 2004-2010 United States Government as represented by
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

package require Tkhtml 3.0

proc handle_select { w y } {
    set curr_sel [$w curselection]
    if { $curr_sel != "" } {
        $w selection clear $curr_sel
    }
    $w selection set [$w nearest $y]
}

proc get_html_data {cmdname} {
    global help_data

    # get file data
    set help_fd [open $cmdname]
    set help_data [read $help_fd]
    close $help_fd
}

proc re_display {w} {

    global help_data
    $w reset;
    $w configure -parsemode html
    $w parse $help_data    

}

proc mkTkImage {file} {
     set name [image create photo -file $file]
     return [list $name [list image delete $name]]
}

proc helpbrowse {} {
    global help_data

    if [winfo exists .mgedhelpbrowse] {
        catch {destroy .mgedhelpbrowse}
    }
    
	# make dialog
	toplevel .helpbrowse
	puts "made Window"
        get_html_data "/usr/brlcad/rel-7.16.2/share/brlcad/7.16.2/html/articles/en/tire.html"
    	wm title .helpbrowse "MGED Help Browser"
    	frame .helpbrowse.top -relief raised -bd 1
    	pack .helpbrowse.top -side top -expand yes -fill both
    	frame .helpbrowse.bot -relief raised -bd 1
    	pack .helpbrowse.bot -side bottom -fill both

    	# Use tkhtml3 to dispay the data
    	frame .helpbrowse.top.msgF
    	html .helpbrowse.top.msgT -yscrollcommand ".helpbrowse.top.msgS set"
    	.helpbrowse.top.msgT configure -parsemode html
        .helpbrowse.top.msgT configure -imagecmd mkTkImage
    	.helpbrowse.top.msgT parse $help_data
    	scrollbar .helpbrowse.top.msgS -command ".helpbrowse.top.msgT yview"
    	grid .helpbrowse.top.msgT .helpbrowse.top.msgS -sticky nsew -in .helpbrowse.top.msgF
    	grid columnconfigure .helpbrowse.top.msgF 0 -weight 1
    	grid rowconfigure .helpbrowse.top.msgF 0 -weight 1
    	pack .helpbrowse.top.msgF -side right -expand yes -fill both -padx 2m -pady 2m

	button .helpbrowse.bot.buttonOK -text "OK" -command "catch {destroy .helpbrowse} "
    	frame .helpbrowse.bot.default -relief sunken -bd 1
    	raise .helpbrowse.bot.buttonOK
    	pack .helpbrowse.bot.default -side left -expand yes -padx 2m -pady 1m
    	pack .helpbrowse.bot.buttonOK -in .helpbrowse.bot.default -side left -padx 1m \
              -pady 1m -ipadx 1m -ipady 1

	bind .helpbrowse <Return> "catch {destroy .helpbrowse}"
	place_near_mouse .helpbrowse   
    }

helpbrowse

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
