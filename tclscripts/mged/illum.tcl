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

proc solid_illum { spath } {
    set state [_mged_status state]

    switch $state {
	VIEWING {
	    _mged_press sill
	}
	default {
	    _mged_press reject
	    _mged_press sill
	}
    }
    
    _mged_ill -n $spath
}

proc matrix_illum { spath path_pos } {
    set state [_mged_status state]

    switch $state {
	VIEWING {
	    _mged_press oill
	    _mged_ill $spath
	}
	"OBJ PICK" {
	    _mged_ill $spath
	}
	default {
	    _mged_press reject
	    _mged_press oill
	    _mged_ill $spath
	}
    }

    _mged_matpick -n $path_pos
}
