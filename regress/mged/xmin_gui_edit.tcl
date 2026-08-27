# Exercise MGED's Tk editing controls using coordinates consumed by xminctl.
# This script may inspect widget geometry, but it deliberately does not invoke
# any GUI action or database edit under test: menu, entry, button, and display
# actions arrive through the X server.

set xmin_test_dir $::env(MGED_XMIN_TEST_DIR)
set xmin_deadline_ms 240000
set xmin_poll_ms 25
set xmin_matrix_view_size 40.0
set xmin_bv_max 2047.0
set xmin_edit_coord_half_range 2048.0
set xmin_matrix_target_numerator 5
set xmin_matrix_target_denominator 8
set xmin_min_widget_extent 16
set xmin_matrix_click_settle_ms 100
set xmin_faceplate_bv_max 2047.0
set xmin_faceplate_menu_x -1600.0
set xmin_faceplate_header_y 1780.0
set xmin_faceplate_3525_y 1468.0
set xmin_pipe_select_y 1676.0
set xmin_pipe_next_y 1572.0
set xmin_pipe_split_y 948.0
set xmin_raytrace_completion_settle_ms 500

proc xmin_write {name contents} {
    global xmin_test_dir
    set destination [file join $xmin_test_dir $name]
    set temporary ${destination}.tmp
    set channel [open $temporary w]
    puts $channel $contents
    close $channel
    file rename -force $temporary $destination
}

proc xmin_publish_target {name widget} {
    xmin_write ${name}_window [winfo id $widget]
    xmin_write $name [list \
	[expr {[winfo width $widget] / 2}] \
	[expr {[winfo height $widget] / 2}]]
}

proc xmin_publish_faceplate_target {name widget bv_x bv_y} {
    global xmin_faceplate_bv_max
    set width [winfo width $widget]
    set height [winfo height $widget]
    set local_x [expr {int((0.5 + $bv_x / (2.0 * $xmin_faceplate_bv_max)) * $width)}]
    set local_y [expr {int((0.5 - $bv_y / (2.0 * $xmin_faceplate_bv_max)) * $height)}]
    xmin_write ${name}_window [winfo id $widget]
    xmin_write $name [list $local_x $local_y]
}

proc xmin_publish_model_point_target {name widget model_point} {
    set view_point [model2view {*}$model_point]
    set width [winfo width $widget]
    set height [winfo height $widget]
    set point_x [expr {[winfo rootx $widget] +
	int((0.5 + [lindex $view_point 0] * 0.5) * $width)}]
    set point_y [expr {[winfo rooty $widget] +
	int((0.5 - [lindex $view_point 1] * 0.5) * $height)}]
    xmin_write ${name}_window root
    xmin_write $name [list $point_x $point_y]
}

proc xmin_descendants {widget} {
    set descendants {}
    foreach child [winfo children $widget] {
	lappend descendants $child
	foreach descendant [xmin_descendants $child] {
	    lappend descendants $descendant
	}
    }
    return $descendants
}

proc xmin_find_widget {root class text} {
    foreach widget [xmin_descendants $root] {
	if {[winfo class $widget] ne $class ||
	    [catch {set widget_text [$widget cget -text]}] ||
	    $widget_text ne $text} {
	    continue
	}
	return $widget
    }
    return ""
}

proc xmin_menu_inventory_walk {menu path inventory_name visited_name} {
    upvar 1 $inventory_name inventory
    upvar 1 $visited_name visited
    if {[dict exists $visited $menu]} {
	return
    }
    dict set visited $menu 1
    if {[catch {set last [$menu index end]}] || $last eq "none"} {
	return
    }
    for {set index 0} {$index <= $last} {incr index} {
	if {[catch {set type [$menu type $index]}] || $type eq "separator"} {
	    continue
	}
	set label [$menu entrycget $index -label]
	set state normal
	catch {set state [$menu entrycget $index -state]}
	set entry_path [join [concat $path [list $label]] /]
	lappend inventory "$entry_path|$type|$state"
	if {$type eq "cascade"} {
	    set submenu [$menu entrycget $index -menu]
	    if {$submenu ne "" && [winfo exists $submenu]} {
		xmin_menu_inventory_walk $submenu \
		    [concat $path [list $label]] inventory visited
	    }
	}
    }
}

