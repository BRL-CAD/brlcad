#                        M E N U . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
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
#	The routines herein are used to implement traditional MGED
#	menus in Tcl/Tk.
#
# Modifications -
#	Bob Parker:
#               *- use grid instead of pack
#		*- Generalized the code to accommodate multiple instances
#		   of the classic MGED "Button Menu" interface.

namespace eval ::brlcad::mged::menu {
    variable state
}

proc ::brlcad::mged::menu::install {id i} {
    variable state

    set w .mmenu$id

    if {![llength [info commands ::winfo]] || ![winfo exists $w]} {
	return
    }

    set result [catch {::brlcad::mged mmenu_get $i} menu]
    if {$result != 0} {
	return
    }

    set state($id,$i) $menu

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

proc ::brlcad::mged::menu::initialize {id} {
    variable state
    global mged_gui
    global mged_display

    ::brlcad::mged cmd_win set $id
    set w .mmenu$id
    catch { destroy $w }
    toplevel $w -screen $mged_gui($id,screen)

    label $w.state -textvariable mged_display(state)
    grid $w.state -row 0

    set state($id,num) 3

    for {set i 0} {$i < $state($id,num)} {incr i} {
	frame $w.f$i -relief raised -bd 1
	listbox $w.f$i.l -bd 2 -exportselection false
	grid $w.f$i.l -sticky nsew -row 0 -column 0
	grid columnconfigure $w.f$i 0 -weight 1
	grid rowconfigure $w.f$i 0 -weight 1

	bind $w.f$i.l <Button-1> \
	    [list ::brlcad::mged::menu::select $id %W %y]
	bind $w.f$i.l <Button-2> \
	    [list ::brlcad::mged::menu::select $id %W %y]

	::brlcad::mged::menu::install $id $i
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

proc ::brlcad::mged::menu::select {id w y} {
    ::handle_select $w $y
    ::brlcad::mged::menu::press $id $w
    return -code break
}

proc ::brlcad::mged::menu::press {id w} {
    ::brlcad::mged cmd_win set $id
    ::brlcad::mged press [$w get [$w curselection]]
}

proc ::brlcad::mged::menu::reconfigure {id} {
    variable state

    if {![winfo exists .mmenu$id]} {
	return
    }

    set w .mmenu$id

    for {set i 0} {$i < $state($id,num)} {incr i} {
	::brlcad::mged::menu::install $id $i
    }
}

# Compatibility entry points for scripts and interactive use.
proc ::mmenu_set {args} {
    tailcall ::brlcad::mged::menu::install {*}$args
}
proc ::mmenu_init {args} {
    tailcall ::brlcad::mged::menu::initialize {*}$args
}
proc ::mged_press {args} {
    tailcall ::brlcad::mged::menu::press {*}$args
}
proc ::reconfig_mmenu {args} {
    tailcall ::brlcad::mged::menu::reconfigure {*}$args
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
