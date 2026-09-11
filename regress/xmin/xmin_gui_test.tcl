source [file join [file dirname [info script]] xmin_test.tcl]

proc ::xmin::test::require {condition message} {
    if {![uplevel 1 [list expr $condition]]} {
	error $message
    }
}

proc ::xmin::test::widget_inventory {root} {
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

proc ::xmin::test::menu_inventory_walk {menu path inventory_name visited_name} {
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

proc ::xmin::test::menu_roots {root} {
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

proc ::xmin::test::menu_inventory {root} {
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

proc ::xmin::test::find_menu_entry {root labels} {
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

proc ::xmin::test::invoke_menu_entry {root labels} {
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
