#                        M E N U . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Description -
#	The routines herein are used to implement traditional MGED
#	menus in Tcl/Tk.
#
# Modifications -
#	Bob Parker:
#               *- use grid instead of pack
#		*- Generalized the code to accommodate multiple instances
#		   of the classic MGED "Button Menu" interface.

proc mmenu_set { id i } {
    global mmenu

    set w .mmenu$id

    if {![winfo exists $w]} {
	return
    }

    set result [catch {mmenu_get $i} menu]
    if {$result != 0} {
	return
    }

    set mmenu($id,$i) $menu

    if { [llength $menu]<=0 } {
	grid forget $w.f$i
	return
    }

    $w.f$i.l delete 0 end
    foreach item $menu {
	$w.f$i.l insert end $item
    }
    $w.f$i.l configure -height [llength $menu]

    set row [expr $i + 1]
    grid $w.f$i -row $row -sticky nsew
}

proc mmenu_init { id } {
    global mmenu
    global mged_gui
    global mged_display

    cmd_win set $id
    set w .mmenu$id
    catch { destroy $w }
    toplevel $w -screen $mged_gui($id,screen)

    label $w.state -textvariable mged_display(state)
    grid $w.state -row 0
    
    set mmenu($id,num) 3

    for { set i 0 } { $i < $mmenu($id,num) } { incr i } {
	frame $w.f$i -relief raised -bd 1
	listbox $w.f$i.l -bd 2 -exportselection false
        grid $w.f$i.l -sticky nsew -row 0 -column 0
	grid columnconfigure $w.f$i 0 -weight 1
	grid rowconfigure $w.f$i 0 -weight 1

	bind $w.f$i.l <Button-1> "handle_select %W %y; mged_press $id %W; break"
	bind $w.f$i.l <Button-2> "handle_select %W %y; mged_press $id %W; break"

	mmenu_set $id $i
    }

    grid columnconfigure $w 0 -weight 1
    grid rowconfigure $w 1 -weight 1
    grid rowconfigure $w 2 -weight 1
    grid rowconfigure $w 3 -weight 1

    wm title $w "MGED Button Menu ($id)"
    wm protocol $w WM_DELETE_WINDOW "toggle_button_menu $id"
    wm resizable $w 0 0

    return
}

proc mged_press { id w } {
    cmd_win set $id
    press [$w get [$w curselection]]
}

proc reconfig_mmenu { id } {
    global mmenu

    if {![winfo exists .mmenu$id]} {
	return
    }

    set w .mmenu$id

    for { set i 0 } { $i < $mmenu($id,num) } { incr i } {
	mmenu_set $id $i
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
