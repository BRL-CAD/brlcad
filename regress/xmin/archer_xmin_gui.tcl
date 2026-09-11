if {![info exists ::env(XMIN_GUI_LIBRARY)] ||
    ![info exists ::env(ARCHER_LAUNCH)] ||
    ![info exists ::env(ARCHER_TEST_DATABASE)]} {
    puts stderr "XMIN_GUI_LIBRARY, ARCHER_LAUNCH, and ARCHER_TEST_DATABASE are required"
    exit 2
}
source $::env(XMIN_GUI_LIBRARY)
set ::env(ARCHER_PREFS_FILE) [file join $::env(XMIN_TEST_DIR) .archerrc]

namespace eval ::archer::xmin {
    variable application ""
    variable dialog_seen ""
}

proc ::archer::xmin::finish {status message} {
    variable application
    if {$application ne "" && [llength [info commands $application]]} {
	catch {::itcl::delete object $application}
    }
    catch {destroy .}
    puts $message
    exit $status
}

proc ::archer::xmin::toplevels {} {
    set toplevels {}
    foreach widget [linsert [::xmin::test::descendants .] 0 .] {
	if {[winfo toplevel $widget] eq $widget} {
	    lappend toplevels $widget
	}
    }
    return [lsort -unique $toplevels]
}

proc ::archer::xmin::dismiss_dialog {} {
    variable application
    variable dialog_seen
    foreach widget [toplevels] {
	if {$widget eq "." || $widget eq $application ||
	    ![winfo ismapped $widget]} {
	    continue
	}
	set dialog_seen [wm title $widget]
	if {[catch {$widget deactivate 0}]} {
	    destroy $widget
	}
	return
    }
    after 25 ::archer::xmin::dismiss_dialog
}

proc ::archer::xmin::exercise_dialog {root labels} {
    variable dialog_seen
    set dialog_seen ""
    after 25 ::archer::xmin::dismiss_dialog
    ::xmin::test::invoke_menu_entry $root $labels
    ::xmin::test::require {$dialog_seen ne ""} \
	"menu entry did not display a dialog: [join $labels { > }]"
}

proc ::archer::xmin::compare_manifest {actual} {
    if {![info exists ::env(ARCHER_MENU_MANIFEST)]} {
	return
    }
    set channel [open $::env(ARCHER_MENU_MANIFEST) r]
    set expected [string trim [read $channel]]
    close $channel
    if {$actual ne $expected} {
	::xmin::test::write menu_inventory_actual $actual
	error "live Archer menu inventory differs from its manifest"
    }
}

proc ::archer::xmin::run {} {
    variable application
    set application $::ArcherCore::application
    ::xmin::test::require {[llength [info commands $application]] == 1} \
	"Archer application object was not created"
    ::xmin::test::require {[winfo ismapped $application]} \
	"Archer main window is not mapped"
    ::xmin::test::require {[string match {Archer *} [wm title $application]]} \
	"unexpected Archer main-window title"

    set widget_inventory [::xmin::test::widget_inventory $application]
    set menu_inventory [::xmin::test::menu_inventory $application]
    ::xmin::test::write widget_inventory $widget_inventory
    ::xmin::test::write menu_inventory $menu_inventory
    compare_manifest $menu_inventory

    foreach labels {
	{File Preferences...}
	{Display {Standard Views} Front}
	{Display {Background Color} Black}
	{Modes Grid}
	{Raytrace rt 512x512}
	{Help {About Archer...}}
    } {
	::xmin::test::require {
	    [::xmin::test::find_menu_entry $application $labels] ne ""
	} "missing Archer menu entry: [join $labels { > }]"
    }

    $application draw all.g
    update
    ::xmin::test::require {
	[lsearch -exact [$application gedCmd who] all.g] >= 0
    } "Archer did not draw all.g"

    ::xmin::test::invoke_menu_entry $application \
	{Display {Standard Views} Front}
    set aet [$application gedCmd aet]
    ::xmin::test::require {
	abs([lindex $aet 0]) < 0.001 && abs([lindex $aet 1]) < 0.001
    } "Front menu entry did not set the expected view: $aet"

    ::xmin::test::invoke_menu_entry $application \
	{Display {Background Color} Black}
    ::xmin::test::require {
	[$application gedCmd bg] eq "0 0 0"
    } "Black background menu entry did not update the display"

    exercise_dialog $application {Display Center...}
    exercise_dialog $application {File Preferences...}
    exercise_dialog $application {Help {About Plug-ins...}}
    exercise_dialog $application {Help {About Archer...}}

    finish 0 "PASS: Archer menu, dialog, widget, and view behavior"
}

proc ::archer::xmin::run_checked {} {
    if {[catch {run} message options]} {
	puts stderr $message
	if {[dict exists $options -errorinfo]} {
	    puts stderr [dict get $options -errorinfo]
	}
	finish 1 "FAIL: Archer GUI regression"
    }
}

set argv [list $::env(ARCHER_TEST_DATABASE)]
set argc 1
set argv0 [info nameofexecutable]
unset -nocomplain ::no_bwish
if {[catch {source $::env(ARCHER_LAUNCH)} message options]} {
    puts stderr $message
    if {[dict exists $options -errorinfo]} {
	puts stderr [dict get $options -errorinfo]
    }
    exit 1
}

# Let the splash timer finish so it cannot obscure dialog discovery.
after 1800 ::archer::xmin::run_checked
vwait forever

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
