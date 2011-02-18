#                      C U R S O R . T C L
# BRL-CAD
#
# Copyright (c) 2006-2011 United States Government as represented by
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
# DESCRIPTION
#    Cursor setting utilities
#
#***
##############################################################


namespace eval cadwidgets {
    set cursorWaitcount 0
}

# PROCEDURE: SetWaitCursor
#
# Changes the cursor for all of the GUI's widgets to the wait cursor.
#
# Arguments:
#       None
#
# Results:
#       None
#
proc SetWaitCursor {_w} {
    incr ::cadwidgets::cursorWaitCount

    if {1 < $::cadwidgets::cursorWaitCount} {
	# Already in cursor wait mode
	return
    }

    $_w configure -cursor watch
    ::update idletasks
}

# PROCEDURE: SetNormalCursor
#
# Changes the cursor for all of the GUI's widgets to the normal cursor.
#
# Arguments:
#       None
#
# Results:
#       None
#
proc SetNormalCursor {_w} {
    incr ::cadwidgets::cursorWaitCount -1

    if {$::cadwidgets::cursorWaitCount != 0} {
	return
    }

    $_w configure -cursor {}
    ::update idletasks
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
