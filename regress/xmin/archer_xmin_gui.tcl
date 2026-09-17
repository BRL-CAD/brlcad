#             A R C H E R _ X M I N _ G U I . T C L
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
if {![info exists ::env(GUI_TEST_LIBRARY)] ||
    ![info exists ::env(ARCHER_LAUNCH)] ||
    ![info exists ::env(ARCHER_TEST_DATABASE)]} {
    puts stderr "GUI_TEST_LIBRARY, ARCHER_LAUNCH, and ARCHER_TEST_DATABASE are required"
    exit 2
}
source $::env(GUI_TEST_LIBRARY)
source [file join [file dirname $::env(GUI_TEST_LIBRARY)] sketch_test.tcl]
set ::env(ARCHER_PREFS_FILE) [file join $::env(GUI_TEST_DIR) .archerrc]

namespace eval ::archer::xmin {
    variable application ""
    variable dialog_seen ""
    variable dialog_poll_delay_ms 25
    variable dialog_timeout_marker "__timeout__"
    variable dialog_timeout_ms 2000
    variable raytrace_poll_delay_ms 10
    variable raytrace_poll_limit 500
}

proc ::archer::xmin::finish {status message} {
    variable application
    if {$application ne "" && [llength [info commands $application]]} {
	catch {::itcl::delete object $application}
    }
    catch {destroy .}
    catch {update}
    lassign [::gui::test::check_background_errors $status $message] \
	status message
    puts $message
    if {$status != 0} {
	puts stderr $message
    }
    exit $status
}

