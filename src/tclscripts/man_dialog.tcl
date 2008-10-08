#                  M A N _ D I A L O G . T C L
# BRL-CAD
#
# Copyright (c) 1995-2008 United States Government as represented by
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
# Description -
#	"man_dialog" uses tkhtml3 to display html
#	manual pages
#
###############################################################################

if {![info exists ::tk::Priv(wait_cmd)]} {
    set ::tk::Priv(wait_cmd) tkwait
}
    
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

if {![file exists [file join $tkhtml3dir tkhtml3.$ext]]} {
     puts "ERROR: Unable to initialize tkhtml3"
     exit 1
}

load [file join $tkhtml3dir tkhtml3.$ext]

# man_dialog --
#
# Makes a dialog window with the supplied html data.
#
proc man_dialog { w screen title data default args } {
    global button$w
    global ::tk::Priv

    if [winfo exists $w] {
	catch {destroy $w}
    }



    # The screen parameter can be the pathname of some
    # widget where the screen value can be obtained.
    # Otherwise, it is assumed to be a genuine X DISPLAY
    # string.
    if [winfo exists $screen] {
	set screen [winfo screen $screen]
    }

    toplevel $w -screen $screen
    wm title $w $title
    wm iconname $w Dialog
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -expand yes -fill both
    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both

    # Use tkhtml3 to dispay the data
    frame $w.top.msgF
    html $w.top.msgT -yscrollcommand "$w.top.msgS set"
    $w.top.msgT configure -parsemode html
    $w.top.msgT parse $data
    scrollbar $w.top.msgS -command "$w.top.msgT yview"
    grid $w.top.msgT $w.top.msgS -sticky nsew -in $w.top.msgF
    grid columnconfigure $w.top.msgF 0 -weight 1
    grid rowconfigure $w.top.msgF 0 -weight 1
    pack $w.top.msgF -side right -expand yes -fill both -padx 2m -pady 2m


        set i 0
    foreach but $args {
	button $w.bot.button$i -text $but -command "set button$w $i"
	if { $i == $default } {
	    frame $w.bot.default -relief sunken -bd 1
	    raise $w.bot.button$i
	    pack $w.bot.default -side left -expand yes -padx 2m -pady 1m
	    pack $w.bot.button$i -in $w.bot.default -side left -padx 1m \
		-pady 1m -ipadx 1m -ipady 1
	} else {
	    pack $w.bot.button$i -side left -expand yes \
		-padx 2m -pady 2m -ipadx 1m -ipady 1
	}
	incr i
    }

    if { $default >= 0 } {
	bind $w <Return> "$w.bot.button$default flash ; set button$w $default"
    }

    place_near_mouse $w

    $::tk::Priv(wait_cmd) variable button$w
    catch { destroy $w }
    return [set button$w]
}


