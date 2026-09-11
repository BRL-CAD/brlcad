if {![info exists ::env(XMIN_GUI_LIBRARY)] ||
    ![info exists ::env(RTWIZARD_TEST_DATABASE)]} {
    puts stderr "XMIN_GUI_LIBRARY and RTWIZARD_TEST_DATABASE are required"
    exit 2
}
source $::env(XMIN_GUI_LIBRARY)

namespace eval ::rtwizard::xmin {
    variable ready_retries 0
}

proc ::rtwizard::xmin::compare_manifest {actual} {
    if {![info exists ::env(RTWIZARD_MENU_MANIFEST)]} {
	return
    }
    set channel [open $::env(RTWIZARD_MENU_MANIFEST) r]
    set expected [string trim [read $channel]]
    close $channel
    if {$actual ne $expected} {
	::xmin::test::write menu_inventory_actual $actual
	error "live rtwizard menu inventory differs from its manifest"
    }
}

proc ::rtwizard::xmin::run {} {
    variable ready_retries
    if {![info exists ::wizardInstance] || $::wizardInstance eq "" ||
	![info exists ::mgedObj] || ![winfo ismapped .]} {
	if {[incr ready_retries] > 200} {
	    error "rtwizard did not finish constructing its GUI"
	}
	after 25 ::rtwizard::xmin::run_checked
	return
    }

    ::xmin::test::require {[wm title .] eq "RtWizard"} \
	"unexpected rtwizard main-window title"

    set widget_inventory [::xmin::test::widget_inventory .]
    set menu_inventory [::xmin::test::menu_inventory .]
    ::xmin::test::write widget_inventory $widget_inventory
    ::xmin::test::write menu_inventory $menu_inventory
    compare_manifest $menu_inventory

    foreach labels {
	{File {Write .rtwizardrc}}
	{Image {New Image...}}
	{Render Preview}
	{Render Full-Size}
	{Help Help...}
	{Help About...}
    } {
	::xmin::test::require {
	    [::xmin::test::find_menu_entry . $labels] ne ""
	} "missing rtwizard menu entry: [join $labels { > }]"
    }

    foreach {title pages} {
	{Simple Full-Color Image} {fullColor}
	{Simple Line Drawing} {lines}
	{Highlighted Image} {highlighted}
	{Mixed Full-Color and Edges} {fullColor lines}
	{Ghost Image with Insert} {ghost fullColor}
	{Ghost Image with Insert and Lines} {ghost fullColor lines}
    } {
	::xmin::test::invoke_menu_entry . [list Image $title]
	::xmin::test::require {[$::exp getImageType] eq $title} \
	    "rtwizard did not select image type: $title"
	foreach page [concat {dbp fbp intro help exp} $pages] {
	    $::wizardInstance select $page
	    update
	}
	::xmin::test::require {
	    [::xmin::test::find_menu_entry . {Steps Greeting}] ne ""
	} "rtwizard did not create its Steps menu for: $title"
    }

    $::wizardInstance select exp
    update
    puts "PASS: rtwizard menus, image types, pages, controls, and widgets"
    exit 0
}

proc ::rtwizard::xmin::run_checked {} {
    if {[catch {run} message options]} {
	puts stderr $message
	if {[dict exists $options -errorinfo]} {
	    puts stderr [dict get $options -errorinfo]
	}
	puts "FAIL: rtwizard GUI regression"
	exit 1
    }
}

namespace eval ::RtWizard {}
set ::RtWizard::wizard_state(dbFile) $::env(RTWIZARD_TEST_DATABASE)
foreach object_list {color_objlist ghost_objlist line_objlist} {
    set ::RtWizard::wizard_state($object_list) {}
}
set argv {}
set argc 0
after 1500 ::rtwizard::xmin::run_checked
package require RaytraceWizard

# RaytraceWizard normally remains in its event loop.  Reaching here means it
# returned without the test callback explicitly completing the process.
puts stderr "rtwizard exited before the GUI regression completed"
exit 1

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
