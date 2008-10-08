#!/usr/bin/env tclsh
#                   M A N . T C L
# BRL-CAD
#
# Copyright (c) 2004-2008 United States Government as represented by
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

if {$tcl_platform(platform) == "windows"} { 
    set ext "dll"
    set tkimgdir [bu_brlcad_root "bin"]
} else {
    set ext "so"
    set tkhtml3dir [bu_brlcad_root "lib"]
        if {![file exists $tkhtml3dir]} {
            set tkhtml3dir [file join [bu_brlcad_data "src"] other tkhtml3 .libs]
        }
    }

load [file join $tkhtml3dir tkhtml3.$ext]


proc man {cmdname} {
    global mged_gui
    global ::tk::Priv
    global mged_players

    if [winfo exists .man] {
        catch {destroy .man}
    }


    # determine the framebuffer window id
    if { [ catch { set mged_players } _mgedFramebufferId ] } {
	puts $_mgedFramebufferId
	puts "assuming default mged framebuffer id: id_0"
	set _mgedFramebufferId "id_0"
    }
    # just in case there are more than one returned
    set _mgedFramebufferId [ lindex $_mgedFramebufferId 0 ]


    if {![file exists [bu_brlcad_data "html/man1/en/$cmdname.html"]]} {
    	puts "No man page found for $cmdname"
	return
    } else {
	# get file data
        set man_fd [open [bu_brlcad_data "html/man1/en/$cmdname.html"]]
        set man_data [read $man_fd]
        close $man_fd
	# make dialog
	toplevel .man -screen $mged_gui($_mgedFramebufferId,screen)
    	wm title .man $cmdname
    	frame .man.top -relief raised -bd 1
    	pack .man.top -side top -expand yes -fill both
    	frame .man.bot -relief raised -bd 1
    	pack .man.bot -side bottom -fill both

    	# Use tkhtml3 to dispay the data
    	frame .man.top.msgF
    	html .man.top.msgT -yscrollcommand ".man.top.msgS set"
    	.man.top.msgT configure -parsemode html
    	.man.top.msgT parse $man_data
    	scrollbar .man.top.msgS -command ".man.top.msgT yview"
    	grid .man.top.msgT .man.top.msgS -sticky nsew -in .man.top.msgF
    	grid columnconfigure .man.top.msgF 0 -weight 1
    	grid rowconfigure .man.top.msgF 0 -weight 1
    	pack .man.top.msgF -side right -expand yes -fill both -padx 2m -pady 2m

    	button .man.bot.buttonOK -text "OK" -command "set button.man OK"
    	frame .man.bot.default -relief sunken -bd 1
    	raise .man.bot.buttonOK
    	pack .man.bot.default -side left -expand yes -padx 2m -pady 1m
    	pack .man.bot.buttonOK -in .man.bot.default -side left -padx 1m \
              -pady 1m -ipadx 1m -ipady 1
	   
	# Make sure the button closes the window
    	bind .man <Return> "catch { destroy .man }"
    	bind .man <1> "catch { destroy .man }"
    	bind .man <2> "catch { destroy .man }"

    }
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
