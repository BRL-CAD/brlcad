#
#			I L L U M . T C L
#
#	Author -
#		Robert G. Parker
#
#	Description -
#		Tcl routines to illuminate solids/objects/matrices.
#

#	Ensure that all commands that this script uses without defining
#	are provided by the calling application
check_externs "_mged_press _mged_ill _mged_matpick"

proc solid_illum { w x y illum_only } {
    $w selection clear 0 end
    $w selection set @$x,$y

    _mged_press sill

    if {$illum_only} {
	_mged_ill -n [$w get @$x,$y]
    } else {
	_mged_ill [$w get @$x,$y]
    }
}

proc obj_illum { id w x y illum_only } {
    $w selection clear 0 end
    $w selection set @$x,$y

    _mged_press oill

    if {$illum_only} {
	_mged_ill -n [$w get @$x,$y]
    } else {
	set item [$w get @$x,$y]
	_mged_ill $item
	build_matrix_menu $id $item
    }
}

proc matrix_illum { w x y illum_only } {
    $w selection clear 0 end
    $w selection set @$x,$y

    if {$illum_only} {
	_mged_matpick -n [$w index @$x,$y]
    } else {
	_mged_matpick [$w index @$x,$y]
    }
}

proc comb_illum { w x y } {
    $w selection clear 0 end
    $w selection set @$x,$y

    set comb [$w get @$x,$y]

    set paths [_mged_x -1]
    set path_index [lsearch $paths *$comb*]
    set path [lindex $paths $path_index]
    regexp "\[^/\].*" $path match
    set path_components [split $match /]
    set path_index [lsearch -exact $path_components $comb]

    _mged_press oill
    _mged_ill $path
    _mged_matpick $path_index
}
