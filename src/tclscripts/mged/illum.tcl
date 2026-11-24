#                       I L L U M . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
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
#
#		Tcl routines to illuminate solids/objects/matrices.
#

#	Ensure that all commands that this script uses without defining
#	are provided by the calling application
check_externs "::brlcad::mged"

proc solid_illum {spath {ri 1}} {
    set state [::brlcad::mged status state]

    switch $state {
	VIEWING {
	    ::brlcad::mged press sill
	}
	default {
	    ::brlcad::mged press reject
	    ::brlcad::mged press sill
	}
    }

    ::brlcad::mged ill -e -n -i $ri $spath
}

proc matrix_illum { spath path_pos {ri 1}} {
    set state [::brlcad::mged status state]

    switch $state {
	VIEWING {
	    ::brlcad::mged press oill
	    ::brlcad::mged ill -e -i $ri $spath
	}
	"OBJ PICK" {
	    ::brlcad::mged ill -e -i $ri $spath
	}
	default {
	    ::brlcad::mged press reject
	    ::brlcad::mged press oill
	    ::brlcad::mged ill -e -i $ri $spath
	}
    }

    ::brlcad::mged matpick -n $path_pos
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