proc xmin_publish_menu_inventory {id} {
    set inventory {}
    set visited [dict create]
    xmin_menu_inventory_walk .$id.menubar {} inventory visited
    xmin_write menu_inventory [join [lsort -dictionary $inventory] "\n"]
}

proc xmin_menu_entry_bounds {menu index} {
    set width [winfo width $menu]
    set height [winfo height $menu]
    set first_x $width
    set first_y $height
    set last_x -1
    set last_y -1
    for {set y 0} {$y < $height} {incr y} {
	for {set x 0} {$x < $width} {incr x} {
	    if {![catch {$menu index @$x,$y} found] && $found ne "none" &&
		$found == $index} {
		set first_x [expr {min($first_x, $x)}]
		set first_y [expr {min($first_y, $y)}]
		set last_x [expr {max($last_x, $x)}]
		set last_y [expr {max($last_y, $y)}]
	    }
	}
    }
    if {$last_x < 0} {
	error "could not locate menu entry $index in $menu"
    }
    return [list $first_x $first_y $last_x $last_y]
}

proc xmin_publish_menu_widget_target {name menu label {coordinate_mode root}} {
    global xmin_target_menu_bounds xmin_target_menu_root
    if {![winfo exists $menu] || ![winfo ismapped $menu] ||
	[winfo width $menu] <= 1 ||
	[catch {set last [$menu index end]}] || $last eq "none"} {
	return 0
    }
    for {set index 0} {$index <= $last} {incr index} {
	if {[catch {set entry_label [$menu entrycget $index -label]}] ||
	    $entry_label ne $label} {
	    continue
        }
	set bounds [xmin_menu_entry_bounds $menu $index]
	lassign $bounds first_x first_y last_x last_y
	if {$coordinate_mode eq "menu"} {
	    xmin_write ${name}_window [winfo id $menu]
	    xmin_write $name [list \
		[expr {($first_x + $last_x) / 2}] \
		[expr {($first_y + $last_y) / 2}]]
	    return 1
	}
	set root_x [winfo rootx $menu]
	set root_y [winfo rooty $menu]
	set xmin_target_menu_bounds($name) $bounds
	set xmin_target_menu_root($name) [list $root_x $root_y]
	xmin_write ${name}_window root
	xmin_write $name [list \
	    [expr {$root_x + ($first_x + $last_x) / 2}] \
	    [expr {$root_y + ($first_y + $last_y) / 2}]]
	return 1
    }
    return 0
}

proc xmin_publish_menu_target {name root label} {
    foreach menu [xmin_descendants $root] {
	if {[winfo class $menu] eq "Menu" &&
	    [xmin_publish_menu_widget_target $name $menu $label]} {
	    return 1
	}
    }
    return 0
}

proc xmin_publish_submenu_targets {id} {
    global xmin_target_menu_bounds xmin_target_menu_root
    set menubar .$id.menubar
    foreach {submenu_label submenu top_target targets} {
	Misc .misc main_misc_menu {
	    faceplate_toggle Faceplate
	    orig_gui_toggle {Faceplate GUI}
	    renderer_depthcue {Depth Cueing}
	    renderer_zbuffer {Z Buffer}
	    renderer_lighting Lighting
	}
	Edit .edit main_edit_menu {
	    primitive_editor_menu {Primitive Editor}
	}
	Tools .tools main_tools_menu {
	    raytrace_control_menu {Raytrace Control Panel}
	}
    } {
	if {![info exists xmin_target_menu_bounds($top_target)] ||
	    ![info exists xmin_target_menu_root($top_target)]} {
	    error "no geometry was recorded for $submenu_label"
	}
	lassign $xmin_target_menu_bounds($top_target) \
	    first_x first_y last_x last_y
	lassign $xmin_target_menu_root($top_target) root_x root_y
	set popup_x [expr {$root_x + $first_x}]
	set popup_y [expr {$root_y + $last_y + 2}]
	set menu ${menubar}${submenu}
	$menu post $popup_x $popup_y
	update idletasks
	foreach {target label} $targets {
	    xmin_publish_menu_widget_target $target $menu $label
	}
	$menu unpost
    }
}

proc xmin_view_state {} {
    return [list [center] [size] [view quat]]
}

