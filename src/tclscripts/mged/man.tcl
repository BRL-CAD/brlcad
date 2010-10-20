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

encoding system utf-8

proc handle_select { w y } {
    set curr_sel [$w curselection]
    if { $curr_sel != "" } {
        $w selection clear $curr_sel
    }
    $w selection set [$w nearest $y]
}

proc get_html_data {cmdname} {
    global man_data

    # get file data
    set man_fd [open [bu_brlcad_data "html/mann/en/$cmdname.html"]]
    set man_data [read $man_fd]
    close $man_fd
}

proc re_display {w} {

    global man_data
    $w reset;
    $w configure -parsemode html
    $w parse $man_data    

}


proc man {cmdname} {
    global mged_gui
    global ::tk::Priv
    global mged_players
    global man_data

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


    if {![file exists [bu_brlcad_data "html/mann/en/$cmdname.html"]]} {
    	puts "No man page found for $cmdname"
	return
    } else {
	get_html_data $cmdname
	# make dialog
	toplevel .man -screen $mged_gui($_mgedFramebufferId,screen)
    	wm title .man $cmdname
    	frame .man.top -relief raised -bd 1
    	pack .man.top -side top -expand yes -fill both
    	frame .man.bot -relief raised -bd 1
    	pack .man.bot -side bottom -fill both

    	# Use tkhtml to dispay the data
    	frame .man.top.msgF
    	html .man.top.msgT -yscrollcommand ".man.top.msgS set"
    	.man.top.msgT configure -parsemode html
    	.man.top.msgT parse $man_data
    	scrollbar .man.top.msgS -command ".man.top.msgT yview"
    	grid .man.top.msgT .man.top.msgS -sticky nsew -in .man.top.msgF
    	grid columnconfigure .man.top.msgF 0 -weight 1
    	grid rowconfigure .man.top.msgF 0 -weight 1
    	pack .man.top.msgF -side right -expand yes -fill both -padx 2m -pady 2m

	frame .man.top.listing
   	scrollbar .man.top.s -command ".man.top.l yview"
    	listbox .man.top.l -bd 2 -yscroll ".man.top.s set" -width 16 -exportselection false
	grid .man.top.l .man.top.s -sticky nsew -in .man.top.listing
	grid columnconfigure .man.top.listing 0 -weight 0
	grid rowconfigure .man.top.listing 0 -weight 1
	set cmdfiles [glob -directory [bu_brlcad_data "html/mann/en"] *.html ]
	set cmds [list ]
	foreach cmdfile $cmdfiles {
	   regexp {(.+/)(.+)(.html)} $cmdfile -> url cmdrootname htmlsuffix 
           if {[string compare $cmdrootname "Introduction"]} {
	      set cmds [concat $cmds [list $cmdrootname]]
           }
	}
	set cmds [lsort $cmds]
	foreach cmd $cmds {
           .man.top.l insert end $cmd
	}
  	pack .man.top.listing -side left -expand no -fill y

    	button .man.bot.buttonOK -text "OK" -command "catch {destroy .man} "
    	frame .man.bot.default -relief sunken -bd 1
    	raise .man.bot.buttonOK
    	pack .man.bot.default -side left -expand yes -padx 2m -pady 1m
    	pack .man.bot.buttonOK -in .man.bot.default -side left -padx 1m \
              -pady 1m -ipadx 1m -ipady 1

	bind .man.top.l <Button-1> {handle_select %W %y; get_html_data [%W get [%W curselection]]; re_display .man.top.msgT}
	
	bind .man <Return> "catch {destroy .man}"
	place_near_mouse .man   
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
