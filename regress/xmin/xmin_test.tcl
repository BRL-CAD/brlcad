namespace eval ::xmin::test {
    variable directory
    if {![info exists ::env(XMIN_TEST_DIR)]} {
	error "XMIN_TEST_DIR is required"
    }
    set directory $::env(XMIN_TEST_DIR)
}

proc ::xmin::test::write {name contents} {
    variable directory
    set destination [file join $directory $name]
    set temporary ${destination}.tmp
    set channel [open $temporary w]
    puts $channel $contents
    close $channel
    file rename -force $temporary $destination
}

proc ::xmin::test::publish_target {name widget} {
    write ${name}_window [winfo id $widget]
    write $name [list \
	[expr {[winfo width $widget] / 2}] \
	[expr {[winfo height $widget] / 2}]]
}

proc ::xmin::test::descendants {widget} {
    set descendants {}
    foreach child [winfo children $widget] {
	lappend descendants $child
	lappend descendants {*}[descendants $child]
    }
    return $descendants
}

proc ::xmin::test::find_widget {root class text} {
    foreach widget [descendants $root] {
	if {[winfo class $widget] ne $class ||
	    [catch {set widget_text [$widget cget -text]}] ||
	    $widget_text ne $text} {
	    continue
	}
	return $widget
    }
    return ""
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