proc ::archer::xmin::toplevels {} {
    set toplevels {}
    foreach widget [linsert [::gui::test::descendants .] 0 .] {
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
    variable dialog_poll_delay_ms
    after $dialog_poll_delay_ms ::archer::xmin::dismiss_dialog
}

proc ::archer::xmin::exercise_dialog {root labels {expected_title ""}} {
    variable dialog_seen
    variable dialog_poll_delay_ms
    variable dialog_timeout_marker
    variable dialog_timeout_ms
    set dialog_seen ""
    after $dialog_poll_delay_ms ::archer::xmin::dismiss_dialog
    ::gui::test::invoke_menu_entry $root $labels
    if {$dialog_seen eq ""} {
	set timeout [after $dialog_timeout_ms \
	    [list set ::archer::xmin::dialog_seen $dialog_timeout_marker]]
	vwait ::archer::xmin::dialog_seen
	after cancel $timeout
    }
    ::gui::test::require {
	$dialog_seen ne "" && $dialog_seen ne $dialog_timeout_marker} \
	"menu entry did not display a dialog: [join $labels { > }]"
    if {$expected_title ne ""} {
	::gui::test::require {$dialog_seen eq $expected_title} \
	    "unexpected dialog title '$dialog_seen', expected '$expected_title'"
    }
}

proc ::archer::xmin::compare_manifest {actual} {
    if {![info exists ::env(ARCHER_MENU_MANIFEST)]} {
	return
    }
    set channel [open $::env(ARCHER_MENU_MANIFEST) r]
    set expected [string trim [read $channel]]
    close $channel
    if {$actual ne $expected} {
	::gui::test::write menu_inventory_actual $actual
	error "live Archer menu inventory differs from its manifest"
    }
}

proc ::archer::xmin::exercise_object_lifecycle {
    class object constructor_args initializer
} {
    foreach cycle {first second} {
	if {[catch {$class $object {*}$constructor_args} message options]} {
	    catch {::itcl::delete object $object}
	    catch {destroy $object}
	    if {[dict exists $options -errorinfo]} {
		append message "\n" [dict get $options -errorinfo]
	    }
	    error "$class $cycle construction failed: $message"
	}
	if {$initializer ne "" &&
	    [catch {{*}$initializer $object} message options]} {
	    catch {::itcl::delete object $object}
	    if {[dict exists $options -errorinfo]} {
		append message "\n" [dict get $options -errorinfo]
	    }
	    error "$class $cycle initialization failed: $message"
	}
	update
	::gui::test::require {
	    [llength [info commands $object]] == 1 &&
	    [winfo exists $object]
	} "$class $cycle construction did not retain its object and window"
	if {[catch {::itcl::delete object $object} message options]} {
	    if {[dict exists $options -errorinfo]} {
		append message "\n" [dict get $options -errorinfo]
	    }
	    error "$class $cycle destruction failed: $message"
	}
	update
	::gui::test::require {
	    [llength [info commands $object]] == 0 &&
	    ![winfo exists $object]
	} "$class $cycle destruction retained its object or window"
    }
}

proc ::archer::xmin::initialize_editor {geometry_data editor} {
    $editor initGeometry $geometry_data
}

proc ::archer::xmin::arb_storage_attribute {primitive logical_vertex} {
    if {$primitive eq "arb4" && $logical_vertex == 4} {
	return V5
    }
    if {$primitive eq "arb6" && $logical_vertex == 6} {
	return V7
    }
    return V$logical_vertex
}

proc ::archer::xmin::require_arb_standard_storage {ged geometry_name primitive} {
    set duplicate_groups [dict create \
	arb4 {{V1 V4} {V5 V6 V7 V8}} \
	arb5 {{V5 V6 V7 V8}} \
	arb6 {{V5 V6} {V7 V8}} \
	arb7 {{V5 V8}} \
	arb8 {}]

    foreach group [dict get $duplicate_groups $primitive] {
	set expected [$ged get $geometry_name [lindex $group 0]]
	foreach attribute [lrange $group 1 end] {
	    set actual [$ged get $geometry_name $attribute]
	    ::gui::test::require {
		[::gui::test::equivalent_values $actual $expected]
	    } "$primitive storage is nonstandard: [lindex $group 0]=$expected, $attribute=$actual"
	}
    }
}

proc ::archer::xmin::exercise_arb_editor_fields {
    object ged geometry_name geometry_data primitive
} {
    set edit_delta 0.125
    set logical_vertex_count [string index $primitive end]

    for {set logical_vertex 1} {$logical_vertex <= $logical_vertex_count} {incr logical_vertex} {
	foreach {axis component_index} {x 0 y 1 z 2} {
	    $object initGeometry $geometry_data
	    set component_name "${primitive}V${logical_vertex}${axis}E"
	    set entry [$object component $component_name]
	    set requested [expr {[$entry get] + $edit_delta}]
	    $entry configure -state normal
	    $entry delete 0 end
	    $entry insert 0 $requested
	    $object updateGeometry

	    set storage_attribute [arb_storage_attribute $primitive $logical_vertex]
	    set actual [lindex [$ged get $geometry_name $storage_attribute] $component_index]
	    ::gui::test::require {
		[::gui::test::equivalent_values $actual $requested]
	    } "$primitive $component_name wrote $actual instead of $requested to $storage_attribute"
	    require_arb_standard_storage $ged $geometry_name $primitive

	    $ged adjust $geometry_name {*}$geometry_data
	}
    }
    $object initGeometry $geometry_data
}

proc ::archer::xmin::exercise_editor_update {
    class object constructor_args geometry_data ged geometry_name
} {
    $class $object {*}$constructor_args
    if {[catch {
	$object initGeometry $geometry_data
	set original [$ged get $geometry_name]
	$object updateGeometry
	set updated [$ged get $geometry_name]
	::gui::test::require \
	    [::gui::test::equivalent_values $updated $original] \
	    "$class changed geometry during an unmodified update: original={$original}, updated={$updated}"
	if {[regexp {^Arb([4-8])EditFrame$} $class unused arb_type]} {
	    exercise_arb_editor_fields $object $ged $geometry_name \
		[lrange $original 1 end] "arb$arb_type"
	}
	$object initGeometry [lrange $updated 1 end]
    } message options]} {
	catch {::itcl::delete object $object}
	if {[dict exists $options -errorinfo]} {
	    append message "\n" [dict get $options -errorinfo]
	}
	error "$class update/reload failed: $message"
    }
    ::itcl::delete object $object
}

proc ::archer::xmin::exercise_editor_lifecycles {application} {
    set ged [$application component ged]
    set editors {
	Arb4EditFrame arb4
	Arb5EditFrame arb5
	Arb6EditFrame arb6
	Arb7EditFrame arb7
	Arb8EditFrame arb8
	BotEditFrame bot
	BrepEditFrame brep
	EhyEditFrame ehy
	EllEditFrame ell
	EpaEditFrame epa
	EtoEditFrame eto
	ExtrudeEditFrame extrude
	GripEditFrame grip
	HalfEditFrame half
	HypEditFrame hyp
	JointEditFrame joint
	MetaballEditFrame metaball
	PartEditFrame part
	PipeEditFrame pipe
	RhcEditFrame rhc
	RpcEditFrame rpc
	SketchEditFrame sketch
	SphereEditFrame sph
	SuperellEditFrame superell
	TgcEditFrame tgc
	TorusEditFrame tor
    }

    foreach {class primitive} $editors {
	set geometry_name "xmin-lifecycle-$primitive.s"
	$ged make $geometry_name $primitive
	set geometry_data [lrange [$ged get $geometry_name] 1 end]
	set editor "${application}.xmin_lifecycle_[string tolower $class]"
	set constructor_args [list -mged $ged -geometryObject $geometry_name \
	    -geometryObjectPath $geometry_name -units mm]
	exercise_editor_update $class $editor $constructor_args \
	    $geometry_data $ged $geometry_name
	exercise_object_lifecycle $class $editor $constructor_args \
	    [list ::archer::xmin::initialize_editor $geometry_data]
    }
}

proc ::archer::xmin::exercise_wizard_build {
    application class action top
} {
    set ged [$application component ged]
    if {[$ged exists $top]} {
	$ged killtree -a $top
    }
    set object ${application}.xmin_build_[string tolower $class]
    $class $object $application $top {} {0 0 0} mm
    if {[catch {$object $action} result options]} {
	catch {::itcl::delete object $object}
	if {[dict exists $options -errorinfo]} {
	    append result "\n" [dict get $options -errorinfo]
	}
	error "$class geometry generation failed: $result"
    }

    ::gui::test::require {$result eq $top} "$class returned '$result' instead of its top object '$top'"
    ::gui::test::require {[$ged exists $top]} "$class did not create its top database object '$top'"
    ::gui::test::require {
	[$ged attr get $top WizardClass] eq $class
    } "$class did not identify its generated database object"
    ::gui::test::require {
	[llength [$object getWizardState]] > 0
    } "$class did not retain its generation parameters"
    ::gui::test::require {
	[lsearch -exact [$ged who] $top] >= 0
    } "$class did not draw its generated database object"
    if {$class eq "TankWizard"} {
	::gui::test::require {
	    [$ged exists ${top}_gun_tube.r]
	} "TankWizard omitted its transformed gun-barrel region"
	set xml [$object buildTankXML]
	::gui::test::require {
	    [string first "<Name>$top</Name>" $xml] >= 0 &&
	    [string first "<Name>Gun Barrel</Name>" $xml] >= 0
	} "TankWizard XML omitted its top system or gun component"
	set open_systems [llength [regexp -all -inline {<System>} $xml]]
	set close_systems [llength [regexp -all -inline {</System>} $xml]]
	::gui::test::require {
	    $open_systems > 10 && $open_systems == $close_systems
	} "TankWizard XML has unbalanced system elements"
    }
    ::itcl::delete object $object
}

proc ::archer::xmin::exercise_wizard_builds {application} {
    foreach {class action top} {
	HumanWizard buildHuman xmin-human
	TireWizard buildTire xmin-tire
	TankWizard buildTank xmin-tank
    } {
	exercise_wizard_build $application $class $action $top
    }
}

proc ::archer::xmin::exercise_plugin_lifecycles {application} {
    foreach class {
	AttrGroupsDisplayUtilityP
	BotUtilityP
	LODUtilityP
    } {
	set object "${application}.xmin_lifecycle_[string tolower $class]"
	exercise_object_lifecycle $class $object [list $application] {}
    }

    foreach class {
	HumanWizard
	TankWizard
	TireWizard
    } {
	set object "${application}.xmin_lifecycle_[string tolower $class]"
	set wizard_top "xmin-lifecycle-[string tolower $class].g"
	exercise_object_lifecycle $class $object \
	    [list $application $wizard_top {} {0 0 0} mm] {}
    }
}

proc ::archer::xmin::exercise_attribute_groups_utility {application} {
    set ged [$application component ged]
    foreach {name value center} {
	xmin-attr-alpha.s alpha {0 0 0}
	xmin-attr-beta.s beta {3 0 0}
    } {
	$ged put $name sph V $center A {1 0 0} B {0 1 0} C {0 0 1}
	$ged attr set $name xmin_category $value
    }
    $ged attr set _GLOBAL Attribute_Groups_List \
	[list [list Xmin xmin_category {alpha beta}]]

    set object "$application.xmin_functional_attrgroups"
    AttrGroupsDisplayUtilityP $object $application
    update
    set group_list [$object component glist]
    ::gui::test::require {
	[$group_list get 0 end] eq "Xmin"
    } "attribute-groups utility did not load its database definition"
    $group_list selection set 0
    uplevel #0 [$group_list cget -selectioncommand]
    ::gui::test::require {
	[$object getCurrentGroup] eq "Xmin"
    } "attribute-groups utility did not select its group"

    set attribute_list [$object component alist]
    ::gui::test::require {
	[$attribute_list get 0 end] eq "alpha beta"
    } "attribute-groups utility did not load the grouped values"
    $attribute_list selection set 0
    set highlighted [$object highlightSelectedAttr]
    ::gui::test::require {
	[lsearch -exact [lindex $highlighted 0] xmin-attr-alpha.s] >= 0
    } "attribute-groups utility did not resolve and highlight the selected value"
    ::itcl::delete object $object
}

proc ::archer::xmin::exercise_lod_utility {application} {
    set ged [$application component ged]
    set original_enabled [$ged lod enabled]
    set original_points [$ged lod scale points]
    set original_curves [$ged lod scale curves]
    set object "$application.xmin_functional_lod"
    LODUtilityP $object $application
    update

    set dialog "$object.lodDialog"
    ::gui::test::require {
	[llength [info commands $dialog]] == 1 && [winfo exists $dialog]
    } "LOD utility did not create its configuration dialog"
    set frame "$dialog.lodFrame"
    if {![$ged lod enabled]} {
	$frame.lodonCheckbutton invoke
    }
    $frame.pointsScale set 0.7
    $frame.curvesScale set 9
    update
    set updated_points [$ged lod scale points]
    set updated_curves [$ged lod scale curves]
    ::gui::test::require {
	abs($updated_points - 0.7) < 1.0e-9 &&
	abs($updated_curves - 9.0) < 1.0e-9
    } "LOD utility controls set points=$updated_points and curves=$updated_curves"
    $frame.updateButton invoke
    update

    ::itcl::delete object $object
    $ged lod scale points $original_points
    $ged lod scale curves $original_curves
    if {$original_enabled} {
	$ged lod on
    } else {
	$ged lod off
    }
}

proc ::archer::xmin::exercise_bot_utility {application} {
    set ged [$application component ged]
    set bot xmin-utility-bot.s
    $ged put $bot bot mode surface orient no flags {} \
	V {{0 0 0} {100 0 0} {0 100 0}} F {{0 1 2}}

    set utility_toplevel "$application.xmin_functional_bot"
    toplevel $utility_toplevel
    set object "$utility_toplevel.utility"
    BotUtilityP $object $application
    update
    set selector [$object component combo]
    ::gui::test::require {
	[lsearch -exact [$selector cget -values] $bot] >= 0
    } "BoT utility did not list its available mesh"
    $object configure -selectedbot $bot

    set existing [::itcl::find object -class BotEditor]
    $object editBot $bot
    update
    set editors {}
    foreach editor [::itcl::find object -class BotEditor] {
	if {[lsearch -exact $existing $editor] < 0} {
	    lappend editors $editor
	}
    }
    ::gui::test::require {
	[llength $editors] == 1 && [$ged exists "$bot.edit"]
    } "BoT utility did not launch an Archer-backed editor with a working copy"
    set editor [lindex $editors 0]
    $ged kill "$bot.edit"
    ::itcl::delete object $editor
    ::itcl::delete object $object
    destroy $utility_toplevel
}

proc ::archer::xmin::exercise_plugin_actions {application} {
    exercise_attribute_groups_utility $application
    exercise_lod_utility $application
    exercise_bot_utility $application
}
proc ::archer::xmin::exercise_combination_edit {application} {
    set ged [$application component ged]
    set members {xmin-tree-a.s {xmin tree b.s} xmin-tree-c.s}
    foreach {name center radius} {
	xmin-tree-a.s {0 0 0} 2
	{xmin tree b.s} {1 0 0} 2
	xmin-tree-c.s {2 0 0} 2
    } {
	$ged put $name sph V $center \
	    A [list $radius 0 0] \
	    B [list 0 $radius 0] \
	    C [list 0 0 $radius]
    }

    # This right-nested subtraction cannot survive unpackTree/packTree.
    # Changing an unrelated attribute must therefore retain the stored tree.
    set requested_tree {u {l xmin-tree-a.s} {- {l {xmin tree b.s}} {l xmin-tree-c.s}}}
    $ged put xmin-tree.c comb region no tree $requested_tree
    set combination [lrange [$ged get xmin-tree.c] 1 end]
    set original_tree [bu_get_value_by_keyword tree $combination]
    set flattened_tree [ArcherCore::unpackTree $original_tree]
    ::gui::test::require {
	[ArcherCore::packTree $flattened_tree] ne $original_tree
    } "Archer combination fixture does not exercise a lossy tree grouping"
    set spaced_leaf [list l [lindex $members 1]]
    ::gui::test::require {
	[ArcherCore::packTree [ArcherCore::unpackTree $spaced_leaf]] eq $spaced_leaf
    } "Archer tree editor did not preserve a member name containing spaces"


    set editor ${application}.xmin_combination_editor
    CombEditFrame $editor -mged $ged -geometryObject xmin-tree.c
    $editor initGeometry $combination
    set rgb_entry [$editor component combRgbE]
    $rgb_entry delete 0 end
    $rgb_entry insert 0 {17 34 51}
    $editor updateGeometry

    set updated [lrange [$ged get xmin-tree.c] 1 end]
    set updated_tree [bu_get_value_by_keyword tree $updated]
    set updated_rgb [bu_get_value_by_keyword rgb $updated]
    ::gui::test::require {$updated_tree eq $original_tree} \
	"editing a combination attribute changed its Boolean tree"
    ::gui::test::require {$updated_rgb eq "17 34 51"} \
	"Archer did not apply the requested combination color"
    ::itcl::delete object $editor
    exercise_object_lifecycle CombEditFrame $editor \
	[list -mged $ged -geometryObject xmin-tree.c] \
	[list ::archer::xmin::initialize_editor $updated]

    # Exercise structural hierarchy discovery with a nested tree and a member
    # name containing spaces.
    $application rebuildTree
    set tree [$application component newtree]
    set combination_node ""
    foreach node [$tree children {}] {
	if {[$tree item $node -text] eq "xmin-tree.c"} {
	    set combination_node $node
	    break
	}
    }
    ::gui::test::require {$combination_node ne ""} \
	"Archer hierarchy omitted the test combination"
    $tree focus $combination_node
    $tree item $combination_node -open true
    event generate $tree <<TreeviewOpen>>
    update
    set child_names {}
    foreach child [$tree children $combination_node] {
	lappend child_names [$tree item $child -text]
    }
    ::gui::test::require {[lsort $child_names] eq [lsort $members]} \
	"Archer hierarchy reported the wrong combination members: $child_names"
}

proc ::archer::xmin::record_raytrace_end {completion_callback aborted} {
    variable raytrace_aborted
    set raytrace_aborted $aborted
    uplevel #0 [linsert $completion_callback end $aborted]
}

proc ::archer::xmin::wait_for {condition description} {
    variable raytrace_poll_delay_ms
    variable raytrace_poll_limit
    for {set attempt 0} {$attempt < $raytrace_poll_limit} {incr attempt} {
	update
	if {[uplevel 1 [list expr $condition]]} {
	    return
	}
	after $raytrace_poll_delay_ms
    }
    error "timed out waiting for $description"
}

proc ::archer::xmin::exercise_raytrace_abort {application} {
    variable raytrace_aborted
    set toolbar [$application component primaryToolbar]
    set raytrace_button [$toolbar component raytrace]
    set launch_command [$toolbar itemcget raytrace -command]

    ::gui::test::require {
	[string first raytracePlus $launch_command] >= 0
    } "Archer raytrace toolbar button does not start a raytrace"

    set raytrace_aborted ""
    set completion_callback [$application gedCmd rt_end_callback]
    $application gedCmd rt_end_callback \
	[list ::archer::xmin::record_raytrace_end $completion_callback]
    $raytrace_button invoke

    set abort_command [$toolbar itemcget raytrace -command]
    ::gui::test::require {
	$abort_command ne $launch_command &&
	[string first abort $abort_command] >= 0
    } "Archer did not turn its raytrace toolbar button into Abort"

    $raytrace_button invoke
    wait_for {$raytrace_aborted ne ""} "RtWizard abort callback"
    ::gui::test::require {$raytrace_aborted} \
	"Archer reported normal completion after the Abort action"
    wait_for {
	[string first raytracePlus [$toolbar itemcget raytrace -command]] >= 0
    } "Archer raytrace toolbar restoration"
}

proc ::archer::xmin::run {} {
    variable application
    set application $::ArcherCore::application
    ::gui::test::require {[llength [info commands $application]] == 1} \
	"Archer application object was not created"
    ::gui::test::require {[winfo ismapped $application]} \
	"Archer main window is not mapped"
    ::gui::test::require {[string match {Archer *} [wm title $application]]} \
	"unexpected Archer main-window title"

    set opened_database [$application WhatsOpen]
    ::gui::test::require {
	[file normalize $opened_database] eq [file normalize $::env(ARCHER_WORK_DATABASE)]
    } "Archer did not open the relative database argument"

    set widget_inventory [::gui::test::widget_inventory $application]
    set menu_inventory [::gui::test::menu_inventory $application]
    ::gui::test::write widget_inventory $widget_inventory
    ::gui::test::write menu_inventory $menu_inventory
    compare_manifest $menu_inventory

    foreach labels {
	{File Preferences...}
	{Display {Standard Views} Front}
	{Display {Background Color} Black}
	{Modes Grid}
	{Raytrace rt 512x512}
	{Help {Archer Help...}}
	{Help {About Archer...}}
    } {
	::gui::test::require {
	    [::gui::test::find_menu_entry $application $labels] ne ""
	} "missing Archer menu entry: [join $labels { > }]"
    }

    $application draw all.g
    update
    ::gui::test::require {
	[lsearch -exact [$application gedCmd who] all.g] >= 0
    } "Archer did not draw all.g"

    ::gui::test::invoke_menu_entry $application \
	{Display {Standard Views} Front}
    set aet [$application gedCmd aet]
    ::gui::test::require {
	abs([lindex $aet 0]) < 0.001 && abs([lindex $aet 1]) < 0.001
    } "Front menu entry did not set the expected view: $aet"

    ::gui::test::invoke_menu_entry $application \
	{Display {Background Color} Black}
    ::gui::test::require {
	[$application gedCmd bg] eq "0 0 0"
    } "Black background menu entry did not update the display"

    exercise_dialog $application {Display Center...}
    exercise_dialog $application {File Preferences...} Preferences
    exercise_dialog $application {Help {Archer Help...}} {Archer Help Browser}
    exercise_dialog $application {Help {About Plug-ins...}} {Plug-in Information}
    exercise_dialog $application {Help {About Archer...}} {About Archer}

    exercise_combination_edit $application
    ::gui::test::exercise_sketch_segments SketchCArc SketchBezier Archer ::archer::xmin
    exercise_wizard_builds $application
    exercise_editor_lifecycles $application
    exercise_plugin_lifecycles $application
    exercise_plugin_actions $application
    exercise_raytrace_abort $application

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

set work_database [file join $::env(GUI_TEST_DIR) archer-relative.g]
file copy -force $::env(ARCHER_TEST_DATABASE) $work_database
set ::env(ARCHER_WORK_DATABASE) $work_database
cd $::env(GUI_TEST_DIR)
set argv [list [file tail $work_database]]
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
::gui::test::capture_background_errors

# Let the splash timer finish so it cannot obscure dialog discovery.
after 1800 ::archer::xmin::run_checked
vwait forever

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
