#               M E N U _ O V E R R I D E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2007 United States Government as represented by
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
##
#				M E N U _ O V E R R I D E . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
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

proc ::tk::TraverseWithinMenu { w char } {
    if {$char == ""} {
	return
    }

    set char [string tolower $char]
    set last [$w index last]

    if {$last == "none"} {
	return
    }

    for {set i 0} {$i <= $last} {incr i} {
	if [catch {set char2 [string index  [$w entrycget $i -label]  [$w entrycget $i -underline]]}] {
	    continue
	}

	if {[string compare $char [string tolower $char2]] == 0} {
	    if {[$w type $i] == "cascade"} {
		$w activate $i
		$w postcascade active
		event generate $w <<MenuSelect>>
		set m2 [$w entrycget $i -menu]
		if {$m2 != ""} {
		    cad_MenuFirstEntry $m2
		}
	    }    else {
		::tk::MenuUnpost $w
		uplevel #0 [list $w invoke $i]
	    }
	    return
	}

    }
}

proc ::tk::MenuNextMenu {menu direction} {
    global ::tk::Priv

    # First handle traversals into and out of cascaded menus.

    if {$direction == "right"} {
	set count 1
	set parent [winfo parent $menu]
	set class [winfo class $parent]
	if {[$menu type active] == "cascade"} {
	    $menu postcascade active
	    set m2 [$menu entrycget active -menu]
	    if {$m2 != ""} {
		cad_MenuFirstEntry $m2
	    }
	    return
	} else {
	    set parent [winfo parent $menu]
	    while {($parent != ".")} {
		if {([winfo class $parent] == "Menu")
			&& ([$parent cget -type] == "menubar")} {
		    tk_menuSetFocus $parent
		    ::tk::MenuNextEntry $parent 1
		    return
		}
		set parent [winfo parent $parent]
	    }
	}
    } else {
	set count -1
	set m2 [winfo parent $menu]
	if {[winfo class $m2] == "Menu"} {
	    if {[$m2 cget -type] != "menubar"} {
		$menu activate none
		::tk::GenerateMenuSelect $menu
		tk_menuSetFocus $m2

		# This code unposts any posted submenu in the parent.

		set tmp [$m2 index active]
		$m2 activate none
		$m2 activate $tmp
		return
	    }
	}
    }

    # Can't traverse into or out of a cascaded menu.  Go to the next
    # or previous menubutton, if that makes sense.

    set m2 [winfo parent $menu]
    if {[winfo class $m2] == "Menu"} {
	if {[$m2 cget -type] == "menubar"} {
	    tk_menuSetFocus $m2
	    ::tk::MenuNextEntry $m2 -1
	    return
	}
    }

    set w $::tk::Priv(postedMb)
    if {$w == ""} {
	return
    }
    set buttons [winfo children [winfo parent $w]]
    set length [llength $buttons]
    set i [expr [lsearch -exact $buttons $w] + $count]
    while 1 {
	while {$i < 0} {
	    incr i $length
	}
	while {$i >= $length} {
	    incr i -$length
	}
	set mb [lindex $buttons $i]
	if {([winfo class $mb] == "Menubutton")
		&& ([$mb cget -state] != "disabled")
		&& ([$mb cget -menu] != "")
		&& ([[$mb cget -menu] index last] != "none")} {
	    break
	}
	if {$mb == $w} {
	    return
	}
	incr i $count
    }
    ::tk::MbPost $mb
    ::tk::MenuFirstEntry [$mb cget -menu]
}

proc ::tk::MenuNextEntry {menu count} {
    global ::tk::Priv

    if {[$menu index last] == "none"} {
	return
    }
    $menu postcascade none
    set length [expr [$menu index last]+1]
    set quitAfter $length
    set active [$menu index active]
    if {$active == "none"} {
	set i 0
    } else {
	set i [expr $active + $count]
    }
    while 1 {
	if {$quitAfter <= 0} {
	    # We've tried every entry in the menu.  Either there are
	    # none, or they're all disabled.  Just give up.

	    return
	}
	while {$i < 0} {
	    incr i $length
	}
	while {$i >= $length} {
	    incr i -$length
	}
	if {[catch {$menu entrycget $i -state} state] == 0} {
	    if {$state != "disabled"} {
		break
	    }
	}
	if {$i == $active} {
	    return
	}
	incr i $count
	incr quitAfter -1
    }
    $menu activate $i
    ::tk::GenerateMenuSelect $menu
    if {[$menu type $i] == "cascade"} {
	set cascade [$menu entrycget $i -menu]
	if {[string compare $cascade ""] != 0} {
	    $menu postcascade $i
#	    ::tk::MenuFirstEntry $cascade
	}
    }
}

proc ::tk::MenuEscape menu {
    global ::tk::Priv

    set parent [winfo parent $menu]
    if {([winfo class $parent] != "Menu")} {
	::tk::MenuUnpost $menu
    } elseif {([$parent cget -type] == "menubar")} {
	::tk::MenuUnpost $menu
	::tk::RestoreOldGrab
    } else {
	set grand_parent [winfo parent $parent]
	if {[winfo class $grand_parent] != "Menu"} {
	    ::tk::MenuUnpost $menu
	} else {
	    ::tk::MenuNextMenu $menu left
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
