#                  L O D U T I L I T Y . T C L
# BRL-CAD
#
# Copyright (c) 2013-2014 United States Government as represented by
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
# Description:
#	This is an Archer class for configuring Level of Detail drawing.
#

package require Tk
package require Itcl
package require Itk

::itcl::class LODUtility {
    inherit Utility

    constructor {_archer _handleToplevel args} {}
    destructor {}

    public {
	# Override's for the Utility class
	common utilityMajorType $Archer::pluginMajorTypeUtility
	common utilityMinorType $Archer::pluginMinorTypeMged
	common utilityName "LOD Utility"
	common utilityVersion "1.0"
	common utilityClass LODUtility
    }

    protected {
	variable mHandleToplevel 0
	variable mParentBg ""
	variable mToplevel ""

	method updateBgColor {_bg}
    }

    private {
	common instances 0
    }
}

::itcl::body LODUtility::constructor {_archer _handleToplevel args} {
    set parent [winfo parent $itk_interior]
    set mParentBg [$parent cget -background]

    eval itk_initialize $args

    pack [LODDialog $itk_interior.lodDialog -cmdprefix $_archer] \
	-expand true \
	-fill both
}

::itcl::body LODUtility::destructor {} {
}

::itcl::body LODUtility::updateBgColor {_bg} {
    # TODO: update background color of all widgets
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