proc xmin_fail {message} {
    xmin_write result "FAIL: $message"
    after idle _mged_quit
    return
}

proc bgerror {message} {
    global errorInfo
    xmin_write tcl_error_debug $errorInfo
    xmin_fail "background Tcl error: $message"
}

proc xmin_wait_for_gui {} {
    global faceplate mged_gui mged_players orig_gui xmin_faceplate_3525_y
    global xmin_faceplate_header_y xmin_faceplate_menu_x xmin_poll_ms
    if {![info exists mged_players] || [llength $mged_players] == 0} {
	after $xmin_poll_ms xmin_wait_for_gui
	return
    }

    set id [lindex $mged_players 0]
    set top .$id
    if {![winfo exists $top] || ![winfo ismapped $top] || [winfo width $top] <= 1} {
	after $xmin_poll_ms xmin_wait_for_gui
	return
    }

    update idletasks
    if {![xmin_publish_menu_target main_edit_menu $top Edit]} {
	after $xmin_poll_ms xmin_wait_for_gui
	return
    }
    if {![xmin_publish_menu_target main_misc_menu $top Misc]} {
	after $xmin_poll_ms xmin_wait_for_gui
	return
    }
    foreach {name label} {
	file File edit Edit create Create view View viewring ViewRing
	settings Settings modes Modes misc Misc tools Tools help Help
    } {
	if {![xmin_publish_menu_target main_${name}_menu $top $label]} {
	    after $xmin_poll_ms xmin_wait_for_gui
	    return
	}
    }
    set dm $mged_gui($id,active_dm)
    xmin_publish_faceplate_target faceplate_header $dm \
	$xmin_faceplate_menu_x $xmin_faceplate_header_y
    xmin_publish_faceplate_target faceplate_3525 $dm \
	$xmin_faceplate_menu_x $xmin_faceplate_3525_y
    winset $dm
    setview 0 0 0
    xmin_write faceplate_view_initial [view aet]
    xmin_publish_menu_inventory $id
    xmin_publish_renderer_capabilities $id
    xmin_publish_submenu_targets $id
    xmin_write faceplate_current $faceplate
    xmin_write orig_gui_current $orig_gui
    xmin_write main_window "[wm title $top]"
    xmin_write active_dm_geometry [list [winfo rootx $dm] [winfo rooty $dm] \
	[winfo width $dm] [winfo height $dm]]
    after $xmin_poll_ms [list xmin_monitor_faceplate $id]
    after $xmin_poll_ms [list xmin_monitor_renderer $id]
    after $xmin_poll_ms [list xmin_wait_for_primitive_menu $id]
    return
}

proc xmin_publish_renderer_capabilities {id} {
    set capabilities {}
    foreach setting {depthcue zbuffer lighting} {
	if {[mged_dm_supports $id $setting]} {
	    lappend capabilities $setting
	    set initial [dm set $setting]
	    set requested [expr {!$initial}]
	    set mutable 0
	    if {![catch {dm set $setting $requested}] &&
		![catch {dm set $setting} observed] && $observed == $requested} {
		set mutable 1
	    }
	    dm set $setting $initial
	    xmin_write renderer_${setting}_initial $initial
	    xmin_write renderer_${setting}_mutable $mutable
	}
    }
    if {[llength $capabilities] == 0} {
	set capabilities none
    }
    xmin_write renderer_capabilities $capabilities
}

proc xmin_monitor_renderer {id} {
    global mged_gui xmin_poll_ms xmin_test_dir
    if {[file exists [file join $xmin_test_dir stop_general_monitors]]} {
	xmin_write renderer_monitor_stopped 1
	return
    }
    foreach setting {depthcue zbuffer lighting} {
	if {![mged_dm_supports $id $setting]} {
	    continue
	}
	if {![catch {dm set $setting} value]} {
	    xmin_write renderer_${setting}_current $value
	}
	if {[info exists mged_gui($id,$setting)]} {
	    xmin_write renderer_${setting}_gui_current $mged_gui($id,$setting)
	}
    }
    after $xmin_poll_ms [list xmin_monitor_renderer $id]
    return
}

