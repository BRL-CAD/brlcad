#                    P K G I N D E X . T C L
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
# Archer and ArcherCore load Tk-dependent packages.  Register them explicitly
# so generating this index does not require a display or depend on the Itk
# implementation found by the build-time Tcl interpreter.

package ifneeded Archer 1.0 [list source [file join $dir Archer.tcl]]
package ifneeded ArcherCore 1.0 [list source [file join $dir ArcherCore.tcl]]

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
