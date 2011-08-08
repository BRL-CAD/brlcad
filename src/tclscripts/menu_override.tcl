#               M E N U _ O V E R R I D E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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
#	The proc's below were copied from libtk/menu.tcl and modified.
#

bind Menu <ButtonRelease> {
    cad_MenuInvoke %W %b
}

# cad_MenuInvoke --
#
# This procedure is invoked when a button is released over a menu.
# It invokes the appropriate menu action and unposts the menu if
# it came from a menubutton.
#
# Arguments:
# w -			Name of the menu widget.
# button -		Button that was released.
#
proc cad_MenuInvoke { w button } {
    global ::tk::Priv

    if { ([lsearch -exact [array name ::tk::Priv] window] != -1) && $::tk::Priv(window) == ""} {
	# Mouse was pressed over a menu without a menu button, then
	# dragged off the menu (possibly with a cascade posted) and
	# released.  Unpost everything and quit.

	$w postcascade none
	$w activate none
	event generate $w <<MenuSelect>>
	::tk::MenuUnpost $w
	return
    }
    if {[$w type active] == "cascade"} {
	$w postcascade active
	set menu [$w entrycget active -menu]
	::tk::MenuFirstEntry $menu
    } elseif {[$w type active] == "tearoff"} {
	::tk::MenuUnpost $w
	::tk::TearOffMenu $w
    } elseif {[$w cget -type] == "menubar"} {
	$w postcascade none
	$w activate none
	event generate $w <<MenuSelect>>
	::tk::MenuUnpost $w
    } else {
	::tk::MenuUnpost $w

	if {$button == 3} {
	    hoc_menu_callback $w
	} else {
	    uplevel #0 [list $w invoke active]
	}
    }
}

proc cad_MenuFirstEntry { menu } {
    if {$menu == ""} {
	return
    }

    tk_menuSetFocus $menu

    if {[$menu index active] != "none"} {
	return
    }

    set last [$menu index last]

    if {$last == "none"} {
	return
    }

    for {set i 0} {$i <= $last} {incr i} {
	if {([catch {set state [$menu entrycget $i -state]}] == 0)
	    && ($state != "disabled") && ([$menu type $i] != "tearoff")} {
	    $menu activate $i
	    ::tk::GenerateMenuSelect $menu
	    if {[$menu type $i] == "cascade"} {
		set cascade [$menu entrycget $i -menu]
		if {[string compare $cascade ""] != 0} {
		    $menu postcascade $i
		}
	    }
	    return
	}
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