proc xmin_monitor_faceplate {id} {
    global faceplate mged_gui orig_gui xmin_poll_ms xmin_test_dir
    if {[file exists [file join $xmin_test_dir stop_general_monitors]]} {
	xmin_write faceplate_monitor_stopped 1
	return
    }
    winset $mged_gui($id,active_dm)
    xmin_write faceplate_current $faceplate
    xmin_write orig_gui_current $orig_gui

    if {![catch {view aet} view]} {
	xmin_write faceplate_view_current $view
	}
    if {![catch {mmenu_get} menus]} {
	xmin_write faceplate_menu_current $menus
    }
    if {![catch {mmenu_get 2} general_menu] &&
	[lsearch -exact $general_menu "35,25"] >= 0} {
	xmin_write faceplate_general_ready 1
    }
    after $xmin_poll_ms [list xmin_monitor_faceplate $id]
    return
}

proc xmin_wait_for_primitive_menu {id} {
    global xmin_poll_ms
    set top .$id
    update idletasks
    if {![xmin_publish_menu_target primitive_editor_menu $top "Primitive Editor"]} {
	after $xmin_poll_ms [list xmin_wait_for_primitive_menu $id]
	return
    }
    after $xmin_poll_ms [list xmin_wait_for_editor $id]
    return
}

proc xmin_wait_for_editor {id} {
    global xmin_poll_ms
    set editor .$id.edit_solid
    if {![winfo exists $editor] || ![winfo ismapped $editor]} {
	after $xmin_poll_ms [list xmin_wait_for_editor $id]
	return
    }

    update idletasks
    xmin_write editor_window "[wm title $editor]"
    xmin_publish_target editor_name $editor.nameE
    xmin_write editor_name_initial [$editor.nameE get]
    xmin_publish_target editor_reset $editor.resetB
    after $xmin_poll_ms [list xmin_monitor_entry editor_name $editor.nameE]
    after $xmin_poll_ms [list xmin_wait_for_editor_load $id]
    return
}

proc xmin_monitor_entry {name widget} {
    global xmin_poll_ms
    if {[winfo exists $widget]} {
	xmin_write ${name}_current [$widget get]
	after $xmin_poll_ms [list xmin_monitor_entry $name $widget]
    }
    return
}

proc xmin_wait_for_editor_load {id} {
    global xmin_poll_ms xmin_test_dir
    if {![file exists [file join $xmin_test_dir editor_loaded]]} {
	after $xmin_poll_ms [list xmin_wait_for_editor_load $id]
	return
    }
    after $xmin_poll_ms [list xmin_wait_for_sphere_form $id]
    return
}

proc xmin_wait_for_sphere_form {id} {
    global esol_control xmin_poll_ms
    set editor .$id.edit_solid
    set vx $editor.sformF._F._VE0
    if {![info exists esol_control($id,name)] || $esol_control($id,name) ne "gui.s" ||
	![winfo exists $vx]} {
	after $xmin_poll_ms [list xmin_wait_for_sphere_form $id]
	return
    }

    update idletasks
    xmin_publish_target sphere_vx $vx
    xmin_write sphere_vx_initial [$vx get]
    xmin_publish_target apply_button $editor.applyB
    after $xmin_poll_ms [list xmin_monitor_entry sphere_vx $vx]
    after $xmin_poll_ms xmin_wait_for_primitive_apply
    return
}

proc xmin_wait_for_primitive_apply {} {
    global xmin_poll_ms xmin_test_dir
    if {![file exists [file join $xmin_test_dir primitive_applied]]} {
	after $xmin_poll_ms xmin_wait_for_primitive_apply
	return
    }
    if {[catch {db get gui.s V} vertex] || $vertex ne "5 0 0"} {
	after $xmin_poll_ms xmin_wait_for_primitive_apply
	return
    }
    xmin_write primitive_result \
	"PASS: Xmin delivered the Primitive Editor Apply action"
    after $xmin_poll_ms xmin_wait_for_pipe_start
    return
}

proc xmin_wait_for_pipe_start {} {
    global xmin_poll_ms xmin_test_dir
    if {![file exists [file join $xmin_test_dir begin_pipe]]} {
	after $xmin_poll_ms xmin_wait_for_pipe_start
	return
    }
    if {[catch {xmin_setup_pipe_edit} message]} {
	xmin_fail "could not set up pipe editing: $message"
    }
}

