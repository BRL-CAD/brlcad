# Exercise MGED's Tk editing controls using coordinates consumed by xminctl.
# This script may inspect widget geometry, but it deliberately does not invoke
# any GUI action or database edit under test: menu, entry, button, and display
# actions arrive through the X server.

set xmin_test_dir $::env(MGED_XMIN_TEST_DIR)
set xmin_deadline_ms 30000
set xmin_poll_ms 25
set xmin_matrix_view_size 40.0
set xmin_bv_max 2047.0
set xmin_edit_coord_half_range 2048.0
set xmin_matrix_target_numerator 5
set xmin_matrix_target_denominator 8
set xmin_min_widget_extent 16
set xmin_matrix_click_settle_ms 100
set xmin_sphere_target_x 5.0
set xmin_numeric_tolerance 1.0e-9

proc xmin_write {name contents} {
    global xmin_test_dir
    set channel [open [file join $xmin_test_dir $name] w]
    puts $channel $contents
    close $channel
}

proc xmin_publish_target {name widget} {
    xmin_write ${name}_window [winfo id $widget]
    xmin_write $name [list \
	[expr {[winfo width $widget] / 2}] \
	[expr {[winfo height $widget] / 2}]]
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

proc xmin_menu_entry_point {menu index} {
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
    return [list \
	[expr {[winfo rootx $menu] + ($first_x + $last_x) / 2}] \
	[expr {[winfo rooty $menu] + ($first_y + $last_y) / 2}]]
}

proc xmin_publish_menu_target {name root label} {
    foreach menu [xmin_descendants $root] {
	if {[winfo class $menu] ne "Menu" || ![winfo ismapped $menu] ||
	    [winfo width $menu] <= 1} {
	    continue
	}
	if {[catch {set last [$menu index end]}] || $last eq "none"} {
	    continue
	}
	for {set index 0} {$index <= $last} {incr index} {
	    if {[catch {set entry_label [$menu entrycget $index -label]}] ||
		$entry_label ne $label} {
		continue
	    }
	    xmin_write ${name}_window root
	    xmin_write $name [xmin_menu_entry_point $menu $index]
	    return 1
	}
    }
    return 0
}

proc xmin_view_state {} {
    return [list [center] [size] [view quat]]
}

proc xmin_leaf_matrix {comb member} {
    set tree [db get $comb tree]
    if {[llength $tree] < 2 || [lindex $tree 0] ne "l" || [lindex $tree 1] ne $member} {
	error "$comb is not the expected simple leaf '$member': $tree"
    }
    if {[llength $tree] == 2} {
	return {1 0 0 0  0 1 0 0  0 0 1 0  0 0 0 1}
    }
    if {[llength $tree] != 3 || [llength [lindex $tree 2]] != 16} {
	error "$comb leaf matrix is not 4x4: $tree"
    }
    return [lindex $tree 2]
}

proc xmin_fail {message} {
    xmin_write result "FAIL: $message"
    after idle _mged_quit
}

proc xmin_wait_for_gui {} {
    global mged_players xmin_poll_ms
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
    xmin_write main_window "[wm title $top]"
    if {![xmin_publish_menu_target main_edit_menu $top Edit]} {
	after $xmin_poll_ms xmin_wait_for_gui
	return
    }
    after $xmin_poll_ms [list xmin_wait_for_primitive_menu $id]
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
}

proc xmin_monitor_entry {name widget} {
    global xmin_poll_ms
    if {[winfo exists $widget]} {
	xmin_write ${name}_current [$widget get]
	after $xmin_poll_ms [list xmin_monitor_entry $name $widget]
    }
}

proc xmin_wait_for_editor_load {id} {
    global xmin_poll_ms xmin_test_dir
    if {![file exists [file join $xmin_test_dir editor_loaded]]} {
	after $xmin_poll_ms [list xmin_wait_for_editor_load $id]
	return
    }
    after $xmin_poll_ms [list xmin_wait_for_sphere_form $id]
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
    after $xmin_poll_ms xmin_check_result
}

proc xmin_check_result {} {
    global xmin_numeric_tolerance xmin_poll_ms xmin_sphere_target_x
    if {[catch {set definition [db get gui.s]}]} {
	after $xmin_poll_ms xmin_check_result
	return
    }
    xmin_write edit_result_debug $definition
    set vindex [lsearch -exact $definition V]
    if {$vindex < 0} {
	xmin_fail "sphere definition has no V field: $definition"
	return
    }
    set vertex [lindex $definition [expr {$vindex + 1}]]
    if {[llength $vertex] == 3 &&
	abs([lindex $vertex 0] - $xmin_sphere_target_x) < $xmin_numeric_tolerance &&
	abs([lindex $vertex 1]) < $xmin_numeric_tolerance &&
	abs([lindex $vertex 2]) < $xmin_numeric_tolerance} {
	xmin_write primitive_result "PASS: Xmin MGED primitive editor produced V {$vertex}"
	after $xmin_poll_ms xmin_wait_for_matrix_start
	return
    }
    after $xmin_poll_ms xmin_check_result
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
    global xmin_matrix_expected_x xmin_matrix_expected_y
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
    set xmin_matrix_expected_x [expr {$bv_x / $xmin_edit_coord_half_range * $xmin_matrix_view_size / 2.0}]
    set xmin_matrix_expected_y [expr {$bv_y / $xmin_edit_coord_half_range * $xmin_matrix_view_size / 2.0}]
    set matrix_x [expr {[winfo rootx $dm] + $local_x}]
    set matrix_y [expr {[winfo rooty $dm] + $local_y}]
    xmin_write matrix_dm_window root
    xmin_write matrix_dm [list $matrix_x $matrix_y]
    xmin_write matrix_dimensions_debug [list "${width}x${height}" \
	root [winfo rootx $dm] [winfo rooty $dm] target $matrix_x $matrix_y]
    after $xmin_poll_ms xmin_wait_for_matrix_click
}

proc xmin_wait_for_matrix_click {} {
    global xmin_matrix_click_settle_ms xmin_poll_ms xmin_test_dir
    if {![file exists [file join $xmin_test_dir matrix_clicked]]} {
	after $xmin_poll_ms xmin_wait_for_matrix_click
	return
    }
    after $xmin_matrix_click_settle_ms xmin_finish_matrix_edit
}

proc xmin_finish_matrix_edit {} {
    global xmin_matrix_expected_x xmin_matrix_expected_y xmin_matrix_view_before
    global xmin_numeric_tolerance

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

    if {[catch {set matrix [xmin_leaf_matrix matrix.c matrix.s]} message]} {
	xmin_fail $message
	return
    }
    set expected [list 1 0 0 $xmin_matrix_expected_x \
	0 1 0 $xmin_matrix_expected_y \
	0 0 1 0 \
	0 0 0 1]
    for {set index 0} {$index < 16} {incr index} {
        if {abs([lindex $matrix $index] - [lindex $expected $index]) >
	    $xmin_numeric_tolerance} {
	    xmin_fail "matrix component $index expected [lindex $expected $index], got [lindex $matrix $index] (matrix $matrix)"
	    return
	}
    }

    xmin_write result "PASS: Xmin MGED primitive widget edit and matrix middle-click produced exact expected geometry"
    after idle _mged_quit
}

after $xmin_deadline_ms [list xmin_fail "timed out waiting for GUI edit completion"]
after idle xmin_wait_for_gui
