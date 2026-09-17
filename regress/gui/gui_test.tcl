#               G U I _ T E S T . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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
source [file join [file dirname [info script]] gui_test_core.tcl]

namespace eval ::gui::test {
    variable background_errors {}
    variable numeric_tolerance 1.0e-12
}

proc ::gui::test::record_background_error {message} {
    variable background_errors
    set error_info ""
    set error_code ""
    if {[info exists ::errorInfo]} {
	set error_info $::errorInfo
    }
    if {[info exists ::errorCode]} {
	set error_code $::errorCode
    }
    lappend background_errors [list $message $error_info $error_code]
}

proc ::gui::test::capture_background_errors {} {
    variable background_errors
    set background_errors {}
    proc ::bgerror {message} {
	::gui::test::record_background_error $message
    }
}

proc ::gui::test::require_no_background_errors {} {
    variable background_errors
    if {[llength $background_errors] == 0} {
	return
    }

    set report {}
    foreach background_error $background_errors {
	lassign $background_error message error_info error_code
	lappend report $message
	if {$error_code ne "" && $error_code ne "NONE"} {
	    lappend report "error code: $error_code"
	}
	if {$error_info ne "" && $error_info ne $message} {
	    lappend report $error_info
	}
    }
    error "background Tcl error(s):\n[join $report \n]"
}

proc ::gui::test::check_background_errors {status message} {
    if {$status == 0 &&
	[catch {require_no_background_errors} background_error]} {
	return [list 1 "FAIL: $background_error"]
    }
    return [list $status $message]
}

proc ::gui::test::require {condition message} {
    if {![uplevel 1 [list expr $condition]]} {
	error $message
    }
}

proc ::gui::test::equivalent_values {first second} {
    variable numeric_tolerance
    if {$first eq $second} {
	return 1
    }
    if {[string is double -strict $first] &&
	[string is double -strict $second]} {
	set scale [expr {max(1.0, abs($first), abs($second))}]
	return [expr {abs($first - $second) <= $numeric_tolerance * $scale}]
    }
    if {[catch {set first_length [llength $first]}] ||
	[catch {set second_length [llength $second]}] ||
	$first_length < 2 || $first_length != $second_length} {
	return 0
    }
    foreach first_value $first second_value $second {
	if {![equivalent_values $first_value $second_value]} {
	    return 0
	}
    }
    return 1
}

proc ::gui::test::widget_inventory {root} {
    set counts {}
    foreach widget [linsert [descendants $root] 0 $root] {
	set class [winfo class $widget]
	if {$class ne ""} {
	    dict incr counts $class
	}
    }
    set inventory {}
    dict for {class count} $counts {
	lappend inventory "$class $count"
    }
    return [join [lsort -dictionary $inventory] "\n"]
}

proc ::gui::test::menu_inventory_walk {menu path inventory_name visited_name} {
    upvar 1 $inventory_name inventory $visited_name visited
    if {[dict exists $visited $menu]} {
	return
    }
    dict set visited $menu 1

    if {[catch {set last [$menu index end]}] || $last eq "none"} {
	return
    }
    for {set index 0} {$index <= $last} {incr index} {
	if {[catch {set type [$menu type $index]}] || $type eq "tearoff"} {
	    continue
	}
	set label ""
	set state normal
	catch {set label [$menu entrycget $index -label]}
	catch {set state [$menu entrycget $index -state]}
	set entry_path [concat $path [list $label]]
	lappend inventory [list {*}$entry_path $type $state]
	if {$type eq "cascade"} {
	    set submenu [$menu entrycget $index -menu]
	    if {$submenu ne "" && [winfo exists $submenu]} {
		menu_inventory_walk $submenu $entry_path inventory visited
	    }
	}
    }
}

proc ::gui::test::menu_roots {root} {
    set roots {}
    if {![catch {set menu [$root cget -menu]}] &&
	$menu ne "" && [winfo exists $menu]} {
	lappend roots [list "" $menu]
	return $roots
    }
    foreach widget [descendants $root] {
	if {[winfo class $widget] ne "Menubutton" ||
	    [catch {set menu [$widget cget -menu]}] ||
	    $menu eq "" || ![winfo exists $menu]} {
	    continue
	}
	set ancestor [winfo parent $widget]
	set in_menubar 0
	while {$ancestor ne ""} {
	    if {[winfo class $ancestor] eq "Menubar"} {
		set in_menubar 1
		break
	    }
	    set ancestor [winfo parent $ancestor]
	}
	if {!$in_menubar} {
	    continue
	}
	lappend roots [list [$widget cget -text] $menu]
    }
    return $roots
}

proc ::gui::test::menu_inventory {root} {
    set inventory {}
    set visited {}
    foreach menu_root [menu_roots $root] {
	lassign $menu_root label menu
	set path {}
	if {$label ne ""} {
	    set path [list $label]
	}
	menu_inventory_walk $menu $path inventory visited
    }
    return [join [lsort -dictionary -unique $inventory] "\n"]
}

proc ::gui::test::find_menu_entry {root labels} {
    set menu ""
    set remaining [lrange $labels 1 end]
    foreach menu_root [menu_roots $root] {
	set root_label [lindex $menu_root 0]
	if {$root_label eq ""} {
	    set menu [lindex $menu_root 1]
	    set remaining $labels
	    break
	}
	if {$root_label eq [lindex $labels 0]} {
	    set menu [lindex $menu_root 1]
	    break
	}
    }
    if {$menu eq ""} {
	return {}
    }

    foreach label $remaining {
	set found ""
	set last [$menu index end]
	if {$last eq "none"} {
	    return {}
	}
	for {set index 0} {$index <= $last} {incr index} {
	    if {[catch {set entry_label [$menu entrycget $index -label]}] ||
		$entry_label ne $label} {
		continue
	    }
	    set found [list $menu $index]
	    break
	}
	if {$found eq ""} {
	    return {}
	}
	if {$label ne [lindex $labels end]} {
	    if {[$menu type $index] ne "cascade"} {
		return {}
	    }
	    set menu [$menu entrycget $index -menu]
	}
    }
    return $found
}

proc ::gui::test::invoke_menu_entry {root labels} {
    set entry [find_menu_entry $root $labels]
    if {$entry eq ""} {
	error "menu entry not found: [join $labels { > }]"
    }
    lassign $entry menu index
    if {[$menu entrycget $index -state] eq "disabled"} {
	error "menu entry is disabled: [join $labels { > }]"
    }
    $menu invoke $index
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