proc xmin_setup_pipe_edit {} {
    global mged_gui mged_players xmin_faceplate_menu_x
    global xmin_pipe_next_y xmin_pipe_select_y xmin_pipe_split_y xmin_poll_ms

    set id [lindex $mged_players 0]
    catch {destroy .$id.edit_solid}
    catch {kill gui.pipe}
    in gui.pipe pipe 4 \
	0 0 0 0.5 1 2 \
	4 0 2 0.5 1 2 \
	8 4 4 0.5 1 2 \
	12 4 6 0.5 1 2
    Z
    e gui.pipe
    sed gui.pipe
    center 6 2 0
    size 20
    setview 0 0 0

    if {[status state] ne "SOL EDIT"} {
	error "pipe selection entered [status state], not SOL EDIT"
    }
    if {[lsearch -exact [mmenu_get 0] "Select Point"] < 0 ||
	[lsearch -exact [mmenu_get 0] "Next Point"] < 0 ||
	[lsearch -exact [mmenu_get 0] "Split Segment"] < 0} {
	error "pipe contextual menu is incomplete: [mmenu_get 0]"
    }

    set dm $mged_gui($id,active_dm)
    winset $dm
    update idletasks
    xmin_publish_faceplate_target pipe_select $dm \
	$xmin_faceplate_menu_x $xmin_pipe_select_y
    xmin_publish_faceplate_target pipe_next $dm \
	$xmin_faceplate_menu_x $xmin_pipe_next_y
    xmin_publish_faceplate_target pipe_split $dm \
	$xmin_faceplate_menu_x $xmin_pipe_split_y
    xmin_publish_model_point_target pipe_point $dm {0 0 0}
    xmin_publish_model_point_target pipe_split_point $dm {6 2 2}
    xmin_write pipe_keypoint_initial [keypoint]
    xmin_write pipe_ready 1
    after $xmin_poll_ms xmin_monitor_pipe
    return
}

proc xmin_monitor_pipe {} {
    global xmin_poll_ms xmin_test_dir
    if {[catch {set current_keypoint [keypoint]}]} {
	after $xmin_poll_ms xmin_monitor_pipe
	return
    }
    xmin_write pipe_keypoint_current $current_keypoint
    if {[file exists [file join $xmin_test_dir pipe_point_clicked]]} {
	xmin_write pipe_selected 1
    }
    if {[file exists [file join $xmin_test_dir pipe_finish]]} {
	press accept
	xmin_write pipe_done 1
	after $xmin_poll_ms xmin_wait_for_sketch_start
	return
    }
    after $xmin_poll_ms xmin_monitor_pipe
    return
}

proc xmin_wait_for_sketch_start {} {
    global xmin_poll_ms xmin_test_dir
    if {![file exists [file join $xmin_test_dir begin_sketch]]} {
	after $xmin_poll_ms xmin_wait_for_sketch_start
	return
    }
    if {[catch {xmin_setup_sketch_editor} message]} {
	xmin_fail "could not set up sketch editing: $message"
    }
}

proc xmin_setup_sketch_editor {} {
    global xmin_poll_ms xmin_sketch_editor

    catch {kill gui.sketch}
    put gui.sketch sketch V {0 0 0} A {1 0 0} B {0 1 0} \
	VL {{0 0} {10 0} {10 10} {0 10} {5 0} {5 5}} \
	SL {{line S 0 E 1} {line S 1 E 2} {carc S 4 E 5 R 5 L 1 O 0}}
    Sketch_editor .#auto gui.sketch gui.sketch
    set editors [find objects -class Sketch_editor]
    if {[llength $editors] == 0} {
	error "Sketch_editor did not create an object"
    }
    set xmin_sketch_editor [lindex $editors end]
    set hull $xmin_sketch_editor
    if {![winfo exists $hull]} {
	set hull [$xmin_sketch_editor component hull]
    }
    update idletasks

    foreach {target label} {
	sketch_create_line {Create Line}
	sketch_zoom_in {Zoom In}
	sketch_reset {Reset Sketch}
	sketch_dismiss Dismiss
    } {
	set widget [xmin_find_widget $hull Button $label]
	if {$widget eq ""} {
	    error "Sketch Editor has no '$label' button"
	}
	xmin_publish_target $target $widget
    }
    set canvas ""
    foreach widget [xmin_descendants $hull] {
	if {[winfo class $widget] eq "Canvas"} {
	    set canvas $widget
	    break
	}
    }
    if {$canvas eq ""} {
	error "Sketch Editor has no canvas"
    }
    set width [winfo width $canvas]
    set height [winfo height $canvas]
    xmin_write sketch_canvas_start_window [winfo id $canvas]
    xmin_write sketch_canvas_start [list \
	[expr {$width * 5 / 8}] [expr {$height * 5 / 8}]]
    xmin_write sketch_canvas_end_window [winfo id $canvas]
    xmin_write sketch_canvas_end [list \
	[expr {$width * 3 / 4}] [expr {$height * 3 / 4}]]
    xmin_write sketch_vertex_count_initial \
	[llength [$xmin_sketch_editor get_vlist]]
    xmin_write sketch_ready 1
    after $xmin_poll_ms xmin_monitor_sketch
    return
}

