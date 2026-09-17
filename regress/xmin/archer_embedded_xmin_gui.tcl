#    A R C H E R _ E M B E D D E D _ X M I N _ G U I . T C L
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
if {![info exists ::env(GUI_TEST_LIBRARY)]} {
    puts stderr "GUI_TEST_LIBRARY is required"
    exit 2
}
source $::env(GUI_TEST_LIBRARY)
set ::env(ARCHER_PREFS_FILE) [file join $::env(GUI_TEST_DIR) .archerrc]

# ArcherCore supports embedding in another Itk widget.  Its constructor
# currently performs a synchronous database load after one update, so inspect
# the completed menu hierarchy during that update and exit before Load can
# block an otherwise database-free fixture.
namespace eval ::ArcherCoreBootstrap {
    set parentClass itk::Widget
    set inheritFromToplevel 0
}

if {[catch {package require Archer 1.0} message options]} {
    puts stderr $message
    if {[dict exists $options -errorinfo]} {
	puts stderr [dict get $options -errorinfo]
    }
    exit 1
}
::gui::test::capture_background_errors

proc ::archer_embedded_check {} {
    if {[catch {
	set application .embedded
	::gui::test::require {[winfo exists $application]} \
	    "embedded Archer widget was not created"

	set inventory [::gui::test::menu_inventory $application]
	::gui::test::write embedded_menu_inventory $inventory

	foreach labels {
	    {File Save}
	    {Display {Standard Views} {35, 25}}
	    {Display {Standard Views} {45, 45}}
	    {Modes {Comp Select Mode} List}
	    {Modes {Comp Select Mode} {List (Partial)}}
	    {Modes {Comp Select Mode} {Add to Group}}
	    {Modes {Comp Select Mode} {Add to Group (Partial)}}
	    {Modes {Comp Select Mode} {Remove from Group}}
	    {Modes {Comp Select Mode} {Remove from Group (Partial)}}
	    {Modes {Comp Select Mode} {Select BOT Points}}
	    {Raytrace rt 512x512}
	    {Help {About Archer...}}
	} {
	    ::gui::test::require {
		[::gui::test::find_menu_entry $application $labels] ne ""
	    } "missing embedded Archer menu entry: [join $labels { > }]"
	}

	foreach labels {
	    {File Export}
	    {File Revert}
	    {File Raytrace}
	} {
	    ::gui::test::require {
		[::gui::test::find_menu_entry $application $labels] eq ""
	    } "unexpected embedded Archer menu entry: [join $labels { > }]"
	}
    } message options]} {
	puts stderr $message
	if {[dict exists $options -errorinfo]} {
	    puts stderr [dict get $options -errorinfo]
	}
	puts "FAIL: embedded Archer menu regression"
	exit 1
    }

    catch {update}
    lassign [::gui::test::check_background_errors 0 \
	"PASS: embedded Archer menu construction"] status result
    puts $result
    if {$status != 0} {
	puts stderr $result
    }
    exit $status
}

after 0 ::archer_embedded_check
if {[catch {Archer .embedded} message options]} {
    puts stderr $message
    if {[dict exists $options -errorinfo]} {
	puts stderr [dict get $options -errorinfo]
    }
    exit 1
}

error "embedded Archer constructor returned before its menu check ran"

# Local Variables:
# tab-width: 8
# mode: Tcl
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
