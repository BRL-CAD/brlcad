if {![info exists ::env(XMIN_GUI_LIBRARY)]} {
    puts stderr "XMIN_GUI_LIBRARY is required"
    exit 2
}
source $::env(XMIN_GUI_LIBRARY)
set ::env(ARCHER_PREFS_FILE) [file join $::env(XMIN_TEST_DIR) .archerrc]

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

proc ::archer_embedded_check {} {
    if {[catch {
	set application .embedded
	::xmin::test::require {[winfo exists $application]} \
	    "embedded Archer widget was not created"

	set inventory [::xmin::test::menu_inventory $application]
	::xmin::test::write embedded_menu_inventory $inventory

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
	    ::xmin::test::require {
		[::xmin::test::find_menu_entry $application $labels] ne ""
	    } "missing embedded Archer menu entry: [join $labels { > }]"
	}

	foreach labels {
	    {File Export}
	    {File Revert}
	    {File Raytrace}
	} {
	    ::xmin::test::require {
		[::xmin::test::find_menu_entry $application $labels] eq ""
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

    puts "PASS: embedded Archer menu construction"
    exit 0
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