proc xmin_monitor_sketch {} {
    global xmin_poll_ms xmin_sketch_editor
    if {[llength [info commands $xmin_sketch_editor]] == 0} {
	xmin_write sketch_closed 1
	after $xmin_poll_ms xmin_wait_for_matrix_start
	return
    }
    xmin_write sketch_scale_current [$xmin_sketch_editor get_scale]
    xmin_write sketch_vertex_count_current \
	[llength [$xmin_sketch_editor get_vlist]]
    after $xmin_poll_ms xmin_monitor_sketch
    return
}

proc xmin_wait_for_matrix_start {} {
    global xmin_poll_ms xmin_test_dir
    if {![file exists [file join $xmin_test_dir begin_matrix]]} {
	after $xmin_poll_ms xmin_wait_for_matrix_start
	return
    }

    if {[catch {xmin_setup_matrix_edit} message]} {
	xmin_fail "could not set up matrix editing: $message"
    }
}

proc xmin_setup_matrix_edit {} {
    global mged_gui mged_players xmin_bv_max xmin_edit_coord_half_range
    global xmin_matrix_target_denominator xmin_matrix_target_numerator
    global xmin_matrix_view_before xmin_matrix_view_size xmin_min_widget_extent
    global xmin_poll_ms

    set id [lindex $mged_players 0]
    set editor .$id.edit_solid
    catch {destroy $editor}
    catch {kill matrix.c}
    catch {kill matrix.s}
    in matrix.s sph 0 0 0 10
    comb matrix.c u matrix.s
    Z
    e matrix.c
    center 0 0 0
    size $xmin_matrix_view_size
    setview 0 0 0
    rset grid snap 0

    set dm $mged_gui($id,active_dm)
    winset $dm
    update idletasks
    set width [winfo width $dm]
    set height [winfo height $dm]
    if {$width < $xmin_min_widget_extent ||
	$height < $xmin_min_widget_extent} {
	error "active display widget has unusable dimensions ${width}x${height}"
    }
    dm size $width $height

    _mged_press oill
    _mged_ill -e -i 1 /matrix.c/matrix.s
    _mged_matpick 1
    if {[status state] ne "OBJ EDIT"} {
	error "matrix selection entered [status state], not OBJ EDIT"
    }
    press {XY Move}
    set xmin_matrix_view_before [xmin_view_state]

    set local_x [expr {
	$width * $xmin_matrix_target_numerator / $xmin_matrix_target_denominator}]
    set local_y [expr {
	$height * $xmin_matrix_target_numerator / $xmin_matrix_target_denominator}]
    set aspect [expr {double($width) / $height}]
    set bv_x [expr {int((double($local_x) / $width - 0.5) * 2.0 * $xmin_bv_max)}]
    set bv_y [expr {int((0.5 - double($local_y) / $height) * 2.0 / $aspect * $xmin_bv_max)}]
    set expected_x [expr {$bv_x / $xmin_edit_coord_half_range * $xmin_matrix_view_size / 2.0}]
    set expected_y [expr {$bv_y / $xmin_edit_coord_half_range * $xmin_matrix_view_size / 2.0}]
    xmin_write matrix_expected \
	[list $expected_x $expected_y]
    set matrix_x [expr {[winfo rootx $dm] + $local_x}]
    set matrix_y [expr {[winfo rooty $dm] + $local_y}]
    xmin_write matrix_dm_window root
    xmin_write matrix_dm [list $matrix_x $matrix_y]
    xmin_write matrix_dimensions_debug [list "${width}x${height}" \
	root [winfo rootx $dm] [winfo rooty $dm] target $matrix_x $matrix_y]
    after $xmin_poll_ms xmin_wait_for_matrix_click
    return
}

