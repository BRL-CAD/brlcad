#                    S D I A L O G S . T C L
# BRL-CAD
#
# Copyright (c) 2006-2020 United States Government as represented by
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

package require Tcl 8.0
package require Tk 8.0
package require Itcl
package require Itk
package require Iwidgets

namespace eval ::sdialogs {
    namespace export *

    variable library [file dirname [info script]]
    variable version 1.0
}

lappend auto_path [file join $sdialogs::library scripts]
package provide Sdialogs $sdialogs::version

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
