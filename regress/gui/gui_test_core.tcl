#                   G U I _ T E S T _ C O R E . T C L
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
namespace eval ::gui::test {
    variable directory
    if {![info exists ::env(GUI_TEST_DIR)]} {
	error "GUI_TEST_DIR is required"
    }
    set directory $::env(GUI_TEST_DIR)
}

proc ::gui::test::write {name contents} {
    variable directory
    set destination [file join $directory $name]
    set temporary ${destination}.tmp
    set channel [open $temporary w]
    puts $channel $contents
    close $channel
    file rename -force $temporary $destination
}

proc ::gui::test::publish_target {name widget} {
    write ${name}_window [winfo id $widget]
    write $name [list \
	[expr {[winfo width $widget] / 2}] \
	[expr {[winfo height $widget] / 2}]]
}

proc ::gui::test::descendants {widget} {
    set descendants {}
    foreach child [winfo children $widget] {
	lappend descendants $child
	lappend descendants {*}[descendants $child]
    }
    return $descendants
}

proc ::gui::test::find_widget {root class text} {
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