proc xmin_wait_for_matrix_click {} {
    global xmin_matrix_click_settle_ms xmin_poll_ms xmin_test_dir
    if {![file exists [file join $xmin_test_dir matrix_clicked]]} {
	after $xmin_poll_ms xmin_wait_for_matrix_click
	return
    }
    after $xmin_matrix_click_settle_ms xmin_finish_matrix_edit
    return
}

proc xmin_finish_matrix_edit {} {
    global mged_players xmin_matrix_view_before xmin_poll_ms

    set view_after [xmin_view_state]
    if {$view_after ne $xmin_matrix_view_before} {
	xmin_fail "matrix mouse click changed the view from $xmin_matrix_view_before to $view_after"
	return
    }
    press accept
    if {[status state] ne "VIEWING"} {
	xmin_fail "accept left MGED in [status state], not VIEWING"
	return
    }

    xmin_write matrix_result \
	"PASS: Xmin delivered the MGED matrix middle-click and accept actions"
    set id [lindex $mged_players 0]
    after $xmin_poll_ms [list xmin_wait_for_raytrace_panel $id]
    return
}

proc xmin_publish_raytrace_menu_targets {top} {
    set menu $top.menubar.fb
    $menu post [winfo rootx $top] \
	[expr {[winfo rooty $top.menubar] + [winfo height $top.menubar]}]
    update idletasks
    foreach {target label} {
	raytrace_overlay Overlay
	raytrace_interlay Interlay
	raytrace_underlay Underlay
    } {
	if {![xmin_publish_menu_widget_target $target $menu $label menu]} {
	    $menu unpost
	    error "Raytrace Control Panel has no '$label' framebuffer mode"
	}
	xmin_write ${target}_window [winfo id $top]
    }
    $menu unpost
}

proc xmin_wait_for_raytrace_panel {id} {
    global xmin_poll_ms
    set panel .$id.rt
    if {![winfo exists $panel] || ![winfo ismapped $panel]} {
	after $xmin_poll_ms [list xmin_wait_for_raytrace_panel $id]
	return
    }

    update idletasks
    if {![xmin_publish_menu_target raytrace_framebuffer_menu $panel Framebuffer]} {
	after $xmin_poll_ms [list xmin_wait_for_raytrace_panel $id]
	return
    }
    if {[catch {xmin_publish_raytrace_menu_targets $panel} message]} {
	xmin_fail $message
	return
    }

    xmin_write raytrace_window [wm title $panel]
    xmin_publish_target raytrace_size $panel.sizeE
    xmin_write raytrace_size_initial [$panel.sizeE get]
    xmin_publish_target raytrace_destination $panel.destE
    xmin_write raytrace_destination_initial [$panel.destE get]
    xmin_publish_target raytrace_button $panel.raytraceB
    xmin_publish_target raytrace_active $panel.fbtoggle
    xmin_publish_target raytrace_dismiss $panel.dismissB
    xmin_write raytrace_ready 1
    after $xmin_poll_ms [list xmin_monitor_entry raytrace_size $panel.sizeE]
    after $xmin_poll_ms [list xmin_monitor_entry raytrace_destination $panel.destE]
    after $xmin_poll_ms [list xmin_monitor_raytrace $id]
    return
}

proc xmin_prepare_raytrace_reference {id} {
    global env rt_control xmin_test_dir

    set panel .$id.rt
    set reference_script [file join $xmin_test_dir raytrace-reference.sh]
    set reference_log [file join $xmin_test_dir raytrace-reference.log]
    set reference_image [file join $xmin_test_dir raytrace-reference.pix]
    set gui_image [file join $xmin_test_dir raytrace-gui.pix]
    file delete -force $reference_script $reference_log $reference_image $gui_image

    set rgb [getRGB $panel.colorMB $rt_control($id,color)]
    set options [list -s $rt_control($id,size) \
	-C[join $rgb /] -P$rt_control($id,nproc) \
	-H$rt_control($id,hsample) -J$rt_control($id,jitter) \
	-l$rt_control($id,lmodel) -z$rt_control($id,opencl)]
    if {[catch {saveview -e $env(MGED_RT_BIN) -l $reference_log \
	-o $reference_image $reference_script {*}$options} message]} {
	error "could not prepare standalone raytrace reference: $message"
    }
    xmin_write raytrace_reference_script $reference_script
    xmin_write raytrace_reference_image $reference_image
    xmin_write raytrace_file_destination $gui_image
    xmin_write raytrace_reference_ready 1
}

proc xmin_monitor_raytrace {id} {
    global fb fb_overlay mged_gui xmin_poll_ms
    global xmin_raytrace_completion_settle_ms
    global xmin_raytrace_current_request xmin_raytrace_started
    global xmin_raytrace_wait_started xmin_test_dir

    set panel .$id.rt
    if {[winfo exists $panel]} {
	xmin_write raytrace_fb_current $fb
	xmin_write raytrace_overlay_current $fb_overlay
    } else {
	xmin_write raytrace_closed 1
    }

    if {[file exists [file join $xmin_test_dir raytrace_prepare_reference]] &&
	![file exists [file join $xmin_test_dir raytrace_reference_ready]]} {
	if {[catch {xmin_prepare_raytrace_reference $id} message]} {
	    xmin_fail $message
	    return
	}
    }

    set request_file [file join $xmin_test_dir raytrace_request]
    if {[file exists $request_file]} {
	set channel [open $request_file r]
	set request [string trim [gets $channel]]
	close $channel
	if {![info exists xmin_raytrace_current_request] ||
	    $request ne $xmin_raytrace_current_request} {
	    set xmin_raytrace_current_request $request
	    set xmin_raytrace_wait_started [clock milliseconds]
	    unset -nocomplain xmin_raytrace_started
	}
    }
    if {[info exists xmin_raytrace_wait_started]} {
	set processes [process list]
	set idle [string match "No currently running*" $processes]
	if {!$idle} {
	    set xmin_raytrace_started 1
	    xmin_write raytrace_process_started 1
	}
	set elapsed [expr {[clock milliseconds] - $xmin_raytrace_wait_started}]
	if {$idle && ([info exists xmin_raytrace_started] ||
	    $elapsed >= $xmin_raytrace_completion_settle_ms)} {
	    xmin_write raytrace_elapsed_${xmin_raytrace_current_request}_ms $elapsed
	    xmin_write raytrace_complete $xmin_raytrace_current_request
	    unset xmin_raytrace_wait_started
	}
    }

    if {[file exists [file join $xmin_test_dir raytrace_export_requested]] &&
	![file exists [file join $xmin_test_dir raytrace_export_ready]]} {
	set export_image [file join $xmin_test_dir raytrace-embedded.pix]
	file delete -force $export_image
	winset $mged_gui($id,active_dm)
	if {[catch {fb2pix -s $::env(MGED_RAYTRACE_SIZE) $export_image} message]} {
	    xmin_fail "could not export the embedded framebuffer: $message"
	    return
	}
	xmin_write raytrace_export_image $export_image
	xmin_write raytrace_export_ready 1
    }

    if {[file exists [file join $xmin_test_dir raytrace_responsive]]} {
	if {[catch {view aet} current_view]} {
	    xmin_fail "MGED did not respond after the embedded raytrace"
	    return
	}
	xmin_write raytrace_view_after $current_view
	xmin_write result \
	    "PASS: embedded raytrace and framebuffer modes remained responsive"
	after idle xmin_wait_for_finish
	return
    }

    after $xmin_poll_ms [list xmin_monitor_raytrace $id]
    return
}

proc xmin_wait_for_finish {} {
    global xmin_poll_ms xmin_test_dir
    if {[file exists [file join $xmin_test_dir finish]]} {
	_mged_quit
	return
    }
    after $xmin_poll_ms xmin_wait_for_finish
    return
}

after $xmin_deadline_ms [list xmin_fail "timed out waiting for GUI edit completion"]
after idle xmin_wait_for_gui
